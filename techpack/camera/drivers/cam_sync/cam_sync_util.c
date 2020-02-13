// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */

#include "cam_sync_util.h"

int cam_sync_util_find_and_set_empty_row(struct sync_device *sync_dev,
	long *idx)
{
	int rc = 0;

	mutex_lock(&sync_dev->table_lock);

	*idx = find_first_zero_bit(sync_dev->bitmap, CAM_SYNC_MAX_OBJS);

	if (*idx < CAM_SYNC_MAX_OBJS)
		set_bit(*idx, sync_dev->bitmap);
	else
		rc = -1;

	mutex_unlock(&sync_dev->table_lock);

	return rc;
}

int cam_sync_init_row(struct sync_table_row *table,
	uint32_t idx, const char *name, uint32_t type)
{
	struct sync_table_row *row = table + idx;

	if (!table || idx <= 0 || idx >= CAM_SYNC_MAX_OBJS)
		return -EINVAL;

	memset(row, 0, sizeof(*row));

	if (name)
		strlcpy(row->name, name, SYNC_DEBUG_NAME_LEN);
	INIT_LIST_HEAD(&row->parents_list);
	INIT_LIST_HEAD(&row->children_list);
	row->type = type;
	row->sync_id = idx;
	row->state = CAM_SYNC_STATE_ACTIVE;
	row->remaining = 0;
	atomic_set(&row->ref_cnt, 0);
	init_completion(&row->signaled);
	INIT_LIST_HEAD(&row->callback_list);
	INIT_LIST_HEAD(&row->user_payload_list);
	CAM_DBG(CAM_SYNC,
		"row name:%s sync_id:%i [idx:%u] row_state:%u ",
		row->name, row->sync_id, idx, row->state);

	return 0;
}

int cam_sync_init_group_object(struct sync_table_row *table,
	uint32_t idx,
	uint32_t *sync_objs,
	uint32_t num_objs)
{
	int i, rc = 0;
	struct sync_child_info *child_info;
	struct sync_parent_info *parent_info;
	struct sync_table_row *row = table + idx;
	struct sync_table_row *child_row = NULL;

	cam_sync_init_row(table, idx, "merged_fence", CAM_SYNC_TYPE_GROUP);

	/*
	 * While traversing for children, parent's row list is updated with
	 * child info and each child's row is updated with parent info.
	 * If any child state is ERROR or SUCCESS, it will not be added to list.
	 */
	for (i = 0; i < num_objs; i++) {
		child_row = table + sync_objs[i];
		spin_lock_bh(&sync_dev->row_spinlocks[sync_objs[i]]);

		/* validate child */
		if ((child_row->type == CAM_SYNC_TYPE_GROUP) ||
			(child_row->state == CAM_SYNC_STATE_INVALID)) {
			spin_unlock_bh(&sync_dev->row_spinlocks[sync_objs[i]]);
			CAM_ERR(CAM_SYNC,
				"Invalid child fence:%i state:%u type:%u",
				child_row->sync_id, child_row->state,
				child_row->type);
			rc = -EINVAL;
			goto clean_children_info;
		}

		/* check for child's state */
		if (child_row->state == CAM_SYNC_STATE_SIGNALED_ERROR) {
			row->state = CAM_SYNC_STATE_SIGNALED_ERROR;
			spin_unlock_bh(&sync_dev->row_spinlocks[sync_objs[i]]);
			continue;
		}
		if (child_row->state != CAM_SYNC_STATE_ACTIVE) {
			spin_unlock_bh(&sync_dev->row_spinlocks[sync_objs[i]]);
			continue;
		}

		row->remaining++;

		/* Add child info */
		child_info = kzalloc(sizeof(*child_info), GFP_ATOMIC);
		if (!child_info) {
			spin_unlock_bh(&sync_dev->row_spinlocks[sync_objs[i]]);
			rc = -ENOMEM;
			goto clean_children_info;
		}
		child_info->sync_id = sync_objs[i];
		list_add_tail(&child_info->list, &row->children_list);

		/* Add parent info */
		parent_info = kzalloc(sizeof(*parent_info), GFP_ATOMIC);
		if (!parent_info) {
			spin_unlock_bh(&sync_dev->row_spinlocks[sync_objs[i]]);
			rc = -ENOMEM;
			goto clean_children_info;
		}
		parent_info->sync_id = idx;
		list_add_tail(&parent_info->list, &child_row->parents_list);
		spin_unlock_bh(&sync_dev->row_spinlocks[sync_objs[i]]);
	}

	if (!row->remaining) {
		if (row->state != CAM_SYNC_STATE_SIGNALED_ERROR)
			row->state = CAM_SYNC_STATE_SIGNALED_SUCCESS;
		complete_all(&row->signaled);
	}

	return 0;

clean_children_info:
	row->state = CAM_SYNC_STATE_INVALID;
	for (i = i-1; i >= 0; i--) {
		spin_lock_bh(&sync_dev->row_spinlocks[sync_objs[i]]);
		child_row = table + sync_objs[i];
		cam_sync_util_cleanup_parents_list(child_row,
			SYNC_LIST_CLEAN_ONE, idx);
		spin_unlock_bh(&sync_dev->row_spinlocks[sync_objs[i]]);
	}

	cam_sync_util_cleanup_children_list(row, SYNC_LIST_CLEAN_ALL, 0);
	return rc;
}

