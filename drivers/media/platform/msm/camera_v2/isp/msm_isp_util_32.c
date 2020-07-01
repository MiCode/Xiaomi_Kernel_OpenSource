// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2013-2018, 2020 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/mutex.h>
#include <linux/io.h>
#include <media/v4l2-subdev.h>
#include <linux/ratelimit.h>

#include "msm.h"
#include "msm_isp_util_32.h"
#include "msm_isp_axi_util_32.h"
#include "msm_isp_stats_util_32.h"
#include "msm_camera_io_util.h"
#include "cam_smmu_api.h"

#ifndef UINT16_MAX
#define UINT16_MAX             (65535U)
#endif

#define MAX_ISP_V4l2_EVENTS 100
#define MAX_ISP_REG_LIST 100
static DEFINE_MUTEX(bandwidth_mgr_mutex);
static struct msm_isp_bandwidth_mgr isp_bandwidth_mgr;

static uint64_t msm_isp_cpp_clk_rate;

#define VFE40_8974V2_VERSION 0x1001001A
static struct msm_bus_vectors msm_isp_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
};

static struct msm_bus_vectors msm_isp_ping_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = MSM_ISP_MIN_AB,
		.ib  = MSM_ISP_MIN_IB,
	},
};

static struct msm_bus_vectors msm_isp_pong_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = MSM_ISP_MIN_AB,
		.ib  = MSM_ISP_MIN_IB,
	},
};

static struct msm_bus_paths msm_isp_bus_client_config[] = {
	{
		ARRAY_SIZE(msm_isp_init_vectors),
		msm_isp_init_vectors,
	},
	{
		ARRAY_SIZE(msm_isp_ping_vectors),
		msm_isp_ping_vectors,
	},
	{
		ARRAY_SIZE(msm_isp_pong_vectors),
		msm_isp_pong_vectors,
	},
};

static struct msm_bus_scale_pdata msm_isp_bus_client_pdata = {
	msm_isp_bus_client_config,
	NULL,
	ARRAY_SIZE(msm_isp_bus_client_config),
	.name = "msm_camera_isp",
	0,
};


void msm_camera_io_dump_2(void __iomem *addr, int size)
{
	char line_str[128], *p_str;
	int i;
	u32 __iomem *p = (u32 __iomem *) addr;
	u32 data;

	pr_err("%s: %pK %d\n", __func__, addr, size);
	line_str[0] = '\0';
	p_str = line_str;
	for (i = 0; i < size/4; i++) {
		if (i % 4 == 0) {
#ifdef CONFIG_COMPAT
			snprintf(p_str, 20, "%016lx: ", (unsigned long) p);
			p_str += 18;
#else
			snprintf(p_str, 12, "%08lx: ", (unsigned long) p);
			p_str += 10;
#endif
		}
		data = readl_relaxed(p++);
		snprintf(p_str, 12, "%08x ", data);
		p_str += 9;
		if ((i + 1) % 4 == 0) {
			pr_err("%s\n", line_str);
			line_str[0] = '\0';
			p_str = line_str;
		}
	}
	if (line_str[0] != '\0')
		pr_err("%s\n", line_str);
}

void msm_isp_print_fourcc_error(const char *origin, uint32_t fourcc_format)
{
	int i;
	char text[5];

	text[4] = '\0';
	for (i = 0; i < 4; i++) {
		text[i] = (char)(((fourcc_format) >> (i * 8)) & 0xFF);
		if ((text[i] < '0') || (text[i] > 'z')) {
			pr_err("%s: Invalid output format %d (unprintable)\n",
				origin, fourcc_format);
			return;
		}
	}
	pr_err("%s: Invalid output format %s\n",
		origin, text);
}

int msm_isp_init_bandwidth_mgr(enum msm_isp_hw_client client)
{
	int rc = 0;

	mutex_lock(&bandwidth_mgr_mutex);
	isp_bandwidth_mgr.client_info[client].active = 1;
	if (isp_bandwidth_mgr.use_count++) {
		mutex_unlock(&bandwidth_mgr_mutex);
		return rc;
	}
	isp_bandwidth_mgr.bus_client =
		msm_bus_scale_register_client(&msm_isp_bus_client_pdata);
	if (!isp_bandwidth_mgr.bus_client) {
		pr_err("%s: client register failed\n", __func__);
		mutex_unlock(&bandwidth_mgr_mutex);
		return -EINVAL;
	}

	isp_bandwidth_mgr.bus_vector_active_idx = 1;
	msm_bus_scale_client_update_request(
	   isp_bandwidth_mgr.bus_client,
	   isp_bandwidth_mgr.bus_vector_active_idx);

	mutex_unlock(&bandwidth_mgr_mutex);
	return 0;
}

int msm_isp_update_bandwidth(enum msm_isp_hw_client client,
	uint64_t ab, uint64_t ib)
{
	int i;
	struct msm_bus_paths *path;

	mutex_lock(&bandwidth_mgr_mutex);
	if (!isp_bandwidth_mgr.use_count ||
		!isp_bandwidth_mgr.bus_client) {
		pr_err("%s:error bandwidth manager inactive use_cnt:%d bus_clnt:%d\n",
			__func__, isp_bandwidth_mgr.use_count,
			isp_bandwidth_mgr.bus_client);
		return -EINVAL;
	}

	isp_bandwidth_mgr.client_info[client].ab = ab;
	isp_bandwidth_mgr.client_info[client].ib = ib;
	ALT_VECTOR_IDX(isp_bandwidth_mgr.bus_vector_active_idx);
	path =
		&(msm_isp_bus_client_pdata.usecase[
		  isp_bandwidth_mgr.bus_vector_active_idx]);
	path->vectors[0].ab = 0;
	path->vectors[0].ib = 0;
	for (i = 0; i < MAX_ISP_CLIENT; i++) {
		if (isp_bandwidth_mgr.client_info[i].active) {
			path->vectors[0].ab +=
				isp_bandwidth_mgr.client_info[i].ab;
			path->vectors[0].ib +=
				isp_bandwidth_mgr.client_info[i].ib;
		}
	}
	ISP_DBG("%s: Total AB = %llu IB = %llu\n", __func__,
			path->vectors[0].ab, path->vectors[0].ib);
	msm_bus_scale_client_update_request(isp_bandwidth_mgr.bus_client,
		isp_bandwidth_mgr.bus_vector_active_idx);
	/* Insert into circular buffer */
	msm_isp_update_req_history(isp_bandwidth_mgr.bus_client,
		path->vectors[0].ab,
		path->vectors[0].ib,
		isp_bandwidth_mgr.client_info,
		sched_clock());
	mutex_unlock(&bandwidth_mgr_mutex);
	return 0;
}

void msm_isp_deinit_bandwidth_mgr(enum msm_isp_hw_client client)
{
	if (client >= MAX_ISP_CLIENT) {
		pr_err("invalid Client id %d\n", client);
		return;
	}
	mutex_lock(&bandwidth_mgr_mutex);
	memset(&isp_bandwidth_mgr.client_info[client], 0,
		   sizeof(struct msm_isp_bandwidth_info));
	if (--isp_bandwidth_mgr.use_count) {
		mutex_unlock(&bandwidth_mgr_mutex);
		return;
	}

	if (!isp_bandwidth_mgr.bus_client) {
		pr_err("%s:%d error: bus client invalid\n", __func__, __LINE__);
		mutex_unlock(&bandwidth_mgr_mutex);
		return;
	}

	msm_bus_scale_client_update_request(
	   isp_bandwidth_mgr.bus_client, 0);
	msm_bus_scale_unregister_client(isp_bandwidth_mgr.bus_client);
	isp_bandwidth_mgr.bus_client = 0;
	mutex_unlock(&bandwidth_mgr_mutex);
}

void msm_isp_util_get_bandwidth_stats(struct vfe_device *vfe_dev,
				      struct msm_isp_statistics *stats)
{
	stats->isp_vfe0_active = isp_bandwidth_mgr.client_info[ISP_VFE0].active;
	stats->isp_vfe0_ab = isp_bandwidth_mgr.client_info[ISP_VFE0].ab;
	stats->isp_vfe0_ib = isp_bandwidth_mgr.client_info[ISP_VFE0].ib;

	stats->isp_vfe1_active = isp_bandwidth_mgr.client_info[ISP_VFE1].active;
	stats->isp_vfe1_ab = isp_bandwidth_mgr.client_info[ISP_VFE1].ab;
	stats->isp_vfe1_ib = isp_bandwidth_mgr.client_info[ISP_VFE1].ib;

	stats->isp_cpp_active = isp_bandwidth_mgr.client_info[ISP_CPP].active;
	stats->isp_cpp_ab = isp_bandwidth_mgr.client_info[ISP_CPP].ab;
	stats->isp_cpp_ib = isp_bandwidth_mgr.client_info[ISP_CPP].ib;
	stats->last_overflow_ab = vfe_dev->msm_isp_last_overflow_ab;
	stats->last_overflow_ib = vfe_dev->msm_isp_last_overflow_ib;
	stats->vfe_clk_rate = vfe_dev->msm_isp_vfe_clk_rate;
	stats->cpp_clk_rate = msm_isp_cpp_clk_rate;
}

void msm_isp_util_update_last_overflow_ab_ib(struct vfe_device *vfe_dev)
{
	struct msm_bus_paths *path;
	 path = &(msm_isp_bus_client_pdata.usecase[
		  isp_bandwidth_mgr.bus_vector_active_idx]);
	vfe_dev->msm_isp_last_overflow_ab = path->vectors[0].ab;
	vfe_dev->msm_isp_last_overflow_ib = path->vectors[0].ib;
}

