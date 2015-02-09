/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <sync.h>

#include "mdss_rotator_internal.h"
#include "mdss_mdp.h"

/* waiting for hw time out, 3 vsync for 30fps*/
#define ROT_HW_ACQUIRE_TIMEOUT_IN_MS 100

/* acquire fence time out, following other driver fence time out practice */
#define ROT_FENCE_WAIT_TIMEOUT MSEC_PER_SEC

#define CLASS_NAME "rotator"
#define DRIVER_NAME "mdss_rotator"

static struct mdss_rot_mgr *rot_mgr;
static void mdss_rotator_wq_handler(struct work_struct *work);

static int mdss_rotator_create_fence(struct mdss_rot_entry *entry)
{
	int ret, fd;
	u32 val;
	struct sync_pt *sync_pt;
	struct sync_fence *fence;
	struct mdss_rot_timeline *rot_timeline;

	if (!entry->queue)
		return -EINVAL;

	rot_timeline = &entry->queue->timeline;

	mutex_lock(&rot_timeline->lock);
	val = rot_timeline->next_value + 1;

	sync_pt = sw_sync_pt_create(rot_timeline->timeline, val);
	if (sync_pt == NULL) {
		pr_err("cannot create sync point\n");
		goto sync_pt_create_err;
	}

	/* create fence */
	fence = sync_fence_create(rot_timeline->fence_name, sync_pt);
	if (fence == NULL) {
		pr_err("%s: cannot create fence\n", rot_timeline->fence_name);
		sync_pt_free(sync_pt);
		ret = -ENOMEM;
		goto sync_pt_create_err;
	}

	fd = get_unused_fd_flags(0);
	if (fd < 0) {
		pr_err("get_unused_fd_flags failed error:0x%x\n", fd);
		ret = fd;
		goto get_fd_err;
	}

	sync_fence_install(fence, fd);
	rot_timeline->next_value++;
	mutex_unlock(&rot_timeline->lock);

	entry->output_fence_fd = fd;
	entry->output_fence = fence;

	return 0;

get_fd_err:
	sync_fence_put(fence);
sync_pt_create_err:
	mutex_unlock(&rot_timeline->lock);
	return ret;
}

static void mdss_rotator_clear_fence(struct mdss_rot_entry *entry)
{
	struct mdss_rot_timeline *rot_timeline;

	if (entry->input_fence) {
		sync_fence_put(entry->input_fence);
		entry->input_fence = NULL;
	}

	rot_timeline = &entry->queue->timeline;

	/* fence failed to copy to user space */
	if (entry->output_fence) {
		sync_fence_put(entry->output_fence);
		entry->output_fence = NULL;
		put_unused_fd(entry->output_fence_fd);

		mutex_lock(&rot_timeline->lock);
		rot_timeline->next_value--;
		mutex_unlock(&rot_timeline->lock);
	}
}

static int mdss_rotator_signal_output(struct mdss_rot_entry *entry)
{
	struct mdss_rot_timeline *rot_timeline;

	if (!entry->queue)
		return -EINVAL;

	rot_timeline = &entry->queue->timeline;

	if (entry->output_signaled) {
		pr_debug("output already signaled\n");
		return 0;
	}

	mutex_lock(&rot_timeline->lock);
	sw_sync_timeline_inc(rot_timeline->timeline, 1);
	mutex_unlock(&rot_timeline->lock);

	entry->output_signaled = true;

	return 0;
}

static int mdss_rotator_wait_for_input(struct mdss_rot_entry *entry)
{
	int ret;

	if (!entry->input_fence) {
		pr_debug("invalid input fence, no wait\n");
		return 0;
	}

	ret = sync_fence_wait(entry->input_fence, ROT_FENCE_WAIT_TIMEOUT);
	sync_fence_put(entry->input_fence);
	entry->input_fence = NULL;
	return ret;
}

static int mdss_rotator_import_buffer(struct mdp_layer_buffer *buffer,
	struct mdss_mdp_data *data, u32 flags, struct device *dev, bool input)
{
	int i, ret = 0;
	struct msmfb_data planes[MAX_PLANES];
	int dir = DMA_TO_DEVICE;

	if (!input)
		dir = DMA_FROM_DEVICE;

	memset(planes, 0, sizeof(planes));

	for (i = 0; i < buffer->plane_count; i++) {
		planes[i].memory_id = buffer->planes[i].fd;
		planes[i].offset = buffer->planes[i].offset;
	}

	ret =  mdss_mdp_data_get(data, planes, buffer->plane_count,
			flags, dev, true, dir);

	return ret;
}

static int mdss_rotator_acquire_data(struct mdss_rot_entry *entry)
{
	int ret;
	struct mdp_layer_buffer *input;
	struct mdp_layer_buffer *output;

	input = &entry->item.input;
	output = &entry->item.output;

	ret = mdss_iommu_ctrl(1);
	if (IS_ERR_VALUE(ret))
		return ret;

	/* if error during map, the caller will release the data */
	ret = mdss_mdp_data_map(&entry->src_buf, true, DMA_TO_DEVICE);
	if (!ret)
		ret = mdss_mdp_data_map(&entry->dst_buf, true, DMA_FROM_DEVICE);

	mdss_iommu_ctrl(0);

	return ret;
}

static void mdss_rotator_release_data(struct mdss_rot_entry *entry)
{
	mdss_mdp_data_free(&entry->src_buf, true, DMA_TO_DEVICE);
	mdss_mdp_data_free(&entry->dst_buf, true, DMA_FROM_DEVICE);
}

static struct mdss_rot_hw_resource *mdss_rotator_hw_alloc(
	struct mdss_rot_mgr *mgr, u32 pipe_id, u32 wb_id)
{
	struct mdss_rot_hw_resource *hw;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 offset = mdss_mdp_get_wb_ctl_support(mdata, true);
	int ret;

	hw = devm_kzalloc(&mgr->pdev->dev, sizeof(struct mdss_rot_hw_resource),
		GFP_KERNEL);
	if (!hw)
		return ERR_PTR(-ENOMEM);

	hw->ctl = mdss_mdp_ctl_alloc(mdata, offset);
	if (!hw->ctl) {
		pr_err("unable to allocate ctl\n");
		ret = -ENODEV;
		goto error;
	}

	if (wb_id == MDSS_ROTATION_HW_ANY)
		hw->wb = mdss_mdp_wb_alloc(MDSS_MDP_WB_ROTATOR, hw->ctl->num);
	else
		hw->wb = mdss_mdp_wb_assign(wb_id, hw->ctl->num);

	if (!hw->wb) {
		pr_err("unable to allocate wb\n");
		ret = -ENODEV;
		goto error;
	}
	hw->ctl->wb = hw->wb;
	hw->mixer = mdss_mdp_mixer_assign(hw->wb->num, true);

