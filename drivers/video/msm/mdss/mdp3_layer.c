/* Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
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

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/pm_runtime.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/msm_mdp.h>
#include <linux/memblock.h>
#include <linux/sync.h>
#include <linux/sw_sync.h>
#include <linux/file.h>

#include <soc/qcom/event_timer.h>
#include "mdp3_ctrl.h"
#include "mdp3.h"
#include "mdp3_ppp.h"
#include "mdp3_ctrl.h"
#include "mdss_fb.h"

enum {
	MDP3_RELEASE_FENCE = 0,
	MDP3_RETIRE_FENCE,
};

static struct sync_fence *__mdp3_create_fence(struct msm_fb_data_type *mfd,
	struct msm_sync_pt_data *sync_pt_data, u32 fence_type,
	int *fence_fd, int value)
{
	struct sync_fence *sync_fence = NULL;
	char fence_name[32];
	struct mdp3_session_data *mdp3_session;

	mdp3_session = (struct mdp3_session_data *)mfd->mdp.private1;

	if ((fence_type == MDP3_RETIRE_FENCE) &&
		(mfd->panel.type == MIPI_CMD_PANEL)) {
		if (mdp3_session->vsync_timeline) {
			value = mdp3_session->vsync_timeline->value + 1 +
				mdp3_session->retire_cnt++;
		} else {
			return ERR_PTR(-EPERM);
		}
	}

	if (fence_type == MDP3_RETIRE_FENCE)
		snprintf(fence_name, sizeof(fence_name), "fb%d_retire_%d",
			mfd->index, value);
	else
		snprintf(fence_name, sizeof(fence_name), "fb%d_release_%d",
			mfd->index, value);

	if ((fence_type == MDP3_RETIRE_FENCE) &&
		(mfd->panel.type == MIPI_CMD_PANEL)) {
			sync_fence = mdss_fb_sync_get_fence(
					mdp3_session->vsync_timeline,
						fence_name, value);
	} else {
		sync_fence = mdss_fb_sync_get_fence(sync_pt_data->timeline,
			fence_name, value);
	}

	if (IS_ERR_OR_NULL(sync_fence)) {
		pr_err("%s: unable to retrieve release fence\n", fence_name);
		goto end;
	}

	/* get fence fd */
	*fence_fd = get_unused_fd_flags(0);
	if (*fence_fd < 0) {
		pr_err("%s: get_unused_fd_flags failed error:0x%x\n",
			fence_name, *fence_fd);
		sync_fence_put(sync_fence);
		sync_fence = NULL;
		goto end;
	}

	sync_fence_install(sync_fence, *fence_fd);
end:

	return sync_fence;
}

/*
 * __handle_buffer_fences() - copy sync fences and return release
 * fence to caller.
 *
 * This function copies all input sync fences to acquire fence array and
 * returns release fences to caller. It acts like buff_sync ioctl.
 */
static int __mdp3_handle_buffer_fences(struct msm_fb_data_type *mfd,
	struct mdp_layer_commit_v1 *commit, struct mdp_input_layer *layer_list)
{
	struct sync_fence *fence, *release_fence, *retire_fence;
	struct msm_sync_pt_data *sync_pt_data = NULL;
	struct mdp_input_layer *layer;
	int value;

	u32 acq_fen_count, i, ret = 0;
	u32 layer_count = commit->input_layer_cnt;

	sync_pt_data = &mfd->mdp_sync_pt_data;
	if (!sync_pt_data) {
		pr_err("sync point data are NULL\n");
		return -EINVAL;
	}

	i = mdss_fb_wait_for_fence(sync_pt_data);
	if (i > 0)
		pr_warn("%s: waited on %d active fences\n",
			sync_pt_data->fence_name, i);