void msm_isp_util_update_clk_rate(long clock_rate)
{
	msm_isp_cpp_clk_rate = clock_rate;
}

uint32_t msm_isp_get_framedrop_period(
	enum msm_vfe_frame_skip_pattern frame_skip_pattern)
{
	switch (frame_skip_pattern) {
	case NO_SKIP:
	case EVERY_2FRAME:
	case EVERY_3FRAME:
	case EVERY_4FRAME:
	case EVERY_5FRAME:
	case EVERY_6FRAME:
	case EVERY_7FRAME:
	case EVERY_8FRAME:
		return frame_skip_pattern + 1;
	case EVERY_16FRAME:
		return 16;
	case EVERY_32FRAME:
		return 32;
	case SKIP_ALL:
		return 1;
	default:
		return 1;
	}
	return 1;
}

int msm_isp_get_clk_info(struct vfe_device *vfe_dev,
	struct platform_device *pdev, struct msm_cam_clk_info *vfe_clk_info)
{
	int i, count, rc;
	uint32_t rates[VFE_CLK_INFO_MAX];

	struct device_node *of_node;

	of_node = pdev->dev.of_node;

	count = of_property_count_strings(of_node, "clock-names");

	ISP_DBG("count = %d\n", count);
	if (count <= 0) {
		pr_err("no clocks found in device tree, count=%d\n", count);
		return 0;
	}

	if (count > VFE_CLK_INFO_MAX) {
		pr_err("invalid count=%d, max is %d\n", count,
			VFE_CLK_INFO_MAX);
		return -EINVAL;
	}

	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node, "clock-names",
				i, &(vfe_clk_info[i].clk_name));
		ISP_DBG("clock-names[%d] = %s\n", i, vfe_clk_info[i].clk_name);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			return rc;
		}
	}
	rc = of_property_read_u32_array(of_node, "qcom,clock-rates",
		rates, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return rc;
	}
	for (i = 0; i < count; i++) {
		vfe_clk_info[i].clk_rate =
			(rates[i] == 0) ? (long)-1 : rates[i];
		ISP_DBG("clk_rate[%d] = %ld\n", i, vfe_clk_info[i].clk_rate);
	}
	vfe_dev->num_clk = count;
	return 0;
}

void msm_isp_get_timestamp(struct msm_isp_timestamp *time_stamp,
	struct vfe_device *vfe_dev)
{
	struct timespec ts;

	do_gettimeofday(&(time_stamp->event_time));
	if (vfe_dev->vt_enable) {
		msm_isp_get_avtimer_ts(time_stamp);
		time_stamp->buf_time.tv_sec    = time_stamp->vt_time.tv_sec;
		time_stamp->buf_time.tv_usec   = time_stamp->vt_time.tv_usec;
	} else {
		get_monotonic_boottime(&ts);
		time_stamp->buf_time.tv_sec    = ts.tv_sec;
		time_stamp->buf_time.tv_usec   = ts.tv_nsec/1000;
	}
}

int msm_isp_subscribe_event(struct v4l2_subdev *sd, struct v4l2_fh *fh,
	struct v4l2_event_subscription *sub)
{
	struct vfe_device *vfe_dev = v4l2_get_subdevdata(sd);
	int rc = 0;

	rc = v4l2_event_subscribe(fh, sub, MAX_ISP_V4l2_EVENTS, NULL);
	if (rc == 0) {
		if (sub->type == V4L2_EVENT_ALL) {
			int i;

			vfe_dev->axi_data.event_mask = 0;
			for (i = 0; i < ISP_EVENT_MAX; i++)
				vfe_dev->axi_data.event_mask |= (1 << i);
		} else {
			int event_idx = sub->type - ISP_EVENT_BASE;

			vfe_dev->axi_data.event_mask |= (1 << event_idx);
		}
	}
	return rc;
}

int msm_isp_unsubscribe_event(struct v4l2_subdev *sd, struct v4l2_fh *fh,
	struct v4l2_event_subscription *sub)
{
	struct vfe_device *vfe_dev = v4l2_get_subdevdata(sd);
	int rc = 0;

	rc = v4l2_event_unsubscribe(fh, sub);
	if (sub->type == V4L2_EVENT_ALL) {
		vfe_dev->axi_data.event_mask = 0;
	} else {
		int event_idx = sub->type - ISP_EVENT_BASE;

		vfe_dev->axi_data.event_mask &= ~(1 << event_idx);
	}
	return rc;
}

static int msm_isp_get_max_clk_rate(struct vfe_device *vfe_dev, long *rate)
{
	int           clk_idx = 0;
	unsigned long max_value = ~0;
	long          round_rate = 0;

	if (!vfe_dev || !rate) {
		pr_err("%s:%d failed: vfe_dev %pK rate %pK\n",
			__func__, __LINE__,	vfe_dev, rate);
		return -EINVAL;
	}

	*rate = 0;
	if (!vfe_dev->hw_info) {
		pr_err("%s:%d failed: vfe_dev->hw_info %pK\n", __func__,
			__LINE__, vfe_dev->hw_info);
		return -EINVAL;
	}

	clk_idx = vfe_dev->hw_info->vfe_clk_idx;
	if (clk_idx >= vfe_dev->num_clk) {
		pr_err("%s:%d failed: clk_idx %d max array size %d\n",
			__func__, __LINE__, clk_idx,
			vfe_dev->num_clk);
		return -EINVAL;
	}

	round_rate = clk_round_rate(vfe_dev->vfe_clk[clk_idx], max_value);
	if (round_rate < 0) {
		pr_err("%s: Invalid vfe clock rate\n", __func__);
		return -EINVAL;
	}

	*rate = round_rate;
	return 0;
}

static int msm_isp_set_clk_rate(struct vfe_device *vfe_dev, long *rate)
{
	int rc = 0;
	int clk_idx = vfe_dev->hw_info->vfe_clk_idx;
	long round_rate =
		clk_round_rate(vfe_dev->vfe_clk[clk_idx], *rate);
	if (round_rate < 0) {
		pr_err("%s: Invalid vfe clock rate\n", __func__);
		return round_rate;
	}

	rc = clk_set_rate(vfe_dev->vfe_clk[clk_idx], round_rate);
	if (rc < 0) {
		pr_err("%s: Vfe set rate error\n", __func__);
		return rc;
	}
	*rate = round_rate;
	vfe_dev->msm_isp_vfe_clk_rate = round_rate;
	return 0;
}

void msm_isp_fetch_engine_done_notify(struct vfe_device *vfe_dev,
	struct msm_vfe_fetch_engine_info *fetch_engine_info)
{
	struct msm_isp32_event_data fe_rd_done_event;

	if (!fetch_engine_info->is_busy)
		return;
	memset(&fe_rd_done_event, 0, sizeof(struct msm_isp32_event_data));
	fe_rd_done_event.frame_id =
		vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id;
	fe_rd_done_event.u.buf_done.session_id = fetch_engine_info->session_id;
	fe_rd_done_event.u.buf_done.stream_id = fetch_engine_info->stream_id;
	fe_rd_done_event.u.buf_done.handle = fetch_engine_info->bufq_handle;
	fe_rd_done_event.u.buf_done.buf_idx = fetch_engine_info->buf_idx;
	ISP_DBG("%s:VFE%d ISP_EVENT_FE_READ_DONE buf_idx %d\n",
		__func__, vfe_dev->pdev->id, fetch_engine_info->buf_idx);
	fetch_engine_info->is_busy = 0;
	msm_isp_send_event(vfe_dev, ISP_EVENT_FE_READ_DONE, &fe_rd_done_event);
}

static int msm_isp_cfg_pix(struct vfe_device *vfe_dev,
	struct msm_vfe_input_cfg *input_cfg)
{
	int rc = 0;

	if (vfe_dev->axi_data.src_info[VFE_PIX_0].active) {
		pr_err("%s: src %d path is active\n", __func__, VFE_PIX_0);
		return -EINVAL;
	}

	vfe_dev->axi_data.src_info[VFE_PIX_0].pixel_clock =
		input_cfg->input_pix_clk;
	vfe_dev->axi_data.src_info[VFE_PIX_0].input_mux =
		input_cfg->d.pix_cfg.input_mux;
	vfe_dev->axi_data.src_info[VFE_PIX_0].input_format =
		input_cfg->d.pix_cfg.input_format;

	rc = msm_isp_set_clk_rate(vfe_dev,
		&vfe_dev->axi_data.src_info[VFE_PIX_0].pixel_clock);
	if (rc < 0) {
		pr_err("%s: clock set rate failed\n", __func__);
		return rc;
	}

	ISP_DBG("%s: input mux is %d CAMIF %d io_format 0x%x\n", __func__,
		input_cfg->d.pix_cfg.input_mux, CAMIF,
		input_cfg->d.pix_cfg.input_format);

	if (input_cfg->d.pix_cfg.input_mux == CAMIF) {
		vfe_dev->axi_data.src_info[VFE_PIX_0].width =
			input_cfg->d.pix_cfg.camif_cfg.pixels_per_line;
	} else if (input_cfg->d.pix_cfg.input_mux == EXTERNAL_READ) {
		vfe_dev->axi_data.src_info[VFE_PIX_0].width =
			input_cfg->d.pix_cfg.fetch_engine_cfg.buf_stride;
	}
	vfe_dev->hw_info->vfe_ops.core_ops.cfg_input_mux(
			vfe_dev, &input_cfg->d.pix_cfg);
	return rc;
}

