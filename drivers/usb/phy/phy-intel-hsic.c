/* phy-intel-hsic.c - Intel HSIC PHY driver.
 *
 * Copyright (C) 2014 Intel Corp.
 *
 * Author: Jincan Zhuang <jin.can.zhuang@intel.com>
 * Contributors:
 * Hang Yuan <hang.yuan@intel.com>
 * Feng Wang <feng.a.wang@intel.com>
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/usb.h>
#include <linux/usb/ch9.h>
#include <linux/usb/ch11.h>
#include "../core/hub.h"


/* GPIO names */
#define HSIC_GPIO_DISC		"hsic_disc"
#define HSIC_GPIO_WAKEUP	"hsic_wakeup"

#define HSIC_DISABLE_ENABLE_MIN_DELAY	20
#define HSIC_WAKEUP_SUSPEND_DELAY	5000

#define HSIC_RH_SUSPEND_DELAY		500
#define HSIC_DEV_SUSPEND_DELAY		500

#define hsic_dbg(hsic, fmt, args...) \
	dev_dbg(hsic->dev, fmt , ## args)

struct intel_hsic {
	struct device	*dev;
	struct device	*controller;
	spinlock_t	lock;

	/* platform specific */
	struct gpio_desc	*gpio_disc;
	struct gpio_desc	*gpio_wakeup;
	int		irq_disc;
	int		irq_wakeup;
	int		port_num;

	/* roothub and usb device associated with the HSIC port */
	struct usb_device		*rhdev;
	struct usb_device		*udev;

	/* workers */
	struct work_struct		disc_work;
	struct work_struct		wakeup_work;

	/* notifier to register for usb device ADD, REMOVE events */
	struct notifier_block		usb_nb;

	/* sysfs variables */
	int		hsic_enable;
	/* If device is connected, autosuspend_enable indicates L2 enabled
	 * otherwise, it indicates roothub autosuspend enabled
	 */
	int		autosuspend_enable;
};

/* FIXME: The global g_hsic is a hack for sysfs callback to get intel_hsic
 * data. This can be removed after we register the sysfs files under our
 * platform device
 */
struct intel_hsic *g_hsic;

static void intel_hsic_disc_work(struct work_struct *work)
{
	struct intel_hsic *hsic = container_of(work,
					struct intel_hsic, disc_work);
	struct usb_device *hdev = usb_get_dev(hsic->rhdev);
	struct usb_hub *hub;

	if (!hdev) {
		hsic_dbg(hsic, "hsic disc work, roothub is gone\n");
		return;
	}

	hub = usb_hub_to_struct_hub(hdev);

	if (!hub) {
		hsic_dbg(hsic, "hsic disc work, hub is NULL\n");
		return;
	}

	usb_lock_device(hdev);
	pm_runtime_get_sync(&hdev->dev);
	if (usb_hub_set_port_power(hdev, hub, hsic->port_num, false))
		dev_err(hsic->dev, "disc work, clear port power fail\n");
	usb_unlock_device(hdev);

	usb_notify_port_change(hdev, hsic->port_num);
	msleep(HSIC_DISABLE_ENABLE_MIN_DELAY);

	usb_lock_device(hdev);
	if (usb_hub_set_port_power(hdev, hub, hsic->port_num, true))
		dev_err(hsic->dev, "disc work, set port power fail\n");
	pm_runtime_put(&hdev->dev);
	usb_unlock_device(hdev);

	usb_put_dev(hdev);
}

static irqreturn_t intel_hsic_disc_irq(int irq, void *__hsic)
{
	struct intel_hsic *hsic = __hsic;

	hsic_dbg(hsic, "disconnect irq\n");

	if (!hsic->udev) {
		hsic_dbg(hsic, "bogus disc irq!\n");
		return IRQ_NONE;
	}

	schedule_work(&hsic->disc_work);

	return IRQ_HANDLED;
}

static void intel_hsic_wakeup_work(struct work_struct *work)
{
	struct intel_hsic *hsic = container_of(work,
					struct intel_hsic, wakeup_work);
	struct usb_device *udev = usb_get_dev(hsic->udev);

	if (!udev) {
		hsic_dbg(hsic, "hsic wakeup work, roothub is gone\n");
		return;
	}

	usb_lock_device(udev);
	pm_runtime_get_sync(&udev->dev);
	usleep_range(HSIC_WAKEUP_SUSPEND_DELAY,
			HSIC_WAKEUP_SUSPEND_DELAY + 1000);
	pm_runtime_put(&udev->dev);
	usb_unlock_device(udev);
	usb_put_dev(udev);
}

static irqreturn_t intel_hsic_wakeup_irq(int irq, void *__hsic)
{
	struct intel_hsic *hsic = __hsic;

	hsic_dbg(hsic, "wakeup irq\n");

	if (!hsic->udev) {
		hsic_dbg(hsic, "bogus wakeup irq!\n");
		return IRQ_NONE;
	}

	schedule_work(&hsic->wakeup_work);

	return IRQ_HANDLED;
}

static int intel_hsic_init_gpios(struct intel_hsic *hsic)
{
	struct device	*dev = hsic->controller;
	int ret;

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

	ret = devm_request_irq(dev, hsic->irq_disc,
			intel_hsic_disc_irq,
			IRQF_SHARED | IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND,
			HSIC_GPIO_DISC, hsic);
	if (ret)
		return ret;

	ret = devm_request_irq(dev, hsic->irq_wakeup,
			intel_hsic_wakeup_irq,
			IRQF_SHARED | IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND,
			HSIC_GPIO_WAKEUP, hsic);
	return ret;
}

static inline bool is_hsic_roothub(struct intel_hsic *hsic,
				struct usb_device *udev)
{
	struct usb_hcd *hcd = dev_get_drvdata(hsic->controller);

	return !udev->parent && udev->speed == USB_SPEED_HIGH &&
		hcd && hcd->speed == HCD_USB2 &&
		udev->bus == &hcd->self;
}

static inline bool is_hsic_dev(struct intel_hsic *hsic,
				struct usb_device *udev)
{
	return udev->parent &&
		udev->parent == hsic->rhdev &&
		udev->portnum == hsic->port_num;
}

static struct attribute_group intel_hsic_attr_group;

static void intel_hsic_dev_add(struct intel_hsic *hsic,
				struct usb_device *udev)
{
	unsigned long	flags;

	if (is_hsic_roothub(hsic, udev)) {
		struct usb_hub *hub = usb_hub_to_struct_hub(udev);

		if (!hub)
			return;

		spin_lock_irqsave(&hsic->lock, flags);
		hsic->rhdev = usb_get_dev(udev);
		spin_unlock_irqrestore(&hsic->lock, flags);

		/* roothub is added means xhci is started, we can
		 * disable the port now
		 */

		if (usb_hub_set_port_power(udev, hub, hsic->port_num, false))
			dev_err(hsic->dev,
				"disable port clear port power fail\n");

		pm_runtime_set_autosuspend_delay(&udev->dev,
					HSIC_RH_SUSPEND_DELAY);

		/* FIXME Allow xHCI to enter D3.
		 * Normally this should be done by userspace or
		 * some where else.
		 */
		device_set_run_wake(hsic->controller, true);
		pm_runtime_allow(hsic->controller);

		hsic_dbg(hsic, "%s: HSIC roothub added\n", __func__);

	} else if (is_hsic_dev(hsic, udev)) {

		device_set_wakeup_capable(&udev->dev, true);

		pm_runtime_set_autosuspend_delay(&udev->dev,
					HSIC_DEV_SUSPEND_DELAY);

		/* Telephony specific requirement, when telephony wants to
		 * enumeratethe modem, it requires by default autosuspend of
		 * modem is disabled, thus modem FW flashing won't be impacted
		 * by L2. Telephony later uses L2_autosuspend_enable to enable
		 * back autosuspend when it think it's appropriate.
		 */
		usb_disable_autosuspend(udev);

		spin_lock_irqsave(&hsic->lock, flags);
		hsic->udev = usb_get_dev(udev);
		hsic->autosuspend_enable = 0;
		spin_unlock_irqrestore(&hsic->lock, flags);

		/* Since L2 is disabled, roothub autosuspend should be
		 * enabled back, thus after L2 is enabled back by telephony,
		 * it won't block D3
		 */
		usb_enable_autosuspend(hsic->rhdev);

		/* FIXME this can be moved to hub quirks? */
		udev->persist_enabled = 0;

		hsic_dbg(hsic, "%s: HSIC device added\n", __func__);
	}
}

static void intel_hsic_dev_remove(struct intel_hsic *hsic,
				struct usb_device *udev)
{
	unsigned long	flags;

	if (is_hsic_roothub(hsic, udev)) {

		spin_lock_irqsave(&hsic->lock, flags);
		hsic->rhdev = NULL;
		spin_unlock_irqrestore(&hsic->lock, flags);

		usb_put_dev(udev);

		hsic_dbg(hsic, "%s: HSIC roothub removed\n", __func__);

	} else if (is_hsic_dev(hsic, udev)) {

		spin_lock_irqsave(&hsic->lock, flags);
		hsic->udev = NULL;
		spin_unlock_irqrestore(&hsic->lock, flags);

		usb_put_dev(udev);

		hsic_dbg(hsic, "HSIC device removed\n");
	}
}

static int intel_hsic_notify(struct notifier_block *self,
			unsigned long action, void *dev)
{
	struct intel_hsic *hsic = container_of(self,
				struct intel_hsic, usb_nb);

	switch (action) {
	case USB_DEVICE_ADD:
		intel_hsic_dev_add(hsic, dev);
		break;
	case USB_DEVICE_REMOVE:
		intel_hsic_dev_remove(hsic, dev);
		break;
	}

	return NOTIFY_OK;
}

static ssize_t intel_hsic_port_enable_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct intel_hsic *hsic = g_hsic;

	return sprintf(buf, "%d\n", hsic->hsic_enable);
}

