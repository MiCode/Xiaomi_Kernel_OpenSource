/*
 * tegra30_dam.c - Tegra 30 DAM driver
 *
 * Author: Nikesh Oswal <noswal@nvidia.com>
 * Copyright (C) 2011 - NVIDIA, Inc.
 * Copyright (C) 2012-2014, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/clk.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <sound/soc.h>
#include "tegra30_dam.h"
#include "tegra30_ahub.h"

#define DRV_NAME "tegra30-dam"

static struct tegra30_dam_context	*dams_cont_info[TEGRA30_NR_DAM_IFC];

/*
Static Tables used by DAM driver
*/

enum {
	dam_ch_in0 = 0x0,
	dam_ch_in1,
	dam_ch_out,
	dam_ch_maxnum
} tegra30_dam_chtype;

#ifdef CONFIG_ARCH_TEGRA_3x_SOC
static struct tegra30_dam_src_step_table  step_table[] = {
	{ 8000, 44100, 80 },
	{ 8000, 48000, 1 },
	{ 16000, 44100, 160 },
	{ 16000, 48000, 1 },
	{ 44100, 8000, 441 },
	{ 48000, 8000, 0 },
	{ 44100, 16000, 441 },
	{ 48000, 16000, 0 },
};
#else
static int coefRam16To44[64] = {
				0x156105, // IIR Filter + interpolation
				0x0000d649,
				0x00e87afb, 0xff5f69d0, 0x003df3cf,
				0x007ce488, 0xff99a5c8, 0x0056a6a0,
				0x00344928, 0xffcba3e5, 0x006be470,
				0x00137aa7, 0xffe60276, 0x00773410,
				0x0005fa2a, 0xfff1ac11, 0x007c795b,
				0x00012d36, 0xfff5eca2, 0x007f10ef,
				0x2,
				0x21a102, // IIR Filter + interplation
				0x00000e00,
				0x00e2e000,0xff6e1a00,0x002aaa00,
				0x00610a00,0xff5dda00,0x003ccc00,
				0x00163a00,0xff3c0400,0x00633200,
				0x3,
				0x2c0204, // Farrow Filter
				0x000aaaab,
				0xffaaaaab,
				0xfffaaaab,
				0x00555555,
				0xff600000,
				0xfff55555,
				0x00155555,
				0x00055555,
				0xffeaaaab,
				0x00200000,
				0x005101, //IIR Filter + Decimator
				8252,
				16067893,-13754014,5906912,
				13037808,-13709975,7317389,
				1,
				0,0,0,0,0,
				0,0,0,0,0,0
};

static int coefRam8To48[64] = {
				0x156105, // interpolation + FIlter
				0x0000d649,
				0x00e87afb, 0xff5f69d0, 0x003df3cf,
				0x007ce488, 0xff99a5c8, 0x0056a6a0,
				0x00344928, 0xffcba3e5, 0x006be470,
				0x00137aa7, 0xffe60276, 0x00773410,
				0x0005fa2a, 0xfff1ac11, 0x007c795b,
				0x00012d36, 0xfff5eca2, 0x007f10ef,
				0x2, // ouptut gain
				0x00a102, // filter + interpolation
				0x00000e00,
				0x00e2e000,0xff6e1a00,0x002aaa00,
				0x00610a00,0xff5dda00,0x003ccc00,
				0x00163a00,0xff3c0400,0x00633200,
				0x3,
				0,0,0,0,0,0,
				0,0,0,0,0,0,
				0,0,0,0,0,0,
				0,0,0,0,0,0,
				0,0,0,0,0,0,
				0
};

static int coefRam16To48[64] = {
				0x00a105, // interpolation + Filter
				1924,
				13390190,-13855175,5952947,
				1289485,-12761191,6540917,
				-4787304,-11454255,7249439,
				-7239963,-10512732,7776366,
				-8255332,-9999487,8101770,
				-8632155,-9817625,8305531,
				0x3,0,0,
				0,0,0,
				0,0,0,
				0,0,0,
				0,0,0,
				0,0,0,
				0,0,0,
				0,0,0,
				0,0,0,
				0,0,0,
				0,0,0,
				0,0,0,
				0,0,0,
				0,0,0,
				0,0
};

static int coefRam44To16[64] = {
				0x126104, // IIR Filter + interp0lation
				2802,
				5762750,-14772125,6628868,
				-9304342,-14504578,7161825,
				-12409641,-14227678,7732611,
				-13291674,-14077653,8099947,
				-13563385,-14061743,8309372,
				2,
				0x1d9204, // Farrwo Filter + Decimation
				0x000aaaab,
				0xffaaaaab,
				0xfffaaaab,
				0x00555555,
				0xff600000,
				0xfff55555,
				0x00155555,
				0x00055555,
				0xffeaaaab,
				0x00200000,
				0x005105, // IIR Filter+decimation
				0x0000d649,
				0x00e87afb, 0xff5f69d0, 0x003df3cf,
				0x007ce488, 0xff99a5c8, 0x0056a6a0,
				0x00344928, 0xffcba3e5, 0x006be470,
				0x00137aa7, 0xffe60276, 0x00773410,
				0x0005fa2a, 0xfff1ac11, 0x007c795b,
				0x00012d36, 0xfff5eca2, 0x007f10ef,
				0x1,
				0,0,0,0,0,
				0,0,0,0,0,
				0,0,0,0
};

