/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/rbtree.h>
#include <linux/genalloc.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <mach/ocmem_priv.h>

#define RDM_MAX_ENTRIES 32
#define RDM_MAX_CLIENTS 2

/* Data Mover Parameters */
#define DM_BLOCK_128 0x0
#define DM_BLOCK_256 0x1
#define DM_BR_ID_LPASS 0x0
#define DM_BR_ID_GPS 0x1

#define DM_INTR_CLR (0x8)
#define DM_INTR_MASK (0xC)
#define DM_GEN_STATUS (0x10)
#define DM_STATUS (0x14)
#define DM_CTRL (0x1000)
#define DM_TBL_BASE (0x1010)
#define DM_TBL_IDX(x) ((x) * 0x18)
#define DM_TBL_n(x) (DM_TBL_BASE + (DM_TBL_IDX(x)))
#define DM_TBL_n_offset(x) DM_TBL_n(x)
#define DM_TBL_n_size(x) (DM_TBL_n(x)+0x4)
#define DM_TBL_n_paddr(x) (DM_TBL_n(x)+0x8)
#define DM_TBL_n_ctrl(x) (DM_TBL_n(x)+0x10)

#define BR_CTRL (0x0)
#define BR_CLIENT_BASE (0x4)
#define BR_CLIENT_n_IDX(x) ((x) * 0x4)
#define BR_CLIENT_n_ctrl(x) (BR_CLIENT_BASE + (BR_CLIENT_n_IDX(x)))
#define BR_STATUS (0x14)
/* 16 entries per client are supported */
/* Use entries 0 - 15 for client0 */
#define BR_CLIENT0_MASK	(0x1000)
/* Use entries 16- 31 for client1 */
#define BR_CLIENT1_MASK	(0x2010)

#define BR_TBL_BASE (0x40)
#define BR_TBL_IDX(x) ((x) * 0x18)
#define BR_TBL_n(x) (BR_TBL_BASE + (BR_TBL_IDX(x)))
#define BR_TBL_n_offset(x) BR_TBL_n(x)
#define BR_TBL_n_size(x) (BR_TBL_n(x)+0x4)
#define BR_TBL_n_paddr(x) (BR_TBL_n(x)+0x8)
#define BR_TBL_n_ctrl(x) (BR_TBL_n(x)+0x10)

/* Constants and Shifts */
#define BR_TBL_ENTRY_ENABLE 0x1
#define BR_TBL_START 0x0
#define BR_TBL_END 0x8
#define BR_RW_SHIFT 0x2

#define DM_TBL_START 0x10
#define DM_TBL_END 0x18
#define DM_CLIENT_SHIFT 0x8
#define DM_BR_ID_SHIFT 0x4
#define DM_BR_BLK_SHIFT 0x1
#define DM_DIR_SHIFT 0x0

#define DM_DONE 0x1
#define DM_INTR_ENABLE 0x0
#define DM_INTR_DISABLE 0x1

static void *br_base;
static void *dm_base;

static atomic_t dm_pending;
static wait_queue_head_t dm_wq;
/* Shadow tables for debug purposes */
struct ocmem_br_table {
	unsigned int offset;
	unsigned int size;
	unsigned int ddr_low;
	unsigned int ddr_high;
	unsigned int ctrl;
} br_table[RDM_MAX_ENTRIES];

/* DM Table replicates an entire BR table */
/* Note: There are more than 1 BRs in the system */
struct ocmem_dm_table {
	unsigned int offset;
	unsigned int size;
	unsigned int ddr_low;
	unsigned int ddr_high;
	unsigned int ctrl;
} dm_table[RDM_MAX_ENTRIES];

/* Wrapper that will shadow these values later */
static int ocmem_read(void *at)
{
	return readl_relaxed(at);
}

/* Wrapper that will shadow these values later */
static int ocmem_write(unsigned long val, void *at)
{
	writel_relaxed(val, at);
	return 0;
}

static inline int client_ctrl_id(int id)
{
	return (id == OCMEM_SENSORS) ? 1 : 0;
}

static inline int client_slot_start(int id)
{

	return client_ctrl_id(id) * 16;
}

