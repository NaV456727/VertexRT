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
    /* Program Counter */
    uint32_t pc;

    /* Processor Status */
    uint32_t ps;

    /* Return Address */
    uint32_t a0;

    /* Stack Pointer */
    uint32_t a1;

    /* Function Argument */
    uint32_t a2;

    /* General Purpose Registers */
    uint32_t a3;
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

    return alignedStack;
}