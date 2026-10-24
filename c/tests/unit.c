#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "lexer.h"
#include "ops.h"
#include "pipeline.h"
#include "symbols.h"
#include "vm.h"

static int checks = 0;
static int failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        checks++;                                                               \
        if (!(condition)) {                                                     \
            failures++;                                                         \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        }                                                                       \
    } while (0)

#define CHECK_STR(actual, expected)                                                         \
    do {                                                                                    \
        checks++;                                                                           \
        if (strcmp((actual), (expected)) != 0) {                                            \
            failures++;                                                                     \
            fprintf(stderr, "FAIL %s:%d: got \"%s\" expected \"%s\"\n", __FILE__, __LINE__, \
                    (actual), (expected));                                                  \
        }                                                                                   \
    } while (0)

static char *capture(void (*writer)(FILE *, Compilation *), Compilation *compilation)
{
    char *buffer = NULL;
    size_t size = 0;
    FILE *stream = open_memstream(&buffer, &size);
    writer(stream, compilation);
    fclose(stream);
    return buffer;
}

static void write_tac(FILE *out, Compilation *c)
{
    format_tac(out, &c->table, &c->tac);
}

static char *run_program(const char *source, int optimize, const char **fault)
{
    Compilation c;
    int ok = compile_source(&c, source, strlen(source), optimize);
    CHECK(ok);
    char *buffer = NULL;
    size_t size = 0;
    FILE *stream = open_memstream(&buffer, &size);
    VmResult result = ok ? vm_run(&c.bytecode, stream, VM_DEFAULT_MAX_STEPS) : (VmResult){0, NULL};
    fclose(stream);
    static char message[128];
    if (fault != NULL) {
        message[0] = '\0';
        if (result.fault != NULL) {
            snprintf(message, sizeof message, "%s", result.fault);
        }
        *fault = message;
    }
    compilation_free(&c);
    return buffer;
}

static void expect_output(const char *source, const char *expected)
{
    char *plain = run_program(source, 0, NULL);
    char *optimized = run_program(source, 1, NULL);
    CHECK_STR(plain, expected);
    CHECK_STR(optimized, expected);
    free(plain);
    free(optimized);
}

static size_t error_count(const char *source, char *first_message, size_t size)
{
    Compilation c;
    compile_source(&c, source, strlen(source), 1);
    size_t count = c.errors.count;
    if (first_message != NULL && count > 0) {
        snprintf(first_message, size, "%d:%d:%s", c.errors.items[0].line, c.errors.items[0].col,
                 c.errors.items[0].message);
    }
    compilation_free(&c);
    return count;
}

static void test_arena(void)
{
    Arena arena;
    arena_init(&arena);
    char *text = arena_printf(&arena, "%s-%d", "value", 42);
    CHECK_STR(text, "value-42");
    char *big = arena_alloc(&arena, 200000);
    memset(big, 7, 200000);
    CHECK(big[199999] == 7);
    char *copy = arena_strndup(&arena, "abcdef", 3);
    CHECK_STR(copy, "abc");
    CHECK_STR(text, "value-42");
    arena_free(&arena);
}

static void test_values(void)
{
    Value result;
    CHECK(value_binary(OP_ADD, value_int(INT64_MAX), value_int(1), &result) == 0);
    CHECK(result.u.i == INT64_MIN);
    CHECK(value_binary(OP_DIV, value_int(INT64_MIN), value_int(-1), &result) == 0);
    CHECK(result.u.i == INT64_MIN);
    CHECK(value_binary(OP_MOD, value_int(INT64_MIN), value_int(-1), &result) == 0);
    CHECK(result.u.i == 0);
    CHECK(value_binary(OP_DIV, value_int(-7), value_int(2), &result) == 0 && result.u.i == -3);
    CHECK(value_binary(OP_MOD, value_int(-7), value_int(3), &result) == 0 && result.u.i == -1);
    CHECK(value_binary(OP_DIV, value_int(1), value_int(0), &result) == 1);
    CHECK(value_binary(OP_MOD, value_int(1), value_int(0), &result) == 1);
    CHECK(value_binary(OP_DIV, value_float(1.0), value_float(0.0), &result) == 1);
    CHECK(value_binary(OP_LT, value_float(1.0), value_float(2.0), &result) == 0 && result.u.i == 1);
    CHECK(!result.is_float);
    CHECK(value_neg(value_int(INT64_MIN)).u.i == INT64_MIN);
    CHECK(value_not(value_int(0)).u.i == 1);
    CHECK(value_not(value_float(2.5)).u.i == 0);
    CHECK(value_to_float(value_int(3)).u.f == 3.0);
}

