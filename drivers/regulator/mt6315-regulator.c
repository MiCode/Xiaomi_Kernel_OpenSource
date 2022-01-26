// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/interrupt.h>
#include <linux/mfd/mt6315/registers.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/mt6315-misc.h>
#include <linux/regulator/mt6315-regulator.h>
#include <linux/regulator/of_regulator.h>

#define MT6315_REG_WIDTH		8

#define MT6315_BUCK_MODE_AUTO		0
#define MT6315_BUCK_MODE_FORCE_PWM	1
#define MT6315_BUCK_MODE_NORMAL		0
#define MT6315_BUCK_MODE_LP		2

/*
 * MT6315 irqs' information
 */
enum mt6315_irq_numbers {
	MT6315_IRQ_VBUCK1_OC = 0,
	MT6315_IRQ_VBUCK2_OC,
	MT6315_IRQ_VBUCK3_OC,
	MT6315_IRQ_VBUCK4_OC,
	MT6315_IRQ_TEMP_BACK_110D,
	MT6315_IRQ_TEMP_OVER_125D,
	MT6315_IRQ_RCS0,
	MT6315_IRQ_RCS1,
	MT6315_IRQ_NR,
};

struct pmic_irq_data {
	//unsigned int num_top;
	unsigned int num_pmic_irqs;
	unsigned int reg_width;
	//unsigned short top_int_status_reg;
	unsigned int *enable_hwirq;
	unsigned int *cache_hwirq;
	//struct irq_top_t *pmic_ints;
	unsigned int en_reg;
	unsigned int sta_reg;
};

/*
 * MT6315 regulators' information
 *
 * @desc: standard fields of regulator description.
 * @da_reg: for query status of regulators.
 * @qi: Mask for query enable signal status of regulators.
 * @modeset_reg: for operating AUTO/PWM mode register.
 * @modeset_mask: MASK for operating modeset register.
 */
struct mt6315_regulator_info {
	struct regulator_desc desc;
	struct regulation_constraints constraints;
	u32 da_vsel_reg;
	u32 da_vsel_mask;
	u32 da_vsel_shift;
	u32 da_reg;
	u32 qi;
	u32 modeset_reg;
	u32 modeset_mask;
	u32 lp_mode_reg;
	u32 lp_mode_mask;
	u32 lp_mode_shift;
};

/*
 * MTK regulators' init data
 *
 * @id: chip slave id
 * @size: num of regulators
 * @regulator_info: regulator info.
 */
struct mt_regulator_init_data {
	u32 id;
	u32 size;
	struct mt6315_regulator_info *regulator_info;
};

struct mt6315_chip {
	struct device *dev;
	struct regmap *regmap;
	int irq;
	struct irq_domain *irq_domain;
	struct mutex irqlock;
	void *irq_data;
	struct mutex lock;
	u32 reg_value;
	u32 slave_id;
};

/* MT6315 irqs function */
static void pmic_irq_enable(struct irq_data *data)
{
	unsigned int hwirq = irqd_to_hwirq(data);
	struct mt6315_chip *chip = irq_data_get_irq_chip_data(data);
	struct pmic_irq_data *irq_data = chip->irq_data;

	irq_data->enable_hwirq[hwirq] = 1;
}

static void pmic_irq_disable(struct irq_data *data)
{
	unsigned int hwirq = irqd_to_hwirq(data);
	struct mt6315_chip *chip = irq_data_get_irq_chip_data(data);
	struct pmic_irq_data *irq_data = chip->irq_data;

	irq_data->enable_hwirq[hwirq] = 0;
}

static void pmic_irq_lock(struct irq_data *data)
{
	struct mt6315_chip *chip = irq_data_get_irq_chip_data(data);

	mutex_lock(&chip->irqlock);
}

static void pmic_irq_sync_unlock(struct irq_data *data)
{
	unsigned int i, en_reg, shift;
	struct mt6315_chip *chip = irq_data_get_irq_chip_data(data);
	struct pmic_irq_data *irq_data = chip->irq_data;

	for (i = 0; i < irq_data->num_pmic_irqs; i++) {
		if (irq_data->enable_hwirq[i] ==
				irq_data->cache_hwirq[i])
			continue;

		en_reg = irq_data->en_reg;
		shift = i % irq_data->reg_width;
		regmap_update_bits(chip->regmap, en_reg, 0x1 << shift,
				   irq_data->enable_hwirq[i] << shift);
		irq_data->cache_hwirq[i] = irq_data->enable_hwirq[i];
	}
	mutex_unlock(&chip->irqlock);
}

