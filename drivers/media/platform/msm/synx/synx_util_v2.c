// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/random.h>
#include <linux/vmalloc.h>

#include "synx_debugfs_v2.h"
#include "synx_util_v2.h"

extern void synx_external_callback(s32 sync_obj, int status, void *data);

int synx_util_init_coredata(struct synx_coredata *synx_obj,
	struct synx_create_params *params,
	struct dma_fence_ops *ops,
	u64 dma_context)
{
	int rc = -SYNX_INVALID;
	spinlock_t *fence_lock;
	struct dma_fence *fence;
	struct synx_fence_entry *entry;

	if (IS_ERR_OR_NULL(synx_obj) || IS_ERR_OR_NULL(params) ||
		 IS_ERR_OR_NULL(ops) || IS_ERR_OR_NULL(params->h_synx))
		return -SYNX_INVALID;

	if (params->flags & SYNX_CREATE_GLOBAL_FENCE &&
		*params->h_synx != 0) {
		rc = synx_global_get_ref(
			synx_util_global_idx(*params->h_synx));
		synx_obj->global_idx = synx_util_global_idx(*params->h_synx);
	} else if (params->flags & SYNX_CREATE_GLOBAL_FENCE) {
		rc = synx_alloc_global_handle(params->h_synx);
		synx_obj->global_idx = synx_util_global_idx(*params->h_synx);
	} else {
		rc = synx_alloc_local_handle(params->h_synx);
	}

	if (rc != SYNX_SUCCESS)
		return rc;

	synx_obj->map_count = 1;
	synx_obj->num_bound_synxs = 0;
	synx_obj->type |= params->flags;
	kref_init(&synx_obj->refcount);
	mutex_init(&synx_obj->obj_lock);
	INIT_LIST_HEAD(&synx_obj->reg_cbs_list);
	if (params->name)
		strlcpy(synx_obj->name, params->name, sizeof(synx_obj->name));

	if (params->flags & SYNX_CREATE_DMA_FENCE) {
		fence = params->fence;
		if (IS_ERR_OR_NULL(fence)) {
			dprintk(SYNX_ERR, "invalid external fence\n");
			goto free;
		}

		dma_fence_get(fence);
		synx_obj->fence = fence;
	} else {
		/*
		 * lock and fence memory will be released in fence
		 * release function
		 */
		fence_lock = kzalloc(sizeof(*fence_lock), GFP_KERNEL);
		if (IS_ERR_OR_NULL(fence_lock)) {
			rc = -SYNX_NOMEM;
			goto free;
		}

		fence = kzalloc(sizeof(*fence), GFP_KERNEL);
		if (IS_ERR_OR_NULL(fence)) {
			kfree(fence_lock);
			rc = -SYNX_NOMEM;
			goto free;
		}

		spin_lock_init(fence_lock);
		dma_fence_init(fence, ops, fence_lock, dma_context, 1);

		synx_obj->fence = fence;
		synx_util_activate(synx_obj);
		dprintk(SYNX_MEM,
			"allocated backing fence %pK\n", fence);

		entry = kzalloc(sizeof(*entry), GFP_KERNEL);
		if (IS_ERR_OR_NULL(entry)) {
			rc = -SYNX_NOMEM;
			goto clean;
		}

		entry->key = (u64)fence;
		if (params->flags & SYNX_CREATE_GLOBAL_FENCE)
			entry->g_handle = *params->h_synx;
		else
			entry->l_handle = *params->h_synx;

		rc = synx_util_insert_fence_entry(entry,
				params->h_synx,
				params->flags & SYNX_CREATE_GLOBAL_FENCE);
		BUG_ON(rc != SYNX_SUCCESS);
	}

	if (rc != SYNX_SUCCESS)
		goto clean;

	return SYNX_SUCCESS;

clean:
	dma_fence_put(fence);
free:
	if (params->flags & SYNX_CREATE_GLOBAL_FENCE)
		synx_global_put_ref(
			synx_util_global_idx(*params->h_synx));
	else
		clear_bit(synx_util_global_idx(*params->h_synx),
			synx_dev->native->bitmap);

	return rc;
}