static int msm_isp_cfg_rdi(struct vfe_device *vfe_dev,
	struct msm_vfe_input_cfg *input_cfg)
{
	int rc = 0;

	if (vfe_dev->axi_data.src_info[input_cfg->input_src].active) {
		pr_err("%s: RAW%d path is active\n", __func__,
			   input_cfg->input_src - VFE_RAW_0);
		return -EINVAL;
	}

	vfe_dev->axi_data.src_info[input_cfg->input_src].pixel_clock =
		input_cfg->input_pix_clk;
	vfe_dev->hw_info->vfe_ops.core_ops.cfg_rdi_reg(
		vfe_dev, &input_cfg->d.rdi_cfg, input_cfg->input_src);
	return rc;
}

int msm_isp_cfg_input(struct vfe_device *vfe_dev, void *arg)
{
	int rc = 0;
	struct msm_vfe_input_cfg *input_cfg = arg;

	switch (input_cfg->input_src) {
	case VFE_PIX_0:
		rc = msm_isp_cfg_pix(vfe_dev, input_cfg);
		break;
	case VFE_RAW_0:
	case VFE_RAW_1:
	case VFE_RAW_2:
		rc = msm_isp_cfg_rdi(vfe_dev, input_cfg);
		break;
	default:
		pr_err("%s: Invalid input source\n", __func__);
		rc = -EINVAL;
	}
	return rc;
}

static int msm_isp_proc_cmd_list_unlocked(struct vfe_device *vfe_dev, void *arg)
{
	int rc = 0;
	uint32_t count = 0;
	struct msm_vfe_cfg_cmd_list *proc_cmd =
		(struct msm_vfe_cfg_cmd_list *)arg;
	struct msm_vfe_cfg_cmd_list cmd, cmd_next;
	struct msm_vfe_cfg_cmd2 cfg_cmd;

	if (!vfe_dev || !arg) {
		pr_err("%s:%d failed: vfe_dev %pK arg %pK\n",
			__func__, __LINE__, vfe_dev, arg);
		return -EINVAL;
	}

	rc = msm_isp_proc_cmd(vfe_dev, &proc_cmd->cfg_cmd);
	if (rc < 0)
		pr_err("%s:%d failed: rc %d\n", __func__, __LINE__, rc);

	cmd = *proc_cmd;

	while (cmd.next) {
		if (cmd.next_size != sizeof(struct msm_vfe_cfg_cmd_list)) {
			pr_err("%s:%d failed: next size %u != expected %zu\n",
				__func__, __LINE__, cmd.next_size,
				sizeof(struct msm_vfe_cfg_cmd_list));
			break;
		}
		if (++count >= MAX_ISP_REG_LIST) {
			pr_err("%s:%d Error exceeding the max register count:%u\n",
				__func__, __LINE__, count);
			rc = -EFAULT;
			break;
		}
		if (copy_from_user(&cmd_next, (void __user *)cmd.next,
			sizeof(struct msm_vfe_cfg_cmd_list))) {
			rc = -EFAULT;
			continue;
		}

		cfg_cmd = cmd_next.cfg_cmd;

		rc = msm_isp_proc_cmd(vfe_dev, &cfg_cmd);
		if (rc < 0)
			pr_err("%s:%d failed: rc %d\n", __func__, __LINE__, rc);

		cmd = cmd_next;
	}
	return rc;
}

#ifdef CONFIG_COMPAT
struct msm_vfe_cfg_cmd2_32 {
	uint16_t num_cfg;
	uint16_t cmd_len;
	compat_caddr_t cfg_data;
	compat_caddr_t cfg_cmd;
};

struct msm_vfe_cfg_cmd_list_32 {
	struct msm_vfe_cfg_cmd2_32   cfg_cmd;
	compat_caddr_t               next;
	uint32_t                     next_size;
};

#define VIDIOC_MSM_VFE_REG_CFG_COMPAT \
	_IOWR('V', BASE_VIDIOC_PRIVATE, struct msm_vfe_cfg_cmd2_32)
#define VIDIOC_MSM_VFE_REG_LIST_CFG_COMPAT \
	_IOWR('V', BASE_VIDIOC_PRIVATE+14, struct msm_vfe_cfg_cmd_list_32)

static void msm_isp_compat_to_proc_cmd(struct msm_vfe_cfg_cmd2 *proc_cmd,
	struct msm_vfe_cfg_cmd2_32 *proc_cmd_ptr32)
{
	proc_cmd->num_cfg = proc_cmd_ptr32->num_cfg;
	proc_cmd->cmd_len = proc_cmd_ptr32->cmd_len;
	proc_cmd->cfg_data = compat_ptr(proc_cmd_ptr32->cfg_data);
	proc_cmd->cfg_cmd = compat_ptr(proc_cmd_ptr32->cfg_cmd);
}

static int msm_isp_proc_cmd_list_compat(struct vfe_device *vfe_dev, void *arg)
{
	int rc = 0;
	uint32_t count = 0;
	struct msm_vfe_cfg_cmd_list_32 *proc_cmd =
		(struct msm_vfe_cfg_cmd_list_32 *)arg;
	struct msm_vfe_cfg_cmd_list_32 cmd, cmd_next;
	struct msm_vfe_cfg_cmd2 current_cmd;

	if (!vfe_dev || !arg) {
		pr_err("%s:%d failed: vfe_dev %pK arg %pK\n",
			__func__, __LINE__, vfe_dev, arg);
		return -EINVAL;
	}
	msm_isp_compat_to_proc_cmd(&current_cmd, &proc_cmd->cfg_cmd);
	rc = msm_isp_proc_cmd(vfe_dev, &current_cmd);
	if (rc < 0)
		pr_err("%s:%d failed: rc %d\n", __func__, __LINE__, rc);

	cmd = *proc_cmd;

	while (compat_ptr(cmd.next) != NULL) {
		if (cmd.next_size != sizeof(struct msm_vfe_cfg_cmd_list_32)) {
			pr_err("%s:%d failed: next size %u != expected %zu\n",
				__func__, __LINE__, cmd.next_size,
				sizeof(struct msm_vfe_cfg_cmd_list));
			break;
		}
		if (++count >= MAX_ISP_REG_LIST) {
			pr_err("%s:%d Error exceeding the max register count:%u\n",
				__func__, __LINE__, count);
			rc = -EFAULT;
			break;
		}
		if (copy_from_user(&cmd_next, compat_ptr(cmd.next),
			sizeof(struct msm_vfe_cfg_cmd_list_32))) {
			rc = -EFAULT;
			continue;
		}

		msm_isp_compat_to_proc_cmd(&current_cmd, &cmd_next.cfg_cmd);
		rc = msm_isp_proc_cmd(vfe_dev, &current_cmd);
		if (rc < 0)
			pr_err("%s:%d failed: rc %d\n", __func__, __LINE__, rc);

		cmd = cmd_next;
	}
	return rc;
}

static int msm_isp_proc_cmd_list(struct vfe_device *vfe_dev, void *arg)
{
	if (is_compat_task())
		return msm_isp_proc_cmd_list_compat(vfe_dev, arg);
	else
		return msm_isp_proc_cmd_list_unlocked(vfe_dev, arg);
}
#else /* CONFIG_COMPAT */
static int msm_isp_proc_cmd_list(struct vfe_device *vfe_dev, void *arg)
{
	return msm_isp_proc_cmd_list_unlocked(vfe_dev, arg);
}
#endif /* CONFIG_COMPAT */


