/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef __SYNX_API_H__
#define __SYNX_API_H__

#include <linux/list.h>
#include <uapi/media/synx.h>

/**
 * SYNX_FLAG_GLOBAL_FENCE   : Creates a global synx object
 *                            If flag not set, creates local synx object
 * SYNX_FLAG_EXTERNAL_FENCE : Creates an synx object with external fence
 */
enum synx_flags {
	SYNX_FLAG_GLOBAL_FENCE = 0x1,
	SYNX_FLAG_MERGED_FENCE = 0x2,
	SYNX_FLAG_EXTERNAL_FENCE = 0x4,
	SYNX_FLAG_MAX = 0x8,
};

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
 * @ops  : Bind operations struct
 * @name : External client name
 *         Only first 64 bytes are accepted, rest will be ignored
 * @type : Synx external client type
 */
struct synx_register_params {
	struct bind_operations ops;
	char *name;
	u32 type;
};

/**
 * struct synx_session - Client session handle
 *
 * @client_id : Client id
 */
struct synx_session {
	u32 client_id;
};

/**
 * struct synx_initialization_params - Session params
 *
 * @name : Client session name
 *         Only first 64 bytes are accepted, rest will be ignored
 */
struct synx_initialization_params {
	const char *name;
};

/**
 * struct synx_create_params - Synx creation parameters
 *
 * @h_synx : Pointer to synx object handle (filled by function)
 * @type   : Synx flags for customization
 * @name   : Optional parameter associating a name with the synx
 *           object for debug purposes
 *           Only first 64 bytes are accepted,
 *           rest will be ignored
 */
struct synx_create_params {
	s32 *h_synx;
	u32 type;
	const char *name;
};

/**
 * struct synx_export_params - Synx export parameters
 *
 * @h_synx     : Synx object handle to export
 * @secure_key : Pointer to Key generated for authentication
 *               (filled by the function)
 * @fence      : Pointer to dma fence for external synx object
 */
struct synx_export_params {
	s32 h_synx;
	u32 *secure_key;
	struct dma_fence *fence;
};

/**
 * struct synx_import_params - Synx import parameters
 *
 * @h_synx     : Synx object handle to import
 * @secure_key : Key to verify authenticity
 * @new_h_synx : Pointer to newly created synx object
 *               (filled by the function)
 *               This handle should be used by importing
 *               process for all synx api operations
 */
