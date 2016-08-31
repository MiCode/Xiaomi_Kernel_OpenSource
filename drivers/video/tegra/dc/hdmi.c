/*
 * drivers/video/tegra/dc/hdmi.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Author: Erik Gilling <konkers@android.com>
 *
 * Copyright (c) 2010-2014, NVIDIA CORPORATION, All rights reserved.
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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#ifdef CONFIG_SWITCH
#include <linux/switch.h>
#endif
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/device.h>
#include <linux/export.h>
#include <linux/clk/tegra.h>

#include <mach/dc.h>
#include <mach/fb.h>
#include <linux/nvhost.h>
#include <mach/hdmi-audio.h>

#include <video/tegrafb.h>

#include "dc_reg.h"
#include "dc_priv.h"
#include "hdmi_reg.h"
#include "hdmi.h"
#include "hdmi_state_machine.h"
#include "edid.h"
#include "nvhdcp.h"
#include <linux/of.h>
#include <linux/of_address.h>

/* datasheet claims this will always be 216MHz */
#define HDMI_AUDIOCLK_FREQ		216000000

#define HDMI_REKEY_DEFAULT		56

#define HDMI_ELD_RESERVED1_INDEX		1
#define HDMI_ELD_RESERVED2_INDEX		3
#define HDMI_ELD_VER_INDEX			0
#define HDMI_ELD_BASELINE_LEN_INDEX		2
#define HDMI_ELD_CEA_VER_MNL_INDEX		4
#define HDMI_ELD_SAD_CNT_CON_TYP_SAI_HDCP_INDEX		5
#define HDMI_ELD_AUD_SYNC_DELAY_INDEX	6
#define HDMI_ELD_SPK_ALLOC_INDEX		7
#define HDMI_ELD_PORT_ID_INDEX		8
#define HDMI_ELD_MANF_NAME_INDEX		16
#define HDMI_ELD_PRODUCT_CODE_INDEX		18
#define HDMI_ELD_MONITOR_NAME_INDEX		20

/* These two values need to be cross checked in case of
     addition/removal from tegra_dc_hdmi_aspect_ratios[] */
#define TEGRA_DC_HDMI_MIN_ASPECT_RATIO_PERCENT	80
#define TEGRA_DC_HDMI_MAX_ASPECT_RATIO_PERCENT	320

struct tegra_dc_hdmi_data *dc_hdmi;

#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
const struct tmds_config tmds_config[] = {
	{ /* 480p modes */
	.pclk = 27000000,
	.pll0 = SOR_PLL_BG_V17_S(3) | SOR_PLL_ICHPMP(1) |
		SOR_PLL_RESISTORSEL_EXT | SOR_PLL_VCOCAP(0) |
		SOR_PLL_TX_REG_LOAD(0),
	.pll1 = SOR_PLL_TMDS_TERM_ENABLE,
	.pe_current = 0x00000000,
	.drive_current = 0x0a0a0a0a,
	},
	{ /* 720p modes */
	.pclk = 74250000,
	.pll0 = SOR_PLL_BG_V17_S(3) | SOR_PLL_ICHPMP(1) |
		SOR_PLL_RESISTORSEL_EXT | SOR_PLL_VCOCAP(1) |
		SOR_PLL_TX_REG_LOAD(0),
	.pll1 = SOR_PLL_TMDS_TERM_ENABLE | SOR_PLL_PE_EN,
	.pe_current = 0x0a0a0a0a,
	.drive_current = 0x0a0a0a0a,
	},
	{ /* 1080p modes */
	.pclk = INT_MAX,
	.pll0 = SOR_PLL_BG_V17_S(3) | SOR_PLL_ICHPMP(1) |
		SOR_PLL_RESISTORSEL_EXT | SOR_PLL_VCOCAP(3) |
		SOR_PLL_TX_REG_LOAD(0),
	.pll1 = SOR_PLL_TMDS_TERM_ENABLE | SOR_PLL_PE_EN,
	.pe_current = 0x0a0a0a0a,
	.drive_current = 0x0a0a0a0a,
	},
};
#elif defined(CONFIG_ARCH_TEGRA_2x_SOC)
const struct tmds_config tmds_config[] = {
	{ /* 480p modes */
	.pclk = 27000000,
	.pll0 = SOR_PLL_BG_V17_S(3) | SOR_PLL_ICHPMP(1) |
		SOR_PLL_RESISTORSEL_EXT | SOR_PLL_VCOCAP(0) |
		SOR_PLL_TX_REG_LOAD(3),
	.pll1 = SOR_PLL_TMDS_TERM_ENABLE,
	.pe_current = 0x00000000,
	.drive_current = 0x0f0f0f0f,
	},
	{ /* 720p modes */
	.pclk = 74250000,
	.pll0 = SOR_PLL_BG_V17_S(3) | SOR_PLL_ICHPMP(1) |
		SOR_PLL_RESISTORSEL_EXT | SOR_PLL_VCOCAP(1) |
		SOR_PLL_TX_REG_LOAD(3),
	.pll1 = SOR_PLL_TMDS_TERM_ENABLE | SOR_PLL_PE_EN,
	.pe_current = 0x0c0c0c0c,
	.drive_current = 0x0f0f0f0f,
	},
	{ /* 1080p modes */
	.pclk = INT_MAX,
	.pll0 = SOR_PLL_BG_V17_S(3) | SOR_PLL_ICHPMP(1) |
		SOR_PLL_RESISTORSEL_EXT | SOR_PLL_VCOCAP(1) |
		SOR_PLL_TX_REG_LOAD(3),
	.pll1 = SOR_PLL_TMDS_TERM_ENABLE | SOR_PLL_PE_EN,
	.pe_current = 0x0c0c0c0c,
	.drive_current = 0x0f0f0f0f,
	},
};
#elif defined(CONFIG_ARCH_TEGRA_11x_SOC)
const struct tmds_config tmds_config[] = {
	{ /* 480p/576p / 25.2MHz/27MHz modes */
	.pclk = 27000000,
	.pll0 = SOR_PLL_ICHPMP(1) | SOR_PLL_BG_V17_S(3) |
		SOR_PLL_VCOCAP(0) | SOR_PLL_RESISTORSEL_EXT,
	.pll1 = SOR_PLL_LOADADJ(3) | SOR_PLL_TMDS_TERMADJ(0),
	.pe_current = 0x00000000,
	.drive_current = 0x1a1a1a1a,
	.peak_current = 0x00000000,
	},
	{ /* 720p / 74.25MHz modes */
	.pclk = 74250000,
	.pll0 = SOR_PLL_ICHPMP(1) | SOR_PLL_BG_V17_S(3) |
		SOR_PLL_VCOCAP(1) | SOR_PLL_RESISTORSEL_EXT,
	.pll1 = SOR_PLL_PE_EN | SOR_PLL_LOADADJ(3) | SOR_PLL_TMDS_TERMADJ(0),
	.pe_current = 0x0f0f0f0f,
	.drive_current = 0x1a1a1a1a,
	.peak_current = 0x00000000,
	},
	{ /* 1080p / 148.5MHz modes */
	.pclk = 148500000,
	.pll0 = SOR_PLL_ICHPMP(1) | SOR_PLL_BG_V17_S(3) |
		SOR_PLL_VCOCAP(3) | SOR_PLL_RESISTORSEL_EXT,
	.pll1 = SOR_PLL_PE_EN | SOR_PLL_LOADADJ(3) | SOR_PLL_TMDS_TERMADJ(0),
	.pe_current = 0x0a0a0a0a,
	.drive_current = 0x1f1f1f1f,
	.peak_current = 0x00000000,
	},
	{ /* 225/297MHz modes */
	.pclk = INT_MAX,
	.pll0 = SOR_PLL_ICHPMP(1) | SOR_PLL_BG_V17_S(3) |
		SOR_PLL_VCOCAP(0xf) | SOR_PLL_RESISTORSEL_EXT,
	.pll1 = SOR_PLL_LOADADJ(3) | SOR_PLL_TMDS_TERMADJ(7)
		| SOR_PLL_TMDS_TERM_ENABLE,
	.pe_current = 0x00000000,
	.drive_current = 0x303f3f3f, /* lane3 needs a slightly lower current */
	.peak_current = 0x040f0f0f,
	/* Alternative settings:
	.drive_current = 0x2c333333,
	.pll1 = SOR_PLL_LOADADJ(3) | SOR_PLL_TMDS_TERMADJ(6)
		| SOR_PLL_TMDS_TERM_ENABLE,
	*/
	},
};
#elif defined(CONFIG_ARCH_TEGRA_12x_SOC)
const struct tmds_config tmds_config[] = {
	{ /* 480p/576p / 25.2MHz/27MHz modes */
	.version = MKDEV(1, 0),
	.pclk = 27000000,
	.pll0 = SOR_PLL_ICHPMP(1) | SOR_PLL_BG_V17_S(3) |
		SOR_PLL_VCOCAP(0) | SOR_PLL_RESISTORSEL_EXT,
	.pll1 = SOR_PLL_LOADADJ(3) | SOR_PLL_TMDS_TERMADJ(0),
	.pe_current = 0x00000000,
	.drive_current = 0x1a1a1a1a,
	.peak_current = 0x00000000,
	.pad_ctls0_mask    = 0xfffff0ff,
	.pad_ctls0_setting = 0x00000400, /* BG_VREF_LEVEL */
	},
	{ /* 720p / 74.25MHz modes */
	.pclk = 74250000,
	.pll0 = SOR_PLL_ICHPMP(1) | SOR_PLL_BG_V17_S(3) |
		SOR_PLL_VCOCAP(1) | SOR_PLL_RESISTORSEL_EXT,
	.pll1 = SOR_PLL_PE_EN | SOR_PLL_LOADADJ(3) | SOR_PLL_TMDS_TERMADJ(0),
	.pe_current = 0x0f0f0f0f,
	.drive_current = 0x1a1a1a1a,
	.peak_current = 0x00000000,
	.pad_ctls0_mask    = 0xfffff0ff,
	.pad_ctls0_setting = 0x00000400, /* BG_VREF_LEVEL */
	},
	{ /* 1080p / 148.5MHz modes */
	.pclk = 148500000,
	.pll0 = SOR_PLL_ICHPMP(1) | SOR_PLL_BG_V17_S(3) |
		SOR_PLL_VCOCAP(3) | SOR_PLL_RESISTORSEL_EXT,
	.pll1 = SOR_PLL_PE_EN | SOR_PLL_LOADADJ(3) | SOR_PLL_TMDS_TERMADJ(0),
	.pe_current = 0x0a0a0a0a,
	.drive_current = 0x1f1f1f1f,
	.peak_current = 0x00000000,
	.pad_ctls0_mask    = 0xfffff0ff,
	.pad_ctls0_setting = 0x00000400, /* BG_VREF_LEVEL */
	},
	{ /* 225/297MHz modes */
	.pclk = INT_MAX,
	.pll0 = SOR_PLL_ICHPMP(1) | SOR_PLL_BG_V17_S(3) |
		SOR_PLL_VCOCAP(0xf) | SOR_PLL_RESISTORSEL_EXT,
	.pll1 = SOR_PLL_LOADADJ(3) | SOR_PLL_TMDS_TERMADJ(7)
		| SOR_PLL_TMDS_TERM_ENABLE,
	.pe_current = 0x00000000,
	.drive_current = 0x303f3f3f, /* lane3 needs a slightly lower current */
	.peak_current = 0x040f0f0f,
	.pad_ctls0_mask    = 0xfffff0ff,
	.pad_ctls0_setting = 0x00000600, /* BG_VREF_LEVEL */
	},
};
#elif defined(CONFIG_ARCH_TEGRA_14x_SOC)
const struct tmds_config tmds_config[] = {
	{ /* 480p modes */
	.pclk = 27000000,
	.pll0 = SOR_PLL_BG_V17_S(3) | SOR_PLL_ICHPMP(1) |
		SOR_PLL_RESISTORSEL_EXT | SOR_PLL_VCOCAP(0x0) |
		SOR_PLL_TX_REG_LOAD(0),
	.pll1 = SOR_PLL_TMDS_TERM_ENABLE |
		SOR_PLL_TMDS_TERMADJ(0xD) | SOR_PLL_LOADADJ(3),
	.pe_current = 0x0,
	.drive_current = 0x1f1f1f1f,
	.peak_current = 0x0,
	},
	{ /* 720p modes */
	.pclk = 74250000,
	.pll0 = SOR_PLL_BG_V17_S(3) | SOR_PLL_ICHPMP(1) |
		SOR_PLL_RESISTORSEL_EXT | SOR_PLL_VCOCAP(0x1) |
		SOR_PLL_TX_REG_LOAD(0),
	.pll1 = SOR_PLL_TMDS_TERM_ENABLE |
		SOR_PLL_TMDS_TERMADJ(0xD) | SOR_PLL_LOADADJ(3),
	.pe_current = 0x0,
	.drive_current = 0x20202020,
	.peak_current = 0x0,
	},
	{ /* 1080p modes */
	.pclk = INT_MAX,
	.pll0 = SOR_PLL_BG_V17_S(3) | SOR_PLL_ICHPMP(1) |
		SOR_PLL_RESISTORSEL_EXT | SOR_PLL_VCOCAP(0x3) |
		SOR_PLL_TX_REG_LOAD(0),
	.pll1 = SOR_PLL_TMDS_TERM_ENABLE |
		SOR_PLL_TMDS_TERMADJ(0xD) | SOR_PLL_LOADADJ(3),
	.pe_current = 0x0,
	.drive_current = 0x22222222,
	.peak_current = 0x04040404,
	},
};
#else
#warning tmds_config needs to be defined for your arch
#endif