static long msm_isp_ioctl_unlocked(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	long rc = 0;
	struct vfe_device *vfe_dev = v4l2_get_subdevdata(sd);

	if (!vfe_dev || !vfe_dev->vfe_base) {
		pr_err("%s:%d failed: invalid params %pK\n",
			__func__, __LINE__, vfe_dev);
		if (vfe_dev)
			pr_err("%s:%d failed %pK\n", __func__,
				__LINE__, vfe_dev->vfe_base);
		return -EINVAL;
	}

	/* use real time mutex for hard real-time ioctls such as
	 * buffer operations and register updates.
	 * Use core mutex for other ioctls that could take
	 * longer time to complete such as start/stop ISP streams
	 * which blocks until the hardware start/stop streaming
	 */
	ISP_DBG("%s cmd: %d\n", __func__, _IOC_TYPE(cmd));
	switch (cmd) {
	case VIDIOC_MSM_VFE_REG_CFG: {
		mutex_lock(&vfe_dev->realtime_mutex);
		rc = msm_isp_proc_cmd(vfe_dev, arg);
		mutex_unlock(&vfe_dev->realtime_mutex);
		break;
	}
	case VIDIOC_MSM_VFE_REG_LIST_CFG: {
		mutex_lock(&vfe_dev->realtime_mutex);
		rc = msm_isp_proc_cmd_list(vfe_dev, arg);
		mutex_unlock(&vfe_dev->realtime_mutex);
		break;
	}
	case VIDIOC_MSM_ISP_REQUEST_BUF:
	case VIDIOC_MSM_ISP_ENQUEUE_BUF:
	case VIDIOC_MSM_ISP_RELEASE_BUF: {
		mutex_lock(&vfe_dev->core_mutex);
		rc = msm_isp_proc_buf_cmd(vfe_dev->buf_mgr, cmd, arg);
		mutex_unlock(&vfe_dev->core_mutex);
		break;
	}
	case VIDIOC_MSM_ISP32_REQUEST_STREAM:
		mutex_lock(&vfe_dev->core_mutex);
		rc = msm_isp_request_axi_stream(vfe_dev, arg);
		mutex_unlock(&vfe_dev->core_mutex);
		break;
	case VIDIOC_MSM_ISP_RELEASE_STREAM:
		mutex_lock(&vfe_dev->core_mutex);
		rc = msm_isp_release_axi_stream(vfe_dev, arg);
		mutex_unlock(&vfe_dev->core_mutex);
		break;
	case VIDIOC_MSM_ISP_CFG_STREAM:
		mutex_lock(&vfe_dev->core_mutex);
		rc = msm_isp_cfg_axi_stream(vfe_dev, arg);
		mutex_unlock(&vfe_dev->core_mutex);
		break;
	case VIDIOC_MSM_ISP_AXI_HALT:
		mutex_lock(&vfe_dev->core_mutex);
		rc = msm_isp_axi_halt(vfe_dev, arg);
		mutex_unlock(&vfe_dev->core_mutex);
		break;
	case VIDIOC_MSM_ISP_AXI_RESET:
		mutex_lock(&vfe_dev->core_mutex);
		rc = msm_isp_axi_reset(vfe_dev, arg);
		mutex_unlock(&vfe_dev->core_mutex);
		break;
	case VIDIOC_MSM_ISP_AXI_RESTART:
		mutex_lock(&vfe_dev->core_mutex);
		rc = msm_isp_axi_restart(vfe_dev, arg);
		mutex_unlock(&vfe_dev->core_mutex);
		break;
	case VIDIOC_MSM_ISP_INPUT_CFG:
		mutex_lock(&vfe_dev->core_mutex);
		rc = msm_isp_cfg_input(vfe_dev, arg);
		mutex_unlock(&vfe_dev->core_mutex);
		break;
	case VIDIOC_MSM_ISP_FETCH_ENG_START:
		mutex_lock(&vfe_dev->core_mutex);
		rc = vfe_dev->hw_info->vfe_ops.core_ops.start_fetch_eng(
			vfe_dev, arg);
		mutex_unlock(&vfe_dev->core_mutex);
		break;
	case VIDIOC_MSM_ISP_REG_UPDATE_CMD:
		if (arg) {
			enum msm_vfe_input_src frame_src =
				*((enum msm_vfe_input_src *)arg);
			vfe_dev->hw_info->vfe_ops.core_ops.reg_update(vfe_dev,
				(1 << frame_src));
			vfe_dev->axi_data.src_info[frame_src].last_updt_frm_id =
			  vfe_dev->axi_data.src_info[frame_src].frame_id;
		}
		break;
	case VIDIOC_MSM_ISP_SET_SRC_STATE:
		mutex_lock(&vfe_dev->core_mutex);
		rc = msm_isp_set_src_state(vfe_dev, arg);
		mutex_unlock(&vfe_dev->core_mutex);
		break;
	case VIDIOC_MSM_ISP_REQUEST_STATS_STREAM:
		mutex_lock(&vfe_dev->core_mutex);
		rc = msm_isp_request_stats_stream(vfe_dev, arg);
		mutex_unlock(&vfe_dev->core_mutex);
		break;
	case VIDIOC_MSM_ISP_RELEASE_STATS_STREAM:
		mutex_lock(&vfe_dev->core_mutex);
		rc = msm_isp_release_stats_stream(vfe_dev, arg);
		mutex_unlock(&vfe_dev->core_mutex);
		break;
	case VIDIOC_MSM_ISP_CFG_STATS_STREAM:
		mutex_lock(&vfe_dev->core_mutex);
		rc = msm_isp_cfg_stats_stream(vfe_dev, arg);
		mutex_unlock(&vfe_dev->core_mutex);
		break;
	case VIDIOC_MSM_ISP_UPDATE_STATS_STREAM:
		mutex_lock(&vfe_dev->core_mutex);
		rc = msm_isp_update_stats_stream(vfe_dev, arg);
		mutex_unlock(&vfe_dev->core_mutex);
		break;
	case VIDIOC_MSM_ISP_UPDATE_STREAM:
		mutex_lock(&vfe_dev->core_mutex);
		rc = msm_isp_update_axi_stream(vfe_dev, arg);
		mutex_unlock(&vfe_dev->core_mutex);
		break;
	case VIDIOC_MSM_ISP_SMMU_ATTACH:
		mutex_lock(&vfe_dev->core_mutex);
		rc = msm_isp_smmu_attach(vfe_dev->buf_mgr, arg);
		mutex_unlock(&vfe_dev->core_mutex);
		break;
	case MSM_SD_NOTIFY_FREEZE:
		vfe_dev->isp_sof_debug = 0;
		break;
	case VIDIOC_MSM_ISP_BUF_DONE:
		mutex_lock(&vfe_dev->core_mutex);
		rc = msm_isp_user_buf_done(vfe_dev, arg);
		mutex_unlock(&vfe_dev->core_mutex);
		break;
	case MSM_SD_SHUTDOWN:
		while (vfe_dev->vfe_open_cnt != 0)
			msm_isp_close_node(sd, NULL);
		break;

	default:
		pr_err_ratelimited("%s: Invalid ISP command\n", __func__);
		rc = -EINVAL;
	}
	return rc;
}


#ifdef CONFIG_COMPAT
static long msm_isp_ioctl_compat(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	struct vfe_device *vfe_dev = v4l2_get_subdevdata(sd);
	long rc = 0;

	if (!vfe_dev || !vfe_dev->vfe_base) {
		pr_err("%s:%d failed: invalid params %pK\n",
			__func__, __LINE__, vfe_dev);
		if (vfe_dev)
			pr_err("%s:%d failed %pK\n", __func__,
				__LINE__, vfe_dev->vfe_base);
		return -EINVAL;
	}

	switch (cmd) {
	case VIDIOC_MSM_VFE_REG_CFG_COMPAT: {
		struct msm_vfe_cfg_cmd2 proc_cmd;

		mutex_lock(&vfe_dev->realtime_mutex);
		msm_isp_compat_to_proc_cmd(&proc_cmd,
			(struct msm_vfe_cfg_cmd2_32 *) arg);
		rc = msm_isp_proc_cmd(vfe_dev, &proc_cmd);
		mutex_unlock(&vfe_dev->realtime_mutex);
		break;
	}
	case VIDIOC_MSM_VFE_REG_LIST_CFG_COMPAT: {
		mutex_lock(&vfe_dev->realtime_mutex);
		rc = msm_isp_proc_cmd_list(vfe_dev, arg);
		mutex_unlock(&vfe_dev->realtime_mutex);
		break;
	}
	default:
		return msm_isp_ioctl_unlocked(sd, cmd, arg);
	}

	return rc;
}

long msm_isp_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	return msm_isp_ioctl_compat(sd, cmd, arg);
}
#else /* CONFIG_COMPAT */
long msm_isp_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	return msm_isp_ioctl_unlocked(sd, cmd, arg);
}
#endif /* CONFIG_COMPAT */

