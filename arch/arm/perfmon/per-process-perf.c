/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/

/*
per-process_perf
DESCRIPTION
Capture the processor performances registers when the process context
switches.  The /proc file system is used to control and access the results
of the performance counters.

Each time a process is context switched, the performance counters for
the Snoop Control Unit and the standard ARM counters are set according
to the values stored for that process.

The events to capture per process are set in the /proc/ppPerf/settings
directory.

EXTERNALIZED FUNCTIONS

INITIALIZATION AND SEQUENCING REQUIREMENTS
Detail how to initialize and use this service.  The sequencing aspect
is only needed if the order of operations is important.
*/

/*
INCLUDE FILES FOR MODULE
*/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/sysrq.h>
#include <linux/time.h>
#include "linux/proc_fs.h"
#include "linux/kernel_stat.h"
#include <asm/thread_notify.h>
#include "asm/uaccess.h"
#include "cp15_registers.h"
#include "l2_cp15_registers.h"
#include <asm/perftypes.h>
#include "per-axi.h"
#include "perf.h"

#define DEBUG_SWAPIO
#ifdef DEBUG_SWAPIO
#define MR_SIZE 1024
#define PM_PP_ERR -1
struct mark_data_s {
  long c;
  long cpu;
  unsigned long pid_old;
  unsigned long pid_new;
};

struct mark_data_s markRay[MR_SIZE] __attribute__((aligned(16)));
int mrcnt;

DEFINE_SPINLOCK(_mark_lock);

static inline void MARKPIDS(char a, int opid, int npid)
{
	int cpu = smp_processor_id();

	if (opid == 0)
		return;
	spin_lock(&_mark_lock);
	if (++mrcnt >= MR_SIZE)
		mrcnt = 0;
	spin_unlock(&_mark_lock);

	markRay[mrcnt].pid_old = opid;
	markRay[mrcnt].pid_new = npid;
	markRay[mrcnt].cpu = cpu;
	markRay[mrcnt].c = a;
}
static inline void MARK(char a) { MARKPIDS(a, 0xFFFF, 0xFFFF); }
static inline void MARKPID(char a, int pid) { MARKPIDS(a, pid, 0xFFFF); }

#else
#define MARK(a)
#define MARKPID(a, b)
#define MARKPIDS(a, b, c)

#endif /* DEBUG_SWAPIO */

/*
DEFINITIONS AND DECLARATIONS FOR MODULE

This section contains definitions for constants, macros, types, variables
and other items needed by this module.
*/

/*
Constant / Define Declarations
*/

#define PERF_MON_PROCESS_NUM 0x400
#define PERF_MON_PROCESS_MASK (PERF_MON_PROCESS_NUM-1)
#define PP_MAX_PROC_ENTRIES 32

/*
 * The entry is locked and is not to be replaced.
 */
#define PERF_ENTRY_LOCKED (1<<0)
#define PERF_NOT_FIRST_TIME   (1<<1)
#define PERF_EXITED (1<<2)
#define PERF_AUTOLOCK (1<<3)

#define IS_LOCKED(p) (p->flags & PERF_ENTRY_LOCKED)

#define PERF_NUM_MONITORS 4

#define L1_EVENTS_0    0
#define L1_EVENTS_1    1
#define L2_EVENTS_0  2
#define L2_EVENTS_1  3

#define PM_CYCLE_OVERFLOW_MASK 0x80000000
#define L2_PM_CYCLE_OVERFLOW_MASK 0x80000000

#define PM_START_ALL() do {\
	if (pm_global) \
		pmStartAll();\
	} while (0);
#define PM_STOP_ALL() do {\
	if (pm_global)\
		pmStopAll();\
	} while (0);
#define PM_RESET_ALL() do {\
	if (pm_global)\
		pmResetAll();\
	} while (0);

/*
 * Accessors for SMP based variables.
 */
#define _SWAPS(p) ((p)->cnts[smp_processor_id()].swaps)
#define _CYCLES(p) ((p)->cnts[smp_processor_id()].cycles)
#define _COUNTS(p, i) ((p)->cnts[smp_processor_id()].counts[i])
#define _L2COUNTS(p, i) ((p)->cnts[smp_processor_id()].l2_counts[i])
#define _L2CYCLES(p) ((p)->cnts[smp_processor_id()].l2_cycles)

/*
  Type Declarations
*/

/*
 * Counts are on a per core basis.
 */
struct pm_counters_s {
  unsigned long long  cycles;
  unsigned long long  l2_cycles;
  unsigned long long  counts[PERF_NUM_MONITORS];
  unsigned long long  l2_counts[PERF_NUM_MONITORS];
  unsigned long  swaps;
};

