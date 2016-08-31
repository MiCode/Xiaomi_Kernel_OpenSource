/*
 * drivers/video/tegra/dc/of_dc.c
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/ktime.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/of_i2c.h>
#include <linux/nvhost.h>
#include <linux/timer.h>

#include <mach/clk.h>
#include <mach/dc.h>
#include <mach/fb.h>
#include <mach/mc.h>
#include <mach/latency_allowance.h>

#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
#include <mach/pinmux-t11.h>
#elif defined(CONFIG_ARCH_TEGRA_12x_SOC)
#include <mach/pinmux-t12.h>
#endif
#include "dc_reg.h"
#include "dc_config.h"
#include "dc_priv.h"
#include "dev.h"
#include "nvsd.h"
#include "dsi.h"

#ifdef CONFIG_OF
/* #define OF_DC_DEBUG	1 */

#undef OF_DC_LOG
#ifdef OF_DC_DEBUG
#define OF_DC_LOG(fmt, args...) pr_info("OF_DC_LOG: " fmt, ## args)
#else
#define OF_DC_LOG(fmt, args...)
#endif

#define DC_GEN_NODE		"/host1x/dc@"
#define DSI_NODE		"/host1x/dsi"
#define HDMI_NODE		"/host1x/hdmi"
#define STRING_ADDR(x)		#x
#define DC0_BASE_ADDR		STRING_ADDR(54200000)
#define DC1_BASE_ADDR		STRING_ADDR(54240000)
#define DEFAULT_OUT		"/dc-default-out"
#define DISP_TIMINGS		"/display-timings"
#define SMARTDIMMER		"/smartdimmer"
#define CMU			"/cmu"
#define FRAMEBUFFER_DATA	"/framebuffer-data"
#define OUT_TMDS_CFG		"/nvidia,out-tmds-cfg"

#define DC0_DEFAULT_OUT		\
	(DC_GEN_NODE DC0_BASE_ADDR DEFAULT_OUT)
#define DC1_DEFAULT_OUT		\
	(DC_GEN_NODE DC1_BASE_ADDR DEFAULT_OUT)

#define DC0_DISP_TIMINGS	\
	(DC_GEN_NODE DC0_BASE_ADDR DISP_TIMINGS)
#define DC1_DISP_TIMINGS	\
	(DC_GEN_NODE DC1_BASE_ADDR DISP_TIMINGS)

#define DC0_SMARTDIMMER		\
	(DC_GEN_NODE DC0_BASE_ADDR SMARTDIMMER)
#define DC1_SMARTDIMMER		\
	(DC_GEN_NODE DC1_BASE_ADDR SMARTDIMMER)

#define DC0_CMU			\
	(DC_GEN_NODE DC0_BASE_ADDR CMU)
#define DC1_CMU			\
	(DC_GEN_NODE DC1_BASE_ADDR CMU)

#define DC0_FRAMEBUFFER_DATA	\
	(DC_GEN_NODE DC0_BASE_ADDR FRAMEBUFFER_DATA)
#define DC1_FRAMEBUFFER_DATA	\
	(DC_GEN_NODE DC1_BASE_ADDR FRAMEBUFFER_DATA)

#define TMDS_CFG_NODE		\
	(HDMI_NODE OUT_TMDS_CFG)

static struct regulator *of_hdmi_vddio;
static struct regulator *of_hdmi_reg;
static struct regulator *of_hdmi_pll;

#ifdef CONFIG_TEGRA_DC_CMU
static struct tegra_dc_cmu default_cmu = {
	/* lut1 maps sRGB to linear space. */
	{
		0,    1,    2,    4,    5,    6,    7,    9,
		10,   11,   12,   14,   15,   16,   18,   20,
		21,   23,   25,   27,   29,   31,   33,   35,
		37,   40,   42,   45,   48,   50,   53,   56,
		59,   62,   66,   69,   72,   76,   79,   83,
		87,   91,   95,   99,   103,  107,  112,  116,
		121,  126,  131,  136,  141,  146,  151,  156,
		162,  168,  173,  179,  185,  191,  197,  204,
		210,  216,  223,  230,  237,  244,  251,  258,
		265,  273,  280,  288,  296,  304,  312,  320,
		329,  337,  346,  354,  363,  372,  381,  390,
		400,  409,  419,  428,  438,  448,  458,  469,
		479,  490,  500,  511,  522,  533,  544,  555,
		567,  578,  590,  602,  614,  626,  639,  651,
		664,  676,  689,  702,  715,  728,  742,  755,
		769,  783,  797,  811,  825,  840,  854,  869,
		884,  899,  914,  929,  945,  960,  976,  992,
		1008, 1024, 1041, 1057, 1074, 1091, 1108, 1125,
		1142, 1159, 1177, 1195, 1213, 1231, 1249, 1267,
		1286, 1304, 1323, 1342, 1361, 1381, 1400, 1420,
		1440, 1459, 1480, 1500, 1520, 1541, 1562, 1582,
		1603, 1625, 1646, 1668, 1689, 1711, 1733, 1755,
		1778, 1800, 1823, 1846, 1869, 1892, 1916, 1939,
		1963, 1987, 2011, 2035, 2059, 2084, 2109, 2133,
		2159, 2184, 2209, 2235, 2260, 2286, 2312, 2339,
		2365, 2392, 2419, 2446, 2473, 2500, 2527, 2555,
		2583, 2611, 2639, 2668, 2696, 2725, 2754, 2783,
		2812, 2841, 2871, 2901, 2931, 2961, 2991, 3022,
		3052, 3083, 3114, 3146, 3177, 3209, 3240, 3272,
		3304, 3337, 3369, 3402, 3435, 3468, 3501, 3535,
		3568, 3602, 3636, 3670, 3705, 3739, 3774, 3809,
		3844, 3879, 3915, 3950, 3986, 4022, 4059, 4095,
	},
	/* csc */
	{
		0,
	},
	/*lut2*/
	{
		0,
	}
};
#endif
#endif

#ifdef CONFIG_OF
static int parse_dc_out_type(struct device_node *np,
		struct tegra_dc_out *default_out)
{
	const char *temp_str0;
	if (!of_property_read_string(np, "nvidia,out-type", &temp_str0)) {
		if (!strncmp(temp_str0, "dsi", strlen(temp_str0))) {
			default_out->type = TEGRA_DC_OUT_DSI;
			OF_DC_LOG("dsi out\n");
		} else if (!strncmp(temp_str0, "hdmi", strlen(temp_str0))) {
			default_out->type = TEGRA_DC_OUT_HDMI;
			OF_DC_LOG("hdmi out\n");
		} else {
			pr_err("no dc out support except dsi / hdmi\n");
			return -EINVAL;
		}
	}
	return 0;
}

static int parse_tmds(struct device_node *np,
	u8 *addr)
{
	u32 temp;
	struct tmds_config *tmds_cfg_addr;
	tmds_cfg_addr = (struct tmds_config *)addr;

	if (!of_property_read_u32(np, "pclk", &temp)) {
		tmds_cfg_addr->pclk = (int)temp;
		OF_DC_LOG("tmds pclk %d\n", temp);
	} else {
		goto parse_tmds_fail;
	}
	if (!of_property_read_u32(np, "pll0", &temp)) {
		tmds_cfg_addr->pll0 = (u32)temp;
		OF_DC_LOG("tmds pll0 %d\n", temp);
	} else {
		goto parse_tmds_fail;
	}
	if (!of_property_read_u32(np, "pll1", &temp)) {
		tmds_cfg_addr->pll1 = (u32)temp;
		OF_DC_LOG("tmds pll1 %d\n", temp);
	} else {
		goto parse_tmds_fail;
	}
	if (!of_property_read_u32(np, "pe-current", &temp)) {
		tmds_cfg_addr->pe_current = (u32)temp;
		OF_DC_LOG("tmds pe-current %d\n", temp);
	} else {
		goto parse_tmds_fail;
	}
	if (!of_property_read_u32(np, "drive-current", &temp)) {
		tmds_cfg_addr->drive_current = (u32)temp;
		OF_DC_LOG("tmds drive-current %d\n", temp);
	} else {
		goto parse_tmds_fail;
	}
	if (!of_property_read_u32(np, "peak-current", &temp)) {
		tmds_cfg_addr->peak_current = (u32)temp;
		OF_DC_LOG("tmds peak-current %d\n", temp);
	} else {
		goto parse_tmds_fail;
	}
	return 0;
parse_tmds_fail:
	pr_err("parse tmds fail!\n");
	return -EINVAL;
}

