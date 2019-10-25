// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/dma-direction.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include "msm_cvp_core.h"
#include "msm_cvp_internal.h"
#include "msm_cvp_debug.h"
#include "msm_cvp.h"
#include "msm_cvp_common.h"
#include <linux/delay.h>
#include "cvp_hfi_api.h"
#include "msm_cvp_clocks.h"
#include <linux/dma-buf.h>
#include <uapi/media/msm_media_info.h>
#include <synx_api.h>

#define MAX_EVENTS 30
#define NUM_CYCLES16X16_HCD_FRAME 95
#define NUM_CYCLES16X16_DME_FRAME 600
#define NUM_CYCLES16X16_NCC_FRAME 400
#define NUM_CYCLES16X16_DS_FRAME  80
#define NUM_CYCLESFW_FRAME  1680000
#define NUM_DME_MAX_FEATURE_POINTS 500
#define CYCLES_MARGIN_IN_POWEROF2 3

int msm_cvp_est_cycles(struct cvp_kmd_usecase_desc *cvp_desc,
		struct cvp_kmd_request_power *cvp_voting)
{
	unsigned int cvp_cycles = 0;
	unsigned int hcd_cycles = 0;
	unsigned int dme_cycles = 0;
	unsigned int ds_cycles = 0;
	unsigned int ncc_cycles = 0;
	unsigned int num_16x16_blocks = 0;

	unsigned int cvp_bw = 0;
	unsigned int ds_pixel_read = 0;
	unsigned int ds_pixel_write = 0;
	unsigned int hcd_pixel_read = 0;
	unsigned int hcd_stats_write = 0;
	unsigned int dme_pixel_read = 0;
	unsigned int ncc_pixel_read = 0;
	unsigned int process_width = 0;
	unsigned int process_height = 0;

	if (!cvp_desc || !cvp_voting) {
		dprintk(CVP_ERR, "%s: invalid args\n", __func__);
		return -EINVAL;
	}

	if (cvp_desc->is_downscale) {
		num_16x16_blocks = (cvp_desc->fullres_width>>4)
			* (cvp_desc->fullres_height>>4);
		ds_cycles = NUM_CYCLES16X16_DS_FRAME * num_16x16_blocks;
		process_width = cvp_desc->downscale_width;
		process_height = cvp_desc->downscale_height;
		num_16x16_blocks = (process_width>>4)*(process_height>>4);
		hcd_cycles = NUM_CYCLES16X16_HCD_FRAME * num_16x16_blocks;
		/*Estimate downscale output (always UBWC) BW stats*/
		if (cvp_desc->fullres_width <= 1920) {
			/*w*h/1.58=w*h*(81/128)*/
			ds_pixel_write = ((process_width*process_height*81)>>7);
		} else {
			/*w*h/2.38=w*h*(54/128)*/
			ds_pixel_write = ((process_width*process_height*54)>>7);
		}
		/*Estimate downscale input BW stats based on colorfmt*/
		switch (cvp_desc->colorfmt) {
		case COLOR_FMT_NV12:
		{
			/*w*h*1.5*/
			ds_pixel_read = ((cvp_desc->fullres_width
				* cvp_desc->fullres_height * 3)>>1);
			break;
		}
		case COLOR_FMT_P010:
		{
			/*w*h*2*1.5*/
			ds_pixel_read = cvp_desc->fullres_width
				* cvp_desc->fullres_height * 3;
			break;
		}
		case COLOR_FMT_NV12_UBWC:
		{
			/*w*h*1.5/factor(factor=width>1920?2.38:1.58)*/
			if (cvp_desc->fullres_width <= 1920) {
				/*w*h*1.5/1.58 = w*h*121/128*/
				ds_pixel_read = ((cvp_desc->fullres_width
					* cvp_desc->fullres_height * 121)>>7);
			} else {
				/*w*h*1.5/1.61 = w*h*119/128*/
				ds_pixel_read = ((cvp_desc->fullres_width
					* cvp_desc->fullres_height * 119)>>7);
			}
			break;
		}
		case COLOR_FMT_NV12_BPP10_UBWC:
		{
			/*w*h*1.33*1.5/factor(factor=width>1920?2.38:1.58)*/
			if (cvp_desc->fullres_width <= 1920) {
				/*w*h*1.33*1.5/1.58 = w*h*5/4*/
				ds_pixel_read = ((cvp_desc->fullres_width
					* cvp_desc->fullres_height * 5)>>2);
			} else {
				/*w*h*1.33*1.5/1.61 = w*h*79/64*/
				ds_pixel_read = ((cvp_desc->fullres_width
					* cvp_desc->fullres_height * 79)>>6);
			}
			break;
		}
		default:
			dprintk(CVP_ERR, "Defaulting to linear P010\n");
			/*w*h*1.5*2 COLOR_FMT_P010*/
			ds_pixel_read = (cvp_desc->fullres_width
				* cvp_desc->fullres_height * 3);
		}
	} else {
		process_width = cvp_desc->fullres_width;
		process_height = cvp_desc->fullres_height;
		num_16x16_blocks = (process_width>>4)*(process_height>>4);
		hcd_cycles = NUM_CYCLES16X16_HCD_FRAME * num_16x16_blocks;
	}

