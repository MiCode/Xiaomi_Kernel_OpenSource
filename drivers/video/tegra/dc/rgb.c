/*
 * drivers/video/tegra/dc/rgb.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Author: Erik Gilling <konkers@android.com>
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

#include "dc_reg.h"
#include "dc_priv.h"


static const u32 tegra_dc_rgb_enable_partial_pintable[] = {
	DC_COM_PIN_OUTPUT_ENABLE0,	0x00000000,
	DC_COM_PIN_OUTPUT_ENABLE1,	0x00000000,
	DC_COM_PIN_OUTPUT_ENABLE2,	0x00000000,
	DC_COM_PIN_OUTPUT_ENABLE3,	0x00000000,
	DC_COM_PIN_OUTPUT_POLARITY0,	0x00000000,
	DC_COM_PIN_OUTPUT_POLARITY2,	0x00000000,
	DC_COM_PIN_OUTPUT_DATA0,	0x00000000,
	DC_COM_PIN_OUTPUT_DATA1,	0x00000000,
	DC_COM_PIN_OUTPUT_DATA2,	0x00000000,
	DC_COM_PIN_OUTPUT_DATA3,	0x00000000,
};

static const u32 tegra_dc_rgb_enable_pintable[] = {
	DC_COM_PIN_OUTPUT_ENABLE0,	0x00000000,
	DC_COM_PIN_OUTPUT_ENABLE1,	0x00000000,
	DC_COM_PIN_OUTPUT_ENABLE2,	0x00000000,
	DC_COM_PIN_OUTPUT_ENABLE3,	0x00000000,
	DC_COM_PIN_OUTPUT_POLARITY0,	0x00000000,
	DC_COM_PIN_OUTPUT_POLARITY1,	0x01000000,
	DC_COM_PIN_OUTPUT_POLARITY2,	0x00000000,
	DC_COM_PIN_OUTPUT_POLARITY3,	0x00000000,
	DC_COM_PIN_OUTPUT_DATA0,	0x00000000,
	DC_COM_PIN_OUTPUT_DATA1,	0x00000000,
	DC_COM_PIN_OUTPUT_DATA2,	0x00000000,
	DC_COM_PIN_OUTPUT_DATA3,	0x00000000,
};

static const u32 tegra_dc_rgb_enable_out_sel_pintable[] = {
	DC_COM_PIN_OUTPUT_SELECT0,	0x00000000,
	DC_COM_PIN_OUTPUT_SELECT1,	0x00000000,
	DC_COM_PIN_OUTPUT_SELECT2,	0x00000000,
#ifdef CONFIG_TEGRA_SILICON_PLATFORM
	DC_COM_PIN_OUTPUT_SELECT3,	0x00000000,
#else
	/* The display panel sub-board used on FPGA platforms (panel 86)
	   is non-standard. It expects the Data Enable signal on the WR
	   pin instead of the DE pin. */
	DC_COM_PIN_OUTPUT_SELECT3,	0x00200000,
#endif
	DC_COM_PIN_OUTPUT_SELECT4,	0x00210222,
	DC_COM_PIN_OUTPUT_SELECT5,	0x00002200,
	DC_COM_PIN_OUTPUT_SELECT6,	0x00020000,
};

static const u32 tegra_dc_rgb_disable_pintable[] = {
	DC_COM_PIN_OUTPUT_ENABLE0,	0x55555555,
	DC_COM_PIN_OUTPUT_ENABLE1,	0x55150005,
	DC_COM_PIN_OUTPUT_ENABLE2,	0x55555555,
	DC_COM_PIN_OUTPUT_ENABLE3,	0x55555555,
	DC_COM_PIN_OUTPUT_POLARITY0,	0x00000000,
	DC_COM_PIN_OUTPUT_POLARITY1,	0x00000000,
	DC_COM_PIN_OUTPUT_POLARITY2,	0x00000000,
	DC_COM_PIN_OUTPUT_POLARITY3,	0x00000000,
	DC_COM_PIN_OUTPUT_DATA0,	0xaaaaaaaa,
	DC_COM_PIN_OUTPUT_DATA1,	0xaaaaaaaa,
	DC_COM_PIN_OUTPUT_DATA2,	0xaaaaaaaa,
	DC_COM_PIN_OUTPUT_DATA3,	0xaaaaaaaa,
	DC_COM_PIN_OUTPUT_SELECT0,	0x00000000,
	DC_COM_PIN_OUTPUT_SELECT1,	0x00000000,
	DC_COM_PIN_OUTPUT_SELECT2,	0x00000000,
	DC_COM_PIN_OUTPUT_SELECT3,	0x00000000,
	DC_COM_PIN_OUTPUT_SELECT4,	0x00000000,
	DC_COM_PIN_OUTPUT_SELECT5,	0x00000000,
	DC_COM_PIN_OUTPUT_SELECT6,	0x00000000,
};

