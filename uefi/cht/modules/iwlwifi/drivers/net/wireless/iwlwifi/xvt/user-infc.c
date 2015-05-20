/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2007 - 2014 Intel Corporation. All rights reserved.
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
 * Copyright(c) 2005 - 2014 Intel Corporation. All rights reserved.
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

#include "iwl-drv.h"
#include "iwl-prph.h"
#include "iwl-csr.h"
#include "iwl-io.h"
#include "iwl-trans.h"
#include "iwl-op-mode.h"
#include "iwl-phy-db.h"
#include "xvt.h"
#include "user-infc.h"
#include "iwl-tm-gnl.h"
#include "iwl-dnt-cfg.h"
#include "iwl-dnt-dispatch.h"
#include "iwl-trans.h"

#define XVT_UCODE_CALIB_TIMEOUT (2*HZ)
#define XVT_SCU_BASE	(0xe6a00000)
#define XVT_SCU_SNUM1	(XVT_SCU_BASE + 0x300)
#define XVT_SCU_SNUM2	(XVT_SCU_SNUM1 + 0x4)
#define XVT_SCU_SNUM3	(XVT_SCU_SNUM2 + 0x4)


int iwl_xvt_send_user_rx_notif(struct iwl_xvt *xvt,
			       struct iwl_rx_cmd_buffer *rxb)
{
	struct iwl_rx_packet *pkt = rxb_addr(rxb);
	void *data = pkt->data;
	u32 size = iwl_rx_packet_payload_len(pkt);

	switch (pkt->hdr.cmd) {
	case GET_SET_PHY_DB_CMD:
		return iwl_xvt_user_send_notif(xvt,
					       IWL_TM_USER_CMD_NOTIF_PHY_DB,
					       data, size, GFP_ATOMIC);
		break;
	case DTS_MEASUREMENT_NOTIFICATION:
		return iwl_xvt_user_send_notif(xvt,
					IWL_TM_USER_CMD_NOTIF_DTS_MEASUREMENTS,
					data, size, GFP_ATOMIC);
		break;
	case REPLY_RX_DSP_EXT_INFO:
		if (!xvt->rx_hdr_enabled)
			break;

		return iwl_xvt_user_send_notif(xvt,
					       IWL_TM_USER_CMD_NOTIF_RX_HDR,
					       data, size, GFP_ATOMIC);
	case APMG_PD_SV_CMD:
		if (!xvt->apmg_pd_en)
			break;

		return iwl_xvt_user_send_notif(xvt,
						IWL_TM_USER_CMD_NOTIF_APMG_PD,
						   data, size, GFP_ATOMIC);
	case REPLY_RX_MPDU_CMD:
		return iwl_xvt_user_send_notif(xvt,
					IWL_TM_USER_CMD_NOTIF_UCODE_RX_PKT,
					data, size, GFP_ATOMIC);

	case NVM_COMMIT_COMPLETE_NOTIFICATION:
		return iwl_xvt_user_send_notif(xvt,
					IWL_TM_USER_CMD_NOTIF_COMMIT_STATISTICS,
					data, size, GFP_ATOMIC);

	case REPLY_HD_PARAMS_CMD:
		return iwl_xvt_user_send_notif(xvt,
				IWL_TM_USER_CMD_NOTIF_BFE,
				data, size, GFP_ATOMIC);

	case DEBUG_LOG_MSG:
		return iwl_dnt_dispatch_collect_ucode_message(xvt->trans, rxb);

	case REPLY_RX_PHY_CMD:
		IWL_DEBUG_INFO(xvt,
			       "REPLY_RX_PHY_CMD received but not handled\n");
	default:
		IWL_DEBUG_INFO(xvt, "xVT mode RX command 0x%x not handled\n",
			       pkt->hdr.cmd);
	}

	return 0;
}

static void iwl_xvt_led_enable(struct iwl_xvt *xvt)
{
	iwl_write32(xvt->trans, CSR_LED_REG, CSR_LED_REG_TURN_ON);
}

static void iwl_xvt_led_disable(struct iwl_xvt *xvt)
{
	iwl_write32(xvt->trans, CSR_LED_REG, CSR_LED_REG_TURN_OFF);
}

static int iwl_xvt_sdio_io_toggle(struct iwl_xvt *xvt,
				 struct iwl_tm_data *data_in,
				 struct iwl_tm_data *data_out)
{
	struct iwl_tm_sdio_io_toggle *sdio_io_toggle = data_in->data;

	return iwl_trans_test_mode_cmd(xvt->trans, sdio_io_toggle->enable);
}

