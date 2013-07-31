/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#define DSI_POLL_SLEEP_US 1000
#define DSI_POLL_TIMEOUT_US 16000
#define DSI_ESC_CLK_RATE 19200000
#define DSI_DMA_CMD_TIMEOUT_MS 200

struct dsi_host_v2_private {
	struct completion dma_comp;
	int irq_enabled;
	spinlock_t irq_lock;

	int irq_no;
	unsigned char *dsi_base;
	size_t dsi_reg_size;
	struct device dis_dev;

	void (*debug_enable_clk)(int on);
};

static struct dsi_host_v2_private *dsi_host_private;

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

	init_completion(&dsi_host_private->dma_comp);
	spin_lock_init(&dsi_host_private->irq_lock);
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
		pr_debug("%s: status=%x\n", __func__, status);
	}
}

void msm_dsi_timeout_status(unsigned char *ctrl_base)
{
	u32 status;

	status = MIPI_INP(ctrl_base + DSI_TIMEOUT_STATUS);
	if (status & 0x0111) {
		MIPI_OUTP(ctrl_base + DSI_TIMEOUT_STATUS, status);
		pr_debug("%s: status=%x\n", __func__, status);
	}
}

void msm_dsi_dln0_phy_err(unsigned char *ctrl_base)
{
	u32 status;

	status = MIPI_INP(ctrl_base + DSI_DLN0_PHY_ERR);

	if (status & 0x011111) {
		MIPI_OUTP(ctrl_base + DSI_DLN0_PHY_ERR, status);
		pr_debug("%s: status=%x\n", __func__, status);
	}
}

void msm_dsi_fifo_status(unsigned char *ctrl_base)
{
	u32 status;

	status = MIPI_INP(ctrl_base + DSI_FIFO_STATUS);

	if (status & 0x44444489) {
		MIPI_OUTP(ctrl_base + DSI_FIFO_STATUS, status);
		pr_debug("%s: status=%x\n", __func__, status);
	}
}

