/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _NVSHM_PRIV_H
#define _NVSHM_PRIV_H

#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/wakelock.h>
#include <asm/memory.h>
#include <asm/cacheflush.h>
#include <asm/outercache.h>
#include "nvshm_types.h"
/*
 * Test stub is used to implement nvshm on private memory for testing purpose.
 * Data are allocated into this private memory but queues loop on themselves
 */
#define NVSHM_TEST_STUB

/* Generate NVSHM_IPC MSG */
#define NVSHM_IPC_MESSAGE(id) (((~id & 0xFFFF) << 16) | (id & 0xFFFF))

/* Flags for descriptors */
#define NVSHM_DESC_AP    (0x01)  /* AP descriptor ownership */
#define NVSHM_DESC_BB    (0x02)  /* BB descriptor ownership */
#define NVSHM_DESC_OPEN  (0x04)  /* OOB channel open */
#define NVSHM_DESC_CLOSE (0x08)  /* OOB channel close */
#define NVSHM_DESC_XOFF  (0x10)  /* OOB channel Tx off */
#define NVSHM_DESC_XON   (0x20)  /* OOB channel Tx on */

#define FLUSH_CPU_DCACHE(va, size)	\
	do {	\
		unsigned long _pa_ = page_to_phys(vmalloc_to_page((va))) \
			+ ((unsigned long)va & ~PAGE_MASK);		\
		__cpuc_flush_dcache_area((void *)(va), (size_t)(size));	\
		outer_flush_range(_pa_, _pa_+(size_t)(size));		\
	} while (0)

#define INV_CPU_DCACHE(va, size)		\
	do {	\
		unsigned long _pa_ = page_to_phys(vmalloc_to_page((va))) \
			+ ((unsigned long)va & ~PAGE_MASK);              \
		outer_inv_range(_pa_, _pa_+(size_t)(size));		\
		__cpuc_flush_dcache_area((void *)(va), (size_t)(size));	\
	} while (0)

struct nvshm_handle {
	spinlock_t lock;
	spinlock_t qlock;
	struct wake_lock ul_lock;
	struct wake_lock dl_lock;
	int instance;
	int old_status;
	int configured;
	int bb_irq;
	int errno;
	struct nvshm_config *conf;
	void *ipc_base_virt;
	void *mb_base_virt;
	void *desc_base_virt; /* AP desc region */
	void *data_base_virt; /* AP data region */
	void *stats_base_virt;
	unsigned long ipc_size;
	unsigned long mb_size;
	unsigned long desc_size;
	unsigned long data_size;
	unsigned long stats_size;
	struct nvshm_iobuf *shared_queue_head; /* shared desc list */
	struct nvshm_iobuf *shared_queue_tail; /* shared desc list */
	struct nvshm_iobuf *free_pool_head;    /* free desc list */
	struct nvshm_channel chan[NVSHM_MAX_CHANNELS];
	struct work_struct nvshm_work;
	struct workqueue_struct *nvshm_wq;
	struct hrtimer wake_timer;
	int timeout;
	char wq_name[16];
	struct device *dev;
	void *ipc_data;
	void (*generate_ipc)(void *ipc_data);
	struct platform_device *tegra_bb;
};

extern inline struct nvshm_handle *nvshm_get_handle(void);

extern int nvshm_tty_init(struct nvshm_handle *handle);
extern void nvshm_tty_cleanup(void);

extern int nvshm_net_init(struct nvshm_handle *handle);
extern void nvshm_net_cleanup(void);

extern int nvshm_rpc_init(struct nvshm_handle *handle);
extern void nvshm_rpc_cleanup(void);

extern void nvshm_stats_init(struct nvshm_handle *handle);
extern void nvshm_stats_cleanup(void);

extern int nvshm_rpc_dispatcher_init(void);
extern void nvshm_rpc_dispatcher_cleanup(void);

#endif /* _NVSHM_PRIV_H */
