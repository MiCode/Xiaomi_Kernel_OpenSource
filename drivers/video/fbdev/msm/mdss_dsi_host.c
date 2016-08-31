/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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

#include <linux/msm-bus.h>

#include "mdss.h"
#include "mdss_dsi.h"
#include "mdss_panel.h"
#include "mdss_debug.h"
#include "mdss_smmu.h"
#include "mdss_dsi_phy.h"

#define VSYNC_PERIOD 17
#define DMA_TX_TIMEOUT 200
#define DMA_TPG_FIFO_LEN 64

#define FIFO_STATUS	0x0C
#define LANE_STATUS	0xA8

#define MDSS_DSI_INT_CTRL	0x0110
#define LANE_SWAP_CTRL			0x0B0
#define LOGICAL_LANE_SWAP_CTRL		0x310

#define CEIL(x, y)		(((x) + ((y)-1)) / (y))

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

/* Mutex common for both the controllers */
static struct mutex dsi_mtx;

/* event */
struct dsi_event_q {
	struct mdss_dsi_ctrl_pdata *ctrl;
	u32 arg;
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

	if (!(ctrl->dsi_irq_line))
		ctrl->dsi_hw->irq_info = mdss_intr_line();

	ctrl->panel_mode = ctrl->panel_data.panel_info.mipi.mode;

	ctrl_list[ctrl->ndx] = ctrl;	/* keep it */

	if (ctrl->mdss_util->register_irq(ctrl->dsi_hw))
		pr_err("%s: mdss_register_irq failed.\n", __func__);

	pr_debug("%s: ndx=%d base=%pK\n", __func__, ctrl->ndx, ctrl->ctrl_base);

	init_completion(&ctrl->dma_comp);
	init_completion(&ctrl->mdp_comp);
	init_completion(&ctrl->video_comp);
	init_completion(&ctrl->dynamic_comp);
	init_completion(&ctrl->bta_comp);
	spin_lock_init(&ctrl->irq_lock);
	spin_lock_init(&ctrl->mdp_lock);
	mutex_init(&ctrl->mutex);
	mutex_init(&ctrl->cmd_mutex);
	mutex_init(&ctrl->clk_lane_mutex);
	mutex_init(&ctrl->cmdlist_mutex);
	mdss_dsi_buf_alloc(ctrl_dev, &ctrl->tx_buf, SZ_4K);
	mdss_dsi_buf_alloc(ctrl_dev, &ctrl->rx_buf, SZ_4K);
	mdss_dsi_buf_alloc(ctrl_dev, &ctrl->status_buf, SZ_4K);
	ctrl->cmdlist_commit = mdss_dsi_cmdlist_commit;
	ctrl->err_cont.err_time_delta = 100;
	ctrl->err_cont.max_err_index = MAX_ERR_INDEX;

	if (dsi_event.inited == 0) {
		kthread_run(dsi_event_thread, (void *)&dsi_event,
						"mdss_dsi_event");
		mutex_init(&dsi_mtx);
		dsi_event.inited  = 1;
	}
}

void mdss_dsi_set_reg(struct mdss_dsi_ctrl_pdata *ctrl, int off,
						u32 mask, u32 val)
{
	u32 data;

	off &= ~0x03;
	val &= mask;    /* set bits indicated at mask only */
	data = MIPI_INP(ctrl->ctrl_base + off);
	data &= ~mask;
	data |= val;
	pr_debug("%s: ndx=%d off=%x data=%x\n", __func__,
				ctrl->ndx, off, data);
	MIPI_OUTP(ctrl->ctrl_base + off, data);
}

void mdss_dsi_clk_req(struct mdss_dsi_ctrl_pdata *ctrl,
	struct dsi_panel_clk_ctrl *clk_ctrl)
{
	enum dsi_clk_req_client client = clk_ctrl->client;
	int enable = clk_ctrl->state;
	void *clk_handle = ctrl->mdp_clk_handle;

	if (clk_ctrl->client == DSI_CLK_REQ_DSI_CLIENT)
		clk_handle = ctrl->dsi_clk_handle;

	MDSS_XLOG(ctrl->ndx, enable, ctrl->mdp_busy, current->pid,
		client);
	if (enable == 0) {
		/* need wait before disable */
		mutex_lock(&ctrl->cmd_mutex);
		mdss_dsi_cmd_mdp_busy(ctrl);
		mutex_unlock(&ctrl->cmd_mutex);
	}

	MDSS_XLOG(ctrl->ndx, enable, ctrl->mdp_busy, current->pid,
		client);
	mdss_dsi_clk_ctrl(ctrl, clk_handle,
		  MDSS_DSI_ALL_CLKS, enable);
}

void mdss_dsi_pll_relock(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc;

	/*
	 * todo: this code does not work very well with dual
	 * dsi use cases. Need to fix this eventually.
	 */

	rc = mdss_dsi_clk_force_toggle(ctrl->dsi_clk_handle, MDSS_DSI_LINK_CLK);
	if (rc)
		pr_err("clock toggle failed, rc = %d\n", rc);
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
		ctrl->mdss_util->enable_irq(ctrl->dsi_hw);
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
		ctrl->mdss_util->disable_irq(ctrl->dsi_hw);
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
		ctrl->mdss_util->disable_irq_nosync(ctrl->dsi_hw);
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

void mdss_dsi_read_hw_revision(struct mdss_dsi_ctrl_pdata *ctrl)
{
	if (ctrl->shared_data->hw_rev)
		return;

	/* clock must be on */
	ctrl->shared_data->hw_rev = MIPI_INP(ctrl->ctrl_base);
}

void mdss_dsi_read_phy_revision(struct mdss_dsi_ctrl_pdata *ctrl)
{
	u32 reg_val;

	if (ctrl->shared_data->phy_rev > DSI_PHY_REV_UNKNOWN)
		return;

	reg_val = MIPI_INP(ctrl->phy_io.base);

	if (reg_val == DSI_PHY_REV_30)
		ctrl->shared_data->phy_rev = DSI_PHY_REV_30;
	else if (reg_val == DSI_PHY_REV_20)
		ctrl->shared_data->phy_rev = DSI_PHY_REV_20;
	else if (reg_val == DSI_PHY_REV_10)
		ctrl->shared_data->phy_rev = DSI_PHY_REV_10;
	else
		ctrl->shared_data->phy_rev = DSI_PHY_REV_UNKNOWN;
}

static void mdss_dsi_config_data_lane_swap(struct mdss_dsi_ctrl_pdata *ctrl)
{
	if (ctrl->shared_data->hw_rev < MDSS_DSI_HW_REV_200)
		MIPI_OUTP((ctrl->ctrl_base) + LANE_SWAP_CTRL, ctrl->dlane_swap);
	else
		MIPI_OUTP(ctrl->ctrl_base + LOGICAL_LANE_SWAP_CTRL,
			ctrl->lane_map[DSI_LOGICAL_LANE_0] |
			ctrl->lane_map[DSI_LOGICAL_LANE_1] << 4 |
			ctrl->lane_map[DSI_LOGICAL_LANE_2] << 8 |
			ctrl->lane_map[DSI_LOGICAL_LANE_3] << 12);
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

	mdss_dsi_config_data_lane_swap(ctrl_pdata);

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
	/*
	 * DSI_HS_TIMER_CTRL -> timer resolution = 8 esc clk
	 * HS TX timeout - 16136 (0x3f08) esc clk
	 */
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x00bc, 0x3fd08);


	/* allow only ack-err-status  to generate interrupt */
	/* DSI_ERR_INT_MASK0 */
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x010c, 0x03f03fc0);

	intr_ctrl |= DSI_INTR_ERROR_MASK;
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x0110,
				intr_ctrl); /* DSI_INTL_CTRL */

	/* turn esc, byte, dsi, pclk, sclk, hclk on */
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x11c,
					0x23f); /* DSI_CLK_CTRL */

	/* Reset DSI_LANE_CTRL */
	if (!ctrl_pdata->mmss_clamp)
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x00ac, 0x0);

	dsi_ctrl |= BIT(0);	/* enable dsi */
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x0004, dsi_ctrl);

	/* enable contention detection for receiving */
	mdss_dsi_lp_cd_rx(ctrl_pdata);

	/* set DMA FIFO read watermark to 15/16 full */
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x50, 0x30);

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

void mdss_dsi_sw_reset(struct mdss_dsi_ctrl_pdata *ctrl, bool restore)
{
	u32 data0;
	unsigned long flag;

	if (!ctrl) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	data0 = MIPI_INP(ctrl->ctrl_base + 0x0004);
	MIPI_OUTP(ctrl->ctrl_base + 0x0004, (data0 & ~BIT(0)));
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

	if (restore) {
		MIPI_OUTP(ctrl->ctrl_base + 0x0004, data0);
		wmb();	/* make sure dsi controller enabled again */
	}

	/* It is safe to clear mdp_busy as reset is happening */
	spin_lock_irqsave(&ctrl->mdp_lock, flag);
	ctrl->mdp_busy = false;
	complete_all(&ctrl->mdp_comp);
	spin_unlock_irqrestore(&ctrl->mdp_lock, flag);
}

/**
 * mdss_dsi_wait_for_lane_idle() - Wait for DSI lanes to be idle
 * @ctrl: pointer to DSI controller structure
 *
 * This function waits for all the active DSI lanes to be idle by polling all
 * the *FIFO_EMPTY bits and polling the lane status to ensure that all the lanes
 * are in stop state. This function assumes that the bus clocks required to
 * access the registers are already turned on.
 */
int mdss_dsi_wait_for_lane_idle(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc;
	u32 val;
	u32 fifo_empty_mask = 0;
	u32 stop_state_mask = 0;
	struct mipi_panel_info *mipi;
	u32 const sleep_us = 10;
	u32 const timeout_us = 100;

	if (!ctrl) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	mipi = &ctrl->panel_data.panel_info.mipi;

	if (mipi->data_lane0) {
		stop_state_mask |= BIT(0);
		fifo_empty_mask |= (BIT(12) | BIT(16));
	}
	if (mipi->data_lane1) {
		stop_state_mask |= BIT(1);
		fifo_empty_mask |= BIT(20);
	}
	if (mipi->data_lane2) {
		stop_state_mask |= BIT(2);
		fifo_empty_mask |= BIT(24);
	}
	if (mipi->data_lane3) {
		stop_state_mask |= BIT(3);
		fifo_empty_mask |= BIT(28);
	}

	pr_debug("%s: polling for fifo empty, mask=0x%08x\n", __func__,
		fifo_empty_mask);
	rc = readl_poll_timeout(ctrl->ctrl_base + FIFO_STATUS, val,
		(val & fifo_empty_mask), sleep_us, timeout_us);
	if (rc) {
		pr_err("%s: fifo not empty, FIFO_STATUS=0x%08x\n",
			__func__, val);
		goto error;
	}

	pr_debug("%s: polling for lanes to be in stop state, mask=0x%08x\n",
		__func__, stop_state_mask);
	if (ctrl->shared_data->phy_rev == DSI_PHY_REV_30)
		rc = mdss_dsi_phy_v3_wait_for_lanes_stop_state(ctrl, &val);
	else
		rc = readl_poll_timeout(ctrl->ctrl_base + LANE_STATUS, val,
			(val & stop_state_mask), sleep_us, timeout_us);
	if (rc) {
		pr_debug("%s: lanes not in stop state, LANE_STATUS=0x%08x\n",
			__func__, val);
		goto error;
	}

error:
	return rc;
}

static void mdss_dsi_cfg_lane_ctrl(struct mdss_dsi_ctrl_pdata *ctrl,
						u32 bits, int set)
{
	u32 data;

	data = MIPI_INP(ctrl->ctrl_base + 0x00ac);
	if (set)
		data |= bits;
	else
		data &= ~bits;
	MIPI_OUTP(ctrl->ctrl_base + 0x0ac, data);
}


static inline bool mdss_dsi_poll_clk_lane(struct mdss_dsi_ctrl_pdata *ctrl)
{
	u32 clk = 0;

	if (readl_poll_timeout(((ctrl->ctrl_base) + 0x00a8),
				clk,
				(clk & 0x0010),
				10, 1000)) {
		pr_err("%s: ndx=%d clk lane NOT stopped, clk=%x\n",
					__func__, ctrl->ndx, clk);

		return false;
	}
	return true;
}

