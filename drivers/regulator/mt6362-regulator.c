// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/interrupt.h>
#include <dt-bindings/mfd/mt6362.h>

#define MT6362_REG_BUCK1_SEQOFFDLY      (0x107)
#define MT6362_POFF_SEQ_MAX	9

/* Set sub-pmic buck1/2/3/4/5/6&ldo7/6/4 power off seuqence */
static const u8 def_pwr_off_seq[MT6362_POFF_SEQ_MAX] = {
	0x24, 0x24, 0x04, 0x22, 0x00, 0x00, 0x00, 0x02, 0x04,
};

enum {
	MT6362_IDX_BUCK1 = 0,
	MT6362_IDX_BUCK2,
	MT6362_IDX_BUCK3,
	MT6362_IDX_BUCK4,
	MT6362_IDX_BUCK5,
	MT6362_IDX_BUCK6,
	MT6362_IDX_LDO1,
	MT6362_IDX_LDO2,
	MT6362_IDX_LDO3,
	MT6362_IDX_LDO4,
	MT6362_IDX_LDO5,
	MT6362_IDX_LDO6,
	MT6362_IDX_LDO7,
	MT6362_IDX_MAX,
};

#define MT6362_VOL_REGMASK		(0xff)
#define MT6362_VOL_MAX			(256)

#define LDOVOL_OFFSET			(0xA)
#define LDOENA_OFFSET			(0x6)
#define LDOADE_OFFSET			(0x9)
#define LDOMOD_OFFSET			(0x6)
#define MT6362_LDOENA_REGMASK		BIT(7)
#define MT6362_LDOMOD_REGMASK		BIT(6)
#define MT6362_LDOADE_REGMASK		BIT(2)
#define MT6362_LDO1_BASE		(0x310)
#define MT6362_LDO2_BASE		(0x320)
#define MT6362_LDO3_BASE		(0x330)
#define MT6362_LDO4_BASE		(0x140)
#define MT6362_LDO5_BASE		(0x350)
#define MT6362_LDO6_BASE		(0x130)
#define MT6362_LDO7_BASE		(0x120)
#define MT6362_LDOVOL_REGADDR(_id)	(MT6362_LDO##_id##_BASE + LDOVOL_OFFSET)
#define MT6362_LDOENA_REGADDR(_id)	(MT6362_LDO##_id##_BASE + LDOENA_OFFSET)
#define MT6362_LDOADE_REGADDR(_id)	(MT6362_LDO##_id##_BASE + LDOADE_OFFSET)
#define MT6362_LDOMOD_REGADDR(_id)	(MT6362_LDO##_id##_BASE + LDOMOD_OFFSET)

#define MT6362_BUCKVOL_REGBASE		(0x70C)
#define MT6362_BUCKVOL_REGADDR(_id)	(MT6362_BUCKVOL_REGBASE + _id - 1)
#define MT6362_BUCKENA_REGADDR		(0x700)
#define MT6362_BUCKMOD_REGADDR		(0x706)
#define MT6362_VOLFBD2_MASK		BIT(7)

#define MT6362_DEFAULT_BUCKSTEP		(6250)
#define MT6362_VOLFBD2_BUCKSTEP		(8333)

struct mt6362_regulator_desc {
	struct regulator_desc desc;
	unsigned int mode_reg;
	unsigned int mode_mask;
};

struct mt6362_regulator_irqt {
	const char *name;
	irq_handler_t irqh;
};

struct mt6362_regulator_data {
	struct device *dev;
	struct regmap *regmap;
	u8 pwr_off_seq[MT6362_POFF_SEQ_MAX];
};

static int mt6362_enable_poweroff_sequence(struct mt6362_regulator_data *data)
{
	int i, ret;

	dev_dbg(data->dev, "%s\n", __func__);
	for (i = 0; i < MT6362_POFF_SEQ_MAX; i++) {
		ret = regmap_write(data->regmap,
				   MT6362_REG_BUCK1_SEQOFFDLY + i,
				   data->pwr_off_seq[i]);
		if (ret < 0) {
			dev_notice(data->dev, "%s: set buck(%d) fail\n",
				   __func__, i);
			return ret;
		}
	}
	return ret;
}

static int mt6362_general_set_active_discharge(struct regulator_dev *rdev,
					       bool enable)
{
	const struct regulator_desc *desc = rdev->desc;

	if (!desc->active_discharge_reg)
		return 0;

	return regulator_set_active_discharge_regmap(rdev, enable);
}

