/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __SYNX_SHARED_MEM_H__
#define __SYNX_SHARED_MEM_H__

#include "synx_err.h"
#include "ipclite_client.h"

#include <uapi/media/synx.h>

/**
 * enum synx_core_id - Synx core IDs
 *
 * SYNX_CORE_APSS : APSS core
 * SYNX_CORE_NSP  : NSP core
 * SYNX_CORE_EVA  : EVA core
 * SYNX_CORE_IRIS : IRIS core
 */
enum synx_core_id {
	SYNX_CORE_APSS = 0,
	SYNX_CORE_NSP,
	SYNX_CORE_EVA,
	SYNX_CORE_IRIS,
	SYNX_CORE_MAX,
};

/* synx handle encoding */
#define SYNX_HANDLE_INDEX_BITS         16
#define SYNX_HANDLE_CORE_BITS          4
#define SYNX_HANDLE_GLOBAL_FLAG_BIT    1

#define SYNX_GLOBAL_SHARED_LOCKS       1
#define SYNX_GLOBAL_MAX_OBJS           4096
#define SYNX_GLOBAL_MAX_PARENTS        4

#define SYNX_HANDLE_INDEX_MASK         ((1UL<<SYNX_HANDLE_INDEX_BITS)-1)

#define SHRD_MEM_DUMP_NUM_BMAP_WORDS   10
#define NUM_CHAR_BIT                   8

/* spin lock timeout (ms) */
#define SYNX_HWSPIN_TIMEOUT            500
#define SYNX_HWSPIN_ID                 10

/* internal signal states */
#define SYNX_STATE_INVALID             0
#define SYNX_STATE_ACTIVE              1
#define SYNX_STATE_SIGNALED_ERROR      3
#define SYNX_STATE_SIGNALED_EXTERNAL   5
#define SYNX_STATE_SIGNALED_SSR        6

/**
 * struct synx_global_coredata - Synx global object, used for book keeping
 * of all metadata associated with each individual global entry
 *
 * @status      : Synx signaling status
 * @handle      : Handle of global entry
 * @refcount    : References owned by each core
 * @num_child   : Count of children pending signal (for composite handle)
 * @subscribers : Cores owning reference on this object
 * @waiters     : Cores waiting for notification
 * @parents     : Composite global coredata index of parent entities
 *                Can be part of SYNX_GLOBAL_MAX_PARENTS composite entries.
 */
struct synx_global_coredata {
	u32 status;
	u32 handle;
	u16 refcount;
	u16 num_child;
	u16 subscribers;
	u16 waiters;
	u16 parents[SYNX_GLOBAL_MAX_PARENTS];
};

/**
 * struct synx_shared_mem - Synx global shared memory descriptor
 *
 * @bitmap : Bitmap for allocating entries form table
 * @locks  : Array of locks for exclusive access to table entries
 * @table  : Array of Synx global entries
 */
struct synx_shared_mem {
	u32 *bitmap;
	u32 *locks;
	struct synx_global_coredata *table;
};

static inline bool synx_is_valid_idx(u32 idx)
{
	if (idx < SYNX_GLOBAL_MAX_OBJS)
		return true;
	return false;
}

/**
 * synx_global_mem_init - Initialize global shared memory
 *
 * @return Zero on success, negative error on failure.
 */
int synx_global_mem_init(void);

/**
 * synx_global_map_core_id - Map Synx core ID to IPC Lite host
 *
 * @param id : Core Id to map
 *
 * @return IPC host ID.
 */
u32 synx_global_map_core_id(enum synx_core_id id);

/**
 * synx_global_alloc_index - Allocate new global entry
 *
 * @param idx : Pointer to global table index (filled by function)
 *
 * @return SYNX_SUCCESS on success. Negative error on failure.
 */
int synx_global_alloc_index(u32 *idx);

/**
 * synx_global_init_coredata - Allocate new global entry
 *
 * @param h_synx : Synx global handle
 *
 * @return SYNX_SUCCESS on success. Negative error on failure.
 */
int synx_global_init_coredata(u32 h_synx);

/**
 * synx_global_get_waiting_cores - Get list of all the waiting core on global entry
 *
 * Will fill the cores array with TRUE if core is waiting, and
 * false if not. Indexed through enum synx_core_id.
 *
 * @param idx   : Global entry index
 * @param cores : Array of boolean variables, one each for supported core.
 *                Array should contain SYNX_CORE_MAX entries.
 *
 * @return SYNX_SUCCESS on success. Negative error on failure.
 */
int synx_global_get_waiting_cores(u32 idx, bool *cores);

