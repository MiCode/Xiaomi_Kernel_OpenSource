/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/file.h>
#include <linux/uaccess.h>

#include "mdss_mdp_wfd.h"

/*
 * time out value for wfd to wait for any pending frames to finish
 * assuming 30fps, and max 5 frames in the queue
 */
#define WFD_TIMEOUT_IN_MS 150

struct mdss_mdp_wfd *mdss_mdp_wfd_init(struct device *device,
	struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_wfd *wfd;

	wfd = kzalloc(sizeof(struct mdss_mdp_wfd), GFP_KERNEL);
	if (!wfd) {
		pr_err("fail to allocate wfd session\n");
		return ERR_PTR(-ENOMEM);
	}

	mutex_init(&wfd->lock);
	INIT_LIST_HEAD(&wfd->data_queue);
	init_completion(&wfd->comp);
	wfd->ctl = ctl;
	wfd->device = device;

	return wfd;
}

void mdss_mdp_wfd_deinit(struct mdss_mdp_wfd *wfd)
{
	struct mdss_mdp_wfd_data *node, *temp;

	list_for_each_entry_safe(node, temp, &wfd->data_queue, next)
		 mdss_mdp_wfd_remove_data(wfd, node);

	kfree(wfd);
}

int mdss_mdp_wfd_wait_for_finish(struct mdss_mdp_wfd *wfd)
{
	int ret;

	mutex_lock(&wfd->lock);
	if (list_empty(&wfd->data_queue)) {
		mutex_unlock(&wfd->lock);
		return 0;
	}
	init_completion(&wfd->comp);
	mutex_unlock(&wfd->lock);

	ret = wait_for_completion_timeout(&wfd->comp,
				msecs_to_jiffies(WFD_TIMEOUT_IN_MS));

	if (ret == 0)
		ret = -ETIME;
	else if (ret > 0)
		ret = 0;

	return ret;
}

void mdss_mdp_wfd_destroy(struct mdss_mdp_wfd *wfd)
{
	struct mdss_mdp_ctl *ctl = wfd->ctl;

	if (!ctl)
		return;

	if (ctl->ops.stop_fnc)
		ctl->ops.stop_fnc(ctl, 0);

	if (ctl->wb)
		mdss_mdp_wb_free(ctl->wb);

	if (ctl->mixer_left)
		mdss_mdp_mixer_free(ctl->mixer_left);

	ctl->mixer_left = NULL;
	ctl->wb = NULL;
}

static bool mdss_mdp_wfd_is_config_same(struct mdss_mdp_wfd *wfd,
	struct mdp_output_layer *layer)
{
	struct mdss_mdp_ctl *ctl = wfd->ctl;
	struct mdss_mdp_writeback *wb = NULL;
	struct mdss_mdp_mixer *mixer = NULL;

	wb = ctl->wb;
	mixer = ctl->mixer_left;

	if (!wb || !mixer)
		return false;

	if (wb->num != layer->writeback_ndx)
		return false;

	if (mixer->width != layer->buffer.width)
		return false;

	if (mixer->height != layer->buffer.height)
		return false;

	return true;
}

int mdss_mdp_wfd_setup(struct mdss_mdp_wfd *wfd,
	struct mdp_output_layer *layer)
{
	u32 wb_idx = layer->writeback_ndx;
	struct mdss_mdp_ctl *ctl = wfd->ctl;
	struct mdss_mdp_writeback *wb = NULL;
	struct mdss_mdp_mixer *mixer = NULL;
	int ret = 0;

	if (!ctl)
		return -EINVAL;

	if (mdss_mdp_wfd_is_config_same(wfd, layer)) {
		pr_debug("wfd prepared already\n");
		return 0;
	}

	if (ctl->wb) {
		pr_debug("config change, wait for pending buffer done\n");
		ret = mdss_mdp_wfd_wait_for_finish(wfd);
		if (ret) {
			pr_err("fail to wait for outstanding request\n");
			return ret;
		}
		mdss_mdp_wfd_destroy(wfd);
	}

	wb = mdss_mdp_wb_assign(wb_idx, ctl->num);
	if (!wb) {
		pr_err("could not allocate wb\n");
		ret = -EINVAL;
		goto wfd_setup_error;
	}
	ctl->wb = wb;