static int parse_dc_default_out(struct platform_device *ndev,
		struct device_node *np, struct tegra_dc_out *default_out)
{
	int err;
	u32 temp;
	const char *temp_str0;
	int hotplug_gpio = 0;
	enum of_gpio_flags flags;
	struct device_node *ddc;
	struct device_node *np_hdmi =
		of_find_node_by_path(HDMI_NODE);
	struct device_node *tmds_np = NULL;
	struct device_node *entry = NULL;
	u8 *addr;

	err = parse_dc_out_type(np, default_out);
	if (err) {
		pr_err("parse_dc_out_type err\n");
		return err;
	}
	if (!of_property_read_u32(np, "nvidia,out-width", &temp)) {
		default_out->width = (unsigned) temp;
		OF_DC_LOG("out_width %d\n", default_out->width);
	}
	if (!of_property_read_u32(np, "nvidia,out-height", &temp)) {
		default_out->height = (unsigned) temp;
		OF_DC_LOG("out_height %d\n", default_out->height);
	}
	if (np_hdmi && of_device_is_available(np_hdmi) &&
		(default_out->type == TEGRA_DC_OUT_HDMI)) {
		int id;
		ddc = of_parse_phandle(np_hdmi, "nvidia,ddc-i2c-bus", 0);

		if (!ddc) {
			pr_err("No ddc device node\n");
			return -EINVAL;
		} else
			id = of_alias_get_id(ddc, "i2c");

		if (id >= 0) {
			default_out->dcc_bus = id;
			OF_DC_LOG("out_dcc bus %d\n", id);
		} else {
			pr_err("Invalid i2c id\n");
			return -EINVAL;
		}

		hotplug_gpio = of_get_named_gpio_flags(np_hdmi,
				"nvidia,hpd-gpio", 0, &flags);
		if (hotplug_gpio != 0)
			default_out->hotplug_gpio = hotplug_gpio;
	}
	if (!of_property_read_u32(np, "nvidia,out-max-pixclk", &temp)) {
		default_out->max_pixclock = (unsigned) KHZ2PICOS(temp);
		OF_DC_LOG("khz %d => out_dcc %d in picos unit\n",
			temp, default_out->max_pixclock);
	}
	if (!of_property_read_string(np, "nvidia,out-flags", &temp_str0)) {
		if (!strncmp(temp_str0, "continuous", strlen(temp_str0))) {
			default_out->flags = TEGRA_DC_OUT_CONTINUOUS_MODE;
		} else if (!strncmp(temp_str0, "oneshot", strlen(temp_str0))) {
			default_out->flags = TEGRA_DC_OUT_ONE_SHOT_MODE;
		} else if (!strncmp(temp_str0, "continuous_initialized",
			strlen(temp_str0))) {
			default_out->flags = TEGRA_DC_OUT_CONTINUOUS_MODE |
				TEGRA_DC_OUT_INITIALIZED_MODE;
		} else if (!strncmp(temp_str0, "oneshot_initialized",
			strlen(temp_str0))) {
			default_out->flags = TEGRA_DC_OUT_ONE_SHOT_MODE |
				TEGRA_DC_OUT_INITIALIZED_MODE;
		} else if (!strncmp(temp_str0,
			"hotplug_high", strlen(temp_str0))) {
			default_out->flags = TEGRA_DC_OUT_HOTPLUG_HIGH;
		} else if (!strncmp(temp_str0,
			"hotplug_low", strlen(temp_str0))) {
			default_out->flags = TEGRA_DC_OUT_HOTPLUG_LOW;
		} else if (!strncmp(temp_str0,
			"hotplug_high_wake_lp0", strlen(temp_str0))) {
			default_out->flags = TEGRA_DC_OUT_HOTPLUG_HIGH |
				TEGRA_DC_OUT_HOTPLUG_WAKE_LP0;
		} else if (!strncmp(temp_str0,
			"hotplug_low_wake_lp0", strlen(temp_str0))) {
			default_out->flags = TEGRA_DC_OUT_HOTPLUG_LOW |
				TEGRA_DC_OUT_HOTPLUG_WAKE_LP0;
		} else {
			pr_err("invalid out flags\n");
			return -EINVAL;
		}
	}
	if (!of_property_read_string(np, "nvidia,out-align", &temp_str0)) {
		if (!strncmp(temp_str0, "msb", strlen(temp_str0))) {
			default_out->align = TEGRA_DC_ALIGN_MSB;
		} else if (!strncmp(temp_str0, "lsb", strlen(temp_str0))) {
			default_out->align = TEGRA_DC_ALIGN_LSB;
		} else {
			pr_err("invalid out align\n");
			return -EINVAL;
		}
	}
	if (!of_property_read_string(np, "nvidia,out-order", &temp_str0)) {
		if (!strncmp(temp_str0, "rtob", strlen(temp_str0))) {
			default_out->order = TEGRA_DC_ORDER_RED_BLUE;
		} else if (!strncmp(temp_str0, "btor", strlen(temp_str0))) {
			default_out->order = TEGRA_DC_ORDER_BLUE_RED;
		} else {
			pr_err("invalid out order\n");
			return -EINVAL;
		}
	}

	if (!of_property_read_string(np, "nvidia,out-parent-clk", &temp_str0)) {
		default_out->parent_clk = temp_str0;
		OF_DC_LOG("parent clk %s\n",
			default_out->parent_clk);
	} else {
		goto fail_dc_default_out;
	}
	if (default_out->type == TEGRA_DC_OUT_HDMI)
		tmds_np = of_find_node_by_path(TMDS_CFG_NODE);

	if (!tmds_np) {
		pr_info("%s: No nvidia,out-tmds-cfg\n",
			__func__);
	} else {
		int tmds_set_count =
			of_get_child_count(tmds_np);
		if (!tmds_set_count) {
			pr_info("tmds node exists but no cfg!\n");
			goto success_dc_default_out;
		}

		default_out->hdmi_out = devm_kzalloc(&ndev->dev,
			sizeof(struct tegra_hdmi_out), GFP_KERNEL);
		if (!default_out->hdmi_out) {
			dev_err(&ndev->dev, "not enough memory\n");
			return -ENOMEM;
		}
		default_out->hdmi_out->n_tmds_config =
			tmds_set_count;

		default_out->hdmi_out->tmds_config = devm_kzalloc(&ndev->dev,
			tmds_set_count * sizeof(struct tmds_config),
			GFP_KERNEL);
		if (!default_out->hdmi_out->tmds_config) {
			dev_err(&ndev->dev, "not enough memory\n");
			return -ENOMEM;
		}
		addr = (u8 *)default_out->hdmi_out->tmds_config;
		for_each_child_of_node(tmds_np, entry) {
			err = parse_tmds(entry, addr);
			if (err)
				goto fail_dc_default_out;
			addr += sizeof(struct tmds_config);
		}
	}
	if (default_out->type == TEGRA_DC_OUT_HDMI) {
		default_out->depth = 0;
#ifdef CONFIG_FRAMEBUFFER_CONSOLE
		if (!of_property_read_u32(np,
			"nvidia,out-depth", &temp)) {
			default_out->depth = (unsigned) temp;
			OF_DC_LOG("out-depth for HDMI FB console %d\n", temp);
		}
#endif
	} else {
		/* default_out->type == TEGRA_DC_OUT_DSI */
		if (!of_property_read_u32(np,
			"nvidia,out-depth", &temp)) {
			default_out->depth = (unsigned) temp;
			OF_DC_LOG("out-depth for DSI display %d\n", temp);
		}
	}
success_dc_default_out:
	return 0;

fail_dc_default_out:
	pr_err("%s: a parse error\n", __func__);
	return -EINVAL;
}

