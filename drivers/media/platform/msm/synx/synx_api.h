/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef __SYNX_API_H__
#define __SYNX_API_H__

#include <linux/list.h>
#include <uapi/media/synx.h>

typedef void (*synx_callback)(s32 sync_obj, int status, void *data);

/**
 * struct bind_operations - Function pointers that need to be defined
 *    to achieve bind functionality for external fence with synx obj
 *
 * @register_callback   : Function to register with external sync object
 * @deregister_callback : Function to deregister with external sync object
 * @enable_signaling    : Function to enable the signaling on the external
 *                        sync object (optional)
 * @signal              : Function to signal the external sync object
 */
struct bind_operations {
	int (*register_callback)(synx_callback cb_func,
		void *userdata, s32 sync_obj);
	int (*deregister_callback)(synx_callback cb_func,
		void *userdata, s32 sync_obj);
	int (*enable_signaling)(s32 sync_obj);
	int (*signal)(s32 sync_obj, u32 status);
};

/**
 * struct synx_register_params - External registration parameters
 *
 * @ops  : Pointer to bind operations struct
 * @name : Client name
 *         Only first 32 bytes are accepted, rest will be ignored
 * @type : Synx external client type
 */
struct synx_register_params {
	struct bind_operations ops;
	char *name;
	u32 type;
};

/**
 * struct synx_initialization_params - Session params (optional)
 *
 * @name : Client session name
 *         Only first 64 bytes are accepted, rest will be ignored
 */
struct synx_initialization_params {
	const char *name;
};

/* Kernel APIs */

/* @brief: Register operations for external synchronization
 *
 * Register with synx for enabling external synchronization through bind
 *
 * @param params : Pointer to register params
 *
 * @return Status of operation. Zero in case of success.
 * -EINVAL will be returned if params are invalid.
 * -ENOMEM will be returned if client cannot be registered due to not
 * enough memory.
 * -EALREADY will be returned if client name is already in use
 */
int synx_register_ops(const struct synx_register_params *params);

/**
 * @brief: De-register external synchronization operations
 *
 * @param params : Pointer to register params
 *
 * @return Status of operation. Zero in case of success.
 * -EINVAL will be returned if client not found.
 */
int synx_deregister_ops(const struct synx_register_params *params);

/**
 * @brief: Initializes a new client session
 *
 * @param params : Pointer to session init params
 *
 * @return Status of operation. Zero in case of success.
 * -EINVAL will be returned if params is not valid.
 * -ENOMEM will be returned if the kernel can't allocate space for
 * new client
 */
int synx_initialize(struct synx_initialization_params *params);

/**
 * @brief: Destroys the client session
 *
 * @return Status of operation. Zero in case of success.
 */
int synx_uninitialize(void);

/**
 * @brief: Creates a synx object
 *
 *  The newly created synx obj is assigned to synx_obj.
 *
 * @param synx_obj : Pointer to synx object handle (filled by the function)
 * @param name     : Optional parameter associating a name with the synx
 *                   object for debug purposes.
 *                   Only first SYNC_DEBUG_NAME_LEN bytes are accepted,
 *                   rest will be ignored.
 *
 * @return Status of operation. Zero in case of success.
 * -EINVAL will be returned if synx_obj is an invalid pointer.
 * -ENOMEM will be returned if the kernel can't allocate space for
 * synx object.
 */
int synx_create(s32 *synx_obj, const char *name);

/**
 * @brief: Registers a callback with a synx object
 *
 * @param synx_obj : int referencing the synx object.
 * @param userdata : Opaque pointer passed back with callback.
 * @param cb_func  : Pointer to callback to be registered
 *
 * @return Status of operation. Zero in case of success.
 * -EINVAL will be returned if userdata is invalid.
 * -ENOMEM will be returned if cb_func is invalid.
 *
 */
int synx_register_callback(s32 synx_obj,
	void *userdata, synx_callback cb_func);

