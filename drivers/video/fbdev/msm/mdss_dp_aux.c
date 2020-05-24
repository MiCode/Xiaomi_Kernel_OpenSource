// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2013-2014, 2016-2017 The Linux Foundation. All rights reserved. */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/semaphore.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/bug.h>
#include <linux/of_gpio.h>

#include "mdss_panel.h"
#include "mdss_dp.h"
#include "mdss_dp_util.h"

static void dp_sink_parse_test_request(struct mdss_dp_drv_pdata *ep);
static void dp_sink_parse_sink_count(struct mdss_dp_drv_pdata *ep);

/*
 * edp buffer operation
 */
static char *dp_buf_init(struct edp_buf *eb, char *buf, int size)
{
	eb->start = buf;
	eb->size = size;
	eb->data = eb->start;
	eb->end = eb->start + eb->size;
	eb->len = 0;
	eb->trans_num = 0;
	eb->i2c = 0;
	return eb->data;
}

static char *dp_buf_reset(struct edp_buf *eb)
{
	eb->data = eb->start;
	eb->len = 0;
	eb->trans_num = 0;
	eb->i2c = 0;
	return eb->data;
}

static char *dp_buf_push(struct edp_buf *eb, int len)
{
	eb->data += len;
	eb->len += len;
	return eb->data;
}

static int dp_buf_trailing(struct edp_buf *eb)
{
	return (int)(eb->end - eb->data);
}

static void mdss_dp_aux_clear_hw_interrupts(void __iomem *phy_base)
{
	u32 data;

	data = dp_read(phy_base + DP_PHY_AUX_INTERRUPT_STATUS);
	pr_debug("PHY_AUX_INTERRUPT_STATUS=0x%08x\n", data);

	dp_write(phy_base + DP_PHY_AUX_INTERRUPT_CLEAR, 0x1f);
	dp_write(phy_base + DP_PHY_AUX_INTERRUPT_CLEAR, 0x9f);
	dp_write(phy_base + DP_PHY_AUX_INTERRUPT_CLEAR, 0);

	/* Ensure that all interrupts are cleared and acked */
	wmb();
}

/*
 * edp aux dp_buf_add_cmd:
 * NO native and i2c command mix allowed
 */
static int dp_buf_add_cmd(struct edp_buf *eb, struct edp_cmd *cmd)
{
	char data;
	char *bp, *cp;
	int i, len;

	if (cmd->read)	/* read */
		len = 4;
	else
		len = cmd->len + 4;

	if (dp_buf_trailing(eb) < len)
		return 0;

	/*
	 * cmd fifo only has depth of 144 bytes
	 * limit buf length to 128 bytes here
	 */
	if ((eb->len + len) > 128)
		return 0;

	bp = eb->data;
	data = cmd->addr >> 16;
	data &=  0x0f;	/* 4 addr bits */
	if (cmd->read)
		data |=  BIT(4);
	*bp++ = data;
	*bp++ = cmd->addr >> 8;
	*bp++ = cmd->addr;
	*bp++ = cmd->len - 1;

	if (!cmd->read) { /* write */
		cp = cmd->datap;
		for (i = 0; i < cmd->len; i++)
			*bp++ = *cp++;
	}
	dp_buf_push(eb, len);

	if (cmd->i2c)
		eb->i2c++;

	eb->trans_num++;	/* Increase transaction number */

	return cmd->len - 1;
}

static int dp_cmd_fifo_tx(struct mdss_dp_drv_pdata *dp)
{
	u32 data;
	char *datap;
	int len, cnt;
	struct edp_buf *tp = &dp->txp;
	void __iomem *base = dp->base;


	len = tp->len;	/* total byte to cmd fifo */
	if (len == 0)
		return 0;

	cnt = 0;
	datap = tp->start;

	while (cnt < len) {
		data = *datap; /* data byte */
		data <<= 8;
		data &= 0x00ff00; /* index = 0, write */
		if (cnt == 0)
			data |= BIT(31);  /* INDEX_WRITE */
		dp_write(base + DP_AUX_DATA, data);
		cnt++;
		datap++;
	}

	/* clear the current tx request before queuing a new one */
	dp_write(base + DP_AUX_TRANS_CTRL, 0);

	/* clear any previous PHY AUX interrupts */
	mdss_dp_aux_clear_hw_interrupts(dp->phy_io.base);

	data = (tp->trans_num - 1);
	if (tp->i2c) {
		data |= BIT(8); /* I2C */
		if (tp->no_send_addr)
			data |= BIT(10); /* NO SEND ADDR */
		if (tp->no_send_stop)
			data |= BIT(11); /* NO SEND STOP */
	}

	data |= BIT(9); /* GO */
	dp_write(base + DP_AUX_TRANS_CTRL, data);

	return tp->len;
}

static int dp_cmd_fifo_rx(struct edp_buf *rp, int len, unsigned char *base)
{
	u32 data;
	char *dp;
	int i, actual_i;

	data = 0; /* index = 0 */
	data |= BIT(31);  /* INDEX_WRITE */
	data |= BIT(0);	/* read */
	dp_write(base + DP_AUX_DATA, data);

	dp = rp->data;

	/* discard first byte */
	data = dp_read(base + DP_AUX_DATA);
	for (i = 0; i < len; i++) {
		data = dp_read(base + DP_AUX_DATA);
		*dp++ = (char)((data >> 8) & 0xFF);

		actual_i = (data >> 16) & 0xFF;
		if (i != actual_i)
			pr_warn("Index mismatch: expected %d, found %d\n",
				i, actual_i);
	}

	rp->len = len;
	return len;
}

static int dp_aux_write_cmds(struct mdss_dp_drv_pdata *ep,
					struct edp_cmd *cmd)
{
	struct edp_cmd *cm;
	struct edp_buf *tp;
	int len, ret;

	mutex_lock(&ep->aux_mutex);
	ep->aux_cmd_busy = 1;

	tp = &ep->txp;
	dp_buf_reset(tp);

	cm = cmd;
	while (cm) {
		ret = dp_buf_add_cmd(tp, cm);
		if (ret <= 0)
			break;
		if (cm->next == 0)
			break;
		cm++;
	}

	if (tp->i2c)
		ep->aux_cmd_i2c = 1;
	else
		ep->aux_cmd_i2c = 0;

	reinit_completion(&ep->aux_comp);

	tp->no_send_addr = true;
	tp->no_send_stop = true;
	len = dp_cmd_fifo_tx(ep);

	if (!wait_for_completion_timeout(&ep->aux_comp, HZ/4)) {
		pr_err("aux write timeout\n");
		ep->aux_error_num = EDP_AUX_ERR_TOUT;
		/* Reset the AUX controller state machine */
		mdss_dp_aux_reset(&ep->ctrl_io);
	}

	if (ep->aux_error_num == EDP_AUX_ERR_NONE)
		ret = len;
	else
		ret = ep->aux_error_num;

	ep->aux_cmd_busy = 0;
	mutex_unlock(&ep->aux_mutex);
	return  ret;
}

static int dp_aux_read_cmds(struct mdss_dp_drv_pdata *ep,
				struct edp_cmd *cmds)
{
	struct edp_cmd *cm;
	struct edp_buf *tp;
	struct edp_buf *rp;
	int len, ret;
	u32 data;

	mutex_lock(&ep->aux_mutex);
	ep->aux_cmd_busy = 1;

	tp = &ep->txp;
	rp = &ep->rxp;
	dp_buf_reset(tp);
	dp_buf_reset(rp);

	cm = cmds;
	len = 0;
	while (cm) {
		ret = dp_buf_add_cmd(tp, cm);
		len += cm->len;
		if (ret <= 0)
			break;
		if (cm->next == 0)
			break;
		cm++;
	}

	if (tp->i2c)
		ep->aux_cmd_i2c = 1;
	else
		ep->aux_cmd_i2c = 0;

	reinit_completion(&ep->aux_comp);

	tp->no_send_addr = true;
	tp->no_send_stop = false;
	dp_cmd_fifo_tx(ep);

	if (!wait_for_completion_timeout(&ep->aux_comp, HZ/4)) {
		pr_err("aux read timeout\n");
		ep->aux_error_num = EDP_AUX_ERR_TOUT;
		/* Reset the AUX controller state machine */
		mdss_dp_aux_reset(&ep->ctrl_io);
		ret = ep->aux_error_num;
		goto end;
	}

	/* clear the current rx request before queuing a new one */
	data = dp_read(ep->base + DP_AUX_TRANS_CTRL);
	data &= (~BIT(9));
	dp_write(ep->base + DP_AUX_TRANS_CTRL, data);
	if (ep->aux_error_num == EDP_AUX_ERR_NONE) {
		ret = dp_cmd_fifo_rx(rp, len, ep->base);

		if (cmds->out_buf)
			memcpy(cmds->out_buf, rp->data, cmds->len);

	} else {
		ret = ep->aux_error_num;
	}

end:
	ep->aux_cmd_busy = 0;
	mutex_unlock(&ep->aux_mutex);

	return ret;
}

void dp_aux_native_handler(struct mdss_dp_drv_pdata *ep, u32 isr)
{
	pr_debug("isr=0x%08x\n", isr);
	if (isr & EDP_INTR_AUX_I2C_DONE) {
		ep->aux_error_num = EDP_AUX_ERR_NONE;
	} else if (isr & EDP_INTR_WRONG_ADDR) {
		ep->aux_error_num = EDP_AUX_ERR_ADDR;
	} else if (isr & EDP_INTR_TIMEOUT) {
		ep->aux_error_num = EDP_AUX_ERR_TOUT;
	} else if (isr & EDP_INTR_NACK_DEFER) {
		ep->aux_error_num = EDP_AUX_ERR_NACK;
	} else if (isr & EDP_INTR_PHY_AUX_ERR) {
		ep->aux_error_num = EDP_AUX_ERR_PHY;
		mdss_dp_aux_clear_hw_interrupts(ep->phy_io.base);
	} else {
		ep->aux_error_num = EDP_AUX_ERR_NONE;
	}

	complete(&ep->aux_comp);
}

void dp_aux_i2c_handler(struct mdss_dp_drv_pdata *ep, u32 isr)
{
	pr_debug("isr=0x%08x\n", isr);
	if (isr & EDP_INTR_AUX_I2C_DONE) {
		if (isr & (EDP_INTR_I2C_NACK | EDP_INTR_I2C_DEFER))
			ep->aux_error_num = EDP_AUX_ERR_NACK;
		else
			ep->aux_error_num = EDP_AUX_ERR_NONE;
	} else {
		if (isr & EDP_INTR_WRONG_ADDR) {
			ep->aux_error_num = EDP_AUX_ERR_ADDR;
		} else if (isr & EDP_INTR_TIMEOUT) {
			ep->aux_error_num = EDP_AUX_ERR_TOUT;
		} else if (isr & EDP_INTR_NACK_DEFER) {
			ep->aux_error_num = EDP_AUX_ERR_NACK_DEFER;
		} else if (isr & EDP_INTR_I2C_NACK) {
			ep->aux_error_num = EDP_AUX_ERR_NACK;
		} else if (isr & EDP_INTR_I2C_DEFER) {
			ep->aux_error_num = EDP_AUX_ERR_DEFER;
		} else if (isr & EDP_INTR_PHY_AUX_ERR) {
			ep->aux_error_num = EDP_AUX_ERR_PHY;
			mdss_dp_aux_clear_hw_interrupts(ep->phy_io.base);
		} else {
			ep->aux_error_num = EDP_AUX_ERR_NONE;
		}
	}

	complete(&ep->aux_comp);
}

