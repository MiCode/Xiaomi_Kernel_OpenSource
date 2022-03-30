// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: owen.chen <owen.chen@mediatek.com>
 */

/*
 * @file    mtk-clk-buf-ctl.c
 * @brief   Driver for clock buffer control
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "mtk_clkbuf_ctl.h"
#include "mtk_clkbuf_common.h"
#include "mtk_clkbuf_hw.h"

DEFINE_MUTEX(clk_buf_ctrl_lock);

static bool _clk_buf_check(void)
{

	if (clk_buf_get_bringup_sta())
		return false;

	return clk_buf_get_init_sta();
}

int clk_buf_voter_ctrl_by_id(const uint8_t subsys_id, enum RC_CTRL_CMD rc_req)
{
	return 0;
}
EXPORT_SYMBOL(clk_buf_voter_ctrl_by_id);

int clk_buf_set_xo_ctrl(u32 id, bool onoff)
{
	if (_clk_buf_check())
		return clk_buf_hw_ctrl(id, onoff);

	return CLK_BUF_FAIL;
}
EXPORT_SYMBOL(clk_buf_set_xo_ctrl);

/* for spm driver use */
int is_clk_buf_under_flightmode(void)
{
	if (_clk_buf_check())
		return clk_buf_get_flight_mode();

	return CLK_BUF_FAIL;
}
EXPORT_SYMBOL(is_clk_buf_under_flightmode);

/* for ccci driver to notify this */
int clk_buf_set_flight_mode(bool on)
{
	if (_clk_buf_check())
		return clk_buf_hw_set_flight_mode(on);

	return CLK_BUF_FAIL;
}
EXPORT_SYMBOL(clk_buf_set_flight_mode);

int clk_buf_set_bblpm(bool on)
{
	if (_clk_buf_check())
		return clk_buf_ctrl_bblpm_sw(on);

	return CLK_BUF_FAIL;
}
EXPORT_SYMBOL(clk_buf_set_bblpm);

int clk_buf_dump_log(void)
{
	if (_clk_buf_check()) {
		clk_buf_hw_dump_misc_log();
		return 0;
	}

	return CLK_BUF_FAIL;
}
EXPORT_SYMBOL(clk_buf_dump_log);

int clk_buf_get_xo_en_sta(u32 id)
{
	u32 stat = 0;
	int ret = 0;

	if (_clk_buf_check()) {
		ret = clk_buf_hw_get_xo_en(id, &stat);
		if (!ret)
			return stat;
	}

	return CLK_BUF_FAIL;
}
EXPORT_SYMBOL(clk_buf_get_xo_en_sta);

int clk_buf_get_bblpm_enter_cond(u32 *bblpm_cond)
{
	if (_clk_buf_check()) {
		clk_buf_get_enter_bblpm_cond(bblpm_cond);
		return 0;
	}

	return CLK_BUF_FAIL;
}
EXPORT_SYMBOL(clk_buf_get_bblpm_enter_cond);

static int mtk_clkbuf_probe(struct platform_device *pdev)
{
	int ret = 0;

	if (clkbuf_hw_is_ready() == CLK_BUF_NOT_READY)
		return -EPROBE_DEFER;

	clk_buf_get_bringup_node(pdev);
	if (clk_buf_get_bringup_sta())
		return 0;

	ret = clk_buf_dts_init(pdev);
	if (ret) {
		pr_err("%s: failed due to chip DTS failed\n", __func__);
		return ret;
	}

	ret = clk_buf_fs_init();
	if (ret) {
		pr_err("%s: failed due to file operation failed\n", __func__);
		return ret;
	}

	if (clk_buf_get_init_sta())
		return -EALREADY;

	clk_buf_set_init_sta(true);

	ret = clk_buf_xo_init();
	if (ret) {
		pr_err("%s: failed to init xo(%d)\n", __func__, ret);
		return ret;
	}

	ret = clk_buf_bblpm_init();
	if (ret) {
		pr_err("%s: failed due bblpm init failed\n", __func__);
		return ret;
	}

	return ret;
}

static const struct platform_device_id mtk_clkbuf_ids[] = {
	{"mtk-clock-buffer", 0},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(platform, mtk_clkbuf_ids);

static const struct of_device_id mtk_clkbuf_of_match[] = {
	{
		.compatible = "mediatek,clock_buffer",
	},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, mtk_clkbuf_of_match);

static struct platform_driver mtk_clkbuf_driver = {
	.driver = {
		.name = "mtk-clock-buffer",
		.of_match_table = of_match_ptr(mtk_clkbuf_of_match),
	},
	.probe = mtk_clkbuf_probe,
	.id_table = mtk_clkbuf_ids,
};

module_platform_driver(mtk_clkbuf_driver);
MODULE_AUTHOR("Owen Chen <owen.chen@mediatek.com>");
MODULE_DESCRIPTION("SOC Driver for MediaTek Clock Buffer");
MODULE_LICENSE("GPL v2");
