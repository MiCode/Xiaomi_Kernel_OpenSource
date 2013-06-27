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
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/iopoll.h>
#include <linux/of_device.h>

#include "mdss_panel.h"
#include "dsi_v2.h"

static struct dsi_panel_common_pdata *panel_common_data;
static struct dsi_interface dsi_intf;

static int dsi_off(struct mdss_panel_data *pdata)
{
	int rc = 0;

	pr_debug("turn off dsi controller\n");
	if (dsi_intf.off)
		rc = dsi_intf.off(pdata);

	if (rc) {
		pr_err("mdss_dsi_off DSI failed %d\n", rc);
		return rc;
	}
	return rc;
}

static int dsi_on(struct mdss_panel_data *pdata)
{
	int rc = 0;

	pr_debug("dsi_on DSI controller on\n");
	if (dsi_intf.on)
		rc = dsi_intf.on(pdata);

	if (rc) {
		pr_err("mdss_dsi_on DSI failed %d\n", rc);
		return rc;
	}
	return rc;
}
static int dsi_panel_handler(struct mdss_panel_data *pdata, int enable)
{
	int rc = 0;

	pr_debug("dsi_panel_handler enable=%d\n", enable);
	if (!panel_common_data || !pdata)
		return -ENODEV;

	if (enable) {
		if (panel_common_data->reset)
			panel_common_data->reset(pdata, 1);

		if (panel_common_data->on)
			rc = panel_common_data->on(pdata);

		if (rc)
			pr_err("dsi_panel_handler panel on failed %d\n", rc);
	} else {
		if (dsi_intf.op_mode_config)
			dsi_intf.op_mode_config(DSI_CMD_MODE, pdata);

		if (panel_common_data->off)
			panel_common_data->off(pdata);

		if (panel_common_data->reset)
			panel_common_data->reset(pdata, 0);
	}
	return rc;
}

static int dsi_event_handler(struct mdss_panel_data *pdata,
				int event, void *arg)
{
	int rc = 0;

	if (!pdata || !panel_common_data) {
		pr_err("%s: Invalid input data\n", __func__);
		return -ENODEV;
	}

	switch (event) {
	case MDSS_EVENT_UNBLANK:
		rc = dsi_on(pdata);
		break;
	case MDSS_EVENT_BLANK:
		rc = dsi_off(pdata);
		break;
	case MDSS_EVENT_PANEL_ON:
		rc = dsi_panel_handler(pdata, 1);
		break;
	case MDSS_EVENT_PANEL_OFF:
		rc = dsi_panel_handler(pdata, 0);
		break;
	default:
		pr_debug("%s: unhandled event=%d\n", __func__, event);
		break;
	}
	return rc;
}

static struct platform_device *get_dsi_platform_device(
			struct platform_device *dev)
{
	struct device_node *dsi_ctrl_np;
	struct platform_device *ctrl_pdev;

	dsi_ctrl_np = of_parse_phandle(dev->dev.of_node,
					"qcom,dsi-ctrl-phandle", 0);

	if (!dsi_ctrl_np)
		return NULL;

	ctrl_pdev = of_find_device_by_node(dsi_ctrl_np);
	if (!ctrl_pdev)
		return NULL;

	return ctrl_pdev;
}