void msm_dsi_status(unsigned char *ctrl_base)
{
	u32 status;

	status = MIPI_INP(ctrl_base + DSI_STATUS);

	if (status & 0x80000000) {
		MIPI_OUTP(ctrl_base + DSI_STATUS, status);
		pr_debug("%s: status=%x\n", __func__, status);
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

void msm_dsi_enable_irq(void)
{
	unsigned long flags;

	spin_lock_irqsave(&dsi_host_private->irq_lock, flags);
	dsi_host_private->irq_enabled++;
	if (dsi_host_private->irq_enabled == 1)
		enable_irq(dsi_host_private->irq_no);

	spin_unlock_irqrestore(&dsi_host_private->irq_lock, flags);
}

void msm_dsi_disable_irq(void)
{
	unsigned long flags;

	spin_lock_irqsave(&dsi_host_private->irq_lock, flags);
	dsi_host_private->irq_enabled--;
	if (dsi_host_private->irq_enabled == 0)
		disable_irq(dsi_host_private->irq_no);

	spin_unlock_irqrestore(&dsi_host_private->irq_lock, flags);
}

void msm_dsi_disable_irq_nosync(void)
{
	spin_lock(&dsi_host_private->irq_lock);
	dsi_host_private->irq_enabled--;
	if (dsi_host_private->irq_enabled == 0)
		disable_irq_nosync(dsi_host_private->irq_no);
	spin_unlock(&dsi_host_private->irq_lock);
}

irqreturn_t msm_dsi_isr(int irq, void *ptr)
{
	u32 isr;

	isr = MIPI_INP(dsi_host_private->dsi_base + DSI_INT_CTRL);
	MIPI_OUTP(dsi_host_private->dsi_base + DSI_INT_CTRL, isr);

	if (isr & DSI_INTR_ERROR)
		msm_dsi_error(dsi_host_private->dsi_base);

	if (isr & DSI_INTR_CMD_DMA_DONE)
		complete(&dsi_host_private->dma_comp);

	return IRQ_HANDLED;
}

int msm_dsi_irq_init(struct device *dev, int irq_no)
{
	int ret;

	ret = devm_request_irq(dev, irq_no, msm_dsi_isr,
				IRQF_DISABLED, "DSI", NULL);
	if (ret) {
		pr_err("msm_dsi_irq_init request_irq() failed!\n");
		return ret;
	}
	dsi_host_private->irq_no = irq_no;
	disable_irq(irq_no);
	return 0;
}

void msm_dsi_host_init(struct mipi_panel_info *pinfo)
{
	u32 dsi_ctrl, intr_ctrl, data;
	unsigned char *ctrl_base = dsi_host_private->dsi_base;

	pr_debug("msm_dsi_host_init\n");
	pinfo->rgb_swap = DSI_RGB_SWAP_RGB;

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
	MIPI_OUTP(ctrl_base + DSI_LANE_SWAP_CTRL, pinfo->dlane_swap);

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

	intr_ctrl |= DSI_INTR_ERROR_MASK;
	MIPI_OUTP(ctrl_base + DSI_INT_CTRL, intr_ctrl);

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
				DSI_POLL_SLEEP_US, DSI_POLL_TIMEOUT_US))
		pr_err("%s: DSI status=%x failed\n", __func__, status);

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
	u32 dsi_ctrl, intr_ctrl;
	unsigned char *ctrl_base = dsi_host_private->dsi_base;

	pr_debug("msm_dsi_op_mode_config\n");

	dsi_ctrl = MIPI_INP(ctrl_base + DSI_CTRL);
	/*If Video enabled, Keep Video and Cmd mode ON */


	dsi_ctrl &= ~0x06;

	if (mode == DSI_VIDEO_MODE) {
		dsi_ctrl |= 0x02;
		intr_ctrl = DSI_INTR_CMD_DMA_DONE_MASK;
	} else {		/* command mode */
		dsi_ctrl |= 0x04;

		intr_ctrl = DSI_INTR_CMD_DMA_DONE_MASK | DSI_INTR_ERROR_MASK |
				DSI_INTR_CMD_MDP_DONE_MASK;
	}

	pr_debug("%s: dsi_ctrl=%x intr=%x\n", __func__, dsi_ctrl, intr_ctrl);

	MIPI_OUTP(ctrl_base + DSI_INT_CTRL, intr_ctrl);
	MIPI_OUTP(ctrl_base + DSI_CTRL, dsi_ctrl);
	wmb();
}

int msm_dsi_cmd_reg_tx(u32 data)
{
	unsigned char *ctrl_base = dsi_host_private->dsi_base;

	MIPI_OUTP(ctrl_base + DSI_TRIG_CTRL, 0x04);/* sw trigger */
	MIPI_OUTP(ctrl_base + DSI_CTRL, 0x135);
	wmb();

	MIPI_OUTP(ctrl_base + DSI_COMMAND_MODE_DMA_CTRL, data);
	wmb();
	MIPI_OUTP(ctrl_base + DSI_CMD_MODE_DMA_SW_TRIGGER, 0x01);
	wmb();

	udelay(300); /*per spec*/

	return 0;
}

int msm_dsi_cmd_dma_tx(struct dsi_buf *tp)
{
	int len, rc;
	unsigned long size, addr;
	unsigned char *ctrl_base = dsi_host_private->dsi_base;

	len = ALIGN(tp->len, 4);
	size = ALIGN(tp->len, SZ_4K);

	tp->dmap = dma_map_single(&dsi_host_private->dis_dev, tp->data, size,
				DMA_TO_DEVICE);
	if (dma_mapping_error(&dsi_host_private->dis_dev, tp->dmap)) {
		pr_err("%s: dmap mapp failed\n", __func__);
		return -ENOMEM;
	}

	addr = tp->dmap;

	INIT_COMPLETION(dsi_host_private->dma_comp);

	MIPI_OUTP(ctrl_base + DSI_DMA_CMD_OFFSET, addr);
	MIPI_OUTP(ctrl_base + DSI_DMA_CMD_LENGTH, len);
	wmb();

	MIPI_OUTP(ctrl_base + DSI_CMD_MODE_DMA_SW_TRIGGER, 0x01);
	wmb();

	rc = wait_for_completion_timeout(&dsi_host_private->dma_comp,
				msecs_to_jiffies(DSI_DMA_CMD_TIMEOUT_MS));
	if (rc == 0) {
		pr_err("DSI command transaction time out\n");
		rc = -ETIME;
	}

	dma_unmap_single(&dsi_host_private->dis_dev, tp->dmap, size,
			DMA_TO_DEVICE);
	tp->dmap = 0;
	return rc;
}

