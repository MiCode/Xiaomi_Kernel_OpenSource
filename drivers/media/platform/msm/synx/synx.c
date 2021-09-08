// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */
#define pr_fmt(fmt) "synx: " fmt

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#include "synx_api.h"
#include "synx_util.h"
#include "synx_debugfs.h"

struct synx_device *synx_dev;

void synx_external_callback(s32 sync_obj, int status, void *data)
{
	int rc;
	struct synx_handle_coredata *synx_data;
	struct synx_coredata *synx_obj;
	struct synx_client *client = NULL;
	struct synx_external_data *bind_data = data;

	if (!bind_data) {
		pr_err("invalid payload from sync external obj %d\n",
			sync_obj);
		return;
	}

	client = synx_get_client(bind_data->session_id);
	if (!client) {
		pr_err("invalid payload content from sync external obj %d\n",
			sync_obj);
		goto free;
	}

	synx_data = synx_util_acquire_handle(client, bind_data->h_synx);
	synx_obj = synx_util_obtain_object(synx_data);
	if (!synx_obj || !synx_obj->fence) {
		pr_err("[sess: %u] invalid callback from external obj %d handle %d\n",
			client->id, sync_obj, bind_data->h_synx);
		goto fail;
	}

	pr_debug("[sess: %u] external callback from %d on handle %d\n",
		client->id, sync_obj, bind_data->h_synx);

	mutex_lock(&synx_obj->obj_lock);
	rc = synx_signal_fence(synx_obj, status);
	if (rc)
		pr_err("[sess: %u] signaling failed for handle %d with err: %d\n",
			client->id, bind_data->h_synx, rc);
	else
		synx_signal_core(synx_obj, status, true, sync_obj);
	mutex_unlock(&synx_obj->obj_lock);

fail:
	synx_util_release_handle(synx_data);
	synx_put_client(client);
free:
	kfree(bind_data);
}
EXPORT_SYMBOL(synx_external_callback);

bool synx_fence_enable_signaling(struct dma_fence *fence)
{
	return true;
}

const char *synx_fence_driver_name(struct dma_fence *fence)
{
	return "Global Synx driver";
}

void synx_fence_release(struct dma_fence *fence)
{
	/* release the memory allocated during create */
	kfree(fence->lock);
	kfree(fence);
	pr_debug("released synx backing fence %pK\n", fence);
}
EXPORT_SYMBOL(synx_fence_release);

static struct dma_fence_ops synx_fence_ops = {
	.wait = dma_fence_default_wait,
	.enable_signaling = synx_fence_enable_signaling,
	.get_driver_name = synx_fence_driver_name,
	.get_timeline_name = synx_fence_driver_name,
	.release = synx_fence_release,
};

struct dma_fence *synx_get_fence(struct synx_session session_id,
	s32 h_synx)
{
	struct synx_client *client;
	struct synx_handle_coredata *synx_data;
	struct synx_coredata *synx_obj;
	struct dma_fence *fence = NULL;

	pr_debug("[sess: %u] Enter from pid %d\n",
		session_id.client_id, current->pid);

	client = synx_get_client(session_id);
	if (!client)
		return NULL;

	synx_data = synx_util_acquire_handle(client, h_synx);
	synx_obj = synx_util_obtain_object(synx_data);
	if (!synx_obj || !synx_obj->fence) {
		pr_err("%s: [sess: %u] invalid handle access %d\n",
			__func__, client->id, h_synx);
		goto fail;
	}

	mutex_lock(&synx_obj->obj_lock);
	fence = synx_obj->fence;
	/* obtain an additional reference to the fence */
	dma_fence_get(fence);
	mutex_unlock(&synx_obj->obj_lock);

fail:
	synx_util_release_handle(synx_data);
	synx_put_client(client);
	pr_debug("[sess: %u] Exit from pid %d\n",
		session_id.client_id, current->pid);
	return fence;
}
EXPORT_SYMBOL(synx_get_fence);

int synx_create(struct synx_session session_id,
	struct synx_create_params *params)
{
	int rc;
	long h_synx;
	struct synx_client *client;
	struct synx_coredata *synx_obj;

	pr_debug("[sess: %u] Enter create from pid %d\n",
		session_id.client_id, current->pid);

	if (!params || !params->h_synx) {
		pr_err("[sess: %u] invalid create arguments\n",
			session_id.client_id);
		return -EINVAL;
	}

	client = synx_get_client(session_id);
	if (!client)
		return -EINVAL;

	synx_obj = kzalloc(sizeof(*synx_obj), GFP_KERNEL);
	if (!synx_obj) {
		rc = -ENOMEM;
		goto fail;
	}

	rc = synx_util_init_coredata(synx_obj, params, &synx_fence_ops);
	if (rc) {
		pr_err("[sess: %u] error initializing synx obj\n",
			client->id);
		goto clear;
	}

	rc = synx_util_init_handle(client, synx_obj, &h_synx);
	if (rc < 0) {
		pr_err("[sess: %u] unable to init new handle\n",
			client->id);
		goto clean_up;
	}

	*params->h_synx = h_synx;
	pr_debug("[sess: %u] new synx obj with handle %ld, fence %pK\n",
		client->id, h_synx, synx_obj);
	synx_put_client(client);
	return 0;

clean_up:
	if (!synx_util_is_external_object(synx_obj)) {
		dma_fence_remove_callback(synx_obj->fence,
			&synx_obj->fence_cb);
		dma_fence_put(synx_obj->fence);
	}
clear:
	kfree(synx_obj);
fail:
	synx_put_client(client);
	return rc;
}
EXPORT_SYMBOL(synx_create);