static int mt6362_general_set_mode(struct regulator_dev *rdev,
				   unsigned int mode)
{
	struct mt6362_regulator_desc *mdesc =
			(struct mt6362_regulator_desc *)rdev->desc;
	int ret;

	switch (mode) {
	case REGULATOR_MODE_NORMAL:
		ret = regmap_update_bits(rdev->regmap, mdesc->mode_reg,
					 mdesc->mode_mask, 0);
		break;
	case REGULATOR_MODE_IDLE:
		ret = regmap_update_bits(rdev->regmap, mdesc->mode_reg,
					 mdesc->mode_mask, mdesc->mode_mask);
		break;
	default:
		ret = -EINVAL;
	}

	return ret ? : 0;
}

static unsigned int mt6362_general_get_mode(struct regulator_dev *rdev)
{
	struct mt6362_regulator_desc *mdesc =
			(struct mt6362_regulator_desc *)rdev->desc;
	unsigned int val = 0;
	int ret;

	ret = regmap_read(rdev->regmap, mdesc->mode_reg, &val);
	if (ret)
		return 0;
	if (val & mdesc->mode_mask)
		return REGULATOR_MODE_IDLE;

	return REGULATOR_MODE_NORMAL;
}

static unsigned int mt6362_general_of_map_mode(unsigned int mode)
{
	switch (mode) {
	case MT6362_REGULATOR_MODE_LP:
		return REGULATOR_MODE_IDLE;
	case MT6362_REGULATOR_MODE_NORMAL:
		return REGULATOR_MODE_NORMAL;
	default:
		return REGULATOR_MODE_INVALID;
	}
}

static const struct regulator_ops mt6362_ldo_regulator_ops = {
	.list_voltage		= regulator_list_voltage_linear_range,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.is_enabled		= regulator_is_enabled_regmap,
	.set_active_discharge	= mt6362_general_set_active_discharge,
	.set_mode		= mt6362_general_set_mode,
	.get_mode		= mt6362_general_get_mode,
};

static const struct regulator_ops mt6362_buck_regulator_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.is_enabled		= regulator_is_enabled_regmap,
	.set_active_discharge	= mt6362_general_set_active_discharge,
	.set_mode		= mt6362_general_set_mode,
	.get_mode		= mt6362_general_get_mode,
};

static const struct regulator_linear_range lvldo_ranges[] = {
	REGULATOR_LINEAR_RANGE(500000, 0x00, 0x0a, 10000),
	REGULATOR_LINEAR_RANGE(600000, 0x0b, 0x0f, 0),
	REGULATOR_LINEAR_RANGE(600000, 0x10, 0x1a, 10000),
	REGULATOR_LINEAR_RANGE(700000, 0x1b, 0x1f, 0),
	REGULATOR_LINEAR_RANGE(700000, 0x20, 0x2a, 10000),
	REGULATOR_LINEAR_RANGE(800000, 0x2b, 0x2f, 0),
	REGULATOR_LINEAR_RANGE(800000, 0x30, 0x3a, 10000),
	REGULATOR_LINEAR_RANGE(900000, 0x3b, 0x3f, 0),
	REGULATOR_LINEAR_RANGE(900000, 0x40, 0x4a, 10000),
	REGULATOR_LINEAR_RANGE(1000000, 0x4b, 0x4f, 0),
	REGULATOR_LINEAR_RANGE(1000000, 0x50, 0x5a, 10000),
	REGULATOR_LINEAR_RANGE(1100000, 0x5b, 0x5f, 0),
	REGULATOR_LINEAR_RANGE(1100000, 0x60, 0x6a, 10000),
	REGULATOR_LINEAR_RANGE(1200000, 0x6b, 0x6f, 0),
	REGULATOR_LINEAR_RANGE(1200000, 0x70, 0x7a, 10000),
	REGULATOR_LINEAR_RANGE(1300000, 0x7b, 0x7f, 0),
	REGULATOR_LINEAR_RANGE(1300000, 0x80, 0x8a, 10000),
	REGULATOR_LINEAR_RANGE(1400000, 0x8b, 0x8f, 0),
	REGULATOR_LINEAR_RANGE(1400000, 0x90, 0x9a, 10000),
	REGULATOR_LINEAR_RANGE(1500000, 0x9b, 0x9f, 0),
	REGULATOR_LINEAR_RANGE(1500000, 0xa0, 0xaa, 10000),
	REGULATOR_LINEAR_RANGE(1600000, 0xab, 0xaf, 0),
	REGULATOR_LINEAR_RANGE(1600000, 0xb0, 0xba, 10000),
	REGULATOR_LINEAR_RANGE(1700000, 0xbb, 0xbf, 0),
	REGULATOR_LINEAR_RANGE(1700000, 0xc0, 0xca, 10000),
	REGULATOR_LINEAR_RANGE(1800000, 0xcb, 0xcf, 0),
	REGULATOR_LINEAR_RANGE(1800000, 0xd0, 0xda, 10000),
	REGULATOR_LINEAR_RANGE(1900000, 0xdb, 0xdf, 0),
	REGULATOR_LINEAR_RANGE(1900000, 0xe0, 0xea, 10000),
	REGULATOR_LINEAR_RANGE(2000000, 0xeb, 0xef, 0),
	REGULATOR_LINEAR_RANGE(2000000, 0xf0, 0xfa, 10000),
	REGULATOR_LINEAR_RANGE(2100000, 0xfb, 0xff, 0),
};

