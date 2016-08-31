/*
 * drivers/video/tegra/dc/dsi.c
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION, All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/moduleparam.h>
#include <linux/export.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/nvhost.h>
#include <linux/lcm.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/clk/tegra.h>
#include <linux/tegra-soc.h>
#include <linux/nvhost.h>
#include <linux/of_address.h>

#include <mach/dc.h>
#include <mach/fb.h>
#include <mach/csi.h>

#include "dc_reg.h"
#include "dc_priv.h"
#include "dev.h"
#include "dsi_regs.h"
#include "dsi.h"
#include "mipi_cal.h"

/* HACK! This needs to come from DT */
#include "../../../../arch/arm/mach-tegra/iomap.h"

#define APB_MISC_GP_MIPI_PAD_CTRL_0	(TEGRA_APB_MISC_BASE + 0x820)
#define DSIB_MODE_ENABLE		0x2

/* Only enabling for T124 for now; causes issues for other chips */
#ifdef CONFIG_ARCH_TEGRA_12x_SOC
#define DSI_USE_SYNC_POINTS 1
#else
#define DSI_USE_SYNC_POINTS 0
#endif

#define S_TO_MS(x)			(1000 * (x))
#define MS_TO_US(x)			(1000 * (x))

#define DSI_MODULE_NOT_INIT		0x0
#define DSI_MODULE_INIT			0x1

#define DSI_LPHS_NOT_INIT		0x0
#define DSI_LPHS_IN_LP_MODE		0x1
#define DSI_LPHS_IN_HS_MODE		0x2

#define DSI_VIDEO_TYPE_NOT_INIT		0x0
#define DSI_VIDEO_TYPE_VIDEO_MODE	0x1
#define DSI_VIDEO_TYPE_CMD_MODE		0x2

#define DSI_DRIVEN_MODE_NOT_INIT	0x0
#define DSI_DRIVEN_MODE_DC		0x1
#define DSI_DRIVEN_MODE_HOST		0x2

#define DSI_PHYCLK_OUT_DIS		0x0
#define DSI_PHYCLK_OUT_EN		0x1

#define DSI_PHYCLK_NOT_INIT		0x0
#define DSI_PHYCLK_CONTINUOUS		0x1
#define DSI_PHYCLK_TX_ONLY		0x2

#define DSI_CLK_BURST_NOT_INIT		0x0
#define DSI_CLK_BURST_NONE_BURST	0x1
#define DSI_CLK_BURST_BURST_MODE	0x2

#define DSI_DC_STREAM_DISABLE		0x0
#define DSI_DC_STREAM_ENABLE		0x1

#define DSI_LP_OP_NOT_INIT		0x0
#define DSI_LP_OP_WRITE			0x1
#define DSI_LP_OP_READ			0x2

#define DSI_HOST_IDLE_PERIOD		1000
static atomic_t dsi_syncpt_rst = ATOMIC_INIT(0);

static bool enable_read_debug;
module_param(enable_read_debug, bool, 0644);
MODULE_PARM_DESC(enable_read_debug,
		"Enable to print read fifo and return packet type");

bool tegra_dsi_enable_read_debug(struct tegra_dc_dsi_data *dsi)
{
	enable_read_debug = true;
	return enable_read_debug;
}

bool tegra_dsi_disable_read_debug(struct tegra_dc_dsi_data *dsi)
{
	enable_read_debug = false;
	return enable_read_debug;
}

/* source of video data */
enum {
	TEGRA_DSI_DRIVEN_BY_DC,
	TEGRA_DSI_DRIVEN_BY_HOST,
};

const u32 dsi_pkt_seq_reg[NUMOF_PKT_SEQ] = {
	DSI_PKT_SEQ_0_LO,
	DSI_PKT_SEQ_0_HI,
	DSI_PKT_SEQ_1_LO,
	DSI_PKT_SEQ_1_HI,
	DSI_PKT_SEQ_2_LO,
	DSI_PKT_SEQ_2_HI,
	DSI_PKT_SEQ_3_LO,
	DSI_PKT_SEQ_3_HI,
	DSI_PKT_SEQ_4_LO,
	DSI_PKT_SEQ_4_HI,
	DSI_PKT_SEQ_5_LO,
	DSI_PKT_SEQ_5_HI,
};

const u32 dsi_pkt_seq_video_non_burst_syne[NUMOF_PKT_SEQ] = {
	PKT_ID0(CMD_VS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(7) | PKT_LP,
	0,
	PKT_ID0(CMD_VE) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(7) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(7) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_BLNK) | PKT_LEN1(1) |
	PKT_ID2(CMD_HE) | PKT_LEN2(0),
	PKT_ID3(CMD_BLNK) | PKT_LEN3(2) | PKT_ID4(CMD_RGB) | PKT_LEN4(3) |
	PKT_ID5(CMD_BLNK) | PKT_LEN5(4),
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(7) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_BLNK) | PKT_LEN1(1) |
	PKT_ID2(CMD_HE) | PKT_LEN2(0),
	PKT_ID3(CMD_BLNK) | PKT_LEN3(2) | PKT_ID4(CMD_RGB) | PKT_LEN4(3) |
	PKT_ID5(CMD_BLNK) | PKT_LEN5(4),
};

const u32 dsi_pkt_seq_video_non_burst[NUMOF_PKT_SEQ] = {
	PKT_ID0(CMD_VS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(7) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(7) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(7) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_BLNK) | PKT_LEN1(2) |
	PKT_ID2(CMD_RGB) | PKT_LEN2(3),
	PKT_ID3(CMD_BLNK) | PKT_LEN3(4),
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(7) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_BLNK) | PKT_LEN1(2) |
	PKT_ID2(CMD_RGB) | PKT_LEN2(3),
	PKT_ID3(CMD_BLNK) | PKT_LEN3(4),
};

const u32 dsi_pkt_seq_video_non_burst_no_eot_no_lp_no_hbp[NUMOF_PKT_SEQ] = {
	PKT_ID0(CMD_VS) | PKT_LEN0(0),
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0),
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0),
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_RGB) | PKT_LEN1(3) |
	PKT_ID2(CMD_BLNK) | PKT_LEN2(4),
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0),
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_RGB) | PKT_LEN1(3) |
	PKT_ID2(CMD_BLNK) | PKT_LEN2(4),
	0,
};

static const u32 dsi_pkt_seq_video_burst[NUMOF_PKT_SEQ] = {
	PKT_ID0(CMD_VS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(7) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(7) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(7) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_BLNK) | PKT_LEN1(2)|
	PKT_ID2(CMD_RGB) | PKT_LEN2(3) | PKT_LP,
	PKT_ID0(CMD_EOT) | PKT_LEN0(7),
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(7) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_BLNK) | PKT_LEN1(2)|
	PKT_ID2(CMD_RGB) | PKT_LEN2(3) | PKT_LP,
	PKT_ID0(CMD_EOT) | PKT_LEN0(7),
};

static const u32 dsi_pkt_seq_video_burst_no_eot[NUMOF_PKT_SEQ] = {
	PKT_ID0(CMD_VS) | PKT_LEN0(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_BLNK) | PKT_LEN1(2)|
	PKT_ID2(CMD_RGB) | PKT_LEN2(3) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_BLNK) | PKT_LEN1(2)|
	PKT_ID2(CMD_RGB) | PKT_LEN2(3) | PKT_LP,
	0,
};

const u32 dsi_pkt_seq_video_non_burst_no_eot[NUMOF_PKT_SEQ] = {
	PKT_ID0(CMD_VS) | PKT_LEN0(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_BLNK) | PKT_LEN1(2) |
		PKT_ID2(CMD_RGB) | PKT_LEN2(3),
	PKT_ID3(CMD_BLNK) | PKT_LEN3(4),
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_BLNK) | PKT_LEN1(2) |
		PKT_ID2(CMD_RGB) | PKT_LEN2(3),
	PKT_ID3(CMD_BLNK) | PKT_LEN3(4),
};

const u32 dsi_pkt_seq_cmd_mode[NUMOF_PKT_SEQ] = {
	0,
	0,
	0,
	0,
	0,
	0,
	PKT_ID0(CMD_LONGW) | PKT_LEN0(3) | PKT_ID1(CMD_EOT) |
		PKT_LEN1(7) | PKT_LP,
	0,
	0,
	0,
	PKT_ID0(CMD_LONGW) | PKT_LEN0(3) | PKT_ID1(CMD_EOT) |
		PKT_LEN1(7) | PKT_LP,
	0,
};

const u32 init_reg[] = {
	DSI_INT_ENABLE,
	DSI_INT_STATUS,
	DSI_INT_MASK,
	DSI_INIT_SEQ_DATA_0,
	DSI_INIT_SEQ_DATA_1,
	DSI_INIT_SEQ_DATA_2,
	DSI_INIT_SEQ_DATA_3,
	DSI_INIT_SEQ_DATA_4,
	DSI_INIT_SEQ_DATA_5,
	DSI_INIT_SEQ_DATA_6,
	DSI_INIT_SEQ_DATA_7,
	DSI_DCS_CMDS,
	DSI_PKT_SEQ_0_LO,
	DSI_PKT_SEQ_1_LO,
	DSI_PKT_SEQ_2_LO,
	DSI_PKT_SEQ_3_LO,
	DSI_PKT_SEQ_4_LO,
	DSI_PKT_SEQ_5_LO,
	DSI_PKT_SEQ_0_HI,
	DSI_PKT_SEQ_1_HI,
	DSI_PKT_SEQ_2_HI,
	DSI_PKT_SEQ_3_HI,
	DSI_PKT_SEQ_4_HI,
	DSI_PKT_SEQ_5_HI,
	DSI_CONTROL,
	DSI_HOST_DSI_CONTROL,
	DSI_PAD_CONTROL,
	DSI_PAD_CONTROL_CD,
	DSI_SOL_DELAY,
	DSI_MAX_THRESHOLD,
	DSI_TRIGGER,
	DSI_TX_CRC,
	DSI_INIT_SEQ_CONTROL,
	DSI_PKT_LEN_0_1,
	DSI_PKT_LEN_2_3,
	DSI_PKT_LEN_4_5,
	DSI_PKT_LEN_6_7,
};

const u32 init_reg_vs1_ext[] = {
	DSI_PAD_CONTROL_0_VS1,
	DSI_PAD_CONTROL_CD_VS1,
	DSI_PAD_CD_STATUS_VS1,
	DSI_PAD_CONTROL_1_VS1,
	DSI_PAD_CONTROL_2_VS1,
	DSI_PAD_CONTROL_3_VS1,
	DSI_PAD_CONTROL_4_VS1,
	DSI_GANGED_MODE_CONTROL,
	DSI_GANGED_MODE_START,
	DSI_GANGED_MODE_SIZE,
};

static int tegra_dsi_host_suspend(struct tegra_dc *dc);
static int tegra_dsi_host_resume(struct tegra_dc *dc);
static void tegra_dc_dsi_idle_work(struct work_struct *work);
static void tegra_dsi_send_dc_frames(struct tegra_dc *dc,
				     struct tegra_dc_dsi_data *dsi,
				     int no_of_frames);

unsigned long tegra_dsi_controller_readl(struct tegra_dc_dsi_data *dsi,
							u32 reg, int index)
{
	unsigned long ret;

	BUG_ON(!nvhost_module_powered_ext(dsi->dc->ndev));
	ret = readl(dsi->base[index] + reg * 4);
	trace_display_readl(dsi->dc, ret, dsi->base[index] + reg * 4);
	return ret;
}
EXPORT_SYMBOL(tegra_dsi_controller_readl);

void tegra_dsi_controller_writel(struct tegra_dc_dsi_data *dsi,
						u32 val, u32 reg, int index)
{
	BUG_ON(!nvhost_module_powered_ext(dsi->dc->ndev));
	trace_display_writel(dsi->dc, val, dsi->base[index] + reg * 4);
	writel(val, dsi->base[index] + reg * 4);
}
EXPORT_SYMBOL(tegra_dsi_controller_writel);

unsigned long tegra_dsi_readl(struct tegra_dc_dsi_data *dsi, u32 reg)
{
	unsigned long ret;
	BUG_ON(!nvhost_module_powered_ext(dsi->dc->ndev));
	ret = readl(dsi->base[DSI_INSTANCE_0] + reg * 4);
	trace_display_readl(dsi->dc, ret, dsi->base[DSI_INSTANCE_0] + reg * 4);
	return ret;
}
EXPORT_SYMBOL(tegra_dsi_readl);

void tegra_dsi_writel(struct tegra_dc_dsi_data *dsi, u32 val, u32 reg)
{
	int  i = 0;
	BUG_ON(!nvhost_module_powered_ext(dsi->dc->ndev));
	for (i = 0; i < dsi->max_instances; i++) {
		trace_display_writel(dsi->dc, val, dsi->base[i] + reg * 4);
		writel(val, dsi->base[i] + reg * 4);
	}
}
EXPORT_SYMBOL(tegra_dsi_writel);

inline void tegra_dsi_reset_deassert(struct tegra_dc_dsi_data *dsi)
{
	int i = 0;
	for (i = 0; i < dsi->max_instances; i++)
		tegra_periph_reset_deassert(dsi->dsi_clk[i]);
}

inline void tegra_dsi_reset_assert(struct tegra_dc_dsi_data *dsi)
{
	int i = 0;
	for (i = 0; i < dsi->max_instances; i++)
		tegra_periph_reset_assert(dsi->dsi_clk[i]);
}

static inline void tegra_dsi_lp_clk_enable(struct tegra_dc_dsi_data *dsi);
static inline void tegra_dsi_lp_clk_disable(struct tegra_dc_dsi_data *dsi);

void tegra_dsi_clk_enable(struct tegra_dc_dsi_data *dsi)
{
	int i = 0;
	for (i = 0; i < dsi->max_instances; i++) {
		clk_prepare_enable(dsi->dsi_clk[i]);
		udelay(800);
	}
}

void tegra_dsi_clk_disable(struct tegra_dc_dsi_data *dsi)
{
	int i = 0;
	for (i = 0; i < dsi->max_instances; i++) {
		clk_disable_unprepare(dsi->dsi_clk[i]);
		udelay(800);
	}
}

static inline void tegra_dsi_lp_clk_enable(struct tegra_dc_dsi_data *dsi)
{
	int i = 0;
	for (i = 0; i < dsi->max_instances; i++) {
		clk_prepare_enable(dsi->dsi_lp_clk[i]);
		udelay(800);
	}
}

static inline void tegra_dsi_lp_clk_disable(struct tegra_dc_dsi_data *dsi)
{
	int i = 0;
	for (i = 0; i < dsi->max_instances; i++) {
		clk_disable_unprepare(dsi->dsi_lp_clk[i]);
		udelay(800);
	}
}

static void tegra_dsi_setup_clk(struct tegra_dc *dc,
	struct tegra_dc_dsi_data *dsi)
{
	int i = 0;
	for (i = 0; i < dsi->max_instances; i++) {
		tegra_dc_setup_clk(dc, dsi->dsi_clk[i]);
		mdelay(3);
	}
}

static void __maybe_unused tegra_dsi_syncpt_reset(
				struct tegra_dc_dsi_data *dsi)
{
	tegra_dsi_writel(dsi, 0x1, DSI_INCR_SYNCPT_CNTRL);
	/* stabilization delay */
	udelay(300);
	tegra_dsi_writel(dsi, 0x0, DSI_INCR_SYNCPT_CNTRL);
	/* stabilization delay */
	udelay(300);
}

static int __maybe_unused tegra_dsi_syncpt
	(struct tegra_dc_dsi_data *dsi, u8 link_id)
{
	u32 val;
	int ret = 0;

	dsi->syncpt_val = nvhost_syncpt_read_ext(dsi->dc->ndev, dsi->syncpt_id);

	val = DSI_INCR_SYNCPT_COND(OP_DONE) |
		DSI_INCR_SYNCPT_INDX(dsi->syncpt_id);
	tegra_dsi_controller_writel(dsi, val, DSI_INCR_SYNCPT, link_id);

	ret = nvhost_syncpt_wait_timeout_ext(dsi->dc->ndev, dsi->syncpt_id,
		dsi->syncpt_val + 1, (u32)MAX_SCHEDULE_TIMEOUT, NULL, NULL);
	if (ret < 0) {
		dev_err(&dsi->dc->ndev->dev, "DSI sync point failure\n");
		goto fail;
	}

	(dsi->syncpt_val)++;
	return 0;
fail:
	return ret;
}

static u32 tegra_dsi_get_hs_clk_rate(struct tegra_dc_dsi_data *dsi)
{
	u32 dsi_clock_rate_khz;

	switch (dsi->info.video_burst_mode) {
	case TEGRA_DSI_VIDEO_BURST_MODE_LOW_SPEED:
	case TEGRA_DSI_VIDEO_BURST_MODE_MEDIUM_SPEED:
	case TEGRA_DSI_VIDEO_BURST_MODE_FAST_SPEED:
	case TEGRA_DSI_VIDEO_BURST_MODE_FASTEST_SPEED:
		/* Calculate DSI HS clock rate for DSI burst mode */
		dsi_clock_rate_khz = dsi->default_pixel_clk_khz *
					dsi->shift_clk_div.mul /
					dsi->shift_clk_div.div;
		break;
	case TEGRA_DSI_VIDEO_NONE_BURST_MODE:
	case TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END:
	case TEGRA_DSI_VIDEO_BURST_MODE_LOWEST_SPEED:
	default:
		/* Clock rate is default DSI clock rate for non-burst mode */
		dsi_clock_rate_khz = dsi->default_hs_clk_khz;
		break;
	}

	return dsi_clock_rate_khz;
}

static u32 tegra_dsi_get_lp_clk_rate(struct tegra_dc_dsi_data *dsi, u8 lp_op)
{
	u32 dsi_clock_rate_khz;

	if (dsi->info.enable_hs_clock_on_lp_cmd_mode)
		if (dsi->info.hs_clk_in_lp_cmd_mode_freq_khz)
			dsi_clock_rate_khz =
				dsi->info.hs_clk_in_lp_cmd_mode_freq_khz;
		else
			dsi_clock_rate_khz = tegra_dsi_get_hs_clk_rate(dsi);
	else
		if (lp_op == DSI_LP_OP_READ)
			dsi_clock_rate_khz =
				dsi->info.lp_read_cmd_mode_freq_khz;
		else
			dsi_clock_rate_khz =
				dsi->info.lp_cmd_mode_freq_khz;

	return dsi_clock_rate_khz;
}

static struct tegra_dc_shift_clk_div tegra_dsi_get_shift_clk_div(
						struct tegra_dc_dsi_data *dsi)
{
	struct tegra_dc_shift_clk_div shift_clk_div;
	struct tegra_dc_shift_clk_div max_shift_clk_div;
	u32 temp_lcm;
	u32 burst_width;
	u32 burst_width_max;

	/* Get the real value of default shift_clk_div. default_shift_clk_div
	 * holds the real value of shift_clk_div.
	 */
	shift_clk_div = dsi->default_shift_clk_div;

	/* Calculate shift_clk_div which can match the video_burst_mode. */
	if (dsi->info.video_burst_mode >=
			TEGRA_DSI_VIDEO_BURST_MODE_LOWEST_SPEED) {
		if (dsi->info.max_panel_freq_khz >= dsi->default_hs_clk_khz) {
			/* formula:
			 * dsi->info.max_panel_freq_khz * shift_clk_div /
			 * dsi->default_hs_clk_khz
			 */
			max_shift_clk_div.mul = dsi->info.max_panel_freq_khz *
						shift_clk_div.mul;
			max_shift_clk_div.div = dsi->default_hs_clk_khz *
						dsi->default_shift_clk_div.div;
		} else {
			max_shift_clk_div = shift_clk_div;
		}

		burst_width = dsi->info.video_burst_mode
				- TEGRA_DSI_VIDEO_BURST_MODE_LOWEST_SPEED;
		burst_width_max = TEGRA_DSI_VIDEO_BURST_MODE_FASTEST_SPEED
				- TEGRA_DSI_VIDEO_BURST_MODE_LOWEST_SPEED;

		/* formula:
		 * (max_shift_clk_div - shift_clk_div) *
		 * burst_width / burst_width_max
		 */
		temp_lcm = lcm(max_shift_clk_div.div, shift_clk_div.div);
		shift_clk_div.mul = (max_shift_clk_div.mul * temp_lcm /
					max_shift_clk_div.div -
					shift_clk_div.mul * temp_lcm /
					shift_clk_div.div)*
					burst_width;
		shift_clk_div.div = temp_lcm * burst_width_max;
	}

	return shift_clk_div;
}

static void tegra_dsi_pix_correction(struct tegra_dc *dc,
					struct tegra_dc_dsi_data *dsi)
{
	u32 h_width_pixels;
	u32 h_act_corr = 0;
	u32 hfp_corr = 0;
	u32 temp = 0;

	h_width_pixels = dc->mode.h_back_porch + dc->mode.h_front_porch +
			dc->mode.h_sync_width + dc->mode.h_active;

	if (WARN(!dsi->info.n_data_lanes, "dsi n_data_lanes is 0\n"))
		return;

	if (dsi->info.ganged_type == TEGRA_DSI_GANGED_SYMMETRIC_EVEN_ODD) {
		temp = dc->mode.h_active % dsi->info.n_data_lanes;
		if (temp) {
			h_act_corr = dsi->info.n_data_lanes - temp;
			h_width_pixels += h_act_corr;
		}
	}

	temp = h_width_pixels % dsi->info.n_data_lanes;
	if (temp) {
		hfp_corr = dsi->info.n_data_lanes - temp;
		h_width_pixels += hfp_corr;
	}

	while (1) {
		if (WARN(!dsi->pixel_scaler_div, "dsi pixel_scaler_div is 0"))
			temp = 0;
		else
			temp = (h_width_pixels * dsi->pixel_scaler_mul /
				dsi->pixel_scaler_div) % dsi->info.n_data_lanes;
		if (temp) {
			hfp_corr += dsi->info.n_data_lanes;
			h_width_pixels += dsi->info.n_data_lanes;
		}
		else
			break;
	}

	dc->mode.h_front_porch += hfp_corr;
	dc->mode.h_active += h_act_corr;
}

static void tegra_dsi_init_sw(struct tegra_dc *dc,
			struct tegra_dc_dsi_data *dsi)
{
	u32 h_width_pixels;
	u32 v_width_lines;
	u32 pixel_clk_hz;
	u32 byte_clk_hz;
	u32 plld_clk_mhz;
	u8 n_data_lanes;

	switch (dsi->info.pixel_format) {
	case TEGRA_DSI_PIXEL_FORMAT_16BIT_P:
		/* 2 bytes per pixel */
		dsi->pixel_scaler_mul = 2;
		dsi->pixel_scaler_div = 1;
		break;
	case TEGRA_DSI_PIXEL_FORMAT_18BIT_P:
		/* 2.25 bytes per pixel */
		dsi->pixel_scaler_mul = 9;
		dsi->pixel_scaler_div = 4;
		break;
	case TEGRA_DSI_PIXEL_FORMAT_18BIT_NP:
	case TEGRA_DSI_PIXEL_FORMAT_24BIT_P:
		/* 3 bytes per pixel */
		dsi->pixel_scaler_mul = 3;
		dsi->pixel_scaler_div = 1;
		break;
	default:
		break;
	}

	dsi->ulpm = false;
	dsi->enabled = false;
	dsi->clk_ref = false;

	n_data_lanes = dsi->info.n_data_lanes;
	if (dsi->info.ganged_type == TEGRA_DSI_GANGED_SYMMETRIC_LEFT_RIGHT ||
		dsi->info.ganged_type == TEGRA_DSI_GANGED_SYMMETRIC_EVEN_ODD)
		n_data_lanes /= 2;

	dsi->dsi_control_val =
			DSI_CONTROL_VIRTUAL_CHANNEL(dsi->info.virtual_channel) |
			DSI_CONTROL_NUM_DATA_LANES(n_data_lanes - 1) |
			DSI_CONTROL_VID_SOURCE(dc->ndev->id) |
			DSI_CONTROL_DATA_FORMAT(dsi->info.pixel_format);

	if (dsi->info.ganged_type)
		tegra_dsi_pix_correction(dc, dsi);

	/* Below we are going to calculate dsi and dc clock rate.
	 * Calcuate the horizontal and vertical width.
	 */
	h_width_pixels = dc->mode.h_back_porch + dc->mode.h_front_porch +
			dc->mode.h_sync_width + dc->mode.h_active;

	v_width_lines = dc->mode.v_back_porch + dc->mode.v_front_porch +
			dc->mode.v_sync_width + dc->mode.v_active;

