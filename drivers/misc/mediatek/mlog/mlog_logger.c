/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/types.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/sched.h>
#include <linux/vmstat.h>
#include <linux/sysinfo.h>
#include <linux/swap.h>
#include <linux/cpu.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cpumask.h>
#include <linux/cred.h>
#include <linux/rcupdate.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#ifdef CONFIG_MTK_GPU_SUPPORT
#define COLLECT_GPU_MEMINFO
#endif

#ifdef COLLECT_GPU_MEMINFO
#include <mt-plat/mtk_gpu_utility.h>
#endif

#ifdef CONFIG_ZRAM
#include <zram_drv.h>
#endif

/* for collecting ion total memory usage*/
#ifdef CONFIG_MTK_ION
#include <mtk/ion_drv.h>
#endif

#include "mlog_internal.h"
#include "mlog_logger.h"

#define CONFIG_MLOG_BUF_SHIFT   16	/* 64KB for 32bit, 128kB for 64bit */

#define P2K(x) (((unsigned long)x) << (PAGE_SHIFT - 10))
#define B2K(x) (((unsigned long)x) >> (10))


#define MLOG_STR_LEN        32
#define MLOG_BUF_LEN        ((1 << CONFIG_MLOG_BUF_SHIFT) >> 2)
#define MLOG_BUF_MASK       (mlog_buf_len-1)
#define MLOG_BUF(idx)       (mlog_buffer[(idx) & MLOG_BUF_MASK])

#define MLOG_ID             ULONG_MAX

#define AID_ROOT            0	/* traditional unix root user */
#define AID_SYSTEM          1000	/* system server */

#define M_MEMFREE           (1 << 0)
#define M_SWAPFREE          (1 << 1)
#define M_CACHED            (1 << 2)
#define M_KERNEL            (1 << 3)
#define M_PAGE_TABLE        (1 << 4)
#define M_SLAB              (1 << 5)
#define M_GPUUSE            (1 << 6)
#define M_MLOCK             (1 << 7)
#define M_ZRAM              (1 << 8)
#define M_ACTIVE            (1 << 9)
#define M_INACTIVE          (1 << 10)
#define M_SHMEM             (1 << 11)
#define M_GPU_PAGE_CACHE    (1 << 12)
#define M_ION               (1 << 13)

#define V_PSWPIN            (1 << 0)
#define V_PSWPOUT           (1 << 1)
#define V_PGFMFAULT         (1 << 2)

#define P_ADJ               (1 << 0)
#define P_RSS               (1 << 1)
#define P_RSWAP             (1 << 2)
#define P_SWPIN             (1 << 3)
#define P_SWPOUT            (1 << 4)
#define P_FMFAULT           (1 << 5)
#define P_MINFAULT          (1 << 6)
#define P_MAJFAULT          (1 << 7)

#define B_NORMAL            (1 << 0)
#define B_HIGH              (1 << 1)

#define P_FMT_SIZE          (P_RSS | P_RSWAP)
#define P_FMT_COUNT         (P_SWPIN | P_SWPOUT | P_FMFAULT | P_MINFAULT \
				| P_MAJFAULT)

#define M_FILTER_ALL        (M_MEMFREE | M_SWAPFREE | M_CACHED \
				| M_KERNEL | M_PAGE_TABLE | M_SLAB \
				| M_GPUUSE | M_GPU_PAGE_CACHE | M_MLOCK \
				| M_ZRAM | M_ACTIVE | M_INACTIVE \
				| M_SHMEM | M_ION)

#define V_FILTER_ALL        (V_PSWPIN | V_PSWPOUT | V_PGFMFAULT)
#define P_FILTER_ALL        (P_ADJ | P_RSS | P_RSWAP | P_SWPIN \
				| P_SWPOUT | P_FMFAULT)
#define B_FILTER_ALL        (B_NORMAL | B_HIGH)

#define MLOG_TRIGGER_TIMER  0
#define MLOG_TRIGGER_LMK    1
#define MLOG_TRIGGER_LTK    2

#define VMSTAT_EVENTALL_STOP			0
#define VMSTAT_EVENTALL_START			1
#define VMSTAT_EVENTALL_START_NO_SUCCEED	2

static uint meminfo_filter = M_FILTER_ALL;
static uint vmstat_filter = V_FILTER_ALL;
static uint vmstat_eventall = VMSTAT_EVENTALL_STOP;
static uint proc_filter = P_FILTER_ALL;
static uint buddyinfo_filter = B_FILTER_ALL;

static DEFINE_SPINLOCK(mlogbuf_lock);
DECLARE_WAIT_QUEUE_HEAD(mlog_wait);
static long mlog_buffer[MLOG_BUF_LEN];
static int mlog_buf_len = MLOG_BUF_LEN;
static unsigned int mlog_start;
static unsigned int mlog_end;

static int min_adj = -1000;
static int max_adj = 1000;
static int limit_pid = -1;

static struct timer_list mlog_timer;
static unsigned long timer_intval = HZ;

static const char **strfmt_list;
static int strfmt_idx;
static int strfmt_len;
static int strfmt_proc;

