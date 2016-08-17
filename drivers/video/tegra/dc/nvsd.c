/*
 * drivers/video/tegra/dc/nvsd.c
 *
 * Copyright (c) 2010-2012, NVIDIA CORPORATION, All rights reserved.
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
#include <mach/dc.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/backlight.h>
#include <linux/stat.h>

#include "dc_reg.h"
#include "dc_priv.h"
#include "nvsd.h"

/* Elements for sysfs access */
#define NVSD_ATTR(__name) static struct kobj_attribute nvsd_attr_##__name = \
	__ATTR(__name, S_IRUGO|S_IWUSR, nvsd_settings_show, nvsd_settings_store)
#define NVSD_ATTRS_ENTRY(__name) (&nvsd_attr_##__name.attr)
#define IS_NVSD_ATTR(__name) (attr == &nvsd_attr_##__name)

static ssize_t nvsd_settings_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf);

static ssize_t nvsd_settings_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count);

static ssize_t nvsd_registers_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf);

NVSD_ATTR(enable);
NVSD_ATTR(aggressiveness);
NVSD_ATTR(phase_in_settings);
NVSD_ATTR(phase_in_adjustments);
NVSD_ATTR(bin_width);
NVSD_ATTR(hw_update_delay);
NVSD_ATTR(use_vid_luma);
NVSD_ATTR(coeff);
NVSD_ATTR(blp_time_constant);
NVSD_ATTR(blp_step);
NVSD_ATTR(fc_time_limit);
NVSD_ATTR(fc_threshold);
NVSD_ATTR(lut);
NVSD_ATTR(bltf);
#ifdef CONFIG_TEGRA_SD_GEN2
NVSD_ATTR(k_limit_enable);
NVSD_ATTR(k_limit);
NVSD_ATTR(sd_window_enable);
NVSD_ATTR(sd_window);
NVSD_ATTR(soft_clipping_enable);
NVSD_ATTR(soft_clipping_threshold);
NVSD_ATTR(smooth_k_enable);
NVSD_ATTR(smooth_k_incr);
NVSD_ATTR(use_vpulse2);
#endif
static struct kobj_attribute nvsd_attr_registers =
	__ATTR(registers, S_IRUGO, nvsd_registers_show, NULL);

static struct attribute *nvsd_attrs[] = {
	NVSD_ATTRS_ENTRY(enable),
	NVSD_ATTRS_ENTRY(aggressiveness),
	NVSD_ATTRS_ENTRY(phase_in_settings),
	NVSD_ATTRS_ENTRY(phase_in_adjustments),
	NVSD_ATTRS_ENTRY(bin_width),
	NVSD_ATTRS_ENTRY(hw_update_delay),
	NVSD_ATTRS_ENTRY(use_vid_luma),
	NVSD_ATTRS_ENTRY(coeff),
	NVSD_ATTRS_ENTRY(blp_time_constant),
	NVSD_ATTRS_ENTRY(blp_step),
	NVSD_ATTRS_ENTRY(fc_time_limit),
	NVSD_ATTRS_ENTRY(fc_threshold),
	NVSD_ATTRS_ENTRY(lut),
	NVSD_ATTRS_ENTRY(bltf),
	NVSD_ATTRS_ENTRY(registers),
#ifdef CONFIG_TEGRA_SD_GEN2
	NVSD_ATTRS_ENTRY(k_limit_enable),
	NVSD_ATTRS_ENTRY(k_limit),
	NVSD_ATTRS_ENTRY(sd_window_enable),
	NVSD_ATTRS_ENTRY(sd_window),
	NVSD_ATTRS_ENTRY(soft_clipping_enable),
	NVSD_ATTRS_ENTRY(soft_clipping_threshold),
	NVSD_ATTRS_ENTRY(smooth_k_enable),
	NVSD_ATTRS_ENTRY(smooth_k_incr),
	NVSD_ATTRS_ENTRY(use_vpulse2),
#endif
	NULL,
};

static struct attribute_group nvsd_attr_group = {
	.attrs = nvsd_attrs,
};

static struct kobject *nvsd_kobj;

/* shared brightness variable */
static atomic_t *sd_brightness;
/* shared boolean for manual K workaround */
static atomic_t man_k_until_blank = ATOMIC_INIT(0);

static u8 nvsd_get_bw_idx(struct tegra_dc_sd_settings *settings)
{
	u8 bw;

	switch (settings->bin_width) {
	default:
	case -1:
	/* A -1 bin-width indicates 'automatic'
	   based upon aggressiveness. */
		settings->bin_width = -1;
		switch (settings->aggressiveness) {
		default:
		case 0:
		case 1:
			bw = SD_BIN_WIDTH_ONE;
			break;
		case 2:
		case 3:
		case 4:
			bw = SD_BIN_WIDTH_TWO;
			break;
		case 5:
			bw = SD_BIN_WIDTH_FOUR;
			break;
		}
		break;
	case 1:
		bw = SD_BIN_WIDTH_ONE;
		break;
	case 2:
		bw = SD_BIN_WIDTH_TWO;
		break;
	case 4:
		bw = SD_BIN_WIDTH_FOUR;
		break;
	case 8:
		bw = SD_BIN_WIDTH_EIGHT;
		break;
	}
	return bw >> 3;

}

static bool nvsd_phase_in_adjustments(struct tegra_dc *dc,
	struct tegra_dc_sd_settings *settings)
{
	u8 step, cur_sd_brightness;
	u16 target_k, cur_k;
	u32 man_k, val;

	cur_sd_brightness = atomic_read(sd_brightness);

	target_k = tegra_dc_readl(dc, DC_DISP_SD_HW_K_VALUES);
	target_k = SD_HW_K_R(target_k);
	cur_k = tegra_dc_readl(dc, DC_DISP_SD_MAN_K_VALUES);
	cur_k = SD_HW_K_R(cur_k);