	/* Calculate minimum required pixel rate. */
	pixel_clk_hz = h_width_pixels * v_width_lines * dsi->info.refresh_rate;
	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE) {
		if (dsi->info.rated_refresh_rate >= dsi->info.refresh_rate)
			dev_info(&dc->ndev->dev, "DSI: measured refresh rate "
				"should be larger than rated refresh rate.\n");
		dc->mode.rated_pclk = h_width_pixels * v_width_lines *
						dsi->info.rated_refresh_rate;
	}

	/* Calculate minimum byte rate on DSI interface. */
	byte_clk_hz = (pixel_clk_hz * dsi->pixel_scaler_mul) /
			(dsi->pixel_scaler_div * dsi->info.n_data_lanes);

	/* Round up to multiple of mega hz. */
	plld_clk_mhz = DIV_ROUND_UP((byte_clk_hz * NUMOF_BIT_PER_BYTE),
								1000000);

	/* Calculate default real shift_clk_div. */
	dsi->default_shift_clk_div.mul = NUMOF_BIT_PER_BYTE *
					dsi->pixel_scaler_mul;
	dsi->default_shift_clk_div.div = 2 * dsi->pixel_scaler_div *
					dsi->info.n_data_lanes;

	/* Calculate default DSI hs clock. DSI interface is double data rate.
	 * Data is transferred on both rising and falling edge of clk, div by 2
	 * to get the actual clock rate.
	 */
	dsi->default_hs_clk_khz = plld_clk_mhz * 1000 / 2;

	dsi->default_pixel_clk_khz = (plld_clk_mhz * 1000 *
					dsi->default_shift_clk_div.div) /
					(2 * dsi->default_shift_clk_div.mul);

	/* Get the actual shift_clk_div and clock rates. */
	dsi->shift_clk_div = tegra_dsi_get_shift_clk_div(dsi);
	dsi->target_lp_clk_khz =
			tegra_dsi_get_lp_clk_rate(dsi, DSI_LP_OP_WRITE);
	dsi->target_hs_clk_khz = tegra_dsi_get_hs_clk_rate(dsi);

	dev_info(&dc->ndev->dev, "DSI: HS clock rate is %d\n",
					dsi->target_hs_clk_khz);

#if DSI_USE_SYNC_POINTS
	dsi->syncpt_id = NVSYNCPT_DSI;
#endif

	/*
	 * Force video clock to be continuous mode if
	 * enable_hs_clock_on_lp_cmd_mode is set
	 */
	if (dsi->info.enable_hs_clock_on_lp_cmd_mode) {
		if (dsi->info.video_clock_mode !=
					TEGRA_DSI_VIDEO_CLOCK_CONTINUOUS)
			dev_warn(&dc->ndev->dev,
				"Force clock continuous mode\n");

		dsi->info.video_clock_mode = TEGRA_DSI_VIDEO_CLOCK_CONTINUOUS;
	}

	atomic_set(&dsi->host_ref, 0);
	dsi->host_suspended = false;
	mutex_init(&dsi->host_lock);
	init_completion(&dc->out->user_vblank_comp);
	INIT_DELAYED_WORK(&dsi->idle_work, tegra_dc_dsi_idle_work);
	dsi->idle_delay = msecs_to_jiffies(DSI_HOST_IDLE_PERIOD);
}

#define SELECT_T_PHY(platform_t_phy_ps, default_phy, clk_ps, hw_inc) ( \
(platform_t_phy_ps) ? ( \
((DSI_CONVERT_T_PHY_PS_TO_T_PHY(platform_t_phy_ps, clk_ps, hw_inc)) < 0 ? 0 : \
(DSI_CONVERT_T_PHY_PS_TO_T_PHY(platform_t_phy_ps, clk_ps, hw_inc)))) : \
((default_phy) < 0 ? 0 : (default_phy)))

static void tegra_dsi_get_clk_phy_timing(struct tegra_dc_dsi_data *dsi,
		struct dsi_phy_timing_inclk *phy_timing_clk, u32 clk_ps)
{
	phy_timing_clk->t_tlpx = SELECT_T_PHY(
		dsi->info.phy_timing.t_tlpx_ns * 1000,
		T_TLPX_DEFAULT(clk_ps), clk_ps, T_TLPX_HW_INC);

	phy_timing_clk->t_clktrail = SELECT_T_PHY(
		dsi->info.phy_timing.t_clktrail_ns * 1000,
		T_CLKTRAIL_DEFAULT(clk_ps), clk_ps, T_CLKTRAIL_HW_INC);

	phy_timing_clk->t_clkpost = SELECT_T_PHY(
		dsi->info.phy_timing.t_clkpost_ns * 1000,
		T_CLKPOST_DEFAULT(clk_ps), clk_ps, T_CLKPOST_HW_INC);

	phy_timing_clk->t_clkzero = SELECT_T_PHY(
		dsi->info.phy_timing.t_clkzero_ns * 1000,
		T_CLKZERO_DEFAULT(clk_ps), clk_ps, T_CLKZERO_HW_INC);

	phy_timing_clk->t_clkprepare = SELECT_T_PHY(
		dsi->info.phy_timing.t_clkprepare_ns * 1000,
		T_CLKPREPARE_DEFAULT(clk_ps), clk_ps, T_CLKPREPARE_HW_INC);

	phy_timing_clk->t_clkpre = SELECT_T_PHY(
		dsi->info.phy_timing.t_clkpre_ns * 1000,
		T_CLKPRE_DEFAULT, clk_ps, T_CLKPRE_HW_INC);
}

static void tegra_dsi_get_hs_phy_timing(struct tegra_dc_dsi_data *dsi,
		struct dsi_phy_timing_inclk *phy_timing_clk, u32 clk_ps)
{
	phy_timing_clk->t_tlpx = SELECT_T_PHY(
		dsi->info.phy_timing.t_tlpx_ns * 1000,
		T_TLPX_DEFAULT(clk_ps), clk_ps, T_TLPX_HW_INC);

	phy_timing_clk->t_hsdexit = SELECT_T_PHY(
		dsi->info.phy_timing.t_hsdexit_ns * 1000,
		T_HSEXIT_DEFAULT(clk_ps), clk_ps, T_HSEXIT_HW_INC);

	phy_timing_clk->t_hstrail = SELECT_T_PHY(
		dsi->info.phy_timing.t_hstrail_ns * 1000,
		T_HSTRAIL_DEFAULT(clk_ps), clk_ps, T_HSTRAIL_HW_INC);

	phy_timing_clk->t_datzero = SELECT_T_PHY(
		dsi->info.phy_timing.t_datzero_ns * 1000,
		T_DATZERO_DEFAULT(clk_ps), clk_ps, T_DATZERO_HW_INC);

	phy_timing_clk->t_hsprepare = SELECT_T_PHY(
		dsi->info.phy_timing.t_hsprepare_ns * 1000,
		T_HSPREPARE_DEFAULT(clk_ps), clk_ps, T_HSPREPARE_HW_INC);
}

static void tegra_dsi_get_escape_phy_timing(struct tegra_dc_dsi_data *dsi,
		struct dsi_phy_timing_inclk *phy_timing_clk, u32 clk_ps)
{
	phy_timing_clk->t_tlpx = SELECT_T_PHY(
		dsi->info.phy_timing.t_tlpx_ns * 1000,
		T_TLPX_DEFAULT(clk_ps), clk_ps, T_TLPX_HW_INC);
}

static void tegra_dsi_get_bta_phy_timing(struct tegra_dc_dsi_data *dsi,
		struct dsi_phy_timing_inclk *phy_timing_clk, u32 clk_ps)
{
	phy_timing_clk->t_tlpx = SELECT_T_PHY(
		dsi->info.phy_timing.t_tlpx_ns * 1000,
		T_TLPX_DEFAULT(clk_ps), clk_ps, T_TLPX_HW_INC);

	phy_timing_clk->t_taget = SELECT_T_PHY(
		dsi->info.phy_timing.t_taget_ns * 1000,
		T_TAGET_DEFAULT(clk_ps), clk_ps, T_TAGET_HW_INC);

	phy_timing_clk->t_tasure = SELECT_T_PHY(
		dsi->info.phy_timing.t_tasure_ns * 1000,
		T_TASURE_DEFAULT(clk_ps), clk_ps, T_TASURE_HW_INC);

	phy_timing_clk->t_tago = SELECT_T_PHY(
		dsi->info.phy_timing.t_tago_ns * 1000,
		T_TAGO_DEFAULT(clk_ps), clk_ps, T_TAGO_HW_INC);
}

static void tegra_dsi_get_ulps_phy_timing(struct tegra_dc_dsi_data *dsi,
		struct dsi_phy_timing_inclk *phy_timing_clk, u32 clk_ps)
{
	phy_timing_clk->t_tlpx = SELECT_T_PHY(
		dsi->info.phy_timing.t_tlpx_ns * 1000,
		T_TLPX_DEFAULT(clk_ps), clk_ps, T_TLPX_HW_INC);

	phy_timing_clk->t_wakeup = SELECT_T_PHY(
		dsi->info.phy_timing.t_wakeup_ns * 1000,
		T_WAKEUP_DEFAULT, clk_ps, T_WAKEUP_HW_INC);
}

#undef SELECT_T_PHY

static void tegra_dsi_get_phy_timing(struct tegra_dc_dsi_data *dsi,
				struct dsi_phy_timing_inclk *phy_timing_clk,
				u32 clk_ps, u8 lphs)
{
	if (tegra_platform_is_fpga() && !(dsi->info.ganged_type)) {
		clk_ps = (1000 * 1000 * 1000) / (dsi->info.fpga_freq_khz ?
			dsi->info.fpga_freq_khz : DEFAULT_FPGA_FREQ_KHZ);
	}

	if (lphs == DSI_LPHS_IN_HS_MODE) {
		tegra_dsi_get_clk_phy_timing(dsi, phy_timing_clk, clk_ps);
		tegra_dsi_get_hs_phy_timing(dsi, phy_timing_clk, clk_ps);
	} else {
		/* default is LP mode */
		tegra_dsi_get_escape_phy_timing(dsi, phy_timing_clk, clk_ps);
		tegra_dsi_get_bta_phy_timing(dsi, phy_timing_clk, clk_ps);
		tegra_dsi_get_ulps_phy_timing(dsi, phy_timing_clk, clk_ps);
		if (dsi->info.enable_hs_clock_on_lp_cmd_mode)
			tegra_dsi_get_clk_phy_timing(dsi,
					phy_timing_clk, clk_ps);
	}
}

static int tegra_dsi_mipi_phy_timing_range(struct tegra_dc_dsi_data *dsi,
				struct dsi_phy_timing_inclk *phy_timing,
				u32 clk_ps, u8 lphs)
{
	int err = 0;

#define CHECK_RANGE(val, min, max) ( \
		((min) == NOT_DEFINED ? 0 : (val) < (min)) || \
		((max) == NOT_DEFINED ? 0 : (val) > (max)) ? -EINVAL : 0)

	if (tegra_platform_is_fpga())
		clk_ps = dsi->info.fpga_freq_khz ?
			((1000 * 1000 * 1000) / dsi->info.fpga_freq_khz) :
			DEFAULT_FPGA_FREQ_KHZ;

	err = CHECK_RANGE(
	DSI_CONVERT_T_PHY_TO_T_PHY_PS(
			phy_timing->t_tlpx, clk_ps, T_TLPX_HW_INC),
			MIPI_T_TLPX_PS_MIN, MIPI_T_TLPX_PS_MAX);
	if (err < 0) {
		dev_warn(&dsi->dc->ndev->dev,
			"dsi: Tlpx mipi range violated\n");
		goto fail;
	}

	if (lphs == DSI_LPHS_IN_HS_MODE) {
		err = CHECK_RANGE(
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
			phy_timing->t_hsdexit, clk_ps, T_HSEXIT_HW_INC),
			MIPI_T_HSEXIT_PS_MIN, MIPI_T_HSEXIT_PS_MAX);
		if (err < 0) {
			dev_warn(&dsi->dc->ndev->dev,
				"dsi: HsExit mipi range violated\n");
			goto fail;
		}

		err = CHECK_RANGE(
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
			phy_timing->t_hstrail, clk_ps, T_HSTRAIL_HW_INC),
			MIPI_T_HSTRAIL_PS_MIN(clk_ps), MIPI_T_HSTRAIL_PS_MAX);
		if (err < 0) {
			dev_warn(&dsi->dc->ndev->dev,
				"dsi: HsTrail mipi range violated\n");
			goto fail;
		}

		err = CHECK_RANGE(
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
			phy_timing->t_datzero, clk_ps, T_DATZERO_HW_INC),
			MIPI_T_HSZERO_PS_MIN, MIPI_T_HSZERO_PS_MAX);
		if (err < 0) {
			dev_warn(&dsi->dc->ndev->dev,
				"dsi: HsZero mipi range violated\n");
			goto fail;
		}

		err = CHECK_RANGE(
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
			phy_timing->t_hsprepare, clk_ps, T_HSPREPARE_HW_INC),
			MIPI_T_HSPREPARE_PS_MIN(clk_ps),
			MIPI_T_HSPREPARE_PS_MAX(clk_ps));
		if (err < 0) {
			dev_warn(&dsi->dc->ndev->dev,
				"dsi: HsPrepare mipi range violated\n");
			goto fail;
		}

		err = CHECK_RANGE(
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
			phy_timing->t_hsprepare, clk_ps, T_HSPREPARE_HW_INC) +
			DSI_CONVERT_T_PHY_TO_T_PHY_PS(
			phy_timing->t_datzero, clk_ps, T_DATZERO_HW_INC),
			MIPI_T_HSPREPARE_ADD_HSZERO_PS_MIN(clk_ps),
			MIPI_T_HSPREPARE_ADD_HSZERO_PS_MAX);
		if (err < 0) {
			dev_warn(&dsi->dc->ndev->dev,
			"dsi: HsPrepare + HsZero mipi range violated\n");
			goto fail;
		}
	} else {
		/* default is LP mode */
		err = CHECK_RANGE(
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
			phy_timing->t_wakeup, clk_ps, T_WAKEUP_HW_INC),
			MIPI_T_WAKEUP_PS_MIN, MIPI_T_WAKEUP_PS_MAX);
		if (err < 0) {
			dev_warn(&dsi->dc->ndev->dev,
				"dsi: WakeUp mipi range violated\n");
			goto fail;
		}

		err = CHECK_RANGE(
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
			phy_timing->t_tasure, clk_ps, T_TASURE_HW_INC),
			MIPI_T_TASURE_PS_MIN(DSI_CONVERT_T_PHY_TO_T_PHY_PS(
			phy_timing->t_tlpx, clk_ps, T_TLPX_HW_INC)),
			MIPI_T_TASURE_PS_MAX(DSI_CONVERT_T_PHY_TO_T_PHY_PS(
			phy_timing->t_tlpx, clk_ps, T_TLPX_HW_INC)));
		if (err < 0) {
			dev_warn(&dsi->dc->ndev->dev,
				"dsi: TaSure mipi range violated\n");
			goto fail;
		}
	}

	if (lphs == DSI_LPHS_IN_HS_MODE ||
		dsi->info.enable_hs_clock_on_lp_cmd_mode) {
		err = CHECK_RANGE(
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
			phy_timing->t_clktrail, clk_ps, T_CLKTRAIL_HW_INC),
			MIPI_T_CLKTRAIL_PS_MIN, MIPI_T_CLKTRAIL_PS_MAX);
		if (err < 0) {
			dev_warn(&dsi->dc->ndev->dev,
				"dsi: ClkTrail mipi range violated\n");
			goto fail;
		}

		err = CHECK_RANGE(
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
			phy_timing->t_clkpost, clk_ps, T_CLKPOST_HW_INC),
			MIPI_T_CLKPOST_PS_MIN(clk_ps), MIPI_T_CLKPOST_PS_MAX);
		if (err < 0) {
			dev_warn(&dsi->dc->ndev->dev,
				"dsi: ClkPost mipi range violated\n");
			goto fail;
		}

		err = CHECK_RANGE(
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
			phy_timing->t_clkzero, clk_ps, T_CLKZERO_HW_INC),
			MIPI_T_CLKZERO_PS_MIN, MIPI_T_CLKZERO_PS_MAX);
		if (err < 0) {
			dev_warn(&dsi->dc->ndev->dev,
				"dsi: ClkZero mipi range violated\n");
			goto fail;
		}

		err = CHECK_RANGE(
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
			phy_timing->t_clkprepare, clk_ps, T_CLKPREPARE_HW_INC),
			MIPI_T_CLKPREPARE_PS_MIN, MIPI_T_CLKPREPARE_PS_MAX);
		if (err < 0) {
			dev_warn(&dsi->dc->ndev->dev,
				"dsi: ClkPrepare mipi range violated\n");
			goto fail;
		}

		err = CHECK_RANGE(
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
			phy_timing->t_clkpre, clk_ps, T_CLKPRE_HW_INC),
			MIPI_T_CLKPRE_PS_MIN, MIPI_T_CLKPRE_PS_MAX);
		if (err < 0) {
			dev_warn(&dsi->dc->ndev->dev,
				"dsi: ClkPre mipi range violated\n");
			goto fail;
		}

		err = CHECK_RANGE(
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
		phy_timing->t_clkprepare, clk_ps, T_CLKPREPARE_HW_INC) +
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
			phy_timing->t_clkzero, clk_ps, T_CLKZERO_HW_INC),
			MIPI_T_CLKPREPARE_ADD_CLKZERO_PS_MIN,
			MIPI_T_CLKPREPARE_ADD_CLKZERO_PS_MAX);
		if (err < 0) {
			dev_warn(&dsi->dc->ndev->dev,
			"dsi: ClkPrepare + ClkZero mipi range violated\n");
			goto fail;
		}
	}
fail:
#undef CHECK_RANGE
	return err;
}

static int tegra_dsi_hs_phy_len(struct tegra_dc_dsi_data *dsi,
				struct dsi_phy_timing_inclk *phy_timing,
				u32 clk_ps, u8 lphs)
{
	u32 hs_t_phy_ps = 0;
	u32 clk_t_phy_ps = 0;
	u32 t_phy_ps;
	u32 h_blank_ps;
	struct tegra_dc_mode *modes;
	u32 t_pix_ps;
	int err = 0;

	if (!(lphs == DSI_LPHS_IN_HS_MODE))
		goto fail;

	if (dsi->info.video_data_type ==
		TEGRA_DSI_VIDEO_TYPE_VIDEO_MODE &&
		dsi->info.video_burst_mode <=
		TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END)
		goto fail;

	modes = dsi->dc->out->modes;
	t_pix_ps = clk_ps * BITS_PER_BYTE *
		dsi->pixel_scaler_mul / dsi->pixel_scaler_div;

	hs_t_phy_ps =
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
		phy_timing->t_tlpx, clk_ps, T_TLPX_HW_INC) +
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
		phy_timing->t_tlpx, clk_ps, T_TLPX_HW_INC) +
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
		phy_timing->t_hsprepare, clk_ps, T_HSPREPARE_HW_INC) +
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
		phy_timing->t_datzero, clk_ps, T_DATZERO_HW_INC) +
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
		phy_timing->t_hstrail, clk_ps, T_HSTRAIL_HW_INC) +
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
		phy_timing->t_hsdexit, clk_ps, T_HSEXIT_HW_INC);

	if (dsi->info.video_clock_mode == TEGRA_DSI_VIDEO_CLOCK_TX_ONLY) {
		clk_t_phy_ps =
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
		phy_timing->t_clkpost, clk_ps, T_CLKPOST_HW_INC) +
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
		phy_timing->t_clktrail, clk_ps, T_CLKTRAIL_HW_INC) +
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
		phy_timing->t_hsdexit, clk_ps, T_HSEXIT_HW_INC) +
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
		phy_timing->t_tlpx, clk_ps, T_TLPX_HW_INC) +
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
		phy_timing->t_clkprepare, clk_ps, T_CLKPREPARE_HW_INC) +
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
		phy_timing->t_clkzero, clk_ps, T_CLKZERO_HW_INC) +
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
		phy_timing->t_clkpre, clk_ps, T_CLKPRE_HW_INC);

		/* clk_pre overlaps LP-11 hs mode start sequence */
		hs_t_phy_ps -= DSI_CONVERT_T_PHY_TO_T_PHY_PS(
			phy_timing->t_tlpx, clk_ps, T_TLPX_HW_INC);
	}

	h_blank_ps = t_pix_ps * (modes->h_sync_width + modes->h_back_porch +
						modes->h_front_porch);

	/* Extra tlpx and byte cycle required by dsi HW */
	t_phy_ps = dsi->info.n_data_lanes * (hs_t_phy_ps + clk_t_phy_ps +
		DSI_CONVERT_T_PHY_TO_T_PHY_PS(
		phy_timing->t_tlpx, clk_ps, T_TLPX_HW_INC) +
		clk_ps * BITS_PER_BYTE);

	if (h_blank_ps < t_phy_ps) {
		err = -EINVAL;
		dev_WARN(&dsi->dc->ndev->dev,
			"dsi: Hblank is smaller than HS phy timing: %u pix\n",
					(t_phy_ps - h_blank_ps) / t_pix_ps);
		goto fail;
	}

	return 0;
fail:
	return err;
}

static int tegra_dsi_constraint_phy_timing(struct tegra_dc_dsi_data *dsi,
				struct dsi_phy_timing_inclk *phy_timing,
				u32 clk_ps, u8 lphs)
{
	int err = 0;

	err = tegra_dsi_mipi_phy_timing_range(dsi, phy_timing, clk_ps, lphs);
	if (err < 0) {
		dev_warn(&dsi->dc->ndev->dev, "dsi: mipi range violated\n");
		goto fail;
	}

	err = tegra_dsi_hs_phy_len(dsi, phy_timing, clk_ps, lphs);
	if (err < 0) {
		dev_err(&dsi->dc->ndev->dev, "dsi: Hblank too short\n");
		goto fail;
	}

	/* TODO: add more contraints */
fail:
	return err;
}

static void tegra_dsi_set_phy_timing(struct tegra_dc_dsi_data *dsi, u8 lphs)
{
	u32 val;
	struct dsi_phy_timing_inclk phy_timing = dsi->phy_timing;

	tegra_dsi_get_phy_timing
		(dsi, &phy_timing, dsi->current_bit_clk_ps, lphs);

	tegra_dsi_constraint_phy_timing(dsi, &phy_timing,
					dsi->current_bit_clk_ps, lphs);

	if (tegra_platform_is_fpga() && dsi->info.ganged_type) {
		phy_timing.t_hsdexit += T_HSEXIT_HW_INC;
		phy_timing.t_hstrail += T_HSTRAIL_HW_INC + 3;
		phy_timing.t_datzero += T_DATZERO_HW_INC;
		phy_timing.t_hsprepare += T_HSPREPARE_HW_INC;

		phy_timing.t_clktrail += T_CLKTRAIL_HW_INC;
		phy_timing.t_clkpost += T_CLKPOST_HW_INC;
		phy_timing.t_clkzero += T_CLKZERO_HW_INC;
		phy_timing.t_tlpx += T_TLPX_HW_INC;

		phy_timing.t_clkprepare += T_CLKPREPARE_HW_INC;
		phy_timing.t_clkpre += T_CLKPRE_HW_INC;
		phy_timing.t_wakeup += T_WAKEUP_HW_INC;

		phy_timing.t_taget += T_TAGET_HW_INC;
		phy_timing.t_tasure += T_TASURE_HW_INC;
		phy_timing.t_tago += T_TAGO_HW_INC;
	}
	val = DSI_PHY_TIMING_0_THSDEXIT(phy_timing.t_hsdexit) |
			DSI_PHY_TIMING_0_THSTRAIL(phy_timing.t_hstrail) |
			DSI_PHY_TIMING_0_TDATZERO(phy_timing.t_datzero) |
			DSI_PHY_TIMING_0_THSPREPR(phy_timing.t_hsprepare);
	tegra_dsi_writel(dsi, val, DSI_PHY_TIMING_0);

	val = DSI_PHY_TIMING_1_TCLKTRAIL(phy_timing.t_clktrail) |
			DSI_PHY_TIMING_1_TCLKPOST(phy_timing.t_clkpost) |
			DSI_PHY_TIMING_1_TCLKZERO(phy_timing.t_clkzero) |
			DSI_PHY_TIMING_1_TTLPX(phy_timing.t_tlpx);
	tegra_dsi_writel(dsi, val, DSI_PHY_TIMING_1);

	val = DSI_PHY_TIMING_2_TCLKPREPARE(phy_timing.t_clkprepare) |
		DSI_PHY_TIMING_2_TCLKPRE(phy_timing.t_clkpre) |
			DSI_PHY_TIMING_2_TWAKEUP(phy_timing.t_wakeup);
	tegra_dsi_writel(dsi, val, DSI_PHY_TIMING_2);

	val = DSI_BTA_TIMING_TTAGET(phy_timing.t_taget) |
			DSI_BTA_TIMING_TTASURE(phy_timing.t_tasure) |
			DSI_BTA_TIMING_TTAGO(phy_timing.t_tago);
	tegra_dsi_writel(dsi, val, DSI_BTA_TIMING);

	dsi->phy_timing = phy_timing;
}

