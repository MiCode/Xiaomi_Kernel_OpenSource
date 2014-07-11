/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/iopoll.h>
#include <linux/kthread.h>

#include <linux/msm_iommu_domains.h>

#include "mdss.h"
#include "mdss_dsi.h"
#include "mdss_panel.h"
#include "mdss_debug.h"

#define VSYNC_PERIOD 17

struct mdss_dsi_ctrl_pdata *ctrl_list[DSI_CTRL_MAX];

struct mdss_hw mdss_dsi0_hw = {
	.hw_ndx = MDSS_HW_DSI0,
	.ptr = NULL,
	.irq_handler = mdss_dsi_isr,
};

struct mdss_hw mdss_dsi1_hw = {
	.hw_ndx = MDSS_HW_DSI1,
	.ptr = NULL,
	.irq_handler = mdss_dsi_isr,
};


#define DSI_EVENT_Q_MAX	4

#define DSI_BTA_EVENT_TIMEOUT (HZ / 10)

/* event */
struct dsi_event_q {
	struct mdss_dsi_ctrl_pdata *ctrl;
	u32 todo;
};

struct mdss_dsi_event {
	int inited;
	wait_queue_head_t event_q;
	u32 event_pndx;
	u32 event_gndx;
	struct dsi_event_q todo_list[DSI_EVENT_Q_MAX];
	spinlock_t event_lock;
};

static struct mdss_dsi_event dsi_event;

static int dsi_event_thread(void *data);

void mdss_dsi_ctrl_init(struct device *ctrl_dev,
			struct mdss_dsi_ctrl_pdata *ctrl)
{
	if (ctrl->panel_data.panel_info.pdest == DISPLAY_1) {
		mdss_dsi0_hw.ptr = (void *)(ctrl);
		ctrl->dsi_hw = &mdss_dsi0_hw;
		ctrl->ndx = DSI_CTRL_0;
	} else {
		mdss_dsi1_hw.ptr = (void *)(ctrl);
		ctrl->dsi_hw = &mdss_dsi1_hw;
		ctrl->ndx = DSI_CTRL_1;
	}

	ctrl->panel_mode = ctrl->panel_data.panel_info.mipi.mode;

	ctrl_list[ctrl->ndx] = ctrl;	/* keep it */

	if (mdss_register_irq(ctrl->dsi_hw))
		pr_err("%s: mdss_register_irq failed.\n", __func__);

	pr_debug("%s: ndx=%d base=%p\n", __func__, ctrl->ndx, ctrl->ctrl_base);

	init_completion(&ctrl->dma_comp);
	init_completion(&ctrl->mdp_comp);
	init_completion(&ctrl->video_comp);
	init_completion(&ctrl->bta_comp);
	spin_lock_init(&ctrl->irq_lock);
	spin_lock_init(&ctrl->mdp_lock);
	mutex_init(&ctrl->mutex);
	mutex_init(&ctrl->cmd_mutex);
	mdss_dsi_buf_alloc(ctrl_dev, &ctrl->tx_buf, SZ_4K);
	mdss_dsi_buf_alloc(ctrl_dev, &ctrl->rx_buf, SZ_4K);
	mdss_dsi_buf_alloc(ctrl_dev, &ctrl->status_buf, SZ_4K);
	ctrl->cmdlist_commit = mdss_dsi_cmdlist_commit;


	if (dsi_event.inited == 0) {
		kthread_run(dsi_event_thread, (void *)&dsi_event,
						"mdss_dsi_event");
		dsi_event.inited  = 1;
	}
}

void mdss_dsi_clk_req(struct mdss_dsi_ctrl_pdata *ctrl, int enable)
{
	MDSS_XLOG(ctrl->ndx, enable, ctrl->mdp_busy, current->pid);
	if (enable == 0) {
		/* need wait before disable */
		mutex_lock(&ctrl->cmd_mutex);
		mdss_dsi_cmd_mdp_busy(ctrl);
		mutex_unlock(&ctrl->cmd_mutex);
	}

	MDSS_XLOG(ctrl->ndx, enable, ctrl->mdp_busy, current->pid);
	mdss_dsi_clk_ctrl(ctrl, DSI_ALL_CLKS, enable);
}

void mdss_dsi_pll_relock(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int i, cnt;

	/*
	 * todo: this code does not work very well with dual
	 * dsi use cases. Need to fix this eventually.
	 */
	cnt = ctrl->link_clk_cnt;

	/* disable dsi clk */
	for (i = 0; i < cnt; i++)
		mdss_dsi_clk_ctrl(ctrl, DSI_LINK_CLKS, 0);

	/* enable dsi clk */
	for (i = 0; i < cnt; i++)
		mdss_dsi_clk_ctrl(ctrl, DSI_LINK_CLKS, 1);
}

void mdss_dsi_enable_irq(struct mdss_dsi_ctrl_pdata *ctrl, u32 term)
{
	unsigned long flags;

	spin_lock_irqsave(&ctrl->irq_lock, flags);
	if (ctrl->dsi_irq_mask & term) {
		spin_unlock_irqrestore(&ctrl->irq_lock, flags);
		return;
	}
	if (ctrl->dsi_irq_mask == 0) {
		MDSS_XLOG(ctrl->ndx, term);
		mdss_enable_irq(ctrl->dsi_hw);
		pr_debug("%s: IRQ Enable, ndx=%d mask=%x term=%x\n", __func__,
			ctrl->ndx, (int)ctrl->dsi_irq_mask, (int)term);
	}
	ctrl->dsi_irq_mask |= term;
	spin_unlock_irqrestore(&ctrl->irq_lock, flags);
}

void mdss_dsi_disable_irq(struct mdss_dsi_ctrl_pdata *ctrl, u32 term)
{
	unsigned long flags;

	spin_lock_irqsave(&ctrl->irq_lock, flags);
	if (!(ctrl->dsi_irq_mask & term)) {
		spin_unlock_irqrestore(&ctrl->irq_lock, flags);
		return;
	}
	ctrl->dsi_irq_mask &= ~term;
	if (ctrl->dsi_irq_mask == 0) {
		MDSS_XLOG(ctrl->ndx, term);
		mdss_disable_irq(ctrl->dsi_hw);
		pr_debug("%s: IRQ Disable, ndx=%d mask=%x term=%x\n", __func__,
			ctrl->ndx, (int)ctrl->dsi_irq_mask, (int)term);
	}
	spin_unlock_irqrestore(&ctrl->irq_lock, flags);
}

/*
 * mdss_dsi_disale_irq_nosync() should be called
 * from interrupt context
 */
void mdss_dsi_disable_irq_nosync(struct mdss_dsi_ctrl_pdata *ctrl, u32 term)
{
	spin_lock(&ctrl->irq_lock);
	if (!(ctrl->dsi_irq_mask & term)) {
		spin_unlock(&ctrl->irq_lock);
		return;
	}
	ctrl->dsi_irq_mask &= ~term;
	if (ctrl->dsi_irq_mask == 0) {
		MDSS_XLOG(ctrl->ndx, term);
		mdss_disable_irq_nosync(ctrl->dsi_hw);
		pr_debug("%s: IRQ Disable, ndx=%d mask=%x term=%x\n", __func__,
			ctrl->ndx, (int)ctrl->dsi_irq_mask, (int)term);
	}
	spin_unlock(&ctrl->irq_lock);
}

