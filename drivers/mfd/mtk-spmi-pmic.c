// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Author: Argus Lin <argus.lin@mediatek.com>
 */

#include <linux/interrupt.h>
#include <linux/mfd/core.h>
#include <linux/mfd/mt6363/core.h>
#include <linux/mfd/mt6363/registers.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/regmap.h>
#include <linux/spmi.h>

#define MTK_SPMI_PMIC_REG_WIDTH	8
#define PMIC_SWCID		0xB
#define RCS_INT_DONE		0x41B

struct irq_top_t {
	int hwirq_base;
	unsigned int num_int_regs;
	unsigned int en_reg;
	unsigned int en_reg_shift;
	unsigned int sta_reg;
	unsigned int sta_reg_shift;
	unsigned int top_offset;
};

struct mtk_spmi_pmic_data {
	const struct mfd_cell *cells;
	int cell_size;
	unsigned int num_top;
	unsigned int num_pmic_irqs;
	unsigned short top_int_status_reg;
	struct irq_top_t *pmic_ints;
};

struct pmic_core {
	struct device *dev;
	struct spmi_device *sdev;
	struct regmap *regmap;
	u16 chip_id;
	int irq;
	bool *enable_hwirq;
	bool *cache_hwirq;
	struct mutex irqlock;
	struct irq_domain *irq_domain;
	const struct mtk_spmi_pmic_data *chip_data;
};

static const struct resource mt6363_keys_resources[] = {
	DEFINE_RES_IRQ(MT6363_IRQ_PWRKEY),
	DEFINE_RES_IRQ(MT6363_IRQ_HOMEKEY),
	DEFINE_RES_IRQ(MT6363_IRQ_PWRKEY_R),
	DEFINE_RES_IRQ(MT6363_IRQ_HOMEKEY_R),
};

static const struct mfd_cell mt6363_devs[] = {
	{
		.name = "mt6363-auxadc",
		.of_compatible = "mediatek,mt6363-auxadc",
	}, {
		.name = "mt6363-efuse",
		.of_compatible = "mediatek,mt6363-efuse",
	},
	{ .name = "mt6363-regulator", },
	{
		.name = "mt-pmic",
		.of_compatible = "mediatek,spmi-pmic-debug",
	}, {
		.name = "mtk-pmic-keys",
		.num_resources = ARRAY_SIZE(mt6363_keys_resources),
		.resources = mt6363_keys_resources,
		.of_compatible = "mediatek,mt6363-keys"
	},
};

static struct irq_top_t mt6363_ints[] = {
	MT6363_TOP_GEN(BUCK),
	MT6363_TOP_GEN(LDO),
	MT6363_TOP_GEN(PSC),
	MT6363_TOP_GEN(MISC),
	MT6363_TOP_GEN(HK),
	MT6363_TOP_GEN(BM),
};

static const struct mtk_spmi_pmic_data common_data = {
	.num_pmic_irqs = 0,
};

static const struct mtk_spmi_pmic_data mt6363_data = {
	.cells = mt6363_devs,
	.cell_size = ARRAY_SIZE(mt6363_devs),
	.num_top = ARRAY_SIZE(mt6363_ints),
	.num_pmic_irqs = MT6363_IRQ_NR,
	.top_int_status_reg = MT6363_TOP_INT_STATUS1,
	.pmic_ints = mt6363_ints,
};

static void mtk_spmi_pmic_irq_enable(struct irq_data *data)
{
	unsigned int hwirq = irqd_to_hwirq(data);
	struct pmic_core *core = irq_data_get_irq_chip_data(data);

	core->enable_hwirq[hwirq] = true;
}

static void mtk_spmi_pmic_irq_disable(struct irq_data *data)
{
	unsigned int hwirq = irqd_to_hwirq(data);
	struct pmic_core *core = irq_data_get_irq_chip_data(data);

	core->enable_hwirq[hwirq] = false;
}

static void mtk_spmi_pmic_irq_lock(struct irq_data *data)
{
	struct pmic_core *core = irq_data_get_irq_chip_data(data);

	mutex_lock(&core->irqlock);
}

