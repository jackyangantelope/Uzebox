/* BSD-3-Clause deterministic SRAM-hot / PSRAM-bulk arenas. */
#include "jack_memory.h"

bool jack_arena_init(jack_arena *arena, void *memory, size_t capacity) {
    if(arena == NULL || memory == NULL || capacity == 0u) {
        return false;
    }
    *arena = (jack_arena){.base = (uint8_t *)memory, .capacity = capacity};
    return true;
}

void *jack_arena_alloc(jack_arena *arena, size_t size, size_t alignment) {
    if(arena == NULL || size == 0u || alignment == 0u ||
       (alignment & (alignment - 1u)) != 0u) {
        if(arena != NULL) {
            arena->failures++;
        }
        return NULL;
    }
    const size_t aligned = (arena->used + alignment - 1u) & ~(alignment - 1u);
    if(aligned > arena->capacity || size > arena->capacity - aligned) {
        arena->failures++;
        return NULL;
    }
    void *result = arena->base + aligned;
    arena->used = aligned + size;
    if(arena->used > arena->high_water) {
        arena->high_water = arena->used;
    }
    return result;
}

size_t jack_arena_mark(const jack_arena *arena) {
    return arena == NULL ? 0u : arena->used;
}

bool jack_arena_reset(jack_arena *arena, size_t mark) {
    if(arena == NULL || mark > arena->used) {
        return false;
    }
    arena->used = mark;
    return true;
}

bool jack_memory_init(jack_memory_layout *layout, void *sram, size_t sram_size,
                      void *psram, size_t psram_size) {
    return layout != NULL && jack_arena_init(&layout->hot, sram, sram_size) &&
           jack_arena_init(&layout->bulk, psram, psram_size);
}

void *jack_memory_alloc_hot(jack_memory_layout *layout, size_t size, size_t alignment) {
    return layout == NULL ? NULL : jack_arena_alloc(&layout->hot, size, alignment);
}

void *jack_memory_alloc_bulk(jack_memory_layout *layout, size_t size, size_t alignment) {
    return layout == NULL ? NULL : jack_arena_alloc(&layout->bulk, size, alignment);
}
