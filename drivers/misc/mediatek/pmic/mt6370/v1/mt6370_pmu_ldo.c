/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/of.h>
#include "inc/mt6370_pmu.h"

#define MT6370_PMU_LDO_DRV_VERSION	"1.0.1_MTK"

struct mt6370_ldo_regulator_struct {
	unsigned char vol_reg;
	unsigned char vol_mask;
	unsigned char vol_shift;
	unsigned char enable_reg;
	unsigned char enable_bit;
};

static struct mt6370_ldo_regulator_struct mt6370_ldo_regulators = {
	.vol_reg = MT6370_PMU_REG_LDOVOUT,
	.vol_mask = (0x0F),
	.vol_shift = (0),
	.enable_reg = MT6370_PMU_REG_LDOVOUT,
	.enable_bit = (1 << 7),
};

#define mt6370_ldo_min_uV (1600000)
#define mt6370_ldo_max_uV (4000000)
#define mt6370_ldo_step_uV (200000)
#define mt6370_ldo_id 0
#define mt6370_ldo_type REGULATOR_VOLTAGE

struct mt6370_pmu_ldo_data {
	struct regulator_desc *desc;
	struct regulator_dev *regulator;
	struct mt6370_pmu_chip *chip;
	struct device *dev;
};

struct mt6370_pmu_ldo_platform_data {
	uint8_t cfg;
};

static irqreturn_t mt6370_pmu_ldo_oc_irq_handler(int irq, void *data)
{
	pr_info("%s: IRQ triggered\n", __func__);
	return IRQ_HANDLED;
}

static struct mt6370_pmu_irq_desc mt6370_ldo_irq_desc[] = {
	MT6370_PMU_IRQDESC(ldo_oc),
};

static void mt6370_pmu_ldo_irq_register(struct platform_device *pdev)
{
	struct resource *res;
	int i, ret = 0;

	for (i = 0; i < ARRAY_SIZE(mt6370_ldo_irq_desc); i++) {
		if (!mt6370_ldo_irq_desc[i].name)
			continue;
		res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
				mt6370_ldo_irq_desc[i].name);
		if (!res)
			continue;
		ret = devm_request_threaded_irq(&pdev->dev, res->start, NULL,
				mt6370_ldo_irq_desc[i].irq_handler,
				IRQF_TRIGGER_FALLING,
				mt6370_ldo_irq_desc[i].name,
				platform_get_drvdata(pdev));
		if (ret < 0) {
			dev_err(&pdev->dev, "request %s irq fail\n", res->name);
			continue;
		}
		mt6370_ldo_irq_desc[i].irq = res->start;
	}
}

static int mt6370_ldo_list_voltage(struct regulator_dev *rdev,
		unsigned int selector)
{
	int vout = 0;

	vout = mt6370_ldo_min_uV + selector * mt6370_ldo_step_uV;
	if (vout > mt6370_ldo_max_uV)
		return -EINVAL;
	return vout;
}

static int mt6370_ldo_set_voltage_sel(
		struct regulator_dev *rdev, unsigned int selector)
{
	struct mt6370_pmu_ldo_data *info = rdev_get_drvdata(rdev);
	const int count = rdev->desc->n_voltages;
	u8 data;

	if (selector > count)
		return -EINVAL;

	data = (u8)selector;
	data <<= mt6370_ldo_regulators.vol_shift;

	return mt6370_pmu_reg_update_bits(info->chip,
		mt6370_ldo_regulators.vol_reg,
		mt6370_ldo_regulators.vol_mask, data);
}

static int mt6370_ldo_get_voltage_sel(struct regulator_dev *rdev)
{
	struct mt6370_pmu_ldo_data *info = rdev_get_drvdata(rdev);
	int ret;

	ret = mt6370_pmu_reg_read(info->chip,
		mt6370_ldo_regulators.vol_reg);

	if (ret < 0)
		return ret;

	return (ret&mt6370_ldo_regulators.vol_mask)>>
		mt6370_ldo_regulators.vol_shift;
}

static int mt6370_ldo_enable(struct regulator_dev *rdev)
{
	struct mt6370_pmu_ldo_data *info = rdev_get_drvdata(rdev);

	return mt6370_pmu_reg_set_bit(info->chip,
		mt6370_ldo_regulators.enable_reg,
		mt6370_ldo_regulators.enable_bit);
}

static int mt6370_ldo_disable(struct regulator_dev *rdev)
{
	struct mt6370_pmu_ldo_data *info = rdev_get_drvdata(rdev);

	return mt6370_pmu_reg_clr_bit(info->chip,
		mt6370_ldo_regulators.enable_reg,
		mt6370_ldo_regulators.enable_bit);
}

static int mt6370_ldo_is_enabled(struct regulator_dev *rdev)
{
	struct mt6370_pmu_ldo_data *info = rdev_get_drvdata(rdev);
	int ret;

	ret = mt6370_pmu_reg_read(info->chip,
		mt6370_ldo_regulators.enable_reg);
	if (ret < 0)
		return ret;
	return ret&mt6370_ldo_regulators.enable_bit ? 1 : 0;
}

static struct regulator_ops mt6370_ldo_regulator_ops = {
	.list_voltage = mt6370_ldo_list_voltage,
	.set_voltage_sel = mt6370_ldo_set_voltage_sel,
	.get_voltage_sel = mt6370_ldo_get_voltage_sel,
	.enable = mt6370_ldo_enable,
	.disable = mt6370_ldo_disable,
	.is_enabled = mt6370_ldo_is_enabled,
};

static struct regulator_desc mt6370_ldo_regulator_desc = {
	.id = 0,
	.name = "mt6370_ldo",
	.n_voltages = 13,
	.ops = &mt6370_ldo_regulator_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
};

