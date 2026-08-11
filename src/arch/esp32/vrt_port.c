#include "vrt_port.h"
#include "vrt_port_frame.h"

#include <stdint.h>
#include <string.h>

/*
 * Xtensa PS bits used by the FreeRTOS-style initial
 * task context.
 *
 * PS_UM       = user mode
 * PS_EXCM     = exception mode
 * PS_WOE      = window overflow enable
 * PS_CALLINC  = pretend the task was entered by CALL4
 *
 * For a normal windowed Xtensa task:
 *
 *     PS = PS_UM | PS_EXCM | PS_WOE | PS_CALLINC(1)
 *
 * Which is:
 *
 *     0x00050020
 */

#define VRT_PS_UM 0x00000020U
#define VRT_PS_EXCM 0x00000010U
#define VRT_PS_WOE 0x00040000U
#define VRT_PS_CALLINC_1 0x00010000U

#define VRT_INITIAL_PS \
    (VRT_PS_UM | VRT_PS_EXCM | VRT_PS_WOE | VRT_PS_CALLINC_1)

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
     * Stack grows downward.
     *
     * The frame must be 16-byte aligned.
     */
    uintptr_t top =
        (uintptr_t)stackTop;

    uintptr_t address =
        top - sizeof(vrt_stack_frame_t);

    address &= ~((uintptr_t)VRT_STACK_ALIGNMENT - 1U);

    vrt_stack_frame_t *frame =
        (vrt_stack_frame_t *)address;

    /*
     * Clear the entire initial context.
     */
    memset(
        frame,
        0,
        sizeof(vrt_stack_frame_t));

    /*=====================================================
     * Initial execution state
     *=====================================================*/

    /*
     * Task entry point.
     */
    frame->pc =
        (uint32_t)(uintptr_t)entry;

    /*
     * Initial processor status.
     */
    frame->ps =
        VRT_INITIAL_PS;

    /*=====================================================
     * Stack registers
     *=====================================================*/

    /*
     * A0 = 0
     *
     * This makes the initial task appear at the bottom
     * of the call chain.
     */
    frame->a0 = 0U;

    /*
     * A1 = physical top of the frame.
     */
    frame->a1 =
        (uint32_t)(address +
                   sizeof(vrt_stack_frame_t));

    /*
     * A2 = task argument.
     *
     * This matches the normal Xtensa C calling convention
     * for the task entry function.
     */
    frame->a2 =
        (uint32_t)(uintptr_t)argument;

    /*
     * Remaining registers start at zero.
     */
    frame->a3 = 0U;
    frame->a4 = 0U;
    frame->a5 = 0U;
    frame->a6 = 0U;
    frame->a7 = 0U;
    frame->a8 = 0U;
    frame->a9 = 0U;
    frame->a10 = 0U;
    frame->a11 = 0U;
    frame->a12 = 0U;
    frame->a13 = 0U;
    frame->a14 = 0U;
    frame->a15 = 0U;

    /*=====================================================
     * Special registers
     *=====================================================*/

    frame->sar = 0U;

    frame->exccause = 0U;
    frame->excvaddr = 0U;

    frame->lbeg = 0U;
    frame->lend = 0U;
    frame->lcount = 0U;

    /*=====================================================
     * Window spill/fill temporary storage
     *=====================================================*/

    frame->tmp0 = 0U;
    frame->tmp1 = 0U;
    frame->tmp2 = 0U;

    /*
     * Return the beginning of the saved context.
     */
    return (uint32_t *)frame;
}