static int pmic_irq_set_type(struct irq_data *data, unsigned int type)
{
	return 0;
}

static struct irq_chip mt6315_irq_chip = {
	.name = "mt6315-irq",
	.irq_enable = pmic_irq_enable,
	.irq_disable = pmic_irq_disable,
	.irq_bus_lock = pmic_irq_lock,
	.irq_bus_sync_unlock = pmic_irq_sync_unlock,
	.irq_set_type = pmic_irq_set_type,
};

static irqreturn_t mt6315_irq_handler(int irq, void *data)
{
	struct mt6315_chip *chip = data;
	struct pmic_irq_data *irq_data = chip->irq_data;
	unsigned int int_status = 0;
	unsigned int hwirq, virq;
	int ret;

	ret = regmap_read(chip->regmap,
			  irq_data->sta_reg,
			  &int_status);
	if (ret) {
		dev_notice(chip->dev, "Can't read INT_STATUS ret=%d\n", ret);
		return IRQ_NONE;
	}

	for (hwirq = 0; hwirq < MT6315_REG_WIDTH ; hwirq++) {
		if ((int_status & BIT(hwirq)) == 0)
			continue;
		virq = irq_find_mapping(chip->irq_domain, hwirq);
		dev_info(chip->dev,
			"Reg[0x%x]=0x%x,hwirq=%d,type=%d\n",
			irq_data->sta_reg, int_status, hwirq,
			irq_get_trigger_type(virq));
		if (virq)
			handle_nested_irq(virq);
	}

	regmap_write(chip->regmap, irq_data->sta_reg, int_status);

	return IRQ_HANDLED;
}

static int pmic_irq_domain_map(struct irq_domain *d, unsigned int irq,
			       irq_hw_number_t hw)
{
	struct mt6315_chip *chip = d->host_data;

	irq_set_chip_data(irq, chip);
	irq_set_chip_and_handler(irq, &mt6315_irq_chip, handle_level_irq);
	irq_set_nested_thread(irq, 1);
	irq_set_noprobe(irq);

	return 0;
}

static const struct irq_domain_ops mt6315_irq_domain_ops = {
	.map = pmic_irq_domain_map,
	.xlate = irq_domain_xlate_twocell,
};

static int mt6315_irq_init(struct mt6315_chip *chip)
{
	int i, ret;
	struct pmic_irq_data *irq_data;

	if (chip->irq <= 0) {
		dev_notice(chip->dev, "bypass %s", __func__);
		return 0;
	}

	irq_data = devm_kzalloc(chip->dev, sizeof(struct pmic_irq_data),
				GFP_KERNEL);
	if (!irq_data)
		return -ENOMEM;

	chip->irq_data = irq_data;

	mutex_init(&chip->irqlock);

	irq_data->num_pmic_irqs = MT6315_IRQ_NR;
	irq_data->reg_width = MT6315_REG_WIDTH;
	irq_data->en_reg = MT6315_PMIC_RG_INT_EN_VBUCK1_OC_ADDR;
	irq_data->sta_reg = MT6315_PMIC_RG_INT_STATUS_VBUCK1_OC_ADDR;

	irq_data->enable_hwirq = devm_kcalloc(chip->dev,
					      irq_data->num_pmic_irqs,
					      sizeof(unsigned int),
					      GFP_KERNEL);
	if (!irq_data->enable_hwirq)
		return -ENOMEM;

	irq_data->cache_hwirq = devm_kcalloc(chip->dev,
					     irq_data->num_pmic_irqs,
					     sizeof(unsigned int),
					     GFP_KERNEL);
	if (!irq_data->cache_hwirq)
		return -ENOMEM;

	/* Disable all interrupt for initializing */
	for (i = 0; i < irq_data->num_pmic_irqs; i++)
		regmap_write(chip->regmap, irq_data->en_reg, 0);

	chip->irq_domain = irq_domain_add_linear(chip->dev->of_node,
						 irq_data->num_pmic_irqs,
						 &mt6315_irq_domain_ops, chip);
	if (!chip->irq_domain) {
		dev_notice(chip->dev, "could not create irq domain\n");
		return -ENODEV;
	}

	ret = devm_request_threaded_irq(chip->dev, chip->irq, NULL,
					mt6315_irq_handler, IRQF_ONESHOT,
					dev_name(chip->dev), chip);
	if (ret) {
		dev_notice(chip->dev, "failed to register irq=%d; err: %d\n",
			chip->irq, ret);
		return ret;
	}

	enable_irq_wake(chip->irq);
	return ret;
}

