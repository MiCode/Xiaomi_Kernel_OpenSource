/*COPYRIGHT**
    Copyright (C) 2005-2014 Intel Corporation.  All Rights Reserved.
 
    This file is part of SEP Development Kit
 
    SEP Development Kit is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.
 
    SEP Development Kit is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with SEP Development Kit; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 
    As a special exception, you may use this file as part of a free software
    library without restriction.  Specifically, if other files instantiate
    templates or use macros or inline functions from this file, or you compile
    this file and link it with other files to produce an executable, this
    file does not by itself cause the resulting executable to be covered by
    the GNU General Public License.  This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
**COPYRIGHT*/


#include "lwpmudrv_defines.h"
#include <linux/version.h>
#include <linux/jiffies.h>
#include <linux/time.h>
#include <linux/percpu.h>
#include "lwpmudrv_types.h"
#include "rise_errors.h"
#include "lwpmudrv_ecb.h"
#include "lwpmudrv_struct.h"
#include "lwpmudrv.h"
#include "control.h"

static PVOID     em_tables      = NULL;
static size_t    em_tables_size = 0;

/* ------------------------------------------------------------------------- */
/*!
 * @fn          VOID eventmux_Allocate_Groups (
 *                         VOID  *params
 *                         )
 *
 * @brief       Allocate memory need to support event multiplexing
 *
 * @param       params - pointer to a S32 that holds the size of buffer to allocate
 *
 * @return      NONE
 *
 * <I>Special Notes:</I>
 *              Allocate the memory needed to save different group counters
 *              Called via the parallel control mechanism
 */
