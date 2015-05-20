/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2013 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2014 Intel Mobile Communications GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * BSD LICENSE
 *
 * Copyright(c) 2012 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2014 Intel Mobile Communications GmbH
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/

#ifndef __iwl_trans_slv_shared_h__
#define __iwl_trans_slv_shared_h__

#include <linux/types.h>
#include <linux/skbuff.h>

#include "iwl-trans.h"
#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif

#define IWL_SLV_TXQ_GET_ENTRY(txq_entry, outer_ptr)\
	((outer_ptr) = container_of((txq_entry),\
				    typeof(*(outer_ptr)), txq_entry))

/* Receiver address (actually, Rx station's index into station table),
 * combined with Traffic ID (QOS priority), in format used by Tx Scheduler */
#define BUILD_RAxTID(sta_id, tid)	(((sta_id) << 4) + (tid))

#define HOST_COMPLETE_TIMEOUT (2 * HZ)

/* Pool Manager for AL Store/Forward Memory */
#define IWL_SLV_AL_INVALID 0xffff

/**
 * struct iwl_slv_al_pool_mgr - represents a pool of items
 * @pool_size:	the size of the pool. Must be <= IWL_IDI_POOL_MGR_MAX_SIZE
 * @used:	bitmap of free/used elements. Set bit means the elem is used.
 * @free_count:	number of free items in the pool. Useful for debugging.
 * @order:	the order of the element's size. Element size is 2^order
 *
 * It manages a bitmap of elements and is able to provide the real addresses
 * of the items it manages. The pool has to be contiguous in memory, with equal
 * sized items, in order to be manageable.
 * Note, the pool is not thread-safe - must be called from a locked context.
*/
struct iwl_slv_al_mem_pool {
	u16 pool_size;
	u16 free_count;
	unsigned long *used;
};

/**
 * struct iwl_slv_tx_chunk_info - temporary data during DTU processing
 * @addr:	virtual address of memory chunk start
 * @len:	memory chunk length
 * @desc_num:	number of descriptors required (in order to compute only once)
 */
struct iwl_slv_tx_chunk_info {
	u8 *addr;
	u32 len;
	u8 desc_num;
};

struct slv_config {
	int max_queues_num;
	int queue_size;
	int tb_size;
	int max_data_desc_count;
	int hcmd_headroom;
	void (*policy_trigger)(struct work_struct *);
	void (*clean_dtu)(struct iwl_trans *, void *);
	void (*free_dtu_mem)(struct iwl_trans *, void **);
	void (*calc_desc_num)(struct iwl_trans *,
			      struct iwl_slv_tx_chunk_info *);
	int (*grab_bus)(struct iwl_trans *trans);
	int (*release_bus)(struct iwl_trans *trans);
	void (*rx_dma_idle)(struct iwl_trans *trans);
};

struct iwl_trans_slv_tx {
	spinlock_t mem_rsrc_lock;
	struct iwl_slv_al_mem_pool tfd_pool;
	struct iwl_slv_al_mem_pool tb_pool;
};

struct slv_mini_rpm_config {
	int (*runtime_suspend)(struct iwl_trans *);
	int (*runtime_resume)(struct iwl_trans *);
	int autosuspend_delay; /* given in ms */
};

enum iwl_slv_rpm_flags {
	IWL_SLV_RPM_FLAG_REF_TAKEN,
};

