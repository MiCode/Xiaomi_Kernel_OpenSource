/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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

#include "mdss_dsi_cmd.h"
#include "mdss_dsi.h"

/*
 * mipi dsi buf mechanism
 */
char *mdss_dsi_buf_reserve(struct dsi_buf *dp, int len)
{
	dp->data += len;
	return dp->data;
}

char *mdss_dsi_buf_unreserve(struct dsi_buf *dp, int len)
{
	dp->data -= len;
	return dp->data;
}

char *mdss_dsi_buf_push(struct dsi_buf *dp, int len)
{
	dp->data -= len;
	dp->len += len;
	return dp->data;
}

char *mdss_dsi_buf_reserve_hdr(struct dsi_buf *dp, int hlen)
{
	dp->hdr = (u32 *)dp->data;
	return mdss_dsi_buf_reserve(dp, hlen);
}

char *mdss_dsi_buf_init(struct dsi_buf *dp)
{
	int off;

	dp->data = dp->start;
	off = (int) (unsigned long) dp->data;
	/* 8 byte align */
	off &= 0x07;
	if (off)
		off = 8 - off;
	dp->data += off;
	dp->len = 0;
	dp->read_cnt = 0;
	return dp->data;
}

int mdss_dsi_buf_alloc(struct device *ctrl_dev, struct dsi_buf *dp, int size)
{
	dp->start = dma_alloc_writecombine(ctrl_dev, size, &dp->dmap,
					   GFP_KERNEL);
	if (dp->start == NULL) {
		pr_err("%s:%u\n", __func__, __LINE__);
		return -ENOMEM;
	}
	dp->end = dp->start + size;
	dp->size = size;

	if ((int) (unsigned long) dp->start & 0x07)
		pr_err("%s: buf NOT 8 bytes aligned\n", __func__);

	dp->data = dp->start;
	dp->len = 0;
	dp->read_cnt = 0;
	return size;
}

/*
 * mipi dsi generic long write
 */
static int mdss_dsi_generic_lwrite(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	struct dsi_ctrl_hdr *dchdr;
	char *bp;
	u32 *hp;
	int i, len = 0;

	dchdr = &cm->dchdr;
	bp = mdss_dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);

	/* fill up payload */
	if (cm->payload) {
		len = dchdr->dlen;
		len += 3;
		len &= ~0x03;	/* multipled by 4 */
		for (i = 0; i < dchdr->dlen; i++)
			*bp++ = cm->payload[i];

		/* append 0xff to the end */
		for (; i < len; i++)
			*bp++ = 0xff;

		dp->len += len;
	}

	/* fill up header */
	hp = dp->hdr;
	*hp = 0;
	*hp = DSI_HDR_WC(dchdr->dlen);
	*hp |= DSI_HDR_VC(dchdr->vc);
	*hp |= DSI_HDR_LONG_PKT;
	*hp |= DSI_HDR_DTYPE(DTYPE_GEN_LWRITE);
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	mdss_dsi_buf_push(dp, DSI_HOST_HDR_SIZE);
	len += DSI_HOST_HDR_SIZE;

	return len;
}

/*
 * mipi dsi generic short write with 0, 1 2 parameters
 */
static int mdss_dsi_generic_swrite(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	struct dsi_ctrl_hdr *dchdr;
	u32 *hp;
	int len;

	dchdr = &cm->dchdr;
	if (dchdr->dlen && cm->payload == 0) {
		pr_err("%s: NO payload error\n", __func__);
		return 0;
	}

	mdss_dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(dchdr->vc);
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;


	len = (dchdr->dlen > 2) ? 2 : dchdr->dlen;

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

	mdss_dsi_buf_push(dp, DSI_HOST_HDR_SIZE);
	return DSI_HOST_HDR_SIZE; /* 4 bytes */
}

/*
 * mipi dsi gerneric read with 0, 1 2 parameters
 */
static int mdss_dsi_generic_read(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	struct dsi_ctrl_hdr *dchdr;
	u32 *hp;
	int len;

	dchdr = &cm->dchdr;
	if (dchdr->dlen && cm->payload == 0) {
		pr_err("%s: NO payload error\n", __func__);
		return 0;
	}

	mdss_dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(dchdr->vc);
	*hp |= DSI_HDR_BTA;
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	len = (dchdr->dlen > 2) ? 2 : dchdr->dlen;

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

	mdss_dsi_buf_push(dp, DSI_HOST_HDR_SIZE);
	return DSI_HOST_HDR_SIZE; /* 4 bytes */
}

