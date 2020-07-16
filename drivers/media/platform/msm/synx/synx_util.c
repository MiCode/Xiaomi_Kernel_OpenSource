// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */
#define pr_fmt(fmt) "synx: " fmt

#include <linux/slab.h>
#include <linux/random.h>
#include <linux/vmalloc.h>

#include "synx_api.h"
#include "synx_util.h"

extern void synx_external_callback(s32 sync_obj, int status, void *data);

bool synx_util_is_valid_bind_type(u32 type)
{
	if (type < SYNX_MAX_BIND_TYPES)
		return true;

	return false;
}

u32 synx_util_get_object_type(struct synx_coredata *synx_obj)
{
	if (!synx_obj)
		return 0;

	return synx_obj->type;
}

bool synx_util_is_merged_object(struct synx_coredata *synx_obj)
{
	if (synx_util_get_object_type(synx_obj)
		& SYNX_OBJ_TYPE_MERGED)
		return true;
	return false;
}

struct dma_fence *synx_util_get_fence(struct synx_coredata *synx_obj)
{
	if (!synx_obj)
		return NULL;

	if (synx_util_is_merged_object(synx_obj))
		return synx_obj->merged_fence;

	return &synx_obj->fence;
}

void synx_util_get_object(struct synx_coredata *synx_obj)
{
	struct dma_fence *fence;

	if (!synx_obj)
		return;

	fence = synx_util_get_fence(synx_obj);
	dma_fence_get(fence);
}

void synx_util_put_object(struct synx_coredata *synx_obj)
{
	struct dma_fence *fence;

	if (!synx_obj)
		return;

	fence = synx_util_get_fence(synx_obj);
	dma_fence_put(fence);
}

int synx_util_init_coredata(struct synx_coredata *synx_obj,
	struct synx_create_params *params,
	struct dma_fence_ops *ops)
{
	if (!synx_obj || !params || !ops) {
		pr_err("invalid arguments\n");
		return -EINVAL;
	}

	spin_lock_init(&synx_obj->lock);
	dma_fence_init(&synx_obj->fence, ops,
		&synx_obj->lock, synx_dev->dma_context, 1);

	synx_obj->type = SYNX_OBJ_TYPE_LOCAL;
	INIT_LIST_HEAD(&synx_obj->reg_cbs_list);
	if (params->name)
		strlcpy(synx_obj->name, params->name, sizeof(synx_obj->name));

	return 0;
}

int synx_util_init_group_coredata(struct synx_coredata *synx_obj,
	struct dma_fence **fences,
	u32 num_objs)
{
	struct dma_fence_array *array;

	if (!synx_obj)
		return -EINVAL;

	array = dma_fence_array_create(num_objs, fences,
				synx_dev->dma_context, 1, false);
	if (!array)
		return -EINVAL;

	synx_obj->merged_fence = &array->base;
	synx_obj->type = SYNX_OBJ_TYPE_LOCAL | SYNX_OBJ_TYPE_MERGED;
	INIT_LIST_HEAD(&synx_obj->reg_cbs_list);

	return 0;
}

void synx_util_object_destroy(struct synx_coredata *synx_obj)
{
	int rc;
	u32 i;
	s32 sync_id;
	u32 type;
	struct synx_cb_data *synx_cb, *synx_cb_temp;
	struct synx_bind_desc *bind_desc;
	struct bind_operations *bind_ops;
	struct synx_external_data *data;

	/* clear all the undispatched callbacks */
	list_for_each_entry_safe(synx_cb,
		synx_cb_temp, &synx_obj->reg_cbs_list, node) {
		pr_err("[sess: %u] cleaning up callback\n",
			synx_cb->session_id.client_id);
		list_del_init(&synx_cb->node);
		kfree(synx_cb);
	}

	for (i = 0; i < synx_obj->num_bound_synxs; i++) {
		bind_desc = &synx_obj->bound_synxs[i];
		sync_id = bind_desc->external_desc.id[0];
		type = bind_desc->external_desc.type;
		data = bind_desc->external_data;
		bind_ops = synx_util_get_bind_ops(type);
		rc = bind_ops->deregister_callback(
				synx_external_callback, data, sync_id);
		if (rc < 0) {
			pr_err("de-registration fail id: %d, type: %u, err: %d\n",
				sync_id, type, rc);
			continue;
		}

		/*
		 * release the memory allocated for external data.
		 * It is safe to release this memory
		 * only if deregistration is successful.
		 */
		kfree(data);
	}

	kfree(synx_obj);
	pr_debug("cleaned up synx object fence\n");
}

