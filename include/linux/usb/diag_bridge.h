/* Copyright (c) 2011, 2013, 2018, The Linux Foundation. All rights reserved.
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

#ifndef __LINUX_USB_DIAG_BRIDGE_H__
#define __LINUX_USB_DIAG_BRIDGE_H__

struct diag_bridge_ops {
	void *ctxt;
	void (*read_complete_cb)(void *ctxt, char *buf,
			int buf_size, int actual);
	void (*write_complete_cb)(void *ctxt, char *buf,
			int buf_size, int actual);
	int (*suspend)(void *ctxt);
	void (*resume)(void *ctxt);
};

#if IS_ENABLED(CONFIG_USB_QCOM_DIAG_BRIDGE)

extern int diag_bridge_read(int id, char *data, int size);
extern int diag_bridge_write(int id, char *data, int size);
extern int diag_bridge_open(int id, struct diag_bridge_ops *ops);
extern void diag_bridge_close(int id);

#else

static int __maybe_unused diag_bridge_read(int id, char *data, int size)
{
	return -ENODEV;
}

static int __maybe_unused diag_bridge_write(int id, char *data, int size)
{
	return -ENODEV;
}

static int __maybe_unused diag_bridge_open(int id, struct diag_bridge_ops *ops)
{
	return -ENODEV;
}

static void __maybe_unused diag_bridge_close(int id) { }

#endif

#endif
