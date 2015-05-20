/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2014 Intel Mobile Communications GmbH
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
 * Copyright(c) 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2014 Intel Mobile Communications GmbH
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
#include <linux/types.h>
#include <linux/export.h>
#include <linux/vmalloc.h>

#include "iwl-debug.h"
#include "iwl-dnt-cfg.h"
#include "iwl-dnt-dispatch.h"
#include "iwl-dnt-dev-if.h"
#include "iwl-tm-infc.h"
#include "iwl-tm-gnl.h"
#include "iwl-io.h"
#include "iwl-fw-error-dump.h"


struct dnt_collect_db *iwl_dnt_dispatch_allocate_collect_db(struct iwl_dnt *dnt)
{
	struct dnt_collect_db *db;

	db = kzalloc(sizeof(struct dnt_collect_db), GFP_KERNEL);
	if (!db) {
		dnt->iwl_dnt_status |= IWL_DNT_STATUS_FAILED_TO_ALLOCATE_DB;
		return NULL;
	}

	spin_lock_init(&db->db_lock);

	return db;
}

static void iwl_dnt_dispatch_free_collect_db(struct dnt_collect_db *db)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(db->collect_array); i++)
		kfree(db->collect_array[i].data);

	kfree(db);
}

static int iwl_dnt_dispatch_get_list_data(struct dnt_collect_db *db,
					   u8 *buffer, u32 buffer_size)
{
	struct dnt_collect_entry *cur_entry;
	int i, cur_index = 0, data_offset = 0;

	spin_lock_bh(&db->db_lock);
	for (i = 0; i < ARRAY_SIZE(db->collect_array); i++) {
		cur_index = (i + db->read_ptr) % IWL_DNT_ARRAY_SIZE;
		cur_entry = &db->collect_array[cur_index];
		if (data_offset + cur_entry->size > buffer_size)
			break;
		memcpy(buffer + data_offset, cur_entry->data, cur_entry->size);
		data_offset += cur_entry->size;
		cur_entry->size = 0;
		kfree(cur_entry->data);
		cur_entry->data = NULL;
	}

	db->read_ptr = cur_index;
	spin_unlock_bh(&db->db_lock);
	return data_offset;
}

/**
 * iwl_dnt_dispatch_push_ftrace_handler - handles data ad push it to ftrace.
 *
 */
static void iwl_dnt_dispatch_push_ftrace_handler(struct iwl_dnt *dnt,
						 u8 *buffer, u32 buffer_size)
{
	trace_iwlwifi_dev_dnt_data(dnt->dev, buffer, buffer_size);
}

/**
 * iwl_dnt_dispatch_push_netlink_handler - handles data ad push it to netlink.
 *
 */
static int iwl_dnt_dispatch_push_netlink_handler(struct iwl_dnt *dnt,
						 struct iwl_trans *trans,
						 unsigned int cmd_id,
						 u8 *buffer, u32 buffer_size)
{
	return iwl_tm_gnl_send_msg(trans, cmd_id, false, buffer, buffer_size,
				   GFP_ATOMIC);
}

static int iwl_dnt_dispatch_pull_monitor(struct iwl_dnt *dnt,
					 struct iwl_trans *trans, u8 *buffer,
					 u32 buffer_size)
{
	int ret = 0;

	if (dnt->cur_mon_type == INTERFACE)
		ret = iwl_dnt_dispatch_get_list_data(dnt->dispatch.dbgm_db,
						     buffer, buffer_size);
	else
		ret = iwl_dnt_dev_if_retrieve_monitor_data(dnt, trans, buffer,
							   buffer_size);
	return ret;
}