static const char cr_str[] = "%c";
static const char type_str[] = "<%ld>";
static const char time_sec_str[] = ",[%5lu";
static const char time_nanosec_str[] = ".%06lu]";
static const char mem_size_str[] = ",%6lu";
static const char acc_count_str[] = ",%7lu";
static const char pid_str[] = ",[%lu]";
static const char pname_str[] = ", %s";
static const char adj_str[] = ", %5ld";

/*
 * buddyinfo
 * Node 0, zone   Normal    486    297    143     59     30     16      7      0      2      1     54
 * Node 0, zone  HighMem     74     18      7     65    161     67     23     10      0      1     21
 */
static const char order_start_str[] = ", [%6lu";
static const char order_middle_str[] = ", %6lu";
static const char order_end_str[] = ", %6lu]";

/*
 * active & inactive
 * Active:           211748 kB
 * Inactive:         257988 kB
 */
struct mlog_header {
	char *buffer;
	size_t index;
	size_t len;
};

struct mlog_session {
	unsigned int start;
	unsigned int end;
	int fmt_idx;
	bool is_header_dump;
	struct mlog_header header;
};


static void mlog_emit_32(long v)
{
	MLOG_BUF(mlog_end) = v;
	mlog_end++;
	if (mlog_end - mlog_start > mlog_buf_len)
		mlog_start = mlog_end - mlog_buf_len;
}

static void mlog_reset_format(void)
{
	int len;
	uint vm_eventall = vmstat_eventall;
	uint vm_filter = vmstat_filter;

	spin_lock_bh(&mlogbuf_lock);

	if (meminfo_filter)
		meminfo_filter = M_FILTER_ALL;
	if (vm_filter) {
		vm_filter = V_FILTER_ALL;
		vmstat_filter = V_FILTER_ALL;
	}
	if (buddyinfo_filter)
		buddyinfo_filter = B_FILTER_ALL;
	if (proc_filter)
		proc_filter = P_FILTER_ALL;

	/* calc len */
	len = 4;		/* id, type, sec, nanosec */

	len += hweight32(meminfo_filter);

	if (vm_filter && vm_eventall == VMSTAT_EVENTALL_STOP)
		len += hweight32(vm_filter);
	else
		len += NR_VM_EVENT_ITEMS;

	if (vm_eventall != VMSTAT_EVENTALL_START_NO_SUCCEED) {
		/* buddyinfo */
		len += (2 * MAX_ORDER);

		if (proc_filter) {
			len++;		/* PID */
#ifdef PRINT_PROCESS_NAME_DEBUG
			len++;		/* Process name */
#endif
			len += hweight32(proc_filter);
		}
	}

	if (!strfmt_list || strfmt_len != len) {
		kfree(strfmt_list);
		strfmt_list = kmalloc_array(len, sizeof(char *), GFP_ATOMIC);
		strfmt_len = len;

		//if (!strfmt_list)
		//	aee_kernel_exception("mlog",
		//	"unaligned strfmt variable and len\n");
	}

	/* setup str format */
	len = 0;
	strfmt_proc = 0;
	strfmt_list[len++] = cr_str;
	strfmt_list[len++] = type_str;
	strfmt_list[len++] = time_sec_str;
	strfmt_list[len++] = time_nanosec_str;

	if (meminfo_filter) {
		int i;

		for (i = 0; i < hweight32(meminfo_filter); ++i)
			strfmt_list[len++] = mem_size_str;
	}

	if (vm_eventall <= VMSTAT_EVENTALL_START) {
		if (vm_filter && vm_eventall == VMSTAT_EVENTALL_STOP) {
			int i;

			for (i = 0; i < hweight32(vm_filter); ++i)
				strfmt_list[len++] = acc_count_str;
		} else {
			int i;

			for (i = 0; i < NR_VM_EVENT_ITEMS; i++)
				strfmt_list[len++] = acc_count_str;
		}

		if (buddyinfo_filter) {
			int i, j;

			/* normal and high zone */
			for (i = 0; i < 2; ++i) {
				strfmt_list[len++] = order_start_str;
				for (j = 0; j < MAX_ORDER - 2; ++j)
					strfmt_list[len++] = order_middle_str;

				strfmt_list[len++] = order_end_str;
			}
		}

		if (proc_filter) {
			int i;

			strfmt_proc = len;
			strfmt_list[len++] = pid_str;	/* PID */
#ifdef PRINT_PROCESS_NAME_DEBUG
			strfmt_list[len++] = pname_str;	/* Process name */
#endif
			strfmt_list[len++] = adj_str;	/* ADJ */
			i = 0;
			for (; i < hweight32(proc_filter & (P_FMT_SIZE)); ++i)
				strfmt_list[len++] = mem_size_str;
			i = 0;
			for (; i < hweight32(proc_filter & (P_FMT_COUNT)); ++i)
				strfmt_list[len++] = acc_count_str;
		}
	} else {
		int i;

		for (i = 0; i < NR_VM_EVENT_ITEMS; i++)
			strfmt_list[len++] = acc_count_str;
	}

	strfmt_idx = 0;

	spin_unlock_bh(&mlogbuf_lock);

	pr_debug("[mlog] reset format %d(%d)\n", strfmt_len, len);
}