/*
 * mipi dsi dcs long write
 */
static int mdss_dsi_dcs_lwrite(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	struct dsi_ctrl_hdr *dchdr;
	char *bp;
	u32 *hp;
	int i, len = 0;

	dchdr = &cm->dchdr;
	bp = mdss_dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);

	/*
	 * fill up payload
	 * dcs command byte (first byte) followed by payload
	 */
	if (cm->payload) {
		len = dchdr->dlen;
		len += 3;
		len &= ~0x03;	/* multipled by 4 */
		for (i = 0; i < dchdr->dlen; i++)
			*bp++ = cm->payload[i];

		/* append 0xff to the end */
		for (; i < len; i++)
			*bp++ = 0xff;

		dp->len += len;
	}

	/* fill up header */
	hp = dp->hdr;
	*hp = 0;
	*hp = DSI_HDR_WC(dchdr->dlen);
	*hp |= DSI_HDR_VC(dchdr->vc);
	*hp |= DSI_HDR_LONG_PKT;
	*hp |= DSI_HDR_DTYPE(DTYPE_DCS_LWRITE);
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	mdss_dsi_buf_push(dp, DSI_HOST_HDR_SIZE);

	len += DSI_HOST_HDR_SIZE;
	return len;
}

/*
 * mipi dsi dcs short write with 0 parameters
 */
static int mdss_dsi_dcs_swrite(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	struct dsi_ctrl_hdr *dchdr;
	u32 *hp;
	int len;

	dchdr = &cm->dchdr;
	if (cm->payload == 0) {
		pr_err("%s: NO payload error\n", __func__);
		return -EINVAL;
	}

	mdss_dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(dchdr->vc);
	if (dchdr->ack)		/* ask ACK trigger msg from peripeheral */
		*hp |= DSI_HDR_BTA;
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	len = (dchdr->dlen > 1) ? 1 : dchdr->dlen;

	*hp |= DSI_HDR_DTYPE(DTYPE_DCS_WRITE);
	*hp |= DSI_HDR_DATA1(cm->payload[0]);	/* dcs command byte */
	*hp |= DSI_HDR_DATA2(0);

	mdss_dsi_buf_push(dp, DSI_HOST_HDR_SIZE);
	return DSI_HOST_HDR_SIZE; /* 4 bytes */
}

/*
 * mipi dsi dcs short write with 1 parameters
 */
static int mdss_dsi_dcs_swrite1(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	struct dsi_ctrl_hdr *dchdr;
	u32 *hp;

	dchdr = &cm->dchdr;
	if (dchdr->dlen < 2 || cm->payload == 0) {
		pr_err("%s: NO payload error\n", __func__);
		return -EINVAL;
	}

	mdss_dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(dchdr->vc);
	if (dchdr->ack)		/* ask ACK trigger msg from peripeheral */
		*hp |= DSI_HDR_BTA;
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	*hp |= DSI_HDR_DTYPE(DTYPE_DCS_WRITE1);
	*hp |= DSI_HDR_DATA1(cm->payload[0]);	/* dcs comamnd byte */
	*hp |= DSI_HDR_DATA2(cm->payload[1]);	/* parameter */

	mdss_dsi_buf_push(dp, DSI_HOST_HDR_SIZE);
	return DSI_HOST_HDR_SIZE; /* 4 bytes */
}
/*
 * mipi dsi dcs read with 0 parameters
 */

static int mdss_dsi_dcs_read(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	struct dsi_ctrl_hdr *dchdr;
	u32 *hp;

	dchdr = &cm->dchdr;
	if (cm->payload == 0) {
		pr_err("%s: NO payload error\n", __func__);
		return -EINVAL;
	}

	mdss_dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(dchdr->vc);
	*hp |= DSI_HDR_BTA;
	*hp |= DSI_HDR_DTYPE(DTYPE_DCS_READ);
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	*hp |= DSI_HDR_DATA1(cm->payload[0]);	/* dcs command byte */
	*hp |= DSI_HDR_DATA2(0);

	mdss_dsi_buf_push(dp, DSI_HOST_HDR_SIZE);
	return DSI_HOST_HDR_SIZE; /* 4 bytes */
}