int iwl_dnt_dispatch_pull(struct iwl_trans *trans, u8 *buffer, u32 buffer_size,
			  u32 input)
{
	struct iwl_dnt *dnt = trans->tmdev->dnt;
	int ret = 0;

	switch (input) {
	case MONITOR:
		ret = iwl_dnt_dispatch_pull_monitor(dnt, trans, buffer,
						    buffer_size);
		break;
	case UCODE_MESSAGES:
		ret = iwl_dnt_dispatch_get_list_data(dnt->dispatch.um_db,
						     buffer, buffer_size);
		break;
	default:
		WARN_ONCE(1, "Invalid input mode %d\n", input);
		return -EINVAL;
	}

	return ret;
}

static int iwl_dnt_dispatch_collect_data(struct iwl_dnt *dnt,
					 struct dnt_collect_db *db,
					 struct iwl_rx_packet *pkt)
{
	struct dnt_collect_entry *wr_entry;
	u32 data_size;

	data_size = GET_RX_PACKET_SIZE(pkt);
	spin_lock(&db->db_lock);
	wr_entry = &db->collect_array[db->wr_ptr];

	/*
	 * cheking if wr_ptr is already in use
	 * if so it means that we complete a cycle in the array
	 * hence replacing data in wr_ptr
	 */
	if (wr_entry->data) {
		/*
		 * since we overrun oldest data we should update read
		 * ptr to the next oldest data
		 */
		db->read_ptr = (db->read_ptr + 1) % IWL_DNT_ARRAY_SIZE;
		kfree(wr_entry->data);
		wr_entry->data = NULL;
	}

	wr_entry->size = data_size;
	wr_entry->data = kzalloc(data_size, GFP_ATOMIC);
	if (!wr_entry->data) {
		spin_unlock(&db->db_lock);
		return -ENOMEM;
	}

	memcpy(wr_entry->data, pkt->data, wr_entry->size);
	db->wr_ptr = (db->wr_ptr + 1) % IWL_DNT_ARRAY_SIZE;
	spin_unlock(&db->db_lock);

	return 0;
}

int iwl_dnt_dispatch_collect_ucode_message(struct iwl_trans *trans,
					   struct iwl_rx_cmd_buffer *rxb)
{
	struct iwl_dnt *dnt = trans->tmdev->dnt;
	struct iwl_rx_packet *pkt = rxb_addr(rxb);
	struct iwl_dnt_dispatch *dispatch;
	struct dnt_collect_db *db;
	int data_size;

	dispatch = &dnt->dispatch;
	db = dispatch->um_db;

	if (dispatch->ucode_msgs_in_mode != COLLECT) {
		IWL_INFO(dnt, "uCode Message ignored\n");
		return 0;
	}

	if (dispatch->ucode_msgs_out_mode != PUSH)
		return iwl_dnt_dispatch_collect_data(dnt, db, pkt);

	data_size = GET_RX_PACKET_SIZE(pkt);
	if (dispatch->ucode_msgs_output == FTRACE)
		iwl_dnt_dispatch_push_ftrace_handler(dnt, pkt->data, data_size);
	else if (dispatch->ucode_msgs_output == NETLINK)
		iwl_dnt_dispatch_push_netlink_handler(dnt, trans,
				IWL_TM_USER_CMD_NOTIF_UCODE_MSGS_DATA,
				pkt->data, data_size);

	return 0;
}
IWL_EXPORT_SYMBOL(iwl_dnt_dispatch_collect_ucode_message);

void iwl_dnt_dispatch_free(struct iwl_dnt *dnt, struct iwl_trans *trans)
{
	struct iwl_dnt_dispatch *dispatch = &dnt->dispatch;
	struct dnt_crash_data *crash = &dispatch->crash;

	if (dispatch->dbgm_db)
		iwl_dnt_dispatch_free_collect_db(dispatch->dbgm_db);
	if (dispatch->um_db)
		iwl_dnt_dispatch_free_collect_db(dispatch->um_db);

	if (dnt->mon_buf_cpu_addr)
		dma_free_coherent(trans->dev, dnt->mon_buf_size,
				  dnt->mon_buf_cpu_addr, dnt->mon_dma_addr);

	if (crash->sram)
		vfree(crash->sram);
	if (crash->rx)
		vfree(crash->rx);
	if (crash->dbgm)
		vfree(crash->dbgm);

	memset(dispatch, 0, sizeof(*dispatch));
}

