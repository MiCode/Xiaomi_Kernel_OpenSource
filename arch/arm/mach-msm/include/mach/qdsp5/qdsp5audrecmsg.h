#ifndef QDSP5AUDRECMSGI_H
#define QDSP5AUDRECMSGI_H

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*
 *
 *    A U D I O   R E C O R D  M E S S A G E S
 *
 * GENERAL DESCRIPTION
 *  This file contains defintions of format blocks of messages
 *  that are sent by AUDREC Task
 *
 * REFERENCES
 *   None
 *
 * EXTERNALIZED FUNCTIONS
 *  None
 *
 * Copyright (c) 1992-2009, 2011 The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/

/*===========================================================================

                      EDIT HISTORY FOR FILE

This section contains comments describing changes made to this file.
Notice that changes are listed in reverse chronological order.
   
 $Header: //source/qcom/qct/multimedia2/Audio/drivers/QDSP5Driver/QDSP5Interface/main/latest/qdsp5audrecmsg.h#3 $

============================================================================*/

/*
 * AUDRECTASK MESSAGES
 * AUDRECTASK uses audRecUpRlist to communicate with ARM
 * Location : MEMC
 * Buffer size : 4
 * No of buffers in a queue : 2
 */

/*
 * Message to notify that config command is done
 */

#define AUDREC_MSG_CMD_CFG_DONE_MSG	0x0002
#define AUDREC_MSG_CMD_CFG_DONE_MSG_LEN	\
	sizeof(struct audrec_msg_cmd_cfg_done_msg)


#define AUDREC_MSG_CFG_DONE_TYPE_0_ENA		0x4000
#define AUDREC_MSG_CFG_DONE_TYPE_0_DIS		0x0000

#define AUDREC_MSG_CFG_DONE_TYPE_0_NO_UPDATE	0x0000
#define AUDREC_MSG_CFG_DONE_TYPE_0_UPDATE	0x8000

#define AUDREC_MSG_CFG_DONE_TYPE_1_ENA		0x4000
#define AUDREC_MSG_CFG_DONE_TYPE_1_DIS		0x0000

#define AUDREC_MSG_CFG_DONE_TYPE_1_NO_UPDATE	0x0000
#define AUDREC_MSG_CFG_DONE_TYPE_1_UPDATE	0x8000

#define AUDREC_MSG_CFG_DONE_ENC_ENA		0x8000
#define AUDREC_MSG_CFG_DONE_ENC_DIS		0x0000

struct audrec_msg_cmd_cfg_done_msg {
	unsigned short	audrec_enc_type;
	unsigned short	audrec_obj_idx;
} __attribute__((packed));

/*
 * Message to notify arec0/1 or concurrent encoder cfg done
 * and recording params recieved by task
 */

#define	AUDREC_MSG_CMD_AREC_PARAM_CFG_DONE_MSG		0x0003
#define	AUDREC_MSG_CMD_AREC_PARAM_CFG_DONE_MSG_LEN	\
	sizeof(struct audrec_msg_cmd_arec_param_cfg_done_msg)


#define	AUDREC_MSG_AREC_PARAM_TYPE_0	0x0000
#define	AUDREC_MSG_AREC_PARAM_TYPE_1	0x0001

struct audrec_msg_cmd_arec_param_cfg_done_msg {
	unsigned short	audrec_obj_idx;
} __attribute__((packed));

/*
 * Message to notify no more buffers are available in ext mem to DME
 * Or no concurrent encoder supported
 */
/* for 7x27 */
#define AUDREC_MSG_FATAL_ERR_MSG		0x0004
#define AUDREC_MSG_FATAL_ERR_MSG_LEN	\
	sizeof(struct audrec_msg_fatal_err_msg)


#define AUDREC_MSG_FATAL_ERR_TYPE_0	0x0000
#define AUDREC_MSG_FATAL_ERR_TYPE_1	0x0001

struct audrec_msg_fatal_err_msg {
	unsigned short	audrec_obj_idx;
	unsigned short	audrec_err_id;
} __attribute__((packed));

/* for 7x27A */
#define AUDREC_MSG_NO_EXT_PKT_AVAILABLE_MSG		0x0004
#define AUDREC_MSG_NO_EXT_PKT_AVAILABLE_MSG_LEN	\
	sizeof(struct audrec_msg_no_ext_pkt_avail_msg)