static void test_value_format(void)
{
    char buffer[64];
    value_format(value_float(3.0), buffer, sizeof buffer);
    CHECK_STR(buffer, "3.0");
    value_format(value_float(0.1 + 0.2), buffer, sizeof buffer);
    CHECK_STR(buffer, "0.3");
    value_format(value_float(1e18), buffer, sizeof buffer);
    CHECK_STR(buffer, "1e+18");
    value_format(value_float(-0.0), buffer, sizeof buffer);
    CHECK_STR(buffer, "-0.0");
    value_format(value_int(INT64_MIN), buffer, sizeof buffer);
    CHECK_STR(buffer, "-9223372036854775808");
    value_format(value_float(1.0 / 3.0), buffer, sizeof buffer);
    CHECK_STR(buffer, "0.3333333333");
}

static void test_lexer(void)
{
    Arena arena;
    arena_init(&arena);
    TokenList tokens;
    ErrorList errors;
    memset(&tokens, 0, sizeof tokens);
    memset(&errors, 0, sizeof errors);
    const char *source = "int x = 3.5; // note\nx <= 2 && y != 1;";
    lex_source(&arena, source, strlen(source), &tokens, &errors);
    CHECK(errors.count == 0);
    CHECK(tokens.count == 14);
    CHECK(tokens.items[0].kind == TK_KEYWORD);
    CHECK(tokens.items[3].kind == TK_FLOAT);
    CHECK_STR(tokens.items[6].text, "<=");
    CHECK(tokens.items[6].line == 2 && tokens.items[6].col == 3);
    CHECK(tokens.items[tokens.count - 1].kind == TK_EOF);
    arena_free(&arena);
}

static void test_lexer_errors(void)
{
    char message[200];
    CHECK(error_count("int a = 4 $ 2;", message, sizeof message) == 1);
    CHECK_STR(message, "1:11:unexpected character '$'");
    CHECK(error_count("9abc;", message, sizeof message) == 1);
    CHECK(strstr(message, "cannot start with a digit") != NULL);
    CHECK(error_count("3.;", message, sizeof message) == 1);
    CHECK(strstr(message, "missing digits") != NULL);
    CHECK(error_count("/* open", message, sizeof message) == 1);
    CHECK(error_count("int big = 9223372036854775808;", NULL, 0) == 1);
    CHECK(error_count("int big = 9223372036854775807;", NULL, 0) == 0);
    CHECK(error_count("int big = 0000009223372036854775807;", NULL, 0) == 0);
    CHECK(error_count("a & b;", message, sizeof message) == 1);
    CHECK(strstr(message, "&&") != NULL);
    CHECK(error_count("int x = 1; \xc3\xa9", message, sizeof message) == 1);
    CHECK_STR(message, "1:12:unexpected character '\xc3\xa9'");
}

static void test_parser_errors(void)
{
    char message[200];
    CHECK(error_count("int x = ;", message, sizeof message) == 1);
    CHECK_STR(message, "1:9:unexpected token ';'");
    CHECK(error_count("int x = 5\nint y;", message, sizeof message) == 1);
    CHECK_STR(message, "2:1:expected ';' before 'int'");
    CHECK(error_count("int a = ;\nint b = 1;\nint c = ;", NULL, 0) == 2);
    CHECK(error_count("{ int a = ; int b = 2; } int c = 3;", NULL, 0) == 1);
    CHECK(error_count("else x = 1;", message, sizeof message) >= 1);
    CHECK(strstr(message, "without a matching") != NULL);
    CHECK(error_count("x = x y;", message, sizeof message) == 1);
    CHECK(strstr(message, "missing an operator") != NULL);
    CHECK(error_count("while (1) int z;", message, sizeof message) == 1);
    CHECK(strstr(message, "cannot be the body") != NULL);
    CHECK(error_count("}", NULL, 0) >= 1);
    CHECK(error_count("{ int x;", message, sizeof message) == 1);
}

static void test_deep_nesting(void)
{
    size_t depth = 400;
    char *source = malloc(depth * 2 + 16);
    size_t used = 0;
    memcpy(source, "int x = ", 8);
    used = 8;
    memset(source + used, '(', depth);
    used += depth;
    source[used++] = '1';
    memset(source + used, ')', depth);
    used += depth;
    source[used++] = ';';
    source[used] = '\0';
    char message[200];
    CHECK(error_count(source, message, sizeof message) == 1);
    CHECK(strstr(message, "too deep") != NULL);
    free(source);
}

static void test_semantic_errors(void)
{
    char message[200];
    CHECK(error_count("print(x);", message, sizeof message) == 1);
    CHECK_STR(message, "1:7:undeclared variable 'x'");
    CHECK(error_count("int x; int x;", message, sizeof message) == 1);
    CHECK(strstr(message, "already declared") != NULL);
    CHECK(error_count("int x = 1; { int x = 2; print(x); }", NULL, 0) == 0);
    CHECK(error_count("{ int y; } print(y);", NULL, 0) == 1);
    CHECK(error_count("int n = 2.5;", message, sizeof message) == 1);
    CHECK(strstr(message, "cannot assign float") != NULL);
    CHECK(error_count("float f = 1; f = 2;", NULL, 0) == 0);
    CHECK(error_count("float f = 1.0; print(f % 2);", message, sizeof message) == 1);
    CHECK(strstr(message, "requires integer operands") != NULL);
    CHECK(error_count("int x = (y + 1) * 2;", NULL, 0) == 1);
    CHECK(error_count("print(b);\nprint(a);", message, sizeof message) == 2);
    CHECK(strncmp(message, "1:7", 3) == 0);
}