static void iwl_dnt_dispatch_retrieve_crash_sram(struct iwl_dnt *dnt,
						 struct iwl_trans *trans)
{
	int ret;
	struct dnt_crash_data *crash = &dnt->dispatch.crash;

	if (crash->sram) {
		crash->sram_buf_size = 0;
		vfree(crash->sram);
	}

	ret = iwl_dnt_dev_if_read_sram(dnt, trans);
	if (ret) {
		IWL_ERR(dnt, "Failed to read sram\n");
		return;
	}
}

static void iwl_dnt_dispatch_retrieve_crash_rx(struct iwl_dnt *dnt,
					       struct iwl_trans *trans)
{
	int ret;
	struct dnt_crash_data *crash = &dnt->dispatch.crash;

	if (crash->rx) {
		crash->rx_buf_size = 0;
		vfree(crash->rx);
	}

	ret = iwl_dnt_dev_if_read_rx(dnt, trans);
	if (ret) {
		IWL_ERR(dnt, "Failed to read rx\n");
		return;
	}
}

static void iwl_dnt_dispatch_retrieve_crash_dbgm(struct iwl_dnt *dnt,
					       struct iwl_trans *trans)
{
	int ret;
	u32 buf_size;
	struct dnt_crash_data *crash = &dnt->dispatch.crash;

	if (crash->dbgm) {
		crash->dbgm_buf_size = 0;
		vfree(crash->dbgm);
	}

	switch (dnt->cur_mon_type) {
	case DMA:
		buf_size = dnt->mon_buf_size;
		break;
	case MARBH_ADC:
	case MARBH_DBG:
		buf_size = 0x2000 * sizeof(u32);
		break;
	case INTERFACE:
		if (dnt->dispatch.mon_output == NETLINK)
			return;
		buf_size = ARRAY_SIZE(dnt->dispatch.dbgm_db->collect_array);
		break;
	default:
		return;
	}
	crash->dbgm = vmalloc(buf_size);
	if (!crash->dbgm)
		return;

	if (dnt->cur_mon_type == INTERFACE) {
		iwl_dnt_dispatch_get_list_data(dnt->dispatch.dbgm_db,
					       crash->dbgm, buf_size);

	} else {
		ret = iwl_dnt_dev_if_retrieve_monitor_data(dnt, trans,
							   crash->dbgm,
							   buf_size);
		if (ret != buf_size) {
			IWL_ERR(dnt, "Failed to read DBGM\n");
			vfree(crash->dbgm);
			return;
		}
	}
	crash->dbgm_buf_size = buf_size;
}

static void iwl_dnt_dispatch_create_tlv(struct iwl_fw_error_dump_data *tlv,
				       u32 type, u32 len, u8 *value)
{
	tlv->type = cpu_to_le32(type);
	tlv->len = cpu_to_le32(len);
	memcpy(tlv->data, value, len);
}