static void mdss_dsi_wait_clk_lane_to_stop(struct mdss_dsi_ctrl_pdata *ctrl)
{
	if (mdss_dsi_poll_clk_lane(ctrl)) /* stopped */
		return;

	/* clk stuck at hs, start recovery process */

	/* force clk lane tx stop -- bit 20 */
	mdss_dsi_cfg_lane_ctrl(ctrl, BIT(20), 1);

	if (mdss_dsi_poll_clk_lane(ctrl) == false)
		pr_err("%s: clk lane recovery failed\n", __func__);

	/* clear clk lane tx stop -- bit 20 */
	mdss_dsi_cfg_lane_ctrl(ctrl, BIT(20), 0);
}

static void mdss_dsi_stop_hs_clk_lane(struct mdss_dsi_ctrl_pdata *ctrl);

/*
 * mdss_dsi_start_hs_clk_lane:
 * this function is work around solution for 8994 dsi clk lane
 * may stuck at HS problem
 */
static void mdss_dsi_start_hs_clk_lane(struct mdss_dsi_ctrl_pdata *ctrl)
{

	/* make sure clk lane is stopped */
	mdss_dsi_stop_hs_clk_lane(ctrl);

	mutex_lock(&ctrl->clk_lane_mutex);
	mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle, MDSS_DSI_ALL_CLKS,
			  MDSS_DSI_CLK_ON);
	if (ctrl->clk_lane_cnt) {
		pr_err("%s: ndx=%d do-wait, cnt=%d\n",
				__func__, ctrl->ndx, ctrl->clk_lane_cnt);
		mdss_dsi_wait_clk_lane_to_stop(ctrl);
	}

	/* force clk lane hs for next dma or mdp stream */
	mdss_dsi_cfg_lane_ctrl(ctrl, BIT(28), 1);
	ctrl->clk_lane_cnt++;
	pr_debug("%s: ndx=%d, set_hs, cnt=%d\n", __func__,
				ctrl->ndx, ctrl->clk_lane_cnt);
	mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle, MDSS_DSI_ALL_CLKS,
			  MDSS_DSI_CLK_OFF);
	mutex_unlock(&ctrl->clk_lane_mutex);
}

/*
 * mdss_dsi_stop_hs_clk_lane:
 * this function is work around solution for 8994 dsi clk lane
 * may stuck at HS problem
 */
static void mdss_dsi_stop_hs_clk_lane(struct mdss_dsi_ctrl_pdata *ctrl)
{
	u32 fifo = 0;
	u32 lane = 0;

	mutex_lock(&ctrl->clk_lane_mutex);
	if (ctrl->clk_lane_cnt == 0)	/* stopped already */
		goto release;

	mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle, MDSS_DSI_ALL_CLKS,
			  MDSS_DSI_CLK_ON);
	/* fifo */
	if (readl_poll_timeout(((ctrl->ctrl_base) + 0x000c),
			   fifo,
			   ((fifo & 0x11110000) == 0x11110000),
			       10, 1000)) {
		pr_err("%s: fifo NOT empty, fifo=%x\n",
					__func__, fifo);
		goto end;
	}

	/* data lane status */
	if (readl_poll_timeout(((ctrl->ctrl_base) + 0x00a8),
			   lane,
			   ((lane & 0x000f) == 0x000f),
			       100, 2000)) {
		pr_err("%s: datalane NOT stopped, lane=%x\n",
					__func__, lane);
	}
end:
	/* stop force clk lane hs */
	mdss_dsi_cfg_lane_ctrl(ctrl, BIT(28), 0);

	mdss_dsi_wait_clk_lane_to_stop(ctrl);

	ctrl->clk_lane_cnt = 0;
release:
	pr_debug("%s: ndx=%d, cnt=%d\n", __func__,
			ctrl->ndx, ctrl->clk_lane_cnt);

	mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle, MDSS_DSI_ALL_CLKS,
			  MDSS_DSI_CLK_OFF);
	mutex_unlock(&ctrl->clk_lane_mutex);
}

static void mdss_dsi_cmd_start_hs_clk_lane(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_dsi_ctrl_pdata *mctrl = NULL;

	if (mdss_dsi_sync_wait_enable(ctrl)) {
		if (!mdss_dsi_sync_wait_trigger(ctrl))
			return;
		mctrl = mdss_dsi_get_other_ctrl(ctrl);

		if (mctrl)
			mdss_dsi_start_hs_clk_lane(mctrl);
	}

	mdss_dsi_start_hs_clk_lane(ctrl);
}

static void mdss_dsi_cmd_stop_hs_clk_lane(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_dsi_ctrl_pdata *mctrl = NULL;

	if (mdss_dsi_sync_wait_enable(ctrl)) {
		if (!mdss_dsi_sync_wait_trigger(ctrl))
			return;
		mctrl = mdss_dsi_get_other_ctrl(ctrl);

		if (mctrl)
			mdss_dsi_stop_hs_clk_lane(mctrl);
	}

	mdss_dsi_stop_hs_clk_lane(ctrl);
}

static void mdss_dsi_ctl_phy_reset(struct mdss_dsi_ctrl_pdata *ctrl, u32 event)
{
	u32 data0, data1, mask = 0, data_lane_en = 0;
	struct mdss_dsi_ctrl_pdata *ctrl0, *ctrl1;
	u32 ln0, ln1, ln_ctrl0, ln_ctrl1, i;
	/*
	 * Add 2 ms delay suggested by HW team.
	 * Check clk lane stop state after every 200 us
	 */
	u32 loop = 10, u_dly = 200;
	pr_debug("%s: MDSS DSI CTRL and PHY reset. ctrl-num = %d\n",
					__func__, ctrl->ndx);
	if (event == DSI_EV_DLNx_FIFO_OVERFLOW) {
		mask = BIT(20); /* clock lane only for overflow recovery */
	} else if (event == DSI_EV_LP_RX_TIMEOUT) {
		data_lane_en = (MIPI_INP(ctrl->ctrl_base + 0x0004) &
			DSI_DATA_LANES_ENABLED) >> 4;
		/* clock and data lanes for LP_RX_TO recovery */
		mask = BIT(20) | (data_lane_en << 16);
	}

	if (mdss_dsi_is_hw_config_split(ctrl->shared_data)) {
		pr_debug("%s: Split display enabled\n", __func__);
		ctrl0 = mdss_dsi_get_ctrl_by_index(DSI_CTRL_0);
		ctrl1 = mdss_dsi_get_ctrl_by_index(DSI_CTRL_1);

		if (ctrl0->recovery)
			ctrl0->recovery->fxn(ctrl0->recovery->data,
					MDP_INTF_DSI_VIDEO_FIFO_OVERFLOW);
		/*
		 * Disable PHY contention detection and receive.
		 * Configure the strength ctrl 1 register.
		 */
		MIPI_OUTP((ctrl0->phy_io.base) + 0x0188, 0);
		MIPI_OUTP((ctrl1->phy_io.base) + 0x0188, 0);

		data0 = MIPI_INP(ctrl0->ctrl_base + 0x0004);
		data1 = MIPI_INP(ctrl1->ctrl_base + 0x0004);
		/* Disable DSI video mode */
		MIPI_OUTP(ctrl0->ctrl_base + 0x004, (data0 & ~BIT(1)));
		MIPI_OUTP(ctrl1->ctrl_base + 0x004, (data1 & ~BIT(1)));
		/* Disable DSI controller */
		MIPI_OUTP(ctrl0->ctrl_base + 0x004,
					(data0 & ~(BIT(0) | BIT(1))));
		MIPI_OUTP(ctrl1->ctrl_base + 0x004,
					(data1 & ~(BIT(0) | BIT(1))));
		/* "Force On" all dynamic clocks */
		MIPI_OUTP(ctrl0->ctrl_base + 0x11c, 0x100a00);
		MIPI_OUTP(ctrl1->ctrl_base + 0x11c, 0x100a00);

		/* DSI_SW_RESET */
		MIPI_OUTP(ctrl0->ctrl_base + 0x118, 0x1);
		MIPI_OUTP(ctrl1->ctrl_base + 0x118, 0x1);
		wmb();
		MIPI_OUTP(ctrl0->ctrl_base + 0x118, 0x0);
		MIPI_OUTP(ctrl1->ctrl_base + 0x118, 0x0);
		wmb();

		/* Remove "Force On" all dynamic clocks */
		MIPI_OUTP(ctrl0->ctrl_base + 0x11c, 0x00); /* DSI_CLK_CTRL */
		MIPI_OUTP(ctrl1->ctrl_base + 0x11c, 0x00); /* DSI_CLK_CTRL */

		/* Enable DSI controller */
		MIPI_OUTP(ctrl0->ctrl_base + 0x004, (data0 & ~BIT(1)));
		MIPI_OUTP(ctrl1->ctrl_base + 0x004, (data1 & ~BIT(1)));

		/*
		 * Toggle Clk lane Force TX stop so that
		 * clk lane status is no more in stop state
		 */
		ln0 = MIPI_INP(ctrl0->ctrl_base + 0x00a8);
		ln1 = MIPI_INP(ctrl1->ctrl_base + 0x00a8);
		pr_debug("%s: lane status, ctrl0 = 0x%x, ctrl1 = 0x%x\n",
			 __func__, ln0, ln1);
		ln_ctrl0 = MIPI_INP(ctrl0->ctrl_base + 0x00ac);
		ln_ctrl1 = MIPI_INP(ctrl1->ctrl_base + 0x00ac);
		MIPI_OUTP(ctrl0->ctrl_base + 0x0ac, ln_ctrl0 | mask);
		MIPI_OUTP(ctrl1->ctrl_base + 0x0ac, ln_ctrl1 | mask);
		ln_ctrl0 = MIPI_INP(ctrl0->ctrl_base + 0x00ac);
		ln_ctrl1 = MIPI_INP(ctrl1->ctrl_base + 0x00ac);
		for (i = 0; i < loop; i++) {
			ln0 = MIPI_INP(ctrl0->ctrl_base + 0x00a8);
			ln1 = MIPI_INP(ctrl1->ctrl_base + 0x00a8);
			if ((ln0 == 0x1f1f) && (ln1 == 0x1f1f))
				break;
			else
				/* Check clk lane stopState for every 200us */
				udelay(u_dly);
		}
		if (i == loop) {
			MDSS_XLOG(ctrl0->ndx, ln0, 0x1f1f);
			MDSS_XLOG(ctrl1->ndx, ln1, 0x1f1f);
			pr_err("%s: Clock lane still in stop state\n",
					__func__);
			MDSS_XLOG_TOUT_HANDLER("mdp", "dsi0_ctrl", "dsi0_phy",
				"dsi1_ctrl", "dsi1_phy", "panic");
		}
		pr_debug("%s: lane ctrl, ctrl0 = 0x%x, ctrl1 = 0x%x\n",
			 __func__, ln0, ln1);
		MIPI_OUTP(ctrl0->ctrl_base + 0x0ac, ln_ctrl0 & ~mask);
		MIPI_OUTP(ctrl1->ctrl_base + 0x0ac, ln_ctrl1 & ~mask);

		/* Enable Video mode for DSI controller */
		MIPI_OUTP(ctrl0->ctrl_base + 0x004, data0);
		MIPI_OUTP(ctrl1->ctrl_base + 0x004, data1);

		/*
		 * Enable PHY contention detection and receive.
		 * Configure the strength ctrl 1 register.
		 */
		MIPI_OUTP((ctrl0->phy_io.base) + 0x0188, 0x6);
		MIPI_OUTP((ctrl1->phy_io.base) + 0x0188, 0x6);
		/*
		 * Add sufficient delay to make sure
		 * pixel transmission as started
		 */
		udelay(200);
	} else {
		if (ctrl->recovery)
			ctrl->recovery->fxn(ctrl->recovery->data,
					MDP_INTF_DSI_VIDEO_FIFO_OVERFLOW);
		/* Disable PHY contention detection and receive */
		MIPI_OUTP((ctrl->phy_io.base) + 0x0188, 0);

		data0 = MIPI_INP(ctrl->ctrl_base + 0x0004);
		/* Disable DSI video mode */
		MIPI_OUTP(ctrl->ctrl_base + 0x004, (data0 & ~BIT(1)));
		/* Disable DSI controller */
		MIPI_OUTP(ctrl->ctrl_base + 0x004,
					(data0 & ~(BIT(0) | BIT(1))));
		/* "Force On" all dynamic clocks */
		MIPI_OUTP(ctrl->ctrl_base + 0x11c, 0x100a00);

		/* DSI_SW_RESET */
		MIPI_OUTP(ctrl->ctrl_base + 0x118, 0x1);
		wmb();
		MIPI_OUTP(ctrl->ctrl_base + 0x118, 0x0);
		wmb();

		/* Remove "Force On" all dynamic clocks */
		MIPI_OUTP(ctrl->ctrl_base + 0x11c, 0x00);
		/* Enable DSI controller */
		MIPI_OUTP(ctrl->ctrl_base + 0x004, (data0 & ~BIT(1)));

		/*
		 * Toggle Clk lane Force TX stop so that
		 * clk lane status is no more in stop state
		 */
		ln0 = MIPI_INP(ctrl->ctrl_base + 0x00a8);
		pr_debug("%s: lane status, ctrl = 0x%x\n",
			 __func__, ln0);
		ln_ctrl0 = MIPI_INP(ctrl->ctrl_base + 0x00ac);
		MIPI_OUTP(ctrl->ctrl_base + 0x0ac, ln_ctrl0 | mask);
		ln_ctrl0 = MIPI_INP(ctrl->ctrl_base + 0x00ac);
		for (i = 0; i < loop; i++) {
			ln0 = MIPI_INP(ctrl->ctrl_base + 0x00a8);
			if (ln0 == 0x1f1f)
				break;
			else
				/* Check clk lane stopState for every 200us */
				udelay(u_dly);
		}
		if (i == loop) {
			MDSS_XLOG(ctrl->ndx, ln0, 0x1f1f);
			pr_err("%s: Clock lane still in stop state\n",
					__func__);
			MDSS_XLOG_TOUT_HANDLER("mdp", "dsi0_ctrl", "dsi0_phy",
				"dsi1_ctrl", "dsi1_phy", "panic");
		}
		pr_debug("%s: lane status = 0x%x\n",
			 __func__, ln0);
		MIPI_OUTP(ctrl->ctrl_base + 0x0ac, ln_ctrl0 & ~mask);

		/* Enable Video mode for DSI controller */
		MIPI_OUTP(ctrl->ctrl_base + 0x004, data0);
		/* Enable PHY contention detection and receiver */
		MIPI_OUTP((ctrl->phy_io.base) + 0x0188, 0x6);
		/*
		 * Add sufficient delay to make sure
		 * pixel transmission as started
		 */
		udelay(200);
	}
	pr_debug("Recovery done\n");
}