	/* read brightness value */
	val = tegra_dc_readl(dc, DC_DISP_SD_BL_CONTROL);
	val = SD_BLC_BRIGHTNESS(val);

	step = settings->phase_adj_step;
	if (cur_sd_brightness != val || target_k != cur_k) {
		if (!step)
			step = ADJ_PHASE_STEP;

		/* Phase in Backlight and Pixel K
		every ADJ_PHASE_STEP frames*/
		if ((step-- & ADJ_PHASE_STEP) == ADJ_PHASE_STEP) {

			if (val != cur_sd_brightness) {
				val > cur_sd_brightness ?
				(cur_sd_brightness++) :
				(cur_sd_brightness--);
			}

			if (target_k != cur_k) {
				if (target_k > cur_k)
					cur_k += K_STEP;
				else
					cur_k -= K_STEP;
			}

			/* Set manual k value */
			man_k = SD_MAN_K_R(cur_k) |
				SD_MAN_K_G(cur_k) | SD_MAN_K_B(cur_k);
			tegra_dc_io_start(dc);
			tegra_dc_writel(dc, man_k, DC_DISP_SD_MAN_K_VALUES);
			tegra_dc_io_end(dc);
			/* Set manual brightness value */
			atomic_set(sd_brightness, cur_sd_brightness);
		}
		settings->phase_adj_step = step;
		return true;
	} else
		return false;
}

/* phase in the luts based on the current and max step */
static void nvsd_phase_in_luts(struct tegra_dc_sd_settings *settings,
	struct tegra_dc *dc)
{
	u32 val;
	u8 bw_idx;
	int i;
	u16 phase_settings_step = settings->phase_settings_step;
	u16 num_phase_in_steps = settings->num_phase_in_steps;

	bw_idx = nvsd_get_bw_idx(settings);

	/* Phase in Final LUT */
	for (i = 0; i < DC_DISP_SD_LUT_NUM; i++) {
		val = SD_LUT_R((settings->lut[bw_idx][i].r *
				phase_settings_step)/num_phase_in_steps) |
			SD_LUT_G((settings->lut[bw_idx][i].g *
				phase_settings_step)/num_phase_in_steps) |
			SD_LUT_B((settings->lut[bw_idx][i].b *
				phase_settings_step)/num_phase_in_steps);

		tegra_dc_writel(dc, val, DC_DISP_SD_LUT(i));
	}
	/* Phase in Final BLTF */
	for (i = 0; i < DC_DISP_SD_BL_TF_NUM; i++) {
		val = SD_BL_TF_POINT_0(255-((255-settings->bltf[bw_idx][i][0])
				* phase_settings_step)/num_phase_in_steps) |
			SD_BL_TF_POINT_1(255-((255-settings->bltf[bw_idx][i][1])
				* phase_settings_step)/num_phase_in_steps) |
			SD_BL_TF_POINT_2(255-((255-settings->bltf[bw_idx][i][2])
				* phase_settings_step)/num_phase_in_steps) |
			SD_BL_TF_POINT_3(255-((255-settings->bltf[bw_idx][i][3])
				* phase_settings_step)/num_phase_in_steps);

		tegra_dc_writel(dc, val, DC_DISP_SD_BL_TF(i));
	}
}

/* handle the commands that may be invoked for phase_in_settings */
static void nvsd_cmd_handler(struct tegra_dc_sd_settings *settings,
	struct tegra_dc *dc)
{
	u32 val;
	u8 bw_idx, bw;

	if (settings->cmd & ENABLE) {
		settings->phase_settings_step++;
		if (settings->phase_settings_step >=
				settings->num_phase_in_steps)
			settings->cmd &= ~ENABLE;

		nvsd_phase_in_luts(settings, dc);
	}
	if (settings->cmd & DISABLE) {
		settings->phase_settings_step--;
		nvsd_phase_in_luts(settings, dc);
		if (settings->phase_settings_step == 0) {
			/* finish up aggressiveness phase in */
			if (settings->cmd & AGG_CHG)
				settings->aggressiveness = settings->final_agg;
			settings->cmd = NO_CMD;
			settings->enable = 0;
			nvsd_init(dc, settings);
		}
	}
	if (settings->cmd & AGG_CHG) {
		if (settings->aggressiveness == settings->final_agg)
			settings->cmd &= ~AGG_CHG;
		if ((settings->cur_agg_step++ & (STEPS_PER_AGG_CHG - 1)) == 0) {
			settings->final_agg > settings->aggressiveness ?
				settings->aggressiveness++ :
				settings->aggressiveness--;

			/* Update aggressiveness value in HW */
			val = tegra_dc_readl(dc, DC_DISP_SD_CONTROL);
			val &= ~SD_AGGRESSIVENESS(0x7);
			val |= SD_AGGRESSIVENESS(settings->aggressiveness);

			/* Adjust bin_width for automatic setting */
			if (settings->bin_width == -1) {
				bw_idx = nvsd_get_bw_idx(settings);

				bw = bw_idx << 3;

				val &= ~SD_BIN_WIDTH_MASK;
				val |= bw;
			}

			tegra_dc_writel(dc, val, DC_DISP_SD_CONTROL);

			nvsd_phase_in_luts(settings, dc);
		}
	}
}

static bool nvsd_update_enable(struct tegra_dc_sd_settings *settings,
	int enable_val)
{

	if (enable_val != 1 && enable_val != 0)
		return false;

	if (!settings->cmd && settings->enable != enable_val) {
		settings->num_phase_in_steps =
			STEPS_PER_AGG_LVL*settings->aggressiveness;
		settings->phase_settings_step = enable_val ?
			0 : settings->num_phase_in_steps;
	}

	if (settings->enable != enable_val || settings->cmd & DISABLE) {
		settings->cmd &= ~(ENABLE | DISABLE);
		if (!settings->enable && enable_val)
			settings->cmd |= PHASE_IN;
		settings->cmd |= enable_val ? ENABLE : DISABLE;
		return true;
	}

	return false;
}

