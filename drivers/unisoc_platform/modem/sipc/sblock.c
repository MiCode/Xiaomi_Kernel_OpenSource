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

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-sblock: " fmt
#define R_RD_START_ADDR 0

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/log2.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/seq_file.h>
#include <linux/sipc.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <uapi/linux/sched/types.h>

#include "sblock.h"
#include "sipc_priv.h"

#if defined(CONFIG_DEBUG_FS)
struct sblock_device {
	int			major;
	int			minor;
	struct cdev		cdev;
	struct device		*sys_dev;	/* Device object in sysfs */
};

static struct class *sblock_class;
static struct sblock_device *sblock_dev;
#endif

static struct sblock_mgr *sblocks[SIPC_ID_NR][SMSG_VALID_CH_NR];

struct sb_prepare_info sb_prepare_list_info[LOG_ID_NR];
EXPORT_SYMBOL_GPL(sb_prepare_list_info);

u8 slog_dst2index[SIPC_ID_NR];
EXPORT_SYMBOL_GPL(slog_dst2index);

void (*sblock_clean_slog_sendlist_callback)(void) = NULL;

/* put one block pack to the pool */
void sblock_put(u8 dst, u8 channel, struct sblock *blk)
{
	struct sblock_mgr *sblock;
	struct sblock_ring *ring;
	unsigned long flags;
	int txpos;
	int index;
	u8 ch_index;
	struct sblock_ring_header_op *poolhd_op;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return;
	}

	sblock = sblocks[dst][ch_index];
	if (!sblock)
		return;

	ring = sblock->ring;
	poolhd_op = &(ring->header_op.poolhd_op);

	spin_lock_irqsave(&ring->p_txlock, flags);
	txpos = sblock_get_ringpos(*(poolhd_op->tx_rd_p) - 1,
				   poolhd_op->tx_count);
	ring->p_txblks[txpos].addr = blk->addr -
				     sblock->smem_virt +
				     sblock->stored_smem_addr;
	ring->p_txblks[txpos].length = poolhd_op->tx_size;
	*(poolhd_op->tx_rd_p) = *(poolhd_op->tx_rd_p) - 1;
	if ((int)(*(poolhd_op->tx_wt_p) - *(poolhd_op->tx_rd_p)) == 1)
		wake_up_interruptible_all(&(ring->getwait));
	index = sblock_get_index((blk->addr - ring->txblk_virt),
				 sblock->txblksz);
	ring->txrecord[index] = SBLOCK_BLK_STATE_DONE;

	spin_unlock_irqrestore(&ring->p_txlock, flags);

	/* set write mask. */
	spin_lock_irqsave(&ring->poll_lock, flags);
	ring->poll_mask |= POLLOUT | POLLWRNORM;
	spin_unlock_irqrestore(&ring->poll_lock, flags);

	/* request in sblock_get, release here */
	sipc_smem_release_resource(ring->tx_pms, dst);
}
EXPORT_SYMBOL_GPL(sblock_put);

void sblock_register_slog_clean_sendlist(void (*callback)(void))
{
	sblock_clean_slog_sendlist_callback = callback;
}
EXPORT_SYMBOL_GPL(sblock_register_slog_clean_sendlist);

void sblock_unregister_slog_clean_sendlist(void)
{
	sblock_clean_slog_sendlist_callback = NULL;
}
EXPORT_SYMBOL_GPL(sblock_unregister_slog_clean_sendlist);

/* clean rings and recover pools */
static int sblock_recover(u8 dst, u8 channel)
{
	struct sblock_mgr *sblock;
	struct sblock_ring *ring = NULL;
	struct sblock_ring_header_op *poolhd_op;
	struct sblock_ring_header_op *ringhd_op;
	unsigned long pflags, qflags;
	int i, j, rval;
	u8 ch_index;
	u8 dst_index;

	pr_info("sblock_recover:channel %d", channel);
	if (channel == SMSG_CH_PLOG) {
		dst_index = slog_dst2index[dst];
		if (dst_index == INVALID_SLOG_DST_INDEX) {
			pr_err("slog dst %d invalid!\n", dst);
			return -EINVAL;
		}
	}
	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return -EINVAL;
	}

	sblock = sblocks[dst][ch_index];
	if (!sblock)
		return -ENODEV;

	ring = sblock->ring;
	poolhd_op = &(ring->header_op.poolhd_op);
	ringhd_op = &(ring->header_op.ringhd_op);

	sblock->state = SBLOCK_STATE_IDLE;
	wake_up_interruptible_all(&ring->getwait);
	wake_up_interruptible_all(&ring->recvwait);

	/* must request resource before read or write share memory */
	rval = sipc_smem_request_resource(ring->rx_pms, dst, -1);
	if (rval < 0)
		return rval;

	spin_lock_irqsave(&ring->r_txlock, pflags);
	/* clean txblks ring */
	*(ringhd_op->tx_wt_p) = *(ringhd_op->tx_rd_p);

	spin_lock_irqsave(&ring->p_txlock, qflags);
	/* recover txblks pool */
	*(poolhd_op->tx_rd_p) = *(poolhd_op->tx_wt_p);
	for (i = 0, j = 0; i < poolhd_op->tx_count; i++) {
		if (ring->txrecord[i] == SBLOCK_BLK_STATE_DONE) {
			ring->p_txblks[j].addr = i * sblock->txblksz +
						 poolhd_op->tx_addr;
			ring->p_txblks[j].length = sblock->txblksz;
			*(poolhd_op->tx_wt_p) = *(poolhd_op->tx_wt_p) + 1;
			j++;
		}
	}
	spin_unlock_irqrestore(&ring->p_txlock, qflags);
	spin_unlock_irqrestore(&ring->r_txlock, pflags);

	if (channel == SMSG_CH_PLOG && sblock_clean_slog_sendlist_callback)
		sblock_clean_slog_sendlist_callback();
	spin_lock_irqsave(&ring->r_rxlock, pflags);
	/* clean rxblks ring */
	if (channel != SMSG_CH_PLOG)
		*(ringhd_op->rx_rd_p) = *(ringhd_op->rx_wt_p);

	spin_lock_irqsave(&ring->p_rxlock, qflags);
	/* recover rxblks pool */
	if (channel == SMSG_CH_PLOG) {
		sb_prepare_list_info[dst_index].first_recv_flag = 1;
		sb_prepare_list_info[dst_index].first_release_flag = 1;
		sb_prepare_list_info[dst_index].receive_not_continue_cnt = 0;
		sb_prepare_list_info[dst_index].release_not_continue_cnt = 0;
		for (i = 0, j = 0; i < poolhd_op->rx_count; i++) {
			ring->p_rxblks[j].addr = i * sblock->rxblksz +
						 poolhd_op->rx_addr;
			ring->p_rxblks[j].length = sblock->rxblksz;
			ring->rxrecord[i] = SBLOCK_BLK_STATE_DONE;
			j++;
		}
#ifndef CONFIG_UNISOC_SIPC_SLOG_BRIDGE_5G
		*(ringhd_op->rx_rd_p) = 0;
		*(ringhd_op->rx_wt_p) = 0;
		*(poolhd_op->rx_rd_p) = 0;
#endif
		*(poolhd_op->rx_wt_p) = poolhd_op->rx_count;
	} else {
		/* channel 2,3 recover rxblks pool */
		*(poolhd_op->rx_wt_p) = *(poolhd_op->rx_rd_p);
		for (i = 0, j = 0; i < poolhd_op->rx_count; i++) {
			if (ring->rxrecord[i] == SBLOCK_BLK_STATE_DONE) {
				ring->p_rxblks[j].addr = i * sblock->rxblksz +
							 poolhd_op->rx_addr;
				ring->p_rxblks[j].length = sblock->rxblksz;
				*(poolhd_op->rx_wt_p) = *(poolhd_op->rx_wt_p) + 1;
				j++;
			}
		}
	}
	spin_unlock_irqrestore(&ring->p_rxlock, qflags);
	spin_unlock_irqrestore(&ring->r_rxlock, pflags);

	/* restore write mask. */
	spin_lock_irqsave(&ring->poll_lock, qflags);
	ring->poll_mask |= POLLOUT | POLLWRNORM;
	spin_unlock_irqrestore(&ring->poll_lock, qflags);

	/* release resource */
	sipc_smem_release_resource(ring->rx_pms, dst);

	return 0;
}