static const struct regulator_linear_range hvldo_ranges[] = {
	REGULATOR_LINEAR_RANGE(1200000, 0x00, 0x0a, 10000),
	REGULATOR_LINEAR_RANGE(1300000, 0x0b, 0x0f, 0),
	REGULATOR_LINEAR_RANGE(1300000, 0x10, 0x1a, 10000),
	REGULATOR_LINEAR_RANGE(1400000, 0x1b, 0x1f, 0),
	REGULATOR_LINEAR_RANGE(1500000, 0x20, 0x2a, 10000),
	REGULATOR_LINEAR_RANGE(1600000, 0x2b, 0x2f, 0),
	REGULATOR_LINEAR_RANGE(1700000, 0x30, 0x3a, 10000),
	REGULATOR_LINEAR_RANGE(1800000, 0x3b, 0x3f, 0),
	REGULATOR_LINEAR_RANGE(1800000, 0x40, 0x4a, 10000),
	REGULATOR_LINEAR_RANGE(1900000, 0x4b, 0x4f, 0),
	REGULATOR_LINEAR_RANGE(2000000, 0x50, 0x5a, 10000),
	REGULATOR_LINEAR_RANGE(2100000, 0x5b, 0x5f, 0),
	REGULATOR_LINEAR_RANGE(2100000, 0x60, 0x6a, 10000),
	REGULATOR_LINEAR_RANGE(2200000, 0x6b, 0x6f, 0),
	REGULATOR_LINEAR_RANGE(2500000, 0x70, 0x7a, 10000),
	REGULATOR_LINEAR_RANGE(2600000, 0x7b, 0x7f, 0),
	REGULATOR_LINEAR_RANGE(2700000, 0x80, 0x8a, 10000),
	REGULATOR_LINEAR_RANGE(2800000, 0x8b, 0x8f, 0),
	REGULATOR_LINEAR_RANGE(2800000, 0x90, 0x9a, 10000),
	REGULATOR_LINEAR_RANGE(2900000, 0x9b, 0x9f, 0),
	REGULATOR_LINEAR_RANGE(2900000, 0xa0, 0xaa, 10000),
	REGULATOR_LINEAR_RANGE(3000000, 0xab, 0xaf, 0),
	REGULATOR_LINEAR_RANGE(3000000, 0xb0, 0xba, 10000),
	REGULATOR_LINEAR_RANGE(3100000, 0xbb, 0xbf, 0),
	REGULATOR_LINEAR_RANGE(3100000, 0xc0, 0xca, 10000),
	REGULATOR_LINEAR_RANGE(3200000, 0xcb, 0xcf, 0),
	REGULATOR_LINEAR_RANGE(3300000, 0xd0, 0xda, 10000),
	REGULATOR_LINEAR_RANGE(3400000, 0xdb, 0xdf, 0),
	REGULATOR_LINEAR_RANGE(3400000, 0xe0, 0xea, 10000),
	REGULATOR_LINEAR_RANGE(3500000, 0xeb, 0xef, 0),
	REGULATOR_LINEAR_RANGE(3500000, 0xf0, 0xfa, 10000),
	REGULATOR_LINEAR_RANGE(3600000, 0xfb, 0xff, 0),
};

