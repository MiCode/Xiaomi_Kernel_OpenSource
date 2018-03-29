/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <drm/drmP.h>
#include <drm/drm_gem.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/regulator/consumer.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <video/mipi_display.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <video/videomode.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>

#include "mtk_drm_drv.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp.h"
#include "mtk_drm_gem.h"
#include "mtk_dsi.h"
#include "mtk_mipi_tx.h"
#include "mtk_dsi_debugfs.h"

#define DSI_VIDEO_FIFO_DEPTH	(1920 / 4)
#define DSI_HOST_FIFO_DEPTH	64

#define DSI_START		0x00
#define SLEEPOUT_START		BIT(2)
#define VM_CMD_START		BIT(16)

#define DSI_INTEN		0x08
#define DSI_INTSTA		0x0c

#define DSI_CON_CTRL		0x10
#define DSI_RESET		BIT(0)
#define DSI_EN			BIT(1)

#define DSI_MODE_CTRL		0x14
#define MODE			(3)
#define CMD_MODE		0
#define SYNC_PULSE_MODE		1
#define SYNC_EVENT_MODE		2
#define BURST_MODE		3
#define FRM_MODE		BIT(16)
#define MIX_MODE		BIT(17)
#define V2C_SWITCH_ON		BIT(18)
#define C2V_SWITCH_ON		BIT(19)
#define SLEEP_MODE		BIT(20)

#define DSI_TXRX_CTRL		0x18
#define VC_NUM			(2 << 0)
#define LANE_NUM		(0xf << 2)
#define DIS_EOT			BIT(6)
#define NULL_EN			BIT(7)
#define TE_FREERUN		BIT(8)
#define EXT_TE_EN		BIT(9)
#define EXT_TE_EDGE		BIT(10)
#define MAX_RTN_SIZE		(0xf << 12)
#define HSTX_CKLP_EN		BIT(16)

#define DSI_PSCTRL		0x1c
#define DSI_PS_WC		0x3fff
#define DSI_PS_SEL		(3 << 16)
#define PACKED_PS_16BIT_RGB565	(0 << 16)
#define LOOSELY_PS_18BIT_RGB666	(1 << 16)
#define PACKED_PS_18BIT_RGB666	(2 << 16)
#define PACKED_PS_24BIT_RGB888	(3 << 16)

#define DSI_VSA_NL		0x20
#define DSI_VBP_NL		0x24
#define DSI_VFP_NL		0x28
#define DSI_VACT_NL		0x2C
#define DSI_HSA_WC		0x50
#define DSI_HBP_WC		0x54
#define DSI_HFP_WC		0x58

#define DSI_CMDQ_SIZE		0x60
#define CMDQ_SIZE		0x3f

#define DSI_HSTX_CKL_WC		0x64

#define DSI_RX_DATA0		0x74
#define DSI_RX_DATA1		0x78
#define DSI_RX_DATA2		0x7c
#define DSI_RX_DATA3		0x80

#define DSI_RACK		0x84
#define DSI_MEM_CONTI		0x90

#define DSI_PHY_LCCON		0x104
#define LC_HS_TX_EN		BIT(0)
#define LC_ULPM_EN		BIT(1)
#define LC_WAKEUP_EN		BIT(2)

#define DSI_PHY_LD0CON		0x108
#define LD0_HS_TX_EN		BIT(0)
#define LD0_ULPM_EN		BIT(1)
#define LD0_WAKEUP_EN		BIT(2)

#define DSI_PHY_TIMECON0	0x110
#define LPX			(0xff << 0)
#define HS_PRPR			(0xff << 8)
#define HS_ZERO			(0xff << 16)
#define HS_TRAIL		(0xff << 24)

#define DSI_PHY_TIMECON1	0x114
#define TA_GO			(0xff << 0)
#define TA_SURE			(0xff << 8)
#define TA_GET			(0xff << 16)
#define DA_HS_EXIT		(0xff << 24)

#define DSI_PHY_TIMECON2	0x118
#define CONT_DET		(0xff << 0)
#define CLK_ZERO		(0xff << 16)
#define CLK_TRAIL		(0xff << 24)

#define DSI_PHY_TIMECON3	0x11c
#define CLK_HS_PRPR		(0xff << 0)
#define CLK_HS_POST		(0xff << 8)
#define CLK_HS_EXIT		(0xff << 16)

#define DSI_PHY_TIMECON4	0x120
#define ULPS_WAKEUP		(0x1fffff << 0)

#define DSI_VM_CMD_CON		0x130
#define VM_CMD_EN		BIT(0)
#define TS_VFP_EN		BIT(5)

#define DSI_VM_CMDQ0		0x134

#define DSI_STATE_DBG6		0x160

#define DSI_CMDQ0		0x180

#define NS_TO_CYCLE(n, c)    ((n) / c + (((n) % c) ? 1 : 0))

#define MTK_DSI_IRQ_INTERRUPT_MODE

#define MTK_DSI_HOST_IS_READ(type) \
	((type == MIPI_DSI_GENERIC_READ_REQUEST_0_PARAM) || \
	(type == MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM) || \
	(type == MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM) || \
	(type == MIPI_DSI_DCS_READ))

#define MTK_DSI_HOST_IS_WRITE(type) \
	((type == MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM) || \
	(type == MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM) || \
	(type == MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM) || \
	(type == MIPI_DSI_DCS_SHORT_WRITE) || \
	(type == MIPI_DSI_DCS_SHORT_WRITE_PARAM) || \
	(type == MIPI_DSI_GENERIC_LONG_WRITE) || \
	(type == MIPI_DSI_DCS_LONG_WRITE))

struct dsi_cmd_t0 {
	u8 config;
	u8 type;
	u8 data0;
	u8 data1;
};

struct dsi_cmd_t1 {
	unsigned config;
	unsigned type;
	unsigned mem_start0;
	unsigned mem_start1;
};

struct dsi_cmd_t2 {
	u8 config;
	u8 type;
	u16 wc16;
	u8 *pdata;
};

struct dsi_cmd_t3 {
	unsigned config;
	unsigned type;
	unsigned mem_start0;
	unsigned mem_start1;
};

struct dsi_rx_data {
	u8 byte0;
	u8 byte1;
	u8 byte2;
	u8 byte3;
};

struct dsi_tx_cmdq {
	u8 byte0;
	u8 byte1;
	u8 byte2;
	u8 byte3;
};

struct dsi_tx_cmdq_regs {
	struct dsi_tx_cmdq data[32];
};

enum {
	DSI_INT_SLEEPOUT_DONE_FLAG	= BIT(6),
	DSI_INT_VM_CMD_DONE_FLAG	= BIT(5),
	DSI_INT_EXT_TE_RDY_FLAG		= BIT(4),
	DSI_INT_VM_DONE_FLAG		= BIT(3),
	DSI_INT_TE_RDY_FLAG		= BIT(2),
	DSI_INT_CMD_DONE_FLAG		= BIT(1),
	DSI_INT_LPRX_RD_RDY_FLAG	= BIT(0),
	DSI_INT_ALL_BITS		= (0x7F)
};

static void mtk_dsi_mask(struct mtk_dsi *dsi, u32 offset, u32 mask, u32 data)
{
	u32 temp = readl(dsi->regs + offset);

	writel((temp & ~mask) | (data & mask), dsi->regs + offset);
}

