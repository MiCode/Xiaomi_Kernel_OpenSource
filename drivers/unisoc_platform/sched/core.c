// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023, Unisoc (shanghai) Technologies Co., Ltd
 */

#define pr_fmt(fmt)	"unisoc-sched: " fmt

#include <trace/hooks/sched.h>
#include <trace/hooks/dtask.h>
#include <trace/hooks/topology.h>
#include <trace/hooks/ftrace_dump.h>
#include <trace/events/power.h>
#include <linux/kmemleak.h>

#include "uni_sched.h"
#include "cpu_netlink.h"

#define CREATE_TRACE_POINTS
#include "trace.h"

bool uni_sched_disabled = true;

unsigned int min_max_possible_capacity = 1024;
unsigned int max_possible_capacity = 1024; /* max(rq->max_possible_capacity) */

struct list_head cluster_head;
int num_sched_clusters;

/* if a cpu is halting */
struct cpumask __cpu_halt_mask = { CPU_BITS_NONE };

static struct sched_cluster init_cluster = {
	.list			= LIST_HEAD_INIT(init_cluster.list),
	.id			= 0,
	.capacity		= 1024,
};

/* Integer rounded range for each bucket */
#define UCLAMP_BUCKET_DELTA DIV_ROUND_CLOSEST(SCHED_CAPACITY_SCALE, UCLAMP_BUCKETS)

#define for_each_clamp_id(clamp_id) \
	for ((clamp_id) = 0; (clamp_id) < UCLAMP_CNT; (clamp_id)++)

static inline unsigned int uclamp_none(enum uclamp_id clamp_id)
{
	if (clamp_id == UCLAMP_MIN)
		return 0;
	return SCHED_CAPACITY_SCALE;
}

static inline unsigned int uclamp_bucket_id(unsigned int clamp_value)
{
	return min_t(unsigned int, clamp_value / UCLAMP_BUCKET_DELTA, UCLAMP_BUCKETS - 1);
}

static inline void uclamp_se_set(struct uclamp_se *uc_se,
				 unsigned int value, bool user_defined)
{
	uc_se->value = value;
	uc_se->bucket_id = uclamp_bucket_id(value);
	uc_se->user_defined = user_defined;
}

static void init_clusters(void)
{
	init_cluster.cpus = *cpu_possible_mask;
	raw_spin_lock_init(&init_cluster.load_lock);
	INIT_LIST_HEAD(&cluster_head);
	list_add(&init_cluster.list, &cluster_head);
}

static void insert_cluster(struct sched_cluster *cluster, struct list_head *head)
{
	struct sched_cluster *tmp;
	struct list_head *iter = head;

	list_for_each_entry(tmp, head, list) {
		if (arch_scale_cpu_capacity(cpumask_first(&cluster->cpus))
			< arch_scale_cpu_capacity(cpumask_first(&tmp->cpus)))
			break;
		iter = &tmp->list;
	}

	list_add(&cluster->list, iter);
}

static struct sched_cluster *alloc_new_cluster(const struct cpumask *cpus)
{
	struct sched_cluster *cluster = NULL;

	cluster = kzalloc(sizeof(struct sched_cluster), GFP_ATOMIC);
	BUG_ON(!cluster);

	INIT_LIST_HEAD(&cluster->list);

	raw_spin_lock_init(&cluster->load_lock);
	cluster->cpus = *cpus;

	return cluster;
}

static void add_cluster(const struct cpumask *cpus, struct list_head *head)
{
	struct sched_cluster *cluster = alloc_new_cluster(cpus);
	int i;
	struct uni_rq *uni_rq;

	for_each_cpu(i, cpus) {
		uni_rq = (struct uni_rq *) cpu_rq(i)->android_vendor_data1;
		uni_rq->cluster = cluster;
	}
	insert_cluster(cluster, head);
	num_sched_clusters++;
}

static void cleanup_clusters(struct list_head *head)
{
	struct sched_cluster *cluster, *tmp;
	int i;
	struct uni_rq *uni_rq;

	list_for_each_entry_safe(cluster, tmp, head, list) {
		for_each_cpu(i, &cluster->cpus) {
			uni_rq = (struct uni_rq *) cpu_rq(i)->android_vendor_data1;
			uni_rq->cluster = &init_cluster;
		}
		list_del(&cluster->list);
		num_sched_clusters--;
		kfree(cluster);
	}
}

