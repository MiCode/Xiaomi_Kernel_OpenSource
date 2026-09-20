/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2024-2024 XRing Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _PTE_HOT_PREREAD_H_
#define _PTE_HOT_PREREAD_H_

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/rwsem.h>
#include <linux/xarray.h>
#include <linux/percpu.h>
#include <linux/workqueue.h>
#include <asm/pgtable.h>

#define MPS_PAGE_TYPE_ANON 0x01
#define MPS_PAGE_TYPE_FILE 0x02
#define MPS_ACTION_SNAPSHOT_TYPE(flags) ((flags)&0x03)
#define MPS_ACTION_PREPAGE_TYPE(flags) (((flags) >> 4) & 0x03)

#define MPS_ACTION_SNAPSHOT_ANON 0x01
#define MPS_ACTION_SNAPSHOT_FILE 0x02
#define MPS_ACTION_DEACTIVATE 0x04
#define MPS_ACTION_PREPAGE_ANON 0x10
#define MPS_ACTION_PREPAGE_FILE 0x20
#define MPS_ACTION_MADVISE_RA 0x200
#define MPS_ACTION_DEBUG_HIT 0x1000
#define MPS_ACTION_DEBUG_VERBOSE 0x2000

#define MPS_PAGE_INDEX(vaddr) ((vaddr) / PAGE_SIZE)
#define MPS_SEGMENT_NBIT PTRS_PER_PTE
#define MPS_SEG_INDEX(page_index) ((page_index) / MPS_SEGMENT_NBIT)
#define MPS_SEG_OFFSET(page_index) ((page_index) % MPS_SEGMENT_NBIT)
#define MPS_SEG_TO_PAGE_INDEX(seg_index, seg_off) \
	((seg_index)*MPS_SEGMENT_NBIT + (seg_off))


#define MPS_ANON_BEGIN_ADDR SZ_4G /* skip dalvik vm range */

#define MPS_PTE_SEG_ARR_SIZE (DIV_ROUND_UP(MPS_SEGMENT_NBIT, BITS_PER_LONG))
struct pte_segment {
	unsigned long pagemap[MPS_PTE_SEG_ARR_SIZE];
};

/* all time statistics are in usec. */
#define BYTES_TO_MB(b) ((b) >> 20ULL)
#define MB_TO_BYTES(b) ((b) << 20)
#define MPS_STAT_WRITE_TIME(stat, member) ((stat)->member = ktime_to_us(ktime_get_ns()))
#define MPS_STAT_ASSIGN_TIME(stat, member, dur) ((stat)->member = dur)
#define MPS_STAT_READ_TIME(stat, member) ((stat)->member)
#define MPS_STAT_DUR_TIME(stat, member) (ktime_to_us(ktime_get_ns()) - (stat)->member)
#define MPS_STAT_WRITE_SWAPIN_ACC_TIME(stat, member, dur) ((stat)->member += dur)
extern bool g_rlock_expire_interruput_enable;
extern bool g_swapin_expire_interruput_enable;
extern bool g_pte_mark_file_anon_enable;
extern bool g_pte_fadvise_enable;
extern bool g_pte_anon_swapin_enable;
extern u64 g_rlock_acquire_threshold;
extern u64 g_swapin_total_time_threshold;
extern u32 g_pte_max_active_apps;
extern u32 g_max_pte_segment_count;
#define DEF_RLOCK_EXPIRE_INTERRUPT_ENABLE 1 /* > g_rlock_acquire_threshold then mps_set_interrupt */
#define DEF_SWAPIN_EXPIRE_INTERRUPT_ENABLE 1 /* > g_swapin_total_time_threshold then mps_set_interrupt */
#define DEF_MARK_FILE_ANON_ENABLE 1 /* enable file and anon mark */
#define DEF_PTE_FADVISE_ENABLE 0 /* disable fadvise by default */
#define DEF_PTE_ANON_SWAPIN_ENABLE 1 /* enable anon swapin by default */
#define DEF_RLOCK_ACQUIRE_THRESHOLD (2 * USEC_PER_MSEC) /* rlock acquire time large than this will stop pte read. */
#define DEF_SWAPIN_TOTAL_TIME_THRESHOLD (30 * USEC_PER_MSEC) /* total swapin time large than this will stop pte read. */
#define DEF_PTE_MAX_ACTIVE_APPS 50
#define DEF_MAX_PTE_SEGMENT_COUNT (MB_TO_BYTES(100) / sizeof(struct pte_segment))

