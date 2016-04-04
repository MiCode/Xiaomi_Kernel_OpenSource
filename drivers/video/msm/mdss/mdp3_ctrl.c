/* Copyright (c) 2013-2016, The Linux Foundation. All rights reserved.
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
#include <linux/dma-buf.h>
#include <linux/pm_runtime.h>

#include "mdp3_ctrl.h"
#include "mdp3.h"
#include "mdp3_ppp.h"
#include "mdss_smmu.h"

#define VSYNC_EXPIRE_TICK	4

static void mdp3_ctrl_pan_display(struct msm_fb_data_type *mfd);
static int mdp3_overlay_unset(struct msm_fb_data_type *mfd, int ndx);
static int mdp3_histogram_stop(struct mdp3_session_data *session,
					u32 block);
static int mdp3_ctrl_clk_enable(struct msm_fb_data_type *mfd, int enable);
static int mdp3_ctrl_vsync_enable(struct msm_fb_data_type *mfd, int enable);
static int mdp3_ctrl_get_intf_type(struct msm_fb_data_type *mfd);
static int mdp3_ctrl_lut_read(struct msm_fb_data_type *mfd,
				struct mdp_rgb_lut_data *cfg);
static int mdp3_ctrl_lut_config(struct msm_fb_data_type *mfd,
				struct mdp_rgb_lut_data *cfg);
static void mdp3_ctrl_pp_resume(struct msm_fb_data_type *mfd);
static int mdp3_ctrl_reset(struct msm_fb_data_type *mfd);
static int mdp3_ctrl_get_pack_pattern(u32 imgType);

u32 mdp_lut_inverse16[MDP_LUT_SIZE] = {
0, 65536, 32768, 21845, 16384, 13107, 10923, 9362, 8192, 7282, 6554, 5958,
5461, 5041, 4681, 4369, 4096, 3855, 3641, 3449, 3277, 3121, 2979, 2849, 2731,
2621, 2521, 2427, 2341, 2260, 2185, 2114, 2048, 1986, 1928, 1872, 1820, 1771,
1725, 1680, 1638, 1598, 1560, 1524, 1489, 1456, 1425, 1394, 1365, 1337, 1311,
1285, 1260, 1237, 1214, 1192, 1170, 1150, 1130, 1111, 1092, 1074, 1057, 1040,
1024, 1008, 993, 978, 964, 950, 936, 923, 910, 898, 886, 874, 862, 851, 840,
830, 819, 809, 799, 790, 780, 771, 762, 753, 745, 736, 728, 720, 712, 705, 697,
690, 683, 676, 669, 662, 655, 649, 643, 636, 630, 624, 618, 612, 607, 601, 596,
590, 585, 580, 575, 570, 565, 560, 555, 551, 546, 542, 537, 533, 529, 524, 520,
516, 512, 508, 504, 500, 496, 493, 489, 485, 482, 478, 475, 471, 468, 465, 462,
458, 455, 452, 449, 446, 443, 440, 437, 434, 431, 428, 426, 423, 420, 417, 415,
412, 410, 407, 405, 402, 400, 397, 395, 392, 390, 388, 386, 383, 381, 379, 377,
374, 372, 370, 368, 366, 364, 362, 360, 358, 356, 354, 352, 350, 349, 347, 345,
343, 341, 340, 338, 336, 334, 333, 331, 329, 328, 326, 324, 323, 321, 320, 318,
317, 315, 314, 312, 311, 309, 308, 306, 305, 303, 302, 301, 299, 298, 297, 295,
294, 293, 291, 290, 289, 287, 286, 285, 284, 282, 281, 280, 279, 278, 277, 275,
274, 273, 272, 271, 270, 269, 267, 266, 265, 264, 263, 262, 261, 260, 259, 258,
257};

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

	while (count-- && (bufq->pop_idx >= 0)) {
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
	int cnt = 0;

	pr_debug("%s\n", __func__);
	session = container_of(work, struct mdp3_session_data,
				dma_done_work);
	if (!session)
		return;

	cnt = atomic_read(&session->dma_done_cnt);

	while (cnt > 0) {
		mdp3_ctrl_notify(session, MDP_NOTIFY_FRAME_DONE);
		atomic_dec(&session->dma_done_cnt);
		cnt--;
	}
}

static void mdp3_dispatch_clk_off(struct work_struct *work)
{
	struct mdp3_session_data *session;
	int rc;
	bool dmap_busy;
	int retry_count = 2;

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

	if (session->intf->active) {
retry_dma_done:
		rc = wait_for_completion_timeout(&session->dma_completion,
							WAIT_DMA_TIMEOUT);
		if (rc <= 0) {
			struct mdss_panel_data *panel;

			panel = session->panel;
			pr_debug("cmd kickoff timed out (%d)\n", rc);
			dmap_busy = session->dma->busy();
			if (dmap_busy) {
				if (--retry_count) {
					pr_err("dmap is busy, retry %d\n",
						retry_count);
					goto retry_dma_done;
				}
				pr_err("dmap is still busy, bug_on\n");
				BUG_ON(1);
			} else {
				pr_debug("dmap is not busy, continue\n");
			}
		}
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
	atomic_inc(&session->dma_done_cnt);
	schedule_work(&session->dma_done_work);
	complete_all(&session->dma_completion);
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

	pr_debug("fb%d vsync=%llu\n", mfd->index, vsync_ticks);
	rc = scnprintf(buf, PAGE_SIZE, "VSYNC=%llu\n", vsync_ticks);
	return rc;
}

static ssize_t mdp3_packpattern_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdp3_session_data *mdp3_session = NULL;
	int rc;
	u32 pattern = 0;

	if (!mfd || !mfd->mdp.private1)
		return -EAGAIN;

	mdp3_session = (struct mdp3_session_data *)mfd->mdp.private1;

	pattern = mdp3_session->dma->output_config.pack_pattern;

	/* If pattern was found to be 0 then get pattern for fb imagetype */
	if (!pattern)
		pattern = mdp3_ctrl_get_pack_pattern(mfd->fb_imgType);

	pr_debug("fb%d pack_pattern c= %d.", mfd->index, pattern);
	rc = scnprintf(buf, PAGE_SIZE, "packpattern=%d\n", pattern);
	return rc;
}

static ssize_t mdp3_dyn_pu_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = fbi->par;
	struct mdp3_session_data *mdp3_session = NULL;
	int ret, state;

	if (!mfd || !mfd->mdp.private1)
		return -EAGAIN;

	mdp3_session = (struct mdp3_session_data *)mfd->mdp.private1;
	state = (mdp3_session->dyn_pu_state >= 0) ?
		mdp3_session->dyn_pu_state : -1;
	ret = scnprintf(buf, PAGE_SIZE, "%d", state);
	return ret;
}

static ssize_t mdp3_dyn_pu_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = fbi->par;
	struct mdp3_session_data *mdp3_session = NULL;
	int ret, dyn_pu;

	if (!mfd || !mfd->mdp.private1)
		return -EAGAIN;

	mdp3_session = (struct mdp3_session_data *)mfd->mdp.private1;
	ret = kstrtoint(buf, 10, &dyn_pu);
	if (ret) {
		pr_err("Invalid input for partial update: ret = %d\n", ret);
		return ret;
	}

	mdp3_session->dyn_pu_state = dyn_pu;
	sysfs_notify(&dev->kobj, NULL, "dyn_pu");
	return count;
}

