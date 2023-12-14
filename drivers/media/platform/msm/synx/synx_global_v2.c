// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/hwspinlock.h>
#include <linux/string.h>
#include <linux/slab.h>

#include "synx_debugfs_v2.h"
#include "synx_global_v2.h"

static struct synx_shared_mem synx_gmem;
static struct hwspinlock *synx_hwlock;

static u32 synx_gmem_lock_owner(u32 idx)
{
	/*
	 * subscribers field of global table index 0 is used to
	 * maintain synx gmem lock owner data.
	 * core updates the field after acquiring the lock and
	 * before releasing the lock appropriately.
	 */
	return synx_gmem.table[0].subscribers;
}

static void synx_gmem_lock_owner_set(u32 idx)
{
	synx_gmem.table[0].subscribers = SYNX_CORE_APSS;
}

static void synx_gmem_lock_owner_clear(u32 idx)
{
	if (synx_gmem.table[0].subscribers != SYNX_CORE_APSS)
		dprintk(SYNX_WARN, "reset lock owned by core %u\n",
			synx_gmem.table[0].subscribers);

	synx_gmem.table[0].subscribers = SYNX_CORE_MAX;
}

static int synx_gmem_lock(u32 idx, unsigned long *flags)
{
	int rc;

	if (!synx_hwlock)
		return -SYNX_INVALID;

	rc = hwspin_lock_timeout_irqsave(
		synx_hwlock, SYNX_HWSPIN_TIMEOUT, flags);
	if (!rc)
		synx_gmem_lock_owner_set(idx);

	return rc;
}

static void synx_gmem_unlock(u32 idx, unsigned long *flags)
{
	synx_gmem_lock_owner_clear(idx);
	hwspin_unlock_irqrestore(synx_hwlock, flags);
}

static void synx_global_print_data(
	struct synx_global_coredata *synx_g_obj,
	const char *func)
{
	int i = 0;

	dprintk(SYNX_VERB, "%s: status %u, handle %u, refcount %u",
		func, synx_g_obj->status,
		synx_g_obj->handle, synx_g_obj->refcount);

	dprintk(SYNX_VERB, "%s: subscribers %u, waiters %u, pending %u",
		func, synx_g_obj->subscribers, synx_g_obj->waiters,
		synx_g_obj->num_child);

	for (i = 0; i < SYNX_GLOBAL_MAX_PARENTS; i++)
		if (synx_g_obj->parents[i])
			dprintk(SYNX_VERB, "%s: parents %u:%u",
				func, i, synx_g_obj->parents[i]);
}

int synx_global_dump_shared_memory(void)
{
	int rc = SYNX_SUCCESS, idx;
	unsigned long flags;
	struct synx_global_coredata *synx_g_obj;

	if (!synx_gmem.table)
		return -SYNX_INVALID;

	/* Print bitmap memory*/
	for (idx = 0; idx < SHRD_MEM_DUMP_NUM_BMAP_WORDS; idx++) {
		rc = synx_gmem_lock(idx, &flags);

		if (rc)
			return rc;

		dprintk(SYNX_VERB, "%s: idx %d, bitmap value %d",
		__func__, idx, synx_gmem.bitmap[idx]);

		synx_gmem_unlock(idx, &flags);
	}

	/* Print table memory*/
	for (idx = 0;
		idx < SHRD_MEM_DUMP_NUM_BMAP_WORDS * sizeof(u32) * NUM_CHAR_BIT;
		idx++) {
		rc = synx_gmem_lock(idx, &flags);

		if (rc)
			return rc;

		dprintk(SYNX_VERB, "%s: idx %d\n", __func__, idx);

		synx_g_obj = &synx_gmem.table[idx];
		synx_global_print_data(synx_g_obj, __func__);

		synx_gmem_unlock(idx, &flags);
	}
	return rc;
}