static int msm_isp_send_hw_cmd(struct vfe_device *vfe_dev,
	struct msm_vfe_reg_cfg_cmd *reg_cfg_cmd,
	uint32_t *cfg_data, uint32_t cmd_len)
{
	if (!vfe_dev || !reg_cfg_cmd) {
		pr_err("%s:%d failed: vfe_dev %pK reg_cfg_cmd %pK\n", __func__,
			__LINE__, vfe_dev, reg_cfg_cmd);
		return -EINVAL;
	}
	if ((reg_cfg_cmd->cmd_type != VFE_CFG_MASK) &&
		(!cfg_data || !cmd_len)) {
		pr_err("%s:%d failed: cmd type %d cfg_data %pK cmd_len %d\n",
			__func__, __LINE__, reg_cfg_cmd->cmd_type, cfg_data,
			cmd_len);
		return -EINVAL;
	}

	/* Validate input parameters */
	switch (reg_cfg_cmd->cmd_type) {
	case VFE_WRITE:
	case VFE_READ:
	case VFE_WRITE_MB: {
		if ((reg_cfg_cmd->u.rw_info.reg_offset >
			(UINT_MAX - reg_cfg_cmd->u.rw_info.len)) ||
			((reg_cfg_cmd->u.rw_info.reg_offset +
			reg_cfg_cmd->u.rw_info.len) >
			resource_size(vfe_dev->vfe_mem)) ||
			(reg_cfg_cmd->u.rw_info.reg_offset & 0x3)) {
			pr_err("%s:%d reg_offset %d len %d res %d\n",
				__func__, __LINE__,
				reg_cfg_cmd->u.rw_info.reg_offset,
				reg_cfg_cmd->u.rw_info.len,
				(uint32_t)resource_size(vfe_dev->vfe_mem));
			return -EINVAL;
		}

		if ((reg_cfg_cmd->u.rw_info.cmd_data_offset >
			(UINT_MAX - reg_cfg_cmd->u.rw_info.len)) ||
			((reg_cfg_cmd->u.rw_info.cmd_data_offset +
			reg_cfg_cmd->u.rw_info.len) > cmd_len)) {
			pr_err("%s:%d cmd_data_offset %d len %d cmd_len %d\n",
				__func__, __LINE__,
				reg_cfg_cmd->u.rw_info.cmd_data_offset,
				reg_cfg_cmd->u.rw_info.len, cmd_len);
			return -EINVAL;
		}
		break;
	}

	case VFE_WRITE_DMI_16BIT:
	case VFE_WRITE_DMI_32BIT:
	case VFE_WRITE_DMI_64BIT:
	case VFE_READ_DMI_16BIT:
	case VFE_READ_DMI_32BIT:
	case VFE_READ_DMI_64BIT: {
		if (reg_cfg_cmd->cmd_type == VFE_WRITE_DMI_64BIT ||
			reg_cfg_cmd->cmd_type == VFE_READ_DMI_64BIT) {
			if ((reg_cfg_cmd->u.dmi_info.hi_tbl_offset <=
				reg_cfg_cmd->u.dmi_info.lo_tbl_offset) ||
				(reg_cfg_cmd->u.dmi_info.hi_tbl_offset -
				reg_cfg_cmd->u.dmi_info.lo_tbl_offset !=
				(sizeof(uint32_t)))) {
				pr_err("%s:%d hi %d lo %d\n",
					__func__, __LINE__,
					reg_cfg_cmd->u.dmi_info.hi_tbl_offset,
					reg_cfg_cmd->u.dmi_info.hi_tbl_offset);
				return -EINVAL;
			}
			if (reg_cfg_cmd->u.dmi_info.len <= sizeof(uint32_t)) {
				pr_err("%s:%d len %d\n",
					__func__, __LINE__,
					reg_cfg_cmd->u.dmi_info.len);
				return -EINVAL;
			}
			if (((UINT_MAX -
				reg_cfg_cmd->u.dmi_info.hi_tbl_offset) <
				(reg_cfg_cmd->u.dmi_info.len -
				sizeof(uint32_t))) ||
				((reg_cfg_cmd->u.dmi_info.hi_tbl_offset +
				reg_cfg_cmd->u.dmi_info.len -
				sizeof(uint32_t)) > cmd_len)) {
				pr_err("%s:%d hi_tbl_offset %d len %d cmd %d\n",
					__func__, __LINE__,
					reg_cfg_cmd->u.dmi_info.hi_tbl_offset,
					reg_cfg_cmd->u.dmi_info.len, cmd_len);
				return -EINVAL;
			}
		}
		if ((reg_cfg_cmd->u.dmi_info.lo_tbl_offset >
			(UINT_MAX - reg_cfg_cmd->u.dmi_info.len)) ||
			((reg_cfg_cmd->u.dmi_info.lo_tbl_offset +
			reg_cfg_cmd->u.dmi_info.len) > cmd_len)) {
			pr_err("%s:%d lo_tbl_offset %d len %d cmd_len %d\n",
				__func__, __LINE__,
				reg_cfg_cmd->u.dmi_info.lo_tbl_offset,
				reg_cfg_cmd->u.dmi_info.len, cmd_len);
			return -EINVAL;
		}
		break;
	}

	default:
		break;
	}

	switch (reg_cfg_cmd->cmd_type) {
	case VFE_WRITE: {
		msm_camera_io_memcpy(vfe_dev->vfe_base +
			reg_cfg_cmd->u.rw_info.reg_offset,
			cfg_data + reg_cfg_cmd->u.rw_info.cmd_data_offset/4,
			reg_cfg_cmd->u.rw_info.len);
		break;
	}
	case VFE_WRITE_MB: {
		msm_camera_io_memcpy_mb(vfe_dev->vfe_base +
			reg_cfg_cmd->u.rw_info.reg_offset,
			cfg_data + reg_cfg_cmd->u.rw_info.cmd_data_offset/4,
			reg_cfg_cmd->u.rw_info.len);
		break;
	}
	case VFE_CFG_MASK: {
		uint32_t temp;

		if ((UINT_MAX - sizeof(temp) <
			reg_cfg_cmd->u.mask_info.reg_offset) ||
			(resource_size(vfe_dev->vfe_mem) <
			reg_cfg_cmd->u.mask_info.reg_offset +
			sizeof(temp)) ||
			(reg_cfg_cmd->u.mask_info.reg_offset & 0x3)) {
			pr_err("%s: VFE_CFG_MASK: Invalid length\n", __func__);
			return -EINVAL;
		}
		temp = msm_camera_io_r(vfe_dev->vfe_base +
			reg_cfg_cmd->u.mask_info.reg_offset);

		temp &= ~reg_cfg_cmd->u.mask_info.mask;
		temp |= reg_cfg_cmd->u.mask_info.val;
		msm_camera_io_w(temp, vfe_dev->vfe_base +
			reg_cfg_cmd->u.mask_info.reg_offset);
		break;
	}
	case VFE_WRITE_DMI_16BIT:
	case VFE_WRITE_DMI_32BIT:
	case VFE_WRITE_DMI_64BIT: {
		int i;
		uint32_t *hi_tbl_ptr = NULL, *lo_tbl_ptr = NULL;
		uint32_t hi_val, lo_val, lo_val1;

		if (reg_cfg_cmd->cmd_type == VFE_WRITE_DMI_64BIT) {
			hi_tbl_ptr = cfg_data +
				reg_cfg_cmd->u.dmi_info.hi_tbl_offset/4;
		}
		lo_tbl_ptr = cfg_data +
			reg_cfg_cmd->u.dmi_info.lo_tbl_offset/4;
		if (reg_cfg_cmd->cmd_type == VFE_WRITE_DMI_64BIT)
			reg_cfg_cmd->u.dmi_info.len =
				reg_cfg_cmd->u.dmi_info.len / 2;
		for (i = 0; i < reg_cfg_cmd->u.dmi_info.len/4; i++) {
			lo_val = *lo_tbl_ptr++;
			if (reg_cfg_cmd->cmd_type == VFE_WRITE_DMI_16BIT) {
				lo_val1 = lo_val & 0x0000FFFF;
				lo_val = (lo_val & 0xFFFF0000)>>16;
				msm_camera_io_w(lo_val1, vfe_dev->vfe_base +
					vfe_dev->hw_info->dmi_reg_offset + 0x4);
			} else if (reg_cfg_cmd->cmd_type ==
					   VFE_WRITE_DMI_64BIT) {
				lo_tbl_ptr++;
				hi_val = *hi_tbl_ptr;
				hi_tbl_ptr = hi_tbl_ptr + 2;
				msm_camera_io_w(hi_val, vfe_dev->vfe_base +
					vfe_dev->hw_info->dmi_reg_offset);
			}
			msm_camera_io_w(lo_val, vfe_dev->vfe_base +
				vfe_dev->hw_info->dmi_reg_offset + 0x4);
		}
		break;
	}
	case VFE_READ_DMI_16BIT:
	case VFE_READ_DMI_32BIT:
	case VFE_READ_DMI_64BIT: {
		int i;
		uint32_t *hi_tbl_ptr = NULL, *lo_tbl_ptr = NULL;
		uint32_t hi_val, lo_val, lo_val1;

		if (reg_cfg_cmd->cmd_type == VFE_READ_DMI_64BIT) {
			hi_tbl_ptr = cfg_data +
				reg_cfg_cmd->u.dmi_info.hi_tbl_offset/4;
		}

		lo_tbl_ptr = cfg_data +
			reg_cfg_cmd->u.dmi_info.lo_tbl_offset/4;

		if (reg_cfg_cmd->cmd_type == VFE_READ_DMI_64BIT)
			reg_cfg_cmd->u.dmi_info.len =
				reg_cfg_cmd->u.dmi_info.len / 2;

		for (i = 0; i < reg_cfg_cmd->u.dmi_info.len/4; i++) {
			lo_val = msm_camera_io_r(vfe_dev->vfe_base +
					vfe_dev->hw_info->dmi_reg_offset + 0x4);

			if (reg_cfg_cmd->cmd_type == VFE_READ_DMI_16BIT) {
				lo_val1 = msm_camera_io_r(vfe_dev->vfe_base +
					vfe_dev->hw_info->dmi_reg_offset + 0x4);
				lo_val |= lo_val1 << 16;
			}
			*lo_tbl_ptr++ = lo_val;
			if (reg_cfg_cmd->cmd_type == VFE_READ_DMI_64BIT) {
				hi_val = msm_camera_io_r(vfe_dev->vfe_base +
					vfe_dev->hw_info->dmi_reg_offset);
				*hi_tbl_ptr = hi_val;
				hi_tbl_ptr += 2;
				lo_tbl_ptr++;
			}
		}
		break;
	}
	case VFE_HW_UPDATE_LOCK: {
		uint32_t update_id =
			vfe_dev->axi_data.src_info[VFE_PIX_0].last_updt_frm_id;
		if (update_id) {
			ISP_DBG("%s hw_update_lock fail cur_id %u,last_id %u\n",
				__func__,
				vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id,
				update_id);
			return -EINVAL;
		}
		break;
	}
	case VFE_HW_UPDATE_UNLOCK: {
		if (vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id
			!= *cfg_data) {
			ISP_DBG("hw_updt over frm bound,strt_id %u end_id %d\n",
				*cfg_data,
				vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id);
		}
		vfe_dev->axi_data.src_info[VFE_PIX_0].last_updt_frm_id =
			vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id;
		break;
	}
	case VFE_READ: {
		int i;
		uint32_t *data_ptr = cfg_data +
			reg_cfg_cmd->u.rw_info.cmd_data_offset/4;
		for (i = 0; i < reg_cfg_cmd->u.rw_info.len/4; i++) {
			if ((data_ptr < cfg_data) ||
				(UINT_MAX / sizeof(*data_ptr) <
				 (data_ptr - cfg_data)) ||
				(sizeof(*data_ptr) * (data_ptr - cfg_data) >=
				 cmd_len))
				return -EINVAL;
			*data_ptr++ = msm_camera_io_r(vfe_dev->vfe_base +
				reg_cfg_cmd->u.rw_info.reg_offset);
			reg_cfg_cmd->u.rw_info.reg_offset += 4;
		}
		break;
	}
	case GET_MAX_CLK_RATE: {
		int rc = 0;
		unsigned long rate;

		if (cmd_len != sizeof(__u32)) {
			pr_err("%s:%d failed: invalid cmd len %u exp %zu\n",
				__func__, __LINE__, cmd_len,
				sizeof(__u32));
			return -EINVAL;
		}
		rc = msm_isp_get_max_clk_rate(vfe_dev, &rate);
		if (rc < 0) {
			pr_err("%s:%d failed: rc %d\n", __func__, __LINE__, rc);
			return -EINVAL;
		}

		*(__u32 *)cfg_data = (__u32)rate;

		break;
	}
	case GET_ISP_ID: {
		uint32_t *isp_id = NULL;

		if (cmd_len < sizeof(uint32_t)) {
			pr_err("%s:%d failed: invalid cmd len %u exp %zu\n",
				__func__, __LINE__, cmd_len,
				sizeof(uint32_t));
			return -EINVAL;
		}

		isp_id = (uint32_t *)cfg_data;
		*isp_id = vfe_dev->pdev->id;
		break;
	}
	case SET_WM_UB_SIZE:
		break;
	case SET_UB_POLICY: {

		if (cmd_len < sizeof(vfe_dev->vfe_ub_policy)) {
			pr_err("%s:%d failed: invalid cmd len %u exp %zu\n",
				__func__, __LINE__, cmd_len,
				sizeof(vfe_dev->vfe_ub_policy));
			return -EINVAL;
		}
		vfe_dev->vfe_ub_policy = *cfg_data;
		break;
	}
	default:
		break;
	}
	return 0;
}

