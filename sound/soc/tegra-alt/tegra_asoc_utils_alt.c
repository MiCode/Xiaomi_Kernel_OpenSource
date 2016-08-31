/*
 * tegra_asoc_utils_alt.c - MCLK and DAP Utility driver
 *
 * Author: Stephen Warren <swarren@nvidia.com>
 * Copyright (c) 2010-2013 NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>

#include <mach/clk.h>
#include <mach/pinmux.h>
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
#include <mach/pinmux-tegra20.h>
#endif
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
#include <mach/pinmux-tegra30.h>
#endif
#ifdef CONFIG_ARCH_TEGRA_11x_SOC
#include <mach/pinmux-t11.h>
#endif
#ifdef CONFIG_ARCH_TEGRA_12x_SOC
#include <mach/pinmux-t12.h>
#endif
#ifdef CONFIG_ARCH_TEGRA_14x_SOC
#include <mach/pinmux-t14.h>
#endif

#include <sound/soc.h>

#include "tegra_asoc_utils_alt.h"

static atomic_t dap_ref_count[5];

#define TRISTATE_DAP_PORT(n) \
static void tristate_dap_##n(bool tristate) \
{ \
	enum tegra_pingroup fs, sclk, din, dout; \
	fs = TEGRA_PINGROUP_DAP##n##_FS; \
	sclk = TEGRA_PINGROUP_DAP##n##_SCLK; \
	din = TEGRA_PINGROUP_DAP##n##_DIN; \
	dout = TEGRA_PINGROUP_DAP##n##_DOUT; \
	if (tristate) { \
		if (atomic_dec_return(&dap_ref_count[n-1]) == 0) {\
			tegra_pinmux_set_tristate(fs, TEGRA_TRI_TRISTATE); \
			tegra_pinmux_set_tristate(sclk, TEGRA_TRI_TRISTATE); \
			tegra_pinmux_set_tristate(din, TEGRA_TRI_TRISTATE); \
			tegra_pinmux_set_tristate(dout, TEGRA_TRI_TRISTATE); \
		} \
	} else { \
		if (atomic_inc_return(&dap_ref_count[n-1]) == 1) {\
			tegra_pinmux_set_tristate(fs, TEGRA_TRI_NORMAL); \
			tegra_pinmux_set_tristate(sclk, TEGRA_TRI_NORMAL); \
			tegra_pinmux_set_tristate(din, TEGRA_TRI_NORMAL); \
			tegra_pinmux_set_tristate(dout, TEGRA_TRI_NORMAL); \
		} \
	} \
}

TRISTATE_DAP_PORT(1)
TRISTATE_DAP_PORT(2)
/*I2S2 and I2S3 for other chips do not map to DAP3 and DAP4 (also
these pinmux dont exist for other chips), they map to some
other pinmux*/
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)\
		|| defined(CONFIG_ARCH_TEGRA_12x_SOC)\
		|| defined(CONFIG_ARCH_TEGRA_3x_SOC)
	TRISTATE_DAP_PORT(3)
	TRISTATE_DAP_PORT(4)
#endif