int synx_signal_core(struct synx_coredata *synx_obj,
	u32 status,
	bool cb_signal,
	s32 ext_sync_id)
{
	int rc = 0, ret;
	u32 i = 0;
	u32 idx = 0;
	s32 sync_id;
	u32 type;
	struct synx_external_data *data = NULL;
	struct synx_bind_desc bind_descs[SYNX_MAX_NUM_BINDINGS];
	struct bind_operations *bind_ops = NULL;

	if (!synx_obj)
		return -EINVAL;

	synx_util_callback_dispatch(synx_obj, status);

	/*
	 * signal the external bound sync obj/s even if fence signal fails,
	 * w/ error signal state (set above) to prevent deadlock
	 */
	if (synx_obj->num_bound_synxs > 0) {
		memset(bind_descs, 0,
			sizeof(struct synx_bind_desc) * SYNX_MAX_NUM_BINDINGS);
		for (i = 0; i < synx_obj->num_bound_synxs; i++) {
			/* signal invoked by external sync obj */
			if (cb_signal &&
				(ext_sync_id ==
				synx_obj->bound_synxs[i].external_desc.id[0])) {
				pr_debug("skipping signaling inbound sync: %d\n",
					ext_sync_id);
				memset(&synx_obj->bound_synxs[i], 0,
					sizeof(struct synx_bind_desc));
				continue;
			}
			memcpy(&bind_descs[idx++],
				&synx_obj->bound_synxs[i],
				sizeof(struct synx_bind_desc));
			/* clear the memory, its been backed up above */
			memset(&synx_obj->bound_synxs[i], 0,
				sizeof(struct synx_bind_desc));
		}
		synx_obj->num_bound_synxs = 0;
	}

	for (i = 0; i < idx; i++) {
		sync_id = bind_descs[i].external_desc.id[0];
		data = bind_descs[i].external_data;
		type = bind_descs[i].external_desc.type;
		bind_ops = synx_util_get_bind_ops(type);
		if (!bind_ops) {
			pr_err("invalid bind ops for type: %u\n", type);
			kfree(data);
			continue;
		}
		/*
		 * we are already signaled, so don't want to
		 * recursively be signaled
		 */
		ret = bind_ops->deregister_callback(
				synx_external_callback, data, sync_id);
		if (ret < 0) {
			pr_err("deregistration fail on %d, type: %u, err: %d\n",
				sync_id, type, ret);
			continue;
		}
		pr_debug("signal external sync: %d, type: %u, status: %u\n",
			sync_id, type, status);
		/* optional function to enable external signaling */
		if (bind_ops->enable_signaling) {
			ret = bind_ops->enable_signaling(sync_id);
			if (ret < 0)
				pr_err("enabling fail on %d, type: %u, err: %d\n",
					sync_id, type, ret);
		}
		ret = bind_ops->signal(sync_id, status);
		if (ret < 0)
			pr_err("signaling fail on %d, type: %u, err: %d\n",
				sync_id, type, ret);
		/*
		 * release the memory allocated for external data.
		 * It is safe to release this memory as external cb
		 * has been already deregistered before this.
		 */
		kfree(data);
	}

	return rc;
}

static int synx_signal_global(struct synx_coredata *synx_obj)
{
	return 0;
}

void synx_fence_callback(struct dma_fence *fence,
	struct dma_fence_cb *cb)
{
	struct synx_coredata *synx_obj =
		container_of(cb, struct synx_coredata, fence_cb);

	synx_signal_global(synx_obj);
}
EXPORT_SYMBOL(synx_fence_callback);

int synx_signal_fence(struct synx_coredata *synx_obj,
	u32 status)
{
	int rc = 0;
	unsigned long flags;

	if (!synx_obj || !synx_obj->fence)
		return -EINVAL;

	if (status < SYNX_STATE_SIGNALED_SUCCESS) {
		pr_err("signaling with wrong status = %u\n",
			status);
		return -EINVAL;
	}

	if (synx_util_is_merged_object(synx_obj)) {
		pr_err("signaling a composite object\n");
		return -EINVAL;
	}

	if (synx_util_get_object_status(synx_obj) !=
		SYNX_STATE_ACTIVE)
		return -EALREADY;

	/*
	 * remove registered callback for the fence
	 * so it does not invoke the signal through callback again
	 */
	if (!dma_fence_remove_callback(synx_obj->fence,
		&synx_obj->fence_cb)) {
		pr_err("synx callback could not be removed\n");
		return -EINVAL;
	}

	spin_lock_irqsave(synx_obj->fence->lock, flags);
	/* set fence error to model {signal w/ error} */
	if (status != SYNX_STATE_SIGNALED_SUCCESS)
		dma_fence_set_error(synx_obj->fence, -status);

	rc = dma_fence_signal_locked(synx_obj->fence);
	if (rc)
		pr_err("signaling object failed with err: %d\n", rc);
	spin_unlock_irqrestore(synx_obj->fence->lock, flags);

	return rc;
}

int synx_signal(struct synx_session session_id, s32 h_synx, u32 status)
{
	int rc = 0;
	struct synx_client *client;
	struct synx_handle_coredata *synx_data;
	struct synx_coredata *synx_obj;

	pr_debug("[sess: %u] Enter signal from pid %d\n",
		session_id.client_id, current->pid);

	client = synx_get_client(session_id);
	if (!client)
		return -EINVAL;

	synx_data = synx_util_acquire_handle(client, h_synx);
	synx_obj = synx_util_obtain_object(synx_data);
	if (!synx_obj || !synx_obj->fence) {
		pr_err("%s: [sess: %u] invalid handle access %d\n",
			__func__, client->id, h_synx);
		rc = -EINVAL;
		goto fail;
	}

	mutex_lock(&synx_obj->obj_lock);
	rc = synx_signal_fence(synx_obj, status);
	if (rc)
		pr_err("[sess: %u] signaling failed for handle %d with err: %d\n",
			client->id, h_synx, rc);
	else
		rc = synx_signal_core(synx_obj, status, false, 0);
	mutex_unlock(&synx_obj->obj_lock);

fail:
	synx_util_release_handle(synx_data);
	synx_put_client(client);
	pr_debug("[sess: %u] exit signal with status %d\n",
		session_id.client_id, rc);
	return rc;
}
EXPORT_SYMBOL(synx_signal);

static int synx_match_payload(struct synx_kernel_payload *cb_payload,
	struct synx_kernel_payload *payload)
{
	int rc = 0;

	if (!cb_payload || !payload)
		return -EINVAL;

	if ((cb_payload->cb_func == payload->cb_func) &&
		(cb_payload->data == payload->data)) {
		if (payload->cancel_cb_func) {
			cb_payload->cb_func =
				payload->cancel_cb_func;
			rc = 1;
		} else {
			rc = 2;
			pr_debug("kernel cb de-registration success\n");
		}
	}

	return rc;
}

