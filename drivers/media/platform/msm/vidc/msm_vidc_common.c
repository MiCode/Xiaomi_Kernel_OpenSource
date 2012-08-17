/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/iommu.h>
#include <mach/iommu.h>
#include <mach/iommu_domains.h>
#include <mach/peripheral-loader.h>

#include "msm_vidc_common.h"
#include "vidc_hal_api.h"
#include "msm_smem.h"

#define HW_RESPONSE_TIMEOUT (5 * 60 * 1000)

#define IS_ALREADY_IN_STATE(__p, __d) ({\
	int __rc = (__p >= __d);\
	__rc; \
})

#define V4L2_EVENT_SEQ_CHANGED_SUFFICIENT \
		V4L2_EVENT_MSM_VIDC_PORT_SETTINGS_CHANGED_SUFFICIENT
#define V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT \
		V4L2_EVENT_MSM_VIDC_PORT_SETTINGS_CHANGED_INSUFFICIENT

#define NUM_MBS_PER_SEC(__height, __width, __fps) ({\
	(__height >> 4) * (__width >> 4) * __fps; \
})

#define VIDC_BUS_LOAD(__height, __width, __fps, __br) ({\
	__height * __width * __fps; \
})

#define GET_NUM_MBS(__h, __w) ({\
	u32 __mbs = (__h >> 4) * (__w >> 4);\
	__mbs;\
})

/*While adding entries to this array make sure
 * they are in descending order.
 * Look @ msm_comm_get_load function*/
static const u32 clocks_table[][2] = {
	{979200, 410000000},
	{560145, 266670000},
	{421161, 200000000},
	{243000, 133330000},
	{108000, 100000000},
	{36000, 50000000},
};

static const u32 bus_table[] = {
	0,
	9216000,
	27648000,
	62208000,
};

static int msm_comm_get_bus_load(struct msm_vidc_core *core)
{
	struct msm_vidc_inst *inst = NULL;
	int load = 0;
	if (!core) {
		pr_err("Invalid args: %p\n", core);
		return -EINVAL;
	}
	list_for_each_entry(inst, &core->instances, list) {
		load += VIDC_BUS_LOAD(inst->prop.height,
				inst->prop.width, inst->prop.fps,
				2000000);
	}
	return load;
}

static int get_bus_vector(int load)
{
	int num_rows = sizeof(bus_table)/(sizeof(u32));
	int i;
	for (i = num_rows - 1; i > 0; i--) {
		if ((load >= bus_table[i]) || (i == 1))
			break;
	}
	pr_err("Required bus = %d\n", i);
	return i;
}

int msm_comm_scale_bus(struct msm_vidc_core *core)
{
	int load;
	int rc = 0;
	if (!core) {
		pr_err("Invalid args: %p\n", core);
		return -EINVAL;
	}
	load = msm_comm_get_bus_load(core);
	if (load <= 0) {
		pr_err("Failed to scale bus for %d load\n",
			load);
		goto fail_scale_bus;
	}
	rc = msm_bus_scale_client_update_request(
			core->resources.bus_info.vcodec_handle,
			get_bus_vector(load));
	if (rc) {
		pr_err("Failed to scale bus: %d\n", rc);
		goto fail_scale_bus;
	}
	rc = msm_bus_scale_client_update_request(
			core->resources.bus_info.ocmem_handle,
			get_bus_vector(load));
	if (rc) {
		pr_err("Failed to scale bus: %d\n", rc);
		goto fail_scale_bus;
	}
fail_scale_bus:
	return rc;
}

static int msm_comm_get_load(struct msm_vidc_core *core)
{
	struct msm_vidc_inst *inst = NULL;
	int num_mbs_per_sec = 0;
	if (!core) {
		pr_err("Invalid args: %p\n", core);
		return -EINVAL;
	}
	list_for_each_entry(inst, &core->instances, list)
		num_mbs_per_sec += NUM_MBS_PER_SEC(inst->prop.height,
				inst->prop.width, inst->prop.fps);
	return num_mbs_per_sec;
}

static unsigned long get_clock_rate(struct core_clock *clock,
	int num_mbs_per_sec)
{
	int num_rows = clock->count;
	struct load_freq_table *table = clock->load_freq_tbl;
	unsigned long ret = table[num_rows-1].freq;
	int i;
	for (i = 0; i < num_rows; i++) {
		if (num_mbs_per_sec > table[i].load)
			break;
		ret = table[i].freq;
	}
	pr_err("Required clock rate = %lu\n", ret);
	return ret;
}

struct msm_vidc_core *get_vidc_core(int core_id)
{
	struct msm_vidc_core *core;
	int found = 0;
	unsigned long flags;
	if (core_id > MSM_VIDC_CORES_MAX) {
		pr_err("Core id = %d is greater than max = %d\n",
			core_id, MSM_VIDC_CORES_MAX);
		return NULL;
	}
	spin_lock_irqsave(&vidc_driver->lock, flags);
	list_for_each_entry(core, &vidc_driver->cores, list) {
		if (core && core->id == core_id)
			found = 1;
			break;
	}
	spin_unlock_irqrestore(&vidc_driver->lock, flags);
	if (found)
		return core;
	return NULL;
}

static int msm_comm_iommu_attach(struct msm_vidc_core *core)
{
	int rc;
	struct iommu_domain *domain;
	int i;
	struct iommu_info *io_map;
	struct device *dev;
	for (i = 0; i < MAX_MAP; i++) {
		io_map = &core->resources.io_map[i];
		dev = msm_iommu_get_ctx(io_map->ctx);
		domain = msm_get_iommu_domain(io_map->domain);
		if (IS_ERR_OR_NULL(domain)) {
			pr_err("Failed to get domain: %s\n", io_map->name);
			rc = PTR_ERR(domain);
			break;
		}
		rc = iommu_attach_device(domain, dev);
		if (rc) {
			pr_err("IOMMU attach failed: %s\n", io_map->name);
			break;
		}
	}
	if (i < MAX_MAP) {
		i--;
		for (; i >= 0; i--) {
			io_map = &core->resources.io_map[i];
			dev = msm_iommu_get_ctx(io_map->ctx);
			domain = msm_get_iommu_domain(io_map->domain);
			if (dev && domain)
				iommu_detach_device(domain, dev);
		}
	}
	return rc;
}

