/* drivers/misc/lowmemorykiller.c
 *
 * The lowmemorykiller driver lets user-space specify a set of memory thresholds
 * where processes with a range of oom_score_adj values will get killed. Specify
 * the minimum oom_score_adj values in
 * /sys/module/lowmemorykiller/parameters/adj and the number of free pages in
 * /sys/module/lowmemorykiller/parameters/minfree. Both files take a comma
 * separated list of numbers in ascending order.
 *
 * For example, write "0,8" to /sys/module/lowmemorykiller/parameters/adj and
 * "1024,4096" to /sys/module/lowmemorykiller/parameters/minfree to kill
 * processes with a oom_score_adj value of 8 or higher when the free memory
 * drops below 4096 pages and kill processes with a oom_score_adj value of 0 or
 * higher when the free memory drops below 1024 pages.
 *
 * The driver considers memory used for caches to be free, but if a large
 * percentage of the cached memory is locked this can be very inaccurate
 * and processes may not get killed until the normal oom killer is triggered.
 *
 * Copyright (C) 2007-2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/sched.h>
#include <linux/swap.h>
#include <linux/rcupdate.h>
#include <linux/profile.h>
#include <linux/notifier.h>
#include <linux/circ_buf.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/freezer.h>
#include <linux/ratelimit.h>

#define MTK_LMK_USER_EVENT

#ifdef MTK_LMK_USER_EVENT
#include <linux/miscdevice.h>
#endif

#if defined(CONFIG_MTK_AEE_FEATURE) && defined(CONFIG_MTK_ENG_BUILD)
#include <mt-plat/aee.h>
#include <disp_assert_layer.h>
#endif

#include "internal.h"

#define CREATE_TRACE_POINTS
#include "trace/lowmemorykiller.h"

static DEFINE_SPINLOCK(lowmem_shrink_lock);
static short lowmem_warn_adj, lowmem_no_warn_adj = 200;
static u32 lowmem_debug_level = 1;
static short lowmem_adj[6] = {
	0,
	1,
	6,
	12,
};

static int lowmem_adj_size = 4;
static int lowmem_minfree[6] = {
	3 * 512,	/* 6MB */
	2 * 1024,	/* 8MB */
	4 * 1024,	/* 16MB */
	16 * 1024,	/* 64MB */
};

static int lowmem_minfree_size = 4;

static unsigned long lowmem_deathpending_timeout;

#define lowmem_print(level, x...)			\
	do {						\
		if (lowmem_debug_level >= (level))	\
			pr_info(x);			\
	} while (0)

static DECLARE_WAIT_QUEUE_HEAD(event_wait);
static DEFINE_SPINLOCK(lmk_event_lock);
static struct circ_buf event_buffer;
#define MAX_BUFFERED_EVENTS 8
#define MAX_TASKNAME 128

struct lmk_event {
	char taskname[MAX_TASKNAME];
	pid_t pid;
	uid_t uid;
	pid_t group_leader_pid;
	unsigned long min_flt;
	unsigned long maj_flt;
	unsigned long rss_in_pages;
	short oom_score_adj;
	short min_score_adj;
	unsigned long long start_time;
	struct list_head list;
};

void handle_lmk_event(struct task_struct *selected, int selected_tasksize,
		      short min_score_adj)
{
	int head;
	int tail;
	struct lmk_event *events;
	struct lmk_event *event;
	int res;
	char taskname[MAX_TASKNAME];

	res = get_cmdline(selected, taskname, MAX_TASKNAME - 1);

	/* No valid process name means this is definitely not associated with a
	 * userspace activity.
	 */

	if (res <= 0 || res >= MAX_TASKNAME)
		return;

	taskname[res] = '\0';

	spin_lock(&lmk_event_lock);

	head = event_buffer.head;
	tail = READ_ONCE(event_buffer.tail);

	/* Do not continue to log if no space remains in the buffer. */
	if (CIRC_SPACE(head, tail, MAX_BUFFERED_EVENTS) < 1) {
		spin_unlock(&lmk_event_lock);
		return;
	}

	events = (struct lmk_event *)event_buffer.buf;
	event = &events[head];

	memcpy(event->taskname, taskname, res + 1);

	event->pid = selected->pid;
	event->uid = from_kuid_munged(current_user_ns(), task_uid(selected));
	if (selected->group_leader)
		event->group_leader_pid = selected->group_leader->pid;
	else
		event->group_leader_pid = -1;
	event->min_flt = selected->min_flt;
	event->maj_flt = selected->maj_flt;
	event->oom_score_adj = selected->signal->oom_score_adj;
	event->start_time = nsec_to_clock_t(selected->real_start_time);
	event->rss_in_pages = selected_tasksize;
	event->min_score_adj = min_score_adj;

	event_buffer.head = (head + 1) & (MAX_BUFFERED_EVENTS - 1);

	spin_unlock(&lmk_event_lock);

	wake_up_interruptible(&event_wait);
}

