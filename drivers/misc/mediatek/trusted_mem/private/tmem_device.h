/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef TMEM_DEVICE_H
#define TMEM_DEVICE_H

#include <linux/mutex.h>
#include <linux/workqueue.h>

#define SSMR_FEAT_INVALID_ID (0xFFFFFFFF)

#ifdef TCORE_PROFILING_SUPPORT
enum PROFILE_ENTRY_TYPE {
	PROFILE_ENTRY_SSMR_GET = 0,
	PROFILE_ENTRY_SSMR_PUT = 1,
	PROFILE_ENTRY_CHUNK_ALLOC = 2,
	PROFILE_ENTRY_CHUNK_FREE = 3,
	PROFILE_ENTRY_MEM_ADD = 4,
	PROFILE_ENTRY_MEM_REMOVE = 5,
	PROFILE_ENTRY_SESSION_OPEN = 6,
	PROFILE_ENTRY_SESSION_CLOSE = 7,
	PROFILE_ENTRY_INVOKE_COMMAND = 8,

	PROFILE_ENTRY_MAX = 9,
};

struct profile_data_item {
	u64 count;
	u64 sec;
	u64 usec;
	struct mutex lock;
};

struct profile_data_context {
	struct profile_data_item item[PROFILE_ENTRY_MAX];
};
#endif

enum TRUSTED_MEM_TYPE {
	TRUSTED_MEM_START = 0,
	TRUSTED_MEM_SVP = TRUSTED_MEM_START,
	TRUSTED_MEM_PROT = 1,
	TRUSTED_MEM_WFD = 2,
	TRUSTED_MEM_HAPP = 3,
	TRUSTED_MEM_HAPP_EXTRA = 4,
	TRUSTED_MEM_SDSP = 5,
	TRUSTED_MEM_SDSP_SHARED = 6,
	TRUSTED_MEM_2D_FR = 7,

	TRUSTED_MEM_MAX = 8,
	TRUSTED_MEM_INVALID = 0xFFFFFFFF
};

enum REGMGR_REGION_STATE {
	REGMGR_REGION_STATE_OFF = 0,
	REGMGR_REGION_STATE_ON = 1
};

struct trusted_peer_session {
	u64 mem_pa_start;
	u32 mem_size;
	u32 mem_size_runtime;
	u64 ref_chunks; /* chunks that are not freed yet! */
	bool opened;
	struct mutex lock;
	void *peer_data;
};

struct trusted_driver_cmd_params {
	u32 cmd;
	u64 param0;
	u64 param1;
	u64 param2;
	u64 param3;
};

struct trusted_driver_operations {
	int (*session_open)(void **peer_data, void *dev_desc);
	int (*session_close)(void *peer_data, void *dev_desc);
	int (*memory_alloc)(u32 alignment, u32 size, u32 *refcount,
			    u32 *sec_handle, u8 *owner, u32 id, u32 clean,
			    void *peer_data, void *dev_desc);
	int (*memory_free)(u32 sec_handle, u8 *owner, u32 id, void *peer_data,
			   void *priv);
	int (*memory_grant)(u64 pa, u32 size, void *peer_data, void *dev_desc);
	int (*memory_reclaim)(void *peer_data, void *dev_desc);
	int (*invoke_cmd)(struct trusted_driver_cmd_params *invoke_params,
			  void *peer_data, void *dev_desc);
};

struct ssmr_operations {
	int (*offline)(u64 *pa, u32 *size, u32 feat, void *dev_desc);
	int (*online)(u32 feat, void *dev_desc);
};

struct peer_mgr_desc {
	int (*mgr_sess_open)(struct trusted_driver_operations *drv_ops,
			     struct trusted_peer_session *sess_data,
			     void *dev_desc);
	int (*mgr_sess_close)(bool keep_alive,
			      struct trusted_driver_operations *drv_ops,
			      struct trusted_peer_session *sess_data,
			      void *dev_desc);
	int (*mgr_sess_mem_alloc)(u32 alignment, u32 size, u32 *refcount,
				  u32 *sec_handle, u8 *owner, u32 id, u32 clean,
				  struct trusted_driver_operations *drv_ops,
				  struct trusted_peer_session *sess_data,
				  void *dev_desc);
	int (*mgr_sess_mem_free)(u32 sec_handle, u8 *owner, u32 id,
				 struct trusted_driver_operations *drv_ops,
				 struct trusted_peer_session *sess_data,
				 void *dev_desc);
	int (*mgr_sess_mem_add)(u64 pa, u32 size,
				struct trusted_driver_operations *drv_ops,
				struct trusted_peer_session *sess_data,
				void *dev_desc);
	int (*mgr_sess_mem_remove)(struct trusted_driver_operations *drv_ops,
				   struct trusted_peer_session *sess_data,
				   void *dev_desc);
	int (*mgr_sess_invoke_cmd)(
		struct trusted_driver_cmd_params *invoke_params,
		struct trusted_driver_operations *drv_ops,
		struct trusted_peer_session *sess_data, void *dev_desc);
	struct trusted_peer_session peer_mgr_data;
};

struct region_mgr_work_data {
};

struct region_mgr_desc {
	struct workqueue_struct *defer_off_wq;
	struct delayed_work defer_off_work;
	struct mutex lock;

	u64 online_acc_count;
	u32 valid_ref_count;
	u32 defer_off_delay_ms;
	enum REGMGR_REGION_STATE state;
	void *mem_device;
	enum TRUSTED_MEM_TYPE active_mem_type;
};

#define MAX_DEVICE_NAME_LEN (32)

struct trusted_mem_configs {
	u32 caps;
	u32 minimal_chunk_size;
	u32 phys_mem_shift_bits;
	u32 phys_limit_min_alloc_size;
	bool session_keep_alive_enable;
	bool min_size_check_enable;
	bool alignment_check_enable;
};

#ifdef TCORE_PROFILING_SUPPORT
struct profile_mgr_desc {
	struct profile_data_context data;
	struct ssmr_operations *profiled_ssmr_ops;
	struct trusted_driver_operations *profiled_peer_ops;
	void *profiled_dev_desc;
};
#endif

struct trusted_mem_device {
	struct ssmr_operations *ssmr_ops;
	struct trusted_driver_operations *peer_ops;
	void *dev_desc;

	struct peer_mgr_desc *peer_mgr;
	struct region_mgr_desc *reg_mgr;

	char name[MAX_DEVICE_NAME_LEN];
	enum TRUSTED_MEM_TYPE mem_type;
	u32 ssmr_feature_id;
	bool is_device_busy;

	struct trusted_mem_configs configs;
#ifdef TCORE_PROFILING_SUPPORT
	struct profile_mgr_desc *profile_mgr;
#endif
};

#endif /* end of TMEM_DEVICE_H */