	if (!hw->mixer) {
		pr_err("unable to allocate wb mixer\n");
		ret = -ENODEV;
		goto error;
	}
	hw->ctl->mixer_left = hw->mixer;
	hw->mixer->ctl = hw->ctl;

	hw->mixer->rotator_mode = true;

	switch (hw->mixer->num) {
	case MDSS_MDP_WB_LAYERMIXER0:
		hw->ctl->opmode = MDSS_MDP_CTL_OP_ROT0_MODE;
		break;
	case MDSS_MDP_WB_LAYERMIXER1:
		hw->ctl->opmode =  MDSS_MDP_CTL_OP_ROT1_MODE;
		break;
	default:
		pr_err("invalid layer mixer=%d\n", hw->mixer->num);
		ret = -EINVAL;
		goto error;
	}

	hw->ctl->start_fnc = mdss_mdp_writeback_start;
	hw->ctl->power_state = MDSS_PANEL_POWER_ON;
	hw->ctl->wb_type = MDSS_MDP_WB_CTL_TYPE_BLOCK;


	if (hw->ctl->start_fnc)
		ret = hw->ctl->start_fnc(hw->ctl);

	if (ret)
		goto error;

	hw->pipe = mdss_mdp_pipe_alloc_dma(hw->mixer);
	if (!hw->pipe) {
		pr_err("dma pipe allocation failed\n");
		ret = -ENODEV;
		goto error;
	}

	hw->pipe_id = hw->wb->num;
	hw->wb_id = hw->wb->num;

	return hw;
error:
	if (hw->pipe)
		mdss_mdp_pipe_destroy(hw->pipe);
	if (hw->ctl)
		mdss_mdp_ctl_free(hw->ctl);
	devm_kfree(&mgr->pdev->dev, hw);

	return ERR_PTR(ret);
}

static void mdss_rotator_free_hw(struct mdss_rot_mgr *mgr,
	struct mdss_rot_hw_resource *hw)
{
	struct mdss_mdp_mixer *mixer;
	struct mdss_mdp_ctl *ctl;

	mixer = hw->pipe->mixer_left;

	mdss_mdp_pipe_destroy(hw->pipe);

	ctl = mdss_mdp_ctl_mixer_switch(mixer->ctl,
		MDSS_MDP_WB_CTL_TYPE_BLOCK);
	if (ctl) {
		if (ctl->stop_fnc)
			ctl->stop_fnc(ctl, MDSS_PANEL_POWER_OFF);
		mdss_mdp_ctl_free(ctl);
	}

	devm_kfree(&mgr->pdev->dev, hw);
}

struct mdss_rot_hw_resource *mdss_rotator_get_hw_resource(
	struct mdss_rot_queue *queue, struct mdss_rot_entry *entry)
{
	struct mdss_rot_hw_resource *hw = queue->hw;

	if (!hw) {
		pr_err("no hw in the queue\n");
		return NULL;
	}

	mutex_lock(&queue->hw_lock);

	if (hw->workload) {
		hw = ERR_PTR(-EBUSY);
		goto get_hw_resource_err;
	}
	hw->workload = entry;

get_hw_resource_err:
	mutex_unlock(&queue->hw_lock);
	return hw;
}

static void mdss_rotator_put_hw_resource(struct mdss_rot_queue *queue,
	struct mdss_rot_hw_resource *hw)
{
	mutex_lock(&queue->hw_lock);
	hw->workload = NULL;
	mutex_unlock(&queue->hw_lock);
}

/*
 * caller will need to call mdss_rotator_deinit_queue when
 * the function returns error
 */
static int mdss_rotator_init_queue(struct mdss_rot_mgr *mgr)
{
	int i, size, ret = 0;
	char name[32];

	size = sizeof(struct mdss_rot_queue) * mgr->queue_count;
	mgr->queues = devm_kzalloc(&mgr->pdev->dev, size, GFP_KERNEL);
	if (!mgr->queues)
		return -ENOMEM;

	for (i = 0; i < mgr->queue_count; i++) {
		snprintf(name, sizeof(name), "rot_workq_%d", i);
		pr_debug("work queue name=%s\n", name);
		mgr->queues[i].rot_work_queue =
			create_singlethread_workqueue(name);
		if (!mgr->queues[i].rot_work_queue) {
			ret = -EPERM;
			break;
		}

		snprintf(name, sizeof(name), "rot_timeline_%d", i);
		pr_debug("timeline name=%s\n", name);
		mgr->queues[i].timeline.timeline =
			sw_sync_timeline_create(name);
		if (!mgr->queues[i].timeline.timeline) {
			ret = -EPERM;
			break;
		}

		size = sizeof(mgr->queues[i].timeline.fence_name);
		snprintf(mgr->queues[i].timeline.fence_name, size,
				"rot_fence_%d", i);
		mutex_init(&mgr->queues[i].timeline.lock);

		mutex_init(&mgr->queues[i].hw_lock);
	}

	return ret;
}

static void mdss_rotator_deinit_queue(struct mdss_rot_mgr *mgr)
{
	int i;

	if (!mgr->queues)
		return;

	for (i = 0; i < mgr->queue_count; i++) {
		if (mgr->queues[i].rot_work_queue)
			destroy_workqueue(mgr->queues[i].rot_work_queue);

		if (mgr->queues[i].timeline.timeline) {
			struct sync_timeline *obj;
			obj = (struct sync_timeline *)
				mgr->queues[i].timeline.timeline;
			sync_timeline_destroy(obj);
		}
	}
	devm_kfree(&mgr->pdev->dev, mgr->queues);
	mgr->queue_count = 0;
}

static int mdss_rotator_assign_queue(struct mdss_rot_mgr *mgr,
	struct mdss_rot_entry *entry)
{
	struct mdss_rot_queue *queue;
	struct mdss_rot_hw_resource *hw;
	struct mdp_rotation_item *item = &entry->item;
	u32 wb_idx = item->wb_idx;
	u32 pipe_idx = item->pipe_idx;
	int ret = 0;

	/*
	 * todo: instead of always assign writeback block 0, we can
	 * apply some load balancing logic in the future
	 */
	if (wb_idx == MDSS_ROTATION_HW_ANY) {
		wb_idx = 0;
		pipe_idx = 0;
	}

	if (wb_idx >= mgr->queue_count)
		return -EINVAL;

	queue = mgr->queues + wb_idx;

	mutex_lock(&queue->hw_lock);

	if (!queue->hw) {
		hw = mdss_rotator_hw_alloc(mgr, pipe_idx, wb_idx);
		if (IS_ERR_OR_NULL(hw)) {
			pr_err("fail to allocate hw\n");
			ret = PTR_ERR(hw);
		} else {
			queue->hw = hw;
		}
	}

