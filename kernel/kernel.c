#include "idt.h"
#include "vga.h"
#include "keyboard.h"
#include "memory.h"
#include "types.h"
#include "shell.h"
#include "timer.h"
#include "memmap.h"
#include "pmm.h"

void kernel_main()
{
    vga_clear();
    vga_print(0,0, "Welcome to the kernel.", WHITE_ON_BLACK);

    //ensure all memory regions are correctly registered
    memmap_supplement(); 
    
    //use bump_alloc for pmm
    pmm_init();

    //set up heap and use malloc from now on
    memory_init();
    

    idt_init();
    timer_init(100);    
    shell_init();

    //now the hardware can interrupt us
    __asm__ volatile ("sti");
    

    for (;;)
    {
        __asm__ volatile ("hlt");
    }
}