int mlog_snprint_fmt(char *buf, size_t len)
{
	int ret = 0;

	ret = snprintf(buf, len, "<type>,    [time]");

	if (meminfo_filter & M_MEMFREE)
		ret += snprintf(buf + ret, len - ret, ", memfr");
	if (meminfo_filter & M_SWAPFREE)
		ret += snprintf(buf + ret, len - ret, ", swpfr");
	if (meminfo_filter & M_CACHED)
		ret += snprintf(buf + ret, len - ret, ", cache");
	if (meminfo_filter & M_GPUUSE)
		ret += snprintf(buf + ret, len - ret, ", kernel_stack");
	if (meminfo_filter & M_GPUUSE)
		ret += snprintf(buf + ret, len - ret, ", page_table");
	if (meminfo_filter & M_GPUUSE)
		ret += snprintf(buf + ret, len - ret, ", slab");
	if (meminfo_filter & M_GPUUSE)
		ret += snprintf(buf + ret, len - ret, ",   gpu");
	if (meminfo_filter & M_GPU_PAGE_CACHE)
		ret += snprintf(buf + ret, len - ret, ",   gpu_page_cache");
	if (meminfo_filter & M_MLOCK)
		ret += snprintf(buf + ret, len - ret, ", mlock");
	if (meminfo_filter & M_ZRAM)
		ret += snprintf(buf + ret, len - ret, ",  zram");
	if (meminfo_filter & M_ACTIVE)
		ret += snprintf(buf + ret, len - ret, ",  active");
	if (meminfo_filter & M_INACTIVE)
		ret += snprintf(buf + ret, len - ret, ",  inactive");
	if (meminfo_filter & M_SHMEM)
		ret += snprintf(buf + ret, len - ret, ",  shmem");
	if (meminfo_filter & M_ION)
		ret += snprintf(buf + ret, len - ret, ",  ion");

	if (vmstat_eventall == VMSTAT_EVENTALL_STOP) {
		if (vmstat_filter & V_PSWPIN)
			ret += snprintf(buf + ret, len - ret, ",  swpin");
		if (vmstat_filter & V_PSWPOUT)
			ret += snprintf(buf + ret, len - ret, ", swpout");
		if (vmstat_filter & V_PGFMFAULT)
			ret += snprintf(buf + ret, len - ret, ",  fmflt");
	} else {
		int vm_event_offset = NR_VM_ZONE_STAT_ITEMS +
				      NR_VM_NODE_STAT_ITEMS +
				      2;/* NR_VM_WRITEBACK_STAT_ITEMS */
		int vm_event_size = vm_event_offset + NR_VM_EVENT_ITEMS;

		for (; vm_event_offset < vm_event_size; vm_event_offset++)
			ret += snprintf(buf + ret,
					len - ret,
					",%s", vmstat_text[vm_event_offset]);
	}

	if (vmstat_eventall != VMSTAT_EVENTALL_START_NO_SUCCEED) {
		if (buddyinfo_filter) {
			int order;

			ret += snprintf(buf + ret, len - ret, ",  [normal: 0");
			for (order = 1; order < MAX_ORDER; ++order)
				ret += snprintf(buf + ret,
						len - ret,
						", %d", order);

			ret += snprintf(buf + ret, len - ret,	"]");
			ret += snprintf(buf + ret, len - ret, ",  [high: 0");

			for (order = 1; order < MAX_ORDER; ++order)
				ret += snprintf(buf + ret,
						len - ret,
						", %d", order);

			ret += snprintf(buf + ret, len - ret,	"]");
		}

		if (proc_filter) {
			ret += snprintf(buf + ret, len - ret, ", [pid]");
#ifdef PRINT_PROCESS_NAME_DEBUG
			ret += snprintf(buf + ret, len - ret, ", name");
#endif
			if (proc_filter & P_ADJ)
				ret += snprintf(buf + ret, len - ret,
						", score_adj");
			if (proc_filter & P_RSS)
				ret += snprintf(buf + ret, len - ret, ", rss");
			if (proc_filter & P_RSWAP)
				ret += snprintf(buf + ret, len - ret, ", rswp");
			if (proc_filter & P_SWPIN)
				ret += snprintf(buf + ret, len - ret,
						", pswpin");
			if (proc_filter & P_SWPOUT)
				ret += snprintf(buf + ret, len - ret,
						", pswpout");
			if (proc_filter & P_FMFAULT)
				ret += snprintf(buf + ret, len - ret,
						", pfmflt");
		}
	}
	return ret;
}

int mlog_header_dump(char __user *buf, size_t len, struct mlog_header *header)
{
	int ret = min(len, header->len - header->index);

	if (__copy_to_user(buf, header->buffer + header->index, ret))
		return -EFAULT;

	header->index += ret;
	return ret;
}