static void msm_comm_iommu_detach(struct msm_vidc_core *core)
{
	struct device *dev;
	struct iommu_domain *domain;
	struct iommu_info *io_map;
	int i;
	if (!core) {
		pr_err("Invalid paramter: %p\n", core);
		return;
	}
	for (i = 0; i < MAX_MAP; i++) {
		io_map = &core->resources.io_map[i];
		dev = msm_iommu_get_ctx(io_map->ctx);
		domain = msm_get_iommu_domain(io_map->domain);
		if (dev && domain)
			iommu_detach_device(domain, dev);
	}
}

const struct msm_vidc_format *msm_comm_get_pixel_fmt_index(
	const struct msm_vidc_format fmt[], int size, int index, int fmt_type)
{
	int i, k = 0;
	if (!fmt || index < 0) {
		pr_err("Invalid inputs, fmt = %p, index = %d\n",
						fmt, index);
		return NULL;
	}
	for (i = 0; i < size; i++) {
		if (fmt[i].type != fmt_type)
			continue;
		if (k == index)
			break;
		k++;
	}
	if (i == size) {
		pr_err("Format not found\n");
		return NULL;
	}
	return &fmt[i];
}
const struct msm_vidc_format *msm_comm_get_pixel_fmt_fourcc(
	const struct msm_vidc_format fmt[], int size, int fourcc, int fmt_type)
{
	int i;
	if (!fmt) {
		pr_err("Invalid inputs, fmt = %p\n", fmt);
		return NULL;
	}
	for (i = 0; i < size; i++) {
		if (fmt[i].fourcc == fourcc)
				break;
	}
	if (i == size) {
		pr_err("Format not found\n");
		return NULL;
	}
	return &fmt[i];
}

struct vb2_queue *msm_comm_get_vb2q(
		struct msm_vidc_inst *inst,	enum v4l2_buf_type type)
{
	if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return &inst->vb2_bufq[CAPTURE_PORT];
	if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return &inst->vb2_bufq[OUTPUT_PORT];
	return NULL;
}

static void handle_sys_init_done(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_core *core;
	struct vidc_hal_sys_init_done *sys_init_msg;
	int index = SYS_MSG_INDEX(cmd);
	if (!response) {
		pr_err("Failed to get valid response for sys init\n");
		return;
	}
	core = get_vidc_core(response->device_id);
	if (!core) {
		pr_err("Wrong device_id received\n");
		return;
	}
	pr_debug("index = %d\n", index);
	pr_debug("ptr = %p\n", &(core->completions[index]));
	complete(&(core->completions[index]));
	sys_init_msg = response->data;
	if (!sys_init_msg) {
		pr_err("sys_init_done message not proper\n");
		return;
	}
}

static void handle_sys_release_res_done(
	enum command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_core *core;
	if (!response) {
		pr_err("Failed to get valid response for sys init\n");
		return;
	}
	core = get_vidc_core(response->device_id);
	if (!core) {
		pr_err("Wrong device_id received\n");
		return;
	}
	complete(&core->completions[SYS_MSG_INDEX(cmd)]);
}

static inline void change_inst_state(struct msm_vidc_inst *inst,
	enum instance_state state)
{
	unsigned long flags;
	spin_lock_irqsave(&inst->lock, flags);
	pr_debug("Moved inst: %p from state: %d to state: %d\n",
		   inst, inst->state, state);
	inst->state = state;
	spin_unlock_irqrestore(&inst->lock, flags);
}

static int signal_session_msg_receipt(enum command_response cmd,
		struct msm_vidc_inst *inst)
{
	if (!inst) {
		pr_err("Invalid(%p) instance id\n", inst);
		return -EINVAL;
	}
	complete(&inst->completions[SESSION_MSG_INDEX(cmd)]);
	return 0;
}

static int wait_for_sess_signal_receipt(struct msm_vidc_inst *inst,
	enum command_response cmd)
{
	int rc = 0;
	rc = wait_for_completion_interruptible_timeout(
		&inst->completions[SESSION_MSG_INDEX(cmd)],
		msecs_to_jiffies(HW_RESPONSE_TIMEOUT));
	if (!rc) {
		pr_err("Wait interrupted or timeout: %d\n", rc);
		rc = -EIO;
	} else {
		rc = 0;
	}
	return rc;
}

static int wait_for_state(struct msm_vidc_inst *inst,
	enum instance_state flipped_state,
	enum instance_state desired_state,
	enum command_response hal_cmd)
{
	int rc = 0;
	if (IS_ALREADY_IN_STATE(flipped_state, desired_state)) {
		pr_err("inst: %p is already in state: %d\n", inst, inst->state);
		goto err_same_state;
	}
	pr_debug("Waiting for hal_cmd: %d\n", hal_cmd);
	rc = wait_for_sess_signal_receipt(inst, hal_cmd);
	if (!rc)
		change_inst_state(inst, desired_state);
err_same_state:
	return rc;
}

static void handle_session_init_done(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;
	if (response) {
		inst = (struct msm_vidc_inst *)response->session_id;
		signal_session_msg_receipt(cmd, inst);
	} else {
		pr_err("Failed to get valid response for session init\n");
	}
}

static void handle_event_change(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;
	struct v4l2_event dqevent;
	struct msm_vidc_cb_event *event_notify;
	if (response) {
		inst = (struct msm_vidc_inst *)response->session_id;
		dqevent.id = 0;
		event_notify = (struct msm_vidc_cb_event *) response->data;
		switch (event_notify->hal_event_type) {
		case HAL_EVENT_SEQ_CHANGED_SUFFICIENT_RESOURCES:
			dqevent.type =
				V4L2_EVENT_SEQ_CHANGED_SUFFICIENT;
			break;
		case HAL_EVENT_SEQ_CHANGED_INSUFFICIENT_RESOURCES:
			dqevent.type =
				V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT;
			break;
		default:
			break;
		}
		inst->reconfig_height = event_notify->height;
		inst->reconfig_width = event_notify->width;
		inst->in_reconfig = true;
		v4l2_event_queue_fh(&inst->event_handler, &dqevent);
		return;
	} else {
		pr_err("Failed to get valid response for event_change\n");
	}
}

static void handle_session_prop_info(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;
	unsigned long flags;
	if (!response || !response->data) {
		pr_err("Failed to get valid response for prop info\n");
		return;
	}
	inst = (struct msm_vidc_inst *)response->session_id;
	spin_lock_irqsave(&inst->lock, flags);
	memcpy(&inst->buff_req, response->data,
			sizeof(struct buffer_requirements));
	spin_unlock_irqrestore(&inst->lock, flags);
	signal_session_msg_receipt(cmd, inst);
}

