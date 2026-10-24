#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"

#define ARENA_CHUNK_SIZE 65536
#define ARENA_ALIGN 16

struct ArenaChunk {
    ArenaChunk *next;
    size_t used;
    size_t cap;
    _Alignas(ARENA_ALIGN) unsigned char data[];
};

static void *checked_malloc(size_t size)
{
    void *memory = malloc(size);
    if (memory == NULL) {
        fputs("minic: out of memory\n", stderr);
        exit(1);
    }
    return memory;
}

void arena_init(Arena *arena)
{
    arena->head = NULL;
}

void *arena_alloc(Arena *arena, size_t size)
{
    size_t rounded = (size + (ARENA_ALIGN - 1)) & ~(size_t)(ARENA_ALIGN - 1);
    if (rounded == 0) {
        rounded = ARENA_ALIGN;
    }
    ArenaChunk *chunk = arena->head;
    if (chunk == NULL || chunk->cap - chunk->used < rounded) {
        size_t cap = rounded > ARENA_CHUNK_SIZE ? rounded : ARENA_CHUNK_SIZE;
        chunk = checked_malloc(sizeof(ArenaChunk) + cap);
        chunk->next = arena->head;
        chunk->used = 0;
        chunk->cap = cap;
        arena->head = chunk;
    }
    void *memory = chunk->data + chunk->used;
    chunk->used += rounded;
    memset(memory, 0, rounded);
    return memory;
}

char *arena_strndup(Arena *arena, const char *text, size_t length)
{
    char *copy = arena_alloc(arena, length + 1);
    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

char *arena_strdup(Arena *arena, const char *text)
{
    return arena_strndup(arena, text, strlen(text));
}

char *arena_printf(Arena *arena, const char *format, ...)
{
    char scratch[256];
    va_list args;
    va_start(args, format);
    va_list again;
    va_copy(again, args);
    int length = vsnprintf(scratch, sizeof scratch, format, args);
    va_end(args);
    if (length < 0) {
        va_end(again);
        return arena_strdup(arena, "");
    }
    if ((size_t)length < sizeof scratch) {
        va_end(again);
        return arena_strndup(arena, scratch, (size_t)length);
    }
    char *text = arena_alloc(arena, (size_t)length + 1);
    vsnprintf(text, (size_t)length + 1, format, again);
    va_end(again);
    return text;
}

void arena_free(Arena *arena)
{
    ArenaChunk *chunk = arena->head;
    while (chunk != NULL) {
        ArenaChunk *next = chunk->next;
        free(chunk);
        chunk = next;
    }
    arena->head = NULL;
}