static int dp_aux_rw_cmds_retry(struct mdss_dp_drv_pdata *dp,
	struct edp_cmd *cmd, enum dp_aux_transaction transaction)
{
	int const retry_count = 5;
	int adjust_count = 0;
	int i;
	u32 aux_cfg1_config_count;
	int ret;
	bool connected = false;

	aux_cfg1_config_count = mdss_dp_phy_aux_get_config_cnt(dp,
			PHY_AUX_CFG1);
retry:
	i = 0;
	ret = 0;
	do {
		struct edp_cmd cmd1 = *cmd;

		mutex_lock(&dp->attention_lock);
		connected = dp->cable_connected;
		mutex_unlock(&dp->attention_lock);

		if (!connected) {
			pr_err("dp cable disconnected\n");
			ret = -ENODEV;
			goto end;
		}

		dp->aux_error_num = EDP_AUX_ERR_NONE;
		pr_debug("Trying %s, iteration count: %d\n",
			mdss_dp_aux_transaction_to_string(transaction),
			i + 1);
		if (transaction == DP_AUX_READ)
			ret = dp_aux_read_cmds(dp, &cmd1);
		else if (transaction == DP_AUX_WRITE)
			ret = dp_aux_write_cmds(dp, &cmd1);

		i++;
	} while ((i < retry_count) && (ret < 0));

	if (ret >= 0) /* rw success */
		goto end;

	if (adjust_count >= aux_cfg1_config_count) {
		pr_err("PHY_AUX_CONFIG1 calibration failed\n");
		goto end;
	}

	/* Adjust AUX configuration and retry */
	pr_debug("AUX failure (%d), adjust AUX settings\n", ret);
	mdss_dp_phy_aux_update_config(dp, PHY_AUX_CFG1);
	adjust_count++;
	goto retry;

end:
	return ret;
}

/**
 * dp_aux_write_buf_retry() - send a AUX write command
 * @dp: display port driver data
 * @addr: AUX address (in hex) to write the command to
 * @buf: the buffer containing the actual payload
 * @len: the length of the buffer @buf
 * @i2c: indicates if it is an i2c-over-aux transaction
 * @retry: specifies if retries should be attempted upon failures
 *
 * Send an AUX write command with the specified payload over the AUX
 * channel. This function can send both native AUX command or an
 * i2c-over-AUX command. In addition, if specified, it can also retry
 * when failures are detected. The retry logic would adjust AUX PHY
 * parameters on the fly.
 */
static int dp_aux_write_buf_retry(struct mdss_dp_drv_pdata *dp, u32 addr,
	char *buf, int len, int i2c, bool retry)
{
	struct edp_cmd	cmd;

	cmd.read = 0;
	cmd.i2c = i2c;
	cmd.addr = addr;
	cmd.datap = buf;
	cmd.len = len & 0x0ff;
	cmd.next = 0;

	if (retry)
		return dp_aux_rw_cmds_retry(dp, &cmd, DP_AUX_WRITE);
	else
		return dp_aux_write_cmds(dp, &cmd);
}

static int dp_aux_write_buf(struct mdss_dp_drv_pdata *dp, u32 addr,
	char *buf, int len, int i2c)
{
	return dp_aux_write_buf_retry(dp, addr, buf, len, i2c, true);
}

int dp_aux_write(void *dp, struct edp_cmd *cmd)
{
	int rc = dp_aux_write_cmds(dp, cmd);

	return rc < 0 ? -EINVAL : 0;
}

/**
 * dp_aux_read_buf_retry() - send a AUX read command
 * @dp: display port driver data
 * @addr: AUX address (in hex) to write the command to
 * @buf: the buffer containing the actual payload
 * @len: the length of the buffer @buf
 * @i2c: indicates if it is an i2c-over-aux transaction
 * @retry: specifies if retries should be attempted upon failures
 *
 * Send an AUX write command with the specified payload over the AUX
 * channel. This function can send both native AUX command or an
 * i2c-over-AUX command. In addition, if specified, it can also retry
 * when failures are detected. The retry logic would adjust AUX PHY
 * parameters on the fly.
 */
static int dp_aux_read_buf_retry(struct mdss_dp_drv_pdata *dp, u32 addr,
		int len, int i2c, bool retry)
{
	struct edp_cmd cmd = {0};

	cmd.read = 1;
	cmd.i2c = i2c;
	cmd.addr = addr;
	cmd.datap = NULL;
	cmd.len = len & 0x0ff;
	cmd.next = 0;

	if (retry)
		return dp_aux_rw_cmds_retry(dp, &cmd, DP_AUX_READ);
	else
		return dp_aux_read_cmds(dp, &cmd);
}

static int dp_aux_read_buf(struct mdss_dp_drv_pdata *dp, u32 addr,
				int len, int i2c)
{
	return dp_aux_read_buf_retry(dp, addr, len, i2c, true);
}

int dp_aux_read(void *dp, struct edp_cmd *cmds)
{
	int rc = dp_aux_read_cmds(dp, cmds);

	return rc  < 0 ? -EINVAL : 0;
}

/*
 * edid standard header bytes
 */
static u8 edid_hdr[8] = {0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};

static bool dp_edid_is_valid_header(u8 *buf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(edid_hdr); i++) {
		if (buf[i] != edid_hdr[i])
			return false;
	}

	return true;
}

int dp_edid_buf_error(char *buf, int len)
{
	char *bp;
	int i;
	char csum = 0;

	bp = buf;
	if (len < 128) {
		pr_err("Error: len=%x\n", len);
		return -EINVAL;
	}

	for (i = 0; i < 128; i++)
		csum += *bp++;

	if (csum != 0) {
		pr_err("Error: csum=%x\n", csum);
		return -EINVAL;
	}

	return 0;
}


void dp_extract_edid_manufacturer(struct edp_edid *edid, char *buf)
{
	char *bp;
	char data;

	bp = &buf[8];
	data = *bp & 0x7f;
	data >>= 2;
	edid->id_name[0] = 'A' + data - 1;
	data = *bp & 0x03;
	data <<= 3;
	bp++;
	data |= (*bp >> 5);
	edid->id_name[1] = 'A' + data - 1;
	data = *bp & 0x1f;
	edid->id_name[2] = 'A' + data - 1;
	edid->id_name[3] = 0;

	pr_debug("edid manufacturer = %s\n", edid->id_name);
}

void dp_extract_edid_product(struct edp_edid *edid, char *buf)
{
	char *bp;
	u32 data;

	bp = &buf[0x0a];
	data =  *bp;
	edid->id_product = *bp++;
	edid->id_product &= 0x0ff;
	data = *bp & 0x0ff;
	data <<= 8;
	edid->id_product |= data;

	pr_debug("edid product = 0x%x\n", edid->id_product);
};

void dp_extract_edid_version(struct edp_edid *edid, char *buf)
{
	edid->version = buf[0x12];
	edid->revision = buf[0x13];
	pr_debug("edid version = %d.%d\n", edid->version,
			edid->revision);
};

void dp_extract_edid_ext_block_cnt(struct edp_edid *edid, char *buf)
{
	edid->ext_block_cnt = buf[0x7e];
	pr_debug("edid extension = %d\n",
			edid->ext_block_cnt);
};

void dp_extract_edid_video_support(struct edp_edid *edid, char *buf)
{
	char *bp;

	bp = &buf[0x14];
	if (*bp & 0x80) {
		edid->video_intf = *bp & 0x0f;
		/* 6, 8, 10, 12, 14 and 16 bit per component */
		edid->color_depth = ((*bp & 0x70) >> 4); /* color bit depth */
		/* decrement to match with the test_bit_depth enum definition */
		edid->color_depth--;
		pr_debug("Digital Video intf=%d color_depth=%d\n",
			 edid->video_intf, edid->color_depth);
	} else {
		pr_debug("Analog video interface, set color depth to 8\n");
		edid->color_depth = DP_TEST_BIT_DEPTH_8;
	}
};

void dp_extract_edid_feature(struct edp_edid *edid, char *buf)
{
	char *bp;
	char data;

	bp = &buf[0x18];
	data = *bp;
	data &= 0xe0;
	data >>= 5;
	if (data == 0x01)
		edid->dpm = 1; /* display power management */

	if (edid->video_intf) {
		if (*bp & 0x80) {
			/* RGB 4:4:4, YcrCb 4:4:4 and YCrCb 4:2:2 */
			edid->color_format = *bp & 0x18;
			edid->color_format >>= 3;
		}
	}

	pr_debug("edid dpm=%d color_format=%d\n",
			edid->dpm, edid->color_format);
};

char mdss_dp_gen_link_clk(struct mdss_dp_drv_pdata *dp)
{
	const u32 encoding_factx10 = 8;
	const u32 ln_to_link_ratio = 10;
	u32 min_link_rate, reminder = 0;
	char calc_link_rate = 0;
	struct mdss_panel_info *pinfo = &dp->panel_data.panel_info;
	char lane_cnt = dp->dpcd.max_lane_count;

	pr_debug("clk_rate=%llu, bpp= %d, lane_cnt=%d\n",
	       pinfo->clk_rate, pinfo->bpp, lane_cnt);

	if (lane_cnt == 0) {
		pr_warn("Invalid max lane count\n");
		return 0;
	}

	/*
	 * The max pixel clock supported is 675Mhz. The
	 * current calculations below will make sure
	 * the min_link_rate is within 32 bit limits.
	 * Any changes in the section of code should
	 * consider this limitation.
	 */
	min_link_rate = (u32)div_u64(pinfo->clk_rate,
				(lane_cnt * encoding_factx10));
	min_link_rate /= ln_to_link_ratio;
	min_link_rate = (min_link_rate * pinfo->bpp);
	min_link_rate = (u32)div_u64_rem(min_link_rate * 10,
				DP_LINK_RATE_MULTIPLIER, &reminder);

	/*
	 * To avoid any fractional values,
	 * increment the min_link_rate
	 */
	if (reminder)
		min_link_rate += 1;
	pr_debug("min_link_rate = %d\n", min_link_rate);

	if (min_link_rate <= DP_LINK_RATE_162)
		calc_link_rate = DP_LINK_RATE_162;
	else if (min_link_rate <= DP_LINK_RATE_270)
		calc_link_rate = DP_LINK_RATE_270;
	else if (min_link_rate <= DP_LINK_RATE_540)
		calc_link_rate = DP_LINK_RATE_540;
	else {
		/* Cap the link rate to the max supported rate */
		pr_debug("link_rate = %d is unsupported\n", min_link_rate);
		calc_link_rate = DP_LINK_RATE_540;
	}

	pr_debug("calc_link_rate=0x%x, Max rate supported by sink=0x%x\n",
		dp->link_rate, dp->dpcd.max_link_rate);
	if (calc_link_rate > dp->dpcd.max_link_rate)
		calc_link_rate = dp->dpcd.max_link_rate;
	pr_debug("calc_link_rate = 0x%x\n", calc_link_rate);
	return calc_link_rate;
}