void mdss_dsi_err_intr_ctrl(struct mdss_dsi_ctrl_pdata *ctrl, u32 mask,
					int enable)
{
	u32 intr;

	intr = MIPI_INP(ctrl->ctrl_base + 0x0110);
	intr &= DSI_INTR_TOTAL_MASK;

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
		mdss_dsi_sw_reset(ctrl_pdata, false);
	}

	dsi_ctrl = MIPI_INP((ctrl_pdata->ctrl_base) + 0x0004);
	if (enable)
		dsi_ctrl |= 0x01;
	else
		dsi_ctrl &= ~0x01;

	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x0004, dsi_ctrl);
	wmb();
}

void mdss_dsi_restore_intr_mask(struct mdss_dsi_ctrl_pdata *ctrl)
{
	u32 mask;

	mask = MIPI_INP((ctrl->ctrl_base) + 0x0110);
	mask &= DSI_INTR_TOTAL_MASK;
	mask |= (DSI_INTR_CMD_DMA_DONE_MASK | DSI_INTR_ERROR_MASK |
				DSI_INTR_BTA_DONE_MASK);
	MIPI_OUTP((ctrl->ctrl_base) + 0x0110, mask);
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
		intr_ctrl = DSI_INTR_CMD_DMA_DONE_MASK | DSI_INTR_BTA_DONE_MASK
			| DSI_INTR_ERROR_MASK;
	} else {		/* command mode */
		dsi_ctrl |= 0x05;
		if (pdata->panel_info.type == MIPI_VIDEO_PANEL)
			dsi_ctrl |= 0x02;

		intr_ctrl = DSI_INTR_CMD_DMA_DONE_MASK | DSI_INTR_ERROR_MASK |
			DSI_INTR_CMD_MDP_DONE_MASK | DSI_INTR_BTA_DONE_MASK;
	}

	dma_ctrl = BIT(28) | BIT(26);	/* embedded mode & LP mode */
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
	int i, rc, *lenp;
	int start = 0;
	struct dcs_cmd_req cmdreq;

	rc = 1;
	lenp = ctrl->status_valid_params ?: ctrl->status_cmds_rlen;

	if (!lenp || !ctrl->status_cmds_rlen) {
		pr_err("invalid dsi read params!\n");
		return 0;
	}

	for (i = 0; i < ctrl->status_cmds.cmd_cnt; ++i) {
		memset(&cmdreq, 0, sizeof(cmdreq));
		cmdreq.cmds = ctrl->status_cmds.cmds + i;
		cmdreq.cmds_cnt = 1;
		cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL | CMD_REQ_RX;
		cmdreq.rlen = ctrl->status_cmds_rlen[i];
		cmdreq.cb = NULL;
		cmdreq.rbuf = ctrl->status_buf.data;

		if (ctrl->status_cmds.link_state == DSI_LP_MODE)
			cmdreq.flags  |= CMD_REQ_LP_MODE;
		else if (ctrl->status_cmds.link_state == DSI_HS_MODE)
			cmdreq.flags |= CMD_REQ_HS_MODE;

		rc = mdss_dsi_cmdlist_put(ctrl, &cmdreq);
		if (rc <= 0) {
			pr_err("%s: get status: fail\n", __func__);
			return rc;
		}

		memcpy(ctrl->return_buf + start,
			ctrl->status_buf.data, lenp[i]);
		start += lenp[i];
	}

	return rc;
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
	struct mdss_dsi_ctrl_pdata *sctrl_pdata = NULL;

	if (ctrl_pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return 0;
	}

	pr_debug("%s: Checking Register status\n", __func__);

	mdss_dsi_clk_ctrl(ctrl_pdata, ctrl_pdata->dsi_clk_handle,
			  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_ON);

	sctrl_pdata = mdss_dsi_get_other_ctrl(ctrl_pdata);
	if (!mdss_dsi_sync_wait_enable(ctrl_pdata)) {
		ret = mdss_dsi_read_status(ctrl_pdata);
	} else {
		/*
		 * Read commands to check ESD status are usually sent at
		 * the same time to both the controllers. However, if
		 * sync_wait is enabled, we need to ensure that the
		 * dcs commands are first sent to the non-trigger
		 * controller so that when the commands are triggered,
		 * both controllers receive it at the same time.
		 */
		if (mdss_dsi_sync_wait_trigger(ctrl_pdata)) {
			if (sctrl_pdata)
				ret = mdss_dsi_read_status(sctrl_pdata);
			ret = mdss_dsi_read_status(ctrl_pdata);
		} else {
			ret = mdss_dsi_read_status(ctrl_pdata);
			if (sctrl_pdata)
				ret = mdss_dsi_read_status(sctrl_pdata);
		}
	}

	/*
	 * mdss_dsi_read_status returns the number of bytes returned
	 * by the panel. Success value is greater than zero and failure
	 * case returns zero.
	 */
	if (ret > 0) {
		if (!mdss_dsi_sync_wait_enable(ctrl_pdata) ||
			mdss_dsi_sync_wait_trigger(ctrl_pdata))
			ret = ctrl_pdata->check_read_status(ctrl_pdata);
		else if (sctrl_pdata)
			ret = ctrl_pdata->check_read_status(sctrl_pdata);
	} else {
		pr_err("%s: Read status register returned error\n", __func__);
	}

	mdss_dsi_clk_ctrl(ctrl_pdata, ctrl_pdata->dsi_clk_handle,
			  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_OFF);
	pr_debug("%s: Read register done with ret: %d\n", __func__, ret);

	return ret;
}

void mdss_dsi_dsc_config(struct mdss_dsi_ctrl_pdata *ctrl, struct dsc_desc *dsc)
{
	u32 data, offset;

	if (!dsc) {
		if (ctrl->panel_mode == DSI_VIDEO_MODE)
			offset = MDSS_DSI_VIDEO_COMPRESSION_MODE_CTRL;
		else
			offset = MDSS_DSI_COMMAND_COMPRESSION_MODE_CTRL;
		MIPI_OUTP((ctrl->ctrl_base) + offset, 0);
		return;
	}

	if (dsc->pkt_per_line <= 0) {
		pr_err("%s: Error: pkt_per_line cannot be negative or 0\n",
			__func__);
		return;
	}

	if (ctrl->panel_mode == DSI_VIDEO_MODE) {
		MIPI_OUTP((ctrl->ctrl_base) +
			MDSS_DSI_VIDEO_COMPRESSION_MODE_CTRL2, 0);
		data = dsc->bytes_per_pkt << 16;
		data |= (0x0b << 8);	/*  dtype of compressed image */
		offset = MDSS_DSI_VIDEO_COMPRESSION_MODE_CTRL;
	} else {
		/* strem 0 */
		MIPI_OUTP((ctrl->ctrl_base) +
			MDSS_DSI_COMMAND_COMPRESSION_MODE_CTRL3, 0);

		MIPI_OUTP((ctrl->ctrl_base) +
			MDSS_DSI_COMMAND_COMPRESSION_MODE_CTRL2,
						dsc->bytes_in_slice);

		data = DTYPE_DCS_LWRITE << 8;
		offset = MDSS_DSI_COMMAND_COMPRESSION_MODE_CTRL;
	}

	/*
	 * pkt_per_line:
	 * 0 == 1 pkt
	 * 1 == 2 pkt
	 * 2 == 4 pkt
	 * 3 pkt is not support
	 */
	if (dsc->pkt_per_line == 4)
		data |= (dsc->pkt_per_line - 2) << 6;
	else
		data |= (dsc->pkt_per_line - 1) << 6;
	data |= dsc->eol_byte_num << 4;
	data |= 1;	/* enable */
	MIPI_OUTP((ctrl->ctrl_base) + offset, data);
}

void mdss_dsi_set_burst_mode(struct mdss_dsi_ctrl_pdata *ctrl)
{
	u32 data;

	if (ctrl->shared_data->hw_rev < MDSS_DSI_HW_REV_103)
		return;

	data = MIPI_INP(ctrl->ctrl_base + 0x1b8);

	/*
	 * idle and burst mode are mutually exclusive features,
	 * so disable burst mode if idle has been configured for
	 * the panel, otherwise enable the feature.
	 */
	if (ctrl->idle_enabled)
		data &= ~BIT(16); /* disable burst mode */
	else
		data |= BIT(16); /* enable burst mode */

	ctrl->burst_mode_enabled = !ctrl->idle_enabled;

	MIPI_OUTP((ctrl->ctrl_base + 0x1b8), data);
	pr_debug("%s: burst=%d\n", __func__, ctrl->burst_mode_enabled);

}

