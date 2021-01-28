// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

/*
 * @file    mtk_clk_buf_ctl.c
 * @brief   Driver for clock buffer control
 *
 */

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/mfd/mt6397/core.h>

#include <mtk_spm.h>
#include <mtk_clkbuf_ctl.h>
#include <mtk_clkbuf_common.h>

DEFINE_MUTEX(clk_buf_ctrl_lock);

bool is_clkbuf_initiated;

/* false: rf_clkbuf, true: pmic_clkbuf */
bool is_pmic_clkbuf = true;

bool clkbuf_debug;
static bool g_is_flightmode_on;
unsigned int bblpm_cnt;

unsigned int clkbuf_ctrl_stat;

short __attribute__((weak)) is_clkbuf_bringup(void)
{
	return 0;
}

int __attribute__((weak)) clk_buf_dts_map(void)
{
	return 0;
}

void __attribute__((weak)) clk_buf_dump_dts_log(void)
{
}

int __attribute__((weak)) clk_buf_fs_init(void)
{
	pr_info("%s: dummy func\n", __func__);
	return 0;
}

void __attribute__((weak)) clk_buf_post_init(void)
{
}

void __attribute__((weak)) clk_buf_init_pmic_swctrl(void)
{
}

void __attribute__((weak)) clk_buf_init_pmic_clkbuf(struct regmap *regmap)
{
}

void __attribute__((weak)) clk_buf_init_pmic_wrap(void)
{
}

void __attribute__((weak)) clk_buf_ctrl_bblpm_hw(short on)
{
}

/* for spm driver use */
bool is_clk_buf_under_flightmode(void)
{
	return g_is_flightmode_on;
}

/* for ccci driver to notify this */
void clk_buf_set_by_flightmode(bool is_flightmode_on)
{
	g_is_flightmode_on = is_flightmode_on;
	if (is_flightmode_on)
		clk_buf_ctrl_bblpm_hw(true);
	else
		clk_buf_ctrl_bblpm_hw(false);
}

void clk_buf_get_swctrl_status(enum CLK_BUF_SWCTRL_STATUS_T *status)
{
	if (!is_clkbuf_initiated)
		return;

	if (is_pmic_clkbuf)
		return;
}

/*
 * Let caller get driving current setting of RF clock buffer
 * Caller: ccci & ccci will send it to modem
 */
void clk_buf_get_rf_drv_curr(void *rf_drv_curr)
{
	if (!is_clkbuf_initiated)
		return;

	if (is_pmic_clkbuf)
		return;
}

/* Called by ccci driver to keep afcdac value sent from modem */
void clk_buf_save_afc_val(unsigned int afcdac)
{
	if (is_pmic_clkbuf)
		return;
}

/* Called by suspend driver to write afcdac into SPM register */
bool is_clk_buf_from_pmic(void)
{
	return true;
}

static int mtk_clk_buf_probe(struct platform_device *pdev)
{
	struct mt6397_chip *chip;

	chip = dev_get_drvdata(pdev->dev.parent);

	if (is_clkbuf_bringup()) {
		clk_buf_dts_map();
		clk_buf_fs_init();
		return 0;
	}

	if (is_clkbuf_initiated)
		return -1;

	if (clk_buf_dts_map()) {
		pr_err("%s: failed due to DTS failed\n", __func__);
		return -1;
	}

	clk_buf_dump_dts_log();

	if (clk_buf_fs_init())
		return -1;

	/* Co-TSX @PMIC */
	if (is_clk_buf_from_pmic()) {
		is_pmic_clkbuf = true;
		clk_buf_init_pmic_clkbuf(chip->regmap);
		clk_buf_init_pmic_swctrl();
		clk_buf_init_pmic_wrap();
	}

	clk_buf_post_init();

	is_clkbuf_initiated = true;

	return 0;
}

static const struct of_device_id mtk_clkbuf_of_match[] = {
{
		.compatible = "mediatek,pmic_clock_buffer",
	},
};

MODULE_DEVICE_TABLE(of, mtk_clkbuf_of_match);

static struct platform_driver mtk_clkbuf_driver = {
	.driver = {
		.name = "mtk_clkbuf_driver",
		.of_match_table = mtk_clkbuf_of_match,
	},
	.probe	= mtk_clk_buf_probe,
};
module_platform_driver(mtk_clkbuf_driver);


