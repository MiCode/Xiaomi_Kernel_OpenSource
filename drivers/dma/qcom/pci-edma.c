/* Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/atomic.h>
#include <linux/completion.h>
#include <linux/ctype.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ipc_logging.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_dma.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/pci.h>
#include <linux/scatterlist.h>
#include <linux/sched_clock.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/msm_ep_pcie.h>
#include "../dmaengine.h"

/* global logging macros */
#define EDMA_LOG(e_dev, fmt, ...) do { \
	if (e_dev->klog_lvl != LOG_LVL_MASK_ALL) \
		dev_dbg(e_dev->dev, "[I] %s: %s: " fmt, e_dev->label, \
			__func__,  ##__VA_ARGS__); \
	if (e_dev->ipc_log && e_dev->ipc_log_lvl != LOG_LVL_MASK_ALL) \
		ipc_log_string(e_dev->ipc_log, \
			"[I] %s: %s: " fmt, e_dev->label, __func__, \
			##__VA_ARGS__); \
	} while (0)
#define EDMA_ERR(e_dev, fmt, ...) do { \
	if (e_dev->klog_lvl <= LOG_LVL_ERROR) \
		dev_err(e_dev->dev, "[E] %s: %s: " fmt, e_dev->label, \
			__func__, ##__VA_ARGS__); \
	if (e_dev->ipc_log && e_dev->ipc_log_lvl <= LOG_LVL_ERROR) \
		ipc_log_string(e_dev->ipc_log, \
			"[E] %s: %s: " fmt, e_dev->label, __func__, \
			##__VA_ARGS__); \
	} while (0)

#define EDMA_ASSERT(cond, msg) do { \
	if (cond) \
		panic(msg); \
	} while (0)

/* edmac specific logging macros */
#define EDMAC_INFO(ec_dev, ev_ch, fmt, ...) do { \
	if (ec_dev->klog_lvl <= LOG_LVL_INFO) \
		pr_info("[I] %s: %s: %u: %u: %s: " fmt, ec_dev->label, \
			TO_EDMA_DIR_CH_STR(ec_dev->dir), ec_dev->ch_id, ev_ch, \
			__func__, ##__VA_ARGS__); \
	if (ec_dev->ipc_log && ec_dev->ipc_log_lvl <= LOG_LVL_INFO) \
		ipc_log_string(ec_dev->ipc_log, \
			       "[I] %s: EC_%u: EV_%u: %s: " fmt, \
				TO_EDMA_DIR_CH_STR(ec_dev->dir), \
				ec_dev->ch_id, ev_ch, __func__, \
				##__VA_ARGS__); \
	} while (0)
#define EDMAC_ERR(ec_dev, ev_ch, fmt, ...) do { \
	if (ec_dev->klog_lvl <= LOG_LVL_ERROR) \
		pr_err("[E] %s: %s: %u: %u: %s: " fmt, ec_dev->label, \
			TO_EDMA_DIR_CH_STR(ec_dev->dir), ec_dev->ch_id, ev_ch, \
			__func__, ##__VA_ARGS__); \
	if (ec_dev->ipc_log && ec_dev->ipc_log_lvl <= LOG_LVL_ERROR) \
		ipc_log_string(ec_dev->ipc_log, \
			       "[E] %s: EC_%u: EV_%u: %s: " fmt, \
				TO_EDMA_DIR_CH_STR(ec_dev->dir), \
				ec_dev->ch_id, ev_ch, __func__, \
				##__VA_ARGS__); \
	} while (0)

enum debug_log_lvl {
	LOG_LVL_VERBOSE,
	LOG_LVL_INFO,
	LOG_LVL_ERROR,
	LOG_LVL_MASK_ALL,
};

#define EDMA_DRV_NAME "edma"
#define DEFAULT_KLOG_LVL (LOG_LVL_ERROR)

static struct edma_dev *e_dev_info;

#ifdef CONFIG_QCOM_PCI_EDMA_DEBUG
#define DEFAULT_IPC_LOG_LVL (LOG_LVL_VERBOSE)
#define IPC_LOG_PAGES (40)
#define EDMA_IRQ(e_dev, ec_ch, fmt, ...) do { \
	if (e_dev->klog_lvl != LOG_LVL_MASK_ALL) \
		dev_dbg(e_dev->dev, "[IRQ] %s: EC_%u: %s: " fmt, e_dev->label, \
			ec_ch, __func__, ##__VA_ARGS__); \
	if (e_dev->ipc_log_irq && e_dev->ipc_log_lvl != LOG_LVL_MASK_ALL) \
		ipc_log_string(e_dev->ipc_log_irq, \
			"[IRQ] %s: EC_%u: %s: " fmt, e_dev->label, ec_ch, \
			__func__, ##__VA_ARGS__); \
	} while (0)
#define EDMAC_VERB(ec_dev, ev_ch, fmt, ...) do { \
	if (ec_dev->klog_lvl <= LOG_LVL_VERBOSE) \
		pr_info("[V] %s: %s: %u: %u: %s: " fmt, ec_dev->label, \
			TO_EDMA_DIR_CH_STR(ec_dev->dir), ec_dev->ch_id, ev_ch, \
			__func__, ##__VA_ARGS__); \
	if (ec_dev->ipc_log && ec_dev->ipc_log_lvl <= LOG_LVL_VERBOSE) \
		ipc_log_string(ec_dev->ipc_log, \
			       "[V] %s: EC_%u: EV_%u: %s: " fmt, \
				TO_EDMA_DIR_CH_STR(ec_dev->dir), \
				ec_dev->ch_id, ev_ch, __func__, \
				##__VA_ARGS__); \
	} while (0)

#else
#define IPC_LOG_PAGES (2)
#define DEFAULT_IPC_LOG_LVL (LOG_LVL_ERROR)
#define EDMA_IRQ(e_dev, ec_ch, fmt, ...)
#define EDMAC_REG(ec_dev, ev_ch, fmt, ...)
#define EDMAC_VERB(ec_dev, ev_ch, fmt, ...)
#endif

#define WR_CH_BASE (0x200)
#define RD_CH_BASE (0x300)
#define DMA_CH_BASE(d, n) ((d == EDMA_WR_CH ? WR_CH_BASE : RD_CH_BASE) +\
				(n * 0x200))

#define DMA_CH_CONTROL1_REG_DIR_CH_N(d, n) (DMA_CH_BASE(d, n))
#define DMA_LLP_LOW_OFF_DIR_CH_N(d, n) (DMA_CH_BASE(d, n) + 0x1c)
#define DMA_LLP_HIGH_OFF_DIR_CH_N(d, n) (DMA_CH_BASE(d, n) + 0x20)

#define DMA_WRITE_ENGINE_EN_OFF (0xc)
#define DMA_WRITE_DOORBELL_OFF (0x10)
#define DMA_WRITE_INT_STATUS_OFF (0x4c)
#define DMA_WRITE_INT_MASK_OFF (0x54)
#define DMA_WRITE_INT_CLEAR_OFF (0x58)
#define DMA_WRITE_LINKED_LIST_ERR_EN_OFF (0x90)

#define DMA_READ_ENGINE_EN_OFF (0x2c)
#define DMA_READ_DOORBELL_OFF (0x30)
#define DMA_READ_INT_STATUS_OFF (0xa0)
#define DMA_READ_INT_MASK_OFF (0xa8)
#define DMA_READ_INT_CLEAR_OFF (0xac)
#define DMA_READ_LINKED_LIST_ERR_EN_OFF (0xc4)

#define DMA_CTRL_OFF (0x8)
#define DMA_CTRL_NUM_CH_MASK (0xf)
#define DMA_CTRL_NUM_WR_CH_SHIFT (0)
#define DMA_CTRL_NUM_RD_CH_SHIFT (16)

#define EDMA_LABEL_SIZE (256)
#define EDMA_DESC_LIST_SIZE (256)
#define EDMA_NUM_MAX_EV_CH (32)
#define EDMA_NUM_TL_INIT (2)
#define EDMA_NUM_TL_ELE (1024)

#define EDMA_CH_CONTROL1_LLE BIT(9) /* Linked List Enable */
#define EDMA_CH_CONTROL1_CCS BIT(8) /* Consumer Cycle Status */
#define EDMA_CH_CONTROL1_LIE BIT(3) /* Local Interrupt Enable */
#define EDMA_CH_CONTROL1_LLP BIT(2) /* Load Link Pointer */
#define EDMA_CH_CONTROL1_CB BIT(0) /* Cycle bit */

#define EDMA_CH_CONTROL1_INIT (EDMA_CH_CONTROL1_LLE | EDMA_CH_CONTROL1_CCS)
#define EDMA_HW_STATUS_MASK (BIT(6) | BIT(5))
#define EDMA_HW_STATUS_SHIFT (5)
#define EDMA_INT_ERR_MASK (0xff00)
#define EDMA_MAX_SIZE 0x2000

#define REQ_OF_DMA_ARGS (2) /* # of arguments required from client */

/*
 * EDMAV CH ID = EDMA CH ID * EDMAV_BASE_CH_ID + EDMAV index
 *
 * Virtual channel is assigned a channel ID based on the physical
 * channel is it assigned to.
 * ex:
 *	physical channel 0: virtual base = 0
 *	physical channel 1: virtual base = 100
 *	physical channel 2: virtual base = 200
 */
#define EDMAV_BASE_CH_ID (100)
#define EDMAV_NO_CH_ID (99) /* RESERVED CH ID for no virtual channel */

enum edma_dir_ch {
	EDMA_WR_CH,
	EDMA_RD_CH,
	EDMA_DIR_CH_MAX,
};

static const char *const edma_dir_ch_str[EDMA_DIR_CH_MAX] = {
	[EDMA_WR_CH] = "WR",
	[EDMA_RD_CH] = "RD",
};

#define TO_EDMA_DIR_CH_STR(dir) (dir < EDMA_DIR_CH_MAX ? \
				edma_dir_ch_str[dir] : "INVALID")

enum edma_hw_state {
	EDMA_HW_STATE_INIT,
	EDMA_HW_STATE_ACTIVE,
	EDMA_HW_STATE_HALTED,
	EDMA_HW_STATE_STOPPED,
	EDMA_HW_STATE_MAX,
};

static const char *const edma_hw_state_str[EDMA_HW_STATE_MAX] = {
	[EDMA_HW_STATE_INIT] = "INIT",
	[EDMA_HW_STATE_ACTIVE] = "ACTIVE",
	[EDMA_HW_STATE_HALTED] = "HALTED",
	[EDMA_HW_STATE_STOPPED] = "STOPPED",
};

#define TO_EDMA_HW_STATE_STR(state) (state < EDMA_HW_STATE_MAX ? \
					edma_hw_state_str[state] : "INVALID")

/* transfer list element */
struct data_element {
	u32 ch_ctrl;
	u32 size;
	u64 sar;
	u64 dar;
};

/* transfer list last element */
struct link_element {
	u32 ch_ctrl;
	u32 reserved0;
	u64 lle_ptr; /* points to new transfer list */
	u32 reserved1;
	u32 reserved2;
};

union edma_element {
	struct data_element de;
	struct link_element le;
};

/* main structure for eDMA driver */
struct edma_dev {
	struct list_head node;
	struct dma_device dma_device;
	struct device_node *of_node;
	struct device *dev;
	struct edmac_dev *ec_wr_devs; /* array of wr channels */
	struct edmac_dev *ec_rd_devs; /* array of rd channels */
	struct edmav_dev *ev_devs; /* array of virtual channels */
	u32 n_max_ec_ch; /* max physical channels */
	u32 n_max_ev_ch; /* max virtual channels */
	u32 n_wr_ch;
	u32 n_rd_ch;
	u32 cur_wr_ch_idx; /* index for wr round robin allocation */
	u32 cur_rd_ch_idx; /* index for rd round robin allocation */
	u32 cur_ev_ch_idx; /* index for next available eDMA virtual chan */
	u32 n_tl_init; /* # of transfer list for each channel init */
	u32 n_tl_ele; /* (# of de + le) */
	phys_addr_t base_phys;
	size_t base_size;
	void __iomem *base;
	int irq;
	u32 edmac_mask; /* edma channel available for apps */
	char label[EDMA_LABEL_SIZE];
	void *ipc_log;
	void *ipc_log_irq;
	enum debug_log_lvl ipc_log_lvl;
	enum debug_log_lvl klog_lvl;
};

/* eDMA physical channels */
struct edmac_dev {
	struct list_head ev_list;
	struct edma_dev *e_dev;
	u32 n_ev;
	u32 ch_id;
	spinlock_t edma_lock;
	enum edma_dir_ch dir;
	dma_addr_t tl_dma_rd_p; /* address of next DE to process */
	struct data_element *tl_wr_p; /* next available DE in TL */
	struct data_element *tl_rd_p;
	struct link_element *le_p; /* link element of newest TL */
	struct edma_desc **dl_wr_p; /* next available desc in DL */
	struct edma_desc **dl_rd_p;
	struct edma_desc **ldl_p; /* last desc of newest desc list */
	u32 n_de_avail; /* # of available DE in current TL (excludes LE) */
	struct tasklet_struct proc_task; /* processing tasklet */
	char label[EDMA_LABEL_SIZE];
	void *ipc_log;
	enum debug_log_lvl ipc_log_lvl;
	enum debug_log_lvl klog_lvl;
	enum edma_hw_state hw_state;

	void __iomem *engine_en_reg;
	void __iomem *int_mask_reg;
	void __iomem *ll_err_en_reg;
	void __iomem *ch_ctrl1_reg;
	void __iomem *llp_low_reg;
	void __iomem *llp_high_reg;
	void __iomem *db_reg;
};

/* eDMA virtual channels */
struct edmav_dev {
	struct list_head node;
	struct edmac_dev *ec_dev;
	struct list_head dl;
	struct dma_chan dma_ch;
	enum edma_dir_ch dir;
	u32 ch_id;
	u32 priority;
	u32 n_de;
	u32 outstanding;
};

/* eDMA descriptor and last descriptor */
struct edma_desc {
	struct list_head node;
	struct edmav_dev *ev_dev;
	struct dma_async_tx_descriptor tx;
	struct data_element *de;

	/* Below are for LDESC (last desc of the list) */
	dma_addr_t tl_dma; /* start of this transfer list (physical) */
	union edma_element *tl; /* start of this transfer list (virtual) */
	struct edma_desc **dl; /* start of this desc list */
	dma_addr_t tl_dma_next; /* next transfer list (physical) */
	union edma_element *tl_next; /* next transfer list (virtual) */
	struct edma_desc **dl_next;/* next desc list */
};

static void edma_set_clear(void __iomem *addr, u32 set,
				u32 clear)
{
	u32 val;

	val = (readl_relaxed(addr) & ~clear) | set;
	writel_relaxed(val, addr);

	/* ensure register write goes through before next register operation */
	wmb();
}

static enum edma_hw_state edma_get_hw_state(struct edmac_dev *ec_dev)
{
	u32 val;

	val = readl_relaxed(ec_dev->ch_ctrl1_reg);
	val &= EDMA_HW_STATUS_MASK;
	val >>= EDMA_HW_STATUS_SHIFT;

	return val;
}

static struct edmav_dev *to_edmav_dev(struct dma_chan *dma_ch)
{
	return container_of(dma_ch, struct edmav_dev, dma_ch);
}

static void edmac_process_tasklet(unsigned long data)
{
	struct edmac_dev *ec_dev = (struct edmac_dev *)data;
	struct edma_dev *e_dev = ec_dev->e_dev;
	dma_addr_t llp_low, llp_high, llp;
	unsigned long flags;

	spin_lock_irqsave(&ec_dev->edma_lock, flags);
	EDMAC_VERB(ec_dev, EDMAV_NO_CH_ID, "enter\n");

	llp_low = readl_relaxed(ec_dev->llp_low_reg);
	llp_high = readl_relaxed(ec_dev->llp_high_reg);
	llp = (u64)(llp_high << 32) | llp_low;
	EDMAC_VERB(ec_dev, EDMAV_NO_CH_ID, "Start: DMA_LLP = %pad\n", &llp);

	while (ec_dev->tl_dma_rd_p != llp) {
		struct edma_desc *desc;

		/* current element is a link element. Need to jump and free */
		if (ec_dev->tl_rd_p->ch_ctrl & EDMA_CH_CONTROL1_LLP) {
			struct edma_desc *ldesc = *ec_dev->dl_rd_p;

			ec_dev->tl_dma_rd_p = ldesc->tl_dma_next;
			ec_dev->tl_rd_p = (struct data_element *)ldesc->tl_next;
			ec_dev->dl_rd_p = ldesc->dl_next;

			EDMAC_VERB(ec_dev, EDMAV_NO_CH_ID,
				"free transfer list: %pad\n", &ldesc->tl_dma);
			dma_free_coherent(e_dev->dev,
				sizeof(*ldesc->tl) * e_dev->n_tl_ele,
				ldesc->tl, ldesc->tl_dma);
			kfree(ldesc->dl);
			continue;
		}

		EDMAC_VERB(ec_dev, EDMAV_NO_CH_ID, "TL_DMA_RD_P: %pad\n",
			&ec_dev->tl_dma_rd_p);

		desc = *ec_dev->dl_rd_p;
		ec_dev->tl_dma_rd_p += sizeof(struct data_element);
		ec_dev->tl_rd_p++;
		ec_dev->dl_rd_p++;
		if (desc) {
			/*
			 * Clients might queue descriptors in the call back
			 * context. Release spinlock to avoid deadlock scenarios
			 * as we use same lock duing descriptor queuing.
			 */
			spin_unlock_irqrestore(&ec_dev->edma_lock, flags);
			dmaengine_desc_get_callback_invoke(&desc->tx, NULL);

			/* Acquire spinlock again to continue edma operations */
			spin_lock_irqsave(&ec_dev->edma_lock, flags);
			kfree(desc);
		} else {
			EDMAC_VERB(ec_dev, EDMAV_NO_CH_ID,
				"edma desc is NULL\n");
		}
	}

	edma_set_clear(ec_dev->int_mask_reg, 0, BIT(ec_dev->ch_id));
	EDMAC_VERB(ec_dev, EDMAV_NO_CH_ID, "exit\n");
	spin_unlock_irqrestore(&ec_dev->edma_lock, flags);
}

static irqreturn_t handle_edma_irq(int irq, void *data)
{
	struct edma_dev *e_dev = data;
	u32 wr_int_status;
	u32 rd_int_status;
	int i = 0;

	wr_int_status = readl_relaxed(e_dev->base + DMA_WRITE_INT_STATUS_OFF);
	rd_int_status = readl_relaxed(e_dev->base + DMA_READ_INT_STATUS_OFF);

	edma_set_clear(e_dev->base + DMA_WRITE_INT_CLEAR_OFF, wr_int_status, 0);
	edma_set_clear(e_dev->base + DMA_READ_INT_CLEAR_OFF, rd_int_status, 0);

	EDMA_IRQ(e_dev, EDMAV_NO_CH_ID,
		"IRQ wr status: 0x%x rd status: 0x%x\n",
		wr_int_status, rd_int_status);

	EDMA_ASSERT((wr_int_status & EDMA_INT_ERR_MASK) ||
		(rd_int_status & EDMA_INT_ERR_MASK),
		"Error reported by H/W\n");

	while (wr_int_status) {
		if (wr_int_status & 0x1) {
			struct edmac_dev *ec_dev = &e_dev->ec_wr_devs[i];

			edma_set_clear(ec_dev->int_mask_reg,
					BIT(ec_dev->ch_id), 0);
			tasklet_schedule(&ec_dev->proc_task);
		}
		wr_int_status >>= 1;
		i++;
	}

	i = 0;
	while (rd_int_status) {
		if (rd_int_status & 0x1) {
			struct edmac_dev *ec_dev = &e_dev->ec_rd_devs[i];

			edma_set_clear(ec_dev->int_mask_reg,
					BIT(ec_dev->ch_id), 0);
			tasklet_schedule(&ec_dev->proc_task);
		}
		rd_int_status >>= 1;
		i++;
	}

	return IRQ_HANDLED;
}

static struct dma_chan *edma_of_dma_xlate(struct of_phandle_args *args,
					 struct of_dma *of_dma)
{
	struct edma_dev *e_dev = (struct edma_dev *)of_dma->of_dma_data;
	struct edmac_dev *ec_dev;
	struct edmav_dev *ev_dev;
	u32 ch_id;

	if (args->args_count < REQ_OF_DMA_ARGS) {
		EDMA_ERR(e_dev,
			"EDMA requires atleast %d arguments, client passed:%d\n",
			REQ_OF_DMA_ARGS, args->args_count);
		return NULL;
	}

	if (e_dev->cur_ev_ch_idx >= e_dev->n_max_ev_ch) {
		EDMA_ERR(e_dev, "No more eDMA virtual channels available\n");
		return NULL;
	}

	ev_dev = &e_dev->ev_devs[e_dev->cur_ev_ch_idx++];

	ev_dev->dir = args->args[0] ? EDMA_RD_CH : EDMA_WR_CH;
	ev_dev->priority = args->args[1];

	/* use round robin to allocate eDMA channel */
	if (ev_dev->dir == EDMA_WR_CH) {
		ch_id = e_dev->cur_wr_ch_idx++ % e_dev->n_wr_ch;
		ec_dev = &e_dev->ec_wr_devs[ch_id];
	} else {
		ch_id = e_dev->cur_rd_ch_idx++ % e_dev->n_rd_ch;
		ec_dev = &e_dev->ec_rd_devs[ch_id];
	}

	ev_dev->ec_dev = ec_dev;
	ev_dev->ch_id = EDMAV_BASE_CH_ID * ec_dev->ch_id + ec_dev->n_ev;

	list_add_tail(&ev_dev->node, &ec_dev->ev_list);

	EDMA_LOG(e_dev, "EC_ID: %u EV_ID: %u direction: %s priority: %u",
		ec_dev->ch_id, ev_dev->ch_id, TO_EDMA_DIR_CH_STR(ev_dev->dir),
		ev_dev->priority);

	return dma_get_slave_channel(&ev_dev->dma_ch);
}

static int edma_alloc_transfer_list(struct edmac_dev *ec_dev)
{
	struct edma_dev *e_dev = ec_dev->e_dev;
	union edma_element *tl;
	struct edma_desc **dl;
	struct edma_desc *ldesc;
	dma_addr_t tl_dma;
	u32 n_tl_ele = e_dev->n_tl_ele;

	EDMAC_VERB(ec_dev, EDMAV_NO_CH_ID, "enter\n");

	tl = dma_zalloc_coherent(e_dev->dev, sizeof(*tl) * n_tl_ele,
				&tl_dma, GFP_ATOMIC);
	if (!tl)
		return -ENOMEM;

	dl = kcalloc(n_tl_ele, sizeof(*dl), GFP_ATOMIC);
	if (!dl)
		goto free_transfer_list;

	ldesc = kzalloc(sizeof(*ldesc), GFP_ATOMIC);
	if (!ldesc)
		goto free_descriptor_list;

	EDMAC_VERB(ec_dev, EDMAV_NO_CH_ID,
		"allocated transfer list dma: %pad\n", &tl_dma);

	dl[n_tl_ele - 1] = ldesc;

	if (ec_dev->tl_wr_p) {
		/* link current lists with new lists */
		ec_dev->le_p->lle_ptr = tl_dma;
		(*ec_dev->ldl_p)->tl_dma_next = tl_dma;
		(*ec_dev->ldl_p)->tl_next = tl;
		(*ec_dev->ldl_p)->dl_next = dl;
	} else {
		/* init read and write ptr if these are the initial lists */
		ec_dev->tl_dma_rd_p = tl_dma;
		ec_dev->tl_wr_p = ec_dev->tl_rd_p = (struct data_element *)tl;
		ec_dev->dl_wr_p = ec_dev->dl_rd_p = dl;
	}

	/* move ptr and compose LE and LDESC of new lists */
	ec_dev->le_p = (struct link_element *)&tl[n_tl_ele - 1];
	ec_dev->le_p->ch_ctrl = EDMA_CH_CONTROL1_LLP | EDMA_CH_CONTROL1_CB;

	/* setup ldesc */
	ec_dev->ldl_p = dl + n_tl_ele - 1;
	(*ec_dev->ldl_p)->tl_dma = tl_dma;
	(*ec_dev->ldl_p)->tl = tl;
	(*ec_dev->ldl_p)->dl = dl;

	EDMAC_VERB(ec_dev, EDMAV_NO_CH_ID, "exit\n");

	return 0;

free_descriptor_list:
	kfree(dl);
free_transfer_list:
	dma_free_coherent(e_dev->dev, sizeof(*tl) * n_tl_ele, tl, tl_dma);

	EDMAC_VERB(ec_dev, EDMAV_NO_CH_ID, "exit with error\n");

	return -ENOMEM;
}

static void edma_free_chan_resources(struct dma_chan *chan)
{
	struct edmav_dev *ev_dev = to_edmav_dev(chan);
	struct edmac_dev *ec_dev = ev_dev->ec_dev;
	struct edma_dev *e_dev = ec_dev->e_dev;
	struct edma_desc **ldesc;

	if (!ec_dev->n_ev)
		return;

	if (--ec_dev->n_ev)
		return;

	/* get ldesc of desc */
	ldesc = ec_dev->dl_wr_p + ec_dev->n_de_avail + 1;
	while (ldesc) {
		struct edma_desc *ldesc_t = *ldesc;

		/* move ldesc ptr to next list ldesc and free current lists */
		if (ldesc_t->dl_next)
			ldesc = ldesc_t->dl_next + e_dev->n_tl_ele - 1;
		else
			ldesc = NULL;

		if (ldesc_t->tl)
			dma_free_coherent(e_dev->dev,
				sizeof(*ldesc_t->tl) * e_dev->n_tl_ele,
				ldesc_t->tl, ldesc_t->tl_dma);
		kfree(ldesc_t->dl);
	}

	ec_dev->dl_wr_p = ec_dev->dl_rd_p = ec_dev->ldl_p = NULL;
	ec_dev->tl_wr_p = ec_dev->tl_rd_p = NULL;
	ec_dev->le_p = NULL;
	ec_dev->tl_dma_rd_p = 0;
}

static int edma_alloc_chan_resources(struct dma_chan *chan)
{
	struct edmav_dev *ev_dev = to_edmav_dev(chan);
	struct edmac_dev *ec_dev = ev_dev->ec_dev;
	struct edma_dev *e_dev = ec_dev->e_dev;
	int ret = 0;

	/*
	 * If this is the first client for this eDMA channel, setup the initial
	 * transfer and descriptor lists and configure the H/W for it.
	 */
	if (!ec_dev->n_ev) {
		int i;

		for (i = 0; i < e_dev->n_tl_init; i++) {
			ret = edma_alloc_transfer_list(ec_dev);
			if (ret)
				goto out;
		}

		writel_relaxed(true, ec_dev->engine_en_reg);
		writel_relaxed(0, ec_dev->int_mask_reg);
		writel_relaxed(true, ec_dev->ll_err_en_reg);
		writel_relaxed(EDMA_CH_CONTROL1_INIT, ec_dev->ch_ctrl1_reg);
		writel_relaxed(lower_32_bits(ec_dev->tl_dma_rd_p),
				ec_dev->llp_low_reg);
		writel_relaxed(upper_32_bits(ec_dev->tl_dma_rd_p),
				ec_dev->llp_high_reg);
		ec_dev->n_de_avail = e_dev->n_tl_ele - 1;
	}
	ec_dev->n_ev++;

	return 0;
out:
	edma_free_chan_resources(chan);
	return ret;
}

static inline void edma_compose_data_element(struct edmav_dev *ev_dev,
				struct data_element *de, dma_addr_t dst_addr,
				dma_addr_t src_addr, size_t size,
				unsigned long flags)
{
	EDMAC_VERB(ev_dev->ec_dev, ev_dev->ch_id,
		"size = %d, dst_addr: %pad\tsrc_addr: %pad\n",
		size, &dst_addr, &src_addr);

	if (flags & DMA_PREP_INTERRUPT)
		de->ch_ctrl |= EDMA_CH_CONTROL1_LIE;
	de->size = size;
	de->sar = src_addr;
	de->dar = dst_addr;
}

static struct edma_desc *edma_alloc_descriptor(struct edmav_dev *ev_dev)
{
	struct edma_desc *desc;

	desc = kzalloc(sizeof(*desc) + sizeof(*desc->de),
			GFP_ATOMIC);
	if (!desc)
		return NULL;

	desc->de = (struct data_element *)(&desc[1]);
	desc->ev_dev = ev_dev;
	ev_dev->n_de++;

	dma_async_tx_descriptor_init(&desc->tx, &ev_dev->dma_ch);

	return desc;
}

struct dma_async_tx_descriptor *edma_prep_dma_memcpy(struct dma_chan *chan,
						dma_addr_t dst, dma_addr_t src,
						size_t len, unsigned long flags)
{
	struct edmav_dev *ev_dev = to_edmav_dev(chan);
	struct edma_desc *desc;
	unsigned long l_flags;

	spin_lock_irqsave(&ev_dev->ec_dev->edma_lock, l_flags);
	EDMAC_VERB(ev_dev->ec_dev, ev_dev->ch_id, "enter\n");

	desc = edma_alloc_descriptor(ev_dev);
	if (!desc)
		goto err;

	edma_compose_data_element(ev_dev, desc->de, dst, src, len, flags);

	/* insert the descriptor to client descriptor list */
	list_add_tail(&desc->node, &ev_dev->dl);

	EDMAC_VERB(ev_dev->ec_dev, ev_dev->ch_id, "exit\n");
	spin_unlock_irqrestore(&ev_dev->ec_dev->edma_lock, l_flags);

	return &desc->tx;
err:
	EDMAC_VERB(ev_dev->ec_dev, ev_dev->ch_id,
		"edma alloc descriptor failed for channel:%d\n", ev_dev->ch_id);
	spin_unlock_irqrestore(&ev_dev->ec_dev->edma_lock, l_flags);
	return NULL;
}

static void edma_issue_descriptor(struct edmac_dev *ec_dev,
				struct edma_desc *desc)
{
	/* set descriptor for last Data Element */
	*ec_dev->dl_wr_p = desc;
	memcpy(ec_dev->tl_wr_p, desc->de, sizeof(*desc->de));

	/*
	 * Ensure Desc data element should be flushed to tl_wr_p
	 * before updating CB flag
	 */
	mb();

	EDMAC_VERB(ec_dev, EDMAV_NO_CH_ID,
			"size: %d, dst_addr: %pad\tsrc_addr: %pad\n",
			ec_dev->tl_wr_p->size, &ec_dev->tl_wr_p->dar,
			&ec_dev->tl_wr_p->sar);

	ec_dev->tl_wr_p->ch_ctrl |= EDMA_CH_CONTROL1_CB;

	/*
	 * Ensure that CB flag is properly flushed because
	 * HW starts processing the descriptor based on CB flag.
	 */
	mb();

	EDMAC_VERB(ec_dev, EDMAV_NO_CH_ID, "ch_ctrl = %d\n",
		ec_dev->tl_wr_p->ch_ctrl);

	ec_dev->dl_wr_p++;
	ec_dev->tl_wr_p++;
	ec_dev->n_de_avail--;

	/* dl_wr_p points to ldesc and tl_wr_p points link element */
	if (!ec_dev->n_de_avail) {
		int ret;

		ec_dev->tl_wr_p = (struct data_element *)
			(*ec_dev->dl_wr_p)->tl_next;
		ec_dev->dl_wr_p = (*ec_dev->dl_wr_p)->dl_next;
		ec_dev->n_de_avail = ec_dev->e_dev->n_tl_ele - 1;

		ret = edma_alloc_transfer_list(ec_dev);
		EDMA_ASSERT(ret, "failed to allocate new transfer list\n");
	}
}

static void edma_issue_pending(struct dma_chan *chan)
{
	struct edmav_dev *ev_dev = to_edmav_dev(chan);
	struct edmac_dev *ec_dev = ev_dev->ec_dev;
	struct edma_desc *desc;
	enum edma_hw_state hw_state;
	unsigned long flags;

	spin_lock_irqsave(&ec_dev->edma_lock, flags);
	EDMAC_VERB(ec_dev, ev_dev->ch_id, "enter\n");

	if (unlikely(list_empty(&ev_dev->dl))) {
		EDMAC_VERB(ec_dev, ev_dev->ch_id, "No descriptor to issue\n");
		spin_unlock_irqrestore(&ec_dev->edma_lock, flags);
		return;
	}

	list_for_each_entry(desc, &ev_dev->dl, node)
		edma_issue_descriptor(ec_dev, desc);

	list_del_init(&ev_dev->dl);

	hw_state = edma_get_hw_state(ec_dev);
	if ((hw_state == EDMA_HW_STATE_STOPPED) ||
		(hw_state == EDMA_HW_STATE_INIT)) {
		/* Disable Engine and enable it back */
		writel_relaxed(false, ec_dev->engine_en_reg);
		writel_relaxed(true, ec_dev->engine_en_reg);
		EDMAC_VERB(ec_dev, ev_dev->ch_id,
			"Channel stopped: Disable Engine and enable back\n");
		/* Ensure that engine is restarted */
		mb();

		/*
		 * From spec, when channel is stopped,
		 * require to write llp reg to start edma transaction
		 */
		writel_relaxed(
			readl_relaxed(ec_dev->llp_low_reg),
			ec_dev->llp_low_reg);
		writel_relaxed(
			readl_relaxed(ec_dev->llp_high_reg),
			ec_dev->llp_high_reg);
		/* Ensure LLP registers are properly updated */
		mb();

		/*
		 * As Channel is stopped, to start edma transaction,
		 * ring channel doorbell
		 */
		EDMAC_VERB(ec_dev, EDMAV_NO_CH_ID, "ringing doorbell\n");
		writel_relaxed(ec_dev->ch_id, ec_dev->db_reg);
		ec_dev->hw_state = EDMA_HW_STATE_ACTIVE;
		/* Ensure DB register is properly updated */
		mb();
	} else {
		EDMAC_VERB(ec_dev, ev_dev->ch_id, "EDMA Channel is Active\n");
	}

	EDMAC_VERB(ec_dev, ev_dev->ch_id, "exit\n");
	spin_unlock_irqrestore(&ec_dev->edma_lock, flags);
}

static int edma_config(struct dma_chan *chan, struct dma_slave_config *config)
{
	return -EINVAL;
}

static int edma_terminate_all(struct dma_chan *chan)
{
	return -EINVAL;
}

static int edma_pause(struct dma_chan *chan)
{
	return -EINVAL;
}

static int edma_resume(struct dma_chan *chan)
{
	return -EINVAL;
}

static int edma_init_irq(struct edma_dev *e_dev)
{
	int ret;

	ret = of_irq_get_byname(e_dev->of_node, "pci-edma-int");
	if (ret <= 0) {
		EDMA_ERR(e_dev, "failed to get IRQ from DT. ret: %d\n", ret);
		return ret;
	}

	e_dev->irq = ret;
	EDMA_LOG(e_dev, "received eDMA irq %d", e_dev->irq);

	ret = devm_request_irq(e_dev->dev, e_dev->irq,
			       handle_edma_irq, IRQF_TRIGGER_HIGH,
			       e_dev->label, e_dev);
	if (ret < 0) {
		EDMA_ERR(e_dev, "failed to request irq: %d ret: %d\n",
			e_dev->irq, ret);
		return ret;
	}

	return 0;
}

static void edma_init_log(struct edma_dev *e_dev, struct edmac_dev *ec_dev)
{
	if (!ec_dev) {
		snprintf(e_dev->label, EDMA_LABEL_SIZE, "%s_%llx",
			EDMA_DRV_NAME, (u64)e_dev->base_phys);

		e_dev->ipc_log_lvl = DEFAULT_IPC_LOG_LVL;
		e_dev->klog_lvl = DEFAULT_KLOG_LVL;
		e_dev->ipc_log = ipc_log_context_create(IPC_LOG_PAGES,
							e_dev->label, 0);
		e_dev->ipc_log_irq = ipc_log_context_create(IPC_LOG_PAGES,
							e_dev->label, 0);
	} else {
		snprintf(ec_dev->label, EDMA_LABEL_SIZE, "%s_%llx_%s_ch_%u",
			EDMA_DRV_NAME, (u64)e_dev->base_phys,
			(ec_dev->dir == EDMA_WR_CH ? "wr" : "rd"),
			ec_dev->ch_id);

		ec_dev->ipc_log_lvl = DEFAULT_IPC_LOG_LVL;
		ec_dev->klog_lvl = DEFAULT_KLOG_LVL;
		ec_dev->ipc_log = ipc_log_context_create(IPC_LOG_PAGES,
							ec_dev->label, 0);
	}
}

static void edma_init_channels(struct edma_dev *e_dev)
{
	int i;

	/* setup physical channels */
	for (i = 0; i < e_dev->n_max_ec_ch; i++) {
		struct edmac_dev *ec_dev;
		enum edma_dir_ch dir;
		u32 ch_id;
		u32 engine_en_off, int_mask_off, ll_err_en_off;
		u32 ch_ctrl1_off, llp_low_off, llp_high_off, db_off;

		if (i < e_dev->n_wr_ch) {
			ch_id = i;
			ec_dev = &e_dev->ec_wr_devs[ch_id];
			dir = EDMA_WR_CH;
			engine_en_off = DMA_WRITE_ENGINE_EN_OFF;
			int_mask_off = DMA_WRITE_INT_MASK_OFF;
			ll_err_en_off = DMA_WRITE_LINKED_LIST_ERR_EN_OFF;
			ch_ctrl1_off = DMA_CH_CONTROL1_REG_DIR_CH_N(dir, ch_id);
			llp_low_off = DMA_LLP_LOW_OFF_DIR_CH_N(dir, ch_id);
			llp_high_off = DMA_LLP_HIGH_OFF_DIR_CH_N(dir, ch_id);
			db_off = DMA_WRITE_DOORBELL_OFF;
		} else {
			ch_id = i - e_dev->n_wr_ch;
			ec_dev = &e_dev->ec_rd_devs[ch_id];
			dir = EDMA_RD_CH;
			engine_en_off = DMA_READ_ENGINE_EN_OFF;
			int_mask_off = DMA_READ_INT_MASK_OFF;
			ll_err_en_off = DMA_READ_LINKED_LIST_ERR_EN_OFF;
			ch_ctrl1_off = DMA_CH_CONTROL1_REG_DIR_CH_N(dir, ch_id);
			llp_low_off = DMA_LLP_LOW_OFF_DIR_CH_N(dir, ch_id);
			llp_high_off = DMA_LLP_HIGH_OFF_DIR_CH_N(dir, ch_id);
			db_off = DMA_READ_DOORBELL_OFF;
		}

		ec_dev->e_dev = e_dev;
		ec_dev->ch_id = ch_id;
		ec_dev->dir = dir;
		ec_dev->hw_state = EDMA_HW_STATE_INIT;

		edma_init_log(e_dev, ec_dev);

		INIT_LIST_HEAD(&ec_dev->ev_list);
		tasklet_init(&ec_dev->proc_task, edmac_process_tasklet,
			     (unsigned long)ec_dev);

		EDMA_LOG(e_dev, "EC_DIR: %s EC_INDEX: %d EC_ADDR: 0x%pK\n",
			TO_EDMA_DIR_CH_STR(ec_dev->dir), ec_dev->ch_id, ec_dev);

		ec_dev->engine_en_reg = e_dev->base + engine_en_off;
		ec_dev->int_mask_reg = e_dev->base + int_mask_off;
		ec_dev->ll_err_en_reg = e_dev->base + ll_err_en_off;
		ec_dev->ch_ctrl1_reg = e_dev->base + ch_ctrl1_off;
		ec_dev->llp_low_reg = e_dev->base + llp_low_off;
		ec_dev->llp_high_reg = e_dev->base + llp_high_off;
		ec_dev->db_reg = e_dev->base + db_off;
		spin_lock_init(&ec_dev->edma_lock);
	}

	/* setup virtual channels */
	for (i = 0; i < e_dev->n_max_ev_ch; i++) {
		struct edmav_dev *ev_dev = &e_dev->ev_devs[i];

		dma_cookie_init(&ev_dev->dma_ch);
		INIT_LIST_HEAD(&ev_dev->dl);
		ev_dev->dma_ch.device = &e_dev->dma_device;

		list_add_tail(&ev_dev->dma_ch.device_node,
				&e_dev->dma_device.channels);

		EDMA_LOG(e_dev, "EV_INDEX: %d EV_ADDR: 0x%pK\n", i, ev_dev);
	}
}

static void edma_init_dma_device(struct edma_dev *e_dev)
{
	/* clear and set capabilities */
	dma_cap_zero(e_dev->dma_device.cap_mask);
	dma_cap_set(DMA_MEMCPY, e_dev->dma_device.cap_mask);

	e_dev->dma_device.dev = e_dev->dev;
	e_dev->dma_device.device_config = edma_config;
	e_dev->dma_device.device_pause = edma_pause;
	e_dev->dma_device.device_resume = edma_resume;
	e_dev->dma_device.device_terminate_all = edma_terminate_all;
	e_dev->dma_device.device_alloc_chan_resources =
		edma_alloc_chan_resources;
	e_dev->dma_device.device_free_chan_resources =
		edma_free_chan_resources;
	e_dev->dma_device.device_prep_dma_memcpy = edma_prep_dma_memcpy;
	e_dev->dma_device.device_issue_pending = edma_issue_pending;
	e_dev->dma_device.device_tx_status = dma_cookie_status;
}

void edma_dump(void)
{
	int i;

	for (i = 0; i < EDMA_MAX_SIZE; i += 32) {
		pr_err("EDMA Reg : 0x%04x %08x %08x %08x %08x %08x %08x %08x %08x\n",
			i,
			readl_relaxed(e_dev_info->base + i),
			readl_relaxed(e_dev_info->base + (i + 4)),
			readl_relaxed(e_dev_info->base + (i + 8)),
			readl_relaxed(e_dev_info->base + (i + 12)),
			readl_relaxed(e_dev_info->base + (i + 16)),
			readl_relaxed(e_dev_info->base + (i + 20)),
			readl_relaxed(e_dev_info->base + (i + 24)),
			readl_relaxed(e_dev_info->base + (i + 28)));
	}
}
EXPORT_SYMBOL(edma_dump);

/*
 * Initializes and enables eDMA driver and H/W block for PCIe controllers.
 * Only call this function if PCIe supports eDMA and has all its resources
 * turned on.
 */
int qcom_edma_init(struct device *dev)
{
	int ret;
	struct edma_dev *e_dev;
	struct device_node *of_node;
	const __be32 *prop_val;

	if (!dev || !dev->of_node) {
		pr_err("EDMA: invalid %s\n", dev ? "of_node" : "dev");
		return -EINVAL;
	}

	of_node = of_parse_phandle(dev->of_node, "edma-parent", 0);
	if (!of_node) {
		pr_info("EDMA: no phandle for eDMA found\n");
		return -ENODEV;
	}

	if (!of_device_is_compatible(of_node, "qcom,pci-edma")) {
		pr_info("EDMA: no compatible qcom,pci-edma found\n");
		return -ENODEV;
	}

	e_dev = devm_kzalloc(dev, sizeof(*e_dev), GFP_KERNEL);
	if (!e_dev)
		return -ENOMEM;

	e_dev->dev = dev;
	e_dev->of_node = of_node;

	prop_val = of_get_address(e_dev->of_node, 0, NULL, NULL);
	if (!prop_val) {
		pr_err("EDMA: missing 'reg' devicetree\n");
		return -EINVAL;
	}

	e_dev->base_phys = be32_to_cpup(prop_val);
	if (!e_dev->base_phys) {
		pr_err("EDMA: failed to get eDMA base register address\n");
		return -EINVAL;
	}

	e_dev->base_size = be32_to_cpup(&prop_val[1]);
	if (!e_dev->base_size) {
		pr_err("EDMA: failed to get the size of eDMA register space\n");
		return -EINVAL;
	}

	e_dev->base = devm_ioremap_nocache(e_dev->dev, e_dev->base_phys,
					e_dev->base_size);
	if (!e_dev->base) {
		pr_err("EDMA: failed to remap eDMA base register\n");
		return -EFAULT;
	}

	edma_init_log(e_dev, NULL);

	e_dev->n_wr_ch = (readl_relaxed(e_dev->base + DMA_CTRL_OFF) >>
			DMA_CTRL_NUM_WR_CH_SHIFT) & DMA_CTRL_NUM_CH_MASK;
	EDMA_LOG(e_dev, "number of write channels: %d\n",
		e_dev->n_wr_ch);

	e_dev->n_rd_ch = (readl_relaxed(e_dev->base + DMA_CTRL_OFF) >>
			DMA_CTRL_NUM_RD_CH_SHIFT) & DMA_CTRL_NUM_CH_MASK;
	EDMA_LOG(e_dev, "number of read channels: %d\n",
		e_dev->n_rd_ch);

	e_dev->n_max_ec_ch = e_dev->n_wr_ch + e_dev->n_rd_ch;
	EDMA_LOG(e_dev, "number of eDMA physical channels: %d\n",
		e_dev->n_max_ec_ch);

	ret = of_property_read_u32(e_dev->of_node, "qcom,n-max-ev-ch",
				&e_dev->n_max_ev_ch);
	if (ret)
		e_dev->n_max_ev_ch = EDMA_NUM_MAX_EV_CH;

	EDMA_LOG(e_dev, "number of eDMA virtual channels: %d\n",
		e_dev->n_max_ev_ch);

	ret = of_property_read_u32(e_dev->of_node, "qcom,n-tl-init",
				&e_dev->n_tl_init);
	if (ret)
		e_dev->n_tl_init = EDMA_NUM_TL_INIT;

	EDMA_LOG(e_dev,
		"number of initial transfer and descriptor lists: %d\n",
		e_dev->n_tl_init);

	ret = of_property_read_u32(e_dev->of_node, "qcom,n-tl-ele",
				&e_dev->n_tl_ele);
	if (ret)
		e_dev->n_tl_ele = EDMA_NUM_TL_ELE;

	EDMA_LOG(e_dev,
		"number of elements for transfer and descriptor list: %d\n",
		e_dev->n_tl_ele);

	e_dev->ec_wr_devs = devm_kcalloc(e_dev->dev, e_dev->n_wr_ch,
				sizeof(*e_dev->ec_wr_devs), GFP_KERNEL);
	if (!e_dev->ec_wr_devs)
		return -ENOMEM;

	e_dev->ec_rd_devs = devm_kcalloc(e_dev->dev, e_dev->n_rd_ch,
				sizeof(*e_dev->ec_rd_devs), GFP_KERNEL);
	if (!e_dev->ec_rd_devs)
		return -ENOMEM;

	e_dev->ev_devs = devm_kcalloc(e_dev->dev, e_dev->n_max_ev_ch,
				sizeof(*e_dev->ev_devs), GFP_KERNEL);
	if (!e_dev->ev_devs)
		return -ENOMEM;

	INIT_LIST_HEAD(&e_dev->dma_device.channels);
	edma_init_channels(e_dev);

	ret = edma_init_irq(e_dev);
	if (ret)
		return ret;

	edma_init_dma_device(e_dev);
	e_dev_info = e_dev;

	ret = dma_async_device_register(&e_dev->dma_device);
	if (ret) {
		EDMA_ERR(e_dev, "failed to register device: %d\n", ret);
		return ret;
	}

	ret = of_dma_controller_register(e_dev->of_node, edma_of_dma_xlate,
					e_dev);
	if (ret) {
		EDMA_ERR(e_dev, "failed to register controller %d\n", ret);
		dma_async_device_unregister(&e_dev->dma_device);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(qcom_edma_init);

MODULE_DESCRIPTION("QTI PCIe eDMA driver");
MODULE_LICENSE("GPL v2");
