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

#ifndef QDSP5RMTCMDI_H
#define QDSP5RMTCMDI_H

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

    R M T A S K I N T E R N A L  C O M M A N D S

GENERAL DESCRIPTION
  This file contains defintions of format blocks of commands
  that are accepted by RM Task

REFERENCES
  None

EXTERNALIZED FUNCTIONS
  None

*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/

/*
 * ARM to RMTASK Commands
 *
 * ARM uses one command queue to communicate with AUDPPTASK
 * 1) apuRmtQueue: Used to send commands to RMTASK from APPS processor
 * Location : MEMA
 * Buffer Size : 3 words
 */

#define RM_CMD_AUD_CODEC_CFG	0x0

#define RM_AUD_CLIENT_ID	0x0
#define RMT_ENABLE		0x1
#define RMT_DISABLE		0x0

struct aud_codec_config_cmd {
	unsigned short			cmd_id;
	unsigned char			task_id;
	unsigned char			client_id;
	unsigned short			enable;
	unsigned short			dec_type;
} __attribute__((packed));

#endif /* QDSP5RMTCMDI_H */
