/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#ifndef _CNSS_USB_H
#define _CNSS_USB_H

#include <linux/usb.h>

#include "main.h"

struct cnss_usb_data {
	struct usb_interface *usb_intf;
	struct cnss_plat_data *plat_priv;
	const struct usb_device_id *usb_device_id;
	u16 device_id; /*USB PID*/
	u16 target_version; /* [QCN7605] - from bcdDevice*/
	struct cnss_usb_wlan_driver *driver_ops;
};

static inline void cnss_set_usb_priv(struct usb_interface *usb_intf, void *data)
{
	usb_set_intfdata(usb_intf, data);
}

static inline struct cnss_usb_data *cnss_get_usb_priv(struct usb_interface
						      *usb_intf)
{
	return usb_get_intfdata(usb_intf);
}

static inline struct cnss_plat_data *cnss_usb_priv_to_plat_priv(void *bus_priv)
{
	struct cnss_usb_data *usb_priv = bus_priv;

	return usb_priv->plat_priv;
}

int cnss_usb_init(struct cnss_plat_data *plat_priv);
void cnss_usb_deinit(struct cnss_plat_data *plat_priv);
void cnss_usb_collect_dump_info(struct cnss_usb_data *usb_priv, bool in_panic);
void cnss_usb_clear_dump_info(struct cnss_usb_data *usb_priv);
int cnss_usb_force_fw_assert_hdlr(struct cnss_usb_data *usb_priv);
void cnss_usb_fw_boot_timeout_hdlr(struct cnss_usb_data *usb_priv);
int cnss_usb_call_driver_probe(struct cnss_usb_data *usb_priv);
int cnss_usb_call_driver_remove(struct cnss_usb_data *usb_priv);
int cnss_usb_dev_powerup(struct cnss_usb_data *usb_priv);
int cnss_usb_dev_shutdown(struct cnss_usb_data *usb_priv);
int cnss_usb_dev_crash_shutdown(struct cnss_usb_data *usb_priv);
int cnss_usb_dev_ramdump(struct cnss_usb_data *usb_priv);

int cnss_usb_register_driver_hdlr(struct cnss_usb_data *usb_priv, void *data);

int cnss_usb_unregister_driver_hdlr(struct cnss_usb_data *usb_priv);
int cnss_usb_call_driver_modem_status(struct cnss_usb_data *usb_priv,
				      int modem_current_status);

#endif /* _CNSS_USB_H */