static int mdss_dsi_cm_on(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	struct dsi_ctrl_hdr *dchdr;
	u32 *hp;

	dchdr = &cm->dchdr;
	mdss_dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(dchdr->vc);
	*hp |= DSI_HDR_DTYPE(DTYPE_CM_ON);
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	mdss_dsi_buf_push(dp, DSI_HOST_HDR_SIZE);
	return DSI_HOST_HDR_SIZE; /* 4 bytes */
}

static int mdss_dsi_dsc_pps(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	struct dsi_ctrl_hdr *dchdr;
	char *bp;
	u32 *hp;
	int i, len = 0;

	dchdr = &cm->dchdr;
	bp = mdss_dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);

	/*
	 * fill up payload
	 * dcs command byte (first byte) followed by payload
	 */
	if (cm->payload) {
		len = dchdr->dlen;
		len += 3;
		len &= ~0x03;	/* multipled by 4 */
		for (i = 0; i < dchdr->dlen; i++)
			*bp++ = cm->payload[i];

		/* append 0xff to the end */
		for (; i < len; i++)
			*bp++ = 0xff;

		dp->len += len;
	}

	/* fill up header */
	hp = dp->hdr;
	*hp = 0;
	*hp = DSI_HDR_WC(dchdr->dlen);
	*hp |= DSI_HDR_VC(dchdr->vc);
	*hp |= DSI_HDR_LONG_PKT;
	*hp |= DSI_HDR_DTYPE(DTYPE_PPS);
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	mdss_dsi_buf_push(dp, DSI_HOST_HDR_SIZE);

	len += DSI_HOST_HDR_SIZE;
	return len;
}

static int mdss_dsi_cm_off(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	struct dsi_ctrl_hdr *dchdr;
	u32 *hp;

	dchdr = &cm->dchdr;
	mdss_dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(dchdr->vc);
	*hp |= DSI_HDR_DTYPE(DTYPE_CM_OFF);
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	mdss_dsi_buf_push(dp, DSI_HOST_HDR_SIZE);
	return DSI_HOST_HDR_SIZE; /* 4 bytes */
}

static int mdss_dsi_peripheral_on(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	struct dsi_ctrl_hdr *dchdr;
	u32 *hp;

	dchdr = &cm->dchdr;
	mdss_dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(dchdr->vc);
	*hp |= DSI_HDR_DTYPE(DTYPE_PERIPHERAL_ON);
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	mdss_dsi_buf_push(dp, DSI_HOST_HDR_SIZE);
	return DSI_HOST_HDR_SIZE; /* 4 bytes */
}

static int mdss_dsi_peripheral_off(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	struct dsi_ctrl_hdr *dchdr;
	u32 *hp;

	dchdr = &cm->dchdr;
	mdss_dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(dchdr->vc);
	*hp |= DSI_HDR_DTYPE(DTYPE_PERIPHERAL_OFF);
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	mdss_dsi_buf_push(dp, DSI_HOST_HDR_SIZE);
	return DSI_HOST_HDR_SIZE; /* 4 bytes */
}

static int mdss_dsi_set_max_pktsize(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	struct dsi_ctrl_hdr *dchdr;
	u32 *hp;

	dchdr = &cm->dchdr;
	if (cm->payload == 0) {
		pr_err("%s: NO payload error\n", __func__);
		return 0;
	}

	mdss_dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(dchdr->vc);
	*hp |= DSI_HDR_DTYPE(DTYPE_MAX_PKTSIZE);
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	*hp |= DSI_HDR_DATA1(cm->payload[0]);
	*hp |= DSI_HDR_DATA2(cm->payload[1]);

	mdss_dsi_buf_push(dp, DSI_HOST_HDR_SIZE);
	return DSI_HOST_HDR_SIZE; /* 4 bytes */
}

static int mdss_dsi_compression_mode(struct dsi_buf *dp,
					struct dsi_cmd_desc *cm)
{
	struct dsi_ctrl_hdr *dchdr;
	u32 *hp;

	dchdr = &cm->dchdr;
	if (cm->payload == 0) {
		pr_err("%s: NO payload error\n", __func__);
		return 0;
	}

