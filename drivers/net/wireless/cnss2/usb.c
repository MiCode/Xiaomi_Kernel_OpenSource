/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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

#include "main.h"
#include "bus.h"
#include "debug.h"
#include "usb.h"

int cnss_usb_dev_powerup(struct cnss_usb_data *usb_priv)
{
	int ret = 0;

	if (!usb_priv) {
		cnss_pr_err("usb_priv is NULL\n");
		return -ENODEV;
	}
	return ret;
}

int cnss_usb_wlan_register_driver(struct cnss_usb_wlan_driver *driver_ops)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);
	struct cnss_usb_data *usb_priv;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return -ENODEV;
	}

	usb_priv = plat_priv->bus_priv;
	if (!usb_priv) {
		cnss_pr_err("usb_priv is NULL\n");
		return -ENODEV;
	}

	if (usb_priv->driver_ops) {
		cnss_pr_err("Driver has already registered\n");
		return -EEXIST;
	}

	ret = cnss_driver_event_post(plat_priv,
				     CNSS_DRIVER_EVENT_REGISTER_DRIVER,
				     CNSS_EVENT_SYNC_UNINTERRUPTIBLE,
				     driver_ops);
	return ret;
}
EXPORT_SYMBOL(cnss_usb_wlan_register_driver);

void cnss_usb_wlan_unregister_driver(struct cnss_usb_wlan_driver *driver_ops)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return;
	}

	cnss_driver_event_post(plat_priv,
			       CNSS_DRIVER_EVENT_UNREGISTER_DRIVER,
			       CNSS_EVENT_SYNC_UNINTERRUPTIBLE, NULL);
}
EXPORT_SYMBOL(cnss_usb_wlan_unregister_driver);

int cnss_usb_register_driver_hdlr(struct cnss_usb_data *usb_priv,
				  void *data)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = usb_priv->plat_priv;

	set_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state);
	usb_priv->driver_ops = data;

	ret = cnss_bus_call_driver_probe(plat_priv);

	return ret;
}

int cnss_usb_unregister_driver_hdlr(struct cnss_usb_data *usb_priv)
{
	struct cnss_plat_data *plat_priv = usb_priv->plat_priv;

	set_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state);
	cnss_usb_dev_shutdown(usb_priv);
	usb_priv->driver_ops = NULL;

	return 0;
}

int cnss_usb_dev_shutdown(struct cnss_usb_data *usb_priv)
{
	int ret = 0;

	if (!usb_priv) {
		cnss_pr_err("usb_priv is NULL\n");
		return -ENODEV;
	}

	switch (usb_priv->device_id) {
	case QCN7605_COMPOSITE_DEVICE_ID:
	case QCN7605_STANDALONE_DEVICE_ID:
		break;
	default:
		cnss_pr_err("Unknown device_id found: 0x%x\n",
			    usb_priv->device_id);
		ret = -ENODEV;
	}
	return ret;
}

int cnss_usb_call_driver_probe(struct cnss_usb_data *usb_priv)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = usb_priv->plat_priv;

	if (!usb_priv->driver_ops) {
		cnss_pr_err("driver_ops is NULL\n");
		ret = -EINVAL;
		goto out;
	}

	if (test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state) &&
	    test_bit(CNSS_DRIVER_PROBED, &plat_priv->driver_state)) {
		ret = usb_priv->driver_ops->reinit(usb_priv->usb_intf,
						   usb_priv->usb_device_id);
		if (ret) {
			cnss_pr_err("Failed to reinit host driver, err = %d\n",
				    ret);
			goto out;
		}
		clear_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
	} else if (test_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state)) {
		ret = usb_priv->driver_ops->probe(usb_priv->usb_intf,
						  usb_priv->usb_device_id);
		if (ret) {
			cnss_pr_err("Failed to probe host driver, err = %d\n",
				    ret);
			goto out;
		}
		clear_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
		clear_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state);
		set_bit(CNSS_DRIVER_PROBED, &plat_priv->driver_state);
	}

	return 0;

out:
	return ret;
}

int cnss_usb_call_driver_remove(struct cnss_usb_data *usb_priv)
{
	struct cnss_plat_data *plat_priv = usb_priv->plat_priv;

	if (test_bit(CNSS_COLD_BOOT_CAL, &plat_priv->driver_state) ||
	    test_bit(CNSS_FW_BOOT_RECOVERY, &plat_priv->driver_state) ||
	    test_bit(CNSS_DRIVER_DEBUG, &plat_priv->driver_state)) {
		cnss_pr_dbg("Skip driver remove\n");
		return 0;
	}

	if (!usb_priv->driver_ops) {
		cnss_pr_err("driver_ops is NULL\n");
		return -EINVAL;
	}

	if (test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state) &&
	    test_bit(CNSS_DRIVER_PROBED, &plat_priv->driver_state)) {
		usb_priv->driver_ops->shutdown(usb_priv->usb_intf);
	} else if (test_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state)) {
		usb_priv->driver_ops->remove(usb_priv->usb_intf);
		clear_bit(CNSS_DRIVER_PROBED, &plat_priv->driver_state);
	}

	return 0;
}

static struct usb_driver cnss_usb_driver;
#define QCN7605_WLAN_INTERFACE_NUM      0x0000