static int lmk_event_show(struct seq_file *s, void *unused)
{
	struct lmk_event *events = (struct lmk_event *)event_buffer.buf;
	int head;
	int tail;
	struct lmk_event *event;

	spin_lock(&lmk_event_lock);

	head = event_buffer.head;
	tail = event_buffer.tail;

	if (head == tail) {
		spin_unlock(&lmk_event_lock);
		return -EAGAIN;
	}

	event = &events[tail];

	seq_printf(s, "%lu %lu %lu %lu %lu %lu %hd %hd %llu\n%s\n",
		   (unsigned long)event->pid, (unsigned long)event->uid,
		   (unsigned long)event->group_leader_pid, event->min_flt,
		   event->maj_flt, event->rss_in_pages, event->oom_score_adj,
		   event->min_score_adj, event->start_time, event->taskname);

	event_buffer.tail = (tail + 1) & (MAX_BUFFERED_EVENTS - 1);

	spin_unlock(&lmk_event_lock);
	return 0;
}

static unsigned int lmk_event_poll(struct file *file, poll_table *wait)
{
	int ret = 0;

	poll_wait(file, &event_wait, wait);
	spin_lock(&lmk_event_lock);
	if (event_buffer.head != event_buffer.tail)
		ret = POLLIN;
	spin_unlock(&lmk_event_lock);
	return ret;
}

static int lmk_event_open(struct inode *inode, struct file *file)
{
	return single_open(file, lmk_event_show, inode->i_private);
}

static const struct file_operations event_file_ops = {
	.open = lmk_event_open,
	.poll = lmk_event_poll,
	.read = seq_read
};

static void lmk_event_init(void)
{
	struct proc_dir_entry *entry;

	event_buffer.head = 0;
	event_buffer.tail = 0;
	event_buffer.buf = kmalloc(
		sizeof(struct lmk_event) * MAX_BUFFERED_EVENTS, GFP_KERNEL);
	if (!event_buffer.buf)
		return;
	entry = proc_create("lowmemorykiller", 0444, NULL, &event_file_ops);
	if (!entry)
		pr_info("error creating kernel lmk event file\n");
}

static unsigned long lowmem_count(struct shrinker *s,
				  struct shrink_control *sc)
{
#ifdef CONFIG_FREEZER
	/* Don't bother LMK when system is freezing */
	if (pm_freezing)
		return 0;
#endif
	return global_node_page_state(NR_ACTIVE_ANON) +
		global_node_page_state(NR_ACTIVE_FILE) +
		global_node_page_state(NR_INACTIVE_ANON) +
		global_node_page_state(NR_INACTIVE_FILE);
}

#ifdef MTK_LMK_USER_EVENT
static const struct file_operations mtklmk_fops = {
	.owner = THIS_MODULE,
};

static struct miscdevice mtklmk_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mtklmk",
	.fops = &mtklmk_fops,
};

static struct work_struct mtklmk_work;
static int uevent_adj, uevent_minfree;
static void mtklmk_async_uevent(struct work_struct *work)
{
#define MTKLMK_EVENT_LENGTH	(24)
	char adj[MTKLMK_EVENT_LENGTH], free[MTKLMK_EVENT_LENGTH];
	char *envp[3] = { adj, free, NULL };

	snprintf(adj, MTKLMK_EVENT_LENGTH, "OOM_SCORE_ADJ=%d", uevent_adj);
	snprintf(free, MTKLMK_EVENT_LENGTH, "MINFREE=%d", uevent_minfree);
	kobject_uevent_env(&mtklmk_misc.this_device->kobj, KOBJ_CHANGE, envp);
#undef MTKLMK_EVENT_LENGTH
}