	if (wb->caps & MDSS_MDP_WB_INTF)
		mixer = mdss_mdp_mixer_alloc(ctl,
			MDSS_MDP_MIXER_TYPE_INTF, false, 0);
	else
		mixer = mdss_mdp_mixer_assign(wb->num, true);

	if (!mixer) {
		pr_err("could not allocate mixer\n");
		ret = -ENODEV;
		goto wfd_setup_error;
	}
	ctl->mixer_left = mixer;

	if (mixer->type == MDSS_MDP_MIXER_TYPE_INTF ||
			ctl->mdata->wfd_mode == MDSS_MDP_WFD_DEDICATED) {
		ctl->opmode = MDSS_MDP_CTL_OP_WFD_MODE;
	} else {
		switch (mixer->num) {
		case MDSS_MDP_WB_LAYERMIXER0:
			ctl->opmode = MDSS_MDP_CTL_OP_WB0_MODE;
			break;
		case MDSS_MDP_WB_LAYERMIXER1:
			ctl->opmode = MDSS_MDP_CTL_OP_WB1_MODE;
			break;
		default:
			pr_err("Incorrect writeback config num=%d\n",
					mixer->num);
			ret = -EINVAL;
			goto wfd_setup_error;
		}
		ctl->wb_type = MDSS_MDP_WB_CTL_TYPE_LINE;
	}

	ctl->dst_format = layer->buffer.format;
	ctl->dst_comp_ratio = layer->buffer.comp_ratio;
	ctl->width = layer->buffer.width;
	ctl->height = layer->buffer.height;
	ctl->roi =  (struct mdss_rect) {0, 0, ctl->width, ctl->height};

	ctl->is_secure = (layer->flags & MDP_LAYER_SECURE_SESSION);

	mixer->width = layer->buffer.width;
	mixer->height = layer->buffer.height;
	mixer->roi = (struct mdss_rect) {0, 0, mixer->width, mixer->height};
	mixer->ctl = ctl;

	if (ctl->ops.start_fnc) {
		ret = ctl->ops.start_fnc(ctl);
		if (ret) {
			pr_err("wfd start failed %d\n", ret);
			goto wfd_setup_error;
		}
	}

	return ret;

wfd_setup_error:
	mdss_mdp_wfd_destroy(wfd);
	return ret;
}

static int mdss_mdp_wfd_import_data(struct device *device,
	struct mdss_mdp_wfd_data *wfd_data)
{
	int i, ret = 0;
	u32 flags = 0;
	struct mdp_layer_buffer *buffer = &wfd_data->layer.buffer;
	struct mdss_mdp_data *data = &wfd_data->data;
	struct msmfb_data planes[MAX_PLANES];

	if (wfd_data->layer.flags & MDP_LAYER_SECURE_SESSION)
		flags = MDP_SECURE_OVERLAY_SESSION;

	memset(planes, 0, sizeof(planes));

	for (i = 0; i < buffer->plane_count; i++) {
		planes[i].memory_id = buffer->planes[i].fd;
		planes[i].offset = buffer->planes[i].offset;
	}

	ret =  mdss_mdp_data_get_and_validate_size(data, planes,
			buffer->plane_count, flags, device,
			false, DMA_FROM_DEVICE, buffer);

	return ret;
}

struct mdss_mdp_wfd_data *mdss_mdp_wfd_add_data(
	struct mdss_mdp_wfd *wfd,
	struct mdp_output_layer *layer)
{
	int ret;
	struct mdss_mdp_wfd_data *wfd_data;

	if (!wfd->ctl || !wfd->ctl->wb) {
		pr_err("wfd not setup\n");
		return ERR_PTR(-EINVAL);
	}

	wfd_data = kzalloc(sizeof(struct mdss_mdp_wfd_data), GFP_KERNEL);
	if (!wfd_data) {
		pr_err("fail to allocate wfd data\n");
		return ERR_PTR(-ENOMEM);
	}

	wfd_data->layer = *layer;
	ret = mdss_mdp_wfd_import_data(wfd->device, wfd_data);
	if (ret) {
		pr_err("fail to import data\n");
		mdss_mdp_data_free(&wfd_data->data, true, DMA_FROM_DEVICE);
		kfree(wfd_data);
		return ERR_PTR(ret);
	}

	mutex_lock(&wfd->lock);
	list_add_tail(&wfd_data->next, &wfd->data_queue);
	mutex_unlock(&wfd->lock);

	return wfd_data;
}