int msm_dsi_cmd_dma_rx(struct dsi_buf *rp, int rlen)
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

	return 0;
}

int msm_dsi_cmds_tx(struct mdss_panel_data *pdata,
			struct dsi_buf *tp, struct dsi_cmd_desc *cmds, int cnt)
{
	struct dsi_cmd_desc *cm;
	u32 dsi_ctrl, ctrl;
	int i, video_mode;
	unsigned char *ctrl_base = dsi_host_private->dsi_base;

	/* turn on cmd mode
	* for video mode, do not send cmds more than
	* one pixel line, since it only transmit it
	* during BLLP.
	*/
	dsi_ctrl = MIPI_INP(ctrl_base + DSI_CTRL);
	video_mode = dsi_ctrl & 0x02; /* VIDEO_MODE_EN */
	if (video_mode) {
		ctrl = dsi_ctrl | 0x04; /* CMD_MODE_EN */
		MIPI_OUTP(ctrl_base + DSI_CTRL, ctrl);
	}

	msm_dsi_enable_irq();

	cm = cmds;
	for (i = 0; i < cnt; i++) {
		dsi_buf_init(tp);
		dsi_cmd_dma_add(tp, cm);
		msm_dsi_cmd_dma_tx(tp);
		if (cm->dchdr.wait)
			msleep(cm->dchdr.wait);
		cm++;
	}

	msm_dsi_disable_irq();

	if (video_mode)
		MIPI_OUTP(ctrl_base + DSI_CTRL, dsi_ctrl);
	return 0;
}

/* MDSS_DSI_MRPS, Maximum Return Packet Size */
static char max_pktsize[2] = {0x00, 0x00}; /* LSB tx first, 10 bytes */

static struct dsi_cmd_desc pkt_size_cmd[] = {
	{{DTYPE_MAX_PKTSIZE, 1, 0, 0, 0,
		sizeof(max_pktsize)}, max_pktsize}
};

/*
 * DSI panel reply with  MAX_RETURN_PACKET_SIZE bytes of data
 * plus DCS header, ECC and CRC for DCS long read response
 * mdss_dsi_controller only have 4x32 bits register ( 16 bytes) to
 * hold data per transaction.
 * MDSS_DSI_LEN equal to 8
 * len should be either 4 or 8
 * any return data more than MDSS_DSI_LEN need to be break down
 * to multiple transactions.
 *
 * ov_mutex need to be acquired before call this function.
 */
int msm_dsi_cmds_rx(struct mdss_panel_data *pdata,
			struct dsi_buf *tp, struct dsi_buf *rp,
			struct dsi_cmd_desc *cmds, int rlen)
{
	int cnt, len, diff, pkt_size;
	char cmd;

	if (pdata->panel_info.mipi.no_max_pkt_size)
		rlen = ALIGN(rlen, 4); /* Only support rlen = 4*n */

	len = rlen;
	diff = 0;

	if (len <= 2) {
		cnt = 4;	/* short read */
	} else {
		if (len > MDSS_DSI_LEN)
			len = MDSS_DSI_LEN;	/* 8 bytes at most */

		len = ALIGN(len, 4); /* len 4 bytes align */
		diff = len - rlen;
		/*
		 * add extra 2 bytes to len to have overall
		 * packet size is multipe by 4. This also make
		 * sure 4 bytes dcs headerlocates within a
		 * 32 bits register after shift in.
		 * after all, len should be either 6 or 10.
		 */
		len += 2;
		cnt = len + 6; /* 4 bytes header + 2 bytes crc */
	}

	msm_dsi_enable_irq();