static int synx_gmem_init(void)
{
	if (!synx_gmem.table)
		return -SYNX_NOMEM;

	synx_hwlock = hwspin_lock_request_specific(SYNX_HWSPIN_ID);
	if (!synx_hwlock) {
		dprintk(SYNX_ERR, "hwspinlock request failed\n");
		return -SYNX_NOMEM;
	}

	/* zero idx not allocated for clients */
	ipclite_global_test_and_set_bit(0,
		(ipclite_atomic_uint32_t *)synx_gmem.bitmap);
	memset(&synx_gmem.table[0], 0, sizeof(struct synx_global_coredata));

	return SYNX_SUCCESS;
}

u32 synx_global_map_core_id(enum synx_core_id id)
{
	u32 host_id;

	switch (id) {
	case SYNX_CORE_APSS:
		host_id = IPCMEM_APPS; break;
	case SYNX_CORE_NSP:
		host_id = IPCMEM_CDSP; break;
	case SYNX_CORE_IRIS:
		host_id = IPCMEM_VPU; break;
	case SYNX_CORE_EVA:
		host_id = IPCMEM_CVP; break;
	default:
		host_id = IPCMEM_NUM_HOSTS;
		dprintk(SYNX_ERR, "invalid core id\n");
	}

	return host_id;
}

int synx_global_alloc_index(u32 *idx)
{
	int rc = SYNX_SUCCESS;
	u32 prev, index;
	const u32 size = SYNX_GLOBAL_MAX_OBJS;

	if (!synx_gmem.table)
		return -SYNX_NOMEM;

	if (IS_ERR_OR_NULL(idx))
		return -SYNX_INVALID;

	do {
		index = find_first_zero_bit((unsigned long *)synx_gmem.bitmap, size);
		if (index >= size) {
			rc = -SYNX_NOMEM;
			break;
		}
		prev = ipclite_global_test_and_set_bit(index % 32,
				(ipclite_atomic_uint32_t *)(synx_gmem.bitmap + index/32));
		if ((prev & (1UL << (index % 32))) == 0) {
			*idx = index;
			dprintk(SYNX_MEM, "allocated global idx %u\n", *idx);
			break;
		}
	} while (true);

	return rc;
}

int synx_global_init_coredata(u32 h_synx)
{
	int rc;
	unsigned long flags;
	struct synx_global_coredata *synx_g_obj;
	u32 idx = h_synx & SYNX_HANDLE_INDEX_MASK;

	if (!synx_gmem.table)
		return -SYNX_NOMEM;

	if (!synx_is_valid_idx(idx))
		return -SYNX_INVALID;

	rc = synx_gmem_lock(idx, &flags);
	if (rc)
		return rc;
	synx_g_obj = &synx_gmem.table[idx];
	if (synx_g_obj->status != 0 || synx_g_obj->refcount != 0 ||
		synx_g_obj->subscribers != 0 || synx_g_obj->handle != 0 ||
		synx_g_obj->parents[0] != 0) {
		dprintk(SYNX_ERR,
				"entry not cleared for idx %u,\n"
				"synx_g_obj->status %d,\n"
				"synx_g_obj->refcount %d,\n"
				"synx_g_obj->subscribers %d,\n"
				"synx_g_obj->handle %u,\n"
				"synx_g_obj->parents[0] %d\n",
				idx, synx_g_obj->status,
				synx_g_obj->refcount,
				synx_g_obj->subscribers,
				synx_g_obj->handle,
				synx_g_obj->parents[0]);
		synx_gmem_unlock(idx, &flags);
		return -SYNX_INVALID;
	}
	memset(synx_g_obj, 0, sizeof(*synx_g_obj));
	/* set status to active */
	synx_g_obj->status = SYNX_STATE_ACTIVE;
	synx_g_obj->refcount = 1;
	synx_g_obj->subscribers = (1UL << SYNX_CORE_APSS);
	synx_g_obj->handle = h_synx;
	synx_gmem_unlock(idx, &flags);

	return SYNX_SUCCESS;
}