static int sblock_host_init(struct smsg_ipc *sipc, struct sblock_mgr *sblock,
		     u32 txblocknum, u32 txblocksize,
		     u32 rxblocknum, u32 rxblocksize)
{
	volatile struct sblock_ring_header *ringhd;
	volatile struct sblock_ring_header *poolhd;
	struct sblock_ring_header_op *ringhd_op;
	struct sblock_ring_header_op *poolhd_op;

	u32 hsize;
	int i, rval = -ENOMEM;
	phys_addr_t offset = 0;
	u8 dst = sblock->dst;
	u16 smem = (u16)sblock->smem;

	txblocksize = ALIGN(txblocksize, SBLOCK_ALIGN_BYTES);
	rxblocksize = ALIGN(rxblocksize, SBLOCK_ALIGN_BYTES);
	sblock->txblksz = txblocksize;
	sblock->rxblksz = rxblocksize;
	sblock->txblknum = txblocknum;
	sblock->rxblknum = rxblocknum;

	pr_debug("channel %d-%d, txblocksize=%#x, rxblocksize=%#x!\n",
		 sblock->dst,
		 sblock->channel,
		 txblocksize,
		 rxblocksize);

	pr_debug("channel %d-%d, txblocknum=%#x, rxblocknum=%#x!\n",
		 sblock->dst,
		 sblock->channel,
		 txblocknum,
		 rxblocknum);

	/* allocate smem */
	hsize = sizeof(struct sblock_header);
		/* for header*/
	sblock->smem_size = hsize +
		/* for blks */
		txblocknum * txblocksize + rxblocknum * rxblocksize +
		/* for ring */
		(txblocknum + rxblocknum) * sizeof(struct sblock_blks) +
		/* for pool */
		(txblocknum + rxblocknum) * sizeof(struct sblock_blks);

	sblock->smem_addr = smem_alloc_ex(dst, smem, sblock->smem_size);
	if (!sblock->smem_addr)
		return -ENOMEM;

	pr_debug("channel %d-%d, smem_addr=%#llx, smem_size=%#x!\n",
		 sblock->dst,
		 sblock->channel,
		 sblock->smem_addr + offset,
		 sblock->smem_size);

	sblock->dst_smem_addr = sblock->smem_addr -
		sipc->smem_ptr[smem].smem_base + sipc->smem_ptr[smem].dst_smem_base;

	/* in host mode,  it is client physial address. */
	sblock->stored_smem_addr = sblock->dst_smem_addr;

#ifdef CONFIG_PHYS_ADDR_T_64BIT
	offset = sipc->high_offset;
	offset = offset << 32;
#endif
	pr_debug("channel %d-%d, offset = 0x%llx!\n",
		 sblock->dst,
		 sblock->channel,
		 offset);
	sblock->smem_virt = shmem_ram_vmap_nocache(dst,
						   sblock->smem_addr + offset,
						   sblock->smem_size);
	if (!sblock->smem_virt)
		goto sblock_host_smem_free;

	/* must request resource before read or write share memory */
	rval = sipc_smem_request_resource(sipc->sipc_pms, sipc->dst, -1);
	if (rval < 0)
		goto sblock_host_rx_free;

	/* initialize header */
	ringhd = (volatile struct sblock_ring_header *)(sblock->smem_virt);
	ringhd->txblk_addr = sblock->stored_smem_addr + hsize;
	ringhd->txblk_count = txblocknum;
	ringhd->txblk_size = txblocksize;
	ringhd->txblk_rdptr = 0;
	ringhd->txblk_wrptr = 0;
	ringhd->txblk_blks = ringhd->txblk_addr +
		txblocknum * txblocksize + rxblocknum * rxblocksize;
	ringhd->rxblk_addr = ringhd->txblk_addr + txblocknum * txblocksize;
	ringhd->rxblk_count = rxblocknum;
	ringhd->rxblk_size = rxblocksize;
	ringhd->rxblk_rdptr = 0;
	ringhd->rxblk_wrptr = 0;
	ringhd->rxblk_blks = ringhd->txblk_blks +
		txblocknum * sizeof(struct sblock_blks);

	poolhd = ringhd + 1;
	poolhd->txblk_addr = sblock->stored_smem_addr + hsize;
	poolhd->txblk_count = txblocknum;
	poolhd->txblk_size = txblocksize;
	poolhd->txblk_rdptr = 0;
	poolhd->txblk_wrptr = 0;
	poolhd->txblk_blks = ringhd->rxblk_blks +
		rxblocknum * sizeof(struct sblock_blks);
	poolhd->rxblk_addr = ringhd->txblk_addr + txblocknum * txblocksize;
	poolhd->rxblk_count = rxblocknum;
	poolhd->rxblk_size = rxblocksize;
	poolhd->rxblk_rdptr = 0;
	poolhd->rxblk_wrptr = 0;
	poolhd->rxblk_blks = poolhd->txblk_blks +
		txblocknum * sizeof(struct sblock_blks);

	pr_debug("channel %d-%d, int ring!\n",
		 sblock->dst,
		 sblock->channel);

	/* initialize ring */
	sblock->ring->txrecord = kcalloc(txblocknum, sizeof(int), GFP_KERNEL);
	if (!sblock->ring->txrecord)
		goto sblock_host_unmap;

	sblock->ring->rxrecord = kcalloc(rxblocknum, sizeof(int), GFP_KERNEL);
	if (!sblock->ring->rxrecord)
		goto sblock_host_tx_free;

	sblock->ring->header = sblock->smem_virt;
	sblock->ring->txblk_virt = sblock->smem_virt +
		(ringhd->txblk_addr - sblock->stored_smem_addr);
	sblock->ring->r_txblks = sblock->smem_virt +
		(ringhd->txblk_blks - sblock->stored_smem_addr);
	sblock->ring->rxblk_virt = sblock->smem_virt +
		(ringhd->rxblk_addr - sblock->stored_smem_addr);
	sblock->ring->r_rxblks = sblock->smem_virt +
		(ringhd->rxblk_blks - sblock->stored_smem_addr);
	sblock->ring->p_txblks = sblock->smem_virt +
		(poolhd->txblk_blks - sblock->stored_smem_addr);
	sblock->ring->p_rxblks = sblock->smem_virt +
		(poolhd->rxblk_blks - sblock->stored_smem_addr);

	for (i = 0; i < txblocknum; i++) {
		sblock->ring->r_txblks[i].addr = ringhd->txblk_addr +
						 i * txblocksize;
		sblock->ring->r_txblks[i].length = 0;
	}
	for (i = 0; i < rxblocknum; i++) {
		sblock->ring->r_rxblks[i].addr = ringhd->rxblk_addr +
						 i * rxblocksize;
		sblock->ring->r_rxblks[i].length = 0;
	}
	for (i = 0; i < txblocknum; i++) {
		sblock->ring->p_txblks[i].addr = poolhd->txblk_addr +
						 i * txblocksize;
		sblock->ring->p_txblks[i].length = txblocksize;
		sblock->ring->txrecord[i] = SBLOCK_BLK_STATE_DONE;
		poolhd->txblk_wrptr++;
	}
	for (i = 0; i < rxblocknum; i++) {
		sblock->ring->p_rxblks[i].addr = poolhd->rxblk_addr +
						 i * rxblocksize;
		sblock->ring->p_rxblks[i].length = rxblocksize;
		sblock->ring->rxrecord[i] = SBLOCK_BLK_STATE_DONE;
		poolhd->rxblk_wrptr++;
	}

	/* init, set write mask. */
	sblock->ring->poll_mask = POLLOUT | POLLWRNORM;

	/* init header op */
	ringhd_op = &((sblock->ring->header_op).ringhd_op);
	ringhd_op->tx_rd_p  = &ringhd->txblk_rdptr;
	ringhd_op->tx_wt_p  = &ringhd->txblk_wrptr;
	ringhd_op->rx_rd_p  = &ringhd->rxblk_rdptr;
	ringhd_op->rx_wt_p  = &ringhd->rxblk_wrptr;
	ringhd_op->tx_addr  = ringhd->txblk_addr;
	ringhd_op->tx_count = ringhd->txblk_count;
	ringhd_op->tx_size  = ringhd->txblk_size;
	ringhd_op->tx_blks  = ringhd->txblk_blks;
	ringhd_op->rx_addr  = ringhd->rxblk_addr;
	ringhd_op->rx_count = ringhd->rxblk_count;
	ringhd_op->rx_size  = ringhd->rxblk_size;
	ringhd_op->rx_blks  = ringhd->rxblk_blks;
	poolhd_op = &((sblock->ring->header_op).poolhd_op);
	poolhd_op->tx_rd_p  = &poolhd->txblk_rdptr;
	poolhd_op->tx_wt_p  = &poolhd->txblk_wrptr;
	poolhd_op->rx_rd_p  = &poolhd->rxblk_rdptr;
	poolhd_op->rx_wt_p  = &poolhd->rxblk_wrptr;
	poolhd_op->tx_addr  = poolhd->txblk_addr;
	poolhd_op->tx_count = poolhd->txblk_count;
	poolhd_op->tx_size  = poolhd->txblk_size;
	poolhd_op->tx_blks  = poolhd->txblk_blks;
	poolhd_op->rx_addr  = poolhd->rxblk_addr;
	poolhd_op->rx_count = poolhd->rxblk_count;
	poolhd_op->rx_size  = poolhd->rxblk_size;
	poolhd_op->rx_blks  = poolhd->rxblk_blks;

	/* release resource */
	sipc_smem_release_resource(sipc->sipc_pms, sipc->dst);

	return 0;

sblock_host_rx_free:
	kfree(sblock->ring->rxrecord);
sblock_host_tx_free:
	kfree(sblock->ring->txrecord);
sblock_host_unmap:
	shmem_ram_unmap(dst, sblock->smem_virt);
sblock_host_smem_free:
	smem_free(dst, sblock->smem_addr, sblock->smem_size);

	pr_err("channel %d-%d, failed, ENOMEM!\n",
	       sblock->dst,
	       sblock->channel);

	return -ENOMEM;
}

