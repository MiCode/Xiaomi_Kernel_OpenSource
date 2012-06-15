/**
 * Header file of MobiCore Driver Kernel Module.
 *
 * @addtogroup MCD_MCDIMPL_KMOD_IMPL
 * @{
 * Internal structures of the McDrvModule
 * @file
 *
 * Header file the MobiCore Driver Kernel Module,
 * its internal structures and defines.
 *
 * <!-- Copyright Giesecke & Devrient GmbH 2009-2012 -->
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _MC_DRV_KMOD_H_
#define _MC_DRV_KMOD_H_

#include <asm/pgtable.h>
#include <linux/semaphore.h>

#include "public/mc_drv_module_api.h"
/** Platform specific settings */
#include "platform.h"

/** ARM Trustzone specific masks and modes
 * Vanilla Linux is unaware of TrustZone extension.
 * I.e. arch/arm/include/asm/ptrace.h does not define monitor mode.
 * Also TZ bits in cpuid is not defined, ARM port uses magic numbers,
 * see arch/arm/kernel/setup.c */
#define ARM_MONITOR_MODE 0b10110
#define ARM_SECURITY_EXTENSION_MASK 0x30

/**
 * Contiguous buffer allocated to TLCs.
 * These buffers are uses as world shared memory (wsm) and shared with
 * secure world.
 * The virtual kernel address is added for a simpler search algorithm.
 */
struct mc_contg_buffer {
	unsigned int	handle; /* unique handle */
	void		*virt_user_addr; /**< virtual User start address */
	void		*virt_kernel_addr; /**< virtual Kernel start address */
	void		*phys_addr; /**< physical start address */
	unsigned int	order; /**< order of number of pages */
};

/** Maximum number of contiguous buffer allocations that one process can get via
 * mmap. */
#define MC_DRV_KMOD_CONTG_BUFFER_MAX	16

/** Instance data for MobiCore Daemon and TLCs. */
struct mc_instance {
	/** unique handle */
	unsigned int	handle;
	/** process that opened this instance */
	pid_t		pid_vnr;
	/** buffer list for mmap generated address space and
		its virtual client address */
	struct mc_contg_buffer	contg_buffers[MC_DRV_KMOD_CONTG_BUFFER_MAX];
};

/** MobiCore specific page tables for world shared memory.
 * Linux uses shadow page tables, see arch/arm/include/asm/pgtable-2level.
 * MobiCore uses the default ARM format.
 *
 * Number of page table entries in one L2 table. This is ARM specific, an
 * L2 table covers 1 MiB by using 256 entry referring to 4KiB pages each.
 */
#define MC_ARM_L2_TABLE_ENTRIES		256

/** ARM level 2 (L2) table with 256 entries. Size: 1k */
struct l2table {
	pte_t	table_entries[MC_ARM_L2_TABLE_ENTRIES];
};

/** Number of pages for L2 tables. There are 4 table in each page. */
#define MC_DRV_KMOD_L2_TABLE_PER_PAGES	4

/** Store for four L2 tables in one 4kb page*/
struct mc_l2_table_store {
	struct l2table table[MC_DRV_KMOD_L2_TABLE_PER_PAGES];
};

/** Usage and maintenance information about mc_l2_table_store */
struct mc_l2_tables_set {
	struct list_head		list;
	unsigned int			usage_bitmap;	/**< usage bitmap */
	struct mc_l2_table_store	*kernel_virt;	/**< kernel virtual address */
	struct mc_l2_table_store	*phys;		/**< physical address */
	struct page			*page;		/**< pointer to page struct */
};

/**
 * L2 table allocated to the Daemon or a TLC describing a world shared buffer.
 * When users map a malloc()ed area into SWd, a L2 table is allocated.
 * In addition, the area of maximum 1MB virtual address space is mapped into
 * the L2 table and a handle for this table is returned to the user.
 */
struct mc_used_l2_table {
	struct list_head	list;

	/** handle as communicated to user mode */
	unsigned int		handle;
	unsigned int		flags;

