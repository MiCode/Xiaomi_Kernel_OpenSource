// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */
#define pr_fmt(fmt) "synx: " fmt

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "synx_api.h"
#include "synx_util.h"
#include "synx_debugfs.h"

struct synx_device *synx_dev;

void synx_external_callback(s32 sync_obj, int status, void *data)
{
	s32 synx_obj;
	struct synx_table_row *row = NULL;
	struct synx_external_data *bind_data = data;

	if (bind_data) {
		synx_obj = bind_data->synx_obj;
		row = synx_from_key(bind_data->synx_obj, bind_data->secure_key);
		kfree(bind_data);
	}

	if (row) {
		mutex_lock(&synx_dev->row_locks[row->index]);
		row->signaling_id = sync_obj;
		mutex_unlock(&synx_dev->row_locks[row->index]);

		pr_debug("signaling synx 0x%x from external callback %d\n",
			synx_obj, sync_obj);
		synx_signal_core(row, status);
	} else {
		pr_err("invalid callback from sync external obj %d\n",
			sync_obj);
	}
}

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
	struct synx_table_row *row = NULL;

	pr_debug("Enter %s\n", __func__);

	row = synx_from_fence(fence);
	if (row) {
		/* metadata (row) will be cleared in the deinit function */
		synx_deinit_object(row);
	}

	pr_debug("Exit %s\n", __func__);
}

static struct dma_fence_ops synx_fence_ops = {
	.wait = dma_fence_default_wait,
	.enable_signaling = synx_fence_enable_signaling,
	.get_driver_name = synx_fence_driver_name,
	.get_timeline_name = synx_fence_driver_name,
	.release = synx_fence_release,
};

int synx_create(s32 *synx_obj, const char *name)
{
	int rc;
	long idx;
	bool bit;
	s32 id;
	struct synx_table_row *row = NULL;

	pr_debug("Enter %s\n", __func__);

	do {
		idx = find_first_zero_bit(synx_dev->bitmap, SYNX_MAX_OBJS);
		if (idx >= SYNX_MAX_OBJS)
			return -ENOMEM;
		pr_debug("index location available at idx: %ld\n", idx);
		bit = test_and_set_bit(idx, synx_dev->bitmap);
	} while (bit);

	/* global synx id */
	id = synx_create_handle(synx_dev->synx_table + idx);
	if (id < 0) {
		pr_err("unable to allocate the synx handle\n");
		clear_bit(idx, synx_dev->bitmap);
		return -EINVAL;
	}

	rc = synx_init_object(synx_dev->synx_table,
			idx, id, name, &synx_fence_ops);
	if (rc < 0) {
		pr_err("unable to init row at idx = %ld\n", idx);
		clear_bit(idx, synx_dev->bitmap);
		return -EINVAL;
	}

	row = synx_dev->synx_table + idx;
	rc = synx_activate(row);
	if (rc) {
		pr_err("unable to activate row at idx = %ld\n", idx);
		synx_deinit_object(row);
		return -EINVAL;
	}

	*synx_obj = id;

	pr_debug("row: synx id: 0x%x, index: %d\n",
		id, row->index);
	pr_debug("Exit %s\n", __func__);

	return rc;
}

int synx_register_callback(s32 synx_obj,
	void *userdata, synx_callback cb_func)
{
	u32 state = SYNX_STATE_INVALID;
	struct synx_callback_info *synx_cb;
	struct synx_callback_info *temp_cb_info;
	struct synx_table_row *row = NULL;

	row = synx_from_handle(synx_obj);
	if (!row || !cb_func)
		return -EINVAL;

	mutex_lock(&synx_dev->row_locks[row->index]);

	state = synx_status(row);
	/* do not register if callback registered earlier */
	list_for_each_entry(temp_cb_info, &row->callback_list, list) {
		if (temp_cb_info->callback_func == cb_func &&
			temp_cb_info->cb_data == userdata) {
			pr_err("duplicate registration for synx 0x%x\n",
				synx_obj);
			mutex_unlock(&synx_dev->row_locks[row->index]);
			return -EALREADY;
		}
	}

	synx_cb = kzalloc(sizeof(*synx_cb), GFP_KERNEL);
	if (!synx_cb) {
		mutex_unlock(&synx_dev->row_locks[row->index]);
		return -ENOMEM;
	}

	synx_cb->callback_func = cb_func;
	synx_cb->cb_data = userdata;
	synx_cb->synx_obj = synx_obj;
	INIT_WORK(&synx_cb->cb_dispatch_work, synx_util_cb_dispatch);

	/* trigger callback if synx object is already in SIGNALED state */
	if (state == SYNX_STATE_SIGNALED_SUCCESS ||
		state == SYNX_STATE_SIGNALED_ERROR) {
		synx_cb->status = state;
		pr_debug("callback triggered for synx 0x%x\n",
			synx_cb->synx_obj);
		queue_work(synx_dev->work_queue,
			&synx_cb->cb_dispatch_work);
		mutex_unlock(&synx_dev->row_locks[row->index]);
		return 0;
	}

	list_add_tail(&synx_cb->list, &row->callback_list);
	mutex_unlock(&synx_dev->row_locks[row->index]);

	return 0;
}