static ssize_t intel_hsic_port_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct intel_hsic *hsic = g_hsic;
	struct usb_device *hdev;
	struct usb_hub *hub;
	unsigned long	flags;
	int enable;

	if (size > 2)
		return -EINVAL;

	if (sscanf(buf, "%d", &enable) != 1) {
		dev_err(dev, "invalid value\n");
		return -EINVAL;
	}

	hsic_dbg(hsic, "%s ----> enable = %d\n", __func__, enable);

	hdev = usb_get_dev(hsic->rhdev);
	if (!hdev) {
		hsic_dbg(hsic, "roothub not exist\n");
		return -ENODEV;
	}

	hub = usb_hub_to_struct_hub(hdev);

	if (!hub) {
		hsic_dbg(hsic, "hub not exist\n");
		usb_put_dev(hdev);
		return -ENODEV;
	}
	if (enable) {
		/* When hsic is enabled, if no device is connected,
		 * need to disable roothub autosuspend, thus xhci
		 * won't enter D3 to avoid enumeration failure.
		 */
		spin_lock_irqsave(&hsic->lock, flags);
		if (!hsic->udev) {
			hsic->autosuspend_enable = 0;
			spin_unlock_irqrestore(&hsic->lock, flags);

			usb_lock_device(hdev);
			usb_disable_autosuspend(hdev);
			usb_unlock_device(hdev);

			spin_lock_irqsave(&hsic->lock, flags);
		}
		spin_unlock_irqrestore(&hsic->lock, flags);

		usb_lock_device(hdev);
		pm_runtime_get_sync(&hdev->dev);
		if (usb_hub_set_port_power(hdev, hub, hsic->port_num, true))
			dev_err(hsic->dev, "enable port set port power fail\n");
		pm_runtime_put(&hdev->dev);
		usb_unlock_device(hdev);
	} else {
		usb_lock_device(hdev);
		pm_runtime_get_sync(&hdev->dev);
		if (usb_hub_set_port_power(hdev, hub, hsic->port_num, false))
			dev_err(hsic->dev,
				"disable port clear port power fail\n");
		pm_runtime_put(&hdev->dev);
		usb_unlock_device(hdev);

		usb_notify_port_change(hdev, hsic->port_num);

		/* When hsic is disabled, if no device is connected,
		 * need to enable roothub autosuspend, thus xhci
		 * can enter D3.
		 */
		spin_lock_irqsave(&hsic->lock, flags);
		if (!hsic->udev) {
			hsic->autosuspend_enable = 1;
			spin_unlock_irqrestore(&hsic->lock, flags);

			usb_lock_device(hdev);
			usb_enable_autosuspend(hdev);
			usb_unlock_device(hdev);

			spin_lock_irqsave(&hsic->lock, flags);
		}
		spin_unlock_irqrestore(&hsic->lock, flags);
	}
	usb_put_dev(hdev);
	hsic->hsic_enable = enable;

	return size;
}
static DEVICE_ATTR(hsic_enable, S_IRUGO | S_IWUSR | S_IROTH | S_IWOTH,
	intel_hsic_port_enable_show, intel_hsic_port_enable_store);