int synx_register_callback(struct synx_session session_id,
	s32 h_synx,
	synx_callback cb_func,
	void *userdata)
{
	int rc = 0;
	u32 idx;
	u32 status;
	struct synx_client *client;
	struct synx_handle_coredata *synx_data;
	struct synx_coredata *synx_obj;
	struct synx_cb_data *synx_cb;
	struct synx_kernel_payload payload;

	pr_debug("[sess: %u] Enter register cb from pid %d\n",
		session_id.client_id, current->pid);

	client = synx_get_client(session_id);
	if (!client)
		return -EINVAL;

	synx_data = synx_util_acquire_handle(client, h_synx);
	synx_obj = synx_util_obtain_object(synx_data);
	if (!synx_obj) {
		pr_err("%s: [sess: %u] invalid handle access %d\n",
			__func__, client->id, h_synx);
		rc = -EINVAL;
		goto fail;
	}

	mutex_lock(&synx_obj->obj_lock);
	if (synx_util_is_merged_object(synx_obj) ||
		synx_util_is_external_object(synx_obj)) {
		pr_err("cannot register cb with composite synx object\n");
		rc = -EINVAL;
		goto release;
	}

	synx_cb = kzalloc(sizeof(*synx_cb), GFP_ATOMIC);
	if (!synx_cb) {
		rc = -ENOMEM;
		goto release;
	}

	payload.h_synx = h_synx;
	payload.cb_func = cb_func;
	payload.data = userdata;

	/* allocate a free index from client cb table */
	rc = synx_util_alloc_cb_entry(client, &payload, &idx);
	if (rc) {
		pr_err("[sess :%u] error allocating cb entry\n",
			client->id);
		kfree(synx_cb);
		goto release;
	}

	status = synx_util_get_object_status(synx_obj);
	synx_cb->session_id = session_id;
	synx_cb->idx = idx;
	INIT_WORK(&synx_cb->cb_dispatch, synx_util_cb_dispatch);

	/* add callback if object still ACTIVE, dispatch if SIGNALED */
	if (status == SYNX_STATE_ACTIVE) {
		pr_debug("[sess: %u] callback added\n", client->id);
		list_add(&synx_cb->node, &synx_obj->reg_cbs_list);
	} else {
		synx_cb->status = status;
		pr_debug("[sess: %u] callback queued\n", client->id);
		queue_work(synx_dev->work_queue,
			&synx_cb->cb_dispatch);
	}

release:
	mutex_unlock(&synx_obj->obj_lock);
fail:
	synx_util_release_handle(synx_data);
	synx_put_client(client);
	pr_debug("[sess: %u] exit register cb with status %d\n",
		session_id.client_id, rc);
	return rc;
}
EXPORT_SYMBOL(synx_register_callback);

int synx_deregister_callback(struct synx_session session_id,
	s32 h_synx,
	synx_callback cb_func,
	void *userdata,
	synx_callback cancel_cb_func)
{
	int rc = 0, ret = 0;
	u32 status;
	bool match_found = false;
	struct synx_client *client;
	struct synx_handle_coredata *synx_data;
	struct synx_coredata *synx_obj;
	struct synx_kernel_payload payload;
	struct synx_cb_data *synx_cb, *synx_cb_temp;
	struct synx_client_cb *cb_payload;

	pr_debug("[sess: %u] Enter deregister cb from pid %d\n",
		session_id.client_id, current->pid);

	client = synx_get_client(session_id);
	if (!client)
		return -EINVAL;

	synx_data = synx_util_acquire_handle(client, h_synx);
	synx_obj = synx_util_obtain_object(synx_data);
	if (!synx_obj) {
		pr_err("%s: [sess: %u] invalid handle access %d\n",
			__func__, client->id, h_synx);
		rc = -EINVAL;
		goto fail;
	}

	mutex_lock(&synx_obj->obj_lock);
	if (synx_util_is_merged_object(synx_obj) ||
		synx_util_is_external_object(synx_obj)) {
		pr_err("cannot deregister cb with composite synx object\n");
		goto release;
	}

	payload.h_synx = h_synx;
	payload.cb_func = cb_func;
	payload.data = userdata;
	payload.cancel_cb_func = cancel_cb_func;

	status = synx_util_get_object_status(synx_obj);
	if (status != SYNX_STATE_ACTIVE) {
		pr_err("handle %d already signaled. cannot deregister cb/s\n",
			h_synx);
		rc = -EINVAL;
		goto release;
	}

	status = SYNX_CALLBACK_RESULT_CANCELED;
	/* remove all cb payloads mayching the deregister call */
	list_for_each_entry_safe(synx_cb, synx_cb_temp,
		&synx_obj->reg_cbs_list, node) {
		if (synx_cb->session_id.client_id != client->id) {
			continue;
		} else if (synx_cb->idx == 0 ||
			synx_cb->idx >= SYNX_MAX_OBJS) {
			/*
			 * this should not happen. Even if it does,
			 * the allocated memory will be cleaned up
			 * when object is destroyed, preventing any
			 * memory leaks.
			 */
			pr_err("[sess: %u] invalid callback data\n",
				session_id.client_id);
			continue;
		}

		cb_payload = &client->cb_table[synx_cb->idx];
		ret = synx_match_payload(&cb_payload->kernel_cb, &payload);
		switch (ret) {
		case 1:
			/* queue the cancel cb work */
			list_del_init(&synx_cb->node);
			synx_cb->status = status;
			queue_work(synx_dev->work_queue,
				&synx_cb->cb_dispatch);
			match_found = true;
			break;
		case 2:
			/* no cancellation cb */
			if (synx_util_clear_cb_entry(client, cb_payload))
				pr_err("%s: [sess: %u] error clearing cb %d\n",
				__func__, client->id, h_synx);
			list_del_init(&synx_cb->node);
			kfree(synx_cb);
			match_found = true;
			break;
		default:
			break;
		}
	}

	if (!match_found)
		rc = -EINVAL;

release:
	mutex_unlock(&synx_obj->obj_lock);
fail:
	synx_util_release_handle(synx_data);
	synx_put_client(client);

	pr_debug("[sess: %u] exit deregister cb with status %d\n",
		session_id.client_id, rc);
	return rc;
}
EXPORT_SYMBOL(synx_deregister_callback);

int synx_merge(struct synx_session session_id,
	s32 *h_synxs,
	u32 num_objs,
	s32 *h_synx_merged)
{
	int rc;
	long h_synx;
	u32 count = 0;
	struct synx_client *client;
	struct dma_fence **fences = NULL;
	struct synx_coredata *synx_obj;

	pr_debug("[sess: %u] Enter merge from pid %d\n",
		session_id.client_id, current->pid);

	if (!h_synxs || !h_synx_merged) {
		pr_err("%s: [sess: %u] invalid arguments\n",
			__func__, session_id.client_id);
		return -EINVAL;
	}

	client = synx_get_client(session_id);
	if (!client)
		return -EINVAL;

	rc = synx_util_validate_merge(client, h_synxs,
			num_objs, &fences, &count);
	if (rc < 0) {
		pr_err("[sess: %u] merge validation failed\n",
			client->id);
		rc = -EINVAL;
		goto fail;
	}

	synx_obj = kzalloc(sizeof(*synx_obj), GFP_KERNEL);
	if (!synx_obj) {
		rc = -ENOMEM;
		goto fail;
	}

	rc = synx_util_init_group_coredata(synx_obj, fences, count);
	if (rc) {
		pr_err("[sess: %u] error initializing merge synx obj\n",
			client->id);
		goto clean_up;
	}

	rc = synx_util_init_handle(client, synx_obj, &h_synx);
	if (rc) {
		pr_err("[sess: %u] unable to init merge handle %ld\n",
			client->id, h_synx);
		dma_fence_put(synx_obj->fence);
		goto clean_up;
	}

	*h_synx_merged = h_synx;
	synx_put_client(client);
	pr_debug("[sess: %u] exit merge with status %d\n",
		session_id.client_id, rc);
	return 0;

clean_up:
	kfree(synx_obj);
fail:
	synx_util_merge_error(client, h_synxs, count);
	if (num_objs && num_objs <= count)
		kfree(fences);
	synx_put_client(client);
	return rc;
}
EXPORT_SYMBOL(synx_merge);

