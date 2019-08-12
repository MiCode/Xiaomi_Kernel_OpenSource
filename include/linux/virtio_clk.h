/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#ifndef _LINUX_VIRTIO_CLK_H
#define _LINUX_VIRTIO_CLK_H

#include <linux/types.h>
#include <linux/virtio_ids.h>
#include <linux/virtio_config.h>
#include <linux/virtio_types.h>

/* Feature bits */
#define VIRTIO_CLK_F_RESET	1	/* Support reset */
#define VIRTIO_CLK_F_NAME	2	/* Support clock name */

/* Configuration layout */
struct virtio_clk_config {
	__u32 num_clks;
	__u32 num_resets;
	__u8 name[20];
} __packed;

/* Request/response message format */
struct virtio_clk_msg {
	u8 name[40];
	__virtio32 id;
	__virtio32 type;
	__virtio32 result;
	__virtio32 data[4];
};

/* Request type */
#define VIRTIO_CLK_T_ENABLE	0
#define VIRTIO_CLK_T_DISABLE	1
#define VIRTIO_CLK_T_SET_RATE	2
#define VIRTIO_CLK_T_GET_RATE	3
#define VIRTIO_CLK_T_ROUND_RATE	4
#define VIRTIO_CLK_T_RESET	5
#define VIRTIO_CLK_T_SET_FLAGS	6

#endif /* _LINUX_VIRTIO_CLK_H */
