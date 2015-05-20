/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2012 - 2014 Intel Corporation. All rights reserved.
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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/pci_ids.h>
#include <net/net_namespace.h>
#include <linux/netdevice.h>
#include <net/cfg80211.h>
#include <net/mac80211.h>
#include <net/netlink.h>

#include "iwl-drv.h"
#include "mvm.h"
#include "iwl-prph.h"
#include "iwl-csr.h"
#include "iwl-fh.h"
#include "iwl-io.h"
#include "iwl-trans.h"
#include "iwl-op-mode.h"
#include "iwl-tm-infc.h"
#include "iwl-tm-gnl.h"

int iwl_mvm_testmode_send_cmd(struct iwl_op_mode *op_mode,
			      struct iwl_host_cmd *cmd)
{
	struct iwl_mvm *mvm = IWL_OP_MODE_GET_MVM(op_mode);
	return iwl_mvm_send_cmd(mvm, cmd);
}

bool iwl_mvm_testmode_valid_hw_addr(u32 addr)
{
	/* TODO need to implement */
	return true;
}

u32 iwl_mvm_testmode_get_fw_ver(struct iwl_op_mode *op_mode)
{
	struct iwl_mvm *mvm = IWL_OP_MODE_GET_MVM(op_mode);
	return mvm->fw->ucode_ver;
}

struct sk_buff *iwl_mvm_testmode_alloc_reply(struct iwl_op_mode *op_mode,
					     int len)
{
	struct iwl_mvm *mvm = IWL_OP_MODE_GET_MVM(op_mode);
	return cfg80211_testmode_alloc_reply_skb(mvm->hw->wiphy, len);
}

int iwl_mvm_testmode_reply(struct iwl_op_mode *op_mode, struct sk_buff *skb)
{
	return cfg80211_testmode_reply(skb);
}

struct sk_buff *iwl_mvm_testmode_alloc_event(struct iwl_op_mode *op_mode,
					     int len)
{
	struct iwl_mvm *mvm = IWL_OP_MODE_GET_MVM(op_mode);
	return cfg80211_testmode_alloc_event_skb(mvm->hw->wiphy, len,
						 GFP_ATOMIC);
}

void iwl_mvm_testmode_event(struct iwl_op_mode *op_mode, struct sk_buff *skb)
{
	cfg80211_testmode_event(skb, GFP_ATOMIC);
}

static int iwl_mvm_tm_send_hcmd(struct iwl_mvm *mvm,
				struct iwl_tm_data *data_in,
				struct iwl_tm_data *data_out)
{
	struct iwl_tm_cmd_request *hcmd_req = data_in->data;
	struct iwl_tm_cmd_request *cmd_resp;
	u32 reply_len, resp_size;
	struct iwl_rx_packet *pkt;
	struct iwl_host_cmd host_cmd = {
		.id = hcmd_req->id,
		.data[0] = hcmd_req->data,
		.len[0] = hcmd_req->len,
		.dataflags[0] = IWL_HCMD_DFL_NOCOPY,
	};
	int ret;

	if (hcmd_req->want_resp)
		host_cmd.flags |= CMD_WANT_SKB;

	mutex_lock(&mvm->mutex);
	ret = iwl_mvm_send_cmd(mvm, &host_cmd);
	mutex_unlock(&mvm->mutex);
	if (ret)
		return ret;
	/* if no reply is required, we are done */
	if (!(host_cmd.flags & CMD_WANT_SKB))
		return 0;

	/* Retrieve response packet */
	pkt = host_cmd.resp_pkt;
	if (!pkt) {
		IWL_ERR(mvm->trans, "HCMD received a null response packet\n");
		return -ENOMSG;
	}
	reply_len = iwl_rx_packet_len(pkt);

	/* Set response data */
	resp_size = sizeof(struct iwl_tm_cmd_request) + reply_len;
	cmd_resp = kzalloc(resp_size, GFP_KERNEL);
	if (!cmd_resp) {
		iwl_free_resp(&host_cmd);
		return -ENOMEM;
	}
	cmd_resp->id = hcmd_req->id;
	cmd_resp->len = reply_len;
	memcpy(cmd_resp->data, &(pkt->hdr), reply_len);

	iwl_free_resp(&host_cmd);

	data_out->data = cmd_resp;
	data_out->len = resp_size;

	return 0;
}

static void iwl_mvm_tm_execute_reg_ops(struct iwl_trans *trans,
				       struct iwl_tm_regs_request *request,
				       struct iwl_tm_regs_request *result)
{
	struct iwl_tm_reg_op *cur_op;
	u32 idx, read_idx;
	for (idx = 0, read_idx = 0; idx < request->num; idx++) {
		cur_op = &request->reg_ops[idx];

		if  (cur_op->op_type == IWL_TM_REG_OP_READ) {
			cur_op->value = iwl_read32(trans, cur_op->address);
			memcpy(&result->reg_ops[read_idx], cur_op,
			       sizeof(*cur_op));
			read_idx++;
		} else {
			/* IWL_TM_REG_OP_WRITE is the only possible option */
			iwl_write32(trans, cur_op->address, cur_op->value);
		}
	}
}

