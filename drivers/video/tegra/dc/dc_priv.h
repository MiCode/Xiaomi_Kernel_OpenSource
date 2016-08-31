/*
 * drivers/video/tegra/dc/dc_priv.h
 *
 * Copyright (C) 2010 Google, Inc.
 * Author: Erik Gilling <konkers@android.com>
 *
 * Copyright (c) 2010-2013, NVIDIA CORPORATION, All rights reserved.
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

#ifndef __DRIVERS_VIDEO_TEGRA_DC_DC_PRIV_H
#define __DRIVERS_VIDEO_TEGRA_DC_DC_PRIV_H

#include "dc_priv_defs.h"
#ifndef CREATE_TRACE_POINTS
# include <trace/events/display.h>
#define WIN_IS_BLOCKLINEAR(win)	((win)->flags & TEGRA_WIN_FLAG_BLOCKLINEAR)
#endif
#include <linux/tegra-powergate.h>
#include <video/tegra_dc_ext.h>


#define WIN_IS_TILED(win)	((win)->flags & TEGRA_WIN_FLAG_TILED)
#define WIN_IS_ENABLED(win)	((win)->flags & TEGRA_WIN_FLAG_ENABLED)
#define WIN_IS_FB(win)		((win)->flags & TEGRA_WIN_FLAG_FB)

#define WIN_IS_INTERLACE(win) ((win)->flags & TEGRA_WIN_FLAG_INTERLACE)

#if defined(CONFIG_ARCH_TEGRA_14x_SOC)
#define WIN_ALL_ACT_REQ (WIN_A_ACT_REQ | WIN_B_ACT_REQ | WIN_C_ACT_REQ | \
	WIN_D_ACT_REQ | WIN_H_ACT_REQ)
#else
#define WIN_ALL_ACT_REQ (WIN_A_ACT_REQ | WIN_B_ACT_REQ | WIN_C_ACT_REQ)
#endif

static inline void tegra_dc_io_start(struct tegra_dc *dc)
{
	nvhost_module_busy_ext(dc->ndev);
}

static inline void tegra_dc_io_end(struct tegra_dc *dc)
{
	nvhost_module_idle_ext(dc->ndev);
}

static inline unsigned long tegra_dc_readl(struct tegra_dc *dc,
					   unsigned long reg)
{
	unsigned long ret;

	BUG_ON(!nvhost_module_powered_ext(dc->ndev));

	if (WARN(!tegra_is_clk_enabled(dc->clk), "DC is clock-gated.\n") ||
		WARN(!tegra_powergate_is_powered(dc->powergate_id),
			"DC is power-gated.\n"))
		return 0;

	ret = readl(dc->base + reg * 4);
	trace_display_readl(dc, ret, dc->base + reg * 4);
	return ret;
}

static inline void tegra_dc_writel(struct tegra_dc *dc, unsigned long val,
				   unsigned long reg)
{
	BUG_ON(!nvhost_module_powered_ext(dc->ndev));

	if (WARN(!tegra_is_clk_enabled(dc->clk), "DC is clock-gated.\n") ||
		WARN(!tegra_powergate_is_powered(dc->powergate_id),
			"DC is power-gated.\n"))
		return;

	trace_display_writel(dc, val, dc->base + reg * 4);
	writel(val, dc->base + reg * 4);
}

static inline void tegra_dc_power_on(struct tegra_dc *dc)
{
	tegra_dc_writel(dc, PW0_ENABLE | PW1_ENABLE | PW2_ENABLE | PW3_ENABLE |
					PW4_ENABLE | PM0_ENABLE | PM1_ENABLE,
					DC_CMD_DISPLAY_POWER_CONTROL);
}

static inline void _tegra_dc_write_table(struct tegra_dc *dc, const u32 *table,
					 unsigned len)
{
	int i;

	for (i = 0; i < len; i++)
		tegra_dc_writel(dc, table[i * 2 + 1], table[i * 2]);
}

#define tegra_dc_write_table(dc, table)		\
	_tegra_dc_write_table(dc, table, ARRAY_SIZE(table) / 2)

static inline void tegra_dc_set_outdata(struct tegra_dc *dc, void *data)
{
	dc->out_data = data;
}

static inline void *tegra_dc_get_outdata(struct tegra_dc *dc)
{
	return dc->out_data;
}

static inline unsigned long tegra_dc_get_default_emc_clk_rate(
	struct tegra_dc *dc)
{
	return dc->pdata->emc_clk_rate ? dc->pdata->emc_clk_rate : ULONG_MAX;
}

/* return the color format field */
static inline int tegra_dc_fmt(int fmt)
{
	return (fmt & TEGRA_DC_EXT_FMT_MASK) >> TEGRA_DC_EXT_FMT_SHIFT;
}