/* -------------------- Retrieve Information -------------------- */
void mtk_dsi_dump_registers(struct mtk_dsi *dsi)
{
	/*
	description of dsi status
	Bit Value   Description
	[0] 0x0001  Idle (wait for command)
	[1] 0x0002  Reading command queue for header
	[2] 0x0004  Sending type-0 command
	[3] 0x0008  Waiting frame data from RDMA for type-1 command
	[4] 0x0010  Sending type-1 command
	[5] 0x0020  Sending type-2 command
	[6] 0x0040  Reading command queue for data
	[7] 0x0080  Sending type-3 command
	[8] 0x0100  Sending BTA
	[9] 0x0200  Waiting RX-read data
	[10]    0x0400  Waiting SW RACK for RX-read data
	[11]    0x0800  Waiting TE
	[12]    0x1000  Get TE
	[13]    0x2000  Waiting external TE
	[14]    0x4000  Waiting SW RACK for TE
	*/

	const u8 *dsi_dbg_status_description[] = {
		"null",
		"Idle (wait for command)",
		"Reading command queue for header",
		"Sending type-0 command",
		"Waiting frame data from RDMA for type-1 command",
		"Sending type-1 command",
		"Sending type-2 command",
		"Reading command queue for data",
		"Sending type-3 command",
		"Sending BTA",
		"Waiting RX-read data ",
		"Waiting SW RACK for RX-read data",
		"Waiting TE",
		"Get TE ",
		"Waiting SW RACK for TE",
		"Waiting external TE",
	};

	u32 i, dsi_dbg6_status = (readl(dsi->regs + DSI_STATE_DBG6)) & 0xffff;
	int count = 0;
	struct mtk_mipi_tx *mipi_tx = phy_get_drvdata(dsi->phy);

	while (dsi_dbg6_status) {
		dsi_dbg6_status >>= 1;
		count++;
	}

	dev_info(dsi->dev, "---------- Start dump DSI registers ----------\n");
	dev_info(dsi->dev, "dsi_status_dbg6=0x%08x, count=%d, means: [%s]\n",
		dsi_dbg6_status, count, dsi_dbg_status_description[count]);
	dev_info(dsi->dev, "---------- Start dump DSI registers ----------\n");

	for (i = 0; i < 0x180; i += 0x10) {
		dev_info(dsi->dev, "DSI+%04x : 0x%08x  0x%08x  0x%08x  0x%08x\n",
			i, readl(dsi->regs + i), readl(dsi->regs + i + 0x4),
			readl(dsi->regs + i + 0x8), readl(dsi->regs + i + 0xc));
	}

	for (i = 0; i < 0x80; i += 0x10) {
		dev_info(dsi->dev, "DSI_CMD+%04x : 0x%08x  0x%08x  0x%08x  0x%08x\n",
			i, readl(dsi->regs+0x180+i), readl(dsi->regs+0x180+i+0x4),
			readl(dsi->regs+0x180+i+0x8), readl(dsi->regs+0x180+i+0xc));
	}

	for (i = 0; i < 0x90; i += 0x10) {
		dev_info(dsi->dev, "DSI_PHY+%04x : 0x%08x  0x%08x  0x%08x  0x%08x\n",
			i, readl(mipi_tx->regs+i), readl(mipi_tx->regs+i+0x4),
			readl(mipi_tx->regs+i+0x8), readl(mipi_tx->regs+i+0xc));
	}
}

static void mtk_dsi_phy_timconfig(struct mtk_dsi *dsi)
{
	u32 timcon0, timcon1, timcon2, timcon3;
	unsigned int ui, cycle_time;
	unsigned int lpx;

	ui = 1000 / dsi->data_rate + 0x01;
	cycle_time = 8000 / dsi->data_rate + 0x01;
	lpx = 5;

	timcon0 = (8 << 24) | (0xa << 16) | (0x6 << 8) | lpx;
	timcon1 = (7 << 24) | (5 * lpx << 16) | ((3 * lpx) / 2) << 8 |
		  (4 * lpx);
	timcon2 = ((NS_TO_CYCLE(0x64, cycle_time) + 0xa) << 24) |
		  (NS_TO_CYCLE(0x150, cycle_time) << 16);
	timcon3 = (2 * lpx) << 16 | NS_TO_CYCLE(80 + 52 * ui, cycle_time) << 8 |
		   NS_TO_CYCLE(0x40, cycle_time);

	writel(timcon0, dsi->regs + DSI_PHY_TIMECON0);
	writel(timcon1, dsi->regs + DSI_PHY_TIMECON1);
	writel(timcon2, dsi->regs + DSI_PHY_TIMECON2);
	writel(timcon3, dsi->regs + DSI_PHY_TIMECON3);
}

static void mtk_dsi_enable(struct mtk_dsi *dsi)
{
	mtk_dsi_mask(dsi, DSI_CON_CTRL, DSI_EN, DSI_EN);
}

static void mtk_dsi_disable(struct mtk_dsi *dsi)
{
	mtk_dsi_mask(dsi, DSI_CON_CTRL, DSI_EN, 0);
}

static void mtk_dsi_reset_engine(struct mtk_dsi *dsi)
{
	mtk_dsi_mask(dsi, DSI_CON_CTRL, DSI_RESET, DSI_RESET);
	mtk_dsi_mask(dsi, DSI_CON_CTRL, DSI_RESET, 0);
}

static void mtk_dsi_reset_all(struct mtk_dsi *dsi)
{
	regmap_update_bits(dsi->mmsys_sw_rst_b, dsi->sw_rst_b,
			   BIT(18), ~(BIT(18)));
	usleep_range(1000, 1100);

	regmap_update_bits(dsi->mmsys_sw_rst_b, dsi->sw_rst_b,
			   BIT(18), BIT(18));
}

static int mtk_dsi_poweron(struct mtk_dsi *dsi)
{
	struct device *dev = dsi->dev;
	int ret = 0;

	if (++dsi->refcount != 1)
		return 0;

	if (dsi->poweron)
		return ret;

	ret |= clk_prepare_enable(dsi->engine_clk);
	if (ret < 0) {
		dev_err(dev, "can't enable engine %d\n", ret);
		goto err_engine_clk;
	}

	ret |= clk_prepare_enable(dsi->digital_clk);
	if (ret < 0) {
		dev_err(dev, "can't enable digital %d\n", ret);
		goto err_digital_clk;
	}

	/**
	 * data_rate = (pixel_clock / 1000) * pixel_dipth * mipi_ratio;
	 * pixel_clock unit is Khz, data_rata unit is MHz, so need divide 1000.
	 * mipi_ratio is mipi clk coefficient for balance the pixel clk in mipi.
	 * we set mipi_ratio is 1.05.
	 */

	dsi->data_rate = dsi->vm.pixelclock * 3 * 21 / (1 * 1000 * 10);

	ret |= mtk_mipi_tx_set_data_rate(dsi->phy, dsi->data_rate, dsi->ssc_data);
	ret |= phy_power_on(dsi->phy);

	dsi->poweron = true;

	return ret;

err_digital_clk:
	clk_disable_unprepare(dsi->engine_clk);
err_engine_clk:
	dsi->refcount--;
	return ret;
}