static bool nvsd_update_agg(struct tegra_dc_sd_settings *settings, int agg_val)
{
	int i;
	int pri_lvl = SD_AGG_PRI_LVL(agg_val);
	int agg_lvl = SD_GET_AGG(agg_val);
	struct tegra_dc_sd_agg_priorities *sd_agg_priorities =
		&settings->agg_priorities;

	if (agg_lvl > 5 || agg_lvl < 0)
		return false;
	else if (agg_lvl == 0 && pri_lvl == 0)
		return false;

	if (pri_lvl >= 0 && pri_lvl < 4)
		sd_agg_priorities->agg[pri_lvl] = agg_lvl;

	for (i = NUM_AGG_PRI_LVLS - 1; i >= 0; i--) {
		if (sd_agg_priorities->agg[i])
			break;
	}

	sd_agg_priorities->pri_lvl = i;
	pri_lvl = i;
	agg_lvl = sd_agg_priorities->agg[i];

	if (settings->phase_in_settings && settings->enable &&
		settings->aggressiveness != agg_lvl) {

		settings->final_agg = agg_lvl;
		settings->cmd |= AGG_CHG;
		settings->cur_agg_step = 0;
		return true;
	} else if (settings->aggressiveness != agg_lvl) {
		settings->aggressiveness = agg_lvl;
		return true;
	}

	return false;
}

