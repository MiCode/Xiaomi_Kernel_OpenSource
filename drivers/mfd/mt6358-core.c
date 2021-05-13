/*
 * Copyright (C) 2018 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/interrupt.h>
#include <linux/mfd/core.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/regmap.h>
#include <linux/wakeup_reason.h>
#if defined(CONFIG_MTK_PMIC_CHIP_MT6357)
#include <linux/mfd/mt6357/irq.h>
#include <linux/mfd/mt6357/registers.h>
#elif defined(CONFIG_MTK_PMIC_CHIP_MT6358)
#include <linux/mfd/mt6358/irq.h>
#include <linux/mfd/mt6358/registers.h>
#elif defined(CONFIG_MTK_PMIC_CHIP_MT6359)
#include <linux/mfd/mt6359/irq.h>
#include <linux/mfd/mt6359/registers.h>
#elif defined(CONFIG_MTK_PMIC_CHIP_MT6359P)
#include <linux/mfd/mt6359p/irq.h>
#include <linux/mfd/mt6359p/registers.h>
#elif defined(CONFIG_MTK_PMIC_CHIP_MT6390)
#include <linux/mfd/mt6390/irq.h>
#include <linux/mfd/mt6390/registers.h>
#endif
#include <linux/mfd/mt6358/core.h>

#define MT6357_CID_CODE		0x5700
#define MT6358_CID_CODE		0x5800
#define MT6359_CID_CODE		0x5900
#define MT6366_CID_CODE		0x6600
#define MT6390_CID_CODE		0x9000

static const struct mfd_cell mt6357_devs[] = {
	{
		.name = "mt-pmic",
		.of_compatible = "mediatek,mt-pmic",
	}, {
		.name = "mt635x-auxadc",
		.of_compatible = "mediatek,mt6357-auxadc",
	}, {
		.name = "mt6357-rtc",
		.of_compatible = "mediatek,mt6357-rtc",
	}, {
		.name = "mt6357-misc",
		.of_compatible = "mediatek,mt6357-misc",
	}, {
		.name = "mt6357-dcxo",
		.of_compatible = "mediatek,mt6357-dcxo",
	},
};

static const struct mfd_cell mt6358_devs[] = {
	{
		.name = "mt-pmic",
		.of_compatible = "mediatek,mt-pmic",
	}, {
		.name = "mt635x-auxadc",
		.of_compatible = "mediatek,mt6358-auxadc",
	}, {
		.name = "mt6358-regulator",
		.of_compatible = "mediatek,mt6358-regulator",
	}, {
		.name = "mt6358-rtc",
		.of_compatible = "mediatek,mt6358-rtc",
	}, {
		.name = "mt6358-misc",
		.of_compatible = "mediatek,mt6358-misc",
	}, {
		.name = "pmic-oc-debug",
		.of_compatible = "mediatek,pmic-oc-debug",
	},
};

static const struct mfd_cell mt6359_devs[] = {
	{
		.name = "mt-pmic",
		.of_compatible = "mediatek,mt-pmic",
	}, {
		.name = "mt635x-auxadc",
		.of_compatible = "mediatek,mt6359-auxadc",
	}, {
		.name = "mt6359-regulator",
		.of_compatible = "mediatek,mt6359-regulator",
	}, {
		.name = "mt6359p-regulator",
		.of_compatible = "mediatek,mt6359p-regulator",
	}, {
		.name = "mt6359-rtc",
		.of_compatible = "mediatek,mt6359-rtc",
	}, {
		.name = "mt6359-misc",
		.of_compatible = "mediatek,mt6359-misc",
	}, {
		.name = "mt6359p-misc",
		.of_compatible = "mediatek,mt6359p-misc",
	}, {
		.name = "mt635x-ot-debug",
		.of_compatible = "mediatek,mt635x-ot-debug",
	}, {
		.name = "pmic-oc-debug",
		.of_compatible = "mediatek,pmic-oc-debug",
	},
};

struct sp_top_t {
	int hwirq_base;
	unsigned int num_int_regs;
	unsigned int en_reg;
	unsigned int mask_reg;
	unsigned int sta_reg;
	unsigned int raw_sta_reg;
	unsigned int top_offset;
};

struct pmic_irq_t {
	const char *name;
	int hwirq;
	struct sp_top_t *sp_top;
};

static struct sp_top_t sp_top_ints[] = {
	SP_TOP_GEN(BUCK),
	SP_TOP_GEN(LDO),
	SP_TOP_GEN(PSC),
	SP_TOP_GEN(SCK),
	SP_TOP_GEN(BM),
	SP_TOP_GEN(HK),
	SP_TOP_GEN(AUD),
	SP_TOP_GEN(MISC),
};

static struct pmic_irq_t *pmic_irqs;

unsigned int mt6358_irq_get_virq(struct device *dev, unsigned int hwirq)
{
	struct mt6358_chip *chip = dev_get_drvdata(dev);

	return irq_create_mapping(chip->irq_domain, hwirq);
}
EXPORT_SYMBOL(mt6358_irq_get_virq);

const char *mt6358_irq_get_name(struct device *dev, unsigned int hwirq)
{
	struct mt6358_chip *chip = dev_get_drvdata(dev);

	if (hwirq >= chip->num_pmic_irqs)
		return NULL;
	return pmic_irqs[hwirq].name;
}
EXPORT_SYMBOL(mt6358_irq_get_name);

static void mt6358_irq_sp_handler(struct mt6358_chip *chip,
				  unsigned int sp)
{
	unsigned int sta_reg, sp_int_status = 0;
	unsigned int hwirq, virq;
	int ret, i, j;

	for (i = 0; i < sp_top_ints[sp].num_int_regs; i++) {
		sta_reg = sp_top_ints[sp].sta_reg + 0x2 * i;
		ret = regmap_read(chip->regmap, sta_reg, &sp_int_status);
		if (ret) {
			dev_notice(chip->dev,
				"Failed to read irq status: %d\n", ret);
			return;
		}
		if (!sp_int_status)
			continue;
		for (j = 0; j < 16 ; j++) {
			if ((sp_int_status & BIT(j)) == 0)
				continue;
			hwirq = sp_top_ints[sp].hwirq_base + 16 * i + j;
			virq = irq_find_mapping(chip->irq_domain, hwirq);
			dev_info(chip->dev,
				"Reg[0x%x]=0x%x,name=%s,hwirq=%d,type=%d\n",
				sta_reg, sp_int_status,
				pmic_irqs[hwirq].name, hwirq,
				irq_get_trigger_type(virq));
			if (!strncmp(pmic_irqs[hwirq].name, "chrdet_edge", 11)) {
				regmap_write(chip->regmap, sta_reg, BIT(j));
				sp_int_status &= ~BIT(j);
			}
			log_threaded_irq_wakeup_reason(virq, chip->irq);
			if (virq)
				handle_nested_irq(virq);
		}
		regmap_write(chip->regmap, sta_reg, sp_int_status);
	}
}

static irqreturn_t mt6358_irq_handler(int irq, void *data)
{
	struct mt6358_chip *chip = data;
	unsigned int top_int_status = 0;
	unsigned int i;
	int ret;

	pm_stay_awake(chip->dev);
	ret = regmap_read(chip->regmap,
			  chip->top_int_status_reg,
			  &top_int_status);
	if (ret) {
		dev_notice(chip->dev, "Can't read TOP_INT_STATUS ret=%d\n",
			ret);
		return IRQ_NONE;
	}
	for (i = 0; i < chip->num_sps; i++) {
		if (top_int_status & BIT(sp_top_ints[i].top_offset))
			mt6358_irq_sp_handler(chip, i);
	}
	pm_relax(chip->dev);
	return IRQ_HANDLED;
}

static void mt6358_irq_enable(struct irq_data *data)
{
	unsigned int en_reg, int_regs;
	unsigned int hwirq = irqd_to_hwirq(data);
	struct sp_top_t *sp_top = pmic_irqs[hwirq].sp_top;
	struct mt6358_chip *chip = irq_data_get_irq_chip_data(data);

	int_regs = (hwirq - sp_top->hwirq_base) / 16;
	en_reg = sp_top->en_reg + 0x6 * int_regs;
#if defined(CONFIG_MTK_PMIC_CHIP_MT6359) || \
defined(CONFIG_MTK_PMIC_CHIP_MT6359P)
	/* PMIC MT6359 miss LDO_INT_CON1_SET/CLR, use LDO_INT_CON1 */
	if (en_reg == MT6359_LDO_TOP_INT_CON1)
		regmap_update_bits(chip->regmap,
				   en_reg, BIT(hwirq % 16), BIT(hwirq % 16));
	else
		regmap_write(chip->regmap, en_reg + 0x2, 0x1 << (hwirq % 16));
