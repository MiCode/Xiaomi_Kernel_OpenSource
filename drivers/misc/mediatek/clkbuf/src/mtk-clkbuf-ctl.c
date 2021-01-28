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

#include <mtk-clkbuf-bridge.h>
#include <mtk_clkbuf_ctl.h>
#include <mtk_clkbuf_common.h>

DEFINE_MUTEX(clk_buf_ctrl_lock);

struct clk_buf {
	const char		*name;
	const struct clk_buf_op	*ops;
	struct clk_hw		*hw;
	struct device		*dev;
};

static struct clk_buf *clk_buf_core;

static bool _clk_buf_check(void)
{
	if (!clk_buf_core || !clk_buf_core->ops) {
		pr_err("Not register operand yet\n");
		return false;
	}

	if (clk_buf_core->ops->get_bringup_sta())
		return false;

	return clk_buf_core->ops->get_clkbuf_init_sta();
}

int clk_buf_set_xo_ctrl(enum clk_buf_id id, bool onoff)
{
	if (_clk_buf_check())
		return clk_buf_core->ops->set_xo_sta(id, onoff);

	return CLK_BUF_NOT_SUPPORT;
}

/* for spm driver use */
int is_clk_buf_under_flightmode(void)
{
	if (_clk_buf_check())
		return clk_buf_core->ops->get_flight_mode();

	return CLK_BUF_NOT_SUPPORT;
}

/* for ccci driver to notify this */
int clk_buf_set_flight_mode(bool on)
{
	if (_clk_buf_check()) {
		clk_buf_core->ops->set_flight_mode(on);
		return CLK_BUF_OK;
	}

	return CLK_BUF_NOT_SUPPORT;
}

int clk_buf_set_bblpm(bool on)
{
	int ret = 0;

	if (_clk_buf_check()) {
		ret = clk_buf_core->ops->set_bblpm_sta(on);

		return ret;
	}

	return CLK_BUF_NOT_SUPPORT;
}

void clk_buf_dump_log(void)
{
	if (_clk_buf_check())
		clk_buf_core->ops->get_main_log();
}

u8 clk_buf_get_xo_ctrl(enum xo_id id)
{
	if (_clk_buf_check())
		return clk_buf_core->ops->get_xo_sta(id);

	return CLK_BUF_NOT_SUPPORT;
}

int clk_buf_get_bblpm_enter_cond(u32 *bblpm_cond)
{
	if (_clk_buf_check()) {
		clk_buf_core->ops->get_bblpm_enter_cond(bblpm_cond);
		return CLK_BUF_OK;
	}
	return CLK_BUF_NOT_SUPPORT;
}

int mtk_register_clk_buf(struct device *dev, struct clk_buf_op *ops)
{
	struct clk_buf_bridge pbridge;
	int ret = 0;

	clk_buf_core = kzalloc(sizeof(struct clk_buf), GFP_KERNEL);
	if (!clk_buf_core) {
		ret = -ENOMEM;
		goto fail_out;
	}

	if (WARN_ON(!ops)) {
		ret = -EINVAL;
		goto fail_ops;
	}
	clk_buf_core->ops = ops;
	clk_buf_core->dev = dev;
	clk_buf_core->name = dev->driver->name;

	pbridge.get_xo_ctrl_cb = clk_buf_get_xo_ctrl;
	pbridge.get_bblpm_enter_cond_cb = clk_buf_get_bblpm_enter_cond;
	pbridge.set_bblpm_cb = clk_buf_set_bblpm;
	pbridge.set_flight_mode_cb = clk_buf_set_flight_mode;
	pbridge.set_xo_ctrl_cb = clk_buf_set_xo_ctrl;
	pbridge.dump_log_cb = clk_buf_dump_log;
	clk_buf_export_platform_bridge_register(&pbridge);

	return ret;
fail_ops:
	kfree(clk_buf_core);
fail_out:
	return ret;
}

static int mtk_clkbuf_probe(struct platform_device *pdev)
{
	int (*clkbuf_probe)(struct platform_device *pdev);
	int ret = 0;

	clkbuf_probe = of_device_get_match_data(&pdev->dev);
	if (!clkbuf_probe)
		return -EINVAL;

	ret = clkbuf_probe(pdev);
	if (ret)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, ret);

	if (!clk_buf_core || !clk_buf_core->ops) {
		pr_err("Not register operand yet\n");
		return -ENODEV;
	}

	if (clk_buf_core->ops->get_bringup_sta())
		return 0;

	ret = clk_buf_core->ops->dts_init(pdev);
	if (ret) {
		pr_err("%s: failed due to chip DTS failed\n", __func__);
		return ret;
	}

	ret = clk_buf_core->ops->fs_init();
	if (ret) {
		pr_err("%s: failed due to file operation failed\n", __func__);
		return ret;
	}

	if (clk_buf_core->ops->get_clkbuf_init_sta())
		return -EALREADY;

	clk_buf_core->ops->xo_init();

	clk_buf_core->ops->bblpm_init();

	clk_buf_core->ops->set_clkbuf_init_sta(true);

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
		.data = clk_buf_hw_probe,
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
MODULE_DESCRIPTION("SOC Driver for MediaTek PMIC Clock Buffer");
MODULE_LICENSE("GPL v2");