#define MT_BUCK_EN		(REGULATOR_CHANGE_STATUS)
#define MT_BUCK_VOL		(REGULATOR_CHANGE_VOLTAGE)
#define MT_BUCK_VOL_EN		(REGULATOR_CHANGE_STATUS |	\
				 REGULATOR_CHANGE_VOLTAGE)
#define MT_BUCK_VOL_EN_MODE	(REGULATOR_CHANGE_STATUS |	\
				 REGULATOR_CHANGE_VOLTAGE |	\
				 REGULATOR_CHANGE_MODE)

#define MT_BUCK(match, _name, min, max, step, volt_ranges, _bid, mode,	\
		_modeset_mask)						\
[MT6315_ID_##_name] = {							\
	.desc = {							\
		.name = #_name,						\
		.of_match = of_match_ptr(match),			\
		.ops = &mt6315_volt_range_ops,				\
		.type = REGULATOR_VOLTAGE,				\
		.id = MT6315_ID_##_name,				\
		.owner = THIS_MODULE,					\
		.uV_step = (step),					\
		.linear_min_sel = (0x30),				\
		.n_voltages = (max) / (step) + 1,			\
		.min_uV = (min),					\
		.linear_ranges = volt_ranges,				\
		.n_linear_ranges = ARRAY_SIZE(volt_ranges),		\
		.vsel_reg = MT6315_PMIC_RG_BUCK_VBUCK##_bid##_VOSEL_ADDR,\
		.vsel_mask = \
			(MT6315_PMIC_RG_BUCK_VBUCK##_bid##_VOSEL_MASK << \
			MT6315_PMIC_RG_BUCK_VBUCK##_bid##_VOSEL_SHIFT),	\
		.enable_reg = MT6315_PMIC_RG_BUCK_VBUCK##_bid##_EN_ADDR,\
		.enable_mask = BIT(MT6315_PMIC_RG_BUCK_VBUCK##_bid##_EN_SHIFT),\
	},								\
	.constraints = {						\
		.valid_ops_mask = (mode),				\
		.valid_modes_mask =					\
			(REGULATOR_MODE_NORMAL |			\
			 REGULATOR_MODE_FAST |				\
			 REGULATOR_MODE_IDLE),				\
	},								\
	.da_vsel_reg = MT6315_PMIC_DA_VBUCK##_bid##_VOSEL_ADDR,		\
	.da_vsel_mask = MT6315_PMIC_DA_VBUCK##_bid##_VOSEL_MASK,	\
	.da_vsel_shift = MT6315_PMIC_DA_VBUCK##_bid##_VOSEL_SHIFT,	\
	.da_reg = MT6315_PMIC_DA_VBUCK##_bid##_EN_ADDR,			\
	.qi = BIT(0),							\
	.lp_mode_reg = MT6315_PMIC_RG_BUCK_VBUCK##_bid##_LP_ADDR,	\
	.lp_mode_mask = BIT(MT6315_PMIC_RG_BUCK_VBUCK##_bid##_LP_SHIFT),\
	.lp_mode_shift = MT6315_PMIC_RG_BUCK_VBUCK##_bid##_LP_SHIFT,	\
	.modeset_reg = MT6315_PMIC_RG_VBUCK##_bid##_FCCM_ADDR,		\
	.modeset_mask = _modeset_mask,					\
}

static const struct regulator_linear_range mt_volt_range1[] = {
	REGULATOR_LINEAR_RANGE(300000, 0x30, 0xbf, 6250),
};

static int mt6315_regulator_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	if (rdev->use_count == 0) {
		dev_notice(&rdev->dev, "%s:%s should not be disable.(use_count=0)\n"
			, __func__
			, rdev->desc->name);
		ret = -1;
	} else {
		ret = regulator_disable_regmap(rdev);
	}

	return ret;
}

static int mt6315_regulator_get_voltage_sel(struct regulator_dev *rdev)
{
	struct mt6315_regulator_info *info = rdev_get_drvdata(rdev);
	int ret = 0, regval = 0;

	ret = regmap_read(rdev->regmap, info->da_vsel_reg, &regval);
	if (ret != 0) {
		dev_notice(&rdev->dev,
			"Failed to get mt6315 regulator voltage: %d\n", ret);
		return ret;
	}

	ret = (regval >> info->da_vsel_shift) & info->da_vsel_mask;

	return ret;
}

static unsigned int mt6315_regulator_get_mode(struct regulator_dev *rdev)
{
	struct mt6315_regulator_info *info = rdev_get_drvdata(rdev);
	int ret = 0, regval = 0;

	ret = regmap_read(rdev->regmap, info->modeset_reg, &regval);
	if (ret != 0) {
		dev_notice(&rdev->dev,
			"Failed to get mt6315 buck mode: %d\n", ret);
		return ret;
	}

	if ((regval & info->modeset_mask) == info->modeset_mask)
		return REGULATOR_MODE_FAST;

	ret = regmap_read(rdev->regmap, info->lp_mode_reg, &regval);
	if (ret != 0) {
		dev_notice(&rdev->dev,
			"Failed to get mt6315 buck lp mode: %d\n", ret);
		return ret;
	}

	if (regval & info->lp_mode_mask)
		return REGULATOR_MODE_IDLE;
	else
		return REGULATOR_MODE_NORMAL;
}

static int mt6315_regulator_set_mode(struct regulator_dev *rdev,
				     unsigned int mode)
{
	struct mt6315_regulator_info *info = rdev_get_drvdata(rdev);
	int ret = 0, val;
	int curr_mode;

	curr_mode = mt6315_regulator_get_mode(rdev);
	switch (mode) {
	case REGULATOR_MODE_FAST:
		if (curr_mode == REGULATOR_MODE_IDLE) {
			WARN_ON(1);
			dev_notice(&rdev->dev,
				   "BUCK %s is LP mode, can't FPWM\n",
				   rdev->desc->name);
			return -EIO;
		}
		ret = regmap_update_bits(rdev->regmap,
					 info->modeset_reg,
					 info->modeset_mask,
					 info->modeset_mask);
		break;
	case REGULATOR_MODE_NORMAL:
		if (curr_mode == REGULATOR_MODE_FAST) {
			ret = regmap_update_bits(rdev->regmap,
						 info->modeset_reg,
						 info->modeset_mask,
						 0);
		} else if (curr_mode == REGULATOR_MODE_IDLE) {
			val = MT6315_BUCK_MODE_NORMAL;
			val <<= info->lp_mode_shift;
			ret = regmap_update_bits(rdev->regmap,
						 info->lp_mode_reg,
						 info->lp_mode_mask,
						 val);
			udelay(100);
		}
		break;
	case REGULATOR_MODE_IDLE:
		if (curr_mode == REGULATOR_MODE_FAST) {
			WARN_ON(1);
			dev_notice(&rdev->dev,
				   "BUCK %s is FPWM mode, can't enter LP\n",
				   rdev->desc->name);
			return -EIO;
		}
		val = MT6315_BUCK_MODE_LP >> 1;
		val <<= info->lp_mode_shift;
		ret = regmap_update_bits(rdev->regmap,
					 info->lp_mode_reg,
					 info->lp_mode_mask,
					 val);
		break;
	default:
		ret = -EINVAL;
		goto err_mode;
	}

err_mode:
	if (ret != 0) {
		dev_notice(&rdev->dev,
			"Failed to set mt6315 buck mode: %d\n", ret);
		return ret;
	}

	return 0;
}

static int mt6315_get_status(struct regulator_dev *rdev)
{
	int ret = 0;
	u32 regval = 0;
	struct mt6315_regulator_info *info = rdev_get_drvdata(rdev);

	ret = regmap_read(rdev->regmap, info->da_reg, &regval);
	if (ret != 0) {
		dev_notice(&rdev->dev, "Failed to get enable reg: %d\n", ret);
		return ret;
	}

	return (regval & info->qi) ? REGULATOR_STATUS_ON : REGULATOR_STATUS_OFF;
}

static const struct regulator_ops mt6315_volt_range_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = mt6315_regulator_get_voltage_sel,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = regulator_enable_regmap,
	.disable = mt6315_regulator_disable,
	.is_enabled = regulator_is_enabled_regmap,
	.get_status = mt6315_get_status,
	.set_mode = mt6315_regulator_set_mode,
	.get_mode = mt6315_regulator_get_mode,
};

/* The array is indexed by id(MT6315_ID_SID_XXX) */
static struct mt6315_regulator_info mt6315_6_regulators[] = {
#if defined(CONFIG_MACH_MT6893)
	MT_BUCK("6_vbuck1", 6_VBUCK1, 300000, 1193750, 6250,
		mt_volt_range1, 1, MT_BUCK_VOL_EN_MODE, 0xF),
#elif defined(CONFIG_MACH_MT6877)
	MT_BUCK("6_vbuck1", 6_VBUCK1, 300000, 1193750, 6250,
		mt_volt_range1, 1, MT_BUCK_VOL_EN_MODE, 0x3),
#else
	MT_BUCK("6_vbuck1", 6_VBUCK1, 300000, 1193750, 6250,
		mt_volt_range1, 1, MT_BUCK_VOL_EN_MODE, 0xB),
#endif
	MT_BUCK("6_vbuck3", 6_VBUCK3, 300000, 1193750, 6250,
		mt_volt_range1, 3, MT_BUCK_VOL_EN_MODE, 0x4),
#if defined(CONFIG_MACH_MT6877)
	MT_BUCK("6_vbuck4", 6_VBUCK4, 300000, 1193750, 6250,
		mt_volt_range1, 4, MT_BUCK_VOL_EN_MODE, 0x8),
#endif
};

static struct mt6315_regulator_info mt6315_7_regulators[] = {
#if defined(CONFIG_MACH_MT6885)
	MT_BUCK("7_vbuck1", 7_VBUCK1, 300000, 1193750, 6250,
		mt_volt_range1, 1, MT_BUCK_VOL_EN_MODE, 0xB),
#elif defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6893)
	MT_BUCK("7_vbuck1", 7_VBUCK1, 300000, 1193750, 6250,
		mt_volt_range1, 1, MT_BUCK_VOL_EN_MODE, 0x3),
#endif
	MT_BUCK("7_vbuck3", 7_VBUCK3, 300000, 1193750, 6250,
		mt_volt_range1, 3, MT_BUCK_VOL_EN, 0x4),
#if defined(CONFIG_MACH_MT6893)
	MT_BUCK("7_vbuck4", 7_VBUCK4, 300000, 1193750, 6250,
		mt_volt_range1, 4, MT_BUCK_VOL_EN, 0x8),
#endif
};

static struct mt6315_regulator_info mt6315_3_regulators[] = {
	MT_BUCK("3_vbuck1", 3_VBUCK1, 300000, 1193750, 6250,
		mt_volt_range1, 1, MT_BUCK_VOL_EN, 0x3),
	MT_BUCK("3_vbuck3", 3_VBUCK3, 300000, 1193750, 6250,
		mt_volt_range1, 3, MT_BUCK_VOL_EN, 0x4),
	MT_BUCK("3_vbuck4", 3_VBUCK4, 300000, 1193750, 6250,
		mt_volt_range1, 4, MT_BUCK_VOL_EN, 0x8),
};

static const struct mt_regulator_init_data mt6315_6_regulator_init_data = {
	.id = MT6315_SLAVE_ID_6,
	.size = MT6315_ID_6_MAX,
	.regulator_info = &mt6315_6_regulators[0],
};

static const struct mt_regulator_init_data mt6315_7_regulator_init_data = {
	.id = MT6315_SLAVE_ID_7,
	.size = MT6315_ID_7_MAX,
	.regulator_info = &mt6315_7_regulators[0],
};

static const struct mt_regulator_init_data mt6315_3_regulator_init_data = {
	.id = MT6315_SLAVE_ID_3,
	.size = MT6315_ID_3_MAX,
	.regulator_info = &mt6315_3_regulators[0],
};

static const struct of_device_id mt6315_of_match[] = {
	{
		.compatible = "mediatek,mt6315_6-regulator",
		.data = &mt6315_6_regulator_init_data,
	}, {
		.compatible = "mediatek,mt6315_7-regulator",
		.data = &mt6315_7_regulator_init_data,
	}, {
		.compatible = "mediatek,mt6315_3-regulator",
		.data = &mt6315_3_regulator_init_data,
	}, {
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, mt6315_of_match);

static ssize_t extbuck_access_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct mt6315_chip *chip = dev_get_drvdata(dev);

	pr_info("[%s] 0x%x\n", __func__, chip->reg_value);

	return sprintf(buf, "0x%x\n", chip->reg_value);
}

static ssize_t extbuck_access_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf,
				    size_t size)
{
	struct mt6315_chip *chip;
	int ret = 0;
	char *pvalue = NULL, *addr, *val;
	unsigned int sid = 0;
	unsigned int reg_val = 0;
	unsigned int reg_adr = 0;

	if (dev) {
		chip = dev_get_drvdata(dev);
		if (!chip)
			return -ENODEV;
	} else
		return -ENODEV;

	if (buf != NULL && size != 0) {
		pr_info("[%s] size is %d, buf is %s\n", __func__,
			(int)size, buf);

		pvalue = (char *)buf;
		addr = strsep(&pvalue, " ");
		val = strsep(&pvalue, " ");
		sid = chip->slave_id;
		if (addr)
			ret = kstrtou32(addr, 16, (unsigned int *)&reg_adr);
		if (val) {
			ret = kstrtou32(val, 16, (unsigned int *)&reg_val);
			pr_info("write MT6315_S%d Reg[0x%x] to 0x%x!\n",
				sid, reg_adr, reg_val);
			ret = regmap_write(chip->regmap, reg_adr, reg_val);
		} else {
			mutex_lock(&chip->lock);
			ret = regmap_read(chip->regmap,
					  reg_adr, &chip->reg_value);
			mutex_unlock(&chip->lock);
			pr_info("read MT6315_S%d Reg[0x%x]=0x%x!\n",
				sid, reg_adr, chip->reg_value);
		}
	}
	return size;
}
static DEVICE_ATTR_RW(extbuck_access);