static VOID 
eventmux_Allocate_Groups (
    PVOID  params
)
{
    U32        this_cpu;
    S32        alloc_size = *(S32 *)params;
    CPU_STATE  cpu_state;

    preempt_disable();
    this_cpu   = CONTROL_THIS_CPU();
    cpu_state  = &pcb[this_cpu];
    preempt_enable();

    CPU_STATE_em_tables(cpu_state) = em_tables + (alloc_size * this_cpu);
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn          VOID eventmux_Deallocate_Groups (
 *                         VOID  *params
 *                         )
 *
 * @brief       Free the scratch memory need to support event multiplexing
 *
 * @param       params - pointer to NULL
 *
 * @return      NONE
 *
 * <I>Special Notes:</I>
 *              Free the memory needed to save different group counters
 *              Called via the parallel control mechanism
 */
static VOID 
eventmux_Deallocate_Groups (
    PVOID  params
)
{
    U32        this_cpu;
    CPU_STATE  cpu_state;

    preempt_disable();
    this_cpu  = CONTROL_THIS_CPU();
    cpu_state = &pcb[this_cpu];
    preempt_enable();

    CPU_STATE_em_tables(cpu_state) = NULL;
    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn          VOID eventmux_Timer_Callback_Thread (
 *                         )
 *
 * @brief       Stop all the timer threads and terminate them
 *
 * @param       none
 *
 * @return      NONE
 *
 * <I>Special Notes:</I>
 *              timer routine - The event multiplexing happens here.
 */
static VOID
eventmux_Timer_Callback_Thread (
    unsigned long arg
)
{
    U32        this_cpu;
    CPU_STATE  pcpu;

    preempt_disable();
    this_cpu = CONTROL_THIS_CPU();
    pcpu     = &pcb[this_cpu];
    preempt_enable();


    if (CPU_STATE_em_tables(pcpu) == NULL) {
        return;
    }

    dispatch->swap_group(TRUE);
    CPU_STATE_em_timer(pcpu)->expires  = jiffies+arg;
    add_timer(CPU_STATE_em_timer(pcpu));
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn          VOID eventmux_Prepare_Timer_Threads (
 *                         VOID
 *                         )
 *
 * @brief       Stop all the timer threads and terminate them
 *
 * @param       NONE
 *
 * @return      NONE
 *
 * <I>Special Notes:</I>
 *              Set up the timer threads to prepare for event multiplexing.
 *              Do not start the threads as yet
 */
static VOID
eventmux_Prepare_Timer_Threads (
                                PVOID arg
)
{
    CPU_STATE   pcpu;

    // initialize and set up the timer for all cpus
    // Do not start the timer as yet.
    preempt_disable();
    pcpu = &pcb[CONTROL_THIS_CPU()];
    preempt_enable();
    CPU_STATE_em_timer(pcpu) = (struct timer_list*)CONTROL_Allocate_Memory(sizeof(struct timer_list));

    if (CPU_STATE_em_timer(pcpu) == NULL) {
       SEP_PRINT_ERROR("eventmux_Prepare_Timer_Threads skipped pcpu=NULL\n")
       return;
    }

    init_timer(CPU_STATE_em_timer(pcpu));
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn          VOID eventmux_Cancel_Timers (
 *                         VOID
 *                         )
 *
 * @brief       Stop all the timer threads and terminate them
 *
 * @param       NONE
 *
 * @return      NONE
 *
 * <I>Special Notes:</I>
 *              Cancel all the timer threads that have been started
 */
static VOID
eventmux_Cancel_Timers (
    VOID
)
{
    CPU_STATE   pcpu;
    S32         i;

    /*
     *  Cancel the timer for all active CPUs
     */
    for (i=0; i < GLOBAL_STATE_active_cpus(driver_state); i++) {
        pcpu = &pcb[i];
        del_timer_sync(CPU_STATE_em_timer(pcpu));
    }
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn          VOID eventmux_Start_Timers (
 *                         long unsigned arg
 *                         )
 *
 * @brief       Start the timer on a single cpu
 *
 * @param       delay   interval time in jiffies
 *
 * @return      NONE
 *
 * <I>Special Notes:</I>
 *              start the timer on a single cpu
 *              Call from each cpu to get cpu affinity for Timer_Callback_Thread
 */
static VOID
eventmux_Start_Timers(
                      PVOID arg
                      )
{
    CPU_STATE     pcpu;
    preempt_disable();
    pcpu = &pcb[CONTROL_THIS_CPU()];
    preempt_enable();
    CPU_STATE_em_timer(pcpu)->function = eventmux_Timer_Callback_Thread;
    CPU_STATE_em_timer(pcpu)->data     = (unsigned long)arg;
    CPU_STATE_em_timer(pcpu)->expires  = jiffies+(unsigned long)arg;
    add_timer(CPU_STATE_em_timer(pcpu));
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn          VOID EVENTMUX_Start (
 *                         EVENT_CONFIG ec
 *                         )
 *
 * @brief       Start the timers and enable all the threads
 *
 * @param       ec    - Event Configuration
 *
 * @return      NONE
 *
 * <I>Special Notes:</I>
 *              if event multiplexing has been enabled, set up the time slices and
 *              start the timer threads for all the timers
 */
extern VOID 
EVENTMUX_Start (
    EVENT_CONFIG ec
)
{
    unsigned long delay;

    if (EVENT_CONFIG_mode(ec) != EM_TIMER_BASED ||
        EVENT_CONFIG_num_groups(ec) == 1) {
        return;
    }
    /*
     * notice we want to use group 0's time slice for the initial timer
     */
    delay = msecs_to_jiffies(EVENT_CONFIG_em_factor(ec));
    SEP_PRINT_DEBUG("EVENTMUX_Start: delay is %lu jiffies\n", delay);
    /*
     * Start the timer for all cpus
     */
    CONTROL_Invoke_Parallel(eventmux_Start_Timers, (PVOID)delay);
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn          VOID EVENTMUX_Initialize (
 *                         EVENT_CONFIG ec
 *                         )
 *
 * @brief       Initialize the event multiplexing module
 *
 * @param       ec    - Event Configuration
 *
 * @return      NONE
 *
 * <I>Special Notes:</I>
 *              if event multiplexing has been enabled, 
 *              then allocate the memory needed to save and restore all the counter data
 *              set up the timers needed, but do not start them
 */
extern VOID
EVENTMUX_Initialize (
    EVENT_CONFIG ec
)
{
    S32   size_of_vector;
    if (EVENT_CONFIG_mode(ec)       == EM_DISABLED ||
        EVENT_CONFIG_num_groups(ec) == 1) {
        return;
    }

    size_of_vector = EVENT_CONFIG_num_groups(ec)    *
                     EVENT_CONFIG_max_gp_events(ec) *
                     sizeof(S64);

    em_tables_size = GLOBAL_STATE_num_cpus(driver_state) * size_of_vector;
    em_tables = CONTROL_Allocate_Memory(em_tables_size);
    CONTROL_Invoke_Parallel(eventmux_Allocate_Groups,
                            (VOID *)&(size_of_vector));
    
    if (EVENT_CONFIG_mode(ec) == EM_TIMER_BASED) {
        CONTROL_Invoke_Parallel(eventmux_Prepare_Timer_Threads, NULL);
    }
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn          VOID EVENTMUX_Destroy (
 *                         EVENT_CONFIG ec
 *                         )
 *
 * @brief       Clean up the event multiplexing threads
 *
 * @param       ec    - Event Configuration
 *
 * @return      NONE
 *
 * <I>Special Notes:</I>
 *              if event multiplexing has been enabled, then stop and cancel all the timers
 *              free up all the memory that is associated with EM
 */
extern VOID
EVENTMUX_Destroy (
    EVENT_CONFIG ec
)
{
    if (ec == NULL) {
        return;
    }
    if (EVENT_CONFIG_mode(ec)       == EM_DISABLED ||
        EVENT_CONFIG_num_groups(ec) == 1) {
        return;
    }
    if (EVENT_CONFIG_mode(ec) == EM_TIMER_BASED) {
        eventmux_Cancel_Timers();
    }

    em_tables      = CONTROL_Free_Memory(em_tables);
    em_tables_size = 0;
    CONTROL_Invoke_Parallel(eventmux_Deallocate_Groups, (VOID *)(size_t)0);
}