	if (queue->hw) {
		entry->queue = queue;
		queue->hw->pending_count++;
	}

	mutex_unlock(&queue->hw_lock);

	return ret;
}

static void mdss_rotator_unassign_queue(struct mdss_rot_mgr *mgr,
	struct mdss_rot_entry *entry)
{
	struct mdss_rot_queue *queue = entry->queue;

	if (!queue)
		return;

	entry->queue = NULL;

	mutex_lock(&queue->hw_lock);

	if (!queue->hw) {
		pr_err("entry assigned a queue with no hw\n");
		mutex_unlock(&queue->hw_lock);
		return;
	}

	queue->hw->pending_count--;
	if (queue->hw->pending_count == 0) {
		mdss_rotator_free_hw(mgr, queue->hw);
		queue->hw = NULL;
	}

	mutex_unlock(&queue->hw_lock);
}

static void mdss_rotator_queue_request(struct mdss_rot_mgr *mgr,
	struct mdss_rot_entry_container *req)
{
	struct mdss_rot_entry *entry;
	struct mdss_rot_queue *queue;
	int i;

	for (i = 0; i < req->count; i++) {
		entry = req->entries + i;
		queue = entry->queue;
		entry->output_fence = NULL;
		queue_work(queue->rot_work_queue, &entry->commit_work);
	}
}

static int mdss_rotator_calc_perf(struct mdss_rot_perf *perf)
{
	struct mdp_rotation_config *config = &perf->config;

	perf->clk_rate = config->input.width * config->input.height;
	perf->clk_rate *= config->frame_rate;
	/* rotator processes 4 pixels per clock */
	perf->clk_rate /= 4;

	/*
	 * todo: need to revisit to refine the bw calculation
	 */
	perf->bw = 0;

	return 0;
}

static int mdss_rotator_update_perf(struct mdss_rot_mgr *mgr)
{
	struct mdss_rot_file_private *priv;
	u32 clk_rate = 0;

	/*
	 * todo: need to revisit to refine the bw/clock calculation
	 */
	mutex_lock(&rot_mgr->file_lock);
	list_for_each_entry(priv, &mgr->file_list, list) {
		struct mdss_rot_perf *perf;
		mutex_lock(&priv->perf_lock);
		list_for_each_entry(perf, &priv->perf_list, list) {
			clk_rate = max(clk_rate, perf->clk_rate);
		}
		mutex_unlock(&priv->perf_lock);
	}
	mutex_unlock(&rot_mgr->file_lock);

	return 0;
}

static void mdss_rotator_release_entry(struct mdss_rot_mgr *mgr,
	struct mdss_rot_entry *entry)
{
	mdss_rotator_clear_fence(entry);
	mdss_rotator_release_data(entry);
	mdss_rotator_unassign_queue(mgr, entry);
}

static int mdss_rotator_config_dnsc_factor(struct mdss_rot_mgr *mgr,
	struct mdss_rot_entry *entry)
{
	int ret = 0;
	u16 src_w, src_h, dst_w, dst_h, bit;
	struct mdp_rotation_item *item = &entry->item;

	src_w = item->src_rect.w;
	src_h = item->src_rect.h;

	if (item->flags & MDP_ROTATION_90) {
		dst_w = item->dst_rect.h;
		dst_h = item->dst_rect.w;
	} else {
		dst_w = item->dst_rect.w;
		dst_h = item->dst_rect.h;
	}

	if (!mgr->has_downscale &&
		(src_w != dst_w || src_h != dst_h)) {
		pr_err("rotator downscale not supported\n");
		ret = -EINVAL;
		goto dnsc_err;
	}

	entry->dnsc_factor_w = 0;
	entry->dnsc_factor_h = 0;

	if ((src_w != dst_w) || (src_h != dst_h)) {
		if ((src_w % dst_w) || (src_h % dst_h)) {
			ret = -EINVAL;
			goto dnsc_err;
		}
		entry->dnsc_factor_w = src_w / dst_w;
		bit = fls(entry->dnsc_factor_w);
		if ((entry->dnsc_factor_w & ~BIT(bit - 1)) || (bit > 5)) {
			ret = -EINVAL;
			goto dnsc_err;
		}
		entry->dnsc_factor_h = src_h / dst_h;
		bit = fls(entry->dnsc_factor_h);
		if ((entry->dnsc_factor_h & ~BIT(bit - 1)) || (bit > 5)) {
			ret = -EINVAL;
			goto dnsc_err;
		}
	}

dnsc_err:

	/* Downscaler does not support asymmetrical dnsc */
	if (entry->dnsc_factor_w != entry->dnsc_factor_h)
		ret = -EINVAL;

	if (ret) {
		pr_err("Invalid rotator downscale ratio %dx%d->%dx%d\n",
			src_w, src_h, dst_w, dst_h);
		entry->dnsc_factor_w = 0;
		entry->dnsc_factor_h = 0;
	}
	return ret;
}

static u32 mdss_rotator_get_out_format(u32 in_format, bool rot90)
{
	u32 format;

	switch (in_format) {
	case MDP_RGB_565:
	case MDP_BGR_565:
		if (rot90)
			format = MDP_RGB_888;
		else
			format = in_format;
		break;
	case MDP_RGBA_8888:
		format = in_format;
		break;
	case MDP_Y_CBCR_H2V2_VENUS:
	case MDP_Y_CBCR_H2V2:
		if (rot90)
			format = MDP_Y_CRCB_H2V2;
		else
			format = in_format;
		break;
	case MDP_Y_CB_CR_H2V2:
	case MDP_Y_CR_CB_GH2V2:
	case MDP_Y_CR_CB_H2V2:
		format = MDP_Y_CRCB_H2V2;
		break;
	default:
		format = in_format;
		break;
	}

	return format;
}

static int mdss_rotator_validate_entry(struct mdss_rot_mgr *mgr,
	struct mdss_rot_entry *entry)
{
	int ret;
	u32 out_format, in_format;
	struct mdp_rotation_item *item;
	struct mdss_mdp_format_params *fmt;
	bool rotation = false;

	item = &entry->item;
	in_format = item->input.format;

	if (item->wb_idx != item->pipe_idx) {
		pr_err("invalid writeback and pipe idx\n");
		return -EINVAL;
	}

	if (item->wb_idx != MDSS_ROTATION_HW_ANY &&
		item->wb_idx > mgr->queue_count) {
		pr_err("invalid writeback idx\n");
		return -EINVAL;
	}

	fmt = mdss_mdp_get_format_params(in_format);
	if (!fmt) {
		pr_err("invalid input format %u\n", in_format);
		return -EINVAL;
	}

