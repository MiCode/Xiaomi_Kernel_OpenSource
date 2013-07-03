/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/
#include <linux/hrtimer.h>
#include <linux/time.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <media/videobuf2-core.h>
#include "enc-subdev.h"
#include "vsg-subdev.h"
#include "wfd-util.h"

#define DEFAULT_FRAME_INTERVAL (66*NSEC_PER_MSEC)
#define DEFAULT_MAX_FRAME_INTERVAL (1*NSEC_PER_SEC)
#define DEFAULT_MODE ((enum vsg_modes)VSG_MODE_CFR)
#define MAX_BUFS_BUSY_WITH_ENC 5

static int vsg_release_input_buffer(struct vsg_context *context,
		struct vsg_buf_info *buf)
{
	WFD_MSG_DBG("Releasing frame with ts %lld ms, paddr %p\n",
			timespec_to_ns(&buf->time),
			(void *)buf->mdp_buf_info.paddr);

	if (buf->flags & VSG_NEVER_RELEASE)
		WFD_MSG_WARN("Warning releasing buffer that's"
				"not supposed to be released\n");

	return context->vmops.release_input_frame(context->vmops.cbdata,
			buf);

}

static int vsg_encode_frame(struct vsg_context *context,
		struct vsg_buf_info *buf)
{
	WFD_MSG_DBG("Encoding frame with ts %lld ms, paddr %p\n",
			timespec_to_ns(&buf->time),
			(void *)buf->mdp_buf_info.paddr);

	return context->vmops.encode_frame(context->vmops.cbdata,
			buf);
}

static void vsg_set_last_buffer(struct vsg_context *context,
		struct vsg_buf_info *buf)
{
	if (buf->flags & VSG_NEVER_SET_LAST_BUFFER)
		WFD_MSG_WARN("Shouldn't be setting this to last buffer\n");

	context->last_buffer = buf;

	WFD_MSG_DBG("Setting last buffer to paddr %p\n",
			(void *)buf->mdp_buf_info.paddr);
}

static void vsg_encode_helper_func(struct work_struct *task)
{
	struct vsg_encode_work *work =
		container_of(task, struct vsg_encode_work, work);

	/*
	 * Note: don't need to lock for context below as we only
	 * access fields that are "static".
	 */
	int rc = vsg_encode_frame(work->context, work->buf);
	if (rc < 0) {
		mutex_lock(&work->context->mutex);
		work->context->state = VSG_STATE_ERROR;
		mutex_unlock(&work->context->mutex);
	}
	kfree(work);
}

static void vsg_work_func(struct work_struct *task)
{
	struct vsg_work *work =
		container_of(task, struct vsg_work, work);
	struct vsg_encode_work *encode_work;
	struct vsg_context *context = work->context;
	struct vsg_buf_info *buf_info = NULL, *temp = NULL;
	int rc = 0, count = 0;
	mutex_lock(&context->mutex);

	if (list_empty(&context->free_queue.node)) {
		WFD_MSG_DBG("%s: queue empty doing nothing\n", __func__);
		goto err_skip_encode;
	} else if (context->state != VSG_STATE_STARTED) {
		WFD_MSG_DBG("%s: vsg is stopped or in error state "
				"doing nothing\n", __func__);
		goto err_skip_encode;
	}

	list_for_each_entry(temp, &context->busy_queue.node, node) {
		if (++count > MAX_BUFS_BUSY_WITH_ENC) {
			WFD_MSG_WARN(
				"Skipping encode, too many buffers with encoder\n");
			goto err_skip_encode;
		}
	}

	buf_info = list_first_entry(&context->free_queue.node,
			struct vsg_buf_info, node);
	list_del(&buf_info->node);
	INIT_LIST_HEAD(&buf_info->node);

	ktime_get_ts(&buf_info->time);
	hrtimer_forward_now(&context->threshold_timer, ns_to_ktime(
				context->max_frame_interval));

	temp = NULL;
	list_for_each_entry(temp, &context->busy_queue.node, node) {
		if (mdp_buf_info_equals(&temp->mdp_buf_info,
					&buf_info->mdp_buf_info)) {
			temp->flags |= VSG_NEVER_RELEASE;
		}
	}

	if (context->last_buffer &&
		mdp_buf_info_equals(&context->last_buffer->mdp_buf_info,
			&buf_info->mdp_buf_info)) {
		context->last_buffer->flags |= VSG_NEVER_RELEASE;
	}

	encode_work = kmalloc(sizeof(*encode_work), GFP_KERNEL);
	encode_work->buf = buf_info;
	encode_work->context = context;
	INIT_WORK(&encode_work->work, vsg_encode_helper_func);
	rc = queue_work(context->work_queue, &encode_work->work);
	if (!rc) {
		WFD_MSG_ERR("Queueing buffer for encode failed\n");
		kfree(encode_work);
		encode_work = NULL;
		goto err_skip_encode;
	}

	buf_info->flags |= VSG_BUF_BEING_ENCODED;
	if (!(buf_info->flags & VSG_NEVER_SET_LAST_BUFFER)) {
		if (context->last_buffer) {
			struct vsg_buf_info *old_last_buffer =
				context->last_buffer;
			bool last_buf_with_us = old_last_buffer &&
				!(old_last_buffer->flags &
					VSG_BUF_BEING_ENCODED);
			bool can_release = old_last_buffer &&
				!(old_last_buffer->flags &
					VSG_NEVER_RELEASE);

			if (old_last_buffer && last_buf_with_us
				&& can_release) {
				vsg_release_input_buffer(context,
					old_last_buffer);
			}

			if (last_buf_with_us)
				kfree(old_last_buffer);

		}
		vsg_set_last_buffer(context, buf_info);
	}

	list_add_tail(&buf_info->node, &context->busy_queue.node);
err_skip_encode:
	mutex_unlock(&context->mutex);
	kfree(work);
}