struct per_process_perf_mon_type{
  struct pm_counters_s cnts[NR_CPUS];
  unsigned long  control;
  unsigned long  index[PERF_NUM_MONITORS];
  unsigned long  l2_index[PERF_NUM_MONITORS];
  unsigned long  pid;
  struct proc_dir_entry *proc;
  struct proc_dir_entry *l2_proc;
  unsigned short flags;
  unsigned short running_cpu;
  char           *pidName;
  unsigned long lpm0evtyper;
  unsigned long lpm1evtyper;
  unsigned long lpm2evtyper;
  unsigned long l2lpmevtyper;
  unsigned long vlpmevtyper;
  unsigned long l2pmevtyper0;
  unsigned long l2pmevtyper1;
  unsigned long l2pmevtyper2;
  unsigned long l2pmevtyper3;
  unsigned long l2pmevtyper4;
};

unsigned long last_in_pid[NR_CPUS];
unsigned long fake_swap_out[NR_CPUS] = {0};

/*
  Local Object Definitions
*/
struct per_process_perf_mon_type perf_mons[PERF_MON_PROCESS_NUM];
struct proc_dir_entry *proc_dir;
struct proc_dir_entry *settings_dir;
struct proc_dir_entry *values_dir;
struct proc_dir_entry *axi_dir;
struct proc_dir_entry *l2_dir;
struct proc_dir_entry *axi_settings_dir;
struct proc_dir_entry *axi_results_dir;
struct proc_dir_entry *l2_results_dir;

unsigned long pp_enabled;
unsigned long pp_settings_valid = -1;
unsigned long pp_auto_lock;
unsigned long pp_set_pid;
signed long pp_clear_pid = -1;
unsigned long per_proc_event[PERF_NUM_MONITORS];
unsigned long l2_per_proc_event[PERF_NUM_MONITORS];
unsigned long dbg_flags;
unsigned long pp_lpm0evtyper;
unsigned long pp_lpm1evtyper;
unsigned long pp_lpm2evtyper;
unsigned long pp_l2lpmevtyper;
unsigned long pp_vlpmevtyper;
unsigned long pm_stop_for_interrupts;
unsigned long pm_global;   /* track all, not process based */
unsigned long pm_global_enable;
unsigned long pm_remove_pid;

unsigned long pp_l2pmevtyper0;
unsigned long pp_l2pmevtyper1;
unsigned long pp_l2pmevtyper2;
unsigned long pp_l2pmevtyper3;
unsigned long pp_l2pmevtyper4;

unsigned long pp_proc_entry_index;
char *per_process_proc_names[PP_MAX_PROC_ENTRIES];

unsigned int axi_swaps;
#define MAX_AXI_SWAPS	10
int first_switch = 1;
/*
  Forward Declarations
*/

/*
Function Definitions
*/

/*
FUNCTION  per_process_find

DESCRIPTION
  Find the per process information based on the process id (pid) passed.
  This is a simple mask based on the number of entries stored in the
  static array

DEPENDENCIES

RETURN VALUE
  Pointer to the per process data
SIDE EFFECTS

*/
struct per_process_perf_mon_type *per_process_find(unsigned long pid)
{
  return &perf_mons[pid & PERF_MON_PROCESS_MASK];
}

/*
FUNCTION  per_process_get_name

DESCRIPTION
  Retreive the name of the performance counter based on the table and
  index passed.  We have two different sets of performance counters so
  different table need to be used.

DEPENDENCIES

RETURN VALUE
  Pointer to char string with the name of the event or "BAD"
  Never returns NULL or a bad pointer.

SIDE EFFECTS
*/
char *per_process_get_name(unsigned long index)
{
  return pm_find_event_name(index);
}

/*
FUNCTION per_process_results_read

DESCRIPTION
  Print out the formatted results from the process id read.  Event names
  and counts are printed.

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
int per_process_results_read(char *page, char **start, off_t off, int count,
   int *eof, void *data)
{
	struct per_process_perf_mon_type *p =
		(struct per_process_perf_mon_type *)data;
	struct pm_counters_s cnts;
	int i, j;

	/*
	* Total across all CPUS
	*/
	memset(&cnts, 0, sizeof(cnts));
	for (i = 0; i < num_possible_cpus(); i++) {
		cnts.swaps += p->cnts[i].swaps;
		cnts.cycles += p->cnts[i].cycles;
		for (j = 0; j < PERF_NUM_MONITORS; j++)
			cnts.counts[j] += p->cnts[i].counts[j];
	}

	/*
	* Display as single results of the totals calculated above.
	* Do we want to display or have option to display individula cores?
	*/
	return sprintf(page, "pid:%lu one:%s:%llu two:%s:%llu three:%s:%llu \
	four:%s:%llu cycles:%llu swaps:%lu\n",
	p->pid,
	per_process_get_name(p->index[0]), cnts.counts[0],
	per_process_get_name(p->index[1]), cnts.counts[1],
	per_process_get_name(p->index[2]), cnts.counts[2],
	per_process_get_name(p->index[3]), cnts.counts[3],
	cnts.cycles, cnts.swaps);
}