struct tegra_hdmi_audio_config {
	unsigned pix_clock;
	unsigned n;
	unsigned cts;
	unsigned aval;
};


const struct tegra_hdmi_audio_config tegra_hdmi_audio_32k[] = {
	{25200000,	4096,	25200,	24000},
	{27000000,	4096,	27000,	24000},
	{74250000,	4096,	74250,	24000},
	{148500000,	4096,	148500,	24000},
	{241500000,	4096,	241500,	24000},
	{297000000,	3072,	222750,	24000},
	{0,		0,	0},
};

const struct tegra_hdmi_audio_config tegra_hdmi_audio_44_1k[] = {
	{25200000,	5880,	26250,	25000},
	{27000000,	5880,	28125,	25000},
	{74250000,	4704,	61875,	20000},
	{148500000,	4704,	123750,	20000},
	{241500000,	4704,	201250,	20000},
	{297000000,	4704,	247500,	20000},
	{0,		0,	0},
};

const struct tegra_hdmi_audio_config tegra_hdmi_audio_48k[] = {
	{25200000,	6144,	25200,	24000},
	{27000000,	6144,	27000,	24000},
	{74250000,	6144,	74250,	24000},
	{148500000,	6144,	148500,	24000},
	{241500000,	5632,	221375,	22000},
	{297000000,	5120,	247500,	24000},
	{0,		0,	0},
};

const struct tegra_hdmi_audio_config tegra_hdmi_audio_88_2k[] = {
	{25200000,	11760,	26250,	25000},
	{27000000,	11760,	28125,	25000},
	{74250000,	9408,	61875,	20000},
	{148500000,	9408,	123750, 20000},
	{241500000,	9408,	201250,	20000},
	{297000000,	9408,	247500, 20000},
	{0,		0,	0},
};

const struct tegra_hdmi_audio_config tegra_hdmi_audio_96k[] = {
	{25200000,	12288,	25200,	24000},
	{27000000,	12288,	27000,	24000},
	{74250000,	12288,	74250,	24000},
	{148500000,	12288,	148500,	24000},
	{241500000,	11264,	221375,	22000},
	{297000000,	10240,	247500,	24000},
	{0,		0,	0},
};

const struct tegra_hdmi_audio_config tegra_hdmi_audio_176_4k[] = {
	{25200000,	23520,	26250,	25000},
	{27000000,	23520,	28125,	25000},
	{74250000,	18816,	61875,	20000},
	{148500000,	18816,	123750,	20000},
	{241500000,	18816,	201250,	20000},
	{297000000,	18816,	247500,	20000},
	{0,		0,	0},
};

const struct tegra_hdmi_audio_config tegra_hdmi_audio_192k[] = {
	{25200000,	24576,	25200,	24000},
	{27000000,	24576,	27000,	24000},
	{74250000,	24576,	74250,	24000},
	{148500000,	24576,	148500,	24000},
	{241500000,	22528,	221375,	22000},
	{297000000,	20480,	247500,	24000},
	{0,		0,	0},
};

static const struct tegra_hdmi_audio_config
*tegra_hdmi_get_audio_config(unsigned audio_freq, unsigned pix_clock)
{
	const struct tegra_hdmi_audio_config *table;

	switch (audio_freq) {
	case AUDIO_FREQ_32K:
		table = tegra_hdmi_audio_32k;
		break;
	case AUDIO_FREQ_44_1K:
		table = tegra_hdmi_audio_44_1k;
		break;
	case AUDIO_FREQ_48K:
		table = tegra_hdmi_audio_48k;
		break;
	case AUDIO_FREQ_88_2K:
		table = tegra_hdmi_audio_88_2k;
		break;
	case AUDIO_FREQ_96K:
		table = tegra_hdmi_audio_96k;
		break;
	case AUDIO_FREQ_176_4K:
		table = tegra_hdmi_audio_176_4k;
		break;
	case AUDIO_FREQ_192K:
		table = tegra_hdmi_audio_192k;
		break;
	default:
		return NULL;
	}

	while (table->pix_clock) {
		if (table->pix_clock > (pix_clock/100*99) &&
                table->pix_clock < (pix_clock/100*101) &&
                table->pix_clock >= 1000)
			return table;
		table++;
	}

	return NULL;
}


unsigned long tegra_hdmi_readl(struct tegra_dc_hdmi_data *hdmi,
					     unsigned long reg)
{
	unsigned long ret;
	ret = readl(hdmi->base + reg * 4);
	trace_display_readl(hdmi->dc, ret, hdmi->base + reg * 4);
	return ret;
}

void tegra_hdmi_writel(struct tegra_dc_hdmi_data *hdmi,
				     unsigned long val, unsigned long reg)
{
	trace_display_writel(hdmi->dc, val, hdmi->base + reg * 4);
	writel(val, hdmi->base + reg * 4);
}

static inline void tegra_hdmi_clrsetbits(struct tegra_dc_hdmi_data *hdmi,
					 unsigned long reg, unsigned long clr,
					 unsigned long set)
{
	unsigned long val = tegra_hdmi_readl(hdmi, reg);
	val &= ~clr;
	val |= set;
	tegra_hdmi_writel(hdmi, val, reg);
}

