#include "vrt_port.h"
#include <string.h>

/*=========================================================
 * Xtensa Initial Register Values
 *========================================================*/

#define VRT_INITIAL_PS 0x00060020U

/*=========================================================
 * Xtensa Initial Stack Frame
 *========================================================*/

typedef struct vrt_stack_frame
{
    /*=====================================================
     * Exception Frame
     *====================================================*/

    /* Program Counter */
    uint32_t pc;

    /* Processor Status */
    uint32_t ps;

    /* Return Address */
    uint32_t a0;

    /* Stack Pointer */
    uint32_t a1;

    /* Function Arguments */
    uint32_t a2;
    uint32_t a3;

    /* General Purpose Registers */
    uint32_t a4;
    uint32_t a5;
    uint32_t a6;
    uint32_t a7;
    uint32_t a8;
    uint32_t a9;
    uint32_t a10;
    uint32_t a11;
    uint32_t a12;
    uint32_t a13;
    uint32_t a14;
    uint32_t a15;

    /*=====================================================
     * Special Registers
     *====================================================*/

    /* Shift Amount Register */
    uint32_t sar;

    /* Zero-Overhead Loop Registers */
    uint32_t lbeg;
    uint32_t lend;
    uint32_t lcount;

    /* Exception Exit Handler */
    uint32_t exit;

} vrt_stack_frame_t;

uint32_t *vrt_port_stack_init(
    uint32_t *stackTop,
    vrt_task_function_t entry,
    void *argument)
{
    uint32_t *alignedStack;
    vrt_stack_frame_t *frame;

    /*
     * Align stack to a 16-byte boundary.
     */
    uintptr_t stackAddress;

    stackAddress = VRT_ALIGN_DOWN(
        ((uintptr_t)stackTop - sizeof(vrt_stack_frame_t)),
        VRT_STACK_ALIGNMENT);

    alignedStack = (uint32_t *)stackAddress;

    /*
     * Clear the initial stack frame.
     */
    memset(
        alignedStack,
        0,
        sizeof(vrt_stack_frame_t));

    frame = (vrt_stack_frame_t *)alignedStack;

    /*
     * Initialize CPU registers.
     */

    /* Program Counter */
    frame->pc = (uintptr_t)entry;

    /* Initial Processor Status */
    frame->ps = VRT_INITIAL_PS;

    /* Return Address */
    frame->a0 = 0;

    /* Initial Stack Pointer */
    frame->a1 =
        (uintptr_t)(alignedStack + (sizeof(vrt_stack_frame_t) / sizeof(uint32_t)));

    /* Task Argument */
    frame->a2 = (uintptr_t)argument;

    /*
     * Initialize remaining registers.
     */

    frame->a3 = 0;
    frame->a4 = 0;
    frame->a5 = 0;
    frame->a6 = 0;
    frame->a7 = 0;
    frame->a8 = 0;
    frame->a9 = 0;
    frame->a10 = 0;
    frame->a11 = 0;
    frame->a12 = 0;
    frame->a13 = 0;
    frame->a14 = 0;
    frame->a15 = 0;

    /* Special Registers */
    frame->sar = 0;
    frame->lbeg = 0;
    frame->lend = 0;
    frame->lcount = 0;

    /*
     * Exception exit handler.
     *
     * TODO:
     * Replace with the architecture-specific
     * context restore routine.
     */
    frame->exit = 0;

    return alignedStack;
}