int msm_isp_proc_cmd(struct vfe_device *vfe_dev, void *arg)
{
	int rc = 0, i;
	struct msm_vfe_cfg_cmd2 *proc_cmd = arg;
	struct msm_vfe_reg_cfg_cmd *reg_cfg_cmd;
	uint32_t *cfg_data = NULL;

	if (!proc_cmd->num_cfg) {
		pr_err("%s: Passed num_cfg as 0\n", __func__);
		return -EINVAL;
	}
	reg_cfg_cmd = memdup_user((void __user *)(proc_cmd->cfg_cmd),
			sizeof(struct msm_vfe_reg_cfg_cmd)*proc_cmd->num_cfg);
	if (IS_ERR(reg_cfg_cmd))
		return PTR_ERR(reg_cfg_cmd);

	if (proc_cmd->cmd_len > 0 &&
		proc_cmd->cmd_len < UINT16_MAX) {
		cfg_data = memdup_user((void __user *)(proc_cmd->cfg_data),
			proc_cmd->cmd_len);
		if (IS_ERR(cfg_data))
			return PTR_ERR(cfg_data);

	}

	for (i = 0; i < proc_cmd->num_cfg; i++)
		rc = msm_isp_send_hw_cmd(vfe_dev, &reg_cfg_cmd[i],
			cfg_data, proc_cmd->cmd_len);

	if (copy_to_user(proc_cmd->cfg_data,
			cfg_data, proc_cmd->cmd_len)) {
		rc = -EFAULT;
		goto copy_cmd_failed;
	}

copy_cmd_failed:
	kfree(cfg_data);
}

int msm_isp_send_event(struct vfe_device *vfe_dev,
	uint32_t event_type,
	struct msm_isp32_event_data *event_data)
{
	struct v4l2_event isp_event;

	memset(&isp_event, 0, sizeof(struct v4l2_event));
	isp_event.id = 0;
	isp_event.type = event_type;

	memcpy(&isp_event.u.data[0], event_data,
		sizeof(struct msm_isp32_event_data));
	v4l2_event_queue(vfe_dev->subdev.sd.devnode, &isp_event);
	return 0;
}

#define CAL_WORD(width, M, N) ((width * M + N - 1) / N)

int msm_isp_cal_word_per_line(uint32_t output_format,
	uint32_t pixel_per_line)
{
	int val = -1;

	switch (output_format) {
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
	case V4L2_PIX_FMT_QBGGR8:
	case V4L2_PIX_FMT_QGBRG8:
	case V4L2_PIX_FMT_QGRBG8:
	case V4L2_PIX_FMT_QRGGB8:
	case V4L2_PIX_FMT_JPEG:
	case V4L2_PIX_FMT_META:
		val = CAL_WORD(pixel_per_line, 1, 8);
		break;
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
		val = CAL_WORD(pixel_per_line, 5, 32);
		break;
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
		val = CAL_WORD(pixel_per_line, 3, 16);
		break;
	case V4L2_PIX_FMT_SBGGR14:
	case V4L2_PIX_FMT_SGBRG14:
	case V4L2_PIX_FMT_SGRBG14:
	case V4L2_PIX_FMT_SRGGB14:
		val = CAL_WORD(pixel_per_line, 7, 32);
		break;
	case V4L2_PIX_FMT_QBGGR10:
	case V4L2_PIX_FMT_QGBRG10:
	case V4L2_PIX_FMT_QGRBG10:
	case V4L2_PIX_FMT_QRGGB10:
		val = CAL_WORD(pixel_per_line, 1, 6);
		break;
	case V4L2_PIX_FMT_QBGGR12:
	case V4L2_PIX_FMT_QGBRG12:
	case V4L2_PIX_FMT_QGRBG12:
	case V4L2_PIX_FMT_QRGGB12:
		val = CAL_WORD(pixel_per_line, 1, 5);
		break;
	case V4L2_PIX_FMT_QBGGR14:
	case V4L2_PIX_FMT_QGBRG14:
	case V4L2_PIX_FMT_QGRBG14:
	case V4L2_PIX_FMT_QRGGB14:
		val = CAL_WORD(pixel_per_line, 1, 4);
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV14:
	case V4L2_PIX_FMT_NV41:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		val = CAL_WORD(pixel_per_line, 1, 8);
		break;
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
		val = CAL_WORD(pixel_per_line, 2, 8);
	break;
	case V4L2_PIX_FMT_P16BGGR10:
	case V4L2_PIX_FMT_P16GBRG10:
	case V4L2_PIX_FMT_P16GRBG10:
	case V4L2_PIX_FMT_P16RGGB10:
		val = CAL_WORD(pixel_per_line, 1, 4);
	break;
		/*TD: Add more image format*/
	default:
		msm_isp_print_fourcc_error(__func__, output_format);
		break;
	}
	return val;
}

enum msm_isp_pack_fmt msm_isp_get_pack_format(uint32_t output_format)
{
	switch (output_format) {
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
	case V4L2_PIX_FMT_SBGGR14:
	case V4L2_PIX_FMT_SGBRG14:
	case V4L2_PIX_FMT_SGRBG14:
	case V4L2_PIX_FMT_SRGGB14:
		return MIPI;
	case V4L2_PIX_FMT_QBGGR8:
	case V4L2_PIX_FMT_QGBRG8:
	case V4L2_PIX_FMT_QGRBG8:
	case V4L2_PIX_FMT_QRGGB8:
	case V4L2_PIX_FMT_QBGGR10:
	case V4L2_PIX_FMT_QGBRG10:
	case V4L2_PIX_FMT_QGRBG10:
	case V4L2_PIX_FMT_QRGGB10:
	case V4L2_PIX_FMT_QBGGR12:
	case V4L2_PIX_FMT_QGBRG12:
	case V4L2_PIX_FMT_QGRBG12:
	case V4L2_PIX_FMT_QRGGB12:
	case V4L2_PIX_FMT_QBGGR14:
	case V4L2_PIX_FMT_QGBRG14:
	case V4L2_PIX_FMT_QGRBG14:
	case V4L2_PIX_FMT_QRGGB14:
		return QCOM;
	case V4L2_PIX_FMT_P16BGGR10:
	case V4L2_PIX_FMT_P16GBRG10:
	case V4L2_PIX_FMT_P16GRBG10:
	case V4L2_PIX_FMT_P16RGGB10:
		return PLAIN16;
	default:
		msm_isp_print_fourcc_error(__func__, output_format);
		break;
	}
	return -EINVAL;
}

int msm_isp_get_bit_per_pixel(uint32_t output_format)
{
	switch (output_format) {
	case V4L2_PIX_FMT_Y4:
		return 4;
	case V4L2_PIX_FMT_Y6:
		return 6;
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
	case V4L2_PIX_FMT_QBGGR8:
	case V4L2_PIX_FMT_QGBRG8:
	case V4L2_PIX_FMT_QGRBG8:
	case V4L2_PIX_FMT_QRGGB8:
	case V4L2_PIX_FMT_JPEG:
	case V4L2_PIX_FMT_META:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV14:
	case V4L2_PIX_FMT_NV41:
	case V4L2_PIX_FMT_YVU410:
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YYUV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_YUV411P:
	case V4L2_PIX_FMT_Y41P:
	case V4L2_PIX_FMT_YUV444:
	case V4L2_PIX_FMT_YUV555:
	case V4L2_PIX_FMT_YUV565:
	case V4L2_PIX_FMT_YUV32:
	case V4L2_PIX_FMT_YUV410:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_GREY:
	case V4L2_PIX_FMT_PAL8:
	case V4L2_PIX_FMT_UV8:
	case MSM_V4L2_PIX_FMT_META:
		return 8;
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
	case V4L2_PIX_FMT_QBGGR10:
	case V4L2_PIX_FMT_QGBRG10:
	case V4L2_PIX_FMT_QGRBG10:
	case V4L2_PIX_FMT_QRGGB10:
	case V4L2_PIX_FMT_Y10:
	case V4L2_PIX_FMT_Y10BPACK:
	case V4L2_PIX_FMT_P16BGGR10:
	case V4L2_PIX_FMT_P16GBRG10:
	case V4L2_PIX_FMT_P16GRBG10:
	case V4L2_PIX_FMT_P16RGGB10:
		return 10;
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
	case V4L2_PIX_FMT_QBGGR12:
	case V4L2_PIX_FMT_QGBRG12:
	case V4L2_PIX_FMT_QGRBG12:
	case V4L2_PIX_FMT_QRGGB12:
	case V4L2_PIX_FMT_Y12:
		return 12;
	case V4L2_PIX_FMT_SBGGR14:
	case V4L2_PIX_FMT_SGBRG14:
	case V4L2_PIX_FMT_SGRBG14:
	case V4L2_PIX_FMT_SRGGB14:
	case V4L2_PIX_FMT_QBGGR14:
	case V4L2_PIX_FMT_QGBRG14:
	case V4L2_PIX_FMT_QGRBG14:
	case V4L2_PIX_FMT_QRGGB14:
		return 14;
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_Y16:
		return 16;
		/*TD: Add more image format*/
	default:
		msm_isp_print_fourcc_error(__func__, output_format);
		pr_err("%s: Invalid output format %x\n",
			__func__, output_format);
		return -EINVAL;
	}
}

