/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/ratelimit.h>
#include <linux/pm_qos.h>
#include <soc/qcom/pm.h>
#include "wsa881x-irq.h"
#include "wsa881x-registers-analog.h"

#define BYTE_BIT_MASK(nr)		(1UL << ((nr) % BITS_PER_BYTE))
#define BIT_BYTE(nr)			((nr) / BITS_PER_BYTE)


#define WSA_MAX_NUM_IRQS 8

#ifndef NO_IRQ
#define NO_IRQ	(-1)
#endif

static int virq_to_phyirq(
	struct wsa_resource *wsa_res, int virq);
static int phyirq_to_virq(
	struct wsa_resource *wsa_res, int irq);
static unsigned int wsa_irq_get_upstream_irq(
	struct wsa_resource *wsa_res);
static void wsa_irq_put_upstream_irq(
	struct wsa_resource *wsa_res);
static int wsa_map_irq(
	struct wsa_resource *wsa_res, int irq);

static struct snd_soc_codec *ptr_codec;

/**
 * wsa_set_codec() - to update codec pointer
 * @codec: codec pointer.
 *
 * To update the codec pointer, which is used to read/write
 * wsa register.
 *
 * Return: void.
 */
void wsa_set_codec(struct snd_soc_codec *codec)
{
	if (codec == NULL) {
		pr_err("%s: codec pointer is NULL\n", __func__);
		ptr_codec = NULL;
		return;
	}
	ptr_codec = codec;
	/* Initialize interrupt mask and level registers */
	snd_soc_write(codec, WSA881X_INTR_LEVEL, 0x8F);
	snd_soc_write(codec, WSA881X_INTR_MASK, 0x8F);
}

static void wsa_irq_lock(struct irq_data *data)
{
	struct wsa_resource *wsa_res =
			irq_data_get_irq_chip_data(data);

	if (wsa_res == NULL) {
		pr_err("%s: wsa_res pointer is NULL\n", __func__);
		return;
	}
	mutex_lock(&wsa_res->irq_lock);
}

static void wsa_irq_sync_unlock(struct irq_data *data)
{
	struct wsa_resource *wsa_res =
			irq_data_get_irq_chip_data(data);

	if (wsa_res == NULL) {
		pr_err("%s: wsa_res pointer is NULL\n", __func__);
		return;
	}
	if (wsa_res->codec == NULL) {
		pr_err("%s: codec pointer not registered\n", __func__);
		if (ptr_codec == NULL) {
			pr_err("%s: did not receive valid codec pointer\n",
					__func__);
			goto unlock;
		} else {
			wsa_res->codec = ptr_codec;
		}
	}

	/*
	 * If there's been a change in the mask write it back
	 * to the hardware.
	 */
	if (wsa_res->irq_masks_cur !=
			wsa_res->irq_masks_cache) {

		wsa_res->irq_masks_cache =
			wsa_res->irq_masks_cur;
		snd_soc_write(wsa_res->codec,
			WSA881X_INTR_MASK,
			wsa_res->irq_masks_cur);
	}
unlock:
	mutex_unlock(&wsa_res->irq_lock);
}

static void wsa_irq_enable(struct irq_data *data)
{
	struct wsa_resource *wsa_res =
			irq_data_get_irq_chip_data(data);
	int wsa_irq;

	if (wsa_res == NULL) {
		pr_err("%s: wsa_res pointer is NULL\n", __func__);
		return;
	}
	wsa_irq = virq_to_phyirq(wsa_res, data->irq);
	pr_debug("%s: wsa_irq = %d\n", __func__, wsa_irq);
	wsa_res->irq_masks_cur &=
			~(BYTE_BIT_MASK(wsa_irq));
}

static void wsa_irq_disable(struct irq_data *data)
{
	struct wsa_resource *wsa_res =
			irq_data_get_irq_chip_data(data);
	int wsa_irq;

	if (wsa_res == NULL) {
		pr_err("%s: wsa_res pointer is NULL\n", __func__);
		return;
	}
	wsa_irq = virq_to_phyirq(wsa_res, data->irq);
	pr_debug("%s: wsa_irq = %d\n", __func__, wsa_irq);
	wsa_res->irq_masks_cur
			|= BYTE_BIT_MASK(wsa_irq);
}

static void wsa_irq_ack(struct irq_data *data)
{
	int wsa_irq = 0;
	struct wsa_resource *wsa_res =
			irq_data_get_irq_chip_data(data);

	if (wsa_res == NULL) {
		pr_err("%s: wsa_res is NULL\n", __func__);
		return;
	}
	wsa_irq = virq_to_phyirq(wsa_res, data->irq);
	pr_debug("%s: IRQ_ACK called for WCD9XXX IRQ: %d\n",
				__func__, wsa_irq);
}

