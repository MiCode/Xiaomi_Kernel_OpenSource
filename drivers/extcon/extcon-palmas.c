/*
 * palmas-extcon.c -- Palmas VBUS detection in extcon framework.
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */
#include <linux/module.h>
#include <linux/err.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/pm.h>
#include <linux/extcon.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/palmas.h>

#define MAX_INT_NAME	40
struct palmas_extcon {
	struct device		*dev;
	struct palmas		*palmas;
	struct extcon_dev	*edev;
	int			vbus_irq;
	int			id_irq;
	char			vbus_irq_name[MAX_INT_NAME];
	char			id_irq_name[MAX_INT_NAME];
	bool			enable_vbus_detection;
	bool			enable_id_pin_detection;
};

const char *palmas_excon_cable[] = {
	[0] = "USB",
	[1] = "USB-Host",
	NULL,
};

static int palmas_extcon_vbus_cable_update(
		struct palmas_extcon *palma_econ)
{
	int ret;
	unsigned int status;

	ret = palmas_read(palma_econ->palmas, PALMAS_INTERRUPT_BASE,
				PALMAS_INT3_LINE_STATE,	&status);
	if (ret < 0) {
		dev_err(palma_econ->dev,
			"INT3_LINE_STATE read failed: %d\n", ret);
		return ret;
	}

	if (status & PALMAS_INT3_LINE_STATE_VBUS)
		extcon_set_cable_state(palma_econ->edev, "USB", true);
	else
		extcon_set_cable_state(palma_econ->edev, "USB", false);

	dev_info(palma_econ->dev, "VBUS %s status: 0x%02x\n",
		(status & PALMAS_INT3_LINE_STATE_VBUS) ? "Valid" : "Invalid",
		status);

	return 0;
}

static int palmas_extcon_id_cable_update(
		struct palmas_extcon *palma_econ)
{
	int ret;
	unsigned int status;

	ret = palmas_read(palma_econ->palmas, PALMAS_INTERRUPT_BASE,
				PALMAS_INT3_LINE_STATE,	&status);
	if (ret < 0) {
		dev_err(palma_econ->dev,
			"INT3_LINE_STATE read failed: %d\n", ret);
		return ret;
	}

	if (status & PALMAS_INT3_LINE_STATE_ID)
		extcon_set_cable_state(palma_econ->edev, "USB-Host", true);
	else
		extcon_set_cable_state(palma_econ->edev, "USB-Host", false);

	dev_info(palma_econ->dev, "ID %s status: 0x%02x\n",
		(status & PALMAS_INT3_LINE_STATE_ID) ? "Valid" : "Invalid",
		status);

	return 0;
}

static irqreturn_t palmas_extcon_irq(int irq, void *data)
{
	struct palmas_extcon *palma_econ = data;

	if (irq == palma_econ->vbus_irq)
		palmas_extcon_vbus_cable_update(palma_econ);
	else if (irq == palma_econ->id_irq)
		palmas_extcon_id_cable_update(palma_econ);
	else
		dev_err(palma_econ->dev, "Unknown interrupt %d\n", irq);

	return IRQ_HANDLED;
}

static int __devinit palmas_extcon_probe(struct platform_device *pdev)
{
	struct palmas_platform_data *pdata;
	struct palmas_extcon_platform_data *epdata = NULL;
	struct palmas_extcon *palma_econ;
	struct extcon_dev *edev;
	int ret;

	pdata = dev_get_platdata(pdev->dev.parent);
	if (pdata)
		epdata = pdata->extcon_pdata;
	if (!epdata) {
		dev_err(&pdev->dev, "No platform data\n");
		return -EINVAL;
	}

	palma_econ = devm_kzalloc(&pdev->dev, sizeof(*palma_econ), GFP_KERNEL);
	if (!palma_econ) {
		dev_err(&pdev->dev, "Memory alloc failed for palma_econ\n");
		return -ENOMEM;
	}

	edev = devm_kzalloc(&pdev->dev, sizeof(*edev), GFP_KERNEL);
	if (!edev) {
		dev_err(&pdev->dev, "Memory allocation failed for edev\n");
		return -ENOMEM;
	}

	palma_econ->edev = edev;
	palma_econ->edev->name = (epdata->connection_name) ?
				epdata->connection_name : pdev->name;
	palma_econ->edev->supported_cable = palmas_excon_cable;

	palma_econ->dev = &pdev->dev;
	palma_econ->palmas = dev_get_drvdata(pdev->dev.parent);
	dev_set_drvdata(&pdev->dev, palma_econ);

	palma_econ->enable_vbus_detection = epdata->enable_vbus_detection;
	palma_econ->enable_id_pin_detection = epdata->enable_id_pin_detection;
	palma_econ->vbus_irq = platform_get_irq(pdev, 0);
	palma_econ->id_irq = platform_get_irq(pdev, 1);

	ret = extcon_dev_register(palma_econ->edev, NULL);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to register extcon device\n");
		return ret;
	}

	/* Set initial state */
	if (epdata->enable_vbus_detection) {
		ret = palmas_extcon_vbus_cable_update(palma_econ);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"VBUS Cable init failed: %d\n", ret);
			goto out;
		}
		snprintf(palma_econ->vbus_irq_name, MAX_INT_NAME,
			"vbus-%s\n", dev_name(palma_econ->dev));
		ret = request_threaded_irq(palma_econ->vbus_irq, NULL,
			palmas_extcon_irq, IRQF_ONESHOT | IRQF_EARLY_RESUME,
			palma_econ->vbus_irq_name, palma_econ);
		if (ret < 0) {
			dev_err(palma_econ->dev, "request irq %d failed: %d\n",
				palma_econ->vbus_irq, ret);
			goto out;
		}
	}

	if (epdata->enable_id_pin_detection) {
		ret = palmas_update_bits(palma_econ->palmas,
				PALMAS_USB_OTG_BASE,
				PALMAS_USB_WAKEUP,
				PALMAS_USB_WAKEUP_ID_WK_UP_COMP, 1);
		if (ret < 0) {
			dev_err(palma_econ->dev,
				"USB_WAKEUP write failed: %d\n", ret);
			goto out_free_vbus;
		}

		ret = palmas_extcon_id_cable_update(palma_econ);
		if (ret < 0) {
			dev_err(&pdev->dev, "ID Cable init failed: %d\n", ret);
			goto out;
		}
		snprintf(palma_econ->id_irq_name, MAX_INT_NAME,
			"id-%s\n", dev_name(palma_econ->dev));
		ret = request_threaded_irq(palma_econ->id_irq, NULL,
			palmas_extcon_irq, IRQF_ONESHOT | IRQF_EARLY_RESUME,
			palma_econ->id_irq_name, palma_econ);
		if (ret < 0) {
			dev_err(palma_econ->dev, "request irq %d failed: %d\n",
				palma_econ->id_irq, ret);
			goto out_free_vbus;
		}
	}

	device_set_wakeup_capable(&pdev->dev, 1);
	return 0;
