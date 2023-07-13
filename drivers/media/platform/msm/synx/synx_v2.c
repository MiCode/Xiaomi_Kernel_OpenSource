// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/atomic.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/random.h>
#include <linux/remoteproc/qcom_rproc.h>
#include <linux/slab.h>
#include <linux/sync_file.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#include "synx_debugfs_v2.h"
#include "synx_private_v2.h"
#include "synx_util_v2.h"

struct synx_device *synx_dev;
static atomic64_t synx_counter = ATOMIC64_INIT(1);

void synx_external_callback(s32 sync_obj, int status, void *data)
{
	struct synx_signal_cb *signal_cb = data;

	if (IS_ERR_OR_NULL(signal_cb)) {
		dprintk(SYNX_ERR,
			"invalid payload from external obj %d [%d]\n",
			sync_obj, status);
		return;
	}

	signal_cb->status = status;
	signal_cb->ext_sync_id = sync_obj;
	signal_cb->flag = SYNX_SIGNAL_FROM_CALLBACK;

	dprintk(SYNX_DBG,
		"external callback from %d on handle %u\n",
		sync_obj, signal_cb->handle);

	/*
	 * invoke the handler directly as external callback
	 * is invoked from separate task.
	 * avoids creation of separate task again.
	 */
	synx_signal_handler(&signal_cb->cb_dispatch);
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
	dprintk(SYNX_MEM, "released backing fence %pK\n", fence);
}
EXPORT_SYMBOL(synx_fence_release);

static struct dma_fence_ops synx_fence_ops = {
	.wait = dma_fence_default_wait,
	.enable_signaling = synx_fence_enable_signaling,
	.get_driver_name = synx_fence_driver_name,
	.get_timeline_name = synx_fence_driver_name,
	.release = synx_fence_release,
};

static int synx_create_sync_fd(struct dma_fence *fence)
{
	int fd;
	struct sync_file *sync_file;

	if (IS_ERR_OR_NULL(fence))
		return -SYNX_INVALID;

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0)
		return fd;

	sync_file = sync_file_create(fence);
	if (IS_ERR_OR_NULL(sync_file)) {
		dprintk(SYNX_ERR, "error creating sync file\n");
		goto err;
	}

	fd_install(fd, sync_file->file);
	return fd;

err:
	put_unused_fd(fd);
	return -SYNX_INVALID;
}