static void mdss_dsi_mode_setup(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo;
	struct mipi_panel_info *mipi;
	struct dsc_desc *dsc = NULL;
	u32 data = 0;
	u32 hbp, hfp, vbp, vfp, hspw, vspw, width, height;
	u32 ystride, bpp, dst_bpp, byte_num;
	u32 stream_ctrl, stream_total;
	u32 dummy_xres = 0, dummy_yres = 0;
	u32 hsync_period, vsync_period;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pinfo = &pdata->panel_info;
	if (pinfo->compression_mode == COMPRESSION_DSC)
		dsc = &pinfo->dsc;

	dst_bpp = pdata->panel_info.fbc.enabled ?
		(pdata->panel_info.fbc.target_bpp) : (pinfo->bpp);

	hbp = pdata->panel_info.lcdc.h_back_porch;
	hfp = pdata->panel_info.lcdc.h_front_porch;
	vbp = pdata->panel_info.lcdc.v_back_porch;
	vfp = pdata->panel_info.lcdc.v_front_porch;
	hspw = pdata->panel_info.lcdc.h_pulse_width;
	vspw = pdata->panel_info.lcdc.v_pulse_width;
	width = mult_frac(pdata->panel_info.xres, dst_bpp,
			pdata->panel_info.bpp);
	height = pdata->panel_info.yres;
	pr_debug("%s: fbc=%d width=%d height=%d dst_bpp=%d\n", __func__,
			pdata->panel_info.fbc.enabled, width, height, dst_bpp);

	if (dsc)	/* compressed */
		width = dsc->pclk_per_line;

	if (pdata->panel_info.type == MIPI_VIDEO_PANEL) {
		dummy_xres = mult_frac((pdata->panel_info.lcdc.border_left +
				pdata->panel_info.lcdc.border_right),
				dst_bpp, pdata->panel_info.bpp);
		dummy_yres = pdata->panel_info.lcdc.border_top +
				pdata->panel_info.lcdc.border_bottom;
	}

	mipi = &pdata->panel_info.mipi;
	if (pdata->panel_info.type == MIPI_VIDEO_PANEL) {
		vsync_period = vspw + vbp + height + dummy_yres + vfp;
		hsync_period = hspw + hbp + width + dummy_xres + hfp;

		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x24,
			((hspw + hbp + width + dummy_xres) << 16 |
			(hspw + hbp)));
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x28,
			((vspw + vbp + height + dummy_yres) << 16 |
			(vspw + vbp)));
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x2C,
				((vsync_period - 1) << 16)
				| (hsync_period - 1));

		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x30, (hspw << 16));
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x34, 0);
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x38, (vspw << 16));
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

		if (dsc) {
			byte_num =  dsc->bytes_per_pkt;
			if (pinfo->mipi.insert_dcs_cmd)
				byte_num++;

			stream_ctrl = (byte_num << 16) |
					(mipi->vc << 8) | DTYPE_DCS_LWRITE;
			stream_total = dsc->pic_height << 16 |
							dsc->pclk_per_line;
		} else if (pinfo->partial_update_enabled &&
			mdss_dsi_is_panel_on(pdata) && pinfo->roi.w &&
			pinfo->roi.h) {
			stream_ctrl = (((pinfo->roi.w * bpp) + 1) << 16) |
					(mipi->vc << 8) | DTYPE_DCS_LWRITE;
			stream_total = pinfo->roi.h << 16 | pinfo->roi.w;
		} else {
			stream_ctrl = (ystride << 16) | (mipi->vc << 8) |
					DTYPE_DCS_LWRITE;
			stream_total = height << 16 | width;
		}

		/* DSI_COMMAND_MODE_NULL_INSERTION_CTRL */
		if ((ctrl_pdata->shared_data->hw_rev >= MDSS_DSI_HW_REV_104)
			&& ctrl_pdata->null_insert_enabled) {
			data = (mipi->vc << 1); /* Virtual channel ID */
			data |= 0 << 16; /* Word count of the NULL packet */
			data |= 0x1; /* Enable Null insertion */
			MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x2b4, data);
		}

		mdss_dsi_set_burst_mode(ctrl_pdata);

		/* DSI_COMMAND_MODE_MDP_STREAM_CTRL */
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x60, stream_ctrl);
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x58, stream_ctrl);

		/* DSI_COMMAND_MODE_MDP_STREAM_TOTAL */
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x64, stream_total);
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x5C, stream_total);
	}

	mdss_dsi_dsc_config(ctrl_pdata, dsc);
}

void mdss_dsi_ctrl_setup(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_panel_data *pdata = &ctrl->panel_data;

	pr_debug("%s: called for ctrl%d\n", __func__, ctrl->ndx);

	mdss_dsi_mode_setup(pdata);
	mdss_dsi_host_init(pdata);
	mdss_dsi_op_mode_config(pdata->panel_info.mipi.mode, pdata);
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
	int ignore_underflow = 0;

	if (ctrl_pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);

		/*
		 * This should not return error otherwise
		 * BTA status thread will treat it as dead panel scenario
		 * and request for blank/unblank
		 */
		return 0;
	}

	mutex_lock(&ctrl_pdata->cmd_mutex);

	if (ctrl_pdata->panel_mode == DSI_VIDEO_MODE)
		ignore_underflow = 1;

	pr_debug("%s: Checking BTA status\n", __func__);

	mdss_dsi_clk_ctrl(ctrl_pdata, ctrl_pdata->dsi_clk_handle,
			  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_ON);
	spin_lock_irqsave(&ctrl_pdata->mdp_lock, flag);
	reinit_completion(&ctrl_pdata->bta_comp);
	mdss_dsi_enable_irq(ctrl_pdata, DSI_BTA_TERM);
	spin_unlock_irqrestore(&ctrl_pdata->mdp_lock, flag);
	/* mask out overflow errors */
	if (ignore_underflow)
		mdss_dsi_set_reg(ctrl_pdata, 0x10c, 0x0f0000, 0x0f0000);
	MIPI_OUTP(ctrl_pdata->ctrl_base + 0x098, 0x01); /* trigger  */
	wmb();

	ret = wait_for_completion_killable_timeout(&ctrl_pdata->bta_comp,
						DSI_BTA_EVENT_TIMEOUT);
	if (ret <= 0) {
		mdss_dsi_disable_irq(ctrl_pdata, DSI_BTA_TERM);
		pr_err("%s: DSI BTA error: %i\n", __func__, ret);
	}

	if (ignore_underflow) {
		/* clear pending overflow status */
		mdss_dsi_set_reg(ctrl_pdata, 0xc, 0xffffffff, 0x44440000);
		/* restore overflow isr */
		mdss_dsi_set_reg(ctrl_pdata, 0x10c, 0x0f0000, 0);
	}

	mdss_dsi_clk_ctrl(ctrl_pdata, ctrl_pdata->dsi_clk_handle,
			  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_OFF);
	pr_debug("%s: BTA done with ret: %d\n", __func__, ret);

	mutex_unlock(&ctrl_pdata->cmd_mutex);

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

static int mdss_dsi_cmd_dma_tpg_tx(struct mdss_dsi_ctrl_pdata *ctrl,
					struct dsi_buf *tp)
{
	int len, i, ret = 0, data = 0;
	u32 *bp;
	struct mdss_dsi_ctrl_pdata *mctrl = NULL;

	if (tp->len > DMA_TPG_FIFO_LEN) {
		pr_debug("command length more than FIFO length\n");
		return -EINVAL;
	}

	if (ctrl->shared_data->hw_rev < MDSS_DSI_HW_REV_103) {
		pr_err("CMD DMA TPG not supported for this DSI version\n");
		return -EINVAL;
	}

	bp = (u32 *)tp->data;
	len = ALIGN(tp->len, 4);

	reinit_completion(&ctrl->dma_comp);

	if (mdss_dsi_sync_wait_trigger(ctrl))
		mctrl = mdss_dsi_get_other_ctrl(ctrl);

	data = BIT(16) | BIT(17);	/* select CMD_DMA_PATTERN_SEL to 3 */
	data |= BIT(2);			/* select CMD_DMA_FIFO_MODE to 1 */
	data |= BIT(1);			/* enable CMD_DMA_TPG */

	MIPI_OUTP(ctrl->ctrl_base + 0x15c, data);
	if (mctrl)
		MIPI_OUTP(mctrl->ctrl_base + 0x15c, data);

	/*
	 * The DMA command parameters need to be programmed to the DMA_INIT_VAL
	 * register in the proper order. The 'len' value will be a multiple
	 * of 4, the padding bytes to make sure of this will be taken care of in
	 * mdss_dsi_cmd_dma_add API.
	 */
	for (i = 0; i < len; i += 4) {
		MIPI_OUTP(ctrl->ctrl_base + 0x17c, *bp);
		if (mctrl)
			MIPI_OUTP(mctrl->ctrl_base + 0x17c, *bp);
		wmb(); /* make sure write happens before writing next command */
		bp++;
	}

	/*
	 * The number of writes to the DMA_INIT_VAL register should be an even
	 * number of dwords (32 bits). In case 'len' is not a multiple of 8,
	 * we need to do make an extra write to the register with 0x00 to
	 * satisfy this condition.
	 */
	if ((len % 8) != 0) {
		MIPI_OUTP(ctrl->ctrl_base + 0x17c, 0x00);
		if (mctrl)
			MIPI_OUTP(mctrl->ctrl_base + 0x17c, 0x00);
	}

	if (mctrl) {
		MIPI_OUTP(mctrl->ctrl_base + 0x04c, len);
		MIPI_OUTP(mctrl->ctrl_base + 0x090, 0x01); /* trigger */
	}
	MIPI_OUTP(ctrl->ctrl_base + 0x04c, len);
	wmb(); /* make sure DMA length is programmed */

	MIPI_OUTP(ctrl->ctrl_base + 0x090, 0x01); /* trigger */
	wmb(); /* make sure DMA trigger happens */

	ret = wait_for_completion_timeout(&ctrl->dma_comp,
				msecs_to_jiffies(DMA_TX_TIMEOUT));
	if (ret == 0)
		ret = -ETIMEDOUT;
	else
		ret = tp->len;

	/* Reset the DMA TPG FIFO */
	MIPI_OUTP(ctrl->ctrl_base + 0x1ec, 0x1);
	wmb(); /* make sure FIFO reset happens */
	MIPI_OUTP(ctrl->ctrl_base + 0x1ec, 0x0);
	wmb(); /* make sure FIFO reset happens */
	/* Disable CMD_DMA_TPG */
	MIPI_OUTP(ctrl->ctrl_base + 0x15c, 0x0);

	if (mctrl) {
		/* Reset the DMA TPG FIFO */
		MIPI_OUTP(mctrl->ctrl_base + 0x1ec, 0x1);
		wmb(); /* make sure FIFO reset happens */
		MIPI_OUTP(mctrl->ctrl_base + 0x1ec, 0x0);
		wmb(); /* make sure FIFO reset happens */
		/* Disable CMD_DMA_TPG */
		MIPI_OUTP(mctrl->ctrl_base + 0x15c, 0x0);
	}

	return ret;
}

static int mdss_dsi_cmds2buf_tx(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_cmd_desc *cmds, int cnt, int use_dma_tpg)
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
			return 0;
		}
		tot += len;
		if (dchdr->last) {
			tp->data = tp->start; /* begin of buf */

			wait = mdss_dsi_wait4video_eng_busy(ctrl);

			mdss_dsi_enable_irq(ctrl, DSI_CMD_TERM);
			if (use_dma_tpg)
				len = mdss_dsi_cmd_dma_tpg_tx(ctrl, tp);
			else
				len = mdss_dsi_cmd_dma_tx(ctrl, tp);
			if (IS_ERR_VALUE(len)) {
				mdss_dsi_disable_irq(ctrl, DSI_CMD_TERM);
				pr_err("%s: failed to call cmd_dma_tx for cmd = 0x%x\n",
					__func__,  cm->payload[0]);
				return 0;
			}
			pr_debug("%s: cmd_dma_tx for cmd = 0x%x, len = %d\n",
					__func__,  cm->payload[0], len);

			if (!wait || dchdr->wait > VSYNC_PERIOD)
				usleep_range(dchdr->wait * 1000, dchdr->wait * 1000);

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
		struct dsi_cmd_desc *cmds, int cnt, int use_dma_tpg)
{
	int len = 0;
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

	len = mdss_dsi_cmds2buf_tx(ctrl, cmds, cnt, use_dma_tpg);
	if (!len)
		pr_err("%s: failed to call\n", __func__);

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