	rotation = (item->flags &  MDP_ROTATION_90) ? true : false;
	out_format =
		mdss_rotator_get_out_format(in_format, rotation);
	if (item->output.format != out_format) {
		pr_err("invalid output format %u\n", item->output.format);
		return -EINVAL;
	}

	ret = mdss_rotator_config_dnsc_factor(mgr, entry);
	if (ret) {
		pr_err("fail to configure downscale factor\n");
		return ret;
	}

	return ret;
}

/*
 * Upon failure from the function, caller needs to make sure
 * to call mdss_rotator_remove_request to clean up resources.
 */
static int mdss_rotator_add_request(struct mdss_rot_mgr *mgr,
	struct mdss_rot_file_private *private,
	struct mdss_rot_entry_container *req)
{
	struct mdss_rot_entry *entry;
	struct mdp_rotation_item *item;
	u32 flag = 0;
	int i, ret;

	for (i = 0; i < req->count; i++) {
		entry = req->entries + i;
		item = &entry->item;

		if (item->flags & MDP_ROTATION_SECURE)
			flag = MDP_SECURE_OVERLAY_SESSION;

		ret = mdss_rotator_validate_entry(mgr, entry);
		if (ret) {
			pr_err("fail to validate the entry\n");
			return ret;
		}

		ret = mdss_rotator_import_buffer(&item->input,
			&entry->src_buf, flag, mgr->device, true);
		if (ret) {
			pr_err("fail to import input buffer\n");
			return ret;
		}

		/*
		 * driver assumes ouput buffer is ready to be written
		 * immediately
		 */
		ret = mdss_rotator_import_buffer(&item->output,
			&entry->dst_buf, flag, mgr->device, false);
		if (ret) {
			pr_err("fail to import output buffer\n");
			return ret;
		}

		if (item->input.fence >= 0) {
			entry->input_fence =
				sync_fence_fdget(item->input.fence);
			if (!entry->input_fence) {
				pr_err("invalid input fence fd\n");
				return -EINVAL;
			}
		}

		ret = mdss_rotator_assign_queue(mgr, entry);
		if (ret) {
			pr_err("fail to assign queue to entry\n");
			return ret;
		}

		entry->request = req;

		INIT_WORK(&entry->commit_work, mdss_rotator_wq_handler);

		ret = mdss_rotator_create_fence(entry);
		if (ret) {
			pr_err("fail to create fence\n");
			return ret;
		}
		item->output.fence = entry->output_fence_fd;
	}

	mutex_lock(&private->req_lock);
	list_add(&req->list, &private->req_list);
	mutex_unlock(&private->req_lock);

	return 0;
}

static void mdss_rotator_remove_request(struct mdss_rot_mgr *mgr,
	struct mdss_rot_file_private *private,
	struct mdss_rot_entry_container *req)
{
	int i;

	mutex_lock(&private->req_lock);
	for (i = 0; i < req->count; i++)
		mdss_rotator_release_entry(mgr, req->entries + i);
	list_del_init(&req->list);
	mutex_unlock(&private->req_lock);
}

static void mdss_rotator_cancel_request(struct mdss_rot_mgr *mgr,
	struct mdss_rot_entry_container *req)
{
	struct mdss_rot_entry *entry;
	int i;

	/*
	 * To avoid signal the rotation entry output fence in the wrong
	 * order, all the entries in the same request needs to be cancelled
	 * first, before signaling the output fence.
	 */
	for (i = req->count - 1; i >= 0; i--) {
		entry = req->entries + i;
		cancel_work_sync(&entry->commit_work);
	}

	for (i = req->count - 1; i >= 0; i--) {
		entry = req->entries + i;
		mdss_rotator_signal_output(entry);
		mdss_rotator_release_entry(mgr, entry);
	}

	list_del_init(&req->list);
	devm_kfree(&mgr->pdev->dev, req);
}

static void mdss_rotator_release_private_resource(
	struct mdss_rot_mgr *mgr,
	struct mdss_rot_file_private *private)
{
	struct mdss_rot_entry_container *req, *req_next;
	struct mdss_rot_perf *perf, *perf_next;

	mutex_lock(&private->req_lock);
	list_for_each_entry_safe(req, req_next, &private->req_list, list)
		mdss_rotator_cancel_request(mgr, req);
	mutex_unlock(&private->req_lock);

	mutex_lock(&private->perf_lock);
	list_for_each_entry_safe(perf, perf_next, &private->perf_list, list) {
		list_del_init(&perf->list);
		devm_kfree(&mgr->pdev->dev, perf);
	}
	mutex_unlock(&private->perf_lock);
}

static void mdss_rotator_free_request(struct mdss_rot_mgr *mgr,
	struct mdss_rot_file_private *private)
{
	struct mdss_rot_entry_container *req, *req_next;

	mutex_lock(&private->req_lock);
	list_for_each_entry_safe(req, req_next, &private->req_list, list) {
		if (atomic_read(&req->pending_count) == 0) {
			list_del_init(&req->list);
			devm_kfree(&mgr->pdev->dev, req);
		}
	}
	mutex_unlock(&private->req_lock);
}

static void mdss_rotator_release_all(struct mdss_rot_mgr *mgr)
{
	struct mdss_rot_file_private *priv, *priv_next;

	mutex_lock(&mgr->file_lock);
	list_for_each_entry_safe(priv, priv_next, &mgr->file_list, list) {
		mdss_rotator_release_private_resource(rot_mgr, priv);
		list_del_init(&priv->list);
		devm_kfree(&mgr->pdev->dev, priv);
	}
	mutex_unlock(&rot_mgr->file_lock);

	mdss_rotator_update_perf(mgr);
}

static int mdss_rotator_prepare_hw(struct mdss_rot_hw_resource *hw,
	struct mdss_rot_entry *entry)
{
	struct mdss_mdp_pipe *pipe;
	struct mdss_mdp_ctl *orig_ctl, *rot_ctl;
	int ret;

	pipe = hw->pipe;
	orig_ctl = pipe->mixer_left->ctl;
	if (orig_ctl->shared_lock)
		mutex_lock(orig_ctl->shared_lock);

	rot_ctl = mdss_mdp_ctl_mixer_switch(orig_ctl,
						MDSS_MDP_WB_CTL_TYPE_BLOCK);
	if (!rot_ctl) {
		ret = -EINVAL;
		goto error;
	} else {
		hw->ctl = rot_ctl;
		pipe->mixer_left = rot_ctl->mixer_left;
	}

	return 0;

error:
	if (orig_ctl->shared_lock)
		mutex_unlock(orig_ctl->shared_lock);
	return ret;
}

