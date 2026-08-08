/*
 *=========================================================
 * VertexRT
 * ESP32 Xtensa Port
 *=========================================================
 */

    .text
    .align  4

#include "vrt_offsets.h"

/*=========================================================
 * External Functions
 *=========================================================*/

    .extern vrt_scheduler_get_instance


/*=========================================================
 * vrt_port_start_first_task
 *
 * Start the task selected by the scheduler.
 *
 * Flow:
 *
 *     scheduler
 *         |
 *         v
 *     currentTask
 *         |
 *         v
 *     currentTask->sp
 *         |
 *         v
 *     initial stack frame
 *         |
 *         v
 *     restore CPU context
 *         |
 *         v
 *     task entry
 *=========================================================*/

    .global vrt_port_start_first_task
.type   vrt_port_start_first_task, @function

vrt_port_start_first_task:

    /*
     * Get the active scheduler.
     *
     * A2 = vrt_scheduler_t *
     */
    call0   vrt_scheduler_get_instance

    /*
     * A2 = scheduler->currentTask
     */
    l32i    a2, a2, VRT_SCHEDULER_CURRENT_TASK_OFFSET

    /*
     * A2 = currentTask->sp
     *
     * A2 now points to the initial stack frame.
     */
    l32i    a2, a2, VRT_TASK_SP_OFFSET


    /*
     *=====================================================
     * Preserve frame pointer
     *=====================================================
     *
     * A2 = frame
     * A3 = frame
     *
     * A3 remains the frame pointer until every required
     * value has been loaded.
     */
    mov     a3, a2


    /*
     *=====================================================
     * Load values that we need later
     *=====================================================
     *
     * A4 = task entry PC
     * A5 = initial PS
     */
    l32i    a4, a3, VRT_FRAME_PC
    l32i    a5, a3, VRT_FRAME_PS


    /*
     *=====================================================
     * Restore registers
     *=====================================================
     *
     * A2 and A3 are deliberately restored last.
     */

    l32i    a0,  a3, VRT_FRAME_A0
    l32i    a1,  a3, VRT_FRAME_A1

    l32i    a6,  a3, VRT_FRAME_A6
    l32i    a7,  a3, VRT_FRAME_A7
    l32i    a8,  a3, VRT_FRAME_A8
    l32i    a9,  a3, VRT_FRAME_A9
    l32i    a10, a3, VRT_FRAME_A10
    l32i    a11, a3, VRT_FRAME_A11
    l32i    a12, a3, VRT_FRAME_A12
    l32i    a13, a3, VRT_FRAME_A13
    l32i    a14, a3, VRT_FRAME_A14
    l32i    a15, a3, VRT_FRAME_A15


    /*
     *=====================================================
     * Restore special registers
     *=====================================================
     */

    l32i    a6, a3, VRT_FRAME_SAR
    wsr     a6, sar
    rsync

    l32i    a6, a3, VRT_FRAME_LBEG
    wsr     a6, lbeg
    rsync

    l32i    a6, a3, VRT_FRAME_LEND
    wsr     a6, lend
    rsync

    l32i    a6, a3, VRT_FRAME_LCOUNT
    wsr     a6, lcount
    rsync


    /*
     *=====================================================
     * Restore processor status
     *=====================================================
     *
     * A5 contains the saved PS value.
     */
    wsr     a5, ps
    rsync


    /*
     *=====================================================
     * Restore A2
     *=====================================================
     *
     * This must happen after all frame accesses using A3.
     */
    l32i    a2, a3, VRT_FRAME_A2


    /*
     *=====================================================
     * Restore A3
     *=====================================================
     *
     * This is the final access through the frame pointer.
     */
    l32i    a3, a3, VRT_FRAME_A3


    /*
     *=====================================================
     * Start task
     *=====================================================
     *
     * A4 still contains the task entry address.
     */
    jx      a4

/*=========================================================
 * vrt_port_switch_context
 *
 * Context switching will save the current task context
 * into its stack frame and restore the next task's frame.
 *
 * This is intentionally separate from first-task startup.
 *=========================================================*/

    .global vrt_port_switch_context
    .type   vrt_port_switch_context, @function

vrt_port_switch_context:

    /*
     * Context save/restore path will be implemented here.
     */

    retw


    .size vrt_port_switch_context, .-vrt_port_switch_context