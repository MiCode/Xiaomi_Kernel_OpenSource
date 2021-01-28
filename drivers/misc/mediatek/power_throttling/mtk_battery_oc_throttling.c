// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#include <linux/interrupt.h>
#include <linux/mfd/mt6359/registers.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include "mtk_battery_oc_throttling.h"

#define DEF_BAT_OC_THD_H	5800
#define DEF_BAT_OC_THD_L	6300

#define UNIT_FGCURRENT		(610352)
#define CURRENT_CONVERT_RATIO	95

#define OCCB_MAX_NUM 16

/*
 * 65535–(I_mA * 1000 * r_fg_value / DEFAULT_RFG * 1000000 / car_tune_value
 * / UNIT_FGCURRENT * CURRENT_CONVERT_RATIO / 100)
 * 65535 - (I_mA 1000 * 50 / 50 * 1000000 / 1000 / 610352 * 95 / 100)
 * 65535 - (I_mA * 1000000 / 610352 * 95 / 100)
 */
static unsigned int to_fg_code(u64 cur_mA)
{
	cur_mA = div_u64(cur_mA * 1000000, UNIT_FGCURRENT);
	cur_mA = div_u64(cur_mA * CURRENT_CONVERT_RATIO, 100);

	/* 2's complement */
	return (0xFFFF - cur_mA);
}

struct reg_t {
	unsigned int addr;
	unsigned int mask;
};

struct battery_oc_regs_t {
	struct reg_t fg_cur_hth;
	struct reg_t fg_cur_lth;
};

struct battery_oc_regs_t mt6359_battery_oc_regs = {
	.fg_cur_hth = {MT6359_FGADC_CUR_CON2, 0xFFFF},
	.fg_cur_lth = {MT6359_FGADC_CUR_CON1, 0xFFFF},
};

struct battery_oc_priv {
	struct regmap *regmap;
	int oc_level;
	unsigned int oc_thd_h;
	unsigned int oc_thd_l;
	int fg_cur_h_irq;
	int fg_cur_l_irq;
	const struct battery_oc_regs_t *regs;
};

struct battery_oc_callback_table {
	void (*occb)(enum BATTERY_OC_LEVEL_TAG);
};

static struct battery_oc_callback_table occb_tb[OCCB_MAX_NUM] = { {0} };

void register_battery_oc_notify(battery_oc_callback oc_cb,
				enum BATTERY_OC_PRIO_TAG prio_val)
{
	if (prio_val >= OCCB_MAX_NUM || prio_val < 0) {
		pr_info("[%s] prio_val=%d, out of boundary\n",
			__func__, prio_val);
		return;
	}
	occb_tb[prio_val].occb = oc_cb;
	pr_info("[%s] prio_val=%d\n", __func__, prio_val);
}
EXPORT_SYMBOL(register_battery_oc_notify);

static void exec_battery_oc_callback(enum BATTERY_OC_LEVEL_TAG battery_oc_level)
{
	int i;

	for (i = 0; i < OCCB_MAX_NUM; i++) {
		if (occb_tb[i].occb) {
			occb_tb[i].occb(battery_oc_level);
			pr_info("[%s] prio_val=%d,battery_oc_level=%d\n",
				__func__, i, battery_oc_level);
		}
	}
}

static irqreturn_t fg_cur_h_int_handler(int irq, void *data)
{
	struct battery_oc_priv *priv = data;

	priv->oc_level = BATTERY_OC_LEVEL_0;
	exec_battery_oc_callback(priv->oc_level);
	disable_irq_nosync(priv->fg_cur_h_irq);
	enable_irq(priv->fg_cur_l_irq);

	return IRQ_HANDLED;
}

static irqreturn_t fg_cur_l_int_handler(int irq, void *data)
{
	struct battery_oc_priv *priv = data;

	priv->oc_level = BATTERY_OC_LEVEL_1;
	exec_battery_oc_callback(priv->oc_level);
	disable_irq_nosync(priv->fg_cur_l_irq);
	enable_irq(priv->fg_cur_h_irq);

	return IRQ_HANDLED;
}