static void mtk_dsi_clk_ulp_mode_enter(struct mtk_dsi *dsi)
{
	mtk_dsi_mask(dsi, DSI_PHY_LCCON, LC_HS_TX_EN, 0);
	mtk_dsi_mask(dsi, DSI_PHY_LCCON, LC_ULPM_EN, LC_ULPM_EN);
}

static void mtk_dsi_clk_ulp_mode_leave(struct mtk_dsi *dsi)
{
	mtk_dsi_mask(dsi, DSI_PHY_LCCON, LC_ULPM_EN, 0);
	mtk_dsi_mask(dsi, DSI_PHY_LCCON, LC_WAKEUP_EN, LC_WAKEUP_EN);
	mtk_dsi_mask(dsi, DSI_PHY_LCCON, LC_WAKEUP_EN, 0);
}

static void mtk_dsi_lane0_ulp_mode_enter(struct mtk_dsi *dsi)
{
	mtk_dsi_mask(dsi, DSI_PHY_LD0CON, LD0_HS_TX_EN, 0);
	mtk_dsi_mask(dsi, DSI_PHY_LD0CON, LD0_ULPM_EN, LD0_ULPM_EN);
}

static void mtk_dsi_lane0_ulp_mode_leave(struct mtk_dsi *dsi)
{
	mtk_dsi_mask(dsi, DSI_PHY_LD0CON, LD0_ULPM_EN, 0);
	mtk_dsi_mask(dsi, DSI_PHY_LD0CON, LD0_WAKEUP_EN, LD0_WAKEUP_EN);
	mtk_dsi_mask(dsi, DSI_PHY_LD0CON, LD0_WAKEUP_EN, 0);
}

static bool mtk_dsi_clk_hs_state(struct mtk_dsi *dsi)
{
	u32 tmp_reg1;

	tmp_reg1 = readl(dsi->regs + DSI_PHY_LCCON);
	return ((tmp_reg1 & LC_HS_TX_EN) == 1) ? true : false;
}

static void mtk_dsi_clk_hs_mode(struct mtk_dsi *dsi, bool enter)
{
	if (enter && !mtk_dsi_clk_hs_state(dsi))
		mtk_dsi_mask(dsi, DSI_PHY_LCCON, LC_HS_TX_EN, LC_HS_TX_EN);
	else if (!enter && mtk_dsi_clk_hs_state(dsi))
		mtk_dsi_mask(dsi, DSI_PHY_LCCON, LC_HS_TX_EN, 0);
}

static void mtk_dsi_set_mode(struct mtk_dsi *dsi)
{
	u32 vid_mode = CMD_MODE;

	if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO) {
		vid_mode = SYNC_PULSE_MODE;

		if ((dsi->mode_flags & MIPI_DSI_MODE_VIDEO_BURST) &&
		    !(dsi->mode_flags & MIPI_DSI_MODE_VIDEO_SYNC_PULSE))
			vid_mode = BURST_MODE;
	}

	writel(vid_mode, dsi->regs + DSI_MODE_CTRL);
}

static void mtk_dsi_set_vm_cmd(struct mtk_dsi *dsi)
{
	writel(0x3c, dsi->regs + DSI_MEM_CONTI);
	mtk_dsi_mask(dsi, DSI_VM_CMD_CON, VM_CMD_EN, VM_CMD_EN);
	mtk_dsi_mask(dsi, DSI_VM_CMD_CON, TS_VFP_EN, TS_VFP_EN);
}

static void mtk_dsi_ps_control_vact(struct mtk_dsi *dsi)
{
	struct videomode *vm = &dsi->vm;
	u32 dsi_buf_bpp, ps_wc;
	u32 ps_bpp_mode;

	if (dsi->format == MIPI_DSI_FMT_RGB565)
		dsi_buf_bpp = 2;
	else
		dsi_buf_bpp = 3;

	ps_wc = vm->hactive * dsi_buf_bpp;
	ps_bpp_mode = ps_wc;

	switch (dsi->format) {
	case MIPI_DSI_FMT_RGB888:
		ps_bpp_mode |= PACKED_PS_24BIT_RGB888;
		break;
	case MIPI_DSI_FMT_RGB666:
		ps_bpp_mode |= PACKED_PS_18BIT_RGB666;
		break;
	case MIPI_DSI_FMT_RGB666_PACKED:
		ps_bpp_mode |= LOOSELY_PS_18BIT_RGB666;
		break;
	case MIPI_DSI_FMT_RGB565:
		ps_bpp_mode |= PACKED_PS_16BIT_RGB565;
		break;
	}

	writel(vm->vactive, dsi->regs + DSI_VACT_NL);
	writel(ps_bpp_mode, dsi->regs + DSI_PSCTRL);
	writel(ps_wc, dsi->regs + DSI_HSTX_CKL_WC);
}

static void mtk_dsi_rxtx_control(struct mtk_dsi *dsi)
{
	u32 tmp_reg = 0;

	switch (dsi->lanes) {
	case 1:
		tmp_reg = 1 << 2;
		break;
	case 2:
		tmp_reg = 3 << 2;
		break;
	case 3:
		tmp_reg = 7 << 2;
		break;
	case 4:
		tmp_reg = 0xf << 2;
		break;
	default:
		tmp_reg = 0xf << 2;
		break;
	}

	tmp_reg |= (dsi->mode_flags & MIPI_DSI_CLOCK_NON_CONTINUOUS) << 6;
	tmp_reg |= (dsi->mode_flags & MIPI_DSI_MODE_EOT_PACKET) >> 3;

	writel(tmp_reg, dsi->regs + DSI_TXRX_CTRL);
}

static void mtk_dsi_ps_control(struct mtk_dsi *dsi)
{
	unsigned int dsi_tmp_buf_bpp;
	u32 tmp_reg;

	switch (dsi->format) {
	case MIPI_DSI_FMT_RGB888:
		tmp_reg = PACKED_PS_24BIT_RGB888;
		dsi_tmp_buf_bpp = 3;
		break;
	case MIPI_DSI_FMT_RGB666:
		tmp_reg = LOOSELY_PS_18BIT_RGB666;
		dsi_tmp_buf_bpp = 3;
		break;
	case MIPI_DSI_FMT_RGB666_PACKED:
		tmp_reg = PACKED_PS_18BIT_RGB666;
		dsi_tmp_buf_bpp = 3;
		break;
	case MIPI_DSI_FMT_RGB565:
		tmp_reg = PACKED_PS_16BIT_RGB565;
		dsi_tmp_buf_bpp = 2;
		break;
	default:
		tmp_reg = PACKED_PS_24BIT_RGB888;
		dsi_tmp_buf_bpp = 3;
		break;
	}

	tmp_reg += dsi->vm.hactive * dsi_tmp_buf_bpp & DSI_PS_WC;
	writel(tmp_reg, dsi->regs + DSI_PSCTRL);
}