#else
	regmap_write(chip->regmap, en_reg + 0x2, 0x1 << (hwirq % 16));
#endif
}

static void mt6358_irq_disable(struct irq_data *data)
{
	unsigned int en_reg, int_regs;
	unsigned int hwirq = irqd_to_hwirq(data);
	struct sp_top_t *sp_top = pmic_irqs[hwirq].sp_top;
	struct mt6358_chip *chip = irq_data_get_irq_chip_data(data);

	int_regs = (hwirq - sp_top->hwirq_base) / 16;
	en_reg = sp_top->en_reg + 0x6 * int_regs;
#if defined(CONFIG_MTK_PMIC_CHIP_MT6359) || \
defined(CONFIG_MTK_PMIC_CHIP_MT6359P)
	/* PMIC MT6359 miss LDO_INT_CON1_SET/CLR, use LDO_INT_CON1 */
	if (en_reg == MT6359_LDO_TOP_INT_CON1)
		regmap_update_bits(chip->regmap,
				   en_reg, BIT(hwirq % 16), 0);
	else
		regmap_write(chip->regmap, en_reg + 0x4, 0x1 << (hwirq % 16));
#else
	regmap_write(chip->regmap, en_reg + 0x4, 0x1 << (hwirq % 16));
#endif
}

static void mt6358_irq_lock(struct irq_data *data)
{
	struct mt6358_chip *chip = irq_data_get_irq_chip_data(data);

	mutex_lock(&chip->irqlock);
}