static unsigned int mtklmk_initialized;
static unsigned int mtklmk_uevent_timeout = 10000; /* ms */
module_param_named(uevent_timeout, mtklmk_uevent_timeout, uint, 0644);
static void mtklmk_uevent(int oom_score_adj, int minfree)
{
	static unsigned long last_time;
	unsigned long timeout;

	/* change to use jiffies */
	timeout = msecs_to_jiffies(mtklmk_uevent_timeout);

	if (!last_time)
		last_time = jiffies - timeout;

	if (time_before(jiffies, last_time + timeout))
		return;

	last_time = jiffies;

	uevent_adj = oom_score_adj;
	uevent_minfree = minfree;
	schedule_work(&mtklmk_work);
}
#endif

#ifndef CONFIG_MTK_ENABLE_AGO
/* Check memory status by zone, pgdat */
static int lowmem_check_status_by_zone(enum zone_type high_zoneidx,
				       int *other_free, int *other_file)
{
	struct pglist_data *pgdat;
	struct zone *z;
	enum zone_type zoneidx;
	unsigned long accumulated_pages = 0;
	u64 scale = (u64)totalram_pages;
	int new_other_free = 0, new_other_file = 0;
	int memory_pressure = 0;
	int unreclaimable = 0;

	if (high_zoneidx < MAX_NR_ZONES - 1) {
		/* Go through all memory nodes */
		for_each_online_pgdat(pgdat) {
			for (zoneidx = 0; zoneidx <= high_zoneidx; zoneidx++) {
				z = pgdat->node_zones + zoneidx;
				accumulated_pages += z->managed_pages;
				new_other_free +=
					zone_page_state(z, NR_FREE_PAGES);
				new_other_free -= high_wmark_pages(z);
				new_other_file +=
				zone_page_state(z, NR_ZONE_ACTIVE_FILE) +
				zone_page_state(z, NR_ZONE_INACTIVE_FILE);

				/* Compute memory pressure level */
				memory_pressure +=
				zone_page_state(z, NR_ZONE_ACTIVE_FILE) +
				zone_page_state(z, NR_ZONE_INACTIVE_FILE) +
#ifdef CONFIG_SWAP
				zone_page_state(z, NR_ZONE_ACTIVE_ANON) +
				zone_page_state(z, NR_ZONE_INACTIVE_ANON) +
#endif
				new_other_free;
			}

			/*
			 * Consider pgdat as unreclaimable when hitting one of
			 * following two cases,
			 * 1. Memory node is unreclaimable in vmscan.c
			 * 2. Memory node is reclaimable, but nearly no user
			 *    pages(under high wmark)
			 */
			if (!pgdat_reclaimable(pgdat) ||
			    (pgdat_reclaimable(pgdat) && memory_pressure < 0))
				unreclaimable++;
		}

		/*
		 * Update if we go through ONLY lower zone(s) ACTUALLY
		 * and scale in totalram_pages
		 */
		if (totalram_pages > accumulated_pages) {
			do_div(scale, accumulated_pages);
			if ((u64)totalram_pages >
			    (u64)accumulated_pages * scale)
				scale += 1;
			new_other_free *= scale;
			new_other_file *= scale;
		}

		/*
		 * Update if not kswapd or
		 * "being kswapd and high memory pressure"
		 */
		if (!current_is_kswapd() ||
		    (current_is_kswapd() && memory_pressure < 0)) {
			*other_free = new_other_free;
			*other_file = new_other_file;
		}
	}

	return unreclaimable;
}