static int coefRam44To8[64] = {
				0x120104, // IIR Filter
				2802,
				5762750,-14772125,6628868,
				-9304342,-14504578,7161825,
				-12409641,-14227678,7732611,
				-13291674,-14077653,8099947,
				-13563385,-14061743,8309372,
				1,
				0x1d9204, // Farrwo Filter
				0x000aaaab,
				0xffaaaaab,
				0xfffaaaab,
				0x00555555,
				0xff600000,
				0xfff55555,
				0x00155555,
				0x00055555,
				0xffeaaaab,
				0x00200000,
				0x005105, // IIR Filter
				0x0000d649,
				0x00e87afb, 0xff5f69d0, 0x003df3cf,
				0x007ce488, 0xff99a5c8, 0x0056a6a0,
				0x00344928, 0xffcba3e5, 0x006be470,
				0x00137aa7, 0xffe60276, 0x00773410,
				0x0005fa2a, 0xfff1ac11, 0x007c795b,
				0x00012d36, 0xfff5eca2, 0x007f10ef,
				0x1,
				0,0,0,0,0,0,
				0,0,0,0,0,0,
				0,0
};

static int coefRam48To16[64] = {
				0x009105, // IIR FIlter + Decimation
				1924,
				13390190,-13855175,5952947,
				1289485,-12761191,6540917,
				-4787304,-11454255,7249439,
				-7239963,-10512732,7776366,
				-8255332,-9999487,8101770,
				-8632155,-9817625,8305531,
				0x1,
				0,0,0,
				0,0,0,0,0,0,0,0,
				0,0,0,0,0,0,0,0,
				0,0,0,0,0,0,0,0,
				0,0,0,0,0,0,0,0,
				0,0,0,0,0,0,0,0
};

static int coefRam48To8[64] = {
				0x0c9102,	//IIR Filter + decimation
				0x00000e00,
				0x00e2e000,0xff6e1a00,0x002aaa00,
				0x00610a00,0xff5dda00,0x003ccc00,
				0x00163a00,0xff3c0400,0x00633200,
				0x1,
				0x005105,   //IIR Filter + Decimator
				0x0000d649,
				0x00e87afb, 0xff5f69d0, 0x003df3cf,
				0x007ce488, 0xff99a5c8, 0x0056a6a0,
				0x00344928, 0xffcba3e5, 0x006be470,
				0x00137aa7, 0xffe60276, 0x00773410,
				0x0005fa2a, 0xfff1ac11, 0x007c795b,
				0x00012d36, 0xfff5eca2, 0x007f10ef,
				0x1, // ouptut gain
				0,0,0,0,0,0,
				0,0,0,0,0,0,
				0,0,0,0,0,0,
				0,0,0,0,0,0,
				0,0,0,0,0,0,
				0
};

static int coefRam8To44[64] = {
				0x0156105, // IIR filter +interpllation
				0x0000d649,
				0x00e87afb, 0xff5f69d0, 0x003df3cf,
				0x007ce488, 0xff99a5c8, 0x0056a6a0,
				0x00344928, 0xffcba3e5, 0x006be470,
				0x00137aa7, 0xffe60276, 0x00773410,
				0x0005fa2a, 0xfff1ac11, 0x007c795b,
				0x00012d36, 0xfff5eca2, 0x007f10ef,
				0x2, // ouptut gain
				0x21a102, // filter + interp0lation
				0x00000e00,
				0x00e2e000,0xff6e1a00,0x002aaa00,
				0x00610a00,0xff5dda00,0x003ccc00,
				0x00163a00,0xff3c0400,0x00633200,
				0x3,
				0x000204,
				0x000aaaab,
				0xffaaaaab,
				0xfffaaaab,
				0x00555555,
				0xff600000,
				0xfff55555,
				0x00155555,
				0x00055555,
				0xffeaaaab,
				0x00200000,
				0,0,
				0,0,0,0,0,0,
				0,0,0,0,0,0,
				0,0,0,0,0,0
};

static int coefRam8To16[64] = {
				0x00006105, // interpolation + IIR Filter
				0x0000d649, // input gain
				0x00e87afb, 0xff5f69d0, 0x003df3cf,
				0x007ce488, 0xff99a5c8, 0x0056a6a0,
				0x00344928, 0xffcba3e5, 0x006be470,
				0x00137aa7, 0xffe60276, 0x00773410,
				0x0005fa2a, 0xfff1ac11, 0x007c795b,
				0x00012d36, 0xfff5eca2, 0x007f10ef,
				0x00000002, // ouptut gain
};

static int coefRam16To8[64] = {
				0x00005105,   //IIR Filter + Decimator
				0x0000d649, //input gain
				0x00e87afb, 0xff5f69d0, 0x003df3cf,
				0x007ce488, 0xff99a5c8, 0x0056a6a0,
				0x00344928, 0xffcba3e5, 0x006be470,
				0x00137aa7, 0xffe60276, 0x00773410,
				0x0005fa2a, 0xfff1ac11, 0x007c795b,
				0x00012d36, 0xfff5eca2, 0x007f10ef,
				0x00000001, // ouptut gain
};
#endif

/*
Internally used helper (static) function prototypes
*/

static inline void tegra30_dam_writel(struct tegra30_dam_context *dam,
			u32 val, u32 reg);
static inline u32 tegra30_dam_readl(struct tegra30_dam_context *dam,
															u32 reg);
