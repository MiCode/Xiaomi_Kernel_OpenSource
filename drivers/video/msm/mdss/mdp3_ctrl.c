/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#include "mdp3_ctrl.h"
#include "mdp3.h"
#include "mdp3_ppp.h"

#define MDP_CORE_CLK_RATE	100000000
#define MDP_VSYNC_CLK_RATE	19200000
#define VSYNC_PERIOD 16

static void mdp3_ctrl_pan_display(struct msm_fb_data_type *mfd);
static int mdp3_overlay_unset(struct msm_fb_data_type *mfd, int ndx);

static void mdp3_bufq_init(struct mdp3_buffer_queue *bufq)
{
	bufq->count = 0;
	bufq->push_idx = 0;
	bufq->pop_idx = 0;
}

static void mdp3_bufq_deinit(struct mdp3_buffer_queue *bufq)
{
	int count = bufq->count;

	if (!count)
		return;

	while (count--) {
		struct mdp3_img_data *data = &bufq->img_data[bufq->pop_idx];
		bufq->pop_idx = (bufq->pop_idx + 1) % MDP3_MAX_BUF_QUEUE;
		mdp3_put_img(data);
	}
	bufq->count = 0;
	bufq->push_idx = 0;
	bufq->pop_idx = 0;
}

static int mdp3_bufq_push(struct mdp3_buffer_queue *bufq,
			struct mdp3_img_data *data)
{
	if (bufq->count >= MDP3_MAX_BUF_QUEUE) {
		pr_err("bufq full\n");
		return -EPERM;
	}

	bufq->img_data[bufq->push_idx] = *data;
	bufq->push_idx = (bufq->push_idx + 1) % MDP3_MAX_BUF_QUEUE;
	bufq->count++;
	return 0;
}

static struct mdp3_img_data *mdp3_bufq_pop(struct mdp3_buffer_queue *bufq)
{
	struct mdp3_img_data *data;
	if (bufq->count == 0)
		return NULL;

	data = &bufq->img_data[bufq->pop_idx];
	bufq->count--;
	bufq->pop_idx = (bufq->pop_idx + 1) % MDP3_MAX_BUF_QUEUE;
	return data;
}

static int mdp3_bufq_count(struct mdp3_buffer_queue *bufq)
{
	return bufq->count;
}

void vsync_notify_handler(void *arg)
{
	struct mdp3_session_data *session = (struct mdp3_session_data *)arg;
	spin_lock(&session->vsync_lock);
	session->vsync_time = ktime_get();
	complete(&session->vsync_comp);
	spin_unlock(&session->vsync_lock);
}

static int mdp3_ctrl_vsync_enable(struct msm_fb_data_type *mfd, int enable)
{
	struct mdp3_session_data *mdp3_session;
	struct mdp3_vsync_notification vsync_client;
	struct mdp3_vsync_notification *arg = NULL;
	unsigned long flag;

	pr_debug("mdp3_ctrl_vsync_enable =%d\n", enable);
	mdp3_session = (struct mdp3_session_data *)mfd->mdp.private1;
	if (!mdp3_session || !mdp3_session->panel || !mdp3_session->dma ||
		!mdp3_session->intf)
		return -ENODEV;

	if (!mdp3_session->status) {
		pr_debug("fb%d is not on yet", mfd->index);
		return -EINVAL;
	}
	if (enable) {
		vsync_client.handler = vsync_notify_handler;
		vsync_client.arg = mdp3_session;
		arg = &vsync_client;
	}

	mutex_lock(&mdp3_session->lock);
	mdp3_session->dma->vsync_enable(mdp3_session->dma, arg);
	mutex_unlock(&mdp3_session->lock);
	spin_lock_irqsave(&mdp3_session->vsync_lock, flag);
	if (enable)
		INIT_COMPLETION(mdp3_session->vsync_comp);
	else
		complete_all(&mdp3_session->vsync_comp);
	spin_unlock_irqrestore(&mdp3_session->vsync_lock, flag);
	return 0;
}

