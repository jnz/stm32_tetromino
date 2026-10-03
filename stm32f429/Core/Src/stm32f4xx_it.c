/*
 * Interrupt and exception handlers. SysTick is the only interrupt in use,
 * the display flip polls VSYNC.
 *
 * Faults reset the board instead of hanging: this runs unattended as a
 * desktop ornament, and a reset brings it back with the high score intact.
 * With a debugger attached it stops on a breakpoint first, so a fault can
 * still be inspected.
 */
#include "main.h"
#include "stm32f4xx_it.h"

static void fault(void)
{
    if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0U)
        __BKPT(0);
    NVIC_SystemReset();
}

void NMI_Handler(void)
{
    fault();
}

void HardFault_Handler(void)
{
    fault();
}

void MemManage_Handler(void)
{
    fault();
}

void BusFault_Handler(void)
{
    fault();
}

void UsageFault_Handler(void)
{
    fault();
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}
