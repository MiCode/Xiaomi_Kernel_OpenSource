/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/ratelimit.h>
#include <linux/irqdomain.h>
#include <linux/regmap.h>
#include <linux/pm_runtime.h>

#include "pdata.h"
#include "aqt1000.h"

#include "aqt1000-registers.h"
#include "aqt1000-irq.h"

static const struct regmap_irq aqt1000_irqs[AQT1000_NUM_IRQS] = {
	REGMAP_IRQ_REG(AQT1000_IRQ_MBHC_BUTTON_RELEASE_DET, 0, 0x01),
	REGMAP_IRQ_REG(AQT1000_IRQ_MBHC_BUTTON_PRESS_DET, 0, 0x02),
	REGMAP_IRQ_REG(AQT1000_IRQ_MBHC_ELECT_INS_REM_DET, 0, 0x04),
	REGMAP_IRQ_REG(AQT1000_IRQ_MBHC_ELECT_INS_REM_LEG_DET, 0, 0x08),
	REGMAP_IRQ_REG(AQT1000_IRQ_MBHC_SW_DET, 0, 0x10),
	REGMAP_IRQ_REG(AQT1000_IRQ_HPH_PA_OCPL_FAULT, 0, 0x20),
	REGMAP_IRQ_REG(AQT1000_IRQ_HPH_PA_OCPR_FAULT, 0, 0x40),
	REGMAP_IRQ_REG(AQT1000_IRQ_HPH_PA_CNPL_COMPLETE, 0, 0x80),
	REGMAP_IRQ_REG(AQT1000_IRQ_HPH_PA_CNPR_COMPLETE, 1, 0x01),
	REGMAP_IRQ_REG(AQT1000_CDC_HPHL_SURGE, 1, 0x02),
	REGMAP_IRQ_REG(AQT1000_CDC_HPHR_SURGE, 1, 0x04),
};

static const struct regmap_irq_chip aqt_regmap_irq_chip = {
	.name = "AQT1000",
	.irqs = aqt1000_irqs,
	.num_irqs = ARRAY_SIZE(aqt1000_irqs),
	.num_regs = 2,
	.status_base = AQT1000_INTR_CTRL_INT_STATUS_2,
	.mask_base = AQT1000_INTR_CTRL_INT_MASK_2,
	.unmask_base = AQT1000_INTR_CTRL_INT_CLEAR_2,
	.ack_base = AQT1000_INTR_CTRL_INT_STATUS_2,
	.runtime_pm = true,
};

static int aqt_map_irq(struct aqt1000 *aqt, int irq)
{
	return regmap_irq_get_virq(aqt->irq_chip, irq);
}

/**
 * aqt_request_irq: Request a thread handler for the given IRQ
 * @aqt: pointer to aqt1000 structure
 * @irq: irq number
 * @name: name for the IRQ thread
 * @handler: irq handler
 * @data: data pointer
 *
 * Returns 0 on success or error on failure
 */
int aqt_request_irq(struct aqt1000 *aqt, int irq, const char *name,
			irq_handler_t handler, void *data)
{
	irq = aqt_map_irq(aqt, irq);
	if (irq < 0)
		return irq;

	return request_threaded_irq(irq, NULL, handler,
				    IRQF_ONESHOT | IRQF_TRIGGER_RISING,
				    name, data);
}
EXPORT_SYMBOL(aqt_request_irq);

/**
 * aqt_free_irq: Free the IRQ resources allocated during request_irq
 * @aqt: pointer to aqt1000 structure
 * @irq: irq number
 * @data: data pointer
 */
void aqt_free_irq(struct aqt1000 *aqt, int irq, void *data)
{
	irq = aqt_map_irq(aqt, irq);
	if (irq < 0)
		return;

	free_irq(irq, data);
}
EXPORT_SYMBOL(aqt_free_irq);

/**
 * aqt_enable_irq: Enable the given IRQ
 * @aqt: pointer to aqt1000 structure
 * @irq: irq number
 */
void aqt_enable_irq(struct aqt1000 *aqt, int irq)
{
	if (aqt)
		enable_irq(aqt_map_irq(aqt, irq));
}
EXPORT_SYMBOL(aqt_enable_irq);

/**
 * aqt_disable_irq: Disable the given IRQ
 * @aqt: pointer to aqt1000 structure
 * @irq: irq number
 */
void aqt_disable_irq(struct aqt1000 *aqt, int irq)
{
	if (aqt)
		disable_irq(aqt_map_irq(aqt, irq));
}
EXPORT_SYMBOL(aqt_disable_irq);

static irqreturn_t aqt_irq_thread(int irq, void *data)
{
	int ret = 0;
	u8 sts[2];
	struct aqt1000 *aqt = data;
	int num_irq_regs = aqt->num_irq_regs;
	struct aqt1000_pdata *pdata;

	pdata = dev_get_platdata(aqt->dev);

	memset(sts, 0, sizeof(sts));
	ret = regmap_bulk_read(aqt->regmap, AQT1000_INTR_CTRL_INT_STATUS_2,
				sts, num_irq_regs);
	if (ret < 0) {
		dev_err(aqt->dev, "%s: Failed to read intr status: %d\n",
			__func__, ret);
	} else if (ret == 0) {
		while (gpio_get_value_cansleep(pdata->irq_gpio))
			handle_nested_irq(irq_find_mapping(aqt->virq, 0));
	}

	return IRQ_HANDLED;
}