	if (!pdata->panel_info.mipi.no_max_pkt_size) {
		/* packet size need to be set at every read */
		pkt_size = len;
		max_pktsize[0] = pkt_size;
		dsi_buf_init(tp);
		dsi_cmd_dma_add(tp, pkt_size_cmd);
		msm_dsi_cmd_dma_tx(tp);
		pr_debug("%s: Max packet size sent\n", __func__);
	}

	dsi_buf_init(tp);
	dsi_cmd_dma_add(tp, cmds);

	/* transmit read comamnd to client */
	msm_dsi_cmd_dma_tx(tp);
	/*
	 * once cmd_dma_done interrupt received,
	 * return data from client is ready and stored
	 * at RDBK_DATA register already
	 */
	dsi_buf_init(rp);
	if (pdata->panel_info.mipi.no_max_pkt_size) {
		/*
		 * expect rlen = n * 4
		 * short alignement for start addr
		 */
		rp->data += 2;
	}

	msm_dsi_cmd_dma_rx(rp, cnt);

	msm_dsi_disable_irq();

	if (pdata->panel_info.mipi.no_max_pkt_size) {
		/*
		 * remove extra 2 bytes from previous
		 * rx transaction at shift register
		 * which was inserted during copy
		 * shift registers to rx buffer
		 * rx payload start from long alignment addr
		 */
		rp->data += 2;
	}

	cmd = rp->data[0];
	switch (cmd) {
	case DTYPE_ACK_ERR_RESP:
		pr_debug("%s: rx ACK_ERR_PACLAGE\n", __func__);
		break;
	case DTYPE_GEN_READ1_RESP:
	case DTYPE_DCS_READ1_RESP:
		dsi_short_read1_resp(rp);
		break;
	case DTYPE_GEN_READ2_RESP:
	case DTYPE_DCS_READ2_RESP:
		dsi_short_read2_resp(rp);
		break;
	case DTYPE_GEN_LREAD_RESP:
	case DTYPE_DCS_LREAD_RESP:
		dsi_long_read_resp(rp);
		rp->len -= 2; /* extra 2 bytes added */
		rp->len -= diff; /* align bytes */
		break;
	default:
		pr_debug("%s: Unknown cmd received\n", __func__);
		break;
	}

	return rp->len;
}

static int msm_dsi_cal_clk_rate(struct mdss_panel_data *pdata,
				u32 *bitclk_rate,
				u32 *dsiclk_rate,
				u32 *byteclk_rate,
				u32 *pclk_rate)
{
	struct mdss_panel_info *pinfo;
	struct mipi_panel_info *mipi;
	u32 hbp, hfp, vbp, vfp, hspw, vspw, width, height;
	int lanes;

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
	*bitclk_rate /= lanes;

	*byteclk_rate = *bitclk_rate / 8;
	*dsiclk_rate = *byteclk_rate * lanes;
	*pclk_rate = *byteclk_rate * lanes * 8 / pdata->panel_info.bpp;

	pr_debug("dsiclk_rate=%u, byteclk=%u, pck_=%u\n",
		*dsiclk_rate, *byteclk_rate, *pclk_rate);
	return 0;
}

