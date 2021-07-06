/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef MTK_GADGET_H
#define MTK_GADGET_H

#ifdef CONFIG_USB_CONFIGFS_UEVENT
extern char *serial_string;
extern int serial_idx;
#endif
extern void composite_setup_complete(struct usb_ep *ep,
		struct usb_request *req);
extern int acm_shortcut(void);

#endif
