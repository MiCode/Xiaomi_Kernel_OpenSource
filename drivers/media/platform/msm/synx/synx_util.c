// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */
#define pr_fmt(fmt) "synx: " fmt

#include <linux/slab.h>
#include <linux/random.h>

#include "synx_api.h"
#include "synx_util.h"

bool synx_debugfs_enabled(void)
{
	return synx_dev->debugfs_root != NULL;
}

bool is_valid_type(u32 type)
{
	if (type < SYNX_MAX_BIND_TYPES)
		return true;

	return false;
}

int synx_init_object(struct synx_table_row *table,
	u32 idx,
	s32 id,
	const char *name,
	struct dma_fence_ops *ops)
{
	struct dma_fence *fence = NULL;
	struct synx_table_row *row = table + idx;

	if (!table || idx <= 0 || idx >= SYNX_MAX_OBJS)
		return -EINVAL;

	fence = kzalloc(sizeof(*fence), GFP_KERNEL);
	if (!fence)
		return -ENOMEM;

	dma_fence_init(fence, ops, &synx_dev->row_spinlocks[idx],
		synx_dev->dma_context, 1);

	row->fence = fence;
	row->synx_obj = id;
	row->index = idx;
	INIT_LIST_HEAD(&row->callback_list);
	INIT_LIST_HEAD(&row->user_payload_list);

	if (name)
		strlcpy(row->name, name, sizeof(row->name));

	pr_debug("synx obj init: id:0x%x state:%u fence: 0x%pK\n",
		row->synx_obj, synx_status_locked(row), fence);

	return 0;
}

int synx_init_group_object(struct synx_table_row *table,
	u32 idx,
	s32 id,
	struct dma_fence **fences,
	u32 num_objs)
{
	struct synx_table_row *row = table + idx;
	struct dma_fence_array *array;

	array = dma_fence_array_create(num_objs,
				fences, synx_dev->dma_context, 1, false);
	if (!array)
		return -EINVAL;

	row->fence = &array->base;
	row->synx_obj = id;
	row->index = idx;
	INIT_LIST_HEAD(&row->callback_list);
	INIT_LIST_HEAD(&row->user_payload_list);

	pr_debug("synx group obj init: id:0x%x state:%u fence: 0x%pK\n",
		row->synx_obj, synx_status_locked(row), row->fence);

	return 0;
}

void synx_callback_dispatch(struct synx_table_row *row)
{
	u32 state = SYNX_STATE_INVALID;
	struct synx_client *client = NULL;
	struct synx_callback_info *synx_cb, *temp_synx_cb;
	struct synx_cb_data *payload_info, *temp_payload_info;

	if (!row)
		return;

	state = synx_status_locked(row);

	/* dispatch the kernel callbacks registered (if any) */
	list_for_each_entry_safe(synx_cb,
		temp_synx_cb, &row->callback_list, list) {
		synx_cb->status = state;
		list_del_init(&synx_cb->list);
		queue_work(synx_dev->work_queue,
			&synx_cb->cb_dispatch_work);
		pr_debug("dispatched kernel cb\n");
	}

	/* add user payloads to eventq */
	list_for_each_entry_safe(payload_info, temp_payload_info,
		&row->user_payload_list, list) {
		payload_info->data.status = state;
		client = payload_info->client;
		if (!client) {
			pr_err("invalid client member in cb list\n");
			continue;
		}
		spin_lock_bh(&client->eventq_lock);
		list_move_tail(&payload_info->list, &client->eventq);
		spin_unlock_bh(&client->eventq_lock);
		/*
		 * since cb can be registered by multiple clients,
		 * wake the process right away
		 */
		wake_up_all(&client->wq);
		pr_debug("dispatched user cb\n");
	}
}

int synx_activate(struct synx_table_row *row)
{
	if (!row)
		return -EINVAL;

	/* move synx to ACTIVE state and register cb */
	dma_fence_enable_sw_signaling(row->fence);

	return 0;
}

int synx_deinit_object(struct synx_table_row *row)
{
	s32 synx_obj;
	struct synx_callback_info *synx_cb, *temp_cb;
	struct synx_cb_data  *upayload_info, *temp_upayload;

	if (!row || !synx_dev)
		return -EINVAL;

	synx_obj = row->synx_obj;

	spin_lock_bh(&synx_dev->idr_lock);
	if ((struct synx_table_row *)idr_remove(&synx_dev->synx_ids,
			row->synx_obj) != row) {
		pr_err("removing data in idr table failed 0x%x\n",
			row->synx_obj);
		spin_unlock_bh(&synx_dev->idr_lock);
		return -EINVAL;
	}
	spin_unlock_bh(&synx_dev->idr_lock);

	/*
	 * release the fence memory only for individual obj.
	 * dma fence array will release all the allocated mem
	 * in its registered release function.
	 */
	if (!is_merged_synx(row))
		kfree(row->fence);

	list_for_each_entry_safe(upayload_info, temp_upayload,
			&row->user_payload_list, list) {
		pr_err("pending user callback payload\n");
		list_del_init(&upayload_info->list);
		kfree(upayload_info);
	}

	list_for_each_entry_safe(synx_cb, temp_cb,
			&row->callback_list, list) {
		pr_err("pending kernel callback payload\n");
		list_del_init(&synx_cb->list);
		kfree(synx_cb);
	}

	clear_bit(row->index, synx_dev->bitmap);
	memset(row, 0, sizeof(*row));

	pr_debug("destroying synx obj: 0x%x successful\n", synx_obj);
	return 0;
}