static int sblock_client_init(struct smsg_ipc *sipc, struct sblock_mgr *sblock)
{
	volatile struct sblock_ring_header *ringhd;
	volatile struct sblock_ring_header *poolhd;
	struct sblock_ring_header_op *ringhd_op;
	struct sblock_ring_header_op *poolhd_op;
	u32 hsize;
	u8 dst = sblock->dst;
	phys_addr_t offset = 0;
	u32 txblocknum, txblocksize, rxblocknum, rxblocksize;
	int rval = -ENOMEM;

#ifdef CONFIG_PHYS_ADDR_T_64BIT
	offset = sipc->high_offset;
	offset = offset << 32;
	pr_debug("channel %d-%d, offset = 0x%llx!\n",
		 sblock->dst,
		 sblock->channel,
		 offset);
#endif

	/* get blcok num and block size */
	hsize = sizeof(struct sblock_header);
	sblock->smem_virt = shmem_ram_vmap_nocache(dst,
						   sblock->smem_addr + offset,
						   hsize);
	if (!sblock->smem_virt)
		return -ENOMEM;
	ringhd = (volatile struct sblock_ring_header *)(sblock->smem_virt);
	/* client mode, tx <==> rx */
	txblocknum = ringhd->rxblk_count;
	txblocksize = ringhd->rxblk_size;
	rxblocknum = ringhd->txblk_count;
	rxblocksize = ringhd->txblk_size;

	sblock->txblksz = txblocksize;
	sblock->rxblksz = rxblocksize;
	sblock->txblknum = txblocknum;
	sblock->rxblknum = rxblocknum;
	shmem_ram_unmap(dst, sblock->smem_virt);

	pr_debug("channel %d-%d, txblocksize=%#x, rxblocksize=%#x!\n",
		 sblock->dst,
		 sblock->channel,
		 txblocksize,
		 rxblocksize);

	pr_debug("channel %d-%d, txblocknum=%#x, rxblocknum=%#x!\n",
		 sblock->dst,
		 sblock->channel,
		 txblocknum,
		 rxblocknum);

	/* allocate smem */
		/* for header*/
	sblock->smem_size = hsize +
		/* for blks */
		txblocknum * txblocksize + rxblocknum * rxblocksize +
		/* for ring */
		(txblocknum + rxblocknum) * sizeof(struct sblock_blks) +
		/* for pool */
		(txblocknum + rxblocknum) * sizeof(struct sblock_blks);

	sblock->smem_addr_debug = smem_alloc(dst, sblock->smem_size);
	if (!sblock->smem_addr_debug)
		return -ENOMEM;

	pr_debug("channel %d-%d, smem_addr=%#llx, smem_size=%#x!\n",
		 sblock->dst,
		 sblock->channel,
		 sblock->smem_addr + offset,
		 sblock->smem_size);

	/* in client mode,  it is own physial address. */
	sblock->stored_smem_addr = sblock->smem_addr;

	/* get smem virtual address */
	sblock->smem_virt = shmem_ram_vmap_nocache(dst,
						   sblock->smem_addr + offset,
						   sblock->smem_size);
	if (!sblock->smem_virt)
		goto sblock_client_smem_free;

	/* initialize ring and header */
	sblock->ring->txrecord = kcalloc(txblocknum, sizeof(int), GFP_KERNEL);
	if (!sblock->ring->txrecord)
		goto sblock_client_unmap;

	sblock->ring->rxrecord = kcalloc(rxblocknum, sizeof(int), GFP_KERNEL);
	if (!sblock->ring->rxrecord)
		goto sblock_client_tx_free;

	ringhd = (volatile struct sblock_ring_header *)(sblock->smem_virt);
	poolhd = ringhd + 1;

	/* must request resource before read or write share memory */
	rval = sipc_smem_request_resource(sipc->sipc_pms, sipc->dst, -1);
	if (rval < 0)
		goto sblock_client_tx_free;

	/* client mode, tx <==> rx */
	sblock->ring->header = sblock->smem_virt;
	sblock->ring->txblk_virt = sblock->smem_virt +
		(ringhd->rxblk_addr - sblock->stored_smem_addr);
	sblock->ring->rxblk_virt = sblock->smem_virt +
		(ringhd->txblk_addr - sblock->stored_smem_addr);
	sblock->ring->r_txblks = sblock->smem_virt +
		(ringhd->rxblk_blks - sblock->stored_smem_addr);
	sblock->ring->r_rxblks = sblock->smem_virt +
		(ringhd->txblk_blks - sblock->stored_smem_addr);
	sblock->ring->p_txblks = sblock->smem_virt +
		(poolhd->rxblk_blks - sblock->stored_smem_addr);
	sblock->ring->p_rxblks = sblock->smem_virt +
		(poolhd->txblk_blks - sblock->stored_smem_addr);

	/* init header op,  tx <==> rx */
	ringhd_op = &((sblock->ring->header_op).ringhd_op);
	ringhd_op->tx_rd_p  = &ringhd->rxblk_rdptr;
	ringhd_op->tx_wt_p  = &ringhd->rxblk_wrptr;
	ringhd_op->rx_rd_p  = &ringhd->txblk_rdptr;
	ringhd_op->rx_wt_p  = &ringhd->txblk_wrptr;
	ringhd_op->tx_addr  = ringhd->rxblk_addr;
	ringhd_op->tx_count = ringhd->rxblk_count;
	ringhd_op->tx_size  = ringhd->rxblk_size;
	ringhd_op->tx_blks  = ringhd->rxblk_blks;
	ringhd_op->rx_addr  = ringhd->txblk_addr;
	ringhd_op->rx_count = ringhd->txblk_count;
	ringhd_op->rx_size  = ringhd->txblk_size;
	ringhd_op->rx_blks  = ringhd->txblk_blks;
	poolhd_op = &((sblock->ring->header_op).poolhd_op);
	poolhd_op->tx_rd_p  = &poolhd->rxblk_rdptr;
	poolhd_op->tx_wt_p  = &poolhd->rxblk_wrptr;
	poolhd_op->rx_rd_p  = &poolhd->txblk_rdptr;
	poolhd_op->rx_wt_p  = &poolhd->txblk_wrptr;
	poolhd_op->tx_addr  = poolhd->rxblk_addr;
	poolhd_op->tx_count = poolhd->rxblk_count;
	poolhd_op->tx_size  = poolhd->rxblk_size;
	poolhd_op->tx_blks  = poolhd->rxblk_blks;
	poolhd_op->rx_addr  = poolhd->txblk_addr;
	poolhd_op->rx_count = poolhd->txblk_count;
	poolhd_op->rx_size  = poolhd->txblk_size;
	poolhd_op->rx_blks  = poolhd->txblk_blks;

	/* release resource */
	sipc_smem_release_resource(sipc->sipc_pms, sipc->dst);

	return 0;

sblock_client_tx_free:
	kfree(sblock->ring->txrecord);
sblock_client_unmap:
	shmem_ram_unmap(dst, sblock->smem_virt);
sblock_client_smem_free:
	smem_free(dst, sblock->smem_addr_debug, sblock->smem_size);

	return -ENOMEM;
}

static int sblock_thread(void *data)
{
	struct sblock_mgr *sblock = data;
	struct smsg mcmd, mrecv;
	unsigned long flags;
	int rval;
	int recovery = 0;
	struct smsg_ipc *sipc;
	struct sblock_ring *ring;

	/* since the channel open may hang, we call it in the sblock thread */
	rval = smsg_ch_open(sblock->dst, sblock->channel, -1);
	if (rval != 0) {
		pr_err("Failed to open channel %d\n",
		       sblock->channel);

		if (sblock->pre_cfg && sblock->handler) {
			sblock->handler(SBLOCK_NOTIFY_OPEN_FAILED,
					sblock->data);
		}

		return rval;
	}

	if (sblock->pre_cfg) {
		sblock->state = SBLOCK_STATE_READY;
		recovery = 1;
		if (sblock->handler)
			sblock->handler(SBLOCK_NOTIFY_OPEN, sblock->data);
	}

	/* if client, send SMSG_CMD_SBLOCK_INIT, wait SMSG_DONE_SBLOCK_INIT */
	sipc = smsg_ipcs[sblock->dst];
	if (sipc->client) {
		smsg_set(&mcmd, sblock->channel, SMSG_TYPE_CMD,
			 SMSG_CMD_SBLOCK_INIT, 0);
		smsg_send(sblock->dst, &mcmd, -1);
		do {
			smsg_set(&mrecv, sblock->channel, 0, 0, 0);
			rval = smsg_recv(sblock->dst, &mrecv, -1);
			if (rval != 0) {
				sblock->thread = NULL;
				return rval;
			}
		} while (mrecv.type != SMSG_TYPE_DONE ||
			 mrecv.flag != SMSG_DONE_SBLOCK_INIT);
		sblock->smem_addr = mrecv.value;
		pr_info("channel %d-%d, done_sblock_init, address=0x%x!\n",
			sblock->dst,
			sblock->channel,
			sblock->smem_addr);
		if (sblock_client_init(sipc, sblock)) {
			sblock->thread = NULL;
			return 0;
		}
		sblock->state = SBLOCK_STATE_READY;
		if (sblock->handler)
			sblock->handler(SBLOCK_NOTIFY_OPEN,
					sblock->data);
	}

	/* handle the sblock events */
	while (!kthread_should_stop()) {
		/* monitor sblock recv smsg */
		smsg_set(&mrecv, sblock->channel, 0, 0, 0);
		rval = smsg_recv(sblock->dst, &mrecv, -1);
		if (rval == -EIO || rval == -ENODEV) {
			/* channel state is FREE */
			msleep(20);
			continue;
		}

		pr_debug("sblock thread recv msg: dst=%d, channel=%d, type=%d, flag=0x%04x, value=0x%08x\n",
			 sblock->dst, sblock->channel,
			 mrecv.type, mrecv.flag, mrecv.value);

		switch (mrecv.type) {
		case SMSG_TYPE_OPEN:
			pr_info("sblock thread recv msg: dst=%d, channel=%d, type=%d, flag=0x%04x, value=0x%08x\n",
				sblock->dst, sblock->channel,
				mrecv.type, mrecv.flag, mrecv.value);
			/* handle channel recovery */
			if (recovery) {
				if (sblock->handler)
					sblock->handler(SBLOCK_NOTIFY_CLOSE,
							sblock->data);
				sblock_recover(sblock->dst, sblock->channel);
			}
			smsg_open_ack(sblock->dst, sblock->channel);
			if (sblock->pre_cfg)
				sblock->state = SBLOCK_STATE_READY;
			break;
		case SMSG_TYPE_CLOSE:
			pr_info("sblock thread recv msg: dst=%d, channel=%d, type=%d, flag=0x%04x, value=0x%08x\n",
				sblock->dst, sblock->channel,
				mrecv.type, mrecv.flag, mrecv.value);
			/* handle channel recovery */
			smsg_close_ack(sblock->dst, sblock->channel);
			if (sblock->handler)
				sblock->handler(SBLOCK_NOTIFY_CLOSE,
						sblock->data);
			sblock->state = SBLOCK_STATE_IDLE;
			break;
		case SMSG_TYPE_CMD:
			pr_info("sblock thread recv msg: dst=%d, channel=%d, type=%d, flag=0x%04x, value=0x%08x\n",
				sblock->dst, sblock->channel,
				mrecv.type, mrecv.flag, mrecv.value);
			if (!sblock->pre_cfg) {
				/* respond cmd done for sblock init */
				WARN_ON(mrecv.flag != SMSG_CMD_SBLOCK_INIT);
				smsg_set(&mcmd,
					 sblock->channel,
					 SMSG_TYPE_DONE,
					 SMSG_DONE_SBLOCK_INIT,
					 sblock->dst_smem_addr);
				smsg_send(sblock->dst, &mcmd, -1);
				pr_info("send init done!\n");
				sblock->state = SBLOCK_STATE_READY;
				recovery = 1;
				pr_info("channel %d-%d, SMSG_CMD_SBLOCK_INIT, dst address = 0x%x!\n",
					sblock->dst,
					sblock->channel,
					sblock->dst_smem_addr);

				if (sblock->handler)
					sblock->handler(SBLOCK_NOTIFY_OPEN,
							sblock->data);
			}
			break;
		case SMSG_TYPE_EVENT:
			/* handle sblock send/release events */
			switch (mrecv.flag) {
			case SMSG_EVENT_SBLOCK_SEND:
				ring = sblock->ring;
				/* set read mask. */
				spin_lock_irqsave(&ring->poll_lock, flags);
				ring->poll_mask |= POLLIN | POLLRDNORM;
				spin_unlock_irqrestore(&ring->poll_lock, flags);
				/* SMSG_CH_LOG_LOOP used for log loop */
				if (sblock->channel == SMSG_CH_LOG_LOOP)
					sblock->log_loop_flag = 1;
				wake_up_interruptible_all(&sblock->ring->recvwait);
				if (sblock->handler)
					sblock->handler(SBLOCK_NOTIFY_RECV,
							sblock->data);
				break;
			case SMSG_EVENT_SBLOCK_RELEASE:
				ring = sblock->ring;
				/* set write mask. */
				spin_lock_irqsave(&ring->poll_lock, flags);
				ring->poll_mask |= POLLOUT | POLLWRNORM;
				spin_unlock_irqrestore(&ring->poll_lock, flags);
				wake_up_interruptible_all(&sblock->ring->getwait);
				if (sblock->handler)
					sblock->handler(SBLOCK_NOTIFY_GET,
							sblock->data);
				break;
			default:
				rval = 1;
				break;
			}
			break;
		default:
			rval = 1;
			break;
		};

		if (rval) {
			pr_info("non-handled sblock msg: %d-%d, %d, %d, %d\n",
				sblock->dst, sblock->channel,
				mrecv.type, mrecv.flag, mrecv.value);
			rval = 0;
		}
	}

	pr_info("sblock %d-%d thread stop",
		sblock->dst, sblock->channel);
	return rval;
}

