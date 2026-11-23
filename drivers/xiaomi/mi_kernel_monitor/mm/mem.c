
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/cgroup.h>
#include <linux/mmzone.h>
#include <linux/jiffies.h>
#include <linux/sched/cputime.h>
#include <linux/swap.h>
#include <linux/mi_kernel_monitor.h>
#include <trace/events/sched.h>
#include <trace/events/cma.h>
#include <trace/events/vmscan.h>
#include <trace/events/kmem.h>
#include <trace/hooks/mm.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/vmscan.h>
#include <trace/hooks/compaction.h>
#include "../../../kernel/sched/sched.h"
#include "internal.h"
#include "pub/kprobe.h"
#include "event.h"
#include "kernel_monitor.h"
#include "mem_monitor.h"

extern int hook_tracepoint(const char *name, void *probe, void *data);
extern int unhook_tracepoint(const char *name, void *probe, void *data);
////#include "../../../../../../kernel-6.6/mm/internal.h"

#define mem_warn(fmt, ...) \
		pr_info("<xiaomi_mem_monitor><%s> "fmt, __func__, ##__VA_ARGS__)

static atomic_t css_id_top;
static atomic_t css_id_fg;
static atomic_t css_id_bg;
static int mem_enable;

static atomic_t kswapd_wakeup_cnt;
static atomic_t slowpath_cnt;
static atomic_t kcompactd_wakeup_cnt;
static atomic_t page_alloc_cnt;
static atomic_t direct_reclaim_cnt;

static bool __read_mostly mem_monitor_enable;
static bool __read_mostly mem_failure_prop_enable = true;
static bool __read_mostly mem_monitor_debug_trace_enable;

static u64 __read_mostly mem_failure_delay_threshold_ns = 16000000;
static u64 __read_mostly mem_failure_prop_window_ns = 4000000;
static u64 __read_mostly mem_monitor_direct_reclaim_window_ns = 48000000;
static unsigned short mem_failure_prop_max_depth = 10;

static char *grp_name[GRP_TYPES] = {"vip", "rt", "oth",};

enum mem_type {
#ifdef TRACK_PAGE_ALLOC	
	PAGE_ALLOC = 0,
#endif	
	CMA_ALLOC,
	DIRECT_RECLAIM,
	SLOWPATH_ALLOC,
	MEM_TYPE_MAX
};

struct mem_config {
	char name[16];
	u32 min;    /*<1ms*/
	u32 low;    /* ms */
	u32 normal; /* ms */
	u32 high;   /* ms */
};

struct mem_config mem_config[MEM_TYPE_MAX] = {
#ifdef TRACK_PAGE_ALLOC		
	{.name = "page_alloc",	.min=1,	  .low = 8,	.normal = 15,	.high = 30},
#endif	
	{.name = "cma_alloc",	.min=1,   .low = 8,	.normal = 15,	.high = 30},
	{.name = "direct_reclaim",	.min=1,   .low = 8,	.normal = 15,	.high = 30},
	{.name = "slowpath_alloc",	.min=1,   .low = 8,	.normal = 15,	.high = 30},
};
/* Stats counts for low,middle,high,fatal */
struct mem_entry {
	atomic_long_t min_counts;
	atomic_long_t l_counts;
	atomic_long_t m_counts;
	atomic_long_t h_counts;
	atomic_long_t f_counts;
	atomic_long_t all_costs_us;
	atomic_long_t all_costs_ms;
	atomic_long_t max_latency_ms;
};

struct mem_entry mem_entry[GRP_TYPES][MEM_TYPE_MAX];
extern int get_task_grp_idx(struct task_struct* task);

static void tracing_printk(const char *fmt, ...)
{
	char buf[120];
	va_list args;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	trace_printk(buf);
}

static inline void init_mem_failure_prop_info(struct mi_kernel_monitor *km_ts, u64 now,
					       bool little_direct_reclaim,
					       const char *failure_type)
{
	if (!is_critical_ux_task(current)) {
		if (!little_direct_reclaim ||
		    (km_ts->mem_failure_prop_expire_time > now &&
		     km_ts->mem_failure_prop_expire_time <= now +
		     mem_failure_delay_threshold_ns)) {
			/* little_direct_reclaim表示当前的直接回收还属于 */
			/* 一个比较短的直接回收，只有之前直接回收故障 */
			/* 影响窗口包含now，才会持续更新 */
			km_ts->mem_failure_prop_expire_time = now +
							    mem_failure_prop_window_ns;
			km_ts->mem_failure_prop_depth = 1;

			if (unlikely(mem_monitor_debug_trace_enable))
				tracing_printk("I|%d|init mem_failure depth=%u "
					"expire_time=%llu little_dr=%d "
					"now=%llu type=%s",
					task_tgid_vnr(current),
					km_ts->mem_failure_prop_depth,
					km_ts->mem_failure_prop_expire_time,
					little_direct_reclaim, now,
					failure_type);
		}
	}
}

static inline bool update_mem_failure_prop_info(struct mi_kernel_monitor *waked_km_ts,
						struct mi_kernel_monitor *curr_km_ts,
						struct task_struct *waked_task, u64 now)
{
	if (!in_interrupt() && curr_km_ts->mem_failure_prop_depth) {
		if (curr_km_ts->mem_failure_prop_depth >= mem_failure_prop_max_depth ||
		    curr_km_ts->mem_failure_prop_expire_time <= now) {
			if (unlikely(mem_monitor_debug_trace_enable))
				tracing_printk("I|%d|reinit mem_failure wake_tid=%d "
					"depth=%u expire_time=%llu  now=%llu",
					task_tgid_vnr(current),
					waked_task->pid,
					curr_km_ts->mem_failure_prop_depth,
					curr_km_ts->mem_failure_prop_expire_time,
					now);
			curr_km_ts->mem_failure_prop_depth = 0;
			curr_km_ts->mem_failure_prop_expire_time = 0;

			return false;
		}

		/* 发现loop，可能是两种情况: */
		/* 1.同一个线程多次唤醒另一个线程 */
		/* 2.A唤醒B,B唤醒A */
		if (curr_km_ts->mem_failure_prop_expire_time ==
		    waked_km_ts->mem_failure_prop_expire_time) {

			if (unlikely(mem_monitor_debug_trace_enable))
				tracing_printk("I|%d|loop mem_failure "
					"wake_tid=%d now_depth=%u "
					"cur_depth=%u cur_expire=%llu",
					task_tgid_vnr(current),
					waked_task->pid,
					waked_km_ts->mem_failure_prop_depth,
					curr_km_ts->mem_failure_prop_depth,
					curr_km_ts->mem_failure_prop_expire_time);

			return false;
		}

		if (!is_critical_ux_task(waked_task)) {
			u64 orig_prop_expire_time =
				waked_km_ts->mem_failure_prop_expire_time;
			unsigned short orig_prop_depth =
				waked_km_ts->mem_failure_prop_depth;

			/* 考虑故障的交叉传递 */
			if (!orig_prop_depth)
				waked_km_ts->mem_failure_prop_depth =
					curr_km_ts->mem_failure_prop_depth + 1;
			else
				waked_km_ts->mem_failure_prop_depth =
					min(waked_km_ts->mem_failure_prop_depth,
					    curr_km_ts->mem_failure_prop_depth + 1);

			waked_km_ts->mem_failure_prop_expire_time =
				max(orig_prop_expire_time,
				    curr_km_ts->mem_failure_prop_expire_time);

			if (unlikely(mem_monitor_debug_trace_enable))
				tracing_printk("I|%d|mod mem_failure tid=%d "
					"orig_depth=%u "
					"orig_expire=%llu "
					"new_depth=%u "
					"new_expire=%llu",
				task_tgid_vnr(current),
				waked_task->pid,
				orig_prop_depth,
				orig_prop_expire_time,
				waked_km_ts->mem_failure_prop_depth,
				waked_km_ts->mem_failure_prop_expire_time);
		}
		return true;
	}
	return false;
}

static void clear_stats(void)
{
	int i, j;

	for (i = 0; i < GRP_TYPES; i++) {
		for (j = 0; j < MEM_TYPE_MAX; j++) {
			atomic64_set(&mem_entry[i][j].min_counts, 0);
			atomic64_set(&mem_entry[i][j].l_counts, 0);
			atomic64_set(&mem_entry[i][j].m_counts, 0);
 			atomic64_set(&mem_entry[i][j].h_counts, 0);
 			atomic64_set(&mem_entry[i][j].f_counts, 0);
 			atomic64_set(&mem_entry[i][j].all_costs_us, 0);
 			atomic64_set(&mem_entry[i][j].all_costs_ms, 0);
			atomic64_set(&mem_entry[i][j].max_latency_ms, 0);
		}
	}

	atomic_set(&kswapd_wakeup_cnt, 0);
	atomic_set(&slowpath_cnt, 0);
	atomic_set(&kcompactd_wakeup_cnt, 0);
	atomic_set(&page_alloc_cnt, 0);
	atomic_set(&direct_reclaim_cnt, 0);
}

static void update_mem_entry(int grp_type, int mem_type, u64 delay)
{
	////u64 update_delay = (delay >> 20); /* ns -> ms */
	u64 update_delay_us = (delay / 1000); /* ns -> ms */
	u64 update_delay_ms = (delay / 1000000); /* ns -> ms */

	if (update_delay_ms >= mem_config[mem_type].high) {
		atomic_long_inc(&mem_entry[grp_type][mem_type].f_counts);
	}else if (update_delay_ms >= mem_config[mem_type].normal && update_delay_ms < mem_config[mem_type].high) {
		atomic_long_inc(&mem_entry[grp_type][mem_type].h_counts);
	}else if (update_delay_ms >= mem_config[mem_type].low && update_delay_ms < mem_config[mem_type].normal) {
		atomic_long_inc(&mem_entry[grp_type][mem_type].m_counts);
	}else if (update_delay_ms >= 1 && update_delay_ms < mem_config[mem_type].low) {
		atomic_long_inc(&mem_entry[grp_type][mem_type].l_counts);
	} else{
		atomic_long_inc(&mem_entry[grp_type][mem_type].min_counts);
	}

	if (update_delay_ms >= 1/*mem_config[mem_type].low*/)
	{
        atomic_long_add(update_delay_ms, &mem_entry[grp_type][mem_type].all_costs_ms);
		if (update_delay_ms > atomic_long_read(&mem_entry[grp_type][mem_type].max_latency_ms))
		{
			atomic_long_set(&mem_entry[grp_type][mem_type].max_latency_ms, update_delay_ms);
		}
	}
	else
	{
        atomic_long_add(update_delay_us, &mem_entry[grp_type][mem_type].all_costs_us);
	}
}

static void cma_alloc_start_handler(void *unused, const char* name, unsigned long count, unsigned int align)
{
	struct mi_kernel_monitor *km_ts = NULL;
	km_ts = get_mi_kernel_monitor(current);

	if (unlikely(IS_ERR_OR_NULL(km_ts))) {
		return;
	}

	km_ts->cma_alloc_start_tm = local_clock();
}

static void cma_alloc_finish_handler(void *unused, const char* name, unsigned long pfn, const struct page*page, unsigned long count, unsigned int align, int errono)
{
	struct mi_kernel_monitor *km_ts = NULL;
	int grp_type = get_task_grp_idx(current);
	u64 delta = 0;
	u64 clock = local_clock();

	km_ts = get_mi_kernel_monitor(current);
	if (unlikely(IS_ERR_OR_NULL(km_ts))) {
		return;
	}

	if (km_ts->cma_alloc_start_tm != 0) {
		delta = clock - km_ts->cma_alloc_start_tm;
		update_mem_entry(grp_type, CMA_ALLOC, delta);
	}

	if (mem_monitor_enable && delta >= mem_failure_delay_threshold_ns) {
		trace_cma_alloc_delay(name, delta, count, errono);
		if (likely(mem_failure_prop_enable))
			init_mem_failure_prop_info(km_ts, local_clock(), false, "cma");
	}

	km_ts->cma_alloc_start_tm = 0;
}

static void direct_reclaim_enter_handler(void *unused, unsigned int order)
{
	struct mi_kernel_monitor *km_ts = NULL;
	u64 clock = local_clock();
	km_ts = get_mi_kernel_monitor(current);

	atomic_inc(&direct_reclaim_cnt);
	if (unlikely(IS_ERR_OR_NULL(km_ts))) {
		return;
	}

	km_ts->direct_reclaim_start_tm = clock;

	if (mem_monitor_enable) {
		km_ts->throttle_reclaim_start_tm = clock;
		km_ts->reclaim_order = order;
	}
}

static void vmscan_direct_reclaim_begin_handler(void *unused, int order, gfp_t gfp_mask)
{
	struct mi_kernel_monitor *km_ts = NULL;

	km_ts = get_mi_kernel_monitor(current);
	if (unlikely(IS_ERR_OR_NULL(km_ts)))
		return;

	if (km_ts->throttle_reclaim_start_tm != 0) {
		u64 delta = 0, now = local_clock();

		delta = now - km_ts->throttle_reclaim_start_tm;
		km_ts->throttle_reclaim_start_tm = now;

		if (delta >= mem_failure_delay_threshold_ns) {
			trace_try_to_free_pages_delay(order, delta);
			if (likely(mem_failure_prop_enable))
				init_mem_failure_prop_info(km_ts, now, false, "throttle");
		}

		if (km_ts->direct_reclaim_window_start_tm == 0 ||
			now - km_ts->direct_reclaim_window_start_tm >
			mem_monitor_direct_reclaim_window_ns) {
			km_ts->direct_reclaim_window_start_tm = now;
			km_ts->direct_reclaim_accumulate_tm = 0;
			km_ts->direct_reclaim_accumulate_cnt = 0;
		}
	}
}
static void direct_reclaim_exit_handler(void *unused,
					unsigned long did_some_progress,
					int retry_times)
{
	struct mi_kernel_monitor *km_ts = NULL;
	int grp_type = get_task_grp_idx(current);
	u64 delta = 0;
	u64 clock = local_clock();

	km_ts = get_mi_kernel_monitor(current);
	if (unlikely(IS_ERR_OR_NULL(km_ts)))
		return;

	if (km_ts->direct_reclaim_start_tm != 0) {
		delta = clock - km_ts->direct_reclaim_start_tm;
		update_mem_entry(grp_type, DIRECT_RECLAIM, delta);
	}
	if (mem_monitor_enable) {
		u64 now = clock, delta;

		delta = now - km_ts->throttle_reclaim_start_tm;

		km_ts->direct_reclaim_accumulate_tm += delta;
		km_ts->direct_reclaim_accumulate_cnt++;

		if (km_ts->direct_reclaim_accumulate_tm >=
		    mem_failure_delay_threshold_ns) {
			trace_direct_reclaim_delay(km_ts->reclaim_order, delta,
						   did_some_progress,
						   km_ts->direct_reclaim_accumulate_tm,
						   km_ts->direct_reclaim_accumulate_cnt,
						   now - km_ts->direct_reclaim_window_start_tm);
			if (likely(mem_failure_prop_enable))
				init_mem_failure_prop_info(km_ts, now, false,
							    "dr");

			km_ts->direct_reclaim_window_start_tm = 0;
			km_ts->direct_reclaim_accumulate_tm = 0;
			km_ts->direct_reclaim_accumulate_cnt = 0;
		} else {
			if (likely(mem_failure_prop_enable))
				init_mem_failure_prop_info(km_ts, now, true,
							    "dr");
		}

		km_ts->throttle_reclaim_start_tm = 0;
		km_ts->reclaim_order = -1;
	}

	km_ts->direct_reclaim_start_tm = 0;
}

static void alloc_pages_slowpath_handler(void *unused, gfp_t *gfp_mask, unsigned int order, unsigned long alloc_start,
		u64 stime, unsigned long did_some_progress, unsigned long pages_reclaimed, int retry_loop_count)
{
	unsigned long delta_jiffies = jiffies - alloc_start;
	int grp_type = get_task_grp_idx(current);
	u64 delta_ns = (delta_jiffies *1000000000)/HZ;

	atomic_inc(&slowpath_cnt);
	update_mem_entry(grp_type, SLOWPATH_ALLOC, delta_ns);
}

static void vmscan_wakeup_kswapd_handler(void *unused, int nid, int zid, int order, gfp_t gfp_flags)
{
	atomic_inc(&kswapd_wakeup_cnt);
}

static void mm_compaction_wakeup_kcompacted_handler(void *unused, int nid, int zid, enum zone_type highest_zoneidx)
{
	atomic_inc(&kcompactd_wakeup_cnt);
}

static void trace_mm_page_alloc_handler(void *unused, struct page *page, unsigned int order, gfp_t gfp_flags, int migratetype)
{
	atomic_inc(&page_alloc_cnt);
}

static void mem_enable_change(int enable)
{
	int ret;
	if (enable) {
		ret = hook_tracepoint("cma_alloc_start", cma_alloc_start_handler, NULL);
		ret = hook_tracepoint("cma_alloc_finish", cma_alloc_finish_handler, NULL);
		
		ret = register_trace_android_vh_mm_direct_reclaim_enter(direct_reclaim_enter_handler, NULL);
		ret = register_trace_android_vh_mm_direct_reclaim_exit(direct_reclaim_exit_handler, NULL);
				
		ret = register_trace_android_vh_alloc_pages_slowpath_end(alloc_pages_slowpath_handler, NULL);
		
		ret = hook_tracepoint("mm_vmscan_wakeup_kswapd", vmscan_wakeup_kswapd_handler, NULL);
		ret = hook_tracepoint("mm_compaction_wakeup_kcompactd", mm_compaction_wakeup_kcompacted_handler, NULL);
		ret = hook_tracepoint("mm_page_alloc", trace_mm_page_alloc_handler, NULL);

		mem_warn("huqinghe_mem register hook handlers, ret is %d\n", ret);
	} else {
		ret = unhook_tracepoint("cma_alloc_finish", cma_alloc_finish_handler, NULL);	
		ret = unhook_tracepoint("cma_alloc_start", cma_alloc_start_handler, NULL);

		ret = unregister_trace_android_vh_mm_direct_reclaim_enter(direct_reclaim_enter_handler, NULL);
		ret = unregister_trace_android_vh_mm_direct_reclaim_exit(direct_reclaim_exit_handler, NULL);
		ret = unregister_trace_android_vh_alloc_pages_slowpath_end(alloc_pages_slowpath_handler, NULL);

		ret = unhook_tracepoint("mm_vmscan_wakeup_kswapd", vmscan_wakeup_kswapd_handler, NULL);
		ret = unhook_tracepoint("mm_compaction_wakeup_kcompactd", mm_compaction_wakeup_kcompacted_handler, NULL);
		ret = unhook_tracepoint("mm_page_alloc", trace_mm_page_alloc_handler, NULL);

		mem_warn("huqinghe_mem unregister hook handlers, ret is %d\n", ret);
	}
}

static ssize_t mem_common_write(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	int *pval = (int *)pde_data(file_inode(file));
	char kbuf[4] = {0};
	int err;
	if (pval == NULL)
		goto out;
	memset(kbuf, 0, sizeof(kbuf));
	if (count > sizeof(kbuf) - 1)
		count = sizeof(kbuf) - 1;
	if (copy_from_user(kbuf, buf, count)) {
		mem_warn("failed to do copy_from_user\n");
		return -EFAULT;
	}
	err = kstrtoint(strstrip(kbuf), 0, pval);
	if (err < 0) {
		mem_warn("failed to do kstrtoint\n");
		return -EFAULT;
	}
	if (pval == &mem_enable)
		mem_enable_change(*pval);
out:
	return count;
}

static int mem_common_show(struct seq_file *m, void *v)
{
	u64 min_counts, l_counts, m_counts, h_counts, f_counts, all_costs_ms, all_costs_us;
	u64 total_ms, avg_ms, total_cnt, avg_us, max_ms;
	int i, j;
	if (m->private == &mem_enable) {
		seq_printf(m, "%d\n", *(int*) m->private);
		goto out;
	}
	for (j = 0; j < GRP_TYPES; j++)
	/* m->private is NULL */
	for (i = 0; i < MEM_TYPE_MAX; ++i) {
		struct mem_entry entry = mem_entry[j][i];
		char str[32] = {0};
		min_counts = atomic_long_read(&entry.min_counts);
		l_counts  = atomic_long_read(&entry.l_counts);
		m_counts  = atomic_long_read(&entry.m_counts);
		h_counts  = atomic_long_read(&entry.h_counts);
		f_counts  = atomic_long_read(&entry.f_counts);
		max_ms = atomic_long_read(&entry.max_latency_ms);
		all_costs_ms = atomic_long_read(&entry.all_costs_ms);
		total_ms = l_counts + m_counts + h_counts + f_counts;
		avg_ms = total_ms ? (all_costs_ms / total_ms) : 0;
		all_costs_us = atomic_long_read(&entry.all_costs_us);
		total_cnt = min_counts + l_counts + m_counts + h_counts + f_counts;
		avg_us = total_cnt ? (all_costs_us / total_cnt) : 0;

		snprintf(str, sizeof(str), "%s_%s_lat_min_cnt", grp_name[j], mem_config[i].name);
		seq_printf(m, "%-32s: %llu\n", str, min_counts);
		snprintf(str, sizeof(str), "%s_%s_lat_l_cnt", grp_name[j], mem_config[i].name);
		seq_printf(m, "%-32s: %llu\n", str, l_counts);
		snprintf(str, sizeof(str), "%s_%s_lat_m_cnt", grp_name[j], mem_config[i].name);
		seq_printf(m, "%-32s: %llu\n", str, m_counts);
		snprintf(str, sizeof(str), "%s_%s_lat_h_cnt", grp_name[j], mem_config[i].name);
		seq_printf(m, "%-32s: %llu\n", str, h_counts);
		snprintf(str, sizeof(str), "%s_%s_lat_f_cnt", grp_name[j], mem_config[i].name);
		seq_printf(m, "%-32s: %llu\n", str, f_counts);
		snprintf(str, sizeof(str), "%s_%s_lat_avg_%s", grp_name[j], mem_config[i].name, "ms");
		seq_printf(m, "%-32s: %llu\n", str, avg_ms);
		snprintf(str, sizeof(str), "%s_%s_lat_avg_%s", grp_name[j], mem_config[i].name, "us");
		seq_printf(m, "%-32s: %llu\n", str, avg_us);
		snprintf(str, sizeof(str), "%s_%s_lat_max_%s", grp_name[j], mem_config[i].name, "ms");
		seq_printf(m, "%-32s: %llu\n", str, max_ms);		
	}
	
	seq_printf(m, "%-32s: %d\n", "kswapd_cnt", atomic_read(&kswapd_wakeup_cnt));
	seq_printf(m, "%-32s: %d\n", "kcompactd_cnt", atomic_read(&kcompactd_wakeup_cnt));
	seq_printf(m, "%-32s: %d\n", "direct_reclaim_cnt", atomic_read(&direct_reclaim_cnt));
	seq_printf(m, "%-32s: %d\n", "slowpath_cnt", atomic_read(&slowpath_cnt));
	seq_printf(m, "%-32s: %d\n", "page_alloc_cnt", atomic_read(&page_alloc_cnt));
out:
	return 0;
}

static int mem_common_open(struct inode *inode, struct file *file)
{
	return single_open(file, mem_common_show, pde_data(inode));
}

static const struct proc_ops mem_common_proc_ops = {
	.proc_open		= mem_common_open,
	.proc_write		= mem_common_write,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= single_release,
};

/*******************************reset enable********************************/

static int reset_show(struct seq_file *m, void *ptr)
{
	seq_printf(m,"%s\n", "Data Reset");
	return 0;
}

static ssize_t reset_store(void *priv, const char __user *buf, size_t count)
{
	int val;
	if (kstrtoint_from_user(buf, count, 0, &val))
		return -EINVAL;

	if (val) 
	{
		clear_stats();
	}

	return count;
}
DEFINE_PROC_ATTRIBUTE_RW(reset);

/*******************************stack trace info********************************/

#ifdef CONFIG_FAIR_GROUP_SCHED
static void init_cgroup_id(void)
{
	struct cgroup_subsys_state *css = &root_task_group.css;
	struct cgroup_subsys_state *top_css = css;
	if (css->cgroup && (css->cgroup->level == 0)) {
		mem_warn("root group, css->id=%d (expected 1)\n", css->id);
	}
	rcu_read_lock();
	css_for_each_child(css, top_css) {
		mem_warn("traversal css, level is %d, name is %-12s, id is %d\n",
				css->cgroup->level, css->cgroup->kn->name,
				css->id);
		if (!strcmp(css->cgroup->kn->name, "top-app"))
			atomic_set(&css_id_top, css->id);
		else if (!strcmp(css->cgroup->kn->name, "foreground"))
			atomic_set(&css_id_fg, css->id);
		else if (!strcmp(css->cgroup->kn->name, "background"))
			atomic_set(&css_id_bg, css->id);
	}
	rcu_read_unlock();
	mem_warn("traversal css end, top is %d, fg is %d, bg is %d\n",
			atomic_read(&css_id_top), atomic_read(&css_id_fg),
			atomic_read(&css_id_bg));
}
#else
static inline void init_cgroup_id(void) {}
#endif


static void compaction_try_to_compact_pages_handler(void *unused, int order,
						      gfp_t gfp_mask, int prio)
{
	struct mi_kernel_monitor *km_ts = NULL;

	km_ts = get_mi_kernel_monitor(current);

	if (IS_ERR_OR_NULL(km_ts))
		return;

	km_ts->compact_order = order;
	km_ts->direct_compact_start_tm = local_clock();
}

static void compaction_try_to_compact_exit_handler(void *unused,
						    enum compact_result *compact_result)
{
	struct mi_kernel_monitor *km_ts = NULL;
	u64 delta = 0, now;

	km_ts = get_mi_kernel_monitor(current);

	if (unlikely(IS_ERR_OR_NULL(km_ts)))
		return;

	now = local_clock();
	delta = now - km_ts->direct_compact_start_tm;

	if (delta > mem_failure_delay_threshold_ns) {
		trace_try_to_compact_pages_delay(km_ts->compact_order, delta,
						 *compact_result);

		if (likely(mem_failure_prop_enable))
			init_mem_failure_prop_info(km_ts, now, false,
						    "compaction");
	}

	km_ts->compact_order = -1;
	km_ts->direct_compact_start_tm = 0;
}


static int migration_entry_wait_on_locked_entry_handler(struct kretprobe_instance *ri,
							 struct pt_regs *regs)
{
	u64 *start_tm;

	start_tm = (u64 *)ri->data;
	*start_tm = local_clock();
	return 0;
}
NOKPROBE_SYMBOL(migration_entry_wait_on_locked_entry_handler);


static int migration_entry_wait_on_locked_ret_handler(struct kretprobe_instance *ri,
						       struct pt_regs *regs)
{
	u64 *start_tm;
	u64 delta;
	u64 now;

	start_tm = (u64 *)ri->data;
	now = local_clock();
	delta = now - *start_tm;

	if (delta > mem_failure_delay_threshold_ns) {
		trace_migration_entry_wait_delay(delta);

		if (likely(mem_failure_prop_enable)) {
			struct mi_kernel_monitor *km_ts = NULL;

			km_ts = get_mi_kernel_monitor(current);

			if (unlikely(IS_ERR_OR_NULL(km_ts)))
				return 0;

			init_mem_failure_prop_info(km_ts, now, false,
						    "migrate");
		}
	}
	return 0;
}
NOKPROBE_SYMBOL(migration_entry_wait_on_locked_ret_handler);

static struct kretprobe orig_migration_entry_wait_on_locked_kretprobe = {
	.kp = {.symbol_name = "migration_entry_wait_on_locked"},
	.handler = migration_entry_wait_on_locked_ret_handler,
	.entry_handler = migration_entry_wait_on_locked_entry_handler,
	.data_size = sizeof(unsigned long),
	.maxactive = 32,
};

static inline bool is_reclaim_thread(struct task_struct *task)
{
	return strncmp(task->comm, "MiuiIntegratedM", 15) == 0;
}

/* Vendor hook handlers for swap timing */
static void swap_writepage_start_handler(void *unused, unsigned long *start_tm)
{
	/* 过滤一体化内存服务回收、kswapd线程 */
	if (current_is_kswapd() || is_reclaim_thread(current)) {
		*start_tm = 0;
		return;
	}

	*start_tm = local_clock();
}

static void swap_writepage_end_handler(void *unused, struct page *page,
				       struct writeback_control *wbc,
				       unsigned long start_tm)
{
	u64 delta;
	u64 now;

	/* 如果start_tm为0，说明是被过滤的进程，直接返回 */
	if (start_tm == 0)
		return;

	now = local_clock();
	delta = now - start_tm;

	if (delta > mem_failure_delay_threshold_ns) {
		trace_swap_write_folio_delay(delta);

		if (likely(mem_failure_prop_enable)) {
			struct mi_kernel_monitor *km_ts = NULL;

			km_ts = get_mi_kernel_monitor(current);

			if (unlikely(IS_ERR_OR_NULL(km_ts)))
				return;

			init_mem_failure_prop_info(km_ts, now, false,
						    "swap_write");
		}
	}
}

static void swapin_start_handler(void *unused, unsigned long *start_tm)
{
	struct mi_kernel_monitor *km_ts = NULL;

	km_ts = get_mi_kernel_monitor(current);

	if (IS_ERR_OR_NULL(km_ts)) {
		*start_tm = 0;
		return;
	}

	km_ts->io_waited = false;
	*start_tm = local_clock();
}

static void swapin_end_handler(void *unused, struct folio *folio,
			      unsigned long start_tm)
{
	u64 delta, now;
	struct mi_kernel_monitor *km_ts = NULL;

	/* 如果start_tm为0，说明是异常情况，直接返回 */
	if (start_tm == 0)
		return;

	km_ts = get_mi_kernel_monitor(current);

	if (IS_ERR_OR_NULL(km_ts))
		return;

	now = local_clock();
	delta = now - start_tm;

	if (delta > mem_failure_delay_threshold_ns) {
		trace_swap_read_folio_delay(delta, km_ts->io_waited);

		if (likely(mem_failure_prop_enable))
			init_mem_failure_prop_info(km_ts, now, false,
						    "swap_read");
	}
}

static struct kretprobe migration_entry_wait_on_locked_kretprobe;

static void try_to_wake_up_handler(void *unused, struct task_struct *p)
{
	struct mi_kernel_monitor *km_ts = NULL;

	if (!mem_monitor_enable)
		return;

	km_ts = get_mi_kernel_monitor(p);

	if (IS_ERR_OR_NULL(km_ts))
		return;

	if (p->in_iowait)
		km_ts->io_waited = true;

	if (likely(mem_failure_prop_enable)) {
		struct mi_kernel_monitor *curr_km_ts = NULL;

		curr_km_ts = get_mi_kernel_monitor(current);

		if (unlikely(IS_ERR_OR_NULL(curr_km_ts)))
			return;

		if (update_mem_failure_prop_info(km_ts, curr_km_ts, p,
						 local_clock()))
			trace_mem_failure_prop_info(p,
						    curr_km_ts->mem_failure_prop_depth);
	}
}

enum MEM_MONITOR_TYPE {
	MEM_MONITOR_TYPE_DIRECT_RECLAIM_BEGIN = 0,
	MEM_MONITOR_TYPE_COMPACT_PAGES,
	MEM_MONITOR_TYPE_COMPACT_EXIT,
	MEM_MONITOR_TYPE_MIGRATION_ENTRY,
	MEM_MONITOR_TYPE_SWAP_WRITE_START,
	MEM_MONITOR_TYPE_SWAP_WRITE_END,
	MEM_MONITOR_TYPE_SWAP_READ_START,
	MEM_MONITOR_TYPE_SWAP_READ_END,
	MEM_MONITOR_TYPE_TRY_TO_WAKEUP,
	MEM_MONITOR_TYPE_MAX
};
static char *mem_monitor_hook_type[] = {
	"vmscan_direct_reclaim_begin",
	"mm_compaction_try_to_compact_pages",
	"mm_compaction_try_to_compact_exit",
	"migration_entry_wait_on_locked",
	"swap_writepage_start",
	"swap_writepage_end",
	"swapin_start",
	"swapin_end",
	"try_to_wake_up",
};

static DECLARE_BITMAP(mem_monitor_register_status, MEM_MONITOR_TYPE_MAX);

static void mem_monitor_hook_register(bool enable)
{
	int i;
	int err_code[MEM_MONITOR_TYPE_MAX] = {0};

	if (enable) {
		if (!test_bit(MEM_MONITOR_TYPE_DIRECT_RECLAIM_BEGIN,
			     mem_monitor_register_status))
			err_code[MEM_MONITOR_TYPE_DIRECT_RECLAIM_BEGIN] =
				hook_tracepoint("mm_vmscan_direct_reclaim_begin",
					vmscan_direct_reclaim_begin_handler,
					NULL);

		if (!test_bit(MEM_MONITOR_TYPE_COMPACT_PAGES,
			     mem_monitor_register_status))
			err_code[MEM_MONITOR_TYPE_COMPACT_PAGES] =
				hook_tracepoint("mm_compaction_try_to_compact_pages",
					compaction_try_to_compact_pages_handler,
					NULL);

		if (!test_bit(MEM_MONITOR_TYPE_COMPACT_EXIT,
			     mem_monitor_register_status))
			err_code[MEM_MONITOR_TYPE_COMPACT_EXIT] =
				register_trace_android_vh_compaction_try_to_compact_exit(
					compaction_try_to_compact_exit_handler,
					NULL);

		if (!test_bit(MEM_MONITOR_TYPE_MIGRATION_ENTRY,
			mem_monitor_register_status)) {
			memcpy(&migration_entry_wait_on_locked_kretprobe,
				&orig_migration_entry_wait_on_locked_kretprobe,
				sizeof(orig_migration_entry_wait_on_locked_kretprobe));
			err_code[MEM_MONITOR_TYPE_MIGRATION_ENTRY] =
				register_kretprobe(&migration_entry_wait_on_locked_kretprobe);
		}

		if (!test_bit(MEM_MONITOR_TYPE_SWAP_WRITE_START,
			     mem_monitor_register_status))
			err_code[MEM_MONITOR_TYPE_SWAP_WRITE_START] =
				register_trace_android_vh_swap_writepage_start(
					swap_writepage_start_handler, NULL);

		if (!test_bit(MEM_MONITOR_TYPE_SWAP_WRITE_END,
			     mem_monitor_register_status))
			err_code[MEM_MONITOR_TYPE_SWAP_WRITE_END] =
				register_trace_android_vh_swap_writepage_end(
					swap_writepage_end_handler, NULL);

		if (!test_bit(MEM_MONITOR_TYPE_SWAP_READ_START,
			     mem_monitor_register_status))
			err_code[MEM_MONITOR_TYPE_SWAP_READ_START] =
				register_trace_android_vh_swapin_start(
					swapin_start_handler, NULL);

		if (!test_bit(MEM_MONITOR_TYPE_SWAP_READ_END,
			     mem_monitor_register_status))
			err_code[MEM_MONITOR_TYPE_SWAP_READ_END] =
				register_trace_android_vh_swapin_end(
					swapin_end_handler, NULL);

		if (!test_bit(MEM_MONITOR_TYPE_TRY_TO_WAKEUP,
			     mem_monitor_register_status))
			err_code[MEM_MONITOR_TYPE_TRY_TO_WAKEUP] =
				register_trace_android_rvh_try_to_wake_up(
					try_to_wake_up_handler, NULL);

		for (i = 0; i < MEM_MONITOR_TYPE_MAX; i++) {
			if (err_code[i])
				pr_err("mem_monitor_hook register hook handler failed for %s, ret is %d\n",
					mem_monitor_hook_type[i], err_code[i]);
			else
				set_bit(i, mem_monitor_register_status);
		}
	} else {

		if (test_bit(MEM_MONITOR_TYPE_MIGRATION_ENTRY,
			     mem_monitor_register_status))
			unregister_kretprobe(&migration_entry_wait_on_locked_kretprobe);

		if (test_bit(MEM_MONITOR_TYPE_SWAP_WRITE_START,
			     mem_monitor_register_status))
			err_code[MEM_MONITOR_TYPE_SWAP_WRITE_START] =
				unregister_trace_android_vh_swap_writepage_start(
					swap_writepage_start_handler, NULL);

		if (test_bit(MEM_MONITOR_TYPE_SWAP_WRITE_END,
			     mem_monitor_register_status))
			err_code[MEM_MONITOR_TYPE_SWAP_WRITE_END] =
				unregister_trace_android_vh_swap_writepage_end(
					swap_writepage_end_handler, NULL);

		if (test_bit(MEM_MONITOR_TYPE_SWAP_READ_START,
			     mem_monitor_register_status))
			err_code[MEM_MONITOR_TYPE_SWAP_READ_START] =
				unregister_trace_android_vh_swapin_start(
					swapin_start_handler, NULL);

		if (test_bit(MEM_MONITOR_TYPE_SWAP_READ_END,
			     mem_monitor_register_status))
			err_code[MEM_MONITOR_TYPE_SWAP_READ_END] =
				unregister_trace_android_vh_swapin_end(
					swapin_end_handler, NULL);

		if (test_bit(MEM_MONITOR_TYPE_COMPACT_EXIT,
			     mem_monitor_register_status))
			err_code[MEM_MONITOR_TYPE_COMPACT_EXIT] =
				unregister_trace_android_vh_compaction_try_to_compact_exit(
					compaction_try_to_compact_exit_handler,
					NULL);

		if (test_bit(MEM_MONITOR_TYPE_COMPACT_PAGES,
			     mem_monitor_register_status))
			err_code[MEM_MONITOR_TYPE_COMPACT_PAGES] =
				unhook_tracepoint("mm_compaction_try_to_compact_pages",
					compaction_try_to_compact_pages_handler,
					NULL);

		if (test_bit(MEM_MONITOR_TYPE_DIRECT_RECLAIM_BEGIN,
			     mem_monitor_register_status))
			err_code[MEM_MONITOR_TYPE_DIRECT_RECLAIM_BEGIN] =
				unhook_tracepoint("mm_vmscan_direct_reclaim_begin",
					vmscan_direct_reclaim_begin_handler,
					NULL);

		for (i = 0; i < MEM_MONITOR_TYPE_MAX; i++) {
			if (i == MEM_MONITOR_TYPE_TRY_TO_WAKEUP)
				continue;

			if (err_code[i])
				pr_err("mem_monitor_hook unregister hook handler failed for %s, ret is %d\n",
					mem_monitor_hook_type[i], err_code[i]);
			else
				clear_bit(i, mem_monitor_register_status);
		}
	}
}

/*******************************mem_monitor_enable********************************/

static int mem_monitor_enable_show(struct seq_file *m, void *ptr)
{
	seq_printf(m, "%d\n", mem_monitor_enable);
	return 0;
}

static ssize_t mem_monitor_enable_store(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	char kbuf[4] = {0};
	int val;
	bool old_val = mem_monitor_enable;

	if (count > sizeof(kbuf) - 1)
		count = sizeof(kbuf) - 1;
	if (copy_from_user(kbuf, buf, count)) {
		mem_warn("failed to do copy_from_user\n");
		return -EFAULT;
	}
	if (kstrtoint(strstrip(kbuf), 0, &val) < 0) {
		mem_warn("failed to do kstrtoint\n");
		return -EFAULT;
	}

	mem_monitor_enable = !!val;
	if (old_val != mem_monitor_enable)
		mem_monitor_hook_register(mem_monitor_enable);

	return count;
}

static int mem_monitor_enable_open(struct inode *inode, struct file *file)
{
	return single_open(file, mem_monitor_enable_show, NULL);
}

static const struct proc_ops mem_monitor_enable_proc_ops = {
	.proc_open		= mem_monitor_enable_open,
	.proc_write		= mem_monitor_enable_store,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= single_release,
};

/*******************************mem_monitor_config********************************/

enum mem_monitor_config_cmd {
	MEM_MONITOR_CONFIG_FAILURE_PROP_ENABLE = 0,
	MEM_MONITOR_CONFIG_DEBUG_TRACE_ENABLE,
	MEM_MONITOR_CONFIG_FAILURE_PROP_WINDOW_NS,
	MEM_MONITOR_CONFIG_FAILURE_DELAY_THRESHOLD_NS,
	MEM_MONITOR_CONFIG_DIRECT_RECLAIM_WINDOW_NS,
	MEM_MONITOR_CONFIG_FAILURE_PROP_MAX_DEPTH,
	MEM_MONITOR_CONFIG_MAX
};

static int mem_monitor_config_show(struct seq_file *m, void *ptr)
{
	seq_puts(m, "Usage: echo \"cmd_id value\" > mem_monitor_config\n");
	seq_puts(m, "cmd_id:\n");
	seq_printf(m, "  %d - mem_failure_prop_enable (bool)\n",
		   MEM_MONITOR_CONFIG_FAILURE_PROP_ENABLE);
	seq_printf(m, "  %d - mem_monitor_debug_trace_enable (bool)\n",
		   MEM_MONITOR_CONFIG_DEBUG_TRACE_ENABLE);
	seq_printf(m, "  %d - mem_failure_prop_window_ns (u64)\n",
		   MEM_MONITOR_CONFIG_FAILURE_PROP_WINDOW_NS);
	seq_printf(m, "  %d - mem_failure_delay_threshold_ns (u64)\n",
		   MEM_MONITOR_CONFIG_FAILURE_DELAY_THRESHOLD_NS);
	seq_printf(m, "  %d - mem_monitor_direct_reclaim_window_ns (u64)\n",
		   MEM_MONITOR_CONFIG_DIRECT_RECLAIM_WINDOW_NS);
	seq_printf(m, "  %d - mem_failure_prop_max_depth (u16, max 20)\n",
		   MEM_MONITOR_CONFIG_FAILURE_PROP_MAX_DEPTH);
	seq_puts(m, "\nCurrent values:\n");
	seq_printf(m, "mem_failure_prop_enable: %d\n", mem_failure_prop_enable);
	seq_printf(m, "mem_monitor_debug_trace_enable: %d\n",
		   mem_monitor_debug_trace_enable);
	seq_printf(m, "mem_failure_prop_window_ns: %llu\n",
		   mem_failure_prop_window_ns);
	seq_printf(m, "mem_failure_delay_threshold_ns: %llu\n",
		   mem_failure_delay_threshold_ns);
	seq_printf(m, "mem_monitor_direct_reclaim_window_ns: %llu\n",
		   mem_monitor_direct_reclaim_window_ns);
	seq_printf(m, "mem_failure_prop_max_depth: %u\n",
		   mem_failure_prop_max_depth);
	return 0;
}

static ssize_t mem_monitor_config_store(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	char kbuf[128] = {0};
	int cmd;
	u64 val;

	if (count > sizeof(kbuf) - 1)
		count = sizeof(kbuf) - 1;
	if (copy_from_user(kbuf, buf, count)) {
		mem_warn("failed to do copy_from_user\n");
		return -EFAULT;
	}

	if (sscanf(kbuf, "%d %llu", &cmd, &val) != 2) {
		mem_warn("invalid format, expected: cmd_id value\n");
		return -EINVAL;
	}

	switch (cmd) {
	case MEM_MONITOR_CONFIG_FAILURE_PROP_ENABLE:
		mem_failure_prop_enable = !!val;
		break;
	case MEM_MONITOR_CONFIG_DEBUG_TRACE_ENABLE:
		mem_monitor_debug_trace_enable = !!val;
		break;
	case MEM_MONITOR_CONFIG_FAILURE_PROP_WINDOW_NS:
		mem_failure_prop_window_ns = val;
		break;
	case MEM_MONITOR_CONFIG_FAILURE_DELAY_THRESHOLD_NS:
		mem_failure_delay_threshold_ns = val;
		break;
	case MEM_MONITOR_CONFIG_DIRECT_RECLAIM_WINDOW_NS:
		mem_monitor_direct_reclaim_window_ns = val;
		break;
	case MEM_MONITOR_CONFIG_FAILURE_PROP_MAX_DEPTH:
		if (val > 20)
			val = 20;
		mem_failure_prop_max_depth = (unsigned short)val;
		break;
	default:
		mem_warn("invalid cmd_id: %d\n", cmd);
		return -EINVAL;
	}

	return count;
}

static int mem_monitor_config_open(struct inode *inode, struct file *file)
{
	return single_open(file, mem_monitor_config_show, NULL);
}

static const struct proc_ops mem_monitor_config_proc_ops = {
	.proc_open		= mem_monitor_config_open,
	.proc_write		= mem_monitor_config_store,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= single_release,
};

/*******************************mem_monitor_status********************************/

static int mem_monitor_status_show(struct seq_file *m, void *ptr)
{
	int i;

	seq_puts(m, "mem_monitor_register_status:\n");
	for (i = 0; i < MEM_MONITOR_TYPE_MAX; i++)
		seq_printf(m, "%s:%d\n", mem_monitor_hook_type[i],
			   test_bit(i, mem_monitor_register_status));

	seq_puts(m, "\nmem_monitor_nmissed:\n");
	seq_printf(m, "migration_entry_wait_on_locked: %d\n",
		   migration_entry_wait_on_locked_kretprobe.nmissed);
	return 0;
}

static int mem_monitor_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, mem_monitor_status_show, NULL);
}