void dp_extract_edid_detailed_timing_description(struct edp_edid *edid,
		char *buf)
{
	char *bp;
	u32 data;
	struct display_timing_desc *dp;

	dp = &edid->timing[0];

	bp = &buf[0x36];
	dp->pclk = 0;
	dp->pclk = *bp++; /* byte 0x36 */
	dp->pclk |= (*bp++ << 8); /* byte 0x37 */

	dp->h_addressable = *bp++; /* byte 0x38 */

	if (dp->pclk == 0 && dp->h_addressable == 0)
		return;	/* Not detailed timing definition */

	dp->pclk *= 10000;

	dp->h_blank = *bp++;/* byte 0x39 */
	data = *bp & 0xf0; /* byte 0x3A */
	data  <<= 4;
	dp->h_addressable |= data;

	data = *bp++ & 0x0f;
	data <<= 8;
	dp->h_blank |= data;

	dp->v_addressable = *bp++; /* byte 0x3B */
	dp->v_blank = *bp++; /* byte 0x3C */
	data = *bp & 0xf0; /* byte 0x3D */
	data  <<= 4;
	dp->v_addressable |= data;

	data = *bp++ & 0x0f;
	data <<= 8;
	dp->v_blank |= data;

	dp->h_fporch = *bp++; /* byte 0x3E */
	dp->h_sync_pulse = *bp++; /* byte 0x3F */

	dp->v_fporch = *bp & 0x0f0; /* byte 0x40 */
	dp->v_fporch  >>= 4;
	dp->v_sync_pulse = *bp & 0x0f;

	bp++;
	data = *bp & 0xc0; /* byte 0x41 */
	data <<= 2;
	dp->h_fporch |= data;

	data = *bp & 0x30;
	data <<= 4;
	dp->h_sync_pulse |= data;

	data = *bp & 0x0c;
	data <<= 2;
	dp->v_fporch |= data;

	data = *bp & 0x03;
	data <<= 4;
	dp->v_sync_pulse |= data;

	bp++;
	dp->width_mm = *bp++; /* byte 0x42 */
	dp->height_mm = *bp++; /* byte 0x43 */
	data = *bp & 0x0f0; /* byte 0x44 */
	data <<= 4;
	dp->width_mm |= data;
	data = *bp & 0x0f;
	data <<= 8;
	dp->height_mm |= data;

	bp++;
	dp->h_border = *bp++; /* byte 0x45 */
	dp->v_border = *bp++; /* byte 0x46 */

	/* progressive or interlaved */
	dp->interlaced = *bp & 0x80; /* byte 0x47 */

	dp->stereo = *bp & 0x60;
	dp->stereo >>= 5;

	data = *bp & 0x1e; /* bit 4,3,2 1*/
	data >>= 1;
	dp->sync_type = data & 0x08;
	dp->sync_type >>= 3;	/* analog or digital */
	if (dp->sync_type) {
		dp->sync_separate = data & 0x04;
		dp->sync_separate >>= 2;
		if (dp->sync_separate) {
			if (data & 0x02)
				dp->vsync_pol = 1; /* positive */
			else
				dp->vsync_pol = 0;/* negative */

			if (data & 0x01)
				dp->hsync_pol = 1; /* positive */
			else
				dp->hsync_pol = 0; /* negative */
		}
	}

	pr_debug("pixel_clock = %d\n", dp->pclk);

	pr_debug("horizontal=%d, blank=%d, porch=%d, sync=%d\n",
			dp->h_addressable, dp->h_blank,
			dp->h_fporch, dp->h_sync_pulse);
	pr_debug("vertical=%d, blank=%d, porch=%d, vsync=%d\n",
			dp->v_addressable, dp->v_blank,
			dp->v_fporch, dp->v_sync_pulse);
	pr_debug("panel size in mm, width=%d height=%d\n",
			dp->width_mm, dp->height_mm);
	pr_debug("panel border horizontal=%d vertical=%d\n",
				dp->h_border, dp->v_border);
	pr_debug("flags: interlaced=%d stereo=%d sync_type=%d sync_sep=%d\n",
			dp->interlaced, dp->stereo,
			dp->sync_type, dp->sync_separate);
	pr_debug("polarity vsync=%d, hsync=%d\n",
			dp->vsync_pol, dp->hsync_pol);
}


/*
 * EDID structure can be found in VESA standard here:
 * http://read.pudn.com/downloads110/ebook/456020/E-EDID%20Standard.pdf
 *
 * following table contains default edid
 * static char edid_raw_data[128] = {
 * 0, 255, 255, 255, 255, 255, 255, 0,
 * 6, 175, 93, 48, 0, 0, 0, 0, 0, 22,
 * 1, 4,
 * 149, 26, 14, 120, 2,
 * 164, 21,158, 85, 78, 155, 38, 15, 80, 84,
 * 0, 0, 0,
 * 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
 * 29, 54, 128, 160, 112, 56, 30, 64, 48, 32, 142, 0, 0, 144, 16,0,0,24,
 * 19, 36, 128, 160, 112, 56, 30, 64, 48, 32, 142, 0, 0, 144, 16,0,0,24,
 * 0, 0, 0, 254, 0, 65, 85, 79, 10, 32, 32, 32, 32, 32, 32, 32, 32, 32,
 * 0, 0, 0, 254, 0, 66, 49, 49, 54, 72, 65, 78, 48, 51, 46, 48, 32, 10,
 * 0, 75 };
 */

static int dp_aux_chan_ready(struct mdss_dp_drv_pdata *ep)
{
	int cnt, ret = 0;
	char data = 0;

	for (cnt = 5; cnt; cnt--) {
		ret = dp_aux_write_buf(ep, EDID_START_ADDRESS, &data, 1, 1);
		pr_debug("ret = %d, aux_error = %s\n",
				ret, mdss_dp_get_aux_error(ep->aux_error_num));
		if (ret >= 0)
			break;

		if (ret == -ENODEV)
			return ret;

		msleep(100);
	}

	if (cnt <= 0) {
		pr_err("aux chan NOT ready\n");
		return -EIO;
	}

	return 0;
}

static void dp_aux_send_checksum(struct mdss_dp_drv_pdata *dp, u32 checksum)
{
	char data[4];

	data[0] = checksum;
	pr_debug("writing checksum %d\n", data[0]);
	dp_aux_write_buf(dp, 0x261, data, 1, 0);

	data[0] = TEST_EDID_CHECKSUM_WRITE;
	pr_debug("sending test response %s\n",
			mdss_dp_get_test_response(data[0]));
	dp_aux_write_buf(dp, 0x260, data, 1, 0);
}

int mdss_dp_aux_read_edid(struct mdss_dp_drv_pdata *dp,
	u8 *buf, int size, int blk_num)
{
	int max_size_bytes = 16;
	int rc, read_size;
	int ret = 0;
	u8 offset_lut[] = {0x0, 0x80};
	u8 offset;

	if (dp->test_data.test_requested == TEST_EDID_READ)
		max_size_bytes = 128;

	/*
	 * Calculate the offset of the desired EDID block to be read.
	 * For even blocks, offset starts at 0x0
	 * For odd blocks, offset starts at 0x80
	 */
	if (blk_num % 2)
		offset = offset_lut[1];
	else
		offset = offset_lut[0];

	do {
		struct edp_cmd cmd = {0};

		read_size = min(size, max_size_bytes);
		cmd.read = 1;
		cmd.addr = EDID_START_ADDRESS;
		cmd.len = read_size;
		cmd.out_buf = buf;
		cmd.i2c = 1;

		/* Write the offset first prior to reading the data */
		pr_debug("offset=0x%x, size=%d\n", offset, size);
		dp_aux_write_buf_retry(dp, EDID_START_ADDRESS, &offset, 1, 1,
			false);
		rc = dp_aux_read(dp, &cmd);
		if (rc < 0) {
			pr_err("aux read failed\n");
			return rc;
		}

		print_hex_dump(KERN_DEBUG, "DP:EDID: ", DUMP_PREFIX_NONE, 16, 1,
				buf, read_size, false);
		buf += read_size;
		offset += read_size;
		size -= read_size;
		ret += read_size;
	} while (size > 0);

	return ret;
}

int mdss_dp_edid_read(struct mdss_dp_drv_pdata *dp)
{
	int rlen, ret = 0;
	int edid_blk = 0, blk_num = 0, retries = 10;
	bool edid_parsing_done = false;
	u32 const segment_addr = 0x30;
	u32 checksum = 0;
	bool phy_aux_update_requested = false;
	bool ext_block_parsing_done = false;
	bool connected = false;

	ret = dp_aux_chan_ready(dp);
	if (ret) {
		pr_err("aux chan NOT ready\n");
		return ret;
	}

	memset(dp->edid_buf, 0, dp->edid_buf_size);

	/**
	 * Parse the test request vector to see whether there is a
	 * TEST_EDID_READ test request.
	 */
	dp_sink_parse_test_request(dp);

	while (retries) {
		u8 segment;
		u8 edid_buf[EDID_BLOCK_SIZE] = {0};

		mutex_lock(&dp->attention_lock);
		connected = dp->cable_connected;
		mutex_unlock(&dp->attention_lock);

		if (!connected) {
			pr_err("DP sink not connected\n");
			return -ENODEV;
		}

		/*
		 * Write the segment first.
		 * Segment = 0, for blocks 0 and 1
		 * Segment = 1, for blocks 2 and 3
		 * Segment = 2, for blocks 3 and 4
		 * and so on ...
		 */
		segment = blk_num >> 1;
		dp_aux_write_buf_retry(dp, segment_addr, &segment, 1, 1, false);

		rlen = mdss_dp_aux_read_edid(dp, edid_buf, EDID_BLOCK_SIZE,
			blk_num);
		if (rlen != EDID_BLOCK_SIZE) {
			pr_err("Read failed. rlen=%s\n",
				mdss_dp_get_aux_error(rlen));
			mdss_dp_phy_aux_update_config(dp, PHY_AUX_CFG1);
			phy_aux_update_requested = true;
			retries--;
			continue;
		}
		pr_debug("blk_num=%d, rlen=%d\n", blk_num, rlen);
		print_hex_dump(KERN_DEBUG, "DP:EDID: ", DUMP_PREFIX_NONE, 16, 1,
				edid_buf, EDID_BLOCK_SIZE, false);
		if (dp_edid_is_valid_header(edid_buf)) {
			ret = dp_edid_buf_error(edid_buf, rlen);
			if (ret) {
				pr_err("corrupt edid block detected\n");
				mdss_dp_phy_aux_update_config(dp, PHY_AUX_CFG1);
				phy_aux_update_requested = true;
				retries--;
				continue;
			}

			if (edid_parsing_done) {
				pr_debug("block 0 parsed already\n");
				blk_num++;
				retries--;
				continue;
			}

			dp_extract_edid_manufacturer(&dp->edid, edid_buf);
			dp_extract_edid_product(&dp->edid, edid_buf);
			dp_extract_edid_version(&dp->edid, edid_buf);
			dp_extract_edid_ext_block_cnt(&dp->edid, edid_buf);
			dp_extract_edid_video_support(&dp->edid, edid_buf);
			dp_extract_edid_feature(&dp->edid, edid_buf);
			dp_extract_edid_detailed_timing_description(&dp->edid,
				edid_buf);

			edid_parsing_done = true;
		} else if (!edid_parsing_done) {
			pr_debug("Invalid edid block 0 header\n");
			/* Retry block 0 with adjusted phy aux settings */
			mdss_dp_phy_aux_update_config(dp, PHY_AUX_CFG1);
			phy_aux_update_requested = true;
			retries--;
			continue;
		} else {
			edid_blk++;
			blk_num++;
		}

		memcpy(dp->edid_buf + (edid_blk * EDID_BLOCK_SIZE),
			edid_buf, EDID_BLOCK_SIZE);

		checksum = edid_buf[rlen - 1];

		/* break if no more extension blocks present */
		if (edid_blk >= dp->edid.ext_block_cnt) {
			ext_block_parsing_done = true;
			break;
		}
	}

	if (dp->test_data.test_requested == TEST_EDID_READ) {
		pr_debug("sending checksum %d\n", checksum);
		dp_aux_send_checksum(dp, checksum);
		dp->test_data = (const struct dpcd_test_request){ 0 };
	}

	/*
	 * Trigger the reading of DPCD if there was a change in the AUX
	 * configuration caused by a failure while reading the EDID.
	 * This is required to ensure the integrity and validity
	 * of the sink capabilities read that will subsequently be used
	 * to establish the mainlink.
	 */
	if (edid_parsing_done && ext_block_parsing_done
			&& phy_aux_update_requested) {
		dp->dpcd_read_required = true;
	}

	return ret;
}

