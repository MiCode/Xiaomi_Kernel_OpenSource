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
#include "edma_reg.h"

#include "apusys_power.h"
#include "edma_dbgfs.h"
#include "edma_plat_internal.h"
#include "apusys_dbg.h"

#define NO_INTERRUPT		0
#define EDMA_POWEROFF_TIME_DEFAULT 2000

static void edma_sw_reset(struct edma_sub *edma_sub);

void print_error_status(struct edma_sub *edma_sub,
				struct edma_request *req)
{
	u32 status, i, j;
	unsigned int *ext_reg = NULL;

	status = edma_read_reg32(edma_sub->base_addr, APU_EDMA2_ERR_STATUS);
	pr_notice("%s error status %x\n", edma_sub->sub_name,
		status);

	for (i = 0; i < (EDMA_REG_SHOW_RANGE >> 2); i++) {
		status = edma_read_reg32(edma_sub->base_addr, i*4);
		pr_notice("edma error dump register[0x%x] = 0x%x\n",
		i*4, status);
	}

	for (i = (0xC00/4); i < (0xD00/4); i++) {
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
	edma_sw_reset(edma_sub);
}

irqreturn_t edma_isr_handler(int irq, void *edma_sub_info)
{
	struct edma_sub *edma_sub = (struct edma_sub *)edma_sub_info;
	u32 status;
	u32 desp0_done;
	u32 desp0_intr;
	u32 desp1_done;
	u32 desp1_intr;
	u32 desp2_done;
	u32 desp2_intr;
	u32 desp3_done;
	u32 desp3_intr;
	u32 desp4_done;
	u32 desp4_intr;

	status = edma_read_reg32(edma_sub->base_addr, APU_EDMA2_INT_STATUS);
	desp0_done = status & DESP0_DONE_STATUS;
	desp1_done = status & DESP1_DONE_STATUS;
	desp2_done = status & DESP2_DONE_STATUS;
	desp3_done = status & DESP3_DONE_STATUS;
	desp4_done = status & EXT_DESP_DONE_STATUS;
	desp0_intr = status & DESP0_DONE_INT_STATUS;
	desp1_intr = status & DESP1_DONE_INT_STATUS;
	desp2_intr = status & DESP2_DONE_INT_STATUS;
	desp3_intr = status & DESP3_DONE_INT_STATUS;
	desp4_intr = status & EXT_DESP_DONE_INT_STATUS;

	edma_sub->is_cmd_done = true;
	if (desp0_done | desp1_done | desp2_done | desp3_done | desp4_done)
		wake_up_interruptible(&edma_sub->cmd_wait);

	if (desp0_intr)
		edma_set_reg32(edma_sub->base_addr, APU_EDMA2_INT_STATUS,
						DESP0_DONE_INT_STATUS);
	else if (desp1_intr)
		edma_set_reg32(edma_sub->base_addr, APU_EDMA2_INT_STATUS,
						DESP1_DONE_INT_STATUS);
	else if (desp2_intr)
		edma_set_reg32(edma_sub->base_addr, APU_EDMA2_INT_STATUS,
						DESP2_DONE_INT_STATUS);
	else if (desp3_intr)
		edma_set_reg32(edma_sub->base_addr, APU_EDMA2_INT_STATUS,
						DESP3_DONE_INT_STATUS);
	else if (desp4_intr)
		edma_set_reg32(edma_sub->base_addr, APU_EDMA2_INT_STATUS,
						EXT_DESP_DONE_INT_STATUS);

	return IRQ_HANDLED;
}

void edma_enable_sequence(struct edma_sub *edma_sub)
{
	edma_set_reg32(edma_sub->base_addr, APU_EDMA2_CTL_0, CLK_ENABLE);
	edma_set_reg32(edma_sub->base_addr, APU_EDMA2_CTL_0, DMA_SW_RST);
	edma_clear_reg32(edma_sub->base_addr, APU_EDMA2_CTL_0, DMA_SW_RST);
}

static void edma_sw_reset(struct edma_sub *edma_sub)
{
	u32 value = 0, count = 0;
	unsigned long flags;

	LOG_DBG("%s\n", __func__);

	spin_lock_irqsave(&edma_sub->reg_lock, flags);

	edma_set_reg32(edma_sub->base_addr, APU_EDMA2_CTL_0, CLK_ENABLE);

	edma_set_reg32(edma_sub->base_addr, APU_EDMA2_CTL_0, AXI_PROT_EN);

	while (!(value & RST_PROT_IDLE)) {
		value = edma_read_reg32(edma_sub->base_addr, APU_EDMA2_CTL_0);
		udelay(5);
		count++;
		if (count > 10000) {
			u32 status, i;

			spin_unlock_irqrestore(&edma_sub->reg_lock, flags);

			/* dump error log */
			pr_notice("hang on %s direct dump...\n", __func__);
			for (i = 0; i < (EDMA_REG_SHOW_RANGE >> 2); i++) {
				status = edma_read_reg32(edma_sub->base_addr,
					i*4);
				pr_notice("hang %s edma error dump [0x%x] = 0x%x\n",
					__func__, i*4, status);
			}
			apusys_reg_dump();

			/* continues do edma */
			spin_lock_irqsave(&edma_sub->reg_lock, flags);

			break;
		}
	}

	LOG_DBG("value = 0x%x\n", value);

	edma_set_reg32(edma_sub->base_addr, APU_EDMA2_CTL_0, DMA_SW_RST);
	udelay(5);
	edma_clear_reg32(edma_sub->base_addr, APU_EDMA2_CTL_0, DMA_SW_RST);
	udelay(5);
	edma_clear_reg32(edma_sub->base_addr, APU_EDMA2_CTL_0, AXI_PROT_EN);

	spin_unlock_irqrestore(&edma_sub->reg_lock, flags);

}


/**
 * @brief trigger edma external mode
 *  parameters:
 *    base_addr: each edma base address
 *    ext_addr:  external descriptor mem addr
 *    num_desp: number of desc.
 *    desp_iommu_en: enable iommu or not
 */
void edma_trigger_external(void __iomem *base_addr, u32 ext_addr, u32 num_desp,
					u8 desp_iommu_en)
{
	edma_set_reg32(base_addr, APU_EDMA2_CTL_0, EDMA_DESCRIPTOR_MODE);

	num_desp--;
	if (desp_iommu_en)
		edma_write_reg32(base_addr, APU_EDMA2_EXT_DESP_CFG_0,
			EXT_DESP_INT_ENABLE | EXT_DESP_USER_IOMMU | num_desp);
	else
		edma_write_reg32(base_addr, APU_EDMA2_EXT_DESP_CFG_0,
				EXT_DESP_INT_ENABLE | num_desp);

	edma_write_reg32(base_addr, APU_EDMA2_EXT_DESP_CFG_1, ext_addr);
	edma_set_reg32(base_addr, APU_EDMA2_CFG_0, EXT_DESP_START);
}


/**
 * @brief excute request by each edma_sub
 *  parameters:
 *    edma_sub: include each edma base address
 */
int edma_exe_v20(struct edma_sub *edma_sub, struct edma_request *req)
{
	int ret = 0;
	void __iomem *base_addr;
	unsigned long flags;


	base_addr = edma_sub->base_addr;
	//edma_enable_sequence(edma_sub);

	edma_sw_reset(edma_sub); // no need in edma 3.0



	spin_lock_irqsave(&edma_sub->reg_lock, flags);

	edma_write_reg32(base_addr, APU_EDMA2_FILL_VALUE, req->fill_value);
	edma_trigger_external(edma_sub->base_addr,
				req->ext_reg_addr,
				req->ext_count,
				req->desp_iommu_en);

	spin_unlock_irqrestore(&edma_sub->reg_lock, flags);

	return ret;

}