/*
 * PMIC dump record register log
 */
static ssize_t dump_rec_pmic_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	struct mt6315_chip *chip;
	unsigned int rdata0 = 0, rdata1 = 0, rdata2 = 0, rdata3 = 0;
	unsigned int offset, sid;
	int ret, log_size = 0;

	if (dev) {
		chip = dev_get_drvdata(dev);
		if (!chip)
			return -ENODEV;
	} else
		return -ENODEV;
	sid = chip->slave_id;
	ret = regmap_read(chip->regmap, MT6315_SPMI_DEBUG_CMD0, &rdata0);
	log_size += sprintf(buf + log_size, "slvid:%d DBG. Last cmd idx:%d\n",
			    sid, (((rdata0 & 0xc) >> 2) + 3) % 4);
	/* log sequence, idx 0->1->2->3->0 */
	for (offset = MT6315_SPMI_DEBUG_ADDR0;
	     offset <= MT6315_SPMI_DEBUG_ADDR3; offset += 4) {
		ret = regmap_read(chip->regmap, offset, &rdata0);
		ret = regmap_read(chip->regmap, offset + 1, &rdata1);
		ret = regmap_read(chip->regmap, offset + 2, &rdata2);
		ret = regmap_read(chip->regmap, offset + 3, &rdata3);
		log_size += sprintf(buf + log_size,
				    "Idx:%d slvid:%d Type:0x%x, [0x%x]=0x%x\n",
				    (offset - MT6315_SPMI_DEBUG_ADDR0) / 4,
				    sid, (rdata3 & 0x3),
				    (rdata1 << 0x8) | rdata0, rdata2);
	}
	pr_info("\n[SPMISLV] %s", buf);

	return log_size;
}
static DEVICE_ATTR_RO(dump_rec_pmic);

