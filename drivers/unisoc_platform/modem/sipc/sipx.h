/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SIPX_H
#define __SIPX_H

#include <asm/cacheflush.h>
#include <linux/dma-direction.h>
#include <linux/dma-mapping.h>
#include <linux/hrtimer.h>

#include "sblock.h"

#define SIPX_STATE_IDLE		0
#define SIPX_STATE_READY	0x7c7d7e7f

#ifdef CONFIG_UNISOC_SIPC_MEM_CACHE_EN
#define SIPC_DATA_TO_SKB_CACHE_INV(dev, addr, size) \
	dma_sync_single_for_cpu(dev, addr, size, DMA_FROM_DEVICE)
#define SKB_DATA_TO_SIPC_CACHE_FLUSH(dev, addr, size) \
	dma_sync_single_for_device(dev, addr, size, DMA_TO_DEVICE)
#endif

struct sipx_mgr;

struct sipx_init_data {
	const char *name;
	u8 dst;
	u32 dl_pool_size;
	u32 dl_ack_pool_size;
	u32 ul_pool_size;
	u32 ul_ack_pool_size;
};

struct sipx_blk {
	u32	addr; /*phy address*/
	u32	length;
	u16	index;
	u16	offset;
};

struct sipx_blk_item {
	u16	index; /* index in pools */

	/* bit0-bit10: valid length, bit11-bit15 valid offset */
	u16	desc;
} __packed;

struct sipx_fifo_info {
	u32	blks_addr;
	u32	blk_size;
	u32	fifo_size;
	u32	fifo_rdptr;
	u32	fifo_wrptr;
	u32	fifo_addr;
};

struct sipx_pool {
	volatile struct sipx_fifo_info *fifo_info;
	struct sipx_blk_item *fifo_buf;/* virt of info->fifo_addr */
	u32 fifo_size;
	void *blks_buf; /* virt of info->blks_addr */
	u32 blk_size;

	/* lock for sipx-pool */
	spinlock_t lock;

	struct sipx_mgr *sipx;
};

struct sipx_ring {
	volatile struct sipx_fifo_info	*fifo_info;
	/* virt of info->fifo_addr */
	struct sipx_blk_item		*fifo_buf;
	/* virt of info->blks_addr */
	void	*blks_buf;
	u32	fifo_size;
	u32	blk_size;

	/* lock for sipx-ring */
	spinlock_t	lock;
	struct sipx_mgr	*sipx;
};

struct sipx_channel {
	u32	dst;
	u32	channel;
	u32	state;

	void	*smem_virt;
	u32	smem_addr;
	u32	dst_smem_addr;
	u32	smem_size;

	struct sipx_ring	*dl_ring;
	struct sipx_ring	*dl_ack_ring;
	struct sipx_ring	*ul_ring;
	struct sipx_ring	*ul_ack_ring;

	u32		dl_record_flag;
	struct sblock	dl_record_blk;
	u32		ul_record_flag;
	struct sblock	ul_record_blk;

	/* lock for sipx-channel */
	spinlock_t	lock;
	struct hrtimer	ul_timer;
	int		ul_timer_active;
	ktime_t		ul_timer_val;

	struct task_struct	*thread;
	void			(*handler)(int event, void *data);
	void			*data;
	struct sipx_mgr		*sipx;
};

struct sipx_mgr {
	u32	dst;
	u32	state;
	int	recovery;
	void	*smem_virt;
	void	*smem_cached_virt;
	u32	smem_addr;
	u32	dst_smem_addr;
	u32	smem_size;
	void	*dl_ack_start;
	void	*ul_ack_start;

	u32	dl_pool_size;
	u32	dl_ack_pool_size;
	u32	ul_pool_size;
	u32	ul_ack_pool_size;
	u32	blk_size;
	u32	ack_blk_size;

	struct sipx_init_data	*pdata;
	struct sipx_pool	*dl_pool;
	struct sipx_pool	*dl_ack_pool;
	struct sipx_pool	*ul_pool;
	struct sipx_pool	*ul_ack_pool;
	struct sipx_channel	*channels[SMSG_VALID_CH_NR];
};
#endif /* !__SIPX_H */
