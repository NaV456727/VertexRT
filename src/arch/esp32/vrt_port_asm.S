/*
 *=========================================================
 * VertexRT
 * ESP32 Xtensa Port
 *
 * Initial task startup + voluntary context switching.
 *
 * This implementation uses the Xtensa windowed ABI model
 * used by the ESP32 FreeRTOS port.
 *=========================================================
 */

    .text
    .align  4

#include "vrt_offsets.h"


/*=========================================================
 * External C functions
 *=========================================================*/

    .extern vrt_scheduler_get_instance
    .extern vrt_scheduler_schedule


/*=========================================================
 * vrt_port_start_first_task
 *
 * Start the task selected by VertexRT.
 *
 * At entry:
 *
 *     no task context is active
 *
 * We obtain:
 *
 *     scheduler
 *         ↓
 *     currentTask
 *         ↓
 *     currentTask->sp
 *         ↓
 *     initial Xtensa frame
 *
 * and restore the initial task state.
 *=========================================================*/

    .global vrt_port_start_first_task
    .type   vrt_port_start_first_task, @function

vrt_port_start_first_task:

    /*
     * A2 = scheduler
     */
    call0   vrt_scheduler_get_instance

    /*
     * A2 = scheduler->currentTask
     */
    l32i    a2, a2, VRT_SCHEDULER_CURRENT_TASK_OFFSET

    /*
     * A2 = currentTask->sp
     */
    l32i    a2, a2, VRT_TASK_SP_OFFSET

    /*
     * A3 = frame pointer
     */
    mov     a3, a2


    /*=====================================================
     * Load values required after register restoration
     *=====================================================*/

    l32i    a4, a3, VRT_FRAME_PC
    l32i    a5, a3, VRT_FRAME_PS


    /*=====================================================
     * Restore A0-A15
     *
     * A2 and A3 are restored last.
     *=====================================================*/

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


    /*=====================================================
     * Restore SAR
     *=====================================================*/

    l32i    a6, a3, VRT_FRAME_SAR

    wsr     a6, sar
    rsync


    /*=====================================================
     * Restore loop registers
     *=====================================================*/

    l32i    a6, a3, VRT_FRAME_LBEG
    wsr     a6, lbeg
    rsync

    l32i    a6, a3, VRT_FRAME_LEND
    wsr     a6, lend
    rsync

    l32i    a6, a3, VRT_FRAME_LCOUNT
    wsr     a6, lcount
    rsync


    /*=====================================================
     * Restore processor status
     *=====================================================*/

    wsr     a5, ps
    rsync


    /*=====================================================
     * Restore A2
     *=====================================================*/

    l32i    a2, a3, VRT_FRAME_A2


    /*=====================================================
     * Restore A3
     *=====================================================*/

    l32i    a3, a3, VRT_FRAME_A3


    /*=====================================================
     * Jump to task entry
     *=====================================================*/

    jx      a4

    .size vrt_port_start_first_task, .-vrt_port_start_first_task



/*=========================================================
 * vrt_port_switch_context
 *
 * Voluntary context switch.
 *
 * IMPORTANT:
 *
 * This first version deliberately keeps the same basic
 * frame convention as the initial task context.
 *
 * Current stack:
 *
 *     A1
 *      │
 *      ▼
 *   +----------+
 *   | context  |
 *   | 112 byte |
 *   +----------+
 *
 * The current task's SP is saved into its TCB.
 * The scheduler selects another task.
 * The selected task's context is then restored.
 *=========================================================*/

    .global vrt_port_switch_context
    .type   vrt_port_switch_context, @function