static void mt6358_irq_sync_unlock(struct irq_data *data)
{
	struct mt6358_chip *chip = irq_data_get_irq_chip_data(data);

	mutex_unlock(&chip->irqlock);
}

static int mt6358_irq_set_type(struct irq_data *data, unsigned int type)
{
	return 0;
}

static struct irq_chip mt6358_irq_chip = {
	.name = "mt6358-irq",
	.irq_enable = mt6358_irq_enable,
	.irq_disable = mt6358_irq_disable,
	.irq_bus_lock = mt6358_irq_lock,
	.irq_bus_sync_unlock = mt6358_irq_sync_unlock,
	.irq_set_type = mt6358_irq_set_type,
};

static int mt6358_irq_domain_map(struct irq_domain *d, unsigned int irq,
				 irq_hw_number_t hwirq)
{
	struct mt6358_chip *chip = d->host_data;

	irq_set_chip_data(irq, chip);
	irq_set_chip_and_handler(irq, &mt6358_irq_chip, handle_level_irq);
	irq_set_nested_thread(irq, 1);
	irq_set_noprobe(irq);

	return 0;
}

static const struct irq_domain_ops mt6358_irq_domain_ops = {
	.map = mt6358_irq_domain_map,
	.xlate = irq_domain_xlate_twocell,
};

