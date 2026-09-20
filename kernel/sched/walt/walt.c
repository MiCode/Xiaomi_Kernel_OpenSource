// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/syscore_ops.h>
#include <linux/cpufreq.h>
#include <linux/list_sort.h>
#include <linux/jiffies.h>
#include <linux/sched/stat.h>
#include <linux/module.h>
#include <linux/cpumask.h>
#include <linux/arch_topology.h>
#include <linux/cpu.h>
#include <linux/sysctl.h>
#include <linux/of_platform.h>
#include <linux/delay.h>
#include <linux/time64.h>

#include <trace/hooks/sched.h>
#include <trace/hooks/cpufreq.h>
#include <trace/events/power.h>
#include "walt.h"
#include "trace.h"
// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_MI_SCHED_WALT
#include <linux/pkg_stat.h>
#endif
// END Performance_TurboSched

// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_METIS_WALT
#include "../../../drivers/mihw/include/mi_module.h"
extern void mi_wake_up_new_task_hook(struct task_struct *new);
#endif
// END Performance_TurboSched
// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_MI_SOC_DFLT
extern void trigger_mist_load_tracking(struct workqueue_struct *migt_wq);	
#endif
// END Performance_TurboSched
// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_MIGT_WALT
mi_update_fravg_hooks mi_update_fravg_f;
void register_mi_update_fravg(mi_update_fravg_hooks f)
{
	pr_info("%s now\n", __FUNCTION__);
	mi_update_fravg_f = f;
}
EXPORT_SYMBOL_GPL(register_mi_update_fravg);
void unregister_mi_update_fravg(void)
{
	mi_update_fravg_f = NULL;
}
EXPORT_SYMBOL_GPL(unregister_mi_update_fravg);
walt_window_update_f walt_window_update_hooks;
void register_walt_update_hooks(walt_window_update_f f)
{
	pr_info("%s now\n", __FUNCTION__);
	walt_window_update_hooks = f;
}
EXPORT_SYMBOL_GPL(register_walt_update_hooks);
void unregister_walt_update_hooks(void)
{
	walt_window_update_hooks = NULL;
}
EXPORT_SYMBOL_GPL(unregister_walt_update_hooks);
typedef void (*oem_get_ravg_window_f)(unsigned int);
static oem_get_ravg_window_f oem_get_ravg_window_hook = NULL;
void register_game_ravg_window(oem_get_ravg_window_f f){
	if (likely(f))
		oem_get_ravg_window_hook = f;
}
EXPORT_SYMBOL_GPL(register_game_ravg_window);
void unregister_game_ravg_window(void){
	oem_get_ravg_window_hook = NULL;
}
EXPORT_SYMBOL_GPL(unregister_game_ravg_window);
#endif

#ifdef CONFIG_MI_SOC_DFLT
extern void mi_dflt_schedule_hook(void);
#endif
// END Performance_TurboSched

const char *task_event_names[] = {
	"PUT_PREV_TASK",
	"PICK_NEXT_TASK",
	"TASK_WAKE",
	"TASK_MIGRATE",
	"TASK_UPDATE",
	"IRQ_UPDATE"
};

const char *migrate_type_names[] = {
	"GROUP_TO_RQ",
	"RQ_TO_GROUP",
	"RQ_TO_RQ",
	"GROUP_TO_GROUP"
};

#define SCHED_FREQ_ACCOUNT_WAIT_TIME 0
#define SCHED_ACCOUNT_WAIT_TIME 1

#define EARLY_DETECTION_DURATION 9500000
#define MAX_NUM_CGROUP_COLOC_ID 20

#define NEW_TASK_ACTIVE_TIME 100000000

cpumask_t walt_cpus_taken_mask = { CPU_BITS_NONE };
DEFINE_SPINLOCK(cpus_taken_lock);
DEFINE_PER_CPU(int, cpus_taken_refcount);

cpumask_t walt_enforce_high_irq_cpu_mask = { CPU_BITS_NONE };
DEFINE_SPINLOCK(enforce_high_irq_cpu_lock);
DEFINE_PER_CPU(int, enforce_high_irq_cpus_refcount);

DEFINE_PER_CPU(struct walt_rq, walt_rq);
// MIUI ADD: Performance_TurboSched
EXPORT_PER_CPU_SYMBOL_GPL(walt_rq);
// END Performance_TurboSched

// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_MI_SCHED_WALT
extern void update_pkg_load(struct task_struct *tsk, int cpu, int flag,
                u64 wallclock, u64 delta);
extern bool pkg_enable(void);
extern void mi_task_fork(void *nouse, struct task_struct *tsk);
#endif
// END Performance_TurboSched

// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_METIS_WALT
extern struct enqueue_task_fair_hooks mi_enqueue_task_fair_func[MI_SCHED_TYPE];
extern struct dequeue_task_fair_hooks mi_dequeue_task_fair_func[MI_SCHED_TYPE];
extern bool oem_rvh_try_to_wake_up_success(struct task_struct *p);
extern void mi_schedule_hook(struct task_struct *prev,
        struct task_struct *next, struct rq *rq);
extern void metis_update_top_task_info(struct task_struct *p, struct rq *rq, bool busy,
                        u64 wallclock, u64 mark_start);
extern void mi_scheduler_tick_hook(struct rq *rq);
extern struct mi_try_to_wake_up_hooks mi_try_to_wake_up_func[MI_SCHED_TYPE];
extern void metis_tp_fixup_cumulative_runnable_avg_hook(struct rq *rq, struct task_struct *p,
		s64 demand_scaled_delta, s64 pred_demand_scaled_delta, int cmd,
		u64 raw_cumulative_runnable_avg_scaled, u64 raw_pred_demands_sum_scaled);
extern cpumask_t __read_mostly **metis_lb_cpu_array;
extern int metis_lb_min_possible_cluster_id;
extern void metis_schlat_schedule_hook(struct task_struct *prev, struct task_struct *next, int cpu);
#endif

#ifdef CONFIG_MI_SCHED_WALT
extern void oem_update_num_sched_clusters(int clusters);
#endif
// END Performance_TurboSched

unsigned int sysctl_sched_user_hint;
static u64 sched_clock_last;
static bool walt_clock_suspended;
DECLARE_COMPLETION(walt_get_cycle_counts_cb_completion);
bool use_cycle_counter;
u64 (*walt_get_cycle_counts_cb)(int cpu, u64 wc);

static u64 walt_load_reported_window;

struct irq_work walt_cpufreq_irq_work;
struct irq_work walt_migration_irq_work;
unsigned int walt_rotation_enabled;

unsigned int __read_mostly sched_ravg_window = 20000000;
int min_possible_cluster_id;
int max_possible_cluster_id;
/* Initial task load. Newly created tasks are assigned this load. */
unsigned int __read_mostly sched_init_task_load_windows;
/*
 * Task load is categorized into buckets for the purpose of top task tracking.
 * The entire range of load from 0 to sched_ravg_window needs to be covered
 * in NUM_LOAD_INDICES number of buckets. Therefore the size of each bucket
 * is given by sched_ravg_window / NUM_LOAD_INDICES. Since the default value
 * of sched_ravg_window is DEFAULT_SCHED_RAVG_WINDOW, use that to compute
 * sched_load_granule.
 */
unsigned int __read_mostly sched_load_granule;

// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_MIGT_WALT
typedef void (*oem_update_f)(struct task_struct *p, struct rq *rq, u32 fstat, u64 wallclock,
		       u64 irqtime, int event);
static oem_update_f oem_update_frame_load_hook = NULL;
typedef int (*oem_flt_enable_f)(void);
static oem_flt_enable_f oem_flt_enable_hook = NULL;
#endif
// END Performance_TurboSched
static struct walt_related_thread_group
			*related_thread_groups[MAX_NUM_CGROUP_COLOC_ID];
static LIST_HEAD(active_related_thread_groups);
static DEFINE_RWLOCK(related_thread_group_lock);

bool walt_is_idle_task(struct task_struct *p)
{
	return walt_flag_test(p, WALT_IDLE_TASK_BIT);
}

u64 walt_sched_clock(void)
{
	if (unlikely(walt_clock_suspended))
		return sched_clock_last;
	return sched_clock();
}
// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_MIGT_WALT
EXPORT_SYMBOL_GPL(walt_sched_clock);
#endif
// END Performance_TurboSched

static void walt_resume(void)
{
	walt_clock_suspended = false;
}

static int walt_suspend(void)
{
	sched_clock_last = sched_clock();
	walt_clock_suspended = true;
	return 0;
}

static struct syscore_ops walt_syscore_ops = {
	.resume		= walt_resume,
	.suspend	= walt_suspend
};

/*
 *@boost:should be 0,1,2.
 *@period:boost time based on ms units.
 */
int set_task_boost(int boost, u64 period)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) current->android_vendor_data1;

	if (unlikely(walt_disabled))
		return -EAGAIN;

	if (boost < TASK_BOOST_NONE || boost >= TASK_BOOST_END)
		return -EINVAL;
	if (boost) {
		wts->boost = boost;
		wts->boost_period = (u64)period * 1000 * 1000;
		wts->boost_expires = walt_sched_clock() + wts->boost_period;
	} else {
		wts->boost = 0;
		wts->boost_expires = 0;
		wts->boost_period = 0;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(set_task_boost);

static inline void acquire_rq_locks_irqsave(const cpumask_t *cpus,
				     unsigned long *flags)
{
	int cpu;
	int level;

	local_irq_save(*flags);

	level = 0;
	for_each_cpu(cpu, cpus) {
		if (level == 0)
			raw_spin_lock(&cpu_rq(cpu)->__lock);
		else
			raw_spin_lock_nested(&cpu_rq(cpu)->__lock, level);
		level++;
	}
}

static inline void release_rq_locks_irqrestore(const cpumask_t *cpus,
					unsigned long *flags)
{
	int cpu;

	for_each_cpu(cpu, cpus)
		raw_spin_unlock(&cpu_rq(cpu)->__lock);
	local_irq_restore(*flags);
}

static inline u64 walt_rq_clock(struct rq *rq)
{
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));

	if (unlikely(walt_clock_suspended))
		return sched_clock_last;

	walt_lockdep_assert_rq(rq, NULL);

	if (!(rq->clock_update_flags & RQCF_UPDATED))
		update_rq_clock(rq);

	return max(rq_clock(rq), wrq->latest_clock);
}

static unsigned int walt_cpu_high_irqload;

static __read_mostly unsigned int sched_io_is_busy = 1;

/* Window size (in ns) */
static __read_mostly unsigned int new_sched_ravg_window = DEFAULT_SCHED_RAVG_WINDOW;

static DEFINE_SPINLOCK(sched_ravg_window_lock);
static u64 sched_ravg_window_change_time;

static unsigned int __read_mostly sched_init_task_load_windows_scaled;

/* Size of bitmaps maintained to track top tasks */
static const unsigned int top_tasks_bitmap_size =
		BITS_TO_LONGS(NUM_LOAD_INDICES + 1) * sizeof(unsigned long);

__read_mostly unsigned int walt_scale_demand_divisor;

#define SCHED_PRINT(arg)	printk_deferred("%s=%llu", #arg, (unsigned long long)arg)
#define STRG(arg)		#arg

void walt_task_dump(struct task_struct *p)
{
	char buff[WALT_NR_CPUS * 16];
	int i, j = 0;
	int buffsz = WALT_NR_CPUS * 16;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	bool is_32bit_thread = is_compat_thread(task_thread_info(p));

	printk_deferred("Task: %.16s-%d\n", p->comm, p->pid);
	SCHED_PRINT(READ_ONCE(p->__state));
	SCHED_PRINT(task_thread_info(p)->cpu);
	SCHED_PRINT(p->policy);
	SCHED_PRINT(p->prio);
	SCHED_PRINT(wts->mark_start);
	SCHED_PRINT(wts->demand);
	SCHED_PRINT(wts->coloc_demand);
	SCHED_PRINT(wts->enqueue_after_migration);
	SCHED_PRINT(wts->prev_cpu);
	SCHED_PRINT(wts->new_cpu);
	SCHED_PRINT(wts->misfit);
	SCHED_PRINT(wts->prev_on_rq);
	SCHED_PRINT(wts->prev_on_rq_cpu);
	SCHED_PRINT(wts->mvp_prio);
	SCHED_PRINT(wts->iowaited);
	SCHED_PRINT(sched_ravg_window);
	SCHED_PRINT(new_sched_ravg_window);

	for (i = 0 ; i < nr_cpu_ids; i++)
		j += scnprintf(buff + j, buffsz - j, "%u ",
				wts->curr_window_cpu[i]);
	printk_deferred("%s=%u (%s)\n", STRG(wts->curr_window),
			wts->curr_window, buff);

	for (i = 0, j = 0 ; i < nr_cpu_ids; i++)
		j += scnprintf(buff + j, buffsz - j, "%u ",
				wts->prev_window_cpu[i]);
	printk_deferred("%s=%u (%s)\n", STRG(wts->prev_window),
			wts->prev_window, buff);

	SCHED_PRINT(wts->last_sleep_ts);
	SCHED_PRINT(wts->last_wake_ts);
	SCHED_PRINT(wts->last_enqueued_ts);
	SCHED_PRINT(wts->mark_start_birth_ts);
	SCHED_PRINT(wts->misfit);
	SCHED_PRINT(wts->unfilter);
	SCHED_PRINT(is_32bit_thread);
	SCHED_PRINT(wts->grp);
	SCHED_PRINT(p->on_cpu);
	SCHED_PRINT(p->on_rq);
}

void walt_rq_dump(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	struct task_struct *tsk = cpu_curr(cpu);
	int i;
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));

	/*
	 * Increment the task reference so that it can't be
	 * freed on a remote CPU. Since we are going to
	 * enter panic, there is no need to decrement the
	 * task reference. Decrementing the task reference
	 * can't be done in atomic context, especially with
	 * rq locks held.
	 */
	get_task_struct(tsk);
	printk_deferred("CPU:%d nr_running:%u current: %d (%s)\n",
			cpu, rq->nr_running, tsk->pid, tsk->comm);

	printk_deferred("==========================================");
	SCHED_PRINT(wrq->latest_clock);
	SCHED_PRINT(wrq->window_start);
	SCHED_PRINT(wrq->prev_window_size);
	SCHED_PRINT(wrq->curr_runnable_sum);
	SCHED_PRINT(wrq->prev_runnable_sum);
	SCHED_PRINT(wrq->nt_curr_runnable_sum);
	SCHED_PRINT(wrq->nt_prev_runnable_sum);
	SCHED_PRINT(wrq->task_exec_scale);
	SCHED_PRINT(wrq->grp_time.curr_runnable_sum);
	SCHED_PRINT(wrq->grp_time.prev_runnable_sum);
	SCHED_PRINT(wrq->grp_time.nt_curr_runnable_sum);
	SCHED_PRINT(wrq->grp_time.nt_prev_runnable_sum);
	for (i = 0 ; i < NUM_TRACKED_WINDOWS; i++) {
		printk_deferred("wrq->load_subs[%d].window_start=%llu)\n", i,
				wrq->load_subs[i].window_start);
		printk_deferred("wrq->load_subs[%d].subs=%llu)\n", i,
				wrq->load_subs[i].subs);
		printk_deferred("wrq->load_subs[%d].new_subs=%llu)\n", i,
				wrq->load_subs[i].new_subs);
	}
	walt_task_dump(tsk);
	SCHED_PRINT(sched_capacity_margin_up[cpu]);
	SCHED_PRINT(sched_capacity_margin_down[cpu]);
}

void walt_dump(void)
{
	int cpu;

	printk_deferred("============ WALT RQ DUMP START ==============\n");
	printk_deferred("Sched clock: %llu\n", walt_sched_clock());
	printk_deferred("Time last window changed=%llu\n",
			sched_ravg_window_change_time);
	printk_deferred("global_ws=%llu\n",
			 atomic64_read(&walt_irq_work_lastq_ws));
	for_each_online_cpu(cpu)
		walt_rq_dump(cpu);
	SCHED_PRINT(max_possible_cluster_id);
	printk_deferred("============ WALT RQ DUMP END ==============\n");
}

int in_sched_bug;

static inline void
fixup_cumulative_runnable_avg(struct rq *rq,
			      struct task_struct *p,
			      struct walt_sched_stats *stats,
			      s64 demand_scaled_delta,
			      s64 pred_demand_scaled_delta)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	s64 cumulative_runnable_avg_scaled =
		stats->cumulative_runnable_avg_scaled + demand_scaled_delta;
	s64 pred_demands_sum_scaled =
		stats->pred_demands_sum_scaled + pred_demand_scaled_delta;

	walt_lockdep_assert_rq(rq, p);

	if (task_rq(p) != rq)
		WALT_BUG(WALT_BUG_UPSTREAM, p, "on CPU %d task %s(%d) not on rq %d",
			 raw_smp_processor_id(), p->comm, p->pid, rq->cpu);

	if (cumulative_runnable_avg_scaled < 0) {
		WALT_BUG(WALT_BUG_WALT, p, "on CPU %d task ds=%hu is higher than cra=%llu\n",
			 raw_smp_processor_id(), wts->demand_scaled,
			 stats->cumulative_runnable_avg_scaled);
		cumulative_runnable_avg_scaled = 0;
	}
	stats->cumulative_runnable_avg_scaled = (u64)cumulative_runnable_avg_scaled;

	if (pred_demands_sum_scaled < 0) {
		WALT_BUG(WALT_BUG_WALT, p, "on CPU %d task pds=%hu is higher than pds_sum=%llu\n",
			 raw_smp_processor_id(), wts->pred_demand_scaled,
			 stats->pred_demands_sum_scaled);
		pred_demands_sum_scaled = 0;
	}
	stats->pred_demands_sum_scaled = (u64)pred_demands_sum_scaled;
}

static void fixup_walt_sched_stats_common(struct rq *rq, struct task_struct *p,
				   u16 updated_demand_scaled,
				   u16 updated_pred_demand_scaled)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	s64 task_load_delta = (s64)updated_demand_scaled -
			      wts->demand_scaled;
	s64 pred_demand_delta = (s64)updated_pred_demand_scaled -
				wts->pred_demand_scaled;
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));

	fixup_cumulative_runnable_avg(rq, p, &wrq->walt_stats, task_load_delta,
				      pred_demand_delta);

// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_METIS_WALT
	metis_tp_fixup_cumulative_runnable_avg_hook(rq, p, task_load_delta,
				pred_demand_delta, LAYERED_LOAD_UPDATE,
				wrq->walt_stats.cumulative_runnable_avg_scaled,
				wrq->walt_stats.pred_demands_sum_scaled);
#endif
// END Performance_TurboSched
}

static void rollover_cpu_window(struct rq *rq, bool full_window);
static void rollover_top_tasks(struct rq *rq, bool full_window);

/*
 * Demand aggregation for frequency purpose:
 *
 * CPU demand of tasks from various related groups is aggregated per-cluster and
 * added to the "max_busy_cpu" in that cluster, where max_busy_cpu is determined
 * by just wrq->prev_runnable_sum.
 *
 * Some examples follow, which assume:
 *	Cluster0 = CPU0-3, Cluster1 = CPU4-7
 *	One related thread group A that has tasks A0, A1, A2
 *
 *	A->cpu_time[X].curr/prev_sum = counters in which cpu execution stats of
 *	tasks belonging to group A are accumulated when they run on cpu X.
 *
 *	CX->curr/prev_sum = counters in which cpu execution stats of all tasks
 *	not belonging to group A are accumulated when they run on cpu X
 *
 * Lets say the stats for window M was as below:
 *
 *	C0->prev_sum = 1ms, A->cpu_time[0].prev_sum = 5ms
 *		Task A0 ran 5ms on CPU0
 *		Task B0 ran 1ms on CPU0
 *
 *	C1->prev_sum = 5ms, A->cpu_time[1].prev_sum = 6ms
 *		Task A1 ran 4ms on CPU1
 *		Task A2 ran 2ms on CPU1
 *		Task B1 ran 5ms on CPU1
 *
 *	C2->prev_sum = 0ms, A->cpu_time[2].prev_sum = 0
 *		CPU2 idle
 *
 *	C3->prev_sum = 0ms, A->cpu_time[3].prev_sum = 0
 *		CPU3 idle
 *
 * In this case, CPU1 was most busy going by just its prev_sum counter. Demand
 * from all group A tasks are added to CPU1. IOW, at end of window M, cpu busy
 * time reported to governor will be:
 *
 *
 *	C0 busy time = 1ms
 *	C1 busy time = 5 + 5 + 6 = 16ms
 *
 */
__read_mostly bool sched_freq_aggr_en;

static u64
update_window_start(struct rq *rq, u64 wallclock, int event)
{
	s64 delta;
	int nr_windows;
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));
	u64 old_window_start = wrq->window_start;
	bool full_window;

	if (wallclock < wrq->latest_clock) {
		printk_deferred("WALT-BUG CPU%d; wallclock=%llu(0x%llx) is lesser than latest_clock=%llu(0x%llx)",
				rq->cpu, wallclock, wallclock, wrq->latest_clock,
				wrq->latest_clock);
		WALT_PANIC(1);
	}
	delta = wallclock - wrq->window_start;
	if (delta < 0) {
		printk_deferred("WALT-BUG CPU%d; wallclock=%llu(0x%llx) is lesser than window_start=%llu(0x%llx)",
				rq->cpu, wallclock, wallclock,
				wrq->window_start, wrq->window_start);
		WALT_PANIC(1);
	}
	wrq->latest_clock = wallclock;
	if (delta < sched_ravg_window)
		return old_window_start;

	nr_windows = div64_u64(delta, sched_ravg_window);
	wrq->window_start += (u64)nr_windows * (u64)sched_ravg_window;

	wrq->prev_window_size = sched_ravg_window;

	full_window = nr_windows > 1;
	rollover_cpu_window(rq, full_window);
	rollover_top_tasks(rq, full_window);

	/* Update yielder statistics */
	if (cpu_of(rq) == 0)
		account_yields(wallclock);

	return old_window_start;
}

/*
 * Assumes rq_lock is held and wallclock was recorded in the same critical
 * section as this function's invocation.
 */
static inline u64 read_cycle_counter(int cpu, u64 wallclock)
{
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu);

	if (wrq->last_cc_update != wallclock) {
		wrq->cycles = walt_get_cycle_counts_cb(cpu, wallclock);
		wrq->last_cc_update = wallclock;
	}

	return wrq->cycles;
}

static void update_task_cpu_cycles(struct task_struct *p, int cpu,
				   u64 wallclock)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	if (use_cycle_counter)
		wts->cpu_cycles = read_cycle_counter(cpu, wallclock);
}

static inline bool is_ed_enabled(void)
{
	return (boost_policy != SCHED_BOOST_NONE);
}

static inline bool is_ed_task(struct task_struct *p, u64 wallclock)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	return (wallclock - wts->last_wake_ts >= EARLY_DETECTION_DURATION);
}

static bool is_ed_task_present(struct rq *rq, u64 wallclock, struct task_struct *deq_task)
{
	struct task_struct *p;
	int loop_max = 10;
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));

	wrq->ed_task = NULL;

	if (!is_ed_enabled() || !rq->cfs.h_nr_running)
		return false;

	list_for_each_entry(p, &rq->cfs_tasks, se.group_node) {
		if (!loop_max)
			break;

		if (p == deq_task)
			continue;

		if (is_ed_task(p, wallclock)) {
			wrq->ed_task = p;
			return true;
		}

		loop_max--;
	}

	return false;
}

// MIUI ADD: Game_TurboSched
#ifdef CONFIG_MIGT_WALT
extern void walt_update_task_ravg(struct task_struct *p, struct rq *rq, int event,
						u64 wallclock, u64 irqtime);
#else
static void walt_update_task_ravg(struct task_struct *p, struct rq *rq, int event,
						u64 wallclock, u64 irqtime);
#endif
// END Game_TurboSched
/*
 * Return total number of tasks "eligible" to run on higher capacity cpus
 */
unsigned int walt_big_tasks(int cpu)
{
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu);

	return wrq->walt_stats.nr_big_tasks;
}

int walt_trailblazer_tasks(int cpu)
{
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu);

	return wrq->walt_stats.nr_trailblazer_tasks;
}

bool trailblazer_on_prime(void)
{
	int cpu;

	for_each_cpu(cpu, &cpu_array[0][num_sched_clusters - 1]) {
		if (walt_trailblazer_tasks(cpu))
			return true;
	}

	return false;
}

static void clear_walt_request(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long flags;
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));

	clear_reserved(cpu);
	if (wrq->push_task) {
		struct task_struct *push_task = NULL;

		raw_spin_lock_irqsave(&rq->__lock, flags);
		if (wrq->push_task) {
			clear_reserved(rq->push_cpu);
			push_task = wrq->push_task;
			wrq->push_task = NULL;
		}
		rq->active_balance = 0;
		raw_spin_unlock_irqrestore(&rq->__lock, flags);
		if (push_task)
			put_task_struct(push_task);
	}
}

/*
 * Special case the last index and provide a fast path for index = 0.
 * Note that sched_load_granule can change underneath us if we are not
 * holding any runqueue locks while calling the two functions below.
 */
static u32 top_task_load(struct rq *rq)
{
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));
	int index = wrq->prev_top;
	u8 prev = 1 - wrq->curr_table;

	if (!index) {
		int msb = NUM_LOAD_INDICES - 1;

		if (!test_bit(msb, wrq->top_tasks_bitmap[prev]))
			return 0;
		else
			return sched_load_granule;
	} else if (index == NUM_LOAD_INDICES - 1) {
		return sched_ravg_window;
	} else {
		return (index + 1) * sched_load_granule;
	}
}

unsigned long sched_user_hint_reset_time;
static bool is_cluster_hosting_top_app(struct walt_sched_cluster *cluster);

static inline bool
should_apply_suh_freq_boost(struct walt_sched_cluster *cluster)
{
	if (sched_freq_aggr_en || !sysctl_sched_user_hint ||
				  !cluster->aggr_grp_load)
		return false;

	return is_cluster_hosting_top_app(cluster);
}

