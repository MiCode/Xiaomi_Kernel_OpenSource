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

#ifndef MDSS_DSI_H
#define MDSS_DSI_H

#include <linux/list.h>
#include <mach/scm-io.h>

#include "mdss_panel.h"

#define MIPI_OUTP(addr, data) writel_relaxed((data), (addr))
#define MIPI_INP(addr) readl_relaxed(addr)

#define MIPI_DSI_PRIM 1
#define MIPI_DSI_SECD 2

#define MIPI_DSI_PANEL_VGA	0
#define MIPI_DSI_PANEL_WVGA	1
#define MIPI_DSI_PANEL_WVGA_PT	2
#define MIPI_DSI_PANEL_FWVGA_PT	3
#define MIPI_DSI_PANEL_WSVGA_PT	4
#define MIPI_DSI_PANEL_QHD_PT 5
#define MIPI_DSI_PANEL_WXGA	6
#define MIPI_DSI_PANEL_WUXGA	7
#define MIPI_DSI_PANEL_720P_PT	8
#define DSI_PANEL_MAX	8

enum {
	DSI_VIDEO_MODE,
	DSI_CMD_MODE,
};

enum {
	ST_DSI_CLK_OFF,
	ST_DSI_SUSPEND,
	ST_DSI_RESUME,
	ST_DSI_PLAYING,
	ST_DSI_NUM
};

enum {
	EV_DSI_UPDATE,
	EV_DSI_DONE,
	EV_DSI_TOUT,
	EV_DSI_NUM
};

enum {
	LANDSCAPE = 1,
	PORTRAIT = 2,
};

enum {
	DSI_CMD_MODE_DMA,
	DSI_CMD_MODE_MDP,
};

enum {
	BL_PWM,
	BL_WLED,
	BL_DCS_CMD,
	UNKNOWN_CTRL,
};

enum {
	DSI_LP_MODE,
	DSI_HS_MODE,
};

#define DSI_NON_BURST_SYNCH_PULSE	0
#define DSI_NON_BURST_SYNCH_EVENT	1
#define DSI_BURST_MODE			2

#define DSI_RGB_SWAP_RGB	0
#define DSI_RGB_SWAP_RBG	1
#define DSI_RGB_SWAP_BGR	2
#define DSI_RGB_SWAP_BRG	3
#define DSI_RGB_SWAP_GRB	4
#define DSI_RGB_SWAP_GBR	5

#define DSI_VIDEO_DST_FORMAT_RGB565		0
#define DSI_VIDEO_DST_FORMAT_RGB666		1
#define DSI_VIDEO_DST_FORMAT_RGB666_LOOSE	2
#define DSI_VIDEO_DST_FORMAT_RGB888		3

#define DSI_CMD_DST_FORMAT_RGB111	0
#define DSI_CMD_DST_FORMAT_RGB332	3
#define DSI_CMD_DST_FORMAT_RGB444	4
#define DSI_CMD_DST_FORMAT_RGB565	6
#define DSI_CMD_DST_FORMAT_RGB666	7
#define DSI_CMD_DST_FORMAT_RGB888	8

#define DSI_CMD_TRIGGER_NONE		0x0	/* mdp trigger */
#define DSI_CMD_TRIGGER_TE		0x02
#define DSI_CMD_TRIGGER_SW		0x04
#define DSI_CMD_TRIGGER_SW_SEOF		0x05	/* cmd dma only */
#define DSI_CMD_TRIGGER_SW_TE		0x06

#define DSI_HOST_HDR_SIZE	4
#define DSI_HDR_LAST		BIT(31)
#define DSI_HDR_LONG_PKT	BIT(30)
#define DSI_HDR_BTA		BIT(29)
#define DSI_HDR_VC(vc)		(((vc) & 0x03) << 22)
#define DSI_HDR_DTYPE(dtype)	(((dtype) & 0x03f) << 16)
#define DSI_HDR_DATA2(data)	(((data) & 0x0ff) << 8)
#define DSI_HDR_DATA1(data)	((data) & 0x0ff)
#define DSI_HDR_WC(wc)		((wc) & 0x0ffff)

#define DSI_BUF_SIZE	1024
#define DSI_MRPS	0x04  /* Maximum Return Packet Size */

#define DSI_LEN 8 /* 4 x 4 - 6 - 2, bytes dcs header+crc-align  */

struct dsi_buf {
	u32 *hdr;	/* dsi host header */
	char *start;	/* buffer start addr */
	char *end;	/* buffer end addr */
	int size;	/* size of buffer */
	char *data;	/* buffer */
	int len;	/* data length */
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

#define DTYPE_TEAR_ON		0x35	/* set tear on */
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

struct dsi_cmd_desc {
	int dtype;
	int last;
	int vc;
	int ack;	/* ask ACK from peripheral */
	int wait;
	int dlen;
	char *payload;
};

struct dsi_panel_cmds_list {
	struct dsi_cmd_desc *buf;
	int size;
	char ctrl_state;
};

struct dsi_panel_common_pdata {
	struct mdss_panel_info panel_info;
	int (*on) (struct mdss_panel_data *pdata);
	int (*off) (struct mdss_panel_data *pdata);
	void (*reset)(struct mdss_panel_data *pdata, int enable);
	void (*bl_fnc) (struct mdss_panel_data *pdata, u32 bl_level);
	struct dsi_panel_cmds_list *dsi_panel_on_cmds;
	struct dsi_panel_cmds_list *dsi_panel_off_cmds;
};

struct dsi_interface {
	int (*on)(struct mdss_panel_data *pdata);
	int (*off)(struct mdss_panel_data *pdata);
	void (*op_mode_config)(int mode, struct mdss_panel_data *pdata);
	int (*tx)(struct mdss_panel_data *pdata,
		struct dsi_buf *tp, struct dsi_cmd_desc *cmds, int cnt);
	int (*rx)(struct mdss_panel_data *pdata,
		 struct dsi_buf *tp, struct dsi_buf *rp,
		struct dsi_cmd_desc *cmds, int len);
	int index;
	void *private;
};

int dsi_panel_device_register_v2(struct platform_device *pdev,
				struct dsi_panel_common_pdata *panel_data,
				char bl_ctrl);

void dsi_register_interface(struct dsi_interface *intf);

int dsi_cmds_rx_v2(struct mdss_panel_data *pdata,
			struct dsi_buf *tp, struct dsi_buf *rp,
			struct dsi_cmd_desc *cmds, int len);

int dsi_cmds_tx_v2(struct mdss_panel_data *pdata,
			struct dsi_buf *tp, struct dsi_cmd_desc *cmds,
			int cnt);

char *dsi_buf_init(struct dsi_buf *dp);

int dsi_buf_alloc(struct dsi_buf *dp, int size);

int dsi_cmd_dma_add(struct dsi_buf *dp, struct dsi_cmd_desc *cm);

int dsi_short_read1_resp(struct dsi_buf *rp);

int dsi_short_read2_resp(struct dsi_buf *rp);

int dsi_long_read_resp(struct dsi_buf *rp);

#endif /* MDSS_DSI_H */