static u32 tegra_dsi_sol_delay_burst(struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi)
{
	u32 dsi_to_pixel_clk_ratio;
	u32 temp;
	u32 temp1;
	u32 mipi_clk_adj_kHz = 0;
	u32 sol_delay;
	struct tegra_dc_mode *dc_modes = &dc->mode;

	/* Get Fdsi/Fpixel ration (note: Fdsi is in bit format) */
	dsi_to_pixel_clk_ratio = (dsi->current_dsi_clk_khz * 2 +
		dsi->default_pixel_clk_khz - 1) / dsi->default_pixel_clk_khz;

	/* Convert Fdsi to byte format */
	dsi_to_pixel_clk_ratio *= 1000/8;

	/* Multiplying by 1000 so that we don't loose the fraction part */
	temp = dc_modes->h_active * 1000;
	temp1 = dc_modes->h_active + dc_modes->h_back_porch +
			dc_modes->h_sync_width;

	sol_delay = temp1 * dsi_to_pixel_clk_ratio -
			temp * dsi->pixel_scaler_mul /
			(dsi->pixel_scaler_div * dsi->info.n_data_lanes);

	/* Do rounding on sol delay */
	sol_delay = (sol_delay + 1000 - 1)/1000;

	/* TODO:
	 * 1. find out the correct sol fifo depth to use
	 * 2. verify with hw about the clamping function
	 */
	if (sol_delay > (480 * 4)) {
		sol_delay = (480 * 4);
		mipi_clk_adj_kHz = sol_delay +
			(dc_modes->h_active * dsi->pixel_scaler_mul) /
			(dsi->info.n_data_lanes * dsi->pixel_scaler_div);

		mipi_clk_adj_kHz *= (dsi->default_pixel_clk_khz / temp1);

		mipi_clk_adj_kHz *= 4;
	}

	dsi->target_hs_clk_khz = mipi_clk_adj_kHz;

	return sol_delay;
}

static void tegra_dsi_set_sol_delay(struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi)
{
	u32 sol_delay;
	u32 internal_delay;
	u32 h_width_byte_clk;
	u32 h_width_pixels;
	u32 h_width_ganged_byte_clk;
	u8 n_data_lanes_this_cont = 0;
	u8 n_data_lanes_ganged = 0;

	if (!(dsi->info.ganged_type)) {
		if (dsi->info.video_burst_mode ==
			TEGRA_DSI_VIDEO_NONE_BURST_MODE ||
			dsi->info.video_burst_mode ==
			TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END) {
#define VIDEO_FIFO_LATENCY_PIXEL_CLK 8
			sol_delay = VIDEO_FIFO_LATENCY_PIXEL_CLK *
				dsi->pixel_scaler_mul / dsi->pixel_scaler_div;
#undef VIDEO_FIFO_LATENCY_PIXEL_CLK
			dsi->status.clk_burst = DSI_CLK_BURST_NONE_BURST;
		} else {
			sol_delay = tegra_dsi_sol_delay_burst(dc, dsi);
			dsi->status.clk_burst = DSI_CLK_BURST_BURST_MODE;
		}
	} else {
#define SOL_TO_VALID_PIX_CLK_DELAY 4
#define VALID_TO_FIFO_PIX_CLK_DELAY 4
#define FIFO_WR_PIX_CLK_DELAY 2
#define FIFO_RD_BYTE_CLK_DELAY 6
#define TOT_INTERNAL_PIX_DELAY (SOL_TO_VALID_PIX_CLK_DELAY + \
				VALID_TO_FIFO_PIX_CLK_DELAY + \
				FIFO_WR_PIX_CLK_DELAY)

		internal_delay = DIV_ROUND_UP(
				TOT_INTERNAL_PIX_DELAY * dsi->pixel_scaler_mul,
				dsi->pixel_scaler_div * dsi->info.n_data_lanes)
				+ FIFO_RD_BYTE_CLK_DELAY;

		h_width_pixels = dc->mode.h_sync_width +
					dc->mode.h_back_porch +
					dc->mode.h_active +
					dc->mode.h_front_porch;

		h_width_byte_clk = DIV_ROUND_UP(h_width_pixels *
					dsi->pixel_scaler_mul,
					dsi->pixel_scaler_div *
					dsi->info.n_data_lanes);

		if (dsi->info.ganged_type ==
			TEGRA_DSI_GANGED_SYMMETRIC_LEFT_RIGHT ||
			dsi->info.ganged_type ==
			TEGRA_DSI_GANGED_SYMMETRIC_EVEN_ODD) {
			n_data_lanes_this_cont = dsi->info.n_data_lanes / 2;
			n_data_lanes_ganged = dsi->info.n_data_lanes;
		}

		h_width_ganged_byte_clk = DIV_ROUND_UP(
					n_data_lanes_this_cont *
					h_width_byte_clk,
					n_data_lanes_ganged);

		sol_delay = h_width_byte_clk - h_width_ganged_byte_clk +
							internal_delay;
		sol_delay = (dsi->info.video_data_type ==
				TEGRA_DSI_VIDEO_TYPE_COMMAND_MODE) ?
				sol_delay + 20 : sol_delay;

#undef SOL_TO_VALID_PIX_CLK_DELAY
#undef VALID_TO_FIFO_PIX_CLK_DELAY
#undef FIFO_WR_PIX_CLK_DELAY
#undef FIFO_RD_BYTE_CLK_DELAY
#undef TOT_INTERNAL_PIX_DELAY
	}

	tegra_dsi_writel(dsi, DSI_SOL_DELAY_SOL_DELAY(sol_delay),
						DSI_SOL_DELAY);
}

static void tegra_dsi_set_timeout(struct tegra_dc_dsi_data *dsi)
{
	u32 val;
	u32 bytes_per_frame;
	u32 timeout = 0;

	/* TODO: verify the following equation */
	bytes_per_frame = dsi->current_dsi_clk_khz * 1000 * 2 /
						(dsi->info.refresh_rate * 8);
	timeout = bytes_per_frame / DSI_CYCLE_COUNTER_VALUE;
	timeout = (timeout + DSI_HTX_TO_MARGIN) & 0xffff;

	val = DSI_TIMEOUT_0_LRXH_TO(DSI_LRXH_TO_VALUE) |
			DSI_TIMEOUT_0_HTX_TO(timeout);
	tegra_dsi_writel(dsi, val, DSI_TIMEOUT_0);

	if (dsi->info.panel_reset_timeout_msec)
		timeout = (dsi->info.panel_reset_timeout_msec * 1000 * 1000 *
					 1000) / dsi->current_bit_clk_ps;
	else
		timeout = DSI_PR_TO_VALUE;

	val = DSI_TIMEOUT_1_PR_TO(timeout) |
		DSI_TIMEOUT_1_TA_TO(DSI_TA_TO_VALUE);
	tegra_dsi_writel(dsi, val, DSI_TIMEOUT_1);

	val = DSI_TO_TALLY_P_RESET_STATUS(IN_RESET) |
		DSI_TO_TALLY_TA_TALLY(DSI_TA_TALLY_VALUE)|
		DSI_TO_TALLY_LRXH_TALLY(DSI_LRXH_TALLY_VALUE)|
		DSI_TO_TALLY_HTX_TALLY(DSI_HTX_TALLY_VALUE);
	tegra_dsi_writel(dsi, val, DSI_TO_TALLY);
}

static void tegra_dsi_setup_ganged_mode_pkt_length(struct tegra_dc *dc,
						struct tegra_dc_dsi_data *dsi)
{
	u32 hact_pkt_len_pix_orig = dc->mode.h_active;
	u32 hact_pkt_len_pix = 0;
	u32 hact_pkt_len_bytes = 0;
	u32 hfp_pkt_len_bytes = 0;
	u32 pix_per_line_orig = 0;
	u32 pix_per_line = 0;
	u32 val = 0;
	int i = 0;

/* hsync + hact + hfp = (4) + (4+2) + (4+2) */
#define HEADER_OVERHEAD 16

	pix_per_line_orig = dc->mode.h_sync_width + dc->mode.h_back_porch +
			dc->mode.h_active + dc->mode.h_front_porch;

	val = DSI_PKT_LEN_0_1_LENGTH_0(0) |
		DSI_PKT_LEN_0_1_LENGTH_1(0);
	tegra_dsi_writel(dsi, val, DSI_PKT_LEN_0_1);

	switch (dsi->info.ganged_type) {
	case TEGRA_DSI_GANGED_SYMMETRIC_LEFT_RIGHT: /* fall through */
	case TEGRA_DSI_GANGED_SYMMETRIC_EVEN_ODD: /* fall through */
		hact_pkt_len_pix = DIV_ROUND_UP(hact_pkt_len_pix_orig, 2);
		pix_per_line = DIV_ROUND_UP(pix_per_line_orig, 2);
		break;
	default:
		dev_err(&dc->ndev->dev, "dsi: invalid ganged type\n");
	}

	for (i = 0; i < dsi->max_instances; i++) {
		hact_pkt_len_bytes = hact_pkt_len_pix *
			dsi->pixel_scaler_mul / dsi->pixel_scaler_div;
		hfp_pkt_len_bytes = pix_per_line *
			dsi->pixel_scaler_mul / dsi->pixel_scaler_div -
			hact_pkt_len_bytes - HEADER_OVERHEAD;

		val = DSI_PKT_LEN_2_3_LENGTH_2(0x0) |
			DSI_PKT_LEN_2_3_LENGTH_3(hact_pkt_len_bytes);
		tegra_dsi_controller_writel(dsi, val, DSI_PKT_LEN_2_3, i);

		val = DSI_PKT_LEN_4_5_LENGTH_4(hfp_pkt_len_bytes) |
			DSI_PKT_LEN_4_5_LENGTH_5(0);
		tegra_dsi_controller_writel(dsi, val, DSI_PKT_LEN_4_5, i);

		hact_pkt_len_pix =
			hact_pkt_len_pix_orig - hact_pkt_len_pix;
		pix_per_line = pix_per_line_orig - pix_per_line;
	}

	val = DSI_PKT_LEN_6_7_LENGTH_6(0) |
		DSI_PKT_LEN_6_7_LENGTH_7(0x0f0f);
	tegra_dsi_writel(dsi, val, DSI_PKT_LEN_6_7);

#undef HEADER_OVERHEAD
}

static void tegra_dsi_setup_video_mode_pkt_length(struct tegra_dc *dc,
						struct tegra_dc_dsi_data *dsi)
{
	u32 val;
	u32 hact_pkt_len;
	u32 hsa_pkt_len;
	u32 hbp_pkt_len;
	u32 hfp_pkt_len;

	hact_pkt_len = dc->mode.h_active * dsi->pixel_scaler_mul /
							dsi->pixel_scaler_div;
	hsa_pkt_len = dc->mode.h_sync_width * dsi->pixel_scaler_mul /
							dsi->pixel_scaler_div;
	hbp_pkt_len = dc->mode.h_back_porch * dsi->pixel_scaler_mul /
							dsi->pixel_scaler_div;
	hfp_pkt_len = dc->mode.h_front_porch * dsi->pixel_scaler_mul /
							dsi->pixel_scaler_div;

	if (dsi->info.video_burst_mode !=
				TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END)
		hbp_pkt_len += hsa_pkt_len;

	hsa_pkt_len -= DSI_HSYNC_BLNK_PKT_OVERHEAD;
	hbp_pkt_len -= DSI_HBACK_PORCH_PKT_OVERHEAD;
	hfp_pkt_len -= DSI_HFRONT_PORCH_PKT_OVERHEAD;

	val = DSI_PKT_LEN_0_1_LENGTH_0(0) |
			DSI_PKT_LEN_0_1_LENGTH_1(hsa_pkt_len);
	tegra_dsi_writel(dsi, val, DSI_PKT_LEN_0_1);

	val = DSI_PKT_LEN_2_3_LENGTH_2(hbp_pkt_len) |
			DSI_PKT_LEN_2_3_LENGTH_3(hact_pkt_len);
	tegra_dsi_writel(dsi, val, DSI_PKT_LEN_2_3);

	val = DSI_PKT_LEN_4_5_LENGTH_4(hfp_pkt_len) |
			DSI_PKT_LEN_4_5_LENGTH_5(0);
	tegra_dsi_writel(dsi, val, DSI_PKT_LEN_4_5);

	val = DSI_PKT_LEN_6_7_LENGTH_6(0) | DSI_PKT_LEN_6_7_LENGTH_7(0x0f0f);
	tegra_dsi_writel(dsi, val, DSI_PKT_LEN_6_7);
}

static void tegra_dsi_setup_cmd_mode_pkt_length(struct tegra_dc *dc,
						struct tegra_dc_dsi_data *dsi)
{
	unsigned long	val;
	unsigned long	act_bytes;

	if (dsi->info.ganged_type) {
		act_bytes = DIV_ROUND_UP(dc->mode.h_active, 2);
		act_bytes = (act_bytes) * dsi->pixel_scaler_mul /
				dsi->pixel_scaler_div + 1;
	} else {
		act_bytes = dc->mode.h_active * dsi->pixel_scaler_mul /
				dsi->pixel_scaler_div + 1;
	}
	val = DSI_PKT_LEN_0_1_LENGTH_0(0) | DSI_PKT_LEN_0_1_LENGTH_1(0);
	tegra_dsi_writel(dsi, val, DSI_PKT_LEN_0_1);

	val = DSI_PKT_LEN_2_3_LENGTH_2(0) | DSI_PKT_LEN_2_3_LENGTH_3(act_bytes);
	tegra_dsi_writel(dsi, val, DSI_PKT_LEN_2_3);

	val = DSI_PKT_LEN_4_5_LENGTH_4(0) | DSI_PKT_LEN_4_5_LENGTH_5(act_bytes);
	tegra_dsi_writel(dsi, val, DSI_PKT_LEN_4_5);

	val = DSI_PKT_LEN_6_7_LENGTH_6(0) | DSI_PKT_LEN_6_7_LENGTH_7(0x0f0f);
	tegra_dsi_writel(dsi, val, DSI_PKT_LEN_6_7);
}

static void tegra_dsi_set_pkt_length(struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi)
{
	if (dsi->driven_mode == TEGRA_DSI_DRIVEN_BY_HOST)
		return;

	if (dsi->info.video_data_type == TEGRA_DSI_VIDEO_TYPE_VIDEO_MODE) {
		if (dsi->info.ganged_type)
			tegra_dsi_setup_ganged_mode_pkt_length(dc, dsi);
		else
			tegra_dsi_setup_video_mode_pkt_length(dc, dsi);
	} else {
		tegra_dsi_setup_cmd_mode_pkt_length(dc, dsi);
	}
}

static void tegra_dsi_set_pkt_seq(struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi)
{
	const u32 *pkt_seq;
	u32 rgb_info;
	u32 pkt_seq_3_5_rgb_lo;
	u32 pkt_seq_3_5_rgb_hi;
	u32	val;
	u32 reg;
	u8  i;

	if (dsi->driven_mode == TEGRA_DSI_DRIVEN_BY_HOST)
		return;

	switch (dsi->info.pixel_format) {
	case TEGRA_DSI_PIXEL_FORMAT_16BIT_P:
		rgb_info = CMD_RGB_16BPP;
		break;
	case TEGRA_DSI_PIXEL_FORMAT_18BIT_P:
		rgb_info = CMD_RGB_18BPP;
		break;
	case TEGRA_DSI_PIXEL_FORMAT_18BIT_NP:
		rgb_info = CMD_RGB_18BPPNP;
		break;
	case TEGRA_DSI_PIXEL_FORMAT_24BIT_P:
	default:
		rgb_info = CMD_RGB_24BPP;
		break;
	}

	pkt_seq_3_5_rgb_lo = 0;
	pkt_seq_3_5_rgb_hi = 0;
	if (dsi->info.pkt_seq)
		pkt_seq = dsi->info.pkt_seq;
	else if (dsi->info.video_data_type ==
		TEGRA_DSI_VIDEO_TYPE_COMMAND_MODE) {
			pkt_seq = dsi_pkt_seq_cmd_mode;
	} else {
		switch (dsi->info.video_burst_mode) {
		case TEGRA_DSI_VIDEO_BURST_MODE_LOWEST_SPEED:
		case TEGRA_DSI_VIDEO_BURST_MODE_LOW_SPEED:
		case TEGRA_DSI_VIDEO_BURST_MODE_MEDIUM_SPEED:
		case TEGRA_DSI_VIDEO_BURST_MODE_FAST_SPEED:
		case TEGRA_DSI_VIDEO_BURST_MODE_FASTEST_SPEED:
			pkt_seq_3_5_rgb_lo =
					DSI_PKT_SEQ_3_LO_PKT_32_ID(rgb_info);
			if (!dsi->info.no_pkt_seq_eot)
				pkt_seq = dsi_pkt_seq_video_burst;
			else
				pkt_seq = dsi_pkt_seq_video_burst_no_eot;
			break;
		case TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END:
			pkt_seq_3_5_rgb_hi =
					DSI_PKT_SEQ_3_HI_PKT_34_ID(rgb_info);
			pkt_seq = dsi_pkt_seq_video_non_burst_syne;
			break;
		case TEGRA_DSI_VIDEO_NONE_BURST_MODE:
		default:
			if (dsi->info.ganged_type) {
				pkt_seq_3_5_rgb_lo =
					DSI_PKT_SEQ_3_LO_PKT_31_ID(rgb_info);
				pkt_seq =
				dsi_pkt_seq_video_non_burst_no_eot_no_lp_no_hbp;
			} else {
				pkt_seq_3_5_rgb_lo =
					DSI_PKT_SEQ_3_LO_PKT_32_ID(rgb_info);
				pkt_seq = dsi_pkt_seq_video_non_burst;
			}

			/* Simulator does not support EOT packet yet */
			if (tegra_cpu_is_asim())
				pkt_seq = dsi_pkt_seq_video_non_burst_no_eot;
			break;
		}
	}

	for (i = 0; i < NUMOF_PKT_SEQ; i++) {
		val = pkt_seq[i];
		reg = dsi_pkt_seq_reg[i];
		if ((reg == DSI_PKT_SEQ_3_LO) || (reg == DSI_PKT_SEQ_5_LO))
			val |= pkt_seq_3_5_rgb_lo;
		if ((reg == DSI_PKT_SEQ_3_HI) || (reg == DSI_PKT_SEQ_5_HI))
			val |= pkt_seq_3_5_rgb_hi;
		tegra_dsi_writel(dsi, val, reg);
	}
}

static void tegra_dsi_reset_underflow_overflow
				(struct tegra_dc_dsi_data *dsi)
{
	u32 val;

	val = tegra_dsi_readl(dsi, DSI_STATUS);
	val &= (DSI_STATUS_LB_OVERFLOW(0x1) | DSI_STATUS_LB_UNDERFLOW(0x1));
	if (val) {
		if (val & DSI_STATUS_LB_OVERFLOW(0x1))
			dev_warn(&dsi->dc->ndev->dev,
				"dsi: video fifo overflow. Resetting flag\n");
		if (val & DSI_STATUS_LB_UNDERFLOW(0x1))
			dev_warn(&dsi->dc->ndev->dev,
				"dsi: video fifo underflow. Resetting flag\n");
		val = tegra_dsi_readl(dsi, DSI_HOST_DSI_CONTROL);
		val |= DSI_HOST_CONTROL_FIFO_STAT_RESET(0x1);
		tegra_dsi_writel(dsi, val, DSI_HOST_DSI_CONTROL);
		udelay(5);
	}
}

static void tegra_dsi_soft_reset(struct tegra_dc_dsi_data *dsi)
{
	u32 trigger;
	u32 val;
	u32 frame_period = DIV_ROUND_UP(S_TO_MS(1), dsi->info.refresh_rate);
	struct tegra_dc_mode mode = dsi->dc->mode;
	u32 tot_lines = mode.v_sync_width + mode.v_back_porch +
				mode.v_active + mode.v_front_porch;
	u32 line_period = DIV_ROUND_UP(MS_TO_US(frame_period), tot_lines);
	u32 timeout_cnt = 0;

/* wait for 1 frame duration + few extra cycles for dsi to go idle */
#define DSI_IDLE_TIMEOUT	(tot_lines + 5)

	val = tegra_dsi_readl(dsi, DSI_STATUS);
	while (!(val & DSI_STATUS_IDLE(0x1))) {
		cpu_relax();
		udelay(line_period);
		val = tegra_dsi_readl(dsi, DSI_STATUS);
		if (timeout_cnt++ > DSI_IDLE_TIMEOUT) {
			dev_warn(&dsi->dc->ndev->dev, "dsi not idle when soft reset\n");
			break;
		}
	}

	tegra_dsi_writel(dsi,
		DSI_POWER_CONTROL_LEG_DSI_ENABLE(TEGRA_DSI_DISABLE),
		DSI_POWER_CONTROL);
	/* stabilization delay */
	udelay(300);

	tegra_dsi_writel(dsi,
		DSI_POWER_CONTROL_LEG_DSI_ENABLE(TEGRA_DSI_ENABLE),
		DSI_POWER_CONTROL);
	/* stabilization delay */
	udelay(300);

	/* dsi HW does not clear host trigger bit automatically
	 * on dsi interface disable if host fifo is empty or in mid
	 * of host transmission
	 */
	trigger = tegra_dsi_readl(dsi, DSI_TRIGGER);
	if (trigger)
		tegra_dsi_writel(dsi, 0x0, DSI_TRIGGER);

#undef DSI_IDLE_TIMEOUT
}

static void tegra_dsi_stop_dc_stream(struct tegra_dc *dc,
					struct tegra_dc_dsi_data *dsi)
{
	tegra_dc_get(dc);

	tegra_dc_writel(dc, DISP_CTRL_MODE_STOP, DC_CMD_DISPLAY_COMMAND);
	tegra_dc_writel(dc, 0, DC_DISP_DISP_WIN_OPTIONS);
	tegra_dc_writel(dc, GENERAL_UPDATE, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, GENERAL_ACT_REQ , DC_CMD_STATE_CONTROL);

	tegra_dc_put(dc);

	dsi->status.dc_stream = DSI_DC_STREAM_DISABLE;
}

/* wait for frame end interrupt or (timeout_n_frames * 1 frame duration)
 * whichever happens to occur first
 */
static int tegra_dsi_wait_frame_end(struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi,
				u32 timeout_n_frames)
{
	int val;
	long timeout;
	u32 frame_period = DIV_ROUND_UP(S_TO_MS(1), dsi->info.refresh_rate);
	struct tegra_dc_mode mode = dc->mode;
	u32 line_period = DIV_ROUND_UP(
				MS_TO_US(frame_period),
				mode.v_sync_width + mode.v_back_porch +
				mode.v_active + mode.v_front_porch);

	if (timeout_n_frames < 2)
		dev_WARN(&dc->ndev->dev,
		"dsi: to stop at next frame give at least 2 frame delay\n");

	INIT_COMPLETION(dc->frame_end_complete);

	tegra_dc_get(dc);

	tegra_dc_flush_interrupt(dc, FRAME_END_INT);
	/* unmask frame end interrupt */
	val = tegra_dc_unmask_interrupt(dc, FRAME_END_INT);

	timeout = wait_for_completion_interruptible_timeout(
			&dc->frame_end_complete,
			msecs_to_jiffies(timeout_n_frames * frame_period));

	/* reinstate interrupt mask */
	tegra_dc_writel(dc, val, DC_CMD_INT_MASK);

	/* wait for v_ref_to_sync no. of lines after frame end interrupt */
	udelay(mode.v_ref_to_sync * line_period);

	tegra_dc_put(dc);

	return timeout;
}

static void tegra_dsi_stop_dc_stream_at_frame_end(struct tegra_dc *dc,
						struct tegra_dc_dsi_data *dsi,
						u32 timeout_n_frames)
{
	tegra_dsi_stop_dc_stream(dc, dsi);

	tegra_dsi_wait_frame_end(dc, dsi, timeout_n_frames);

	tegra_dsi_soft_reset(dsi);

	tegra_dsi_reset_underflow_overflow(dsi);
}

static void tegra_dc_gpio_to_spio(struct tegra_dc_dsi_data *dsi, unsigned gpio)
{
	int err;

	/* convert to spio */
	err = gpio_request(gpio, "temp_request");
	if (err < 0) {
		dev_err(&dsi->dc->ndev->dev,
			"dsi: %s: gpio request failed %d\n", __func__, err);
		return;
	}
	gpio_free(gpio);
}

static void tegra_dsi_start_dc_stream(struct tegra_dc *dc,
					struct tegra_dc_dsi_data *dsi)
{
	u32 val;