static inline u64 freq_policy_load(struct rq *rq, unsigned int *reason, bool trace)
{
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));
	struct walt_sched_cluster *cluster = wrq->cluster;
	u64 aggr_grp_load = cluster->aggr_grp_load;
	u64 load, tt_load = 0, kload = 0;
	struct task_struct *cpu_ksoftirqd = per_cpu(ksoftirqd, cpu_of(rq));

	if (sched_freq_aggr_en) {
		load = wrq->prev_runnable_sum + aggr_grp_load;
		*reason = CPUFREQ_REASON_FREQ_AGR_BIT;
	} else {
		load = wrq->prev_runnable_sum +
					wrq->grp_time.prev_runnable_sum;
	}

	if (cpu_ksoftirqd && READ_ONCE(cpu_ksoftirqd->__state) == TASK_RUNNING) {
		kload = task_load(cpu_ksoftirqd);
		if (kload > load) {
			load = kload;
			*reason = CPUFREQ_REASON_KSOFTIRQD_BIT;
		}
	}

	tt_load = top_task_load(rq);
	if (tt_load > load) {
		load = tt_load;
		*reason = CPUFREQ_REASON_TT_LOAD_BIT;
	}

	if (should_apply_suh_freq_boost(cluster)) {
		if (is_suh_max())
			load = sched_ravg_window;
		else
			load = div64_u64(load * sysctl_sched_user_hint,
					 (u64)100);
		*reason = CPUFREQ_REASON_SUH_BIT;
	}

	if (wrq->ed_task) {
		load = mult_frac(load, 100 + sysctl_ed_boost_pct, 100);
		*reason = CPUFREQ_REASON_EARLY_DET_BIT;
	}

	if (wrq->lrb_pipeline_start_time) {
		load = mult_frac(load, 100 + sysctl_pipeline_busy_boost_pct, 100);
		*reason = CPUFREQ_REASON_PIPELINE_BUSY_BIT;
	}

	if (walt_rotation_enabled) {
		load = sched_ravg_window;
		*reason = CPUFREQ_REASON_BTR_BIT;
	}

	if (walt_trailblazer_tasks(cpu_of(rq)) && walt_feat(WALT_FEAT_TRAILBLAZER_BIT)) {
		load = sched_ravg_window;
		*reason = CPUFREQ_REASON_TRAILBLAZER_CPU_BIT;
	}

	if (trace)
		trace_sched_load_to_gov(rq, aggr_grp_load, tt_load, sched_freq_aggr_en,
				load, 0, walt_rotation_enabled,
				sysctl_sched_user_hint, wrq, *reason);
	return load;
}

static bool rtgb_active;

static inline unsigned long
__cpu_util_freq_walt(int cpu, struct walt_cpu_load *walt_load, unsigned int *reason, bool trace)
{
	u64 util;
	struct rq *rq = cpu_rq(cpu);
	unsigned long capacity = capacity_orig_of(cpu);
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));

	util = scale_time_to_util(freq_policy_load(rq, reason, trace));

	/*
	 * util is on a scale of 0 to 1024.  this is the utilization
	 * of the cpu in the last window
	 */
	wrq->util = util;

	if (walt_load) {
		u64 nl = wrq->nt_prev_runnable_sum +
				wrq->grp_time.nt_prev_runnable_sum;
		u64 pl = wrq->walt_stats.pred_demands_sum_scaled;

		wrq->old_busy_time = util;
		wrq->old_estimated_time = pl;

		nl = scale_time_to_util(nl);
		walt_load->nl = nl;
		walt_load->pl = pl;
		walt_load->ws = walt_load_reported_window;
		walt_load->rtgb_active = rtgb_active;
		if (wrq->ed_task)
			walt_load->ed_active = true;
		else
			walt_load->ed_active = false;
		walt_load->trailblazer_state = trailblazer_state;
	}

	return (util >= capacity) ? capacity : util;
}

#define PIPELINE_SYNC_VAL(first, second, x)       \
	(max(first, mult_frac(second, x, 100)))

int other_sync_pct(unsigned long util_other)
{
	int pct;

	if (sched_ravg_window >= SCHED_RAVG_16MS_WINDOW) {
		if (util_other <=
			load_sync_util_thres_60fps[num_sched_clusters - 2][num_sched_clusters - 1])
			pct =
			load_sync_low_pct_60fps[num_sched_clusters - 2][num_sched_clusters - 1];
		else
			pct =
			load_sync_high_pct_60fps[num_sched_clusters - 2][num_sched_clusters - 1];

		return pct;
	}

	if (util_other <= load_sync_util_thres[num_sched_clusters - 2][num_sched_clusters - 1])
		pct = load_sync_low_pct[num_sched_clusters - 2][num_sched_clusters - 1];
	else
		pct = load_sync_high_pct[num_sched_clusters - 2][num_sched_clusters - 1];

	return pct;
}

int prime_sync_pct(unsigned long util_prime)
{
	int pct;

	if (sched_ravg_window >= SCHED_RAVG_16MS_WINDOW) {
		if (util_prime <=
			load_sync_util_thres_60fps[num_sched_clusters - 1][num_sched_clusters - 2])
			pct =
			load_sync_low_pct_60fps[num_sched_clusters - 1][num_sched_clusters - 2];
		else
			pct =
			load_sync_high_pct_60fps[num_sched_clusters - 1][num_sched_clusters - 2];

		return pct;
	}

	if (util_prime <= load_sync_util_thres[num_sched_clusters - 1][num_sched_clusters - 2])
		pct = load_sync_low_pct[num_sched_clusters - 1][num_sched_clusters - 2];
	else
		pct = load_sync_high_pct[num_sched_clusters - 1][num_sched_clusters - 2];

	return pct;
}

unsigned long
cpu_util_freq_walt(int cpu, struct walt_cpu_load *walt_load, unsigned int *reason)
{
	struct walt_cpu_load wl_other = {0};
	struct walt_cpu_load wl_prime = {0};
	unsigned long util = 0, util_other = 0, util_prime = 0;
	unsigned long capacity = capacity_orig_of(cpu);
	int i, mpct_other, mpct_prime;
	unsigned long max_nl_other = 0, max_pl_other = 0;
	unsigned long max_nl_prime = 0, max_pl_prime = 0;

	util =  __cpu_util_freq_walt(cpu, walt_load, reason, true);

	if (enable_load_sync(cpu)) {
		for_each_cpu(i, &pipeline_sync_cpus) {
			if (cpumask_test_cpu(i, &cpu_array[0][num_sched_clusters-1])) {
				util_prime = max(util_prime,
						__cpu_util_freq_walt(i, &wl_prime, reason, false));
				max_nl_prime = max(max_nl_prime, wl_prime.nl);
				max_pl_prime = max(max_pl_prime, wl_prime.pl);
			} else {
				util_other = max(util_other,
						__cpu_util_freq_walt(i, &wl_other, reason, false));
				max_nl_other = max(max_nl_other, wl_other.nl);
				max_pl_other = max(max_pl_other, wl_other.pl);
			}
		}

		mpct_other = other_sync_pct(util_other);
		mpct_prime = prime_sync_pct(util_prime);

		if (cpumask_test_cpu(cpu, &cpu_array[0][num_sched_clusters-1])) {
			util = PIPELINE_SYNC_VAL(util_prime, util_other, mpct_other);
			walt_load->nl = PIPELINE_SYNC_VAL(max_nl_prime, max_nl_other, mpct_other);
			walt_load->pl = PIPELINE_SYNC_VAL(max_pl_prime, max_pl_other, mpct_other);
			trace_sched_load_sync_settings(cpu, util_other, util_prime, mpct_other);
		} else {
			util = PIPELINE_SYNC_VAL(util_other, util_prime, mpct_prime);
			walt_load->nl = PIPELINE_SYNC_VAL(max_nl_other, max_nl_prime, mpct_prime);
			walt_load->pl = PIPELINE_SYNC_VAL(max_pl_other, max_pl_prime, mpct_prime);
			trace_sched_load_sync_settings(cpu, util_other, util_prime, mpct_prime);
		}
	}

	if (!cpumask_test_cpu(cpu, &asym_cap_sibling_cpus))
		goto finish;

	if (is_state1())
		goto finish;

	for_each_cpu(i, &asym_cap_sibling_cpus) {
		if (i != cpu) {
			util_other = max(util_other,
					__cpu_util_freq_walt(i, &wl_other, reason, false));
			max_nl_other = max(max_nl_other, wl_other.nl);
			max_pl_other = max(max_pl_other, wl_other.pl);
		}
	}

	util = max(util, util_other);
	walt_load->nl = max(walt_load->nl, max_nl_other);
	walt_load->pl = max(walt_load->pl, max_pl_other);
finish:
	return (util >= capacity) ? capacity : util;
}

/*
 * In this function we match the accumulated subtractions with the current
 * and previous windows we are operating with. Ignore any entries where
 * the window start in the load_subtraction struct does not match either
 * the current or the previous window. This could happen whenever CPUs
 * become idle or busy with interrupts disabled for an extended period.
 */
static inline void account_load_subtractions(struct rq *rq)
{
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));
	u64 ws = wrq->window_start;
	u64 prev_ws = ws - wrq->prev_window_size;
	struct load_subtractions *ls = wrq->load_subs;
	int i;

	for (i = 0; i < NUM_TRACKED_WINDOWS; i++) {
		if (ls[i].window_start == ws) {
			wrq->curr_runnable_sum -= ls[i].subs;
			wrq->nt_curr_runnable_sum -= ls[i].new_subs;
		} else if (ls[i].window_start == prev_ws) {
			wrq->prev_runnable_sum -= ls[i].subs;
			wrq->nt_prev_runnable_sum -= ls[i].new_subs;
		}

		ls[i].subs = 0;
		ls[i].new_subs = 0;
	}

	if ((s64)wrq->prev_runnable_sum < 0) {
		WALT_BUG(WALT_BUG_WALT, NULL, "wrq->prev_runnable_sum=%llu < 0",
				(s64)wrq->prev_runnable_sum);
		wrq->prev_runnable_sum = 0;
	}
	if ((s64)wrq->curr_runnable_sum < 0) {
		WALT_BUG(WALT_BUG_WALT, NULL, "wrq->curr_runnable_sum=%llu < 0",
				(s64)wrq->curr_runnable_sum);
		wrq->curr_runnable_sum = 0;
	}
	if ((s64)wrq->nt_prev_runnable_sum < 0) {
		WALT_BUG(WALT_BUG_WALT, NULL, "wrq->nt_prev_runnable_sum=%llu < 0",
				(s64)wrq->nt_prev_runnable_sum);
		wrq->nt_prev_runnable_sum = 0;
	}
	if ((s64)wrq->nt_curr_runnable_sum < 0) {
		WALT_BUG(WALT_BUG_WALT, NULL, "wrq->nt_curr_runnable_sum=%llu < 0",
				(s64)wrq->nt_curr_runnable_sum);
		wrq->nt_curr_runnable_sum = 0;
	}
}

static inline void create_subtraction_entry(struct rq *rq, u64 ws, int index)
{
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));

	wrq->load_subs[index].window_start = ws;
	wrq->load_subs[index].subs = 0;
	wrq->load_subs[index].new_subs = 0;
}

static int get_top_index(unsigned long *bitmap, unsigned long old_top)
{
	int index = find_next_bit(bitmap, NUM_LOAD_INDICES, old_top);

	if (index == NUM_LOAD_INDICES)
		return 0;

	return NUM_LOAD_INDICES - 1 - index;
}

static bool get_subtraction_index(struct rq *rq, u64 ws)
{
	int i;
	u64 oldest = ULLONG_MAX;
	int oldest_index = 0;
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));

	for (i = 0; i < NUM_TRACKED_WINDOWS; i++) {
		u64 entry_ws = wrq->load_subs[i].window_start;

		if (ws == entry_ws)
			return i;

		if (entry_ws < oldest) {
			oldest = entry_ws;
			oldest_index = i;
		}
	}

	create_subtraction_entry(rq, ws, oldest_index);
	return oldest_index;
}

static void update_rq_load_subtractions(int index, struct rq *rq,
					u32 sub_load, bool new_task)
{
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));

	wrq->load_subs[index].subs += sub_load;
	if (new_task)
		wrq->load_subs[index].new_subs += sub_load;
}

static void update_cluster_load_subtractions(struct task_struct *p,
					int cpu, u64 ws, bool new_task)
{
	struct walt_sched_cluster *cluster = cpu_cluster(cpu);
	struct cpumask cluster_cpus = cluster->cpus;
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu);
	u64 prev_ws = ws - wrq->prev_window_size;
	int i;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	cpumask_clear_cpu(cpu, &cluster_cpus);
	raw_spin_lock(&cluster->load_lock);

	for_each_cpu(i, &cluster_cpus) {
		struct rq *rq = cpu_rq(i);
		int index;

		if (wts->curr_window_cpu[i]) {
			index = get_subtraction_index(rq, ws);
			update_rq_load_subtractions(index, rq,
				wts->curr_window_cpu[i], new_task);
			wts->curr_window_cpu[i] = 0;
		}

		if (wts->prev_window_cpu[i]) {
			index = get_subtraction_index(rq, prev_ws);
			update_rq_load_subtractions(index, rq,
				wts->prev_window_cpu[i], new_task);
			wts->prev_window_cpu[i] = 0;
		}
	}

	raw_spin_unlock(&cluster->load_lock);
}

static inline void migrate_inter_cluster_subtraction(struct task_struct *p, int task_cpu,
			bool new_task)
{
	struct rq *src_rq = cpu_rq(task_cpu);
	struct walt_rq *src_wrq = &per_cpu(walt_rq, task_cpu);
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	if (src_wrq->curr_runnable_sum < wts->curr_window_cpu[task_cpu]) {
		WALT_BUG(WALT_BUG_WALT, p,
			 "pid=%u CPU%d src_crs=%llu is lesser than task_contrib=%u",
			 p->pid, src_rq->cpu,
			 src_wrq->curr_runnable_sum,
			 wts->curr_window_cpu[task_cpu]);
		src_wrq->curr_runnable_sum = wts->curr_window_cpu[task_cpu];
	}
	src_wrq->curr_runnable_sum -= wts->curr_window_cpu[task_cpu];

	if (src_wrq->prev_runnable_sum < wts->prev_window_cpu[task_cpu]) {
		WALT_BUG(WALT_BUG_WALT, p,
			 "pid=%u CPU%d src_prs=%llu is lesser than task_contrib=%u",
			 p->pid, src_rq->cpu,
			 src_wrq->prev_runnable_sum,
			 wts->prev_window_cpu[task_cpu]);
		 src_wrq->prev_runnable_sum = wts->prev_window_cpu[task_cpu];
	}
	src_wrq->prev_runnable_sum -= wts->prev_window_cpu[task_cpu];

	if (new_task) {
		if (src_wrq->nt_curr_runnable_sum < wts->curr_window_cpu[task_cpu]) {
			WALT_BUG(WALT_BUG_WALT, p,
				 "pid=%u CPU%d src_nt_crs=%llu is lesser than task_contrib=%u",
				 p->pid, src_rq->cpu,
				 src_wrq->nt_curr_runnable_sum,
				 wts->curr_window_cpu[task_cpu]);
			src_wrq->nt_curr_runnable_sum = wts->curr_window_cpu[task_cpu];
		}
		src_wrq->nt_curr_runnable_sum -=
				wts->curr_window_cpu[task_cpu];

		if (src_wrq->nt_prev_runnable_sum < wts->prev_window_cpu[task_cpu]) {
			WALT_BUG(WALT_BUG_WALT, p,
				 "pid=%u CPU%d src_nt_prs=%llu is lesser than task_contrib=%u",
				 p->pid, src_rq->cpu,
				 src_wrq->nt_prev_runnable_sum,
				 wts->prev_window_cpu[task_cpu]);
			src_wrq->nt_prev_runnable_sum = wts->prev_window_cpu[task_cpu];
		}
		src_wrq->nt_prev_runnable_sum -=
				wts->prev_window_cpu[task_cpu];
	}

	wts->curr_window_cpu[task_cpu] = 0;
	wts->prev_window_cpu[task_cpu] = 0;

	update_cluster_load_subtractions(p, task_cpu,
			src_wrq->window_start, new_task);
}

static inline void migrate_inter_cluster_addition(struct task_struct *p, int new_cpu,
			bool new_task)
{
	struct walt_rq *dest_wrq = &per_cpu(walt_rq, new_cpu);
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;


	wts->curr_window_cpu[new_cpu] = wts->curr_window;
	wts->prev_window_cpu[new_cpu] = wts->prev_window;

	dest_wrq->curr_runnable_sum += wts->curr_window;
	dest_wrq->prev_runnable_sum += wts->prev_window;

	if (new_task) {
		dest_wrq->nt_curr_runnable_sum += wts->curr_window;
		dest_wrq->nt_prev_runnable_sum += wts->prev_window;
	}
}

static u32 load_to_index(u32 load)
{
	u32 index = load / sched_load_granule;

	return min(index, (u32)(NUM_LOAD_INDICES - 1));
}

static void migrate_top_tasks_subtraction(struct task_struct *p, struct rq *src_rq)
{
	int index;
	int top_index;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	u32 curr_window = wts->curr_window;
	u32 prev_window = wts->prev_window;
	struct walt_rq *src_wrq = &per_cpu(walt_rq, cpu_of(src_rq));
	u8 src = src_wrq->curr_table;
	u8 *src_table;

	if (curr_window) {
		src_table = src_wrq->top_tasks[src];
		index = load_to_index(curr_window);
		src_table[index] -= 1;

		if (!src_table[index])
			__clear_bit(NUM_LOAD_INDICES - index - 1,
				src_wrq->top_tasks_bitmap[src]);

		top_index = src_wrq->curr_top;
		if (index == top_index && !src_table[index])
			src_wrq->curr_top = get_top_index(
				src_wrq->top_tasks_bitmap[src], top_index);
	}

	if (prev_window) {
		src = 1 - src;
		src_table = src_wrq->top_tasks[src];
		index = load_to_index(prev_window);
		src_table[index] -= 1;

		if (!src_table[index])
			__clear_bit(NUM_LOAD_INDICES - index - 1,
				src_wrq->top_tasks_bitmap[src]);

		top_index = src_wrq->prev_top;
		if (index == top_index && !src_table[index])
			src_wrq->prev_top = get_top_index(
				src_wrq->top_tasks_bitmap[src], top_index);
	}
}

static void migrate_top_tasks_addition(struct task_struct *p, struct rq *rq)
{
	int index;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	u32 curr_window = wts->curr_window;
	u32 prev_window = wts->prev_window;
	struct walt_rq *dst_wrq = &per_cpu(walt_rq, cpu_of(rq));
	u8 dst = dst_wrq->curr_table;
	u8 *dst_table;

	if (curr_window) {
		dst_table = dst_wrq->top_tasks[dst];
		index = load_to_index(curr_window);
		dst_table[index] += 1;

		if (dst_table[index] == 1)
			__set_bit(NUM_LOAD_INDICES - index - 1,
				dst_wrq->top_tasks_bitmap[dst]);

		if (index > dst_wrq->curr_top)
			dst_wrq->curr_top = index;
	}

	if (prev_window) {
		dst = 1 - dst;
		dst_table = dst_wrq->top_tasks[dst];
		index = load_to_index(prev_window);
		dst_table[index] += 1;

		if (dst_table[index] == 1)
			__set_bit(NUM_LOAD_INDICES - index - 1,
				dst_wrq->top_tasks_bitmap[dst]);

		if (index > dst_wrq->prev_top)
			dst_wrq->prev_top = index;
	}
}

// MIUI MOD: Performance_TurboSched
#ifdef CONFIG_MIGT_WALT
bool is_new_task(struct task_struct *p)
#else
static inline bool is_new_task(struct task_struct *p)
#endif
// END Performance_TurboSched
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	return wts->active_time < NEW_TASK_ACTIVE_TIME;
}
// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_MIGT_WALT
EXPORT_SYMBOL_GPL(is_new_task);
#endif
// END Performance_TurboSched

static inline int run_walt_irq_work_rollover(u64 old_window_start, struct rq *rq);

static void migrate_busy_time_subtraction(struct task_struct *p, int new_cpu)
{
	struct rq *src_rq = task_rq(p);
	u64 wallclock;
	u64 *src_curr_runnable_sum, *src_prev_runnable_sum;
	u64 *src_nt_curr_runnable_sum, *src_nt_prev_runnable_sum;
	bool new_task;
	struct walt_related_thread_group *grp;
	long pstate;
	struct walt_rq *src_wrq = &per_cpu(walt_rq, cpu_of(src_rq));
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	if (!p->on_rq && READ_ONCE(p->__state) != TASK_WAKING)
		return;

	pstate = READ_ONCE(p->__state);

	if (pstate == TASK_WAKING)
		raw_spin_rq_lock(src_rq);

	walt_lockdep_assert_rq(src_rq, p);

	if (task_rq(p) != src_rq)
		WALT_BUG(WALT_BUG_UPSTREAM, p, "on CPU %d task %s(%d) not on src_rq %d",
				raw_smp_processor_id(), p->comm, p->pid, src_rq->cpu);

	wts->new_cpu = new_cpu;

	if (!same_freq_domain(task_cpu(p), new_cpu))
		wts->enqueue_after_migration = 2; /* 2 is intercluster */
	else
		wts->enqueue_after_migration = 1; /* 1 is within cluster */

	wallclock = walt_sched_clock();
	walt_update_task_ravg(p, task_rq(p), TASK_MIGRATE, wallclock, 0);

	/*
	 * Update task's cycles wrt to the new cpu.
	 */
	wts->cpu_cycles = walt_get_cycle_counts_cb(new_cpu, wallclock);

	if (wts->window_start != src_wrq->window_start)
		WALT_BUG(WALT_BUG_WALT, p,
				"CPU%d: %s task %s(%d)'s ws=%llu not equal to src_rq %d's ws=%llu",
				raw_smp_processor_id(), __func__, p->comm, p->pid,
				wts->window_start, src_rq->cpu, src_wrq->window_start);

	new_task = is_new_task(p);
	/* Protected by rq_lock */
	grp = wts->grp;

	/*
	 * For frequency aggregation, we continue to do migration fixups
	 * even for intra cluster migrations. This is because, the aggregated
	 * load has to reported on a single CPU regardless.
	 */
	if (grp) {
		struct group_cpu_time *cpu_time;

		if (grp != related_thread_groups[DEFAULT_CGROUP_COLOC_ID])
			WALT_BUG(WALT_BUG_WALT, p,
				"CPU%d: %s task %s(%d)'s grp=%p not equal to rtg[1]=%p'",
				raw_smp_processor_id(), __func__, p->comm, p->pid,
				grp, related_thread_groups[DEFAULT_CGROUP_COLOC_ID]);

		cpu_time = &src_wrq->grp_time;
		src_curr_runnable_sum = &cpu_time->curr_runnable_sum;
		src_prev_runnable_sum = &cpu_time->prev_runnable_sum;
		src_nt_curr_runnable_sum = &cpu_time->nt_curr_runnable_sum;
		src_nt_prev_runnable_sum = &cpu_time->nt_prev_runnable_sum;

		if (wts->curr_window) {
			*src_curr_runnable_sum -= wts->curr_window;
			if (new_task)
				*src_nt_curr_runnable_sum -= wts->curr_window;
		}

		if (wts->prev_window) {
			*src_prev_runnable_sum -= wts->prev_window;
			if (new_task)
				*src_nt_prev_runnable_sum -= wts->prev_window;
		}
	} else {
		if (wts->enqueue_after_migration == 2)
			migrate_inter_cluster_subtraction(p, task_cpu(p), new_task);
	}

	migrate_top_tasks_subtraction(p, src_rq);

	if (is_ed_enabled() && (p == src_wrq->ed_task))
		src_wrq->ed_task = NULL;

	wts->prev_cpu = task_cpu(p);

	if (pstate == TASK_WAKING)
		raw_spin_rq_unlock(src_rq);
}

static void migrate_busy_time_addition(struct task_struct *p, int new_cpu, u64 wallclock)
{
	struct rq *dest_rq = cpu_rq(new_cpu);
	u64 *dst_curr_runnable_sum, *dst_prev_runnable_sum;
	u64 *dst_nt_curr_runnable_sum, *dst_nt_prev_runnable_sum;
	bool new_task;
	struct walt_related_thread_group *grp;
	struct walt_rq *dest_wrq = &per_cpu(walt_rq, new_cpu);
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	int src_cpu = wts->prev_cpu;
	struct walt_rq *src_wrq = &per_cpu(walt_rq, src_cpu);

	walt_lockdep_assert_rq(dest_rq, p);

	walt_update_task_ravg(p, dest_rq, TASK_UPDATE, wallclock, 0);

	if (wts->window_start != dest_wrq->window_start)
		WALT_BUG(WALT_BUG_WALT, p,
				"CPU%d: %s task %s(%d)'s ws=%llu not equal to dest_rq %d's ws=%llu",
				raw_smp_processor_id(), __func__, p->comm, p->pid,
				wts->window_start, dest_rq->cpu, dest_wrq->window_start);

	new_task = is_new_task(p);
	/* Protected by rq_lock */
	grp = wts->grp;

	/*
	 * For frequency aggregation, we continue to do migration fixups
	 * even for intra cluster migrations. This is because, the aggregated
	 * load has to reported on a single CPU regardless.
	 */
	if (grp) {
		struct group_cpu_time *cpu_time;

		if (grp != related_thread_groups[DEFAULT_CGROUP_COLOC_ID])
			WALT_BUG(WALT_BUG_WALT, p,
				"CPU%d: %s task %s(%d)'s grp=%p not equal to rtg[1]=%p'",
				raw_smp_processor_id(), __func__, p->comm, p->pid,
				grp, related_thread_groups[DEFAULT_CGROUP_COLOC_ID]);

		cpu_time = &dest_wrq->grp_time;
		dst_curr_runnable_sum = &cpu_time->curr_runnable_sum;
		dst_prev_runnable_sum = &cpu_time->prev_runnable_sum;
		dst_nt_curr_runnable_sum = &cpu_time->nt_curr_runnable_sum;
		dst_nt_prev_runnable_sum = &cpu_time->nt_prev_runnable_sum;

		if (wts->curr_window) {
			*dst_curr_runnable_sum += wts->curr_window;
			if (new_task)
				*dst_nt_curr_runnable_sum += wts->curr_window;
		}

		if (wts->prev_window) {
			*dst_prev_runnable_sum += wts->prev_window;
			if (new_task)
				*dst_nt_prev_runnable_sum += wts->prev_window;
		}
	} else {
		if (wts->enqueue_after_migration == 2)
			migrate_inter_cluster_addition(p, new_cpu, new_task);
	}

	migrate_top_tasks_addition(p, dest_rq);

	if (wts->enqueue_after_migration == 2) {
		src_wrq->notif_pending = true;
		dest_wrq->notif_pending = true;
		walt_irq_work_queue(&walt_migration_irq_work);
	}

	if (is_ed_enabled() && is_ed_task(p, wallclock))
		dest_wrq->ed_task = p;

	wts->new_cpu = -1;
}

