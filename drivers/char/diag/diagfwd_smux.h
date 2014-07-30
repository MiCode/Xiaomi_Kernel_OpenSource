/* Copyright (c) 2012,2014 The Linux Foundation. All rights reserved.
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

struct diag_smux_info {
	int lcid;
	unsigned char *read_buf;
	int read_len;
	int in_busy;
	int enabled;
	int connected;
};

extern struct diag_smux_info *diag_smux;

int diagfwd_write_complete_smux(void);
int diagfwd_connect_smux(void);
extern struct platform_driver msm_diagfwd_smux_driver;

#endif
