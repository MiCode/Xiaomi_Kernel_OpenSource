/* Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
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
#include <linux/interrupt.h>
#include <linux/of_device.h>

#include "dsi_v2.h"
#include "dsi_io_v2.h"
#include "dsi_host_v2.h"
#include "mdss_debug.h"
#include "mdp3.h"

#define DSI_POLL_SLEEP_US 1000
#define DSI_POLL_TIMEOUT_US 16000
#define DSI_ESC_CLK_RATE 19200000
#define DSI_DMA_CMD_TIMEOUT_MS 200
#define VSYNC_PERIOD 17
#define DSI_MAX_PKT_SIZE 10
#define DSI_SHORT_PKT_DATA_SIZE 2
#define DSI_MAX_BYTES_TO_READ 16

struct dsi_host_v2_private {
	unsigned char *dsi_base;
	size_t dsi_reg_size;
	struct device dis_dev;
	int clk_count;
	int dsi_on;

	void (*debug_enable_clk)(int on);
};

static struct dsi_host_v2_private *dsi_host_private;
static int msm_dsi_clk_ctrl(struct mdss_panel_data *pdata, int enable);

int msm_dsi_init(void)
{
	if (!dsi_host_private) {
		dsi_host_private = kzalloc(sizeof(struct dsi_host_v2_private),
					GFP_KERNEL);
		if (!dsi_host_private) {
			pr_err("fail to alloc dsi host private data\n");
			return -ENOMEM;
		}
	}

	return 0;
}

void msm_dsi_deinit(void)
{
	kfree(dsi_host_private);
	dsi_host_private = NULL;
}

void msm_dsi_ack_err_status(unsigned char *ctrl_base)
{
	u32 status;

	status = MIPI_INP(ctrl_base + DSI_ACK_ERR_STATUS);

	if (status) {
		MIPI_OUTP(ctrl_base + DSI_ACK_ERR_STATUS, status);

		/* Writing of an extra 0 needed to clear error bits */
		MIPI_OUTP(ctrl_base + DSI_ACK_ERR_STATUS, 0);
		pr_err("%s: status=%x\n", __func__, status);
	}
}

void msm_dsi_timeout_status(unsigned char *ctrl_base)
{
	u32 status;

	status = MIPI_INP(ctrl_base + DSI_TIMEOUT_STATUS);
	if (status & 0x0111) {
		MIPI_OUTP(ctrl_base + DSI_TIMEOUT_STATUS, status);
		pr_err("%s: status=%x\n", __func__, status);
	}
}

void msm_dsi_dln0_phy_err(unsigned char *ctrl_base)
{
	u32 status;

	status = MIPI_INP(ctrl_base + DSI_DLN0_PHY_ERR);

	if (status & 0x011111) {
		MIPI_OUTP(ctrl_base + DSI_DLN0_PHY_ERR, status);
		pr_err("%s: status=%x\n", __func__, status);
	}
}

void msm_dsi_fifo_status(unsigned char *ctrl_base)
{
	u32 status;

	status = MIPI_INP(ctrl_base + DSI_FIFO_STATUS);

	if (status & 0x44444489) {
		MIPI_OUTP(ctrl_base + DSI_FIFO_STATUS, status);
		pr_err("%s: status=%x\n", __func__, status);
	}
}

void msm_dsi_status(unsigned char *ctrl_base)
{
	u32 status;

	status = MIPI_INP(ctrl_base + DSI_STATUS);

	if (status & 0x80000000) {
		MIPI_OUTP(ctrl_base + DSI_STATUS, status);
		pr_err("%s: status=%x\n", __func__, status);
	}
}

void msm_dsi_error(unsigned char *ctrl_base)
{
	msm_dsi_ack_err_status(ctrl_base);
	msm_dsi_timeout_status(ctrl_base);
	msm_dsi_fifo_status(ctrl_base);
	msm_dsi_status(ctrl_base);
	msm_dsi_dln0_phy_err(ctrl_base);
}

static void msm_dsi_set_irq_mask(struct mdss_dsi_ctrl_pdata *ctrl, u32 mask)
{
	u32 intr_ctrl;
	intr_ctrl = MIPI_INP(dsi_host_private->dsi_base + DSI_INT_CTRL);
	intr_ctrl |= mask;
	MIPI_OUTP(dsi_host_private->dsi_base + DSI_INT_CTRL, intr_ctrl);
}

static void msm_dsi_clear_irq_mask(struct mdss_dsi_ctrl_pdata *ctrl, u32 mask)
{
	u32 intr_ctrl;
	intr_ctrl = MIPI_INP(dsi_host_private->dsi_base + DSI_INT_CTRL);
	intr_ctrl &= ~mask;
	MIPI_OUTP(dsi_host_private->dsi_base + DSI_INT_CTRL, intr_ctrl);
}

static void msm_dsi_set_irq(struct mdss_dsi_ctrl_pdata *ctrl, u32 mask)
{
	unsigned long flags;

	spin_lock_irqsave(&ctrl->irq_lock, flags);
	if (ctrl->dsi_irq_mask & mask) {
		spin_unlock_irqrestore(&ctrl->irq_lock, flags);
		return;
	}
	if (ctrl->dsi_irq_mask == 0) {
		ctrl->mdss_util->enable_irq(ctrl->dsi_hw);
		pr_debug("%s: IRQ Enable, mask=%x term=%x\n", __func__,
			(int)ctrl->dsi_irq_mask, (int)mask);
	}

	msm_dsi_set_irq_mask(ctrl, mask);
	ctrl->dsi_irq_mask |= mask;
	spin_unlock_irqrestore(&ctrl->irq_lock, flags);
}

static void msm_dsi_clear_irq(struct mdss_dsi_ctrl_pdata *ctrl, u32 mask)
{
	unsigned long flags;

	spin_lock_irqsave(&ctrl->irq_lock, flags);
	if (!(ctrl->dsi_irq_mask & mask)) {
		spin_unlock_irqrestore(&ctrl->irq_lock, flags);
		return;
	}
	ctrl->dsi_irq_mask &= ~mask;
	if (ctrl->dsi_irq_mask == 0) {
		ctrl->mdss_util->disable_irq(ctrl->dsi_hw);
		pr_debug("%s: IRQ Disable, mask=%x term=%x\n", __func__,
			(int)ctrl->dsi_irq_mask, (int)mask);
	}
	msm_dsi_clear_irq_mask(ctrl, mask);
	spin_unlock_irqrestore(&ctrl->irq_lock, flags);
}

irqreturn_t msm_dsi_isr_handler(int irq, void *ptr)
{
	u32 isr;

	struct mdss_dsi_ctrl_pdata *ctrl =
		(struct mdss_dsi_ctrl_pdata *)ptr;

	spin_lock(&ctrl->mdp_lock);

	if (ctrl->dsi_irq_mask == 0) {
		spin_unlock(&ctrl->mdp_lock);
		return IRQ_HANDLED;
	}

	isr = MIPI_INP(dsi_host_private->dsi_base + DSI_INT_CTRL);
	MIPI_OUTP(dsi_host_private->dsi_base + DSI_INT_CTRL, isr);

	pr_debug("%s: isr=%x", __func__, isr);

	if (isr & DSI_INTR_ERROR) {
		pr_err("%s: isr=%x %x", __func__, isr, (int)DSI_INTR_ERROR);
		msm_dsi_error(dsi_host_private->dsi_base);
	}

	if (isr & DSI_INTR_VIDEO_DONE)
		complete(&ctrl->video_comp);

	if (isr & DSI_INTR_CMD_DMA_DONE)
		complete(&ctrl->dma_comp);

	if (isr & DSI_INTR_BTA_DONE)
		complete(&ctrl->bta_comp);

	if (isr & DSI_INTR_CMD_MDP_DONE)
		complete(&ctrl->mdp_comp);

	spin_unlock(&ctrl->mdp_lock);

	return IRQ_HANDLED;
}

int msm_dsi_irq_init(struct device *dev, int irq_no,
			struct mdss_dsi_ctrl_pdata *ctrl)
{
	int ret;
	u32 isr;
	struct mdss_hw *dsi_hw;