static void mtk_spmi_pmic_irq_sync_unlock(struct irq_data *data)
{
	unsigned int i, top_gp, gp_offset, en_reg, int_regs, shift;
	struct irq_top_t *pmic_int;
	struct pmic_core *core = irq_data_get_irq_chip_data(data);
	const struct mtk_spmi_pmic_data *chip_data = core->chip_data;

	for (i = 0; i < chip_data->num_pmic_irqs; i++) {
		if (core->enable_hwirq[i] == core->cache_hwirq[i])
			continue;

		/* Find out the IRQ group */
		top_gp = 0;
		while ((top_gp + 1) < chip_data->num_top &&
			i >= chip_data->pmic_ints[top_gp + 1].hwirq_base)
			top_gp++;

		pmic_int = &(chip_data->pmic_ints[top_gp]);
		/* Find the IRQ registers */
		gp_offset = i - pmic_int->hwirq_base;
		int_regs = gp_offset / MTK_SPMI_PMIC_REG_WIDTH;
		shift = gp_offset % MTK_SPMI_PMIC_REG_WIDTH;
		en_reg = pmic_int->en_reg + (pmic_int->en_reg_shift * int_regs);

		regmap_update_bits(core->regmap, en_reg, BIT(shift),
				   core->enable_hwirq[i] << shift);
		core->cache_hwirq[i] = core->enable_hwirq[i];
	}
	mutex_unlock(&core->irqlock);
}

static struct irq_chip mtk_spmi_pmic_irq_chip = {
	.name = "spmi-pmic-irq",
	.flags = IRQCHIP_SKIP_SET_WAKE,
	.irq_enable = mtk_spmi_pmic_irq_enable,
	.irq_disable = mtk_spmi_pmic_irq_disable,
	.irq_bus_lock = mtk_spmi_pmic_irq_lock,
	.irq_bus_sync_unlock = mtk_spmi_pmic_irq_sync_unlock,
};

static void mtk_spmi_pmic_irq_sp_handler(struct pmic_core *core,
					 unsigned int top_gp)
{
	unsigned int irq_status, sta_reg, status;
	unsigned int hwirq, virq;
	int ret, i, j;
	struct irq_top_t *pmic_int;
	const struct mtk_spmi_pmic_data *chip_data = core->chip_data;

	for (i = 0; i < chip_data->pmic_ints[top_gp].num_int_regs; i++) {
		pmic_int = &(chip_data->pmic_ints[top_gp]);
		sta_reg = pmic_int->sta_reg + (pmic_int->sta_reg_shift * i);

		ret = regmap_read(core->regmap, sta_reg, &irq_status);
		if (ret) {
			dev_err(core->dev,
				"Failed to read irq status: %d\n", ret);
			return;
		}

		if (!irq_status)
			continue;

		status = irq_status;
		do {
			j = __ffs(status);

			hwirq = pmic_int->hwirq_base + MTK_SPMI_PMIC_REG_WIDTH * i + j;

			virq = irq_find_mapping(core->irq_domain, hwirq);
			dev_info(core->dev, "Reg[0x%x]=0x%x,hwirq=%d\n",
				 sta_reg, irq_status, hwirq);
			if (virq)
				handle_nested_irq(virq);

			status &= ~BIT(j);
		} while (status);

		regmap_write(core->regmap, sta_reg, irq_status);
	}
}

