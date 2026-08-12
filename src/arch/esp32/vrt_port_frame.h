#ifndef VRT_PORT_FRAME_H
#define VRT_PORT_FRAME_H

#include <stdint.h>
#include <stddef.h>

#include <xtensa/xtensa_context.h>

/*
 * VertexRT uses the Xtensa SDK's canonical exception frame.
 *
 * Do NOT duplicate XtExcFrame here. The layout must remain exactly
 * the same as the Xtensa/FreeRTOS port expects.
 */
typedef XtExcFrame vrt_context_frame_t;

/*
 * C-side offsets.
 *
 * XT_STK_* are assembler symbols and are not available to C.
 */
#define VRT_CTX_EXIT ((uint32_t)offsetof(XtExcFrame, exit))
#define VRT_CTX_PC ((uint32_t)offsetof(XtExcFrame, pc))
#define VRT_CTX_PS ((uint32_t)offsetof(XtExcFrame, ps))

#define VRT_CTX_A0 ((uint32_t)offsetof(XtExcFrame, a0))
#define VRT_CTX_A1 ((uint32_t)offsetof(XtExcFrame, a1))
#define VRT_CTX_A2 ((uint32_t)offsetof(XtExcFrame, a2))
#define VRT_CTX_A3 ((uint32_t)offsetof(XtExcFrame, a3))
#define VRT_CTX_A4 ((uint32_t)offsetof(XtExcFrame, a4))
#define VRT_CTX_A5 ((uint32_t)offsetof(XtExcFrame, a5))
#define VRT_CTX_A6 ((uint32_t)offsetof(XtExcFrame, a6))
#define VRT_CTX_A7 ((uint32_t)offsetof(XtExcFrame, a7))
#define VRT_CTX_A8 ((uint32_t)offsetof(XtExcFrame, a8))
#define VRT_CTX_A9 ((uint32_t)offsetof(XtExcFrame, a9))
#define VRT_CTX_A10 ((uint32_t)offsetof(XtExcFrame, a10))
#define VRT_CTX_A11 ((uint32_t)offsetof(XtExcFrame, a11))
#define VRT_CTX_A12 ((uint32_t)offsetof(XtExcFrame, a12))
#define VRT_CTX_A13 ((uint32_t)offsetof(XtExcFrame, a13))
#define VRT_CTX_A14 ((uint32_t)offsetof(XtExcFrame, a14))
#define VRT_CTX_A15 ((uint32_t)offsetof(XtExcFrame, a15))

#define VRT_CTX_SAR ((uint32_t)offsetof(XtExcFrame, sar))
#define VRT_CTX_EXCCAUSE ((uint32_t)offsetof(XtExcFrame, exccause))
#define VRT_CTX_EXCVADDR ((uint32_t)offsetof(XtExcFrame, excvaddr))

#if XCHAL_HAVE_LOOPS
#define VRT_CTX_LBEG ((uint32_t)offsetof(XtExcFrame, lbeg))
#define VRT_CTX_LEND ((uint32_t)offsetof(XtExcFrame, lend))
#define VRT_CTX_LCOUNT ((uint32_t)offsetof(XtExcFrame, lcount))
#endif

#define VRT_CTX_TMP0 ((uint32_t)offsetof(XtExcFrame, tmp0))
#define VRT_CTX_TMP1 ((uint32_t)offsetof(XtExcFrame, tmp1))
#define VRT_CTX_TMP2 ((uint32_t)offsetof(XtExcFrame, tmp2))

#define VRT_CTX_FRAME_SIZE ((uint32_t)sizeof(XtExcFrame))

/*
 * Verify the fields VertexRT actually depends on.
 */
_Static_assert(
    offsetof(XtExcFrame, exit) == 0x00,
    "XtExcFrame exit offset mismatch");

_Static_assert(
    offsetof(XtExcFrame, pc) == 0x04,
    "XtExcFrame pc offset mismatch");

_Static_assert(
    offsetof(XtExcFrame, ps) == 0x08,
    "XtExcFrame ps offset mismatch");

_Static_assert(
    offsetof(XtExcFrame, a0) == 0x0C,
    "XtExcFrame a0 offset mismatch");

_Static_assert(
    offsetof(XtExcFrame, a1) == 0x10,
    "XtExcFrame a1 offset mismatch");

_Static_assert(
    offsetof(XtExcFrame, a2) == 0x14,
    "XtExcFrame a2 offset mismatch");

_Static_assert(
    offsetof(XtExcFrame, a3) == 0x18,
    "XtExcFrame a3 offset mismatch");

_Static_assert(
    offsetof(XtExcFrame, a15) == 0x48,
    "XtExcFrame a15 offset mismatch");

_Static_assert(
    offsetof(XtExcFrame, sar) == 0x4C,
    "XtExcFrame sar offset mismatch");

_Static_assert(
    offsetof(XtExcFrame, exccause) == 0x50,
    "XtExcFrame exccause offset mismatch");

_Static_assert(
    offsetof(XtExcFrame, excvaddr) == 0x54,
    "XtExcFrame excvaddr offset mismatch");

#if XCHAL_HAVE_LOOPS

_Static_assert(
    offsetof(XtExcFrame, lbeg) == 0x58,
    "XtExcFrame lbeg offset mismatch");

_Static_assert(
    offsetof(XtExcFrame, lend) == 0x5C,
    "XtExcFrame lend offset mismatch");

_Static_assert(
    offsetof(XtExcFrame, lcount) == 0x60,
    "XtExcFrame lcount offset mismatch");

#endif

_Static_assert(
    offsetof(XtExcFrame, tmp0) == 0x64,
    "XtExcFrame tmp0 offset mismatch");

_Static_assert(
    offsetof(XtExcFrame, tmp1) == 0x68,
    "XtExcFrame tmp1 offset mismatch");

_Static_assert(
    offsetof(XtExcFrame, tmp2) == 0x6C,
    "XtExcFrame tmp2 offset mismatch");

/*
 * The ESP32 XtExcFrame itself is the architectural frame.
 * The larger XT_STK_FRMSZ includes additional stack space.
 */
_Static_assert(
    sizeof(XtExcFrame) == 0x70,
    "Unexpected XtExcFrame size");

#endif /* VRT_PORT_FRAME_H */