static int cnss_usb_probe(struct usb_interface *interface,
			  const struct usb_device_id *id)
{
	int ret = 0;
	struct cnss_usb_data *usb_priv;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);
	struct usb_device *usb_dev;
	unsigned short bcd_device;

	cnss_pr_dbg("USB probe, vendor ID: 0x%x, product ID: 0x%x\n",
		    id->idVendor, id->idProduct);

	usb_dev = interface_to_usbdev(interface);
	usb_priv = devm_kzalloc(&usb_dev->dev, sizeof(*usb_priv),
				GFP_KERNEL);
	if (!usb_priv) {
		ret = -ENOMEM;
		goto out;
	}

	if (interface->cur_altsetting->desc.bInterfaceNumber ==
	    QCN7605_WLAN_INTERFACE_NUM) {
		if (usb_driver_claim_interface(&cnss_usb_driver,
					       interface,
					       NULL)) {
			ret = -ENODEV;
			goto reset_priv;
		}
	}
	bcd_device = le16_to_cpu(usb_dev->descriptor.bcdDevice);
	usb_priv->plat_priv = plat_priv;
	usb_priv->usb_intf = interface;
	usb_priv->usb_device_id = id;
	usb_priv->device_id = id->idProduct;
	usb_priv->target_version = bcd_device;
	cnss_set_usb_priv(interface, usb_priv);
	plat_priv->device_id = usb_priv->device_id;
	plat_priv->bus_priv = usb_priv;

	/*increment the ref count of usb dev structure*/
	usb_get_dev(usb_dev);

	ret = cnss_register_subsys(plat_priv);
	if (ret)
		goto reset_ctx;

	ret = cnss_register_ramdump(plat_priv);
	if (ret)
		goto unregister_subsys;

	switch (usb_priv->device_id) {
	case QCN7605_COMPOSITE_DEVICE_ID:
	case QCN7605_STANDALONE_DEVICE_ID:
		break;
	default:
		cnss_pr_err("Unknown USB device found: 0x%x\n",
			    usb_priv->device_id);
		ret = -ENODEV;
		goto unregister_ramdump;
	}

	return 0;

unregister_ramdump:
	cnss_unregister_ramdump(plat_priv);
unregister_subsys:
	cnss_unregister_subsys(plat_priv);
reset_ctx:
	plat_priv->bus_priv = NULL;
reset_priv:
	devm_kfree(&usb_dev->dev, usb_priv);
out:
	return ret;
}

static void cnss_usb_remove(struct usb_interface *interface)
{
	struct usb_device *usb_dev;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);
	struct cnss_usb_data *usb_priv = plat_priv->bus_priv;

	usb_priv->plat_priv = NULL;
	plat_priv->bus_priv = NULL;
	usb_dev = interface_to_usbdev(interface);
	usb_put_dev(usb_dev);
	devm_kfree(&usb_dev->dev, usb_priv);
}

static int cnss_usb_suspend(struct usb_interface *interface, pm_message_t state)
{
	int ret = 0;
	struct cnss_usb_data *usb_priv;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);

	usb_priv = plat_priv->bus_priv;
	if (!usb_priv->driver_ops) {
		cnss_pr_err("driver_ops is NULL\n");
		ret = -EINVAL;
		goto out;
	}
	ret = usb_priv->driver_ops->suspend(usb_priv->usb_intf,
						  state);
out:
	return ret;
}

static int cnss_usb_resume(struct usb_interface *interface)
{
	int ret = 0;
	struct cnss_usb_data *usb_priv;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);

	usb_priv = plat_priv->bus_priv;
	if (!usb_priv->driver_ops) {
		cnss_pr_err("driver_ops is NULL\n");
		ret = -EINVAL;
		goto out;
	}
	ret = usb_priv->driver_ops->resume(usb_priv->usb_intf);

out:
	return ret;
}

static int cnss_usb_reset_resume(struct usb_interface *interface)
{
	return 0;
}

static struct usb_device_id cnss_usb_id_table[] = {
	{ USB_DEVICE_AND_INTERFACE_INFO(QCN7605_USB_VENDOR_ID,
				       QCN7605_COMPOSITE_PRODUCT_ID,
				       QCN7605_WLAN_INTERFACE_NUM,
				       0xFF, 0xFF) },
	{ USB_DEVICE_AND_INTERFACE_INFO(QCN7605_USB_VENDOR_ID,
				       QCN7605_STANDALONE_PRODUCT_ID,
				       QCN7605_WLAN_INTERFACE_NUM,
				       0xFF, 0xFF) },
	{}			/* Terminating entry */
};

static struct usb_driver cnss_usb_driver = {
	.name       = "cnss_usb",
	.id_table   = cnss_usb_id_table,
	.probe      = cnss_usb_probe,
	.disconnect = cnss_usb_remove,
	.suspend    = cnss_usb_suspend,
	.resume     = cnss_usb_resume,
	.reset_resume = cnss_usb_reset_resume,
	.supports_autosuspend = true,
};

int cnss_usb_init(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	ret = usb_register(&cnss_usb_driver);
	if (ret) {
		cnss_pr_err("Failed to register to Linux USB framework, err = %d\n",
			    ret);
		goto out;
	}

	return 0;
out:
	return ret;
}

void cnss_usb_deinit(struct cnss_plat_data *plat_priv)
{
	usb_deregister(&cnss_usb_driver);
}
