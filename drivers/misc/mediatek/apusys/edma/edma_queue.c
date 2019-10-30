/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: JB Tsai <jb.tsai@mediatek.com>
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

#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/kthread.h>

#include "edma_driver.h"
#include "edma_cmd_hnd.h"
#include "edma_ioctl.h"
#include "edma_queue.h"

DECLARE_VLIST(edma_user);
DECLARE_VLIST(edma_request);

int edma_alloc_request(struct edma_request **rreq)
{
	struct edma_request *req;

	req = kzalloc(sizeof(vlist_type(struct edma_request)), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	*rreq = req;

	return 0;
}

int edma_free_request(struct edma_request *req)
{
	kfree(req);

	return 0;
}

int edma_create_user(struct edma_user **user, struct edma_device *edma_device)
{
	struct edma_user *u;

	u = kzalloc(sizeof(vlist_type(struct edma_user)), GFP_KERNEL);
	if (!u)
		return -ENOMEM;

	u->dev = edma_device->dev;
	u->id = ++(edma_device->edma_num_users);
	u->open_pid = current->pid;
	u->open_tgid = current->tgid;

	mutex_init(&u->data_mutex);
	INIT_LIST_HEAD(&u->enque_list);
	INIT_LIST_HEAD(&u->deque_list);
	init_waitqueue_head(&u->deque_wait);

	mutex_lock(&edma_device->user_mutex);
	list_add_tail(vlist_link(u, struct edma_user), &edma_device->user_list);
	mutex_unlock(&edma_device->user_mutex);

	*user = u;
	return 0;
}

int edma_delete_user(struct edma_user *user, struct edma_device *edma_device)
{
	struct list_head *head, *temp;
	struct edma_request *req;

	edma_flush_requests_from_queue(user);

	/* clear the list of deque */
	mutex_lock(&user->data_mutex);
	list_for_each_safe(head, temp, &user->deque_list) {
		req = vlist_node_of(head, struct edma_request);
		list_del(head);
		edma_free_request(req);
	}
	mutex_unlock(&user->data_mutex);

	mutex_lock(&edma_device->user_mutex);
	list_del(vlist_link(user, struct edma_user));
	mutex_unlock(&edma_device->user_mutex);

	kfree(user);

	return 0;
}

static int edma_enque_handler(struct edma_request *req,
			     struct edma_user *user, struct edma_sub *edma_sub)
{
	int ret = 0;

	edma_power_on(edma_sub);
	edma_enable_sequence(edma_sub);
	switch (req->cmd) {
	case EDMA_PROC_NORMAL:
		ret = edma_normal_mode(edma_sub, req);
		break;
	case EDMA_PROC_FILL:
		ret = edma_fill_mode(edma_sub, req);
		break;
	case EDMA_PROC_NUMERICAL:
		ret = edma_numerical_mode(edma_sub, req);
		break;
	case EDMA_PROC_FORMAT:
		ret = edma_format_mode(edma_sub, req);
		break;
	case EDMA_PROC_COMPRESS:
		ret = edma_compress_mode(edma_sub, req);
		break;
	case EDMA_PROC_DECOMPRESS:
		ret = edma_decompress_mode(edma_sub, req);
		break;
	case EDMA_PROC_RAW:
		ret = edma_raw_mode(edma_sub, req);
		break;
	case EDMA_PROC_EXT_MODE:
		ret = edma_ext_mode(edma_sub, req);
		break;
	default:
		pr_notice("%s: bad command!\n", __func__);
		ret = -EINVAL;
	}
	edma_power_off(edma_sub, 0);

	return ret;
}

void edma_setup_normal_request(struct edma_request *req,
			       struct edma_normal *edma_normal,
			       unsigned int type)
{
	req->handle = (u64) req;
	req->cmd = type;
	req->desp.src_tile_channel = edma_normal->tile_channel;
	req->desp.src_tile_width = edma_normal->tile_width;
	req->desp.src_tile_height = edma_normal->tile_height;
	req->desp.src_channel_stride = edma_normal->src_channel_stride;
	req->desp.src_width_stride = edma_normal->src_width_stride;
	req->desp.dst_tile_channel = edma_normal->tile_channel;
	req->desp.dst_tile_width = edma_normal->tile_width;
	req->desp.dst_channel_stride = edma_normal->dst_channel_stride;
	req->desp.dst_width_stride = edma_normal->dst_width_stride;
	req->desp.src_addr = edma_normal->src_addr;
	req->desp.dst_addr = edma_normal->dst_addr;
	req->buf_iommu_en = edma_normal->buf_iommu_en;
	req->cmd_result = 0;
	req->cmd_status = EDMA_REQ_STATUS_ENQUEUE;
}

void edma_setup_fill_request(struct edma_request *req,
			       struct edma_fill *edma_fill,
			       unsigned int type)
{
	req->handle = (u64) req;
	req->cmd = type;
	req->desp.dst_tile_channel = edma_fill->tile_channel;
	req->desp.dst_tile_width = edma_fill->tile_width;
	req->desp.src_tile_height = edma_fill->tile_height;
	req->desp.dst_channel_stride = edma_fill->dst_channel_stride;
	req->desp.dst_width_stride = edma_fill->dst_width_stride;
	req->desp.dst_addr = edma_fill->dst_addr;
	req->fill_value = edma_fill->fill_value;
	req->buf_iommu_en = edma_fill->buf_iommu_en;
	req->cmd_result = 0;
	req->cmd_status = EDMA_REQ_STATUS_ENQUEUE;
}

void edma_setup_numerical_request(struct edma_request *req,
			       struct edma_numerical *edma_numerical,
			       unsigned int type)
{
	req->handle = (u64) req;
	req->cmd = type;
	req->desp.src_tile_channel = edma_numerical->tile_channel;
	req->desp.src_tile_width = edma_numerical->tile_width;
	req->desp.src_tile_height = edma_numerical->tile_height;
	req->desp.src_channel_stride = edma_numerical->src_channel_stride;
	req->desp.src_width_stride = edma_numerical->src_width_stride;
	req->desp.dst_tile_channel = edma_numerical->tile_channel;
	req->desp.dst_tile_width = edma_numerical->tile_width;
	req->desp.dst_channel_stride = edma_numerical->dst_channel_stride;
	req->desp.dst_width_stride = edma_numerical->dst_width_stride;
	req->desp.src_addr = edma_numerical->src_addr;
	req->desp.dst_addr = edma_numerical->dst_addr;
	req->desp.range_scale = edma_numerical->range_scale;
	req->desp.min_fp32 = edma_numerical->min_fp32;
	req->desp.in_format = edma_numerical->in_format;
	req->desp.out_format = edma_numerical->out_format;
	req->cmd_result = 0;
	req->cmd_status = EDMA_REQ_STATUS_ENQUEUE;
}

void edma_setup_format_request(struct edma_request *req,
			       struct edma_format *edma_format,
			       unsigned int type)
{
	req->handle = (u64) req;
	req->cmd = type;
	req->desp.src_tile_channel = edma_format->src_tile_channel;
	req->desp.src_tile_width = edma_format->src_tile_width;
	req->desp.src_tile_height = edma_format->src_tile_height;
	req->desp.src_channel_stride = edma_format->src_channel_stride;
	req->desp.src_uv_channel_stride = edma_format->src_uv_channel_stride;
	req->desp.src_width_stride = edma_format->src_width_stride;
	req->desp.src_uv_width_stride = edma_format->src_uv_width_stride;
	req->desp.dst_tile_channel = edma_format->dst_tile_channel;
	req->desp.dst_tile_width = edma_format->dst_tile_width;
	req->desp.dst_channel_stride = edma_format->dst_channel_stride;
	req->desp.dst_uv_channel_stride = edma_format->dst_uv_channel_stride;
	req->desp.dst_width_stride = edma_format->dst_width_stride;
	req->desp.dst_uv_width_stride = edma_format->dst_uv_width_stride;
	req->desp.src_addr = edma_format->src_addr;
	req->desp.src_uv_addr = edma_format->src_uv_addr;
	req->desp.dst_addr = edma_format->dst_addr;
	req->desp.dst_uv_addr = edma_format->dst_uv_addr;
	req->desp.param_a = edma_format->param_a;
	req->desp.in_format = edma_format->in_format;
	req->desp.out_format = edma_format->out_format;
	req->cmd_result = 0;
	req->cmd_status = EDMA_REQ_STATUS_ENQUEUE;
}

void edma_setup_compress_request(struct edma_request *req,
			       struct edma_compress *edma_compress,
			       unsigned int type)
{
	req->handle = (u64) req;
	req->cmd = type;
	req->desp.src_tile_channel = edma_compress->src_tile_channel;
	req->desp.src_tile_width = edma_compress->src_tile_width;
	req->desp.src_tile_height = edma_compress->src_tile_height;
	req->desp.src_channel_stride = edma_compress->src_channel_stride;
	req->desp.src_uv_channel_stride = edma_compress->src_uv_channel_stride;
	req->desp.src_width_stride = edma_compress->src_width_stride;
	req->desp.src_uv_width_stride = edma_compress->src_uv_width_stride;
	req->desp.dst_tile_channel = edma_compress->dst_tile_channel;
	req->desp.dst_tile_width = edma_compress->dst_tile_width;
	req->desp.dst_channel_stride = edma_compress->dst_channel_stride;
	req->desp.dst_uv_channel_stride = edma_compress->dst_uv_channel_stride;
	req->desp.dst_width_stride = edma_compress->dst_width_stride;
	req->desp.dst_uv_width_stride = edma_compress->dst_uv_width_stride;
	req->desp.src_addr = edma_compress->src_addr;
	req->desp.src_uv_addr = edma_compress->src_uv_addr;
	req->desp.dst_addr = edma_compress->dst_addr;
	req->desp.dst_uv_addr = edma_compress->dst_uv_addr;
	req->desp.param_a = edma_compress->param_a;
	req->desp.param_m = edma_compress->param_m;
	req->desp.cmprs_src_pxl = edma_compress->cmprs_src_pxl;
	req->desp.cmprs_dst_pxl = edma_compress->cmprs_dst_pxl;
	req->desp.src_c_stride_pxl = edma_compress->src_c_stride_pxl;
	req->desp.src_w_stride_pxl = edma_compress->src_w_stride_pxl;
	req->desp.src_c_offset_m1 = edma_compress->src_c_offset_m1;
	req->desp.src_w_offset_m1 = edma_compress->src_w_offset_m1;
	req->desp.dst_c_stride_pxl = edma_compress->dst_c_stride_pxl;
	req->desp.dst_w_stride_pxl = edma_compress->dst_w_stride_pxl;
	req->desp.dst_c_offset_m1 = edma_compress->dst_c_offset_m1;
	req->desp.dst_w_offset_m1 = edma_compress->dst_w_offset_m1;
	req->desp.in_format = edma_compress->in_format;
	req->desp.out_format = edma_compress->out_format;
	req->desp.rgb2yuv_mat_bypass = edma_compress->rgb2yuv_mat_bypass;
	req->desp.rgb2yuv_mat_select = edma_compress->rgb2yuv_mat_select;
	req->cmd_result = 0;
	req->cmd_status = EDMA_REQ_STATUS_ENQUEUE;
}

void edma_setup_decompress_request(struct edma_request *req,
			       struct edma_decompress *edma_decompress,
			       unsigned int type)
{
	req->handle = (u64) req;
	req->cmd = type;
	req->desp.src_tile_channel = edma_decompress->src_tile_channel;
	req->desp.src_tile_width = edma_decompress->src_tile_width;
	req->desp.src_tile_height = edma_decompress->src_tile_height;
	req->desp.src_channel_stride = edma_decompress->src_channel_stride;
	req->desp.src_uv_channel_stride =
					edma_decompress->src_uv_channel_stride;
	req->desp.src_width_stride = edma_decompress->src_width_stride;
	req->desp.src_uv_width_stride = edma_decompress->src_uv_width_stride;
	req->desp.dst_tile_channel = edma_decompress->dst_tile_channel;
	req->desp.dst_tile_width = edma_decompress->dst_tile_width;
	req->desp.dst_channel_stride = edma_decompress->dst_channel_stride;
	req->desp.dst_uv_channel_stride =
				edma_decompress->dst_uv_channel_stride;
	req->desp.dst_width_stride = edma_decompress->dst_width_stride;
	req->desp.dst_uv_width_stride = edma_decompress->dst_uv_width_stride;
	req->desp.src_addr = edma_decompress->src_addr;
	req->desp.src_uv_addr = edma_decompress->src_uv_addr;
	req->desp.dst_addr = edma_decompress->dst_addr;
	req->desp.dst_uv_addr = edma_decompress->dst_uv_addr;
	req->desp.param_a = edma_decompress->param_a;
	req->desp.param_m = edma_decompress->param_m;
	req->desp.cmprs_src_pxl = edma_decompress->cmprs_src_pxl;
	req->desp.cmprs_dst_pxl = edma_decompress->cmprs_dst_pxl;
	req->desp.src_c_stride_pxl = edma_decompress->src_c_stride_pxl;
	req->desp.src_w_stride_pxl = edma_decompress->src_w_stride_pxl;
	req->desp.src_c_offset_m1 = edma_decompress->src_c_offset_m1;
	req->desp.src_w_offset_m1 = edma_decompress->src_w_offset_m1;
	req->desp.dst_c_stride_pxl = edma_decompress->dst_c_stride_pxl;
	req->desp.dst_w_stride_pxl = edma_decompress->dst_w_stride_pxl;
	req->desp.dst_c_offset_m1 = edma_decompress->dst_c_offset_m1;
	req->desp.dst_w_offset_m1 = edma_decompress->dst_w_offset_m1;
	req->desp.in_format = edma_decompress->in_format;
	req->desp.out_format = edma_decompress->out_format;
	req->desp.rgb2yuv_mat_bypass = edma_decompress->rgb2yuv_mat_bypass;
	req->desp.rgb2yuv_mat_select = edma_decompress->rgb2yuv_mat_select;
	req->cmd_result = 0;
	req->cmd_status = EDMA_REQ_STATUS_ENQUEUE;
}

void edma_setup_raw_request(struct edma_request *req,
			       struct edma_raw *edma_raw,
			       unsigned int type)
{
	req->handle = (u64) req;
	req->cmd = type;
	req->desp.src_tile_channel = edma_raw->src_tile_channel;
	req->desp.src_tile_width = edma_raw->src_tile_width;
	req->desp.src_tile_height = edma_raw->src_tile_height;
	req->desp.src_channel_stride = edma_raw->src_channel_stride;
	req->desp.src_width_stride = edma_raw->src_width_stride;
	req->desp.dst_tile_channel = edma_raw->dst_tile_channel;
	req->desp.dst_tile_width = edma_raw->dst_tile_width;
	req->desp.dst_channel_stride = edma_raw->dst_channel_stride;
	req->desp.dst_width_stride = edma_raw->dst_width_stride;
	req->desp.src_addr = edma_raw->src_addr;
	req->desp.src_uv_addr = edma_raw->src_uv_addr;
	req->desp.dst_addr = edma_raw->dst_addr;
	req->desp.plane_num = edma_raw->plane_num;
	req->desp.unpack_shift = edma_raw->unpack_shift;
	req->desp.bit_num = edma_raw->bit_num;
	req->cmd_result = 0;
	req->cmd_status = EDMA_REQ_STATUS_ENQUEUE;
}

void edma_setup_ext_mode_request(struct edma_request *req,
			       struct edma_ext *edma_ext,
			       unsigned int type)
{
	req->handle = (u64) req;
	req->cmd = type;
	req->ext_reg_addr = edma_ext->reg_addr;
	req->ext_count = edma_ext->count;
	req->fill_value = edma_ext->fill_value;
	req->desp_iommu_en = edma_ext->desp_iommu_en;
	req->cmd_result = 0;
	req->cmd_status = EDMA_REQ_STATUS_ENQUEUE;
}


int edma_push_request_to_queue(struct edma_user *user,
			      struct edma_request *req)
{
	struct edma_device *edma_device;
	int ret = 0;

	if (!user) {
		pr_notice("empty user");
		return -EINVAL;
	}

	edma_device = dev_get_drvdata(user->dev);

	mutex_lock(&user->data_mutex);
	list_add_tail(vlist_link(req, struct edma_request),
		      &user->enque_list);
	mutex_unlock(&user->data_mutex);

	wake_up(&edma_device->req_wait);

	return ret;
}

int edma_pop_request_from_queue(u64 handle,
			       struct edma_user *user,
			       struct edma_request **rreq)
{
	struct list_head *head, *temp;
	struct edma_request *req = NULL;
	int status = EDMA_REQ_STATUS_RUN;

	mutex_lock(&user->data_mutex);
	/* This part should not be happened */
	if (list_empty(&user->deque_list)) {
		mutex_unlock(&user->data_mutex);
		*rreq = NULL;
		return status;
	};

	status = EDMA_REQ_STATUS_INVALID;
	list_for_each_safe(head, temp, &user->deque_list) {
		req = vlist_node_of(head, struct edma_request);
		if (req->handle == handle) {
			list_del_init(vlist_link(req, struct edma_request));
			status = EDMA_REQ_STATUS_DEQUEUE;
			req->cmd_status = EDMA_REQ_STATUS_DEQUEUE;
			break;
		}
	}
	mutex_unlock(&user->data_mutex);

	*rreq = req;

	return status;
}

static bool users_queue_are_empty(struct edma_device *edma_device)
{
	struct list_head *head;
	struct edma_user *user;
	bool is_empty = true;

	mutex_lock(&edma_device->user_mutex);
	list_for_each(head, &edma_device->user_list) {
		user = vlist_node_of(head, struct edma_user);
			if (!list_empty(&user->enque_list)) {
				is_empty = false;
				break;
			}
	}
	mutex_unlock(&edma_device->user_mutex);

	return is_empty;
}

int edma_enque_routine_loop(void *arg)
{
	struct edma_sub *edma_sub = (struct edma_sub *)arg;
	struct list_head *head;
	struct edma_user *user;
	struct edma_request *req;
	u32 sub = edma_sub->sub;
	struct edma_device *edma_device = edma_sub->edma_device;
	DEFINE_WAIT_FUNC(wait, woken_wake_function);

	for (; !kthread_should_stop();) {
		/* wait for requests if there is no one in user's queue */
		add_wait_queue(&edma_device->req_wait, &wait);
		while (1) {
			if (!users_queue_are_empty(edma_device))
				break;

			wait_woken(&wait, TASK_INTERRUPTIBLE,
						MAX_SCHEDULE_TIMEOUT);
		}
		remove_wait_queue(&edma_device->req_wait, &wait);

		/* consume the user's queue */
		mutex_lock(&edma_device->user_mutex);
		list_for_each(head, &edma_device->user_list) {
			user = vlist_node_of(head, struct edma_user);
			mutex_lock(&user->data_mutex);
			/* thread will handle the remaining queue if flush */
			if (user->flush ||
			    list_empty(&user->enque_list)) {
				mutex_unlock(&user->data_mutex);
				continue;
			}

			/* get first node from enque list */
			req =
			    vlist_node_of(user->enque_list.next,
					  struct edma_request);
			list_del_init(vlist_link(req, struct edma_request));
			req->sub = sub;
			user->running = true;
			mutex_unlock(&user->data_mutex);
			/* unlock for avoiding long time locking */
			mutex_unlock(&edma_device->user_mutex);
			edma_enque_handler(req, user, edma_sub);
			mutex_lock(&edma_device->user_mutex);
			mutex_lock(&user->data_mutex);
			list_add_tail(vlist_link(req, struct edma_request),
				      &user->deque_list);
			user->running = false;
			mutex_unlock(&user->data_mutex);

			wake_up_interruptible_all(&user->deque_wait);
		}
		mutex_unlock(&edma_device->user_mutex);
		/* release cpu for another operations */
		usleep_range(1, 10);
	}

	return 0;
}


int edma_flush_requests_from_queue(struct edma_user *user)
{
	struct list_head *head, *temp;
	struct edma_request *req;

	mutex_lock(&user->data_mutex);

	if (!user->running && list_empty(&user->enque_list)) {
		mutex_unlock(&user->data_mutex);
		return 0;
	}

	user->flush = true;
	mutex_unlock(&user->data_mutex);

	/* the running request will add to the deque before interrupt */
	wait_event_interruptible(user->deque_wait, !user->running);

	mutex_lock(&user->data_mutex);
	/* push the remaining enque to the deque */
	list_for_each_safe(head, temp, &user->enque_list) {
		req = vlist_node_of(head, struct edma_request);
		req->cmd_status = EDMA_REQ_STATUS_FLUSH;
		list_del_init(head);
		list_add_tail(head, &user->deque_list);
	}

	user->flush = false;
	mutex_unlock(&user->data_mutex);

	return 0;
}
