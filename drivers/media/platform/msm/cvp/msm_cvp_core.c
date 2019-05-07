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

	INIT_MSM_CVP_LIST(&inst->freqs);
	INIT_MSM_CVP_LIST(&inst->persistbufs);
	INIT_MSM_CVP_LIST(&inst->registeredbufs);
	INIT_MSM_CVP_LIST(&inst->cvpcpubufs);
	INIT_MSM_CVP_LIST(&inst->cvpdspbufs);

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
	DEINIT_MSM_CVP_LIST(&inst->registeredbufs);
	DEINIT_MSM_CVP_LIST(&inst->freqs);

	kfree(inst);
	inst = NULL;
err_invalid_core:
	return inst;
}
EXPORT_SYMBOL(msm_cvp_open);

static void msm_cvp_cleanup_instance(struct msm_cvp_inst *inst)
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
	DEINIT_MSM_CVP_LIST(&inst->registeredbufs);
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

	msm_cvp_cleanup_instance(inst);
	msm_cvp_session_deinit(inst);
	rc = msm_cvp_comm_try_state(inst, MSM_CVP_CORE_UNINIT);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to move inst %pK to uninit state\n", inst);
		rc = msm_cvp_comm_force_cleanup(inst);
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
