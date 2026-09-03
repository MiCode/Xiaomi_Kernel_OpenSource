/*
* Copyright (C) 2015 Spreadtrum Communications Inc .
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied,distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY；without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE。 See the
* GNU General Public License for more details.
*/
#define pr_fmt(fmt) "shrink_async: " fmt

#include <linux/module.h>
#include <trace/hooks/vmscan.h>
#include <linux/swap.h>
#include <linux/proc_fs.h>
#include <linux/gfp.h>
#include <linux/types.h>
#include <linux/cpufreq.h>
#include <linux/freezer.h>
#include <linux/wait.h>
#include <linux/memcontrol.h>
#include <linux/mutex.h>

#define SHRINK_SLABD_NAME "kshrink_slabd"
extern unsigned long shrink_slab(gfp_t gfp_mask, int nid,
				 struct mem_cgroup *memcg,
				 int priority);

static int kshrink_slabd_pid;
static struct task_struct *shrink_slabd_tsk;

wait_queue_head_t shrink_slabd_wait;

struct async_slabd_parameter {
	struct mem_cgroup *shrink_slabd_memcg;
	gfp_t shrink_slabd_gfp_mask;
	int shrink_slabd_runnable;
	int shrink_slabd_nid;
	int priority;
	struct reclaim_state *reclaim_state;
} asp;

static struct reclaim_state async_reclaim_state = {
		.reclaimed_slab = 0,
};

/* copy from mm/vmscan.c */
static void set_task_reclaim_state(struct task_struct *task,
				   struct reclaim_state *rs)
{
	/* Check for an overwrite */
	WARN_ON_ONCE(rs && task->reclaim_state);

	/* Check for the nulling of an already-nulled member */
	WARN_ON_ONCE(!rs && !task->reclaim_state);

	task->reclaim_state = rs;
}

static DEFINE_MUTEX(async_shrink_slab_mutex);

static bool is_shrink_slabd_task(struct task_struct *tsk)
{
	return tsk->pid == kshrink_slabd_pid;
}

static bool wakeup_shrink_slabd(gfp_t gfp_mask, int nid,
				 struct mem_cgroup *memcg,
				 int priority)
{
	if (!mutex_trylock(&async_shrink_slab_mutex))
		return false;

	if (asp.shrink_slabd_runnable == 1) {
		mutex_unlock(&async_shrink_slab_mutex);
		return false;
	}
	async_reclaim_state.reclaimed_slab = 0;
	asp.reclaim_state = &async_reclaim_state;
	asp.shrink_slabd_gfp_mask = gfp_mask;
	asp.shrink_slabd_nid = nid;
	asp.shrink_slabd_memcg = memcg;
	asp.priority = priority;
	asp.shrink_slabd_runnable = 1;
	css_get(&memcg->css);
	mutex_unlock(&async_shrink_slab_mutex);

	wake_up_interruptible(&shrink_slabd_wait);
	return true;
}

static void set_async_slabd_cpus(void)
{
	struct cpumask cpumask = { CPU_BITS_NONE };
	unsigned int cpu;
	unsigned long cpu_cap;
	unsigned long cpuid_cap = arch_scale_cpu_capacity(0);
	static bool set_slabd_cpus_success;

	if (likely(set_slabd_cpus_success))
		return;

	for_each_possible_cpu(cpu) {
		cpu_cap = arch_scale_cpu_capacity(cpu);

		if (cpu_cap == cpuid_cap)
			cpumask_set_cpu(cpu, &cpumask);
	}

	if (!cpumask_empty(&cpumask)) {
		set_cpus_allowed_ptr(shrink_slabd_tsk, &cpumask);
		set_slabd_cpus_success = true;
	}
}

static int kshrink_slabd_func(void *p)
{
	struct mem_cgroup *memcg;
	gfp_t gfp_mask;
	int nid, priority;
	/*
	 * Tell the memory management that we're a "memory allocator",
	 * and that if we need more memory we should get access to it
	 * regardless (see "__alloc_pages()"). "kswapd" should
	 * never get caught in the normal page freeing logic.
	 *
	 * (Kswapd normally doesn't need memory anyway, but sometimes
	 * you need a small amount of memory in order to be able to
	 * page out something else, and this flag essentially protects
	 * us from recursively trying to free more memory as we're
	 * trying to free the first piece of memory in the first place).
	 */
	current->flags |= PF_MEMALLOC |  PF_KSWAPD;
	set_freezable();

	asp.reclaim_state = NULL;
	asp.shrink_slabd_gfp_mask = 0;
	asp.shrink_slabd_nid = 0;
	asp.shrink_slabd_memcg = NULL;
	asp.shrink_slabd_runnable = 0;
	asp.priority = 0;

	while (!kthread_should_stop()) {
		wait_event_freezable(shrink_slabd_wait, asp.shrink_slabd_runnable == 1);

		set_async_slabd_cpus();

		mutex_lock(&async_shrink_slab_mutex);
		set_task_reclaim_state(current, asp.reclaim_state);
		nid = asp.shrink_slabd_nid;
		gfp_mask = asp.shrink_slabd_gfp_mask;
		priority = asp.priority;
		memcg = asp.shrink_slabd_memcg;
		asp.shrink_slabd_runnable = 0;
		mutex_unlock(&async_shrink_slab_mutex);

		shrink_slab(gfp_mask, nid, memcg, priority);
		css_put(&memcg->css);
		set_task_reclaim_state(current, NULL);
	}
	current->flags &= ~(PF_MEMALLOC | PF_KSWAPD);
	current->reclaim_state = NULL;

	return 0;
}

static void should_shrink_async(void *data, gfp_t gfp_mask, int nid,
			struct mem_cgroup *memcg, int priority, bool *bypass)
{
	if (!is_shrink_slabd_task(current))
		*bypass = wakeup_shrink_slabd(gfp_mask, nid, memcg, priority);
}

int kshrink_slabd_async_init(void)
{
	int ret;

	init_waitqueue_head(&shrink_slabd_wait);

	shrink_slabd_tsk = kthread_run(kshrink_slabd_func, NULL, SHRINK_SLABD_NAME);
	if (IS_ERR_OR_NULL(shrink_slabd_tsk)) {
		pr_err("Failed to start shrink_slabd on node 0\n");
		ret = PTR_ERR(shrink_slabd_tsk);
		shrink_slabd_tsk = NULL;
		return ret;
	}

	kshrink_slabd_pid = shrink_slabd_tsk->pid;

	ret = register_trace_android_vh_shrink_slab_bypass(should_shrink_async, NULL);
	if (ret != 0) {
		kthread_stop(shrink_slabd_tsk);
		shrink_slabd_tsk = NULL;
		return ret;
	}

	return 0;
}

void kshrink_slabd_async_exit(void)
{
	unregister_trace_android_vh_shrink_slab_bypass(should_shrink_async, NULL);

	if (shrink_slabd_tsk) {
		kthread_stop(shrink_slabd_tsk);
		shrink_slabd_tsk = NULL;
	}
}
