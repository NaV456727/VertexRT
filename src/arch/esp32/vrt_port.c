#include "vrt_port.h"
#include "vrt_port_frame.h"

#include <stdint.h>
#include <stddef.h>

#include <xtensa/xtensa_context.h>

/*
 * ============================================================================
 * ESP32 FreeRTOS stack initializer
 * ============================================================================
 *
 * This function is already supplied by the ESP32 FreeRTOS port and is linked
 * from:
 *
 *     libfreertos.a
 *
 * We deliberately use the framework's implementation instead of reproducing
 * the Xtensa initial-stack ABI ourselves.
 *
 * This gives VertexRT the exact same:
 *
 *     - stack alignment
 *     - XtExcFrame layout
 *     - register-window setup
 *     - A1 initialization
 *     - PS configuration
 *     - TLS handling
 *     - task argument placement
 *     - initial PC
 *
 * as the working ESP32 FreeRTOS port.
 * ============================================================================
 */

extern uint32_t *pxPortInitialiseStack(
    uint32_t *topOfStack,
    void (*taskFunction)(void *),
    void *parameter);

/*
 * ============================================================================
 * Initial task stack
 * ============================================================================
 */

uint32_t *vrt_port_stack_init(
    uint32_t *stackTop,
    vrt_task_function_t entry,
    void *argument)
{
    if (stackTop == NULL || entry == NULL)
    {
        return NULL;
    }

    /*
     * Do not construct an XtExcFrame ourselves.
     *
     * Pass the stack directly to the ESP32 FreeRTOS Xtensa initializer.
     *
     * stackTop is the LAST ELEMENT of the stack array because vrt_task_init()
     * passes stackEnd = stackStart + stackSize.
     *
     * pxPortInitialiseStack() expects the top-of-stack pointer in exactly the
     * same convention used by the ESP32 FreeRTOS implementation.
     */
    return pxPortInitialiseStack(
        stackTop,
        (void (*)(void *))entry,
        argument);
}