#ifdef CONFIG_DEBUG_FS
static int dbg_hdmi_show(struct seq_file *m, void *unused)
{
	struct tegra_dc_hdmi_data *hdmi = m->private;

	/* If gated quitely return */
	if (!tegra_dc_is_powered(hdmi->dc))
		return 0;

#define DUMP_REG(a) do {						\
		seq_printf(m, "%-32s\t%03x\t%08lx\n",			\
		       #a, a, tegra_hdmi_readl(hdmi, a));		\
	} while (0)

	tegra_dc_io_start(hdmi->dc);
	clk_prepare_enable(hdmi->clk);

	DUMP_REG(HDMI_CTXSW);
	DUMP_REG(HDMI_NV_PDISP_SOR_STATE0);
	DUMP_REG(HDMI_NV_PDISP_SOR_STATE1);
	DUMP_REG(HDMI_NV_PDISP_SOR_STATE2);
	DUMP_REG(HDMI_NV_PDISP_RG_HDCP_AN_MSB);
	DUMP_REG(HDMI_NV_PDISP_RG_HDCP_AN_LSB);
	DUMP_REG(HDMI_NV_PDISP_RG_HDCP_CN_MSB);
	DUMP_REG(HDMI_NV_PDISP_RG_HDCP_CN_LSB);
	DUMP_REG(HDMI_NV_PDISP_RG_HDCP_AKSV_MSB);
	DUMP_REG(HDMI_NV_PDISP_RG_HDCP_AKSV_LSB);
	DUMP_REG(HDMI_NV_PDISP_RG_HDCP_BKSV_MSB);
	DUMP_REG(HDMI_NV_PDISP_RG_HDCP_BKSV_LSB);
	DUMP_REG(HDMI_NV_PDISP_RG_HDCP_CKSV_MSB);
	DUMP_REG(HDMI_NV_PDISP_RG_HDCP_CKSV_LSB);
	DUMP_REG(HDMI_NV_PDISP_RG_HDCP_DKSV_MSB);
	DUMP_REG(HDMI_NV_PDISP_RG_HDCP_DKSV_LSB);
	DUMP_REG(HDMI_NV_PDISP_RG_HDCP_CTRL);
	DUMP_REG(HDMI_NV_PDISP_RG_HDCP_CMODE);
	DUMP_REG(HDMI_NV_PDISP_RG_HDCP_MPRIME_MSB);
	DUMP_REG(HDMI_NV_PDISP_RG_HDCP_MPRIME_LSB);
	DUMP_REG(HDMI_NV_PDISP_RG_HDCP_SPRIME_MSB);
	DUMP_REG(HDMI_NV_PDISP_RG_HDCP_SPRIME_LSB2);
	DUMP_REG(HDMI_NV_PDISP_RG_HDCP_SPRIME_LSB1);
	DUMP_REG(HDMI_NV_PDISP_RG_HDCP_RI);
	DUMP_REG(HDMI_NV_PDISP_RG_HDCP_CS_MSB);
	DUMP_REG(HDMI_NV_PDISP_RG_HDCP_CS_LSB);
	DUMP_REG(HDMI_NV_PDISP_HDMI_AUDIO_EMU0);
	DUMP_REG(HDMI_NV_PDISP_HDMI_AUDIO_EMU_RDATA0);
	DUMP_REG(HDMI_NV_PDISP_HDMI_AUDIO_EMU1);
	DUMP_REG(HDMI_NV_PDISP_HDMI_AUDIO_EMU2);
	DUMP_REG(HDMI_NV_PDISP_HDMI_AUDIO_INFOFRAME_CTRL);
	DUMP_REG(HDMI_NV_PDISP_HDMI_AUDIO_INFOFRAME_STATUS);
	DUMP_REG(HDMI_NV_PDISP_HDMI_AUDIO_INFOFRAME_HEADER);
	DUMP_REG(HDMI_NV_PDISP_HDMI_AUDIO_INFOFRAME_SUBPACK0_LOW);
	DUMP_REG(HDMI_NV_PDISP_HDMI_AUDIO_INFOFRAME_SUBPACK0_HIGH);
	DUMP_REG(HDMI_NV_PDISP_HDMI_AVI_INFOFRAME_CTRL);
	DUMP_REG(HDMI_NV_PDISP_HDMI_AVI_INFOFRAME_STATUS);
	DUMP_REG(HDMI_NV_PDISP_HDMI_AVI_INFOFRAME_HEADER);
	DUMP_REG(HDMI_NV_PDISP_HDMI_AVI_INFOFRAME_SUBPACK0_LOW);
	DUMP_REG(HDMI_NV_PDISP_HDMI_AVI_INFOFRAME_SUBPACK0_HIGH);
	DUMP_REG(HDMI_NV_PDISP_HDMI_AVI_INFOFRAME_SUBPACK1_LOW);
	DUMP_REG(HDMI_NV_PDISP_HDMI_AVI_INFOFRAME_SUBPACK1_HIGH);
	DUMP_REG(HDMI_NV_PDISP_HDMI_GENERIC_CTRL);
	DUMP_REG(HDMI_NV_PDISP_HDMI_GENERIC_STATUS);
	DUMP_REG(HDMI_NV_PDISP_HDMI_GENERIC_HEADER);
	DUMP_REG(HDMI_NV_PDISP_HDMI_GENERIC_SUBPACK0_LOW);
	DUMP_REG(HDMI_NV_PDISP_HDMI_GENERIC_SUBPACK0_HIGH);
	DUMP_REG(HDMI_NV_PDISP_HDMI_GENERIC_SUBPACK1_LOW);
	DUMP_REG(HDMI_NV_PDISP_HDMI_GENERIC_SUBPACK1_HIGH);
	DUMP_REG(HDMI_NV_PDISP_HDMI_GENERIC_SUBPACK2_LOW);
	DUMP_REG(HDMI_NV_PDISP_HDMI_GENERIC_SUBPACK2_HIGH);
	DUMP_REG(HDMI_NV_PDISP_HDMI_GENERIC_SUBPACK3_LOW);
	DUMP_REG(HDMI_NV_PDISP_HDMI_GENERIC_SUBPACK3_HIGH);
	DUMP_REG(HDMI_NV_PDISP_HDMI_ACR_CTRL);
	DUMP_REG(HDMI_NV_PDISP_HDMI_ACR_0320_SUBPACK_LOW);
	DUMP_REG(HDMI_NV_PDISP_HDMI_ACR_0320_SUBPACK_HIGH);
	DUMP_REG(HDMI_NV_PDISP_HDMI_ACR_0441_SUBPACK_LOW);
	DUMP_REG(HDMI_NV_PDISP_HDMI_ACR_0441_SUBPACK_HIGH);
	DUMP_REG(HDMI_NV_PDISP_HDMI_ACR_0882_SUBPACK_LOW);
	DUMP_REG(HDMI_NV_PDISP_HDMI_ACR_0882_SUBPACK_HIGH);
	DUMP_REG(HDMI_NV_PDISP_HDMI_ACR_1764_SUBPACK_LOW);
	DUMP_REG(HDMI_NV_PDISP_HDMI_ACR_1764_SUBPACK_HIGH);
	DUMP_REG(HDMI_NV_PDISP_HDMI_ACR_0480_SUBPACK_LOW);
	DUMP_REG(HDMI_NV_PDISP_HDMI_ACR_0480_SUBPACK_HIGH);
	DUMP_REG(HDMI_NV_PDISP_HDMI_ACR_0960_SUBPACK_LOW);
	DUMP_REG(HDMI_NV_PDISP_HDMI_ACR_0960_SUBPACK_HIGH);
	DUMP_REG(HDMI_NV_PDISP_HDMI_ACR_1920_SUBPACK_LOW);
	DUMP_REG(HDMI_NV_PDISP_HDMI_ACR_1920_SUBPACK_HIGH);
	DUMP_REG(HDMI_NV_PDISP_HDMI_CTRL);
	DUMP_REG(HDMI_NV_PDISP_HDMI_VSYNC_KEEPOUT);
	DUMP_REG(HDMI_NV_PDISP_HDMI_VSYNC_WINDOW);
	DUMP_REG(HDMI_NV_PDISP_HDMI_GCP_CTRL);
	DUMP_REG(HDMI_NV_PDISP_HDMI_GCP_STATUS);
	DUMP_REG(HDMI_NV_PDISP_HDMI_GCP_SUBPACK);
	DUMP_REG(HDMI_NV_PDISP_HDMI_CHANNEL_STATUS1);
	DUMP_REG(HDMI_NV_PDISP_HDMI_CHANNEL_STATUS2);
	DUMP_REG(HDMI_NV_PDISP_HDMI_EMU0);
	DUMP_REG(HDMI_NV_PDISP_HDMI_EMU1);
	DUMP_REG(HDMI_NV_PDISP_HDMI_EMU1_RDATA);
	DUMP_REG(HDMI_NV_PDISP_HDMI_SPARE);
	DUMP_REG(HDMI_NV_PDISP_HDMI_SPDIF_CHN_STATUS1);
	DUMP_REG(HDMI_NV_PDISP_HDMI_SPDIF_CHN_STATUS2);
	DUMP_REG(HDMI_NV_PDISP_HDCPRIF_ROM_CTRL);
	DUMP_REG(HDMI_NV_PDISP_SOR_CAP);
	DUMP_REG(HDMI_NV_PDISP_SOR_PWR);
	DUMP_REG(HDMI_NV_PDISP_SOR_TEST);
	DUMP_REG(HDMI_NV_PDISP_SOR_PLL0);
	DUMP_REG(HDMI_NV_PDISP_SOR_PLL1);
	DUMP_REG(HDMI_NV_PDISP_SOR_PLL2);
	DUMP_REG(HDMI_NV_PDISP_SOR_CSTM);
	DUMP_REG(HDMI_NV_PDISP_SOR_LVDS);
	DUMP_REG(HDMI_NV_PDISP_SOR_CRCA);
	DUMP_REG(HDMI_NV_PDISP_SOR_CRCB);
	DUMP_REG(HDMI_NV_PDISP_SOR_BLANK);
	DUMP_REG(HDMI_NV_PDISP_SOR_SEQ_CTL);
	DUMP_REG(HDMI_NV_PDISP_SOR_SEQ_INST0);
	DUMP_REG(HDMI_NV_PDISP_SOR_SEQ_INST1);
	DUMP_REG(HDMI_NV_PDISP_SOR_SEQ_INST2);
	DUMP_REG(HDMI_NV_PDISP_SOR_SEQ_INST3);
	DUMP_REG(HDMI_NV_PDISP_SOR_SEQ_INST4);
	DUMP_REG(HDMI_NV_PDISP_SOR_SEQ_INST5);
	DUMP_REG(HDMI_NV_PDISP_SOR_SEQ_INST6);
	DUMP_REG(HDMI_NV_PDISP_SOR_SEQ_INST7);
	DUMP_REG(HDMI_NV_PDISP_SOR_SEQ_INST8);
	DUMP_REG(HDMI_NV_PDISP_SOR_SEQ_INST9);
	DUMP_REG(HDMI_NV_PDISP_SOR_SEQ_INSTA);
	DUMP_REG(HDMI_NV_PDISP_SOR_SEQ_INSTB);
	DUMP_REG(HDMI_NV_PDISP_SOR_SEQ_INSTC);
	DUMP_REG(HDMI_NV_PDISP_SOR_SEQ_INSTD);
	DUMP_REG(HDMI_NV_PDISP_SOR_SEQ_INSTE);
	DUMP_REG(HDMI_NV_PDISP_SOR_SEQ_INSTF);
	DUMP_REG(HDMI_NV_PDISP_SOR_VCRCA0);
	DUMP_REG(HDMI_NV_PDISP_SOR_VCRCA1);
	DUMP_REG(HDMI_NV_PDISP_SOR_CCRCA0);
	DUMP_REG(HDMI_NV_PDISP_SOR_CCRCA1);
	DUMP_REG(HDMI_NV_PDISP_SOR_EDATAA0);
	DUMP_REG(HDMI_NV_PDISP_SOR_EDATAA1);
	DUMP_REG(HDMI_NV_PDISP_SOR_COUNTA0);
	DUMP_REG(HDMI_NV_PDISP_SOR_COUNTA1);
	DUMP_REG(HDMI_NV_PDISP_SOR_DEBUGA0);
	DUMP_REG(HDMI_NV_PDISP_SOR_DEBUGA1);
	DUMP_REG(HDMI_NV_PDISP_SOR_TRIG);
	DUMP_REG(HDMI_NV_PDISP_SOR_MSCHECK);
	DUMP_REG(HDMI_NV_PDISP_SOR_LANE_DRIVE_CURRENT);
	DUMP_REG(HDMI_NV_PDISP_AUDIO_DEBUG0);
	DUMP_REG(HDMI_NV_PDISP_AUDIO_DEBUG1);
	DUMP_REG(HDMI_NV_PDISP_AUDIO_DEBUG2);
	DUMP_REG(HDMI_NV_PDISP_AUDIO_FS(0));
	DUMP_REG(HDMI_NV_PDISP_AUDIO_FS(1));
	DUMP_REG(HDMI_NV_PDISP_AUDIO_FS(2));
	DUMP_REG(HDMI_NV_PDISP_AUDIO_FS(3));
	DUMP_REG(HDMI_NV_PDISP_AUDIO_FS(4));
	DUMP_REG(HDMI_NV_PDISP_AUDIO_FS(5));
	DUMP_REG(HDMI_NV_PDISP_AUDIO_FS(6));
	DUMP_REG(HDMI_NV_PDISP_AUDIO_PULSE_WIDTH);
	DUMP_REG(HDMI_NV_PDISP_AUDIO_THRESHOLD);
	DUMP_REG(HDMI_NV_PDISP_AUDIO_CNTRL0);
	DUMP_REG(HDMI_NV_PDISP_AUDIO_N);
	DUMP_REG(HDMI_NV_PDISP_HDCPRIF_ROM_TIMING);
	DUMP_REG(HDMI_NV_PDISP_SOR_REFCLK);
	DUMP_REG(HDMI_NV_PDISP_CRC_CONTROL);
	DUMP_REG(HDMI_NV_PDISP_INPUT_CONTROL);
	DUMP_REG(HDMI_NV_PDISP_SCRATCH);
	DUMP_REG(HDMI_NV_PDISP_PE_CURRENT);
	DUMP_REG(HDMI_NV_PDISP_KEY_CTRL);
	DUMP_REG(HDMI_NV_PDISP_KEY_DEBUG0);
	DUMP_REG(HDMI_NV_PDISP_KEY_DEBUG1);
	DUMP_REG(HDMI_NV_PDISP_KEY_DEBUG2);
	DUMP_REG(HDMI_NV_PDISP_KEY_HDCP_KEY_0);
	DUMP_REG(HDMI_NV_PDISP_KEY_HDCP_KEY_1);
	DUMP_REG(HDMI_NV_PDISP_KEY_HDCP_KEY_2);
	DUMP_REG(HDMI_NV_PDISP_KEY_HDCP_KEY_3);
	DUMP_REG(HDMI_NV_PDISP_KEY_HDCP_KEY_TRIG);
	DUMP_REG(HDMI_NV_PDISP_KEY_SKEY_INDEX);
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
	DUMP_REG(HDMI_NV_PDISP_SOR_IO_PEAK_CURRENT);
#endif
#undef DUMP_REG

	clk_disable_unprepare(hdmi->clk);
	tegra_dc_io_end(hdmi->dc);

	return 0;
}

static int dbg_hdmi_show_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_hdmi_show, inode->i_private);
}

static const struct file_operations dbg_hdmi_show_fops = {
	.open		= dbg_hdmi_show_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int dbg_hotplug_show(struct seq_file *m, void *unused)
{
	struct tegra_dc_hdmi_data *hdmi = m->private;
	struct tegra_dc *dc = hdmi->dc;

	if (WARN_ON(!hdmi || !dc || !dc->out))
		return -EINVAL;

	seq_put_decimal_ll(m, '\0', dc->out->hotplug_state);
	seq_putc(m, '\n');
	return 0;
}

static int dbg_hotplug_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_hotplug_show, inode->i_private);
}

