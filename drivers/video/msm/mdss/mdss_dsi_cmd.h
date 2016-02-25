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

#ifndef MDSS_DSI_CMD_H
#define MDSS_DSI_CMD_H

#include "mdss.h"

struct mdss_dsi_ctrl_pdata;

#define DSI_HOST_HDR_SIZE	4
#define DSI_HDR_LAST		BIT(31)
#define DSI_HDR_LONG_PKT	BIT(30)
#define DSI_HDR_BTA		BIT(29)
#define DSI_HDR_VC(vc)		(((vc) & 0x03) << 22)
#define DSI_HDR_DTYPE(dtype)	(((dtype) & 0x03f) << 16)
#define DSI_HDR_DATA2(data)	(((data) & 0x0ff) << 8)
#define DSI_HDR_DATA1(data)	((data) & 0x0ff)
#define DSI_HDR_WC(wc)		((wc) & 0x0ffff)

#define MDSS_DSI_MRPS	0x04  /* Maximum Return Packet Size */

#define MDSS_DSI_LEN 8 /* 4 x 4 - 6 - 2, bytes dcs header+crc-align  */

struct dsi_buf {
	u32 *hdr;	/* dsi host header */
	char *start;	/* buffer start addr */
	char *end;	/* buffer end addr */
	int size;	/* size of buffer */
	char *data;	/* buffer */
	int len;	/* data length */
	int read_cnt;	/* DSI read count */
	dma_addr_t dmap; /* mapped dma addr */
};

/* dcs read/write */
#define DTYPE_DCS_WRITE		0x05	/* short write, 0 parameter */
#define DTYPE_DCS_WRITE1	0x15	/* short write, 1 parameter */
#define DTYPE_DCS_READ		0x06	/* read */
#define DTYPE_DCS_LWRITE	0x39	/* long write */

/* generic read/write */
#define DTYPE_GEN_WRITE		0x03	/* short write, 0 parameter */
#define DTYPE_GEN_WRITE1	0x13	/* short write, 1 parameter */
#define DTYPE_GEN_WRITE2	0x23	/* short write, 2 parameter */
#define DTYPE_GEN_LWRITE	0x29	/* long write */
#define DTYPE_GEN_READ		0x04	/* long read, 0 parameter */
#define DTYPE_GEN_READ1		0x14	/* long read, 1 parameter */
#define DTYPE_GEN_READ2		0x24	/* long read, 2 parameter */

#define DTYPE_COMPRESSION_MODE	0x07	/* compression mode */
#define DTYPE_PPS		0x0a	/* pps */
#define DTYPE_MAX_PKTSIZE	0x37	/* set max packet size */
#define DTYPE_NULL_PKT		0x09	/* null packet, no data */
#define DTYPE_BLANK_PKT		0x19	/* blankiing packet, no data */

#define DTYPE_CM_ON		0x02	/* color mode off */
#define DTYPE_CM_OFF		0x12	/* color mode on */
#define DTYPE_PERIPHERAL_OFF	0x22
#define DTYPE_PERIPHERAL_ON	0x32

/*
 * dcs response
 */
#define DTYPE_ACK_ERR_RESP      0x02
#define DTYPE_EOT_RESP          0x08    /* end of tx */
#define DTYPE_GEN_READ1_RESP    0x11    /* 1 parameter, short */
#define DTYPE_GEN_READ2_RESP    0x12    /* 2 parameter, short */
#define DTYPE_GEN_LREAD_RESP    0x1a
#define DTYPE_DCS_LREAD_RESP    0x1c
#define DTYPE_DCS_READ1_RESP    0x21    /* 1 parameter, short */
#define DTYPE_DCS_READ2_RESP    0x22    /* 2 parameter, short */

struct dsi_ctrl_hdr {
	char dtype;	/* data type */
	char last;	/* last in chain */
	char vc;	/* virtual chan */
	char ack;	/* ask ACK from peripheral */
	char wait;	/* ms */
	short dlen;	/* 16 bits */
} __packed;

struct dsi_cmd_desc {
	struct dsi_ctrl_hdr dchdr;
	char *payload;
};

#define CMD_REQ_MAX     4
#define CMD_REQ_RX      0x0001
#define CMD_REQ_COMMIT  0x0002
#define CMD_CLK_CTRL    0x0004
#define CMD_REQ_UNICAST 0x0008
#define CMD_REQ_DMA_TPG 0x0040
#define CMD_REQ_NO_MAX_PKT_SIZE 0x0008
#define CMD_REQ_LP_MODE 0x0010
#define CMD_REQ_HS_MODE 0x0020

struct dcs_cmd_req {
	struct dsi_cmd_desc *cmds;
	int cmds_cnt;
	u32 flags;
	int rlen;       /* rx length */
	char *rbuf;	/* rx buf */
	void (*cb)(int data);
};

struct dcs_cmd_list {
	int put;
	int get;
	int tot;
	struct dcs_cmd_req list[CMD_REQ_MAX];
};

char *mdss_dsi_buf_reserve(struct dsi_buf *dp, int len);
char *mdss_dsi_buf_unreserve(struct dsi_buf *dp, int len);
char *mdss_dsi_buf_push(struct dsi_buf *dp, int len);
char *mdss_dsi_buf_reserve_hdr(struct dsi_buf *dp, int hlen);
char *mdss_dsi_buf_init(struct dsi_buf *dp);
int mdss_dsi_buf_alloc(struct device *ctrl_dev, struct dsi_buf *dp, int size);
int mdss_dsi_cmd_dma_add(struct dsi_buf *dp, struct dsi_cmd_desc *cm);
int mdss_dsi_short_read1_resp(struct dsi_buf *rp);
int mdss_dsi_short_read2_resp(struct dsi_buf *rp);
int mdss_dsi_long_read_resp(struct dsi_buf *rp);
void mdss_dsi_set_tear_on(struct mdss_dsi_ctrl_pdata *ctrl);
void mdss_dsi_set_tear_off(struct mdss_dsi_ctrl_pdata *ctrl);
struct dcs_cmd_req *mdss_dsi_cmdlist_get(struct mdss_dsi_ctrl_pdata *ctrl,
				int from_mdp);
int mdss_dsi_cmdlist_put(struct mdss_dsi_ctrl_pdata *ctrl,
				struct dcs_cmd_req *cmdreq);
#endif