static void tegra30_dam_set_output_samplerate(
						struct tegra30_dam_context *dam, int fsout);
static void tegra30_dam_set_input_samplerate(
						struct tegra30_dam_context *dam, int fsin);
static int tegra30_dam_set_step_reset(struct tegra30_dam_context *dam,
										int insample, int outsample);
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
static void tegra30_dam_ch0_set_step(struct tegra30_dam_context *dam,
															int step);
#else
static void tegra30_dam_write_coeff_ram(struct tegra30_dam_context *dam,
												int fsin, int fsout);
static void tegra30_dam_set_farrow_param(
				struct tegra30_dam_context *dam, int fsin, int fsout);
static void tegra30_dam_set_biquad_fixed_coef(
									struct tegra30_dam_context *dam);
static void tegra30_dam_enable_coeff_ram(
									struct tegra30_dam_context *dam);
static void tegra30_dam_set_filter_stages(
				struct tegra30_dam_context *dam, int fsin, int fsout);
#endif


/*
Regmap and Runtime PM callback function
*/

static bool tegra30_dam_wr_rd_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA30_DAM_CTRL:
	case TEGRA30_DAM_CLIP:
	case TEGRA30_DAM_CLIP_THRESHOLD:
	case TEGRA30_DAM_AUDIOCIF_OUT_CTRL:
	case TEGRA30_DAM_CH0_CTRL:
	case TEGRA30_DAM_CH0_CONV:
	case TEGRA30_DAM_AUDIOCIF_CH0_CTRL:
	case TEGRA30_DAM_CH1_CTRL:
	case TEGRA30_DAM_CH1_CONV:
	case TEGRA30_DAM_AUDIOCIF_CH1_CTRL:
#ifndef CONFIG_ARCH_TEGRA_3x_SOC
	case TEGRA30_DAM_CH0_BIQUAD_FIXED_COEF_0:
	case TEGRA30_DAM_FARROW_PARAM_0:
	case TEGRA30_DAM_AUDIORAMCTL_DAM_CTRL_0:
	case TEGRA30_DAM_AUDIORAMCTL_DAM_DATA_0:
#endif
		return true;
	default:
		return false;
	};
}

static const struct regmap_config tegra30_dam_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
#ifndef CONFIG_ARCH_TEGRA_3x_SOC
	.max_register = TEGRA30_DAM_AUDIORAMCTL_DAM_DATA_0,
#else
	.max_register = TEGRA30_DAM_AUDIOCIF_CH1_CTRL,
#endif
	.writeable_reg = tegra30_dam_wr_rd_reg,
	.readable_reg = tegra30_dam_wr_rd_reg,
	.cache_type = REGCACHE_RBTREE,
};

static int tegra30_dam_runtime_suspend(struct device *dev)
{
	struct tegra30_dam_context *dam = dev_get_drvdata(dev);

	tegra30_ahub_disable_clocks();
	regcache_cache_only(dam->regmap, true);
	clk_disable_unprepare(dam->dam_clk);

	return 0;
}

static int tegra30_dam_runtime_resume(struct device *dev)
{
	struct tegra30_dam_context *dam = dev_get_drvdata(dev);
	int ret;

	tegra30_ahub_enable_clocks();
	ret = clk_prepare_enable(dam->dam_clk);
	if (ret) {
		dev_err(dev, "clk_enable failed: %d\n", ret);
		return ret;
	}
	regcache_cache_only(dam->regmap, false);

	return 0;
}

/*
DebugFs callback functions
*/

#ifdef CONFIG_DEBUG_FS
static int tegra30_dam_show(struct seq_file *s, void *unused)
{
#define REG(r) { r, #r }
	static const struct {
		int offset;
		const char *name;
	} regs[] = {
		REG(TEGRA30_DAM_CTRL),
		REG(TEGRA30_DAM_CLIP),
		REG(TEGRA30_DAM_CLIP_THRESHOLD),
		REG(TEGRA30_DAM_AUDIOCIF_OUT_CTRL),
		REG(TEGRA30_DAM_CH0_CTRL),
		REG(TEGRA30_DAM_CH0_CONV),
		REG(TEGRA30_DAM_AUDIOCIF_CH0_CTRL),
		REG(TEGRA30_DAM_CH1_CTRL),
		REG(TEGRA30_DAM_CH1_CONV),
		REG(TEGRA30_DAM_AUDIOCIF_CH1_CTRL),
	};
#undef REG

	struct tegra30_dam_context *dam = s->private;
	int i;

	tegra30_ahub_enable_clocks();
	clk_enable(dam->dam_clk);

	for (i = 0; i < ARRAY_SIZE(regs); i++) {
		u32 val = tegra30_dam_readl(dam, regs[i].offset);
		seq_printf(s, "%s = %08x\n", regs[i].name, val);
	}

	clk_disable(dam->dam_clk);
	tegra30_ahub_disable_clocks();

	return 0;
}

static int tegra30_dam_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, tegra30_dam_show, inode->i_private);
}