static void mdss_rotator_translate_rect(struct mdss_rect *dst,
	struct mdp_rect *src)
{
	dst->x = src->x;
	dst->y = src->y;
	dst->w = src->w;
	dst->h = src->h;
}

static u32 mdss_rotator_translate_flags(u32 input)
{
	u32 output;

	if (input & MDP_ROTATION_NOP)
		output |= MDP_ROT_NOP;
	if (input & MDP_ROTATION_FLIP_LR)
		output |= MDP_FLIP_LR;
	if (input & MDP_ROTATION_FLIP_UD)
		output |= MDP_FLIP_UD;
	if (input & MDP_ROTATION_90)
		output |= MDP_ROT_90;
	if (input & MDP_ROTATION_180)
		output |= MDP_ROT_180;
	if (input & MDP_ROTATION_270)
		output |= MDP_ROT_270;
	if (input & MDP_ROTATION_SECURE)
		output |= MDP_SECURE_OVERLAY_SESSION;
	if (input & MDP_ROTATION_BWC_EN)
		output |= MDP_BWC_EN;

	return output;
}

static int mdss_rotator_config_hw(struct mdss_rot_hw_resource *hw,
	struct mdss_rot_entry *entry)
{
	struct mdss_mdp_pipe *pipe;
	struct mdp_rotation_item *item;
	int ret;

	pipe = hw->pipe;
	item = &entry->item;

	pipe->flags = mdss_rotator_translate_flags(item->flags);
	pipe->src_fmt = mdss_mdp_get_format_params(item->input.format);
	pipe->img_width = item->input.width;
	pipe->img_height = item->input.height;
	mdss_rotator_translate_rect(&pipe->src, &item->src_rect);
	mdss_rotator_translate_rect(&pipe->dst, &item->src_rect);

	pipe->params_changed++;

	mdss_mdp_smp_release(pipe);

	ret = mdss_mdp_smp_reserve(pipe);
	if (ret) {
		pr_debug("unable to mdss_mdp_smp_reserve rot data\n");
		return ret;
	}

	ret = mdss_mdp_pipe_queue_data(pipe, &entry->src_buf);
	return ret;
}

static int mdss_rotator_kickoff_entry(struct mdss_rot_hw_resource *hw,
	struct mdss_rot_entry *entry)
{
	int ret;
	struct mdss_mdp_writeback_arg wb_args = {
		.data = &entry->dst_buf,
		.priv_data = entry,
	};

	ret = mdss_mdp_writeback_display_commit(hw->ctl, &wb_args);
	return ret;
}

static int mdss_rotator_wait_for_entry(struct mdss_rot_hw_resource *hw,
	struct mdss_rot_entry *entry)
{
	int ret;
	struct mdss_mdp_ctl *ctl = hw->ctl;

	ret = mdss_mdp_display_wait4comp(ctl);
	if (ctl->shared_lock)
		mutex_unlock(ctl->shared_lock);
	return ret;
}

static int mdss_rotator_commit_entry(struct mdss_rot_hw_resource *hw,
	struct mdss_rot_entry *entry)
{
	int ret;

	ret = mdss_rotator_prepare_hw(hw, entry);
	if (ret) {
		pr_err("fail to prepare hw resource %d\n", ret);
		return ret;
	}

	ret = mdss_rotator_config_hw(hw, entry);
	if (ret) {
		pr_err("fail to configure hw resource %d\n", ret);
		return ret;
	}

	ret = mdss_rotator_kickoff_entry(hw, entry);
	if (ret) {
		pr_err("fail to do kickoff %d\n", ret);
		return ret;
	}

	ret = mdss_rotator_wait_for_entry(hw, entry);
	if (ret) {
		pr_err("fail to wait for completion %d\n", ret);
		return ret;
	}

	return ret;
}

static int mdss_rotator_handle_entry(struct mdss_rot_hw_resource *hw,
	struct mdss_rot_entry *entry)
{
	int ret;

	ret = mdss_rotator_wait_for_input(entry);
	if (ret) {
		pr_err("wait for input buffer failed %d\n", ret);
		return ret;
	}

	ret = mdss_rotator_acquire_data(entry);
	if (ret) {
		pr_err("fail to prepare input/output data %d\n", ret);
		return ret;
	}

	ret = mdss_rotator_commit_entry(hw, entry);
	if (ret)
		pr_err("rotator commit failed %d\n", ret);

	return ret;
}

static void mdss_rotator_wq_handler(struct work_struct *work)
{
	struct mdss_rot_entry *entry;
	struct mdss_rot_entry_container *request;
	struct mdss_rot_hw_resource *hw;
	int ret;

	entry = container_of(work, struct mdss_rot_entry, commit_work);
	request = entry->request;

	if (!request) {
		pr_err("fatal error, no request with entry\n");
		return;
	}

	hw = mdss_rotator_get_hw_resource(entry->queue, entry);
	if (!hw) {
		pr_err("no hw for the queue\n");
		goto get_hw_res_err;
	}

	ret = mdss_rotator_handle_entry(hw, entry);
	if (ret)
		pr_err("fail to complete the roation request\n");

	mdss_rotator_put_hw_resource(entry->queue, hw);

get_hw_res_err:
	mdss_rotator_signal_output(entry);
	mdss_rotator_release_entry(rot_mgr, entry);
	atomic_dec(&request->pending_count);
}

static int mdss_rotator_validate_request(struct mdss_rot_mgr *mgr,
	struct mdss_rot_entry_container *req)
{
	int i, ret = 0;
	struct mdss_rot_entry *entry;

	for (i = 0; i < req->count; i++) {
		entry = req->entries + i;
		ret = mdss_rotator_validate_entry(mgr, entry);
		if (ret) {
			pr_err("fail to validate the entry\n");
			return ret;
		}
	}

	return ret;
}

static u32 mdss_rotator_generator_session_id(struct mdss_rot_mgr *mgr)
{
	u32 id;
	mutex_lock(&mgr->lock);
	id = mgr->session_id_generator++;
	mutex_unlock(&mgr->lock);
	return id;
}

static struct mdss_rot_perf *mdss_rotator_find_session(
	struct mdss_rot_file_private *private,
	u32 session_id)
{
	struct mdss_rot_perf *perf, *perf_next;
	bool found = false;

	mutex_lock(&private->perf_lock);

	list_for_each_entry_safe(perf, perf_next, &private->perf_list, list) {
		if (perf->config.session_id == session_id) {
			found = true;
			break;
		}
	}

	mutex_unlock(&private->perf_lock);
	if (!found)
		perf = NULL;
	return perf;
}

static int mdss_rotator_open_session(struct mdss_rot_mgr *mgr,
	struct mdss_rot_file_private *private, unsigned long arg)
{
	struct mdp_rotation_config config;
	struct mdss_rot_perf *perf;
	int ret;
	u32 in_format;