static void wsa_irq_mask(struct irq_data *d)
{
	/* do nothing but required as linux calls irq_mask without NULL check */
}

static struct irq_chip wsa_irq_chip = {
	.name = "wsa",
	.irq_bus_lock = wsa_irq_lock,
	.irq_bus_sync_unlock = wsa_irq_sync_unlock,
	.irq_disable = wsa_irq_disable,
	.irq_enable = wsa_irq_enable,
	.irq_mask = wsa_irq_mask,
	.irq_ack = wsa_irq_ack,
};

static irqreturn_t wsa_irq_thread(int irq, void *data)
{
	struct wsa_resource *wsa_res = data;
	int i;
	u8 status;

	if (wsa_res == NULL) {
		pr_err("%s: wsa_res is NULL\n", __func__);
		return IRQ_HANDLED;
	}
	if (wsa_res->codec == NULL) {
		pr_err("%s: codec pointer not registered\n", __func__);
		if (ptr_codec == NULL) {
			pr_err("%s: did not receive valid codec pointer\n",
					__func__);
			return IRQ_HANDLED;
		}
		wsa_res->codec = ptr_codec;
	}
	status = snd_soc_read(wsa_res->codec, WSA881X_INTR_STATUS);
	/* Apply masking */
	status &= ~wsa_res->irq_masks_cur;

	for (i = 0; i < wsa_res->num_irqs; i++) {
		if (status & BYTE_BIT_MASK(i)) {
			mutex_lock(&wsa_res->nested_irq_lock);
			handle_nested_irq(phyirq_to_virq(wsa_res, i));
			mutex_unlock(&wsa_res->nested_irq_lock);
		}
	}

	return IRQ_HANDLED;
}

/**
 * wsa_free_irq() - to free an interrupt
 * @irq: interrupt number.
 * @data: pointer to wsa resource.
 *
 * To free already requested interrupt.
 *
 * Return: void.
 */
void wsa_free_irq(int irq, void *data)
{
	struct wsa_resource *wsa_res = data;

	if (wsa_res == NULL) {
		pr_err("%s: wsa_res is NULL\n", __func__);
		return;
	}
	free_irq(phyirq_to_virq(wsa_res, irq), data);
}

/**
 * wsa_enable_irq() - to enable an interrupt
 * @wsa_res: pointer to wsa resource.
 * @irq: interrupt number.
 *
 * This function is to enable an interrupt.
 *
 * Return: void.
 */
void wsa_enable_irq(struct wsa_resource *wsa_res, int irq)
{
	if (wsa_res == NULL) {
		pr_err("%s: wsa_res is NULL\n", __func__);
		return;
	}
	enable_irq(phyirq_to_virq(wsa_res, irq));
}

/**
 * wsa_disable_irq() - to disable an interrupt
 * @wsa_res: pointer to wsa resource.
 * @irq: interrupt number.
 *
 * To disable an interrupt without waiting for executing
 * handler to complete.
 *
 * Return: void.
 */
void wsa_disable_irq(struct wsa_resource *wsa_res, int irq)
{
	if (wsa_res == NULL) {
		pr_err("%s: wsa_res is NULL\n", __func__);
		return;
	}
	disable_irq_nosync(phyirq_to_virq(wsa_res, irq));
}

/**
 * wsa_disable_irq_sync() - to disable an interrupt
 * @wsa_res: pointer to wsa resource.
 * @irq: interrupt number.
 *
 * To disable an interrupt, wait for executing IRQ
 * handler to complete.
 *
 * Return: void.
 */
void wsa_disable_irq_sync(
			struct wsa_resource *wsa_res, int irq)
{
	if (wsa_res == NULL) {
		pr_err("%s: wsa_res is NULL\n", __func__);
		return;
	}
	disable_irq(phyirq_to_virq(wsa_res, irq));
}