void mdss_dsi_video_test_pattern(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int i;

	MIPI_OUTP((ctrl->ctrl_base) + 0x015c, 0x021);
	MIPI_OUTP((ctrl->ctrl_base) + 0x0164, 0xff0000); /* red */
	i = 0;
	while (i++ < 50) {
		MIPI_OUTP((ctrl->ctrl_base) + 0x0180, 0x1);
		/* Add sleep to get ~50 fps frame rate*/
		msleep(20);
	}
	MIPI_OUTP((ctrl->ctrl_base) + 0x015c, 0x0);
}

void mdss_dsi_cmd_test_pattern(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int i;

	MIPI_OUTP((ctrl->ctrl_base) + 0x015c, 0x201);
	MIPI_OUTP((ctrl->ctrl_base) + 0x016c, 0xff0000); /* red */
	i = 0;
	while (i++ < 50) {
		MIPI_OUTP((ctrl->ctrl_base) + 0x0184, 0x1);
		/* Add sleep to get ~50 fps frame rate*/
		msleep(20);
	}
	MIPI_OUTP((ctrl->ctrl_base) + 0x015c, 0x0);
}

void mdss_dsi_host_init(struct mdss_panel_data *pdata)
{
	u32 dsi_ctrl, intr_ctrl;
	u32 data;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mipi_panel_info *pinfo = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pinfo = &pdata->panel_info.mipi;

	if (pinfo->mode == DSI_VIDEO_MODE) {
		data = 0;
		if (pinfo->last_line_interleave_en)
			data |= BIT(31);
		if (pinfo->pulse_mode_hsa_he)
			data |= BIT(28);
		if (pinfo->hfp_power_stop)
			data |= BIT(24);
		if (pinfo->hbp_power_stop)
			data |= BIT(20);
		if (pinfo->hsa_power_stop)
			data |= BIT(16);
		if (pinfo->eof_bllp_power_stop)
			data |= BIT(15);
		if (pinfo->bllp_power_stop)
			data |= BIT(12);
		data |= ((pinfo->traffic_mode & 0x03) << 8);
		data |= ((pinfo->dst_format & 0x03) << 4); /* 2 bits */
		data |= (pinfo->vc & 0x03);
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x0010, data);

		data = 0;
		data |= ((pinfo->rgb_swap & 0x07) << 12);
		if (pinfo->b_sel)
			data |= BIT(8);
		if (pinfo->g_sel)
			data |= BIT(4);
		if (pinfo->r_sel)
			data |= BIT(0);
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x0020, data);
	} else if (pinfo->mode == DSI_CMD_MODE) {
		data = 0;
		data |= ((pinfo->interleave_max & 0x0f) << 20);
		data |= ((pinfo->rgb_swap & 0x07) << 16);
		if (pinfo->b_sel)
			data |= BIT(12);
		if (pinfo->g_sel)
			data |= BIT(8);
		if (pinfo->r_sel)
			data |= BIT(4);
		data |= (pinfo->dst_format & 0x0f);	/* 4 bits */
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x0040, data);

		/* DSI_COMMAND_MODE_MDP_DCS_CMD_CTRL */
		data = pinfo->wr_mem_continue & 0x0ff;
		data <<= 8;
		data |= (pinfo->wr_mem_start & 0x0ff);
		if (pinfo->insert_dcs_cmd)
			data |= BIT(16);
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x0044, data);
	} else
		pr_err("%s: Unknown DSI mode=%d\n", __func__, pinfo->mode);

	dsi_ctrl = BIT(8) | BIT(2);	/* clock enable & cmd mode */
	intr_ctrl = 0;
	intr_ctrl = (DSI_INTR_CMD_DMA_DONE_MASK | DSI_INTR_CMD_MDP_DONE_MASK);

	if (pinfo->crc_check)
		dsi_ctrl |= BIT(24);
	if (pinfo->ecc_check)
		dsi_ctrl |= BIT(20);
	if (pinfo->data_lane3)
		dsi_ctrl |= BIT(7);
	if (pinfo->data_lane2)
		dsi_ctrl |= BIT(6);
	if (pinfo->data_lane1)
		dsi_ctrl |= BIT(5);
	if (pinfo->data_lane0)
		dsi_ctrl |= BIT(4);


	data = 0;
	if (pinfo->te_sel)
		data |= BIT(31);
	data |= pinfo->mdp_trigger << 4;/* cmd mdp trigger */
	data |= pinfo->dma_trigger;	/* cmd dma trigger */
	data |= (pinfo->stream & 0x01) << 8;
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x0084,
				data); /* DSI_TRIG_CTRL */

	/* DSI_LAN_SWAP_CTRL */
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x00b0, pinfo->dlane_swap);

	/* clock out ctrl */
	data = pinfo->t_clk_post & 0x3f;	/* 6 bits */
	data <<= 8;
	data |= pinfo->t_clk_pre & 0x3f;	/*  6 bits */
	/* DSI_CLKOUT_TIMING_CTRL */
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0xc4, data);

	data = 0;
	if (pinfo->rx_eot_ignore)
		data |= BIT(4);
	if (pinfo->tx_eot_append)
		data |= BIT(0);
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x00cc,
				data); /* DSI_EOT_PACKET_CTRL */


	/* allow only ack-err-status  to generate interrupt */
	/* DSI_ERR_INT_MASK0 */
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x010c, 0x13ff3fe0);

	intr_ctrl |= DSI_INTR_ERROR_MASK;
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x0110,
				intr_ctrl); /* DSI_INTL_CTRL */

	/* turn esc, byte, dsi, pclk, sclk, hclk on */
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x11c,
					0x23f); /* DSI_CLK_CTRL */

	dsi_ctrl |= BIT(0);	/* enable dsi */
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x0004, dsi_ctrl);

	wmb();
}

void mdss_dsi_set_tx_power_mode(int mode, struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	u32 data;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	data = MIPI_INP((ctrl_pdata->ctrl_base) + 0x3c);

	if (mode == 0)
		data &= ~BIT(26);
	else
		data |= BIT(26);

	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x3c, data);
}

void mdss_dsi_sw_reset(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	u32 dsi_ctrl;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	dsi_ctrl = MIPI_INP((ctrl_pdata->ctrl_base) + 0x0004);
	dsi_ctrl &= ~0x01;
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x0004, dsi_ctrl);
	wmb();

	/* turn esc, byte, dsi, pclk, sclk, hclk on */
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x11c,
					0x23f); /* DSI_CLK_CTRL */
	wmb();

	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x118, 0x01);
	wmb();
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x118, 0x00);
	wmb();
}