static const struct file_operations tegra30_dam_debug_fops = {
	.open    = tegra30_dam_debug_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static void tegra30_dam_debug_add(struct tegra30_dam_context *dam, int id)
{
	char name[] = DRV_NAME ".0";

	snprintf(name, sizeof(name), DRV_NAME".%1d", id);
	dam->debug = debugfs_create_file(name, S_IRUGO, snd_soc_debugfs_root,
			dam, &tegra30_dam_debug_fops);
}

static void tegra30_dam_debug_remove(struct tegra30_dam_context *dam)
{
	if (dam->debug)
		debugfs_remove(dam->debug);
}
#else
static inline void tegra30_dam_debug_add(struct tegra30_dam_context *dam,
						int id)
{
}

static inline void tegra30_dam_debug_remove(struct tegra30_dam_context *dam)
{
}
#endif

/*
Internally used helper (static) functions
*/
static inline void tegra30_dam_writel(struct tegra30_dam_context *dam,
			u32 val, u32 reg)
{
	regmap_write(dam->regmap, reg, val);
}

static inline u32 tegra30_dam_readl(struct tegra30_dam_context *dam, u32 reg)
{
	u32 val;

	regmap_read(dam->regmap, reg, &val);
	return val;
}

static void tegra30_dam_set_output_samplerate(struct tegra30_dam_context *dam,
					int fsout)
{
	u32 val;

	val = tegra30_dam_readl(dam, TEGRA30_DAM_CTRL);
	val &= ~TEGRA30_DAM_CTRL_FSOUT_MASK;

	switch (fsout) {
	case TEGRA30_AUDIO_SAMPLERATE_8000:
		val |= TEGRA30_DAM_CTRL_FSOUT_FS8;
		break;
	case TEGRA30_AUDIO_SAMPLERATE_16000:
		val |= TEGRA30_DAM_CTRL_FSOUT_FS16;
		break;
	case TEGRA30_AUDIO_SAMPLERATE_44100:
		val |= TEGRA30_DAM_CTRL_FSOUT_FS44;
		break;
	case TEGRA30_AUDIO_SAMPLERATE_48000:
		val |= TEGRA30_DAM_CTRL_FSOUT_FS48;
		break;
	default:
		break;
	}

	tegra30_dam_writel(dam, val, TEGRA30_DAM_CTRL);
}

static void tegra30_dam_set_input_samplerate(struct tegra30_dam_context *dam,
	int fsin)
{
	u32 val;

	val = tegra30_dam_readl(dam, TEGRA30_DAM_CH0_CTRL);
	val &= ~TEGRA30_DAM_CH0_CTRL_FSIN_MASK;

	switch (fsin) {
	case TEGRA30_AUDIO_SAMPLERATE_8000:
		val |= TEGRA30_DAM_CH0_CTRL_FSIN_FS8;
		break;
	case TEGRA30_AUDIO_SAMPLERATE_16000:
		val |= TEGRA30_DAM_CH0_CTRL_FSIN_FS16;
		break;
	case TEGRA30_AUDIO_SAMPLERATE_44100:
		val |= TEGRA30_DAM_CH0_CTRL_FSIN_FS44;
		break;
	case TEGRA30_AUDIO_SAMPLERATE_48000:
		val |= TEGRA30_DAM_CH0_CTRL_FSIN_FS48;
		break;
	default:
		break;
	}

	tegra30_dam_writel(dam, val, TEGRA30_DAM_CH0_CTRL);
}

static int tegra30_dam_set_step_reset(struct tegra30_dam_context *dam,
		int insample, int outsample)
{
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
	int step_reset = 0;
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(step_table); i++) {
		if ((insample == step_table[i].insample) &&
			(outsample == step_table[i].outsample))
			step_reset = step_table[i].stepreset;
	}

	tegra30_dam_ch0_set_step(dam, step_reset);
#else
	tegra30_dam_write_coeff_ram(dam, insample, outsample);
	tegra30_dam_set_farrow_param(dam, insample, outsample);
	tegra30_dam_set_biquad_fixed_coef(dam);
	tegra30_dam_enable_coeff_ram(dam);
	tegra30_dam_set_filter_stages(dam, insample, outsample);
#endif

	return 0;
}

#ifdef CONFIG_ARCH_TEGRA_3x_SOC
static void tegra30_dam_ch0_set_step(struct tegra30_dam_context *dam, int step)
{
	u32 val;

	val = tegra30_dam_readl(dam, TEGRA30_DAM_CH0_CTRL);
	val &= ~TEGRA30_DAM_CH0_CTRL_STEP_MASK;
	val |= step << TEGRA30_DAM_CH0_CTRL_STEP_SHIFT;
	tegra30_dam_writel(dam, val, TEGRA30_DAM_CH0_CTRL);
}
#else
static void tegra30_dam_write_coeff_ram(struct tegra30_dam_context *dam, int fsin, int fsout)
{
	u32 val;
	int i, *coefRam = NULL;

	tegra30_dam_writel(dam, 0x00002000,
			TEGRA30_DAM_AUDIORAMCTL_DAM_CTRL_0);

	switch(fsin) {
		case TEGRA30_AUDIO_SAMPLERATE_8000:
			if (fsout == TEGRA30_AUDIO_SAMPLERATE_48000)
				coefRam = coefRam8To48;
			else if (fsout == TEGRA30_AUDIO_SAMPLERATE_44100)
				coefRam = coefRam8To44;
			else if (fsout == TEGRA30_AUDIO_SAMPLERATE_16000)
				coefRam = coefRam8To16;
			break;

		case TEGRA30_AUDIO_SAMPLERATE_16000:
			if (fsout == TEGRA30_AUDIO_SAMPLERATE_48000)
				coefRam = coefRam16To48;
			else if (fsout == TEGRA30_AUDIO_SAMPLERATE_44100)
				coefRam = coefRam16To44;
			else if (fsout == TEGRA30_AUDIO_SAMPLERATE_8000)
				coefRam = coefRam16To8;
			break;

		case TEGRA30_AUDIO_SAMPLERATE_44100:
			if (fsout == TEGRA30_AUDIO_SAMPLERATE_8000)
				coefRam = coefRam44To8;
			else if (fsout == TEGRA30_AUDIO_SAMPLERATE_16000)
				coefRam = coefRam44To16;
			break;

		case TEGRA30_AUDIO_SAMPLERATE_48000:
			if (fsout == TEGRA30_AUDIO_SAMPLERATE_8000)
				coefRam = coefRam48To8;
			else if (fsout == TEGRA30_AUDIO_SAMPLERATE_16000)
				coefRam = coefRam48To16;
			break;

		default:
			break;
	}

	tegra30_dam_writel(dam, 0x00005000,
			TEGRA30_DAM_AUDIORAMCTL_DAM_CTRL_0);

	if (coefRam) {
		for (i = 0; i < 64; i++) {
			val = coefRam[i];
			tegra30_dam_writel(dam, val,
				TEGRA30_DAM_AUDIORAMCTL_DAM_DATA_0);
		}
	}
}

