// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
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
#include <media/msm_media_info.h>

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
	dprintk(CVP_ERR, "Deprecated cvp func %s\n", __func__);
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

static int __init_session_queue(struct msm_cvp_inst *inst)
{
	spin_lock_init(&inst->session_queue.lock);
	INIT_LIST_HEAD(&inst->session_queue.msgs);
	inst->session_queue.msg_count = 0;
	init_waitqueue_head(&inst->session_queue.wq);
	inst->session_queue.state = QUEUE_ACTIVE;
	return 0;
}

static void __init_fence_queue(struct msm_cvp_inst *inst)
{
	mutex_init(&inst->fence_cmd_queue.lock);
	INIT_LIST_HEAD(&inst->fence_cmd_queue.wait_list);
	INIT_LIST_HEAD(&inst->fence_cmd_queue.sched_list);
	init_waitqueue_head(&inst->fence_cmd_queue.wq);
	inst->fence_cmd_queue.state = QUEUE_ACTIVE;
	inst->fence_cmd_queue.mode = OP_NORMAL;

	spin_lock_init(&inst->session_queue_fence.lock);
	INIT_LIST_HEAD(&inst->session_queue_fence.msgs);
	inst->session_queue_fence.msg_count = 0;
	init_waitqueue_head(&inst->session_queue_fence.wq);
	inst->session_queue_fence.state = QUEUE_ACTIVE;
}

static void __deinit_fence_queue(struct msm_cvp_inst *inst)
{
	mutex_destroy(&inst->fence_cmd_queue.lock);
	inst->fence_cmd_queue.state = QUEUE_INVALID;
	inst->fence_cmd_queue.mode = OP_INVALID;
}

static void __deinit_session_queue(struct msm_cvp_inst *inst)
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

	if (!core->resources.auto_pil && session_type == MSM_CVP_BOOT) {
		dprintk(CVP_SESS, "Auto PIL disabled, bypass CVP init at boot");
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

	pr_info(CVP_DBG_TAG "Opening cvp instance: %pK\n", "sess", inst);
	mutex_init(&inst->sync_lock);
	mutex_init(&inst->lock);
	spin_lock_init(&inst->event_handler.lock);

	INIT_MSM_CVP_LIST(&inst->persistbufs);
	INIT_DMAMAP_CACHE(&inst->dma_cache);
	INIT_MSM_CVP_LIST(&inst->cvpdspbufs);
	INIT_MSM_CVP_LIST(&inst->frames);

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
	inst->clk_data.core_id = 0;

	for (i = SESSION_MSG_INDEX(SESSION_MSG_START);
		i <= SESSION_MSG_INDEX(SESSION_MSG_END); i++) {
		init_completion(&inst->completions[i]);
	}

	msm_cvp_session_init(inst);

	mutex_lock(&core->lock);
	list_add_tail(&inst->list, &core->instances);
	mutex_unlock(&core->lock);

	__init_fence_queue(inst);

	rc = __init_session_queue(inst);
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
	__deinit_session_queue(inst);
	mutex_lock(&core->lock);
	list_del(&inst->list);
	mutex_unlock(&core->lock);
	mutex_destroy(&inst->sync_lock);
	mutex_destroy(&inst->lock);

	DEINIT_MSM_CVP_LIST(&inst->persistbufs);
	DEINIT_DMAMAP_CACHE(&inst->dma_cache);
	DEINIT_MSM_CVP_LIST(&inst->cvpdspbufs);
	DEINIT_MSM_CVP_LIST(&inst->frames);

	kfree(inst);
	inst = NULL;
err_invalid_core:
	return inst;
}
EXPORT_SYMBOL(msm_cvp_open);

static void msm_cvp_clean_sess_queue(struct msm_cvp_inst *inst,
		struct cvp_session_queue *sq)
{
	struct cvp_session_msg *mptr, *dummy;
	u64 ktid;

	spin_lock(&sq->lock);
	if (sq->msg_count && sq->state != QUEUE_ACTIVE) {
		list_for_each_entry_safe(mptr, dummy, &sq->msgs, node) {
			ktid = mptr->pkt.client_data.kdata;
			if (ktid) {
				list_del_init(&mptr->node);
				sq->msg_count--;
				msm_cvp_unmap_frame(inst, ktid);
				kmem_cache_free(cvp_driver->msg_cache, mptr);
			}
		}
	}
	spin_unlock(&sq->lock);
}

static void msm_cvp_cleanup_instance(struct msm_cvp_inst *inst)
{
	bool empty;
	int max_retries;
	struct msm_cvp_frame *frame;
	struct cvp_session_queue *sq, *sqf;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return;
	}

	sqf = &inst->session_queue_fence;
	sq = &inst->session_queue;

	max_retries =  inst->core->resources.msm_cvp_hw_rsp_timeout >> 1;
	msm_cvp_session_queue_stop(inst);

wait:
	mutex_lock(&inst->frames.lock);
	empty = list_empty(&inst->frames.list);
	if (!empty && max_retries > 0) {
		mutex_unlock(&inst->frames.lock);
		usleep_range(1000, 2000);
		msm_cvp_clean_sess_queue(inst, sqf);
		msm_cvp_clean_sess_queue(inst, sq);
		max_retries--;
		goto wait;
	}
	mutex_unlock(&inst->frames.lock);

	if (!empty) {
		dprintk(CVP_WARN,
			"Failed to process frames before session close\n");
		mutex_lock(&inst->frames.lock);
		list_for_each_entry(frame, &inst->frames.list, list)
			dprintk(CVP_WARN, "Unprocessed frame %d\n",
				frame->pkt_type);
		mutex_unlock(&inst->frames.lock);
		cvp_dump_fence_queue(inst);
	}

	if (cvp_release_arp_buffers(inst))
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

	DEINIT_MSM_CVP_LIST(&inst->persistbufs);
	DEINIT_DMAMAP_CACHE(&inst->dma_cache);
	DEINIT_MSM_CVP_LIST(&inst->cvpdspbufs);
	DEINIT_MSM_CVP_LIST(&inst->frames);

	mutex_destroy(&inst->sync_lock);
	mutex_destroy(&inst->lock);

	msm_cvp_debugfs_deinit_inst(inst);

	__deinit_session_queue(inst);
	__deinit_fence_queue(inst);
	synx_uninitialize(inst->synx_session_id);

	pr_info(CVP_DBG_TAG "Closed cvp instance: %pK session_id = %d\n",
		"sess", inst, hash32_ptr(inst->session));
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

	if (inst->session_type != MSM_CVP_BOOT) {
		msm_cvp_cleanup_instance(inst);
		msm_cvp_session_deinit(inst);
	}

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