int mdss_dp_dpcd_cap_read(struct mdss_dp_drv_pdata *ep)
{
	int const len = 16; /* read 16 bytes */
	char *bp;
	char data;
	struct dpcd_cap *cap;
	struct edp_buf *rp;
	int rlen;
	int i;

	cap = &ep->dpcd;
	memset(cap, 0, sizeof(*cap));

	rlen = dp_aux_read_buf(ep, 0, len, 0);
	if (rlen <= 0) {
		pr_err("edp aux read failed\n");
		return rlen;
	}

	if (rlen != len) {
		pr_debug("Read size expected(%d) bytes, actual(%d) bytes\n",
			len, rlen);
		return -EINVAL;
	}

	rp = &ep->rxp;
	bp = rp->data;

	data = *bp++; /* byte 0 */
	cap->major = (data >> 4) & 0x0f;
	cap->minor = data & 0x0f;
	pr_debug("version: %d.%d\n", cap->major, cap->minor);

	data = *bp++; /* byte 1 */
	/* 162, 270 and 540 MB, symbol rate, NOT bit rate */
	cap->max_link_rate = data;
	pr_debug("link_rate=%d\n", cap->max_link_rate);

	data = *bp++; /* byte 2 */
	if (data & BIT(7))
		cap->enhanced_frame++;

	if (data & 0x40) {
		cap->flags |=  DPCD_TPS3;
		pr_debug("pattern 3 supported\n");
	} else {
		pr_debug("pattern 3 not supported\n");
	}

	data &= 0x0f;
	cap->max_lane_count = data;
	pr_debug("lane_count=%d\n", cap->max_lane_count);

	data = *bp++; /* byte 3 */
	if (data & BIT(0)) {
		cap->flags |= DPCD_MAX_DOWNSPREAD_0_5;
		pr_debug("max_downspread\n");
	}

	if (data & BIT(6)) {
		cap->flags |= DPCD_NO_AUX_HANDSHAKE;
		pr_debug("NO Link Training\n");
	}

	data = *bp++; /* byte 4 */
	cap->num_rx_port = (data & BIT(0)) + 1;
	pr_debug("rx_ports=%d\n", cap->num_rx_port);

	data = *bp++; /* Byte 5: DOWN_STREAM_PORT_PRESENT */
	cap->downstream_port.dsp_present = data & BIT(0);
	cap->downstream_port.dsp_type = (data & 0x6) >> 1;
	cap->downstream_port.format_conversion = data & BIT(3);
	cap->downstream_port.detailed_cap_info_available = data & BIT(4);
	pr_debug("dsp_present = %d, dsp_type = %d\n",
			cap->downstream_port.dsp_present,
			cap->downstream_port.dsp_type);
	pr_debug("format_conversion = %d, detailed_cap_info_available = %d\n",
			cap->downstream_port.format_conversion,
			cap->downstream_port.detailed_cap_info_available);

	bp += 1;	/* Skip Byte 6 */

	data = *bp++; /* Byte 7: DOWN_STREAM_PORT_COUNT */
	cap->downstream_port.dsp_count = data & 0x7;
	if (cap->downstream_port.dsp_count > DP_MAX_DS_PORT_COUNT) {
		pr_debug("DS port count %d greater that max (%d) supported\n",
			cap->downstream_port.dsp_count, DP_MAX_DS_PORT_COUNT);
		cap->downstream_port.dsp_count = DP_MAX_DS_PORT_COUNT;
	}
	cap->downstream_port.msa_timing_par_ignored = data & BIT(6);
	cap->downstream_port.oui_support = data & BIT(7);
	pr_debug("dsp_count = %d, msa_timing_par_ignored = %d\n",
			cap->downstream_port.dsp_count,
			cap->downstream_port.msa_timing_par_ignored);
	pr_debug("oui_support = %d\n", cap->downstream_port.oui_support);

	for (i = 0; i < DP_MAX_DS_PORT_COUNT; i++) {
		data = *bp++; /* byte 8 + i*2 */
		pr_debug("parsing capabilities for DS port %d\n", i);
		if (data & BIT(1)) {
			if (i == 0)
				cap->flags |= DPCD_PORT_0_EDID_PRESENTED;
			else
				cap->flags |= DPCD_PORT_1_EDID_PRESENTED;
			pr_debug("local edid present\n");
		} else {
			pr_debug("local edid absent\n");
		}

		data = *bp++; /* byte 9 + i*2 */
		cap->rx_port_buf_size[i] = (data + 1) * 32;
		pr_debug("lane_buf_size=%d\n", cap->rx_port_buf_size[i]);
	}

	data = *bp++;	/* byte 12 */
	cap->i2c_speed_ctrl = data;
	if (cap->i2c_speed_ctrl > 0)
		pr_debug("i2c_rate=%d\n", cap->i2c_speed_ctrl);

	data = *bp++;	/* byte 13 */
	cap->scrambler_reset = data & BIT(0);
	pr_debug("scrambler_reset=%d\n",
					cap->scrambler_reset);

	if (data & BIT(1))
		cap->enhanced_frame++;

	pr_debug("enhanced_framing=%d\n",
					cap->enhanced_frame);

	data = *bp++; /* byte 14 */
	if (data == 0)
		cap->training_read_interval = 4000; /* us */
	else
		cap->training_read_interval = 4000 * data; /* us */
	pr_debug("training_interval=%d\n",
			 cap->training_read_interval);

	dp_sink_parse_sink_count(ep);

	return 0;
}

int mdss_dp_aux_link_status_read(struct mdss_dp_drv_pdata *ep, int len)
{
	char *bp;
	char data;
	struct dpcd_link_status *sp;
	struct edp_buf *rp;
	int rlen;

	pr_debug("len=%d\n", len);
	/* skip byte 0x200 and 0x201 */
	rlen = dp_aux_read_buf(ep, 0x202, len, 0);
	if (rlen < len) {
		pr_err("edp aux read failed\n");
		return rlen;
	}
	rp = &ep->rxp;
	bp = rp->data;
	sp = &ep->link_status;

	data = *bp++; /* byte 0x202 */
	sp->lane_01_status = data; /* lane 0, 1 */

	data = *bp++; /* byte 0x203 */
	sp->lane_23_status = data; /* lane 2, 3 */

	data = *bp++; /* byte 0x204 */
	sp->interlane_align_done = (data & BIT(0));
	sp->downstream_port_status_changed = (data & BIT(6));
	sp->link_status_updated = (data & BIT(7));

	data = *bp++; /* byte 0x205 */
	sp->port_0_in_sync = (data & BIT(0));
	sp->port_1_in_sync = (data & BIT(1));

	data = *bp++; /* byte 0x206 */
	sp->req_voltage_swing[0] = data & 0x03;
	data >>= 2;
	sp->req_pre_emphasis[0] = data & 0x03;
	data >>= 2;
	sp->req_voltage_swing[1] = data & 0x03;
	data >>= 2;
	sp->req_pre_emphasis[1] = data & 0x03;

	data = *bp++; /* byte 0x207 */
	sp->req_voltage_swing[2] = data & 0x03;
	data >>= 2;
	sp->req_pre_emphasis[2] = data & 0x03;
	data >>= 2;
	sp->req_voltage_swing[3] = data & 0x03;
	data >>= 2;
	sp->req_pre_emphasis[3] = data & 0x03;

	return len;
}

/*
 * mdss_dp_aux_send_psm_request() - sends a power save mode messge to sink
 * @dp: Display Port Driver data
 */
int mdss_dp_aux_send_psm_request(struct mdss_dp_drv_pdata *dp, bool enable)
{
	u8 psm_request[4];
	int rc = 0;

	psm_request[0] = enable ? 2 : 1;

	pr_debug("sending psm %s request\n", enable ? "entry" : "exit");
	if (enable) {
		dp_aux_write_buf(dp, 0x600, psm_request, 1, 0);
	} else {
		ktime_t timeout = ktime_add_ms(ktime_get(), 20);

		/*
		 * It could take up to 1ms (20 ms of embedded sinks) till
		 * the sink is ready to reply to this AUX transaction. It is
		 * expected that the source keep retrying periodically during
		 * this time.
		 */
		for (;;) {
			rc = dp_aux_write_buf(dp, 0x600, psm_request, 1, 0);
			if ((rc >= 0) ||
				(ktime_compare(ktime_get(), timeout) > 0))
				break;
			usleep_range(100, 120);
		}

		/*
		 * if the aux transmission succeeded, then the function would
		 * return the number of bytes transmitted.
		 */
		if (rc > 0)
			rc = 0;
	}

	if (!rc)
		dp->psm_enabled = enable;

	return rc;
}

/**
 * mdss_dp_aux_send_test_response() - sends a test response to the sink
 * @dp: Display Port Driver data
 */
void mdss_dp_aux_send_test_response(struct mdss_dp_drv_pdata *dp)
{
	char test_response[4];

	test_response[0] = dp->test_data.response;

	pr_debug("sending test response %s\n",
			mdss_dp_get_test_response(test_response[0]));
	dp_aux_write_buf(dp, 0x260, test_response, 1, 0);
}

/**
 * mdss_dp_aux_is_link_rate_valid() - validates the link rate
 * @lane_rate: link rate requested by the sink
 *
 * Returns true if the requested link rate is supported.
 */
bool mdss_dp_aux_is_link_rate_valid(u32 link_rate)
{
	return (link_rate == DP_LINK_RATE_162) ||
		(link_rate == DP_LINK_RATE_270) ||
		(link_rate == DP_LINK_RATE_540);
}

/**
 * mdss_dp_aux_is_lane_count_valid() - validates the lane count
 * @lane_count: lane count requested by the sink
 *
 * Returns true if the requested lane count is supported.
 */
bool mdss_dp_aux_is_lane_count_valid(u32 lane_count)
{
	return (lane_count == DP_LANE_COUNT_1) ||
		(lane_count == DP_LANE_COUNT_2) ||
		(lane_count == DP_LANE_COUNT_4);
}

int mdss_dp_aux_parse_vx_px(struct mdss_dp_drv_pdata *ep)
{
	char *bp;
	char data;
	struct edp_buf *rp;
	int rlen;
	int const param_len = 0x1;
	int const addr1 = 0x206;
	int const addr2 = 0x207;
	int ret = 0;
	u32 v0, p0, v1, p1, v2, p2, v3, p3;

	pr_info("Parsing DPCP for updated voltage and pre-emphasis levels\n");

	rlen = dp_aux_read_buf(ep, addr1, param_len, 0);
	if (rlen < param_len) {
		pr_err("failed reading lanes 0/1\n");
		ret = -EINVAL;
		goto end;
	}

	rp = &ep->rxp;
	bp = rp->data;
	data = *bp++;

	pr_info("lanes 0/1 (Byte 0x206): 0x%x\n", data);

	v0 = data & 0x3;
	data = data >> 2;
	p0 = data & 0x3;
	data = data >> 2;

	v1 = data & 0x3;
	data = data >> 2;
	p1 = data & 0x3;
	data = data >> 2;

	rlen = dp_aux_read_buf(ep, addr2, param_len, 0);
	if (rlen < param_len) {
		pr_err("failed reading lanes 2/3\n");
		ret = -EINVAL;
		goto end;
	}

	rp = &ep->rxp;
	bp = rp->data;
	data = *bp++;

	pr_info("lanes 2/3 (Byte 0x207): 0x%x\n", data);

	v2 = data & 0x3;
	data = data >> 2;
	p2 = data & 0x3;
	data = data >> 2;

	v3 = data & 0x3;
	data = data >> 2;
	p3 = data & 0x3;
	data = data >> 2;

	pr_info("vx: 0=%d, 1=%d, 2=%d, 3=%d\n", v0, v1, v2, v3);
	pr_info("px: 0=%d, 1=%d, 2=%d, 3=%d\n", p0, p1, p2, p3);

	/**
	 * Update the voltage and pre-emphasis levels as per DPCD request
	 * vector.
	 */
	pr_info("Current: v_level = 0x%x, p_level = 0x%x\n",
			ep->v_level, ep->p_level);
	pr_info("Requested: v_level = 0x%x, p_level = 0x%x\n", v0, p0);
	ep->v_level = v0;
	ep->p_level = p0;

	pr_info("Success\n");
end:
	return ret;
}

