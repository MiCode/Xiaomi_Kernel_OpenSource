/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __SYNX_UTIL_H__
#define __SYNX_UTIL_H__

#include "synx_private.h"

extern struct synx_device *synx_dev;
extern spinlock_t camera_tbl_lock;
extern spinlock_t global_tbl_lock;

extern void synx_fence_callback(struct dma_fence *fence,
	struct dma_fence_cb *cb);

/**
 * @brief: Function to check if the external obj type is valid
 *
 * @param type : External sync object type
 *
 * @return True if valid. False otherwise
 */
static inline bool synx_util_is_valid_bind_type(u32 type)
{
	if (type < SYNX_MAX_BIND_TYPES)
		return true;

	return false;
}

/**
 * @brief: Function to get the synx object type
 *
 * @param synx_obj : Pointer to synx object
 *
 * @return Synx object type. Zero if invalid
 */
static inline u32 synx_util_get_object_type(
	struct synx_coredata *synx_obj)
{
	return synx_obj->type;
}

/**
 * @brief: Function to get the client metadata index from session id
 *
 * @param client_id : Client session id
 *
 * @return Client metadata index.
 */
static inline u32 synx_util_client_index(u32 client_id)
{
	return (client_id & SYNX_CLIENT_HANDLE_MASK);
}

/**
 * @brief: Function to get the client session id from synx handle
 *
 * @param h_synx : Synx object handle
 *
 * @return Client metadata index.
 */
static inline u32 synx_util_client_id(s32 h_synx)
{
	return ((h_synx >> SYNX_CLIENT_ENCODE_SHIFT) &
		SYNX_CLIENT_ENCODE_MASK);
}

/**
 * @brief: Function to get the client synx table index from handle
 *
 * @param h_synx : Synx object handle
 *
 * @return Synx object index.
 */
static inline u32 synx_util_handle_index(s32 h_synx)
{
	return (h_synx & SYNX_OBJ_HANDLE_MASK);
}

/**
 * @brief: Function to check if the synx object is a composite (merged)
 *
 * @param synx_obj : Pointer to synx object
 *
 * @return True if merged object. False otherwise
 */
static inline bool synx_util_is_merged_object(
	struct synx_coredata *synx_obj)
{
	if (synx_obj->type & SYNX_FLAG_MERGED_FENCE)
		return true;

	return false;
}

/**
 * @brief: Function to check if object is of global type
 *
 * @param synx_obj : Pointer to synx object
 *
 * @return True if global object. False otherwise.
 */
static inline bool synx_util_is_global_object(
	struct synx_coredata *synx_obj)
{
	if (synx_obj->type & SYNX_FLAG_GLOBAL_FENCE)
		return true;

	return false;
}

/**
 * @brief: Function to check if object is of external type
 *
 * The dma fence backing the external synx object is created and
 * managed outside the synx framework.
 *
 * @param synx_obj : Pointer to synx object
 *
 * @return True if external object. False otherwise.
 */
static inline bool synx_util_is_external_object(
	struct synx_coredata *synx_obj)
{
	if (synx_obj->type & SYNX_FLAG_EXTERNAL_FENCE)
		return true;

	return false;
}

/**
 * @brief: Function to acquire reference to synx object
 *
 * This acquires additional reference to the synx object and should be
 * released using synx_util_put_object
 *
 * @param synx_obj : Pointer to synx object
 */
void synx_util_get_object(struct synx_coredata *synx_obj);

/**
 * @brief: Function to release reference to synx object
 *
 * This releases reference to the synx object and should be
 * matched with synx_util_get_object
 *
 * @param synx_obj : Pointer to synx object
 */
void synx_util_put_object(struct synx_coredata *synx_obj);

/**
 * @brief: Function to initialize synx object
 *
 * It also initializes the dma fence. This should be called only
 * for individual objects.
 *
 * @param synx_obj : Pointer to synx object
 * @param params   : Pointer to create params
 * @param ops      : Dma fence ops required for fence initialization
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_util_init_coredata(struct synx_coredata *synx_obj,
	struct synx_create_params *params,
	struct dma_fence_ops *ops);

/**
 * @brief: Function to initialize a merged synx object.
 *
 * It also initializes dma fence array.
 *
 * @param synx_obj : Pointer to synx object
 * @param fences   : Array of fence objects which will merged
 *                   or grouped together to a fence array
 * @param num_objs : Number of fence objects in the array
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_util_init_group_coredata(struct synx_coredata *synx_obj,
	struct dma_fence **fences,
	u32 num_objs);

/**
 * @brief: Function to cleanup the synx object
 *
 * This function will be invoked when the dma fence ref count reaches 0.
 *
 * @param synx_obj : Pointer to synx object
 */
void synx_util_object_destroy(struct synx_coredata *synx_obj);