int synx_util_init_handle(struct synx_client *client,
	struct synx_coredata *synx_obj,
	long *h_synx)
{
	bool bit;
	long idx;
	struct synx_handle_coredata *synx_data;

	if (!client || !synx_obj)
		return -EINVAL;

	do {
		idx = find_first_zero_bit(client->bitmap, SYNX_MAX_OBJS);
		if (idx >= SYNX_MAX_OBJS) {
			pr_err("[sess: %u] free index not available\n",
				client->id);
			return -ENOMEM;
		}
		bit = test_and_set_bit(idx, client->bitmap);
	} while (bit);

	synx_data = &client->synx_table[idx];
	synx_data->client = client;
	synx_data->id = idx;
	synx_data->synx_obj = synx_obj;
	kref_init(&synx_data->internal_refcount);

	*h_synx = idx;
	return 0;
}

int synx_util_activate(struct synx_coredata *synx_obj)
{
	struct dma_fence *fence;

	if (!synx_obj)
		return -EINVAL;

	fence = synx_util_get_fence(synx_obj);
	/* move synx to ACTIVE state and register cb for merged object */
	dma_fence_enable_sw_signaling(fence);
	return 0;
}

static u32 synx_util_get_references(struct synx_coredata *synx_obj)
{
	u32 count = 0;
	u32 i = 0;
	struct dma_fence *fence;
	struct dma_fence_array *array = NULL;

	fence = synx_util_get_fence(synx_obj);
	/* obtain dma fence reference */
	if (dma_fence_is_array(fence)) {
		array = to_dma_fence_array(fence);
		if (!array)
			return 0;

		for (i = 0; i < array->num_fences; i++)
			dma_fence_get(array->fences[i]);

		count = array->num_fences;
	} else {
		dma_fence_get(fence);
		count = 1;
	}

	return count;
}

static void synx_util_put_references(struct synx_coredata *synx_obj)
{
	u32 i = 0;
	struct dma_fence *fence;
	struct dma_fence_array *array = NULL;

	fence = synx_util_get_fence(synx_obj);
	if (dma_fence_is_array(fence)) {
		array = to_dma_fence_array(fence);
		if (!array)
			return;

		for (i = 0; i < array->num_fences; i++)
			dma_fence_put(array->fences[i]);
	} else {
		dma_fence_put(fence);
	}
}

static u32 synx_util_add_fence(struct synx_coredata *synx_obj,
	struct dma_fence **fences,
	u32 idx)
{
	struct dma_fence *fence;
	struct dma_fence_array *array = NULL;
	u32 i = 0;

	fence = synx_util_get_fence(synx_obj);
	if (dma_fence_is_array(fence)) {
		array = to_dma_fence_array(fence);
		if (!array)
			return 0;

		for (i = 0; i < array->num_fences; i++)
			fences[idx+i] = array->fences[i];

		return array->num_fences;
	}

	fences[idx] = fence;
	return 1;
}