static ssize_t dbg_hotplug_write(struct file *file, const char __user *addr,
	size_t len, loff_t *pos)
{
	struct seq_file *m = file->private_data; /* single_open() initialized */
	struct tegra_dc_hdmi_data *hdmi = m->private;
	struct tegra_dc *dc = hdmi->dc;
	int ret;
	long new_state;

	if (WARN_ON(!hdmi || !dc || !dc->out))
		return -EINVAL;

	ret = kstrtol_from_user(addr, len, 10, &new_state);
	if (ret < 0)
		return ret;

	if (dc->out->hotplug_state == 0 && new_state != 0) {
		/* was 0, now -1 or 1.
		 * we are overriding the hpd GPIO, so ignore the interrupt. */
		int gpio_irq = gpio_to_irq(dc->out->hotplug_gpio);

		disable_irq(gpio_irq);
	} else if (dc->out->hotplug_state != 0 && new_state == 0) {
		/* was -1 or 1, and now 0
		 * restore the interrupt for hpd GPIO. */
		int gpio_irq = gpio_to_irq(dc->out->hotplug_gpio);

		enable_irq(gpio_irq);
	}

	dc->out->hotplug_state = new_state;

	hdmi_state_machine_set_pending_hpd();

	return len;
}

static const struct file_operations dbg_hotplug_fops = {
	.open		= dbg_hotplug_open,
	.read		= seq_read,
	.write		= dbg_hotplug_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static struct dentry *hdmidir;

static void tegra_dc_hdmi_debug_create(struct tegra_dc_hdmi_data *hdmi)
{
	struct dentry *retval;

	hdmidir = debugfs_create_dir("tegra_hdmi", NULL);
	if (!hdmidir)
		return;
	retval = debugfs_create_file("regs", S_IRUGO, hdmidir, hdmi,
		&dbg_hdmi_show_fops);
	if (!retval)
		goto free_out;
	retval = debugfs_create_file("hotplug", S_IRUGO, hdmidir, hdmi,
		&dbg_hotplug_fops);
	if (!retval)
		goto free_out;
	return;
free_out:
	debugfs_remove_recursive(hdmidir);
	hdmidir = NULL;
	return;
}
#else
static inline void tegra_dc_hdmi_debug_create(struct tegra_dc_hdmi_data *hdmi)
{ }
#endif

#define PIXCLOCK_TOLERANCE	200

static int tegra_dc_calc_clock_per_frame(const struct fb_videomode *mode)
{
	return (mode->left_margin + mode->xres +
		mode->right_margin + mode->hsync_len) *
	       (mode->upper_margin + mode->yres +
		mode->lower_margin + mode->vsync_len);
}

static bool tegra_dc_hdmi_valid_pixclock(const struct tegra_dc *dc,
					const struct fb_videomode *mode)
{
	unsigned max_pixclock = tegra_dc_get_out_max_pixclock(dc);
	if (max_pixclock) {
		/* this might look counter-intuitive,
		 * but pixclock's unit is picos(not Khz)
		 */
		return mode->pixclock >= max_pixclock;
	} else {
		return true;
	}
}

static bool tegra_dc_check_constraint(const struct fb_videomode *mode)
{
	return mode->hsync_len > 1 && mode->vsync_len > 1 &&
		mode->lower_margin + mode->vsync_len + mode->upper_margin > 1 &&
		mode->xres >= 16 && mode->yres >= 16;
}

/* adjusts pixclock to fit audio table */
static bool tegra_dc_hdmi_adjust_pixclock(const struct tegra_dc *dc,
					struct fb_videomode *mode)
{
	const struct tegra_hdmi_audio_config *cfg = tegra_hdmi_audio_44_1k;
	unsigned pclk;

	if (!mode->pixclock)
		return false;

	pclk = PICOS2KHZ(mode->pixclock) * 1000;

	/* look on 44.1k audio table, if mode's pixel clock is within 1%, then
	 * use the pixel clock from the audio table.*/
	while (cfg->pix_clock) {
		if (cfg->pix_clock > (pclk / 100 * 99) &&
			cfg->pix_clock < (pclk / 100 * 101) &&
			cfg->pix_clock >= 1000) {
			mode->pixclock = KHZ2PICOS(cfg->pix_clock / 1000);
			return true;
		}
		cfg++;
	}
	return false;
}

bool tegra_dc_hdmi_mode_filter(const struct tegra_dc *dc,
					struct fb_videomode *mode)
{
	long total_clocks;

#ifndef CONFIG_ARCH_TEGRA_12x_SOC
	if (mode->vmode & FB_VMODE_INTERLACED)
		return false;
#endif

	/* Ignore modes with a 0 pixel clock */
	if (!mode->pixclock)
		return false;

#ifdef CONFIG_TEGRA_HDMI_74MHZ_LIMIT
		if (PICOS2KHZ(mode->pixclock) > 74250)
			return false;
#endif

#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
	/* Display B max is 4096 */
	if (mode->xres > 4096)
		return false;
#elif defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC)
	if (mode->xres > 2560)
		return false;
#else
	/* don't filter any modes due to width - probably not what you want */
#endif

	tegra_dc_hdmi_adjust_pixclock(dc, mode);

	/* Check if the mode's pixel clock is more than the max rate*/
	if (!tegra_dc_hdmi_valid_pixclock(dc, mode))
		return false;

	/* Work around for modes that fail the constrait:
	 * V_FRONT_PORCH >= V_REF_TO_SYNC + 1 */
	if (mode->lower_margin == 1) {
		mode->lower_margin++;
		mode->upper_margin--;
	}

	/* even after fix-ups the mode still isn't supported */
	if (!tegra_dc_check_constraint(mode))
		return false;

	mode->flag |= FB_MODE_IS_DETAILED;
	total_clocks = tegra_dc_calc_clock_per_frame(mode);
	mode->refresh = total_clocks ?
		(PICOS2KHZ(mode->pixclock) * 1000) / total_clocks : 0;

	return true;
}

/* used by tegra_dc_probe() to detect hpd/hdmi status at boot */
static bool tegra_dc_hdmi_detect(struct tegra_dc *dc)
{
	hdmi_state_machine_set_pending_hpd();
	/* result isn't used by dc */

	return true;
}

static irqreturn_t tegra_dc_hdmi_irq(int irq, void *ptr)
{
	struct tegra_dc *dc = ptr;
	struct tegra_dc_hdmi_data *hdmi = tegra_dc_get_outdata(dc);

	pr_info("%s: start\n", __func__);
	rt_mutex_lock(&hdmi->suspend_lock);
	if (!hdmi->suspended)
		hdmi_state_machine_set_pending_hpd();
	rt_mutex_unlock(&hdmi->suspend_lock);
	pr_info("%s: end\n", __func__);
	return IRQ_HANDLED;
}

static void tegra_dc_hdmi_suspend(struct tegra_dc *dc)
{
	struct tegra_dc_hdmi_data *hdmi = tegra_dc_get_outdata(dc);

	tegra_nvhdcp_suspend(hdmi->nvhdcp);

	if (dc->out->flags & TEGRA_DC_OUT_HOTPLUG_WAKE_LP0) {
		int wake_irq = gpio_to_irq(dc->out->hotplug_gpio);
		int ret;

		ret = enable_irq_wake(wake_irq);
		if (ret < 0) {
			dev_err(&dc->ndev->dev,
			"%s: Couldn't enable HDMI wakeup, irq=%d, error=%d\n",
			__func__, wake_irq, ret);
		}
	}

	rt_mutex_lock(&hdmi->suspend_lock);
	hdmi->suspended = true;
	rt_mutex_unlock(&hdmi->suspend_lock);

}

static void tegra_dc_hdmi_resume(struct tegra_dc *dc)
{
	struct tegra_dc_hdmi_data *hdmi = tegra_dc_get_outdata(dc);

	rt_mutex_lock(&hdmi->suspend_lock);
	hdmi->suspended = false;
	hdmi_state_machine_set_pending_hpd();
	rt_mutex_unlock(&hdmi->suspend_lock);

	if (dc->out->flags & TEGRA_DC_OUT_HOTPLUG_WAKE_LP0)
		disable_irq_wake(gpio_to_irq(dc->out->hotplug_gpio));

	tegra_nvhdcp_resume(hdmi->nvhdcp);
}

#ifdef CONFIG_SWITCH
static ssize_t underscan_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tegra_dc_hdmi_data *hdmi =
			container_of(dev_get_drvdata(dev), struct tegra_dc_hdmi_data, hpd_switch);

	if (hdmi->edid)
		return sprintf(buf, "%d\n", tegra_edid_underscan_supported(hdmi->edid));
	else
		return 0;
}

static DEVICE_ATTR(underscan, S_IRUGO, underscan_show, NULL);

static ssize_t hdmi_audio_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tegra_dc_hdmi_data *hdmi =
			container_of(dev_get_drvdata(dev),
				   struct tegra_dc_hdmi_data, audio_switch);

	if (hdmi->edid)
		return sprintf(buf, "%d\n",
				   tegra_edid_audio_supported(hdmi->edid));
	else
		return 0;
}

static DEVICE_ATTR(hdmi_audio, S_IRUGO, hdmi_audio_show, NULL);
#endif

