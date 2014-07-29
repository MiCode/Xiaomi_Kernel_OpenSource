/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#ifndef __LINUX_USB_CTRL_QTI_H
#define __LINUX_USB_CTRL_QTI_H

#include <linux/ioctl.h>

#define MAX_QTI_PKT_SIZE 2048

#define QTI_CTRL_IOCTL_MAGIC	'r'
#define QTI_CTRL_GET_LINE_STATE	_IOR(QTI_CTRL_IOCTL_MAGIC, 2, int)
#define QTI_CTRL_EP_LOOKUP _IOR(QTI_CTRL_IOCTL_MAGIC, 3, struct ep_info)
#define QTI_CTRL_MODEM_OFFLINE _IO(QTI_CTRL_IOCTL_MAGIC, 4)
#define QTI_CTRL_MODEM_ONLINE _IO(QTI_CTRL_IOCTL_MAGIC, 5)

enum peripheral_ep_type {
	DATA_EP_TYPE_RESERVED	= 0x0,
	DATA_EP_TYPE_HSIC	= 0x1,
	DATA_EP_TYPE_HSUSB	= 0x2,
	DATA_EP_TYPE_PCIE	= 0x3,
	DATA_EP_TYPE_EMBEDDED	= 0x4,
	DATA_EP_TYPE_BAM_DMUX	= 0x5,
};

struct peripheral_ep_info {
	enum peripheral_ep_type		ep_type;
	u32				peripheral_iface_id;
};


struct ipa_ep_pair {
	u32 cons_pipe_num;
	u32 prod_pipe_num;
};

struct ep_info {
	struct peripheral_ep_info	ph_ep_info;
	struct ipa_ep_pair		ipa_ep_pair;

};


#endif /* __LINUX_USB_CTRL_QTI_H */