static int parse_sd_settings(struct device_node *np,
	struct tegra_dc_sd_settings *sd_settings)
{
	struct property *prop;
	const __be32 *p;
	u32 u;
	const char *sd_str1;
	u8 coeff[3] = {0, };
	u8 fc[2] = {0, };
	u32 blp[2] = {0, };

	int coeff_count = 0;
	int fc_count = 0;
	int blp_count = 0;
	int bltf_count = 0;
	u8 *addr;
	int sd_lut[108] = {0, };
	int sd_i = 0;
	int  sd_j = 0;
	int sd_index = 0;
	u32 temp;

	if (of_device_is_available(np))
		sd_settings->enable = (unsigned) 1;
	else
		sd_settings->enable = (unsigned) 0;

	OF_DC_LOG("nvidia,sd-enable %d\n", sd_settings->enable);

	if (!of_property_read_u32(np, "nvidia,use-auto-pwm", &temp)) {
		sd_settings->use_auto_pwm = (bool) temp;
		OF_DC_LOG("nvidia,use-auto-pwm %d\n", temp);
	}
	if (!of_property_read_u32(np, "nvidia,hw-update-delay", &temp)) {
		sd_settings->hw_update_delay = (u8) temp;
		OF_DC_LOG("nvidia,hw-update-delay %d\n", temp);
	}
	if (!of_property_read_u32(np, "nvidia,bin-width", &temp)) {
		s32 s32_val;
		s32_val = (s32)temp;
		sd_settings->bin_width = (short)s32_val;
		OF_DC_LOG("nvidia,bin-width %d\n", s32_val);
	}
	if (!of_property_read_u32(np, "nvidia,aggressiveness", &temp)) {
		sd_settings->aggressiveness = (u8) temp;
		OF_DC_LOG("nvidia,aggressiveness %d\n", temp);
	}
	if (!of_property_read_u32(np, "nvidia,use-vid-luma", &temp)) {
		sd_settings->use_vid_luma = (bool) temp;
		OF_DC_LOG("nvidia,use-vid-luma %d\n", temp);
	}
	if (!of_property_read_u32(np, "nvidia,phase-in-settings", &temp)) {
		sd_settings->phase_in_settings = (u8) temp;
		OF_DC_LOG("nvidia,phase-in-settings  %d\n", temp);
	}
	if (!of_property_read_u32(np,
		"nvidia,phase-in-adjustments", &temp)) {
		sd_settings->phase_in_adjustments = (u8) temp;
		OF_DC_LOG("nvidia,phase-in-adjustments  %d\n", temp);
	}
	if (!of_property_read_u32(np, "nvidia,k-limit-enable", &temp)) {
		sd_settings->k_limit_enable = (bool) temp;
		OF_DC_LOG("nvidia,k-limit-enable  %d\n", temp);
	}
	if (!of_property_read_u32(np, "nvidia,k-limit", &temp)) {
		sd_settings->k_limit = (u16) temp;
		OF_DC_LOG("nvidia,k-limit  %d\n", temp);
	}
	if (!of_property_read_u32(np, "nvidia,sd-window-enable", &temp)) {
		sd_settings->sd_window_enable = (bool) temp;
		OF_DC_LOG("nvidia,sd-window-enable  %d\n", temp);
	}
	if (!of_property_read_u32(np,
		"nvidia,soft-clipping-enable", &temp)) {
		sd_settings->soft_clipping_enable = (bool) temp;
		OF_DC_LOG("nvidia,soft-clipping-enable %d\n", temp);
	}
	if (!of_property_read_u32(np,
		"nvidia,soft-clipping-threshold", &temp)) {
		sd_settings->soft_clipping_threshold = (u8) temp;
		OF_DC_LOG("nvidia,soft-clipping-threshold %d\n", temp);
	}
	if (!of_property_read_u32(np, "nvidia,smooth-k-enable", &temp)) {
		sd_settings->smooth_k_enable = (bool) temp;
		OF_DC_LOG("nvidia,smooth-k-enable %d\n", temp);
	}
	if (!of_property_read_u32(np, "nvidia,smooth-k-incr", &temp)) {
		sd_settings->smooth_k_incr = (u16) temp;
		OF_DC_LOG("nvidia,smooth-k-incr %d\n", temp);
	}

	sd_settings->sd_brightness = &sd_brightness;

	if (!of_property_read_u32(np, "nvidia,use-vpulse2", &temp)) {
		sd_settings->use_vpulse2 = (bool) temp;
		OF_DC_LOG("nvidia,use-vpulse2 %d\n", temp);
	}

	if (!of_property_read_string(np, "nvidia,bl-device-name",
		&sd_str1)) {
		sd_settings->bl_device_name = (char *)sd_str1;
		OF_DC_LOG("nvidia,bl-device-name %s\n", sd_str1);
	}

	coeff_count = 0;
	of_property_for_each_u32(np, "nvidia,coeff", prop, p, u)
		coeff_count++;

	if (coeff_count > (sizeof(coeff) / sizeof(coeff[0]))) {
		pr_err("sd_coeff overflow\n");
		return -EINVAL;
	} else {
		coeff_count = 0;
		of_property_for_each_u32(np, "nvidia,coeff", prop, p, u)
			coeff[coeff_count++] = (u8)u;
		sd_settings->coeff.r = coeff[0];
		sd_settings->coeff.g = coeff[1];
		sd_settings->coeff.b = coeff[2];
		OF_DC_LOG("nvidia,coeff %d %d %d\n",
				coeff[0], coeff[1], coeff[2]);
	}
	fc_count = 0;
	of_property_for_each_u32(np, "nvidia,fc", prop, p, u)
		fc_count++;

	if (fc_count > sizeof(fc) / sizeof(fc[0])) {
		pr_err("sd fc overflow\n");
		return -EINVAL;
	} else {
		fc_count = 0;
		of_property_for_each_u32(np, "nvidia,fc", prop, p, u)
		fc[fc_count++] = (u8)u;

		sd_settings->fc.time_limit = fc[0];
		sd_settings->fc.threshold = fc[1];
		OF_DC_LOG("nvidia,fc %d %d\n", fc[0], fc[1]);
	}

	blp_count = 0;
	of_property_for_each_u32(np, "nvidia,blp", prop, p, u)
		blp_count++;

	if (blp_count > sizeof(blp) / sizeof(blp[0])) {
		pr_err("sd blp overflow\n");
		return -EINVAL;
	} else {
		blp_count = 0;
		of_property_for_each_u32(np, "nvidia,blp", prop, p, u)
			blp[blp_count++] = (u32)u;
		sd_settings->blp.time_constant = (u16)blp[0];
		sd_settings->blp.step = (u8)blp[1];
		OF_DC_LOG("nvidia,blp %d %d\n", blp[0], blp[1]);
	}

	bltf_count = 0;
	of_property_for_each_u32(np, "nvidia,bltf", prop, p, u)
		bltf_count++;

	if (bltf_count > (sizeof(sd_settings->bltf) /
			sizeof(sd_settings->bltf[0][0][0]))) {
		pr_err("sd bltf overflow of sd_settings\n");
		return -EINVAL;
	} else {
		addr = &(sd_settings->bltf[0][0][0]);
		of_property_for_each_u32(np, "nvidia,bltf", prop, p, u)
			*(addr++) = u;
	}

	sd_index = 0;
	of_property_for_each_u32(np, "nvidia,lut", prop, p, u)
		sd_index++;

	if (sd_index > sizeof(sd_lut)/sizeof(sd_lut[0])) {
		pr_err("sd lut size overflow of sd_settings\n");
		return -EINVAL;
	} else {
		sd_index = 0;
		of_property_for_each_u32(np, "nvidia,lut", prop, p, u)
			sd_lut[sd_index++] = u;

		sd_index = 0;

		if (prop) {
			for (sd_i = 0; sd_i < 4; sd_i++)
				for (sd_j = 0; sd_j < 9; sd_j++) {
					sd_settings->lut[sd_i][sd_j].r =
						sd_lut[sd_index++];
					sd_settings->lut[sd_i][sd_j].g =
						sd_lut[sd_index++];
					sd_settings->lut[sd_i][sd_j].b =
						sd_lut[sd_index++];
			}
		}
	}
	return 0;
}