static int mt6315_regulator_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_id;
	struct device *dev = &pdev->dev;
	struct regmap *regmap;
	struct mt6315_regulator_info *mt_regulators;
	struct mt_regulator_init_data *regulator_init_data;
	struct mt6315_chip *chip;
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	struct regulation_constraints *c;
	int i = 0, ret = 0;
	u32 reg_value = 0;

	regmap = dev_get_regmap(dev->parent, NULL);
	if (!regmap)
		return -ENODEV;

	chip = devm_kzalloc(&pdev->dev, sizeof(struct mt6315_chip),
			       GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	of_id = of_match_device(mt6315_of_match, &pdev->dev);
	if (!of_id || !of_id->data)
		return -ENODEV;

	regulator_init_data = (struct mt_regulator_init_data *)of_id->data;
	mt_regulators = regulator_init_data->regulator_info;
	chip->slave_id = regulator_init_data->id;
	chip->dev = &pdev->dev;
	chip->regmap = regmap;
	chip->irq = platform_get_irq(pdev, 0);
	if (chip->irq <= 0) {
		dev_notice(&pdev->dev,
			"failed to get platform irq, ret=%d", chip->irq);
	}
	mutex_init(&chip->lock);
	platform_set_drvdata(pdev, chip);

	/* Read chip revision to update constraints */
	if (regmap_read(regmap, MT6315_SWCID_H, &reg_value) < 0) {
		dev_notice(&pdev->dev, "Failed to read Chip ID\n");
		return -EIO;
	}
	dev_info(&pdev->dev, "Chip ID = 0x%x\n", reg_value);

	for (i = 0; i < regulator_init_data->size; i++) {
		config.dev = &pdev->dev;
		config.driver_data = (mt_regulators + i);
		config.regmap = regmap;
		rdev = devm_regulator_register(&pdev->dev,
				&(mt_regulators + i)->desc, &config);
		if (IS_ERR(rdev)) {
			dev_notice(&pdev->dev, "failed to register %s\n",
				(mt_regulators + i)->desc.name);
			continue;
		}

		c = rdev->constraints;
		c->valid_ops_mask |=
			(mt_regulators + i)->constraints.valid_ops_mask;
		c->valid_modes_mask |=
			(mt_regulators + i)->constraints.valid_modes_mask;
	}

	ret = mt6315_irq_init(chip);
	if (ret)
		return ret;
	mt6315_misc_init(regulator_init_data->id, regmap);
	/* Create sysfs entry */
	ret = device_create_file(&pdev->dev, &dev_attr_extbuck_access);
	if (ret)
		dev_notice(&pdev->dev, "failed to create regs access file\n");
	ret = device_create_file(&pdev->dev, &dev_attr_dump_rec_pmic);
	if (ret)
		dev_notice(&pdev->dev, "failed to create regs record file\n");

	return 0;
}