void mdss_dsi_sw_reset_restore(struct mdss_dsi_ctrl_pdata *ctrl)
{
	u32 data0, data1;

	data0 = MIPI_INP(ctrl->ctrl_base + 0x0004);
	data1 = data0;
	data1 &= ~0x01;
	MIPI_OUTP(ctrl->ctrl_base + 0x0004, data1);
	/*
	 * dsi controller need to be disabled before
	 * clocks turned on
	 */
	wmb();	/* make sure dsi contoller is disabled */

	/* turn esc, byte, dsi, pclk, sclk, hclk on */
	MIPI_OUTP(ctrl->ctrl_base + 0x11c, 0x23f); /* DSI_CLK_CTRL */
	wmb();	/* make sure clocks enabled */

	/* dsi controller can only be reset while clocks are running */
	MIPI_OUTP(ctrl->ctrl_base + 0x118, 0x01);
	wmb();	/* make sure reset happen */
	MIPI_OUTP(ctrl->ctrl_base + 0x118, 0x00);
	wmb();	/* controller out of reset */
	MIPI_OUTP(ctrl->ctrl_base + 0x0004, data0);
	wmb();	/* make sure dsi controller enabled again */
}

void mdss_dsi_err_intr_ctrl(struct mdss_dsi_ctrl_pdata *ctrl, u32 mask,
					int enable)
{
	u32 intr;

	intr = MIPI_INP(ctrl->ctrl_base + 0x0110);

	if (enable)
		intr |= mask;
	else
		intr &= ~mask;

	pr_debug("%s: intr=%x enable=%d\n", __func__, intr, enable);

	MIPI_OUTP(ctrl->ctrl_base + 0x0110, intr); /* DSI_INTL_CTRL */
}

void mdss_dsi_controller_cfg(int enable,
			     struct mdss_panel_data *pdata)
{

	u32 dsi_ctrl;
	u32 status;
	u32 sleep_us = 1000;
	u32 timeout_us = 16000;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	/* Check for CMD_MODE_DMA_BUSY */
	if (readl_poll_timeout(((ctrl_pdata->ctrl_base) + 0x0008),
			   status,
			   ((status & 0x02) == 0),
			       sleep_us, timeout_us))
		pr_info("%s: DSI status=%x failed\n", __func__, status);

	/* Check for x_HS_FIFO_EMPTY */
	if (readl_poll_timeout(((ctrl_pdata->ctrl_base) + 0x000c),
			   status,
			   ((status & 0x11111000) == 0x11111000),
			       sleep_us, timeout_us))
		pr_info("%s: FIFO status=%x failed\n", __func__, status);

	/* Check for VIDEO_MODE_ENGINE_BUSY */
	if (readl_poll_timeout(((ctrl_pdata->ctrl_base) + 0x0008),
			   status,
			   ((status & 0x08) == 0),
			       sleep_us, timeout_us)) {
		pr_debug("%s: DSI status=%x\n", __func__, status);
		pr_debug("%s: Doing sw reset\n", __func__);
		mdss_dsi_sw_reset(pdata);
	}

	dsi_ctrl = MIPI_INP((ctrl_pdata->ctrl_base) + 0x0004);
	if (enable)
		dsi_ctrl |= 0x01;
	else
		dsi_ctrl &= ~0x01;

	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x0004, dsi_ctrl);
	wmb();
}

void mdss_dsi_op_mode_config(int mode,
			     struct mdss_panel_data *pdata)
{
	u32 dsi_ctrl, intr_ctrl, dma_ctrl;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	dsi_ctrl = MIPI_INP((ctrl_pdata->ctrl_base) + 0x0004);
	/*If Video enabled, Keep Video and Cmd mode ON */
	if (dsi_ctrl & 0x02)
		dsi_ctrl &= ~0x05;
	else
		dsi_ctrl &= ~0x07;

	if (mode == DSI_VIDEO_MODE) {
		dsi_ctrl |= 0x03;
		intr_ctrl = DSI_INTR_CMD_DMA_DONE_MASK | DSI_INTR_BTA_DONE_MASK;
	} else {		/* command mode */
		dsi_ctrl |= 0x05;
		if (pdata->panel_info.type == MIPI_VIDEO_PANEL)
			dsi_ctrl |= 0x02;

		intr_ctrl = DSI_INTR_CMD_DMA_DONE_MASK | DSI_INTR_ERROR_MASK |
			DSI_INTR_CMD_MDP_DONE_MASK | DSI_INTR_BTA_DONE_MASK;
	}

	dma_ctrl = BIT(28);	/* embedded mode */
	if (mdss_dsi_sync_wait_enable(ctrl_pdata))
		dma_ctrl |= BIT(31);

	pr_debug("%s: configuring ctrl%d\n", __func__, ctrl_pdata->ndx);
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x0110, intr_ctrl);
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x0004, dsi_ctrl);
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x003c, dma_ctrl);
	wmb();
}

void mdss_dsi_cmd_bta_sw_trigger(struct mdss_panel_data *pdata)
{
	u32 status;
	int timeout_us = 10000;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x098, 0x01);	/* trigger */
	wmb();

	/* Check for CMD_MODE_DMA_BUSY */
	if (readl_poll_timeout(((ctrl_pdata->ctrl_base) + 0x0008),
				status, ((status & 0x0010) == 0),
				0, timeout_us))
		pr_info("%s: DSI status=%x failed\n", __func__, status);

	mdss_dsi_ack_err_status(ctrl_pdata);

	pr_debug("%s: BTA done, status = %d\n", __func__, status);
}

static int mdss_dsi_read_status(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct dcs_cmd_req cmdreq;

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = ctrl->status_cmds.cmds;
	cmdreq.cmds_cnt = ctrl->status_cmds.cmd_cnt;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL | CMD_REQ_RX;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;
	cmdreq.rbuf = ctrl->status_buf.data;

	return mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}


/**
 * mdss_dsi_reg_status_check() - Check dsi panel status through reg read
 * @ctrl_pdata: pointer to the dsi controller structure
 *
 * This function can be used to check the panel status through reading the
 * status register from the panel.
 *
 * Return: positive value if the panel is in good state, negative value or
 * zero otherwise.
 */
int mdss_dsi_reg_status_check(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int ret = 0;

	if (ctrl_pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return 0;
	}

	pr_debug("%s: Checking Register status\n", __func__);

	mdss_dsi_clk_ctrl(ctrl_pdata, DSI_ALL_CLKS, 1);

	if (ctrl_pdata->status_cmds.link_state == DSI_HS_MODE)
		mdss_dsi_set_tx_power_mode(0, &ctrl_pdata->panel_data);

	ret = mdss_dsi_read_status(ctrl_pdata);

	if (ctrl_pdata->status_cmds.link_state == DSI_HS_MODE)
		mdss_dsi_set_tx_power_mode(1, &ctrl_pdata->panel_data);

	if (ret == 0) {
		if (ctrl_pdata->status_buf.data[0] !=
						ctrl_pdata->status_value) {
			pr_err("%s: Read back value from panel is incorrect\n",
								__func__);
			ret = -EINVAL;
		} else {
			ret = 1;
		}
	} else {
		pr_err("%s: Read status register returned error\n", __func__);
	}

	mdss_dsi_clk_ctrl(ctrl_pdata, DSI_ALL_CLKS, 0);
	pr_debug("%s: Read register done with ret: %d\n", __func__, ret);

	return ret;
}

/**
 * mdss_dsi_bta_status_check() - Check dsi panel status through bta check
 * @ctrl_pdata: pointer to the dsi controller structure
 *
 * This function can be used to check status of the panel using bta check
 * for the panel.
 *
 * Return: positive value if the panel is in good state, negative value or
 * zero otherwise.
 */