static void tegra_dc_rgb_enable(struct tegra_dc *dc)
{
	int i;
	u32 out_sel_pintable[ARRAY_SIZE(tegra_dc_rgb_enable_out_sel_pintable)];

	tegra_dc_io_start(dc);
	tegra_dc_writel(dc, PW0_ENABLE | PW1_ENABLE | PW2_ENABLE | PW3_ENABLE |
			PW4_ENABLE | PM0_ENABLE | PM1_ENABLE,
			DC_CMD_DISPLAY_POWER_CONTROL);

	tegra_dc_writel(dc, DISP_CTRL_MODE_C_DISPLAY, DC_CMD_DISPLAY_COMMAND);

	if (dc->out->out_pins) {
		tegra_dc_set_out_pin_polars(dc, dc->out->out_pins,
			dc->out->n_out_pins);
		tegra_dc_write_table(dc, tegra_dc_rgb_enable_partial_pintable);
	} else {
		tegra_dc_write_table(dc, tegra_dc_rgb_enable_pintable);
	}

	memcpy(out_sel_pintable, tegra_dc_rgb_enable_out_sel_pintable,
		sizeof(tegra_dc_rgb_enable_out_sel_pintable));

	if (dc->out && dc->out->out_sel_configs) {
		u8 *out_sels = dc->out->out_sel_configs;
		for (i = 0; i < dc->out->n_out_sel_configs; i++) {
			switch (out_sels[i]) {
			case TEGRA_PIN_OUT_CONFIG_SEL_LM1_M1:
				out_sel_pintable[5*2+1] =
					(out_sel_pintable[5*2+1] &
					~PIN5_LM1_LCD_M1_OUTPUT_MASK) |
					PIN5_LM1_LCD_M1_OUTPUT_M1;
				break;
			case TEGRA_PIN_OUT_CONFIG_SEL_LM1_LD21:
				out_sel_pintable[5*2+1] =
					(out_sel_pintable[5*2+1] &
					~PIN5_LM1_LCD_M1_OUTPUT_MASK) |
					PIN5_LM1_LCD_M1_OUTPUT_LD21;
				break;
			case TEGRA_PIN_OUT_CONFIG_SEL_LM1_PM1:
				out_sel_pintable[5*2+1] =
					(out_sel_pintable[5*2+1] &
					~PIN5_LM1_LCD_M1_OUTPUT_MASK) |
					PIN5_LM1_LCD_M1_OUTPUT_PM1;
				break;
			default:
				dev_err(&dc->ndev->dev,
					"Invalid pin config[%d]: %d\n",
					 i, out_sels[i]);
				break;
			}
		}
	}

	tegra_dc_write_table(dc, out_sel_pintable);

	/* Inform DC register updated */
	tegra_dc_writel(dc, GENERAL_UPDATE, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);
	tegra_dc_io_end(dc);
}

static void tegra_dc_rgb_disable(struct tegra_dc *dc)
{
	tegra_dc_io_start(dc);
	tegra_dc_writel(dc, 0x00000000, DC_CMD_DISPLAY_POWER_CONTROL);

	tegra_dc_write_table(dc, tegra_dc_rgb_disable_pintable);
	tegra_dc_io_end(dc);
}

static long tegra_dc_rgb_setup_clk(struct tegra_dc *dc, struct clk *clk)
{
	unsigned long rate;
	struct clk *parent_clk =
		clk_get_sys(NULL, dc->out->parent_clk ? : "pll_p");

	if (dc->out->parent_clk_backup &&
	    (parent_clk == clk_get_sys(NULL, "pll_p"))) {
		rate = tegra_dc_pclk_predict_rate(
			parent_clk, dc->mode.pclk);
		/* use pll_d as last resort */
		if (rate < (dc->mode.pclk / 100 * 99) ||
		    rate > (dc->mode.pclk / 100 * 109))
			parent_clk = clk_get_sys(
				NULL, dc->out->parent_clk_backup);
	}

	if (clk_get_parent(clk) != parent_clk)
		clk_set_parent(clk, parent_clk);

	if (parent_clk != clk_get_sys(NULL, "pll_p")) {
		struct clk *base_clk = clk_get_parent(parent_clk);

		/* Assuming either pll_d or pll_d2 is used */
		rate = dc->mode.pclk * 2;

		if (rate != clk_get_rate(base_clk))
			clk_set_rate(base_clk, rate);
	}

	return tegra_dc_pclk_round_rate(dc, dc->mode.pclk);
}

struct tegra_dc_out_ops tegra_dc_rgb_ops = {
	.enable = tegra_dc_rgb_enable,
	.disable = tegra_dc_rgb_disable,
	.setup_clk = tegra_dc_rgb_setup_clk,
};