static void sblock_pms_init(uint8_t dst, uint8_t ch, struct sblock_ring *ring)
{
	sprintf(ring->tx_pms_name, "sblock-%d-%d-tx", dst, ch);
	ring->tx_pms = sprd_pms_create(dst, ring->tx_pms_name, true);
	if (!ring->tx_pms)
		pr_warn("create pms %s failed!\n", ring->tx_pms_name);

	sprintf(ring->rx_pms_name, "sblock-%d-%d-rx", dst, ch);
	ring->rx_pms = sprd_pms_create(dst, ring->rx_pms_name, true);
	if (!ring->rx_pms)
		pr_warn("create pms %s failed!\n", ring->rx_pms_name);
}

static void sblock_pms_destroy(struct sblock_ring *ring)
{
	sprd_pms_destroy(ring->tx_pms);
	sprd_pms_destroy(ring->rx_pms);
	ring->tx_pms = NULL;
	ring->rx_pms = NULL;
}

static int sblock_mgr_create(uint8_t dst,
			     uint8_t channel,
			     int pre_cfg,
			     uint8_t smem,
			     uint32_t tx_blk_num, uint32_t tx_blk_sz,
			     uint32_t rx_blk_num, uint32_t rx_blk_sz,
			     struct sblock_mgr **sb_mgr)
{
	struct sblock_mgr *sblock = NULL;
	struct smsg_ipc *sipc = smsg_ipcs[dst];
	int ret;

	pr_debug("dst=%d channel=%d\n", dst, channel);

	if (!sipc)
		return -EINVAL;

	sblock = kzalloc(sizeof(struct sblock_mgr), GFP_KERNEL);
	if (!sblock)
		return -ENOMEM;

	sblock->ring = kzalloc(sizeof(struct sblock_ring), GFP_KERNEL);
	if (!sblock->ring) {
		kfree(sblock);
		return -ENOMEM;
	}

	sblock->dst = dst;
	sblock->channel = channel;
	sblock->pre_cfg = pre_cfg;
	sblock->smem = smem;
	sblock->state = SBLOCK_STATE_IDLE;

	if (!sipc->client) {
		ret = sblock_host_init(sipc, sblock,
					 tx_blk_num, tx_blk_sz,
					 rx_blk_num, rx_blk_sz);
		if (ret) {
			kfree(sblock->ring);
			kfree(sblock);
			return ret;
		}
	}

	sblock_pms_init(dst, channel, sblock->ring);
	init_waitqueue_head(&sblock->ring->getwait);
	init_waitqueue_head(&sblock->ring->recvwait);
	spin_lock_init(&sblock->ring->r_txlock);
	spin_lock_init(&sblock->ring->r_rxlock);
	spin_lock_init(&sblock->ring->p_txlock);
	spin_lock_init(&sblock->ring->p_rxlock);
	spin_lock_init(&sblock->ring->poll_lock);

	*sb_mgr = sblock;

	return 0;
}

bool sblock_has_data(struct sblock_mgr *sblock, bool tx)
{
	volatile struct sblock_ring_header *ringhd;
	struct sblock_ring *ring = sblock->ring;

	ringhd = (volatile struct sblock_ring_header *)(&ring->header->ring);

	if (tx)
		return (ringhd->txblk_wrptr != ringhd->txblk_rdptr);
	else
		return (ringhd->rxblk_wrptr != ringhd->rxblk_rdptr);
}
EXPORT_SYMBOL_GPL(sblock_has_data);

int sblock_create_ex(u8 dst, u8 channel, u8 smem,
		     u32 txblocknum, u32 txblocksize,
		     u32 rxblocknum, u32 rxblocksize,
		     void (*handler)(int event, void *data), void *data)
{
	struct sblock_mgr *sblock = NULL;
	int result;
	u8 ch_index;
	struct smsg_ipc *sipc;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return -EINVAL;
	}

	if (dst >= SIPC_ID_NR) {
		pr_err("dst = %d is invalid\n", dst);
		return -EINVAL;
	}

	pr_debug("dst=%d channel=%d\n", dst, channel);

	result = sblock_mgr_create(dst, channel, 0, smem,
				   txblocknum, txblocksize,
				   rxblocknum, rxblocksize,
				   &sblock);
	if (!result) {
		struct sched_param param = {.sched_priority = 88};
		sblock->thread = kthread_create(sblock_thread, sblock,
						"sblock-%d-%d", dst, channel);
		if (IS_ERR(sblock->thread)) {
			pr_err("Failed to create kthread: sblock-%d-%d\n",
				dst, channel);
			sipc = smsg_ipcs[sblock->dst];
			if (!sipc->client) {
				shmem_ram_unmap(dst, sblock->smem_virt);
				smem_free(dst,
					  sblock->smem_addr,
					  sblock->smem_size);
				kfree(sblock->ring->txrecord);
				kfree(sblock->ring->rxrecord);
			}
			kfree(sblock->ring);
			result = PTR_ERR(sblock->thread);
			kfree(sblock);
			return result;
		}

		/* Prevent the thread task_struct from being destroyed. */
		get_task_struct(sblock->thread);

		sblocks[dst][ch_index] = sblock;
		if ((handler != NULL) && (data != NULL)) {
			result = sblock_register_notifier(dst, channel,
							  handler, data);
			if (result < 0) {
				sblock_destroy(dst, channel);
				return result;
			}
		}
		/* set the thread as a real time thread, and its priority is 11 */
		sched_setscheduler(sblock->thread, SCHED_RR, &param);
		wake_up_process(sblock->thread);
	}

	pr_debug("sblock-%d-%d create over, result = %d\n",
		dst, channel, result);

	return result;
}
EXPORT_SYMBOL_GPL(sblock_create_ex);

int sblock_create(u8 dst, u8 channel,
		  u32 txblocknum, u32 txblocksize,
		  u32 rxblocknum, u32 rxblocksize)
{
	return sblock_create_ex(dst, channel, 0, txblocknum, txblocksize,
				rxblocknum, rxblocksize, NULL, NULL);
}
EXPORT_SYMBOL_GPL(sblock_create);

int sblock_pcfg_create(u8 dst, u8 channel, u8 smem,
		       u32 tx_blk_num, u32 tx_blk_sz,
		       u32 rx_blk_num, u32 rx_blk_sz)
{
	struct sblock_mgr *sblock = NULL;
	int result;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return -EINVAL;
	}
	if (dst >= SIPC_ID_NR) {
		pr_err("sblock_create_ex: dst = %d is invalid\n", dst);
		return -EINVAL;
	}

	pr_debug("dst=%d channel=%d\n", dst, channel);

	result = sblock_mgr_create(dst,
				   channel,
				   1,
				   smem,
				   tx_blk_num, tx_blk_sz,
				   rx_blk_num, rx_blk_sz,
				   &sblock);
	if (!result) {
		struct sched_param param = {.sched_priority = 88};

		sblock->thread = kthread_create(sblock_thread, sblock,
						"sblock-%d-%d", dst, channel);
		if (IS_ERR(sblock->thread)) {
			struct smsg_ipc *sipc;

			pr_err("Failed to create kthread: sblock-%d-%d\n",
				dst, channel);
			sipc = smsg_ipcs[sblock->dst];
			if (!sipc->client) {
				shmem_ram_unmap(dst, sblock->smem_virt);
				smem_free(dst,
					  sblock->smem_addr,
					  sblock->smem_size);
				kfree(sblock->ring->txrecord);
				kfree(sblock->ring->rxrecord);
			}
			kfree(sblock->ring);
			result = PTR_ERR(sblock->thread);
			kfree(sblock);
			return result;
		}

		/* Prevent the thread task_struct from being destroyed. */
		get_task_struct(sblock->thread);

		sblocks[dst][ch_index] = sblock;
		/* Set the thread as a real time thread, and its priority is 11. */
		sched_setscheduler(sblock->thread, SCHED_RR, &param);
		wake_up_process(sblock->thread);
	}

	return result;
}
EXPORT_SYMBOL_GPL(sblock_pcfg_create);

void sblock_destroy(u8 dst, u8 channel)
{
	struct sblock_mgr *sblock;
	u8 ch_index;
	struct smsg_ipc *sipc;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return;
	}

	sblock = sblocks[dst][ch_index];
	if (sblock == NULL)
		return;

	sblock->state = SBLOCK_STATE_IDLE;
	smsg_ch_close(dst, channel, -1);

	/* stop sblock thread if it's created successfully and still alive */
	if (!IS_ERR_OR_NULL(sblock->thread)) {
		kthread_stop(sblock->thread);
		put_task_struct(sblock->thread);
		sblock->thread = NULL;
	}

	if (sblock->ring) {
		sblock_pms_destroy(sblock->ring);
		wake_up_interruptible_all(&sblock->ring->recvwait);
		wake_up_interruptible_all(&sblock->ring->getwait);
		/* kfree(NULL) is safe */
		/* if (sblock->ring->txrecord) */
			kfree(sblock->ring->txrecord);
		/* if (sblock->ring->rxrecord) */
			kfree(sblock->ring->rxrecord);
		kfree(sblock->ring);
	}
	if (sblock->smem_virt)
		shmem_ram_unmap(dst, sblock->smem_virt);

	sipc = smsg_ipcs[dst];
	if (sipc->client)
		smem_free(dst, sblock->smem_addr_debug, sblock->smem_size);
	else
		smem_free(dst, sblock->smem_addr, sblock->smem_size);
	kfree(sblock);

	sblocks[dst][ch_index] = NULL;
}
EXPORT_SYMBOL_GPL(sblock_destroy);