	mdss_dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(dchdr->vc);
	*hp |= DSI_HDR_DTYPE(DTYPE_COMPRESSION_MODE);
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	*hp |= DSI_HDR_DATA1(cm->payload[0]);
	*hp |= DSI_HDR_DATA2(cm->payload[1]);

	mdss_dsi_buf_push(dp, DSI_HOST_HDR_SIZE);
	return DSI_HOST_HDR_SIZE; /* 4 bytes */
}

static int mdss_dsi_null_pkt(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	struct dsi_ctrl_hdr *dchdr;
	u32 *hp;

	dchdr = &cm->dchdr;
	mdss_dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp = DSI_HDR_WC(dchdr->dlen);
	*hp |= DSI_HDR_LONG_PKT;
	*hp |= DSI_HDR_VC(dchdr->vc);
	*hp |= DSI_HDR_DTYPE(DTYPE_NULL_PKT);
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	mdss_dsi_buf_push(dp, DSI_HOST_HDR_SIZE);
	return DSI_HOST_HDR_SIZE; /* 4 bytes */
}

static int mdss_dsi_blank_pkt(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	struct dsi_ctrl_hdr *dchdr;
	u32 *hp;

	dchdr = &cm->dchdr;
	mdss_dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp = DSI_HDR_WC(dchdr->dlen);
	*hp |= DSI_HDR_LONG_PKT;
	*hp |= DSI_HDR_VC(dchdr->vc);
	*hp |= DSI_HDR_DTYPE(DTYPE_BLANK_PKT);
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	mdss_dsi_buf_push(dp, DSI_HOST_HDR_SIZE);
	return DSI_HOST_HDR_SIZE; /* 4 bytes */
}

/*
 * prepare cmd buffer to be txed
 */
int mdss_dsi_cmd_dma_add(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	struct dsi_ctrl_hdr *dchdr;
	int len = 0;

	dchdr = &cm->dchdr;

	switch (dchdr->dtype) {
	case DTYPE_GEN_WRITE:
	case DTYPE_GEN_WRITE1:
	case DTYPE_GEN_WRITE2:
		len = mdss_dsi_generic_swrite(dp, cm);
		break;
	case DTYPE_GEN_LWRITE:
		len = mdss_dsi_generic_lwrite(dp, cm);
		break;
	case DTYPE_GEN_READ:
	case DTYPE_GEN_READ1:
	case DTYPE_GEN_READ2:
		len = mdss_dsi_generic_read(dp, cm);
		break;
	case DTYPE_DCS_LWRITE:
		len = mdss_dsi_dcs_lwrite(dp, cm);
		break;
	case DTYPE_DCS_WRITE:
		len = mdss_dsi_dcs_swrite(dp, cm);
		break;
	case DTYPE_DCS_WRITE1:
		len = mdss_dsi_dcs_swrite1(dp, cm);
		break;
	case DTYPE_DCS_READ:
		len = mdss_dsi_dcs_read(dp, cm);
		break;
	case DTYPE_MAX_PKTSIZE:
		len = mdss_dsi_set_max_pktsize(dp, cm);
		break;
	case DTYPE_PPS:
		len = mdss_dsi_dsc_pps(dp, cm);
		break;
	case DTYPE_COMPRESSION_MODE:
		len = mdss_dsi_compression_mode(dp, cm);
		break;
	case DTYPE_NULL_PKT:
		len = mdss_dsi_null_pkt(dp, cm);
		break;
	case DTYPE_BLANK_PKT:
		len = mdss_dsi_blank_pkt(dp, cm);
		break;
	case DTYPE_CM_ON:
		len = mdss_dsi_cm_on(dp, cm);
		break;
	case DTYPE_CM_OFF:
		len = mdss_dsi_cm_off(dp, cm);
		break;
	case DTYPE_PERIPHERAL_ON:
		len = mdss_dsi_peripheral_on(dp, cm);
		break;
	case DTYPE_PERIPHERAL_OFF:
		len = mdss_dsi_peripheral_off(dp, cm);
		break;
	default:
		pr_debug("%s: dtype=%x NOT supported\n",
					__func__, dchdr->dtype);
		break;

	}

	return len;
}

/*
 * mdss_dsi_short_read1_resp: 1 parameter
 */
