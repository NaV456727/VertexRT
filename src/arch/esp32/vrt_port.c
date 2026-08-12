#include "vrt_port.h"
#include "vrt_port_frame.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <xtensa/corebits.h>
#include <xtensa/xtensa_context.h>

/*
 * Xtensa user exception exit dispatcher supplied by the SDK.
 */
extern void _xt_user_exit(void);

/*
 * --------------------------------------------------------------------------
 * Task trampoline
 * --------------------------------------------------------------------------
 *
 * For the windowed ABI the initial frame places:
 *
 *     A6 = entry
 *     A7 = argument
 *
 * because the initial PS pretends this function was entered by CALL4.
 */
static void vrt_task_trampoline(
    vrt_task_function_t entry,
    void *argument)
{
    if (entry != NULL)
    {
        entry(argument);
    }

    /*
     * A task must not return.
     * vrt_task_exit() switches to another task.
     */
    vrt_task_exit();

    /*
     * Defensive fallback.
     *
     * We should never get here.
     */
    for (;;)
    {
    }
}

/*
 * --------------------------------------------------------------------------
 * Initial Xtensa task stack
 * --------------------------------------------------------------------------
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

    uintptr_t stackPointer = (uintptr_t)stackTop;

    /*
     * Xtensa requires 16-byte stack alignment.
     */
    stackPointer &=
        ~((uintptr_t)0x0FU);

    /*
     * XT_STK_FRMSZ includes the actual XtExcFrame plus the
     * additional stack space required by the windowed ABI.
     */
    if (stackPointer < (uintptr_t)XT_STK_FRMSZ)
    {
        return NULL;
    }

    stackPointer -= (uintptr_t)XT_STK_FRMSZ;

    /*
     * Keep the complete frame aligned.
     */
    stackPointer &=
        ~((uintptr_t)0x0FU);

    /*
     * Clear the entire allocation, not merely sizeof(XtExcFrame).
     */
    memset(
        (void *)stackPointer,
        0,
        (size_t)XT_STK_FRMSZ);

    XtExcFrame *frame =
        (XtExcFrame *)stackPointer;

    /*
     * ----------------------------------------------------------------------
     * Core context
     * ----------------------------------------------------------------------
     */

    /*
     * No caller.
     */
    frame->a0 = 0;

    /*
     * A1 is the physical top of this allocated task frame.
     *
     * This is exactly how the FreeRTOS Xtensa port initializes it.
     */
    frame->a1 =
        (uint32_t)(stackPointer +
                   (uintptr_t)XT_STK_FRMSZ);

    /*
     * Exception exit dispatcher.
     */
    frame->exit =
        (uint32_t)(uintptr_t)_xt_user_exit;

    /*
     * The task begins in our trampoline.
     */
    frame->pc =
        (uint32_t)(uintptr_t)vrt_task_trampoline;

#ifdef __XTENSA_CALL0_ABI__

    /*
     * Call0 ABI:
     *
     *   vrt_task_trampoline(entry, argument)
     *
     * arguments are A2/A3.
     */
    frame->a2 =
        (uint32_t)(uintptr_t)entry;

    frame->a3 =
        (uint32_t)(uintptr_t)argument;

    frame->ps =
        PS_UM |
        PS_EXCM;

#else

    /*
     * Windowed Xtensa ABI.
     *
     * FreeRTOS initializes a function entered as though it had
     * been CALL4'd, so the first two arguments are placed in A6/A7.
     */
    frame->a6 =
        (uint32_t)(uintptr_t)entry;

    frame->a7 =
        (uint32_t)(uintptr_t)argument;

    /*
     * User mode
     * Exception context
     * Window overflow enabled
     * CALLINC = 1 (CALL4-style entry)
     */
    frame->ps =
        PS_UM |
        PS_EXCM |
        PS_WOE |
        PS_CALLINC(1);

#endif

    return (uint32_t *)stackPointer;
}