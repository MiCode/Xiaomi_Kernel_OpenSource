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
 * Copyright (C) 2018 XiaoMi, Inc.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/swap.h>
#include <linux/rcupdate.h>
#include <linux/notifier.h>
#include <linux/freezer.h>
#include <linux/cpu.h>
#include <linux/fb.h>

#if defined(CONFIG_MTK_AEE_FEATURE) && defined(CONFIG_MT_ENG_BUILD)
#include <mt-plat/aee.h>
#include <disp_assert_layer.h>
static uint32_t in_lowmem;
#endif

#ifdef CONFIG_HIGHMEM
#include <linux/highmem.h>
#endif

#ifdef CONFIG_MTK_ION
#include "mtk/ion_drv.h"
#endif

#ifdef CONFIG_MTK_GPU_SUPPORT
#include <mt-plat/mtk_gpu_utility.h>
#endif

#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES
#define CONVERT_ADJ(x) ((x * OOM_SCORE_ADJ_MAX) / -OOM_DISABLE)
#define REVERT_ADJ(x)  (x * (-OOM_DISABLE + 1) / OOM_SCORE_ADJ_MAX)
#else
#define CONVERT_ADJ(x) (x)
#define REVERT_ADJ(x) (x)
#endif /* CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES */

static short lowmem_debug_adj = CONVERT_ADJ(0);
#ifdef CONFIG_MT_ENG_BUILD
#ifdef CONFIG_MTK_AEE_FEATURE
static short lowmem_kernel_warn_adj = CONVERT_ADJ(0);
#endif
#define output_expect(x) likely(x)
static uint32_t enable_candidate_log = 1;
#define LMK_LOG_BUF_SIZE 500
static uint8_t lmk_log_buf[LMK_LOG_BUF_SIZE];
#else
#define output_expect(x) unlikely(x)
static uint32_t enable_candidate_log;
#endif
static DEFINE_SPINLOCK(lowmem_shrink_lock);

#define CREATE_TRACE_POINTS
#include "trace/lowmemorykiller.h"

static uint32_t lowmem_debug_level = 1;
static short lowmem_adj[9] = {
	0,
	1,
	6,
	12,
};
static int lowmem_adj_size = 9;
int lowmem_minfree[9] = {
	3 * 512,	/* 6MB */
	2 * 1024,	/* 8MB */
	4 * 1024,	/* 16MB */
	16 * 1024,	/* 64MB */
};
static int lowmem_minfree_size = 9;

#ifdef CONFIG_HIGHMEM
static int total_low_ratio = 1;
#endif

static struct task_struct *lowmem_deathpending;
static unsigned long lowmem_deathpending_timeout;
static unsigned long lowmem_scan(struct shrinker *s, struct shrink_control *sc);

#define lowmem_print(level, x...)			\
	do {						\
		if (lowmem_debug_level >= (level))	\
			pr_info(x);			\
	} while (0)

static int
task_notify_func(struct notifier_block *self, unsigned long val, void *data);

static struct notifier_block task_nb = {
	.notifier_call	= task_notify_func,
};

static int
task_notify_func(struct notifier_block *self, unsigned long val, void *data)
{
	struct task_struct *task = data;

	if (task == lowmem_deathpending)
		lowmem_deathpending = NULL;

	return NOTIFY_DONE;
}

static unsigned long lowmem_count(struct shrinker *s,
				  struct shrink_control *sc)
{
#ifdef CONFIG_FREEZER
	/* Do not allow LMK to work when system is freezing */
	if (pm_freezing)
		return 0;
#endif
	return global_page_state(NR_ACTIVE_ANON) +
		global_page_state(NR_ACTIVE_FILE) +
		global_page_state(NR_INACTIVE_ANON) +
		global_page_state(NR_INACTIVE_FILE);
}

