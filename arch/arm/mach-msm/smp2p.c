/* arch/arm/mach-msm/smp2p.c
 *
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/list.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <mach/msm_smem.h>
#include <mach/msm_ipc_logging.h>
#include "smp2p_private_api.h"
#include "smp2p_private.h"

#define NUM_LOG_PAGES 3

/**
 * struct msm_smp2p_out - This structure represents the outbound SMP2P entry.
 *
 * @remote_pid: Outbound processor ID.
 * @name: Entry name.
 * @out_edge_list: Adds this structure into smp2p_out_list_item::list.
 * @msm_smp2p_notifier_list: Notifier block head used to notify for open event.
 * @open_nb: Notifier block used to notify for open event.
 * @l_smp2p_entry: Pointer to the actual entry in the SMEM item.
 */
struct msm_smp2p_out {
	int remote_pid;
	char name[SMP2P_MAX_ENTRY_NAME];
	struct list_head out_edge_list;
	struct raw_notifier_head msm_smp2p_notifier_list;
	struct notifier_block *open_nb;
	uint32_t __iomem *l_smp2p_entry;
};

/**
 * struct smp2p_out_list_item - Maintains the state of outbound edge.
 *
 * @out_item_lock_lha1: Lock protecting all elements of the structure.
 * @list: list of outbound entries (struct msm_smp2p_out).
 * @smem_edge_out: Pointer to outbound smem item.
 * @smem_edge_state: State of the outbound edge.
 * @ops_ptr: Pointer to internal version-specific SMEM item access functions.
 *
 * @feature_ssr_ack_enabled: SSR ACK Support Enabled
 * @restart_ack: Current cached state of the local ack bit
 */
struct smp2p_out_list_item {
	spinlock_t out_item_lock_lha1;

	struct list_head list;
	struct smp2p_smem __iomem *smem_edge_out;
	enum msm_smp2p_edge_state smem_edge_state;
	struct smp2p_version_if *ops_ptr;

	bool feature_ssr_ack_enabled;
	bool restart_ack;
};
static struct smp2p_out_list_item out_list[SMP2P_NUM_PROCS];

static void *log_ctx;
static int smp2p_debug_mask = MSM_SMP2P_INFO | MSM_SMP2P_DEBUG;
module_param_named(debug_mask, smp2p_debug_mask,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);

/**
 * struct smp2p_in - Represents the entry on remote processor.
 *
 * @name: Name of the entry.
 * @remote_pid: Outbound processor ID.
 * @in_edge_list: Adds this structure into smp2p_in_list_item::list.
 * @in_notifier_list: List for notifier block for entry opening/updates.
 * @prev_entry_val: Previous value of the entry.
 * @entry_ptr: Points to the current value in smem item.
 * @notifier_count: Counts the number of notifier registered per pid,entry.
 */
struct smp2p_in {
	int remote_pid;
	char name[SMP2P_MAX_ENTRY_NAME];
	struct list_head in_edge_list;
	struct raw_notifier_head in_notifier_list;
	uint32_t prev_entry_val;
	uint32_t __iomem *entry_ptr;
	uint32_t notifier_count;
};

/**
 * struct smp2p_in_list_item - Maintains the inbound edge state.
 *
 * @in_item_lock_lhb1: Lock protecting all elements of the structure.
 * @list: List head for the entries on remote processor.
 * @smem_edge_in: Pointer to the remote smem item.
 */
struct smp2p_in_list_item {
	spinlock_t in_item_lock_lhb1;
	struct list_head list;
	struct smp2p_smem __iomem *smem_edge_in;
	uint32_t item_size;
	uint32_t safe_total_entries;
};
static struct smp2p_in_list_item in_list[SMP2P_NUM_PROCS];

/**
 * SMEM Item access function interface.
 *
 * This interface is used to help isolate the implementation of
 * the functionality from any changes in the shared data structures
 * that may happen as versions are changed.
 *
 * @is_supported: True if this version is supported by SMP2P
 * @negotiate_features: Returns (sub)set of supported features
 * @negotiation_complete:  Called when negotiation has been completed
 * @find_entry: Finds existing / next empty entry
 * @create_entry: Creates a new entry
 * @read_entry: Reads the value of an entry
 * @write_entry: Writes a new value to an entry
 * @modify_entry: Does a read/modify/write of an entry
 * validate_size: Verifies the size of the remote SMEM item to ensure that
 *                an invalid item size doesn't result in an out-of-bounds
 *                memory access.
 */
struct smp2p_version_if {
	/* common functions */
	bool is_supported;
	uint32_t (*negotiate_features)(uint32_t features);
	void (*negotiation_complete)(struct smp2p_out_list_item *);
	void (*find_entry)(struct smp2p_smem __iomem *item,
			uint32_t entries_total,	char *name,
			uint32_t **entry_ptr, int *empty_spot);

	/* outbound entry functions */
	int (*create_entry)(struct msm_smp2p_out *);
	int (*read_entry)(struct msm_smp2p_out *, uint32_t *);
	int (*write_entry)(struct msm_smp2p_out *, uint32_t);
	int (*modify_entry)(struct msm_smp2p_out *, uint32_t, uint32_t);

	/* inbound entry functions */
	struct smp2p_smem __iomem *(*validate_size)(int remote_pid,
			struct smp2p_smem __iomem *, uint32_t);
};

static int smp2p_do_negotiation(int remote_pid, struct smp2p_out_list_item *p);
static void smp2p_send_interrupt(int remote_pid);

/* v0 (uninitialized SMEM item) interface functions */
static uint32_t smp2p_negotiate_features_v0(uint32_t features);
static void smp2p_negotiation_complete_v0(struct smp2p_out_list_item *out_item);
static void smp2p_find_entry_v0(struct smp2p_smem __iomem *item,
		uint32_t entries_total, char *name, uint32_t **entry_ptr,
		int *empty_spot);
static int smp2p_out_create_v0(struct msm_smp2p_out *);
static int smp2p_out_read_v0(struct msm_smp2p_out *, uint32_t *);
static int smp2p_out_write_v0(struct msm_smp2p_out *, uint32_t);
static int smp2p_out_modify_v0(struct msm_smp2p_out *, uint32_t, uint32_t);
static struct smp2p_smem __iomem *smp2p_in_validate_size_v0(int remote_pid,
		struct smp2p_smem __iomem *smem_item, uint32_t size);

/* v1 interface functions */
static uint32_t smp2p_negotiate_features_v1(uint32_t features);
static void smp2p_negotiation_complete_v1(struct smp2p_out_list_item *out_item);
static void smp2p_find_entry_v1(struct smp2p_smem __iomem *item,
		uint32_t entries_total, char *name, uint32_t **entry_ptr,
		int *empty_spot);
static int smp2p_out_create_v1(struct msm_smp2p_out *);
static int smp2p_out_read_v1(struct msm_smp2p_out *, uint32_t *);
static int smp2p_out_write_v1(struct msm_smp2p_out *, uint32_t);
static int smp2p_out_modify_v1(struct msm_smp2p_out *, uint32_t, uint32_t);
static struct smp2p_smem __iomem *smp2p_in_validate_size_v1(int remote_pid,
		struct smp2p_smem __iomem *smem_item, uint32_t size);

/* Version interface functions */
static struct smp2p_version_if version_if[] = {
	[0] = {
		.negotiate_features = smp2p_negotiate_features_v0,
		.negotiation_complete = smp2p_negotiation_complete_v0,
		.find_entry = smp2p_find_entry_v0,
		.create_entry = smp2p_out_create_v0,
		.read_entry = smp2p_out_read_v0,
		.write_entry = smp2p_out_write_v0,
		.modify_entry = smp2p_out_modify_v0,
		.validate_size = smp2p_in_validate_size_v0,
	},
	[1] = {
		.is_supported = true,
		.negotiate_features = smp2p_negotiate_features_v1,
		.negotiation_complete = smp2p_negotiation_complete_v1,
		.find_entry = smp2p_find_entry_v1,
		.create_entry = smp2p_out_create_v1,
		.read_entry = smp2p_out_read_v1,
		.write_entry = smp2p_out_write_v1,
		.modify_entry = smp2p_out_modify_v1,
		.validate_size = smp2p_in_validate_size_v1,
	},
};