/* Functional initialization */
void nvsd_init(struct tegra_dc *dc, struct tegra_dc_sd_settings *settings)
{
	u32 i = 0;
	u32 val = 0;
	u32 bw = 0;
	u32 bw_idx = 0;
	/* TODO: check if HW says SD's available */

	tegra_dc_io_start(dc);
	/* If SD's not present or disabled, clear the register and return. */
	if (!settings || settings->enable == 0) {
		/* clear the brightness val, too. */
		if (sd_brightness)
			atomic_set(sd_brightness, 255);

		sd_brightness = NULL;

		if (settings)
			settings->phase_settings_step = 0;
		tegra_dc_writel(dc, 0, DC_DISP_SD_CONTROL);
		tegra_dc_io_end(dc);
		return;
	}

	dev_dbg(&dc->ndev->dev, "NVSD Init:\n");

	/* init agg_priorities */
	if (!settings->agg_priorities.agg[0])
		settings->agg_priorities.agg[0] = settings->aggressiveness;

	/* WAR: Settings will not be valid until the next flip.
	 * Thus, set manual K to either HW's current value (if
	 * we're already enabled) or a non-effective value (if
	 * we're about to enable). */
	val = tegra_dc_readl(dc, DC_DISP_SD_CONTROL);

	if (val & SD_ENABLE_NORMAL)
		if (settings->phase_in_adjustments)
			i = tegra_dc_readl(dc, DC_DISP_SD_MAN_K_VALUES);
		else
			i = tegra_dc_readl(dc, DC_DISP_SD_HW_K_VALUES);
	else
		i = 0; /* 0 values for RGB = 1.0, i.e. non-affected */

	tegra_dc_writel(dc, i, DC_DISP_SD_MAN_K_VALUES);
	/* Enable manual correction mode here so that changing the
	 * settings won't immediately impact display dehavior. */
	val |= SD_CORRECTION_MODE_MAN;
	tegra_dc_writel(dc, val, DC_DISP_SD_CONTROL);

	bw_idx = nvsd_get_bw_idx(settings);
	bw = SD_BIN_WIDTH(bw_idx);

	/* Values of SD LUT & BL TF are different according to bin_width on T30
	 * due to HW bug. Therefore we use bin_width to select the correct table
	 * on T30. On T114, we will use 1st table by default.*/
#ifdef CONFIG_TEGRA_SD_GEN2
	bw_idx = 0;
#endif
	/* Write LUT */
	if (!settings->cmd) {
		dev_dbg(&dc->ndev->dev, "  LUT:\n");

		for (i = 0; i < DC_DISP_SD_LUT_NUM; i++) {
			val = SD_LUT_R(settings->lut[bw_idx][i].r) |
				SD_LUT_G(settings->lut[bw_idx][i].g) |
				SD_LUT_B(settings->lut[bw_idx][i].b);
			tegra_dc_writel(dc, val, DC_DISP_SD_LUT(i));

			dev_dbg(&dc->ndev->dev, "    %d: 0x%08x\n", i, val);
		}
	}

	/* Write BL TF */
	if (!settings->cmd) {
		dev_dbg(&dc->ndev->dev, "  BL_TF:\n");

		for (i = 0; i < DC_DISP_SD_BL_TF_NUM; i++) {
			val = SD_BL_TF_POINT_0(settings->bltf[bw_idx][i][0]) |
				SD_BL_TF_POINT_1(settings->bltf[bw_idx][i][1]) |
				SD_BL_TF_POINT_2(settings->bltf[bw_idx][i][2]) |
				SD_BL_TF_POINT_3(settings->bltf[bw_idx][i][3]);

			tegra_dc_writel(dc, val, DC_DISP_SD_BL_TF(i));

			dev_dbg(&dc->ndev->dev, "    %d: 0x%08x\n", i, val);
		}
	} else if ((settings->cmd & PHASE_IN)) {
		settings->cmd &= ~PHASE_IN;
		/* Write NO_OP values for BLTF */
		for (i = 0; i < DC_DISP_SD_BL_TF_NUM; i++) {
			val = SD_BL_TF_POINT_0(0xFF) |
				SD_BL_TF_POINT_1(0xFF) |
				SD_BL_TF_POINT_2(0xFF) |
				SD_BL_TF_POINT_3(0xFF);

			tegra_dc_writel(dc, val, DC_DISP_SD_BL_TF(i));

			dev_dbg(&dc->ndev->dev, "    %d: 0x%08x\n", i, val);
		}
	}

	/* Set step correctly on init */
	if (!settings->cmd && settings->phase_in_settings) {
		settings->num_phase_in_steps = STEPS_PER_AGG_LVL *
			settings->aggressiveness;
		settings->phase_settings_step = settings->enable ?
			settings->num_phase_in_steps : 0;
	}

	/* Write Coeff */
	val = SD_CSC_COEFF_R(settings->coeff.r) |
		SD_CSC_COEFF_G(settings->coeff.g) |
		SD_CSC_COEFF_B(settings->coeff.b);
	tegra_dc_writel(dc, val, DC_DISP_SD_CSC_COEFF);
	dev_dbg(&dc->ndev->dev, "  COEFF: 0x%08x\n", val);

	/* Write BL Params */
	val = SD_BLP_TIME_CONSTANT(settings->blp.time_constant) |
		SD_BLP_STEP(settings->blp.step);
	tegra_dc_writel(dc, val, DC_DISP_SD_BL_PARAMETERS);
	dev_dbg(&dc->ndev->dev, "  BLP: 0x%08x\n", val);

	/* Write Auto/Manual PWM */
	val = (settings->use_auto_pwm) ? SD_BLC_MODE_AUTO : SD_BLC_MODE_MAN;
	tegra_dc_writel(dc, val, DC_DISP_SD_BL_CONTROL);
	dev_dbg(&dc->ndev->dev, "  BL_CONTROL: 0x%08x\n", val);

	/* Write Flicker Control */
	val = SD_FC_TIME_LIMIT(settings->fc.time_limit) |
		SD_FC_THRESHOLD(settings->fc.threshold);
	tegra_dc_writel(dc, val, DC_DISP_SD_FLICKER_CONTROL);
	dev_dbg(&dc->ndev->dev, "  FLICKER_CONTROL: 0x%08x\n", val);

#ifdef CONFIG_TEGRA_SD_GEN2
	/* Write K limit */
	if (settings->k_limit_enable) {
		val = settings->k_limit;
		if (val < 128)
			val = 128;
		else if (val > 255)
			val = 255;
		val = SD_K_LIMIT(val);
		tegra_dc_writel(dc, val, DC_DISP_SD_K_LIMIT);
		dev_dbg(&dc->ndev->dev, "  K_LIMIT: 0x%08x\n", val);
	}

	if (settings->sd_window_enable) {
		/* Write sd window */
		val = SD_WIN_H_POSITION(settings->sd_window.h_position) |
			SD_WIN_V_POSITION(settings->sd_window.v_position);
		tegra_dc_writel(dc, val, DC_DISP_SD_WINDOW_POSITION);
		dev_dbg(&dc->ndev->dev, "  SD_WINDOW_POSITION: 0x%08x\n", val);

		val = SD_WIN_H_POSITION(settings->sd_window.h_size) |
			SD_WIN_V_POSITION(settings->sd_window.v_size);
		tegra_dc_writel(dc, val, DC_DISP_SD_WINDOW_SIZE);
		dev_dbg(&dc->ndev->dev, "  SD_WINDOW_SIZE: 0x%08x\n", val);
	}

	if (settings->soft_clipping_enable) {
		/* Write soft clipping */
		val = (64 * 1024) / (256 - settings->soft_clipping_threshold);
		val = SD_SOFT_CLIPPING_RECIP(val) |
		SD_SOFT_CLIPPING_THRESHOLD(settings->soft_clipping_threshold);
		tegra_dc_writel(dc, val, DC_DISP_SD_SOFT_CLIPPING);
		dev_dbg(&dc->ndev->dev, "  SOFT_CLIPPING: 0x%08x\n", val);
	}

	if (settings->smooth_k_enable) {
		/* Write K incr value */
		val = SD_SMOOTH_K_INCR(settings->smooth_k_incr);
		tegra_dc_writel(dc, val, DC_DISP_SD_SMOOTH_K);
		dev_dbg(&dc->ndev->dev, "  SMOOTH_K: 0x%08x\n", val);
	}
#endif

	/* Manage SD Control */
	val = 0;
	/* Stay in manual correction mode until the next flip. */
	val |= SD_CORRECTION_MODE_MAN;
	/* Enable / One-Shot */
	val |= (settings->enable == 2) ?
		(SD_ENABLE_ONESHOT | SD_ONESHOT_ENABLE) :
		SD_ENABLE_NORMAL;
	/* HW Update Delay */
	val |= SD_HW_UPDATE_DLY(settings->hw_update_delay);
	/* Video Luma */
	val |= (settings->use_vid_luma) ? SD_USE_VID_LUMA : 0;
	/* Aggressiveness */
	val |= SD_AGGRESSIVENESS(settings->aggressiveness);
	/* Bin Width (value derived above) */
	val |= bw;
#ifdef CONFIG_TEGRA_SD_GEN2
	/* K limit enable */
	val |= (settings->k_limit_enable) ? SD_K_LIMIT_ENABLE : 0;
	/* Programmable sd window enable */
	val |= (settings->sd_window_enable) ? SD_WINDOW_ENABLE : 0;
	/* Soft clipping enable */
	val |= (settings->soft_clipping_enable) ? SD_SOFT_CLIPPING_ENABLE : 0;
	/* Smooth K enable */
	val |= (settings->smooth_k_enable) ? SD_SMOOTH_K_ENABLE : 0;
	/* SD proc control */
	val |= (settings->use_vpulse2) ? SD_VPULSE2 : SD_VSYNC;
#endif
	/* Finally, Write SD Control */
	tegra_dc_writel(dc, val, DC_DISP_SD_CONTROL);
	dev_dbg(&dc->ndev->dev, "  SD_CONTROL: 0x%08x\n", val);
	tegra_dc_io_end(dc);

	/* set the brightness pointer */
	sd_brightness = settings->sd_brightness;

	/* note that we're in manual K until the next flip */
	atomic_set(&man_k_until_blank, 1);
}

