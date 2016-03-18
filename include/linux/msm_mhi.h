/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef MSM_MHI_H
#define MSM_MHI_H
#include <linux/types.h>

struct mhi_client_handle;

#define MHI_DMA_MASK       0xFFFFFFFFFFULL
#define MHI_MAX_MTU        0xFFFF

enum MHI_CLIENT_CHANNEL {
	MHI_CLIENT_LOOPBACK_OUT = 0,
	MHI_CLIENT_LOOPBACK_IN = 1,
	MHI_CLIENT_SAHARA_OUT = 2,
	MHI_CLIENT_SAHARA_IN = 3,
	MHI_CLIENT_DIAG_OUT = 4,
	MHI_CLIENT_DIAG_IN = 5,
	MHI_CLIENT_SSR_OUT = 6,
	MHI_CLIENT_SSR_IN = 7,
	MHI_CLIENT_QDSS_OUT = 8,
	MHI_CLIENT_QDSS_IN = 9,
	MHI_CLIENT_EFS_OUT = 10,
	MHI_CLIENT_EFS_IN = 11,
	MHI_CLIENT_MBIM_OUT = 12,
	MHI_CLIENT_MBIM_IN = 13,
	MHI_CLIENT_QMI_OUT = 14,
	MHI_CLIENT_QMI_IN = 15,
	MHI_CLIENT_IP_CTRL_0_OUT = 16,
	MHI_CLIENT_IP_CTRL_0_IN = 17,
	MHI_CLIENT_IP_CTRL_1_OUT = 18,
	MHI_CLIENT_IP_CTRL_1_IN = 19,
	MHI_CLIENT_DCI_OUT = 20,
	MHI_CLIENT_DCI_IN = 21,
	MHI_CLIENT_TF_OUT = 22,
	MHI_CLIENT_TF_IN = 23,
	MHI_CLIENT_BL_OUT = 24,
	MHI_CLIENT_BL_IN = 25,
	MHI_CLIENT_DUN_OUT = 32,
	MHI_CLIENT_DUN_IN = 33,
	MHI_CLIENT_IPC_ROUTER_OUT = 34,
	MHI_CLIENT_IPC_ROUTER_IN = 35,
	MHI_CLIENT_IP_SW_1_OUT = 36,
	MHI_CLIENT_IP_SW_1_IN = 37,
	MHI_CLIENT_IP_SW_2_OUT = 38,
	MHI_CLIENT_IP_SW_2_IN = 39,
	MHI_CLIENT_IP_SW_3_OUT = 40,
	MHI_CLIENT_IP_SW_3_IN = 41,
	MHI_CLIENT_CSVT_OUT = 42,
	MHI_CLIENT_CSVT_IN = 43,
	MHI_CLIENT_SMCT_OUT = 44,
	MHI_CLIENT_SMCT_IN = 45,
	MHI_CLIENT_RESERVED_1_LOWER = 46,
	MHI_CLIENT_RESERVED_1_UPPER = 99,
	MHI_CLIENT_IP_HW_0_OUT = 100,
	MHI_CLIENT_IP_HW_0_IN = 101,
	MHI_CLIENT_RESERVED_2_LOWER = 102,
	MHI_CLIENT_RESERVED_2_UPPER = 127,
	MHI_MAX_CHANNELS = 102
};

enum MHI_CB_REASON {
	MHI_CB_XFER = 0x0,
	MHI_CB_MHI_DISABLED = 0x4,
	MHI_CB_MHI_ENABLED = 0x8,
	MHI_CB_CHAN_RESET_COMPLETE = 0x10,
	MHI_CB_reserved = 0x80000000,
};

enum MHI_FLAGS {
	MHI_EOB = 0x100,
	MHI_EOT = 0x200,
	MHI_CHAIN = 0x1,
	MHI_FLAGS_reserved = 0x80000000,
};

struct mhi_result {
	void *user_data;
	void *buf_addr;
	size_t bytes_xferd;
	int transaction_status;
	enum MHI_FLAGS flags;
};

struct mhi_cb_info {
	struct mhi_result *result;
	enum MHI_CB_REASON cb_reason;
	u32 chan;
};

struct mhi_client_info_t {
	void (*mhi_client_cb)(struct mhi_cb_info *);
};