#define AUDREC_MSG_NO_EXT_PKT_AVAILABLE_TYPE_0	0x0000
#define AUDREC_MSG_NO_EXT_PKT_AVAILABLE_TYPE_1	0x0001

struct audrec_msg_no_ext_pkt_avail_msg {
	unsigned short	audrec_obj_idx;
	unsigned short	audrec_err_id;
} __packed;

/*
 * Message to notify DME deliverd the encoded pkt to ext pkt buffer
 */

#define AUDREC_MSG_PACKET_READY_MSG		0x0005
#define AUDREC_MSG_PACKET_READY_MSG_LEN	\
	sizeof(struct audrec_msg_packet_ready_msg)


#define AUDREC_MSG_PACKET_READY_TYPE_0	0x0000
#define AUDREC_MSG_PACKET_READY_TYPE_1	0x0001

struct audrec_msg_packet_ready_msg {
	unsigned short	audrec_obj_idx;
	unsigned short	pkt_counter_msw;
	unsigned short	pkt_counter_lsw;
	unsigned short	pkt_read_cnt_msw;
	unsigned short	pkt_read_cnt_lsw;
} __attribute__((packed));

/*
 * Message to notify external memory cfg done and recieved by task
 */

#define	AUDREC_MSG_CMD_AREC_MEM_CFG_DONE_MSG		0x0006
#define	AUDREC_MSG_CMD_AREC_MEM_CFG_DONE_MSG_LEN	\
	sizeof(struct audrec_msg_cmd_arec_mem_cfg_done_msg)


struct audrec_msg_cmd_arec_mem_cfg_done_msg {
	unsigned short	audrec_obj_idx;
} __attribute__((packed));

/*
 * Message to indicate Routing mode
 * configuration success or failure
 */

#define AUDREC_MSG_CMD_ROUTING_MODE_DONE_MSG		 0x0007
#define AUDREC_MSG_CMD_ROUTING_MODE_DONE_MSG_LEN	 \
	sizeof(struct audrec_msg_cmd_routing_mode_done_msg)

struct audrec_msg_cmd_routing_mode_done_msg {
	unsigned short configuration;
} __packed;

/*
 * Message to indicate pcm buffer configured
 */

#define AUDREC_CMD_PCM_CFG_ARM_TO_ENC_DONE_MSG		0x0008
#define AUDREC_CMD_PCM_CFG_ARM_TO_ENC_DONE_MSG_LEN	\
	sizeof(struct audrec_cmd_pcm_cfg_arm_to_enc_msg)

struct  audrec_cmd_pcm_cfg_arm_to_enc_msg {
	unsigned short configuration;
} __packed;

/*
 * Message to indicate encoded packet is delivered to external buffer in FTRT
 */

#define AUDREC_UP_NT_PACKET_READY_MSG			0x0009
#define AUDREC_UP_NT_PACKET_READY_MSG_LEN	\
	sizeof(struct audrec_up_nt_packet_ready_msg)

struct  audrec_up_nt_packet_ready_msg {
	unsigned short audrec_packetwrite_cnt_lsw;
	unsigned short audrec_packetwrite_cnt_msw;
	unsigned short audrec_upprev_readcount_lsw;
	unsigned short audrec_upprev_readcount_msw;
} __packed;

/*
 * Message to indicate pcm buffer is consumed
 */

#define AUDREC_CMD_PCM_BUFFER_PTR_UPDATE_ARM_TO_ENC_MSG 0x000A
#define AUDREC_CMD_PCM_BUFFER_PTR_UPDATE_ARM_TO_ENC_MSG_LEN	\
	sizeof(struct audrec_cmd_pcm_buffer_ptr_update_arm_to_enc_msg)

struct  audrec_cmd_pcm_buffer_ptr_update_arm_to_enc_msg {
	unsigned short buffer_readcnt_msw;
	unsigned short buffer_readcnt_lsw;
	unsigned short number_of_buffers;
	unsigned short buffer_address_length[];
} __packed;

/*
 * Message to indicate flush acknowledgement
 */

#define AUDREC_CMD_FLUSH_DONE_MSG			0x000B

#define ADSP_MESSAGE_ID 0xFFFF

#endif