int synx_deregister_callback(s32 synx_obj,
	synx_callback cb_func,
	void *userdata,
	synx_callback cancel_cb_func)
{
	u32 state = SYNX_STATE_INVALID;
	struct synx_table_row *row = NULL;
	struct synx_callback_info *synx_cb, *temp;

	row = synx_from_handle(synx_obj);
	if (!row) {
		pr_err("invalid synx 0x%x\n", synx_obj);
		return -EINVAL;
	}

	mutex_lock(&synx_dev->row_locks[row->index]);

	state = synx_status(row);
	pr_debug("de-registering callback for synx 0x%x\n",
		synx_obj);
	list_for_each_entry_safe(synx_cb, temp, &row->callback_list, list) {
		if (synx_cb->callback_func == cb_func &&
			synx_cb->cb_data == userdata) {
			list_del_init(&synx_cb->list);
			if (cancel_cb_func) {
				synx_cb->status = SYNX_CALLBACK_RESULT_CANCELED;
				synx_cb->callback_func = cancel_cb_func;
				queue_work(synx_dev->work_queue,
					&synx_cb->cb_dispatch_work);
			} else {
				kfree(synx_cb);
			}
		}
	}

	mutex_unlock(&synx_dev->row_locks[row->index]);
	return 0;
}

int synx_signal_core(struct synx_table_row *row, u32 status)
{
	int rc, ret;
	u32 i = 0;
	u32 idx = 0;
	s32 sync_id;
	struct synx_external_data *data = NULL;
	struct synx_bind_desc bind_descs[SYNX_MAX_NUM_BINDINGS];
	struct bind_operations *bind_ops = NULL;

	pr_debug("Enter %s\n", __func__);

	if (!row) {
		pr_err("invalid synx row\n");
		return -EINVAL;
	}

	if (status != SYNX_STATE_SIGNALED_SUCCESS &&
		status != SYNX_STATE_SIGNALED_ERROR) {
		pr_err("signaling with undefined status = %d\n",
			status);
		return -EINVAL;
	}

	if (is_merged_synx(row)) {
		pr_err("signaling a composite synx object at %d\n",
			row->index);
		return -EINVAL;
	}

	mutex_lock(&synx_dev->row_locks[row->index]);

	if (!row->index) {
		mutex_unlock(&synx_dev->row_locks[row->index]);
		pr_err("object already cleaned up at %d\n",
			row->index);
		return -EINVAL;
	}

	if (synx_status(row) != SYNX_STATE_ACTIVE) {
		mutex_unlock(&synx_dev->row_locks[row->index]);
		pr_err("object already signaled synx at %d\n",
			row->index);
		return -EALREADY;
	}

	/* set fence error to model {signal w/ error} */
	if (status == SYNX_STATE_SIGNALED_ERROR)
		dma_fence_set_error(row->fence, -EINVAL);

	rc = dma_fence_signal(row->fence);
	if (rc < 0) {
		pr_err("unable to signal synx at %d, err: %d\n",
			row->index, rc);
		if (status != SYNX_STATE_SIGNALED_ERROR) {
			dma_fence_set_error(row->fence, -EINVAL);
			status = SYNX_STATE_SIGNALED_ERROR;
		}
	}

	synx_callback_dispatch(row);

	/*
	 * signal the external bound sync obj/s even if fence signal fails,
	 * w/ error signal state (set above) to prevent deadlock
	 */
	if (row->num_bound_synxs > 0) {
		memset(bind_descs, 0,
			sizeof(struct synx_bind_desc) * SYNX_MAX_NUM_BINDINGS);
		for (i = 0; i < row->num_bound_synxs; i++) {
			/* signal invoked by external sync obj */
			if (row->signaling_id ==
				row->bound_synxs[i].external_desc.id[0]) {
				pr_debug("signaling_bound_sync: %d, skipping\n",
					row->signaling_id);
				memset(&row->bound_synxs[i], 0,
					sizeof(struct synx_bind_desc));
				continue;
			}
			memcpy(&bind_descs[idx++],
				&row->bound_synxs[i],
				sizeof(struct synx_bind_desc));
			/* clear the memory, its been backed up above */
			memset(&row->bound_synxs[i], 0,
				sizeof(struct synx_bind_desc));
		}
		row->num_bound_synxs = 0;
	}
	mutex_unlock(&synx_dev->row_locks[row->index]);

	for (i = 0; i < idx; i++) {
		sync_id = bind_descs[i].external_desc.id[0];
		data = bind_descs[i].external_data;
		bind_ops = synx_get_bind_ops(
					bind_descs[i].external_desc.type);
		if (!bind_ops) {
			pr_err("invalid bind ops for %u\n",
				bind_descs[i].external_desc.type);
			kfree(data);
			continue;
		}
		/*
		 * we are already signaled, so don't want to
		 * recursively be signaled
		 */
		ret = bind_ops->deregister_callback(
				synx_external_callback, data, sync_id);
		if (ret < 0)
			pr_err("de-registration fail on sync: %d, err: %d\n",
				sync_id, ret);
		pr_debug("signaling external sync: %d, status: %u\n",
			sync_id, status);
		/* optional function to enable external signaling */
		if (bind_ops->enable_signaling) {
			ret = bind_ops->enable_signaling(sync_id);
			if (ret < 0) {
				pr_err("enable signaling fail on sync: %d, err: %d\n",
					sync_id, ret);
				continue;
			}
		}

		ret = bind_ops->signal(sync_id, status);
		if (ret < 0)
			pr_err("signaling fail on sync: %d, err: %d\n",
				sync_id, ret);

		/*
		 * release the memory allocated for external data.
		 * It is safe to release this memory as external cb
		 * has been already deregistered before this.
		 */
		kfree(data);
	}

	pr_debug("Exit %s\n", __func__);
	return rc;
}

int synx_signal(s32 synx_obj, u32 status)
{
	struct synx_table_row *row = NULL;

	row = synx_from_handle(synx_obj);
	if (!row) {
		pr_err("invalid synx: 0x%x\n", synx_obj);
		return -EINVAL;
	}

	return synx_signal_core(row, status);
}