int mlog_print_fmt(struct seq_file *m)
{
	seq_puts(m, "<type>,    [time]");

	if (meminfo_filter & M_MEMFREE)
		seq_puts(m, ", memfr");
	if (meminfo_filter & M_SWAPFREE)
		seq_puts(m, ", swpfr");
	if (meminfo_filter & M_CACHED)
		seq_puts(m, ", cache");
	if (meminfo_filter & M_GPUUSE)
		seq_puts(m, ", kernel_stack");
	if (meminfo_filter & M_GPUUSE)
		seq_puts(m, ", page_table");
	if (meminfo_filter & M_GPUUSE)
		seq_puts(m, ", slab");
	if (meminfo_filter & M_GPUUSE)
		seq_puts(m, ",   gpu");
	if (meminfo_filter & M_GPU_PAGE_CACHE)
		seq_puts(m, ",   gpu_page_cache");
	if (meminfo_filter & M_MLOCK)
		seq_puts(m, ", mlock");
	if (meminfo_filter & M_ZRAM)
		seq_puts(m, ",  zram");
	if (meminfo_filter & M_ACTIVE)
		seq_puts(m, ",  active");
	if (meminfo_filter & M_INACTIVE)
		seq_puts(m, ",  inactive");
	if (meminfo_filter & M_SHMEM)
		seq_puts(m, ",  shmem");
	if (meminfo_filter & M_ION)
		seq_puts(m, ",  ion");

	if (vmstat_eventall == VMSTAT_EVENTALL_STOP) {
		if (vmstat_filter & V_PSWPIN)
			seq_puts(m, ",  swpin");
		if (vmstat_filter & V_PSWPOUT)
			seq_puts(m, ", swpout");
		if (vmstat_filter & V_PGFMFAULT)
			seq_puts(m, ",  fmflt");
	} else {
		int vm_event_offset = NR_VM_ZONE_STAT_ITEMS +
				      NR_VM_NODE_STAT_ITEMS +
				      2;/* NR_VM_WRITEBACK_STAT_ITEMS */
		int vm_event_size = vm_event_offset + NR_VM_EVENT_ITEMS;

		for (; vm_event_offset < vm_event_size; vm_event_offset++)
			seq_printf(m, ",%s", vmstat_text[vm_event_offset]);
	}

	if (vmstat_eventall != VMSTAT_EVENTALL_START_NO_SUCCEED) {
		if (buddyinfo_filter) {
			int order;

			seq_puts(m, ",  [normal: 0");
			for (order = 1; order < MAX_ORDER; ++order)
				seq_printf(m,	", %d", order);

			seq_puts(m,	"]");
			seq_puts(m, ",  [high: 0");

			for (order = 1; order < MAX_ORDER; ++order)
				seq_printf(m,	", %d", order);

			seq_puts(m,	"]");
		}

		if (proc_filter) {
			seq_puts(m, ", [pid]");
#ifdef PRINT_PROCESS_NAME_DEBUG
			seq_puts(m, ", name");
#endif
			if (proc_filter & P_ADJ)
				seq_puts(m, ", score_adj");
			if (proc_filter & P_RSS)
				seq_puts(m, ", rss");
			if (proc_filter & P_RSWAP)
				seq_puts(m, ", rswp");
			if (proc_filter & P_SWPIN)
				seq_puts(m, ", pswpin");
			if (proc_filter & P_SWPOUT)
				seq_puts(m, ", pswpout");
			if (proc_filter & P_FMFAULT)
				seq_puts(m, ", pfmflt");
		}
	}

	seq_puts(m, "\n");
	return 0;
}

static void mlog_reset_buffer(void)
{
	spin_lock_bh(&mlogbuf_lock);
	mlog_end = mlog_start = 0;
	spin_unlock_bh(&mlogbuf_lock);

	MLOG_PRINTK("[mlog] reset buffer\n");
}

#ifndef CONFIG_MTKPASR
#define mtkpasr_show_page_reserved(void) (0)
#endif