	return len;
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
 * 1st read: 4 bytes header + 10 bytes payload + 2 crc
 * 2nd read: 14 bytes payload + 2 crc
 * 3rd read: 14 bytes payload + 2 crc
 *
 */
int mdss_dsi_cmds_rx(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_cmd_desc *cmds, int rlen, int use_dma_tpg)
{
	int data_byte, rx_byte, dlen, end;
	int short_response, diff, pkt_size, ret = 0;
	struct dsi_buf *tp, *rp;
	char cmd;
	struct mdss_dsi_ctrl_pdata *mctrl = NULL;


	if (ctrl->panel_data.panel_info.panel_ack_disabled) {
		pr_err("%s: ACK from Client not supported\n", __func__);
		return rlen;
	}

	if (rlen == 0) {
		pr_debug("%s: Minimum MRPS value should be 1\n", __func__);
		return 0;
	}

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
		data_byte = 10;	/* first read */
		if (rlen < data_byte)
			pkt_size = rlen;
		else
			pkt_size = data_byte;
		rx_byte = data_byte + 6; /* 4 header + 2 crc */
	}


	tp = &ctrl->tx_buf;
	rp = &ctrl->rx_buf;

	end = 0;
	mdss_dsi_buf_init(rp);
	while (!end) {
		pr_debug("%s:  rlen=%d pkt_size=%d rx_byte=%d\n",
				__func__, rlen, pkt_size, rx_byte);
		/*
		 * Skip max_pkt_size dcs cmd if
		 * its already been configured
		 * for the requested pkt_size
		 */
		if (pkt_size == ctrl->cur_max_pkt_size)
			goto skip_max_pkt_size;

		max_pktsize[0] = pkt_size;
		mdss_dsi_buf_init(tp);
		ret = mdss_dsi_cmd_dma_add(tp, &pkt_size_cmd);
		if (!ret) {
			pr_err("%s: failed to add max_pkt_size\n",
				__func__);
			rp->len = 0;
			rp->read_cnt = 0;
			goto end;
		}

		mdss_dsi_wait4video_eng_busy(ctrl);

		mdss_dsi_enable_irq(ctrl, DSI_CMD_TERM);
		if (use_dma_tpg)
			ret = mdss_dsi_cmd_dma_tpg_tx(ctrl, tp);
		else
			ret = mdss_dsi_cmd_dma_tx(ctrl, tp);
		if (IS_ERR_VALUE(ret)) {
			mdss_dsi_disable_irq(ctrl, DSI_CMD_TERM);
			pr_err("%s: failed to tx max_pkt_size\n",
				__func__);
			rp->len = 0;
			rp->read_cnt = 0;
			goto end;
		}
		ctrl->cur_max_pkt_size = pkt_size;
		pr_debug("%s: max_pkt_size=%d sent\n",
					__func__, pkt_size);

skip_max_pkt_size:
		mdss_dsi_buf_init(tp);
		ret = mdss_dsi_cmd_dma_add(tp, cmds);
		if (!ret) {
			pr_err("%s: failed to add cmd = 0x%x\n",
				__func__,  cmds->payload[0]);
			rp->len = 0;
			rp->read_cnt = 0;
			goto end;
		}

		if (ctrl->shared_data->hw_rev >= MDSS_DSI_HW_REV_101) {
			/* clear the RDBK_DATA registers */
			MIPI_OUTP(ctrl->ctrl_base + 0x01d4, 0x1);
			wmb(); /* make sure the RDBK registers are cleared */
			MIPI_OUTP(ctrl->ctrl_base + 0x01d4, 0x0);
			wmb(); /* make sure the RDBK registers are cleared */
		}

		mdss_dsi_wait4video_eng_busy(ctrl);	/* video mode only */
		mdss_dsi_enable_irq(ctrl, DSI_CMD_TERM);
		/* transmit read comamnd to client */
		if (use_dma_tpg)
			ret = mdss_dsi_cmd_dma_tpg_tx(ctrl, tp);
		else
			ret = mdss_dsi_cmd_dma_tx(ctrl, tp);
		if (IS_ERR_VALUE(ret)) {
			mdss_dsi_disable_irq(ctrl, DSI_CMD_TERM);
			pr_err("%s: failed to tx cmd = 0x%x\n",
				__func__,  cmds->payload[0]);
			rp->len = 0;
			rp->read_cnt = 0;
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

		if (!dlen)
			goto end;

		if (short_response)
			break;

		if (rlen <= data_byte) {
			diff = data_byte - rlen;
			end = 1;
		} else {
			diff = 0;
			rlen -= data_byte;
		}

		dlen -= 2; /* 2 crc */
		dlen -= diff;
		rp->data += dlen;	/* next start position */
		rp->len += dlen;
		if (!end) {
			data_byte = 14; /* NOT first read */
			if (rlen < data_byte)
				pkt_size += rlen;
			else
				pkt_size += data_byte;
		}
		pr_debug("%s: rp data=%x len=%d dlen=%d diff=%d\n",
			 __func__, (int) (unsigned long) rp->data,
			 rp->len, dlen, diff);
	}

	/*
	 * For single Long read, if the requested rlen < 10,
	 * we need to shift the start position of rx
	 * data buffer to skip the bytes which are not
	 * updated.
	 */
	if (rp->read_cnt < 16 && !short_response)
		rp->data = rp->start + (16 - rp->read_cnt);
	else
		rp->data = rp->start;
	cmd = rp->data[0];
	switch (cmd) {
	case DTYPE_ACK_ERR_RESP:
		pr_debug("%s: rx ACK_ERR_PACLAGE\n", __func__);
		rp->len = 0;
		rp->read_cnt = 0;
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
		rp->read_cnt = 0;
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

	if (rp->len && (rp->len != rp->read_cnt))
		pr_err("Bytes read: %d requested:%d mismatch\n",
					rp->read_cnt, rp->len);

	return rp->read_cnt;
}

static int mdss_dsi_cmd_dma_tx(struct mdss_dsi_ctrl_pdata *ctrl,
					struct dsi_buf *tp)
{
	int len, ret = 0;
	int domain = MDSS_IOMMU_DOMAIN_UNSECURE;
	char *bp;
	struct mdss_dsi_ctrl_pdata *mctrl = NULL;
	int ignored = 0;	/* overflow ignored */

	bp = tp->data;

	len = ALIGN(tp->len, 4);
	ctrl->dma_size = ALIGN(tp->len, SZ_4K);

	ctrl->mdss_util->iommu_lock();
	if (ctrl->mdss_util->iommu_attached()) {
		ret = mdss_smmu_dsi_map_buffer(tp->dmap, domain, ctrl->dma_size,
			&(ctrl->dma_addr), tp->start, DMA_TO_DEVICE);
		if (IS_ERR_VALUE(ret)) {
			pr_err("unable to map dma memory to iommu(%d)\n", ret);
			ctrl->mdss_util->iommu_unlock();
			return -ENOMEM;
		}
		ctrl->dmap_iommu_map = true;
	} else {
		ctrl->dma_addr = tp->dmap;
	}

	reinit_completion(&ctrl->dma_comp);

	if (ctrl->panel_mode == DSI_VIDEO_MODE)
		ignored = 1;

	if (mdss_dsi_sync_wait_trigger(ctrl)) {
		/* broadcast same cmd to other panel */
		mctrl = mdss_dsi_get_other_ctrl(ctrl);
		if (mctrl && mctrl->dma_addr == 0) {
			if (ignored) {
				/* mask out overflow isr */
				mdss_dsi_set_reg(mctrl, 0x10c,
						0x0f0000, 0x0f0000);
			}
			MIPI_OUTP(mctrl->ctrl_base + 0x048, ctrl->dma_addr);
			MIPI_OUTP(mctrl->ctrl_base + 0x04c, len);
			MIPI_OUTP(mctrl->ctrl_base + 0x090, 0x01); /* trigger */
		}
	}

	if (ignored) {
		/* mask out overflow isr */
		mdss_dsi_set_reg(ctrl, 0x10c, 0x0f0000, 0x0f0000);
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
	if (ret == 0) {
		u32 reg_val, status;

		reg_val = MIPI_INP(ctrl->ctrl_base + 0x0110);/* DSI_INTR_CTRL */
		status = reg_val & DSI_INTR_CMD_DMA_DONE;
		if (status) {
			reg_val &= DSI_INTR_MASK_ALL;
			/* clear CMD DMA isr only */
			reg_val |= DSI_INTR_CMD_DMA_DONE;
			MIPI_OUTP(ctrl->ctrl_base + 0x0110, reg_val);
			mdss_dsi_disable_irq_nosync(ctrl, DSI_CMD_TERM);
			complete(&ctrl->dma_comp);

			pr_warn("%s: dma tx done but irq not triggered\n",
				__func__);
		} else {
			ret = -ETIMEDOUT;
		}
	}

	if (!IS_ERR_VALUE(ret))
		ret = tp->len;

	if (mctrl && mctrl->dma_addr) {
		if (ignored) {
			/* clear pending overflow status */
			mdss_dsi_set_reg(mctrl, 0xc, 0xffffffff, 0x44440000);
			/* restore overflow isr */
			mdss_dsi_set_reg(mctrl, 0x10c, 0x0f0000, 0);
		}
		if (mctrl->dmap_iommu_map) {
			mdss_smmu_dsi_unmap_buffer(mctrl->dma_addr, domain,
				mctrl->dma_size, DMA_TO_DEVICE);
			mctrl->dmap_iommu_map = false;
		}
		mctrl->dma_addr = 0;
		mctrl->dma_size = 0;
	}

	if (ctrl->dmap_iommu_map) {
		mdss_smmu_dsi_unmap_buffer(ctrl->dma_addr, domain,
			ctrl->dma_size, DMA_TO_DEVICE);
		ctrl->dmap_iommu_map = false;
	}

	if (ignored) {
		/* clear pending overflow status */
		mdss_dsi_set_reg(ctrl, 0xc, 0xffffffff, 0x44440000);
		/* restore overflow isr */
		mdss_dsi_set_reg(ctrl, 0x10c, 0x0f0000, 0);
	}
	ctrl->dma_addr = 0;
	ctrl->dma_size = 0;
end:
	ctrl->mdss_util->iommu_unlock();
	return ret;
}

static int mdss_dsi_cmd_dma_rx(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_buf *rp, int rx_byte)

{
	u32 *lp, *temp, data;
	int i, j = 0, off, cnt;
	bool ack_error = false;
	char reg[16] = {0x0};
	int repeated_bytes = 0;

	lp = (u32 *)rp->data;
	temp = (u32 *)reg;
	cnt = rx_byte;
	cnt += 3;
	cnt >>= 2;

	if (cnt > 4)
		cnt = 4; /* 4 x 32 bits registers only */

	if (ctrl->shared_data->hw_rev >= MDSS_DSI_HW_REV_101) {
		rp->read_cnt = (MIPI_INP((ctrl->ctrl_base) + 0x01d4) >> 16);
		pr_debug("%s: bytes read:%d\n", __func__, rp->read_cnt);

		ack_error = (rx_byte == 4) ? (rp->read_cnt == 8) :
				((rp->read_cnt - 4) == (max_pktsize[0] + 6));

		if (ack_error)
			rp->read_cnt -= 4; /* 4 byte read err report */
		if (!rp->read_cnt) {
			pr_err("%s: Errors detected, no data rxed\n", __func__);
			return 0;
		}
	} else if (rx_byte == 4) {
		rp->read_cnt = 4;
	} else {
		rp->read_cnt = (max_pktsize[0] + 6);
	}

	/*
	 * In case of multiple reads from the panel, after the first read, there
	 * is possibility that there are some bytes in the payload repeating in
	 * the RDBK_DATA registers. Since we read all the parameters from the
	 * panel right from the first byte for every pass. We need to skip the
	 * repeating bytes and then append the new parameters to the rx buffer.
	 */
	if (rp->read_cnt > 16) {
		int bytes_shifted, data_lost = 0, rem_header_bytes = 0;
		/* Any data more than 16 bytes will be shifted out */
		bytes_shifted = rp->read_cnt - rx_byte;
		if (bytes_shifted >= 4)
			data_lost = bytes_shifted - 4; /* remove dcs header */
		else
			rem_header_bytes = 4 - bytes_shifted; /* rem header */
		/*
		 * (rp->len - 4) -> current rx buffer data length.
		 * If data_lost > 0, then ((rp->len - 4) - data_lost) will be
		 * the number of repeating bytes.
		 * If data_lost == 0, then ((rp->len - 4) + rem_header_bytes)
		 * will be the number of bytes repeating in between rx buffer
		 * and the current RDBK_DATA registers. We need to skip the
		 * repeating bytes.
		 */
		repeated_bytes = (rp->len - 4) - data_lost + rem_header_bytes;
	}

	off = 0x06c;	/* DSI_RDBK_DATA0 */
	off += ((cnt - 1) * 4);

	for (i = 0; i < cnt; i++) {
		data = (u32)MIPI_INP((ctrl->ctrl_base) + off);
		/* to network byte order */
		if (!repeated_bytes)
			*lp++ = ntohl(data);
		else
			*temp++ = ntohl(data);
		pr_debug("%s: data = 0x%x and ntohl(data) = 0x%x\n",
					 __func__, data, ntohl(data));
		off -= 4;
	}

	/* Skip duplicates and append other data to the rx buffer */
	if (repeated_bytes) {
		for (i = repeated_bytes; i < 16; i++)
			rp->data[j++] = reg[i];
	}

	return rx_byte;
}

static int mdss_dsi_bus_bandwidth_vote(struct dsi_shared_data *sdata, bool on)
{
	int rc = 0;
	bool changed = false;

	if (on) {
		if (sdata->bus_refcount == 0)
			changed = true;
		sdata->bus_refcount++;
	} else {
		if (sdata->bus_refcount != 0) {
			sdata->bus_refcount--;
			if (sdata->bus_refcount == 0)
				changed = true;
		} else {
			pr_warn("%s: bus bw votes are not balanced\n",
				__func__);
		}
	}

	if (changed) {
		rc = msm_bus_scale_client_update_request(sdata->bus_handle,
							 on ? 1 : 0);
		if (rc)
			pr_err("%s: Bus bandwidth vote failed\n", __func__);
	}

	return rc;
}


int mdss_dsi_en_wait4dynamic_done(struct mdss_dsi_ctrl_pdata *ctrl)
{
	unsigned long flag;
	u32 data;
	int rc = 0;
	struct mdss_dsi_ctrl_pdata *sctrl_pdata = NULL;

	/* DSI_INTL_CTRL */
	data = MIPI_INP((ctrl->ctrl_base) + 0x0110);
	data &= DSI_INTR_TOTAL_MASK;
	data |= DSI_INTR_DYNAMIC_REFRESH_MASK;
	MIPI_OUTP((ctrl->ctrl_base) + 0x0110, data);

	spin_lock_irqsave(&ctrl->mdp_lock, flag);
	reinit_completion(&ctrl->dynamic_comp);
	mdss_dsi_enable_irq(ctrl, DSI_DYNAMIC_TERM);
	spin_unlock_irqrestore(&ctrl->mdp_lock, flag);

	/*
	 * Ensure that registers are updated before triggering
	 * dynamic refresh
	 */
	wmb();

	MIPI_OUTP((ctrl->ctrl_base) + DSI_DYNAMIC_REFRESH_CTRL,
		(BIT(13) | BIT(8) | BIT(0)));

	/*
	 * Configure DYNAMIC_REFRESH_CTRL for second controller only
	 * for split DSI cases.
	 */
	if (mdss_dsi_is_ctrl_clk_master(ctrl))
		sctrl_pdata = mdss_dsi_get_ctrl_clk_slave();

	if (sctrl_pdata)
		MIPI_OUTP((sctrl_pdata->ctrl_base) + DSI_DYNAMIC_REFRESH_CTRL,
				(BIT(13) | BIT(8) | BIT(0)));

	rc = wait_for_completion_timeout(&ctrl->dynamic_comp,
			msecs_to_jiffies(VSYNC_PERIOD * 4));
	if (rc == 0) {
		u32 reg_val, status;

		reg_val = MIPI_INP(ctrl->ctrl_base + MDSS_DSI_INT_CTRL);
		status = reg_val & DSI_INTR_DYNAMIC_REFRESH_DONE;
		if (status) {
			reg_val &= DSI_INTR_MASK_ALL;
			/* clear dfps DONE isr only */
			reg_val |= DSI_INTR_DYNAMIC_REFRESH_DONE;
			MIPI_OUTP(ctrl->ctrl_base + MDSS_DSI_INT_CTRL, reg_val);
			mdss_dsi_disable_irq(ctrl, DSI_DYNAMIC_TERM);
			pr_warn_ratelimited("%s: dfps done but irq not triggered\n",
				__func__);
		} else {
			pr_err("Dynamic interrupt timedout\n");
			rc = -ETIMEDOUT;
		}
	}

	data = MIPI_INP((ctrl->ctrl_base) + 0x0110);
	data &= DSI_INTR_TOTAL_MASK;
	data &= ~DSI_INTR_DYNAMIC_REFRESH_MASK;
	MIPI_OUTP((ctrl->ctrl_base) + 0x0110, data);

	return rc;
}

void mdss_dsi_wait4video_done(struct mdss_dsi_ctrl_pdata *ctrl)
{
	unsigned long flag;
	u32 data;

	/* DSI_INTL_CTRL */
	data = MIPI_INP((ctrl->ctrl_base) + 0x0110);
	data &= DSI_INTR_TOTAL_MASK;
	data |= DSI_INTR_VIDEO_DONE_MASK;

	MIPI_OUTP((ctrl->ctrl_base) + 0x0110, data);

	spin_lock_irqsave(&ctrl->mdp_lock, flag);
	reinit_completion(&ctrl->video_comp);
	mdss_dsi_enable_irq(ctrl, DSI_VIDEO_TERM);
	spin_unlock_irqrestore(&ctrl->mdp_lock, flag);

	wait_for_completion_timeout(&ctrl->video_comp,
			msecs_to_jiffies(VSYNC_PERIOD * 4));

	data = MIPI_INP((ctrl->ctrl_base) + 0x0110);
	data &= DSI_INTR_TOTAL_MASK;
	data &= ~DSI_INTR_VIDEO_DONE_MASK;
	MIPI_OUTP((ctrl->ctrl_base) + 0x0110, data);
}

static int mdss_dsi_wait4video_eng_busy(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int ret = 0;
	u32 v_total = 0, v_blank = 0, sleep_ms = 0, fps = 0;
	struct mdss_panel_info *pinfo = &ctrl->panel_data.panel_info;

	if (ctrl->panel_mode == DSI_CMD_MODE)
		return ret;

	if (ctrl->ctrl_state & CTRL_STATE_MDP_ACTIVE) {
		mdss_dsi_wait4video_done(ctrl);
		v_total = mdss_panel_get_vtotal(pinfo);
		v_blank = pinfo->lcdc.v_back_porch + pinfo->lcdc.v_front_porch +
			pinfo->lcdc.v_pulse_width;
		if (pinfo->dynamic_fps && pinfo->current_fps)
			fps = pinfo->current_fps;
		else
			fps = pinfo->mipi.frame_rate;

		sleep_ms = CEIL((v_blank * 1000), (v_total * fps)) + 1;
		/* delay sleep_ms to skip BLLP */
		if (sleep_ms)
			udelay(sleep_ms * 1000);
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
	reinit_completion(&ctrl->mdp_comp);
	MDSS_XLOG(ctrl->ndx, ctrl->mdp_busy, current->pid);
	spin_unlock_irqrestore(&ctrl->mdp_lock, flag);
}

static int mdss_dsi_mdp_busy_tout_check(struct mdss_dsi_ctrl_pdata *ctrl)
{
	unsigned long flag;
	u32 isr;
	bool stop_hs_clk = false;
	int tout = 1;

	/*
	 * two possible scenario:
	 * 1) DSI_INTR_CMD_MDP_DONE set but isr not fired
	 * 2) DSI_INTR_CMD_MDP_DONE set and cleared (isr fired)
	 * but event_thread not wakeup
	 */
	mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle, MDSS_DSI_ALL_CLKS,
			  MDSS_DSI_CLK_ON);
	spin_lock_irqsave(&ctrl->mdp_lock, flag);

	isr = MIPI_INP(ctrl->ctrl_base + 0x0110);
	if (isr & DSI_INTR_CMD_MDP_DONE) {
		pr_warn("INTR_CMD_MDP_DONE set but isr not fired\n");
		isr &= DSI_INTR_MASK_ALL;
		isr |= DSI_INTR_CMD_MDP_DONE; /* clear this isr only */
		MIPI_OUTP(ctrl->ctrl_base + 0x0110, isr);
		mdss_dsi_disable_irq_nosync(ctrl, DSI_MDP_TERM);
		ctrl->mdp_busy = false;
		if (ctrl->shared_data->cmd_clk_ln_recovery_en &&
			ctrl->panel_mode == DSI_CMD_MODE) {
			/* has hs_lane_recovery do the work */
			stop_hs_clk = true;
		}
		tout = 0;	/* recovered */
	}

	spin_unlock_irqrestore(&ctrl->mdp_lock, flag);

	if (stop_hs_clk)
		mdss_dsi_stop_hs_clk_lane(ctrl);

	complete_all(&ctrl->mdp_comp);

	mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle, MDSS_DSI_ALL_CLKS,
			  MDSS_DSI_CLK_OFF);

	return tout;
}

void mdss_dsi_cmd_mdp_busy(struct mdss_dsi_ctrl_pdata *ctrl)
{
	unsigned long flags;
	int need_wait = 0;
	int rc;

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
		rc = wait_for_completion_timeout(&ctrl->mdp_comp,
					msecs_to_jiffies(DMA_TX_TIMEOUT));
		spin_lock_irqsave(&ctrl->mdp_lock, flags);
		if (!ctrl->mdp_busy)
			rc = 1;
		spin_unlock_irqrestore(&ctrl->mdp_lock, flags);
		if (!rc) {
			if (mdss_dsi_mdp_busy_tout_check(ctrl)) {
				pr_err("%s: timeout error\n", __func__);
				MDSS_XLOG_TOUT_HANDLER("mdp", "dsi0_ctrl",
					"dsi0_phy", "dsi1_ctrl", "dsi1_phy",
					"vbif", "vbif_nrt", "dbg_bus",
					"vbif_dbg_bus", "panic");
			}
		}
	}
	pr_debug("%s: done pid=%d\n", __func__, current->pid);
	MDSS_XLOG(ctrl->ndx, ctrl->mdp_busy, current->pid, XLOG_FUNC_EXIT);
}

int mdss_dsi_cmdlist_tx(struct mdss_dsi_ctrl_pdata *ctrl,
				struct dcs_cmd_req *req)
{
	int len;