	tegra_dc_get(dc);
	tegra_dvfs_set_rate(dc->clk, dc->mode.pclk);

	tegra_dc_writel(dc, DSI_ENABLE, DC_DISP_DISP_WIN_OPTIONS);

	/* TODO: clean up */
	tegra_dc_writel(dc, PW0_ENABLE | PW1_ENABLE | PW2_ENABLE | PW3_ENABLE |
			PW4_ENABLE | PM0_ENABLE | PM1_ENABLE,
			DC_CMD_DISPLAY_POWER_CONTROL);

	/* Configure one-shot mode or continuous mode */
	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE) {
		/* disable LSPI/LCD_DE output */
		val = PIN_OUTPUT_LSPI_OUTPUT_DIS;
		tegra_dc_writel(dc, val, DC_COM_PIN_OUTPUT_ENABLE3);

		/* enable MSF & set MSF polarity */
		val = MSF_ENABLE | MSF_LSPI;
		if (!dsi->info.te_polarity_low)
			val |= MSF_POLARITY_HIGH;
		else
			val |= MSF_POLARITY_LOW;
		tegra_dc_writel(dc, val, DC_CMD_DISPLAY_COMMAND_OPTION0);

		/* set non-continuous mode */
		tegra_dc_writel(dc, DISP_CTRL_MODE_NC_DISPLAY,
						DC_CMD_DISPLAY_COMMAND);

		tegra_dc_writel(dc, GENERAL_UPDATE, DC_CMD_STATE_CONTROL);
		tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);

		if (dsi->info.te_gpio)
			tegra_dc_gpio_to_spio(dsi, dsi->info.te_gpio);
	} else {
		/* set continuous mode */
		tegra_dc_writel(dc, DISP_CTRL_MODE_C_DISPLAY,
						DC_CMD_DISPLAY_COMMAND);
		tegra_dc_writel(dc, GENERAL_UPDATE, DC_CMD_STATE_CONTROL);
		tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);
	}

	tegra_dc_put(dc);

	dsi->status.dc_stream = DSI_DC_STREAM_ENABLE;
}

static void tegra_dsi_set_dc_clk(struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi)
{
	u32 shift_clk_div_register;
	u32 val;

	/* formula: (dsi->shift_clk_div - 1) * 2 */
	shift_clk_div_register = (dsi->shift_clk_div.mul -
				dsi->shift_clk_div.div) * 2 /
				dsi->shift_clk_div.div;

	if (tegra_platform_is_fpga()) {
		shift_clk_div_register = 1;
		if (dsi->info.ganged_type)
			shift_clk_div_register = 0;
	}

	tegra_dc_get(dc);

	val = PIXEL_CLK_DIVIDER_PCD1 |
		SHIFT_CLK_DIVIDER(shift_clk_div_register + 2);

	/* SW WAR for bug 1045373. To make the shift clk dividor effect under
	 * all circumstances, write N+2 to SHIFT_CLK_DIVIDER and activate it.
	 * After 2us delay, write the target values to it. */
#if defined(CONFIG_ARCH_TEGRA_14x_SOC)
	tegra_dc_writel(dc, val, DC_DISP_DISP_CLOCK_CONTROL);
	tegra_dc_writel(dc, GENERAL_UPDATE, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);

	udelay(2);
#endif

	/* TODO: find out if PCD3 option is required */
	val = PIXEL_CLK_DIVIDER_PCD1 |
		SHIFT_CLK_DIVIDER(shift_clk_div_register);

	tegra_dc_writel(dc, val, DC_DISP_DISP_CLOCK_CONTROL);

	tegra_dc_put(dc);
}

static void tegra_dsi_set_dsi_clk(struct tegra_dc *dc,
			struct tegra_dc_dsi_data *dsi, u32 clk)
{
	u32 rm;
	u32 pclk_khz;

	/* Round up to MHz */
	rm = clk % 1000;
	if (rm != 0)
		clk -= rm;

	/* Set up pixel clock */
	pclk_khz = (clk * dsi->shift_clk_div.div) /
				dsi->shift_clk_div.mul;

	dc->mode.pclk = pclk_khz * 1000;

	dc->shift_clk_div.mul = dsi->shift_clk_div.mul;
	dc->shift_clk_div.div = dsi->shift_clk_div.div;

	/* TODO: Define one shot work delay in board file. */
	/* Since for one-shot mode, refresh rate is usually set larger than
	 * expected refresh rate, it needs at least 3 frame period. Less
	 * delay one shot work is, more powering saving we have. */
	dc->one_shot_delay_ms = 4 *
			DIV_ROUND_UP(S_TO_MS(1), dsi->info.refresh_rate);

	tegra_dsi_setup_clk(dc, dsi);
	tegra_dsi_reset_deassert(dsi);

	dsi->current_dsi_clk_khz =
			clk_get_rate(dsi->dsi_clk[0]) / 1000;
	dsi->current_bit_clk_ps =  DIV_ROUND_CLOSEST((1000 * 1000 * 1000),
					(dsi->current_dsi_clk_khz * 2));
}

static void tegra_dsi_hs_clk_out_enable(struct tegra_dc_dsi_data *dsi)
{
	u32 val;

	val = tegra_dsi_readl(dsi, DSI_CONTROL);
	val &= ~DSI_CONTROL_HS_CLK_CTRL(1);

	if (dsi->info.video_clock_mode == TEGRA_DSI_VIDEO_CLOCK_CONTINUOUS) {
		val |= DSI_CONTROL_HS_CLK_CTRL(CONTINUOUS);
		dsi->status.clk_mode = DSI_PHYCLK_CONTINUOUS;
	} else {
		val |= DSI_CONTROL_HS_CLK_CTRL(TX_ONLY);
		dsi->status.clk_mode = DSI_PHYCLK_TX_ONLY;
	}
	tegra_dsi_writel(dsi, val, DSI_CONTROL);

	val = tegra_dsi_readl(dsi, DSI_HOST_DSI_CONTROL);
	val &= ~DSI_HOST_DSI_CONTROL_HIGH_SPEED_TRANS(1);
	val |= DSI_HOST_DSI_CONTROL_HIGH_SPEED_TRANS(TEGRA_DSI_HIGH);
	tegra_dsi_writel(dsi, val, DSI_HOST_DSI_CONTROL);

	dsi->status.clk_out = DSI_PHYCLK_OUT_EN;
}

static void tegra_dsi_hs_clk_out_enable_in_lp(struct tegra_dc_dsi_data *dsi)
{
	u32 val;
	tegra_dsi_hs_clk_out_enable(dsi);

	val = tegra_dsi_readl(dsi, DSI_HOST_DSI_CONTROL);
	val &= ~DSI_HOST_DSI_CONTROL_HIGH_SPEED_TRANS(1);
	val |= DSI_HOST_DSI_CONTROL_HIGH_SPEED_TRANS(TEGRA_DSI_LOW);
	tegra_dsi_writel(dsi, val, DSI_HOST_DSI_CONTROL);
}

static void tegra_dsi_hs_clk_out_disable(struct tegra_dc *dc,
						struct tegra_dc_dsi_data *dsi)
{
	u32 val;

	if (dsi->status.dc_stream == DSI_DC_STREAM_ENABLE)
		tegra_dsi_stop_dc_stream_at_frame_end(dc, dsi, 2);

	tegra_dsi_writel(dsi, TEGRA_DSI_DISABLE, DSI_POWER_CONTROL);
	/* stabilization delay */
	udelay(300);

	val = tegra_dsi_readl(dsi, DSI_HOST_DSI_CONTROL);
	val &= ~DSI_HOST_DSI_CONTROL_HIGH_SPEED_TRANS(1);
	val |= DSI_HOST_DSI_CONTROL_HIGH_SPEED_TRANS(TEGRA_DSI_LOW);
	tegra_dsi_writel(dsi, val, DSI_HOST_DSI_CONTROL);

	tegra_dsi_writel(dsi, TEGRA_DSI_ENABLE, DSI_POWER_CONTROL);
	/* stabilization delay */
	udelay(300);

	dsi->status.clk_mode = DSI_PHYCLK_NOT_INIT;
	dsi->status.clk_out = DSI_PHYCLK_OUT_DIS;
}

static void tegra_dsi_set_control_reg_lp(struct tegra_dc_dsi_data *dsi)
{
	u32 dsi_control;
	u32 host_dsi_control;
	u32 max_threshold;

	dsi_control = dsi->dsi_control_val | DSI_CTRL_HOST_DRIVEN;
	host_dsi_control = HOST_DSI_CTRL_COMMON |
			HOST_DSI_CTRL_HOST_DRIVEN |
			DSI_HOST_DSI_CONTROL_HIGH_SPEED_TRANS(TEGRA_DSI_LOW);
	max_threshold = DSI_MAX_THRESHOLD_MAX_THRESHOLD(DSI_HOST_FIFO_DEPTH);

	tegra_dsi_writel(dsi, max_threshold, DSI_MAX_THRESHOLD);
	tegra_dsi_writel(dsi, dsi_control, DSI_CONTROL);
	tegra_dsi_writel(dsi, host_dsi_control, DSI_HOST_DSI_CONTROL);

	dsi->status.driven = DSI_DRIVEN_MODE_HOST;
	dsi->status.clk_burst = DSI_CLK_BURST_NOT_INIT;
	dsi->status.vtype = DSI_VIDEO_TYPE_NOT_INIT;
}

static void tegra_dsi_set_control_reg_hs(struct tegra_dc_dsi_data *dsi,
						u8 driven_mode)
{
	u32 dsi_control;
	u32 host_dsi_control;
	u32 max_threshold;
	u32 dcs_cmd;

	dsi_control = dsi->dsi_control_val;
	host_dsi_control = HOST_DSI_CTRL_COMMON;
	max_threshold = 0;
	dcs_cmd = 0;

	if (driven_mode == TEGRA_DSI_DRIVEN_BY_HOST) {
		dsi_control |= DSI_CTRL_HOST_DRIVEN;
		host_dsi_control |= HOST_DSI_CTRL_HOST_DRIVEN;
		max_threshold =
			DSI_MAX_THRESHOLD_MAX_THRESHOLD(DSI_HOST_FIFO_DEPTH);
		dsi->status.driven = DSI_DRIVEN_MODE_HOST;
	} else {
		dsi_control |= DSI_CTRL_DC_DRIVEN;
		host_dsi_control |= HOST_DSI_CTRL_DC_DRIVEN;
		max_threshold =
			DSI_MAX_THRESHOLD_MAX_THRESHOLD(DSI_VIDEO_FIFO_DEPTH);
		dsi->status.driven = DSI_DRIVEN_MODE_DC;

		if (dsi->info.video_data_type ==
			TEGRA_DSI_VIDEO_TYPE_COMMAND_MODE) {
			dsi_control |= DSI_CTRL_CMD_MODE;
			dcs_cmd = DSI_DCS_CMDS_LT5_DCS_CMD(
				DSI_WRITE_MEMORY_START)|
				DSI_DCS_CMDS_LT3_DCS_CMD(
				DSI_WRITE_MEMORY_CONTINUE);
			dsi->status.vtype = DSI_VIDEO_TYPE_CMD_MODE;
		} else {
			dsi_control |= DSI_CTRL_VIDEO_MODE;
			dsi->status.vtype = DSI_VIDEO_TYPE_VIDEO_MODE;
		}
	}

	tegra_dsi_writel(dsi, max_threshold, DSI_MAX_THRESHOLD);
	tegra_dsi_writel(dsi, dcs_cmd, DSI_DCS_CMDS);
	tegra_dsi_writel(dsi, dsi_control, DSI_CONTROL);
	tegra_dsi_writel(dsi, host_dsi_control, DSI_HOST_DSI_CONTROL);
}

static void tegra_dsi_pad_disable(struct tegra_dc_dsi_data *dsi)
{
	u32 val;

	if (dsi->info.controller_vs == DSI_VS_1) {
		val = tegra_dsi_readl(dsi, DSI_PAD_CONTROL_0_VS1);
		val &= ~(DSI_PAD_CONTROL_0_VS1_PAD_PDIO(0xf) |
			DSI_PAD_CONTROL_0_VS1_PAD_PDIO_CLK(0x1) |
			DSI_PAD_CONTROL_0_VS1_PAD_PULLDN_ENAB(0xf) |
			DSI_PAD_CONTROL_0_VS1_PAD_PULLDN_CLK_ENAB(0x1));
		val |= DSI_PAD_CONTROL_0_VS1_PAD_PDIO(0xf) |
			DSI_PAD_CONTROL_0_VS1_PAD_PDIO_CLK
						(TEGRA_DSI_PAD_DISABLE) |
			DSI_PAD_CONTROL_0_VS1_PAD_PULLDN_ENAB(0xf) |
			DSI_PAD_CONTROL_0_VS1_PAD_PULLDN_CLK_ENAB
						(TEGRA_DSI_PAD_DISABLE);
		tegra_dsi_writel(dsi, val, DSI_PAD_CONTROL_0_VS1);
	} else {
		val = tegra_dsi_readl(dsi, DSI_PAD_CONTROL);
		val &= ~(DSI_PAD_CONTROL_PAD_PDIO(0x3) |
			DSI_PAD_CONTROL_PAD_PDIO_CLK(0x1) |
			DSI_PAD_CONTROL_PAD_PULLDN_ENAB(0x1));
		val |= DSI_PAD_CONTROL_PAD_PDIO(0x3) |
			DSI_PAD_CONTROL_PAD_PDIO_CLK(TEGRA_DSI_PAD_DISABLE) |
			DSI_PAD_CONTROL_PAD_PULLDN_ENAB(TEGRA_DSI_PAD_DISABLE);
		tegra_dsi_writel(dsi, val, DSI_PAD_CONTROL);
	}
}

static void tegra_dsi_pad_enable(struct tegra_dc_dsi_data *dsi)
{
	u32 val;

	if (dsi->info.controller_vs == DSI_VS_1) {
		val = tegra_dsi_readl(dsi, DSI_PAD_CONTROL_0_VS1);
		val &= ~(DSI_PAD_CONTROL_0_VS1_PAD_PDIO(0xf) |
			DSI_PAD_CONTROL_0_VS1_PAD_PDIO_CLK(0x1) |
			DSI_PAD_CONTROL_0_VS1_PAD_PULLDN_ENAB(0xf) |
			DSI_PAD_CONTROL_0_VS1_PAD_PULLDN_CLK_ENAB(0x1));
		val |= DSI_PAD_CONTROL_0_VS1_PAD_PDIO(TEGRA_DSI_PAD_ENABLE) |
			DSI_PAD_CONTROL_0_VS1_PAD_PDIO_CLK(
						TEGRA_DSI_PAD_ENABLE) |
			DSI_PAD_CONTROL_0_VS1_PAD_PULLDN_ENAB(
						TEGRA_DSI_PAD_ENABLE) |
			DSI_PAD_CONTROL_0_VS1_PAD_PULLDN_CLK_ENAB(
						TEGRA_DSI_PAD_ENABLE);
		tegra_dsi_writel(dsi, val, DSI_PAD_CONTROL_0_VS1);
	} else {
		val = tegra_dsi_readl(dsi, DSI_PAD_CONTROL);
		val &= ~(DSI_PAD_CONTROL_PAD_PDIO(0x3) |
			DSI_PAD_CONTROL_PAD_PDIO_CLK(0x1) |
			DSI_PAD_CONTROL_PAD_PULLDN_ENAB(0x1));
		val |= DSI_PAD_CONTROL_PAD_PDIO(TEGRA_DSI_PAD_ENABLE) |
			DSI_PAD_CONTROL_PAD_PDIO_CLK(TEGRA_DSI_PAD_ENABLE) |
			DSI_PAD_CONTROL_PAD_PULLDN_ENAB(TEGRA_DSI_PAD_ENABLE);
		tegra_dsi_writel(dsi, val, DSI_PAD_CONTROL);
	}
}

static void __maybe_unused
tegra_dsi_mipi_calibration_status(struct tegra_dc_dsi_data *dsi)
{
	u32 val = 0;
	u32 timeout = 0;
	/* Start calibration */
	val = tegra_mipi_cal_read(dsi->mipi_cal,
		MIPI_CAL_MIPI_CAL_CTRL_0);
	val |= (MIPI_CAL_STARTCAL(0x1));
	tegra_mipi_cal_write(dsi->mipi_cal, val,
		MIPI_CAL_MIPI_CAL_CTRL_0);

	for (timeout = MIPI_DSI_AUTOCAL_TIMEOUT_USEC;
			timeout; timeout -= 100) {
		val = tegra_mipi_cal_read(dsi->mipi_cal,
		MIPI_CAL_CIL_MIPI_CAL_STATUS_0);
		if (!(val & MIPI_CAL_ACTIVE(0x1)) &&
			(val & MIPI_AUTO_CAL_DONE(0x1))) {
				dev_info(&dsi->dc->ndev->dev, "DSI pad calibration done\n");
				break;
		}
		usleep_range(10, 100);
	}
	if (timeout <= 0)
		dev_info(&dsi->dc->ndev->dev, "DSI calibration timed out\n");
}

#ifdef CONFIG_ARCH_TEGRA_12x_SOC
static void tegra_dsi_mipi_calibration_12x(struct tegra_dc_dsi_data *dsi)
{
	u32 val;
	struct clk *clk72mhz = NULL;

	clk72mhz = clk_get_sys("clk72mhz", NULL);
	if (IS_ERR_OR_NULL(clk72mhz)) {
		dev_err(&dsi->dc->ndev->dev, "dsi: can't get clk72mhz clock\n");
		return;
	}
	clk_prepare_enable(clk72mhz);

	/* Calibration settings begin */
	val = tegra_mipi_cal_read(dsi->mipi_cal,
			MIPI_CAL_MIPI_BIAS_PAD_CFG1_0);
	val &= ~PAD_DRIV_UP_REF(0x7);
	val |= PAD_DRIV_UP_REF(0x3);
	tegra_mipi_cal_write(dsi->mipi_cal, val,
			MIPI_CAL_MIPI_BIAS_PAD_CFG1_0);

	val = (DSI_PAD_SLEWUPADJ(0x7) | DSI_PAD_SLEWDNADJ(0x7) |
		DSI_PAD_LPUPADJ(0x1) | DSI_PAD_LPDNADJ(0x1) |
		DSI_PAD_OUTADJCLK(0x0));
	tegra_dsi_writel(dsi, val, DSI_PAD_CONTROL_2_VS1);

	val = tegra_dsi_readl(dsi, DSI_PAD_CONTROL_3_VS1);
	val |= (DSI_PAD_PREEMP_PU_CLK(0x3) |
		   DSI_PAD_PREEMP_PD(0x3) | DSI_PAD_PREEMP_PU(0x3));
	tegra_dsi_writel(dsi, val, DSI_PAD_CONTROL_3_VS1);

	/* Calibrate DSI 0 */
	if (dsi->info.ganged_type ||
		dsi->info.dsi_instance == DSI_INSTANCE_0) {
		val = MIPI_CAL_OVERIDEDSIA(0x0) |
			MIPI_CAL_SELDSIA(0x1) |
			MIPI_CAL_HSPDOSDSIA(0x0) |
			MIPI_CAL_HSPUOSDSIA(0x0) |
			MIPI_CAL_TERMOSDSIA(0x0);
		tegra_mipi_cal_write(dsi->mipi_cal, val,
			MIPI_CAL_DSIA_MIPI_CAL_CONFIG_0);
		tegra_mipi_cal_write(dsi->mipi_cal, val,
			MIPI_CAL_DSIB_MIPI_CAL_CONFIG_0);

		val = (MIPI_CAL_CLKSELDSIA(0x1) |
				MIPI_CAL_HSCLKPDOSDSIA(0x1) |
				MIPI_CAL_HSCLKPUOSDSIA(0x2));
		tegra_mipi_cal_write(dsi->mipi_cal, val,
			MIPI_CAL_DSIA_MIPI_CAL_CONFIG_2_0);
		tegra_mipi_cal_write(dsi->mipi_cal, val,
			MIPI_CAL_DSIB_MIPI_CAL_CONFIG_2_0);

		/* Deselect PAD C */
		val = tegra_mipi_cal_read(dsi->mipi_cal,
			MIPI_CAL_CILC_MIPI_CAL_CONFIG_2_0);
		val &= ~(MIPI_CAL_SELDSIC(0x1));
		tegra_mipi_cal_write(dsi->mipi_cal, val,
			MIPI_CAL_CILC_MIPI_CAL_CONFIG_2_0);

		/* Deselect PAD D */
		val = tegra_mipi_cal_read(dsi->mipi_cal,
			MIPI_CAL_CILD_MIPI_CAL_CONFIG_2_0);
		val &= ~(MIPI_CAL_SELDSID(0x1));
		tegra_mipi_cal_write(dsi->mipi_cal, val,
			MIPI_CAL_CILD_MIPI_CAL_CONFIG_2_0);

		val = MIPI_CAL_NOISE_FLT(0xa) |
			  MIPI_CAL_PRESCALE(0x2) |
			  MIPI_CAL_CLKEN_OVR(0x1) |
			  MIPI_CAL_AUTOCAL_EN(0x0);
		tegra_mipi_cal_write(dsi->mipi_cal, val,
			MIPI_CAL_MIPI_CAL_CTRL_0);

		tegra_dsi_mipi_calibration_status(dsi);
	}
	/* Calibrate DSI 1 */
	if (dsi->info.ganged_type ||
		dsi->info.dsi_instance == DSI_INSTANCE_1) {
		val = MIPI_CAL_OVERIDEC(0x0) |
			MIPI_CAL_SELC(0x1) |
			MIPI_CAL_HSPDOSC(0x0) |
			MIPI_CAL_HSPUOSC(0x0) |
			MIPI_CAL_TERMOSC(0x0);
		tegra_mipi_cal_write(dsi->mipi_cal, val,
			MIPI_CAL_CILC_MIPI_CAL_CONFIG_0);
		tegra_mipi_cal_write(dsi->mipi_cal, val,
			MIPI_CAL_CILD_MIPI_CAL_CONFIG_0);

		val = (MIPI_CAL_CLKSELDSIA(0x1) |
				MIPI_CAL_HSCLKPDOSDSIA(0x1) |
				MIPI_CAL_HSCLKPUOSDSIA(0x2));
		tegra_mipi_cal_write(dsi->mipi_cal, val,
			MIPI_CAL_CILC_MIPI_CAL_CONFIG_2_0);
		tegra_mipi_cal_write(dsi->mipi_cal, val,
			MIPI_CAL_CILD_MIPI_CAL_CONFIG_2_0);

		/* Deselect PAD A */
		val = tegra_mipi_cal_read(dsi->mipi_cal,
			MIPI_CAL_DSIA_MIPI_CAL_CONFIG_2_0);
		val &= ~(MIPI_CAL_SELDSIC(0x1));
		tegra_mipi_cal_write(dsi->mipi_cal, val,
			MIPI_CAL_DSIA_MIPI_CAL_CONFIG_2_0);

		/* Deselect PAD B */
		val = tegra_mipi_cal_read(dsi->mipi_cal,
			MIPI_CAL_DSIB_MIPI_CAL_CONFIG_2_0);
		val &= ~(MIPI_CAL_SELDSID(0x1));
		tegra_mipi_cal_write(dsi->mipi_cal, val,
			MIPI_CAL_DSIB_MIPI_CAL_CONFIG_2_0);

		val = MIPI_CAL_NOISE_FLT(0xa) |
			  MIPI_CAL_PRESCALE(0x2) |
			  MIPI_CAL_CLKEN_OVR(0x1) |
			  MIPI_CAL_AUTOCAL_EN(0x0);
		tegra_mipi_cal_write(dsi->mipi_cal, val,
			MIPI_CAL_MIPI_CAL_CTRL_0);

		tegra_dsi_mipi_calibration_status(dsi);
	}

	clk_disable_unprepare(clk72mhz);
}
#endif