static int mdp3_ctrl_blit_req(struct msm_fb_data_type *mfd, void __user *p)
{
	const int MAX_LIST_WINDOW = 16;
	struct mdp_blit_req req_list[MAX_LIST_WINDOW];
	struct mdp_blit_req_list req_list_header;
	int rc, count, i, req_list_count;

	if (copy_from_user(&req_list_header, p, sizeof(req_list_header)))
		return -EFAULT;
	p += sizeof(req_list_header);
	count = req_list_header.count;
	if (count < 0 || count >= MAX_BLIT_REQ)
		return -EINVAL;
	while (count > 0) {
		/*
		 * Access the requests through a narrow window to decrease copy
		 * overhead and make larger requests accessible to the
		 * coherency management code.
		 * NOTE: The window size is intended to be larger than the
		 *       typical request size, but not require more than 2
		 *       kbytes of stack storage.
		 */
		req_list_count = count;
		if (req_list_count > MAX_LIST_WINDOW)
			req_list_count = MAX_LIST_WINDOW;
		if (copy_from_user(&req_list, p,
				sizeof(struct mdp_blit_req)*req_list_count))
			return -EFAULT;
		/*
		 * Do the blit DMA, if required -- returning early only if
		 * there is a failure.
		 */
		for (i = 0; i < req_list_count; i++) {
			if (!(req_list[i].flags & MDP_NO_BLIT)) {
				/* Do the actual blit. */
				rc = mdp3_ppp_start_blit(mfd, &(req_list[i]));
				if (rc)
					return rc;
			}
		}

		/* Go to next window of requests. */
		count -= req_list_count;
		p += sizeof(struct mdp_blit_req)*req_list_count;
	}
	return 0;
}

static ssize_t mdp3_vsync_show_event(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdp3_session_data *mdp3_session = NULL;
	u64 vsync_ticks;
	int rc;
	unsigned long flag;

	if (!mfd || !mfd->mdp.private1)
		return 0;

	mdp3_session = (struct mdp3_session_data *)mfd->mdp.private1;

	rc = wait_for_completion_interruptible(&mdp3_session->vsync_comp);
	if (rc < 0)
		return rc;

	spin_lock_irqsave(&mdp3_session->vsync_lock, flag);
	vsync_ticks = ktime_to_ns(mdp3_session->vsync_time);
	spin_unlock_irqrestore(&mdp3_session->vsync_lock, flag);

	pr_debug("fb%d vsync=%llu", mfd->index, vsync_ticks);
	rc = snprintf(buf, PAGE_SIZE, "VSYNC=%llu", vsync_ticks);
	return rc;
}

static DEVICE_ATTR(vsync_event, S_IRUGO, mdp3_vsync_show_event, NULL);

static struct attribute *vsync_fs_attrs[] = {
	&dev_attr_vsync_event.attr,
	NULL,
};

static struct attribute_group vsync_fs_attr_group = {
	.attrs = vsync_fs_attrs,
};

static int mdp3_ctrl_res_req_bus(struct msm_fb_data_type *mfd, int status)
{
	int rc = 0;
	if (status) {
		struct mdss_panel_info *panel_info = mfd->panel_info;
		int ab = 0;
		int ib = 0;
		ab = panel_info->xres * panel_info->yres * 4;
		ab *= panel_info->mipi.frame_rate;
		ib = (ab * 3) / 2;
		rc = mdp3_bus_scale_set_quota(MDP3_CLIENT_DMA_P, ab, ib);
	} else {
		rc = mdp3_bus_scale_set_quota(MDP3_CLIENT_DMA_P, 0, 0);
	}
	return rc;
}

static int mdp3_ctrl_res_req_clk(struct msm_fb_data_type *mfd, int status)
{
	int rc = 0;
	if (status) {

		mdp3_clk_set_rate(MDP3_CLK_CORE, MDP_CORE_CLK_RATE);
		mdp3_clk_set_rate(MDP3_CLK_VSYNC, MDP_VSYNC_CLK_RATE);

		rc = mdp3_clk_enable(true);
		if (rc)
			return rc;

	} else {
		rc = mdp3_clk_enable(false);
	}
	return rc;
}

static int mdp3_ctrl_get_intf_type(struct msm_fb_data_type *mfd)
{
	int type;
	switch (mfd->panel.type) {
	case MIPI_VIDEO_PANEL:
		type = MDP3_DMA_OUTPUT_SEL_DSI_VIDEO;
		break;
	case MIPI_CMD_PANEL:
		type = MDP3_DMA_OUTPUT_SEL_DSI_CMD;
		break;
	case LCDC_PANEL:
		type = MDP3_DMA_OUTPUT_SEL_LCDC;
		break;
	default:
		type = MDP3_DMA_OUTPUT_SEL_MAX;
	}
	return type;
}