int per_process_l2_results_read(char *page, char **start, off_t off, int count,
   int *eof, void *data)
{
  struct per_process_perf_mon_type *p =
	(struct per_process_perf_mon_type *)data;
	struct pm_counters_s cnts;
	int i, j;

	/*
	* Total across all CPUS
	*/
	memset(&cnts, 0, sizeof(cnts));
	for (i = 0; i < num_possible_cpus(); i++) {
		cnts.l2_cycles += p->cnts[i].l2_cycles;
		for (j = 0; j < PERF_NUM_MONITORS; j++)
			cnts.l2_counts[j] += p->cnts[i].l2_counts[j];
	}

  /*
   * Display as single results of the totals calculated above.
   * Do we want to display or have option to display individula cores?
   */
   return sprintf(page, "pid:%lu l2_one:%s:%llu l2_two:%s:%llu \
	l2_three:%s:%llu \
	l2_four:%s:%llu l2_cycles:%llu\n",
     p->pid,
     per_process_get_name(p->l2_index[0]), cnts.l2_counts[0],
     per_process_get_name(p->l2_index[1]), cnts.l2_counts[1],
     per_process_get_name(p->l2_index[2]), cnts.l2_counts[2],
     per_process_get_name(p->l2_index[3]), cnts.l2_counts[3],
     cnts.l2_cycles);
}

/*
FUNCTION  per_process_results_write

DESCRIPTION
  Allow some control over the results.  If the user forgets to autolock or
  wants to unlock the results so they will be deleted, then this is
  where it is processed.

  For example, to unlock process 23
  echo "unlock" > 23

DEPENDENCIES

RETURN VALUE
  Number of characters used (all of them!)

SIDE EFFECTS
*/
int per_process_results_write(struct file *file, const char *buff,
    unsigned long cnt, void *data)
{
  char *newbuf;
  struct per_process_perf_mon_type *p =
	(struct per_process_perf_mon_type *)data;

	if (p == 0)
		return cnt;
	/*
	* Alloc the user data in kernel space. and then copy user to kernel
	*/
	newbuf = kmalloc(cnt + 1, GFP_KERNEL);
	if (0 == newbuf)
		return cnt;
	if (copy_from_user(newbuf, buff, cnt) != 0) {
		printk(KERN_INFO "%s copy_from_user failed\n", __func__);
		return cnt;
	}

	if (0 == strcmp("lock", newbuf))
		p->flags |= PERF_ENTRY_LOCKED;
	else if (0 == strcmp("unlock", newbuf))
		p->flags &= ~PERF_ENTRY_LOCKED;
	else if (0 == strcmp("auto", newbuf))
		p->flags |= PERF_AUTOLOCK;
	else if (0 == strcmp("autoun", newbuf))
		p->flags &= ~PERF_AUTOLOCK;

	return cnt;
}

/*
FUNCTION  perProcessCreateResults

DESCRIPTION
  Create the results /proc file if the system parameters allow it...
DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
void per_process_create_results_proc(struct per_process_perf_mon_type *p)
{

	if (0 == p->pidName)
		p->pidName = kmalloc(12, GFP_KERNEL);
	if (0 == p->pidName)
		return;
	sprintf(p->pidName, "%ld", p->pid);

	if (0 == p->proc) {
		p->proc = create_proc_entry(p->pidName, 0777, values_dir);
		if (0 == p->proc)
			return;
	} else {
		p->proc->name = p->pidName;
	}

	p->proc->read_proc = per_process_results_read;
	p->proc->write_proc = per_process_results_write;
	p->proc->data = (void *)p;
}

void per_process_create_l2_results_proc(struct per_process_perf_mon_type *p)
{

	if (0 == p->pidName)
		p->pidName = kmalloc(12, GFP_KERNEL);
	if (0 == p->pidName)
		return;
	sprintf(p->pidName, "%ld", p->pid);

	if (0 == p->l2_proc) {
		p->l2_proc = create_proc_entry(p->pidName, 0777,
			l2_results_dir);
		if (0 == p->l2_proc)
			return;
	} else {
		p->l2_proc->name = p->pidName;
	}

	p->l2_proc->read_proc = per_process_l2_results_read;
	p->l2_proc->write_proc = per_process_results_write;
	p->l2_proc->data = (void *)p;
}
/*
FUNCTION per_process_swap_out

DESCRIPTION
  Store the counters from the process that is about to swap out.  We take
  the old counts and add them to the current counts in the perf registers.
  Before the new process is swapped in, the counters are reset.

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
typedef void (*vfun)(void *);
void per_process_swap_out(struct per_process_perf_mon_type *data)
{
	int i;
	unsigned long overflow;
#ifdef CONFIG_ARCH_MSM8X60
	unsigned long l2_overflow;
#endif
	struct per_process_perf_mon_type *p = data;

	MARKPIDS('O', p->pid, 0);
	RCP15_PMOVSR(overflow);
#ifdef CONFIG_ARCH_MSM8X60
	RCP15_L2PMOVSR(l2_overflow);
#endif

	if (!pp_enabled)
		return;

	/*
	* The kernel for some reason (2.6.32.9) starts a process context on
	* one core and ends on another.  So the swap in and swap out can be
	* on different cores.  If this happens, we need to stop the
	* counters and collect the data on the core that started the counters
	* ....otherwise we receive invalid data.  So we mark the the core with
	* the process as deferred.  The next time a process is swapped on
	* the core that the process was running on, the counters will be
	* updated.
	*/
	if ((smp_processor_id() != p->running_cpu) && (p->pid != 0)) {
		fake_swap_out[p->running_cpu] = 1;
		return;
	}

	_SWAPS(p)++;
	_CYCLES(p) += pm_get_cycle_count();

	if (overflow & PM_CYCLE_OVERFLOW_MASK)
		_CYCLES(p) += 0xFFFFFFFF;

	for (i = 0; i < PERF_NUM_MONITORS; i++) {
		_COUNTS(p, i) += pm_get_count(i);
		if (overflow & (1 << i))
			_COUNTS(p, i) += 0xFFFFFFFF;
	}