/* return the byte swap field */
static inline int tegra_dc_fmt_byteorder(int fmt)
{
	return (fmt & TEGRA_DC_EXT_FMT_BYTEORDER_MASK) >>
		TEGRA_DC_EXT_FMT_BYTEORDER_SHIFT;
}

static inline int tegra_dc_fmt_bpp(int fmt)
{
	switch (tegra_dc_fmt(fmt)) {
	case TEGRA_WIN_FMT_P1:
		return 1;

	case TEGRA_WIN_FMT_P2:
		return 2;

	case TEGRA_WIN_FMT_P4:
		return 4;

	case TEGRA_WIN_FMT_P8:
		return 8;

	case TEGRA_WIN_FMT_B4G4R4A4:
	case TEGRA_WIN_FMT_B5G5R5A:
	case TEGRA_WIN_FMT_B5G6R5:
	case TEGRA_WIN_FMT_AB5G5R5:
		return 16;

	case TEGRA_WIN_FMT_B8G8R8A8:
	case TEGRA_WIN_FMT_R8G8B8A8:
	case TEGRA_WIN_FMT_B6x2G6x2R6x2A8:
	case TEGRA_WIN_FMT_R6x2G6x2B6x2A8:
		return 32;

	/* for planar formats, size of the Y plane, 8bit */
	case TEGRA_WIN_FMT_YCbCr420P:
	case TEGRA_WIN_FMT_YUV420P:
	case TEGRA_WIN_FMT_YCbCr422P:
	case TEGRA_WIN_FMT_YUV422P:
	case TEGRA_WIN_FMT_YCbCr422R:
	case TEGRA_WIN_FMT_YUV422R:
	case TEGRA_WIN_FMT_YCbCr422RA:
	case TEGRA_WIN_FMT_YUV422RA:
	case TEGRA_WIN_FMT_YCbCr444P:
	case TEGRA_WIN_FMT_YUV444P:
	case TEGRA_WIN_FMT_YUV422SP:
	case TEGRA_WIN_FMT_YUV420SP:
	case TEGRA_WIN_FMT_YCbCr420SP:
	case TEGRA_WIN_FMT_YCbCr422SP:
		return 8;

	/* YUYV packed into 32-bits */
	case TEGRA_WIN_FMT_YCbCr422:
	case TEGRA_WIN_FMT_YUV422:
		return 16;
	}
	return 0;
}

static inline bool tegra_dc_is_yuv(int fmt)
{
	switch (tegra_dc_fmt(fmt)) {
	case TEGRA_WIN_FMT_YUV420P:
	case TEGRA_WIN_FMT_YCbCr420P:
	case TEGRA_WIN_FMT_YCbCr422P:
	case TEGRA_WIN_FMT_YUV422P:
	case TEGRA_WIN_FMT_YCbCr422:
	case TEGRA_WIN_FMT_YUV422:
	case TEGRA_WIN_FMT_YCbCr422R:
	case TEGRA_WIN_FMT_YUV422R:
	case TEGRA_WIN_FMT_YCbCr422RA:
	case TEGRA_WIN_FMT_YUV422RA:
	case TEGRA_WIN_FMT_YCbCr444P:
	case TEGRA_WIN_FMT_YUV444P:
	case TEGRA_WIN_FMT_YUV422SP:
	case TEGRA_WIN_FMT_YCbCr422SP:
	case TEGRA_WIN_FMT_YCbCr420SP:
	case TEGRA_WIN_FMT_YUV420SP:
		return true;
	}
	return false;
}