void msm_isp_update_error_frame_count(struct vfe_device *vfe_dev)
{
	struct msm_vfe_error_info *error_info = &vfe_dev->error_info;

	error_info->info_dump_frame_count++;
}

void msm_isp_process_error_info(struct vfe_device *vfe_dev)
{
	int i;
	uint8_t num_stats_type =
		vfe_dev->hw_info->stats_hw_info->num_stats_type;
	struct msm_vfe_error_info *error_info = &vfe_dev->error_info;
	static DEFINE_RATELIMIT_STATE(rs,
		DEFAULT_RATELIMIT_INTERVAL, DEFAULT_RATELIMIT_BURST);
	static DEFINE_RATELIMIT_STATE(rs_stats,
		DEFAULT_RATELIMIT_INTERVAL, DEFAULT_RATELIMIT_BURST);

	if (error_info->error_count == 1 ||
		!(error_info->info_dump_frame_count % 100)) {
		vfe_dev->hw_info->vfe_ops.core_ops.process_error_status(
				vfe_dev);
		error_info->error_mask0 = 0;
		error_info->error_mask1 = 0;
		error_info->camif_status = 0;
		error_info->violation_status = 0;
		for (i = 0; i < MAX_NUM_STREAM; i++) {
			if (error_info->stream_framedrop_count[i] != 0 &&
				__ratelimit(&rs)) {
				pr_err("%s: Stream[%d]: dropped %d frames\n",
					__func__, i,
					error_info->stream_framedrop_count[i]);
				error_info->stream_framedrop_count[i] = 0;
			}
		}
		for (i = 0; i < num_stats_type; i++) {
			if (error_info->stats_framedrop_count[i] != 0 &&
				__ratelimit(&rs_stats)) {
				pr_err("%s: Stats stream[%d]: dropped %d frames\n",
					__func__, i,
					error_info->stats_framedrop_count[i]);
				error_info->stats_framedrop_count[i] = 0;
			}
		}
	}
}

static inline void msm_isp_update_error_info(struct vfe_device *vfe_dev,
	uint32_t error_mask0, uint32_t error_mask1)
{
	vfe_dev->error_info.error_mask0 |= error_mask0;
	vfe_dev->error_info.error_mask1 |= error_mask1;
	vfe_dev->error_info.error_count++;
}

static void msm_isp_process_overflow_irq(
	struct vfe_device *vfe_dev,
	uint32_t *irq_status0, uint32_t *irq_status1)
{
	uint32_t overflow_mask;

	/* if there are no active streams - do not start recovery */
	if (!vfe_dev->axi_data.num_active_stream)
		return;

	/*Mask out all other irqs if recovery is started*/
	if (atomic_read(&vfe_dev->error_info.overflow_state) != NO_OVERFLOW) {
		uint32_t halt_restart_mask0, halt_restart_mask1;

		vfe_dev->hw_info->vfe_ops.core_ops.get_halt_restart_mask(
			&halt_restart_mask0, &halt_restart_mask1);
		*irq_status0 &= halt_restart_mask0;
		*irq_status1 &= halt_restart_mask1;

		return;
	}

	/*Check if any overflow bit is set*/
	vfe_dev->hw_info->vfe_ops.core_ops.get_overflow_mask(&overflow_mask);
	overflow_mask &= *irq_status1;

	if (overflow_mask) {
		struct msm_isp32_event_data error_event;

		if (vfe_dev->reset_pending == 1) {
			pr_err("%s:%d failed: overflow %x during reset\n",
				__func__, __LINE__, overflow_mask);
			/* Clear overflow bits since reset is pending */
			*irq_status1 &= ~overflow_mask;
			return;
		}

		ISP_DBG("%s: Bus overflow detected: 0x%x, start recovery!\n",
				__func__, overflow_mask);
		atomic_set(&vfe_dev->error_info.overflow_state,
				OVERFLOW_DETECTED);
		/*Store current IRQ mask*/
		vfe_dev->hw_info->vfe_ops.core_ops.get_irq_mask(vfe_dev,
			&vfe_dev->error_info.overflow_recover_irq_mask0,
			&vfe_dev->error_info.overflow_recover_irq_mask1);

		/*Halt the hardware & Clear all other IRQ mask*/
		vfe_dev->hw_info->vfe_ops.axi_ops.halt(vfe_dev, 0);

		/*Stop CAMIF Immediately*/
		vfe_dev->hw_info->vfe_ops.core_ops.update_camif_state(vfe_dev,
				DISABLE_CAMIF_IMMEDIATELY);

		/*Update overflow state*/
		*irq_status0 = 0;
		*irq_status1 = 0;

		memset(&error_event, 0, sizeof(error_event));
		error_event.frame_id =
			vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id;
		error_event.u.error_info.error_mask = 1 << ISP_WM_BUS_OVERFLOW;
		msm_isp_send_event(vfe_dev,
			ISP_EVENT_WM_BUS_OVERFLOW, &error_event);
	}
}

void msm_isp_reset_burst_count_and_frame_drop(
	struct vfe_device *vfe_dev, struct msm_vfe_axi_stream *stream_info)
{
	uint32_t framedrop_period = 0;

	if (stream_info->state != ACTIVE ||
		stream_info->stream_type != BURST_STREAM) {
		return;
	}
	if (stream_info->num_burst_capture != 0) {
		framedrop_period = msm_isp_get_framedrop_period(
		   stream_info->frame_skip_pattern);
		stream_info->burst_frame_count =
			stream_info->init_frame_drop +
			(stream_info->num_burst_capture - 1) *
			framedrop_period + 1;
		msm_isp_reset_framedrop(vfe_dev, stream_info);
	}
}

irqreturn_t msm_isp_process_irq(int irq_num, void *data)
{
	unsigned long flags;
	struct msm_vfe_tasklet_queue_cmd *queue_cmd;
	struct vfe_device *vfe_dev = (struct vfe_device *) data;
	uint32_t irq_status0, irq_status1;
	uint32_t error_mask0, error_mask1;

	vfe_dev->hw_info->vfe_ops.irq_ops.read_irq_status(vfe_dev,
				&irq_status0, &irq_status1);

	if ((irq_status0 == 0) && (irq_status1 == 0)) {
		pr_err_ratelimited("%s:VFE%d irq_status0 & 1 are both 0\n",
			__func__, vfe_dev->pdev->id);
		return IRQ_HANDLED;
	}

	msm_isp_process_overflow_irq(vfe_dev,
		&irq_status0, &irq_status1);

	vfe_dev->hw_info->vfe_ops.core_ops.get_error_mask(&error_mask0,
		&error_mask1);
	error_mask0 &= irq_status0;
	error_mask1 &= irq_status1;
	irq_status0 &= ~error_mask0;
	irq_status1 &= ~error_mask1;
	if (!vfe_dev->ignore_error &&
		((error_mask0 != 0) || (error_mask1 != 0)))
		msm_isp_update_error_info(vfe_dev, error_mask0, error_mask1);

	if ((irq_status0 == 0) && (irq_status1 == 0) &&
		(!(((error_mask0 != 0) || (error_mask1 != 0)) &&
		 vfe_dev->error_info.error_count == 1))) {
		ISP_DBG("%s: error_mask0/1 & error_count are set!\n", __func__);
		return IRQ_HANDLED;
	}

	spin_lock_irqsave(&vfe_dev->tasklet_lock, flags);
	queue_cmd = &vfe_dev->tasklet_queue_cmd[vfe_dev->taskletq_idx];
	if (queue_cmd->cmd_used) {
		pr_err_ratelimited("%s: Tasklet queue overflow: %d\n",
			__func__, vfe_dev->pdev->id);
		list_del(&queue_cmd->list);
	} else {
		atomic_add(1, &vfe_dev->irq_cnt);
	}
	queue_cmd->vfeInterruptStatus0 = irq_status0;
	queue_cmd->vfeInterruptStatus1 = irq_status1;
	msm_isp_get_timestamp(&queue_cmd->ts, vfe_dev);
	queue_cmd->cmd_used = 1;
	vfe_dev->taskletq_idx =
		(vfe_dev->taskletq_idx + 1) % MSM_VFE_TASKLETQ_SIZE;
	list_add_tail(&queue_cmd->list, &vfe_dev->tasklet_q);
	spin_unlock_irqrestore(&vfe_dev->tasklet_lock, flags);
	tasklet_schedule(&vfe_dev->vfe_tasklet);
	return IRQ_HANDLED;
}

