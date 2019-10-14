/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef __SYNX_UTIL_H__
#define __SYNX_UTIL_H__

#include "synx_private.h"

extern struct synx_device *synx_dev;

bool synx_debugfs_enabled(void);

/**
 * @brief: Function to check if the external sync obj is valid
 *
 * @param type : External sync object type
 *
 * @return True if valid. False otherwise
 */
bool is_valid_type(u32 type);

/**
 * @brief: Function to initialize an empty row in the synx table.
 *         It also initializes dma fence.
 *         This should be called only for individual objects.
 *
 * @param table : Pointer to the synx objects table
 * @param idx   : Index of row to initialize
 * @param id    : Id associated with the object
 * @param name  : Optional string representation of the synx object. Should be
 *                63 characters or less
 * @param ops   : dma fence ops required for fence initialization
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_init_object(struct synx_table_row *table,
	u32 idx,
	s32 id,
	const char *name,
	struct dma_fence_ops *ops);

/**
 * @brief: Function to uninitialize a row in the synx table.
 *
 * @param row : Pointer to the synx object row
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_deinit_object(struct synx_table_row *row);

/**
 * @brief: Function to initialize a row in the synx table when the object is a
 *         a merged synx object. It also initializes dma fence array.
 *
 * @param table    : Pointer to the synx objects table
 * @param idx      : Index of row to initialize
 * @param id       : Id associated with the object
 * @param fences   : Array of fence objects which will merged
 *                   or grouped together to a fence array
 * @param num_objs : Number of fence objects in the array
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_init_group_object(struct synx_table_row *table,
	u32 idx,
	s32 id,
	struct dma_fence **fences,
	u32 num_objs);

/**
 * @brief: Function to activate the synx object. Moves the synx from INVALID
 *         state to ACTIVE state.
 *
 * @param row : Pointer to the synx object row
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_activate(struct synx_table_row *row);

/**
 * @brief: Function to dispatch callbacks registered with
 *         the synx object.
 *
 * @param row : Pointer to the synx object row
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
void synx_callback_dispatch(struct synx_table_row *row);

/**
 * @brief: Function to handle error during group synx obj initialization.
 *         Removes the references added on the fence objects.
 *
 * @param synx_objs : Array of synx objects to merge
 * @param num_objs  : Number of synx obects in the array
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
s32 synx_merge_error(s32 *synx_objs, u32 num_objs);

/**
 * @brief: Function to validate synx merge arguments. It obtains the necessary
 *         references to the fence objects and also removes duplicates (if any).
 *
 * @param synx_objs : Array of synx objects to merge
 * @param num_objs  : Number of synx objects in the array
 * @param fences    : Address to a list of dma fence* array
 * @param fence_cnt : Number of fence objects in the fences array
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_util_validate_merge(s32 *synx_objs,
	u32 num_objs,
	struct dma_fence ***fences,
	u32 *fence_cnt);

/**
 * @brief: Function to dispatch a kernel callback for a sync callback
 *
 * @param cb_dispatch_work : Pointer to the work_struct that needs to be
 *                           dispatched
 *
 * @return None
 */
void synx_util_cb_dispatch(struct work_struct *cb_dispatch_work);

/**
 * @brief: Function to check if the synx is a merged (grouped) object
 *
 * @param row : Pointer to the synx object row
 *
 * @return True if merged object. Otherwise false.
 */
bool is_merged_synx(struct synx_table_row *row);

/**
 * @brief: Function to check the state of synx object.
 *         The row lock associated with the synx obj should not be
 *         held when invoking this function.
 *         Use synx_status_locked for state enquiry when holding lock.
 *
 * @param row : Pointer to the synx object row
 *
 * @return Status of the synx object.
 */
u32 synx_status(struct synx_table_row *row);

/**
 * @brief: Function to check the state of synx object.
 *         The row lock associated with the synx obj should be
 *         held when invoking this function.
 *         Use synx_status for state enquiry when not holding lock.
 *
 * @param row : Pointer to the synx object row
 *
 * @return Status of the synx object.
 */
u32 synx_status_locked(struct synx_table_row *row);

/**
 * @brief: Function to look up a synx handle
 *         It also verifies the authenticity of the request through
 *         the key provided.
 *
 * @param synx_id    : Synx handle
 * @param secure_key : Key to verify authenticity

 * @return The synx table entry corresponding to the given synx ID
 */
void *synx_from_key(s32 synx_id, u32 secure_key);

/**
 * @brief: Function to look up a synx handle using the backed dma fence
 *
 * @param fence : dma fence backing the synx object

 * @return The synx table entry corresponding to the given dma fence.
 * NULL otherwise.
 */
struct synx_table_row *synx_from_fence(struct dma_fence *fence);

/**
 * @brief: Function to look up a synx handle
 *
 * @param synx_id : Synx handle
 *
 * @return The synx corresponding to the given handle or NULL if
 *         handle is invalid (or not permitted).
 */
void *synx_from_handle(s32 synx_id);

/**
 * @brief: Function to create a new synx handle
 *
 * @param pObj : Object to be associated with the created handle
 *
 * @return The created handle
 */
s32 synx_create_handle(void *pObj);

/**
 * @brief: Function to retrieve the bind ops for external sync
 *
 * @param type : External sync type
 *
 * @return Bind operations registered by external sync.
 * NULL otherwise.
 */
struct bind_operations *synx_get_bind_ops(u32 type);

/**
 * @brief: Function to generate a secure key for authentication
 *         Used to verify the requests generated on synx objects
 *         not owned by the process.
 *
 * @param row : Pointer to the synx object row
 *
 * @return The created handle
 */
int synx_generate_secure_key(struct synx_table_row *row);

/**
 * @brief: Function to generate a key for authenticating requests
 *         Generated key for synx object being exported is
 *         verified during import.
 *
 * @param row      : Pointer to the synx object row
 * @param synx_obj : Synx handle
 * @param key      : Pointer to key (filled by the function)
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_generate_import_key(struct synx_table_row *row,
	s32 synx_obj,
	u32 *key);

/**
 * @brief: Function to authenticate requests for importing synx handle
 *         Used to verify the requests generated on synx object
 *         being imported.
 *
 * @param synx_obj : Synx handle being imported
 * @param key      : Key to authenticate import request
 *
 * @return Pointer to the synx object row for valid request. NULL otherwise.
 */
struct synx_table_row *synx_from_import_key(s32 synx_obj, u32 key);

/**
 * @brief: Function to handle adding an error
 *         code to a synx
 *
 * @param error_code : error to add
 *
 * @param synx_obj : synx_obj to add the error to
 */
void log_synx_error(s32 error_code, s32 synx_obj);

#endif /* __SYNX_UTIL_H__ */