/* interrupt configuration (filled by device tree) */
static struct smp2p_interrupt_config smp2p_int_cfgs[SMP2P_NUM_PROCS] = {
	[SMP2P_MODEM_PROC].name = "modem",
	[SMP2P_AUDIO_PROC].name = "lpass",
	[SMP2P_WIRELESS_PROC].name = "wcnss",
	[SMP2P_REMOTE_MOCK_PROC].name = "mock",
};

/**
 * smp2p_get_log_ctx - Return log context for other SMP2P modules.
 *
 * @returns: Log context or NULL if none.
 */
void *smp2p_get_log_ctx(void)
{
	return log_ctx;
}

/**
 * smp2p_get_debug_mask - Return debug mask.
 *
 * @returns: Current debug mask.
 */
int smp2p_get_debug_mask(void)
{
	return smp2p_debug_mask;
}

/**
 * smp2p_interrupt_config -  Return interrupt configuration.
 *
 * @returns interrupt configuration array for usage by debugfs.
 */
struct smp2p_interrupt_config *smp2p_get_interrupt_config(void)
{
	return smp2p_int_cfgs;
}

/**
 * smp2p_pid_to_name -  Lookup name for remote pid.
 *
 * @returns: name (may be NULL).
 */
const char *smp2p_pid_to_name(int remote_pid)
{
	if (remote_pid >= SMP2P_NUM_PROCS)
		return NULL;

	return smp2p_int_cfgs[remote_pid].name;
}

/**
 * smp2p_get_in_item - Return pointer to remote smem item.
 *
 * @remote_pid: Processor ID of the remote system.
 * @returns:    Pointer to inbound SMEM item
 *
 * This is used by debugfs to print the smem items.
 */
struct smp2p_smem __iomem *smp2p_get_in_item(int remote_pid)
{
	void *ret = NULL;
	unsigned long flags;

	spin_lock_irqsave(&in_list[remote_pid].in_item_lock_lhb1, flags);
	if (remote_pid < SMP2P_NUM_PROCS)
		ret = in_list[remote_pid].smem_edge_in;
	spin_unlock_irqrestore(&in_list[remote_pid].in_item_lock_lhb1,
								flags);

	return ret;
}

/**
 * smp2p_get_out_item - Return pointer to outbound SMEM item.
 *
 * @remote_pid: Processor ID of remote system.
 * @state:      Edge state of the outbound SMEM item.
 * @returns:    Pointer to outbound (remote) SMEM item.
 */
struct smp2p_smem __iomem *smp2p_get_out_item(int remote_pid, int *state)
{
	void *ret = NULL;
	unsigned long flags;

	spin_lock_irqsave(&out_list[remote_pid].out_item_lock_lha1, flags);
	if (remote_pid < SMP2P_NUM_PROCS) {
		ret = out_list[remote_pid].smem_edge_out;
		if (state)
			*state = out_list[remote_pid].smem_edge_state;
	}
	spin_unlock_irqrestore(&out_list[remote_pid].out_item_lock_lha1, flags);

	return ret;
}

/**
 * smp2p_get_smem_item_id - Return the proper SMEM item ID.
 *
 * @write_id:	Processor that will write to the item.
 * @read_id:    Processor that will read from the item.
 * @returns:    SMEM ID
 */
static int smp2p_get_smem_item_id(int write_pid, int read_pid)
{
	int ret = -EINVAL;

	switch (write_pid) {
	case SMP2P_APPS_PROC:
		ret = SMEM_SMP2P_APPS_BASE + read_pid;
		break;
	case SMP2P_MODEM_PROC:
		ret = SMEM_SMP2P_MODEM_BASE + read_pid;
		break;
	case SMP2P_AUDIO_PROC:
		ret = SMEM_SMP2P_AUDIO_BASE + read_pid;
		break;
	case SMP2P_WIRELESS_PROC:
		ret = SMEM_SMP2P_WIRLESS_BASE + read_pid;
		break;
	case SMP2P_POWER_PROC:
		ret = SMEM_SMP2P_POWER_BASE + read_pid;
		break;
	}

	return ret;
}

/**
 * Return pointer to SMEM item owned by the local processor.
 *
 * @remote_pid: Remote processor ID
 * @returns:    NULL for failure; otherwise pointer to SMEM item
 *
 * Must be called with out_item_lock_lha1 locked for mock proc.
 */
static void *smp2p_get_local_smem_item(int remote_pid)
{
	struct smp2p_smem __iomem *item_ptr = NULL;

	if (remote_pid < SMP2P_REMOTE_MOCK_PROC) {
		unsigned size;
		int smem_id;

		/* lookup or allocate SMEM item */
		smem_id = smp2p_get_smem_item_id(SMP2P_APPS_PROC, remote_pid);
		if (smem_id >= 0) {
			item_ptr = smem_get_entry(smem_id, &size);

			if (!item_ptr) {
				size = sizeof(struct smp2p_smem_item);
				item_ptr = smem_alloc2(smem_id, size);
			}
		}
	} else if (remote_pid == SMP2P_REMOTE_MOCK_PROC) {
		/*
		 * This path is only used during unit testing so
		 * the GFP_ATOMIC allocation should not be a
		 * concern.
		 */
		if (!out_list[SMP2P_REMOTE_MOCK_PROC].smem_edge_out)
			item_ptr = kzalloc(
					sizeof(struct smp2p_smem_item),
					GFP_ATOMIC);
	}
	return item_ptr;
}

/**
 * smp2p_get_remote_smem_item - Return remote SMEM item.
 *
 * @remote_pid: Remote processor ID
 * @out_item:   Pointer to the output item structure
 * @returns:    NULL for failure; otherwise pointer to SMEM item
 *
 * Return pointer to SMEM item owned by the remote processor.
 *
 * Note that this function does an SMEM lookup which uses a remote spinlock,
 * so this function should not be called more than necessary.
 *
 * Must be called with out_item_lock_lha1 and in_item_lock_lhb1 locked.
 */
static void *smp2p_get_remote_smem_item(int remote_pid,
	struct smp2p_out_list_item *out_item)
{
	void *item_ptr = NULL;
	unsigned size = 0;

	if (!out_item)
		return item_ptr;

	if (remote_pid < SMP2P_REMOTE_MOCK_PROC) {
		int smem_id;

		smem_id = smp2p_get_smem_item_id(remote_pid, SMP2P_APPS_PROC);
		if (smem_id >= 0)
			item_ptr = smem_get_entry(smem_id, &size);
	} else if (remote_pid == SMP2P_REMOTE_MOCK_PROC) {
		item_ptr = msm_smp2p_get_remote_mock_smem_item(&size);
	}
	item_ptr = out_item->ops_ptr->validate_size(remote_pid, item_ptr, size);

	return item_ptr;
}

/**
 * smp2p_ssr_ack_needed - Returns true if SSR ACK required
 *
 * @rpid: Remote processor ID
 *
 * Must be called with out_item_lock_lha1 and in_item_lock_lhb1 locked.
 */
static bool smp2p_ssr_ack_needed(uint32_t rpid)
{
	bool ssr_done;

	if (!out_list[rpid].feature_ssr_ack_enabled)
		return false;

	ssr_done = SMP2P_GET_RESTART_DONE(in_list[rpid].smem_edge_in->flags);
	if (ssr_done != out_list[rpid].restart_ack)
		return true;

	return false;
}

/**
 * smp2p_do_ssr_ack - Handles SSR ACK
 *
 * @rpid: Remote processor ID
 *
 * Must be called with out_item_lock_lha1 and in_item_lock_lhb1 locked.
 */
static void smp2p_do_ssr_ack(uint32_t rpid)
{
	bool ack;

	if (!smp2p_ssr_ack_needed(rpid))
		return;

	ack = !out_list[rpid].restart_ack;
	SMP2P_INFO("%s: ssr ack pid %d: %d -> %d\n", __func__, rpid,
			out_list[rpid].restart_ack, ack);
	out_list[rpid].restart_ack = ack;
	SMP2P_SET_RESTART_ACK(out_list[rpid].smem_edge_out->flags, ack);
	smp2p_send_interrupt(rpid);
}

/**
 * smp2p_negotiate_features_v1 - Initial feature negotiation.
 *
 * @features: Inbound feature set.
 * @returns: Supported features (will be a same/subset of @features).
 */