static void handle_load_resource_done(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;
	if (response)
		inst = (struct msm_vidc_inst *)response->session_id;
	else
		pr_err("Failed to get valid response for load resource\n");
}

static void handle_start_done(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;
	if (response) {
		inst = (struct msm_vidc_inst *)response->session_id;
		signal_session_msg_receipt(cmd, inst);
	} else {
		pr_err("Failed to get valid response for start\n");
	}
}

static void handle_stop_done(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;
	if (response) {
		inst = (struct msm_vidc_inst *)response->session_id;
		signal_session_msg_receipt(cmd, inst);
	} else {
		pr_err("Failed to get valid response for stop\n");
	}
}

static void handle_release_res_done(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;
	if (response) {
		inst = (struct msm_vidc_inst *)response->session_id;
		signal_session_msg_receipt(cmd, inst);
	} else {
		pr_err("Failed to get valid response for release resource\n");
	}
}

static void handle_session_flush(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;
	struct v4l2_event dqevent;
	if (response) {
		inst = (struct msm_vidc_inst *)response->session_id;
		dqevent.type = V4L2_EVENT_MSM_VIDC_FLUSH_DONE;
		dqevent.id = 0;
		v4l2_event_queue_fh(&inst->event_handler, &dqevent);
	} else {
		pr_err("Failed to get valid response for flush\n");
	}
}


static void handle_session_close(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;
	struct v4l2_event dqevent;
	if (response) {
		inst = (struct msm_vidc_inst *)response->session_id;
		signal_session_msg_receipt(cmd, inst);
		dqevent.type = V4L2_EVENT_MSM_VIDC_CLOSE_DONE;
		dqevent.id = 0;
		v4l2_event_queue_fh(&inst->event_handler, &dqevent);
	} else {
		pr_err("Failed to get valid response for session close\n");
	}
}

static struct vb2_buffer *get_vb_from_device_addr(struct vb2_queue *q,
		u32 dev_addr)
{
	struct vb2_buffer *vb = NULL;
	int found = 0;
	if (!q) {
		pr_err("Invalid parameter\n");
		return NULL;
	}
	list_for_each_entry(vb, &q->queued_list, queued_entry) {
		if (vb->v4l2_planes[0].m.userptr == dev_addr) {
			found = 1;
			break;
		}
	}
	if (!found) {
		pr_err("Failed to find the buffer in queued list: %d, %d\n",
			dev_addr, q->type);
		vb = NULL;
	}
	return vb;
}

static void handle_ebd(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_data_done *response = data;
	struct vb2_buffer *vb;
	if (!response) {
		pr_err("Invalid response from vidc_hal\n");
		return;
	}
	vb = response->clnt_data;
	if (vb)
		vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
}

static void handle_fbd(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_data_done *response = data;
	struct msm_vidc_inst *inst;
	struct vb2_buffer *vb;
	struct vidc_hal_fbd *fill_buf_done;
	if (!response) {
		pr_err("Invalid response from vidc_hal\n");
		return;
	}
	inst = (struct msm_vidc_inst *)response->session_id;
	fill_buf_done = (struct vidc_hal_fbd *)&response->output_done;
	vb = get_vb_from_device_addr(&inst->vb2_bufq[CAPTURE_PORT],
		(u32)fill_buf_done->packet_buffer1);
	if (vb) {
		vb->v4l2_planes[0].bytesused = fill_buf_done->filled_len1;
		pr_debug("Filled length = %d\n", vb->v4l2_planes[0].bytesused);
		if (fill_buf_done->flags1 & HAL_BUFFERFLAG_EOS)
			vb->v4l2_buf.flags |= V4L2_BUF_FLAG_EOS;
		vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
	} else {
		/*
		 * FIXME:
		 * Special handling for EOS case: if we sent a 0 length input
		 * buf with EOS set, Venus doesn't return a valid output buffer.
		 * So pick up a random buffer that's with us, and send it to
		 * v4l2 client with EOS flag set.
		 *
		 * This would normally be OK unless client decides to send
		 * frames even after EOS.
		 *
		 * This should be fixed in upcoming versions of firmware
		 */
		if (fill_buf_done->flags1 & HAL_BUFFERFLAG_EOS
			&& fill_buf_done->filled_len1 == 0) {
			struct vb2_queue *q = &inst->vb2_bufq[CAPTURE_PORT];

			if (!list_empty(&q->queued_list)) {
				vb = list_first_entry(&q->queued_list,
					struct vb2_buffer, queued_entry);
				vb->v4l2_planes[0].bytesused = 0;
				vb->v4l2_buf.flags |= V4L2_BUF_FLAG_EOS;
				vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
			}

		}

	}
}

void handle_cmd_response(enum command_response cmd, void *data)
{
	pr_debug("Command response = %d\n", cmd);
	switch (cmd) {
	case SYS_INIT_DONE:
		handle_sys_init_done(cmd, data);
		break;
	case RELEASE_RESOURCE_DONE:
		handle_sys_release_res_done(cmd, data);
		break;
	case SESSION_INIT_DONE:
		handle_session_init_done(cmd, data);
		break;
	case SESSION_PROPERTY_INFO:
		handle_session_prop_info(cmd, data);
		break;
	case SESSION_LOAD_RESOURCE_DONE:
		handle_load_resource_done(cmd, data);
		break;
	case SESSION_START_DONE:
		handle_start_done(cmd, data);
		break;
	case SESSION_ETB_DONE:
		handle_ebd(cmd, data);
		break;
	case SESSION_FTB_DONE:
		handle_fbd(cmd, data);
		break;
	case SESSION_STOP_DONE:
		handle_stop_done(cmd, data);
		break;
	case SESSION_RELEASE_RESOURCE_DONE:
		handle_release_res_done(cmd, data);
		break;
	case SESSION_END_DONE:
		handle_session_close(cmd, data);
		break;
	case VIDC_EVENT_CHANGE:
		handle_event_change(cmd, data);
		break;
	case SESSION_FLUSH_DONE:
		handle_session_flush(cmd, data);
		break;
	default:
		pr_err("response unhandled\n");
		break;
	}
}