/**
 * struct iwl_trans_slv - slave common transport
 * @txqs: array of data queues and one cmd queue
 * @cmd_entry_pool:	the pool of iwl_slv_tx_cmd_entry for dynamic alloc
 * @data_entry_pool:	the pool of iwl_slv_tx_data_entry for dynamic alloc
 * @policy_wq:		wq for the implementation of quota reservation mechanism
 * @policy_trigger:	corresponding work_struct
 * @config:		bus specific configurations
 * @queue_stopped_map:	indicates stopped queues
 * @txq_lock:		sync access to tx queues.
 * @cmd_queue:		index of command queue
 * @cmd_fifo:		index of command fifo
 * @command_names:
 * @wait_command_queue:
 * @rx_page_order:	page order for receive buffer size
 * @n_no_reclaim_cmds:	number of no reclaim commands
 * @no_reclaim_cmds:	no reclaim command ids
 * @bc_table_dword: true if the BC table expects DWORD (as opposed to bytes)
 * @bus_specific:	pointer to bus specific struct
 * @refcount		rpm references count
 * @rpm_lock		spinlock protecting the rpm data
 * @rpm_wq		workqueue the rpm work run on
 * @rpm_resume_work	rpm resume work
 * @rpm_suspend_work	rpm suspend work
 * @suspended		indicate whether the bus is suspended
 * @rpm_config		rpm configuration
 * @wowlan_enabled	wowlan is enabled
 */
struct iwl_trans_slv {
	struct iwl_slv_tx_queue *txqs;
	struct kmem_cache *cmd_entry_pool;
	struct kmem_cache *data_entry_pool;
	struct workqueue_struct *policy_wq;
	struct work_struct policy_trigger;
	struct slv_config config;
	unsigned long queue_stopped_map[BITS_TO_LONGS(IWL_MAX_HW_QUEUES)];
	spinlock_t txq_lock;
	u8 cmd_queue;
	u8 cmd_fifo;
	const char *const *command_names;
	wait_queue_head_t wait_command_queue;

	u32 rx_page_order;
	u8 n_no_reclaim_cmds;
	u8 no_reclaim_cmds[MAX_NO_RECLAIM_CMDS];

	bool bc_table_dword;
	bool suspending;

	struct device *d0i3_dev;
	struct device *host_dev;
	wait_queue_head_t d0i3_waitq;

	bool wowlan_enabled;

#ifdef CPTCFG_IWLWIFI_MINI_PM_RUNTIME
	/* protects rpm data (refcount, suspended) */
	spinlock_t rpm_lock;
	int refcount;
	bool suspended;
	struct workqueue_struct *rpm_wq;
	struct work_struct rpm_resume_work;
	struct delayed_work rpm_suspend_work;
	struct slv_mini_rpm_config rpm_config;
#endif
	unsigned long rpm_flags;
#ifdef CONFIG_HAS_WAKELOCK
	struct wake_lock slv_wake_lock;
	struct wake_lock data_wake_lock;
#endif

	/* Ensure that this pointer will always be aligned to sizeof pointer */
	char bus_specific[0] __aligned(sizeof(void *));
};

static inline
void iwl_slv_set_reclaim_cmds(struct iwl_trans_slv *trans_slv,
			      const struct iwl_trans_config *trans_cfg)
{
	if (WARN_ON(trans_cfg->n_no_reclaim_cmds > MAX_NO_RECLAIM_CMDS))
		trans_slv->n_no_reclaim_cmds = 0;
	else
		trans_slv->n_no_reclaim_cmds = trans_cfg->n_no_reclaim_cmds;
	if (trans_slv->n_no_reclaim_cmds)
		memcpy(trans_slv->no_reclaim_cmds, trans_cfg->no_reclaim_cmds,
		       trans_slv->n_no_reclaim_cmds * sizeof(u8));
}

static inline
void iwl_slv_set_rx_page_order(struct iwl_trans_slv *trans_slv,
			       const struct iwl_trans_config *trans_cfg)
{
	if (trans_cfg->rx_buf_size_8k)
		trans_slv->rx_page_order = get_order(8 * 1024);
	else
		trans_slv->rx_page_order = get_order(4 * 1024);
}

#define IWL_TRANS_GET_SLV_TRANS(_iwl_trans)\
	((struct iwl_trans_slv *)((_iwl_trans)->trans_specific))

#define IWL_TRANS_SLV_GET_IWL_TRANS(_slv_trans)\
	container_of((void *)_slv_trans, struct iwl_trans, trans_specific)

