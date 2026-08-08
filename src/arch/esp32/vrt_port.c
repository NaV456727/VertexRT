#include "vrt_port.h"
#include <string.h>

/*=========================================================
 * Xtensa Architecture Definitions
 *=========================================================*/

/*
 * Initial processor status.
 *
 * PS_UM   = User mode
 * PS_EXCM  = Exception mode
 *
 * This is the initial state used when entering a task.
 */
#define VRT_INITIAL_PS 0x00060020U

/*=========================================================
 * External Xtensa Functions
 *=========================================================*/

/*
 * ESP32 Xtensa user exception exit dispatcher.
 *
 * The initial task frame is entered through the Xtensa
 * exception-return mechanism, which eventually uses this
 * dispatcher to transition into normal task execution.
 */
extern void _xt_user_exit(void);

/*=========================================================
 * Xtensa Exception Stack Frame
 *=========================================================*/

/*
 * IMPORTANT:
 *
 * This layout follows the classic ESP32 Xtensa XtExcFrame
 * ordering:
 *
 *     exit
 *     pc
 *     ps
 *     a0
 *     a1
 *     a2
 *     ...
 *     a15
 *     sar
 *     exccause
 *     excvaddr
 *     lbeg
 *     lend
 *     lcount
 *
 * This ordering is important because the assembly port and
 * the Xtensa exception machinery depend on fixed offsets.
 */

typedef struct vrt_stack_frame
{
    /*=====================================================
     * Exception Return
     *=====================================================*/

    /*
     * Exception exit / dispatch address.
     */
    uint32_t exit;

    /*=====================================================
     * Processor State
     *=====================================================*/

    /*
     * Program counter to return to.
     */
    uint32_t pc;

    /*
     * Processor status register.
     */
    uint32_t ps;

    /*=====================================================
     * General Purpose Registers
     *=====================================================*/

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

    /*=====================================================
     * Special Registers
     *=====================================================*/

    uint32_t sar;

    /*=====================================================
     * Exception Information
     *=====================================================*/

    uint32_t exccause;
    uint32_t excvaddr;

    /*=====================================================
     * Zero-Overhead Loop Registers
     *=====================================================*/

    uint32_t lbeg;
    uint32_t lend;
    uint32_t lcount;

} vrt_stack_frame_t;

/*=========================================================
 * Stack Initialization
 *=========================================================*/

uint32_t *vrt_port_stack_init(
    uint32_t *stackTop,
    vrt_task_function_t entry,
    void *argument)
{
    uint32_t *alignedStack;
    vrt_stack_frame_t *frame;

    uintptr_t stackAddress;

    /*=====================================================
     * Calculate Initial Frame Address
     *=====================================================*/

    /*
     * Reserve space for the initial exception frame and
     * align the resulting address to 16 bytes.
     */
    stackAddress =
        VRT_ALIGN_DOWN(
            ((uintptr_t)stackTop -
             sizeof(vrt_stack_frame_t)),
            VRT_STACK_ALIGNMENT);

    alignedStack = (uint32_t *)stackAddress;

    /*=====================================================
     * Clear Initial Frame
     *=====================================================*/

    memset(
        alignedStack,
        0,
        sizeof(vrt_stack_frame_t));

    frame = (vrt_stack_frame_t *)alignedStack;

    /*=====================================================
     * Initialize Exception Entry
     *=====================================================*/

    /*
     * When the initial context is restored, the Xtensa
     * exception machinery uses this dispatcher to leave
     * exception context and enter the task.
     */
    frame->exit =
        (uintptr_t)_xt_user_exit;

    /*=====================================================
     * Initialize Program Counter
     *=====================================================*/

    /*
     * The task begins execution at its entry function.
     */
    frame->pc =
        (uintptr_t)entry;

    /*=====================================================
     * Initialize Processor Status
     *=====================================================*/

    frame->ps =
        VRT_INITIAL_PS;

    /*=====================================================
     * Initialize Return Address
     *=====================================================*/

    /*
     * There is no caller for the initial task context.
     *
     * Setting A0 to zero also terminates the backtrace
     * cleanly when debugging.
     */
    frame->a0 = 0;

    /*=====================================================
     * Initialize Stack Pointer
     *=====================================================*/

    /*
     * A1 must point to the physical top of the initial
     * exception frame.
     */
    frame->a1 =
        (uintptr_t)(alignedStack +
                    (sizeof(vrt_stack_frame_t) /
                     sizeof(uint32_t)));

    /*=====================================================
     * Initialize Task Argument
     *=====================================================*/

    /*
     * For the ESP32 call0 ABI, the first function argument
     * is passed through A2.
     */
    frame->a2 =
        (uintptr_t)argument;

    /*=====================================================
     * Initialize Remaining Registers
     *=====================================================*/

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

    /*=====================================================
     * Initialize Special Registers
     *=====================================================*/

    frame->sar = 0;

    /*=====================================================
     * Initialize Exception Information
     *=====================================================*/

    /*
     * These values are irrelevant for a freshly-created
     * task because no exception has occurred yet.
     */
    frame->exccause = 0;
    frame->excvaddr = 0;

    /*=====================================================
     * Initialize Loop Registers
     *=====================================================*/

    frame->lbeg = 0;
    frame->lend = 0;
    frame->lcount = 0;

    /*=====================================================
     * Return Initial Stack Pointer
     *=====================================================*/

    return alignedStack;
}