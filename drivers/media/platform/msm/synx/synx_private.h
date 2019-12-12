/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef __SYNX_PRIVATE_H__
#define __SYNX_PRIVATE_H__

#include <linux/bitmap.h>
#include <linux/cdev.h>
#include <linux/dma-fence.h>
#include <linux/dma-fence-array.h>
#include <linux/idr.h>
#include <linux/workqueue.h>

#define MAX_TIMESTAMP_SIZE          32
#define SYNX_OBJ_NAME_LEN           64
#define SYNX_MAX_OBJS               1024
#define SYNX_MAX_REF_COUNTS         2048
#define SYNX_PAYLOAD_WORDS          4
#define SYNX_NAME                   "synx"
#define SYNX_WORKQUEUE_NAME         "hiprio_synx_work_queue"
#define SYNX_MAX_NUM_BINDINGS       8
#define SYNX_DEVICE_NAME            "synx_device"

/**
 * struct synx_external_data - data passed over to external sync objects
 * to pass on callback
 *
 * @synx_obj    : synx obj id
 * @secure_key  : secure key for authentication
 */
struct synx_external_data {
	s32 synx_obj;
	u32 secure_key;
};

/**
 * struct synx_bind_desc - bind payload descriptor
 *
 * @external_desc : external bind information
 * @bind_data     : pointer to data passed over
 */
struct synx_bind_desc {
	struct synx_external_desc external_desc;
	struct synx_external_data *external_data;
};

/**
 * struct error_node - Single error node related to a table_row
 *
 * @timestamp       : Time that the error occurred
 * @error_code      : Code related to the error
 * @node            : List member used to append
 *                    this node to a linked list
 */
struct error_node {
	char timestamp[MAX_TIMESTAMP_SIZE];
	s32 error_code;
	s32 synx_obj;
	struct list_head node;
};

/**
 * struct synx_callback_info - Single node of information about a kernel
 * callback registered on a sync object
 *
 * @callback_func    : Callback function, registered by client driver
 * @cb_data          : Callback data, registered by client driver
 * @status           : Status with which callback will be invoked in client
 * @synx_obj         : Sync id of the object for which callback is registered
 * @cb_dispatch_work : Work representing the call dispatch
 * @list             : List member used to append this node to a linked list
 */
struct synx_callback_info {
	synx_callback callback_func;
	void *cb_data;
	int status;
	s32 synx_obj;
	struct work_struct cb_dispatch_work;
	struct list_head list;
};

struct synx_client;

/**
 * struct synx_user_payload - Single node of information about a callback
 * registered from user space
 *
 * @synx_obj     : Global id
 * @status       : synx obj status or callback failure
 * @payload_data : Payload data, opaque to kernel
 */
struct synx_user_payload {
	s32 synx_obj;
	int status;
	u64 payload_data[SYNX_PAYLOAD_WORDS];
};

/**
 * struct synx_cb_data - Single node of information about a user space
 * payload registered from user space
 *
 * @client : Synx client
 * @data   : Payload data, opaque to kernel
 * @list   : List member used to append this node to user cb list
 */
struct synx_cb_data {
	struct synx_client *client;
	struct synx_user_payload data;
	struct list_head list;
};

/**
 * struct synx_obj_node - Single node of info for the synx handle
 * mapped to synx object metadata
 *
 * @synx_obj : Synx integer handle
 * @list     : List member used to append to synx handle list
 */
struct synx_obj_node {
	s32 synx_obj;
	struct list_head list;
};

/**
 * struct synx_table_row - Single row of information about a synx object, used
 * for internal book keeping in the synx driver
 *
 * @name              : Optional string representation of the synx object
 * @fence             : dma fence backing the synx object
 * @spinlock          : Spinlock for the dma fence
 * @synx_obj_list     : List of synx integer handles mapped
 * @index             : Index of the spin lock table associated with synx obj
 * @num_bound_synxs   : Number of external bound synx objects
 * @signaling_id      : ID of the external sync object invoking the callback
 * @secure_key        : Secure key generated for authentication
 * @bound_synxs       : Array of bound synx objects
 * @callback_list     : Linked list of kernel callbacks registered
 * @user_payload_list : Linked list of user space payloads registered
 */