static void vsg_timer_helper_func(struct work_struct *task)
{
	struct vsg_work *work =
		container_of(task, struct vsg_work, work);
	struct vsg_work *new_work = NULL;
	struct vsg_context *context = work->context;
	int num_bufs_to_queue = 1, c = 0;

	mutex_lock(&context->mutex);

	if (context->state != VSG_STATE_STARTED)
		goto err_locked;

	if (list_empty(&context->free_queue.node)
		&& context->last_buffer) {
		struct vsg_buf_info *info = NULL, *buf_to_encode = NULL;

		if (context->mode == VSG_MODE_CFR)
			num_bufs_to_queue = 1;
		else if (context->mode == VSG_MODE_VFR)
			num_bufs_to_queue = 2;

		for (c = 0; c < num_bufs_to_queue; ++c) {
			info = kzalloc(sizeof(*info), GFP_KERNEL);

			if (!info) {
				WFD_MSG_ERR("Couldn't allocate memory in %s\n",
					__func__);
				goto err_locked;
			}

			buf_to_encode = context->last_buffer;

			info->mdp_buf_info = buf_to_encode->mdp_buf_info;
			info->flags = 0;
			INIT_LIST_HEAD(&info->node);

			list_add_tail(&info->node, &context->free_queue.node);
			WFD_MSG_DBG("Regenerated frame with paddr %p\n",
				(void *)info->mdp_buf_info.paddr);
		}
	}

	for (c = 0; c < num_bufs_to_queue; ++c) {
		new_work = kzalloc(sizeof(*new_work), GFP_KERNEL);
		if (!new_work) {
			WFD_MSG_ERR("Unable to allocate memory"
					"to queue buffer\n");
			goto err_locked;
		}

		INIT_WORK(&new_work->work, vsg_work_func);
		new_work->context = context;
		queue_work(context->work_queue, &new_work->work);
	}

err_locked:
	mutex_unlock(&context->mutex);
	kfree(work);
}

static enum hrtimer_restart vsg_threshold_timeout_func(struct hrtimer *timer)
{
	struct vsg_context *context = NULL;
	struct vsg_work *task = NULL;

	task = kzalloc(sizeof(*task), GFP_ATOMIC);
	context = container_of(timer, struct vsg_context,
			threshold_timer);
	if (!task) {
		WFD_MSG_ERR("Out of memory in %s", __func__);
		goto threshold_err_bad_param;
	} else if (!context) {
		WFD_MSG_ERR("Context not proper in %s", __func__);
		goto threshold_err_no_context;
	}

	INIT_WORK(&task->work, vsg_timer_helper_func);
	task->context = context;

	queue_work(context->work_queue, &task->work);
threshold_err_bad_param:
	hrtimer_forward_now(&context->threshold_timer, ns_to_ktime(
				context->max_frame_interval));
	return HRTIMER_RESTART;
threshold_err_no_context:
	return HRTIMER_NORESTART;
}