#define INC_STEP 8
#define DEC_STEP 2
#define CONSISTENT_THRES 16
#define INC_STEP_BIG 16
/*
 * bucket_increase - update the count of all buckets
 *
 * @buckets: array of buckets tracking busy time of a task
 * @idx: the index of bucket to be incremented
 *
 * Each time a complete window finishes, count of bucket that runtime
 * falls in (@idx) is incremented. Counts of all other buckets are
 * decayed. The rate of increase and decay could be different based
 * on current count in the bucket.
 */
static inline void bucket_increase(u8 *buckets, u16 *bucket_bitmask, int idx)
{
	int i, step;

	for (i = 0; i < NUM_BUSY_BUCKETS; i++) {
		if (idx != i) {
			if (buckets[i] > DEC_STEP)
				buckets[i] -= DEC_STEP;
			else {
				buckets[i] = 0;
				*bucket_bitmask &= ~BIT_MASK(i);
			}
		} else {
			step = buckets[i] >= CONSISTENT_THRES ?
						INC_STEP_BIG : INC_STEP;
			if (buckets[i] > U8_MAX - step)
				buckets[i] = U8_MAX;
			else
				buckets[i] += step;
			*bucket_bitmask |= BIT_MASK(i);
		}
	}
}

static inline int busy_to_bucket(u16 normalized_rt)
{
	int bidx;

	bidx = normalized_rt >> (SCHED_CAPACITY_SHIFT - NUM_BUSY_BUCKETS_SHIFT);
	bidx = min(bidx, NUM_BUSY_BUCKETS - 1);

	/*
	 * Combine lowest two buckets. The lowest frequency falls into
	 * 2nd bucket and thus keep predicting lowest bucket is not
	 * useful.
	 */
	if (!bidx)
		bidx++;

	return bidx;
}

/*
 * get_pred_busy - calculate predicted demand for a task on runqueue
 *
 * @p: task whose prediction is being updated
 * @start: starting bucket. returned prediction should not be lower than
 *         this bucket.
 * @runtime: runtime of the task. returned prediction should not be lower
 *           than this runtime.
 * Note: @start can be derived from @runtime. It's passed in only to
 * avoid duplicated calculation in some cases.
 *
 * A new predicted busy time is returned for task @p based on @runtime
 * passed in. The function searches through buckets that represent busy
 * time equal to or bigger than @runtime and attempts to find the bucket
 * to use for prediction. Once found, it searches through historical busy
 * time and returns the latest that falls into the bucket. If no such busy
 * time exists, it returns the medium of that bucket.
 */
static u32 get_pred_busy(struct task_struct *p,
				int start, u16 runtime_scaled, u16 bucket_bitmask)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	u16 dmin, dmax;
	int first = NUM_BUSY_BUCKETS, final = NUM_BUSY_BUCKETS;
	u16 ret = runtime_scaled;
	u16 next_mask = bucket_bitmask >> start;
	u16 *hist_util = wts->sum_history_util;
	int i;

	/* skip prediction for new tasks due to lack of history */
	if (unlikely(is_new_task(p)))
		goto out;

	/* find minimal bucket index to pick */
	if (next_mask)
		first = ffs(next_mask) - 1 + start;

	/* if no higher buckets are filled, predict runtime */
	if (first >= NUM_BUSY_BUCKETS)
		goto out;

	/* compute the bucket for prediction */
	final = first;

	/* determine demand range for the predicted bucket */
	if (final < 2) {
		/* lowest two buckets are combined */
		dmin = 0;
		final = 1;
	} else {
		dmin = final << (SCHED_CAPACITY_SHIFT - NUM_BUSY_BUCKETS_SHIFT);
	}
	dmax = (final + 1) << (SCHED_CAPACITY_SHIFT - NUM_BUSY_BUCKETS_SHIFT);

	/*
	 * search through runtime history and return first runtime that falls
	 * into the range of predicted bucket.
	 */
	for (i = 0; i < RAVG_HIST_SIZE; i++) {
		if (hist_util[i] >= dmin && hist_util[i] < dmax) {
			ret = hist_util[i];
			break;
		}
	}
	/* no historical runtime within bucket found, use average of the bin */
	if (ret < dmin)
		ret = (u16) (((u32)dmin + dmax) / 2);
	/*
	 * when updating in middle of a window, runtime could be higher
	 * than all recorded history. Always predict at least runtime.
	 */
	ret = max(runtime_scaled, ret);
out:
	trace_sched_update_pred_demand(p, runtime_scaled,
		ret, start, first, final, wts);
	return ret;
}

/*
 * predictive demand of a task was calculated at the last window roll-over.
 * if the task current window busy time exceeds the predicted
 * demand, update it here to reflect the task needs.
 */
static void update_task_pred_demand(struct rq *rq, struct task_struct *p, int event)
{
	u16 new_pred_demand_scaled;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	u16 curr_window_scaled;

	if (walt_is_idle_task(p))
		return;

	if (event != PUT_PREV_TASK && event != TASK_UPDATE &&
			(!SCHED_FREQ_ACCOUNT_WAIT_TIME ||
			 (event != TASK_MIGRATE &&
			 event != PICK_NEXT_TASK)))
		return;

	/*
	 * TASK_UPDATE can be called on sleeping task, when its moved between
	 * related groups
	 */
	if (event == TASK_UPDATE) {
		if (!p->on_rq && !SCHED_FREQ_ACCOUNT_WAIT_TIME)
			return;
	}

	curr_window_scaled = scale_time_to_util(wts->curr_window);
	if (wts->pred_demand_scaled >= curr_window_scaled)
		return;

	new_pred_demand_scaled = get_pred_busy(p, busy_to_bucket(curr_window_scaled),
			     curr_window_scaled, wts->bucket_bitmask);

	if (task_on_rq_queued(p))
		fixup_walt_sched_stats_common(rq, p,
				wts->demand_scaled,
				new_pred_demand_scaled);

	wts->pred_demand_scaled = new_pred_demand_scaled;
}

static void clear_top_tasks_bitmap(unsigned long *bitmap)
{
	memset(bitmap, 0, top_tasks_bitmap_size);
	__set_bit(NUM_LOAD_INDICES, bitmap);
}

static inline void clear_top_tasks_table(u8 *table)
{
	memset(table, 0, NUM_LOAD_INDICES * sizeof(u8));
}

static void update_top_tasks(struct task_struct *p, struct rq *rq,
		u32 old_curr_window, int new_window, bool full_window)
{
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	u8 curr = wrq->curr_table;
	u8 prev = 1 - curr;
	u8 *curr_table = wrq->top_tasks[curr];
	u8 *prev_table = wrq->top_tasks[prev];
	int old_index, new_index, update_index;
	u32 curr_window = wts->curr_window;
	u32 prev_window = wts->prev_window;
	bool zero_index_update;

	if (old_curr_window == curr_window && !new_window)
		return;

	old_index = load_to_index(old_curr_window);
	new_index = load_to_index(curr_window);

	if (!new_window) {
		zero_index_update = !old_curr_window && curr_window;
		if (old_index != new_index || zero_index_update) {
			if (old_curr_window)
				curr_table[old_index] -= 1;
			if (curr_window)
				curr_table[new_index] += 1;
			if (new_index > wrq->curr_top)
				wrq->curr_top = new_index;
		}

		if (!curr_table[old_index])
			__clear_bit(NUM_LOAD_INDICES - old_index - 1,
				wrq->top_tasks_bitmap[curr]);

		if (curr_table[new_index] == 1)
			__set_bit(NUM_LOAD_INDICES - new_index - 1,
				wrq->top_tasks_bitmap[curr]);

		return;
	}

	/*
	 * The window has rolled over for this task. By the time we get
	 * here, curr/prev swaps would has already occurred. So we need
	 * to use prev_window for the new index.
	 */
	update_index = load_to_index(prev_window);

	if (full_window) {
		/*
		 * Two cases here. Either 'p' ran for the entire window or
		 * it didn't run at all. In either case there is no entry
		 * in the prev table. If 'p' ran the entire window, we just
		 * need to create a new entry in the prev table. In this case
		 * update_index will be correspond to sched_ravg_window
		 * so we can unconditionally update the top index.
		 */
		if (prev_window) {
			prev_table[update_index] += 1;
			wrq->prev_top = update_index;
		}

		if (prev_table[update_index] == 1)
			__set_bit(NUM_LOAD_INDICES - update_index - 1,
				wrq->top_tasks_bitmap[prev]);
	} else {
		zero_index_update = !old_curr_window && prev_window;
		if (old_index != update_index || zero_index_update) {
			if (old_curr_window)
				prev_table[old_index] -= 1;

			prev_table[update_index] += 1;

			if (update_index > wrq->prev_top)
				wrq->prev_top = update_index;

			if (!prev_table[old_index])
				__clear_bit(NUM_LOAD_INDICES - old_index - 1,
						wrq->top_tasks_bitmap[prev]);

			if (prev_table[update_index] == 1)
				__set_bit(NUM_LOAD_INDICES - update_index - 1,
						wrq->top_tasks_bitmap[prev]);
		}
	}

	if (curr_window) {
		curr_table[new_index] += 1;

		if (new_index > wrq->curr_top)
			wrq->curr_top = new_index;

		if (curr_table[new_index] == 1)
			__set_bit(NUM_LOAD_INDICES - new_index - 1,
				wrq->top_tasks_bitmap[curr]);
	}
}

static void rollover_top_tasks(struct rq *rq, bool full_window)
{
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));
	u8 curr_table = wrq->curr_table;
	u8 prev_table = 1 - curr_table;
	int curr_top = wrq->curr_top;

	clear_top_tasks_table(wrq->top_tasks[prev_table]);
	clear_top_tasks_bitmap(wrq->top_tasks_bitmap[prev_table]);

	if (full_window) {
		curr_top = 0;
		clear_top_tasks_table(wrq->top_tasks[curr_table]);
		clear_top_tasks_bitmap(wrq->top_tasks_bitmap[curr_table]);
	}

	wrq->curr_table = prev_table;
	wrq->prev_top = curr_top;
	wrq->curr_top = 0;
}

static u32 empty_windows[WALT_NR_CPUS];

static void rollover_task_window(struct task_struct *p, bool full_window)
{
	u32 *curr_cpu_windows = empty_windows;
	u32 curr_window;
	int i;
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(task_rq(p)));
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	/* Rollover the sum */
	curr_window = 0;

	if (!full_window) {
		curr_window = wts->curr_window;
		curr_cpu_windows = wts->curr_window_cpu;
	}

	wts->prev_window = curr_window;
	wts->curr_window = 0;

	/* Roll over individual CPU contributions */
	for (i = 0; i < nr_cpu_ids; i++) {
		wts->prev_window_cpu[i] = curr_cpu_windows[i];
		wts->curr_window_cpu[i] = 0;
	}

	if (is_new_task(p))
		wts->active_time += wrq->prev_window_size;
}

// MIUI MOD: Performance_TurboSched
#ifdef CONFIG_MIGT_WALT
int cpu_is_waiting_on_io(struct rq *rq)
#else
static inline int cpu_is_waiting_on_io(struct rq *rq)
#endif
// END Performance_TurboSched
{
	if (!sched_io_is_busy)
		return 0;

	return atomic_read(&rq->nr_iowait);
}
// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_MIGT_WALT
EXPORT_SYMBOL_GPL(cpu_is_waiting_on_io);
#endif
// END Performance_TurboSched

static int account_busy_for_cpu_time(struct rq *rq, struct task_struct *p,
				     u64 irqtime, int event)
{
	if (walt_is_idle_task(p)) {
		/* TASK_WAKE && TASK_MIGRATE is not possible on idle task! */
		if (event == PICK_NEXT_TASK)
			return 0;

		/* PUT_PREV_TASK, TASK_UPDATE && IRQ_UPDATE are left */
		return irqtime || cpu_is_waiting_on_io(rq);
	}

	if (event == TASK_WAKE)
		return 0;

	if (event == PUT_PREV_TASK || event == IRQ_UPDATE)
		return 1;

	/*
	 * TASK_UPDATE can be called on sleeping task, when its moved between
	 * related groups
	 */
	if (event == TASK_UPDATE) {
		if (rq->curr == p)
			return 1;

		return p->on_rq ? SCHED_FREQ_ACCOUNT_WAIT_TIME : 0;
	}

	/* TASK_MIGRATE, PICK_NEXT_TASK left */
	return SCHED_FREQ_ACCOUNT_WAIT_TIME;
}

#define DIV64_U64_ROUNDUP(X, Y) div64_u64((X) + (Y - 1), Y)

static inline u64 scale_exec_time(u64 delta, struct rq *rq, struct walt_task_struct *wts)
{
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));

	delta = (delta * wrq->task_exec_scale) >> SCHED_CAPACITY_SHIFT;

	if (wts->load_boost && wts->grp)
		delta = (delta * (1024 + wts->boosted_task_load) >> 10);

	return delta;
}

// MIUI ADD: Game_TurboSched
#ifdef CONFIG_MIGT_WALT
u64 get_scale_exec_time(u64 delta, int cpu, struct walt_task_struct *wts)
{
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu);

	delta = (delta * wrq->task_exec_scale) >> SCHED_CAPACITY_SHIFT;
	//del rtg boost influence
	return delta;
}
EXPORT_SYMBOL_GPL(get_scale_exec_time);
#endif
// END Game_TurboSched

/* Convert busy time to frequency equivalent
 * Assumes load is scaled to 1024
 */
static inline unsigned int load_to_freq(struct rq *rq, unsigned int load)
{
	return mult_frac(cpu_max_possible_freq(cpu_of(rq)), load,
		 (unsigned int)arch_scale_cpu_capacity(cpu_of(rq)));
}

static bool do_pl_notif(struct rq *rq)
{
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));
	u64 prev = wrq->old_busy_time;
	u64 pl = wrq->walt_stats.pred_demands_sum_scaled;
	int cpu = cpu_of(rq);

	/* If already at max freq, bail out */
	if (capacity_orig_of(cpu) == capacity_curr_of(cpu))
		return false;

	prev = max(prev, wrq->old_estimated_time);

	/* 400 MHz filter. */
	return (pl > prev) && (load_to_freq(rq, pl - prev) > 400000);
}

static void rollover_cpu_window(struct rq *rq, bool full_window)
{
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));
	u64 curr_sum = wrq->curr_runnable_sum;
	u64 nt_curr_sum = wrq->nt_curr_runnable_sum;
	u64 grp_curr_sum = wrq->grp_time.curr_runnable_sum;
	u64 grp_nt_curr_sum = wrq->grp_time.nt_curr_runnable_sum;

	if (unlikely(full_window)) {
		curr_sum = 0;
		nt_curr_sum = 0;
		grp_curr_sum = 0;
		grp_nt_curr_sum = 0;
	}

	wrq->prev_runnable_sum = curr_sum;
	wrq->nt_prev_runnable_sum = nt_curr_sum;
	wrq->grp_time.prev_runnable_sum = grp_curr_sum;
	wrq->grp_time.nt_prev_runnable_sum = grp_nt_curr_sum;

	wrq->curr_runnable_sum = 0;
	wrq->nt_curr_runnable_sum = 0;
	wrq->grp_time.curr_runnable_sum = 0;
	wrq->grp_time.nt_curr_runnable_sum = 0;
}

/*
 * Account cpu activity in its
 * busy time counters(wrq->curr/prev_runnable_sum)
 *
 * While the comments at the top of update_task_demand() apply, irqtime handling
 * needs some explanation.
 *
 * Note that update_task_ravg() with irqtime is only called when idle, i.e. p is
 * always idle
 *
 * ms_i = mark_start of idle task
 * ws = wrq->window_start
 * irq_s = start time of irq
 * irq_e = end time of irq = wallclock
 *
 * note irqtime = irq_e - irq_s
 *
 * Similar to the explanation at update_task_demand() we have few situations for irqtime
 *
 *              ws   ms_i   is    ie
 *              |    |      |      |
 *              V    V      V      V
 *      --------|--------------------|
 *          prev    curr
 *
 * In the above case, new_window is false and irqtime is accounted in curr_runnable_sum, this is
 * done in the if (!new_window) block.
 *
 *             ms_i  ws     is    ie
 *              |    |      |      |
 *              V    V      V      V
 *      -------------|---------------------
 *               prev   curr
 *
 * In this case, new_window is true, however the irqtime falls within the current window, the
 * entire irqtime is accounted in curr_runnable_sum. This is handled in the if (irqtime) block and
 * within that if (mark_start > window_start) block
 *
 *             ms_i  is     ws    ie
 *              |    |      |      |
 *              V    V      V      V
 *      --------------------|---------------
 *                      prev    curr
 *
 * In this case, new_window is true, portion  of the irqtime  needs to be accounted in
 * prev_runnable_sum while the rest is in curr_runnable_sum. This is handled in the
 * if (irqtime) block
 */
static void update_cpu_busy_time(struct task_struct *p, struct rq *rq,
				 int event, u64 wallclock, u64 irqtime)
{
	int new_window, full_window = 0;
	int p_is_curr_task = (p == rq->curr);
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	u64 mark_start = wts->mark_start;
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));
	u64 window_start = wrq->window_start;
	u32 window_size = wrq->prev_window_size;
	u64 delta;
	u64 *curr_runnable_sum = &wrq->curr_runnable_sum;
	u64 *prev_runnable_sum = &wrq->prev_runnable_sum;
	u64 *nt_curr_runnable_sum = &wrq->nt_curr_runnable_sum;
	u64 *nt_prev_runnable_sum = &wrq->nt_prev_runnable_sum;
	bool new_task;
	struct walt_related_thread_group *grp;
	int cpu = rq->cpu;
	u32 old_curr_window = wts->curr_window;

	walt_lockdep_assert_rq(rq, p);

	new_window = mark_start < window_start;
	if (new_window)
		full_window = (window_start - mark_start) >= window_size;

	/*
	 * Handle per-task window rollover. We don't care about the
	 * idle task.
	 */
	if (new_window) {
		if (!walt_is_idle_task(p))
			rollover_task_window(p, full_window);
		wts->window_start = window_start;
	}

	new_task = is_new_task(p);

	if (!account_busy_for_cpu_time(rq, p, irqtime, event))
		goto done;

	grp = wts->grp;
	if (grp) {
		struct group_cpu_time *cpu_time = &wrq->grp_time;

		curr_runnable_sum = &cpu_time->curr_runnable_sum;
		prev_runnable_sum = &cpu_time->prev_runnable_sum;

		nt_curr_runnable_sum = &cpu_time->nt_curr_runnable_sum;
		nt_prev_runnable_sum = &cpu_time->nt_prev_runnable_sum;
	}

	if (wts->window_start != wrq->window_start)
		WALT_BUG(WALT_BUG_WALT, p,
				"CPU%d: %s task %s(%d)'s ws=%llu not equal to rq %d's ws=%llu",
				raw_smp_processor_id(), __func__, p->comm, p->pid,
				wts->window_start, rq->cpu, wrq->window_start);

	if (!new_window) {
		/*
		 * account_busy_for_cpu_time() = 1 so busy time needs
		 * to be accounted to the current window. No rollover
		 * since we didn't start a new window. An example of this is
		 * when a task starts execution and then sleeps within the
		 * same window.
		 */

		if (!irqtime || !walt_is_idle_task(p) || cpu_is_waiting_on_io(rq))
			delta = wallclock - mark_start;
		else
			delta = irqtime;
		delta = scale_exec_time(delta, rq, wts);
		*curr_runnable_sum += delta;
		if (new_task)
			*nt_curr_runnable_sum += delta;

		if (!walt_is_idle_task(p)) {
			wts->curr_window += delta;
			wts->curr_window_cpu[cpu] += delta;
		}

		goto done;
	}

	/*
	 * situations below this need window rollover,
	 * Rollover of cpu counters (curr/prev_runnable_sum) should have already be done
	 * in update_window_start()
	 *
	 * For task counters curr/prev_window[_cpu] are rolled over in the early part of
	 * this function. If full_window(s) have expired and time since last update needs
	 * to be accounted as busy time, set the prev to a complete window size time, else
	 * add the prev window portion.
	 *
	 * For task curr counters a new window has begun, always assign
	 */

	if (!p_is_curr_task) {
		/*
		 * account_busy_for_cpu_time() = 1 so busy time needs
		 * to be accounted to the current window. A new window
		 * must have been started in udpate_window_start()
		 * - just split up and account as necessary into curr and prev.
		 *
		 * Irqtime can't be accounted by a task that isn't the
		 * currently running task.
		 */

		if (!full_window) {
			/*
			 * A full window hasn't elapsed, account partial
			 * contribution to previous completed window.
			 */
			delta = scale_exec_time(window_start - mark_start, rq, wts);
			wts->prev_window += delta;
			wts->prev_window_cpu[cpu] += delta;
		} else {
			/*
			 * Since at least one full window has elapsed,
			 * the contribution to the previous window is the
			 * full window (window_size).
			 */
			delta = scale_exec_time(window_size, rq, wts);
			wts->prev_window = delta;
			wts->prev_window_cpu[cpu] = delta;
		}

		*prev_runnable_sum += delta;
		if (new_task)
			*nt_prev_runnable_sum += delta;

		/* Account piece of busy time in the current window. */
		delta = scale_exec_time(wallclock - window_start, rq, wts);
		*curr_runnable_sum += delta;
		if (new_task)
			*nt_curr_runnable_sum += delta;

		wts->curr_window = delta;
		wts->curr_window_cpu[cpu] = delta;

		goto done;
	}

	if (!irqtime || !walt_is_idle_task(p) || cpu_is_waiting_on_io(rq)) {
		/*
		 * account_busy_for_cpu_time() = 1 so busy time needs
		 * to be accounted to the current window. A new window
		 * must have been started in udpate_window_start()
		 * If any of these three above conditions are true
		 * then this busy time can't be accounted as irqtime.
		 *
		 * Busy time for the idle task need not be accounted.
		 *
		 * An example of this would be a task that starts execution
		 * and then sleeps once a new window has begun.
		 */

		if (!full_window) {
			/*
			 * A full window hasn't elapsed, account partial
			 * contribution to previous completed window.
			 */
			delta = scale_exec_time(window_start - mark_start, rq, wts);
			if (!walt_is_idle_task(p)) {
				wts->prev_window += delta;
				wts->prev_window_cpu[cpu] += delta;
			}
		} else {
			/*
			 * Since at least one full window has elapsed,
			 * the contribution to the previous window is the
			 * full window (window_size).
			 */
			delta = scale_exec_time(window_size, rq, wts);
			if (!walt_is_idle_task(p)) {
				wts->prev_window = delta;
				wts->prev_window_cpu[cpu] = delta;
			}
		}

		*prev_runnable_sum += delta;
		if (new_task)
			*nt_prev_runnable_sum += delta;

		/* Account piece of busy time in the current window. */
		delta = scale_exec_time(wallclock - window_start, rq, wts);
		*curr_runnable_sum += delta;
		if (new_task)
			*nt_curr_runnable_sum += delta;

		if (!walt_is_idle_task(p)) {
			wts->curr_window = delta;
			wts->curr_window_cpu[cpu] = delta;
		}

		goto done;
	}

	if (irqtime) {
		/*
		 * account_busy_for_cpu_time() = 1 so busy time needs
		 * to be accounted to the current window. A new window
		 * must have been started in udpate_window_start()
		 * The current task must be the idle task because
		 * irqtime is not accounted for any other task.
		 *
		 * Irqtime will be accounted each time we process IRQ activity
		 * after a period of idleness, so we know the IRQ busy time
		 * started at wallclock - irqtime.
		 */

		WALT_PANIC(!walt_is_idle_task(p));
		/* mark_start here becomes the starting time of interrupt */
		mark_start = wallclock - irqtime;

		/*
		 * If IRQ busy time was just in the current
		 * window then that is all that need be accounted.
		 */
		if (mark_start > window_start) {
			*curr_runnable_sum += scale_exec_time(irqtime, rq, wts);
			return;
		}

		/*
		 * The IRQ busy time spanned multiple windows. Process the
		 * busy time preceding the current window start first.
		 */
		delta = window_start - mark_start;
		if (delta > window_size)
			delta = window_size;
		delta = scale_exec_time(delta, rq, wts);
		*prev_runnable_sum += delta;

		/* Process the remaining IRQ busy time in the current window. */
		delta = wallclock - window_start;
		wrq->curr_runnable_sum += scale_exec_time(delta, rq, wts);

		return;
	}

done:
	if (!walt_is_idle_task(p))
		update_top_tasks(p, rq, old_curr_window,
					new_window, full_window);
}

static inline u16 predict_and_update_buckets(
			struct task_struct *p, u16 runtime_scaled) {
	int bidx;
	u32 pred_demand_scaled;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	bidx = busy_to_bucket(runtime_scaled);
	pred_demand_scaled = get_pred_busy(p, bidx, runtime_scaled, wts->bucket_bitmask);
	bucket_increase(wts->busy_buckets, &wts->bucket_bitmask, bidx);

	return pred_demand_scaled;
}

static int
account_busy_for_task_demand(struct rq *rq, struct task_struct *p, int event)
{
	/*
	 * No need to bother updating task demand for the idle task.
	 */
	if (walt_is_idle_task(p))
		return 0;

	/*
	 * When a task is waking up it is completing a segment of non-busy
	 * time. Likewise, if wait time is not treated as busy time, then
	 * when a task begins to run or is migrated, it is not running and
	 * is completing a segment of non-busy time.
	 */
	if (event == TASK_WAKE || (!SCHED_ACCOUNT_WAIT_TIME &&
			 (event == PICK_NEXT_TASK || event == TASK_MIGRATE)))
		return 0;

	/*
	 * The idle exit time is not accounted for the first task _picked_ up to
	 * run on the idle CPU.
	 */
	if (event == PICK_NEXT_TASK && rq->curr == rq->idle)
		return 0;

	/*
	 * TASK_UPDATE can be called on sleeping task, when its moved between
	 * related groups
	 */
	if (event == TASK_UPDATE) {
		if (rq->curr == p)
			return 1;

		return p->on_rq ? SCHED_ACCOUNT_WAIT_TIME : 0;
	}

	return 1;
}