/**
 * synx_global_set_waiting_core - Set core as a waiting core on global entry
 *
 * @param idx : Global entry index
 * @param id  : Core to be set as waiter
 *
 * @return SYNX_SUCCESS on success. Negative error on failure.
 */
int synx_global_set_waiting_core(u32 idx, enum synx_core_id id);

/**
 * synx_global_get_subscribed_cores - Get list of all the subscribed core on global entry
 *
 * Will fill the cores array with TRUE if core is subscribed, and
 * false if not. Indexed through enum synx_core_id.
 *
 * @param idx   : Global entry index
 * @param cores : Array of boolean variables, one each for supported core.
 *                Array should contain SYNX_CORE_MAX entries.
 *
 * @return SYNX_SUCCESS on success. Negative error on failure.
 */
int synx_global_get_subscribed_cores(u32 idx, bool *cores);

/**
 * synx_global_set_subscribed_core - Set core as a subscriber core on global entry
 *
 * @param idx : Global entry index
 * @param id  : Core to be added as subscriber
 *
 * @return SYNX_SUCCESS on success. Negative error on failure.
 */
int synx_global_set_subscribed_core(u32 idx, enum synx_core_id id);

/**
 * synx_global_clear_subscribed_core - Clear core as a subscriber core on global entry
 *
 * @param idx : Global entry index
 * @param id  : Core to be added as subscriber
 *
 * @return SYNX_SUCCESS on success. Negative error on failure.
 */
int synx_global_clear_subscribed_core(u32 idx, enum synx_core_id id);

/**
 * synx_global_get_status - Get status of the global entry
 *
 * @param idx : Global entry index
 *
 * @return Global entry status
 */
u32 synx_global_get_status(u32 idx);

/**
 * synx_global_test_status_set_wait - Check status and add core as waiter is not signaled
 *
 * This tests and adds the waiter in one atomic operation, to avoid
 * race with signal which can miss sending the IPC signal if
 * check status and set as done as two different operations
 * (signal coming in between the two ops).
 *
 * @param idx : Global entry index
 * @param id  : Core to be set as waiter (if unsignaled)
 *
 * @return Status of global entry idx.
 */
u32 synx_global_test_status_set_wait(u32 idx,
	enum synx_core_id id);

/**
 * synx_global_update_status - Update status of the global entry
 *
 * Function also updates the parent composite handles
 * about the signaling.
 *
 * @param idx    : Global entry index
 * @param status : Signaling status
 *
 * @return SYNX_SUCCESS on success. Negative error on failure.
 */
int synx_global_update_status(u32 idx, u32 status);

/**
 * synx_global_get_ref - Get additional reference on global entry
 *
 * @param idx : Global entry index
 *
 * @return SYNX_SUCCESS on success. Negative error on failure.
 */
int synx_global_get_ref(u32 idx);

/**
 * synx_global_put_ref - Release reference on global entry
 *
 * @param idx : Global entry index
 */
void synx_global_put_ref(u32 idx);

/**
 * synx_global_get_parents - Get the global entry index of all composite parents
 *
 * @param idx     : Global entry index whose parents are requested
 * @param parents : Array of global entry index of composite handles
 *                  Filled by the function. Array should contain atleast
 *                  SYNX_GLOBAL_MAX_PARENTS entries.
 *
 * @return SYNX_SUCCESS on success. Negative error on failure.
 */
int synx_global_get_parents(u32 idx, u32 *parents);

/**
 * synx_global_merge - Merge handles to form global handle
 *
 * Is essential for merge functionality.
 *
 * @param idx_list : List of global indexes to merge
 * @param num_list : Number of handles in the list to merge
 * @params p_idx   : Global entry index allocated for composite handle
 *
 * @return SYNX_SUCCESS on success. Negative error on failure.
 */
int synx_global_merge(u32 *idx_list, u32 num_list, u32 p_idx);

/**
 * synx_global_recover - Recover handles subscribed by specific core
 *
 * @param id : Core ID to clean up
 *
 * @return SYNX_SUCCESS on success. Negative error on failure.
 */
int synx_global_recover(enum synx_core_id id);

/**
 * synx_global_clean_cdsp_mem - Release handles created/used by CDSP
 *
 * @return SYNX_SUCCESS on success. Negative error on failure.
 */
int synx_global_clean_cdsp_mem(void);

/**
 * synx_global_dump_shared_memory - Prints the top entries of
 * bitmap and table in global shared memory.
 *
 * @return SYNX_SUCCESS on success. Negative error on failure.
 */

int synx_global_dump_shared_memory(void);

#endif /* __SYNX_SHARED_MEM_H__ */