int sblock_pcfg_open(uint8_t dest, uint8_t channel,
		     void (*notifier)(int event, void *client),
		     void *client)
{
	struct sblock_mgr *sblock;
	uint8_t idx;
	int ret;
	struct sched_param param = {.sched_priority = 88};

	pr_debug("dst=%d channel=%d\n", dest, channel);

	if (!notifier)
		return -EINVAL;

	idx = sipc_channel2index(channel);
	if (idx == INVALID_CHANEL_INDEX) {
		pr_err("invalid channel %d!\n", channel);
		return -ENODEV;
	}

	sblock = sblocks[dest][idx];
	if (!sblock)
		return -ENODEV;

	if (!sblock->pre_cfg)
		return -EINVAL;

	if (sblock->thread) {
		pr_err("SBLOCK %u/%u already open",
		       (unsigned int)sblock->dst,
		       (unsigned int)sblock->channel);
		return -EPROTO;
	}

	ret = 0;
	sblock->thread = kthread_create(sblock_thread, sblock,
					"sblock-%d-%d", dest, channel);
	if (IS_ERR(sblock->thread)) {
		pr_err("create thread error\n");
		sblock->thread = NULL;
		ret = -EBUSY;
	} else {
		/* Prevent the thread task_struct from being destroyed. */
		get_task_struct(sblock->thread);

		sblock->handler = notifier;
		sblock->data = client;
		/* set the thread as a real time thread, and its priority is 11 */
		sched_setscheduler(sblock->thread, SCHED_RR, &param);
		wake_up_process(sblock->thread);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(sblock_pcfg_open);

int sblock_close(uint8_t dest, uint8_t channel)
{
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL_GPL(sblock_close);

int sblock_register_notifier(u8 dst, u8 channel,
			     void (*handler)(int event, void *data),
			     void *data)
{
	struct sblock_mgr *sblock;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return -EINVAL;
	}

	sblock = sblocks[dst][ch_index];

	if (!sblock) {
		pr_err("sblock-%d-%d not ready!\n", dst, channel);
		return -ENODEV;
	}
#ifndef CONFIG_SPRD_SIPC_WCN
	if (sblock->handler) {
		pr_err("sblock handler already registered\n");
		return -EBUSY;
	}
#endif
	sblock->handler = handler;
	sblock->data = data;

	return 0;
}
EXPORT_SYMBOL_GPL(sblock_register_notifier);

struct sblock_mgr *sblock_register_notifier_ex(u8 dst, u8 channel,
					       void (*handler)(int event,
							       void *data),
					       void *data)
{
	struct sblock_mgr *sblock;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d invalid!\n", __func__, channel);
		return NULL;
	}

	sblock = sblocks[dst][ch_index];

	if (!sblock) {
		pr_err("%s:sblock-%d-%d not create!\n", __func__, dst, channel);
		return NULL;
	}

	/* client block, after block ready, return the sblock. */
	if (smsg_ipcs[dst]->client && sblock->state != SBLOCK_STATE_READY) {
		pr_err("%s:sblock-%d-%d not ready!\n", __func__, dst, channel);
		return NULL;
	}

	sblock->handler = handler;
	sblock->data = data;

	return sblock;
}
EXPORT_SYMBOL_GPL(sblock_register_notifier_ex);

int sblock_get_smem_cp_addr(uint8_t dest, uint8_t channel,
			    uint32_t *paddr)
{
	struct sblock_mgr *sblock;
	uint8_t idx;

	if (!paddr)
		return -EINVAL;

	idx = sipc_channel2index(channel);
	if (idx == INVALID_CHANEL_INDEX) {
		pr_err("invalid channel %d!\n", channel);
		return -ENODEV;
	}

	sblock = sblocks[dest][idx];
	if (!sblock)
		return -ENODEV;

	*paddr = sblock->dst_smem_addr;

	return 0;
}
EXPORT_SYMBOL_GPL(sblock_get_smem_cp_addr);

int sblock_get(u8 dst, u8 channel, struct sblock *blk, int timeout)
{
	struct sblock_mgr *sblock;
	struct sblock_ring *ring;
	struct sblock_ring_header_op *poolhd_op;
	struct sblock_ring_header_op *ringhd_op;

	int txpos, index;
	int rval = 0;
	unsigned long flags;
	u8 ch_index;
	bool no_data;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return -EINVAL;
	}

	sblock = sblocks[dst][ch_index];

	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		pr_err("sblock-%d-%d not ready!\n", dst, channel);
		return sblock ? -EIO : -ENODEV;
	}

	ring = sblock->ring;
	poolhd_op = &(ring->header_op.poolhd_op);
	ringhd_op = &(ring->header_op.ringhd_op);

	/* must request resource before read or write share memory */
	rval = sipc_smem_request_resource(ring->tx_pms, dst, timeout);
	if (rval < 0)
		return rval;

	spin_lock_irqsave(&ring->poll_lock, flags);
	no_data = *(poolhd_op->tx_rd_p) == *(poolhd_op->tx_wt_p);
	/* update write mask */
	if (no_data)
		ring->poll_mask &= ~(POLLOUT | POLLWRNORM);
	else
		ring->poll_mask |= POLLOUT | POLLWRNORM;
	spin_unlock_irqrestore(&ring->poll_lock, flags);

	/* release resource */
	sipc_smem_release_resource(ring->tx_pms, dst);

	if (no_data) {
		if (timeout == 0) {
			/* no wait */
			pr_err("%d-%d is empty!\n", dst, channel);
			rval = -ENODATA;
		} else if (timeout < 0) {
			/* wait forever */
			rval = wait_event_interruptible(ring->getwait,
							*(poolhd_op->tx_rd_p) != *(poolhd_op->tx_wt_p) ||
							sblock->state == SBLOCK_STATE_IDLE);
			if (rval < 0)
				pr_debug("wait interrupted!\n");

			if (sblock->state == SBLOCK_STATE_IDLE) {
				pr_err("state is idle!\n");
				rval = -EIO;
			}
		} else {
			/* wait timeout */
			rval = wait_event_interruptible_timeout(ring->getwait,
								*(poolhd_op->tx_rd_p) != *(poolhd_op->tx_wt_p) ||
								sblock == SBLOCK_STATE_IDLE,
				timeout);
			if (rval < 0) {
				pr_debug("wait interrupted!\n");
			} else if (rval == 0) {
				pr_info("wait timeout!\n");
				rval = -ETIME;
			}

			if (sblock->state == SBLOCK_STATE_IDLE) {
				pr_info("state is idle!\n");
				rval = -EIO;
			}
		}
	}

	if (rval < 0)
		return rval;

	/* must request resource before read or write share memory */
	rval = sipc_smem_request_resource(ring->tx_pms, dst, timeout);
	if (rval < 0)
		return rval;

	/* multi-gotter may cause got failure */
	spin_lock_irqsave(&ring->p_txlock, flags);
	if (*(poolhd_op->tx_rd_p) != *(poolhd_op->tx_wt_p) &&
			sblock->state == SBLOCK_STATE_READY) {
		txpos = sblock_get_ringpos(*(poolhd_op->tx_rd_p),
					   poolhd_op->tx_count);
		blk->addr = sblock->smem_virt +
			    (ring->p_txblks[txpos].addr -
			     sblock->stored_smem_addr);
		blk->length = poolhd_op->tx_size;
		*(poolhd_op->tx_rd_p) = *(poolhd_op->tx_rd_p) + 1;
		index = sblock_get_index((blk->addr - ring->txblk_virt),
					 sblock->txblksz);
		ring->txrecord[index] = SBLOCK_BLK_STATE_PENDING;
	} else {
		/* release resource */
		sipc_smem_release_resource(ring->tx_pms, dst);
		rval = sblock->state == SBLOCK_STATE_READY ? -EAGAIN : -EIO;
	}
	spin_unlock_irqrestore(&ring->p_txlock, flags);

	spin_lock_irqsave(&ring->poll_lock, flags);
	/* update write mask */
	if (*(poolhd_op->tx_wt_p) == *(poolhd_op->tx_rd_p))
		ring->poll_mask &= ~(POLLOUT | POLLWRNORM);
	else
		ring->poll_mask |= POLLOUT | POLLWRNORM;
	spin_unlock_irqrestore(&ring->poll_lock, flags);

	return rval;
}
EXPORT_SYMBOL_GPL(sblock_get);

static int sblock_send_ex(u8 dst, u8 channel,
			  struct sblock *blk, bool yell)
{
	struct sblock_mgr *sblock;
	struct sblock_ring *ring;
	struct sblock_ring_header_op *ringhd_op;
	struct smsg mevt;
	int txpos, index;
	int rval = 0;
	unsigned long flags;
	u8 ch_index;
	bool send_event = false;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return -EINVAL;
	}

	sblock = sblocks[dst][ch_index];

	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		pr_err("sblock-%d-%d not ready!\n", dst, channel);
		return sblock ? -EIO : -ENODEV;
	}

	pr_debug("sblock_send: dst=%d, channel=%d, addr=%p, len=%d\n",
		 dst, channel, blk->addr, blk->length);

	ring = sblock->ring;
	ringhd_op = &(ring->header_op.ringhd_op);

	spin_lock_irqsave(&ring->r_txlock, flags);

	txpos = sblock_get_ringpos(*(ringhd_op->tx_wt_p), ringhd_op->tx_count);
	ring->r_txblks[txpos].addr = blk->addr -
				     sblock->smem_virt +
				     sblock->stored_smem_addr;
	ring->r_txblks[txpos].length = blk->length;
	pr_debug("sblock_send: channel=%d, wrptr=%d, txpos=%d, addr=%x\n",
		 channel, *(ringhd_op->tx_wt_p),
		 txpos, ring->r_txblks[txpos].addr);
	*(ringhd_op->tx_wt_p) = *(ringhd_op->tx_wt_p) + 1;

	if (sblock->state == SBLOCK_STATE_READY) {
		if (yell) {
			send_event = true;
		} else if (!ring->yell) {
			if ((int)(*(ringhd_op->tx_wt_p) -
				  *(ringhd_op->tx_rd_p)) == 1)
				ring->yell = 1;
		}
	}
	index = sblock_get_index((blk->addr - ring->txblk_virt),
				 sblock->txblksz);
	ring->txrecord[index] = SBLOCK_BLK_STATE_DONE;

	/* request in sblock_get, release here */
	sipc_smem_release_resource(ring->tx_pms, dst);

	/*
	 * smsg_send may caused schedule,
	 * can't be called in spinlock protected context.
	 */
	if (send_event && (((int)(*(ringhd_op->tx_wt_p) -
	    *(ringhd_op->tx_rd_p)) == 1) || ((int)(*(ringhd_op->tx_wt_p) -
	    *(ringhd_op->tx_rd_p)) >= ringhd_op->tx_count))) {
		spin_unlock_irqrestore(&ring->r_txlock, flags);
		smsg_set(&mevt, channel,
			 SMSG_TYPE_EVENT,
			 SMSG_EVENT_SBLOCK_SEND,
			 0);
		rval = smsg_send(dst, &mevt, -1);
	} else
		spin_unlock_irqrestore(&ring->r_txlock, flags);

	return rval;
}