	mutex_lock(&sync_pt_data->sync_mutex);
	for (i = 0, acq_fen_count = 0; i < layer_count; i++) {
		layer = &layer_list[i];

		if (layer->buffer.fence < 0)
			continue;

		fence = sync_fence_fdget(layer->buffer.fence);
		if (!fence) {
			pr_err("%s: sync fence get failed! fd=%d\n",
				sync_pt_data->fence_name, layer->buffer.fence);
			ret = -EINVAL;
			goto sync_fence_err;
		} else {
			sync_pt_data->acq_fen[acq_fen_count++] = fence;
		}
	}

	sync_pt_data->acq_fen_cnt = acq_fen_count;
	if (ret)
		goto sync_fence_err;

	value = sync_pt_data->timeline_value + sync_pt_data->threshold +
			atomic_read(&sync_pt_data->commit_cnt);

	release_fence = __mdp3_create_fence(mfd, sync_pt_data,
		MDP3_RELEASE_FENCE, &commit->release_fence, value);
	if (IS_ERR_OR_NULL(release_fence)) {
		pr_err("unable to retrieve release fence\n");
		ret = PTR_ERR(release_fence);
		goto release_fence_err;
	}

	retire_fence = __mdp3_create_fence(mfd, sync_pt_data,
		MDP3_RETIRE_FENCE, &commit->retire_fence, value);
	if (IS_ERR_OR_NULL(retire_fence)) {
		pr_err("unable to retrieve retire fence\n");
		ret = PTR_ERR(retire_fence);
		goto retire_fence_err;
	}

	mutex_unlock(&sync_pt_data->sync_mutex);
	return ret;

retire_fence_err:
	put_unused_fd(commit->release_fence);
	sync_fence_put(release_fence);
release_fence_err:
	commit->retire_fence = -1;
	commit->release_fence = -1;
sync_fence_err:
	for (i = 0; i < sync_pt_data->acq_fen_cnt; i++)
		sync_fence_put(sync_pt_data->acq_fen[i]);
	sync_pt_data->acq_fen_cnt = 0;

	mutex_unlock(&sync_pt_data->sync_mutex);

	return ret;
}

/*
 * __map_layer_buffer() - map input layer buffer
 *
 */
static int __mdp3_map_layer_buffer(struct msm_fb_data_type *mfd,
		struct mdp_input_layer *input_layer)
{
	struct mdp3_session_data *mdp3_session = mfd->mdp.private1;
	struct mdp_input_layer *layer = NULL;
	struct mdp_layer_buffer *buffer;
	struct msmfb_data img;
	bool is_panel_type_cmd = false;
	struct mdp3_img_data data;
	int intf_type;
	int rc = 0;

	layer = &input_layer[0];
	buffer = &layer->buffer;

	/* current implementation only supports one plane mapping */
	if (buffer->planes[0].fd < 0) {
		pr_err("invalid file descriptor for layer buffer\n");
		goto err;
	}

	intf_type = mdp3_get_ion_client(mfd);
	memset(&img, 0, sizeof(img));
	img.memory_id = buffer->planes[0].fd;
	img.offset = buffer->planes[0].offset;

	memset(&data, 0, sizeof(struct mdp3_img_data));

	if (mfd->panel.type == MIPI_CMD_PANEL || intf_type == MDP3_CLIENT_SPI)
		is_panel_type_cmd = true;
	if (is_panel_type_cmd) {
		rc = mdp3_iommu_enable(intf_type);
		if (rc) {
			pr_err("fail to enable iommu\n");
			return rc;
		}
	}

	if (layer->flags & MDP_LAYER_SECURE_DISPLAY_SESSION)
		data.flags |=  MDP_SECURE_DISPLAY_OVERLAY_SESSION;

	rc = mdp3_get_img(&img, &data, intf_type);
	if (rc) {
		pr_err("fail to get overlay buffer\n");
		goto err;
	}

	rc = mdp3_bufq_push(&mdp3_session->bufq_in, &data);
	if (rc) {
		pr_err("fail to queue the overlay buffer, buffer drop\n");
		mdp3_put_img(&data, MDP3_CLIENT_DMA_P);
		goto err;
	}
	rc = 0;
err:
	if (is_panel_type_cmd)
		mdp3_iommu_disable(MDP3_CLIENT_DMA_P);
	return rc;
}

