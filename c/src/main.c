#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pipeline.h"
#include "vm.h"

#define MINIC_VERSION "1.0.0"

enum { EXIT_COMPILE_ERROR = 1, EXIT_USAGE = 2, EXIT_RUNTIME_ERROR = 3 };

typedef struct {
    const char *file;
    int tokens;
    int ast;
    int symbols;
    int tac;
    int bytecode;
    int optimize;
    int no_run;
    int stats;
    long long max_steps;
} Options;

static void print_usage(FILE *out)
{
    fputs("usage: minic [-h] [--tokens] [--ast] [--symbols] [--tac] [--bytecode] [-O0]\n"
          "             [--no-run] [--stats] [--max-steps N] [--version]\n"
          "             file\n",
          out);
}

static void print_help(void)
{
    print_usage(stdout);
    fputs("\nMiniC compiler and virtual machine\n\n"
          "positional arguments:\n"
          "  file             source file to compile and run\n\n"
          "options:\n"
          "  -h, --help       show this help message and exit\n"
          "  --tokens         print the token stream\n"
          "  --ast            print the typed syntax tree\n"
          "  --symbols        print the symbol table\n"
          "  --tac            print the three-address code\n"
          "  --bytecode       print the stack VM bytecode\n"
          "  -O0              disable optimizer\n"
          "  --no-run         compile without executing\n"
          "  --stats          print compilation statistics\n"
          "  --max-steps N    abort execution after this many VM steps\n"
          "  --version        show program's version number and exit\n",
          stdout);
}

static int usage_error(const char *message, const char *detail)
{
    print_usage(stderr);
    fprintf(stderr, "minic: error: %s%s\n", message, detail);
    return EXIT_USAGE;
}

static int parse_steps(const char *text, long long *value)
{
    char *end = NULL;
    errno = 0;
    long long parsed = strtoll(text, &end, 10);
    if (end == text || *end != '\0' || errno != 0) {
        return 0;
    }
    *value = parsed;
    return 1;
}

static int parse_options(int argc, char **argv, Options *options)
{
    options->file = NULL;
    options->optimize = 1;
    options->max_steps = VM_DEFAULT_MAX_STEPS;
    for (int index = 1; index < argc; index++) {
        const char *arg = argv[index];
        if (strcmp(arg, "--tokens") == 0) {
            options->tokens = 1;
        } else if (strcmp(arg, "--ast") == 0) {
            options->ast = 1;
        } else if (strcmp(arg, "--symbols") == 0) {
            options->symbols = 1;
        } else if (strcmp(arg, "--tac") == 0) {
            options->tac = 1;
        } else if (strcmp(arg, "--bytecode") == 0) {
            options->bytecode = 1;
        } else if (strcmp(arg, "-O0") == 0) {
            options->optimize = 0;
        } else if (strcmp(arg, "--no-run") == 0) {
            options->no_run = 1;
        } else if (strcmp(arg, "--stats") == 0) {
            options->stats = 1;
        } else if (strcmp(arg, "--version") == 0) {
            printf("minic %s\n", MINIC_VERSION);
            return -1;
        } else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            print_help();
            return -1;
        } else if (strcmp(arg, "--max-steps") == 0 || strncmp(arg, "--max-steps=", 12) == 0) {
            const char *value = NULL;
            if (arg[11] == '=') {
                value = arg + 12;
            } else if (index + 1 < argc) {
                value = argv[++index];
            } else {
                return usage_error("argument --max-steps: expected one argument", "");
            }
            if (!parse_steps(value, &options->max_steps)) {
                return usage_error("argument --max-steps: invalid int value: ", value);
            }
        } else if (arg[0] == '-' && arg[1] != '\0') {
            return usage_error("unrecognized arguments: ", arg);
        } else if (options->file == NULL) {
            options->file = arg;
        } else {
            return usage_error("unrecognized arguments: ", arg);
        }
    }
    if (options->file == NULL) {
        return usage_error("the following arguments are required: file", "");
    }
    return 0;
}

static char *read_file(const char *path, size_t *length)
{
    FILE *handle = fopen(path, "rb");
    if (handle == NULL) {
        return NULL;
    }
    size_t capacity = 4096;
    size_t used = 0;
    char *data = malloc(capacity);
    if (data == NULL) {
        fclose(handle);
        return NULL;
    }
    for (;;) {
        size_t got = fread(data + used, 1, capacity - used, handle);
        used += got;
        if (used < capacity) {
            break;
        }
        capacity *= 2;
        char *grown = realloc(data, capacity);
        if (grown == NULL) {
            free(data);
            fclose(handle);
            return NULL;
        }
        data = grown;
    }
    fclose(handle);
    *length = used;
    return data;
}

static void begin_section(const char *title)
{
    printf("== %s ==\n", title);
}

static int run(const Options *options)
{
    size_t length = 0;
    char *source = read_file(options->file, &length);
    if (source == NULL) {
        fprintf(stderr, "minic: cannot open '%s'\n", options->file);
        return EXIT_USAGE;
    }

    Compilation result;
    int ok = compile_source(&result, source, length, options->optimize);
    int show_dumps =
        options->tokens || options->ast || options->symbols || options->tac || options->bytecode;
    int status = 0;

    if (options->tokens) {
        begin_section("tokens");
        format_tokens(stdout, &result.tokens);
    }

    if (!ok) {
        for (size_t index = 0; index < result.errors.count; index++) {
            format_error(stderr, options->file, source, length, &result.errors.items[index]);
        }
        size_t count = result.errors.count;
        fprintf(stderr, "compilation failed: %zu error%s\n", count, count != 1 ? "s" : "");
        status = EXIT_COMPILE_ERROR;
        goto done;
    }

    if (options->ast) {
        begin_section("ast");
        dump_ast(stdout, result.program);
    }
    if (options->symbols) {
        begin_section("symbols");
        format_symbols(stdout, &result.table);
    }
    if (options->tac) {
        begin_section("tac");
        format_tac(stdout, &result.table, &result.tac);
    }
    if (options->bytecode) {
        begin_section("bytecode");
        format_bytecode(stdout, &result.bytecode);
    }
    if (options->stats) {
        fprintf(stderr, "tac instructions : %zu -> %zu\n", result.raw_tac.count, result.tac.count);
        fprintf(stderr, "bytecode size    : %zu\n", result.bytecode.code.count);
    }
    if (options->no_run) {
        goto done;
    }
    if (show_dumps) {
        begin_section("output");
    }

    VmResult outcome = vm_run(&result.bytecode, stdout, options->max_steps);
    if (outcome.fault != NULL) {
        fflush(stdout);
        fprintf(stderr, "runtime error: %s\n", outcome.fault);
        status = EXIT_RUNTIME_ERROR;
        goto done;
    }
    if (options->stats) {
        fprintf(stderr, "vm steps         : %lld\n", outcome.steps);
    }

done:
    fflush(stdout);
    compilation_free(&result);
    free(source);
    return status;
}

int main(int argc, char **argv)
{
    Options options;
    memset(&options, 0, sizeof options);
    int parsed = parse_options(argc, argv, &options);
    if (parsed < 0) {
        return 0;
    }
    if (parsed > 0) {
        return parsed;
    }
    return run(&options);
}