int mdss_dsi_bta_status_check(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int ret = 0;
	unsigned long flag;

	if (ctrl_pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);

		/*
		 * This should not return error otherwise
		 * BTA status thread will treat it as dead panel scenario
		 * and request for blank/unblank
		 */
		return 0;
	}

	pr_debug("%s: Checking BTA status\n", __func__);

	mdss_dsi_clk_ctrl(ctrl_pdata, DSI_ALL_CLKS, 1);
	spin_lock_irqsave(&ctrl_pdata->mdp_lock, flag);
	INIT_COMPLETION(ctrl_pdata->bta_comp);
	mdss_dsi_enable_irq(ctrl_pdata, DSI_BTA_TERM);
	spin_unlock_irqrestore(&ctrl_pdata->mdp_lock, flag);
	MIPI_OUTP(ctrl_pdata->ctrl_base + 0x098, 0x01); /* trigger  */
	wmb();

	ret = wait_for_completion_killable_timeout(&ctrl_pdata->bta_comp,
						DSI_BTA_EVENT_TIMEOUT);
	if (ret <= 0) {
		mdss_dsi_disable_irq(ctrl_pdata, DSI_BTA_TERM);
		pr_err("%s: DSI BTA error: %i\n", __func__, ret);
	}

	mdss_dsi_clk_ctrl(ctrl_pdata, DSI_ALL_CLKS, 0);
	pr_debug("%s: BTA done with ret: %d\n", __func__, ret);

	return ret;
}

int mdss_dsi_cmd_reg_tx(u32 data,
			unsigned char *ctrl_base)
{
	int i;
	char *bp;

	bp = (char *)&data;
	pr_debug("%s: ", __func__);
	for (i = 0; i < 4; i++)
		pr_debug("%x ", *bp++);

	pr_debug("\n");

	MIPI_OUTP(ctrl_base + 0x0084, 0x04);/* sw trigger */
	MIPI_OUTP(ctrl_base + 0x0004, 0x135);

	wmb();

	MIPI_OUTP(ctrl_base + 0x03c, data);
	wmb();
	MIPI_OUTP(ctrl_base + 0x090, 0x01);	/* trigger */
	wmb();

	udelay(300);

	return 4;
}

static int mdss_dsi_wait4video_eng_busy(struct mdss_dsi_ctrl_pdata *ctrl);

static int mdss_dsi_cmd_dma_tx(struct mdss_dsi_ctrl_pdata *ctrl,
					struct dsi_buf *tp);

static int mdss_dsi_cmd_dma_rx(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_buf *rp, int rlen);

static int mdss_dsi_cmds2buf_tx(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_cmd_desc *cmds, int cnt)
{
	struct dsi_buf *tp;
	struct dsi_cmd_desc *cm;
	struct dsi_ctrl_hdr *dchdr;
	int len, wait, tot = 0;

	tp = &ctrl->tx_buf;
	mdss_dsi_buf_init(tp);
	cm = cmds;
	len = 0;
	while (cnt--) {
		dchdr = &cm->dchdr;
		mdss_dsi_buf_reserve(tp, len);
		len = mdss_dsi_cmd_dma_add(tp, cm);
		if (!len) {
			pr_err("%s: failed to add cmd = 0x%x\n",
				__func__,  cm->payload[0]);
			return -EINVAL;
		}
		tot += len;
		if (dchdr->last) {
			tp->data = tp->start; /* begin of buf */

			wait = mdss_dsi_wait4video_eng_busy(ctrl);

			mdss_dsi_enable_irq(ctrl, DSI_CMD_TERM);
			len = mdss_dsi_cmd_dma_tx(ctrl, tp);
			if (IS_ERR_VALUE(len)) {
				mdss_dsi_disable_irq(ctrl, DSI_CMD_TERM);
				pr_err("%s: failed to call cmd_dma_tx for cmd = 0x%x\n",
					__func__,  cmds->payload[0]);
				return -EINVAL;
			}

			if (!wait || dchdr->wait > VSYNC_PERIOD)
				usleep(dchdr->wait * 1000);

			mdss_dsi_buf_init(tp);
			len = 0;
		}
		cm++;
	}
	return tot;
}

/**
 * __mdss_dsi_cmd_mode_config() - Enable/disable command mode engine
 * @ctrl: pointer to the dsi controller structure
 * @enable: true to enable command mode, false to disable command mode
 *
 * This function can be used to temporarily enable the command mode
 * engine (even for video mode panels) so as to transfer any dma commands to
 * the panel. It can also be used to disable the command mode engine
 * when no longer needed.
 *
 * Return: true, if there was a mode switch to command mode for video mode
 * panels.
 */
static inline bool __mdss_dsi_cmd_mode_config(
	struct mdss_dsi_ctrl_pdata *ctrl, bool enable)
{
	bool mode_changed = false;
	u32 dsi_ctrl;

	dsi_ctrl = MIPI_INP((ctrl->ctrl_base) + 0x0004);
	/* if currently in video mode, enable command mode */
	if (enable) {
		if ((dsi_ctrl) & BIT(1)) {
			MIPI_OUTP((ctrl->ctrl_base) + 0x0004,
				dsi_ctrl | BIT(2));
			mode_changed = true;
		}
	} else {
		MIPI_OUTP((ctrl->ctrl_base) + 0x0004, dsi_ctrl & ~BIT(2));
	}

	return mode_changed;
}

/*
 * mdss_dsi_cmds_tx:
 * thread context only
 */
int mdss_dsi_cmds_tx(struct mdss_dsi_ctrl_pdata *ctrl,
		struct dsi_cmd_desc *cmds, int cnt)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *mctrl = NULL;

	/*
	 * Turn on cmd mode in order to transmit the commands.
	 * For video mode, do not send cmds more than one pixel line,
	 * since it only transmit it during BLLP.
	 */

	if (mdss_dsi_sync_wait_enable(ctrl)) {
		if (mdss_dsi_sync_wait_trigger(ctrl)) {
			mctrl = mdss_dsi_get_other_ctrl(ctrl);
			if (!mctrl) {
				pr_warn("%s: sync_wait, NULL at other control\n",
							__func__);
				goto do_send;
			}

			mctrl->cmd_cfg_restore =
					__mdss_dsi_cmd_mode_config(mctrl, 1);
		} else if (!ctrl->do_unicast) {
			/* broadcast cmds, let cmd_trigger do it */
			return 0;

		}
	}

	pr_debug("%s: ctrl=%d do_unicast=%d\n", __func__,
				ctrl->ndx, ctrl->do_unicast);

do_send:
	ctrl->cmd_cfg_restore = __mdss_dsi_cmd_mode_config(ctrl, 1);

	ret = mdss_dsi_cmds2buf_tx(ctrl, cmds, cnt);
	if (IS_ERR_VALUE(ret)) {
		pr_err("%s: failed to call\n", __func__);
		cnt = -EINVAL;
	}

	if (!ctrl->do_unicast) {
		if (mctrl && mctrl->cmd_cfg_restore) {
			__mdss_dsi_cmd_mode_config(mctrl, 0);
			mctrl->cmd_cfg_restore = false;
		}

		if (ctrl->cmd_cfg_restore) {
			__mdss_dsi_cmd_mode_config(ctrl, 0);
			ctrl->cmd_cfg_restore = false;
		}
	}

	return cnt;
}