int synx_merge(s32 *synx_objs, u32 num_objs, s32 *synx_merged)
{
	int rc;
	long idx = 0;
	bool bit;
	s32 id;
	u32 count = 0;
	struct dma_fence **fences = NULL;
	struct synx_table_row *row = NULL;

	pr_debug("Enter %s\n", __func__);

	if (!synx_objs || !synx_merged) {
		pr_err("invalid pointer(s)\n");
		return -EINVAL;
	}

	rc = synx_util_validate_merge(synx_objs, num_objs, &fences, &count);
	if (rc < 0) {
		pr_err("validation failed, merge not allowed\n");
		rc = -EINVAL;
		goto free;
	}

	do {
		idx = find_first_zero_bit(synx_dev->bitmap, SYNX_MAX_OBJS);
		if (idx >= SYNX_MAX_OBJS) {
			rc = -ENOMEM;
			goto free;
		}
		bit = test_and_set_bit(idx, synx_dev->bitmap);
	} while (bit);

	/* global synx id */
	id = synx_create_handle(synx_dev->synx_table + idx);

	rc = synx_init_group_object(synx_dev->synx_table,
			idx, id, fences, count);
	if (rc < 0) {
		pr_err("unable to init row at idx = %ld\n", idx);
		goto clear;
	}

	row = synx_dev->synx_table + idx;
	rc = synx_activate(row);
	if (rc) {
		pr_err("unable to activate row at idx = %ld, synx 0x%x\n",
			idx, id);
		goto clear;
	}

	*synx_merged = id;

	pr_debug("row (merged): synx 0x%x, index: %d\n",
		id, row->index);
	pr_debug("Exit %s\n", __func__);

	return 0;

clear:
	clear_bit(idx, synx_dev->bitmap);
free:
	synx_merge_error(synx_objs, count);
	if (num_objs <= count)
		kfree(fences);
	return rc;
}

static int synx_release_core(struct synx_table_row *row)
{
	s32 idx;
	struct dma_fence *fence = NULL;

	/*
	 * metadata might be cleared after invoking dma_fence_put
	 * (definitely for merged synx on invoing deinit)
	 * be carefull while accessing the metadata
	 */
	mutex_lock(&synx_dev->row_locks[row->index]);
	fence = row->fence;
	idx = row->index;
	if (!idx) {
		mutex_unlock(&synx_dev->row_locks[idx]);
		pr_err("object already cleaned up at %d\n", idx);
		return -EINVAL;
	}
	/*
	 * we need to clear the metadata for merged synx obj upon synx_release
	 * itself as it does not invoke the synx_fence_release function.
	 * See synx_export for more explanation.
	 */
	if (is_merged_synx(row))
		synx_deinit_object(row);

	/* do not reference fence and row in the function after this */
	dma_fence_put(fence);
	mutex_unlock(&synx_dev->row_locks[idx]);
	pr_debug("Exit %s\n", __func__);

	return 0;
}

int synx_release(s32 synx_obj)
{
	struct synx_table_row *row  = NULL;

	pr_debug("Enter %s\n", __func__);

	row = synx_from_handle(synx_obj);
	if (!row) {
		pr_err("invalid synx: 0x%x\n", synx_obj);
		return -EINVAL;
	}

	return synx_release_core(row);
}

int synx_wait(s32 synx_obj, u64 timeout_ms)
{
	unsigned long timeleft;
	struct synx_table_row *row = NULL;

	pr_debug("Enter %s\n", __func__);

	row = synx_from_handle(synx_obj);
	if (!row) {
		pr_err("invalid synx: 0x%x\n", synx_obj);
		return -EINVAL;
	}

	mutex_lock(&synx_dev->row_locks[row->index]);
	if (!row->index) {
		mutex_unlock(&synx_dev->row_locks[row->index]);
		pr_err("object already cleaned up at %d\n",
			row->index);
		return -EINVAL;
	}
	mutex_unlock(&synx_dev->row_locks[row->index]);

	timeleft = dma_fence_wait_timeout(row->fence, (bool) 0,
					msecs_to_jiffies(timeout_ms));
	if (timeleft <= 0) {
		pr_err("timed out for synx obj 0x%x\n", synx_obj);
		return -ETIMEDOUT;
	}

	if (synx_status(row) != SYNX_STATE_SIGNALED_SUCCESS) {
		pr_err("signaled error on synx obj 0x%x\n", synx_obj);
		return -EINVAL;
	}

	pr_debug("Exit %s\n", __func__);

	return 0;
}