/**
 * @brief: Function to find a free index from the bitmap
 *
 * @param bitmap : Pointer to bitmap
 * @param size   : Max index
 *
 * @return Free index if available.
 */
long synx_util_get_free_handle(unsigned long *bitmap, unsigned int size);

/**
 * @brief: Function to allocate synx_client_cb entry from cb table
 *
 * @param client : Pointer to client session info
 * @param data   : Kernel payload data
 * @param cb_idx : Allocated index (filled by function)
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_util_alloc_cb_entry(struct synx_client *client,
	struct synx_kernel_payload *data,
	u32 *cb_idx);

/**
 * @brief: Function to clean up synx_client_cb entry from cb table
 *
 * @param client : Pointer to client session info
 * @param cb     : Pointer to client cb entry
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_util_clear_cb_entry(struct synx_client *client,
	struct synx_client_cb *cb);

/**
 * @brief: Function to initialize synx object handle for the client
 *
 * @param client   : Pointer to client session info
 * @param synx_obj : Pointer to synx object
 * @param h_synx   : Pointer to synx object handle (filled by function)
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_util_init_handle(struct synx_client *client,
	struct synx_coredata *synx_obj,
	long *h_synx);

/**
 * @brief: Function to activate the synx object. Moves the synx from INVALID
 * state to ACTIVE state.
 *
 * @param synx_obj : Pointer to synx object
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_util_activate(struct synx_coredata *synx_obj);

/**
 * @brief: Default kernel callback function to handle userspace callbacks
 *
 * @param h_synx : Synx object handle
 * @param status : Synx object state
 * @param data   : Opaque pointer
 */
void synx_util_default_user_callback(s32 h_synx, int status, void *data);

/**
 * @brief: Function to queue all the registered callbacks by clients for
 * the synx object
 *
 * @param synx_obj : Pointer to synx object
 * @param state    : State of the object
 */
void synx_util_callback_dispatch(struct synx_coredata *synx_obj, u32 state);

/**
 * @brief: Function to dispatch client registered callback
 *
 * @param cb_dispatch : Pointer to work_struct that needs to be dispatched
 */
void synx_util_cb_dispatch(struct work_struct *cb_dispatch);

/**
 * @brief: Function to handle error during merging of synx objects
 *
 * Removes the references added on the dma fence objects
 *
 * @param client   : Pointer to client session
 * @param h_synxs  : Array of synx object handles to merge
 * @param num_objs : Number of synx obects in the array
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
s32 synx_util_merge_error(struct synx_client *client,
	s32 *h_synxs,
	u32 num_objs);

/**
 * @brief: Function to validate synx merge arguments.
 *
 * It obtains the necessary references to the fence objects
 * and also removes duplicates (if any).
 *
 * @param client    : Pointer to client session
 * @param h_synxs   : Array of synx object handles to merge
 * @param num_objs  : Number of synx objects in the array
 * @param fences    : Address to a list of dma fence* array
 * @param fence_cnt : Number of fence objects in the fences array
 *                    (filled by the function)
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_util_validate_merge(struct synx_client *client,
	s32 *h_synxs,
	u32 num_objs,
	struct dma_fence ***fences,
	u32 *fence_cnt);

/**
 * @brief: Function to obtain the state of synx object
 *
 * The lock associated with the synx obj should not be
 * held when invoking this function.
 * Use synx_util_get_object_status_locked for state enquiry
 * when holding lock
 *
 * @param synx_obj : Pointer to synx object
 *
 * @return Status of the synx object.
 */
u32 synx_util_get_object_status(struct synx_coredata *synx_obj);

/**
 * @brief: Function to obtain the state of synx object
 *
 * The lock associated with the synx obj should be held
 * when invoking this function.
 * Use synx_util_get_object_status for state enquiry
 * when not holding lock
 *
 * @param synx_obj : Pointer to synx object
 *
 * @return Status of the synx object.
 */
u32 synx_util_get_object_status_locked(struct synx_coredata *synx_obj);

/**
 * @brief: Function to acquire the synx object handle
 *
 * This function increments the reference count of synx handle data.
 * The reference should be released by calling synx_util_release_handle
 *
 * @param client : Pointer to client session
 * @param h_synx : Synx object handle
 *
 * @return Pointer to synx handle data on success. NULL otherwise.
 */
struct synx_handle_coredata *synx_util_acquire_handle(
	struct synx_client *client, s32 h_synx);

/**
 * @brief: Function to update synx object handle - coredata mapping
 *
 * Function used during bind to ensure that all the external fence
 * id/s are mapped to same coredata structure, thereby avoiding
 * any roundtrip delays.
 *
 * @param client  : Pointer to client session
 * @param h_synx  : Synx object handle
 * @param sync_id : External fence id
 * @param type    : External fence type
 * @param handle  : Address of synx handle coredata pointer
 *                  If the sync id entry is not available, then
 *                  handle will contain valid pointer. If duplicate,
 *                  entry, handle will be NULL.
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_util_update_handle(struct synx_client *client,
	s32 h_synx, u32 sync_id, u32 type,
	struct synx_handle_coredata **handle);

/**
 * @brief: Function to obtain the synx object
 *
 * @param synx_data : Pointer to synx object handle data
 *
 * @return Pointer to synx object on success. NULL otherwise.
 */