void *synx_get_fence(struct synx_session *session,
	u32 h_synx)
{
	struct synx_client *client;
	struct synx_handle_coredata *synx_data;
	struct synx_coredata *synx_obj;
	struct dma_fence *fence = NULL;

	client = synx_get_client(session);
	if (IS_ERR_OR_NULL(client))
		return NULL;

	synx_data = synx_util_acquire_handle(client, h_synx);
	synx_obj = synx_util_obtain_object(synx_data);
	if (IS_ERR_OR_NULL(synx_obj) ||
		 IS_ERR_OR_NULL(synx_obj->fence)) {
		dprintk(SYNX_ERR,
			"[sess :%llu] invalid handle access %u\n",
			client->id, h_synx);
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
	return fence;
}
EXPORT_SYMBOL(synx_get_fence);

static int synx_native_check_bind(struct synx_client *client,
	struct synx_create_params *params)
{
	int rc;
	u32 h_synx;
	struct synx_entry_64 *ext_entry;
	struct synx_map_entry *entry;

	if (IS_ERR_OR_NULL(params->fence))
		return -SYNX_INVALID;

	ext_entry = synx_util_retrieve_data(params->fence,
					synx_util_map_params_to_type(params->flags));
	if (IS_ERR_OR_NULL(ext_entry))
		return -SYNX_NOENT;

	h_synx = ext_entry->data[0];
	synx_util_remove_data(params->fence,
		synx_util_map_params_to_type(params->flags));

	entry = synx_util_get_map_entry(h_synx);
	if (IS_ERR_OR_NULL(entry))
		/* possible cleanup, retry to alloc new handle */
		return -SYNX_NOENT;

	rc = synx_util_init_handle(client, entry->synx_obj,
			&h_synx, entry);
	if (rc != SYNX_SUCCESS) {
		dprintk(SYNX_ERR,
			"[sess :%llu] new handle init failed\n",
			client->id);
		goto fail;
	}

	*params->h_synx = h_synx;
	return SYNX_SUCCESS;

fail:
	synx_util_release_map_entry(entry);
	return rc;
}

static int synx_native_create_core(struct synx_client *client,
	struct synx_create_params *params)
{
	int rc;
	struct synx_coredata *synx_obj;
	struct synx_map_entry *map_entry;

	if (IS_ERR_OR_NULL(client) || IS_ERR_OR_NULL(params) ||
		IS_ERR_OR_NULL(params->h_synx))
		return -SYNX_INVALID;

	synx_obj = kzalloc(sizeof(*synx_obj), GFP_KERNEL);
	if (IS_ERR_OR_NULL(synx_obj))
		return -SYNX_NOMEM;

	rc = synx_util_init_coredata(synx_obj, params,
			&synx_fence_ops, client->dma_context);
	if (rc) {
		dprintk(SYNX_ERR,
			"[sess :%llu] handle allocation failed\n",
			client->id);
		kfree(synx_obj);
		goto fail;
	}

	map_entry = synx_util_insert_to_map(synx_obj,
					*params->h_synx, 0);
	if (IS_ERR_OR_NULL(map_entry)) {
		rc = PTR_ERR(map_entry);
		synx_util_put_object(synx_obj);
		goto fail;
	}

	rc = synx_util_add_callback(synx_obj, *params->h_synx);
	if (rc != SYNX_SUCCESS) {
		synx_util_release_map_entry(map_entry);
		goto fail;
	}

	rc = synx_util_init_handle(client, synx_obj,
			params->h_synx, map_entry);
	if (rc < 0) {
		dprintk(SYNX_ERR,
			"[sess :%llu] unable to init new handle\n",
			client->id);
		synx_util_release_map_entry(map_entry);
		goto fail;
	}

	dprintk(SYNX_MEM,
		"[sess :%llu] allocated %u, core %pK, fence %pK\n",
		client->id, *params->h_synx, synx_obj, synx_obj->fence);
	return SYNX_SUCCESS;

fail:
	return rc;
}

int synx_create(struct synx_session *session,
	struct synx_create_params *params)
{
	int rc = -SYNX_NOENT;
	struct synx_client *client;
	struct synx_external_desc_v2 ext_desc = {0};

	if (IS_ERR_OR_NULL(params) || IS_ERR_OR_NULL(params->h_synx) ||
		params->flags > SYNX_CREATE_MAX_FLAGS) {
		dprintk(SYNX_ERR, "invalid create arguments\n");
		return -SYNX_INVALID;
	}

	if (params->flags & SYNX_CREATE_DMA_FENCE) {
		dprintk(SYNX_ERR,
			"handle create with native fence not supported\n");
		return -SYNX_NOSUPPORT;
	}

	client = synx_get_client(session);
	if (IS_ERR_OR_NULL(client))
		return -SYNX_INVALID;

	*params->h_synx = 0;

	do {
		/* create with external fence */
		if (!IS_ERR_OR_NULL(params->fence))
			rc = synx_native_check_bind(client, params);

		if (rc == -SYNX_NOENT) {
			rc = synx_native_create_core(client, params);
			if (rc == SYNX_SUCCESS &&
				 !IS_ERR_OR_NULL(params->fence)) {
				/* save external fence details */
				rc = synx_util_save_data(params->fence,
					synx_util_map_params_to_type(params->flags),
					*params->h_synx);
				if (rc == -SYNX_ALREADY) {
					/*
					 * raced with create on same fence from
					 * another client. clear the allocated
					 * handle and retry.
					 */
					synx_native_release_core(client, *params->h_synx);
					*params->h_synx = 0;
					rc = -SYNX_NOENT;
					continue;
				} else if (rc != SYNX_SUCCESS) {
					dprintk(SYNX_ERR,
						"allocating handle failed=%d", rc);
					synx_native_release_core(client, *params->h_synx);
					break;
				}

				/* bind with external fence */
				ext_desc.id = *((u32 *)params->fence);
				ext_desc.type = synx_util_map_params_to_type(params->flags);
				rc = synx_bind(session, *params->h_synx, ext_desc);
				if (rc != SYNX_SUCCESS) {
					dprintk(SYNX_ERR,
						"[sess :%llu] bind external fence failed\n",
						client->id);
					synx_native_release_core(client, *params->h_synx);
					goto fail;
				}
			}
		}

		if (rc == SYNX_SUCCESS)
			dprintk(SYNX_VERB,
				"[sess :%llu] handle allocated %u\n",
				client->id, *params->h_synx);

		break;
	} while (true);

fail:
	synx_put_client(client);
	return rc;
}
EXPORT_SYMBOL(synx_create);

int synx_native_signal_core(struct synx_coredata *synx_obj,
	u32 status,
	bool cb_signal,
	u64 ext_sync_id)
{
	int rc = 0;
	int ret;
	u32 i = 0;
	u32 idx = 0;
	s32 sync_id;
	u32 type;
	void *data = NULL;
	struct synx_bind_desc bind_descs[SYNX_MAX_NUM_BINDINGS];
	struct bind_operations *bind_ops = NULL;

	if (IS_ERR_OR_NULL(synx_obj))
		return -SYNX_INVALID;

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
				 synx_obj->bound_synxs[i].external_desc.id)) {
				dprintk(SYNX_VERB,
					"skipping signaling inbound sync: %llu\n",
					ext_sync_id);
				type = synx_obj->bound_synxs[i].external_desc.type;
				memset(&synx_obj->bound_synxs[i], 0,
					sizeof(struct synx_bind_desc));
				/* clear the hash table entry */
				synx_util_remove_data(&ext_sync_id, type);
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
		sync_id = bind_descs[i].external_desc.id;
		data = bind_descs[i].external_data;
		type = bind_descs[i].external_desc.type;
		bind_ops = synx_util_get_bind_ops(type);
		if (IS_ERR_OR_NULL(bind_ops)) {
			dprintk(SYNX_ERR,
				"invalid bind ops for type: %u\n", type);
			kfree(data);
			continue;
		}

		/* clear the hash table entry */
		synx_util_remove_data(&sync_id, type);

		/*
		 * we are already signaled, so don't want to
		 * recursively be signaled
		 */
		ret = bind_ops->deregister_callback(
				synx_external_callback, data, sync_id);
		if (ret < 0) {
			dprintk(SYNX_ERR,
				"deregistration fail on %d, type: %u, err=%d\n",
				sync_id, type, ret);
			continue;
		}
		dprintk(SYNX_VERB,
			"signal external sync: %d, type: %u, status: %u\n",
			sync_id, type, status);
		/* optional function to enable external signaling */
		if (bind_ops->enable_signaling) {
			ret = bind_ops->enable_signaling(sync_id);
			if (ret < 0)
				dprintk(SYNX_ERR,
					"enabling fail on %d, type: %u, err=%d\n",
					sync_id, type, ret);
		}
		ret = bind_ops->signal(sync_id, status);
		if (ret < 0)
			dprintk(SYNX_ERR,
				"signaling fail on %d, type: %u, err=%d\n",
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

int synx_native_signal_fence(struct synx_coredata *synx_obj,
	u32 status)
{
	int rc = 0;
	unsigned long flags;

	if (IS_ERR_OR_NULL(synx_obj) || IS_ERR_OR_NULL(synx_obj->fence))
		return -SYNX_INVALID;

	if (status <= SYNX_STATE_ACTIVE) {
		dprintk(SYNX_ERR, "signaling with wrong status: %u\n",
			status);
		return -SYNX_INVALID;
	}

	if (synx_util_is_merged_object(synx_obj)) {
		dprintk(SYNX_ERR, "signaling a composite handle\n");
		return -SYNX_INVALID;
	}

	if (synx_util_get_object_status(synx_obj) !=
		SYNX_STATE_ACTIVE)
		return -SYNX_ALREADY;

	if (IS_ERR_OR_NULL(synx_obj->signal_cb)) {
		dprintk(SYNX_ERR, "signal cb in bad state\n");
		return -SYNX_INVALID;
	}

	/*
	 * remove registered callback for the fence
	 * so it does not invoke the signal through callback again
	 */
	if (!dma_fence_remove_callback(synx_obj->fence,
		&synx_obj->signal_cb->fence_cb)) {
		dprintk(SYNX_ERR, "callback could not be removed\n");
		return -SYNX_INVALID;
	}

	dprintk(SYNX_MEM, "signal cb destroyed %pK\n",
		synx_obj->signal_cb);
	kfree(synx_obj->signal_cb);
	synx_obj->signal_cb = NULL;

	/* releasing reference held by signal cb */
	synx_util_put_object(synx_obj);

	spin_lock_irqsave(synx_obj->fence->lock, flags);
	/* check the status again acquiring lock to avoid errors */
	if (synx_util_get_object_status_locked(synx_obj) !=
		SYNX_STATE_ACTIVE) {
		spin_unlock_irqrestore(synx_obj->fence->lock, flags);
		return -SYNX_ALREADY;
	}

	/* set fence error to model {signal w/ error} */
	if (status != SYNX_STATE_SIGNALED_SUCCESS)
		dma_fence_set_error(synx_obj->fence, -status);

	rc = dma_fence_signal_locked(synx_obj->fence);
	if (rc)
		dprintk(SYNX_ERR,
			"signaling fence %pK failed=%d\n",
			synx_obj->fence, rc);
	spin_unlock_irqrestore(synx_obj->fence->lock, flags);

	return rc;
}

void synx_signal_handler(struct work_struct *cb_dispatch)
{
	int rc = SYNX_SUCCESS;
	u32 idx;
	struct synx_signal_cb *signal_cb =
		container_of(cb_dispatch, struct synx_signal_cb, cb_dispatch);
	struct synx_coredata *synx_obj = signal_cb->synx_obj;

	u32 h_synx = signal_cb->handle;
	u32 status = signal_cb->status;

	if ((signal_cb->flag & SYNX_SIGNAL_FROM_FENCE) &&
			(synx_util_is_global_handle(h_synx) ||
			synx_util_is_global_object(synx_obj))) {
		idx = (IS_ERR_OR_NULL(synx_obj)) ?
				synx_util_global_idx(h_synx) :
				synx_obj->global_idx;
		rc = synx_global_update_status(idx, status);
		if (rc != SYNX_SUCCESS)
			dprintk(SYNX_ERR,
				"global status update of %u failed=%d\n",
				h_synx, rc);
		/*
		 * We are decrementing the reference here assuming this code will be
		 * executed after handle is released. But in case if clients signal
		 * dma fence in middle of execution sequence, then we will put
		 * one reference thus deleting the global idx. As of now clients cannot
		 * signal dma fence.
		 */
		synx_global_put_ref(idx);
	}

	/*
	 * when invoked from external callback, possible for
	 * all local clients to have released the handle coredata.
	 */
	if (IS_ERR_OR_NULL(synx_obj)) {
		dprintk(SYNX_WARN,
			"handle %d has no local clients\n",
			h_synx);
		dprintk(SYNX_MEM, "signal cb destroyed %pK\n",
			signal_cb);
		kfree(signal_cb);
		return;
	}

	if (rc != SYNX_SUCCESS) {
		dprintk(SYNX_ERR,
			"global status update for %u failed=%d\n",
			h_synx, rc);
		goto fail;
	}

	mutex_lock(&synx_obj->obj_lock);

	if (signal_cb->flag & SYNX_SIGNAL_FROM_IPC)
		rc = synx_native_signal_fence(synx_obj, status);

	if (rc == SYNX_SUCCESS)
		rc = synx_native_signal_core(synx_obj, status,
				(signal_cb->flag & SYNX_SIGNAL_FROM_CALLBACK) ?
				true : false, signal_cb->ext_sync_id);

	mutex_unlock(&synx_obj->obj_lock);

	if (rc != SYNX_SUCCESS)
		dprintk(SYNX_ERR,
			"internal signaling %u failed=%d",
			h_synx, rc);

fail:
	/* release reference held by signal cb */
	synx_util_put_object(synx_obj);
	dprintk(SYNX_MEM, "signal cb destroyed %pK\n", signal_cb);
	kfree(signal_cb);
	dprintk(SYNX_VERB, "signal handle %u dispatch complete=%d",
		h_synx, rc);
}

/* function would be called from atomic context */
void synx_fence_callback(struct dma_fence *fence,
	struct dma_fence_cb *cb)
{
	s32 status;
	struct synx_signal_cb *signal_cb =
		container_of(cb, struct synx_signal_cb, fence_cb);

	dprintk(SYNX_DBG,
		"callback from external fence %pK for handle %u\n",
		fence, signal_cb->handle);

	/* other signal_cb members would be set during cb registration */
	status = dma_fence_get_status_locked(fence);

	/*
	 * dma_fence_get_status_locked API returns 1 if signaled,
	 * 0 if ACTIVE,
	 * and negative error code in case of any failure
	 */
	if (status == 1)
		status = SYNX_STATE_SIGNALED_SUCCESS;
	else if (status < 0)
		status = SYNX_STATE_SIGNALED_EXTERNAL;

	signal_cb->status = status;

	INIT_WORK(&signal_cb->cb_dispatch, synx_signal_handler);
	queue_work(synx_dev->wq_cb, &signal_cb->cb_dispatch);
}
EXPORT_SYMBOL(synx_fence_callback);

static int synx_signal_offload_job(
	struct synx_client *client,
	struct synx_coredata *synx_obj,
	u32 h_synx, u32 status)
{
	int rc = SYNX_SUCCESS;
	struct synx_signal_cb *signal_cb;

	signal_cb = kzalloc(sizeof(*signal_cb), GFP_ATOMIC);
	if (IS_ERR_OR_NULL(signal_cb)) {
		rc = -SYNX_NOMEM;
		goto fail;
	}

	/*
	 * since the signal will be queued to separate thread,
	 * to ensure the synx coredata pointer remain valid, get
	 * additional reference, thus avoiding any potential
	 * use-after-free.
	 */
	synx_util_get_object(synx_obj);

	signal_cb->handle = h_synx;
	signal_cb->status = status;
	signal_cb->synx_obj = synx_obj;
	signal_cb->flag = SYNX_SIGNAL_FROM_CLIENT;

	dprintk(SYNX_VERB,
		"[sess :%llu] signal work queued for %u\n",
		client->id, h_synx);

	INIT_WORK(&signal_cb->cb_dispatch, synx_signal_handler);
	queue_work(synx_dev->wq_cb, &signal_cb->cb_dispatch);

fail:
	return rc;
}

int synx_signal(struct synx_session *session, u32 h_synx, u32 status)
{
	int rc = SYNX_SUCCESS;
	struct synx_client *client;
	struct synx_handle_coredata *synx_data = NULL;
	struct synx_coredata *synx_obj;

	client = synx_get_client(session);
	if (IS_ERR_OR_NULL(client))
		return -SYNX_INVALID;

	if (status <= SYNX_STATE_ACTIVE) {
		dprintk(SYNX_ERR,
			"[sess :%llu] signaling with wrong status: %u\n",
			client->id, status);
		rc = -SYNX_INVALID;
		goto fail;
	}

	synx_data = synx_util_acquire_handle(client, h_synx);
	synx_obj = synx_util_obtain_object(synx_data);
	if (IS_ERR_OR_NULL(synx_obj) ||
			IS_ERR_OR_NULL(synx_obj->fence)) {
		dprintk(SYNX_ERR,
			"[sess :%llu] invalid handle access %u\n",
			client->id, h_synx);
		rc = -SYNX_INVALID;
		goto fail;
	}

	mutex_lock(&synx_obj->obj_lock);
	if (synx_util_is_global_handle(h_synx) ||
			synx_util_is_global_object(synx_obj))
		rc = synx_global_update_status(
				synx_obj->global_idx, status);

	if (rc != SYNX_SUCCESS) {
		mutex_unlock(&synx_obj->obj_lock);
		dprintk(SYNX_ERR,
			"[sess :%llu] status update %d failed=%d\n",
			client->id, h_synx, rc);
		goto fail;
	}

	/*
	 * offload callback dispatch and external fence
	 * notification to separate worker thread, if any.
	 */
	if (synx_obj->num_bound_synxs ||
			!list_empty(&synx_obj->reg_cbs_list))
		rc = synx_signal_offload_job(client, synx_obj,
				h_synx, status);

	rc = synx_native_signal_fence(synx_obj, status);
	if (rc != SYNX_SUCCESS)
		dprintk(SYNX_ERR,
			"[sess :%llu] signaling %u failed=%d\n",
			client->id, h_synx, rc);
	mutex_unlock(&synx_obj->obj_lock);

fail:
	synx_util_release_handle(synx_data);
	synx_put_client(client);
	return rc;
}
EXPORT_SYMBOL(synx_signal);

static int synx_match_payload(struct synx_kernel_payload *cb_payload,
	struct synx_kernel_payload *payload)
{
	int rc = 0;

	if (IS_ERR_OR_NULL(cb_payload) || IS_ERR_OR_NULL(payload))
		return -SYNX_INVALID;

	if ((cb_payload->cb_func == payload->cb_func) &&
			(cb_payload->data == payload->data)) {
		if (payload->cancel_cb_func) {
			cb_payload->cb_func =
				payload->cancel_cb_func;
			rc = 1;
		} else {
			rc = 2;
			dprintk(SYNX_VERB,
				"kernel cb de-registration success\n");
		}
	}

	return rc;
}

int synx_async_wait(struct synx_session *session,
	struct synx_callback_params *params)
{
	int rc = 0;
	u32 idx;
	u32 status;
	struct synx_client *client;
	struct synx_handle_coredata *synx_data;
	struct synx_coredata *synx_obj;
	struct synx_cb_data *synx_cb;
	struct synx_kernel_payload payload;

	if (IS_ERR_OR_NULL(session) || IS_ERR_OR_NULL(params))
		return -SYNX_INVALID;

	if (params->timeout_ms != SYNX_NO_TIMEOUT)
		return -SYNX_NOSUPPORT;

	client = synx_get_client(session);
	if (IS_ERR_OR_NULL(client))
		return -SYNX_INVALID;

	synx_data = synx_util_acquire_handle(client, params->h_synx);
	synx_obj = synx_util_obtain_object(synx_data);
	if (IS_ERR_OR_NULL(synx_obj)) {
		dprintk(SYNX_ERR,
			"[sess :%llu] invalid handle access %u\n",
			client->id, params->h_synx);
		rc = -SYNX_INVALID;
		goto fail;
	}

	mutex_lock(&synx_obj->obj_lock);
	if (synx_util_is_merged_object(synx_obj)) {
		dprintk(SYNX_ERR,
			"[sess :%llu] cannot async wait on merged handle %u\n",
			client->id, params->h_synx);
		rc = -SYNX_INVALID;
		goto release;
	}

	synx_cb = kzalloc(sizeof(*synx_cb), GFP_ATOMIC);
	if (IS_ERR_OR_NULL(synx_cb)) {
		rc = -SYNX_NOMEM;
		goto release;
	}

	payload.h_synx = params->h_synx;
	payload.cb_func = params->cb_func;
	payload.data = params->userdata;

	/* allocate a free index from client cb table */
	rc = synx_util_alloc_cb_entry(client, &payload, &idx);
	if (rc) {
		dprintk(SYNX_ERR,
			"[sess :%llu] error allocating cb entry\n",
			client->id);
		kfree(synx_cb);
		goto release;
	}

	if (synx_util_is_global_handle(params->h_synx) ||
			synx_util_is_global_object(synx_obj))
		status = synx_global_test_status_set_wait(
					synx_util_global_idx(params->h_synx),
					SYNX_CORE_APSS);
	else
		status = synx_util_get_object_status(synx_obj);

	synx_cb->session = session;
	synx_cb->idx = idx;
	INIT_WORK(&synx_cb->cb_dispatch, synx_util_cb_dispatch);

	/* add callback if object still ACTIVE, dispatch if SIGNALED */
	if (status == SYNX_STATE_ACTIVE) {
		dprintk(SYNX_VERB,
			"[sess :%llu] callback added for handle %u\n",
			client->id, params->h_synx);
		list_add(&synx_cb->node, &synx_obj->reg_cbs_list);
	} else {
		synx_cb->status = status;
		dprintk(SYNX_VERB,
			"[sess :%llu] callback queued for handle %u\n",
			client->id, params->h_synx);
		queue_work(synx_dev->wq_cb,
			&synx_cb->cb_dispatch);
	}

release:
	mutex_unlock(&synx_obj->obj_lock);
fail:
	synx_util_release_handle(synx_data);
	synx_put_client(client);
	return rc;
}
EXPORT_SYMBOL(synx_async_wait);

int synx_cancel_async_wait(
	struct synx_session *session,
	struct synx_callback_params *params)
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

	if (IS_ERR_OR_NULL(session) || IS_ERR_OR_NULL(params))
		return -SYNX_INVALID;

	client = synx_get_client(session);
	if (IS_ERR_OR_NULL(client))
		return -SYNX_INVALID;

	synx_data = synx_util_acquire_handle(client, params->h_synx);
	synx_obj = synx_util_obtain_object(synx_data);
	if (IS_ERR_OR_NULL(synx_obj)) {
		dprintk(SYNX_ERR,
			"[sess :%llu] invalid handle access %u\n",
			client->id, params->h_synx);
		rc = -SYNX_INVALID;
		goto fail;
	}

	mutex_lock(&synx_obj->obj_lock);
	if (synx_util_is_merged_object(synx_obj) ||
		synx_util_is_external_object(synx_obj)) {
		dprintk(SYNX_ERR,
			"cannot cancel wait on composite handle\n");
		goto release;
	}

	payload.h_synx = params->h_synx;
	payload.cb_func = params->cb_func;
	payload.data = params->userdata;
	payload.cancel_cb_func = params->cancel_cb_func;

	status = synx_util_get_object_status(synx_obj);
	if (status != SYNX_STATE_ACTIVE) {
		dprintk(SYNX_ERR,
			"handle %u already signaled cannot cancel\n",
			params->h_synx);
		rc = -SYNX_INVALID;
		goto release;
	}

	status = SYNX_CALLBACK_RESULT_CANCELED;
	/* remove all cb payloads mayching the deregister call */
	list_for_each_entry_safe(synx_cb, synx_cb_temp,
			&synx_obj->reg_cbs_list, node) {
		if (synx_cb->session != session) {
			continue;
		} else if (synx_cb->idx == 0 ||
			synx_cb->idx >= SYNX_MAX_OBJS) {
			/*
			 * this should not happen. Even if it does,
			 * the allocated memory will be cleaned up
			 * when object is destroyed, preventing any
			 * memory leaks.
			 */
			dprintk(SYNX_ERR,
				"[sess :%llu] invalid callback data\n",
				client->id);
			continue;
		}

		cb_payload = &client->cb_table[synx_cb->idx];
		ret = synx_match_payload(&cb_payload->kernel_cb, &payload);
		switch (ret) {
		case 1:
			/* queue the cancel cb work */
			list_del_init(&synx_cb->node);
			synx_cb->status = status;
			queue_work(synx_dev->wq_cb,
				&synx_cb->cb_dispatch);
			match_found = true;
			break;
		case 2:
			/* no cancellation cb */
			if (synx_util_clear_cb_entry(client, cb_payload))
				dprintk(SYNX_ERR,
				"[sess :%llu] error clearing cb %u\n",
				client->id, params->h_synx);
			list_del_init(&synx_cb->node);
			kfree(synx_cb);
			match_found = true;
			break;
		default:
			break;
		}
	}

	if (!match_found)
		rc = -SYNX_INVALID;

release:
	mutex_unlock(&synx_obj->obj_lock);
fail:
	synx_util_release_handle(synx_data);
	synx_put_client(client);
	return rc;
}
EXPORT_SYMBOL(synx_cancel_async_wait);

int synx_merge(struct synx_session *session,
	struct synx_merge_params *params)
{
	int rc, i, j = 0;
	u32 h_child;
	u32 count = 0;
	u32 *h_child_list;
	struct synx_client *client;
	struct dma_fence **fences = NULL;
	struct synx_coredata *synx_obj;
	struct synx_map_entry *map_entry;

	if (IS_ERR_OR_NULL(session) || IS_ERR_OR_NULL(params))
		return -SYNX_INVALID;

	if (IS_ERR_OR_NULL(params->h_synxs) ||
		IS_ERR_OR_NULL(params->h_merged_obj)) {
		dprintk(SYNX_ERR, "invalid arguments\n");
		return -SYNX_INVALID;
	}

	client = synx_get_client(session);
	if (IS_ERR_OR_NULL(client))
		return -SYNX_INVALID;

	rc = synx_util_validate_merge(client, params->h_synxs,
			params->num_objs, &fences, &count);
	if (rc < 0) {
		dprintk(SYNX_ERR,
			"[sess :%llu] merge validation failed\n",
			client->id);
		rc = -SYNX_INVALID;
		goto fail;
	}

	synx_obj = kzalloc(sizeof(*synx_obj), GFP_KERNEL);
	if (IS_ERR_OR_NULL(synx_obj)) {
		rc = -SYNX_NOMEM;
		goto fail;
	}

	rc = synx_util_init_group_coredata(synx_obj, fences,
			params, count, client->dma_context);
	if (rc) {
		dprintk(SYNX_ERR,
		"[sess :%llu] error initializing merge handle\n",
			client->id);
		goto clean_up;
	}

	map_entry = synx_util_insert_to_map(synx_obj,
					*params->h_merged_obj, 0);
	if (IS_ERR_OR_NULL(map_entry)) {
		rc = PTR_ERR(map_entry);
		goto clean_up;
	}

	rc = synx_util_init_handle(client, synx_obj,
			params->h_merged_obj, map_entry);
	if (rc) {
		dprintk(SYNX_ERR,
			"[sess :%llu] unable to init merge handle %u\n",
			client->id, *params->h_merged_obj);
		dma_fence_put(synx_obj->fence);
		goto clear;
	}

	if (params->flags & SYNX_MERGE_GLOBAL_FENCE) {
		h_child_list = kzalloc(count*4, GFP_KERNEL);
		if (IS_ERR_OR_NULL(synx_obj)) {
			rc = -SYNX_NOMEM;
			goto clear;
		}

		for (i = 0; i < count; i++) {
			h_child = synx_util_get_fence_entry((u64)fences[i], 1);
			if (!synx_util_is_global_handle(h_child))
				continue;

			h_child_list[j++] = synx_util_global_idx(h_child);
		}

		rc = synx_global_merge(h_child_list, j,
			synx_util_global_idx(*params->h_merged_obj));
		if (rc != SYNX_SUCCESS) {
			dprintk(SYNX_ERR, "global merge failed\n");
			goto clear;
		}
	}

	dprintk(SYNX_MEM,
		"[sess :%llu] merge allocated %u, core %pK, fence %pK\n",
		client->id, *params->h_merged_obj, synx_obj,
		synx_obj->fence);
	synx_put_client(client);
	return SYNX_SUCCESS;

clear:
	synx_util_release_map_entry(map_entry);
clean_up:
	kfree(synx_obj);
fail:
	synx_util_merge_error(client, params->h_synxs, count);
	if (params->num_objs && params->num_objs <= count)
		kfree(fences);
	synx_put_client(client);
	return rc;
}
EXPORT_SYMBOL(synx_merge);

int synx_native_release_core(struct synx_client *client,
	u32 h_synx)
{
	int rc = -SYNX_INVALID;
	struct synx_handle_coredata *curr, *synx_handle = NULL;

	spin_lock_bh(&client->handle_map_lock);
	hash_for_each_possible(client->handle_map,
			curr, node, h_synx) {
		if (curr->key == h_synx &&
			curr->rel_count != 0) {
			curr->rel_count--;
			synx_handle = curr;
			rc = SYNX_SUCCESS;
			break;
		}
	}
	spin_unlock_bh(&client->handle_map_lock);

	/* release the reference obtained at synx creation */
	synx_util_release_handle(synx_handle);

	return rc;
}

int synx_release(struct synx_session *session, u32 h_synx)
{
	int rc = 0;
	struct synx_client *client;

	client = synx_get_client(session);
	if (IS_ERR_OR_NULL(client))
		return -SYNX_INVALID;

	rc = synx_native_release_core(client, h_synx);

	synx_put_client(client);
	return rc;
}
EXPORT_SYMBOL(synx_release);

int synx_wait(struct synx_session *session,
	u32 h_synx, u64 timeout_ms)
{
	int rc = 0;
	unsigned long timeleft;
	struct synx_client *client;
	struct synx_handle_coredata *synx_data;
	struct synx_coredata *synx_obj;

	client = synx_get_client(session);
	if (IS_ERR_OR_NULL(client))
		return -SYNX_INVALID;

	synx_data = synx_util_acquire_handle(client, h_synx);
	synx_obj = synx_util_obtain_object(synx_data);
	if (IS_ERR_OR_NULL(synx_obj) || IS_ERR_OR_NULL(synx_obj->fence)) {
		dprintk(SYNX_ERR,
			"[sess :%llu] invalid handle access %u\n",
			client->id, h_synx);
		rc = -SYNX_INVALID;
		goto fail;
	}

	if (synx_util_is_global_handle(h_synx)) {
		rc = synx_global_test_status_set_wait(
			synx_util_global_idx(h_synx), SYNX_CORE_APSS);
		if (rc != SYNX_STATE_ACTIVE)
			goto fail;
	}

	timeleft = dma_fence_wait_timeout(synx_obj->fence, (bool) 0,
					msecs_to_jiffies(timeout_ms));
	if (timeleft <= 0) {
		dprintk(SYNX_ERR,
			"[sess :%llu] wait timeout for handle %u\n",
			client->id, h_synx);
		rc = -ETIMEDOUT;
		goto fail;
	}

	mutex_lock(&synx_obj->obj_lock);
	rc = synx_util_get_object_status(synx_obj);
	mutex_unlock(&synx_obj->obj_lock);

fail:
	synx_util_release_handle(synx_data);
	synx_put_client(client);
	return rc;
}
EXPORT_SYMBOL(synx_wait);

int synx_bind(struct synx_session *session,
	u32 h_synx,
	struct synx_external_desc_v2 external_sync)
{
	int rc = 0;
	u32 i;
	u32 bound_idx;
	struct synx_client *client;
	struct synx_handle_coredata *synx_data = NULL;
	struct synx_coredata *synx_obj;
	struct synx_signal_cb *data = NULL;
	struct bind_operations *bind_ops = NULL;

	client = synx_get_client(session);
	if (IS_ERR_OR_NULL(client))
		return -SYNX_INVALID;

	synx_data = synx_util_acquire_handle(client, h_synx);
	synx_obj = synx_util_obtain_object(synx_data);
	if (IS_ERR_OR_NULL(synx_obj)) {
		if (rc || synx_data)
			dprintk(SYNX_ERR,
				"[sess :%llu] invalid handle access %u\n",
				client->id, h_synx);
		goto fail;
	}

	bind_ops = synx_util_get_bind_ops(external_sync.type);
	if (IS_ERR_OR_NULL(bind_ops)) {
		dprintk(SYNX_ERR,
			"[sess :%llu] invalid bind ops for %u\n",
			client->id, external_sync.type);
		rc = -SYNX_INVALID;
		goto fail;
	}

	mutex_lock(&synx_obj->obj_lock);
	if (synx_util_is_merged_object(synx_obj)) {
		dprintk(SYNX_ERR,
			"[sess :%llu] cannot bind to composite handle %u\n",
			client->id, h_synx);
		rc = -SYNX_INVALID;
		goto release;
	}

	if (synx_obj->num_bound_synxs >= SYNX_MAX_NUM_BINDINGS) {
		dprintk(SYNX_ERR,
			"[sess :%llu] max bindings reached for handle %u\n",
			client->id, h_synx);
		rc = -SYNX_NOMEM;
		goto release;
	}

	/* don't bind external sync obj if already done */
	for (i = 0; i < synx_obj->num_bound_synxs; i++) {
		if ((external_sync.id ==
				synx_obj->bound_synxs[i].external_desc.id) &&
				(external_sync.type ==
				synx_obj->bound_synxs[i].external_desc.type)){
			dprintk(SYNX_ERR,
				"[sess :%llu] duplicate bind for sync %llu\n",
				client->id, external_sync.id);
			rc = -SYNX_ALREADY;
			goto release;
		}
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (IS_ERR_OR_NULL(data)) {
		rc = -SYNX_NOMEM;
		goto release;
	}

	/* get additional reference since passing pointer to cb */
	synx_util_get_object(synx_obj);

	/* data passed to external callback */
	data->handle = h_synx;
	data->synx_obj = synx_obj;

	bound_idx = synx_obj->num_bound_synxs;
	memcpy(&synx_obj->bound_synxs[bound_idx],
		&external_sync, sizeof(struct synx_external_desc_v2));
	synx_obj->bound_synxs[bound_idx].external_data = data;
	synx_obj->num_bound_synxs++;
	mutex_unlock(&synx_obj->obj_lock);

	rc = bind_ops->register_callback(synx_external_callback,
			data, external_sync.id);
	if (rc) {
		dprintk(SYNX_ERR,
			"[sess :%llu] callback reg failed for %llu\n",
			client->id, external_sync.id);
		mutex_lock(&synx_obj->obj_lock);
		memset(&synx_obj->bound_synxs[bound_idx], 0,
			sizeof(struct synx_external_desc_v2));
		synx_obj->num_bound_synxs--;
		mutex_unlock(&synx_obj->obj_lock);
		synx_util_put_object(synx_obj);
		kfree(data);
		goto fail;
	}

	synx_util_release_handle(synx_data);
	dprintk(SYNX_DBG,
		"[sess :%llu] ext sync %llu bound to handle %u\n",
		client->id, external_sync.id, h_synx);
	synx_put_client(client);
	return SYNX_SUCCESS;

release:
	mutex_unlock(&synx_obj->obj_lock);
fail:
	synx_util_release_handle(synx_data);
	synx_put_client(client);
	return rc;
}
EXPORT_SYMBOL(synx_bind);

int synx_get_status(struct synx_session *session,
	u32 h_synx)
{
	int rc = 0;
	struct synx_client *client;
	struct synx_handle_coredata *synx_data;
	struct synx_coredata *synx_obj;

	client = synx_get_client(session);
	if (IS_ERR_OR_NULL(client))
		return -SYNX_INVALID;

	synx_data = synx_util_acquire_handle(client, h_synx);
	synx_obj = synx_util_obtain_object(synx_data);
	if (IS_ERR_OR_NULL(synx_obj) ||
		IS_ERR_OR_NULL(synx_obj->fence)) {
		dprintk(SYNX_ERR,
			"[sess :%llu] invalid handle access %u\n",
			client->id, h_synx);
		rc = -SYNX_INVALID;
		goto fail;
	}

	if (synx_util_is_global_handle(h_synx)) {
		rc = synx_global_get_status(
				synx_util_global_idx(h_synx));
		if (rc != SYNX_STATE_ACTIVE) {
			dprintk(SYNX_VERB,
				"[sess :%llu] handle %u in status %d\n",
				client->id, h_synx, rc);
			goto fail;
		}
	}

	mutex_lock(&synx_obj->obj_lock);
	rc = synx_util_get_object_status(synx_obj);
	mutex_unlock(&synx_obj->obj_lock);
	dprintk(SYNX_VERB,
		"[sess :%llu] handle %u status %d\n",
		client->id, h_synx, rc);

fail:
	synx_util_release_handle(synx_data);
	synx_put_client(client);
	return rc;
}
EXPORT_SYMBOL(synx_get_status);

static struct synx_map_entry *synx_handle_conversion(
	struct synx_client *client,
	u32 *h_synx, struct synx_map_entry *old_entry)
{
	int rc;
	struct synx_map_entry *map_entry = NULL;
	struct synx_coredata *synx_obj;

	if (IS_ERR_OR_NULL(old_entry)) {
		old_entry = synx_util_get_map_entry(*h_synx);
		if (IS_ERR_OR_NULL(old_entry)) {
			rc = PTR_ERR(old_entry);
			dprintk(SYNX_ERR,
				"invalid import handle %u err=%d",
				*h_synx, rc);
			return old_entry;
		}
	}

	synx_obj = old_entry->synx_obj;
	BUG_ON(synx_obj == NULL);

	mutex_lock(&synx_obj->obj_lock);
	synx_util_get_object(synx_obj);
	if (synx_obj->global_idx != 0) {
		*h_synx = synx_encode_handle(
				synx_obj->global_idx, SYNX_CORE_APSS, true);

		map_entry = synx_util_get_map_entry(*h_synx);
		if (IS_ERR_OR_NULL(map_entry)) {
			/* raced with release from last global client */
			map_entry = synx_util_insert_to_map(synx_obj,
						*h_synx, 0);
			if (IS_ERR_OR_NULL(map_entry)) {
				rc = PTR_ERR(map_entry);
				dprintk(SYNX_ERR,
					"addition of %u to map failed=%d",
					*h_synx, rc);
			}
		}
	} else {
		rc = synx_alloc_global_handle(h_synx);
		if (rc == SYNX_SUCCESS) {
			synx_obj->global_idx =
				synx_util_global_idx(*h_synx);
			synx_obj->type |= SYNX_CREATE_GLOBAL_FENCE;

			map_entry = synx_util_insert_to_map(synx_obj,
						*h_synx, 0);
			if (IS_ERR_OR_NULL(map_entry)) {
				rc = PTR_ERR(map_entry);
				synx_global_put_ref(
					synx_util_global_idx(*h_synx));
				dprintk(SYNX_ERR,
					"insertion of %u to map failed=%d",
					*h_synx, rc);
			}
		}
	}
	mutex_unlock(&synx_obj->obj_lock);

	if (IS_ERR_OR_NULL(map_entry))
		synx_util_put_object(synx_obj);

	synx_util_release_map_entry(old_entry);
	return map_entry;
}

static int synx_native_import_handle(struct synx_client *client,
	struct synx_import_indv_params *params)
{
	int rc = SYNX_SUCCESS;
	u32 h_synx, core_id;
	struct synx_map_entry *map_entry, *old_entry;
	struct synx_coredata *synx_obj;
	struct synx_handle_coredata *synx_data = NULL, *curr;
	char name[SYNX_OBJ_NAME_LEN] = {0};
	struct synx_create_params c_params = {0};

	if (IS_ERR_OR_NULL(client) || IS_ERR_OR_NULL(params) ||
		IS_ERR_OR_NULL(params->fence) ||
		IS_ERR_OR_NULL(params->new_h_synx))
		return -SYNX_INVALID;

	h_synx = *((u32 *)params->fence);

	/* check if already mapped to client */
	spin_lock_bh(&client->handle_map_lock);
	hash_for_each_possible(client->handle_map,
			curr, node, h_synx) {
		if (curr->key == h_synx &&
				curr->rel_count != 0 &&
				(synx_util_is_global_handle(h_synx) ||
				params->flags & SYNX_IMPORT_LOCAL_FENCE)) {
			curr->rel_count++;
			kref_get(&curr->refcount);
			synx_data = curr;
			break;
		}
	}
	spin_unlock_bh(&client->handle_map_lock);

	if (synx_data) {
		*params->new_h_synx = h_synx;
		return SYNX_SUCCESS;
	}

	map_entry = synx_util_get_map_entry(h_synx);
	if (IS_ERR_OR_NULL(map_entry)) {
		core_id = (h_synx & SYNX_OBJ_CORE_ID_MASK)
					>> SYNX_HANDLE_INDEX_BITS;
		if (core_id == SYNX_CORE_APSS) {
			dprintk(SYNX_ERR,
				"[sess :%llu] invalid import handle %u\n",
				client->id, h_synx);
			return -SYNX_INVALID;
		} else if (synx_util_is_global_handle(h_synx)) {
			/* import global handle created in another core */
			synx_util_map_import_params_to_create(params, &c_params);
			scnprintf(name, SYNX_OBJ_NAME_LEN, "import-client-%d",
				current->pid);
			c_params.name = name;
			c_params.h_synx = &h_synx;

			rc = synx_native_create_core(client, &c_params);
			if (rc != SYNX_SUCCESS)
				return rc;

			*params->new_h_synx = h_synx;
			return SYNX_SUCCESS;
		}
		dprintk(SYNX_ERR,
			"[sess :%llu] invalid handle %u\n",
			client->id, h_synx);
		return -SYNX_INVALID;
	}

	synx_obj = map_entry->synx_obj;
	BUG_ON(synx_obj == NULL);

	if ((params->flags & SYNX_IMPORT_GLOBAL_FENCE) &&
		!synx_util_is_global_handle(h_synx)) {
		old_entry = map_entry;
		map_entry = synx_handle_conversion(client, &h_synx,
						old_entry);
	}

	if (rc != SYNX_SUCCESS)
		return rc;

	*params->new_h_synx = h_synx;

	rc = synx_util_init_handle(client, map_entry->synx_obj,
		params->new_h_synx, map_entry);
	if (rc != SYNX_SUCCESS) {
		dprintk(SYNX_ERR,
			"[sess :%llu] init of imported handle %u failed=%d\n",
			client->id, h_synx, rc);
		synx_util_release_map_entry(map_entry);
	}

	return rc;
}

static int synx_native_import_fence(struct synx_client *client,
	struct synx_import_indv_params *params)
{
	int rc = SYNX_SUCCESS;
	u32 curr_h_synx;
	u32 global;
	struct synx_create_params c_params = {0};
	char name[SYNX_OBJ_NAME_LEN] = {0};
	struct synx_fence_entry *entry;
	struct synx_map_entry *map_entry = NULL;
	struct synx_handle_coredata *synx_data = NULL, *curr;

	if (IS_ERR_OR_NULL(client) || IS_ERR_OR_NULL(params) ||
			IS_ERR_OR_NULL(params->fence) ||
			IS_ERR_OR_NULL(params->new_h_synx))
		return -SYNX_INVALID;

	global = SYNX_IMPORT_GLOBAL_FENCE & params->flags;

retry:
	*params->new_h_synx =
		synx_util_get_fence_entry((u64)params->fence, global);
	if (*params->new_h_synx == 0) {
		/* create a new synx obj and add to fence map */
		synx_util_map_import_params_to_create(params, &c_params);
		scnprintf(name, SYNX_OBJ_NAME_LEN, "import-client-%d",
			current->pid);
		c_params.name = name;
		c_params.h_synx = params->new_h_synx;
		c_params.fence = params->fence;

		rc = synx_native_create_core(client, &c_params);
		if (rc != SYNX_SUCCESS)
			return rc;

		curr_h_synx = *params->new_h_synx;

		entry = kzalloc(sizeof(*entry), GFP_KERNEL);
		if (IS_ERR_OR_NULL(entry)) {
			rc = -SYNX_NOMEM;
			curr_h_synx = *c_params.h_synx;
			goto fail;
		}

		do {
			entry->key = (u64)params->fence;
			if (global)
				entry->g_handle = *params->new_h_synx;
			else
				entry->l_handle = *params->new_h_synx;

			rc = synx_util_insert_fence_entry(entry,
					params->new_h_synx, global);
			if (rc == SYNX_SUCCESS) {
				dprintk(SYNX_DBG,
					"mapped fence %pK to new handle %u\n",
					params->fence, *params->new_h_synx);
				break;
			} else if (rc == -SYNX_ALREADY) {
				/*
				 * release the new handle allocated
				 * and use the available handle
				 * already mapped instead.
				 */
				map_entry = synx_util_get_map_entry(
								*params->new_h_synx);
				if (IS_ERR_OR_NULL(map_entry)) {
					/* race with fence release, need to retry */
					dprintk(SYNX_DBG,
						"re-attempting handle import\n");
					*params->new_h_synx = curr_h_synx;
					continue;
				}

				rc = synx_util_init_handle(client,
						map_entry->synx_obj,
						params->new_h_synx, map_entry);

				dprintk(SYNX_DBG, "mapped fence %pK to handle %u\n",
					params->fence, *params->new_h_synx);
				goto release;
			} else {
				dprintk(SYNX_ERR,
					"importing fence %pK failed, err=%d\n",
					params->fence, rc);
				goto release;
			}
		} while (true);
	} else {
		/* check if already mapped to client */
		spin_lock_bh(&client->handle_map_lock);
		hash_for_each_possible(client->handle_map,
				curr, node, *params->new_h_synx) {
			if (curr->key == *params->new_h_synx &&
					curr->rel_count != 0) {
				curr->rel_count++;
				kref_get(&curr->refcount);
				synx_data = curr;
				break;
			}
		}
		spin_unlock_bh(&client->handle_map_lock);

		if (synx_data) {
			dprintk(SYNX_DBG, "mapped fence %pK to handle %u\n",
				params->fence, *params->new_h_synx);
			return SYNX_SUCCESS;
		}

		if (global && !synx_util_is_global_handle(
				*params->new_h_synx))
			map_entry = synx_handle_conversion(client,
				params->new_h_synx, NULL);
		else
			map_entry = synx_util_get_map_entry(
						*params->new_h_synx);

		if (IS_ERR_OR_NULL(map_entry)) {
			/* race with fence release, need to retry */
			dprintk(SYNX_DBG, "re-attempting handle import\n");
			goto retry;
		}

		rc = synx_util_init_handle(client, map_entry->synx_obj,
			params->new_h_synx, map_entry);

		dprintk(SYNX_DBG, "mapped fence %pK to existing handle %u\n",
			params->fence, *params->new_h_synx);
	}

	return rc;

release:
	kfree(entry);
fail:
	synx_native_release_core(client, curr_h_synx);
	return rc;
}

static int synx_native_import_indv(struct synx_client *client,
	struct synx_import_indv_params *params)
{
	int rc = -SYNX_INVALID;

	if (IS_ERR_OR_NULL(params) ||
		IS_ERR_OR_NULL(params->new_h_synx) ||
		IS_ERR_OR_NULL(params->fence)) {
		dprintk(SYNX_ERR, "invalid import arguments\n");
		return -SYNX_INVALID;
	}

	if (likely(params->flags & SYNX_IMPORT_DMA_FENCE))
		rc = synx_native_import_fence(client, params);
	else if (params->flags & SYNX_IMPORT_SYNX_FENCE)
		rc = synx_native_import_handle(client, params);

	dprintk(SYNX_DBG,
		"[sess :%llu] import of fence %pK %s, handle %u\n",
		client->id, params->fence,
		rc ? "failed" : "successful",
		rc ? 0 : *params->new_h_synx);

	return rc;
}

static int synx_native_import_arr(struct synx_client *client,
	struct synx_import_arr_params *params)
{
	u32 i;
	int rc = SYNX_SUCCESS;

	if (IS_ERR_OR_NULL(params) || params->num_fences == 0) {
		dprintk(SYNX_ERR, "invalid import arr arguments\n");
		return -SYNX_INVALID;
	}

	for (i = 0; i < params->num_fences; i++) {
		rc = synx_native_import_indv(client, &params->list[i]);
		if (rc != SYNX_SUCCESS) {
			dprintk(SYNX_ERR,
				"importing fence[%u] %pK failed=%d\n",
				i, params->list[i].fence, rc);
			break;
		}
	}

	if (rc != SYNX_SUCCESS)
		while (i--) {
			/* release the imported handles and cleanup */
			if (synx_native_release_core(client,
				*params->list[i].new_h_synx) != SYNX_SUCCESS)
				dprintk(SYNX_ERR,
					"error cleaning up imported handle[%u] %u\n",
					i, *params->list[i].new_h_synx);
		}

	return rc;
}

int synx_import(struct synx_session *session,
	struct synx_import_params *params)
{
	int rc = 0;
	struct synx_client *client;

	if (IS_ERR_OR_NULL(params)) {
		dprintk(SYNX_ERR, "invalid import arguments\n");
		return -SYNX_INVALID;
	}

	client = synx_get_client(session);
	if (IS_ERR_OR_NULL(client))
		return -SYNX_INVALID;

	/* import fence based on its type */
	if (params->type == SYNX_IMPORT_ARR_PARAMS)
		rc = synx_native_import_arr(client, &params->arr);
	else
		rc = synx_native_import_indv(client, &params->indv);

	synx_put_client(client);
	return rc;
}
EXPORT_SYMBOL(synx_import);

static int synx_handle_create(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	int result;
	int csl_fence;
	struct synx_create_v2 create_info;
	struct synx_create_params params = {0};

	if (k_ioctl->size != sizeof(create_info))
		return -SYNX_INVALID;

	if (copy_from_user(&create_info,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	params.h_synx = &create_info.synx_obj;
	params.name = create_info.name;
	params.flags = create_info.flags;
	if (create_info.flags & SYNX_CREATE_CSL_FENCE) {
		csl_fence = create_info.desc.id[0];
		params.fence = &csl_fence;
	}
	result = synx_create(session, &params);

	if (!result)
		if (copy_to_user(u64_to_user_ptr(k_ioctl->ioctl_ptr),
				&create_info,
				k_ioctl->size))
			return -EFAULT;

	return result;
}

static int synx_handle_getstatus(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	struct synx_signal_v2 signal_info;

	if (k_ioctl->size != sizeof(signal_info))
		return -SYNX_INVALID;

	if (copy_from_user(&signal_info,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	signal_info.synx_state =
		synx_get_status(session, signal_info.synx_obj);

	if (copy_to_user(u64_to_user_ptr(k_ioctl->ioctl_ptr),
			&signal_info,
			k_ioctl->size))
		return -EFAULT;

	return SYNX_SUCCESS;
}

static int synx_handle_import(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	struct synx_import_info import_info;
	struct synx_import_params params = {0};

	if (k_ioctl->size != sizeof(import_info))
		return -SYNX_INVALID;

	if (copy_from_user(&import_info,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	if (import_info.flags & SYNX_IMPORT_SYNX_FENCE)
		params.indv.fence = &import_info.synx_obj;
	else if (import_info.flags & SYNX_IMPORT_DMA_FENCE)
		params.indv.fence =
			sync_file_get_fence(import_info.desc.id[0]);

	params.type = SYNX_IMPORT_INDV_PARAMS;
	params.indv.flags = import_info.flags;
	params.indv.new_h_synx = &import_info.new_synx_obj;

	if (synx_import(session, &params))
		return -SYNX_INVALID;

	if (import_info.flags & SYNX_IMPORT_DMA_FENCE)
		dma_fence_put(params.indv.fence);

	if (copy_to_user(u64_to_user_ptr(k_ioctl->ioctl_ptr),
			&import_info,
			k_ioctl->size))
		return -EFAULT;

	return SYNX_SUCCESS;
}

static int synx_handle_import_arr(
	struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	int rc = -SYNX_INVALID;
	u32 idx = 0;
	struct synx_client *client;
	struct synx_import_arr_info arr_info;
	struct synx_import_info *arr;
	struct synx_import_indv_params params = {0};

	if (k_ioctl->size != sizeof(arr_info))
		return -SYNX_INVALID;

	if (copy_from_user(&arr_info,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	arr = kcalloc(arr_info.num_objs,
				sizeof(*arr), GFP_KERNEL);
	if (IS_ERR_OR_NULL(arr))
		return -ENOMEM;

	client = synx_get_client(session);
	if (IS_ERR_OR_NULL(client)) {
		rc = PTR_ERR(client);
		goto clean;
	}

	if (copy_from_user(arr,
			u64_to_user_ptr(arr_info.list),
			sizeof(*arr) * arr_info.num_objs)) {
		rc = -EFAULT;
		goto fail;
	}

	while (idx < arr_info.num_objs) {
		params.new_h_synx = &arr[idx].new_synx_obj;
		params.flags = arr[idx].flags;
		if (arr[idx].flags & SYNX_IMPORT_SYNX_FENCE)
			params.fence = &arr[idx].synx_obj;
		if (arr[idx].flags & SYNX_IMPORT_DMA_FENCE)
			params.fence =
				sync_file_get_fence(arr[idx].desc.id[0]);
		rc = synx_native_import_indv(client, &params);
		if (rc != SYNX_SUCCESS)
			break;
		idx++;
	}

	/* release allocated handles in case of failure */
	if (rc != SYNX_SUCCESS) {
		while (idx > 0)
			synx_native_release_core(client,
				arr[--idx].new_synx_obj);
	} else {
		if (copy_to_user(u64_to_user_ptr(arr_info.list),
			arr,
			sizeof(*arr) * arr_info.num_objs)) {
			rc = -EFAULT;
			goto fail;
		}
	}

fail:
	synx_put_client(client);
clean:
	kfree(arr);
	return rc;
}

static int synx_handle_export(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	return -SYNX_INVALID;
}

static int synx_handle_signal(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	struct synx_signal_v2 signal_info;

	if (k_ioctl->size != sizeof(signal_info))
		return -SYNX_INVALID;

	if (copy_from_user(&signal_info,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	return synx_signal(session, signal_info.synx_obj,
		signal_info.synx_state);
}

static int synx_handle_merge(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	u32 *h_synxs;
	int result;
	struct synx_merge_v2 merge_info;
	struct synx_merge_params params = {0};

	if (k_ioctl->size != sizeof(merge_info))
		return -SYNX_INVALID;

	if (copy_from_user(&merge_info,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	if (merge_info.num_objs >= SYNX_MAX_OBJS)
		return -SYNX_INVALID;

	h_synxs = kcalloc(merge_info.num_objs,
				sizeof(*h_synxs), GFP_KERNEL);
	if (IS_ERR_OR_NULL(h_synxs))
		return -ENOMEM;

	if (copy_from_user(h_synxs,
			u64_to_user_ptr(merge_info.synx_objs),
			sizeof(u32) * merge_info.num_objs)) {
		kfree(h_synxs);
		return -EFAULT;
	}

	params.num_objs = merge_info.num_objs;
	params.h_synxs = h_synxs;
	params.flags = merge_info.flags;
	params.h_merged_obj = &merge_info.merged;

	result = synx_merge(session, &params);
	if (!result)
		if (copy_to_user(u64_to_user_ptr(k_ioctl->ioctl_ptr),
				&merge_info,
				k_ioctl->size)) {
			kfree(h_synxs);
			return -EFAULT;
	}

	kfree(h_synxs);
	return result;
}

static int synx_handle_wait(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	struct synx_wait_v2 wait_info;

	if (k_ioctl->size != sizeof(wait_info))
		return -SYNX_INVALID;

	if (copy_from_user(&wait_info,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	k_ioctl->result = synx_wait(session,
		wait_info.synx_obj, wait_info.timeout_ms);

	return SYNX_SUCCESS;
}

static int synx_handle_async_wait(
	struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	int rc = 0;
	struct synx_userpayload_info_v2 user_data;
	struct synx_callback_params params = {0};

	if (k_ioctl->size != sizeof(user_data))
		return -SYNX_INVALID;

	if (copy_from_user(&user_data,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	params.h_synx = user_data.synx_obj;
	params.cb_func = synx_util_default_user_callback;
	params.userdata = (void *)user_data.payload[0];
	params.timeout_ms = user_data.payload[2];

	rc = synx_async_wait(session, &params);
	if (rc)
		dprintk(SYNX_ERR,
			"user cb registration failed for handle %d\n",
			user_data.synx_obj);

	return rc;
}

static int synx_handle_cancel_async_wait(
	struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	int rc = 0;
	struct synx_userpayload_info_v2 user_data;
	struct synx_callback_params params = {0};

	if (k_ioctl->size != sizeof(user_data))
		return -SYNX_INVALID;

	if (copy_from_user(&user_data,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	params.h_synx = user_data.synx_obj;
	params.cb_func = synx_util_default_user_callback;
	params.userdata = (void *)user_data.payload[0];

	rc = synx_cancel_async_wait(session, &params);
	if (rc)
		dprintk(SYNX_ERR,
			"user cb deregistration failed for handle %d\n",
			user_data.synx_obj);

	return rc;
}

static int synx_handle_bind(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	struct synx_bind_v2 synx_bind_info;

	if (k_ioctl->size != sizeof(synx_bind_info))
		return -SYNX_INVALID;

	if (copy_from_user(&synx_bind_info,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	k_ioctl->result = synx_bind(session,
		synx_bind_info.synx_obj,
		synx_bind_info.ext_sync_desc);

	return k_ioctl->result;
}

static int synx_handle_release(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	struct synx_info release_info;

	if (k_ioctl->size != sizeof(release_info))
		return -SYNX_INVALID;

	if (copy_from_user(&release_info,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	return synx_release(session, release_info.synx_obj);
}

static int synx_handle_get_fence(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	struct synx_fence_fd fence_fd;
	struct dma_fence *fence;

	if (k_ioctl->size != sizeof(fence_fd))
		return -SYNX_INVALID;

	if (copy_from_user(&fence_fd,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	fence = synx_get_fence(session, fence_fd.synx_obj);
	fence_fd.fd = synx_create_sync_fd(fence);
	/*
	 * release additional reference taken in synx_get_fence.
	 * additional reference ensures the fence is valid and
	 * does not race with handle/fence release.
	 */
	dma_fence_put(fence);

	if (copy_to_user(u64_to_user_ptr(k_ioctl->ioctl_ptr),
			&fence_fd, k_ioctl->size))
		return -EFAULT;

	return SYNX_SUCCESS;
}

static long synx_ioctl(struct file *filep,
	unsigned int cmd,
	unsigned long arg)
{
	s32 rc = 0;
	struct synx_private_ioctl_arg k_ioctl;
	struct synx_session *session = filep->private_data;

	if (cmd != SYNX_PRIVATE_IOCTL_CMD) {
		dprintk(SYNX_ERR, "invalid ioctl cmd\n");
		return -ENOIOCTLCMD;
	}

	if (copy_from_user(&k_ioctl,
			(struct synx_private_ioctl_arg *)arg,
			sizeof(k_ioctl))) {
		dprintk(SYNX_ERR, "invalid ioctl args\n");
		return -EFAULT;
	}

	if (!k_ioctl.ioctl_ptr)
		return -SYNX_INVALID;

	dprintk(SYNX_VERB, "[sess :%llu] Enter cmd %u from pid %d\n",
		((struct synx_client *)session)->id,
		k_ioctl.id, current->pid);

	switch (k_ioctl.id) {
	case SYNX_CREATE:
		rc = synx_handle_create(&k_ioctl, session);
		break;
	case SYNX_RELEASE:
		rc = synx_handle_release(&k_ioctl, session);
		break;
	case SYNX_REGISTER_PAYLOAD:
		rc = synx_handle_async_wait(&k_ioctl,
				session);
		break;
	case SYNX_DEREGISTER_PAYLOAD:
		rc = synx_handle_cancel_async_wait(&k_ioctl,
				session);
		break;
	case SYNX_SIGNAL:
		rc = synx_handle_signal(&k_ioctl, session);
		break;
	case SYNX_MERGE:
		rc = synx_handle_merge(&k_ioctl, session);
		break;
	case SYNX_WAIT:
		rc = synx_handle_wait(&k_ioctl, session);
		if (copy_to_user((void *)arg,
			&k_ioctl,
			sizeof(k_ioctl))) {
			dprintk(SYNX_ERR, "invalid ioctl args\n");
			rc = -EFAULT;
		}
		break;
	case SYNX_BIND:
		rc = synx_handle_bind(&k_ioctl, session);
		break;
	case SYNX_GETSTATUS:
		rc = synx_handle_getstatus(&k_ioctl, session);
		break;
	case SYNX_IMPORT:
		rc = synx_handle_import(&k_ioctl, session);
		break;
	case SYNX_IMPORT_ARR:
		rc = synx_handle_import_arr(&k_ioctl, session);
		break;
	case SYNX_EXPORT:
		rc = synx_handle_export(&k_ioctl, session);
		break;
	case SYNX_GETFENCE_FD:
		rc = synx_handle_get_fence(&k_ioctl, session);
		break;
	default:
		rc = -SYNX_INVALID;
	}

	dprintk(SYNX_VERB, "[sess :%llu] exit with status %d\n",
		((struct synx_client *)session)->id, rc);

	return rc;
}

static ssize_t synx_read(struct file *filep,
	char __user *buf, size_t size, loff_t *f_pos)
{
	ssize_t rc = 0;
	struct synx_client *client = NULL;
	struct synx_client_cb *cb;
	struct synx_session *session = filep->private_data;
	struct synx_userpayload_info_v2 data;

	if (size != sizeof(struct synx_userpayload_info_v2)) {
		dprintk(SYNX_ERR, "invalid read size\n");
		return -SYNX_INVALID;
	}

	client = synx_get_client(session);
	if (IS_ERR_OR_NULL(client))
		return -SYNX_INVALID;

	mutex_lock(&client->event_q_lock);
	cb = list_first_entry_or_null(&client->event_q,
			struct synx_client_cb, node);
	if (IS_ERR_OR_NULL(cb)) {
		mutex_unlock(&client->event_q_lock);
		rc = 0;
		goto fail;
	}

	if (cb->idx == 0 || cb->idx >= SYNX_MAX_OBJS) {
		dprintk(SYNX_ERR, "invalid index\n");
		mutex_unlock(&client->event_q_lock);
		rc = -SYNX_INVALID;
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
			sizeof(struct synx_userpayload_info_v2))) {
		dprintk(SYNX_ERR, "couldn't copy user callback data\n");
		rc = -EFAULT;
	}

	if (synx_util_clear_cb_entry(client, cb))
		dprintk(SYNX_ERR,
			"[sess :%llu] error clearing cb for handle %u\n",
			client->id, data.synx_obj);
fail:
	synx_put_client(client);
	return rc;
}

static unsigned int synx_poll(struct file *filep,
	struct poll_table_struct *poll_table)
{
	int rc = 0;
	struct synx_client *client;
	struct synx_session *session = filep->private_data;

	client = synx_get_client(session);
	if (IS_ERR_OR_NULL(client)) {
		dprintk(SYNX_ERR, "invalid session in poll\n");
		return SYNX_SUCCESS;
	}

	poll_wait(filep, &client->event_wq, poll_table);
	mutex_lock(&client->event_q_lock);
	if (!list_empty(&client->event_q))
		rc = POLLPRI;
	mutex_unlock(&client->event_q_lock);

	synx_put_client(client);
	return rc;
}

struct synx_session *synx_initialize(
	struct synx_initialization_params *params)
{
	struct synx_client *client;

	if (IS_ERR_OR_NULL(params))
		return ERR_PTR(-SYNX_INVALID);

	client = vzalloc(sizeof(*client));
	if (IS_ERR_OR_NULL(client))
		return ERR_PTR(-SYNX_NOMEM);

	if (params->name)
		strlcpy(client->name, params->name, sizeof(client->name));

	client->active = true;
	client->dma_context = dma_fence_context_alloc(1);
	client->id = atomic64_inc_return(&synx_counter);
	kref_init(&client->refcount);
	spin_lock_init(&client->handle_map_lock);
	mutex_init(&client->event_q_lock);
	INIT_LIST_HEAD(&client->event_q);
	init_waitqueue_head(&client->event_wq);
	/* zero idx not allowed */
	set_bit(0, client->cb_bitmap);

	spin_lock_bh(&synx_dev->native->metadata_map_lock);
	hash_add(synx_dev->native->client_metadata_map,
		&client->node, (u64)client);
	spin_unlock_bh(&synx_dev->native->metadata_map_lock);

	dprintk(SYNX_INFO, "[sess :%llu] session created %s\n",
		client->id, params->name);

	return (struct synx_session *)client;
}
EXPORT_SYMBOL(synx_initialize);

int synx_uninitialize(struct synx_session *session)
{
	struct synx_client *client = NULL, *curr;

	spin_lock_bh(&synx_dev->native->metadata_map_lock);
	hash_for_each_possible(synx_dev->native->client_metadata_map,
			curr, node, (u64)session) {
		if (curr == (struct synx_client *)session) {
			if (curr->active) {
				curr->active = false;
				client = curr;
			}
			break;
		}
	}
	spin_unlock_bh(&synx_dev->native->metadata_map_lock);

	/* release the reference obtained at synx init */
	synx_put_client(client);
	return SYNX_SUCCESS;
}
EXPORT_SYMBOL(synx_uninitialize);

static int synx_open(struct inode *inode, struct file *filep)
{
	int rc = 0;
	char name[SYNX_OBJ_NAME_LEN];
	struct synx_initialization_params params = {0};

	dprintk(SYNX_VERB, "Enter pid: %d\n", current->pid);

	scnprintf(name, SYNX_OBJ_NAME_LEN, "umd-client-%d", current->pid);
	params.name = name;
	params.id = SYNX_CLIENT_NATIVE;

	filep->private_data = synx_initialize(&params);
	if (IS_ERR_OR_NULL(filep->private_data)) {
		dprintk(SYNX_ERR, "session allocation failed for pid: %d\n",
			current->pid);
		rc = PTR_ERR(filep->private_data);
	} else {
		dprintk(SYNX_VERB, "allocated new session for pid: %d\n",
			current->pid);
	}

	return rc;
}

static int synx_close(struct inode *inode, struct file *filep)
{
	struct synx_session *session = filep->private_data;

	return synx_uninitialize(session);
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

int synx_register_ops(
	const struct synx_register_params *params)
{
	s32 rc = 0;
	struct synx_registered_ops *client_ops;

	if (!synx_dev || !params || !params->name ||
		 !synx_util_is_valid_bind_type(params->type) ||
		 !params->ops.register_callback ||
		 !params->ops.deregister_callback ||
		 !params->ops.signal) {
		dprintk(SYNX_ERR, "invalid register params\n");
		return -SYNX_INVALID;
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
		dprintk(SYNX_INFO,
			"registered bind ops type %u for %s\n",
			params->type, params->name);
	} else {
		dprintk(SYNX_WARN,
			"client already registered for type %u by %s\n",
			client_ops->type, client_ops->name);
		rc = -SYNX_ALREADY;
	}
	mutex_unlock(&synx_dev->vtbl_lock);

	return rc;
}
EXPORT_SYMBOL(synx_register_ops);

int synx_deregister_ops(
	const struct synx_register_params *params)
{
	struct synx_registered_ops *client_ops;

	if (IS_ERR_OR_NULL(params) || params->name ||
		!synx_util_is_valid_bind_type(params->type)) {
		dprintk(SYNX_ERR, "invalid params\n");
		return -SYNX_INVALID;
	}

	mutex_lock(&synx_dev->vtbl_lock);
	client_ops = &synx_dev->bind_vtbl[params->type];
	memset(client_ops, 0, sizeof(*client_ops));
	dprintk(SYNX_INFO, "deregistered bind ops for %s\n",
		params->name);
	mutex_unlock(&synx_dev->vtbl_lock);

	return SYNX_SUCCESS;
}
EXPORT_SYMBOL(synx_deregister_ops);

void synx_ipc_handler(struct work_struct *cb_dispatch)
{
	struct synx_signal_cb *signal_cb =
		container_of(cb_dispatch, struct synx_signal_cb, cb_dispatch);
	struct synx_map_entry *map_entry;

	map_entry = synx_util_get_map_entry(signal_cb->handle);
	if (IS_ERR_OR_NULL(map_entry)) {
		dprintk(SYNX_WARN,
			"no clients to notify for %u\n",
			signal_cb->handle);
		dprintk(SYNX_MEM, "signal cb destroyed %pK\n", signal_cb);
		kfree(signal_cb);
		return;
	}

	/* get reference on synx coredata for signal cb */
	synx_util_get_object(map_entry->synx_obj);
	signal_cb->synx_obj = map_entry->synx_obj;
	synx_util_release_map_entry(map_entry);
	synx_signal_handler(&signal_cb->cb_dispatch);
}

int synx_ipc_callback(u32 client_id,
	s64 data, void *priv)
{
	struct synx_signal_cb *signal_cb;
	u32 status = (u32)data;
	u32 handle = (u32)(data >> 32);

	signal_cb = kzalloc(sizeof(*signal_cb), GFP_ATOMIC);
	if (IS_ERR_OR_NULL(signal_cb))
		return -SYNX_NOMEM;

	dprintk(SYNX_DBG,
		"signal notification for %u received with status %u\n",
		handle, status);

	signal_cb->status = status;
	signal_cb->handle = handle;
	signal_cb->flag = SYNX_SIGNAL_FROM_IPC;

	INIT_WORK(&signal_cb->cb_dispatch, synx_ipc_handler);
	queue_work(synx_dev->wq_cb, &signal_cb->cb_dispatch);

	return SYNX_SUCCESS;
}
EXPORT_SYMBOL(synx_ipc_callback);

int synx_recover(enum synx_client_id id)
{
	u32 core_id;

	core_id = synx_util_map_client_id_to_core(id);
	if (core_id >= SYNX_CORE_MAX) {
		dprintk(SYNX_ERR, "invalid client id %u\n", id);
		return -SYNX_INVALID;
	}

	switch (core_id) {
	case SYNX_CORE_EVA:
	case SYNX_CORE_IRIS:
		break;
	default:
		dprintk(SYNX_ERR, "recovery not supported on %u\n", id);
		return -SYNX_NOSUPPORT;
	}

	return synx_global_recover(core_id);
}
EXPORT_SYMBOL(synx_recover);

static int synx_local_mem_init(void)
{
	if (!synx_dev->native)
		return -SYNX_INVALID;

	hash_init(synx_dev->native->client_metadata_map);
	hash_init(synx_dev->native->fence_map);
	hash_init(synx_dev->native->global_map);
	hash_init(synx_dev->native->local_map);
	hash_init(synx_dev->native->csl_fence_map);

	spin_lock_init(&synx_dev->native->metadata_map_lock);
	spin_lock_init(&synx_dev->native->fence_map_lock);
	spin_lock_init(&synx_dev->native->global_map_lock);
	spin_lock_init(&synx_dev->native->local_map_lock);
	spin_lock_init(&synx_dev->native->csl_map_lock);

	/* zero idx not allowed */
	set_bit(0, synx_dev->native->bitmap);
	return 0;
}

static int synx_cdsp_restart_notifier(struct notifier_block *nb,
	unsigned long code, void *data)
{
	struct synx_cdsp_ssr *cdsp_ssr = &synx_dev->cdsp_ssr;

	if (&cdsp_ssr->nb != nb) {
		dprintk(SYNX_ERR, "Invalid SSR Notifier block\n");
		return NOTIFY_BAD;
	}

	switch (code) {
	case QCOM_SSR_BEFORE_SHUTDOWN:
		break;
	case QCOM_SSR_AFTER_SHUTDOWN:
		if (cdsp_ssr->ssrcnt != 0) {
			dprintk(SYNX_INFO, "Cleaning up global memory\n");
			synx_global_recover(SYNX_CORE_NSP);
		}
		break;
	case QCOM_SSR_BEFORE_POWERUP:
		break;
	case QCOM_SSR_AFTER_POWERUP:
		dprintk(SYNX_DBG, "CDSP is up");
		if (cdsp_ssr->ssrcnt == 0)
			cdsp_ssr->ssrcnt++;
		break;
	default:
		dprintk(SYNX_ERR, "Unknown status code for CDSP SSR\n");
		break;
	}

	return NOTIFY_DONE;
}

static int __init synx_init(void)
{
	int rc;

	dprintk(SYNX_INFO, "device initialization start\n");

	synx_dev = kzalloc(sizeof(*synx_dev), GFP_KERNEL);
	if (IS_ERR_OR_NULL(synx_dev))
		return -SYNX_NOMEM;

	rc = alloc_chrdev_region(&synx_dev->dev, 0, 1, SYNX_DEVICE_NAME);
	if (rc < 0) {
		dprintk(SYNX_ERR, "region allocation failed\n");
		goto alloc_fail;
	}

	cdev_init(&synx_dev->cdev, &synx_fops);
	synx_dev->cdev.owner = THIS_MODULE;
	rc = cdev_add(&synx_dev->cdev, synx_dev->dev, 1);
	if (rc < 0) {
		dprintk(SYNX_ERR, "device registation failed\n");
		goto reg_fail;
	}

	synx_dev->class = class_create(THIS_MODULE, SYNX_DEVICE_NAME);
	device_create(synx_dev->class, NULL, synx_dev->dev,
		NULL, SYNX_DEVICE_NAME);

	synx_dev->wq_cb = alloc_workqueue(SYNX_WQ_CB_NAME,
		WQ_HIGHPRI | WQ_UNBOUND, SYNX_WQ_CB_THREADS);
	synx_dev->wq_cleanup = alloc_workqueue(SYNX_WQ_CLEANUP_NAME,
		WQ_HIGHPRI | WQ_UNBOUND, SYNX_WQ_CLEANUP_THREADS);
	if (!synx_dev->wq_cb || !synx_dev->wq_cleanup) {
		dprintk(SYNX_ERR,
			"high priority work queue creation failed\n");
		rc = -SYNX_INVALID;
		goto fail;
	}

	synx_dev->native = vzalloc(sizeof(*synx_dev->native));
	if (IS_ERR_OR_NULL(synx_dev->native))
		goto fail;

	mutex_init(&synx_dev->vtbl_lock);
	mutex_init(&synx_dev->error_lock);
	INIT_LIST_HEAD(&synx_dev->error_list);
	synx_dev->debugfs_root = synx_init_debugfs_dir(synx_dev);

	rc = synx_global_mem_init();
	if (rc) {
		dprintk(SYNX_ERR, "shared mem init failed, err=%d\n", rc);
		goto err;
	}

	synx_dev->cdsp_ssr.ssrcnt = 0;
	synx_dev->cdsp_ssr.nb.notifier_call = synx_cdsp_restart_notifier;
	synx_dev->cdsp_ssr.handle =
		qcom_register_ssr_notifier("cdsp", &synx_dev->cdsp_ssr.nb);
	if (synx_dev->cdsp_ssr.handle == NULL) {
		dprintk(SYNX_ERR, "SSR registration failed\n");
		goto err;
	}

	ipclite_register_client(synx_ipc_callback, NULL);
	synx_local_mem_init();

	dprintk(SYNX_INFO, "device initialization success\n");

	return 0;

err:
	vfree(synx_dev->native);
fail:
	device_destroy(synx_dev->class, synx_dev->dev);
	class_destroy(synx_dev->class);
reg_fail:
	unregister_chrdev_region(synx_dev->dev, 1);
alloc_fail:
	kfree(synx_dev);
	synx_dev = NULL;
	return rc;
}

static void __exit synx_exit(void)
{
	struct error_node *err_node, *err_node_tmp;

	flush_workqueue(synx_dev->wq_cb);
	flush_workqueue(synx_dev->wq_cleanup);
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
	mutex_destroy(&synx_dev->vtbl_lock);
	mutex_destroy(&synx_dev->error_lock);
	vfree(synx_dev->native);
	kfree(synx_dev);
}

module_init(synx_init);
module_exit(synx_exit);

MODULE_DESCRIPTION("Global Synx Driver");
MODULE_LICENSE("GPL v2");