static int bl_tf[17] = {
				57,  65,  73,  82,  92,
				103, 114, 125, 138, 150,
				164, 178, 193, 208, 224,
				241, 255,
			};

static int nvsd_backlght_interplate(u32 in, u32 base)
{
	int q, r;

	if (in <= base)
		return bl_tf[0];

	if (in > 255) {
		WARN(1, "PRISM gain is out of range!\n");
		return -1;
	}

	q = (in - base) / 8;
	r = (in - base) % 8;

	return (bl_tf[q] * (8 - r) + bl_tf[q + 1] * r) / 8;
}

/* Estimate the pixel gain of PRISM enhancement and soft-clipping algorithm*/
static u32 nvsd_softclip(fixed20_12 pixel, fixed20_12 k, fixed20_12 th)
{
	fixed20_12 num, f;

	if (pixel.full >= th.full) {
		num.full = pixel.full - th.full;
		f.full = dfixed_const(1) - dfixed_div(num, th);
	} else {
		f.full = dfixed_const(1);
	}

	num.full = dfixed_mul(pixel, f);
	f.full = dfixed_mul(num, k);
	num.full = pixel.full + f.full;

	return min_t(u32, num.full, dfixed_const(255));
}

static int nvsd_set_brightness(struct tegra_dc *dc)
{
	u32 bin_width;
	int i, j;
	int val;
	int pix;
	int bin_idx;

	int incr;
	int base;

	u32 histo[32];
	u32 histo_total = 0;		/* count of pixels */
	fixed20_12 nonhisto_gain;	/* gain of pixels not in histogram */
	fixed20_12 est_achieved_gain;	/* final gain of pixels */
	fixed20_12 histo_gain = dfixed_init(0);	/* gain of pixels */
	fixed20_12 k, threshold;
	fixed20_12 den, num, out;
	fixed20_12 pix_avg, pix_avg_softclip;

	/* Collet the inputs of the algorithm */
	for (i = 0; i < DC_DISP_SD_HISTOGRAM_NUM; i++) {
		val = tegra_dc_readl(dc, DC_DISP_SD_HISTOGRAM(i));
		for (j = 0; j < 4; j++)
			histo[i * 4 + j] = SD_HISTOGRAM_BIN(val, (j * 8));
	}

	val = tegra_dc_readl(dc, DC_DISP_SD_HW_K_VALUES);
	k.full = SD_HW_K_R(val) << 2;

	val = tegra_dc_readl(dc, DC_DISP_SD_SOFT_CLIPPING);
	threshold.full = dfixed_const(SD_SOFT_CLIPPING_THRESHOLD(val));

	val = tegra_dc_readl(dc, DC_DISP_SD_CONTROL);
	bin_width = SD_BIN_WIDTH(val)>>3;
	incr = 1 << bin_width;
	base = 256 - 32 * incr;

	for (pix = base, bin_idx = 0; pix < 256; pix += incr, bin_idx++) {
		num.full = dfixed_const(pix + pix + incr);
		den.full = dfixed_const(2);
		pix_avg.full = dfixed_div(num, den);
		pix_avg_softclip.full = nvsd_softclip(pix_avg, k, threshold);

		num.full = dfixed_const(histo[bin_idx]);
		den.full = dfixed_const(256);
		out.full = dfixed_div(num, den);
		num.full = dfixed_mul(out, pix_avg_softclip);
		out.full = dfixed_div(num, pix_avg);
		histo_gain.full += out.full;
		histo_total += histo[bin_idx];
	}

	out.full = dfixed_const(256 - histo_total);
	den.full = dfixed_const(1) + k.full;
	num.full = dfixed_mul(out, den);
	den.full = dfixed_const(256);
	nonhisto_gain.full = dfixed_div(num, den);

	den.full = nonhisto_gain.full + histo_gain.full;
	num.full = dfixed_const(1);
	out.full = dfixed_div(num, den);
	num.full = dfixed_const(255);
	est_achieved_gain.full = dfixed_mul(num, out);
	val = dfixed_trunc(est_achieved_gain);

	return nvsd_backlght_interplate(val, 128);
}

/* Periodic update */
bool nvsd_update_brightness(struct tegra_dc *dc)
{
	u32 val = 0;
	int cur_sd_brightness;
	int sw_sd_brightness;
	struct tegra_dc_sd_settings *settings = dc->out->sd_settings;

	if (sd_brightness) {
		if (atomic_read(&man_k_until_blank) &&
					!settings->phase_in_adjustments) {
			val = tegra_dc_readl(dc, DC_DISP_SD_CONTROL);
			val &= ~SD_CORRECTION_MODE_MAN;
			tegra_dc_writel(dc, val, DC_DISP_SD_CONTROL);
			atomic_set(&man_k_until_blank, 0);
		}

		if (settings->cmd)
			nvsd_cmd_handler(settings, dc);

		/* nvsd_cmd_handler may turn off didim */
		if (!settings->enable)
			return true;

		cur_sd_brightness = atomic_read(sd_brightness);

		/* read brightness value */
		val = tegra_dc_readl(dc, DC_DISP_SD_BL_CONTROL);
		val = SD_BLC_BRIGHTNESS(val);

		/* PRISM is updated by hw or sw algorithm. Brightness is
		 * compensated according to histogram for soft-clipping
		 * if hw output is used to update brightness. */
		if (settings->phase_in_adjustments) {
			return nvsd_phase_in_adjustments(dc, settings);
		} else if (settings->soft_clipping_correction) {
			sw_sd_brightness = nvsd_set_brightness(dc);
			if (sw_sd_brightness != cur_sd_brightness) {
				atomic_set(sd_brightness, sw_sd_brightness);
				return true;
			}
		} else if (val != (u32)cur_sd_brightness) {
			/* set brightness value and note the update */
			atomic_set(sd_brightness, (int)val);
			return true;
		}

	}

	/* No update needed. */
	return false;
}

