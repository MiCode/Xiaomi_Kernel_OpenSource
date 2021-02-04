/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

extern void mt_usbhost_connect(void);
extern void mt_usbhost_disconnect(void);
extern void mt_vbus_on(void);
extern void mt_vbus_off(void);
extern int usb_otg_set_vbus(int is_on);
extern void mt_usb_connect(void);
extern void mt_usb_disconnect(void);