	ret = copy_from_user(&config, (void __user *)arg, sizeof(config));
	if (ret) {
		pr_err("fail to copy session data\n");
		return ret;
	}

	perf = devm_kzalloc(&mgr->pdev->dev, sizeof(*perf), GFP_KERNEL);
	if (!perf) {
		pr_err("fail to allocate session\n");
		return -ENOMEM;
	}

	config.session_id = mdss_rotator_generator_session_id(mgr);
	in_format = config.input.format;
	config.output.format =
		mdss_rotator_get_out_format(in_format, true);
	perf->config = config;
	INIT_LIST_HEAD(&perf->list);

	ret = mdss_rotator_calc_perf(perf);
	if (ret) {
		pr_err("error setting the session%d\n", ret);
		goto copy_user_err;
	}

	ret = copy_to_user((void *)arg, &config, sizeof(config));
	if (ret) {
		pr_err("fail to copy to user\n");
		goto copy_user_err;
	}

	mutex_lock(&private->perf_lock);
	list_add(&perf->list, &private->perf_list);
	mutex_unlock(&private->perf_lock);

	ret = mdss_rotator_update_perf(mgr);
	if (ret) {
		pr_err("fail to open session, not enough clk/bw\n");
		mutex_lock(&private->perf_lock);
		list_del_init(&perf->list);
		mutex_unlock(&private->perf_lock);
		goto copy_user_err;
	}

	return ret;
copy_user_err:
	devm_kfree(&mgr->pdev->dev, perf);
	return ret;
}

static int mdss_rotator_close_session(struct mdss_rot_mgr *mgr,
	struct mdss_rot_file_private *private, unsigned long arg)
{
	struct mdss_rot_perf *perf;
	u32 id;

	id = (u32)arg;
	perf = mdss_rotator_find_session(private, id);
	if (!perf)
		return -EINVAL;

	mutex_lock(&private->perf_lock);
	list_del_init(&perf->list);
	devm_kfree(&mgr->pdev->dev, perf);
	mutex_unlock(&private->perf_lock);
	mdss_rotator_update_perf(mgr);
	return 0;
}

static int mdss_rotator_config_session(struct mdss_rot_mgr *mgr,
	struct mdss_rot_file_private *private, unsigned long arg)
{
	int ret;
	struct mdss_rot_perf *perf;
	struct mdp_rotation_config config;

	ret = copy_from_user(&config, (void __user *)arg,
				sizeof(config));
	if (ret) {
		pr_err("fail to copy session data\n");
		return ret;
	}

	perf = mdss_rotator_find_session(private, config.session_id);
	if (!perf)
		return -EINVAL;

	mutex_lock(&private->perf_lock);
	perf->config = config;
	ret = mdss_rotator_calc_perf(perf);
	mutex_unlock(&private->perf_lock);

	if (ret) {
		pr_err("error in configuring the session %d\n", ret);
		return ret;
	}

	ret = mdss_rotator_update_perf(mgr);

	return ret;
}

struct mdss_rot_entry_container *mdss_rotator_req_init(
	struct mdss_rot_mgr *mgr, struct mdp_rotation_item *items,
	u32 count, u32 flags)
{
	struct mdss_rot_entry_container *req;
	int size, i;

	size = sizeof(struct mdss_rot_entry_container);
	size += sizeof(struct mdss_rot_entry) * count;
	req = devm_kzalloc(&mgr->pdev->dev, size, GFP_KERNEL);

	if (!req) {
		pr_err("fail to allocate rotation request\n");
		return ERR_PTR(-ENOMEM);
	}

	INIT_LIST_HEAD(&req->list);
	req->count = count;
	req->entries = (struct mdss_rot_entry *)
		((void *)req + sizeof(struct mdss_rot_entry_container));
	req->flags = flags;
	atomic_set(&req->pending_count, count);

	for (i = 0; i < count; i++)
		req->entries[i].item = items[i];

	return req;
}

static int mdss_rotator_handle_request_common(struct mdss_rot_mgr *mgr,
	struct mdss_rot_file_private *private,
	struct mdss_rot_entry_container *req,
	struct mdp_rotation_item *items)
{
	int i, ret;

	mdss_rotator_free_request(mgr, private);

	ret = mdss_rotator_add_request(mgr, private, req);
	if (ret) {
		pr_err("fail to add rotation request\n");
		mdss_rotator_remove_request(mgr, private, req);
		return ret;
	}

	for (i = 0; i < req->count; i++)
		items[i].output.fence =
			req->entries[i].item.output.fence;

	return ret;
}

static int mdss_rotator_handle_request(struct mdss_rot_mgr *mgr,
	struct mdss_rot_file_private *private, unsigned long arg)
{
	struct mdp_rotation_request user_req;
	struct mdp_rotation_item *items = NULL;
	struct mdss_rot_entry_container *req = NULL;
	int size, ret;

	ret = copy_from_user(&user_req, (void __user *)arg,
					sizeof(user_req));
	if (ret) {
		pr_err("fail to copy rotation request\n");
		return ret;
	}

	/*
	 * here, we make a copy of the items so that we can copy
	 * all the output fences to the client in one call.   Otherwise,
	 * we will have to call multiple copy_to_user
	 */
	size = sizeof(struct mdp_rotation_item) * user_req.count;
	items = devm_kzalloc(&mgr->pdev->dev, size, GFP_KERNEL);
	if (!items) {
		pr_err("fail to allocate rotation items\n");
		return -ENOMEM;
	}
	ret = copy_from_user(items, user_req.list, size);
	if (ret) {
		pr_err("fail to copy rotation items\n");
		goto handle_request_err;
	}

	req = mdss_rotator_req_init(mgr, items, user_req.count, user_req.flags);
	if (IS_ERR_OR_NULL(req)) {
		pr_err("fail to allocate rotation request\n");
		ret = PTR_ERR(req);
		goto handle_request_err;
	}

	mutex_lock(&mgr->lock);

	if (req->flags & MDSS_ROTATION_REQUEST_VALIDATE) {
		ret = mdss_rotator_validate_request(mgr, req);
		goto handle_request_err1;
	}

	ret = mdss_rotator_handle_request_common(mgr, private, req, items);
	if (ret) {
		pr_err("fail to handle request\n");
		goto handle_request_err1;
	}

	ret = copy_to_user(user_req.list, items, size);
	if (ret) {
		pr_err("fail to copy output fence to user\n");
		mdss_rotator_remove_request(mgr, private, req);
		goto handle_request_err1;
	}

	mdss_rotator_queue_request(mgr, req);

	mutex_unlock(&mgr->lock);