static int synx_global_get_waiting_cores_locked(
	struct synx_global_coredata *synx_g_obj,
	bool *cores)
{
	int i;

	synx_global_print_data(synx_g_obj, __func__);
	for (i = 0; i < SYNX_CORE_MAX; i++) {
		if (synx_g_obj->waiters & (1UL << i)) {
			cores[i] = true;
			dprintk(SYNX_VERB,
				"waiting for handle %u/n",
				synx_g_obj->handle);
		}
	}

	/* clear waiter list so signals are not repeated */
	synx_g_obj->waiters = 0;

	return SYNX_SUCCESS;
}

int synx_global_get_waiting_cores(u32 idx, bool *cores)
{
	int rc;
	unsigned long flags;
	struct synx_global_coredata *synx_g_obj;

	if (!synx_gmem.table)
		return -SYNX_NOMEM;

	if (IS_ERR_OR_NULL(cores) || !synx_is_valid_idx(idx))
		return -SYNX_INVALID;

	rc = synx_gmem_lock(idx, &flags);
	if (rc)
		return rc;
	synx_g_obj = &synx_gmem.table[idx];
	synx_global_get_waiting_cores_locked(synx_g_obj, cores);
	synx_gmem_unlock(idx, &flags);

	return SYNX_SUCCESS;
}

int synx_global_set_waiting_core(u32 idx, enum synx_core_id id)
{
	int rc;
	unsigned long flags;
	struct synx_global_coredata *synx_g_obj;

	if (!synx_gmem.table)
		return -SYNX_NOMEM;

	if (id >= SYNX_CORE_MAX || !synx_is_valid_idx(idx))
		return -SYNX_INVALID;

	rc = synx_gmem_lock(idx, &flags);
	if (rc)
		return rc;
	synx_g_obj = &synx_gmem.table[idx];
	synx_g_obj->waiters |= (1UL << id);
	synx_gmem_unlock(idx, &flags);

	return SYNX_SUCCESS;
}

int synx_global_get_subscribed_cores(u32 idx, bool *cores)
{
	int i;
	int rc;
	unsigned long flags;
	struct synx_global_coredata *synx_g_obj;

	if (!synx_gmem.table)
		return -SYNX_NOMEM;

	if (IS_ERR_OR_NULL(cores) || !synx_is_valid_idx(idx))
		return -SYNX_INVALID;

	rc = synx_gmem_lock(idx, &flags);
	if (rc)
		return rc;
	synx_g_obj = &synx_gmem.table[idx];
	for (i = 0; i < SYNX_CORE_MAX; i++)
		if (synx_g_obj->subscribers & (1UL << i))
			cores[i] = true;
	synx_gmem_unlock(idx, &flags);

	return SYNX_SUCCESS;
}

int synx_global_set_subscribed_core(u32 idx, enum synx_core_id id)
{
	int rc;
	unsigned long flags;
	struct synx_global_coredata *synx_g_obj;

	if (!synx_gmem.table)
		return -SYNX_NOMEM;

	if (id >= SYNX_CORE_MAX || !synx_is_valid_idx(idx))
		return -SYNX_INVALID;

	rc = synx_gmem_lock(idx, &flags);
	if (rc)
		return rc;
	synx_g_obj = &synx_gmem.table[idx];
	synx_g_obj->subscribers |= (1UL << id);
	synx_gmem_unlock(idx, &flags);

	return SYNX_SUCCESS;
}

int synx_global_clear_subscribed_core(u32 idx, enum synx_core_id id)
{
	int rc;
	unsigned long flags;
	struct synx_global_coredata *synx_g_obj;

	if (!synx_gmem.table)
		return -SYNX_NOMEM;

	if (id >= SYNX_CORE_MAX || !synx_is_valid_idx(idx))
		return -SYNX_INVALID;

	rc = synx_gmem_lock(idx, &flags);
	if (rc)
		return rc;
	synx_g_obj = &synx_gmem.table[idx];
	synx_g_obj->subscribers &= ~(1UL << id);
	synx_gmem_unlock(idx, &flags);

	return SYNX_SUCCESS;
}

