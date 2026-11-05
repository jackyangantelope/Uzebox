/* BSD-3-Clause deterministic SRAM-hot / PSRAM-bulk arenas. */
#ifndef JACK_YELLOW_MEMORY_H
#define JACK_YELLOW_MEMORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *base;
    size_t capacity;
    size_t used;
    size_t high_water;
    uint32_t failures;
} jack_arena;

typedef struct {
    jack_arena hot;
    jack_arena bulk;
} jack_memory_layout;

bool jack_arena_init(jack_arena *arena, void *memory, size_t capacity);
void *jack_arena_alloc(jack_arena *arena, size_t size, size_t alignment);
size_t jack_arena_mark(const jack_arena *arena);
bool jack_arena_reset(jack_arena *arena, size_t mark);
bool jack_memory_init(jack_memory_layout *layout, void *sram, size_t sram_size,
                      void *psram, size_t psram_size);
void *jack_memory_alloc_hot(jack_memory_layout *layout, size_t size, size_t alignment);
void *jack_memory_alloc_bulk(jack_memory_layout *layout, size_t size, size_t alignment);

#endif