static void test_symbol_table(void)
{
    Arena arena;
    arena_init(&arena);
    SymbolTable table;
    symtab_init(&table, &arena);
    CHECK(symtab_declare(&table, "x", TYPE_INT, 1, 1) != NULL);
    CHECK(symtab_declare(&table, "x", TYPE_INT, 2, 1) == NULL);
    symtab_push_scope(&table);
    Symbol *inner = symtab_declare(&table, "x", TYPE_FLOAT, 3, 1);
    CHECK(inner != NULL);
    CHECK_STR(inner->ir_name, "x.1");
    CHECK(symtab_lookup(&table, "x")->type == TYPE_FLOAT);
    symtab_pop_scope(&table);
    CHECK(symtab_lookup(&table, "x")->type == TYPE_INT);
    CHECK(symtab_lookup(&table, "missing") == NULL);
    arena_free(&arena);
}

static void test_optimizer(void)
{
    Compilation c;
    const char *source = "int x = 2 + 3 * 4; print(x);";
    compile_source(&c, source, strlen(source), 1);
    char *text = capture(write_tac, &c);
    CHECK_STR(text, "    x = 14\n    print 14\n");
    free(text);
    compilation_free(&c);

    source = "int i = 0; while (i < 3) { i = i + 1; } print(i);";
    compile_source(&c, source, strlen(source), 1);
    text = capture(write_tac, &c);
    CHECK(strstr(text, "i < 3") != NULL);
    CHECK(strstr(text, "print i") != NULL);
    free(text);
    compilation_free(&c);

    source = "print(1 / 0);";
    compile_source(&c, source, strlen(source), 1);
    text = capture(write_tac, &c);
    CHECK(strstr(text, "/") != NULL);
    free(text);
    compilation_free(&c);

    source = "int r; if (1) r = 1; else r = 2; print(r);";
    compile_source(&c, source, strlen(source), 1);
    text = capture(write_tac, &c);
    CHECK(strstr(text, "r = 2") == NULL);
    free(text);
    compilation_free(&c);
}

static void test_execution(void)
{
    expect_output("print(1 + 2 * 3);", "7\n");
    expect_output("print((1 + 2) * 3);", "9\n");
    expect_output("print(7 / 2); print(-7 / 2); print(7.0 / 2);", "3\n-3\n3.5\n");
    expect_output("print(-7 % 3); print(7 % -3);", "-1\n1\n");
    expect_output("float f = 3; print(f); int x; print(x);", "3.0\n0\n");
    expect_output("print(1 == 1.0); print(2 >= 3 || 0); print(!5);", "1\n0\n0\n");
    expect_output("int z = 0; print(z != 0 && 1 / z > 0); print(z == 0 || 1 / z > 0);", "0\n1\n");
    expect_output("int m = 9223372036854775807; print(m + 1);", "-9223372036854775808\n");
    expect_output("int x = 1; { int x = 2; { int x = 3; print(x); } print(x); } print(x);",
                  "3\n2\n1\n");
    expect_output("int i = 0; int s = 0; while (i < 10) { s = s + i * i; i = i + 1; } print(s);",
                  "285\n");
    expect_output("int v = 7; if (v < 5) print(1); else if (v < 10) print(2); else print(3);",
                  "2\n");
    expect_output("int i = 0; while (i < 3) { int fresh; print(fresh); fresh = 9; i = i + 1; }",
                  "0\n0\n0\n");
}

static void test_runtime_faults(void)
{
    const char *fault = NULL;
    char *out = run_program("int z = 0; print(1); print(1 / z);", 1, &fault);
    CHECK_STR(out, "1\n");
    CHECK_STR(fault, "division by zero");
    free(out);

    Compilation c;
    const char *source = "while (1) { }";
    compile_source(&c, source, strlen(source), 1);
    FILE *sink = fopen("/dev/null", "w");
    VmResult result = vm_run(&c.bytecode, sink, 500);
    fclose(sink);
    CHECK(result.fault != NULL && strstr(result.fault, "step limit") != NULL);
    CHECK(result.steps == 500);
    compilation_free(&c);
}

int main(void)
{
    test_arena();
    test_values();
    test_value_format();
    test_lexer();
    test_lexer_errors();
    test_parser_errors();
    test_deep_nesting();
    test_semantic_errors();
    test_symbol_table();
    test_optimizer();
    test_execution();
    test_runtime_faults();
    printf("unit: %d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