u32 synx_add_reference(struct dma_fence *fence)
{
	u32 count = 0;
	u32 i = 0;
	struct dma_fence_array *array = NULL;

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

void synx_release_reference(struct dma_fence *fence)
{
	struct dma_fence_array *array = NULL;
	u32 i = 0;

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

u32 synx_fence_add(struct dma_fence *fence,
	struct dma_fence **fences,
	u32 idx)
{
	struct dma_fence_array *array = NULL;
	u32 i = 0;

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

u32 synx_remove_duplicates(struct dma_fence **arr, u32 num)
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
				dma_fence_put(arr[i]);
				break;
			}
		}
		if (j == wr_idx)
			arr[wr_idx++] = arr[i];
	}

	return wr_idx;
}

s32 synx_merge_error(s32 *synx_objs, u32 num_objs)
{
	struct synx_table_row *row = NULL;
	u32 i = 0;

	if (!synx_objs)
		return -EINVAL;

	for (i = 0; i < num_objs; i++) {
		row = (struct synx_table_row *)synx_from_handle(synx_objs[i]);
		if (!row) {
			pr_err("invalid handle 0x%x\n", synx_objs[i]);
			return -EINVAL;
		}

		spin_lock_bh(&synx_dev->row_spinlocks[row->index]);
		synx_release_reference(row->fence);
		spin_unlock_bh(&synx_dev->row_spinlocks[row->index]);
	}

	return 0;
}

int synx_util_validate_merge(s32 *synx_objs,
	u32 num_objs,
	struct dma_fence ***fence_list,
	u32 *fence_cnt)
{
	u32 count = 0;
	u32 i = 0;
	struct synx_table_row *row = NULL;
	struct dma_fence **fences = NULL;

	if (num_objs <= 1) {
		pr_err("single object merge is not allowed\n");
		return -EINVAL;
	}

	for (i = 0; i < num_objs; i++) {
		row = (struct synx_table_row *)synx_from_handle(synx_objs[i]);
		if (!row) {
			pr_err("invalid handle 0x%x\n", synx_objs[i]);
			*fence_cnt = i;
			return -EINVAL;
		}

		spin_lock_bh(&synx_dev->row_spinlocks[row->index]);
		count += synx_add_reference(row->fence);
		spin_unlock_bh(&synx_dev->row_spinlocks[row->index]);
	}

	fences = kcalloc(count, sizeof(*fences), GFP_KERNEL);
	if (!fences) {
		*fence_cnt = num_objs;
		return -ENOMEM;
	}

	*fence_list = fences;
	count = 0;

	for (i = 0; i < num_objs; i++) {
		row = (struct synx_table_row *)synx_from_handle(synx_objs[i]);
		if (!row) {
			*fence_cnt = num_objs;
			return -EINVAL;
		}

		spin_lock_bh(&synx_dev->row_spinlocks[row->index]);
		count += synx_fence_add(row->fence, fences, count);
		spin_unlock_bh(&synx_dev->row_spinlocks[row->index]);
	}

	/* eliminate duplicates */
	*fence_cnt = synx_remove_duplicates(fences, count);
	return 0;
}

void synx_util_cb_dispatch(struct work_struct *cb_dispatch_work)
{
	struct synx_callback_info *cb_info = container_of(cb_dispatch_work,
		struct synx_callback_info,
		cb_dispatch_work);

	cb_info->callback_func(cb_info->synx_obj,
		cb_info->status,
		cb_info->cb_data);

	kfree(cb_info);
}

bool is_merged_synx(struct synx_table_row *row)
{
	if (!row || !row->fence) {
		pr_err("invalid row argument\n");
		return false;
	}

	if (dma_fence_is_array(row->fence))
		return true;

	return false;
}

u32 __fence_state(struct dma_fence *fence, bool locked)
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
	default:
		state = SYNX_STATE_SIGNALED_ERROR;
	}

	return state;
}

u32 __fence_group_state(struct dma_fence *fence, bool locked)
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
u32 synx_status(struct synx_table_row *row)
{
	u32 state;

	if (!row)
		return SYNX_STATE_INVALID;

	if (is_merged_synx(row))
		state = __fence_group_state(row->fence, false);
	else
		state = __fence_state(row->fence, false);

	return state;
}

