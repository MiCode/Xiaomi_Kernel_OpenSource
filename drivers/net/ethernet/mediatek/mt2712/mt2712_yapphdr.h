/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MT2712_YAPPHDR_H__
#define __MT2712_YAPPHDR_H__

#define MAX_TX_QUEUE_CNT 8
#define MAX_RX_QUEUE_CNT 8

/* Private IOCTL for handling device specific task */
#define PRV_IOCTL	SIOCDEVPRIVATE

#define MAC_READ_CMD 1
#define MAC_WRITE_CMD 2
#define PHY_READ_CMD 3
#define PHY_WRITE_CMD 4
#define SEND_FRAME_CMD 5

/* List of command errors driver can set */
#define	CONFIG_FAIL	-1
#define	CONFIG_SUCCESS	0

/* common data structure between driver and application for
 * sharing info through ioctl
 */
struct ifr_data_struct {
	unsigned int q_inx;
	unsigned int num; /* dma channel no to be configured */
	unsigned int cmd;
};

#endif