	msm_dsi_ahb_ctrl(1);
	isr = MIPI_INP(dsi_host_private->dsi_base + DSI_INT_CTRL);
	isr &= ~DSI_INTR_ALL_MASK;
	MIPI_OUTP(dsi_host_private->dsi_base + DSI_INT_CTRL, isr);
	msm_dsi_ahb_ctrl(0);

	ret = devm_request_irq(dev, irq_no, msm_dsi_isr_handler,
				0x0, "DSI", ctrl);
	if (ret) {
		pr_err("msm_dsi_irq_init request_irq() failed!\n");
		return ret;
	}

	dsi_hw = kzalloc(sizeof(struct mdss_hw), GFP_KERNEL);
	if (!dsi_hw) {
		pr_err("no mem to save hw info: kzalloc fail\n");
		return -ENOMEM;
	}
	ctrl->dsi_hw = dsi_hw;

	dsi_hw->irq_info = kzalloc(sizeof(struct irq_info), GFP_KERNEL);
	if (!dsi_hw->irq_info) {
		kfree(dsi_hw);
		pr_err("no mem to save irq info: kzalloc fail\n");
		return -ENOMEM;
	}

	dsi_hw->hw_ndx = MDSS_HW_DSI0;
	dsi_hw->irq_info->irq = irq_no;
	dsi_hw->irq_info->irq_mask = 0;
	dsi_hw->irq_info->irq_ena = false;
	dsi_hw->irq_info->irq_buzy = false;

	ctrl->mdss_util->register_irq(ctrl->dsi_hw);
	ctrl->mdss_util->disable_irq(ctrl->dsi_hw);

	return 0;
}

static void msm_dsi_get_cmd_engine(struct mdss_dsi_ctrl_pdata *ctrl)
{
	unsigned char *ctrl_base = dsi_host_private->dsi_base;
	u32 dsi_ctrl;

	if (ctrl->panel_mode == DSI_VIDEO_MODE) {
		dsi_ctrl = MIPI_INP(ctrl_base + DSI_CTRL);
		MIPI_OUTP(ctrl_base + DSI_CTRL, dsi_ctrl | 0x04);
	}
}

static void msm_dsi_release_cmd_engine(struct mdss_dsi_ctrl_pdata *ctrl)
{
	unsigned char *ctrl_base = dsi_host_private->dsi_base;
	u32 dsi_ctrl;
	if (ctrl->panel_mode == DSI_VIDEO_MODE) {
		dsi_ctrl = MIPI_INP(ctrl_base + DSI_CTRL);
		dsi_ctrl &= ~0x04;
		MIPI_OUTP(ctrl_base + DSI_CTRL, dsi_ctrl);
	}
}

static int msm_dsi_wait4mdp_done(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc;
	unsigned long flag;

	spin_lock_irqsave(&ctrl->mdp_lock, flag);
	reinit_completion(&ctrl->mdp_comp);
	msm_dsi_set_irq(ctrl, DSI_INTR_CMD_MDP_DONE_MASK);
	spin_unlock_irqrestore(&ctrl->mdp_lock, flag);

	rc = wait_for_completion_timeout(&ctrl->mdp_comp,
			msecs_to_jiffies(VSYNC_PERIOD * 4));

	if (rc == 0) {
		pr_err("DSI wait 4 mdp done time out\n");
		rc = -ETIME;
	} else if (!IS_ERR_VALUE(rc)) {
		rc = 0;
	}

	msm_dsi_clear_irq(ctrl, DSI_INTR_CMD_MDP_DONE_MASK);

	return rc;
}

void msm_dsi_cmd_mdp_busy(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc;
	u32 dsi_status;
	unsigned char *ctrl_base = dsi_host_private->dsi_base;

	if (ctrl->panel_mode == DSI_VIDEO_MODE)
		return;

	dsi_status = MIPI_INP(ctrl_base + DSI_STATUS);
	if (dsi_status & 0x04) {
		pr_debug("dsi command engine is busy\n");
		rc = msm_dsi_wait4mdp_done(ctrl);
		if (rc)
			pr_err("Timed out waiting for mdp done");
	}
}

static int msm_dsi_wait4video_done(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc;
	unsigned long flag;

	spin_lock_irqsave(&ctrl->mdp_lock, flag);
	reinit_completion(&ctrl->video_comp);
	msm_dsi_set_irq(ctrl, DSI_INTR_VIDEO_DONE_MASK);
	spin_unlock_irqrestore(&ctrl->mdp_lock, flag);

	rc = wait_for_completion_timeout(&ctrl->video_comp,
				msecs_to_jiffies(VSYNC_PERIOD * 4));

	if (rc == 0) {
		pr_err("DSI wait 4 video done time out\n");
		rc = -ETIME;
	} else if (!IS_ERR_VALUE(rc)) {
		rc = 0;
	}

	msm_dsi_clear_irq(ctrl, DSI_INTR_VIDEO_DONE_MASK);

	return rc;
}

static int msm_dsi_wait4video_eng_busy(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc = 0;
	u32 dsi_status;
	unsigned char *ctrl_base = dsi_host_private->dsi_base;

	if (ctrl->panel_mode == DSI_CMD_MODE)
		return rc;

	dsi_status = MIPI_INP(ctrl_base + DSI_STATUS);
	if (dsi_status & 0x08) {
		pr_debug("dsi command in video mode wait for active region\n");
		rc = msm_dsi_wait4video_done(ctrl);
		/* delay 4-5 ms to skip BLLP */
		if (!rc)
			usleep_range(4000, 5000);
	}
	return rc;
}

void msm_dsi_host_init(struct mdss_panel_data *pdata)
{
	u32 dsi_ctrl, data;
	unsigned char *ctrl_base = dsi_host_private->dsi_base;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mipi_panel_info *pinfo;

	pr_debug("msm_dsi_host_init\n");

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	pinfo  = &pdata->panel_info.mipi;


	if (pinfo->mode == DSI_VIDEO_MODE) {
		data = 0;
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
		MIPI_OUTP(ctrl_base + DSI_VIDEO_MODE_CTRL, data);

		data = 0;
		data |= ((pinfo->rgb_swap & 0x07) << 12);
		if (pinfo->b_sel)
			data |= BIT(8);
		if (pinfo->g_sel)
			data |= BIT(4);
		if (pinfo->r_sel)
			data |= BIT(0);
		MIPI_OUTP(ctrl_base + DSI_VIDEO_MODE_DATA_CTRL, data);
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
		data |= (pinfo->dst_format & 0x0f); /* 4 bits */
		MIPI_OUTP(ctrl_base + DSI_COMMAND_MODE_MDP_CTRL, data);

		/* DSI_COMMAND_MODE_MDP_DCS_CMD_CTRL */
		data = pinfo->wr_mem_continue & 0x0ff;
		data <<= 8;
		data |= (pinfo->wr_mem_start & 0x0ff);
		if (pinfo->insert_dcs_cmd)
			data |= BIT(16);
		MIPI_OUTP(ctrl_base + DSI_COMMAND_MODE_MDP_DCS_CMD_CTRL,
				data);
	} else
		pr_err("%s: Unknown DSI mode=%d\n", __func__, pinfo->mode);

	dsi_ctrl = BIT(8) | BIT(2); /* clock enable & cmd mode */

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

	/* from frame buffer, low power mode */
	/* DSI_COMMAND_MODE_DMA_CTRL */
	MIPI_OUTP(ctrl_base + DSI_COMMAND_MODE_DMA_CTRL, 0x14000000);

	data = 0;
	if (pinfo->te_sel)
		data |= BIT(31);
	data |= pinfo->mdp_trigger << 4;/* cmd mdp trigger */
	data |= pinfo->dma_trigger;	/* cmd dma trigger */
	data |= (pinfo->stream & 0x01) << 8;
	MIPI_OUTP(ctrl_base + DSI_TRIG_CTRL, data);

	/* DSI_LAN_SWAP_CTRL */
	MIPI_OUTP(ctrl_base + DSI_LANE_SWAP_CTRL, ctrl_pdata->dlane_swap);

	/* clock out ctrl */
	data = pinfo->t_clk_post & 0x3f;	/* 6 bits */
	data <<= 8;
	data |= pinfo->t_clk_pre & 0x3f;	/*  6 bits */
	/* DSI_CLKOUT_TIMING_CTRL */
	MIPI_OUTP(ctrl_base + DSI_CLKOUT_TIMING_CTRL, data);

	data = 0;
	if (pinfo->rx_eot_ignore)
		data |= BIT(4);
	if (pinfo->tx_eot_append)
		data |= BIT(0);
	MIPI_OUTP(ctrl_base + DSI_EOT_PACKET_CTRL, data);


	/* allow only ack-err-status  to generate interrupt */
	/* DSI_ERR_INT_MASK0 */
	MIPI_OUTP(ctrl_base + DSI_ERR_INT_MASK0, 0x13ff3fe0);

	/* turn esc, byte, dsi, pclk, sclk, hclk on */
	MIPI_OUTP(ctrl_base + DSI_CLK_CTRL, 0x23f);

	dsi_ctrl |= BIT(0);	/* enable dsi */
	MIPI_OUTP(ctrl_base + DSI_CTRL, dsi_ctrl);

	wmb();
}