static void tegra30_dam_set_farrow_param(struct tegra30_dam_context *dam, int fsin, int fsout)
{
	u32 val = TEGRA30_FARROW_PARAM_RESET;

	switch(fsin) {
		case TEGRA30_AUDIO_SAMPLERATE_8000:
			if (fsout == TEGRA30_AUDIO_SAMPLERATE_48000)
				val = TEGRA30_FARROW_PARAM_1;
			else if (fsout == TEGRA30_AUDIO_SAMPLERATE_44100)
				val = TEGRA30_FARROW_PARAM_2;
			else if (fsout == TEGRA30_AUDIO_SAMPLERATE_16000)
				val = TEGRA30_FARROW_PARAM_1;
			break;

		case TEGRA30_AUDIO_SAMPLERATE_16000:
			if (fsout == TEGRA30_AUDIO_SAMPLERATE_48000)
				val = TEGRA30_FARROW_PARAM_1;
			else if (fsout == TEGRA30_AUDIO_SAMPLERATE_44100)
				val = TEGRA30_FARROW_PARAM_2;
			else if (fsout == TEGRA30_AUDIO_SAMPLERATE_8000)
				val = TEGRA30_FARROW_PARAM_1;
			break;

		case TEGRA30_AUDIO_SAMPLERATE_44100:
			if (fsout == TEGRA30_AUDIO_SAMPLERATE_8000)
				val = TEGRA30_FARROW_PARAM_3;
			else if (fsout == TEGRA30_AUDIO_SAMPLERATE_16000)
				val = TEGRA30_FARROW_PARAM_3;
			break;

		case TEGRA30_AUDIO_SAMPLERATE_48000:
			if (fsout == TEGRA30_AUDIO_SAMPLERATE_8000)
				val = TEGRA30_FARROW_PARAM_1;
			else if (fsout == TEGRA30_AUDIO_SAMPLERATE_16000)
				val = TEGRA30_FARROW_PARAM_1;
			break;

		default:
			break;
	}

	tegra30_dam_writel(dam, val,
			TEGRA30_DAM_FARROW_PARAM_0);
}

static void tegra30_dam_set_biquad_fixed_coef(struct tegra30_dam_context *dam)
{
	u32 val = TEGRA30_DAM_CH0_BIQUAD_FIXED_COEF_0_VAL;

	tegra30_dam_writel(dam, val,
			TEGRA30_DAM_CH0_BIQUAD_FIXED_COEF_0);
}

static void tegra30_dam_enable_coeff_ram(struct tegra30_dam_context *dam)
{
	u32 val;

	val = tegra30_dam_readl(dam, TEGRA30_DAM_CH0_CTRL);
	val |= TEGRA30_DAM_CH0_CTRL_COEFF_RAM_ENABLE;

	tegra30_dam_writel(dam, val, TEGRA30_DAM_CH0_CTRL);
}

static void tegra30_dam_set_filter_stages(struct tegra30_dam_context *dam, int fsin, int fsout)
{
	u32 val;
	int filt_stages = 0;

	val = tegra30_dam_readl(dam, TEGRA30_DAM_CH0_CTRL);

	switch(fsin) {
		case TEGRA30_AUDIO_SAMPLERATE_8000:
			if (fsout == TEGRA30_AUDIO_SAMPLERATE_48000)
				filt_stages = 1;
			else if (fsout == TEGRA30_AUDIO_SAMPLERATE_44100)
				filt_stages = 2;
			else if (fsout == TEGRA30_AUDIO_SAMPLERATE_16000)
				filt_stages = 0;
			break;

		case TEGRA30_AUDIO_SAMPLERATE_16000:
			if (fsout == TEGRA30_AUDIO_SAMPLERATE_48000)
				filt_stages = 0;
			else if (fsout == TEGRA30_AUDIO_SAMPLERATE_44100)
				filt_stages = 3;
			else if (fsout == TEGRA30_AUDIO_SAMPLERATE_8000)
				filt_stages = 0;
			break;

		case TEGRA30_AUDIO_SAMPLERATE_44100:
			if (fsout == TEGRA30_AUDIO_SAMPLERATE_8000)
				filt_stages = 2;
			else if (fsout == TEGRA30_AUDIO_SAMPLERATE_16000)
				filt_stages = 2;
			break;

		case TEGRA30_AUDIO_SAMPLERATE_48000:
			if (fsout == TEGRA30_AUDIO_SAMPLERATE_8000)
				filt_stages = 1;
			else if (fsout == TEGRA30_AUDIO_SAMPLERATE_16000)
				filt_stages = 0;
			break;

		default:
			break;
	}

	val |= filt_stages << TEGRA30_DAM_CH0_CTRL_FILT_STAGES_SHIFT;

	tegra30_dam_writel(dam, val, TEGRA30_DAM_CH0_CTRL);
}
#endif