/**
 * dp_parse_link_training_params() - parses link training parameters from DPCD
 * @ep: Display Port Driver data
 *
 * Returns 0 if it successfully parses the link rate (Byte 0x219) and lane
 * count (Byte 0x220), and if these values parse are valid.
 */
static int dp_parse_link_training_params(struct mdss_dp_drv_pdata *ep)
{
	int ret = 0;
	char *bp;
	char data;
	struct edp_buf *rp;
	int rlen;
	int const test_parameter_len = 0x1;
	int const test_link_rate_addr = 0x219;
	int const test_lane_count_addr = 0x220;

	/* Read the requested link rate (Byte 0x219). */
	rlen = dp_aux_read_buf(ep, test_link_rate_addr,
			test_parameter_len, 0);
	if (rlen < test_parameter_len) {
		pr_err("failed to read link rate\n");
		ret = -EINVAL;
		goto exit;
	}
	rp = &ep->rxp;
	bp = rp->data;
	data = *bp++;

	if (!mdss_dp_aux_is_link_rate_valid(data)) {
		pr_err("invalid link rate = 0x%x\n", data);
		ret = -EINVAL;
		goto exit;
	}

	ep->test_data.test_link_rate = data;
	pr_debug("link rate = 0x%x\n", ep->test_data.test_link_rate);

	/* Read the requested lane count (Byte 0x220). */
	rlen = dp_aux_read_buf(ep, test_lane_count_addr,
			test_parameter_len, 0);
	if (rlen < test_parameter_len) {
		pr_err("failed to read lane count\n");
		ret = -EINVAL;
		goto exit;
	}
	rp = &ep->rxp;
	bp = rp->data;
	data = *bp++;
	data &= 0x1F;

	if (!mdss_dp_aux_is_lane_count_valid(data)) {
		pr_err("invalid lane count = 0x%x\n", data);
		ret = -EINVAL;
		goto exit;
	}

	ep->test_data.test_lane_count = data;
	pr_debug("lane count = 0x%x\n", ep->test_data.test_lane_count);

exit:
	return ret;
}

/**
 * dp_sink_parse_sink_count() - parses the sink count
 * @ep: Display Port Driver data
 *
 * Parses the DPCD to check if there is an update to the sink count
 * (Byte 0x200), and whether all the sink devices connected have Content
 * Protection enabled.
 */
static void dp_sink_parse_sink_count(struct mdss_dp_drv_pdata *ep)
{
	char *bp;
	char data;
	struct edp_buf *rp;
	int rlen;
	int const param_len = 0x1;
	int const sink_count_addr = 0x200;

	ep->prev_sink_count = ep->sink_count;

	rlen = dp_aux_read_buf(ep, sink_count_addr, param_len, 0);
	if (rlen < param_len) {
		pr_err("failed to read sink count\n");
		return;
	}
	rp = &ep->rxp;
	bp = rp->data;
	data = *bp++;

	/* BIT 7, BIT 5:0 */
	ep->sink_count.count = (data & BIT(7)) >> 1 | (data & 0x3F);
	/* BIT 6*/
	ep->sink_count.cp_ready = data & BIT(6);

	pr_debug("sink_count = 0x%x, cp_ready = 0x%x\n",
			ep->sink_count.count, ep->sink_count.cp_ready);
}

static int dp_get_test_period(struct mdss_dp_drv_pdata *ep, int const addr)
{
	int ret = 0;
	char *bp;
	char data;
	struct edp_buf *rp;
	int rlen;
	int const test_parameter_len = 0x1;
	int const max_audio_period = 0xA;

	/* TEST_AUDIO_PERIOD_CH_XX */
	rlen = dp_aux_read_buf(ep, addr, test_parameter_len, 0);
	if (rlen < test_parameter_len) {
		pr_err("failed to read test_audio_period (0x%x)\n", addr);
		ret = -EINVAL;
		goto exit;
	}
	rp = &ep->rxp;
	bp = rp->data;
	data = *bp++;

	/* Period - Bits 3:0 */
	data = data & 0xF;
	if ((int)data > max_audio_period) {
		pr_err("invalid test_audio_period_ch_1 = 0x%x\n", data);
		ret = -EINVAL;
		goto exit;
	}

	ret = data;

exit:
	return ret;
}

static int dp_parse_audio_channel_test_period(struct mdss_dp_drv_pdata *ep)
{
	int ret = 0;
	int const test_audio_period_ch_1_addr = 0x273;
	int const test_audio_period_ch_2_addr = 0x274;
	int const test_audio_period_ch_3_addr = 0x275;
	int const test_audio_period_ch_4_addr = 0x276;
	int const test_audio_period_ch_5_addr = 0x277;
	int const test_audio_period_ch_6_addr = 0x278;
	int const test_audio_period_ch_7_addr = 0x279;
	int const test_audio_period_ch_8_addr = 0x27A;

	/* TEST_AUDIO_PERIOD_CH_1 (Byte 0x273) */
	ret = dp_get_test_period(ep, test_audio_period_ch_1_addr);
	if (ret == -EINVAL)
		goto exit;

	ep->test_data.test_audio_period_ch_1 = ret;
	pr_debug("test_audio_period_ch_1 = 0x%x\n", ret);

	/* TEST_AUDIO_PERIOD_CH_2 (Byte 0x274) */
	ret = dp_get_test_period(ep, test_audio_period_ch_2_addr);
	if (ret == -EINVAL)
		goto exit;

	ep->test_data.test_audio_period_ch_2 = ret;
	pr_debug("test_audio_period_ch_2 = 0x%x\n", ret);

	/* TEST_AUDIO_PERIOD_CH_3 (Byte 0x275) */
	ret = dp_get_test_period(ep, test_audio_period_ch_3_addr);
	if (ret == -EINVAL)
		goto exit;

	ep->test_data.test_audio_period_ch_3 = ret;
	pr_debug("test_audio_period_ch_3 = 0x%x\n", ret);

	/* TEST_AUDIO_PERIOD_CH_4 (Byte 0x276) */
	ret = dp_get_test_period(ep, test_audio_period_ch_4_addr);
	if (ret == -EINVAL)
		goto exit;

	ep->test_data.test_audio_period_ch_4 = ret;
	pr_debug("test_audio_period_ch_4 = 0x%x\n", ret);

	/* TEST_AUDIO_PERIOD_CH_5 (Byte 0x277) */
	ret = dp_get_test_period(ep, test_audio_period_ch_5_addr);
	if (ret == -EINVAL)
		goto exit;

	ep->test_data.test_audio_period_ch_5 = ret;
	pr_debug("test_audio_period_ch_5 = 0x%x\n", ret);

	/* TEST_AUDIO_PERIOD_CH_6 (Byte 0x278) */
	ret = dp_get_test_period(ep, test_audio_period_ch_6_addr);
	if (ret == -EINVAL)
		goto exit;

	ep->test_data.test_audio_period_ch_6 = ret;
	pr_debug("test_audio_period_ch_6 = 0x%x\n", ret);

	/* TEST_AUDIO_PERIOD_CH_7 (Byte 0x279) */
	ret = dp_get_test_period(ep, test_audio_period_ch_7_addr);
	if (ret == -EINVAL)
		goto exit;

	ep->test_data.test_audio_period_ch_7 = ret;
	pr_debug("test_audio_period_ch_7 = 0x%x\n", ret);

	/* TEST_AUDIO_PERIOD_CH_8 (Byte 0x27A) */
	ret = dp_get_test_period(ep, test_audio_period_ch_8_addr);
	if (ret == -EINVAL)
		goto exit;

	ep->test_data.test_audio_period_ch_8 = ret;
	pr_debug("test_audio_period_ch_8 = 0x%x\n", ret);


exit:
	return ret;
}

static int dp_parse_audio_pattern_type(struct mdss_dp_drv_pdata *ep)
{
	int ret = 0;
	char *bp;
	char data;
	struct edp_buf *rp;
	int rlen;
	int const test_parameter_len = 0x1;
	int const test_audio_pattern_type_addr = 0x272;
	int const max_audio_pattern_type = 0x1;

	/* Read the requested audio pattern type (Byte 0x272). */
	rlen = dp_aux_read_buf(ep, test_audio_pattern_type_addr,
			test_parameter_len, 0);
	if (rlen < test_parameter_len) {
		pr_err("failed to read test audio mode data\n");
		ret = -EINVAL;
		goto exit;
	}
	rp = &ep->rxp;
	bp = rp->data;
	data = *bp++;

	/* Audio Pattern Type - Bits 7:0 */
	if ((int)data > max_audio_pattern_type) {
		pr_err("invalid audio pattern type = 0x%x\n", data);
		ret = -EINVAL;
		goto exit;
	}

	ep->test_data.test_audio_pattern_type = data;
	pr_debug("audio pattern type = %s\n",
			mdss_dp_get_audio_test_pattern(data));

exit:
	return ret;
}

static int dp_parse_audio_mode(struct mdss_dp_drv_pdata *ep)
{
	int ret = 0;
	char *bp;
	char data;
	struct edp_buf *rp;
	int rlen;
	int const test_parameter_len = 0x1;
	int const test_audio_mode_addr = 0x271;
	int const max_audio_sampling_rate = 0x6;
	int const max_audio_channel_count = 0x8;
	int sampling_rate = 0x0;
	int channel_count = 0x0;

	/* Read the requested audio mode (Byte 0x271). */
	rlen = dp_aux_read_buf(ep, test_audio_mode_addr,
			test_parameter_len, 0);
	if (rlen < test_parameter_len) {
		pr_err("failed to read test audio mode data\n");
		ret = -EINVAL;
		goto exit;
	}
	rp = &ep->rxp;
	bp = rp->data;
	data = *bp++;

	/* Sampling Rate - Bits 3:0 */
	sampling_rate = data & 0xF;
	if (sampling_rate > max_audio_sampling_rate) {
		pr_err("sampling rate (0x%x) greater than max (0x%x)\n",
				sampling_rate, max_audio_sampling_rate);
		ret = -EINVAL;
		goto exit;
	}

	/* Channel Count - Bits 7:4 */
	channel_count = ((data & 0xF0) >> 4) + 1;
	if (channel_count > max_audio_channel_count) {
		pr_err("channel_count (0x%x) greater than max (0x%x)\n",
				channel_count, max_audio_channel_count);
		ret = -EINVAL;
		goto exit;
	}

	ep->test_data.test_audio_sampling_rate = sampling_rate;
	ep->test_data.test_audio_channel_count = channel_count;
	pr_debug("sampling_rate = %s, channel_count = 0x%x\n",
		mdss_dp_get_audio_sample_rate(sampling_rate), channel_count);

exit:
	return ret;
}

/**
 * dp_parse_audio_pattern_params() - parses audio pattern parameters from DPCD
 * @ep: Display Port Driver data
 *
 * Returns 0 if it successfully parses the audio test pattern parameters.
 */
