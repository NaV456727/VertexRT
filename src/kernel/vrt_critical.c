#include "vrt_critical.h"

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

static portMUX_TYPE
    vrt_kernel_mux =
        portMUX_INITIALIZER_UNLOCKED;

void vrt_kernel_critical_enter(void)
{
    portENTER_CRITICAL(
        &vrt_kernel_mux);
}

void vrt_kernel_critical_exit(void)
{
    portEXIT_CRITICAL(
        &vrt_kernel_mux);
}

void IRAM_ATTR
vrt_kernel_critical_enter_isr(void)
{
    portENTER_CRITICAL_ISR(
        &vrt_kernel_mux);
}

void IRAM_ATTR
vrt_kernel_critical_exit_isr(void)
{
    portEXIT_CRITICAL_ISR(
        &vrt_kernel_mux);
}