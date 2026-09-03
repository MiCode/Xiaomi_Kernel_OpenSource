// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "gsp_debug.h"
#include "gsp_layer.h"
#include "gsp_sync.h"

#define GSP_FENCE_WAIT_TIMEOUT 2900/* ms */

static const struct dma_fence_ops gsp_sync_fence_ops;

static struct gsp_sync_timeline *
gsp_sync_fence_to_timeline(struct dma_fence *fence)
{
	BUG_ON(fence->ops != &gsp_sync_fence_ops);
	return container_of(fence->lock, struct gsp_sync_timeline, fence_lock);
}

static const char *gsp_sync_fence_get_driver_name(struct dma_fence *fence)
{
	struct gsp_sync_timeline *tl = gsp_sync_fence_to_timeline(fence);

	return tl->driver_name;
}

static const char *gsp_sync_fence_get_timeline_name(struct dma_fence *fence)
{
	struct gsp_sync_timeline *tl = gsp_sync_fence_to_timeline(fence);

	return tl->timeline_name;
}

static bool gsp_sync_fence_enable_signaling(struct dma_fence *fence)
{
	return true;
}

static const struct dma_fence_ops gsp_sync_fence_ops = {
	.get_driver_name = gsp_sync_fence_get_driver_name,
	.get_timeline_name = gsp_sync_fence_get_timeline_name,
	.enable_signaling = gsp_sync_fence_enable_signaling,
	.wait = dma_fence_default_wait,
};

void gsp_sync_timeline_destroy(struct gsp_sync_timeline *obj)
{
	kfree(obj);
	obj = NULL;
}

struct gsp_sync_timeline *gsp_sync_timeline_create(const char *name)
{
	struct gsp_sync_timeline *obj = NULL;

	GSP_INFO("create timeline: %s\n", name);
	obj = kzalloc(sizeof(struct gsp_sync_timeline), GFP_KERNEL);
	if (IS_ERR_OR_NULL(obj)) {
		GSP_ERR("gsp_sync_timeline allocated failed\n");
		return NULL;
	}

	obj->fence_context = dma_fence_context_alloc(1);
	spin_lock_init(&obj->fence_lock);
	snprintf(obj->driver_name,
		sizeof(obj->timeline_name), "%s", name);
	snprintf(obj->timeline_name,
		sizeof(obj->timeline_name), "%s_timeline", name);

	return obj;
}

void gsp_sync_fence_signal(struct gsp_fence_data *data)
{
	if (data->sig_fen) {
		dma_fence_signal(data->sig_fen);
		dma_fence_put(data->sig_fen);
	}
}

static int gsp_sync_sig_fence_create(struct gsp_sync_timeline *obj,
				struct dma_fence **sig_fen)
{
	struct dma_fence *fence;

	fence = kzalloc(sizeof(*fence), GFP_KERNEL);
	if (!fence) {
		GSP_ERR("fence create failed\n");
		return -ENOMEM;
	}

	dma_fence_init(fence, &gsp_sync_fence_ops, &obj->fence_lock,
				   obj->fence_context, ++obj->fence_seqno);

	/*store the new fence with the sig_fen pointer */
	*sig_fen = fence;

	return 0;
}

int gsp_sync_sig_fd_copy_to_user(struct dma_fence *fence, int32_t __user *ufd)
{
	struct sync_file *sync_file = NULL;
	int fd  = get_unused_fd_flags(O_CLOEXEC);

	if (fd < 0) {
		GSP_ERR("fd overflow, fd: %d\n", fd);
		return fd;
	}

	if (put_user(fd, ufd)) {
		GSP_ERR("signal fence fd copy to user failed\n");
		dma_fence_put(fence);
		goto err;
	}

	sync_file = sync_file_create(fence);
	if (!sync_file) {
		GSP_ERR("signal fence fd copy to user failed\n");
		return -ENOMEM;
	}

	fd_install(fd, sync_file->file);

	GSP_DEBUG("copy signal fd: %d to ufd: %p success\n", fd, ufd);
	return 0;

err:
	put_unused_fd(fd);
	return -1;
}