/* Aggressive Memory Reclaim(AMR) */
static short lowmem_amr_check(int *to_be_aggressive, int other_file)
{
#ifdef CONFIG_SWAP
#ifdef CONFIG_64BIT
#define ENABLE_AMR_RAMSIZE	(0x60000)	/* > 1.5GB */
#else
#define ENABLE_AMR_RAMSIZE	(0x40000)	/* > 1GB */
#endif
	unsigned long swap_pages = 0;
	short amr_adj = OOM_SCORE_ADJ_MAX + 1;
#ifndef CONFIG_MTK_GMO_RAM_OPTIMIZE
	int i;
#endif
	swap_pages = atomic_long_read(&nr_swap_pages);
	/* More than 1/2 swap usage */
	if (swap_pages * 2 < total_swap_pages)
		(*to_be_aggressive)++;
	/* More than 3/4 swap usage */
	if (swap_pages * 4 < total_swap_pages)
		(*to_be_aggressive)++;

#ifndef CONFIG_MTK_GMO_RAM_OPTIMIZE
	/* Try to enable AMR when we have enough memory */
	if (totalram_pages < ENABLE_AMR_RAMSIZE) {
		*to_be_aggressive = 0;
	} else {
		i = lowmem_adj_size - 1;
		/*
		 * Comparing other_file with lowmem_minfree to make
		 * amr less aggressive.
		 * ex.
		 * For lowmem_adj[] = {0, 100, 200, 300, 900, 906},
		 * if swap usage > 50%,
		 * try to kill 906       when other_file >= lowmem_minfree[5]
		 * try to kill 300 ~ 906 when other_file  < lowmem_minfree[5]
		 */
		if (*to_be_aggressive > 0) {
			if (other_file < lowmem_minfree[i])
				i -= *to_be_aggressive;
			if (likely(i >= 0))
				amr_adj = lowmem_adj[i];
		}
	}
#endif

	return amr_adj;
#undef ENABLE_AMR_RAMSIZE

#else	/* !CONFIG_SWAP */
	*to_be_aggressive = 0;
	return OOM_SCORE_ADJ_MAX + 1;
#endif
}
#else
static int lowmem_check_status_by_zone(enum zone_type high_zoneidx,
				       int *other_free, int *other_file)
{
	return 0;
}

#define lowmem_amr_check(a, b) (short)(OOM_SCORE_ADJ_MAX + 1)
#endif

static void __lowmem_trigger_warning(struct task_struct *selected)
{
#if defined(CONFIG_MTK_AEE_FEATURE) && defined(CONFIG_MTK_ENG_BUILD)
#define MSG_SIZE_TO_AEE 70
	char msg_to_aee[MSG_SIZE_TO_AEE];

	lowmem_print(1, "low memory trigger kernel warning\n");
	snprintf(msg_to_aee, MSG_SIZE_TO_AEE,
		 "please contact AP/AF memory module owner[pid:%d]\n",
		 selected->pid);

	aee_kernel_warning_api("LMK", 0, DB_OPT_DEFAULT |
			       DB_OPT_DUMPSYS_ACTIVITY |
			       DB_OPT_LOW_MEMORY_KILLER |
			       DB_OPT_PID_MEMORY_INFO | /* smaps and hprof*/
			       DB_OPT_PROCESS_COREDUMP |
			       DB_OPT_DUMPSYS_SURFACEFLINGER |
			       DB_OPT_DUMPSYS_GFXINFO |
			       DB_OPT_DUMPSYS_PROCSTATS,
			       "Framework low memory\nCRDISPATCH_KEY:FLM_APAF",
			       msg_to_aee);
#undef MSG_SIZE_TO_AEE
#else
	pr_info("(%s) no warning triggered for selected(%s)(%d)\n",
		__func__, selected->comm, selected->pid);
#endif
}

/* try to trigger warning to get more information */
static void lowmem_trigger_warning(struct task_struct *selected,
				   short selected_oom_score_adj)
{
	static DEFINE_RATELIMIT_STATE(ratelimit, 60 * HZ, 1);

	if (selected_oom_score_adj > lowmem_warn_adj)
		return;

	if (!__ratelimit(&ratelimit))
		return;

	__lowmem_trigger_warning(selected);
}

/* try to dump more memory status */
static void dump_memory_status(short selected_oom_score_adj)
{
	static DEFINE_RATELIMIT_STATE(ratelimit, 5 * HZ, 1);
	static DEFINE_RATELIMIT_STATE(ratelimit_urgent, 2 * HZ, 1);

	if (selected_oom_score_adj > lowmem_warn_adj &&
	    !__ratelimit(&ratelimit))
		return;

	if (!__ratelimit(&ratelimit_urgent))
		return;

	show_task_mem();
	show_free_areas(0);
	oom_dump_extra_info();
}