static inline void assign_cluster_ids(struct list_head *head)
{
	struct sched_cluster *cluster;
	int pos = 0;

	list_for_each_entry(cluster, head, list) {
		cluster->id = pos;
		pos++;
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

	/* Ensure list sanity beore making the head visible to all CPUs. */
	smp_mb();
	dst->next = first;
}

static void parse_capacity_from_clusters(void)
{
	struct sched_cluster *cluster;
	unsigned long biggest_cap = 0, smallest_cap = ULONG_MAX;

	for_each_sched_cluster(cluster) {
		unsigned long cap = arch_scale_cpu_capacity(cluster_first_cpu(cluster));

		if (cap > biggest_cap)
			biggest_cap = cap;

		if (cap < smallest_cap)
			smallest_cap = cap;

		cluster->capacity = cap;
	}

	max_possible_capacity = biggest_cap;
	min_max_possible_capacity = smallest_cap;
}

static void get_possible_siblings(int cpuid, struct cpumask *cluster_cpus)
{
	int cpu;
	struct cpu_topology *cpuid_topo = &cpu_topology[cpuid];
	unsigned long cpu_cap, cpuid_cap = arch_scale_cpu_capacity(cpuid);

	if (cpuid_topo->package_id == -1)
		return;

	for_each_possible_cpu(cpu) {
		cpu_cap = arch_scale_cpu_capacity(cpu);

		if (cpu_cap != cpuid_cap)
			continue;
		cpumask_set_cpu(cpu, cluster_cpus);
	}
}

static void update_cluster_topology(void)
{
	struct cpumask cpus = *cpu_possible_mask;
	struct cpumask cluster_cpus;
	struct list_head new_head;
	int i;

	INIT_LIST_HEAD(&new_head);

	for_each_cpu(i, &cpus) {
		cpumask_clear(&cluster_cpus);
		get_possible_siblings(i, &cluster_cpus);
		if (cpumask_empty(&cluster_cpus)) {
			WARN(1, "WALT: Invalid cpu topology!!");
			cleanup_clusters(&new_head);
			return;
		}
		cpumask_andnot(&cpus, &cpus, &cluster_cpus);
		add_cluster(&cluster_cpus, &new_head);
	}

	assign_cluster_ids(&new_head);

	/*
	 * Ensure cluster ids are visible to all CPUs before making
	 * cluster_head visible.
	 */
	move_list(&cluster_head, &new_head, false);
	parse_capacity_from_clusters();
}

#ifdef CONFIG_UNISOC_SCHED_PAUSE_CPU
static inline void cpu_pause_enqueue(struct rq *rq, struct task_struct *p)
{
	struct uni_rq *wrq = (struct uni_rq *) rq->android_vendor_data1;

	if (!is_per_cpu_kthread(p))
		wrq->enqueue_counter++;
	if (cpu_halted(cpu_of(rq)) && !(p->flags & PF_KTHREAD) && halt_check_last(cpu_of(rq)))
		pr_err("Non Kthread Started on halted cpu_of(rq)=%d comm=%s(%d) affinity=0x%x\n",
			 cpu_of(rq), p->comm, p->pid,
			 ((unsigned int)*(cpumask_bits(p->cpus_ptr))));
}
#else
static inline void cpu_pause_enqueue(struct rq *rq, struct task_struct *p) {}
#endif

static void android_rvh_after_enqueue_task(void *data, struct rq *rq,
					   struct task_struct *p, int flags)
{
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *)p->android_vendor_data1;
	u64 wallclock = sched_ktime_clock();
	int freq_flags = 0;

	if (unlikely(uni_sched_disabled))
		return;

	cpu_pause_enqueue(rq, p);

	uni_tsk->last_enqueue_ts = wallclock;

	walt_inc_cumulative_runnable_avg(rq, p);

	group_boost_enqueue(rq, p);

	if (is_fair_task(p))
		enqueue_cfs_vip_task(rq, p);

	if (p->in_iowait)
		freq_flags |= SCHED_CPUFREQ_IOWAIT;

	walt_cpufreq_update_util(rq, freq_flags);
}