	if (mdss_dsi_sync_wait_enable(ctrl)) {
		ctrl->do_unicast = false;
		 if (!ctrl->cmd_sync_wait_trigger &&
					req->flags & CMD_REQ_UNICAST)
			ctrl->do_unicast = true;
	}

	len = mdss_dsi_cmds_tx(ctrl, req->cmds, req->cmds_cnt,
				(req->flags & CMD_REQ_DMA_TPG));

	if (req->cb)
		req->cb(len);

	return len;
}

int mdss_dsi_cmdlist_rx(struct mdss_dsi_ctrl_pdata *ctrl,
				struct dcs_cmd_req *req)
{
	struct dsi_buf *rp;
	int len = 0;

	if (req->rbuf) {
		rp = &ctrl->rx_buf;
		len = mdss_dsi_cmds_rx(ctrl, req->cmds, req->rlen,
				(req->flags & CMD_REQ_DMA_TPG));
		memcpy(req->rbuf, rp->data, rp->len);
		ctrl->rx_len = len;
	} else {
		pr_err("%s: No rx buffer provided\n", __func__);
	}

	if (req->cb)
		req->cb(len);

	return len;
}

static inline bool mdss_dsi_delay_cmd(struct mdss_dsi_ctrl_pdata *ctrl,
	bool from_mdp)
{
	unsigned long flags;
	bool mdp_busy = false;
	bool need_wait = false;

	if (!ctrl->mdp_callback)
		goto exit;

	/* delay only for split dsi, cmd mode and burst mode enabled cases */
	if (!mdss_dsi_is_hw_config_split(ctrl->shared_data) ||
	    !(ctrl->panel_mode == DSI_CMD_MODE) ||
	    !ctrl->burst_mode_enabled)
		goto exit;

	/* delay only if cmd is not from mdp and panel has been initialized */
	if (from_mdp || !(ctrl->ctrl_state & CTRL_STATE_PANEL_INIT))
		goto exit;

	/* if broadcast enabled, apply delay only if this is the ctrl trigger */
	if (mdss_dsi_sync_wait_enable(ctrl) &&
	   !mdss_dsi_sync_wait_trigger(ctrl))
		goto exit;

	spin_lock_irqsave(&ctrl->mdp_lock, flags);
	if (ctrl->mdp_busy == true)
		mdp_busy = true;
	spin_unlock_irqrestore(&ctrl->mdp_lock, flags);

	/*
	 * apply delay only if:
	 *  mdp_busy bool is set - kickoff is being scheduled by sw
	 *  MDP_BUSY bit  is not set - transfer is not on-going in hw yet
	 */
	if (mdp_busy && !(MIPI_INP(ctrl->ctrl_base + 0x008) & BIT(2)))
		need_wait = true;

exit:
	MDSS_XLOG(need_wait, from_mdp, mdp_busy);
	return need_wait;
}

int mdss_dsi_cmdlist_commit(struct mdss_dsi_ctrl_pdata *ctrl, int from_mdp)
{
	struct dcs_cmd_req *req;
	struct mdss_panel_info *pinfo;
	struct mdss_rect *roi = NULL;
	bool use_iommu = false;
	int ret = -EINVAL;
	int rc = 0;
	bool hs_req = false;
	bool cmd_mutex_acquired = false;

	if (from_mdp) {	/* from mdp kickoff */
		if (!ctrl->burst_mode_enabled) {
			mutex_lock(&ctrl->cmd_mutex);
			cmd_mutex_acquired = true;
		}
		pinfo = &ctrl->panel_data.panel_info;
		if (pinfo->partial_update_enabled)
			roi = &pinfo->roi;
	}

	req = mdss_dsi_cmdlist_get(ctrl, from_mdp);
	if (req && from_mdp && ctrl->burst_mode_enabled) {
		mutex_lock(&ctrl->cmd_mutex);
		cmd_mutex_acquired = true;
	}

	MDSS_XLOG(ctrl->ndx, from_mdp, ctrl->mdp_busy, current->pid,
							XLOG_FUNC_ENTRY);

	if (req && (req->flags & CMD_REQ_HS_MODE))
		hs_req = true;

	if ((!ctrl->burst_mode_enabled) || from_mdp) {
		/* make sure dsi_cmd_mdp is idle */
		mdss_dsi_cmd_mdp_busy(ctrl);
	}

	/*
	 * if secure display session is enabled
	 * and DSI controller version is above 1.3.0,
	 * then send DSI commands using TPG FIFO.
	 */
	if (mdss_get_sd_client_cnt() && req) {
		if (ctrl->shared_data->hw_rev >= MDSS_DSI_HW_REV_103) {
			req->flags |= CMD_REQ_DMA_TPG;
		} else {
			if (cmd_mutex_acquired)
				mutex_unlock(&ctrl->cmd_mutex);
			return -EPERM;
		}
	}

	/* For DSI versions less than 1.3.0, CMD DMA TPG is not supported */
	if (req && (ctrl->shared_data->hw_rev < MDSS_DSI_HW_REV_103))
		req->flags &= ~CMD_REQ_DMA_TPG;

	pr_debug("%s: ctrl=%d from_mdp=%d pid=%d\n", __func__,
				ctrl->ndx, from_mdp, current->pid);

	if (from_mdp) { /* from mdp kickoff */
		/*
		 * when partial update enabled, the roi of pinfo
		 * is updated before mdp kickoff. Either width or
		 * height of roi is non zero, then really kickoff
		 * will followed.
		 */
		if (!roi || (roi->w != 0 || roi->h != 0)) {
			if (ctrl->shared_data->cmd_clk_ln_recovery_en &&
					ctrl->panel_mode == DSI_CMD_MODE)
				mdss_dsi_start_hs_clk_lane(ctrl);
		}
	} else {	/* from dcs send */
		if (ctrl->shared_data->cmd_clk_ln_recovery_en &&
				ctrl->panel_mode == DSI_CMD_MODE && hs_req)
			mdss_dsi_cmd_start_hs_clk_lane(ctrl);
	}

	if (!req)
		goto need_lock;

	MDSS_XLOG(ctrl->ndx, req->flags, req->cmds_cnt, from_mdp, current->pid);

	pr_debug("%s:  from_mdp=%d pid=%d\n", __func__, from_mdp, current->pid);

	if (!(req->flags & CMD_REQ_DMA_TPG)) {
		/*
		* mdss interrupt is generated in mdp core clock domain
		* mdp clock need to be enabled to receive dsi interrupt
		* also, axi bus bandwidth need since dsi controller will
		* fetch dcs commands from axi bus
		*/
		rc = mdss_dsi_bus_bandwidth_vote(ctrl->shared_data, true);
		if (rc) {
			pr_err("%s: Bus bw vote failed\n", __func__);
			if (from_mdp)
				mutex_unlock(&ctrl->cmd_mutex);
			return rc;
		}

		if (ctrl->mdss_util->iommu_ctrl) {
			rc = ctrl->mdss_util->iommu_ctrl(1);
			if (IS_ERR_VALUE(rc)) {
				pr_err("IOMMU attach failed\n");
				mutex_unlock(&ctrl->cmd_mutex);
				return rc;
			}
			use_iommu = true;
		}
	}

	mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle, MDSS_DSI_ALL_CLKS,
			  MDSS_DSI_CLK_ON);