static unsigned long lowmem_scan(struct shrinker *s, struct shrink_control *sc)
{
#define LOWMEM_P_STATE_D	(0x1)
#define LOWMEM_P_STATE_R	(0x2)
#define LOWMEM_P_STATE_OTHER	(0x4)

	struct task_struct *tsk;
	struct task_struct *selected = NULL;
	unsigned long rem = 0;
	int tasksize;
	int i;
	short min_score_adj = OOM_SCORE_ADJ_MAX + 1;
	int minfree = 0;
	int selected_tasksize = 0;
	short selected_oom_score_adj;
	int array_size = ARRAY_SIZE(lowmem_adj);
	int other_free = global_page_state(NR_FREE_PAGES) - totalreserve_pages;
	int other_file = global_node_page_state(NR_FILE_PAGES) -
				global_node_page_state(NR_SHMEM) -
				global_node_page_state(NR_UNEVICTABLE) -
				total_swapcache_pages();
	enum zone_type high_zoneidx = gfp_zone(sc->gfp_mask);
	int p_state_is_found = 0;
	short other_min_score_adj = OOM_SCORE_ADJ_MAX + 1;
	int to_be_aggressive = 0;

	if (!spin_trylock(&lowmem_shrink_lock)) {
		lowmem_print(4, "lowmem_shrink lock failed\n");
		return SHRINK_STOP;
	}

	/*
	 * Check whether it is caused by low memory in lower zone(s)!
	 * This will help solve over-reclaiming situation while total number
	 * of free pages is enough, but lower one(s) is(are) under low memory.
	 */
	if (lowmem_check_status_by_zone(high_zoneidx, &other_free, &other_file)
			> 0)
		other_min_score_adj = 0;

	other_min_score_adj =
		min(other_min_score_adj,
		    lowmem_amr_check(&to_be_aggressive, other_file));

	/* Let other_free be positive or zero */
	if (other_free < 0)
		other_free = 0;

	if (lowmem_adj_size < array_size)
		array_size = lowmem_adj_size;
	if (lowmem_minfree_size < array_size)
		array_size = lowmem_minfree_size;
	for (i = 0; i < array_size; i++) {
		minfree = lowmem_minfree[i];
		if (other_free < minfree && other_file < minfree) {
			if (to_be_aggressive != 0 && i > 3) {
				i -= to_be_aggressive;
				if (i < 3)
					i = 3;
			}
			min_score_adj = lowmem_adj[i];
			break;
		}
	}

	/* Compute suitable min_score_adj */
	min_score_adj = min(min_score_adj, other_min_score_adj);

	lowmem_print(3, "lowmem_scan %lu, %x, ofree %d %d, ma %hd\n",
		     sc->nr_to_scan, sc->gfp_mask, other_free,
		     other_file, min_score_adj);

	if (min_score_adj == OOM_SCORE_ADJ_MAX + 1) {
		lowmem_print(5, "lowmem_scan %lu, %x, return 0\n",
			     sc->nr_to_scan, sc->gfp_mask);
		spin_unlock(&lowmem_shrink_lock);
		return 0;
	}

	selected_oom_score_adj = min_score_adj;

	rcu_read_lock();
	for_each_process(tsk) {
		struct task_struct *p;
		short oom_score_adj;

		if (tsk->flags & PF_KTHREAD)
			continue;

		if (task_lmk_waiting(tsk) &&
		    time_before_eq(jiffies, lowmem_deathpending_timeout)) {
			rcu_read_unlock();
			spin_unlock(&lowmem_shrink_lock);
			return 0;
		} else if (task_lmk_waiting(tsk)) {
#ifdef CONFIG_MTK_ENG_BUILD
			lowmem_print(1,
				     "%d (%s) is dying, find next candidate\n",
				     tsk->pid, tsk->comm);
#endif
			if (tsk->state == TASK_RUNNING)
				p_state_is_found |= LOWMEM_P_STATE_R;
			else
				p_state_is_found |= LOWMEM_P_STATE_OTHER;

			continue;
		}

		p = find_lock_task_mm(tsk);
		if (!p)
			continue;

		/* Bypass D-state process */
		if (p->state & TASK_UNINTERRUPTIBLE) {
			lowmem_print(2,
				     "lowmem_scan filter D state process: %d (%s) state:0x%lx\n",
				     p->pid, p->comm, p->state);
			task_unlock(p);
			p_state_is_found |= LOWMEM_P_STATE_D;
			continue;
		}

		oom_score_adj = p->signal->oom_score_adj;
		if (oom_score_adj < min_score_adj) {
			task_unlock(p);
			continue;
		}

		tasksize = get_mm_rss(p->mm) +
			get_mm_counter(p->mm, MM_SWAPENTS);
		task_unlock(p);
		if (tasksize <= 0)
			continue;
		if (selected) {
			if (oom_score_adj < selected_oom_score_adj)
				continue;
			if (oom_score_adj == selected_oom_score_adj &&
			    tasksize <= selected_tasksize)
				continue;
		}
		selected = p;
		selected_tasksize = tasksize;
		selected_oom_score_adj = oom_score_adj;
		lowmem_print(2, "select '%s' (%d), adj %hd, size %d, to kill\n",
			     p->comm, p->pid, oom_score_adj, tasksize);
	}
	if (selected) {
		long cache_size = other_file * (long)(PAGE_SIZE / 1024);
		long cache_limit = minfree * (long)(PAGE_SIZE / 1024);
		long free = other_free * (long)(PAGE_SIZE / 1024);

		task_lock(selected);
		send_sig(SIGKILL, selected, 0);
		if (selected->mm)
			task_set_lmk_waiting(selected);
		task_unlock(selected);
		trace_lowmemory_kill(selected, cache_size, cache_limit, free);
		lowmem_print(1, "Killing '%s' (%d) (tgid %d), adj %hd,\n"
				 "   to free %ldkB on behalf of '%s' (%d) because\n"
				 "   cache %ldkB is below limit %ldkB for oom_score_adj %hd (%hd)\n"
				 "   Free memory is %ldkB above reserved(decrease %d level)\n",
			     selected->comm, selected->pid, selected->tgid,
			     selected_oom_score_adj,
			     selected_tasksize * (long)(PAGE_SIZE / 1024),
			     current->comm, current->pid,
			     cache_size, cache_limit,
			     min_score_adj, other_min_score_adj,
			     free, to_be_aggressive);
		lowmem_deathpending_timeout = jiffies + HZ;
		lowmem_trigger_warning(selected, selected_oom_score_adj);

		rem += selected_tasksize;
		get_task_struct(selected);
	} else {
		if (p_state_is_found & LOWMEM_P_STATE_D)
			lowmem_print(2,
				     "No selected (full of D-state processes at %d)\n",
				     (int)min_score_adj);
		if (p_state_is_found & LOWMEM_P_STATE_R)
			lowmem_print(2,
				     "No selected (full of R-state processes at %d)\n",
				     (int)min_score_adj);
		if (p_state_is_found & LOWMEM_P_STATE_OTHER)
			lowmem_print(2,
				     "No selected (full of OTHER-state processes at %d)\n",
				     (int)min_score_adj);
	}

	lowmem_print(4, "lowmem_scan %lu, %x, return %lu\n",
		     sc->nr_to_scan, sc->gfp_mask, rem);
	rcu_read_unlock();
	spin_unlock(&lowmem_shrink_lock);

	lockdep_off();
	if (selected) {
		if (current_is_kswapd())
			handle_lmk_event(selected, selected_tasksize,
					 min_score_adj);
		put_task_struct(selected);
	}
	lockdep_on();

	/* dump more memory info outside the lock */
	if (selected && selected_oom_score_adj <= lowmem_no_warn_adj &&
	    min_score_adj <= lowmem_warn_adj)
		dump_memory_status(selected_oom_score_adj);

#ifdef MTK_LMK_USER_EVENT
	/* Send uevent if needed */
	if (mtklmk_initialized && current_is_kswapd() && mtklmk_uevent_timeout)
		mtklmk_uevent(min_score_adj, minfree);
#endif

	return rem;

#undef LOWMEM_P_STATE_D
#undef LOWMEM_P_STATE_R
#undef LOWMEM_P_STATE_OTHER
}