static ssize_t intel_hsic_autosuspend_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct intel_hsic *hsic = g_hsic;

	hsic_dbg(hsic, "%s-->, enable = %d\n",
			__func__, hsic->autosuspend_enable);
	return sprintf(buf, "%d\n", hsic->autosuspend_enable);
}

static ssize_t intel_hsic_autosuspend_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct intel_hsic *hsic = g_hsic;
	struct usb_device *udev;
	struct usb_device *hdev;
	unsigned long	flags;
	int enable;

	if (size > 2)
		return -EINVAL;

	if (sscanf(buf, "%d", &enable) != 1) {
		dev_err(dev, "invalid value\n");
		return -EINVAL;
	}

	hsic_dbg(hsic, "%s-->, enable = %d\n", __func__, enable);

	spin_lock_irqsave(&hsic->lock, flags);
	udev = usb_get_dev(hsic->udev);
	hdev = usb_get_dev(hsic->rhdev);
	if (udev) {
		hsic->autosuspend_enable = enable;
		spin_unlock_irqrestore(&hsic->lock, flags);

		usb_lock_device(udev);
		if (enable)
			usb_enable_autosuspend(udev);
		else
			usb_disable_autosuspend(udev);
		usb_unlock_device(udev);

		spin_lock_irqsave(&hsic->lock, flags);
	} else if (hdev) {
		hsic->autosuspend_enable = enable;
		spin_unlock_irqrestore(&hsic->lock, flags);

		usb_lock_device(hdev);
		if (enable)
			usb_enable_autosuspend(hdev);
		else
			usb_disable_autosuspend(hdev);
		usb_unlock_device(hdev);

		spin_lock_irqsave(&hsic->lock, flags);
	}
	spin_unlock_irqrestore(&hsic->lock, flags);

	usb_put_dev(hdev);
	usb_put_dev(udev);

	return size;
}
static DEVICE_ATTR(L2_autosuspend_enable, S_IRUGO | S_IWUSR | S_IROTH | S_IWOTH,
			intel_hsic_autosuspend_enable_show,
			intel_hsic_autosuspend_enable_store);

