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
	spinlock_t *spinlock = NULL;
	struct synx_table_row *row = table + idx;
	struct synx_obj_node *obj_node;

	if (!table || idx <= 0 || idx >= SYNX_MAX_OBJS)
		return -EINVAL;

	fence = kzalloc(sizeof(*fence), GFP_KERNEL);
	if (!fence)
		return -ENOMEM;

	spinlock = kzalloc(sizeof(*spinlock), GFP_KERNEL);
	if (!spinlock) {
		kfree(fence);
		return -ENOMEM;
	}

	spin_lock_init(spinlock);

	obj_node = kzalloc(sizeof(*obj_node), GFP_KERNEL);
	if (!obj_node) {
		kfree(spinlock);
		kfree(fence);
		return -ENOMEM;
	}

	dma_fence_init(fence, ops, spinlock, synx_dev->dma_context, 1);

	mutex_lock(&synx_dev->row_locks[idx]);
	row->fence = fence;
	row->spinlock = spinlock;
	obj_node->synx_obj = id;
	row->index = idx;
	INIT_LIST_HEAD(&row->synx_obj_list);
	INIT_LIST_HEAD(&row->callback_list);
	INIT_LIST_HEAD(&row->user_payload_list);

	list_add(&obj_node->list, &row->synx_obj_list);
	if (name)
		strlcpy(row->name, name, sizeof(row->name));
	mutex_unlock(&synx_dev->row_locks[idx]);

	pr_debug("synx obj init: id:0x%x state:%u fence: 0x%pK\n",
		synx_status(row), fence);

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
	struct synx_obj_node *obj_node;

	array = dma_fence_array_create(num_objs,
				fences, synx_dev->dma_context, 1, false);
	if (!array)
		return -EINVAL;

	obj_node = kzalloc(sizeof(*obj_node), GFP_KERNEL);
	if (!obj_node)
		return -ENOMEM;

	mutex_lock(&synx_dev->row_locks[idx]);
	row->fence = &array->base;
	obj_node->synx_obj = id;
	row->index = idx;
	INIT_LIST_HEAD(&row->synx_obj_list);
	INIT_LIST_HEAD(&row->callback_list);
	INIT_LIST_HEAD(&row->user_payload_list);

	list_add(&obj_node->list, &row->synx_obj_list);
	mutex_unlock(&synx_dev->row_locks[idx]);

	pr_debug("synx group obj init: id:%d state:%u fence: 0x%pK\n",
		id, synx_status(row), row->fence);

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

	state = synx_status(row);

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
		mutex_lock(&client->eventq_lock);
		list_move_tail(&payload_info->list, &client->eventq);
		mutex_unlock(&client->eventq_lock);
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
	s32 index;
	struct synx_client *client;
	struct synx_callback_info *synx_cb, *temp_cb;
	struct synx_cb_data  *upayload_info, *temp_upayload;
	struct synx_obj_node *obj_node, *temp_obj_node;
	unsigned long flags;

	if (!row || !synx_dev)
		return -EINVAL;

	index = row->index;
	spin_lock_irqsave(&synx_dev->idr_lock, flags);
	list_for_each_entry_safe(obj_node,
		temp_obj_node, &row->synx_obj_list, list) {
		if ((struct synx_table_row *)idr_remove(&synx_dev->synx_ids,
				obj_node->synx_obj) != row) {
			pr_err("removing data in idr table failed 0x%x\n",
				obj_node->synx_obj);
			spin_unlock_irqrestore(&synx_dev->idr_lock, flags);
			return -EINVAL;
		}
		pr_debug("removed synx obj at 0x%x successful\n",
			obj_node->synx_obj);
		list_del_init(&obj_node->list);
		kfree(obj_node);
	}
	spin_unlock_irqrestore(&synx_dev->idr_lock, flags);

	/*
	 * release the fence memory only for individual obj.
	 * dma fence array will release all the allocated mem
	 * in its registered release function.
	 */
	if (!is_merged_synx(row)) {
		kfree(row->spinlock);
		kfree(row->fence);

		/*
		 * invoke remaining userspace and kernel callbacks on
		 * synx obj destroyed, not signaled, with cancellation
		 * event.
		 */
		list_for_each_entry_safe(upayload_info, temp_upayload,
				&row->user_payload_list, list) {
			upayload_info->data.status =
				SYNX_CALLBACK_RESULT_CANCELED;
			memcpy(&upayload_info->data.payload_data[2],
				&upayload_info->data.payload_data[0],
				sizeof(u64));
			client = upayload_info->client;
			if (!client) {
				pr_err("invalid client member in cb list\n");
				continue;
			}
			mutex_lock(&client->eventq_lock);
			list_move_tail(&upayload_info->list, &client->eventq);
			mutex_unlock(&client->eventq_lock);
			/*
			 * since cb can be registered by multiple clients,
			 * wake the process right away
			 */
			wake_up_all(&client->wq);
			pr_debug("dispatched user cb\n");
		}

		list_for_each_entry_safe(synx_cb, temp_cb,
				&row->callback_list, list) {
			synx_cb->status = SYNX_CALLBACK_RESULT_CANCELED;
			list_del_init(&synx_cb->list);
			queue_work(synx_dev->work_queue,
				&synx_cb->cb_dispatch_work);
			pr_debug("dispatched kernel cb\n");
		}
	}

	memset(row, 0, sizeof(*row));
	clear_bit(index, synx_dev->bitmap);

	pr_debug("destroying synx obj at %d successful\n", index);
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

		mutex_lock(&synx_dev->row_locks[row->index]);
		synx_release_reference(row->fence);
		mutex_unlock(&synx_dev->row_locks[row->index]);
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

		mutex_lock(&synx_dev->row_locks[row->index]);
		count += synx_add_reference(row->fence);
		mutex_unlock(&synx_dev->row_locks[row->index]);
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

		mutex_lock(&synx_dev->row_locks[row->index]);
		count += synx_fence_add(row->fence, fences, count);
		mutex_unlock(&synx_dev->row_locks[row->index]);
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
	unsigned long flags;

	if (!synx_dev)
		return NULL;

	spin_lock_irqsave(&synx_dev->idr_lock, flags);
	row = (struct synx_table_row *) idr_find(&synx_dev->synx_ids,
		synx_obj);
	spin_unlock_irqrestore(&synx_dev->idr_lock, flags);

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
	unsigned long flags;

	if (!synx_dev)
		return -EINVAL;

	spin_lock_irqsave(&synx_dev->idr_lock, flags);
	id = idr_alloc(&synx_dev->synx_ids, pObj,
			base, base + 0x10000, GFP_ATOMIC);
	spin_unlock_irqrestore(&synx_dev->idr_lock, flags);

	pr_debug("generated Id: 0x%x, base: 0x%x, client: 0x%x\n",
		id, base, current->tgid);
	return id;
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
			pr_debug("synx global data found at %d\n",
				row->index);
			break;
		}
	}

	return row;
}