static uint32_t smp2p_negotiate_features_v1(uint32_t features)
{
	return SMP2P_FEATURE_SSR_ACK;
}

/**
 * smp2p_negotiation_complete_v1 - Negotiation completed
 *
 * @out_item:   Pointer to the output item structure
 *
 * Can be used to do final configuration based upon the negotiated feature set.
 *
 * Must be called with out_item_lock_lha1 locked.
 */
static void smp2p_negotiation_complete_v1(struct smp2p_out_list_item *out_item)
{
	uint32_t features;

	features = SMP2P_GET_FEATURES(out_item->smem_edge_out->feature_version);

	if (features & SMP2P_FEATURE_SSR_ACK)
		out_item->feature_ssr_ack_enabled = true;
}

/**
 * smp2p_find_entry_v1 - Search for an entry in SMEM item.
 *
 * @item: Pointer to the smem item.
 * @entries_total: Total number of entries in @item.
 * @name: Name of the entry.
 * @entry_ptr: Set to pointer of entry if found, NULL otherwise.
 * @empty_spot: If non-null, set to the value of the next empty entry.
 *
 * Searches for entry @name in the SMEM item.  If found, a pointer
 * to the item is returned.  If it isn't found, the first empty
 * index is returned in @empty_spot.
 */
static void smp2p_find_entry_v1(struct smp2p_smem __iomem *item,
		uint32_t entries_total, char *name, uint32_t **entry_ptr,
		int *empty_spot)
{
	int i;
	struct smp2p_entry_v1 *pos;

	if (!item || !name || !entry_ptr) {
		SMP2P_ERR("%s: invalid arguments %p, %p, %p\n",
				__func__, item, name, entry_ptr);
		return;
	}

	*entry_ptr = NULL;
	if (empty_spot)
		*empty_spot = -1;

	pos = (struct smp2p_entry_v1 *)(char *)(item + 1);
	for (i = 0; i < entries_total; i++, ++pos) {
		if (pos->name[0]) {
			if (!strncmp(pos->name, name, SMP2P_MAX_ENTRY_NAME)) {
				*entry_ptr = &pos->entry;
				break;
			}
		} else if (empty_spot && *empty_spot < 0) {
			*empty_spot = i;
		}
	}
}

/**
 * smp2p_out_create_v1 - Creates a outbound SMP2P entry.
 *
 * @out_entry: Pointer to the SMP2P entry structure.
 * @returns: 0 on success, standard Linux error code otherwise.
 *
 * Must be called with out_item_lock_lha1 locked.
 */
static int smp2p_out_create_v1(struct msm_smp2p_out *out_entry)
{
	struct smp2p_smem __iomem *smp2p_h_ptr;
	struct smp2p_out_list_item *p_list;
	uint32_t *state_entry_ptr;
	uint32_t empty_spot;
	uint32_t entries_total;
	uint32_t entries_valid;

	if (!out_entry)
		return -EINVAL;

	p_list = &out_list[out_entry->remote_pid];
	if (p_list->smem_edge_state != SMP2P_EDGE_STATE_OPENED) {
		SMP2P_ERR("%s: item '%s':%d opened - wrong create called\n",
			__func__, out_entry->name, out_entry->remote_pid);
		return -ENODEV;
	}

	smp2p_h_ptr = p_list->smem_edge_out;
	entries_total = SMP2P_GET_ENT_TOTAL(smp2p_h_ptr->valid_total_ent);
	entries_valid = SMP2P_GET_ENT_VALID(smp2p_h_ptr->valid_total_ent);

	p_list->ops_ptr->find_entry(smp2p_h_ptr, entries_total,
			out_entry->name, &state_entry_ptr, &empty_spot);
	if (state_entry_ptr) {
		/* re-use existing entry */
		out_entry->l_smp2p_entry = state_entry_ptr;

		SMP2P_DBG("%s: item '%s':%d reused\n", __func__,
				out_entry->name, out_entry->remote_pid);
	} else if (entries_valid >= entries_total) {
		/* need to allocate entry, but not more space */
		SMP2P_ERR("%s: no space for item '%s':%d\n",
			__func__, out_entry->name, out_entry->remote_pid);
		return -ENOMEM;
	} else {
		/* allocate a new entry */
		struct smp2p_entry_v1 *entry_ptr;

		entry_ptr = (struct smp2p_entry_v1 *)((char *)(smp2p_h_ptr + 1)
			+ empty_spot * sizeof(struct smp2p_entry_v1));
		strlcpy(entry_ptr->name, out_entry->name,
				sizeof(entry_ptr->name));
		out_entry->l_smp2p_entry = &entry_ptr->entry;
		++entries_valid;
		SMP2P_DBG("%s: item '%s':%d fully created as entry %d of %d\n",
				__func__, out_entry->name,
				out_entry->remote_pid,
				entries_valid, entries_total);
		SMP2P_SET_ENT_VALID(smp2p_h_ptr->valid_total_ent,
				entries_valid);
		smp2p_send_interrupt(out_entry->remote_pid);
	}
	raw_notifier_call_chain(&out_entry->msm_smp2p_notifier_list,
		  SMP2P_OPEN, 0);

	return 0;
}

/**
 * smp2p_out_read_v1 -  Read the data from an outbound entry.
 *
 * @out_entry: Pointer to the SMP2P entry structure.
 * @data: Out pointer, the data is available in this argument on success.
 * @returns: 0 on success, standard Linux error code otherwise.
 *
 * Must be called with out_item_lock_lha1 locked.
 */
static int smp2p_out_read_v1(struct msm_smp2p_out *out_entry, uint32_t *data)
{
	struct smp2p_smem __iomem  *smp2p_h_ptr;
	uint32_t remote_pid;

	if (!out_entry)
		return -EINVAL;

	smp2p_h_ptr = out_list[out_entry->remote_pid].smem_edge_out;
	remote_pid = SMP2P_GET_REMOTE_PID(smp2p_h_ptr->rem_loc_proc_id);

	if (remote_pid != out_entry->remote_pid)
		return -EINVAL;

	if (out_entry->l_smp2p_entry) {
		*data = readl_relaxed(out_entry->l_smp2p_entry);
	} else {
		SMP2P_ERR("%s: '%s':%d not yet OPEN\n", __func__,
				out_entry->name, remote_pid);
		return -ENODEV;
	}

	return 0;
}

/**
 * smp2p_out_write_v1 - Writes an outbound entry value.
 *
 * @out_entry: Pointer to the SMP2P entry structure.
 * @data: The data to be written.
 * @returns: 0 on success, standard Linux error code otherwise.
 *
 * Must be called with out_item_lock_lha1 locked.
 */
static int smp2p_out_write_v1(struct msm_smp2p_out *out_entry, uint32_t data)
{
	struct smp2p_smem __iomem  *smp2p_h_ptr;
	uint32_t remote_pid;

	if (!out_entry)
		return -EINVAL;

	smp2p_h_ptr = out_list[out_entry->remote_pid].smem_edge_out;
	remote_pid = SMP2P_GET_REMOTE_PID(smp2p_h_ptr->rem_loc_proc_id);

	if (remote_pid != out_entry->remote_pid)
		return -EINVAL;

	if (out_entry->l_smp2p_entry) {
		writel_relaxed(data, out_entry->l_smp2p_entry);
		smp2p_send_interrupt(remote_pid);
	} else {
		SMP2P_ERR("%s: '%s':%d not yet OPEN\n", __func__,
				out_entry->name, remote_pid);
		return -ENODEV;
	}
	return 0;
}

/**
 * smp2p_out_modify_v1 - Modifies and outbound value.
 *
 * @set_mask:  Mask containing the bits that needs to be set.
 * @clear_mask: Mask containing the bits that needs to be cleared.
 * @returns: 0 on success, standard Linux error code otherwise.
 *
 * The clear mask is applied first, so  if a bit is set in both clear and
 * set mask, the result will be that the bit is set.
 *
 * Must be called with out_item_lock_lha1 locked.
 */
