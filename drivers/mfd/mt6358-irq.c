// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/interrupt.h>
#include <linux/mfd/mt6358/core.h>
#include <linux/mfd/mt6358/registers.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

struct irq_top_t {
	int hwirq_base;
	unsigned int num_int_regs;
	unsigned int num_int_bits;
	unsigned int en_reg;
	unsigned int en_reg_shift;
	unsigned int sta_reg;
	unsigned int sta_reg_shift;
	unsigned int top_offset;
};

struct pmic_irq_data {
	unsigned int num_top;
	unsigned int num_pmic_irqs;
	unsigned int reg_width;
	unsigned short top_int_status_reg;
	unsigned int *enable_hwirq;
	unsigned int *cache_hwirq;
	struct irq_top_t *pmic_ints;
};

static struct irq_top_t mt6358_ints[] = {
	MT6358_TOP_GEN(BUCK),
	MT6358_TOP_GEN(LDO),
	MT6358_TOP_GEN(PSC),
	MT6358_TOP_GEN(SCK),
	MT6358_TOP_GEN(BM),
	MT6358_TOP_GEN(HK),
	MT6358_TOP_GEN(AUD),
	MT6358_TOP_GEN(MISC),
};

static int parsing_hwirq_to_top_group(struct pmic_irq_data *irq_data,
				      unsigned int hwirq)
{
	int top_group;

	for (top_group = 1; top_group < irq_data->num_top; top_group++) {
		if (irq_data->pmic_ints[top_group].hwirq_base > hwirq) {
			top_group--;
			break;
		}
	}
	return top_group;
}

static void pmic_irq_enable(struct irq_data *data)
{
	unsigned int hwirq = irqd_to_hwirq(data);
	struct mt6397_chip *chip = irq_data_get_irq_chip_data(data);
	struct pmic_irq_data *irq_data = chip->irq_data;

	irq_data->enable_hwirq[hwirq] = 1;
}

static void pmic_irq_disable(struct irq_data *data)
{
	unsigned int hwirq = irqd_to_hwirq(data);
	struct mt6397_chip *chip = irq_data_get_irq_chip_data(data);
	struct pmic_irq_data *irq_data = chip->irq_data;

	irq_data->enable_hwirq[hwirq] = 0;
}

static void pmic_irq_lock(struct irq_data *data)
{
	struct mt6397_chip *chip = irq_data_get_irq_chip_data(data);

	mutex_lock(&chip->irqlock);
}

