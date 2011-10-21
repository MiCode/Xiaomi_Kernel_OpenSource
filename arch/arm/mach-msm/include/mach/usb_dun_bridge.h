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

#ifndef __USB_DUN_BRIDGE_H
#define __USB_DUN_BRIDGE_H

/**
 * struct dun_bridge_ops - context and callbacks for DUN bridge
 *
 * @ctxt: caller private context
 * @read_complete: called when read is completed. buf and len correspond
 *	to original passed-in values. actual length of buffer returned, or
 *	negative error value.
 * @write_complete: called when write is completed. buf and len correspond
 *	to original passed-in values. actual length of buffer returned, or
 *	negative error value.
 * @ctrl_status: asynchronous notification of control status. ctrl_bits
 *	is a bitfield of CDC ACM control status bits.
 */
struct dun_bridge_ops {
	void *ctxt;
	void (*read_complete)(void *ctxt, char *buf, size_t len, size_t actual);
	void (*write_complete)(void *ctxt, char *buf,
				size_t len, size_t actual);
	void (*ctrl_status)(void *ctxt, unsigned int ctrl_bits);
};

#ifdef CONFIG_USB_QCOM_DUN_BRIDGE

/**
 * dun_bridge_open - Open the DUN bridge
 *
 * @ops: pointer to ops struct containing private context and callback
 *	pointers
 */
int dun_bridge_open(struct dun_bridge_ops *ops);

/**
 * dun_bridge_close - Closes the DUN bridge
 */
int dun_bridge_close(void);

/**
 * dun_bridge_read - Request to read data from the DUN bridge. This call is
 *	asynchronous: user's read callback (ops->read_complete) will be called
 *	when data is returned.
 *
 * @data: pointer to caller-allocated buffer to fill in
 * @len: size of the buffer
 */
int dun_bridge_read(void *data, int len);

/**
 * dun_bridge_write - Request to write data to the DUN bridge. This call is
 *	asynchronous: user's write callback (ops->write_complete) will be called
 *	upon completion of the write indicating status and number of bytes
 *	written.
 *
 * @data: pointer to caller-allocated buffer to write
 * @len: length of the data in buffer
 */
int dun_bridge_write(void *data, int len);

/**
 * dun_bridge_send_ctrl_bits - Request to write line control data to the DUN
 *	bridge.  This call is asynchronous, however no callback will be issued
 *	upon completion.
 *
 * @ctrl_bits: CDC ACM line control bits
 */
int dun_bridge_send_ctrl_bits(unsigned ctrl_bits);

#else

#include <linux/errno.h>

static int __maybe_unused dun_bridge_open(struct dun_bridge_ops *ops)
{
	return -ENODEV;
}

static int __maybe_unused dun_bridge_close(void)
{
	return -ENODEV;
}

static int __maybe_unused dun_bridge_read(void *data, int len)
{
	return -ENODEV;
}

static int __maybe_unused dun_bridge_write(void *data, int len)
{
	return -ENODEV;
}

static int __maybe_unused dun_bridge_send_ctrl_bits(unsigned ctrl_bits)
{
	return -ENODEV;
}

#endif

#endif
