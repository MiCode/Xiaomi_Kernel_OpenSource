/* intel-gpio-hsic.c - Intel HSIC GPIO Driver
 *
 * Copyright (C) 2014 Intel Corp.
 *
 * Author: Jincan Zhuang <jin.can.zhuang@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/acpi.h>
#include <linux/usb.h>
#include <linux/gpio/consumer.h>

/* FIXME: Should we move the usb_hub_set_port_power to linux/usb.h
 * to avoid ths ../..?
 */
#include <../../drivers/usb/core/hub.h>

#define HSIC_GPIO_DISC "hsic_disc"
#define HSIC_GPIO_WAKEUP "hsic_wakeup"

/* HSIC_DISCONNECT_TO_RESET = 1 ms.
 *
 * Mandating a minimum time ensures devices have sufficient time to disconnect
 * A maximum time is not defined and is mplementation-specific.
 */
#define HSIC_DISCONNECT_TO_RESET 1

/* HSIC_DISCONNECT_RESIDENCY = 5 ms.
 *
 * Mandating a minimum time ensures devices have sufficient time to disconnect
 * A maximum time is not defined and is mplementation-specific.
 */
#define HSIC_DISCONNECT_RESIDENCY 5

struct gpio_hsic {
	spinlock_t	lock;

	/* HSIC PHY platform device */
	struct device	*dev;
	/* USB controller owning the HSIC port */
	struct acpi_device	*controller;

	/* Platform specific */
	struct gpio_desc *gpio_disc;
	struct gpio_desc *gpio_wakeup;
	int		irq_disc;
	int		irq_wakeup;
	int		port_num;

	/* roothub and usb device associated with the HSIC port */
	struct usb_device		*rhdev;
	struct usb_device		*udev;

	/* notifier to register for usb device ADD, REMOVE events */
	struct notifier_block		usb_nb;
};

static inline bool is_hsic_roothub(struct gpio_hsic *hsic,
				struct usb_device *udev)
{
	acpi_handle handle;
	struct acpi_device *device;

	/* MUST be High Speed HUB */
	if (udev->parent || udev->speed != USB_SPEED_HIGH)
		return false;

	/* The acpi_device of the bus controller must be the same as
	 * hsic->controller
	 */
	handle = ACPI_HANDLE(udev->bus->controller);
	if (!handle)
		return false;

	if (acpi_bus_get_device(handle, &device))
		return false;

	return device == hsic->controller;
}

static inline bool is_hsic_dev(struct gpio_hsic *hsic,
				struct usb_device *udev)
{
	return udev->parent &&
		udev->parent == hsic->rhdev &&
		udev->portnum == hsic->port_num;
}

static void gpio_hsic_dev_add(struct gpio_hsic *hsic,
				struct usb_device *udev)
{
	unsigned long	flags;

	if (is_hsic_roothub(hsic, udev)) {

		spin_lock_irqsave(&hsic->lock, flags);
		hsic->rhdev = usb_get_dev(udev);
		spin_unlock_irqrestore(&hsic->lock, flags);

		dev_dbg(hsic->dev, "%s: HSIC roothub added\n", __func__);

	} else if (is_hsic_dev(hsic, udev)) {

		spin_lock_irqsave(&hsic->lock, flags);
		hsic->udev = usb_get_dev(udev);
		spin_unlock_irqrestore(&hsic->lock, flags);

		dev_dbg(hsic->dev, "%s: HSIC device added\n", __func__);
	}
}

static void gpio_hsic_dev_remove(struct gpio_hsic *hsic,
				struct usb_device *udev)
{
	unsigned long	flags;

	if (is_hsic_roothub(hsic, udev)) {

		spin_lock_irqsave(&hsic->lock, flags);
		hsic->rhdev = NULL;
		spin_unlock_irqrestore(&hsic->lock, flags);

		usb_put_dev(udev);

		dev_dbg(hsic->dev, "%s: HSIC roothub removed\n", __func__);

	} else if (is_hsic_dev(hsic, udev)) {

		spin_lock_irqsave(&hsic->lock, flags);
		hsic->udev = NULL;
		spin_unlock_irqrestore(&hsic->lock, flags);

		usb_put_dev(udev);

		dev_dbg(hsic->dev, "HSIC device removed\n");
	}
}