#define TRAILBLAZER_THRES 230
#define TRAILBLAZER_BYPASS 243
#define FINAL_BUCKET_STEP_UP 8
#define FINAL_BUCKET_STEP_DOWN 1

static inline u32 scale_util_to_time(u16 util)
{
	return util * walt_scale_demand_divisor;
}

static void update_trailblazer_accounting(struct task_struct *p, struct rq *rq,
		u32 runtime, u16 runtime_scaled, u32 *demand, u16 *trailblazer_demand)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));
	bool is_prev_trailblazer = walt_flag_test(p, WALT_TRAILBLAZER_BIT);
	u64 trailblazer_capacity;

	if (!pipeline_in_progress() && walt_feat(WALT_FEAT_TRAILBLAZER_BIT) &&
			(((runtime >= *demand) && (wts->high_util_history >= TRAILBLAZER_THRES)) ||
			wts->high_util_history >= TRAILBLAZER_BYPASS)) {
		*trailblazer_demand = 1 << SCHED_CAPACITY_SHIFT;
		*demand = scale_util_to_time(*trailblazer_demand);
		walt_flag_set(p, WALT_TRAILBLAZER_BIT, 1);
	} else if (is_prev_trailblazer) {
		walt_flag_set(p, WALT_TRAILBLAZER_BIT, 0);
	}

	/*
	 * In the event that a trailblazer task is detected (or an existing trailblazer task
	 * no longer matches the criteria) and is already enqueued on the cpu, ensure to
	 * close the prod-sum accounts for this task before the next update takes place.
	 */
	if (task_on_rq_queued(p)) {
		if (is_prev_trailblazer != walt_flag_test(p, WALT_TRAILBLAZER_BIT))
			sched_update_nr_prod(rq->cpu, 0);
		if (is_prev_trailblazer && !walt_flag_test(p, WALT_TRAILBLAZER_BIT))
			wrq->walt_stats.nr_trailblazer_tasks--;
		else if (!is_prev_trailblazer && walt_flag_test(p, WALT_TRAILBLAZER_BIT))
			wrq->walt_stats.nr_trailblazer_tasks++;
	}

	/*
	 * The CPU might be running with capped capacities. In order for a runtime to be considered
	 * as trailblazer worthy, it must be 87.5% or more of the prime CPU capacity.
	 */
	trailblazer_capacity =
		capacity_orig_of(cpumask_first(&cpu_array[0][num_sched_clusters - 1]));
	trailblazer_capacity = trailblazer_capacity - (trailblazer_capacity >> 3);
	if (runtime_scaled >= (u16)trailblazer_capacity) {
		if (wts->high_util_history > U8_MAX - FINAL_BUCKET_STEP_UP)
			wts->high_util_history = U8_MAX;
		else
			wts->high_util_history += FINAL_BUCKET_STEP_UP;
	} else if (wts->high_util_history) {
		wts->high_util_history -= FINAL_BUCKET_STEP_DOWN;
	}
}

#define MIN_WINDOWS_FOR_LST	3ULL
#define LST_ACTIVATION_TIMEOUT	250ULL
#define LST_DELAY_MULTIPLIER	5
/*
 * LST detection and timeout flow:
 * - For > 3 windows didn't receive any event thus marked as LST
 * - LST timeout
 * ms                                         ms   ms    ms    ms             ms
 * |        |         |         |         |    |    |     |     |              |
 * |        |         |         |         |    |    |     |     |              |
 * v--------|---------|---------|---------|----v----v-----v-----v--------------v
 *          |         |         |         |   LST(marked here)
 *          |         |         |         |    |                           |
 *       window    window    window    window  |<------------------------->|
 *          1         2         3         4    |  Task in LST state        |
 *								        LST timeout
 */
static void update_lst(struct walt_task_struct *wts, u64 wallclock,
		       int new_window)
{
	u64 lst_delay_factor;
	u64 activity_target_hyst_ns = MIN_WINDOWS_FOR_LST * sched_ravg_window;

	if ((wts->mark_start != 0) && ((wts->mark_start + activity_target_hyst_ns) < wallclock)) {
		/*
		 * task has been inactive for the threshold period treat it as
		 * LST task.
		 */
		wts->lst = true;
		wts->lst_state_counter++;

		/* LST delay calculation based on how frequent task was in LST */
		lst_delay_factor = min((wts->lst_state_counter  + LST_DELAY_MULTIPLIER)  /
				       LST_DELAY_MULTIPLIER, 10);

		/* target time after which task will come out of LST state */
		wts->lst_tgt_ns = wallclock +
			(LST_ACTIVATION_TIMEOUT * MSEC_TO_NSEC * lst_delay_factor);
	} else if (wts->lst_tgt_ns && (wts->lst_tgt_ns < wallclock)) {
		wts->lst = false;
		wts->lst_tgt_ns = 0;
		wts->lst_state_counter = 0;
	}

	/* tracking the number of windows where a task has encountered an event. */
	if (new_window)
		atomic_inc(&wts->event_windows);

}

// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_MI_SCHED_WALT
static int
__account_busy_for_task_demand(struct rq *rq, struct task_struct *p, int event, bool account_wait_time)
{
	/*
	 * No need to bother updating task demand for the idle task.
	 */
	if (is_idle_task(p))
		return 0;

	/*
	 * When a task is waking up it is completing a segment of non-busy
	 * time. Likewise, if wait time is not treated as busy time, then
	 * when a task begins to run or is migrated, it is not running and
	 * is completing a segment of non-busy time.
	 */
	if (event == TASK_WAKE || (!account_wait_time &&
			 (event == PICK_NEXT_TASK || event == TASK_MIGRATE)))
		return 0;

	/*
	 * The idle exit time is not accounted for the first task _picked_ up to
	 * run on the idle CPU.
	 */
	if (event == PICK_NEXT_TASK && rq->curr == rq->idle)
		return 0;

	/*
	 * TASK_UPDATE can be called on sleeping task, when its moved between
	 * related groups
	 */
	if (event == TASK_UPDATE) {
		if (rq->curr == p)
			return 1;

		return p->on_rq ? account_wait_time : 0;
	}

	return 1;
}

static int 
account_pkg_busy_time(struct rq *rq, struct task_struct *p, int event)
{
	if (is_idle_task(p)) {
		if (event == PICK_NEXT_TASK)
			return 0;
		return 1;
        }
        return __account_busy_for_task_demand(rq, p, event, false);
}
#endif
// END Performance_TurboSched

/*
 * Called when new window is starting for a task, to record cpu usage over
 * recently concluded window(s). Normally 'samples' should be 1. It can be > 1
 * when, say, a real-time task runs without preemption for several windows at a
 * stretch.
 */
static void update_history(struct rq *rq, struct task_struct *p,
			 u32 runtime, int samples, int event)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	u32 *hist = &wts->sum_history[0];
	u16 *hist_util = &wts->sum_history_util[0];
	int i;
	u32 max = 0, avg, demand;
	u64 sum = 0;
	u16 demand_scaled, pred_demand_scaled, runtime_scaled;
	u16 trailblazer_demand = 0;
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));

	/* clear yield status of task if there is a change in window */

	if ((wts->yield_state & YIELD_CNT_MASK) < MAX_YIELD_CNT_PER_TASK_THR)
		wts->yield_state = 0;

	/* Ignore windows where task had no activity */
	if (!runtime || walt_is_idle_task(p) || !samples)
		goto done;

	runtime_scaled = scale_time_to_util(runtime);
	/* Push new 'runtime' value onto stack */
	for (; samples > 0; samples--) {
		hist[wts->cidx] = runtime;
		hist_util[wts->cidx] = runtime_scaled;
		wts->cidx++;
		wts->cidx = wts->cidx % RAVG_HIST_SIZE;
	}

	for (i = 0; i < RAVG_HIST_SIZE; i++) {
		sum += hist[i];
		if (hist[i] > max)
			max = hist[i];
	}

	wts->sum = 0;
	avg = div64_u64(sum, RAVG_HIST_SIZE);

	switch (sysctl_sched_window_stats_policy) {
	case WINDOW_STATS_RECENT:
		demand = runtime;
		break;
	case WINDOW_STATS_MAX:
		demand = max;
		break;
	case WINDOW_STATS_AVG:
		demand = avg;
		break;
	default:
		demand = max(avg, runtime);
	}

	if (walt_fair_task(p))
		update_trailblazer_accounting(p, rq, runtime, runtime_scaled,
				&demand, &trailblazer_demand);
	pred_demand_scaled = predict_and_update_buckets(p, runtime_scaled);
	demand_scaled = scale_time_to_util(demand);

	/*
	 * Avoid double accounting of task demand as demand will be updated
	 * to CRA as part of enqueue/dequeue.
	 *
	 * When window is rolled over, the cumulative window demand
	 * is reset to the cumulative runnable average (contribution from
	 * the tasks on the runqueue). If the current task is dequeued
	 * already, it's demand is not included in the cumulative runnable
	 * average. So add the task demand separately to cumulative window
	 * demand.
	 */
	if (task_on_rq_queued(p))
		fixup_walt_sched_stats_common(rq, p,
					      demand_scaled, pred_demand_scaled);

	wts->demand = demand;
	wts->demand_scaled = demand_scaled;
	wts->coloc_demand = avg;
	wts->pred_demand_scaled = pred_demand_scaled;

	if (demand_scaled > sysctl_sched_min_task_util_for_colocation)
		wts->unfilter = sysctl_sched_task_unfilter_period;
	else
		if (wts->unfilter)
			wts->unfilter = max_t(int, 0,
				wts->unfilter - wrq->prev_window_size);
done:
	trace_sched_update_history(rq, p, runtime, samples, event, wrq, wts, trailblazer_demand);
}

static u64 add_to_task_demand(struct rq *rq, struct task_struct *p, u64 delta)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	delta = scale_exec_time(delta, rq, wts);
	wts->sum += delta;
	if (unlikely(wts->sum > sched_ravg_window))
		wts->sum = sched_ravg_window;

	return delta;
}

/*
 * Account cpu demand of task and/or update task's cpu demand history
 *
 * ms = wts->mark_start;
 * wc = wallclock
 * ws = wrq->window_start
 *
 * Three possibilities:
 *
 *	a) Task event is contained within one window.
 *		window_start < mark_start < wallclock
 *
 *		ws   ms  wc
 *		|    |   |
 *		V    V   V
 *		|---------------|
 *
 *	In this case, wts->sum is updated *iff* event is appropriate
 *	(ex: event == PUT_PREV_TASK)
 *
 *	b) Task event spans two windows.
 *		mark_start < window_start < wallclock
 *
 *		ms   ws   wc
 *		|    |    |
 *		V    V    V
 *		-----|-------------------
 *
 *	In this case, wts->sum is updated with (ws - ms) *iff* event
 *	is appropriate, then a new window sample is recorded followed
 *	by wts->sum being set to (wc - ws) *iff* event is appropriate.
 *
 *	c) Task event spans more than two windows.
 *
 *		ms ws_tmp			   ws  wc
 *		|  |				   |   |
 *		V  V				   V   V
 *		---|-------|-------|-------|-------|------
 *		   |				   |
 *		   |<------ nr_full_windows ------>|
 *
 *	In this case, wts->sum is updated with (ws_tmp - ms) first *iff*
 *	event is appropriate, window sample of wts->sum is recorded,
 *	'nr_full_window' samples of window_size is also recorded *iff*
 *	event is appropriate and finally wts->sum is set to (wc - ws)
 *	*iff* event is appropriate.
 *
 * IMPORTANT : Leave wts->mark_start unchanged, as update_cpu_busy_time()
 * depends on it!
 */
static u64 update_task_demand(struct task_struct *p, struct rq *rq,
			       int event, u64 wallclock)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	struct walt_related_thread_group *rtg = wts->grp;
	u64 mark_start = wts->mark_start;
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));
	u64 delta, window_start = wrq->window_start;
	int new_window, nr_full_windows;
	u32 window_size = sched_ravg_window;
	u64 runtime;

	new_window = mark_start < window_start;
	/*
	 * activity count is only used for pipeline filtering
	 * update activity count only if pipleine is in progress.
	 */
	if (pipeline_in_progress() && rtg && rtg->id == DEFAULT_CGROUP_COLOC_ID)
		update_lst(wts, wallclock, new_window);

	if (!account_busy_for_task_demand(rq, p, event)) {
		if (new_window)
			/*
			 * If the time accounted isn't being accounted as
			 * busy time, and a new window started, only the
			 * previous window need be closed out with the
			 * pre-existing demand. Multiple windows may have
			 * elapsed, but since empty windows are dropped,
			 * it is not necessary to account those.
			 */
			update_history(rq, p, wts->sum, 1, event);
		return 0;
	}

	if (!new_window) {
		/*
		 * The simple case - busy time contained within the existing
		 * window.
		 */
		return add_to_task_demand(rq, p, wallclock - mark_start);
	}

	/*
	 * Busy time spans at least two windows. Temporarily rewind
	 * window_start to first window boundary after mark_start.
	 */
	delta = window_start - mark_start;
	nr_full_windows = div64_u64(delta, window_size);
	window_start -= (u64)nr_full_windows * (u64)window_size;

	/* Process (window_start - mark_start) first */
	runtime = add_to_task_demand(rq, p, window_start - mark_start);

	/* Push new sample(s) into task's demand history */
	update_history(rq, p, wts->sum, 1, event);
	if (nr_full_windows) {
		u64 scaled_window = scale_exec_time(window_size, rq, wts);

		update_history(rq, p, scaled_window, nr_full_windows, event);
		runtime += nr_full_windows * scaled_window;
	}

	/*
	 * Roll window_start back to current to process any remainder
	 * in current window.
	 */
	window_start += (u64)nr_full_windows * (u64)window_size;

	/* Process (wallclock - window_start) next */
	mark_start = window_start;
	runtime += add_to_task_demand(rq, p, wallclock - mark_start);

	return runtime;
}

static inline unsigned int cpu_cur_freq(int cpu)
{
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu);

	return wrq->cluster->cur_freq;
}

static void
update_task_rq_cpu_cycles(struct task_struct *p, struct rq *rq, int event,
			  u64 wallclock, u64 irqtime)
{
	u64 cur_cycles;
	u64 cycles_delta;
	u64 time_delta;
	int cpu = cpu_of(rq);
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	walt_lockdep_assert_rq(rq, p);

	if (!use_cycle_counter) {
		wrq->task_exec_scale = DIV64_U64_ROUNDUP(cpu_cur_freq(cpu) *
				arch_scale_cpu_capacity(cpu),
				wrq->cluster->max_possible_freq);
		return;
	}

	cur_cycles = read_cycle_counter(cpu, wallclock);

	/*
	 * If current task is idle task and irqtime == 0 CPU was
	 * indeed idle and probably its cycle counter was not
	 * increasing.  We still need estimatied CPU frequency
	 * for IO wait time accounting.  Use the previously
	 * calculated frequency in such a case.
	 */
	if (!walt_is_idle_task(rq->curr) || irqtime) {
		if (unlikely(cur_cycles < wts->cpu_cycles))
			cycles_delta = cur_cycles + (U64_MAX -
				wts->cpu_cycles);
		else
			cycles_delta = cur_cycles - wts->cpu_cycles;
		cycles_delta = cycles_delta * NSEC_PER_MSEC;

		if (event == IRQ_UPDATE && walt_is_idle_task(p))
			/*
			 * Time between mark_start of idle task and IRQ handler
			 * entry time is CPU cycle counter stall period.
			 * Upon IRQ handler entry walt_sched_account_irqstart()
			 * replenishes idle task's cpu cycle counter so
			 * cycles_delta now represents increased cycles during
			 * IRQ handler rather than time between idle entry and
			 * IRQ exit.  Thus use irqtime as time delta.
			 */
			time_delta = irqtime;
		else
			time_delta = wallclock - wts->mark_start;

		if ((s64)time_delta < 0) {
			printk_deferred("WALT-BUG pid=%u CPU%d wallclock=%llu(0x%llx) < mark_start=%llu(0x%llx) event=%d irqtime=%llu",
					 p->pid, rq->cpu, wallclock, wallclock,
					 wts->mark_start, wts->mark_start, event, irqtime);
			WALT_PANIC((s64)time_delta < 0);
		}

		wrq->task_exec_scale = DIV64_U64_ROUNDUP(cycles_delta *
				arch_scale_cpu_capacity(cpu),
				time_delta *
					wrq->cluster->max_possible_freq);

		trace_sched_get_task_cpu_cycles(cpu, event,
				cycles_delta, time_delta, p);
	}

	wts->cpu_cycles = cur_cycles;
}

/*
 * Returns
 * 0: if window rollover not required or is not the winning CPU.
 * 1: if this CPU is tasked with window rollover duties.
 */
static inline int run_walt_irq_work_rollover(u64 old_window_start, struct rq *rq)
{
	u64 result;
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));

	if (old_window_start == wrq->window_start)
		return 0;

	result = atomic64_cmpxchg(&walt_irq_work_lastq_ws, old_window_start,
				   wrq->window_start);
	if (result == old_window_start) {
		walt_irq_work_queue(&walt_cpufreq_irq_work);
		trace_walt_window_rollover(wrq->window_start);
		return 1;
	}

	return 0;
}

static inline void set_bits(struct walt_task_struct *wts,
		int nr_bits, bool set_bit)
{
	int mask = 0;

	if (nr_bits > 16)
		nr_bits = 16;
	wts->busy_bitmap = wts->busy_bitmap << nr_bits;
	if (set_bit)
		mask = (1 << nr_bits) - 1;

	wts->busy_bitmap |= mask;
}

/*
 * Easy Case
 *
 *  |          ms          wc             |
 *  |          |            |             |
 *  +----------+------------+-------------+--------
 *  | contrib->+----------->|             |
 *  |          |            |             |
 * boundary                          next_ms_boundary
 *
 *
 * ms in old ms boundary while wc in new ms boundary, which case the code accounts for bit
 * until then next_ms_boundary and from next_ms_boundary to wc gets accounted in period
 *
 *  |          ms                        |          wc
 *  |          |                         |          |
 *  +----------+-------------------------+----------+-
 *  |          |                         | contrib->|
 *  |          |                         |
 * boundary                          next_ms_boundary
 *
 *
 * multiple boundaries between ms and wc,  which case the code accounts for bit
 * until the next_ms_boundary and fills in the interim periods and the leftover from
 * the closest is accounted in period
 *
 *  |          ms                        |                       |         wc
 *  |          |                         |                       |         |
 *  +----------+-------------------------+------periods----------+---------+--------
 *  |          |                         |                       |contrib->
 *  |          |                         |                       |
 * boundary                          next_ms_boundary
 */
static void update_busy_bitmap(struct task_struct *p, struct rq *rq, int event,
		u64 wallclock)
{
	struct walt_task_struct *wts = (struct walt_task_struct *)p->android_vendor_data1;
	struct walt_rq *wrq = &per_cpu(walt_rq, task_cpu(p));
	u64 next_ms_boundary, delta;
	int periods;
	bool running;
	int no_boost_reason = 0;

	/*
	 * If it has been active for more than 4mS turn it off, the task that caused this activation
	 * should have slept and if its still running it must have updated its load via
	 * prs. No need to continue boosting.
	 */
	if (wallclock > wrq->lrb_pipeline_start_time + 4000000)
		wrq->lrb_pipeline_start_time = 0;

	if (!pipeline_in_progress())
		return;

	/*
	 * Figure out whether pipeline_cpu, cpu_of(rq) are both same or if it
	 * even matters.
	 */
	if (wts->pipeline_cpu == -1)
		return;

	if (wallclock < wts->mark_start) {
		printk_deferred("WALT-BUG CPU%d: %s task %s(%d) mark_start %llu is higher than wallclock %llu\n",
				raw_smp_processor_id(), __func__, p->comm, p->pid,
				wts->mark_start, wallclock);
		WALT_PANIC(1);
		wallclock = wts->mark_start;
	}

	running = account_busy_for_cpu_time(rq, p, 0, event);

	/* task woke up or utra happened while its asleep, clear old boosts */
	if (p->on_rq == 0)
		walt_flag_set(p, WALT_LRB_PIPELINE_BIT, 0);

	next_ms_boundary = ((wts->mark_start + (NSEC_PER_MSEC - 1)) / NSEC_PER_MSEC) *
				NSEC_PER_MSEC;
	if (wallclock < next_ms_boundary) {
		if (running)
			wts->period_contrib_run += wallclock - wts->mark_start;
		goto out;
	}

	/* Exceeding a ms boundary */

	/* Close the bit corresponding to the mark_start */
	if (running)
		wts->period_contrib_run += next_ms_boundary - wts->mark_start;
	/* Set the bit representing the ms if runtime within that ms is more than 500us*/
	if (wts->period_contrib_run > 500000)
		set_bits(wts, 1, true);
	else
		set_bits(wts, 1, false);
	wts->period_contrib_run = 0;

	/* Account the action starting from next_ms_boundary to the closest ms boundary */
	delta = wallclock - next_ms_boundary;
	periods = delta / NSEC_PER_MSEC;

	if (periods) {
		if (running)
			set_bits(wts, periods, true);
		else
			set_bits(wts, periods, false);
	}

	/* Start contributions for latest ms */
	if (running)
		wts->period_contrib_run = wallclock % NSEC_PER_MSEC;

	/* task had already set a boost since wakeup, boost just once since wakeup */
	if (walt_flag_test(p, WALT_LRB_PIPELINE_BIT)) {
		no_boost_reason = 1;
		goto out;
	}

	/*
	 * task is not on_rq - if it is in the process of waking up, boost will be applied on the
	 * right cpu at PICK event
	 */
	if (p->on_rq == 0) {
		no_boost_reason = 2;
		goto out;
	}

	if (sched_ravg_window <= SCHED_RAVG_8MS_WINDOW &&
			((hweight16(wts->busy_bitmap & 0x00FF) < sysctl_sched_lrpb_active_ms[0]) ||
			!sysctl_sched_lrpb_active_ms[0])) {
		no_boost_reason = 3;
		goto out;
	}

	if (sched_ravg_window == SCHED_RAVG_12MS_WINDOW &&
			((hweight16(wts->busy_bitmap & 0x0FFF) < sysctl_sched_lrpb_active_ms[1]) ||
			 !sysctl_sched_lrpb_active_ms[1])) {
		no_boost_reason = 4;
		goto out;
	}

	if (sched_ravg_window >= SCHED_RAVG_16MS_WINDOW &&
			((hweight16(wts->busy_bitmap) < sysctl_sched_lrpb_active_ms[2]) ||
			 !sysctl_sched_lrpb_active_ms[2])) {
		no_boost_reason = 5;
		goto out;
	}

	/* cpu already boosted, so don't extend */
	if (wrq->lrb_pipeline_start_time != 0) {
		no_boost_reason = 6;
		goto out;
	}

	walt_flag_set(p, WALT_LRB_PIPELINE_BIT, 1);
	wrq->lrb_pipeline_start_time = wallclock;

out:
	trace_sched_update_busy_bitmap(p, rq, wts, wrq, event,
			wallclock, next_ms_boundary, no_boost_reason);
}

// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_MIGT_WALT
void register_oem_update_frame_load_hook(oem_update_f f)
{
	if (likely(f))
		oem_update_frame_load_hook = f;
}
EXPORT_SYMBOL_GPL(register_oem_update_frame_load_hook);

void unregister_oem_update_frame_load_hook(void)
{
	oem_update_frame_load_hook = NULL;
}
EXPORT_SYMBOL(unregister_oem_update_frame_load_hook);

void register_oem_flt_enable_hook(oem_flt_enable_f f)
{
	if (likely(f))
		oem_flt_enable_hook = f;
}
EXPORT_SYMBOL_GPL(register_oem_flt_enable_hook);

void unregister_oem_flt_enable_hook(void)
{
	oem_flt_enable_hook = NULL;
}
EXPORT_SYMBOL(unregister_oem_flt_enable_hook);
#endif
// END Performance_TurboSched

/* Reflect task activity on its demand and cpu's busy time statistics */
// MIUI ADD: Game_TurboSched
#ifdef CONFIG_MIGT_WALT
void walt_update_task_ravg(struct task_struct *p, struct rq *rq, int event,
						u64 wallclock, u64 irqtime)
#else
static void walt_update_task_ravg(struct task_struct *p, struct rq *rq, int event,
						u64 wallclock, u64 irqtime)