static u32 synx_util_remove_duplicates(struct dma_fence **arr, u32 num)
{
	int i, j;
	u32 wr_idx = 1;

	if (!arr) {
		pr_err("invalid input array\n");
		return 0;
	}

	for (i = 1; i < num; i++) {
		for (j = 0; j < wr_idx ; j++) {
			if (arr[i] == arr[j]) {
				/* release reference obtained for duplicate */
				pr_debug("releasing duplicate reference\n");
				dma_fence_put(arr[i]);
				break;
			}
		}
		if (j == wr_idx)
			arr[wr_idx++] = arr[i];
	}

	return wr_idx;
}

s32 synx_util_merge_error(struct synx_client *client,
	s32 *h_synxs,
	u32 num_objs)
{
	u32 i = 0;
	struct synx_coredata *synx_obj;

	if (!client || !h_synxs)
		return -EINVAL;

	for (i = 0; i < num_objs; i++) {
		synx_obj = synx_util_acquire_object(client, h_synxs[i]);
		if (!synx_obj) {
			pr_err("[sess: %u] invalid handle %d in merge cleanup\n",
				client->id, h_synxs[i]);
			continue;
		}
		/* release all references obtained during merge validatation */
		synx_util_put_references(synx_obj);
		synx_util_release_object(client, h_synxs[i]);
	}

	return 0;
}

int synx_util_validate_merge(struct synx_client *client,
	s32 *h_synxs,
	u32 num_objs,
	struct dma_fence ***fence_list,
	u32 *fence_cnt)
{
	u32 count = 0;
	u32 i = 0;
	struct synx_coredata **synx_objs = NULL;
	struct dma_fence **fences = NULL;

	if (num_objs <= 1) {
		pr_err("single object merge is not allowed\n");
		return -EINVAL;
	}

	synx_objs = kcalloc(num_objs, sizeof(*synx_objs), GFP_KERNEL);
	if (!synx_objs)
		return -ENOMEM;

	for (i = 0; i < num_objs; i++) {
		synx_objs[i] = synx_util_acquire_object(client, h_synxs[i]);
		if (!synx_objs) {
			pr_err("[sess: %u] invalid handle %d in merge list\n",
				client->id, h_synxs[i]);
			*fence_cnt = i;
			return -EINVAL;
		}
		count += synx_util_get_references(synx_objs[i]);
	}

	fences = kcalloc(count, sizeof(*fences), GFP_KERNEL);
	if (!fences) {
		*fence_cnt = num_objs;
		return -ENOMEM;
	}

	/* memory will be released later in the invoking function */
	*fence_list = fences;
	count = 0;

	for (i = 0; i < num_objs; i++) {
		count += synx_util_add_fence(synx_objs[i], fences, count);
		/* release the reference obtained earlier in the function */
		synx_util_release_object(client, h_synxs[i]);
	}

	*fence_cnt = synx_util_remove_duplicates(fences, count);
	kfree(synx_objs);
	return 0;
}

static u32 __fence_state(struct dma_fence *fence, bool locked)
{
	s32 status;
	u32 state = SYNX_STATE_INVALID;

	if (!fence) {
		pr_err("invalid dma fence addr\n");
		return SYNX_STATE_INVALID;
	}

	if (locked)
		status = dma_fence_get_status_locked(fence);
	else
		status = dma_fence_get_status(fence);

	/* convert fence status to synx state */
	switch (status) {
	case 0:
		state = SYNX_STATE_ACTIVE;
		break;
	case 1:
		state = SYNX_STATE_SIGNALED_SUCCESS;
		break;
	case -SYNX_STATE_SIGNALED_CANCEL:
		state = SYNX_STATE_SIGNALED_CANCEL;
		break;
	case -SYNX_STATE_FORCED_RELEASE:
		state = SYNX_STATE_FORCED_RELEASE;
		break;
	case -SYNX_STATE_SIGNALED_ERROR:
	default:
		state = SYNX_STATE_SIGNALED_ERROR;
	}

	return state;
}