static int battery_oc_throttling_probe(struct platform_device *pdev)
{
	int ret;
	struct battery_oc_priv *priv;
	struct mt6397_chip *chip;

	chip = dev_get_drvdata(pdev->dev.parent);
	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	dev_set_drvdata(&pdev->dev, priv);
	priv->regmap = chip->regmap;
	priv->regs = of_device_get_match_data(&pdev->dev);
	priv->oc_thd_h = DEF_BAT_OC_THD_H;
	priv->oc_thd_l = DEF_BAT_OC_THD_L;

	/* set Maximum threshold to avoid irq being triggered at init */
	regmap_update_bits(priv->regmap, priv->regs->fg_cur_hth.addr,
			   priv->regs->fg_cur_hth.mask, 0x7FFF);
	regmap_update_bits(priv->regmap, priv->regs->fg_cur_lth.addr,
			   priv->regs->fg_cur_lth.mask, 0x8000);
	priv->fg_cur_h_irq = platform_get_irq_byname(pdev, "fg_cur_h");
	if (priv->fg_cur_h_irq < 0) {
		dev_notice(&pdev->dev, "failed to get fg_cur_h irq, ret=%d\n",
			   priv->fg_cur_h_irq);
		return priv->fg_cur_h_irq;
	}
	priv->fg_cur_l_irq = platform_get_irq_byname(pdev, "fg_cur_l");
	if (priv->fg_cur_l_irq < 0) {
		dev_notice(&pdev->dev, "failed to get fg_cur_l irq, ret=%d\n",
			   priv->fg_cur_l_irq);
		return priv->fg_cur_l_irq;
	}
	ret = devm_request_threaded_irq(&pdev->dev, priv->fg_cur_h_irq, NULL,
					fg_cur_h_int_handler, IRQF_TRIGGER_NONE,
					"fg_cur_h", priv);
	if (ret < 0)
		dev_notice(&pdev->dev, "request fg_cur_h irq fail\n");
	ret = devm_request_threaded_irq(&pdev->dev, priv->fg_cur_l_irq, NULL,
					fg_cur_l_int_handler, IRQF_TRIGGER_NONE,
					"fg_cur_l", priv);
	if (ret < 0)
		dev_notice(&pdev->dev, "request fg_cur_l irq fail\n");
	disable_irq_nosync(priv->fg_cur_h_irq);
	regmap_update_bits(priv->regmap, priv->regs->fg_cur_hth.addr,
			   priv->regs->fg_cur_hth.mask,
			   to_fg_code(priv->oc_thd_h));
	regmap_update_bits(priv->regmap, priv->regs->fg_cur_lth.addr,
			   priv->regs->fg_cur_lth.mask,
			   to_fg_code(priv->oc_thd_l));
	dev_info(&pdev->dev, "%dmA(0x%x), %dmA(0x%x) Done\n",
		 priv->oc_thd_h, to_fg_code(priv->oc_thd_h),
		 priv->oc_thd_l, to_fg_code(priv->oc_thd_l));
	return ret;
}

static int __maybe_unused battery_oc_throttling_suspend(struct device *d)
{
	struct battery_oc_priv *priv = dev_get_drvdata(d);

	if (priv->oc_level == BATTERY_OC_LEVEL_0)
		disable_irq_nosync(priv->fg_cur_l_irq);
	else
		disable_irq_nosync(priv->fg_cur_h_irq);
	return 0;
}

static int __maybe_unused battery_oc_throttling_resume(struct device *d)
{
	struct battery_oc_priv *priv = dev_get_drvdata(d);

	if (priv->oc_level == BATTERY_OC_LEVEL_0)
		enable_irq(priv->fg_cur_l_irq);
	else
		enable_irq(priv->fg_cur_h_irq);
	return 0;
}

static SIMPLE_DEV_PM_OPS(battery_oc_throttling_pm_ops,
			 battery_oc_throttling_suspend,
			 battery_oc_throttling_resume);

static const struct of_device_id battery_oc_throttling_of_match[] = {
	{
		.compatible = "mediatek,mt6359-battery_oc_throttling",
		.data = &mt6359_battery_oc_regs,
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, battery_oc_throttling_of_match);

static struct platform_driver battery_oc_throttling_driver = {
	.driver = {
		.name = "mtk_battery_oc_throttling",
		.of_match_table = battery_oc_throttling_of_match,
		.pm = &battery_oc_throttling_pm_ops,
	},
	.probe = battery_oc_throttling_probe,
};
module_platform_driver(battery_oc_throttling_driver);

MODULE_AUTHOR("Jeter Chen");
MODULE_DESCRIPTION("MTK battery over current throttling driver");
MODULE_LICENSE("GPL");
