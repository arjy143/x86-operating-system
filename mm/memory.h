#ifndef MEMORY_H
#define MEMORY_H

#include "types.h"

//init allocator
void memory_init();

//initial bump allocator used only for the pmm
void* bump_alloc(uint32_t size);

void* malloc(uint32_t size);

void free(void* ptr);

uint32_t memory_free();

//find out how much memory is being used currently
uint32_t memory_used();

#endif