/*
DAM Driver API's
*/

int tegra30_dam_allocate_controller()
{
	int i = 0;
	struct tegra30_dam_context *dam = NULL;

	for (i = 0; i < TEGRA30_NR_DAM_IFC; i++) {

		dam =  dams_cont_info[i];

		if (!dam->in_use) {
			dam->in_use = true;
			return i;
		}
	}

	return -ENOENT;
}

void tegra30_dam_disable_clock(int ifc)
{
	struct tegra30_dam_context *dam;

	if (ifc >= TEGRA30_NR_DAM_IFC)
		return;

	dam =  dams_cont_info[ifc];
	pm_runtime_put(dam->dev);
}

int tegra30_dam_enable_clock(int ifc)
{
	struct tegra30_dam_context *dam;

	if (ifc >= TEGRA30_NR_DAM_IFC)
		return -EINVAL;

	dam =  dams_cont_info[ifc];
	pm_runtime_get_sync(dam->dev);

	return 0;
}

int tegra30_dam_allocate_channel(int ifc, int chid)
{
	struct tegra30_dam_context *dam = NULL;

	if (ifc >= TEGRA30_NR_DAM_IFC)
		return -EINVAL;

	dam =  dams_cont_info[ifc];

	if (!dam->ch_alloc[chid]) {
		dam->ch_alloc[chid] = true;
		return 0;
	}

	return -ENOENT;
}

int tegra30_dam_free_channel(int ifc, int chid)
{
	struct tegra30_dam_context *dam = NULL;

	if (ifc >= TEGRA30_NR_DAM_IFC)
		return -EINVAL;

	dam =  dams_cont_info[ifc];

	if (dam->ch_alloc[chid]) {
		dam->ch_alloc[chid] = false;
		return 0;
	}

	return -EINVAL;
}

int tegra30_dam_free_controller(int ifc)
{
	struct tegra30_dam_context *dam = NULL;

	if (ifc >= TEGRA30_NR_DAM_IFC)
		return -EINVAL;

	dam =  dams_cont_info[ifc];

	if (!dam->ch_alloc[dam_ch_in0] &&
		!dam->ch_alloc[dam_ch_in1]) {
		dam->in_use = false;
		return 0;
	}

	return -EINVAL;
}

void tegra30_dam_set_samplerate(int ifc, int chid, int samplerate)
{
	struct tegra30_dam_context *dam = dams_cont_info[ifc];

	if (ifc >= TEGRA30_NR_DAM_IFC)
		return;

	switch (chid) {
	case dam_ch_in0:
		tegra30_dam_set_input_samplerate(dam, samplerate);
		dam->ch_insamplerate[dam_ch_in0] = samplerate;
		tegra30_dam_set_step_reset(dam, samplerate, dam->outsamplerate);
		break;
	case dam_ch_in1:
		if (samplerate != dam->outsamplerate)
			return;
		dam->ch_insamplerate[dam_ch_in1] = samplerate;
		break;
	case dam_ch_out:
		tegra30_dam_set_output_samplerate(dam, samplerate);
		dam->outsamplerate = samplerate;
		break;
	default:
		break;
	}
}

int tegra30_dam_set_gain(int ifc, int chid, int gain)
{
	if (ifc >= TEGRA30_NR_DAM_IFC)
		return -EINVAL;

	switch (chid) {
	case dam_ch_in0:
		tegra30_dam_writel(dams_cont_info[ifc], gain,
			TEGRA30_DAM_CH0_CONV);
		break;
	case dam_ch_in1:
		tegra30_dam_writel(dams_cont_info[ifc], gain,
			TEGRA30_DAM_CH1_CONV);
		break;
	default:
		break;
	}

	return 0;
}

