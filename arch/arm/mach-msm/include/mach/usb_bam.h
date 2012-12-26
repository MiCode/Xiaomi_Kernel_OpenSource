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
#include <mach/ipa.h>

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

struct usb_bam_connect_ipa_params {
	u8 idx;
	u32 *src_pipe;
	u32 *dst_pipe;
	enum usb_bam_pipe_dir dir;
	/* client handle assigned by IPA to client */
	u32 prod_clnt_hdl;
	u32 cons_clnt_hdl;
	/* params assigned by the CD */
	enum ipa_client_type client;
	struct ipa_ep_cfg ipa_ep_cfg;
	void *priv;
	void (*notify)(void *priv, enum ipa_dp_evt_type evt,
			unsigned long data);
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
 * Connect USB-to-IPA SPS connection.
 *
 * This function returns the allocated pipes number adn clnt handles.
 *
 * @ipa_params - in/out parameters
 *
 * @return 0 on success, negative value on error
 *
 */
int usb_bam_connect_ipa(struct usb_bam_connect_ipa_params *ipa_params);

/**
 * Disconnect USB-to-IPA SPS connection.
 *
 * @idx - Connection index.
 *
 * @ipa_params - in/out parameters
 *
 * @return 0 on success, negative value on error
 *
 */
int usb_bam_disconnect_ipa(u8 idx,
		struct usb_bam_connect_ipa_params *ipa_params);

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
 * Register a callback for peer BAM reset.
 *
 * @idx - Connection index.
 *
 * @callback - the callback function that will be called in USB
 *				driver upon a peer bam reset
 *
 * @param - context that the caller can supply
 *
 * @return 0 on success, negative value on error
 *
 */
int usb_bam_register_peer_reset_cb(u8 idx,
	 int (*callback)(void *), void *param);

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

/**
 * Resets the entire USB BAM.
 *
 */
int usb_bam_reset(void);

/**
 * Indicates if the client of the USB BAM is ready to start
 * sending/receiving transfers.
 *
 * @ready - TRUE to enable, FALSE to disable.
 *
 */
int usb_bam_client_ready(bool ready);

#else
static inline int usb_bam_connect(u8 idx, u32 *src_pipe_idx, u32 *dst_pipe_idx)
{
	return -ENODEV;
}

static inline int usb_bam_connect_ipa(
			struct usb_bam_connect_ipa_params *ipa_params)
{
	return -ENODEV;
}

static inline int usb_bam_disconnect_ipa(u8 idx,
			struct usb_bam_connect_ipa_params *ipa_params)
{
	return -ENODEV;
}

static inline int usb_bam_register_wake_cb(u8 idx,
	int (*callback)(void *), void* param)
{
	return -ENODEV;
}

static inline int usb_bam_register_peer_reset_cb(u8 idx,
	 int (*callback)(void *), void *param)
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

static inline int usb_bam_reset(void)
{
	return -ENODEV;
}

static inline int usb_bam_client_ready(bool ready)
{
	return -ENODEV;
}

#endif
#endif				/* _USB_BAM_H_ */