/* MIPI_DSI_MRPS, Maximum Return Packet Size */
static char max_pktsize[2] = {0x00, 0x00}; /* LSB tx first, 10 bytes */

static struct dsi_cmd_desc pkt_size_cmd = {
	{DTYPE_MAX_PKTSIZE, 1, 0, 0, 0, sizeof(max_pktsize)},
	max_pktsize,
};

/*
 * mdss_dsi_cmds_rx() - dcs read from panel
 * @ctrl: dsi controller
 * @cmds: read command descriptor
 * @len: number of bytes to read back
 *
 * controller have 4 registers can hold 16 bytes of rxed data
 * dcs packet: 4 bytes header + payload + 2 bytes crc
 * 2 padding bytes add to payload to have payload length is mutipled by 4
 * 1st read: 4 bytes header + 8 bytes payload + 2 padding + 2 crc
 * 2nd read: 12 bytes payload + 2 padding + 2 crc
 * 3rd read: 12 bytes payload + 2 padding + 2 crc
 *
 */
int mdss_dsi_cmds_rx(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_cmd_desc *cmds, int rlen)
{
	int data_byte, rx_byte, dlen, end;
	int short_response, diff, pkt_size, ret = 0;
	struct dsi_buf *tp, *rp;
	char cmd;
	struct mdss_dsi_ctrl_pdata *mctrl = NULL;


	/*
	 * Turn on cmd mode in order to transmit the commands.
	 * For video mode, do not send cmds more than one pixel line,
	 * since it only transmit it during BLLP.
	 */
	if (mdss_dsi_sync_wait_enable(ctrl)) {
		if (mdss_dsi_sync_wait_trigger(ctrl)) {
			mctrl = mdss_dsi_get_other_ctrl(ctrl);
			if (!mctrl) {
				pr_warn("%s: sync_wait, NULL at other control\n",
							__func__);
				goto do_send;
			}

			mctrl->cmd_cfg_restore =
					__mdss_dsi_cmd_mode_config(mctrl, 1);
		} else {
			/* skip cmds, let cmd_trigger do it */
			return 0;

		}
	}

do_send:
	ctrl->cmd_cfg_restore = __mdss_dsi_cmd_mode_config(ctrl, 1);

	if (rlen <= 2) {
		short_response = 1;
		pkt_size = rlen;
		rx_byte = 4;
	} else {
		short_response = 0;
		data_byte = 8;	/* first read */
		/*
		 * add extra 2 padding bytes to have overall
		 * packet size is multipe by 4. This also make
		 * sure 4 bytes dcs headerlocates within a
		 * 32 bits register after shift in.
		 */
		pkt_size = data_byte + 2;
		rx_byte = data_byte + 8; /* 4 header + 2 crc  + 2 padding*/
	}


	tp = &ctrl->tx_buf;
	rp = &ctrl->rx_buf;

	end = 0;
	mdss_dsi_buf_init(rp);
	while (!end) {
		pr_debug("%s:  rlen=%d pkt_size=%d rx_byte=%d\n",
				__func__, rlen, pkt_size, rx_byte);
		max_pktsize[0] = pkt_size;
		mdss_dsi_buf_init(tp);
		ret = mdss_dsi_cmd_dma_add(tp, &pkt_size_cmd);
		if (!ret) {
			pr_err("%s: failed to add max_pkt_size\n",
				__func__);
			rp->len = 0;
			goto end;
		}

		mdss_dsi_wait4video_eng_busy(ctrl);

		mdss_dsi_enable_irq(ctrl, DSI_CMD_TERM);
		ret = mdss_dsi_cmd_dma_tx(ctrl, tp);
		if (IS_ERR_VALUE(ret)) {
			mdss_dsi_disable_irq(ctrl, DSI_CMD_TERM);
			pr_err("%s: failed to tx max_pkt_size\n",
				__func__);
			rp->len = 0;
			goto end;
		}
		pr_debug("%s: max_pkt_size=%d sent\n",
					__func__, pkt_size);

		mdss_dsi_buf_init(tp);
		ret = mdss_dsi_cmd_dma_add(tp, cmds);
		if (!ret) {
			pr_err("%s: failed to add cmd = 0x%x\n",
				__func__,  cmds->payload[0]);
			rp->len = 0;
			goto end;
		}

		mdss_dsi_wait4video_eng_busy(ctrl);	/* video mode only */
		mdss_dsi_enable_irq(ctrl, DSI_CMD_TERM);
		/* transmit read comamnd to client */
		ret = mdss_dsi_cmd_dma_tx(ctrl, tp);
		if (IS_ERR_VALUE(ret)) {
			mdss_dsi_disable_irq(ctrl, DSI_CMD_TERM);
			pr_err("%s: failed to tx cmd = 0x%x\n",
				__func__,  cmds->payload[0]);
			rp->len = 0;
			goto end;
		}
		/*
		 * once cmd_dma_done interrupt received,
		 * return data from client is ready and stored
		 * at RDBK_DATA register already
		 * since rx fifo is 16 bytes, dcs header is kept at first loop,
		 * after that dcs header lost during shift into registers
		 */
		dlen = mdss_dsi_cmd_dma_rx(ctrl, rp, rx_byte);

		if (short_response)
			break;

		if (rlen <= data_byte) {
			diff = data_byte - rlen;
			end = 1;
		} else {
			diff = 0;
			rlen -= data_byte;
		}

		dlen -= 2; /* 2 padding bytes */
		dlen -= 2; /* 2 crc */
		dlen -= diff;
		rp->data += dlen;	/* next start position */
		rp->len += dlen;
		data_byte = 12;	/* NOT first read */
		pkt_size += data_byte;
		pr_debug("%s: rp data=%x len=%d dlen=%d diff=%d\n",
			 __func__, (int) (unsigned long) rp->data,
			 rp->len, dlen, diff);
	}

	rp->data = rp->start;	/* move back to start position */
	cmd = rp->data[0];
	switch (cmd) {
	case DTYPE_ACK_ERR_RESP:
		pr_debug("%s: rx ACK_ERR_PACLAGE\n", __func__);
		rp->len = 0;
	case DTYPE_GEN_READ1_RESP:
	case DTYPE_DCS_READ1_RESP:
		mdss_dsi_short_read1_resp(rp);
		break;
	case DTYPE_GEN_READ2_RESP:
	case DTYPE_DCS_READ2_RESP:
		mdss_dsi_short_read2_resp(rp);
		break;
	case DTYPE_GEN_LREAD_RESP:
	case DTYPE_DCS_LREAD_RESP:
		mdss_dsi_long_read_resp(rp);
		break;
	default:
		pr_warning("%s:Invalid response cmd\n", __func__);
		rp->len = 0;
	}
end:

	if (mctrl && mctrl->cmd_cfg_restore) {
		__mdss_dsi_cmd_mode_config(mctrl, 0);
		mctrl->cmd_cfg_restore = false;
	}

	if (ctrl->cmd_cfg_restore) {
		__mdss_dsi_cmd_mode_config(ctrl, 0);
		ctrl->cmd_cfg_restore = false;
	}

	return rp->len;
}

#define DMA_TX_TIMEOUT 200

