/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
#ifndef _QCA402_H_
#define _QCA402_H_
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/msm_ion.h>
#include "../drivers/net/wireless/qca402x/htca_mbox/htca.h"

/* current htca buffer limitation */
#define HTCA_MAX_BUFF_SIZE (2048)
enum lpchtca_packet_type {
	/* the below three types are for piece of data (i.e. frame) with
	 *  known size
	 */
	LPCHTCA_START_PACKET = 1,
	LPCHTCA_MID_PACKET,
	LPCHTCA_END_PACKET,

	/* For piece of data (i.e. frame) with known size that fits in one
	 *  packet
	 */
	LPCHTCA_FULL_PACKET,

	/* If data size is unknown/continuous - this case assumes no header or
	 * user space to take care of headers
	 */
	LPCHTCA_UNSPEC_PACKET,

};

/* Message Flags */
#define LPCHTCA_ASYNC_MESSAGE        (1 << 7)
/* this bit is added to one of the above to indicate the packet is last that
 * will be received for this stream
 */
#define LPCHTCA_END_TRANSMISSON      (1 << 6)
#define LPCHTCA_PACKET_MASK          ((1 << 6) - 1)
#define QCA402_MIN_NUM_WORKBUFS (4)

struct msm_qca402_file_data_t;

struct __packed msm_qca402_htca_header_t {
	__u8 channel_id;
	__u8 msg_flags;
	__u16 meta_size;
};

#define QCA_MSG_HEADER_SIZE (sizeof(struct msm_qca402_htca_header_t))
#define QCA_MSG_PAYLOAD_SIZE (HTCA_MAX_BUFF_SIZE - QCA_MSG_HEADER_SIZE -\
				HTCA_HEADER_LEN_MAX)

struct __packed msm_qca402_htca_message_t {
	__u8 htca_private[HTCA_HEADER_LEN_MAX];
	struct msm_qca402_htca_header_t header;
	__u8 payload[QCA_MSG_PAYLOAD_SIZE];
};

struct file_data_list_t {
	struct msm_qca402_file_data_t *file_data;
	struct list_head list;
};

struct msm_qca402_workbuff_list_t {
	struct msm_qca402_htca_message_t htca_msg;
	__u32 size;
	struct list_head list;
};

struct msm_qca402_buffer_list_t {
	struct msm_qca_message_type qca_msg;
	struct msm_qca402_workbuff_list_t *wbb;
	void *vaddr;
	struct ion_handle *ih;
	__u32 valid_size;
	struct list_head list;
};

struct msm_qca402_evt_list_t {
	__u8 channel_id;
	__u8 cmd;
	struct list_head list;
};

struct msm_qca402_channel_list_t {
	struct msm_qca402_buffer_list_t *current_entry;
	struct msm_qca402_workbuff_list_t *wbin;
	struct msm_qca402_workbuff_list_t *wbout;
	struct list_head enqueued_list;
	struct list_head ready_list;
	struct list_head dequeued_list;
	__u32 ref_cnt;
	__u8 channel_id;
	__u8 has_buff;
	struct list_head list;
};

struct msm_qca402_ready_list_t {
	struct msm_qca402_channel_list_t *ch_data;
	__u8 cmd;
	struct list_head list;
};

struct msm_qca402_dev_data_t {
	struct cdev cdev;
	struct device *dev;
	struct ion_client *ion_client;
	__u8 endPointId;
	void *htca_target;
	struct list_head file_list;
	struct list_head channel_list;
	struct mutex dev_mutex;
	struct list_head work_list;
	wait_queue_head_t in_queue;
	struct list_head in_list;
	wait_queue_head_t recv_queue;
	struct list_head recv_list;
	spinlock_t lock;
	__u8 dev_idx;
	__u8 sending;
	__u8 receiving;
};

struct msm_qca402_file_data_t {
	struct msm_qca402_dev_data_t *dev_data;
	wait_queue_head_t out_queue;
	struct mutex file_mutex;
	struct list_head evt_list;
	struct list_head ready_ch_list;
};

#endif /* _QCA402_H_ */