int synx_release(struct synx_session session_id, s32 h_synx)
{
	int rc = 0;
	u32 idx;
	struct synx_client *client;
	struct synx_handle_coredata *synx_data;

	pr_debug("[sess: %u] Enter release from pid %d\n",
		session_id.client_id, current->pid);

	client = synx_get_client(session_id);
	if (!client)
		return -EINVAL;

	synx_data = NULL;
	idx = synx_util_handle_index(h_synx);

	mutex_lock(&client->synx_table_lock[idx]);
	synx_data = &client->synx_table[idx];
	if (!synx_data->synx_obj) {
		pr_err("[sess: %u] invalid object handle %d\n",
			client->id, h_synx);
	} else if (synx_data->handle != h_synx) {
		pr_err("[sess: %u] stale object handle %d\n",
			client->id, h_synx);
	} else if (synx_data->rel_count == 0) {
		pr_err("[sess: %u] released object handle %d\n",
			client->id, h_synx);
	} else if (!kref_read(&synx_data->internal_refcount)) {
		pr_err("[sess: %u] destroyed object handle %d\n",
			client->id, h_synx);
	} else {
		synx_data->rel_count--;
		/* release the reference obtained at synx creation */
		kref_put(&synx_data->internal_refcount,
			synx_util_destroy_internal_handle);
	}
	mutex_unlock(&client->synx_table_lock[idx]);

	synx_put_client(client);
	pr_debug("[sess: %u] exit release with status %d\n",
		session_id.client_id, rc);

	return rc;
}
EXPORT_SYMBOL(synx_release);

int synx_wait(struct synx_session session_id, s32 h_synx, u64 timeout_ms)
{
	int rc = 0;
	unsigned long timeleft;
	struct synx_client *client;
	struct synx_handle_coredata *synx_data;
	struct synx_coredata *synx_obj;

	pr_debug("[sess: %u] Enter wait from pid %d\n",
		session_id.client_id, current->pid);

	client = synx_get_client(session_id);
	if (!client)
		return -EINVAL;

	synx_data = synx_util_acquire_handle(client, h_synx);
	synx_obj = synx_util_obtain_object(synx_data);
	if (!synx_obj || !synx_obj->fence) {
		pr_err("%s: [sess: %u] invalid handle access %d\n",
			__func__, client->id, h_synx);
		rc = -EINVAL;
		goto fail;
	}

	timeleft = dma_fence_wait_timeout(synx_obj->fence, (bool) 0,
					msecs_to_jiffies(timeout_ms));
	if (timeleft <= 0) {
		pr_err("[sess: %u] wait timeout for handle %d\n",
			client->id, h_synx);
		rc = -ETIMEDOUT;
		goto fail;
	}

	mutex_lock(&synx_obj->obj_lock);
	rc = synx_util_get_object_status(synx_obj);
	mutex_unlock(&synx_obj->obj_lock);
	/* remap the state if signaled successfully */
	if (rc == SYNX_STATE_SIGNALED_SUCCESS)
		rc = 0;

fail:
	synx_util_release_handle(synx_data);
	synx_put_client(client);
	pr_debug("[sess: %u] exit wait with status %d\n",
		session_id.client_id, rc);
	return rc;
}
EXPORT_SYMBOL(synx_wait);

int synx_bind(struct synx_session session_id,
	s32 h_synx,
	struct synx_external_desc external_sync)
{
	int rc = 0;
	u32 i;
	u32 bound_idx;
	struct synx_client *client;
	struct synx_handle_coredata *synx_data;
	struct synx_coredata *synx_obj;
	struct synx_external_data *data = NULL;
	struct bind_operations *bind_ops = NULL;

	pr_debug("[sess: %u] Enter bind from pid %d\n",
		session_id.client_id, current->pid);

	client = synx_get_client(session_id);
	if (!client)
		return -EINVAL;

	synx_data = synx_util_acquire_handle(client, h_synx);
	synx_obj = synx_util_obtain_object(synx_data);
	if (!synx_obj) {
		pr_err("%s: [sess: %u] invalid handle access %d\n",
			__func__, client->id, h_synx);
		rc = -EINVAL;
		goto fail;
	}

	bind_ops = synx_util_get_bind_ops(external_sync.type);
	if (!bind_ops) {
		pr_err("[sess: %u] invalid bind ops for %u\n",
			client->id, external_sync.type);
		rc = -EINVAL;
		goto fail;
	}

	mutex_lock(&synx_obj->obj_lock);
	if (synx_util_is_merged_object(synx_obj) ||
		synx_util_is_external_object(synx_obj)) {
		pr_err("[sess: %u] cannot bind to merged handle %d\n",
			client->id, h_synx);
		rc = -EINVAL;
		goto release;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		rc = -ENOMEM;
		goto release;
	}

	if (synx_util_get_object_status(synx_obj) != SYNX_STATE_ACTIVE) {
		pr_err("[sess: %u] bind prohibited to inactive handle %d\n",
			client->id, h_synx);
		rc = -EINVAL;
		goto free;
	}

	if (synx_obj->num_bound_synxs >= SYNX_MAX_NUM_BINDINGS) {
		pr_err("[sess: %u] max bindings reached for handle %d\n",
			client->id, h_synx);
		rc = -ENOMEM;
		goto free;
	}

	/* don't bind external sync obj is already done */
	for (i = 0; i < synx_obj->num_bound_synxs; i++) {
		if (external_sync.id[0] ==
			synx_obj->bound_synxs[i].external_desc.id[0]) {
			pr_err("[sess: %u] duplicate binding for external sync %d\n",
				client->id, external_sync.id[0]);
			rc = -EALREADY;
			goto free;
		}
	}

	/* data passed to external callback */
	data->h_synx = h_synx;
	data->session_id = session_id;