void dsi_set_tx_power_mode(int mode)
{
	u32 data;
	unsigned char *ctrl_base = dsi_host_private->dsi_base;

	data = MIPI_INP(ctrl_base + DSI_COMMAND_MODE_DMA_CTRL);

	if (mode == 0)
		data &= ~BIT(26);
	else
		data |= BIT(26);

	MIPI_OUTP(ctrl_base + DSI_COMMAND_MODE_DMA_CTRL, data);
}

void msm_dsi_sw_reset(void)
{
	u32 dsi_ctrl;
	unsigned char *ctrl_base = dsi_host_private->dsi_base;

	pr_debug("msm_dsi_sw_reset\n");

	dsi_ctrl = MIPI_INP(ctrl_base + DSI_CTRL);
	dsi_ctrl &= ~0x01;
	MIPI_OUTP(ctrl_base + DSI_CTRL, dsi_ctrl);
	wmb();

	/* turn esc, byte, dsi, pclk, sclk, hclk on */
	MIPI_OUTP(ctrl_base + DSI_CLK_CTRL, 0x23f);
	wmb();

	MIPI_OUTP(ctrl_base + DSI_SOFT_RESET, 0x01);
	wmb();
	MIPI_OUTP(ctrl_base + DSI_SOFT_RESET, 0x00);
	wmb();
}

void msm_dsi_controller_cfg(int enable)
{
	u32 dsi_ctrl, status;
	unsigned char *ctrl_base = dsi_host_private->dsi_base;

	pr_debug("msm_dsi_controller_cfg\n");

	/* Check for CMD_MODE_DMA_BUSY */
	if (readl_poll_timeout((ctrl_base + DSI_STATUS),
				status,
				((status & 0x02) == 0),
				DSI_POLL_SLEEP_US, DSI_POLL_TIMEOUT_US)) {
		pr_err("%s: DSI status=%x failed\n", __func__, status);
		pr_err("%s: Doing sw reset\n", __func__);
		msm_dsi_sw_reset();
	}

	/* Check for x_HS_FIFO_EMPTY */
	if (readl_poll_timeout((ctrl_base + DSI_FIFO_STATUS),
				status,
				((status & 0x11111000) == 0x11111000),
				DSI_POLL_SLEEP_US, DSI_POLL_TIMEOUT_US))
		pr_err("%s: FIFO status=%x failed\n", __func__, status);

	/* Check for VIDEO_MODE_ENGINE_BUSY */
	if (readl_poll_timeout((ctrl_base + DSI_STATUS),
				status,
				((status & 0x08) == 0),
				DSI_POLL_SLEEP_US, DSI_POLL_TIMEOUT_US)) {
		pr_err("%s: DSI status=%x\n", __func__, status);
		pr_err("%s: Doing sw reset\n", __func__);
		msm_dsi_sw_reset();
	}

	dsi_ctrl = MIPI_INP(ctrl_base + DSI_CTRL);
	if (enable)
		dsi_ctrl |= 0x01;
	else
		dsi_ctrl &= ~0x01;

	MIPI_OUTP(ctrl_base + DSI_CTRL, dsi_ctrl);
	wmb();
}

void msm_dsi_op_mode_config(int mode, struct mdss_panel_data *pdata)
{
	u32 dsi_ctrl;
	unsigned char *ctrl_base = dsi_host_private->dsi_base;

	pr_debug("msm_dsi_op_mode_config\n");

	dsi_ctrl = MIPI_INP(ctrl_base + DSI_CTRL);

	if (dsi_ctrl & DSI_VIDEO_MODE_EN)
		dsi_ctrl &= ~(DSI_CMD_MODE_EN|DSI_EN);
	else
		dsi_ctrl &= ~(DSI_CMD_MODE_EN|DSI_VIDEO_MODE_EN|DSI_EN);

	if (mode == DSI_VIDEO_MODE) {
		dsi_ctrl |= (DSI_VIDEO_MODE_EN|DSI_EN);
	} else {
		dsi_ctrl |= (DSI_CMD_MODE_EN|DSI_EN);
		/* For Video mode panel, keep Video and Cmd mode ON */
		if (pdata->panel_info.type == MIPI_VIDEO_PANEL)
			dsi_ctrl |= DSI_VIDEO_MODE_EN;
	}

	pr_debug("%s: dsi_ctrl=%x\n", __func__, dsi_ctrl);

	MIPI_OUTP(ctrl_base + DSI_CTRL, dsi_ctrl);
	wmb();
}

int msm_dsi_cmd_dma_tx(struct mdss_dsi_ctrl_pdata *ctrl,
				struct dsi_buf *tp)
{
	int len, rc;
	unsigned long size, addr;
	unsigned char *ctrl_base = dsi_host_private->dsi_base;
	unsigned long flag;

	len = ALIGN(tp->len, 4);
	size = ALIGN(tp->len, SZ_4K);

	tp->dmap = dma_map_single(&dsi_host_private->dis_dev, tp->data, size,
				DMA_TO_DEVICE);
	if (dma_mapping_error(&dsi_host_private->dis_dev, tp->dmap)) {
		pr_err("%s: dmap mapp failed\n", __func__);
		return -ENOMEM;
	}

	addr = tp->dmap;

	msm_dsi_get_cmd_engine(ctrl);

	spin_lock_irqsave(&ctrl->mdp_lock, flag);
	reinit_completion(&ctrl->dma_comp);
	msm_dsi_set_irq(ctrl, DSI_INTR_CMD_DMA_DONE_MASK);
	spin_unlock_irqrestore(&ctrl->mdp_lock, flag);

	MIPI_OUTP(ctrl_base + DSI_DMA_CMD_OFFSET, addr);
	MIPI_OUTP(ctrl_base + DSI_DMA_CMD_LENGTH, len);
	wmb();

	MIPI_OUTP(ctrl_base + DSI_CMD_MODE_DMA_SW_TRIGGER, 0x01);
	wmb();

	rc = wait_for_completion_timeout(&ctrl->dma_comp,
				msecs_to_jiffies(DSI_DMA_CMD_TIMEOUT_MS));
	if (rc == 0) {
		pr_err("DSI command transaction time out\n");
		rc = -ETIME;
	} else if (!IS_ERR_VALUE(rc)) {
		rc = 0;
	}

	dma_unmap_single(&dsi_host_private->dis_dev, tp->dmap, size,
			DMA_TO_DEVICE);
	tp->dmap = 0;

	msm_dsi_clear_irq(ctrl, DSI_INTR_CMD_DMA_DONE_MASK);

	msm_dsi_release_cmd_engine(ctrl);

	return rc;
}

int msm_dsi_cmd_dma_rx(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_buf *rp, int rlen)
{
	u32 *lp, data;
	int i, off, cnt;
	unsigned char *ctrl_base = dsi_host_private->dsi_base;

	lp = (u32 *)rp->data;
	cnt = rlen;
	cnt += 3;
	cnt >>= 2;

	if (cnt > 4)
		cnt = 4; /* 4 x 32 bits registers only */

	off = DSI_RDBK_DATA0;
	off += ((cnt - 1) * 4);

	for (i = 0; i < cnt; i++) {
		data = (u32)MIPI_INP(ctrl_base + off);
		*lp++ = ntohl(data); /* to network byte order */
		pr_debug("%s: data = 0x%x and ntohl(data) = 0x%x\n",
					 __func__, data, ntohl(data));
		off -= 4;
		rp->len += sizeof(*lp);
	}

	return rlen;
}