u32 synx_global_get_parents_num(u32 idx)
{
	int rc;
	unsigned long flags;
	struct synx_global_coredata *synx_g_obj;
	u32 i, count = 0;

	if (!synx_gmem.table)
		return 0;

	if (!synx_is_valid_idx(idx))
		return 0;

	rc = synx_gmem_lock(idx, &flags);
	if (rc)
		return rc;
	synx_g_obj = &synx_gmem.table[idx];
	for (i = 0; i < SYNX_GLOBAL_MAX_PARENTS; i++) {
		if (synx_g_obj->parents[i] != 0)
			count++;
	}
	synx_gmem_unlock(idx, &flags);

	return count;
}

static int synx_global_get_parents_locked(
	struct synx_global_coredata *synx_g_obj, u32 *parents)
{
	u32 i;

	if (!synx_g_obj || !parents)
		return -SYNX_NOMEM;

	for (i = 0; i < SYNX_GLOBAL_MAX_PARENTS; i++)
		parents[i] = synx_g_obj->parents[i];

	return SYNX_SUCCESS;
}

int synx_global_get_parents(u32 idx, u32 *parents)
{
	int rc;
	unsigned long flags;
	struct synx_global_coredata *synx_g_obj;

	if (!synx_gmem.table || !parents)
		return -SYNX_NOMEM;

	if (!synx_is_valid_idx(idx))
		return -SYNX_INVALID;

	rc = synx_gmem_lock(idx, &flags);
	if (rc)
		return rc;
	synx_g_obj = &synx_gmem.table[idx];
	rc = synx_global_get_parents_locked(synx_g_obj, parents);
	synx_gmem_unlock(idx, &flags);

	return rc;
}

u32 synx_global_get_status(u32 idx)
{
	int rc;
	unsigned long flags;
	u32 status;
	struct synx_global_coredata *synx_g_obj;

	if (!synx_gmem.table)
		return 0;

	if (!synx_is_valid_idx(idx))
		return 0;

	rc = synx_gmem_lock(idx, &flags);
	if (rc)
		return rc;
	synx_g_obj = &synx_gmem.table[idx];
	status = synx_g_obj->status;
	synx_gmem_unlock(idx, &flags);

	return status;
}

u32 synx_global_test_status_set_wait(u32 idx,
	enum synx_core_id id)
{
	int rc;
	unsigned long flags;
	u32 status;
	struct synx_global_coredata *synx_g_obj;

	if (!synx_gmem.table)
		return 0;

	if (id >= SYNX_CORE_MAX || !synx_is_valid_idx(idx))
		return 0;

	rc = synx_gmem_lock(idx, &flags);
	if (rc)
		return 0;
	synx_g_obj = &synx_gmem.table[idx];
	synx_global_print_data(synx_g_obj, __func__);
	status = synx_g_obj->status;
	/* if handle is still ACTIVE */
	if (status == SYNX_STATE_ACTIVE)
		synx_g_obj->waiters |= (1UL << id);
	else
		dprintk(SYNX_DBG, "handle %u already signaled %u",
			synx_g_obj->handle, synx_g_obj->status);
	synx_gmem_unlock(idx, &flags);

	return status;
}

