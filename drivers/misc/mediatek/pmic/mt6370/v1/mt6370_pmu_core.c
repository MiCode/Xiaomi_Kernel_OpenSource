/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/delay.h>

#include "inc/mt6370_pmu.h"
#include "inc/mt6370_pmu_core.h"

#define MT6370_PMU_CORE_DRV_VERSION	"1.0.1_MTK"

struct mt6370_pmu_core_data {
	struct mt6370_pmu_chip *chip;
	struct device *dev;
};

static irqreturn_t mt6370_pmu_otp_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_core_data *core_data = data;

	dev_err(core_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_vdda_ovp_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_core_data *core_data = data;

	dev_err(core_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_vdda_uv_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_core_data *core_data = data;

	dev_err(core_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static struct mt6370_pmu_irq_desc mt6370_core_irq_desc[] = {
	MT6370_PMU_IRQDESC(otp),
	MT6370_PMU_IRQDESC(vdda_ovp),
	MT6370_PMU_IRQDESC(vdda_uv),
};

static void mt6370_pmu_core_irq_register(struct platform_device *pdev)
{
	struct resource *res;
	int i, ret = 0;

	for (i = 0; i < ARRAY_SIZE(mt6370_core_irq_desc); i++) {
		if (!mt6370_core_irq_desc[i].name)
			continue;
		res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
						mt6370_core_irq_desc[i].name);
		if (!res)
			continue;
		ret = devm_request_threaded_irq(&pdev->dev, res->start, NULL,
					mt6370_core_irq_desc[i].irq_handler,
					IRQF_TRIGGER_FALLING,
					mt6370_core_irq_desc[i].name,
					platform_get_drvdata(pdev));
		if (ret < 0) {
			dev_err(&pdev->dev, "request %s irq fail\n", res->name);
			continue;
		}
		mt6370_core_irq_desc[i].irq = res->start;
	}
}

static inline int mt6370_pmu_core_init_register(
	struct mt6370_pmu_core_data *core_data)
{
	struct mt6370_pmu_core_platdata *pdata =
					dev_get_platdata(core_data->dev);
	uint8_t reg_data = 0, reg_mask = 0;
	int ret = 0;

	reg_mask |= MT6370_I2CRST_ENMASK;
	reg_data |= (pdata->i2cstmr_rst_en << MT6370_I2CRST_ENSHFT);
	reg_mask |= MT6370_I2CRST_TMRMASK;
	reg_data |= (pdata->i2cstmr_rst_tmr << MT6370_I2CRST_TMRSHFT);
	reg_mask |= MT6370_MRSTB_ENMASK;
	reg_data |= (pdata->mrstb_en << MT6370_MRSTB_ENSHFT);
	reg_mask |= MT6370_MRSTB_TMRMASK;
	reg_data |= (pdata->mrstb_tmr << MT6370_MRSTB_TMRSHFT);
	ret = mt6370_pmu_reg_update_bits(core_data->chip,
					 MT6370_PMU_REG_CORECTRL1, reg_mask,
					 reg_data);
	if (ret < 0)
		return ret;

	reg_data = reg_mask = 0;
	reg_mask |= MT6370_INTWDT_TMRMASK;
	reg_data |= (pdata->int_wdt << MT6370_INTWDT_TMRSHFT);
	reg_mask |= MT6370_INTDEG_TIMEMASK;
	reg_data |= (pdata->int_deg << MT6370_INTDEG_TIMESHFT);
	ret = mt6370_pmu_reg_update_bits(core_data->chip, MT6370_PMU_REG_IRQSET,
					 reg_mask, reg_data);
	if (ret < 0)
		return ret;
	return 0;
}

static inline int mt_parse_dt(struct device *dev)
{
	struct mt6370_pmu_core_platdata *pdata = dev_get_platdata(dev);
	struct device_node *np = dev->of_node;
	u32 tmp = 0;

	if (of_property_read_bool(np, "i2cstmr_rst_en"))
		pdata->i2cstmr_rst_en = 1;
	if (of_property_read_u32(np, "i2cstmr_rst_tmr", &tmp) < 0)
		pdata->i2cstmr_rst_tmr = 0;
	else
		pdata->i2cstmr_rst_tmr = tmp;
	if (of_property_read_bool(np, "mrstb_en"))
		pdata->mrstb_en = 1;
	if (of_property_read_u32(np, "mrstb_tmr", &tmp) < 0)
		pdata->mrstb_tmr = 0;
	else
		pdata->mrstb_tmr = tmp;
	if (of_property_read_u32(np, "int_wdt", &tmp) < 0)
		pdata->int_wdt = 0;
	else
		pdata->int_wdt = tmp;
	if (of_property_read_u32(np, "int_deg", &tmp) < 0)
		pdata->int_deg = 0;
	else
		pdata->int_deg = tmp;
	return 0;
}

static int mt6370_pmu_core_reset(struct mt6370_pmu_core_data *core_data)
{
	const u8 pascode[2] = {0xC5, 0x7E};
	u8 chip_vid = core_data->chip->chip_vid;
	int ret = 0;

	dev_info(core_data->dev, "%s\n", __func__);

	if (chip_vid == MT6372_VENDOR_ID || chip_vid == MT6372C_VENDOR_ID)
		return 0;

	ret = mt6370_pmu_reg_write(core_data->chip,
				   MT6370_PMU_REG_RSTPASCODE1, 0xA9);
	if (ret < 0)
		dev_err(core_data->dev, "set passcode1 fail\n");
	ret = mt6370_pmu_reg_write(core_data->chip,
				   MT6370_PMU_REG_RSTPASCODE2, 0x96);
	if (ret < 0)
		dev_err(core_data->dev, "set passcode2 fail\n");
	/* reset chg/fled/ldo/rgb/bl/dsv logic and all pmu register */
	ret = mt6370_pmu_reg_write(core_data->chip,
				   MT6370_PMU_REG_CORECTRL2, 0x7F);
	if (ret < 0)
		dev_err(core_data->dev, "reset all reg/logic fail\n");
	mdelay(1);
	ret = rt_regmap_cache_reload(core_data->chip->rd);
	if (ret < 0)
		dev_err(core_data->dev, "cache reload fail\n");
	ret = mt6370_pmu_reg_block_write(core_data->chip,
					 MT6370_PMU_REG_RSTPASCODE1,
					 2, pascode);
	if (ret < 0)
		dev_err(core_data->dev, "excute reset pascode fail\n");
	/* add dsvp discharge bit */
	return mt6370_pmu_reg_write(core_data->chip,
				    MT6370_PMU_REG_DBCTRL2, 0x32);
}

static int mt6370_pmu_core_probe(struct platform_device *pdev)
{
	struct mt6370_pmu_core_platdata *pdata = dev_get_platdata(&pdev->dev);
	struct mt6370_pmu_core_data *core_data;
	bool use_dt = pdev->dev.of_node;
	int ret = 0;

	pr_info("%s: (%s)\n", __func__, MT6370_PMU_CORE_DRV_VERSION);

	core_data = devm_kzalloc(&pdev->dev, sizeof(*core_data), GFP_KERNEL);
	if (!core_data)
		return -ENOMEM;
	if (use_dt) {
		/* DTS used */
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			ret = -ENOMEM;
			goto out_pdata;
		}
		pdev->dev.platform_data = pdata;
		ret = mt_parse_dt(&pdev->dev);
		if (ret < 0) {
			devm_kfree(&pdev->dev, pdata);
			goto out_pdata;
		}
	} else {
		if (!pdata) {
			ret = -EINVAL;
			goto out_pdata;
		}
	}
	core_data->chip = dev_get_drvdata(pdev->dev.parent);
	core_data->dev = &pdev->dev;
	platform_set_drvdata(pdev, core_data);

	ret = mt6370_pmu_core_init_register(core_data);
	if (ret < 0)
		goto out_init_reg;

	mt6370_pmu_core_irq_register(pdev);
	dev_info(&pdev->dev, "%s successfully\n", __func__);
	return 0;
out_init_reg:
out_pdata:
	devm_kfree(&pdev->dev, core_data);
	return ret;
}

static int mt6370_pmu_core_remove(struct platform_device *pdev)
{
	struct mt6370_pmu_core_data *core_data = platform_get_drvdata(pdev);

	dev_info(core_data->dev, "%s successfully\n", __func__);
	return 0;
}

static void mt6370_pmu_core_shutdown(struct platform_device *pdev)
{
	struct mt6370_pmu_core_data *core_data = platform_get_drvdata(pdev);
	int ret;

	ret = mt6370_pmu_core_reset(core_data);
	if (ret < 0)
		dev_err(core_data->dev, "pmu core reset fail\n");
}

static const struct of_device_id mt_ofid_table[] = {
	{ .compatible = "mediatek,mt6370_pmu_core", },
	{ },
};
MODULE_DEVICE_TABLE(of, mt_ofid_table);

static const struct platform_device_id mt_id_table[] = {
	{ "mt6370_pmu_core", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, mt_id_table);

static struct platform_driver mt6370_pmu_core = {
	.driver = {
		.name = "mt6370_pmu_core",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mt_ofid_table),
	},
	.probe = mt6370_pmu_core_probe,
	.remove = mt6370_pmu_core_remove,
	.shutdown = mt6370_pmu_core_shutdown,
	.id_table = mt_id_table,
};
module_platform_driver(mt6370_pmu_core);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek MT6370 PMU Core");
MODULE_VERSION(MT6370_PMU_CORE_DRV_VERSION);

/*
 * Release Note
 * 1.0.1_MTK
 * (1) Do not run mt6370_pmu_core_reset() for MT6372
 *
 * 1.0.0_MTK
 * (1) Initial Release
 */
