/*
 *=========================================================
 * VertexRT
 * ESP32 Xtensa Port
 *========================================================*/

    .text
    .align 4

/*
 *---------------------------------------------------------
 * Start First Task
 *---------------------------------------------------------
*/

    .global vrt_port_start_first_task
    .type   vrt_port_start_first_task, @function

    .extern vrt_scheduler_get_instance

vrt_port_start_first_task:

    /* Get scheduler instance */
    call0 vrt_scheduler_get_instance

    /* A2 now contains the scheduler pointer */

1:
    j 1b

/*
 *---------------------------------------------------------
 * Context Switch
 *---------------------------------------------------------
*/

    .global vrt_port_switch_context
    .type   vrt_port_switch_context, @function

vrt_port_switch_context:

2:
    j 2b