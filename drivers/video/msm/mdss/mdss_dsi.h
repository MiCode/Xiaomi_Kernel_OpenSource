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
 *
 */

#ifndef MDSS_DSI_H
#define MDSS_DSI_H

#include <linux/list.h>
#include <mach/scm-io.h>

#include "mdss_panel.h"

#define MMSS_SERDES_BASE_PHY 0x04f01000 /* mmss (De)Serializer CFG */

#define MIPI_OUTP(addr, data) writel_relaxed((data), (addr))
#define MIPI_INP(addr) readl_relaxed(addr)

#ifdef CONFIG_MSM_SECURE_IO
#define MIPI_OUTP_SECURE(addr, data) secure_writel((data), (addr))
#define MIPI_INP_SECURE(addr) secure_readl(addr)
#else
#define MIPI_OUTP_SECURE(addr, data) writel_relaxed((data), (addr))
#define MIPI_INP_SECURE(addr) readl_relaxed(addr)
#endif

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

enum {		/* mipi dsi panel */
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

enum dsi_trigger_type {
	DSI_CMD_MODE_DMA,
	DSI_CMD_MODE_MDP,
};

enum dsi_panel_bl_ctrl {
	BL_PWM,
	BL_WLED,
	BL_DCS_CMD,
	UNKNOWN_CTRL,
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

#define DSI_INTR_ERROR_MASK		BIT(25)
#define DSI_INTR_ERROR			BIT(24)
#define DSI_INTR_VIDEO_DONE_MASK	BIT(17)
#define DSI_INTR_VIDEO_DONE		BIT(16)
#define DSI_INTR_CMD_MDP_DONE_MASK	BIT(9)
#define DSI_INTR_CMD_MDP_DONE		BIT(8)
#define DSI_INTR_CMD_DMA_DONE_MASK	BIT(1)
#define DSI_INTR_CMD_DMA_DONE		BIT(0)

#define DSI_CMD_TRIGGER_NONE		0x0	/* mdp trigger */
#define DSI_CMD_TRIGGER_TE		0x02
#define DSI_CMD_TRIGGER_SW		0x04
#define DSI_CMD_TRIGGER_SW_SEOF		0x05	/* cmd dma only */
#define DSI_CMD_TRIGGER_SW_TE		0x06

extern struct device dsi_dev;
extern int mdss_dsi_clk_on;
extern u32 dsi_irq;

struct dsiphy_pll_divider_config {
	u32 clk_rate;
	u32 fb_divider;
	u32 ref_divider_ratio;
	u32 bit_clk_divider;	/* oCLK1 */
	u32 byte_clk_divider;	/* oCLK2 */
	u32 analog_posDiv;
	u32 digital_posDiv;
};

extern struct dsiphy_pll_divider_config pll_divider_config;

struct dsi_clk_mnd_table {
	u8 lanes;
	u8 bpp;
	u8 pll_digital_posDiv;
	u8 pclk_m;
	u8 pclk_n;
	u8 pclk_d;
};

static const struct dsi_clk_mnd_table mnd_table[] = {
	{ 1, 2,  8, 1, 1, 0},
	{ 1, 3, 12, 1, 1, 0},
	{ 2, 2,  4, 1, 1, 0},
	{ 2, 3,  6, 1, 1, 0},
	{ 3, 2,  1, 3, 8, 4},
	{ 3, 3,  4, 1, 1, 0},
	{ 4, 2,  2, 1, 1, 0},
	{ 4, 3,  3, 1, 1, 0},
};

struct dsi_clk_desc {
	u32 src;
	u32 m;
	u32 n;
	u32 d;
	u32 mnd_mode;
	u32 pre_div_func;
};

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
#define MDSS_DSI_MRPS	0x04  /* Maximum Return Packet Size */

#define MDSS_DSI_LEN 8 /* 4 x 4 - 6 - 2, bytes dcs header+crc-align  */

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

struct dsi_kickoff_action {
	struct list_head act_entry;
	void (*action) (void *);
	void *data;
};

struct mdss_panel_common_pdata {
	struct mdss_panel_info panel_info;
	int (*on) (struct mdss_panel_data *pdata);
	int (*off) (struct mdss_panel_data *pdata);
	void (*bl_fnc) (struct mdss_panel_data *pdata, u32 bl_level);
};

struct mdss_dsi_drv_pdata {
	struct regulator *vdd_vreg;
	struct regulator *vdd_io_vreg;
	struct regulator *dsi_vreg;
};

struct mdss_dsi_ctrl_pdata {
	int (*on) (struct mdss_panel_data *pdata);
	int (*off) (struct mdss_panel_data *pdata);
	struct mdss_panel_data panel_data;
	unsigned char *ctrl_base;
	char bl_ctrl;
};

int dsi_panel_device_register(struct platform_device *pdev,
			      struct mdss_panel_common_pdata *panel_data,
			      char bl_ctrl);

char *mdss_dsi_buf_reserve_hdr(struct dsi_buf *dp, int hlen);
char *mdss_dsi_buf_init(struct dsi_buf *dp);
void mdss_dsi_init(void);
int mdss_dsi_buf_alloc(struct dsi_buf *, int size);
int mdss_dsi_cmd_dma_add(struct dsi_buf *dp, struct dsi_cmd_desc *cm);
int mdss_dsi_cmds_tx(struct mdss_panel_data *pdata,
		struct dsi_buf *dp, struct dsi_cmd_desc *cmds, int cnt);

int mdss_dsi_cmd_dma_tx(struct dsi_buf *dp,
				struct mdss_panel_data *pdata);
int mdss_dsi_cmd_reg_tx(u32 data,
				unsigned char *ctrl_base);
int mdss_dsi_cmds_rx(struct mdss_panel_data *pdata,
			struct dsi_buf *tp, struct dsi_buf *rp,
			struct dsi_cmd_desc *cmds, int len);
int mdss_dsi_cmd_dma_rx(struct dsi_buf *tp, int rlen,
				struct mdss_panel_data *pdata);
void mdss_dsi_host_init(struct mipi_panel_info *pinfo,
				struct mdss_panel_data *pdata);
void mdss_dsi_op_mode_config(int mode,
				struct mdss_panel_data *pdata);
void mdss_dsi_cmd_mode_ctrl(int enable);
void mdp4_dsi_cmd_trigger(void);
void mdss_dsi_cmd_mdp_start(void);
void mdss_dsi_cmd_bta_sw_trigger(struct mdss_panel_data *pdata);
void mdss_dsi_ack_err_status(unsigned char *dsi_base);
void mdss_dsi_clk_enable(struct mdss_panel_data *pdata);
void mdss_dsi_clk_disable(struct mdss_panel_data *pdata);
void mdss_dsi_controller_cfg(int enable,
				struct mdss_panel_data *pdata);
void mdss_dsi_sw_reset(struct mdss_panel_data *pdata);

irqreturn_t mdss_dsi_isr(int irq, void *ptr);
void mdss_dsi_irq_handler_config(struct mdss_dsi_ctrl_pdata *ctrl_pdata);

void mipi_set_tx_power_mode(int mode, struct mdss_panel_data *pdata);
int mdss_dsi_clk_div_config(u8 bpp, u8 lanes,
			    u32 *expected_dsi_pclk);
int mdss_dsi_clk_init(struct platform_device *pdev);
void mdss_dsi_clk_deinit(struct device *dev);
void mdss_dsi_prepare_clocks(void);
void mdss_dsi_unprepare_clocks(void);
void mdss_dsi_panel_reset(int enable);
void mdss_dsi_phy_enable(unsigned char *ctrl_base, int on);
void mdss_dsi_phy_init(struct mdss_panel_data *pdata);
void mdss_dsi_phy_sw_reset(unsigned char *ctrl_base);

#endif /* MDSS_DSI_H */