static int parse_modes(struct device_node *np,
	struct tegra_dc_mode *modes)
{
	u32 temp;

	if (!of_property_read_u32(np, "clock-frequency", &temp)) {
		modes->pclk = temp;
		OF_DC_LOG("of pclk %d\n", temp);
	} else {
		goto parse_modes_fail;
	}
	if (!of_property_read_u32(np, "nvidia,h-ref-to-sync", &temp)) {
		modes->h_ref_to_sync = temp;
	} else {
		OF_DC_LOG("of h_ref_to_sync %d\n", temp);
		goto parse_modes_fail;
	}
	if (!of_property_read_u32(np, "nvidia,v-ref-to-sync", &temp)) {
		modes->v_ref_to_sync = temp;
	} else {
		OF_DC_LOG("of v_ref_to_sync %d\n", temp);
		goto parse_modes_fail;
	}
	if (!of_property_read_u32(np, "hsync-len", &temp)) {
		modes->h_sync_width = temp;
	} else {
		OF_DC_LOG("of h_sync_width %d\n", temp);
		goto parse_modes_fail;
	}
	if (!of_property_read_u32(np, "vsync-len", &temp)) {
		modes->v_sync_width = temp;
	} else {
		OF_DC_LOG("of v_sync_width %d\n", temp);
		goto parse_modes_fail;
	}
	if (!of_property_read_u32(np, "hback-porch", &temp)) {
		modes->h_back_porch = temp;
	} else {
		OF_DC_LOG("of h_back_porch %d\n", temp);
		goto parse_modes_fail;
	}
	if (!of_property_read_u32(np, "vback-porch", &temp)) {
		modes->v_back_porch = temp;
	} else {
		OF_DC_LOG("of v_back_porch %d\n", temp);
		goto parse_modes_fail;
	}
	if (!of_property_read_u32(np, "hactive", &temp)) {
		modes->h_active = temp;
	} else {
		OF_DC_LOG("of h_active %d\n", temp);
		goto parse_modes_fail;
	}
	if (!of_property_read_u32(np, "vactive", &temp)) {
		modes->v_active = temp;
	} else {
		OF_DC_LOG("of v_active %d\n", temp);
		goto parse_modes_fail;
	}
	if (!of_property_read_u32(np, "hfront-porch", &temp)) {
		modes->h_front_porch = temp;
	} else {
		OF_DC_LOG("of h_front_porch %d\n", temp);
		goto parse_modes_fail;
	}
	if (!of_property_read_u32(np, "vfront-porch", &temp)) {
		modes->v_front_porch = temp;
	} else {
		OF_DC_LOG("of v_front_porch %d\n", temp);
		goto parse_modes_fail;
	}
	return 0;
parse_modes_fail:
	pr_err("a mode parameter parse fail!\n");
	return -EINVAL;
}

#ifdef CONFIG_TEGRA_DC_CMU
static int parse_cmu_data(struct device_node *np,
	struct tegra_dc_cmu *cmu)
{
	u16 *csc_parse;
	u8 *addr_cmu_lut2;
	struct property *prop;
	const __be32 *p;
	u32 u;
	int csc_count = 0;
	int lut2_count = 0;

	memcpy(cmu, &default_cmu, sizeof(struct tegra_dc_cmu));

	csc_parse = &(cmu->csc.krr);
	addr_cmu_lut2 = &(cmu->lut2[0]);

	of_property_for_each_u32(np, "nvidia,cmu-csc", prop, p, u)
		csc_count++;
	if (csc_count >
		(sizeof(cmu->csc) / sizeof(cmu->csc.krr))) {
		pr_err("cmu csc overflow\n");
		return -EINVAL;
	} else {
		of_property_for_each_u32(np,
			"nvidia,cmu-csc", prop, p, u) {
			OF_DC_LOG("cmu csc 0x%x\n", u);
			*(csc_parse++) = (u16)u;
		}
	}

	of_property_for_each_u32(np, "nvidia,cmu-lut2", prop, p, u)
		lut2_count++;

	if (lut2_count >
		(sizeof(cmu->lut2) / sizeof(cmu->lut2[0]))) {
		pr_err("cmu lut2 overflow\n");
		return -EINVAL;
	} else {
		of_property_for_each_u32(np, "nvidia,cmu-lut2",
			prop, p, u) {
			/* OF_DC_LOG("cmu lut2 0x%x\n", u); */
			*(addr_cmu_lut2++) = (u8)u;
		}
	}
	return 0;
}
#endif

static int parse_fb_info(struct device_node *np, struct tegra_fb_data *fb)
{
	u32 temp;
	const char *temp_str0;

	/*
	 * set fb->win to 0 in default
	 */
	fb->win = 0;

	if (!of_property_read_u32(np, "nvidia,fb-bpp", &temp)) {
		fb->bits_per_pixel = (int)temp;
		OF_DC_LOG("fb bpp %d\n", fb->bits_per_pixel);
	} else {
		goto fail_fb_info;
	}
	if (!of_property_read_string(np, "nvidia,fb-flags", &temp_str0)) {
		if (!strncmp(temp_str0, "flip_on_probe", strlen(temp_str0))) {
			fb->flags = TEGRA_FB_FLIP_ON_PROBE;
			OF_DC_LOG("fb flip on probe\n");
		} else {
			pr_err("invalid fb_flags\n");
			return -EINVAL;
		}
	}
	if (!of_property_read_u32(np, "nvidia,fb-xres", &temp)) {
		fb->xres = (int)temp;
		OF_DC_LOG("fb xres %d\n", fb->xres);
	} else {
		goto fail_fb_info;
	}
	if (!of_property_read_u32(np, "nvidia,fb-yres", &temp)) {
		fb->yres = (int)temp;
		OF_DC_LOG("fb yres %d\n", fb->yres);
	} else {
		goto fail_fb_info;
	}
	return 0;

fail_fb_info:
	pr_err("%s: a parse error\n", __func__);
	return -EINVAL;
}

struct tegra_dsi_cmd *tegra_dsi_parse_cmd_dt(struct platform_device *ndev,
					const struct device_node *node,
					struct property *prop,
					u32 n_cmd)
{
	struct tegra_dsi_cmd *dsi_cmd, *temp;
	u32 *prop_val_ptr;
	u32 cnt = 0, i = 0;
	u8 arg1, arg2;

	if (!n_cmd)
		return NULL;

	if (!prop)
		return NULL;
	prop_val_ptr = prop->value;

	dsi_cmd = devm_kzalloc(&ndev->dev, sizeof(*dsi_cmd) * n_cmd,
				GFP_KERNEL);
	if (!dsi_cmd) {
		pr_err("dsi: cmd memory allocation failed\n");
		return ERR_PTR(-ENOMEM);
	}
	temp = dsi_cmd;

	for (cnt  = 0; cnt < n_cmd; cnt++, temp++) {
		temp->cmd_type = be32_to_cpu(*prop_val_ptr++);
		if (temp->cmd_type == TEGRA_DSI_PACKET_CMD) {
			temp->data_id = be32_to_cpu(*prop_val_ptr++);
			arg1 = be32_to_cpu(*prop_val_ptr++);
			arg2 = be32_to_cpu(*prop_val_ptr++);
			prop_val_ptr++; /* skip ecc */
			if (temp->data_id == DSI_GENERIC_LONG_WRITE ||
				temp->data_id == DSI_DCS_LONG_WRITE ||
				temp->data_id == DSI_NULL_PKT_NO_DATA ||
				temp->data_id == DSI_BLANKING_PKT_NO_DATA) {
				/* long pkt */
				temp->sp_len_dly.data_len =
					(arg2 << NUMOF_BIT_PER_BYTE) | arg1;
				temp->pdata = devm_kzalloc(&ndev->dev,
					temp->sp_len_dly.data_len, GFP_KERNEL);
				for (i = 0; i < temp->sp_len_dly.data_len; i++)
					(temp->pdata)[i] =
					be32_to_cpu(*prop_val_ptr++);
				prop_val_ptr += 2; /* skip checksum */
			} else {
				temp->sp_len_dly.sp.data0 = arg1;
				temp->sp_len_dly.sp.data1 = arg2;
			}
		} else if (temp->cmd_type == TEGRA_DSI_DELAY_MS) {
			temp->sp_len_dly.delay_ms =
				be32_to_cpu(*prop_val_ptr++);
		}
	}

	return dsi_cmd;
}