int mdss_dsi_short_read1_resp(struct dsi_buf *rp)
{
	/* strip out dcs type */
	rp->data++;
	rp->len = 1;
	/* 1 byte for dcs type + 1 byte for ECC + 1 byte for 2nd data byte */
	rp->read_cnt -= 3;
	return rp->len;
}

/*
 * mdss_dsi_short_read2_resp: 2 parameter
 */
int mdss_dsi_short_read2_resp(struct dsi_buf *rp)
{
	/* strip out dcs type */
	rp->data++;
	rp->len = 2;
	rp->read_cnt -= 2; /* 1 byte for dcs type + 1 byte for ECC */
	return rp->len;
}

int mdss_dsi_long_read_resp(struct dsi_buf *rp)
{
	/* strip out dcs header */
	rp->data += 4;
	rp->len -= 4;
	rp->read_cnt -= 6; /* 4 bytes for dcs header + 2 bytes for CRC */
	return rp->len;
}

static char set_tear_on[2] = {0x35, 0x00};
static struct dsi_cmd_desc dsi_tear_on_cmd = {
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(set_tear_on)}, set_tear_on};

static char set_tear_off[2] = {0x34, 0x00};
static struct dsi_cmd_desc dsi_tear_off_cmd = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(set_tear_off)}, set_tear_off};

void mdss_dsi_set_tear_on(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct dcs_cmd_req cmdreq;
	struct mdss_panel_info *pinfo;

	pinfo = &(ctrl->panel_data.panel_info);
	if (pinfo->dcs_cmd_by_left && ctrl->ndx != DSI_CTRL_LEFT)
		return;

	cmdreq.cmds = &dsi_tear_on_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}

void mdss_dsi_set_tear_off(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct dcs_cmd_req cmdreq;
	struct mdss_panel_info *pinfo;

	pinfo = &(ctrl->panel_data.panel_info);
	if (pinfo->dcs_cmd_by_left && ctrl->ndx != DSI_CTRL_LEFT)
		return;

	cmdreq.cmds = &dsi_tear_off_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}

/*
 * mdss_dsi_cmd_get: ctrl->cmd_mutex acquired by caller
 */
struct dcs_cmd_req *mdss_dsi_cmdlist_get(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct dcs_cmd_list *clist;
	struct dcs_cmd_req *req = NULL;

	mutex_lock(&ctrl->cmdlist_mutex);
	clist = &ctrl->cmdlist;
	if (clist->get != clist->put) {
		req = &clist->list[clist->get];
		clist->get++;
		clist->get %= CMD_REQ_MAX;
		clist->tot--;
		pr_debug("%s: tot=%d put=%d get=%d\n", __func__,
		clist->tot, clist->put, clist->get);
	}
	mutex_unlock(&ctrl->cmdlist_mutex);
	return req;
}

int mdss_dsi_cmdlist_put(struct mdss_dsi_ctrl_pdata *ctrl,
				struct dcs_cmd_req *cmdreq)
{
	struct dcs_cmd_req *req;
	struct dcs_cmd_list *clist;
	int ret = 0;

	mutex_lock(&ctrl->cmd_mutex);
	mutex_lock(&ctrl->cmdlist_mutex);
	clist = &ctrl->cmdlist;
	req = &clist->list[clist->put];
	*req = *cmdreq;
	clist->put++;
	clist->put %= CMD_REQ_MAX;
	clist->tot++;
	if (clist->put == clist->get) {
		/* drop the oldest one */
		pr_debug("%s: DROP, tot=%d put=%d get=%d\n", __func__,
			clist->tot, clist->put, clist->get);
		clist->get++;
		clist->get %= CMD_REQ_MAX;
		clist->tot--;
	}

	pr_debug("%s: tot=%d put=%d get=%d\n", __func__,
		clist->tot, clist->put, clist->get);

	mutex_unlock(&ctrl->cmdlist_mutex);

	if (req->flags & CMD_REQ_COMMIT) {
		if (!ctrl->cmdlist_commit)
			pr_err("cmdlist_commit not implemented!\n");
		else
			ret = ctrl->cmdlist_commit(ctrl, 0);
	}
	mutex_unlock(&ctrl->cmd_mutex);

	return ret;
}