static void mdp3_validate_secure_layer(struct msm_fb_data_type *mfd,
		 struct mdp_input_layer *input_layer)
{
	struct mdp3_session_data *mdp3_session = mfd->mdp.private1;
	struct mdp_input_layer *layer = &input_layer[0];

	if (!atomic_read(&mdp3_session->secure_display) &&
			(layer->flags & MDP_LAYER_SECURE_DISPLAY_SESSION)) {
		mdp3_session->transition_state = NONSECURE_TO_SECURE;
	} else if (atomic_read(&mdp3_session->secure_display) &&
			!(layer->flags & MDP_LAYER_SECURE_DISPLAY_SESSION)) {
		mdp3_session->transition_state = SECURE_TO_NONSECURE;
	} else {
		mdp3_session->transition_state = NO_TRANSITION;
	}
}

int mdp3_layer_pre_commit(struct msm_fb_data_type *mfd,
	struct file *file, struct mdp_layer_commit_v1 *commit)
{
	int ret;
	struct mdp_input_layer *layer, *layer_list;
	struct mdp3_session_data *mdp3_session;
	struct mdp3_dma *dma;
	int layer_count = commit->input_layer_cnt;
	int stride, format, client;

	/* Handle NULL commit */
	if (!layer_count) {
		pr_debug("Handle NULL commit\n");
		return 0;
	}

	mdp3_session = mfd->mdp.private1;
	dma = mdp3_session->dma;

	mutex_lock(&mdp3_session->lock);

	client = mdp3_get_ion_client(mfd);
	mdp3_bufq_deinit(&mdp3_session->bufq_in, client);

	layer_list = commit->input_layers;
	layer = &layer_list[0];

	stride = layer->buffer.width * ppp_bpp(layer->buffer.format);
	format = mdp3_ctrl_get_source_format(layer->buffer.format);
	pr_debug("stride:%d layer_width:%d", stride, layer->buffer.width);

	if ((dma->source_config.format != format) ||
			(dma->source_config.stride != stride)) {
		dma->source_config.format = format;
		dma->source_config.stride = stride;
		dma->output_config.pack_pattern =
			mdp3_ctrl_get_pack_pattern(layer->buffer.format);
		dma->update_src_cfg = true;
	}
	mdp3_session->overlay.id = 1;

	ret = __mdp3_handle_buffer_fences(mfd, commit, layer_list);
	if (ret) {
		pr_err("Failed to handle buffer fences\n");
		mutex_unlock(&mdp3_session->lock);
		return ret;
	}

	mdp3_validate_secure_layer(mfd, layer);

	ret = __mdp3_map_layer_buffer(mfd, layer);
	if (ret) {
		pr_err("Failed to map buffer\n");
		mutex_unlock(&mdp3_session->lock);
		return ret;
	}

	pr_debug("mdp3 precommit ret = %d\n", ret);
	mutex_unlock(&mdp3_session->lock);
	return ret;
}

/*
 * mdp3_layer_atomic_validate() - validate input layers
 * @mfd:	Framebuffer data structure for display
 * @commit:	Commit version-1 structure for display
 *
 * This function validates only input layers received from client. It
 * does perform any validation for mdp_output_layer defined for writeback
 * display.
 */
int mdp3_layer_atomic_validate(struct msm_fb_data_type *mfd,
	struct file *file, struct mdp_layer_commit_v1 *commit)
{
	struct mdp3_session_data *mdp3_session;

	if (!mfd || !commit) {
		pr_err("invalid input params\n");
		return -EINVAL;
	}

	if (mdss_fb_is_power_off(mfd)) {
		pr_err("display interface is in off state fb:%d\n",
			mfd->index);
		return -EPERM;
	}

	mdp3_session = mfd->mdp.private1;

	if (mdp3_session->in_splash_screen) {
		mdp3_ctrl_reset(mfd);
		mdp3_session->in_splash_screen = 0;
	}

	return 0;
}