	/** owner of this L2 table */
	struct mc_instance	*owner;

	/** set describing where our L2 table is stored */
	struct mc_l2_tables_set	*set;

	/** index into L2 table set */
	unsigned int		idx;

	/** size of buffer */
	unsigned int		nr_of_pages;
};

#define MC_WSM_L2_CONTAINER_WSM_LOCKED_BY_APP   (1U << 0)
#define MC_WSM_L2_CONTAINER_WSM_LOCKED_BY_MC    (1U << 1)

/** MobiCore S-SIQ interrupt context data. */
struct mc_ssiq_ctx {
	/** S-SIQ interrupt counter */
	atomic_t	counter;
};

/** MobiCore Daemon context data. */
struct mc_daemon_ctx {
	/** event semaphore */
	struct semaphore	sem;
	struct fasync_struct	*async_queue;
	/** event counter */
	unsigned int		ssiq_counter;
};

/** MobiCore Driver Kernel Module context data. */
struct mc_drv_kmod_ctx {

	/** ever incrementing counter */
	atomic_t		unique_counter;

	/** S-SIQ interrupt context */
	struct mc_ssiq_ctx	ssiq_ctx;

	/** MobiCore Daemon context */
	struct mc_daemon_ctx	daemon_ctx;

	/** pointer to instance of daemon */
	struct mc_instance	*daemon_inst;

	/** Backing store for L2 tables */
	struct list_head	mc_l2_tables_sets;

	/** Bookkeeping for used L2 tables */
	struct list_head	mc_used_l2_tables;

	/** semaphore to synchronize access to above lists */
	struct semaphore	wsm_l2_sem;
};

/** MobiCore internal trace buffer structure. */
struct mc_trace_buf {
	uint32_t version; /**< version of trace buffer */
	uint32_t length; /**< length of allocated buffer(includes header) */
	uint32_t write_pos; /**< last write position */
	char  buff[1]; /**< start of the log buffer */
};

/*** MobiCore internal trace log setup. */
void mobicore_log_read(void);
long mobicore_log_setup(void *);
void mobicore_log_free(void);

#define MCDRV_DBG_ERROR(txt, ...) \
	printk(KERN_ERR "mcDrvKMod [%d] %s() ### ERROR: " txt, \
		task_pid_vnr(current), \
		__func__, \
		##__VA_ARGS__)

/* dummy function helper macro. */
#define DUMMY_FUNCTION()    do {} while (0)

#if defined(DEBUG)

/* #define DEBUG_VERBOSE */
#if defined(DEBUG_VERBOSE)
#define MCDRV_DBG_VERBOSE          MCDRV_DBG
#else
#define MCDRV_DBG_VERBOSE(...)     DUMMY_FUNCTION()
#endif

#define MCDRV_DBG(txt, ...) \
	printk(KERN_INFO "mcDrvKMod [%d on CPU%d] %s(): " txt, \
		task_pid_vnr(current), \
		raw_smp_processor_id(), \
		__func__, \
		##__VA_ARGS__)

#define MCDRV_DBG_WARN(txt, ...) \
	printk(KERN_WARNING "mcDrvKMod [%d] %s() WARNING: " txt, \
		task_pid_vnr(current), \
		__func__, \
		##__VA_ARGS__)

#define MCDRV_ASSERT(cond) \
	do { \
		if (unlikely(!(cond))) { \
			panic("mcDrvKMod Assertion failed: %s:%d\n", \
				__FILE__, __LINE__); \
		} \
	} while (0)

#else

#define MCDRV_DBG_VERBOSE(...)	DUMMY_FUNCTION()
#define MCDRV_DBG(...)		DUMMY_FUNCTION()
#define MCDRV_DBG_WARN(...)	DUMMY_FUNCTION()

#define MCDRV_ASSERT(...)	DUMMY_FUNCTION()

#endif /* [not] defined(DEBUG) */

#endif /* _MC_DRV_KMOD_H_ */
/** @} */