	devm_kfree(&mgr->pdev->dev, items);
	return ret;

handle_request_err1:
	mutex_unlock(&mgr->lock);
handle_request_err:
	devm_kfree(&mgr->pdev->dev, items);
	devm_kfree(&mgr->pdev->dev, req);
	return ret;
}

static int mdss_rotator_open(struct inode *inode, struct file *file)
{
	struct mdss_rot_file_private *private;

	if (!rot_mgr)
		return -ENODEV;

	if (atomic_read(&rot_mgr->device_suspended))
		return -EPERM;

	private = devm_kzalloc(&rot_mgr->pdev->dev, sizeof(*private),
		GFP_KERNEL);
	if (!private) {
		pr_err("fail to allocate rotation file private data\n");
		return -ENOMEM;
	}
	mutex_init(&private->req_lock);
	mutex_init(&private->perf_lock);
	INIT_LIST_HEAD(&private->req_list);
	INIT_LIST_HEAD(&private->perf_list);
	INIT_LIST_HEAD(&private->list);

	mutex_lock(&rot_mgr->file_lock);
	list_add(&private->list, &rot_mgr->file_list);
	mutex_unlock(&rot_mgr->file_lock);

	file->private_data = private;

	return 0;
}

static int mdss_rotator_close(struct inode *inode, struct file *file)
{
	struct mdss_rot_file_private *private;

	if (!rot_mgr)
		return -ENODEV;

	if (!file->private_data)
		return -EINVAL;

	private = (struct mdss_rot_file_private *)file->private_data;

	mdss_rotator_release_private_resource(rot_mgr, private);

	mutex_lock(&rot_mgr->file_lock);
	list_del_init(&private->list);
	devm_kfree(&rot_mgr->pdev->dev, private);
	file->private_data = NULL;
	mutex_unlock(&rot_mgr->file_lock);

	mdss_rotator_update_perf(rot_mgr);
	return 0;
}

#ifdef CONFIG_COMPAT
static int mdss_rotator_handle_request32(struct mdss_rot_mgr *mgr,
	struct mdss_rot_file_private *private, unsigned long arg)
{
	struct mdp_rotation_request32 user_req32;
	struct mdp_rotation_item *items = NULL;
	struct mdss_rot_entry_container *req = NULL;
	int size, ret;

	ret = copy_from_user(&user_req32, (void __user *)arg,
					sizeof(user_req32));
	if (ret) {
		pr_err("fail to copy rotation request\n");
		return ret;
	}

	size = sizeof(struct mdp_rotation_item) * user_req32.count;
	items = devm_kzalloc(&mgr->pdev->dev, size, GFP_KERNEL);
	if (!items) {
		pr_err("fail to allocate rotation items\n");
		return -ENOMEM;
	}
	ret = copy_from_user(items, user_req32.list, size);
	if (ret) {
		pr_err("fail to copy rotation items\n");
		goto handle_request32_err;
	}

	req = mdss_rotator_req_init(mgr, items, user_req32.count,
		user_req32.flags);
	if (IS_ERR_OR_NULL(req)) {
		pr_err("fail to allocate rotation request\n");
		ret = PTR_ERR(req);
		goto handle_request32_err;
	}

	mutex_lock(&mgr->lock);

	if (req->flags & MDSS_ROTATION_REQUEST_VALIDATE) {
		ret = mdss_rotator_validate_request(mgr, req);
		goto handle_request32_err1;
	}

	ret = mdss_rotator_handle_request_common(mgr, private, req, items);
	if (ret) {
		pr_err("fail to handle request\n");
		goto handle_request32_err1;
	}

	ret = copy_to_user(user_req32.list, items, size);
	if (ret) {
		pr_err("fail to copy output fence to user\n");
		mdss_rotator_remove_request(mgr, private, req);
		goto handle_request32_err1;
	}

	mdss_rotator_queue_request(mgr, req);

	mutex_unlock(&mgr->lock);

	devm_kfree(&mgr->pdev->dev, items);
	return ret;

handle_request32_err1:
	mutex_unlock(&mgr->lock);
handle_request32_err:
	devm_kfree(&mgr->pdev->dev, items);
	devm_kfree(&mgr->pdev->dev, req);
	return ret;
}

static long mdss_rotator_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	struct mdss_rot_file_private *private;
	int ret = -EINVAL;

	if (!rot_mgr)
		return -ENODEV;

	if (atomic_read(&rot_mgr->device_suspended))
		return -EPERM;

	if (!file->private_data)
		return -EINVAL;

	private = (struct mdss_rot_file_private *)file->private_data;

	switch (cmd) {
	case MDSS_ROTATION_REQUEST:
		ret = mdss_rotator_handle_request32(rot_mgr, private, arg);
		break;
	case MDSS_ROTATION_OPEN:
		ret = mdss_rotator_open_session(rot_mgr, private, arg);
		break;
	case MDSS_ROTATION_CLOSE:
		ret = mdss_rotator_close_session(rot_mgr, private, arg);
		break;
	case MDSS_ROTATION_CONFIG:
		ret = mdss_rotator_config_session(rot_mgr, private, arg);
		break;
	default:
		pr_err("unexpected IOCTL %d\n", cmd);
	}

	if (ret)
		pr_err("rotator ioctl=%d failed, err=%d\n", cmd, ret);
	return ret;

}
#endif

static long mdss_rotator_ioctl(struct file *file, unsigned int cmd,
						 unsigned long arg)
{
	struct mdss_rot_file_private *private;
	int ret = -EINVAL;

	if (!rot_mgr)
		return -ENODEV;

	if (atomic_read(&rot_mgr->device_suspended))
		return -EPERM;

	if (!file->private_data)
		return -EINVAL;

	private = (struct mdss_rot_file_private *)file->private_data;

	switch (cmd) {
	case MDSS_ROTATION_REQUEST:
		ret = mdss_rotator_handle_request(rot_mgr, private, arg);
		break;
	case MDSS_ROTATION_OPEN:
		ret = mdss_rotator_open_session(rot_mgr, private, arg);
		break;
	case MDSS_ROTATION_CLOSE:
		ret = mdss_rotator_close_session(rot_mgr, private, arg);
		break;
	case MDSS_ROTATION_CONFIG:
		ret = mdss_rotator_config_session(rot_mgr, private, arg);
		break;
	default:
		pr_err("unexpected IOCTL %d\n", cmd);
	}

	if (ret)
		pr_err("rotator ioctl=%d failed, err=%d\n", cmd, ret);
	return ret;
}