#ifdef CONFIG_ARCH_TEGRA_14x_SOC
void tegra_dsi_mipi_calibration_14x(struct tegra_dc_dsi_data *dsi)
{
	u32 val;
	struct clk *clk72mhz = NULL;

	clk72mhz = clk_get_sys("clk72mhz", NULL);
	if (IS_ERR_OR_NULL(clk72mhz)) {
		dev_err(&dsi->dc->ndev->dev, "dsi: can't get clk72mhz clock\n");
		return;
	}
	clk_prepare_enable(clk72mhz);

	tegra_mipi_cal_write(dsi->mipi_cal,
			PAD_DRIV_DN_REF(0x2),
			MIPI_CAL_MIPI_BIAS_PAD_CFG1_0);

	val = (DSI_PAD_SLEWUPADJ(0x7) | DSI_PAD_SLEWDNADJ(0x7) |
		     DSI_PAD_LPUPADJ(0x1) | DSI_PAD_LPDNADJ(0x1));
	tegra_dsi_writel(dsi, val, DSI_PAD_CONTROL_2_VS1);

	val = (DSI_PAD_PREEMP_PD(0x3) | DSI_PAD_PREEMP_PU(0x3));
	tegra_dsi_writel(dsi, val, DSI_PAD_CONTROL_3_VS1);

	val = MIPI_CAL_HSCLKPDOSDSIA(0x2) |
		MIPI_CAL_HSCLKPUOSDSIA(0x2);
	tegra_mipi_cal_write(dsi->mipi_cal, val,
		MIPI_CAL_DSIA_MIPI_CAL_CONFIG_2_0);
	tegra_mipi_cal_write(dsi->mipi_cal, val,
		MIPI_CAL_DSIB_MIPI_CAL_CONFIG_2_0);

	val = MIPI_CAL_OVERIDEDSIA(0x0) |
		MIPI_CAL_SELDSIA(0x1) |
		MIPI_CAL_HSPDOSDSIA(0x0) |
		MIPI_CAL_HSPUOSDSIA(0x0) |
		MIPI_CAL_TERMOSDSIA(0x0);
	tegra_mipi_cal_write(dsi->mipi_cal, val,
		MIPI_CAL_DSIA_MIPI_CAL_CONFIG_0);
	tegra_mipi_cal_write(dsi->mipi_cal, val,
		MIPI_CAL_DSIB_MIPI_CAL_CONFIG_0);

	val = MIPI_CAL_NOISE_FLT(0xa) |
		  MIPI_CAL_PRESCALE(0x2) |
		  MIPI_CAL_CLKEN_OVR(0x1) |
		  MIPI_CAL_AUTOCAL_EN(0x0);
	tegra_mipi_cal_write(dsi->mipi_cal, val,
		MIPI_CAL_MIPI_CAL_CTRL_0);

	tegra_dsi_mipi_calibration_status(dsi);

	clk_disable_unprepare(clk72mhz);
}
#endif

#ifdef CONFIG_ARCH_TEGRA_11x_SOC
static void tegra_dsi_mipi_calibration_11x(struct tegra_dc_dsi_data *dsi)
{
	u32 val;
	/* Calibration settings begin */
	val = (DSI_PAD_SLEWUPADJ(0x7) | DSI_PAD_SLEWDNADJ(0x7) |
		DSI_PAD_LPUPADJ(0x1) | DSI_PAD_LPDNADJ(0x1) |
		DSI_PAD_OUTADJCLK(0x0));
	tegra_dsi_writel(dsi, val, DSI_PAD_CONTROL_2_VS1);

	/* Calibrate DSI 0 */
	if (dsi->info.ganged_type ||
		dsi->info.dsi_instance == DSI_INSTANCE_0) {
		val = tegra_mipi_cal_read(dsi->mipi_cal,
			MIPI_CAL_DSIA_MIPI_CAL_CONFIG_0);
		val = MIPI_CAL_OVERIDEDSIA(0x0) |
			MIPI_CAL_SELDSIA(0x1) |
			MIPI_CAL_HSPDOSDSIA(0x0) |
			MIPI_CAL_HSPUOSDSIA(0x4) |
			MIPI_CAL_TERMOSDSIA(0x5);
		tegra_mipi_cal_write(dsi->mipi_cal, val,
			MIPI_CAL_DSIA_MIPI_CAL_CONFIG_0);
		tegra_mipi_cal_write(dsi->mipi_cal, val,
			MIPI_CAL_DSIB_MIPI_CAL_CONFIG_0);

		/* Deselect PAD C */
		val = tegra_mipi_cal_read(dsi->mipi_cal,
			MIPI_CAL_DSIC_MIPI_CAL_CONFIG_0);
		val &= ~(MIPI_CAL_SELDSIC(0x1));
		tegra_mipi_cal_write(dsi->mipi_cal, val,
			MIPI_CAL_DSIC_MIPI_CAL_CONFIG_0);

		/* Deselect PAD D */
		val = tegra_mipi_cal_read(dsi->mipi_cal,
			MIPI_CAL_DSID_MIPI_CAL_CONFIG_0);
		val &= ~(MIPI_CAL_SELDSID(0x1));
		tegra_mipi_cal_write(dsi->mipi_cal, val,
			MIPI_CAL_DSID_MIPI_CAL_CONFIG_0);

		val = tegra_mipi_cal_read(dsi->mipi_cal,
			MIPI_CAL_MIPI_CAL_CTRL_0);
		val = MIPI_CAL_NOISE_FLT(0xa) |
			  MIPI_CAL_PRESCALE(0x2) |
			  MIPI_CAL_CLKEN_OVR(0x1) |
			  MIPI_CAL_AUTOCAL_EN(0x0);
		tegra_mipi_cal_write(dsi->mipi_cal, val,
			MIPI_CAL_MIPI_CAL_CTRL_0);

		tegra_dsi_mipi_calibration_status(dsi);
	}

	/* Calibrate DSI 1 */
	if (dsi->info.ganged_type ||
		dsi->info.dsi_instance == DSI_INSTANCE_1) {
		val = tegra_mipi_cal_read(dsi->mipi_cal,
			MIPI_CAL_DSIC_MIPI_CAL_CONFIG_0);
		val = MIPI_CAL_OVERIDEDSIC(0x0) |
			MIPI_CAL_SELDSIC(0x1) |
			MIPI_CAL_HSPDOSDSIC(0x0) |
			MIPI_CAL_HSPUOSDSIC(0x4) |
			MIPI_CAL_TERMOSDSIC(0x5);
		tegra_mipi_cal_write(dsi->mipi_cal, val,
			MIPI_CAL_DSIC_MIPI_CAL_CONFIG_0);
		tegra_mipi_cal_write(dsi->mipi_cal, val,
			MIPI_CAL_DSID_MIPI_CAL_CONFIG_0);

		/* Deselect PAD A */
		val = tegra_mipi_cal_read(dsi->mipi_cal,
			MIPI_CAL_DSIA_MIPI_CAL_CONFIG_0);
		val &= ~(MIPI_CAL_SELDSIA(0x1));
		tegra_mipi_cal_write(dsi->mipi_cal, val,
			MIPI_CAL_DSIA_MIPI_CAL_CONFIG_0);

		/* Deselect PAD B */
		val = tegra_mipi_cal_read(dsi->mipi_cal,
			MIPI_CAL_DSIB_MIPI_CAL_CONFIG_0);
		val &= ~(MIPI_CAL_SELDSIB(0x1));
		tegra_mipi_cal_write(dsi->mipi_cal, val,
			MIPI_CAL_DSIB_MIPI_CAL_CONFIG_0);

		val = tegra_mipi_cal_read(dsi->mipi_cal,
			MIPI_CAL_MIPI_CAL_CTRL_0);
		val = MIPI_CAL_NOISE_FLT(0xa) |
			  MIPI_CAL_PRESCALE(0x2) |
			  MIPI_CAL_CLKEN_OVR(0x1) |
			  MIPI_CAL_AUTOCAL_EN(0x0);
		tegra_mipi_cal_write(dsi->mipi_cal, val,
			MIPI_CAL_MIPI_CAL_CTRL_0);

		tegra_dsi_mipi_calibration_status(dsi);
	}
}
#endif
static void tegra_dsi_pad_calibration(struct tegra_dc_dsi_data *dsi)
{
	if (!dsi->ulpm)
		tegra_dsi_pad_enable(dsi);
	else
		tegra_dsi_pad_disable(dsi);

	if (dsi->info.controller_vs == DSI_VS_1) {

		tegra_mipi_cal_init_hw(dsi->mipi_cal);

		tegra_mipi_cal_clk_enable(dsi->mipi_cal);

		tegra_mipi_cal_write(dsi->mipi_cal,
			MIPI_BIAS_PAD_E_VCLAMP_REF(0x1),
			MIPI_CAL_MIPI_BIAS_PAD_CFG0_0);
		tegra_mipi_cal_write(dsi->mipi_cal,
			PAD_PDVREG(0x0) | PAD_VCLAMP_LEVEL(0x0),
			MIPI_CAL_MIPI_BIAS_PAD_CFG2_0);
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
		tegra_dsi_mipi_calibration_11x(dsi);
#elif defined(CONFIG_ARCH_TEGRA_14x_SOC)
		tegra_dsi_mipi_calibration_14x(dsi);
#elif defined(CONFIG_ARCH_TEGRA_12x_SOC)
		tegra_dsi_mipi_calibration_12x(dsi);
#endif
		tegra_mipi_cal_clk_disable(dsi->mipi_cal);
	} else {
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
		u32 val = 0;

		val = tegra_dsi_readl(dsi, DSI_PAD_CONTROL);
		val &= ~(DSI_PAD_CONTROL_PAD_LPUPADJ(0x3) |
			DSI_PAD_CONTROL_PAD_LPDNADJ(0x3) |
			DSI_PAD_CONTROL_PAD_PREEMP_EN(0x1) |
			DSI_PAD_CONTROL_PAD_SLEWDNADJ(0x7) |
			DSI_PAD_CONTROL_PAD_SLEWUPADJ(0x7));

		val |= DSI_PAD_CONTROL_PAD_LPUPADJ(0x1) |
			DSI_PAD_CONTROL_PAD_LPDNADJ(0x1) |
			DSI_PAD_CONTROL_PAD_PREEMP_EN(0x1) |
			DSI_PAD_CONTROL_PAD_SLEWDNADJ(0x6) |
			DSI_PAD_CONTROL_PAD_SLEWUPADJ(0x6);

		tegra_dsi_writel(dsi, val, DSI_PAD_CONTROL);

		val = MIPI_CAL_TERMOSA(0x4);
		tegra_vi_csi_writel(val, CSI_CILA_MIPI_CAL_CONFIG_0);

		val = MIPI_CAL_TERMOSB(0x4);
		tegra_vi_csi_writel(val, CSI_CILB_MIPI_CAL_CONFIG_0);

		val = MIPI_CAL_HSPUOSD(0x3) | MIPI_CAL_HSPDOSD(0x4);
		tegra_vi_csi_writel(val, CSI_DSI_MIPI_CAL_CONFIG);

		val = PAD_DRIV_DN_REF(0x5) | PAD_DRIV_UP_REF(0x7);
		tegra_vi_csi_writel(val, CSI_MIPIBIAS_PAD_CONFIG);

		val = PAD_CIL_PDVREG(0x0);
		tegra_vi_csi_writel(val, CSI_CIL_PAD_CONFIG);
#endif
	}
}

static void tegra_dsi_panelB_enable(void)
{
	unsigned int val;

	val = readl(IO_ADDRESS(APB_MISC_GP_MIPI_PAD_CTRL_0));
	val |= DSIB_MODE_ENABLE;
	writel(val, (IO_ADDRESS(APB_MISC_GP_MIPI_PAD_CTRL_0)));
}

static int tegra_dsi_init_hw(struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi)
{
	u32 i;
	int err = 0;

	if (dsi->avdd_dsi_csi)
		err = regulator_enable(dsi->avdd_dsi_csi);
	if (WARN(err, "unable to enable regulator"))
		return err;
	/* stablization delay */
	mdelay(50);
	/* Enable DSI clocks */
	tegra_dsi_clk_enable(dsi);
	tegra_dsi_set_dsi_clk(dc, dsi, dsi->target_lp_clk_khz);

	/* Stop DC stream before configuring DSI registers
	 * to avoid visible glitches on panel during transition
	 * from bootloader to kernel driver
	 */
	tegra_dsi_stop_dc_stream_at_frame_end(dc, dsi, 2);

	tegra_dsi_writel(dsi,
		DSI_POWER_CONTROL_LEG_DSI_ENABLE(TEGRA_DSI_DISABLE),
		DSI_POWER_CONTROL);
	/* stabilization delay */
	udelay(300);

	if (dsi->info.dsi_instance || dsi->info.ganged_type)
		tegra_dsi_panelB_enable();

	tegra_dsi_set_phy_timing(dsi, DSI_LPHS_IN_LP_MODE);

	/* Initialize DSI registers */
	for (i = 0; i < ARRAY_SIZE(init_reg); i++)
		tegra_dsi_writel(dsi, 0, init_reg[i]);
	if (dsi->info.controller_vs == DSI_VS_1) {
		for (i = 0; i < ARRAY_SIZE(init_reg_vs1_ext); i++)
			tegra_dsi_writel(dsi, 0, init_reg_vs1_ext[i]);
	}

	tegra_dsi_pad_calibration(dsi);

	tegra_dsi_writel(dsi,
		DSI_POWER_CONTROL_LEG_DSI_ENABLE(TEGRA_DSI_ENABLE),
		DSI_POWER_CONTROL);
	/* stabilization delay */
	udelay(300);

	dsi->status.init = DSI_MODULE_INIT;
	dsi->status.lphs = DSI_LPHS_NOT_INIT;
	dsi->status.vtype = DSI_VIDEO_TYPE_NOT_INIT;
	dsi->status.driven = DSI_DRIVEN_MODE_NOT_INIT;
	dsi->status.clk_out = DSI_PHYCLK_OUT_DIS;
	dsi->status.clk_mode = DSI_PHYCLK_NOT_INIT;
	dsi->status.clk_burst = DSI_CLK_BURST_NOT_INIT;
	dsi->status.dc_stream = DSI_DC_STREAM_DISABLE;
	dsi->status.lp_op = DSI_LP_OP_NOT_INIT;

	return 0;
}

static int tegra_dsi_set_to_lp_mode(struct tegra_dc *dc,
			struct tegra_dc_dsi_data *dsi, u8 lp_op)
{
	int err;

	if (dsi->status.init != DSI_MODULE_INIT) {
		err = -EPERM;
		goto fail;
	}

	if (dsi->status.lphs == DSI_LPHS_IN_LP_MODE &&
			dsi->status.lp_op == lp_op)
		goto success;

	if (dsi->status.dc_stream == DSI_DC_STREAM_ENABLE)
		tegra_dsi_stop_dc_stream_at_frame_end(dc, dsi, 2);

	/* disable/enable hs clk according to enable_hs_clock_on_lp_cmd_mode */
	if ((dsi->status.clk_out == DSI_PHYCLK_OUT_EN) &&
		(!dsi->info.enable_hs_clock_on_lp_cmd_mode))
		tegra_dsi_hs_clk_out_disable(dc, dsi);

	dsi->target_lp_clk_khz = tegra_dsi_get_lp_clk_rate(dsi, lp_op);
	if (dsi->current_dsi_clk_khz != dsi->target_lp_clk_khz) {
		tegra_dsi_set_dsi_clk(dc, dsi, dsi->target_lp_clk_khz);
		tegra_dsi_set_timeout(dsi);
	}

	tegra_dsi_set_phy_timing(dsi, DSI_LPHS_IN_LP_MODE);

	tegra_dsi_set_control_reg_lp(dsi);

	if ((dsi->status.clk_out == DSI_PHYCLK_OUT_DIS) &&
		(dsi->info.enable_hs_clock_on_lp_cmd_mode))
		tegra_dsi_hs_clk_out_enable_in_lp(dsi);

	dsi->status.lphs = DSI_LPHS_IN_LP_MODE;
	dsi->status.lp_op = lp_op;
	dsi->driven_mode = TEGRA_DSI_DRIVEN_BY_HOST;
success:
	err = 0;
fail:
	return err;
}

static void tegra_dsi_ganged(struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi)
{
	u32 low_width = 0;
	u32 high_width = 0;
	u32 h_active = dc->out->modes->h_active;
	u32 val = 0;

	if (dsi->info.controller_vs < DSI_VS_1) {
		dev_err(&dc->ndev->dev, "dsi: ganged mode not"
		"supported with current controller version\n");
		return;
	}

	if (dsi->info.ganged_type ==
			TEGRA_DSI_GANGED_SYMMETRIC_LEFT_RIGHT) {
		/* DSI 0 */
		tegra_dsi_controller_writel(dsi,
			DSI_GANGED_MODE_START_POINTER(0),
			DSI_GANGED_MODE_START, DSI_INSTANCE_0);
		/* DSI 1 */
		tegra_dsi_controller_writel(dsi,
			DSI_GANGED_MODE_START_POINTER(h_active / 2),
			DSI_GANGED_MODE_START, DSI_INSTANCE_1);

		low_width = DIV_ROUND_UP(h_active, 2);
		high_width = h_active - low_width;
		val = DSI_GANGED_MODE_SIZE_VALID_LOW_WIDTH(low_width) |
			DSI_GANGED_MODE_SIZE_VALID_HIGH_WIDTH(high_width);

	} else if (dsi->info.ganged_type ==
			TEGRA_DSI_GANGED_SYMMETRIC_EVEN_ODD) {
		/* DSI 0 */
		tegra_dsi_controller_writel(dsi,
			DSI_GANGED_MODE_START_POINTER(0),
			DSI_GANGED_MODE_START, DSI_INSTANCE_0);
		/* DSI 1 */
		tegra_dsi_controller_writel(dsi,
			DSI_GANGED_MODE_START_POINTER(1),
			DSI_GANGED_MODE_START, DSI_INSTANCE_1);

		low_width = 0x1;
		high_width = 0x1;
		val = DSI_GANGED_MODE_SIZE_VALID_LOW_WIDTH(low_width) |
			DSI_GANGED_MODE_SIZE_VALID_HIGH_WIDTH(high_width);
	}

	tegra_dsi_writel(dsi, val, DSI_GANGED_MODE_SIZE);

	tegra_dsi_writel(dsi, DSI_GANGED_MODE_CONTROL_EN(TEGRA_DSI_ENABLE),
						DSI_GANGED_MODE_CONTROL);
}

static int tegra_dsi_set_to_hs_mode(struct tegra_dc *dc,
					struct tegra_dc_dsi_data *dsi,
					u8 driven_mode)
{
	int err;

	if (dsi->status.init != DSI_MODULE_INIT) {
		err = -EPERM;
		goto fail;
	}

	if (dsi->status.lphs == DSI_LPHS_IN_HS_MODE &&
		dsi->driven_mode == driven_mode)
		goto success;

	dsi->driven_mode = driven_mode;

	if (dsi->status.dc_stream == DSI_DC_STREAM_ENABLE)
		tegra_dsi_stop_dc_stream_at_frame_end(dc, dsi, 2);

	if ((dsi->status.clk_out == DSI_PHYCLK_OUT_EN) &&
		(!dsi->info.enable_hs_clock_on_lp_cmd_mode))
		tegra_dsi_hs_clk_out_disable(dc, dsi);

	if (dsi->current_dsi_clk_khz != dsi->target_hs_clk_khz) {
		tegra_dsi_set_dsi_clk(dc, dsi, dsi->target_hs_clk_khz);
		tegra_dsi_set_timeout(dsi);
	}

	tegra_dsi_set_phy_timing(dsi, DSI_LPHS_IN_HS_MODE);

	if (driven_mode == TEGRA_DSI_DRIVEN_BY_DC) {
		tegra_dsi_set_pkt_seq(dc, dsi);
		tegra_dsi_set_pkt_length(dc, dsi);
		tegra_dsi_set_sol_delay(dc, dsi);
		tegra_dsi_set_dc_clk(dc, dsi);
	}

	tegra_dsi_set_control_reg_hs(dsi, driven_mode);

	if (dsi->info.ganged_type)
		tegra_dsi_ganged(dc, dsi);

	if (dsi->status.clk_out == DSI_PHYCLK_OUT_DIS ||
		dsi->info.enable_hs_clock_on_lp_cmd_mode)
		tegra_dsi_hs_clk_out_enable(dsi);

	dsi->status.lphs = DSI_LPHS_IN_HS_MODE;
success:
	dsi->status.lp_op = DSI_LP_OP_NOT_INIT;
	err = 0;
fail:
	return err;
}

static bool tegra_dsi_write_busy(struct tegra_dc_dsi_data *dsi, u8 link_id)
{
	u32 timeout = 0;
	bool retVal = true;

	while (timeout <= DSI_MAX_COMMAND_DELAY_USEC) {
		if (!(DSI_TRIGGER_HOST_TRIGGER(0x1) &
			tegra_dsi_controller_readl(dsi,
					DSI_TRIGGER, link_id))) {
			retVal = false;
			break;
		}
		udelay(DSI_COMMAND_DELAY_STEPS_USEC);
		timeout += DSI_COMMAND_DELAY_STEPS_USEC;
	}

	return retVal;
}

static bool tegra_dsi_read_busy(struct tegra_dc_dsi_data *dsi, u8 link_id)
{
	u32 timeout = 0;
	bool retVal = true;

	while (timeout <  DSI_STATUS_POLLING_DURATION_USEC) {
		if (!(DSI_HOST_DSI_CONTROL_IMM_BTA(0x1) &
			tegra_dsi_controller_readl(dsi,
					DSI_HOST_DSI_CONTROL, link_id))) {
			retVal = false;
			break;
		}
		udelay(DSI_STATUS_POLLING_DELAY_USEC);
		timeout += DSI_STATUS_POLLING_DELAY_USEC;
	}

	return retVal;
}

static bool tegra_dsi_host_busy(struct tegra_dc_dsi_data *dsi, u8 link_id)
{
	int err = 0;

	if (tegra_dsi_write_busy(dsi, link_id)) {
		err = -EBUSY;
		dev_err(&dsi->dc->ndev->dev,
			"DSI trigger bit already set\n");
		goto fail;
	}

	if (tegra_dsi_read_busy(dsi, link_id)) {
		err = -EBUSY;
		dev_err(&dsi->dc->ndev->dev,
			"DSI immediate bta bit already set\n");
		goto fail;
	}
fail:
	return (err < 0 ? true : false);
}

static void tegra_dsi_reset_read_count(struct tegra_dc_dsi_data *dsi)
{
	u32 val;

	val = tegra_dsi_readl(dsi, DSI_STATUS);
	val &= DSI_STATUS_RD_FIFO_COUNT(0x1f);
	if (val) {
		dev_warn(&dsi->dc->ndev->dev,
			"DSI read count not zero, resetting\n");
		tegra_dsi_soft_reset(dsi);
	}
}

static struct dsi_status *tegra_dsi_save_state_switch_to_host_cmd_mode(
						struct tegra_dc_dsi_data *dsi,
						struct tegra_dc *dc,
						u8 lp_op)
{
	struct dsi_status *init_status = NULL;
	int err;

	if (dsi->status.init != DSI_MODULE_INIT ||
		dsi->status.lphs == DSI_LPHS_NOT_INIT) {
		err = -EPERM;
		goto fail;
	}

	init_status = kzalloc(sizeof(*init_status), GFP_KERNEL);
	if (!init_status)
		return ERR_PTR(-ENOMEM);

	*init_status = dsi->status;

	if (dsi->info.hs_cmd_mode_supported) {
		err = tegra_dsi_set_to_hs_mode(dc, dsi,
				TEGRA_DSI_DRIVEN_BY_HOST);
		if (err < 0) {
			dev_err(&dc->ndev->dev,
			"Switch to HS host mode failed\n");
			goto fail;
		}

		goto success;
	}

	if (dsi->status.lp_op != lp_op) {
		err = tegra_dsi_set_to_lp_mode(dc, dsi, lp_op);
		if (err < 0) {
			dev_err(&dc->ndev->dev,
			"DSI failed to go to LP mode\n");
			goto fail;
		}
	}
success:
	return init_status;
fail:
	kfree(init_status);
	return ERR_PTR(err);
}

static struct dsi_status *tegra_dsi_prepare_host_transmission(
				struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi,
				u8 lp_op)
{
	int i = 0;
	int err = 0;
	struct dsi_status *init_status;
	bool restart_dc_stream = false;

	if (dsi->status.init != DSI_MODULE_INIT ||
		dsi->ulpm) {
		err = -EPERM;
		goto fail;
	}

	if (dsi->status.dc_stream == DSI_DC_STREAM_ENABLE) {
		restart_dc_stream = true;
		tegra_dsi_stop_dc_stream_at_frame_end(dc, dsi, 2);
	}

	for (i = 0; i < dsi->max_instances; i++) {
		if (tegra_dsi_host_busy(dsi, i)) {
			tegra_dsi_soft_reset(dsi);
			if (tegra_dsi_host_busy(dsi, i)) {
				err = -EBUSY;
				dev_err(&dc->ndev->dev, "DSI host busy\n");
				goto fail;
			}
		}
	}

	if (lp_op == DSI_LP_OP_READ)
		tegra_dsi_reset_read_count(dsi);

	if (dsi->status.lphs == DSI_LPHS_NOT_INIT) {
		err = tegra_dsi_set_to_lp_mode(dc, dsi, lp_op);
		if (err < 0) {
			dev_err(&dc->ndev->dev, "Failed to config LP write\n");
			goto fail;
		}
	}

	init_status = tegra_dsi_save_state_switch_to_host_cmd_mode
					(dsi, dc, lp_op);
	if (IS_ERR_OR_NULL(init_status)) {
		err = PTR_ERR(init_status);
		dev_err(&dc->ndev->dev, "DSI state saving failed\n");
		goto fail;
	}

	if (restart_dc_stream)
		init_status->dc_stream = DSI_DC_STREAM_ENABLE;

	if (!tegra_cpu_is_asim() && DSI_USE_SYNC_POINTS)
		if (atomic_read(&dsi_syncpt_rst))
			tegra_dsi_syncpt_reset(dsi);

	return init_status;
fail:
	return ERR_PTR(err);
}