int dsi_panel_device_register_v2(struct platform_device *dev,
				struct dsi_panel_common_pdata *panel_data,
				char backlight_ctrl)
{
	struct mipi_panel_info *mipi;
	struct platform_device *ctrl_pdev;
	int rc;
	u8 lanes = 0, bpp;
	u32 h_period, v_period;
	static struct mdss_panel_data dsi_panel_data;

	h_period = ((panel_data->panel_info.lcdc.h_pulse_width)
			+ (panel_data->panel_info.lcdc.h_back_porch)
			+ (panel_data->panel_info.xres)
			+ (panel_data->panel_info.lcdc.h_front_porch));

	v_period = ((panel_data->panel_info.lcdc.v_pulse_width)
			+ (panel_data->panel_info.lcdc.v_back_porch)
			+ (panel_data->panel_info.yres)
			+ (panel_data->panel_info.lcdc.v_front_porch));

	mipi  = &panel_data->panel_info.mipi;

	panel_data->panel_info.type =
		((mipi->mode == DSI_VIDEO_MODE)
			? MIPI_VIDEO_PANEL : MIPI_CMD_PANEL);

	if (mipi->data_lane3)
		lanes += 1;
	if (mipi->data_lane2)
		lanes += 1;
	if (mipi->data_lane1)
		lanes += 1;
	if (mipi->data_lane0)
		lanes += 1;


	if ((mipi->dst_format == DSI_CMD_DST_FORMAT_RGB888)
		|| (mipi->dst_format == DSI_VIDEO_DST_FORMAT_RGB888)
		|| (mipi->dst_format == DSI_VIDEO_DST_FORMAT_RGB666_LOOSE))
		bpp = 3;
	else if ((mipi->dst_format == DSI_CMD_DST_FORMAT_RGB565)
		|| (mipi->dst_format == DSI_VIDEO_DST_FORMAT_RGB565))
		bpp = 2;
	else
		bpp = 3; /* Default format set to RGB888 */

	if (panel_data->panel_info.type == MIPI_VIDEO_PANEL &&
		!panel_data->panel_info.clk_rate) {
		h_period += panel_data->panel_info.lcdc.xres_pad;
		v_period += panel_data->panel_info.lcdc.yres_pad;

		if (lanes > 0) {
			panel_data->panel_info.clk_rate =
			((h_period * v_period * (mipi->frame_rate) * bpp * 8)
			   / lanes);
		} else {
			pr_err("%s: forcing mdss_dsi lanes to 1\n", __func__);
			panel_data->panel_info.clk_rate =
				(h_period * v_period
					 * (mipi->frame_rate) * bpp * 8);
		}
	}

	ctrl_pdev = get_dsi_platform_device(dev);
	if (!ctrl_pdev)
		return -EPROBE_DEFER;

	dsi_panel_data.event_handler = dsi_event_handler;

	dsi_panel_data.panel_info = panel_data->panel_info;

	dsi_panel_data.set_backlight = panel_data->bl_fnc;
	panel_common_data = panel_data;
	/*
	 * register in mdp driver
	 */
	rc = mdss_register_panel(ctrl_pdev, &dsi_panel_data);
	if (rc) {
		dev_err(&dev->dev, "unable to register MIPI DSI panel\n");
		return rc;
	}

	pr_debug("%s: Panal data initialized\n", __func__);
	return 0;
}

void dsi_register_interface(struct dsi_interface *intf)
{
	dsi_intf = *intf;
}

int dsi_cmds_tx_v2(struct mdss_panel_data *pdata,
			struct dsi_buf *tp, struct dsi_cmd_desc *cmds,
			int cnt)
{
	int rc = 0;

	if (!dsi_intf.tx)
		return -EINVAL;

	rc = dsi_intf.tx(pdata, tp, cmds, cnt);
	return rc;
}

int dsi_cmds_rx_v2(struct mdss_panel_data *pdata,
			struct dsi_buf *tp, struct dsi_buf *rp,
			struct dsi_cmd_desc *cmds, int rlen)
{
	int rc = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	if (!dsi_intf.rx)
		return -EINVAL;

	rc = dsi_intf.rx(pdata, tp, rp, cmds, rlen);
	return rc;
}

static char *dsi_buf_reserve(struct dsi_buf *dp, int len)
{
	dp->data += len;
	return dp->data;
}


static char *dsi_buf_push(struct dsi_buf *dp, int len)
{
	dp->data -= len;
	dp->len += len;
	return dp->data;
}

static char *dsi_buf_reserve_hdr(struct dsi_buf *dp, int hlen)
{
	dp->hdr = (u32 *)dp->data;
	return dsi_buf_reserve(dp, hlen);
}

char *dsi_buf_init(struct dsi_buf *dp)
{
	int off;

	dp->data = dp->start;
	off = (int)dp->data;
	/* 8 byte align */
	off &= 0x07;
	if (off)
		off = 8 - off;
	dp->data += off;
	dp->len = 0;
	return dp->data;
}