static int iwl_mvm_tm_reg_ops(struct iwl_trans *trans,
			      struct iwl_tm_data *data_in,
			      struct iwl_tm_data *data_out)
{
	struct iwl_tm_reg_op *cur_op;
	struct iwl_tm_regs_request *request = data_in->data;
	struct iwl_tm_regs_request *result;
	u32 result_size;
	u32 idx, read_idx;
	bool is_grab_nic_access_required = true;
	unsigned long flags;

	/* Calculate result size (result is returned only for read ops) */
	for (idx = 0, read_idx = 0; idx < request->num; idx++) {
		if (request->reg_ops[idx].op_type == IWL_TM_REG_OP_READ)
			read_idx++;
		/* check if there is an operation that it is not */
		/* in the CSR range (0x00000000 - 0x000003FF)    */
		/* and not in the AL range			 */
		cur_op = &request->reg_ops[idx];
		if (IS_AL_ADDR(cur_op->address) ||
		    (cur_op->address < HBUS_BASE))
			is_grab_nic_access_required = false;

	}
	result_size = sizeof(struct iwl_tm_regs_request) +
		      read_idx*sizeof(struct iwl_tm_reg_op);

	result = kzalloc(result_size, GFP_KERNEL);
	if (!result)
		return -ENOMEM;
	result->num = read_idx;
	if (is_grab_nic_access_required) {
		if (!iwl_trans_grab_nic_access(trans, false, &flags)) {
			kfree(result);
			return -EBUSY;
		}
		iwl_mvm_tm_execute_reg_ops(trans, request, result);
		iwl_trans_release_nic_access(trans, &flags);
	} else {
		iwl_mvm_tm_execute_reg_ops(trans, request, result);
	}

	data_out->data = result;
	data_out->len = result_size;

	return 0;
}

static int iwl_tm_get_dev_info(struct iwl_mvm *mvm,
			       struct iwl_tm_data *data_out)
{
	struct iwl_tm_dev_info *dev_info;
	const u8 driver_ver[] = BACKPORTS_GIT_TRACKED;

	dev_info = kzalloc(sizeof(struct iwl_tm_dev_info) +
			   (strlen(driver_ver)+1)*sizeof(u8), GFP_KERNEL);
	if (!dev_info)
		return -ENOMEM;

	dev_info->dev_id = mvm->trans->hw_id;
	dev_info->fw_ver = mvm->fw->ucode_ver;
	dev_info->vendor_id = PCI_VENDOR_ID_INTEL;
	dev_info->silicon_step = CSR_HW_REV_STEP(mvm->trans->hw_rev);

	/* TODO: Assign real value when feature is implemented */
	dev_info->build_ver = 0x00;

	strcpy(dev_info->driver_ver, driver_ver);

	data_out->data = dev_info;
	data_out->len = sizeof(*dev_info);

	return 0;
}

static int iwl_tm_indirect_read(struct iwl_mvm *mvm,
				struct iwl_tm_data *data_in,
				struct iwl_tm_data *data_out)
{
	struct iwl_trans *trans = mvm->trans;
	struct iwl_tm_sram_read_request *cmd_in = data_in->data;
	u32 addr = cmd_in->offset;
	u32 size = cmd_in->length;
	u32 *buf32, size32, i;
	unsigned long flags;

	if (size & (sizeof(u32)-1))
		return -EINVAL;

	data_out->data = kmalloc(size, GFP_KERNEL);
	if (!data_out->data)
		return -ENOMEM;

	data_out->len = size;

	size32 = size / sizeof(u32);
	buf32 = data_out->data;

	mutex_lock(&mvm->mutex);

	/* Hard-coded periphery absolute address */
	if (IWL_ABS_PRPH_START <= addr &&
	    addr < IWL_ABS_PRPH_START + PRPH_END) {
		if (!iwl_trans_grab_nic_access(trans, false, &flags)) {
			mutex_unlock(&mvm->mutex);
			return -EBUSY;
		}
		for (i = 0; i < size32; i++)
			buf32[i] = iwl_trans_read_prph(trans,
						       addr + i * sizeof(u32));
		iwl_trans_release_nic_access(trans, &flags);
	} else {
		/* target memory (SRAM) */
		iwl_trans_read_mem(trans, addr, buf32, size32);
	}

	mutex_unlock(&mvm->mutex);
	return 0;
}

static int iwl_tm_indirect_write(struct iwl_mvm *mvm,
				 struct iwl_tm_data *data_in)
{
	struct iwl_trans *trans = mvm->trans;
	struct iwl_tm_sram_write_request *cmd_in = data_in->data;
	u32 addr = cmd_in->offset;
	u32 size = cmd_in->len;
	u8 *buf = cmd_in->buffer;
	u32 *buf32 = (u32 *)buf, size32 = size / sizeof(u32);
	unsigned long flags;
	u32 val, i;