#define MT6362_LDO_DESC(_id, _type, _supply_name) \
{\
	.desc = {\
		.ops			= &mt6362_ldo_regulator_ops,\
		.name			= "mt6362-ldo" #_id,\
		.supply_name		= _supply_name,\
		.of_match		= "ldo" #_id,\
		.id			= MT6362_IDX_LDO##_id,\
		.type			= REGULATOR_VOLTAGE,\
		.owner			= THIS_MODULE,\
		.n_voltages		= MT6362_VOL_MAX,\
		.linear_ranges		= _type##ldo_ranges,\
		.n_linear_ranges	= ARRAY_SIZE(_type##ldo_ranges),\
		.vsel_reg		= MT6362_LDOVOL_REGADDR(_id),\
		.vsel_mask		= MT6362_VOL_REGMASK,\
		.enable_reg		= MT6362_LDOENA_REGADDR(_id),\
		.enable_mask		= MT6362_LDOENA_REGMASK,\
		.active_discharge_reg	= MT6362_LDOADE_REGADDR(_id),\
		.active_discharge_mask	= MT6362_LDOADE_REGMASK,\
		.active_discharge_on	= MT6362_LDOADE_REGMASK,\
		.of_map_mode		= mt6362_general_of_map_mode,\
	},\
	.mode_reg		= MT6362_LDOMOD_REGADDR(_id),\
	.mode_mask		= MT6362_LDOMOD_REGMASK,\
}

#define MT6362_BUCK_DESC(_id) \
{\
	.desc = {\
		.ops			= &mt6362_buck_regulator_ops,\
		.name			= "mt6362-buck" #_id,\
		.of_match		= "buck" #_id,\
		.id			= MT6362_IDX_BUCK##_id,\
		.type			= REGULATOR_VOLTAGE,\
		.owner			= THIS_MODULE,\
		.uV_step		= MT6362_DEFAULT_BUCKSTEP,\
		.n_voltages		= MT6362_VOL_MAX,\
		.vsel_reg		= MT6362_BUCKVOL_REGADDR(_id),\
		.vsel_mask		= MT6362_VOL_REGMASK,\
		.enable_reg		= MT6362_BUCKENA_REGADDR,\
		.enable_mask		= BIT(_id),\
		.of_map_mode		= mt6362_general_of_map_mode,\
	},\
	.mode_reg		= MT6362_BUCKMOD_REGADDR,\
	.mode_mask		= BIT(_id),\
}

static struct mt6362_regulator_desc mtreg_descs[MT6362_IDX_MAX] = {
	MT6362_BUCK_DESC(1),
	MT6362_BUCK_DESC(2),
	MT6362_BUCK_DESC(3),
	MT6362_BUCK_DESC(4),
	MT6362_BUCK_DESC(5),
	MT6362_BUCK_DESC(6),
	MT6362_LDO_DESC(1, hv, NULL),
	MT6362_LDO_DESC(2, hv, NULL),
	MT6362_LDO_DESC(3, hv, NULL),
	MT6362_LDO_DESC(4, hv, NULL),
	MT6362_LDO_DESC(5, hv, NULL),
	MT6362_LDO_DESC(6, lv, "mt6362-buck3"),
	MT6362_LDO_DESC(7, lv, "mt6362-buck3"),
};

#define MT6362_REGULATOR_IRQH(_name, _event) \
static irqreturn_t  mt6362_##_name##_irq_handler(int irq, void *data)\
{\
	struct regulator_dev *rdev = data;\
	struct device *dev = rdev_get_dev(rdev);\
	dev_warn(dev, "%s: id = %d\n", __func__, rdev_get_id(rdev));\
	regulator_notifier_call_chain(rdev, _event, NULL);\
	return IRQ_HANDLED;\
}

MT6362_REGULATOR_IRQH(oc_evt, REGULATOR_EVENT_OVER_CURRENT)
MT6362_REGULATOR_IRQH(pgb_evt, REGULATOR_EVENT_FAIL)

#define MT6362_IRQ_DECLARE(_name) \
{\
	.name = #_name,\
	.irqh = mt6362_##_name##_irq_handler,\
}

static const struct mt6362_regulator_irqt irqts[] = {
	MT6362_IRQ_DECLARE(oc_evt),
	MT6362_IRQ_DECLARE(pgb_evt),
};

static int mt6362_regulator_irq_register(struct regulator_dev *rdev)
{
	struct device *dev = rdev_get_dev(rdev);
	struct device_node *np = dev->of_node;
	int i, irq, rv;

	if (dev == NULL)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(irqts); i++) {
		irq = of_irq_get_byname(np, irqts[i].name);
		if (irq <= 0)
			continue;

		rv = devm_request_threaded_irq(dev->parent, irq, NULL,
					       irqts[i].irqh, 0, NULL, rdev);
		if (rv)
			return rv;
	}

	return 0;
}