int dsi_buf_alloc(struct dsi_buf *dp, int size)
{
	dp->start = kmalloc(size, GFP_KERNEL);
	if (dp->start == NULL) {
		pr_err("%s:%u\n", __func__, __LINE__);
		return -ENOMEM;
	}

	dp->end = dp->start + size;
	dp->size = size;

	if ((int)dp->start & 0x07) {
		pr_err("%s: buf NOT 8 bytes aligned\n", __func__);
		return -EINVAL;
	}

	dp->data = dp->start;
	dp->len = 0;
	return 0;
}

/*
 * mipi dsi generic long write
 */
static int dsi_generic_lwrite(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	char *bp;
	u32 *hp;
	int i, len;

	bp = dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);

	/* fill up payload */
	if (cm->payload) {
		len = cm->dlen;
		len += 3;
		len &= ~0x03; /* multipled by 4 */
		for (i = 0; i < cm->dlen; i++)
			*bp++ = cm->payload[i];

		/* append 0xff to the end */
		for (; i < len; i++)
			*bp++ = 0xff;

		dp->len += len;
	}

	/* fill up header */
	hp = dp->hdr;
	*hp = 0;
	*hp = DSI_HDR_WC(cm->dlen);
	*hp |= DSI_HDR_VC(cm->vc);
	*hp |= DSI_HDR_LONG_PKT;
	*hp |= DSI_HDR_DTYPE(DTYPE_GEN_LWRITE);
	if (cm->last)
		*hp |= DSI_HDR_LAST;

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);

	return dp->len;
}

/*
 * mipi dsi generic short write with 0, 1 2 parameters
 */
static int dsi_generic_swrite(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	u32 *hp;
	int len;

	if (cm->dlen && cm->payload == 0) {
		pr_err("%s: NO payload error\n", __func__);
		return 0;
	}

	dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(cm->vc);
	if (cm->last)
		*hp |= DSI_HDR_LAST;

	len = (cm->dlen > 2) ? 2 : cm->dlen;

	if (len == 1) {
		*hp |= DSI_HDR_DTYPE(DTYPE_GEN_WRITE1);
		*hp |= DSI_HDR_DATA1(cm->payload[0]);
		*hp |= DSI_HDR_DATA2(0);
	} else if (len == 2) {
		*hp |= DSI_HDR_DTYPE(DTYPE_GEN_WRITE2);
		*hp |= DSI_HDR_DATA1(cm->payload[0]);
		*hp |= DSI_HDR_DATA2(cm->payload[1]);
	} else {
		*hp |= DSI_HDR_DTYPE(DTYPE_GEN_WRITE);
		*hp |= DSI_HDR_DATA1(0);
		*hp |= DSI_HDR_DATA2(0);
	}

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);

	return dp->len;
}

/*
 * mipi dsi gerneric read with 0, 1 2 parameters
 */
static int dsi_generic_read(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	u32 *hp;
	int len;

	if (cm->dlen && cm->payload == 0) {
		pr_err("%s: NO payload error\n", __func__);
		return 0;
	}

	dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(cm->vc);
	*hp |= DSI_HDR_BTA;
	if (cm->last)
		*hp |= DSI_HDR_LAST;

	len = (cm->dlen > 2) ? 2 : cm->dlen;

	if (len == 1) {
		*hp |= DSI_HDR_DTYPE(DTYPE_GEN_READ1);
		*hp |= DSI_HDR_DATA1(cm->payload[0]);
		*hp |= DSI_HDR_DATA2(0);
	} else if (len == 2) {
		*hp |= DSI_HDR_DTYPE(DTYPE_GEN_READ2);
		*hp |= DSI_HDR_DATA1(cm->payload[0]);
		*hp |= DSI_HDR_DATA2(cm->payload[1]);
	} else {
		*hp |= DSI_HDR_DTYPE(DTYPE_GEN_READ);
		*hp |= DSI_HDR_DATA1(0);
		*hp |= DSI_HDR_DATA2(0);
	}

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);
	return dp->len;
}

/*
 * mipi dsi dcs long write
 */