static u32 iwl_dnt_dispatch_create_crash_tlv(struct iwl_trans *trans,
					     u8 **tlv_buf)
{
	struct iwl_fw_error_dump_file *dump_file;
	struct iwl_fw_error_dump_data *cur_tlv;
	struct iwl_dnt *dnt = trans->tmdev->dnt;
	struct dnt_crash_data *crash;
	u32 total_size;

	if (!dnt) {
		IWL_DEBUG_INFO(trans, "DnT is not intialized\n");
		return 0;
	}

	crash = &dnt->dispatch.crash;

	/*
	 * data will be represetned as TLV - each buffer is represented as
	 * follow:
	 * u32 - type (SRAM/DBGM/RX/TX/PERIPHERY)
	 * u32 - length
	 * u8[] - data
	 */
	total_size = sizeof(*dump_file) + crash->sram_buf_size +
		     crash->dbgm_buf_size + crash->rx_buf_size +
		     crash->tx_buf_size + crash->periph_buf_size +
		     sizeof(u32) * 10;
	dump_file = vmalloc(total_size);
	if (!dump_file)
		return 0;

	dump_file->file_len = cpu_to_le32(total_size);
	dump_file->barker = cpu_to_le32(IWL_FW_ERROR_DUMP_BARKER);
	*tlv_buf = (u8 *)dump_file;

	cur_tlv = (void *)dump_file->data;
	if (crash->sram_buf_size) {
		/* TODO: Convert to the new SMEM format */
		iwl_dnt_dispatch_create_tlv(cur_tlv, 0,
					    crash->sram_buf_size, crash->sram);
		cur_tlv = iwl_fw_error_next_data(cur_tlv);
	}
	if (crash->dbgm_buf_size) {
		iwl_dnt_dispatch_create_tlv(cur_tlv,
					    IWL_FW_ERROR_DUMP_FW_MONITOR,
					    crash->dbgm_buf_size,
					    crash->dbgm);
		cur_tlv = iwl_fw_error_next_data(cur_tlv);
	}
	if (crash->tx_buf_size) {
		iwl_dnt_dispatch_create_tlv(cur_tlv, IWL_FW_ERROR_DUMP_TXF,
					    crash->tx_buf_size, crash->tx);
		cur_tlv = iwl_fw_error_next_data(cur_tlv);
	}
	if (crash->rx_buf_size) {
		iwl_dnt_dispatch_create_tlv(cur_tlv, IWL_FW_ERROR_DUMP_RXF,
					    crash->rx_buf_size, crash->rx);
		cur_tlv = iwl_fw_error_next_data(cur_tlv);
	}

	return total_size;
}

static void iwl_dnt_dispatch_handle_crash_netlink(struct iwl_dnt *dnt,
						  struct iwl_trans *trans)
{
	int ret;
	u8 *tlv_buf;
	u32 tlv_buf_size;
	struct iwl_tm_crash_data *crash_notif;

	tlv_buf_size = iwl_dnt_dispatch_create_crash_tlv(trans, &tlv_buf);
	if (!tlv_buf_size)
		return;

	crash_notif = vmalloc(sizeof(struct iwl_tm_crash_data) + tlv_buf_size);
	if (!crash_notif)
		return;

	crash_notif->size = tlv_buf_size;
	memcpy(crash_notif->data, tlv_buf, tlv_buf_size);
	ret = iwl_tm_gnl_send_msg(trans, IWL_TM_USER_CMD_NOTIF_CRASH_DATA,
				  false, crash_notif,
				  sizeof(struct iwl_tm_crash_data) +
				  tlv_buf_size, GFP_ATOMIC);

	if (ret)
		IWL_ERR(dnt, "Failed to send crash data notification\n");

	vfree(crash_notif);
	vfree(tlv_buf);
}

void iwl_dnt_dispatch_handle_nic_err(struct iwl_trans *trans)
{
	struct iwl_dnt *dnt = trans->tmdev->dnt;
	struct iwl_dbg_cfg *dbg_cfg = &trans->dbg_cfg;

	trans->tmdev->dnt->iwl_dnt_status |= IWL_DNT_STATUS_FW_CRASH;

	if (!dbg_cfg->dbg_flags)
		return;

	if (dbg_cfg->dbg_flags & SRAM)
		iwl_dnt_dispatch_retrieve_crash_sram(dnt, trans);
	if (dbg_cfg->dbg_flags & RX_FIFO)
		iwl_dnt_dispatch_retrieve_crash_rx(dnt, trans);
	if (dbg_cfg->dbg_flags & DBGM)
		iwl_dnt_dispatch_retrieve_crash_dbgm(dnt, trans);

	if (dnt->dispatch.crash_out_mode & NETLINK)
		iwl_dnt_dispatch_handle_crash_netlink(dnt, trans);
}
IWL_EXPORT_SYMBOL(iwl_dnt_dispatch_handle_nic_err);
