// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */
#define pr_fmt(fmt) "synx: " fmt

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#ifdef CONFIG_SPECTRA_CAMERA
#include <cam_sync_api.h>
#endif

#include "synx_api.h"
#include "synx_util.h"

struct synx_device *synx_dev;

void synx_external_callback(s32 sync_obj, int status, void *data)
{
	struct synx_table_row *row = NULL;
	struct synx_external_data *bind_data = data;

	if (bind_data) {
		row = synx_from_key(bind_data->synx_obj, bind_data->secure_key);
		kfree(bind_data);
	}

	if (row) {
		spin_lock_bh(&synx_dev->row_spinlocks[row->index]);
		row->signaling_id = sync_obj;
		spin_unlock_bh(&synx_dev->row_spinlocks[row->index]);

		pr_debug("signaling synx 0x%x from external callback %d\n",
			row->synx_obj, sync_obj);
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
	rc = synx_init_object(synx_dev->synx_table,
			idx, id, name, &synx_fence_ops);
	if (rc) {
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

	*synx_obj = row->synx_obj;

	pr_debug("row: synx id: 0x%x, index: %d\n",
		row->synx_obj, row->index);
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

	spin_lock_bh(&synx_dev->row_spinlocks[row->index]);

	state = synx_status_locked(row);
	/* do not register if callback registered earlier */
	list_for_each_entry(temp_cb_info, &row->callback_list, list) {
		if (temp_cb_info->callback_func == cb_func &&
			temp_cb_info->cb_data == userdata) {
			pr_err("duplicate registration for synx 0x%x\n",
				row->synx_obj);
			spin_unlock_bh(&synx_dev->row_spinlocks[row->index]);
			return -EALREADY;
		}
	}

	synx_cb = kzalloc(sizeof(*synx_cb), GFP_ATOMIC);
	if (!synx_cb) {
		spin_unlock_bh(&synx_dev->row_spinlocks[row->index]);
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
		spin_unlock_bh(&synx_dev->row_spinlocks[row->index]);
		return 0;
	}

	list_add_tail(&synx_cb->list, &row->callback_list);
	spin_unlock_bh(&synx_dev->row_spinlocks[row->index]);

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

	spin_lock_bh(&synx_dev->row_spinlocks[row->index]);

	state = synx_status_locked(row);
	pr_debug("de-registering callback for synx 0x%x\n",
		row->synx_obj);
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

	spin_unlock_bh(&synx_dev->row_spinlocks[row->index]);
	return 0;
}

int synx_signal_core(struct synx_table_row *row, u32 status)
{
	int rc, ret;
	u32 i = 0;
	u32 idx = 0;
	u32 type;
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
		pr_err("signaling a composite synx object 0x%x\n",
			row->synx_obj);
		return -EINVAL;
	}

	spin_lock_bh(&synx_dev->row_spinlocks[row->index]);

	if (synx_status_locked(row) != SYNX_STATE_ACTIVE) {
		spin_unlock_bh(&synx_dev->row_spinlocks[row->index]);
		pr_err("object already signaled synx = 0x%x\n",
			row->synx_obj);
		return -EALREADY;
	}

	/* set fence error to model {signal w/ error} */
	if (status == SYNX_STATE_SIGNALED_ERROR)
		dma_fence_set_error(row->fence, -EINVAL);

	rc = dma_fence_signal_locked(row->fence);
	if (rc < 0) {
		pr_err("unable to signal synx 0x%x, err: %d\n",
			row->synx_obj, rc);
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
	spin_unlock_bh(&synx_dev->row_spinlocks[row->index]);

	for (i = 0; i < idx; i++) {
		type = bind_descs[i].external_desc.type;
		sync_id = bind_descs[i].external_desc.id[0];
		data = bind_descs[i].external_data;
		if (is_valid_type(type)) {
			bind_ops = &synx_dev->bind_vtbl[type];
			if (!bind_ops->deregister_callback ||
				!bind_ops->signal) {
				pr_err("invalid bind ops for %u\n", type);
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
		} else {
			pr_warn("unimplemented external type: %u\n", type);
		}

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

	*synx_merged = row->synx_obj;

	pr_debug("row (merged): synx 0x%x, index: %d\n",
		row->synx_obj, row->index);
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

int synx_release(s32 synx_obj)
{
	s32 idx;
	struct dma_fence *fence = NULL;
	struct synx_table_row *row  = NULL;

	pr_debug("Enter %s\n", __func__);

	row = synx_from_handle(synx_obj);
	if (!row) {
		pr_err("invalid synx: 0x%x\n", synx_obj);
		return -EINVAL;
	}

	/*
	 * metadata might be cleared after invoking dma_fence_put
	 * (definitely for merged synx on invoing deinit)
	 * be carefull while accessing the metadata
	 */
	fence = row->fence;
	idx = row->index;
	spin_lock_bh(&synx_dev->row_spinlocks[idx]);
	if (synx_status_locked(row) == SYNX_STATE_ACTIVE) {
		pr_err("need to signal before release synx = 0x%x\n",
			synx_obj);
		spin_unlock_bh(&synx_dev->row_spinlocks[idx]);
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
	spin_unlock_bh(&synx_dev->row_spinlocks[idx]);
	pr_debug("Exit %s\n", __func__);

	return 0;
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

	if (!is_valid_type(external_sync.type)) {
		pr_err("invalid external sync object\n");
		return -EINVAL;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	spin_lock_bh(&synx_dev->row_spinlocks[row->index]);
	if (synx_status_locked(row) != SYNX_STATE_ACTIVE) {
		pr_err("bind to non-active synx is prohibited 0x%x\n",
			synx_obj);
		spin_unlock_bh(&synx_dev->row_spinlocks[row->index]);
		kfree(data);
		return -EINVAL;
	}

	if (row->num_bound_synxs >= SYNX_MAX_NUM_BINDINGS) {
		pr_err("max number of bindings reached for synx_objs 0x%x\n",
			synx_obj);
		spin_unlock_bh(&synx_dev->row_spinlocks[row->index]);
		kfree(data);
		return -ENOMEM;
	}

	/* don't bind external sync obj is already done */
	for (i = 0; i < row->num_bound_synxs; i++) {
		if (external_sync.id[0] ==
			row->bound_synxs[i].external_desc.id[0]) {
			pr_err("duplicate binding for external sync %d\n",
				external_sync.id[0]);
			spin_unlock_bh(&synx_dev->row_spinlocks[row->index]);
			kfree(data);
			return -EALREADY;
		}
	}

	bind_ops = &synx_dev->bind_vtbl[external_sync.type];
	if (!bind_ops->register_callback) {
		pr_err("invalid bind register for %u\n",
			external_sync.type);
		spin_unlock_bh(&synx_dev->row_spinlocks[row->index]);
		kfree(data);
		return -EINVAL;
	}

	/* data passed to external callback */
	data->synx_obj = row->synx_obj;
	data->secure_key = synx_generate_secure_key(row);

	rc = bind_ops->register_callback(synx_external_callback,
			data, external_sync.id[0]);
	if (rc < 0) {
		pr_err("callback registration failed for %d\n",
			external_sync.id[0]);
		spin_unlock_bh(&synx_dev->row_spinlocks[row->index]);
		kfree(data);
		return rc;
	}

	memcpy(&row->bound_synxs[row->num_bound_synxs],
		   &external_sync, sizeof(struct synx_external_desc));
	row->bound_synxs[row->num_bound_synxs].external_data = data;
	row->num_bound_synxs = row->num_bound_synxs + 1;
	spin_unlock_bh(&synx_dev->row_spinlocks[row->index]);

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

	spin_lock_bh(&synx_dev->row_spinlocks[row->index]);
	while (count--)
		dma_fence_get(row->fence);
	spin_unlock_bh(&synx_dev->row_spinlocks[row->index]);

	return 0;
}

int synx_import(s32 synx_obj, u32 secure_key, s32 *new_synx_obj)
{
	bool bit;
	s32 id;
	long idx = 0;
	struct synx_table_row *row = NULL;
	struct synx_table_row *new_row = NULL;

	pr_debug("Enter %s\n", __func__);

	if (!new_synx_obj)
		return -EINVAL;

	row = synx_from_key(synx_obj, secure_key);
	if (!row)
		return -EINVAL;

	/*
	 * Reason for separate metadata (for merged synx) being
	 * dma fence array has separate release func registed with
	 * dma fence ops, which doesn't invoke release func registered
	 * by the framework to clear metadata when all refs are released.
	 * Hence we need to clear the metadata for merged synx obj
	 * upon synx_release itself. But this creates a problem if
	 * the synx obj is exported. Thus we need separate metadata
	 * structures even though they represent same synx obj.
	 * Note, only the metadata is released, and the fence reference
	 * count is decremented still.
	 */
	if (is_merged_synx(row)) {
		do {
			idx = find_first_zero_bit(synx_dev->bitmap,
					SYNX_MAX_OBJS);
			if (idx >= SYNX_MAX_OBJS)
				return -ENOMEM;
			bit = test_and_set_bit(idx, synx_dev->bitmap);
		} while (bit);

		new_row = synx_dev->synx_table + idx;
		/* new global synx id */
		id = synx_create_handle(new_row);

		/* both metadata points to same dma fence */
		new_row->fence = row->fence;
		new_row->index = idx;
		new_row->synx_obj = id;
	} else {
		/* new global synx id. Imported synx points to same metadata */
		id = synx_create_handle(row);
	}

	*new_synx_obj = id;
	pr_debug("Exit %s\n", __func__);

	return 0;
}

int synx_export(s32 synx_obj, u32 *key)
{
	struct synx_table_row *row = NULL;

	row = synx_from_handle(synx_obj);
	if (!row)
		return -EINVAL;

	spin_lock_bh(&synx_dev->row_spinlocks[row->index]);
	*key = synx_generate_secure_key(row);

	/*
	 * to make sure the synx is not lost if the process dies or
	 * synx is released before any other process gets a chance to
	 * import it. The assumption is that an import will match this
	 * and account for the extra reference. Otherwise, this will
	 * be a dangling reference and needs to be garbage collected.
	 */
	dma_fence_get(row->fence);
	spin_unlock_bh(&synx_dev->row_spinlocks[row->index]);

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
	struct synx_private_ioctl_arg *k_ioctl)
{
	u32 state = SYNX_STATE_INVALID;
	struct synx_userpayload_info userpayload_info;
	struct synx_cb_data *user_payload_kernel;
	struct synx_cb_data *user_payload_iter, *temp;
	struct synx_table_row *row = NULL;
	struct synx_client *client = NULL;

	pr_debug("Enter %s\n", __func__);

	if (k_ioctl->size != sizeof(userpayload_info))
		return -EINVAL;

	if (copy_from_user(&userpayload_info,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	row = synx_from_handle(userpayload_info.synx_obj);
	if (!row) {
		pr_err("invalid synx: 0x%x\n", userpayload_info.synx_obj);
		return -EINVAL;
	}

	mutex_lock(&synx_dev->table_lock);
	client = get_current_client();
	mutex_unlock(&synx_dev->table_lock);

	if (!client) {
		pr_err("couldn't find client for process %d\n", current->tgid);
		return -EINVAL;
	}

	user_payload_kernel = kzalloc(sizeof(*user_payload_kernel), GFP_KERNEL);
	if (!user_payload_kernel)
		return -ENOMEM;

	user_payload_kernel->client = client;
	user_payload_kernel->data.synx_obj = row->synx_obj;
	memcpy(user_payload_kernel->data.payload_data,
		userpayload_info.payload,
		SYNX_PAYLOAD_WORDS * sizeof(__u64));

	spin_lock_bh(&synx_dev->row_spinlocks[row->index]);

	state = synx_status_locked(row);
	if (state == SYNX_STATE_SIGNALED_SUCCESS ||
		state == SYNX_STATE_SIGNALED_ERROR) {
		user_payload_kernel->data.status = state;
		spin_lock_bh(&client->eventq_lock);
		list_add_tail(&user_payload_kernel->list, &client->eventq);
		spin_unlock_bh(&client->eventq_lock);
		spin_unlock_bh(&synx_dev->row_spinlocks[row->index]);
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
				row->synx_obj);
			spin_unlock_bh(&synx_dev->row_spinlocks[row->index]);
			kfree(user_payload_kernel);
			return -EALREADY;
		}
	}

	list_add_tail(&user_payload_kernel->list, &row->user_payload_list);
	spin_unlock_bh(&synx_dev->row_spinlocks[row->index]);

	pr_debug("Exit %s\n", __func__);
	return 0;
}

static int synx_handle_deregister_user_payload(
	struct synx_private_ioctl_arg *k_ioctl)
{
	u32 state = SYNX_STATE_INVALID;
	struct synx_client *client = NULL;
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

	row = synx_from_handle(userpayload_info.synx_obj);
	if (!row) {
		pr_err("invalid synx: 0x%x\n", userpayload_info.synx_obj);
		return -EINVAL;
	}

	mutex_lock(&synx_dev->table_lock);
	client = get_current_client();
	mutex_unlock(&synx_dev->table_lock);

	if (!client) {
		pr_err("couldn't find client for process %d\n", current->tgid);
		return -EINVAL;
	}

	spin_lock_bh(&synx_dev->row_spinlocks[row->index]);

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

	spin_unlock_bh(&synx_dev->row_spinlocks[row->index]);

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
		data->synx_obj = row->synx_obj;
		data->status = SYNX_CALLBACK_RESULT_CANCELED;

		spin_lock_bh(&client->eventq_lock);
		list_add_tail(&user_payload_kernel->list, &client->eventq);
		spin_unlock_bh(&client->eventq_lock);
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
	struct synx_private_ioctl_arg k_ioctl;

	pr_debug("Enter %s\n", __func__);

	synx_dev = get_synx_device(filep);

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
			&k_ioctl);
		break;
	case SYNX_DEREGISTER_PAYLOAD:
		rc = synx_handle_deregister_user_payload(
			&k_ioctl);
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

	spin_lock_bh(&client->eventq_lock);
	user_payload_kernel = list_first_entry_or_null(
							&client->eventq,
							struct synx_cb_data,
							list);
	if (!user_payload_kernel) {
		spin_unlock_bh(&client->eventq_lock);
		return 0;
	}
	list_del_init(&user_payload_kernel->list);
	spin_unlock_bh(&client->eventq_lock);

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
	spin_lock_bh(&client->eventq_lock);
	/* if list has pending cb events, notify */
	if (!list_empty(&client->eventq))
		rc = POLLPRI;
	spin_unlock_bh(&client->eventq_lock);

	pr_debug("Exit %s\n", __func__);

	return rc;
}

static int synx_open(struct inode *inode, struct file *filep)
{
	struct synx_device *synx_dev = NULL;
	struct synx_client *client = NULL;

	pr_debug("Enter %s from pid: %d\n", __func__, current->tgid);

	synx_dev = container_of(inode->i_cdev, struct synx_device, cdev);

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return -ENOMEM;

	client->device = synx_dev;
	client->pid = current->tgid;
	init_waitqueue_head(&client->wq);
	INIT_LIST_HEAD(&client->eventq);
	spin_lock_init(&client->eventq_lock);

	mutex_lock(&synx_dev->table_lock);
	list_add_tail(&client->list, &synx_dev->client_list);
	synx_dev->open_cnt++;
	mutex_unlock(&synx_dev->table_lock);

	filep->private_data = client;

	pr_debug("Exit %s\n", __func__);

	return 0;
}

static int synx_close(struct file *filep, fl_owner_t id)
{
	int rc = 0;
	int i;
	struct synx_device *synx_dev = NULL;
	struct synx_client *client, *tmp_client;

	pr_debug("Enter %s\n", __func__);

	synx_dev = get_synx_device(filep);

	mutex_lock(&synx_dev->table_lock);

	synx_dev->open_cnt--;
	if (!synx_dev->open_cnt) {
		for (i = 1; i < SYNX_MAX_OBJS; i++) {
			struct synx_table_row *row =
				synx_dev->synx_table + i;
			/*
			 * signal all ACTIVE objects as ERR, but we don't care
			 * about the return status here apart from logging it.
			 */
			if (row->synx_obj && !is_merged_synx(row) &&
				(synx_status(row) == SYNX_STATE_ACTIVE)) {
				pr_debug("synx 0x%x still active at shutdown\n",
					row->synx_obj);
				rc = synx_signal_core(row,
						SYNX_STATE_SIGNALED_ERROR);
				if (rc < 0)
					pr_err("cleanup signal fail idx:0x%x\n",
						row->synx_obj);
			}
		}

		/*
		 * flush the work queue to wait for pending signal callbacks
		 * to finish
		 */
		flush_workqueue(synx_dev->work_queue);

		/*
		 * now that all objs have been signaled,
		 * destroy them
		 */
		for (i = 1; i < SYNX_MAX_OBJS; i++) {
			struct synx_table_row *row =
				synx_dev->synx_table + i;

			if (row->synx_obj) {
				rc = synx_release(row->synx_obj);
				if (rc < 0) {
					pr_err("cleanup destroy fail idx:0x%x\n",
						row->synx_obj);
				}
			}
		}
	}

	list_for_each_entry_safe(client, tmp_client,
		&synx_dev->client_list, list) {
		if (current->tgid == client->pid) {
			pr_debug("deleting client for process %d\n",
				client->pid);
			list_del_init(&client->list);
			kfree(client);
			break;
		}
	}

	mutex_unlock(&synx_dev->table_lock);

	pr_debug("Exit %s\n", __func__);

	return 0;
}

static const struct file_operations synx_fops = {
	.owner = THIS_MODULE,
	.open  = synx_open,
	.read  = synx_read,
	.flush = synx_close,
	.poll  = synx_poll,
	.unlocked_ioctl = synx_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = synx_ioctl,
#endif
};

#ifdef CONFIG_SPECTRA_CAMERA
static void synx_bind_ops_csl_type(struct bind_operations *vtbl)
{
	if (!vtbl)
		return;

	vtbl->register_callback = cam_sync_register_callback;
	vtbl->deregister_callback = cam_sync_deregister_callback;
	vtbl->enable_signaling = cam_sync_get_obj_ref;
	vtbl->signal = cam_sync_signal;

	pr_debug("csl bind functionality set\n");
}
#else
static void synx_bind_ops_csl_type(struct bind_operations *vtbl)
{
	pr_debug("csl bind functionality not available\n");
}
#endif

static void synx_bind_ops_register(struct synx_device *synx_dev)
{
	u32 i;

	for (i = 0; i < SYNX_MAX_BIND_TYPES; i++) {
		switch (i) {
		case SYNX_TYPE_CSL:
			synx_bind_ops_csl_type(&synx_dev->bind_vtbl[i]);
			break;
		default:
			pr_err("invalid external sync type\n");
		}
	}
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

	for (idx = 0; idx < SYNX_MAX_OBJS; idx++)
		spin_lock_init(&synx_dev->row_spinlocks[idx]);

	idr_init(&synx_dev->synx_ids);

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
	synx_dev->dma_context = dma_fence_context_alloc(1);

	synx_bind_ops_register(synx_dev);

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