static inline bool tegra_dc_is_yuv_planar(int fmt)
{
	switch (tegra_dc_fmt(fmt)) {
	case TEGRA_WIN_FMT_YUV420P:
	case TEGRA_WIN_FMT_YCbCr420P:
	case TEGRA_WIN_FMT_YCbCr422P:
	case TEGRA_WIN_FMT_YUV422P:
	case TEGRA_WIN_FMT_YCbCr422R:
	case TEGRA_WIN_FMT_YUV422R:
	case TEGRA_WIN_FMT_YCbCr422RA:
	case TEGRA_WIN_FMT_YUV422RA:
	case TEGRA_WIN_FMT_YCbCr444P:
	case TEGRA_WIN_FMT_YUV444P:
		return true;
	}
	return false;
}

static inline bool tegra_dc_is_yuv_full_planar(int fmt)
{
	switch (fmt) {
	case TEGRA_WIN_FMT_YCbCr444P:
	case TEGRA_WIN_FMT_YUV444P:
		return true;
	}
	return false;
}

static inline bool tegra_dc_is_yuv_semi_planar(int fmt)
{
	switch (fmt) {
	case TEGRA_WIN_FMT_YUV420SP:
	case TEGRA_WIN_FMT_YCbCr420SP:
	case TEGRA_WIN_FMT_YCbCr422SP:
	case TEGRA_WIN_FMT_YUV422SP:
		return true;
	}
	return false;
}

static inline u32 tegra_dc_unmask_interrupt(struct tegra_dc *dc, u32 int_val)
{
	u32 val;

	val = tegra_dc_readl(dc, DC_CMD_INT_MASK);
	tegra_dc_writel(dc, val | int_val, DC_CMD_INT_MASK);
	return val;
}

static inline u32 tegra_dc_flush_interrupt(struct tegra_dc *dc, u32 int_val)
{
	u32 val;
	unsigned long flag;

	local_irq_save(flag);

	val = tegra_dc_readl(dc, DC_CMD_INT_STATUS);
	tegra_dc_writel(dc, (val | int_val), DC_CMD_INT_STATUS);

	local_irq_restore(flag);

	return val;
}

static inline u32 tegra_dc_mask_interrupt(struct tegra_dc *dc, u32 int_val)
{
	u32 val;

	val = tegra_dc_readl(dc, DC_CMD_INT_MASK);
	tegra_dc_writel(dc, val & ~int_val, DC_CMD_INT_MASK);
	return val;
}

static inline void tegra_dc_restore_interrupt(struct tegra_dc *dc, u32 val)
{
	tegra_dc_writel(dc, val, DC_CMD_INT_MASK);
}

static inline unsigned long tegra_dc_clk_get_rate(struct tegra_dc *dc)
{
	if (!tegra_platform_is_silicon())
		return dc->mode.pclk;

	return clk_get_rate(dc->clk);
}

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && !defined(CONFIG_ARCH_TEGRA_3x_SOC)
static inline void tegra_dc_powergate_locked(struct tegra_dc *dc)
{
	tegra_powergate_partition(dc->powergate_id);
}

static inline void tegra_dc_unpowergate_locked(struct tegra_dc *dc)
{
	int ret;
	ret = tegra_unpowergate_partition(dc->powergate_id);
	if (ret < 0)
		dev_err(&dc->ndev->dev, "%s: could not unpowergate %d\n",
							__func__, ret);
}

static inline bool tegra_dc_is_powered(struct tegra_dc *dc)
{
	return tegra_powergate_is_powered(dc->powergate_id);
}

void tegra_dc_powergate_locked(struct tegra_dc *dc);
void tegra_dc_unpowergate_locked(struct tegra_dc *dc);
#else
static inline void tegra_dc_powergate_locked(struct tegra_dc *dc) { }
static inline void tegra_dc_unpowergate_locked(struct tegra_dc *dc) { }
static inline bool tegra_dc_is_powered(struct tegra_dc *dc)
{
	return true;
}
#endif

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
static inline u32 tegra_dc_reg_l32(dma_addr_t reg)
{
	return reg & 0xffffffff;
}

