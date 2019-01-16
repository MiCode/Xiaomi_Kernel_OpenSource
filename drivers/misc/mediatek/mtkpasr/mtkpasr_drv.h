/*
 * Compressed RAM block device
 *
 * Copyright (C) 2008, 2009, 2010  Nitin Gupta
 *
 * This code is released using a dual license strategy: BSD/GPL
 * You can choose the licence that better fits your requirements.
 *
 * Released under the terms of 3-clause BSD License
 * Released under the terms of GNU General Public License Version 2.0
 *
 * Project home: http://compcache.googlecode.com
 */

#ifndef _MTKPASR_DRV_H_
#define _MTKPASR_DRV_H_

#include <linux/version.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/swap.h>
#ifdef CONFIG_MTKPASR_MAFL
#include <linux/shrinker.h>
#endif

#include <mach/mt_spm_sleep.h>

#include "../zsmalloc/zsmalloc.h"

/* MTKPASR enabled? */
#define IS_MTKPASR_ENABLED		\
	do {				\
		if (!mtkpasr_enable)	\
			return 0;	\
	} while (0)

#define IS_MTKPASR_ENABLED_NORV		\
	do {				\
		if (!mtkpasr_enable)	\
			return;		\
	} while (0)

/* MTKPASR Debug Filter */
#define mtkpasr_print(level, x...)			\
	do {						\
		if (mtkpasr_debug_level >= level)	\
			printk(KERN_CRIT x);		\
	} while (0)

#define MTKPASR		"MTKPASR"
#ifdef CONFIG_MTKPASR_DEBUG
#define	mtkpasr_info(string, args...)	mtkpasr_print(3, "[%s]:[%s][%d] "string, MTKPASR, __func__, __LINE__, ##args)
#define mtkpasr_log(string, args...)    mtkpasr_print(2, "[%s]:[%s][%d] "string, MTKPASR, __func__, __LINE__, ##args)
#define	mtkpasr_err(string, args...)	mtkpasr_print(1, "[%s]:[%s][%d] "string, MTKPASR, __func__, __LINE__, ##args)
#else
#define	mtkpasr_info(string, args...)
#define mtkpasr_log(string, args...)    mtkpasr_print(2, "[%s]:[%s][%d] "string, MTKPASR, __func__, __LINE__, ##args)
#define	mtkpasr_err(string, args...)	mtkpasr_print(1, "[%s]:[%s][%d] "string, MTKPASR, __func__, __LINE__, ##args)
#endif

/* This is an experimental value based on ARMv7 single core with 1.3 GHz! (Let the external decompression time be around 0.25s.) */
/* #define MTKPASR_MAX_EXTCOMP	0x1FFF */
/* This is an experimental value based on ARMv7 single core with 1.1 GHz! (Let the external decompression time be around 0.05s.) */
#ifndef CONFIG_64BIT
#define MTKPASR_MAX_EXTCOMP	0x801
#else
#define MTKPASR_MAX_EXTCOMP	0x0
#endif

#define MTKPASR_FLUSH() do {				\
				local_irq_disable();	\
				lru_add_drain();	\
				drain_local_pages(NULL);\
			} while (0)			\

/* For every MTKPASR_CHECK_ABORTED loops, we will do a check on pending wakeup sources. */
#define MTKPASR_CHECK_ABORTED		(SWAP_CLUSTER_MAX >> 1)
/* A batch of to-be-migrated pages */
#define MTKPASR_CHECK_MIGRATE		SWAP_CLUSTER_MAX

#ifdef CONFIG_MTKPASR_MAFL
/* Current Implementation in Linux - see mm/internal.h */
#define PAGE_ORDER(page)	page_private(page)
#endif

/* Inused pages in bank */
#define BANK_INUSED(i)		(mtkpasr_banks[i].inused)
/* Related rank */
#define BANK_RANK(i)		(mtkpasr_banks[i].rank)

/* Check whether there is any pending wakeup - (bool)*/
#define CHECK_PENDING_WAKEUP	spm_check_wakeup_src()	/*pm_wakeup_pending()*/

