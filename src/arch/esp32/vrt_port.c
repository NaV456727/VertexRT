#include "vrt_port.h"

#include <stdint.h>
#include <string.h>

/*=========================================================
 * Xtensa Architecture Definitions
 *=========================================================*/

/*
 * Initial processor status.
 *
 * This value is suitable for starting a normal task
 * context on the classic ESP32 Xtensa core.
 */
#define VRT_INITIAL_PS 0x00060020U

/*=========================================================
 * Initial Task Stack Frame
 *=========================================================*/

/*
 * This is VertexRT's software-defined initial context.
 *
 * IMPORTANT:
 *
 * This frame is used by our own first-task startup
 * assembly. It is NOT pretending to be an Xtensa
 * exception frame.
 *
 * Keeping this distinction makes the port much easier
 * to reason about.
 */

typedef struct vrt_stack_frame
{
    uint32_t a0;
    uint32_t a1;
    uint32_t a2;
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

    uint32_t ps;
    uint32_t sar;

    uint32_t lbeg;
    uint32_t lend;
    uint32_t lcount;

    uint32_t pc;

} vrt_stack_frame_t;

/*=========================================================
 * Stack Initialization
 *=========================================================*/

uint32_t *vrt_port_stack_init(
    uint32_t *stackTop,
    vrt_task_function_t entry,
    void *argument)
{
    if (stackTop == NULL ||
        entry == NULL)
    {
        return NULL;
    }

    /*
     * Reserve space for our initial CPU context.
     */
    uintptr_t address =
        (uintptr_t)stackTop -
        sizeof(vrt_stack_frame_t);

    /*
     * ESP32 ABI requires stack alignment.
     */
    address &=
        ~((uintptr_t)VRT_STACK_ALIGNMENT - 1U);

    vrt_stack_frame_t *frame =
        (vrt_stack_frame_t *)address;

    /*
     * Start with a clean frame.
     */
    memset(
        frame,
        0,
        sizeof(vrt_stack_frame_t));

    /*=====================================================
     * Initial CPU State
     *=====================================================*/

    /*
     * Task entry point.
     */
    frame->pc =
        (uint32_t)(uintptr_t)entry;

    /*
     * Initial processor state.
     */
    frame->ps =
        VRT_INITIAL_PS;

    /*
     * A0 is the return address.
     *
     * A task should never return normally.
     * vrt_task_exit() will eventually handle this.
     */
    frame->a0 = 0;

    /*
     * A1 is the stack pointer.
     *
     * Point it above the initial frame.
     */
    frame->a1 =
        (uint32_t)(address +
                   sizeof(vrt_stack_frame_t));

    /*
     * First function argument.
     *
     * VertexRT task functions use:
     *
     *     void task(void *argument)
     *
     * so the argument is placed in A2.
     */
    frame->a2 =
        (uint32_t)(uintptr_t)argument;

    /*
     * Remaining registers start cleared.
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

    /*
     * Special registers.
     */
    frame->sar = 0;

    frame->lbeg = 0;
    frame->lend = 0;
    frame->lcount = 0;

    /*
     * Return the address of the saved context.
     */
    return (uint32_t *)frame;
}