/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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

#ifndef _USB_BAM_H_
#define _USB_BAM_H_
#include "sps.h"

/**
 * SPS Pipes direction.
 *
 * USB_TO_PEER_PERIPHERAL	USB (as Producer) to other
 *                          peer peripheral.
 * PEER_PERIPHERAL_TO_USB	Other Peripheral to
 *                          USB (as consumer).
 */
enum usb_bam_pipe_dir {
	USB_TO_PEER_PERIPHERAL,
	PEER_PERIPHERAL_TO_USB,
};

#ifdef CONFIG_USB_BAM
/**
 * Connect USB-to-Periperal SPS connection.
 *
 * This function returns the allocated pipes number.
 *
 * @idx - Connection index.
 *
 * @src_pipe_idx - allocated pipe index - USB as a
 *  source (output)
 *
 * @dst_pipe_idx - allocated pipe index - USB as a
 * destination (output)
 *
 * @return 0 on success, negative value on error
 *
 */
int usb_bam_connect(u8 idx, u32 *src_pipe_idx, u32 *dst_pipe_idx);

/**
 * Register a wakeup callback from peer BAM.
 *
 * @idx - Connection index.
 *
 * @callback - the callback function
 *
 * @return 0 on success, negative value on error
 *
 */
int usb_bam_register_wake_cb(u8 idx,
	 int (*callback)(void *), void* param);

/**
 * Disconnect USB-to-Periperal SPS connection.
 *
 * @idx - Connection index.
 *
 * @return 0 on success, negative value on error
 */
int usb_bam_disconnect_pipe(u8 idx);

/**
 * Returns usb bam connection parameters.
 *
 * @conn_idx - Connection index.
 *
 * @usb_bam_pipe_dir - Usb pipe direction to/from peripheral.
 *
 * @usb_bam_handle - Usb bam handle.
 *
 * @usb_bam_pipe_idx - Usb bam pipe index.
 *
 * @peer_pipe_idx - Peer pipe index.
 *
 * @desc_fifo - Descriptor fifo parameters.
 *
 * @data_fifo - Data fifo parameters.
 *
 */
void get_bam2bam_connection_info(u8 conn_idx, enum usb_bam_pipe_dir pipe_dir,
	u32 *usb_bam_handle, u32 *usb_bam_pipe_idx, u32 *peer_pipe_idx,
	struct sps_mem_buffer *desc_fifo, struct sps_mem_buffer *data_fifo);

#else
static inline int usb_bam_connect(u8 idx, u32 *src_pipe_idx, u32 *dst_pipe_idx)
{
	return -ENODEV;
}

static inline int usb_bam_register_wake_cb(u8 idx,
	int (*callback)(void *), void* param)
{
	return -ENODEV;
}

static inline int usb_bam_disconnect_pipe(u8 idx)
{
	return -ENODEV;
}

static inline void get_bam2bam_connection_info(u8 conn_idx,
	enum usb_bam_pipe_dir pipe_dir, u32 *usb_bam_handle,
	u32 *usb_bam_pipe_idx, u32 *peer_pipe_idx,
	struct sps_mem_buffer *desc_fifo, struct sps_mem_buffer *data_fifo)
{
	return;
}
#endif
#endif				/* _USB_BAM_H_ */