int msm_comm_scale_clocks(struct msm_vidc_core *core)
{
	int num_mbs_per_sec;
	int rc = 0;
	if (!core) {
		pr_err("Invalid args: %p\n", core);
		return -EINVAL;
	}
	num_mbs_per_sec = msm_comm_get_load(core);
	pr_err("num_mbs_per_sec = %d\n", num_mbs_per_sec);
	rc = clk_set_rate(core->resources.clock[VCODEC_CLK].clk,
			get_clock_rate(&core->resources.clock[VCODEC_CLK],
				num_mbs_per_sec));
	if (rc) {
		pr_err("Failed to set clock rate: %d\n", rc);
		goto fail_clk_set_rate;
	}
	rc = msm_comm_scale_bus(core);
	if (rc)
		pr_err("Failed to scale bus bandwidth\n");
fail_clk_set_rate:
	return rc;
}

static inline int msm_comm_enable_clks(struct msm_vidc_core *core)
{
	int i;
	struct core_clock *cl;
	int rc = 0;
	if (!core) {
		pr_err("Invalid params: %p\n", core);
		return -EINVAL;
	}
	for (i = 0; i < VCODEC_MAX_CLKS; i++) {
		cl = &core->resources.clock[i];
		rc = clk_prepare_enable(cl->clk);
		if (rc) {
			pr_err("Failed to enable clocks\n");
			goto fail_clk_enable;
		} else {
			pr_err("Clock: %s enabled\n", cl->name);
		}
	}
	return rc;
fail_clk_enable:
	for (; i >= 0; i--) {
		cl = &core->resources.clock[i];
		clk_disable_unprepare(cl->clk);
	}
	return rc;
}

static inline void msm_comm_disable_clks(struct msm_vidc_core *core)
{
	int i;
	struct core_clock *cl;
	if (!core) {
		pr_err("Invalid params: %p\n", core);
		return;
	}
	for (i = 0; i < VCODEC_MAX_CLKS; i++) {
		cl = &core->resources.clock[i];
		clk_disable_unprepare(cl->clk);
	}
}

static int msm_comm_load_fw(struct msm_vidc_core *core)
{
	int rc = 0;
	if (!core) {
		pr_err("Invalid paramter: %p\n", core);
		return -EINVAL;
	}
	rc = msm_comm_scale_clocks(core);
	if (rc) {
		pr_err("Failed to set clock rate: %d\n", rc);
		goto fail_pil_get;
	}

	if (!core->resources.fw.cookie)
		core->resources.fw.cookie = pil_get("venus");

	if (IS_ERR_OR_NULL(core->resources.fw.cookie)) {
		pr_err("Failed to download firmware\n");
		rc = -ENOMEM;
		goto fail_pil_get;
	}

	rc = msm_comm_enable_clks(core);
	if (rc) {
		pr_err("Failed to enable clocks: %d\n", rc);
		goto fail_enable_clks;
	}

	rc = msm_comm_iommu_attach(core);
	if (rc) {
		pr_err("Failed to attach iommu");
		goto fail_iommu_attach;
	}
	return rc;
fail_iommu_attach:
	msm_comm_disable_clks(core);
fail_enable_clks:
	pil_put(core->resources.fw.cookie);
	core->resources.fw.cookie = NULL;
fail_pil_get:
	return rc;
}

static void msm_comm_unload_fw(struct msm_vidc_core *core)
{
	if (!core) {
		pr_err("Invalid paramter: %p\n", core);
		return;
	}
	if (core->resources.fw.cookie) {
		pil_put(core->resources.fw.cookie);
		core->resources.fw.cookie = NULL;
		msm_comm_iommu_detach(core);
		msm_comm_disable_clks(core);
	}
}

static inline unsigned long get_ocmem_requirement(u32 height, u32 width)
{
	int num_mbs = 0;
	num_mbs = GET_NUM_MBS(height, width);
	/*TODO: This should be changes once the numbers are
	 * available from firmware*/
	return 512 * 1024;
}

static int msm_comm_set_ocmem(struct msm_vidc_core *core,
	struct ocmem_buf *ocmem)
{
	struct vidc_resource_hdr rhdr;
	int rc = 0;
	if (!core || !ocmem) {
		pr_err("Invalid params, core:%p, ocmem: %p\n",
			core, ocmem);
		return -EINVAL;
	}
	rhdr.resource_id = VIDC_RESOURCE_OCMEM;
	rhdr.resource_handle = (u32) &core->resources.ocmem;
	rhdr.size =	ocmem->len;
	rc = vidc_hal_core_set_resource(core->device, &rhdr, ocmem);
	if (rc) {
		pr_err("Failed to set OCMEM on driver\n");
		goto ocmem_set_failed;
	}
	pr_debug("OCMEM set, addr = %lx, size: %ld\n",
		ocmem->addr, ocmem->len);
ocmem_set_failed:
	return rc;
}

static int msm_comm_unset_ocmem(struct msm_vidc_core *core)
{
	struct vidc_resource_hdr rhdr;
	int rc = 0;
	if (!core || !core->resources.ocmem.buf) {
		pr_err("Invalid params, core:%p\n",	core);
		return -EINVAL;
	}
	rhdr.resource_id = VIDC_RESOURCE_OCMEM;
	rhdr.resource_handle = (u32) &core->resources.ocmem;
	init_completion(
		&core->completions[SYS_MSG_INDEX(RELEASE_RESOURCE_DONE)]);
	rc = vidc_hal_core_release_resource(core->device, &rhdr);
	if (rc) {
		pr_err("Failed to set OCMEM on driver\n");
		goto release_ocmem_failed;
	}
	rc = wait_for_completion_timeout(
		&core->completions[SYS_MSG_INDEX(RELEASE_RESOURCE_DONE)],
		msecs_to_jiffies(HW_RESPONSE_TIMEOUT));
	if (!rc) {
		pr_err("Wait interrupted or timeout: %d\n", rc);
		rc = -EIO;
		goto release_ocmem_failed;
	}
release_ocmem_failed:
	return rc;
}

static int msm_comm_alloc_ocmem(struct msm_vidc_core *core,
		unsigned long size)
{
	int rc = 0;
	unsigned long flags;
	struct ocmem_buf *ocmem_buffer;
	if (!core || !size) {
		pr_err("Invalid param, core: %p, size: %lu\n", core, size);
		return -EINVAL;
	}
	spin_lock_irqsave(&core->lock, flags);
	ocmem_buffer = core->resources.ocmem.buf;
	if (!ocmem_buffer ||
		ocmem_buffer->len < size) {
		ocmem_buffer = ocmem_allocate_nb(OCMEM_VIDEO, size);
		if (IS_ERR_OR_NULL(ocmem_buffer)) {
			pr_err("ocmem_allocate_nb failed: %d\n",
				(u32) ocmem_buffer);
			rc = -ENOMEM;
		}
		core->resources.ocmem.buf = ocmem_buffer;
		rc = msm_comm_set_ocmem(core, ocmem_buffer);
		if (rc) {
			pr_err("Failed to set ocmem: %d\n", rc);
			goto ocmem_set_failed;
		}
	} else
		pr_debug("OCMEM is enough. reqd: %lu, available: %lu\n",
			size, ocmem_buffer->len);

ocmem_set_failed:
	spin_unlock_irqrestore(&core->lock, flags);
	return rc;
}

