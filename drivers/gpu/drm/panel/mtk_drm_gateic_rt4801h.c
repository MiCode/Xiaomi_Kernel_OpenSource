// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include "mtk_drm_gateic.h"

#define VOL_MIN_LEVEL (VOL_4_0V)
#define VOL_MAX_LEVEL (VOL_6_0V)
#define VOL_REG_VALUE(level) (level - VOL_MIN_LEVEL)

struct mtk_gateic_data ctx_rt4801h;

static int rt4801h_reset(int on)
{
	ctx_rt4801h.reset_gpio =
		devm_gpiod_get(ctx_rt4801h.dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx_rt4801h.reset_gpio)) {
		dev_err(ctx_rt4801h.dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx_rt4801h.reset_gpio));
		return PTR_ERR(ctx_rt4801h.reset_gpio);
	}
	gpiod_set_value(ctx_rt4801h.reset_gpio, on);
	DDPMSG("%s, gpio:0x%x, on:%d\n",
		__func__, ctx_rt4801h.reset_gpio, on);
	devm_gpiod_put(ctx_rt4801h.dev, ctx_rt4801h.reset_gpio);

	return 0;
}

static int rt4801h_power_on(void)
{
	int ret = 0;

	if (ctx_rt4801h.init != 1) {
		DDPPR_ERR("%s gate ic is not initialized\n", __func__);
		return -1;
	}

	if (ctx_rt4801h.ref++ > 0) {
		DDPPR_ERR("%s gate ic (%u) already power on\n", __func__, ctx_rt4801h.ref);
		return 0;
	}

	//DDPMSG("%s++\n", __func__);
	ctx_rt4801h.bias_pos_gpio = devm_gpiod_get_index(ctx_rt4801h.dev,
		"bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx_rt4801h.bias_pos_gpio)) {
		dev_err(ctx_rt4801h.dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx_rt4801h.bias_pos_gpio));
		return PTR_ERR(ctx_rt4801h.bias_pos_gpio);
	}
	gpiod_set_value(ctx_rt4801h.bias_pos_gpio, 1);
	devm_gpiod_put(ctx_rt4801h.dev, ctx_rt4801h.bias_pos_gpio);



	udelay(2000);

	ctx_rt4801h.bias_neg_gpio = devm_gpiod_get_index(ctx_rt4801h.dev,
		"bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx_rt4801h.bias_neg_gpio)) {
		dev_err(ctx_rt4801h.dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx_rt4801h.bias_neg_gpio));
		return PTR_ERR(ctx_rt4801h.bias_neg_gpio);
	}
	gpiod_set_value(ctx_rt4801h.bias_neg_gpio, 1);
	devm_gpiod_put(ctx_rt4801h.dev, ctx_rt4801h.bias_neg_gpio);

	DDPMSG("%s--, %d\n", __func__, ret);
	return ret;
}

static int rt4801h_power_off(void)
{
	if (ctx_rt4801h.init != 1) {
		DDPPR_ERR("%s gate ic is not initialized\n", __func__);
		return -1;
	}

	if (ctx_rt4801h.ref == 0) {
		DDPPR_ERR("%s gate ic (%u) already power off\n", __func__, ctx_rt4801h.ref);
		return 0;
	}

	//DDPMSG("%s++\n", __func__);
	ctx_rt4801h.ref--;
	ctx_rt4801h.reset_gpio =
		devm_gpiod_get(ctx_rt4801h.dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx_rt4801h.reset_gpio)) {
		dev_err(ctx_rt4801h.dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx_rt4801h.reset_gpio));
		return PTR_ERR(ctx_rt4801h.reset_gpio);
	}
	gpiod_set_value(ctx_rt4801h.reset_gpio, 0);
	devm_gpiod_put(ctx_rt4801h.dev, ctx_rt4801h.reset_gpio);


	ctx_rt4801h.bias_neg_gpio = devm_gpiod_get_index(ctx_rt4801h.dev,
		"bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx_rt4801h.bias_neg_gpio)) {
		dev_err(ctx_rt4801h.dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx_rt4801h.bias_neg_gpio));
		return PTR_ERR(ctx_rt4801h.bias_neg_gpio);
	}
	gpiod_set_value(ctx_rt4801h.bias_neg_gpio, 0);
	devm_gpiod_put(ctx_rt4801h.dev, ctx_rt4801h.bias_neg_gpio);

	udelay(1000);

	ctx_rt4801h.bias_pos_gpio = devm_gpiod_get_index(ctx_rt4801h.dev,
		"bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx_rt4801h.bias_pos_gpio)) {
		dev_err(ctx_rt4801h.dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx_rt4801h.bias_pos_gpio));
		return PTR_ERR(ctx_rt4801h.bias_pos_gpio);
	}
	gpiod_set_value(ctx_rt4801h.bias_pos_gpio, 0);
	devm_gpiod_put(ctx_rt4801h.dev, ctx_rt4801h.bias_pos_gpio);

	DDPMSG("%s--\n", __func__);
	return 0;
}