struct synx_table_row {
	char name[SYNX_OBJ_NAME_LEN];
	struct dma_fence *fence;
	spinlock_t *spinlock;
	struct list_head synx_obj_list;
	s32 index;
	u32 num_bound_synxs;
	s32 signaling_id;
	u32 secure_key;
	struct synx_bind_desc bound_synxs[SYNX_MAX_NUM_BINDINGS];
	struct list_head callback_list;
	struct list_head user_payload_list;
};

/**
 * struct synx_registered_ops - External sync clients registered for bind
 * operations with synx driver
 *
 * @valid : Validity of the client registered bind ops
 * @name  : Name of the external sync client
 * @ops   : Bind operations struct for the client
 * @type  : External client type
 */
struct synx_registered_ops {
	bool valid;
	char name[32];
	struct bind_operations ops;
	u32 type;
};

/**
 * struct synx_import_data - Import metadata for sharing synx handles
 * with processes
 *
 * @key      : Import key for sharing synx handle
 * @synx_obj : Synx handle being exported
 * @row      : Pointer to synx object
 * @list     : List member used to append the node to import list
 */
struct synx_import_data {
	u32 key;
	s32 synx_obj;
	struct synx_table_row *row;
	struct list_head list;
};

/**
 * struct synx_device - Internal struct to book keep synx driver details
 *
 * @cdev          : Character device
 * @dev           : Device type
 * @class         : Device class
 * @synx_table    : Table of all synx objects
 * @row_locks     : Mutex lock array, one for each row in the table
 * @table_lock    : Mutex used to lock the table
 * @open_cnt      : Count of file open calls made on the synx driver
 * @work_queue    : Work queue used for dispatching kernel callbacks
 * @bitmap        : Bitmap representation of all synx objects
 * synx_ids       : Global unique ids
 * idr_lock       : Spin lock for id allocation
 * dma_context    : dma context id
 * vtbl_lock      : Mutex used to lock the bind table
 * bind_vtbl      : Table with registered bind ops for external sync (bind)
 * client_list    : All the synx clients
 * debugfs_root   : Root directory for debugfs
 * synx_node_head : list head for synx nodes
 * synx_node_list_lock : Spinlock for synx nodes
 * import_list    : List to validate synx import requests
 */
struct synx_device {
	struct cdev cdev;
	dev_t dev;
	struct class *class;
	struct synx_table_row synx_table[SYNX_MAX_OBJS];
	struct mutex row_locks[SYNX_MAX_OBJS];
	struct mutex table_lock;
	int open_cnt;
	struct workqueue_struct *work_queue;
	DECLARE_BITMAP(bitmap, SYNX_MAX_OBJS);
	struct idr synx_ids;
	spinlock_t idr_lock;
	u64 dma_context;
	struct mutex vtbl_lock;
	struct synx_registered_ops bind_vtbl[SYNX_MAX_BIND_TYPES];
	struct list_head client_list;
	struct dentry *debugfs_root;
	struct list_head synx_debug_head;
	spinlock_t synx_node_list_lock;
	struct list_head import_list;
};

/**
 * struct synx_client - Internal struct to book keep each client
 * specific details
 *
 * @device      : Pointer to synx device structure
 * @eventq_lock : Mutex for the event queue
 * @wq          : Queue for the polling process
 * @eventq      : All the user callback payloads
 * @list        : List member used to append this node to client_list
 */
struct synx_client {
	struct synx_device *device;
	struct mutex eventq_lock;
	wait_queue_head_t wq;
	struct list_head eventq;
	struct list_head list;
};

/**
 * @brief: Function to signal the synx object
 *
 * @param row    : Pointer to the synx object row
 * @param status : Signaling status
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int synx_signal_core(struct synx_table_row *row, u32 status);

#endif /* __SYNX_PRIVATE_H__ */