static int mt6362_reconfigure_voltage_step(struct mt6362_regulator_data *data,
					   struct regulator_desc *desc)
{
	const unsigned int buck_fbd2_regs[] = {
		0x29b, 0x2a3, 0x2ab, 0x2b3, 0x2d1, 0x2d9
	};
	unsigned int val = 0;
	int rv;

	if (desc->id >= MT6362_IDX_BUCK1 && desc->id <= MT6362_IDX_BUCK6) {
		rv = regmap_read(data->regmap, buck_fbd2_regs[desc->id], &val);
		if (rv)
			return rv;

		if (val & MT6362_VOLFBD2_MASK)
			desc->uV_step = MT6362_VOLFBD2_BUCKSTEP;
	}

	return 0;
}

static int mt6362_parse_dt_data(struct device *dev,
				struct mt6362_regulator_data *data)
{
	struct device_node *np = dev->of_node;
	int ret;

	memcpy(data->pwr_off_seq, &def_pwr_off_seq, MT6362_POFF_SEQ_MAX);
	ret = of_property_read_u8_array(np, "pwr_off_seq", data->pwr_off_seq,
					MT6362_POFF_SEQ_MAX);
	if (ret)
		dev_notice(dev, "%s: undefine pwr_off_seq\n", __func__);
	return ret;
}

static int mt6362_regulator_probe(struct platform_device *pdev)
{
	struct mt6362_regulator_data *data;
	struct regulator_config config = { };
	struct regulator_dev *rdev;
	int i, rv;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	rv = mt6362_parse_dt_data(&pdev->dev, data);
	if (rv < 0) {
		dev_err(&pdev->dev, "parse dt fail\n");
		return rv;
	}

	data->dev = &pdev->dev;

	data->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!data->regmap) {
		dev_err(&pdev->dev, "failed to allocate regmap\n");
		return -ENODEV;
	}

	config.dev = &pdev->dev;
	config.driver_data = data;
	config.regmap = data->regmap;

	for (i = 0; i < MT6362_IDX_MAX; i++) {
		struct mt6362_regulator_desc *mt_desc;

		mt_desc = mtreg_descs + i;
		rv = mt6362_reconfigure_voltage_step(data, &mt_desc->desc);
		if (rv)
			return rv;

		rdev = devm_regulator_register(&pdev->dev,
					       &mt_desc->desc, &config);
		if (IS_ERR(rdev)) {
			dev_err(&pdev->dev,
				"failed to register [%d] regulator\n", i);
			return PTR_ERR(rdev);
		}

		rv = mt6362_regulator_irq_register(rdev);
		if (rv) {
			dev_err(&pdev->dev,
				"failed to register [%d] regulator irq\n", i);
			return rv;
		}
	}

	platform_set_drvdata(pdev, data);

	return 0;
}

static void mt6362_shutdown(struct platform_device *pdev)
{
	struct mt6362_regulator_data *data = platform_get_drvdata(pdev);
	int ret;

	dev_dbg(data->dev, "%s\n", __func__);
	if (data == NULL)
		return;
	ret = mt6362_enable_poweroff_sequence(data);
	if (ret < 0)
		dev_notice(data->dev,
			   "%s: enable power off sequence fail\n", __func__);
}

static const struct of_device_id __maybe_unused mt6362_regulator_ofid_tbls[] = {
	{ .compatible = "mediatek,mt6362-regulator", },
	{ },
};
MODULE_DEVICE_TABLE(of, mt6362_regulator_ofid_tbls);

static struct platform_driver mt6362_regulator_driver = {
	.driver = {
		.name = "mt6362-regulator",
		.of_match_table = of_match_ptr(mt6362_regulator_ofid_tbls),
	},
	.probe = mt6362_regulator_probe,
	.shutdown = mt6362_shutdown,
};
module_platform_driver(mt6362_regulator_driver);

MODULE_AUTHOR("ChiYuan Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6362 SPMI Regulator Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