static int smp2p_out_modify_v1(struct msm_smp2p_out *out_entry,
		uint32_t set_mask, uint32_t clear_mask)
{
	struct smp2p_smem __iomem  *smp2p_h_ptr;
	uint32_t remote_pid;

	if (!out_entry)
		return -EINVAL;

	smp2p_h_ptr = out_list[out_entry->remote_pid].smem_edge_out;
	remote_pid = SMP2P_GET_REMOTE_PID(smp2p_h_ptr->rem_loc_proc_id);

	if (remote_pid != out_entry->remote_pid)
			return -EINVAL;

	if (out_entry->l_smp2p_entry) {
		uint32_t curr_value;

		curr_value = readl_relaxed(out_entry->l_smp2p_entry);
		writel_relaxed((curr_value & ~clear_mask) | set_mask,
			out_entry->l_smp2p_entry);
	} else {
		SMP2P_ERR("%s: '%s':%d not yet OPEN\n", __func__,
				out_entry->name, remote_pid);
		return -ENODEV;
	}

	smp2p_send_interrupt(remote_pid);
	return 0;
}

/**
 * smp2p_in_validate_size_v1 - Size validation for version 1.
 *
 * @remote_pid: Remote processor ID.
 * @smem_item:  Pointer to the inbound SMEM item.
 * @size:       Size of the SMEM item.
 * @returns:    Validated smem_item pointer (or NULL if size is too small).
 *
 * Validates we don't end up with out-of-bounds array access due to invalid
 * smem item size.  If out-of-bound array access can't be avoided, then an
 * error message is printed and NULL is returned to prevent usage of the
 * item.
 *
 * Must be called with in_item_lock_lhb1 locked.
 */
static struct smp2p_smem __iomem *smp2p_in_validate_size_v1(int remote_pid,
		struct smp2p_smem __iomem *smem_item, uint32_t size)
{
	uint32_t total_entries;
	unsigned expected_size;
	struct smp2p_smem __iomem *item_ptr;
	struct smp2p_in_list_item *in_item;

	if (remote_pid >= SMP2P_NUM_PROCS || !smem_item)
		return NULL;

	in_item = &in_list[remote_pid];
	item_ptr = (struct smp2p_smem __iomem *)smem_item;

	total_entries = SMP2P_GET_ENT_TOTAL(item_ptr->valid_total_ent);
	if (total_entries > 0) {
		in_item->safe_total_entries = total_entries;
		in_item->item_size = size;

		expected_size =	sizeof(struct smp2p_smem) +
			(total_entries * sizeof(struct smp2p_entry_v1));

		if (size < expected_size) {
			unsigned new_size;

			new_size = size;
			new_size -= sizeof(struct smp2p_smem);
			new_size /= sizeof(struct smp2p_entry_v1);
			in_item->safe_total_entries = new_size;

			SMP2P_ERR(
				"%s pid %d item too small for %d entries; expected: %d actual: %d; reduced to %d entries\n",
				__func__, remote_pid, total_entries,
				expected_size, size, new_size);
		}
	} else {
		/*
		 * Total entries is 0, so the entry is still being initialized
		 * or is invalid.  Either way, treat it as if the item does
		 * not exist yet.
		 */
		in_item->safe_total_entries = 0;
		in_item->item_size = 0;
	}
	return item_ptr;
}

/**
 * smp2p_negotiate_features_v0 - Initial feature negotiation.
 *
 * @features: Inbound feature set.
 * @returns: 0 (no features supported for v0).
 */
static uint32_t smp2p_negotiate_features_v0(uint32_t features)
{
	/* no supported features */
	return 0;
}

/**
 * smp2p_negotiation_complete_v0 - Negotiation completed
 *
 * @out_item:   Pointer to the output item structure
 *
 * Can be used to do final configuration based upon the negotiated feature set.
 */
static void smp2p_negotiation_complete_v0(struct smp2p_out_list_item *out_item)
{
	SMP2P_ERR("%s: invalid negotiation complete for v0 pid %d\n",
		__func__,
		SMP2P_GET_REMOTE_PID(out_item->smem_edge_out->rem_loc_proc_id));
}

/**
 * smp2p_find_entry_v0 - Stub function.
 *
 * @item: Pointer to the smem item.
 * @entries_total: Total number of entries in @item.
 * @name: Name of the entry.
 * @entry_ptr: Set to pointer of entry if found, NULL otherwise.
 * @empty_spot: If non-null, set to the value of the next empty entry.
 *
 * Entries cannot be searched for until item negotiation has been completed.
 */
static void smp2p_find_entry_v0(struct smp2p_smem __iomem *item,
		uint32_t entries_total, char *name, uint32_t **entry_ptr,
		int *empty_spot)
{
	if (entry_ptr)
		*entry_ptr = NULL;

	if (empty_spot)
		*empty_spot = -1;

	SMP2P_ERR("%s: invalid - item negotiation incomplete\n", __func__);
}

/**
 * smp2p_out_create_v0 - Initial creation function.
 *
 * @out_entry: Pointer to the SMP2P entry structure.
 * @returns: 0 on success, standard Linux error code otherwise.
 *
 * If the outbound SMEM item negotiation is not complete, then
 * this function is called to start the negotiation process.
 * Eventually when the negotiation process is complete, this
 * function pointer is switched with the appropriate function
 * for the version of SMP2P being created.
 *
 * Must be called with out_item_lock_lha1 locked.
 */
static int smp2p_out_create_v0(struct msm_smp2p_out *out_entry)
{
	int edge_state;
	struct smp2p_out_list_item *item_ptr;

	if (!out_entry)
		return -EINVAL;

	edge_state = out_list[out_entry->remote_pid].smem_edge_state;

	switch (edge_state) {
	case SMP2P_EDGE_STATE_CLOSED:
		/* start negotiation */
		item_ptr = &out_list[out_entry->remote_pid];
		edge_state = smp2p_do_negotiation(out_entry->remote_pid,
				item_ptr);
		break;

	case SMP2P_EDGE_STATE_OPENING:
		/* still negotiating */
		break;

	case SMP2P_EDGE_STATE_OPENED:
		SMP2P_ERR("%s: item '%s':%d opened - wrong create called\n",
			__func__, out_entry->name, out_entry->remote_pid);
		break;

	default:
		SMP2P_ERR("%s: item '%s':%d invalid SMEM item state %d\n",
			__func__, out_entry->name, out_entry->remote_pid,
			edge_state);
		break;
	}
	return 0;
}

/**
 * smp2p_out_read_v0 - Stub function.
 *
 * @out_entry: Pointer to the SMP2P entry structure.
 * @data: Out pointer, the data is available in this argument on success.
 * @returns: -ENODEV
 */
static int smp2p_out_read_v0(struct msm_smp2p_out *out_entry, uint32_t *data)
{
	SMP2P_ERR("%s: item '%s':%d not OPEN\n",
		__func__, out_entry->name, out_entry->remote_pid);

	return -ENODEV;
}

/**
 * smp2p_out_write_v0 - Stub function.
 *
 * @out_entry: Pointer to the SMP2P entry structure.
 * @data: The data to be written.
 * @returns: -ENODEV
 */
static int smp2p_out_write_v0(struct msm_smp2p_out *out_entry, uint32_t data)
{
	SMP2P_ERR("%s: item '%s':%d not yet OPEN\n",
		__func__, out_entry->name, out_entry->remote_pid);

	return -ENODEV;
}

/**
 * smp2p_out_modify_v0 - Stub function.
 *
 * @set_mask:  Mask containing the bits that needs to be set.
 * @clear_mask: Mask containing the bits that needs to be cleared.
 * @returns: -ENODEV
 */
static int smp2p_out_modify_v0(struct msm_smp2p_out *out_entry,
		uint32_t set_mask, uint32_t clear_mask)
{
	SMP2P_ERR("%s: item '%s':%d not yet OPEN\n",
		__func__, out_entry->name, out_entry->remote_pid);

	return -ENODEV;
}

/**
 * smp2p_in_validate_size_v0 - Stub function.
 *
 * @remote_pid: Remote processor ID.
 * @smem_item:  Pointer to the inbound SMEM item.
 * @size:       Size of the SMEM item.
 * @returns:    Validated smem_item pointer (or NULL if size is too small).
 *
 * Validates we don't end up with out-of-bounds array access due to invalid
 * smem item size.  If out-of-bound array access can't be avoided, then an
 * error message is printed and NULL is returned to prevent usage of the
 * item.
 *
 * Must be called with in_item_lock_lhb1 locked.
 */