int gsp_sync_wait_fence_collect(struct dma_fence **wait_fen_arr,
				u32 *count, int fd)
{
	struct dma_fence *fence = NULL;
	int ret = -1;

	if (fd < 0) {
		GSP_DEBUG("wait fd < 0: indicate no need to wait\n");
		return 0;
	}

	if (*count >= GSP_WAIT_FENCE_MAX) {
		GSP_ERR("wait fence overflow, cnt: %d\n", *count);
		return ret;
	}

	fence = sync_file_get_fence(fd);
	if (fence != NULL) {
		/*store the wait fence at the wait_fen array */
		wait_fen_arr[*count] = fence;
		/*update the count of sig_fen */
		(*count)++;
		ret = 0;
	} else
		GSP_ERR("wait fence get from fd: %d error\n", fd);

	return ret;
}

int gsp_sync_fence_process(struct gsp_layer *layer,
			struct gsp_fence_data *data, bool last)
{
	int ret = 0;
	int wait_fd = -1;
	int share_fd = -1;
	enum gsp_layer_type type = GSP_INVAL_LAYER;

	if (IS_ERR_OR_NULL(layer) || IS_ERR_OR_NULL(data)) {
		GSP_ERR("layer[%d] fence process params error\n",
			gsp_layer_to_type(layer));
		return -1;
	}

	wait_fd = gsp_layer_to_wait_fd(layer);
	type = gsp_layer_to_type(layer);
	share_fd = gsp_layer_to_share_fd(layer);

	/* first collect wait fence */
	if (layer->enable == 0 || wait_fd < 0) {
		GSP_DEBUG("layer[%d] no need to collect wait fence\n",
			  gsp_layer_to_type(layer));
	} else {
		ret = gsp_sync_wait_fence_collect(data->wait_fen_arr,
						&data->wait_cnt, wait_fd);
		if (ret < 0) {
			GSP_ERR("collect layer[%d] wait fence failed\n",
				gsp_layer_to_type(layer));
			return ret;
		}
	}


	if (type != GSP_DES_LAYER) {
		GSP_DEBUG("no need to create sig fd for img and osd layer\n");
		return ret;
	}

	ret = gsp_sync_sig_fence_create(data->tl, &data->sig_fen);
	if (ret < 0) {
		GSP_ERR("create signal fence failed\n");
		return ret;
	}

	if (last != true) {
		GSP_DEBUG("no need to copy signal fd to user\n");
		return ret;
	}

	ret = gsp_sync_sig_fd_copy_to_user(data->sig_fen, data->ufd);
	if (ret < 0) {
		GSP_ERR("copy signal fd to user failed\n");
		dma_fence_put(data->sig_fen);
		return ret;
	}

	return ret;
}

void gsp_sync_fence_data_setup(struct gsp_fence_data *data,
			struct gsp_sync_timeline *tl,
			int __user *ufd)
{
	if (IS_ERR_OR_NULL(data) || IS_ERR_OR_NULL(tl)) {
		GSP_ERR("sync fence data set up params error\n");
		return;
	}

	data->tl = tl;
	data->ufd = ufd;
}

void gsp_sync_fence_free(struct gsp_fence_data *data)
{
	int i;

	/* free acuqire fence array */
	for (i = 0; i < data->wait_cnt; i++) {
		if (!data->wait_fen_arr[i]) {
			GSP_WARN("free null acquire fen\n");
			continue;
		}

		dma_fence_put(data->wait_fen_arr[i]);
		data->wait_fen_arr[i] = NULL;
	}
	data->wait_cnt = 0;

	/* signal release fence */
	if (data->sig_fen)
		gsp_sync_fence_signal(data);
}

int gsp_sync_fence_wait(struct gsp_fence_data *data)
{
	signed long ret = 0;
	int i;

	/* wait acuqire fence array */
	for (i = 0; i < data->wait_cnt; i++) {
		if (!data->wait_fen_arr[i]) {
			GSP_WARN("wait null acquire fen\n");
			continue;
		}

		/**
		 * Returns -ERESTARTSYS if interrupted,
		 * 0 if the wait timed out, or the
		 * remaining timeout in jiffies on success.
		 */
		ret = dma_fence_wait_timeout(data->wait_fen_arr[i],
				true, msecs_to_jiffies(GSP_FENCE_WAIT_TIMEOUT));
		if (ret <= 0) {
			GSP_ERR("wait %d/%d fence failed, ret:%ld\n",
				i + 1, data->wait_cnt, ret);
			return -1;
		}
		dma_fence_put(data->wait_fen_arr[i]);
		data->wait_fen_arr[i] = NULL;
	}
	data->wait_cnt = 0;

	return 0;
}
