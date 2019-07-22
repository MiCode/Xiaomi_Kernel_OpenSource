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

	if (!cvp_desc || !cvp_voting) {
		dprintk(CVP_ERR, "%s: invalid args\n", __func__);
		return -EINVAL;
	}

	if (cvp_desc->is_downscale) {
		num_16x16_blocks = (cvp_desc->fullres_width>>4)
			* (cvp_desc->fullres_height>>4);
		ds_cycles = NUM_CYCLES16X16_DS_FRAME * num_16x16_blocks;
		num_16x16_blocks = (cvp_desc->downscale_width>>4)
			* (cvp_desc->downscale_height>>4);
		hcd_cycles = NUM_CYCLES16X16_HCD_FRAME * num_16x16_blocks;
	} else {
		num_16x16_blocks = (cvp_desc->fullres_width>>4)
			* (cvp_desc->fullres_height>>4);
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

	if (cvp_desc->is_downscale) {
		if (cvp_desc->fullres_width <= 1920) {
			/*
			 *w*h*1.33(10bpc)*1.5/1.58=
			 *w*h*(4/3)*(3/2)*(5/8)=w*h*(5/4)
			 */
			ds_pixel_read = ((cvp_desc->fullres_width
				* cvp_desc->fullres_height * 5)>>2);
			/*w*h/1.58=w*h*(5/8)*/
			ds_pixel_write = ((cvp_desc->downscale_width
				* cvp_desc->downscale_height * 5)>>3);
			/*w*h*1.5/1.58=w*h*(3/2)*(5/8)*/
			hcd_pixel_read = ((cvp_desc->downscale_width
				* cvp_desc->downscale_height * 15)>>4);
			/*num_16x16_blocks*8*4*/
			hcd_stats_write = (num_16x16_blocks<<5);
			/*NUM_DME_MAX_FEATURE_POINTS*96*48/1.58*/
			dme_pixel_read = NUM_DME_MAX_FEATURE_POINTS * 2880;
			/*NUM_DME_MAX_FEATURE_POINTS*(18/8+1)*32*8*2/1.58*/
			ncc_pixel_read = NUM_DME_MAX_FEATURE_POINTS * 1040;
		} else {
			/*
			 *w*h*1.33(10bpc)*1.5/2.38=
			 *w*h*(4/3)*(3/2)*(54/128)=w*h*(54/64)
			 */
			ds_pixel_read = ((cvp_desc->fullres_width
				* cvp_desc->fullres_height * 54)>>6);
			/*w*h/2.38=w*h*(54/128)*/
			ds_pixel_write = ((cvp_desc->downscale_width
				* cvp_desc->downscale_height * 54)>>7);
			/*w*h*1.5/2.38=w*h*(3/2)*(54/128)*/
			hcd_pixel_read = ((cvp_desc->downscale_width
				* cvp_desc->downscale_height * 81)>>7);
			/*num_16x16_blocks*8*4*/
			hcd_stats_write = (num_16x16_blocks<<5);
			/*NUM_DME_MAX_FEATURE_POINTS*96*48/2.38*/
			dme_pixel_read = NUM_DME_MAX_FEATURE_POINTS * 1944;
			/*NUM_DME_MAX_FEATURE_POINTS*(18/8+1)*32*8*2/2.38*/
			ncc_pixel_read = NUM_DME_MAX_FEATURE_POINTS * 702;
		}
	} else {
		if (cvp_desc->fullres_width <= 1920) {
			/*w*h*1.5/1.58=w*h*(3/2)*(5/8)*/
			hcd_pixel_read = ((cvp_desc->fullres_width
				* cvp_desc->fullres_height * 15)>>4);
			/*num_16x16_blocks*8*4*/
			hcd_stats_write = (num_16x16_blocks<<5);
			/*NUM_DME_MAX_FEATURE_POINTS*96*48/1.58*/
			dme_pixel_read = NUM_DME_MAX_FEATURE_POINTS * 2880;
			/*NUM_DME_MAX_FEATURE_POINTS*(18/8+1)*32*8*2/1.58*/
			ncc_pixel_read = NUM_DME_MAX_FEATURE_POINTS * 1040;
		} else {
			/*w*h*1.5/2.38=w*h*(3/2)*(54/128)*/
			hcd_pixel_read = ((cvp_desc->fullres_width
				* cvp_desc->fullres_height * 81)>>7);
			/*num_16x16_blocks*8*4*/
			hcd_stats_write = (num_16x16_blocks<<5);
			/*NUM_DME_MAX_FEATURE_POINTS*96*48/2.38*/
			dme_pixel_read = NUM_DME_MAX_FEATURE_POINTS * 1944;
			/*NUM_DME_MAX_FEATURE_POINTS*(18/8+1)*32*8*2/2.38*/
			ncc_pixel_read = NUM_DME_MAX_FEATURE_POINTS * 702;
		}
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

	if (inst->session_type == MSM_CVP_CORE) {
		rc = msm_cvp_handle_syscall(inst, arg);
	} else {
		dprintk(CVP_ERR,
			"%s: private cmd %#x not supported for session_type %d\n",
			__func__, cmd, inst->session_type);
		rc = -EINVAL;
	}

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

	if ((instance_count > core->resources.max_inst_count) ||
		(secure_instance_count > core->resources.max_secure_inst_count))
		overload = true;
	return overload;
}

static int _init_session_queue(struct msm_cvp_inst *inst)
{
	spin_lock_init(&inst->session_queue.lock);
	INIT_LIST_HEAD(&inst->session_queue.msgs);
	inst->session_queue.msg_count = 0;
	init_waitqueue_head(&inst->session_queue.wq);
	inst->session_queue.msg_cache = KMEM_CACHE(cvp_session_msg, 0);
	if (!inst->session_queue.msg_cache) {
		dprintk(CVP_ERR, "Failed to allocate msg quque\n");
		return -ENOMEM;
	}
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
		kmem_cache_free(inst->session_queue.msg_cache, msg);
	}
	inst->session_queue.msg_count = 0;
	spin_unlock(&inst->session_queue.lock);

	wake_up_all(&inst->session_queue.wq);

	kmem_cache_destroy(inst->session_queue.msg_cache);
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

	inst = kzalloc(sizeof(*inst), GFP_KERNEL);
	if (!inst) {
		dprintk(CVP_ERR, "Failed to allocate memory\n");
		rc = -ENOMEM;
		goto err_invalid_core;
	}

	pr_info(CVP_DBG_TAG "Opening CVP instance: %pK, %d\n",
		"info", inst, session_type);
	mutex_init(&inst->sync_lock);
	mutex_init(&inst->lock);
	spin_lock_init(&inst->event_handler.lock);

	INIT_MSM_CVP_LIST(&inst->freqs);
	INIT_MSM_CVP_LIST(&inst->persistbufs);
	INIT_MSM_CVP_LIST(&inst->cvpcpubufs);
	INIT_MSM_CVP_LIST(&inst->cvpdspbufs);
	init_waitqueue_head(&inst->event_handler.wq);

	kref_init(&inst->kref);

	inst->session_type = session_type;
	inst->state = MSM_CVP_CORE_UNINIT_DONE;
	inst->core = core;
	inst->clk_data.min_freq = 0;
	inst->clk_data.curr_freq = 0;
	inst->clk_data.ddr_bw = 0;
	inst->clk_data.sys_cache_bw = 0;
	inst->clk_data.bitrate = 0;
	inst->clk_data.core_id = CVP_CORE_ID_DEFAULT;
	inst->deprecate_bitmask = 0;

	for (i = SESSION_MSG_INDEX(SESSION_MSG_START);
		i <= SESSION_MSG_INDEX(SESSION_MSG_END); i++) {
		init_completion(&inst->completions[i]);
	}

	if (session_type == MSM_CVP_CORE) {
		msm_cvp_session_init(inst);
	}

	mutex_lock(&core->lock);
	list_add_tail(&inst->list, &core->instances);
	mutex_unlock(&core->lock);

	rc = _init_session_queue(inst);
	if (rc)
		goto fail_init;

	rc = msm_cvp_comm_try_state(inst, MSM_CVP_CORE_INIT_DONE);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to move video instance to init state\n");
		goto fail_init;
	}

	msm_cvp_dcvs_try_enable(inst);
	core->resources.max_inst_count = MAX_SUPPORTED_INSTANCES;
	if (msm_cvp_check_for_inst_overload(core)) {
		dprintk(CVP_ERR,
			"Instance count reached Max limit, rejecting session");
		goto fail_init;
	}

	msm_cvp_comm_scale_clocks_and_bus(inst);

	inst->debugfs_root =
		msm_cvp_debugfs_init_inst(inst, core->debugfs_root);

	if (inst->session_type == MSM_CVP_CORE) {
		rc = msm_cvp_comm_try_state(inst, MSM_CVP_OPEN_DONE);
		if (rc) {
			dprintk(CVP_ERR,
				"Failed to move video instance to open done state\n");
			goto fail_init;
		}
		rc = cvp_comm_set_arp_buffers(inst);
		if (rc) {
			dprintk(CVP_ERR,
				"Failed to set ARP buffers\n");
			goto fail_init;
		}

	}

	return inst;