static struct smp2p_smem __iomem *smp2p_in_validate_size_v0(int remote_pid,
		struct smp2p_smem __iomem *smem_item, uint32_t size)
{
	struct smp2p_in_list_item *in_item;

	if (remote_pid >= SMP2P_NUM_PROCS || !smem_item)
		return NULL;

	in_item = &in_list[remote_pid];

	if (size < sizeof(struct smp2p_smem)) {
		SMP2P_ERR(
			"%s pid %d item size too small; expected: %zu actual: %d\n",
			__func__, remote_pid,
			sizeof(struct smp2p_smem), size);
		smem_item = NULL;
		in_item->item_size = 0;
	} else {
		in_item->item_size = size;
	}
	return smem_item;
}

/**
 * smp2p_init_header - Initializes the header of the smem item.
 *
 * @header_ptr: Pointer to the smp2p header.
 * @local_pid: Local processor ID.
 * @remote_pid: Remote processor ID.
 * @feature: Features of smp2p implementation.
 * @version: Version of smp2p implementation.
 *
 * Initializes the header as defined in the protocol specification.
 */
void smp2p_init_header(struct smp2p_smem __iomem *header_ptr,
		int local_pid, int remote_pid,
		uint32_t features, uint32_t version)
{
	header_ptr->magic = SMP2P_MAGIC;
	SMP2P_SET_LOCAL_PID(header_ptr->rem_loc_proc_id, local_pid);
	SMP2P_SET_REMOTE_PID(header_ptr->rem_loc_proc_id, remote_pid);
	SMP2P_SET_FEATURES(header_ptr->feature_version, features);
	SMP2P_SET_ENT_TOTAL(header_ptr->valid_total_ent, SMP2P_MAX_ENTRY);
	SMP2P_SET_ENT_VALID(header_ptr->valid_total_ent, 0);
	header_ptr->flags = 0;

	/* ensure that all fields are valid before version is written */
	wmb();
	SMP2P_SET_VERSION(header_ptr->feature_version, version);
}

/**
 * smp2p_do_negotiation - Implements negotiation algorithm.
 *
 * @remote_pid: Remote processor ID.
 * @out_item: Pointer to the outbound list item.
 * @returns: 0 on success, standard Linux error code otherwise.
 *
 * Must be called with out_item_lock_lha1 locked.  Will internally lock
 * in_item_lock_lhb1.
 */
static int smp2p_do_negotiation(int remote_pid,
		struct smp2p_out_list_item *out_item)
{
	struct smp2p_smem __iomem *r_smem_ptr;
	struct smp2p_smem __iomem *l_smem_ptr;
	uint32_t r_version;
	uint32_t r_feature;
	uint32_t l_version, l_feature;
	int prev_state;

	if (remote_pid >= SMP2P_NUM_PROCS || !out_item)
		return -EINVAL;
	if (out_item->smem_edge_state == SMP2P_EDGE_STATE_FAILED)
		return -EPERM;

	prev_state = out_item->smem_edge_state;

	/* create local item */
	if (!out_item->smem_edge_out) {
		out_item->smem_edge_out = smp2p_get_local_smem_item(remote_pid);
		if (!out_item->smem_edge_out) {
			SMP2P_ERR(
				"%s unable to allocate SMEM item for pid %d\n",
				__func__, remote_pid);
			return -ENODEV;
		}
		out_item->smem_edge_state = SMP2P_EDGE_STATE_OPENING;
	}
	l_smem_ptr = out_item->smem_edge_out;

	/* retrieve remote side and version */
	spin_lock(&in_list[remote_pid].in_item_lock_lhb1);
	r_smem_ptr = smp2p_get_remote_smem_item(remote_pid, out_item);
	spin_unlock(&in_list[remote_pid].in_item_lock_lhb1);

	r_version = 0;
	if (r_smem_ptr) {
		r_version = SMP2P_GET_VERSION(r_smem_ptr->feature_version);
		r_feature = SMP2P_GET_FEATURES(r_smem_ptr->feature_version);
	}

	if (r_version == 0) {
		/*
		 * Either remote side doesn't exist, or is in the
		 * process of being initialized (the version is set last).
		 *
		 * In either case, treat as if the other side doesn't exist
		 * and write out our maximum supported version.
		 */
		r_smem_ptr = NULL;
		r_version = ARRAY_SIZE(version_if) - 1;
		r_feature = ~0U;
	}

	/* find maximum supported version and feature set */
	l_version = min(r_version, ARRAY_SIZE(version_if) - 1);
	for (; l_version > 0; --l_version) {
		if (!version_if[l_version].is_supported)
			continue;

		/* found valid version */
		l_feature = version_if[l_version].negotiate_features(~0U);
		if (l_version == r_version)
			l_feature &= r_feature;
		break;
	}

	if (l_version == 0) {
		SMP2P_ERR(
			"%s: negotiation failure pid %d: RV %d RF %x\n",
			__func__, remote_pid, r_version, r_feature
			);
		SMP2P_SET_VERSION(l_smem_ptr->feature_version,
			SMP2P_EDGE_STATE_FAILED);
		smp2p_send_interrupt(remote_pid);
		out_item->smem_edge_state = SMP2P_EDGE_STATE_FAILED;
		return -EPERM;
	}

	/* update header and notify remote side */
	smp2p_init_header(l_smem_ptr, SMP2P_APPS_PROC, remote_pid,
		l_feature, l_version);
	smp2p_send_interrupt(remote_pid);

	/* handle internal state changes */
	if (r_smem_ptr && l_version == r_version &&
			l_feature == r_feature) {
		struct msm_smp2p_out *pos;

		/* negotiation complete */
		out_item->ops_ptr = &version_if[l_version];
		out_item->ops_ptr->negotiation_complete(out_item);
		out_item->smem_edge_state = SMP2P_EDGE_STATE_OPENED;
		SMP2P_INFO(
			"%s: negotiation complete pid %d: State %d->%d F0x%08x\n",
			__func__, remote_pid, prev_state,
			out_item->smem_edge_state, l_feature);

		/* create any pending outbound entries */
		list_for_each_entry(pos, &out_item->list, out_edge_list) {
			out_item->ops_ptr->create_entry(pos);
		}

		/* update inbound edge */
		spin_lock(&in_list[remote_pid].in_item_lock_lhb1);
		(void)out_item->ops_ptr->validate_size(remote_pid, r_smem_ptr,
				in_list[remote_pid].item_size);
		in_list[remote_pid].smem_edge_in = r_smem_ptr;
		spin_unlock(&in_list[remote_pid].in_item_lock_lhb1);
	} else {
		SMP2P_INFO("%s: negotiation pid %d: State %d->%d F0x%08x\n",
			__func__, remote_pid, prev_state,
			out_item->smem_edge_state, l_feature);
	}
	return 0;
}

/**
 * msm_smp2p_out_open - Opens an outbound entry.
 *
 * @remote_pid: Outbound processor ID.
 * @name: Name of the entry.
 * @open_notifier: Notifier block for the open notification.
 * @handle: Handle to the smem entry structure.
 * @returns: 0 on success, standard Linux error code otherwise.
 *
 * Opens an outbound entry with the name specified by entry, from the
 * local processor to the remote processor(remote_pid). If the entry, remote_pid
 * and open_notifier are valid, then handle will be set and zero will be
 * returned. The smem item that holds this entry will be created if it has
 * not been created according to the version negotiation algorithm.
 * The open_notifier will be used to notify the clients about the
 * availability of the entry.
 */
int msm_smp2p_out_open(int remote_pid, const char *name,
				   struct notifier_block *open_notifier,
				   struct msm_smp2p_out **handle)
{
	struct msm_smp2p_out *out_entry;
	struct msm_smp2p_out *pos;
	int ret = 0;
	unsigned long flags;

	if (handle)
		*handle = NULL;

	if (remote_pid >= SMP2P_NUM_PROCS || !name || !open_notifier || !handle)
		return -EINVAL;

	/* Allocate the smp2p object and node */
	out_entry = kzalloc(sizeof(*out_entry), GFP_KERNEL);
	if (!out_entry)
		return -ENOMEM;