/**
 * mhi_deregister_channel - de-register callbacks from MHI
 *
 * @client_handle: Handle populated by MHI, opaque to client
 *
 * @Return errno
 */
int mhi_deregister_channel(struct mhi_client_handle *client_handle);

/**
 * mhi_register_channel - Client must call this function to obtain a handle for
 *			  any MHI operations
 *
 *  @client_handle:  Handle populated by MHI, opaque to client
 *  @chan:           Channel provided by client to which the handle
 *                   maps to.
 *  @device_index:   MHI device for which client wishes to register, if
 *                   there are multiple devices supporting MHI. Client
 *                   should specify 0 for the first device 1 for second etc.
 *  @info:           Client provided callbacks which MHI will invoke on events
 *  @user_data:      Client provided context to be returned to client upon
 *                   callback invocation.
 *  Not thread safe, caller must ensure concurrency protection.
 *
 * @Return errno
 */
int mhi_register_channel(struct mhi_client_handle **client_handle,
		enum MHI_CLIENT_CHANNEL chan, s32 device_index,
		struct mhi_client_info_t *client_info, void *user_data);

/**
 * mhi_open_channel - Client must call this function to open a channel
 *
 * @client_handle:  Handle populated by MHI, opaque to client
 *
 *  Not thread safe, caller must ensure concurrency protection.
 *
 * @Return errno
 */
int mhi_open_channel(struct mhi_client_handle *client_handle);

/**
 * mhi_queue_xfer - Client called function to add a buffer to MHI channel
 *
 *  @client_handle  Pointer to client handle previously obtained from
 *                  mhi_open_channel
 *  @buf            Pointer to client buffer
 *  @buf_len        Length of the client buffer
 *  @chain          Specify whether to set the chain bit on this buffer
 *  @eob            Specify whether this buffer should trigger EOB interrupt
 *
 *  NOTE:
 *  Not thread safe, caller must ensure concurrency protection.
 *  User buffer must be physically contiguous.
 *
 * @Return errno
 */
int mhi_queue_xfer(struct mhi_client_handle *client_handle,
		void *buf, size_t buf_len, enum MHI_FLAGS mhi_flags);

/**
 * mhi_close_channel - Client can request channel to be closed and handle freed
 *
 *  @client_handle  Pointer to client handle previously obtained from
 *                  mhi_open_channel
 *  Not thread safe, caller must ensure concurrency protection.
 *
 * @client_handle  Pointer to handle to be released
 */
void mhi_close_channel(struct mhi_client_handle *client_handle);

/**
 * mhi_get_free_desc - Get the number of free descriptors on channel.
 *  client_handle  Pointer to client handle previously obtained from
 *                      mhi_open_channel.
 *
 * This API returns a snapshot of available descriptors on the given
 * channel
 *
 * @Return  non negative on success
 */
int mhi_get_free_desc(struct mhi_client_handle *client_handle);

/*
 * mhi_poll_inbound - Poll a buffer from MHI channel
 * @client_handle  Pointer to client handle previously obtained from
 *                      mhi_open_channel.
 * @result         Result structure to be populated with buffer info
 *			if available;
 *
 * Client may asynchronously poll on an inbound channel for descriptors
 * which have been populated. This API is used by client to receive data
 * from device after a callback notification has been received.
 *
 *  Not thread safe, caller must ensure concurrency protection.
 *
 * @Return  non negative on success
 */
int mhi_poll_inbound(struct mhi_client_handle *client_handle,
			     struct mhi_result *result);

/**
 * mhi_get_max_desc - Get the maximum number of descriptors
 *			supported on the channel.
 * @client_handle  Pointer to client handle previously obtained from
 *                      mhi_open_channel.
 * @Return  non negative on success
 */
int mhi_get_max_desc(struct mhi_client_handle *client_handle);

/* RmNET Reserved APIs, This APIs are reserved for use by the linux network
* stack only. Use by other clients will introduce system wide issues
*/
int mhi_set_lpm(struct mhi_client_handle *client_handle, int enable_lpm);
int mhi_get_epid(struct mhi_client_handle *mhi_handle);
struct mhi_result *mhi_poll(struct mhi_client_handle *client_handle);
void mhi_mask_irq(struct mhi_client_handle *client_handle);
void mhi_unmask_irq(struct mhi_client_handle *client_handle);
#endif