static int tegra_dsi_restore_state(struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi,
				struct dsi_status *init_status)
{
	int err = 0;

	if (init_status->lphs == DSI_LPHS_IN_LP_MODE) {
		err = tegra_dsi_set_to_lp_mode(dc, dsi, init_status->lp_op);
		if (err < 0) {
			dev_err(&dc->ndev->dev,
				"Failed to config LP mode\n");
			goto fail;
		}
		goto success;
	}

	if (init_status->lphs == DSI_LPHS_IN_HS_MODE) {
		u8 driven = (init_status->driven == DSI_DRIVEN_MODE_DC) ?
			TEGRA_DSI_DRIVEN_BY_DC : TEGRA_DSI_DRIVEN_BY_HOST;
		err = tegra_dsi_set_to_hs_mode(dc, dsi, driven);
		if (err < 0) {
			dev_err(&dc->ndev->dev, "Failed to config HS mode\n");
			goto fail;
		}
	}

	if (init_status->dc_stream == DSI_DC_STREAM_ENABLE)
		tegra_dsi_start_dc_stream(dc, dsi);
success:
fail:
	kfree(init_status);
	return err;
}

static int tegra_dsi_host_trigger(struct tegra_dc_dsi_data *dsi, u8 link_id)
{
	int status = 0;

	if (tegra_dsi_controller_readl(dsi, DSI_TRIGGER, link_id)) {
		status = -EBUSY;
		goto fail;
	}

	tegra_dsi_controller_writel(dsi,
		DSI_TRIGGER_HOST_TRIGGER(TEGRA_DSI_ENABLE),
				DSI_TRIGGER, link_id);

	if (!tegra_cpu_is_asim() && DSI_USE_SYNC_POINTS) {
		status = tegra_dsi_syncpt(dsi, link_id);
		if (status < 0) {
			dev_err(&dsi->dc->ndev->dev,
				"DSI syncpt for host trigger failed\n");
			goto fail;
		}
	} else {
		if (tegra_dsi_write_busy(dsi, link_id)) {
			status = -EBUSY;
			dev_err(&dsi->dc->ndev->dev,
				"Timeout waiting on write completion\n");
		}
	}

fail:
	return status;
}

static int _tegra_dsi_write_data(struct tegra_dc_dsi_data *dsi,
					struct tegra_dsi_cmd *cmd)
{
	u8 virtual_channel;
	u32 val;
	int err;
	u8 *pdata = cmd->pdata;
	u8 data_id = cmd->data_id;
	u16 data_len = cmd->sp_len_dly.data_len;

	err = 0;

	if (!dsi->info.ganged_type && cmd->link_id == TEGRA_DSI_LINK1) {
		dev_err(&dsi->dc->ndev->dev, "DSI invalid command\n");
		return -EINVAL;
	}

	virtual_channel = dsi->info.virtual_channel <<
						DSI_VIR_CHANNEL_BIT_POSITION;

	/* always use hw for ecc */
	val = (virtual_channel | data_id) << 0 |
			data_len << 8;
	tegra_dsi_controller_writel(dsi, val, DSI_WR_DATA, cmd->link_id);

	/* if pdata != NULL, pkt type is long pkt */
	if (pdata != NULL) {
		while (data_len) {
			if (data_len >= 4) {
				val = ((u32 *) pdata)[0];
				data_len -= 4;
				pdata += 4;
			} else {
				val = 0;
				memcpy(&val, pdata, data_len);
				pdata += data_len;
				data_len = 0;
			}
			tegra_dsi_controller_writel(dsi, val,
				DSI_WR_DATA, cmd->link_id);
		}
	}

	if (cmd->cmd_type != TEGRA_DSI_PACKET_VIDEO_VBLANK_CMD) {
		err = tegra_dsi_host_trigger(dsi, cmd->link_id);
		if (err < 0)
			dev_err(&dsi->dc->ndev->dev, "DSI host trigger failed\n");
	}

	return err;
}

static void tegra_dc_dsi_hold_host(struct tegra_dc *dc)
{
	struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);

	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_LP_MODE) {
		atomic_inc(&dsi->host_ref);
		tegra_dsi_host_resume(dc);
	}
}

static void tegra_dc_dsi_release_host(struct tegra_dc *dc)
{
	struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);

	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_LP_MODE) {
		atomic_dec(&dsi->host_ref);

		if (!atomic_read(&dsi->host_ref) &&
		    (dsi->status.dc_stream == DSI_DC_STREAM_ENABLE))
			schedule_delayed_work(&dsi->idle_work, dsi->idle_delay);
	}
}

static void tegra_dc_dsi_idle_work(struct work_struct *work)
{
	struct tegra_dc_dsi_data *dsi = container_of(
		to_delayed_work(work), struct tegra_dc_dsi_data, idle_work);

	if (dsi->dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_LP_MODE)
		tegra_dsi_host_suspend(dsi->dc);
}

static int tegra_dsi_write_data_nosync(struct tegra_dc *dc,
			struct tegra_dc_dsi_data *dsi,
			struct tegra_dsi_cmd *cmd, u8 delay_ms)
{
	int err = 0;
	struct dsi_status *init_status;

	init_status = tegra_dsi_prepare_host_transmission(
				dc, dsi, DSI_LP_OP_WRITE);
	if (IS_ERR_OR_NULL(init_status)) {
		err = PTR_ERR(init_status);
		dev_err(&dc->ndev->dev, "DSI host config failed\n");
		goto fail;
	}

	err = _tegra_dsi_write_data(dsi, cmd);
	mdelay(delay_ms);
fail:
	err = tegra_dsi_restore_state(dc, dsi, init_status);
	if (err < 0)
		dev_err(&dc->ndev->dev, "Failed to restore prev state\n");

	return err;
}

int tegra_dsi_write_data(struct tegra_dc *dc,
			struct tegra_dc_dsi_data *dsi,
			struct tegra_dsi_cmd *cmd, u8 delay_ms)
{
	int err;

	tegra_dc_io_start(dc);
	tegra_dc_dsi_hold_host(dc);

	err = tegra_dsi_write_data_nosync(dc, dsi, cmd, delay_ms);

	tegra_dc_dsi_release_host(dc);
	tegra_dc_io_end(dc);

	return err;
}

EXPORT_SYMBOL(tegra_dsi_write_data);

int tegra_dsi_start_host_cmd_v_blank_video(struct tegra_dc_dsi_data *dsi,
	struct tegra_dsi_cmd *cmd)
{
	struct tegra_dc *dc = dsi->dc;
	int err = 0;
	u32 val;

	if (!dsi->enabled) {
		dev_err(&dsi->dc->ndev->dev, "DSI controller suspended\n");
		return -EINVAL;
	}

	tegra_dc_io_start(dc);
	tegra_dc_dsi_hold_host(dc);
	val = (DSI_CMD_PKT_VID_ENABLE(1) | DSI_LINE_TYPE(4));
	tegra_dsi_writel(dsi, val, DSI_VID_MODE_CONTROL);
	_tegra_dsi_write_data(dsi, cmd);
	if (dsi->status.lphs != DSI_LPHS_IN_HS_MODE) {
		err = tegra_dsi_set_to_hs_mode(dc, dsi,
				TEGRA_DSI_DRIVEN_BY_DC);
		if (err < 0) {
			dev_err(&dc->ndev->dev,
				"dsi: not able to set to hs mode\n");
			goto fail;
		}
		tegra_dsi_start_dc_stream(dc, dsi);
		tegra_dsi_wait_frame_end(dc, dsi, 2);
		err = tegra_dsi_set_to_lp_mode(dc, dsi, DSI_LP_OP_WRITE);
		if (err < 0) {
			dev_err(&dc->ndev->dev,
				"dsi: not able to set to lp mode\n");
			goto fail;
		}
	}
 fail:
	tegra_dc_dsi_release_host(dc);
	tegra_dc_io_end(dc);

	return err;
}
EXPORT_SYMBOL(tegra_dsi_start_host_cmd_v_blank_video);

int tegra_dsi_end_host_cmd_v_blank_video(struct tegra_dc *dc,
					struct tegra_dc_dsi_data *dsi)
{
	if (!dsi->enabled) {
		dev_err(&dsi->dc->ndev->dev, "DSI controller suspended\n");
		return -EINVAL;
	}

	tegra_dc_io_start(dc);
	tegra_dsi_writel(dsi, 0, DSI_VID_MODE_CONTROL);
	tegra_dc_io_end(dc);

	return 0;
}
EXPORT_SYMBOL(tegra_dsi_end_host_cmd_v_blank_video);

static int tegra_dsi_send_panel_cmd(struct tegra_dc *dc,
					struct tegra_dc_dsi_data *dsi,
					struct tegra_dsi_cmd *cmd,
					u32 n_cmd)
{
#define DEFAULT_DELAY_MS 1
	u32 i;
	int err;
	u8 delay_ms;

	err = 0;
	for (i = 0; i < n_cmd; i++) {
		struct tegra_dsi_cmd *cur_cmd;
		cur_cmd = &cmd[i];

		/*
		 * Some Panels need reset midway in the command sequence.
		 */
		if (cur_cmd->cmd_type == TEGRA_DSI_GPIO_SET) {
			gpio_set_value(cur_cmd->sp_len_dly.gpio,
				       cur_cmd->data_id);
		} else if (cur_cmd->cmd_type == TEGRA_DSI_DELAY_MS) {
			mdelay(cur_cmd->sp_len_dly.delay_ms);
		} else if (cur_cmd->cmd_type == TEGRA_DSI_SEND_FRAME) {
				tegra_dsi_send_dc_frames(dc,
						dsi,
						cur_cmd->sp_len_dly.frame_cnt);
		} else if (cur_cmd->cmd_type ==
					TEGRA_DSI_PACKET_VIDEO_VBLANK_CMD) {
			tegra_dsi_start_host_cmd_v_blank_video(dsi, cur_cmd);
			tegra_dsi_end_host_cmd_v_blank_video(dc, dsi);
		} else {
			delay_ms = DEFAULT_DELAY_MS;
			if ((i + 1 < n_cmd) &&
				(cmd[i + 1].cmd_type == TEGRA_DSI_DELAY_MS)) {
				delay_ms = cmd[i + 1].sp_len_dly.delay_ms;
				i++;
			}
			err = tegra_dsi_write_data_nosync(dc, dsi,
							cur_cmd, delay_ms);
			if (err < 0)
				break;
		}
	}
	return err;
#undef DEFAULT_DELAY_MS
}

static u8 tegra_dsi_ecc(u32 header)
{
	char ecc_parity[24] = {
		0x07, 0x0b, 0x0d, 0x0e, 0x13, 0x15, 0x16, 0x19,
		0x1a, 0x1c, 0x23, 0x25, 0x26, 0x29, 0x2a, 0x2c,
		0x31, 0x32, 0x34, 0x38, 0x1f, 0x2f, 0x37, 0x3b
	};
	u8 ecc_byte;
	int i;

	ecc_byte = 0;
	for (i = 0; i < 24; i++)
		ecc_byte ^= ((header >> i) & 1) ? ecc_parity[i] : 0x00;

	return ecc_byte;
}

static u16 tegra_dsi_cs(char *pdata, u16 data_len)
{
	u16 byte_cnt;
	u8 bit_cnt;
	char curr_byte;
	u16 crc = 0xFFFF;
	u16 poly = 0x8408;

	if (data_len > 0) {
		for (byte_cnt = 0; byte_cnt < data_len; byte_cnt++) {
			curr_byte = pdata[byte_cnt];
			for (bit_cnt = 0; bit_cnt < 8; bit_cnt++) {
				if (((crc & 0x0001 ) ^
					(curr_byte & 0x0001)) > 0)
					crc = ((crc >> 1) & 0x7FFF) ^ poly;
				else
					crc = (crc >> 1) & 0x7FFF;

				curr_byte = (curr_byte >> 1 ) & 0x7F;
			}
		}
	}
	return crc;
}

static int tegra_dsi_dcs_pkt_seq_ctrl_init(struct tegra_dc_dsi_data *dsi,
						struct tegra_dsi_cmd *cmd)
{
	u8 virtual_channel;
	u32 val;
	u16 data_len = cmd->sp_len_dly.data_len;
	u8 seq_ctrl_reg = 0;

	virtual_channel = dsi->info.virtual_channel <<
				DSI_VIR_CHANNEL_BIT_POSITION;

	val = (virtual_channel | cmd->data_id) << 0 |
		data_len << 8;

	val |= tegra_dsi_ecc(val) << 24;

	tegra_dsi_writel(dsi, val, DSI_INIT_SEQ_DATA_0 + seq_ctrl_reg++);

	/* if pdata != NULL, pkt type is long pkt */
	if (cmd->pdata != NULL) {
		u8 *pdata;
		u8 *pdata_mem;
		/* allocate memory for pdata + 2 bytes checksum */
		pdata_mem = kzalloc(sizeof(u8) * data_len + 2, GFP_KERNEL);
		if (!pdata_mem) {
			dev_err(&dsi->dc->ndev->dev, "dsi: memory err\n");
			tegra_dsi_soft_reset(dsi);
			return -ENOMEM;
		}

		memcpy(pdata_mem, cmd->pdata, data_len);
		pdata = pdata_mem;
		*((u16 *)(pdata + data_len)) = tegra_dsi_cs(pdata, data_len);

		/* data_len = length of pdata + 2 byte checksum */
		data_len += 2;

		while (data_len) {
			if (data_len >= 4) {
				val = ((u32 *) pdata)[0];
				data_len -= 4;
				pdata += 4;
			} else {
				val = 0;
				memcpy(&val, pdata, data_len);
				pdata += data_len;
				data_len = 0;
			}
			tegra_dsi_writel(dsi, val, DSI_INIT_SEQ_DATA_0 +
							seq_ctrl_reg++);
		}
		kfree(pdata_mem);
	}

	return 0;
}

int tegra_dsi_start_host_cmd_v_blank_dcs(struct tegra_dc_dsi_data *dsi,
						struct tegra_dsi_cmd *cmd)
{
#define PKT_HEADER_LEN_BYTE	4
#define CHECKSUM_LEN_BYTE	2

	int err = 0;
	u32 val;
	u16 tot_pkt_len = PKT_HEADER_LEN_BYTE;
	struct tegra_dc *dc = dsi->dc;

	if (cmd->cmd_type != TEGRA_DSI_PACKET_CMD)
		return -EINVAL;

	mutex_lock(&dsi->lock);
	tegra_dc_io_start(dc);
	tegra_dc_dsi_hold_host(dc);

#if DSI_USE_SYNC_POINTS
	atomic_set(&dsi_syncpt_rst, 1);
#endif

	err = tegra_dsi_dcs_pkt_seq_ctrl_init(dsi, cmd);
	if (err < 0) {
		dev_err(&dsi->dc->ndev->dev,
			"dsi: dcs pkt seq ctrl init failed\n");
		goto fail;
	}

	if (cmd->pdata) {
		u16 data_len = cmd->sp_len_dly.data_len;
		tot_pkt_len += data_len + CHECKSUM_LEN_BYTE;
	}

	val = DSI_INIT_SEQ_CONTROL_DSI_FRAME_INIT_BYTE_COUNT(tot_pkt_len) |
		DSI_INIT_SEQ_CONTROL_DSI_SEND_INIT_SEQUENCE(
						TEGRA_DSI_ENABLE);
	tegra_dsi_writel(dsi, val, DSI_INIT_SEQ_CONTROL);

fail:
	tegra_dc_dsi_release_host(dc);
	tegra_dc_io_end(dc);
	mutex_unlock(&dsi->lock);
	return err;

#undef PKT_HEADER_LEN_BYTE
#undef CHECKSUM_LEN_BYTE
}
EXPORT_SYMBOL(tegra_dsi_start_host_cmd_v_blank_dcs);

void tegra_dsi_stop_host_cmd_v_blank_dcs(struct tegra_dc_dsi_data *dsi)
{
	struct tegra_dc *dc = dsi->dc;
	u32 cnt;

	mutex_lock(&dsi->lock);
	tegra_dc_io_start(dc);
	tegra_dc_dsi_hold_host(dc);

	if (!tegra_cpu_is_asim() && DSI_USE_SYNC_POINTS)
		if (atomic_read(&dsi_syncpt_rst)) {
			tegra_dsi_wait_frame_end(dc, dsi, 2);
			tegra_dsi_syncpt_reset(dsi);
			atomic_set(&dsi_syncpt_rst, 0);
		}

	tegra_dsi_writel(dsi, TEGRA_DSI_DISABLE, DSI_INIT_SEQ_CONTROL);

	/* clear seq data registers */
	for (cnt = 0; cnt < 8; cnt++)
		tegra_dsi_writel(dsi, 0, DSI_INIT_SEQ_DATA_0 + cnt);

	tegra_dc_dsi_release_host(dc);
	tegra_dc_io_end(dc);

	mutex_unlock(&dsi->lock);
}
EXPORT_SYMBOL(tegra_dsi_stop_host_cmd_v_blank_dcs);

static int tegra_dsi_bta(struct tegra_dc_dsi_data *dsi)
{
	u32 val;
	int err = 0;

	val = tegra_dsi_readl(dsi, DSI_HOST_DSI_CONTROL);
	val |= DSI_HOST_DSI_CONTROL_IMM_BTA(TEGRA_DSI_ENABLE);
	tegra_dsi_controller_writel(dsi, val,
				DSI_HOST_DSI_CONTROL, TEGRA_DSI_LINK0);

	if (!tegra_cpu_is_asim() && DSI_USE_SYNC_POINTS) {
		err = tegra_dsi_syncpt(dsi, TEGRA_DSI_LINK0);
		if (err < 0) {
			dev_err(&dsi->dc->ndev->dev,
				"DSI syncpt for bta failed\n");
		}
	} else {
		if (tegra_dsi_read_busy(dsi, TEGRA_DSI_LINK0)) {
			err = -EBUSY;
			dev_err(&dsi->dc->ndev->dev,
				"Timeout wating on read completion\n");
		}
	}

	return err;
}

static int tegra_dsi_parse_read_response(struct tegra_dc *dc,
					u32 rd_fifo_cnt, u8 *read_fifo)
{
	int err;
	u32 payload_size;

	payload_size = 0;
	err = 0;

	switch (read_fifo[0]) {
	case DSI_ESCAPE_CMD:
		dev_info(&dc->ndev->dev, "escape cmd[0x%x]\n", read_fifo[0]);
		break;
	case DSI_ACK_NO_ERR:
		dev_info(&dc->ndev->dev,
			"Panel ack, no err[0x%x]\n", read_fifo[0]);
		return err;
	default:
		dev_info(&dc->ndev->dev, "Invalid read response\n");
		break;
	}

	switch (read_fifo[4] & 0xff) {
	case GEN_LONG_RD_RES:
		/* Fall through */
	case DCS_LONG_RD_RES:
		payload_size = (read_fifo[5] |
				(read_fifo[6] << 8)) & 0xFFFF;
		dev_info(&dc->ndev->dev, "Long read response Packet\n"
				"payload_size[0x%x]\n", payload_size);
		break;
	case GEN_1_BYTE_SHORT_RD_RES:
		/* Fall through */
	case DCS_1_BYTE_SHORT_RD_RES:
		payload_size = 1;
		dev_info(&dc->ndev->dev, "Short read response Packet\n"
			"payload_size[0x%x]\n", payload_size);
		break;
	case GEN_2_BYTE_SHORT_RD_RES:
		/* Fall through */
	case DCS_2_BYTE_SHORT_RD_RES:
		payload_size = 2;
		dev_info(&dc->ndev->dev, "Short read response Packet\n"
			"payload_size[0x%x]\n", payload_size);
		break;
	case ACK_ERR_RES:
		payload_size = 2;
		dev_info(&dc->ndev->dev, "Acknowledge error report response\n"
			"Packet payload_size[0x%x]\n", payload_size);
		break;
	default:
		dev_info(&dc->ndev->dev, "Invalid response packet\n");
		err = -EINVAL;
		break;
	}
	return err;
}

static int tegra_dsi_read_fifo(struct tegra_dc *dc,
			struct tegra_dc_dsi_data *dsi,
			u8 *read_fifo)
{
	u32 val;
	u32 i;
	u32 poll_time = 0;
	u32 rd_fifo_cnt;
	int err = 0;
	u8 *read_fifo_cp = read_fifo;

	while (poll_time <  DSI_DELAY_FOR_READ_FIFO) {
		mdelay(1);
		val = tegra_dsi_readl(dsi, DSI_STATUS);
		rd_fifo_cnt = val & DSI_STATUS_RD_FIFO_COUNT(0x1f);
		if (rd_fifo_cnt << 2 > DSI_READ_FIFO_DEPTH) {
			dev_err(&dc->ndev->dev,
			"DSI RD_FIFO_CNT is greater than RD_FIFO_DEPTH\n");
			break;
		}
		poll_time++;
	}

	if (rd_fifo_cnt == 0) {
		dev_info(&dc->ndev->dev,
			"DSI RD_FIFO_CNT is zero\n");
		err = -EINVAL;
		goto fail;
	}

	if (val & (DSI_STATUS_LB_UNDERFLOW(0x1) |
		DSI_STATUS_LB_OVERFLOW(0x1))) {
		dev_warn(&dc->ndev->dev,
			"DSI overflow/underflow error\n");
	}

	/* Read data from FIFO */
	for (i = 0; i < rd_fifo_cnt; i++) {
		val = tegra_dsi_readl(dsi, DSI_RD_DATA);
		if (enable_read_debug)
			dev_info(&dc->ndev->dev,
			"Read data[%d]: 0x%x\n", i, val);
		memcpy(read_fifo, &val, 4);
		read_fifo += 4;
	}

	/* Make sure all the data is read from the FIFO */
	val = tegra_dsi_readl(dsi, DSI_STATUS);
	val &= DSI_STATUS_RD_FIFO_COUNT(0x1f);
	if (val)
		dev_err(&dc->ndev->dev, "DSI FIFO_RD_CNT not zero"
		" even after reading FIFO_RD_CNT words from read fifo\n");

	if (enable_read_debug) {
		err =
		tegra_dsi_parse_read_response(dc, rd_fifo_cnt, read_fifo_cp);
		if (err < 0)
			dev_warn(&dc->ndev->dev, "Unexpected read data\n");
	}
fail:
	return err;
}

int tegra_dsi_read_data(struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi,
				u16 max_ret_payload_size,
				u8 panel_reg_addr, u8 *read_data)
{
	int err = 0;
	struct dsi_status *init_status;
	static struct tegra_dsi_cmd temp_cmd;

	if (!dsi->enabled) {
		dev_err(&dc->ndev->dev, "DSI controller suspended\n");
		return -EINVAL;
	}
	tegra_dc_dsi_hold_host(dc);
	mutex_lock(&dsi->lock);
	tegra_dc_io_start(dc);
	clk_prepare_enable(dsi->dsi_fixed_clk);
	tegra_dsi_lp_clk_enable(dsi);
	init_status = tegra_dsi_prepare_host_transmission(
				dc, dsi, DSI_LP_OP_WRITE);
	if (IS_ERR_OR_NULL(init_status)) {
		err = PTR_ERR(init_status);
		dev_err(&dc->ndev->dev, "DSI host config failed\n");
		goto fail;
	}

	/* Set max return payload size in words */
	temp_cmd.data_id = dsi_command_max_return_pkt_size;
	temp_cmd.sp_len_dly.data_len = max_ret_payload_size;
	err = _tegra_dsi_write_data(dsi, &temp_cmd);
	if (err < 0) {
		dev_err(&dc->ndev->dev,
				"DSI write failed\n");
		goto fail;
	}

	/* DCS to read given panel register */
	temp_cmd.data_id = dsi_command_dcs_read_with_no_params;
	temp_cmd.sp_len_dly.sp.data0 = panel_reg_addr;
	temp_cmd.sp_len_dly.sp.data1 = 0;
	err = _tegra_dsi_write_data(dsi, &temp_cmd);
	if (err < 0) {
		dev_err(&dc->ndev->dev,
				"DSI write failed\n");
		goto fail;
	}

	tegra_dsi_reset_read_count(dsi);

	if (dsi->status.lp_op == DSI_LP_OP_WRITE) {
		err = tegra_dsi_set_to_lp_mode(dc, dsi, DSI_LP_OP_READ);
		if (err < 0) {
			dev_err(&dc->ndev->dev,
			"DSI failed to go to LP read mode\n");
			goto fail;
		}
	}

	err = tegra_dsi_bta(dsi);
	if (err < 0) {
		dev_err(&dc->ndev->dev,
			"DSI IMM BTA timeout\n");
		goto fail;
	}

	err = tegra_dsi_read_fifo(dc, dsi, read_data);
	if (err < 0) {
		dev_err(&dc->ndev->dev, "DSI read fifo failure\n");
		goto fail;
	}
fail:
	err = tegra_dsi_restore_state(dc, dsi, init_status);
	if (err < 0)
		dev_err(&dc->ndev->dev, "Failed to restore prev state\n");
	tegra_dsi_lp_clk_disable(dsi);
	clk_disable_unprepare(dsi->dsi_fixed_clk);
	tegra_dc_io_end(dc);
	mutex_unlock(&dsi->lock);
	tegra_dc_dsi_release_host(dc);
	return err;
}
EXPORT_SYMBOL(tegra_dsi_read_data);