	/* Handle duplicate registration */
	spin_lock_irqsave(&out_list[remote_pid].out_item_lock_lha1, flags);
	list_for_each_entry(pos, &out_list[remote_pid].list,
			out_edge_list) {
		if (!strcmp(pos->name, name)) {
			spin_unlock_irqrestore(
				&out_list[remote_pid].out_item_lock_lha1,
				flags);
			kfree(out_entry);
			SMP2P_ERR("%s: duplicate registration '%s':%d\n",
				__func__, name, remote_pid);
			return -EBUSY;
		}
	}

	out_entry->remote_pid = remote_pid;
	RAW_INIT_NOTIFIER_HEAD(&out_entry->msm_smp2p_notifier_list);
	strlcpy(out_entry->name, name, SMP2P_MAX_ENTRY_NAME);
	out_entry->open_nb = open_notifier;
	raw_notifier_chain_register(&out_entry->msm_smp2p_notifier_list,
		  out_entry->open_nb);
	list_add(&out_entry->out_edge_list, &out_list[remote_pid].list);

	ret = out_list[remote_pid].ops_ptr->create_entry(out_entry);
	if (ret) {
		list_del(&out_entry->out_edge_list);
		raw_notifier_chain_unregister(
			&out_entry->msm_smp2p_notifier_list,
			out_entry->open_nb);
		spin_unlock_irqrestore(
			&out_list[remote_pid].out_item_lock_lha1, flags);
		kfree(out_entry);
		SMP2P_ERR("%s: unable to open '%s':%d error %d\n",
				__func__, name, remote_pid, ret);
		return ret;
	}
	spin_unlock_irqrestore(&out_list[remote_pid].out_item_lock_lha1,
			flags);
	*handle = out_entry;

	return 0;
}
EXPORT_SYMBOL(msm_smp2p_out_open);

/**
 * msm_smp2p_out_close - Closes the handle to an outbound entry.
 *
 * @handle: Pointer to smp2p out entry handle.
 * @returns: 0 on success, standard Linux error code otherwise.
 *
 * The actual entry will not be deleted and can be re-opened at a later
 * time.  The handle will be set to NULL.
 */
int msm_smp2p_out_close(struct msm_smp2p_out **handle)
{
	unsigned long flags;
	struct msm_smp2p_out *out_entry;
	struct smp2p_out_list_item *out_item;

	if (!handle || !*handle)
		return -EINVAL;

	out_entry = *handle;
	*handle = NULL;

	out_item = &out_list[out_entry->remote_pid];
	spin_lock_irqsave(&out_item->out_item_lock_lha1, flags);
	list_del(&out_entry->out_edge_list);
	raw_notifier_chain_unregister(&out_entry->msm_smp2p_notifier_list,
		out_entry->open_nb);
	spin_unlock_irqrestore(&out_item->out_item_lock_lha1, flags);

	kfree(out_entry);

	return 0;
}
EXPORT_SYMBOL(msm_smp2p_out_close);

/**
 * msm_smp2p_out_read - Allows reading the entry.
 *
 * @handle: Handle to the smem entry structure.
 * @data: Out pointer that holds the read data.
 * @returns: 0 on success, standard Linux error code otherwise.
 *
 * Allows reading of the outbound entry for read-modify-write
 * operation.
 */
int msm_smp2p_out_read(struct msm_smp2p_out *handle, uint32_t *data)
{
	int ret = -EINVAL;
	unsigned long flags;
	struct smp2p_out_list_item *out_item;

	if (!handle || !data)
		return ret;

	out_item = &out_list[handle->remote_pid];
	spin_lock_irqsave(&out_item->out_item_lock_lha1, flags);
	ret = out_item->ops_ptr->read_entry(handle, data);
	spin_unlock_irqrestore(&out_item->out_item_lock_lha1, flags);

	return ret;
}
EXPORT_SYMBOL(msm_smp2p_out_read);

/**
 * msm_smp2p_out_write - Allows writing to the entry.
 *
 * @handle: Handle to smem entry structure.
 * @data: Data that has to be written.
 * @returns: 0 on success, standard Linux error code otherwise.
 *
 * Writes a new value to the output entry. Multiple back-to-back writes
 * may overwrite previous writes before the remote processor get a chance
 * to see them leading to ABA race condition. The client must implement
 * their own synchronization mechanism (such as echo mechanism) if this is
 * not acceptable.
 */
int msm_smp2p_out_write(struct msm_smp2p_out *handle, uint32_t data)
{
	int ret = -EINVAL;
	unsigned long flags;
	struct smp2p_out_list_item *out_item;

	if (!handle)
		return ret;

	out_item = &out_list[handle->remote_pid];
	spin_lock_irqsave(&out_item->out_item_lock_lha1, flags);
	ret = out_item->ops_ptr->write_entry(handle, data);
	spin_unlock_irqrestore(&out_item->out_item_lock_lha1, flags);

	return ret;

}
EXPORT_SYMBOL(msm_smp2p_out_write);

/**
 * msm_smp2p_out_modify - Modifies the entry.
 *
 * @handle: Handle to the smem entry structure.
 * @set_mask: Specifies the bits that needs to be set.
 * @clear_mask: Specifies the bits that needs to be cleared.
 * @returns: 0 on success, standard Linux error code otherwise.
 *
 * The modification is done by doing a bitwise AND of clear mask followed by
 * the bit wise OR of set mask. The clear bit mask is applied first to the
 * data, so if a bit is set in both the clear mask and the set mask, then in
 * the result is a set bit.  Multiple back-to-back modifications may overwrite
 * previous values before the remote processor gets a chance to see them
 * leading to ABA race condition. The client must implement their own
 * synchronization mechanism (such as echo mechanism) if this is not
 * acceptable.
 */
int msm_smp2p_out_modify(struct msm_smp2p_out *handle, uint32_t set_mask,
							uint32_t clear_mask)
{
	int ret = -EINVAL;
	unsigned long flags;
	struct smp2p_out_list_item *out_item;

	if (!handle)
		return ret;

	out_item = &out_list[handle->remote_pid];
	spin_lock_irqsave(&out_item->out_item_lock_lha1, flags);
	ret = out_item->ops_ptr->modify_entry(handle, set_mask, clear_mask);
	spin_unlock_irqrestore(&out_item->out_item_lock_lha1, flags);

	return ret;
}
EXPORT_SYMBOL(msm_smp2p_out_modify);

/**
 * msm_smp2p_in_read - Read an entry on a remote processor.
 *
 * @remote_pid: Processor ID of the remote processor.
 * @name: Name of the entry that is to be read.
 * @data: Output pointer, the value will be placed here if successful.
 * @returns: 0 on success, standard Linux error code otherwise.
 */
int msm_smp2p_in_read(int remote_pid, const char *name, uint32_t *data)
{
	unsigned long flags;
	struct smp2p_out_list_item *out_item;
	uint32_t *entry_ptr = NULL;

	if (remote_pid >= SMP2P_NUM_PROCS)
		return -EINVAL;

	out_item = &out_list[remote_pid];
	spin_lock_irqsave(&out_item->out_item_lock_lha1, flags);
	spin_lock(&in_list[remote_pid].in_item_lock_lhb1);

	if (in_list[remote_pid].smem_edge_in)
		out_item->ops_ptr->find_entry(
			in_list[remote_pid].smem_edge_in,
			in_list[remote_pid].safe_total_entries,
			(char *)name, &entry_ptr, NULL);

	spin_unlock(&in_list[remote_pid].in_item_lock_lhb1);
	spin_unlock_irqrestore(&out_item->out_item_lock_lha1, flags);

	if (!entry_ptr)
		return -ENODEV;

	*data = readl_relaxed(entry_ptr);
	return 0;
}
EXPORT_SYMBOL(msm_smp2p_in_read);

/**
 * msm_smp2p_in_register -  Notifies the change in value of the entry.
 *
 * @pid: Remote processor ID.
 * @name: Name of the entry.
 * @in_notifier: Notifier block used to notify about the event.
 * @returns: 0 on success, standard Linux error code otherwise.
 *
 * Register for change notifications for a remote entry. If the remote entry
 * does not exist yet, then the registration request will be held until the
 * remote side opens. Once the entry is open, then the SMP2P_OPEN notification
 * will be sent. Any changes to the entry will trigger a call to the notifier
 * block with an SMP2P_ENTRY_UPDATE event and the data field will point to an
 * msm_smp2p_update_notif structure containing the current and previous value.
 */
