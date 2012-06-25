/* Target based USB-Gadget Function
 *
 * UAS protocol handling, target callbacks, configfs handling,
 * BBB (USB Mass Storage Class Bulk-Only (BBB) and Transport protocol handling.
 *
 * Author: Sebastian Andrzej Siewior <bigeasy at linutronix dot de>
 * License: GPLv2 as published by FSF.
 */

#include <linux/init.h>
#include <linux/module.h>

#include <linux/usb/composite.h>
#include <linux/usb/gadget.h>

#include "usbstring.c"
#include "epautoconf.c"
#include "config.c"
#include "composite.c"

#include "f_tcm.c"

#define UAS_VENDOR_ID	0x0525	/* NetChip */
#define UAS_PRODUCT_ID	0xa4a5	/* Linux-USB File-backed Storage Gadget */

#define USB_G_STR_MANUFACTOR    1
#define USB_G_STR_PRODUCT       2
#define USB_G_STR_SERIAL        3
#define USB_G_STR_CONFIG        4

static struct usb_device_descriptor usbg_device_desc = {
	.bLength =		sizeof(usbg_device_desc),
	.bDescriptorType =	USB_DT_DEVICE,
	.bcdUSB =		cpu_to_le16(0x0200),
	.bDeviceClass =		USB_CLASS_PER_INTERFACE,
	.idVendor =		cpu_to_le16(UAS_VENDOR_ID),
	.idProduct =		cpu_to_le16(UAS_PRODUCT_ID),
	.iManufacturer =	USB_G_STR_MANUFACTOR,
	.iProduct =		USB_G_STR_PRODUCT,
	.iSerialNumber =	USB_G_STR_SERIAL,

	.bNumConfigurations =   1,
};

static struct usb_string	usbg_us_strings[] = {
	{ USB_G_STR_MANUFACTOR,	"Target Manufactor"},
	{ USB_G_STR_PRODUCT,	"Target Product"},
	{ USB_G_STR_SERIAL,	"000000000001"},
	{ USB_G_STR_CONFIG,	"default config"},
	{ },
};

static struct usb_gadget_strings usbg_stringtab = {
	.language = 0x0409,
	.strings = usbg_us_strings,
};

static struct usb_gadget_strings *usbg_strings[] = {
	&usbg_stringtab,
	NULL,
};

static struct usb_configuration usbg_config_driver = {
	.label                  = "Linux Target",
	.bConfigurationValue    = 1,
	.iConfiguration		= USB_G_STR_CONFIG,
	.bmAttributes           = USB_CONFIG_ATT_SELFPOWER,
};

static int usbg_cfg_bind(struct usb_configuration *c)
{
	return tcm_bind_config(c);
}

static int usb_target_bind(struct usb_composite_dev *cdev)
{
	int ret;

	ret = usb_add_config(cdev, &usbg_config_driver,
			usbg_cfg_bind);
	return ret;
}

static int guas_unbind(struct usb_composite_dev *cdev)
{
	return 0;
}

static struct usb_composite_driver usbg_driver = {
	.name           = "g_target",
	.dev            = &usbg_device_desc,
	.strings        = usbg_strings,
	.max_speed      = USB_SPEED_SUPER,
	.unbind         = guas_unbind,
};

static int usbg_attach_cb(bool connect)
{
	int ret = 0;

	if (connect)
		ret = usb_composite_probe(&usbg_driver, usb_target_bind);
	else
		usb_composite_unregister(&usbg_driver);

	return ret;
}

static int __init usb_target_gadget_init(void)
{
	int ret;

	ret = f_tcm_init(&usbg_attach_cb);
	return ret;
}
module_init(usb_target_gadget_init);

static void __exit usb_target_gadget_exit(void)
{
	f_tcm_exit();
}
module_exit(usb_target_gadget_exit);

MODULE_AUTHOR("Sebastian Andrzej Siewior <bigeasy@linutronix.de>");
MODULE_DESCRIPTION("usb-gadget fabric");
MODULE_LICENSE("GPL v2");