/**
 * @brief: De-registers a callback with a synx object
 *
 * @param synx_obj       : int referencing the synx object.
 * @param cb_func        : Pointer to callback to be de-registered
 * @param userdata       : Opaque pointer passed back with callback.
 * @param cancel_cb_func : Pointer to callback to ack de-registration
 *
 * @return Status of operation. Zero in case of success.
 * -EINVAL will be returned if userdata is invalid.
 * -ENOMEM will be returned if cb_func is invalid.
 */
int synx_deregister_callback(s32 synx_obj,
	synx_callback cb_func,
	void *userdata,
	synx_callback cancel_cb_func);

/**
 * @brief: Signals a synx object with the status argument.
 *
 * This function will signal the synx object referenced by the synx_obj
 * param and invoke any external binding synx objs.
 * The status parameter will indicate whether the entity
 * performing the signaling wants to convey an error case or a success case.
 *
 * @param synx_obj : Synx object handle
 * @param status   : Status of signaling. Value : SYNX_STATE_SIGNALED_SUCCESS or
 *                   SYNX_STATE_SIGNALED_ERROR.
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_signal(s32 synx_obj, u32 status);

/**
 * @brief: Merges multiple synx objects
 *
 * This function will merge multiple synx objects into a synx group.
 *
 * @param synxs    : Pointer to a block of synx handles to be merged
 * @param num_objs : Number of synx objs in the block
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_merge(s32 *synx_obj, u32 num_objs, s32 *merged_obj);

/**
 * @brief: Waits for a synx object synchronously
 *
 * Does a wait on the synx object identified by synx_obj for a maximum
 * of timeout_ms milliseconds. Must not be called from interrupt context as
 * this API can sleep. Should be called from process context only.
 *
 * @param synx_obj      : Synx object handle to be waited upon
 * @timeout_ms synx_obj : Timeout in ms.
 *
 * @return 0 upon success, -EINVAL if synx object is in bad state or arguments
 * are invalid, -ETIMEDOUT if wait times out.
 */
int synx_wait(s32 synx_obj, u64 timeout_ms);

/**
 * @brief: Binds two synx objects
 *
 * Binding two synx objects will unify them in a way that if one
 * of the signals, the other ones is signaled as well.
 *
 * @param synx_obj      : Synx object handle
 * @param external_sync : External synx descriptor to bind to object
 *
 * @return 0 upon success, -EINVAL if synx object is in bad state or arguments
 * are invalid, -ETIMEDOUT if wait times out.
 */
int synx_bind(s32 synx_obj,
	struct synx_external_desc external_sync);

/**
 * @brief: return the status of the synx object
 *
 * @param synx_obj : Synx object handle
 *
 * @return status of the synx object
 */
int synx_get_status(s32 synx_obj);

/**
 * @brief: Adds to the reference count of a synx object
 *
 * When a synx object is created, the refcount will be 1.
 *
 * @param synx_obj : Synx object handle
 * @param count    : Count to add to the refcount
 *
 * @return 0 upon success, -EINVAL if synx object is in bad
 *         state
 */
int synx_addrefcount(s32 synx_obj, s32 count);

/**
 * @brief: Imports (looks up) a synx object from a given ID
 *
 * The given ID should have been exported by another client
 * and provided.
 *
 * @param synx_obj     : Synx object handle to import
 * @param secure_key   : Key to verify authenticity
 * @param new_synx_obj : Pointer to newly created synx object
 *
 * @return 0 upon success, -EINVAL if synx object is bad
 */
int synx_import(s32 synx_obj, u32 secure_key, s32 *new_synx_obj);

/**
 * @brief: Exports a synx object and returns an ID
 *
 *  The given ID may be passed to other clients to be
 *  imported.
 *
 * @param synx_obj   : Synx object handle to export
 * @param secure_key : POinter to gnerated secure key
 *
 * @return 0 upon success, -EINVAL if the ID is bad
 */
int synx_export(s32 synx_obj, u32 *secure_key);

/**
 * @brief: Decrements refcount of a synx object, and destroys it
 *         if becomes 0
 *
 * @param synx_obj: Synx object handle to be destroyed
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_release(s32 synx_obj);

#endif /* __SYNX_API_H__ */