static irqreturn_t mtk_spmi_pmic_irq_handler(int irq, void *data)
{
	int ret;
	unsigned int bit, i, top_irq_status = 0;
	struct pmic_core *core = data;
	const struct mtk_spmi_pmic_data *chip_data = core->chip_data;

	ret = regmap_read(core->regmap, chip_data->top_int_status_reg,
			  &top_irq_status);
	if (ret) {
		dev_err(core->dev,
			"Failed to read status from the device, ret=%d\n", ret);
		return IRQ_NONE;
	}

	dev_info(core->dev, "top_irq_sts:0x%x\n", top_irq_status);
	for (i = 0; i < chip_data->num_top; i++) {
		bit = BIT(chip_data->pmic_ints[i].top_offset);
		if (top_irq_status & bit) {
			mtk_spmi_pmic_irq_sp_handler(core, i);
			if (!top_irq_status)
				break;
		}
	}

	ret = regmap_write(core->regmap, RCS_INT_DONE, 1);
	if (ret) {
		dev_err(core->dev,
			"Failed to clear RCS flag, ret=%d\n", ret);
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}

static int mtk_spmi_pmic_irq_domain_map(struct irq_domain *d, unsigned int irq,
					irq_hw_number_t hw)
{
	struct pmic_core *core = d->host_data;

	irq_set_chip_data(irq, core);
	irq_set_chip_and_handler(irq, &mtk_spmi_pmic_irq_chip,
				 handle_level_irq);
	irq_set_nested_thread(irq, 1);
	irq_set_noprobe(irq);

	return 0;
}

static const struct irq_domain_ops pmic_irq_domain_ops = {
	.map = mtk_spmi_pmic_irq_domain_map,
	.xlate = irq_domain_xlate_twocell,
};

static int mtk_spmi_pmic_irq_init(struct pmic_core *core)
{
	int i, j, ret;
	unsigned int en_reg;
	const struct mtk_spmi_pmic_data *chip_data = core->chip_data;

	mutex_init(&core->irqlock);
	core->enable_hwirq = devm_kcalloc(core->dev,
					  chip_data->num_pmic_irqs,
					  sizeof(bool), GFP_KERNEL);
	if (!core->enable_hwirq)
		return -ENOMEM;

	core->cache_hwirq = devm_kcalloc(core->dev,
					 chip_data->num_pmic_irqs,
					 sizeof(bool), GFP_KERNEL);
	if (!core->cache_hwirq)
		return -ENOMEM;

	/* Disable all interrupt for initializing */
	for (i = 0; i < chip_data->num_top; i++) {
		for (j = 0; j < chip_data->pmic_ints[i].num_int_regs; j++) {
			en_reg = chip_data->pmic_ints[i].en_reg +
				chip_data->pmic_ints[i].en_reg_shift * j;
			regmap_write(core->regmap, en_reg, 0);
		}
	}

	core->irq_domain = irq_domain_add_linear(core->dev->of_node,
						 chip_data->num_pmic_irqs,
						 &pmic_irq_domain_ops,
						 core);
	if (!core->irq_domain) {
		dev_err(core->dev, "Could not create IRQ domain\n");
		return -ENODEV;
	}

	ret = devm_request_threaded_irq(core->dev, core->irq, NULL,
					mtk_spmi_pmic_irq_handler, IRQF_ONESHOT,
					mtk_spmi_pmic_irq_chip.name, core);
	if (ret) {
		dev_err(core->dev, "Failed to register IRQ=%d, ret=%d\n",
			core->irq, ret);
		return ret;
	}

	enable_irq_wake(core->irq);
	return ret;
}

static const struct regmap_config spmi_regmap_config = {
	.reg_bits	= 16,
	.val_bits	= 8,
	.max_register	= 0xffff,
	.fast_io	= true,
};

static int mtk_spmi_pmic_probe(struct spmi_device *sdev)
{
	int ret;
	unsigned int id;
	struct device_node *np = sdev->dev.of_node;
	struct pmic_core *core;
	const struct mtk_spmi_pmic_data *chip_data;

	core = devm_kzalloc(&sdev->dev, sizeof(*core), GFP_KERNEL);
	if (!core)
		return -ENOMEM;

	core->sdev = sdev;
	core->dev = &sdev->dev;
	chip_data = (struct mtk_spmi_pmic_data *)of_device_get_match_data(&sdev->dev);
	if (!chip_data)
		return -ENODEV;

	core->chip_data = chip_data;
	core->regmap = devm_regmap_init_spmi_ext(sdev, &spmi_regmap_config);
	if (IS_ERR(core->regmap))
		return PTR_ERR(core->regmap);
	ret = regmap_read(core->regmap, PMIC_SWCID, &id);
	if (ret) {
		dev_err(&sdev->dev, "Failed to read chip id: %d\n", ret);
		return ret;
	}

	core->chip_id = id;

	if (chip_data->num_pmic_irqs) {
		core->irq = of_irq_get(np, 0);
		if (core->irq < 0)
			dev_err(&sdev->dev, "Failed to get irq(%d)\n", core->irq);

		ret = mtk_spmi_pmic_irq_init(core);
		if (ret)
			dev_err(&sdev->dev, "IRQ_init failed(%d)\n", core->irq);

		ret = devm_mfd_add_devices(&sdev->dev, -1, chip_data->cells,
					   chip_data->cell_size, NULL, 0,
					   core->irq_domain);
		if (ret)
			irq_domain_remove(core->irq_domain);
	} else
		ret = devm_of_platform_populate(&sdev->dev);
	if (ret) {
		dev_err(&sdev->dev, "Failed to add child devices: %d\n", ret);
		return ret;
	}

	device_init_wakeup(&sdev->dev, true);

	dev_info(&sdev->dev, "probe chip id=0x%x done\n", core->chip_id);

	return ret;
}

static const struct of_device_id mtk_spmi_pmic_of_match[] = {
	{ .compatible = "mediatek,mt6315", .data = &common_data, },
	{ .compatible = "mediatek,mt6319", .data = &common_data, },
	{ .compatible = "mediatek,mt6363", .data = &mt6363_data, },
	{ }
};
MODULE_DEVICE_TABLE(of, mtk_spmi_pmic_of_match);

static struct spmi_driver mtk_spmi_pmic_driver = {
	.driver = {
		.name = "mtk-spmi-pmic",
		.of_match_table = of_match_ptr(mtk_spmi_pmic_of_match),
	},
	.probe = mtk_spmi_pmic_probe,
};
module_spmi_driver(mtk_spmi_pmic_driver);

MODULE_DESCRIPTION("Mediatek SPMI PMIC driver");
MODULE_ALIAS("spmi:spmi-pmic");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Argus Lin <argus.lin@mediatek.com>");
MODULE_AUTHOR("Jeter Chen <jeter.chen@mediatek.com>");
