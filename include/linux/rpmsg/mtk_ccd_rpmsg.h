/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef __LINUX_RPMSG_MTK_RPMSG_H
#define __LINUX_RPMSG_MTK_RPMSG_H

#include <linux/device.h>
#include <linux/remoteproc.h>
#include <linux/rpmsg.h>
#include <linux/idr.h>

#define NAME_MAX_LEN			(32)

struct mtk_ccd_rpmsg_endpoint;
struct ccd_master_listen_item;

typedef void (*ipi_handler_t)(void *data, unsigned int len, void *priv);

struct mtk_ccd_listen_item {
	u32 src;
	char name[NAME_MAX_LEN];
	unsigned int cmd;
};

struct mtk_rpmsg_rproc_subdev {
	struct platform_device *pdev;
	struct mtk_ccd_rpmsg_ops *ops;
	struct rproc_subdev subdev;
	struct rpmsg_device *rpdev;
	struct idr endpoints;
	struct mutex endpoints_lock;
	u32    ccd_msgdev_addr;

	struct mutex master_listen_lock;
	struct mtk_ccd_listen_item listen_obj;
	wait_queue_head_t master_listen_wq;
	wait_queue_head_t ccd_listen_wq;
	atomic_t listen_obj_rdy;
};

struct mtk_rpmsg_device {
	struct rpmsg_device rpdev;
	struct mtk_rpmsg_rproc_subdev *mtk_subdev;
};

struct mtk_ccd_channel_info {
	struct rpmsg_channel_info chinfo;
	u32 id;
};

#define to_mtk_subdev(d) container_of(d, struct mtk_rpmsg_rproc_subdev, subdev)
#define to_mtk_rpmsg_device(r) container_of(r, struct mtk_rpmsg_device, rpdev)

struct mtk_ccd_rpmsg_ops {
	int (*ccd_send)(struct mtk_rpmsg_rproc_subdev *mtk_subdev,
			struct mtk_ccd_rpmsg_endpoint *mept,
			void *buf, unsigned int len, unsigned int wait);
};

struct mtk_rpmsg_device *mtk_create_client_msgdevice(
	struct rproc_subdev *subdev,
	struct rpmsg_channel_info *info);

int mtk_destroy_client_msgdevice(struct rproc_subdev *subdev,
			     struct rpmsg_channel_info *info);

struct rproc_subdev *
mtk_rpmsg_create_rproc_subdev(struct platform_device *pdev,
			      struct mtk_ccd_rpmsg_ops *ops);

void mtk_rpmsg_destroy_rproc_subdev(struct rproc_subdev *subdev);

int mtk_rpmsg_subdev_probe(struct rproc_subdev *subdev);
void mtk_rpmsg_subdev_remove(struct rproc_subdev *subdev);

#endif
