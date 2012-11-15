/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#ifndef DIAGFWD_SMUX_H
#define DIAGFWD_SMUX_H

#include <linux/smux.h>
#define LCID_VALID	SMUX_USB_DIAG_0
#define LCID_INVALID	0

int diagfwd_read_complete_smux(void);
int diagfwd_write_complete_smux(void);
int diagfwd_connect_smux(void);
void diag_usb_read_complete_smux_fn(struct work_struct *w);
void diag_read_usb_smux_work_fn(struct work_struct *work);
extern struct platform_driver msm_diagfwd_smux_driver;

#endif
