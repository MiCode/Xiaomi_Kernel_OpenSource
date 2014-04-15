/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
#include <linux/delay.h>

#include "mdp3_ctrl.h"
#include "mdp3.h"
#include "mdp3_ppp.h"

#define VSYNC_EXPIRE_TICK	4

static void mdp3_ctrl_pan_display(struct msm_fb_data_type *mfd,
					struct mdp_overlay *req,
					int image_size,
					int *pipe_ndx);
static int mdp3_overlay_unset(struct msm_fb_data_type *mfd, int ndx);
static int mdp3_histogram_stop(struct mdp3_session_data *session,
					u32 block);
static int mdp3_ctrl_clk_enable(struct msm_fb_data_type *mfd, int enable);
static int mdp3_ctrl_vsync_enable(struct msm_fb_data_type *mfd, int enable);
static int mdp3_ctrl_get_intf_type(struct msm_fb_data_type *mfd);

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
		mdp3_put_img(data, MDP3_CLIENT_DMA_P);
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

void mdp3_ctrl_notifier_register(struct mdp3_session_data *ses,
	struct notifier_block *notifier)
{
	blocking_notifier_chain_register(&ses->notifier_head, notifier);
}

void mdp3_ctrl_notifier_unregister(struct mdp3_session_data *ses,
	struct notifier_block *notifier)
{
	blocking_notifier_chain_unregister(&ses->notifier_head, notifier);
}

int mdp3_ctrl_notify(struct mdp3_session_data *ses, int event)
{
	return blocking_notifier_call_chain(&ses->notifier_head, event, ses);
}

static void mdp3_dispatch_dma_done(struct work_struct *work)
{
	struct mdp3_session_data *session;

	pr_debug("%s\n", __func__);
	session = container_of(work, struct mdp3_session_data,
				dma_done_work);
	if (!session)
		return;

	mdp3_ctrl_notify(session, MDP_NOTIFY_FRAME_DONE);
}

static void mdp3_dispatch_clk_off(struct work_struct *work)
{
	struct mdp3_session_data *session;

	pr_debug("%s\n", __func__);
	session = container_of(work, struct mdp3_session_data,
				clk_off_work);
	if (!session)
		return;

	mutex_lock(&session->lock);
	if (session->vsync_enabled ||
			atomic_read(&session->vsync_countdown) != 0) {
		mutex_unlock(&session->lock);
		pr_debug("Ignoring clk shut down\n");
		return;
	}

	mdp3_ctrl_vsync_enable(session->mfd, 0);
	mdp3_ctrl_clk_enable(session->mfd, 0);
	mutex_unlock(&session->lock);
}

void vsync_notify_handler(void *arg)
{
	struct mdp3_session_data *session = (struct mdp3_session_data *)arg;
	session->vsync_time = ktime_get();
	sysfs_notify_dirent(session->vsync_event_sd);
}

void dma_done_notify_handler(void *arg)
{
	struct mdp3_session_data *session = (struct mdp3_session_data *)arg;
	schedule_work(&session->dma_done_work);
	complete(&session->dma_completion);
}

void vsync_count_down(void *arg)
{
	struct mdp3_session_data *session = (struct mdp3_session_data *)arg;
	/* We are counting down to turn off clocks */
	atomic_dec(&session->vsync_countdown);
	if (atomic_read(&session->vsync_countdown) == 0)
		schedule_work(&session->clk_off_work);
}

void mdp3_ctrl_reset_countdown(struct mdp3_session_data *session,
		struct msm_fb_data_type *mfd)
{
	if (mdp3_ctrl_get_intf_type(mfd) == MDP3_DMA_OUTPUT_SEL_DSI_CMD)
		atomic_set(&session->vsync_countdown, VSYNC_EXPIRE_TICK);
}

static int mdp3_ctrl_vsync_enable(struct msm_fb_data_type *mfd, int enable)
{
	struct mdp3_session_data *mdp3_session;
	struct mdp3_notification vsync_client;
	struct mdp3_notification *arg = NULL;

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
	} else if (atomic_read(&mdp3_session->vsync_countdown)) {
		/*
		 * Now that vsync is no longer needed we will
		 * shutdown dsi clocks as soon as cnt down == 0
		 * for cmd mode panels
		 */
		vsync_client.handler = vsync_count_down;
		vsync_client.arg = mdp3_session;
		arg = &vsync_client;
		enable = 1;
	}

	mdp3_clk_enable(1, 0);
	mdp3_session->dma->vsync_enable(mdp3_session->dma, arg);
	mdp3_clk_enable(0, 0);

	/*
	 * Need to fake vsync whenever dsi interface is not
	 * active or when dsi clocks are currently off
	 */
	if (enable && mdp3_session->status == 1
			&& (mdp3_session->vsync_before_commit ||
			!mdp3_session->intf->active)) {
		mod_timer(&mdp3_session->vsync_timer,
			jiffies + msecs_to_jiffies(mdp3_session->vsync_period));
	} else if (enable && !mdp3_session->clk_on) {
		mdp3_ctrl_reset_countdown(mdp3_session, mfd);
		mdp3_ctrl_clk_enable(mfd, 1);
	} else if (!enable) {
		del_timer(&mdp3_session->vsync_timer);
	}

	return 0;
}

void mdp3_vsync_timer_func(unsigned long arg)
{
	struct mdp3_session_data *session = (struct mdp3_session_data *)arg;
	if (session->status == 1 && (session->vsync_before_commit ||
			!session->intf->active)) {
		pr_debug("mdp3_vsync_timer_func trigger\n");
		vsync_notify_handler(session);
		mod_timer(&session->vsync_timer,
			jiffies + msecs_to_jiffies(session->vsync_period));
	}
}

static int mdp3_ctrl_async_blit_req(struct msm_fb_data_type *mfd,
	void __user *p)
{
	struct mdp_async_blit_req_list req_list_header;
	int rc, count;
	void __user *p_req;

	if (copy_from_user(&req_list_header, p, sizeof(req_list_header)))
		return -EFAULT;
	p_req = p + sizeof(req_list_header);
	count = req_list_header.count;
	if (count < 0 || count >= MAX_BLIT_REQ)
		return -EINVAL;
	rc = mdp3_ppp_parse_req(p_req, &req_list_header, 1);
	if (!rc)
		rc = copy_to_user(p, &req_list_header, sizeof(req_list_header));
	return rc;
}

