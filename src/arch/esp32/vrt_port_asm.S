#include <xtensa/coreasm.h>
#include <xtensa/corebits.h>
#include <xtensa/xtensa_context.h>
#include "sdkconfig.h"

.text
.align 4

.extern _xt_context_restore
.extern xthal_window_spill_nw

/*
 * ============================================================================
 * VertexRT ESP32 Xtensa Port
 *
 * Architecture:
 *     Xtensa LX6 / ESP32
 *     Windowed ABI
 *
 * Context model copied from the ESP32 FreeRTOS Xtensa port:
 *
 *     - XtExcFrame for task startup/full context
 *     - XtSolFrame for cooperative task switching
 *     - _xt_context_restore()
 *     - xthal_window_spill_nw()
 *     - retw for solicited frames
 *     - frame->exit for full frames
 *
 * IMPORTANT:
 *
 * A normal C function called through the Xtensa windowed ABI receives its
 * first arguments in A10/A11.
 *
 * Therefore:
 *
 *     vrt_port_start_first_task(sp)
 *         A10 = sp
 *
 *     vrt_port_restore_context(sp)
 *         A10 = sp
 *
 *     vrt_port_switch_context(&saveSp, restoreSp)
 *         A10 = &saveSp
 *         A11 = restoreSp
 *
 * This is intentionally aligned with the FreeRTOS windowed ABI behaviour.
 * ============================================================================
 */


/*
 * ============================================================================
 * XtSolFrame temporary storage
 *
 * For the ESP32 windowed Xtensa configuration used by this project:
 *
 *     0x00 = XT_SOL_EXIT
 *     0x04 = XT_SOL_PC
 *     0x08 = XT_SOL_PS
 *     0x0C = XT_SOL_THREADPTR
 *     0x10 = XT_SOL_NEXT
 *
 * The complete solicited frame is 32 bytes.
 *
 * The remaining words at:
 *
 *     0x18
 *     0x1C
 *
 * are used here as temporary storage for the two C arguments.
 *
 * These are deliberately literal offsets because the assembler rejects
 * expressions involving XT_SOL_NEXT as symbolic operands on this toolchain.
 * ============================================================================
 */

#define VRT_SOL_SAVE_SP       24
#define VRT_SOL_RESTORE_SP    28


/*
 * ============================================================================
 * vrt_port_start_first_task
 *
 * C:
 *
 *     void vrt_port_start_first_task(uint32_t *sp);
 *
 * Windowed ABI input:
 *
 *     A10 = initial XtExcFrame *
 *
 * This mirrors the FreeRTOS full-frame restore path.
 *
 * _xt_context_restore() restores the architectural registers except the
 * return/exception-control portion handled by the exit dispatcher.
 * ============================================================================
 */

.global vrt_port_start_first_task
.type   vrt_port_start_first_task, @function
.align 4

vrt_port_start_first_task:

    /*
     * Normal windowed C argument:
     *
     *     A10 = initial task stack pointer
     */
    mov     a1, a10

    /*
     * Restore the complete Xtensa task context.
     */
    call0   _xt_context_restore

    /*
     * The frame contains the exit dispatcher.
     *
     * This is exactly the same mechanism used by FreeRTOS _frxt_dispatch().
     */
    l32i    a0, a1, XT_STK_EXIT

    /*
     * _xt_user_exit() now performs the final exception return using the
     * restored frame state.
     */
    ret

.size vrt_port_start_first_task, .-vrt_port_start_first_task


/*
 * ============================================================================
 * vrt_port_restore_context
 *
 * C:
 *
 *     void vrt_port_restore_context(uint32_t *sp);
 *
 * Windowed ABI:
 *
 *     A10 = target task SP
 *
 * Used when a task terminates and therefore must not have its own context
 * saved.
 *
 * Target frame can be:
 *
 *     - XtSolFrame
 *     - XtExcFrame
 * ============================================================================
 */

.global vrt_port_restore_context
.type   vrt_port_restore_context, @function
.align 4

vrt_port_restore_context:

    /*
     * A10 = target task SP.
     */
    mov     a1, a10

    /*
     * XT_SOL_EXIT is located at offset 0 as well.
     *
     * 0     -> solicited frame
     * != 0  -> full interrupt/exception frame
     */
    l32i    a2, a1, XT_STK_EXIT

    bnez    a2, .L_restore_full


/*
 * --------------------------------------------------------------------------
 * Restore solicited frame
 * --------------------------------------------------------------------------
 */

.L_restore_solicited:

#if XCHAL_HAVE_THREADPTR

    l32i    a2, a1, XT_SOL_THREADPTR
    wur.threadptr a2

#endif

    /*
     * Restore saved PS and return PC.
     */
    l32i    a2, a1, XT_SOL_PS
    l32i    a0, a1, XT_SOL_PC

    /*
     * Restore processor state.
     */
    wsr     a2, PS
    rsync

    /*
     * Drop the solicited frame.
     */
    addi    a1, a1, XT_SOL_FRMSZ

    /*
     * Resume the task through the windowed return mechanism.
     */
    retw


/*
 * --------------------------------------------------------------------------
 * Restore full XtExcFrame
 * --------------------------------------------------------------------------
 */

.L_restore_full:

    /*
     * Restore complete task state.
     */
    call0   _xt_context_restore

    /*
     * Return through the task's exit dispatcher.
     */
    l32i    a0, a1, XT_STK_EXIT

    ret

.size vrt_port_restore_context, .-vrt_port_restore_context