static void android_rvh_after_dequeue_task(void *data, struct rq *rq,
					   struct task_struct *p, int flags)
{
	if (unlikely(uni_sched_disabled))
		return;

	walt_dec_cumulative_runnable_avg(rq, p);

	group_boost_dequeue(rq, p);

	if (is_fair_task(p))
		dequeue_cfs_vip_task(rq, p);

	walt_cpufreq_update_util(rq, 0);
}

static void uclamp_fork_init(struct task_struct *p)
{
	enum uclamp_id clamp_id;
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *)p->android_vendor_data1;

	if (likely(!uni_tsk->uclamp_fork_reset))
		return;

	uni_tsk->uclamp_fork_reset = 0;

	for_each_clamp_id(clamp_id) {
		uclamp_se_set(&p->uclamp_req[clamp_id],
			      uclamp_none(clamp_id), false);
	}
}

static void __sched_fork_init(struct task_struct *p)
{
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *)p->android_vendor_data1;

#ifdef CONFIG_UNISOC_SCHED_VIP_TASK
	INIT_LIST_HEAD(&uni_tsk->vip_list);
	uni_tsk->sum_exec_snapshot_for_slice = 0;
	uni_tsk->sum_exec_snapshot_for_total = 0;
	uni_tsk->total_exec = 0;
	uni_tsk->vip_level = SCHED_NOT_VIP;
	uni_tsk->binder_from_cpu = -1;
#endif
	uni_tsk->last_sleep_ts = 0;
	uni_tsk->last_enqueue_ts = 0;

	uclamp_fork_init(p);
}

static void android_vh_dup_task_struct(void *data, struct task_struct *tsk, struct task_struct *orig)
{
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *)tsk->android_vendor_data1;
	struct uni_task_struct *uni_orig = (struct uni_task_struct *)orig->android_vendor_data1;

	uni_tsk->uclamp_fork_reset = uni_orig->uclamp_fork_reset;
}

static void android_rvh_sched_fork_init(void *data, struct task_struct *p)
{
	if (unlikely(uni_sched_disabled))
		return;

	__sched_fork_init(p);
}

static void android_rvh_do_sched_yield(void *data, struct rq *rq)
{
	struct task_struct *curr = rq->curr;

	if (unlikely(uni_sched_disabled))
		return;

	lockdep_assert_rq_held(rq);

	if (is_fair_task(curr))
		dequeue_cfs_vip_task(rq, curr);

	reset_rt_task_arrival_time(cpu_of(rq));
}

static void android_vh_sched_show_task(void *unused, struct task_struct *task)
{
	unsigned int state;

	state = READ_ONCE(task->__state);
	if (state & TASK_UNINTERRUPTIBLE) {
		u64 now = sched_ktime_clock();
		struct uni_task_struct *uni_tsk =
			(struct uni_task_struct *)task->android_vendor_data1;
		u64 last = uni_tsk->last_sleep_ts;
		ktime_t delta = ktime_sub(ns_to_ktime(now), ns_to_ktime(last));

		pr_info("duration_ns:%llu last_sleep_ns:%llu now_ns:%llu\n",
			ktime_to_ns(delta), last, now);
	}
}

#define UNI_CALLER_ADDR(n) ((unsigned long)ftrace_return_address(n))
static void sched_show_untask_stack(void *data, bool preempr,
			struct task_struct *prev, struct task_struct *next)
{
	if (trace_sched_show_untask_stack_enabled()) {
		unsigned long prev_state = READ_ONCE(prev->__state);

		if ((prev_state & TASK_UNINTERRUPTIBLE) &&
		   !(prev_state & TASK_NOLOAD) && !(prev->flags & PF_FROZEN))
			trace_sched_show_untask_stack(prev, UNI_CALLER_ADDR(4),
					UNI_CALLER_ADDR(5), UNI_CALLER_ADDR(6),
					UNI_CALLER_ADDR(7), UNI_CALLER_ADDR(8),
					UNI_CALLER_ADDR(9));
	}
}

static void android_rvh_wake_up_new_task(void *data, struct task_struct *p)
{
	if (unlikely(uni_sched_disabled))
		return;

	walt_init_new_task_load(p);

	if (is_fair_task(p))
		check_parent_vip_status(p);
}