int tegra30_dam_set_acif(int ifc, int chid, unsigned int audio_channels,
	unsigned int audio_bits, unsigned int client_channels,
	unsigned int client_bits)
{
	unsigned int reg;
	unsigned int value = 0;

	if (ifc >= TEGRA30_NR_DAM_IFC)
		return -EINVAL;

#ifndef CONFIG_ARCH_TEGRA_3x_SOC
	/*ch0 takes input as mono always*/
	if ((chid == dam_ch_in0) &&
		((client_channels != 1)))
		return -EINVAL;
	/*as per dam spec file chout is fixed to 32 bits*/
	/*so accept ch0, ch1 and chout as 32bit always*/
	if (client_bits != 32)
		return -EINVAL;
#else
	/*ch0 takes input as mono/16bit always*/
	if ((chid == dam_ch_in0) &&
		((client_channels != 1) || (client_bits != 16)))
		return -EINVAL;
#endif

	value |= TEGRA30_AUDIOCIF_CTRL_MONO_CONV_COPY;
	value |= TEGRA30_AUDIOCIF_CTRL_STEREO_CONV_AVG;
	value |= (audio_channels-1)  <<
				TEGRA30_AUDIOCIF_CTRL_AUDIO_CHANNELS_SHIFT;
	value |= (((audio_bits>>2)-1) <<
				TEGRA30_AUDIOCIF_CTRL_AUDIO_BITS_SHIFT);
	value |= (client_channels-1) <<
				TEGRA30_AUDIOCIF_CTRL_CLIENT_CHANNELS_SHIFT;
	value |= (((client_bits>>2)-1) <<
				TEGRA30_AUDIOCIF_CTRL_CLIENT_BITS_SHIFT);

	switch (chid) {
	case dam_ch_out:
		value |= TEGRA30_CIF_DIRECTION_TX;
		reg = TEGRA30_DAM_AUDIOCIF_OUT_CTRL;
		break;
	case dam_ch_in0:
		value |= TEGRA30_CIF_DIRECTION_RX;
		reg = TEGRA30_DAM_AUDIOCIF_CH0_CTRL;
		break;
	case dam_ch_in1:
		value |= TEGRA30_CIF_DIRECTION_RX;
		reg = TEGRA30_DAM_AUDIOCIF_CH1_CTRL;
		break;
	default:
		return -EINVAL;
	}

	tegra30_dam_writel(dams_cont_info[ifc], value, reg);

	return 0;
}

void tegra30_dam_enable(int ifc, int on, int chid)
{
	u32 old_val, val, enreg;
	u32 old_val_dam, val_dam;
	int dcnt = 10;
	struct tegra30_dam_context *dam;

	if ((ifc < 0) || (ifc >= TEGRA30_NR_DAM_IFC) ||
	    (chid < dam_ch_in0) || (chid > dam_ch_in1))
		return;

	dam = dams_cont_info[ifc];

	if (chid == dam_ch_in0)
		enreg = TEGRA30_DAM_CH0_CTRL;
	else
		enreg = TEGRA30_DAM_CH1_CTRL;

	old_val = val = tegra30_dam_readl(dam, enreg);

	if (on) {
		if (!dam->ch_enable_refcnt[chid]++)
			val |= TEGRA30_DAM_CH0_CTRL_EN;
	} else if (dam->ch_enable_refcnt[chid]) {
		dam->ch_enable_refcnt[chid]--;
		if (!dam->ch_enable_refcnt[chid])
			val &= ~TEGRA30_DAM_CH0_CTRL_EN;
	}

	old_val_dam = val_dam = tegra30_dam_readl(dam, TEGRA30_DAM_CTRL);

	if (dam->ch_enable_refcnt[dam_ch_in0] ||
		dam->ch_enable_refcnt[dam_ch_in1])
		val_dam |= TEGRA30_DAM_CTRL_DAM_EN;
	else
		val_dam &= ~TEGRA30_DAM_CTRL_DAM_EN;

	if (val != old_val) {
		tegra30_dam_writel(dam, val, enreg);

		if (!on) {
			if (chid == dam_ch_in0) {
				while (!tegra30_ahub_dam_ch0_is_empty(ifc)
					&& dcnt--)
					udelay(100);

				dcnt = 10;
			}
			else {
				while (!tegra30_ahub_dam_ch1_is_empty(ifc)
					&& dcnt--)
					udelay(100);

				dcnt = 10;
			}
		}
	}

	if (old_val_dam != val_dam) {
		tegra30_dam_writel(dam, val_dam, TEGRA30_DAM_CTRL);
		if (!on) {
			while (!tegra30_ahub_dam_tx_is_empty(ifc) && dcnt--)
				udelay(100);

			dcnt = 10;
		}
	}
}

void tegra30_dam_ch0_set_datasync(int ifc, int datasync)
{
	u32 val;
	struct tegra30_dam_context *dam = NULL;

	dam =  dams_cont_info[ifc];
	val = tegra30_dam_readl(dam, TEGRA30_DAM_CH0_CTRL);
	val &= ~TEGRA30_DAM_CH0_CTRL_DATA_SYNC_MASK;
	val |= datasync << TEGRA30_DAM_DATA_SYNC_SHIFT;
	tegra30_dam_writel(dam, val, TEGRA30_DAM_CH0_CTRL);
}

void tegra30_dam_ch1_set_datasync(int ifc, int datasync)
{
	u32 val;
	struct tegra30_dam_context *dam = NULL;

	dam =  dams_cont_info[ifc];
	val = tegra30_dam_readl(dam, TEGRA30_DAM_CH1_CTRL);
	val &= ~TEGRA30_DAM_CH1_CTRL_DATA_SYNC_MASK;
	val |= datasync << TEGRA30_DAM_DATA_SYNC_SHIFT;
	tegra30_dam_writel(dam, val, TEGRA30_DAM_CH1_CTRL);
}

void tegra30_dam_enable_clip_counter(struct tegra30_dam_context *dam, int on)
{
	u32 val;

	val = tegra30_dam_readl(dam, TEGRA30_DAM_CLIP);
	val &= ~TEGRA30_DAM_CLIP_COUNTER_ENABLE;
	val |= on ?  TEGRA30_DAM_CLIP_COUNTER_ENABLE : 0;
	tegra30_dam_writel(dam, val, TEGRA30_DAM_CLIP);
}