static int mdss_dsi_cmd_dma_tx(struct mdss_dsi_ctrl_pdata *ctrl,
					struct dsi_buf *tp)
{
	int len, ret = 0;
	int domain = MDSS_IOMMU_DOMAIN_UNSECURE;
	char *bp;
	struct mdss_dsi_ctrl_pdata *mctrl = NULL;

	bp = tp->data;

	len = ALIGN(tp->len, 4);
	ctrl->dma_size = ALIGN(tp->len, SZ_4K);


	if (is_mdss_iommu_attached()) {
		int ret = msm_iommu_map_contig_buffer(tp->dmap,
				mdss_get_iommu_domain(domain), 0,
				ctrl->dma_size, SZ_4K, 0, &(ctrl->dma_addr));
		if (IS_ERR_VALUE(ret)) {
			pr_err("unable to map dma memory to iommu(%d)\n", ret);
			return -ENOMEM;
		}
	} else {
		ctrl->dma_addr = tp->dmap;
	}

	INIT_COMPLETION(ctrl->dma_comp);

	if (mdss_dsi_sync_wait_trigger(ctrl)) {
		/* broadcast same cmd to other panel */
		mctrl = mdss_dsi_get_other_ctrl(ctrl);
		if (mctrl && mctrl->dma_addr == 0) {
			MIPI_OUTP(mctrl->ctrl_base + 0x048, ctrl->dma_addr);
			MIPI_OUTP(mctrl->ctrl_base + 0x04c, len);
			MIPI_OUTP(mctrl->ctrl_base + 0x090, 0x01); /* trigger */
		}
	}

	/* send cmd to its panel */
	MIPI_OUTP((ctrl->ctrl_base) + 0x048, ctrl->dma_addr);
	MIPI_OUTP((ctrl->ctrl_base) + 0x04c, len);
	wmb();

	MIPI_OUTP((ctrl->ctrl_base) + 0x090, 0x01);
	wmb();

	if (ctrl->do_unicast) {
		/* let cmd_trigger to kickoff later */
		pr_debug("%s: SKIP, ndx=%d do_unicast=%d\n", __func__,
					ctrl->ndx, ctrl->do_unicast);
		ret = tp->len;
		goto end;
	}

	ret = wait_for_completion_timeout(&ctrl->dma_comp,
				msecs_to_jiffies(DMA_TX_TIMEOUT));
	if (ret == 0)
		ret = -ETIMEDOUT;
	else
		ret = tp->len;

	if (mctrl && mctrl->dma_addr) {
		if (is_mdss_iommu_attached()) {
			msm_iommu_unmap_contig_buffer(mctrl->dma_addr,
			mdss_get_iommu_domain(domain), 0, mctrl->dma_size);
		}
		mctrl->dma_addr = 0;
		mctrl->dma_size = 0;
	}

	if (is_mdss_iommu_attached()) {
		msm_iommu_unmap_contig_buffer(ctrl->dma_addr,
			mdss_get_iommu_domain(domain), 0, ctrl->dma_size);
	}

	ctrl->dma_addr = 0;
	ctrl->dma_size = 0;
end:
	return ret;
}

static int mdss_dsi_cmd_dma_rx(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_buf *rp, int rx_byte)

{
	u32 *lp, data;
	int i, off, cnt;

	lp = (u32 *)rp->data;
	cnt = rx_byte;
	cnt += 3;
	cnt >>= 2;

	if (cnt > 4)
		cnt = 4; /* 4 x 32 bits registers only */

	off = 0x06c;	/* DSI_RDBK_DATA0 */
	off += ((cnt - 1) * 4);

	for (i = 0; i < cnt; i++) {
		data = (u32)MIPI_INP((ctrl->ctrl_base) + off);
		*lp++ = ntohl(data);	/* to network byte order */
		pr_debug("%s: data = 0x%x and ntohl(data) = 0x%x\n",
					 __func__, data, ntohl(data));
		off -= 4;
	}

	return rx_byte;
}


void mdss_dsi_wait4video_done(struct mdss_dsi_ctrl_pdata *ctrl)
{
	unsigned long flag;
	u32 data;

	/* DSI_INTL_CTRL */
	data = MIPI_INP((ctrl->ctrl_base) + 0x0110);
	data |= DSI_INTR_VIDEO_DONE_MASK;

	MIPI_OUTP((ctrl->ctrl_base) + 0x0110, data);

	spin_lock_irqsave(&ctrl->mdp_lock, flag);
	INIT_COMPLETION(ctrl->video_comp);
	mdss_dsi_enable_irq(ctrl, DSI_VIDEO_TERM);
	spin_unlock_irqrestore(&ctrl->mdp_lock, flag);

	wait_for_completion_timeout(&ctrl->video_comp,
			msecs_to_jiffies(VSYNC_PERIOD * 4));

	data = MIPI_INP((ctrl->ctrl_base) + 0x0110);
	data &= ~DSI_INTR_VIDEO_DONE_MASK;
	MIPI_OUTP((ctrl->ctrl_base) + 0x0110, data);
}

static int mdss_dsi_wait4video_eng_busy(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int ret = 0;

	if (ctrl->panel_mode == DSI_CMD_MODE)
		return ret;

	if (ctrl->ctrl_state & CTRL_STATE_MDP_ACTIVE) {
		mdss_dsi_wait4video_done(ctrl);
		/* delay 4 ms to skip BLLP */
		usleep(4000);
		ret = 1;
	}

	return ret;
}

void mdss_dsi_cmd_mdp_start(struct mdss_dsi_ctrl_pdata *ctrl)
{
	unsigned long flag;

	spin_lock_irqsave(&ctrl->mdp_lock, flag);
	mdss_dsi_enable_irq(ctrl, DSI_MDP_TERM);
	ctrl->mdp_busy = true;
	INIT_COMPLETION(ctrl->mdp_comp);
	MDSS_XLOG(ctrl->ndx, ctrl->mdp_busy, current->pid);
	spin_unlock_irqrestore(&ctrl->mdp_lock, flag);
}

void mdss_dsi_cmd_mdp_busy(struct mdss_dsi_ctrl_pdata *ctrl)
{
	unsigned long flags;
	int need_wait = 0;

	pr_debug("%s: start pid=%d\n",
				__func__, current->pid);

	MDSS_XLOG(ctrl->ndx, ctrl->mdp_busy, current->pid, XLOG_FUNC_ENTRY);
	spin_lock_irqsave(&ctrl->mdp_lock, flags);
	if (ctrl->mdp_busy == true)
		need_wait++;
	spin_unlock_irqrestore(&ctrl->mdp_lock, flags);

	if (need_wait) {
		/* wait until DMA finishes the current job */
		pr_debug("%s: pending pid=%d\n",
				__func__, current->pid);
		if (!wait_for_completion_timeout(&ctrl->mdp_comp,
					msecs_to_jiffies(DMA_TX_TIMEOUT))) {
			pr_err("%s: timeout error\n", __func__);
			MDSS_XLOG_TOUT_HANDLER("mdp", "dsi0", "dsi1",
						"edp", "hdmi", "panic");
		}
	}
	pr_debug("%s: done pid=%d\n", __func__, current->pid);
	MDSS_XLOG(ctrl->ndx, ctrl->mdp_busy, current->pid, XLOG_FUNC_EXIT);
}