static int msm_comm_free_ocmem(struct msm_vidc_core *core)
{
	int rc = 0;
	unsigned long flags;
	spin_lock_irqsave(&core->lock, flags);
	if (core->resources.ocmem.buf) {
		rc = ocmem_free(OCMEM_VIDEO, core->resources.ocmem.buf);
		if (rc)
			pr_err("Failed to free ocmem\n");
	}
	core->resources.ocmem.buf = NULL;
	spin_unlock_irqrestore(&core->lock, flags);
	return rc;
}

int msm_vidc_ocmem_notify_handler(struct notifier_block *this,
		unsigned long event, void *data)
{
	struct ocmem_buf *buff = data;
	struct msm_vidc_core *core;
	struct msm_vidc_resources *resources;
	struct on_chip_mem *ocmem;
	int rc = NOTIFY_DONE;
	if (event == OCMEM_ALLOC_GROW) {
		ocmem = container_of(this, struct on_chip_mem, vidc_ocmem_nb);
		if (!ocmem) {
			pr_err("Wrong handler passed\n");
			rc = NOTIFY_BAD;
			goto bad_notfier;
		}
		resources = container_of(ocmem,
			struct msm_vidc_resources, ocmem);
		core = container_of(resources,
			struct msm_vidc_core, resources);
		if (msm_comm_set_ocmem(core, buff)) {
			pr_err("Failed to set ocmem: %d\n", rc);
			goto ocmem_set_failed;
		}
		rc = NOTIFY_OK;
	}
ocmem_set_failed:
bad_notfier:
	return rc;
}

static int msm_comm_init_core_done(struct msm_vidc_inst *inst)
{
	struct msm_vidc_core *core = inst->core;
	unsigned long flags;
	int rc = 0;
	mutex_lock(&core->sync_lock);
	if (core->state >= VIDC_CORE_INIT_DONE) {
		pr_err("Video core: %d is already in state: %d\n",
				core->id, core->state);
		goto core_already_inited;
	}
	pr_debug("Waiting for SYS_INIT_DONE\n");
	rc = wait_for_completion_timeout(
		&core->completions[SYS_MSG_INDEX(SYS_INIT_DONE)],
		msecs_to_jiffies(HW_RESPONSE_TIMEOUT));
	if (!rc) {
		pr_err("Wait interrupted or timeout: %d\n", rc);
		rc = -EIO;
		goto exit;
	} else {
		spin_lock_irqsave(&core->lock, flags);
		core->state = VIDC_CORE_INIT_DONE;
		spin_unlock_irqrestore(&core->lock, flags);
	}
	pr_debug("SYS_INIT_DONE!!!\n");
core_already_inited:
	change_inst_state(inst, MSM_VIDC_CORE_INIT_DONE);
	rc = 0;
exit:
	mutex_unlock(&core->sync_lock);
	return rc;
}

static int msm_comm_init_core(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_vidc_core *core = inst->core;
	unsigned long flags;
	mutex_lock(&core->sync_lock);
	if (core->state >= VIDC_CORE_INIT) {
		pr_err("Video core: %d is already in state: %d\n",
				core->id, core->state);
		goto core_already_inited;
	}
	rc = msm_comm_load_fw(core);
	if (rc) {
		pr_err("Failed to load video firmware\n");
		goto fail_load_fw;
	}
	init_completion(&core->completions[SYS_MSG_INDEX(SYS_INIT_DONE)]);
	rc = vidc_hal_core_init(core->device,
		core->resources.io_map[NS_MAP].domain);
	if (rc) {
		pr_err("Failed to init core, id = %d\n", core->id);
		goto fail_core_init;
	}
	spin_lock_irqsave(&core->lock, flags);
	core->state = VIDC_CORE_INIT;
	spin_unlock_irqrestore(&core->lock, flags);
core_already_inited:
	change_inst_state(inst, MSM_VIDC_CORE_INIT);
	mutex_unlock(&core->sync_lock);
	return rc;
fail_core_init:
	msm_comm_unload_fw(core);
fail_load_fw:
	mutex_unlock(&core->sync_lock);
	return rc;
}

static int msm_vidc_deinit_core(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_vidc_core *core = inst->core;
	unsigned long flags;
	mutex_lock(&core->sync_lock);
	if (core->state == VIDC_CORE_UNINIT) {
		pr_err("Video core: %d is already in state: %d\n",
				core->id, core->state);
		goto core_already_uninited;
	}
	if (list_empty(&core->instances)) {
		msm_comm_unset_ocmem(core);
		msm_comm_free_ocmem(core);
		rc = vidc_hal_core_release(core->device);
		if (rc) {
			pr_err("Failed to release core, id = %d\n", core->id);
			goto exit;
		}
		spin_lock_irqsave(&core->lock, flags);
		core->state = VIDC_CORE_UNINIT;
		spin_unlock_irqrestore(&core->lock, flags);
		msm_comm_unload_fw(core);
	}
core_already_uninited:
	change_inst_state(inst, MSM_VIDC_CORE_UNINIT);
exit:
	mutex_unlock(&core->sync_lock);
	return rc;
}

static enum hal_domain get_hal_domain(int session_type)
{
	enum hal_domain domain;
	switch (session_type) {
	case MSM_VIDC_ENCODER:
		domain = HAL_VIDEO_DOMAIN_ENCODER;
		break;
	case MSM_VIDC_DECODER:
		domain = HAL_VIDEO_DOMAIN_DECODER;
		break;
	default:
		pr_err("Wrong domain\n");
		domain = HAL_UNUSED_DOMAIN;
		break;
	}
	return domain;
}