static void android_vh_set_uclamp(void *data, struct task_struct *p, int clamp_id, unsigned int val)
{
	if (trace_clock_set_rate_enabled()) {
		char buf[32] = {0};

		if (clamp_id == UCLAMP_MIN) {
			snprintf(buf, sizeof(buf), "uclamp-min-%d", p->pid);
			trace_clock_set_rate(buf, val, smp_processor_id());
		}
	}
}

static void register_sched_vendor_hooks(void)
{
	register_trace_android_vh_dup_task_struct(android_vh_dup_task_struct, NULL);
	register_trace_android_rvh_sched_fork_init(android_rvh_sched_fork_init, NULL);
	register_trace_android_rvh_wake_up_new_task(android_rvh_wake_up_new_task, NULL);
	register_trace_android_rvh_after_enqueue_task(android_rvh_after_enqueue_task, NULL);
	register_trace_android_rvh_after_dequeue_task(android_rvh_after_dequeue_task, NULL);
	register_trace_android_rvh_do_sched_yield(android_rvh_do_sched_yield, NULL);
	register_trace_android_vh_sched_show_task(android_vh_sched_show_task, NULL);
	register_trace_sched_switch(sched_show_untask_stack, NULL);
	register_trace_android_vh_setscheduler_uclamp(android_vh_set_uclamp, NULL);
}

static DEFINE_RAW_SPINLOCK(init_lock);

#if IS_ENABLED(CONFIG_SCHED_WALT) || defined(CONFIG_UNISOC_SCHED_VIP_TASK)

static void sched_init_existing_task(struct task_struct *p)
{
#if IS_ENABLED(CONFIG_SCHED_WALT)
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *)p->android_vendor_data1;
	int i;

	uni_tsk->init_load_pct = 0;
	uni_tsk->mark_start = 0;
	uni_tsk->sum = 0;
	uni_tsk->sum_latest = 0;
	uni_tsk->curr_window = 0;
	uni_tsk->prev_window = 0;

	uni_tsk->demand = 0;
	uni_tsk->demand_scale = 0;
	for (i = 0; i < RAVG_HIST_SIZE_MAX; ++i)
		uni_tsk->sum_history[i] = 0;
#endif
	__sched_fork_init(p);

#ifdef CONFIG_UNISOC_SCHED_PAUSE_CPU
	cpumask_copy(&uni_tsk->cpus_requested, &p->cpus_mask);
#endif

}

#ifdef CONFIG_UNISOC_WORKAROUND_L3_HANG
static struct timer_list stop_machine_timer;
static bool stop_machine_init;

static void stop_machine_time_out(struct timer_list *t)
{
	int cpu;

	for_each_online_cpu(cpu) {
		if (cpu != smp_processor_id()) {
			pr_warn("send ipi resched to cpu:%d\n", cpu);
			smp_send_reschedule(cpu);
		}
	}
}
#endif

static int sched_init_stop_handler(void *data)
{
	int cpu;
	struct task_struct *g, *p;
#if IS_ENABLED(CONFIG_SCHED_WALT)
	u64 window_start_ns, nr_windows;
#endif

	raw_spin_lock(&init_lock);

#ifdef CONFIG_UNISOC_WORKAROUND_L3_HANG
	if (!stop_machine_init) {
		stop_machine_init = true;
		del_timer(&stop_machine_timer);
	}
#endif

	if (!uni_sched_disabled)
		goto unlock;

	read_lock(&tasklist_lock);

#if IS_ENABLED(CONFIG_SCHED_WALT)
	window_start_ns = ktime_get_ns();
	nr_windows = div64_u64(window_start_ns, walt_ravg_window);
	window_start_ns = (u64)nr_windows * (u64)walt_ravg_window;
#endif

	for_each_possible_cpu(cpu) {
		struct rq *rq = cpu_rq(cpu);
		struct uni_rq *uni_rq = (struct uni_rq *) rq->android_vendor_data1;

		raw_spin_rq_lock(rq);

		/* Create task members for idle thread */
		sched_init_existing_task(rq->idle);

#if IS_ENABLED(CONFIG_SCHED_WALT)
		uni_rq->push_task = NULL;

		uni_rq->cumulative_runnable_avg = 0;
		uni_rq->cur_irqload = 0;
		uni_rq->avg_irqload = 0;
		uni_rq->irqload_ts = 0;
		uni_rq->is_busy = 0;
		uni_rq->sched_flag = 0;

		uni_rq->cumulative_runnable_avg = 0;
		uni_rq->curr_runnable_sum = uni_rq->prev_runnable_sum = 0;

		uni_rq->window_start = window_start_ns;
#endif
#ifdef CONFIG_UNISOC_SCHED_VIP_TASK
		uni_rq->num_vip_tasks = 0;
		INIT_LIST_HEAD(&uni_rq->vip_tasks);
#endif
		raw_spin_rq_unlock(cpu_rq(cpu));
	}

	do_each_thread(g, p) {
		sched_init_existing_task(p);
	} while_each_thread(g, p);

	read_unlock(&tasklist_lock);

	uni_sched_disabled = false;

unlock:
	raw_spin_unlock(&init_lock);

	return 0;
}
#else
static int sched_init_stop_handler(void *data)
{
	raw_spin_lock(&init_lock);

	if (!uni_sched_disabled)
		goto unlock;

	uni_sched_disabled = false;

unlock:
	raw_spin_unlock(&init_lock);

	return 0;
}
#endif

