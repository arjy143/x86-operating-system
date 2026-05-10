#include "memory.h"
#include "types.h"
#include "panic.h"

//linker defines this. This is the memory address after all the kernel binary
extern uint32_t kernel_end;

//4mb max heap
#define HEAP_MAX_SIZE 0x400000

struct block_header
{
    uint32_t size;
    uint32_t free;
    struct block_header* next;
};

//free list
static struct block_header* heap_start = 0;
static uint32_t heap_used = 0;

//We can make a simple bump allocator for allocating the bitmap
static uint32_t bump_heap = 0;

static uint32_t allocated = 0;

void* bump_alloc(uint32_t size)
{
    if (bump_heap == 0)
    {
        bump_heap = (uint32_t)&kernel_end;
        
        if (bump_heap % 4 != 0)
        {
            bump_heap += 4 - (bump_heap % 4);
        }
    }

    if (size % 4 != 0)
    {
        size += 4 - (size % 4);
    }

    void* result = (void*)bump_heap;

    bump_heap += size;

    return result;
}

uint32_t bump_alloc_end()
{
    return bump_heap;
}


void memory_init()
{
    //we get the address because the address itself is the value. The linker doesn't allocate any storage for it
    uint32_t heap_address = bump_alloc_end();

    //align to 4 byte boundary
    if (heap_address % 4 != 0)
    {
        heap_address += 4 - (heap_address % 4);
    }
    
    //start just after the bitmap, which itself will be allocated using the bump allocator. After the bitmap, the rest of the heap can be allocated using a better allocator.
    heap_start = (struct block_header*)heap_address;
    heap_start->size = HEAP_MAX_SIZE - sizeof(struct block_header);
    heap_start->free = 1;
    heap_start->next = 0;
    heap_used = 0;
}

void* malloc(uint32_t size)
{
    if (size == 0)
    {
        return 0;
    }
    //first, align to 4 byte boundary for safe mem access
    //The CPU accesses 4 byte values most efficiently when address is divisible by 4, so we pad everything to be a multiple of 4 bytes.
    if (size % 4 != 0)
    {
        size += 4 - (size % 4);
    }
    
    //walk the free list to find a suitable block
    struct block_header* current = heap_start;
    while (current != 0)
    {
        if (current->free && current->size >= size)
        {
            if (current->size >= size + sizeof(struct block_header) + 16)
            {
                struct block_header* new_block = (struct block_header*)((uint8_t*)current + sizeof(struct block_header) + size);

                new_block->size = current->size - size - sizeof(struct block_header);
                new_block->free = 1;
                new_block->next = current->next;

                current->size = size;
                current->next = new_block;
            }

            current->free = 0;
            heap_used += current->size + sizeof(struct block_header);

            return (uint8_t*)current + sizeof(struct block_header);
        }
        current = current->next;
    }
    
    kernel_panic("malloc: out of memory, exceeded heap size.");
    
    return (void*)0;
}

void free(void* ptr)
{
    if (ptr == 0)
    {
        return;
    }

    struct block_header* header = (struct block_header*)((uint8_t*)ptr - sizeof(struct block_header));

    if (header->free)
    {
        kernel_panic("free: double free detected.");
    }

    header->free = 1;
    heap_used -= header->size + sizeof(struct block_header);

    //coalesce blocks if free
    if (header->next != 0 && header->next->free)
    {
        header->size += header->next->size + sizeof(struct block_header);
        header->next = header->next->next;
    }

    struct block_header* current = heap_start;

    while (current != 0 && current->next != header)
    {
        current = current->next;
    }

    if (current != 0 && current->free)
    {
        current->size += header->size + sizeof(struct block_header);
        current->next = header->next;
    }
}

uint32_t memory_used()
{
    return allocated;
}

uint32_t memory_free()
{
    return HEAP_MAX_SIZE - heap_used;
}
