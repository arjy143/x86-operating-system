#include "pmm.h"
#include "memmap.h"
#include "memory.h"
#include "panic.h"
#include "types.h"

static uint32_t* bitmap = 0;
static uint32_t total_frames = 0;
static uint32_t free_count = 0;

//some helper functions
static void bitmap_set(uint32_t frame)
{
    bitmap[frame /32] |= (1 << (frame % 32));
}

static void bitmap_clear(uint32_t frame)
{
    bitmap[frame /32] &= ~(1 << (frame % 32));
}

static uint32_t bitmap_test(uint32_t frame)
{
    return bitmap[frame /32] & (1 << (frame % 32));
}

static void mark_used(uint32_t start, uint32_t length)
{
    uint32_t frame = start / PMM_FRAME_SIZE;
    uint32_t count = (length + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;

    uint32_t i;
    for (i = 0; i < count; i++)
    {
        if (frame + i < total_frames)
        {
            if (!bitmap_test(frame + i))
            {
                bitmap_set(frame + i);
                if (free_count > 0)
                {
                    free_count--;
                }
            }
        }
    }
}

static void mark_free(uint32_t start, uint32_t length)
{
    uint32_t frame = start / PMM_FRAME_SIZE;
    uint32_t count = (length + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;
    uint32_t i;

    for (i = 0; i < count; i++)
    {
        if (frame + i < total_frames)
        {
            if (bitmap_test(frame + i))
            {
                bitmap_clear(frame + i);
                free_count++;
            }
        }
    }
}

void pmm_init()
{
    //theoretically this would get the usable memory
    uint32_t total_memory = memmap_total_usable();

    //to get true total,add reserved regions
    total_memory = 128 * 1024 * 1024;

    total_frames = total_memory / PMM_FRAME_SIZE;

    uint32_t bitmap_size = total_frames / 8;

    bitmap = (uint32_t*)bump_alloc(bitmap_size);

    if (bitmap == 0)
    {
        kernel_panic("PMM: failed to allocate bitmap");
    }

    //initially mark everything as used, for safety, then go through and walk memory map to find free usable regions
    uint32_t i;
    for (i = 0; i < bitmap_size / 4; i++)
    {
        bitmap[i] = 0xffffffff;
    }
    
    free_count = 0;

    uint32_t count = memmap_count();
    for (i = 0; i < count; i++)
    {
        struct memory_region* r = memmap_get(i);
        if (r->type == MEMORY_USABLE)
        {
            mark_free((uint32_t)r->base, (uint32_t)r->length);
        }
    }

    //mark low memory, kernel, bitmap as used
    mark_used(0x00000, 0x100000);

    extern uint32_t kernel_end;
    mark_used(0x100000, (uint32_t)&kernel_end - 0x100000);

    mark_used((uint32_t)bitmap, bitmap_size);
}

//allocate a physical frame and get physical address
uint32_t pmm_alloc_frame()
{
    if (free_count == 0)
    {
        kernel_panic("PMM: out of phys mem");
    }

    uint32_t i;
    for (i = 0; i < total_frames / 32; i++)
    {
        if (bitmap[i] != 0xffffffff)
        {
            uint32_t j;
            for (j = 0; j < 32; j++)
            {   
                uint32_t frame = i * 32 + j;
                
                if (!bitmap_test(frame))
                {
                    bitmap_set(frame);
                    free_count--;

                    return frame * PMM_FRAME_SIZE;
                }
            }
        }
    }

    kernel_panic("PMM: inconsistent bitmap");
    return 0;
}

//free physical memory
void pmm_free_frame(uint32_t frame_address)
{
    uint32_t frame = frame_address / PMM_FRAME_SIZE;

    if (frame >= total_frames)
    {
        kernel_panic("PMM: invalid frame address");
    }

    if (!bitmap_test(frame))
    {
        kernel_panic("PMM: double free");
    }

    bitmap_clear(frame);
    free_count++;
}

uint32_t pmm_free_frames()
{
    return free_count;
}

uint32_t pmm_used_frames()
{
    return total_frames - free_count;
}

