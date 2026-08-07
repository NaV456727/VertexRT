#ifndef VRT_OFFSETS_H
#define VRT_OFFSETS_H

#include <stddef.h>

#include "vrt_scheduler.h"
#include "vrt_task.h"

/*=========================================================
 * Scheduler Structure Offsets
 *========================================================*/

#define VRT_SCHEDULER_CURRENT_TASK_OFFSET \
    offsetof(vrt_scheduler_t, currentTask)

/*=========================================================
 * Task Structure Offsets
 *========================================================*/

#define VRT_TASK_SP_OFFSET \
    offsetof(vrt_task_t, sp)

#endif /* VRT_OFFSETS_H */