static ssize_t mdss_rotator_show_capabilities(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t len = PAGE_SIZE;
	int cnt = 0;

	if (!rot_mgr)
		return cnt;

#define SPRINT(fmt, ...) \
		(cnt += scnprintf(buf + cnt, len - cnt, fmt, ##__VA_ARGS__))

	SPRINT("wb_count=%d\n", rot_mgr->queue_count);
	SPRINT("downscale=%d\n", rot_mgr->has_downscale);

	return cnt;
}

static DEVICE_ATTR(caps, S_IRUGO, mdss_rotator_show_capabilities, NULL);

static struct attribute *mdss_rotator_fs_attrs[] = {
	&dev_attr_caps.attr,
	NULL
};

static struct attribute_group mdss_rotator_fs_attr_group = {
	.attrs = mdss_rotator_fs_attrs
};

static const struct file_operations mdss_rotator_fops = {
	.owner = THIS_MODULE,
	.open = mdss_rotator_open,
	.release = mdss_rotator_close,
	.unlocked_ioctl = mdss_rotator_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mdss_rotator_compat_ioctl,
#endif
};

static int mdss_rotator_parse_dt(struct mdss_rot_mgr *mgr,
	struct platform_device *dev)
{
	int ret;
	u32 data;

	ret = of_property_read_u32(dev->dev.of_node,
		"qcom,mdss-wb-count", &data);
	if (ret) {
		pr_err("Error in device tree\n");
		return ret;
	}

	rot_mgr->queue_count = data;
	rot_mgr->has_downscale = of_property_read_bool(dev->dev.of_node,
					   "qcom,mdss-has-downscale");
	return 0;
}

static int mdss_rotator_probe(struct platform_device *pdev)
{
	int ret;

	rot_mgr = devm_kzalloc(&pdev->dev, sizeof(struct mdss_rot_mgr),
		GFP_KERNEL);
	if (!rot_mgr) {
		pr_err("fail to allocate memory\n");
		return -ENOMEM;
	}

	rot_mgr->pdev = pdev;
	ret = mdss_rotator_parse_dt(rot_mgr, pdev);
	if (ret) {
		pr_err("fail to parse the dt\n");
		goto error_parse_dt;
	}

	mutex_init(&rot_mgr->lock);
	atomic_set(&rot_mgr->device_suspended, 0);
	ret = mdss_rotator_init_queue(rot_mgr);
	if (ret) {
		pr_err("fail to init queue\n");
		goto error_get_dev_num;
	}

	mutex_init(&rot_mgr->file_lock);
	INIT_LIST_HEAD(&rot_mgr->file_list);

	platform_set_drvdata(pdev, rot_mgr);

	ret = alloc_chrdev_region(&rot_mgr->dev_num, 0, 1, DRIVER_NAME);
	if (ret  < 0) {
		pr_err("alloc_chrdev_region failed ret = %d\n", ret);
		goto error_get_dev_num;
	}

	rot_mgr->class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(rot_mgr->class)) {
		ret = PTR_ERR(rot_mgr->class);
		pr_err("couldn't create class rc = %d\n", ret);
		goto error_class_create;
	}

	rot_mgr->device = device_create(rot_mgr->class, NULL,
		rot_mgr->dev_num, NULL, DRIVER_NAME);
	if (IS_ERR(rot_mgr->device)) {
		ret = PTR_ERR(rot_mgr->device);
		pr_err("device_create failed %d\n", ret);
		goto error_class_device_create;
	}

	cdev_init(&rot_mgr->cdev, &mdss_rotator_fops);
	ret = cdev_add(&rot_mgr->cdev,
			MKDEV(MAJOR(rot_mgr->dev_num), 0), 1);
	if (ret < 0) {
		pr_err("cdev_add failed %d\n", ret);
		goto error_cdev_add;
	}

	ret = sysfs_create_group(&rot_mgr->device->kobj,
			&mdss_rotator_fs_attr_group);
	if (ret)
		pr_err("unable to register rotator sysfs nodes\n");

	return 0;

error_cdev_add:
	device_destroy(rot_mgr->class, rot_mgr->dev_num);
error_class_device_create:
	class_destroy(rot_mgr->class);
error_class_create:
	unregister_chrdev_region(rot_mgr->dev_num, 1);
error_get_dev_num:
	mdss_rotator_deinit_queue(rot_mgr);
error_parse_dt:
	devm_kfree(&pdev->dev, rot_mgr);
	rot_mgr = NULL;
	return ret;
}

static int mdss_rotator_remove(struct platform_device *dev)
{
	struct mdss_rot_mgr *mgr;

	mgr = (struct mdss_rot_mgr *)platform_get_drvdata(dev);
	if (!mgr)
		return -ENODEV;

	sysfs_remove_group(&rot_mgr->device->kobj, &mdss_rotator_fs_attr_group);

	mdss_rotator_release_all(mgr);

	cdev_del(&rot_mgr->cdev);
	device_destroy(rot_mgr->class, rot_mgr->dev_num);
	class_destroy(rot_mgr->class);
	unregister_chrdev_region(rot_mgr->dev_num, 1);

	mdss_rotator_deinit_queue(rot_mgr);
	devm_kfree(&dev->dev, rot_mgr);
	rot_mgr = NULL;
	return 0;
}

#if defined(CONFIG_PM)
static int mdss_rotator_suspend(struct platform_device *dev, pm_message_t state)
{
	struct mdss_rot_mgr *mgr;

	mgr = (struct mdss_rot_mgr *)platform_get_drvdata(dev);
	if (!mgr)
		return -ENODEV;

	atomic_inc(&mgr->device_suspended);
	mdss_rotator_release_all(mgr);
	return 0;
}

static int mdss_rotator_resume(struct platform_device *dev)
{
	struct mdss_rot_mgr *mgr;

	mgr = (struct mdss_rot_mgr *)platform_get_drvdata(dev);
	if (!mgr)
		return -ENODEV;

	atomic_dec(&mgr->device_suspended);
	return 0;
}
#endif

static const struct of_device_id mdss_rotator_dt_match[] = {
	{ .compatible = "qcom,mdss_rotator",},
	{}
};

MODULE_DEVICE_TABLE(of, mdss_rotator_dt_match);

static struct platform_driver mdss_rotator_driver = {
	.probe = mdss_rotator_probe,
	.remove = mdss_rotator_remove,
#if defined(CONFIG_PM)
	.suspend = mdss_rotator_suspend,
	.resume = mdss_rotator_resume,
#endif
	.driver = {
		.name = "mdss_rotator",
		.of_match_table = mdss_rotator_dt_match,
		.pm = NULL,
	}
};

static int __init mdss_rotator_init(void)
{
	return platform_driver_register(&mdss_rotator_driver);
}

static void __exit mdss_rotator_exit(void)
{
	return platform_driver_unregister(&mdss_rotator_driver);
}

module_init(mdss_rotator_init);
module_exit(mdss_rotator_exit);

MODULE_DESCRIPTION("MSM Rotator driver");
MODULE_LICENSE("GPL v2");