static const char * const error_sanity[] = {
		"SoT Error",
		"SoT Sync Error",
		"EoT Sync Error",
		"Escape Mode Entry Comand Error",
		"Low-Power Transmit Sync Error",
		"HS Receive Timeout Error",
		"False Control Error",
		"Reserved",
		"ECC Error,Single Bit",
		"ECC Error, Multi Bit",
		"Checksum Error",
		"DSI Data Type Not recognized",
		"DSI VC ID Invalid",
		"DSI Protocol Violation",
		"Reserved",
		"Reserved",
};

int tegra_dsi_panel_sanity_check(struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi,
				struct sanity_status *san)
{
	int err = 0;
	u32 flagset[16];
	u8 read_fifo[DSI_READ_FIFO_DEPTH];
	struct dsi_status *init_status;
	static struct tegra_dsi_cmd dsi_nop_cmd =
			DSI_CMD_SHORT(0x05, 0x0, 0x0);

	if (!dsi->enabled) {
		dev_err(&dc->ndev->dev, "DSI controller suspended\n");
		return -EINVAL;
	}
	tegra_dc_dsi_hold_host(dc);
	tegra_dc_io_start(dc);
	clk_prepare_enable(dsi->dsi_fixed_clk);
	tegra_dsi_lp_clk_enable(dsi);
	memset(flagset, 0, sizeof(flagset));
	init_status = tegra_dsi_prepare_host_transmission(
				dc, dsi, DSI_LP_OP_WRITE);
	if (IS_ERR_OR_NULL(init_status)) {
		err = PTR_ERR(init_status);
		dev_err(&dc->ndev->dev, "DSI host config failed\n");
		goto fail;
	}

	err = _tegra_dsi_write_data(dsi, &dsi_nop_cmd);
	if (err < 0) {
		dev_err(&dc->ndev->dev, "DSI nop write failed\n");
		goto fail;
	}

	tegra_dsi_reset_read_count(dsi);

	if (dsi->status.lp_op == DSI_LP_OP_WRITE) {
		err = tegra_dsi_set_to_lp_mode(dc, dsi, DSI_LP_OP_READ);
		if (err < 0) {
			dev_err(&dc->ndev->dev,
			"DSI failed to go to LP read mode\n");
			goto fail;
		}
	}

	err = tegra_dsi_bta(dsi);
	if (err < 0) {
		dev_err(&dc->ndev->dev, "DSI BTA failed\n");
		goto fail;
	}

	err = tegra_dsi_read_fifo(dc, dsi, read_fifo);
	if (err < 0) {
		dev_err(&dc->ndev->dev, "DSI read fifo failure\n");
		goto fail;
	}

	if (read_fifo[0] != DSI_ACK_NO_ERR) {
		if (read_fifo[4] == ACK_ERR_RES) {
			u16 payload = read_fifo[5] | (read_fifo[6] << 8);
			int i = 0;
			for (; payload; payload >>= 1, i++) {
				if (payload & 1) {
					flagset[i] = 0x01;
					if (enable_read_debug)
						dev_info(&dc->ndev->dev,
							" %s => error flag number %d\n",
							error_sanity[i], i);
				}
			}
			if (san != NULL) {
				san->sot_error = flagset[0];
				san->sot_sync_error = flagset[1];
				san->eot_sync_error = flagset[2];
				san->escape_mode_entry_comand_error =
						flagset[3];
				san->low_power_transmit_sync_error = flagset[4];
				san->hs_receive_timeout_error = flagset[5];
				san->false_control_error = flagset[6];
				san->reserved1 = flagset[7];
				san->ecc_error_single_bit = flagset[8];
				san->ecc_error_multi_bit = flagset[9];
				san->checksum_error = flagset[10];
				san->dsi_data_type_not_recognized = flagset[11];
				san->dsi_vc_id_invalid = flagset[12];
				san->dsi_protocol_violation = flagset[13];
				san->reserved2 = flagset[14];
				san->reserved3 = flagset[15];
			}
		}
		dev_warn(&dc->ndev->dev,
			"Ack no error trigger message not received\n");
		err = -EAGAIN;
	}

fail:
	err = tegra_dsi_restore_state(dc, dsi, init_status);
	if (err < 0)
		dev_err(&dc->ndev->dev, "Failed to restore prev state\n");
	tegra_dsi_lp_clk_disable(dsi);
	clk_disable_unprepare(dsi->dsi_fixed_clk);
	tegra_dc_io_end(dc);
	tegra_dc_dsi_release_host(dc);
	return err;
}
EXPORT_SYMBOL(tegra_dsi_panel_sanity_check);

static int tegra_dsi_enter_ulpm(struct tegra_dc_dsi_data *dsi)
{
	u32 val;
	int ret = 0;

	if (dsi->info.ulpm_not_supported)
		return 0;

	if (!tegra_cpu_is_asim() && DSI_USE_SYNC_POINTS)
		if (atomic_read(&dsi_syncpt_rst))
			tegra_dsi_syncpt_reset(dsi);

	val = tegra_dsi_readl(dsi, DSI_HOST_DSI_CONTROL);
	val &= ~DSI_HOST_DSI_CONTROL_ULTRA_LOW_POWER(3);
	val |= DSI_HOST_DSI_CONTROL_ULTRA_LOW_POWER(ENTER_ULPM);
	tegra_dsi_writel(dsi, val, DSI_HOST_DSI_CONTROL);

	if (!tegra_cpu_is_asim() && DSI_USE_SYNC_POINTS) {
		ret = tegra_dsi_syncpt(dsi, TEGRA_DSI_LINK0);
		if (ret < 0) {
			dev_err(&dsi->dc->ndev->dev,
				"DSI syncpt for ulpm enter failed\n");
			return ret;
		}
	} else {
		/* TODO: Find exact delay required */
		mdelay(10);
	}
	dsi->ulpm = true;

	return ret;
}

static int tegra_dsi_exit_ulpm(struct tegra_dc_dsi_data *dsi)
{
	u32 val;
	int ret = 0;

	if (dsi->info.ulpm_not_supported)
		return 0;

	if (!tegra_cpu_is_asim() && DSI_USE_SYNC_POINTS)
		if (atomic_read(&dsi_syncpt_rst))
			tegra_dsi_syncpt_reset(dsi);

	val = tegra_dsi_readl(dsi, DSI_HOST_DSI_CONTROL);
	val &= ~DSI_HOST_DSI_CONTROL_ULTRA_LOW_POWER(3);
	val |= DSI_HOST_DSI_CONTROL_ULTRA_LOW_POWER(EXIT_ULPM);
	tegra_dsi_writel(dsi, val, DSI_HOST_DSI_CONTROL);

	if (!tegra_cpu_is_asim() && DSI_USE_SYNC_POINTS) {
		ret = tegra_dsi_syncpt(dsi, TEGRA_DSI_LINK0);
		if (ret < 0) {
			dev_err(&dsi->dc->ndev->dev,
				"DSI syncpt for ulpm exit failed\n");
			return ret;
		}
	} else {
		/* TODO: Find exact delay required */
		mdelay(10);
	}
	dsi->ulpm = false;

	val = tegra_dsi_readl(dsi, DSI_HOST_DSI_CONTROL);
	val &= ~DSI_HOST_DSI_CONTROL_ULTRA_LOW_POWER(0x3);
	val |= DSI_HOST_DSI_CONTROL_ULTRA_LOW_POWER(NORMAL);
	tegra_dsi_writel(dsi, val, DSI_HOST_DSI_CONTROL);

	return ret;
}

static void tegra_dsi_send_dc_frames(struct tegra_dc *dc,
				     struct tegra_dc_dsi_data *dsi,
				     int no_of_frames)
{
	int err;
	u32 frame_period = DIV_ROUND_UP(S_TO_MS(1), dsi->info.refresh_rate);
	u8 lp_op = dsi->status.lp_op;
	bool switch_to_lp = (dsi->status.lphs == DSI_LPHS_IN_LP_MODE);

	if (dsi->status.lphs != DSI_LPHS_IN_HS_MODE) {
		err = tegra_dsi_set_to_hs_mode(dc, dsi,
				TEGRA_DSI_DRIVEN_BY_DC);
		if (err < 0) {
			dev_err(&dc->ndev->dev,
				"Switch to HS host mode failed\n");
			return;
		}
	}

	/*
	 * Some panels need DC frames be sent under certain
	 * conditions. We are working on the right fix for this
	 * requirement, while using this current fix.
	 */
	tegra_dsi_start_dc_stream(dc, dsi);

	/*
	 * Send frames in Continuous or One-shot mode.
	 */
	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE) {
		/* FIX ME: tegra_dc_blank() implicitly takes lock */
		int flag = mutex_is_locked(&dc->lock);
		if (flag)
			mutex_unlock(&dc->lock);
		while (no_of_frames--)
			tegra_dc_blank(dc);
		if (flag)
			mutex_lock(&dc->lock);
	} else
		mdelay(no_of_frames * frame_period);

	tegra_dsi_stop_dc_stream_at_frame_end(dc, dsi, 2);

	if (switch_to_lp) {
		err = tegra_dsi_set_to_lp_mode(dc, dsi, lp_op);
		if (err < 0)
			dev_err(&dc->ndev->dev,
				"DSI failed to go to LP mode\n");
	}
}

static void tegra_dsi_setup_initialized_panel(struct tegra_dc_dsi_data *dsi)
{
	int err = 0;

	if (dsi->avdd_dsi_csi)
		err = regulator_enable(dsi->avdd_dsi_csi);
	WARN(err, "unable to enable regulator");

	dsi->status.init = DSI_MODULE_INIT;
	dsi->status.lphs = DSI_LPHS_IN_HS_MODE;
	dsi->status.driven = DSI_DRIVEN_MODE_DC;
	dsi->driven_mode = TEGRA_DSI_DRIVEN_BY_DC;
	dsi->status.clk_out = DSI_PHYCLK_OUT_EN;
	dsi->status.lp_op = DSI_LP_OP_NOT_INIT;
	dsi->status.dc_stream = DSI_DC_STREAM_ENABLE;

	if (dsi->info.video_clock_mode == TEGRA_DSI_VIDEO_CLOCK_CONTINUOUS)
		dsi->status.clk_mode = DSI_PHYCLK_CONTINUOUS;
	else
		dsi->status.clk_mode = DSI_PHYCLK_TX_ONLY;

	if (!(dsi->info.ganged_type)) {
		if (dsi->info.video_burst_mode ==
			TEGRA_DSI_VIDEO_NONE_BURST_MODE ||
			dsi->info.video_burst_mode ==
			TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END)
			dsi->status.clk_burst = DSI_CLK_BURST_NONE_BURST;
		else
			dsi->status.clk_burst = DSI_CLK_BURST_BURST_MODE;
	}

	if (dsi->info.video_data_type == TEGRA_DSI_VIDEO_TYPE_COMMAND_MODE)
		dsi->status.vtype = DSI_VIDEO_TYPE_CMD_MODE;
	else
		dsi->status.vtype = DSI_VIDEO_TYPE_VIDEO_MODE;

	tegra_dsi_clk_enable(dsi);

	dsi->enabled = true;
}

static void tegra_dc_dsi_enable(struct tegra_dc *dc)
{
	struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);
	int err = 0;

	mutex_lock(&dsi->lock);
	tegra_dc_io_start(dc);

	/*
	 * Do not program this panel as the bootloader as has already
	 * initialized it. This avoids periods of blanking during boot.
	 */
	if (dc->out->flags & TEGRA_DC_OUT_INITIALIZED_MODE) {
		tegra_dsi_setup_initialized_panel(dsi);
		goto fail;
	}

	/* Stop DC stream before configuring DSI registers
	 * to avoid visible glitches on panel during transition
	 * from bootloader to kernel driver
	 */
	tegra_dsi_stop_dc_stream(dc, dsi);

	if (dsi->enabled) {
		if (dsi->ulpm) {
			if (tegra_dsi_exit_ulpm(dsi) < 0) {
				dev_err(&dc->ndev->dev,
					"DSI failed to exit ulpm\n");
				goto fail;
			}
		}

		if (dsi->info.panel_reset) {
			/*
			 * Certain panels need dc frames be sent before
			 * waking panel.
			 */
			if (dsi->info.panel_send_dc_frames)
				tegra_dsi_send_dc_frames(dc, dsi, 2);

			err = tegra_dsi_send_panel_cmd(dc, dsi,
							dsi->info.dsi_init_cmd,
							dsi->info.n_init_cmd);
			if (err < 0) {
				dev_err(&dc->ndev->dev,
				"dsi: error sending dsi init cmd\n");
				goto fail;
			}
		} else if (dsi->info.dsi_late_resume_cmd) {
			err = tegra_dsi_send_panel_cmd(dc, dsi,
						dsi->info.dsi_late_resume_cmd,
						dsi->info.n_late_resume_cmd);
			if (err < 0) {
				dev_err(&dc->ndev->dev,
				"dsi: error sending late resume cmd\n");
				goto fail;
			}
		}
	} else {
		err = tegra_dsi_init_hw(dc, dsi);
		if (err < 0) {
			dev_err(&dc->ndev->dev,
				"dsi: not able to init dsi hardware\n");
			goto fail;
		}

		if (dsi->ulpm) {
			if (tegra_dsi_enter_ulpm(dsi) < 0) {
				dev_err(&dc->ndev->dev,
					"DSI failed to enter ulpm\n");
				goto fail;
			}

			tegra_dsi_pad_enable(dsi);

			if (tegra_dsi_exit_ulpm(dsi) < 0) {
				dev_err(&dc->ndev->dev,
					"DSI failed to exit ulpm\n");
				goto fail;
			}
		}

		/*
		 * Certain panels need dc frames be sent before
		 * waking panel.
		 */
		if (dsi->info.panel_send_dc_frames)
			tegra_dsi_send_dc_frames(dc, dsi, 2);

		err = tegra_dsi_set_to_lp_mode(dc, dsi, DSI_LP_OP_WRITE);
		if (err < 0) {
			dev_err(&dc->ndev->dev,
				"dsi: not able to set to lp mode\n");
			goto fail;
		}

		if (dsi->info.lp00_pre_panel_wakeup)
			tegra_dsi_pad_disable(dsi);

		dsi->enabled = true;
	}

	if (dsi->out_ops && dsi->out_ops->enable)
		dsi->out_ops->enable(dsi);
fail:
	tegra_dc_io_end(dc);
	mutex_unlock(&dsi->lock);
}

static void tegra_dc_dsi_postpoweron(struct tegra_dc *dc)
{
	struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);
	int err = 0;

	/*
	 * Do not configure. Use bootloader configuration.
	 * This avoids periods of blanking during boot.
	 */
	if (dc->out->flags & TEGRA_DC_OUT_INITIALIZED_MODE)
		return;

	mutex_lock(&dsi->lock);
	tegra_dc_io_start(dc);

	if (dsi->enabled) {
		if (dsi->info.lp00_pre_panel_wakeup)
			tegra_dsi_pad_enable(dsi);

		err = tegra_dsi_send_panel_cmd(dc, dsi, dsi->info.dsi_init_cmd,
							dsi->info.n_init_cmd);
		if (err < 0) {
			dev_err(&dc->ndev->dev,
				"dsi: error while sending dsi init cmd\n");
			goto fail;
		}

		err = tegra_dsi_set_to_hs_mode(dc, dsi,
				TEGRA_DSI_DRIVEN_BY_DC);
		if (err < 0) {
			dev_err(&dc->ndev->dev,
				"dsi: not able to set to hs mode\n");
			goto fail;
		}

		if (dsi->status.driven == DSI_DRIVEN_MODE_DC)
			tegra_dsi_start_dc_stream(dc, dsi);

		dsi->host_suspended = false;
	}
fail:
	tegra_dc_io_end(dc);
	mutex_unlock(&dsi->lock);
}

static void __tegra_dc_dsi_init(struct tegra_dc *dc)
{
	struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);

#ifdef CONFIG_DEBUG_FS
	tegra_dc_dsi_debug_create(dsi);
#endif

	if (dsi->info.dsi2lvds_bridge_enable)
		dsi->out_ops = &tegra_dsi2lvds_ops;
	else if (dsi->info.dsi2edp_bridge_enable)
		dsi->out_ops = &tegra_dsi2edp_ops;
	else
		dsi->out_ops = NULL;

	if (dsi->out_ops && dsi->out_ops->init)
		dsi->out_ops->init(dsi);

	tegra_dsi_init_sw(dc, dsi);
}

static int tegra_dc_dsi_cp_p_cmd(struct tegra_dsi_cmd *src,
					struct tegra_dsi_cmd *dst, u16 n_cmd)
{
	u16 i;
	u16 len;

	memcpy(dst, src, sizeof(*dst) * n_cmd);

	for (i = 0; i < n_cmd; i++)
		if (src[i].pdata) {
			len = sizeof(*src[i].pdata) *
					src[i].sp_len_dly.data_len;
			dst[i].pdata = kzalloc(len, GFP_KERNEL);
			if (!dst[i].pdata)
				goto free_cmd_pdata;
			memcpy(dst[i].pdata, src[i].pdata, len);
		}

	return 0;

free_cmd_pdata:
	while (i--)
		if (dst[i].pdata)
			kfree(dst[i].pdata);
	return -ENOMEM;
}

static int tegra_dc_dsi_cp_info(struct tegra_dc_dsi_data *dsi,
						struct tegra_dsi_out *p_dsi)
{
	struct tegra_dsi_cmd *p_init_cmd;
	struct tegra_dsi_cmd *p_early_suspend_cmd = NULL;
	struct tegra_dsi_cmd *p_late_resume_cmd = NULL;
	struct tegra_dsi_cmd *p_suspend_cmd;
	int err = 0;

	if (dsi->info.n_data_lanes > MAX_DSI_DATA_LANES)
		return -EINVAL;

	p_init_cmd = kzalloc(sizeof(*p_init_cmd) *
				p_dsi->n_init_cmd, GFP_KERNEL);
	if (!p_init_cmd)
		return -ENOMEM;

	if (p_dsi->dsi_early_suspend_cmd) {
		p_early_suspend_cmd = kzalloc(sizeof(*p_early_suspend_cmd) *
					p_dsi->n_early_suspend_cmd,
					GFP_KERNEL);
		if (!p_early_suspend_cmd) {
			err = -ENOMEM;
			goto err_free_init_cmd;
		}
	}

	if (p_dsi->dsi_late_resume_cmd) {
		p_late_resume_cmd = kzalloc(sizeof(*p_late_resume_cmd) *
					p_dsi->n_late_resume_cmd,
					GFP_KERNEL);
		if (!p_late_resume_cmd) {
			err = -ENOMEM;
			goto err_free_p_early_suspend_cmd;
		}
	}

	p_suspend_cmd = kzalloc(sizeof(*p_suspend_cmd) * p_dsi->n_suspend_cmd,
				GFP_KERNEL);
	if (!p_suspend_cmd) {
		err = -ENOMEM;
		goto err_free_p_late_resume_cmd;
	}

	memcpy(&dsi->info, p_dsi, sizeof(dsi->info));

	/* Copy panel init cmd */
	err = tegra_dc_dsi_cp_p_cmd(p_dsi->dsi_init_cmd,
						p_init_cmd, p_dsi->n_init_cmd);
	if (err < 0)
		goto err_free;
	dsi->info.dsi_init_cmd = p_init_cmd;

	/* Copy panel early suspend cmd */
	if (p_dsi->dsi_early_suspend_cmd) {
		err = tegra_dc_dsi_cp_p_cmd(p_dsi->dsi_early_suspend_cmd,
					p_early_suspend_cmd,
					p_dsi->n_early_suspend_cmd);
		if (err < 0)
			goto err_free;
		dsi->info.dsi_early_suspend_cmd = p_early_suspend_cmd;
	}

	/* Copy panel late resume cmd */
	if (p_dsi->dsi_late_resume_cmd) {
		err = tegra_dc_dsi_cp_p_cmd(p_dsi->dsi_late_resume_cmd,
						p_late_resume_cmd,
						p_dsi->n_late_resume_cmd);
		if (err < 0)
			goto err_free;
		dsi->info.dsi_late_resume_cmd = p_late_resume_cmd;
	}

	/* Copy panel suspend cmd */
	err = tegra_dc_dsi_cp_p_cmd(p_dsi->dsi_suspend_cmd, p_suspend_cmd,
					p_dsi->n_suspend_cmd);
	if (err < 0)
		goto err_free;
	dsi->info.dsi_suspend_cmd = p_suspend_cmd;

	if (!dsi->info.panel_reset_timeout_msec)
		dsi->info.panel_reset_timeout_msec =
						DEFAULT_PANEL_RESET_TIMEOUT;
	if (!dsi->info.panel_buffer_size_byte)
		dsi->info.panel_buffer_size_byte = DEFAULT_PANEL_BUFFER_BYTE;

	if (!dsi->info.max_panel_freq_khz) {
		dsi->info.max_panel_freq_khz = DEFAULT_MAX_DSI_PHY_CLK_KHZ;

		if (dsi->info.video_burst_mode >
				TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END){
			dev_err(&dsi->dc->ndev->dev, "DSI: max_panel_freq_khz"
					"is not set for DSI burst mode.\n");
			dsi->info.video_burst_mode =
				TEGRA_DSI_VIDEO_BURST_MODE_LOWEST_SPEED;
		}
	}

	if (!dsi->info.lp_cmd_mode_freq_khz)
		dsi->info.lp_cmd_mode_freq_khz = DEFAULT_LP_CMD_MODE_CLK_KHZ;

	if (!dsi->info.chip_id || !dsi->info.chip_rev)
		dev_warn(&dsi->dc->ndev->dev,
			"DSI: Failed to get chip info\n");

	if (!dsi->info.lp_read_cmd_mode_freq_khz)
		dsi->info.lp_read_cmd_mode_freq_khz =
			dsi->info.lp_cmd_mode_freq_khz;

	/* host mode is for testing only */
	dsi->driven_mode = TEGRA_DSI_DRIVEN_BY_DC;
	return 0;

err_free:
	kfree(p_suspend_cmd);
err_free_p_late_resume_cmd:
	kfree(p_late_resume_cmd);
err_free_p_early_suspend_cmd:
	kfree(p_early_suspend_cmd);
err_free_init_cmd:
	kfree(p_init_cmd);
	return err;
}

