#include <xtensa/coreasm.h>
#include <xtensa/corebits.h>
#include <xtensa/xtensa_context.h>

.text
.align 4

.extern _xt_context_restore
.extern xthal_window_spill_nw
.extern _xt_user_exit

/*
 * ============================================================================
 * VertexRT ESP32 Xtensa port
 *
 * The low-level Xtensa context machinery is taken from the ESP32 FreeRTOS
 * Xtensa port model:
 *
 *   - XtExcFrame for full contexts
 *   - XtSolFrame for solicited contexts
 *   - _xt_context_restore()
 *   - xthal_window_spill_nw()
 *   - retw for solicited contexts
 *   - frame->exit for full contexts
 *
 * VertexRT differs from FreeRTOS in one important respect:
 *
 *   The C scheduler has already selected the next task.
 *
 * Therefore vrt_port_switch_context() does NOT call a scheduler dispatcher.
 * It receives:
 *
 *   a2 = &currentTask->sp
 *   a3 = nextTask->sp
 *
 * and performs only the architecture-level save/restore.
 *
 * ============================================================================
 */


/*
 * ============================================================================
 * ESP32 XtSolFrame scratch area
 *
 * On the ESP32 Xtensa windowed ABI:
 *
 *     XT_SOL_EXIT       = 0
 *     XT_SOL_PC         = 4
 *     XT_SOL_PS         = 8
 *     XT_SOL_THREADPTR  = 12
 *     XT_SOL_NEXT       = 16
 *
 * XT_SOL_FRMSZ is aligned beyond this area.
 *
 * VertexRT uses the remaining words for the two C arguments:
 *
 *     16  -> saveSpOut
 *     20  -> restoreSp
 *
 * These values must survive xthal_window_spill_nw(), which is a CALL0
 * routine and may destroy normal argument registers.
 *
 * ============================================================================
 */

#define VRT_SOL_SAVE_SP      16
#define VRT_SOL_RESTORE_SP   20


/*
 * ============================================================================
 * vrt_port_start_first_task
 *
 * C prototype:
 *
 *     void vrt_port_start_first_task(uint32_t *sp);
 *
 * Entry:
 *
 *     a2 = initial XtExcFrame *
 *
 * The initial task is represented by a complete exception frame.
 *
 * This follows the FreeRTOS full-frame restore path:
 *
 *     A1 = frame
 *     _xt_context_restore()
 *     A0 = frame->exit
 *     ret
 *
 * _xt_user_exit then performs the final exception return.
 *
 * Does not return.
 * ============================================================================
 */

.global vrt_port_start_first_task
.type   vrt_port_start_first_task, @function
.align  4

/*
 * Called from the normal Xtensa windowed ABI.
 *
 * C prototype:
 *
 *     void vrt_port_start_first_task(uint32_t *sp);
 *
 * Therefore:
 *
 *     A10 = sp
 *
 * This function never returns.
 */

vrt_port_start_first_task:

    /*
     * A10 contains task->sp.
     *
     * Switch to the task's initial Xtensa exception frame.
     */
    mov     a1, a10

    /*
     * Restore the general Xtensa register context.
     *
     * _xt_context_restore() does not restore the architectural
     * return state (PC/PS/A0/A1).
     */
    call0   _xt_context_restore

    /*
     * The first word of the frame is the Xtensa exit dispatcher.
     *
     * For our FreeRTOS-generated task frame this should be:
     *
     *     _xt_user_exit
     */
    l32i    a0, a1, XT_STK_EXIT

    /*
     * Do NOT return through the original CALL8 frame.
     *
     * We are abandoning the scheduler's stack and jumping directly
     * into the Xtensa exception exit dispatcher.
     */
    jx      a0

/*
 * ============================================================================
 * vrt_port_restore_context
 *
 * C prototype:
 *
 *     void vrt_port_restore_context(uint32_t *sp);
 *
 * Used when the current task is terminating and therefore must not be saved.
 *
 * Entry:
 *
 *     a2 = target task SP
 *
 * Target frame:
 *
 *     XT_STK_EXIT == 0  -> XtSolFrame
 *     XT_STK_EXIT != 0  -> XtExcFrame
 *
 * ============================================================================
 */

.global vrt_port_restore_context
.type   vrt_port_restore_context, @function
.align 4

vrt_port_restore_context:

    /*
     * a2 = target task SP
     */
    mov     a1, a2

    /*
     * Determine frame type.
     */
    l32i    a3, a1, XT_STK_EXIT

    bnez    a3, .L_restore_full


/*
 * --------------------------------------------------------------------------
 * Solicited frame
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
    l32i    a3, a1, XT_SOL_PS
    l32i    a0, a1, XT_SOL_PC

    /*
     * Publish PS before returning.
     */
    wsr     a3, PS
    rsync

    /*
     * Windowed ABI:
     *
     * The XtSolFrame was created using ENTRY, so retw restores the
     * previous register-window state.
     */
    addi    a1, a1, XT_SOL_FRMSZ

    retw


/*
 * --------------------------------------------------------------------------
 * Full XtExcFrame
 * --------------------------------------------------------------------------
 */

.L_restore_full:

    /*
     * Restore the complete architectural context.
     */
    call0   _xt_context_restore

    /*
     * Restore the exception exit dispatcher.
     */
    l32i    a0, a1, XT_STK_EXIT

    /*
     * Return through _xt_user_exit / corresponding dispatcher.
     */
    ret