static void mlog_meminfo(void)
{
	unsigned long memfree;
	unsigned long swapfree;
	unsigned long cached;
	unsigned long kernel_stack;
	unsigned long page_table;
	unsigned long slab;
	unsigned int gpuuse = 0;
	unsigned int gpu_page_cache = 0;
	unsigned long mlock;
	unsigned long zram;
	unsigned long active, inactive;
	unsigned long shmem;
	unsigned long ion = 0;

	memfree = P2K(global_page_state(NR_FREE_PAGES) +
			mtkpasr_show_page_reserved());
	swapfree = P2K(atomic_long_read(&nr_swap_pages));
	cached = P2K(global_node_page_state(NR_FILE_PAGES) -
			total_swapcache_pages());
	/*
	 * use following code if kernel version is under 3.10.
	 * swapfree = P2K(nr_swap_pages);
	 * cached = P2K(global_page_state(NR_FILE_PAGES) -
	 * total_swapcache_pages);
	 */

	kernel_stack = global_page_state(NR_KERNEL_STACK_KB);
	page_table   = P2K(global_page_state(NR_PAGETABLE));
	slab         = P2K(global_page_state(NR_SLAB_UNRECLAIMABLE) +
			global_page_state(NR_SLAB_RECLAIMABLE));

#ifdef COLLECT_GPU_MEMINFO
	if (mtk_get_gpu_memory_usage(&gpuuse))
		gpuuse = B2K(gpuuse);
	if (mtk_get_gpu_page_cache(&gpu_page_cache))
		gpu_page_cache = B2K(gpu_page_cache);
#endif

	mlock = P2K(global_page_state(NR_MLOCK));
#if defined(CONFIG_ZRAM) && defined(CONFIG_ZSMALLOC)
	zram = zram_mlog();
#else
	zram = 0;
#endif

	active = P2K(global_node_page_state(NR_ACTIVE_ANON) +
			global_node_page_state(NR_ACTIVE_FILE));
	inactive = P2K(global_node_page_state(NR_INACTIVE_ANON) +
			global_node_page_state(NR_INACTIVE_FILE));
	/* MLOG_PRINTK("active: %lu, inactive: %lu\n", active, inactive); */
	shmem = P2K(global_node_page_state(NR_SHMEM));

#ifdef CONFIG_MTK_ION
	ion = B2K((unsigned long)ion_mm_heap_total_memory());
#endif

	spin_lock_bh(&mlogbuf_lock);
	mlog_emit_32(memfree);
	mlog_emit_32(swapfree);
	mlog_emit_32(cached);

	/* kernel memory usage */
	mlog_emit_32(kernel_stack);
	mlog_emit_32(page_table);
	mlog_emit_32(slab);
	/* hardware memory usage */
	mlog_emit_32(gpuuse);
	mlog_emit_32(gpu_page_cache);
	mlog_emit_32(mlock);
	mlog_emit_32(zram);
	mlog_emit_32(active);
	mlog_emit_32(inactive);
	mlog_emit_32(shmem);
	mlog_emit_32(ion);
	spin_unlock_bh(&mlogbuf_lock);
}

static void mlog_vmstat(void)
{
	int cpu;
	unsigned long v[NR_VM_EVENT_ITEMS];

	memset(v, 0, NR_VM_EVENT_ITEMS * sizeof(unsigned long));

	for_each_online_cpu(cpu) {
		struct vm_event_state *this = &per_cpu(vm_event_states, cpu);

		v[PSWPIN] += this->event[PSWPIN];
		v[PSWPOUT] += this->event[PSWPOUT];
		v[PGFMFAULT] += this->event[PGFMFAULT];
	}

	spin_lock_bh(&mlogbuf_lock);
	mlog_emit_32(v[PSWPIN]);
	mlog_emit_32(v[PSWPOUT]);
	mlog_emit_32(v[PGFMFAULT]);
	spin_unlock_bh(&mlogbuf_lock);
}

static void mlog_vmstat_eventall(void)
{
	int cpu, i;
	unsigned long v[NR_VM_EVENT_ITEMS];

	memset(v, 0, NR_VM_EVENT_ITEMS * sizeof(unsigned long));

	for_each_online_cpu(cpu) {
		struct vm_event_state *this = &per_cpu(vm_event_states, cpu);

		for (i = 0; i < NR_VM_EVENT_ITEMS; i++)
			v[i] += this->event[i];
	}
	v[PGPGIN] /= 2;		/* sectors -> kbytes */
	v[PGPGOUT] /= 2;

	spin_lock_bh(&mlogbuf_lock);
	for (i = 0; i < NR_VM_EVENT_ITEMS; i++)
		mlog_emit_32(v[i]);
	spin_unlock_bh(&mlogbuf_lock);
}

/* static void mlog_buddyinfo(void) */
void mlog_buddyinfo(void)
{
	int i;
	struct zone *zone;
	struct zone *node_zones;
	unsigned int order;
	int zone_nr = 0;
	unsigned long normal_nr_free[MAX_ORDER] = {0};
	unsigned long high_nr_free[MAX_ORDER] = {0};

	for_each_online_node(i) {
		pg_data_t *pgdat = NODE_DATA(i);
		unsigned long flags;

		node_zones = pgdat->node_zones;

		/* MAX_NR_ZONES 3 */
		for (zone = node_zones; zone - node_zones < MAX_NR_ZONES;
				++zone) {
			if (!populated_zone(zone))
				continue;

			spin_lock_irqsave(&zone->lock, flags);

			zone_nr++;

			for (order = 0; order < MAX_ORDER; ++order) {
				if (zone_nr == 1)
					normal_nr_free[order] =
						zone->free_area[order].nr_free;
				if (zone_nr == 2)
					high_nr_free[order] =
						zone->free_area[order].nr_free;
			}
			spin_unlock_irqrestore(&zone->lock, flags);
		}
	}


	if (zone_nr == 1) {
		for (order = 0; order < MAX_ORDER; ++order)
			high_nr_free[order] = 0;
	}
#ifdef CONFIG_MTKPASR
	if (zone_nr == 2)
		high_nr_free[MAX_ORDER - 1] +=
			(mtkpasr_show_page_reserved() >> (MAX_ORDER - 1));

#endif

	spin_lock_bh(&mlogbuf_lock);

	for (order = 0; order < MAX_ORDER; ++order)
		mlog_emit_32(normal_nr_free[order]);


	for (order = 0; order < MAX_ORDER; ++order)
		mlog_emit_32(high_nr_free[order]);


	spin_unlock_bh(&mlogbuf_lock);
}