/* use this for status check when holding on to metadata spinlock */
u32 synx_status_locked(struct synx_table_row *row)
{
	u32 state;

	if (!row)
		return SYNX_STATE_INVALID;

	if (is_merged_synx(row))
		state = __fence_group_state(row->fence, true);
	else
		state = __fence_state(row->fence, true);


	return state;
}

void *synx_from_handle(s32 synx_obj)
{
	s32 base;
	struct synx_table_row *row;

	if (!synx_dev)
		return NULL;

	spin_lock_bh(&synx_dev->idr_lock);
	row = (struct synx_table_row *) idr_find(&synx_dev->synx_ids,
		synx_obj);
	spin_unlock_bh(&synx_dev->idr_lock);

	if (!row) {
		pr_err(
		"synx handle does not exist 0x%x\n", synx_obj);
		return NULL;
	}

	base = current->tgid << 16;

	if ((base >> 16) != (synx_obj >> 16)) {
		pr_err("current client: %d, base: %d, synx_obj: 0x%x\n",
			current->tgid, base, synx_obj);
		return NULL;
	}

	return row;
}

s32 synx_create_handle(void *pObj)
{
	s32 base = current->tgid << 16;
	s32 id;

	if (!synx_dev)
		return -EINVAL;

	spin_lock_bh(&synx_dev->idr_lock);
	id = idr_alloc(&synx_dev->synx_ids, pObj,
			base, base + 0x10000, GFP_ATOMIC);
	spin_unlock_bh(&synx_dev->idr_lock);

	pr_debug("generated Id: 0x%x, base: 0x%x, client: 0x%x\n",
		id, base, current->tgid);
	return id;
}

struct synx_client *get_current_client(void)
{
	struct synx_client *client = NULL;

	list_for_each_entry(client, &synx_dev->client_list, list) {
		if (current->tgid == client->pid)
			break;
	}
	return client;
}

int synx_generate_secure_key(struct synx_table_row *row)
{
	if (!row)
		return -EINVAL;

	if (!row->secure_key)
		get_random_bytes(&row->secure_key, sizeof(row->secure_key));

	return row->secure_key;
}

struct synx_table_row *synx_from_fence(struct dma_fence *fence)
{
	s32 idx = 0;
	struct synx_table_row *row = NULL;
	struct synx_table_row *table = synx_dev->synx_table;

	if (!fence)
		return NULL;

	for (idx = 0; idx < SYNX_MAX_OBJS; idx++) {
		if (table[idx].fence == fence) {
			row = table + idx;
			pr_debug("synx global data found for 0x%x\n",
				row->synx_obj);
			break;
		}
	}

	return row;
}

void *synx_from_key(s32 id, u32 secure_key)
{
	struct synx_table_row *row = NULL;

	if (!synx_dev)
		return NULL;

	spin_lock_bh(&synx_dev->idr_lock);
	row = (struct synx_table_row *) idr_find(&synx_dev->synx_ids, id);
	if (!row) {
		pr_err("invalid synx obj 0x%x\n", id);
		spin_unlock_bh(&synx_dev->idr_lock);
		return NULL;
	}
	spin_unlock_bh(&synx_dev->idr_lock);

	if (row->secure_key != secure_key)
		row = NULL;

	return row;
}

struct bind_operations *synx_get_bind_ops(u32 type)
{
	struct synx_registered_ops *client_ops;

	if (!is_valid_type(type))
		return NULL;

	mutex_lock(&synx_dev->table_lock);
	client_ops = &synx_dev->bind_vtbl[type];
	if (!client_ops->valid) {
		mutex_unlock(&synx_dev->table_lock);
		return NULL;
	}
	pr_debug("found bind ops for %s\n", client_ops->name);
	mutex_unlock(&synx_dev->table_lock);

	return &client_ops->ops;
}

void generate_timestamp(char *timestamp, size_t size)
{
	struct timeval tv;
	struct tm tm;

	do_gettimeofday(&tv);
	time_to_tm(tv.tv_sec, 0, &tm);
	snprintf(timestamp, size, "%02d-%02d %02d:%02d:%02d",
		tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
		tm.tm_min, tm.tm_sec);

}

void log_synx_error(s32 error_code, s32 synx_obj)
{
	struct error_node *err_node;

	if (!synx_debugfs_enabled())
		return;

	err_node = kzalloc(sizeof(*err_node), GFP_KERNEL);
	if (!err_node)
		return;

	err_node->error_code = error_code;
	err_node->synx_obj = synx_obj;
	generate_timestamp(err_node->timestamp,
		sizeof(err_node->timestamp));
	spin_lock_bh(&synx_dev->synx_node_list_lock);
	list_add(&err_node->node,
		&synx_dev->synx_debug_head);
	spin_unlock_bh(&synx_dev->synx_node_list_lock);
}