int synx_bind(s32 synx_obj, struct synx_external_desc external_sync)
{
	int rc = 0;
	u32 i = 0;
	struct synx_table_row *row = NULL;
	struct synx_external_data *data = NULL;
	struct bind_operations *bind_ops = NULL;

	pr_debug("Enter %s\n", __func__);

	row = (struct synx_table_row *)synx_from_handle(synx_obj);
	if (!row) {
		pr_err("invalid synx: 0x%x\n", synx_obj);
		return -EINVAL;
	}

	if (is_merged_synx(row)) {
		pr_err("cannot bind to merged fence: 0x%x\n", synx_obj);
		return -EINVAL;
	}

	bind_ops = synx_get_bind_ops(external_sync.type);
	if (!bind_ops) {
		pr_err("invalid bind ops for %u\n",
			external_sync.type);
		return -EINVAL;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	mutex_lock(&synx_dev->row_locks[row->index]);
	if (synx_status(row) != SYNX_STATE_ACTIVE) {
		pr_err("bind to non-active synx is prohibited 0x%x\n",
			synx_obj);
		mutex_unlock(&synx_dev->row_locks[row->index]);
		kfree(data);
		return -EINVAL;
	}

	if (row->num_bound_synxs >= SYNX_MAX_NUM_BINDINGS) {
		pr_err("max number of bindings reached for synx_objs 0x%x\n",
			synx_obj);
		mutex_unlock(&synx_dev->row_locks[row->index]);
		kfree(data);
		return -ENOMEM;
	}

	/* don't bind external sync obj is already done */
	for (i = 0; i < row->num_bound_synxs; i++) {
		if (external_sync.id[0] ==
			row->bound_synxs[i].external_desc.id[0]) {
			pr_err("duplicate binding for external sync %d\n",
				external_sync.id[0]);
			mutex_unlock(&synx_dev->row_locks[row->index]);
			kfree(data);
			return -EALREADY;
		}
	}

	/* data passed to external callback */
	data->synx_obj = synx_obj;
	data->secure_key = synx_generate_secure_key(row);

	rc = bind_ops->register_callback(synx_external_callback,
			data, external_sync.id[0]);
	if (rc < 0) {
		pr_err("callback registration failed for %d\n",
			external_sync.id[0]);
		mutex_unlock(&synx_dev->row_locks[row->index]);
		kfree(data);
		return rc;
	}

	memcpy(&row->bound_synxs[row->num_bound_synxs],
		   &external_sync, sizeof(struct synx_external_desc));
	row->bound_synxs[row->num_bound_synxs].external_data = data;
	row->num_bound_synxs = row->num_bound_synxs + 1;
	mutex_unlock(&synx_dev->row_locks[row->index]);

	pr_debug("added external sync %d to bindings of 0x%x\n",
		external_sync.id[0], synx_obj);

	pr_debug("Exit %s\n", __func__);
	return rc;
}

int synx_get_status(s32 synx_obj)
{
	struct synx_table_row *row = NULL;

	pr_debug("getting the status for synx 0x%x\n", synx_obj);

	row = (struct synx_table_row *)synx_from_handle(synx_obj);
	if (!row) {
		pr_err("invalid synx: 0x%x\n", synx_obj);
		return SYNX_STATE_INVALID;
	}

	return synx_status(row);
}

int synx_addrefcount(s32 synx_obj, s32 count)
{
	struct synx_table_row *row = NULL;

	row = synx_from_handle(synx_obj);
	if (!row) {
		pr_err("invalid synx: 0x%x\n", synx_obj);
		return -EINVAL;
	}

	if ((count < 0) || (count > SYNX_MAX_REF_COUNTS)) {
		pr_err("invalid count, consider reducing : 0x%x\n",
			synx_obj);
		return -EINVAL;
	}

	mutex_lock(&synx_dev->row_locks[row->index]);
	while (count--)
		dma_fence_get(row->fence);
	mutex_unlock(&synx_dev->row_locks[row->index]);

	return 0;
}

int synx_import(s32 synx_obj, u32 import_key, s32 *new_synx_obj)
{
	s32 id;
	struct dma_fence *fence;
	struct synx_obj_node *obj_node;
	struct synx_table_row *row = NULL;
	u32 index;

	pr_debug("Enter %s\n", __func__);

	if (!new_synx_obj)
		return -EINVAL;

	row = synx_from_import_key(synx_obj, import_key);
	if (!row)
		return -EINVAL;

	obj_node = kzalloc(sizeof(*obj_node), GFP_KERNEL);
	if (!obj_node)
		return -ENOMEM;

	mutex_lock(&synx_dev->row_locks[row->index]);
	if (!row->index) {
		mutex_unlock(&synx_dev->row_locks[row->index]);
		pr_err("object already cleaned up at %d\n",
			row->index);
		kfree(obj_node);
		return -EINVAL;
	}

	/* new global synx id */
	id = synx_create_handle(row);
	if (id < 0) {
		fence = row->fence;
		index = row->index;
		if (is_merged_synx(row)) {
			memset(row, 0, sizeof(*row));
			clear_bit(index, synx_dev->bitmap);
			mutex_unlock(&synx_dev->row_locks[index]);
		}
		/* release the reference obtained during export */
		dma_fence_put(fence);
		kfree(obj_node);
		pr_err("error creating handle for import\n");
		return -EINVAL;
	}

	obj_node->synx_obj = id;
	list_add(&obj_node->list, &row->synx_obj_list);
	mutex_unlock(&synx_dev->row_locks[row->index]);

	*new_synx_obj = id;
	pr_debug("Exit %s\n", __func__);

	return 0;
}

int synx_export(s32 synx_obj, u32 *import_key)
{
	int rc;
	struct synx_table_row *row = NULL;

	pr_debug("Enter %s\n", __func__);

	row = synx_from_handle(synx_obj);
	if (!row)
		return -EINVAL;

	rc = synx_generate_import_key(row, synx_obj, import_key);
	if (rc < 0)
		return rc;

	mutex_lock(&synx_dev->row_locks[row->index]);
	/*
	 * to make sure the synx is not lost if the process dies or
	 * synx is released before any other process gets a chance to
	 * import it. The assumption is that an import will match this
	 * and account for the extra reference. Otherwise, this will
	 * be a dangling reference and needs to be garbage collected.
	 */
	dma_fence_get(row->fence);
	mutex_unlock(&synx_dev->row_locks[row->index]);
	pr_debug("Exit %s\n", __func__);

	return 0;
}


static int synx_handle_create(struct synx_private_ioctl_arg *k_ioctl)
{
	struct synx_info synx_create_info;
	int result;

	if (k_ioctl->size != sizeof(synx_create_info))
		return -EINVAL;

	if (copy_from_user(&synx_create_info,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	result = synx_create(&synx_create_info.synx_obj,
		synx_create_info.name);

	if (!result)
		if (copy_to_user(u64_to_user_ptr(k_ioctl->ioctl_ptr),
			&synx_create_info,
			k_ioctl->size))
			return -EFAULT;

	return result;
}

static int synx_handle_getstatus(struct synx_private_ioctl_arg *k_ioctl)
{
	struct synx_signal synx_status;

	if (k_ioctl->size != sizeof(synx_status))
		return -EINVAL;

	if (copy_from_user(&synx_status,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	synx_status.synx_state = synx_get_status(synx_status.synx_obj);

	if (copy_to_user(u64_to_user_ptr(k_ioctl->ioctl_ptr),
		&synx_status,
		k_ioctl->size))
		return -EFAULT;

	return 0;
}

static int synx_handle_import(struct synx_private_ioctl_arg *k_ioctl)
{
	struct synx_id_info id_info;

	if (k_ioctl->size != sizeof(id_info))
		return -EINVAL;

	if (copy_from_user(&id_info,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	if (synx_import(id_info.synx_obj, id_info.secure_key,
		&id_info.new_synx_obj))
		return -EINVAL;

	if (copy_to_user(u64_to_user_ptr(k_ioctl->ioctl_ptr),
		&id_info,
		k_ioctl->size))
		return -EFAULT;

	return 0;
}

static int synx_handle_export(struct synx_private_ioctl_arg *k_ioctl)
{
	struct synx_id_info id_info;

	if (k_ioctl->size != sizeof(id_info))
		return -EINVAL;

	if (copy_from_user(&id_info,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	if (synx_export(id_info.synx_obj, &id_info.secure_key))
		return -EINVAL;

	if (copy_to_user(u64_to_user_ptr(k_ioctl->ioctl_ptr),
		&id_info,
		k_ioctl->size))
		return -EFAULT;

	return 0;
}

static int synx_handle_signal(struct synx_private_ioctl_arg *k_ioctl)
{
	struct synx_signal synx_signal_info;

	if (k_ioctl->size != sizeof(synx_signal_info))
		return -EINVAL;

	if (copy_from_user(&synx_signal_info,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	return synx_signal(synx_signal_info.synx_obj,
		synx_signal_info.synx_state);
}

static int synx_handle_merge(struct synx_private_ioctl_arg *k_ioctl)
{
	struct synx_merge synx_merge_info;
	s32 *synx_objs;
	u32 num_objs;
	u32 size;
	int result;

	if (k_ioctl->size != sizeof(synx_merge_info))
		return -EINVAL;

	if (copy_from_user(&synx_merge_info,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	if (synx_merge_info.num_objs >= SYNX_MAX_OBJS)
		return -EINVAL;

	size = sizeof(u32) * synx_merge_info.num_objs;
	synx_objs = kcalloc(synx_merge_info.num_objs,
					sizeof(*synx_objs), GFP_KERNEL);
	if (!synx_objs)
		return -ENOMEM;

	if (copy_from_user(synx_objs,
		u64_to_user_ptr(synx_merge_info.synx_objs),
		sizeof(u32) * synx_merge_info.num_objs)) {
		kfree(synx_objs);
		return -EFAULT;
	}

	num_objs = synx_merge_info.num_objs;

	result = synx_merge(synx_objs,
		num_objs,
		&synx_merge_info.merged);

	if (!result)
		if (copy_to_user(u64_to_user_ptr(k_ioctl->ioctl_ptr),
			&synx_merge_info,
			k_ioctl->size)) {
			kfree(synx_objs);
			return -EFAULT;
	}

	kfree(synx_objs);

	return result;
}

static int synx_handle_wait(struct synx_private_ioctl_arg *k_ioctl)
{
	struct synx_wait synx_wait_info;

	if (k_ioctl->size != sizeof(synx_wait_info))
		return -EINVAL;

	if (copy_from_user(&synx_wait_info,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	k_ioctl->result = synx_wait(synx_wait_info.synx_obj,
		synx_wait_info.timeout_ms);

	return 0;
}

static int synx_handle_register_user_payload(
	struct synx_private_ioctl_arg *k_ioctl,
	struct synx_client *client)
{
	s32 synx_obj;
	u32 state = SYNX_STATE_INVALID;
	struct synx_userpayload_info userpayload_info;
	struct synx_cb_data *user_payload_kernel;
	struct synx_cb_data *user_payload_iter, *temp;
	struct synx_table_row *row = NULL;

	pr_debug("Enter %s\n", __func__);

	if (k_ioctl->size != sizeof(userpayload_info))
		return -EINVAL;

	if (copy_from_user(&userpayload_info,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	synx_obj = userpayload_info.synx_obj;
	row = synx_from_handle(synx_obj);
	if (!row) {
		pr_err("invalid synx: 0x%x\n", synx_obj);
		return -EINVAL;
	}

	if (!client) {
		pr_err("invalid client for process %d\n", current->pid);
		return -EINVAL;
	}

	user_payload_kernel = kzalloc(sizeof(*user_payload_kernel), GFP_KERNEL);
	if (!user_payload_kernel)
		return -ENOMEM;

	user_payload_kernel->client = client;
	user_payload_kernel->data.synx_obj = synx_obj;
	memcpy(user_payload_kernel->data.payload_data,
		userpayload_info.payload,
		SYNX_PAYLOAD_WORDS * sizeof(__u64));

	mutex_lock(&synx_dev->row_locks[row->index]);

	state = synx_status(row);
	if (state == SYNX_STATE_SIGNALED_SUCCESS ||
		state == SYNX_STATE_SIGNALED_ERROR) {
		user_payload_kernel->data.status = state;
		mutex_lock(&client->eventq_lock);
		list_add_tail(&user_payload_kernel->list, &client->eventq);
		mutex_unlock(&client->eventq_lock);
		mutex_unlock(&synx_dev->row_locks[row->index]);
		wake_up_all(&client->wq);
		return 0;
	}

	list_for_each_entry_safe(user_payload_iter,
		temp, &row->user_payload_list, list) {
		if (user_payload_iter->data.payload_data[0] ==
				user_payload_kernel->data.payload_data[0] &&
			user_payload_iter->data.payload_data[1] ==
				user_payload_kernel->data.payload_data[1]) {
			pr_err("callback already registered on 0x%x\n",
				synx_obj);
			mutex_unlock(&synx_dev->row_locks[row->index]);
			kfree(user_payload_kernel);
			return -EALREADY;
		}
	}

	list_add_tail(&user_payload_kernel->list, &row->user_payload_list);
	mutex_unlock(&synx_dev->row_locks[row->index]);

	pr_debug("Exit %s\n", __func__);
	return 0;
}

static int synx_handle_deregister_user_payload(
	struct synx_private_ioctl_arg *k_ioctl,
	struct synx_client *client)
{
	s32 synx_obj;
	u32 state = SYNX_STATE_INVALID;
	struct synx_userpayload_info userpayload_info;
	struct synx_cb_data *user_payload_kernel, *temp;
	struct synx_table_row *row = NULL;
	struct synx_user_payload *data = NULL;
	u32 match_found = 0;

	pr_debug("Enter %s\n", __func__);
	if (k_ioctl->size != sizeof(userpayload_info))
		return -EINVAL;

	if (copy_from_user(&userpayload_info,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	synx_obj = userpayload_info.synx_obj;
	row = synx_from_handle(synx_obj);
	if (!row) {
		pr_err("invalid synx: 0x%x\n", synx_obj);
		return -EINVAL;
	}

	if (!client) {
		pr_err("invalid client for process %d\n", current->pid);
		return -EINVAL;
	}

	mutex_lock(&synx_dev->row_locks[row->index]);

	state = synx_status_locked(row);
	list_for_each_entry_safe(user_payload_kernel, temp,
			&row->user_payload_list, list) {
		if (user_payload_kernel->data.payload_data[0] ==
				userpayload_info.payload[0] &&
				user_payload_kernel->data.payload_data[1] ==
				userpayload_info.payload[1]) {
			list_del_init(&user_payload_kernel->list);
			match_found = 1;
			pr_debug("registered callback removed\n");
			break;
		}
	}

	mutex_unlock(&synx_dev->row_locks[row->index]);

	if (match_found)
		kfree(user_payload_kernel);

	/* registration of cancellation cb */
	if (userpayload_info.payload[2] != 0) {
		user_payload_kernel = kzalloc(sizeof(
							*user_payload_kernel),
							GFP_KERNEL);
		if (!user_payload_kernel)
			return -ENOMEM;

		data = &user_payload_kernel->data;
		memcpy(data->payload_data,
			userpayload_info.payload,
			SYNX_PAYLOAD_WORDS * sizeof(__u64));

		user_payload_kernel->client = client;
		data->synx_obj = synx_obj;
		data->status = SYNX_CALLBACK_RESULT_CANCELED;

		mutex_lock(&client->eventq_lock);
		list_add_tail(&user_payload_kernel->list, &client->eventq);
		mutex_unlock(&client->eventq_lock);
		pr_debug("registered cancellation callback\n");
		wake_up_all(&client->wq);
	}

	pr_debug("Exit %s\n", __func__);
	return 0;
}

static int synx_handle_bind(struct synx_private_ioctl_arg *k_ioctl)
{
	struct synx_bind synx_bind_info;

	if (k_ioctl->size != sizeof(synx_bind_info))
		return -EINVAL;

	if (copy_from_user(&synx_bind_info,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	pr_debug("calling synx_bind: 0x%x\n", synx_bind_info.synx_obj);
	k_ioctl->result = synx_bind(synx_bind_info.synx_obj,
		synx_bind_info.ext_sync_desc);

	return k_ioctl->result;
}

static int synx_handle_addrefcount(struct synx_private_ioctl_arg *k_ioctl)
{
	struct synx_addrefcount addrefcount_info;

	if (k_ioctl->size != sizeof(addrefcount_info))
		return -EINVAL;

	if (copy_from_user(&addrefcount_info,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	pr_debug("calling synx_addrefcount: 0x%x, %d\n",
		addrefcount_info.synx_obj, addrefcount_info.count);
	k_ioctl->result = synx_addrefcount(addrefcount_info.synx_obj,
		addrefcount_info.count);

	return k_ioctl->result;
}

static int synx_handle_release(struct synx_private_ioctl_arg *k_ioctl)
{
	struct synx_info info;

	if (k_ioctl->size != sizeof(info))
		return -EINVAL;

	if (copy_from_user(&info,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	return synx_release(info.synx_obj);
}

static struct synx_device *get_synx_device(struct file *filep)
{
	struct synx_client *client = filep->private_data;

	return client->device;
}

static long synx_ioctl(struct file *filep,
	unsigned int cmd,
	unsigned long arg)
{
	s32 rc = 0;
	struct synx_device *synx_dev = NULL;
	struct synx_client *client;
	struct synx_private_ioctl_arg k_ioctl;

	pr_debug("Enter %s\n", __func__);

	synx_dev = get_synx_device(filep);
	client = filep->private_data;

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

	switch (k_ioctl.id) {
	case SYNX_CREATE:
		rc = synx_handle_create(&k_ioctl);
		break;
	case SYNX_RELEASE:
		rc = synx_handle_release(&k_ioctl);
		break;
	case SYNX_REGISTER_PAYLOAD:
		rc = synx_handle_register_user_payload(
			&k_ioctl, client);
		break;
	case SYNX_DEREGISTER_PAYLOAD:
		rc = synx_handle_deregister_user_payload(
			&k_ioctl, client);
		break;
	case SYNX_SIGNAL:
		rc = synx_handle_signal(&k_ioctl);
		break;
	case SYNX_MERGE:
		rc = synx_handle_merge(&k_ioctl);
		break;
	case SYNX_WAIT:
		rc = synx_handle_wait(&k_ioctl);
		if (copy_to_user((void *)arg,
			&k_ioctl,
			sizeof(k_ioctl))) {
			pr_err("invalid ioctl args\n");
			rc = -EFAULT;
		}
		break;
	case SYNX_BIND:
		rc = synx_handle_bind(&k_ioctl);
		break;
	case SYNX_ADDREFCOUNT:
		rc = synx_handle_addrefcount(&k_ioctl);
		break;
	case SYNX_GETSTATUS:
		rc = synx_handle_getstatus(&k_ioctl);
		break;
	case SYNX_IMPORT:
		rc = synx_handle_import(&k_ioctl);
		break;
	case SYNX_EXPORT:
		rc = synx_handle_export(&k_ioctl);
		break;
	default:
		rc = -EINVAL;
	}

	pr_debug("Exit %s\n", __func__);
	return rc;
}

static ssize_t synx_read(struct file *filep,
	char __user *buf, size_t size, loff_t *f_pos)
{
	ssize_t rc = 0;
	struct synx_client *client = NULL;
	struct synx_cb_data *user_payload_kernel;

	pr_debug("Enter %s\n", __func__);

	client = filep->private_data;

	if (size != sizeof(struct synx_user_payload)) {
		pr_err("invalid read size\n");
		return -EINVAL;
	}

	mutex_lock(&client->eventq_lock);
	user_payload_kernel = list_first_entry_or_null(
							&client->eventq,
							struct synx_cb_data,
							list);
	if (!user_payload_kernel) {
		mutex_unlock(&client->eventq_lock);
		return 0;
	}
	list_del_init(&user_payload_kernel->list);
	mutex_unlock(&client->eventq_lock);

	rc = size;
	if (copy_to_user(buf,
			&user_payload_kernel->data,
			sizeof(struct synx_user_payload))) {
		pr_err("couldn't copy user callback data\n");
		rc = -EFAULT;
	}
	kfree(user_payload_kernel);

	pr_debug("Exit %s\n", __func__);
	return rc;
}

static unsigned int synx_poll(struct file *filep,
	struct poll_table_struct *poll_table)
{
	int rc = 0;
	struct synx_client *client = NULL;

	pr_debug("Enter %s\n", __func__);

	client = filep->private_data;

	poll_wait(filep, &client->wq, poll_table);
	mutex_lock(&client->eventq_lock);
	/* if list has pending cb events, notify */
	if (!list_empty(&client->eventq))
		rc = POLLPRI;
	mutex_unlock(&client->eventq_lock);

	pr_debug("Exit %s\n", __func__);

	return rc;
}

static int synx_open(struct inode *inode, struct file *filep)
{
	struct synx_device *synx_dev = NULL;
	struct synx_client *client = NULL;

	pr_debug("Enter %s from pid: %d\n", __func__, current->pid);

	synx_dev = container_of(inode->i_cdev, struct synx_device, cdev);

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return -ENOMEM;

	client->device = synx_dev;
	init_waitqueue_head(&client->wq);
	INIT_LIST_HEAD(&client->eventq);
	mutex_init(&client->eventq_lock);

	mutex_lock(&synx_dev->table_lock);
	list_add_tail(&client->list, &synx_dev->client_list);
	synx_dev->open_cnt++;
	mutex_unlock(&synx_dev->table_lock);

	filep->private_data = client;

	pr_debug("Exit %s\n", __func__);

	return 0;
}

static void synx_object_cleanup(struct synx_client *client)
{
	int i;
	struct synx_cb_data *payload_info, *temp_payload_info;

	for (i = 1; i < SYNX_MAX_OBJS; i++) {
		struct synx_table_row *row =
			synx_dev->synx_table + i;

		mutex_lock(&synx_dev->row_locks[i]);
		if (row->index) {
			list_for_each_entry_safe(payload_info,
				temp_payload_info,
				&row->user_payload_list, list) {
				if (payload_info->client == client) {
					list_del_init(&payload_info->list);
					kfree(payload_info);
					pr_debug("cleaned up client payload\n");
				}
			}
		}
		mutex_unlock(&synx_dev->row_locks[i]);
	}
}

static void synx_table_cleanup(void)
{
	int rc = 0;
	int i;
	struct synx_import_data *data, *tmp_data;

	synx_dev->open_cnt--;
	if (!synx_dev->open_cnt) {
		for (i = 1; i < SYNX_MAX_OBJS; i++) {
			struct synx_table_row *row =
				synx_dev->synx_table + i;
			/*
			 * signal all ACTIVE objects as ERR, but we don't care
			 * about the return status here apart from logging it.
			 */
			if (row->index && !is_merged_synx(row) &&
				(synx_status(row) == SYNX_STATE_ACTIVE)) {
				pr_debug("synx still active at shutdown at %d\n",
					row->index);
				rc = synx_signal_core(row,
						SYNX_STATE_SIGNALED_ERROR);
				if (rc < 0)
					pr_err("cleanup signal fail at %d\n",
						row->index);
			}
		}

		/*
		 * flush the work queue to wait for pending signal callbacks
		 * to finish
		 */
		flush_workqueue(synx_dev->work_queue);

		/*
		 * now that all objs have been signaled, destroy remaining
		 * synx objs.
		 * Start with merged synx objs, thereby releasing references
		 * owned by the merged obj on its constituing synx objs.
		 */
		for (i = 1; i < SYNX_MAX_OBJS; i++) {
			struct synx_table_row *row =
				synx_dev->synx_table + i;

			if (row->index && is_merged_synx(row)) {
				rc = synx_release_core(row);
				if (rc < 0)
					pr_err("cleanup destroy fail at %d\n",
						row->index);
			}
		}

		for (i = 1; i < SYNX_MAX_OBJS; i++) {
			struct synx_table_row *row =
				synx_dev->synx_table + i;
			/*
			 * iterate till all un-cleared reference/s for
			 * synx obj is released since synx_release_core
			 * removes only one reference per invocation.
			 */
			while (row->index) {
				rc = synx_release_core(row);
				if (rc < 0)
					pr_err("cleanup destroy fail at %d\n",
						row->index);
			}
		}

		/* clean remaining un-imported synx data */
		list_for_each_entry_safe(data, tmp_data,
			&synx_dev->import_list, list) {
			pr_debug("clearing import data 0x%x\n",
				data->synx_obj);
			list_del_init(&data->list);
			kfree(data);
		}
	}
}

static int synx_close(struct inode *inode, struct file *filep)
{
	struct synx_device *synx_dev = NULL;
	struct synx_client *client;

	pr_debug("Enter %s from pid: %d\n", __func__, current->pid);

	synx_dev = get_synx_device(filep);
	client = filep->private_data;

	mutex_lock(&synx_dev->table_lock);
	synx_object_cleanup(client);
	synx_table_cleanup();
	list_del_init(&client->list);
	kfree(client);
	mutex_unlock(&synx_dev->table_lock);

	pr_debug("Exit %s\n", __func__);

	return 0;
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

int synx_initialize(struct synx_initialization_params *params)
{
	pr_debug("Enter %s from pid: %d\n", __func__, current->pid);

	mutex_lock(&synx_dev->table_lock);
	synx_dev->open_cnt++;
	mutex_unlock(&synx_dev->table_lock);

	if (params)
		pr_debug("synx client session initialized for %s\n",
			params->name);
	return 0;
}

int synx_uninitialize(void)
{
	pr_debug("Enter %s from pid: %d\n",
		__func__, current->pid);

	mutex_lock(&synx_dev->table_lock);
	synx_table_cleanup();
	mutex_unlock(&synx_dev->table_lock);

	return 0;
}

int synx_register_ops(const struct synx_register_params *params)
{
	s32 rc;
	struct synx_registered_ops *client_ops;

	if (!params || !params->name ||
		!is_valid_type(params->type) ||
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
		pr_info("registered bind ops for %s\n",
			params->name);
		rc = 0;
	} else {
		pr_info("client already registered by %s\n",
			client_ops->name);
		rc = -EINVAL;
	}
	mutex_unlock(&synx_dev->vtbl_lock);

	return rc;
}

int synx_deregister_ops(const struct synx_register_params *params)
{
	struct synx_registered_ops *client_ops;

	if (!params || !params->name ||
		!is_valid_type(params->type)) {
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

static int __init synx_init(void)
{
	int rc;
	int idx;

	pr_info("synx device init start\n");

	synx_dev = kzalloc(sizeof(*synx_dev), GFP_KERNEL);
	if (!synx_dev)
		return -ENOMEM;

	mutex_init(&synx_dev->table_lock);
	mutex_init(&synx_dev->vtbl_lock);

	for (idx = 0; idx < SYNX_MAX_OBJS; idx++)
		mutex_init(&synx_dev->row_locks[idx]);

	idr_init(&synx_dev->synx_ids);
	spin_lock_init(&synx_dev->idr_lock);

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

	/*
	 * we treat zero as invalid handle, so we will keep the 0th bit set
	 * always
	 */
	set_bit(0, synx_dev->bitmap);

	synx_dev->work_queue = alloc_workqueue(SYNX_WORKQUEUE_NAME,
		WQ_HIGHPRI | WQ_UNBOUND, 1);
	if (!synx_dev->work_queue) {
		pr_err("high priority work queue creation failed\n");
		rc = -EINVAL;
		goto fail;
	}

	INIT_LIST_HEAD(&synx_dev->client_list);
	INIT_LIST_HEAD(&synx_dev->import_list);
	synx_dev->dma_context = dma_fence_context_alloc(1);

	synx_dev->debugfs_root = init_synx_debug_dir(synx_dev);
	pr_info("synx device init success\n");

	return 0;

fail:
	device_destroy(synx_dev->class, synx_dev->dev);
	class_destroy(synx_dev->class);
reg_fail:
	unregister_chrdev_region(synx_dev->dev, 1);
alloc_fail:
	mutex_destroy(&synx_dev->table_lock);
	idr_destroy(&synx_dev->synx_ids);
	kfree(synx_dev);
	return rc;
}

device_initcall(synx_init);

MODULE_DESCRIPTION("Global Synx Driver");
MODULE_LICENSE("GPL v2");