/* Kernel APIs */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
#define PAGE_EVICTABLE(page, vma)	page_evictable(page, vma)
#define ZS_CREATE_POOL(name, flags)	zs_create_pool(name, flags)
#else
#define PAGE_EVICTABLE(page, vma)	page_evictable(page)
#define ZS_CREATE_POOL(name, flags)	zs_create_pool(flags)
#endif

/* Page Migration API */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
#define MIGRATE_PAGES(from, func, priv)	migrate_pages(from, func, priv, false, MIGRATE_ASYNC)
#else
#define MIGRATE_PAGES(from, func, priv)	migrate_pages(from, func, priv, MIGRATE_ASYNC, MR_COMPACTION)
#endif

/*-- Configurable parameters */

/* Default mtkpasr disk size: 50% of total RAM */
static const unsigned default_disksize_perc_ram = 50;

/*
 * Pages that compress to size greater than this are stored
 * uncompressed in memory.
 */
static const size_t max_cmpr_size = PAGE_SIZE / 4 * 3;

/*-- End of configurable params */

/* Flags for mtkpasr pages (table[page_no].flags) */
enum mtkpasr_pageflags {
	/* Page consists entirely of zeros */
	MTKPASR_ZERO,

	__NR_MTKPASR_PAGEFLAGS,
};

/*-- Data structures */

/* Allocated for each disk page */
struct table {
	unsigned long handle;
	u16 size;		/* object size (excluding header) */
	u8 count;		/* object ref count (not yet used) */
	u8 flags;
	void *obj;
} __aligned(4);

struct mtkpasr_stats {
	u64 compr_size;		/* compressed size of pages stored */
	u64 failed_reads;	/* should NEVER! happen */
	u64 failed_writes;	/* can happen when memory is too low */
	u32 pages_zero;		/* no. of zero filled pages */
	u32 pages_stored;	/* no. of pages currently stored */
	u32 good_compress;	/* % of pages with compression ratio<=50% */
	u32 bad_compress;	/* % of pages with compression ratio>=75% */
};

struct mtkpasr {

	void *compress_workmem;
	void *compress_buffer;

	spinlock_t stat64_lock;	/* protect 64-bit stats */
	int init_done;
	/* Prevent concurrent execution of device init, reset and R/W request */
	struct rw_semaphore init_lock;

	/* Scan range */
	unsigned long index;

	struct mtkpasr_stats stats;
};

/*
 *      (Virtual Ranks & Banks)
 *            _____   _____   _____
 *	Rank |_____| |_____| |_____|   ...
 *           |      \|      \|      \
 *           |_ _ _ _|_ _ _ _|_ _ _ _|
 *	Bank |_|_|_|_|_|_|_|_|_|_|_|_| ...         // An array
 */

#define MTKPASR_SROFF	0xFFFFFFFF
#define MTKPASR_RDPDON	0xFFFFFFFE
#define MTKPASR_SEGMENT_CH0	0x000000FF
#define MTKPASR_SEGMENT_CH1	0x0000FF00
#define MTKPASR_SEGMENT_CH2	0x00FF0000
#define MTKPASR_SEGMENT_CH3	0xFF000000

/* For no-PASR-imposed banks */
struct nopasr_bank {
	unsigned long start_pfn;	/* The 1st pfn */
	unsigned long end_pfn;		/* The pfn after the last valid one */
	u32 inused;			/* The number of inuse pages or stands for SR OFF */
	u32 segment;			/* Corresponding to which segment */
};

/* Bank information (1 PASR unit) */
struct mtkpasr_bank {
	unsigned long start_pfn;	/* The 1st pfn */
	unsigned long end_pfn;		/* The pfn after the last valid one */
	u32 inused;			/* The number of inuse pages or stands for SR OFF */
	u32 valid_pages;		/* Valid pages in this bank */
	void *rank;			/* Associated rank */
	u32 segment;			/* Corresponding to which segment */
#ifdef CONFIG_MTKPASR_MAFL
	struct list_head mafl;		/* Mark it As Free by removing page blocks from buddy allocator to its List */
	int inmafl;			/* Remaining count in mafl(in pages) */
#endif
	union {
		u32 comp_pos;
		struct {
			s16 comp_start;	/* Compress start pos at extcomp */
			s16 comp_end;	/* Compress pos next to tha last one at extcomp */
		};
	};
};