int sblock_send(u8 dst, u8 channel, struct sblock *blk)
{
	return sblock_send_ex(dst, channel, blk, true);
}
EXPORT_SYMBOL_GPL(sblock_send);

int sblock_send_prepare(u8 dst, u8 channel, struct sblock *blk)
{
	return sblock_send_ex(dst, channel, blk, false);
}
EXPORT_SYMBOL_GPL(sblock_send_prepare);

int sblock_send_finish(u8 dst, u8 channel)
{
	struct sblock_mgr *sblock;
	struct sblock_ring *ring;
	struct sblock_ring_header_op *ringhd_op;
	struct smsg mevt;
	int rval = 0;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return -EINVAL;
	}

	sblock = sblocks[dst][ch_index];
	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		pr_err("sblock-%d-%d not ready!\n", dst, channel);
		return sblock ? -EIO : -ENODEV;
	}

	ring = sblock->ring;
	ringhd_op = &(ring->header_op.ringhd_op);

	/* must wait resource before read or write share memory */
	rval = sipc_smem_request_resource(ring->tx_pms, dst, -1);
	if (rval)
		return rval;

	if (*(ringhd_op->tx_wt_p) != *(ringhd_op->tx_rd_p)) {
		smsg_set(&mevt, channel,
			 SMSG_TYPE_EVENT,
			 SMSG_EVENT_SBLOCK_SEND, 0);
		rval = smsg_send(dst, &mevt, 0);
	}
	/* release resource */
	sipc_smem_release_resource(ring->tx_pms, dst);

	return rval;
}
EXPORT_SYMBOL_GPL(sblock_send_finish);

int sblock_receive(u8 dst, u8 channel,
		   struct sblock *blk, int timeout)
{
	struct sblock_mgr *sblock;
	struct sblock_ring *ring;
	struct sblock_ring_header_op *ringhd_op;
	int rxpos, index, rval = 0;
	unsigned long flags;
	u8 ch_index;
	bool no_data;
	u8 dst_index;

	if (channel == SMSG_CH_PLOG) {
		dst_index = slog_dst2index[dst];
		if (dst_index == INVALID_SLOG_DST_INDEX) {
			pr_err("slog dst %d invalid!\n", dst);
			return -EINVAL;
		}
	}
	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return -EINVAL;
	}

	sblock = sblocks[dst][ch_index];

	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		pr_err("sblock-%d-%d not ready!\n", dst, channel);
		return sblock ? -EIO : -ENODEV;
	}
	pr_debug("dst=%d, channel=%d, timeout=%d\n",
		 dst, channel, timeout);

	ring = sblock->ring;
	ringhd_op = &(ring->header_op.ringhd_op);

	/* must request resource before read or write share memory */
	rval = sipc_smem_request_resource(ring->rx_pms, dst, timeout);
	if (rval < 0)
		return rval;

	pr_debug("channel=%d, wrptr=%d, rdptr=%d",
		 channel,
		 *(ringhd_op->rx_wt_p),
		 *(ringhd_op->rx_rd_p));

	spin_lock_irqsave(&ring->poll_lock, flags);
	no_data = *(ringhd_op->rx_wt_p) == *(ringhd_op->rx_rd_p);
	/* update read mask */
	if (no_data)
		ring->poll_mask &= ~(POLLIN | POLLRDNORM);
	else
		ring->poll_mask |= POLLIN | POLLRDNORM;
	spin_unlock_irqrestore(&ring->poll_lock, flags);

	/* release resource */
	sipc_smem_release_resource(ring->rx_pms, dst);

	if (no_data) {
		if (timeout == 0) {
			/* no wait */
			pr_debug("%d-%d is empty!\n",
				 dst, channel);
			rval = -ENODATA;
		} else if (timeout < 0) {
			/* wait forever */
			rval = wait_event_interruptible(ring->recvwait,
				*(ringhd_op->rx_wt_p) != *(ringhd_op->rx_rd_p));
			if (rval < 0)
				pr_info("wait interrupted!\n");

			if (sblock->state == SBLOCK_STATE_IDLE) {
				pr_info("state is idle!\n");
				rval = -EIO;
			}

		} else {
			/* wait timeout */
			rval = wait_event_interruptible_timeout(ring->recvwait,
				*(ringhd_op->rx_wt_p) != *(ringhd_op->rx_rd_p),
				timeout);
			if (rval < 0) {
				pr_info("wait interrupted!\n");
			} else if (rval == 0) {
				pr_info("wait timeout!\n");
				rval = -ETIME;
			}

			if (sblock->state == SBLOCK_STATE_IDLE) {
				pr_info("state is idle!\n");
				rval = -EIO;
			}
		}
	}

	if (rval < 0)
		return rval;

	/* must request resource before read or write share memory */
	rval = sipc_smem_request_resource(ring->rx_pms, dst, timeout);
	if (rval < 0)
		return rval;

	/* multi-receiver may cause recv failure */
	spin_lock_irqsave(&ring->r_rxlock, flags);

	if (*(ringhd_op->rx_wt_p) != *(ringhd_op->rx_rd_p) &&
			sblock->state == SBLOCK_STATE_READY) {
		rxpos = sblock_get_ringpos(*(ringhd_op->rx_rd_p),
					   ringhd_op->rx_count);
		if (channel == SMSG_CH_PLOG) {
			if ((!sb_prepare_list_info[dst_index].first_recv_flag ||
			    (ring->r_rxblks[rxpos].addr != ringhd_op->rx_addr)) &&
			    (ring->r_rxblks[rxpos].addr -
			    sb_prepare_list_info[dst_index].recv_last_addr != ringhd_op->rx_size) &&
			    (sb_prepare_list_info[dst_index].recv_last_addr -
			    ring->r_rxblks[rxpos].addr != ringhd_op->rx_size *
			    (ringhd_op->rx_count-1)) &&
			    (sb_prepare_list_info[dst_index].receive_not_continue_cnt <
			     MAX_NOT_CONTINUE_CNT)) {
				pr_err("receive %s sblock is not continuous, addr: 0x%x, recv last addr: 0x%x\n",
					sb_prepare_list_info[dst_index].name,
					ring->r_rxblks[rxpos].addr,
					sb_prepare_list_info[dst_index].recv_last_addr);
				sb_prepare_list_info[dst_index].receive_not_continue_cnt++;
			} else if (sb_prepare_list_info[dst_index].first_recv_flag) {
				sb_prepare_list_info[dst_index].first_recv_flag = 0;
				pr_info("receive first %s sblock, addr: 0x%x, recv last addr: 0x%x\n",
					sb_prepare_list_info[dst_index].name,
					ring->r_rxblks[rxpos].addr,
					sb_prepare_list_info[dst_index].recv_last_addr);
			}
			sb_prepare_list_info[dst_index].recv_last_addr = ring->r_rxblks[rxpos].addr;
		}
		blk->addr = ring->r_rxblks[rxpos].addr -
			    sblock->stored_smem_addr +
			    sblock->smem_virt;
		blk->length = ring->r_rxblks[rxpos].length;
		*(ringhd_op->rx_rd_p) = *(ringhd_op->rx_rd_p) + 1;
		pr_debug("channel=%d, rxpos=%d, addr=%p, len=%d\n",
			 channel, rxpos, blk->addr, blk->length);
		index = sblock_get_index((blk->addr - ring->rxblk_virt),
					 sblock->rxblksz);

		if (ring->rxrecord[index] != SBLOCK_BLK_STATE_DONE)
			pr_info("warning: ring->rxrecord[index] != SBLOCK_BLK_STATE_DONE");
		ring->rxrecord[index] = SBLOCK_BLK_STATE_PENDING;
	} else {
		/* release resource */
		sipc_smem_release_resource(ring->rx_pms, dst);
		rval = sblock->state == SBLOCK_STATE_READY ? -EAGAIN : -EIO;
	}
	spin_unlock_irqrestore(&ring->r_rxlock, flags);

	spin_lock_irqsave(&ring->poll_lock, flags);
	/* update read mask */
	if (*(ringhd_op->rx_wt_p) == *(ringhd_op->rx_rd_p))
		ring->poll_mask &= ~(POLLIN | POLLRDNORM);
	else
		ring->poll_mask |= POLLIN | POLLRDNORM;
	spin_unlock_irqrestore(&ring->poll_lock, flags);

	return rval;
}
EXPORT_SYMBOL_GPL(sblock_receive);

/*
 * sblock_receive_loop_ex, sblock_receive_loop and
 * sblock_release_loop used for log loop
 */