static int dsi_dcs_lwrite(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	char *bp;
	u32 *hp;
	int i, len;

	bp = dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);

	/*
	 * fill up payload
	 * dcs command byte (first byte) followed by payload
	 */
	if (cm->payload) {
		len = cm->dlen;
		len += 3;
		len &= ~0x03; /* multipled by 4 */
		for (i = 0; i < cm->dlen; i++)
			*bp++ = cm->payload[i];

		/* append 0xff to the end */
		for (; i < len; i++)
			*bp++ = 0xff;

		dp->len += len;
	}

	/* fill up header */
	hp = dp->hdr;
	*hp = 0;
	*hp = DSI_HDR_WC(cm->dlen);
	*hp |= DSI_HDR_VC(cm->vc);
	*hp |= DSI_HDR_LONG_PKT;
	*hp |= DSI_HDR_DTYPE(DTYPE_DCS_LWRITE);
	if (cm->last)
		*hp |= DSI_HDR_LAST;

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);

	return dp->len;
}

/*
 * mipi dsi dcs short write with 0 parameters
 */
static int dsi_dcs_swrite(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	u32 *hp;
	int len;

	if (cm->payload == 0) {
		pr_err("%s: NO payload error\n", __func__);
		return -EINVAL;
	}

	dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(cm->vc);
	if (cm->ack)
		*hp |= DSI_HDR_BTA;
	if (cm->last)
		*hp |= DSI_HDR_LAST;

	len = (cm->dlen > 1) ? 1 : cm->dlen;

	*hp |= DSI_HDR_DTYPE(DTYPE_DCS_WRITE);
	*hp |= DSI_HDR_DATA1(cm->payload[0]); /* dcs command byte */
	*hp |= DSI_HDR_DATA2(0);

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);
	return dp->len;
}

/*
 * mipi dsi dcs short write with 1 parameters
 */
static int dsi_dcs_swrite1(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	u32 *hp;

	if (cm->dlen < 2 || cm->payload == 0) {
		pr_err("%s: NO payload error\n", __func__);
		return -EINVAL;
	}

	dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(cm->vc);
	if (cm->ack)
		*hp |= DSI_HDR_BTA;
	if (cm->last)
		*hp |= DSI_HDR_LAST;

	*hp |= DSI_HDR_DTYPE(DTYPE_DCS_WRITE1);
	*hp |= DSI_HDR_DATA1(cm->payload[0]); /* dcs comamnd byte */
	*hp |= DSI_HDR_DATA2(cm->payload[1]); /* parameter */

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);

	return dp->len;
}

/*
 * mipi dsi dcs read with 0 parameters
 */
static int dsi_dcs_read(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	u32 *hp;

	if (cm->payload == 0) {
		pr_err("%s: NO payload error\n", __func__);
		return -EINVAL;
	}

	dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(cm->vc);
	*hp |= DSI_HDR_BTA;
	*hp |= DSI_HDR_DTYPE(DTYPE_DCS_READ);
	if (cm->last)
		*hp |= DSI_HDR_LAST;

	*hp |= DSI_HDR_DATA1(cm->payload[0]); /* dcs command byte */
	*hp |= DSI_HDR_DATA2(0);

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);

	return dp->len; /* 4 bytes */
}

static int dsi_cm_on(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	u32 *hp;

	dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(cm->vc);
	*hp |= DSI_HDR_DTYPE(DTYPE_CM_ON);
	if (cm->last)
		*hp |= DSI_HDR_LAST;

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);

	return dp->len; /* 4 bytes */
}

static int dsi_cm_off(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	u32 *hp;

	dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(cm->vc);
	*hp |= DSI_HDR_DTYPE(DTYPE_CM_OFF);
	if (cm->last)
		*hp |= DSI_HDR_LAST;

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);

	return dp->len; /* 4 bytes */
}

static int dsi_peripheral_on(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	u32 *hp;

	dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(cm->vc);
	*hp |= DSI_HDR_DTYPE(DTYPE_PERIPHERAL_ON);
	if (cm->last)
		*hp |= DSI_HDR_LAST;

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);

	return dp->len; /* 4 bytes */
}

static int dsi_peripheral_off(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	u32 *hp;

	dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(cm->vc);
	*hp |= DSI_HDR_DTYPE(DTYPE_PERIPHERAL_OFF);
	if (cm->last)
		*hp |= DSI_HDR_LAST;

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);

	return dp->len; /* 4 bytes */
}

