/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_RPMSG_INTERNAL_H__
#define __MTK_RPMSG_INTERNAL_H__

#include <linux/device.h>
#include <linux/remoteproc.h>
#include <linux/rpmsg.h>
#include <linux/idr.h>

#define MTK_CCD_MSGDEV_ADDR (0x3f0)

struct ccd_worker_item;
struct mtk_ccd_channel_info;

enum ccd_mept_state {
	CCD_MENDPOINT_CREATED = 0,
	CCD_MENDPOINT_DESTROY
};

struct mtk_ccd_queue {
	struct list_head queue;
	spinlock_t queue_lock; /* Protect queue operation */
};

struct mtk_ccd_params {
	struct list_head list_entry;
	struct ccd_worker_item worker_obj;
};

struct mtk_ccd_rpmsg_endpoint {
	struct rpmsg_endpoint ept;
	struct mtk_ccd_channel_info mchinfo;
	struct mtk_rpmsg_rproc_subdev *mtk_subdev;
	wait_queue_head_t worker_readwq;
	wait_queue_head_t ccd_paramswq;
	atomic_t ccd_params_rdy;	/* Should be 0 or 1 */
	struct mtk_ccd_queue pending_sendq;
	atomic_t worker_read_rdy;	/* Should be 0 or 1 */
	atomic_t ccd_cmd_sent;	/* Should be 0, 1, ..., N */
	atomic_t ccd_mep_state;	/* enum ccd_mept_state */
};

struct mtk_ccd_mchinfo_entry {
	struct list_head list_entry;
	struct mtk_ccd_channel_info *mchinfo;
};

#define to_mtk_rpmsg_endpoint(r) \
	container_of(r, struct mtk_ccd_rpmsg_endpoint, ept)

int ccd_msgdev_init(void);

void __ept_release(struct kref *kref);

int
mtk_rpmsg_destroy_rpmsgdev(struct mtk_rpmsg_rproc_subdev *mtk_subdev,
			   struct rpmsg_channel_info *info);

#endif
