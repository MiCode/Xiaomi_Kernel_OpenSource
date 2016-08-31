/*
 * Palmas USB transceiver driver
 *
 * Copyright (C) 2013 Texas Instruments Incorporated - http://www.ti.com
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Graeme Gregory <gg@slimlogic.co.uk>
 * Author: Kishon Vijay Abraham I <kishon@ti.com>
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * Based on twl6030_usb.c
 *
 * Author: Hema HK <hemahk@ti.com>
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/extcon.h>
#include <linux/mfd/palmas.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/sched.h>
#include <linux/workqueue.h>

enum palmas_usb_cable_id {
	USB_CABLE_INIT,
	USB_CABLE_VBUS,
	USB_CABLE_ID_GND,
	USB_CABLE_ID_A,
	USB_CABLE_ID_B,
	USB_CABLE_ID_C,
	USB_CABLE_ID_FLOAT,
};

static char const *palmas_extcon_cable[] = {
	[0] = "USB",
	[1] = "USB-Host",
	[2] = "USB-ID-A",
	[3] = "USB-ID-B",
	[4] = "USB-ID-C",
	NULL,
};

static const int mutually_exclusive[] = {0x3, 0x0};

static void palmas_usb_wakeup(struct palmas *palmas, int enable)
{
	if (enable)
		palmas_write(palmas, PALMAS_USB_OTG_BASE, PALMAS_USB_WAKEUP,
			PALMAS_USB_WAKEUP_ID_WK_UP_COMP);
	else
		palmas_write(palmas, PALMAS_USB_OTG_BASE, PALMAS_USB_WAKEUP, 0);
}

static void palmas_usb_id_int_set(struct palmas_usb *palmas_usb)
{
	unsigned int all_int;

	all_int = PALMAS_USB_ID_INT_SRC_ID_GND |
			PALMAS_USB_ID_INT_SRC_ID_A |
			PALMAS_USB_ID_INT_SRC_ID_B |
			PALMAS_USB_ID_INT_SRC_ID_C |
			PALMAS_USB_ID_INT_SRC_ID_FLOAT;
	if (palmas_usb->id_linkstat == PALMAS_USB_STATE_ID_FLOAT) {
		palmas_write(palmas_usb->palmas, PALMAS_USB_OTG_BASE,
				PALMAS_USB_ID_INT_EN_HI_SET, all_int);
		palmas_write(palmas_usb->palmas, PALMAS_USB_OTG_BASE,
				PALMAS_USB_ID_INT_EN_LO_CLR, all_int);
	} else {
		palmas_write(palmas_usb->palmas, PALMAS_USB_OTG_BASE,
				PALMAS_USB_ID_INT_EN_HI_CLR, all_int);
		palmas_write(palmas_usb->palmas, PALMAS_USB_OTG_BASE,
				PALMAS_USB_ID_INT_EN_HI_SET,
				PALMAS_USB_ID_INT_SRC_ID_FLOAT);
		palmas_write(palmas_usb->palmas, PALMAS_USB_OTG_BASE,
				PALMAS_USB_ID_INT_EN_LO_CLR, all_int);
		palmas_write(palmas_usb->palmas, PALMAS_USB_OTG_BASE,
				PALMAS_USB_ID_INT_EN_LO_SET,
				all_int ^ PALMAS_USB_ID_INT_SRC_ID_FLOAT);
	}
}

static int palmas_usb_id_state_update(struct palmas_usb *palmas_usb)
{
	unsigned int id_src;
	int ret;
	int new_state;
	int new_cable_index;
	int retry = 5;

src_again:
	ret = palmas_read(palmas_usb->palmas, PALMAS_USB_OTG_BASE,
		PALMAS_USB_ID_INT_SRC, &id_src);
	if (ret < 0) {
		dev_err(palmas_usb->dev, "ID_INT_SRC read failed: %d\n", ret);
		return ret;
	}

	dev_info(palmas_usb->dev, "id-state: 0x%02x\n", id_src);
	if (!id_src) {
		dev_err(palmas_usb->dev, "Improper ID state found\n");
		if (retry--) {
			msleep(200);
			goto src_again;
		}
		return -EAGAIN;
	}

	/* If two ID states show sign then allow debouncing to settle */
	if (id_src & (id_src - 1)) {
		dev_info(palmas_usb->dev,
			"ID states are not settled, try later\n");
		return -EAGAIN;
	}

	if (id_src & PALMAS_USB_ID_INT_SRC_ID_GND) {
		new_state = PALMAS_USB_STATE_ID_GND;
		new_cable_index = 1;
	} else if (id_src & PALMAS_USB_ID_INT_SRC_ID_A) {
		new_state = PALMAS_USB_STATE_ID_A;
		new_cable_index = 2;
	} else if (id_src & PALMAS_USB_ID_INT_SRC_ID_B) {
		new_state = PALMAS_USB_STATE_ID_B;
		new_cable_index = 3;
	} else if (id_src & PALMAS_USB_ID_INT_SRC_ID_C) {
		new_state = PALMAS_USB_STATE_ID_C;
		new_cable_index = 4;
	} else if (id_src & PALMAS_USB_ID_INT_SRC_ID_FLOAT) {
		new_state = PALMAS_USB_STATE_ID_FLOAT;
		new_cable_index = -1;
	} else {
		dev_info(palmas_usb->dev, "ID_SRC is not valid\n");
		new_state = PALMAS_USB_STATE_ID_FLOAT;
		new_cable_index = 0;
	}

	if (palmas_usb->id_linkstat == new_state) {
		dev_info(palmas_usb->dev,
			"No change in ID state: Old %d and New %d\n",
			palmas_usb->id_linkstat, new_state);
		palmas_usb_id_int_set(palmas_usb);
		return 0;
	}

	if (palmas_usb->cur_cable_index > 0) {
		dev_info(palmas_usb->dev, "Cable %s detached\n",
			palmas_extcon_cable[palmas_usb->cur_cable_index]);
		extcon_set_cable_state(&palmas_usb->edev,
			palmas_extcon_cable[palmas_usb->cur_cable_index],
			false);
	}

	if ((new_cable_index < 0) && (!palmas_usb->cur_cable_index)) {
		dev_info(palmas_usb->dev, "All cable detached\n");
		extcon_set_cable_state(&palmas_usb->edev,
					palmas_extcon_cable[1], false);
		extcon_set_cable_state(&palmas_usb->edev,
					palmas_extcon_cable[2], false);
		extcon_set_cable_state(&palmas_usb->edev,
					palmas_extcon_cable[3], false);
		extcon_set_cable_state(&palmas_usb->edev,
					palmas_extcon_cable[4], false);
	}

	palmas_usb->cur_cable_index = new_cable_index;
	palmas_usb->id_linkstat = new_state;
	if (palmas_usb->cur_cable_index <= 0)
		goto end;

	dev_info(palmas_usb->dev, "Cable %s attached\n",
		palmas_extcon_cable[palmas_usb->cur_cable_index]);
	extcon_set_cable_state(&palmas_usb->edev,
			palmas_extcon_cable[palmas_usb->cur_cable_index], true);