static ssize_t nvsd_lut_show(struct tegra_dc_sd_settings *sd_settings,
	char *buf, ssize_t res)
{
	u32 i;
	u32 j;

	for (i = 0; i < NUM_BIN_WIDTHS; i++) {
		res += snprintf(buf + res, PAGE_SIZE - res,
			"Bin Width: %d\n", 1 << i);

		for (j = 0; j < DC_DISP_SD_LUT_NUM; j++) {
			res += snprintf(buf + res,
				PAGE_SIZE - res,
				"%d: R: %3d / G: %3d / B: %3d\n",
				j,
				sd_settings->lut[i][j].r,
				sd_settings->lut[i][j].g,
				sd_settings->lut[i][j].b);
		}
	}
	return res;
}

static ssize_t nvsd_bltf_show(struct tegra_dc_sd_settings *sd_settings,
	char *buf, ssize_t res)
{
	u32 i;
	u32 j;

	for (i = 0; i < NUM_BIN_WIDTHS; i++) {
		res += snprintf(buf + res, PAGE_SIZE - res,
			"Bin Width: %d\n", 1 << i);

		for (j = 0; j < DC_DISP_SD_BL_TF_NUM; j++) {
			res += snprintf(buf + res,
				PAGE_SIZE - res,
				"%d: 0: %3d / 1: %3d / 2: %3d / 3: %3d\n",
				j,
				sd_settings->bltf[i][j][0],
				sd_settings->bltf[i][j][1],
				sd_settings->bltf[i][j][2],
				sd_settings->bltf[i][j][3]);
		}
	}
	return res;
}

/* Sysfs accessors */
static ssize_t nvsd_settings_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	struct device *dev = container_of((kobj->parent), struct device, kobj);
	struct platform_device *ndev = to_platform_device(dev);
	struct tegra_dc *dc = platform_get_drvdata(ndev);
	struct tegra_dc_sd_settings *sd_settings = dc->out->sd_settings;
	ssize_t res = 0;

	if (sd_settings) {
		if (IS_NVSD_ATTR(enable))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->enable);
		else if (IS_NVSD_ATTR(aggressiveness))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->aggressiveness);
		else if (IS_NVSD_ATTR(phase_in_settings))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->phase_in_settings);
		else if (IS_NVSD_ATTR(phase_in_adjustments))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->phase_in_adjustments);
		else if (IS_NVSD_ATTR(bin_width))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->bin_width);
		else if (IS_NVSD_ATTR(hw_update_delay))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->hw_update_delay);
		else if (IS_NVSD_ATTR(use_vid_luma))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->use_vid_luma);
		else if (IS_NVSD_ATTR(coeff))
			res = snprintf(buf, PAGE_SIZE,
				"R: %d / G: %d / B: %d\n",
				sd_settings->coeff.r,
				sd_settings->coeff.g,
				sd_settings->coeff.b);
		else if (IS_NVSD_ATTR(blp_time_constant))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->blp.time_constant);
		else if (IS_NVSD_ATTR(blp_step))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->blp.step);
		else if (IS_NVSD_ATTR(fc_time_limit))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->fc.time_limit);
		else if (IS_NVSD_ATTR(fc_threshold))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->fc.threshold);
#ifdef CONFIG_TEGRA_SD_GEN2
		else if (IS_NVSD_ATTR(k_limit_enable))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->k_limit_enable);
		else if (IS_NVSD_ATTR(k_limit))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->k_limit);
		else if (IS_NVSD_ATTR(sd_window_enable))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->sd_window_enable);
		else if (IS_NVSD_ATTR(sd_window))
			res = snprintf(buf, PAGE_SIZE,
				"x: %d, y: %d, w: %d, h: %d\n",
				sd_settings->sd_window.h_position,
				sd_settings->sd_window.v_position,
				sd_settings->sd_window.h_size,
				sd_settings->sd_window.v_size);
		else if (IS_NVSD_ATTR(soft_clipping_enable))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->soft_clipping_enable);
		else if (IS_NVSD_ATTR(soft_clipping_threshold))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->soft_clipping_threshold);
		else if (IS_NVSD_ATTR(smooth_k_enable))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->smooth_k_enable);
		else if (IS_NVSD_ATTR(smooth_k_incr))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->smooth_k_incr);
		else if (IS_NVSD_ATTR(use_vpulse2))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->use_vpulse2);
#endif
		else if (IS_NVSD_ATTR(lut))
			res = nvsd_lut_show(sd_settings, buf, res);
		else if (IS_NVSD_ATTR(bltf))
			res = nvsd_bltf_show(sd_settings, buf, res);
		else
			res = -EINVAL;
	} else {
		/* This shouldn't be reachable. But just in case... */
		res = -EINVAL;
	}

	return res;
}

#define nvsd_check_and_update(_min, _max, _varname) { \
	int val = simple_strtol(buf, NULL, 10); \
	if (val >= _min && val <= _max) { \
		sd_settings->_varname = val; \
		settings_updated = true; \
	} }

#define nvsd_get_multi(_ele, _num, _act, _min, _max) { \
	char *b, *c, *orig_b; \
	b = orig_b = kstrdup(buf, GFP_KERNEL); \
	for (_act = 0; _act < _num; _act++) { \
		if (!b) \
			break; \
		b = strim(b); \
		c = strsep(&b, " "); \
		if (!strlen(c)) \
			break; \
		_ele[_act] = simple_strtol(c, NULL, 10); \
		if (_ele[_act] < _min || _ele[_act] > _max) \
			break; \
	} \
	if (orig_b) \
		kfree(orig_b); \
}

