// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */
#define pr_fmt(fmt) "synx: " fmt

#include <linux/slab.h>
#include <linux/random.h>
#include <linux/vmalloc.h>

#include "synx_api.h"
#include "synx_util.h"

static DECLARE_HASHTABLE(synx_global_key_tbl, 8);
static DECLARE_HASHTABLE(synx_camera_id_tbl, 8);

spinlock_t camera_tbl_lock;
spinlock_t global_tbl_lock;

extern void synx_external_callback(s32 sync_obj, int status, void *data);

int synx_util_init_coredata(struct synx_coredata *synx_obj,
	struct synx_create_params *params,
	struct dma_fence_ops *ops)
{
	spinlock_t *fence_lock;
	struct dma_fence *fence;

	if (!synx_obj || !params || !ops ||
		params->type >= SYNX_FLAG_MAX) {
		pr_err("%s: invalid arguments\n", __func__);
		return -EINVAL;
	}

	synx_obj->type = params->type;
	synx_obj->num_bound_synxs = 0;
	kref_init(&synx_obj->refcount);
	mutex_init(&synx_obj->obj_lock);
	INIT_LIST_HEAD(&synx_obj->reg_cbs_list);
	if (params->name)
		strlcpy(synx_obj->name, params->name, sizeof(synx_obj->name));

	if (!synx_util_is_external_object(synx_obj)) {
		/*
		 * lock and fence memory will be released in fence
		 * release function
		 */
		fence_lock = kzalloc(sizeof(*fence_lock), GFP_KERNEL);
		if (!fence_lock)
			return -ENOMEM;

		fence = kzalloc(sizeof(*fence), GFP_KERNEL);
		if (!fence) {
			kfree(fence_lock);
			return -ENOMEM;
		}

		spin_lock_init(fence_lock);
		dma_fence_init(fence, ops, fence_lock,
			synx_dev->dma_context, 1);

		/*
		 * adding callback enables the fence to be
		 * shared with clients, who can signal fence
		 * through dma signaling functions, and still
		 * get notified to update the synx coredata.
		 */
		if (dma_fence_add_callback(fence,
			&synx_obj->fence_cb, synx_fence_callback)) {
			pr_err("error adding fence callback for %pK\n",
				fence);
			dma_fence_put(fence);
			return -EINVAL;
		}

		synx_obj->fence = fence;
		pr_debug("allocated synx backing fence %pK\n", fence);
	}

	synx_util_activate(synx_obj);
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

	synx_obj->fence = &array->base;
	synx_obj->type = SYNX_FLAG_MERGED_FENCE;
	synx_obj->num_bound_synxs = 0;
	kref_init(&synx_obj->refcount);
	mutex_init(&synx_obj->obj_lock);
	INIT_LIST_HEAD(&synx_obj->reg_cbs_list);

	synx_util_activate(synx_obj);
	return 0;
}

static void synx_util_destroy_coredata(struct kref *kref)
{
	struct synx_coredata *synx_obj =
		container_of(kref, struct synx_coredata, refcount);

	if (synx_obj->fence) {
		/* need to release callback if unsignaled */
		if (!synx_util_is_merged_object(synx_obj) &&
			(synx_util_get_object_status(synx_obj) ==
			SYNX_STATE_ACTIVE))
			if (!dma_fence_remove_callback(synx_obj->fence,
				&synx_obj->fence_cb))
				/* nothing much but logging the error */
				pr_err("synx callback could not be removed %pK\n",
					synx_obj->fence);
		dma_fence_put(synx_obj->fence);
	}

	synx_util_object_destroy(synx_obj);
}

void synx_util_get_object(struct synx_coredata *synx_obj)
{
	kref_get(&synx_obj->refcount);
}

void synx_util_put_object(struct synx_coredata *synx_obj)
{
	kref_put(&synx_obj->refcount, synx_util_destroy_coredata);
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
	struct hash_key_data *entry = NULL;

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
		if (!bind_ops) {
			pr_err("bind ops fail id: %d, type: %u\n",
				sync_id, type);
			continue;
		}

		/* clear the hash table entry */
		entry = synx_util_retrieve_data(sync_id, type);
		if (entry && type == SYNX_TYPE_CSL) {
			spin_lock_bh(&camera_tbl_lock);
			hash_del(&entry->node);
			spin_unlock_bh(&camera_tbl_lock);
			kfree(entry);
		}

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

	mutex_destroy(&synx_obj->obj_lock);
	kfree(synx_obj);
	pr_debug("released synx object %pK\n", synx_obj);
}