void msm_isp_do_tasklet(unsigned long data)
{
	unsigned long flags;
	struct vfe_device *vfe_dev = (struct vfe_device *) data;
	struct msm_vfe_irq_ops *irq_ops = &vfe_dev->hw_info->vfe_ops.irq_ops;
	struct msm_vfe_tasklet_queue_cmd *queue_cmd;
	struct msm_isp_timestamp ts;
	uint32_t irq_status0, irq_status1;

	while (atomic_read(&vfe_dev->irq_cnt)) {
		spin_lock_irqsave(&vfe_dev->tasklet_lock, flags);
		queue_cmd = list_first_entry(&vfe_dev->tasklet_q,
		struct msm_vfe_tasklet_queue_cmd, list);

		if (!queue_cmd) {
			atomic_set(&vfe_dev->irq_cnt, 0);
			spin_unlock_irqrestore(&vfe_dev->tasklet_lock, flags);
			return;
		}
		atomic_sub(1, &vfe_dev->irq_cnt);
		list_del(&queue_cmd->list);
		queue_cmd->cmd_used = 0;
		irq_status0 = queue_cmd->vfeInterruptStatus0;
		irq_status1 = queue_cmd->vfeInterruptStatus1;
		ts = queue_cmd->ts;
		spin_unlock_irqrestore(&vfe_dev->tasklet_lock, flags);
		ISP_DBG("%s: status0: 0x%x status1: 0x%x\n",
			__func__, irq_status0, irq_status1);
		irq_ops->process_reset_irq(vfe_dev,
			irq_status0, irq_status1);
		irq_ops->process_halt_irq(vfe_dev,
			irq_status0, irq_status1);
		if (atomic_read(&vfe_dev->error_info.overflow_state)
			!= NO_OVERFLOW) {
			pr_err("%s: Recovery in processing, Ignore IRQs!!!\n",
				__func__);
			continue;
		}
		msm_isp_process_error_info(vfe_dev);
		irq_ops->process_camif_irq(vfe_dev,
			irq_status0, irq_status1, &ts);
		irq_ops->process_axi_irq(vfe_dev,
			irq_status0, irq_status1, &ts);
		irq_ops->process_stats_irq(vfe_dev,
			irq_status0, irq_status1, &ts);
		irq_ops->process_reg_update(vfe_dev,
			irq_status0, irq_status1, &ts);
		irq_ops->process_epoch_irq(vfe_dev,
			irq_status0, irq_status1, &ts);
	}
}

int msm_isp_set_src_state(struct vfe_device *vfe_dev, void *arg)
{
	struct msm_vfe_axi_src_state *src_state = arg;

	if (src_state->input_src >= VFE_SRC_MAX)
		return -EINVAL;
	vfe_dev->axi_data.src_info[src_state->input_src].active =
	src_state->src_active;
	vfe_dev->axi_data.src_info[src_state->input_src].frame_id =
	src_state->src_frame_id;
	return 0;
}

static void msm_vfe_iommu_fault_handler(struct iommu_domain *domain,
	struct device *dev, unsigned long iova, int flags, void *token)
{
	struct vfe_device *vfe_dev = NULL;

	if (token) {
		vfe_dev = (struct vfe_device *)token;
		if (!vfe_dev->buf_mgr || !vfe_dev->buf_mgr->ops) {
			pr_err("%s:%d] buf_mgr %pK\n", __func__,
				__LINE__, vfe_dev->buf_mgr);
			goto end;
		}
		if (!vfe_dev->buf_mgr->pagefault_debug_disable) {
			pr_err("%s:%d] vfe_dev %pK id %d\n", __func__,
				__LINE__, vfe_dev, vfe_dev->pdev->id);
			vfe_dev->buf_mgr->ops->buf_mgr_debug(vfe_dev->buf_mgr,
								iova);
		}
	} else {
		ISP_DBG("%s:%d] no token received: %pK\n",
			__func__, __LINE__, token);
		goto end;
	}
end:
	return;
}

int msm_isp_open_node(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct vfe_device *vfe_dev = v4l2_get_subdevdata(sd);
	long rc = 0;

	ISP_DBG("%s\n", __func__);

	mutex_lock(&vfe_dev->realtime_mutex);
	mutex_lock(&vfe_dev->core_mutex);

	if (vfe_dev->vfe_open_cnt++ && vfe_dev->vfe_base) {
		mutex_unlock(&vfe_dev->core_mutex);
		mutex_unlock(&vfe_dev->realtime_mutex);
		return 0;
	}

	if (vfe_dev->vfe_base) {
		pr_err("%s:%d invalid params cnt %d base %pK\n", __func__,
			__LINE__, vfe_dev->vfe_open_cnt, vfe_dev->vfe_base);
		vfe_dev->vfe_base = NULL;
	}

	vfe_dev->reset_pending = 0;
	vfe_dev->isp_sof_debug = 0;

	if (vfe_dev->hw_info->vfe_ops.core_ops.init_hw(vfe_dev) < 0) {
		pr_err("%s: init hardware failed\n", __func__);
		vfe_dev->vfe_open_cnt--;
		mutex_unlock(&vfe_dev->core_mutex);
		mutex_unlock(&vfe_dev->realtime_mutex);
		return -EBUSY;
	}

	memset(&vfe_dev->error_info, 0, sizeof(vfe_dev->error_info));
	atomic_set(&vfe_dev->error_info.overflow_state, NO_OVERFLOW);

	vfe_dev->hw_info->vfe_ops.core_ops.clear_status_reg(vfe_dev);

	rc = vfe_dev->hw_info->vfe_ops.core_ops.reset_hw(vfe_dev, 1, 1);
	if (rc <= 0) {
		pr_err("%s: reset timeout\n", __func__);
		vfe_dev->hw_info->vfe_ops.core_ops.release_hw(vfe_dev);
		vfe_dev->vfe_open_cnt--;
		mutex_unlock(&vfe_dev->core_mutex);
		mutex_unlock(&vfe_dev->realtime_mutex);
		return -EINVAL;
	}
	vfe_dev->vfe_hw_version = msm_camera_io_r(vfe_dev->vfe_base);
	ISP_DBG("%s: HW Version: 0x%x\n", __func__, vfe_dev->vfe_hw_version);

	vfe_dev->hw_info->vfe_ops.core_ops.init_hw_reg(vfe_dev);

	vfe_dev->buf_mgr->ops->buf_mgr_init(vfe_dev->buf_mgr, "msm_isp");

	memset(&vfe_dev->axi_data, 0, sizeof(struct msm_vfe_axi_shared_data));
	memset(&vfe_dev->stats_data, 0,
		sizeof(struct msm_vfe_stats_shared_data));
	memset(&vfe_dev->error_info, 0, sizeof(vfe_dev->error_info));
	memset(&vfe_dev->fetch_engine_info, 0,
		sizeof(vfe_dev->fetch_engine_info));
	vfe_dev->axi_data.hw_info = vfe_dev->hw_info->axi_hw_info;
	vfe_dev->taskletq_idx = 0;
	vfe_dev->vt_enable = 0;
	vfe_dev->bus_util_factor = 0;
	rc = of_property_read_u32(vfe_dev->pdev->dev.of_node,
			"bus-util-factor", &vfe_dev->bus_util_factor);
	if (rc < 0)
		ISP_DBG("%s: Use default bus utilization factor\n", __func__);

	cam_smmu_reg_client_page_fault_handler(
			vfe_dev->buf_mgr->iommu_hdl,
			msm_vfe_iommu_fault_handler,
			NULL,
			vfe_dev);

	mutex_unlock(&vfe_dev->core_mutex);
	mutex_unlock(&vfe_dev->realtime_mutex);
	return 0;
}

#ifdef CONFIG_MSM_AVTIMER
static void msm_isp_end_avtimer(void)
{
	avcs_core_disable_power_collapse(0);
}
#else
static void msm_isp_end_avtimer(void)
{
	pr_err("AV Timer is not supported\n");
}
#endif

int msm_isp_close_node(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	long rc = 0;
	struct vfe_device *vfe_dev = v4l2_get_subdevdata(sd);

	ISP_DBG("%s E\n", __func__);
	mutex_lock(&vfe_dev->realtime_mutex);
	mutex_lock(&vfe_dev->core_mutex);

	if (!vfe_dev->vfe_open_cnt) {
		pr_err("%s invalid state open cnt %d\n", __func__,
			vfe_dev->vfe_open_cnt);
		mutex_unlock(&vfe_dev->core_mutex);
		mutex_unlock(&vfe_dev->realtime_mutex);
		return -EINVAL;
	}

	if (--vfe_dev->vfe_open_cnt) {
		mutex_unlock(&vfe_dev->core_mutex);
		mutex_unlock(&vfe_dev->realtime_mutex);
		return 0;
	}

	/* Unregister page fault handler */
	cam_smmu_reg_client_page_fault_handler(
		vfe_dev->buf_mgr->iommu_hdl,
		NULL, NULL, vfe_dev);

	rc = vfe_dev->hw_info->vfe_ops.axi_ops.halt(vfe_dev, 1);
	if (rc < 0)
		pr_err("%s: halt timeout rc=%ld\n", __func__, rc);

	vfe_dev->buf_mgr->ops->buf_mgr_deinit(vfe_dev->buf_mgr);
	vfe_dev->hw_info->vfe_ops.core_ops.release_hw(vfe_dev);
	if (vfe_dev->vt_enable) {
		msm_isp_end_avtimer();
		vfe_dev->vt_enable = 0;
	}
	mutex_unlock(&vfe_dev->core_mutex);
	mutex_unlock(&vfe_dev->realtime_mutex);
	return 0;
}
