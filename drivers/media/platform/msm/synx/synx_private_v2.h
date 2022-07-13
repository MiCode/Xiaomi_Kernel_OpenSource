/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __SYNX_PRIVATE_V2_H__
#define __SYNX_PRIVATE_V2_H__

#include <linux/bitmap.h>
#include <linux/cdev.h>
#include <linux/dma-fence.h>
#include <linux/dma-fence-array.h>
#include <linux/hashtable.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>

#include "synx_api.h"
#include "synx_global_v2.h"

#define SYNX_MAX_OBJS               SYNX_GLOBAL_MAX_OBJS

#define SYNX_NAME                   "synx"
#define SYNX_DEVICE_NAME            "synx_device"
#define SYNX_WQ_CB_NAME             "hiprio_synx_cb_queue"
#define SYNX_WQ_CB_THREADS          4
#define SYNX_WQ_CLEANUP_NAME        "hiprio_synx_cleanup_queue"
#define SYNX_WQ_CLEANUP_THREADS     2
#define SYNX_MAX_NUM_BINDINGS       8

#define SYNX_OBJ_HANDLE_SHIFT       SYNX_HANDLE_INDEX_BITS
#define SYNX_OBJ_CORE_ID_SHIFT      (SYNX_OBJ_HANDLE_SHIFT+SYNX_HANDLE_CORE_BITS)
#define SYNX_OBJ_GLOBAL_FLAG_SHIFT  (SYNX_OBJ_CORE_ID_SHIFT+SYNX_HANDLE_GLOBAL_FLAG_BIT)

#define SYNX_OBJ_HANDLE_MASK        GENMASK_ULL(SYNX_OBJ_HANDLE_SHIFT-1, 0)
#define SYNX_OBJ_CORE_ID_MASK       GENMASK_ULL(SYNX_OBJ_CORE_ID_SHIFT-1, SYNX_OBJ_HANDLE_SHIFT)
#define SYNX_OBJ_GLOBAL_FLAG_MASK   \
	GENMASK_ULL(SYNX_OBJ_GLOBAL_FLAG_SHIFT-1, SYNX_OBJ_CORE_ID_SHIFT)

#define MAX_TIMESTAMP_SIZE          32
#define SYNX_OBJ_NAME_LEN           64

#define SYNX_PAYLOAD_WORDS          4

#define SYNX_CREATE_IM_EX_RELEASE   SYNX_CREATE_MAX_FLAGS
#define SYNX_CREATE_MERGED_FENCE    (SYNX_CREATE_MAX_FLAGS << 1)

#define SYNX_MAX_REF_COUNTS         100

struct synx_bind_desc {
	struct synx_external_desc_v2 external_desc;
	void *external_data;
};

struct error_node {
	char timestamp[32];
	u64 session;
	u32 client_id;
	u32 h_synx;
	s32 error_code;
	struct list_head node;
};

struct synx_entry_32 {
	u32 key;
	void *data;
	struct hlist_node node;
};

struct synx_entry_64 {
	u64 key;
	u32 data[2];
	struct kref refcount;
	struct hlist_node node;
};

struct synx_map_entry {
	struct synx_coredata *synx_obj;
	struct kref refcount;
	u32 flags;
	u32 key;
	struct work_struct dispatch;
	struct hlist_node node;
};

struct synx_fence_entry {
	u32 g_handle;
	u32 l_handle;
	u64 key;
	struct hlist_node node;
};

struct synx_kernel_payload {
	u32 h_synx;
	u32 status;
	void *data;
	synx_user_callback_t cb_func;
	synx_user_callback_t cancel_cb_func;
};

struct synx_cb_data {
	struct synx_session *session;
	u32 idx;
	u32 status;
	struct work_struct cb_dispatch;
	struct list_head node;
};

struct synx_client_cb {
	bool is_valid;
	u32 idx;
	struct synx_client *client;
	struct synx_kernel_payload kernel_cb;
	struct list_head node;
};