static inline struct regulator_dev *mt6370_ldo_regulator_register(
		struct regulator_desc *desc, struct device *dev,
		struct regulator_init_data *init_data, void *driver_data)
{
	struct regulator_config config = {
		.dev = dev,
		.init_data = init_data,
		.driver_data = driver_data,
	};
	return regulator_register(desc, &config);
}

static inline int mt_parse_dt(struct device *dev,
		struct mt6370_pmu_ldo_platform_data *pdata,
		struct mt6370_pmu_ldo_platform_data *mask)
{
	struct device_node *np = dev->of_node;
	uint32_t val = 0;

	if (of_property_read_u32(np, "ldo_oms", &val) == 0) {
		mask->cfg |= (0x1  <<  6);
		pdata->cfg |= (val  <<  6);
	}

	mask->cfg |= (0x01 << 5);
	if (of_property_read_u32(np, "ldo_vrc", &val) == 0) {
		mask->cfg |= (0x3  <<  1);
		pdata->cfg |= (val  <<  1);
		pdata->cfg |= (1  <<  5);
	}

	if (of_property_read_u32(np, "ldo_vrc_lt", &val) == 0) {
		mask->cfg = 0x3  <<  3;
		pdata->cfg = val  <<  3;
	}
	return 0;
}

static struct regulator_init_data *mt_parse_init_data(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct device_node *sub_np;
	struct regulator_init_data *init_data;

	sub_np = of_get_child_by_name(np, "mt6370_ldo");
	if (!sub_np) {
		dev_err(dev, "no mt6370_ldo sub node\n");
		return NULL;
	}
	init_data = of_get_regulator_init_data(dev, sub_np, NULL);
	if (init_data) {
		dev_info(dev,
			"regulator_name = %s, min_uV = %d, max_uV = %d\n",
			init_data->constraints.name,
			init_data->constraints.min_uV,
			init_data->constraints.max_uV);
	} else {
		dev_err(dev, "no init data mt6370_init\n");
		return NULL;
	}
	return init_data;
}

static int ldo_apply_dts(struct mt6370_pmu_chip *chip,
		struct mt6370_pmu_ldo_platform_data *pdata,
		struct mt6370_pmu_ldo_platform_data *mask)
{
	return mt6370_pmu_reg_update_bits(chip, MT6370_PMU_REG_LDOCFG,
			mask->cfg, pdata->cfg);
}

static int mt6370_pmu_ldo_probe(struct platform_device *pdev)
{
	struct mt6370_pmu_ldo_data *ldo_data;
	struct regulator_init_data *init_data = NULL;
	bool use_dt = pdev->dev.of_node;
	struct mt6370_pmu_ldo_platform_data pdata, mask;
	int ret;

	pr_info("%s: (%s)\n", __func__, MT6370_PMU_LDO_DRV_VERSION);

	ldo_data = devm_kzalloc(&pdev->dev, sizeof(*ldo_data), GFP_KERNEL);
	if (!ldo_data)
		return -ENOMEM;

	memset(&pdata, 0, sizeof(pdata));
	memset(&mask, 0, sizeof(mask));
	if (use_dt)
		mt_parse_dt(&pdev->dev, &pdata, &mask);

	init_data = mt_parse_init_data(&pdev->dev);
	if (init_data == NULL) {
		dev_err(&pdev->dev, "no init data\n");
		return -EINVAL;
	}

	ldo_data->chip = dev_get_drvdata(pdev->dev.parent);
	ldo_data->dev = &pdev->dev;
	ldo_data->desc = &mt6370_ldo_regulator_desc;
	platform_set_drvdata(pdev, ldo_data);

	ret = ldo_apply_dts(ldo_data->chip, &pdata, &mask);
	if (ret < 0)
		goto probe_err;

	ldo_data->regulator = mt6370_ldo_regulator_register(ldo_data->desc,
			&pdev->dev, init_data, ldo_data);
	if (IS_ERR(ldo_data->regulator)) {
		dev_err(&pdev->dev, "fail to register ldo regulator %s\n",
			ldo_data->desc->name);
		goto probe_err;
	}

	mt6370_pmu_ldo_irq_register(pdev);

	dev_info(&pdev->dev, "%s successfully\n", __func__);
	return 0;
probe_err:
	dev_info(&pdev->dev, "%s: register mtk regulator failed\n", __func__);
	return ret;
}

static int mt6370_pmu_ldo_remove(struct platform_device *pdev)
{
	struct mt6370_pmu_ldo_data *ldo_data = platform_get_drvdata(pdev);

	dev_info(ldo_data->dev, "%s successfully\n", __func__);
	return 0;
}

static const struct of_device_id mt_ofid_table[] = {
	{ .compatible = "mediatek,mt6370_pmu_ldo", },
	{ },
};
MODULE_DEVICE_TABLE(of, mt_ofid_table);

static const struct platform_device_id mt_id_table[] = {
	{ "mt6370_pmu_ldo", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, mt_id_table);

static struct platform_driver mt6370_pmu_ldo = {
	.driver = {
		.name = "mt6370_pmu_ldo",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mt_ofid_table),
	},
	.probe = mt6370_pmu_ldo_probe,
	.remove = mt6370_pmu_ldo_remove,
	.id_table = mt_id_table,
};
module_platform_driver(mt6370_pmu_ldo);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek MT6370 PMU Vib LDO");
MODULE_VERSION(MT6370_PMU_LDO_DRV_VERSION);

/*
 * Release Note
 * 1.0.1_MTK
 * (1) Remove force OSC on/off for enable/disable LDO
 *
 * 1.0.0_MTK
 * Initial release
 */
