/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __SYNX_API_H__
#define __SYNX_API_H__

#if IS_REACHABLE(CONFIG_MSM_GLOBAL_SYNX)
#include <linux/list.h>
#include <uapi/media/synx.h>

#define SYNX_STATE_INVALID                   0
#define SYNX_STATE_ACTIVE                    1
#define SYNX_STATE_SIGNALED_ERROR            3
#define SYNX_STATE_SIGNALED_EXTERNAL         5

/* signal states */
#define SYNX_STATE_SIGNALED_SUCCESS          2
#define SYNX_STATE_SIGNALED_CANCEL           4

/**
 * type of external sync object
 *
 * SYNX_TYPE_CSL  : Object is a CSL sync object
 */
#define SYNX_TYPE_CSL                        0
#define SYNX_MAX_BIND_TYPES                  1

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
 * @type       : Global fence type
 */
struct synx_export_params {
	s32 h_synx;
	u32 *secure_key;
	struct dma_fence *fence;
	u32 type;
	u32 reserved;
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

#elif IS_REACHABLE(CONFIG_MSM_GLOBAL_SYNX_V2)
#include <linux/list.h>
#include <uapi/media/synx.h>

#include "synx_err.h"

#define SYNX_NO_TIMEOUT        ((u64)-1)

/**
 * enum synx_create_flags - Flags passed during synx_create call
 *
 * SYNX_CREATE_LOCAL_FENCE  : Instructs the framework to create local synx object
 * SYNX_CREATE_GLOBAL_FENCE : Instructs the framework to create global synx object
 * SYNX_CREATE_DMA_FENCE    : Create a synx object by wrapping the provided dma fence.
 *                            Need to pass the dma_fence ptr through fence variable
 *                            if this flag is set.
 * SYNX_CREATE_CSL_FENCE    : Create a synx object with provided csl fence.
 *                            Establishes interop with the csl fence through
 *                            bind operations.
 */
enum synx_create_flags {
	SYNX_CREATE_LOCAL_FENCE  = 0x01,
	SYNX_CREATE_GLOBAL_FENCE = 0x02,
	SYNX_CREATE_DMA_FENCE    = 0x04,
	SYNX_CREATE_CSL_FENCE    = 0x08,
	SYNX_CREATE_MAX_FLAGS    = 0x10,
};

/**
 * enum synx_init_flags - Session initialization flag
 */
enum synx_init_flags {
	SYNX_INIT_MAX = 0x01,
};

/**
 * enum synx_import_flags - Import flags
 *
 * SYNX_IMPORT_LOCAL_FENCE  : Instructs the framework to create local synx object
 * SYNX_IMPORT_GLOBAL_FENCE : Instructs the framework to create global synx object
 * SYNX_IMPORT_SYNX_FENCE   : Import native Synx handle for synchronization
 *                            Need to pass the Synx handle ptr through fence variable
 *                            if this flag is set.
 * SYNX_IMPORT_DMA_FENCE    : Import dma fence.and crate Synx handle for interop
 *                            Need to pass the dma_fence ptr through fence variable
 *                            if this flag is set.
 * SYNX_IMPORT_EX_RELEASE   : Flag to inform relaxed invocation where release call
 *                            need not be called by client on this handle after import.
 */
enum synx_import_flags {
	SYNX_IMPORT_LOCAL_FENCE  = 0x01,
	SYNX_IMPORT_GLOBAL_FENCE = 0x02,
	SYNX_IMPORT_SYNX_FENCE   = 0x04,
	SYNX_IMPORT_DMA_FENCE    = 0x08,
	SYNX_IMPORT_EX_RELEASE   = 0x10,
};

/**
 * enum synx_signal_status - Signal status
 *
 * SYNX_STATE_SIGNALED_SUCCESS : Signal success
 * SYNX_STATE_SIGNALED_CANCEL  : Signal cancellation
 * SYNX_STATE_SIGNALED_MAX     : Clients can send custom notification
 *                               beyond the max value (only positive)
 */
enum synx_signal_status {
	SYNX_STATE_SIGNALED_SUCCESS = 2,
	SYNX_STATE_SIGNALED_CANCEL  = 4,
	SYNX_STATE_SIGNALED_MAX     = 64,
};

/**
 * synx_callback - Callback invoked by external fence
 *
 * External fence dispatch the registered callback to notify
 * signal to synx framework.
 */
typedef void (*synx_callback)(s32 sync_obj, int status, void *data);

/**
 * synx_user_callback - Callback function registered by clients
 *
 * User callback registered for non-blocking wait. Dispatched when
 * synx object is signaled or timeout has expired.
 */
typedef void (*synx_user_callback_t)(u32 h_synx, int status, void *data);

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
 * synx_bind_client_type : External fence supported for bind
 *
 * SYNX_TYPE_CSL : Camera CSL fence
 */
enum synx_bind_client_type {
	SYNX_TYPE_CSL = 0,
	SYNX_MAX_BIND_TYPES,
};

/**
 * struct synx_register_params - External registration parameters
 *
 * @ops  : Bind operations struct
 * @name : External client name
 *         Only first 64 bytes are accepted, rest will be ignored
 * @type : Synx bind client type
 */
struct synx_register_params {
	struct bind_operations ops;
	char *name;
	enum synx_bind_client_type type;
};

/**
 * struct synx_queue_desc - Memory descriptor of the queue allocated by
 *                           the fence driver for each client during
 *                           register.
 *
 * @vaddr    : CPU virtual address of the queue.
 * @dev_addr : Physical address of the memory object.
 * @size     : Size of the memory.
 * @mem_data : Internal pointer with the attributes of the allocation.
 */
struct synx_queue_desc {
	void *vaddr;
	u64 dev_addr;
	u64 size;
	void *mem_data;
};

/**
 * enum synx_client_id : Unique identifier of the supported clients
 *
 * @SYNX_CLIENT_NATIVE   : Native Client
 * @SYNX_CLIENT_GFX_CTX0 : GFX Client 0
 * @SYNX_CLIENT_DPU_CTL0 : DPU Client 0
 * @SYNX_CLIENT_DPU_CTL1 : DPU Client 1
 * @SYNX_CLIENT_DPU_CTL2 : DPU Client 2
 * @SYNX_CLIENT_DPU_CTL3 : DPU Client 3
 * @SYNX_CLIENT_DPU_CTL4 : DPU Client 4
 * @SYNX_CLIENT_DPU_CTL5 : DPU Client 5
 * @SYNX_CLIENT_EVA_CTX0 : EVA Client 0
 * @SYNX_CLIENT_VID_CTX0 : Video Client 0
 * @SYNX_CLIENT_NSP_CTX0 : NSP Client 0
 * @SYNX_CLIENT_IFE_CTX0 : IFE Client 0
 */
enum synx_client_id {
	SYNX_CLIENT_NATIVE = 0,
	SYNX_CLIENT_GFX_CTX0,
	SYNX_CLIENT_DPU_CTL0,
	SYNX_CLIENT_DPU_CTL1,
	SYNX_CLIENT_DPU_CTL2,
	SYNX_CLIENT_DPU_CTL3,
	SYNX_CLIENT_DPU_CTL4,
	SYNX_CLIENT_DPU_CTL5,
	SYNX_CLIENT_EVA_CTX0,
	SYNX_CLIENT_VID_CTX0,
	SYNX_CLIENT_NSP_CTX0,
	SYNX_CLIENT_IFE_CTX0,
	SYNX_CLIENT_MAX,
};

/**
 * struct synx_session - Client session identifier
 *
 * @type   : Session type
 * @client : Pointer to client session
 */
struct synx_session {
	u32 type;
	void *client;
};

/**
 * struct synx_initialization_params - Session params
 *
 * @name  : Client session name
 *          Only first 64 bytes are accepted, rest will be ignored
 * @ptr   : Pointer to queue descriptor (filled by function)
 * @id    : Client identifier
 * @flags : Synx initialization flags
 */
struct synx_initialization_params {
	const char *name;
	struct synx_queue_desc *ptr;
	enum synx_client_id id;
	enum synx_init_flags flags;
};

/**
 * struct synx_create_params - Synx creation parameters
 *
 * @name     : Optional parameter associating a name with the synx
 *             object for debug purposes
 *             Only first 64 bytes are accepted,
 *             rest will be ignored
 * @h_synx   : Pointer to synx object handle (filled by function)
 * @fence    : Pointer to external fence
 * @flags    : Synx flags for customization (mentioned below)
 *
 * SYNX_CREATE_GLOBAL_FENCE - Hints the framework to create global synx object
 *     If flag not set, hints framework to create a local synx object.
 * SYNX_CREATE_DMA_FENCE - Wrap synx object with dma fence.
 *     Need to pass the dma_fence ptr through 'fence' variable if this flag is set.
 * SYNX_CREATE_BIND_FENCE - Create a synx object with provided external fence.
 *     Establishes interop with supported external fence through bind operations.
 *     Need to fill synx_external_desc structure if this flag is set.
 */

struct synx_create_params {
	const char *name;
	u32 *h_synx;
	void *fence;
	enum synx_create_flags flags;
};

/**
 * enum synx_merge_flags - Handle merge flags
 *
 * SYNX_MERGE_LOCAL_FENCE   : Create local composite object.
 * SYNX_MERGE_GLOBAL_FENCE  : Create global composite object.
 * SYNX_MERGE_NOTIFY_ON_ALL : Notify on signaling of ALL objects
 * SYNX_MERGE_NOTIFY_ON_ANY : Notify on signaling of ANY object
 */
enum synx_merge_flags {
	SYNX_MERGE_LOCAL_FENCE   = 0x01,
	SYNX_MERGE_GLOBAL_FENCE  = 0x02,
	SYNX_MERGE_NOTIFY_ON_ALL = 0x04,
	SYNX_MERGE_NOTIFY_ON_ANY = 0x08,
};

/*
 * struct synx_merge_params - Synx merge parameters
 *
 * @h_synxs      : Pointer to a array of synx handles to be merged
 * @flags        : Merge flags
 * @num_objs     : Number of synx objs in the block
 * @h_merged_obj : Merged synx object handle (filled by function)
 */
struct synx_merge_params {
	u32 *h_synxs;
	enum synx_merge_flags flags;
	u32 num_objs;
	u32 *h_merged_obj;
};

/**
 * enum synx_import_type - Import type
 *
 * SYNX_IMPORT_INDV_PARAMS : Import filled with synx_import_indv_params struct
 * SYNX_IMPORT_ARR_PARAMS  : Import filled with synx_import_arr_params struct
 */
enum synx_import_type {
	SYNX_IMPORT_INDV_PARAMS = 0x01,
	SYNX_IMPORT_ARR_PARAMS  = 0x02,
};

/**
 * struct synx_import_indv_params - Synx import indv parameters
 *
 * @new_h_synxs : Pointer to new synx object
 *                (filled by the function)
 *                The new handle/s should be used by importing
 *                process for all synx api operations and
 *                for sharing with FW cores.
 * @flags       : Synx flags
 * @fence       : Pointer to external fence
 */
struct synx_import_indv_params {
	u32 *new_h_synx;
	enum synx_import_flags flags;
	void *fence;
};

/**
 * struct synx_import_arr_params - Synx import arr parameters
 *
 * @list        : Array of synx_import_indv_params pointers
 * @num_fences  : No of fences passed to framework
 */
struct synx_import_arr_params {
	struct synx_import_indv_params *list;
	u32 num_fences;
};

/**
 * struct synx_import_params - Synx import parameters
 *
 * @type : Import params type filled by client
 * @indv : Params to import an individual handle/fence
 * @arr  : Params to import an array of handles/fences
 */
struct synx_import_params {
	enum synx_import_type type;
	union {
		struct synx_import_indv_params indv;
		struct synx_import_arr_params  arr;
	};
};

/**
 * struct synx_callback_params - Synx callback parameters
 *
 * @h_synx         : Synx object handle
 * @cb_func        : Pointer to callback func to be invoked
 * @userdata       : Opaque pointer passed back with callback
 * @cancel_cb_func : Pointer to callback to ack cancellation (optional)
 * @timeout_ms     : Timeout in ms. SYNX_NO_TIMEOUT if no timeout.
 */
struct synx_callback_params {
	u32 h_synx;
	synx_user_callback_t cb_func;
	void *userdata;
	synx_user_callback_t cancel_cb_func;
	u64 timeout_ms;
};

/* Kernel APIs */

/* synx_register_ops - Register operations for external synchronization
 *
 * Register with synx for enabling external synchronization through bind
 *
 * @param params : Pointer to register params
 *
 * @return Status of operation. SYNX_SUCCESS in case of success.
 * -SYNX_INVALID will be returned if params are invalid.
 * -SYNX_NOMEM will be returned if bind ops cannot be registered due to
 * insufficient memory.
 * -SYNX_ALREADY will be returned if type already in use.
 */
int synx_register_ops(const struct synx_register_params *params);

/**
 * synx_deregister_ops - De-register external synchronization operations
 *
 * @param params : Pointer to register params
 *
 * @return Status of operation. SYNX_SUCCESS in case of success.
 * -SYNX_INVALID will be returned if record not found.
 */
int synx_deregister_ops(const struct synx_register_params *params);

/**
 * synx_initialize - Initializes a new client session
 *
 * @param params : Pointer to session init params
 *
 * @return Client session pointer on success. NULL or error in case of failure.
 */
struct synx_session *synx_initialize(struct synx_initialization_params *params);

/**
 * synx_uninitialize - Destroys the client session
 *
 * @param session : Session ptr (returned from synx_initialize)
 *
 * @return Status of operation. SYNX_SUCCESS in case of success.
 */
int synx_uninitialize(struct synx_session *session);

/**
 * synx_create - Creates a synx object
 *
 *  Creates a new synx obj and returns the handle to client.
 *
 * @param session : Session ptr (returned from synx_initialize)
 * @param params  : Pointer to create params
 *
 * @return Status of operation. SYNX_SUCCESS in case of success.
 * -SYNX_INVALID will be returned if params were invalid.
 * -SYNX_NOMEM will be returned if the kernel can't allocate space for
 * synx object.
 */
int synx_create(struct synx_session *session, struct synx_create_params *params);

/**
 * synx_async_wait - Registers a callback with a synx object
 *
 * @param session : Session ptr (returned from synx_initialize)
 * @param params  : Callback params
 *
 * @return Status of operation. SYNX_SUCCESS in case of success.
 * -SYNX_INVALID will be returned if userdata is invalid.
 * -SYNX_NOMEM will be returned if cb_func is invalid.
 */
int synx_async_wait(struct synx_session *session, struct synx_callback_params *params);

/**
 * synx_cancel_async_wait - De-registers a callback with a synx object
 *
 * @param session : Session ptr (returned from synx_initialize)
 * @param params  : Callback params
 *
 * @return Status of operation. SYNX_SUCCESS in case of success.
 * -SYNX_ALREADY if object has already been signaled, and cannot be cancelled.
 * -SYNX_INVALID will be returned if userdata is invalid.
 * -SYNX_NOMEM will be returned if cb_func is invalid.
 */
int synx_cancel_async_wait(struct synx_session *session,
	struct synx_callback_params *params);

/**
 * synx_signal - Signals a synx object with the status argument.
 *
 * This function will signal the synx object referenced by h_synx
 * and invoke any external binding synx objs.
 * The status parameter will indicate whether the entity
 * performing the signaling wants to convey an error case or a success case.
 *
 * @param session : Session ptr (returned from synx_initialize)
 * @param h_synx  : Synx object handle
 * @param status  : Status of signaling.
 *                  Clients can send custom signaling status
 *                  beyond SYNX_STATE_SIGNALED_MAX.
 *
 * @return Status of operation. Negative in case of error. SYNX_SUCCESS otherwise.
 */
int synx_signal(struct synx_session *session, u32 h_synx,
	enum synx_signal_status status);

/**
 * synx_merge - Merges multiple synx objects
 *
 * This function will merge multiple synx objects into a synx group.
 *
 * @param session : Session ptr (returned from synx_initialize)
 * @param params  : Merge params
 *
 * @return Status of operation. Negative in case of error. SYNX_SUCCESS otherwise.
 */
int synx_merge(struct synx_session *session, struct synx_merge_params *params);

/**
 * synx_wait - Waits for a synx object synchronously
 *
 * Does a wait on the synx object identified by h_synx for a maximum
 * of timeout_ms milliseconds. Must not be called from interrupt context as
 * this API can sleep.
 * Will return status if handle was signaled. Status can be from pre-defined
 * states (enum synx_signal_status) or custom status sent by producer.
 *
 * @param session    : Session ptr (returned from synx_initialize)
 * @param h_synx     : Synx object handle to be waited upon
 * @param timeout_ms : Timeout in ms
 *
 * @return Signal status. -SYNX_INVAL if synx object is in bad state or arguments
 * are invalid, -SYNX_TIMEOUT if wait times out.
 */
int synx_wait(struct synx_session *session, u32 h_synx, u64 timeout_ms);

/**
 * synx_get_status - Returns the status of the synx object
 *
 * @param session : Session ptr (returned from synx_initialize)
 * @param h_synx  : Synx object handle
 *
 * @return Status of the synx object.
 */
int synx_get_status(struct synx_session *session, u32 h_synx);

/**
 * synx_import - Imports (looks up) synx object from given handle/fence
 *
 * Import subscribes the client session for notification on signal
 * of handles/fences.
 *
 * @param session : Session ptr (returned from synx_initialize)
 * @param params  : Pointer to import params
 *
 * @return SYNX_SUCCESS upon success, -SYNX_INVAL if synx object is bad state
 */
int synx_import(struct synx_session *session, struct synx_import_params *params);

/**
 * synx_get_fence - Get the native fence backing the synx object
 *
 * Function returns the native fence. Clients need to
 * acquire & release additional reference explicitly.
 *
 * @param session : Session ptr (returned from synx_initialize)
 * @param h_synx  : Synx object handle
 *
 * @return Fence pointer upon success, NULL or error in case of failure.
 */
void *synx_get_fence(struct synx_session *session, u32 h_synx);

/**
 * synx_release - Release the synx object
 *
 * Decrements refcount of a synx object by 1, and destroys it
 * if becomes 0.
 *
 * @param session : Session ptr (returned from synx_initialize)
 * @param h_synx  : Synx object handle to be destroyed
 *
 * @return Status of operation. Negative in case of error. SYNX_SUCCESS otherwise.
 */
int synx_release(struct synx_session *session, u32 h_synx);

/**
 * synx_recover - Recover any possible handle leaks
 *
 * Function should be called on HW hang/reset to
 * recover the Synx handles shared. This cleans up
 * Synx handles held by the rest HW, and avoids
 * potential resource leaks.
 *
 * Function does not destroy the session, but only
 * recover synx handles belonging to the session.
 * Synx session would still be active and clients
 * need to destroy the session explicitly through
 * synx_uninitialize API.
 *
 * @param id : Client ID of core to recover
 *
 * @return Status of operation. Negative in case of error. SYNX_SUCCESS otherwise.
 */
int synx_recover(enum synx_client_id id);
#endif /* CONFIG_MSM_GLOBAL_SYNX_V2 */

#endif /* __SYNX_API_H__ */