int cam_sync_deinit_object(struct sync_table_row *table, uint32_t idx)
{
	struct sync_table_row      *row = table + idx;
	struct sync_child_info     *child_info, *temp_child;
	struct sync_callback_info  *sync_cb, *temp_cb;
	struct sync_parent_info    *parent_info, *temp_parent;
	struct sync_user_payload   *upayload_info, *temp_upayload;
	struct sync_table_row      *child_row = NULL, *parent_row = NULL;
	struct list_head            temp_child_list, temp_parent_list;

	if (!table || idx <= 0 || idx >= CAM_SYNC_MAX_OBJS)
		return -EINVAL;

	CAM_DBG(CAM_SYNC,
		"row name:%s sync_id:%i [idx:%u] row_state:%u",
		row->name, row->sync_id, idx, row->state);

	spin_lock_bh(&sync_dev->row_spinlocks[idx]);
	if (row->state == CAM_SYNC_STATE_INVALID) {
		spin_unlock_bh(&sync_dev->row_spinlocks[idx]);
		CAM_ERR(CAM_SYNC,
			"Error: accessing an uninitialized sync obj: idx = %d",
			idx);
		return -EINVAL;
	}

	if (row->state == CAM_SYNC_STATE_ACTIVE)
		CAM_DBG(CAM_SYNC,
			"Destroying an active sync object name:%s id:%i",
			row->name, row->sync_id);

	row->state = CAM_SYNC_STATE_INVALID;

	/* Object's child and parent objects will be added into this list */
	INIT_LIST_HEAD(&temp_child_list);
	INIT_LIST_HEAD(&temp_parent_list);

	list_for_each_entry_safe(child_info, temp_child, &row->children_list,
		list) {
		if (child_info->sync_id <= 0)
			continue;

		list_del_init(&child_info->list);
		list_add_tail(&child_info->list, &temp_child_list);
	}

	list_for_each_entry_safe(parent_info, temp_parent, &row->parents_list,
		list) {
		if (parent_info->sync_id <= 0)
			continue;

		list_del_init(&parent_info->list);
		list_add_tail(&parent_info->list, &temp_parent_list);
	}

	spin_unlock_bh(&sync_dev->row_spinlocks[idx]);

	/* Cleanup the child to parent link from child list */
	while (!list_empty(&temp_child_list)) {
		child_info = list_first_entry(&temp_child_list,
			struct sync_child_info, list);
		child_row = sync_dev->sync_table + child_info->sync_id;

		spin_lock_bh(&sync_dev->row_spinlocks[child_info->sync_id]);

		if (child_row->state == CAM_SYNC_STATE_INVALID) {
			list_del_init(&child_info->list);
			spin_unlock_bh(&sync_dev->row_spinlocks[
				child_info->sync_id]);
			kfree(child_info);
			continue;
		}

		if (child_row->state == CAM_SYNC_STATE_ACTIVE)
			CAM_DBG(CAM_SYNC,
				"Warning: destroying active child sync obj = %d",
				child_info->sync_id);

		cam_sync_util_cleanup_parents_list(child_row,
			SYNC_LIST_CLEAN_ONE, idx);

		list_del_init(&child_info->list);
		spin_unlock_bh(&sync_dev->row_spinlocks[child_info->sync_id]);
		kfree(child_info);
	}

	/* Cleanup the parent to child link */
	while (!list_empty(&temp_parent_list)) {
		parent_info = list_first_entry(&temp_parent_list,
			struct sync_parent_info, list);
		parent_row = sync_dev->sync_table + parent_info->sync_id;

		spin_lock_bh(&sync_dev->row_spinlocks[parent_info->sync_id]);

		if (parent_row->state == CAM_SYNC_STATE_INVALID) {
			list_del_init(&parent_info->list);
			spin_unlock_bh(&sync_dev->row_spinlocks[
				parent_info->sync_id]);
			kfree(parent_info);
			continue;
		}

		if (parent_row->state == CAM_SYNC_STATE_ACTIVE)
			CAM_DBG(CAM_SYNC,
				"Warning: destroying active parent sync obj = %d",
				parent_info->sync_id);

		cam_sync_util_cleanup_children_list(parent_row,
			SYNC_LIST_CLEAN_ONE, idx);

		list_del_init(&parent_info->list);
		spin_unlock_bh(&sync_dev->row_spinlocks[parent_info->sync_id]);
		kfree(parent_info);
	}

	spin_lock_bh(&sync_dev->row_spinlocks[idx]);
	list_for_each_entry_safe(upayload_info, temp_upayload,
			&row->user_payload_list, list) {
		list_del_init(&upayload_info->list);
		kfree(upayload_info);
	}

	list_for_each_entry_safe(sync_cb, temp_cb,
			&row->callback_list, list) {
		list_del_init(&sync_cb->list);
		kfree(sync_cb);
	}

	memset(row, 0, sizeof(*row));
	clear_bit(idx, sync_dev->bitmap);
	INIT_LIST_HEAD(&row->callback_list);
	INIT_LIST_HEAD(&row->parents_list);
	INIT_LIST_HEAD(&row->children_list);
	INIT_LIST_HEAD(&row->user_payload_list);
	spin_unlock_bh(&sync_dev->row_spinlocks[idx]);

	CAM_DBG(CAM_SYNC, "Destroying sync obj:%d successful", idx);
	return 0;
}