static const struct proc_ops mem_monitor_status_proc_ops = {
	.proc_open		= mem_monitor_status_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= single_release,
};

static struct proc_dir_entry *mem_dir;
static void init_procs(void)
{
	mem_dir = xiaomi_proc_mkdir(PROC_TRACE_MEM, NULL);
	if (!mem_dir) {
		printk("huqinghe failed to create dir: proc/mem_monitor\n");
		return;
	}

	proc_create_data("lat_info", S_IRUGO, mem_dir, &mem_common_proc_ops, NULL);
	proc_create_data("enable", S_IRUGO | S_IWUGO, mem_dir, &mem_common_proc_ops, &mem_enable);
	proc_create("reset", S_IRUGO | S_IWUGO, mem_dir, &reset_fops);
	proc_create("mem_monitor_enable", S_IRUGO | S_IWUGO, mem_dir, &mem_monitor_enable_proc_ops);
	proc_create("mem_monitor_config", S_IRUGO | S_IWUGO, mem_dir, &mem_monitor_config_proc_ops);
	proc_create("mem_monitor_status", S_IRUGO, mem_dir, &mem_monitor_status_proc_ops);
}

int xiaomi_mem_trace_init(void)
{
	init_cgroup_id();
	init_procs();
	mem_enable = 1;
	mem_enable_change(mem_enable);
	printk("huqinghe load sch monitor\n");
	return 0;
}

void xiaomi_mem_trace_exit(void)
{
	if (mem_dir) {
		remove_proc_entry("lat_info", mem_dir);
		remove_proc_entry("enable", mem_dir);
		remove_proc_entry("reset", mem_dir);
		remove_proc_entry("mem_monitor_enable", mem_dir);
		remove_proc_entry("mem_monitor_config", mem_dir);
		remove_proc_entry("mem_monitor_status", mem_dir);
		remove_proc_subtree(PROC_TRACE_MEM, NULL);
		mem_dir = NULL;
	}

	mem_enable = 0;
	mem_enable_change(mem_enable);
	printk("huqinghe de-load sch monitor\n");
	return;
}