/**
 * struct iwl_slv_tx_hcmd_meta - meta data for host comamnds
 * @source:	the original host command
 * @dup_buf:	the duplicated part of HCMD (with DUP flag)
 * @flags:
 * @copy_size:	the size of the header and the copied chunk
 * @hcmd_size:	the total size of the command and the header (in bytes)
 */
struct iwl_slv_tx_hcmd_meta {
	/* only for SYNC commands, iff the reply skb is wanted */
	struct iwl_host_cmd *source;
	void *dup_buf;
	u32 flags;
	u16 copy_size;
	u16 hcmd_size;
};

/**
 * struct iwl_slv_tx_dtu_meta - temporary data to unify tx processing
 * @chunk_info:		array of info about continuous memory chunks
 * @total_len:		the total length of the data to be sent
 * @scd_byte_cnt:	the byte count for SCD
 * @chunks_num:		number of memory chunks
 * @total_desc_num:	total number of descriptors that will be needed
 */
struct iwl_slv_tx_dtu_meta {
	struct iwl_slv_tx_chunk_info chunk_info[IWL_MAX_CMD_TBS_PER_TFD + 1];
	u16 total_len;
	__le16 scd_byte_cnt;
	u8 chunks_num;
	u8 total_desc_num;
};

/**
* struct iwl_slv_txq_entry - queue entry data common for all entries
* @dtu_meta:	the temporary data needed to build DTU
* @list:	to store entry in a list
* @reclaim_info:Holds everything needed to complete reclaim;
*
* This struct is bus specific and used after policy trigger call.
*/
struct iwl_slv_txq_entry {
	struct iwl_slv_tx_dtu_meta dtu_meta;
	struct list_head list;
	void *reclaim_info;
};

/**
 * struct iwl_slv_tx_cmd_entry - host command tx queue entry
 * @txq_entry:		common data
 * @hcmd_meta:		meta data for a host command
 * @cmd_specific:	holds the device_command and other bus specific data.
 *	This implemntation is for the sake of IDI, where we need to have
 *	headroom just before the device command. See iwl_cmd_entry_get_dev_cmd.
 */
struct iwl_slv_tx_cmd_entry {
	struct iwl_slv_txq_entry txq_entry;
	struct iwl_slv_tx_hcmd_meta hcmd_meta;
	u8 cmd_specific[0];
};

static inline struct iwl_device_cmd *
iwl_cmd_entry_get_dev_cmd(struct iwl_trans_slv *trans_slv,
			  struct iwl_slv_tx_cmd_entry *cmd)
{
	return (struct iwl_device_cmd *)
		(cmd->cmd_specific + trans_slv->config.hcmd_headroom);
}

/**
 * struct iwl_slv_tx_data_entry - tx data entry for tx queue
 * @txq_entry: common data
 * @cmd: the original tx command
 * @skb: the skb with the data
 */
struct iwl_slv_tx_data_entry {
	struct iwl_slv_txq_entry txq_entry;
	struct iwl_device_cmd *cmd;
	struct sk_buff *skb;
};

/**
 * struct iwl_slv_tx_cmd_queue
 * @waiting: the queue of cmds added to the transport, but not sent to AL
 * @sent: the queue of cmds sent to AL, waiting for reclaiming
 * @waiting_count: number of entries in the waiting list
 * @waiting_last_idx: the index of the last entry in waiting queue
 * @sent_count: number of entries in the sent list
 */
struct iwl_slv_tx_queue {
	struct list_head waiting;
	struct list_head sent;
	atomic_t waiting_count;
	u32 waiting_last_idx;
	atomic_t sent_count;
};

int iwl_slv_al_mem_pool_init(struct iwl_slv_al_mem_pool *pm, u16 num_elems);
void iwl_slv_al_mem_pool_deinit(struct iwl_slv_al_mem_pool *pm);
int iwl_slv_al_mem_pool_alloc(struct iwl_trans_slv_tx *slv_tx,
			      struct iwl_slv_al_mem_pool *pm);
int iwl_slv_al_mem_pool_alloc_range(struct iwl_trans_slv_tx *slv_tx,
			      struct iwl_slv_al_mem_pool *pm, u8 order);