static int rt4801h_set_voltage(enum vol_level level)
{
	int ret = 0;

	if (ctx_rt4801h.init != 1) {
		DDPPR_ERR("%s gate ic is not initialized\n", __func__);
		return -1;
	}

	if (ctx_rt4801h.ref == 0) {
		DDPPR_ERR("%s gate ic (%u) is power off\n", __func__, ctx_rt4801h.ref);
		return -2;
	}

	if (level < VOL_MIN_LEVEL || level > VOL_MAX_LEVEL) {
		DDPPR_ERR("%s invalid voltage level:%d\n", __func__, level);
		return -3;
	}

	DDPMSG("%s++ level:%d\n", __func__, level);
	ret = mtk_panel_i2c_write_bytes(0, VOL_REG_VALUE(level));
	if (ret)
		return ret;
	ret = mtk_panel_i2c_write_bytes(1, VOL_REG_VALUE(level));
	if (ret)
		return ret;
	//DDPMSG("%s--\n", __func__);
	return 0;
}

static struct mtk_gateic_funcs rt4801h_ops = {
	.reset = rt4801h_reset,
	.power_on = rt4801h_power_on,
	.power_off = rt4801h_power_off,
	.set_voltage = rt4801h_set_voltage,
};

static int rt4801h_drv_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0;

	if (ctx_rt4801h.init == 1)
		return 0;
	DDPMSG("%s++\n", __func__);

	ctx_rt4801h.dev = dev;

	ctx_rt4801h.reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx_rt4801h.reset_gpio)) {
		dev_err(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx_rt4801h.reset_gpio));
		return PTR_ERR(ctx_rt4801h.reset_gpio);
	}
	devm_gpiod_put(dev, ctx_rt4801h.reset_gpio);

	ctx_rt4801h.bias_pos_gpio = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx_rt4801h.bias_pos_gpio)) {
		dev_err(dev, "%s: cannot get bias-pos 0 %ld\n",
			__func__, PTR_ERR(ctx_rt4801h.bias_pos_gpio));
		return PTR_ERR(ctx_rt4801h.bias_pos_gpio);
	}
	devm_gpiod_put(dev, ctx_rt4801h.bias_pos_gpio);

	ctx_rt4801h.bias_neg_gpio = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx_rt4801h.bias_neg_gpio)) {
		dev_err(dev, "%s: cannot get bias-neg 1 %ld\n",
			__func__, PTR_ERR(ctx_rt4801h.bias_neg_gpio));
		return PTR_ERR(ctx_rt4801h.bias_neg_gpio);
	}
	devm_gpiod_put(dev, ctx_rt4801h.bias_neg_gpio);

	ctx_rt4801h.ref = 1;
	ctx_rt4801h.init = 1;

	ret = mtk_drm_gateic_set(&rt4801h_ops, MTK_LCM_FUNC_DSI);
	DDPMSG("%s--, %d\n", __func__, ret);

	return ret;
}

static int rt4801h_drv_remove(struct platform_device *pdev)
{
	DDPMSG("%s\n", __func__);

	return 0;
}


static const struct of_device_id rt4801h_of_match[] = {
	{ .compatible = "mediatek,mtk-drm-gateic-drv-rt4801h", },
	{ }
};

MODULE_DEVICE_TABLE(of, rt4801h_of_match);

struct platform_driver mtk_gateic_rt4801h_driver = {
	.probe = rt4801h_drv_probe,
	.remove = rt4801h_drv_remove,
	.driver = {
		.name = "mtk-drm-gateic-drv-rt4801h",
		.owner = THIS_MODULE,
		.of_match_table = rt4801h_of_match,
	},
};

MODULE_AUTHOR("Cui Zhang <cui.zhang@mediatek.com>");
MODULE_DESCRIPTION("mediatek, drm GATE IC driver of RT4801H");
MODULE_LICENSE("GPL v2");