int vsg_init(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int vsg_open(struct v4l2_subdev *sd, void *arg)
{
	struct vsg_context *context = NULL;

	if (!arg || !sd)
		return -EINVAL;

	context = kzalloc(sizeof(*context), GFP_KERNEL);
	INIT_LIST_HEAD(&context->free_queue.node);
	INIT_LIST_HEAD(&context->busy_queue.node);

	context->vmops = *(struct vsg_msg_ops *)arg;
	context->work_queue = create_singlethread_workqueue("v4l-vsg");

	context->frame_interval = DEFAULT_FRAME_INTERVAL;
	context->max_frame_interval = DEFAULT_MAX_FRAME_INTERVAL;

	hrtimer_init(&context->threshold_timer, CLOCK_MONOTONIC,
			HRTIMER_MODE_REL);
	context->threshold_timer.function = vsg_threshold_timeout_func;

	context->last_buffer = NULL;
	context->mode = DEFAULT_MODE;
	context->state = VSG_STATE_NONE;
	mutex_init(&context->mutex);

	sd->dev_priv = context;
	return 0;
}

static int vsg_close(struct v4l2_subdev *sd)
{
	struct vsg_context *context = NULL;

	if (!sd)
		return -EINVAL;

	context = (struct vsg_context *)sd->dev_priv;
	destroy_workqueue(context->work_queue);
	kfree(context);
	return 0;
}

static int vsg_start(struct v4l2_subdev *sd)
{
	struct vsg_context *context = NULL;

	if (!sd) {
		WFD_MSG_ERR("ERROR, invalid arguments into %s\n", __func__);
		return -EINVAL;
	}

	context = (struct vsg_context *)sd->dev_priv;

	if (context->state == VSG_STATE_STARTED) {
		WFD_MSG_ERR("VSG not stopped, start not allowed\n");
		return -EINPROGRESS;
	} else if (context->state == VSG_STATE_ERROR) {
		WFD_MSG_ERR("VSG in error state, not allowed to restart\n");
		return -ENOTRECOVERABLE;
	}

	context->state = VSG_STATE_STARTED;
	hrtimer_start(&context->threshold_timer, ns_to_ktime(context->
			max_frame_interval), HRTIMER_MODE_REL);
	return 0;
}

static int vsg_stop(struct v4l2_subdev *sd)
{
	struct vsg_context *context = NULL;

	if (!sd) {
		WFD_MSG_ERR("ERROR, invalid arguments into %s\n", __func__);
		return -EINVAL;
	}

	context = (struct vsg_context *)sd->dev_priv;

	mutex_lock(&context->mutex);
	context->state = VSG_STATE_STOPPED;
	{ /*delete pending buffers as we're not going to encode them*/
		struct list_head *pos, *next;
		list_for_each_safe(pos, next, &context->free_queue.node) {
			struct vsg_buf_info *temp =
				list_entry(pos, struct vsg_buf_info, node);
			list_del(&temp->node);
			kfree(temp);
		}
	}

	hrtimer_cancel(&context->threshold_timer);

	mutex_unlock(&context->mutex);

	flush_workqueue(context->work_queue);
	return 0;
}

static long vsg_queue_buffer(struct v4l2_subdev *sd, void *arg)
{
	struct vsg_context *context = NULL;
	struct vsg_buf_info *buf_info = kzalloc(sizeof(*buf_info), GFP_KERNEL);
	int rc = 0;
	bool push = false;

	if (!arg || !sd) {
		WFD_MSG_ERR("ERROR, invalid arguments into %s\n", __func__);
		rc = -EINVAL;
		goto queue_err_bad_param;
	} else if (!buf_info) {
		WFD_MSG_ERR("ERROR, out of memory in %s\n", __func__);
		rc = -ENOMEM;
		goto queue_err_bad_param;
	}

	context = (struct vsg_context *)sd->dev_priv;
	mutex_lock(&context->mutex);

	*buf_info = *(struct vsg_buf_info *)arg;
	INIT_LIST_HEAD(&buf_info->node);
	buf_info->flags = 0;
	ktime_get_ts(&buf_info->time);

	WFD_MSG_DBG("Queue frame with paddr %p\n",
			(void *)buf_info->mdp_buf_info.paddr);

	{ /*return pending buffers as we're not going to encode them*/
		struct list_head *pos, *next;
		list_for_each_safe(pos, next, &context->free_queue.node) {
			struct vsg_buf_info *temp =
				list_entry(pos, struct vsg_buf_info, node);
			bool is_last_buffer = context->last_buffer &&
				mdp_buf_info_equals(
					&context->last_buffer->mdp_buf_info,
					&temp->mdp_buf_info);

			list_del(&temp->node);

			if (!is_last_buffer &&
				!(temp->flags & VSG_NEVER_RELEASE)) {
				vsg_release_input_buffer(context, temp);
			}
			kfree(temp);
		}
	}

	list_add_tail(&buf_info->node, &context->free_queue.node);

	if (context->mode == VSG_MODE_VFR) {
		if (!context->last_buffer)
			push = true;
		else {
			struct timespec diff = timespec_sub(buf_info->time,
					context->last_buffer->time);
			struct timespec temp = ns_to_timespec(
						context->frame_interval);

			if (timespec_compare(&diff, &temp) >= 0)
				push = true;
		}
	} else if (context->mode == VSG_MODE_CFR) {
		if (!context->last_buffer) {
			push = true;
			/*
			 * We need to reset the timer after pushing the buffer
			 * otherwise, diff between two consecutive frames might
			 * be less than max_frame_interval (for just one sample)
			 */
			hrtimer_forward_now(&context->threshold_timer,
				ns_to_ktime(context->max_frame_interval));
		}
	}

	if (push) {
		struct vsg_work *new_work =
			kzalloc(sizeof(*new_work), GFP_KERNEL);

		INIT_WORK(&new_work->work, vsg_work_func);
		new_work->context = context;
		queue_work(context->work_queue, &new_work->work);
	}

	mutex_unlock(&context->mutex);
queue_err_bad_param:
	if (rc < 0)
		kfree(buf_info);

	return rc;
}

static long vsg_return_ip_buffer(struct v4l2_subdev *sd, void *arg)
{
	struct vsg_context *context = NULL;
	struct vsg_buf_info *buf_info = NULL, *temp = NULL,
			/* last buffer sent for encoding */
			*last_buffer = NULL,
			/* buffer we expected to get back, ideally ==
			 * last_buffer, but might not be if sequence is
			 * encode, encode, return */
			*expected_buffer = NULL,
			/* buffer that we've sent for encoding at some point */
			*known_buffer = NULL;
	bool is_last_buffer = false;
	int rc = 0;

	if (!arg || !sd) {
		WFD_MSG_ERR("ERROR, invalid arguments into %s\n", __func__);
		rc = -EINVAL;
		goto return_ip_buf_err_bad_param;
	}

	context = (struct vsg_context *)sd->dev_priv;
	mutex_lock(&context->mutex);
	buf_info = (struct vsg_buf_info *)arg;
	last_buffer = context->last_buffer;

	WFD_MSG_DBG("Return frame with paddr %p\n",
			(void *)buf_info->mdp_buf_info.paddr);

	if (!list_empty(&context->busy_queue.node)) {
		expected_buffer = list_first_entry(&context->busy_queue.node,
				struct vsg_buf_info, node);
	}

	list_for_each_entry(temp, &context->busy_queue.node, node) {
		if (mdp_buf_info_equals(&temp->mdp_buf_info,
				&buf_info->mdp_buf_info)) {
			known_buffer = temp;
			break;
		}
	}

	if (!expected_buffer || !known_buffer) {
		WFD_MSG_ERR("Unexpectedly received buffer from enc with "
			"paddr %p\n", (void *)buf_info->mdp_buf_info.paddr);
		rc = -EBADHANDLE;
		goto return_ip_buf_bad_buf;
	} else if (known_buffer != expected_buffer) {
		/* Buffers can come back out of order if encoder decides to drop
		 * a frame */
		WFD_MSG_DBG(
				"Got a buffer (%p) out of order. Preferred to get %p\n",
				(void *)known_buffer->mdp_buf_info.paddr,
				(void *)expected_buffer->mdp_buf_info.paddr);
	}

	known_buffer->flags &= ~VSG_BUF_BEING_ENCODED;
	is_last_buffer = context->last_buffer &&
		mdp_buf_info_equals(
				&context->last_buffer->mdp_buf_info,
				&known_buffer->mdp_buf_info);

	list_del(&known_buffer->node);
	if (!is_last_buffer &&
			!(known_buffer->flags & VSG_NEVER_RELEASE)) {
		vsg_release_input_buffer(context, known_buffer);
		kfree(known_buffer);
	}

return_ip_buf_bad_buf:
	mutex_unlock(&context->mutex);
return_ip_buf_err_bad_param:
	return rc;
}

static long vsg_set_frame_interval(struct v4l2_subdev *sd, void *arg)
{
	struct vsg_context *context = NULL;
	int64_t interval;

	if (!arg || !sd) {
		WFD_MSG_ERR("ERROR, invalid arguments into %s\n", __func__);
		return -EINVAL;
	}

	context = (struct vsg_context *)sd->dev_priv;
	interval = *(int64_t *)arg;

	if (interval <= 0) {
		WFD_MSG_ERR("ERROR, invalid interval %lld into %s\n",
				interval, __func__);
		return -EINVAL;
	}

	mutex_lock(&context->mutex);

	context->frame_interval = interval;
	if (interval > context->max_frame_interval) {
		WFD_MSG_WARN("Changing max frame interval from %lld to %lld\n",
				context->max_frame_interval, interval);
		context->max_frame_interval = interval;
	}

	mutex_unlock(&context->mutex);
	return 0;
}

static long vsg_get_frame_interval(struct v4l2_subdev *sd, void *arg)
{
	struct vsg_context *context = NULL;

	if (!arg || !sd) {
		WFD_MSG_ERR("ERROR, invalid arguments into %s\n", __func__);
		return -EINVAL;
	}

	context = (struct vsg_context *)sd->dev_priv;
	mutex_lock(&context->mutex);
	*(int64_t *)arg = context->frame_interval;
	mutex_unlock(&context->mutex);

	return 0;
}

static long vsg_set_max_frame_interval(struct v4l2_subdev *sd, void *arg)
{
	struct vsg_context *context = NULL;
	int64_t interval;

	if (!arg || !sd) {
		WFD_MSG_ERR("ERROR, invalid arguments into %s\n", __func__);
		return -EINVAL;
	}

	context = (struct vsg_context *)sd->dev_priv;
	interval = *(int64_t *)arg;

	if (interval <= 0) {
		WFD_MSG_ERR("ERROR, invalid interval %lld into %s\n",
				interval, __func__);
		return -EINVAL;
	}

	mutex_lock(&context->mutex);

	context->max_frame_interval = interval;
	if (interval < context->frame_interval) {
		WFD_MSG_WARN("Changing frame interval from %lld to %lld\n",
				context->frame_interval, interval);
		context->frame_interval = interval;
	}

	mutex_unlock(&context->mutex);

	return 0;
}

static long vsg_get_max_frame_interval(struct v4l2_subdev *sd, void *arg)
{
	struct vsg_context *context = NULL;

	if (!arg || !sd) {
		WFD_MSG_ERR("ERROR, invalid arguments into %s\n", __func__);
		return -EINVAL;
	}

	context = (struct vsg_context *)sd->dev_priv;
	mutex_lock(&context->mutex);
	*(int64_t *)arg = context->max_frame_interval;
	mutex_unlock(&context->mutex);

	return 0;
}

static long vsg_set_mode(struct v4l2_subdev *sd, void *arg)
{
	struct vsg_context *context = NULL;
	enum vsg_modes *mode = NULL;
	int rc = 0;

	if (!arg || !sd) {
		WFD_MSG_ERR("ERROR, invalid arguments into %s\n", __func__);
		rc = -EINVAL;
		goto set_mode_err_bad_parm;
	}

	context = (struct vsg_context *)sd->dev_priv;
	mutex_lock(&context->mutex);
	mode = arg;

	switch (*mode) {
	case VSG_MODE_CFR:
		context->max_frame_interval = context->frame_interval;
		/*fall through*/
	case VSG_MODE_VFR:
		context->mode = *mode;
		break;
	default:
		context->mode = DEFAULT_MODE;
		rc = -EINVAL;
		goto set_mode_err_bad_mode;
		break;
	}

set_mode_err_bad_mode:
	mutex_unlock(&context->mutex);
set_mode_err_bad_parm:
	return rc;
}

long vsg_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int rc = 0;

	WFD_MSG_DBG("VSG ioctl: %d\n", cmd);
	if (sd == NULL)
		return -EINVAL;

	switch (cmd) {
	case VSG_OPEN:
		rc = vsg_open(sd, arg);
		break;
	case VSG_CLOSE:
		rc = vsg_close(sd);
		break;
	case VSG_START:
		rc = vsg_start(sd);
		break;
	case VSG_STOP:
		rc = vsg_stop(sd);
		break;
	case VSG_Q_BUFFER:
		rc = vsg_queue_buffer(sd, arg);
		break;
	case VSG_RETURN_IP_BUFFER:
		rc = vsg_return_ip_buffer(sd, arg);
		break;
	case VSG_GET_FRAME_INTERVAL:
		rc = vsg_get_frame_interval(sd, arg);
		break;
	case VSG_SET_FRAME_INTERVAL:
		rc = vsg_set_frame_interval(sd, arg);
		break;
	case VSG_GET_MAX_FRAME_INTERVAL:
		rc = vsg_get_max_frame_interval(sd, arg);
		break;
	case VSG_SET_MAX_FRAME_INTERVAL:
		rc = vsg_set_max_frame_interval(sd, arg);
		break;
	case VSG_SET_MODE:
		rc = vsg_set_mode(sd, arg);
		break;
	default:
		rc = -ENOTSUPP;
		break;
	}

	return rc;
}