	/*
	 * In ping pong split cases, check if we need to apply a
	 * delay for any commands that are not coming from
	 * mdp path
	 */
	mutex_lock(&ctrl->mutex);
	if (mdss_dsi_delay_cmd(ctrl, from_mdp))
		ctrl->mdp_callback->fxn(ctrl->mdp_callback->data,
			MDP_INTF_CALLBACK_DSI_WAIT);
	mutex_unlock(&ctrl->mutex);

	if (req->flags & CMD_REQ_HS_MODE)
		mdss_dsi_set_tx_power_mode(0, &ctrl->panel_data);

	if (req->flags & CMD_REQ_RX)
		ret = mdss_dsi_cmdlist_rx(ctrl, req);
	else
		ret = mdss_dsi_cmdlist_tx(ctrl, req);

	if (req->flags & CMD_REQ_HS_MODE)
		mdss_dsi_set_tx_power_mode(1, &ctrl->panel_data);

	if (!(req->flags & CMD_REQ_DMA_TPG)) {
		if (use_iommu)
			ctrl->mdss_util->iommu_ctrl(0);

		(void)mdss_dsi_bus_bandwidth_vote(ctrl->shared_data, false);
	}

	mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle, MDSS_DSI_ALL_CLKS,
			MDSS_DSI_CLK_OFF);
need_lock:

	MDSS_XLOG(ctrl->ndx, from_mdp, ctrl->mdp_busy, current->pid,
							XLOG_FUNC_EXIT);

	if (from_mdp) { /* from mdp kickoff */
		/*
		 * when partial update enabled, the roi of pinfo
		 * is updated before mdp kickoff. Either width or
		 * height of roi is 0, then it is false kickoff so
		 * no mdp_busy flag set needed.
		 * when partial update disabled, mdp_busy flag
		 * alway set.
		 */
		if (!roi || (roi->w != 0 || roi->h != 0))
			mdss_dsi_cmd_mdp_start(ctrl);
		if (cmd_mutex_acquired)
			mutex_unlock(&ctrl->cmd_mutex);
	} else {	/* from dcs send */
		if (ctrl->shared_data->cmd_clk_ln_recovery_en &&
				ctrl->panel_mode == DSI_CMD_MODE &&
				(req && (req->flags & CMD_REQ_HS_MODE)))
			mdss_dsi_cmd_stop_hs_clk_lane(ctrl);
	}

	return ret;
}

static void __dsi_fifo_error_handler(struct mdss_dsi_ctrl_pdata *ctrl,
	bool recovery_needed)
{
	struct mdss_dsi_ctrl_pdata *sctrl;
	bool use_pp_split = false;

	use_pp_split = ctrl->panel_data.panel_info.use_pingpong_split;

	mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle, MDSS_DSI_ALL_CLKS,
		  MDSS_DSI_CLK_ON);
	mdss_dsi_sw_reset(ctrl, true);
	if (recovery_needed)
		ctrl->recovery->fxn(ctrl->recovery->data,
			MDP_INTF_DSI_CMD_FIFO_UNDERFLOW);
	mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle, MDSS_DSI_ALL_CLKS,
		  MDSS_DSI_CLK_OFF);

	sctrl = mdss_dsi_get_other_ctrl(ctrl);
	if (sctrl && use_pp_split) {
		mdss_dsi_clk_ctrl(sctrl, sctrl->dsi_clk_handle,
			MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_ON);
		mdss_dsi_sw_reset(sctrl, true);
		mdss_dsi_clk_ctrl(sctrl, sctrl->dsi_clk_handle,
			MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_OFF);
	}
}

static void dsi_send_events(struct mdss_dsi_ctrl_pdata *ctrl,
					u32 events, u32 arg)
{
	struct dsi_event_q *evq;

	if (!dsi_event.inited)
		return;

	pr_debug("%s: ev=%x\n", __func__, events);

	spin_lock(&dsi_event.event_lock);
	evq = &dsi_event.todo_list[dsi_event.event_pndx++];
	evq->todo = events;
	evq->arg = arg;
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
	u32 todo = 0, ln_status, force_clk_ln_hs;
	u32 arg;
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
		arg = evq->arg;
		evq->todo = 0;
		ev->event_gndx %= DSI_EVENT_Q_MAX;
		spin_unlock_irqrestore(&ev->event_lock, flag);

		pr_debug("%s: ev=%x\n", __func__, todo);

		if (todo & DSI_EV_PLL_UNLOCKED)
			mdss_dsi_pll_relock(ctrl);

		if (todo & DSI_EV_DLNx_FIFO_UNDERFLOW) {
			mutex_lock(&ctrl->mutex);
			if (ctrl->recovery) {
				pr_debug("%s: Handling underflow event\n",
							__func__);
				__dsi_fifo_error_handler(ctrl, true);
			}
			mutex_unlock(&ctrl->mutex);
		}

		if (todo & DSI_EV_DSI_FIFO_EMPTY) {
			__dsi_fifo_error_handler(ctrl, false);
		}

		if (todo & DSI_EV_DLNx_FIFO_OVERFLOW) {
			mutex_lock(&dsi_mtx);
			/*
			 * For targets other than msm8994,
			 * run the overflow recovery sequence only when
			 * data lanes are in stop state and
			 * clock lane is not in Stop State.
			 */
			ln_status = MIPI_INP(ctrl->ctrl_base + 0x00a8);
			force_clk_ln_hs = (MIPI_INP(ctrl->ctrl_base + 0x00ac)
					& BIT(28));
			pr_debug("%s: lane_status: 0x%x\n",
				       __func__, ln_status);
			if (ctrl->recovery
					&& (ctrl->shared_data->hw_rev
						!= MDSS_DSI_HW_REV_103)
					&& !(force_clk_ln_hs)
					&& (ln_status
						& DSI_DATA_LANES_STOP_STATE)
					&& !(ln_status
						& DSI_CLK_LANE_STOP_STATE)) {
				pr_debug("%s: Handling overflow event.\n",
								__func__);
				mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle,
						  MDSS_DSI_ALL_CLKS,
						  MDSS_DSI_CLK_ON);
				mdss_dsi_ctl_phy_reset(ctrl,
						DSI_EV_DLNx_FIFO_OVERFLOW);
				mdss_dsi_err_intr_ctrl(ctrl,
						DSI_INTR_ERROR_MASK, 1);
				mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle,
						  MDSS_DSI_ALL_CLKS,
						  MDSS_DSI_CLK_OFF);
			} else if (ctrl->recovery
					&& (ctrl->shared_data->hw_rev
					    == MDSS_DSI_HW_REV_103)) {
				pr_debug("%s: Handle overflow->Rev_103\n",
								__func__);
				mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle,
						  MDSS_DSI_ALL_CLKS,
						  MDSS_DSI_CLK_ON);
				mdss_dsi_ctl_phy_reset(ctrl,
						DSI_EV_DLNx_FIFO_OVERFLOW);
				mdss_dsi_err_intr_ctrl(ctrl,
						DSI_INTR_ERROR_MASK, 1);
				mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle,
						  MDSS_DSI_ALL_CLKS,
						  MDSS_DSI_CLK_OFF);
			}
			mutex_unlock(&dsi_mtx);
		}

		if (todo & DSI_EV_MDP_BUSY_RELEASE) {
			pr_debug("%s: Handling MDP_BUSY_RELEASE event\n",
							__func__);
			spin_lock_irqsave(&ctrl->mdp_lock, flag);
			ctrl->mdp_busy = false;
			mdss_dsi_disable_irq_nosync(ctrl, DSI_MDP_TERM);
			complete(&ctrl->mdp_comp);
			spin_unlock_irqrestore(&ctrl->mdp_lock, flag);

			/* enable dsi error interrupt */
			mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle,
					  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_ON);
			mdss_dsi_err_intr_ctrl(ctrl, DSI_INTR_ERROR_MASK, 1);
			mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle,
					  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_OFF);
		}

		if (todo & DSI_EV_STOP_HS_CLK_LANE)
			mdss_dsi_stop_hs_clk_lane(ctrl);

		if (todo & DSI_EV_LP_RX_TIMEOUT) {
			mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle,
					  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_ON);
			mdss_dsi_ctl_phy_reset(ctrl, DSI_EV_LP_RX_TIMEOUT);
			mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle,
					  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_OFF);
		}
	}

	return 0;
}