	dme_cycles = NUM_CYCLES16X16_DME_FRAME * NUM_DME_MAX_FEATURE_POINTS;
	ncc_cycles = NUM_CYCLES16X16_NCC_FRAME * NUM_DME_MAX_FEATURE_POINTS;

	cvp_cycles = dme_cycles + ds_cycles + hcd_cycles + ncc_cycles;
	cvp_cycles = cvp_cycles + (cvp_cycles>>CYCLES_MARGIN_IN_POWEROF2);

	cvp_voting->clock_cycles_a = cvp_cycles * cvp_desc->fps;
	cvp_voting->clock_cycles_b = 0;
	cvp_voting->reserved[0] = NUM_CYCLESFW_FRAME * cvp_desc->fps;
	cvp_voting->reserved[1] = cvp_cycles * cvp_desc->op_rate;
	cvp_voting->reserved[2] = 0;
	cvp_voting->reserved[3] = NUM_CYCLESFW_FRAME*cvp_desc->op_rate;

	if (process_width <= 1920) {
		/*w*h*1.5(for filter fetch overhead)/1.58=w*h*(3/2)*(5/8)*/
		hcd_pixel_read = ((process_width * process_height * 15)>>4);
		/*num_16x16_blocks*8*4*/
		hcd_stats_write = (num_16x16_blocks<<5);
		/*NUM_DME_MAX_FEATURE_POINTS*96*48/1.58*/
		dme_pixel_read = NUM_DME_MAX_FEATURE_POINTS * 2880;
		/*NUM_DME_MAX_FEATURE_POINTS*(18/8+1)*32*8*2/1.58*/
		ncc_pixel_read = NUM_DME_MAX_FEATURE_POINTS * 1040;
	} else {
		/*w*h*1.5(for filter fetch overhead)/2.38=w*h*(3/2)*(54/128)*/
		hcd_pixel_read = ((process_width * process_height * 81)>>7);
		/*num_16x16_blocks*8*4*/
		hcd_stats_write = (num_16x16_blocks<<5);
		/*NUM_DME_MAX_FEATURE_POINTS*96*48/2.38*/
		dme_pixel_read = NUM_DME_MAX_FEATURE_POINTS * 1944;
		/*NUM_DME_MAX_FEATURE_POINTS*(18/8+1)*32*8*2/2.38*/
		ncc_pixel_read = NUM_DME_MAX_FEATURE_POINTS * 702;
	}

	cvp_bw = ds_pixel_read + ds_pixel_write + hcd_pixel_read
		+ hcd_stats_write + dme_pixel_read + ncc_pixel_read;

	cvp_voting->ddr_bw = cvp_bw * cvp_desc->fps;
	cvp_voting->reserved[4] = cvp_bw * cvp_desc->op_rate;

	dprintk(CVP_DBG, "%s Voting cycles_a, b, bw: %d %d %d\n", __func__,
		cvp_voting->clock_cycles_a, cvp_voting->clock_cycles_b,
		cvp_voting->ddr_bw);

	return 0;
}
EXPORT_SYMBOL(msm_cvp_est_cycles);

int msm_cvp_poll(void *instance, struct file *filp,
		struct poll_table_struct *wait)
{
	return 0;
}
EXPORT_SYMBOL(msm_cvp_poll);

int msm_cvp_private(void *cvp_inst, unsigned int cmd,
		struct cvp_kmd_arg *arg)
{
	int rc = 0;
	struct msm_cvp_inst *inst = (struct msm_cvp_inst *)cvp_inst;