struct synx_import_params {
	s32 h_synx;
	u32 secure_key;
	s32 *new_h_synx;
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
 * -ENOMEM will be returned if bind ops cannot be registered due to
 * insufficient memory.
 * -EALREADY will be returned if type already in use
 */
int synx_register_ops(const struct synx_register_params *params);

/**
 * @brief: De-register external synchronization operations
 *
 * @param params : Pointer to register params
 *
 * @return Status of operation. Zero in case of success.
 * -EINVAL will be returned if record not found.
 */
int synx_deregister_ops(const struct synx_register_params *params);

/**
 * @brief: Initializes a new client session
 *
 * Create a new client and provides access to the synx framework
 *
 * @param session_id : Pointer to client session id (filled by function)
 * @param params     : Pointer to session init params
 *
 * @return Status of operation. Zero in case of success.
 * -EINVAL will be returned if params is not valid.
 * -ENOMEM will be returned if the kernel can't allocate space for
 * new client
 */
int synx_initialize(struct synx_session *session_id,
	struct synx_initialization_params *params);

/**
 * @brief: Destroys the client session
 *
 * The client session is destroyed and all synx objects owned
 * by the client are cleaned up.
 *
 * @param session_id : Client session id
 *
 * @return Status of operation. Zero in case of success.
 */
int synx_uninitialize(struct synx_session session_id);

/**
 * @brief: Creates a synx object
 *
 *  Creates a new synx obj and returns the handle to client
 *
 * @param session_id : Client session id
 * @param params     : Pointer to create params
 *
 * @return Status of operation. Zero in case of success.
 * -EINVAL will be returned if params were invalid.
 * -ENOMEM will be returned if the kernel can't allocate space for
 * synx object.
 */
int synx_create(struct synx_session session_id,
	struct synx_create_params *params);

/**
 * @brief: Registers a callback with a synx object
 *
 * @param session_id : Client session id
 * @param h_synx     : Synx object handle
 * @param cb_func    : Pointer to callback to be registered
 * @param userdata   : Opaque pointer passed back with callback
 *
 * @return Status of operation. Zero in case of success.
 * -EINVAL will be returned if userdata is invalid.
 * -ENOMEM will be returned if cb_func is invalid.
 *
 */
int synx_register_callback(struct synx_session session_id,
	s32 h_synx, synx_callback cb_func, void *userdata);

/**
 * @brief: De-registers a callback with a synx object
 *
 * @param session_id     : Client session id
 * @param h_synx         : Synx object handle
 * @param cb_func        : Pointer to callback to be de-registered
 * @param userdata       : Opaque pointer passed back with callback
 * @param cancel_cb_func : Pointer to callback to ack de-registration
 *
 * @return Status of operation. Zero in case of success.
 * -EINVAL will be returned if userdata is invalid.
 * -ENOMEM will be returned if cb_func is invalid.
 */
int synx_deregister_callback(struct synx_session session_id,
	s32 h_synx,
	synx_callback cb_func,
	void *userdata,
	synx_callback cancel_cb_func);

/**
 * @brief: Signals a synx object with the status argument.
 *
 * This function will signal the synx object referenced by h_synx
 * and invoke any external binding synx objs.
 * The status parameter will indicate whether the entity
 * performing the signaling wants to convey an error case or a success case.
 *
 * @param session_id : Client session id
 * @param h_synx     : Synx object handle
 * @param status     : Status of signaling.
 *                     Value : SYNX_STATE_SIGNALED_SUCCESS or
 *                     SYNX_STATE_SIGNALED_ERROR.
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_signal(struct synx_session session_id, s32 h_synx, u32 status);

/**
 * @brief: Merges multiple synx objects
 *
 * This function will merge multiple synx objects into a synx group.
 *
 * @param session_id   : Client session id
 * @param h_synxs      : Pointer to a block of synx handles to be merged
 * @param num_objs     : Number of synx objs in the block
 * @param h_merged_obj : Merged synx object handle (filled by function)
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_merge(struct synx_session session_id,
	s32 *h_synxs, u32 num_objs, s32 *h_merged_obj);

/**
 * @brief: Waits for a synx object synchronously
 *
 * Does a wait on the synx object identified by h_synx for a maximum
 * of timeout_ms milliseconds. Must not be called from interrupt context as
 * this API can sleep. Should be called from process context only.
 *
 * @param session_id : Client session id
 * @param h_synx     : Synx object handle to be waited upon
 * @param timeout_ms : Timeout in ms
 *
 * @return 0 upon success, -EINVAL if synx object is in bad state or arguments
 * are invalid, -ETIMEDOUT if wait times out.
 */
int synx_wait(struct synx_session session_id, s32 h_synx, u64 timeout_ms);

/**
 * @brief: Binds synx object with external sync
 *
 * Binding synx objects with supported external sync object will unify them
 * in a way that if one of them signals, the other ones is signaled as well
 *
 * @param session_id    : Client session id
 * @param h_synx        : Synx object handle
 * @param external_sync : External synx descriptor to bind to object
 *
 * @return 0 upon success, -EINVAL if synx object is in bad state or arguments
 * are invalid.
 */
int synx_bind(struct synx_session session_id, s32 h_synx,
	struct synx_external_desc external_sync);

/**
 * @brief: Returns the status of the synx object
 *
 * @param session_id : Client session id
 * @param h_synx     : Synx object handle
 *
 * @return Status of the synx object
 */
int synx_get_status(struct synx_session session_id, s32 h_synx);

/**
 * @brief: Adds to the reference count of a synx object
 *
 * When a synx object is created, the refcount will be 1. Every
 * additional refcount requires separate release of object handle.
 *
 * @param session_id : Client session id
 * @param h_synx     : Synx object handle
 * @param count      : Count to add to the refcount
 *
 * @return 0 upon success, -EINVAL if synx object is in bad state
 */
int synx_addrefcount(struct synx_session session_id,
	s32 h_synx, s32 count);

/**
 * @brief: Imports (looks up) synx object from given handle and key
 *
 * The given ID should have been exported by another client
 * and provided with key.
 *
 * @param session_id : Client session id
 * @param params     : Pointer to import params
 *
 * @return 0 upon success, -EINVAL if synx object is bad state
 */
int synx_import(struct synx_session session_id,
	struct synx_import_params *params);

/**
 * @brief: Exports a synx object and returns a key
 *
 * The synx object handle and given key may be passed to other
 * clients to be imported.
 *
 * @param session_id : Client session id
 * @param params     : Pointer to export params
 *
 * @return 0 upon success, -EINVAL if the ID is bad
 */
int synx_export(struct synx_session session_id,
	struct synx_export_params *params);

/**
 * @brief: Get the dma fence backing the synx object
 *
 * Function obtains an additional reference to the fence.
 * This reference needs to be released by the client
 * through dma_fence_put explicitly.
 *
 * @param session_id : Client session id
 * @param h_synx     : Synx object handle
 *
 * @return Dma fence pointer for a valid synx handle. NULL otherwise.
 */
struct dma_fence *synx_get_fence(struct synx_session session_id,
	s32 h_synx);

/**
 * @brief: Release the synx object

 * Decrements refcount of a synx object by 1, and destroys it
 * if becomes 0
 *
 * @param session_id : Client session id
 * @param h_synx     : Synx object handle to be destroyed
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_release(struct synx_session session_id, s32 h_synx);

#endif /* __SYNX_API_H__ */