#ifndef CONFIG_HIGHMEM
#define DYN_MINFREE_TIME 2
static int lmk_fb_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	int minfree_size = ARRAY_SIZE(lowmem_minfree);
	int orig_lowmem_minfree[minfree_size];
	int i;
	int *blank;
	struct fb_event *evdata = data;
	struct shrinker s;
	struct shrink_control sc = {
		.gfp_mask = GFP_KERNEL,
	};

	switch (event) {
	case FB_EVENT_BLANK:
		blank = evdata->data;
		if (*blank == FB_BLANK_POWERDOWN) {
			for (i = 0; i < minfree_size; i++) {
				orig_lowmem_minfree[i] = lowmem_minfree[i];
				lowmem_minfree[i] = orig_lowmem_minfree[i]
							 * DYN_MINFREE_TIME;
			}
			lowmem_scan(&s, &sc);
			lowmem_print(4, "BLANK_POWER: lowmem_minfree[%d]=%d "
					"orig_lowmem_minfree=%d\n",
					(minfree_size-1),
					lowmem_minfree[minfree_size-1],
					orig_lowmem_minfree[minfree_size-1]);
			if (lowmem_minfree[0] == orig_lowmem_minfree[0]
							 * DYN_MINFREE_TIME) {
				for (i = 0; i < minfree_size; i++)
					lowmem_minfree[i] = orig_lowmem_minfree[i];
			}
		}
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block lmk_fb_nb = {
	.notifier_call = lmk_fb_notifier,
	.priority = INT_MAX,
};
#endif

static unsigned long lowmem_scan(struct shrinker *s, struct shrink_control *sc)
{
	struct task_struct *tsk;
	struct task_struct *selected = NULL;
	static struct task_struct *prev_selected;
	unsigned long rem = 0;
	int tasksize;
	int i;
	short min_score_adj = OOM_SCORE_ADJ_MAX + 1;
	int minfree = 0;
	int selected_tasksize = 0;
	short selected_oom_score_adj;
	int array_size = ARRAY_SIZE(lowmem_adj);
	int other_free = global_page_state(NR_FREE_PAGES) - totalreserve_pages;
	int other_file = global_page_state(NR_FILE_PAGES) -
						global_page_state(NR_SHMEM) -
						total_swapcache_pages();

	int print_extra_info = 0;
	static unsigned long lowmem_print_extra_info_timeout;
#if defined(CONFIG_SWAP) && defined(CONFIG_MTK_GMO_RAM_OPTIMIZE)
	int to_be_aggressive = 0;
	unsigned long swap_pages = 0;
#endif
	bool in_cpu_hotplugging = false;

#ifdef CONFIG_MT_ENG_BUILD
	/* dump memory info when framework low memory*/
	int pid_dump = -1; /* process which need to be dump */
	/* int pid_sec_mem = -1; */
	int max_mem = 0;
	static int pid_flm_warn = -1;
	static unsigned long flm_warn_timeout;
	int log_offset = 0, log_ret;
#endif /* CONFIG_MT_ENG_BUILD*/
	/*
	* If we already have a death outstanding, then
	* bail out right away; indicating to vmscan
	* that we have nothing further to offer on
	* this pass.
	*
	*/
	if (lowmem_deathpending &&
	    time_before_eq(jiffies, lowmem_deathpending_timeout))
		return SHRINK_STOP;

	/* Check whether it is in cpu_hotplugging */
	in_cpu_hotplugging = cpu_hotplugging();

	/* Subtract CMA free pages from other_free if this is an unmovable page allocation */
	if (IS_ENABLED(CONFIG_CMA))
		if (!(sc->gfp_mask & __GFP_MOVABLE))
			other_free -= global_page_state(NR_FREE_CMA_PAGES);

	if (!spin_trylock(&lowmem_shrink_lock)) {
		lowmem_print(4, "lowmem_shrink lock failed\n");
		return SHRINK_STOP;
	}

	/* Let other_free be positive or zero */
	if (other_free < 0) {
		/* lowmem_print(1, "Original other_free [%d] is too low!\n", other_free); */
		other_free = 0;
	}

#if defined(CONFIG_64BIT) && defined(CONFIG_SWAP)
	/* Halve other_free if there is less free swap */
	if (vm_swap_full()) {
		lowmem_print(3, "Halve other_free %d\n", other_free);
		other_free >>= 1;
	}
#endif

#if defined(CONFIG_SWAP) && defined(CONFIG_MTK_GMO_RAM_OPTIMIZE)
	swap_pages = atomic_long_read(&nr_swap_pages);
	/* More than 1/2 swap usage */
	if (swap_pages * 2 < total_swap_pages)
		to_be_aggressive++;
	/* More than 3/4 swap usage */
	if (swap_pages * 4 < total_swap_pages)
		to_be_aggressive++;
#endif

	if (lowmem_adj_size < array_size)
		array_size = lowmem_adj_size;
	if (lowmem_minfree_size < array_size)
		array_size = lowmem_minfree_size;
	for (i = 0; i < array_size; i++) {
		minfree = lowmem_minfree[i];
		if (other_free < minfree && other_file < minfree) {
#if defined(CONFIG_SWAP) && defined(CONFIG_MTK_GMO_RAM_OPTIMIZE)
			if (to_be_aggressive != 0 && i > 3) {
				i -= to_be_aggressive;
				if (i < 3)
					i = 3;
			}
#endif
			min_score_adj = lowmem_adj[i];
			break;
		}
	}

	/* If in CPU hotplugging, let LMK be more aggressive */
	if (in_cpu_hotplugging) {
		pr_alert("Aggressive LMK during CPU hotplug!\n");
		min_score_adj = 0;
	}

	lowmem_print(3, "lowmem_scan %lu, %x, ofree %d %d, ma %hd\n",
			sc->nr_to_scan, sc->gfp_mask, other_free,
			other_file, min_score_adj);

	if (min_score_adj == OOM_SCORE_ADJ_MAX + 1) {
		lowmem_print(5, "lowmem_scan %lu, %x, return 0\n",
			     sc->nr_to_scan, sc->gfp_mask);

#if defined(CONFIG_MTK_AEE_FEATURE) && defined(CONFIG_MT_ENG_BUILD)
		/*
		* disable indication if low memory
		*/
		if (in_lowmem) {
			in_lowmem = 0;
			/* DAL_LowMemoryOff(); */
			lowmem_print(1, "LowMemoryOff\n");
		}
#endif
		spin_unlock(&lowmem_shrink_lock);
		return 0;
	}

	selected_oom_score_adj = min_score_adj;

	/* add debug log */
	if (output_expect(enable_candidate_log)) {
		if (min_score_adj <= lowmem_debug_adj) {
			if (time_after_eq(jiffies, lowmem_print_extra_info_timeout)) {
				lowmem_print_extra_info_timeout = jiffies + HZ;
				print_extra_info = 1;
			}
		}

		if (print_extra_info) {
			lowmem_print(1, "Free memory other_free: %d, other_file:%d pages\n", other_free, other_file);
#ifdef CONFIG_MT_ENG_BUILD
			log_offset = snprintf(lmk_log_buf, LMK_LOG_BUF_SIZE, "%s",
#else
			lowmem_print(1,
#endif
#ifdef CONFIG_ZRAM
					"<lmk>  pid  adj  score_adj     rss   rswap name\n");
#else
					"<lmk>  pid  adj  score_adj     rss name\n");
#endif
		}
	}

	rcu_read_lock();
	for_each_process(tsk) {
		struct task_struct *p;
		short oom_score_adj;

		if (tsk->flags & PF_KTHREAD)
			continue;

		p = find_lock_task_mm(tsk);
		if (!p)
			continue;

#ifdef CONFIG_MT_ENG_BUILD
		if (p->signal->flags & SIGNAL_GROUP_COREDUMP) {
			task_unlock(p);
			continue;
		}
#endif

		if (test_tsk_thread_flag(p, TIF_MEMDIE) &&
		    time_before_eq(jiffies, lowmem_deathpending_timeout)) {
#ifdef CONFIG_MT_ENG_BUILD
			static pid_t last_dying_pid;

			if (last_dying_pid != p->pid) {
				lowmem_print(1, "lowmem_shrink return directly, due to  %d (%s) is dying\n",
					p->pid, p->comm);
				last_dying_pid = p->pid;
			}
#endif
			task_unlock(p);
			rcu_read_unlock();
			spin_unlock(&lowmem_shrink_lock);
			return SHRINK_STOP;
		}
		oom_score_adj = p->signal->oom_score_adj;

		if (output_expect(enable_candidate_log)) {
			if (print_extra_info) {
#ifdef CONFIG_MT_ENG_BUILD
log_again:
				log_ret = snprintf(lmk_log_buf+log_offset, LMK_LOG_BUF_SIZE-log_offset,
#else
				lowmem_print(1,
#endif
#ifdef CONFIG_ZRAM
						"<lmk>%5d%5d%11d%8lu%8lu %s\n", p->pid,
						REVERT_ADJ(oom_score_adj), oom_score_adj,
						get_mm_rss(p->mm),
						get_mm_counter(p->mm, MM_SWAPENTS), p->comm);
#else /* CONFIG_ZRAM */
						"<lmk>%5d%5d%11d%8lu %s\n", p->pid,
						REVERT_ADJ(oom_score_adj), oom_score_adj,
						get_mm_rss(p->mm), p->comm);
#endif

#ifdef CONFIG_MT_ENG_BUILD
				if ((log_offset + log_ret) >= LMK_LOG_BUF_SIZE || log_ret < 0) {
					*(lmk_log_buf + log_offset) = '\0';
					lowmem_print(1, "\n%s", lmk_log_buf);
					/* pr_err("lmk log overflow log_offset:%d\n", log_offset); */
					log_offset = 0;
					memset(lmk_log_buf, 0x0, LMK_LOG_BUF_SIZE);
					goto log_again;
				} else
					log_offset += log_ret;
#endif
			}
		}

#ifdef CONFIG_MT_ENG_BUILD
		tasksize = get_mm_rss(p->mm);
#ifdef CONFIG_ZRAM
		tasksize += get_mm_counter(p->mm, MM_SWAPENTS);
#endif
		/*
		* dump memory info when framework low memory:
		* record the first two pid which consumed most memory.
		*/
		if (tasksize > max_mem) {
			max_mem = tasksize;
			/* pid_sec_mem = pid_dump; */
			pid_dump = p->pid;
		}

		if (p->pid == pid_flm_warn &&
			time_before_eq(jiffies, flm_warn_timeout)) {
			task_unlock(p);
			continue;
		}
#endif

		if (oom_score_adj < min_score_adj) {
			task_unlock(p);
			continue;
		}

#ifndef CONFIG_MT_ENG_BUILD
		tasksize = get_mm_rss(p->mm);
#ifdef CONFIG_ZRAM
		tasksize += get_mm_counter(p->mm, MM_SWAPENTS);
#endif
#endif
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
#ifdef CONFIG_MTK_GMO_RAM_OPTIMIZE
		/*
		* if cached > 30MB, don't kill ub:secureRandom while its adj is 9
		*/
		if (!strcmp(p->comm, "ub:secureRandom") &&
			(REVERT_ADJ(oom_score_adj) == 9) && (other_file > 30*256)) {
			pr_info("select but ignore '%s' (%d), oom_score_adj %d, oom_adj %d, size %d, to kill, cache %ldkB is below limit %ldkB",
							p->comm, p->pid,
							oom_score_adj, REVERT_ADJ(oom_score_adj),
							tasksize,
							other_file * (long)(PAGE_SIZE / 1024),
							minfree * (long)(PAGE_SIZE / 1024));
		    continue;
		}
#endif
		selected = p;
		selected_tasksize = tasksize;
		selected_oom_score_adj = oom_score_adj;
		lowmem_print(2, "select '%s' (%d), adj %d, score_adj %hd, size %d, to kill\n",
			     p->comm, p->pid, REVERT_ADJ(oom_score_adj), oom_score_adj, tasksize);
	}

#ifdef CONFIG_MT_ENG_BUILD
	if (log_offset > 0)
		lowmem_print(1, "\n%s", lmk_log_buf);
#endif

	if (selected) {
		long cache_size = other_file * (long)(PAGE_SIZE / 1024);
		long cache_limit = minfree * (long)(PAGE_SIZE / 1024);
		long free = other_free * (long)(PAGE_SIZE / 1024);
		trace_lowmemory_kill(selected, cache_size, cache_limit, free);

		/*  Did it be selected? */
		if (selected == prev_selected) {
			lowmem_print(1, "%s state(%ld)\n", selected->comm, selected->state);
			show_stack(selected, NULL);
		}
		prev_selected = selected;

		lowmem_print(1, "Killing '%s' (%d), adj %d, score_adj %hd,\n"
				"   to free %ldkB on behalf of '%s' (%d) because\n"
				"   cache %ldkB is below limit %ldkB for oom_score_adj %hd\n"
				"   Free memory is %ldkB above reserved\n"
#if defined(CONFIG_SWAP) && defined(CONFIG_MTK_GMO_RAM_OPTIMIZE)
				"   swapfree %lukB of SwapTatal %lukB(decrease %d level)\n"
#endif
				, selected->comm, selected->pid,
				REVERT_ADJ(selected_oom_score_adj),
				selected_oom_score_adj,
				selected_tasksize * (long)(PAGE_SIZE / 1024),
				current->comm, current->pid,
				cache_size, cache_limit,
				min_score_adj,
				free
#if defined(CONFIG_SWAP) && defined(CONFIG_MTK_GMO_RAM_OPTIMIZE)
				, swap_pages * 4, total_swap_pages * 4, to_be_aggressive
#endif
				);
		lowmem_deathpending = selected;
		lowmem_deathpending_timeout = jiffies + HZ;
		set_tsk_thread_flag(selected, TIF_MEMDIE);

		if (output_expect(enable_candidate_log)) {
			if (print_extra_info) {
				show_free_areas(0);
			#ifdef CONFIG_MTK_ION
				/* Show ION status */
				ion_mm_heap_memory_detail();
			#endif
			#ifdef CONFIG_MTK_GPU_SUPPORT
				if (mtk_dump_gpu_memory_usage() == false)
					lowmem_print(1, "mtk_dump_gpu_memory_usage not support\n");
			#endif
			}
		}

#if defined(CONFIG_MTK_AEE_FEATURE) && defined(CONFIG_MT_ENG_BUILD)
		/*
		* when kill adj=0 process trigger kernel warning, only in MTK internal eng load
		*/
		if ((selected_oom_score_adj <= lowmem_kernel_warn_adj) && /*lowmem_kernel_warn_adj=16 for test*/
			time_after_eq(jiffies, flm_warn_timeout)) {
			if (pid_dump != pid_flm_warn) {
				#define MSG_SIZE_TO_AEE 70
				char msg_to_aee[MSG_SIZE_TO_AEE];

				lowmem_print(1, "low memory trigger kernel warning\n");
				snprintf(msg_to_aee, MSG_SIZE_TO_AEE,
						"please contact AP/AF memory module owner[pid:%d]\n", pid_dump);
				aee_kernel_warning_api("LMK", 0, DB_OPT_DEFAULT |
					DB_OPT_DUMPSYS_ACTIVITY |
					DB_OPT_LOW_MEMORY_KILLER |
					DB_OPT_PID_MEMORY_INFO | /* smaps and hprof*/
					DB_OPT_PROCESS_COREDUMP |
					DB_OPT_DUMPSYS_SURFACEFLINGER |
					DB_OPT_DUMPSYS_GFXINFO |
					DB_OPT_DUMPSYS_PROCSTATS,
					"Framework low memory\nCRDISPATCH_KEY:FLM_APAF", msg_to_aee);

				if (pid_dump == selected->pid) {/*select 1st time, filter it*/
					/* pid_dump = pid_sec_mem; */
					pid_flm_warn = pid_dump;
					flm_warn_timeout = jiffies + 60*HZ;
					lowmem_deathpending = NULL;
					lowmem_print(1, "'%s' (%d) max RSS, not kill\n",
								selected->comm, selected->pid);
					send_sig(SIGSTOP, selected, 0);
					rcu_read_unlock();
					spin_unlock(&lowmem_shrink_lock);
					return rem;
				}
			} else {
				lowmem_print(1, "pid_flm_warn:%d, select '%s' (%d)\n",
								pid_flm_warn, selected->comm, selected->pid);
				pid_flm_warn = -1; /*reset*/
			}
		}
#endif

#if defined(CONFIG_MTK_AEE_FEATURE) && defined(CONFIG_MT_ENG_BUILD)
		/*
		* show an indication if low memory
		*/
		if (!in_lowmem && selected_oom_score_adj <= lowmem_debug_adj) {
			in_lowmem = 1;
			/* DAL_LowMemoryOn();*/
			lowmem_print(1, "LowMemoryOn\n");
			/* aee_kernel_warning(module_name, lowmem_warning);*/
		}
#endif

		send_sig(SIGKILL, selected, 0);
		rem += selected_tasksize;
	}

	lowmem_print(4, "lowmem_scan %lu, %x, return %lu\n",
		     sc->nr_to_scan, sc->gfp_mask, rem);
	rcu_read_unlock();
	spin_unlock(&lowmem_shrink_lock);
	return rem;
}

static struct shrinker lowmem_shrinker = {
	.scan_objects = lowmem_scan,
	.count_objects = lowmem_count,
	.seeks = DEFAULT_SEEKS * 16
};

static int __init lowmem_init(void)
{
#ifdef CONFIG_HIGHMEM
	unsigned long normal_pages;
#endif

#ifdef CONFIG_ZRAM
	vm_swappiness = 100;
#endif


	task_free_register(&task_nb);
	register_shrinker(&lowmem_shrinker);

#ifdef CONFIG_HIGHMEM
	normal_pages = totalram_pages - totalhigh_pages;
	total_low_ratio = (totalram_pages + normal_pages - 1) / normal_pages;
	pr_err("[LMK]total_low_ratio[%d] - totalram_pages[%lu] - totalhigh_pages[%lu]\n",
			total_low_ratio, totalram_pages, totalhigh_pages);
#else
	fb_register_client(&lmk_fb_nb);
#endif

	return 0;
}

static void __exit lowmem_exit(void)
{
	unregister_shrinker(&lowmem_shrinker);
	task_free_unregister(&task_nb);
}

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
 * get_min_free_pages
 * returns the low memory killer watermark of the given pid,
 * When the system free memory is lower than the watermark, the LMK (low memory
 * killer) may try to kill processes.
 */
int get_min_free_pages(pid_t pid)
{
	struct task_struct *p;
	int target_oom_adj = 0;
	int i = 0;
	int array_size = ARRAY_SIZE(lowmem_adj);

	if (lowmem_adj_size < array_size)
		array_size = lowmem_adj_size;
	if (lowmem_minfree_size < array_size)
		array_size = lowmem_minfree_size;

	for_each_process(p) {
		/* search pid */
		if (p->pid == pid) {
			task_lock(p);
			target_oom_adj = p->signal->oom_score_adj;
			task_unlock(p);
			/* get min_free value of the pid */
			for (i = array_size - 1; i >= 0; i--) {
				if (target_oom_adj >= lowmem_adj[i]) {
					pr_debug("pid: %d, target_oom_adj = %d, lowmem_adj[%d] = %d, lowmem_minfree[%d] = %d\n",
							pid, target_oom_adj, i, lowmem_adj[i], i,
							lowmem_minfree[i]);
					return lowmem_minfree[i];
				}
			}
			goto out;
		}
	}

out:
	lowmem_print(3, "[%s]pid: %d, adj: %d, lowmem_minfree = 0\n",
			__func__, pid, p->signal->oom_score_adj);
	return 0;
}
EXPORT_SYMBOL(get_min_free_pages);

/* Query LMK minfree settings */
/* To query default value, you can input index with value -1. */
size_t query_lmk_minfree(int index)
{
	int which;

	/* Invalid input index, return default value */
	if (index < 0)
		return lowmem_minfree[2];

	/* Find a corresponding output */
	which = 5;
	do {
		if (lowmem_adj[which] <= index)
			break;
	} while (--which >= 0);

	/* Fix underflow bug */
	which = (which < 0) ? 0 : which;

	return lowmem_minfree[which];
}
EXPORT_SYMBOL(query_lmk_minfree);

module_param_named(cost, lowmem_shrinker.seeks, int, S_IRUGO | S_IWUSR);
#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES
module_param_cb(adj, &lowmem_adj_array_ops,
		.arr = &__param_arr_adj, S_IRUGO | S_IWUSR);
__MODULE_PARM_TYPE(adj, "array of short");
#else
module_param_array_named(adj, lowmem_adj, short, &lowmem_adj_size,
			 S_IRUGO | S_IWUSR);
#endif
module_param_array_named(minfree, lowmem_minfree, uint, &lowmem_minfree_size,
			 S_IRUGO | S_IWUSR);
module_param_named(debug_level, lowmem_debug_level, uint, S_IRUGO | S_IWUSR);

#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES
static int debug_adj_set(const char *val, const struct kernel_param *kp)
{
	const int ret = param_set_uint(val, kp);

	lowmem_debug_adj = lowmem_oom_adj_to_oom_score_adj(lowmem_debug_adj);
	return ret;
}

static struct kernel_param_ops debug_adj_ops = {
	.set = &debug_adj_set,
	.get = &param_get_uint,
};

module_param_cb(debug_adj, &debug_adj_ops, &lowmem_debug_adj, S_IRUGO | S_IWUSR);
__MODULE_PARM_TYPE(debug_adj, short);

#if defined(CONFIG_MTK_AEE_FEATURE) && defined(CONFIG_MT_ENG_BUILD)
static int flm_warn_adj_set(const char *val, const struct kernel_param *kp)
{
	const int ret = param_set_uint(val, kp);

	lowmem_kernel_warn_adj = lowmem_oom_adj_to_oom_score_adj(lowmem_kernel_warn_adj);
	return ret;
}

static struct kernel_param_ops flm_warn_adj_ops = {
	.set = &flm_warn_adj_set,
	.get = &param_get_uint,
};
module_param_cb(flm_warn_adj, &flm_warn_adj_ops, &lowmem_kernel_warn_adj, S_IRUGO | S_IWUSR);
#endif
#else
module_param_named(debug_adj, lowmem_debug_adj, short, S_IRUGO | S_IWUSR);
#endif
module_param_named(candidate_log, enable_candidate_log, uint, S_IRUGO | S_IWUSR);

late_initcall(lowmem_init);
module_exit(lowmem_exit);

MODULE_LICENSE("GPL");