static int msm_dsi_cmds_tx(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_cmd_desc *cmds, int cnt)
{
	struct dsi_buf *tp;
	struct dsi_cmd_desc *cm;
	struct dsi_ctrl_hdr *dchdr;
	int len;
	int rc = 0;


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
			rc = -EINVAL;
			goto dsi_cmds_tx_err;
		}

		if (dchdr->last) {
			tp->data = tp->start; /* begin of buf */
			rc = msm_dsi_wait4video_eng_busy(ctrl);
			if (rc) {
				pr_err("%s: wait4video_eng failed\n", __func__);
				goto dsi_cmds_tx_err;

			}

			rc = msm_dsi_cmd_dma_tx(ctrl, tp);
			if (IS_ERR_VALUE(len)) {
				pr_err("%s: failed to call cmd_dma_tx for cmd = 0x%x\n",
					__func__,  cmds->payload[0]);
				goto dsi_cmds_tx_err;
			}

			if (dchdr->wait)
				usleep_range(dchdr->wait * 1000, dchdr->wait * 1000);

			mdss_dsi_buf_init(tp);
			len = 0;
		}
		cm++;
	}

dsi_cmds_tx_err:
	return rc;
}

static int msm_dsi_parse_rx_response(struct dsi_buf *rp)
{
	int rc = 0;
	unsigned char cmd;

	cmd = rp->data[0];
	switch (cmd) {
	case DTYPE_ACK_ERR_RESP:
		pr_debug("%s: rx ACK_ERR_PACLAGE\n", __func__);
		rc = -EINVAL;
		break;
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
		rc = -EINVAL;
		pr_warn("%s: Unknown cmd received\n", __func__);
		break;
	}

	return rc;
}

/* MIPI_DSI_MRPS, Maximum Return Packet Size */
static char max_pktsize[2] = {0x00, 0x00}; /* LSB tx first, 10 bytes */

static struct dsi_cmd_desc pkt_size_cmd = {
	{DTYPE_MAX_PKTSIZE, 1, 0, 0, 0, sizeof(max_pktsize)},
	max_pktsize,
};

static int msm_dsi_set_max_packet_size(struct mdss_dsi_ctrl_pdata *ctrl,
						int size)
{
	struct dsi_buf *tp;
	int rc;

	tp = &ctrl->tx_buf;
	mdss_dsi_buf_init(tp);
	max_pktsize[0] = size;

	rc = mdss_dsi_cmd_dma_add(tp, &pkt_size_cmd);
	if (!rc) {
		pr_err("%s: failed to add max_pkt_size\n", __func__);
		return -EINVAL;
	}

	rc = msm_dsi_wait4video_eng_busy(ctrl);
	if (rc) {
		pr_err("%s: failed to wait4video_eng\n", __func__);
		return rc;
	}

	rc = msm_dsi_cmd_dma_tx(ctrl, tp);
	if (IS_ERR_VALUE(rc)) {
		pr_err("%s: failed to tx max_pkt_size\n", __func__);
		return rc;
	}
	pr_debug("%s: max_pkt_size=%d sent\n", __func__, size);
	return rc;
}

/* read data length is less than or equal to 10 bytes*/
static int msm_dsi_cmds_rx_1(struct mdss_dsi_ctrl_pdata *ctrl,
				struct dsi_cmd_desc *cmds, int rlen)
{
	int rc;
	struct dsi_buf *tp, *rp;

	tp = &ctrl->tx_buf;
	rp = &ctrl->rx_buf;
	mdss_dsi_buf_init(rp);
	mdss_dsi_buf_init(tp);

	rc = mdss_dsi_cmd_dma_add(tp, cmds);
	if (!rc) {
		pr_err("%s: dsi_cmd_dma_add failed\n", __func__);
		rc = -EINVAL;
		goto dsi_cmds_rx_1_error;
	}

	rc = msm_dsi_wait4video_eng_busy(ctrl);
	if (rc) {
		pr_err("%s: wait4video_eng failed\n", __func__);
		goto dsi_cmds_rx_1_error;
	}

	rc = msm_dsi_cmd_dma_tx(ctrl, tp);
	if (IS_ERR_VALUE(rc)) {
		pr_err("%s: msm_dsi_cmd_dma_tx failed\n", __func__);
		goto dsi_cmds_rx_1_error;
	}

	if (rlen <= DSI_SHORT_PKT_DATA_SIZE) {
		msm_dsi_cmd_dma_rx(ctrl, rp, rlen);
	} else {
		msm_dsi_cmd_dma_rx(ctrl, rp, rlen + DSI_HOST_HDR_SIZE);
		rp->len = rlen + DSI_HOST_HDR_SIZE;
	}
	rc = msm_dsi_parse_rx_response(rp);

dsi_cmds_rx_1_error:
	if (rc)
		rp->len = 0;

	return rc;
}

/* read data length is more than 10 bytes, which requires multiple DSI read*/
static int msm_dsi_cmds_rx_2(struct mdss_dsi_ctrl_pdata *ctrl,
				struct dsi_cmd_desc *cmds, int rlen)
{
	int rc;
	struct dsi_buf *tp, *rp;
	int pkt_size, data_bytes, total;

	tp = &ctrl->tx_buf;
	rp = &ctrl->rx_buf;
	mdss_dsi_buf_init(rp);
	pkt_size = DSI_MAX_PKT_SIZE;
	data_bytes = MDSS_DSI_LEN;
	total = 0;

	while (true) {
		rc = msm_dsi_set_max_packet_size(ctrl, pkt_size);
		if (rc)
			break;

		mdss_dsi_buf_init(tp);
		rc = mdss_dsi_cmd_dma_add(tp, cmds);
		if (!rc) {
			pr_err("%s: dsi_cmd_dma_add failed\n", __func__);
			rc = -EINVAL;
			break;
	}
		rc = msm_dsi_wait4video_eng_busy(ctrl);
		if (rc) {
			pr_err("%s: wait4video_eng failed\n", __func__);
			break;
		}

		rc = msm_dsi_cmd_dma_tx(ctrl, tp);
		if (IS_ERR_VALUE(rc)) {
			pr_err("%s: msm_dsi_cmd_dma_tx failed\n", __func__);
			break;
		}

		msm_dsi_cmd_dma_rx(ctrl, rp, DSI_MAX_BYTES_TO_READ);

		rp->data += DSI_MAX_BYTES_TO_READ - DSI_HOST_HDR_SIZE;
		total += data_bytes;
		if (total >= rlen)
			break;

		data_bytes = DSI_MAX_BYTES_TO_READ - DSI_HOST_HDR_SIZE;
		pkt_size += data_bytes;
	}

	if (!rc) {
		rp->data = rp->start;
		rp->len = rlen + DSI_HOST_HDR_SIZE;
		rc = msm_dsi_parse_rx_response(rp);
	}

	if (rc)
		rp->len = 0;

	return rc;
}

int msm_dsi_cmds_rx(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_cmd_desc *cmds, int rlen)
{
	int rc;
	if (rlen <= DSI_MAX_PKT_SIZE)
		rc = msm_dsi_cmds_rx_1(ctrl, cmds, rlen);
	else
		rc = msm_dsi_cmds_rx_2(ctrl, cmds, rlen);

	return rc;
}

void msm_dsi_cmdlist_tx(struct mdss_dsi_ctrl_pdata *ctrl,
				struct dcs_cmd_req *req)
{
	int ret;

	ret = msm_dsi_cmds_tx(ctrl, req->cmds, req->cmds_cnt);

	if (req->cb)
		req->cb(ret);
}

void msm_dsi_cmdlist_rx(struct mdss_dsi_ctrl_pdata *ctrl,
				struct dcs_cmd_req *req)
{
	struct dsi_buf *rp;
	int len = 0;