int msm_smp2p_in_register(int pid, const char *name,
	struct notifier_block *in_notifier)
{
	struct smp2p_in *pos;
	struct smp2p_in *in = NULL;
	int ret;
	unsigned long flags;
	struct msm_smp2p_update_notif data;
	uint32_t *entry_ptr;

	if (pid >= SMP2P_NUM_PROCS || !name || !in_notifier)
		return -EINVAL;

	/* Pre-allocate before spinlock since we will likely needed it */
	in = kzalloc(sizeof(*in), GFP_KERNEL);
	if (!in)
		return -ENOMEM;

	/* Search for existing entry */
	spin_lock_irqsave(&out_list[pid].out_item_lock_lha1, flags);
	spin_lock(&in_list[pid].in_item_lock_lhb1);

	list_for_each_entry(pos, &in_list[pid].list, in_edge_list) {
		if (!strncmp(pos->name, name,
					SMP2P_MAX_ENTRY_NAME)) {
			kfree(in);
			in = pos;
			break;
		}
	}

	/* Create and add it to the list */
	if (!in->notifier_count) {
		in->remote_pid = pid;
		strlcpy(in->name, name, SMP2P_MAX_ENTRY_NAME);
		RAW_INIT_NOTIFIER_HEAD(&in->in_notifier_list);
		list_add(&in->in_edge_list, &in_list[pid].list);
	}

	ret = raw_notifier_chain_register(&in->in_notifier_list,
			in_notifier);
	if (ret) {
		if (!in->notifier_count) {
			list_del(&in->in_edge_list);
			kfree(in);
		}
		SMP2P_DBG("%s: '%s':%d failed %d\n", __func__, name, pid, ret);
		goto bail;
	}
	in->notifier_count++;

	if (out_list[pid].smem_edge_state == SMP2P_EDGE_STATE_OPENED) {
		out_list[pid].ops_ptr->find_entry(
				in_list[pid].smem_edge_in,
				in_list[pid].safe_total_entries, (char *)name,
				&entry_ptr, NULL);
		if (entry_ptr) {
			in->entry_ptr = entry_ptr;
			in->prev_entry_val = readl_relaxed(entry_ptr);

			data.previous_value = in->prev_entry_val;
			data.current_value = in->prev_entry_val;
			in_notifier->notifier_call(in_notifier, SMP2P_OPEN,
					(void *)&data);
		}
	}
	SMP2P_DBG("%s: '%s':%d registered\n", __func__, name, pid);

bail:
	spin_unlock(&in_list[pid].in_item_lock_lhb1);
	spin_unlock_irqrestore(&out_list[pid].out_item_lock_lha1, flags);
	return ret;

}
EXPORT_SYMBOL(msm_smp2p_in_register);

/**
 * msm_smp2p_in_unregister - Unregister the notifier for remote entry.
 *
 * @remote_pid: Processor Id of the remote processor.
 * @name: The name of the entry.
 * @in_notifier: Notifier block passed during registration.
 * @returns: 0 on success, standard Linux error code otherwise.
 */
int msm_smp2p_in_unregister(int remote_pid, const char *name,
				struct notifier_block *in_notifier)
{
	struct smp2p_in *pos;
	struct smp2p_in *in = NULL;
	int ret = -ENODEV;
	unsigned long flags;

	if (remote_pid >= SMP2P_NUM_PROCS || !name || !in_notifier)
		return -EINVAL;

	spin_lock_irqsave(&in_list[remote_pid].in_item_lock_lhb1, flags);
	list_for_each_entry(pos, &in_list[remote_pid].list,
			in_edge_list) {
		if (!strncmp(pos->name, name, SMP2P_MAX_ENTRY_NAME)) {
			in = pos;
			break;
		}
	}
	if (!in)
		goto fail;

	ret = raw_notifier_chain_unregister(&pos->in_notifier_list,
			in_notifier);
	if (ret == 0) {
		pos->notifier_count--;
		if (!pos->notifier_count) {
			list_del(&pos->in_edge_list);
			kfree(pos);
			ret = 0;
		}
	} else {
		SMP2P_ERR("%s: unregister failure '%s':%d\n", __func__,
			name, remote_pid);
		ret = -ENODEV;
	}

fail:
	spin_unlock_irqrestore(&in_list[remote_pid].in_item_lock_lhb1, flags);

	return ret;
}
EXPORT_SYMBOL(msm_smp2p_in_unregister);

/**
 * smp2p_send_interrupt - Send interrupt to remote system.
 *
 * @remote_pid:  Processor ID of the remote system
 *
 * Must be called with out_item_lock_lha1 locked.
 */
static void smp2p_send_interrupt(int remote_pid)
{
	if (smp2p_int_cfgs[remote_pid].name)
		SMP2P_DBG("SMP2P Int Apps->%s(%d)\n",
			smp2p_int_cfgs[remote_pid].name, remote_pid);

	++smp2p_int_cfgs[remote_pid].out_interrupt_count;
	if (remote_pid != SMP2P_REMOTE_MOCK_PROC &&
			smp2p_int_cfgs[remote_pid].out_int_mask) {
		/* flush any pending writes before triggering interrupt */
		wmb();
		writel_relaxed(smp2p_int_cfgs[remote_pid].out_int_mask,
			smp2p_int_cfgs[remote_pid].out_int_ptr);
	} else {
		smp2p_remote_mock_rx_interrupt();
	}
}

/**
 * smp2p_in_edge_notify - Notifies the entry changed on remote processor.
 *
 * @pid: Processor ID of the remote processor.
 *
 * This function is invoked on an incoming interrupt, it scans
 * the list of the clients registered for the entries on the remote
 * processor and notifies them if  the data changes.
 *
 * Note:  Edge state must be OPENED to avoid a race condition with
 *        out_list[pid].ops_ptr->find_entry.
 */
static void smp2p_in_edge_notify(int pid)
{
	struct smp2p_in *pos;
	uint32_t *entry_ptr;
	unsigned long flags;
	struct smp2p_smem __iomem *smem_h_ptr;
	uint32_t curr_data;
	struct  msm_smp2p_update_notif data;

	spin_lock_irqsave(&in_list[pid].in_item_lock_lhb1, flags);
	smem_h_ptr = in_list[pid].smem_edge_in;
	if (!smem_h_ptr) {
		SMP2P_DBG("%s: No remote SMEM item for pid %d\n",
			__func__, pid);
		spin_unlock_irqrestore(&in_list[pid].in_item_lock_lhb1, flags);
		return;
	}

	list_for_each_entry(pos, &in_list[pid].list, in_edge_list) {
		if (pos->entry_ptr == NULL) {
			/* entry not open - try to open it */
			out_list[pid].ops_ptr->find_entry(smem_h_ptr,
				in_list[pid].safe_total_entries, pos->name,
				&entry_ptr, NULL);

			if (entry_ptr) {
				pos->entry_ptr = entry_ptr;
				pos->prev_entry_val = 0;
				data.previous_value = 0;
				data.current_value = readl_relaxed(entry_ptr);
				raw_notifier_call_chain(
					    &pos->in_notifier_list,
					    SMP2P_OPEN, (void *)&data);
			}
		}

		if (pos->entry_ptr != NULL) {
			/* send update notification */
			curr_data = readl_relaxed(pos->entry_ptr);
			if (curr_data != pos->prev_entry_val) {
				data.previous_value = pos->prev_entry_val;
				data.current_value = curr_data;
				pos->prev_entry_val = curr_data;
				raw_notifier_call_chain(
					&pos->in_notifier_list,
					SMP2P_ENTRY_UPDATE, (void *)&data);
			}
		}
	}
	spin_unlock_irqrestore(&in_list[pid].in_item_lock_lhb1, flags);
}

/**
 * smp2p_interrupt_handler - Incoming interrupt handler.
 *
 * @irq: Interrupt ID
 * @data: Edge
 * @returns: IRQ_HANDLED or IRQ_NONE for invalid interrupt
 */