static u32 __fence_group_state(struct dma_fence *fence, bool locked)
{
	u32 i = 0;
	u32 state = SYNX_STATE_INVALID;
	struct dma_fence_array *array = NULL;
	u32 intr, actv_cnt, sig_cnt, err_cnt;

	if (!fence) {
		pr_err("invalid dma fence addr\n");
		return SYNX_STATE_INVALID;
	}

	actv_cnt = sig_cnt = err_cnt = 0;
	array = to_dma_fence_array(fence);
	if (!array)
		return SYNX_STATE_INVALID;

	for (i = 0; i < array->num_fences; i++) {
		intr = __fence_state(array->fences[i], locked);
		switch (intr) {
		case SYNX_STATE_ACTIVE:
			actv_cnt++;
			break;
		case SYNX_STATE_SIGNALED_SUCCESS:
			sig_cnt++;
			break;
		default:
			err_cnt++;
		}
	}

	pr_debug("group cnt stats act:%u, sig: %u, err: %u\n",
		actv_cnt, sig_cnt, err_cnt);

	if (err_cnt)
		state = SYNX_STATE_SIGNALED_ERROR;
	else if (actv_cnt)
		state = SYNX_STATE_ACTIVE;
	else if (sig_cnt == array->num_fences)
		state = SYNX_STATE_SIGNALED_SUCCESS;

	return state;
}

/*
 * WARN: Should not hold the synx spinlock when invoking
 * this function. Use synx_fence_state_locked instead
 */
u32 synx_util_get_object_status(struct synx_coredata *synx_obj)
{
	u32 state;
	struct dma_fence *fence;

	if (!synx_obj)
		return SYNX_STATE_INVALID;

	fence = synx_util_get_fence(synx_obj);
	if (synx_util_is_merged_object(synx_obj))
		state = __fence_group_state(fence, false);
	else
		state = __fence_state(fence, false);

	return state;
}

/* use this for status check when holding on to metadata spinlock */
u32 synx_util_get_object_status_locked(struct synx_coredata *synx_obj)
{
	u32 state;
	struct dma_fence *fence;

	if (!synx_obj)
		return SYNX_STATE_INVALID;

	fence = synx_util_get_fence(synx_obj);
	if (synx_util_is_merged_object(synx_obj))
		state = __fence_group_state(fence, true);
	else
		state = __fence_state(fence, true);

	return state;
}

struct synx_handle_coredata *synx_util_obtain_handle(
	struct synx_client *client,
	s32 h_synx)
{
	if (!client) {
		pr_err("invalid session argument\n");
		return NULL;
	}

	if (h_synx < 0 || h_synx >= SYNX_MAX_OBJS) {
		pr_err("[sess: %u] invalid handle %d access\n",
			client->id, h_synx);
		return NULL;
	}

	return &client->synx_table[h_synx];
}

struct synx_coredata *synx_util_acquire_object(
	struct synx_client *client, s32 h_synx)
{
	struct synx_coredata *synx_obj = NULL;
	struct synx_handle_coredata *synx_data =
		synx_util_obtain_handle(client, h_synx);

	if (!synx_data)
		return NULL;

	mutex_lock(&client->synx_table_lock[h_synx]);
	synx_obj = synx_data->synx_obj;
	if (synx_obj) {
		synx_util_get_object(synx_obj);
		kref_get(&synx_data->internal_refcount);
		pr_debug("[sess: %u] acquired synx object for handle %d\n",
			client->id, h_synx);
	}
	mutex_unlock(&client->synx_table_lock[h_synx]);

	return synx_obj;
}

static void synx_util_destroy_handle(struct synx_handle_coredata *synx_data)
{
	long idx = synx_data->id;
	struct synx_client *client = synx_data->client;

	if (synx_util_is_merged_object(synx_data->synx_obj)) {
		pr_debug("cleaned up synx object fence\n");
		kfree(synx_data->synx_obj);
	}

	memset(synx_data, 0, sizeof(*synx_data));
	clear_bit(idx, client->bitmap);
	pr_debug("[sess: %u] handle %d destroyed\n",
		client->id, idx);
}