static int tegra_dc_hdmi_init(struct tegra_dc *dc)
{
	struct tegra_dc_hdmi_data *hdmi;
	struct resource *res;
	struct resource hdmi_res;
	struct resource *base_res;
	int ret;
	void __iomem *base;
	struct clk *clk = NULL;
	struct clk *disp1_clk = NULL;
	struct clk *disp2_clk = NULL;
	struct tegra_hdmi_out *hdmi_out = NULL;
	int err;
	struct device_node *np = dc->ndev->dev.of_node;
#ifdef CONFIG_USE_OF
	struct device_node *np_hdmi =
		of_find_node_by_path("/host1x/hdmi");
#else
	struct device_node *np_hdmi = NULL;
#endif
	hdmi = kzalloc(sizeof(*hdmi), GFP_KERNEL);
	if (!hdmi)
		return -ENOMEM;
	if (np) {
		if (np_hdmi && of_device_is_available(np_hdmi)) {
			of_address_to_resource(np_hdmi, 0, &hdmi_res);
			res = &hdmi_res;
		} else {
			err = -EINVAL;
			goto err_free_hdmi;
		}
	} else {
		res = platform_get_resource_byname(dc->ndev,
			IORESOURCE_MEM, "hdmi_regs");
	}
	if (!res) {
		dev_err(&dc->ndev->dev, "hdmi: no mem resource\n");
		err = -ENOENT;
		goto err_free_hdmi;
	}

	base_res = request_mem_region(res->start,
		resource_size(res), dc->ndev->name);
	if (!base_res) {
		dev_err(&dc->ndev->dev, "hdmi: request_mem_region failed\n");
		err = -EBUSY;
		goto err_free_hdmi;
	}

	base = ioremap(res->start, resource_size(res));
	if (!base) {
		dev_err(&dc->ndev->dev, "hdmi: registers can't be mapped\n");
		err = -EBUSY;
		goto err_release_resource_reg;
	}

	clk = clk_get(&dc->ndev->dev, "hdmi");
	if (IS_ERR_OR_NULL(clk)) {
		dev_err(&dc->ndev->dev, "hdmi: can't get clock\n");
		err = -ENOENT;
		goto err_iounmap_reg;
	}

	disp1_clk = clk_get_sys("tegradc.0", NULL);
	if (IS_ERR_OR_NULL(disp1_clk)) {
		dev_err(&dc->ndev->dev, "hdmi: can't disp1 clock\n");
		err = -ENOENT;
		goto err_put_clock;
	}

	disp2_clk = clk_get_sys("tegradc.1", NULL);
	if (IS_ERR_OR_NULL(disp2_clk)) {
		dev_err(&dc->ndev->dev, "hdmi: can't disp2 clock\n");
		err = -ENOENT;
		goto err_put_clock;
	}

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
	hdmi->hda_clk = clk_get_sys("tegra30-hda", "hda");
	if (IS_ERR_OR_NULL(hdmi->hda_clk)) {
		dev_err(&dc->ndev->dev, "hdmi: can't get hda clock\n");
		err = -ENOENT;
		goto err_put_clock;
	}

	hdmi->hda2codec_clk = clk_get_sys("tegra30-hda", "hda2codec");
	if (IS_ERR_OR_NULL(hdmi->hda2codec_clk)) {
		dev_err(&dc->ndev->dev, "hdmi: can't get hda2codec clock\n");
		err = -ENOENT;
		goto err_put_clock;
	}

	hdmi->hda2hdmi_clk = clk_get_sys("tegra30-hda", "hda2hdmi");
	if (IS_ERR_OR_NULL(hdmi->hda2hdmi_clk)) {
		dev_err(&dc->ndev->dev, "hdmi: can't get hda2hdmi clock\n");
		err = -ENOENT;
		goto err_put_clock;
	}
#endif

	/* Get the pointer of board file settings */
	hdmi_out = dc->pdata->default_out->hdmi_out;
	if (hdmi_out)
		memcpy(&hdmi->info, hdmi_out, sizeof(hdmi->info));

	hdmi->edid = tegra_edid_create(dc->out->dcc_bus);
	if (IS_ERR_OR_NULL(hdmi->edid)) {
		dev_err(&dc->ndev->dev, "hdmi: can't create edid\n");
		err = PTR_ERR(hdmi->edid);
		goto err_put_clock;
	}

#ifdef CONFIG_TEGRA_NVHDCP
	hdmi->nvhdcp = tegra_nvhdcp_create(hdmi, dc->ndev->id,
			dc->out->dcc_bus);
	if (IS_ERR_OR_NULL(hdmi->nvhdcp)) {
		dev_err(&dc->ndev->dev, "hdmi: can't create nvhdcp\n");
		err = PTR_ERR(hdmi->nvhdcp);
		goto err_edid_destroy;
	}
#else
	hdmi->nvhdcp = NULL;
#endif
	hdmi_state_machine_init(hdmi);

	hdmi->dc = dc;
	hdmi->base = base;
	hdmi->base_res = base_res;
	hdmi->clk = clk;
	hdmi->disp1_clk = disp1_clk;
	hdmi->disp2_clk = disp2_clk;
	hdmi->suspended = false;
	hdmi->eld_retrieved= false;
	hdmi->clk_enabled = false;
	hdmi->audio_freq = 44100;
	hdmi->audio_source = AUTO;
	rt_mutex_init(&hdmi->suspend_lock);

#ifdef CONFIG_SWITCH
	hdmi->hpd_switch.name = "hdmi";
	ret = switch_dev_register(&hdmi->hpd_switch);

	if (!ret)
		ret = device_create_file(hdmi->hpd_switch.dev,
			&dev_attr_underscan);
	BUG_ON(ret != 0);

	hdmi->audio_switch.name = "hdmi_audio";
	ret = switch_dev_register(&hdmi->audio_switch);

	if (!ret)
		ret = device_create_file(hdmi->audio_switch.dev,
			&dev_attr_hdmi_audio);
	BUG_ON(ret != 0);
#endif

	dc->out->depth = 24;

	tegra_dc_set_outdata(dc, hdmi);

	dc_hdmi = hdmi;
	/* boards can select default content protection policy */
	if (dc->out->flags & TEGRA_DC_OUT_NVHDCP_POLICY_ON_DEMAND)
		tegra_nvhdcp_set_policy(hdmi->nvhdcp,
			TEGRA_NVHDCP_POLICY_ON_DEMAND);
	else
		tegra_nvhdcp_set_policy(hdmi->nvhdcp,
			TEGRA_NVHDCP_POLICY_ALWAYS_ON);

	tegra_dc_hdmi_debug_create(hdmi);

	err = gpio_request(dc->out->hotplug_gpio, "hdmi_hpd");
	if (err < 0) {
		dev_err(&dc->ndev->dev, "hdmi: hpd gpio_request failed\n");
		goto err_nvhdcp_destroy;
	}

	gpio_direction_input(dc->out->hotplug_gpio);

	/* TODO: support non-hotplug */
	ret = request_threaded_irq(gpio_to_irq(dc->out->hotplug_gpio),
		NULL, tegra_dc_hdmi_irq,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		dev_name(&dc->ndev->dev), dc);

	if (ret) {
		dev_err(&dc->ndev->dev, "hdmi: request_irq %d failed - %d\n",
			gpio_to_irq(dc->out->hotplug_gpio), ret);
		err = -EBUSY;
		goto err_gpio_free;
	}

	return 0;

err_gpio_free:
	gpio_free(dc->out->hotplug_gpio);
err_nvhdcp_destroy:
	if (hdmi->nvhdcp)
		tegra_nvhdcp_destroy(hdmi->nvhdcp);
#ifdef CONFIG_TEGRA_NVHDCP
err_edid_destroy:
#endif
	tegra_edid_destroy(hdmi->edid);
err_put_clock:
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
	if (!IS_ERR_OR_NULL(hdmi->hda2hdmi_clk))
		clk_put(hdmi->hda2hdmi_clk);
	if (!IS_ERR_OR_NULL(hdmi->hda2codec_clk))
		clk_put(hdmi->hda2codec_clk);
	if (!IS_ERR_OR_NULL(hdmi->hda_clk))
		clk_put(hdmi->hda_clk);
#endif
	if (!IS_ERR_OR_NULL(disp2_clk))
		clk_put(disp2_clk);
	if (!IS_ERR_OR_NULL(disp1_clk))
		clk_put(disp1_clk);
	if (!IS_ERR_OR_NULL(clk))
		clk_put(clk);
err_iounmap_reg:
	iounmap(base);
err_release_resource_reg:
	release_resource(base_res);
err_free_hdmi:
	kfree(hdmi);
	return err;
}

static void tegra_dc_hdmi_destroy(struct tegra_dc *dc)
{
	struct tegra_dc_hdmi_data *hdmi = tegra_dc_get_outdata(dc);

	free_irq(gpio_to_irq(dc->out->hotplug_gpio), dc);
	hdmi_state_machine_shutdown();
#ifdef CONFIG_SWITCH
	switch_dev_unregister(&hdmi->hpd_switch);
	switch_dev_unregister(&hdmi->audio_switch);
#endif
	iounmap(hdmi->base);
	release_resource(hdmi->base_res);
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
	clk_put(hdmi->hda2hdmi_clk);
	clk_put(hdmi->hda2codec_clk);
	clk_put(hdmi->hda_clk);
#endif
	clk_put(hdmi->clk);
	clk_put(hdmi->disp1_clk);
	clk_put(hdmi->disp2_clk);
	tegra_edid_destroy(hdmi->edid);
	tegra_nvhdcp_destroy(hdmi->nvhdcp);

	kfree(hdmi);

}

static void tegra_dc_hdmi_setup_audio_fs_tables(struct tegra_dc *dc)
{
	struct tegra_dc_hdmi_data *hdmi = tegra_dc_get_outdata(dc);
	int i;
	unsigned freqs[] = {
		32000,
		44100,
		48000,
		88200,
		96000,
		176400,
		192000,
        };

	for (i = 0; i < ARRAY_SIZE(freqs); i++) {
		unsigned f = freqs[i];
		unsigned eight_half;
		unsigned delta;;

		if (f > 96000)
			delta = 2;
		else if (f > 48000)
			delta = 6;
		else
			delta = 9;

		if (!f)
			eight_half = 0;
		else
			eight_half = (8 * HDMI_AUDIOCLK_FREQ) / (f * 128);
		tegra_hdmi_writel(hdmi, AUDIO_FS_LOW(eight_half - delta) |
				  AUDIO_FS_HIGH(eight_half + delta),
				  HDMI_NV_PDISP_AUDIO_FS(i));
	}
}

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
static void tegra_dc_hdmi_setup_eld_buff(struct tegra_dc *dc)
{
	int i;
	int j;
	u8 tmp;

	struct tegra_dc_hdmi_data *hdmi = tegra_dc_get_outdata(dc);

	/* program ELD stuff */
	for (i = 0; i < HDMI_ELD_MONITOR_NAME_INDEX; i++) {
		switch (i) {
		case HDMI_ELD_VER_INDEX:
			tmp = (hdmi->eld.eld_ver << 3);
			tegra_hdmi_writel(hdmi, (i << 8) | tmp,
				  HDMI_NV_PDISP_SOR_AUDIO_HDA_ELD_BUFWR_0);
			break;
		case HDMI_ELD_BASELINE_LEN_INDEX:
			break;
		case HDMI_ELD_CEA_VER_MNL_INDEX:
			tmp = (hdmi->eld.cea_edid_ver << 5);
			tmp |= (hdmi->eld.mnl & 0x1f);
			tegra_hdmi_writel(hdmi, (i << 8) | tmp,
					  HDMI_NV_PDISP_SOR_AUDIO_HDA_ELD_BUFWR_0);
			break;
		case HDMI_ELD_SAD_CNT_CON_TYP_SAI_HDCP_INDEX:
			tmp = (hdmi->eld.sad_count << 4);
			tmp |= (hdmi->eld.conn_type & 0xC);
			tmp |= (hdmi->eld.support_ai & 0x2);
			tmp |= (hdmi->eld.support_hdcp & 0x1);
			tegra_hdmi_writel(hdmi, (i << 8) | tmp,
					  HDMI_NV_PDISP_SOR_AUDIO_HDA_ELD_BUFWR_0);
			break;
		case HDMI_ELD_AUD_SYNC_DELAY_INDEX:
			tegra_hdmi_writel(hdmi, (i << 8) | (hdmi->eld.aud_synch_delay),
					  HDMI_NV_PDISP_SOR_AUDIO_HDA_ELD_BUFWR_0);
			break;
		case HDMI_ELD_SPK_ALLOC_INDEX:
			tegra_hdmi_writel(hdmi, (i << 8) | (hdmi->eld.spk_alloc),
					  HDMI_NV_PDISP_SOR_AUDIO_HDA_ELD_BUFWR_0);
			break;
		case HDMI_ELD_PORT_ID_INDEX:
			for (j = 0; j < 8;j++) {
				tegra_hdmi_writel(hdmi, ((i +j) << 8) | (hdmi->eld.port_id[j]),
					  HDMI_NV_PDISP_SOR_AUDIO_HDA_ELD_BUFWR_0);
			}
			break;
		case HDMI_ELD_MANF_NAME_INDEX:
			for (j = 0; j < 2;j++) {
				tegra_hdmi_writel(hdmi, ((i +j) << 8) | (hdmi->eld.manufacture_id[j]),
					  HDMI_NV_PDISP_SOR_AUDIO_HDA_ELD_BUFWR_0);
			}
			break;
		case HDMI_ELD_PRODUCT_CODE_INDEX:
			for (j = 0; j < 2;j++) {
				tegra_hdmi_writel(hdmi, ((i +j) << 8) | (hdmi->eld.product_id[j]),
					  HDMI_NV_PDISP_SOR_AUDIO_HDA_ELD_BUFWR_0);
			}
			break;
		}
	}
	for (j = 0; j < hdmi->eld.mnl;j++) {
		tegra_hdmi_writel(hdmi, ((j + HDMI_ELD_MONITOR_NAME_INDEX) << 8) |
				  (hdmi->eld.monitor_name[j]),
				  HDMI_NV_PDISP_SOR_AUDIO_HDA_ELD_BUFWR_0);
	}
	for (j = 0; j < hdmi->eld.sad_count;j++) {
		tegra_hdmi_writel(hdmi, ((j + HDMI_ELD_MONITOR_NAME_INDEX + hdmi->eld.mnl) << 8) |
				  (hdmi->eld.sad[j]),
				  HDMI_NV_PDISP_SOR_AUDIO_HDA_ELD_BUFWR_0);
	}
		/* set presence andvalid bit  */
	tegra_hdmi_writel(hdmi, 3, HDMI_NV_PDISP_SOR_AUDIO_HDA_PRESENSE_0);
}
#endif