	if (!inst || !arg) {
		dprintk(CVP_ERR, "%s: invalid args\n", __func__);
		return -EINVAL;
	}

	rc = msm_cvp_handle_syscall(inst, arg);

	return rc;
}
EXPORT_SYMBOL(msm_cvp_private);

static bool msm_cvp_check_for_inst_overload(struct msm_cvp_core *core)
{
	u32 instance_count = 0;
	u32 secure_instance_count = 0;
	struct msm_cvp_inst *inst = NULL;
	bool overload = false;

	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {
		instance_count++;
		/* This flag is not updated yet for the current instance */
		if (inst->flags & CVP_SECURE)
			secure_instance_count++;
	}
	mutex_unlock(&core->lock);

	/* Instance count includes current instance as well. */

	if ((instance_count >= core->resources.max_inst_count) ||
		(secure_instance_count >=
			core->resources.max_secure_inst_count))
		overload = true;
	return overload;
}

static int _init_session_queue(struct msm_cvp_inst *inst)
{
	spin_lock_init(&inst->session_queue.lock);
	INIT_LIST_HEAD(&inst->session_queue.msgs);
	inst->session_queue.msg_count = 0;
	init_waitqueue_head(&inst->session_queue.wq);
	inst->session_queue.state = QUEUE_ACTIVE;
	return 0;
}

static void _deinit_session_queue(struct msm_cvp_inst *inst)
{
	struct cvp_session_msg *msg, *tmpmsg;

	/* free all messages */
	spin_lock(&inst->session_queue.lock);
	list_for_each_entry_safe(msg, tmpmsg, &inst->session_queue.msgs, node) {
		list_del_init(&msg->node);
		kmem_cache_free(cvp_driver->msg_cache, msg);
	}
	inst->session_queue.msg_count = 0;
	inst->session_queue.state = QUEUE_STOP;
	spin_unlock(&inst->session_queue.lock);

	wake_up_all(&inst->session_queue.wq);
}

void *msm_cvp_open(int core_id, int session_type)
{
	struct msm_cvp_inst *inst = NULL;
	struct msm_cvp_core *core = NULL;
	int rc = 0;
	int i = 0;

	if (core_id >= MSM_CVP_CORES_MAX ||
			session_type >= MSM_CVP_MAX_DEVICES) {
		dprintk(CVP_ERR, "Invalid input, core_id = %d, session = %d\n",
			core_id, session_type);
		goto err_invalid_core;
	}
	core = get_cvp_core(core_id);
	if (!core) {
		dprintk(CVP_ERR,
			"Failed to find core for core_id = %d\n", core_id);
		goto err_invalid_core;
	}

	core->resources.max_inst_count = MAX_SUPPORTED_INSTANCES;
	if (msm_cvp_check_for_inst_overload(core)) {
		dprintk(CVP_ERR, "Instance num reached Max, rejecting session");
		mutex_lock(&core->lock);
		list_for_each_entry(inst, &core->instances, list)
			dprintk(CVP_ERR, "inst %pK, cmd %d id %d\n",
				inst, inst->cur_cmd_type,
				hash32_ptr(inst->session));
		mutex_unlock(&core->lock);

		return NULL;
	}

	inst = kzalloc(sizeof(*inst), GFP_KERNEL);
	if (!inst) {
		dprintk(CVP_ERR, "Failed to allocate memory\n");
		rc = -ENOMEM;
		goto err_invalid_core;
	}

	pr_info(CVP_DBG_TAG "Opening cvp instance: %pK\n", "info", inst);
	mutex_init(&inst->sync_lock);
	mutex_init(&inst->lock);
	mutex_init(&inst->fence_lock);
	spin_lock_init(&inst->event_handler.lock);

	INIT_MSM_CVP_LIST(&inst->persistbufs);
	INIT_MSM_CVP_LIST(&inst->cvpcpubufs);
	INIT_MSM_CVP_LIST(&inst->cvpdspbufs);
	INIT_MSM_CVP_LIST(&inst->frames);

	init_waitqueue_head(&inst->event_handler.wq);

	kref_init(&inst->kref);

	synx_initialize(NULL);

	inst->session_type = session_type;
	inst->state = MSM_CVP_CORE_UNINIT_DONE;
	inst->core = core;
	inst->clk_data.min_freq = 0;
	inst->clk_data.curr_freq = 0;
	inst->clk_data.ddr_bw = 0;
	inst->clk_data.sys_cache_bw = 0;
	inst->clk_data.bitrate = 0;
	inst->clk_data.core_id = 0;
	inst->deprecate_bitmask = 0;

	for (i = SESSION_MSG_INDEX(SESSION_MSG_START);
		i <= SESSION_MSG_INDEX(SESSION_MSG_END); i++) {
		init_completion(&inst->completions[i]);
	}

	msm_cvp_session_init(inst);

	mutex_lock(&core->lock);
	list_add_tail(&inst->list, &core->instances);
	mutex_unlock(&core->lock);

	rc = _init_session_queue(inst);
	if (rc)
		goto fail_init;

	rc = msm_cvp_comm_try_state(inst, MSM_CVP_CORE_INIT_DONE);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to move cvp instance to init state\n");
		goto fail_init;
	}

	inst->debugfs_root =
		msm_cvp_debugfs_init_inst(inst, core->debugfs_root);

	return inst;