end:
	palmas_usb_id_int_set(palmas_usb);
	return 0;
}

static void palmas_usb_id_st_wq(struct work_struct *work)
{
	struct palmas_usb *palmas_usb;
	int ret;

	palmas_usb = container_of(work, struct palmas_usb,
			cable_update_wq.work);
	ret = palmas_usb_id_state_update(palmas_usb);
	if (ret == -EAGAIN)
		schedule_delayed_work(&palmas_usb->cable_update_wq,
			msecs_to_jiffies(palmas_usb->cable_debounce_time));
}


static irqreturn_t palmas_vbus_irq_handler(int irq, void *_palmas_usb)
{
	struct palmas_usb *palmas_usb = _palmas_usb;
	unsigned int vbus_line_state;

	palmas_read(palmas_usb->palmas, PALMAS_INTERRUPT_BASE,
		PALMAS_INT3_LINE_STATE, &vbus_line_state);

	dev_info(palmas_usb->dev, "vbus-irq() INT3_LINE_STATE 0x%02x\n",
			vbus_line_state);
	if (vbus_line_state & PALMAS_INT3_LINE_STATE_VBUS) {
		if (palmas_usb->vbus_linkstat != PALMAS_USB_STATE_VBUS) {
			palmas_usb->vbus_linkstat = PALMAS_USB_STATE_VBUS;
			extcon_set_cable_state(&palmas_usb->edev, "USB", true);
			dev_info(palmas_usb->dev, "USB cable is attached\n");
		} else {
			dev_info(palmas_usb->dev,
				"Spurious connect event detected\n");
		}
	} else if (!(vbus_line_state & PALMAS_INT3_LINE_STATE_VBUS)) {
		if (palmas_usb->vbus_linkstat == PALMAS_USB_STATE_VBUS) {
			palmas_usb->vbus_linkstat = PALMAS_USB_STATE_DISCONNECT;
			extcon_set_cable_state(&palmas_usb->edev, "USB", false);
			dev_info(palmas_usb->dev, "USB cable is detached\n");
		} else {
			dev_info(palmas_usb->dev,
				"Spurious disconnect event detected\n");
		}
	}

	return IRQ_HANDLED;
}

