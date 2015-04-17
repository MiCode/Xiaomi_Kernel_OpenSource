/*
 * Copyright (c) 2008-2010, 2012-2014, The Linux Foundation.
 * All rights reserved.
 *
 * All source code in this file is licensed under the following license except
 * where indicated.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org
 */

#ifndef _DRIVERS_USB_DIAG_H_
#define _DRIVERS_USB_DIAG_H_

#include <linux/err.h>

#define DIAG_LEGACY		"diag"
#define DIAG_MDM		"diag_mdm"
#define DIAG_QSC		"diag_qsc"
#define DIAG_MDM2		"diag_mdm2"

#define USB_DIAG_CONNECT	0
#define USB_DIAG_DISCONNECT	1
#define USB_DIAG_WRITE_DONE	2
#define USB_DIAG_READ_DONE	3

struct diag_request {
	char *buf;
	int length;
	int actual;
	int status;
	void *context;
};

struct usb_diag_ch {
	const char *name;
	struct list_head list;
	void (*notify)(void *priv, unsigned event, struct diag_request *d_req);
	void *priv;
	void *priv_usb;
};

#ifdef CONFIG_USB_G_ANDROID
struct usb_diag_ch *usb_diag_open(const char *name, void *priv,
		void (*notify)(void *, unsigned, struct diag_request *));
void usb_diag_close(struct usb_diag_ch *ch);
int usb_diag_alloc_req(struct usb_diag_ch *ch, int n_write, int n_read);
int usb_diag_read(struct usb_diag_ch *ch, struct diag_request *d_req);
int usb_diag_write(struct usb_diag_ch *ch, struct diag_request *d_req);
#else
static inline struct usb_diag_ch *usb_diag_open(const char *name, void *priv,
		void (*notify)(void *, unsigned, struct diag_request *))
{
	return ERR_PTR(-ENODEV);
}
static inline void usb_diag_close(struct usb_diag_ch *ch)
{
}
static inline
int usb_diag_alloc_req(struct usb_diag_ch *ch, int n_write, int n_read)
{
	return -ENODEV;
}
static inline
int usb_diag_read(struct usb_diag_ch *ch, struct diag_request *d_req)
{
	return -ENODEV;
}
static inline
int usb_diag_write(struct usb_diag_ch *ch, struct diag_request *d_req)
{
	return -ENODEV;
}
#endif /* CONFIG_USB_G_ANDROID */
#endif /* _DRIVERS_USB_DIAG_H_ */
