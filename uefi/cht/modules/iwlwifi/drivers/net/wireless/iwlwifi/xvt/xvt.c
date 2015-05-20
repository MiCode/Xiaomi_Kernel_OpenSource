/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2007 - 2014 Intel Corporation. All rights reserved.
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
 * Copyright(c) 2005 - 2014 Intel Corporation. All rights reserved.
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
#include <linux/module.h>
#include <linux/types.h>

#include "iwl-drv.h"
#include "iwl-trans.h"
#include "iwl-op-mode.h"
#include "iwl-fw.h"
#include "iwl-config.h"
#include "iwl-phy-db.h"
#include "iwl-csr.h"
#include "xvt.h"
#include "user-infc.h"
#include "iwl-dnt-cfg.h"
#include "iwl-dnt-dispatch.h"

#define DRV_DESCRIPTION	"Intel(R) xVT driver for Linux"
MODULE_DESCRIPTION(DRV_DESCRIPTION);
MODULE_AUTHOR(DRV_COPYRIGHT " " DRV_AUTHOR);
MODULE_LICENSE("GPL");

static const struct iwl_op_mode_ops iwl_xvt_ops;

/*
 * module init and exit functions
 */
static int __init iwl_xvt_init(void)
{
	return iwl_opmode_register("iwlxvt", &iwl_xvt_ops);
}
module_init(iwl_xvt_init);

static void __exit iwl_xvt_exit(void)
{
	iwl_opmode_deregister("iwlxvt");
}
module_exit(iwl_xvt_exit);

#define CMD(x) [x] = #x

static const char *const iwl_xvt_cmd_strings[REPLY_MAX] = {
	CMD(XVT_ALIVE),
	CMD(INIT_COMPLETE_NOTIF),
	CMD(TX_CMD),
	CMD(PHY_CONFIGURATION_CMD),
	CMD(CALIB_RES_NOTIF_PHY_DB),
	CMD(REPLY_RX_PHY_CMD),
	CMD(REPLY_RX_MPDU_CMD),
	CMD(REPLY_RX_DSP_EXT_INFO),
};
#undef CMD

static struct iwl_op_mode *iwl_xvt_start(struct iwl_trans *trans,
					 const struct iwl_cfg *cfg,
					 const struct iwl_fw *fw,
					 struct dentry *dbgfs_dir)
{
	struct iwl_op_mode *op_mode;
	struct iwl_xvt *xvt;
	struct iwl_trans_config trans_cfg = {};
	static const u8 no_reclaim_cmds[] = {
		TX_CMD,
	};

	op_mode = kzalloc(sizeof(struct iwl_op_mode) +
			  sizeof(struct iwl_xvt), GFP_KERNEL);
	if (!op_mode)
		return NULL;

	op_mode->ops = &iwl_xvt_ops;

	xvt = IWL_OP_MODE_GET_XVT(op_mode);
	xvt->fw = fw;
	xvt->cfg = cfg;
	xvt->trans = trans;
	xvt->dev = trans->dev;

	mutex_init(&xvt->mutex);

	/*
	 * Populate the state variables that the
	 * transport layer needs to know about.
	 */
	trans_cfg.op_mode = op_mode;
	trans_cfg.no_reclaim_cmds = no_reclaim_cmds;
	trans_cfg.n_no_reclaim_cmds = ARRAY_SIZE(no_reclaim_cmds);
	trans_cfg.command_names = iwl_xvt_cmd_strings;

	trans_cfg.cmd_queue = IWL_XVT_CMD_QUEUE;
	trans_cfg.cmd_fifo = IWL_XVT_CMD_FIFO;
	trans_cfg.rx_buf_size_8k = false;
	if (xvt->fw->ucode_capa.flags & IWL_UCODE_TLV_FLAGS_DW_BC_TABLE)
		trans_cfg.bc_table_dword = true;
	trans_cfg.scd_set_active = true;

	/* Configure transport layer */
	iwl_trans_configure(xvt->trans, &trans_cfg);