	mutex_lock(&mvm->mutex);
	if (IWL_ABS_PRPH_START <= addr &&
	    addr < IWL_ABS_PRPH_START + PRPH_END) {
		/* Periphery writes can be 1-3 bytes long, or DWORDs */
		if (size < 4) {
			memcpy(&val, buf, size);
			if (!iwl_trans_grab_nic_access(trans, false, &flags)) {
				mutex_unlock(&mvm->mutex);
				return -EBUSY;
			}
			iwl_write32(trans, HBUS_TARG_PRPH_WADDR,
				    (addr & 0x000FFFFF) | ((size - 1) << 24));
			iwl_write32(trans, HBUS_TARG_PRPH_WDAT, val);
			iwl_trans_release_nic_access(trans, &flags);
		} else {
			if (size % sizeof(u32)) {
				mutex_unlock(&mvm->mutex);
				return -EINVAL;
			}

			for (i = 0; i < size32; i++)
				iwl_write_prph(trans, addr + i*sizeof(u32),
					       buf32[i]);
		}
	} else {
		iwl_trans_write_mem(trans, addr, buf32, size32);
	}
	mutex_unlock(&mvm->mutex);

	return 0;
}

/**
 * iwl_mvm_tm_cmd_execute - Implementation of test command executor callback
 * @op_mode:	  Specific device's operation mode
 * @cmd:	  User space command's index
 * @data_in:	  Input data. "data" field is to be casted to relevant
 *		  data structure. All verification must be done in the
 *		  caller function, therefor assuming that input data
 *		  length is valid.
 * @data_out:	  Will be allocated inside, freeing is in the caller's
 *		  responsibility
 */
int iwl_mvm_tm_cmd_execute(struct iwl_op_mode *op_mode, u32 cmd,
			   struct iwl_tm_data *data_in,
			   struct iwl_tm_data *data_out)
{
	struct iwl_mvm *mvm = IWL_OP_MODE_GET_MVM(op_mode);
	int ret;

	if (WARN_ON_ONCE(!op_mode || !data_in))
		return -EINVAL;

	ret = iwl_mvm_ref_sync(mvm, IWL_MVM_REF_TM_CMD);
	if (ret)
		return ret;

	switch (cmd) {
	case IWL_TM_USER_CMD_HCMD:
		ret = iwl_mvm_tm_send_hcmd(mvm, data_in, data_out);
		break;
	case IWL_TM_USER_CMD_REG_ACCESS:
		ret = iwl_mvm_tm_reg_ops(mvm->trans, data_in, data_out);
		break;
	case IWL_TM_USER_CMD_SRAM_WRITE:
		ret = iwl_tm_indirect_write(mvm, data_in);
		break;
	case IWL_TM_USER_CMD_SRAM_READ:
		ret = iwl_tm_indirect_read(mvm, data_in, data_out);
		break;
	case IWL_TM_USER_CMD_GET_DEVICE_INFO:
		ret = iwl_tm_get_dev_info(mvm, data_out);
		break;
	default:
		ret = -EOPNOTSUPP;
		break;
	}

	iwl_mvm_unref(mvm, IWL_MVM_REF_TM_CMD);

	return ret;
}

#ifdef CPTCFG_NL80211_TESTMODE
/**
 * iwl_tm_mvm_retrieve_monitor() - trigger monitor retrieve event
 */
int iwl_tm_mvm_retrieve_monitor(struct ieee80211_hw *hw,
				struct ieee80211_tx_thrshld_md *md)
{
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct iwl_tm_thrshld_md tm_md;

	if (!md)
		return -1;

	tm_md.mode = md->mode;
	tm_md.monitor_collec_wind = md->monitor_collec_wind;
	tm_md.seq = md->seq;
	tm_md.pkt_start = md->pkt_start;
	tm_md.pkt_end = md->pkt_end;
	tm_md.msrmnt = md->msrmnt;
	tm_md.tid = md->tid;

	return iwl_tm_gnl_send_msg(mvm->trans,
				   IWL_TM_USER_CMD_NOTIF_RETRIEVE_MONITOR,
				   false, &tm_md, sizeof(tm_md),
				   GFP_ATOMIC);
}
#endif

/**
 * iwl_tm_mvm_send_rx() - Send a spontaneous rx message to user
 * @mvm:	mvm opmode pointer
 * @rxb:	Contains rx packet to be sent
 */
void iwl_tm_mvm_send_rx(struct iwl_mvm *mvm, struct iwl_rx_cmd_buffer *rxb)
{
	struct iwl_rx_packet *pkt = rxb_addr(rxb);
	int length = iwl_rx_packet_len(pkt);

	/* the length doesn't include len_n_flags field, so add it manually */
	length += sizeof(__le32);

	iwl_tm_gnl_send_msg(mvm->trans, IWL_TM_USER_CMD_NOTIF_UCODE_RX_PKT,
			    true, (void *)pkt, length, GFP_ATOMIC);
}
