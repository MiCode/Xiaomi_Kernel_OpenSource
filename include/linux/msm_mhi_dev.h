/* Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_MHI_DEV_H
#define __MSM_MHI_DEV_H

#include <linux/types.h>
#include <linux/dma-mapping.h>

#define DMA_SYNC		1
#define DMA_ASYNC		0

enum cb_reason {
	MHI_DEV_TRE_AVAILABLE = 0,
	MHI_DEV_CTRL_UPDATE,
};

struct mhi_dev_client_cb_reason {
	uint32_t		ch_id;
	enum cb_reason		reason;
};

struct mhi_dev_client {
	struct list_head		list;
	struct mhi_dev_channel		*channel;
	void (*event_trigger)(struct mhi_dev_client_cb_reason *cb);

	/* mhi_dev calls are fully synchronous -- only one call may be
	 * active per client at a time for now.
	 */
	struct mutex			write_lock;
	wait_queue_head_t		wait;

	/* trace logs */
	spinlock_t			tr_lock;
	unsigned int			tr_head;
	unsigned int			tr_tail;
	struct mhi_dev_trace		*tr_log;

	/* client buffers */
	struct mhi_dev_iov		*iov;
	uint32_t			nr_iov;
};

enum mhi_ctrl_info {
	MHI_STATE_CONFIGURED = 0,
	MHI_STATE_CONNECTED = 1,
	MHI_STATE_DISCONNECTED = 2,
	MHI_STATE_INVAL,
};

struct mhi_req {
	u32                             chan;
	u32                             mode;
	u32				chain;
	void                            *buf;
	dma_addr_t                      dma;
	u32                             snd_cmpl;
	void                            *context;
	size_t                          len;
	size_t                          transfer_len;
	uint32_t                        rd_offset;
	struct mhi_dev_client           *client;
	struct list_head                list;
	union mhi_dev_ring_element_type *el;
	void (*client_cb)(void *req);
};

/* SW channel client list */
enum mhi_client_channel {
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
	MHI_CLIENT_IPCR_OUT = 20,
	MHI_CLIENT_IPCR_IN = 21,
	MHI_CLIENT_IP_CTRL_3_OUT = 22,
	MHI_CLIENT_IP_CTRL_3_IN = 23,
	MHI_CLIENT_IP_CTRL_4_OUT = 24,
	MHI_CLIENT_IP_CTRL_4_IN = 25,
	MHI_CLIENT_IP_CTRL_5_OUT = 26,
	MHI_CLIENT_IP_CTRL_5_IN = 27,
	MHI_CLIENT_IP_CTRL_6_OUT = 28,
	MHI_CLIENT_IP_CTRL_6_IN = 29,
	MHI_CLIENT_IP_CTRL_7_OUT = 30,
	MHI_CLIENT_IP_CTRL_7_IN = 31,
	MHI_CLIENT_DUN_OUT = 32,
	MHI_CLIENT_DUN_IN = 33,
	MHI_CLIENT_IP_SW_0_OUT = 34,
	MHI_CLIENT_IP_SW_0_IN = 35,
	MHI_CLIENT_ADB_OUT = 36,
	MHI_CLIENT_ADB_IN = 37,
	MHI_CLIENT_IP_SW_2_OUT = 38,
	MHI_CLIENT_IP_SW_2_IN = 39,
	MHI_CLIENT_IP_SW_3_OUT = 40,
	MHI_CLIENT_IP_SW_3_IN = 41,
	MHI_CLIENT_CSVT_OUT = 42,
	MHI_CLIENT_CSVT_IN = 43,
	MHI_CLIENT_SMCT_OUT = 44,
	MHI_CLIENT_SMCT_IN = 45,
	MHI_CLIENT_IP_SW_4_OUT  = 46,
	MHI_CLIENT_IP_SW_4_IN  = 47,
	MHI_MAX_SOFTWARE_CHANNELS,
	MHI_CLIENT_TEST_OUT = 60,
	MHI_CLIENT_TEST_IN = 61,
	MHI_CLIENT_RESERVED_1_LOWER = 62,
	MHI_CLIENT_RESERVED_1_UPPER = 99,
	MHI_CLIENT_IP_HW_0_OUT = 100,
	MHI_CLIENT_IP_HW_0_IN = 101,
	MHI_CLIENT_ADPL_IN = 102,
	MHI_CLIENT_RESERVED_2_LOWER = 102,
	MHI_CLIENT_RESERVED_2_UPPER = 127,
	MHI_MAX_CHANNELS = 102,
	MHI_CLIENT_INVALID = 0xFFFFFFFF
};