static int mdp3_ctrl_get_source_format(struct msm_fb_data_type *mfd)
{
	int format;
	switch (mfd->fb_imgType) {
	case MDP_RGB_565:
		format = MDP3_DMA_IBUF_FORMAT_RGB565;
		break;
	case MDP_RGB_888:
		format = MDP3_DMA_IBUF_FORMAT_RGB888;
		break;
	case MDP_ARGB_8888:
	case MDP_RGBA_8888:
		format = MDP3_DMA_IBUF_FORMAT_XRGB8888;
		break;
	default:
		format = MDP3_DMA_IBUF_FORMAT_UNDEFINED;
	}
	return format;
}

static int mdp3_ctrl_get_pack_pattern(struct msm_fb_data_type *mfd)
{
	int packPattern = MDP3_DMA_OUTPUT_PACK_PATTERN_RGB;
	if (mfd->fb_imgType == MDP_RGBA_8888)
		packPattern = MDP3_DMA_OUTPUT_PACK_PATTERN_BGR;
	return packPattern;
}

static int mdp3_ctrl_intf_init(struct msm_fb_data_type *mfd,
				struct mdp3_intf *intf)
{
	int rc;
	struct mdp3_intf_cfg cfg;
	struct mdp3_video_intf_cfg *video = &cfg.video;
	struct mdss_panel_info *p = mfd->panel_info;
	int h_back_porch = p->lcdc.h_back_porch;
	int h_front_porch = p->lcdc.h_front_porch;
	int w = p->xres;
	int v_back_porch = p->lcdc.v_back_porch;
	int v_front_porch = p->lcdc.v_front_porch;
	int h = p->yres;
	int h_sync_skew = p->lcdc.hsync_skew;
	int h_pulse_width = p->lcdc.h_pulse_width;
	int v_pulse_width = p->lcdc.v_pulse_width;
	int hsync_period = h_front_porch + h_back_porch + w + h_pulse_width;
	int vsync_period = v_front_porch + v_back_porch + h + v_pulse_width;
	vsync_period *= hsync_period;

	cfg.type = mdp3_ctrl_get_intf_type(mfd);
	if (cfg.type == MDP3_DMA_OUTPUT_SEL_DSI_VIDEO ||
		cfg.type == MDP3_DMA_OUTPUT_SEL_LCDC) {
		video->hsync_period = hsync_period;
		video->hsync_pulse_width = h_pulse_width;
		video->vsync_period = vsync_period;
		video->vsync_pulse_width = v_pulse_width * hsync_period;
		video->display_start_x = h_back_porch + h_pulse_width;
		video->display_end_x = hsync_period - h_front_porch - 1;
		video->display_start_y =
			(v_back_porch + v_pulse_width) * hsync_period;
		video->display_end_y =
			vsync_period - v_front_porch * hsync_period - 1;
		video->active_start_x = video->display_start_x;
		video->active_end_x = video->display_end_x;
		video->active_h_enable = true;
		video->active_start_y = video->display_start_y;
		video->active_end_y = video->display_end_y;
		video->active_v_enable = true;
		video->hsync_skew = h_sync_skew;
		video->hsync_polarity = 1;
		video->vsync_polarity = 1;
		video->de_polarity = 1;
	} else if (cfg.type == MDP3_DMA_OUTPUT_SEL_DSI_CMD) {
		cfg.dsi_cmd.primary_dsi_cmd_id = 0;
		cfg.dsi_cmd.secondary_dsi_cmd_id = 1;
		cfg.dsi_cmd.dsi_cmd_tg_intf_sel = 0;
	} else
		return -EINVAL;
	rc = mdp3_intf_init(intf, &cfg);
	return rc;
}

static int mdp3_ctrl_dma_init(struct msm_fb_data_type *mfd,
				struct mdp3_dma *dma)
{
	int rc;
	struct mdss_panel_info *panel_info = mfd->panel_info;
	struct fb_info *fbi = mfd->fbi;
	struct fb_fix_screeninfo *fix;
	struct fb_var_screeninfo *var;
	struct mdp3_dma_output_config outputConfig;
	struct mdp3_dma_source sourceConfig;