void synx_util_destroy_import_handle(struct kref *kref)
{
	struct synx_handle_coredata *synx_data =
		container_of(kref, struct synx_handle_coredata,
		import_refcount);

	pr_debug("[sess: %u] import handle cleanup for %d\n",
		synx_data->client->id, synx_data->id);

	/* in case of pending internal references, abort clean up */
	if (kref_read(&synx_data->internal_refcount))
		return;

	synx_util_destroy_handle(synx_data);
}

void synx_util_destroy_internal_handle(struct kref *kref)
{
	struct synx_handle_coredata *synx_data =
		container_of(kref, struct synx_handle_coredata,
		internal_refcount);

	pr_debug("[sess: %u] internal handle cleanup for %d\n",
		synx_data->client->id, synx_data->id);

	/* in case of pending imports, abort clean up */
	if (kref_read(&synx_data->import_refcount))
		return;

	synx_util_destroy_handle(synx_data);
}

void synx_util_release_object(struct synx_client *client, s32 h_synx)
{
	struct synx_handle_coredata *synx_data =
		synx_util_obtain_handle(client, h_synx);
	struct synx_coredata *synx_obj;
	struct dma_fence *fence;

	if (!synx_data)
		return;

	mutex_lock(&client->synx_table_lock[h_synx]);
	synx_obj = synx_data->synx_obj;
	if (synx_obj) {
		fence = synx_util_get_fence(synx_obj);
		kref_put(&synx_data->internal_refcount,
			synx_util_destroy_internal_handle);
		dma_fence_put(fence);
	}
	mutex_unlock(&client->synx_table_lock[h_synx]);
}

struct bind_operations *synx_util_get_bind_ops(u32 type)
{
	struct synx_registered_ops *client_ops;

	if (!synx_util_is_valid_bind_type(type))
		return NULL;

	mutex_lock(&synx_dev->vtbl_lock);
	client_ops = &synx_dev->bind_vtbl[type];
	if (!client_ops->valid) {
		mutex_unlock(&synx_dev->vtbl_lock);
		return NULL;
	}
	mutex_unlock(&synx_dev->vtbl_lock);

	return &client_ops->ops;
}

int synx_util_alloc_cb_entry(struct synx_client *client,
	struct synx_kernel_payload *data,
	u32 *cb_idx)
{
	bool bit;
	long idx;
	struct synx_client_cb *cb;

	if (!client || !data || !cb_idx)
		return -EINVAL;

	do {
		idx = find_first_zero_bit(client->cb_bitmap, SYNX_MAX_OBJS);
		if (idx >= SYNX_MAX_OBJS) {
			pr_err("[sess: %u] free cb index not available\n",
				client->id);
			return -ENOMEM;
		}
		bit = test_and_set_bit(idx, client->cb_bitmap);
	} while (bit);

	cb = &client->cb_table[idx];
	memset(cb, 0, sizeof(*cb));
	cb->is_valid = true;
	cb->client = client;
	cb->idx = idx;
	memcpy(&cb->kernel_cb, data,
		sizeof(cb->kernel_cb));

	*cb_idx = idx;
	pr_debug("[sess: %u] allocated cb index %u\n", client->id, *cb_idx);
	return 0;
}

int synx_util_clear_cb_entry(struct synx_client *client,
	struct synx_client_cb *cb)
{
	int rc = 0;
	u32 idx;

	if (!cb)
		return -EINVAL;

	idx = cb->idx;
	memset(cb, 0, sizeof(*cb));
	if (idx && idx < SYNX_MAX_OBJS) {
		clear_bit(idx, client->cb_bitmap);
	} else {
		pr_err("%s: found invalid index\n", __func__);
		rc = -EINVAL;
	}

	return rc;
}