static void mt6315_regulator_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct regmap *regmap;
	int ret = 0;

	dev_info(&pdev->dev, "%s\n", __func__);
	regmap = dev_get_regmap(dev->parent, NULL);
	if (!regmap) {
		dev_notice(&pdev->dev, "%s: invalid regmap.\n", __func__);
		return;
	}

	ret |= regmap_write(regmap, MT6315_PMIC_TMA_KEY_H_ADDR, 0x9C);
	ret |= regmap_write(regmap, MT6315_PMIC_TMA_KEY_ADDR, 0xEA);
	ret |= regmap_update_bits(regmap,
		MT6315_PMIC_RG_TOP2_RSV2_ADDR,
		0x1 << 0, 0x1 << 0);
	ret |= regmap_write(regmap, MT6315_PMIC_TMA_KEY_ADDR, 0);
	ret |= regmap_write(regmap, MT6315_PMIC_TMA_KEY_H_ADDR, 0);
	if (ret < 0) {
		dev_notice(&pdev->dev, "%s: enable power off sequence failed.\n"
			   , __func__);
		return;
	}
}

static int mt6315_regulator_remove(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_extbuck_access);

	return 0;
}

static struct platform_driver mt6315_regulator_driver = {
	.driver		= {
		.name	= "mt6315-regulator",
		.of_match_table = of_match_ptr(mt6315_of_match),
	},
	.probe = mt6315_regulator_probe,
	.shutdown = mt6315_regulator_shutdown,
	.remove = mt6315_regulator_remove,
};

module_platform_driver(mt6315_regulator_driver);

MODULE_AUTHOR("Wen Su <wen.su@mediatek.com>");
MODULE_DESCRIPTION("Mediatek MT6315 regulator driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:mt6315-regulator");