static void aqt_irq_disable(struct irq_data *data)
{
}

static void aqt_irq_enable(struct irq_data *data)
{
}

static struct irq_chip aqt_irq_chip = {
	.name = "AQT",
	.irq_disable = aqt_irq_disable,
	.irq_enable = aqt_irq_enable,
};

static struct lock_class_key aqt_irq_lock_class;

static int aqt_irq_map(struct irq_domain *irqd, unsigned int virq,
			irq_hw_number_t hw)
{
	struct aqt1000 *data = irqd->host_data;

	irq_set_chip_data(virq, data);
	irq_set_chip_and_handler(virq, &aqt_irq_chip, handle_simple_irq);
	irq_set_lockdep_class(virq, &aqt_irq_lock_class);
	irq_set_nested_thread(virq, 1);
	irq_set_noprobe(virq);

	return 0;
}

static const struct irq_domain_ops aqt_domain_ops = {
	.map = aqt_irq_map,
	.xlate = irq_domain_xlate_twocell,
};

/**
 * aqt_irq_init: Initializes IRQ module
 * @aqt: pointer to aqt1000 structure
 *
 * Returns 0 on success or error on failure
 */
int aqt_irq_init(struct aqt1000 *aqt)
{
	int i, ret;
	unsigned int flags = IRQF_ONESHOT;
	struct irq_data *irq_data;
	struct aqt1000_pdata *pdata;

	if (!aqt) {
		pr_err("%s: Null pointer handle\n", __func__);
		return -EINVAL;
	}

	pdata = dev_get_platdata(aqt->dev);
	if (!pdata) {
		dev_err(aqt->dev, "%s: Invalid platform data\n", __func__);
		return -EINVAL;
	}

	/* Select default if not defined in DT */
	flags = IRQF_TRIGGER_HIGH | IRQF_ONESHOT;
	if (pdata->irq_flags)
		flags = pdata->irq_flags;

	if (pdata->irq_gpio) {
		aqt->irq = gpio_to_irq(pdata->irq_gpio);
		ret = devm_gpio_request_one(aqt->dev, pdata->irq_gpio,
					    GPIOF_IN, "AQT IRQ");
		if (ret) {
			dev_err(aqt->dev, "%s: Failed to request gpio %d\n",
				__func__, ret);
			pdata->irq_gpio = 0;
			return ret;
		}
	}

	irq_data = irq_get_irq_data(aqt->irq);
	if (!irq_data) {
		dev_err(aqt->dev, "%s: Invalid IRQ: %d\n",
			__func__, aqt->irq);
		return -EINVAL;
	}

	aqt->num_irq_regs = aqt_regmap_irq_chip.num_regs;
	for (i = 0; i < aqt->num_irq_regs; i++) {
		regmap_write(aqt->regmap,
			     (AQT1000_INTR_CTRL_INT_TYPE_2 + i), 0);
	}

	aqt->virq = irq_domain_add_linear(NULL, 1, &aqt_domain_ops, aqt);
	if (!aqt->virq) {
		dev_err(aqt->dev, "%s: Failed to add IRQ domain\n", __func__);
		ret = -EINVAL;
		goto err;
	}
	ret = regmap_add_irq_chip(aqt->regmap,
				  irq_create_mapping(aqt->virq, 0),
				  IRQF_ONESHOT, 0, &aqt_regmap_irq_chip,
				  &aqt->irq_chip);
	if (ret) {
		dev_err(aqt->dev, "%s: Failed to add IRQs: %d\n",
			__func__, ret);
		goto err;
	}

	ret = request_threaded_irq(aqt->irq, NULL, aqt_irq_thread, flags,
				   "aqt", aqt);
	if (ret) {
		dev_err(aqt->dev, "%s: failed to register irq: %d\n",
			__func__, ret);
		goto err_irq;
	}

	return 0;

err_irq:
	regmap_del_irq_chip(irq_create_mapping(aqt->virq, 1), aqt->irq_chip);
err:
	return ret;
}
EXPORT_SYMBOL(aqt_irq_init);

/**
 * aqt_irq_exit: Uninitialize regmap IRQ and free IRQ resources
 * @aqt: pointer to aqt1000 structure
 *
 * Returns 0 on success or error on failure
 */
int aqt_irq_exit(struct aqt1000 *aqt)
{
	if (!aqt) {
		pr_err("%s: Null pointer handle\n", __func__);
		return -EINVAL;
	}
	regmap_del_irq_chip(irq_create_mapping(aqt->virq, 1), aqt->irq_chip);
	free_irq(aqt->irq, aqt);

	return 0;
}
EXPORT_SYMBOL(aqt_irq_exit);