static irqreturn_t ocmem_dm_irq_handler(int irq, void *dev_id)
{
	atomic_set(&dm_pending, 0);
	ocmem_write(DM_INTR_DISABLE, dm_base + DM_INTR_CLR);
	wake_up_interruptible(&dm_wq);
	return IRQ_HANDLED;
}

/* Lock during transfers */
int ocmem_rdm_transfer(int id, struct ocmem_map_list *clist,
			unsigned long start, int direction)
{
	int num_chunks = clist->num_chunks;
	int slot = client_slot_start(id);
	int table_start = 0;
	int table_end = 0;
	int br_ctrl = 0;
	int br_id = 0;
	int dm_ctrl = 0;
	int i = 0;
	int j = 0;
	int status = 0;

	for (i = 0, j = slot; i < num_chunks; i++, j++) {

		struct ocmem_chunk *chunk = &clist->chunks[i];
		int sz = chunk->size;
		int paddr = chunk->ddr_paddr;
		int tbl_n_ctrl = 0;

		tbl_n_ctrl |= BR_TBL_ENTRY_ENABLE;
		if (chunk->ro)
			tbl_n_ctrl |= (1 << BR_RW_SHIFT);

		/* Table Entry n of BR and DM */
		ocmem_write(start, br_base + BR_TBL_n_offset(j));
		ocmem_write(sz, br_base + BR_TBL_n_size(j));
		ocmem_write(paddr, br_base + BR_TBL_n_paddr(j));
		ocmem_write(tbl_n_ctrl, br_base + BR_TBL_n_ctrl(j));

		ocmem_write(start, dm_base + DM_TBL_n_offset(j));
		ocmem_write(sz, dm_base + DM_TBL_n_size(j));
		ocmem_write(paddr, dm_base + DM_TBL_n_paddr(j));
		ocmem_write(tbl_n_ctrl, dm_base + DM_TBL_n_ctrl(j));

		start += sz;
	}

	br_id = client_ctrl_id(id);
	table_start = slot;
	table_end = slot + num_chunks - 1;
	br_ctrl |= (table_start << BR_TBL_START);
	br_ctrl |= (table_end << BR_TBL_END);

	ocmem_write(br_ctrl, (br_base + BR_CLIENT_n_ctrl(br_id)));
	/* Enable BR */
	ocmem_write(0x1, br_base + BR_CTRL);

	/* Compute DM Control Value */
	dm_ctrl |= (table_start << DM_TBL_START);
	dm_ctrl |= (table_end << DM_TBL_END);

	dm_ctrl |= (DM_BR_ID_LPASS << DM_BR_ID_SHIFT);
	dm_ctrl |= (DM_BLOCK_256 << DM_BR_BLK_SHIFT);
	dm_ctrl |= (direction << DM_DIR_SHIFT);

	status = ocmem_read(dm_base + DM_STATUS);
	pr_debug("Transfer status before %x\n", status);
	atomic_set(&dm_pending, 1);
	/* Trigger DM */
	ocmem_write(dm_ctrl, dm_base + DM_CTRL);
	pr_debug("ocmem: rdm: dm_ctrl %x br_ctrl %x\n", dm_ctrl, br_ctrl);

	wait_event_interruptible(dm_wq,
		atomic_read(&dm_pending) == 0);

	return 0;
}

int ocmem_rdm_init(struct platform_device *pdev)
{

	struct ocmem_plat_data *pdata = NULL;
	int rc = 0;

	pdata = platform_get_drvdata(pdev);

	br_base = pdata->br_base;
	dm_base = pdata->dm_base;

	rc = devm_request_irq(&pdev->dev, pdata->dm_irq, ocmem_dm_irq_handler,
				IRQF_TRIGGER_RISING, "ocmem_dm_irq", pdata);

	if (rc) {
		dev_err(&pdev->dev, "Failed to request dm irq");
		return -EINVAL;
	}

	init_waitqueue_head(&dm_wq);
	/* enable dm interrupts */
	ocmem_write(DM_INTR_ENABLE, dm_base + DM_INTR_MASK);
	return 0;
}