static const u32 *tegra_dsi_parse_pkt_seq_dt(struct platform_device *ndev,
						struct device_node *node,
						struct property *prop)
{
	u32 *prop_val_ptr;
	u32 *pkt_seq;
	int line, i;

#define LINE_STOP 0xff

	if (!prop)
		return NULL;

	pkt_seq = devm_kzalloc(&ndev->dev,
				sizeof(u32) * NUMOF_PKT_SEQ, GFP_KERNEL);
	if (!pkt_seq) {
		dev_err(&ndev->dev,
			"dsi: pkt seq memory allocation failed\n");
		return ERR_PTR(-ENOMEM);
	}
	prop_val_ptr = prop->value;
	for (line = 0; line < NUMOF_PKT_SEQ; line += 2) {
		/* compute line value from dt line */
		for (i = 0;; i += 2) {
			u32 cmd = be32_to_cpu(*prop_val_ptr++);
			if (cmd == LINE_STOP)
				break;
			else if (cmd == PKT_LP)
				pkt_seq[line] |= PKT_LP;
			else {
				u32 len = be32_to_cpu(*prop_val_ptr++);
				if (i == 0) /* PKT_ID0 */
					pkt_seq[line] |=
						PKT_ID0(cmd) | PKT_LEN0(len);
				if (i == 2) /* PKT_ID1 */
					pkt_seq[line] |=
						PKT_ID1(cmd) | PKT_LEN1(len);
				if (i == 4) /* PKT_ID2 */
					pkt_seq[line] |=
						PKT_ID2(cmd) | PKT_LEN2(len);
				if (i == 6) /* PKT_ID3 */
					pkt_seq[line + 1] |=
						PKT_ID3(cmd) | PKT_LEN3(len);
				if (i == 8) /* PKT_ID4 */
					pkt_seq[line + 1] |=
						PKT_ID4(cmd) | PKT_LEN4(len);
				if (i == 10) /* PKT_ID5 */
					pkt_seq[line + 1] |=
						PKT_ID5(cmd) | PKT_LEN5(len);
			}
		}
	}

#undef LINE_STOP

	return pkt_seq;
}

int parse_dsi_settings(struct platform_device *ndev,
	struct device_node *np_dsi, struct tegra_dc_platform_data *pdata)
{
	u32 temp;
	int err = 0;
	int dsi_te_gpio = 0;
	struct device_node *np_panel;
	struct tegra_dsi_out *dsi = pdata->default_out->dsi;

	np_panel = tegra_panel_get_dt_node(pdata);

	if (!np_panel) {
		pr_err("There is no valid panel node\n");
		return -EINVAL;
	}

	if (!of_property_read_u32(np_dsi, "nvidia,dsi-controller-vs", &temp)) {
		dsi->controller_vs = (u8)temp;
		if (temp == DSI_VS_0)
			OF_DC_LOG("dsi controller vs DSI_VS_0\n");
		else if (temp == DSI_VS_1)
			OF_DC_LOG("dsi controller vs DSI_VS_1\n");
		else {
			pr_err("invalid dsi controller version\n");
			return -EINVAL;
		}
	}

	if (!of_property_read_u32(np_panel,
			"nvidia,dsi-n-data-lanes", &temp)) {
		dsi->n_data_lanes = (u8)temp;
		OF_DC_LOG("n data lanes %d\n", dsi->n_data_lanes);
	}
	if (!of_property_read_u32(np_panel,
			"nvidia,dsi-video-burst-mode", &temp)) {
		dsi->video_burst_mode = (u8)temp;
		if (temp == TEGRA_DSI_VIDEO_NONE_BURST_MODE)
			OF_DC_LOG("dsi video NON_BURST_MODE\n");
		else if (temp == TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END)
			OF_DC_LOG("dsi video NONE_BURST_MODE_WITH_SYNC_END\n");
		else if (temp == TEGRA_DSI_VIDEO_BURST_MODE_LOWEST_SPEED)
			OF_DC_LOG("dsi video BURST_MODE_LOWEST_SPEED\n");
		else if (temp == TEGRA_DSI_VIDEO_BURST_MODE_LOW_SPEED)
			OF_DC_LOG("dsi video BURST_MODE_LOW_SPEED\n");
		else if (temp == TEGRA_DSI_VIDEO_BURST_MODE_MEDIUM_SPEED)
			OF_DC_LOG("dsi video BURST_MODE_MEDIUM_SPEED\n");
		else if (temp == TEGRA_DSI_VIDEO_BURST_MODE_FAST_SPEED)
			OF_DC_LOG("dsi video BURST_MODE_FAST_SPEED\n");
		else if (temp == TEGRA_DSI_VIDEO_BURST_MODE_FASTEST_SPEED)
			OF_DC_LOG("dsi video BURST_MODE_FASTEST_SPEED\n");
		else {
			pr_err("invalid dsi video burst mode\n");
			return -EINVAL;
		}
	}
	if (!of_property_read_u32(np_panel,
			"nvidia,dsi-pixel-format", &temp)) {
		dsi->pixel_format = (u8)temp;
		if (temp == TEGRA_DSI_PIXEL_FORMAT_16BIT_P)
			OF_DC_LOG("dsi pixel format 16BIT_P\n");
		else if (temp == TEGRA_DSI_PIXEL_FORMAT_18BIT_P)
			OF_DC_LOG("dsi pixel format 18BIT_P\n");
		else if (temp == TEGRA_DSI_PIXEL_FORMAT_18BIT_NP)
			OF_DC_LOG("dsi pixel format 18BIT_NP\n");
		else if (temp == TEGRA_DSI_PIXEL_FORMAT_24BIT_P)
			OF_DC_LOG("dsi pixel format 24BIT_P\n");
		else {
			pr_err("invalid dsi pixel format\n");
			return -EINVAL;
		}
	}
	if (!of_property_read_u32(np_panel,
			"nvidia,dsi-refresh-rate", &temp)) {
		dsi->refresh_rate = (u8)temp;
		OF_DC_LOG("dsi refresh rate %d\n", dsi->refresh_rate);
	}
	if (!of_property_read_u32(np_panel,
			"nvidia,dsi-virtual-channel", &temp)) {
		dsi->virtual_channel = (u8)temp;
		if (temp == TEGRA_DSI_VIRTUAL_CHANNEL_0)
			OF_DC_LOG("dsi virtual channel 0\n");
		else if (temp == TEGRA_DSI_VIRTUAL_CHANNEL_1)
			OF_DC_LOG("dsi virtual channel 1\n");
		else if (temp == TEGRA_DSI_VIRTUAL_CHANNEL_2)
			OF_DC_LOG("dsi virtual channel 2\n");
		else if (temp == TEGRA_DSI_VIRTUAL_CHANNEL_3)
			OF_DC_LOG("dsi virtual channel 3\n");
		else {
			pr_err("invalid dsi virtual ch\n");
			return -EINVAL;
		}
	}
	if (!of_property_read_u32(np_panel, "nvidia,dsi-instance", &temp)) {
		dsi->dsi_instance = (u8)temp;
		if (temp == DSI_INSTANCE_0)
			OF_DC_LOG("dsi instance 0\n");
		else if (temp == DSI_INSTANCE_1)
			OF_DC_LOG("dsi instance 1\n");
		else {
			pr_err("invalid dsi instance\n");
			return -EINVAL;
		}
	}
	if (!of_property_read_u32(np_panel, "nvidia,dsi-panel-reset", &temp)) {
		dsi->panel_reset = (u8)temp;
		OF_DC_LOG("dsi panel reset %d\n", dsi->panel_reset);
	}

	if (!of_property_read_u32(np_panel, "nvidia,dsi-ganged-type", &temp)) {
		dsi->ganged_type = (u8)temp;
		OF_DC_LOG("dsi ganged_type %d\n", dsi->ganged_type);
	}

