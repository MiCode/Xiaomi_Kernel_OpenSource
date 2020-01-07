/* Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
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

#ifndef _LINUX_VIRTIO_SPMI_H
#define _LINUX_VIRTIO_SPMI_H

#include <linux/types.h>
#include <linux/virtio_ids.h>
#include <linux/virtio_config.h>
#include <linux/virtio_types.h>

/* Feature bits */
#define VIRTIO_SPMI_F_INT	1	/* Support interrupt */

#define VM_MAX_PERIPHS 512

/* Configuration layout */
struct virtio_spmi_config {
	__u16 ppid_allowed[VM_MAX_PERIPHS];
} __packed;

struct payload_cmd {
	__virtio32 cmd;
	__virtio32 data[2];
};

struct payload_irq {
	__virtio16 ppid;
	__virtio32 regval;
};

union payload_data {
	struct payload_cmd cmdd;
	struct payload_irq irqd;
};

struct virtio_spmi_msg {
	__virtio32 type;
	__virtio32 res;
	union payload_data payload;
};

/* Virtio SPMI message type */
enum vio_spmi_msg_type {
	VIO_SPMI_BUS_WRITE = 0,
	VIO_SPMI_BUS_READ = 1,
	VIO_SPMI_BUS_CMDMAX  = 1,
	VIO_ACC_ENABLE_WR = 2,
	VIO_ACC_ENABLE_RD = 3,
	VIO_IRQ_CLEAR = 4,
	VIO_IRQ_STATUS = 5,
};

/* Virtio SPMI message type */
enum vio_spmi_msg_res {
	VIO_SPMI_DONE = 0,
	VIO_SPMI_ERR = 1,
};
#endif /* _LINUX_VIRTIO_SPMI_H */