int tegra30_dam_set_acif_stereo_conv(int ifc, int chtype, int conv)
{
	unsigned int reg;
	unsigned int val = 0;

	if (ifc >= TEGRA30_NR_DAM_IFC)
		return -EINVAL;

	if ((conv != TEGRA30_CIF_STEREOCONV_CH0) &&
		(conv != TEGRA30_CIF_STEREOCONV_CH1) &&
		(conv != TEGRA30_CIF_STEREOCONV_AVG))
			return -EINVAL;

	switch (chtype) {
	case dam_ch_out:
		reg = TEGRA30_DAM_AUDIOCIF_OUT_CTRL;
		break;
	case dam_ch_in0:
		reg = TEGRA30_DAM_AUDIOCIF_CH0_CTRL;
		break;
	case dam_ch_in1:
		reg = TEGRA30_DAM_AUDIOCIF_CH1_CTRL;
		break;
	default:
		return -EINVAL;
	}

	val = tegra30_dam_readl(dams_cont_info[ifc], reg);
	val &= ~TEGRA30_CIF_STEREOCONV_MASK;
	val |= conv;

	tegra30_dam_writel(dams_cont_info[ifc], val, reg);

	return 0;
}


/*
DAM Driver probe and remove functions
*/

static int tegra30_dam_probe(struct platform_device *pdev)
{
	struct resource *res,  *region;
	struct tegra30_dam_context *dam;
	int ret = 0;
	int clkm_rate;
	u32 val32;

	if (pdev->dev.of_node) {
		of_property_read_u32(pdev->dev.of_node, "nvidia,ahub-dam-id",
			&val32);
		pdev->id = (int)val32;
	}

	if ((pdev->id < 0) ||
		(pdev->id >= TEGRA30_NR_DAM_IFC)) {
		dev_err(&pdev->dev, "ID %d out of range\n", pdev->id);
		return -EINVAL;
	}

	dams_cont_info[pdev->id] = devm_kzalloc(&pdev->dev,
					sizeof(struct tegra30_dam_context),
					GFP_KERNEL);
	if (!dams_cont_info[pdev->id]) {
		dev_err(&pdev->dev, "Can't allocate dam context\n");
		ret = -ENOMEM;
		goto exit;
	}
	dams_cont_info[pdev->id]->dev = &pdev->dev;

	dam = dams_cont_info[pdev->id];
	dev_set_drvdata(&pdev->dev, dam);

	dam->dam_clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(dam->dam_clk)) {
		dev_err(&pdev->dev, "Can't retrieve dam clock\n");
		ret = PTR_ERR(dam->dam_clk);
		goto err_free;
	}
	clkm_rate = clk_get_rate(clk_get_parent(dam->dam_clk));
	while (clkm_rate > 13000000)
		clkm_rate >>= 1;

	clk_set_rate(dam->dam_clk,clkm_rate);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "No memory 0 resource\n");
		ret = -ENODEV;
		goto err_clk_put_dam;
	}

	region = devm_request_mem_region(&pdev->dev, res->start,
			resource_size(res), pdev->name);
	if (!region) {
		dev_err(&pdev->dev, "Memory region 0 already claimed\n");
		ret = -EBUSY;
		goto err_clk_put_dam;
	}

	dam->damregs = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!dam->damregs) {
		dev_err(&pdev->dev, "ioremap 0 failed\n");
		ret = -ENOMEM;
		goto err_clk_put_dam;
	}

	dam->regmap = devm_regmap_init_mmio(&pdev->dev, dam->damregs,
				    &tegra30_dam_regmap_config);
	if (IS_ERR(dam->regmap)) {
		dev_err(&pdev->dev, "regmap init failed\n");
		ret = PTR_ERR(dam->regmap);
		goto err_clk_put_dam;
	}
	regcache_cache_only(dam->regmap, true);

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = tegra30_dam_runtime_resume(&pdev->dev);
		if (ret)
			goto err_pm_disable;
	}

	tegra30_dam_debug_add(dam, pdev->id);

	return 0;

err_pm_disable:
	pm_runtime_disable(&pdev->dev);
err_clk_put_dam:
	clk_put(dam->dam_clk);
err_free:
	dams_cont_info[pdev->id] = NULL;
exit:
	return ret;
}

static int tegra30_dam_remove(struct platform_device *pdev)
{
	struct tegra30_dam_context *dam;

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra30_dam_runtime_suspend(&pdev->dev);

	dam = platform_get_drvdata(pdev);
	clk_put(dam->dam_clk);
	tegra30_dam_debug_remove(dam);
	dams_cont_info[pdev->id] = NULL;

	return 0;
}

static const struct of_device_id tegra30_dam_of_match[] = {
	{ .compatible = "nvidia,tegra30-dam",},
	{},
};

static const struct dev_pm_ops tegra30_dam_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra30_dam_runtime_suspend,
			   tegra30_dam_runtime_resume, NULL)
};

static struct platform_driver tegra30_dam_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tegra30_dam_of_match,
		.pm = &tegra30_dam_pm_ops,
	},
	.probe = tegra30_dam_probe,
	.remove = tegra30_dam_remove,
};
module_platform_driver(tegra30_dam_driver);

MODULE_AUTHOR("Nikesh Oswal <noswal@nvidia.com>");
MODULE_DESCRIPTION("Tegra 30 DAM driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