static int sblock_receive_loop_ex(u8 dst, u8 channel,
				  struct sblock *blk, int timeout)
{
	struct sblock_mgr *sblock;
	struct sblock_ring *ring;
	volatile struct sblock_ring_header *ringhd;
	int rxpos, index, rval = 0;
	unsigned long flags;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return -EINVAL;
	}

	sblock = sblocks[dst][ch_index];

	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		pr_err("sblock-%d-%d not ready!\n", dst, channel);
		return sblock ? -EIO : -ENODEV;
	}

	ring = sblock->ring;
	ringhd = (volatile struct sblock_ring_header *)(&ring->header->ring);

	pr_debug("sblock_receive: dst=%d, channel=%d, timeout=%d\n",
			dst, channel, timeout);
	pr_debug("sblock_receive: channel=%d, wrptr=%d, rdptr=%d",
			channel, ringhd->rxblk_wrptr, ringhd->rxblk_rdptr);

	if (timeout < 0) {
		/* wait forever */
		rval = wait_event_interruptible(ring->recvwait, sblock->log_loop_flag);
		sblock->log_loop_flag = 0;
		if (rval < 0)
			pr_info("sblock_receive wait interrupted!\n");

		if (sblock->state == SBLOCK_STATE_IDLE) {
			pr_info("sblock_receive sblock state is idle!\n");
			rval = -EIO;
			return rval;
		}
		sblock->rxblk_end_r_wrptr = ringhd->rxblk_wrptr;
		spin_lock_irqsave(&ring->r_rxlock, flags);
		ringhd->rxblk_rdptr = sblock->rxblk_end_r_wrptr;
		spin_unlock_irqrestore(&ring->r_rxlock, flags);
		sblock->wait_recv_flag = 1;

	}

	/* multi-receiver may cause recv failure */
	spin_lock_irqsave(&ring->r_rxlock, flags);

	if (sblock->state == SBLOCK_STATE_READY) {
		rxpos = sblock_get_ringpos(ringhd->rxblk_rdptr,
					   ringhd->rxblk_count);
		blk->addr = ring->r_rxblks[rxpos].addr -
			    sblock->stored_smem_addr +
			    sblock->smem_virt;
		blk->length = ring->r_rxblks[rxpos].length;
		ringhd->rxblk_rdptr = ringhd->rxblk_rdptr + 1;
		if (ringhd->rxblk_rdptr >= ringhd->rxblk_count)
			ringhd->rxblk_rdptr = R_RD_START_ADDR;
		if (ringhd->rxblk_rdptr == sblock->rxblk_end_r_wrptr) {
			sblock->wait_recv_flag = 0;
			ringhd->rxblk_rdptr = R_RD_START_ADDR;
		}
		pr_debug("sblock_receive: channel=%d, rxpos=%d, addr=%p, len=%d\n",
			channel, rxpos, blk->addr, blk->length);
		index = sblock_get_index((blk->addr - ring->rxblk_virt),
					 sblock->rxblksz);
		ring->rxrecord[index] = SBLOCK_BLK_STATE_PENDING;
	} else {
		rval = sblock->state == SBLOCK_STATE_READY ? -EAGAIN : -EIO;
	}

	spin_unlock_irqrestore(&ring->r_rxlock, flags);

	return rval;
}

int sblock_receive_loop(u8 dst, u8 channel,
			struct sblock *blk)
{
	int rval;
	struct sblock_mgr *sblock;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return -EINVAL;
	}

	sblock = sblocks[dst][ch_index];
	if (sblock->wait_recv_flag == 0) {
		rval = sblock_receive_loop_ex(dst, channel, blk, -1);
	} else {
		rval = sblock_receive_loop_ex(dst, channel, blk, 0);
	}
	return rval;
}
EXPORT_SYMBOL_GPL(sblock_receive_loop);

int sblock_get_arrived_count(u8 dst, u8 channel)
{
	struct sblock_mgr *sblock;
	struct sblock_ring *ring;
	struct sblock_ring_header_op *ringhd_op;
	int blk_count = 0;
	unsigned long flags;
	u8 ch_index;
	int rval;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return -EINVAL;
	}

	sblock = sblocks[dst][ch_index];
	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		pr_err("sblock-%d-%d not ready!\n", dst, channel);
		return -ENODEV;
	}

	ring = sblock->ring;
	ringhd_op = &(ring->header_op.ringhd_op);

	/* must request resource before read or write share memory */
	rval = sipc_smem_request_resource(ring->rx_pms, dst, -1);
	if (rval < 0)
		return rval;

	spin_lock_irqsave(&ring->r_rxlock, flags);
	blk_count = (int)(*(ringhd_op->rx_wt_p) - *(ringhd_op->rx_rd_p));
	spin_unlock_irqrestore(&ring->r_rxlock, flags);
	/* release resource */
	sipc_smem_release_resource(ring->rx_pms, dst);

	return blk_count;

}
EXPORT_SYMBOL_GPL(sblock_get_arrived_count);

struct sblock_mgr *sblock_mgr_get_addr(u8 dst, u8 channel)
{
	struct sblock_mgr *sblock;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return NULL;
	}

	sblock = sblocks[dst][ch_index];
	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		pr_err("sblock-%d-%d not ready!\n", dst, channel);
		return NULL;
	}

	return sblock;
}
EXPORT_SYMBOL_GPL(sblock_mgr_get_addr);

int sblock_get_free_count(u8 dst, u8 channel)
{
	struct sblock_mgr *sblock;
	struct sblock_ring *ring;
	struct sblock_ring_header_op *poolhd_op;
	int blk_count = 0, rval;
	unsigned long flags;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return -EINVAL;
	}

	sblock = sblocks[dst][ch_index];
	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		pr_err("sblock-%d-%d not ready!\n", dst, channel);
		return -ENODEV;
	}

	ring = sblock->ring;
	poolhd_op = &(ring->header_op.poolhd_op);

	/* must request resource before read or write share memory */
	rval = sipc_smem_request_resource(ring->tx_pms, dst, -1);
	if (rval < 0)
		return rval;

	spin_lock_irqsave(&ring->p_txlock, flags);
	blk_count = (int)(*(poolhd_op->tx_wt_p) - *(poolhd_op->tx_rd_p));
	spin_unlock_irqrestore(&ring->p_txlock, flags);
	/* release resource */
	sipc_smem_release_resource(ring->tx_pms, dst);

	return blk_count;
}
EXPORT_SYMBOL_GPL(sblock_get_free_count);

int sblock_release(u8 dst, u8 channel, struct sblock *blk)
{
	struct sblock_mgr *sblock;
	struct sblock_ring *ring;
	struct sblock_ring_header_op *poolhd_op;
	struct smsg mevt;
	unsigned long flags;
	int rxpos;
	int index;
	u8 ch_index;
	u8 dst_index;
	bool send_event = false;

	if (channel == SMSG_CH_PLOG) {
		dst_index = slog_dst2index[dst];
		if (dst_index == INVALID_SLOG_DST_INDEX) {
			pr_err("slog dst %d invalid!\n", dst);
			return -EINVAL;
		}
	}

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return -EINVAL;
	}

	sblock = sblocks[dst][ch_index];
	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		pr_err("sblock-%d-%d not ready!\n", dst, channel);
		return -ENODEV;
	}

	pr_debug("dst=%d, channel=%d, addr=%p, len=%d\n",
		 dst, channel, blk->addr, blk->length);

	ring = sblock->ring;
	poolhd_op = &(ring->header_op.poolhd_op);

	spin_lock_irqsave(&ring->p_rxlock, flags);
	if (channel == SMSG_CH_PLOG) {
		if ((sb_prepare_list_info[dst_index].first_release_flag && (blk->addr -
		    sblock->smem_virt + sblock->stored_smem_addr == poolhd_op->rx_addr)) ||
		    (blk->addr - sb_prepare_list_info[dst_index].release_last_addr ==
		    poolhd_op->rx_size) || (sb_prepare_list_info[dst_index].release_last_addr -
		    blk->addr == poolhd_op->rx_size * (poolhd_op->rx_count-1))) {
			if (sb_prepare_list_info[dst_index].first_release_flag) {
				sb_prepare_list_info[dst_index].first_release_flag = 0;
				pr_info("release first %s sblock, addr: 0x%lx, release last addr: 0x%lx\n",
					sb_prepare_list_info[dst_index].name,
					blk->addr - sblock->smem_virt + sblock->stored_smem_addr,
					sb_prepare_list_info[dst_index].release_last_addr -
					sblock->smem_virt + sblock->stored_smem_addr);
			}
		} else if (sb_prepare_list_info[dst_index].release_not_continue_cnt <
			   MAX_NOT_CONTINUE_CNT) {
			pr_err("release %s sblock is not continuous, addr: 0x%lx, release last addr: 0x%lx\n",
				sb_prepare_list_info[dst_index].name,
				blk->addr - sblock->smem_virt + sblock->stored_smem_addr,
				sb_prepare_list_info[dst_index].release_last_addr -
				sblock->smem_virt + sblock->stored_smem_addr);
			sb_prepare_list_info[dst_index].release_not_continue_cnt++;
		}
	}
	rxpos = sblock_get_ringpos(*(poolhd_op->rx_wt_p), poolhd_op->rx_count);
	ring->p_rxblks[rxpos].addr = blk->addr -
				     sblock->smem_virt +
				     sblock->stored_smem_addr;
	ring->p_rxblks[rxpos].length = poolhd_op->rx_size;
	*(poolhd_op->rx_wt_p) = *(poolhd_op->rx_wt_p) + 1;
	if (channel == SMSG_CH_PLOG)
		sb_prepare_list_info[dst_index].release_last_addr = blk->addr;
	pr_debug("addr=%x\n", ring->p_rxblks[rxpos].addr);

	if ((int)(*(poolhd_op->rx_wt_p) - *(poolhd_op->rx_rd_p)) == 1 &&
	    sblock->state == SBLOCK_STATE_READY) {
		/* send smsg to notify the peer side */
		send_event = true;
	}

	index = sblock_get_index((blk->addr - ring->rxblk_virt),
				 sblock->rxblksz);

	if (ring->rxrecord[index] != SBLOCK_BLK_STATE_PENDING)
		pr_info("warning: ring->rxrecord[index] != SBLOCK_BLK_STATE_PENDING");
	ring->rxrecord[index] = SBLOCK_BLK_STATE_DONE;

	spin_unlock_irqrestore(&ring->p_rxlock, flags);

	/* request in sblock_receive, release here */
	sipc_smem_release_resource(ring->rx_pms, dst);

	/*
	 * smsg_send may caused schedule,
	 * can't be called in spinlock protected context.
	 */
	if (send_event) {
		smsg_set(&mevt, channel,
			 SMSG_TYPE_EVENT,
			 SMSG_EVENT_SBLOCK_RELEASE,
			 0);
		smsg_send(dst, &mevt, -1);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(sblock_release);

int sblock_release_loop(u8 dst, u8 channel, struct sblock *blk)
{
	struct sblock_mgr *sblock;
	struct sblock_ring *ring;
	volatile struct sblock_ring_header *ringhd;
	volatile struct sblock_ring_header *poolhd;
	unsigned long flags;
	int rxpos;
	int index;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return -EINVAL;
	}

	sblock = sblocks[dst][ch_index];
	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		pr_err("sblock-%d-%d not ready!\n", dst, channel);
		return -ENODEV;
	}

	pr_debug("sblock_release: dst=%d, channel=%d, addr=%p, len=%d\n",
			dst, channel, blk->addr, blk->length);

	ring = sblock->ring;
	ringhd = (volatile struct sblock_ring_header *)(&ring->header->ring);
	poolhd = (volatile struct sblock_ring_header *)(&ring->header->pool);

	spin_lock_irqsave(&ring->p_rxlock, flags);
	if (sblock->wait_release_flag == 0) {
		sblock->wait_release_flag = 1;
		sblock->rxblk_end_p_rdptr = poolhd->rxblk_rdptr;
		poolhd->rxblk_wrptr = sblock->rxblk_end_p_rdptr;
	}
	rxpos = sblock_get_ringpos(poolhd->rxblk_wrptr, poolhd->rxblk_count);
	ring->p_rxblks[rxpos].addr = blk->addr -
				     sblock->smem_virt +
				     sblock->stored_smem_addr;
	ring->p_rxblks[rxpos].length = poolhd->rxblk_size;
	poolhd->rxblk_wrptr = poolhd->rxblk_wrptr + 1;
	if (poolhd->rxblk_wrptr >= poolhd->rxblk_count)
		poolhd->rxblk_wrptr = 0;
	if (poolhd->rxblk_wrptr == sblock->rxblk_end_p_rdptr) {
		sblock->wait_release_flag = 0;
		poolhd->rxblk_wrptr = poolhd->rxblk_count;
	}
	pr_debug("sblock_release: addr=%x\n", ring->p_rxblks[rxpos].addr);

	index = sblock_get_index((blk->addr - ring->rxblk_virt),
				 sblock->rxblksz);
	ring->rxrecord[index] = SBLOCK_BLK_STATE_DONE;

	spin_unlock_irqrestore(&ring->p_rxlock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(sblock_release_loop);

unsigned int sblock_poll_wait(u8 dst, u8 channel,
			      struct file *filp, poll_table *wait)
{
	struct sblock_mgr *sblock;
	struct sblock_ring *ring;
	struct sblock_ring_header_op *poolhd_op;
	struct sblock_ring_header_op *ringhd_op;
	unsigned int mask = 0;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return mask;
	}
	sblock = sblocks[dst][ch_index];

	if (!sblock)
		return mask;
	ring = sblock->ring;
	poolhd_op = &(ring->header_op.poolhd_op);
	ringhd_op = &(ring->header_op.ringhd_op);

	if (sblock->state != SBLOCK_STATE_READY) {
		pr_err("sblock-%d-%d not ready to poll !\n",
		       dst, channel);
		return mask;
	}
	poll_wait(filp, &ring->recvwait, wait);
	poll_wait(filp, &ring->getwait, wait);
	if (*(ringhd_op->rx_wt_p) != *(ringhd_op->rx_rd_p))
		mask |= POLLIN | POLLRDNORM;
	if (*(poolhd_op->tx_rd_p) != *(poolhd_op->tx_wt_p))
		mask |= POLLOUT | POLLWRNORM;
	return mask;
}
EXPORT_SYMBOL_GPL(sblock_poll_wait);