	bound_idx = synx_obj->num_bound_synxs;
	memcpy(&synx_obj->bound_synxs[bound_idx],
		   &external_sync, sizeof(struct synx_external_desc));
	synx_obj->bound_synxs[bound_idx].external_data = data;
	synx_obj->num_bound_synxs++;
	mutex_unlock(&synx_obj->obj_lock);

	rc = bind_ops->register_callback(synx_external_callback,
			data, external_sync.id[0]);
	if (rc) {
		pr_err("[sess: %u] callback registration failed for %d\n",
			client->id, external_sync.id[0]);
		mutex_lock(&synx_obj->obj_lock);
		memset(&synx_obj->bound_synxs[bound_idx], 0,
			sizeof(struct synx_external_desc));
		synx_obj->num_bound_synxs--;
		goto free;
	}

	synx_util_release_handle(synx_data);
	synx_put_client(client);
	pr_debug("[sess: %u] bind of handle %d with id %d successful\n",
		session_id.client_id, h_synx, external_sync.id[0]);
	return 0;

free:
	kfree(data);
release:
	mutex_unlock(&synx_obj->obj_lock);
fail:
	synx_util_release_handle(synx_data);
	synx_put_client(client);
	return rc;
}
EXPORT_SYMBOL(synx_bind);

int synx_get_status(struct synx_session session_id, s32 h_synx)
{
	int rc = 0;
	struct synx_client *client;
	struct synx_handle_coredata *synx_data;
	struct synx_coredata *synx_obj;

	pr_debug("[sess: %u] Enter get_status from pid %d\n",
		session_id.client_id, current->pid);

	client = synx_get_client(session_id);
	if (!client)
		return -EINVAL;

	synx_data = synx_util_acquire_handle(client, h_synx);
	synx_obj = synx_util_obtain_object(synx_data);
	if (!synx_obj || !synx_obj->fence) {
		pr_err("%s: [sess: %u] invalid handle access %d\n",
			__func__, client->id, h_synx);
		rc = SYNX_STATE_INVALID;
		goto fail;
	}

	mutex_lock(&synx_obj->obj_lock);
	rc = synx_util_get_object_status(synx_obj);
	mutex_unlock(&synx_obj->obj_lock);
	pr_debug("[sess: %u] synx object handle %d status %d\n",
		client->id, h_synx, rc);

fail:
	synx_util_release_handle(synx_data);
	synx_put_client(client);
	pr_debug("[sess: %u] exit get_status with status %d\n",
		session_id.client_id, rc);
	return rc;
}
EXPORT_SYMBOL(synx_get_status);

int synx_addrefcount(struct synx_session session_id, s32 h_synx, s32 count)
{
	int rc = 0;
	u32 idx;
	struct synx_client *client;
	struct synx_handle_coredata *synx_data;
	struct synx_coredata *synx_obj;

	pr_debug("[sess: %u] Enter addrefcount from pid %d\n",
		session_id.client_id, current->pid);

	client = synx_get_client(session_id);
	if (!client)
		return -EINVAL;

	synx_data = synx_util_acquire_handle(client, h_synx);
	synx_obj = synx_util_obtain_object(synx_data);
	if (!synx_obj || !synx_obj->fence) {
		pr_err("%s: [sess: %u] invalid handle access %d\n",
			__func__, client->id, h_synx);
		rc = -EINVAL;
		goto fail;
	}

	if ((count < 0) || (count > SYNX_MAX_REF_COUNTS)) {
		pr_err("[sess: %u] invalid addrefcount for handle %d\n",
			client->id, h_synx);
		rc = -EINVAL;
		goto fail;
	}

	idx = synx_util_handle_index(h_synx);
	mutex_lock(&client->synx_table_lock[idx]);
	/* acquire additional references to handle */
	while (count--) {
		synx_data->rel_count++;
		kref_get(&synx_data->internal_refcount);
	}
	mutex_unlock(&client->synx_table_lock[idx]);

fail:
	synx_util_release_handle(synx_data);
	synx_put_client(client);
	pr_debug("[sess: %u] exit addrefcount with status %d\n",
		session_id.client_id, rc);
	return rc;
}
EXPORT_SYMBOL(synx_addrefcount);

int synx_import(struct synx_session session_id,
	struct synx_import_params *params)
{
	int rc = 0;
	long h_synx;
	struct synx_client *client;
	struct synx_coredata *synx_obj;

	pr_debug("[sess: %u] Enter import from pid %d\n",
		session_id.client_id, current->pid);

	if (!params || !params->new_h_synx) {
		pr_err("[sess: %u] invalid import arguments\n",
			session_id.client_id);
		return -EINVAL;
	}

	client = synx_get_client(session_id);
	if (!client)
		return -EINVAL;

	synx_obj = synx_util_import_object(params);
	if (!synx_obj) {
		pr_err("[sess: %u] invalid import handle %d with key %u\n",
			client->id, params->h_synx, params->secure_key);
		rc = -EINVAL;
		goto fail;
	}

	/*
	 * just need to init handle for importing client, the reference
	 * in synx object has already been accounted for during export
	 */
	rc = synx_util_init_handle(client, synx_obj, &h_synx);
	if (rc) {
		pr_err("[sess: %u] unable to init handle\n",
			client->id);
		goto clean_up;
	}

	*params->new_h_synx = h_synx;
	pr_debug("[sess: %u] new import obj with handle %ld, fence %pK\n",
		client->id, h_synx, synx_obj);
	synx_put_client(client);

	return 0;

clean_up:
	synx_util_put_object(synx_obj);
fail:
	synx_put_client(client);
	return rc;
}
EXPORT_SYMBOL(synx_import);

int synx_export(struct synx_session session_id,
	struct synx_export_params *params)
{
	int rc = 0;
	struct synx_client *client;

	if (!params || !params->secure_key) {
		pr_err("[sess: %u] invalid export arguments\n",
			session_id.client_id);
		return -EINVAL;
	}

	client = synx_get_client(session_id);
	if (!client)
		return -EINVAL;

	rc = synx_util_export_object(client, params);
	if (rc)
		pr_err("[sess: %u] handle export failed %d\n",
			client->id, params->h_synx);

	synx_put_client(client);
	pr_debug("[sess: %u] exit export with status %d\n",
		session_id.client_id, rc);
	return rc;
}
EXPORT_SYMBOL(synx_export);