static int synx_global_update_status_core(u32 idx,
	u32 status)
{
	u32 i, p_idx;
	int rc;
	bool clear = false;
	unsigned long flags;
	uint64_t data;
	struct synx_global_coredata *synx_g_obj;
	u32 h_parents[SYNX_GLOBAL_MAX_PARENTS] = {0};
	bool wait_cores[SYNX_CORE_MAX] = {false};

	rc = synx_gmem_lock(idx, &flags);
	if (rc)
		return rc;
	synx_g_obj = &synx_gmem.table[idx];
	synx_global_print_data(synx_g_obj, __func__);
	/* prepare for cross core signaling */
	data = synx_g_obj->handle;
	data <<= 32;
	if (synx_g_obj->num_child != 0) {
		/* composite handle */
		synx_g_obj->num_child--;
		if (synx_g_obj->num_child == 0) {
			if (synx_g_obj->status == SYNX_STATE_ACTIVE) {
				synx_g_obj->status =
					(status == SYNX_STATE_SIGNALED_SUCCESS) ?
					SYNX_STATE_SIGNALED_SUCCESS : SYNX_STATE_SIGNALED_ERROR;
				data |= synx_g_obj->status;
				synx_global_get_waiting_cores_locked(synx_g_obj,
					wait_cores);
				synx_global_get_parents_locked(synx_g_obj, h_parents);
			} else {
				data = 0;
				dprintk(SYNX_WARN,
					"merged handle %u already in state %u\n",
					synx_g_obj->handle, synx_g_obj->status);
			}
			/* release ref held by constituting handles */
			synx_g_obj->refcount--;
			if (synx_g_obj->refcount == 0) {
				memset(synx_g_obj, 0,
					sizeof(*synx_g_obj));
				clear = true;
			}
		} else if (status != SYNX_STATE_SIGNALED_SUCCESS) {
			synx_g_obj->status = SYNX_STATE_SIGNALED_ERROR;
			data |= synx_g_obj->status;
			synx_global_get_waiting_cores_locked(synx_g_obj,
				wait_cores);
			synx_global_get_parents_locked(synx_g_obj, h_parents);
			dprintk(SYNX_WARN,
				"merged handle %u signaled with error state\n",
				synx_g_obj->handle);
		} else {
			/* pending notification from  handles */
			data = 0;
			dprintk(SYNX_DBG,
				"Child notified parent handle %u, pending %u\n",
				synx_g_obj->handle, synx_g_obj->num_child);
		}
	} else {
		synx_g_obj->status = status;
		data |= synx_g_obj->status;
		synx_global_get_waiting_cores_locked(synx_g_obj,
			wait_cores);
		synx_global_get_parents_locked(synx_g_obj, h_parents);
	}
	synx_gmem_unlock(idx, &flags);

	if (clear) {
		ipclite_global_test_and_clear_bit(idx%32,
			(ipclite_atomic_uint32_t *)(synx_gmem.bitmap + idx/32));
		dprintk(SYNX_MEM,
			"cleared global idx %u\n", idx);
	}

	/* notify waiting clients on signal */
	if (data) {
		/* notify wait client */

	/* In case of SSR, someone might be waiting on same core
	 * However, in other cases, synx_signal API will take care
	 * of signaling handles on same core and thus we don't need
	 * to send interrupt
	 */
		if (status == SYNX_STATE_SIGNALED_SSR)
			i = 0;
		else
			i = 1;

		for (; i < SYNX_CORE_MAX ; i++) {
			if (!wait_cores[i])
				continue;
			dprintk(SYNX_DBG,
				"invoking ipc signal handle %u, status %u\n",
				synx_g_obj->handle, synx_g_obj->status);
			if (ipclite_msg_send(
				synx_global_map_core_id(i),
				data))
				dprintk(SYNX_ERR,
					"ipc signaling %llu to core %u failed\n",
					data, i);
		}
	}

	/* handle parent notifications */
	for (i = 0; i < SYNX_GLOBAL_MAX_PARENTS; i++) {
		p_idx = h_parents[i];
		if (p_idx == 0)
			continue;
		synx_global_update_status_core(p_idx, status);
	}

	return SYNX_SUCCESS;
}

int synx_global_update_status(u32 idx, u32 status)
{
	int rc = -SYNX_INVALID;
	unsigned long flags;
	struct synx_global_coredata *synx_g_obj;

	if (!synx_gmem.table)
		return -SYNX_NOMEM;

	if (!synx_is_valid_idx(idx) || status <= SYNX_STATE_ACTIVE)
		return -SYNX_INVALID;

	rc = synx_gmem_lock(idx, &flags);
	if (rc)
		return rc;
	synx_g_obj = &synx_gmem.table[idx];
	if (synx_g_obj->num_child != 0) {
		/* composite handle cannot be signaled */
		goto fail;
	} else if (synx_g_obj->status != SYNX_STATE_ACTIVE) {
		rc = -SYNX_ALREADY;
		goto fail;
	}
	synx_gmem_unlock(idx, &flags);

	return synx_global_update_status_core(idx, status);

fail:
	synx_gmem_unlock(idx, &flags);
	return rc;
}