static int nvsd_lut_store(struct tegra_dc_sd_settings *sd_settings,
	const char *buf)
{
	int ele[3 * DC_DISP_SD_LUT_NUM * NUM_BIN_WIDTHS];
	int i = 0;
	int j = 0;
	int num = 3 * DC_DISP_SD_LUT_NUM * NUM_BIN_WIDTHS;

	nvsd_get_multi(ele, num, i, 0, 255);

	if (i != num)
		return -EINVAL;

	for (i = 0; i < NUM_BIN_WIDTHS; i++) {
		for (j = 0; j < DC_DISP_SD_LUT_NUM; j++) {
			sd_settings->lut[i][j].r =
				ele[i * NUM_BIN_WIDTHS + j * 3 + 0];
			sd_settings->lut[i][j].g =
				ele[i * NUM_BIN_WIDTHS + j * 3 + 1];
			sd_settings->lut[i][j].b =
				ele[i * NUM_BIN_WIDTHS + j * 3 + 2];
		}
	}
	return 0;
}

static int nvsd_bltf_store(struct tegra_dc_sd_settings *sd_settings,
	const char *buf)
{
	int ele[4 * DC_DISP_SD_BL_TF_NUM * NUM_BIN_WIDTHS];
	int i = 0, j = 0, num = ARRAY_SIZE(ele);

	nvsd_get_multi(ele, num, i, 0, 255);

	if (i != num)
		return -EINVAL;

	for (i = 0; i < NUM_BIN_WIDTHS; i++) {
		for (j = 0; j < DC_DISP_SD_BL_TF_NUM; j++) {
			size_t base = (i * NUM_BIN_WIDTHS *
				       DC_DISP_SD_BL_TF_NUM) + (j * 4);
			sd_settings->bltf[i][j][0] = ele[base + 0];
			sd_settings->bltf[i][j][1] = ele[base + 1];
			sd_settings->bltf[i][j][2] = ele[base + 2];
			sd_settings->bltf[i][j][3] = ele[base + 3];
		}
	}

	return 0;
}

static ssize_t nvsd_settings_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct device *dev = container_of((kobj->parent), struct device, kobj);
	struct platform_device *ndev = to_platform_device(dev);
	struct tegra_dc *dc = platform_get_drvdata(ndev);
	struct tegra_dc_sd_settings *sd_settings = dc->out->sd_settings;
	ssize_t res = count;
	bool settings_updated = false;
	long int result;
	int err;

	if (sd_settings) {
		if (IS_NVSD_ATTR(enable)) {
			if (sd_settings->phase_in_settings) {
				err = strict_strtol(buf, 10, &result);
				if (err)
					return err;

				if (nvsd_update_enable(sd_settings, result))
					nvsd_check_and_update(1, 1, enable);

			} else {
				nvsd_check_and_update(0, 1, enable);
			}
		} else if (IS_NVSD_ATTR(aggressiveness)) {
			err = strict_strtol(buf, 10, &result);
			if (err)
				return err;

			if (nvsd_update_agg(sd_settings, result)
					&& !sd_settings->phase_in_settings)
				settings_updated = true;

		} else if (IS_NVSD_ATTR(phase_in_settings)) {
			nvsd_check_and_update(0, 1, phase_in_settings);
		} else if (IS_NVSD_ATTR(phase_in_adjustments)) {
			nvsd_check_and_update(0, 1, phase_in_adjustments);
		} else if (IS_NVSD_ATTR(bin_width)) {
			nvsd_check_and_update(0, 8, bin_width);
		} else if (IS_NVSD_ATTR(hw_update_delay)) {
			nvsd_check_and_update(0, 2, hw_update_delay);
		} else if (IS_NVSD_ATTR(use_vid_luma)) {
			nvsd_check_and_update(0, 1, use_vid_luma);
		} else if (IS_NVSD_ATTR(coeff)) {
			int ele[3], i = 0, num = 3;
			nvsd_get_multi(ele, num, i, 0, 15);

			if (i == num) {
				sd_settings->coeff.r = ele[0];
				sd_settings->coeff.g = ele[1];
				sd_settings->coeff.b = ele[2];
				settings_updated = true;
			} else {
				res = -EINVAL;
			}
		} else if (IS_NVSD_ATTR(blp_time_constant)) {
			nvsd_check_and_update(0, 1024, blp.time_constant);
		} else if (IS_NVSD_ATTR(blp_step)) {
			nvsd_check_and_update(0, 255, blp.step);
		} else if (IS_NVSD_ATTR(fc_time_limit)) {
			nvsd_check_and_update(0, 255, fc.time_limit);
		} else if (IS_NVSD_ATTR(fc_threshold)) {
			nvsd_check_and_update(0, 255, fc.threshold);
#ifdef CONFIG_TEGRA_SD_GEN2
		} else if (IS_NVSD_ATTR(k_limit_enable)) {
			nvsd_check_and_update(0, 1, k_limit_enable);
		} else if (IS_NVSD_ATTR(k_limit)) {
			nvsd_check_and_update(128, 255, k_limit);
		} else if (IS_NVSD_ATTR(sd_window_enable)) {
			nvsd_check_and_update(0, 1, sd_window_enable);
		} else if (IS_NVSD_ATTR(sd_window)) {
			int ele[4], i = 0, num = 4;
			nvsd_get_multi(ele, num, i, 0, LONG_MAX);

			if (i == num) {
				sd_settings->sd_window.h_position = ele[0];
				sd_settings->sd_window.v_position = ele[1];
				sd_settings->sd_window.h_size = ele[2];
				sd_settings->sd_window.v_size = ele[3];
				settings_updated = true;
			} else {
				res = -EINVAL;
			}
		} else if (IS_NVSD_ATTR(soft_clipping_enable)) {
			nvsd_check_and_update(0, 1, soft_clipping_enable);
		} else if (IS_NVSD_ATTR(soft_clipping_threshold)) {
			nvsd_check_and_update(0, 255, soft_clipping_threshold);
		} else if (IS_NVSD_ATTR(smooth_k_enable)) {
			nvsd_check_and_update(0, 1, smooth_k_enable);
		} else if (IS_NVSD_ATTR(smooth_k_incr)) {
			nvsd_check_and_update(0, 16320, smooth_k_incr);
		} else if (IS_NVSD_ATTR(use_vpulse2)) {
			nvsd_check_and_update(0, 1, use_vpulse2);
#endif
		} else if (IS_NVSD_ATTR(lut)) {
			if (nvsd_lut_store(sd_settings, buf))
				res = -EINVAL;
			else
				settings_updated = true;
		} else if (IS_NVSD_ATTR(bltf)) {
			if (nvsd_bltf_store(sd_settings, buf))
				res = -EINVAL;
			else
				settings_updated = true;
		} else {
			res = -EINVAL;
		}

		/* Re-init if our settings were updated. */
		if (settings_updated) {
			mutex_lock(&dc->lock);
			if (!dc->enabled) {
				mutex_unlock(&dc->lock);
				return -ENODEV;
			}

			tegra_dc_io_start(dc);
			tegra_dc_hold_dc_out(dc);
			nvsd_init(dc, sd_settings);
			tegra_dc_release_dc_out(dc);
			tegra_dc_io_end(dc);

			mutex_unlock(&dc->lock);

			/* Update backlight state IFF we're disabling! */
			if (!sd_settings->enable && sd_settings->bl_device) {
				/* Do the actual brightness update outside of
				 * the mutex */
				struct backlight_device *bl =
					sd_settings->bl_device;

				if (bl)
					backlight_update_status(bl);
			}
		}
	} else {
		/* This shouldn't be reachable. But just in case... */
		res = -EINVAL;
	}

	return res;
}