static int tegra_dc_hdmi_setup_audio(struct tegra_dc *dc, unsigned audio_freq,
					unsigned audio_source)
{
	struct tegra_dc_hdmi_data *hdmi = tegra_dc_get_outdata(dc);
	const struct tegra_hdmi_audio_config *config;
	unsigned long audio_n;
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
	unsigned long reg_addr = 0;
#endif
	unsigned a_source = AUDIO_CNTRL0_SOURCE_SELECT_AUTO;

	if (HDA == audio_source)
		a_source = AUDIO_CNTRL0_SOURCE_SELECT_HDAL;
	else if (SPDIF == audio_source)
		a_source = AUDIO_CNTRL0_SOURCE_SELECT_SPDIF;

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
	if (hdmi->audio_inject_null)
		a_source |= AUDIO_CNTRL0_INJECT_NULLSMPL;

	tegra_hdmi_writel(hdmi,a_source,
			  HDMI_NV_PDISP_SOR_AUDIO_CNTRL0_0);
	tegra_hdmi_writel(hdmi,
			  AUDIO_CNTRL0_ERROR_TOLERANCE(6) |
			  AUDIO_CNTRL0_FRAMES_PER_BLOCK(0xc0),
			  HDMI_NV_PDISP_AUDIO_CNTRL0);
#if !defined(CONFIG_ARCH_TEGRA_3x_SOC)
	tegra_hdmi_writel(hdmi, (1 << HDMI_AUDIO_HBR_ENABLE_SHIFT) |
	   tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_SOR_AUDIO_SPARE0_0),
	   HDMI_NV_PDISP_SOR_AUDIO_SPARE0_0);
#endif
#else
	tegra_hdmi_writel(hdmi,
			  AUDIO_CNTRL0_ERROR_TOLERANCE(6) |
			  AUDIO_CNTRL0_FRAMES_PER_BLOCK(0xc0) |
			  a_source,
			  HDMI_NV_PDISP_AUDIO_CNTRL0);
#endif
	config = tegra_hdmi_get_audio_config(audio_freq, dc->mode.pclk);
	if (!config) {
		dev_err(&dc->ndev->dev,
			"hdmi: can't set audio to %d at %d pix_clock",
			audio_freq, dc->mode.pclk);
		return -EINVAL;
	}

	tegra_hdmi_writel(hdmi, 0, HDMI_NV_PDISP_HDMI_ACR_CTRL);

	audio_n = AUDIO_N_RESETF | AUDIO_N_GENERATE_ALTERNALTE |
		AUDIO_N_VALUE(config->n - 1);
	tegra_hdmi_writel(hdmi, audio_n, HDMI_NV_PDISP_AUDIO_N);

	tegra_hdmi_writel(hdmi, ACR_SUBPACK_N(config->n) | ACR_ENABLE,
			  HDMI_NV_PDISP_HDMI_ACR_0441_SUBPACK_HIGH);

	tegra_hdmi_writel(hdmi, ACR_SUBPACK_CTS(config->cts),
			  HDMI_NV_PDISP_HDMI_ACR_0441_SUBPACK_LOW);

	tegra_hdmi_writel(hdmi, SPARE_HW_CTS | SPARE_FORCE_SW_CTS |
			  SPARE_CTS_RESET_VAL(1),
			  HDMI_NV_PDISP_HDMI_SPARE);

	audio_n &= ~AUDIO_N_RESETF;
	tegra_hdmi_writel(hdmi, audio_n, HDMI_NV_PDISP_AUDIO_N);

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
	switch (audio_freq) {
	case AUDIO_FREQ_32K:
		reg_addr = HDMI_NV_PDISP_SOR_AUDIO_AVAL_0320_0;
		break;
	case AUDIO_FREQ_44_1K:
		reg_addr = HDMI_NV_PDISP_SOR_AUDIO_AVAL_0441_0;
		break;
	case AUDIO_FREQ_48K:
		reg_addr = HDMI_NV_PDISP_SOR_AUDIO_AVAL_0480_0;
		break;
	case AUDIO_FREQ_88_2K:
		reg_addr = HDMI_NV_PDISP_SOR_AUDIO_AVAL_0882_0;
		break;
	case AUDIO_FREQ_96K:
		reg_addr = HDMI_NV_PDISP_SOR_AUDIO_AVAL_0960_0;
		break;
	case AUDIO_FREQ_176_4K:
		reg_addr = HDMI_NV_PDISP_SOR_AUDIO_AVAL_1764_0;
		break;
	case AUDIO_FREQ_192K:
		reg_addr = HDMI_NV_PDISP_SOR_AUDIO_AVAL_1920_0;
		break;
	}

	tegra_hdmi_writel(hdmi, config->aval, reg_addr);
#endif
	tegra_dc_hdmi_setup_audio_fs_tables(dc);

	return 0;
}

int tegra_hdmi_setup_audio_freq_source(unsigned audio_freq, unsigned audio_source)
{
	struct tegra_dc_hdmi_data *hdmi = dc_hdmi;

	if (!hdmi)
		return -EAGAIN;

	/* check for know freq */
	if (AUDIO_FREQ_32K == audio_freq ||
		AUDIO_FREQ_44_1K== audio_freq ||
		AUDIO_FREQ_48K== audio_freq ||
		AUDIO_FREQ_88_2K== audio_freq ||
		AUDIO_FREQ_96K== audio_freq ||
		AUDIO_FREQ_176_4K== audio_freq ||
		AUDIO_FREQ_192K== audio_freq) {
		/* If we can program HDMI, then proceed */
		if (hdmi->clk_enabled)
			tegra_dc_hdmi_setup_audio(hdmi->dc, audio_freq,audio_source);

		/* Store it for using it in enable */
		hdmi->audio_freq = audio_freq;
		hdmi->audio_source = audio_source;
	}
	else
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL(tegra_hdmi_setup_audio_freq_source);

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
int tegra_hdmi_audio_null_sample_inject(bool on)
{
	struct tegra_dc_hdmi_data *hdmi = dc_hdmi;
	unsigned int val = 0;

	if (!hdmi)
		return -EAGAIN;

	if (hdmi->audio_inject_null != on) {
		hdmi->audio_inject_null = on;
		if (hdmi->clk_enabled) {
			val = tegra_hdmi_readl(hdmi,
				HDMI_NV_PDISP_SOR_AUDIO_CNTRL0_0);
			val &= ~AUDIO_CNTRL0_INJECT_NULLSMPL;
			if (on)
				val |= AUDIO_CNTRL0_INJECT_NULLSMPL;
			tegra_hdmi_writel(hdmi,val,
				HDMI_NV_PDISP_SOR_AUDIO_CNTRL0_0);
		}
	}

	return 0;
}
EXPORT_SYMBOL(tegra_hdmi_audio_null_sample_inject);

int tegra_hdmi_setup_hda_presence()
{
	struct tegra_dc_hdmi_data *hdmi = dc_hdmi;

	if (!hdmi)
		return -EAGAIN;

	if (hdmi->clk_enabled && hdmi->eld_retrieved) {
		/* If HDA_PRESENCE is already set reset it */
		tegra_dc_unpowergate_locked(hdmi->dc);
		if (tegra_hdmi_readl(hdmi,
				     HDMI_NV_PDISP_SOR_AUDIO_HDA_PRESENSE_0))
			tegra_hdmi_writel(hdmi, 0,
				     HDMI_NV_PDISP_SOR_AUDIO_HDA_PRESENSE_0);

		tegra_dc_hdmi_setup_eld_buff(hdmi->dc);
		tegra_dc_powergate_locked(hdmi->dc);
		return 0;
	}
	return -ENODEV;

}
EXPORT_SYMBOL(tegra_hdmi_setup_hda_presence);
#endif

static void tegra_dc_hdmi_write_infopack(struct tegra_dc *dc, int header_reg,
					 u8 type, u8 version, void *data, int len)
{
	struct tegra_dc_hdmi_data *hdmi = tegra_dc_get_outdata(dc);
	u32 subpack[2];  /* extra byte for zero padding of subpack */
	int i;
	u8 csum;

	/* first byte of data is the checksum */
	csum = type + version + len - 1;
	for (i = 1; i < len; i++)
		csum +=((u8 *)data)[i];
	((u8 *)data)[0] = 0x100 - csum;

	tegra_hdmi_writel(hdmi, INFOFRAME_HEADER_TYPE(type) |
			  INFOFRAME_HEADER_VERSION(version) |
			  INFOFRAME_HEADER_LEN(len - 1),
			  header_reg);

	/* The audio inforame only has one set of subpack registers.  The hdmi
	 * block pads the rest of the data as per the spec so we have to fixup
	 * the length before filling in the subpacks.
	 */
	if (header_reg == HDMI_NV_PDISP_HDMI_AUDIO_INFOFRAME_HEADER)
		len = 6;

	/* each subpack 7 bytes devided into:
	 *   subpack_low - bytes 0 - 3
	 *   subpack_high - bytes 4 - 6 (with byte 7 padded to 0x00)
	 */
	for (i = 0; i < len; i++) {
		int subpack_idx = i % 7;

		if (subpack_idx == 0)
			memset(subpack, 0x0, sizeof(subpack));

		((u8 *)subpack)[subpack_idx] = ((u8 *)data)[i];

		if (subpack_idx == 6 || (i + 1 == len)) {
			int reg = header_reg + 1 + (i / 7) * 2;

			tegra_hdmi_writel(hdmi, subpack[0], reg);
			tegra_hdmi_writel(hdmi, subpack[1], reg + 1);
		}
	}
}

static int tegra_dc_find_cea_vic(const struct tegra_dc_mode *mode)
{
	struct fb_videomode m;
	unsigned i;
	unsigned best = 0;

	tegra_dc_to_fb_videomode(&m, mode);

	m.vmode &= ~FB_VMODE_STEREO_MASK; /* stereo modes have the same VICs */

	for (i = 1; i < CEA_MODEDB_SIZE; i++) {
		const struct fb_videomode *curr = &cea_modes[i];

		if (!fb_mode_is_equal(&m, curr))
			continue;

		if (!best)
			best = i;
		/* if either flag is set, then match is required */
		if (curr->flag & (FB_FLAG_RATIO_4_3 | FB_FLAG_RATIO_16_9)) {
			if (m.flag & curr->flag & FB_FLAG_RATIO_4_3)
				best = i;
			else if (m.flag & curr->flag & FB_FLAG_RATIO_16_9)
				best = i;
		} else {
			best = i;
		}
	}
	return best;
}

static int tegra_dc_find_hdmi_vic(const struct tegra_dc_mode *mode)
{
	struct fb_videomode m;
	unsigned i;

	tegra_dc_to_fb_videomode(&m, mode);

	for (i = 1; i < HDMI_EXT_MODEDB_SIZE; i++) {
		const struct fb_videomode *curr = &hdmi_ext_modes[i];

		if (fb_mode_is_equal(&m, curr))
			return i;
	}
	return 0;
}

static void tegra_dc_hdmi_disable_generic_infoframe(struct tegra_dc *dc)
{
	struct tegra_dc_hdmi_data *hdmi = tegra_dc_get_outdata(dc);
	u32 val;

	val  = tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_HDMI_GENERIC_CTRL);
	val &= ~GENERIC_CTRL_ENABLE;
	tegra_hdmi_writel(hdmi, val, HDMI_NV_PDISP_HDMI_GENERIC_CTRL);
}

