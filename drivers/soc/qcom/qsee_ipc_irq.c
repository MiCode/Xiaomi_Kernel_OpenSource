// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 */

#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define MAX_BANK_IRQ 32

#define hwirq_to_index(_irq) (_irq / MAX_BANK_IRQ)
#define hwirq_to_bit(_irq) (_irq % MAX_BANK_IRQ)
#define to_hwirq(_index, _bit) ((_index * MAX_BANK_IRQ) + _bit)

struct qsee_irq_data {
	const char *name;
	u32 status;
	u32 clear;
	u32 mask;
	u32 msb;
};

struct qsee_irq_bank {
	const struct qsee_irq_data *data;
	int irq;

	DECLARE_BITMAP(irq_enabled, MAX_BANK_IRQ);
	DECLARE_BITMAP(irq_rising, MAX_BANK_IRQ);
	DECLARE_BITMAP(irq_falling, MAX_BANK_IRQ);
};

struct qsee_irq {
	struct device *dev;

	struct irq_domain *domain;
	struct regmap *regmap;

	int num_banks;
	struct qsee_irq_bank *banks;
};

/**
 * qsee_intr() - interrupt handler for incoming notifications
 * @irq:	unused
 * @data:	qsee driver context
 *
 * Handle notifications from the remote side to handle newly allocated entries
 * or any changes to the state bits of existing entries.
 */
static irqreturn_t qsee_intr(int irq, void *data)
{
	struct qsee_irq *qirq = data;
	struct qsee_irq_bank *bank = NULL;
	struct irq_desc *desc;
	int irq_pin;
	u32 status;
	u32 mask;
	int i;
	int j;

	for (i = 0; i < qirq->num_banks; i++) {
		if (qirq->banks[i].irq == irq) {
			bank = &qirq->banks[i];
			break;
		}
	}
	if (!bank) {
		dev_err(qirq->dev, "Unable to find bank for irq:%d\n", irq);
		return IRQ_HANDLED;
	}

	if (regmap_read(qirq->regmap, bank->data->status, &status)) {
		dev_err(qirq->dev, "Error reading irq %d status\n", irq);
		return IRQ_HANDLED;
	}
	if (regmap_read(qirq->regmap, bank->data->mask, &mask)) {
		dev_err(qirq->dev, "Error reading irq %d mask\n", irq);
		return IRQ_HANDLED;
	}

	for_each_set_bit(j, bank->irq_enabled, bank->data->msb) {
		if (!(status & BIT(j)))
			continue;

		irq_pin = irq_find_mapping(qirq->domain, to_hwirq(i, j));
		desc = irq_to_desc(irq_pin);

		regmap_write(qirq->regmap, bank->data->clear, BIT(j));
		if (desc)
			handle_level_irq(desc);
	}

	return IRQ_HANDLED;
}

static void qsee_mask_irq(struct irq_data *irqd)
{
	struct qsee_irq *qirq = irq_data_get_irq_chip_data(irqd);
	irq_hw_number_t irq = irqd_to_hwirq(irqd);
	struct qsee_irq_bank *bank;
	int index;
	u32 mask;
	int bit;

	index = hwirq_to_index(irq);
	bit = hwirq_to_bit(irq);
	bank = &qirq->banks[index];

	regmap_read(qirq->regmap, bank->data->mask, &mask);
	mask |= BIT(bit);
	regmap_write(qirq->regmap, bank->data->mask, mask);

	clear_bit(bit, bank->irq_enabled);
}

static void qsee_unmask_irq(struct irq_data *irqd)
{
	struct qsee_irq *qirq = irq_data_get_irq_chip_data(irqd);
	irq_hw_number_t irq = irqd_to_hwirq(irqd);
	struct qsee_irq_bank *bank;
	int index;
	u32 mask;
	int bit;

	index = hwirq_to_index(irq);
	bit = hwirq_to_bit(irq);
	bank = &qirq->banks[index];

	regmap_read(qirq->regmap, bank->data->mask, &mask);
	mask &= ~(BIT(bit));
	regmap_write(qirq->regmap, bank->data->mask, mask);

	set_bit(bit, bank->irq_enabled);
}

static int qsee_set_irq_type(struct irq_data *irqd, unsigned int type)
{
	struct qsee_irq *qirq = irq_data_get_irq_chip_data(irqd);
	irq_hw_number_t irq = irqd_to_hwirq(irqd);
	struct qsee_irq_bank *bank;
	int index;
	int bit;

	index = hwirq_to_index(irq);
	bit = hwirq_to_bit(irq);
	bank = &qirq->banks[index];

	if (type & IRQ_TYPE_LEVEL_HIGH)
		return 0;

	if (!(type & IRQ_TYPE_EDGE_BOTH))
		return -EINVAL;

	if (type & IRQ_TYPE_EDGE_RISING)
		set_bit(bit, bank->irq_rising);
	else
		clear_bit(bit, bank->irq_rising);

	if (type & IRQ_TYPE_EDGE_FALLING)
		set_bit(bit, bank->irq_falling);
	else
		clear_bit(bit, bank->irq_falling);

	return 0;
}

static struct irq_chip qsee_irq_chip = {
	.name           = "qsee",
	.irq_mask       = qsee_mask_irq,
	.irq_unmask     = qsee_unmask_irq,
	.irq_set_type	= qsee_set_irq_type,
};