static int msm_dsi_on(struct mdss_panel_data *pdata)
{
	int ret = 0;
	u32 clk_rate;
	struct mdss_panel_info *pinfo;
	struct mipi_panel_info *mipi;
	u32 hbp, hfp, vbp, vfp, hspw, vspw, width, height;
	u32 ystride, bpp, data;
	u32 dummy_xres, dummy_yres;
	u32 bitclk_rate = 0, byteclk_rate = 0, pclk_rate = 0, dsiclk_rate = 0;
	unsigned char *ctrl_base = dsi_host_private->dsi_base;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	pr_debug("msm_dsi_on\n");

	pinfo = &pdata->panel_info;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	ret = msm_dss_enable_vreg(
		ctrl_pdata->power_data.vreg_config,
		ctrl_pdata->power_data.num_vreg, 1);
	if (ret) {
		pr_err("%s: DSI power on failed\n", __func__);
		return ret;
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
	msm_dsi_host_init(mipi);

	if (mipi->force_clk_lane_hs) {
		u32 tmp;

		tmp = MIPI_INP(ctrl_base + DSI_LANE_CTRL);
		tmp |= (1<<28);
		MIPI_OUTP(ctrl_base + DSI_LANE_CTRL, tmp);
		wmb();
	}

	msm_dsi_op_mode_config(mipi->mode, pdata);

	return ret;
}

static int msm_dsi_off(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		ret = -EINVAL;
		return ret;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_debug("msm_dsi_off\n");
	msm_dsi_controller_cfg(0);
	msm_dsi_clk_set_rate(DSI_ESC_CLK_RATE, 0, 0, 0);
	msm_dsi_clk_disable();
	msm_dsi_unprepare_clocks();
	msm_dsi_phy_off(dsi_host_private->dsi_base);
	msm_dsi_ahb_ctrl(0);

	ret = msm_dss_enable_vreg(
		ctrl_pdata->power_data.vreg_config,
		ctrl_pdata->power_data.num_vreg, 0);
	if (ret) {
		pr_err("%s: Panel power off failed\n", __func__);
		return ret;
	}

	return ret;
}

static int msm_dsi_cont_on(struct mdss_panel_data *pdata)
{
	struct mdss_panel_info *pinfo;
	int ret = 0;
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
	ret = msm_dss_enable_vreg(
		ctrl_pdata->power_data.vreg_config,
		ctrl_pdata->power_data.num_vreg, 1);
	if (ret) {
		pr_err("%s: DSI power on failed\n", __func__);
		return ret;
	}

	msm_dsi_ahb_ctrl(1);
	msm_dsi_prepare_clocks();
	msm_dsi_clk_enable();
	return 0;
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
				dsi_host_private->dsi_reg_size);

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
		dsi_pan_node = of_parse_phandle(
			pdev->dev.of_node,
			"qcom,dsi-pref-prim-pan", 0);
		if (!dsi_pan_node) {
			pr_err("%s:can't find panel phandle\n",
			       __func__);
			return NULL;
		}
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
			return NULL;
		}
	}
	return dsi_pan_node;
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
	} else {
		rc = msm_dsi_irq_init(&pdev->dev, mdss_dsi_mres->start);
		if (rc) {
			dev_err(&pdev->dev, "%s: failed to init irq, rc=%d\n",
								__func__, rc);
			goto error_irq_resource;
		}
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

	rc = msm_dsi_io_init(pdev, &(ctrl_pdata->power_data));
	if (rc) {
		dev_err(&pdev->dev, "%s: failed to init DSI IO, rc=%d\n",
								__func__, rc);
		goto error_io_init;
	}

	pr_debug("%s: Dsi Ctrl->0 initialized\n", __func__);

	dsi_host_private->dis_dev = pdev->dev;
	intf.on = msm_dsi_on;
	intf.off = msm_dsi_off;
	intf.cont_on = msm_dsi_cont_on;
	intf.op_mode_config = msm_dsi_op_mode_config;
	intf.tx = msm_dsi_cmds_tx;
	intf.rx = msm_dsi_cmds_rx;
	intf.index = 0;
	intf.private = NULL;
	dsi_register_interface(&intf);

	msm_dsi_debug_init();

	rc = dsi_panel_device_register_v2(pdev, ctrl_pdata);
	if (rc) {
		pr_err("%s: dsi panel dev reg failed\n", __func__);
		goto error_device_register;
	}
	pr_debug("%s success\n", __func__);
	return 0;
error_device_register:
	msm_dsi_io_deinit(pdev, &(ctrl_pdata->power_data));
error_io_init:
	dsi_ctrl_config_deinit(pdev, ctrl_pdata);
error_pan_node:
	of_node_put(dsi_pan_node);
error_platform_pop:
	msm_dsi_disable_irq();
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
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = platform_get_drvdata(pdev);
	if (!ctrl_pdata) {
		pr_err("%s: no driver data\n", __func__);
		return -ENODEV;
	}

	msm_dsi_disable_irq();
	msm_dsi_io_deinit(pdev, &(ctrl_pdata->power_data));
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