void synx_util_default_user_callback(s32 h_synx,
	int status, void *data)
{
	struct synx_client_cb *cb = data;

	if (cb && cb->client) {
		pr_debug("user cb queued for handle %d\n", h_synx);
		cb->kernel_cb.status = status;
		mutex_lock(&cb->client->event_q_lock);
		list_add_tail(&cb->node, &cb->client->event_q);
		mutex_unlock(&cb->client->event_q_lock);
		wake_up_all(&cb->client->event_wq);
	} else {
		pr_err("%s: invalid params\n", __func__);
	}
}

void synx_util_callback_dispatch(struct synx_coredata *synx_obj, u32 status)
{
	struct synx_cb_data *synx_cb, *synx_cb_temp;

	if (!synx_obj) {
		pr_err("invalid arguments\n");
		return;
	}

	list_for_each_entry_safe(synx_cb,
		synx_cb_temp, &synx_obj->reg_cbs_list, node) {
		synx_cb->status = status;
		list_del_init(&synx_cb->node);
		queue_work(synx_dev->work_queue,
			&synx_cb->cb_dispatch);
		pr_debug("dispatched callback\n");
	}
}

void synx_util_cb_dispatch(struct work_struct *cb_dispatch)
{
	struct synx_cb_data *synx_cb =
		container_of(cb_dispatch, struct synx_cb_data, cb_dispatch);
	struct synx_client *client;
	struct synx_client_cb *cb;
	struct synx_kernel_payload payload;
	u32 status;

	client = synx_get_client(synx_cb->session_id);
	if (!client) {
		pr_err("invalid session data %u in cb payload\n",
			synx_cb->session_id);
		goto free;
	}

	if (synx_cb->idx == 0 ||
		synx_cb->idx >= SYNX_MAX_OBJS) {
		pr_err("[sess: %u] invalid cb index %u\n",
			client->id, synx_cb->idx);
		goto fail;
	}

	status = synx_cb->status;
	cb = &client->cb_table[synx_cb->idx];
	if (!cb->is_valid) {
		pr_err("invalid cb payload\n");
		goto fail;
	}

	memcpy(&payload, &cb->kernel_cb, sizeof(cb->kernel_cb));
	payload.status = status;

	if (payload.cb_func == synx_util_default_user_callback) {
		/*
		 * need to send client cb data for default
		 * user cb (userspace cb)
		 */
		payload.data = cb;
	} else {
		/*
		 * clear the cb entry. userspace cb entry
		 * will be cleared after data read by the
		 * polling thread or when client is destroyed
		 */
		if (synx_util_clear_cb_entry(client, cb))
			pr_err("%s: [sess: %u] error clearing cb entry\n",
				__func__, client->id);
	}

	pr_debug("[sess: %u] kernel cb dispatch for handle %d\n",
		client->id, payload.h_synx);

	/* dispatch kernel callback */
	payload.cb_func(payload.h_synx,
		payload.status, payload.data);

fail:
	synx_put_client(client);
free:
	pr_debug("released synx_cb_data memory\n");
	kfree(synx_cb);
}

struct synx_coredata *synx_util_import_object(struct synx_import_params *params)
{
	u16 key;
	s32 h_synx;
	u32 secure_key;
	struct synx_session ex_session_id;
	struct synx_client *ex_client;
	struct synx_handle_coredata *synx_data;
	struct synx_coredata *synx_obj = NULL;

	if (!params)
		return NULL;

	h_synx = params->h_synx;
	secure_key = params->secure_key;

	ex_session_id.client_id = secure_key & SYNX_CLIENT_HANDLE_MASK;
	key = (secure_key >> SYNX_CLIENT_HANDLE_SHIFT) & SYNX_SECURE_KEY_MASK;

	/* get the client exporting the synx handle */
	ex_client = synx_get_client(ex_session_id);
	if (!ex_client) {
		pr_err("sess: %u invalid import handle %d and/or key %u\n",
			ex_session_id.client_id, h_synx, secure_key);
		return NULL;
	}