void cam_sync_util_cb_dispatch(struct work_struct *cb_dispatch_work)
{
	struct sync_callback_info *cb_info = container_of(cb_dispatch_work,
		struct sync_callback_info,
		cb_dispatch_work);

	cb_info->callback_func(cb_info->sync_obj,
		cb_info->status,
		cb_info->cb_data);

	kfree(cb_info);
}

void cam_sync_util_dispatch_signaled_cb(int32_t sync_obj,
	uint32_t status)
{
	struct sync_callback_info  *sync_cb;
	struct sync_user_payload   *payload_info;
	struct sync_callback_info  *temp_sync_cb;
	struct sync_table_row      *signalable_row;
	struct sync_user_payload   *temp_payload_info;

	signalable_row = sync_dev->sync_table + sync_obj;
	if (signalable_row->state == CAM_SYNC_STATE_INVALID) {
		CAM_DBG(CAM_SYNC,
			"Accessing invalid sync object:%i", sync_obj);
		return;
	}

	/* Dispatch kernel callbacks if any were registered earlier */
	list_for_each_entry_safe(sync_cb,
		temp_sync_cb, &signalable_row->callback_list, list) {
		sync_cb->status = status;
		list_del_init(&sync_cb->list);
		queue_work(sync_dev->work_queue,
			&sync_cb->cb_dispatch_work);
	}

	/* Dispatch user payloads if any were registered earlier */
	list_for_each_entry_safe(payload_info, temp_payload_info,
		&signalable_row->user_payload_list, list) {
		spin_lock_bh(&sync_dev->cam_sync_eventq_lock);
		if (!sync_dev->cam_sync_eventq) {
			spin_unlock_bh(
				&sync_dev->cam_sync_eventq_lock);
			break;
		}
		spin_unlock_bh(&sync_dev->cam_sync_eventq_lock);
		cam_sync_util_send_v4l2_event(
			CAM_SYNC_V4L_EVENT_ID_CB_TRIG,
			sync_obj,
			status,
			payload_info->payload_data,
			CAM_SYNC_PAYLOAD_WORDS * sizeof(__u64));

		list_del_init(&payload_info->list);
		/*
		 * We can free the list node here because
		 * sending V4L event will make a deep copy
		 * anyway
		 */
		 kfree(payload_info);
	}

	/*
	 * This needs to be done because we want to unblock anyone
	 * who might be blocked and waiting on this sync object
	 */
	complete_all(&signalable_row->signaled);
}