#endif
// END Game_TurboSched
{
	u64 old_window_start;
	int this_cpu_runs_window_rollover;
	bool old_lrb_pipeline_task_state;
	bool old_lrb_pipeline_cpu_state;
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	if (!wrq->window_start || wts->mark_start == wallclock)
		return;

	if (unlikely(!raw_spin_is_locked(&rq->__lock))) {
		printk_deferred("WALT-BUG CPU%d: %s task %s(%d) unlocked access for cpu=%d suspended=%d last_clk=%llu stack[%pS <== %pS <== %pS]\n",
				raw_smp_processor_id(), __func__, p->comm, p->pid, rq->cpu,
				walt_clock_suspended, sched_clock_last,
				(void *)CALLER_ADDR0, (void *)CALLER_ADDR1, (void *)CALLER_ADDR2);
		WALT_PANIC(1);
	}

	walt_lockdep_assert_rq(rq, p);

	old_window_start = update_window_start(rq, wallclock, event);
	old_lrb_pipeline_task_state = walt_flag_test(p, WALT_LRB_PIPELINE_BIT);
	old_lrb_pipeline_cpu_state = wrq->lrb_pipeline_start_time;
	if (!wts->window_start)
		wts->window_start = wrq->window_start;

	if (!wts->mark_start) {
		update_task_cpu_cycles(p, cpu_of(rq), wallclock);
		goto done;
	}

	update_task_rq_cpu_cycles(p, rq, event, wallclock, irqtime);
	update_task_demand(p, rq, event, wallclock);
	update_cpu_busy_time(p, rq, event, wallclock, irqtime);
	update_task_pred_demand(rq, p, event);
	update_busy_bitmap(p, rq, event, wallclock);
// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_MI_SCHED_WALT
	if (pkg_enable()) {
		int fstat = 0;
		u64 delta = 0;
		int pkg_task_busy = account_pkg_busy_time(rq, p, event);

		if (pkg_task_busy) {
			fstat |= PKG_TASK_BUSY;
			if (is_idle_task(p))
				delta = irqtime;
			else
				delta = wallclock - wts->mark_start;
			delta = scale_exec_time(delta, rq, wts);
			update_pkg_load(p, rq->cpu, fstat, wallclock, delta);
		}
// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_MIGT_WALT
		fstat = 0;
		if (account_pkg_busy_time(rq, p, event))
			fstat |= PKG_TASK_BUSY;
		if (account_busy_for_cpu_time(rq, p, irqtime, event))
			fstat |= FRAME_STAT_CPUBUSY;
		if (account_busy_for_task_demand(rq, p, event))
			fstat |= FRAME_STAT_TASKBUSY;
		if (mi_update_fravg_f)
			mi_update_fravg_f(p, rq, fstat, wallclock, irqtime,
						event);

		if (oem_update_frame_load_hook) {
			fstat = 0;

			if (account_busy_for_task_demand(rq, p, event))
				fstat |=  FRAME_STAT_TASKBUSY;
			if (account_busy_for_cpu_time(rq, p, irqtime, event))
				fstat |= FRAME_STAT_CPUBUSY;
			if (cpu_is_waiting_on_io(rq))
				fstat |= FRAME_STAT_WAITONIO;

            oem_update_frame_load_hook(p, rq, fstat, wallclock, irqtime, event);
        }
#endif
// END Performance_TurboSched
	}

#ifdef CONFIG_METIS_WALT
    metis_update_top_task_info(p, rq, account_pkg_busy_time(rq, p, event),
            wallclock, wts->mark_start);
#endif
#endif
// END Performance_TurboSched

	if (event == PUT_PREV_TASK && READ_ONCE(p->__state))
		wts->iowaited = p->in_iowait;

	trace_sched_update_task_ravg(p, rq, event, wallclock, irqtime,
				&wrq->grp_time, wrq, wts, atomic64_read(&walt_irq_work_lastq_ws));
	trace_sched_update_task_ravg_mini(p, rq, event, wallclock, irqtime,
				&wrq->grp_time, wrq, wts, atomic64_read(&walt_irq_work_lastq_ws));

done:
	wts->mark_start = wallclock;
	if (wts->mark_start > (wts->window_start + sched_ravg_window))
		WALT_BUG(WALT_BUG_WALT, p,
			"CPU%d: %s task %s(%d)'s ms=%llu is ahead of ws=%llu by more than 1 window on rq=%d event=%d",
			raw_smp_processor_id(), __func__, p->comm, p->pid,
			wts->mark_start, wts->window_start, rq->cpu, event);

	this_cpu_runs_window_rollover = run_walt_irq_work_rollover(old_window_start, rq);
	if (likely(!this_cpu_runs_window_rollover)) {
		if ((unlikely(wts->pipeline_cpu != -1) &&
				task_cpu(p) == cpu_of(rq) &&
				!old_lrb_pipeline_task_state &&
				walt_flag_test(p, WALT_LRB_PIPELINE_BIT)) ||
				(old_lrb_pipeline_cpu_state && !wrq->lrb_pipeline_start_time))
			waltgov_run_callback(rq, WALT_CPUFREQ_PIPELINE_BUSY_BIT);
	}
}

// MIUI ADD: Game_TurboSched
#ifdef CONFIG_MIGT_WALT
EXPORT_SYMBOL_GPL(walt_update_task_ravg);
#endif
// END Game_TurboSched

static inline void __sched_fork_init(struct task_struct *p)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	wts->last_sleep_ts	= 0;
	wts->wake_up_idle	= false;
	wts->boost		= 0;
	wts->boost_expires	= 0;
	wts->boost_period	= false;
	wts->low_latency	= false;
	wts->iowaited		= false;
	wts->load_boost		= 0;
	wts->boosted_task_load	= 0;
	wts->reduce_mask	= CPU_MASK_ALL;
	wts->lst		= false;
	wts->lst_tgt_ns		= 0;
	wts->lst_state_counter	= 0;
	wts->pipeline_activity_cnt = 0;
	atomic_set(&wts->event_windows, 0);
}

static void init_new_task_load(struct task_struct *p)
{
	int i;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	struct walt_task_struct *cur_wts =
		(struct walt_task_struct *) current->android_vendor_data1;
	u32 init_load_windows = sched_init_task_load_windows;
	u32 init_load_windows_scaled = sched_init_task_load_windows_scaled;
	u32 init_load_pct = cur_wts->init_load_pct;

	wts->init_load_pct = 0;
	rcu_assign_pointer(wts->grp, NULL);
	INIT_LIST_HEAD(&wts->grp_list);

	wts->prev_cpu = raw_smp_processor_id();
	wts->new_cpu = -1;
	wts->enqueue_after_migration = 0;
	wts->mark_start = 0;
	wts->window_start = 0;
	wts->sum = 0;
	wts->curr_window = 0;
	wts->prev_window = 0;
	wts->active_time = 0;
	wts->prev_on_rq = 0;
	wts->prev_on_rq_cpu = -1;
	wts->pipeline_cpu = -1;
	wts->yield_state = 0;
	wts->busy_bitmap = 0;
	wts->period_contrib_run = 0;

	for (i = 0; i < NUM_BUSY_BUCKETS; ++i)
		wts->busy_buckets[i] = 0;
	wts->bucket_bitmask = 0;
	wts->cpu_cycles = 0;

	memset(wts->curr_window_cpu, 0, sizeof(u32) * WALT_NR_CPUS);
	memset(wts->prev_window_cpu, 0, sizeof(u32) * WALT_NR_CPUS);

	if (init_load_pct) {
		init_load_windows = div64_u64((u64)init_load_pct *
			  (u64)sched_ravg_window, 100);
		init_load_windows_scaled = scale_time_to_util(init_load_windows);
	}

	wts->demand = init_load_windows;
	wts->demand_scaled = init_load_windows_scaled;
	wts->coloc_demand = init_load_windows;
	wts->pred_demand_scaled = 0;
	for (i = 0; i < RAVG_HIST_SIZE; ++i)
		wts->sum_history[i] = init_load_windows;
	wts->misfit = false;
	wts->rtg_high_prio = false;
	wts->unfilter = sysctl_sched_task_unfilter_period;

	INIT_LIST_HEAD(&wts->mvp_list);
	wts->sum_exec_snapshot_for_slice = 0;
	wts->sum_exec_snapshot_for_total = 0;
	wts->total_exec = 0;
	wts->mvp_prio = WALT_NOT_MVP;
	wts->cidx = 0;
	wts->mark_start_birth_ts = 0;
	wts->high_util_history = 0;
	__sched_fork_init(p);

	/* New task inherits the MPAM part_id */
	wts->mpam_part_id = cur_wts->mpam_part_id;

	walt_flag_set(p, WALT_INIT_BIT, 1);
	walt_flag_set(p, WALT_TRAILBLAZER_BIT, 0);
}

int remove_heavy(struct walt_task_struct *wts);
static int __sched_set_group_id(struct task_struct *p, unsigned int group_id);
static void walt_task_dead(struct task_struct *p)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	__sched_set_group_id(p, 0);

	if (wts->low_latency & WALT_LOW_LATENCY_PIPELINE_BIT)
		remove_pipeline(wts);

	remove_heavy(wts);

	if (p == pipeline_special_task)
		remove_special_task();
}

static void mark_task_starting(struct task_struct *p)
{
	struct rq *rq = task_rq(p);
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	u64 wallclock = walt_rq_clock(rq);

	wts->last_wake_ts = wallclock;
	wts->last_enqueued_ts = wallclock;
	wts->mark_start_birth_ts = wallclock;

	if (wts->mark_start)
		return;
	walt_update_task_ravg(p, rq, TASK_UPDATE, wallclock, 0);
}

/*
 * Task groups whose aggregate demand on a cpu is more than
 * sched_group_upmigrate need to be up-migrated if possible.
 */
static unsigned int __read_mostly sched_group_upmigrate = 20000000;

/*
 * Task groups, once up-migrated, will need to drop their aggregate
 * demand to less than sched_group_downmigrate before they are "down"
 * migrated.
 */
static unsigned int __read_mostly sched_group_downmigrate = 19000000;

void walt_update_group_thresholds(void)
{
	unsigned int min_scale = arch_scale_cpu_capacity(
				cluster_first_cpu(sched_cluster[0]));
	u64 min_ms = min_scale * (sched_ravg_window >> SCHED_CAPACITY_SHIFT);

	sched_group_upmigrate = div64_ul(min_ms *
				sysctl_sched_group_upmigrate_pct, 100);
	sched_group_downmigrate = div64_ul(min_ms *
				sysctl_sched_group_downmigrate_pct, 100);
}

struct walt_sched_cluster *sched_cluster[WALT_NR_CPUS];
__read_mostly int num_sched_clusters;

struct list_head cluster_head;

// MIUI ADD: Game_TurboSched
#ifdef CONFIG_MIGT_WALT
EXPORT_SYMBOL_GPL(cluster_head);
#endif
// END Game_TurboSched

static struct walt_sched_cluster init_cluster = {
	.list			= LIST_HEAD_INIT(init_cluster.list),
	.id			= 0,
	.cur_freq		= 1,
	.max_possible_freq	= 1,
	.aggr_grp_load		= 0,
	.found_ts		= 0,
};

static void init_clusters(void)
{
	init_cluster.cpus = *cpu_possible_mask;
	raw_spin_lock_init(&init_cluster.load_lock);
	INIT_LIST_HEAD(&cluster_head);
	list_add(&init_cluster.list, &cluster_head);
}

static void
insert_cluster(struct walt_sched_cluster *cluster, struct list_head *head)
{
	struct walt_sched_cluster *tmp;
	struct list_head *iter = head;

	list_for_each_entry(tmp, head, list) {
		if (arch_scale_cpu_capacity(cluster_first_cpu(cluster))
			< arch_scale_cpu_capacity(cluster_first_cpu(tmp)))
			break;
		iter = &tmp->list;
	}

	list_add(&cluster->list, iter);
}

static struct walt_sched_cluster *alloc_new_cluster(const struct cpumask *cpus)
{
	struct walt_sched_cluster *cluster = NULL;

	cluster = kzalloc(sizeof(struct walt_sched_cluster), GFP_ATOMIC);
	BUG_ON(!cluster);

	INIT_LIST_HEAD(&cluster->list);
	cluster->cur_freq		=	1;
	cluster->max_freq		=	1;
	cluster->max_possible_freq	=	1;

	raw_spin_lock_init(&cluster->load_lock);
	cluster->cpus			= *cpus;
	cluster->found_ts		= 0;
	cluster->sibling_cluster	= -1;

	return cluster;
}

static void add_cluster(const struct cpumask *cpus, struct list_head *head)
{
	struct walt_sched_cluster *cluster = alloc_new_cluster(cpus);
	int i;
	struct walt_rq *wrq;

	BUG_ON(num_sched_clusters >= MAX_CLUSTERS);

	for_each_cpu(i, cpus) {
		wrq = &per_cpu(walt_rq, i);
		wrq->cluster = cluster;
	}

	insert_cluster(cluster, head);
	num_sched_clusters++;
}

static void cleanup_clusters(struct list_head *head)
{
	struct walt_sched_cluster *cluster, *tmp;
	int i;
	struct walt_rq *wrq;

	list_for_each_entry_safe(cluster, tmp, head, list) {
		for_each_cpu(i, &cluster->cpus) {
			wrq = &per_cpu(walt_rq, i);
			wrq->cluster = &init_cluster;
		}
		list_del(&cluster->list);
		num_sched_clusters--;
		kfree(cluster);
	}
}

static inline void align_clusters(struct list_head *head)
{
	struct walt_sched_cluster *tmp;
	struct list_head *cluster1 = head, *cluster2 = head;
	unsigned long capacity1 = 0, capacity2 = 0;
	int i = 0;

	if (num_sched_clusters != 4)
		return;

	list_for_each_entry(tmp, head, list) {
		if (i == 1) {
			cluster1 = &tmp->list;
			capacity1 = arch_scale_cpu_capacity(cluster_first_cpu(tmp));
		}
		if (i == 2) {
			cluster2 = &tmp->list;
			capacity2 = arch_scale_cpu_capacity(cluster_first_cpu(tmp));
		}
		i++;
	}

	if (capacity1 < capacity2)
		list_swap(cluster1, cluster2);
}

static inline void assign_cluster_ids(struct list_head *head)
{
	struct walt_sched_cluster *cluster;
	int pos = 0;

	list_for_each_entry(cluster, head, list) {
		cluster->id = pos;
		sched_cluster[pos++] = cluster;
	}

	WARN_ON(pos > MAX_CLUSTERS);
}

static inline void
move_list(struct list_head *dst, struct list_head *src, bool sync_rcu)
{
	struct list_head *first, *last;

	first = src->next;
	last = src->prev;

	if (sync_rcu) {
		INIT_LIST_HEAD_RCU(src);
		synchronize_rcu();
	}

	first->prev = dst;
	dst->prev = last;
	last->next = dst;

	/* Ensure list sanity before making the head visible to all CPUs. */
	smp_mb();
	dst->next = first;
}

static void update_all_clusters_stats(void)
{
	struct walt_sched_cluster *cluster;
	u64 highest_mpc = 0, lowest_mpc = U64_MAX;

	for_each_sched_cluster(cluster) {
		u64 mpc = arch_scale_cpu_capacity(
				cluster_first_cpu(cluster));
		int cluster_id = cluster->id;

		if (mpc > highest_mpc) {
			highest_mpc = mpc;
			max_possible_cluster_id = cluster_id;
		}

		if (mpc < lowest_mpc) {
			lowest_mpc = mpc;
			min_possible_cluster_id = cluster_id;
// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_METIS_WALT
			metis_lb_min_possible_cluster_id = cluster_id;
#endif
// END Performance_TurboSched
		}
	}
	walt_update_group_thresholds();
}

static bool walt_clusters_parsed;
cpumask_t __read_mostly **cpu_array;

u8 cpu_arrays_init_x11[1][1] = {
	{0}, /* S */
};

u8 cpu_arrays_init_x22[2][2] = {
	{0, 1}, /* S G */
	{1, 0}, /* G S */
};

u8 cpu_arrays_init_x33[3][3] = {
	{0, 1, 2}, /* S G P */
	{1, 2, 0}, /* G P S */
	{2, 1, 0}, /* P G S */
};

u8 cpu_arrays_init_x44[4][4] = {
	{0, 2, 1, 3}, /* S T G P */
	{1, 2, 3, 0}, /* G T P S */
	{2, 3, 1, 0}, /* T P G S */
	{3, 1, 2, 0}, /* P G T S */
};

static void init_cpu_array(void)
{
	int i;
	int rows = num_sched_clusters;

	cpu_array = kcalloc(rows, sizeof(cpumask_t *),
			GFP_ATOMIC | __GFP_NOFAIL);
	if (!cpu_array)
		WALT_PANIC(1);

	for (i = 0; i < rows; i++) {
		cpu_array[i] = kcalloc(num_sched_clusters, sizeof(cpumask_t),
			GFP_ATOMIC | __GFP_NOFAIL);
		if (!cpu_array[i])
			WALT_PANIC(1);
	}
// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_METIS_WALT
	metis_lb_cpu_array = cpu_array;
#endif
// END Performance_TurboSched
}

static void build_cpu_array(void)
{
	u8 *select_init_list;
	u8 id;
	int i, j;

	if (!cpu_array)
		WALT_PANIC(1);

	switch (num_sched_clusters) {
	case 1:
		select_init_list = (u8 *)cpu_arrays_init_x11;
		break;
	case 2:
		select_init_list = (u8 *)cpu_arrays_init_x22;
		break;
	case 3:
		select_init_list = (u8 *)cpu_arrays_init_x33;
		break;
	case 4:
		select_init_list = (u8 *)cpu_arrays_init_x44;
		break;
	default:
		pr_err("unsupported num clusters=%d\n", num_sched_clusters);
		WALT_PANIC(1);
	}

	for (i = 0; i < num_sched_clusters; i++) {
		for (j = 0; j < num_sched_clusters; j++) {
			id = select_init_list[i * num_sched_clusters + j];
			cpumask_copy(&cpu_array[i][j], &sched_cluster[id]->cpus);
		}
	}
}

static void walt_get_possible_siblings(int cpuid, struct cpumask *cluster_cpus)
{
	int cpu;
	struct cpu_topology *cpu_topo, *cpuid_topo = &cpu_topology[cpuid];

	if (cpuid_topo->cluster_id == -1)
		return;

	for_each_possible_cpu(cpu) {
		cpu_topo = &cpu_topology[cpu];

		if (cpuid_topo->cluster_id != cpu_topo->cluster_id)
			continue;
		cpumask_set_cpu(cpu, cluster_cpus);
	}
}

int cpu_l2_sibling[WALT_NR_CPUS] = {[0 ... WALT_NR_CPUS-1] = -1};
static void find_cache_siblings(void)
{
	int cpu, cpu2;
	struct device_node *cpu_dev, *cpu_dev2, *cpu_l2_cache_node, *cpu_l2_cache_node2;

	for_each_possible_cpu(cpu) {
		cpu_dev = of_get_cpu_node(cpu, NULL);
		if (!cpu_dev)
			continue;

		cpu_l2_cache_node = of_parse_phandle(cpu_dev, "next-level-cache", 0);
		if (!cpu_l2_cache_node)
			continue;

		for_each_possible_cpu(cpu2) {
			if (cpu == cpu2)
				continue;

			cpu_dev2 = of_get_cpu_node(cpu2, NULL);
			if (!cpu_dev2)
				continue;

			cpu_l2_cache_node2 = of_parse_phandle(cpu_dev2, "next-level-cache", 0);
			if (!cpu_l2_cache_node2)
				continue;

			if (cpu_l2_cache_node == cpu_l2_cache_node2) {
				cpu_l2_sibling[cpu] = cpu2;
				break;
			}
		}
	}
}

static void walt_update_cluster_topology(void)
{
	struct cpumask cpus = *cpu_possible_mask;
	struct cpumask cluster_cpus;
	struct walt_sched_cluster *cluster;
	struct list_head new_head;
	int i;
	struct walt_rq *wrq;

	INIT_LIST_HEAD(&new_head);

	for_each_cpu(i, &cpus) {
		cpumask_clear(&cluster_cpus);
		walt_get_possible_siblings(i, &cluster_cpus);
		if (cpumask_empty(&cluster_cpus)) {
			WARN(1, "WALT: Invalid cpu topology!!");
			cleanup_clusters(&new_head);
			return;
		}
		cpumask_andnot(&cpus, &cpus, &cluster_cpus);
		add_cluster(&cluster_cpus, &new_head);
	}

// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_MI_SCHED_WALT
	oem_update_num_sched_clusters(num_sched_clusters);
#endif
// END Performance_TurboSched

	align_clusters(&new_head);
	assign_cluster_ids(&new_head);

	list_for_each_entry(cluster, &new_head, list) {
		struct cpufreq_policy *policy;

		policy = cpufreq_cpu_get_raw(cluster_first_cpu(cluster));
		/*
		 * walt_update_cluster_topology() must be called AFTER policies
		 * for all cpus are initialized. If not, simply BUG().
		 */
		WALT_PANIC(!policy);

		if (policy) {
			cluster->max_possible_freq = policy->cpuinfo.max_freq;
			cluster->max_freq = policy->max;
			for_each_cpu(i, &cluster->cpus) {
				wrq = &per_cpu(walt_rq, i);
				cpumask_copy(&wrq->freq_domain_cpumask,
					     policy->related_cpus);
			}
			cpuinfo_max_freq_cached = (cpuinfo_max_freq_cached >
			policy->cpuinfo.max_freq) ? cpuinfo_max_freq_cached
				: policy->cpuinfo.max_freq;
		}
	}

	/*
	 * Ensure cluster ids are visible to all CPUs before making
	 * cluster_head visible.
	 */
	move_list(&cluster_head, &new_head, false);
	update_all_clusters_stats();

	init_cpu_array();
	build_cpu_array();
	find_cache_siblings();

	create_util_to_cost();
	walt_clusters_parsed = true;
}

static void walt_init_cycle_counter(void)
{
	char *walt_cycle_cntr_path = "/soc/walt";
	struct device_node *np = NULL;

	if (soc_feat(SOC_ENABLE_SW_CYCLE_COUNTER_BIT)) {
		walt_cycle_counter_init();
	} else {
		np = of_find_node_by_path(walt_cycle_cntr_path);
		of_platform_populate(np, NULL, NULL, NULL);
	}

	wait_for_completion_interruptible(&walt_get_cycle_counts_cb_completion);
}

static void transfer_busy_time(struct rq *rq,
				struct walt_related_thread_group *grp,
					struct task_struct *p, int event);

/*
 * Enable colocation and frequency aggregation for all threads in a process.
 * The children inherits the group id from the parent.
 */

static inline
void update_best_cluster(struct walt_related_thread_group *grp,
				   u64 combined_demand, bool boost)
{
	if (boost) {
		/*
		 * since we are in boost, we can keep grp on min, the boosts
		 * will ensure tasks get to bigs
		 */
		grp->skip_min = false;
		return;
	}

	if (is_suh_max())
		combined_demand = sched_group_upmigrate;

	if (!grp->skip_min) {
		if (combined_demand >= sched_group_upmigrate)
			grp->skip_min = true;
		return;
	}
	if (combined_demand < sched_group_downmigrate) {
		if (!sysctl_sched_coloc_downmigrate_ns ||
				(grp->last_update - grp->start_ktime_ts) <
				sysctl_sched_hyst_min_coloc_ns) {
			grp->downmigrate_ts = 0;
			grp->skip_min = false;
			return;
		}
		if (!grp->downmigrate_ts) {
			grp->downmigrate_ts = grp->last_update;
			return;
		}
		if (grp->last_update - grp->downmigrate_ts >
				sysctl_sched_coloc_downmigrate_ns) {
			grp->downmigrate_ts = 0;
			grp->skip_min = false;
		}
	} else if (grp->downmigrate_ts)
		grp->downmigrate_ts = 0;
}

static void _set_preferred_cluster(struct walt_related_thread_group *grp)
{
	struct task_struct *p;
	u64 combined_demand = 0;
	bool group_boost = false;
	u64 wallclock;
	bool prev_skip_min = grp->skip_min;
	struct walt_task_struct *wts;

	if (sched_group_upmigrate == 0) {
		grp->skip_min = false;
		goto out;
	}

	if (list_empty(&grp->tasks)) {
		grp->skip_min = false;
		goto out;
	}

	if (!hmp_capable()) {
		grp->skip_min = false;
		goto out;
	}

	wallclock = walt_sched_clock();

	/*
	 * wakeup of two or more related tasks could race with each other and
	 * could result in multiple calls to _set_preferred_cluster being issued
	 * at same time. Avoid overhead in such cases of rechecking preferred
	 * cluster
	 */
	if (wallclock - grp->last_update < sched_ravg_window / 10)
		return;

	list_for_each_entry(wts, &grp->tasks, grp_list) {
		p = wts_to_ts(wts);
		if (task_boost_policy(p) == SCHED_BOOST_ON_BIG) {
			group_boost = true;
			break;
		}

		if (wts->mark_start < wallclock -
		    (sched_ravg_window * RAVG_HIST_SIZE))
			continue;

		combined_demand += wts->coloc_demand;
		if (!trace_sched_set_preferred_cluster_enabled()) {
			if (combined_demand > sched_group_upmigrate)
				break;
		}
	}

	grp->last_update = wallclock;
	update_best_cluster(grp, combined_demand, group_boost);

out:
	trace_sched_set_preferred_cluster(grp, combined_demand, prev_skip_min,
			sched_group_upmigrate, sched_group_downmigrate);
	if (grp->id == DEFAULT_CGROUP_COLOC_ID
			&& grp->skip_min != prev_skip_min) {
		if (grp->skip_min)
			grp->start_ktime_ts = wallclock;
		else
			grp->start_ktime_ts = 0;
		sched_update_hyst_times();
	}
}

static void set_preferred_cluster(struct walt_related_thread_group *grp)
{
	raw_spin_lock(&grp->lock);
	_set_preferred_cluster(grp);
	raw_spin_unlock(&grp->lock);
}

static int update_preferred_cluster(struct walt_related_thread_group *grp,
		struct task_struct *p, u32 old_load, bool from_tick)
{
	u32 new_load = task_load(p);

	if (!grp)
		return 0;

	if (unlikely(from_tick && is_suh_max()))
		return 1;

	/*
	 * Update if task's load has changed significantly or a complete window
	 * has passed since we last updated preference
	 */

	if (abs(new_load - old_load) > sched_ravg_window / 4)
		return 1;

	if (walt_sched_clock() - grp->last_update > sched_ravg_window)
		return 1;

	return 0;
}

#define ADD_TASK	0
#define REM_TASK	1

struct walt_related_thread_group*
lookup_related_thread_group(unsigned int group_id)
{
	return related_thread_groups[group_id];
}

static int alloc_related_thread_groups(void)
{
	int i;
	struct walt_related_thread_group *grp;

	/* groupd_id = 0 is invalid as it's special id to remove group. */
	for (i = 1; i < MAX_NUM_CGROUP_COLOC_ID; i++) {
		grp = kzalloc(sizeof(*grp), GFP_ATOMIC | GFP_NOWAIT);
		BUG_ON(!grp);

		grp->id = i;
		INIT_LIST_HEAD(&grp->tasks);
		INIT_LIST_HEAD(&grp->list);
		raw_spin_lock_init(&grp->lock);

		related_thread_groups[i] = grp;
	}

	return 0;
}