int mdss_dsi_cmdlist_tx(struct mdss_dsi_ctrl_pdata *ctrl,
				struct dcs_cmd_req *req)
{
	int ret, ret_val = -EINVAL;


	if (mdss_dsi_sync_wait_enable(ctrl)) {
		ctrl->do_unicast = false;
		 if (!ctrl->cmd_sync_wait_trigger &&
					req->flags & CMD_REQ_UNICAST)
			ctrl->do_unicast = true;
	}

	ret = mdss_dsi_cmds_tx(ctrl, req->cmds, req->cmds_cnt);

	if (!IS_ERR_VALUE(ret))
		ret_val = 0;

	if (req->cb)
		req->cb(ret);

	return ret_val;
}

int mdss_dsi_cmdlist_rx(struct mdss_dsi_ctrl_pdata *ctrl,
				struct dcs_cmd_req *req)
{
	struct dsi_buf *rp;
	int len = 0, ret = 0;

	if (req->rbuf) {
		rp = &ctrl->rx_buf;
		len = mdss_dsi_cmds_rx(ctrl, req->cmds, req->rlen);
		memcpy(req->rbuf, rp->data, rp->len);
	} else {
		pr_err("%s: No rx buffer provided\n", __func__);
	}

	if (req->cb)
		req->cb(len);

	return ret;
}

int mdss_dsi_cmdlist_commit(struct mdss_dsi_ctrl_pdata *ctrl, int from_mdp)
{
	struct dcs_cmd_req *req;
	int ret = -EINVAL;
	int rc = 0;

	if (mdss_get_sd_client_cnt())
		return -EPERM;

	if (from_mdp)	/* from mdp kickoff */
		mutex_lock(&ctrl->cmd_mutex);

	req = mdss_dsi_cmdlist_get(ctrl);

	MDSS_XLOG(ctrl->ndx, from_mdp, ctrl->mdp_busy, current->pid,
							XLOG_FUNC_ENTRY);

	/* make sure dsi_cmd_mdp is idle */
	mdss_dsi_cmd_mdp_busy(ctrl);

	pr_debug("%s: ctrl=%d from_mdp=%d pid=%d\n", __func__,
				ctrl->ndx, from_mdp, current->pid);

	if (req == NULL)
		goto need_lock;

	MDSS_XLOG(ctrl->ndx, req->flags, req->cmds_cnt, from_mdp, current->pid);

	/*
	 * mdss interrupt is generated in mdp core clock domain
	 * mdp clock need to be enabled to receive dsi interrupt
	 * also, axi bus bandwidth need since dsi controller will
	 * fetch dcs commands from axi bus
	 */
	mdss_bus_scale_set_quota(MDSS_HW_DSI0, SZ_1M, SZ_1M);

	pr_debug("%s:  from_mdp=%d pid=%d\n", __func__, from_mdp, current->pid);
	mdss_dsi_clk_ctrl(ctrl, DSI_ALL_CLKS, 1);

	rc = mdss_iommu_ctrl(1);
	if (IS_ERR_VALUE(rc)) {
		pr_err("IOMMU attach failed\n");
		mutex_unlock(&ctrl->cmd_mutex);
		return rc;
	}
	if (req->flags & CMD_REQ_RX)
		ret = mdss_dsi_cmdlist_rx(ctrl, req);
	else
		ret = mdss_dsi_cmdlist_tx(ctrl, req);
	mdss_iommu_ctrl(0);
	mdss_dsi_clk_ctrl(ctrl, DSI_ALL_CLKS, 0);
	mdss_bus_scale_set_quota(MDSS_HW_DSI0, 0, 0);
need_lock:

	MDSS_XLOG(ctrl->ndx, from_mdp, ctrl->mdp_busy, current->pid,
							XLOG_FUNC_EXIT);

	if (from_mdp) { /* from mdp kickoff */
		/* acquire lock only has new frame update */
		if (ctrl->roi.w != 0 || ctrl->roi.h != 0)
			mdss_dsi_cmd_mdp_start(ctrl);

		mutex_unlock(&ctrl->cmd_mutex);
	}

	return ret;
}

static void dsi_send_events(struct mdss_dsi_ctrl_pdata *ctrl, u32 events)
{
	struct dsi_event_q *evq;

	if (!dsi_event.inited)
		return;

	pr_debug("%s: ev=%x\n", __func__, events);

	spin_lock(&dsi_event.event_lock);
	evq = &dsi_event.todo_list[dsi_event.event_pndx++];
	evq->todo = events;
	evq->ctrl = ctrl;
	dsi_event.event_pndx %= DSI_EVENT_Q_MAX;
	wake_up(&dsi_event.event_q);
	spin_unlock(&dsi_event.event_lock);
}

static int dsi_event_thread(void *data)
{
	struct mdss_dsi_event *ev;
	struct dsi_event_q *evq;
	struct mdss_dsi_ctrl_pdata *ctrl;
	unsigned long flag;
	struct sched_param param;
	u32 todo = 0;
	int ret;

	param.sched_priority = 16;
	ret = sched_setscheduler_nocheck(current, SCHED_FIFO, &param);
	if (ret)
		pr_err("%s: set priority failed\n", __func__);

	ev = (struct mdss_dsi_event *)data;
	/* event */
	init_waitqueue_head(&ev->event_q);
	spin_lock_init(&ev->event_lock);

	while (1) {
		wait_event(ev->event_q, (ev->event_pndx != ev->event_gndx));
		spin_lock_irqsave(&ev->event_lock, flag);
		evq = &ev->todo_list[ev->event_gndx++];
		todo = evq->todo;
		ctrl = evq->ctrl;
		evq->todo = 0;
		ev->event_gndx %= DSI_EVENT_Q_MAX;
		spin_unlock_irqrestore(&ev->event_lock, flag);

		pr_debug("%s: ev=%x\n", __func__, todo);

		if (todo & DSI_EV_PLL_UNLOCKED)
			mdss_dsi_pll_relock(ctrl);

		if (todo & DSI_EV_MDP_FIFO_UNDERFLOW) {
			mutex_lock(&ctrl->mutex);
			if (ctrl->recovery) {
				mdss_dsi_clk_ctrl(ctrl, DSI_ALL_CLKS, 1);
				mdss_dsi_sw_reset_restore(ctrl);
				ctrl->recovery->fxn(ctrl->recovery->data);
				mdss_dsi_clk_ctrl(ctrl, DSI_ALL_CLKS, 0);
			}
			mutex_unlock(&ctrl->mutex);

			MDSS_XLOG_TOUT_HANDLER("mdp", "dsi0", "dsi1",
						"edp", "hdmi", "panic");
		}

		if (todo & DSI_EV_DSI_FIFO_EMPTY)
			mdss_dsi_sw_reset_restore(ctrl);

		if (todo & DSI_EV_MDP_BUSY_RELEASE) {
			spin_lock_irqsave(&ctrl->mdp_lock, flag);
			ctrl->mdp_busy = false;
			mdss_dsi_disable_irq_nosync(ctrl, DSI_MDP_TERM);
			complete(&ctrl->mdp_comp);
			spin_unlock_irqrestore(&ctrl->mdp_lock, flag);

			/* enable dsi error interrupt */
			mdss_dsi_clk_ctrl(ctrl, DSI_ALL_CLKS, 1);
			mdss_dsi_err_intr_ctrl(ctrl, DSI_INTR_ERROR_MASK, 1);
			mdss_dsi_clk_ctrl(ctrl, DSI_ALL_CLKS, 0);
		}

	}

	return 0;
}