	fix = &fbi->fix;
	var = &fbi->var;

	sourceConfig.format = mdp3_ctrl_get_source_format(mfd);
	sourceConfig.width = panel_info->xres;
	sourceConfig.height = panel_info->yres;
	sourceConfig.x = 0;
	sourceConfig.y = 0;
	sourceConfig.stride = fix->line_length;
	sourceConfig.buf = (void *)mfd->iova;

	outputConfig.dither_en = 0;
	outputConfig.out_sel = mdp3_ctrl_get_intf_type(mfd);
	outputConfig.bit_mask_polarity = 0;
	outputConfig.color_components_flip = 0;
	outputConfig.pack_pattern = mdp3_ctrl_get_pack_pattern(mfd);
	outputConfig.pack_align = MDP3_DMA_OUTPUT_PACK_ALIGN_LSB;
	outputConfig.color_comp_out_bits = (MDP3_DMA_OUTPUT_COMP_BITS_8 << 4) |
					(MDP3_DMA_OUTPUT_COMP_BITS_8 << 2)|
					MDP3_DMA_OUTPUT_COMP_BITS_8;

	rc = mdp3_dma_init(dma, &sourceConfig, &outputConfig);
	return rc;
}

static int mdp3_ctrl_on(struct msm_fb_data_type *mfd)
{
	int rc = 0;
	struct mdp3_session_data *mdp3_session;
	struct mdss_panel_data *panel;

	pr_debug("mdp3_ctrl_on\n");
	mdp3_session = (struct mdp3_session_data *)mfd->mdp.private1;
	if (!mdp3_session || !mdp3_session->panel || !mdp3_session->dma ||
		!mdp3_session->intf) {
		pr_err("mdp3_ctrl_on no device");
		return -ENODEV;
	}
	mutex_lock(&mdp3_session->lock);
	if (mdp3_session->status) {
		pr_debug("fb%d is on already", mfd->index);
		goto on_error;
	}

	/* request bus bandwidth before DSI DMA traffic */
	rc = mdp3_ctrl_res_req_bus(mfd, 1);
	if (rc)
		pr_err("fail to request bus resource\n");

	panel = mdp3_session->panel;
	if (panel->event_handler)
		rc = panel->event_handler(panel, MDSS_EVENT_PANEL_ON, NULL);
	if (rc) {
		pr_err("fail to turn on the panel\n");
		goto on_error;
	}
	rc = mdp3_ctrl_res_req_clk(mfd, 1);
	if (rc) {
		pr_err("fail to request mdp clk resource\n");
		goto on_error;
	}

	rc = mdp3_ctrl_dma_init(mfd, mdp3_session->dma);
	if (rc) {
		pr_err("dma init failed\n");
		goto on_error;
	}

	rc = mdp3_ppp_init();
	if (rc) {
		pr_err("ppp init failed\n");
		goto on_error;
	}

	rc = mdp3_ctrl_intf_init(mfd, mdp3_session->intf);
	if (rc) {
		pr_err("display interface init failed\n");



		goto on_error;
	}

	rc = mdp3_session->dma->start(mdp3_session->dma, mdp3_session->intf);
	if (rc) {
		pr_err("fail to start the MDP display interface\n");
		goto on_error;
	}

on_error:
	if (!rc)
		mdp3_session->status = 1;

	mutex_unlock(&mdp3_session->lock);
	return rc;
}