static irqreturn_t smp2p_interrupt_handler(int irq, void *data)
{
	unsigned long flags;
	uint32_t remote_pid = (uint32_t)data;

	if (remote_pid >= SMP2P_NUM_PROCS) {
		SMP2P_ERR("%s: invalid interrupt pid %d\n",
			__func__, remote_pid);
		return IRQ_NONE;
	}

	if (smp2p_int_cfgs[remote_pid].name)
		SMP2P_DBG("SMP2P Int %s(%d)->Apps\n",
			smp2p_int_cfgs[remote_pid].name, remote_pid);

	spin_lock_irqsave(&out_list[remote_pid].out_item_lock_lha1, flags);
	++smp2p_int_cfgs[remote_pid].in_interrupt_count;

	if (out_list[remote_pid].smem_edge_state != SMP2P_EDGE_STATE_OPENED)
		smp2p_do_negotiation(remote_pid, &out_list[remote_pid]);

	if (out_list[remote_pid].smem_edge_state == SMP2P_EDGE_STATE_OPENED) {
		bool do_restart_ack;

		/*
		 * Follow double-check pattern for restart ack since:
		 * 1) we must notify clients of the X->0 transition
		 *    that is part of the restart
		 * 2) lock cannot be held during the
		 *    smp2p_in_edge_notify() call because clients may do
		 *    re-entrant calls into our APIs.
		 *
		 * smp2p_do_ssr_ack() will only do the ack if it is
		 * necessary to handle the race condition exposed by
		 * unlocking the spinlocks.
		 */
		spin_lock(&in_list[remote_pid].in_item_lock_lhb1);
		do_restart_ack = smp2p_ssr_ack_needed(remote_pid);
		spin_unlock(&in_list[remote_pid].in_item_lock_lhb1);
		spin_unlock_irqrestore(&out_list[remote_pid].out_item_lock_lha1,
			flags);

		smp2p_in_edge_notify(remote_pid);

		if (do_restart_ack) {
			spin_lock_irqsave(
				&out_list[remote_pid].out_item_lock_lha1,
				flags);
			spin_lock(&in_list[remote_pid].in_item_lock_lhb1);

			smp2p_do_ssr_ack(remote_pid);

			spin_unlock(&in_list[remote_pid].in_item_lock_lhb1);
			spin_unlock_irqrestore(
				&out_list[remote_pid].out_item_lock_lha1,
				flags);
		}
	} else {
		spin_unlock_irqrestore(&out_list[remote_pid].out_item_lock_lha1,
			flags);
	}

	return IRQ_HANDLED;
}

/**
 * smp2p_reset_mock_edge - Reinitializes the mock edge.
 *
 * @returns: 0 on success, -EAGAIN to retry later.
 *
 * Reinitializes the mock edge to initial power-up state values.
 */
int smp2p_reset_mock_edge(void)
{
	const int rpid = SMP2P_REMOTE_MOCK_PROC;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&out_list[rpid].out_item_lock_lha1, flags);
	spin_lock(&in_list[rpid].in_item_lock_lhb1);

	if (!list_empty(&out_list[rpid].list) ||
			!list_empty(&in_list[rpid].list)) {
		ret = -EAGAIN;
		goto fail;
	}

	kfree(out_list[rpid].smem_edge_out);
	out_list[rpid].smem_edge_out = NULL;
	out_list[rpid].ops_ptr = &version_if[0];
	out_list[rpid].smem_edge_state = SMP2P_EDGE_STATE_CLOSED;
	out_list[rpid].feature_ssr_ack_enabled = false;
	out_list[rpid].restart_ack = false;

	in_list[rpid].smem_edge_in = NULL;
	in_list[rpid].item_size = 0;
	in_list[rpid].safe_total_entries = 0;

fail:
	spin_unlock(&in_list[rpid].in_item_lock_lhb1);
	spin_unlock_irqrestore(&out_list[rpid].out_item_lock_lha1, flags);

	return ret;
}

/**
 * msm_smp2p_interrupt_handler - Triggers incoming interrupt.
 *
 * @remote_pid: Remote processor ID
 *
 * This function is used with the remote mock infrastructure
 * used for testing. It simulates triggering of interrupt in
 * a testing environment.
 */
void msm_smp2p_interrupt_handler(int remote_pid)
{
	smp2p_interrupt_handler(0, (void *)remote_pid);
}

/**
 * msm_smp2p_probe - Device tree probe function.
 *
 * @pdev: Pointer to device tree data.
 * @returns: 0 on success; -ENODEV otherwise
 */
static int __devinit msm_smp2p_probe(struct platform_device *pdev)
{
	struct resource *r;
	void *irq_out_ptr;
	char *key;
	uint32_t edge;
	int ret;
	struct device_node *node;
	uint32_t irq_bitmask;
	uint32_t irq_line;

	node = pdev->dev.of_node;

	key = "qcom,remote-pid";
	ret = of_property_read_u32(node, key, &edge);
	if (ret) {
		SMP2P_ERR("%s: missing edge '%s'\n", __func__, key);
		goto fail;
	}

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r) {
		SMP2P_ERR("%s: failed gathering irq-reg resource for edge %d\n"
				, __func__, edge);
		goto fail;
	}
	irq_out_ptr = ioremap_nocache(r->start, resource_size(r));
	if (!irq_out_ptr) {
		SMP2P_ERR("%s: failed remap from phys to virt for edge %d\n",
				__func__, edge);
		return -ENOMEM;
	}

	key = "qcom,irq-bitmask";
	ret = of_property_read_u32(node, key, &irq_bitmask);
	if (ret)
		goto missing_key;

	key = "interrupts";
	irq_line = platform_get_irq(pdev, 0);
	if (irq_line == -ENXIO)
		goto missing_key;

	ret = request_irq(irq_line, smp2p_interrupt_handler,
			IRQF_TRIGGER_RISING, "smp2p", (void *)edge);
	if (ret < 0) {
		SMP2P_ERR("%s: request_irq() failed on %d (edge %d)\n",
				__func__, irq_line, edge);
		goto fail;
	}

	ret = enable_irq_wake(irq_line);
	if (ret < 0)
		SMP2P_ERR("%s: enable_irq_wake() failed on %d (edge %d)\n",
				__func__, irq_line, edge);

	/*
	 * Set entry (keep is_configured last to prevent usage before
	 * initialization).
	 */
	smp2p_int_cfgs[edge].in_int_id = irq_line;
	smp2p_int_cfgs[edge].out_int_mask = irq_bitmask;
	smp2p_int_cfgs[edge].out_int_ptr = irq_out_ptr;
	smp2p_int_cfgs[edge].is_configured = true;
	return 0;

missing_key:
	SMP2P_ERR("%s: missing '%s' for edge %d\n", __func__, key, edge);
fail:
	return -ENODEV;
}

static struct of_device_id msm_smp2p_match_table[] = {
	{ .compatible = "qcom,smp2p" },
	{},
};

static struct platform_driver msm_smp2p_driver = {
	.probe = msm_smp2p_probe,
	.driver = {
		.name = "msm_smp2p",
		.owner = THIS_MODULE,
		.of_match_table = msm_smp2p_match_table,
	},
};

/**
 * msm_smp2p_init -  Initialization function for the module.
 *
 * @returns: 0 on success, standard Linux error code otherwise.
 */
static int __init msm_smp2p_init(void)
{
	int i;
	int rc;

	for (i = 0; i < SMP2P_NUM_PROCS; i++) {
		spin_lock_init(&out_list[i].out_item_lock_lha1);
		INIT_LIST_HEAD(&out_list[i].list);
		out_list[i].smem_edge_out = NULL;
		out_list[i].smem_edge_state = SMP2P_EDGE_STATE_CLOSED;
		out_list[i].ops_ptr = &version_if[0];
		out_list[i].feature_ssr_ack_enabled = false;
		out_list[i].restart_ack = false;

		spin_lock_init(&in_list[i].in_item_lock_lhb1);
		INIT_LIST_HEAD(&in_list[i].list);
		in_list[i].smem_edge_in = NULL;
	}

	log_ctx = ipc_log_context_create(NUM_LOG_PAGES, "smp2p", 0);
	if (!log_ctx)
		SMP2P_ERR("%s: unable to create log context\n", __func__);

	rc = platform_driver_register(&msm_smp2p_driver);
	if (rc) {
		SMP2P_ERR("%s: msm_smp2p_driver register failed %d\n",
			__func__, rc);
		return rc;
	}

	return 0;
}
module_init(msm_smp2p_init);

MODULE_DESCRIPTION("MSM Shared Memory Point to Point");
MODULE_LICENSE("GPL v2");