	if (req->rbuf) {
		rp = &ctrl->rx_buf;
		len = msm_dsi_cmds_rx(ctrl, req->cmds, req->rlen);
		memcpy(req->rbuf, rp->data, rp->len);
	} else {
		pr_err("%s: No rx buffer provided\n", __func__);
	}

	if (req->cb)
		req->cb(len);
}
int msm_dsi_cmdlist_commit(struct mdss_dsi_ctrl_pdata *ctrl, int from_mdp)
{
	struct dcs_cmd_req *req;
	int dsi_on;
	int ret = -EINVAL;

	mutex_lock(&ctrl->mutex);
	dsi_on = dsi_host_private->dsi_on;
	mutex_unlock(&ctrl->mutex);
	if (!dsi_on) {
		pr_err("try to send DSI commands while dsi is off\n");
		return ret;
	}

	if (from_mdp)	/* from mdp kickoff */
		mutex_lock(&ctrl->cmd_mutex);
	req = mdss_dsi_cmdlist_get(ctrl, from_mdp);

	if (!req) {
		mutex_unlock(&ctrl->cmd_mutex);
		return ret;
	}
	/*
	 * mdss interrupt is generated in mdp core clock domain
	 * mdp clock need to be enabled to receive dsi interrupt
	 * also, axi bus bandwidth need since dsi controller will
	 * fetch dcs commands from axi bus
	 */
	mdp3_res_update(1, 1, MDP3_CLIENT_DMA_P);
	msm_dsi_clk_ctrl(&ctrl->panel_data, 1);

	if (0 == (req->flags & CMD_REQ_LP_MODE))
		dsi_set_tx_power_mode(0);

	if (req->flags & CMD_REQ_RX)
		msm_dsi_cmdlist_rx(ctrl, req);
	else
		msm_dsi_cmdlist_tx(ctrl, req);

	if (0 == (req->flags & CMD_REQ_LP_MODE))
		dsi_set_tx_power_mode(1);

	msm_dsi_clk_ctrl(&ctrl->panel_data, 0);
	mdp3_res_update(0, 1, MDP3_CLIENT_DMA_P);

	if (from_mdp)	/* from mdp kickoff */
		mutex_unlock(&ctrl->cmd_mutex);
	return 0;
}

static int msm_dsi_cal_clk_rate(struct mdss_panel_data *pdata,
				u64 *bitclk_rate,
				u32 *dsiclk_rate,
				u32 *byteclk_rate,
				u32 *pclk_rate)
{
	struct mdss_panel_info *pinfo;
	struct mipi_panel_info *mipi;
	u32 hbp, hfp, vbp, vfp, hspw, vspw, width, height;
	int lanes;
	u64 clk_rate;

	pinfo = &pdata->panel_info;
	mipi  = &pdata->panel_info.mipi;

	hbp = pdata->panel_info.lcdc.h_back_porch;
	hfp = pdata->panel_info.lcdc.h_front_porch;
	vbp = pdata->panel_info.lcdc.v_back_porch;
	vfp = pdata->panel_info.lcdc.v_front_porch;
	hspw = pdata->panel_info.lcdc.h_pulse_width;
	vspw = pdata->panel_info.lcdc.v_pulse_width;
	width = pdata->panel_info.xres;
	height = pdata->panel_info.yres;

	lanes = 0;
	if (mipi->data_lane0)
		lanes++;
	if (mipi->data_lane1)
		lanes++;
	if (mipi->data_lane2)
		lanes++;
	if (mipi->data_lane3)
		lanes++;
	if (lanes == 0)
		return -EINVAL;

	*bitclk_rate = (width + hbp + hfp + hspw) * (height + vbp + vfp + vspw);
	*bitclk_rate *= mipi->frame_rate;
	*bitclk_rate *= pdata->panel_info.bpp;
	do_div(*bitclk_rate, lanes);
	clk_rate = *bitclk_rate;

	do_div(clk_rate, 8U);
	*byteclk_rate = (u32) clk_rate;
	*dsiclk_rate = *byteclk_rate * lanes;
	*pclk_rate = *byteclk_rate * lanes * 8 / pdata->panel_info.bpp;

	pr_debug("dsiclk_rate=%u, byteclk=%u, pck_=%u\n",
		*dsiclk_rate, *byteclk_rate, *pclk_rate);
	return 0;
}

static int msm_dsi_on(struct mdss_panel_data *pdata)
{
	int ret = 0, i;
	u64 clk_rate;
	struct mdss_panel_info *pinfo;
	struct mipi_panel_info *mipi;
	u32 hbp, hfp, vbp, vfp, hspw, vspw, width, height;
	u32 ystride, bpp, data;
	u32 dummy_xres, dummy_yres;
	u64 bitclk_rate = 0
	u32 byteclk_rate = 0, pclk_rate = 0, dsiclk_rate = 0;
	unsigned char *ctrl_base = dsi_host_private->dsi_base;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	pr_debug("msm_dsi_on\n");

	pinfo = &pdata->panel_info;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	mutex_lock(&ctrl_pdata->mutex);


	if (!pdata->panel_info.dynamic_switch_pending) {
		for (i = 0; !ret && (i < DSI_MAX_PM); i++) {
			ret = msm_dss_enable_vreg(
				ctrl_pdata->power_data[i].vreg_config,
				ctrl_pdata->power_data[i].num_vreg, 1);
			if (ret) {
				pr_err("%s: failed to enable vregs for %s\n",
					__func__, __mdss_dsi_pm_name(i));
				goto error_vreg;
			}
		}
	}

	msm_dsi_ahb_ctrl(1);
	msm_dsi_phy_sw_reset(dsi_host_private->dsi_base);
	msm_dsi_phy_init(dsi_host_private->dsi_base, pdata);

	msm_dsi_cal_clk_rate(pdata, &bitclk_rate, &dsiclk_rate,
				&byteclk_rate, &pclk_rate);
	msm_dsi_clk_set_rate(DSI_ESC_CLK_RATE, dsiclk_rate,
				byteclk_rate, pclk_rate);
	msm_dsi_prepare_clocks();
	msm_dsi_clk_enable();

	clk_rate = pdata->panel_info.clk_rate;
	clk_rate = min(clk_rate, pdata->panel_info.clk_max);

	hbp = pdata->panel_info.lcdc.h_back_porch;
	hfp = pdata->panel_info.lcdc.h_front_porch;
	vbp = pdata->panel_info.lcdc.v_back_porch;
	vfp = pdata->panel_info.lcdc.v_front_porch;
	hspw = pdata->panel_info.lcdc.h_pulse_width;
	vspw = pdata->panel_info.lcdc.v_pulse_width;
	width = pdata->panel_info.xres;
	height = pdata->panel_info.yres;

	mipi  = &pdata->panel_info.mipi;
	if (pdata->panel_info.type == MIPI_VIDEO_PANEL) {
		dummy_xres = pdata->panel_info.lcdc.xres_pad;
		dummy_yres = pdata->panel_info.lcdc.yres_pad;

		MIPI_OUTP(ctrl_base + DSI_VIDEO_MODE_ACTIVE_H,
			((hspw + hbp + width + dummy_xres) << 16 |
			(hspw + hbp)));
		MIPI_OUTP(ctrl_base + DSI_VIDEO_MODE_ACTIVE_V,
			((vspw + vbp + height + dummy_yres) << 16 |
			(vspw + vbp)));
		MIPI_OUTP(ctrl_base + DSI_VIDEO_MODE_TOTAL,
			(vspw + vbp + height + dummy_yres +
				vfp - 1) << 16 | (hspw + hbp +
				width + dummy_xres + hfp - 1));

		MIPI_OUTP(ctrl_base + DSI_VIDEO_MODE_HSYNC, (hspw << 16));
		MIPI_OUTP(ctrl_base + DSI_VIDEO_MODE_VSYNC, 0);
		MIPI_OUTP(ctrl_base + DSI_VIDEO_MODE_VSYNC_VPOS,
				(vspw << 16));

	} else {		/* command mode */
		if (mipi->dst_format == DSI_CMD_DST_FORMAT_RGB888)
			bpp = 3;
		else if (mipi->dst_format == DSI_CMD_DST_FORMAT_RGB666)
			bpp = 3;
		else if (mipi->dst_format == DSI_CMD_DST_FORMAT_RGB565)
			bpp = 2;
		else
			bpp = 3;	/* Default format set to RGB888 */

		ystride = width * bpp + 1;

		data = (ystride << 16) | (mipi->vc << 8) | DTYPE_DCS_LWRITE;
		MIPI_OUTP(ctrl_base + DSI_COMMAND_MODE_MDP_STREAM0_CTRL,
			data);
		MIPI_OUTP(ctrl_base + DSI_COMMAND_MODE_MDP_STREAM1_CTRL,
			data);

		data = height << 16 | width;
		MIPI_OUTP(ctrl_base + DSI_COMMAND_MODE_MDP_STREAM1_TOTAL,
			data);
		MIPI_OUTP(ctrl_base + DSI_COMMAND_MODE_MDP_STREAM0_TOTAL,
			data);
	}

	msm_dsi_sw_reset();
	msm_dsi_host_init(pdata);

	if (mipi->force_clk_lane_hs) {
		u32 tmp;

		tmp = MIPI_INP(ctrl_base + DSI_LANE_CTRL);
		tmp |= (1<<28);
		MIPI_OUTP(ctrl_base + DSI_LANE_CTRL, tmp);
		wmb();
	}

	msm_dsi_op_mode_config(mipi->mode, pdata);

	msm_dsi_set_irq(ctrl_pdata, DSI_INTR_ERROR_MASK);
	dsi_host_private->clk_count = 1;
	dsi_host_private->dsi_on = 1;

error_vreg:
	if (ret) {
		for (; i >= 0; i--)
			msm_dss_enable_vreg(
				ctrl_pdata->power_data[i].vreg_config,
				ctrl_pdata->power_data[i].num_vreg, 0);
	}

	mutex_unlock(&ctrl_pdata->mutex);
	return ret;
}