static int wsa_irq_setup_downstream_irq(struct wsa_resource *wsa_res)
{
	int irq, virq, ret;

	if (wsa_res == NULL) {
		pr_err("%s: wsa_res is NULL\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s: enter\n", __func__);

	for (irq = 0; irq < wsa_res->num_irqs; irq++) {
		/* Map OF irq */
		virq = wsa_map_irq(wsa_res, irq);
		pr_debug("%s: irq %d -> %d\n", __func__, irq, virq);
		if (virq == NO_IRQ) {
			pr_err("%s, No interrupt specifier for irq %d\n",
			       __func__, irq);
			return NO_IRQ;
		}

		ret = irq_set_chip_data(virq, wsa_res);
		if (ret) {
			pr_err("%s: Failed to configure irq %d (%d)\n",
			       __func__, irq, ret);
			return ret;
		}

		if (wsa_res->irq_level_high[irq])
			irq_set_chip_and_handler(virq, &wsa_irq_chip,
						 handle_level_irq);
		else
			irq_set_chip_and_handler(virq, &wsa_irq_chip,
						 handle_edge_irq);

		irq_set_nested_thread(virq, 1);
	}

	pr_debug("%s: leave\n", __func__);

	return 0;
}

static int wsa_irq_init(struct wsa_resource *wsa_res)
{
	int i, ret;

	if (wsa_res == NULL) {
		pr_err("%s: wsa_res is NULL\n", __func__);
		return -EINVAL;
	}
	mutex_init(&wsa_res->irq_lock);
	mutex_init(&wsa_res->nested_irq_lock);

	wsa_res->irq = wsa_irq_get_upstream_irq(wsa_res);
	if (!wsa_res->irq) {
		pr_warn("%s: irq driver is not yet initialized\n", __func__);
		mutex_destroy(&wsa_res->irq_lock);
		mutex_destroy(&wsa_res->nested_irq_lock);
		return -EPROBE_DEFER;
	}
	pr_debug("%s: probed irq %d\n", __func__, wsa_res->irq);

	/* Setup downstream IRQs */
	ret = wsa_irq_setup_downstream_irq(wsa_res);
	if (ret) {
		pr_err("%s: Failed to setup downstream IRQ\n", __func__);
		goto fail_irq_init;
	}

	/* mask all the interrupts */
	for (i = 0; i < wsa_res->num_irqs; i++) {
		wsa_res->irq_masks_cur |= BYTE_BIT_MASK(i);
		wsa_res->irq_masks_cache |= BYTE_BIT_MASK(i);
	}

	ret = request_threaded_irq(wsa_res->irq, NULL, wsa_irq_thread,
				   IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
				   "wsa", wsa_res);
	if (ret != 0) {
		dev_err(wsa_res->dev, "Failed to request IRQ %d: %d\n",
			wsa_res->irq, ret);
	} else {
		ret = enable_irq_wake(wsa_res->irq);
		if (ret) {
			dev_err(wsa_res->dev,
				"Failed to set wake interrupt on IRQ %d: %d\n",
				wsa_res->irq, ret);
			free_irq(wsa_res->irq, wsa_res);
		}
	}

	if (ret)
		goto fail_irq_init;

	return ret;

fail_irq_init:
	dev_err(wsa_res->dev,
			"%s: Failed to init wsa irq\n", __func__);
	wsa_irq_put_upstream_irq(wsa_res);
	mutex_destroy(&wsa_res->irq_lock);
	mutex_destroy(&wsa_res->nested_irq_lock);
	return ret;
}

/**
 * wsa_request_irq() - to request/register an interrupt
 * @wsa_res: pointer to wsa_resource.
 * @irq: interrupt number.
 * @handler: interrupt handler function pointer.
 * @name: interrupt name.
 * @data: device info.
 *
 * Convert physical irq to virtual irq and then
 * reguest for threaded handler.
 *
 * Return: Retuns success/failure.
 */
int wsa_request_irq(struct wsa_resource *wsa_res,
			int irq, irq_handler_t handler,
			const char *name, void *data)
{
	int virq;

	if (wsa_res == NULL) {
		pr_err("%s: wsa_res is NULL\n", __func__);
		return -EINVAL;
	}
	virq = phyirq_to_virq(wsa_res, irq);

	/*
	 * ARM needs us to explicitly flag the IRQ as valid
	 * and will set them noprobe when we do so.
	 */
#if defined(CONFIG_ARM) || defined(CONFIG_ARM64)
	set_irq_flags(virq, IRQF_VALID);
#else
	set_irq_noprobe(virq);
#endif

	return request_threaded_irq(virq, NULL, handler, IRQF_TRIGGER_RISING,
				    name, data);
}

/**
 * wsa_irq_exit() - to disable/clear interrupt/resources
 * @wsa_res: pointer to wsa_resource
 *
 * Disable and free the interrupts and then release resources.
 *
 * Return: void.
 */
void wsa_irq_exit(struct wsa_resource *wsa_res)
{
	if (wsa_res == NULL) {
		pr_err("%s: wsa_res is NULL\n", __func__);
		return;
	}
	dev_dbg(wsa_res->dev, "%s: Cleaning up irq %d\n", __func__,
		wsa_res->irq);

	if (wsa_res->irq) {
		disable_irq_wake(wsa_res->irq);
		free_irq(wsa_res->irq, wsa_res);
		/* Release parent's of node */
		wsa_irq_put_upstream_irq(wsa_res);
	}
	mutex_destroy(&wsa_res->irq_lock);
	mutex_destroy(&wsa_res->nested_irq_lock);
}

static int phyirq_to_virq(struct wsa_resource *wsa_res, int offset)
{
	if (wsa_res == NULL) {
		pr_err("%s: wsa_res is NULL\n", __func__);
		return -EINVAL;
	}
	return irq_linear_revmap(wsa_res->domain, offset);
}

static int virq_to_phyirq(struct wsa_resource *wsa_res, int virq)
{
	struct irq_data *irq_data = irq_get_irq_data(virq);

	if (unlikely(!irq_data)) {
		pr_err("%s: irq_data is NULL\n", __func__);
		return -EINVAL;
	}
	return irq_data->hwirq;
}

static unsigned int wsa_irq_get_upstream_irq(struct wsa_resource *wsa_res)
{
	if (wsa_res == NULL) {
		pr_err("%s: wsa_res is NULL\n", __func__);
		return -EINVAL;
	}
	return wsa_res->irq;
}

static void wsa_irq_put_upstream_irq(struct wsa_resource *wsa_res)
{
	if (wsa_res == NULL) {
		pr_err("%s: wsa_res is NULL\n", __func__);
		return;
	}
	/* Hold parent's of node */
	of_node_put(wsa_res->dev->of_node);
}

static int wsa_map_irq(struct wsa_resource *wsa_res, int irq)
{
	if (wsa_res == NULL) {
		pr_err("%s: wsa_res is NULL\n", __func__);
		return -EINVAL;
	}
	return of_irq_to_resource(wsa_res->dev->of_node, irq, NULL);
}

static int wsa_irq_probe(struct platform_device *pdev)
{
	int irq;
	struct wsa_resource *wsa_res = NULL;
	int ret = -EINVAL;

	irq = platform_get_irq_byname(pdev, "wsa-int");
	if (irq < 0) {
		dev_err(&pdev->dev, "%s: Couldn't find wsa-int node(%d)\n",
			__func__, irq);
		return -EINVAL;
	}
	pr_debug("%s: node %s\n", __func__, pdev->name);
	wsa_res = kzalloc(sizeof(*wsa_res), GFP_KERNEL);
	if (!wsa_res) {
		pr_err("%s: could not allocate memory\n", __func__);
		return -ENOMEM;
	}
	/*
	 * wsa interrupt controller supports N to N irq mapping with
	 * single cell binding with irq numbers(offsets) only.
	 * Use irq_domain_simple_ops that has irq_domain_simple_map and
	 * irq_domain_xlate_onetwocell.
	 */
	wsa_res->dev = &pdev->dev;
	wsa_res->domain = irq_domain_add_linear(wsa_res->dev->of_node,
			WSA_MAX_NUM_IRQS, &irq_domain_simple_ops,
			wsa_res);
	if (!wsa_res->domain) {
		dev_err(&pdev->dev, "%s: domain is NULL\n", __func__);
		ret = -ENOMEM;
		goto err;
	}
	wsa_res->dev = &pdev->dev;

	dev_dbg(&pdev->dev, "%s: virq = %d\n", __func__, irq);
	wsa_res->irq = irq;
	wsa_res->num_irq_regs = 1;
	wsa_res->num_irqs = WSA_NUM_IRQS;
	ret = wsa_irq_init(wsa_res);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: failed to do irq init %d\n",
				__func__, ret);
		goto err;
	}

	return ret;
err:
	kfree(wsa_res);
	return ret;
}

static int wsa_irq_remove(struct platform_device *pdev)
{
	struct irq_domain *domain;
	struct wsa_resource *data;

	domain = irq_find_host(pdev->dev.of_node);
	if (unlikely(!domain)) {
		pr_err("%s: domain is NULL\n", __func__);
		return -EINVAL;
	}
	data = (struct wsa_resource *)domain->host_data;
	data->irq = 0;

	return 0;
}

static const struct of_device_id of_match[] = {
	{ .compatible = "qcom,wsa-irq" },
	{ }
};

static struct platform_driver wsa_irq_driver = {
	.probe = wsa_irq_probe,
	.remove = wsa_irq_remove,
	.driver = {
		.name = "wsa_intc",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_match),
	},
};

static int wsa_irq_drv_init(void)
{
	return platform_driver_register(&wsa_irq_driver);
}
subsys_initcall(wsa_irq_drv_init);

static void wsa_irq_drv_exit(void)
{
	platform_driver_unregister(&wsa_irq_driver);
}
module_exit(wsa_irq_drv_exit);

MODULE_DESCRIPTION("WSA881x IRQ driver");
MODULE_LICENSE("GPL v2");
