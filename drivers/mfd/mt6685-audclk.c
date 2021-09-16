// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/mfd/mt6685-audclk.h>

/* PMIC EFUSE registers definition */
#define MT6685_DCXO_EXTBUF5_CW0	0x79e
/* offset mask of OTP_CON0 */
#define XO_BBCK5_MODE_SFT		0
#define XO_BBCK5_MODE_MASK      0x3
#define XO_BBCK5_MODE_MSK_SFT   (0x3 << 0)

#define XO_BBCK5_EN_M_SFT		2
#define XO_BBCK5_EN_M_MASK      0x1
#define XO_BBCK5_EN_M_MSK_SFT   (0x1 << 2)

struct clk_chip_data {
	unsigned int reg_num;
	unsigned int base;
};

struct mt6685_clk {
	struct device *dev;
	struct regmap *regmap;
	struct mutex lock;
};

struct mt6685_clk *clk;
void mt6685_set_dcxo(bool enable)
{
	if (!clk || !clk->regmap)
		return;

	if (enable) {
		regmap_update_bits(clk->regmap, MT6685_DCXO_EXTBUF5_CW0,
				XO_BBCK5_EN_M_MSK_SFT,	0x1 << XO_BBCK5_EN_M_SFT);
		usleep_range(400, 420);
	} else {
		regmap_update_bits(clk->regmap, MT6685_DCXO_EXTBUF5_CW0,
				XO_BBCK5_EN_M_MSK_SFT,	0x0 << XO_BBCK5_EN_M_SFT);
	}
}
EXPORT_SYMBOL(mt6685_set_dcxo);

static int mt6685_audclk_probe(struct platform_device *pdev)
{
	clk = devm_kzalloc(&pdev->dev, sizeof(*clk), GFP_KERNEL);
	if (!clk)
		return -ENOMEM;
	clk->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!clk->regmap) {
		dev_info(&pdev->dev, "failed to get efuse regmap\n");
		return -ENODEV;
	}

	mutex_init(&clk->lock);
	clk->dev = &pdev->dev;
	dev_info(&pdev->dev, "%s done\n", __func__);
	return 0;
}
static const struct clk_chip_data mt6685_audclk_data = {
	.reg_num = 128,
	.base = MT6685_DCXO_EXTBUF5_CW0,
};
static const struct of_device_id mt6338_of_match[] = {
	{.compatible = "mediatek,mt6338_snd",},
	{},
};

static const struct of_device_id mt6685_audclk_of_match[] = {
	{.compatible = "mediatek,mt6685-audclk",},
	{/* sentinel */},
};
static struct platform_driver mt6685_audclk_driver = {
	.probe = mt6685_audclk_probe,
	.driver = {
		.name = "mt6685-audclk",
		.of_match_table = mt6685_audclk_of_match,
	},
};
module_platform_driver(mt6685_audclk_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Ting-Fang Hou <ting-fang.hou@mediatek.com>");
MODULE_DESCRIPTION("MediaTek PMIC AUDCLK Driver for MT6685 PMIC");