static void pmic_irq_sync_unlock(struct irq_data *data)
{
	unsigned int i, top_gp, en_reg, int_regs, shift;
	struct mt6397_chip *chip = irq_data_get_irq_chip_data(data);
	struct pmic_irq_data *irq_data = chip->irq_data;

	for (i = 0; i < irq_data->num_pmic_irqs; i++) {
		if (irq_data->enable_hwirq[i] ==
				irq_data->cache_hwirq[i])
			continue;

		top_gp = parsing_hwirq_to_top_group(irq_data, i);
		int_regs = irq_data->pmic_ints[top_gp].num_int_bits /
			irq_data->reg_width;
		en_reg = irq_data->pmic_ints[top_gp].en_reg +
			irq_data->pmic_ints[top_gp].en_reg_shift * int_regs;
		shift = (i - irq_data->pmic_ints[top_gp].hwirq_base) %
			irq_data->reg_width;
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

static struct irq_chip mt6358_irq_chip = {
	.name = "mt6358-irq",
	.irq_enable = pmic_irq_enable,
	.irq_disable = pmic_irq_disable,
	.irq_bus_lock = pmic_irq_lock,
	.irq_bus_sync_unlock = pmic_irq_sync_unlock,
	.irq_set_type = pmic_irq_set_type,
};

static void mt6358_irq_sp_handler(struct mt6397_chip *chip,
				  unsigned int top_gp)
{
	unsigned int sta_reg, int_status = 0;
	unsigned int hwirq, virq;
	int ret, i, j;
	struct pmic_irq_data *irq_data = chip->irq_data;

	for (i = 0; i < irq_data->pmic_ints[top_gp].num_int_regs; i++) {
		sta_reg = irq_data->pmic_ints[top_gp].sta_reg +
			irq_data->pmic_ints[top_gp].sta_reg_shift * i;
		ret = regmap_read(chip->regmap, sta_reg, &int_status);
		if (ret) {
			dev_err(chip->dev,
				"Failed to read irq status: %d\n", ret);
			return;
		}

		if (!int_status)
			continue;

		for (j = 0; j < 16 ; j++) {
			if ((int_status & BIT(j)) == 0)
				continue;
			hwirq = irq_data->pmic_ints[top_gp].hwirq_base +
				irq_data->reg_width * i + j;
			virq = irq_find_mapping(chip->irq_domain, hwirq);
			dev_info(chip->dev,
				"Reg[0x%x]=0x%x,hwirq=%d,type=%d\n",
				sta_reg, int_status, hwirq,
				irq_get_trigger_type(virq));
			if (virq)
				handle_nested_irq(virq);
		}

		regmap_write(chip->regmap, sta_reg, int_status);
	}
}

static irqreturn_t mt6358_irq_handler(int irq, void *data)
{
	struct mt6397_chip *chip = data;
	struct pmic_irq_data *irq_data = chip->irq_data;
	unsigned int top_int_status = 0;
	unsigned int i;
	int ret;

	ret = regmap_read(chip->regmap,
			  irq_data->top_int_status_reg,
			  &top_int_status);
	if (ret) {
		dev_err(chip->dev, "Can't read TOP_INT_STATUS ret=%d\n", ret);
		return IRQ_NONE;
	}

	for (i = 0; i < irq_data->num_top; i++) {
		if (top_int_status & BIT(irq_data->pmic_ints[i].top_offset))
			mt6358_irq_sp_handler(chip, i);
	}

	return IRQ_HANDLED;
}

static int pmic_irq_domain_map(struct irq_domain *d, unsigned int irq,
			       irq_hw_number_t hw)
{
	struct mt6397_chip *mt6397 = d->host_data;

	irq_set_chip_data(irq, mt6397);
	irq_set_chip_and_handler(irq, &mt6358_irq_chip, handle_level_irq);
	irq_set_nested_thread(irq, 1);
	irq_set_noprobe(irq);

	return 0;
}

static const struct irq_domain_ops mt6358_irq_domain_ops = {
	.map = pmic_irq_domain_map,
	.xlate = irq_domain_xlate_twocell,
};

int mt6358_irq_init(struct mt6397_chip *chip)
{
	int i, j, ret;
	struct pmic_irq_data *irq_data;

	irq_data = devm_kzalloc(chip->dev, sizeof(struct pmic_irq_data *),
				GFP_KERNEL);
	if (!irq_data)
		return -ENOMEM;

	chip->irq_data = irq_data;

	mutex_init(&chip->irqlock);
	switch (chip->chip_id) {
	case MT6358_CID_CODE:
		irq_data->num_top = MT6358_TOP_NR;
		irq_data->num_pmic_irqs = MT6358_IRQ_NR;
		irq_data->reg_width = MT6358_REG_WIDTH;
		irq_data->top_int_status_reg = MT6358_TOP_INT_STATUS0;
		irq_data->pmic_ints = mt6358_ints;
		break;
	default:
		dev_err(chip->dev, "unsupported chip: 0x%x\n", chip->chip_id);
		ret = -ENODEV;
		break;
	}

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
	for (i = 0; i < irq_data->num_top; i++) {
		for (j = 0; j < irq_data->pmic_ints[i].num_int_regs; j++)
			regmap_write(chip->regmap,
				     irq_data->pmic_ints[i].en_reg +
				     irq_data->pmic_ints[i].en_reg_shift * j,
				     0);
	}

	chip->irq_domain = irq_domain_add_linear(chip->dev->of_node,
						 irq_data->num_pmic_irqs,
						 &mt6358_irq_domain_ops, chip);
	if (!chip->irq_domain) {
		dev_err(chip->dev, "could not create irq domain\n");
		return -ENODEV;
	}

	ret = devm_request_threaded_irq(chip->dev, chip->irq, NULL,
					mt6358_irq_handler, IRQF_ONESHOT,
					mt6358_irq_chip.name, chip);
	if (ret) {
		dev_err(chip->dev, "failed to register irq=%d; err: %d\n",
			chip->irq, ret);
		return ret;
	}

	enable_irq_wake(chip->irq);
	return ret;
}