static int mdp3_ctrl_off(struct msm_fb_data_type *mfd)
{
	int rc = 0;
	struct mdp3_session_data *mdp3_session;
	struct mdss_panel_data *panel;

	pr_debug("mdp3_ctrl_off\n");
	mdp3_session = (struct mdp3_session_data *)mfd->mdp.private1;
	if (!mdp3_session || !mdp3_session->panel || !mdp3_session->dma ||
		!mdp3_session->intf) {
		pr_err("mdp3_ctrl_on no device");
		return -ENODEV;
	}

	mutex_lock(&mdp3_session->lock);

	if (!mdp3_session->status) {
		pr_debug("fb%d is off already", mfd->index);
		goto off_error;
	}

	pr_debug("mdp3_ctrl_off stop mdp3 dma engine\n");

	rc = mdp3_session->dma->stop(mdp3_session->dma, mdp3_session->intf);

	if (rc)
		pr_err("fail to stop the MDP3 dma\n");

	pr_debug("mdp3_ctrl_off stop dsi panel and controller\n");
	panel = mdp3_session->panel;
	if (panel->event_handler)
		rc = panel->event_handler(panel, MDSS_EVENT_PANEL_OFF, NULL);
	if (rc)
		pr_err("fail to turn off the panel\n");

	pr_debug("mdp3_ctrl_off release bus and clock\n");
	rc = mdp3_ctrl_res_req_bus(mfd, 0);
	if (rc)
		pr_err("mdp bus resource release failed\n");
	rc = mdp3_ctrl_res_req_clk(mfd, 0);
	if (rc)
		pr_err("mdp clock resource release failed\n");
off_error:
	mdp3_session->status = 0;
	mutex_unlock(&mdp3_session->lock);
	if (mdp3_session->overlay.id != MSMFB_NEW_REQUEST)
		mdp3_overlay_unset(mfd, mdp3_session->overlay.id);
	return 0;
}

static int mdp3_overlay_get(struct msm_fb_data_type *mfd,
				struct mdp_overlay *req)
{
	int rc = 0;
	struct mdp3_session_data *mdp3_session = mfd->mdp.private1;

	mutex_lock(&mdp3_session->lock);

	if (mdp3_session->overlay.id == req->id)
		*req = mdp3_session->overlay;
	else
		rc = -EINVAL;

	mutex_unlock(&mdp3_session->lock);

	return rc;
}

static int mdp3_overlay_set(struct msm_fb_data_type *mfd,
				struct mdp_overlay *req)
{
	int rc = 0;
	struct mdp3_session_data *mdp3_session = mfd->mdp.private1;

	mutex_lock(&mdp3_session->lock);

	if (mdp3_session->overlay.id == req->id) {
		mdp3_session->overlay = *req;
		if (req->id == MSMFB_NEW_REQUEST) {
			mdp3_session->overlay.id = 1;
			req->id = 1;
		}
	} else {
		rc = -EINVAL;
	}
	mutex_unlock(&mdp3_session->lock);

	return rc;
}

static int mdp3_overlay_unset(struct msm_fb_data_type *mfd, int ndx)
{
	int rc = 0;
	struct mdp3_session_data *mdp3_session = mfd->mdp.private1;

	mdp3_ctrl_pan_display(mfd);

	mutex_lock(&mdp3_session->lock);

	if (mdp3_session->overlay.id == ndx && ndx == 1) {
		mdp3_session->overlay.id = MSMFB_NEW_REQUEST;
		mdp3_bufq_deinit(&mdp3_session->bufq_in);
		mdp3_bufq_deinit(&mdp3_session->bufq_out);
	} else {
		rc = -EINVAL;
	}

	mutex_unlock(&mdp3_session->lock);

	return rc;
}

static int mdp3_overlay_queue_buffer(struct msm_fb_data_type *mfd,
					struct msmfb_overlay_data *req)
{
	int rc;
	struct mdp3_session_data *mdp3_session = mfd->mdp.private1;
	struct msmfb_data *img = &req->data;
	struct mdp3_img_data data;

	rc = mdp3_get_img(img, &data);
	if (rc) {
		pr_err("fail to get overlay buffer\n");
		return rc;
	}

	rc = mdp3_bufq_push(&mdp3_session->bufq_in, &data);
	if (rc) {
		pr_err("fail to queue the overlay buffer, buffer drop\n");
		mdp3_put_img(&data);
		return rc;
	}
	return 0;
}

static int mdp3_overlay_play(struct msm_fb_data_type *mfd,
				 struct msmfb_overlay_data *req)
{
	struct mdp3_session_data *mdp3_session = mfd->mdp.private1;
	int rc = 0;

	pr_debug("mdp3_overlay_play req id=%x mem_id=%d\n",
		req->id, req->data.memory_id);

	mutex_lock(&mdp3_session->lock);

	if (mfd->panel_power_on)
		rc = mdp3_overlay_queue_buffer(mfd, req);
	else
		rc = -EPERM;

	mutex_unlock(&mdp3_session->lock);

	return rc;
}