static inline struct synx_coredata *synx_util_obtain_object(
	struct synx_handle_coredata *synx_data) {
	if (!synx_data)
		return NULL;

	return synx_data->synx_obj;
}

/**
 * @brief: Function to release the synx object handle
 *
 * This function decrements the reference count of the synx handle and
 * implicitly calls up the synx object release if count reaches 0.
 * Synx handle data should not be derefrenced in the function after
 * calling this.
 *
 * @param synx_data : Pointer to synx object handle data
 */
void synx_util_release_handle(struct synx_handle_coredata *synx_data);

/**
 * @brief: Function called implicitly when import refcount reaches zero
 *
 * @param kref : &synx_handle_coredata.import_refcount
 */
void synx_util_destroy_import_handle(struct kref *kref);

/**
 * @brief: Function called implicitly when internal refcount reaches zero
 *
 * @param kref : &synx_handle_coredata.internal_refcount
 */
void synx_util_destroy_internal_handle(struct kref *kref);

/**
 * @brief: Function to get the bind operations registered by external driver
 *
 * @param type : External sync type
 *
 * @return Bind operations. NULL if type is invalid
 */
struct bind_operations *synx_util_get_bind_ops(u32 type);

/**
 * @brief: Function to share the synx object to use by other clients
 *
 * Shares the synx object by acquiring additional reference to dma fence
 *
 * @param client : Pointer to client session
 * @param params : Pointer to export params struct
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_util_export_local(struct synx_client *client,
	struct synx_export_params *params);

/**
 * @brief: Function to share the synx object to use by other cores
 *
 * Shares the synx object by sending a message
 *
 * @param client : Pointer to client session
 * @param params : Pointer to export params struct
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_util_export_global(struct synx_client *client,
	struct synx_export_params *params);

/**
 * @brief: Function to import the synx object for use by the client
 *
 * Obtains the synx object corresponding to the importing synx handle
 *
 * @param params : Pointer to import params struct
 *
 * @return Synx object pointer on success. NULL if import failed or invalid
 */
struct synx_coredata *synx_util_import_object(
	struct synx_import_params *params);

/**
 * @brief: Function to acquire the synx client session
 *
 * This increments the reference count of the client in device client_table
 *
 * @param session_id : Client session id
 *
 * @return Pointer to synx client. NULL if invalid session id.
 */
struct synx_client *synx_get_client(struct synx_session session_id);

/**
 * @brief: Function to release the synx client session
 *
 * This decrements the reference count of the client in device client_table and
 * invokes the client session cleanup if reference reaches zero.
 *
 * @param client : Pointer to client session structure
 */
void synx_put_client(struct synx_client *client);

/**
 * @brief: Function to generate timestamp
 *
 * @param timestamp : Timestamp string (filled by function)
 * @param size      : Timestamp string size
 */
void synx_util_generate_timestamp(char *timestamp, size_t size);

/**
 * @brief: Function to log framework errors
 *
 * This saves the error in a global list and can be accessed using debugfs.
 * Needs to be added where we wish to trace the error.
 *
 * @param id     : Client session id
 * @param h_synx : Synx object handle
 * @param err    : Error code
 */
void synx_util_log_error(u32 id, s32 h_synx, s32 err);

/**
 * @brief: Function to initialize the hash tables
 *
 * Initialize the global key and camera id hash table.
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_util_init_table(void);

/**
 * @brief: Function to save data in hash table
 *
 * @param key  : Unique key for the entry
 * @param tbl  : Hash table to add the entry in
 * @param data : Data to be saved
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_util_save_data(u32 key, u32 tbl, void *data);

/**
 * @brief: Function to retrieve data from hash table
 *
 * @param key : Unique key to look up
 * @param tbl : Hash table to look in
 *
 * @return Hash entry if the key is present in the table. NULL otherwise.
 * The hash entry refcount should be released by the client explicitly.
 */
struct hash_key_data *synx_util_retrieve_data(u32 key, u32 tbl);

/**
 * @brief: Function to release data from hash table
 *
 * @param key : Unique key to look up
 * @param tbl : Hash table to look in
 *
 * @return Hash entry if the key is present in the table. NULL otherwise.
 * the entry is also removed from the hash table by this function.
 * The hash entry refcount should be released by the client explicitly.
 */
struct hash_key_data *synx_util_release_data(u32 key, u32 tbl);

/**
 * @brief: Function to free hash data entry
 */
void synx_util_destroy_data(struct kref *kref);

#endif /* __SYNX_UTIL_H__ */
