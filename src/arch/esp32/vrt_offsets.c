#include <stddef.h>

#include "vrt_task.h"
#include "vrt_scheduler.h"
#include "vrt_port_frame.h"

/*
 * VertexRT
 * Architecture Structure Offsets
 *
 * These values are derived directly from the actual C structures.
 *
 * The build system can use these symbols to generate the
 * assembly-visible offset definitions.
 */

/*=========================================================
 * Scheduler
 *=========================================================*/

const unsigned int vrt_generated_scheduler_current_task_offset =
    offsetof(vrt_scheduler_t, currentTask);

/*=========================================================
 * Task
 *=========================================================*/

const unsigned int vrt_generated_task_sp_offset =
    offsetof(vrt_task_t, sp);

/*=========================================================
 * Initial Context Frame
 *=========================================================*/

const unsigned int vrt_generated_frame_a0 =
    offsetof(vrt_stack_frame_t, a0);

const unsigned int vrt_generated_frame_a1 =
    offsetof(vrt_stack_frame_t, a1);

const unsigned int vrt_generated_frame_a2 =
    offsetof(vrt_stack_frame_t, a2);

const unsigned int vrt_generated_frame_a3 =
    offsetof(vrt_stack_frame_t, a3);

const unsigned int vrt_generated_frame_a4 =
    offsetof(vrt_stack_frame_t, a4);

const unsigned int vrt_generated_frame_a5 =
    offsetof(vrt_stack_frame_t, a5);

const unsigned int vrt_generated_frame_a6 =
    offsetof(vrt_stack_frame_t, a6);

const unsigned int vrt_generated_frame_a7 =
    offsetof(vrt_stack_frame_t, a7);

const unsigned int vrt_generated_frame_a8 =
    offsetof(vrt_stack_frame_t, a8);

const unsigned int vrt_generated_frame_a9 =
    offsetof(vrt_stack_frame_t, a9);

const unsigned int vrt_generated_frame_a10 =
    offsetof(vrt_stack_frame_t, a10);

const unsigned int vrt_generated_frame_a11 =
    offsetof(vrt_stack_frame_t, a11);

const unsigned int vrt_generated_frame_a12 =
    offsetof(vrt_stack_frame_t, a12);

const unsigned int vrt_generated_frame_a13 =
    offsetof(vrt_stack_frame_t, a13);

const unsigned int vrt_generated_frame_a14 =
    offsetof(vrt_stack_frame_t, a14);

const unsigned int vrt_generated_frame_a15 =
    offsetof(vrt_stack_frame_t, a15);

/*=========================================================
 * Special Registers
 *=========================================================*/

const unsigned int vrt_generated_frame_ps =
    offsetof(vrt_stack_frame_t, ps);

const unsigned int vrt_generated_frame_sar =
    offsetof(vrt_stack_frame_t, sar);

const unsigned int vrt_generated_frame_lbeg =
    offsetof(vrt_stack_frame_t, lbeg);

const unsigned int vrt_generated_frame_lend =
    offsetof(vrt_stack_frame_t, lend);

const unsigned int vrt_generated_frame_lcount =
    offsetof(vrt_stack_frame_t, lcount);

/*=========================================================
 * Program Counter
 *=========================================================*/

const unsigned int vrt_generated_frame_pc =
    offsetof(vrt_stack_frame_t, pc);

/*=========================================================
 * Frame Size
 *=========================================================*/

const unsigned int vrt_generated_frame_size =
    sizeof(vrt_stack_frame_t);