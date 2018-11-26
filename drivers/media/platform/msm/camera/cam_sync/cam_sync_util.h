/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __CAM_SYNC_UTIL_H__
#define __CAM_SYNC_UTIL_H__


#include <cam_sync_api.h>
#include "cam_sync_private.h"
#include "cam_debug_util.h"

extern struct sync_device *sync_dev;

/**
 * @brief: Finds an empty row in the sync table and sets its corresponding bit
 * in the bit array
 *
 * @param sync_dev : Pointer to the sync device instance
 * @param idx      : Pointer to an long containing the index found in the bit
 *                   array
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_sync_util_find_and_set_empty_row(struct sync_device *sync_dev,
	long *idx);

/**
 * @brief: Function to initialize an empty row in the sync table. This should be
 *         called only for individual sync objects.
 *
 * @param table : Pointer to the sync objects table
 * @param idx   : Index of row to initialize
 * @param name  : Optional string representation of the sync object. Should be
 *                63 characters or less
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_sync_init_object(struct sync_table_row *table,
	uint32_t idx,
	const char *name);

/**
 * @brief: Function to uninitialize a row in the sync table
 *
 * @param table : Pointer to the sync objects table
 * @param idx   : Index of row to initialize
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_sync_deinit_object(struct sync_table_row *table, uint32_t idx);

/**
 * @brief: Function to initialize a row in the sync table when the object is a
 *         group object, also known as a merged sync object
 *
 * @param table     : Pointer to the sync objects table
 * @param idx       : Index of row to initialize
 * @param sync_objs : Array of sync objects which will merged
 *                    or grouped together
 * @param num_objs  : Number of sync objects in the array
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_sync_init_group_object(struct sync_table_row *table,
	uint32_t idx,
	uint32_t *sync_objs,
	uint32_t num_objs);

int cam_sync_deinit_object(struct sync_table_row *table, uint32_t idx);

/**
 * @brief: Function to dispatch a kernel callback for a sync callback
 *
 * @param cb_dispatch_work : Pointer to the work_struct that needs to be
 *                           dispatched
 *
 * @return None
 */
void cam_sync_util_cb_dispatch(struct work_struct *cb_dispatch_work);

/**
 * @brief: Function to send V4L event to user space
 * @param id       : V4L event id to send
 * @param sync_obj : Sync obj for which event needs to be sent
 * @param status   : Status of the event
 * @payload        : Payload that needs to be sent to user space
 * @len            : Length of the payload
 *
 * @return None
 */
void cam_sync_util_send_v4l2_event(uint32_t id,
	uint32_t sync_obj,
	int status,
	void *payload,
	int len);

/**
 * @brief: Function to validate sync merge arguments
 *
 * @param sync_obj : Array of sync objects to merge
 * @param num_objs : Number of sync objects in the array
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_sync_util_validate_merge(uint32_t *sync_obj, uint32_t num_objs);

/**
 * @brief: Function which adds sync object information to the signalable list
 *
 * @param sync_obj : Sync object to add
 * @param status   : Status of above sync object
 * @param list     : Linked list where the information should be added to
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_sync_util_add_to_signalable_list(int32_t sync_obj,
	uint32_t status,
	struct list_head *sync_list);

/**
 * @brief: Function which gets the next state of the sync object based on the
 *         current state and the new state
 *
 * @param current_state : Current state of the sync object
 * @param new_state     : New state of the sync object
 *
 * @return Next state of the sync object
 */
int cam_sync_util_get_state(int current_state,
	int new_state);

/**
 * @brief: Function to clean up the children of a sync object
 * @row                 : Row whose child list to clean
 * @list_clean_type     : Clean specific object or clean all objects
 * @sync_obj            : Sync object to be clean if list clean type is
 *                          SYNC_LIST_CLEAN_ONE
 *
 * @return None
 */
void cam_sync_util_cleanup_children_list(struct sync_table_row *row,
	uint32_t list_clean_type, uint32_t sync_obj);

/**
 * @brief: Function to clean up the parents of a sync object
 * @row                 : Row whose parent list to clean
 * @list_clean_type     : Clean specific object or clean all objects
 * @sync_obj            : Sync object to be clean if list clean type is
 *                          SYNC_LIST_CLEAN_ONE
 *
 * @return None
 */
void cam_sync_util_cleanup_parents_list(struct sync_table_row *row,
	uint32_t list_clean_type, uint32_t sync_obj);

#endif /* __CAM_SYNC_UTIL_H__ */
