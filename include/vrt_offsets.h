#ifndef VRT_OFFSETS_H
#define VRT_OFFSETS_H

/*
 * VertexRT
 * Architecture Structure Offsets
 *
 * This header is shared between C and Xtensa assembly.
 *
 * C code:
 *     Uses offsetof() so offsets remain synchronized
 *     automatically with the actual structures.
 *
 * Assembly:
 *     Uses the generated/equivalent numeric constants.
 */

#ifdef __ASSEMBLER__

/*=========================================================
 * Scheduler Structure Offsets
 *=========================================================*/

#define VRT_SCHEDULER_CURRENT_TASK_OFFSET 12

/*=========================================================
 * Task Structure Offsets
 *=========================================================*/

#define VRT_TASK_SP_OFFSET 48

/*=========================================================
 * Initial Task Frame Offsets
 *=========================================================*/

/*
 * Must match vrt_stack_frame_t in vrt_port.c.
 */

#define VRT_FRAME_A0 0
#define VRT_FRAME_A1 4
#define VRT_FRAME_A2 8
#define VRT_FRAME_A3 12
#define VRT_FRAME_A4 16
#define VRT_FRAME_A5 20
#define VRT_FRAME_A6 24
#define VRT_FRAME_A7 28
#define VRT_FRAME_A8 32
#define VRT_FRAME_A9 36
#define VRT_FRAME_A10 40
#define VRT_FRAME_A11 44
#define VRT_FRAME_A12 48
#define VRT_FRAME_A13 52
#define VRT_FRAME_A14 56
#define VRT_FRAME_A15 60

#define VRT_FRAME_PS 64
#define VRT_FRAME_SAR 68

#define VRT_FRAME_LBEG 72
#define VRT_FRAME_LEND 76
#define VRT_FRAME_LCOUNT 80

#define VRT_FRAME_PC 84

#define VRT_FRAME_SIZE 88

#else /* C compiler */

/*=========================================================
 * C Structure Offsets
 *=========================================================*/

#include <stddef.h>

#include "vrt_scheduler.h"
#include "vrt_task.h"

/*=========================================================
 * Scheduler Structure Offsets
 *=========================================================*/

#define VRT_SCHEDULER_CURRENT_TASK_OFFSET \
    offsetof(vrt_scheduler_t, currentTask)

/*=========================================================
 * Task Structure Offsets
 *=========================================================*/

#define VRT_TASK_SP_OFFSET \
    offsetof(vrt_task_t, sp)

#endif /* __ASSEMBLER__ */

#endif /* VRT_OFFSETS_H */