static enum hal_video_codec get_hal_codec_type(int fourcc)
{
	enum hal_video_codec codec;
	pr_debug("codec in %s is 0x%x", __func__, fourcc);
	switch (fourcc) {
	case V4L2_PIX_FMT_H264:
	case V4L2_PIX_FMT_H264_NO_SC:
		codec = HAL_VIDEO_CODEC_H264;
		break;
	case V4L2_PIX_FMT_H263:
		codec = HAL_VIDEO_CODEC_H263;
		break;
	case V4L2_PIX_FMT_MPEG1:
		codec = HAL_VIDEO_CODEC_MPEG1;
		break;
	case V4L2_PIX_FMT_MPEG2:
		codec = HAL_VIDEO_CODEC_MPEG2;
		break;
	case V4L2_PIX_FMT_MPEG4:
		codec = HAL_VIDEO_CODEC_MPEG4;
		break;
	case V4L2_PIX_FMT_VC1_ANNEX_G:
	case V4L2_PIX_FMT_VC1_ANNEX_L:
		codec = HAL_VIDEO_CODEC_VC1;
		break;
	case V4L2_PIX_FMT_VP8:
		codec = HAL_VIDEO_CODEC_VP8;
		break;
	case V4L2_PIX_FMT_DIVX_311:
		codec = HAL_VIDEO_CODEC_DIVX_311;
		break;
	case V4L2_PIX_FMT_DIVX:
		codec = HAL_VIDEO_CODEC_DIVX;
		break;
		/*HAL_VIDEO_CODEC_MVC
		  HAL_VIDEO_CODEC_SPARK
		  HAL_VIDEO_CODEC_VP6
		  HAL_VIDEO_CODEC_VP7*/
	default:
		pr_err("Wrong codec: %d\n", fourcc);
		codec = HAL_UNUSED_CODEC;
	}
	return codec;
}

static int msm_comm_session_init(int flipped_state,
	struct msm_vidc_inst *inst)
{
	int rc = 0;
	int fourcc = 0;
	if (IS_ALREADY_IN_STATE(flipped_state, MSM_VIDC_OPEN)) {
		pr_err("inst: %p is already in state: %d\n", inst, inst->state);
		goto exit;
	}
	if (inst->session_type == MSM_VIDC_DECODER) {
		fourcc = inst->fmts[OUTPUT_PORT]->fourcc;
	} else if (inst->session_type == MSM_VIDC_ENCODER) {
		fourcc = inst->fmts[CAPTURE_PORT]->fourcc;
	} else {
		pr_err("Invalid session\n");
		return -EINVAL;
	}
	init_completion(
		&inst->completions[SESSION_MSG_INDEX(SESSION_INIT_DONE)]);
	inst->session = vidc_hal_session_init(inst->core->device, (u32) inst,
					get_hal_domain(inst->session_type),
					get_hal_codec_type(fourcc));
	if (!inst->session) {
		pr_err("Failed to call session init for: %d, %d, %d, %d\n",
				(int)inst->core->device, (int)inst,
				inst->session_type, fourcc);
		goto exit;
	}
	change_inst_state(inst, MSM_VIDC_OPEN);
exit:
	return rc;
}

static int msm_vidc_load_resources(int flipped_state,
	struct msm_vidc_inst *inst)
{
	int rc = 0;
	u32 ocmem_sz = 0;
	if (IS_ALREADY_IN_STATE(flipped_state, MSM_VIDC_LOAD_RESOURCES)) {
		pr_err("inst: %p is already in state: %d\n", inst, inst->state);
		goto exit;
	}
	ocmem_sz = get_ocmem_requirement(inst->prop.height, inst->prop.width);
	rc = msm_comm_alloc_ocmem(inst->core, ocmem_sz);
	if (rc)
		pr_warn("Failed to allocate OCMEM. Performance will be impacted\n");

	rc = vidc_hal_session_load_res((void *) inst->session);
	if (rc) {
		pr_err("Failed to send load resources\n");
		goto exit;
	}
	change_inst_state(inst, MSM_VIDC_LOAD_RESOURCES);
exit:
	return rc;
}

static int msm_vidc_start(int flipped_state, struct msm_vidc_inst *inst)
{
	int rc = 0;
	if (IS_ALREADY_IN_STATE(flipped_state, MSM_VIDC_START)) {
		pr_err("inst: %p is already in state: %d\n", inst, inst->state);
		goto exit;
	}
	init_completion(
		&inst->completions[SESSION_MSG_INDEX(SESSION_START_DONE)]);
	rc = vidc_hal_session_start((void *) inst->session);
	if (rc) {
		pr_err("Failed to send load resources\n");
		goto exit;
	}
	change_inst_state(inst, MSM_VIDC_START);
exit:
	return rc;
}

static int msm_vidc_stop(int flipped_state, struct msm_vidc_inst *inst)
{
	int rc = 0;
	if (IS_ALREADY_IN_STATE(flipped_state, MSM_VIDC_STOP)) {
		pr_err("inst: %p is already in state: %d\n", inst, inst->state);
		goto exit;
	}
	pr_debug("Send Stop to hal\n");
	init_completion(
		&inst->completions[SESSION_MSG_INDEX(SESSION_STOP_DONE)]);
	rc = vidc_hal_session_stop((void *) inst->session);
	if (rc) {
		pr_err("Failed to send stop\n");
		goto exit;
	}
	change_inst_state(inst, MSM_VIDC_STOP);
exit:
	return rc;
}

static int msm_vidc_release_res(int flipped_state, struct msm_vidc_inst *inst)
{
	int rc = 0;
	if (IS_ALREADY_IN_STATE(flipped_state, MSM_VIDC_RELEASE_RESOURCES)) {
		pr_err("inst: %p is already in state: %d\n", inst, inst->state);
		goto exit;
	}
	pr_debug("Send release res to hal\n");
	init_completion(
	&inst->completions[SESSION_MSG_INDEX(SESSION_RELEASE_RESOURCE_DONE)]);
	rc = vidc_hal_session_release_res((void *) inst->session);
	if (rc) {
		pr_err("Failed to send load resources\n");
		goto exit;
	}
	change_inst_state(inst, MSM_VIDC_RELEASE_RESOURCES);
exit:
	return rc;
}

static int msm_comm_session_close(int flipped_state, struct msm_vidc_inst *inst)
{
	int rc = 0;
	if (IS_ALREADY_IN_STATE(flipped_state, MSM_VIDC_CLOSE)) {
		pr_err("inst: %p is already in state: %d\n", inst, inst->state);
		goto exit;
	}
	pr_debug("Send session close to hal\n");
	init_completion(
		&inst->completions[SESSION_MSG_INDEX(SESSION_END_DONE)]);
	rc = vidc_hal_session_end((void *) inst->session);
	if (rc) {
		pr_err("Failed to send load resources\n");
		goto exit;
	}
	change_inst_state(inst, MSM_VIDC_OPEN);
exit:
	return rc;
}