static int mt6358_irq_init(struct mt6358_chip *chip)
{
	int i, ret = 0;

	if (chip->irq <= 0) {
		dev_notice(chip->dev,
			   "failed to get platform irq, ret=%d", chip->irq);
		return 0;
	}
	mutex_init(&chip->irqlock);
	ret = of_property_read_u32(chip->dev->of_node,
				   "mediatek,num-pmic-irqs",
				   &chip->num_pmic_irqs);
	if (ret) {
		dev_notice(chip->dev, "no number of pmic_irqs\n");
		return -EINVAL;
	}
	chip->num_sps = ARRAY_SIZE(sp_top_ints);
	pmic_irqs = devm_kcalloc(chip->dev, chip->num_pmic_irqs,
				 sizeof(struct pmic_irq_t), GFP_KERNEL);
	for (i = 0; i < chip->num_pmic_irqs; i++) {
		const char *name = NULL;
		unsigned int hwirq = 0, sp = 0, num_int_bits;
		struct pmic_irq_t *pmic_irq;

		ret = of_property_read_string_index(chip->dev->of_node,
					      "interrupt-names",
					      i, &name);
		if (ret < 0)
			break;
		ret = of_property_read_u32_index(chip->dev->of_node,
						 "mediatek,pmic-irqs",
						 i * 2 + 0, &hwirq);
		ret |= of_property_read_u32_index(chip->dev->of_node,
						 "mediatek,pmic-irqs",
						 i * 2 + 1, &sp);
		if (ret < 0) {
			dev_notice(chip->dev, "%s missing pmic-irqs\n", name);
			break;
		} else if (sp >= chip->num_sps) {
			dev_notice(chip->dev, "%s has invalid sp %d\n",
				   name, sp);
			break;
		}
		pmic_irq = pmic_irqs + hwirq;
		pmic_irq->name = name;
		pmic_irq->hwirq = hwirq;
		pmic_irq->sp_top = &sp_top_ints[sp];
		num_int_bits = sp_top_ints[sp].num_int_regs * 16;
		if (pmic_irq->sp_top->hwirq_base == -1)
			pmic_irq->sp_top->hwirq_base = round_down(hwirq, 16);
		else if (hwirq - pmic_irq->sp_top->hwirq_base >= num_int_bits)
			pmic_irq->sp_top->num_int_regs++;
#if 0
		dev_info(chip->dev,
			"name:%s, hwirq:%d, sp_top:%d, hwirq_base:%d, num_int_regs:%d, ret:%d\n"
			, pmic_irq->name, pmic_irq->hwirq,
			sp, pmic_irq->sp_top->hwirq_base,
			pmic_irq->sp_top->num_int_regs, ret);
#endif

	}

	/* Disable all interrupt for initializing */
	for (i = 0; i < chip->num_sps; i++) {
		int j;

		for (j = 0; j < sp_top_ints[i].num_int_regs; j++)
			regmap_write(chip->regmap,
				     sp_top_ints[i].en_reg + 0x6 * j, 0);
	}
	chip->irq_domain = irq_domain_add_linear(chip->dev->of_node,
						 chip->num_pmic_irqs,
						 &mt6358_irq_domain_ops,
						 chip);
	if (!chip->irq_domain) {
		dev_notice(chip->dev, "could not create irq domain\n");
		return -ENODEV;
	}

	ret = devm_request_threaded_irq(chip->dev, chip->irq, NULL,
					mt6358_irq_handler,
					IRQF_ONESHOT,
					mt6358_irq_chip.name, chip);
	if (ret) {
		dev_notice(chip->dev, "failed to register irq=%d; err: %d\n",
			chip->irq, ret);
		return 0;
	}
	enable_irq_wake(chip->irq);

	return ret;
}