void mdss_dsi_ack_err_status(struct mdss_dsi_ctrl_pdata *ctrl)
{
	u32 status;
	unsigned char *base;

	base = ctrl->ctrl_base;

	status = MIPI_INP(base + 0x0068);/* DSI_ACK_ERR_STATUS */

	if (status) {
		MIPI_OUTP(base + 0x0068, status);
		/* Writing of an extra 0 needed to clear error bits */
		MIPI_OUTP(base + 0x0068, 0);
		pr_err("%s: status=%x\n", __func__, status);
	}
}

void mdss_dsi_timeout_status(struct mdss_dsi_ctrl_pdata *ctrl)
{
	u32 status;
	unsigned char *base;

	base = ctrl->ctrl_base;

	status = MIPI_INP(base + 0x00c0);/* DSI_TIMEOUT_STATUS */

	if (status & 0x0111) {
		MIPI_OUTP(base + 0x00c0, status);
		pr_err("%s: status=%x\n", __func__, status);
	}
}

void mdss_dsi_dln0_phy_err(struct mdss_dsi_ctrl_pdata *ctrl)
{
	u32 status;
	unsigned char *base;

	base = ctrl->ctrl_base;

	status = MIPI_INP(base + 0x00b4);/* DSI_DLN0_PHY_ERR */

	if (status & 0x011111) {
		MIPI_OUTP(base + 0x00b4, status);
		pr_err("%s: status=%x\n", __func__, status);
	}
}

void mdss_dsi_fifo_status(struct mdss_dsi_ctrl_pdata *ctrl)
{
	u32 status;
	unsigned char *base;

	base = ctrl->ctrl_base;

	status = MIPI_INP(base + 0x000c);/* DSI_FIFO_STATUS */

	/* fifo underflow, overflow and empty*/
	if (status & 0xcccc4489) {
		MIPI_OUTP(base + 0x000c, status);
		pr_err("%s: status=%x\n", __func__, status);
		if (status & 0x0080)  /* CMD_DMA_FIFO_UNDERFLOW */
			dsi_send_events(ctrl, DSI_EV_MDP_FIFO_UNDERFLOW);
		if (status & 0x11110000) /* DLN_FIFO_EMPTY */
			dsi_send_events(ctrl, DSI_EV_DSI_FIFO_EMPTY);
	}
}

void mdss_dsi_status(struct mdss_dsi_ctrl_pdata *ctrl)
{
	u32 status;
	unsigned char *base;

	base = ctrl->ctrl_base;

	status = MIPI_INP(base + 0x0008);/* DSI_STATUS */

	if (status & 0x80000000) { /* INTERLEAVE_OP_CONTENTION */
		MIPI_OUTP(base + 0x0008, status);
		pr_err("%s: status=%x\n", __func__, status);
	}
}

void mdss_dsi_clk_status(struct mdss_dsi_ctrl_pdata *ctrl)
{
	u32 status;
	unsigned char *base;

	base = ctrl->ctrl_base;
	status = MIPI_INP(base + 0x0120);/* DSI_CLK_STATUS */

	if (status & 0x10000) { /* DSI_CLK_PLL_UNLOCKED */
		MIPI_OUTP(base + 0x0120, status);
		dsi_send_events(ctrl, DSI_EV_PLL_UNLOCKED);
		pr_err("%s: status=%x\n", __func__, status);
	}
}

void mdss_dsi_error(struct mdss_dsi_ctrl_pdata *ctrl)
{

	/* disable dsi error interrupt */
	mdss_dsi_err_intr_ctrl(ctrl, DSI_INTR_ERROR_MASK, 0);

	/* DSI_ERR_INT_MASK0 */
	mdss_dsi_clk_status(ctrl);	/* Mask0, 0x10000000 */
	mdss_dsi_fifo_status(ctrl);	/* mask0, 0x133d00 */
	mdss_dsi_ack_err_status(ctrl);	/* mask0, 0x01f */
	mdss_dsi_timeout_status(ctrl);	/* mask0, 0x0e0 */
	mdss_dsi_status(ctrl);		/* mask0, 0xc0100 */
	mdss_dsi_dln0_phy_err(ctrl);	/* mask0, 0x3e00000 */

	dsi_send_events(ctrl, DSI_EV_MDP_BUSY_RELEASE);
}

irqreturn_t mdss_dsi_isr(int irq, void *ptr)
{
	u32 isr;
	struct mdss_dsi_ctrl_pdata *ctrl =
			(struct mdss_dsi_ctrl_pdata *)ptr;

	if (!ctrl->ctrl_base) {
		pr_err("%s:%d DSI base adr no Initialized",
						__func__, __LINE__);
		return IRQ_HANDLED;
	}

	isr = MIPI_INP(ctrl->ctrl_base + 0x0110);/* DSI_INTR_CTRL */
	MIPI_OUTP(ctrl->ctrl_base + 0x0110, isr);

	pr_debug("%s: ndx=%d isr=%x\n", __func__, ctrl->ndx, isr);

	if (isr & DSI_INTR_ERROR) {
		MDSS_XLOG(ctrl->ndx, ctrl->mdp_busy, isr, 0x97);
		pr_err("%s: ndx=%d isr=%x\n", __func__, ctrl->ndx, isr);
		mdss_dsi_error(ctrl);
	}

	if (isr & DSI_INTR_VIDEO_DONE) {
		spin_lock(&ctrl->mdp_lock);
		mdss_dsi_disable_irq_nosync(ctrl, DSI_VIDEO_TERM);
		complete(&ctrl->video_comp);
		spin_unlock(&ctrl->mdp_lock);
	}

	if (isr & DSI_INTR_CMD_DMA_DONE) {
		MDSS_XLOG(ctrl->ndx, ctrl->mdp_busy, isr, 0x98);
		spin_lock(&ctrl->mdp_lock);
		mdss_dsi_disable_irq_nosync(ctrl, DSI_CMD_TERM);
		complete(&ctrl->dma_comp);
		spin_unlock(&ctrl->mdp_lock);
	}

	if (isr & DSI_INTR_CMD_MDP_DONE) {
		MDSS_XLOG(ctrl->ndx, ctrl->mdp_busy, isr, 0x99);
		spin_lock(&ctrl->mdp_lock);
		ctrl->mdp_busy = false;
		mdss_dsi_disable_irq_nosync(ctrl, DSI_MDP_TERM);
		complete(&ctrl->mdp_comp);
		spin_unlock(&ctrl->mdp_lock);
	}

	if (isr & DSI_INTR_BTA_DONE) {
		spin_lock(&ctrl->mdp_lock);
		mdss_dsi_disable_irq_nosync(ctrl, DSI_BTA_TERM);
		complete(&ctrl->bta_comp);
		spin_unlock(&ctrl->mdp_lock);
	}

	return IRQ_HANDLED;
}