int iwl_slv_al_mem_pool_free(struct iwl_trans_slv_tx *slv_tx,
			     struct iwl_slv_al_mem_pool *pm, u16 idx);
int iwl_slv_al_mem_pool_free_region(struct iwl_trans_slv_tx *slv_tx,
			     struct iwl_slv_al_mem_pool *pm, u16 idx, u8 order);
static inline
int iwl_slv_al_mem_pool_free_count(struct iwl_trans_slv_tx *slv_tx,
				   struct iwl_slv_al_mem_pool *pm)
{
	lockdep_assert_held(&slv_tx->mem_rsrc_lock);
	return pm->free_count;
}

struct iwl_slv_txq_entry *iwl_slv_txq_pop_entry(
					struct iwl_trans_slv *trans_slv,
					u8 txq_id);
void iwl_slv_txq_pushback_entry(struct iwl_trans_slv *trans_slv, u8 txq_id,
		      struct iwl_slv_txq_entry *txq_entry);

void iwl_slv_txq_add_to_sent(struct iwl_trans_slv *trans_slv, u8 txq_id,
			      struct iwl_slv_txq_entry *txq_entry);

/* debug utilities */
static inline const char *
trans_slv_get_cmd_string(struct iwl_trans_slv *trans_slv, u8 cmd)
{
	if (!trans_slv->command_names || !trans_slv->command_names[cmd])
		return "UNKNOWN";
	return trans_slv->command_names[cmd];
}

void iwl_trans_slv_tx_set_ssn(struct iwl_trans *trans, int txq_id, int ssn);
int iwl_trans_slv_send_cmd(struct iwl_trans *trans, struct iwl_host_cmd *cmd);
int iwl_trans_slv_tx_data_send(struct iwl_trans *trans, struct sk_buff *skb,
			       struct iwl_device_cmd *dev_cmd, int txq_id);
void iwl_trans_slv_tx_data_reclaim(struct iwl_trans *trans, int txq_id,
				   int ssn, struct sk_buff_head *skbs);
void iwl_slv_tx_cmd_complete(struct iwl_trans *trans,
			     struct iwl_rx_cmd_buffer *rxcb,
			     struct iwl_slv_tx_cmd_entry *cmd_entry,
			     int handler_status);
int iwl_trans_slv_wait_txq_empty(struct iwl_trans *trans, u32 txq_bm);
int iwl_slv_rx_handle_dispatch(struct iwl_trans *trans,
			       struct iwl_rx_cmd_buffer *rxcb);
int iwl_slv_tx_get_cmd_entry(struct iwl_trans *trans, struct iwl_rx_packet *pkt,
			     struct iwl_slv_tx_cmd_entry **cmd_entry);

void iwl_slv_tx_stop(struct iwl_trans *trans);
void iwl_slv_free(struct iwl_trans *trans);
int iwl_slv_init(struct iwl_trans *trans);
void iwl_slv_free_data_queue(struct iwl_trans *trans, int txq_id);
int iwl_trans_slv_dbgfs_register(struct iwl_trans *trans,
				 struct dentry *dir);
void iwl_trans_slv_ref(struct iwl_trans *trans);
void iwl_trans_slv_unref(struct iwl_trans *trans);
void iwl_trans_slv_suspend(struct iwl_trans *trans);
void iwl_trans_slv_resume(struct iwl_trans *trans);
int iwl_slv_get_next_queue(struct iwl_trans_slv *trans_slv);
/* mini runtime pm */
#ifdef CPTCFG_IWLWIFI_MINI_PM_RUNTIME
int mini_rpm_init(struct iwl_trans_slv *trans_slv,
		  struct slv_mini_rpm_config *rpm_config);
void mini_rpm_destroy(struct iwl_trans_slv *trans_slv);
void mini_rpm_get(struct iwl_trans_slv *trans_slv);
void mini_rpm_put(struct iwl_trans_slv *trans_slv);
#endif /* CPTCFG_IWLWIFI_MINI_PM_RUNTIME */
#endif