void cam_sync_util_send_v4l2_event(uint32_t id,
	uint32_t sync_obj,
	int status,
	void *payload,
	int len)
{
	struct v4l2_event event;
	__u64 *payload_data = NULL;
	struct cam_sync_ev_header *ev_header = NULL;

	event.id = id;
	event.type = CAM_SYNC_V4L_EVENT;

	ev_header = CAM_SYNC_GET_HEADER_PTR(event);
	ev_header->sync_obj = sync_obj;
	ev_header->status = status;

	payload_data = CAM_SYNC_GET_PAYLOAD_PTR(event, __u64);
	memcpy(payload_data, payload, len);

	v4l2_event_queue(sync_dev->vdev, &event);
	CAM_DBG(CAM_SYNC, "send v4l2 event for sync_obj :%d",
		sync_obj);
}

int cam_sync_util_update_parent_state(struct sync_table_row *parent_row,
	int new_state)
{
	int rc = 0;

	switch (parent_row->state) {
	case CAM_SYNC_STATE_ACTIVE:
	case CAM_SYNC_STATE_SIGNALED_SUCCESS:
		parent_row->state = new_state;
		break;

	case CAM_SYNC_STATE_SIGNALED_ERROR:
		break;

	case CAM_SYNC_STATE_INVALID:
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

void cam_sync_util_cleanup_children_list(struct sync_table_row *row,
	uint32_t list_clean_type, uint32_t sync_obj)
{
	struct sync_child_info *child_info = NULL;
	struct sync_child_info *temp_child_info = NULL;
	uint32_t                curr_sync_obj;

	list_for_each_entry_safe(child_info,
			temp_child_info, &row->children_list, list) {
		if ((list_clean_type == SYNC_LIST_CLEAN_ONE) &&
			(child_info->sync_id != sync_obj))
			continue;

		curr_sync_obj = child_info->sync_id;
		list_del_init(&child_info->list);
		kfree(child_info);

		if ((list_clean_type == SYNC_LIST_CLEAN_ONE) &&
			(curr_sync_obj == sync_obj))
			break;
	}
}

void cam_sync_util_cleanup_parents_list(struct sync_table_row *row,
	uint32_t list_clean_type, uint32_t sync_obj)
{
	struct sync_parent_info *parent_info = NULL;
	struct sync_parent_info *temp_parent_info = NULL;
	uint32_t                 curr_sync_obj;

	list_for_each_entry_safe(parent_info,
			temp_parent_info, &row->parents_list, list) {
		if ((list_clean_type == SYNC_LIST_CLEAN_ONE) &&
			(parent_info->sync_id != sync_obj))
			continue;

		curr_sync_obj = parent_info->sync_id;
		list_del_init(&parent_info->list);
		kfree(parent_info);

		if ((list_clean_type == SYNC_LIST_CLEAN_ONE) &&
			(curr_sync_obj == sync_obj))
			break;
	}
}