	dsi_te_gpio = of_get_named_gpio(np_panel, "nvidia,dsi-te-gpio", 0);
	if (gpio_is_valid(dsi_te_gpio)) {
		dsi->te_gpio = dsi_te_gpio;
		OF_DC_LOG("dsi te_gpio %d\n", dsi_te_gpio);
	}

	if (!of_property_read_u32(np_panel,
		"nvidia,dsi-power-saving-suspend", &temp)) {
		dsi->power_saving_suspend = (bool)temp;
		OF_DC_LOG("dsi power saving suspend %d\n",
			dsi->power_saving_suspend);
	}
	if (!of_property_read_u32(np_panel,
		"nvidia,dsi-video-data-type", &temp)) {
		dsi->video_data_type = (u8)temp;
		if (temp == TEGRA_DSI_VIDEO_TYPE_VIDEO_MODE)
			OF_DC_LOG("dsi video type VIDEO_MODE\n");
		else if (temp == TEGRA_DSI_VIDEO_TYPE_COMMAND_MODE)
			OF_DC_LOG("dsi video type COMMAND_MODE\n");
		else {
			pr_err("invalid dsi video data type\n");
			return -EINVAL;
		}
	}
	if (!of_property_read_u32(np_panel,
		"nvidia,dsi-video-clock-mode", &temp)) {
		dsi->video_clock_mode = (u8)temp;
		if (temp == TEGRA_DSI_VIDEO_CLOCK_CONTINUOUS)
			OF_DC_LOG("dsi video clock mode CONTINUOUS\n");
		else if (temp == TEGRA_DSI_VIDEO_CLOCK_TX_ONLY)
			OF_DC_LOG("dsi video clock mode TX_ONLY\n");
		else {
			pr_err("invalid dsi video clk mode\n");
			return -EINVAL;
		}
	}
	if (!of_property_read_u32(np_panel, "nvidia,dsi-n-init-cmd", &temp)) {
		dsi->n_init_cmd = (u16)temp;
		OF_DC_LOG("dsi n_init_cmd %d\n",
			dsi->n_init_cmd);
	}
	dsi->dsi_init_cmd =
		tegra_dsi_parse_cmd_dt(ndev, np_panel,
			of_find_property(np_panel,
			"nvidia,dsi-init-cmd", NULL),
			dsi->n_init_cmd);
	if (dsi->n_init_cmd &&
		IS_ERR_OR_NULL(dsi->dsi_init_cmd)) {
		dev_err(&ndev->dev,
			"dsi: copy init cmd from dt failed\n");
		err = PTR_ERR(dsi->dsi_init_cmd);
		return err;
	};

	if (!of_property_read_u32(np_panel,
			"nvidia,dsi-n-suspend-cmd", &temp)) {
		dsi->n_suspend_cmd = (u16)temp;
		OF_DC_LOG("dsi n_suspend_cmd %d\n",
			dsi->n_suspend_cmd);
	}
	dsi->dsi_suspend_cmd =
		tegra_dsi_parse_cmd_dt(ndev, np_panel,
			of_find_property(np_panel,
			"nvidia,dsi-suspend-cmd", NULL),
			dsi->n_suspend_cmd);
	if (dsi->n_suspend_cmd &&
		IS_ERR_OR_NULL(dsi->dsi_suspend_cmd)) {
		dev_err(&ndev->dev,
			"dsi: copy suspend cmd from dt failed\n");
		err = PTR_ERR(dsi->dsi_suspend_cmd);
		return err;
	};

	if (!of_property_read_u32(np_panel,
		"nvidia,dsi-n-early-suspend-cmd", &temp)) {
		dsi->n_early_suspend_cmd = (u16)temp;
		OF_DC_LOG("dsi n_early_suspend_cmd %d\n",
			dsi->n_early_suspend_cmd);
	}
	dsi->dsi_early_suspend_cmd =
		tegra_dsi_parse_cmd_dt(ndev, np_panel,
			of_find_property(np_panel,
			"nvidia,dsi-early-suspend-cmd", NULL),
			dsi->n_early_suspend_cmd);
	if (dsi->n_early_suspend_cmd &&
		IS_ERR_OR_NULL(dsi->dsi_early_suspend_cmd)) {
		dev_err(&ndev->dev,
			"dsi: copy early suspend cmd from dt failed\n");
		err = PTR_ERR(dsi->dsi_early_suspend_cmd);
		return err;
	};

	if (!of_property_read_u32(np_panel,
		"nvidia,dsi-n-late-resume-cmd", &temp)) {
		dsi->n_late_resume_cmd = (u16)temp;
		OF_DC_LOG("dsi n_late_resume_cmd %d\n",
			dsi->n_late_resume_cmd);
	}
	dsi->dsi_late_resume_cmd =
		tegra_dsi_parse_cmd_dt(ndev, np_panel,
			of_find_property(np_panel,
			"nvidia,dsi-late-resume-cmd", NULL),
			dsi->n_late_resume_cmd);
	if (dsi->n_late_resume_cmd &&
		IS_ERR_OR_NULL(dsi->dsi_late_resume_cmd)) {
		dev_err(&ndev->dev,
			"dsi: copy late resume cmd from dt failed\n");
		err = PTR_ERR(dsi->dsi_late_resume_cmd);
		return err;
	};

	dsi->pkt_seq =
		tegra_dsi_parse_pkt_seq_dt(ndev, np_panel,
			of_find_property(np_panel,
			"nvidia,dsi-pkt-seq", NULL));
	if (IS_ERR(dsi->pkt_seq)) {
		dev_err(&ndev->dev,
			"dsi pkt seq from dt fail\n");
		return PTR_ERR(dsi->pkt_seq);
	}

	if (!of_property_read_u32(np_panel, "nvidia,dsi-phy-hsdexit", &temp)) {
		dsi->phy_timing.t_hsdexit_ns = (u16)temp;
		OF_DC_LOG("phy t_hsdexit_ns %d\n",
			dsi->phy_timing.t_hsdexit_ns);
	}

	if (!of_property_read_u32(np_panel, "nvidia,dsi-phy-hstrail", &temp)) {
		dsi->phy_timing.t_hstrail_ns = (u16)temp;
		OF_DC_LOG("phy t_hstrail_ns %d\n",
			dsi->phy_timing.t_hstrail_ns);
	}

	if (!of_property_read_u32(np_panel, "nvidia,dsi-phy-datzero", &temp)) {
		dsi->phy_timing.t_datzero_ns = (u16)temp;
		OF_DC_LOG("phy t_datzero_ns %d\n",
			dsi->phy_timing.t_datzero_ns);
	}

	if (!of_property_read_u32(np_panel,
			"nvidia,dsi-phy-hsprepare", &temp)) {
		dsi->phy_timing.t_hsprepare_ns = (u16)temp;
		OF_DC_LOG("phy t_hsprepare_ns %d\n",
			dsi->phy_timing.t_hsprepare_ns);
	}

	if (!of_property_read_u32(np_panel,
			"nvidia,dsi-phy-clktrail", &temp)) {
		dsi->phy_timing.t_clktrail_ns = (u16)temp;
		OF_DC_LOG("phy t_clktrail_ns %d\n",
			dsi->phy_timing.t_clktrail_ns);
	}

	if (!of_property_read_u32(np_panel, "nvidia,dsi-phy-clkpost", &temp)) {
		dsi->phy_timing.t_clkpost_ns = (u16)temp;
		OF_DC_LOG("phy t_clkpost_ns %d\n",
			dsi->phy_timing.t_clkpost_ns);
	}

	if (!of_property_read_u32(np_panel, "nvidia,dsi-phy-clkzero", &temp)) {
		dsi->phy_timing.t_clkzero_ns = (u16)temp;
		OF_DC_LOG("phy t_clkzero_ns %d\n",
			dsi->phy_timing.t_clkzero_ns);
	}

	if (!of_property_read_u32(np_panel, "nvidia,dsi-phy-tlpx", &temp)) {
		dsi->phy_timing.t_tlpx_ns = (u16)temp;
		OF_DC_LOG("phy t_tlpx_ns %d\n",
			dsi->phy_timing.t_tlpx_ns);
	}