static int dp_parse_audio_pattern_params(struct mdss_dp_drv_pdata *ep)
{
	int ret = 0;

	ret = dp_parse_audio_mode(ep);
	if (ret)
		goto exit;

	ret = dp_parse_audio_pattern_type(ep);
	if (ret)
		goto exit;

	ret = dp_parse_audio_channel_test_period(ep);

exit:
	return ret;
}
/**
 * dp_parse_phy_test_params() - parses the phy test parameters
 * @ep: Display Port Driver data
 *
 * Parses the DPCD (Byte 0x248) for the DP PHY test pattern that is being
 * requested.
 */
static int dp_parse_phy_test_params(struct mdss_dp_drv_pdata *ep)
{
	char *bp;
	char data;
	struct edp_buf *rp;
	int rlen;
	int const param_len = 0x1;
	int const phy_test_pattern_addr = 0x248;
	int ret = 0;

	rlen = dp_aux_read_buf(ep, phy_test_pattern_addr, param_len, 0);
	if (rlen < param_len) {
		pr_err("failed to read phy test pattern\n");
		ret = -EINVAL;
		goto end;
	}

	rp = &ep->rxp;
	bp = rp->data;
	data = *bp++;

	ep->test_data.phy_test_pattern_sel = data;

	pr_debug("phy_test_pattern_sel = %s\n",
			mdss_dp_get_phy_test_pattern(data));

	if (!mdss_dp_is_phy_test_pattern_supported(data))
		ret = -EINVAL;

end:
	return ret;
}

static int dp_parse_test_timing_params1(struct mdss_dp_drv_pdata *ep,
	int const addr, int const len, u32 *val)
{
	char *bp;
	struct edp_buf *rp;
	int rlen;

	if (len < 2)
		return -EINVAL;

	/* Read the requested video test pattern (Byte 0x221). */
	rlen = dp_aux_read_buf(ep, addr, len, 0);
	if (rlen < len) {
		pr_err("failed to read 0x%x\n", addr);
		return -EINVAL;
	}
	rp = &ep->rxp;
	bp = rp->data;

	*val = bp[1] | (bp[0] << 8);

	return 0;
}

static int dp_parse_test_timing_params2(struct mdss_dp_drv_pdata *ep,
	int const addr, int const len, u32 *val1, u32 *val2)
{
	char *bp;
	struct edp_buf *rp;
	int rlen;

	if (len < 2)
		return -EINVAL;

	/* Read the requested video test pattern (Byte 0x221). */
	rlen = dp_aux_read_buf(ep, addr, len, 0);
	if (rlen < len) {
		pr_err("failed to read 0x%x\n", addr);
		return -EINVAL;
	}
	rp = &ep->rxp;
	bp = rp->data;

	*val1 = (bp[0] & BIT(7)) >> 7;
	*val2 = bp[1] | ((bp[0] & 0x7F) << 8);

	return 0;
}

static int dp_parse_test_timing_params3(struct mdss_dp_drv_pdata *ep,
	int const addr, u32 *val)
{
	char *bp;
	struct edp_buf *rp;
	int rlen;

	/* Read the requested video test pattern (Byte 0x221). */
	rlen = dp_aux_read_buf(ep, addr, 1, 0);
	if (rlen < 1) {
		pr_err("failed to read 0x%x\n", addr);
		return -EINVAL;
	}
	rp = &ep->rxp;
	bp = rp->data;
	*val = bp[0];

	return 0;
}

/**
 * dp_parse_video_pattern_params() - parses video pattern parameters from DPCD
 * @ep: Display Port Driver data
 *
 * Returns 0 if it successfully parses the video test pattern and the test
 * bit depth requested by the sink and, and if the values parsed are valid.
 */
static int dp_parse_video_pattern_params(struct mdss_dp_drv_pdata *ep)
{
	int ret = 0;
	char *bp;
	char data;
	struct edp_buf *rp;
	int rlen;
	u32 dyn_range;
	int const test_parameter_len = 0x1;
	int const test_video_pattern_addr = 0x221;
	int const test_misc_addr = 0x232;

	/* Read the requested video test pattern (Byte 0x221). */
	rlen = dp_aux_read_buf(ep, test_video_pattern_addr,
			test_parameter_len, 0);
	if (rlen < test_parameter_len) {
		pr_err("failed to read test video pattern\n");
		ret = -EINVAL;
		goto exit;
	}
	rp = &ep->rxp;
	bp = rp->data;
	data = *bp++;

	if (!mdss_dp_is_test_video_pattern_valid(data)) {
		pr_err("invalid test video pattern = 0x%x\n", data);
		ret = -EINVAL;
		goto exit;
	}

	ep->test_data.test_video_pattern = data;
	pr_debug("test video pattern = 0x%x (%s)\n",
		ep->test_data.test_video_pattern,
		mdss_dp_test_video_pattern_to_string(
			ep->test_data.test_video_pattern));

	/* Read the requested color bit depth and dynamic range (Byte 0x232) */
	rlen = dp_aux_read_buf(ep, test_misc_addr, test_parameter_len, 0);
	if (rlen < test_parameter_len) {
		pr_err("failed to read test bit depth\n");
		ret = -EINVAL;
		goto exit;
	}
	rp = &ep->rxp;
	bp = rp->data;
	data = *bp++;

	/* Dynamic Range */
	dyn_range = (data & BIT(3)) >> 3;
	if (!mdss_dp_is_dynamic_range_valid(dyn_range)) {
		pr_err("invalid test dynamic range = 0x%x\n", dyn_range);
		ret = -EINVAL;
		goto exit;
	}
	ep->test_data.test_dyn_range = dyn_range;
	pr_debug("test dynamic range = 0x%x (%s)\n",
		ep->test_data.test_dyn_range,
		mdss_dp_dynamic_range_to_string(ep->test_data.test_dyn_range));

	/* Color bit depth */
	data &= (BIT(5) | BIT(6) | BIT(7));
	data >>= 5;
	if (!mdss_dp_is_test_bit_depth_valid(data)) {
		pr_err("invalid test bit depth = 0x%x\n", data);
		ret = -EINVAL;
		goto exit;
	}

	ep->test_data.test_bit_depth = data;
	pr_debug("test bit depth = 0x%x (%s)\n",
		ep->test_data.test_bit_depth,
		mdss_dp_test_bit_depth_to_string(ep->test_data.test_bit_depth));

	/* resolution timing params */
	ret = dp_parse_test_timing_params1(ep, 0x222, 2,
			&ep->test_data.test_h_total);
	if (ret) {
		pr_err("failed to parse test_h_total (0x222)\n");
		goto exit;
	}
	pr_debug("TEST_H_TOTAL = %d\n", ep->test_data.test_h_total);

	ret = dp_parse_test_timing_params1(ep, 0x224, 2,
			&ep->test_data.test_v_total);
	if (ret) {
		pr_err("failed to parse test_v_total (0x224)\n");
		goto exit;
	}
	pr_debug("TEST_V_TOTAL = %d\n", ep->test_data.test_v_total);

	ret = dp_parse_test_timing_params1(ep, 0x226, 2,
			&ep->test_data.test_h_start);
	if (ret) {
		pr_err("failed to parse test_h_start (0x226)\n");
		goto exit;
	}
	pr_debug("TEST_H_START = %d\n", ep->test_data.test_h_start);

	ret = dp_parse_test_timing_params1(ep, 0x228, 2,
			&ep->test_data.test_v_start);
	if (ret) {
		pr_err("failed to parse test_v_start (0x228)\n");
		goto exit;
	}
	pr_debug("TEST_V_START = %d\n", ep->test_data.test_v_start);

	ret = dp_parse_test_timing_params2(ep, 0x22A, 2,
			&ep->test_data.test_hsync_pol,
			&ep->test_data.test_hsync_width);
	if (ret) {
		pr_err("failed to parse (0x22A)\n");
		goto exit;
	}
	pr_debug("TEST_HSYNC_POL = %d\n", ep->test_data.test_hsync_pol);
	pr_debug("TEST_HSYNC_WIDTH = %d\n", ep->test_data.test_hsync_width);

	ret = dp_parse_test_timing_params2(ep, 0x22C, 2,
			&ep->test_data.test_vsync_pol,
			&ep->test_data.test_vsync_width);
	if (ret) {
		pr_err("failed to parse (0x22C)\n");
		goto exit;
	}
	pr_debug("TEST_VSYNC_POL = %d\n", ep->test_data.test_vsync_pol);
	pr_debug("TEST_VSYNC_WIDTH = %d\n", ep->test_data.test_vsync_width);

	ret = dp_parse_test_timing_params1(ep, 0x22E, 2,
			&ep->test_data.test_h_width);
	if (ret) {
		pr_err("failed to parse test_h_width (0x22E)\n");
		goto exit;
	}
	pr_debug("TEST_H_WIDTH = %d\n", ep->test_data.test_h_width);

	ret = dp_parse_test_timing_params1(ep, 0x230, 2,
			&ep->test_data.test_v_height);
	if (ret) {
		pr_err("failed to parse test_v_height (0x230)\n");
		goto exit;
	}
	pr_debug("TEST_V_HEIGHT = %d\n", ep->test_data.test_v_height);

	ret = dp_parse_test_timing_params3(ep, 0x233, &ep->test_data.test_rr_d);
	ep->test_data.test_rr_d &= BIT(0);
	if (ret) {
		pr_err("failed to parse test_rr_d (0x233)\n");
		goto exit;
	}
	pr_debug("TEST_REFRESH_DENOMINATOR = %d\n", ep->test_data.test_rr_d);

	ret = dp_parse_test_timing_params3(ep, 0x234, &ep->test_data.test_rr_n);
	if (ret) {
		pr_err("failed to parse test_rr_n (0x234)\n");
		goto exit;
	}
	pr_debug("TEST_REFRESH_NUMERATOR = %d\n", ep->test_data.test_rr_n);

exit:
	return ret;
}

/**
 * mdss_dp_is_video_audio_test_requested() - checks for audio/video test request
 * @test: test requested by the sink
 *
 * Returns true if the requested test is a permitted audio/video test.
 */
static bool mdss_dp_is_video_audio_test_requested(u32 test)
{
	return (test == TEST_VIDEO_PATTERN) ||
		(test == (TEST_AUDIO_PATTERN | TEST_VIDEO_PATTERN)) ||
		(test == TEST_AUDIO_PATTERN) ||
		(test == (TEST_AUDIO_PATTERN | TEST_AUDIO_DISABLED_VIDEO));
}

/**
 * dp_is_test_supported() - checks if test requested by sink is supported
 * @test_requested: test requested by the sink
 *
 * Returns true if the requested test is supported.
 */
static bool dp_is_test_supported(u32 test_requested)
{
	return (test_requested == TEST_LINK_TRAINING) ||
		(test_requested == TEST_EDID_READ) ||
		(test_requested == PHY_TEST_PATTERN) ||
		mdss_dp_is_video_audio_test_requested(test_requested);
}

/**
 * dp_sink_parse_test_request() - parses test request parameters from sink
 * @ep: Display Port Driver data
 *
 * Parses the DPCD to check if an automated test is requested (Byte 0x201),
 * and what type of test automation is being requested (Byte 0x218).
 */