bool mdss_dsi_ack_err_status(struct mdss_dsi_ctrl_pdata *ctrl)
{
	u32 status;
	unsigned char *base;
	bool ret = false;

	base = ctrl->ctrl_base;

	status = MIPI_INP(base + 0x0068);/* DSI_ACK_ERR_STATUS */

	if (status) {
		MIPI_OUTP(base + 0x0068, status);
		/* Writing of an extra 0 needed to clear error bits */
		MIPI_OUTP(base + 0x0068, 0);
		/*
		 * After bta done, h/w may have a fake overflow and
		 * that overflow may further cause ack_err about 3 ms
		 * later which is another false alarm. Here the
		 * warning message is ignored.
		 */
		if (ctrl->panel_data.panel_info.esd_check_enabled &&
			(ctrl->status_mode == ESD_BTA) && (status & 0x1008000))
			return false;

		pr_err("%s: status=%x\n", __func__, status);
		ret = true;
	}

	return ret;
}

static bool mdss_dsi_timeout_status(struct mdss_dsi_ctrl_pdata *ctrl)
{
	u32 status;
	unsigned char *base;
	bool ret = false;

	base = ctrl->ctrl_base;

	status = MIPI_INP(base + 0x00c0);/* DSI_TIMEOUT_STATUS */

	if (status & 0x0111) {
		MIPI_OUTP(base + 0x00c0, status);
		if (status & 0x0110)
			dsi_send_events(ctrl, DSI_EV_LP_RX_TIMEOUT, 0);
		pr_err("%s: status=%x\n", __func__, status);
		ret = true;
	}

	return ret;
}

bool mdss_dsi_dln0_phy_err(struct mdss_dsi_ctrl_pdata *ctrl, bool print_en)
{
	u32 status;
	unsigned char *base;
	bool ret = false;

	base = ctrl->ctrl_base;

	status = MIPI_INP(base + 0x00b4);/* DSI_DLN0_PHY_ERR */

	if (status & 0x011111) {
		MIPI_OUTP(base + 0x00b4, status);
		if (print_en)
			pr_err("%s: status=%x\n", __func__, status);
		ctrl->err_cont.phy_err_cnt++;
		ret = true;
	}

	return ret;
}

static bool mdss_dsi_fifo_status(struct mdss_dsi_ctrl_pdata *ctrl)
{
	u32 status;
	unsigned char *base;
	bool ret = false;

	base = ctrl->ctrl_base;

	status = MIPI_INP(base + 0x000c);/* DSI_FIFO_STATUS */

	/* fifo underflow, overflow and empty*/
	if (status & 0xcccc4409) {
		MIPI_OUTP(base + 0x000c, status);

		pr_err("%s: status=%x\n", __func__, status);

		if (status & 0x44440000) {/* DLNx_HS_FIFO_OVERFLOW */
			dsi_send_events(ctrl, DSI_EV_DLNx_FIFO_OVERFLOW, 0);
			/* Ignore FIFO EMPTY when overflow happens */
			status = status & 0xeeeeffff;
		}
		if (status & 0x88880000)  /* DLNx_HS_FIFO_UNDERFLOW */
			dsi_send_events(ctrl, DSI_EV_DLNx_FIFO_UNDERFLOW, 0);
		if (status & 0x11110000) /* DLN_FIFO_EMPTY */
			dsi_send_events(ctrl, DSI_EV_DSI_FIFO_EMPTY, 0);
		ctrl->err_cont.fifo_err_cnt++;
		ret = true;
	}

	return ret;
}

static bool mdss_dsi_status(struct mdss_dsi_ctrl_pdata *ctrl)
{
	u32 status;
	unsigned char *base;
	bool ret = false;

	base = ctrl->ctrl_base;

	status = MIPI_INP(base + 0x0008);/* DSI_STATUS */

	if (status & 0x80000000) { /* INTERLEAVE_OP_CONTENTION */
		MIPI_OUTP(base + 0x0008, status);
		pr_err("%s: status=%x\n", __func__, status);
		ret = true;
	}

	return ret;
}

static bool mdss_dsi_clk_status(struct mdss_dsi_ctrl_pdata *ctrl)
{
	u32 status;
	unsigned char *base;
	bool ret = false;

	base = ctrl->ctrl_base;
	status = MIPI_INP(base + 0x0120);/* DSI_CLK_STATUS */

	if (status & 0x10000) { /* DSI_CLK_PLL_UNLOCKED */
		MIPI_OUTP(base + 0x0120, status);
		/* If PLL unlock is masked, do not report error */
		if (MIPI_INP(base + 0x10c) & BIT(28))
			return false;

		dsi_send_events(ctrl, DSI_EV_PLL_UNLOCKED, 0);
		pr_err("%s: status=%x\n", __func__, status);
		ret = true;
	}

	return ret;
}

static void __dsi_error_counter(struct dsi_err_container *err_container)
{
	s64 prev_time, curr_time;
	int prev_index;

	err_container->err_cnt++;

	err_container->index = (err_container->index + 1) %
		err_container->max_err_index;
	curr_time = ktime_to_ms(ktime_get());
	err_container->err_time[err_container->index] = curr_time;

	prev_index = (err_container->index + 1) % err_container->max_err_index;
	prev_time = err_container->err_time[prev_index];

	if (prev_time &&
		((curr_time - prev_time) < err_container->err_time_delta)) {
		pr_err("%s: panic in WQ as dsi error intrs within:%dms\n",
				__func__, err_container->err_time_delta);
		MDSS_XLOG_TOUT_HANDLER_WQ("mdp", "dsi0_ctrl", "dsi0_phy",
			"dsi1_ctrl", "dsi1_phy", "panic");
	}
}

void mdss_dsi_error(struct mdss_dsi_ctrl_pdata *ctrl)
{
	u32 intr, mask;
	bool err_handled = false;

	/* Ignore the interrupt if the error intr mask is not set */
	mask = MIPI_INP(ctrl->ctrl_base + 0x0110);
	if (!(mask & DSI_INTR_ERROR_MASK)) {
		pr_debug("%s: Ignore interrupt as error mask not set, 0x%x\n",
				__func__, mask);
		return;
	}

	/* disable dsi error interrupt */
	mdss_dsi_err_intr_ctrl(ctrl, DSI_INTR_ERROR_MASK, 0);

	/* DSI_ERR_INT_MASK0 */
	err_handled |= mdss_dsi_clk_status(ctrl);	/* Mask0, 0x10000000 */
	err_handled |= mdss_dsi_fifo_status(ctrl);	/* mask0, 0x133d00 */
	err_handled |= mdss_dsi_ack_err_status(ctrl);	/* mask0, 0x01f */
	err_handled |= mdss_dsi_timeout_status(ctrl);	/* mask0, 0x0e0 */
	err_handled |= mdss_dsi_status(ctrl);		/* mask0, 0xc0100 */
	err_handled |= mdss_dsi_dln0_phy_err(ctrl, true);/* mask0, 0x3e00000 */

	/* clear dsi error interrupt */
	intr = MIPI_INP(ctrl->ctrl_base + 0x0110);
	intr &= DSI_INTR_TOTAL_MASK;
	intr |= DSI_INTR_ERROR;
	MIPI_OUTP(ctrl->ctrl_base + 0x0110, intr);

	if (err_handled)
		__dsi_error_counter(&ctrl->err_cont);

	dsi_send_events(ctrl, DSI_EV_MDP_BUSY_RELEASE, 0);
}

irqreturn_t mdss_dsi_isr(int irq, void *ptr)
{
	u32 isr;
	u32 intr;
	struct mdss_dsi_ctrl_pdata *ctrl =
			(struct mdss_dsi_ctrl_pdata *)ptr;

	if (!ctrl->ctrl_base) {
		pr_err("%s:%d DSI base adr no Initialized",
						__func__, __LINE__);
		return IRQ_HANDLED;
	}

	isr = MIPI_INP(ctrl->ctrl_base + 0x0110);/* DSI_INTR_CTRL */
	MIPI_OUTP(ctrl->ctrl_base + 0x0110, (isr & ~DSI_INTR_ERROR));

	pr_debug("%s: ndx=%d isr=%x\n", __func__, ctrl->ndx, isr);

	if (isr & DSI_INTR_ERROR) {
		MDSS_XLOG(ctrl->ndx, ctrl->mdp_busy, isr, 0x97);
		mdss_dsi_error(ctrl);
	}

	if (isr & DSI_INTR_BTA_DONE) {
		MDSS_XLOG(ctrl->ndx, ctrl->mdp_busy, isr, 0x96);
		spin_lock(&ctrl->mdp_lock);
		mdss_dsi_disable_irq_nosync(ctrl, DSI_BTA_TERM);
		complete(&ctrl->bta_comp);
		/*
		 * When bta done happens, the panel should be in good
		 * state. However, bta could cause the fake overflow
		 * error for video mode. The similar issue happens when
		 * sending dcs cmd. This overflow further causes
		 * flicking because of phy reset which is unncessary,
		 * so here overflow error is ignored, and errors are
		 * cleared.
		 */
		if (ctrl->panel_data.panel_info.esd_check_enabled &&
			(ctrl->status_mode == ESD_BTA) &&
			(ctrl->panel_mode == DSI_VIDEO_MODE)) {
			isr &= ~DSI_INTR_ERROR;
			/* clear only overflow */
			mdss_dsi_set_reg(ctrl, 0x0c, 0x44440000, 0x44440000);
		}
		spin_unlock(&ctrl->mdp_lock);
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
		mdss_dsi_disable_irq_nosync(ctrl, DSI_MDP_TERM);
		if (ctrl->shared_data->cmd_clk_ln_recovery_en &&
				ctrl->panel_mode == DSI_CMD_MODE) {
			/* stop force clk lane hs */
			mdss_dsi_cfg_lane_ctrl(ctrl, BIT(28), 0);
			dsi_send_events(ctrl, DSI_EV_STOP_HS_CLK_LANE,
							DSI_MDP_TERM);
		}
		ctrl->mdp_busy = false;
		complete_all(&ctrl->mdp_comp);
		spin_unlock(&ctrl->mdp_lock);
	}

	if (isr & DSI_INTR_DYNAMIC_REFRESH_DONE) {
		spin_lock(&ctrl->mdp_lock);
		mdss_dsi_disable_irq_nosync(ctrl, DSI_DYNAMIC_TERM);

		/* clear dfps interrupt */
		intr = MIPI_INP(ctrl->ctrl_base + 0x0110);
		intr |= DSI_INTR_DYNAMIC_REFRESH_DONE;
		MIPI_OUTP(ctrl->ctrl_base + 0x0110, intr);

		complete(&ctrl->dynamic_comp);
		spin_unlock(&ctrl->mdp_lock);
	}

	return IRQ_HANDLED;
}