static void remove_task_from_group(struct task_struct *p)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	struct walt_related_thread_group *grp = wts->grp;
	struct rq *rq;
	int empty_group = 1;
	struct rq_flags rf;

	raw_spin_lock(&grp->lock);

	rq = __task_rq_lock(p, &rf);
	transfer_busy_time(rq, wts->grp, p, REM_TASK);
	list_del_init(&wts->grp_list);
	rcu_assign_pointer(wts->grp, NULL);
	__task_rq_unlock(rq, &rf);

	if (!list_empty(&grp->tasks)) {
		empty_group = 0;
		_set_preferred_cluster(grp);
	}

	raw_spin_unlock(&grp->lock);

	/* Reserved groups cannot be destroyed */
	if (empty_group && grp->id != DEFAULT_CGROUP_COLOC_ID)
		 /*
		  * We test whether grp->list is attached with list_empty()
		  * hence re-init the list after deletion.
		  */
		list_del_init(&grp->list);
}

static int
add_task_to_group(struct task_struct *p, struct walt_related_thread_group *grp)
{
	struct rq *rq;
	struct rq_flags rf;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	raw_spin_lock(&grp->lock);

	/*
	 * Change wts->grp under rq->lock. Will prevent races with read-side
	 * reference of wts->grp in various hot-paths
	 */
	rq = __task_rq_lock(p, &rf);
	transfer_busy_time(rq, grp, p, ADD_TASK);
	list_add(&wts->grp_list, &grp->tasks);
	rcu_assign_pointer(wts->grp, grp);
	__task_rq_unlock(rq, &rf);

	_set_preferred_cluster(grp);

	raw_spin_unlock(&grp->lock);

	return 0;
}

#ifdef CONFIG_UCLAMP_TASK_GROUP
static inline bool uclamp_task_colocated(struct task_struct *p)
{
	struct cgroup_subsys_state *css;
	struct task_group *tg;
	bool colocate;
	struct walt_task_group *wtg;

	rcu_read_lock();
	css = task_css(p, cpu_cgrp_id);
	if (!css) {
		rcu_read_unlock();
		return false;
	}
	tg = container_of(css, struct task_group, css);
	wtg = (struct walt_task_group *) tg->android_vendor_data1;
	colocate = wtg->colocate;
	rcu_read_unlock();

	return colocate;
}
#else
static inline bool uclamp_task_colocated(struct task_struct *p)
{
	return false;
}
#endif /* CONFIG_UCLAMP_TASK_GROUP */

static void add_new_task_to_grp(struct task_struct *new)
{
	unsigned long flags;
	struct walt_related_thread_group *grp;
	struct walt_task_struct *wts = (struct walt_task_struct *) new->android_vendor_data1;

	/*
	 * If the task does not belong to colocated schedtune
	 * cgroup, nothing to do. We are checking this without
	 * lock. Even if there is a race, it will be added
	 * to the co-located cgroup via cgroup attach.
	 */
	if (!uclamp_task_colocated(new))
		return;

	grp = lookup_related_thread_group(DEFAULT_CGROUP_COLOC_ID);
	write_lock_irqsave(&related_thread_group_lock, flags);

	/*
	 * It's possible that someone already added the new task to the
	 * group. or it might have taken out from the colocated schedtune
	 * cgroup. check these conditions under lock.
	 */
	if (!uclamp_task_colocated(new) || wts->grp) {
		write_unlock_irqrestore(&related_thread_group_lock, flags);
		return;
	}

	raw_spin_lock(&grp->lock);

	rcu_assign_pointer(wts->grp, grp);
	list_add(&wts->grp_list, &grp->tasks);

	raw_spin_unlock(&grp->lock);
	write_unlock_irqrestore(&related_thread_group_lock, flags);
}

static int __sched_set_group_id(struct task_struct *p, unsigned int group_id)
{
	int rc = 0;
	unsigned long flags;
	struct walt_related_thread_group *grp = NULL;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	if (group_id >= MAX_NUM_CGROUP_COLOC_ID)
		return -EINVAL;

	if (unlikely(!walt_flag_test(p, WALT_INIT_BIT)))
		return -EINVAL;

	raw_spin_lock_irqsave(&p->pi_lock, flags);
	write_lock(&related_thread_group_lock);

	/* Switching from one group to another directly is not permitted */
	if ((!wts->grp && !group_id) || (wts->grp && group_id))
		goto done;

	if (!group_id) {
		remove_task_from_group(p);
		goto done;
	}

	grp = lookup_related_thread_group(group_id);
	if (list_empty(&grp->list))
		list_add(&grp->list, &active_related_thread_groups);

	rc = add_task_to_group(p, grp);
done:
	write_unlock(&related_thread_group_lock);
	raw_spin_unlock_irqrestore(&p->pi_lock, flags);

	return rc;
}

int sched_set_group_id(struct task_struct *p, unsigned int group_id)
{
	/* DEFAULT_CGROUP_COLOC_ID is a reserved id */
	if (group_id == DEFAULT_CGROUP_COLOC_ID)
		return -EINVAL;

	return 0;
}

unsigned int sched_get_group_id(struct task_struct *p)
{
	unsigned int group_id;
	struct walt_related_thread_group *grp;

	rcu_read_lock();
	grp = task_related_thread_group(p);
	group_id = grp ? grp->id : 0;
	rcu_read_unlock();

	return group_id;
}

/*
 * We create a default colocation group at boot. There is no need to
 * synchronize tasks between cgroups at creation time because the
 * correct cgroup hierarchy is not available at boot. Therefore cgroup
 * colocation is turned off by default even though the colocation group
 * itself has been allocated. Furthermore this colocation group cannot
 * be destroyted once it has been created. All of this has been as part
 * of runtime optimizations.
 *
 * The job of synchronizing tasks to the colocation group is done when
 * the colocation flag in the cgroup is turned on.
 */
static int create_default_coloc_group(void)
{
	struct walt_related_thread_group *grp = NULL;
	unsigned long flags;

	grp = lookup_related_thread_group(DEFAULT_CGROUP_COLOC_ID);
	write_lock_irqsave(&related_thread_group_lock, flags);
	list_add(&grp->list, &active_related_thread_groups);
	write_unlock_irqrestore(&related_thread_group_lock, flags);
	return 0;
}

static void walt_update_tg_pointer(struct cgroup_subsys_state *css)
{
	if (!strcmp(css->cgroup->kn->name, "top-app"))
		walt_init_topapp_tg(css_tg(css));
	else if (!strcmp(css->cgroup->kn->name, "foreground"))
		walt_init_foreground_tg(css_tg(css));
	else
		walt_init_tg(css_tg(css));
}

void walt_kick_cpu(int cpu)
{
	unsigned int flags = NOHZ_KICK_MASK;

	if (cpu == -1)
		return;

	/*
	 * Access to rq::nohz_csd is serialized by NOHZ_KICK_MASK; he who sets
	 * the first flag owns it; cleared by nohz_csd_func().
	 */
	flags = atomic_fetch_or(flags, nohz_flags(cpu));
	if (flags & NOHZ_KICK_MASK)
		return;

	/*
	 * This way we generate an IPI on the target CPU which
	 * is idle. And the softirq performing nohz idle load balance
	 * will be run before returning from the IPI.
	 */
	smp_call_function_single_async(cpu, &cpu_rq(cpu)->nohz_csd);
}


static void android_rvh_cpu_cgroup_online(void *unused, struct cgroup_subsys_state *css)
{
	if (unlikely(walt_disabled))
		return;

	walt_update_tg_pointer(css);
}

static void android_rvh_cpu_cgroup_attach(void *unused,
						struct cgroup_taskset *tset)
{
	struct task_struct *task;
	struct cgroup_subsys_state *css;
	struct task_group *tg;
	struct walt_task_group *wtg;
	unsigned int grp_id;
	int ret;

	if (unlikely(walt_disabled))
		return;

	cgroup_taskset_first(tset, &css);
	if (!css)
		return;

	tg = container_of(css, struct task_group, css);
	wtg = (struct walt_task_group *) tg->android_vendor_data1;

	cgroup_taskset_for_each(task, css, tset) {
		grp_id = wtg->colocate ? DEFAULT_CGROUP_COLOC_ID : 0;
		ret = __sched_set_group_id(task, grp_id);
		trace_sched_cgroup_attach(task, grp_id, ret);
	}
}

static bool is_cluster_hosting_top_app(struct walt_sched_cluster *cluster)
{
	struct walt_related_thread_group *grp;
	bool grp_on_min;

	grp = lookup_related_thread_group(DEFAULT_CGROUP_COLOC_ID);

	if (!grp)
		return false;

	grp_on_min = !grp->skip_min && (boost_policy != SCHED_BOOST_ON_BIG);

	return (is_min_capacity_cluster(cluster) == grp_on_min);
}

static void note_task_waking(struct task_struct *p, u64 wallclock)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	wts->last_wake_ts = wallclock;
}

/*
 * Task's cpu usage is accounted in:
 *	wrq->curr/prev_runnable_sum,  when its ->grp is NULL
 *	grp->cpu_time[cpu]->curr/prev_runnable_sum, when its ->grp is !NULL
 *
 * Transfer task's cpu usage between those counters when transitioning between
 * groups
 */
static void transfer_busy_time(struct rq *rq,
				struct walt_related_thread_group *grp,
					struct task_struct *p, int event)
{
	u64 wallclock;
	struct group_cpu_time *cpu_time;
	u64 *src_curr_runnable_sum, *dst_curr_runnable_sum;
	u64 *src_prev_runnable_sum, *dst_prev_runnable_sum;
	u64 *src_nt_curr_runnable_sum, *dst_nt_curr_runnable_sum;
	u64 *src_nt_prev_runnable_sum, *dst_nt_prev_runnable_sum;
	int migrate_type;
	int cpu = cpu_of(rq);
	bool new_task;
	int i;
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	wallclock = walt_sched_clock();

	walt_update_task_ravg(p, rq, TASK_UPDATE, wallclock, 0);

	if (wts->window_start != wrq->window_start)
		WALT_BUG(WALT_BUG_WALT, p,
				"CPU%d: %s event=%d task %s(%d)'s ws=%llu not equal to rq %d's ws=%llu",
				raw_smp_processor_id(), __func__, event, p->comm, p->pid,
				wts->window_start, rq->cpu, wrq->window_start);

	new_task = is_new_task(p);

	if (wts->enqueue_after_migration != 0) {
		wallclock = walt_sched_clock();
		migrate_busy_time_addition(p, cpu_of(rq), wallclock);
		wts->enqueue_after_migration = 0;
	}

	cpu_time = &wrq->grp_time;
	if (event == ADD_TASK) {
		migrate_type = RQ_TO_GROUP;

		src_curr_runnable_sum = &wrq->curr_runnable_sum;
		dst_curr_runnable_sum = &cpu_time->curr_runnable_sum;
		src_prev_runnable_sum = &wrq->prev_runnable_sum;
		dst_prev_runnable_sum = &cpu_time->prev_runnable_sum;

		src_nt_curr_runnable_sum = &wrq->nt_curr_runnable_sum;
		dst_nt_curr_runnable_sum = &cpu_time->nt_curr_runnable_sum;
		src_nt_prev_runnable_sum = &wrq->nt_prev_runnable_sum;
		dst_nt_prev_runnable_sum = &cpu_time->nt_prev_runnable_sum;

		if (*src_curr_runnable_sum < wts->curr_window_cpu[cpu]) {
			WALT_BUG(WALT_BUG_WALT, p,
				 "pid=%u CPU=%d event=%d src_crs=%llu is lesser than task_contrib=%u",
				 p->pid, cpu, event, *src_curr_runnable_sum,
				 wts->curr_window_cpu[cpu]);
			*src_curr_runnable_sum = wts->curr_window_cpu[cpu];
		}
		*src_curr_runnable_sum -= wts->curr_window_cpu[cpu];

		if (*src_prev_runnable_sum < wts->prev_window_cpu[cpu]) {
			WALT_BUG(WALT_BUG_WALT, p,
				 "pid=%u CPU=%d event=%d src_prs=%llu is lesser than task_contrib=%u",
				 p->pid, cpu, event, *src_prev_runnable_sum,
				 wts->prev_window_cpu[cpu]);
			*src_prev_runnable_sum = wts->prev_window_cpu[cpu];
		}
		*src_prev_runnable_sum -= wts->prev_window_cpu[cpu];

		if (new_task) {
			if (*src_nt_curr_runnable_sum < wts->curr_window_cpu[cpu]) {
				WALT_BUG(WALT_BUG_WALT, p,
					 "pid=%u CPU=%d event=%d src_nt_crs=%llu is lesser than task_contrib=%u",
					 p->pid, cpu, event,
					 *src_nt_curr_runnable_sum,
					 wts->curr_window_cpu[cpu]);
				*src_nt_curr_runnable_sum = wts->curr_window_cpu[cpu];
			}
			*src_nt_curr_runnable_sum -=
					wts->curr_window_cpu[cpu];

			if (*src_nt_prev_runnable_sum < wts->prev_window_cpu[cpu]) {
				WALT_BUG(WALT_BUG_WALT, p,
					 "pid=%u CPU=%d event=%d src_nt_prs=%llu is lesser than task_contrib=%u",
					 p->pid, cpu, event,
					 *src_nt_prev_runnable_sum,
					 wts->prev_window_cpu[cpu]);
				*src_nt_prev_runnable_sum = wts->prev_window_cpu[cpu];
			}
			*src_nt_prev_runnable_sum -=
					wts->prev_window_cpu[cpu];
		}

		update_cluster_load_subtractions(p, cpu,
				wrq->window_start, new_task);

	} else {
		migrate_type = GROUP_TO_RQ;

		src_curr_runnable_sum = &cpu_time->curr_runnable_sum;
		dst_curr_runnable_sum = &wrq->curr_runnable_sum;
		src_prev_runnable_sum = &cpu_time->prev_runnable_sum;
		dst_prev_runnable_sum = &wrq->prev_runnable_sum;

		src_nt_curr_runnable_sum = &cpu_time->nt_curr_runnable_sum;
		dst_nt_curr_runnable_sum = &wrq->nt_curr_runnable_sum;
		src_nt_prev_runnable_sum = &cpu_time->nt_prev_runnable_sum;
		dst_nt_prev_runnable_sum = &wrq->nt_prev_runnable_sum;

		if (*src_curr_runnable_sum < wts->curr_window) {
			WALT_BUG(WALT_BUG_WALT, p,
				 "WALT-UG pid=%u CPU=%d event=%d src_crs=%llu is lesser than task_contrib=%u",
				 p->pid, cpu, event, *src_curr_runnable_sum,
				 wts->curr_window);
			*src_curr_runnable_sum = wts->curr_window;
		}
		*src_curr_runnable_sum -= wts->curr_window;

		if (*src_prev_runnable_sum < wts->prev_window) {
			WALT_BUG(WALT_BUG_WALT, p,
				 "pid=%u CPU=%d event=%d src_prs=%llu is lesser than task_contrib=%u",
				 p->pid, cpu, event, *src_prev_runnable_sum,
				 wts->prev_window);
			*src_prev_runnable_sum = wts->prev_window;
		}
		*src_prev_runnable_sum -= wts->prev_window;

		if (new_task) {
			if (*src_nt_curr_runnable_sum < wts->curr_window) {
				WALT_BUG(WALT_BUG_WALT, p,
					 "pid=%u CPU=%d event=%d src_nt_crs=%llu is lesser than task_contrib=%u",
						p->pid, cpu, event,
						*src_nt_curr_runnable_sum,
						wts->curr_window);
				*src_nt_curr_runnable_sum = wts->curr_window;
			}
			*src_nt_curr_runnable_sum -= wts->curr_window;

			if (*src_nt_prev_runnable_sum < wts->prev_window) {
				WALT_BUG(WALT_BUG_WALT, p,
					 "pid=%u CPU=%d event=%d src_nt_prs=%llu is lesser than task_contrib=%u",
					 p->pid, cpu, event,
					 *src_nt_prev_runnable_sum,
					 wts->prev_window);
				*src_nt_prev_runnable_sum = wts->prev_window;
			}
			*src_nt_prev_runnable_sum -= wts->prev_window;
		}

		/*
		 * Need to reset curr/prev windows for all CPUs, not just the
		 * ones in the same cluster. Since inter cluster migrations
		 * did not result in the appropriate book keeping, the values
		 * per CPU would be inaccurate.
		 */
		for_each_possible_cpu(i) {
			wts->curr_window_cpu[i] = 0;
			wts->prev_window_cpu[i] = 0;
		}
	}

	*dst_curr_runnable_sum += wts->curr_window;
	*dst_prev_runnable_sum += wts->prev_window;
	if (new_task) {
		*dst_nt_curr_runnable_sum += wts->curr_window;
		*dst_nt_prev_runnable_sum += wts->prev_window;
	}

	/*
	 * When a task enter or exits a group, it's curr and prev windows are
	 * moved to a single CPU. This behavior might be sub-optimal in the
	 * exit case, however, it saves us the overhead of handling inter
	 * cluster migration fixups while the task is part of a related group.
	 */
	wts->curr_window_cpu[cpu] = wts->curr_window;
	wts->prev_window_cpu[cpu] = wts->prev_window;

	trace_sched_migration_update_sum(p, migrate_type, rq);
}

bool is_rtgb_active(void)
{
	struct walt_related_thread_group *grp;

	grp = lookup_related_thread_group(DEFAULT_CGROUP_COLOC_ID);
	return grp && grp->skip_min;
}

u64 get_rtgb_active_time(void)
{
	struct walt_related_thread_group *grp;
	u64 now = walt_sched_clock();

	grp = lookup_related_thread_group(DEFAULT_CGROUP_COLOC_ID);

	if (grp && grp->skip_min && grp->start_ktime_ts)
		return now - grp->start_ktime_ts;

	return 0;
}

static void walt_init_window_dep(void);
static void walt_tunables_fixup(void)
{
	if (likely(num_sched_clusters > 0))
		walt_update_group_thresholds();
	walt_init_window_dep();
}

static void walt_update_irqload(struct rq *rq)
{
	u64 irq_delta = 0;
	unsigned int nr_windows = 0;
	u64 cur_irq_time;
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));
	u64 last_irq_window = READ_ONCE(wrq->last_irq_window);

	if (wrq->window_start > last_irq_window)
		nr_windows = div64_u64(wrq->window_start - last_irq_window,
				       sched_ravg_window);

	/* Decay CPU's irqload by 3/4 for each window. */
	if (nr_windows < 10)
		wrq->avg_irqload = mult_frac(wrq->avg_irqload, 3, 4);
	else
		wrq->avg_irqload = 0;

	cur_irq_time = irq_time_read(cpu_of(rq));
	if (cur_irq_time > wrq->prev_irq_time)
		irq_delta = cur_irq_time - wrq->prev_irq_time;

	wrq->avg_irqload += irq_delta;
	wrq->prev_irq_time = cur_irq_time;

	if (nr_windows < SCHED_HIGH_IRQ_TIMEOUT)
		wrq->high_irqload = (wrq->avg_irqload >=
					walt_cpu_high_irqload);
	else
		wrq->high_irqload = false;
}

/**
 * __walt_irq_work_locked() - common function to process work
 * @is_migration: if true, performing migration work, else rollover
 * @is_asym_migration: if true, performing migration involving an asym cap sibling
 * @lock_cpus: mask of the cpus involved in the operation.
 *
 * In rq locked context, update the cluster group load and find
 * the load of the min cluster, while tracking the total aggregate
 * work load.  Update the cpufreq through the walt governor,
 * based upon the new load calculated.
 *
 * For the window rollover case lock_cpus will be all possible cpus,
 * and for migrations it will include the cpus from the two clusters
 * involved in the migration.
 */
static inline void __walt_irq_work_locked(bool is_migration, bool is_asym_migration,
				bool is_pipeline_sync_migration, struct cpumask *lock_cpus)
{
	struct walt_sched_cluster *cluster;
	struct rq *rq;
	int cpu;
	u64 wc;
	u64 total_grp_load = 0;
	unsigned long flags;
	struct walt_rq *wrq;

	wc = walt_sched_clock();
	if (!is_migration)
		walt_load_reported_window = atomic64_read(&walt_irq_work_lastq_ws);
	for_each_sched_cluster(cluster) {
		u64 aggr_grp_load = 0;

		raw_spin_lock(&cluster->load_lock);
		for_each_cpu(cpu, &cluster->cpus) {
			rq = cpu_rq(cpu);
			wrq = &per_cpu(walt_rq, cpu_of(rq));
			if (rq->curr) {
				/* only update ravg for locked cpus */
				if (cpumask_intersects(lock_cpus, &cluster->cpus)) {
					if (unlikely(!raw_spin_is_locked(&rq->__lock))) {
						printk_deferred("WALT-BUG %s unlocked cpu=%d is_migration=%d is_asym_migration=%d is_pipeline_sync_migration=%d lock_cpus=%*pbl suspended=%d last_clk=%llu stack[%pS <= %pS <= %pS]\n",
						__func__, rq->cpu, is_migration, is_asym_migration,
						is_pipeline_sync_migration,
						cpumask_pr_args(lock_cpus), walt_clock_suspended,
						sched_clock_last, (void *)CALLER_ADDR0,
						(void *)CALLER_ADDR1, (void *)CALLER_ADDR2);
						WALT_PANIC(1);
					}
					walt_update_task_ravg(rq->curr, rq,
							      TASK_UPDATE, wc, 0);
					account_load_subtractions(rq);
				}

				/* update aggr_grp_load for all clusters, all cpus */
				aggr_grp_load +=
					wrq->grp_time.prev_runnable_sum;
			}
		}
		raw_spin_unlock(&cluster->load_lock);

		cluster->aggr_grp_load = aggr_grp_load;
		total_grp_load += aggr_grp_load;
	}

	if (total_grp_load)
		rtgb_active = is_rtgb_active();
	else
		rtgb_active = false;

	if (!is_migration && sysctl_sched_user_hint && time_after(jiffies,
						sched_user_hint_reset_time))
		sysctl_sched_user_hint = 0;

	for_each_sched_cluster(cluster) {
		cpumask_t cluster_online_cpus;
		unsigned int num_cpus, i = 1;

		/* for migration, skip unnotified clusters */
		if (is_migration && !cpumask_intersects(lock_cpus, &cluster->cpus))
			continue;

		cpumask_and(&cluster_online_cpus, &cluster->cpus,
						cpu_online_mask);
		num_cpus = cpumask_weight(&cluster_online_cpus);
		for_each_cpu(cpu, &cluster_online_cpus) {
			int wflag = 0;

			rq = cpu_rq(cpu);
			wrq = &per_cpu(walt_rq, cpu_of(rq));

			if (is_migration) {
				if (wrq->notif_pending) {
					wrq->notif_pending = false;
					wflag |= WALT_CPUFREQ_IC_MIGRATION_BIT;
				}
				if (is_asym_migration)
					wflag |= WALT_CPUFREQ_ASYM_FIXUP_BIT;
				if (is_pipeline_sync_migration)
					wflag |= WALT_CPUFREQ_SHARED_RAIL_BIT;
			} else {
				wflag |= WALT_CPUFREQ_ROLLOVER_BIT;
			}
// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_MIGT_WALT
			if (walt_window_update_hooks) {
				if (!is_migration && !is_asym_migration
					&& walt_window_update_hooks(cpu)) {
					waltgov_run_callback(cpu_rq(cpu), wflag |
							WALT_CPUFREQ_CONTINUE_BIT);
					i++;
					if (!is_migration)
						walt_update_irqload(rq);
					continue;
				} else if ((!is_migration && !is_asym_migration)
				&& oem_flt_enable_hook && oem_flt_enable_hook()) {
					waltgov_run_callback(cpu_rq(cpu), wflag |
								WALT_CPUFREQ_CONTINUE_BIT);
					i++;
					if (!is_migration)
						walt_update_irqload(rq);
					continue;
				}
			}
#endif
// END Performance_TurboSched
			if (i == num_cpus)
				waltgov_run_callback(cpu_rq(cpu), wflag);
			else
				waltgov_run_callback(cpu_rq(cpu), wflag |
							WALT_CPUFREQ_CONTINUE_BIT);
			i++;

			if (!is_migration)
				walt_update_irqload(rq);
		}
	}
	// MIUI ADD: Performance_TurboSched
	#ifdef CONFIG_MI_SOC_DFLT
	if (!is_migration&&!is_asym_migration){
		trigger_mist_load_tracking(NULL);
	}
	#endif
	// END Performance_TurboSched
	/*
	 * If the window change request is in pending, good place to
	 * change sched_ravg_window since all rq locks are acquired.
	 *
	 * If the current window roll over is delayed such that the
	 * mark_start (current wallclock with which roll over is done)
	 * of the current task went past the window start with the
	 * updated new window size, delay the update to the next
	 * window roll over. Otherwise the CPU counters (prs and crs) are
	 * not rolled over properly as mark_start > window_start.
	 */
	if (!is_migration) {
		spin_lock_irqsave(&sched_ravg_window_lock, flags);
		wrq = &per_cpu(walt_rq, cpu_of(this_rq()));
		if ((sched_ravg_window != new_sched_ravg_window) &&
		    (wc < wrq->window_start + new_sched_ravg_window)) {
			sched_ravg_window_change_time = walt_sched_clock();
			trace_sched_ravg_window_change(sched_ravg_window,
					new_sched_ravg_window,
					sched_ravg_window_change_time);
			sched_ravg_window = new_sched_ravg_window;
// MIUI ADD: Game_TurboSched
#ifdef CONFIG_MIGT_WALT
			if (oem_get_ravg_window_hook)
				oem_get_ravg_window_hook(sched_ravg_window);
#endif
//END Game_TurboSched	
			walt_tunables_fixup();
		}
		spin_unlock_irqrestore(&sched_ravg_window_lock, flags);
	}
}

/**
 * irq_work_restrict_to_mig_clusters() - only allow notified clusters
 * @lock_cpus: mask of the cpus for which the runque should be locked.
 *
 * Remove cpus in clusters that are not part of the migration, using
 * the notif_pending flag to track.
 *
 * This is only valid for the migration irq work.
 */
static inline void irq_work_restrict_to_mig_clusters(cpumask_t *lock_cpus)
{
	struct walt_sched_cluster *cluster;
	struct rq *rq;
	struct walt_rq *wrq;
	int cpu;

	for_each_sched_cluster(cluster) {
		bool keep_locked = false;

		for_each_cpu(cpu, &cluster->cpus) {
			rq = cpu_rq(cpu);
			wrq = &per_cpu(walt_rq, cpu_of(rq));

			/* remove this cluster if it's not being notified */
			if (wrq->notif_pending) {
				keep_locked = true;
				break;
			}
		}
		if (!keep_locked)
			cpumask_andnot(lock_cpus, lock_cpus, &cluster->cpus);
	}
}