#ifdef CONFIG_ARCH_MSM8X60
	_L2CYCLES(p) += l2_pm_get_cycle_count();
	if (l2_overflow & L2_PM_CYCLE_OVERFLOW_MASK)
		_L2CYCLES(p) += 0xFFFFFFFF;
	for (i = 0; i < PERF_NUM_MONITORS; i++) {
		_L2COUNTS(p, i) += l2_pm_get_count(i);
		if (l2_overflow & (1 << i))
			_L2COUNTS(p, i) += 0xFFFFFFFF;
	}
#endif
}

/*
FUNCTION per_process_remove_manual

DESCRIPTION
  Remove an entry from the results directory if the flags allow this.
  When not enbled or the entry is locked, the values/results will
  not be removed.

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
void per_process_remove_manual(unsigned long pid)
{
	struct per_process_perf_mon_type *p = per_process_find(pid);

	/*
	* Check all of the flags to see if we can remove this one
	* Then mark as not used
	*/
	if (0 == p)
		return;
	p->pid = (0xFFFFFFFF);

	/*
	* Remove the proc entry.
	*/
	if (p->proc)
		remove_proc_entry(p->pidName, values_dir);
	if (p->l2_proc)
		remove_proc_entry(p->pidName, l2_results_dir);
	kfree(p->pidName);

	/*
	* Clear them out...and ensure the pid is invalid
	*/
	memset(p, 0, sizeof *p);
	p->pid = 0xFFFFFFFF;
	pm_remove_pid = -1;
}

/*
* Remove called when a process exits...
*/
void _per_process_remove(unsigned long pid) {}

/*
FUNCTION  per_process_initialize

DESCRIPTION
Initialize performance collection information for a new process.

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
May create a new proc entry
*/
void per_process_initialize(struct per_process_perf_mon_type *p,
				unsigned long pid)
{
	int i;

	/*
	* See if this is the pid we are interested in...
	*/
	if (pp_settings_valid == -1)
		return;
	if ((pp_set_pid != pid) && (pp_set_pid != 0))
		return;

	/*
	* Clear out the statistics table then insert this pid
	* We want to keep the proc entry and the name
	*/
	p->pid = pid;

	/*
	* Create a proc entry for this pid, then get the current event types and
	* store in data struct so when the process is switched in we can track
	* it.
	*/
	if (p->proc == 0) {
		per_process_create_results_proc(p);
#ifdef CONFIG_ARCH_MSM8X60
		per_process_create_l2_results_proc(p);
#endif
	}
	_CYCLES(p) = 0;
	_L2CYCLES(p) = 0;
	_SWAPS(p) = 0;
	/*
	* Set the per process data struct, but not the monitors until later...
	* Init only happens with the user sets the SetPID variable to this pid
	* so we can load new values.
	*/
	for (i = 0; i < PERF_NUM_MONITORS; i++) {
		p->index[i] = per_proc_event[i];
#ifdef CONFIG_ARCH_MSM8X60
		p->l2_index[i] = l2_per_proc_event[i];
#endif
		_COUNTS(p, i) = 0;
		_L2COUNTS(p, i) = 0;
	}
	p->lpm0evtyper  = pp_lpm0evtyper;
	p->lpm1evtyper  = pp_lpm1evtyper;
	p->lpm2evtyper  = pp_lpm2evtyper;
	p->l2lpmevtyper = pp_l2lpmevtyper;
	p->vlpmevtyper  = pp_vlpmevtyper;

#ifdef CONFIG_ARCH_MSM8X60
	p->l2pmevtyper0  = pp_l2pmevtyper0;
	p->l2pmevtyper1  = pp_l2pmevtyper1;
	p->l2pmevtyper2  = pp_l2pmevtyper2;
	p->l2pmevtyper3  = pp_l2pmevtyper3;
	p->l2pmevtyper4  = pp_l2pmevtyper4;
#endif

	/*
	* Reset pid and settings value
	*/
	pp_set_pid = -1;
	pp_settings_valid = -1;
}