static struct shrinker lowmem_shrinker = {
	.scan_objects = lowmem_scan,
	.count_objects = lowmem_count,
	.seeks = DEFAULT_SEEKS * 16
};

static int __init lowmem_init(void)
{
	if (IS_ENABLED(CONFIG_ZRAM) &&
	    IS_ENABLED(CONFIG_MTK_GMO_RAM_OPTIMIZE))
		vm_swappiness = 100;

	register_shrinker(&lowmem_shrinker);
	lmk_event_init();

#ifdef MTK_LMK_USER_EVENT
	/* initialize work for uevent */
	INIT_WORK(&mtklmk_work, mtklmk_async_uevent);

	/* register as misc device */
	if (!misc_register(&mtklmk_misc)) {
		pr_info("%s: successful to register misc device!\n", __func__);
		mtklmk_initialized = 1;
	}
#endif

	return 0;
}
device_initcall(lowmem_init);

#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES
static short lowmem_oom_adj_to_oom_score_adj(short oom_adj)
{
	if (oom_adj == OOM_ADJUST_MAX)
		return OOM_SCORE_ADJ_MAX;
	else
		return (oom_adj * OOM_SCORE_ADJ_MAX) / -OOM_DISABLE;
}

static void lowmem_autodetect_oom_adj_values(void)
{
	int i;
	short oom_adj;
	short oom_score_adj;
	int array_size = ARRAY_SIZE(lowmem_adj);

	if (lowmem_adj_size < array_size)
		array_size = lowmem_adj_size;

	if (array_size <= 0)
		return;

	oom_adj = lowmem_adj[array_size - 1];
	if (oom_adj > OOM_ADJUST_MAX)
		return;

	oom_score_adj = lowmem_oom_adj_to_oom_score_adj(oom_adj);
	if (oom_score_adj <= OOM_ADJUST_MAX)
		return;

	lowmem_print(1, "lowmem_shrink: convert oom_adj to oom_score_adj:\n");
	for (i = 0; i < array_size; i++) {
		oom_adj = lowmem_adj[i];
		oom_score_adj = lowmem_oom_adj_to_oom_score_adj(oom_adj);
		lowmem_adj[i] = oom_score_adj;
		lowmem_print(1, "oom_adj %d => oom_score_adj %d\n",
			     oom_adj, oom_score_adj);
	}
}

