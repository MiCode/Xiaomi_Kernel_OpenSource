/*
 * Qualcomm Maemo Composite driver
 *
 * Copyright (C) 2008 David Brownell
 * Copyright (C) 2008 Nokia Corporation
 * Copyright (C) 2009 Samsung Electronics
 * Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program from the Code Aurora Forum is free software; you can
 * redistribute it and/or modify it under the GNU General Public License
 * version 2 and only version 2 as published by the Free Software Foundation.
 * The original work available from [git.kernel.org ] is subject to the
 * notice below.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/kernel.h>
#include <linux/utsname.h>
#include <linux/kdev_t.h>
#include <linux/delay.h>


#define DRIVER_DESC		"Qcom Maemo Composite Gadget"
#define VENDOR_ID		0x05c6
#define PRODUCT_ID		0x902E

/*
 * kbuild is not very cooperative with respect to linking separately
 * compiled library objects into one module.  So for now we won't use
 * separate compilation ... ensuring init/exit sections work to shrink
 * the runtime footprint, and giving us at least some parts of what
 * a "gcc --combine ... part1.c part2.c part3.c ... " build would.
 */

#include "composite.c"
#include "usbstring.c"
#include "config.c"
#include "epautoconf.c"

#define USB_ETH

#define USB_ETH_RNDIS
#ifdef USB_ETH_RNDIS
#  include "f_rndis.c"
#  include "rndis.c"
#endif


#include "u_serial.c"
#include "f_serial.c"

#include "u_ether.c"

#undef DBG     /* u_ether.c has broken idea about macros */
#undef VDBG    /* so clean up after it */
#undef ERROR
#undef INFO

#include "f_mass_storage.c"
#include "f_diag.c"
#include "f_rmnet.c"

/*-------------------------------------------------------------------------*/
/* string IDs are assigned dynamically */

#define STRING_MANUFACTURER_IDX         0
#define STRING_PRODUCT_IDX              1
#define STRING_SERIAL_IDX               2

/* String Table */
static struct usb_string strings_dev[] = {
	/* These dummy values should be overridden by platform data */
	[STRING_MANUFACTURER_IDX].s = "Qualcomm Incorporated",
	[STRING_PRODUCT_IDX].s = "Usb composition",
	[STRING_SERIAL_IDX].s = "0123456789ABCDEF",
	{  }                    /* end of list */
};

static struct usb_gadget_strings stringtab_dev = {
	.language       = 0x0409,       /* en-us */
	.strings        = strings_dev,
};

static struct usb_gadget_strings *dev_strings[] = {
	&stringtab_dev,
	NULL,
};

static struct usb_device_descriptor device_desc = {
	.bLength              = sizeof(device_desc),
	.bDescriptorType      = USB_DT_DEVICE,
	.bcdUSB               = __constant_cpu_to_le16(0x0200),
	.bDeviceClass         = USB_CLASS_PER_INTERFACE,
	.bDeviceSubClass      =      0,
	.bDeviceProtocol      =      0,
	.idVendor             = __constant_cpu_to_le16(VENDOR_ID),
	.idProduct            = __constant_cpu_to_le16(PRODUCT_ID),
	.bcdDevice            = __constant_cpu_to_le16(0xffff),
	.bNumConfigurations   = 1,
};

static u8 hostaddr[ETH_ALEN];
static struct usb_diag_ch *diag_ch;
static struct usb_diag_platform_data usb_diag_pdata = {
	.ch_name = DIAG_LEGACY,
};

/****************************** Configurations ******************************/
static struct fsg_module_parameters mod_data = {
	.stall = 0
};
FSG_MODULE_PARAMETERS(/* no prefix */, mod_data);

static struct fsg_common *fsg_common;
static int maemo_setup_config(struct usb_configuration *c,
			const struct usb_ctrlrequest *ctrl);

static int maemo_do_config(struct usb_configuration *c)
{
	int ret;

	ret = rndis_bind_config(c, hostaddr);
	if (ret < 0)
		return ret;

	ret = diag_function_add(c);
	if (ret < 0)
		return ret;

	ret = gser_bind_config(c, 0);
	if (ret < 0)
		return ret;

	ret = gser_bind_config(c, 1);
	if (ret < 0)
		return ret;

	ret = rmnet_function_add(c);
	if (ret < 0)
		return ret;

	ret = fsg_add(c->cdev, c, fsg_common);
	if (ret < 0)
		return ret;

	return 0;
}