int synx_global_get_ref(u32 idx)
{
	int rc;
	unsigned long flags;
	struct synx_global_coredata *synx_g_obj;

	if (!synx_gmem.table)
		return -SYNX_NOMEM;

	if (!synx_is_valid_idx(idx))
		return -SYNX_INVALID;

	rc = synx_gmem_lock(idx, &flags);
	if (rc)
		return rc;
	synx_g_obj = &synx_gmem.table[idx];
	synx_global_print_data(synx_g_obj, __func__);
	if (synx_g_obj->handle && synx_g_obj->refcount)
		synx_g_obj->refcount++;
	else
		rc = -SYNX_NOENT;
	synx_gmem_unlock(idx, &flags);

	return rc;
}

void synx_global_put_ref(u32 idx)
{
	int rc;
	bool clear = false;
	unsigned long flags;
	struct synx_global_coredata *synx_g_obj;

	if (!synx_gmem.table)
		return;

	if (!synx_is_valid_idx(idx))
		return;

	rc = synx_gmem_lock(idx, &flags);
	if (rc)
		return;
	synx_g_obj = &synx_gmem.table[idx];
	synx_g_obj->refcount--;
	if (synx_g_obj->refcount == 0) {
		memset(synx_g_obj, 0, sizeof(*synx_g_obj));
		clear = true;
	}
	synx_gmem_unlock(idx, &flags);

	if (clear) {
		ipclite_global_test_and_clear_bit(idx%32,
			(ipclite_atomic_uint32_t *)(synx_gmem.bitmap + idx/32));
		dprintk(SYNX_MEM, "cleared global idx %u\n", idx);
	}
}

int synx_global_merge(u32 *idx_list, u32 num_list, u32 p_idx)
{
	int rc = -SYNX_INVALID;
	unsigned long flags;
	struct synx_global_coredata *synx_g_obj;
	u32 i, j = 0;
	u32 idx;
	bool sig_error = false;
	u32 num_child = 0;

	if (!synx_gmem.table)
		return -SYNX_NOMEM;

	if (!synx_is_valid_idx(p_idx))
		return -SYNX_INVALID;

	while (j < num_list) {
		idx = idx_list[j];

		if (!synx_is_valid_idx(idx))
			goto fail;

		rc = synx_gmem_lock(idx, &flags);
		if (rc)
			goto fail;

		synx_g_obj = &synx_gmem.table[idx];
		if (synx_g_obj->status == SYNX_STATE_ACTIVE) {
			for (i = 0; i < SYNX_GLOBAL_MAX_PARENTS; i++) {
				if (synx_g_obj->parents[i] == 0) {
					synx_g_obj->parents[i] = p_idx;
					break;
				}
			}
			num_child++;
		} else if (synx_g_obj->status >
			SYNX_STATE_SIGNALED_SUCCESS) {
			sig_error = true;
		}
		synx_gmem_unlock(idx, &flags);

		if (i >= SYNX_GLOBAL_MAX_PARENTS) {
			rc = -SYNX_NOMEM;
			goto fail;
		}

		j++;
	}

	rc = synx_gmem_lock(p_idx, &flags);
	if (rc)
		goto fail;
	synx_g_obj = &synx_gmem.table[p_idx];
	synx_g_obj->num_child += num_child;
	if (sig_error)
		synx_g_obj->status = SYNX_STATE_SIGNALED_ERROR;
	else if (synx_g_obj->num_child != 0)
		synx_g_obj->refcount++;
	else if (synx_g_obj->num_child == 0 &&
		synx_g_obj->status == SYNX_STATE_ACTIVE)
		synx_g_obj->status = SYNX_STATE_SIGNALED_SUCCESS;
	synx_global_print_data(synx_g_obj, __func__);
	synx_gmem_unlock(p_idx, &flags);

	return SYNX_SUCCESS;

fail:
	while (num_child--) {
		idx = idx_list[num_child];

		if (synx_gmem_lock(idx, &flags))
			continue;
		synx_g_obj = &synx_gmem.table[idx];
		for (i = 0; i < SYNX_GLOBAL_MAX_PARENTS; i++) {
			if (synx_g_obj->parents[i] == p_idx) {
				synx_g_obj->parents[i] = 0;
				break;
			}
		}
		synx_gmem_unlock(idx, &flags);
	}

	return rc;
}