long synx_util_get_free_handle(unsigned long *bitmap, unsigned int size)
{
	bool bit;
	long idx;

	do {
		idx = find_first_zero_bit(bitmap, size);
		if (idx >= size)
			break;
		bit = test_and_set_bit(idx, bitmap);
	} while (bit);

	return idx;
}

int synx_util_init_handle(struct synx_client *client,
	struct synx_coredata *synx_obj,
	long *new_synx)
{
	long idx = 0;
	s32 h_synx = 0;
	u8 unique_id;
	struct synx_handle_coredata *synx_data;

	if (!client || !synx_obj)
		return -EINVAL;

	h_synx = (client->id & SYNX_CLIENT_ENCODE_MASK);
	h_synx <<= SYNX_OBJ_HANDLE_SHIFT;
	idx = synx_util_get_free_handle(client->bitmap, SYNX_MAX_OBJS);
	if (idx >= SYNX_MAX_OBJS)
		return -ENOMEM;
	do {
		get_random_bytes(&unique_id, sizeof(unique_id));
	} while (!unique_id);
	h_synx |= unique_id;
	h_synx <<= SYNX_OBJ_HANDLE_SHIFT;
	h_synx |= (idx & SYNX_OBJ_HANDLE_MASK);

	mutex_lock(&client->synx_table_lock[idx]);
	synx_data = &client->synx_table[idx];
	memset(synx_data, 0, sizeof(*synx_data));
	synx_data->client = client;
	synx_data->handle = h_synx;
	synx_data->synx_obj = synx_obj;
	kref_init(&synx_data->internal_refcount);
	mutex_unlock(&client->synx_table_lock[idx]);

	*new_synx = h_synx;
	return 0;
}

int synx_util_activate(struct synx_coredata *synx_obj)
{
	if (!synx_obj)
		return -EINVAL;

	/* external fence activation is managed by client */
	if (synx_util_is_external_object(synx_obj))
		return 0;

	/* move synx to ACTIVE state and register cb for merged object */
	dma_fence_enable_sw_signaling(synx_obj->fence);
	return 0;
}

static u32 synx_util_get_references(struct synx_coredata *synx_obj)
{
	u32 count = 0;
	u32 i = 0;
	struct dma_fence_array *array = NULL;

	/* obtain dma fence reference */
	if (dma_fence_is_array(synx_obj->fence)) {
		array = to_dma_fence_array(synx_obj->fence);
		if (!array)
			return 0;

		for (i = 0; i < array->num_fences; i++)
			dma_fence_get(array->fences[i]);
		count = array->num_fences;
	} else {
		dma_fence_get(synx_obj->fence);
		count = 1;
	}

	return count;
}

static void synx_util_put_references(struct synx_coredata *synx_obj)
{
	u32 i = 0;
	struct dma_fence_array *array = NULL;

	if (dma_fence_is_array(synx_obj->fence)) {
		array = to_dma_fence_array(synx_obj->fence);
		if (!array)
			return;

		for (i = 0; i < array->num_fences; i++)
			dma_fence_put(array->fences[i]);
	} else {
		dma_fence_put(synx_obj->fence);
	}
}