static int _tegra_dc_dsi_init(struct tegra_dc *dc)
{
	struct tegra_dc_dsi_data *dsi;
	struct resource *res;
	struct resource *base_res;
	struct resource dsi_res;
	void __iomem *base;
	struct clk *dc_clk = NULL;
	struct clk *dsi_clk;
	struct clk *dsi_fixed_clk = NULL;
	struct clk *dsi_lp_clk = NULL;
	struct tegra_dsi_out *dsi_pdata;
	int err = 0, i;
	char *ganged_reg_name[2] = {"ganged_dsia_regs", "ganged_dsib_regs"};
	char *dsi_clk_name[2] = {"dsia", "dsib"};
	char *dsi_lp_clk_name[2] = {"dsialp", "dsiblp"};
	struct device_node *np = dc->ndev->dev.of_node;
#ifdef CONFIG_USE_OF
	struct device_node *np_dsi =
		of_find_node_by_path("/host1x/dsi");
#else
	struct device_node *np_dsi = NULL;
#endif
	dsi = kzalloc(sizeof(*dsi), GFP_KERNEL);
	if (!dsi) {
		dev_err(&dc->ndev->dev, "dsi: memory allocation failed\n");
		return -ENOMEM;
	}

	dsi->max_instances = dc->out->dsi->ganged_type ? MAX_DSI_INSTANCE : 1;
	for (i = 0; i < dsi->max_instances; i++) {
		if (np) {
			if (np_dsi && of_device_is_available(np_dsi)) {
				of_address_to_resource(np_dsi, i, &dsi_res);
				res = &dsi_res;
			} else {
				err = -EINVAL;
				goto err_free_dsi;
			}
		} else {
			res = platform_get_resource_byname(dc->ndev,
					IORESOURCE_MEM,
					dc->out->dsi->ganged_type ?
					ganged_reg_name[i] : "dsi_regs");
		}
		if (!res) {
			dev_err(&dc->ndev->dev, "dsi: no mem resource\n");
			err = -ENOENT;
			goto err_free_dsi;
		}

		base_res = request_mem_region(res->start, resource_size(res),
					dc->ndev->name);
		if (!base_res) {
			dev_err(&dc->ndev->dev,
				"dsi: request_mem_region failed\n");
			err = -EBUSY;
			goto err_free_dsi;
		}

		base = ioremap(res->start, resource_size(res));
		if (!base) {
			dev_err(&dc->ndev->dev,
				"dsi: registers can't be mapped\n");
			err = -EBUSY;
			goto err_release_regs;
		}

		dsi_pdata = dc->pdata->default_out->dsi;
		if (!dsi_pdata) {
			dev_err(&dc->ndev->dev, "dsi: dsi data not available\n");
			goto err_release_regs;
		}

		dsi_clk = dsi->info.dsi_instance ?
				clk_get(&dc->ndev->dev,
				dsi_clk_name[DSI_INSTANCE_1]) :
				clk_get(&dc->ndev->dev, dsi_clk_name[i]);
		dsi_lp_clk = dsi->info.dsi_instance ?
				clk_get(&dc->ndev->dev,
				dsi_lp_clk_name[DSI_INSTANCE_1]) :
				clk_get(&dc->ndev->dev, dsi_lp_clk_name[i]);

		if (IS_ERR_OR_NULL(dsi_clk) || IS_ERR_OR_NULL(dsi_lp_clk)) {
			dev_err(&dc->ndev->dev, "dsi: can't get clock\n");
			err = -EBUSY;
			goto err_dsi_clk_put;
		}

		dsi->base[i] = base;
		dsi->base_res[i] = base_res;
		dsi->dsi_clk[i] = dsi_clk;
		dsi->dsi_lp_clk[i] = dsi_lp_clk;
	}

	dsi_fixed_clk = clk_get(&dc->ndev->dev, "dsi-fixed");

	if (IS_ERR_OR_NULL(dsi_fixed_clk)) {
		dev_err(&dc->ndev->dev, "dsi: can't get fixed clock\n");
		err = -EBUSY;
		goto err_release_regs;
	}

	dc_clk = clk_get_sys(dev_name(&dc->ndev->dev), NULL);
	if (IS_ERR_OR_NULL(dc_clk)) {
		dev_err(&dc->ndev->dev, "dsi: dc clock %s unavailable\n",
			dev_name(&dc->ndev->dev));
		err = -EBUSY;
		goto err_dsi_fixed_clk_put;
	}

	mutex_init(&dsi->lock);
	dsi->dc = dc;
	dsi->dc_clk = dc_clk;
	dsi->dsi_fixed_clk = dsi_fixed_clk;

	err = tegra_dc_dsi_cp_info(dsi, dsi_pdata);
	if (err < 0)
		goto err_dc_clk_put;

	tegra_dc_set_outdata(dc, dsi);
	__tegra_dc_dsi_init(dc);

	return 0;

err_dc_clk_put:
	clk_put(dc_clk);
err_dsi_fixed_clk_put:
	clk_put(dsi_fixed_clk);
err_dsi_clk_put:
	for (i = 0; i < dsi->max_instances; i++) {
		clk_put(dsi->dsi_lp_clk[i]);
		clk_put(dsi->dsi_clk[i]);
	}
err_release_regs:
	for (i = 0; i < dsi->max_instances; i++)
		release_resource(dsi->base_res[i]);
err_free_dsi:
	kfree(dsi);

	return err;
}

static void _tegra_dc_dsi_destroy(struct tegra_dc *dc)
{
	struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);
	u16 i;
	u32 val;

	mutex_lock(&dsi->lock);
	tegra_dc_io_start(dc);

	if (dsi->out_ops && dsi->out_ops->destroy)
		dsi->out_ops->destroy(dsi);

	/* free up the pdata */
	for (i = 0; i < dsi->info.n_init_cmd; i++) {
		if (dsi->info.dsi_init_cmd[i].pdata)
			kfree(dsi->info.dsi_init_cmd[i].pdata);
	}
	kfree(dsi->info.dsi_init_cmd);

	/* Disable dc stream */
	if (dsi->status.dc_stream == DSI_DC_STREAM_ENABLE)
		tegra_dsi_stop_dc_stream_at_frame_end(dc, dsi, 2);

	/* Disable dsi phy clock */
	if (dsi->status.clk_out == DSI_PHYCLK_OUT_EN)
		tegra_dsi_hs_clk_out_disable(dc, dsi);

	val = DSI_POWER_CONTROL_LEG_DSI_ENABLE(TEGRA_DSI_DISABLE);
	tegra_dsi_writel(dsi, val, DSI_POWER_CONTROL);

	for (i = 0; i < dsi->max_instances; i++) {
		iounmap(dsi->base[i]);
		release_resource(dsi->base_res[i]);
	}
	clk_put(dsi->dc_clk);
	for (i = 0; i < dsi->max_instances; i++)
		clk_put(dsi->dsi_clk[i]);

	tegra_dc_io_end(dc);
	mutex_unlock(&dsi->lock);
	mutex_destroy(&dsi->lock);
	kfree(dsi);
}

static void tegra_dsi_config_phy_clk(struct tegra_dc_dsi_data *dsi,
							u32 settings)
{
	struct clk *parent_clk = NULL;
	struct clk *base_clk = NULL;
	int i = 0;

	for (i = 0; i < dsi->max_instances; i++) {
		parent_clk = clk_get_parent(dsi->dsi_clk[i]);
		base_clk = clk_get_parent(parent_clk);

#ifdef CONFIG_ARCH_TEGRA_3x_SOC
		if (dsi->info.dsi_instance)
			tegra_clk_cfg_ex(base_clk,
					TEGRA_CLK_PLLD_CSI_OUT_ENB,
					settings);
		else
			tegra_clk_cfg_ex(base_clk,
					TEGRA_CLK_PLLD_DSI_OUT_ENB,
					settings);
#else
		tegra_clk_cfg_ex(base_clk,
				TEGRA_CLK_PLLD_DSI_OUT_ENB,
				settings);
#endif
	}
}

static int tegra_dsi_te_on_off(struct tegra_dc_dsi_data *dsi, bool flag)
{
	int ret;

	struct tegra_dsi_cmd te_enable[] = {
		DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM,
				DSI_DCS_SET_TEARING_EFFECT_ON, 0x0),
		DSI_DLY_MS(0),
	};

	struct tegra_dsi_cmd te_disable[] = {
		DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM,
				DSI_DCS_SET_TEARING_EFFECT_OFF, 0x0),
		DSI_DLY_MS(0),
	};

	if (flag)
		ret = tegra_dsi_send_panel_cmd(dsi->dc, dsi, te_enable,
					ARRAY_SIZE(te_enable));
	else
		ret = tegra_dsi_send_panel_cmd(dsi->dc, dsi, te_disable,
					ARRAY_SIZE(te_disable));

	return ret;
}

static int _tegra_dsi_host_suspend(struct tegra_dc *dc,
					struct tegra_dc_dsi_data *dsi,
					u32 suspend_aggr)
{
	u32 val = 0;
	int err = 0;

	switch (suspend_aggr) {
	case DSI_HOST_SUSPEND_LV2:
		if (!dsi->ulpm) {
			err = tegra_dsi_enter_ulpm(dsi);
			if (err < 0) {
				dev_err(&dc->ndev->dev,
					"DSI failed to enter ulpm\n");
				goto fail;
			}
		}

		tegra_dsi_pad_disable(dsi);

		/* Suspend core-logic */
		val = DSI_POWER_CONTROL_LEG_DSI_ENABLE(TEGRA_DSI_DISABLE);
		tegra_dsi_writel(dsi, val, DSI_POWER_CONTROL);

		/* disable HS logic */
		val = tegra_dsi_readl(dsi, DSI_PAD_CONTROL_3_VS1);
		val |= DSI_PAD_PDVCLAMP(0x1);
		tegra_dsi_writel(dsi, val, DSI_PAD_CONTROL_3_VS1);

		/* disable mipi bias pad */
		tegra_mipi_cal_clk_enable(dsi->mipi_cal);
		val = tegra_mipi_cal_read(dsi->mipi_cal,
				MIPI_CAL_MIPI_BIAS_PAD_CFG0_0);
		val |= MIPI_BIAS_PAD_PDVCLAMP(0x1);
		tegra_mipi_cal_write(dsi->mipi_cal, val,
				MIPI_CAL_MIPI_BIAS_PAD_CFG0_0);
		tegra_mipi_cal_clk_disable(dsi->mipi_cal);

		/* fall through */
	case DSI_HOST_SUSPEND_LV1:
		/* Disable dsi fast and slow clock */
		tegra_dsi_config_phy_clk(dsi, TEGRA_DSI_DISABLE);
		/* fall through */
	case DSI_HOST_SUSPEND_LV0:
		/* Disable dsi source clock */
		tegra_dsi_clk_disable(dsi);
		break;
	case DSI_NO_SUSPEND:
		break;
	default:
		dev_err(&dc->ndev->dev, "DSI suspend aggressiveness"
						"is not supported.\n");
	}

	tegra_dvfs_set_rate(dc->clk, 0);

	return 0;
fail:
	return err;
}

static int _tegra_dsi_host_resume(struct tegra_dc *dc,
					struct tegra_dc_dsi_data *dsi,
					u32 suspend_aggr)
{
	u32 val;
	int err;

	switch (dsi->info.suspend_aggr) {
	case DSI_HOST_SUSPEND_LV0:
		tegra_dsi_clk_enable(dsi);
		break;
	case DSI_HOST_SUSPEND_LV1:
		tegra_dsi_config_phy_clk(dsi, TEGRA_DSI_ENABLE);
		tegra_dsi_clk_enable(dsi);
		break;
	case DSI_HOST_SUSPEND_LV2:
		tegra_dsi_config_phy_clk(dsi, TEGRA_DSI_ENABLE);
		tegra_dsi_clk_enable(dsi);

		/* enable mipi bias pad */
		tegra_mipi_cal_clk_enable(dsi->mipi_cal);
		val = tegra_mipi_cal_read(dsi->mipi_cal,
				MIPI_CAL_MIPI_BIAS_PAD_CFG0_0);
		val &= ~MIPI_BIAS_PAD_PDVCLAMP(0x1);
		tegra_mipi_cal_write(dsi->mipi_cal, val,
				MIPI_CAL_MIPI_BIAS_PAD_CFG0_0);
		tegra_mipi_cal_clk_disable(dsi->mipi_cal);

		/* enable HS logic */
		val = tegra_dsi_readl(dsi, DSI_PAD_CONTROL_3_VS1);
		val &= ~DSI_PAD_PDVCLAMP(0x1);
		tegra_dsi_writel(dsi, val, DSI_PAD_CONTROL_3_VS1);

		tegra_dsi_writel(dsi,
			DSI_POWER_CONTROL_LEG_DSI_ENABLE(TEGRA_DSI_ENABLE),
			DSI_POWER_CONTROL);

		if (dsi->ulpm) {
			err = tegra_dsi_enter_ulpm(dsi);
			if (err < 0) {
				dev_err(&dc->ndev->dev,
					"DSI failed to enter ulpm\n");
				goto fail;
			}

			tegra_dsi_pad_enable(dsi);

			if (tegra_dsi_exit_ulpm(dsi) < 0) {
				dev_err(&dc->ndev->dev,
					"DSI failed to exit ulpm\n");
				goto fail;
			}
		} else {
			tegra_dsi_pad_enable(dsi);
		}
		break;
	case DSI_NO_SUSPEND:
		break;
	default:
		dev_err(&dc->ndev->dev, "DSI suspend aggressivenes"
						"is not supported.\n");
	}

	tegra_dvfs_set_rate(dc->clk, dc->mode.pclk);

	return 0;
fail:
	return err;
}

static int tegra_dsi_host_suspend_trylock(struct tegra_dc *dc,
					struct tegra_dc_dsi_data *dsi)
{
	if (!mutex_trylock(&dc->one_shot_lp_lock))
		goto fail;
	if (!mutex_trylock(&dc->lock))
		goto unlock_one_shot_lp;
	if (!mutex_trylock(&dsi->host_lock))
		goto unlock_dc_lock;
	if (!mutex_trylock(&dc->one_shot_lock))
		goto unlock_host_lock;

	return 1;

unlock_host_lock:
	mutex_unlock(&dsi->host_lock);
unlock_dc_lock:
	mutex_unlock(&dc->lock);
unlock_one_shot_lp:
	mutex_unlock(&dc->one_shot_lp_lock);
fail:
	return 0;
}

static void tegra_dsi_host_suspend_unlock(struct tegra_dc *dc,
					struct tegra_dc_dsi_data *dsi)
{
	mutex_unlock(&dc->one_shot_lock);
	mutex_unlock(&dsi->host_lock);
	mutex_unlock(&dc->lock);
	mutex_unlock(&dc->one_shot_lp_lock);
}

static int tegra_dsi_host_suspend(struct tegra_dc *dc)
{
	int err = 0;
	struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);

	if (!dsi->enabled)
		return -EINVAL;

	if (dsi->host_suspended)
		return 0;

	while (!tegra_dsi_host_suspend_trylock(dc, dsi))
		cond_resched();

	tegra_dc_io_start(dc);

	dsi->host_suspended = true;

	tegra_dsi_stop_dc_stream(dc, dsi);

	tegra_dsi_te_on_off(dsi, false);

	err = _tegra_dsi_host_suspend(dc, dsi, dsi->info.suspend_aggr);
	if (err < 0) {
		dev_err(&dc->ndev->dev,
			"DSI host suspend failed\n");
		goto fail;
	}

	/* Shutting down. Drop any reference to dc clk */
	while (tegra_is_clk_enabled(dc->clk))
		tegra_dc_put(dc);

	pm_runtime_put_sync(&dc->ndev->dev);
fail:
	tegra_dc_io_end(dc);
	tegra_dsi_host_suspend_unlock(dc, dsi);
	return err;
}

static bool tegra_dc_dsi_osidle(struct tegra_dc *dc)
{
	struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);

	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE)
		return dsi->host_suspended;
	else
		return false;
}

static void tegra_dsi_bl_off(struct backlight_device *bd)
{
	if (!bd)
		return;

	bd->props.brightness = 0;
	backlight_update_status(bd);
}

static int tegra_dsi_deep_sleep(struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi)
{
	int val = 0;
	int err = 0;

	if (!dsi->enabled)
		return 0;

	tegra_dsi_bl_off(get_backlight_device_by_name(dsi->info.bl_name));

	err = tegra_dsi_set_to_lp_mode(dc, dsi, DSI_LP_OP_WRITE);
	if (err < 0) {
		dev_err(&dc->ndev->dev,
		"DSI failed to go to LP mode\n");
		goto fail;
	}

	/* Suspend DSI panel */
	err = tegra_dsi_send_panel_cmd(dc, dsi,
			dsi->info.dsi_suspend_cmd,
			dsi->info.n_suspend_cmd);

	/*
	 * Certain panels need dc frames be sent after
	 * putting panel to sleep.
	 */
	if (dsi->info.panel_send_dc_frames)
		tegra_dsi_send_dc_frames(dc, dsi, 2);

	if (err < 0) {
		dev_err(&dc->ndev->dev,
			"dsi: Error sending suspend cmd\n");
		goto fail;
	}

	if (!dsi->ulpm) {
		err = tegra_dsi_enter_ulpm(dsi);
		if (err < 0) {
			dev_err(&dc->ndev->dev,
				"DSI failed to enter ulpm\n");
			goto fail;
		}
	}

	tegra_dsi_pad_disable(dsi);

	/* Suspend core-logic */
	val = DSI_POWER_CONTROL_LEG_DSI_ENABLE(TEGRA_DSI_DISABLE);
	tegra_dsi_writel(dsi, val, DSI_POWER_CONTROL);

	/* Disable dsi fast and slow clock */
	tegra_dsi_config_phy_clk(dsi, TEGRA_DSI_DISABLE);

	/* Disable dsi source clock */
	tegra_dsi_clk_disable(dsi);

	dsi->enabled = false;
	dsi->host_suspended = true;

	return 0;
fail:
	return err;
}

static void tegra_dc_dsi_postpoweroff(struct tegra_dc *dc)
{
	struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);

	if (!dsi->enabled)
		regulator_disable(dsi->avdd_dsi_csi);
}

static int tegra_dsi_host_resume(struct tegra_dc *dc)
{
	int err = 0;
	struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);

	if (!dsi->enabled)
		return -EINVAL;

	cancel_delayed_work(&dsi->idle_work);

	mutex_lock(&dsi->host_lock);
	if (!dsi->host_suspended) {
		mutex_unlock(&dsi->host_lock);
		return 0;
	}

	tegra_dc_io_start(dc);

	pm_runtime_get_sync(&dc->ndev->dev);

	err = _tegra_dsi_host_resume(dc, dsi, dsi->info.suspend_aggr);
	if (err < 0) {
		dev_err(&dc->ndev->dev,
			"DSI host resume failed\n");
		goto fail;
	}

	tegra_dsi_te_on_off(dsi, true);

	tegra_dsi_start_dc_stream(dc, dsi);
	dsi->host_suspended = false;
fail:
	tegra_dc_io_end(dc);
	mutex_unlock(&dsi->host_lock);
	return err;
}

static void tegra_dc_dsi_disable(struct tegra_dc *dc)
{
	int err;
	struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);

	if (dsi->host_suspended)
		tegra_dsi_host_resume(dc);

	mutex_lock(&dsi->lock);
	tegra_dc_io_start(dc);

	if (dsi->status.dc_stream == DSI_DC_STREAM_ENABLE)
		tegra_dsi_stop_dc_stream_at_frame_end(dc, dsi, 2);

	if (dsi->out_ops && dsi->out_ops->disable)
		dsi->out_ops->disable(dsi);

	if (dsi->info.power_saving_suspend) {
		if (tegra_dsi_deep_sleep(dc, dsi) < 0) {
			dev_err(&dc->ndev->dev,
				"DSI failed to enter deep sleep\n");
			goto fail;
		}
	} else {
		if (dsi->info.dsi_early_suspend_cmd) {
			err = tegra_dsi_send_panel_cmd(dc, dsi,
				dsi->info.dsi_early_suspend_cmd,
				dsi->info.n_early_suspend_cmd);
			if (err < 0) {
				dev_err(&dc->ndev->dev,
				"dsi: Error sending early suspend cmd\n");
				goto fail;
			}
		}

		if (!dsi->ulpm) {
			if (tegra_dsi_enter_ulpm(dsi) < 0) {
				dev_err(&dc->ndev->dev,
					"DSI failed to enter ulpm\n");
				goto fail;
			}
		}
	}
fail:
	mutex_unlock(&dsi->lock);
	tegra_dc_io_end(dc);
}

#ifdef CONFIG_PM
static void tegra_dc_dsi_suspend(struct tegra_dc *dc)
{
	struct tegra_dc_dsi_data *dsi;

	dsi = tegra_dc_get_outdata(dc);

	if (!dsi->enabled)
		return;

	if (dsi->host_suspended)
		tegra_dsi_host_resume(dc);

	tegra_dc_io_start(dc);
	mutex_lock(&dsi->lock);

	if (dsi->out_ops && dsi->out_ops->suspend)
		dsi->out_ops->suspend(dsi);

	if (!dsi->info.power_saving_suspend) {
		if (dsi->ulpm) {
			if (tegra_dsi_exit_ulpm(dsi) < 0) {
				dev_err(&dc->ndev->dev,
					"DSI failed to exit ulpm");
				goto fail;
			}
		}

		if (tegra_dsi_deep_sleep(dc, dsi) < 0) {
			dev_err(&dc->ndev->dev,
				"DSI failed to enter deep sleep\n");
			goto fail;
		}
	}
fail:
	mutex_unlock(&dsi->lock);
	tegra_dc_io_end(dc);
}

static void tegra_dc_dsi_resume(struct tegra_dc *dc)
{
	struct tegra_dc_dsi_data *dsi;

	dsi = tegra_dc_get_outdata(dc);

	/* No dsi config required since tegra_dc_dsi_enable
	 * will reconfigure the controller from scratch
	 */

	 if (dsi->out_ops && dsi->out_ops->resume)
		dsi->out_ops->resume(dsi);
}
#endif

static int tegra_dc_dsi_init(struct tegra_dc *dc)
{
	struct tegra_dc_dsi_data *dsi;
	int err = 0;

	err = _tegra_dc_dsi_init(dc);
	if (err < 0) {
		dev_err(&dc->ndev->dev,
			"dsi: Instance A init failed\n");
		goto err;
	}

	dsi = tegra_dc_get_outdata(dc);

	dsi->avdd_dsi_csi =  regulator_get(&dc->ndev->dev, "avdd_dsi_csi");
	if (IS_ERR_OR_NULL(dsi->avdd_dsi_csi)) {
		dev_err(&dc->ndev->dev, "dsi: avdd_dsi_csi reg get failed\n");
		err = PTR_ERR(dsi->avdd_dsi_csi);
		goto err_reg;
	}

	dsi->mipi_cal = tegra_mipi_cal_init_sw(dc);
	if (IS_ERR(dsi->mipi_cal)) {
		dev_err(&dc->ndev->dev, "dsi: mipi_cal sw init failed\n");
		err = PTR_ERR(dsi->mipi_cal);
		goto err_mipi;
	}
	return 0;
err_mipi:
	regulator_put(dsi->avdd_dsi_csi);
err_reg:
	_tegra_dc_dsi_destroy(dc);
	tegra_dc_set_outdata(dc, NULL);
err:
	return err;
}

static void tegra_dc_dsi_destroy(struct tegra_dc *dc)
{
	struct regulator *avdd_dsi_csi;
	struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);

	avdd_dsi_csi = dsi->avdd_dsi_csi;

	_tegra_dc_dsi_destroy(dc);
	regulator_put(avdd_dsi_csi);
	tegra_mipi_cal_destroy(dc);
}

static long tegra_dc_dsi_setup_clk(struct tegra_dc *dc, struct clk *clk)
{
	unsigned long rate;
	struct clk *parent_clk;
	struct clk *base_clk;
	struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);

	/* divide by 1000 to avoid overflow */
	dc->mode.pclk /= 1000;
	rate = (dc->mode.pclk * dc->shift_clk_div.mul * 2)
				/ dc->shift_clk_div.div;
	rate *= 1000;
	dc->mode.pclk *= 1000;

	if (dc->out->flags & TEGRA_DC_OUT_INITIALIZED_MODE)
		goto skip_setup;

	if (clk == dc->clk) {
		parent_clk = clk_get_sys(NULL,
				dc->out->parent_clk ? : "pll_d_out0");
		base_clk = clk_get_parent(parent_clk);
	} else {
		if (dc->pdata->default_out->dsi->dsi_instance) {
			parent_clk = clk_get_sys(NULL,
				dc->out->parent_clk ? : "pll_d2_out0");
			base_clk = clk_get_parent(parent_clk);
		} else {
			parent_clk = clk_get_sys(NULL,
				dc->out->parent_clk ? : "pll_d_out0");
			base_clk = clk_get_parent(parent_clk);
		}
	}
	tegra_dsi_config_phy_clk(dsi, TEGRA_DSI_ENABLE);

	if (rate != clk_get_rate(base_clk))
		clk_set_rate(base_clk, rate);

	if (clk_get_parent(clk) != parent_clk)
		clk_set_parent(clk, parent_clk);

skip_setup:
	tegra_dvfs_set_rate(dc->clk, dc->mode.pclk);

	return tegra_dc_pclk_round_rate(dc, dc->mode.pclk);
}

struct tegra_dc_out_ops tegra_dc_dsi_ops = {
	.init = tegra_dc_dsi_init,
	.destroy = tegra_dc_dsi_destroy,
	.enable = tegra_dc_dsi_enable,
	.postpoweron = tegra_dc_dsi_postpoweron,
	.disable = tegra_dc_dsi_disable,
	.postpoweroff = tegra_dc_dsi_postpoweroff,
	.hold = tegra_dc_dsi_hold_host,
	.release = tegra_dc_dsi_release_host,
#ifdef CONFIG_PM
	.suspend = tegra_dc_dsi_suspend,
	.resume = tegra_dc_dsi_resume,
#endif
	.setup_clk = tegra_dc_dsi_setup_clk,
	.osidle = tegra_dc_dsi_osidle,
};