.size vrt_port_restore_context, .-vrt_port_restore_context


/*
 * ============================================================================
 * vrt_port_switch_context
 *
 * C prototype:
 *
 *     void vrt_port_switch_context(
 *         uint32_t **saveSpOut,
 *         uint32_t  *restoreSp
 *     );
 *
 * Entry:
 *
 *     a2 = &currentTask->sp
 *     a3 = nextTask->sp
 *
 * The C scheduler has already selected the target.
 *
 * This routine:
 *
 *     1. creates XtSolFrame
 *     2. saves current PC/PS/threadptr
 *     3. saves the C arguments in scratch words
 *     4. performs FreeRTOS-style window spilling
 *     5. publishes current SP
 *     6. loads next SP
 *     7. restores XtSolFrame or XtExcFrame
 *
 * ============================================================================
 */

.global vrt_port_switch_context
.type   vrt_port_switch_context, @function
.align 4

vrt_port_switch_context:

    /*
     * Allocate the normal solicited frame.
     *
     * This matches FreeRTOS vPortYield() for the windowed ABI.
     */
    entry   a1, XT_SOL_FRMSZ


    /*
     * ----------------------------------------------------------------------
     * Save current task state
     * ----------------------------------------------------------------------
     */

    rsr     a4, PS

    s32i    a0, a1, XT_SOL_PC
    s32i    a4, a1, XT_SOL_PS

#if XCHAL_HAVE_THREADPTR

    rur.threadptr a4
    s32i    a4, a1, XT_SOL_THREADPTR

#endif


    /*
     * ----------------------------------------------------------------------
     * Preserve VertexRT switch arguments
     * ----------------------------------------------------------------------
     *
     * xthal_window_spill_nw() is CALL0 and can destroy normal argument
     * registers.
     *
     * The values are therefore kept in the unused/padding portion of the
     * XtSolFrame.
     */

    s32i    a2, a1, VRT_SOL_SAVE_SP
    s32i    a3, a1, VRT_SOL_RESTORE_SP


    /*
     * ----------------------------------------------------------------------
     * FreeRTOS-style register-window spill
     * ----------------------------------------------------------------------
     *
     * Temporarily:
     *
     *     WOE      = 0
     *     INTLEVEL = XCHAL_EXCM_LEVEL
     *
     * so register-window overflow handling can spill the outstanding
     * windows safely.
     */

    movi    a5, ~(PS_WOE_MASK | PS_INTLEVEL_MASK)

    and     a4, a4, a5

    addi    a4, a4, XCHAL_EXCM_LEVEL

    wsr     a4, PS
    rsync

    /*
     * Spill outstanding register windows.
     */
    call0   xthal_window_spill_nw


    /*
     * ----------------------------------------------------------------------
     * Restore original task PS
     * ----------------------------------------------------------------------
     */

    l32i    a4, a1, XT_SOL_PS

    wsr     a4, PS
    rsync


    /*
     * ----------------------------------------------------------------------
     * Prevent interrupts while publishing the new task state.
     * ----------------------------------------------------------------------
     */

    rsil    a5, XCHAL_EXCM_LEVEL


    /*
     * ----------------------------------------------------------------------
     * Recover VertexRT arguments
     * ----------------------------------------------------------------------
     */

    l32i    a2, a1, VRT_SOL_SAVE_SP
    l32i    a3, a1, VRT_SOL_RESTORE_SP


    /*
     * ----------------------------------------------------------------------
     * Mark this frame as solicited
     * ----------------------------------------------------------------------
     *
     * FreeRTOS uses zero in XT_SOL_EXIT to distinguish a solicited frame
     * from an exception/full frame.
     */

    movi    a4, 0

    s32i    a4, a1, XT_SOL_EXIT


    /*
     * ----------------------------------------------------------------------
     * Save current task SP
     * ----------------------------------------------------------------------
     *
     *     *saveSpOut = current task's XtSolFrame
     */

    s32i    a1, a2, 0


    /*
     * ----------------------------------------------------------------------
     * Switch to selected task
     * ----------------------------------------------------------------------
     */

    mov     a1, a3


    /*
     * ----------------------------------------------------------------------
     * Determine target frame type
     * ----------------------------------------------------------------------
     */

    l32i    a4, a1, XT_STK_EXIT

    bnez    a4, .L_switch_full


/*
 * --------------------------------------------------------------------------
 * Solicited target
 * --------------------------------------------------------------------------
 */

.L_switch_solicited:

#if XCHAL_HAVE_THREADPTR

    l32i    a2, a1, XT_SOL_THREADPTR
    wur.threadptr a2

#endif

    /*
     * Restore target task's minimal context.
     */
    l32i    a4, a1, XT_SOL_PS
    l32i    a0, a1, XT_SOL_PC

    /*
     * Restore processor status.
     */
    wsr     a4, PS
    rsync

    /*
     * Return through Xtensa window restoration.
     */
    addi    a1, a1, XT_SOL_FRMSZ

    retw


/*
 * --------------------------------------------------------------------------
 * Full XtExcFrame target
 * --------------------------------------------------------------------------
 */

.L_switch_full:

    /*
     * Restore full architectural context.
     */
    call0   _xt_context_restore

    /*
     * Full frames return through their saved exit dispatcher.
     */
    l32i    a0, a1, XT_STK_EXIT

    ret


.size vrt_port_switch_context, .-vrt_port_switch_context