#define NVSD_PRINT_REG(__name) { \
	u32 val = tegra_dc_readl(dc, __name); \
	res += snprintf(buf + res, PAGE_SIZE - res, #__name ": 0x%08x\n", \
		val); \
}

#define NVSD_PRINT_REG_ARRAY(__name) { \
	u32 val = 0, i = 0; \
	res += snprintf(buf + res, PAGE_SIZE - res, #__name ":\n"); \
	for (i = 0; i < __name##_NUM; i++) { \
		val = tegra_dc_readl(dc, __name(i)); \
		res += snprintf(buf + res, PAGE_SIZE - res, "  %d: 0x%08x\n", \
			i, val); \
	} \
}

static ssize_t nvsd_registers_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	struct device *dev = container_of((kobj->parent), struct device, kobj);
	struct platform_device *ndev = to_platform_device(dev);
	struct tegra_dc *dc = platform_get_drvdata(ndev);
	ssize_t res = 0;

	clk_enable(dc->clk);
	tegra_dc_io_start(dc);

	mutex_lock(&dc->lock);
	if (!dc->enabled) {
		mutex_unlock(&dc->lock);
		return -ENODEV;
	}

	mutex_unlock(&dc->lock);
	NVSD_PRINT_REG(DC_DISP_SD_CONTROL);
	NVSD_PRINT_REG(DC_DISP_SD_CSC_COEFF);
	NVSD_PRINT_REG_ARRAY(DC_DISP_SD_LUT);
	NVSD_PRINT_REG(DC_DISP_SD_FLICKER_CONTROL);
	NVSD_PRINT_REG(DC_DISP_SD_PIXEL_COUNT);
	NVSD_PRINT_REG_ARRAY(DC_DISP_SD_HISTOGRAM);
	NVSD_PRINT_REG(DC_DISP_SD_BL_PARAMETERS);
	NVSD_PRINT_REG_ARRAY(DC_DISP_SD_BL_TF);
	NVSD_PRINT_REG(DC_DISP_SD_BL_CONTROL);
	NVSD_PRINT_REG(DC_DISP_SD_HW_K_VALUES);
	NVSD_PRINT_REG(DC_DISP_SD_MAN_K_VALUES);
#ifdef CONFIG_TEGRA_SD_GEN2
	NVSD_PRINT_REG(DC_DISP_SD_K_LIMIT);
	NVSD_PRINT_REG(DC_DISP_SD_WINDOW_POSITION);
	NVSD_PRINT_REG(DC_DISP_SD_WINDOW_SIZE);
	NVSD_PRINT_REG(DC_DISP_SD_SOFT_CLIPPING);
	NVSD_PRINT_REG(DC_DISP_SD_SMOOTH_K);
#endif

	tegra_dc_io_end(dc);
	clk_disable(dc->clk);

	return res;
}

/* Sysfs initializer */
int nvsd_create_sysfs(struct device *dev)
{
	int retval = 0;

	nvsd_kobj = kobject_create_and_add("smartdimmer", &dev->kobj);

	if (!nvsd_kobj)
		return -ENOMEM;

	retval = sysfs_create_group(nvsd_kobj, &nvsd_attr_group);

	if (retval) {
		kobject_put(nvsd_kobj);
		dev_err(dev, "%s: failed to create attributes\n", __func__);
	}

	return retval;
}

/* Sysfs destructor */
void __devexit nvsd_remove_sysfs(struct device *dev)
{
	if (nvsd_kobj) {
		sysfs_remove_group(nvsd_kobj, &nvsd_attr_group);
		kobject_put(nvsd_kobj);
	}
}
