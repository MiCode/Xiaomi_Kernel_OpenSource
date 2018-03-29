/*
* Copyright (c) 2015 MediaTek Inc.
* Author: Tiffany Lin <tiffany.lin@mediatek.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#include <linux/errno.h>
#include <linux/wait.h>

#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_intr.h"
#include "mtk_vcodec_util.h"

void mtk_vcodec_clean_dev_int_flags(void *data)
{
	struct mtk_vcodec_dev *dev = (struct mtk_vcodec_dev *)data;

	dev->int_cond = 0;
	dev->int_type = 0;
}

int mtk_vcodec_wait_for_done_ctx(void *data, int command,
				 unsigned int timeout, int interrupt)
{
	wait_queue_head_t *waitqueue;
	long timeout_jiff, ret;
	int status = 0;
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)data;

	waitqueue = (wait_queue_head_t *)&ctx->queue;
	timeout_jiff = msecs_to_jiffies(timeout);
	if (interrupt) {
		ret = wait_event_interruptible_timeout(*waitqueue,
				(ctx->int_cond &&
				(ctx->int_type == command)),
				timeout_jiff);
	} else {
		ret = wait_event_timeout(*waitqueue,
				(ctx->int_cond &&
				(ctx->int_type == command)),
				 timeout_jiff);
	}
	if (0 == ret) {
		status = -1;	/* timeout */
		mtk_v4l2_err("[%d] cmd=%d, wait_event_interruptible_timeout time=%lu out %d %d!",
				ctx->idx, command, timeout_jiff, ctx->int_cond,
				ctx->int_type);
	} else if (-ERESTARTSYS == ret) {
		mtk_v4l2_err("[%d] cmd=%d, wait_event_interruptible_timeout interrupted by a signal %d %d",
				ctx->idx, command, ctx->int_cond,
				ctx->int_type);
		status = -1;
	}

	ctx->int_cond = 0;
	ctx->int_type = 0;

	return status;
}

int mtk_vcodec_wait_for_done_dev(void *data, int command,
				 unsigned int timeout, int interrupt)
{
	wait_queue_head_t *waitqueue;
	long timeout_jiff, ret;
	int status = 0;
	struct mtk_vcodec_dev *dev = (struct mtk_vcodec_dev *)data;

	waitqueue = (wait_queue_head_t *)&dev->queue;
	timeout_jiff = msecs_to_jiffies(timeout);
	if (interrupt) {
		ret = wait_event_interruptible_timeout(*waitqueue,
				(dev->int_cond &&
				(dev->int_type == command)),
				timeout_jiff);
	} else {
		ret = wait_event_timeout(*waitqueue,
				(dev->int_cond &&
				(dev->int_type == command)),
				timeout_jiff);
	}
	if (0 == ret) {
		status = -1;	/* timeout */
		mtk_v4l2_err("wait_event_interruptible_timeout time=%lu out %d %d!",
				timeout_jiff, dev->int_cond, dev->int_type);
	} else if (-ERESTARTSYS == ret) {
		mtk_v4l2_err("wait_event_interruptible_timeout interrupted by a signal %d %d",
				dev->int_cond, dev->int_type);
		status = -1;
	}
	dev->int_cond = 0;
	dev->int_type = 0;
	return status;
}
