/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APU_CTRL_RPMSG_H__
#define __APU_CTRL_RPMSG_H__

enum {
	APU_CTRL_MSG_START = 0,
	PWR_SEND_CMD_COMPL_ID = 0,
	PWR_RECV_CMD_COMPL_ID = 1,
	DEBG_RECV_CMD_COMPL_ID = 2,
	MDW_SEND_CMD_ID = 3,
	MDW_RECV_CMD_ID = 4,
	MDLA_SEND_CMD_COMPL_ID = 5,
	REVISER_SEND_CMD_COMPL_ID = 6,
	TRACE_TIMESYNC_SYN = 7,
	MVPU_SEND_CMD_COMPL_ID = 8,
	DEEP_IDLE_ID = 9,

	/* unit test */
	APU_CTRL_MSG_TEST = 15,
	APU_CTRL_MSG_NUM = 16,
};

#define APU_CTRL_MSG_MAX_SIZE	(248)
#define APU_CTRL_MSG_HDR_SIZE	(8)


typedef void (*ctrl_msg_handler)(u32 id, void *priv,
				 void *data, u32 len);
int apu_ctrl_send_msg(u32 ch_id, void *data, unsigned int len,
		      unsigned int timeout);
int apu_ctrl_recv_msg(u32 ch_id);
int apu_ctrl_register_channel(u32 ch_id, ctrl_msg_handler handler, void *priv,
			      void *recv_buf, unsigned int recv_buf_size);
void apu_ctrl_unregister_channel(u32 ch_id);

#endif /* __APU_CTRL_RPMSG_H__ */