void mdss_mdp_wfd_remove_data(struct mdss_mdp_wfd *wfd,
	struct mdss_mdp_wfd_data *wfd_data)
{
	mutex_lock(&wfd->lock);
	list_del_init(&wfd_data->next);
	if (list_empty(&wfd->data_queue))
		complete(&wfd->comp);
	mutex_unlock(&wfd->lock);
	mdss_mdp_data_free(&wfd_data->data, true, DMA_FROM_DEVICE);
	kfree(wfd_data);
}

static int mdss_mdp_wfd_validate_out_configuration(struct mdss_mdp_wfd *wfd,
	struct mdp_output_layer *layer)
{
	struct mdss_mdp_format_params *fmt = NULL;
	struct mdss_mdp_ctl *ctl = wfd->ctl;
	u32 wb_idx = layer->writeback_ndx;

	if (mdss_mdp_is_wb_mdp_intf(wb_idx, ctl->num)) {
		fmt = mdss_mdp_get_format_params(layer->buffer.format);
		if (fmt && !(fmt->flag & VALID_MDP_WB_INTF_FORMAT)) {
			pr_err("wb=%d does not support dst fmt:%d\n", wb_idx,
				layer->buffer.format);
			return -EINVAL;
		}
	}
	return 0;
}

int mdss_mdp_wfd_validate(struct mdss_mdp_wfd *wfd,
	struct mdp_output_layer *layer)
{
	u32 wb_idx = layer->writeback_ndx;

	if (mdss_mdp_wfd_validate_out_configuration(wfd, layer)) {
		pr_err("failed to validate output config\n");
		return -EINVAL;
	}

	if (wb_idx > wfd->ctl->mdata->nwb)
		return -EINVAL;

	return 0;
}

int mdss_mdp_wfd_kickoff(struct mdss_mdp_wfd *wfd,
	struct mdss_mdp_commit_cb *commit_cb)
{
	struct mdss_mdp_ctl *ctl = wfd->ctl;
	struct mdss_mdp_writeback_arg wb_args;
	struct mdss_mdp_wfd_data *wfd_data;
	int ret = 0;

	if (!ctl) {
		pr_err("no ctl\n");
		return -EINVAL;
	}

	if (!ctl->wb) {
		pr_err("wfd not prepared\n");
		return -EINVAL;
	}

	mutex_lock(&wfd->lock);
	if (list_empty(&wfd->data_queue)) {
		pr_debug("no output buffer\n");
		mutex_unlock(&wfd->lock);
		mdss_mdp_ctl_notify(ctl, MDP_NOTIFY_FRAME_DONE);
		return 0;
	}
	wfd_data = list_first_entry(&wfd->data_queue,
				struct mdss_mdp_wfd_data, next);
	mutex_unlock(&wfd->lock);

	ret = mdss_mdp_data_map(&wfd_data->data, true, DMA_FROM_DEVICE);
	if (ret) {
		pr_err("fail to acquire output buffer\n");
		goto kickoff_error;
	}

	memset(&wb_args, 0, sizeof(wb_args));
	wb_args.data = &wfd_data->data;

	ret = mdss_mdp_writeback_display_commit(ctl, &wb_args);
	if (ret) {
		pr_err("wfd commit error = %d, ctl=%d\n", ret, ctl->num);
		goto kickoff_error;
	}

	if (commit_cb)
		commit_cb->commit_cb_fnc(
			MDP_COMMIT_STAGE_SETUP_DONE,
			commit_cb->data);

	ret = mdss_mdp_display_wait4comp(ctl);

	if (commit_cb)
		commit_cb->commit_cb_fnc(MDP_COMMIT_STAGE_READY_FOR_KICKOFF,
			commit_cb->data);

kickoff_error:
	mdss_mdp_wfd_commit_done(wfd);
	return ret;
}

int mdss_mdp_wfd_commit_done(struct mdss_mdp_wfd *wfd)
{
	struct mdss_mdp_wfd_data *wfd_data;

	mutex_lock(&wfd->lock);
	if (list_empty(&wfd->data_queue)) {
		pr_err("no output buffer\n");
		mutex_unlock(&wfd->lock);
		return -EINVAL;
	}
	wfd_data = list_first_entry(&wfd->data_queue,
				struct mdss_mdp_wfd_data, next);
	mutex_unlock(&wfd->lock);

	mdss_mdp_wfd_remove_data(wfd, wfd_data);

	return 0;
}