static int msm_dsi_off(struct mdss_panel_data *pdata)
{
	int ret = 0, i;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		ret = -EINVAL;
		return ret;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_debug("msm_dsi_off\n");
	mutex_lock(&ctrl_pdata->mutex);
	msm_dsi_clear_irq(ctrl_pdata, ctrl_pdata->dsi_irq_mask);
	msm_dsi_controller_cfg(0);
	msm_dsi_clk_set_rate(DSI_ESC_CLK_RATE, 0, 0, 0);
	msm_dsi_clk_disable();
	msm_dsi_unprepare_clocks();
	msm_dsi_phy_off(dsi_host_private->dsi_base);
	msm_dsi_ahb_ctrl(0);

	if (!pdata->panel_info.dynamic_switch_pending) {
		for (i = DSI_MAX_PM - 1; i >= 0; i--) {
			ret = msm_dss_enable_vreg(
				ctrl_pdata->power_data[i].vreg_config,
				ctrl_pdata->power_data[i].num_vreg, 0);
			if (ret)
				pr_err("%s: failed to disable vregs for %s\n",
					__func__, __mdss_dsi_pm_name(i));
		}
	}
	dsi_host_private->clk_count = 0;
	dsi_host_private->dsi_on = 0;

	mutex_unlock(&ctrl_pdata->mutex);

	return ret;
}

static int msm_dsi_cont_on(struct mdss_panel_data *pdata)
{
	struct mdss_panel_info *pinfo;
	int ret = 0, i;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		ret = -EINVAL;
		return ret;
	}


	pr_debug("%s:\n", __func__);

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pinfo = &pdata->panel_info;
	mutex_lock(&ctrl_pdata->mutex);
	for (i = 0; !ret && (i < DSI_MAX_PM); i++) {
		ret = msm_dss_enable_vreg(
			ctrl_pdata->power_data[i].vreg_config,
			ctrl_pdata->power_data[i].num_vreg, 1);
		if (ret) {
			pr_err("%s: failed to enable vregs for %s\n",
				__func__, __mdss_dsi_pm_name(i));
			goto error_vreg;
		}
	}
	pinfo->panel_power_state = MDSS_PANEL_POWER_ON;
	ret = mdss_dsi_panel_reset(pdata, 1);
	if (ret) {
		pr_err("%s: Panel reset failed\n", __func__);
		mutex_unlock(&ctrl_pdata->mutex);
		return ret;
	}

	msm_dsi_ahb_ctrl(1);
	msm_dsi_prepare_clocks();
	msm_dsi_clk_enable();
	msm_dsi_set_irq(ctrl_pdata, DSI_INTR_ERROR_MASK);
	dsi_host_private->clk_count = 1;
	dsi_host_private->dsi_on = 1;

error_vreg:
	if (ret) {
		for (; i >= 0; i--)
			msm_dss_enable_vreg(
				ctrl_pdata->power_data[i].vreg_config,
				ctrl_pdata->power_data[i].num_vreg, 0);
	}

	mutex_unlock(&ctrl_pdata->mutex);
	return ret;
}

static int msm_dsi_read_status(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct dcs_cmd_req cmdreq;

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = ctrl->status_cmds.cmds;
	cmdreq.cmds_cnt = ctrl->status_cmds.cmd_cnt;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL | CMD_REQ_RX;
	cmdreq.rlen = 1;
	cmdreq.cb = NULL;
	cmdreq.rbuf = ctrl->status_buf.data;

	return mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}


/**
 * msm_dsi_reg_status_check() - Check dsi panel status through reg read
 * @ctrl_pdata: pointer to the dsi controller structure
 *
 * This function can be used to check the panel status through reading the
 * status register from the panel.
 *
 * Return: positive value if the panel is in good state, negative value or
 * zero otherwise.
 */
int msm_dsi_reg_status_check(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int ret = 0;

	if (ctrl_pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return 0;
	}

	pr_debug("%s: Checking Register status\n", __func__);

	msm_dsi_clk_ctrl(&ctrl_pdata->panel_data, 1);

	if (ctrl_pdata->status_cmds.link_state == DSI_HS_MODE)
		dsi_set_tx_power_mode(0);

	ret = msm_dsi_read_status(ctrl_pdata);

	if (ctrl_pdata->status_cmds.link_state == DSI_HS_MODE)
		dsi_set_tx_power_mode(1);

	if (ret == 0) {
		if (!mdss_dsi_cmp_panel_reg(ctrl_pdata->status_buf,
			ctrl_pdata->status_value, 0)) {
			pr_err("%s: Read back value from panel is incorrect\n",
								__func__);
			ret = -EINVAL;
		} else {
			ret = 1;
		}
	} else {
		pr_err("%s: Read status register returned error\n", __func__);
	}

	msm_dsi_clk_ctrl(&ctrl_pdata->panel_data, 0);
	pr_debug("%s: Read register done with ret: %d\n", __func__, ret);

	return ret;
}

/**
 * msm_dsi_bta_status_check() - Check dsi panel status through bta check
 * @ctrl_pdata: pointer to the dsi controller structure
 *
 * This function can be used to check status of the panel using bta check
 * for the panel.
 *
 * Return: positive value if the panel is in good state, negative value or
 * zero otherwise.
 */
static int msm_dsi_bta_status_check(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int ret = 0;

	if (ctrl_pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return 0;
	}

	mutex_lock(&ctrl_pdata->cmd_mutex);
	msm_dsi_clk_ctrl(&ctrl_pdata->panel_data, 1);
	msm_dsi_cmd_mdp_busy(ctrl_pdata);
	msm_dsi_set_irq(ctrl_pdata, DSI_INTR_BTA_DONE_MASK);
	reinit_completion(&ctrl_pdata->bta_comp);

	/* BTA trigger */
	MIPI_OUTP(dsi_host_private->dsi_base + DSI_CMD_MODE_BTA_SW_TRIGGER,
									0x01);
	wmb();
	ret = wait_for_completion_killable_timeout(&ctrl_pdata->bta_comp,
									HZ/10);
	msm_dsi_clear_irq(ctrl_pdata, DSI_INTR_BTA_DONE_MASK);
	msm_dsi_clk_ctrl(&ctrl_pdata->panel_data, 0);
	mutex_unlock(&ctrl_pdata->cmd_mutex);

	if (ret <= 0)
		pr_err("%s: DSI BTA error: %i\n", __func__, __LINE__);

	pr_debug("%s: BTA done with ret: %d\n", __func__, ret);
	return ret;
}