static void mtk_dsi_config_vdo_timing(struct mtk_dsi *dsi)
{
	unsigned int horizontal_sync_active_byte;
	unsigned int horizontal_backporch_byte;
	unsigned int horizontal_frontporch_byte;
	unsigned int dsi_tmp_buf_bpp;

	struct videomode *vm = &dsi->vm;

	if (dsi->format == MIPI_DSI_FMT_RGB565)
		dsi_tmp_buf_bpp = 2;
	else
		dsi_tmp_buf_bpp = 3;

	writel(vm->vsync_len, dsi->regs + DSI_VSA_NL);
	writel(vm->vback_porch, dsi->regs + DSI_VBP_NL);
	writel(vm->vfront_porch, dsi->regs + DSI_VFP_NL);
	writel(vm->vactive, dsi->regs + DSI_VACT_NL);

	horizontal_sync_active_byte = (vm->hsync_len * dsi_tmp_buf_bpp - 10);

	if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_SYNC_PULSE)
		horizontal_backporch_byte =
			(vm->hback_porch * dsi_tmp_buf_bpp - 10);
	else
		horizontal_backporch_byte = ((vm->hback_porch + vm->hsync_len) *
			dsi_tmp_buf_bpp - 10);

	horizontal_frontporch_byte = (vm->hfront_porch * dsi_tmp_buf_bpp - 12);

	writel(horizontal_sync_active_byte, dsi->regs + DSI_HSA_WC);
	writel(horizontal_backporch_byte, dsi->regs + DSI_HBP_WC);
	writel(horizontal_frontporch_byte, dsi->regs + DSI_HFP_WC);

	mtk_dsi_ps_control(dsi);
}

static void mtk_dsi_set_interrupt_enable(struct mtk_dsi *dsi)
{
	writel(0x0000003f, dsi->regs + DSI_INTEN);
}

static void mtk_dsi_start(struct mtk_dsi *dsi)
{
	writel(0, dsi->regs + DSI_START);
	writel(1, dsi->regs + DSI_START);
}

static void mtk_dsi_stop(struct mtk_dsi *dsi)
{
	writel(0, dsi->regs + DSI_START);
}

static void mtk_dsi_set_cmd_mode(struct mtk_dsi *dsi)
{
	u32 tmp_reg1;

	tmp_reg1 = CMD_MODE;
	writel(tmp_reg1, dsi->regs + DSI_MODE_CTRL);
}

void mtk_dsi_irq_wakeup(struct mtk_dsi *dsi, u32 irq_bit)
{
	dsi->irq_data |= irq_bit;
}

