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
 * Stack Frame Offsets
 *=========================================================*/

#define VRT_FRAME_PC 0
#define VRT_FRAME_PS 4

#define VRT_FRAME_A0 8
#define VRT_FRAME_A1 12
#define VRT_FRAME_A2 16
#define VRT_FRAME_A3 20
#define VRT_FRAME_A4 24
#define VRT_FRAME_A5 28
#define VRT_FRAME_A6 32
#define VRT_FRAME_A7 36
#define VRT_FRAME_A8 40
#define VRT_FRAME_A9 44
#define VRT_FRAME_A10 48
#define VRT_FRAME_A11 52
#define VRT_FRAME_A12 56
#define VRT_FRAME_A13 60
#define VRT_FRAME_A14 64
#define VRT_FRAME_A15 68

#define VRT_FRAME_SAR 72

#define VRT_FRAME_EXCCAUSE 76
#define VRT_FRAME_EXCVADDR 80

#define VRT_FRAME_LBEG 84
#define VRT_FRAME_LEND 88
#define VRT_FRAME_LCOUNT 92

#define VRT_FRAME_EXIT 96

#define VRT_FRAME_SIZE 100

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