static void uni_sched_init(struct work_struct *work)
{
	struct ctl_table_header *hdr;
	static atomic_t already_inited = ATOMIC_INIT(0);

	might_sleep();

	if (atomic_cmpxchg(&already_inited, 0, 1))
		return;

	walt_init();

	init_clusters();

	register_sched_vendor_hooks();

	init_group_control();

	rt_init();

	fair_init();

	update_cluster_topology();

	stop_machine(sched_init_stop_handler, NULL, NULL);

	hdr = register_sysctl_table(sched_base_table);

	kmemleak_not_leak(hdr);

	uscfreq_gov_register();

	cpu_netlink_init();

	core_pause_init();
	core_ctl_init();

	input_boost_init();
	powerkey_boost_init();
}

static DECLARE_WORK(sched_init_work, uni_sched_init);
static void android_vh_update_topology_flags_workfn(void *unused, void *unused2)
{
	schedule_work(&sched_init_work);
}

static void ftrace_dump_printk(void *unused, struct trace_seq *trace_buf, bool *dump_printk)
{
	*dump_printk = false;
}

#ifdef CONFIG_UNISOC_SCHED_DEBUG_ATOMIC
static void android_rvh_schedule_bug(void *unused, void *prev)
{
	panic("scheduling while atomic\n");
}
#endif

#define UNI_VENDOR_DATA_TEST(unistruct, kstruct)		\
	BUILD_BUG_ON(sizeof(unistruct) > (sizeof(u64) *	\
			ARRAY_SIZE(((kstruct *)0)->android_vendor_data1)))

static __init int sched_module_init(void)
{
	UNI_VENDOR_DATA_TEST(struct uni_task_struct, struct task_struct);
	UNI_VENDOR_DATA_TEST(struct uni_rq, struct rq);
	UNI_VENDOR_DATA_TEST(struct uni_task_group, struct task_group);

	register_trace_android_vh_update_topology_flags_workfn(
			android_vh_update_topology_flags_workfn, NULL);

	register_trace_android_vh_ftrace_dump_buffer(ftrace_dump_printk, NULL);

#ifdef CONFIG_UNISOC_SCHED_DEBUG_ATOMIC
	register_trace_android_rvh_schedule_bug(android_rvh_schedule_bug, NULL);
#endif

#ifdef CONFIG_UNISOC_WORKAROUND_L3_HANG
	timer_setup(&stop_machine_timer, stop_machine_time_out, TIMER_DEFERRABLE);
	stop_machine_timer.expires = jiffies + msecs_to_jiffies(10000);
	add_timer(&stop_machine_timer);
#endif
	if (topology_update_done)
		schedule_work(&sched_init_work);
#ifdef CONFIG_UNISOC_SCHED_OPTIMIC_ON_A7
	else
		schedule_work(&sched_init_work);
#endif

	hung_task_enh_init();

	proc_cpuload_init();

	return 0;
}
core_initcall(sched_module_init);
MODULE_LICENSE("GPL v2");
