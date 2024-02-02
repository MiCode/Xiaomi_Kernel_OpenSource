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

#ifndef _LINUX_VIRTIO_REGULATOR_H
#define _LINUX_VIRTIO_REGULATOR_H

#include <linux/types.h>
#include <linux/virtio_ids.h>
#include <linux/virtio_config.h>
#include <linux/virtio_types.h>

/* Request/response message format */
struct virtio_regulator_msg {
	u8 name[20];
	__virtio32 type;
	__virtio32 result;
	__virtio32 data[4];
};

/* Request type */
#define VIRTIO_REGULATOR_T_ENABLE	0
#define VIRTIO_REGULATOR_T_DISABLE	1
#define VIRTIO_REGULATOR_T_SET_VOLTAGE	2
#define VIRTIO_REGULATOR_T_GET_VOLTAGE	3
#define VIRTIO_REGULATOR_T_SET_CURRENT_LIMIT	4
#define VIRTIO_REGULATOR_T_GET_CURRENT_LIMIT	5
#define VIRTIO_REGULATOR_T_SET_MODE	6
#define VIRTIO_REGULATOR_T_GET_MODE	7
#define VIRTIO_REGULATOR_T_SET_LOAD	8

#endif /* _LINUX_VIRTIO_REGULATOR_H */
