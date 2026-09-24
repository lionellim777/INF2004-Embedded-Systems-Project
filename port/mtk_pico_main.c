#include "mtk_pico_main.h"

#include <stdint.h>
#include <stdio.h>

#include "hardware/clocks.h"
#include "hardware/exception.h"
#include "pico/stdlib.h"

#define KERNEL_POOL_SIZE (96U * 1024U)
#define KERNEL_CLOCK_HZ 125000000U

extern void knl_dispatch_entry(void);
extern void knl_systim_inthdr(void);
extern int mtk_kernel_main(void);

/* The BSP reset handler normally owns these bounds. SDK startup owns reset. */
void *knl_lowmem_top;
void *knl_lowmem_limit;

static uint8_t g_kernel_pool[KERNEL_POOL_SIZE] __attribute__((aligned(8)));

int
main(void)
{
    stdio_init_all();
    knl_lowmem_top = g_kernel_pool;
    knl_lowmem_limit = &g_kernel_pool[sizeof(g_kernel_pool)];

    (void)exception_set_exclusive_handler(PENDSV_EXCEPTION,
                                           knl_dispatch_entry);
    (void)exception_set_exclusive_handler(SYSTICK_EXCEPTION,
                                           knl_systim_inthdr);
    (void)exception_set_priority(PENDSV_EXCEPTION, 0xc0U);
    (void)exception_set_priority(SYSTICK_EXCEPTION, 0x00U);

    if (KERNEL_CLOCK_HZ != clock_get_hz(clk_sys))
    {
        printf("Kernel clock mismatch\n");
        for (;;)
        {
            tight_loop_contents();
        }
    }

    printf("Buddy 1 firmware starting\n");
    (void)mtk_kernel_main();

    for (;;)
    {
        tight_loop_contents();
    }
}