static int mdp3_ctrl_display_commit_kickoff(struct msm_fb_data_type *mfd)
{
	struct mdp3_session_data *mdp3_session;
	struct mdp3_img_data *data;
	int rc = 0;

	if (!mfd || !mfd->mdp.private1)
		return -EINVAL;

	mdp3_session = mfd->mdp.private1;
	if (!mdp3_session || !mdp3_session->dma)
		return -EINVAL;

	if (!mdp3_session->status) {
		pr_err("%s, display off!\n", __func__);
		return -EPERM;
	}

	mutex_lock(&mdp3_session->lock);

	data = mdp3_bufq_pop(&mdp3_session->bufq_in);
	if (data) {
		mdp3_session->dma->update(mdp3_session->dma,
			(void *)data->addr);
		mdp3_bufq_push(&mdp3_session->bufq_out, data);
	}

	if (mdp3_bufq_count(&mdp3_session->bufq_out) > 1) {
		data = mdp3_bufq_pop(&mdp3_session->bufq_out);
		mdp3_put_img(data);
	}

	mutex_unlock(&mdp3_session->lock);
	return rc;
}

static void mdp3_ctrl_pan_display(struct msm_fb_data_type *mfd)
{
	struct fb_info *fbi;
	struct mdp3_session_data *mdp3_session;
	u32 offset;
	int bpp;

	pr_debug("mdp3_ctrl_pan_display\n");
	if (!mfd || !mfd->mdp.private1)
		return;

	mdp3_session = (struct mdp3_session_data *)mfd->mdp.private1;
	if (!mdp3_session || !mdp3_session->dma)
		return;

	if (!mdp3_session->status) {
		pr_err("mdp3_ctrl_pan_display, display off!\n");
		return;
	}

	mutex_lock(&mdp3_session->lock);
	fbi = mfd->fbi;

	bpp = fbi->var.bits_per_pixel / 8;
	offset = fbi->var.xoffset * bpp +
		 fbi->var.yoffset * fbi->fix.line_length;

	if (offset > fbi->fix.smem_len) {
		pr_err("invalid fb offset=%u total length=%u\n",
			offset, fbi->fix.smem_len);
		goto pan_error;
	}

	mdp3_session->dma->update(mdp3_session->dma,
				(void *)mfd->iova + offset);
pan_error:
	mutex_unlock(&mdp3_session->lock);
}

