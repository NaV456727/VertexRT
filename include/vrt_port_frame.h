#ifndef VRT_PORT_FRAME_H
#define VRT_PORT_FRAME_H

#include <stdint.h>
#include <stddef.h>

#include <xtensa/xtensa_context.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * ============================================================================
     * VertexRT Xtensa context-frame definitions
     *
     * The actual frame layout is owned by the Xtensa SDK.
     *
     * C code:
     *     Uses offsetof()/sizeof() on XtExcFrame.
     *
     * Assembly:
     *     Uses XT_STK_* / XT_SOL_* from xtensa_context.h.
     *
     * We therefore do NOT duplicate the architecture's frame layout here.
     * ============================================================================
     */

    typedef XtExcFrame vrt_context_frame_t;
    typedef XtSolFrame vrt_solicited_frame_t;

    /*
     * ============================================================================
     * Full XtExcFrame offsets
     * ============================================================================
     *
     * These names are consumed by vrt_offsets.c.
     */

#define VRT_CTX_EXIT offsetof(XtExcFrame, exit)
#define VRT_CTX_PC offsetof(XtExcFrame, pc)
#define VRT_CTX_PS offsetof(XtExcFrame, ps)

#define VRT_CTX_A0 offsetof(XtExcFrame, a0)
#define VRT_CTX_A1 offsetof(XtExcFrame, a1)
#define VRT_CTX_A2 offsetof(XtExcFrame, a2)
#define VRT_CTX_A3 offsetof(XtExcFrame, a3)
#define VRT_CTX_A4 offsetof(XtExcFrame, a4)
#define VRT_CTX_A5 offsetof(XtExcFrame, a5)
#define VRT_CTX_A6 offsetof(XtExcFrame, a6)
#define VRT_CTX_A7 offsetof(XtExcFrame, a7)
#define VRT_CTX_A8 offsetof(XtExcFrame, a8)
#define VRT_CTX_A9 offsetof(XtExcFrame, a9)
#define VRT_CTX_A10 offsetof(XtExcFrame, a10)
#define VRT_CTX_A11 offsetof(XtExcFrame, a11)
#define VRT_CTX_A12 offsetof(XtExcFrame, a12)
#define VRT_CTX_A13 offsetof(XtExcFrame, a13)
#define VRT_CTX_A14 offsetof(XtExcFrame, a14)
#define VRT_CTX_A15 offsetof(XtExcFrame, a15)

#define VRT_CTX_SAR offsetof(XtExcFrame, sar)

#define VRT_CTX_EXCCAUSE offsetof(XtExcFrame, exccause)
#define VRT_CTX_EXCVADDR offsetof(XtExcFrame, excvaddr)

#if XCHAL_HAVE_LOOPS

#define VRT_CTX_LBEG offsetof(XtExcFrame, lbeg)
#define VRT_CTX_LEND offsetof(XtExcFrame, lend)
#define VRT_CTX_LCOUNT offsetof(XtExcFrame, lcount)

#endif

#define VRT_CTX_TMP0 offsetof(XtExcFrame, tmp0)
#define VRT_CTX_TMP1 offsetof(XtExcFrame, tmp1)
#define VRT_CTX_TMP2 offsetof(XtExcFrame, tmp2)

#define VRT_CTX_FRAME_SIZE sizeof(XtExcFrame)

    /*
     * ============================================================================
     * Compile-time sanity checks
     * ============================================================================
     */

    _Static_assert(
        sizeof(XtExcFrame) > 0,
        "XtExcFrame must have non-zero size");

    _Static_assert(
        sizeof(XtSolFrame) > 0,
        "XtSolFrame must have non-zero size");

    _Static_assert(
        (sizeof(XtExcFrame) % sizeof(uint32_t)) == 0,
        "XtExcFrame must be word aligned");

    _Static_assert(
        (sizeof(XtSolFrame) % sizeof(uint32_t)) == 0,
        "XtSolFrame must be word aligned");

    /*
     * ============================================================================
     * Do NOT use XT_SOL_FRMSZ here.
     *
     * XT_SOL_FRMSZ depends on XtSolFrameSize, which is not available as a
     * C expression in this SDK header configuration.
     *
     * Assembly code will continue to use:
     *
     *     XT_STK_FRMSZ
     *     XT_SOL_FRMSZ
     *
     * directly from xtensa_context.h.
     * ============================================================================
     */

#ifdef __cplusplus
}
#endif

#endif /* VRT_PORT_FRAME_H */