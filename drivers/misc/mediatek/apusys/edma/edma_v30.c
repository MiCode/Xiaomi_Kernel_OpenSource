// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/sched/clock.h>

#include "edma_driver.h"
#include "edma_cmd_hnd.h"
#include "edma3_reg.h"

#include "apusys_power.h"
#include "edma_dbgfs.h"
#include "edma_plat_internal.h"

#define NO_INTERRUPT		0
#define EDMA_POWEROFF_TIME_DEFAULT 2000


void printV30_error_status(struct edma_sub *edma_sub,
				struct edma_request *req)
{
	u32 status, i, j;
	unsigned int *ext_reg = NULL;
	u32 portID = edma_sub->dbg_portID;

	status = edma_read_reg32(edma_sub->base_addr, APU_EDMA3_ERR_STATUS);
	pr_notice("%s error status %x\n", edma_sub->sub_name,
		status);

	for (i = 0; i < (EDMA30_REG_SHOW_RANGE >> 2); i++) {
		status = edma_read_reg32(edma_sub->base_addr, i*4);
		pr_notice("edma error dump register[0x%x] = 0x%x\n",
		i*4, status);
	}

	pr_notice("---------------- dump port [%d] registers ----------------\n"
		, portID);

	for (i = ((0x800+portID*0x100)/4); i < ((0x900+portID*0x100)/4); i++) {
		status = edma_read_reg32(edma_sub->base_addr, i*4);
		pr_notice("edma error dump register[0x%x] = 0x%x\n",
		i*4, status);
	}

	if (req->ext_reg_addr != 0) {
		ext_reg = (unsigned int *)
			apusys_mem_query_kva(req->ext_reg_addr);

		pr_notice("kva ext_reg =  0x%p, req->ext_count = %d\n",
			ext_reg, req->ext_count);

		if (ext_reg !=  0)
			for (i = 0; i < req->ext_count; i++) {
				for (j = 0; j < EDMA_EXT_MODE_SIZE/4; j++)
					pr_notice("descriptor%d [0x%x] = 0x%x\n",
						i, j*4, *(ext_reg + j));
			}
		else
			pr_notice("not support ext_reg dump!!\n");
	}
}

irqreturn_t edmaV30_isr_handler(int irq, void *edma_sub_info)
{
	struct edma_sub *edma_sub = (struct edma_sub *)edma_sub_info;
	u32 status;
	u32 portID = edma_sub->dbg_portID;


	status = edma_read_reg32(edma_sub->base_addr, APU_EDMA3_DONE_STATUS);
	//pr_notice("%s in, done status = 0x%x!!\r\n", __func__, status);

	//status = edma_read_reg32(edma_sub->base_addr, 0x814+portID*0x100);

	//pr_notice("status portID[%d][0x%x]  = 0x%x!!\r\n", portID, 0x814+portID*0x100, status);

	//status = edma_read_reg32(edma_sub->base_addr, APU_EDMA3_ERR_STATUS);

	//pr_notice("APU_EDMA3_ERR_STATUS = 0x%x ", status);

	/* if ((*(uint32_t*)((uint8_t*)edma_sub->base_addr + 0x01c)) & 0x1) { */
	/* check port 5 low channel ==> lo5 = 0x20 */
	/*  {hi7, hi6, hi5, hi4, hi3, hi2, hi1, hi0, lo7, lo6, lo5, lo4, lo3, lo2, lo1, lo0} */
	if (edma_read_reg32(edma_sub->base_addr, APU_EDMA3_DONE_STATUS) & (0x1 << portID)) {
		edma_sub->is_cmd_done = true;
		wake_up_interruptible(&edma_sub->cmd_wait);
	} else if (edma_read_reg32(edma_sub->base_addr, APU_EDMA3_ERR_STATUS) != 0) {
		status = edma_read_reg32(edma_sub->base_addr, APU_EDMA3_ERR_STATUS);
		pr_notice("APU_EDMA3_ERR_STATUS = 0x%x, clear it... ", status);
		edma_write_reg32(edma_sub->base_addr, APU_EDMA3_ERR_STATUS, status);
		edma_sub->is_cmd_done = true;
		wake_up_interruptible(&edma_sub->cmd_wait);
	} else {
		pr_notice("EDMA3_DONE_STATUS not done & not error...");
		edma_sub->is_cmd_done = true;
		wake_up_interruptible(&edma_sub->cmd_wait);
	}

	/* *(uint32_t*)((uint8_t*)edma_sub->base_addr + 0x814) |= 0x1; */
	edma_set_reg32(edma_sub->base_addr, 0x814+portID*0x100, 0x1);

	return IRQ_HANDLED;

}

static void edmaV30_sw_reset(struct edma_sub *edma_sub)
{
	unsigned long flags;

	//pr_notice("%s: new init for edma 3.0\n", __func__);
	spin_lock_irqsave(&edma_sub->reg_lock, flags);

	edma_set_reg32(edma_sub->base_addr, 0x004, (0x1 << 0));
	udelay(5);
	edma_set_reg32(edma_sub->base_addr, 0x004, (0x1 << 4));
	udelay(5);
	edma_clear_reg32(edma_sub->base_addr, 0x004, (0x1 << 4));
	spin_unlock_irqrestore(&edma_sub->reg_lock, flags);
	//LOG_DBG("%s edma 3.0 skip sw reset\n", __func__);
}

/**
 * @brief trigger edma external mode
 *  parameters:
 *    base_addr: each edma base address
 *    ext_addr:  external descriptor mem addr
 *    num_desp: number of desc.
 *    desp_iommu_en: enable iommu or not
 */
void edmaV30_trigger_external(struct edma_sub *edma_sub, u32 ext_addr, u32 num_desp,
					u8 desp_iommu_en)
{
	unsigned long flags;
	u32 portID = edma_sub->dbg_portID;

	pr_notice("%s:port id == %d\n", __func__, portID);

	spin_lock_irqsave(&edma_sub->reg_lock, flags);

	if (desp_iommu_en)
		edma_set_reg32(edma_sub->base_addr, 0x004, ((0x2 << 30) | (0x2 << 28)));

	udelay(5);
	edma_write_reg32(edma_sub->base_addr, 0x800+portID*0x100, ext_addr);

	/* [28] irq_en, [20] vc_sw_rst, [19:0] num */
	edma_write_reg32(edma_sub->base_addr, 0x804+portID*0x100,
			 ((0x1 << 28) | ((num_desp << 0) & 0xfffff)));

	/* [16] stop, [0] fire */
	edma_write_reg32(edma_sub->base_addr, 0x808+portID*0x100, (0x1 << 0));
	spin_unlock_irqrestore(&edma_sub->reg_lock, flags);

}


/**
 * @brief excute request by each edma_sub
 *  parameters:
 *    edma_sub: include each edma base address
 */
int edma_exe_v30(struct edma_sub *edma_sub, struct edma_request *req)
{
	int ret = 0;
	void __iomem *base_addr;


	base_addr = edma_sub->base_addr;
	//edma_enable_sequence(edma_sub);

	edmaV30_sw_reset(edma_sub); // no need in edma 3.0
	edmaV30_trigger_external(edma_sub,
				req->ext_reg_addr,
				req->ext_count,
				req->desp_iommu_en);

	return ret;

}


