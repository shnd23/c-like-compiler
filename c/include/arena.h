#ifndef MINIC_ARENA_H
#define MINIC_ARENA_H

#include <stddef.h>
#include <string.h>

typedef struct ArenaChunk ArenaChunk;

typedef struct Arena {
    ArenaChunk *head;
} Arena;

void arena_init(Arena *arena);
void *arena_alloc(Arena *arena, size_t size);
char *arena_strndup(Arena *arena, const char *text, size_t length);
char *arena_strdup(Arena *arena, const char *text);
char *arena_printf(Arena *arena, const char *format, ...) __attribute__((format(printf, 2, 3)));
void arena_free(Arena *arena);

#define VEC(T) \
    struct {   \
        T *items; \
        size_t count; \
        size_t cap;   \
    }

#define vec_push(arena, vec, value)                                              \
    do {                                                                         \
        if ((vec)->count == (vec)->cap) {                                        \
            size_t new_cap_ = (vec)->cap ? (vec)->cap * 2 : 8;                   \
            void *grown_ = arena_alloc((arena), new_cap_ * sizeof(*(vec)->items)); \
            if ((vec)->count) {                                                  \
                memcpy(grown_, (vec)->items, (vec)->count * sizeof(*(vec)->items)); \
            }                                                                    \
            (vec)->items = grown_;                                               \
            (vec)->cap = new_cap_;                                               \
        }                                                                        \
        (vec)->items[(vec)->count++] = (value);                                  \
    } while (0)

#endif
