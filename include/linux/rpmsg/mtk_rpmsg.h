/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef __LINUX_MTK_RPMSG_H
#define __LINUX_MTK_RPMSG_H

#include <linux/device.h>
#include <linux/rpmsg.h>

struct mtk_rpmsg_channel_info {
	struct rpmsg_channel_info info;
	//bool registered;
	//struct list_head list;
	unsigned int send_slot; //send slot offset
	unsigned int recv_slot; //recv slot offset
	unsigned int send_slot_size; // send slot count
	unsigned int recv_slot_size; // recv slot count
	unsigned int send_pin_index; // pin irq index
	unsigned int recv_pin_index; // pin irq index
	unsigned int send_pin_offset;// pin array offset
	unsigned int recv_pin_offset;// pin array offset
	unsigned int mbox; //mbox
	spinlock_t channel_lock;
};

struct mtk_rpmsg_endpoint {
	struct rpmsg_endpoint ept;
	//struct mtk_rpmsg_rproc_subdev *mtk_subdev;
	struct mtk_rpmsg_device *mdev;
	struct mtk_rpmsg_channel_info *mchan;
};

struct mtk_rpmsg_operations {
#if 0
	int (*register_ipi)(struct platform_device *pdev, enum scp_ipi_id id,
			    scp_ipi_handler_t handler, void *priv);
	void (*unregister_ipi)(struct platform_device *pdev,
			       enum scp_ipi_id id);
	int (*send_ipi)(struct platform_device *pdev, enum scp_ipi_id id,
			void *buf, unsigned int len, unsigned int wait);
#endif
	int (*mbox_send)(struct mtk_rpmsg_endpoint *mept,
		struct mtk_rpmsg_channel_info *mchan,
		void *buf, unsigned int len, unsigned int wait);
};

struct mtk_rpmsg_device {
	struct rpmsg_device rpdev;
	//struct mtk_rpmsg_rproc_subdev *mtk_subdev;
	struct platform_device *pdev;
	struct mtk_rpmsg_operations *ops;
	struct mtk_mbox_device *mbdev;

};

/*
 * create mtk rpmsg device
 */
struct mtk_rpmsg_device *mtk_rpmsg_create_device(struct platform_device *pdev,
		struct mtk_mbox_device *mbdev, unsigned int ipc_chan_id);
/*
 * create mtk rpmsg channel
 */
struct mtk_rpmsg_channel_info *
mtk_rpmsg_create_channel(struct mtk_rpmsg_device *mdev, u32 chan_id,
		char *name);

#endif