static u32 synx_util_add_fence(struct synx_coredata *synx_obj,
	struct dma_fence **fences,
	u32 idx)
{
	struct dma_fence_array *array = NULL;
	u32 i = 0;

	if (dma_fence_is_array(synx_obj->fence)) {
		array = to_dma_fence_array(synx_obj->fence);
		if (!array)
			return 0;

		for (i = 0; i < array->num_fences; i++)
			fences[idx+i] = array->fences[i];

		return array->num_fences;
	}

	fences[idx] = synx_obj->fence;
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
	struct synx_handle_coredata *synx_data;
	struct synx_coredata *synx_obj;

	if (!client || !h_synxs)
		return -EINVAL;

	for (i = 0; i < num_objs; i++) {
		synx_data = synx_util_acquire_handle(client, h_synxs[i]);
		synx_obj = synx_util_obtain_object(synx_data);
		if (!synx_obj || !synx_obj->fence) {
			pr_err("[sess: %u] invalid handle %d in merge cleanup\n",
				client->id, h_synxs[i]);
			continue;
		}
		/* release all references obtained during merge validatation */
		synx_util_put_references(synx_obj);
		synx_util_release_handle(synx_data);
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
	struct synx_handle_coredata **synx_datas;
	struct synx_coredata **synx_objs;
	struct dma_fence **fences = NULL;

	if (num_objs <= 1) {
		pr_err("single object merge is not allowed\n");
		return -EINVAL;
	}

	synx_datas = kcalloc(num_objs, sizeof(*synx_datas), GFP_KERNEL);
	if (!synx_datas)
		return -ENOMEM;

	synx_objs = kcalloc(num_objs, sizeof(*synx_objs), GFP_KERNEL);
	if (!synx_objs)
		return -ENOMEM;

	for (i = 0; i < num_objs; i++) {
		synx_datas[i] = synx_util_acquire_handle(client, h_synxs[i]);
		synx_objs[i] = synx_util_obtain_object(synx_datas[i]);
		if (!synx_objs[i] || !synx_objs[i]->fence) {
			pr_err("[sess: %u] invalid handle %d in merge list\n",
				client->id, h_synxs[i]);
			*fence_cnt = i;
			goto error;
		}
		count += synx_util_get_references(synx_objs[i]);
	}

	fences = kcalloc(count, sizeof(*fences), GFP_KERNEL);
	if (!fences) {
		*fence_cnt = num_objs;
		goto error;
	}

	/* memory will be released later in the invoking function */
	*fence_list = fences;
	count = 0;

	for (i = 0; i < num_objs; i++) {
		count += synx_util_add_fence(synx_objs[i], fences, count);
		/* release the reference obtained earlier in the function */
		synx_util_release_handle(synx_datas[i]);
	}

	*fence_cnt = synx_util_remove_duplicates(fences, count);
	kfree(synx_objs);
	kfree(synx_datas);
	return 0;

error:
	/* release the reference/s obtained earlier in the function */
	for (i = 0; i < *fence_cnt; i++) {
		synx_util_put_references(synx_objs[i]);
		synx_util_release_handle(synx_datas[i]);
	}
	*fence_cnt = 0;
	kfree(synx_objs);
	kfree(synx_datas);
	return -EINVAL;
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
	case -SYNX_STATE_SIGNALED_EXTERNAL:
		state = SYNX_STATE_SIGNALED_EXTERNAL;
		break;
	case -SYNX_STATE_SIGNALED_ERROR:
		state = SYNX_STATE_SIGNALED_ERROR;
		break;
	default:
		state = (u32)(-status);
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
 * WARN: Should not hold the fence spinlock when invoking
 * this function. Use synx_fence_state_locked instead
 */
u32 synx_util_get_object_status(struct synx_coredata *synx_obj)
{
	u32 state;

	if (!synx_obj)
		return SYNX_STATE_INVALID;

	if (synx_util_is_merged_object(synx_obj))
		state = __fence_group_state(synx_obj->fence, false);
	else
		state = __fence_state(synx_obj->fence, false);

	return state;
}

/* use this for status check when holding on to metadata spinlock */
u32 synx_util_get_object_status_locked(struct synx_coredata *synx_obj)
{
	u32 state;

	if (!synx_obj)
		return SYNX_STATE_INVALID;

	if (synx_util_is_merged_object(synx_obj))
		state = __fence_group_state(synx_obj->fence, true);
	else
		state = __fence_state(synx_obj->fence, true);

	return state;
}

struct synx_handle_coredata *synx_util_acquire_handle(
	struct synx_client *client, s32 h_synx)
{
	u32 idx = synx_util_handle_index(h_synx);
	struct synx_handle_coredata *synx_data = NULL;
	struct synx_handle_coredata *synx_handle = NULL;

	if (!client)
		return NULL;

	mutex_lock(&client->synx_table_lock[idx]);
	synx_data = &client->synx_table[idx];
	if (!synx_data->synx_obj) {
		pr_err("[sess: %u] invalid object handle %d\n",
			client->id, h_synx);
	} else if (synx_data->handle != h_synx) {
		pr_err("[sess: %u] stale object handle %d\n",
			client->id, h_synx);
	} else if (!kref_read(&synx_data->internal_refcount)) {
		pr_err("[sess: %u] destroyed object handle %d\n",
			client->id, h_synx);
	} else {
		kref_get(&synx_data->internal_refcount);
		synx_handle = synx_data;
	}
	mutex_unlock(&client->synx_table_lock[idx]);

	return synx_handle;
}

int synx_util_update_handle(struct synx_client *client,
	s32 h_synx, u32 sync_id, u32 type,
	struct synx_handle_coredata **synx_handle)
{
	int rc = 0;
	bool loop;
	u32 idx = synx_util_handle_index(h_synx);
	struct synx_handle_coredata *synx_data = NULL;
	struct hash_key_data *entry = NULL;

	if (!client || !synx_handle)
		return -EINVAL;

	do {
		loop = false;
		mutex_lock(&client->synx_table_lock[idx]);
		synx_data = &client->synx_table[idx];
		if (!synx_data->synx_obj) {
			pr_err("[sess: %u] invalid object handle %d\n",
				client->id, h_synx);
			rc = -EINVAL;
		} else if (synx_data->handle != h_synx) {
			pr_err("[sess: %u] stale object handle %d\n",
				client->id, h_synx);
			rc = -EINVAL;
		} else if (!kref_read(&synx_data->internal_refcount)) {
			pr_err("[sess: %u] destroyed object handle %d\n",
				client->id, h_synx);
			rc = -EINVAL;
		} else {
			if (kref_read(&synx_data->internal_refcount) == 1) {
				entry = synx_util_retrieve_data(sync_id, type);
				if (entry &&
					((struct synx_coredata *)entry->data
					!= synx_data->synx_obj)) {
					/*
					 * release existing coredata and replace.
					 * this ensures that all external fence ids
					 * are mapped to same coredata object, thus
					 * eliminating roundtrip delays on signaling
					 */
					synx_util_put_object(synx_data->synx_obj);
					synx_data->synx_obj = entry->data;
					synx_util_get_object(synx_data->synx_obj);
					*synx_handle = NULL;
				} else {
					kref_get(&synx_data->internal_refcount);
					*synx_handle = synx_data;
				}
			} else {
				/* wait till other thread ref/s are released */
				loop = true;
			}
		}
		mutex_unlock(&client->synx_table_lock[idx]);
	} while (loop);

	return rc;
}

static void synx_util_destroy_handle(struct synx_handle_coredata *synx_data)
{
	long idx = synx_util_handle_index(synx_data->handle);
	struct synx_client *client = synx_data->client;
	struct synx_coredata *synx_obj = synx_data->synx_obj;

	memset(synx_data, 0, sizeof(*synx_data));
	clear_bit(idx, client->bitmap);
	synx_util_put_object(synx_obj);
	pr_debug("[sess: %u] handle %d destroyed %pK\n",
		client->id, idx, synx_obj);
}

void synx_util_destroy_import_handle(struct kref *kref)
{
	struct synx_handle_coredata *synx_data =
		container_of(kref, struct synx_handle_coredata,
		import_refcount);

	pr_debug("[sess: %u] import handle cleanup for %d\n",
		synx_data->client->id, synx_data->handle);

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
		synx_data->client->id, synx_data->handle);

	/* in case of pending imports, abort clean up */
	if (kref_read(&synx_data->import_refcount))
		return;

	synx_util_destroy_handle(synx_data);
}

void synx_util_release_handle(struct synx_handle_coredata *synx_data)
{
	u32 idx;
	struct synx_client *client;

	if (!synx_data)
		return;

	idx = synx_util_handle_index(synx_data->handle);
	client = synx_data->client;
	mutex_lock(&client->synx_table_lock[idx]);
	if (synx_data->synx_obj)
		kref_put(&synx_data->internal_refcount,
			synx_util_destroy_internal_handle);
	else
		pr_err("%s: invalid handle %d\n",
			__func__, synx_data->handle);
	mutex_unlock(&client->synx_table_lock[idx]);
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
	long idx;
	struct synx_client_cb *cb;

	if (!client || !data || !cb_idx)
		return -EINVAL;

	idx = synx_util_get_free_handle(client->cb_bitmap, SYNX_MAX_OBJS);
	if (idx >= SYNX_MAX_OBJS) {
		pr_err("[sess: %u] free cb index not available\n",
			client->id);
		return -ENOMEM;
	}

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
	struct synx_client *client = NULL;

	if (cb && cb->client) {
		client = cb->client;
		pr_debug("[sess: %u] user cb queued for handle %d\n",
			client->id, h_synx);
		cb->kernel_cb.status = status;
		mutex_lock(&client->event_q_lock);
		list_add_tail(&cb->node, &client->event_q);
		mutex_unlock(&client->event_q_lock);
		wake_up_all(&client->event_wq);
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
	kfree(synx_cb);
}

struct synx_coredata *synx_util_import_object(struct synx_import_params *params)
{
	u32 idx;
	struct synx_session ex_session_id;
	struct synx_client *ex_client;
	struct synx_handle_coredata *synx_data;
	struct synx_coredata *synx_obj = NULL;

	if (!params)
		return NULL;

	ex_session_id.client_id = synx_util_client_id(params->h_synx);

	/* get the client exporting the synx handle */
	ex_client = synx_get_client(ex_session_id);
	if (!ex_client) {
		pr_err("sess: %u invalid import handle %d\n",
			ex_session_id.client_id, params->h_synx);
		return NULL;
	}

	idx = synx_util_handle_index(params->h_synx);
	/*
	 * need to access directly instead of acquire_handle
	 * as internal refcount might be released completely.
	 */
	mutex_lock(&ex_client->synx_table_lock[idx]);
	synx_data = &ex_client->synx_table[idx];
	synx_obj = synx_util_obtain_object(synx_data);
	if (!synx_obj) {
		pr_err("[sess: %u] invalid import handle %d\n",
			ex_client->id, params->h_synx);
		goto fail;
	}

	if (synx_data->handle != params->h_synx) {
		pr_err("[sess: %u] stale import handle %d\n",
			ex_client->id, params->h_synx);
		goto fail;
	}

	/* need to check whether import is accounted for in import_refcount */
	if (kref_read(&synx_data->import_refcount)) {
		/* get additional reference for client */
		synx_util_get_object(synx_obj);
		/* release the reference obtained during export */
		kref_put(&synx_data->import_refcount,
			synx_util_destroy_import_handle);
		pr_debug("sess: %u handle %d import successful\n",
			ex_client->id, params->h_synx);
	} else {
		synx_obj = NULL;
	}

fail:
	mutex_unlock(&ex_client->synx_table_lock[idx]);
	synx_put_client(ex_client);
	return synx_obj;
}

static int synx_util_export_internal(struct synx_coredata *synx_obj,
	struct synx_export_params *params)
{
	if (!synx_obj || !synx_obj->fence)
		return -EINVAL;

	return 0;
}

static int synx_util_export_external(struct synx_coredata *synx_obj,
	struct synx_export_params *params)
{
	int rc;

	if (!synx_obj || !params || !params->fence)
		return -EINVAL;

	if (synx_obj->fence) {
		/*
		 * remove the previous dma fence (if any).
		 * should not call synx_util_put_object here,
		 * as we will reuse the synx obj memory. so,
		 * release just the fence reference.
		 * note: before releasing the reference, need
		 * to ensure registered callback is removed
		 * for unsignaled object.
		 */
		if (synx_util_get_object_status(synx_obj) ==
			SYNX_STATE_ACTIVE)
			if (!dma_fence_remove_callback(synx_obj->fence,
				&synx_obj->fence_cb))
				/* continue after logging the error */
				pr_err("synx callback could not be removed %pK\n",
					synx_obj->fence);
		dma_fence_put(synx_obj->fence);
		pr_info("%s: released fence reference %pK, new fence %pK\n",
			__func__, synx_obj->fence, params->fence);
	}

	synx_obj->fence = params->fence;
	/* get lone synx framework reference on the fence */
	dma_fence_get(synx_obj->fence);
	rc = dma_fence_add_callback(synx_obj->fence,
		&synx_obj->fence_cb, synx_fence_callback);
	if (rc && rc != -ENOENT) {
		pr_err("error registering fence callback on handle %d\n",
			params->h_synx);
		return rc;
	}

	/* if fence is not active, invoke synx signaling */
	if (rc == -ENOENT)
		synx_signal_core(synx_obj, SYNX_STATE_SIGNALED_EXTERNAL,
			false, 0);
	return 0;
}

int synx_util_export_global(struct synx_client *client,
	struct synx_export_params *params)
{
	int rc = 0;
	struct synx_handle_coredata *synx_data;
	struct synx_coredata *synx_obj;

	if (!params || !params->secure_key)
		return -EINVAL;

	synx_data = synx_util_acquire_handle(client, params->h_synx);
	synx_obj = synx_util_obtain_object(synx_data);
	if (!synx_obj) {
		pr_err("[sess: %u] invalid export handle %d\n",
			client->id, params->h_synx);
		rc = -EINVAL;
		goto fail;
	}

	mutex_lock(&synx_obj->obj_lock);
	/*
	 * set the global key to the handle of the
	 * first exporting handle.
	 */
	synx_obj->type |= SYNX_FLAG_GLOBAL_FENCE;
	if (!synx_obj->global_key) {
		synx_obj->global_key = (params->h_synx &
			SYNX_CLIENT_IDX_OBJ_MASK);
		rc = synx_util_save_data(synx_obj->global_key,
			SYNX_GLOBAL_KEY_TBL, (void *)synx_obj);
		if (rc) {
			pr_err("[sess: %u] global export failed %d\n",
				client->id, params->h_synx);
			mutex_unlock(&synx_obj->obj_lock);
			goto fail;
		}
	}
	*params->secure_key = synx_obj->global_key;

	if (synx_util_is_external_object(synx_obj))
		rc = synx_util_export_external(synx_obj, params);
	else
		rc = synx_util_export_internal(synx_obj, params);
	mutex_unlock(&synx_obj->obj_lock);

fail:
	synx_util_release_handle(synx_data);
	return rc;
}

int synx_util_export_local(struct synx_client *client,
	struct synx_export_params *params)
{
	int rc = 0;
	u32 idx;
	struct synx_handle_coredata *synx_data;
	struct synx_coredata *synx_obj;

	if (!params || !params->secure_key)
		return -EINVAL;

	synx_data = synx_util_acquire_handle(client, params->h_synx);
	synx_obj = synx_util_obtain_object(synx_data);
	if (!synx_obj) {
		pr_err("[sess: %u] invalid export handle %d\n",
			client->id, params->h_synx);
		rc = -EINVAL;
		goto fail;
	}

	mutex_lock(&synx_obj->obj_lock);
	if (synx_util_is_external_object(synx_obj))
		rc = synx_util_export_external(synx_obj, params);
	else
		rc = synx_util_export_internal(synx_obj, params);
	mutex_unlock(&synx_obj->obj_lock);

	if (rc)
		goto fail;

	idx = synx_util_handle_index(params->h_synx);
	mutex_lock(&client->synx_table_lock[idx]);
	if (!kref_read(&synx_data->import_refcount))
		kref_init(&synx_data->import_refcount);
	else
		kref_get(&synx_data->import_refcount);
	mutex_unlock(&client->synx_table_lock[idx]);

fail:
	synx_util_release_handle(synx_data);
	return rc;
}

struct synx_client *synx_get_client(struct synx_session session_id)
{
	struct synx_client_metadata *client_metadata;
	struct synx_client *client;
	u32 id = synx_util_client_index(session_id.client_id);

	if (id >= SYNX_MAX_CLIENTS) {
		pr_err("%s: invalid session handle %u from pid: %d\n",
			__func__, id, current->pid);
		return NULL;
	}

	mutex_lock(&synx_dev->dev_table_lock);
	client_metadata = &synx_dev->client_table[id];
	client = client_metadata->client;
	if (client) {
		if (client->id == session_id.client_id) {
			kref_get(&client_metadata->refcount);
		} else {
			pr_err("session %u mismatch pid: %d\n",
				session_id.client_id, current->pid);
			client = NULL;
		}
	} else {
		pr_err("session %u not available, pid: %d\n",
			session_id.client_id, current->pid);
	}
	mutex_unlock(&synx_dev->dev_table_lock);

	return client;
}

static void synx_client_cleanup(struct work_struct *cb_dispatch)
{
	u32 i;
	struct synx_cleanup_cb *client_cb = container_of(cb_dispatch,
		struct synx_cleanup_cb, cb_dispatch);
	struct synx_client *client = client_cb->data;
	struct synx_handle_coredata *synx_data;

	/* go over all the remaining synx obj handles and clear them */
	for (i = 0; i < SYNX_MAX_OBJS; i++) {
		synx_data = &client->synx_table[i];
		/*
		 * cleanup unreleased references by the client
		 * Note: it is only safe to access synx_obj if
		 * there are refcounts (internal, import)
		 * remaining in the current handle, as it
		 * gurantees corresponding reference to fence.
		 */
		if (synx_data->synx_obj) {
			while (kref_read(&synx_data->internal_refcount))
				kref_put(&synx_data->internal_refcount,
					synx_util_destroy_internal_handle);
			while (kref_read(&synx_data->import_refcount))
				kref_put(&synx_data->import_refcount,
					synx_util_destroy_import_handle);
		}
		mutex_destroy(&client->synx_table_lock[i]);
	}
	mutex_destroy(&client->event_q_lock);

	pr_info("[sess: %u] session destroyed %s, uid: %u\n",
		client->id, client->name, client->id);
	vfree(client);
	kfree(client_cb);
}

static void synx_client_destroy(struct kref *kref)
{
	struct synx_client_metadata *client_metadata =
		container_of(kref, struct synx_client_metadata, refcount);
	u32 id = client_metadata->client->id;
	struct synx_cleanup_cb *client_cb;

	if (!client_metadata->client) {
		pr_err("error destroying session\n");
		return;
	}

	client_cb = kzalloc(sizeof(*client_cb), GFP_KERNEL);
	if (!client_cb)
		return;

	id = client_metadata->client->id;
	client_cb->data = (void *)client_metadata->client;
	memset(client_metadata, 0, sizeof(*client_metadata));
	clear_bit(synx_util_client_index(id), synx_dev->bitmap);

	INIT_WORK(&client_cb->cb_dispatch, synx_client_cleanup);
	queue_work(synx_dev->work_queue, &client_cb->cb_dispatch);
}

void synx_put_client(struct synx_client *client)
{
	struct synx_client_metadata *client_metadata;

	if (!client) {
		pr_err("%s: invalid client ptr\n", __func__);
		return;
	}

	if (synx_util_client_index(client->id) >= SYNX_MAX_CLIENTS) {
		pr_err("%s: session id %u invalid from pid: %d\n",
			__func__, client->id, current->pid);
		return;
	}

	mutex_lock(&synx_dev->dev_table_lock);
	client_metadata =
		&synx_dev->client_table[synx_util_client_index(client->id)];
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

int synx_util_init_table(void)
{
	hash_init(synx_global_key_tbl);
	hash_init(synx_camera_id_tbl);

	spin_lock_init(&global_tbl_lock);
	spin_lock_init(&camera_tbl_lock);

	return 0;
}

int synx_util_save_data(u32 key, u32 tbl, void *data)
{
	int rc = 0;
	struct hash_key_data *entry;

	if (!data)
		return -EINVAL;

	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return -ENOMEM;

	entry->key = key;
	entry->data = data;

	switch (tbl) {
	case SYNX_CAMERA_ID_TBL:
		spin_lock_bh(&camera_tbl_lock);
		hash_add(synx_camera_id_tbl, &entry->node, key);
		spin_unlock_bh(&camera_tbl_lock);
		break;
	case SYNX_GLOBAL_KEY_TBL:
		synx_util_get_object((struct synx_coredata *) data);
		spin_lock_bh(&global_tbl_lock);
		hash_add(synx_global_key_tbl, &entry->node, key);
		spin_unlock_bh(&global_tbl_lock);
		break;
	default:
		pr_err("invalid hash table selection\n");
		kfree(entry);
		rc = -EINVAL;
	}

	return rc;
}

struct hash_key_data *synx_util_retrieve_data(u32 key,
	u32 tbl)
{
	void *entry = NULL;
	struct hash_key_data *curr = NULL;

	switch (tbl) {
	case SYNX_CAMERA_ID_TBL:
		spin_lock_bh(&camera_tbl_lock);
		hash_for_each_possible(synx_camera_id_tbl,
			curr, node, key) {
			if (curr->key == key) {
				entry = curr;
				break;
			}
		}
		spin_unlock_bh(&camera_tbl_lock);
		break;
	case SYNX_GLOBAL_KEY_TBL:
		spin_lock_bh(&global_tbl_lock);
		hash_for_each_possible(synx_global_key_tbl,
			curr, node, key) {
			if (curr->key == key) {
				entry = curr;
				break;
			}
		}
		spin_unlock_bh(&global_tbl_lock);
		break;
	default:
		pr_err("invalid hash table selection %d\n",
			tbl);
	}

	return entry;
}