int sblock_query(u8 dst, u8 channel)
{
	struct sblock_mgr *sblock = NULL;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return -EINVAL;
	}

	sblock = sblocks[dst][ch_index];
	if (!sblock)
		return -ENODEV;
	if (sblock->state != SBLOCK_STATE_READY) {
		pr_debug("sblock-%d-%d not ready!\n", dst, channel);
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(sblock_query);

#if defined(CONFIG_DEBUG_FS)
static int sblock_debug_show(struct seq_file *m, void *private)
{
	struct smsg_ipc *sipc = NULL;
	struct sblock_mgr *sblock;
	struct sblock_ring  *ring;
	struct sblock_ring_header_op *poolhd_op;
	struct sblock_ring_header_op *ringhd_op;
	int i, j;

	for (i = 0; i < SIPC_ID_NR; i++) {
		sipc = smsg_ipcs[i];
		if (!sipc)
			continue;

		/* must request resource before read or write share memory */
		if (sipc_smem_request_resource(sipc->sipc_pms,
					       sipc->dst, 1000) < 0)
			continue;

		for (j = 0;  j < SMSG_VALID_CH_NR; j++) {
			sblock = sblocks[i][j];
			if (!sblock)
				continue;

			sipc_debug_putline(m, '*', 170);
			seq_printf(m, "sblock dst %d, channel: %3d, state: %d, smem_virt: 0x%lx, smem_addr: 0x%0x, dst_smem_addr: 0x%0x, smem_size: 0x%0x, txblksz: %d, rxblksz: %d\n",
				   sblock->dst,
				   sblock->channel,
				   sblock->state,
				   (unsigned long)sblock->smem_virt,
				   sblock->smem_addr,
				   sblock->dst_smem_addr,
				   sblock->smem_size,
				   sblock->txblksz,
				   sblock->rxblksz);

			/*
			 * in precfg channel,  the ring pinter can be null
			 * before the the block manger has been created
			 * and ring->header pointer can also be null
			 * before the block handshake with host,
			 * so must add null pointer protect here.
			 */
			ring = sblock->ring;
			if (!ring || !ring->header)
				continue;

			poolhd_op = &(ring->header_op.poolhd_op);
			ringhd_op = &(ring->header_op.ringhd_op);
			seq_printf(m, "sblock ring: txblk_virt :0x%lx, rxblk_virt :0x%lx\n",
				   (unsigned long)ring->txblk_virt,
				   (unsigned long)ring->rxblk_virt);
			seq_printf(m, "sblock ring header: rxblk_addr :0x%0x, rxblk_rdptr :0x%0x, rxblk_wrptr :0x%0x, rxblk_size :%d, rxblk_count :%d, rxblk_blks: 0x%0x\n",
				   ringhd_op->rx_addr, *(ringhd_op->rx_rd_p),
				   *(ringhd_op->rx_wt_p), ringhd_op->rx_size,
				   ringhd_op->rx_count, ringhd_op->rx_blks);
			seq_printf(m, "sblock ring header: txblk_addr :0x%0x, txblk_rdptr :0x%0x, txblk_wrptr :0x%0x, txblk_size :%d, txblk_count :%d, txblk_blks: 0x%0x\n",
				   ringhd_op->tx_addr, *(ringhd_op->tx_rd_p),
				   *(ringhd_op->tx_wt_p), ringhd_op->tx_size,
				   ringhd_op->tx_count, ringhd_op->tx_blks);
			seq_printf(m, "sblock pool header: rxblk_addr :0x%0x, rxblk_rdptr :0x%0x, rxblk_wrptr :0x%0x, rxblk_size :%d, rxpool_count :%d, rxblk_blks: 0x%0x\n",
				   poolhd_op->rx_addr, *(poolhd_op->rx_rd_p),
				   *(poolhd_op->rx_wt_p), poolhd_op->rx_size,
				   poolhd_op->rx_count, poolhd_op->rx_blks);
			seq_printf(m, "sblock pool header: txblk_addr :0x%0x, txblk_rdptr :0x%0x, txblk_wrptr :0x%0x, txblk_size :%d, txpool_count :%d, txblk_blks: 0x%0x\n",
				   poolhd_op->tx_addr, *(poolhd_op->tx_rd_p),
				   *(poolhd_op->tx_wt_p), poolhd_op->tx_size,
				   poolhd_op->tx_count, poolhd_op->tx_blks);
		}
		/* release resource */
		sipc_smem_release_resource(sipc->sipc_pms, sipc->dst);
	}
	return 0;

}

static int sblock_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, sblock_debug_show, inode->i_private);
}

static const struct file_operations sblock_debug_fops = {
	.open = sblock_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int sblock_init_debug(void)
{
	int rval;
	dev_t dev_no;

	sblock_dev = kzalloc(sizeof(struct sblock_device), GFP_KERNEL);
	if (!sblock_dev)
		return -ENOMEM;
	rval = alloc_chrdev_region(&dev_no, 0, 1, "sblock_debug");
	if (rval) {
		pr_err("Failed to alloc chrdev region\n");
		goto free_sblock_dev;
	}
	sblock_dev->major = MAJOR(dev_no);
	sblock_dev->minor = MINOR(dev_no);
	cdev_init(&sblock_dev->cdev, &sblock_debug_fops);
	rval = cdev_add(&sblock_dev->cdev, dev_no, 1);
	if (rval) {
		pr_err("Failed to add sblock cdev\n");
		goto free_devno;
	}
	sblock_class = class_create(THIS_MODULE, "sblock");
	if (IS_ERR(sblock_class)) {
		rval = PTR_ERR(sblock_class);
		pr_err("Failed to create sblock class\n");
		goto free_cdev;
	}
	sblock_dev->sys_dev = device_create(sblock_class, NULL,
				       dev_no,
				       sblock_dev, "sipc_sblock");

	return 0;

free_cdev:
	cdev_del(&sblock_dev->cdev);

free_devno:
	unregister_chrdev_region(dev_no, 1);

free_sblock_dev:
	kfree(sblock_dev);
	sblock_dev = NULL;
	return rval;
}
EXPORT_SYMBOL_GPL(sblock_init_debug);

void sblock_destroy_debug(void)
{
	if (sblock_dev) {
		dev_t dev_no = MKDEV(sblock_dev->major, sblock_dev->minor);

		if (sblock_dev->sys_dev) {
			device_destroy(sblock_class, dev_no);
			sblock_dev->sys_dev = NULL;
		}

		if (!IS_ERR(sblock_class))
			class_destroy(sblock_class);

		unregister_chrdev_region(dev_no, 1);
		cdev_del(&sblock_dev->cdev);
		kfree(sblock_dev);
		sblock_dev = NULL;
	}
}
EXPORT_SYMBOL_GPL(sblock_destroy_debug);
#endif /* CONFIG_DEBUG_FS */


MODULE_AUTHOR("Chen Gaopeng");
MODULE_DESCRIPTION("SIPC/SBLOCK driver");
MODULE_LICENSE("GPL v2");