/*
FUNCTION  per_process_swap_in

DESCRIPTION
  Called when a context switch is about to start this PID.
  We check to see if this process has an entry or not and create one
  if not locked...

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
void per_process_swap_in(struct per_process_perf_mon_type *p_new,
				unsigned long pid)
{
	int i;

	MARKPIDS('I', p_new->pid, 0);
	/*
	* If the set proc variable == the current pid then init a new
	* entry...
	*/
	if (pp_set_pid == pid)
		per_process_initialize(p_new, pid);

	p_new->running_cpu = smp_processor_id();
	last_in_pid[smp_processor_id()] = pid;

	/*
	* setup the monitors for this process.
	*/
	for (i = 0; i < PERF_NUM_MONITORS; i++) {
		pm_set_event(i, p_new->index[i]);
#ifdef CONFIG_ARCH_MSM8X60
		l2_pm_set_event(i, p_new->l2_index[i]);
#endif
	}
	pm_set_local_iu(p_new->lpm0evtyper);
	pm_set_local_xu(p_new->lpm1evtyper);
	pm_set_local_su(p_new->lpm2evtyper);
	pm_set_local_l2(p_new->l2lpmevtyper);

#ifdef CONFIG_ARCH_MSM8X60
	pm_set_local_bu(p_new->l2pmevtyper0);
	pm_set_local_cb(p_new->l2pmevtyper1);
	pm_set_local_mp(p_new->l2pmevtyper2);
	pm_set_local_sp(p_new->l2pmevtyper3);
	pm_set_local_scu(p_new->l2pmevtyper4);
#endif
}

/*
FUNCTION  perProcessSwitch

DESCRIPTION
  Called during context switch.  Updates the counts on the process about to
  be swapped out and brings in the counters for the process about to be
  swapped in.

  All is dependant on the enabled and lock flags.

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/

DEFINE_SPINLOCK(pm_lock);
void _per_process_switch(unsigned long old_pid, unsigned long new_pid)
{
  struct per_process_perf_mon_type *p_old, *p_new;

	if (pm_global_enable == 0)
		return;

	spin_lock(&pm_lock);

	pm_stop_all();
#ifdef CONFIG_ARCH_MSM8X60
	l2_pm_stop_all();
#endif

	/*
	 * We detected that the process was swapped in on one core and out on
	 * a different core.  This does not allow us to stop and stop counters
	 * properly so we need to defer processing.  This checks to see if there
	 * is any defered processing necessary. And does it... */
	if (fake_swap_out[smp_processor_id()] != 0) {
		fake_swap_out[smp_processor_id()] = 0;
		p_old = per_process_find(last_in_pid[smp_processor_id()]);
		last_in_pid[smp_processor_id()] = 0;
		if (p_old != 0)
			per_process_swap_out(p_old);
	}

	/*
	* Clear the data collected so far for this process?
	*/
	if (pp_clear_pid != -1) {
		struct per_process_perf_mon_type *p_clear =
			per_process_find(pp_clear_pid);
		if (p_clear) {
			memset(p_clear->cnts, 0,
			sizeof(struct pm_counters_s)*num_possible_cpus());
			printk(KERN_INFO "Clear Per Processor Stats for \
				PID:%ld\n", pp_clear_pid);
			pp_clear_pid = -1;
		}
	}
       /*
       * Always collect for 0, it collects for all.
       */
       if (pp_enabled) {
		if (first_switch == 1) {
			per_process_initialize(&perf_mons[0], 0);
			first_switch = 0;
		}
		if (pm_global) {
			per_process_swap_out(&perf_mons[0]);
			per_process_swap_in(&perf_mons[0], 0);
		} else {
			p_old = per_process_find(old_pid);
			p_new = per_process_find(new_pid);


			/*
			* save the old counts to the old data struct, if the
			* returned ptr is NULL or the process id passed is not
			* the same as the process id in the data struct then
			* don't update the data.
			*/
			if ((p_old) && (p_old->pid == old_pid) &&
				(p_old->pid != 0)) {
				per_process_swap_out(p_old);
			}

	/*
	* Setup the counters for the new process
	*/
	if (pp_set_pid == new_pid)
		per_process_initialize(p_new, new_pid);
	if ((p_new->pid == new_pid) && (new_pid != 0))
		per_process_swap_in(p_new, new_pid);
	}
		pm_reset_all();
#ifdef CONFIG_ARCH_MSM8X60
	l2_pm_reset_all();
#endif
#ifdef CONFIG_ARCH_QSD8X50
		axi_swaps++;
		if (axi_swaps%pm_axi_info.refresh == 0) {
			if (pm_axi_info.clear == 1) {
				pm_axi_clear_cnts();
					pm_axi_info.clear = 0;
				}
				if (pm_axi_info.enable == 0)
					pm_axi_disable();
				else
					pm_axi_update_cnts();
				axi_swaps = 0;
		}
#endif
	}
	pm_start_all();