static int gpio_hsic_notify(struct notifier_block *self,
			unsigned long action, void *dev)
{
	struct gpio_hsic *hsic = container_of(self,
				struct gpio_hsic, usb_nb);

	switch (action) {
	case USB_DEVICE_ADD:
		gpio_hsic_dev_add(hsic, dev);
		break;
	case USB_DEVICE_REMOVE:
		gpio_hsic_dev_remove(hsic, dev);
		break;
	}

	return NOTIFY_OK;
}

static irqreturn_t gpio_hsic_disc_irq(int irq, void *__hsic)
{
	struct gpio_hsic *hsic = __hsic;
	struct usb_device *udev;
	struct usb_hub *hub;

	dev_dbg(hsic->dev, "disconnect irq\n");

	udev = usb_get_dev(hsic->udev);
	if (!udev) {
		dev_dbg(hsic->dev, "bogus disc irq!\n");
		return IRQ_NONE;
	}

	msleep(HSIC_DISCONNECT_TO_RESET);

	hub = usb_hub_to_struct_hub(hsic->rhdev);

	usb_lock_device(hsic->rhdev);
	pm_runtime_get_sync(&hsic->rhdev->dev);

	/* Reset HSIC port. It's done by clearing and setting back the port
	 * power. This clears the CCS bit of the portsc, and makes the port
	 * ready for a next connection.
	 */
	if (usb_hub_set_port_power(hsic->rhdev, hub, hsic->port_num, false))
		dev_err(hsic->dev, "disc work, clear port power fail\n");

	msleep(HSIC_DISCONNECT_RESIDENCY);

	if (usb_hub_set_port_power(hsic->rhdev, hub, hsic->port_num, true))
		dev_err(hsic->dev, "disc work, set port power fail\n");

	pm_runtime_put(&hsic->rhdev->dev);
	usb_unlock_device(hsic->rhdev);

	usb_put_dev(udev);

	return IRQ_HANDLED;
}

static irqreturn_t gpio_hsic_wakeup_irq(int irq, void *__hsic)
{
	struct gpio_hsic *hsic = __hsic;
	struct usb_device *udev;

	dev_dbg(hsic->dev, "wakeup irq\n");

	udev = usb_get_dev(hsic->udev);
	if (!udev) {
		dev_dbg(hsic->dev, "bogus wakeup irq!\n");
		return IRQ_NONE;
	}

	usb_lock_device(udev);
	if (!usb_autoresume_device(udev))
		usb_autosuspend_device(udev);
	usb_unlock_device(udev);
	usb_put_dev(udev);

	return IRQ_HANDLED;
}

static int gpio_hsic_init_gpios(struct gpio_hsic *hsic)
{
	int ret;
	struct device *dev = hsic->dev;

	/* FIXME: hardcoding of the index 0, 1 should be fix when upstream.
	 * However ACPI _DSD is not support in Gmin yet and we need to live
	 * with it.
	 */
	hsic->gpio_disc = devm_gpiod_get_index(dev, HSIC_GPIO_DISC, 0);
	if (IS_ERR(hsic->gpio_disc)) {
		dev_err(dev, "Can't request gpio_disc\n");
		return PTR_ERR(hsic->gpio_disc);
	}
	hsic->gpio_wakeup = devm_gpiod_get_index(dev, HSIC_GPIO_WAKEUP, 1);
	if (IS_ERR(hsic->gpio_wakeup)) {
		dev_err(dev, "Can't request gpio_wakeup\n");
		return PTR_ERR(hsic->gpio_wakeup);
	}

	ret = gpiod_direction_input(hsic->gpio_disc);
	if (ret) {
		dev_err(dev, "Can't configure gpio_disc as input\n");
		return ret;
	}
	ret = gpiod_direction_input(hsic->gpio_wakeup);
	if (ret) {
		dev_err(dev, "Can't configure gpio_wakeup as input\n");
		return ret;
	}

	ret = gpiod_to_irq(hsic->gpio_disc);
	if (ret < 0) {
		dev_err(dev, "can't get valid irq num for gpio disc\n");
		return ret;
	}
	hsic->irq_disc = ret;

	ret = gpiod_to_irq(hsic->gpio_wakeup);
	if (ret < 0) {
		dev_err(dev, "can't get valid irq num for gpio wakeup\n");
		return ret;
	}
	hsic->irq_wakeup = ret;

	ret = devm_request_threaded_irq(dev, hsic->irq_disc, NULL,
			gpio_hsic_disc_irq,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_NO_SUSPEND,
			HSIC_GPIO_DISC, hsic);
	if (ret) {
		dev_err(dev, "failed to request IRQ for irq_disc\n");
		return ret;
	}

	ret = devm_request_threaded_irq(dev, hsic->irq_wakeup, NULL,
			gpio_hsic_wakeup_irq,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT | IRQF_NO_SUSPEND,
			HSIC_GPIO_WAKEUP, hsic);
	if (ret) {
		dev_err(dev, "failed to request IRQ for irq_wakeup\n");
		return ret;
	}

	return 0;
}

