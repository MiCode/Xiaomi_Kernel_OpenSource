/*
 * Copyright (C) 2017 MediaTek Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/scatterlist.h>
#include <linux/mutex.h>

#include <linux/fs.h>
#include <linux/uaccess.h>

#include <linux/usb.h>
#include "musb_core.h"

#ifdef CONFIG_MTK_MUSB_CARPLAY_SUPPORT

struct carplay_dev {
	struct usb_interface *intf;
	struct usb_device *dev;
};

struct carplay_dev *apple_dev;
bool apple;

int send_switch_cmd(void)
{
	int retval;

	if (apple_dev == NULL) {
		DBG(0, "no apple device attach.\n");
		return -1;
	}
	DBG(0, "before usb_control_msg\n");
	retval = usb_control_msg(apple_dev->dev,
				usb_rcvctrlpipe(apple_dev->dev, 0),
				0x51, 0x40, 1, 0, NULL, 0,
				USB_CTRL_GET_TIMEOUT);

	DBG(0, "after usb_control_msg retval = %d\n", retval);

	if (retval != 0) {
		DBG(0, "send_switch_cmd fail retval = %d\n", retval);
		return -1;
	}

	return 0;
}

static int carplay_probe(struct usb_interface *intf,
						const struct usb_device_id *id)
{
	struct usb_device *udev;
	struct carplay_dev *car_dev;

	DBG(0, "++ carplay probe ++\n");
	udev = interface_to_usbdev(intf);

	car_dev = kzalloc(sizeof(*car_dev), GFP_KERNEL);
	if (!car_dev)
		return -ENOMEM;

	usb_set_intfdata(intf, car_dev);
	car_dev->dev = udev;
	car_dev->intf = intf;
	apple_dev = car_dev;
	apple = true;
	if (car_dev->dev == NULL)
		DBG(0, "car_dev->dev error\n");

	return 0;
}

static void carplay_disconnect(struct usb_interface *intf)
{
	struct carplay_dev *car_dev = usb_get_intfdata(intf);

	usb_set_intfdata(intf, NULL);
	dev_dbg(&intf->dev, "disconnect\n");
	car_dev->dev = NULL;
	car_dev->intf = NULL;
	kfree(car_dev);
	apple_dev = NULL;
	apple = false;
	DBG(0, "carplay_disconnect.\n");
}

static const struct usb_device_id id_table[] = {

	/*-------------------------------------------------------------*/

	/* EZ-USB devices which download firmware to replace (or in our
	 * case augment) the default device implementation.
	 */

	/* generic EZ-USB FX2 controller (or development board) */
	{USB_DEVICE(0x05ac, 0x12a8),
	 },

	/*-------------------------------------------------------------*/

	{}
};

/* MODULE_DEVICE_TABLE(usb, id_table); */

static struct usb_driver carplay_driver = {
	.name = "carplay",
	.id_table = id_table,
	.probe = carplay_probe,
	.disconnect = carplay_disconnect,
};

/*-------------------------------------------------------------------------*/

static int __init carplay_init(void)
{
	DBG(0, "carplay_init register carplay_driver\n");
	return usb_register(&carplay_driver);
}
module_init(carplay_init);

static void __exit carplay_exit(void)
{
	usb_deregister(&carplay_driver);
}
module_exit(carplay_exit);

MODULE_DESCRIPTION("USB Core/HCD Testing Driver");
MODULE_LICENSE("GPL");
#endif