static struct attribute	*intel_hsic_attrs[] = {
	&dev_attr_hsic_enable.attr,
	&dev_attr_L2_autosuspend_enable.attr,
	NULL,
};

static struct attribute_group intel_hsic_attr_group = {
	.attrs = intel_hsic_attrs,
};

static int intel_hsic_probe(struct platform_device *pdev)
{
	struct intel_hsic *hsic;
	struct device *controller = pdev->dev.parent;
	int ret;

	dev_info(&pdev->dev, "intel_hsic_probe called\n");

	if (!controller)
		return -EINVAL;

	hsic = devm_kzalloc(&pdev->dev, sizeof(*hsic), GFP_KERNEL);
	if (!hsic)
		return -ENOMEM;
	hsic->dev = &pdev->dev;
	hsic->controller = controller;
	platform_set_drvdata(pdev, hsic);

	/* FIXME: hardcode some platform specific settings */
	hsic->port_num = 6;

	hsic->autosuspend_enable = 1;
	INIT_WORK(&hsic->disc_work, intel_hsic_disc_work);
	INIT_WORK(&hsic->wakeup_work, intel_hsic_wakeup_work);
	spin_lock_init(&hsic->lock);

	ret = intel_hsic_init_gpios(hsic);
	if (ret)
		return ret;

	hsic->usb_nb.notifier_call = intel_hsic_notify;
	usb_register_notify(&hsic->usb_nb);

	/* FIXME: Attributs should be registered under the platform
	 * device in the probe function. However, currently telephony
	 * uses attributes under the host controller. Need to fix this
	 * after telephony switch to use platform device attributes.
	 */
	ret = sysfs_create_group(&controller->kobj, &intel_hsic_attr_group);
	if (ret) {
		dev_err(&pdev->dev, "intel_hsic_attr_group can't be created\n");
		goto err;
	}

	/* FIXME */
	g_hsic = hsic;

	pm_runtime_no_callbacks(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	return 0;

err:
	usb_unregister_notify(&hsic->usb_nb);
	return ret;
}

static int intel_hsic_remove(struct platform_device *pdev)
{
	struct intel_hsic	*hsic = platform_get_drvdata(pdev);

	sysfs_remove_group(&hsic->controller->kobj, &intel_hsic_attr_group);
	usb_unregister_notify(&hsic->usb_nb);
	usb_put_dev(hsic->udev);
	usb_put_dev(hsic->rhdev);

	return 0;
}

static void intel_hsic_shutdown(struct platform_device *pdev)
{
	struct intel_hsic	*hsic = platform_get_drvdata(pdev);

	disable_irq(hsic->irq_disc);
	disable_irq(hsic->irq_wakeup);
}

static struct platform_driver intel_hsic_driver = {
	.probe          = intel_hsic_probe,
	.remove         = intel_hsic_remove,
	.shutdown	= intel_hsic_shutdown,
	.driver         = {
		.name   = "phy-intel-hsic",
		.owner  = THIS_MODULE,
	}
};

static int __init intel_hsic_init(void)
{
	return platform_driver_register(&intel_hsic_driver);
}
subsys_initcall(intel_hsic_init);

static void __exit intel_hsic_exit(void)
{
	platform_driver_unregister(&intel_hsic_driver);
}
module_exit(intel_hsic_exit);

MODULE_DESCRIPTION("Intel HSIC phy");
MODULE_AUTHOR("Zhuang Jin Can <jin.can.zhuang@intel.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:intel-hsic");