static int synx_handle_create(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session session_id)
{
	int result;
	struct synx_info synx_create_info;
	struct synx_create_params params;

	if (k_ioctl->size != sizeof(synx_create_info))
		return -EINVAL;

	if (copy_from_user(&synx_create_info,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	params.h_synx = &synx_create_info.synx_obj;
	params.name = synx_create_info.name;
	params.type = 0;
	result = synx_create(session_id, &params);

	if (!result)
		if (copy_to_user(u64_to_user_ptr(k_ioctl->ioctl_ptr),
			&synx_create_info,
			k_ioctl->size))
			return -EFAULT;

	return result;
}

static int synx_handle_getstatus(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session session_id)
{
	struct synx_signal synx_status;

	if (k_ioctl->size != sizeof(synx_status))
		return -EINVAL;

	if (copy_from_user(&synx_status,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	synx_status.synx_state =
		synx_get_status(session_id, synx_status.synx_obj);

	if (copy_to_user(u64_to_user_ptr(k_ioctl->ioctl_ptr),
		&synx_status,
		k_ioctl->size))
		return -EFAULT;

	return 0;
}

static int synx_handle_import(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session session_id)
{
	struct synx_id_info id_info;
	struct synx_import_params params;

	if (k_ioctl->size != sizeof(id_info))
		return -EINVAL;

	if (copy_from_user(&id_info,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	params.h_synx = id_info.synx_obj;
	params.secure_key = id_info.secure_key;
	params.new_h_synx = &id_info.new_synx_obj;
	if (synx_import(session_id, &params))
		return -EINVAL;

	if (copy_to_user(u64_to_user_ptr(k_ioctl->ioctl_ptr),
		&id_info,
		k_ioctl->size))
		return -EFAULT;

	return 0;
}

static int synx_handle_export(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session session_id)
{
	struct synx_id_info id_info;
	struct synx_export_params params;

	if (k_ioctl->size != sizeof(id_info))
		return -EINVAL;

	if (copy_from_user(&id_info,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	params.h_synx = id_info.synx_obj;
	params.secure_key = &id_info.secure_key;
	params.fence = NULL;
	if (synx_export(session_id, &params))
		return -EINVAL;

	if (copy_to_user(u64_to_user_ptr(k_ioctl->ioctl_ptr),
		&id_info,
		k_ioctl->size))
		return -EFAULT;

	return 0;
}

static int synx_handle_signal(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session session_id)
{
	struct synx_signal synx_signal_info;

	if (k_ioctl->size != sizeof(synx_signal_info))
		return -EINVAL;

	if (copy_from_user(&synx_signal_info,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	return synx_signal(session_id, synx_signal_info.synx_obj,
		synx_signal_info.synx_state);
}

static int synx_handle_merge(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session session_id)
{
	s32 *h_synxs;
	u32 num_objs;
	u32 size;
	int result;
	struct synx_merge synx_merge_info;

	if (k_ioctl->size != sizeof(synx_merge_info))
		return -EINVAL;

	if (copy_from_user(&synx_merge_info,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	if (synx_merge_info.num_objs >= SYNX_MAX_OBJS)
		return -EINVAL;

	num_objs = synx_merge_info.num_objs;
	size = sizeof(u32) * num_objs;
	h_synxs = kcalloc(synx_merge_info.num_objs,
					sizeof(*h_synxs), GFP_KERNEL);
	if (!h_synxs)
		return -ENOMEM;

	if (copy_from_user(h_synxs,
		u64_to_user_ptr(synx_merge_info.synx_objs),
		sizeof(u32) * synx_merge_info.num_objs)) {
		kfree(h_synxs);
		return -EFAULT;
	}

	result = synx_merge(session_id, h_synxs,
		num_objs, &synx_merge_info.merged);

	if (!result)
		if (copy_to_user(u64_to_user_ptr(k_ioctl->ioctl_ptr),
			&synx_merge_info,
			k_ioctl->size)) {
			kfree(h_synxs);
			return -EFAULT;
	}

	kfree(h_synxs);
	return result;
}

static int synx_handle_wait(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session session_id)
{
	struct synx_wait synx_wait_info;

	if (k_ioctl->size != sizeof(synx_wait_info))
		return -EINVAL;

	if (copy_from_user(&synx_wait_info,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	k_ioctl->result = synx_wait(session_id, synx_wait_info.synx_obj,
		synx_wait_info.timeout_ms);

	return 0;
}

static int synx_handle_register_user_payload(
	struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session session_id)
{
	int rc = 0;
	struct synx_userpayload_info user_data;

	if (k_ioctl->size != sizeof(user_data))
		return -EINVAL;

	if (copy_from_user(&user_data,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	pr_debug("user cb registration with payload %x\n",
		user_data.payload[0]);
	rc = synx_register_callback(session_id, user_data.synx_obj,
		synx_util_default_user_callback, (void *)user_data.payload[0]);
	if (rc)
		pr_err("[sess: %u] user cb registration failed for handle %d\n",
			session_id.client_id, user_data.synx_obj);

	return rc;
}

static int synx_handle_deregister_user_payload(
	struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session session_id)
{
	int rc = 0;
	struct synx_userpayload_info user_data;

	if (k_ioctl->size != sizeof(user_data))
		return -EINVAL;

	if (copy_from_user(&user_data,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	rc = synx_deregister_callback(session_id,
			user_data.synx_obj, synx_util_default_user_callback,
			(void *)user_data.payload[0], NULL);
	if (rc)
		pr_err("[sess: %u] callback deregistration failed for handle %d\n",
			session_id.client_id, user_data.synx_obj);

	return rc;
}

static int synx_handle_bind(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session session_id)
{
	struct synx_bind synx_bind_info;

	if (k_ioctl->size != sizeof(synx_bind_info))
		return -EINVAL;

	if (copy_from_user(&synx_bind_info,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	k_ioctl->result = synx_bind(session_id, synx_bind_info.synx_obj,
		synx_bind_info.ext_sync_desc);

	return k_ioctl->result;
}

static int synx_handle_addrefcount(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session session_id)
{
	struct synx_addrefcount addrefcount_info;

	if (k_ioctl->size != sizeof(addrefcount_info))
		return -EINVAL;

	if (copy_from_user(&addrefcount_info,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	k_ioctl->result = synx_addrefcount(session_id,
		addrefcount_info.synx_obj,
		addrefcount_info.count);

	return k_ioctl->result;
}

static int synx_handle_release(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session session_id)
{
	struct synx_info info;

	if (k_ioctl->size != sizeof(info))
		return -EINVAL;

	if (copy_from_user(&info,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	return synx_release(session_id, info.synx_obj);
}

static long synx_ioctl(struct file *filep,
	unsigned int cmd,
	unsigned long arg)
{
	s32 rc = 0;
	struct synx_private_ioctl_arg k_ioctl;
	struct synx_session *session = filep->private_data;

	if (cmd != SYNX_PRIVATE_IOCTL_CMD) {
		pr_err("invalid ioctl cmd\n");
		return -ENOIOCTLCMD;
	}

	if (copy_from_user(&k_ioctl,
		(struct synx_private_ioctl_arg *)arg,
		sizeof(k_ioctl))) {
		pr_err("invalid ioctl args\n");
		return -EFAULT;
	}

	if (!k_ioctl.ioctl_ptr)
		return -EINVAL;

	pr_debug("[sess: %u] Enter cmd %u from pid %d\n",
		session->client_id, k_ioctl.id, current->pid);

	switch (k_ioctl.id) {
	case SYNX_CREATE:
		rc = synx_handle_create(&k_ioctl, *session);
		break;
	case SYNX_RELEASE:
		rc = synx_handle_release(&k_ioctl, *session);
		break;
	case SYNX_REGISTER_PAYLOAD:
		rc = synx_handle_register_user_payload(&k_ioctl,
				*session);
		break;
	case SYNX_DEREGISTER_PAYLOAD:
		rc = synx_handle_deregister_user_payload(&k_ioctl,
				*session);
		break;
	case SYNX_SIGNAL:
		rc = synx_handle_signal(&k_ioctl, *session);
		break;
	case SYNX_MERGE:
		rc = synx_handle_merge(&k_ioctl, *session);
		break;
	case SYNX_WAIT:
		rc = synx_handle_wait(&k_ioctl, *session);
		if (copy_to_user((void *)arg,
			&k_ioctl,
			sizeof(k_ioctl))) {
			pr_err("invalid ioctl args\n");
			rc = -EFAULT;
		}
		break;
	case SYNX_BIND:
		rc = synx_handle_bind(&k_ioctl, *session);
		break;
	case SYNX_ADDREFCOUNT:
		rc = synx_handle_addrefcount(&k_ioctl, *session);
		break;
	case SYNX_GETSTATUS:
		rc = synx_handle_getstatus(&k_ioctl, *session);
		break;
	case SYNX_IMPORT:
		rc = synx_handle_import(&k_ioctl, *session);
		break;
	case SYNX_EXPORT:
		rc = synx_handle_export(&k_ioctl, *session);
		break;
	default:
		rc = -EINVAL;
	}

	pr_debug("[sess: %u] exit with status %d\n",
		session->client_id, rc);

	return rc;
}

static ssize_t synx_read(struct file *filep,
	char __user *buf, size_t size, loff_t *f_pos)
{
	ssize_t rc = 0;
	struct synx_client *client = NULL;
	struct synx_client_cb *cb;
	struct synx_session *session = filep->private_data;
	struct synx_userpayload_info data;

	pr_debug("[sess: %u] Enter from pid %d\n",
		session->client_id, current->pid);

	if (size != sizeof(struct synx_userpayload_info)) {
		pr_err("invalid read size\n");
		return -EINVAL;
	}

	client = synx_get_client(*session);
	if (!client)
		return -EINVAL;

	mutex_lock(&client->event_q_lock);
	cb = list_first_entry_or_null(&client->event_q,
			struct synx_client_cb, node);
	if (!cb) {
		mutex_unlock(&client->event_q_lock);
		rc = 0;
		goto fail;
	}

	if (cb->idx == 0 || cb->idx >= SYNX_MAX_OBJS) {
		pr_err("%s invalid index\n", __func__);
		mutex_unlock(&client->event_q_lock);
		rc = -EINVAL;
		goto fail;
	}

	list_del_init(&cb->node);
	mutex_unlock(&client->event_q_lock);

	rc = size;
	data.synx_obj = cb->kernel_cb.h_synx;
	data.reserved = cb->kernel_cb.status;
	data.payload[0] = (u64)cb->kernel_cb.data;
	if (copy_to_user(buf,
			&data,
			sizeof(struct synx_userpayload_info))) {
		pr_err("couldn't copy user callback data\n");
		rc = -EFAULT;
	}

	if (synx_util_clear_cb_entry(client, cb))
		pr_err("%s: [sess: %u] error clearing cb for handle %d\n",
			__func__, client->id, data.synx_obj);
fail:
	synx_put_client(client);
	pr_debug("[sess: %u] exit with status %d\n",
		session->client_id, rc);

	return rc;
}

static unsigned int synx_poll(struct file *filep,
	struct poll_table_struct *poll_table)
{
	int rc = 0;
	struct synx_client *client;
	struct synx_session *session = filep->private_data;

	pr_debug("[sess: %u] Enter from pid %d\n",
		session->client_id, current->pid);

	client = synx_get_client(*session);
	if (!client) {
		pr_err("invalid session in poll\n");
		return 0;
	}

	poll_wait(filep, &client->event_wq, poll_table);
	mutex_lock(&client->event_q_lock);
	if (!list_empty(&client->event_q))
		rc = POLLPRI;
	mutex_unlock(&client->event_q_lock);

	pr_debug("[sess: %u] exit with status %d\n",
		session->client_id, rc);
	synx_put_client(client);

	return rc;
}

int synx_initialize(struct synx_session *session_id,
	struct synx_initialization_params *params)
{
	u32 i;
	u16 unique_id;
	long idx;
	struct synx_client *client;
	struct synx_client_metadata *client_metadata;

	if (!session_id || !params)
		return -EINVAL;

	idx = synx_util_get_free_handle(synx_dev->bitmap, SYNX_MAX_CLIENTS);
	if (idx >= SYNX_MAX_CLIENTS) {
		pr_err("maximum client limit reached\n");
		return -ENOMEM;
	}

	client = vzalloc(sizeof(*client));
	if (!client) {
		clear_bit(idx, synx_dev->bitmap);
		return -ENOMEM;
	}

	if (params->name)
		strlcpy(client->name, params->name, sizeof(client->name));

	do {
		get_random_bytes(&unique_id, sizeof(unique_id));
	} while (!unique_id);

	client->device = synx_dev;
	client->id = unique_id;
	client->id <<= SYNX_CLIENT_HANDLE_SHIFT;
	client->id |= (idx & SYNX_CLIENT_HANDLE_MASK);

	mutex_init(&client->event_q_lock);
	for (i = 0; i < SYNX_MAX_OBJS; i++)
		mutex_init(&client->synx_table_lock[i]);
	INIT_LIST_HEAD(&client->event_q);
	init_waitqueue_head(&client->event_wq);
	/* zero handle not allowed */
	set_bit(0, client->bitmap);
	set_bit(0, client->cb_bitmap);

	mutex_lock(&synx_dev->dev_table_lock);
	client_metadata = &synx_dev->client_table[idx];
	client_metadata->client = client;
	kref_init(&client_metadata->refcount);
	session_id->client_id = client->id;
	mutex_unlock(&synx_dev->dev_table_lock);

	pr_info("[sess: %u] session created %s\n",
		session_id->client_id, params->name);
	return 0;
}
EXPORT_SYMBOL(synx_initialize);

int synx_uninitialize(struct synx_session session_id)
{
	struct synx_client *client;

	client = synx_get_client(session_id);
	if (!client) {
		pr_err("%s: invalid session id %u\n",
			__func__, session_id.client_id);
		return -EINVAL;
	}

	/* release reference obtained in current function */
	synx_put_client(client);
	/* release reference obtained during initialize */
	synx_put_client(client);
	return 0;
}
EXPORT_SYMBOL(synx_uninitialize);

static int synx_open(struct inode *inode, struct file *filep)
{
	int rc = 0;
	struct synx_session *session;
	char name[SYNX_OBJ_NAME_LEN];
	struct synx_initialization_params params;

	pr_debug("Enter pid: %d\n", current->pid);

	session = kzalloc(sizeof(*session), GFP_KERNEL);
	if (!session)
		return -ENOMEM;

	scnprintf(name, SYNX_OBJ_NAME_LEN, "umd-client-%d", current->pid);
	params.name = name;
	rc = synx_initialize(session, &params);
	if (rc) {
		kfree(session);
	} else {
		filep->private_data = session;
		pr_debug("[sess: %u] allocated new session for pid: %d\n",
			session->client_id, current->pid);
	}

	return rc;
}

static int synx_close(struct inode *inode, struct file *filep)
{
	int rc = 0;
	struct synx_session *session = filep->private_data;

	pr_debug("[sess: %u] Enter pid: %d\n",
		session->client_id, current->pid);

	rc = synx_uninitialize(*session);
	kfree(session);

	pr_debug("exit with status %d\n", rc);

	return rc;
}

static const struct file_operations synx_fops = {
	.owner = THIS_MODULE,
	.open  = synx_open,
	.read  = synx_read,
	.release = synx_close,
	.poll  = synx_poll,
	.unlocked_ioctl = synx_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = synx_ioctl,
#endif
};

int synx_register_ops(const struct synx_register_params *params)
{
	s32 rc = 0;
	struct synx_registered_ops *client_ops;

	if (!params || !params->name ||
		!synx_util_is_valid_bind_type(params->type) ||
		!params->ops.register_callback ||
		!params->ops.deregister_callback ||
		!params->ops.signal) {
		pr_err("invalid register params\n");
		return -EINVAL;
	}

	mutex_lock(&synx_dev->vtbl_lock);
	client_ops = &synx_dev->bind_vtbl[params->type];
	if (!client_ops->valid) {
		client_ops->valid = true;
		memcpy(&client_ops->ops, &params->ops,
			sizeof(client_ops->ops));
		strlcpy(client_ops->name, params->name,
			sizeof(client_ops->name));
		client_ops->type = params->type;
		pr_info("registered bind ops type %u for %s\n",
			params->type, params->name);
	} else {
		pr_info("client already registered for type %u by %s\n",
			client_ops->type, client_ops->name);
		rc = -EALREADY;
	}
	mutex_unlock(&synx_dev->vtbl_lock);

	return rc;
}
EXPORT_SYMBOL(synx_register_ops);

int synx_deregister_ops(const struct synx_register_params *params)
{
	struct synx_registered_ops *client_ops;

	if (!params || !params->name ||
		!synx_util_is_valid_bind_type(params->type)) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&synx_dev->vtbl_lock);
	client_ops = &synx_dev->bind_vtbl[params->type];
	memset(client_ops, 0, sizeof(*client_ops));
	pr_info("deregistered bind ops for %s\n",
		params->name);
	mutex_unlock(&synx_dev->vtbl_lock);

	return 0;
}
EXPORT_SYMBOL(synx_deregister_ops);

static int __init synx_init(void)
{
	int rc;

	pr_info("synx device initialization start\n");

	synx_dev = kzalloc(sizeof(*synx_dev), GFP_KERNEL);
	if (!synx_dev)
		return -ENOMEM;

	rc = alloc_chrdev_region(&synx_dev->dev, 0, 1, SYNX_DEVICE_NAME);
	if (rc < 0) {
		pr_err("region allocation failed\n");
		goto alloc_fail;
	}

	cdev_init(&synx_dev->cdev, &synx_fops);
	synx_dev->cdev.owner = THIS_MODULE;
	rc = cdev_add(&synx_dev->cdev, synx_dev->dev, 1);
	if (rc < 0) {
		pr_err("device registation failed\n");
		goto reg_fail;
	}

	synx_dev->class = class_create(THIS_MODULE, SYNX_DEVICE_NAME);
	device_create(synx_dev->class, NULL, synx_dev->dev,
		NULL, SYNX_DEVICE_NAME);

	synx_dev->work_queue = alloc_workqueue(SYNX_WORKQUEUE_NAME,
		WQ_HIGHPRI | WQ_UNBOUND, SYNX_WORKQUEUE_THREADS);
	if (!synx_dev->work_queue) {
		pr_err("high priority work queue creation failed\n");
		rc = -EINVAL;
		goto fail;
	}

	mutex_init(&synx_dev->dev_table_lock);
	mutex_init(&synx_dev->vtbl_lock);
	synx_dev->dma_context = dma_fence_context_alloc(1);
	/* zero session id not allowed */
	set_bit(0, synx_dev->bitmap);
	synx_dev->debugfs_root = synx_init_debugfs_dir(synx_dev);

	pr_info("synx device initialization success\n");

	return 0;

fail:
	device_destroy(synx_dev->class, synx_dev->dev);
	class_destroy(synx_dev->class);
reg_fail:
	unregister_chrdev_region(synx_dev->dev, 1);
alloc_fail:
	kfree(synx_dev);
	return rc;
}

static void __exit synx_exit(void)
{
	struct error_node *err_node, *err_node_tmp;

	device_destroy(synx_dev->class, synx_dev->dev);
	class_destroy(synx_dev->class);
	cdev_del(&synx_dev->cdev);
	unregister_chrdev_region(synx_dev->dev, 1);
	synx_remove_debugfs_dir(synx_dev);
	/* release uncleared error nodes */
	list_for_each_entry_safe(
		err_node, err_node_tmp,
		&synx_dev->error_list,
		node) {
		list_del(&err_node->node);
		kfree(err_node);
	}
	mutex_destroy(&synx_dev->dev_table_lock);
	mutex_destroy(&synx_dev->error_lock);
	kfree(synx_dev);
}

module_init(synx_init);
module_exit(synx_exit);

MODULE_DESCRIPTION("Global Synx Driver");
MODULE_LICENSE("GPL v2");
