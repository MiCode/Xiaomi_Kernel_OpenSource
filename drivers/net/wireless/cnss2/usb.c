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
#include "linux/delay.h"

void cnss_usb_fw_boot_timeout_hdlr(struct cnss_usb_data *usb_priv)
{
	if (!usb_priv)
		return;

	cnss_pr_err("Timeout waiting for FW ready indication\n");
}

static int cnss_qcn7605_usb_powerup(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	unsigned int timeout;

	ret = cnss_power_on_device(plat_priv);
	if (ret) {
		cnss_pr_err("Failed to power on device, err = %d\n", ret);
		goto out;
	}

	timeout = cnss_get_qmi_timeout();
	if (timeout) {
		mod_timer(&plat_priv->fw_boot_timer,
			  jiffies + msecs_to_jiffies(timeout));
	}

out:
	return ret;
}

int cnss_usb_dev_powerup(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	switch (plat_priv->device_id) {
	case QCN7605_COMPOSITE_DEVICE_ID:
	case QCN7605_STANDALONE_DEVICE_ID:
	case QCN7605_VER20_STANDALONE_DEVICE_ID:
	case QCN7605_VER20_COMPOSITE_DEVICE_ID:
		ret = cnss_qcn7605_usb_powerup(plat_priv);
		break;
	default:
		cnss_pr_err("Unknown device_id found: %lu\n",
			    plat_priv->device_id);
		ret = -ENODEV;
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

	if (plat_priv->bus_type != CNSS_BUS_USB) {
		cnss_pr_err("Wrong bus type. Expected bus_type %d\n",
			    plat_priv->bus_type);
		return -EFAULT;
	}

	usb_priv = plat_priv->bus_priv;
	usb_priv->plat_priv = plat_priv;

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

int cnss_usb_is_device_down(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return -ENODEV;
	}

	if (test_bit(CNSS_DEV_REMOVED, &plat_priv->driver_state)) {
		cnss_pr_err("usb device disconnected\n");
		return -ENODEV;
	}
	return 0;
}
EXPORT_SYMBOL(cnss_usb_is_device_down);

int cnss_usb_register_driver_hdlr(struct cnss_usb_data *usb_priv,
				  void *data)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = usb_priv->plat_priv;

	set_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state);
	usb_priv->driver_ops = data;

	if (test_bit(CNSS_FW_READY, &plat_priv->driver_state)) {
		cnss_pr_dbg("CNSS_FW_READY set - call wlan probe\n");
		ret = cnss_bus_call_driver_probe(plat_priv);
	} else {
		ret = cnss_usb_dev_powerup(usb_priv->plat_priv);
		if (ret) {
			clear_bit(CNSS_DRIVER_LOADING,
				  &plat_priv->driver_state);
			usb_priv->driver_ops = NULL;
		}
	}

	return ret;
}

int cnss_usb_unregister_driver_hdlr(struct cnss_usb_data *usb_priv)
{
	struct cnss_plat_data *plat_priv = usb_priv->plat_priv;

	set_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state);
	cnss_usb_dev_shutdown(usb_priv);
	usb_priv->driver_ops = NULL;
	usb_priv->plat_priv = NULL;
	return 0;
}

int cnss_usb_dev_shutdown(struct cnss_usb_data *usb_priv)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv;

	if (!usb_priv) {
		cnss_pr_err("usb_priv is NULL\n");
		return -ENODEV;
	}
	plat_priv = usb_priv->plat_priv;

	switch (usb_priv->device_id) {
	case QCN7605_COMPOSITE_DEVICE_ID:
	case QCN7605_STANDALONE_DEVICE_ID:
	case QCN7605_VER20_STANDALONE_DEVICE_ID:
	case QCN7605_VER20_COMPOSITE_DEVICE_ID:
		cnss_pr_dbg("cnss driver state %lu\n", plat_priv->driver_state);
		if (!test_bit(CNSS_DEV_REMOVED, &plat_priv->driver_state)) {
			cnss_usb_call_driver_remove(usb_priv);
			cnss_power_off_device(plat_priv);
		}
		clear_bit(CNSS_FW_READY, &plat_priv->driver_state);
		clear_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state);
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

	if (test_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state) ||
	    test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state)) {
		ret = usb_priv->driver_ops->probe(usb_priv->usb_intf,
						  usb_priv->usb_device_id);
		if (ret) {
			cnss_pr_err("Host drv probe failed, err = %d\n", ret);
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
		cnss_pr_dbg("Recovery set after driver probed.Call shutdown\n");
		usb_priv->driver_ops->shutdown(usb_priv->usb_intf);
	} else if (test_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state)) {
		cnss_pr_dbg("driver_ops->remove\n");
		usb_priv->driver_ops->remove(usb_priv->usb_intf);
		clear_bit(CNSS_DRIVER_PROBED, &plat_priv->driver_state);
	}
	return 0;
}