static int lowmem_adj_array_set(const char *val, const struct kernel_param *kp)
{
	int ret;

	ret = param_array_ops.set(val, kp);

	/* HACK: Autodetect oom_adj values in lowmem_adj array */
	lowmem_autodetect_oom_adj_values();

	return ret;
}

static int lowmem_adj_array_get(char *buffer, const struct kernel_param *kp)
{
	return param_array_ops.get(buffer, kp);
}

static void lowmem_adj_array_free(void *arg)
{
	param_array_ops.free(arg);
}

static struct kernel_param_ops lowmem_adj_array_ops = {
	.set = lowmem_adj_array_set,
	.get = lowmem_adj_array_get,
	.free = lowmem_adj_array_free,
};

static const struct kparam_array __param_arr_adj = {
	.max = ARRAY_SIZE(lowmem_adj),
	.num = &lowmem_adj_size,
	.ops = &param_ops_short,
	.elemsize = sizeof(lowmem_adj[0]),
	.elem = lowmem_adj,
};
#endif

/*
 * not really modular, but the easiest way to keep compat with existing
 * bootargs behaviour is to continue using module_param here.
 */
module_param_named(cost, lowmem_shrinker.seeks, int, 0644);
#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES
module_param_cb(adj, &lowmem_adj_array_ops,
		.arr = &__param_arr_adj,
		0644);
__MODULE_PARM_TYPE(adj, "array of short");
#else
module_param_array_named(adj, lowmem_adj, short, &lowmem_adj_size, 0644);
#endif
module_param_array_named(minfree, lowmem_minfree, uint, &lowmem_minfree_size,
			 0644);
module_param_named(debug_level, lowmem_debug_level, uint, 0644);
module_param_named(debug_adj, lowmem_warn_adj, short, 0644);
module_param_named(no_debug_adj, lowmem_no_warn_adj, short, 0644);
