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
#include "iwl-trans.h"
#include "iwl-op-mode.h"
#include "iwl-fw.h"
#include "iwl-csr.h"

#include "xvt.h"
#include "iwl-dnt-cfg.h"

#define XVT_UCODE_ALIVE_TIMEOUT	HZ

struct iwl_xvt_alive_data {
	bool valid;
	u32 scd_base_addr;
};

static inline const struct fw_img *
iwl_get_ucode_image(struct iwl_xvt *xvt, enum iwl_ucode_type ucode_type)
{
	if (ucode_type >= IWL_UCODE_TYPE_MAX)
		return NULL;

	return &xvt->fw->img[ucode_type];
}

static bool iwl_alive_fn(struct iwl_notif_wait_data *notif_wait,
			 struct iwl_rx_packet *pkt,
			 void *data)
{
	struct iwl_xvt *xvt =
		container_of(notif_wait, struct iwl_xvt, notif_wait);
	struct iwl_xvt_alive_data *alive_data = data;
	struct xvt_alive_resp *palive;
	struct xvt_alive_resp_ver2 *palive2;
	struct xvt_alive_resp_ver3 *palive3;

	if (iwl_rx_packet_payload_len(pkt) == sizeof(*palive)) {
		palive = (void *)pkt->data;
		xvt->error_event_table = le32_to_cpu(
						palive->error_event_table_ptr);
		alive_data->scd_base_addr = le32_to_cpu(
						palive->scd_base_ptr);
		alive_data->valid = le16_to_cpu(palive->status) ==
							IWL_ALIVE_STATUS_OK;

		IWL_DEBUG_FW(xvt, "Alive ucode status 0x%04x revision 0x%01X "
			     "0x%01X\n", le16_to_cpu(palive->status),
			     palive->ver_type, palive->ver_subtype);
	} else if (iwl_rx_packet_payload_len(pkt) == sizeof(*palive2)) {

		palive2 = (void *)pkt->data;

		xvt->error_event_table =
			le32_to_cpu(palive2->error_event_table_ptr);
		alive_data->scd_base_addr = le32_to_cpu(palive2->scd_base_ptr);
		xvt->sf_space.addr = le32_to_cpu(palive2->st_fwrd_addr);
		xvt->sf_space.size = le32_to_cpu(palive2->st_fwrd_size);

		alive_data->valid = le16_to_cpu(palive2->status) ==
				    IWL_ALIVE_STATUS_OK;

		IWL_DEBUG_FW(xvt,
			     "Alive VER2 ucode status 0x%04x revision 0x%01X "
			     "0x%01X flags 0x%01X\n",
			     le16_to_cpu(palive2->status), palive2->ver_type,
			     palive2->ver_subtype, palive2->flags);

		IWL_DEBUG_FW(xvt,
			     "UMAC version: Major - 0x%x, Minor - 0x%x\n",
			     palive2->umac_major, palive2->umac_minor);
	} else {
		palive3 = (void *)pkt->data;

		xvt->error_event_table =
			le32_to_cpu(palive3->error_event_table_ptr);
		alive_data->scd_base_addr = le32_to_cpu(palive3->scd_base_ptr);
		xvt->sf_space.addr = le32_to_cpu(palive3->st_fwrd_addr);
		xvt->sf_space.size = le32_to_cpu(palive3->st_fwrd_size);

		alive_data->valid = le16_to_cpu(palive3->status) ==
				    IWL_ALIVE_STATUS_OK;

		IWL_DEBUG_FW(xvt,
			     "Alive VER3 ucode status 0x%04x revision 0x%01X 0x%01X flags 0x%01X\n",
			     le16_to_cpu(palive3->status), palive3->ver_type,
			     palive3->ver_subtype, palive3->flags);

		IWL_DEBUG_FW(xvt,
			     "UMAC version: Major - 0x%x, Minor - 0x%x\n",
			     palive3->umac_major, palive3->umac_minor);
	}

	return true;
}