static void dp_sink_parse_test_request(struct mdss_dp_drv_pdata *ep)
{
	int ret = 0;
	char *bp;
	char data;
	struct edp_buf *rp;
	int rlen;
	int const test_parameter_len = 0x1;
	int const device_service_irq_addr = 0x201;
	int const test_request_addr = 0x218;
	char buf[4];

	/**
	 * Read the device service IRQ vector (Byte 0x201) to determine
	 * whether an automated test has been requested by the sink.
	 */
	rlen = dp_aux_read_buf(ep, device_service_irq_addr,
			test_parameter_len, 0);
	if (rlen < test_parameter_len) {
		pr_err("failed to read device service IRQ vector\n");
		return;
	}
	rp = &ep->rxp;
	bp = rp->data;
	data = *bp++;

	pr_debug("device service irq vector = 0x%x\n", data);

	if (!(data & BIT(1))) {
		pr_debug("no test requested\n");
		return;
	}

	/**
	 * Read the test request byte (Byte 0x218) to determine what type
	 * of automated test has been requested by the sink.
	 */
	rlen = dp_aux_read_buf(ep, test_request_addr,
			test_parameter_len, 0);
	if (rlen < test_parameter_len) {
		pr_err("failed to read test_requested\n");
		return;
	}
	rp = &ep->rxp;
	bp = rp->data;
	data = *bp++;

	if (!dp_is_test_supported(data)) {
		pr_debug("test 0x%x not supported\n", data);
		return;
	}

	pr_debug("%s (0x%x) requested\n", mdss_dp_get_test_name(data), data);
	ep->test_data.test_requested = data;

	if (ep->test_data.test_requested == PHY_TEST_PATTERN) {
		ret = dp_parse_phy_test_params(ep);
		if (ret)
			goto end;
		ret = dp_parse_link_training_params(ep);
	}

	if (ep->test_data.test_requested == TEST_LINK_TRAINING)
		ret = dp_parse_link_training_params(ep);

	if (mdss_dp_is_video_audio_test_requested(
				ep->test_data.test_requested)) {
		ret = dp_parse_video_pattern_params(ep);
		if (ret)
			goto end;
		ret = dp_parse_audio_pattern_params(ep);
	}
end:
	/* clear the test request IRQ */
	buf[0] = 1;
	dp_aux_write_buf(ep, test_request_addr, buf, 1, 0);

	/**
	 * Send a TEST_ACK if all test parameters are valid, otherwise send
	 * a TEST_NACK.
	 */
	if (ret)
		ep->test_data.response = TEST_NACK;
	else
		ep->test_data.response = TEST_ACK;
}

static int dp_cap_lane_rate_set(struct mdss_dp_drv_pdata *ep)
{
	char buf[4];
	int len = 0;
	struct dpcd_cap *cap;

	cap = &ep->dpcd;

	pr_debug("bw=%x lane=%d\n", ep->link_rate, ep->lane_cnt);
	buf[0] = ep->link_rate;
	buf[1] = ep->lane_cnt;
	if (cap->enhanced_frame)
		buf[1] |= 0x80;
	len = dp_aux_write_buf(ep, 0x100, buf, 2, 0);

	return len;
}

static int dp_lane_set_write(struct mdss_dp_drv_pdata *ep, int voltage_level,
		int pre_emphasis_level)
{
	int i;
	char buf[4];
	u32 max_level_reached = 0;

	if (voltage_level == DPCD_LINK_VOLTAGE_MAX) {
		pr_debug("max. voltage swing level reached %d\n",
				voltage_level);
		max_level_reached |= BIT(2);
	}

	if (pre_emphasis_level == DPCD_LINK_PRE_EMPHASIS_MAX) {
		pr_debug("max. pre-emphasis level reached %d\n",
				pre_emphasis_level);
		max_level_reached  |= BIT(5);
	}

	pr_debug("max_level_reached = 0x%x\n", max_level_reached);

	pre_emphasis_level <<= 3;

	for (i = 0; i < 4; i++)
		buf[i] = voltage_level | pre_emphasis_level | max_level_reached;

	pr_debug("p|v=0x%x\n", voltage_level | pre_emphasis_level);
	return dp_aux_write_buf(ep, 0x103, buf, 4, 0);
}

static int dp_train_pattern_set_write(struct mdss_dp_drv_pdata *ep,
						int pattern)
{
	char buf[4];

	pr_debug("pattern=%x\n", pattern);
	buf[0] = pattern;
	return dp_aux_write_buf(ep, 0x102, buf, 1, 0);
}

bool mdss_dp_aux_clock_recovery_done(struct mdss_dp_drv_pdata *ep)
{
	u32 mask;
	u32 data;

	if (ep->lane_cnt == 1) {
		mask = 0x01;	/* lane 0 */
		data = ep->link_status.lane_01_status;
	} else if (ep->lane_cnt == 2) {
		mask = 0x011; /*B lane 0, 1 */
		data = ep->link_status.lane_01_status;
	} else {
		mask = 0x01111; /*B lane 0, 1 */
		data = ep->link_status.lane_23_status;
		data <<= 8;
		data |= ep->link_status.lane_01_status;
	}

	pr_debug("data=%x mask=%x\n", data, mask);
	data &= mask;
	if (data == mask) /* all done */
		return true;

	return false;
}

bool mdss_dp_aux_channel_eq_done(struct mdss_dp_drv_pdata *ep)
{
	u32 mask;
	u32 data;

	pr_debug("Entered++\n");

	if (!ep->link_status.interlane_align_done) { /* not align */
		pr_err("interlane align failed\n");
		return false;
	}

	if (ep->lane_cnt == 1) {
		mask = 0x7;
		data = ep->link_status.lane_01_status;
	} else if (ep->lane_cnt == 2) {
		mask = 0x77;
		data = ep->link_status.lane_01_status;
	} else {
		mask = 0x7777;
		data = ep->link_status.lane_23_status;
		data <<= 8;
		data |= ep->link_status.lane_01_status;
	}

	pr_debug("data=%x mask=%x\n", data, mask);

	data &= mask;
	if (data == mask)/* all done */
		return true;

	return false;
}

void dp_sink_train_set_adjust(struct mdss_dp_drv_pdata *ep)
{
	int i;
	int max = 0;


	/* use the max level across lanes */
	for (i = 0; i < ep->lane_cnt; i++) {
		pr_debug("lane=%d req_voltage_swing=%d\n",
			i, ep->link_status.req_voltage_swing[i]);
		if (max < ep->link_status.req_voltage_swing[i])
			max = ep->link_status.req_voltage_swing[i];
	}

	ep->v_level = max;

	/* use the max level across lanes */
	max = 0;
	for (i = 0; i < ep->lane_cnt; i++) {
		pr_debug("lane=%d req_pre_emphasis=%d\n",
			i, ep->link_status.req_pre_emphasis[i]);
		if (max < ep->link_status.req_pre_emphasis[i])
			max = ep->link_status.req_pre_emphasis[i];
	}

	ep->p_level = max;

	/**
	 * Adjust the voltage swing and pre-emphasis level combination to within
	 * the allowable range.
	 */
	if (ep->v_level > DPCD_LINK_VOLTAGE_MAX) {
		pr_debug("Requested vSwingLevel=%d, change to %d\n",
				ep->v_level, DPCD_LINK_VOLTAGE_MAX);
		ep->v_level = DPCD_LINK_VOLTAGE_MAX;
	}

	if (ep->p_level > DPCD_LINK_PRE_EMPHASIS_MAX) {
		pr_debug("Requested preEmphasisLevel=%d, change to %d\n",
				ep->p_level, DPCD_LINK_PRE_EMPHASIS_MAX);
		ep->p_level = DPCD_LINK_PRE_EMPHASIS_MAX;
	}

	if ((ep->p_level > DPCD_LINK_PRE_EMPHASIS_LEVEL_1)
			&& (ep->v_level == DPCD_LINK_VOLTAGE_LEVEL_2)) {
		pr_debug("Requested preEmphasisLevel=%d, change to %d\n",
				ep->p_level, DPCD_LINK_PRE_EMPHASIS_LEVEL_1);
		ep->p_level = DPCD_LINK_PRE_EMPHASIS_LEVEL_1;
	}

	pr_debug("v_level=%d, p_level=%d\n",
					ep->v_level, ep->p_level);
}

static void dp_host_train_set(struct mdss_dp_drv_pdata *ep, int train)
{
	int bit, cnt;
	u32 data;


	bit = 1;
	bit  <<=  (train - 1);
	pr_debug("bit=%d train=%d\n", bit, train);
	dp_write(ep->base + DP_STATE_CTRL, bit);

	bit = 8;
	bit <<= (train - 1);
	cnt = 10;
	while (cnt--) {
		data = dp_read(ep->base + DP_MAINLINK_READY);
		if (data & bit)
			break;
	}

	if (cnt == 0)
		pr_err("set link_train=%d failed\n", train);
}

char vm_pre_emphasis[4][4] = {
	{0x00, 0x0B, 0x12, 0xFF},       /* pe0, 0 db */
	{0x00, 0x0A, 0x12, 0xFF},       /* pe1, 3.5 db */
	{0x00, 0x0C, 0xFF, 0xFF},       /* pe2, 6.0 db */
	{0xFF, 0xFF, 0xFF, 0xFF}        /* pe3, 9.5 db */
};

/* voltage swing, 0.2v and 1.0v are not support */
char vm_voltage_swing[4][4] = {
	{0x07, 0x0F, 0x14, 0xFF}, /* sw0, 0.4v  */
	{0x11, 0x1D, 0x1F, 0xFF}, /* sw1, 0.6 v */
	{0x18, 0x1F, 0xFF, 0xFF}, /* sw1, 0.8 v */
	{0xFF, 0xFF, 0xFF, 0xFF}  /* sw1, 1.2 v, optional */
};

void mdss_dp_aux_update_voltage_and_pre_emphasis_lvl(
		struct mdss_dp_drv_pdata *dp)
{
	u32 value0 = 0;
	u32 value1 = 0;

	pr_debug("v=%d p=%d\n", dp->v_level, dp->p_level);

	value0 = vm_voltage_swing[(int)(dp->v_level)][(int)(dp->p_level)];
	value1 = vm_pre_emphasis[(int)(dp->v_level)][(int)(dp->p_level)];

	/* program default setting first */
	dp_write(dp->phy_io.base + QSERDES_TX0_OFFSET + TXn_TX_DRV_LVL, 0x2A);
	dp_write(dp->phy_io.base + QSERDES_TX1_OFFSET + TXn_TX_DRV_LVL, 0x2A);
	dp_write(dp->phy_io.base + QSERDES_TX0_OFFSET + TXn_TX_EMP_POST1_LVL,
		0x20);
	dp_write(dp->phy_io.base + QSERDES_TX1_OFFSET + TXn_TX_EMP_POST1_LVL,
		0x20);

	/* Enable MUX to use Cursor values from these registers */
	value0 |= BIT(5);
	value1 |= BIT(5);
	/* Configure host and panel only if both values are allowed */
	if (value0 != 0xFF && value1 != 0xFF) {
		dp_write(dp->phy_io.base +
			QSERDES_TX0_OFFSET + TXn_TX_DRV_LVL,
			value0);
		dp_write(dp->phy_io.base +
			QSERDES_TX1_OFFSET + TXn_TX_DRV_LVL,
			value0);
		dp_write(dp->phy_io.base +
			QSERDES_TX0_OFFSET + TXn_TX_EMP_POST1_LVL,
			value1);
		dp_write(dp->phy_io.base +
			QSERDES_TX1_OFFSET + TXn_TX_EMP_POST1_LVL,
			value1);

		pr_debug("host PHY settings: value0=0x%x value1=0x%x\n",
						value0, value1);
		dp_lane_set_write(dp, dp->v_level, dp->p_level);
	}

}