static void msm_dsi_debug_enable_clock(int on)
{
	if (dsi_host_private->debug_enable_clk)
		dsi_host_private->debug_enable_clk(on);

	if (on)
		msm_dsi_ahb_ctrl(1);
	else
		msm_dsi_ahb_ctrl(0);
}

static int msm_dsi_debug_init(void)
{
	int rc;

	if (!mdss_res)
		return 0;

	dsi_host_private->debug_enable_clk =
			mdss_res->debug_inf.debug_enable_clock;

	mdss_res->debug_inf.debug_enable_clock = msm_dsi_debug_enable_clock;


	rc = mdss_debug_register_base("dsi0",
				dsi_host_private->dsi_base,
				dsi_host_private->dsi_reg_size,
				NULL);

	return rc;
}

static int dsi_get_panel_cfg(char *panel_cfg)
{
	int rc;
	struct mdss_panel_cfg *pan_cfg = NULL;

	if (!panel_cfg)
		return MDSS_PANEL_INTF_INVALID;

	pan_cfg = mdp3_panel_intf_type(MDSS_PANEL_INTF_DSI);
	if (IS_ERR(pan_cfg)) {
		panel_cfg[0] = 0;
		return PTR_ERR(pan_cfg);
	} else if (!pan_cfg) {
		panel_cfg[0] = 0;
		return 0;
	}

	pr_debug("%s:%d: cfg:[%s]\n", __func__, __LINE__,
		 pan_cfg->arg_cfg);
	rc = strlcpy(panel_cfg, pan_cfg->arg_cfg,
				MDSS_MAX_PANEL_LEN);
	return rc;
}

static struct device_node *dsi_pref_prim_panel(
		struct platform_device *pdev)
{
	struct device_node *dsi_pan_node = NULL;

	pr_debug("%s:%d: Select primary panel from dt\n",
					__func__, __LINE__);
	dsi_pan_node = of_parse_phandle(pdev->dev.of_node,
					"qcom,dsi-pref-prim-pan", 0);
	if (!dsi_pan_node)
		pr_err("%s:can't find panel phandle\n", __func__);

	return dsi_pan_node;
}

/**
 * dsi_find_panel_of_node(): find device node of dsi panel
 * @pdev: platform_device of the dsi ctrl node
 * @panel_cfg: string containing intf specific config data
 *
 * Function finds the panel device node using the interface
 * specific configuration data. This configuration data is
 * could be derived from the result of bootloader's GCDB
 * panel detection mechanism. If such config data doesn't
 * exist then this panel returns the default panel configured
 * in the device tree.
 *
 * returns pointer to panel node on success, NULL on error.
 */
static struct device_node *dsi_find_panel_of_node(
		struct platform_device *pdev, char *panel_cfg)
{
	int l;
	char *panel_name;
	struct device_node *dsi_pan_node = NULL, *mdss_node = NULL;

	if (!panel_cfg)
		return NULL;

	l = strlen(panel_cfg);
	if (!l) {
		/* no panel cfg chg, parse dt */
		pr_debug("%s:%d: no cmd line cfg present\n",
			 __func__, __LINE__);
		dsi_pan_node = dsi_pref_prim_panel(pdev);
	} else {
		if (panel_cfg[0] != '0') {
			pr_err("%s:%d:ctrl id=[%d] not supported\n",
			       __func__, __LINE__, panel_cfg[0]);
			return NULL;
		}
		/*
		 * skip first two chars '<dsi_ctrl_id>' and
		 * ':' to get to the panel name
		 */
		panel_name = panel_cfg + 2;
		pr_debug("%s:%d:%s:%s\n", __func__, __LINE__,
			 panel_cfg, panel_name);

		mdss_node = of_parse_phandle(pdev->dev.of_node,
					     "qcom,mdss-mdp", 0);

		if (!mdss_node) {
			pr_err("%s: %d: mdss_node null\n",
			       __func__, __LINE__);
			return NULL;
		}
		dsi_pan_node = of_find_node_by_name(mdss_node,
						    panel_name);
		if (!dsi_pan_node) {
			pr_err("%s: invalid pan node\n",
			       __func__);
			dsi_pan_node = dsi_pref_prim_panel(pdev);
		}
	}
	return dsi_pan_node;
}

static int msm_dsi_clk_ctrl(struct mdss_panel_data *pdata, int enable)
{
	u32 bitclk_rate = 0, byteclk_rate = 0, pclk_rate = 0, dsiclk_rate = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	pr_debug("%s:\n", __func__);

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	mutex_lock(&ctrl_pdata->mutex);

	if (enable) {
		dsi_host_private->clk_count++;
		if (dsi_host_private->clk_count == 1) {
			msm_dsi_ahb_ctrl(1);
			msm_dsi_cal_clk_rate(pdata, &bitclk_rate, &dsiclk_rate,
						&byteclk_rate, &pclk_rate);
			msm_dsi_clk_set_rate(DSI_ESC_CLK_RATE, dsiclk_rate,
						byteclk_rate, pclk_rate);
			msm_dsi_prepare_clocks();
			msm_dsi_clk_enable();
		}
	} else {
		dsi_host_private->clk_count--;
		if (dsi_host_private->clk_count == 0) {
			msm_dsi_clear_irq(ctrl_pdata, ctrl_pdata->dsi_irq_mask);
			msm_dsi_clk_set_rate(DSI_ESC_CLK_RATE, 0, 0, 0);
			msm_dsi_clk_disable();
			msm_dsi_unprepare_clocks();
			msm_dsi_ahb_ctrl(0);
		}
	}
	mutex_unlock(&ctrl_pdata->mutex);
	return 0;
}

void msm_dsi_ctrl_init(struct mdss_dsi_ctrl_pdata *ctrl)
{
	init_completion(&ctrl->dma_comp);
	init_completion(&ctrl->mdp_comp);
	init_completion(&ctrl->bta_comp);
	init_completion(&ctrl->video_comp);
	spin_lock_init(&ctrl->irq_lock);
	spin_lock_init(&ctrl->mdp_lock);
	mutex_init(&ctrl->mutex);
	mutex_init(&ctrl->cmd_mutex);
	complete(&ctrl->mdp_comp);
	dsi_buf_alloc(&ctrl->tx_buf, SZ_4K);
	dsi_buf_alloc(&ctrl->rx_buf, SZ_4K);
	dsi_buf_alloc(&ctrl->status_buf, SZ_4K);
	ctrl->cmdlist_commit = msm_dsi_cmdlist_commit;
	ctrl->panel_mode = ctrl->panel_data.panel_info.mipi.mode;

	if (ctrl->status_mode == ESD_REG)
		ctrl->check_status = msm_dsi_reg_status_check;
	else if (ctrl->status_mode == ESD_BTA)
		ctrl->check_status = msm_dsi_bta_status_check;

	if (ctrl->status_mode == ESD_MAX) {
		pr_err("%s: Using default BTA for ESD check\n", __func__);
		ctrl->check_status = msm_dsi_bta_status_check;
	}
}

static void msm_dsi_parse_lane_swap(struct device_node *np, char *dlane_swap)
{
	const char *data;

	*dlane_swap = DSI_LANE_MAP_0123;
	data = of_get_property(np, "qcom,lane-map", NULL);
	if (data) {
		if (!strcmp(data, "lane_map_3012"))
			*dlane_swap = DSI_LANE_MAP_3012;
		else if (!strcmp(data, "lane_map_2301"))
			*dlane_swap = DSI_LANE_MAP_2301;
		else if (!strcmp(data, "lane_map_1230"))
			*dlane_swap = DSI_LANE_MAP_1230;
		else if (!strcmp(data, "lane_map_0321"))
			*dlane_swap = DSI_LANE_MAP_0321;
		else if (!strcmp(data, "lane_map_1032"))
			*dlane_swap = DSI_LANE_MAP_1032;
		else if (!strcmp(data, "lane_map_2103"))
			*dlane_swap = DSI_LANE_MAP_2103;
		else if (!strcmp(data, "lane_map_3210"))
			*dlane_swap = DSI_LANE_MAP_3210;
	}
}