#ifdef CONFIG_ARCH_MSM8X60
	l2_pm_start_all();
#endif

    spin_unlock(&pm_lock);
}

/*
FUNCTION  pmInterruptIn

DESCRIPTION
  Called when an interrupt is being processed.  If the pmStopForInterrutps
  flag is non zero then we disable the counting of performance monitors.

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
static int pm_interrupt_nesting_count;
static unsigned long pm_cycle_in, pm_cycle_out;
void _perf_mon_interrupt_in(void)
{
	if (pm_global_enable == 0)
		return;
	if (pm_stop_for_interrupts == 0)
		return;
	pm_interrupt_nesting_count++;	/* Atomic */
	pm_stop_all();
	pm_cycle_in = pm_get_cycle_count();
}

/*
FUNCTION perfMonInterruptOut

DESCRIPTION
  Reenable performance monitor counting whn the nest count goes to zero
  provided the counting has been stoped

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
void _perf_mon_interrupt_out(void)
{
	if (pm_global_enable == 0)
		return;
	if (pm_stop_for_interrupts == 0)
		return;
	--pm_interrupt_nesting_count;  /* Atomic?? */

	if (pm_interrupt_nesting_count <= 0) {
		pm_cycle_out = pm_get_cycle_count();
		if (pm_cycle_in != pm_cycle_out)
			printk(KERN_INFO "pmIn!=pmOut in:%lx out:%lx\n",
			pm_cycle_in, pm_cycle_out);
		if (pp_enabled) {
			pm_start_all();
#ifdef CONFIG_ARCH_MSM8X60
			l2_pm_start_all();
#endif
		}
		pm_interrupt_nesting_count = 0;
	}
}

void per_process_do_global(unsigned long g)
{
	pm_global = g;

	if (pm_global == 1) {
		pm_stop_all();
#ifdef CONFIG_ARCH_MSM8X60
		l2_pm_stop_all();
#endif
		pm_reset_all();
#ifdef CONFIG_ARCH_MSM8X60
		l2_pm_reset_all();
#endif
		pp_set_pid = 0;
		per_process_swap_in(&perf_mons[0], 0);
		pm_start_all();
#ifdef CONFIG_ARCH_MSM8X60
		l2_pm_start_all();
#endif
	} else {
		pm_stop_all();
#ifdef CONFIG_ARCH_MSM8X60
		l2_pm_stop_all();
#endif
	}
}


/*
FUNCTION   per_process_write

DESCRIPTION
  Generic routine to handle any of the settings /proc directory writes.

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
int per_process_write(struct file *file, const char *buff,
    unsigned long cnt, void *data, const char *fmt)
{
	char *newbuf;
	unsigned long *d = (unsigned long *)data;

	/*
	* Alloc the user data in kernel space. and then copy user to kernel
	*/
	newbuf = kmalloc(cnt + 1, GFP_KERNEL);
	if (0 == newbuf)
		return PM_PP_ERR;
	if (copy_from_user(newbuf, buff, cnt) != 0) {
		printk(KERN_INFO "%s copy_from_user failed\n", __func__);
	return cnt;
	}
	sscanf(newbuf, fmt, d);
	kfree(newbuf);

	/*
	* If this is a remove  command then do it now...
	*/
	if (d == &pm_remove_pid)
		per_process_remove_manual(*d);
	if (d == &pm_global)
		per_process_do_global(*d);
	return cnt;
}

int per_process_write_dec(struct file *file, const char *buff,
    unsigned long cnt, void *data)
{
	return per_process_write(file, buff, cnt, data, "%ld");
}

int per_process_write_hex(struct file *file, const char *buff,
    unsigned long cnt, void *data)
{
	return per_process_write(file, buff, cnt, data, "%lx");
}

/*
FUNCTION per_process_read

DESCRIPTION
  Generic read handler for the /proc settings directory.

DEPENDENCIES

RETURN VALUE
  Number of characters to output.

SIDE EFFECTS
*/
int per_process_read(char *page, char **start, off_t off, int count,
   int *eof, void *data)
{
	unsigned long *d = (unsigned long *)data;
	return sprintf(page, "%lx", *d);
}

int per_process_read_decimal(char *page, char **start, off_t off, int count,
   int *eof, void *data)
{
	unsigned long *d = (unsigned long *)data;
	return sprintf(page, "%ld", *d);
}

