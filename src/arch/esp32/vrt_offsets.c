#include "vrt_port_frame.h"
#include "vrt_offsets.h"

#include <stddef.h>

/* --------------------------------------------------------------------------
 * Register/context offset verification
 * -------------------------------------------------------------------------- */

_Static_assert(
    offsetof(vrt_context_frame_t, exit) == VRT_CTX_EXIT,
    "exit offset mismatch");

_Static_assert(
    offsetof(vrt_context_frame_t, pc) == VRT_CTX_PC,
    "pc offset mismatch");

_Static_assert(
    offsetof(vrt_context_frame_t, ps) == VRT_CTX_PS,
    "ps offset mismatch");

_Static_assert(
    offsetof(vrt_context_frame_t, a0) == VRT_CTX_A0,
    "a0 offset mismatch");

_Static_assert(
    offsetof(vrt_context_frame_t, a1) == VRT_CTX_A1,
    "a1 offset mismatch");

_Static_assert(
    offsetof(vrt_context_frame_t, a2) == VRT_CTX_A2,
    "a2 offset mismatch");

_Static_assert(
    offsetof(vrt_context_frame_t, a3) == VRT_CTX_A3,
    "a3 offset mismatch");

_Static_assert(
    offsetof(vrt_context_frame_t, a4) == VRT_CTX_A4,
    "a4 offset mismatch");

_Static_assert(
    offsetof(vrt_context_frame_t, a5) == VRT_CTX_A5,
    "a5 offset mismatch");

_Static_assert(
    offsetof(vrt_context_frame_t, a6) == VRT_CTX_A6,
    "a6 offset mismatch");

_Static_assert(
    offsetof(vrt_context_frame_t, a7) == VRT_CTX_A7,
    "a7 offset mismatch");

_Static_assert(
    offsetof(vrt_context_frame_t, a8) == VRT_CTX_A8,
    "a8 offset mismatch");

_Static_assert(
    offsetof(vrt_context_frame_t, a9) == VRT_CTX_A9,
    "a9 offset mismatch");

_Static_assert(
    offsetof(vrt_context_frame_t, a10) == VRT_CTX_A10,
    "a10 offset mismatch");

_Static_assert(
    offsetof(vrt_context_frame_t, a11) == VRT_CTX_A11,
    "a11 offset mismatch");

_Static_assert(
    offsetof(vrt_context_frame_t, a12) == VRT_CTX_A12,
    "a12 offset mismatch");

_Static_assert(
    offsetof(vrt_context_frame_t, a13) == VRT_CTX_A13,
    "a13 offset mismatch");

_Static_assert(
    offsetof(vrt_context_frame_t, a14) == VRT_CTX_A14,
    "a14 offset mismatch");

_Static_assert(
    offsetof(vrt_context_frame_t, a15) == VRT_CTX_A15,
    "a15 offset mismatch");

_Static_assert(
    offsetof(vrt_context_frame_t, sar) == VRT_CTX_SAR,
    "sar offset mismatch");

_Static_assert(
    offsetof(vrt_context_frame_t, exccause) == VRT_CTX_EXCCAUSE,
    "exccause offset mismatch");

_Static_assert(
    offsetof(vrt_context_frame_t, excvaddr) == VRT_CTX_EXCVADDR,
    "excvaddr offset mismatch");

_Static_assert(
    offsetof(vrt_context_frame_t, lbeg) == VRT_CTX_LBEG,
    "lbeg offset mismatch");

_Static_assert(
    offsetof(vrt_context_frame_t, lend) == VRT_CTX_LEND,
    "lend offset mismatch");

_Static_assert(
    offsetof(vrt_context_frame_t, lcount) == VRT_CTX_LCOUNT,
    "lcount offset mismatch");

_Static_assert(
    offsetof(vrt_context_frame_t, tmp0) == VRT_CTX_TMP0,
    "tmp0 offset mismatch");

_Static_assert(
    offsetof(vrt_context_frame_t, tmp1) == VRT_CTX_TMP1,
    "tmp1 offset mismatch");

_Static_assert(
    offsetof(vrt_context_frame_t, tmp2) == VRT_CTX_TMP2,
    "tmp2 offset mismatch");

/* --------------------------------------------------------------------------
 * Structure size verification
 * -------------------------------------------------------------------------- */

_Static_assert(
    sizeof(vrt_context_frame_t) == VRT_CTX_FRAME_SIZE,
    "context frame size mismatch");