	if (!of_property_read_u32(np_panel,
			"nvidia,dsi-phy-clkprepare", &temp)) {
		dsi->phy_timing.t_clkprepare_ns = (u16)temp;
		OF_DC_LOG("phy t_clkprepare_ns %d\n",
			dsi->phy_timing.t_clkprepare_ns);
	}

	if (!of_property_read_u32(np_panel, "nvidia,dsi-phy-clkpre", &temp)) {
		dsi->phy_timing.t_clkpre_ns = (u16)temp;
		OF_DC_LOG("phy t_clkpre_ns %d\n",
			dsi->phy_timing.t_clkpre_ns);
	}

	if (!of_property_read_u32(np_panel, "nvidia,dsi-phy-wakeup", &temp)) {
		dsi->phy_timing.t_wakeup_ns = (u16)temp;
		OF_DC_LOG("phy t_wakeup_ns %d\n",
			dsi->phy_timing.t_wakeup_ns);
	}

	if (!of_property_read_u32(np_panel, "nvidia,dsi-phy-taget", &temp)) {
		dsi->phy_timing.t_taget_ns = (u16)temp;
		OF_DC_LOG("phy t_taget_ns %d\n",
			dsi->phy_timing.t_taget_ns);
	}

	if (!of_property_read_u32(np_panel, "nvidia,dsi-phy-tasure", &temp)) {
		dsi->phy_timing.t_tasure_ns = (u16)temp;
		OF_DC_LOG("phy t_tasure_ns %d\n",
			dsi->phy_timing.t_tasure_ns);
	}

	if (!of_property_read_u32(np_panel, "nvidia,dsi-phy-tago", &temp)) {
		dsi->phy_timing.t_tago_ns = (u16)temp;
		OF_DC_LOG("phy t_tago_ns %d\n",
			dsi->phy_timing.t_tago_ns);
	}

	return 0;
}

static int dc_hdmi_out_enable(struct device *dev)
{
	int ret;

	struct device_node *np_hdmi =
		of_find_node_by_path(HDMI_NODE);

	if (!np_hdmi || !of_device_is_available(np_hdmi)) {
		pr_info("%s: no valid hdmi node\n", __func__);
		return 0;
	}

	if (!of_hdmi_reg) {
		of_hdmi_reg = regulator_get(dev, "avdd_hdmi");
		if (IS_ERR_OR_NULL(of_hdmi_reg)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi\n");
			of_hdmi_reg = NULL;
			return PTR_ERR(of_hdmi_reg);
		}
	}
	ret = regulator_enable(of_hdmi_reg);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi\n");
		return ret;
	}
	if (!of_hdmi_pll) {
		of_hdmi_pll = regulator_get(dev, "avdd_hdmi_pll");
		if (IS_ERR_OR_NULL(of_hdmi_pll)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi_pll\n");
			of_hdmi_pll = NULL;
			regulator_put(of_hdmi_reg);
			of_hdmi_reg = NULL;
			return PTR_ERR(of_hdmi_pll);
		}
	}
	ret = regulator_enable(of_hdmi_pll);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi_pll\n");
		return ret;
	}
	return 0;
}

static int dc_hdmi_out_disable(void)
{
	if (of_hdmi_reg) {
		regulator_disable(of_hdmi_reg);
		regulator_put(of_hdmi_reg);
		of_hdmi_reg = NULL;
	}

	if (of_hdmi_pll) {
		regulator_disable(of_hdmi_pll);
		regulator_put(of_hdmi_pll);
		of_hdmi_pll = NULL;
	}

	return 0;
}

static int dc_hdmi_hotplug_init(struct device *dev)
{
	int ret = 0;

	struct device_node *np_hdmi =
		of_find_node_by_path(HDMI_NODE);

	if (!np_hdmi || !of_device_is_available(np_hdmi)) {
		pr_info("%s: no valid hdmi node\n", __func__);
		return 0;
	}

	if (!of_hdmi_vddio) {
		of_hdmi_vddio = regulator_get(dev, "vdd_hdmi_5v0");
		if (IS_ERR_OR_NULL(of_hdmi_vddio)) {
			ret = PTR_ERR(of_hdmi_vddio);
			pr_err("hdmi: couldn't get regulator vdd_hdmi_5v0\n");
			of_hdmi_vddio = NULL;
			return ret;
		}
	}
	ret = regulator_enable(of_hdmi_vddio);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator vdd_hdmi_5v0\n");
		regulator_put(of_hdmi_vddio);
		of_hdmi_vddio = NULL;
		return ret;
	}
	return ret;
}

static int dc_hdmi_postsuspend(void)
{
	if (of_hdmi_vddio) {
		regulator_disable(of_hdmi_vddio);
		regulator_put(of_hdmi_vddio);
		of_hdmi_vddio = NULL;
	}
	return 0;
}

#if defined(CONFIG_ARCH_TEGRA_11x_SOC) ||	\
	defined(CONFIG_ARCH_TEGRA_12x_SOC)
static void dc_hdmi_hotplug_report(bool state)
{
	if (state) {
		tegra_pinmux_set_pullupdown(TEGRA_PINGROUP_DDC_SDA,
						TEGRA_PUPD_PULL_DOWN);
		tegra_pinmux_set_pullupdown(TEGRA_PINGROUP_DDC_SCL,
						TEGRA_PUPD_PULL_DOWN);
	} else {
		tegra_pinmux_set_pullupdown(TEGRA_PINGROUP_DDC_SDA,
						TEGRA_PUPD_NORMAL);
		tegra_pinmux_set_pullupdown(TEGRA_PINGROUP_DDC_SCL,
						TEGRA_PUPD_NORMAL);
	}
}
#endif