int msm_comm_try_state(struct msm_vidc_inst *inst, int state)
{
	int rc = 0;
	int flipped_state;
	if (!inst) {
		pr_err("Invalid instance pointer = %p\n", inst);
		return -EINVAL;
	}
	pr_debug("Trying to move inst: %p from: 0x%x to 0x%x\n",
				inst, inst->state, state);
	mutex_lock(&inst->sync_lock);
	flipped_state = inst->state;
	if (flipped_state < MSM_VIDC_STOP
		&& state > MSM_VIDC_STOP) {
		flipped_state = MSM_VIDC_STOP + (MSM_VIDC_STOP - flipped_state);
		flipped_state &= 0xFFFE;
		flipped_state = flipped_state - 1;
	} else if (flipped_state > MSM_VIDC_STOP
		&& state < MSM_VIDC_STOP) {
		flipped_state = MSM_VIDC_STOP -
				(flipped_state - MSM_VIDC_STOP + 1);
		flipped_state &= 0xFFFE;
		flipped_state = flipped_state - 1;
	}
	pr_debug("flipped_state = 0x%x\n", flipped_state);
	switch (flipped_state) {
	case MSM_VIDC_CORE_UNINIT_DONE:
	case MSM_VIDC_CORE_INIT:
		rc = msm_comm_init_core(inst);
		if (rc || state <= inst->state)
			break;
	case MSM_VIDC_CORE_INIT_DONE:
		rc = msm_comm_init_core_done(inst);
		if (rc || state <= inst->state)
			break;
	case MSM_VIDC_OPEN:
		rc = msm_comm_session_init(flipped_state, inst);
		if (rc || state <= inst->state)
			break;
	case MSM_VIDC_OPEN_DONE:
		rc = wait_for_state(inst, flipped_state, MSM_VIDC_OPEN_DONE,
			SESSION_INIT_DONE);
		if (rc || state <= inst->state)
			break;
	case MSM_VIDC_LOAD_RESOURCES:
		rc = msm_vidc_load_resources(flipped_state, inst);
		if (rc || state <= inst->state)
			break;
	case MSM_VIDC_LOAD_RESOURCES_DONE:
	case MSM_VIDC_START:
		rc = msm_vidc_start(flipped_state, inst);
		if (rc || state <= inst->state)
			break;
	case MSM_VIDC_START_DONE:
		rc = wait_for_state(inst, flipped_state, MSM_VIDC_START_DONE,
				SESSION_START_DONE);
		if (rc || state <= inst->state)
			break;
	case MSM_VIDC_STOP:
		rc = msm_vidc_stop(flipped_state, inst);
		if (rc || state <= inst->state)
			break;
	case MSM_VIDC_STOP_DONE:
		rc = wait_for_state(inst, flipped_state, MSM_VIDC_STOP_DONE,
				SESSION_STOP_DONE);
		if (rc || state <= inst->state)
			break;
		pr_debug("Moving to Stop Done state\n");
	case MSM_VIDC_RELEASE_RESOURCES:
		rc = msm_vidc_release_res(flipped_state, inst);
		if (rc || state <= inst->state)
			break;
	case MSM_VIDC_RELEASE_RESOURCES_DONE:
		rc = wait_for_state(inst, flipped_state,
			MSM_VIDC_RELEASE_RESOURCES_DONE,
			SESSION_RELEASE_RESOURCE_DONE);
		if (rc || state <= inst->state)
			break;
		pr_debug("Moving to release resources done state\n");
	case MSM_VIDC_CLOSE:
		rc = msm_comm_session_close(flipped_state, inst);
		if (rc || state <= inst->state)
			break;
	case MSM_VIDC_CLOSE_DONE:
		rc = wait_for_state(inst, flipped_state, MSM_VIDC_CLOSE_DONE,
			SESSION_END_DONE);
		if (rc || state <= inst->state)
			break;
	case MSM_VIDC_CORE_UNINIT:
		pr_debug("***************Sending core uninit\n");
		rc = msm_vidc_deinit_core(inst);
		if (rc || state == inst->state)
			break;
	default:
		pr_err("State not recognized: %d\n", flipped_state);
		rc = -EINVAL;
		break;
	}
	mutex_unlock(&inst->sync_lock);
	if (rc)
		pr_err("Failed to move from state: %d to %d\n",
			inst->state, state);
	return rc;
}

int msm_comm_qbuf(struct vb2_buffer *vb)
{
	int rc = 0;
	struct vb2_queue *q;
	struct msm_vidc_inst *inst;
	unsigned long flags;
	struct vb2_buf_entry *entry;
	struct vidc_frame_data frame_data;
	q = vb->vb2_queue;
	inst = q->drv_priv;

	if (!inst || !vb) {
		pr_err("Invalid input: %p, %p\n", inst, vb);
		return -EINVAL;
	}
	if (inst->state != MSM_VIDC_START_DONE) {
			entry = kzalloc(sizeof(*entry), GFP_KERNEL);
			if (!entry) {
				pr_err("Out of memory\n");
				goto err_no_mem;
			}
			entry->vb = vb;
			pr_debug("Queueing buffer in pendingq\n");
			spin_lock_irqsave(&inst->lock, flags);
			list_add_tail(&entry->list, &inst->pendingq);
			spin_unlock_irqrestore(&inst->lock, flags);
	} else {
		memset(&frame_data, 0 , sizeof(struct vidc_frame_data));
		frame_data.alloc_len = vb->v4l2_planes[0].length;
		frame_data.filled_len = vb->v4l2_planes[0].bytesused;
		frame_data.device_addr = vb->v4l2_planes[0].m.userptr;
		frame_data.timestamp = vb->v4l2_buf.timestamp.tv_usec;
		frame_data.clnt_data = (u32)vb;
		if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
			frame_data.buffer_type = HAL_BUFFER_INPUT;
			if (vb->v4l2_buf.flags & V4L2_BUF_FLAG_EOS) {
				frame_data.flags = HAL_BUFFERFLAG_EOS;
				pr_debug("Received EOS on output capability\n");
			}
			pr_debug("Sending etb to hal: Alloc: %d :filled: %d\n",
				frame_data.alloc_len, frame_data.filled_len);
			rc = vidc_hal_session_etb((void *) inst->session,
					&frame_data);
			pr_debug("Sent etb to HAL\n");
		} else if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
			frame_data.filled_len = 0;
			frame_data.buffer_type = HAL_BUFFER_OUTPUT;
			if (inst->extradata_handle) {
				frame_data.extradata_addr =
					inst->extradata_handle->device_addr;
			} else {
				frame_data.extradata_addr = 0;
			}
			pr_debug("Sending ftb to hal..: Alloc: %d :filled: %d",
				frame_data.alloc_len, frame_data.filled_len);
			pr_debug(" extradata_addr: %d\n",
				frame_data.extradata_addr);
			rc = vidc_hal_session_ftb((void *) inst->session,
					&frame_data);
		} else {
			pr_err("This capability is not supported: %d\n",
				q->type);
			rc = -EINVAL;
		}
	}
	if (rc)
		pr_err("Failed to queue buffer\n");
