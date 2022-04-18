/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __SYNX_PRIVATE_H__
#define __SYNX_PRIVATE_H__

#include <linux/bitmap.h>
#include <linux/cdev.h>
#include <linux/dma-fence.h>
#include <linux/dma-fence-array.h>
#include <linux/hashtable.h>
#include <linux/workqueue.h>

#define SYNX_NAME                   "synx"
#define SYNX_WORKQUEUE_NAME         "hiprio_synx_work_queue"
#define SYNX_WORKQUEUE_THREADS      4
#define SYNX_MAX_NUM_BINDINGS       8
#define SYNX_DEVICE_NAME            "synx_device"

#define SYNX_CLIENT_HANDLE_SHIFT    8
#define SYNX_OBJ_HANDLE_SHIFT       8
#define SYNX_CLIENT_ENCODE_SHIFT    16
#define SYNX_OBJ_ENCODE_SHIFT       16

#define MAX_TIMESTAMP_SIZE          32
#define SYNX_OBJ_NAME_LEN           64
#define SYNX_MAX_CLIENTS            (1UL<<SYNX_CLIENT_HANDLE_SHIFT)
#define SYNX_MAX_OBJS               (1UL<<SYNX_OBJ_HANDLE_SHIFT)
#define SYNX_MAX_REF_COUNTS         2048
#define SYNX_PAYLOAD_WORDS          4

#define SYNX_CLIENT_HANDLE_MASK     (SYNX_MAX_CLIENTS-1)
#define SYNX_OBJ_HANDLE_MASK        (SYNX_MAX_OBJS-1)
#define SYNX_CLIENT_ENCODE_MASK     ((1UL<<SYNX_CLIENT_ENCODE_SHIFT)-1)
#define SYNX_CLIENT_IDX_OBJ_MASK    ((1UL<<(SYNX_CLIENT_HANDLE_SHIFT+SYNX_OBJ_ENCODE_SHIFT))-1)

/* external sync table to be same enum as type */
#define SYNX_CAMERA_ID_TBL          SYNX_TYPE_CSL
#define SYNX_GLOBAL_KEY_TBL         SYNX_MAX_BIND_TYPES

/**
 * struct synx_external_data - data passed over to external sync objects
 * to pass on callback
 *
 * @session_id : Synx client id
 * @h_synx     : Synx object handle
 */
struct synx_external_data {
	struct synx_session session_id;
	s32 h_synx;
};

/**
 * struct synx_bind_desc - bind payload descriptor
 *
 * @external_desc : External bind information
 * @external_data : Pointer to data passed over
 */
struct synx_bind_desc {
	struct synx_external_desc external_desc;
	struct synx_external_data *external_data;
};

/**
 * struct error_node - Single error node related to a table_row
 *
 * @timestamp  : Time that the error occurred
 * @client_id  : Synx client id
 * @h_synx     : Synx object handle
 * @error_code : Code related to the error
 * @node       : List member used to append this node to error list
 */
struct error_node {
	char timestamp[MAX_TIMESTAMP_SIZE];
	u32 client_id;
	s32 h_synx;
	s32 error_code;
	struct list_head node;
};

/**
 * struct hash_key_data - Single node of entry in hash table
 *
 * @key  : Unique key used to hash to table
 * @data : Data to be saved
 * @refcount : Refcount for node entry
 * @node : Hash list member used to append this node to table
 */
struct hash_key_data {
	u32 key;
	void *data;
	struct kref refcount;
	struct hlist_node node;
};

/**
 * struct synx_kernel_payload - Single node of information about a kernel
 * callback registered on a synx object
 *
 * @h_synx         : Synx object handle
 * @status         : Synx obj status or callback failure
 * @data           : Callback data, passed by client driver
 * @cb_func        : Callback function, registered by client driver
 * @cancel_cb_func : Cancellation callback function
 */
struct synx_kernel_payload {
	s32 h_synx;
	u32 status;
	void *data;
	synx_callback cb_func;
	synx_callback cancel_cb_func;
};