struct synx_registered_ops {
	char name[SYNX_OBJ_NAME_LEN];
	struct bind_operations ops;
	enum synx_bind_client_type type;
	bool valid;
};

struct synx_cleanup_cb {
	void *data;
	struct work_struct cb_dispatch;
};

enum synx_signal_handler {
	SYNX_SIGNAL_FROM_CLIENT   = 0x1,
	SYNX_SIGNAL_FROM_FENCE    = 0x2,
	SYNX_SIGNAL_FROM_IPC      = 0x4,
	SYNX_SIGNAL_FROM_CALLBACK = 0x8,
};

struct synx_signal_cb {
	u32 handle;
	u32 status;
	u64 ext_sync_id;
	struct synx_coredata *synx_obj;
	enum synx_signal_handler flag;
	struct dma_fence_cb fence_cb;
	struct work_struct cb_dispatch;
};

struct synx_coredata {
	char name[SYNX_OBJ_NAME_LEN];
	struct dma_fence *fence;
	struct mutex obj_lock;
	struct kref refcount;
	u32 type;
	u32 num_bound_synxs;
	struct synx_bind_desc bound_synxs[SYNX_MAX_NUM_BINDINGS];
	struct list_head reg_cbs_list;
	u32 global_idx;
	u32 map_count;
	struct synx_signal_cb *signal_cb;
};

struct synx_client;
struct synx_device;

struct synx_handle_coredata {
	struct synx_client *client;
	struct synx_coredata *synx_obj;
	void *map_entry;
	struct kref refcount;
	u32 key;
	u32 rel_count;
	struct work_struct dispatch;
	struct hlist_node node;
};

struct synx_client {
	u32 type;
	bool active;
	struct synx_device *device;
	char name[SYNX_OBJ_NAME_LEN];
	u64 id;
	u64 dma_context;
	struct kref refcount;
	struct mutex event_q_lock;
	struct list_head event_q;
	wait_queue_head_t event_wq;
	DECLARE_BITMAP(cb_bitmap, SYNX_MAX_OBJS);
	struct synx_client_cb cb_table[SYNX_MAX_OBJS];
	DECLARE_HASHTABLE(handle_map, 8);
	spinlock_t handle_map_lock;
	struct work_struct dispatch;
	struct hlist_node node;
};

struct synx_native {
	spinlock_t metadata_map_lock;
	DECLARE_HASHTABLE(client_metadata_map, 8);
	spinlock_t fence_map_lock;
	DECLARE_HASHTABLE(fence_map, 10);
	spinlock_t global_map_lock;
	DECLARE_HASHTABLE(global_map, 10);
	spinlock_t local_map_lock;
	DECLARE_HASHTABLE(local_map, 8);
	spinlock_t csl_map_lock;
	DECLARE_HASHTABLE(csl_fence_map, 8);
	DECLARE_BITMAP(bitmap, SYNX_MAX_OBJS);
};

struct synx_cdsp_ssr {
	u64 ssrcnt;
	void *handle;
	struct notifier_block nb;
};

struct synx_device {
	struct cdev cdev;
	dev_t dev;
	struct class *class;
	struct synx_native *native;
	struct workqueue_struct *wq_cb;
	struct workqueue_struct *wq_cleanup;
	struct mutex vtbl_lock;
	struct synx_registered_ops bind_vtbl[SYNX_MAX_BIND_TYPES];
	struct dentry *debugfs_root;
	struct list_head error_list;
	struct mutex error_lock;
	struct synx_cdsp_ssr cdsp_ssr;
};

int synx_signal_core(struct synx_coredata *synx_obj,
	u32 status,
	bool cb_signal,
	s32 ext_sync_id);

int synx_ipc_callback(uint32_t client_id,
	int64_t data, void *priv);

void synx_signal_handler(struct work_struct *cb_dispatch);

int synx_native_release_core(struct synx_client *session,
	u32 h_synx);

int synx_bind(struct synx_session *session,
	u32 h_synx,
	struct synx_external_desc_v2 external_sync);

#endif /* __SYNX_PRIVATE_V2_H__ */