static irqreturn_t palmas_id_irq_handler(int irq, void *_palmas_usb)
{
	unsigned int set;
	struct palmas_usb *palmas_usb = _palmas_usb;

	palmas_read(palmas_usb->palmas, PALMAS_USB_OTG_BASE,
		PALMAS_USB_ID_INT_LATCH_SET, &set);

	palmas_write(palmas_usb->palmas, PALMAS_USB_OTG_BASE,
		PALMAS_USB_ID_INT_LATCH_CLR, set);

	schedule_delayed_work(&palmas_usb->cable_update_wq,
			msecs_to_jiffies(palmas_usb->cable_debounce_time));
	return IRQ_HANDLED;
}

static void palmas_enable_irq(struct palmas_usb *palmas_usb)
{
	int ret;

	palmas_usb->vbus_linkstat = PALMAS_USB_STATE_INIT;
	palmas_usb->id_linkstat = PALMAS_USB_STATE_INIT;
	palmas_write(palmas_usb->palmas, PALMAS_USB_OTG_BASE,
		PALMAS_USB_VBUS_CTRL_SET,
		PALMAS_USB_VBUS_CTRL_SET_VBUS_ACT_COMP);

	palmas_write(palmas_usb->palmas, PALMAS_USB_OTG_BASE,
		PALMAS_USB_ID_CTRL_CLEAR, PALMAS_USB_ID_CTRL_SET_ID_SRC_5U);
	palmas_write(palmas_usb->palmas, PALMAS_USB_OTG_BASE,
		PALMAS_USB_ID_CTRL_SET, PALMAS_USB_ID_CTRL_SET_ID_SRC_16U |
				PALMAS_USB_ID_CTRL_SET_ID_ACT_COMP);

	if (palmas_usb->enable_vbus_detection)
		palmas_vbus_irq_handler(palmas_usb->vbus_irq, palmas_usb);

	if (palmas_usb->enable_id_detection) {
		/* Wait for the comparator to update status */
		msleep(palmas_usb->cable_debounce_time);
		ret = palmas_usb_id_state_update(palmas_usb);
		if (ret == -EAGAIN)
			schedule_delayed_work(&palmas_usb->cable_update_wq,
			    msecs_to_jiffies(palmas_usb->cable_debounce_time));
	}
}

static int palmas_usb_probe(struct platform_device *pdev)
{
	struct palmas *palmas = dev_get_drvdata(pdev->dev.parent);
	struct palmas_platform_data *pdata;
	struct palmas_extcon_platform_data *epdata = NULL;
	struct device_node *node = pdev->dev.of_node;
	struct palmas_usb *palmas_usb;
	int status;
	const char *ext_name = NULL;

	palmas_usb = devm_kzalloc(&pdev->dev, sizeof(*palmas_usb), GFP_KERNEL);
	if (!palmas_usb)
		return -ENOMEM;

	pdata = dev_get_platdata(pdev->dev.parent);
	if (pdata)
		epdata = pdata->extcon_pdata;

	if (node && !epdata) {
		palmas_usb->wakeup = of_property_read_bool(node, "ti,wakeup");
		palmas_usb->enable_id_detection = of_property_read_bool(node,
						"ti,enable-id-detection");
		palmas_usb->enable_vbus_detection = of_property_read_bool(node,
						"ti,enable-vbus-detection");
		status = of_property_read_string(node, "extcon-name", &ext_name);
		if (status < 0)
			ext_name = NULL;
	} else {
		palmas_usb->wakeup = true;
		palmas_usb->enable_id_detection = true;
		palmas_usb->enable_vbus_detection = true;

		if (epdata) {
			palmas_usb->wakeup = epdata->wakeup;
			palmas_usb->enable_id_detection =
					epdata->enable_id_pin_detection;
			palmas_usb->enable_vbus_detection =
					epdata->enable_vbus_detection;
			if (palmas_usb->enable_id_detection)
				palmas_usb->wakeup = true;
			ext_name = epdata->connection_name;
		}
	}

	palmas_usb->palmas = palmas;
	palmas_usb->dev	 = &pdev->dev;
	palmas_usb->cable_debounce_time = 300;

	palmas_usb->id_otg_irq = palmas_irq_get_virq(palmas, PALMAS_ID_OTG_IRQ);
	palmas_usb->id_irq = palmas_irq_get_virq(palmas, PALMAS_ID_IRQ);
	palmas_usb->vbus_otg_irq = palmas_irq_get_virq(palmas,
						PALMAS_VBUS_OTG_IRQ);
	palmas_usb->vbus_irq = palmas_irq_get_virq(palmas, PALMAS_VBUS_IRQ);

	palmas_usb_wakeup(palmas, palmas_usb->wakeup);

	platform_set_drvdata(pdev, palmas_usb);

	palmas_usb->edev.supported_cable = palmas_extcon_cable;
	palmas_usb->edev.mutually_exclusive = mutually_exclusive;
	palmas_usb->edev.name  = (ext_name) ? ext_name : dev_name(&pdev->dev);

	status = extcon_dev_register(&palmas_usb->edev, palmas_usb->dev);
	if (status < 0) {
		dev_err(&pdev->dev, "failed to register extcon device\n");
		return status;
	}

	if (palmas_usb->enable_id_detection) {
		status = devm_request_threaded_irq(palmas_usb->dev,
				palmas_usb->id_irq,
				NULL, palmas_id_irq_handler,
				IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING |
				IRQF_ONESHOT | IRQF_EARLY_RESUME,
				"palmas_usb_id", palmas_usb);
		if (status < 0) {
			dev_err(&pdev->dev, "can't get IRQ %d, err %d\n",
					palmas_usb->id_irq, status);
			goto fail_extcon;
		}
		status = devm_request_threaded_irq(palmas_usb->dev,
				palmas_usb->id_otg_irq,
				NULL, palmas_id_irq_handler,
				IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING |
				IRQF_ONESHOT | IRQF_EARLY_RESUME,
				"palmas_usb_id-otg", palmas_usb);
		if (status < 0) {
			dev_err(&pdev->dev, "can't get IRQ %d, err %d\n",
					palmas_usb->id_irq, status);
			goto fail_extcon;
		}
		INIT_DELAYED_WORK(&palmas_usb->cable_update_wq,
				palmas_usb_id_st_wq);
	}

	if (palmas_usb->enable_vbus_detection) {
		status = devm_request_threaded_irq(palmas_usb->dev,
				palmas_usb->vbus_irq, NULL,
				palmas_vbus_irq_handler,
				IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING |
				IRQF_ONESHOT | IRQF_EARLY_RESUME,
				"palmas_usb_vbus", palmas_usb);
		if (status < 0) {
			dev_err(&pdev->dev, "can't get IRQ %d, err %d\n",
					palmas_usb->vbus_irq, status);
			goto fail_extcon;
		}
	}

	palmas_enable_irq(palmas_usb);
	device_set_wakeup_capable(&pdev->dev, true);
	return 0;

fail_extcon:
	extcon_dev_unregister(&palmas_usb->edev);
	if (palmas_usb->enable_id_detection)
		cancel_delayed_work(&palmas_usb->cable_update_wq);

	return status;
}