/**
 * struct synx_cb_data - Single node of information about callback
 * registered by client for the synx object
 *
 * @session_id  : Synx client id
 * @idx         : Client payload table index
 * @status      : Synx obj status or callback failure
 * @cb_dispatch : Work representing the callback dispatch
 * @node        : List member used to append this node to reg cb list
 */
struct synx_cb_data {
	struct synx_session session_id;
	u32 idx;
	u32 status;
	struct work_struct cb_dispatch;
	struct list_head node;
};

/**
 * struct synx_client_cb - Single node of information about cb payload
 * registered by client
 *
 * @is_valid     : True if this is a valid entry
 * @idx          : Index of the client callback table
 * @client       : Client session
 * @kernel_cb    : Kernel callback payload
 * @node         : List member used to append this node to event list
 */
struct synx_client_cb {
	bool is_valid;
	u32 idx;
	struct synx_client *client;
	struct synx_kernel_payload kernel_cb;
	struct list_head node;
};

/**
 * struct synx_registered_ops - External sync clients registered for bind
 * operations with synx driver
 *
 * @name  : Name of the external sync client
 * @ops   : Bind operations struct for the client
 * @type  : External client type
 * @valid : Validity of the client registered bind ops
 */
struct synx_registered_ops {
	char name[SYNX_OBJ_NAME_LEN];
	struct bind_operations ops;
	u32 type;
	bool valid;
};

/**
 * struct synx_cleanup_cb - Data for worker to cleanup client session
 *
 * @data        : Client session data
 * @cb_dispatch : Work representing the call dispatch
 */
struct synx_cleanup_cb {
	void *data;
	struct work_struct cb_dispatch;
};

/**
 * struct synx_ipc_cb - Data exchanged between signaling cores through
 * IPC Lite framework
 *
 * @global_key : Unique key representing the synx object
 * @status     : Signaling status
 */
struct synx_ipc_msg {
	u32 global_key;
	u32 status;
};

/**
 * struct synx_ipc_cb - Data for worker to handle IPC send/callback
 *
 * @msg         : IPC Lite message
 * @cb_dispatch : Work representing the call dispatch
 */
struct synx_ipc_cb {
	struct synx_ipc_msg msg;
	struct work_struct cb_dispatch;
};

/**
 * struct synx_coredata - Synx object, used for internal book keeping
 * of all metadata associated with each individual synx object
 *
 * @name              : Optional string representation of the synx object
 * @fence             : Pointer to dma fence backing synx object
 * @fence_cb          : Callback struct registered with external fence
 * @obj_lock          : Mutex lock for coredata access
 * @refcount          : References by the various client
 * @type              : Synx object type
 * @num_bound_synxs   : Number of external bound synx objects
 * @bound_synxs       : Array of bound external sync objects
 * @reg_cbs_list      : List of all registered callbacks
 * @global_key        : Global key for synchronization
 */
struct synx_coredata {
	char name[SYNX_OBJ_NAME_LEN];
	struct dma_fence *fence;
	struct dma_fence_cb fence_cb;
	struct mutex obj_lock;
	struct kref refcount;
	u32 type;
	u32 num_bound_synxs;
	struct synx_bind_desc bound_synxs[SYNX_MAX_NUM_BINDINGS];
	struct list_head reg_cbs_list;
	u32 global_key;
};

struct synx_client;
struct synx_device;

/**
 * struct synx_handle_coredata - Internal struct to manage synx handle to
 * synx object mapping along with tracking synx object usage by the client
 * through reference counting
 *
 * @client            : Pointer to client owning the synx object
 * @synx_obj          : Pointer to synx object
 * @internal_refcount : References by the client
 * @import_refcount   : References for external clients for import
 * @id                : Synx object handle for the client
 * @key               : Key for import authentication
 * @rel_count         : No of allowed release counts
 */
struct synx_handle_coredata {
	struct synx_client *client;
	struct synx_coredata *synx_obj;
	struct kref internal_refcount;
	struct kref import_refcount;
	u32 handle;
	u16 key;
	u32 rel_count;
};