int synx_util_add_callback(struct synx_coredata *synx_obj,
	u32 h_synx)
{
	int rc;
	struct synx_signal_cb *signal_cb;

	if (IS_ERR_OR_NULL(synx_obj))
		return -SYNX_INVALID;

	signal_cb = kzalloc(sizeof(*signal_cb), GFP_KERNEL);
	if (IS_ERR_OR_NULL(signal_cb))
		return -SYNX_NOMEM;

	signal_cb->handle = h_synx;
	signal_cb->flag = SYNX_SIGNAL_FROM_FENCE;
	signal_cb->synx_obj = synx_obj;

	/* get reference on synx coredata for signal cb */
	synx_util_get_object(synx_obj);

	/*
	 * adding callback enables synx framework to
	 * get notified on signal from clients using
	 * native dma fence operations.
	 */
	rc = dma_fence_add_callback(synx_obj->fence,
			&signal_cb->fence_cb, synx_fence_callback);
	if (rc != 0) {
		if (rc == -ENOENT) {
			if (synx_util_is_global_object(synx_obj)) {
				/* signal (if) global handle */
				rc = synx_global_update_status(
					synx_obj->global_idx,
					synx_util_get_object_status(synx_obj));
				if (rc != SYNX_SUCCESS)
					dprintk(SYNX_ERR,
						"status update of %u with fence %pK\n",
						synx_obj->global_idx, synx_obj->fence);
			} else {
				rc = SYNX_SUCCESS;
			}
		} else {
			dprintk(SYNX_ERR,
				"error adding callback for %pK err %d\n",
				synx_obj->fence, rc);
		}
		synx_util_put_object(synx_obj);
		kfree(signal_cb);
		return rc;
	}

	synx_obj->signal_cb = signal_cb;
	dprintk(SYNX_VERB, "added callback %pK to fence %pK\n",
		signal_cb, synx_obj->fence);

	return SYNX_SUCCESS;
}

int synx_util_init_group_coredata(struct synx_coredata *synx_obj,
	struct dma_fence **fences,
	struct synx_merge_params *params,
	u32 num_objs,
	u64 dma_context)
{
	int rc;
	struct dma_fence_array *array;

	if (IS_ERR_OR_NULL(synx_obj))
		return -SYNX_INVALID;

	if (params->flags & SYNX_MERGE_GLOBAL_FENCE) {
		rc = synx_alloc_global_handle(params->h_merged_obj);
		synx_obj->global_idx =
			synx_util_global_idx(*params->h_merged_obj);
	} else {
		rc = synx_alloc_local_handle(params->h_merged_obj);
	}

	if (rc != SYNX_SUCCESS)
		return rc;

	array = dma_fence_array_create(num_objs, fences,
				dma_context, 1, false);
	if (IS_ERR_OR_NULL(array))
		return -SYNX_INVALID;

	synx_obj->fence = &array->base;
	synx_obj->map_count = 1;
	synx_obj->type = params->flags;
	synx_obj->type |= SYNX_CREATE_MERGED_FENCE;
	synx_obj->num_bound_synxs = 0;
	kref_init(&synx_obj->refcount);
	mutex_init(&synx_obj->obj_lock);
	INIT_LIST_HEAD(&synx_obj->reg_cbs_list);

	synx_util_activate(synx_obj);
	return rc;
}