/* return 1 if generic infoframe is used, 0 if not used */
static int tegra_dc_hdmi_setup_hdmi_vic_infoframe(struct tegra_dc *dc, bool dvi)
{
	struct tegra_dc_hdmi_data *hdmi = tegra_dc_get_outdata(dc);
	struct hdmi_extres_infoframe extres;
	int hdmi_vic;
	u32 val;

	if (dvi)
		return 0;
	hdmi_vic = tegra_dc_find_hdmi_vic(&dc->mode);
	if (hdmi_vic <= 0)
		return 0;

	memset(&extres, 0x0, sizeof(extres));

	extres.csum = 0;
	extres.regid0 = 0x03;
	extres.regid1 = 0x0c;
	extres.regid2 = 0x00;
	extres.hdmi_video_format = 1; /* Extended Resolution Format */
	extres.hdmi_vic = hdmi_vic;

	tegra_dc_hdmi_write_infopack(dc,
		HDMI_NV_PDISP_HDMI_GENERIC_HEADER,
		HDMI_INFOFRAME_TYPE_VENDOR, HDMI_VENDOR_VERSION,
		&extres, 6);
	val = tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_HDMI_GENERIC_CTRL);
	val |= GENERIC_CTRL_ENABLE;
	tegra_hdmi_writel(hdmi, val, HDMI_NV_PDISP_HDMI_GENERIC_CTRL);
	return 1;
}

static void tegra_dc_hdmi_setup_avi_infoframe(struct tegra_dc *dc, bool dvi)
{
	struct tegra_dc_hdmi_data *hdmi = tegra_dc_get_outdata(dc);
	struct hdmi_avi_infoframe avi;
	unsigned int blender_reg;

	if (dvi) {
		tegra_hdmi_writel(hdmi, 0x0,
				  HDMI_NV_PDISP_HDMI_AVI_INFOFRAME_CTRL);
		return;
	}

	memset(&avi, 0x0, sizeof(avi));

	/* Indicate active format info is valid. */
	avi.a = 1;
	avi.r = HDMI_AVI_R_SAME;

#if !defined(CONFIG_TEGRA_DC_BLENDER_GEN2)
	blender_reg = DC_DISP_BORDER_COLOR;
#else
	blender_reg = DC_DISP_BLEND_BACKGROUND_COLOR;
#endif

	if ((dc->mode.h_active == 720) && ((dc->mode.v_active == 480) || (dc->mode.v_active == 576)))
		tegra_dc_writel(dc, 0x00101010, blender_reg);
	else
		tegra_dc_writel(dc, 0x00000000, blender_reg);

	avi.vic = tegra_dc_find_cea_vic(&dc->mode);
	avi.m = dc->mode.avi_m;
	if (tegra_edid_underscan_supported(hdmi->edid))
		avi.s = HDMI_AVI_S_UNDERSCAN;
	dev_dbg(&dc->ndev->dev, "HDMI AVI vic=%d m=%d\n", avi.vic, avi.m);

	tegra_dc_hdmi_write_infopack(dc, HDMI_NV_PDISP_HDMI_AVI_INFOFRAME_HEADER,
				     HDMI_INFOFRAME_TYPE_AVI,
				     HDMI_AVI_VERSION,
				     &avi, sizeof(avi));

	tegra_hdmi_writel(hdmi, INFOFRAME_CTRL_ENABLE,
			  HDMI_NV_PDISP_HDMI_AVI_INFOFRAME_CTRL);
}

static void tegra_dc_hdmi_setup_stereo_infoframe(struct tegra_dc *dc)
{
	struct tegra_dc_hdmi_data *hdmi = tegra_dc_get_outdata(dc);
	struct hdmi_stereo_infoframe stereo;
	u32 val;

	WARN(!dc->mode.stereo_mode,
		"function assumes 3D/stereo mode is disabled\n");

	memset(&stereo, 0x0, sizeof(stereo));

	stereo.regid0 = 0x03;
	stereo.regid1 = 0x0c;
	stereo.regid2 = 0x00;
	stereo.hdmi_video_format = 2; /* 3D_Structure present */
#ifndef CONFIG_TEGRA_HDMI_74MHZ_LIMIT
	stereo._3d_structure = 0; /* frame packing */
#else
	stereo._3d_structure = 8; /* side-by-side (half) */
	stereo._3d_ext_data = 0; /* something which fits into 00XX bit req */
#endif

	tegra_dc_hdmi_write_infopack(dc, HDMI_NV_PDISP_HDMI_GENERIC_HEADER,
					HDMI_INFOFRAME_TYPE_VENDOR,
					HDMI_VENDOR_VERSION,
					&stereo, 6);

	val  = tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_HDMI_GENERIC_CTRL);
	val |= GENERIC_CTRL_ENABLE;

	tegra_hdmi_writel(hdmi, val, HDMI_NV_PDISP_HDMI_GENERIC_CTRL);
}

static void tegra_dc_hdmi_setup_audio_infoframe(struct tegra_dc *dc, bool dvi)
{
	struct tegra_dc_hdmi_data *hdmi = tegra_dc_get_outdata(dc);
	struct hdmi_audio_infoframe audio;

	if (dvi) {
		tegra_hdmi_writel(hdmi, 0x0,
				  HDMI_NV_PDISP_HDMI_AUDIO_INFOFRAME_CTRL);
		return;
	}

	memset(&audio, 0x0, sizeof(audio));

	audio.cc = HDMI_AUDIO_CC_2;
	tegra_dc_hdmi_write_infopack(dc, HDMI_NV_PDISP_HDMI_AUDIO_INFOFRAME_HEADER,
				     HDMI_INFOFRAME_TYPE_AUDIO,
				     HDMI_AUDIO_VERSION,
				     &audio, sizeof(audio));

	tegra_hdmi_writel(hdmi, INFOFRAME_CTRL_ENABLE,
			  HDMI_NV_PDISP_HDMI_AUDIO_INFOFRAME_CTRL);
}

static void tegra_dc_hdmi_setup_tmds(struct tegra_dc_hdmi_data *hdmi,
		const struct tmds_config *tc)
{
#if defined(CONFIG_ARCH_TEGRA_11x_SOC) || defined(CONFIG_ARCH_TEGRA_12x_SOC)
	u32 val;
#endif

	tegra_hdmi_writel(hdmi, tc->pll0, HDMI_NV_PDISP_SOR_PLL0);
	tegra_hdmi_writel(hdmi, tc->pll1, HDMI_NV_PDISP_SOR_PLL1);

	tegra_hdmi_writel(hdmi, tc->pe_current, HDMI_NV_PDISP_PE_CURRENT);

#if defined(CONFIG_ARCH_TEGRA_11x_SOC) || defined(CONFIG_ARCH_TEGRA_12x_SOC)
	tegra_hdmi_writel(hdmi, tc->drive_current,
		HDMI_NV_PDISP_SOR_LANE_DRIVE_CURRENT);
	val = tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_SOR_PAD_CTLS0);
	val |= DRIVE_CURRENT_FUSE_OVERRIDE_T11x;
	if (MAJOR(tc->version) >= 1) {
		val &= tc->pad_ctls0_mask;
		val |= tc->pad_ctls0_setting;
	}
	tegra_hdmi_writel(hdmi, val, HDMI_NV_PDISP_SOR_PAD_CTLS0);

	tegra_hdmi_writel(hdmi, tc->peak_current,
		HDMI_NV_PDISP_SOR_IO_PEAK_CURRENT);
#elif defined(CONFIG_ARCH_TEGRA_14x_SOC)
	tegra_hdmi_writel(hdmi, tc->drive_current,
		HDMI_NV_PDISP_SOR_LANE_DRIVE_CURRENT);
	tegra_hdmi_writel(hdmi, 0x800034bb, HDMI_NV_PDISP_SOR_PAD_CTLS0);

	tegra_hdmi_writel(hdmi, tc->peak_current,
		HDMI_NV_PDISP_SOR_IO_PEAK_CURRENT);
#else
	tegra_hdmi_writel(hdmi, tc->drive_current | DRIVE_CURRENT_FUSE_OVERRIDE,
		HDMI_NV_PDISP_SOR_LANE_DRIVE_CURRENT);
#endif
}