err_no_mem:
	return rc;
}

int msm_comm_try_get_bufreqs(struct msm_vidc_inst *inst)
{
	int rc = 0;
	mutex_lock(&inst->sync_lock);
	init_completion(
		&inst->completions[SESSION_MSG_INDEX(SESSION_PROPERTY_INFO)]);
	rc = vidc_hal_session_get_buf_req((void *) inst->session);
	if (rc) {
		pr_err("Failed to get property\n");
		goto exit;
	}
	rc = wait_for_completion_timeout(
		&inst->completions[SESSION_MSG_INDEX(SESSION_PROPERTY_INFO)],
		msecs_to_jiffies(HW_RESPONSE_TIMEOUT));
	if (!rc) {
		pr_err("Wait interrupted or timeout: %d\n", rc);
		rc = -EIO;
		goto exit;
	}
	rc = 0;
exit:
	mutex_unlock(&inst->sync_lock);
	return rc;
}

int msm_comm_set_scratch_buffers(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_smem *handle;
	struct internal_buf *binfo;
	struct list_head *ptr, *next;
	struct vidc_buffer_addr_info buffer_info;
	unsigned long flags;
	struct hal_buffer_requirements *scratch_buf =
		&inst->buff_req.buffer[HAL_BUFFER_INTERNAL_SCRATCH];
	int i;

	pr_debug("scratch: num = %d, size = %d\n",
			scratch_buf->buffer_count_actual,
			scratch_buf->buffer_size);
	spin_lock_irqsave(&inst->lock, flags);
	if (!list_empty(&inst->internalbufs)) {
		list_for_each_safe(ptr, next, &inst->internalbufs) {
			binfo = list_entry(ptr, struct internal_buf,
					list);
			list_del(&binfo->list);
			msm_smem_free(inst->mem_client, binfo->handle);
			kfree(binfo);
		}
	}
	spin_unlock_irqrestore(&inst->lock, flags);
	if (scratch_buf->buffer_size) {
		for (i = 0; i < scratch_buf->buffer_count_actual;
				i++) {
			handle = msm_smem_alloc(inst->mem_client,
				scratch_buf->buffer_size, 1, 0,
				inst->core->resources.io_map[NS_MAP].domain, 0);
			if (!handle) {
				pr_err("Failed to allocate scratch memory\n");
				rc = -ENOMEM;
				goto err_no_mem;
			}
			binfo = kzalloc(sizeof(*binfo), GFP_KERNEL);
			if (!binfo) {
				pr_err("Out of memory\n");
				rc = -ENOMEM;
				goto fail_kzalloc;
			}
			binfo->handle = handle;
			buffer_info.buffer_size = scratch_buf->buffer_size;
			buffer_info.buffer_type = HAL_BUFFER_INTERNAL_SCRATCH;
			buffer_info.num_buffers = 1;
			buffer_info.align_device_addr = handle->device_addr;
			rc = vidc_hal_session_set_buffers(
					(void *) inst->session,	&buffer_info);
			if (rc) {
				pr_err("vidc_hal_session_set_buffers failed");
				goto fail_set_buffers;
			}
			spin_lock_irqsave(&inst->lock, flags);
			list_add_tail(&binfo->list, &inst->internalbufs);
			spin_unlock_irqrestore(&inst->lock, flags);
		}
	}
	return rc;
fail_set_buffers:
	kfree(binfo);
fail_kzalloc:
	msm_smem_free(inst->mem_client, handle);
err_no_mem:
	return rc;
}

int msm_comm_set_persist_buffers(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_smem *handle;
	struct internal_buf *binfo;
	struct vidc_buffer_addr_info buffer_info;
	unsigned long flags;
	struct hal_buffer_requirements *persist_buf =
		&inst->buff_req.buffer[HAL_BUFFER_INTERNAL_PERSIST];
	int i;
	pr_debug("persist: num = %d, size = %d\n",
		persist_buf->buffer_count_actual,
		persist_buf->buffer_size);
	if (!list_empty(&inst->persistbufs)) {
		pr_err("Persist buffers already allocated\n");
		return rc;
	}

	if (persist_buf->buffer_size) {
		for (i = 0;	i <	persist_buf->buffer_count_actual; i++) {
			handle = msm_smem_alloc(inst->mem_client,
				persist_buf->buffer_size, 1, 0,
				inst->core->resources.io_map[NS_MAP].domain, 0);
			if (!handle) {
				pr_err("Failed to allocate persist memory\n");
				rc = -ENOMEM;
				goto err_no_mem;
			}
			binfo = kzalloc(sizeof(*binfo), GFP_KERNEL);
			if (!binfo) {
				pr_err("Out of memory\n");
				rc = -ENOMEM;
				goto fail_kzalloc;
			}
			binfo->handle = handle;
			buffer_info.buffer_size = persist_buf->buffer_size;
			buffer_info.buffer_type = HAL_BUFFER_INTERNAL_PERSIST;
			buffer_info.num_buffers = 1;
			buffer_info.align_device_addr = handle->device_addr;
			rc = vidc_hal_session_set_buffers(
				(void *) inst->session, &buffer_info);
			if (rc) {
				pr_err("vidc_hal_session_set_buffers failed");
				goto fail_set_buffers;
			}
			spin_lock_irqsave(&inst->lock, flags);
			list_add_tail(&binfo->list, &inst->persistbufs);
			spin_unlock_irqrestore(&inst->lock, flags);
		}
	}
	return rc;
fail_set_buffers:
	kfree(binfo);
fail_kzalloc:
	msm_smem_free(inst->mem_client, handle);
err_no_mem:
	return rc;
}