int tegra_alt_asoc_utils_tristate_dap(int id, bool tristate)
{
	switch (id) {
	case 0:
		tristate_dap_1(tristate);
		break;
	case 1:
		tristate_dap_2(tristate);
		break;
/*I2S2 and I2S3 for other chips do not map to DAP3 and DAP4 (also
these pinmux dont exist for other chips), they map to some
other pinmux*/
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)\
	|| defined(CONFIG_ARCH_TEGRA_12x_SOC)\
	|| defined(CONFIG_ARCH_TEGRA_3x_SOC)
	case 2:
		tristate_dap_3(tristate);
		break;
	case 3:
		tristate_dap_4(tristate);
		break;
#endif
	default:
		pr_warn("Invalid DAP port\n");
		break;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(tegra_alt_asoc_utils_tristate_dap);

int tegra_alt_asoc_utils_set_rate(struct tegra_asoc_audio_clock_info *data,
				int srate,
				int mclk,
				int clk_out_rate)
{
	int new_baseclock;
	bool clk_change;
	int err;

	switch (srate) {
	case 11025:
	case 22050:
	case 44100:
	case 88200:
		if (data->soc == TEGRA_ASOC_UTILS_SOC_TEGRA20)
			new_baseclock = 56448000;
		else if (data->soc == TEGRA_ASOC_UTILS_SOC_TEGRA30)
			new_baseclock = 564480000;
		else
			new_baseclock = 282240000;
		break;
	case 8000:
	case 16000:
	case 32000:
	case 48000:
	case 64000:
	case 96000:
		if (data->soc == TEGRA_ASOC_UTILS_SOC_TEGRA20)
			new_baseclock = 73728000;
		else if (data->soc == TEGRA_ASOC_UTILS_SOC_TEGRA30)
			new_baseclock = 552960000;
		else
			new_baseclock = 368640000;
		break;
	default:
		return -EINVAL;
	}

	clk_change = ((new_baseclock != data->set_baseclock) ||
			(mclk != data->set_mclk));
	if (!clk_change)
		return 0;

	/* Don't change rate if already one dai-link is using it */
	if (data->lock_count)
		return -EINVAL;

	data->set_baseclock = 0;
	data->set_mclk = 0;

	if (data->clk_pll_a_state) {
		clk_disable_unprepare(data->clk_pll_a);
		data->clk_pll_a_state = 0;
	}

	if (data->clk_pll_a_out0_state) {
		clk_disable_unprepare(data->clk_pll_a_out0);
		data->clk_pll_a_out0_state = 0;
	}

	if (data->clk_cdev1_state) {
		clk_disable_unprepare(data->clk_cdev1);
		data->clk_cdev1_state = 0;
	}

	err = clk_set_rate(data->clk_pll_a, new_baseclock);
	if (err) {
		dev_err(data->dev, "Can't set pll_a rate: %d\n", err);
		return err;
	}

	err = clk_set_rate(data->clk_pll_a_out0, mclk);
	if (err) {
		dev_err(data->dev, "Can't set clk_pll_a_out0 rate: %d\n", err);
		return err;
	}

	err = clk_set_rate(data->clk_cdev1, clk_out_rate);
	if (err) {
		dev_err(data->dev, "Can't set clk_cdev1 rate: %d\n", err);
		return err;
	}

	err = clk_prepare_enable(data->clk_pll_a);
	if (err) {
		dev_err(data->dev, "Can't enable pll_a: %d\n", err);
		return err;
	}
	data->clk_pll_a_state = 1;

	err = clk_prepare_enable(data->clk_pll_a_out0);
	if (err) {
		dev_err(data->dev, "Can't enable pll_a_out0: %d\n", err);
		return err;
	}
	data->clk_pll_a_out0_state = 1;

	err = clk_prepare_enable(data->clk_cdev1);
	if (err) {
		dev_err(data->dev, "Can't enable cdev1: %d\n", err);
		return err;
	}
	data->clk_cdev1_state = 1;

	data->set_baseclock = new_baseclock;
	data->set_mclk = mclk;

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_alt_asoc_utils_set_rate);

void tegra_alt_asoc_utils_lock_clk_rate(struct tegra_asoc_audio_clock_info *data,
				    int lock)
{
	if (lock)
		data->lock_count++;
	else if (data->lock_count)
		data->lock_count--;
}
EXPORT_SYMBOL_GPL(tegra_alt_asoc_utils_lock_clk_rate);

int tegra_alt_asoc_utils_clk_enable(struct tegra_asoc_audio_clock_info *data)
{
	int err;

	err = clk_prepare_enable(data->clk_cdev1);
	if (err) {
		dev_err(data->dev, "Can't enable cdev1: %d\n", err);
		return err;
	}
	data->clk_cdev1_state = 1;

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_alt_asoc_utils_clk_enable);

int tegra_alt_asoc_utils_clk_disable(struct tegra_asoc_audio_clock_info *data)
{
	clk_disable_unprepare(data->clk_cdev1);
	data->clk_cdev1_state = 0;

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_alt_asoc_utils_clk_disable);

int tegra_alt_asoc_utils_init(struct tegra_asoc_audio_clock_info *data,
			  struct device *dev, struct snd_soc_card *card)
{
	int ret;

	data->dev = dev;
	data->card = card;

	data->clk_pll_p_out1 = clk_get_sys(NULL, "pll_p_out1");
	if (IS_ERR(data->clk_pll_p_out1)) {
		dev_err(data->dev, "Can't retrieve clk pll_p_out1\n");
		ret = PTR_ERR(data->clk_pll_p_out1);
		goto err;
	}

	if (of_machine_is_compatible("nvidia,tegra20"))
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA20;
	else if (of_machine_is_compatible("nvidia,tegra30"))
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA30;
	else if (of_machine_is_compatible("nvidia,tegra114"))
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA114;
	else if (of_machine_is_compatible("nvidia,tegra148"))
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA148;
	else if (of_machine_is_compatible("nvidia,tegra124"))
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA124;
	else if (!dev->of_node) {
		/* non-DT is always Tegra20 */
#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA20;
#elif defined(CONFIG_ARCH_TEGRA_3x_SOC)
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA30;
#elif defined(CONFIG_ARCH_TEGRA_11x_SOC)
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA114;
#elif defined(CONFIG_ARCH_TEGRA_14x_SOC)
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA148;
#elif defined(CONFIG_ARCH_TEGRA_12x_SOC)
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA124;
#endif
	} else
		/* DT boot, but unknown SoC */
		return -EINVAL;

	data->clk_pll_a = clk_get_sys(NULL, "pll_a");
	if (IS_ERR(data->clk_pll_a)) {
		dev_err(data->dev, "Can't retrieve clk pll_a\n");
		ret = PTR_ERR(data->clk_pll_a);
		goto err_put_pll_p_out1;
	}

	data->clk_pll_a_out0 = clk_get_sys(NULL, "pll_a_out0");
	if (IS_ERR(data->clk_pll_a_out0)) {
		dev_err(data->dev, "Can't retrieve clk pll_a_out0\n");
		ret = PTR_ERR(data->clk_pll_a_out0);
		goto err_put_pll_a;
	}

	data->clk_m = clk_get_sys(NULL, "clk_m");
	if (IS_ERR(data->clk_m)) {
		dev_err(data->dev, "Can't retrieve clk clk_m\n");
		ret = PTR_ERR(data->clk_m);
		goto err;
	}

	if (data->soc == TEGRA_ASOC_UTILS_SOC_TEGRA20)
		data->clk_cdev1 = clk_get_sys(NULL, "cdev1");
	else
		data->clk_cdev1 = clk_get_sys("extern1", NULL);

	if (IS_ERR(data->clk_cdev1)) {
		dev_err(data->dev, "Can't retrieve clk cdev1\n");
		ret = PTR_ERR(data->clk_cdev1);
		goto err_put_pll_a_out0;
	}

	if (data->soc == TEGRA_ASOC_UTILS_SOC_TEGRA20)
		data->clk_out1 = ERR_PTR(-ENOENT);
	else {
		data->clk_out1 = clk_get_sys("clk_out_1", "extern1");
		if (IS_ERR(data->clk_out1)) {
			dev_err(data->dev, "Can't retrieve clk out1\n");
			ret = PTR_ERR(data->clk_out1);
			goto err_put_cdev1;
		}
	}

	ret = clk_prepare_enable(data->clk_cdev1);
	if (ret) {
		dev_err(data->dev, "Can't enable clk cdev1/extern1");
		goto err_put_out1;
	}
	data->clk_cdev1_state = 1;

	if (!IS_ERR(data->clk_out1)) {
		ret = clk_prepare_enable(data->clk_out1);
		if (ret) {
			dev_err(data->dev, "Can't enable clk out1");
			goto err_put_out1;
		}
	}

	ret = tegra_alt_asoc_utils_set_rate(data, 48000, 256 * 48000, 256 * 48000);
	if (ret)
		goto err_put_out1;

	return 0;

err_put_out1:
	if (!IS_ERR(data->clk_out1))
		clk_put(data->clk_out1);
err_put_cdev1:
	clk_put(data->clk_cdev1);
err_put_pll_a_out0:
	clk_put(data->clk_pll_a_out0);
err_put_pll_a:
	clk_put(data->clk_pll_a);
err_put_pll_p_out1:
	clk_put(data->clk_pll_p_out1);
err:
	return ret;
}
EXPORT_SYMBOL_GPL(tegra_alt_asoc_utils_init);

int tegra_alt_asoc_utils_set_parent(struct tegra_asoc_audio_clock_info *data,
			int is_i2s_master)
{
	int ret = -ENODEV;

	if (data->soc == TEGRA_ASOC_UTILS_SOC_TEGRA20)
		return ret;

	if (is_i2s_master) {
		ret = clk_set_parent(data->clk_cdev1, data->clk_pll_a_out0);
		if (ret) {
			dev_err(data->dev, "Can't set clk cdev1/extern1 parent");
			return ret;
		}
	} else {
		ret = clk_set_parent(data->clk_cdev1, data->clk_m);
		if (ret) {
			dev_err(data->dev, "Can't set clk cdev1/extern1 parent");
			return ret;
		}

		ret = clk_set_rate(data->clk_cdev1, 13000000);
		if (ret) {
			dev_err(data->dev, "Can't set clk rate");
			return ret;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_alt_asoc_utils_set_parent);

void tegra_alt_asoc_utils_fini(struct tegra_asoc_audio_clock_info *data)
{
	if (data->clk_cdev1_state)
		clk_disable(data->clk_cdev1);

	if (!IS_ERR(data->clk_out1))
		clk_put(data->clk_out1);

	if (!IS_ERR(data->clk_pll_a_out0))
		clk_put(data->clk_pll_a_out0);

	if (!IS_ERR(data->clk_pll_a))
		clk_put(data->clk_pll_a);

	if (!IS_ERR(data->clk_pll_p_out1))
		clk_put(data->clk_pll_p_out1);
}
EXPORT_SYMBOL_GPL(tegra_alt_asoc_utils_fini);

MODULE_AUTHOR("Stephen Warren <swarren@nvidia.com>");
MODULE_DESCRIPTION("Tegra ASoC utility code");
MODULE_LICENSE("GPL");