struct task_struct *find_trylock_task_mm(struct task_struct *t)
{
	if (spin_trylock(&t->alloc_lock)) {
		if (likely(t->mm))
			return t;
		task_unlock(t);
	}
	return NULL;
}

static void mlog_procinfo(void)
{
	struct task_struct *tsk;

	rcu_read_lock();
	for_each_process(tsk) {
		int oom_score_adj;
		const struct cred *cred = NULL;
		struct task_struct *real_parent;
		struct task_struct *p;
		pid_t ppid;
		struct task_struct *t;
		unsigned long swap_in, swap_out, fm_flt, min_flt, maj_flt;
		unsigned long rss;
		unsigned long rswap;

		if (tsk->flags & PF_KTHREAD)
			continue;

		p = find_trylock_task_mm(tsk);
		if (!p)
			continue;

		if (!p->signal)
			goto unlock_continue;

		oom_score_adj = p->signal->oom_score_adj;

		if (max_adj < oom_score_adj || oom_score_adj < min_adj)
			goto unlock_continue;

		if (limit_pid != -1 && p->pid != limit_pid)
			goto unlock_continue;

		cred = get_task_cred(p);
		if (!cred)
			goto unlock_continue;

		/*
		 * camerahalserver is a suspect in many ANR/FLM cases.
		 */
		if (strncmp("camerahalserver", p->comm, TASK_COMM_LEN) == 0)
			goto collect_proc_mem_info;

		/* skip root user */
		if (__kuid_val(cred->uid) == AID_ROOT)
			goto unlock_continue;

		real_parent = rcu_dereference(p->real_parent);
		if (!real_parent)
			goto unlock_continue;

		ppid = real_parent->pid;
		/* skip non java proc (parent is init) */
		if (ppid == 1)
			goto unlock_continue;

		if (oom_score_adj == -16) {
			/* only keep system server */
			if (__kuid_val(cred->uid) != AID_SYSTEM)
				goto unlock_continue;
		}

collect_proc_mem_info:
		/* reset data */
		swap_in = swap_out = fm_flt = min_flt = maj_flt = 0;

		/* all threads */
		t = p;
		do {
			/* min_flt += t->min_flt; */
			/* maj_flt += t->maj_flt; */

			fm_flt += t->fm_flt;
#ifdef CONFIG_SWAP
			swap_in += t->swap_in;
			swap_out += t->swap_out;
#endif
			t = next_thread(t);
#ifdef MLOG_DEBUG
#if defined(__LP64__) || defined(_LP64)
			if ((long long)t < 0xffffffc000000000)
				break;
#endif
#endif

		} while (t && t != p);

		/* emit log */
		rss = P2K(get_mm_rss(p->mm));
		rswap = P2K(get_mm_counter(p->mm, MM_SWAPENTS));

		spin_lock_bh(&mlogbuf_lock);
		mlog_emit_32(p->pid);
#ifdef PRINT_PROCESS_NAME_DEBUG
		mlog_emit_32((unsigned long)p->comm);
#endif
		mlog_emit_32(oom_score_adj);
		mlog_emit_32(rss);
		mlog_emit_32(rswap);
		mlog_emit_32(swap_in);
		mlog_emit_32(swap_out);
		mlog_emit_32(fm_flt);
		/* mlog_emit_32(min_flt); */
		/* mlog_emit_32(maj_flt); */
		spin_unlock_bh(&mlogbuf_lock);

 unlock_continue:
		if (cred)
			put_cred(cred);

		task_unlock(p);
	}
	rcu_read_unlock();

}

void mlog(int type)
{
	/* unsigned long flag; */
	unsigned long microsec_rem;
	unsigned long long t = local_clock();
#ifdef PROFILE_MLOG_OVERHEAD
	unsigned long long t1 = t;
#endif

	/* time stamp */
	microsec_rem = do_div(t, 1000000000);

	/* spin_lock_irqsave(&mlogbuf_lock, flag); */

	spin_lock_bh(&mlogbuf_lock);
	mlog_emit_32(MLOG_ID);	/* tag for correct start point */
	mlog_emit_32(type);
	mlog_emit_32((unsigned long)t);
	mlog_emit_32(microsec_rem / 1000);
	spin_unlock_bh(&mlogbuf_lock);

	/* memory log */
	if (meminfo_filter)
		mlog_meminfo();

	if (vmstat_eventall <= VMSTAT_EVENTALL_START) {
		if (vmstat_filter && vmstat_eventall == VMSTAT_EVENTALL_STOP)
			mlog_vmstat();
		else
			mlog_vmstat_eventall();

		if (buddyinfo_filter)
			mlog_buddyinfo();

		if (proc_filter)
			mlog_procinfo();
	} else {
		mlog_vmstat_eventall();
	}

	/*
	 * mlog buffer have something to dump
	 */
	if (waitqueue_active(&mlog_wait))
		wake_up_interruptible(&mlog_wait);

#ifdef PROFILE_MLOG_OVERHEAD
	MLOG_PRINTK("[mlog] %llu ns\n", local_clock() - t1);
#endif
}
EXPORT_SYMBOL(mlog);