static int iwl_xvt_send_hcmd(struct iwl_xvt *xvt,
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

	ret = iwl_xvt_send_cmd(xvt, &host_cmd);
	if (ret)
		return ret;
	/* if no reply is required, we are done */
	if (!(host_cmd.flags & CMD_WANT_SKB))
		return 0;

	/* Retrieve response packet */
	pkt = host_cmd.resp_pkt;
	if (!pkt) {
		IWL_ERR(xvt->trans, "HCMD received a null response packet\n");
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

static void iwl_xvt_execute_reg_ops(struct iwl_trans *trans,
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

static int iwl_xvt_reg_ops(struct iwl_trans *trans,
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
		iwl_xvt_execute_reg_ops(trans, request, result);
		iwl_trans_release_nic_access(trans, &flags);
	} else {
		iwl_xvt_execute_reg_ops(trans, request, result);
	}

	data_out->data = result;
	data_out->len = result_size;

	return 0;
}

/**
 * iwl_xvt_read_sv_drop - read SV drop version
 * @xvt: xvt data
 * Return: the SV drop (>= 0) or a negative error number
 */
static int iwl_xvt_read_sv_drop(struct iwl_xvt *xvt)
{
	struct xvt_debug_cmd debug_cmd = {
		.opcode = cpu_to_le32(XVT_DBG_GET_SVDROP_VER_OP),
		.dw_num = 0,
	};
	struct xvt_debug_res *debug_res;
	struct iwl_rx_packet *pkt;
	struct iwl_host_cmd host_cmd = {
		.id = REPLY_DEBUG_XVT_CMD,
		.data[0] = &debug_cmd,
		.len[0] = sizeof(debug_cmd),
		.dataflags[0] = IWL_HCMD_DFL_NOCOPY,
		.flags = CMD_WANT_SKB,
	};
	int ret;

	if (xvt->state != IWL_XVT_STATE_OPERATIONAL)
		return 0;

	ret = iwl_xvt_send_cmd(xvt, &host_cmd);
	if (ret)
		return ret;

	/* Retrieve response packet */
	pkt = host_cmd.resp_pkt;
	if (!pkt) {
		IWL_ERR(xvt->trans, "HCMD received a null response packet\n");
		return -ENOMSG;
	}

	/* Get response data */
	debug_res = (struct xvt_debug_res *)pkt->data;
	if (le32_to_cpu(debug_res->dw_num) < 1)
		return -ENODATA;
	return le32_to_cpu(debug_res->data[0]) & 0xFF;
}

static int iwl_xvt_get_dev_info(struct iwl_xvt *xvt,
				struct iwl_tm_data *data_in,
				struct iwl_tm_data *data_out)
{
	struct iwl_tm_dev_info_req *dev_info_req;
	struct iwl_tm_dev_info *dev_info;
	const u8 driver_ver[] = BACKPORTS_GIT_TRACKED;
	int sv_step = 0x00;
	int dev_info_size;
	bool read_sv_drop = true;

	if (data_in) {
		dev_info_req = (struct iwl_tm_dev_info_req *)data_in->data;
		read_sv_drop = dev_info_req->read_sv ? true : false;
	}

	if (xvt->cur_ucode == IWL_UCODE_REGULAR && read_sv_drop) {
		sv_step = iwl_xvt_read_sv_drop(xvt);
		if (sv_step < 0)
			return sv_step;
	}

	dev_info_size = sizeof(struct iwl_tm_dev_info) +
			(strlen(driver_ver)+1)*sizeof(u8);
	dev_info = kzalloc(dev_info_size, GFP_KERNEL);
	if (!dev_info)
		return -ENOMEM;

	dev_info->dev_id = xvt->trans->hw_id;
	dev_info->fw_ver = xvt->fw->ucode_ver;
	dev_info->vendor_id = PCI_VENDOR_ID_INTEL;
	dev_info->build_ver = sv_step;

	/*
	 * TODO: Silicon step is retrieved by reading
	 * radio register 0x00. Simplifying implementation
	 * by reading it in user space.
	 */
	dev_info->silicon_step = 0x00;

	strcpy(dev_info->driver_ver, driver_ver);

	data_out->data = dev_info;
	data_out->len = dev_info_size;

	return 0;
}

static int iwl_xvt_indirect_read(struct iwl_xvt *xvt,
				 struct iwl_tm_data *data_in,
				 struct iwl_tm_data *data_out)
{
	struct iwl_trans *trans = xvt->trans;
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

	/* Hard-coded periphery absolute address */
	if (IWL_ABS_PRPH_START <= addr &&
	    addr < IWL_ABS_PRPH_START + PRPH_END) {
		if (!iwl_trans_grab_nic_access(trans, false, &flags))
			return -EBUSY;
		for (i = 0; i < size32; i++)
			buf32[i] = iwl_trans_read_prph(trans,
						       addr + i * sizeof(u32));
		iwl_trans_release_nic_access(trans, &flags);
	} else {
		/* target memory (SRAM) */
		iwl_trans_read_mem(trans, addr, buf32, size32);
	}

	return 0;
}

static int iwl_xvt_indirect_write(struct iwl_xvt *xvt,
				  struct iwl_tm_data *data_in)
{
	struct iwl_trans *trans = xvt->trans;
	struct iwl_tm_sram_write_request *cmd_in = data_in->data;
	u32 addr = cmd_in->offset;
	u32 size = cmd_in->len;
	u8 *buf = cmd_in->buffer;
	u32 *buf32 = (u32 *)buf, size32 = size / sizeof(u32);
	unsigned long flags;
	u32 val, i;

	if (IWL_ABS_PRPH_START <= addr &&
	    addr < IWL_ABS_PRPH_START + PRPH_END) {
		/* Periphery writes can be 1-3 bytes long, or DWORDs */
		if (size < 4) {
			memcpy(&val, buf, size);
			if (!iwl_trans_grab_nic_access(trans, false, &flags))
				return -EBUSY;
			iwl_write32(trans, HBUS_TARG_PRPH_WADDR,
				    (addr & 0x0000FFFF) | ((size - 1) << 24));
			iwl_write32(trans, HBUS_TARG_PRPH_WDAT, val);
			iwl_trans_release_nic_access(trans, &flags);
		} else {
			if (size % sizeof(u32))
				return -EINVAL;

			for (i = 0; i < size32; i++)
				iwl_write_prph(trans, addr + i*sizeof(u32),
					       buf32[i]);
		}
	} else {
		iwl_trans_write_mem(trans, addr, buf32, size32);
	}

	return 0;
}

static int iwl_xvt_set_sw_config(struct iwl_xvt *xvt,
				  struct iwl_tm_data *data_in)
{
	struct iwl_xvt_sw_cfg_request *sw_cfg =
				(struct iwl_xvt_sw_cfg_request *)data_in->data;
	struct iwl_phy_cfg_cmd *fw_calib_cmd_cfg =
				xvt->sw_stack_cfg.fw_calib_cmd_cfg;
	__le32 cfg_mask = cpu_to_le32(sw_cfg->cfg_mask),
	       fw_calib_event, fw_calib_flow,
	       event_override, flow_override;
	int usr_idx, iwl_idx;

	if (data_in->len < sizeof(struct iwl_xvt_sw_cfg_request))
		return -EINVAL;

	xvt->sw_stack_cfg.fw_dbg_flags = sw_cfg->dbg_flags;
	xvt->sw_stack_cfg.load_mask = sw_cfg->load_mask;
	xvt->sw_stack_cfg.calib_override_mask = sw_cfg->cfg_mask;

	for (usr_idx = 0; usr_idx < IWL_USER_FW_IMAGE_IDX_TYPE_MAX; usr_idx++) {
		switch (usr_idx) {
		case IWL_USER_FW_IMAGE_IDX_INIT:
			iwl_idx = IWL_UCODE_INIT;
			break;
		case IWL_USER_FW_IMAGE_IDX_REGULAR:
			iwl_idx = IWL_UCODE_REGULAR;
			break;
		case IWL_USER_FW_IMAGE_IDX_WOWLAN:
			iwl_idx = IWL_UCODE_WOWLAN;
			break;
		}
		/* TODO: Calculate PHY config according to device values */
		fw_calib_cmd_cfg[iwl_idx].phy_cfg =
			cpu_to_le32(xvt->fw->phy_config);

		/*
		 * If a cfg_mask bit is unset, take the default value
		 * from the FW. Otherwise, take the value from sw_cfg.
		 */
		fw_calib_event = xvt->fw->default_calib[iwl_idx].event_trigger;
		event_override =
			 cpu_to_le32(sw_cfg->calib_ctrl[usr_idx].event_trigger);

		fw_calib_cmd_cfg[iwl_idx].calib_control.event_trigger =
			(~cfg_mask & fw_calib_event) |
			(cfg_mask & event_override);

		fw_calib_flow = xvt->fw->default_calib[iwl_idx].flow_trigger;
		flow_override =
			cpu_to_le32(sw_cfg->calib_ctrl[usr_idx].flow_trigger);

		fw_calib_cmd_cfg[iwl_idx].calib_control.flow_trigger =
			(~cfg_mask & fw_calib_flow) |
			(cfg_mask & flow_override);
	}

	return 0;
}

static int iwl_xvt_get_sw_config(struct iwl_xvt *xvt,
				 struct iwl_tm_data *data_in,
				 struct iwl_tm_data *data_out)
{
	struct iwl_xvt_sw_cfg_request *get_cfg_req;
	struct iwl_xvt_sw_cfg_request *sw_cfg;
	struct iwl_phy_cfg_cmd *fw_calib_cmd_cfg =
				xvt->sw_stack_cfg.fw_calib_cmd_cfg;
	__le32 event_trigger, flow_trigger;
	int i, u;

	if (data_in->len < sizeof(struct iwl_xvt_sw_cfg_request))
		return -EINVAL;

	get_cfg_req = data_in->data;
	sw_cfg = kzalloc(sizeof(*sw_cfg), GFP_KERNEL);
	if (!sw_cfg)
		return -ENOMEM;

	sw_cfg->load_mask = xvt->sw_stack_cfg.load_mask;
	sw_cfg->phy_config = xvt->fw->phy_config;
	sw_cfg->cfg_mask = xvt->sw_stack_cfg.calib_override_mask;
	sw_cfg->dbg_flags = xvt->sw_stack_cfg.fw_dbg_flags;
	for (i = 0; i < IWL_UCODE_TYPE_MAX; i++) {
		switch (i) {
		case IWL_UCODE_INIT:
			u = IWL_USER_FW_IMAGE_IDX_INIT;
			break;
		case IWL_UCODE_REGULAR:
			u = IWL_USER_FW_IMAGE_IDX_REGULAR;
			break;
		case IWL_UCODE_WOWLAN:
			u = IWL_USER_FW_IMAGE_IDX_WOWLAN;
			break;
		case IWL_UCODE_REGULAR_USNIFFER:
			continue;
		}
		if (get_cfg_req->get_calib_type == IWL_XVT_GET_CALIB_TYPE_DEF) {
			event_trigger =
				xvt->fw->default_calib[i].event_trigger;
			flow_trigger =
				xvt->fw->default_calib[i].flow_trigger;
		} else {
			event_trigger =
				fw_calib_cmd_cfg[i].calib_control.event_trigger;
			flow_trigger =
				fw_calib_cmd_cfg[i].calib_control.flow_trigger;
		}
		sw_cfg->calib_ctrl[u].event_trigger =
			le32_to_cpu(event_trigger);
		sw_cfg->calib_ctrl[u].flow_trigger =
			le32_to_cpu(flow_trigger);
	}

	data_out->data = sw_cfg;
	data_out->len = sizeof(*sw_cfg);
	return 0;
}

static int iwl_xvt_send_phy_cfg_cmd(struct iwl_xvt *xvt, u32 ucode_type)
{
	struct iwl_phy_cfg_cmd *calib_cmd_cfg =
		&xvt->sw_stack_cfg.fw_calib_cmd_cfg[ucode_type];
	int err;

	IWL_DEBUG_INFO(xvt, "Sending Phy CFG command: 0x%x\n",
		       calib_cmd_cfg->phy_cfg);

	/* Sending calibration configuration control data */
	err = iwl_xvt_send_cmd_pdu(xvt, PHY_CONFIGURATION_CMD, 0,
				   sizeof(*calib_cmd_cfg), calib_cmd_cfg);
	if (err)
		IWL_ERR(xvt, "Error (%d) running INIT calibrations control\n",
			err);

	return err;
}

static int iwl_xvt_run_runtime_fw(struct iwl_xvt *xvt)
{
	int err;

	err = iwl_xvt_run_fw(xvt, IWL_UCODE_REGULAR);
	if (err)
		goto fw_error;

	xvt->state = IWL_XVT_STATE_OPERATIONAL;

	/* Send phy db control command and then phy db calibration*/
	err = iwl_send_phy_db_data(xvt->phy_db);
	if (err)
		goto phy_error;

	err = iwl_xvt_send_phy_cfg_cmd(xvt, IWL_UCODE_REGULAR);
	if (err)
		goto phy_error;

	return 0;

phy_error:
	iwl_trans_stop_device(xvt->trans);

fw_error:
	xvt->state = IWL_XVT_STATE_UNINITIALIZED;

	return err;
}

static bool iwl_xvt_wait_phy_db_entry(struct iwl_notif_wait_data *notif_wait,
				  struct iwl_rx_packet *pkt, void *data)
{
	struct iwl_phy_db *phy_db = data;

	if (pkt->hdr.cmd != CALIB_RES_NOTIF_PHY_DB) {
		WARN_ON(pkt->hdr.cmd != INIT_COMPLETE_NOTIF);
		return true;
	}

	WARN_ON(iwl_phy_db_set_section(phy_db, pkt, GFP_ATOMIC));

	return false;
}

/*
 * iwl_xvt_start_op_mode starts FW according to load mask,
 * waits for alive notification from device, and sends it
 * to user.
 */
static int iwl_xvt_start_op_mode(struct iwl_xvt *xvt)
{
	int err = 0;

	/*
	 * If init FW and runtime FW are both enabled,
	 * Runtime FW will be executed after "continue
	 * initialization" is done.
	 * If init FW is disabled and runtime FW is
	 * enabled, run Runtime FW. If runtime fw is
	 * disabled, do nothing.
	 */
	if (!(xvt->sw_stack_cfg.load_mask & IWL_XVT_LOAD_MASK_INIT)) {
		if (xvt->sw_stack_cfg.load_mask & IWL_XVT_LOAD_MASK_RUNTIME) {
			err = iwl_xvt_run_runtime_fw(xvt);
		} else {
			if (xvt->state != IWL_XVT_STATE_UNINITIALIZED) {
				xvt->fw_running = false;
				iwl_trans_stop_device(xvt->trans);
			}
			err = iwl_trans_start_hw(xvt->trans);
			if (err) {
				IWL_ERR(xvt, "Failed to start HW\n");
			} else {
				iwl_write32(xvt->trans, CSR_RESET, 0);
				xvt->state = IWL_XVT_STATE_NO_FW;
			}
		}

		return err;
	}

	err = iwl_xvt_run_fw(xvt, IWL_UCODE_INIT);
	if (err)
		return err;

	xvt->state = IWL_XVT_STATE_INIT_STARTED;

	err = iwl_xvt_nvm_init(xvt);
	if (err)
		IWL_ERR(xvt, "Failed to read NVM: %d\n", err);

	/*
	 * The initialization flow is not yet complete.
	 * User need to execute "Continue initialization"
	 * flow in order to complete it.
	 *
	 * NOT sending ALIVE notification to user. User
	 * knows that FW is alive when "start op mode"
	 * returns without errors.
	 */

	return err;
}

static void iwl_xvt_stop_op_mode(struct iwl_xvt *xvt)
{
	if (xvt->state == IWL_XVT_STATE_UNINITIALIZED)
		return;

	if (xvt->fw_running) {
		iwl_trans_txq_disable(xvt->trans, IWL_XVT_DEFAULT_TX_QUEUE,
				      true);
		xvt->fw_running = false;
	}
	iwl_trans_stop_device(xvt->trans);
	xvt->state = IWL_XVT_STATE_UNINITIALIZED;
}

/*
 * iwl_xvt_continue_init get phy calibrations data from
 * device and stores them. It also runs runtime FW if it
 * is marked in the load mask.
 */
static int iwl_xvt_continue_init(struct iwl_xvt *xvt)
{
	struct iwl_notification_wait calib_wait;
	static const u8 init_complete[] = {
		INIT_COMPLETE_NOTIF,
		CALIB_RES_NOTIF_PHY_DB
	};
	int err;

	if (xvt->state != IWL_XVT_STATE_INIT_STARTED)
		return -EINVAL;

	iwl_init_notification_wait(&xvt->notif_wait,
				   &calib_wait,
				   init_complete,
				   ARRAY_SIZE(init_complete),
				   iwl_xvt_wait_phy_db_entry,
				   xvt->phy_db);

	err = iwl_xvt_send_phy_cfg_cmd(xvt, IWL_UCODE_INIT);
	if (err) {
		iwl_remove_notification(&xvt->notif_wait, &calib_wait);
		goto error;
	}

	/*
	 * Waiting for the calibration complete notification
	 * iwl_xvt_wait_phy_db_entry will store the calibrations
	 */
	err = iwl_wait_notification(&xvt->notif_wait, &calib_wait,
				    XVT_UCODE_CALIB_TIMEOUT);
	if (err)
		goto error;

	xvt->state = IWL_XVT_STATE_OPERATIONAL;

	if (xvt->sw_stack_cfg.load_mask & IWL_XVT_LOAD_MASK_RUNTIME)
		/* Run runtime FW stops the device by itself if error occurs */
		err = iwl_xvt_run_runtime_fw(xvt);

	goto cont_init_end;

error:
	xvt->state = IWL_XVT_STATE_UNINITIALIZED;
	iwl_trans_txq_disable(xvt->trans, IWL_XVT_DEFAULT_TX_QUEUE, true);
	iwl_trans_stop_device(xvt->trans);

cont_init_end:

	return err;
}

static int iwl_xvt_get_phy_db(struct iwl_xvt *xvt,
			      struct iwl_tm_data *data_in,
			      struct iwl_tm_data *data_out)
{
	struct iwl_xvt_phy_db_request *phy_db_req =
				(struct iwl_xvt_phy_db_request *)data_in->data;
	struct iwl_xvt_phy_db_request *phy_db_resp;
	u8 *phy_data;
	u16 phy_size;
	u32 resp_size;
	int err;

	if ((data_in->len < sizeof(struct iwl_xvt_phy_db_request)) ||
	    (phy_db_req->size != 0))
		return -EINVAL;

	err = iwl_phy_db_get_section_data(xvt->phy_db,
					  phy_db_req->type,
					  &phy_data, &phy_size,
					  phy_db_req->chg_id);
	if (err)
		return err;

	resp_size = sizeof(*phy_db_resp) + phy_size;
	phy_db_resp = kzalloc(resp_size, GFP_KERNEL);
	if (!phy_db_resp)
		return -ENOMEM;
	phy_db_resp->chg_id = phy_db_req->chg_id;
	phy_db_resp->type = phy_db_req->type;
	phy_db_resp->size = phy_size;
	memcpy(phy_db_resp->data, phy_data, phy_size);

	data_out->data = phy_db_resp;
	data_out->len = resp_size;

	return 0;
}

/*
 * Allocates and sets the Tx cmd the driver data pointers in the skb
 */
static struct iwl_device_cmd *
iwl_xvt_set_mod_tx_params(struct iwl_xvt *xvt, struct sk_buff *skb,
			  u8 sta_id, u32 rate_flags)
{
	struct iwl_device_cmd *dev_cmd, **cb_dev_cmd = (void *)skb->cb;
	struct iwl_tx_cmd *tx_cmd;

	dev_cmd = iwl_trans_alloc_tx_cmd(xvt->trans);
	if (unlikely(!dev_cmd))
		return NULL;

	memset(dev_cmd, 0, sizeof(*dev_cmd));
	dev_cmd->hdr.cmd = TX_CMD;
	tx_cmd = (struct iwl_tx_cmd *)dev_cmd->payload;

	tx_cmd->len = cpu_to_le16((u16)skb->len);
	tx_cmd->life_time = cpu_to_le32(TX_CMD_LIFE_TIME_INFINITE);

	tx_cmd->sta_id = sta_id;
	tx_cmd->rate_n_flags = cpu_to_le32(rate_flags);
	tx_cmd->tx_flags = TX_CMD_FLG_ACK_MSK;

	/* the skb should already hold the data */
	memcpy(tx_cmd->hdr, skb->data, sizeof(struct ieee80211_hdr));

	/*
	 * Saving device command address itself in the
	 * control buffer, to be used when reclaiming
	 * the command.
	 */
	*cb_dev_cmd = dev_cmd;

	return dev_cmd;
}

static int iwl_xvt_modulated_tx(struct iwl_xvt *xvt,
				struct iwl_tm_data *data_in)
{
	struct iwl_tm_mod_tx_request *tx_req =
			(struct iwl_tm_mod_tx_request *)data_in->data;
	struct sk_buff *skb;
	struct iwl_device_cmd *dev_cmd;
	u32 tx_count;
	int time_remain, err = 0;

	xvt->tot_tx = tx_req->times;
	xvt->tx_counter = 0;
	for (tx_count = 0; tx_count < tx_req->times; tx_count++) {

		if (xvt->fw_error) {
			IWL_ERR(xvt, "FW Error while sending Tx\n");
			return -ENODEV;
		}

		skb = alloc_skb(tx_req->len, GFP_KERNEL);
		if (!skb) {
			return -ENOMEM;
		}
		memcpy(skb_put(skb, tx_req->len), tx_req->data, tx_req->len);

		dev_cmd = iwl_xvt_set_mod_tx_params(xvt, skb,
						    tx_req->sta_id,
						    tx_req->rate_flags);
		if (!dev_cmd) {
			kfree_skb(skb);
			return -ENOMEM;
		}

		if (tx_req->trigger_led)
			iwl_xvt_led_enable(xvt);

		/* wait until the tx queue isn't full */
		time_remain = wait_event_interruptible_timeout(xvt->mod_tx_wq,
							!xvt->txq_full, HZ);

		if (time_remain <= 0) {
			/* This should really not happen */
			WARN_ON_ONCE(xvt->txq_full);
			IWL_ERR(xvt, "Error while sending Tx\n");
			iwl_trans_free_tx_cmd(xvt->trans, dev_cmd);
			kfree_skb(skb);
			return -EIO;
		}

		if (xvt->fw_error) {
			WARN_ON_ONCE(xvt->txq_full);
			IWL_ERR(xvt, "FW Error while sending Tx\n");
			iwl_trans_free_tx_cmd(xvt->trans, dev_cmd);
			kfree_skb(skb);
			return -ENODEV;
		}

		/*
		 * Assume we have one Txing thread only: the queue is not full
		 * any more - nobody could fill it up in the meantime since we
		 * were blocked.
		 */

		local_bh_disable();

		err = iwl_trans_tx(xvt->trans, skb, dev_cmd,
				   IWL_XVT_DEFAULT_TX_QUEUE);

		local_bh_enable();
		if (err) {
			IWL_ERR(xvt, "Tx command failed (error %d)\n", err);
			kfree_skb(skb);
			iwl_trans_free_tx_cmd(xvt->trans, dev_cmd);
			return err;
		}

		if (tx_req->trigger_led)
			iwl_xvt_led_disable(xvt);

		if (tx_req->delay_us)
			udelay(tx_req->delay_us);
	}

	time_remain = wait_event_interruptible_timeout(xvt->mod_tx_done_wq,
					 xvt->tx_counter == xvt->tot_tx,
					 5 * HZ);
	if (time_remain <= 0) {
		IWL_ERR(xvt, "Not all Tx messages were sent\n");
		return -EIO;
	}

	return err;
}

static int iwl_xvt_rx_hdrs_mode(struct iwl_xvt *xvt,
				  struct iwl_tm_data *data_in)
{
	struct iwl_xvt_rx_hdrs_mode_request *rx_hdr = data_in->data;

	if (data_in->len < sizeof(struct iwl_xvt_rx_hdrs_mode_request))
		return -EINVAL;

	if (rx_hdr->mode)
		xvt->rx_hdr_enabled = true;
	else
		xvt->rx_hdr_enabled = false;

	return 0;
}

static int iwl_xvt_apmg_pd_mode(struct iwl_xvt *xvt,
				  struct iwl_tm_data *data_in)
{
	struct iwl_xvt_apmg_pd_mode_request *apmg_pd = data_in->data;

	if (apmg_pd->mode)
		xvt->apmg_pd_en = true;
	else
		xvt->apmg_pd_en = false;

	return 0;
}

static int iwl_xvt_allocate_dma(struct iwl_xvt *xvt,
				struct iwl_tm_data *data_in,
				struct iwl_tm_data *data_out)
{
	struct iwl_xvt_alloc_dma *dma_req = data_in->data;
	struct iwl_xvt_alloc_dma *dma_res;

	if (data_in->len < sizeof(struct iwl_xvt_alloc_dma))
		return -EINVAL;

	if (xvt->dma_cpu_addr) {
		IWL_ERR(xvt, "XVT DMA already allocated\n");
		return -EBUSY;
	}

	xvt->dma_cpu_addr = dma_alloc_coherent(xvt->trans->dev, dma_req->size,
					       &(xvt->dma_addr), GFP_KERNEL);

	if (!xvt->dma_cpu_addr) {
		return false;
	}

	dma_res = kmalloc(sizeof(*dma_res), GFP_KERNEL);
	if (!dma_res) {
		dma_free_coherent(xvt->trans->dev, dma_req->size,
				  xvt->dma_cpu_addr, xvt->dma_addr);
		xvt->dma_cpu_addr = NULL;
		xvt->dma_addr = 0;
		return -ENOMEM;
	}
	dma_res->size = dma_req->size;
	/* Casting to avoid compilation warnings when DMA address is 32bit */
	dma_res->addr = (u64)xvt->dma_addr;

	data_out->data = dma_res;
	data_out->len = sizeof(struct iwl_xvt_alloc_dma);
	xvt->dma_buffer_size = dma_req->size;

	return 0;
}

static int iwl_xvt_get_dma(struct iwl_xvt *xvt,
			   struct iwl_tm_data *data_in,
			   struct iwl_tm_data *data_out)
{
	struct iwl_xvt_get_dma *get_dma_resp;
	u32 resp_size;

	if (!xvt->dma_cpu_addr) {
		return -ENOMEM;
	}

	resp_size = sizeof(*get_dma_resp) + xvt->dma_buffer_size;
	get_dma_resp = kmalloc(resp_size, GFP_KERNEL);
	if (!get_dma_resp) {
		return -ENOMEM;
	}

	get_dma_resp->size = xvt->dma_buffer_size;
	memcpy(get_dma_resp->data, xvt->dma_cpu_addr, xvt->dma_buffer_size);
	data_out->data = get_dma_resp;
	data_out->len = resp_size;

	return 0;
}

static int iwl_xvt_free_dma(struct iwl_xvt *xvt,
			    struct iwl_tm_data *data_in)
{

	if (!xvt->dma_cpu_addr) {
		IWL_ERR(xvt, "XVT DMA was not allocated\n");
		return 0;
	}

	dma_free_coherent(xvt->trans->dev, xvt->dma_buffer_size,
			  xvt->dma_cpu_addr, xvt->dma_addr);
	xvt->dma_cpu_addr = NULL;
	xvt->dma_addr = 0;
	xvt->dma_buffer_size = 0;

	return 0;
}

static int iwl_xvt_get_chip_id(struct iwl_xvt *xvt,
			       struct iwl_tm_data *data_out)
{
	struct iwl_xvt_chip_id *chip_id;

	chip_id = kmalloc(sizeof(struct iwl_xvt_chip_id), GFP_KERNEL);
	if (!chip_id)
		return -ENOMEM;

	chip_id->registers[0] = ioread32((void __iomem *)XVT_SCU_SNUM1);
	chip_id->registers[1] = ioread32((void __iomem *)XVT_SCU_SNUM2);
	chip_id->registers[2] = ioread32((void __iomem *)XVT_SCU_SNUM3);


	data_out->data = chip_id;
	data_out->len = sizeof(struct iwl_xvt_chip_id);

	return 0;
}

int iwl_xvt_user_cmd_execute(struct iwl_op_mode *op_mode, u32 cmd,
			     struct iwl_tm_data *data_in,
			     struct iwl_tm_data *data_out)
{
	struct iwl_xvt *xvt = IWL_OP_MODE_GET_XVT(op_mode);
	int ret = 0;

	if (WARN_ON_ONCE(!op_mode || !data_in))
		return -EINVAL;

	IWL_DEBUG_INFO(xvt, "%s cmd=0x%X\n", __func__, cmd);
	mutex_lock(&xvt->mutex);

	switch (cmd) {

	/* Tesmode cases */

	case IWL_TM_USER_CMD_HCMD:
		ret = iwl_xvt_send_hcmd(xvt, data_in, data_out);
		break;

	case IWL_TM_USER_CMD_REG_ACCESS:
		ret = iwl_xvt_reg_ops(xvt->trans, data_in, data_out);
		break;

	case IWL_TM_USER_CMD_SRAM_WRITE:
		ret = iwl_xvt_indirect_write(xvt, data_in);
		break;

	case IWL_TM_USER_CMD_SRAM_READ:
		ret = iwl_xvt_indirect_read(xvt, data_in, data_out);
		break;

	case IWL_TM_USER_CMD_GET_DEVICE_INFO:
		ret = iwl_xvt_get_dev_info(xvt, data_in, data_out);
		break;

	case IWL_TM_USER_CMD_SV_IO_TOGGLE:
		ret = iwl_xvt_sdio_io_toggle(xvt, data_in, data_out);
		break;

	/* xVT cases */

	case IWL_XVT_CMD_START:
		ret = iwl_xvt_start_op_mode(xvt);
		break;

	case IWL_XVT_CMD_STOP:
		iwl_xvt_stop_op_mode(xvt);
		break;

	case IWL_XVT_CMD_CONTINUE_INIT:
		ret = iwl_xvt_continue_init(xvt);
		break;

	case IWL_XVT_CMD_GET_PHY_DB_ENTRY:
		ret = iwl_xvt_get_phy_db(xvt, data_in, data_out);
		break;

	case IWL_XVT_CMD_SET_CONFIG:
		ret = iwl_xvt_set_sw_config(xvt, data_in);
		break;

	case IWL_XVT_CMD_GET_CONFIG:
		ret = iwl_xvt_get_sw_config(xvt, data_in, data_out);
		break;

	case IWL_XVT_CMD_MOD_TX:
		ret = iwl_xvt_modulated_tx(xvt, data_in);
		break;

	case IWL_XVT_CMD_RX_HDRS_MODE:
		ret = iwl_xvt_rx_hdrs_mode(xvt, data_in);
		break;

	case IWL_XVT_CMD_APMG_PD_MODE:
		ret = iwl_xvt_apmg_pd_mode(xvt, data_in);
		break;

	case IWL_XVT_CMD_ALLOC_DMA:
		ret = iwl_xvt_allocate_dma(xvt, data_in, data_out);
		break;

	case IWL_XVT_CMD_GET_DMA:
		ret = iwl_xvt_get_dma(xvt, data_in, data_out);
		break;

	case IWL_XVT_CMD_FREE_DMA:
		ret = iwl_xvt_free_dma(xvt, data_in);
		break;
	case IWL_XVT_CMD_GET_CHIP_ID:
		ret = iwl_xvt_get_chip_id(xvt, data_out);
		break;
	default:
		ret = -EOPNOTSUPP;
		break;
	}

	mutex_unlock(&xvt->mutex);

	if (ret)
		IWL_ERR(xvt, "%s ret=%d\n", __func__, ret);
	else
		IWL_DEBUG_INFO(xvt, "%s ended Ok\n", __func__);
	return ret;
}