static int gpio_hsic_probe(struct platform_device *pdev)
{
	struct gpio_hsic *hsic;
	struct device *dev = &pdev->dev;
	struct acpi_device *adev;
	acpi_status status;
	unsigned long long addr;
	int ret;


	adev = ACPI_COMPANION(&pdev->dev);
	if (!adev) {
		dev_err(dev, "no acpi device\n");
		return -ENODEV;
	}

	hsic = devm_kzalloc(dev, sizeof(*hsic), GFP_KERNEL);
	if (!hsic)
		return -ENOMEM;

	platform_set_drvdata(pdev, hsic);
	hsic->dev = dev;

	/* Find the acpi_device of the USB controller */
	if (!adev->parent || !adev->parent->parent ||
		!adev->parent->parent->parent) {
		dev_err(dev, "can't get controller\n");
		return -ENODEV;
	}
	hsic->controller = adev->parent->parent->parent;

	/* Get port number */
	status = acpi_evaluate_integer(adev->parent->handle, METHOD_NAME__ADR,
				NULL, &addr);
	if (ACPI_FAILURE(status)) {
		dev_err(dev, "can't get port addr\n");
		return -EINVAL;
	}
	hsic->port_num = addr;

	ret = gpio_hsic_init_gpios(hsic);
	if (ret) {
		dev_err(dev, "init gpios fail\n");
		return ret;
	}

	hsic->usb_nb.notifier_call = gpio_hsic_notify;
	usb_register_notify(&hsic->usb_nb);

	dev_info(dev, "Intel HSIC driver probe succeed\n");

	return 0;
}

static int gpio_hsic_remove(struct platform_device *pdev)
{
	struct gpio_hsic *hsic = platform_get_drvdata(pdev);

	usb_unregister_notify(&hsic->usb_nb);
	usb_put_dev(hsic->udev);
	usb_put_dev(hsic->rhdev);

	return 0;
}

static void gpio_hsic_shutdown(struct platform_device *pdev)
{
	struct gpio_hsic *hsic = platform_get_drvdata(pdev);

	disable_irq(hsic->irq_disc);
	disable_irq(hsic->irq_wakeup);
}

static const struct acpi_device_id gpio_hsic_acpi_ids[] = {
	/* FIXME: This is just a provisional phony _HID
	 * When upstream, we need a official _HID.
	 */
	{"HSP0001", 0},
	{"", 0},
};
MODULE_DEVICE_TABLE(acpi, hsic_device_ids);

static struct platform_driver gpio_hsic_driver = {
	.driver = {
		.name = "intel-gpio-hsic",
		.owner = THIS_MODULE,
		.acpi_match_table = ACPI_PTR(gpio_hsic_acpi_ids),
	},
	.probe = gpio_hsic_probe,
	.remove = gpio_hsic_remove,
	.shutdown = gpio_hsic_shutdown,
};

static int __init gpio_hsic_init(void)
{
	return platform_driver_register(&gpio_hsic_driver);
}
fs_initcall(gpio_hsic_init);

static void __exit gpio_hsic_exit(void)
{
	platform_driver_unregister(&gpio_hsic_driver);
}
module_exit(gpio_hsic_exit);

MODULE_AUTHOR("Zhuang Jin Can <jin.can.zhuang@intel.com>");
MODULE_DESCRIPTION("Intel HSIC GPIO Driver");
MODULE_LICENSE("GPL");
