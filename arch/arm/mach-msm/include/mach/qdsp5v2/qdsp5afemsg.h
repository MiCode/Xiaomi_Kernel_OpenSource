/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __MACH_QDSP5_V2_QDSP5AFEMSG_H
#define __MACH_QDSP5_V2_QDSP5AFEMSG_H

#define AFE_APU_MSG_CODEC_CONFIG_ACK		0x0001
#define AFE_APU_MSG_CODEC_CONFIG_ACK_LEN	\
	sizeof(struct afe_msg_codec_config_ack)

#define AFE_APU_MSG_VOC_TIMING_SUCCESS		0x0002

#define AFE_MSG_CODEC_CONFIG_ENABLED 0x1
#define AFE_MSG_CODEC_CONFIG_DISABLED 0xFFFF

struct afe_msg_codec_config_ack {
	uint16_t device_id;
	uint16_t device_activity;
	uint16_t reserved;
} __attribute__((packed));

#endif /* QDSP5AFEMSG_H */
