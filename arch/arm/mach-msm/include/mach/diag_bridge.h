
/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
			size_t buf_size, size_t actual);
	void (*write_complete_cb)(void *ctxt, char *buf,
			size_t buf_size, size_t actual);
};

extern int diag_read(char *data, size_t size);
extern int diag_write(char *data, size_t size);
extern int diag_open(struct diag_bridge_ops *ops);
extern void diag_close(void);

#endif