static struct usb_driver cnss_usb_driver;
#define QCN7605_WLAN_STANDALONE_INTERFACE_NUM	0x0000
#define QCN7605_WLAN_COMPOSITE_INTERFACE_NUM	0x0002

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
	usb_priv = (struct cnss_usb_data *)plat_priv->bus_priv;
	if (!usb_priv) {
		ret = -ENOMEM;
		goto out;
	}

	bcd_device = le16_to_cpu(usb_dev->descriptor.bcdDevice);
	usb_priv->plat_priv = plat_priv;
	usb_priv->usb_intf = interface;
	usb_priv->usb_device_id = id;
	usb_priv->device_id = id->idProduct;
	usb_priv->target_version = bcd_device;
	cnss_set_usb_priv(interface, usb_priv);
	plat_priv->device_id = usb_priv->device_id;

	/*increment the ref count of usb dev structure*/
	usb_get_dev(usb_dev);

	clear_bit(CNSS_DEV_REMOVED, &plat_priv->driver_state);
	ret = cnss_register_subsys(plat_priv);
	if (ret)
		goto reset_ctx;

	ret = cnss_register_ramdump(plat_priv);
	if (ret)
		goto unregister_subsys;

	switch (usb_priv->device_id) {
	case QCN7605_COMPOSITE_DEVICE_ID:
	case QCN7605_STANDALONE_DEVICE_ID:
	case QCN7605_VER20_STANDALONE_DEVICE_ID:
	case QCN7605_VER20_COMPOSITE_DEVICE_ID:
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
out:
	return ret;
}

static void cnss_usb_remove(struct usb_interface *interface)
{
	struct usb_device *usb_dev;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);
	struct cnss_usb_data *usb_priv = plat_priv->bus_priv;

	del_timer(&plat_priv->fw_boot_timer);

	clear_bit(CNSS_FW_READY, &plat_priv->driver_state);
	set_bit(CNSS_DEV_REMOVED, &plat_priv->driver_state);
	if (usb_priv->driver_ops) {
		cnss_pr_dbg("driver_ops remove state %lu\n",
			    plat_priv->driver_state);
		set_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
		usb_priv->driver_ops->update_status(usb_priv->usb_intf,
						    CNSS_FW_DOWN);
		usb_priv->driver_ops->remove(usb_priv->usb_intf);
		clear_bit(CNSS_DRIVER_PROBED, &plat_priv->driver_state);
	}
	cnss_unregister_ramdump(plat_priv);
	cnss_unregister_subsys(plat_priv);
	usb_dev = interface_to_usbdev(interface);
	usb_put_dev(usb_dev);
	usb_priv->usb_intf = NULL;
	usb_priv->usb_device_id = NULL;
	cnss_pr_dbg("driver state %lu\n", plat_priv->driver_state);
}

static int cnss_usb_suspend(struct usb_interface *interface, pm_message_t state)
{
	int ret = 0;
	struct cnss_usb_data *usb_priv;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);

	usb_priv = plat_priv->bus_priv;
	if (usb_priv->driver_ops)
		ret = usb_priv->driver_ops->suspend(usb_priv->usb_intf, state);
	else
		cnss_pr_dbg("driver_ops is NULL\n");

	return ret;
}

static int cnss_usb_resume(struct usb_interface *interface)
{
	int ret = 0;
	struct cnss_usb_data *usb_priv;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);

	usb_priv = plat_priv->bus_priv;
	if (usb_priv->driver_ops)
		ret = usb_priv->driver_ops->resume(usb_priv->usb_intf);
	else
		cnss_pr_dbg("driver_ops is NULL\n");

	return ret;
}

static int cnss_usb_reset_resume(struct usb_interface *interface)
{
	return 0;
}

static struct usb_device_id cnss_usb_id_table[] = {
	{ USB_DEVICE_INTERFACE_NUMBER(QCN7605_USB_VENDOR_ID,
				      QCN7605_COMPOSITE_PRODUCT_ID,
				      QCN7605_WLAN_COMPOSITE_INTERFACE_NUM) },
	{ USB_DEVICE_INTERFACE_NUMBER(QCN7605_USB_VENDOR_ID,
				      QCN7605_STANDALONE_PRODUCT_ID,
				      QCN7605_WLAN_STANDALONE_INTERFACE_NUM) },
	{ USB_DEVICE_INTERFACE_NUMBER(QCN7605_USB_VENDOR_ID,
				      QCN7605_VER20_STANDALONE_PID,
				      QCN7605_WLAN_STANDALONE_INTERFACE_NUM) },
	{ USB_DEVICE_INTERFACE_NUMBER(QCN7605_USB_VENDOR_ID,
				      QCN7605_VER20_COMPOSITE_PID,
				      QCN7605_WLAN_COMPOSITE_INTERFACE_NUM) },
	{}                      /* Terminating entry */
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
	struct cnss_usb_data *usb_priv;

	plat_priv->bus_priv = kzalloc(sizeof(*usb_priv), GFP_KERNEL);
	if (!plat_priv->bus_priv) {
		ret = -ENOMEM;
		goto out;
	}

	usb_priv = plat_priv->bus_priv;
	usb_priv->plat_priv = plat_priv;
	ret = usb_register(&cnss_usb_driver);
	if (ret) {
		cnss_pr_err("Failed to register to Linux USB framework, err = %d\n",
			    ret);
		goto out;
	}

	return 0;
out:
	kfree(plat_priv->bus_priv);
	return ret;
}

void cnss_usb_deinit(struct cnss_plat_data *plat_priv)
{
	kfree(plat_priv->bus_priv);
	usb_deregister(&cnss_usb_driver);
}