fail_init:
	_deinit_session_queue(inst);
	mutex_lock(&core->lock);
	list_del(&inst->list);
	mutex_unlock(&core->lock);
	mutex_destroy(&inst->sync_lock);
	mutex_destroy(&inst->lock);
	mutex_destroy(&inst->fence_lock);

	DEINIT_MSM_CVP_LIST(&inst->persistbufs);
	DEINIT_MSM_CVP_LIST(&inst->cvpcpubufs);
	DEINIT_MSM_CVP_LIST(&inst->cvpdspbufs);
	DEINIT_MSM_CVP_LIST(&inst->frames);

	synx_uninitialize();

	kfree(inst);
	inst = NULL;
err_invalid_core:
	return inst;
}
EXPORT_SYMBOL(msm_cvp_open);

static void msm_cvp_cleanup_instance(struct msm_cvp_inst *inst)
{
	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return;
	}

	if (cvp_comm_release_persist_buffers(inst))
		dprintk(CVP_ERR,
			"Failed to release persist buffers\n");
}

int msm_cvp_destroy(struct msm_cvp_inst *inst)
{
	struct msm_cvp_core *core;

	if (!inst || !inst->core) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	core = inst->core;

	mutex_lock(&core->lock);
	/* inst->list lives in core->instances */
	list_del(&inst->list);
	mutex_unlock(&core->lock);

	synx_uninitialize();

	DEINIT_MSM_CVP_LIST(&inst->persistbufs);
	DEINIT_MSM_CVP_LIST(&inst->cvpcpubufs);
	DEINIT_MSM_CVP_LIST(&inst->cvpdspbufs);
	DEINIT_MSM_CVP_LIST(&inst->frames);

	mutex_destroy(&inst->sync_lock);
	mutex_destroy(&inst->lock);
	mutex_destroy(&inst->fence_lock);

	msm_cvp_debugfs_deinit_inst(inst);
	_deinit_session_queue(inst);

	pr_info(CVP_DBG_TAG "Closed cvp instance: %pK session_id = %d\n",
		"info", inst, hash32_ptr(inst->session));
	if (inst->cur_cmd_type)
		dprintk(CVP_ERR, "deleted instance has pending cmd %d\n",
				inst->cur_cmd_type);
	inst->session = (void *)0xdeadbeef;
	kfree(inst);
	return 0;
}

static void close_helper(struct kref *kref)
{
	struct msm_cvp_inst *inst = container_of(kref,
			struct msm_cvp_inst, kref);

	msm_cvp_destroy(inst);
}

int msm_cvp_close(void *instance)
{
	struct msm_cvp_inst *inst = instance;
	int rc = 0;

	if (!inst || !inst->core) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	msm_cvp_cleanup_instance(inst);

	if (inst->session_type != MSM_CVP_BOOT)
		msm_cvp_session_deinit(inst);

	rc = msm_cvp_comm_try_state(inst, MSM_CVP_CORE_UNINIT);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to move inst %pK to uninit state\n", inst);
		rc = msm_cvp_deinit_core(inst);
	}

	msm_cvp_comm_session_clean(inst);

	kref_put(&inst->kref, close_helper);
	return 0;
}
EXPORT_SYMBOL(msm_cvp_close);

int msm_cvp_suspend(int core_id)
{
	return msm_cvp_comm_suspend(core_id);
}
EXPORT_SYMBOL(msm_cvp_suspend);