	synx_data = synx_util_obtain_handle(ex_client, h_synx);
	if (!synx_data) {
		pr_err("[sess: %u] invalid import handle %d\n",
			ex_client->id, h_synx);
		goto fail;
	}

	mutex_lock(&ex_client->synx_table_lock[h_synx]);
	/* need to check whether import is accounted for in import_refcount */
	if (synx_data->synx_obj &&
		key && (key == synx_data->key) &&
		kref_read(&synx_data->import_refcount)) {
		if (synx_util_is_merged_object(synx_data->synx_obj)) {
			/*
			 * need to copy the synx coredata for merged object
			 * as we need to clean up the coredata on release
			 * since fence array cannot be registered with
			 * synx_fence_release function.
			 */
			synx_obj = kzalloc(sizeof(*synx_obj), GFP_KERNEL);
			if (synx_obj)
				memcpy(synx_obj, synx_data->synx_obj,
					sizeof(*synx_obj));
		} else {
			synx_obj = synx_data->synx_obj;
		}
		/* release the reference obtained during export */
		kref_put(&synx_data->import_refcount,
			synx_util_destroy_import_handle);
		pr_debug("sess: %u handle %d import successful\n",
			ex_client->id, h_synx);
	}
	mutex_unlock(&ex_client->synx_table_lock[h_synx]);

fail:
	synx_put_client(ex_client);
	return synx_obj;
}

int synx_util_export_object(struct synx_client *client,
	struct synx_export_params *params)
{
	int rc = 0;
	u16 key = 0;
	s32 h_synx;
	struct synx_handle_coredata *synx_data;

	if (!params || !params->secure_key)
		return -EINVAL;

	h_synx = params->h_synx;
	synx_data = synx_util_obtain_handle(client, h_synx);
	if (!synx_data) {
		pr_err("[sess: %u] invalid export handle %d\n",
			client->id, h_synx);
		rc = -EINVAL;
		goto fail;
	}

	mutex_lock(&client->synx_table_lock[h_synx]);
	/* need to check whether import is accounted for in import_refcount */
	if (synx_data->synx_obj) {
		/* generate the key on the first export of the handle */
		if (!synx_data->key) {
			kref_init(&synx_data->import_refcount);
			while (!key)
				get_random_bytes(&key, sizeof(key));
			synx_data->key = key;
			pr_debug("[sess: %u] import refcount initialized\n",
				client->id);
		} else {
			kref_get(&synx_data->import_refcount);
			key = synx_data->key;
		}
		/*
		 * to make sure the synx is not lost if the process
		 * dies or synx is released before any other process
		 * gets a chance to import it.
		 * The assumption is that an import will match this
		 * and account for the extra reference. Otherwise,
		 * this will be released upon client uninit.
		 */
		synx_util_get_object(synx_data->synx_obj);

		/* encode the key and the client id to user provided variable */
		*params->secure_key = key;
		*params->secure_key <<= SYNX_CLIENT_HANDLE_SHIFT;
		*params->secure_key |= client->id;

		pr_debug("[sess: %u] handle %d export successful with key %u\n",
			client->id, params->h_synx, *params->secure_key);
	} else {
		pr_err("[sess: %u] invalid export object %d\n",
			client->id, h_synx);
		rc = -EINVAL;
	}
	mutex_unlock(&client->synx_table_lock[h_synx]);

fail:
	return rc;
}

struct synx_client *synx_get_client(struct synx_session session_id)
{
	struct synx_client_metadata *client_metadata;
	struct synx_client *client;
	u32 id = session_id.client_id;

	if (id >= SYNX_MAX_CLIENTS) {
		pr_err("%s: invalid session handle %u from pid: %d\n",
			__func__, id, current->pid);
		return NULL;
	}