/*
 * ============================================================================
 * vrt_port_switch_context
 *
 * C:
 *
 *     void vrt_port_switch_context(
 *         uint32_t **saveSpOut,
 *         uint32_t  *restoreSp
 *     );
 *
 * Windowed ABI input:
 *
 *     A10 = saveSpOut
 *     A11 = restoreSp
 *
 * This is deliberately modelled after FreeRTOS vPortYield().
 *
 * Steps:
 *
 *     1. Create XtSolFrame
 *     2. Save PC/PS/threadptr
 *     3. Save A10/A11 in temporary frame slots
 *     4. Spill register windows
 *     5. Restore original PS
 *     6. Disable interrupts
 *     7. Recover A10/A11
 *     8. Mark frame as solicited
 *     9. Store current task SP
 *    10. Load next task SP
 *    11. Restore next task
 *
 * The scheduler has already selected the next task in C.
 * ============================================================================
 */

.global vrt_port_switch_context
.type   vrt_port_switch_context, @function
.align 4

vrt_port_switch_context:

    /*
     * FreeRTOS vPortYield() uses the standard XtSolFrame.
     *
     * For this ESP32 configuration:
     *
     *     XT_SOL_FRMSZ = 32
     */
    entry   sp, XT_SOL_FRMSZ


    /*
     * ----------------------------------------------------------------------
     * Save current task state
     * ----------------------------------------------------------------------
     */

    rsr     a4, PS

    s32i    a0, sp, XT_SOL_PC
    s32i    a4, sp, XT_SOL_PS

#if XCHAL_HAVE_THREADPTR

    rur.threadptr a4
    s32i    a4, sp, XT_SOL_THREADPTR

#endif


    /*
     * ----------------------------------------------------------------------
     * Save the incoming C arguments.
     *
     * IMPORTANT:
     *
     * These are A10/A11, not A2/A3.
     *
     * The FreeRTOS port does not have this problem because vPortYield()
     * takes no arguments.
     *
     * We therefore preserve our two arguments explicitly before calling
     * xthal_window_spill_nw().
     * ----------------------------------------------------------------------
     */

    s32i    a10, sp, VRT_SOL_SAVE_SP
    s32i    a11, sp, VRT_SOL_RESTORE_SP


    /*
     * ----------------------------------------------------------------------
     * FreeRTOS window-spill sequence
     * ----------------------------------------------------------------------
     *
     * Temporarily:
     *
     *     WOE      = 0
     *     INTLEVEL = XCHAL_EXCM_LEVEL
     *
     * This is the same mechanism used by FreeRTOS vPortYield().
     */

    movi    a6, ~(PS_WOE_MASK | PS_INTLEVEL_MASK)

    and     a4, a4, a6

    addi    a4, a4, XCHAL_EXCM_LEVEL

    wsr     a4, PS
    rsync

    /*
     * Spill all outstanding register windows.
     */
    call0   xthal_window_spill_nw


    /*
     * ----------------------------------------------------------------------
     * Restore the task's original PS
     * ----------------------------------------------------------------------
     */

    l32i    a4, sp, XT_SOL_PS

    wsr     a4, PS
    rsync


    /*
     * ----------------------------------------------------------------------
     * Prevent interruption while publishing the saved context
     * ----------------------------------------------------------------------
     */

    rsil    a6, XCHAL_EXCM_LEVEL


    /*
     * ----------------------------------------------------------------------
     * Recover the original C arguments.
     * ----------------------------------------------------------------------
     */

    l32i    a10, sp, VRT_SOL_SAVE_SP
    l32i    a11, sp, VRT_SOL_RESTORE_SP


    /*
     * ----------------------------------------------------------------------
     * Mark this as a solicited frame.
     *
     * FreeRTOS uses XT_SOL_EXIT == 0 for this purpose.
     * ----------------------------------------------------------------------
     */

    movi    a4, 0
    s32i    a4, sp, XT_SOL_EXIT


    /*
     * ----------------------------------------------------------------------
     * Save current task's SP
     *
     *     *saveSpOut = current XtSolFrame
     *
     * A10 = saveSpOut
     * ----------------------------------------------------------------------
     */

    s32i    sp, a10, 0


    /*
     * ----------------------------------------------------------------------
     * Switch to next task
     *
     * A11 = next task SP
     * ----------------------------------------------------------------------
     */

    mov     sp, a11


    /*
     * ----------------------------------------------------------------------
     * Determine target frame type
     * ----------------------------------------------------------------------
     */

    l32i    a4, sp, XT_STK_EXIT

    bnez    a4, .L_switch_full


/*
 * --------------------------------------------------------------------------
 * Restore solicited frame
 * --------------------------------------------------------------------------
 */

.L_switch_solicited:

#if XCHAL_HAVE_THREADPTR

    l32i    a2, sp, XT_SOL_THREADPTR
    wur.threadptr a2

#endif

    /*
     * Restore target PS and return PC.
     */
    l32i    a4, sp, XT_SOL_PS
    l32i    a0, sp, XT_SOL_PC

    /*
     * Restore processor state.
     */
    wsr     a4, PS
    rsync

    /*
     * Remove target's solicited frame.
     */
    addi    sp, sp, XT_SOL_FRMSZ

    /*
     * Resume target task.
     */
    retw


/*
 * --------------------------------------------------------------------------
 * Restore full XtExcFrame
 * --------------------------------------------------------------------------
 */

.L_switch_full:

    /*
     * Restore complete architectural state.
     */
    call0   _xt_context_restore

    /*
     * Return through target task's exit dispatcher.
     */
    l32i    a0, sp, XT_STK_EXIT

    ret

.size vrt_port_switch_context, .-vrt_port_switch_context