struct synx_table_row *synx_from_import_key(s32 synx_obj, u32 key)
{
	struct synx_import_data *data, *tmp_data;
	struct synx_table_row *row = NULL;

	mutex_lock(&synx_dev->table_lock);
	list_for_each_entry_safe(data, tmp_data,
		&synx_dev->import_list, list) {
		if (data->key == key && data->synx_obj == synx_obj) {
			pr_debug("found synx handle, importing 0x%x\n",
				synx_obj);
			row = data->row;
			list_del_init(&data->list);
			kfree(data);
			break;
		}
	}
	mutex_unlock(&synx_dev->table_lock);

	return row;
}

int synx_generate_import_key(struct synx_table_row *row,
	s32 synx_obj,
	u32 *key)
{
	bool bit;
	long idx = 0;
	struct synx_import_data *data;
	struct synx_table_row *new_row;

	if (!row)
		return -EINVAL;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	mutex_lock(&synx_dev->table_lock);
	do {
		/* obtain a random non-zero key */
		get_random_bytes(key, sizeof(*key));
	} while (!*key);

	data->key = *key;
	data->synx_obj = synx_obj;
	/*
	 * Reason for separate metadata (for merged synx)
	 * being dma fence array has separate release func
	 * registed with dma fence ops, which doesn't invoke
	 * release func registered by the framework to clear
	 * metadata when all refs are released.
	 * Hence we need to clear the metadata for merged synx
	 * obj upon synx_release itself. But this creates a
	 * problem if synx obj is exported. Thus need separate
	 * metadata structures even though they represent same
	 * synx obj.
	 * Note, only the metadata is released, and the fence
	 * reference count is decremented still.
	 */
	if (is_merged_synx(row)) {
		do {
			idx = find_first_zero_bit(
					synx_dev->bitmap,
					SYNX_MAX_OBJS);
			if (idx >= SYNX_MAX_OBJS) {
				kfree(data);
				mutex_unlock(
					&synx_dev->table_lock);
				return -ENOMEM;
			}
			bit = test_and_set_bit(idx,
					synx_dev->bitmap);
		} while (bit);

		new_row = synx_dev->synx_table + idx;
		/* both metadata points to same dma fence */
		new_row->fence = row->fence;
		new_row->index = idx;
		INIT_LIST_HEAD(&new_row->synx_obj_list);
		INIT_LIST_HEAD(&new_row->callback_list);
		INIT_LIST_HEAD(&new_row->user_payload_list);
		data->row = new_row;
	} else {
		data->row = row;
	}
	list_add(&data->list, &synx_dev->import_list);
	pr_debug("allocated import key for 0x%x\n",
		synx_obj);
	mutex_unlock(&synx_dev->table_lock);

	return 0;
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

	mutex_lock(&synx_dev->vtbl_lock);
	client_ops = &synx_dev->bind_vtbl[type];
	if (!client_ops->valid) {
		mutex_unlock(&synx_dev->vtbl_lock);
		return NULL;
	}
	pr_debug("found bind ops for %s\n", client_ops->name);
	mutex_unlock(&synx_dev->vtbl_lock);

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