static struct usb_configuration maemo_config_driver = {
	.label			= "Qcom Maemo Gadget",
	.bind			= maemo_do_config,
	.setup			= maemo_setup_config,
	.bConfigurationValue	= 1,
	.bMaxPower		= 0xFA,
};
static int maemo_setup_config(struct usb_configuration *c,
		const struct usb_ctrlrequest *ctrl)
{
	int i;
	int ret = -EOPNOTSUPP;

	for (i = 0; i < maemo_config_driver.next_interface_id; i++) {
		if (maemo_config_driver.interface[i]->setup) {
			ret = maemo_config_driver.interface[i]->setup(
				maemo_config_driver.interface[i], ctrl);
			if (ret >= 0)
				return ret;
		}
	}

	return ret;
}

static int maemo_bind(struct usb_composite_dev *cdev)
{
	struct usb_gadget *gadget = cdev->gadget;
	int status, gcnum;

	/* set up diag channel */
	diag_ch = diag_setup(&usb_diag_pdata);
	if (IS_ERR(diag_ch))
		return PTR_ERR(diag_ch);

	/* set up network link layer */
	status = gether_setup(cdev->gadget, hostaddr);
	if (status < 0)
		goto diag_clean;

	/* set up serial link layer */
	status = gserial_setup(cdev->gadget, 2);
	if (status < 0)
		goto fail0;

	/* set up mass storage function */
	fsg_common = fsg_common_from_params(0, cdev, &mod_data);
	if (IS_ERR(fsg_common)) {
		status = PTR_ERR(fsg_common);
		goto fail1;
	}

	gcnum = usb_gadget_controller_number(gadget);
	if (gcnum >= 0)
		device_desc.bcdDevice = cpu_to_le16(0x0300 | gcnum);
	else {
		/* gadget zero is so simple (for now, no altsettings) that
		 * it SHOULD NOT have problems with bulk-capable hardware.
		 * so just warn about unrcognized controllers -- don't panic.
		 *
		 * things like configuration and altsetting numbering
		 * can need hardware-specific attention though.
		 */
		WARNING(cdev, "controller '%s' not recognized\n",
			gadget->name);
		device_desc.bcdDevice = __constant_cpu_to_le16(0x9999);
	}

	/* Allocate string descriptor numbers ... note that string
	 * contents can be overridden by the composite_dev glue.
	*/

	status = usb_string_id(cdev);
	if (status < 0)
		goto fail2;
	strings_dev[STRING_MANUFACTURER_IDX].id = status;
	device_desc.iManufacturer = status;

	status = usb_string_id(cdev);
	if (status < 0)
		goto fail2;
	strings_dev[STRING_PRODUCT_IDX].id = status;
	device_desc.iProduct = status;

	if (!usb_gadget_set_selfpowered(gadget))
		maemo_config_driver.bmAttributes |= USB_CONFIG_ATT_SELFPOWER;

	if (gadget->ops->wakeup)
		maemo_config_driver.bmAttributes |= USB_CONFIG_ATT_WAKEUP;

	/* register our first configuration */
	status = usb_add_config(cdev, &maemo_config_driver);
	if (status < 0)
		goto fail2;

	usb_gadget_set_selfpowered(gadget);
	dev_info(&gadget->dev, DRIVER_DESC "\n");
	fsg_common_put(fsg_common);
	return 0;

fail2:
	fsg_common_put(fsg_common);
fail1:
	gserial_cleanup();
fail0:
	gether_cleanup();
diag_clean:
	diag_cleanup(diag_ch);

	return status;
}

static int __exit maemo_unbind(struct usb_composite_dev *cdev)
{
	gserial_cleanup();
	gether_cleanup();
	diag_cleanup(diag_ch);
	return 0;
}

static struct usb_composite_driver qcom_maemo_driver = {
	.name		= "Qcom Maemo Gadget",
	.dev		= &device_desc,
	.strings	= dev_strings,
	.bind		= maemo_bind,
	.unbind		= __exit_p(maemo_unbind),
};

static int __init qcom_maemo_usb_init(void)
{
	return usb_composite_register(&qcom_maemo_driver);
}
module_init(qcom_maemo_usb_init);

static void __exit qcom_maemo_usb_cleanup(void)
{
	usb_composite_unregister(&qcom_maemo_driver);
}
module_exit(qcom_maemo_usb_cleanup);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