static int msm_dsi_probe(struct platform_device *pdev)
{
	struct dsi_interface intf;
	char panel_cfg[MDSS_MAX_PANEL_LEN];
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	int rc = 0;
	struct device_node *dsi_pan_node = NULL;
	bool cmd_cfg_cont_splash = false;
	struct resource *mdss_dsi_mres;
	int i;

	pr_debug("%s\n", __func__);

	rc = msm_dsi_init();
	if (rc)
		return rc;

	if (!pdev->dev.of_node) {
		pr_err("%s: Device node is not accessible\n", __func__);
		rc = -ENODEV;
		goto error_no_mem;
	}
	pdev->id = 0;

	ctrl_pdata = platform_get_drvdata(pdev);
	if (!ctrl_pdata) {
		ctrl_pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct mdss_dsi_ctrl_pdata), GFP_KERNEL);
		if (!ctrl_pdata) {
			pr_err("%s: FAILED: cannot alloc dsi ctrl\n", __func__);
			rc = -ENOMEM;
			goto error_no_mem;
		}
		platform_set_drvdata(pdev, ctrl_pdata);
	}

	ctrl_pdata->mdss_util = mdss_get_util_intf();
	if (mdp3_res->mdss_util == NULL) {
		pr_err("Failed to get mdss utility functions\n");
		return -ENODEV;
	}

	mdss_dsi_mres = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mdss_dsi_mres) {
		pr_err("%s:%d unable to get the MDSS reg resources",
							__func__, __LINE__);
		rc = -ENOMEM;
		goto error_io_resource;
	} else {
		dsi_host_private->dsi_reg_size = resource_size(mdss_dsi_mres);
		dsi_host_private->dsi_base = ioremap(mdss_dsi_mres->start,
						dsi_host_private->dsi_reg_size);
		if (!dsi_host_private->dsi_base) {
			pr_err("%s:%d unable to remap dsi resources",
							__func__, __LINE__);
			rc = -ENOMEM;
			goto error_io_resource;
		}
	}

	mdss_dsi_mres = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!mdss_dsi_mres || mdss_dsi_mres->start == 0) {
		pr_err("%s:%d unable to get the MDSS irq resources",
							__func__, __LINE__);
		rc = -ENODEV;
		goto error_irq_resource;
	}

	rc = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (rc) {
		dev_err(&pdev->dev, "%s: failed to add child nodes, rc=%d\n",
								__func__, rc);
		goto error_platform_pop;
	}

	/* DSI panels can be different between controllers */
	rc = dsi_get_panel_cfg(panel_cfg);
	if (!rc)
		/* dsi panel cfg not present */
		pr_warn("%s:%d:dsi specific cfg not present\n",
							 __func__, __LINE__);

	/* find panel device node */
	dsi_pan_node = dsi_find_panel_of_node(pdev, panel_cfg);
	if (!dsi_pan_node) {
		pr_err("%s: can't find panel node %s\n", __func__,
								panel_cfg);
		goto error_pan_node;
	}

	cmd_cfg_cont_splash = mdp3_panel_get_boot_cfg() ? true : false;

	rc = mdss_dsi_panel_init(dsi_pan_node, ctrl_pdata, cmd_cfg_cont_splash);
	if (rc) {
		pr_err("%s: dsi panel init failed\n", __func__);
		goto error_pan_node;
	}

	rc = dsi_ctrl_config_init(pdev, ctrl_pdata);
	if (rc) {
		dev_err(&pdev->dev, "%s: failed to parse mdss dtsi rc=%d\n",
								__func__, rc);
		goto error_pan_node;
	}

	msm_dsi_parse_lane_swap(pdev->dev.of_node, &(ctrl_pdata->dlane_swap));

	for (i = 0;  i < DSI_MAX_PM; i++) {
		rc = msm_dsi_io_init(pdev, &(ctrl_pdata->power_data[i]));
		if (rc) {
			dev_err(&pdev->dev, "%s: failed to init IO for %s\n",
				__func__, __mdss_dsi_pm_name(i));
			goto error_io_init;
		}
	}

	pr_debug("%s: Dsi Ctrl->0 initialized\n", __func__);

	dsi_host_private->dis_dev = pdev->dev;
	intf.on = msm_dsi_on;
	intf.off = msm_dsi_off;
	intf.cont_on = msm_dsi_cont_on;
	intf.clk_ctrl = msm_dsi_clk_ctrl;
	intf.op_mode_config = msm_dsi_op_mode_config;
	intf.index = 0;
	intf.private = NULL;
	dsi_register_interface(&intf);

	msm_dsi_debug_init();

	msm_dsi_ctrl_init(ctrl_pdata);

	rc = msm_dsi_irq_init(&pdev->dev, mdss_dsi_mres->start,
					   ctrl_pdata);
	if (rc) {
		dev_err(&pdev->dev, "%s: failed to init irq, rc=%d\n",
			__func__, rc);
		goto error_irq_init;
	}

	rc = dsi_panel_device_register_v2(pdev, ctrl_pdata);
	if (rc) {
		pr_err("%s: dsi panel dev reg failed\n", __func__);
		goto error_device_register;
	}
	pr_debug("%s success\n", __func__);
	return 0;
error_device_register:
	kfree(ctrl_pdata->dsi_hw->irq_info);
	kfree(ctrl_pdata->dsi_hw);
error_irq_init:
	for (i = DSI_MAX_PM - 1; i >= 0; i--)
		msm_dsi_io_deinit(pdev, &(ctrl_pdata->power_data[i]));
error_io_init:
	dsi_ctrl_config_deinit(pdev, ctrl_pdata);
error_pan_node:
	of_node_put(dsi_pan_node);
error_platform_pop:
	msm_dsi_clear_irq(ctrl_pdata, ctrl_pdata->dsi_irq_mask);
error_irq_resource:
	if (dsi_host_private->dsi_base) {
		iounmap(dsi_host_private->dsi_base);
		dsi_host_private->dsi_base = NULL;
	}
error_io_resource:
	devm_kfree(&pdev->dev, ctrl_pdata);
error_no_mem:
	msm_dsi_deinit();

	return rc;
}

static int msm_dsi_remove(struct platform_device *pdev)
{
	int i;

	struct mdss_dsi_ctrl_pdata *ctrl_pdata = platform_get_drvdata(pdev);
	if (!ctrl_pdata) {
		pr_err("%s: no driver data\n", __func__);
		return -ENODEV;
	}

	msm_dsi_clear_irq(ctrl_pdata, ctrl_pdata->dsi_irq_mask);
	for (i = DSI_MAX_PM - 1; i >= 0; i--)
		msm_dsi_io_deinit(pdev, &(ctrl_pdata->power_data[i]));
	dsi_ctrl_config_deinit(pdev, ctrl_pdata);
	iounmap(dsi_host_private->dsi_base);
	dsi_host_private->dsi_base = NULL;
	msm_dsi_deinit();
	devm_kfree(&pdev->dev, ctrl_pdata);

	return 0;
}

static const struct of_device_id msm_dsi_v2_dt_match[] = {
	{.compatible = "qcom,msm-dsi-v2"},
	{}
};
MODULE_DEVICE_TABLE(of, msm_dsi_v2_dt_match);

static struct platform_driver msm_dsi_v2_driver = {
	.probe = msm_dsi_probe,
	.remove = msm_dsi_remove,
	.shutdown = NULL,
	.driver = {
		.name = "msm_dsi_v2",
		.of_match_table = msm_dsi_v2_dt_match,
	},
};

static int msm_dsi_v2_register_driver(void)
{
	return platform_driver_register(&msm_dsi_v2_driver);
}

static int __init msm_dsi_v2_driver_init(void)
{
	int ret;

	ret = msm_dsi_v2_register_driver();
	if (ret) {
		pr_err("msm_dsi_v2_register_driver() failed!\n");
		return ret;
	}

	return ret;
}
module_init(msm_dsi_v2_driver_init);

static void __exit msm_dsi_v2_driver_cleanup(void)
{
	platform_driver_unregister(&msm_dsi_v2_driver);
}
module_exit(msm_dsi_v2_driver_cleanup);