/*
FUNCTION  per_process_proc_entry

DESCRIPTION
  Create a generic entry for the /proc settings directory.

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
void per_process_proc_entry(char *name, unsigned long *var,
    struct proc_dir_entry *d, int hex)
{
	struct proc_dir_entry *pe;

	pe = create_proc_entry(name, 0777, d);
	if (0 == pe)
		return;
	if (hex) {
		pe->read_proc = per_process_read;
		pe->write_proc = per_process_write_hex;
	} else {
		pe->read_proc = per_process_read_decimal;
		pe->write_proc = per_process_write_dec;
	}
	pe->data = (void *)var;

	if (pp_proc_entry_index >= PP_MAX_PROC_ENTRIES) {
		printk(KERN_INFO "PERF: proc entry overflow,\
		memleak on module unload occured");
	return;
	}
	per_process_proc_names[pp_proc_entry_index++] = name;
}

static int perfmon_notifier(struct notifier_block *self, unsigned long cmd,
	void *v)
{
	static int old_pid = -1;
	struct thread_info *thread = v;
	int current_pid;

	if (cmd != THREAD_NOTIFY_SWITCH)
		return old_pid;

	current_pid = thread->task->pid;
	if (old_pid != -1)
		_per_process_switch(old_pid, current_pid);
	old_pid = current_pid;
	return old_pid;
}

static struct notifier_block perfmon_notifier_block = {
	.notifier_call  = perfmon_notifier,
};

/*
FUNCTION  per_process_perf_init

DESCRIPTION
  Initialze the per process performance monitor variables and /proc space.

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
int per_process_perf_init(void)
{
#ifdef CONFIG_ARCH_MSM8X60
	smp_call_function_single(0, (void *)pm_initialize, (void *)NULL, 1);
	smp_call_function_single(1, (void *)pm_initialize, (void *)NULL, 1);
	l2_pm_initialize();
#else
	pm_initialize();
#endif
	pm_axi_init();
	pm_axi_clear_cnts();
	proc_dir = proc_mkdir("ppPerf", NULL);
	values_dir = proc_mkdir("results", proc_dir);
	settings_dir = proc_mkdir("settings", proc_dir);
	per_process_proc_entry("enable", &pp_enabled, settings_dir, 1);
	per_process_proc_entry("valid", &pp_settings_valid, settings_dir, 1);
	per_process_proc_entry("setPID", &pp_set_pid, settings_dir, 0);
	per_process_proc_entry("clearPID", &pp_clear_pid, settings_dir, 0);
	per_process_proc_entry("event0", &per_proc_event[0], settings_dir, 1);
	per_process_proc_entry("event1", &per_proc_event[1], settings_dir, 1);
	per_process_proc_entry("event2", &per_proc_event[2], settings_dir, 1);
	per_process_proc_entry("event3", &per_proc_event[3], settings_dir, 1);
	per_process_proc_entry("l2_event0", &l2_per_proc_event[0], settings_dir,
		1);
	per_process_proc_entry("l2_event1", &l2_per_proc_event[1], settings_dir,
		1);
	per_process_proc_entry("l2_event2", &l2_per_proc_event[2], settings_dir,
		1);
	per_process_proc_entry("l2_event3", &l2_per_proc_event[3], settings_dir,
		1);
	per_process_proc_entry("debug", &dbg_flags, settings_dir, 1);
	per_process_proc_entry("autolock", &pp_auto_lock, settings_dir, 1);
	per_process_proc_entry("lpm0evtyper", &pp_lpm0evtyper, settings_dir, 1);
	per_process_proc_entry("lpm1evtyper", &pp_lpm1evtyper, settings_dir, 1);
	per_process_proc_entry("lpm2evtyper", &pp_lpm2evtyper, settings_dir, 1);
	per_process_proc_entry("l2lpmevtyper", &pp_l2lpmevtyper, settings_dir,
				1);
	per_process_proc_entry("vlpmevtyper", &pp_vlpmevtyper, settings_dir, 1);
	per_process_proc_entry("l2pmevtyper0", &pp_l2pmevtyper0, settings_dir,
		1);
	per_process_proc_entry("l2pmevtyper1", &pp_l2pmevtyper1, settings_dir,
		1);
	per_process_proc_entry("l2pmevtyper2", &pp_l2pmevtyper2, settings_dir,
		1);
	per_process_proc_entry("l2pmevtyper3", &pp_l2pmevtyper3, settings_dir,
		1);
	per_process_proc_entry("l2pmevtyper4", &pp_l2pmevtyper4, settings_dir,
		1);
	per_process_proc_entry("stopForInterrupts", &pm_stop_for_interrupts,
		settings_dir, 1);
	per_process_proc_entry("global", &pm_global, settings_dir, 1);
	per_process_proc_entry("globalEnable", &pm_global_enable, settings_dir,
				1);
	per_process_proc_entry("removePID", &pm_remove_pid, settings_dir, 0);

	axi_dir = proc_mkdir("axi", proc_dir);
	axi_settings_dir = proc_mkdir("settings", axi_dir);
	axi_results_dir = proc_mkdir("results", axi_dir);
	pm_axi_set_proc_entry("axi_enable", &pm_axi_info.enable,
		axi_settings_dir, 1);
	pm_axi_set_proc_entry("axi_clear", &pm_axi_info.clear, axi_settings_dir,
		0);
	pm_axi_set_proc_entry("axi_valid", &pm_axi_info.valid, axi_settings_dir,
		1);
	pm_axi_set_proc_entry("axi_sel_reg0", &pm_axi_info.sel_reg0,
		axi_settings_dir, 1);
	pm_axi_set_proc_entry("axi_sel_reg1", &pm_axi_info.sel_reg1,
		axi_settings_dir, 1);
	pm_axi_set_proc_entry("axi_ten_sel", &pm_axi_info.ten_sel_reg,
		axi_settings_dir, 1);
	pm_axi_set_proc_entry("axi_refresh", &pm_axi_info.refresh,
		axi_settings_dir, 1);
	pm_axi_get_cnt_proc_entry("axi_cnts", &axi_cnts, axi_results_dir, 0);
	l2_dir = proc_mkdir("l2", proc_dir);
	l2_results_dir = proc_mkdir("results", l2_dir);

	memset(perf_mons, 0, sizeof(perf_mons));
	per_process_create_results_proc(&perf_mons[0]);
	per_process_create_l2_results_proc(&perf_mons[0]);
	thread_register_notifier(&perfmon_notifier_block);
	/*
	* Set the function pointers so the module can be activated.
	*/
	pp_interrupt_out_ptr = _perf_mon_interrupt_out;
	pp_interrupt_in_ptr  = _perf_mon_interrupt_in;
	pp_process_remove_ptr = _per_process_remove;
	pp_loaded = 1;
	pm_axi_info.refresh = 1;

