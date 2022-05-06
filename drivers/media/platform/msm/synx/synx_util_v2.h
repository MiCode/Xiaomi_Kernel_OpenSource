/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __SYNX_UTIL_V2_H__
#define __SYNX_UTIL_V2_H__

#include "synx_api.h"
#include "synx_private_v2.h"

extern struct synx_device *synx_dev;

extern void synx_fence_callback(struct dma_fence *fence,
	struct dma_fence_cb *cb);
extern int synx_native_signal_fence(struct synx_coredata *synx_obj,
	u32 status);

static inline bool synx_util_is_valid_bind_type(u32 type)
{
	if (type < SYNX_MAX_BIND_TYPES)
		return true;

	return false;
}

static inline bool synx_util_is_global_handle(u32 h_synx)
{
	return (h_synx & SYNX_OBJ_GLOBAL_FLAG_MASK) ? true : false;
}

static inline u32 synx_util_get_object_type(
	struct synx_coredata *synx_obj)
{
	return synx_obj ? synx_obj->type : 0;
}

static inline bool synx_util_is_merged_object(
	struct synx_coredata *synx_obj)
{
	if (synx_obj &&
		(synx_obj->type & SYNX_CREATE_MERGED_FENCE))
		return true;

	return false;
}

static inline bool synx_util_is_global_object(
	struct synx_coredata *synx_obj)
{
	if (synx_obj &&
		(synx_obj->type & SYNX_CREATE_GLOBAL_FENCE))
		return true;

	return false;
}

static inline bool synx_util_is_external_object(
	struct synx_coredata *synx_obj)
{
	if (synx_obj &&
		(synx_obj->type & SYNX_CREATE_DMA_FENCE))
		return true;

	return false;
}

static inline u32 synx_util_map_params_to_type(u32 flags)
{
	if (flags & SYNX_CREATE_CSL_FENCE)
		return SYNX_TYPE_CSL;

	return SYNX_MAX_BIND_TYPES;
}

static inline u32 synx_util_global_idx(u32 h_synx)
{
	return (h_synx & SYNX_OBJ_HANDLE_MASK);
}

/* coredata memory functions */
void synx_util_get_object(struct synx_coredata *synx_obj);
void synx_util_put_object(struct synx_coredata *synx_obj);
void synx_util_object_destroy(struct synx_coredata *synx_obj);

static inline struct synx_coredata *synx_util_obtain_object(
	struct synx_handle_coredata *synx_data)
{
	if (IS_ERR_OR_NULL(synx_data))
		return NULL;

	return synx_data->synx_obj;
}

/* global/local map functions */
struct synx_map_entry *synx_util_insert_to_map(struct synx_coredata *synx_obj,
			u32 h_synx, u32 flags);
struct synx_map_entry *synx_util_get_map_entry(u32 h_synx);
void synx_util_release_map_entry(struct synx_map_entry *map_entry);

/* fence map functions */
int synx_util_insert_fence_entry(struct synx_fence_entry *entry, u32 *h_synx,
			u32 global);
u32 synx_util_get_fence_entry(u64 key, u32 global);
void synx_util_release_fence_entry(u64 key);

/* coredata initialize functions */
int synx_util_init_coredata(struct synx_coredata *synx_obj,
			struct synx_create_params *params,
			struct dma_fence_ops *ops,
			u64 dma_context);
int synx_util_init_group_coredata(struct synx_coredata *synx_obj,
			struct dma_fence **fences,
			struct synx_merge_params *params,
			u32 num_objs,
			u64 dma_context);

/* handle related functions */
int synx_alloc_global_handle(u32 *new_synx);
int synx_alloc_local_handle(u32 *new_synx);
long synx_util_get_free_handle(unsigned long *bitmap, unsigned int size);
int synx_util_init_handle(struct synx_client *client, struct synx_coredata *obj,
			u32 *new_h_synx,
			void *map_entry);

u32 synx_encode_handle(u32 idx, u32 core_id, bool global_idx);

/* callback related functions */
int synx_util_alloc_cb_entry(struct synx_client *client,
			struct synx_kernel_payload *data,
			u32 *cb_idx);
int synx_util_clear_cb_entry(struct synx_client *client,
			struct synx_client_cb *cb);
void synx_util_default_user_callback(u32 h_synx, int status, void *data);
void synx_util_callback_dispatch(struct synx_coredata *synx_obj, u32 state);
void synx_util_cb_dispatch(struct work_struct *cb_dispatch);

/* external fence functions */
int synx_util_activate(struct synx_coredata *synx_obj);
int synx_util_add_callback(struct synx_coredata *synx_obj, u32 h_synx);

/* merge related helper functions */
s32 synx_util_merge_error(struct synx_client *client, u32 *h_synxs, u32 num_objs);
int synx_util_validate_merge(struct synx_client *client, u32 *h_synxs, u32 num_objs,
			struct dma_fence ***fences,
			u32 *fence_cnt);

/* coredata status functions */
u32 synx_util_get_object_status(struct synx_coredata *synx_obj);
u32 synx_util_get_object_status_locked(struct synx_coredata *synx_obj);

/* client handle map related functions */
struct synx_handle_coredata *synx_util_acquire_handle(struct synx_client *client,
			u32 h_synx);
void synx_util_release_handle(struct synx_handle_coredata *synx_data);
int synx_util_update_handle(struct synx_client *client, u32 h_synx, u32 sync_id,
			u32 type, struct synx_handle_coredata **handle);

/* client memory handler functions */
struct synx_client *synx_get_client(struct synx_session *session);
void synx_put_client(struct synx_client *client);

/* error log functions */
void synx_util_generate_timestamp(char *timestamp, size_t size);
void synx_util_log_error(u32 id, u32 h_synx, s32 err);

/* external fence map functions */
int synx_util_save_data(void *fence, u32 flags, u32 data);
struct synx_entry_64 *synx_util_retrieve_data(void *fence, u32 type);
void synx_util_remove_data(void *fence, u32 type);

/* misc */
void synx_util_map_import_params_to_create(
			struct synx_import_indv_params *params,
			struct synx_create_params *c_params);

struct bind_operations *synx_util_get_bind_ops(u32 type);
u32 synx_util_map_client_id_to_core(enum synx_client_id id);

#endif /* __SYNX_UTIL_V2_H__ */