static DEVICE_ATTR(vsync_event, S_IRUGO, mdp3_vsync_show_event, NULL);
static DEVICE_ATTR(packpattern, S_IRUGO, mdp3_packpattern_show, NULL);
static DEVICE_ATTR(dyn_pu, S_IRUGO | S_IWUSR | S_IWGRP, mdp3_dyn_pu_show,
		mdp3_dyn_pu_store);

static struct attribute *generic_attrs[] = {
	&dev_attr_packpattern.attr,
	&dev_attr_dyn_pu.attr,
	NULL,
};

static struct attribute *vsync_fs_attrs[] = {
	&dev_attr_vsync_event.attr,
	NULL,
};

static struct attribute_group vsync_fs_attr_group = {
	.attrs = vsync_fs_attrs,
};

static struct attribute_group generic_attr_group = {
	.attrs = generic_attrs,
};

static int mdp3_ctrl_clk_enable(struct msm_fb_data_type *mfd, int enable)
{
	struct mdp3_session_data *session;
	struct mdss_panel_data *panel;
	struct dsi_panel_clk_ctrl clk_ctrl;
	int rc = 0;

	pr_debug("mdp3_ctrl_clk_enable %d\n", enable);

	session = mfd->mdp.private1;
	panel = session->panel;

	if (!panel->event_handler)
		return 0;

	if ((enable && session->clk_on == 0) ||
				(!enable && session->clk_on == 1)) {
		clk_ctrl.client = DSI_CLK_REQ_MDP_CLIENT;
		clk_ctrl.state = enable;
		rc = panel->event_handler(panel,
			MDSS_EVENT_PANEL_CLK_CTRL, (void *)&clk_ctrl);
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
		u64 ab = 0;
		u64 ib = 0;
		mdp3_calc_dma_res(mfd->panel_info, NULL, &ab, &ib,
			ppp_bpp(mfd->fb_imgType));
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
		u64 mdp_clk_rate = 0;

		mdp3_calc_dma_res(mfd->panel_info, &mdp_clk_rate,
			NULL, NULL, 0);

		mdp3_clk_set_rate(MDP3_CLK_MDP_SRC, mdp_clk_rate,
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
	if (imgType == MDP_RGBA_8888 || imgType == MDP_RGB_888)
		packPattern = MDP3_DMA_OUTPUT_PACK_PATTERN_BGR;
	return packPattern;
}

static int mdp3_ctrl_intf_init(struct msm_fb_data_type *mfd,
				struct mdp3_intf *intf)
{
	int rc = 0;
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
	struct mdp3_session_data *mdp3_session;

	mdp3_session = (struct mdp3_session_data *)mfd->mdp.private1;
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

	if (!(mdp3_session->in_splash_screen)) {
		if (intf->config)
			rc = intf->config(intf, &cfg);
		else
			rc = -EINVAL;
	}
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
	struct mdp3_tear_check te;
	struct mdp3_session_data *mdp3_session;

	mdp3_session = (struct mdp3_session_data *)mfd->mdp.private1;

	vbp = panel_info->lcdc.v_back_porch;
	vfp = panel_info->lcdc.v_front_porch;
	vspw = panel_info->lcdc.v_pulse_width;
	vporch = vbp + vfp + vspw;
	vtotal = vporch + panel_info->yres;

	fix = &fbi->fix;
	var = &fbi->var;

	sourceConfig.width = panel_info->xres;
	sourceConfig.height = panel_info->yres;
	sourceConfig.x = 0;
	sourceConfig.y = 0;
	sourceConfig.buf = mfd->iova;
	sourceConfig.vporch = vporch;
	sourceConfig.vsync_count =
		MDP_VSYNC_CLK_RATE / (frame_rate * vtotal);

	outputConfig.dither_en = 0;
	outputConfig.out_sel = mdp3_ctrl_get_intf_type(mfd);
	outputConfig.bit_mask_polarity = 0;
	outputConfig.color_components_flip = 0;
	outputConfig.pack_align = MDP3_DMA_OUTPUT_PACK_ALIGN_LSB;
	outputConfig.color_comp_out_bits = (MDP3_DMA_OUTPUT_COMP_BITS_8 << 4) |
					(MDP3_DMA_OUTPUT_COMP_BITS_8 << 2)|
					MDP3_DMA_OUTPUT_COMP_BITS_8;

	if (dma->update_src_cfg) {
		/* configuration has been updated through PREPARE call */
		sourceConfig.format = dma->source_config.format;
		sourceConfig.stride = dma->source_config.stride;
		outputConfig.pack_pattern = dma->output_config.pack_pattern;
	} else {
		sourceConfig.format =
			mdp3_ctrl_get_source_format(mfd->fb_imgType);
		outputConfig.pack_pattern =
			mdp3_ctrl_get_pack_pattern(mfd->fb_imgType);
		sourceConfig.stride = fix->line_length;
	}

	te.frame_rate = panel_info->mipi.frame_rate;
	te.hw_vsync_mode = panel_info->mipi.hw_vsync_mode;
	te.tear_check_en = panel_info->te.tear_check_en;
	te.sync_cfg_height = panel_info->te.sync_cfg_height;
	te.vsync_init_val = panel_info->te.vsync_init_val;
	te.sync_threshold_start = panel_info->te.sync_threshold_start;
	te.sync_threshold_continue = panel_info->te.sync_threshold_continue;
	te.start_pos = panel_info->te.start_pos;
	te.rd_ptr_irq = panel_info->te.rd_ptr_irq;
	te.refx100 = panel_info->te.refx100;

	if (dma->dma_config) {
		if (!panel_info->partial_update_enabled) {
			dma->roi.w = sourceConfig.width;
			dma->roi.h = sourceConfig.height;
			dma->roi.x = sourceConfig.x;
			dma->roi.y = sourceConfig.y;
		}
		rc = dma->dma_config(dma, &sourceConfig, &outputConfig,
					mdp3_session->in_splash_screen);
	} else {
		pr_err("%s: dma config failed\n", __func__);
		rc = -EINVAL;
	}

	if (outputConfig.out_sel == MDP3_DMA_OUTPUT_SEL_DSI_CMD) {
		if (dma->dma_sync_config)
			rc = dma->dma_sync_config(dma,
					&sourceConfig, &te);
		else
			rc = -EINVAL;
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

	panel = mdp3_session->panel;
	pr_err("%s %d in_splash_screen %d\n", __func__, __LINE__,
		mdp3_session->in_splash_screen);
	/* make sure DSI host is initialized properly */
	if (panel) {
		pr_debug("%s : dsi host init, power state = %d Splash %d\n",
			__func__, mfd->panel_power_state,
			mdp3_session->in_splash_screen);
		if (mdss_fb_is_power_on_lp(mfd) ||
			mdp3_session->in_splash_screen) {
			/* Turn on panel so that it can exit low power mode */
		pr_err("%s %d\n", __func__, __LINE__);
			mdp3_clk_enable(1, 0);
		rc = panel->event_handler(panel,
				MDSS_EVENT_LINK_READY, NULL);
		rc |= panel->event_handler(panel,
				MDSS_EVENT_UNBLANK, NULL);
		rc |= panel->event_handler(panel,
				MDSS_EVENT_PANEL_ON, NULL);
			mdp3_clk_enable(0, 0);
		}
	}

	if (mdp3_session->status) {
		pr_err("fb%d is on already\n", mfd->index);
		goto end;
	}

	if (mdp3_session->intf->active) {
		pr_debug("continuous splash screen, initialized already\n");
		mdp3_session->status = 1;
		goto end;
	}

	/*
	* Get a reference to the runtime pm device.
	* If idle pc feature is enabled, it will be released
	* at end of this routine else, when device is turned off.
	*/
	pm_runtime_get_sync(&mdp3_res->pdev->dev);

	/* Increment the overlay active count */
	atomic_inc(&mdp3_res->active_intf_cnt);
	mdp3_ctrl_notifier_register(mdp3_session,
		&mdp3_session->mfd->mdp_sync_pt_data.notifier);

	/* request bus bandwidth before DSI DMA traffic */
	rc = mdp3_ctrl_res_req_bus(mfd, 1);
	if (rc) {
		pr_err("fail to request bus resource\n");
		goto on_error;
	}

	rc = mdp3_dynamic_clock_gating_ctrl(0);
	if (rc) {
		pr_err("fail to disable dynamic clock gating\n");
		goto on_error;
	}
	mdp3_qos_remapper_setup(panel);

	rc = mdp3_ctrl_res_req_clk(mfd, 1);
	if (rc) {
		pr_err("fail to request mdp clk resource\n");
		goto on_error;
	}

	if (panel->event_handler) {
		rc = panel->event_handler(panel, MDSS_EVENT_LINK_READY, NULL);
		rc |= panel->event_handler(panel, MDSS_EVENT_UNBLANK, NULL);
		rc |= panel->event_handler(panel, MDSS_EVENT_PANEL_ON, NULL);
		if (panel->panel_info.type == MIPI_CMD_PANEL) {
			struct dsi_panel_clk_ctrl clk_ctrl;

			clk_ctrl.state = MDSS_DSI_CLK_ON;
			clk_ctrl.client = DSI_CLK_REQ_MDP_CLIENT;
			rc |= panel->event_handler(panel,
					MDSS_EVENT_PANEL_CLK_CTRL,
					(void *)&clk_ctrl);
	}
	}
	if (rc) {
		pr_err("fail to turn on the panel\n");
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
	if (mfd->panel_info->panel_dead)
		mdp3_session->esd_recovery = true;

		mdp3_session->status = 1;

	mdp3_ctrl_pp_resume(mfd);
on_error:
	if (rc || (mdp3_res->idle_pc_enabled &&
			(mfd->panel_info->type == MIPI_CMD_PANEL))) {
		if (rc) {
			pr_err("Failed to turn on fb%d\n", mfd->index);
			atomic_dec(&mdp3_res->active_intf_cnt);
		}
		pm_runtime_put(&mdp3_res->pdev->dev);
	}
end:
	mutex_unlock(&mdp3_session->lock);
	return rc;
}

static int mdp3_ctrl_off(struct msm_fb_data_type *mfd)
{
	int rc = 0;
	bool intf_stopped = true;
	struct mdp3_session_data *mdp3_session;
	struct mdss_panel_data *panel;

	pr_debug("mdp3_ctrl_off\n");
	mdp3_session = (struct mdp3_session_data *)mfd->mdp.private1;
	if (!mdp3_session || !mdp3_session->panel || !mdp3_session->dma ||
		!mdp3_session->intf) {
		pr_err("mdp3_ctrl_on no device");
		return -ENODEV;
	}

	/*
	 * Keep a reference to the runtime pm until the overlay is turned
	 * off, and then release this last reference at the end. This will
	 * help in distinguishing between idle power collapse versus suspend
	 * power collapse
	 */
	pm_runtime_get_sync(&mdp3_res->pdev->dev);

	panel = mdp3_session->panel;
	mutex_lock(&mdp3_session->lock);

	pr_debug("Requested power state = %d\n", mfd->panel_power_state);
	if (mdss_fb_is_power_on_lp(mfd)) {
		/*
		* Transition to low power
		* As display updates are expected in low power mode,
		* keep the interface and clocks on.
		*/
		intf_stopped = false;
	} else {
		/* Transition to display off */
		if (!mdp3_session->status) {
			pr_debug("fb%d is off already", mfd->index);
			goto off_error;
		}
		if (panel && panel->set_backlight)
			panel->set_backlight(panel, 0);
	}

	/*
	* While transitioning from interactive to low power,
	* events need to be sent to the interface so that the
	* panel can be configured in low power mode
	*/
	if (panel->event_handler)
		rc = panel->event_handler(panel, MDSS_EVENT_BLANK,
			(void *) (long int)mfd->panel_power_state);
	if (rc)
		pr_err("EVENT_BLANK error (%d)\n", rc);

	if (intf_stopped) {
		if (!mdp3_session->clk_on)
			mdp3_ctrl_clk_enable(mfd, 1);
		/* PP related programming for ctrl off */
		mdp3_histogram_stop(mdp3_session, MDP_BLOCK_DMA_P);
		mutex_lock(&mdp3_session->dma->pp_lock);
		mdp3_session->dma->ccs_config.ccs_dirty = false;
		mdp3_session->dma->lut_config.lut_dirty = false;
		mutex_unlock(&mdp3_session->dma->pp_lock);

		rc = mdp3_session->dma->stop(mdp3_session->dma,
					mdp3_session->intf);
		if (rc)
			pr_debug("fail to stop the MDP3 dma\n");
		/* Wait to ensure TG to turn off */
		msleep(20);
		mfd->panel_info->cont_splash_enabled = 0;

		/* Disable Auto refresh once continuous splash disabled */
		mdp3_autorefresh_disable(mfd->panel_info);
		mdp3_splash_done(mfd->panel_info);

		mdp3_irq_deregister();
	}

	if (panel->event_handler)
		rc = panel->event_handler(panel, MDSS_EVENT_PANEL_OFF,
			(void *) (long int)mfd->panel_power_state);
	if (rc)
		pr_err("EVENT_PANEL_OFF error (%d)\n", rc);

	if (intf_stopped) {
		if (mdp3_session->clk_on) {
			pr_debug("mdp3_ctrl_off stop clock\n");
			if (panel->event_handler &&
				(panel->panel_info.type == MIPI_CMD_PANEL)) {
				struct dsi_panel_clk_ctrl clk_ctrl;

				clk_ctrl.state = MDSS_DSI_CLK_OFF;
				clk_ctrl.client = DSI_CLK_REQ_MDP_CLIENT;
				rc |= panel->event_handler(panel,
					MDSS_EVENT_PANEL_CLK_CTRL,
					(void *)&clk_ctrl);
			}

			rc = mdp3_dynamic_clock_gating_ctrl(1);
			rc = mdp3_res_update(0, 1, MDP3_CLIENT_DMA_P);
			if (rc)
				pr_err("mdp clock resource release failed\n");
		}

		mdp3_ctrl_notifier_unregister(mdp3_session,
			&mdp3_session->mfd->mdp_sync_pt_data.notifier);

		mdp3_session->vsync_enabled = 0;
		atomic_set(&mdp3_session->vsync_countdown, 0);
		atomic_set(&mdp3_session->dma_done_cnt, 0);
		mdp3_session->clk_on = 0;
		mdp3_session->in_splash_screen = 0;
		mdp3_res->solid_fill_vote_en = false;
		mdp3_session->status = 0;
		if (atomic_dec_return(&mdp3_res->active_intf_cnt) != 0) {
			pr_warn("active_intf_cnt unbalanced\n");
			atomic_set(&mdp3_res->active_intf_cnt, 0);
		}
		/*
		* Release the pm runtime reference held when
		* idle pc feature is not enabled
		*/
		if (!mdp3_res->idle_pc_enabled ||
			(mfd->panel_info->type != MIPI_CMD_PANEL)) {
			rc = pm_runtime_put(&mdp3_res->pdev->dev);
			if (rc)
				pr_err("%s: pm_runtime_put failed (rc %d)\n",
					__func__, rc);
		}
		mdp3_bufq_deinit(&mdp3_session->bufq_out);
		if (mdp3_session->overlay.id != MSMFB_NEW_REQUEST) {
			mdp3_session->overlay.id = MSMFB_NEW_REQUEST;
			mdp3_bufq_deinit(&mdp3_session->bufq_in);
		}
	}
off_error:
	mutex_unlock(&mdp3_session->lock);
	/* Release the last reference to the runtime device */
	pm_runtime_put(&mdp3_res->pdev->dev);

	return 0;
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

	panel = mdp3_session->panel;
	mdp3_dma = mdp3_session->dma;
	mutex_lock(&mdp3_session->lock);
	if (mdp3_res->idle_pc) {
		mdp3_clk_enable(1, 0);
		mdp3_dynamic_clock_gating_ctrl(0);
		mdp3_qos_remapper_setup(panel);
	}

	rc = mdp3_iommu_enable(MDP3_CLIENT_DMA_P);
	if (rc) {
		pr_err("fail to attach dma iommu\n");
		if (mdp3_res->idle_pc)
			mdp3_clk_enable(0, 0);
		goto reset_error;
	}

	vsync_client = mdp3_dma->vsync_client;

	mdp3_ctrl_intf_init(mfd, mdp3_session->intf);
	mdp3_ctrl_dma_init(mfd, mdp3_dma);
	mdp3_ppp_init();
	mdp3_ctrl_pp_resume(mfd);
	if (vsync_client.handler)
		mdp3_dma->vsync_enable(mdp3_dma, &vsync_client);

	if (!mdp3_res->idle_pc) {
		mdp3_session->first_commit = true;
	mfd->panel_info->cont_splash_enabled = 0;
	mdp3_session->in_splash_screen = 0;
	mdp3_splash_done(mfd->panel_info);
		/* Disable Auto refresh */
		mdp3_autorefresh_disable(mfd->panel_info);
	} else {
		mdp3_res->idle_pc = false;
		mdp3_clk_enable(0, 0);
		mdp3_iommu_disable(MDP3_CLIENT_DMA_P);
	}

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


	if (mdp3_session->overlay.id != req->id)
		pr_err("overlay was not released, continue to recover\n");
	/*
	 * A change in overlay structure will always come with
	 * MSMFB_NEW_REQUEST for MDP3
	*/
	if (req->id == MSMFB_NEW_REQUEST) {
		mutex_lock(&mdp3_session->lock);
		if (dma->source_config.stride != stride ||
				dma->source_config.format != format) {
			dma->source_config.format = format;
			dma->source_config.stride = stride;
			dma->output_config.pack_pattern =
				mdp3_ctrl_get_pack_pattern(req->src.format);
			dma->update_src_cfg = true;
		}
		mdp3_session->overlay = *req;
		mdp3_session->overlay.id = 1;
		req->id = 1;
	mutex_unlock(&mdp3_session->lock);
	}

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
	struct mdp3_dma *dma = mdp3_session->dma;

	memset(&data, 0, sizeof(struct mdp3_img_data));
	rc = mdp3_get_img(img, &data, MDP3_CLIENT_DMA_P);
	if (rc) {
		pr_err("fail to get overlay buffer\n");
		return rc;
	}

	if (data.len < dma->source_config.stride * dma->source_config.height) {
		pr_err("buf size(0x%lx) is smaller than dma config(0x%x)\n",
			data.len, (dma->source_config.stride *
			dma->source_config.height));
		mdp3_put_img(&data, MDP3_CLIENT_DMA_P);
		return -EINVAL;
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

	if (mdp3_session->overlay.id == MSMFB_NEW_REQUEST) {
		pr_err("overlay play without overlay set first\n");
		mutex_unlock(&mdp3_session->lock);
		return -EINVAL;
	}

	if (mdss_fb_is_power_on(mfd))
		rc = mdp3_overlay_queue_buffer(mfd, req);
	else
		rc = -EPERM;

	mutex_unlock(&mdp3_session->lock);

	return rc;
}

bool update_roi(struct mdp3_rect oldROI, struct mdp_rect newROI)
{
	return ((newROI.x != oldROI.x) || (newROI.y != oldROI.y) ||
		(newROI.w != oldROI.w) || (newROI.h != oldROI.h));
}

bool is_roi_valid(struct mdp3_dma_source source_config, struct mdp_rect roi)
{
	return  (roi.w > 0) && (roi.h > 0) &&
		(roi.x >= source_config.x) &&
		((roi.x + roi.w) <= source_config.width) &&
		(roi.y >= source_config.y) &&
		((roi.y + roi.h) <= source_config.height);
}

static int mdp3_ctrl_display_commit_kickoff(struct msm_fb_data_type *mfd,
					struct mdp_display_commit *cmt_data)
{
	struct mdp3_session_data *mdp3_session;
	struct mdp3_img_data *data;
	struct mdss_panel_info *panel_info;
	int rc = 0;
	static bool splash_done;
	struct mdss_panel_data *panel;

	if (!mfd || !mfd->mdp.private1)
		return -EINVAL;

	panel_info = mfd->panel_info;
	mdp3_session = mfd->mdp.private1;
	if (!mdp3_session || !mdp3_session->dma)
		return -EINVAL;

	if (mdp3_bufq_count(&mdp3_session->bufq_in) == 0) {
		pr_debug("no buffer in queue yet\n");
		return -EPERM;
	}

	if (panel_info->partial_update_enabled &&
		is_roi_valid(mdp3_session->dma->source_config, cmt_data->l_roi)
		&& update_roi(mdp3_session->dma->roi, cmt_data->l_roi)) {
			mdp3_session->dma->roi.x = cmt_data->l_roi.x;
			mdp3_session->dma->roi.y = cmt_data->l_roi.y;
			mdp3_session->dma->roi.w = cmt_data->l_roi.w;
			mdp3_session->dma->roi.h = cmt_data->l_roi.h;
			mdp3_session->dma->update_src_cfg = true;
			pr_debug("%s: ROI: x=%d y=%d w=%d h=%d\n", __func__,
				mdp3_session->dma->roi.x,
				mdp3_session->dma->roi.y,
				mdp3_session->dma->roi.w,
				mdp3_session->dma->roi.h);
	}

	panel = mdp3_session->panel;
	if (mdp3_session->in_splash_screen ||
		mdp3_res->idle_pc) {
		pr_debug("%s: reset- in_splash = %d, idle_pc = %d", __func__,
			mdp3_session->in_splash_screen, mdp3_res->idle_pc);
		rc = mdp3_ctrl_reset(mfd);
		if (rc) {
			pr_err("fail to reset display\n");
			return -EINVAL;
		}
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
		if (mdp3_session->dma->update_src_cfg &&
				panel_info->partial_update_enabled) {
			panel->panel_info.roi.x = mdp3_session->dma->roi.x;
			panel->panel_info.roi.y = mdp3_session->dma->roi.y;
			panel->panel_info.roi.w = mdp3_session->dma->roi.w;
			panel->panel_info.roi.h = mdp3_session->dma->roi.h;
			rc = mdp3_session->dma->update(mdp3_session->dma,
					(void *)(int)data->addr,
					mdp3_session->intf, (void *)panel);
		} else {
			rc = mdp3_session->dma->update(mdp3_session->dma,
					(void *)(int)data->addr,
					mdp3_session->intf, NULL);
		}
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
		if (data)
			mdp3_put_img(data, MDP3_CLIENT_DMA_P);
	}

	if (mdp3_session->first_commit) {
		/*wait to ensure frame is sent to panel*/
		if (panel_info->mipi.init_delay)
			msleep(((1000 / panel_info->mipi.frame_rate) + 1) *
					panel_info->mipi.init_delay);
		else
			msleep(1000 / panel_info->mipi.frame_rate);
		mdp3_session->first_commit = false;
		if (panel)
			rc |= panel->event_handler(panel,
				MDSS_EVENT_POST_PANEL_ON, NULL);
	}

	mdp3_session->vsync_before_commit = 0;
	if (!splash_done || mdp3_session->esd_recovery == true) {
		if (panel && panel->set_backlight)
			panel->set_backlight(panel, panel->panel_info.bl_max);
		splash_done = true;
		mdp3_session->esd_recovery = false;
	}

	/* start vsync tick countdown for cmd mode if vsync isn't enabled */
	if (mfd->panel.type == MIPI_CMD_PANEL && !mdp3_session->vsync_enabled)
		mdp3_ctrl_vsync_enable(mdp3_session->mfd, 0);

	mutex_unlock(&mdp3_session->lock);

	mdss_fb_update_notify_update(mfd);

	return 0;
}

static int mdp3_map_pan_buff_immediate(struct msm_fb_data_type *mfd)
{
	int rc = 0;
	unsigned long length;
	dma_addr_t addr;
	int domain = mfd->mdp.fb_mem_get_iommu_domain();

	rc = mdss_smmu_map_dma_buf(mfd->fbmem_buf, mfd->fb_table, domain,
					&addr, &length, DMA_BIDIRECTIONAL);
	if (IS_ERR_VALUE(rc))
		goto err_unmap;
	else
		mfd->iova = addr;

	pr_debug("%s : smmu map dma buf VA: (%llx) MFD->iova %llx\n",
			__func__, (u64) addr, (u64) mfd->iova);
	return rc;

err_unmap:
	pr_err("smmu map dma buf failed: (%d)\n", rc);
	dma_buf_unmap_attachment(mfd->fb_attachment, mfd->fb_table,
			mdss_smmu_dma_data_direction(DMA_BIDIRECTIONAL));
	dma_buf_detach(mfd->fbmem_buf, mfd->fb_attachment);
	dma_buf_put(mfd->fbmem_buf);
	return rc;
}

static void mdp3_ctrl_pan_display(struct msm_fb_data_type *mfd)
{
	struct fb_info *fbi;
	struct mdp3_session_data *mdp3_session;
	u32 offset;
	int bpp;
	struct mdss_panel_info *panel_info;
	static bool splash_done;
	struct mdss_panel_data *panel;

	int rc;

	pr_debug("mdp3_ctrl_pan_display\n");
	if (!mfd || !mfd->mdp.private1)
		return;

	panel_info = mfd->panel_info;
	mdp3_session = (struct mdp3_session_data *)mfd->mdp.private1;
	if (!mdp3_session || !mdp3_session->dma)
		return;

	if (mdp3_session->in_splash_screen ||
		mdp3_res->idle_pc) {
		pr_debug("%s: reset- in_splash = %d, idle_pc = %d", __func__,
			mdp3_session->in_splash_screen, mdp3_res->idle_pc);
		rc = mdp3_ctrl_reset(mfd);
		if (rc) {
			pr_err("fail to reset display\n");
			return;
		}
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
		if (mdp3_session->first_commit) {
			rc = mdp3_map_pan_buff_immediate(mfd);
			if (IS_ERR_VALUE(rc))
				goto pan_error;
		}
		rc = mdp3_session->dma->update(mdp3_session->dma,
				(void *)(int)(mfd->iova + offset),
				mdp3_session->intf, NULL);
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

	panel = mdp3_session->panel;
	if (mdp3_session->first_commit) {
		/*wait to ensure frame is sent to panel*/
		if (panel_info->mipi.init_delay)
			msleep(((1000 / panel_info->mipi.frame_rate) + 1) *
					panel_info->mipi.init_delay);
		else
			msleep(1000 / panel_info->mipi.frame_rate);
		mdp3_session->first_commit = false;
		if (panel)
			panel->event_handler(panel, MDSS_EVENT_POST_PANEL_ON,
					NULL);
	}

	mdp3_session->vsync_before_commit = 0;
	if (!splash_done || mdp3_session->esd_recovery == true) {
		if (panel && panel->set_backlight)
			panel->set_backlight(panel, panel->panel_info.bl_max);
		splash_done = true;
		mdp3_session->esd_recovery = false;
	}


pan_error:
	mutex_unlock(&mdp3_session->lock);
}

static int mdp3_set_metadata(struct msm_fb_data_type *mfd,
				struct msmfb_metadata *metadata_ptr)
{
	int ret = 0;
	switch (metadata_ptr->op) {
	case metadata_op_crc:
		ret = mdp3_ctrl_res_req_clk(mfd, 1);
		if (ret) {
			pr_err("failed to turn on mdp clks\n");
			return ret;
		}
		ret = mdp3_misr_set(&metadata_ptr->data.misr_request);
		ret = mdp3_ctrl_res_req_clk(mfd, 0);
		if (ret) {
			pr_err("failed to release mdp clks\n");
			return ret;
		}
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
		metadata->data.caps.mdp_rev = 305;
		metadata->data.caps.rgb_pipes = 0;
		metadata->data.caps.vig_pipes = 0;
		metadata->data.caps.dma_pipes = 1;
		break;
	case metadata_op_crc:
		ret = mdp3_ctrl_res_req_clk(mfd, 1);
		if (ret) {
			pr_err("failed to turn on mdp clks\n");
			return ret;
		}
		ret = mdp3_misr_get(&metadata->data.misr_request);
		ret = mdp3_ctrl_res_req_clk(mfd, 0);
		if (ret) {
			pr_err("failed to release mdp clks\n");
			return ret;
		}
		break;
	case metadata_op_get_ion_fd:
		if (mfd->fb_ion_handle) {
			metadata->data.fbmem_ionfd =
					dma_buf_fd(mfd->fbmem_buf, 0);
			if (metadata->data.fbmem_ionfd < 0)
				pr_err("fd allocation failed. fd = %d\n",
						metadata->data.fbmem_ionfd);
		}
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
	bool mv_valid = false;
	for (i = 0; i < 9; i++) {
		if (data->csc_data.csc_mv[i] >=
				MDP_HISTOGRAM_CSC_MATRIX_MAX)
			return -EINVAL;
		if ((!mv_valid) && (data->csc_data.csc_mv[i] != 0))
			mv_valid = true;
	}
	if (!mv_valid) {
		pr_err("%s: black screen data! csc_mv is all 0s\n", __func__);
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

	mutex_lock(&session->lock);
	if (!session->status) {
		mutex_unlock(&session->lock);
		return -EPERM;
	}

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
		pr_info("mdp3_histogram_start already started\n");
		mutex_unlock(&session->histo_lock);
		return 0;
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
	mutex_unlock(&session->lock);
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
		pr_debug("mdp3_histogram_stop already stopped!");
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

	mutex_lock(&session->histo_lock);

	if (!session->histo_status) {
		pr_debug("mdp3_histogram_collect not started\n");
		mutex_unlock(&session->histo_lock);
		return -EPROTO;
	}

	mutex_unlock(&session->histo_lock);

	if (!session->clk_on) {
		pr_debug("mdp/dsi clock off currently\n");
		return -EPERM;
	}

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

	mutex_lock(&session->lock);
	mutex_lock(&session->dma->pp_lock);
	session->dma->cc_vect_sel = (session->dma->cc_vect_sel + 1) % 2;

	config.ccs_enable = 1;
	config.ccs_sel = session->dma->cc_vect_sel;
	config.pre_limit_sel = session->dma->cc_vect_sel;
	config.post_limit_sel = session->dma->cc_vect_sel;
	config.pre_bias_sel = session->dma->cc_vect_sel;
	config.post_bias_sel = session->dma->cc_vect_sel;
	config.ccs_dirty = true;

	ccs.mv = data->csc_data.csc_mv;
	ccs.pre_bv = data->csc_data.csc_pre_bv;
	ccs.post_bv = data->csc_data.csc_post_bv;
	ccs.pre_lv = data->csc_data.csc_pre_lv;
	ccs.post_lv = data->csc_data.csc_post_lv;

	/* cache one copy of setting for suspend/resume reconfiguring */
	session->dma->ccs_cache = *data;

	mdp3_clk_enable(1, 0);
	ret = session->dma->config_ccs(session->dma, &config, &ccs);
	mdp3_clk_enable(0, 0);
	mutex_unlock(&session->dma->pp_lock);
	mutex_unlock(&session->lock);
	return ret;
}

static int mdp3_pp_ioctl(struct msm_fb_data_type *mfd,
					void __user *argp)
{
	int ret = -EINVAL;
	struct msmfb_mdp_pp mdp_pp;
	struct mdp_lut_cfg_data *lut;
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
		/* Checking state of dyn_pu before programming CSC block */
		if (mdp3_session->dyn_pu_state) {
			pr_debug("Partial update feature is enabled.\n");
			return -EPERM;
		}
		ret = mdp3_validate_csc_data(&(mdp_pp.data.csc_cfg_data));
		if (ret) {
			pr_err("%s: invalid csc data\n", __func__);
			break;
		}
		ret = mdp3_csc_config(mdp3_session,
						&(mdp_pp.data.csc_cfg_data));
		break;
	case mdp_op_lut_cfg:
		lut = &mdp_pp.data.lut_cfg_data;
		if (lut->lut_type != mdp_lut_rgb) {
			pr_err("Lut type %d is not supported", lut->lut_type);
			return -EINVAL;
		}
		if (lut->data.rgb_lut_data.flags & MDP_PP_OPS_READ)
			ret = mdp3_ctrl_lut_read(mfd,
						&(lut->data.rgb_lut_data));
		else
			ret = mdp3_ctrl_lut_config(mfd,
						&(lut->data.rgb_lut_data));
		if (ret)
			pr_err("RGB LUT ioctl failed\n");
		else
			ret = copy_to_user(argp, &mdp_pp, sizeof(mdp_pp));
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

static int mdp3_validate_lut_data(struct fb_cmap *cmap)
{
	u32 i = 0;

	if (!cmap || !cmap->red || !cmap->green || !cmap->blue) {
		pr_err("Invalid arguments!\n");
		return -EINVAL;
	}

	for (i = 0; i < MDP_LUT_SIZE; i++) {
		if (cmap->red[i] > 0xFF || cmap->green[i] > 0xFF ||
			cmap->blue[i] > 0xFF) {
			pr_err("LUT value over 255 (limit) at %d index\n", i);
			return -EINVAL;
		}
	}

	return 0;
}

static inline int mdp3_copy_lut_buffer(struct fb_cmap *dst, struct fb_cmap *src)
{
	if (!dst || !src || !dst->red || !dst->blue || !dst->green ||
		!src->red || !src->green || !src->blue) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	dst->start = src->start;
	dst->len = src->len;

	memcpy(dst->red,   src->red,   MDP_LUT_SIZE * sizeof(u16));
	memcpy(dst->green, src->green, MDP_LUT_SIZE * sizeof(u16));
	memcpy(dst->blue,  src->blue,  MDP_LUT_SIZE * sizeof(u16));
	return 0;
}

static int mdp3_alloc_lut_buffer(struct platform_device *pdev, void **cmap)
{
	struct fb_cmap *map;

	map = devm_kzalloc(&pdev->dev, sizeof(struct fb_cmap), GFP_KERNEL);
	if (map == NULL) {
		pr_err("Failed memory allocation for cmap\n");
		return -ENOMEM;
	}
	memset(map, 0, sizeof(struct fb_cmap));

	map->red = devm_kzalloc(&pdev->dev, MDP_LUT_SIZE * sizeof(u16),
				GFP_KERNEL);
	if (map->red == NULL) {
		pr_err("Failed cmap allocation for red\n");
		goto exit_red;
	}
	memset(map->red, 0, sizeof(u16) * MDP_LUT_SIZE);

	map->green = devm_kzalloc(&pdev->dev, MDP_LUT_SIZE * sizeof(u16),
				GFP_KERNEL);
	if (map->green == NULL) {
		pr_err("Failed cmap allocation for green\n");
		goto exit_green;
	}
	memset(map->green, 0, sizeof(u16) * MDP_LUT_SIZE);

	map->blue = devm_kzalloc(&pdev->dev, MDP_LUT_SIZE * sizeof(u16),
				GFP_KERNEL);
	if (map->blue == NULL) {
		pr_err("Failed cmap allocation for blue\n");
		goto exit_blue;
	}
	memset(map->blue, 0, sizeof(u16) * MDP_LUT_SIZE);

	*cmap = map;
	return 0;
exit_blue:
	devm_kfree(&pdev->dev, map->green);
exit_green:
	devm_kfree(&pdev->dev, map->red);
exit_red:
	devm_kfree(&pdev->dev, map);
	return -ENOMEM;
}

static void mdp3_free_lut_buffer(struct platform_device *pdev, void **cmap)
{
	struct fb_cmap *map = (struct fb_cmap *)(*cmap);

	if (map == NULL)
		return;

	devm_kfree(&pdev->dev, map->blue);
	map->blue = NULL;
	devm_kfree(&pdev->dev, map->green);
	map->green = NULL;
	devm_kfree(&pdev->dev, map->red);
	map->red = NULL;
	devm_kfree(&pdev->dev, map);
	map = NULL;
}

static int mdp3_lut_combine_gain(struct fb_cmap *cmap, struct mdp3_dma *dma)
{
	int i = 0;
	u32 r = 0, g = 0, b = 0;

	if (!cmap || !dma || !dma->gc_cmap || !dma->hist_cmap ||
		!dma->gc_cmap->red || !dma->gc_cmap->green ||
		!dma->gc_cmap->blue || !dma->hist_cmap->red ||
		!dma->hist_cmap->green || !dma->hist_cmap->blue) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	for (i = 1; i < MDP_LUT_SIZE; i++) {
		r = MIN(dma->gc_cmap->red[i] * dma->hist_cmap->red[i] *
			mdp_lut_inverse16[i], 0xFF0000);
		g = MIN(dma->gc_cmap->green[i] * dma->hist_cmap->green[i] *
			mdp_lut_inverse16[i], 0xFF0000);
		b = MIN(dma->gc_cmap->blue[i] * dma->hist_cmap->blue[i] *
			mdp_lut_inverse16[i], 0xFF0000);

		cmap->red[i]   = (r >> 16) & 0xFF;
		cmap->green[i] = (g >> 16) & 0xFF;
		cmap->blue[i]  = (b >> 16) & 0xFF;
	}
	return 0;
}

/* Called from within pp_lock and session lock locked context */
static int mdp3_ctrl_lut_update(struct msm_fb_data_type *mfd,
				struct fb_cmap *cmap)
{
	int rc = 0;
	struct mdp3_session_data *mdp3_session = mfd->mdp.private1;
	struct mdp3_dma *dma;
	struct mdp3_dma_lut_config lut_config;

	dma = mdp3_session->dma;

	if (!dma->config_lut) {
		pr_err("Config LUT not defined!\n");
		return -EINVAL;
	}

	lut_config.lut_enable = 7;
	lut_config.lut_sel = mdp3_session->lut_sel;
	lut_config.lut_position = 1;
	lut_config.lut_dirty = true;

	if (!mdp3_session->status) {
		pr_err("display off!\n");
		return -EPERM;
	}

	mdp3_clk_enable(1, 0);
	rc = dma->config_lut(dma, &lut_config, cmap);
	mdp3_clk_enable(0, 0);
	if (rc)
		pr_err("mdp3_ctrl_lut_update failed\n");

	mdp3_session->lut_sel = (mdp3_session->lut_sel + 1) % 2;
	return rc;
}

static int mdp3_ctrl_lut_config(struct msm_fb_data_type *mfd,
				struct mdp_rgb_lut_data *cfg)
{
	int rc = 0;
	bool data_validated = false;
	struct mdp3_session_data *mdp3_session = mfd->mdp.private1;
	struct mdp3_dma *dma;
	struct fb_cmap *cmap;

	dma = mdp3_session->dma;

	if (cfg->cmap.start + cfg->cmap.len > MDP_LUT_SIZE) {
		pr_err("Invalid arguments\n");
		return  -EINVAL;
	}

	rc = mdp3_alloc_lut_buffer(mfd->pdev, (void **) &cmap);
	if (rc) {
		pr_err("No memory\n");
		return -ENOMEM;
	}

	mutex_lock(&mdp3_session->lock);
	mutex_lock(&dma->pp_lock);
	rc = copy_from_user(cmap->red + cfg->cmap.start,
			cfg->cmap.red, sizeof(u16) * cfg->cmap.len);
	rc |= copy_from_user(cmap->green + cfg->cmap.start,
			cfg->cmap.green, sizeof(u16) * cfg->cmap.len);
	rc |= copy_from_user(cmap->blue + cfg->cmap.start,
			cfg->cmap.blue, sizeof(u16) * cfg->cmap.len);
	if (rc) {
		pr_err("Copying user data failed!\n");
		goto exit_err;
	}

	switch (cfg->lut_type) {
	case mdp_rgb_lut_gc:
		if (cfg->flags & MDP_PP_OPS_DISABLE) {
			if (dma->lut_sts & MDP3_LUT_GC_EN)
				/* Free GC cmap cache since disabled */
				mdp3_free_lut_buffer(mfd->pdev,
						(void **)&dma->gc_cmap);
			dma->lut_sts &= ~MDP3_LUT_GC_EN;
		} else if (!(dma->lut_sts & MDP3_LUT_GC_EN)) {
			/* Check if values sent are valid */
			rc = mdp3_validate_lut_data(cmap);
			if (rc) {
				pr_err("Invalid GC LUT data\n");
				goto exit_err;
			}
			data_validated = true;

			/* Allocate GC cmap cache to store values */
			rc = mdp3_alloc_lut_buffer(mfd->pdev,
					(void **)&dma->gc_cmap);
			if (rc) {
				pr_err("GC LUT config failed\n");
				goto exit_err;
			}
			dma->lut_sts |= MDP3_LUT_GC_EN;
		}
		/*
		 * Copy the GC values from userspace to maintain the
		 * correct values user intended to program in cache.
		 * The values programmed in HW might factor in presence
		 * of other LUT modifying features hence can be
		 * different from these user given values.
		 */
		if (dma->lut_sts & MDP3_LUT_GC_EN) {
			/* Validate LUT data if not yet validated */
			if (!data_validated) {
				rc = mdp3_validate_lut_data(cmap);
				if (rc) {
					pr_err("Invalid GC LUT data\n");
					goto exit_err;
				}
			}
			rc = mdp3_copy_lut_buffer(dma->gc_cmap, cmap);
			if (rc) {
				pr_err("Could not store GC to cache\n");
				goto exit_err;
			}
		}
		break;
	case mdp_rgb_lut_hist:
		if (cfg->flags & MDP_PP_OPS_DISABLE) {
			if (dma->lut_sts & MDP3_LUT_HIST_EN)
				/* Free HIST cmap cache since disabled */
				mdp3_free_lut_buffer(mfd->pdev,
						(void **)&dma->hist_cmap);
			dma->lut_sts &= ~MDP3_LUT_HIST_EN;
		} else if (!(dma->lut_sts & MDP3_LUT_HIST_EN)) {
			/* Check if values sent are valid */
			rc = mdp3_validate_lut_data(cmap);
			if (rc) {
				pr_err("Invalid HIST LUT data\n");
				goto exit_err;
			}
			data_validated = true;

			/* Allocate HIST cmap cache to store values */
			rc = mdp3_alloc_lut_buffer(mfd->pdev,
					(void **)&dma->hist_cmap);
			if (rc) {
				pr_err("HIST LUT config failed\n");
				goto exit_err;
			}
			dma->lut_sts |= MDP3_LUT_HIST_EN;
		}
		/*
		 * Copy the HIST LUT values from userspace to maintain
		 * correct values user intended to program in cache.
		 * The values programmed in HW might factor in presence
		 * of other LUT modifying features hence can be
		 * different from these user given values.
		 */
		if (dma->lut_sts & MDP3_LUT_HIST_EN) {
			/* Validate LUT data if not yet validated */
			if (!data_validated) {
				rc = mdp3_validate_lut_data(cmap);
				if (rc) {
					pr_err("Invalid H LUT data\n");
					goto exit_err;
				}
			}
			rc = mdp3_copy_lut_buffer(dma->hist_cmap, cmap);
			if (rc) {
				pr_err("Could not cache Hist LUT\n");
				goto exit_err;
			}
		}
		break;
	default:
		pr_err("Invalid lut type: %u\n", cfg->lut_type);
		rc = -EINVAL;
		goto exit_err;
	}

	/*
	 * In case both GC LUT and HIST LUT need to be programmed the gains
	 * of each the individual LUTs need to be applied onto a single LUT
	 * and applied in HW
	 */
	if ((dma->lut_sts & MDP3_LUT_HIST_EN) &&
		(dma->lut_sts & MDP3_LUT_GC_EN)) {
		rc = mdp3_lut_combine_gain(cmap, dma);
		if (rc) {
			pr_err("Combining gains failed rc = %d\n", rc);
		goto exit_err;
	}
	}

	rc = mdp3_ctrl_lut_update(mfd, cmap);
	if (rc)
		pr_err("Updating LUT failed! rc = %d\n", rc);
exit_err:
	mutex_unlock(&dma->pp_lock);
	mutex_unlock(&mdp3_session->lock);
	mdp3_free_lut_buffer(mfd->pdev, (void **) &cmap);
	return rc;
}

static int mdp3_ctrl_lut_read(struct msm_fb_data_type *mfd,
				struct mdp_rgb_lut_data *cfg)
{
	int rc = 0;
	struct fb_cmap *cmap;
	struct mdp3_session_data *mdp3_session = mfd->mdp.private1;
	struct mdp3_dma *dma = mdp3_session->dma;

	switch (cfg->lut_type) {
	case mdp_rgb_lut_gc:
		if (!dma->gc_cmap) {
			pr_err("GC not programmed\n");
			return -EPERM;
		}
		cmap = dma->gc_cmap;
		break;
	case mdp_rgb_lut_hist:
		if (!dma->hist_cmap) {
			pr_err("Hist LUT not programmed\n");
			return -EPERM;
		}
		cmap = dma->hist_cmap;
		break;
	default:
		pr_err("Invalid lut type %u\n", cfg->lut_type);
		return -EINVAL;
	}

	cfg->cmap.start = cmap->start;
	cfg->cmap.len = cmap->len;

	mutex_lock(&dma->pp_lock);
	rc = copy_to_user(cfg->cmap.red, cmap->red, sizeof(u16) *
								MDP_LUT_SIZE);
	rc |= copy_to_user(cfg->cmap.green, cmap->green, sizeof(u16) *
								MDP_LUT_SIZE);
	rc |= copy_to_user(cfg->cmap.blue, cmap->blue, sizeof(u16) *
								MDP_LUT_SIZE);
	mutex_unlock(&dma->pp_lock);
	return rc;
}

/*  Invoked from ctrl_on with session lock locked context */
static void mdp3_ctrl_pp_resume(struct msm_fb_data_type *mfd)
{
	struct mdp3_session_data *mdp3_session;
	struct mdp3_dma *dma;
	struct fb_cmap *cmap;
	int rc = 0;

	mdp3_session = mfd->mdp.private1;
	dma = mdp3_session->dma;

	mutex_lock(&dma->pp_lock);
	/*
	 * if dma->ccs_config.ccs_enable is set then DMA PP block was enabled
	 * via user space IOCTL.
	 * Then set dma->ccs_config.ccs_dirty flag
	 * Then PP block will be reconfigured when next kickoff comes.
	 */
	if (dma->ccs_config.ccs_enable)
		dma->ccs_config.ccs_dirty = true;

	/*
	 * If gamma correction was enabled then we program the LUT registers
	 * with the last configuration data before suspend. If gamma correction
	 * is not enabled then we do not program anything. The LUT from
	 * histogram processing algorithms will program hardware based on new
	 * frame data if they are enabled.
	 */
	if (dma->lut_sts & MDP3_LUT_GC_EN) {

		rc = mdp3_alloc_lut_buffer(mfd->pdev, (void **)&cmap);
		if (rc) {
			pr_err("No memory for GC LUT, rc = %d\n", rc);
			goto exit_err;
		}

		if (dma->lut_sts & MDP3_LUT_HIST_EN) {
			rc = mdp3_lut_combine_gain(cmap, dma);
			if (rc) {
				pr_err("Combining the gain failed rc=%d\n", rc);
				goto exit_err;
			}
		} else {
			rc = mdp3_copy_lut_buffer(cmap, dma->gc_cmap);
			if (rc) {
				pr_err("Updating GC failed rc = %d\n", rc);
				goto exit_err;
			}
		}

		rc = mdp3_ctrl_lut_update(mfd, cmap);
		if (rc)
			pr_err("GC Lut update failed rc=%d\n", rc);
exit_err:
		mdp3_free_lut_buffer(mfd->pdev, (void **)&cmap);
	}

	mutex_unlock(&dma->pp_lock);
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

	if (copy_from_user(&req_list, ovlist.overlay_list,
				sizeof(struct mdp_overlay *)))
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
		cmd != MSMFB_HISTOGRAM_STOP && cmd != MSMFB_HISTOGRAM) {
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
		if (mdp3_session->in_splash_screen || mdp3_res->idle_pc) {
			pr_err("%s: reset- in_splash = %d, idle_pc = %d",
				__func__, mdp3_session->in_splash_screen,
				mdp3_res->idle_pc);
			mdp3_ctrl_reset(mfd);
		}
		rc = mdp3_ctrl_async_blit_req(mfd, argp);
		break;
	case MSMFB_BLIT:
		if (mdp3_session->in_splash_screen)
			mdp3_ctrl_reset(mfd);
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

static int mdp3_update_panel_info(struct msm_fb_data_type *mfd, int mode,
		int dest_ctrl)
{
	int ret = 0;
	struct mdp3_session_data *mdp3_session;
	struct mdss_panel_data *panel;
	u32 intf_type = 0;

	if (!mfd || !mfd->mdp.private1)
		return -EINVAL;

	mdp3_session = mfd->mdp.private1;
	panel = mdp3_session->panel;

	if (!panel->event_handler)
		return 0;
	ret = panel->event_handler(panel, MDSS_EVENT_DSI_UPDATE_PANEL_DATA,
						(void *)(unsigned long)mode);
	if (ret)
		pr_err("Dynamic switch to %s mode failed!\n",
					mode ? "command" : "video");
	if (mode == 1)
		mfd->panel.type = MIPI_CMD_PANEL;
	else
		mfd->panel.type = MIPI_VIDEO_PANEL;

	if (mfd->panel.type != MIPI_VIDEO_PANEL)
		mdp3_session->wait_for_dma_done = mdp3_wait_for_dma_done;

	intf_type = mdp3_ctrl_get_intf_type(mfd);
	mdp3_session->intf->cfg.type = intf_type;
	mdp3_session->intf->available = 1;
	mdp3_session->intf->in_use = 1;
	mdp3_res->intf[intf_type].in_use = 1;

	mdp3_intf_init(mdp3_session->intf);

	mdp3_session->dma->output_config.out_sel = intf_type;
	mdp3_session->status = mdp3_session->intf->active;

	return 0;
}

int mdp3_ctrl_init(struct msm_fb_data_type *mfd)
{
	struct device *dev = mfd->fbi->dev;
	struct msm_mdp_interface *mdp3_interface = &mfd->mdp;
	struct mdp3_session_data *mdp3_session = NULL;
	u32 intf_type = MDP3_DMA_OUTPUT_SEL_DSI_VIDEO;
	int rc;
	int splash_mismatch = 0;

	pr_info("mdp3_ctrl_init\n");
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
	mdp3_interface->lut_update = NULL;
	mdp3_interface->configure_panel = mdp3_update_panel_info;

	mdp3_session = kzalloc(sizeof(struct mdp3_session_data), GFP_KERNEL);
	if (!mdp3_session) {
		pr_err("fail to allocate mdp3 private data structure");
		return -ENOMEM;
	}
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
	rc = sysfs_create_group(&dev->kobj, &generic_attr_group);
	if (rc) {
		pr_err("generic sysfs group creation failed, ret=%d\n", rc);
		goto init_done;
	}

	mdp3_session->vsync_event_sd = sysfs_get_dirent(dev->kobj.sd,
							"vsync_event");
	if (!mdp3_session->vsync_event_sd) {
		pr_err("vsync_event sysfs lookup failed\n");
		rc = -ENODEV;
		goto init_done;
	}

	rc = mdp3_create_sysfs_link(dev);
	if (rc)
		pr_warn("problem creating link to mdp sysfs\n");

	/* Enable PM runtime */
	pm_runtime_set_suspended(&mdp3_res->pdev->dev);
	pm_runtime_enable(&mdp3_res->pdev->dev);

	kobject_uevent(&dev->kobj, KOBJ_ADD);
	pr_debug("vsync kobject_uevent(KOBJ_ADD)\n");

	if (mdp3_get_cont_spash_en()) {
		mdp3_session->clk_on = 1;
		mdp3_session->in_splash_screen = 1;
		mdp3_ctrl_notifier_register(mdp3_session,
			&mdp3_session->mfd->mdp_sync_pt_data.notifier);
	}

	/*
	* Increment the overlay active count.
	* This is needed to ensure that if idle power collapse kicks in
	* right away, it would be handled correctly.
	*/
	atomic_inc(&mdp3_res->active_intf_cnt);
	if (splash_mismatch) {
		pr_err("splash memory mismatch, stop splash\n");
		mdp3_ctrl_off(mfd);
	}

	mdp3_session->vsync_before_commit = true;
	mdp3_session->dyn_pu_state = mfd->panel_info->partial_update_enabled;
init_done:
	if (IS_ERR_VALUE(rc))
		kfree(mdp3_session);

	return rc;
}