static void synx_util_destroy_coredata(struct kref *kref)
{
	int rc;
	struct synx_coredata *synx_obj =
		container_of(kref, struct synx_coredata, refcount);

	if (synx_util_is_global_object(synx_obj)) {
		rc = synx_global_clear_subscribed_core(synx_obj->global_idx, SYNX_CORE_APSS);
		if (rc)
			dprintk(SYNX_ERR, "Failed to clear subscribers");

		synx_global_put_ref(synx_obj->global_idx);
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
	unsigned long flags;
	struct synx_cb_data *synx_cb, *synx_cb_temp;
	struct synx_bind_desc *bind_desc;
	struct bind_operations *bind_ops;
	struct synx_external_data *data;

	/* clear all the undispatched callbacks */
	list_for_each_entry_safe(synx_cb,
		synx_cb_temp, &synx_obj->reg_cbs_list, node) {
		dprintk(SYNX_ERR,
			"cleaning up callback of session %pK\n",
			synx_cb->session);
		list_del_init(&synx_cb->node);
		kfree(synx_cb);
	}

	for (i = 0; i < synx_obj->num_bound_synxs; i++) {
		bind_desc = &synx_obj->bound_synxs[i];
		sync_id = bind_desc->external_desc.id;
		type = bind_desc->external_desc.type;
		data = bind_desc->external_data;
		bind_ops = synx_util_get_bind_ops(type);
		if (IS_ERR_OR_NULL(bind_ops)) {
			dprintk(SYNX_ERR,
				"bind ops fail id: %d, type: %u, err: %d\n",
				sync_id, type, rc);
			continue;
		}

		/* clear the hash table entry */
		synx_util_remove_data(&sync_id, type);

		rc = bind_ops->deregister_callback(
				synx_external_callback, data, sync_id);
		if (rc < 0) {
			dprintk(SYNX_ERR,
				"de-registration fail id: %d, type: %u, err: %d\n",
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
	synx_util_release_fence_entry((u64)synx_obj->fence);

	/* dma fence framework expects handles are signaled before release,
	 * so signal if active handle and has last refcount. Synx handles
	 * on other cores are still active to carry out usual callflow.
	 */
	if (!IS_ERR_OR_NULL(synx_obj->fence)) {
		spin_lock_irqsave(synx_obj->fence->lock, flags);
		if (kref_read(&synx_obj->fence->refcount) == 1 &&
				(synx_util_get_object_status_locked(synx_obj) ==
				SYNX_STATE_ACTIVE)) {
			// set fence error to cancel
			dma_fence_set_error(synx_obj->fence,
				-SYNX_STATE_SIGNALED_CANCEL);

			rc = dma_fence_signal_locked(synx_obj->fence);
			if (rc)
				dprintk(SYNX_ERR,
					"signaling fence %pK failed=%d\n",
					synx_obj->fence, rc);
		}
		spin_unlock_irqrestore(synx_obj->fence->lock, flags);
	}

	dma_fence_put(synx_obj->fence);
	kfree(synx_obj);
	dprintk(SYNX_MEM, "released synx object %pK\n", synx_obj);
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

u32 synx_encode_handle(u32 idx, u32 core_id, bool global_idx)
{
	u32 handle = 0;

	if (idx >= SYNX_MAX_OBJS)
		return 0;

	if (global_idx) {
		handle = 1;
		handle <<= SYNX_HANDLE_CORE_BITS;
	}

	handle |= core_id;
	handle <<= SYNX_HANDLE_INDEX_BITS;
	handle |= idx;

	return handle;
}

int synx_alloc_global_handle(u32 *new_synx)
{
	int rc;
	u32 idx;

	rc = synx_global_alloc_index(&idx);
	if (rc != SYNX_SUCCESS)
		return rc;

	*new_synx = synx_encode_handle(idx, SYNX_CORE_APSS, true);
	dprintk(SYNX_DBG, "allocated global handle %u (0x%x)\n",
		*new_synx, *new_synx);

	rc = synx_global_init_coredata(*new_synx);
	return rc;
}

int synx_alloc_local_handle(u32 *new_synx)
{
	u32 idx;

	idx = synx_util_get_free_handle(synx_dev->native->bitmap,
		SYNX_MAX_OBJS);
	if (idx >= SYNX_MAX_OBJS)
		return -SYNX_NOMEM;

	*new_synx = synx_encode_handle(idx, SYNX_CORE_APSS, false);
	dprintk(SYNX_DBG, "allocated local handle %u (0x%x)\n",
		*new_synx, *new_synx);

	return SYNX_SUCCESS;
}

int synx_util_init_handle(struct synx_client *client,
	struct synx_coredata *synx_obj, u32 *new_h_synx,
	void *map_entry)
{
	int rc = SYNX_SUCCESS;
	bool found = false;
	struct synx_handle_coredata *synx_data, *curr;

	if (IS_ERR_OR_NULL(client) || IS_ERR_OR_NULL(synx_obj) ||
		IS_ERR_OR_NULL(new_h_synx) || IS_ERR_OR_NULL(map_entry))
		return -SYNX_INVALID;

	synx_data = kzalloc(sizeof(*synx_data), GFP_ATOMIC);
	if (IS_ERR_OR_NULL(synx_data))
		return -SYNX_NOMEM;

	synx_data->client = client;
	synx_data->synx_obj = synx_obj;
	synx_data->key = *new_h_synx;
	synx_data->map_entry = map_entry;
	kref_init(&synx_data->refcount);
	synx_data->rel_count = 1;

	spin_lock_bh(&client->handle_map_lock);
	hash_for_each_possible(client->handle_map,
		curr, node, *new_h_synx) {
		if (curr->key == *new_h_synx) {
			if (curr->synx_obj != synx_obj) {
				rc = -SYNX_INVALID;
				dprintk(SYNX_ERR,
					"inconsistent data in handle map\n");
			} else {
				kref_get(&curr->refcount);
				curr->rel_count++;
			}
			found = true;
			break;
		}
	}
	if (unlikely(found))
		kfree(synx_data);
	else
		hash_add(client->handle_map,
			&synx_data->node, *new_h_synx);
	spin_unlock_bh(&client->handle_map_lock);

	return rc;
}

int synx_util_activate(struct synx_coredata *synx_obj)
{
	if (IS_ERR_OR_NULL(synx_obj))
		return -SYNX_INVALID;

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
		if (IS_ERR_OR_NULL(array))
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
		if (IS_ERR_OR_NULL(array))
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
		if (IS_ERR_OR_NULL(array))
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

	if (IS_ERR_OR_NULL(arr)) {
		dprintk(SYNX_ERR, "invalid input array\n");
		return 0;
	}

	for (i = 1; i < num; i++) {
		for (j = 0; j < wr_idx ; j++) {
			if (arr[i] == arr[j]) {
				/* release reference obtained for duplicate */
				dprintk(SYNX_DBG,
					"releasing duplicate reference\n");
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
	u32 *h_synxs,
	u32 num_objs)
{
	u32 i = 0;
	struct synx_handle_coredata *synx_data;
	struct synx_coredata *synx_obj;

	if (IS_ERR_OR_NULL(client) || IS_ERR_OR_NULL(h_synxs))
		return -SYNX_INVALID;

	for (i = 0; i < num_objs; i++) {
		synx_data = synx_util_acquire_handle(client, h_synxs[i]);
		synx_obj = synx_util_obtain_object(synx_data);
		if (IS_ERR_OR_NULL(synx_obj) ||
			IS_ERR_OR_NULL(synx_obj->fence)) {
			dprintk(SYNX_ERR,
				"[sess :%llu] invalid handle %d in cleanup\n",
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
	u32 *h_synxs,
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
		dprintk(SYNX_ERR, "single handle merge is not allowed\n");
		return -SYNX_INVALID;
	}

	synx_datas = kcalloc(num_objs, sizeof(*synx_datas), GFP_KERNEL);
	if (IS_ERR_OR_NULL(synx_datas))
		return -SYNX_NOMEM;

	synx_objs = kcalloc(num_objs, sizeof(*synx_objs), GFP_KERNEL);
	if (IS_ERR_OR_NULL(synx_objs)) {
		kfree(synx_datas);
		return -SYNX_NOMEM;
	}

	for (i = 0; i < num_objs; i++) {
		synx_datas[i] = synx_util_acquire_handle(client, h_synxs[i]);
		synx_objs[i] = synx_util_obtain_object(synx_datas[i]);
		if (IS_ERR_OR_NULL(synx_objs[i]) ||
			IS_ERR_OR_NULL(synx_objs[i]->fence)) {
			dprintk(SYNX_ERR,
				"[sess :%llu] invalid handle %d in merge list\n",
				client->id, h_synxs[i]);
			*fence_cnt = i;
			goto error;
		}
		count += synx_util_get_references(synx_objs[i]);
	}

	fences = kcalloc(count, sizeof(*fences), GFP_KERNEL);
	if (IS_ERR_OR_NULL(fences)) {
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
	return -SYNX_INVALID;
}

static u32 __fence_state(struct dma_fence *fence, bool locked)
{
	s32 status;
	u32 state = SYNX_STATE_INVALID;

	if (IS_ERR_OR_NULL(fence)) {
		dprintk(SYNX_ERR, "invalid fence\n");
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

	if (IS_ERR_OR_NULL(fence)) {
		dprintk(SYNX_ERR, "invalid fence\n");
		return SYNX_STATE_INVALID;
	}

	actv_cnt = sig_cnt = err_cnt = 0;
	array = to_dma_fence_array(fence);
	if (IS_ERR_OR_NULL(array))
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

	dprintk(SYNX_DBG,
		"group cnt stats act:%u, sig: %u, err: %u\n",
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

	if (IS_ERR_OR_NULL(synx_obj))
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

	if (IS_ERR_OR_NULL(synx_obj))
		return SYNX_STATE_INVALID;

	if (synx_util_is_merged_object(synx_obj))
		state = __fence_group_state(synx_obj->fence, true);
	else
		state = __fence_state(synx_obj->fence, true);

	return state;
}

struct synx_handle_coredata *synx_util_acquire_handle(
	struct synx_client *client, u32 h_synx)
{
	struct synx_handle_coredata *synx_data = NULL;
	struct synx_handle_coredata *synx_handle =
		ERR_PTR(-SYNX_NOENT);

	if (IS_ERR_OR_NULL(client))
		return ERR_PTR(-SYNX_INVALID);

	spin_lock_bh(&client->handle_map_lock);
	hash_for_each_possible(client->handle_map,
		synx_data, node, h_synx) {
		if (synx_data->key == h_synx &&
			synx_data->rel_count != 0) {
			kref_get(&synx_data->refcount);
			synx_handle = synx_data;
			break;
		}
	}
	spin_unlock_bh(&client->handle_map_lock);

	return synx_handle;
}

struct synx_map_entry *synx_util_insert_to_map(
	struct synx_coredata *synx_obj,
	u32 h_synx, u32 flags)
{
	struct synx_map_entry *map_entry;

	map_entry = kzalloc(sizeof(*map_entry), GFP_KERNEL);
	if (IS_ERR_OR_NULL(map_entry))
		return ERR_PTR(-SYNX_NOMEM);

	kref_init(&map_entry->refcount);
	map_entry->synx_obj = synx_obj;
	map_entry->flags = flags;
	map_entry->key = h_synx;

	if (synx_util_is_global_handle(h_synx)) {
		spin_lock_bh(&synx_dev->native->global_map_lock);
		hash_add(synx_dev->native->global_map,
			&map_entry->node, h_synx);
		spin_unlock_bh(&synx_dev->native->global_map_lock);
		dprintk(SYNX_MEM,
			"added handle %u to global map %pK\n",
			h_synx, map_entry);
	} else {
		spin_lock_bh(&synx_dev->native->local_map_lock);
		hash_add(synx_dev->native->local_map,
			&map_entry->node, h_synx);
		spin_unlock_bh(&synx_dev->native->local_map_lock);
		dprintk(SYNX_MEM,
			"added handle %u to local map %pK\n",
			h_synx, map_entry);
	}

	return map_entry;
}

struct synx_map_entry *synx_util_get_map_entry(u32 h_synx)
{
	struct synx_map_entry *curr;
	struct synx_map_entry *map_entry = ERR_PTR(-SYNX_NOENT);

	if (h_synx == 0)
		return ERR_PTR(-SYNX_INVALID);

	if (synx_util_is_global_handle(h_synx)) {
		spin_lock_bh(&synx_dev->native->global_map_lock);
		hash_for_each_possible(synx_dev->native->global_map,
			curr, node, h_synx) {
			if (curr->key == h_synx) {
				kref_get(&curr->refcount);
				map_entry = curr;
				break;
			}
		}
		spin_unlock_bh(&synx_dev->native->global_map_lock);
	} else {
		spin_lock_bh(&synx_dev->native->local_map_lock);
		hash_for_each_possible(synx_dev->native->local_map,
			curr, node, h_synx) {
			if (curr->key == h_synx) {
				kref_get(&curr->refcount);
				map_entry = curr;
				break;
			}
		}
		spin_unlock_bh(&synx_dev->native->local_map_lock);
	}

	/* should we allocate if entry not found? */
	return map_entry;
}

static void synx_util_cleanup_fence(
	struct synx_coredata *synx_obj)
{
	struct synx_signal_cb *signal_cb;
	unsigned long flags;
	u32 g_status;
	u32 f_status;

	mutex_lock(&synx_obj->obj_lock);
	synx_obj->map_count--;
	signal_cb = synx_obj->signal_cb;
	f_status = synx_util_get_object_status(synx_obj);
	dprintk(SYNX_VERB, "f_status:%u, signal_cb:%p, map:%u, idx:%u\n",
		f_status, signal_cb, synx_obj->map_count, synx_obj->global_idx);
	if (synx_obj->map_count == 0 &&
		(signal_cb != NULL) &&
		(synx_obj->global_idx != 0) &&
		(f_status == SYNX_STATE_ACTIVE)) {
		/*
		 * no more clients interested for notification
		 * on handle on local core.
		 * remove reference held by callback on synx
		 * coredata structure and update cb (if still
		 * un-signaled) with global handle idx to
		 * notify any cross-core clients waiting on
		 * handle.
		 */
		g_status = synx_global_get_status(synx_obj->global_idx);
		if (g_status > SYNX_STATE_ACTIVE) {
			dprintk(SYNX_DBG, "signaling fence %pK with status %u\n",
				synx_obj->fence, g_status);
			synx_native_signal_fence(synx_obj, g_status);
		} else {
			spin_lock_irqsave(synx_obj->fence->lock, flags);
			if (synx_util_get_object_status_locked(synx_obj) ==
				SYNX_STATE_ACTIVE) {
				signal_cb->synx_obj = NULL;
				synx_obj->signal_cb =  NULL;
				/*
				 * release reference held by signal cb and
				 * get reference on global index instead.
				 */
				synx_util_put_object(synx_obj);
				synx_global_get_ref(synx_obj->global_idx);
			}
			spin_unlock_irqrestore(synx_obj->fence->lock, flags);
		}
	} else if (synx_obj->map_count == 0 && signal_cb &&
		(f_status == SYNX_STATE_ACTIVE)) {
		if (dma_fence_remove_callback(synx_obj->fence,
			&signal_cb->fence_cb)) {
			kfree(signal_cb);
			synx_obj->signal_cb = NULL;
			/*
			 * release reference held by signal cb and
			 * get reference on global index instead.
			 */
			synx_util_put_object(synx_obj);
			dprintk(SYNX_MEM, "signal cb destroyed %pK\n",
				synx_obj->signal_cb);
		}
	}
	mutex_unlock(&synx_obj->obj_lock);
}

static void synx_util_destroy_map_entry_worker(
	struct work_struct *dispatch)
{
	struct synx_map_entry *map_entry =
		container_of(dispatch, struct synx_map_entry, dispatch);
	struct synx_coredata *synx_obj;

	synx_obj = map_entry->synx_obj;
	if (!IS_ERR_OR_NULL(synx_obj)) {
		synx_util_cleanup_fence(synx_obj);
		/* release reference held by map entry */
		synx_util_put_object(synx_obj);
	}

	if (!synx_util_is_global_handle(map_entry->key))
		clear_bit(synx_util_global_idx(map_entry->key),
			synx_dev->native->bitmap);
	dprintk(SYNX_VERB, "map entry for %u destroyed %pK\n",
		map_entry->key, map_entry);
	kfree(map_entry);
}

static void synx_util_destroy_map_entry(struct kref *kref)
{
	struct synx_map_entry *map_entry =
		container_of(kref, struct synx_map_entry, refcount);

	hash_del(&map_entry->node);
	dprintk(SYNX_MEM, "map entry for %u removed %pK\n",
		map_entry->key, map_entry);
	INIT_WORK(&map_entry->dispatch, synx_util_destroy_map_entry_worker);
	queue_work(synx_dev->wq_cleanup, &map_entry->dispatch);
}

void synx_util_release_map_entry(struct synx_map_entry *map_entry)
{
	spinlock_t *lock;

	if (IS_ERR_OR_NULL(map_entry))
		return;

	if (synx_util_is_global_handle(map_entry->key))
		lock = &synx_dev->native->global_map_lock;
	else
		lock = &synx_dev->native->local_map_lock;

	spin_lock_bh(lock);
	kref_put(&map_entry->refcount,
		synx_util_destroy_map_entry);
	spin_unlock_bh(lock);
}

static void synx_util_destroy_handle_worker(
	struct work_struct *dispatch)
{
	struct synx_handle_coredata *synx_data =
		container_of(dispatch, struct synx_handle_coredata,
		dispatch);

	synx_util_release_map_entry(synx_data->map_entry);
	dprintk(SYNX_VERB, "handle %u destroyed %pK\n",
		synx_data->key, synx_data);
	kfree(synx_data);
}

static void synx_util_destroy_handle(struct kref *kref)
{
	struct synx_handle_coredata *synx_data =
		container_of(kref, struct synx_handle_coredata,
		refcount);

	hash_del(&synx_data->node);
	dprintk(SYNX_MEM, "[sess :%llu] handle %u removed %pK\n",
		synx_data->client->id, synx_data->key, synx_data);
	INIT_WORK(&synx_data->dispatch, synx_util_destroy_handle_worker);
	queue_work(synx_dev->wq_cleanup, &synx_data->dispatch);
}

void synx_util_release_handle(struct synx_handle_coredata *synx_data)
{
	struct synx_client *client;

	if (IS_ERR_OR_NULL(synx_data))
		return;

	client = synx_data->client;
	if (IS_ERR_OR_NULL(client))
		return;

	spin_lock_bh(&client->handle_map_lock);
	kref_put(&synx_data->refcount,
		synx_util_destroy_handle);
	spin_unlock_bh(&client->handle_map_lock);
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

	if (IS_ERR_OR_NULL(client) || IS_ERR_OR_NULL(data) ||
		IS_ERR_OR_NULL(cb_idx))
		return -SYNX_INVALID;

	idx = synx_util_get_free_handle(client->cb_bitmap, SYNX_MAX_OBJS);
	if (idx >= SYNX_MAX_OBJS) {
		dprintk(SYNX_ERR,
			"[sess :%llu] free cb index not available\n",
			client->id);
		return -SYNX_NOMEM;
	}

	cb = &client->cb_table[idx];
	memset(cb, 0, sizeof(*cb));
	cb->is_valid = true;
	cb->client = client;
	cb->idx = idx;
	memcpy(&cb->kernel_cb, data,
		sizeof(cb->kernel_cb));

	*cb_idx = idx;
	dprintk(SYNX_VERB, "[sess :%llu] allocated cb index %u\n",
		client->id, *cb_idx);
	return 0;
}

int synx_util_clear_cb_entry(struct synx_client *client,
	struct synx_client_cb *cb)
{
	int rc = 0;
	u32 idx;

	if (IS_ERR_OR_NULL(cb))
		return -SYNX_INVALID;

	idx = cb->idx;
	memset(cb, 0, sizeof(*cb));
	if (idx && idx < SYNX_MAX_OBJS) {
		clear_bit(idx, client->cb_bitmap);
	} else {
		dprintk(SYNX_ERR, "invalid index\n");
		rc = -SYNX_INVALID;
	}

	return rc;
}

void synx_util_default_user_callback(u32 h_synx,
	int status, void *data)
{
	struct synx_client_cb *cb = data;
	struct synx_client *client = NULL;

	if (cb && cb->client) {
		client = cb->client;
		dprintk(SYNX_VERB,
			"[sess :%llu] user cb queued for handle %d\n",
			client->id, h_synx);
		cb->kernel_cb.status = status;
		mutex_lock(&client->event_q_lock);
		list_add_tail(&cb->node, &client->event_q);
		mutex_unlock(&client->event_q_lock);
		wake_up_all(&client->event_wq);
	} else {
		dprintk(SYNX_ERR, "invalid params\n");
	}
}

void synx_util_callback_dispatch(struct synx_coredata *synx_obj, u32 status)
{
	struct synx_cb_data *synx_cb, *synx_cb_temp;

	if (IS_ERR_OR_NULL(synx_obj)) {
		dprintk(SYNX_ERR, "invalid arguments\n");
		return;
	}

	list_for_each_entry_safe(synx_cb,
		synx_cb_temp, &synx_obj->reg_cbs_list, node) {
		synx_cb->status = status;
		list_del_init(&synx_cb->node);
		queue_work(synx_dev->wq_cb,
			&synx_cb->cb_dispatch);
		dprintk(SYNX_VERB, "dispatched callback\n");
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

	client = synx_get_client(synx_cb->session);
	if (IS_ERR_OR_NULL(client)) {
		dprintk(SYNX_ERR,
			"invalid session data %pK in cb payload\n",
			synx_cb->session);
		goto free;
	}

	if (synx_cb->idx == 0 ||
		synx_cb->idx >= SYNX_MAX_OBJS) {
		dprintk(SYNX_ERR,
			"[sess :%llu] invalid cb index %u\n",
			client->id, synx_cb->idx);
		goto fail;
	}

	status = synx_cb->status;
	cb = &client->cb_table[synx_cb->idx];
	if (!cb->is_valid) {
		dprintk(SYNX_ERR, "invalid cb payload\n");
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
			dprintk(SYNX_ERR,
				"[sess :%llu] error clearing cb entry\n",
				client->id);
	}

	dprintk(SYNX_DBG,
		"callback dispatched for handle %u, status %u, data %pK\n",
		payload.h_synx, payload.status, payload.data);

	/* dispatch kernel callback */
	payload.cb_func(payload.h_synx,
		payload.status, payload.data);

fail:
	synx_put_client(client);
free:
	kfree(synx_cb);
}

u32 synx_util_get_fence_entry(u64 key, u32 global)
{
	u32 h_synx = 0;
	struct synx_fence_entry *curr;

	spin_lock_bh(&synx_dev->native->fence_map_lock);
	hash_for_each_possible(synx_dev->native->fence_map,
		curr, node, key) {
		if (curr->key == key) {
			if (global)
				h_synx = curr->g_handle;
			/* return local handle if global not available */
			if (h_synx == 0)
				h_synx = curr->l_handle;

			break;
		}
	}
	spin_unlock_bh(&synx_dev->native->fence_map_lock);

	return h_synx;
}

void synx_util_release_fence_entry(u64 key)
{
	struct synx_fence_entry *entry = NULL, *curr;

	spin_lock_bh(&synx_dev->native->fence_map_lock);
	hash_for_each_possible(synx_dev->native->fence_map,
		curr, node, key) {
		if (curr->key == key) {
			entry = curr;
			break;
		}
	}

	if (entry) {
		hash_del(&entry->node);
		dprintk(SYNX_MEM,
			"released fence entry %pK for fence %pK\n",
			entry, (void *)key);
		kfree(entry);
	}

	spin_unlock_bh(&synx_dev->native->fence_map_lock);
}

int synx_util_insert_fence_entry(struct synx_fence_entry *entry,
	u32 *h_synx, u32 global)
{
	int rc = SYNX_SUCCESS;
	struct synx_fence_entry *curr;

	if (IS_ERR_OR_NULL(entry) || IS_ERR_OR_NULL(h_synx))
		return -SYNX_INVALID;

	spin_lock_bh(&synx_dev->native->fence_map_lock);
	hash_for_each_possible(synx_dev->native->fence_map,
		curr, node, entry->key) {
		/* raced with import from another process on same fence */
		if (curr->key == entry->key) {
			if (global)
				*h_synx = curr->g_handle;

			if (*h_synx == 0 || !global)
				*h_synx = curr->l_handle;

			rc = -SYNX_ALREADY;
			break;
		}
	}
	/* add entry only if its not present in the map */
	if (rc == SYNX_SUCCESS) {
		hash_add(synx_dev->native->fence_map,
			&entry->node, entry->key);
		dprintk(SYNX_MEM,
			"added fence entry %pK for fence %pK\n",
			entry, (void *)entry->key);
	}
	spin_unlock_bh(&synx_dev->native->fence_map_lock);

	return rc;
}

struct synx_client *synx_get_client(struct synx_session *session)
{
	struct synx_client *client = NULL;
	struct synx_client *curr;

	if (IS_ERR_OR_NULL(session))
		return ERR_PTR(-SYNX_INVALID);

	spin_lock_bh(&synx_dev->native->metadata_map_lock);
	hash_for_each_possible(synx_dev->native->client_metadata_map,
		curr, node, (u64)session) {
		if (curr == (struct synx_client *)session) {
			if (curr->active) {
				kref_get(&curr->refcount);
				client = curr;
			}
			break;
		}
	}
	spin_unlock_bh(&synx_dev->native->metadata_map_lock);

	return client;
}

static void synx_client_cleanup(struct work_struct *dispatch)
{
	int i, j;
	struct synx_client *client =
		container_of(dispatch, struct synx_client, dispatch);
	struct synx_handle_coredata *curr;
	struct hlist_node *tmp;

	/*
	 * go over all the remaining synx obj handles
	 * un-released from this session and remove them.
	 */
	hash_for_each_safe(client->handle_map, i, tmp, curr, node) {
		dprintk(SYNX_WARN,
			"[sess :%llu] un-released handle %u\n",
			client->id, curr->key);
		j = kref_read(&curr->refcount);
		/* release pending reference */
		while (j--)
			kref_put(&curr->refcount, synx_util_destroy_handle);
	}

	mutex_destroy(&client->event_q_lock);

	dprintk(SYNX_VERB, "session %llu [%s] destroyed %pK\n",
		client->id, client->name, client);
	vfree(client);
}

static void synx_client_destroy(struct kref *kref)
{
	struct synx_client *client =
		container_of(kref, struct synx_client, refcount);

	hash_del(&client->node);
	dprintk(SYNX_INFO, "[sess :%llu] session removed %s\n",
		client->id, client->name);

	INIT_WORK(&client->dispatch, synx_client_cleanup);
	queue_work(synx_dev->wq_cleanup, &client->dispatch);
}

void synx_put_client(struct synx_client *client)
{
	if (IS_ERR_OR_NULL(client))
		return;

	spin_lock_bh(&synx_dev->native->metadata_map_lock);
	kref_put(&client->refcount, synx_client_destroy);
	spin_unlock_bh(&synx_dev->native->metadata_map_lock);
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

void synx_util_log_error(u32 client_id, u32 h_synx, s32 err)
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

int synx_util_save_data(void *fence, u32 flags,
	u32 h_synx)
{
	int rc = SYNX_SUCCESS;
	struct synx_entry_64 *entry, *curr;
	u64 key;
	u32 tbl = synx_util_map_params_to_type(flags);

	switch (tbl) {
	case SYNX_TYPE_CSL:
		key = *(u32 *)fence;
		spin_lock_bh(&synx_dev->native->csl_map_lock);
		/* ensure fence is not already added to map */
		hash_for_each_possible(synx_dev->native->csl_fence_map,
			curr, node, key) {
			if (curr->key == key) {
				rc = -SYNX_ALREADY;
				break;
			}
		}
		if (rc == SYNX_SUCCESS) {
			entry = kzalloc(sizeof(*entry), GFP_ATOMIC);
			if (entry) {
				entry->data[0] = h_synx;
				entry->key = key;
				kref_init(&entry->refcount);
				hash_add(synx_dev->native->csl_fence_map,
					&entry->node, entry->key);
				dprintk(SYNX_MEM, "added csl fence %d to map %pK\n",
					entry->key, entry);
			} else {
				rc = -SYNX_NOMEM;
			}
		}
		spin_unlock_bh(&synx_dev->native->csl_map_lock);
		break;
	default:
		dprintk(SYNX_ERR, "invalid hash table selection\n");
		kfree(entry);
		rc = -SYNX_INVALID;
	}

	return rc;
}

struct synx_entry_64 *synx_util_retrieve_data(void *fence,
	u32 type)
{
	u64 key;
	struct synx_entry_64 *entry = NULL;
	struct synx_entry_64 *curr;

	switch (type) {
	case SYNX_TYPE_CSL:
		key = *(u32 *)fence;
		spin_lock_bh(&synx_dev->native->csl_map_lock);
		hash_for_each_possible(synx_dev->native->csl_fence_map,
			curr, node, key) {
			if (curr->key == key) {
				kref_get(&curr->refcount);
				entry = curr;
				break;
			}
		}
		spin_unlock_bh(&synx_dev->native->csl_map_lock);
		break;
	default:
		dprintk(SYNX_ERR, "invalid hash table selection %u\n",
			type);
	}

	return entry;
}

static void synx_util_destroy_data(struct kref *kref)
{
	struct synx_entry_64 *entry =
		container_of(kref, struct synx_entry_64, refcount);

	hash_del(&entry->node);
	dprintk(SYNX_MEM, "released fence %llu entry %pK\n",
		entry->key, entry);
	kfree(entry);
}

void synx_util_remove_data(void *fence,
	u32 type)
{
	u64 key;
	struct synx_entry_64 *entry = NULL;
	struct synx_entry_64 *curr;

	if (IS_ERR_OR_NULL(fence))
		return;

	switch (type) {
	case SYNX_TYPE_CSL:
		key = *((u32 *)fence);
		spin_lock_bh(&synx_dev->native->csl_map_lock);
		hash_for_each_possible(synx_dev->native->csl_fence_map,
			curr, node, key) {
			if (curr->key == key) {
				entry = curr;
				break;
			}
		}
		if (entry)
			kref_put(&entry->refcount, synx_util_destroy_data);
		spin_unlock_bh(&synx_dev->native->csl_map_lock);
		break;
	default:
		dprintk(SYNX_ERR, "invalid hash table selection %u\n",
			type);
	}
}

void synx_util_map_import_params_to_create(
	struct synx_import_indv_params *params,
	struct synx_create_params *c_params)
{
	if (IS_ERR_OR_NULL(params) || IS_ERR_OR_NULL(c_params))
		return;

	if (params->flags & SYNX_IMPORT_GLOBAL_FENCE)
		c_params->flags |= SYNX_CREATE_GLOBAL_FENCE;

	if (params->flags & SYNX_IMPORT_LOCAL_FENCE)
		c_params->flags |= SYNX_CREATE_LOCAL_FENCE;

	if (params->flags & SYNX_IMPORT_DMA_FENCE)
		c_params->flags |= SYNX_CREATE_DMA_FENCE;
}

u32 synx_util_map_client_id_to_core(
	enum synx_client_id id)
{
	u32 core_id;

	switch (id) {
	case SYNX_CLIENT_NATIVE:
		core_id = SYNX_CORE_APSS; break;
	case SYNX_CLIENT_EVA_CTX0:
		core_id = SYNX_CORE_EVA; break;
	case SYNX_CLIENT_VID_CTX0:
		core_id = SYNX_CORE_IRIS; break;
	case SYNX_CLIENT_NSP_CTX0:
		core_id = SYNX_CORE_NSP; break;
	default:
		core_id = SYNX_CORE_MAX;
	}

	return core_id;
}