static int dsi_set_max_pktsize(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	u32 *hp;

	if (cm->payload == 0) {
		pr_err("%s: NO payload error\n", __func__);
		return 0;
	}

	dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(cm->vc);
	*hp |= DSI_HDR_DTYPE(DTYPE_MAX_PKTSIZE);
	if (cm->last)
		*hp |= DSI_HDR_LAST;

	*hp |= DSI_HDR_DATA1(cm->payload[0]);
	*hp |= DSI_HDR_DATA2(cm->payload[1]);

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);

	return dp->len; /* 4 bytes */
}

static int dsi_null_pkt(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	u32 *hp;

	dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp = DSI_HDR_WC(cm->dlen);
	*hp |= DSI_HDR_LONG_PKT;
	*hp |= DSI_HDR_VC(cm->vc);
	*hp |= DSI_HDR_DTYPE(DTYPE_NULL_PKT);
	if (cm->last)
		*hp |= DSI_HDR_LAST;

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);

	return dp->len; /* 4 bytes */
}

static int dsi_blank_pkt(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	u32 *hp;

	dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp = DSI_HDR_WC(cm->dlen);
	*hp |= DSI_HDR_LONG_PKT;
	*hp |= DSI_HDR_VC(cm->vc);
	*hp |= DSI_HDR_DTYPE(DTYPE_BLANK_PKT);
	if (cm->last)
		*hp |= DSI_HDR_LAST;

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);

	return dp->len; /* 4 bytes */
}

/*
 * prepare cmd buffer to be txed
 */
int dsi_cmd_dma_add(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	int len = 0;

	switch (cm->dtype) {
	case DTYPE_GEN_WRITE:
	case DTYPE_GEN_WRITE1:
	case DTYPE_GEN_WRITE2:
		len = dsi_generic_swrite(dp, cm);
		break;
	case DTYPE_GEN_LWRITE:
		len = dsi_generic_lwrite(dp, cm);
		break;
	case DTYPE_GEN_READ:
	case DTYPE_GEN_READ1:
	case DTYPE_GEN_READ2:
		len = dsi_generic_read(dp, cm);
		break;
	case DTYPE_DCS_LWRITE:
		len = dsi_dcs_lwrite(dp, cm);
		break;
	case DTYPE_DCS_WRITE:
		len = dsi_dcs_swrite(dp, cm);
		break;
	case DTYPE_DCS_WRITE1:
		len = dsi_dcs_swrite1(dp, cm);
		break;
	case DTYPE_DCS_READ:
		len = dsi_dcs_read(dp, cm);
		break;
	case DTYPE_MAX_PKTSIZE:
		len = dsi_set_max_pktsize(dp, cm);
		break;
	case DTYPE_NULL_PKT:
		len = dsi_null_pkt(dp, cm);
		break;
	case DTYPE_BLANK_PKT:
		len = dsi_blank_pkt(dp, cm);
		break;
	case DTYPE_CM_ON:
		len = dsi_cm_on(dp, cm);
		break;
	case DTYPE_CM_OFF:
		len = dsi_cm_off(dp, cm);
		break;
	case DTYPE_PERIPHERAL_ON:
		len = dsi_peripheral_on(dp, cm);
		break;
	case DTYPE_PERIPHERAL_OFF:
		len = dsi_peripheral_off(dp, cm);
		break;
	default:
		pr_debug("%s: dtype=%x NOT supported\n",
					__func__, cm->dtype);
		break;

	}

	return len;
}

/*
 * mdss_dsi_short_read1_resp: 1 parameter
 */
int dsi_short_read1_resp(struct dsi_buf *rp)
{
	/* strip out dcs type */
	rp->data++;
	rp->len = 1;
	return rp->len;
}

/*
 * mdss_dsi_short_read2_resp: 2 parameter
 */
int dsi_short_read2_resp(struct dsi_buf *rp)
{
	/* strip out dcs type */
	rp->data++;
	rp->len = 2;
	return rp->len;
}

int dsi_long_read_resp(struct dsi_buf *rp)
{
	short len;

	len = rp->data[2];
	len <<= 8;
	len |= rp->data[1];
	/* strip out dcs header */
	rp->data += 4;
	rp->len -= 4;
	/* strip out 2 bytes of checksum */
	rp->len -= 2;
	return len;
}