struct pte_time_stat {
	s64 last_rlock_acquire_ts; /* record timestamp of acquiring read mmap lock. */
	s64 total_rlock_acquire_time; /* total time spend on acquiring read lock. */
	s64 last_swapin_trigger_ts; /* record timestamp of . */
	s64 total_swapin_trigger_time; /* total time spend on triggering swapin. */
	s64 last_fadvise_trigger_ts; /* record timestamp of triggering fadvise. */
	s64 total_fadvise_trigger_time; /* total time spend on acquire read lock. */
	s64 last_preread_start_ts; /* record timestamp of preread. */
	s64 total_preread_time; /* total time spend on pre read. */
};

enum {
	PTE_ANON_PAGE = 0,
	PTE_NON_PAGE = PTE_ANON_PAGE,
	PTE_FILE_PAGE,
	PTE_PAGE_COUNT,
};
/**
 * store in mm->android_kabi_reserved_1
 * free when trace_android_vh_exit_mm
 */
struct pte_record {
	uint32_t pid;
	uint32_t uid;
	struct mm_struct *rec_mm;
	struct xarray rec_segs[PTE_PAGE_COUNT];
	struct rcu_head rec_rcu;
	struct mutex rec_lock;
	struct pte_segment *last_seg;
	ulong last_seg_index;
	int last_seg_class;
	atomic_t rec_users;
	struct lb_hot_mps *rec_mps;
	struct list_head rec_node;

	uint64_t rec_jiffies;
	uint32_t nr_segments;
	uint32_t nr_anon;
	uint32_t nr_file;
	uint32_t nr_active;
	uint32_t nr_refer_young;
	uint32_t nr_referenced;
	uint32_t nr_young;
	uint32_t nr_pre_anon;
	uint32_t nr_pre_file;
	uint32_t nr_new_seg;
	uint32_t nr_new_anon;
	uint32_t nr_new_file;
	uint32_t nr_hit_anon;
	uint32_t nr_hit_file;

	atomic_t interrupt; /* preread will be interruptted in any stage. */
	struct pte_time_stat stat;
};

static inline void mps_set_interrupt(struct pte_record *mps_rec)
{
	atomic_set(&mps_rec->interrupt, 1);
}

static inline int mps_get_interrupt(struct pte_record *mps_rec)
{
	return atomic_read(&mps_rec->interrupt);
}

#define PACKAGE_NAME_MAX 256 /* reference 'struct iorap_info' in com_miui_server_iorap_IorapServiceNative.cpp*/
enum {
	MPS_INTERRUPT_BY_IOCTL,
	MPS_INTERRUPT_BY_RLOCK,
	MPS_INTERRUPT_BY_SWAPIN,
	MPS_INTERRUPT_COUNT,
};

struct lb_hot_stat {
	s64 max_swapin_time;
	char longest_app[PACKAGE_NAME_MAX];
	s64 min_swapin_time;
	char shortest_app[PACKAGE_NAME_MAX];
	atomic64_t total_rlock_time;
	atomic64_t total_swapin_time;
	atomic64_t total_fadvise_time;
	atomic64_t total_preread_time;
	atomic64_t total_swapin_count;
	atomic64_t interrupt_count[MPS_INTERRUPT_COUNT]; /* preread will be interruptted in any stage. */
};

struct lb_hot_mps {
	spinlock_t list_lock;
	uint32_t nr_records;
	uint32_t max_records;
	uint32_t mps_flags;

	struct kmem_cache *seg_cachep;
	struct list_head rec_list;
	struct xarray rec_tree;

	/* memory statistic. */
	atomic_t nr_alloc_segs;
	atomic_t nr_alloc_mps_recs;

	uint32_t nr_snapshot;
	uint32_t nr_prepage;
	uint32_t nr_start_fra;
	uint32_t nr_error;
	uint64_t total_anon;
	uint64_t total_pre_anon;
	uint64_t total_file;
	uint64_t total_pre_file;
	char file_path[128];

	struct lb_hot_stat stat;
};

extern struct lb_hot_mps g_hot_mps;

int lb_warm_proc_task(struct lb_hot_mps *mps, uint32_t pid, uint32_t action_flags,
		      uint32_t nr_anon, uint32_t nr_file);
int lb_warm_show_stat(struct lb_hot_mps *mps, char *buf, size_t count);

int lb_hot_init(void);
void lb_hot_exit(void);

int mi_lb_dealwith_pte_snapshot_cmd(struct lb_owner *owner);
int mi_lb_dealwith_pte_preread_cmd(struct lb_owner *owner);
int mi_lb_dealwith_pte_stop_cmd(struct lb_owner *owner);
int mi_lb_dealwith_pte_interrupt_cmd(struct lb_owner *owner);
int mi_lb_dealwith_pte_clean_cmd(struct lb_owner *owner);

#endif