static int qsee_irq_map(struct irq_domain *d,
			 unsigned int irq,
			 irq_hw_number_t hw)
{
	struct qsee_irq *qirq = d->host_data;

	irq_set_chip_and_handler(irq, &qsee_irq_chip, handle_level_irq);
	irq_set_chip_data(irq, qirq);
	irq_set_noprobe(irq);

	return 0;
}

static int qsee_irq_xlate_threecell(struct irq_domain *d,
				    struct device_node *ctrlr,
				    const u32 *intspec,
				    unsigned int intsize,
				    irq_hw_number_t *out_hwirq,
				    unsigned int *out_type)
{
	struct qsee_irq *qirq = d->host_data;
	u32 index, bit;

	if (WARN_ON(intsize < 3))
		return -EINVAL;

	index = intspec[0];
	if (WARN_ON(index >= qirq->num_banks))
		return -EINVAL;

	bit = intspec[1];
	if (WARN_ON(bit >= qirq->banks[index].data->msb))
		return -EINVAL;

	*out_hwirq = (index * MAX_BANK_IRQ) + bit;
	*out_type = intspec[2] & IRQ_TYPE_SENSE_MASK;

	return 0;
}

static const struct irq_domain_ops qsee_irq_ops = {
	.map = qsee_irq_map,
	.xlate = qsee_irq_xlate_threecell,
};

static int qsee_irq_probe(struct platform_device *pdev)
{
	const struct qsee_irq_data *data;
	struct device *dev = &pdev->dev;
	struct qsee_irq_bank *bank;
	struct device_node *syscon;
	struct qsee_irq *qirq;
	int irq_count;
	u32 mask;
	int idx;
	int ret;
	int i;

	qirq = devm_kzalloc(dev, sizeof(*qirq), GFP_KERNEL);
	if (!qirq)
		return -ENOMEM;

	platform_set_drvdata(pdev, qirq);
	qirq->dev = dev;

	syscon = of_parse_phandle(dev->of_node, "syscon", 0);
	if (!syscon) {
		dev_err(dev, "no syscon node\n");
		return -ENODEV;
	}

	qirq->regmap = syscon_node_to_regmap(syscon);
	if (IS_ERR(qirq->regmap))
		return PTR_ERR(qirq->regmap);

	data = (struct qsee_irq_data *)of_device_get_match_data(dev);
	if (!data)
		return -ENODEV;

	irq_count = platform_irq_count(pdev);
	qirq->banks = devm_kzalloc(dev, sizeof(*qirq->banks) * irq_count,
				   GFP_KERNEL);
	if (!qirq->banks)
		return -ENOMEM;

	qirq->num_banks = irq_count;
	for (i = 0; data[i].name; i++) {
		idx = of_property_match_string(dev->of_node, "interrupt-names",
					       data[i].name);
		if (idx < 0)
			return -EINVAL;

		bank = &qirq->banks[idx];
		bank->data = &data[i];
		bank->irq = platform_get_irq(pdev, idx);
		if (bank->irq < 0) {
			dev_err(dev, "unable to acquire %s interrupt\n",
				data[i].name);
			return -EINVAL;
		}

		/* Mask all interrupts until client registers */
		mask = (1 << bank->data->msb) - 1;
		regmap_write(qirq->regmap, bank->data->mask, mask);

		ret = devm_request_irq(dev, bank->irq, qsee_intr,
				       IRQF_NO_SUSPEND | IRQF_ONESHOT,
				       "qsee_irq", qirq);
		if (ret) {
			dev_err(dev, "failed to request interrupt\n");
			return ret;
		}
	}

	qirq->domain = irq_domain_add_linear(dev->of_node, 32 * irq_count,
						 &qsee_irq_ops, qirq);
	if (!qirq->domain) {
		dev_err(dev, "failed to add irq_domain\n");
		return -ENOMEM;
	}

	return 0;
}

static int qsee_irq_remove(struct platform_device *pdev)
{
	struct qsee_irq *qirq = platform_get_drvdata(pdev);

	irq_domain_remove(qirq->domain);

	return 0;
}

static const struct qsee_irq_data qsee_irq_data_init[] = {
	{
		.name = "sp_ipc0",
		.status = 0x6000,
		.clear = 0x6008,
		.mask = 0x601C,
		.msb = 4,
	},
	{
		.name = "sp_ipc1",
		.status = 0x8000,
		.clear = 0x8008,
		.mask = 0x801C,
		.msb = 4,
	},
	{},
};

static const struct of_device_id qsee_irq_of_match[] = {
	{ .compatible = "qcom,sm8150-qsee-irq", .data = &qsee_irq_data_init},
	{ .compatible = "qcom,kona-qsee-irq", .data = &qsee_irq_data_init},
	{},
};
MODULE_DEVICE_TABLE(of, qsee_irq_of_match);

static struct platform_driver qsee_irq_driver = {
	.probe = qsee_irq_probe,
	.remove = qsee_irq_remove,
	.driver = {
			.name = "qsee_irq",
			.of_match_table = qsee_irq_of_match,
	},
};

static int __init qsee_irq_init(void)
{
	int rc;

	rc = platform_driver_register(&qsee_irq_driver);
	if (rc)
		pr_err("%s: platform driver reg failed %d\n", __func__, rc);

	return rc;
}
postcore_initcall(qsee_irq_init);

MODULE_DESCRIPTION("QTI Secure Execution Environment IRQ driver");
MODULE_LICENSE("GPL v2");