	mutex_lock(&synx_dev->dev_table_lock);
	client_metadata = &synx_dev->client_table[id];
	client = client_metadata->client;
	if (client)
		kref_get(&client_metadata->refcount);
	else
		pr_err("session %u not available, pid: %d\n",
			id, current->pid);
	mutex_unlock(&synx_dev->dev_table_lock);

	return client;
}

static void synx_client_destroy(struct kref *kref)
{
	u32 i;
	struct synx_client_metadata *client_metadata =
		container_of(kref, struct synx_client_metadata, refcount);
	struct synx_client *client = client_metadata->client;
	struct synx_handle_coredata *synx_data;
	struct synx_coredata *synx_obj;
	struct dma_fence *fence;

	/* TODO:can create a thread and handle it */

	/* go over all the remaining synx obj handles and clear them */
	for (i = 0; i < SYNX_MAX_OBJS; i++) {
		synx_data = &client->synx_table[i];
		synx_obj = synx_data->synx_obj;
		/*
		 * cleanup unreleased references by the client
		 * Note: it is only safe to access synx_obj if
		 * there are refcounts (internal, import)
		 * remaining in the current handle, as it
		 * gurantees corresponding reference to fence.
		 */
		if (synx_obj) {
			fence = synx_util_get_fence(synx_obj);
			while (kref_read(&synx_data->internal_refcount)) {
				kref_put(&synx_data->internal_refcount,
					synx_util_destroy_internal_handle);
				dma_fence_put(fence);
			}
			while (kref_read(&synx_data->import_refcount)) {
				kref_put(&synx_data->import_refcount,
					synx_util_destroy_import_handle);
				dma_fence_put(fence);
			}
		}
		mutex_destroy(&client->synx_table_lock[i]);
	}
	mutex_destroy(&client->event_q_lock);
	memset(client_metadata, 0, sizeof(*client_metadata));
	clear_bit(client->id, synx_dev->bitmap);

	pr_info("[sess: %u] session destroyed %s\n",
		client->id, client->name);
	vfree(client);
}

void synx_put_client(struct synx_client *client)
{
	struct synx_client_metadata *client_metadata;

	if (!client) {
		pr_err("%s: invalid client ptr\n", __func__);
		return;
	}

	if (client->id >= SYNX_MAX_CLIENTS) {
		pr_err("%s: session id %u invalid from pid: %d\n",
			__func__, client->id, current->pid);
		return;
	}

	mutex_lock(&synx_dev->dev_table_lock);
	client_metadata = &synx_dev->client_table[client->id];
	if (client_metadata->client == client)
		/* should not reference client after this call */
		kref_put(&client_metadata->refcount, synx_client_destroy);
	else
		pr_err("%s: invalid session %u from pid: %d\n",
			__func__, client->id, current->pid);
	mutex_unlock(&synx_dev->dev_table_lock);
}

void synx_util_generate_timestamp(char *timestamp, size_t size)
{
	struct timespec64 tv;
	struct tm tm;

	ktime_get_real_ts64(&tv);
	time64_to_tm(tv.tv_sec, 0, &tm);
	snprintf(timestamp, size, "%02d-%02d %02d:%02d:%02d",
		tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
		tm.tm_min, tm.tm_sec);
}

void synx_util_log_error(u32 client_id, s32 h_synx, s32 err)
{
	struct error_node *err_node;

	if (!synx_dev->debugfs_root)
		return;

	err_node = kzalloc(sizeof(*err_node), GFP_KERNEL);
	if (!err_node)
		return;

	err_node->client_id = client_id;
	err_node->error_code = err;
	err_node->h_synx = h_synx;
	synx_util_generate_timestamp(err_node->timestamp,
		sizeof(err_node->timestamp));
	mutex_lock(&synx_dev->error_lock);
	list_add(&err_node->node,
		&synx_dev->error_list);
	mutex_unlock(&synx_dev->error_lock);
}
