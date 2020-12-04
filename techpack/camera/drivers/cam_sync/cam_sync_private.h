/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#ifndef __CAM_SYNC_PRIVATE_H__
#define __CAM_SYNC_PRIVATE_H__

#include <linux/bitmap.h>
#include <linux/videodev2.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>

#ifdef CONFIG_CAM_SYNC_DBG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#endif

#define CAM_SYNC_OBJ_NAME_LEN           64
//changed by xiaomi: CMI-6335(vedio/slow motion) needs upto 1024 fences
#define CAM_SYNC_MAX_OBJS               1792 // 1536=1024+768
#define CAM_SYNC_MAX_V4L2_EVENTS        200
#define CAM_SYNC_DEBUG_FILENAME         "cam_debug"
#define CAM_SYNC_DEBUG_BASEDIR          "cam"
#define CAM_SYNC_DEBUG_BUF_SIZE         32
#define CAM_SYNC_PAYLOAD_WORDS          2
#define CAM_SYNC_NAME                   "cam_sync"
#define CAM_SYNC_WORKQUEUE_NAME         "HIPRIO_SYNC_WORK_QUEUE"

#define CAM_SYNC_TYPE_INDV              0
#define CAM_SYNC_TYPE_GROUP             1

/**
 * enum sync_type - Enum to indicate the type of sync object,
 * i.e. individual or group.
 *
 * @SYNC_TYPE_INDV  : Object is an individual sync object
 * @SYNC_TYPE_GROUP : Object is a group sync object
 */
enum sync_type {
	SYNC_TYPE_INDV,
	SYNC_TYPE_GROUP
};

/**
 * enum sync_list_clean_type - Enum to indicate the type of list clean action
 * to be peformed, i.e. specific sync ID or all list sync ids.
 *
 * @SYNC_CLEAN_ONE : Specific object to be cleaned in the list
 * @SYNC_CLEAN_ALL : Clean all objects in the list
 */
enum sync_list_clean_type {
	SYNC_LIST_CLEAN_ONE,
	SYNC_LIST_CLEAN_ALL
};

/**
 * struct sync_parent_info - Single node of information about a parent
 * of a sync object, usually part of the parents linked list
 *
 * @sync_id  : Sync object id of parent
 * @list     : List member used to append this node to a linked list
 */
struct sync_parent_info {
	int32_t sync_id;
	struct list_head list;
};

/**
 * struct sync_parent_info - Single node of information about a child
 * of a sync object, usually part of the children linked list
 *
 * @sync_id  : Sync object id of child
 * @list     : List member used to append this node to a linked list
 */
struct sync_child_info {
	int32_t sync_id;
	struct list_head list;
};


/**
 * struct sync_callback_info - Single node of information about a kernel
 * callback registered on a sync object
 *
 * @callback_func    : Callback function, registered by client driver
 * @cb_data          : Callback data, registered by client driver
 * @status........   : Status with which callback will be invoked in client
 * @sync_obj         : Sync id of the object for which callback is registered
 * @cb_dispatch_work : Work representing the call dispatch
 * @list             : List member used to append this node to a linked list
 */
struct sync_callback_info {
	sync_callback callback_func;
	void *cb_data;
	int status;
	int32_t sync_obj;
	struct work_struct cb_dispatch_work;
	struct list_head list;
};

/**
 * struct sync_user_payload - Single node of information about a user space
 * payload registered from user space
 *
 * @payload_data    : Payload data, opaque to kernel
 * @list            : List member used to append this node to a linked list
 */
struct sync_user_payload {
	uint64_t payload_data[CAM_SYNC_PAYLOAD_WORDS];
	struct list_head list;
};

/**
 * struct sync_table_row - Single row of information about a sync object, used
 * for internal book keeping in the sync driver
 *
 * @name              : Optional string representation of the sync object
 * @type              : Type of the sync object (individual or group)
 * @sync_id           : Integer id representing this sync object
 * @parents_list      : Linked list of parents of this sync object
 * @children_list     : Linked list of children of this sync object
 * @state             : State (INVALID, ACTIVE, SIGNALED_SUCCESS or
 *                      SIGNALED_ERROR)
 * @remaining         : Count of remaining children that not been signaled
 * @signaled          : Completion variable on which block calls will wait
 * @callback_list     : Linked list of kernel callbacks registered
 * @user_payload_list : LInked list of user space payloads registered
 * @ref_cnt           : ref count of the number of usage of the fence.
 */
struct sync_table_row {
	char name[CAM_SYNC_OBJ_NAME_LEN];
	enum sync_type type;
	int32_t sync_id;
	/* List of parents, which are merged objects */
	struct list_head parents_list;
	/* List of children, which constitute the merged object */
	struct list_head children_list;
	uint32_t state;
	uint32_t remaining;
	struct completion signaled;
	struct list_head callback_list;
	struct list_head user_payload_list;
	atomic_t ref_cnt;
};

/**
 * struct cam_signalable_info - Information for a single sync object that is
 * ready to be signaled
 *
 * @sync_obj : Sync object id of signalable object
 * @status   : Status with which to signal
 * @list     : List member used to append this node to a linked list
 */
struct cam_signalable_info {
	int32_t sync_obj;
	uint32_t status;
	struct list_head list;
};

/**
 * struct sync_device - Internal struct to book keep sync driver details
 *
 * @vdev            : Video device
 * @v4l2_dev        : V4L2 device
 * @sync_table      : Table of all sync objects
 * @row_spinlocks   : Spinlock array, one for each row in the table
 * @table_lock      : Mutex used to lock the table
 * @open_cnt        : Count of file open calls made on the sync driver
 * @dentry          : Debugfs entry
 * @work_queue      : Work queue used for dispatching kernel callbacks
 * @cam_sync_eventq : Event queue used to dispatch user payloads to user space
 * @bitmap          : Bitmap representation of all sync objects
 */
struct sync_device {
	struct video_device *vdev;
	struct v4l2_device v4l2_dev;
	struct sync_table_row sync_table[CAM_SYNC_MAX_OBJS];
	spinlock_t row_spinlocks[CAM_SYNC_MAX_OBJS];
	struct mutex table_lock;
	int open_cnt;
	struct dentry *dentry;
	struct workqueue_struct *work_queue;
	struct v4l2_fh *cam_sync_eventq;
	spinlock_t cam_sync_eventq_lock;
	DECLARE_BITMAP(bitmap, CAM_SYNC_MAX_OBJS);
};


#endif /* __CAM_SYNC_PRIVATE_H__ */