struct tegra_dc_platform_data
		*of_dc_parse_platform_data(struct platform_device *ndev)
{
	struct tegra_dc_platform_data *pdata;
	struct device_node *np = ndev->dev.of_node;
	struct device_node *np_dsi = NULL;
	struct device_node *timings_np = NULL;
	struct device_node *fb_np = NULL;
	struct device_node *sd_np = NULL;
	struct device_node *default_out_np = NULL;
	struct device_node *entry = NULL;
#ifdef CONFIG_TEGRA_DC_CMU
	struct device_node *cmu_np = NULL;
#endif
	const char *temp_str0;
	int err;
	u32 temp;

	/*
	 * Memory for pdata, pdata->default_out, pdata->fb
	 * need to be allocated in default
	 * since it is expected data for these needs to be
	 * parsed from DTB.
	 */
	pdata = devm_kzalloc(&ndev->dev,
		sizeof(struct tegra_dc_platform_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(&ndev->dev, "not enough memory\n");
		goto fail_parse;
	}

	pdata->default_out = devm_kzalloc(&ndev->dev,
		sizeof(struct tegra_dc_out), GFP_KERNEL);
	if (!pdata->default_out) {
		dev_err(&ndev->dev, "not enough memory\n");
		goto fail_parse;
	}

	pdata->fb = devm_kzalloc(&ndev->dev,
		sizeof(struct tegra_fb_data), GFP_KERNEL);
	if (!pdata->fb) {
		dev_err(&ndev->dev, "not enough memory\n");
		goto fail_parse;
	}

	/*
	 * determine dc out type
	 */
	default_out_np = of_find_node_by_name(np, "dc-default-out");
	if (!default_out_np) {
		pr_err("%s: could not find dc-default-out node\n",
			__func__);
		goto fail_parse;
	} else {
		err = parse_dc_default_out(ndev, default_out_np,
			pdata->default_out);
		if (err)
			goto fail_parse;
	}

#ifndef CONFIG_TEGRA_HDMI_PRIMARY
	if (pdata->default_out->type == TEGRA_DC_OUT_DSI)
		timings_np = of_find_node_by_path(DC0_DISP_TIMINGS);
	else
		timings_np = of_find_node_by_path(DC1_DISP_TIMINGS);
#else
		timings_np = of_find_node_by_path(DC0_DISP_TIMINGS);
#endif
	if (!timings_np) {
		if (pdata->default_out->type == TEGRA_DC_OUT_DSI) {
			pr_err("%s: could not find display-timings node\n",
				__func__);
			goto fail_parse;
		}
	} else if (pdata->default_out->type == TEGRA_DC_OUT_DSI) {
		/* pdata->default_out->type == TEGRA_DC_OUT_DSI */
		pdata->default_out->n_modes =
			of_get_child_count(timings_np);
		if (pdata->default_out->n_modes == 0) {
			/*
			 * Should never happen !
			 */
			dev_err(&ndev->dev, "no timing given\n");
			goto fail_parse;
		}
		pdata->default_out->modes = devm_kzalloc(&ndev->dev,
			pdata->default_out->n_modes *
			sizeof(struct tegra_dc_mode), GFP_KERNEL);
		if (!pdata->default_out->modes) {
			dev_err(&ndev->dev, "not enough memory\n");
			goto fail_parse;
		}
	} else {
		/* pdata->default_out->type == TEGRA_DC_OUT_HDMI */
		pdata->default_out->n_modes = 0;
#ifdef CONFIG_FRAMEBUFFER_CONSOLE
		pdata->default_out->n_modes =
			of_get_child_count(timings_np);
		if (pdata->default_out->n_modes == 0) {
			/*
			 * Should never happen !
			 */
			dev_err(&ndev->dev, "no timing given\n");
			goto fail_parse;
		} else {
			pdata->default_out->modes = devm_kzalloc(&ndev->dev,
				pdata->default_out->n_modes *
				sizeof(struct tegra_dc_mode), GFP_KERNEL);
			if (!pdata->default_out->modes) {
				dev_err(&ndev->dev, "not enough memory\n");
				goto fail_parse;
			}
		}
#endif
	}
#ifndef CONFIG_TEGRA_HDMI_PRIMARY
	if (pdata->default_out->type == TEGRA_DC_OUT_DSI)
		sd_np = of_find_node_by_path(DC0_SMARTDIMMER);
	else
		sd_np = of_find_node_by_path(DC1_SMARTDIMMER);
#else
		sd_np = of_find_node_by_path(DC0_SMARTDIMMER);
#endif
	if (!sd_np) {
		pr_info("%s: could not find SD settings node\n",
			__func__);
	} else {
		if (of_device_is_available(sd_np)) {
			pdata->default_out->sd_settings =
				devm_kzalloc(&ndev->dev,
				sizeof(struct tegra_dc_sd_settings),
				GFP_KERNEL);
			if (!pdata->default_out->sd_settings) {
				dev_err(&ndev->dev, "not enough memory\n");
				goto fail_parse;
			}
		} else {
			dev_err(&ndev->dev, "sd_settings: No data in node\n");
			goto fail_parse;
		}
	}

#ifdef CONFIG_TEGRA_DC_CMU
#ifndef CONFIG_TEGRA_HDMI_PRIMARY
	if (pdata->default_out->type == TEGRA_DC_OUT_DSI)
		cmu_np = of_find_node_by_path(DC0_CMU);
	else
		cmu_np = of_find_node_by_path(DC1_CMU);
#else
		cmu_np = of_find_node_by_path(DC0_CMU);
#endif
	if (!cmu_np) {
		pr_info("%s: could not find cmu node\n",
			__func__);
	} else {
		if (of_device_is_available(cmu_np)) {
			pdata->cmu = devm_kzalloc(&ndev->dev,
				sizeof(struct tegra_dc_cmu), GFP_KERNEL);
			if (!pdata->cmu) {
				dev_err(&ndev->dev, "not enough memory\n");
				goto fail_parse;
			}
		} else {
			dev_err(&ndev->dev, "cmu: No data in node\n");
			goto fail_parse;
		}
	}
#endif

	if (pdata->default_out->type == TEGRA_DC_OUT_DSI) {
		np_dsi = of_find_node_by_path(DSI_NODE);

		if (!np_dsi) {
			pr_err("%s: could not find dsi node\n", __func__);
			goto fail_parse;
		} else if (of_device_is_available(np_dsi)) {
			pdata->default_out->dsi = devm_kzalloc(&ndev->dev,
				sizeof(struct tegra_dsi_out), GFP_KERNEL);
			if (!pdata->default_out->dsi) {
				dev_err(&ndev->dev, "not enough memory\n");
				goto fail_parse;
			}
		}
	} else if (pdata->default_out->type == TEGRA_DC_OUT_HDMI) {
		bool hotplug_report = false;
		struct device_node *np_hdmi =
			of_find_node_by_path(HDMI_NODE);

		if (np_hdmi && of_device_is_available(np_hdmi)) {
				if (!of_property_read_u32(np_hdmi,
					"nvidia,hotplug-report", &temp)) {
					hotplug_report = (bool)temp;
				}
		}

		pdata->default_out->enable = dc_hdmi_out_enable;
		pdata->default_out->disable = dc_hdmi_out_disable;
		pdata->default_out->hotplug_init = dc_hdmi_hotplug_init;
		pdata->default_out->postsuspend = dc_hdmi_postsuspend;
#if defined(CONFIG_ARCH_TEGRA_11x_SOC) ||	\
	defined(CONFIG_ARCH_TEGRA_12x_SOC)
		if (hotplug_report)
			pdata->default_out->hotplug_report =
				dc_hdmi_hotplug_report;
#endif
	}

	/*
	 * parse sd_settings values
	 */
	if (pdata->default_out->sd_settings != NULL) {
		err = parse_sd_settings(sd_np, pdata->default_out->sd_settings);
		if (err)
			goto fail_parse;
	}

	if (pdata->default_out->modes != NULL) {
		struct tegra_dc_mode *cur_mode
			= pdata->default_out->modes;
		for_each_child_of_node(timings_np, entry) {
			err = parse_modes(entry, cur_mode);
			if (err)
				goto fail_parse;
			cur_mode++;
		}
	}

#ifdef CONFIG_TEGRA_DC_CMU
	if (pdata->cmu != NULL) {
		err = parse_cmu_data(cmu_np, pdata->cmu);
		if (err)
			goto fail_parse;
	}
#endif

	if (pdata->default_out->dsi) {
		/* It happens in case of TEGRA_DC_OUT_DSI only */
		err = parse_dsi_settings(ndev, np_dsi, pdata);
		if (err)
			goto fail_parse;
	}

#ifndef CONFIG_TEGRA_HDMI_PRIMARY
	if (pdata->default_out->type == TEGRA_DC_OUT_DSI)
		fb_np = of_find_node_by_path(DC0_FRAMEBUFFER_DATA);
	else
		fb_np = of_find_node_by_path(DC1_FRAMEBUFFER_DATA);
#else
		fb_np = of_find_node_by_path(DC0_FRAMEBUFFER_DATA);
#endif
	if (!fb_np) {
		pr_err("%s: err, No framebuffer-data\n",
			__func__);
		goto fail_parse;
	} else {
		err = parse_fb_info(fb_np, pdata->fb);
		if (err)
			goto fail_parse;
	}

	if (!of_property_read_string(np, "nvidia,dc-flags", &temp_str0)) {
		if (!strncmp(temp_str0, "dc_flag_en", strlen(temp_str0))) {
			pdata->flags = TEGRA_DC_FLAG_ENABLED;
			OF_DC_LOG("dc flag en\n");
		}
	}
	if (!of_property_read_u32(np, "nvidia,emc-clk-rate", &temp)) {
		pdata->emc_clk_rate = (unsigned long)temp;
		OF_DC_LOG("emc clk rate %lu\n", pdata->emc_clk_rate);
	}
#ifdef CONFIG_TEGRA_DC_CMU
	if (!of_property_read_u32(np, "nvidia,cmu-enable", &temp)) {
		pdata->cmu_enable = (bool)temp;
		OF_DC_LOG("cmu enable %d\n", pdata->cmu_enable);
	} else {
		pdata->cmu_enable = false;
	}
#endif
	if (!of_property_read_u32(np, "nvidia,low-v-win", &temp)) {
		pdata->low_v_win = (unsigned long)temp;
		OF_DC_LOG("low_v_win %lu\n", pdata->low_v_win);
	}
	return pdata;

fail_parse:
	return NULL;
}
#else
struct tegra_dc_platform_data
		*of_dc_parse_platform_data(struct platform_device *ndev)
{
	return NULL;
}
#endif