struct mhi_dev_client_cb_data {
	void			*user_data;
	enum mhi_client_channel	channel;
	enum mhi_ctrl_info	ctrl_info;
};

typedef void (*mhi_state_cb)(struct mhi_dev_client_cb_data *cb_dat);

struct mhi_dev_ready_cb_info {
	struct list_head		list;
	mhi_state_cb			cb;
	struct mhi_dev_client_cb_data	cb_data;
};

#if defined(CONFIG_MSM_MHI_DEV)
/**
 * mhi_dev_open_channel() - Channel open for a given client done prior
 *		to read/write.
 * @chan_id:	Software Channel ID for the assigned client.
 * @handle_client: Structure device for client handle.
 * @notifier: Client issued callback notification.
 */
int mhi_dev_open_channel(uint32_t chan_id,
		struct mhi_dev_client **handle_client,
		void (*event_trigger)(struct mhi_dev_client_cb_reason *cb));

/**
 * mhi_dev_close_channel() - Channel close for a given client.
 */
void mhi_dev_close_channel(struct mhi_dev_client *handle_client);

/**
 * mhi_dev_read_channel() - Channel read for a given client
 * @mreq:       mreq is the client argument which includes meta info
 *              like write data location, buffer len, read offset, mode,
 *              chain and client call back function which will be invoked
 *              when data read is completed.
 */
int mhi_dev_read_channel(struct mhi_req *mreq);

/**
 * mhi_dev_write_channel() - Channel write for a given software client.
 * @wreq	wreq is the client argument which includes meta info like
 *              client handle, read data location, buffer length, mode,
 *              and client call back function which will free the packet.
 *              when data write is completed.
 */
int mhi_dev_write_channel(struct mhi_req *wreq);

/**
 * mhi_dev_channel_isempty() - Checks if there is any pending TRE's to process.
 * @handle_client:	Client Handle issued during mhi_dev_open_channel
 */
int mhi_dev_channel_isempty(struct mhi_dev_client *handle);

/**
* mhi_dev_channel_has_pending_write() - Checks if there are any pending writes
*					to be completed on inbound channel
* @handle_client:	Client Handle issued during mhi_dev_open_channel
*/
bool mhi_dev_channel_has_pending_write(struct mhi_dev_client *handle);

/**
 * mhi_ctrl_state_info() - Provide MHI state info
 *		@idx: Channel number idx. Look at channel_state_info and
 *		pass the index for the corresponding channel.
 *		@info: Return the control info.
 *		MHI_STATE=CONFIGURED - MHI device is present but not ready
 *					for data traffic.
 *		MHI_STATE=CONNECTED - MHI device is ready for data transfer.
 *		MHI_STATE=DISCONNECTED - MHI device has its pipes suspended.
 *		exposes device nodes for the supported MHI software
 *		channels.
 */
int mhi_ctrl_state_info(uint32_t idx, uint32_t *info);

/**
 * mhi_register_state_cb() - Clients can register and receive callback after
 *		MHI channel is connected or disconnected.
 */
int mhi_register_state_cb(void (*mhi_state_cb)
			(struct mhi_dev_client_cb_data *cb_data), void *data,
			enum mhi_client_channel channel);

#else
static inline int mhi_dev_open_channel(uint32_t chan_id,
		struct mhi_dev_client **handle_client,
		void (*event_trigger)(struct mhi_dev_client_cb_reason *cb))
{
	return -EINVAL;
};

static inline int mhi_dev_close_channel(struct mhi_dev_client *handle_client)
{
	return -EINVAL;
};

static inline int mhi_dev_read_channel(struct mhi_req *mreq)
{
	return -EINVAL;
};

static inline int mhi_dev_write_channel(struct mhi_req *wreq)
{
	return -EINVAL;
};

static inline int mhi_dev_channel_isempty(struct mhi_dev_client *handle)
{
	return -EINVAL;
};

static inline bool mhi_dev_channel_has_pending_write
	(struct mhi_dev_client *handle)
{
	return false;
}

static inline int mhi_ctrl_state_info(uint32_t idx, uint32_t *info)
{
	return -EINVAL;
};

static inline int mhi_register_state_cb(void (*mhi_state_cb)
			(struct mhi_dev_client_cb_data *cb_data), void *data,
			enum mhi_client_channel channel)
{
	return -EINVAL;
};
#endif

#endif /* _MSM_MHI_DEV_H*/