static int palmas_usb_remove(struct platform_device *pdev)
{
	struct palmas_usb *palmas_usb = platform_get_drvdata(pdev);

	extcon_dev_unregister(&palmas_usb->edev);

	if (palmas_usb->enable_id_detection)
		cancel_delayed_work(&palmas_usb->cable_update_wq);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int palmas_usb_suspend(struct device *dev)
{
	struct palmas_usb *palmas_usb = dev_get_drvdata(dev);

	if (device_may_wakeup(dev)) {
		if (palmas_usb->enable_vbus_detection)
			enable_irq_wake(palmas_usb->vbus_irq);
		if (palmas_usb->enable_id_detection)
			enable_irq_wake(palmas_usb->id_irq);
	}
	return 0;
}

static int palmas_usb_resume(struct device *dev)
{
	struct palmas_usb *palmas_usb = dev_get_drvdata(dev);

	if (device_may_wakeup(dev)) {
		if (palmas_usb->enable_vbus_detection)
			disable_irq_wake(palmas_usb->vbus_irq);
		if (palmas_usb->enable_id_detection)
			disable_irq_wake(palmas_usb->id_irq);
	}
	return 0;
};
#endif

static const struct dev_pm_ops palmas_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(palmas_usb_suspend,
				palmas_usb_resume)
};

static struct of_device_id of_palmas_match_tbl[] = {
	{ .compatible = "ti,palmas-usb", },
	{ .compatible = "ti,twl6035-usb", },
	{ /* end */ }
};

static struct platform_driver palmas_usb_driver = {
	.probe = palmas_usb_probe,
	.remove = palmas_usb_remove,
	.driver = {
		.name = "palmas-usb",
		.of_match_table = of_palmas_match_tbl,
		.owner = THIS_MODULE,
		.pm = &palmas_pm_ops,
	},
};

static int __init palmas_usb_driver_init(void)
{
	return platform_driver_register(&palmas_usb_driver);
}
subsys_initcall_sync(palmas_usb_driver_init);

static void __exit palmas_usb_driver_exit(void)
{
	platform_driver_unregister(&palmas_usb_driver);
}
module_exit(palmas_usb_driver_exit);

MODULE_ALIAS("platform:palmas-usb");
MODULE_AUTHOR("Graeme Gregory <gg@slimlogic.co.uk>");
MODULE_DESCRIPTION("Palmas USB transceiver driver");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(of, of_palmas_match_tbl);