static int mdp3_ctrl_blit_req(struct msm_fb_data_type *mfd, void __user *p)
{
	struct mdp_async_blit_req_list req_list_header;
	int rc, count;
	void __user *p_req;

	if (copy_from_user(&(req_list_header.count), p,
			sizeof(struct mdp_blit_req_list)))
		return -EFAULT;
	p_req = p + sizeof(struct mdp_blit_req_list);
	count = req_list_header.count;
	if (count < 0 || count >= MAX_BLIT_REQ)
		return -EINVAL;
	req_list_header.sync.acq_fen_fd_cnt = 0;
	rc = mdp3_ppp_parse_req(p_req, &req_list_header, 0);
	return rc;
}

static ssize_t mdp3_vsync_show_event(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdp3_session_data *mdp3_session = NULL;
	u64 vsync_ticks;
	int rc;

	if (!mfd || !mfd->mdp.private1)
		return -EAGAIN;

	mdp3_session = (struct mdp3_session_data *)mfd->mdp.private1;

	vsync_ticks = ktime_to_ns(mdp3_session->vsync_time);

	pr_debug("fb%d vsync=%llu", mfd->index, vsync_ticks);
	rc = scnprintf(buf, PAGE_SIZE, "VSYNC=%llu", vsync_ticks);
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

static int mdp3_ctrl_clk_enable(struct msm_fb_data_type *mfd, int enable)
{
	struct mdp3_session_data *session;
	struct mdss_panel_data *panel;
	int rc = 0;

	pr_debug("mdp3_ctrl_clk_enable %d\n", enable);

	session = mfd->mdp.private1;
	panel = session->panel;

	if (!panel->event_handler)
		return 0;

	if ((enable && session->clk_on == 0) ||
				(!enable && session->clk_on == 1)) {
		rc = panel->event_handler(panel,
			MDSS_EVENT_PANEL_CLK_CTRL, (void *)enable);
		rc |= mdp3_res_update(enable, 1, MDP3_CLIENT_DMA_P);
	} else {
		pr_debug("enable = %d, clk_on=%d\n", enable, session->clk_on);
	}

	session->clk_on = enable;
	return rc;
}

static int mdp3_ctrl_res_req_bus(struct msm_fb_data_type *mfd, int status)
{
	int rc = 0;
	if (status) {
		struct mdss_panel_info *panel_info = mfd->panel_info;
		u64 ab = 0;
		u64 ib = 0;
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

		mdp3_clk_set_rate(MDP3_CLK_CORE, MDP_CORE_CLK_RATE,
				MDP3_CLIENT_DMA_P);
		mdp3_clk_set_rate(MDP3_CLK_VSYNC, MDP_VSYNC_CLK_RATE,
				MDP3_CLIENT_DMA_P);

		rc = mdp3_res_update(1, 1, MDP3_CLIENT_DMA_P);
		if (rc) {
			pr_err("mdp3 clk enable fail\n");
			return rc;
		}
	} else {
		rc = mdp3_res_update(0, 1, MDP3_CLIENT_DMA_P);
		if (rc)
			pr_err("mdp3 clk disable fail\n");
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

static int mdp3_ctrl_get_source_format(u32 imgType)
{
	int format;
	switch (imgType) {
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

static int mdp3_ctrl_get_pack_pattern(u32 imgType)
{
	int packPattern = MDP3_DMA_OUTPUT_PACK_PATTERN_RGB;
	if (imgType == MDP_RGBA_8888)
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
		video->underflow_color = p->lcdc.underflow_clr;
	} else if (cfg.type == MDP3_DMA_OUTPUT_SEL_DSI_CMD) {
		cfg.dsi_cmd.primary_dsi_cmd_id = 0;
		cfg.dsi_cmd.secondary_dsi_cmd_id = 1;
		cfg.dsi_cmd.dsi_cmd_tg_intf_sel = 0;
	} else
		return -EINVAL;

	if (intf->config)
		rc = intf->config(intf, &cfg);
	else
		rc = -EINVAL;
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
	int frame_rate = mfd->panel_info->mipi.frame_rate;
	int vbp, vfp, vspw;
	int vtotal, vporch;
	struct mdp3_notification dma_done_callback;

	vbp = panel_info->lcdc.v_back_porch;
	vfp = panel_info->lcdc.v_front_porch;
	vspw = panel_info->lcdc.v_pulse_width;
	vporch = vbp + vfp + vspw;
	vtotal = vporch + panel_info->yres;

	fix = &fbi->fix;
	var = &fbi->var;

	sourceConfig.format = mdp3_ctrl_get_source_format(mfd->fb_imgType);
	sourceConfig.width = panel_info->xres;
	sourceConfig.height = panel_info->yres;
	sourceConfig.x = 0;
	sourceConfig.y = 0;
	sourceConfig.stride = fix->line_length;
	sourceConfig.buf = (void *)mfd->iova;
	sourceConfig.vporch = vporch;
	sourceConfig.vsync_count =
		MDP_VSYNC_CLK_RATE / (frame_rate * vtotal);

	outputConfig.dither_en = 0;
	outputConfig.out_sel = mdp3_ctrl_get_intf_type(mfd);
	outputConfig.bit_mask_polarity = 0;
	outputConfig.color_components_flip = 0;
	outputConfig.pack_pattern = mdp3_ctrl_get_pack_pattern(mfd->fb_imgType);
	outputConfig.pack_align = MDP3_DMA_OUTPUT_PACK_ALIGN_LSB;
	outputConfig.color_comp_out_bits = (MDP3_DMA_OUTPUT_COMP_BITS_8 << 4) |
					(MDP3_DMA_OUTPUT_COMP_BITS_8 << 2)|
					MDP3_DMA_OUTPUT_COMP_BITS_8;

	if (dma->dma_config)
		rc = dma->dma_config(dma, &sourceConfig, &outputConfig);
	else
		rc = -EINVAL;

	if (outputConfig.out_sel == MDP3_DMA_OUTPUT_SEL_DSI_CMD) {
		dma_done_callback.handler = dma_done_notify_handler;
		dma_done_callback.arg = mfd->mdp.private1;
		dma->dma_done_notifier(dma, &dma_done_callback);
	}

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

	if (mdp3_session->intf->active) {
		pr_debug("continuous splash screen, initialized already\n");
		goto on_error;
	}

	mdp3_batfet_ctrl(true);
	mdp3_ctrl_notifier_register(mdp3_session,
		&mdp3_session->mfd->mdp_sync_pt_data.notifier);

	/* request bus bandwidth before DSI DMA traffic */
	rc = mdp3_ctrl_res_req_bus(mfd, 1);
	if (rc) {
		pr_err("fail to request bus resource\n");
		goto on_error;
	}

	panel = mdp3_session->panel;
	if (panel->event_handler) {
		rc = panel->event_handler(panel, MDSS_EVENT_UNBLANK, NULL);
		rc |= panel->event_handler(panel, MDSS_EVENT_PANEL_ON, NULL);
	}
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
	mdp3_session->clk_on = 1;

	mdp3_session->first_commit = true;

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

	panel = mdp3_session->panel;
	mutex_lock(&mdp3_session->lock);

	if (!mdp3_session->status) {
		pr_debug("fb%d is off already", mfd->index);
		goto off_error;
	}

	mdp3_ctrl_clk_enable(mfd, 1);

	mdp3_histogram_stop(mdp3_session, MDP_BLOCK_DMA_P);

	rc = mdp3_session->dma->stop(mdp3_session->dma, mdp3_session->intf);
	if (rc)
		pr_debug("fail to stop the MDP3 dma\n");


	if (panel->event_handler)
		rc = panel->event_handler(panel, MDSS_EVENT_PANEL_OFF, NULL);
	if (rc)
		pr_err("fail to turn off the panel\n");

	mdp3_irq_deregister();

	pr_debug("mdp3_ctrl_off stop clock\n");
	if (mdp3_session->clk_on) {
		rc = mdp3_res_update(0, 1, MDP3_CLIENT_DMA_P);
		if (rc)
			pr_err("mdp clock resource release failed\n");

		pr_debug("mdp3_ctrl_off stop dsi controller\n");
		if (panel->event_handler)
			rc = panel->event_handler(panel,
				MDSS_EVENT_BLANK, NULL);
		if (rc)
			pr_err("fail to turn off the panel\n");
	}

	mdp3_ctrl_notifier_unregister(mdp3_session,
		&mdp3_session->mfd->mdp_sync_pt_data.notifier);
	mdp3_batfet_ctrl(false);
	mdp3_session->vsync_enabled = 0;
	atomic_set(&mdp3_session->vsync_countdown, 0);
	mdp3_session->clk_on = 0;
off_error:
	mdp3_session->status = 0;
	mdp3_bufq_deinit(&mdp3_session->bufq_out);
	if (mdp3_session->overlay.id != MSMFB_NEW_REQUEST) {
		mdp3_session->overlay.id = MSMFB_NEW_REQUEST;
		mdp3_bufq_deinit(&mdp3_session->bufq_in);
	}
	mutex_unlock(&mdp3_session->lock);
	return 0;
}

static int mdp3_ctrl_reset_cmd(struct msm_fb_data_type *mfd)
{
	int rc = 0;
	struct mdp3_session_data *mdp3_session;
	struct mdp3_dma *mdp3_dma;
	struct mdss_panel_data *panel;
	struct mdp3_notification vsync_client;

	pr_debug("mdp3_ctrl_reset_cmd\n");
	mdp3_session = (struct mdp3_session_data *)mfd->mdp.private1;
	if (!mdp3_session || !mdp3_session->panel || !mdp3_session->dma ||
		!mdp3_session->intf) {
		pr_err("mdp3_ctrl_reset no device");
		return -ENODEV;
	}

	panel = mdp3_session->panel;
	mdp3_dma = mdp3_session->dma;
	mutex_lock(&mdp3_session->lock);

	vsync_client = mdp3_dma->vsync_client;

	rc = mdp3_dma->stop(mdp3_dma, mdp3_session->intf);
	if (rc) {
		pr_err("fail to stop the MDP3 dma\n");
		goto reset_error;
	}

	rc = mdp3_iommu_enable(MDP3_CLIENT_DMA_P);
	if (rc) {
		pr_err("fail to attach dma iommu\n");
		goto reset_error;
	}

	mdp3_ctrl_intf_init(mfd, mdp3_session->intf);
	mdp3_ctrl_dma_init(mfd, mdp3_dma);

	if (vsync_client.handler)
		mdp3_dma->vsync_enable(mdp3_dma, &vsync_client);

	mdp3_session->first_commit = true;

reset_error:
	mutex_unlock(&mdp3_session->lock);
	return rc;
}

static int mdp3_ctrl_reset(struct msm_fb_data_type *mfd)
{
	int rc = 0;
	struct mdp3_session_data *mdp3_session;
	struct mdp3_dma *mdp3_dma;
	struct mdss_panel_data *panel;
	struct mdp3_notification vsync_client;

	pr_debug("mdp3_ctrl_reset\n");
	mdp3_session = (struct mdp3_session_data *)mfd->mdp.private1;
	if (!mdp3_session || !mdp3_session->panel || !mdp3_session->dma ||
		!mdp3_session->intf) {
		pr_err("mdp3_ctrl_reset no device");
		return -ENODEV;
	}

	if (mfd->panel.type == MIPI_CMD_PANEL) {
		rc = mdp3_ctrl_reset_cmd(mfd);
		return rc;
	}

	panel = mdp3_session->panel;
	mdp3_dma = mdp3_session->dma;
	mutex_lock(&mdp3_session->lock);

	vsync_client = mdp3_dma->vsync_client;
	if (panel && panel->set_backlight)
		panel->set_backlight(panel, 0);

	rc = panel->event_handler(panel, MDSS_EVENT_PANEL_OFF, NULL);
	if (rc)
		pr_err("fail to turn off panel\n");

	rc = mdp3_dma->stop(mdp3_dma, mdp3_session->intf);
	if (rc) {
		pr_err("fail to stop the MDP3 dma\n");
		goto reset_error;
	}

	rc = mdp3_put_mdp_dsi_clk();
	if (rc) {
		pr_err("fail to release mdp clocks\n");
		goto reset_error;
	}

	rc = panel->event_handler(panel, MDSS_EVENT_BLANK, NULL);
	if (rc) {
		pr_err("fail to blank the panel\n");
		goto reset_error;
	}

	rc = mdp3_iommu_enable(MDP3_CLIENT_DMA_P);
	if (rc) {
		pr_err("fail to attach dma iommu\n");
		goto reset_error;
	}

	rc = panel->event_handler(panel, MDSS_EVENT_UNBLANK, NULL);
	if (rc) {
		pr_err("fail to unblank the panel\n");
		goto reset_error;
	}

	rc = panel->event_handler(panel, MDSS_EVENT_PANEL_ON, NULL);
	if (rc) {
		pr_err("fail to turn on the panel\n");
		goto reset_error;
	}

	rc = mdp3_get_mdp_dsi_clk();
	if (rc) {
		pr_err("fail to turn on mdp clks\n");
		goto reset_error;
	}

	mdp3_ctrl_intf_init(mfd, mdp3_session->intf);
	mdp3_ctrl_dma_init(mfd, mdp3_dma);

	if (vsync_client.handler)
		mdp3_dma->vsync_enable(mdp3_dma, &vsync_client);

	mdp3_session->first_commit = true;

reset_error:
	mutex_unlock(&mdp3_session->lock);
	return rc;
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
	struct mdp3_dma *dma = mdp3_session->dma;
	struct fb_fix_screeninfo *fix;
	struct fb_info *fbi = mfd->fbi;
	int stride;
	int format;

	fix = &fbi->fix;
	stride = req->src.width * ppp_bpp(req->src.format);
	format = mdp3_ctrl_get_source_format(req->src.format);

	mutex_lock(&mdp3_session->lock);

	if (mdp3_session->overlay.id != req->id)
		pr_err("overlay was not released, continue to recover\n");

	mdp3_session->overlay = *req;
	if (req->id == MSMFB_NEW_REQUEST) {
		if (dma->source_config.stride != stride ||
				dma->source_config.format != format) {
			dma->source_config.format = format;
			dma->source_config.stride = stride;
			dma->output_config.pack_pattern =
				mdp3_ctrl_get_pack_pattern(req->src.format);
			dma->update_src_cfg = true;
		}
		mdp3_session->overlay.id = 1;
		req->id = 1;
	}

	mutex_unlock(&mdp3_session->lock);

	return rc;
}

static int mdp3_overlay_unset(struct msm_fb_data_type *mfd, int ndx)
{
	int rc = 0;
	struct mdp3_session_data *mdp3_session = mfd->mdp.private1;
	struct fb_info *fbi = mfd->fbi;
	struct fb_fix_screeninfo *fix;
	int format;

	fix = &fbi->fix;
	format = mdp3_ctrl_get_source_format(mfd->fb_imgType);
	mutex_lock(&mdp3_session->lock);

	if (mdp3_session->overlay.id == ndx && ndx == 1) {
		mdp3_session->overlay.id = MSMFB_NEW_REQUEST;
		mdp3_bufq_deinit(&mdp3_session->bufq_in);
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

	rc = mdp3_get_img(img, &data, MDP3_CLIENT_DMA_P);
	if (rc) {
		pr_err("fail to get overlay buffer\n");
		return rc;
	}

	rc = mdp3_bufq_push(&mdp3_session->bufq_in, &data);
	if (rc) {
		pr_err("fail to queue the overlay buffer, buffer drop\n");
		mdp3_put_img(&data, MDP3_CLIENT_DMA_P);
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

static int mdp3_ctrl_display_commit_kickoff(struct msm_fb_data_type *mfd,
					struct mdp_display_commit *cmt_data)
{
	struct mdp3_session_data *mdp3_session;
	struct mdp3_img_data *data;
	struct mdss_panel_info *panel_info = mfd->panel_info;
	int rc = 0;
	bool reset_done = false;
	struct mdss_panel_data *panel;

	if (!mfd || !mfd->mdp.private1)
		return -EINVAL;

	mdp3_session = mfd->mdp.private1;
	if (!mdp3_session || !mdp3_session->dma)
		return -EINVAL;

	if (mdp3_bufq_count(&mdp3_session->bufq_in) == 0) {
		pr_debug("no buffer in queue yet\n");
		return -EPERM;
	}

	panel = mdp3_session->panel;
	if (!mdp3_iommu_is_attached(MDP3_CLIENT_DMA_P)) {
		pr_debug("continuous splash screen, IOMMU not attached\n");
		mdp3_ctrl_reset(mfd);
		reset_done = true;
	}

	mutex_lock(&mdp3_session->lock);

	if (!mdp3_session->status) {
		pr_err("%s, display off!\n", __func__);
		mutex_unlock(&mdp3_session->lock);
		return -EPERM;
	}

	mdp3_ctrl_notify(mdp3_session, MDP_NOTIFY_FRAME_BEGIN);
	data = mdp3_bufq_pop(&mdp3_session->bufq_in);
	if (data) {
		mdp3_ctrl_reset_countdown(mdp3_session, mfd);
		mdp3_ctrl_clk_enable(mfd, 1);
		rc = mdp3_session->dma->update(mdp3_session->dma,
			(void *)data->addr,
			mdp3_session->intf);
		/* This is for the previous frame */
		if (rc < 0) {
			mdp3_ctrl_notify(mdp3_session,
				MDP_NOTIFY_FRAME_TIMEOUT);
		} else {
			if (mdp3_ctrl_get_intf_type(mfd) ==
						MDP3_DMA_OUTPUT_SEL_DSI_VIDEO) {
				mdp3_ctrl_notify(mdp3_session,
					MDP_NOTIFY_FRAME_DONE);
			}
		}
		mdp3_session->dma_active = 1;
		init_completion(&mdp3_session->dma_completion);
		mdp3_ctrl_notify(mdp3_session, MDP_NOTIFY_FRAME_FLUSHED);
		mdp3_bufq_push(&mdp3_session->bufq_out, data);
	}

	if (mdp3_bufq_count(&mdp3_session->bufq_out) > 1) {
		mdp3_release_splash_memory(mfd);
		data = mdp3_bufq_pop(&mdp3_session->bufq_out);
		mdp3_put_img(data, MDP3_CLIENT_DMA_P);
	}

	if (mdp3_session->first_commit) {
		/*wait for one frame time to ensure frame is sent to panel*/
		msleep(1000 / panel_info->mipi.frame_rate);
		mdp3_session->first_commit = false;
	}

	mdp3_session->vsync_before_commit = 0;
	if (reset_done && (panel && panel->set_backlight))
		panel->set_backlight(panel, panel->panel_info.bl_max);

	mutex_unlock(&mdp3_session->lock);

	mdss_fb_update_notify_update(mfd);

	return 0;
}

static void mdp3_ctrl_pan_display(struct msm_fb_data_type *mfd,
					struct mdp_overlay *req,
					int image_size,
					int *pipe_ndx)
{
	struct fb_info *fbi;
	struct mdp3_session_data *mdp3_session;
	u32 offset;
	int bpp;
	struct mdss_panel_info *panel_info = mfd->panel_info;
	int rc;

	pr_debug("mdp3_ctrl_pan_display\n");
	if (!mfd || !mfd->mdp.private1)
		return;

	mdp3_session = (struct mdp3_session_data *)mfd->mdp.private1;
	if (!mdp3_session || !mdp3_session->dma)
		return;

	if (!mdp3_iommu_is_attached(MDP3_CLIENT_DMA_P)) {
		pr_debug("continuous splash screen, IOMMU not attached\n");
		mdp3_ctrl_reset(mfd);
	}

	mutex_lock(&mdp3_session->lock);

	if (!mdp3_session->status) {
		pr_err("mdp3_ctrl_pan_display, display off!\n");
		goto pan_error;
	}

	fbi = mfd->fbi;

	bpp = fbi->var.bits_per_pixel / 8;
	offset = fbi->var.xoffset * bpp +
		 fbi->var.yoffset * fbi->fix.line_length;

	if (offset > fbi->fix.smem_len) {
		pr_err("invalid fb offset=%u total length=%u\n",
			offset, fbi->fix.smem_len);
		goto pan_error;
	}

	if (mfd->fbi->screen_base) {
		mdp3_ctrl_reset_countdown(mdp3_session, mfd);
		mdp3_ctrl_notify(mdp3_session, MDP_NOTIFY_FRAME_BEGIN);
		mdp3_ctrl_clk_enable(mfd, 1);
		rc = mdp3_session->dma->update(mdp3_session->dma,
				(void *)(mfd->iova + offset),
				mdp3_session->intf);
		/* This is for the previous frame */
		if (rc < 0) {
			mdp3_ctrl_notify(mdp3_session,
				MDP_NOTIFY_FRAME_TIMEOUT);
		} else {
			if (mdp3_ctrl_get_intf_type(mfd) ==
						MDP3_DMA_OUTPUT_SEL_DSI_VIDEO) {
				mdp3_ctrl_notify(mdp3_session,
					MDP_NOTIFY_FRAME_DONE);
			}
		}
		mdp3_session->dma_active = 1;
		init_completion(&mdp3_session->dma_completion);
		mdp3_ctrl_notify(mdp3_session, MDP_NOTIFY_FRAME_FLUSHED);
	} else {
		pr_debug("mdp3_ctrl_pan_display no memory, stop interface");
		mdp3_clk_enable(1, 0);
		mdp3_session->dma->stop(mdp3_session->dma, mdp3_session->intf);
		mdp3_clk_enable(0, 0);
	}

	if (mdp3_session->first_commit) {
		/*wait for one frame time to ensure frame is sent to panel*/
		msleep(1000 / panel_info->mipi.frame_rate);
		mdp3_session->first_commit = false;
	}

	mdp3_session->vsync_before_commit = 0;

pan_error:
	mutex_unlock(&mdp3_session->lock);
}

static int mdp3_set_metadata(struct msm_fb_data_type *mfd,
				struct msmfb_metadata *metadata_ptr)
{
	int ret = 0;
	switch (metadata_ptr->op) {
	case metadata_op_crc:
		ret = mdp3_misr_set(&metadata_ptr->data.misr_request);
		break;
	default:
		pr_warn("Unsupported request to MDP SET META IOCTL.\n");
		ret = -EINVAL;
		break;
	}
	return ret;
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
	case metadata_op_crc:
		ret = mdp3_misr_get(&metadata->data.misr_request);
		break;
	default:
		pr_warn("Unsupported request to MDP GET  META IOCTL.\n");
		ret = -EINVAL;
		break;
	}
	return ret;
}

int mdp3_validate_start_req(struct mdp_histogram_start_req *req)
{
	if (req->frame_cnt >= MDP_HISTOGRAM_FRAME_COUNT_MAX) {
		pr_err("%s invalid req frame_cnt\n", __func__);
		return -EINVAL;
	}
	if (req->bit_mask >= MDP_HISTOGRAM_BIT_MASK_MAX) {
		pr_err("%s invalid req bit mask\n", __func__);
		return -EINVAL;
	}
	if (req->block != MDP_BLOCK_DMA_P ||
		req->num_bins != MDP_HISTOGRAM_BIN_NUM) {
		pr_err("mdp3_histogram_start invalid request\n");
		return -EINVAL;
	}
	return 0;
}

int mdp3_validate_scale_config(struct mdp_bl_scale_data *data)
{
	if (data->scale > MDP_HISTOGRAM_BL_SCALE_MAX) {
		pr_err("%s invalid bl_scale\n", __func__);
		return -EINVAL;
	}
	if (data->min_lvl > MDP_HISTOGRAM_BL_LEVEL_MAX) {
		pr_err("%s invalid bl_min_lvl\n", __func__);
		return -EINVAL;
	}
	return 0;
}

int mdp3_validate_csc_data(struct mdp_csc_cfg_data *data)
{
	int i;
	for (i = 0; i < 9; i++) {
		if (data->csc_data.csc_mv[i] >=
				MDP_HISTOGRAM_CSC_MATRIX_MAX)
			return -EINVAL;
	}
	for (i = 0; i < 3; i++) {
		if (data->csc_data.csc_pre_bv[i] >=
				MDP_HISTOGRAM_CSC_VECTOR_MAX)
			return -EINVAL;
		if (data->csc_data.csc_post_bv[i] >=
				MDP_HISTOGRAM_CSC_VECTOR_MAX)
			return -EINVAL;
	}
	for (i = 0; i < 6; i++) {
		if (data->csc_data.csc_pre_lv[i] >=
				MDP_HISTOGRAM_CSC_VECTOR_MAX)
			return -EINVAL;
		if (data->csc_data.csc_post_lv[i] >=
				MDP_HISTOGRAM_CSC_VECTOR_MAX)
			return -EINVAL;
	}
	return 0;
}

static int mdp3_histogram_start(struct mdp3_session_data *session,
					struct mdp_histogram_start_req *req)
{
	int ret;
	struct mdp3_dma_histogram_config histo_config;

	pr_debug("mdp3_histogram_start\n");

	ret = mdp3_validate_start_req(req);
	if (ret)
		return ret;

	if (!session->dma->histo_op ||
		!session->dma->config_histo) {
		pr_err("mdp3_histogram_start not supported\n");
		return -EINVAL;
	}

	mutex_lock(&session->histo_lock);

	if (session->histo_status) {
		pr_err("mdp3_histogram_start already started\n");
		mutex_unlock(&session->histo_lock);
		return -EBUSY;
	}

	mdp3_res_update(1, 0, MDP3_CLIENT_DMA_P);
	ret = session->dma->histo_op(session->dma, MDP3_DMA_HISTO_OP_RESET);
	if (ret) {
		pr_err("mdp3_histogram_start reset error\n");
		goto histogram_start_err;
	}

	histo_config.frame_count = req->frame_cnt;
	histo_config.bit_mask = req->bit_mask;
	histo_config.auto_clear_en = 1;
	histo_config.bit_mask_polarity = 0;
	ret = session->dma->config_histo(session->dma, &histo_config);
	if (ret) {
		pr_err("mdp3_histogram_start config error\n");
		goto histogram_start_err;
	}

	ret = session->dma->histo_op(session->dma, MDP3_DMA_HISTO_OP_START);
	if (ret) {
		pr_err("mdp3_histogram_start config error\n");
		goto histogram_start_err;
	}

	session->histo_status = 1;

histogram_start_err:
	mdp3_res_update(0, 0, MDP3_CLIENT_DMA_P);
	mutex_unlock(&session->histo_lock);
	return ret;
}

static int mdp3_histogram_stop(struct mdp3_session_data *session,
					u32 block)
{
	int ret;
	pr_debug("mdp3_histogram_stop\n");

	if (!session->dma->histo_op || block != MDP_BLOCK_DMA_P) {
		pr_err("mdp3_histogram_stop not supported\n");
		return -EINVAL;
	}

	mutex_lock(&session->histo_lock);

	if (!session->histo_status) {
		ret = 0;
		goto histogram_stop_err;
	}

	mdp3_clk_enable(1, 0);
	ret = session->dma->histo_op(session->dma, MDP3_DMA_HISTO_OP_CANCEL);
	mdp3_clk_enable(0, 0);
	if (ret)
		pr_err("mdp3_histogram_stop error\n");

	session->histo_status = 0;

histogram_stop_err:
	mutex_unlock(&session->histo_lock);
	return ret;
}

static int mdp3_histogram_collect(struct mdp3_session_data *session,
				struct mdp_histogram_data *hist)
{
	int ret;
	struct mdp3_dma_histogram_data *mdp3_histo;

	pr_debug("%s\n", __func__);
	if (!session->dma->get_histo) {
		pr_err("mdp3_histogram_collect not supported\n");
		return -EINVAL;
	}

	if (!session->clk_on) {
		pr_debug("mdp/dsi clock off currently\n");
		return -EPERM;
	}

	mutex_lock(&session->histo_lock);

	if (!session->histo_status) {
		pr_err("mdp3_histogram_collect not started\n");
		mutex_unlock(&session->histo_lock);
		return -EPERM;
	}

	mutex_unlock(&session->histo_lock);

	mdp3_clk_enable(1, 0);
	ret = session->dma->get_histo(session->dma);
	mdp3_clk_enable(0, 0);
	if (ret) {
		pr_debug("mdp3_histogram_collect error = %d\n", ret);
		return ret;
	}

	mdp3_histo = &session->dma->histo_data;

	ret = copy_to_user(hist->c0, mdp3_histo->r_data,
			sizeof(uint32_t) * MDP_HISTOGRAM_BIN_NUM);
	if (ret)
		return ret;

	ret = copy_to_user(hist->c1, mdp3_histo->g_data,
			sizeof(uint32_t) * MDP_HISTOGRAM_BIN_NUM);
	if (ret)
		return ret;

	ret = copy_to_user(hist->c2, mdp3_histo->b_data,
			sizeof(uint32_t) * MDP_HISTOGRAM_BIN_NUM);
	if (ret)
		return ret;

	ret = copy_to_user(hist->extra_info, mdp3_histo->extra,
			sizeof(uint32_t) * 2);
	if (ret)
		return ret;

	hist->bin_cnt = MDP_HISTOGRAM_BIN_NUM;
	hist->block = MDP_BLOCK_DMA_P;
	return ret;
}

static int mdp3_bl_scale_config(struct msm_fb_data_type *mfd,
					struct mdp_bl_scale_data *data)
{
	int ret = 0;
	int curr_bl;
	mutex_lock(&mfd->bl_lock);
	curr_bl = mfd->bl_level;
	mfd->bl_scale = data->scale;
	mfd->bl_min_lvl = data->min_lvl;
	pr_debug("update scale = %d, min_lvl = %d\n", mfd->bl_scale,
							mfd->bl_min_lvl);

	/* update current backlight to use new scaling*/
	mdss_fb_set_backlight(mfd, curr_bl);
	mutex_unlock(&mfd->bl_lock);
	return ret;
}

static int mdp3_csc_config(struct mdp3_session_data *session,
					struct mdp_csc_cfg_data *data)
{
	struct mdp3_dma_color_correct_config config;
	struct mdp3_dma_ccs ccs;
	int ret = -EINVAL;

	if (!data->csc_data.csc_mv || !data->csc_data.csc_pre_bv ||
		!data->csc_data.csc_post_bv || !data->csc_data.csc_pre_lv ||
			!data->csc_data.csc_post_lv) {
		pr_err("%s : Invalid csc vectors", __func__);
		return -EINVAL;
	}

	session->cc_vect_sel = (session->cc_vect_sel + 1) % 2;

	config.ccs_enable = 1;
	config.ccs_sel = session->cc_vect_sel;
	config.pre_limit_sel = session->cc_vect_sel;
	config.post_limit_sel = session->cc_vect_sel;
	config.pre_bias_sel = session->cc_vect_sel;
	config.post_bias_sel = session->cc_vect_sel;
	config.ccs_dirty = true;

	ccs.mv = data->csc_data.csc_mv;
	ccs.pre_bv = data->csc_data.csc_pre_bv;
	ccs.post_bv = data->csc_data.csc_post_bv;
	ccs.pre_lv = data->csc_data.csc_pre_lv;
	ccs.post_lv = data->csc_data.csc_post_lv;

	mutex_lock(&session->lock);
	mdp3_clk_enable(1, 0);
	ret = session->dma->config_ccs(session->dma, &config, &ccs);
	mdp3_clk_enable(0, 0);
	mutex_unlock(&session->lock);
	return ret;
}

static int mdp3_pp_ioctl(struct msm_fb_data_type *mfd,
					void __user *argp)
{
	int ret = -EINVAL;
	struct msmfb_mdp_pp mdp_pp;
	struct mdp3_session_data *mdp3_session;

	if (!mfd || !mfd->mdp.private1)
		return -EINVAL;

	mdp3_session = mfd->mdp.private1;

	ret = copy_from_user(&mdp_pp, argp, sizeof(mdp_pp));
	if (ret)
		return ret;

	switch (mdp_pp.op) {
	case mdp_bl_scale_cfg:
		ret = mdp3_validate_scale_config(&mdp_pp.data.bl_scale_data);
		if (ret) {
			pr_err("%s: invalid scale config\n", __func__);
			break;
		}
		ret = mdp3_bl_scale_config(mfd, (struct mdp_bl_scale_data *)
						&mdp_pp.data.bl_scale_data);
		break;
	case mdp_op_csc_cfg:
		ret = mdp3_validate_csc_data(&(mdp_pp.data.csc_cfg_data));
		if (ret) {
			pr_err("%s: invalid csc data\n", __func__);
			break;
		}
		ret = mdp3_csc_config(mdp3_session,
						&(mdp_pp.data.csc_cfg_data));
		break;

	default:
		pr_err("Unsupported request to MDP_PP IOCTL.\n");
		ret = -EINVAL;
		break;
	}
	if (!ret)
		ret = copy_to_user(argp, &mdp_pp, sizeof(struct msmfb_mdp_pp));
	return ret;
}

static int mdp3_histo_ioctl(struct msm_fb_data_type *mfd, u32 cmd,
				void __user *argp)
{
	int ret = -ENOSYS;
	struct mdp_histogram_data hist;
	struct mdp_histogram_start_req hist_req;
	u32 block;
	struct mdp3_session_data *mdp3_session;

	if (!mfd || !mfd->mdp.private1)
		return -EINVAL;

	mdp3_session = mfd->mdp.private1;

	switch (cmd) {
	case MSMFB_HISTOGRAM_START:
		ret = copy_from_user(&hist_req, argp, sizeof(hist_req));
		if (ret)
			return ret;

		ret = mdp3_histogram_start(mdp3_session, &hist_req);
		break;

	case MSMFB_HISTOGRAM_STOP:
		ret = copy_from_user(&block, argp, sizeof(int));
		if (ret)
			return ret;

		ret = mdp3_histogram_stop(mdp3_session, block);
		break;

	case MSMFB_HISTOGRAM:
		ret = copy_from_user(&hist, argp, sizeof(hist));
		if (ret)
			return ret;

		ret = mdp3_histogram_collect(mdp3_session, &hist);
		if (!ret)
			ret = copy_to_user(argp, &hist, sizeof(hist));
		break;
	default:
		break;
	}
	return ret;
}

static int mdp3_ctrl_lut_update(struct msm_fb_data_type *mfd,
				struct fb_cmap *cmap)
{
	int rc = 0;
	struct mdp3_session_data *mdp3_session = mfd->mdp.private1;
	struct mdp3_dma_lut_config lut_config;
	struct mdp3_dma_lut lut;
	static u16 r[MDP_LUT_SIZE];
	static u16 g[MDP_LUT_SIZE];
	static u16 b[MDP_LUT_SIZE];

	if (!mdp3_session->dma->config_lut)
		return -EINVAL;

	if (cmap->start > MDP_LUT_SIZE || cmap->len > MDP_LUT_SIZE ||
			(cmap->start + cmap->len > MDP_LUT_SIZE)) {
		pr_err("mdp3_ctrl_lut_update invalid arguments\n");
		return  -EINVAL;
	}

	rc = copy_from_user(r + cmap->start,
					cmap->red, sizeof(u16)*cmap->len);
	rc |= copy_from_user(g + cmap->start,
					cmap->green, sizeof(u16)*cmap->len);
	rc |= copy_from_user(b + cmap->start,
					cmap->blue, sizeof(u16)*cmap->len);
	if (rc)
		return rc;

	lut_config.lut_enable = 7;
	lut_config.lut_sel = mdp3_session->lut_sel;
	lut_config.lut_position = 0;
	lut_config.lut_dirty = true;
	lut.color0_lut = r;
	lut.color1_lut = g;
	lut.color2_lut = b;

	mutex_lock(&mdp3_session->lock);

	if (!mdp3_session->status) {
		pr_err("%s, display off!\n", __func__);
		mutex_unlock(&mdp3_session->lock);
		return -EPERM;
	}

	mdp3_clk_enable(1, 0);
	rc = mdp3_session->dma->config_lut(mdp3_session->dma, &lut_config,
					&lut);
	mdp3_clk_enable(0, 0);
	if (rc)
		pr_err("mdp3_ctrl_lut_update failed\n");

	mdp3_session->lut_sel = (mdp3_session->lut_sel + 1) % 2;

	mutex_unlock(&mdp3_session->lock);
	return rc;
}

static int mdp3_overlay_prepare(struct msm_fb_data_type *mfd,
		struct mdp_overlay_list __user *user_ovlist)
{
	struct mdp_overlay_list ovlist;
	struct mdp3_session_data *mdp3_session = mfd->mdp.private1;
	struct mdp_overlay *req_list;
	struct mdp_overlay *req;
	int rc;

	if (!mdp3_session)
		return -ENODEV;

	req = &mdp3_session->req_overlay;

	if (copy_from_user(&ovlist, user_ovlist, sizeof(ovlist)))
		return -EFAULT;

	if (ovlist.num_overlays != 1) {
		pr_err("OV_PREPARE failed: only 1 overlay allowed\n");
		return -EINVAL;
	}

	if (copy_from_user(&req_list, ovlist.overlay_list, sizeof(struct mdp_overlay*)))
		return -EFAULT;

	if (copy_from_user(req, req_list, sizeof(*req)))
		return -EFAULT;

	rc = mdp3_overlay_set(mfd, req);
	if (!IS_ERR_VALUE(rc)) {
		if (copy_to_user(req_list, req, sizeof(*req)))
			return -EFAULT;
	}

	if (put_user(IS_ERR_VALUE(rc) ? 0 : 1,
			&user_ovlist->processed_overlays))
		return -EFAULT;

	return rc;
}

static int mdp3_ctrl_ioctl_handler(struct msm_fb_data_type *mfd,
					u32 cmd, void __user *argp)
{
	int rc = -EINVAL;
	struct mdp3_session_data *mdp3_session;
	struct msmfb_metadata metadata;
	struct mdp_overlay *req = NULL;
	struct msmfb_overlay_data ov_data;
	int val;

	mdp3_session = (struct mdp3_session_data *)mfd->mdp.private1;
	if (!mdp3_session)
		return -ENODEV;

	req = &mdp3_session->req_overlay;

	if (!mdp3_session->status && cmd != MSMFB_METADATA_GET &&
		cmd != MSMFB_HISTOGRAM_STOP) {
		pr_err("mdp3_ctrl_ioctl_handler, display off!\n");
		return -EPERM;
	}

	switch (cmd) {
	case MSMFB_MDP_PP:
		rc = mdp3_pp_ioctl(mfd, argp);
		break;
	case MSMFB_HISTOGRAM_START:
	case MSMFB_HISTOGRAM_STOP:
	case MSMFB_HISTOGRAM:
		rc = mdp3_histo_ioctl(mfd, cmd, argp);
		break;

	case MSMFB_VSYNC_CTRL:
	case MSMFB_OVERLAY_VSYNC_CTRL:
		if (!copy_from_user(&val, argp, sizeof(val))) {
			mutex_lock(&mdp3_session->lock);
			mdp3_session->vsync_enabled = val;
			rc = mdp3_ctrl_vsync_enable(mfd, val);
			mutex_unlock(&mdp3_session->lock);
		} else {
			pr_err("MSMFB_OVERLAY_VSYNC_CTRL failed\n");
			rc = -EFAULT;
		}
		break;
	case MSMFB_ASYNC_BLIT:
		rc = mdp3_ctrl_async_blit_req(mfd, argp);
		break;
	case MSMFB_BLIT:
		rc = mdp3_ctrl_blit_req(mfd, argp);
		break;
	case MSMFB_METADATA_GET:
		rc = copy_from_user(&metadata, argp, sizeof(metadata));
		if (!rc)
			rc = mdp3_get_metadata(mfd, &metadata);
		if (!rc)
			rc = copy_to_user(argp, &metadata, sizeof(metadata));
		if (rc)
			pr_err("mdp3_get_metadata failed (%d)\n", rc);
		break;
	case MSMFB_METADATA_SET:
		rc = copy_from_user(&metadata, argp, sizeof(metadata));
		if (!rc)
			rc = mdp3_set_metadata(mfd, &metadata);
		if (rc)
			pr_err("mdp3_set_metadata failed (%d)\n", rc);
		break;
	case MSMFB_OVERLAY_GET:
		rc = copy_from_user(req, argp, sizeof(*req));
		if (!rc) {
			rc = mdp3_overlay_get(mfd, req);

		if (!IS_ERR_VALUE(rc))
			rc = copy_to_user(argp, req, sizeof(*req));
		}
		if (rc)
			pr_err("OVERLAY_GET failed (%d)\n", rc);
		break;
	case MSMFB_OVERLAY_SET:
		rc = copy_from_user(req, argp, sizeof(*req));
		if (!rc) {
			rc = mdp3_overlay_set(mfd, req);

		if (!IS_ERR_VALUE(rc))
			rc = copy_to_user(argp, req, sizeof(*req));
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
	case MSMFB_OVERLAY_PREPARE:
		rc = mdp3_overlay_prepare(mfd, argp);
		break;
	default:
		break;
	}
	return rc;
}

int mdp3_wait_for_dma_done(struct mdp3_session_data *session)
{
	int rc = 0;

	if (session->dma_active) {
		rc = wait_for_completion_timeout(&session->dma_completion,
			KOFF_TIMEOUT);
		if (rc > 0) {
			session->dma_active = 0;
			rc = 0;
		} else if (rc == 0) {
			rc = -ETIME;
		}
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
	int splash_mismatch = 0;

	pr_debug("mdp3_ctrl_init\n");
	rc = mdp3_parse_dt_splash(mfd);
	if (rc)
		splash_mismatch = 1;

	mdp3_interface->on_fnc = mdp3_ctrl_on;
	mdp3_interface->off_fnc = mdp3_ctrl_off;
	mdp3_interface->do_histogram = NULL;
	mdp3_interface->cursor_update = NULL;
	mdp3_interface->dma_fnc = mdp3_ctrl_pan_display;
	mdp3_interface->ioctl_handler = mdp3_ctrl_ioctl_handler;
	mdp3_interface->kickoff_fnc = mdp3_ctrl_display_commit_kickoff;
	mdp3_interface->lut_update = mdp3_ctrl_lut_update;

	mdp3_session = kmalloc(sizeof(struct mdp3_session_data), GFP_KERNEL);
	if (!mdp3_session) {
		pr_err("fail to allocate mdp3 private data structure");
		return -ENOMEM;
	}
	memset(mdp3_session, 0, sizeof(struct mdp3_session_data));
	mutex_init(&mdp3_session->lock);
	INIT_WORK(&mdp3_session->clk_off_work, mdp3_dispatch_clk_off);
	INIT_WORK(&mdp3_session->dma_done_work, mdp3_dispatch_dma_done);
	atomic_set(&mdp3_session->vsync_countdown, 0);
	mutex_init(&mdp3_session->histo_lock);
	mdp3_session->dma = mdp3_get_dma_pipe(MDP3_DMA_CAP_ALL);
	if (!mdp3_session->dma) {
		rc = -ENODEV;
		goto init_done;
	}

	rc = mdp3_dma_init(mdp3_session->dma);
	if (rc) {
		pr_err("fail to init dma\n");
		goto init_done;
	}

	intf_type = mdp3_ctrl_get_intf_type(mfd);
	mdp3_session->intf = mdp3_get_display_intf(intf_type);
	if (!mdp3_session->intf) {
		rc = -ENODEV;
		goto init_done;
	}
	rc = mdp3_intf_init(mdp3_session->intf);
	if (rc) {
		pr_err("fail to init interface\n");
		goto init_done;
	}

	mdp3_session->dma->output_config.out_sel = intf_type;
	mdp3_session->mfd = mfd;
	mdp3_session->panel = dev_get_platdata(&mfd->pdev->dev);
	mdp3_session->status = mdp3_session->intf->active;
	mdp3_session->overlay.id = MSMFB_NEW_REQUEST;
	mdp3_bufq_init(&mdp3_session->bufq_in);
	mdp3_bufq_init(&mdp3_session->bufq_out);
	mdp3_session->histo_status = 0;
	mdp3_session->lut_sel = 0;
	BLOCKING_INIT_NOTIFIER_HEAD(&mdp3_session->notifier_head);

	init_timer(&mdp3_session->vsync_timer);
	mdp3_session->vsync_timer.function = mdp3_vsync_timer_func;
	mdp3_session->vsync_timer.data = (u32)mdp3_session;
	mdp3_session->vsync_period = 1000 / mfd->panel_info->mipi.frame_rate;
	mfd->mdp.private1 = mdp3_session;
	init_completion(&mdp3_session->dma_completion);
	if (intf_type != MDP3_DMA_OUTPUT_SEL_DSI_VIDEO)
		mdp3_session->wait_for_dma_done = mdp3_wait_for_dma_done;

	rc = sysfs_create_group(&dev->kobj, &vsync_fs_attr_group);
	if (rc) {
		pr_err("vsync sysfs group creation failed, ret=%d\n", rc);
		goto init_done;
	}

	mdp3_session->vsync_event_sd = sysfs_get_dirent(dev->kobj.sd, NULL,
							"vsync_event");
	if (!mdp3_session->vsync_event_sd) {
		pr_err("vsync_event sysfs lookup failed\n");
		rc = -ENODEV;
		goto init_done;
	}

	rc = mdp3_create_sysfs_link(dev);
	if (rc)
		pr_warn("problem creating link to mdp sysfs\n");

	kobject_uevent(&dev->kobj, KOBJ_ADD);
	pr_debug("vsync kobject_uevent(KOBJ_ADD)\n");

	if (mdp3_get_cont_spash_en()) {
		mdp3_session->clk_on = 1;
		mdp3_ctrl_notifier_register(mdp3_session,
			&mdp3_session->mfd->mdp_sync_pt_data.notifier);
	}

	if (splash_mismatch) {
		pr_err("splash memory mismatch, stop splash\n");
		mdp3_ctrl_off(mfd);
	}

	mdp3_session->vsync_before_commit = true;
init_done:
	if (IS_ERR_VALUE(rc))
		kfree(mdp3_session);

	return rc;
}