static void tegra_dc_hdmi_enable(struct tegra_dc *dc)
{
	struct tegra_dc_hdmi_data *hdmi = tegra_dc_get_outdata(dc);
	int pulse_start;
	int dispclk_div_8_2;
	int retries;
	int rekey;
	int err;
	unsigned long val;
	unsigned i;
	const struct tmds_config *tmds_ptr;
	size_t tmds_len;

	/* enable power, clocks, resets, etc. */

	/* The upstream DC needs to be clocked for accesses to HDMI to not
	 * hard lock the system.  Because we don't know if HDMI is conencted
	 * to disp1 or disp2 we need to enable both until we set the DC mux.
	 */
	clk_prepare_enable(hdmi->disp1_clk);
	clk_prepare_enable(hdmi->disp2_clk);

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
	/* Enabling HDA clocks before asserting HDA PD and ELDV bits */
	clk_prepare_enable(hdmi->hda_clk);
	clk_prepare_enable(hdmi->hda2codec_clk);
	clk_prepare_enable(hdmi->hda2hdmi_clk);
#endif

	tegra_dc_setup_clk(dc, hdmi->clk);
	clk_set_rate(hdmi->clk, dc->mode.pclk);

	clk_prepare_enable(hdmi->clk);
	tegra_periph_reset_assert(hdmi->clk);
	mdelay(1);
	tegra_periph_reset_deassert(hdmi->clk);

	/* TODO: copy HDCP keys from KFUSE to HDMI */

	/* Program display timing registers: handled by dc */

	/* program HDMI registers and SOR sequencer */

	tegra_dc_io_start(dc);
	tegra_dc_writel(dc, VSYNC_H_POSITION(1), DC_DISP_DISP_TIMING_OPTIONS);

	dc->out->depth = 24;
	dc->out->dither = TEGRA_DC_DISABLE_DITHER;
	tegra_dc_set_color_control(dc);

	/* video_preamble uses h_pulse2 */
	pulse_start = dc->mode.h_ref_to_sync + dc->mode.h_sync_width +
		dc->mode.h_back_porch - 10;
	tegra_dc_writel(dc, H_PULSE_2_ENABLE, DC_DISP_DISP_SIGNAL_OPTIONS0);
	tegra_dc_writel(dc,
			PULSE_MODE_NORMAL |
			PULSE_POLARITY_HIGH |
			PULSE_QUAL_VACTIVE |
			PULSE_LAST_END_A,
			DC_DISP_H_PULSE2_CONTROL);
	tegra_dc_writel(dc, PULSE_START(pulse_start) | PULSE_END(pulse_start + 8),
		  DC_DISP_H_PULSE2_POSITION_A);

	tegra_hdmi_writel(hdmi,
			  VSYNC_WINDOW_END(0x210) |
			  VSYNC_WINDOW_START(0x200) |
			  VSYNC_WINDOW_ENABLE,
			  HDMI_NV_PDISP_HDMI_VSYNC_WINDOW);

	if ((dc->mode.h_active == 720) && ((dc->mode.v_active == 480) || (dc->mode.v_active == 576)))
		tegra_hdmi_writel(hdmi,
				  (dc->ndev->id ? HDMI_SRC_DISPLAYB : HDMI_SRC_DISPLAYA) |
				  ARM_VIDEO_RANGE_FULL,
				  HDMI_NV_PDISP_INPUT_CONTROL);
	else
		tegra_hdmi_writel(hdmi,
				  (dc->ndev->id ? HDMI_SRC_DISPLAYB : HDMI_SRC_DISPLAYA) |
				  ARM_VIDEO_RANGE_LIMITED,
				  HDMI_NV_PDISP_INPUT_CONTROL);

	clk_disable_unprepare(hdmi->disp1_clk);
	clk_disable_unprepare(hdmi->disp2_clk);

	dispclk_div_8_2 = clk_get_rate(hdmi->clk) / 1000000 * 4;
	tegra_hdmi_writel(hdmi,
			  SOR_REFCLK_DIV_INT(dispclk_div_8_2 >> 2) |
			  SOR_REFCLK_DIV_FRAC(dispclk_div_8_2),
			  HDMI_NV_PDISP_SOR_REFCLK);

	hdmi->clk_enabled = true;

	if (!hdmi->dvi) {
		err = tegra_dc_hdmi_setup_audio(dc, hdmi->audio_freq,
			hdmi->audio_source);

		if (err < 0)
			hdmi->dvi = true;
	}

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
	if (hdmi->eld_retrieved)
		tegra_dc_hdmi_setup_eld_buff(dc);
#endif

	rekey = HDMI_REKEY_DEFAULT;
	val = HDMI_CTRL_REKEY(rekey);
	val |= HDMI_CTRL_MAX_AC_PACKET((dc->mode.h_sync_width +
					dc->mode.h_back_porch +
					dc->mode.h_front_porch -
					rekey - 18) / 32);
	if (!hdmi->dvi)
		val |= HDMI_CTRL_ENABLE;
	tegra_hdmi_writel(hdmi, val, HDMI_NV_PDISP_HDMI_CTRL);

	if (hdmi->dvi)
		tegra_hdmi_writel(hdmi, 0x0,
				  HDMI_NV_PDISP_HDMI_GENERIC_CTRL);
	else
		tegra_hdmi_writel(hdmi, GENERIC_CTRL_AUDIO,
				  HDMI_NV_PDISP_HDMI_GENERIC_CTRL);

	tegra_dc_hdmi_setup_avi_infoframe(dc, hdmi->dvi);

	if (dc->mode.stereo_mode)
		tegra_dc_hdmi_setup_stereo_infoframe(dc);
	else if (!tegra_dc_hdmi_setup_hdmi_vic_infoframe(dc, hdmi->dvi))
		tegra_dc_hdmi_disable_generic_infoframe(dc);

	tegra_dc_hdmi_setup_audio_infoframe(dc, hdmi->dvi);

	/* Set tmds config. Set it to custom values provided in board file;
	 * otherwise, set it to default values. */
	if (hdmi->info.tmds_config && hdmi->info.n_tmds_config) {
		tmds_ptr = hdmi->info.tmds_config;
		tmds_len = hdmi->info.n_tmds_config;
	} else {
		tmds_ptr = tmds_config;
		tmds_len = ARRAY_SIZE(tmds_config);
	}

	for (i = 0; i < tmds_len && tmds_ptr[i].pclk < dc->mode.pclk; i++)
		;
	if (i < tmds_len) {
		tegra_dc_hdmi_setup_tmds(hdmi, &tmds_ptr[i]);
	} else {
		dev_warn(&dc->ndev->dev,
			"pixel clock %u not present on TMDS table.\n",
			dc->mode.pclk);
		tegra_dc_hdmi_setup_tmds(hdmi, &tmds_ptr[tmds_len - 1]);
	}

	tegra_hdmi_writel(hdmi,
			  SOR_SEQ_CTL_PU_PC(0) |
			  SOR_SEQ_PU_PC_ALT(0) |
			  SOR_SEQ_PD_PC(8) |
			  SOR_SEQ_PD_PC_ALT(8),
			  HDMI_NV_PDISP_SOR_SEQ_CTL);

	val = SOR_SEQ_INST_WAIT_TIME(1) |
		SOR_SEQ_INST_WAIT_UNITS_VSYNC |
		SOR_SEQ_INST_HALT |
		SOR_SEQ_INST_PIN_A_LOW |
		SOR_SEQ_INST_PIN_B_LOW |
		SOR_SEQ_INST_DRIVE_PWM_OUT_LO;

	tegra_hdmi_writel(hdmi, val, HDMI_NV_PDISP_SOR_SEQ_INST0);
	tegra_hdmi_writel(hdmi, val, HDMI_NV_PDISP_SOR_SEQ_INST8);

	val = 0x1c800;
	val &= ~SOR_CSTM_ROTCLK(~0);
	val |= SOR_CSTM_ROTCLK(2);
	tegra_hdmi_writel(hdmi, val, HDMI_NV_PDISP_SOR_CSTM);


	tegra_dc_writel(dc, DISP_CTRL_MODE_STOP, DC_CMD_DISPLAY_COMMAND);
	tegra_dc_writel(dc, GENERAL_ACT_REQ << 8, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);


	/* start SOR */
	tegra_hdmi_writel(hdmi,
			  SOR_PWR_NORMAL_STATE_PU |
			  SOR_PWR_NORMAL_START_NORMAL |
			  SOR_PWR_SAFE_STATE_PD |
			  SOR_PWR_SETTING_NEW_TRIGGER,
			  HDMI_NV_PDISP_SOR_PWR);
	tegra_hdmi_writel(hdmi,
			  SOR_PWR_NORMAL_STATE_PU |
			  SOR_PWR_NORMAL_START_NORMAL |
			  SOR_PWR_SAFE_STATE_PD |
			  SOR_PWR_SETTING_NEW_DONE,
			  HDMI_NV_PDISP_SOR_PWR);

	retries = 1000;
	do {
		BUG_ON(--retries < 0);
		val = tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_SOR_PWR);
	} while (val & SOR_PWR_SETTING_NEW_PENDING);

	val = SOR_STATE_ASY_CRCMODE_COMPLETE |
		SOR_STATE_ASY_OWNER_HEAD0 |
		SOR_STATE_ASY_SUBOWNER_BOTH |
		SOR_STATE_ASY_PROTOCOL_SINGLE_TMDS_A |
		SOR_STATE_ASY_DEPOL_POS;

	if (dc->mode.flags & TEGRA_DC_MODE_FLAG_NEG_H_SYNC)
		val |= SOR_STATE_ASY_HSYNCPOL_NEG;
	else
		val |= SOR_STATE_ASY_HSYNCPOL_POS;

	if (dc->mode.flags & TEGRA_DC_MODE_FLAG_NEG_V_SYNC)
		val |= SOR_STATE_ASY_VSYNCPOL_NEG;
	else
		val |= SOR_STATE_ASY_VSYNCPOL_POS;

	tegra_hdmi_writel(hdmi, val, HDMI_NV_PDISP_SOR_STATE2);

	val = SOR_STATE_ASY_HEAD_OPMODE_AWAKE | SOR_STATE_ASY_ORMODE_NORMAL;
	tegra_hdmi_writel(hdmi, val, HDMI_NV_PDISP_SOR_STATE1);

	tegra_hdmi_writel(hdmi, 0, HDMI_NV_PDISP_SOR_STATE0);
	tegra_hdmi_writel(hdmi, SOR_STATE_UPDATE, HDMI_NV_PDISP_SOR_STATE0);
	tegra_hdmi_writel(hdmi, val | SOR_STATE_ATTACHED,
			  HDMI_NV_PDISP_SOR_STATE1);
	tegra_hdmi_writel(hdmi, 0, HDMI_NV_PDISP_SOR_STATE0);

	tegra_dc_writel(dc, HDMI_ENABLE, DC_DISP_DISP_WIN_OPTIONS);

	tegra_dc_writel(dc, PW0_ENABLE | PW1_ENABLE | PW2_ENABLE | PW3_ENABLE |
			PW4_ENABLE | PM0_ENABLE | PM1_ENABLE,
			DC_CMD_DISPLAY_POWER_CONTROL);

	tegra_dc_writel(dc, DISP_CTRL_MODE_C_DISPLAY, DC_CMD_DISPLAY_COMMAND);
	tegra_dc_writel(dc, GENERAL_ACT_REQ << 8, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);

	tegra_nvhdcp_set_plug(hdmi->nvhdcp, 1);
	tegra_dc_io_end(dc);
}

static void tegra_dc_hdmi_disable(struct tegra_dc *dc)
{
	struct tegra_dc_hdmi_data *hdmi = tegra_dc_get_outdata(dc);

	tegra_nvhdcp_set_plug(hdmi->nvhdcp, 0);

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
	tegra_hdmi_writel(hdmi, 0, HDMI_NV_PDISP_SOR_AUDIO_HDA_PRESENSE_0);
	/* sleep 1ms before disabling clocks to ensure HDA gets the interrupt */
	msleep(1);
	clk_disable_unprepare(hdmi->hda2hdmi_clk);
	clk_disable_unprepare(hdmi->hda2codec_clk);
	clk_disable_unprepare(hdmi->hda_clk);
#endif
	tegra_periph_reset_assert(hdmi->clk);
	hdmi->clk_enabled = false;
	clk_disable_unprepare(hdmi->clk);
	tegra_dvfs_set_rate(hdmi->clk, 0);
}

static long tegra_dc_hdmi_setup_clk(struct tegra_dc *dc, struct clk *clk)
{
	unsigned long rate;
	struct clk *parent_clk = clk_get_sys(NULL,
		dc->out->parent_clk ? : "pll_d_out0");
#if !defined(CONFIG_ARCH_TEGRA_12x_SOC)
	struct clk *base_clk = clk_get_parent(parent_clk);
#endif
	if (clk != dc->clk) {
#if !defined(CONFIG_ARCH_TEGRA_12x_SOC)
		clk_set_rate(base_clk, dc->mode.pclk);

		if (clk_get_parent(clk) != parent_clk)
			clk_set_parent(clk, parent_clk);

		clk_set_rate(clk, dc->mode.pclk / 4);
#else
		rate = 100000000;
		clk_set_rate(parent_clk, rate);
		if (clk_get_parent(clk) != parent_clk)
			clk_set_parent(clk, parent_clk);
		clk_set_rate(clk,  rate / 4);
#endif
	}

	/*
	 * Providing dynamic frequency rate setting for T20/T30 HDMI.
	 * The required rate needs to be setup at 4x multiplier,
	 * as out0 is 1/2 of the actual PLL output.
	 */
#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
	if (dc->mode.pclk == 25200000)
		rate = dc->mode.pclk  * 10;
	else {
		rate = dc->mode.pclk * 2;
		while (rate < 400000000)
			rate *= 2;
	}
#else
	rate = dc->mode.pclk * 2;
	while (rate < 500000000)
		rate *= 2;
#endif
#if !defined(CONFIG_ARCH_TEGRA_12x_SOC)
	if (rate != clk_get_rate(base_clk))
		clk_set_rate(base_clk, rate);
#else
	if (rate != clk_get_rate(parent_clk))
		clk_set_rate(parent_clk, rate);
#endif
	if (clk_get_parent(clk) != parent_clk)
		clk_set_parent(clk, parent_clk);

	return tegra_dc_pclk_round_rate(dc, dc->mode.pclk);
}

struct tegra_dc_out_ops tegra_dc_hdmi_ops = {
	.init = tegra_dc_hdmi_init,
	.destroy = tegra_dc_hdmi_destroy,
	.enable = tegra_dc_hdmi_enable,
	.disable = tegra_dc_hdmi_disable,
	.detect = tegra_dc_hdmi_detect,
	.suspend = tegra_dc_hdmi_suspend,
	.resume = tegra_dc_hdmi_resume,
	.mode_filter = tegra_dc_hdmi_mode_filter,
	.setup_clk = tegra_dc_hdmi_setup_clk,
};

struct tegra_dc_edid *tegra_dc_get_edid(struct tegra_dc *dc)
{
	struct tegra_dc_hdmi_data *hdmi;

	/* TODO: Support EDID on non-HDMI devices */
	if (dc->out->type != TEGRA_DC_OUT_HDMI)
		return ERR_PTR(-ENODEV);

	hdmi = tegra_dc_get_outdata(dc);

	return tegra_edid_get_data(hdmi->edid);
}
EXPORT_SYMBOL(tegra_dc_get_edid);

void tegra_dc_put_edid(struct tegra_dc_edid *edid)
{
	tegra_edid_put_data(edid);
}
EXPORT_SYMBOL(tegra_dc_put_edid);

struct tegra_dc *tegra_dc_hdmi_get_dc(struct tegra_dc_hdmi_data *hdmi)
{
	return hdmi ? hdmi->dc : NULL;
}