vrt_port_switch_context:

    /*=====================================================
     * Allocate context frame
     *=====================================================*/

    addi    a1, a1, -VRT_FRAME_SIZE


    /*=====================================================
     * Save general registers
     *=====================================================*/

    s32i    a0,  a1, VRT_FRAME_A0

    /*
     * Save A2-A15 before using them.
     */
    s32i    a2,  a1, VRT_FRAME_A2
    s32i    a3,  a1, VRT_FRAME_A3
    s32i    a4,  a1, VRT_FRAME_A4
    s32i    a5,  a1, VRT_FRAME_A5
    s32i    a6,  a1, VRT_FRAME_A6
    s32i    a7,  a1, VRT_FRAME_A7
    s32i    a8,  a1, VRT_FRAME_A8
    s32i    a9,  a1, VRT_FRAME_A9
    s32i    a10, a1, VRT_FRAME_A10
    s32i    a11, a1, VRT_FRAME_A11
    s32i    a12, a1, VRT_FRAME_A12
    s32i    a13, a1, VRT_FRAME_A13
    s32i    a14, a1, VRT_FRAME_A14
    s32i    a15, a1, VRT_FRAME_A15


    /*=====================================================
     * Save original A1
     *=====================================================*/

    addi    a2, a1, VRT_FRAME_SIZE

    s32i    a2, a1, VRT_FRAME_A1


    /*=====================================================
     * Save processor state
     *=====================================================*/

    rsr     a2, ps
    s32i    a2, a1, VRT_FRAME_PS

    rsr     a2, sar
    s32i    a2, a1, VRT_FRAME_SAR

    rsr     a2, lbeg
    s32i    a2, a1, VRT_FRAME_LBEG

    rsr     a2, lend
    s32i    a2, a1, VRT_FRAME_LEND

    rsr     a2, lcount
    s32i    a2, a1, VRT_FRAME_LCOUNT


    /*=====================================================
     * Save resume PC
     *=====================================================*/

    /*
     * A0 is the return address created by the call0
     * instruction that entered this function.
     *
     * Save it as the resume address.
     */
    l32i    a2, a1, VRT_FRAME_A0
    s32i    a2, a1, VRT_FRAME_PC


    /*=====================================================
     * Save unused fields
     *=====================================================*/

    movi    a2, 0

    s32i    a2, a1, VRT_FRAME_EXIT
    s32i    a2, a1, VRT_FRAME_EXCCAUSE
    s32i    a2, a1, VRT_FRAME_EXCVADDR

    s32i    a2, a1, VRT_FRAME_TMP0
    s32i    a2, a1, VRT_FRAME_TMP1
    s32i    a2, a1, VRT_FRAME_TMP2


    /*=====================================================
     * Preserve frame pointer
     *=====================================================*/

    /*
     * A8 is callee-saved by the Xtensa windowed ABI.
     *
     * The original A8 has already been saved into the
     * frame, so we can use it as our temporary frame
     * pointer across the scheduler calls.
     */
    mov     a8, a1


    /*=====================================================
     * Restore normal stack pointer for C code
     *=====================================================*/

    l32i    a1, a8, VRT_FRAME_A1


    /*=====================================================
     * Save current task SP
     *=====================================================*/

    call0   vrt_scheduler_get_instance

    /*
     * A2 = scheduler->currentTask
     */
    l32i    a2, a2, VRT_SCHEDULER_CURRENT_TASK_OFFSET

    /*
     * currentTask->sp = frame
     */
    s32i    a8, a2, VRT_TASK_SP_OFFSET


    /*=====================================================
     * Run scheduler
     *=====================================================*/

    call0   vrt_scheduler_get_instance

    /*
     * A2 = scheduler
     */
    call0   vrt_scheduler_schedule


    /*=====================================================
     * Obtain selected task
     *=====================================================*/

    call0   vrt_scheduler_get_instance

    /*
     * A2 = scheduler->currentTask
     */
    l32i    a2, a2, VRT_SCHEDULER_CURRENT_TASK_OFFSET

    /*
     * A2 = nextTask->sp
     */
    l32i    a2, a2, VRT_TASK_SP_OFFSET


    /*
     * A3 = selected frame
     */
    mov     a3, a2


    /*=====================================================
     * Load values required later
     *=====================================================*/

    l32i    a4, a3, VRT_FRAME_PC
    l32i    a5, a3, VRT_FRAME_PS


    /*=====================================================
     * Restore registers
     *=====================================================*/

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


    /*=====================================================
     * Restore special registers
     *=====================================================*/

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


    /*=====================================================
     * Restore PS
     *=====================================================*/

    wsr     a5, ps
    rsync


    /*=====================================================
     * Restore A2
     *=====================================================*/

    l32i    a2, a3, VRT_FRAME_A2


    /*=====================================================
     * Restore A3
     *=====================================================*/

    l32i    a3, a3, VRT_FRAME_A3


    /*=====================================================
     * Resume
     *=====================================================*/

    jx      a4

    .size vrt_port_switch_context, .-vrt_port_switch_context