#ifdef MTK_DSI_IRQ_INTERRUPT_MODE
static irqreturn_t mtk_dsi_irq(int irq, void *dev_id)
{
	struct mtk_dsi *dsi = dev_id;
#else
static irqreturn_t mtk_dsi_irq(struct mtk_dsi *dsi)
{
#endif

	u32 status, tmp;

	status = readl(dsi->regs + DSI_INTSTA);

	if (status & DSI_INT_LPRX_RD_RDY_FLAG) {
		/* write clear RD_RDY interrupt */
		/* write clear RD_RDY interrupt must be before DSI_RACK */
		/* because CMD_DONE will raise after DSI_RACK, */
		/* so write clear RD_RDY after that will clear CMD_DONE too */
		do {
			/* send read ACK */
			mtk_dsi_mask(dsi, DSI_RACK,	BIT(0), BIT(0));
			tmp = readl(dsi->regs + DSI_INTSTA);
		} while (tmp & BIT(31));

		mtk_dsi_mask(dsi, DSI_INTSTA, DSI_INT_LPRX_RD_RDY_FLAG, 0);
		mtk_dsi_irq_wakeup(dsi, DSI_INT_LPRX_RD_RDY_FLAG);
	}

	if (status & DSI_INT_CMD_DONE_FLAG) {
		mtk_dsi_mask(dsi, DSI_INTSTA, DSI_INT_CMD_DONE_FLAG, 0);
		mtk_dsi_irq_wakeup(dsi, DSI_INT_CMD_DONE_FLAG);
	}

	if (status & DSI_INT_TE_RDY_FLAG) {
		mtk_dsi_mask(dsi, DSI_INTSTA, DSI_INT_TE_RDY_FLAG, 0);
		mtk_dsi_irq_wakeup(dsi, DSI_INT_TE_RDY_FLAG);
	}

	if (status & DSI_INT_VM_DONE_FLAG) {
		mtk_dsi_mask(dsi, DSI_INTSTA, DSI_INT_VM_DONE_FLAG, 0);
		mtk_dsi_irq_wakeup(dsi, DSI_INT_VM_DONE_FLAG);
	}

	if (status & DSI_INT_EXT_TE_RDY_FLAG) {
		mtk_dsi_mask(dsi, DSI_INTSTA, DSI_INT_EXT_TE_RDY_FLAG, 0);
		mtk_dsi_irq_wakeup(dsi, DSI_INT_EXT_TE_RDY_FLAG);
	}

	if (status & DSI_INT_VM_CMD_DONE_FLAG) {
		mtk_dsi_mask(dsi, DSI_INTSTA, DSI_INT_VM_CMD_DONE_FLAG, 0);
		mtk_dsi_irq_wakeup(dsi, DSI_INT_VM_CMD_DONE_FLAG);
	}

	if (status & DSI_INT_SLEEPOUT_DONE_FLAG) {
		mtk_dsi_mask(dsi, DSI_INTSTA, DSI_INT_SLEEPOUT_DONE_FLAG, 0);
		mtk_dsi_irq_wakeup(dsi, DSI_INT_SLEEPOUT_DONE_FLAG);
	}

	return IRQ_HANDLED;
}

static void mtk_dsi_wait_for_idle(struct mtk_dsi *dsi)
{
	u32 timeout_ms = 500000; /* total 1s ~ 2s timeout */

	while (timeout_ms--) {
		if (!(readl(dsi->regs + DSI_INTSTA) & BIT(31)))
			break;

		usleep_range(2, 4);
	}

	if (0 == timeout_ms) {
		dev_info(dsi->dev, "polling dsi wait not busy timeout!\n");

		mtk_dsi_enable(dsi);
		mtk_dsi_reset_engine(dsi);
		mtk_dsi_dump_registers(dsi);
	}
}

static s32 mtk_dsi_wait_for_irq_timeout(struct mtk_dsi *dsi, u32 irq_bit, u32 timeout_ms)
{
	while (timeout_ms--) {
		if (dsi->irq_data & irq_bit) {
			dsi->irq_data &= ~irq_bit;
			return 0;
		}

	#ifndef MTK_DSI_IRQ_INTERRUPT_MODE
		mtk_dsi_irq(dsi);
	#endif

		usleep_range(1000, 1100);
	}

	dsi->irq_data = 0;

	return -1;
}

static void mtk_dsi_switch_to_cmd_mode(struct mtk_dsi *dsi)
{
	s32 ret = 0;

	mtk_dsi_set_cmd_mode(dsi);

	ret = mtk_dsi_wait_for_irq_timeout(dsi, DSI_INT_VM_DONE_FLAG, 500);
	if (0 != ret) {
		dev_info(dsi->dev, "dsi wait engine idle timeout\n");

		mtk_dsi_enable(dsi);
		mtk_dsi_reset_engine(dsi);
		mtk_dsi_dump_registers(dsi);
	}
}

static void mtk_dsi_poweroff(struct mtk_dsi *dsi)
{
	if (WARN_ON(dsi->refcount == 0))
		return;

	if (--dsi->refcount != 0)
		return;

	if (!dsi->poweron)
		return;

	mtk_dsi_switch_to_cmd_mode(dsi);

	if (dsi->panel) {
		if (drm_panel_unprepare(dsi->panel)) {
			DRM_ERROR("failed to prepare the panel\n");
			return;
		}

		if (drm_panel_disable(dsi->panel)) {
			DRM_ERROR("failed to disable the panel\n");
			return;
		}
	}

	mtk_dsi_reset_engine(dsi);
	mtk_dsi_lane0_ulp_mode_enter(dsi);
	mtk_dsi_clk_ulp_mode_enter(dsi);

	mtk_dsi_disable(dsi);

	clk_disable_unprepare(dsi->engine_clk);
	clk_disable_unprepare(dsi->digital_clk);

	phy_power_off(dsi->phy);

	dsi->poweron = false;
}

static void mtk_output_dsi_enable(struct mtk_dsi *dsi)
{
	int ret;

	if (dsi->enabled)
		return;

	if (dsi->panel) {
		if (drm_panel_enable(dsi->panel)) {
			DRM_ERROR("failed to enable the panel\n");
			return;
		}
	}

	ret = mtk_dsi_poweron(dsi);
	if (ret < 0) {
		DRM_ERROR("failed to power on dsi\n");
		return;
	}

	mtk_dsi_rxtx_control(dsi);
	mtk_dsi_phy_timconfig(dsi);
	mtk_dsi_ps_control_vact(dsi);
	mtk_dsi_set_vm_cmd(dsi);
	mtk_dsi_config_vdo_timing(dsi);

	mtk_dsi_enable(dsi);
	mtk_dsi_clk_ulp_mode_leave(dsi);
	mtk_dsi_lane0_ulp_mode_leave(dsi);
	mtk_dsi_clk_hs_mode(dsi, 0);

	if (dsi->panel) {
		if (drm_panel_prepare(dsi->panel)) {
			DRM_ERROR("failed to prepare the panel\n");
			return;
		}
	}

	mtk_dsi_set_interrupt_enable(dsi);
	mtk_dsi_set_mode(dsi);
	mtk_dsi_clk_hs_mode(dsi, 1);
	mtk_dsi_start(dsi);

	dsi->enabled = true;
}

static void mtk_output_dsi_disable(struct mtk_dsi *dsi)
{
	if (!dsi->enabled)
		return;

	mtk_dsi_stop(dsi);

	mtk_dsi_poweroff(dsi);

	dsi->enabled = false;
}

static void mtk_dsi_encoder_destroy(struct drm_encoder *encoder)
{
	drm_encoder_cleanup(encoder);
}

static const struct drm_encoder_funcs mtk_dsi_encoder_funcs = {
	.destroy = mtk_dsi_encoder_destroy,
};

static void mtk_dsi_encoder_dpms(struct drm_encoder *encoder, int mode)
{
	struct mtk_dsi *dsi = encoder_to_dsi(encoder);

	dev_info(dsi->dev, "mtk_dsi_encoder_dpms 0x%x\n", mode);

	switch (mode) {
	case DRM_MODE_DPMS_ON:
		mtk_output_dsi_enable(dsi);
		break;
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_OFF:
		mtk_output_dsi_disable(dsi);
		break;
	default:
		break;
	}
}

static bool mtk_dsi_encoder_mode_fixup(
	struct drm_encoder *encoder,
	const struct drm_display_mode *mode,
	struct drm_display_mode *adjusted_mode
	)
{
	return true;
}

static void mtk_dsi_encoder_prepare(struct drm_encoder *encoder)
{
	struct mtk_dsi *dsi = encoder_to_dsi(encoder);

	mtk_output_dsi_disable(dsi);
}

static void mtk_dsi_encoder_mode_set(
		struct drm_encoder *encoder,
		struct drm_display_mode *mode,
		struct drm_display_mode *adjusted
		)
{
	struct mtk_dsi *dsi = encoder_to_dsi(encoder);

	dsi->vm.pixelclock = adjusted->clock;
	dsi->vm.hactive = adjusted->hdisplay;
	dsi->vm.hback_porch = adjusted->htotal - adjusted->hsync_end;
	dsi->vm.hfront_porch = adjusted->hsync_start - adjusted->hdisplay;
	dsi->vm.hsync_len = adjusted->hsync_end - adjusted->hsync_start;

	dsi->vm.vactive = adjusted->vdisplay;
	dsi->vm.vback_porch = adjusted->vtotal - adjusted->vsync_end;
	dsi->vm.vfront_porch = adjusted->vsync_start - adjusted->vdisplay;
	dsi->vm.vsync_len = adjusted->vsync_end - adjusted->vsync_start;
}

static void mtk_dsi_encoder_disable(struct drm_encoder *encoder)
{
	struct mtk_dsi *dsi = encoder_to_dsi(encoder);

	mtk_output_dsi_disable(dsi);
}

static void mtk_dsi_encoder_enable(struct drm_encoder *encoder)
{
	struct mtk_dsi *dsi = encoder_to_dsi(encoder);

	mtk_output_dsi_enable(dsi);
}

static void mtk_dsi_encoder_commit(struct drm_encoder *encoder)
{
	struct mtk_dsi *dsi = encoder_to_dsi(encoder);

	mtk_output_dsi_enable(dsi);
}

static enum drm_connector_status
mtk_dsi_connector_detect(struct drm_connector *connector, bool force)
{
	struct mtk_dsi *dsi = connector_to_dsi(connector);

	if (!dsi->panel) {
		dsi->panel = of_drm_find_panel(dsi->panel_node);
		if (dsi->panel)
			drm_panel_attach(dsi->panel, &dsi->conn);
	} else if (!dsi->panel_node) {
		drm_panel_detach(dsi->panel);
		dsi->panel = NULL;
	}

	if (dsi->panel)
		return connector_status_connected;

	return connector_status_disconnected;
}

static void mtk_dsi_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static int mtk_dsi_connector_get_modes(struct drm_connector *connector)
{
	struct mtk_dsi *dsi = connector_to_dsi(connector);

	return drm_panel_get_modes(dsi->panel);
}

static struct drm_encoder
*mtk_dsi_connector_best_encoder(struct drm_connector *connector)
{
	struct mtk_dsi *dsi = connector_to_dsi(connector);

	return &dsi->encoder;
}

static const struct drm_encoder_helper_funcs mtk_dsi_encoder_helper_funcs = {
	.dpms = mtk_dsi_encoder_dpms,
	.mode_fixup = mtk_dsi_encoder_mode_fixup,
	.prepare = mtk_dsi_encoder_prepare,
	.mode_set = mtk_dsi_encoder_mode_set,
	.commit = mtk_dsi_encoder_commit,
	.disable = mtk_dsi_encoder_disable,
	.enable = mtk_dsi_encoder_enable,
};

static const struct drm_connector_funcs mtk_dsi_connector_funcs = {
	.dpms = drm_atomic_helper_connector_dpms,
	.detect = mtk_dsi_connector_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = mtk_dsi_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static const struct drm_connector_helper_funcs
	mtk_dsi_connector_helper_funcs = {
	.get_modes = mtk_dsi_connector_get_modes,
	.best_encoder = mtk_dsi_connector_best_encoder,
};

static int mtk_drm_attach_lcm_bridge(struct drm_bridge *bridge,
				     struct drm_encoder *encoder)
{
	int ret;

	encoder->bridge = bridge;
	bridge->encoder = encoder;
	ret = drm_bridge_attach(encoder->dev, bridge);
	if (ret) {
		DRM_ERROR("Failed to attach bridge to drm\n");
		return ret;
	}

	return 0;
}

static int mtk_dsi_create_conn_enc(struct drm_device *drm, struct mtk_dsi *dsi)
{
	int ret;

	ret = drm_encoder_init(drm, &dsi->encoder, &mtk_dsi_encoder_funcs,
			       DRM_MODE_ENCODER_DSI);
	if (ret) {
		DRM_ERROR("Failed to encoder init to drm\n");
		return ret;
	}

	drm_encoder_helper_add(&dsi->encoder, &mtk_dsi_encoder_helper_funcs);

	dsi->encoder.possible_crtcs = 1;

	/* Pre-empt DP connector creation if there's a bridge */
	if (dsi->bridge) {
		ret = mtk_drm_attach_lcm_bridge(dsi->bridge, &dsi->encoder);
		if (!ret)
			return 0;
	}

	ret = drm_connector_init(drm, &dsi->conn, &mtk_dsi_connector_funcs,
				 DRM_MODE_CONNECTOR_DSI);
	if (ret) {
		DRM_ERROR("Failed to connector init to drm\n");
		goto errconnector;
	}

	drm_connector_helper_add(&dsi->conn, &mtk_dsi_connector_helper_funcs);

	ret = drm_connector_register(&dsi->conn);
	if (ret) {
		DRM_ERROR("Failed to connector register to drm\n");
		goto errconnectorreg;
	}

	dsi->conn.dpms = DRM_MODE_DPMS_OFF;
	drm_mode_connector_attach_encoder(&dsi->conn, &dsi->encoder);

	if (dsi->panel) {
		ret = drm_panel_attach(dsi->panel, &dsi->conn);
		if (ret) {
			DRM_ERROR("Failed to attact panel to drm\n");
			return ret;
		}
	}

	mtk_dsi_reset_all(dsi);

	return 0;

errconnector:
	drm_encoder_cleanup(&dsi->encoder);
errconnectorreg:
	drm_connector_cleanup(&dsi->conn);

	return ret;
}

static void mtk_dsi_destroy_conn_enc(struct mtk_dsi *dsi)
{
	drm_encoder_cleanup(&dsi->encoder);
	drm_connector_unregister(&dsi->conn);
	drm_connector_cleanup(&dsi->conn);
}

static int mtk_dsi_host_attach(struct mipi_dsi_host *host,
			       struct mipi_dsi_device *device)
{
	struct mtk_dsi *dsi = host_to_dsi(host);

	dsi->lanes = device->lanes;
	dsi->format = device->format;
	dsi->mode_flags = device->mode_flags;
	dsi->panel_node = device->dev.of_node;

	if (dsi->conn.dev)
		drm_helper_hpd_irq_event(dsi->conn.dev);

	return 0;
}

static int mtk_dsi_host_detach(struct mipi_dsi_host *host,
			       struct mipi_dsi_device *device)
{
	struct mtk_dsi *dsi = host_to_dsi(host);

	dsi->panel_node = NULL;

	if (dsi->conn.dev)
		drm_helper_hpd_irq_event(dsi->conn.dev);

	return 0;
}

static ssize_t mtk_dsi_host_read_cmd(struct mtk_dsi *dsi,
				     const struct mipi_dsi_msg *msg)
{
	u8 max_try_count = 5;
	u32 recv_data_cnt, tmp_val, recv_data0, recv_data1, recv_data2, recv_data3;
	struct dsi_rx_data read_data0, read_data1, read_data2, read_data3;
	struct dsi_cmd_t0 t0;
	s32 ret;

	u8 *buffer = msg->rx_buf;
	u8 buffer_size = msg->rx_len;
	u8 packet_type;

	if (readl(dsi->regs + DSI_MODE_CTRL) & 0x03) {
		dev_info(dsi->dev, "dsi engine is not command mode\n");
		return -1;
	}

	if (buffer == NULL || buffer_size == 0) {
		dev_info(dsi->dev, "dsi receive buffer size may be NULL\n");
		return -1;
	}

	do {
		if (max_try_count == 0) {
			dev_info(dsi->dev, "dsi engine read counter has been maxinum\n");
			return -1;
		}

		max_try_count--;
		recv_data_cnt = 0;

		mtk_dsi_wait_for_idle(dsi);

		t0.config = 0x04;
		t0.data0 = *((u8 *)(msg->tx_buf));

		if (buffer_size < 0x3)
			t0.type = MIPI_DSI_DCS_READ;
		else
			t0.type = MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM;

		t0.data1 = 0;

		tmp_val = (t0.data1 << 24) | (t0.data0 << 16) | (t0.type << 8) | t0.config;

		writel(tmp_val, dsi->regs + DSI_CMDQ0);
		mtk_dsi_mask(dsi, DSI_CMDQ_SIZE, CMDQ_SIZE, 1);

		mtk_dsi_mask(dsi, DSI_RACK, BIT(0), BIT(0));
		mtk_dsi_mask(dsi, DSI_INTSTA, BIT(0), ~(BIT(0)));
		mtk_dsi_mask(dsi, DSI_INTSTA, BIT(1), ~(BIT(1)));
		mtk_dsi_mask(dsi, DSI_INTEN, BIT(0), BIT(0));
		mtk_dsi_mask(dsi, DSI_INTEN, BIT(1), BIT(1));

		mtk_dsi_start(dsi);

		dev_info(dsi->dev, "Start polling DSI read ready!!!\n");

		 /* 2s timeout*/
		ret = mtk_dsi_wait_for_irq_timeout(dsi, DSI_INT_LPRX_RD_RDY_FLAG, 2000);
		if (ret) {
			dev_info(dsi->dev, "Polling DSI read ready timeout!!!\n");

			mtk_dsi_enable(dsi);
			mtk_dsi_reset_engine(dsi);
			mtk_dsi_dump_registers(dsi);

			return ret;
		}

		dev_info(dsi->dev, "End polling DSI read ready!!!\n");

		mtk_dsi_mask(dsi, DSI_RACK, BIT(0), BIT(0));
		mtk_dsi_mask(dsi, DSI_INTSTA, BIT(0), ~(BIT(0)));

		recv_data0 = readl(dsi->regs + DSI_RX_DATA0);
		recv_data1 = readl(dsi->regs + DSI_RX_DATA1);
		recv_data2 = readl(dsi->regs + DSI_RX_DATA2);
		recv_data3 = readl(dsi->regs + DSI_RX_DATA3);

		read_data0 = *((struct dsi_rx_data *)(&recv_data0));
		read_data1 = *((struct dsi_rx_data *)(&recv_data1));
		read_data2 = *((struct dsi_rx_data *)(&recv_data2));
		read_data3 = *((struct dsi_rx_data *)(&recv_data3));

		ret = readl(dsi->regs + DSI_CMDQ_SIZE);
		dev_info(dsi->dev, "DSI_CMDQ_SIZE : 0x%x\n", ret & CMDQ_SIZE);

		ret = readl(dsi->regs + DSI_CMDQ0);
		dev_info(dsi->dev, "DSI_CMDQ_DATA0 : 0x%x\n", ret & 0xff);
		dev_info(dsi->dev, "DSI_CMDQ_DATA1 : 0x%x\n", (ret >> 8) & 0xff);
		dev_info(dsi->dev, "DSI_CMDQ_DATA2 : 0x%x\n", (ret >> 16) & 0xff);
		dev_info(dsi->dev, "DSI_CMDQ_DATA3 : 0x%x\n", (ret >> 24) & 0xff);

		dev_info(dsi->dev, "DSI_RX_DATA0: 0x%x\n", recv_data0);
		dev_info(dsi->dev, "DSI_RX_DATA1: 0x%x\n", recv_data1);
		dev_info(dsi->dev, "DSI_RX_DATA2: 0x%x\n", recv_data2);
		dev_info(dsi->dev, "DSI_RX_DATA3: 0x%x\n", recv_data3);

		dev_info(dsi->dev, "read_data0: %x,%x,%x,%x\n",
			read_data0.byte0, read_data0.byte1, read_data0.byte2, read_data0.byte3);
		dev_info(dsi->dev, "read_data1: %x,%x,%x,%x\n",
			read_data1.byte0, read_data1.byte1, read_data1.byte2, read_data1.byte3);
		dev_info(dsi->dev, "read_data2: %x,%x,%x,%x\n",
			read_data2.byte0, read_data2.byte1, read_data2.byte2, read_data2.byte3);
		dev_info(dsi->dev, "read_data3: %x,%x,%x,%x\n",
			read_data3.byte0, read_data3.byte1, read_data3.byte2, read_data3.byte3);

		packet_type = read_data0.byte0;
		dev_info(dsi->dev, "DSI read packet_type is 0x%x\n", packet_type);

		if (packet_type == 0x1A || packet_type == 0x1C) {
			void *read_tmp = (void *)&recv_data1;

			recv_data_cnt = read_data0.byte1 + read_data0.byte2 * 16;
			if (recv_data_cnt > 10)
				recv_data_cnt = 10;

			if (recv_data_cnt > buffer_size)
				recv_data_cnt = buffer_size;

			memcpy((void *)buffer, read_tmp, recv_data_cnt);
		} else {
			/* short  packet */
			recv_data_cnt = 2;
			if (recv_data_cnt > buffer_size)
				recv_data_cnt = buffer_size;

			memcpy((void *)buffer, (void *)&read_data0.byte1, 2);
		}
	} while (packet_type != 0x1C && packet_type != 0x21 &&
		packet_type != 0x22 && packet_type != 0x1A);

	dev_info(dsi->dev, "dsi get %d byte data from the panel address(0x%x)\n",
		 recv_data_cnt, *((u8 *)(msg->tx_buf)));

	return recv_data_cnt;
}

static ssize_t mtk_dsi_host_write_cmd(struct mtk_dsi *dsi,
				     const struct mipi_dsi_msg *msg)
{
	u32 i;
	u32 goto_addr, mask_para, set_para, reg_val;
	struct dsi_cmd_t0 t0;
	struct dsi_cmd_t2 t2;
	const char *tx_buf = msg->tx_buf;
	struct dsi_tx_cmdq_regs *dsi_cmd_reg;

	dsi_cmd_reg = (struct dsi_tx_cmdq_regs *)(dsi->regs + DSI_CMDQ0);

	mtk_dsi_wait_for_idle(dsi);

	if (msg->tx_len > 2) {
		t2.config = 2;
		t2.type = msg->type;
		t2.wc16 = msg->tx_len;

		reg_val = (t2.wc16 << 16) | (t2.type << 8) | t2.config;

		writel(reg_val, &dsi_cmd_reg->data[0]);

		goto_addr = (u32)(&dsi_cmd_reg->data[1].byte0);
		mask_para = (0xff << ((goto_addr & 0x3) * 8));
		set_para = (tx_buf[0] << ((goto_addr & 0x3) * 8));
		mtk_dsi_mask(dsi, goto_addr & (~0x3), mask_para,
				 set_para);

		for (i = 0; i < msg->tx_len - 1; i++) {
			goto_addr = (u32)(&dsi_cmd_reg->data[1].byte1) + i;
			mask_para = (0xff << ((goto_addr & 0x3) * 8));
			set_para = (tx_buf[i] << ((goto_addr & 0x3) * 8));
			mtk_dsi_mask(dsi, goto_addr & (~0x3), mask_para,
				     set_para);
		}

		mtk_dsi_mask(dsi, DSI_CMDQ_SIZE, CMDQ_SIZE,
			     2 + (msg->tx_len - 1) / 4);
	} else {
		t0.config = 0;
		t0.data0 = tx_buf[0];
		if (msg->tx_len == 2) {
			t0.type = MIPI_DSI_DCS_SHORT_WRITE_PARAM;
			t0.data1 = tx_buf[1];
		} else {
			t0.type = MIPI_DSI_DCS_SHORT_WRITE;
			t0.data1 = 0;
		}

		reg_val = (t0.data1 << 24) | (t0.data0 << 16) | (t0.type << 8) |
			   t0.config;

		writel(reg_val, &dsi_cmd_reg->data[0]);
		mtk_dsi_mask(dsi, DSI_CMDQ_SIZE, CMDQ_SIZE, 1);
	}

	mtk_dsi_start(dsi);
	mtk_dsi_wait_for_idle(dsi);

	return 0;
}

static ssize_t mtk_dsi_host_transfer(struct mipi_dsi_host *host,
				     const struct mipi_dsi_msg *msg)
{
	struct mtk_dsi *dsi = host_to_dsi(host);
	u8 type = msg->type;
	ssize_t ret = 0;

	if (MTK_DSI_HOST_IS_READ(type))
		ret = mtk_dsi_host_read_cmd(dsi, msg);
	else if (MTK_DSI_HOST_IS_WRITE(type))
		ret = mtk_dsi_host_write_cmd(dsi, msg);

	return ret;
}

static const struct mipi_dsi_host_ops mtk_dsi_ops = {
	.attach = mtk_dsi_host_attach,
	.detach = mtk_dsi_host_detach,
	.transfer = mtk_dsi_host_transfer,
};

static void mtk_dsi_ddp_start(struct mtk_ddp_comp *comp)
{
	struct mtk_dsi *dsi = container_of(comp, struct mtk_dsi, ddp_comp);

	mtk_dsi_poweron(dsi);
}

static void mtk_dsi_ddp_stop(struct mtk_ddp_comp *comp)
{
	struct mtk_dsi *dsi = container_of(comp, struct mtk_dsi, ddp_comp);

	mtk_dsi_poweroff(dsi);
}

static const struct mtk_ddp_comp_funcs mtk_dsi_funcs = {
	.start = mtk_dsi_ddp_start,
	.stop = mtk_dsi_ddp_stop,
};

static int mtk_dsi_bind(struct device *dev, struct device *master, void *data)
{
	int ret;
	struct drm_device *drm = data;
	struct mtk_dsi *dsi = dev_get_drvdata(dev);

	ret = mtk_ddp_comp_register(drm, &dsi->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	ret = mtk_dsi_create_conn_enc(drm, dsi);
	if (ret) {
		DRM_ERROR("Encoder create failed with %d\n", ret);
		goto err_unregister;
	}

	return 0;

err_unregister:
	mtk_ddp_comp_unregister(drm, &dsi->ddp_comp);
	return ret;
}

static void mtk_dsi_unbind(struct device *dev, struct device *master,
			   void *data)
{
	struct drm_device *drm = data;
	struct mtk_dsi *dsi;

	dsi = platform_get_drvdata(to_platform_device(dev));
	mtk_dsi_destroy_conn_enc(dsi);
	mipi_dsi_host_unregister(&dsi->host);
	mtk_ddp_comp_unregister(drm, &dsi->ddp_comp);
}

static const struct component_ops mtk_dsi_component_ops = {
	.bind = mtk_dsi_bind,
	.unbind = mtk_dsi_unbind,
};

static int mtk_dsi_probe(struct platform_device *pdev)
{
	struct mtk_dsi *dsi;
	struct device *dev = &pdev->dev;
	struct device_node *remote_node, *endpoint;
	struct resource *regs;
	int comp_id;
	int ret;
	struct regmap *regmap;
	unsigned int ssc_range;

	dsi = devm_kzalloc(dev, sizeof(*dsi), GFP_KERNEL);
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->lanes = 4;
	dsi->dev = dev;

	dsi->engine_clk = devm_clk_get(dev, "engine");
	if (IS_ERR(dsi->engine_clk)) {
		ret = PTR_ERR(dsi->engine_clk);
		dev_err(dev, "Failed to get engine clock: %d\n", ret);
		return ret;
	}

	dsi->digital_clk = devm_clk_get(dev, "digital");
	if (IS_ERR(dsi->digital_clk)) {
		ret = PTR_ERR(dsi->digital_clk);
		dev_err(dev, "Failed to get digital clock: %d\n", ret);
		return ret;
	}

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dsi->regs = devm_ioremap_resource(dev, regs);
	if (IS_ERR(dsi->regs)) {
		ret = PTR_ERR(dsi->regs);
		dev_err(dev, "Failed to ioremap memory: %d\n", ret);
		return ret;
	}

	dsi->phy = devm_phy_get(dev, "dphy");
	if (IS_ERR(dsi->phy)) {
		ret = PTR_ERR(dsi->phy);
		dev_err(dev, "Failed to get MIPI-DPHY: %d\n", ret);
		return ret;
	}

	regmap = syscon_regmap_lookup_by_phandle(dev->of_node, "mediatek,syscon-dsi");
	ret = of_property_read_u32_index(dev->of_node, "mediatek,syscon-dsi", 1, &dsi->sw_rst_b);

	if (IS_ERR(regmap))
		ret = PTR_ERR(regmap);
	if (ret) {
		ret = PTR_ERR(regmap);
		dev_err(dev, "Failed to get system configuration registers: %d\n", ret);
		return ret;
	}

	dsi->mmsys_sw_rst_b = regmap;

	ret = of_property_read_u32(dev->of_node, "mediatek,ssc-range", &ssc_range);
	dsi->ssc_data = ssc_range;
	dev_info(dev, "dsi ssc is %s, the amplitude is 0x%x\n",
		 (0 == ssc_range)?"disable":"enable", ssc_range);

	endpoint = of_graph_get_next_endpoint(dev->of_node, NULL);
	if (endpoint) {
		remote_node = of_graph_get_remote_port_parent(endpoint);
		of_node_put(endpoint);

		if (!remote_node) {
			dev_err(dev, "No panel connected\n");
			return -ENODEV;
		}

		dsi->device_node = remote_node;
		of_node_put(remote_node);
	}

	dsi->host.ops = &mtk_dsi_ops;
	dsi->host.dev = dev;
	mipi_dsi_host_register(&dsi->host);

	dsi->bridge = of_drm_find_bridge(dsi->device_node);
	dsi->panel = of_drm_find_panel(dsi->device_node);

	dev_info(dev, "dsi connector detect panel = 0x%p, bridge = 0x%p\n",
		 dsi->panel, dsi->bridge);

	if (!dsi->bridge && !dsi->panel) {
		dev_info(dev, "Waiting for bridge or panel driver\n");
		return -EPROBE_DEFER;
	}

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DSI);
	if (comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &dsi->ddp_comp, comp_id,
				&mtk_dsi_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	#ifdef MTK_DSI_IRQ_INTERRUPT_MODE
	dsi->irq_num = platform_get_irq(pdev, 0);
	if (dsi->irq_num < 0) {
		dev_err(&pdev->dev, "failed to request dsi irq resource\n");
		ret = dsi->irq_num;
		return -EPROBE_DEFER;
	}

	irq_set_status_flags(dsi->irq_num, IRQ_TYPE_LEVEL_LOW);
	ret = devm_request_irq(&pdev->dev, dsi->irq_num, mtk_dsi_irq,
			       IRQF_TRIGGER_LOW, dev_name(&pdev->dev), dsi);
	if (ret) {
		dev_err(&pdev->dev, "failed to request mediatek dsi irq\n");
		return -EPROBE_DEFER;
	}
	#endif

	dsi->irq_data = 0;
	dev_info(dev, "dsi irq num is 0x%x\n", dsi->irq_num);

	platform_set_drvdata(pdev, dsi);

	ret = component_add(&pdev->dev, &mtk_dsi_component_ops);
	if (ret) {
		dev_err(dev, "Failed to add DSI component\n");
		return -EPROBE_DEFER;
	}

	ret = mtk_drm_dsi_debugfs_init(dsi);
	if (ret) {
		dev_err(dev, "Failed to initialize dsi debugfs\n");
		return ret;
	}

	return 0;
}

static int mtk_dsi_remove(struct platform_device *pdev)
{
	struct mtk_dsi *dsi = platform_get_drvdata(pdev);

	mtk_output_dsi_disable(dsi);
	component_del(&pdev->dev, &mtk_dsi_component_ops);
	mtk_drm_dsi_debugfs_exit(dsi);

	return 0;
}

#ifdef CONFIG_PM
static int mtk_dsi_suspend(struct device *dev)
{
	struct mtk_dsi *dsi;

	dsi = dev_get_drvdata(dev);

	mtk_output_dsi_disable(dsi);
	dev_info(dsi->dev, "dsi suspend success!\n");

	return 0;
}

static int mtk_dsi_resume(struct device *dev)
{
	struct mtk_dsi *dsi;

	dsi = dev_get_drvdata(dev);

	mtk_output_dsi_enable(dsi);
	dev_info(dsi->dev, "dsi resume success!\n");

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(mtk_dsi_pm_ops, mtk_dsi_suspend, mtk_dsi_resume);

static const struct of_device_id mtk_dsi_of_match[] = {
	{ .compatible = "mediatek,mt2701-dsi" },
	{ .compatible = "mediatek,mt8173-dsi" },
	{ },
};

struct platform_driver mtk_dsi_driver = {
	.probe = mtk_dsi_probe,
	.remove = mtk_dsi_remove,
	.driver = {
		.name = "mtk-dsi",
		.of_match_table = mtk_dsi_of_match,
		.pm = &mtk_dsi_pm_ops,
	},
};