void mlog_doopen(void)
{
	spin_lock_bh(&mlogbuf_lock);
	strfmt_idx = 0;
	spin_unlock_bh(&mlogbuf_lock);
}

int mlog_unread(void)
{
	return mlog_end - mlog_start;
}

static int _doread(char __user *buf, size_t len, unsigned int *start,
		unsigned int *end, int *fmt_idx)
{
	int size = 0;
	long v;
	char mlog_buf[MLOG_STR_LEN];

	spin_lock_bh(&mlogbuf_lock);
	/* mlog_start go over session->start, no data to dump */
	if (unlikely(*start < mlog_start))
		goto exit_dump;


	while (*start < *end) {
		/* retrieve value */
		v = MLOG_BUF(*start);
		*start += 1;

		if (*fmt_idx == 0 && v != MLOG_ID)
			continue;
		else if (v == MLOG_ID && (*fmt_idx != 0))
			*fmt_idx = 0;

		break;
	}

	if (*start >= *end)
		goto exit_dump;

	/* strfmt_list is changed, just reset the index. */
	if (*fmt_idx >= strfmt_len)
		*fmt_idx = strfmt_proc;

	if (*fmt_idx == 0)
		v = '\n';

	size = snprintf(mlog_buf, MLOG_STR_LEN, strfmt_list[*fmt_idx], v);
	*fmt_idx += 1;

	if (*fmt_idx >= strfmt_len)
		*fmt_idx = strfmt_proc;

	spin_unlock_bh(&mlogbuf_lock);

	if (__copy_to_user(buf, mlog_buf, size))
		return -EFAULT;

	return size;

exit_dump:
	spin_unlock_bh(&mlogbuf_lock);
	return size;
}

int mlog_doread(char __user *buf, size_t len)
{
	int error;
	size_t size = 0;
	size_t ret;

	if (!buf || len < 0)
		return -EINVAL;
	if (len == 0)
		return 0;
	if (!access_ok(VERIFY_WRITE, buf, len))
		return -EFAULT;

	error = wait_event_interruptible(mlog_wait, (mlog_start - mlog_end));
	if (error)
		return error;

	while (len - size > MLOG_STR_LEN) {
		ret = _doread(buf + size, len - size, &mlog_start,
				&mlog_end, &strfmt_idx);

		if (ret == 0)
			break;
		size = size + ret;
		cond_resched();
	}
	return size;
}

int dmlog_open(struct inode *inode, struct file *file)
{
	struct mlog_session *session;
	struct mlog_header *header;
	int fmt_buf_len = 512;

	if (vmstat_eventall != VMSTAT_EVENTALL_STOP)
		fmt_buf_len += 1024;

	session = kzalloc(sizeof(struct mlog_session), GFP_KERNEL);
	session->start = mlog_start;
	session->end = mlog_end;
	session->fmt_idx = 0;
	header = &session->header;
	header->buffer = kmalloc(fmt_buf_len, GFP_KERNEL);
	header->len = mlog_snprint_fmt(header->buffer, fmt_buf_len);

	file->private_data = session;
	return 0;
}

int dmlog_release(struct inode *inode, struct file *file)
{
	struct mlog_session *session = file->private_data;
	struct mlog_header *header = &session->header;

	kfree(header->buffer);
	kfree(file->private_data);
	return 0;
}

ssize_t dmlog_read(struct file *file, char __user *buf, size_t len,
		loff_t *ppos)
{
	size_t size = 0;
	size_t ret;
	struct mlog_session *session = file->private_data;
	struct mlog_header *header = &session->header;

	if (!buf || len < 0)
		return -EINVAL;
	if (len == 0)
		return 0;
	if (!access_ok(VERIFY_WRITE, buf, len))
		return -EFAULT;

	if (!session->is_header_dump) {
		ret = mlog_header_dump(buf, len, header);
		size = size + ret;
		if (header->index >= header->len)
			session->is_header_dump = true;
	}

	while (len - size > MLOG_STR_LEN) {
		ret = _doread(buf + size, len - size, &session->start,
				&session->end, &session->fmt_idx);

		/* start go reach end */
		if (ret == 0)
			break;

		size = size + ret;
		cond_resched();
	}
	return size;
}