#define MTKPASR_DPDON	0xFFFF		/* All banks belonging to it should be set as MTKPASR_RDPDON! */
/* Rank information (1 DPD unit) */
struct mtkpasr_rank {
	u16 start_bank;			/* The 1st bank */
	u16 end_bank;			/* The last bank */
	u16 hw_rank;			/* Corresponding to which hw rank */
	u16 inused;			/* The number of inuse banks or stands for DPD ON */
};

/* MTKPASR Status */
enum mtkpasr_phase {
	MTKPASR_OFF,			/* PASR is off */
	MTKPASR_ENTERING,		/* Entering PASR. Do data compression on highmem inuse pages. */
	MTKPASR_DISABLINGSR,		/* After entering PASR, disabling SR */
	MTKPASR_RESTORING,		/* If there is any incoming ITR, then to terminate any on-going(Entering, Disabling SR) PASR */
	MTKPASR_EXITING,		/* Exiting PASR. Do data decompression on highmem inuse pages. */
	MTKPASR_ENABLINGSR,		/* Before exiting PASR, enabling SR */
	MTKPASR_ON,			/* PASR is on */
	MTKPASR_DPD_OFF,		/* DPD is off */
	MTKPASR_DPD_ON,			/* DPD is on */
	MTKPASR_PHASE_TOTAL,
	MTKPASR_SUCCESS,
	MTKPASR_FAIL,
	MTKPASR_WRONG_STATE,		/* Wrong state! */
	MTKPASR_GET_WAKEUP,		/* There exists a pending wakeup source during PASR flow! Another ret value is -EBUSY */
	MTKPASR_PHASE_ONE,
	MTKPASR_PHASE_TWO,
};

/* Compacting on banks */
struct mtkpasr_bank_cc {
	int to_bank;
	unsigned long to_cursor;
};

extern struct mtkpasr *mtkpasr_device;
#ifdef CONFIG_SYSFS
extern struct attribute_group mtkpasr_attr_group;
#endif

/* MTKPASR switch */
extern int mtkpasr_enable;
extern unsigned long mtkpasr_enable_sr;

/* Debug filter */
extern int mtkpasr_debug_level;

/*-----------------*/
/* MTKPASR preinit */
/*-----------------*/
/* Helper of constructing Memory (Virtual) Rank & Bank Information */
extern int compute_valid_pasr_range(unsigned long *start_pfn, unsigned long *end_pfn, int *num_ranks);
/* Give bank, this function will return its (start_pfn, end_pfn) and corresponding rank */
extern int __init query_bank_information(int bank, unsigned long *spfn, unsigned long *epfn, bool fully);
/* Translate sw bank to physical dram segment */
extern u32 pasr_bank_to_segment(unsigned long start_pfn, unsigned long end_pfn);

/* Show mem banks */
extern int mtkpasr_show_banks(char *);

#ifdef CONFIG_MTKPASR_MAFL
#define MAX_OPS_INVARIANT	(3)
#define MAX_NO_OPS_INVARIANT	(MAX_OPS_INVARIANT << 2)
#define KEEP_NO_OPS		(0x7FFFFFFF)
extern unsigned long mtkpasr_show_page_reserved(void);
extern bool mtkpasr_no_phaseone_ops(void);
extern bool mtkpasr_no_ops(void);
#endif

extern enum mtkpasr_phase mtkpasr_entering(void);
extern enum mtkpasr_phase mtkpasr_disablingSR(u32 *sr, u32 *dpd);
extern enum mtkpasr_phase mtkpasr_enablingSR(void);
extern enum mtkpasr_phase mtkpasr_exiting(void);
extern void mtkpasr_restoring(void);

extern void mtkpasr_reset_slots(void);
extern int mtkpasr_init_device(struct mtkpasr *mtkpasr);
extern void __mtkpasr_reset_device(struct mtkpasr *mtkpasr);
extern int mtkpasr_forward_rw(struct mtkpasr *mtkpasr, u32 index, struct page *page, int rw);

extern int mtkpasr_acquire_total(void);
extern int mtkpasr_acquire_frees(void);

extern void set_mtkpasr_triggered(void);
extern void clear_mtkpasr_triggered(void);
extern bool is_mtkpasr_triggered(void);

#endif