	/* set up notification wait support */
	iwl_notification_wait_init(&xvt->notif_wait);

	/* Init phy db */
	xvt->phy_db = iwl_phy_db_init(xvt->trans);
	if (!xvt->phy_db)
		goto out_free;

	iwl_dnt_init(xvt->trans, dbgfs_dir);

	init_waitqueue_head(&xvt->mod_tx_wq);
	init_waitqueue_head(&xvt->mod_tx_done_wq);

	IWL_INFO(xvt, "Detected %s, REV=0x%X, xVT operation mode\n",
		 xvt->cfg->name, xvt->trans->hw_rev);

	return op_mode;

out_free:
	kfree(op_mode);

	return NULL;
}

static void iwl_xvt_stop(struct iwl_op_mode *op_mode)
{
	struct iwl_xvt *xvt = IWL_OP_MODE_GET_XVT(op_mode);

	if (xvt->state != IWL_XVT_STATE_UNINITIALIZED) {
		if (xvt->fw_running) {
			iwl_trans_txq_disable(xvt->trans,
					      IWL_XVT_DEFAULT_TX_QUEUE,
					      true);
			xvt->fw_running = false;
		}
		iwl_trans_stop_device(xvt->trans);
	}

	iwl_phy_db_free(xvt->phy_db);
	xvt->phy_db = NULL;
	iwl_dnt_free(xvt->trans);
	kfree(op_mode);
}

static void iwl_xvt_rx_tx_cmd_handler(struct iwl_xvt *xvt,
				      struct iwl_rx_packet *pkt)
{
	struct iwl_xvt_tx_resp *tx_resp = (void *)pkt->data;
	int txq_id = SEQ_TO_QUEUE(le16_to_cpu(pkt->hdr.sequence));
	u16 ssn = iwl_xvt_get_scd_ssn(tx_resp);
	struct sk_buff_head skbs;
	struct sk_buff *skb;
	struct iwl_device_cmd **cb_dev_cmd;

	__skb_queue_head_init(&skbs);

	iwl_trans_reclaim(xvt->trans, txq_id, ssn, &skbs);

	while (!skb_queue_empty(&skbs)) {
		skb = __skb_dequeue(&skbs);
		cb_dev_cmd = (void *)skb->cb;
		xvt->tx_counter++;
		iwl_trans_free_tx_cmd(xvt->trans, *cb_dev_cmd);
		kfree_skb(skb);
	}
	if (xvt->tot_tx == xvt->tx_counter)
		wake_up_interruptible(&xvt->mod_tx_done_wq);
}

static int iwl_xvt_rx_dispatch(struct iwl_op_mode *op_mode,
			       struct iwl_rx_cmd_buffer *rxb,
			       struct iwl_device_cmd *cmd)
{
	struct iwl_xvt *xvt = IWL_OP_MODE_GET_XVT(op_mode);
	struct iwl_rx_packet *pkt = rxb_addr(rxb);

	iwl_notification_wait_notify(&xvt->notif_wait, pkt);

	if (pkt->hdr.cmd == TX_CMD)
		iwl_xvt_rx_tx_cmd_handler(xvt, pkt);

	return iwl_xvt_send_user_rx_notif(xvt, rxb);
}

static void iwl_xvt_nic_config(struct iwl_op_mode *op_mode)
{
	struct iwl_xvt *xvt = IWL_OP_MODE_GET_XVT(op_mode);
	/*
	 * TODO: Define NIC configuration flow
	 *
	 * Handle is required for operational flow,
	 * so in order to avoid problems, at the
	 * meanwhile this callback is implemented
	 * as a stub.
	 */
	iwl_trans_set_bits_mask(xvt->trans,
				CSR_HW_IF_CONFIG_REG,
				CSR_HW_IF_CONFIG_REG_BIT_MAC_SI,
				CSR_HW_IF_CONFIG_REG_BIT_MAC_SI);
}