static inline u32 tegra_dc_reg_h32(dma_addr_t reg)
{
	return reg >> 32;
}
#endif
extern struct tegra_dc_out_ops tegra_dc_rgb_ops;
extern struct tegra_dc_out_ops tegra_dc_hdmi_ops;
extern struct tegra_dc_out_ops tegra_dc_dsi_ops;
#ifdef CONFIG_TEGRA_DP
extern struct tegra_dc_out_ops tegra_dc_dp_ops;
#endif
#ifdef CONFIG_TEGRA_LVDS
extern struct tegra_dc_out_ops tegra_dc_lvds_ops;
#endif

/* defined in dc_sysfs.c, used by dc.c */
void tegra_dc_remove_sysfs(struct device *dev);
void tegra_dc_create_sysfs(struct device *dev);

/* defined in dc.c, used by dc_sysfs.c */
void tegra_dc_stats_enable(struct tegra_dc *dc, bool enable);
bool tegra_dc_stats_get(struct tegra_dc *dc);

/* defined in dc.c, used by dc_sysfs.c */
u32 tegra_dc_read_checksum_latched(struct tegra_dc *dc);
void tegra_dc_enable_crc(struct tegra_dc *dc);
void tegra_dc_disable_crc(struct tegra_dc *dc);

void tegra_dc_set_out_pin_polars(struct tegra_dc *dc,
				const struct tegra_dc_out_pin *pins,
				const unsigned int n_pins);
/* defined in dc.c, used in bandwidth.c and ext/dev.c */
unsigned int tegra_dc_has_multiple_dc(void);

/* defined in dc.c, used in dsi.c */
void tegra_dc_clk_enable(struct tegra_dc *dc);
void tegra_dc_clk_disable(struct tegra_dc *dc);

/* defined in dc.c, used in nvsd.c and dsi.c */
void tegra_dc_get(struct tegra_dc *dc);
void tegra_dc_put(struct tegra_dc *dc);

/* defined in dc.c, used in window.c */
void tegra_dc_hold_dc_out(struct tegra_dc *dc);
void tegra_dc_release_dc_out(struct tegra_dc *dc);

/* defined in bandwidth.c, used in dc.c */
void tegra_dc_clear_bandwidth(struct tegra_dc *dc);
void tegra_dc_program_bandwidth(struct tegra_dc *dc, bool use_new);
int tegra_dc_set_dynamic_emc(struct tegra_dc *dc);
#ifdef CONFIG_TEGRA_ISOMGR
int tegra_dc_bandwidth_negotiate_bw(struct tegra_dc *dc,
			struct tegra_dc_win *windows[], int n);
void tegra_dc_bandwidth_renegotiate(void *p, u32 avail_bw);
#endif
long tegra_dc_calc_min_bandwidth(struct tegra_dc *dc);

/* defined in mode.c, used in dc.c and window.c */
int tegra_dc_program_mode(struct tegra_dc *dc, struct tegra_dc_mode *mode);
int tegra_dc_calc_refresh(const struct tegra_dc_mode *m);
int tegra_dc_update_mode(struct tegra_dc *dc);

/* defined in clock.c, used in dc.c, rgb.c, dsi.c and hdmi.c */
void tegra_dc_setup_clk(struct tegra_dc *dc, struct clk *clk);
unsigned long tegra_dc_pclk_round_rate(struct tegra_dc *dc, int pclk);
unsigned long tegra_dc_pclk_predict_rate(struct clk *parent, int pclk);

/* defined in lut.c, used in dc.c */
void tegra_dc_init_lut_defaults(struct tegra_dc_lut *lut);
void tegra_dc_set_lut(struct tegra_dc *dc, struct tegra_dc_win *win);

/* defined in csc.c, used in dc.c */
void tegra_dc_init_csc_defaults(struct tegra_dc_csc *csc);
void tegra_dc_set_csc(struct tegra_dc *dc, struct tegra_dc_csc *csc);

/* defined in window.c, used in dc.c */
void tegra_dc_trigger_windows(struct tegra_dc *dc);

void tegra_dc_set_color_control(struct tegra_dc *dc);
#ifdef CONFIG_TEGRA_DC_CMU
void tegra_dc_cmu_enable(struct tegra_dc *dc, bool cmu_enable);
int tegra_dc_update_cmu(struct tegra_dc *dc, struct tegra_dc_cmu *cmu);
#endif

struct tegra_dc_platform_data
	*of_dc_parse_platform_data(struct platform_device *ndev);

#endif
