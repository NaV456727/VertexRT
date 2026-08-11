#ifndef VRT_OFFSETS_H
#define VRT_OFFSETS_H

/*
 * VertexRT
 * ESP32 Xtensa port offsets.
 *
 * These values correspond directly to the structures
 * used by the C port.
 */

#ifdef __ASSEMBLER__

/*=========================================================
 * Scheduler
 *=========================================================*/

/*
 * vrt_scheduler_t:
 *
 *     readyQueue
 *     currentTask
 *     idleTask
 *     tickCount
 *     taskCount
 *     running
 *
 * On the current 32-bit ESP32 layout:
 *
 *     readyQueue = 0
 *     currentTask = 12
 */
#define VRT_SCHEDULER_CURRENT_TASK_OFFSET 12

/*=========================================================
 * Task
 *=========================================================*/

/*
 * vrt_task_t:
 *
 * id                  0
 * name                4
 * entry              20
 * argument           24
 * priority           28
 * state              32
 * stackStart         36
 * stackEnd           40
 * stackSize          44
 * sp                 48
 */
#define VRT_TASK_SP_OFFSET 48

/*=========================================================
 * Xtensa initial stack frame
 *=========================================================*/

#define VRT_FRAME_EXIT 0
#define VRT_FRAME_PC 4
#define VRT_FRAME_PS 8

#define VRT_FRAME_A0 12
#define VRT_FRAME_A1 16
#define VRT_FRAME_A2 20
#define VRT_FRAME_A3 24
#define VRT_FRAME_A4 28
#define VRT_FRAME_A5 32
#define VRT_FRAME_A6 36
#define VRT_FRAME_A7 40
#define VRT_FRAME_A8 44
#define VRT_FRAME_A9 48
#define VRT_FRAME_A10 52
#define VRT_FRAME_A11 56
#define VRT_FRAME_A12 60
#define VRT_FRAME_A13 64
#define VRT_FRAME_A14 68
#define VRT_FRAME_A15 72

#define VRT_FRAME_SAR 76

#define VRT_FRAME_EXCCAUSE 80
#define VRT_FRAME_EXCVADDR 84

#define VRT_FRAME_LBEG 88
#define VRT_FRAME_LEND 92
#define VRT_FRAME_LCOUNT 96

#define VRT_FRAME_TMP0 100
#define VRT_FRAME_TMP1 104
#define VRT_FRAME_TMP2 108

#define VRT_FRAME_SIZE 112

#endif /* __ASSEMBLER__ */

#endif /* VRT_OFFSETS_H */