fail_init:
	_deinit_session_queue(inst);
	mutex_lock(&core->lock);
	list_del(&inst->list);
	mutex_unlock(&core->lock);
	mutex_destroy(&inst->sync_lock);
	mutex_destroy(&inst->lock);

	DEINIT_MSM_CVP_LIST(&inst->persistbufs);
	DEINIT_MSM_CVP_LIST(&inst->cvpcpubufs);
	DEINIT_MSM_CVP_LIST(&inst->cvpdspbufs);
	DEINIT_MSM_CVP_LIST(&inst->freqs);

	kfree(inst);
	inst = NULL;
err_invalid_core:
	return inst;
}
EXPORT_SYMBOL(msm_cvp_open);

void msm_cvp_cleanup_instance(struct msm_cvp_inst *inst)
{
	int rc = 0;
	struct msm_cvp_internal_buffer *cbuf, *dummy;
	struct cvp_hal_session *session;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return;
	}

	session = (struct cvp_hal_session *)inst->session;
	if (!session) {
		dprintk(CVP_ERR, "%s: invalid session\n", __func__);
		return;
	}

	mutex_lock(&inst->cvpcpubufs.lock);
	list_for_each_entry_safe(cbuf, dummy, &inst->cvpcpubufs.list,
			list) {
		print_client_buffer(CVP_DBG, "remove from cvpcpubufs",
				inst, &cbuf->buf);
		msm_cvp_smem_unmap_dma_buf(inst, &cbuf->smem);
		list_del(&cbuf->list);
	}
	mutex_unlock(&inst->cvpcpubufs.lock);

	mutex_lock(&inst->cvpdspbufs.lock);
	list_for_each_entry_safe(cbuf, dummy, &inst->cvpdspbufs.list,
			list) {
		print_client_buffer(CVP_DBG, "remove from cvpdspbufs",
				inst, &cbuf->buf);
		rc = cvp_dsp_deregister_buffer(
			(uint32_t)cbuf->smem.device_addr,
			cbuf->buf.index, cbuf->buf.size,
			hash32_ptr(session));
		if (rc)
			dprintk(CVP_ERR,
				"%s: failed dsp deregistration fd=%d rc=%d",
				__func__, cbuf->buf.fd, rc);

		msm_cvp_smem_unmap_dma_buf(inst, &cbuf->smem);
		list_del(&cbuf->list);
	}
	mutex_unlock(&inst->cvpdspbufs.lock);

	msm_cvp_comm_free_freq_table(inst);

	if (cvp_comm_release_persist_buffers(inst))
		dprintk(CVP_ERR,
			"Failed to release persist buffers\n");

	if (inst->extradata_handle)
		msm_cvp_comm_smem_free(inst, inst->extradata_handle);
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

	DEINIT_MSM_CVP_LIST(&inst->persistbufs);
	DEINIT_MSM_CVP_LIST(&inst->cvpcpubufs);
	DEINIT_MSM_CVP_LIST(&inst->cvpdspbufs);
	DEINIT_MSM_CVP_LIST(&inst->freqs);

	mutex_destroy(&inst->sync_lock);
	mutex_destroy(&inst->lock);

	msm_cvp_debugfs_deinit_inst(inst);
	_deinit_session_queue(inst);

	pr_info(CVP_DBG_TAG "Closed cvp instance: %pK\n",
			"info", inst);
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

	inst = cvp_get_inst_validate(inst->core, inst);
	if (!inst)
		return 0;

	msm_cvp_cleanup_instance(inst);
	msm_cvp_session_deinit(inst);
	rc = msm_cvp_comm_try_state(inst, MSM_CVP_CORE_UNINIT);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to move inst %pK to uninit state\n", inst);
		rc = msm_cvp_comm_force_cleanup(inst);
	}

	msm_cvp_comm_session_clean(inst);

	cvp_put_inst(inst);
	kref_put(&inst->kref, close_helper);
	return 0;
}
EXPORT_SYMBOL(msm_cvp_close);

int msm_cvp_suspend(int core_id)
{
	return msm_cvp_comm_suspend(core_id);
}
EXPORT_SYMBOL(msm_cvp_suspend);