/**
 * struct synx_client - Internal struct to book keep each client
 * details
 *
 * @device          : Pointer to synx device structure
 * @name            : Optional string representation of the client
 * @id              : Unique client session id
 * @event_q_lock    : Mutex for the event queue
 * @event_q         : All the user callback payloads
 * @event_wq        : Wait queue for the polling process
 * @cb_bitmap       : Bitmap representation of all cb table entries
 * @cb_table        : Table of all registered callbacks by client
 * @bitmap          : Bitmap representation of all synx object handles
 * @synx_table      : Table of all synx objects
 * @synx_table_lock : Mutex array, one for each row in the synx table
 */
struct synx_client {
	struct synx_device *device;
	char name[SYNX_OBJ_NAME_LEN];
	u32 id;
	struct mutex event_q_lock;
	struct list_head event_q;
	wait_queue_head_t event_wq;
	DECLARE_BITMAP(cb_bitmap, SYNX_MAX_OBJS);
	struct synx_client_cb cb_table[SYNX_MAX_OBJS];
	DECLARE_BITMAP(bitmap, SYNX_MAX_OBJS);
	struct synx_handle_coredata synx_table[SYNX_MAX_OBJS];
	struct mutex synx_table_lock[SYNX_MAX_OBJS];
};

/**
 * struct synx_client_metadata - Internal struct to map client id with
 * client data along with tracking usage through reference counting
 *
 * @client   : Pointer to client data
 * @refcount : Outstanding references to client data
 */
struct synx_client_metadata {
	struct synx_client *client;
	struct kref refcount;
};

/**
 * struct synx_device - Internal struct to book keep synx driver details
 *
 * @cdev          : Character device
 * @dev           : Device type
 * @class         : Device class
 * @table_lock    : Mutex used to lock the table
 * @bitmap        : Bitmap representation of all synx clients
 * @client_table  : Table of all synx clients
 * @work_queue    : Work queue used for dispatching callbacks
 * @dma_context   : dma context id
 * @vtbl_lock     : Mutex used to lock the bind table
 * @bind_vtbl     : Table with registered bind ops for external sync
 * @debugfs_root  : Root directory for debugfs
 * @error_list    : List of all errors occurred
 * @error_lock    : Mutex used to modify the error list
 */
struct synx_device {
	struct cdev cdev;
	dev_t dev;
	struct class *class;
	struct mutex dev_table_lock;
	DECLARE_BITMAP(bitmap, SYNX_MAX_CLIENTS);
	struct synx_client_metadata client_table[SYNX_MAX_CLIENTS];
	struct workqueue_struct *work_queue;
	u64 dma_context;
	struct mutex vtbl_lock;
	struct synx_registered_ops bind_vtbl[SYNX_MAX_BIND_TYPES];
	struct dentry *debugfs_root;
	struct list_head error_list;
	struct mutex error_lock;
	struct workqueue_struct *ipc_work_queue;
};

/**
 * @brief: Internal function to signal the synx object
 *
 * @param synx_obj    : Pointer to the synx object to signal
 * @param status      : Signaling status
 * @param cb_signal   : If signaling invoked from external cb
 * @param ext_sync_id : External sync id
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_signal_core(struct synx_coredata *synx_obj,
	u32 status,
	bool cb_signal,
	s32 ext_sync_id);


/**
 * @brief: Callback registered with ipc framework
 *
 * @param client_id   : Client core id
 * @param data        : Message received
 * @param priv        : Private data passed back
 *                      (Provided during callback registration)
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_ipc_callback(uint32_t client_id,
	uint64_t data, void *priv);

/**
 * @brief: Internal function to signal the synx fence
 *
 * @param synx_obj : Pointer to the synx object to signal
 * @param status   : Signaling status
 * @param internal : Flag to separate signal originating
 *                   within the core (TRUE) and external to core (FALSE)
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_signal_fence(struct synx_coredata *synx_obj,
	u32 status, bool internal);

#endif /* __SYNX_PRIVATE_H__ */