int synx_global_recover(enum synx_core_id core_id)
{
	int rc = SYNX_SUCCESS;
	u32 idx = 0;
	const u32 size = SYNX_GLOBAL_MAX_OBJS;
	unsigned long flags;
	struct synx_global_coredata *synx_g_obj;
	int *clear_idx = NULL;
	bool update;

	dprintk(SYNX_WARN, "Subsystem restart for core_id: %d\n", core_id);
	if (IS_ERR_OR_NULL(synx_gmem.table))
		return -SYNX_NOMEM;

	clear_idx = kzalloc(sizeof(int)*SYNX_GLOBAL_MAX_OBJS, GFP_KERNEL);

	if (IS_ERR_OR_NULL(clear_idx))
		return -SYNX_NOMEM;

	ipclite_recover(synx_global_map_core_id(core_id));

	/* recover synx gmem lock if it was owned by core in ssr */
	if (synx_gmem_lock_owner(0) == core_id) {
		synx_gmem_lock_owner_clear(0);
		hwspin_unlock_raw(synx_hwlock);
	}

	idx = find_next_bit((unsigned long *)synx_gmem.bitmap,
			size, idx + 1);
	while (idx < size) {
		update = false;
		rc = synx_gmem_lock(idx, &flags);
		if (rc)
			goto free;
		synx_g_obj = &synx_gmem.table[idx];
		if (synx_g_obj->refcount &&
			 synx_g_obj->subscribers & (1UL << core_id)) {
			synx_g_obj->subscribers &= ~(1UL << core_id);
			synx_g_obj->refcount--;
			if (synx_g_obj->refcount == 0) {
				memset(synx_g_obj, 0, sizeof(*synx_g_obj));
				clear_idx[idx] = 1;
			} else if (synx_g_obj->status == SYNX_STATE_ACTIVE) {
				update = true;
			}
		}
		synx_gmem_unlock(idx, &flags);
		if (update)
			synx_global_update_status(idx,
				SYNX_STATE_SIGNALED_SSR);
		idx = find_next_bit((unsigned long *)synx_gmem.bitmap,
				size, idx + 1);
	}

	for (idx = 1; idx < size; idx++) {
		if (clear_idx[idx]) {
			ipclite_global_test_and_clear_bit(idx % 32,
				(ipclite_atomic_uint32_t *)(synx_gmem.bitmap + idx/32));
			dprintk(SYNX_MEM, "released global idx %u\n", idx);
		}
	}

free:
	kfree(clear_idx);
	return rc;
}

int synx_global_mem_init(void)
{
	int rc;
	int bitmap_size = SYNX_GLOBAL_MAX_OBJS/32;
	struct global_region_info mem_info;

	rc = get_global_partition_info(&mem_info);
	if (rc) {
		dprintk(SYNX_ERR, "error setting up global shared memory\n");
		return rc;
	}

	memset(mem_info.virt_base, 0, mem_info.size);
	dprintk(SYNX_DBG, "global shared memory %pK size %u\n",
		mem_info.virt_base, mem_info.size);

	synx_gmem.bitmap = (u32 *)mem_info.virt_base;
	synx_gmem.locks = synx_gmem.bitmap + bitmap_size;
	synx_gmem.table =
		(struct synx_global_coredata *)(synx_gmem.locks + 2);
	dprintk(SYNX_DBG, "global memory bitmap %pK, table %pK\n",
		synx_gmem.bitmap, synx_gmem.table);

	return synx_gmem_init();
}
