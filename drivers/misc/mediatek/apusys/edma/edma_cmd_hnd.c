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

#define NO_INTERRUPT		0
#define EDMA_POWEROFF_TIME_DEFAULT 2000

static inline void lock_command(struct edma_sub *edma_sub)
{
	mutex_lock(&edma_sub->cmd_mutex);
	edma_sub->is_cmd_done = false;
}

static inline int wait_command(struct edma_sub *edma_sub, u32 timeout)
{
	return (wait_event_interruptible_timeout(edma_sub->cmd_wait,
						 edma_sub->is_cmd_done,
						 msecs_to_jiffies
						 (timeout)) > 0)
	    ? 0 : -ETIMEDOUT;
}

static inline void unlock_command(struct edma_sub *edma_sub)
{
	mutex_unlock(&edma_sub->cmd_mutex);
}

static void print_error_status(struct edma_sub *edma_sub,
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

void edma_sw_reset(struct edma_sub *edma_sub)
{

	pr_notice("%s\n", __func__);

	edma_set_reg32(edma_sub->base_addr, APU_EDMA2_CTL_0, AXI_PROT_EN);
	edma_set_reg32(edma_sub->base_addr, APU_EDMA2_CTL_0, RST_PROT_IDLE);

	edma_set_reg32(edma_sub->base_addr, APU_EDMA2_CTL_0, CLK_ENABLE);
	edma_set_reg32(edma_sub->base_addr, APU_EDMA2_CTL_0, DMA_SW_RST);
	mdelay(5);
	edma_clear_reg32(edma_sub->base_addr, APU_EDMA2_CTL_0, DMA_SW_RST);
	mdelay(5);

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
int edma_ext_by_sub(struct edma_sub *edma_sub, struct edma_request *req)
{
	int ret = 0;
	void __iomem *base_addr;
	unsigned long long get_port_time = 0;


	base_addr = edma_sub->base_addr;
	ret = edma_power_on(edma_sub);
	if (ret != 0)
		return ret;

	edma_enable_sequence(edma_sub);

	get_port_time = sched_clock();

	lock_command(edma_sub);
	edma_write_reg32(base_addr, APU_EDMA2_FILL_VALUE, req->fill_value);
	edma_trigger_external(edma_sub->base_addr,
				req->ext_reg_addr,
				req->ext_count,
				req->desp_iommu_en);

	ret = wait_command(edma_sub, CMD_WAIT_TIME_MS);
	if (ret) {
		pr_notice
		    ("%s:timeout\n", __func__);
		print_error_status(edma_sub, req);
	}

	get_port_time = sched_clock() - get_port_time; //ns

	edma_sub->ip_time = get_port_time/1000;

	unlock_command(edma_sub);
	edma_power_off(edma_sub, 0);

	return ret;
}

bool edma_is_all_power_off(struct edma_device *edma_device)
{
	int i;

	for (i = 0; i < edma_device->edma_sub_num; i++)
		if (edma_device->edma_sub[i]->power_state == EDMA_POWER_ON)
			return false;

	return true;
}

int edma_power_on(struct edma_sub *edma_sub)
{
	struct edma_device *edma_device;
	int ret = 0;

	edma_device = edma_sub->edma_device;
	mutex_lock(&edma_device->power_mutex);
	if (edma_is_all_power_off(edma_device))	{
		if (timer_pending(&edma_device->power_timer)) {
			del_timer(&edma_device->power_timer);
			LOG_DBG("%s deltimer no pwr on, state = %d\n",
					__func__, edma_device->power_state);
		} else if (edma_device->power_state ==
			EDMA_POWER_ON) {
			LOG_ERR("%s power on twice\n", __func__);
		} else {
			ret = apu_device_power_on(EDMA);
			if (!ret) {
				LOG_DBG("%s power on success\n", __func__);
				edma_device->power_state = EDMA_POWER_ON;
			} else {
				LOG_ERR("%s power on fail\n",
								__func__);
				mutex_unlock(&edma_device->power_mutex);
				return ret;
			}
		}
	}
	edma_sub->power_state = EDMA_POWER_ON;
	mutex_unlock(&edma_device->power_mutex);
	return ret;
}

void edma_start_power_off(struct work_struct *work)
{
	int ret = 0;
	struct edma_device *edmaDev = NULL;

	edmaDev =
	    container_of(work, struct edma_device, power_off_work);

	LOG_DBG("%s: contain power_state = %d!!\n", __func__,
		edmaDev->power_state);

	if (edmaDev->power_state == EDMA_POWER_OFF)
		LOG_ERR("%s pwr off twice\n",
						__func__);

	mutex_lock(&edmaDev->power_mutex);

	ret = apu_device_power_off(EDMA);
	if (ret != 0) {
		LOG_ERR("%s power off fail\n", __func__);
	} else {
		pr_notice("%s: power off done!!\n", __func__);
		edmaDev->power_state = EDMA_POWER_OFF;
	}
	mutex_unlock(&edmaDev->power_mutex);

}

void edma_power_time_up(struct timer_list *tlist)
{
	//struct edma_device *edma_device = (struct edma_device *)data;

	struct edma_device *edma_device =
	    container_of(tlist, struct edma_device, power_timer);

	pr_notice("%s: !!\n", __func__);
	//use kwork job to prevent power off at irq
	schedule_work(&edma_device->power_off_work);
}

int edma_power_off(struct edma_sub *edma_sub, u8 force)
{
	struct edma_device *edma_device =
		edma_sub->edma_device;
	int ret = 0;

	if (edma_device->dbg_cfg
		& EDMA_DBG_DISABLE_PWR_OFF) {

		pr_notice("%s:no power off!!\n", __func__);

		return 0;
	}

	mutex_lock(&edma_device->power_mutex);
	edma_sub->power_state = EDMA_POWER_OFF;
	if (edma_is_all_power_off(edma_device)) {

		if (timer_pending(&edma_device->power_timer))
			del_timer(&edma_device->power_timer);

		if (force == 1) {

			if (edma_device->power_state != EDMA_POWER_OFF) {
				ret = apu_device_power_suspend(EDMA, 1);

				pr_notice("%s: force power off!!\n", __func__);
				if (!ret) {
					LOG_INF("%s power off success\n",
							__func__);
					edma_device->power_state =
						EDMA_POWER_OFF;
				} else {
					LOG_ERR("%s power off fail\n",
						__func__);
				}

			} else
				LOG_INF("%s force power off skip\n",
						__func__);

		} else {
			edma_device->power_timer.expires = jiffies +
			msecs_to_jiffies(EDMA_POWEROFF_TIME_DEFAULT);
			add_timer(&edma_device->power_timer);
		}
	}
	mutex_unlock(&edma_device->power_mutex);

	return ret;

}
#ifndef EDMA_IOCTRL
void edma_setup_ext_mode_request(struct edma_request *req,
			       struct edma_ext *edma_ext,
			       unsigned int type)
{
	req->handle = (u64) req;
	req->cmd = type;
	req->ext_reg_addr = edma_ext->reg_addr;
	req->ext_count = edma_ext->count;
	req->fill_value = edma_ext->fill_value;
	req->desp_iommu_en = edma_ext->desp_iommu_en;
	req->cmd_result = 0;
}

#endif

int edma_execute(struct edma_sub *edma_sub, struct edma_ext *edma_ext)
{
	int ret = 0;
	struct edma_request req = {0};
#ifdef DEBUG
	uint32_t t1, t2;
	uint32_t exe_time;

	t1 = ktime_get_ns();
#endif
	edma_setup_ext_mode_request(&req, edma_ext, EDMA_PROC_EXT_MODE);
	ret = edma_ext_by_sub(edma_sub, &req);

#ifdef DEBUG
	t2 = ktime_get_ns();
	exe_time = (t2 - t1)/1000;

	//pr_notice("%s:ip time = %d\n", __func__, edma_sub->ip_time);
	LOG_DBG("%s:function done, exe_time = %d, ip time = %d\n",
		__func__, exe_time, edma_sub->ip_time);
#endif
	return ret;
}