/* Get mlog_buffer & its offset */
void mlog_get_buffer(char **ptr, int *size)
{
#ifdef CONFIG_MTK_ENG_BUILD
#define MLOG_MSG_LENGTH	(3072)
#define MLOG_PRINT(args...)	do {\
					v = MLOG_BUF(start++);\
					offset = sprintf(msg_pos, args);\
					msg_pos += offset;\
					msg_stored += offset;\
				} while (0)

	static char mlog_msg[MLOG_MSG_LENGTH];
	unsigned int start, end;
	long v;
	char *msg_pos;
	int msg_stored = 0, offset, i;

	msg_pos = mlog_msg;
	start = mlog_start;
	end = mlog_end;
	while ((end - start) <= (mlog_buf_len + 1)) {
		v = MLOG_BUF(start);
		start += 1;
		if (v != MLOG_ID)
			continue;

		/* bypss type */
		start += 1;

		/* time */
		if (MLOG_MSG_LENGTH < (msg_stored + 16))
			break;

		MLOG_PRINT("%ld.", v);
		MLOG_PRINT("%ld", v);

		/* status */
		for (i = 0; i < 17; i++) {
			if (MLOG_MSG_LENGTH < (msg_stored + 22))
				break;

			MLOG_PRINT(" %ld", v);
		}

		if (MLOG_MSG_LENGTH < (msg_stored + 1))
			break;

		MLOG_PRINT("\n");
	}

	*ptr = mlog_msg;
	*size = msg_stored;

#undef MLOG_MSG_LENGTH
#undef MLOG_PRINT
#else
	pr_info("%s: not eng build\n", __func__);
#endif
}

static void mlog_timer_handler(unsigned long data)
{
	mlog(MLOG_TRIGGER_TIMER);

	mod_timer(&mlog_timer, round_jiffies(jiffies + timer_intval));
}

static void mlog_init_logger(void)
{
	spin_lock_init(&mlogbuf_lock);
	mlog_reset_format();
	mlog_reset_buffer();

	setup_timer(&mlog_timer, mlog_timer_handler, 0);
	mlog_timer.expires = jiffies + timer_intval;

	add_timer(&mlog_timer);
}

static void mlog_exit_logger(void)
{

	kfree(strfmt_list);
	strfmt_list = NULL;
}

static int __init mlog_init(void)
{
	mlog_init_logger();
	mlog_init_procfs();
	return 0;
}

static void __exit mlog_exit(void)
{
	mlog_exit_logger();
}

module_param(min_adj, int, 0644);
module_param(max_adj, int, 0644);
module_param(limit_pid, int, 0644);

static int do_filter_handler(const char *val, const struct kernel_param *kp)
{
	const int ret = param_set_uint(val, kp);

	mlog_reset_format();
	mlog_reset_buffer();
	return ret;
}

static const struct kernel_param_ops param_ops_change_filter = {
	.set = &do_filter_handler,
	.get = &param_get_uint,
	.free = NULL,
};

static int do_time_intval_handler(const char *val,
		const struct kernel_param *kp)
{
	const int ret = param_set_uint(val, kp);

	mod_timer(&mlog_timer, jiffies + ret);
	return ret;
}

static const struct kernel_param_ops param_ops_change_time_intval = {
	.set = &do_time_intval_handler,
	.get = &param_get_uint,
	.free = NULL,
};

param_check_uint(meminfo_filter, &meminfo_filter);
module_param_cb(meminfo_filter, &param_ops_change_filter,
		&meminfo_filter, 0644);
__MODULE_PARM_TYPE(meminfo_filter, uint);

param_check_uint(vmstat_filter, &vmstat_filter);
module_param_cb(vmstat_filter, &param_ops_change_filter,
		&vmstat_filter, 0644);
__MODULE_PARM_TYPE(vmstat_filter, uint);

param_check_uint(vmstat_eventall, &vmstat_eventall);
module_param_cb(vmstat_eventall, &param_ops_change_filter,
		&vmstat_eventall, 0644);
__MODULE_PARM_TYPE(vmstat_eventall, uint);

param_check_uint(proc_filter, &proc_filter);
module_param_cb(proc_filter, &param_ops_change_filter, &proc_filter, 0644);
__MODULE_PARM_TYPE(proc_filter, uint);

param_check_ulong(timer_intval, &timer_intval);
module_param_cb(timer_intval, &param_ops_change_time_intval,
		&timer_intval, 0644);
__MODULE_PARM_TYPE(timer_intval, ulong);

static uint do_mlog;

static int do_mlog_handler(const char *val, const struct kernel_param *kp)
{
	const int ret = param_set_uint(val, kp);

	mlog(do_mlog);
	/* MLOG_PRINTK("[mlog] do_mlog %d\n", do_mlog); */
	return ret;
}

static const struct kernel_param_ops param_ops_do_mlog = {
	.set = &do_mlog_handler,
	.get = &param_get_uint,
	.free = NULL,
};

param_check_uint(do_mlog, &do_mlog);
module_param_cb(do_mlog, &param_ops_do_mlog, &do_mlog, 0644);
__MODULE_PARM_TYPE(do_mlog, uint);

module_init(mlog_init);
module_exit(mlog_exit);

/* TODO module license & information */

MODULE_DESCRIPTION("MEDIATEK Memory Log Driver");
MODULE_AUTHOR("Jimmy Su<jimmy.su@mediatek.com>");
MODULE_LICENSE("GPL");