void update_cpu_capacity_helper(int cpu)
{
	unsigned long fmax_capacity = arch_scale_cpu_capacity(cpu);
	unsigned long thermal_pressure = arch_scale_thermal_pressure(cpu);
	unsigned long thermal_cap, old;
	struct walt_sched_cluster *cluster;
	struct rq *rq = cpu_rq(cpu);

	if (unlikely(walt_disabled))
		return;

	/*
	 * thermal_pressure = cpu_scale - curr_cap_as_per_thermal.
	 * so,
	 * curr_cap_as_per_thermal = cpu_scale - thermal_pressure.
	 */

	thermal_cap = fmax_capacity - thermal_pressure;

	cluster = cpu_cluster(cpu);
	/* reduce the fmax_capacity under cpufreq constraints */
	if (cluster->walt_internal_freq_limit != cluster->max_possible_freq)
		fmax_capacity = mult_frac(fmax_capacity,
					min(cluster->walt_internal_freq_limit, cluster->max_freq),
					cluster->max_possible_freq);

	old = rq->cpu_capacity_orig;
	rq->cpu_capacity_orig = min(fmax_capacity, thermal_cap);

	if (old != rq->cpu_capacity_orig)
		trace_update_cpu_capacity(cpu, fmax_capacity, rq->cpu_capacity_orig);
}

/*
 * The intention of this hook is to update cpu_capacity_orig as well as
 * (*capacity), otherwise we will end up capacity_of() > capacity_orig_of().
 */
static void android_rvh_update_cpu_capacity(void *unused, int cpu, unsigned long *capacity)
{
	unsigned long rt_pressure = arch_scale_cpu_capacity(cpu) - *capacity;

	update_cpu_capacity_helper(cpu);
	*capacity = max((int)(cpu_rq(cpu)->cpu_capacity_orig - rt_pressure), 0);
}

/*
 *	big_task_pid is used by the One Big Enqueue Task feature to track the
 *	number of big tasks enqueued on largest cluster.
 *	big_task_pid = {0, pid, -1}
 *	0 -> indicates there are 0 big tasks enqueued on the CPU
 *	pid -> indicates that there is one big task enqueued whose PID
 *	is pid
 *	-1 -> indicates that there is more than one big task enqueued
 *	on the CPU.
 *	The big_task_pid value is checked every window rollover
 *	and updated according to the rules above.
 */
DEFINE_PER_CPU(pid_t, big_task_pid);
bool is_obet;

/*
 * check_obet() needs to be called with all the rq locks held.
 * It resets per cpu big_task_pid and does cpu checks on a
 * single big task.
 */
static void check_obet(void)
{
	struct task_struct *p;
	int is_obet_temp = 0;
	int mid_cluster_cpu, cpu;

	if (num_sched_clusters < 2)
		return;

	mid_cluster_cpu = cpumask_first(&cpu_array[0][num_sched_clusters - 2]);

	for_each_cpu(cpu, &cpu_array[0][num_sched_clusters - 1]) {
		if (per_cpu(big_task_pid, cpu) == -1) {
			is_obet_temp = -1;
		} else if (per_cpu(big_task_pid, cpu) != 0) {
			if (is_obet_temp == 0) {
				is_obet_temp = per_cpu(big_task_pid, cpu);
			} else {
				if (is_obet_temp != per_cpu(big_task_pid, cpu))
					is_obet_temp = -1;
			}
		}
	}

	if (is_obet_temp == -1 || is_obet_temp == 0)
		is_obet = false;
	else
		is_obet = true;

	//reset per CPU big_task_pid for the upcoming window
	for_each_cpu(cpu, &cpu_array[0][num_sched_clusters - 1]) {
		pid_t pid = per_cpu(big_task_pid, cpu);

		if (pid) {
			int task_count = 0;
			int big_task_count = 0;

			list_for_each_entry(p, &(cpu_rq(cpu)->cfs_tasks),
					se.group_node) {
				task_count++;
				if (!task_fits_max(p, mid_cluster_cpu)) {
					big_task_count++;
					pid = p->pid;
					if (big_task_count == 2)
						break;
				}
				if (task_count == 10)
					break;
			}
			if (task_count == 10)
				per_cpu(big_task_pid, cpu) = -1;
			else if (big_task_count == 0)
				per_cpu(big_task_pid, cpu) = 0;
			else if (big_task_count == 1)
				per_cpu(big_task_pid, cpu) = pid;
			else
				per_cpu(big_task_pid, cpu) = -1;
		}
	}
}

static void check_obet_set_boost(void)
{
	static bool prev_is_obet;
	bool now_is_obet;

	//Determine if core_ctl boost is needed
	now_is_obet = is_obet;
	if (prev_is_obet != now_is_obet)
		core_ctl_set_cluster_boost(num_sched_clusters - 1, is_obet);
	prev_is_obet = now_is_obet;
}

#define CORE_UTIL_PERIOD 1000000000
static void walt_core_utilization(int cpu)
{
	static u64 sum[WALT_NR_CPUS];
	static u64 timestamp;
	static int nr_windows[WALT_NR_CPUS];
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu);
	u64 max_capacity = arch_scale_cpu_capacity(cpu);

	if (wrq->window_start > timestamp + CORE_UTIL_PERIOD) {
		sysctl_sched_walt_core_util[cpu] = sum[cpu] / nr_windows[cpu];
		sum[cpu] = 0;
		nr_windows[cpu] = 0;
		if (cpu == cpumask_last(cpu_online_mask))
			timestamp = wrq->window_start;
	}

	nr_windows[cpu]++;
	if (max_capacity < wrq->walt_stats.cumulative_runnable_avg_scaled)
		sum[cpu] += max_capacity;
	else
		sum[cpu] += wrq->walt_stats.cumulative_runnable_avg_scaled;
}

DEFINE_PER_CPU(u32, wakeup_ctr);
/**
 * walt_irq_work() - perform walt irq work for rollover and migration
 *
 * Process a workqueue call scheduled, while running in a hard irq
 * protected context.  Handle migration and window rollover work
 * with common functionality, and on window rollover ask core control
 * to decide if it needs to adjust the active cpus.
 */
static void walt_irq_work(struct irq_work *irq_work)
{
	cpumask_t lock_cpus;
	struct walt_rq *wrq;
	int level;
	int cpu;
	bool is_migration = false, is_asym_migration = false, is_pipeline_sync_migration = false;
	u32 wakeup_ctr_sum = 0;
	struct walt_sched_cluster *cluster;
	bool need_assign_heavy = false;

	if (irq_work == &walt_migration_irq_work)
		is_migration = true;

	cpumask_copy(&lock_cpus, cpu_possible_mask);

	if (is_migration) {
		irq_work_restrict_to_mig_clusters(&lock_cpus);

		/*
		 * if the notif_pending was handled by a previous
		 * walt_irq_work invocation, there is no migration
		 * work.
		 */
		if (cpumask_empty(&lock_cpus))
			return;

		if (pipeline_in_progress() && cpumask_intersects(&lock_cpus, &pipeline_sync_cpus)) {
			cpumask_or(&lock_cpus, &lock_cpus, &pipeline_sync_cpus);
			is_pipeline_sync_migration = true;
		}
		if (!is_state1() &&
				cpumask_intersects(&lock_cpus, &asym_cap_sibling_cpus)) {
			cpumask_or(&lock_cpus, &lock_cpus, &asym_cap_sibling_cpus);
			is_asym_migration = true;
		}
	}

	level = 0;
	for_each_cpu(cpu, &lock_cpus) {
		if (level == 0)
			raw_spin_lock(&cpu_rq(cpu)->__lock);
		else
			raw_spin_lock_nested(&cpu_rq(cpu)->__lock, level);
		level++;
	}

	__walt_irq_work_locked(is_migration, is_asym_migration,
			is_pipeline_sync_migration, &lock_cpus);

	if (!is_migration) {
		for_each_cpu(cpu, cpu_online_mask) {
			wakeup_ctr_sum += per_cpu(wakeup_ctr, cpu);
			per_cpu(wakeup_ctr, cpu) = 0;

			walt_core_utilization(cpu);
			set_cpu_flag(cpu, CPU_FIRST_ENQ_IN_WINDOW, 0);
		}

		check_obet();
	}

	for_each_cpu(cpu, &lock_cpus)
		raw_spin_unlock(&cpu_rq(cpu)->__lock);

	if (!is_migration) {
		wrq = &per_cpu(walt_rq, cpu_of(this_rq()));
		need_assign_heavy = pipeline_check(wrq);
		core_ctl_check(wrq->window_start, wakeup_ctr_sum);
		pipeline_rearrange(wrq, need_assign_heavy);
		for_each_sched_cluster(cluster) {
			update_smart_freq_legacy_reason_hyst_time(cluster);
		}
		check_obet_set_boost();
	}
}

void walt_rotation_checkpoint(int nr_big)
{
	int i;
	bool prev = walt_rotation_enabled;

	if (!hmp_capable())
		return;

	if (!sysctl_sched_walt_rotate_big_tasks || sched_boost_type != NO_BOOST) {
		walt_rotation_enabled = 0;
		return;
	}

	walt_rotation_enabled = nr_big >= num_possible_cpus();

	for (i = 0; i < num_sched_clusters; i++) {
		if (walt_rotation_enabled && !prev)
			freq_cap[HIGH_PERF_CAP][i] = high_perf_cluster_freq_cap[i];
		else if (!walt_rotation_enabled && prev)
			freq_cap[HIGH_PERF_CAP][i] = FREQ_QOS_MAX_DEFAULT_VALUE;
	}

	update_smart_freq_capacities();
}

void walt_fill_ta_data(struct core_ctl_notif_data *data)
{
	struct walt_related_thread_group *grp;
	unsigned long flags;
	u64 total_demand = 0, wallclock;
	int min_cap_cpu, scale = 1024;
	struct walt_sched_cluster *cluster;
	int i = 0;
	struct walt_task_struct *wts;

	grp = lookup_related_thread_group(DEFAULT_CGROUP_COLOC_ID);

	raw_spin_lock_irqsave(&grp->lock, flags);
	if (list_empty(&grp->tasks)) {
		raw_spin_unlock_irqrestore(&grp->lock, flags);
		goto fill_util;
	}

	wallclock = walt_sched_clock();

	list_for_each_entry(wts, &grp->tasks, grp_list) {
		if (wts->mark_start < wallclock -
		    (sched_ravg_window * RAVG_HIST_SIZE))
			continue;

		total_demand += wts->coloc_demand;
	}

	raw_spin_unlock_irqrestore(&grp->lock, flags);

	/*
	 * Scale the total demand to the lowest capacity CPU and
	 * convert into percentage.
	 *
	 * P = total_demand/sched_ravg_window * 1024/scale * 100
	 */

	min_cap_cpu = cpumask_first(&cpu_array[0][0]);
	if (min_cap_cpu != -1)
		scale = arch_scale_cpu_capacity(min_cap_cpu);

	data->coloc_load_pct = div64_u64(total_demand * 1024 * 100,
			       (u64)sched_ravg_window * scale);

fill_util:
	for_each_sched_cluster(cluster) {
		int fcpu = cluster_first_cpu(cluster);

		if (i == MAX_CLUSTERS)
			break;

		scale = arch_scale_cpu_capacity(fcpu);
		data->ta_util_pct[i] = div64_u64(cluster->aggr_grp_load * 1024 *
				       100, (u64)sched_ravg_window * scale);

		scale = arch_scale_freq_capacity(fcpu);
		data->cur_cap_pct[i] = (scale * 100)/1024;
		i++;
	}
}

#define INIT_TASK_LOAD_PCT 15
static void walt_init_window_dep(void)
{
	walt_scale_demand_divisor = sched_ravg_window >> SCHED_CAPACITY_SHIFT;
	/* default task to 15 pct */
	sched_init_task_load_windows = div64_u64((u64)INIT_TASK_LOAD_PCT *
			(u64)sched_ravg_window, 100);
	sched_init_task_load_windows_scaled =
		scale_time_to_util(sched_init_task_load_windows);

	walt_cpu_high_irqload = div64_u64((u64)sched_ravg_window * 95, (u64) 100);
}

static void walt_init_once(void)
{
	init_irq_work(&walt_migration_irq_work, walt_irq_work);
	init_irq_work(&walt_cpufreq_irq_work, walt_irq_work);
	walt_init_window_dep();
}

static void walt_sched_init_rq(struct rq *rq)
{
	int j;
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));

	cpumask_set_cpu(cpu_of(rq), &wrq->freq_domain_cpumask);

	wrq->walt_stats.cumulative_runnable_avg_scaled = 0;
	wrq->prev_window_size = sched_ravg_window;
	wrq->window_start = 0;
	wrq->walt_stats.nr_big_tasks = 0;
	wrq->walt_stats.nr_trailblazer_tasks = 0;
	wrq->walt_flags = 0;
	wrq->avg_irqload = 0;
	wrq->prev_irq_time = 0;
	wrq->last_irq_window = 0;
	wrq->high_irqload = false;
	wrq->task_exec_scale = 1024;
	wrq->push_task = NULL;
	wrq->lrb_pipeline_start_time = 0;

	wrq->curr_runnable_sum = wrq->prev_runnable_sum = 0;
	wrq->nt_curr_runnable_sum = wrq->nt_prev_runnable_sum = 0;
	memset(&wrq->grp_time, 0, sizeof(struct group_cpu_time));
	wrq->old_busy_time = 0;
	wrq->old_estimated_time = 0;
	wrq->walt_stats.pred_demands_sum_scaled = 0;
	wrq->walt_stats.nr_rtg_high_prio_tasks = 0;
	wrq->ed_task = NULL;
	wrq->curr_table = 0;
	wrq->prev_top = 0;
	wrq->curr_top = 0;
	wrq->last_cc_update = 0;
	wrq->cycles = 0;
	for (j = 0; j < NUM_TRACKED_WINDOWS; j++) {
		memset(&wrq->load_subs[j], 0,
				sizeof(struct load_subtractions));
		wrq->top_tasks[j] = kcalloc(NUM_LOAD_INDICES,
				sizeof(u8), GFP_ATOMIC | GFP_NOWAIT);
		/* No other choice */
		BUG_ON(!wrq->top_tasks[j]);
		clear_top_tasks_bitmap(wrq->top_tasks_bitmap[j]);
	}
	wrq->notif_pending = false;

	wrq->num_mvp_tasks = 0;
	INIT_LIST_HEAD(&wrq->mvp_tasks);
	wrq->mvp_arrival_time = 0;
	wrq->mvp_throttle_time = 0;
	wrq->skip_mvp = false;
	wrq->uclamp_limit[UCLAMP_MIN] = 0;
	wrq->uclamp_limit[UCLAMP_MAX] = SCHED_CAPACITY_SCALE;
}

void sched_window_nr_ticks_change(void)
{
	unsigned long flags;

	spin_lock_irqsave(&sched_ravg_window_lock, flags);
	new_sched_ravg_window = mult_frac(sysctl_sched_ravg_window_nr_ticks,
						NSEC_PER_SEC, HZ);
	spin_unlock_irqrestore(&sched_ravg_window_lock, flags);
}

static void
walt_inc_cumulative_runnable_avg(struct rq *rq, struct task_struct *p)
{
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	fixup_cumulative_runnable_avg(rq, p, &wrq->walt_stats, wts->demand_scaled,
					wts->pred_demand_scaled);

// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_METIS_WALT
	metis_tp_fixup_cumulative_runnable_avg_hook(rq, p, wts->demand_scaled,
				wts->pred_demand_scaled, LAYERED_LOAD_INC,
				wrq->walt_stats.cumulative_runnable_avg_scaled,
				wrq->walt_stats.pred_demands_sum_scaled);
#endif
// END Performance_TurboSched
}

static void
walt_dec_cumulative_runnable_avg(struct rq *rq, struct task_struct *p)
{
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	fixup_cumulative_runnable_avg(rq, p, &wrq->walt_stats,
				      -(s64)wts->demand_scaled,
				      -(s64)wts->pred_demand_scaled);

// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_METIS_WALT
	metis_tp_fixup_cumulative_runnable_avg_hook(rq, p, -(s64)wts->demand_scaled,
				-(s64)wts->pred_demand_scaled, LAYERED_LOAD_DEC,
				wrq->walt_stats.cumulative_runnable_avg_scaled,
				wrq->walt_stats.pred_demands_sum_scaled);
#endif
// END Performance_TurboSched
}

static void android_rvh_wake_up_new_task(void *unused, struct task_struct *new)
{
	if (unlikely(walt_disabled))
		return;
	init_new_task_load(new);
	add_new_task_to_grp(new);
}

static void walt_cpu_frequency_limits(void *unused, struct cpufreq_policy *policy)
{
	int cpu;

	if (unlikely(walt_disabled))
		return;

	cpu_cluster(policy->cpu)->max_freq = policy->max;
	for_each_cpu(cpu, policy->related_cpus)
		update_cpu_capacity_helper(cpu);
}

static void android_rvh_sched_cpu_starting(void *unused, int cpu)
{
	if (unlikely(walt_disabled))
		return;
	clear_walt_request(cpu);
}

static void android_rvh_sched_cpu_dying(void *unused, int cpu)
{
	if (unlikely(walt_disabled))
		return;
	clear_walt_request(cpu);
}

static void android_rvh_set_task_cpu(void *unused, struct task_struct *p, unsigned int new_cpu)
{
	if (unlikely(walt_disabled))
		return;

	migrate_busy_time_subtraction(p, (int) new_cpu);

	if (!cpumask_test_cpu(new_cpu, p->cpus_ptr))
		WALT_BUG(WALT_BUG_WALT, p, "selecting unaffined cpu=%d comm=%s(%d) affinity=0x%lx",
			 new_cpu, p->comm, p->pid, (*(cpumask_bits(p->cpus_ptr))));

	if (!p->in_execve &&
	    is_compat_thread(task_thread_info(p)) &&
	    !cpumask_test_cpu(new_cpu, system_32bit_el0_cpumask()))
		WALT_BUG(WALT_BUG_WALT, p,
			 "selecting non 32 bit cpu=%d comm=%s(%d) 32bit_cpus=0x%lx",
			 new_cpu, p->comm, p->pid, (*(cpumask_bits(system_32bit_el0_cpumask()))));
}

static void android_rvh_new_task_stats(void *unused, struct task_struct *p)
{
	if (unlikely(walt_disabled))
		return;
	mark_task_starting(p);
}

static void android_rvh_account_irq(void *unused, struct task_struct *curr, int cpu,
					s64 delta, bool start)
{
	struct rq *rq;
	unsigned long flags;
	struct walt_rq *wrq;

	if (unlikely(walt_disabled))
		return;

	if (!walt_is_idle_task(curr))
		return;

	rq = cpu_rq(cpu);
	wrq = &per_cpu(walt_rq, cpu_of(rq));

	if (start) {
		if (!wrq->window_start)
			return;

		/* We're here without rq->lock held, IRQ disabled */
		raw_spin_lock(&rq->__lock);
		update_task_cpu_cycles(curr, cpu, walt_sched_clock());
		raw_spin_unlock(&rq->__lock);
	} else {
		raw_spin_lock_irqsave(&rq->__lock, flags);
		walt_update_task_ravg(curr, rq, IRQ_UPDATE, walt_sched_clock(), delta);
		raw_spin_unlock_irqrestore(&rq->__lock, flags);

		wrq->last_irq_window = wrq->window_start;
	}
}

static void android_rvh_flush_task(void *unused, struct task_struct *p)
{
	if (unlikely(walt_disabled))
		return;
	walt_task_dead(p);
}

static void android_rvh_enqueue_task(void *unused, struct rq *rq,
		struct task_struct *p, int flags)
{
	u64 wallclock;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));
	bool double_enqueue = false;
	int mid_cluster_cpu;
// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_METIS_WALT
    int mod;
    for (mod = 0; mod < MI_SCHED_TYPE; mod++) {
        if (mi_enqueue_task_fair_func[mod].f)
            mi_enqueue_task_fair_func[mod].f(rq, p, flags);
    }
#endif
// END Performance_TurboSched

	if (unlikely(walt_disabled))
		return;

	walt_lockdep_assert_rq(rq, p);

	if (flags & ENQUEUE_WAKEUP)
		per_cpu(wakeup_ctr, cpu_of(rq)) += 1;

	if (!is_per_cpu_kthread(p))
		wrq->enqueue_counter++;

	if (task_thread_info(p)->cpu != cpu_of(rq))
		WALT_BUG(WALT_BUG_UPSTREAM, p, "enqueuing on rq %d when task->cpu is %d\n",
				cpu_of(rq), task_thread_info(p)->cpu);

	/* catch double enqueue */
	if (wts->prev_on_rq == 1) {
		WALT_BUG(WALT_BUG_UPSTREAM, p, "double enqueue detected: task_cpu=%d new_cpu=%d\n",
			 task_cpu(p), cpu_of(rq));
		double_enqueue = true;
	}

	wallclock = walt_rq_clock(rq);
	if (wts->enqueue_after_migration != 0) {
		wallclock = walt_sched_clock();
		migrate_busy_time_addition(p, cpu_of(rq), wallclock);
		wts->enqueue_after_migration = 0;
	}

	wts->prev_on_rq = 1;
	wts->prev_on_rq_cpu = cpu_of(rq);

	wts->last_enqueued_ts = wallclock;
	sched_update_nr_prod(rq->cpu, 1);

	if (walt_fair_task(p)) {
		wts->misfit = !task_fits_max(p, rq->cpu);
		if (!double_enqueue)
			inc_rq_walt_stats(rq, p);
		walt_cfs_enqueue_task(rq, p);
	}

	if (!double_enqueue)
		walt_inc_cumulative_runnable_avg(rq, p);

// MIUI ADD: Performance_TurboSched
#ifdef  CONFIG_METIS_WALT
	if ((flags & ENQUEUE_WAKEUP) && oem_rvh_try_to_wake_up_success(p))
        	waltgov_run_callback(rq, OEM_CPUFREQ_UPDATE);
	else {
		if ((flags & ENQUEUE_WAKEUP)) {
			if (walt_flag_test(p, WALT_TRAILBLAZER_BIT))
				waltgov_run_callback(rq, WALT_CPUFREQ_TRAILBLAZER_BIT);
			else if (do_pl_notif(rq))
				waltgov_run_callback(rq, WALT_CPUFREQ_PL_BIT);
		}
    }
#else
	if ((flags & ENQUEUE_WAKEUP) && walt_flag_test(p, WALT_TRAILBLAZER_BIT)) {
		waltgov_run_callback(rq, WALT_CPUFREQ_TRAILBLAZER_BIT);
	} else if (((flags & ENQUEUE_WAKEUP) ||
			!is_cpu_flag_set(cpu_of(rq), CPU_FIRST_ENQ_IN_WINDOW)) && do_pl_notif(rq)) {
		waltgov_run_callback(rq, WALT_CPUFREQ_PL_BIT);
	} else if (walt_feat(WALT_FEAT_UCLAMP_FREQ_BIT)) {
		unsigned long min, max;

		min = uclamp_rq_get(rq, UCLAMP_MIN);
		max = uclamp_rq_get(rq, UCLAMP_MAX);
		if ((wrq->uclamp_limit[UCLAMP_MIN] != min) ||
		    (wrq->uclamp_limit[UCLAMP_MAX] != max)) {
			wrq->uclamp_limit[UCLAMP_MIN] = min;
			wrq->uclamp_limit[UCLAMP_MAX] = max;
			waltgov_run_callback(rq, WALT_CPUFREQ_UCLAMP_BIT);
		}
	}
#endif
// END Performance_TurboSched

	set_cpu_flag(cpu_of(rq), CPU_FIRST_ENQ_IN_WINDOW, 1);
	if (num_sched_clusters >= 2) {
		mid_cluster_cpu = cpumask_first(
				&cpu_array[0][num_sched_clusters - 2]);
		if (is_max_possible_cluster_cpu(rq->cpu) &&
				!task_fits_max(p, mid_cluster_cpu)) {
			if (!per_cpu(big_task_pid, rq->cpu))
				per_cpu(big_task_pid, rq->cpu) = p->pid;
			else if (p->pid != per_cpu(big_task_pid, rq->cpu))
				per_cpu(big_task_pid, rq->cpu) = -1;
		}
	}

	trace_sched_enq_deq_task(p, 1, cpumask_bits(p->cpus_ptr)[0], is_mvp(wts));
}

static void android_rvh_dequeue_task(void *unused, struct rq *rq,
		struct task_struct *p, int flags)
{
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	bool double_dequeue = false;

// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_METIS_WALT
    int mod;
    for (mod = 0; mod < MI_SCHED_TYPE; mod++) {
        if (mi_dequeue_task_fair_func[mod].f)
            mi_dequeue_task_fair_func[mod].f(rq, p, flags);
    }
#endif
// END Performance_TurboSched

	if (unlikely(walt_disabled))
		return;

	walt_lockdep_assert_rq(rq, p);

	/*
	 * a task can be enqueued before walt is started, and dequeued after.
	 * therefore the check to ensure that prev_on_rq_cpu is needed to prevent
	 * an invalid failure.
	 */
	if (wts->prev_on_rq_cpu >= 0 && wts->prev_on_rq_cpu != cpu_of(rq) &&
			walt_flag_test(p, WALT_INIT_BIT))
		WALT_BUG(WALT_BUG_UPSTREAM, p, "dequeue cpu %d not same as enqueue %d\n",
			 cpu_of(rq), wts->prev_on_rq_cpu);

	/* no longer on a cpu */
	wts->prev_on_rq_cpu = -1;

	/* catch double deq */
	if (wts->prev_on_rq == 2) {
		WALT_BUG(WALT_BUG_UPSTREAM, p, "double dequeue detected: task_cpu=%d new_cpu=%d\n",
			 task_cpu(p), cpu_of(rq));
		double_dequeue = true;
	}

	wts->prev_on_rq = 2;
	if (p == wrq->ed_task)
		is_ed_task_present(rq, walt_rq_clock(rq), p);

