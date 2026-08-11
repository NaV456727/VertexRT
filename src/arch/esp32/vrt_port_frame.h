#ifndef VRT_PORT_FRAME_H
#define VRT_PORT_FRAME_H

#include <stdint.h>

/*
 * VertexRT
 * ESP32 Xtensa initial task context.
 *
 * This layout follows the important part of the Xtensa
 * FreeRTOS task stack frame:
 *
 *     +0   exit
 *     +4   pc
 *     +8   ps
 *     +12  a0
 *     +16  a1
 *     +20  a2
 *     +24  a3
 *     ...
 *     +72  a15
 *     +76  sar
 *     +80  exccause
 *     +84  excvaddr
 *     +88  lbeg
 *     +92  lend
 *     +96  lcount
 *     +100 tmp0
 *     +104 tmp1
 *     +108 tmp2
 *
 * Total: 112 bytes
 *
 * The temporary registers are required by the windowed
 * Xtensa context machinery.
 */

typedef struct vrt_stack_frame
{
    uint32_t exit;

    uint32_t pc;
    uint32_t ps;

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

    uint32_t sar;

    uint32_t exccause;
    uint32_t excvaddr;

    uint32_t lbeg;
    uint32_t lend;
    uint32_t lcount;

    /*
     * Window spill/fill temporary storage.
     */
    uint32_t tmp0;
    uint32_t tmp1;
    uint32_t tmp2;

} vrt_stack_frame_t;

#endif /* VRT_PORT_FRAME_H */