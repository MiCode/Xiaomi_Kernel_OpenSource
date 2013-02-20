/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
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

#ifndef QDSP5RMTMSG_H
#define QDSP5RMTMSG_H

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

       R M T A S K   M S G

GENERAL DESCRIPTION
  Messages sent by RMTASK to APPS PROCESSOR

REFERENCES
  None

EXTERNALIZED FUNCTIONS
  None
*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/

/*
 * RMTASK uses RmtApuRlist to send messages to the APPS PROCESSOR
 * Location : MEMA
 * Buffer Size : 3
 */

#define RMT_CODEC_CONFIG_ACK	0x1

struct aud_codec_config_ack {
	unsigned char			task_id;
	unsigned char			client_id;
	unsigned char			reason;
	unsigned char			enable;
	unsigned short			dec_type;
} __attribute__((packed));

#define RMT_DSP_OUT_OF_MIPS	0x2

struct rmt_dsp_out_of_mips {
	unsigned short			dec_info;
	unsigned short			rvd_0;
	unsigned short			rvd_1;
} __attribute__((packed));

#endif /* QDSP5RMTMSG_H */

