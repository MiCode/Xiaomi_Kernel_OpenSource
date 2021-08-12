// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 *
 * Author: Hank Liao <hank.liao@mediatek.com>
 */

#include <linux/kernel.h>
#include <linux/mfd/core.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/spmi.h>

#define MT6685_DCXO_BASE 0x780
#define MT6685_DCXO_SIZE 0x75

static const struct resource mt6685_dcxo_resources[] = {
	DEFINE_RES_MEM(MT6685_DCXO_BASE, MT6685_DCXO_SIZE),
};

static const struct mfd_cell mt6685_devs[] = {
	{
		.name = "mt6685-rtc",
		.of_compatible = "mediatek,mt6685-rtc",
	}, {
		.name = "mt6685-consys",
		.of_compatible = "mediatek,mt6685-consys",
	}, {
		.name = "mt6685-clock_buffer",
		.num_resources = ARRAY_SIZE(mt6685_dcxo_resources),
		.resources = mt6685_dcxo_resources,
		.of_compatible = "mediatek,clock_buffer",
	}, {
		.name = "mt6685-audclk",
		.of_compatible = "mediatek,mt6685-audclk",
	}
};

static const struct regmap_config spmi_regmap_config = {
	.reg_bits	= 16,
	.val_bits	= 8,
	.max_register	= 0x2000,
	.fast_io	= true,
	.use_single_read = true,
	.use_single_write = true
};

static int mt6685_spmi_probe(struct spmi_device *sdev)
{
	int ret;
	struct regmap *regmap;

	regmap = devm_regmap_init_spmi_ext(sdev, &spmi_regmap_config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	ret = devm_mfd_add_devices(&sdev->dev, -1, mt6685_devs,
				   ARRAY_SIZE(mt6685_devs), NULL, 0, NULL);
	if (ret) {
		pr_info("Failed to add child devices: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct of_device_id mt6685_id_table[] = {
	{ .compatible = "mediatek,mt6685", },
	{ }
};
MODULE_DEVICE_TABLE(of, mt6685_id_table);

static struct spmi_driver mt6685_spmi_driver = {
	.probe = mt6685_spmi_probe,
	.driver = {
		.name = "mt6685",
		.of_match_table = mt6685_id_table,
	},
};
module_spmi_driver(mt6685_spmi_driver);

MODULE_DESCRIPTION("Mediatek SPMI MT6685 Clock IC driver");
MODULE_AUTHOR("Hank Liao <hank.liao@mediatek.com>");
MODULE_LICENSE("GPL v2");