	sched_update_nr_prod(rq->cpu, -1);

	if (walt_fair_task(p)) {
		if (!double_dequeue)
			dec_rq_walt_stats(rq, p);
		walt_cfs_dequeue_task(rq, p);
	}

	if (!double_dequeue)
		walt_dec_cumulative_runnable_avg(rq, p);

	if (walt_feat(WALT_FEAT_UCLAMP_FREQ_BIT)) {
		unsigned long min, max;

		min = uclamp_rq_get(rq, UCLAMP_MIN);
		max = uclamp_rq_get(rq, UCLAMP_MAX);
		if ((wrq->uclamp_limit[UCLAMP_MIN] != min) ||
		    (wrq->uclamp_limit[UCLAMP_MAX] != max)) {
			wrq->uclamp_limit[UCLAMP_MIN] = min;
			wrq->uclamp_limit[UCLAMP_MAX] = max;
			waltgov_run_callback(rq, WALT_CPUFREQ_UCLAMP_BIT);
		}
	}
	trace_sched_enq_deq_task(p, 0, cpumask_bits(p->cpus_ptr)[0], is_mvp(wts));
}

static void android_rvh_update_misfit_status(void *unused, struct task_struct *p,
		struct rq *rq, bool *need_update)
{
	struct walt_task_struct *wts;
	struct walt_rq *wrq;
	bool old_misfit, misfit;
	int change;

	if (unlikely(walt_disabled))
		return;
	*need_update = false;

	if (!p) {
		rq->misfit_task_load = 0;
		return;
	}

	wrq = &per_cpu(walt_rq, cpu_of(rq));
	wts = (struct walt_task_struct *) p->android_vendor_data1;
	old_misfit = wts->misfit;

	if (task_fits_max(p, rq->cpu))
		rq->misfit_task_load = 0;
	else
		rq->misfit_task_load = task_util(p);

	misfit = rq->misfit_task_load;

	change = misfit - old_misfit;
	if (change) {
		sched_update_nr_prod(rq->cpu, 0);
		wts->misfit = misfit;
		wrq->walt_stats.nr_big_tasks += change;
		BUG_ON(wrq->walt_stats.nr_big_tasks < 0);
	}
}

/* utility function to update walt signals at wakeup */
static void android_rvh_try_to_wake_up(void *unused, struct task_struct *p)
{
	struct rq *rq = cpu_rq(task_cpu(p));
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	struct rq_flags rf;
	u64 wallclock;
	unsigned int old_load;
	struct walt_related_thread_group *grp = NULL;

// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_METIS_WALT
	int mod;
	for (mod = 0; mod < MI_SCHED_TYPE; mod++) {
		if (mi_try_to_wake_up_func[mod].f)
			mi_try_to_wake_up_func[mod].f(NULL, p);
	}
#endif
// END Performance_TurboSched

	if (unlikely(walt_disabled))
		return;
	rq_lock_irqsave(rq, &rf);
	old_load = task_load(p);
	wallclock = walt_sched_clock();

	/*
	 * Once task does a sleep(not the yield induce sleep)
	 * reset the flag, to ensure task is no longer qualified
	 * as frequent yielder.
	 * i.e. task needs to qualify again as frequent yielder.
	 */
	if (!(wts->yield_state & YIELD_INDUCED_SLEEP))
		wts->yield_state = 0;
	else
		wts->yield_state &= YIELD_CNT_MASK;

	if (walt_is_idle_task(rq->curr) && p->in_iowait)
		walt_update_task_ravg(rq->curr, rq, TASK_UPDATE, wallclock, 0);
	walt_update_task_ravg(p, rq, TASK_WAKE, wallclock, 0);
	note_task_waking(p, wallclock);
	rq_unlock_irqrestore(rq, &rf);

	rcu_read_lock();
	grp = task_related_thread_group(p);
	if (update_preferred_cluster(grp, p, old_load, false))
		set_preferred_cluster(grp);
	rcu_read_unlock();
}

static u64 tick_sched_clock;
static DECLARE_COMPLETION(tick_sched_clock_completion);

DEFINE_PER_CPU(unsigned long, intr_cnt);
DEFINE_PER_CPU(unsigned long, cycle_cnt);
DEFINE_PER_CPU(unsigned int, ipc_level);
DEFINE_PER_CPU(unsigned long, ipc_cnt);
DEFINE_PER_CPU(u64, last_ipc_update);
DEFINE_PER_CPU(u64, ipc_deactivate_ns);
DEFINE_PER_CPU(bool, tickless_mode);
static unsigned long calculate_ipc(int cpu)
{
	unsigned long amu_cnt, delta_cycl = 0, delta_intr = 0;
	unsigned long prev_cycl_cnt = per_cpu(cycle_cnt, cpu);
	unsigned long prev_intr_cnt = per_cpu(intr_cnt, cpu);
	unsigned long ipc = 0;
	struct walt_sched_cluster *cluster = cpu_cluster(cpu);

	amu_cnt = read_sysreg_s(SYS_AMEVCNTR0_CORE_EL0);
	delta_cycl = amu_cnt - prev_cycl_cnt;
	per_cpu(cycle_cnt, cpu) = amu_cnt;
	amu_cnt = read_sysreg_s(SYS_AMEVCNTR0_INST_RET_EL0);
	per_cpu(intr_cnt, cpu) = amu_cnt;
	delta_intr = amu_cnt - prev_intr_cnt;
	if (prev_cycl_cnt && delta_cycl > cluster->smart_freq_info->min_cycles)
		ipc = (delta_intr * 100) / delta_cycl;

	per_cpu(ipc_cnt, cpu) = ipc;
	per_cpu(last_ipc_update, cpu) = cpu_rq(cpu)->clock;
	trace_ipc_update(cpu, per_cpu(cycle_cnt, cpu), per_cpu(intr_cnt, cpu),
			 per_cpu(ipc_cnt, cpu), per_cpu(last_ipc_update, cpu),
			 per_cpu(ipc_deactivate_ns, cpu), cpu_rq(cpu)->clock);

	return ipc;
}

static void android_rvh_tick_entry(void *unused, struct rq *rq)
{
	u64 wallclock;

	if (unlikely(walt_disabled))
		return;

	walt_lockdep_assert_rq(rq, NULL);
	wallclock = walt_rq_clock(rq);

	walt_update_task_ravg(rq->curr, rq, TASK_UPDATE, wallclock, 0);

	if (is_ed_task_present(rq, wallclock, NULL))
		waltgov_run_callback(rq, WALT_CPUFREQ_EARLY_DET_BIT);
}

bool is_sbt_or_oscillate(void)
{
	return now_is_sbt || (oscillate_cpu != -1);
}

bool should_boost_bus_dcvs(void)
{
	trace_sched_boost_bus_dcvs(oscillate_cpu);

	return (oscillate_cpu != -1) || is_storage_boost();
}
EXPORT_SYMBOL_GPL(should_boost_bus_dcvs);

/*
 * oscillate_cpu = {-1, cpu} tells if system is currently rotating a big
 * task between Prime CPUs and on which CPU the big task is currently
 * executing.
 * If it is -1, no big task oscillation is occurring.
 */
int oscillate_cpu = -1;

bool should_oscillate(unsigned int busy_cpu, int *no_oscillate_reason)
{
	int cpu;
	int busy_cpu_count = 0;

	if (busy_cpu >= nr_cpu_ids) {
		*no_oscillate_reason = 1;
		return false;
	}

	if (!is_obet) {
		*no_oscillate_reason = 2;
		return false;
	}

	if (!is_max_possible_cluster_cpu(busy_cpu)) {
		*no_oscillate_reason = 3;
		return false;
	}

	if (cpumask_weight(&cpu_array[0][num_sched_clusters - 1]) == 1) {
		*no_oscillate_reason = 4;
		return false;
	}

	for_each_cpu(cpu, &cpu_array[0][num_sched_clusters - 1]) {
		busy_cpu_count += !available_idle_cpu(cpu);
	}
	if (busy_cpu_count != 1) {
		*no_oscillate_reason = 5;
		return false;
	}

	return true;
}

static void android_vh_scheduler_tick(void *unused, struct rq *rq)
{
	struct walt_related_thread_group *grp;
	unsigned int old_load, last_ipc_level, curr_ipc_level;
	unsigned long ipc;
	int i, cpu = cpu_of(rq);
	struct walt_sched_cluster *cluster;
	struct smart_freq_cluster_info *smart_freq_info;
	u64 last_deactivate_ns;
	bool inform_governor;

	if (!tick_sched_clock) {
		/*
		 * Let the window begin 20us prior to the tick,
		 * that way we are guaranteed a rollover when the tick occurs.
		 * Use rq->clock directly instead of rq_clock() since
		 * we do not have the rq lock and
		 * rq->clock was updated in the tick callpath.
		 */
		tick_sched_clock = rq->clock - 20000;
		complete(&tick_sched_clock_completion);
	}

	if (unlikely(walt_disabled))
		return;

	old_load = task_load(rq->curr);
	rcu_read_lock();
	grp = task_related_thread_group(rq->curr);
	if (update_preferred_cluster(grp, rq->curr, old_load, true))
		set_preferred_cluster(grp);
	rcu_read_unlock();

	walt_lb_tick(rq);

	/* IPC based smart FMAX */
	cluster = cpu_cluster(cpu);
	smart_freq_info = cluster->smart_freq_info;

	if (smart_freq_init_done &&
		smart_freq_info->smart_freq_ipc_participation_mask & IPC_PARTICIPATION
		&& IS_ENABLED(CONFIG_ARM64_AMU_EXTN)) {
		last_ipc_level = per_cpu(ipc_level, cpu);
		last_deactivate_ns = per_cpu(ipc_deactivate_ns, cpu);
		ipc = calculate_ipc(cpu);
		for (i = 0; i < SMART_FMAX_IPC_MAX; i++)
			if (ipc < smart_freq_info->ipc_reason_config[i].ipc)
				break;

		if (i >= SMART_FMAX_IPC_MAX)
			i = SMART_FMAX_IPC_MAX - 1;

		curr_ipc_level = i;
		if ((curr_ipc_level != last_ipc_level) || per_cpu(tickless_mode, cpu))
			inform_governor = true;

		if ((curr_ipc_level < last_ipc_level) &&
		    (smart_freq_info->ipc_reason_config[last_ipc_level].hyst_ns > 0)) {
			if (!last_deactivate_ns) {
				per_cpu(ipc_deactivate_ns, cpu) = rq->clock;
				inform_governor = false;
			} else {
				u64 delta = rq->clock - last_deactivate_ns;

				if (smart_freq_info->ipc_reason_config[last_ipc_level].hyst_ns >
					delta)
					inform_governor = false;
			}
		}

		if (inform_governor) {
			per_cpu(ipc_level, cpu) = curr_ipc_level;
			per_cpu(ipc_deactivate_ns, cpu) = 0;
			waltgov_run_callback(rq, WALT_CPUFREQ_SMART_FREQ_BIT);
		}
	}
// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_METIS_WALT
    mi_scheduler_tick_hook(rq);
#endif
// END Performance_TurboSched
}

static void android_rvh_schedule(void *unused, struct task_struct *prev,
		struct task_struct *next, struct rq *rq)
{
	u64 wallclock;
	struct walt_task_struct *wts = (struct walt_task_struct *) prev->android_vendor_data1;
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));

	if (unlikely(walt_disabled))
		return;

	wallclock = walt_rq_clock(rq);

	/*
	 * reset mvp arrival time as we are switching to non-CFS task
	 * If rq is already in MVP throttling state then continue with
	 * throttling until throttling time expires.
	 */
	if (!walt_fair_task(next))
		wrq->mvp_arrival_time = 0;

	if (likely(prev != next)) {
		if (!prev->on_rq)
			wts->last_sleep_ts = wallclock;
		walt_update_task_ravg(prev, rq, PUT_PREV_TASK, wallclock, 0);
		walt_update_task_ravg(next, rq, PICK_NEXT_TASK, wallclock, 0);
	} else {
		walt_update_task_ravg(prev, rq, TASK_UPDATE, wallclock, 0);
	}
// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_METIS_WALT
	mi_schedule_hook(prev, next, rq);
	metis_schlat_schedule_hook(prev, next, rq->cpu);
#endif

#ifdef CONFIG_MI_SOC_DFLT
	mi_dflt_schedule_hook();
#endif
// END Performance_TurboSched
}

static void android_rvh_sched_fork_init(void *unused, struct task_struct *p)
{
	if (unlikely(walt_disabled))
		return;

// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_MI_SCHED_WALT
	mi_task_fork(NULL, p);
#endif
// END Performance_TurboSched
	__sched_fork_init(p);
}

static void android_rvh_ttwu_cond(void *unused, int cpu, bool *cond)
{
	if (unlikely(walt_disabled))
		return;
	*cond = (sysctl_sched_many_wakeup_threshold < WALT_MANY_WAKEUP_DEFAULT) &&
			(cpu != smp_processor_id());
}

static void android_rvh_sched_exec(void *unused, bool *cond)
{
	if (unlikely(walt_disabled))
		return;
	*cond = true;
}

static void android_rvh_build_perf_domains(void *unused, bool *eas_check)
{
	if (unlikely(walt_disabled))
		return;
	*eas_check = true;
}

static void android_rvh_update_thermal_stats(void *unused, int cpu)
{
	if (unlikely(walt_disabled))
		return;
	update_cpu_capacity_helper(cpu);
}

static DECLARE_COMPLETION(rebuild_domains_completion);
static void rebuild_sd_workfn(struct work_struct *work);
static DECLARE_WORK(rebuild_sd_work, rebuild_sd_workfn);

/** rebuild_sd_workfn
 *
 * rebuild the sched domains (and therefore the perf
 * domains). It is absolutely necessary that the
 * em_pds are created for each cpu device before
 * proceeding, and this must complete for walt to
 * function properly.
 */
static void rebuild_sd_workfn(struct work_struct *work)
{
	int cpu;
	struct device *cpu_dev;

	for_each_possible_cpu(cpu) {
		cpu_dev = get_cpu_device(cpu);
		if (cpu_dev->em_pd)
			continue;

		WARN_ONCE(true, "must wait for perf domains to be created");
		schedule_work(&rebuild_sd_work);

		/* do not rebuild domains yet, and do not complete this action */
		return;
	}

	rebuild_sched_domains();
	complete(&rebuild_domains_completion);
}

unsigned int walt_sched_yield_counter;
static void walt_do_sched_yield(void *unused, struct rq *rq)
{
	struct task_struct *curr = rq->curr;
	struct walt_task_struct *wts = (struct walt_task_struct *) curr->android_vendor_data1;

	if (unlikely(walt_disabled))
		return;

	walt_lockdep_assert_rq(rq, NULL);

	if (!list_empty(&wts->mvp_list) && wts->mvp_list.next) {
		if (!pipeline_in_progress() || !walt_pipeline_low_latency_task(curr))
			walt_cfs_deactivate_mvp_task(rq, curr);
	}

	if (per_cpu(rt_task_arrival_time, cpu_of(rq)))
		per_cpu(rt_task_arrival_time, cpu_of(rq)) = 0;

	walt_sched_yield_counter++;
}

int walt_set_cpus_taken(struct cpumask *set)
{
	unsigned long flags;
	int cpu;

	if (unlikely(walt_disabled))
		return -EAGAIN;

	spin_lock_irqsave(&cpus_taken_lock, flags);
	for_each_cpu(cpu, set) {
		per_cpu(cpus_taken_refcount, cpu)++;
	}
	cpumask_or(&walt_cpus_taken_mask, &walt_cpus_taken_mask, set);
	spin_unlock_irqrestore(&cpus_taken_lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(walt_set_cpus_taken);

int walt_unset_cpus_taken(struct cpumask *unset)
{
	unsigned long flags;
	int cpu;

	if (unlikely(walt_disabled))
		return -EAGAIN;

	spin_lock_irqsave(&cpus_taken_lock, flags);
	for_each_cpu(cpu, unset) {
		if (per_cpu(cpus_taken_refcount, cpu) >= 1)
			per_cpu(cpus_taken_refcount, cpu)--;
		if (!per_cpu(cpus_taken_refcount, cpu))
			cpumask_clear_cpu(cpu, &walt_cpus_taken_mask);
	}
	spin_unlock_irqrestore(&cpus_taken_lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(walt_unset_cpus_taken);

cpumask_t walt_get_cpus_taken(void)
{
	return walt_cpus_taken_mask;
}
EXPORT_SYMBOL_GPL(walt_get_cpus_taken);

int walt_set_enforce_high_irq_cpus(struct cpumask *set)
{
	unsigned long flags;
	int cpu;

	if (unlikely(walt_disabled))
		return -EAGAIN;

	spin_lock_irqsave(&enforce_high_irq_cpu_lock, flags);
	for_each_cpu(cpu, set) {
		per_cpu(enforce_high_irq_cpus_refcount, cpu)++;
	}
	cpumask_or(&walt_enforce_high_irq_cpu_mask, &walt_enforce_high_irq_cpu_mask, set);
	spin_unlock_irqrestore(&enforce_high_irq_cpu_lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(walt_set_enforce_high_irq_cpus);

int walt_unset_enforce_high_irq_cpus(struct cpumask *unset)
{
	unsigned long flags;
	int cpu;

	if (unlikely(walt_disabled))
		return -EAGAIN;

	spin_lock_irqsave(&enforce_high_irq_cpu_lock, flags);
	for_each_cpu(cpu, unset) {
		if (per_cpu(enforce_high_irq_cpus_refcount, cpu) >= 1)
			per_cpu(enforce_high_irq_cpus_refcount, cpu)--;
		if (!per_cpu(enforce_high_irq_cpus_refcount, cpu))
			cpumask_clear_cpu(cpu, &walt_enforce_high_irq_cpu_mask);
	}
	spin_unlock_irqrestore(&enforce_high_irq_cpu_lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(walt_unset_enforce_high_irq_cpus);

cpumask_t walt_get_enforce_high_irq_cpus(void)
{
	return walt_enforce_high_irq_cpu_mask;
}
EXPORT_SYMBOL_GPL(walt_get_enforce_high_irq_cpus);

int walt_get_cpus_in_state1(struct cpumask *cpus)
{
	if (unlikely(walt_disabled))
		return -EAGAIN;

	cpumask_or(cpus, cpu_partial_halt_mask, &sched_cluster[0]->cpus);
	cpumask_andnot(cpus, cpus, cpu_halt_mask);
	return 0;
}
EXPORT_SYMBOL_GPL(walt_get_cpus_in_state1);

cpumask_t walt_get_halted_cpus(void)
{
	return *(cpu_halt_mask);
}
EXPORT_SYMBOL_GPL(walt_get_halted_cpus);

static void register_walt_hooks(void)
{
	register_trace_android_rvh_wake_up_new_task(android_rvh_wake_up_new_task, NULL);
	register_trace_android_rvh_update_cpu_capacity(android_rvh_update_cpu_capacity, NULL);
	register_trace_android_rvh_sched_cpu_starting(android_rvh_sched_cpu_starting, NULL);
	register_trace_android_rvh_sched_cpu_dying(android_rvh_sched_cpu_dying, NULL);
	register_trace_android_rvh_set_task_cpu(android_rvh_set_task_cpu, NULL);
	register_trace_android_rvh_new_task_stats(android_rvh_new_task_stats, NULL);
	register_trace_android_rvh_account_irq(android_rvh_account_irq, NULL);
	register_trace_android_rvh_flush_task(android_rvh_flush_task, NULL);
	register_trace_android_rvh_update_misfit_status(android_rvh_update_misfit_status, NULL);
	register_trace_android_rvh_enqueue_task(android_rvh_enqueue_task, NULL);
	register_trace_android_rvh_dequeue_task(android_rvh_dequeue_task, NULL);
	register_trace_android_rvh_try_to_wake_up(android_rvh_try_to_wake_up, NULL);
	register_trace_android_rvh_tick_entry(android_rvh_tick_entry, NULL);
	register_trace_android_vh_scheduler_tick(android_vh_scheduler_tick, NULL);
	register_trace_android_rvh_schedule(android_rvh_schedule, NULL);
	register_trace_android_rvh_cpu_cgroup_attach(android_rvh_cpu_cgroup_attach, NULL);
	register_trace_android_rvh_cpu_cgroup_online(android_rvh_cpu_cgroup_online, NULL);
	register_trace_android_rvh_sched_fork_init(android_rvh_sched_fork_init, NULL);
	register_trace_android_rvh_ttwu_cond(android_rvh_ttwu_cond, NULL);
	register_trace_android_rvh_sched_exec(android_rvh_sched_exec, NULL);
	register_trace_android_rvh_build_perf_domains(android_rvh_build_perf_domains, NULL);
	register_trace_cpu_frequency_limits(walt_cpu_frequency_limits, NULL);
	register_trace_android_rvh_do_sched_yield(walt_do_sched_yield, NULL);
	register_trace_android_rvh_update_thermal_stats(android_rvh_update_thermal_stats, NULL);
}

atomic64_t walt_irq_work_lastq_ws;
bool walt_disabled = true;

static int walt_init_stop_handler(void *data)
{
	int cpu;
	struct task_struct *g, *p;
	struct walt_rq *wrq;
	int level;

	read_lock(&tasklist_lock);
	level = 0;
	for_each_possible_cpu(cpu) {
		if (level == 0)
			raw_spin_lock(&cpu_rq(cpu)->__lock);
		else
			raw_spin_lock_nested(&cpu_rq(cpu)->__lock, level);
		level++;
	}

	/* existing tasks get a demand of 0, including idle task */
	for_each_process_thread(g, p) {
		init_new_task_load(p);
	}

	for_each_possible_cpu(cpu) {
		/* Create task members for idle thread */
		init_new_task_load(cpu_rq(cpu)->idle);
		walt_flag_set(cpu_rq(cpu)->idle, WALT_IDLE_TASK_BIT, 1);
	}

	/* post walt_init_once() a new task will get a non zero demand */
	walt_init_once();

	for_each_possible_cpu(cpu) {
		struct rq *rq = cpu_rq(cpu);


		walt_sched_init_rq(rq);

		wrq = &per_cpu(walt_rq, cpu_of(rq));
		wrq->window_start = tick_sched_clock;
	}

	atomic64_set(&walt_irq_work_lastq_ws, tick_sched_clock);

	create_default_coloc_group();

	walt_disabled = false;

	for_each_possible_cpu(cpu) {
		raw_spin_unlock(&cpu_rq(cpu)->__lock);
	}
	read_unlock(&tasklist_lock);
	return 0;
}

static void walt_init_tg_pointers(void)
{
	struct cgroup_subsys_state *css = &root_task_group.css;
	struct cgroup_subsys_state *top_css = css;

	rcu_read_lock();
	css_for_each_child(css, top_css)
		walt_update_tg_pointer(css);
	rcu_read_unlock();
}

static void walt_remove_cpufreq_efficiencies_available(void)
{
	struct cpufreq_policy *policy;
	struct walt_sched_cluster *cluster;

	for_each_sched_cluster(cluster) {
		policy = cpufreq_cpu_get(cluster_first_cpu(cluster));
		if (policy) {
			policy->efficiencies_available = false;
			cpufreq_cpu_put(policy);
		}
	}
}

static void walt_init(struct work_struct *work)
{
	static atomic_t already_inited = ATOMIC_INIT(0);
	struct root_domain *rd = cpu_rq(cpumask_first(cpu_active_mask))->rd;
	int i;

	might_sleep();

	if (atomic_cmpxchg(&already_inited, 0, 1))
		return;

	register_syscore_ops(&walt_syscore_ops);
	BUG_ON(alloc_related_thread_groups());
	init_clusters();
	walt_init_tg_pointers();

	register_walt_hooks();
	walt_fixup_init();
	walt_lb_init();
	walt_rt_init();
	walt_cfs_init();
	walt_halt_init();
	walt_mvp_lock_ordering_init();

	wait_for_completion_interruptible(&tick_sched_clock_completion);

	if (!rcu_access_pointer(rd->pd)) {
		/*
		 * perf domains not properly configured.  this is a must as
		 * create_util_to_cost depends on rd->pd being properly
		 * initialized.
		 */
		schedule_work(&rebuild_sd_work);
		wait_for_completion_interruptible(&rebuild_domains_completion);
	}

	walt_update_cluster_topology();
	walt_remove_cpufreq_efficiencies_available();
	walt_config();
	walt_init_cycle_counter();

	stop_machine(walt_init_stop_handler, NULL, NULL);

	walt_register_sysctl();
	walt_register_debugfs();

	input_boost_init();
	core_ctl_init();
	walt_boost_init();
	waltgov_register();

	i = match_string(sched_feat_names, __SCHED_FEAT_NR, "TTWU_QUEUE");
	if (i >= 0) {
		static_key_disable(&sched_feat_keys[i]);
		sysctl_sched_features &= ~(1UL << i);
	}

	topology_clear_scale_freq_source(SCALE_FREQ_SOURCE_ARCH, cpu_online_mask);
}

static DECLARE_WORK(walt_init_work, walt_init);
static void android_vh_update_topology_flags_workfn(void *unused, void *unused2)
{
	schedule_work(&walt_init_work);
}

#define WALT_VENDOR_DATA_SIZE_TEST(wstruct, kstruct)		\
	BUILD_BUG_ON(sizeof(wstruct) > (sizeof(u64) *		\
		ARRAY_SIZE(((kstruct *)0)->android_vendor_data1)))

static int walt_module_init(void)
{
	/* compile time checks for vendor data size */
	WALT_VENDOR_DATA_SIZE_TEST(struct walt_task_struct, struct task_struct);
	WALT_VENDOR_DATA_SIZE_TEST(struct walt_task_group, struct task_group);

	register_trace_android_vh_update_topology_flags_workfn(
			android_vh_update_topology_flags_workfn, NULL);

	if (topology_update_done)
		schedule_work(&walt_init_work);

	walt_cpufreq_cycle_cntr_driver_register();
	walt_gclk_cycle_counter_driver_register();

	return 0;
}

module_init(walt_module_init);
MODULE_LICENSE("GPL v2");

MODULE_SOFTDEP("pre: socinfo");
#if IS_ENABLED(CONFIG_SCHED_WALT_DEBUG)
MODULE_SOFTDEP("pre: sched-walt-debug");
#endif