static void iwl_xvt_nic_error(struct iwl_op_mode *op_mode)
{
	struct iwl_xvt *xvt = IWL_OP_MODE_GET_XVT(op_mode);
	void *p_table;
	struct iwl_error_event_table_v1 table_v1;
	struct iwl_error_event_table_v2 table_v2;
	int err, table_size;

	xvt->fw_error = true;
	wake_up_interruptible(&xvt->mod_tx_wq);

	if (xvt->fw->ucode_capa.api[0] & IWL_UCODE_TLV_API_NEW_VERSION) {
		iwl_xvt_get_nic_error_log_v2(xvt, &table_v2);
		iwl_xvt_dump_nic_error_log_v2(xvt, &table_v2);
		p_table = kmemdup(&table_v2, sizeof(table_v2), GFP_ATOMIC);
		table_size = sizeof(table_v2);
	} else {
		iwl_xvt_get_nic_error_log_v1(xvt, &table_v1);
		iwl_xvt_dump_nic_error_log_v1(xvt, &table_v1);
		p_table = kmemdup(&table_v1, sizeof(table_v1), GFP_ATOMIC);
		table_size = sizeof(table_v1);
	}
	if (p_table) {
		err = iwl_xvt_user_send_notif(xvt, IWL_XVT_CMD_SEND_NIC_ERROR,
					      (void *)p_table, table_size,
					      GFP_ATOMIC);
		if (err)
			IWL_WARN(xvt,
				 "Error %d sending NIC error notification\n",
				 err);
	}
}

static bool iwl_xvt_set_hw_rfkill_state(struct iwl_op_mode *op_mode, bool state)
{
	struct iwl_xvt *xvt = IWL_OP_MODE_GET_XVT(op_mode);
	u32 rfkill_state = state ? IWL_XVT_RFKILL_ON : IWL_XVT_RFKILL_OFF;
	int err;

	err = iwl_xvt_user_send_notif(xvt, IWL_XVT_CMD_SEND_RFKILL,
				      &rfkill_state, sizeof(rfkill_state),
				      GFP_ATOMIC);
	if (err)
		IWL_WARN(xvt, "Error %d sending RFKILL notification\n", err);

	return false;
}

static bool iwl_xvt_valid_hw_addr(u32 addr)
{
	/* TODO need to implement */
	return true;
}

static void iwl_xvt_free_skb(struct iwl_op_mode *op_mode, struct sk_buff *skb)
{
	struct iwl_xvt *xvt = IWL_OP_MODE_GET_XVT(op_mode);
	struct iwl_device_cmd **cb_dev_cmd = (void *)skb->cb;

	iwl_trans_free_tx_cmd(xvt->trans, *cb_dev_cmd);
	kfree_skb(skb);
}

static void iwl_xvt_stop_sw_queue(struct iwl_op_mode *op_mode, int queue)
{
	struct iwl_xvt *xvt = IWL_OP_MODE_GET_XVT(op_mode);

	xvt->txq_full = true;
}

static void iwl_xvt_wake_sw_queue(struct iwl_op_mode *op_mode, int queue)
{
	struct iwl_xvt *xvt = IWL_OP_MODE_GET_XVT(op_mode);

	xvt->txq_full = false;
	wake_up_interruptible(&xvt->mod_tx_wq);
}

static const struct iwl_op_mode_ops iwl_xvt_ops = {
	.start = iwl_xvt_start,
	.stop = iwl_xvt_stop,
	.rx = iwl_xvt_rx_dispatch,
	.nic_config = iwl_xvt_nic_config,
	.nic_error = iwl_xvt_nic_error,
	.hw_rf_kill = iwl_xvt_set_hw_rfkill_state,
	.free_skb = iwl_xvt_free_skb,
	.queue_full = iwl_xvt_stop_sw_queue,
	.queue_not_full = iwl_xvt_wake_sw_queue,
	.test_ops = {
		.cmd_execute = iwl_xvt_user_cmd_execute,
		.valid_hw_addr = iwl_xvt_valid_hw_addr,
	},
};