#ifdef CONFIG_ARCH_MSM8X60
	smp_call_function_single(0, (void *)pm_reset_all, (void *)NULL, 1);
	smp_call_function_single(1, (void *)pm_reset_all, (void *)NULL, 1);
	smp_call_function_single(0, (void *)l2_pm_reset_all, (void *)NULL, 1);
	smp_call_function_single(1, (void *)l2_pm_reset_all, (void *)NULL, 1);
#else
	pm_reset_all();
#endif

	return 0;
}

/*
FUNCTION  per_process_perf_exit

DESCRIPTION
  Module exit functionm, clean up, renmove proc entries

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
  No more per process
*/
void per_process_perf_exit(void)
{
	unsigned long i;
	/*
	* Sert the function pointers to 0 so the functions will no longer
	* be invoked
	*/
	pp_loaded = 0;
	pp_interrupt_out_ptr = 0;
	pp_interrupt_in_ptr  = 0;
	pp_process_remove_ptr = 0;
	/*
	* Remove the results
	*/
	for (i = 0; i < PERF_MON_PROCESS_NUM; i++)
		per_process_remove_manual(perf_mons[i].pid);
	/*
	* Remove the proc entries in the settings dir
	*/
	i = 0;
	for (i = 0; i < pp_proc_entry_index; i++)
		remove_proc_entry(per_process_proc_names[i], settings_dir);

	/*remove proc axi files*/
	remove_proc_entry("axi_enable", axi_settings_dir);
	remove_proc_entry("axi_valid", axi_settings_dir);
	remove_proc_entry("axi_refresh", axi_settings_dir);
	remove_proc_entry("axi_clear", axi_settings_dir);
	remove_proc_entry("axi_sel_reg0", axi_settings_dir);
	remove_proc_entry("axi_sel_reg1", axi_settings_dir);
	remove_proc_entry("axi_ten_sel", axi_settings_dir);
	remove_proc_entry("axi_cnts", axi_results_dir);
	/*
	* Remove the directories
	*/
	remove_proc_entry("results", l2_dir);
	remove_proc_entry("l2", proc_dir);
	remove_proc_entry("results", proc_dir);
	remove_proc_entry("settings", proc_dir);
	remove_proc_entry("results", axi_dir);
	remove_proc_entry("settings", axi_dir);
	remove_proc_entry("axi", proc_dir);
	remove_proc_entry("ppPerf", NULL);
	pm_free_irq();
#ifdef CONFIG_ARCH_MSM8X60
	l2_pm_free_irq();
#endif
	thread_unregister_notifier(&perfmon_notifier_block);
#ifdef CONFIG_ARCH_MSM8X60
	smp_call_function_single(0, (void *)pm_deinitialize, (void *)NULL, 1);
	smp_call_function_single(1, (void *)pm_deinitialize, (void *)NULL, 1);
	l2_pm_deinitialize();
#else
	pm_deinitialize();
#endif
}
