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

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <mach/clk.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#include "msm_mercury_hw.h"
#include "msm_mercury_core.h"
#include "msm_mercury_platform.h"
#include "msm_mercury_common.h"

static int reset_done_ack;
static spinlock_t reset_lock;
static wait_queue_head_t reset_wait;

int mercury_core_reset(void)
{
	struct clk *clk = NULL;

	/*Resettting MMSS Fabric*/

	clk = clk_get(NULL, "jpegd_clk");

	if (!IS_ERR(clk))
		clk_enable(clk);

	msm_bus_axi_porthalt(MSM_BUS_MASTER_JPEG_DEC);
	clk_reset(clk, CLK_RESET_ASSERT);

	/*need to have some delay here, there is no
	 other way to know if hardware reset is complete*/
	usleep_range(1000, 1200);

	msm_bus_axi_portunhalt(MSM_BUS_MASTER_JPEG_DEC);
	clk_reset(clk, CLK_RESET_DEASSERT);

	return 0;
}

int msm_mercury_core_reset(void)
{
	unsigned long flags;
	int rc = 0;
	int tm = 500;/*500ms*/
	MCR_DBG("\n%s\n(%d)%s()\n", __FILE__, __LINE__, __func__);

	spin_lock_irqsave(&reset_lock, flags);
	reset_done_ack = 0;
	spin_unlock_irqrestore(&reset_lock, flags);

	msm_mercury_hw_reset();
	rc = wait_event_interruptible_timeout(reset_wait,
		reset_done_ack,
		msecs_to_jiffies(tm));

	if (!reset_done_ack) {
		MCR_DBG("%s: reset ACK failed %d", __func__, rc);
		return -EBUSY;
	}

	MCR_DBG("(%d)%s() reset_done_ack rc %d\n\n", __LINE__, __func__, rc);
	spin_lock_irqsave(&reset_lock, flags);
	reset_done_ack = 0;
	spin_unlock_irqrestore(&reset_lock, flags);

	return 0;
}

void msm_mercury_core_init(void)
{
	init_waitqueue_head(&reset_wait);
	spin_lock_init(&reset_lock);
}

static int (*msm_mercury_irq_handler) (int, void *, void *);

irqreturn_t msm_mercury_core_irq(int irq_num, void *context)
{
	void *data = NULL;
	unsigned long flags;
	uint16_t mcr_rd_irq;
	uint16_t mcr_wr_irq;
	uint32_t jpeg_status;

	MCR_DBG("\n(%d)%s() irq_number = %d", __LINE__, __func__, irq_num);

	spin_lock_irqsave(&reset_lock, flags);
	reset_done_ack = 1;
	spin_unlock_irqrestore(&reset_lock, flags);

	msm_mercury_hw_irq_get_status(&mcr_rd_irq, &mcr_wr_irq);
	msm_mercury_hw_get_jpeg_status(&jpeg_status);
	MCR_DBG("mercury_rd_irq = 0x%08X\n", mcr_rd_irq);
	MCR_DBG("mercury_wr_irq = 0x%08X\n", mcr_wr_irq);
	MCR_DBG("jpeg_status = 0x%08X\n", jpeg_status);
	if (mcr_wr_irq & MSM_MERCURY_HW_IRQ_SW_RESET_ACK) {
		MCR_DBG("*** SW Reset IRQ received ***\n");
		wake_up(&reset_wait);
		msm_mercury_hw_wr_irq_clear(MSM_MERCURY_HW_IRQ_SW_RESET_ACK);
	}
	if (mcr_wr_irq & MSM_MERCURY_HW_IRQ_WR_ERR_ACK) {
		MCR_DBG("   *** Error IRQ received ***\n");
		msm_mercury_irq_handler(MSM_MERCURY_HW_IRQ_WR_ERR_ACK,
								context, data);
	}
	if (mcr_wr_irq & MSM_MERCURY_HW_IRQ_WR_EOI_ACK) {
		MCR_DBG("   *** WE_EOI IRQ received ***\n");
		msm_mercury_irq_handler(MSM_MERCURY_HW_IRQ_WR_EOI_ACK,
								context, data);
	}
	return IRQ_HANDLED;
}

void msm_mercury_core_irq_install(int (*irq_handler) (int, void *, void *))
{
	msm_mercury_irq_handler = irq_handler;
}

void msm_mercury_core_irq_remove(void)
{
	msm_mercury_irq_handler = NULL;
}