static int dp_start_link_train_1(struct mdss_dp_drv_pdata *ep)
{
	int tries, old_v_level;
	int ret = 0;
	int usleep_time;
	int const maximum_retries = 5;

	pr_debug("Entered++\n");

	dp_write(ep->base + DP_STATE_CTRL, 0x0);
	/* Make sure to clear the current pattern before starting a new one */
	wmb();

	dp_host_train_set(ep, 0x01); /* train_1 */
	dp_cap_lane_rate_set(ep);
	dp_train_pattern_set_write(ep, 0x21); /* train_1 */
	mdss_dp_aux_update_voltage_and_pre_emphasis_lvl(ep);

	tries = 0;
	old_v_level = ep->v_level;
	while (1) {
		usleep_time = ep->dpcd.training_read_interval;
		usleep_range(usleep_time, usleep_time + 50);

		ret = mdss_dp_aux_link_status_read(ep, 6);
		if (ret == -ENODEV)
			break;

		if (mdss_dp_aux_clock_recovery_done(ep)) {
			ret = 0;
			break;
		}

		if (ep->v_level == DPCD_LINK_VOLTAGE_MAX) {
			ret = -EAGAIN;
			break;	/* quit */
		}

		if (old_v_level == ep->v_level) {
			tries++;
			if (tries >= maximum_retries) {
				ret = -EAGAIN;
				break;	/* quit */
			}
		} else {
			tries = 0;
			old_v_level = ep->v_level;
		}

		dp_sink_train_set_adjust(ep);
		mdss_dp_aux_update_voltage_and_pre_emphasis_lvl(ep);
	}

	return ret;
}

static int dp_start_link_train_2(struct mdss_dp_drv_pdata *ep)
{
	int tries = 0;
	int ret = 0;
	int usleep_time;
	char pattern;
	int const maximum_retries = 5;

	pr_debug("Entered++\n");

	if (ep->dpcd.flags & DPCD_TPS3)
		pattern = 0x03;
	else
		pattern = 0x02;

	mdss_dp_aux_update_voltage_and_pre_emphasis_lvl(ep);
	dp_host_train_set(ep, pattern);
	dp_train_pattern_set_write(ep, pattern | 0x20);/* train_2 */

	do  {
		usleep_time = ep->dpcd.training_read_interval;
		usleep_range(usleep_time, usleep_time + 50);

		ret = mdss_dp_aux_link_status_read(ep, 6);
		if (ret == -ENODEV)
			break;

		if (mdss_dp_aux_channel_eq_done(ep)) {
			ret = 0;
			break;
		}

		if (tries > maximum_retries) {
			ret = -EAGAIN;
			break;
		}
		tries++;

		dp_sink_train_set_adjust(ep);
		mdss_dp_aux_update_voltage_and_pre_emphasis_lvl(ep);
	} while (1);

	return ret;
}

static int dp_link_rate_down_shift(struct mdss_dp_drv_pdata *ep)
{
	int ret = 0;

	if (!ep)
		return -EINVAL;

	switch (ep->link_rate) {
	case DP_LINK_RATE_540:
		ep->link_rate = DP_LINK_RATE_270;
		break;
	case DP_LINK_RATE_270:
		ep->link_rate = DP_LINK_RATE_162;
		break;
	case DP_LINK_RATE_162:
	default:
		ret = -EINVAL;
		break;
	}

	pr_debug("new rate=%d\n", ep->link_rate);

	return ret;
}

static void dp_clear_training_pattern(struct mdss_dp_drv_pdata *ep)
{
	int usleep_time;

	pr_debug("Entered++\n");
	dp_train_pattern_set_write(ep, 0);
	usleep_time = ep->dpcd.training_read_interval;
	usleep_range(usleep_time, usleep_time + 50);
}

int mdss_dp_link_train(struct mdss_dp_drv_pdata *dp)
{
	int ret = 0;

	ret = dp_aux_chan_ready(dp);
	if (ret) {
		pr_err("LINK Train failed: aux chan NOT ready\n");
		return ret;
	}

	dp->v_level = 0; /* start from default level */
	dp->p_level = 0;
	mdss_dp_config_ctrl(dp);

	mdss_dp_state_ctrl(&dp->ctrl_io, 0);

	ret = dp_start_link_train_1(dp);
	if (ret < 0) {
		if ((ret == -EAGAIN) && !dp_link_rate_down_shift(dp)) {
			pr_debug("retry with lower rate\n");
			ret = -EINVAL;
			goto clear;
		} else {
			pr_err("Training 1 failed\n");
			ret = -EINVAL;
			goto clear;
		}
	}

	pr_debug("Training 1 completed successfully\n");

	dp_write(dp->base + DP_STATE_CTRL, 0x0);
	/* Make sure to clear the current pattern before starting a new one */
	wmb();

	ret = dp_start_link_train_2(dp);
	if (ret < 0) {
		if ((ret == -EAGAIN) && !dp_link_rate_down_shift(dp)) {
			pr_debug("retry with lower rate\n");
			ret = -EINVAL;
			goto clear;
		} else {
			pr_err("Training 2 failed\n");
			ret = -EINVAL;
			goto clear;
		}
	}

	pr_debug("Training 2 completed successfully\n");

	dp_write(dp->base + DP_STATE_CTRL, 0x0);
	/* Make sure to clear the current pattern before starting a new one */
	wmb();

clear:
	dp_clear_training_pattern(dp);

	return ret;
}

void mdss_dp_aux_parse_sink_status_field(struct mdss_dp_drv_pdata *ep)
{
	dp_sink_parse_sink_count(ep);
	mdss_dp_aux_link_status_read(ep, 6);
	dp_sink_parse_test_request(ep);
}

int mdss_dp_dpcd_status_read(struct mdss_dp_drv_pdata *ep)
{
	struct dpcd_link_status *sp;
	int ret = 0; /* not sync */

	ret = mdss_dp_aux_link_status_read(ep, 6);

	if (ret > 0) {
		sp = &ep->link_status;
		ret = sp->port_0_in_sync; /* 1 == sync */
	}

	return ret;
}

void mdss_dp_fill_link_cfg(struct mdss_dp_drv_pdata *ep)
{
	struct display_timing_desc *dp;

	dp = &ep->edid.timing[0];
	ep->lane_cnt = ep->dpcd.max_lane_count;

	pr_debug("pclk=%d rate=%d lane=%d\n",
		ep->pixel_rate, ep->link_rate, ep->lane_cnt);

}

/**
 * mdss_dp_aux_config_sink_frame_crc() - enable/disable per frame CRC calc
 * @dp: Display Port Driver data
 * @enable: true - start CRC calculation, false - stop CRC calculation
 *
 * Program the sink DPCD register 0x270 to start/stop CRC calculation.
 * This would take effect with the next frame.
 */
int mdss_dp_aux_config_sink_frame_crc(struct mdss_dp_drv_pdata *dp,
	bool enable)
{
	int rlen;
	struct edp_buf *rp;
	u8 *bp;
	u8 buf[4];
	u8 crc_supported;
	u32 const test_sink_addr = 0x270;
	u32 const test_sink_misc_addr = 0x246;

	if (dp->sink_crc.en == enable) {
		pr_debug("sink crc already %s\n",
			enable ? "enabled" : "disabled");
		return 0;
	}

	rlen = dp_aux_read_buf(dp, test_sink_misc_addr, 1, 0);
	if (rlen < 1) {
		pr_err("failed to TEST_SINK_ADDR\n");
		return -EPERM;
	}
	rp = &dp->rxp;
	bp = rp->data;
	crc_supported = bp[0] & BIT(5);
	pr_debug("crc supported=%s\n", crc_supported ? "true" : "false");

	if (!crc_supported) {
		pr_err("sink does not support CRC generation\n");
		return -EINVAL;
	}

	buf[0] = enable ? 1 : 0;
	dp_aux_write_buf(dp, test_sink_addr, buf, BIT(0), 0);

	if (!enable)
		mdss_dp_reset_frame_crc_data(&dp->sink_crc);
	dp->sink_crc.en = enable;
	pr_debug("TEST_SINK_START (CRC calculation) %s\n",
		enable ? "enabled" : "disabled");

	return 0;
}

/**
 * mdss_dp_aux_read_sink_frame_crc() - read frame CRC values from the sink
 * @dp: Display Port Driver data
 */
int mdss_dp_aux_read_sink_frame_crc(struct mdss_dp_drv_pdata *dp)
{
	int rlen;
	struct edp_buf *rp;
	u8 *bp;
	u32 addr, len;
	struct mdss_dp_crc_data *crc = &dp->sink_crc;

	addr = 0x270; /* TEST_SINK */
	len = 1; /* one byte */
	rlen = dp_aux_read_buf(dp, addr, len, 0);
	if (rlen < len) {
		pr_err("failed to read TEST SINK\n");
		return -EPERM;
	}
	rp = &dp->rxp;
	bp = rp->data;
	if (!(bp[0] & BIT(0))) {
		pr_err("Sink side CRC calculation not enabled, TEST_SINK=0x%08x\n",
			(u32)bp[0]);
		return -EINVAL;
	}

	addr = 0x240; /* TEST_CRC_R_Cr */
	len = 2; /* 2 bytes */
	rlen = dp_aux_read_buf(dp, addr, len, 0);
	if (rlen < len) {
		pr_err("failed to read TEST_CRC_R_Cr\n");
		return -EPERM;
	}
	rp = &dp->rxp;
	bp = rp->data;
	crc->r_cr = bp[0] | (bp[1] << 8);

	addr = 0x242; /* TEST_CRC_G_Y */
	len = 2; /* 2 bytes */
	rlen = dp_aux_read_buf(dp, addr, len, 0);
	if (rlen < len) {
		pr_err("failed to read TEST_CRC_G_Y\n");
		return -EPERM;
	}
	rp = &dp->rxp;
	bp = rp->data;
	crc->g_y = bp[0] | (bp[1] << 8);

	addr = 0x244; /* TEST_CRC_B_Cb */
	len = 2; /* 2 bytes */
	rlen = dp_aux_read_buf(dp, addr, len, 0);
	if (rlen < len) {
		pr_err("failed to read TEST_CRC_B_Cb\n");
		return -EPERM;
	}
	rp = &dp->rxp;
	bp = rp->data;
	crc->b_cb = bp[0] | (bp[1] << 8);

	pr_debug("r_cr=0x%08x\t g_y=0x%08x\t b_cb=0x%08x\n",
		crc->r_cr, crc->g_y, crc->b_cb);

	return 0;
}

void mdss_dp_aux_init(struct mdss_dp_drv_pdata *ep)
{
	reinit_completion(&ep->aux_comp);
	reinit_completion(&ep->idle_comp);
	reinit_completion(&ep->video_comp);
	complete(&ep->video_comp); /* make non block at first time */

	dp_buf_init(&ep->txp, ep->txbuf, sizeof(ep->txbuf));
	dp_buf_init(&ep->rxp, ep->rxbuf, sizeof(ep->rxbuf));
}

int mdss_dp_aux_read_rx_status(struct mdss_dp_drv_pdata *dp, u8 *rx_status)
{
	bool cp_irq;
	int rc = 0;

	if (!dp) {
		pr_err("%s Invalid input\n", __func__);
		return -EINVAL;
	}

	*rx_status = 0;

	rc = dp_aux_read_buf(dp, DP_DPCD_CP_IRQ, 1, 0);
	if (!rc) {
		pr_err("Error reading CP_IRQ\n");
		return -EINVAL;
	}

	cp_irq = *dp->rxp.data & BIT(2);

	if (cp_irq) {
		rc = dp_aux_read_buf(dp, DP_DPCD_RXSTATUS, 1, 0);
		if (!rc) {
			pr_err("Error reading RxStatus\n");
			return -EINVAL;
		}

		*rx_status = *dp->rxp.data;
	}

	return 0;
}