static struct mt6358_chip *mt6358_pm_off;
static void mt6358_power_off(void)
{
	pr_info("%s\n", __func__);
	if (mt6358_pm_off)
		regmap_update_bits(mt6358_pm_off->regmap,
				PMIC_RG_PWRHOLD_ADDR,
				PMIC_RG_PWRHOLD_MASK << PMIC_RG_PWRHOLD_SHIFT,
				0 << PMIC_RG_PWRHOLD_SHIFT);
}

static int mt6358_probe(struct platform_device *pdev)
{
	int ret;
	unsigned int id = 0;
	struct mt6358_chip *chip;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &pdev->dev;
	chip->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!chip->regmap)
		return -ENODEV;

	chip->irq = platform_get_irq(pdev, 0);
	if (chip->irq <= 0) {
		dev_notice(&pdev->dev,
			"failed to get platform irq, ret=%d", chip->irq);
	}

	platform_set_drvdata(pdev, chip);

	ret = regmap_read(chip->regmap, PMIC_HWCID_ADDR, &id);
	if (ret) {
		dev_notice(chip->dev, "Failed to read chip id: %d\n", ret);
		return ret;
	}
	dev_info(chip->dev, "PMIC irq=%d, PMIC HWCID=0x%x, ret=%d\n",
		 chip->irq, id, ret);

	mt6358_pm_off = chip;
	pm_power_off = mt6358_power_off;

	switch (id & 0xFF00) {
	case MT6357_CID_CODE:
	case MT6390_CID_CODE:
		chip->top_int_status_reg = PMIC_INT_STATUS_TOP_RSV_ADDR;
		ret = mt6358_irq_init(chip);
		if (ret)
			return ret;
		ret = devm_mfd_add_devices(&pdev->dev, -1, mt6357_devs,
					   ARRAY_SIZE(mt6357_devs), NULL,
					   0, chip->irq_domain);
		break;
	case MT6358_CID_CODE:
	case MT6366_CID_CODE:
		chip->top_int_status_reg = PMIC_INT_STATUS_TOP_RSV_ADDR;
		ret = mt6358_irq_init(chip);
		if (ret)
			return ret;
		ret = devm_mfd_add_devices(&pdev->dev, -1, mt6358_devs,
					   ARRAY_SIZE(mt6358_devs), NULL,
					   0, chip->irq_domain);
		break;
	case MT6359_CID_CODE:
		chip->top_int_status_reg = PMIC_INT_STATUS_TOP_RSV_ADDR;
		ret = mt6358_irq_init(chip);
		if (ret)
			return ret;
		ret = devm_mfd_add_devices(&pdev->dev, -1, mt6359_devs,
					   ARRAY_SIZE(mt6359_devs), NULL,
					   0, NULL);
		break;
	default:
		dev_notice(&pdev->dev, "unsupported chip: %d\n", id);
		ret = -ENODEV;
		break;
	}

	if (ret) {
		irq_domain_remove(chip->irq_domain);
		dev_notice(&pdev->dev, "failed to add child devices: %d\n",
			ret);
	}

	return ret;
}

static const struct of_device_id mt6358_of_match[] = {
	{
		.compatible = "mediatek,mt6357-pmic",
	}, {
		.compatible = "mediatek,mt6358-pmic",
	}, {
		.compatible = "mediatek,mt6359-pmic",
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, mt6358_of_match);

static struct platform_driver mt6358_driver = {
	.probe = mt6358_probe,
	.driver = {
		.name = "mt6358-pmic",
		.of_match_table = of_match_ptr(mt6358_of_match),
	},
};

static int __init mt6358_init(void)
{
	pr_info("%s!!\n", __func__);
	return platform_driver_register(&mt6358_driver);
}
fs_initcall(mt6358_init);

MODULE_AUTHOR("Jeter Chen <Jeter.Chen@mediatek.com>");
MODULE_DESCRIPTION("Driver for MediaTek MT6358 PMIC");
MODULE_LICENSE("GPL v2");