static int iwl_xvt_load_ucode_wait_alive(struct iwl_xvt *xvt,
					 enum iwl_ucode_type ucode_type)
{
	struct iwl_notification_wait alive_wait;
	struct iwl_xvt_alive_data alive_data;
	const struct fw_img *fw;
	int ret;
	enum iwl_ucode_type old_type = xvt->cur_ucode;
	static const u8 alive_cmd[] = { XVT_ALIVE };

	xvt->cur_ucode = ucode_type;
	fw = iwl_get_ucode_image(xvt, ucode_type);

	if (!fw)
		return -EINVAL;

	iwl_init_notification_wait(&xvt->notif_wait, &alive_wait,
				   alive_cmd, ARRAY_SIZE(alive_cmd),
				   iwl_alive_fn, &alive_data);

	ret = iwl_trans_start_fw_dbg(xvt->trans, fw,
				     ucode_type == IWL_UCODE_INIT,
				     xvt->sw_stack_cfg.fw_dbg_flags);
	if (ret) {
		xvt->cur_ucode = old_type;
		iwl_remove_notification(&xvt->notif_wait, &alive_wait);
		return ret;
	}

	/*
	 * Some things may run in the background now, but we
	 * just wait for the ALIVE notification here.
	 */
	ret = iwl_wait_notification(&xvt->notif_wait, &alive_wait,
				    XVT_UCODE_ALIVE_TIMEOUT);
	if (ret) {
		xvt->cur_ucode = old_type;
		return ret;
	}

	if (!alive_data.valid) {
		IWL_ERR(xvt, "Loaded ucode is not valid!\n");
		xvt->cur_ucode = old_type;
		return -EIO;
	}

	/* fresh firmware was loaded */
	xvt->fw_error = false;

	/*
	 * update the sdio allocation according to the pointer we get in the
	 * alive notification.
	 */
	ret = iwl_trans_update_sf(xvt->trans, &xvt->sf_space);

	iwl_trans_fw_alive(xvt->trans, alive_data.scd_base_addr);

	if (ucode_type == IWL_UCODE_REGULAR)
		iwl_trans_ac_txq_enable(xvt->trans,
					IWL_XVT_DEFAULT_TX_QUEUE,
					IWL_XVT_DEFAULT_TX_FIFO, 0);

	xvt->fw_running = true;

	return 0;
}

int iwl_xvt_run_fw(struct iwl_xvt *xvt, u32 ucode_type)
{
	int ret;

	if (ucode_type >= IWL_UCODE_TYPE_MAX)
		return -EINVAL;

	lockdep_assert_held(&xvt->mutex);

	if (xvt->state != IWL_XVT_STATE_UNINITIALIZED) {
		if (xvt->fw_running) {
			xvt->fw_running = false;
			if (xvt->cur_ucode == IWL_UCODE_REGULAR)
				iwl_trans_txq_disable(xvt->trans,
						      IWL_XVT_DEFAULT_TX_QUEUE,
						      true);
		}
		iwl_trans_stop_device(xvt->trans);
	}

	ret = iwl_trans_start_hw(xvt->trans);
	if (ret) {
		IWL_ERR(xvt, "Failed to start HW\n");
		return ret;
	}

	iwl_trans_set_bits_mask(xvt->trans,
				CSR_HW_IF_CONFIG_REG,
				CSR_HW_IF_CONFIG_REG_BIT_MAC_SI,
				CSR_HW_IF_CONFIG_REG_BIT_MAC_SI);

	/* Will also start the device */
	ret = iwl_xvt_load_ucode_wait_alive(xvt, ucode_type);
	if (ret) {
		IWL_ERR(xvt, "Failed to start ucode: %d\n", ret);
		iwl_trans_stop_device(xvt->trans);
	}
	iwl_dnt_start(xvt->trans);

	return ret;
}