out_free_vbus:
	if (epdata->enable_vbus_detection)
		free_irq(palma_econ->vbus_irq, palma_econ);
out:
	extcon_dev_unregister(palma_econ->edev);
	return ret;
}

static int __devexit palmas_extcon_remove(struct platform_device *pdev)
{
	struct palmas_extcon *palma_econ = dev_get_drvdata(&pdev->dev);

	extcon_dev_unregister(palma_econ->edev);
	if (palma_econ->enable_vbus_detection)
		free_irq(palma_econ->vbus_irq, palma_econ);
	if (palma_econ->enable_id_pin_detection)
		free_irq(palma_econ->id_irq, palma_econ);
	return 0;
}

static void palmas_extcon_shutdown(struct platform_device *pdev)
{
	struct palmas_extcon *palma_econ = dev_get_drvdata(&pdev->dev);
	int ret;

	ret = palmas_update_bits(palma_econ->palmas,
			PALMAS_USB_OTG_BASE,
			PALMAS_USB_WAKEUP,
			PALMAS_USB_WAKEUP_ID_WK_UP_COMP, 0);

	if (ret < 0) {
		dev_err(palma_econ->dev,
			"USB_WAKEUP write failed: %d at shutdown\n", ret);
	}
}

#ifdef CONFIG_PM_SLEEP
static int palmas_extcon_suspend(struct device *dev)
{
	struct palmas_extcon *palma_econ = dev_get_drvdata(dev);

	if (device_may_wakeup(dev)) {
		if (palma_econ->enable_vbus_detection)
			enable_irq_wake(palma_econ->vbus_irq);
		if (palma_econ->enable_id_pin_detection)
			enable_irq_wake(palma_econ->id_irq);
	}
	return 0;
}

static int palmas_extcon_resume(struct device *dev)
{
	struct palmas_extcon *palma_econ = dev_get_drvdata(dev);

	if (device_may_wakeup(dev)) {
		if (palma_econ->enable_vbus_detection)
			disable_irq_wake(palma_econ->vbus_irq);
		if (palma_econ->enable_id_pin_detection)
			disable_irq_wake(palma_econ->id_irq);
	}
	return 0;
};
#endif

static const struct dev_pm_ops palmas_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(palmas_extcon_suspend,
				palmas_extcon_resume)
};

static struct platform_driver palmas_extcon_driver = {
	.probe = palmas_extcon_probe,
	.remove = __devexit_p(palmas_extcon_remove),
	.shutdown = palmas_extcon_shutdown,
	.driver = {
		.name = "palmas-extcon",
		.owner = THIS_MODULE,
		.pm = &palmas_pm_ops,
	},
};

static int __init palmas_extcon_driver_init(void)
{
	return platform_driver_register(&palmas_extcon_driver);
}
subsys_initcall_sync(palmas_extcon_driver_init);

static void __exit palmas_extcon_driver_exit(void)
{
	platform_driver_unregister(&palmas_extcon_driver);
}
module_exit(palmas_extcon_driver_exit);

MODULE_DESCRIPTION("palmas extcon driver");
MODULE_AUTHOR("Laxman Dewangan<ldewangan@nvidia.com>");
MODULE_ALIAS("platform:palmas-extcon");
MODULE_LICENSE("GPL v2");