static int mdp3_get_metadata(struct msm_fb_data_type *mfd,
				struct msmfb_metadata *metadata)
{
	int ret = 0;
	switch (metadata->op) {
	case metadata_op_frame_rate:
		metadata->data.panel_frame_rate =
			mfd->panel_info->mipi.frame_rate;
		break;
	case metadata_op_get_caps:
		metadata->data.caps.mdp_rev = 304;
		metadata->data.caps.rgb_pipes = 0;
		metadata->data.caps.vig_pipes = 0;
		metadata->data.caps.dma_pipes = 1;
		break;
	default:
		pr_warn("Unsupported request to MDP META IOCTL.\n");
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int mdp3_ctrl_ioctl_handler(struct msm_fb_data_type *mfd,
					u32 cmd, void __user *argp)
{
	int rc = -EINVAL;
	struct mdp3_session_data *mdp3_session;
	struct msmfb_metadata metadata;
	struct mdp_overlay req;
	struct msmfb_overlay_data ov_data;
	int val;

	pr_debug("mdp3_ctrl_ioctl_handler\n");

	mdp3_session = (struct mdp3_session_data *)mfd->mdp.private1;
	if (!mdp3_session)
		return -ENODEV;

	if (!mdp3_session->status) {
		pr_err("mdp3_ctrl_ioctl_handler, display off!\n");
		return -EINVAL;
	}

	switch (cmd) {
	case MSMFB_VSYNC_CTRL:
	case MSMFB_OVERLAY_VSYNC_CTRL:
		if (!copy_from_user(&val, argp, sizeof(val))) {
			rc = mdp3_ctrl_vsync_enable(mfd, val);
		} else {
			pr_err("MSMFB_OVERLAY_VSYNC_CTRL failed\n");
			rc = -EFAULT;
		}
		break;
	case MSMFB_BLIT:
		rc = mdp3_ctrl_blit_req(mfd, argp);
		break;
	case MSMFB_METADATA_GET:
		rc = copy_from_user(&metadata, argp, sizeof(metadata));
		if (rc)
			return rc;
		rc = mdp3_get_metadata(mfd, &metadata);
		if (!rc)
			rc = copy_to_user(argp, &metadata, sizeof(metadata));
		break;
	case MSMFB_OVERLAY_GET:
		rc = copy_from_user(&req, argp, sizeof(req));
		if (!rc) {
			rc = mdp3_overlay_get(mfd, &req);

		if (!IS_ERR_VALUE(rc))
			rc = copy_to_user(argp, &req, sizeof(req));
		}
		if (rc)
			pr_err("OVERLAY_GET failed (%d)\n", rc);
		break;
	case MSMFB_OVERLAY_SET:
		rc = copy_from_user(&req, argp, sizeof(req));
		if (!rc) {
			rc = mdp3_overlay_set(mfd, &req);

		if (!IS_ERR_VALUE(rc))
			rc = copy_to_user(argp, &req, sizeof(req));
		}
		if (rc)
			pr_err("OVERLAY_SET failed (%d)\n", rc);
		break;
	case MSMFB_OVERLAY_UNSET:
		if (!IS_ERR_VALUE(copy_from_user(&val, argp, sizeof(val))))
			rc = mdp3_overlay_unset(mfd, val);
		break;
	case MSMFB_OVERLAY_PLAY:
		rc = copy_from_user(&ov_data, argp, sizeof(ov_data));
		if (!rc)
			rc = mdp3_overlay_play(mfd, &ov_data);
		if (rc)
			pr_err("OVERLAY_PLAY failed (%d)\n", rc);
		break;
	default:
		break;
	}
	return rc;
}

int mdp3_ctrl_init(struct msm_fb_data_type *mfd)
{
	struct device *dev = mfd->fbi->dev;
	struct msm_mdp_interface *mdp3_interface = &mfd->mdp;
	struct mdp3_session_data *mdp3_session = NULL;
	u32 intf_type = MDP3_DMA_OUTPUT_SEL_DSI_VIDEO;
	int rc;

	pr_debug("mdp3_ctrl_init\n");
	mdp3_interface->on_fnc = mdp3_ctrl_on;
	mdp3_interface->off_fnc = mdp3_ctrl_off;
	mdp3_interface->do_histogram = NULL;
	mdp3_interface->cursor_update = NULL;
	mdp3_interface->dma_fnc = mdp3_ctrl_pan_display;
	mdp3_interface->ioctl_handler = mdp3_ctrl_ioctl_handler;
	mdp3_interface->kickoff_fnc = mdp3_ctrl_display_commit_kickoff;

	mdp3_session = kmalloc(sizeof(struct mdp3_session_data), GFP_KERNEL);
	if (!mdp3_session) {
		pr_err("fail to allocate mdp3 private data structure");
		return -ENOMEM;
	}
	memset(mdp3_session, 0, sizeof(struct mdp3_session_data));
	mutex_init(&mdp3_session->lock);
	init_completion(&mdp3_session->vsync_comp);
	spin_lock_init(&mdp3_session->vsync_lock);
	mdp3_session->dma = mdp3_get_dma_pipe(MDP3_DMA_CAP_ALL);
	if (!mdp3_session->dma) {
		rc = -ENODEV;
		goto init_done;
	}

	intf_type = mdp3_ctrl_get_intf_type(mfd);
	mdp3_session->intf = mdp3_get_display_intf(intf_type);
	if (!mdp3_session->intf) {
		rc = -ENODEV;
		goto init_done;
	}

	mdp3_session->panel = dev_get_platdata(&mfd->pdev->dev);
	mdp3_session->status = 0;
	mdp3_session->overlay.id = MSMFB_NEW_REQUEST;
	mdp3_bufq_init(&mdp3_session->bufq_in);
	mdp3_bufq_init(&mdp3_session->bufq_out);

	mfd->mdp.private1 = mdp3_session;

	rc = sysfs_create_group(&dev->kobj, &vsync_fs_attr_group);
	if (rc) {
		pr_err("vsync sysfs group creation failed, ret=%d\n", rc);
		goto init_done;
	}

	kobject_uevent(&dev->kobj, KOBJ_ADD);
	pr_debug("vsync kobject_uevent(KOBJ_ADD)\n");

init_done:
	if (IS_ERR_VALUE(rc))
		kfree(mdp3_session);

	return rc;
}
