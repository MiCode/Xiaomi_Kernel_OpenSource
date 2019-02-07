/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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

#include "ipa_i.h"
#include <linux/if_ether.h>
#include <linux/log2.h>
#include <linux/ipa_wigig.h>

#define IPA_WIGIG_DESC_RING_EL_SIZE	32
#define IPA_WIGIG_STATUS_RING_EL_SIZE	16

#define GSI_STOP_MAX_RETRY_CNT 10

#define IPA_WIGIG_CONNECTED BIT(0)
#define IPA_WIGIG_ENABLED BIT(1)
#define IPA_WIGIG_MSB_MASK 0xFFFFFFFF00000000
#define IPA_WIGIG_LSB_MASK 0x00000000FFFFFFFF
#define IPA_WIGIG_MSB(num) ((u32)((num & IPA_WIGIG_MSB_MASK) >> 32))
#define IPA_WIGIG_LSB(num) ((u32)(num & IPA_WIGIG_LSB_MASK))
/* extract PCIE addresses [0:39] relevant msb */
#define IPA_WIGIG_8_MSB_MASK 0xFF00000000
#define IPA_WIGIG_8_MSB(num) ((u32)((num & IPA_WIGIG_8_MSB_MASK) >> 32))
#define W11AD_RX 0
#define W11AD_TX 1
#define W11AD_TO_GSI_DB_m 1
#define W11AD_TO_GSI_DB_n 1


static int ipa3_wigig_uc_loaded_handler(struct notifier_block *self,
	unsigned long val, void *data)
{
	IPADBG("val %d\n", val);

	if (!ipa3_ctx) {
		IPAERR("IPA ctx is null\n");
		return -EINVAL;
	}

	WARN_ON(data != ipa3_ctx);

	if (ipa3_ctx->uc_wigig_ctx.uc_ready_cb) {
		ipa3_ctx->uc_wigig_ctx.uc_ready_cb(
			ipa3_ctx->uc_wigig_ctx.priv);

		ipa3_ctx->uc_wigig_ctx.uc_ready_cb =
			NULL;
		ipa3_ctx->uc_wigig_ctx.priv = NULL;
	}

	IPADBG("exit\n");
	return 0;
}

static struct notifier_block uc_loaded_notifier = {
	.notifier_call = ipa3_wigig_uc_loaded_handler,
};

int ipa3_wigig_init_i(void)
{
	IPADBG("\n");

	ipa3_uc_register_ready_cb(&uc_loaded_notifier);

	IPADBG("exit\n");

	return 0;
}

int ipa3_wigig_uc_init(
	struct ipa_wdi_uc_ready_params *inout,
	ipa_wigig_misc_int_cb int_notify,
	phys_addr_t *uc_db_pa)
{
	int result = 0;

	IPADBG("\n");

	if (inout == NULL) {
		IPAERR("inout is NULL");
		return -EINVAL;
	}

	if (int_notify == NULL) {
		IPAERR("int_notify is NULL");
		return -EINVAL;
	}

	result = ipa3_uc_state_check();
	if (result) {
		inout->is_uC_ready = false;
		ipa3_ctx->uc_wigig_ctx.uc_ready_cb = inout->notify;
	} else {
		inout->is_uC_ready = true;
	}
	ipa3_ctx->uc_wigig_ctx.priv = inout->priv;
	ipa3_ctx->uc_wigig_ctx.misc_notify_cb = int_notify;

	*uc_db_pa = ipa3_ctx->ipa_wrapper_base +
		ipahal_get_reg_base() +
		ipahal_get_reg_mn_ofst(
			IPA_UC_MAILBOX_m_n,
			W11AD_TO_GSI_DB_m,
			W11AD_TO_GSI_DB_n);

	IPADBG("exit\n");

	return 0;
}

static int ipa3_wigig_tx_bit_to_ep(
	const u8 tx_bit_num,
	enum ipa_client_type *type)
{
	IPADBG("tx_bit_num %d\n", tx_bit_num);

	switch (tx_bit_num) {
	case 2:
		*type = IPA_CLIENT_WIGIG1_CONS;
		break;
	case 3:
		*type = IPA_CLIENT_WIGIG2_CONS;
		break;
	case 4:
		*type = IPA_CLIENT_WIGIG3_CONS;
		break;
	case 5:
		*type = IPA_CLIENT_WIGIG4_CONS;
		break;
	default:
		IPAERR("invalid tx_bit_num %d\n", tx_bit_num);
		return -EINVAL;
	}

	IPADBG("exit\n");
	return 0;
}

static int ipa3_wigig_smmu_map_channel(bool Rx,
	struct ipa_wigig_pipe_setup_info_smmu *pipe_smmu,
	void *buff,
	bool map)
{
	int result = 0;

	IPADBG("\n");

	/*
	 * --------------------------------------------------------------------
	 *  entity         |HWHEAD|HWTAIL|HWHEAD|HWTAIL| misc | buffers| rings|
	 *                 |Sring |Sring |Dring |Dring | regs |        |      |
	 * --------------------------------------------------------------------
	 *  GSI (apps CB)  |  TX  |RX, TX|      |RX, TX|      |        |Rx, TX|
	 * --------------------------------------------------------------------
	 *  IPA (WLAN CB)  |      |      |      |      |      | RX, TX |      |
	 * --------------------------------------------------------------------
	 *  uc (uC CB)     |  RX  |      |  TX  |      |always|        |      |
	 * --------------------------------------------------------------------
	 */

	if (Rx) {
		result = ipa3_smmu_map_peer_reg(
			pipe_smmu->status_ring_HWHEAD_pa,
			map,
			IPA_SMMU_CB_UC);
		if (result) {
			IPAERR(
				"failed to %s status_ring_HWAHEAD %d\n",
				map ? "map" : "unmap",
				result);
			goto fail_status_HWHEAD;
		}
	} else {

		result = ipa3_smmu_map_peer_reg(
			pipe_smmu->status_ring_HWHEAD_pa,
			map,
			IPA_SMMU_CB_AP);
		if (result) {
			IPAERR(
				"failed to %s status_ring_HWAHEAD %d\n",
				map ? "map" : "unmap",
				result);
			goto fail_status_HWHEAD;
		}

		result = ipa3_smmu_map_peer_reg(
			pipe_smmu->desc_ring_HWHEAD_pa,
			map,
			IPA_SMMU_CB_UC);
		if (result) {
			IPAERR("failed to %s desc_ring_HWHEAD %d\n",
				map ? "map" : "unmap",
				result);
			goto fail;
		}
	}

	result = ipa3_smmu_map_peer_reg(
		pipe_smmu->status_ring_HWTAIL_pa,
		map,
		IPA_SMMU_CB_AP);
	if (result) {
		IPAERR(
			"failed to %s status_ring_HWTAIL %d\n",
			map ? "map" : "unmap",
			result);
		goto fail_status_HWTAIL;
	}

	result = ipa3_smmu_map_peer_reg(
		pipe_smmu->desc_ring_HWTAIL_pa,
		map,
		IPA_SMMU_CB_AP);
	if (result) {
		IPAERR("failed to %s desc_ring_HWTAIL %d\n",
			map ? "map" : "unmap",
			result);
		goto fail_desc_HWTAIL;
	}

	result = ipa3_smmu_map_peer_buff(
		pipe_smmu->desc_ring_base_iova,
		pipe_smmu->desc_ring_size,
		map,
		&pipe_smmu->desc_ring_base,
		IPA_SMMU_CB_AP);
	if (result) {
		IPAERR("failed to %s desc_ring_base %d\n",
			map ? "map" : "unmap",
			result);
		goto fail_desc_ring;
	}

	result = ipa3_smmu_map_peer_buff(
		pipe_smmu->status_ring_base_iova,
		pipe_smmu->status_ring_size,
		map,
		&pipe_smmu->status_ring_base,
		IPA_SMMU_CB_AP);
	if (result) {
		IPAERR("failed to %s status_ring_base %d\n",
			map ? "map" : "unmap",
			result);
		goto fail_status_ring;
	}

	if (Rx) {
		struct ipa_wigig_rx_pipe_data_buffer_info_smmu *dbuff_smmu =
			(struct ipa_wigig_rx_pipe_data_buffer_info_smmu *)buff;

		int num_elem =
			pipe_smmu->desc_ring_size /
			IPA_WIGIG_DESC_RING_EL_SIZE;

		result = ipa3_smmu_map_peer_buff(
			dbuff_smmu->data_buffer_base_iova,
			dbuff_smmu->data_buffer_size * num_elem,
			map,
			&dbuff_smmu->data_buffer_base,
			IPA_SMMU_CB_WLAN);
		if (result) {
			IPAERR(
				"failed to %s rx data_buffer %d, num elem %d\n"
				, map ? "map" : "unmap",
				result, num_elem);
			goto fail_map_buff;
		}

	} else {
		int i;
		struct ipa_wigig_tx_pipe_data_buffer_info_smmu *dbuff_smmu =
			(struct ipa_wigig_tx_pipe_data_buffer_info_smmu *)buff;

		for (i = 0; i < dbuff_smmu->num_buffers; i++) {
			result = ipa3_smmu_map_peer_buff(
				*(dbuff_smmu->data_buffer_base_iova + i),
				dbuff_smmu->data_buffer_size,
				map,
				(dbuff_smmu->data_buffer_base + i),
				IPA_SMMU_CB_WLAN);
			if (result) {
				IPAERR(
					"%d: failed to %s tx data buffer %d\n"
					, i, map ? "map" : "unmap",
					result);
				for (i--; i >= 0; i--) {
					result = ipa3_smmu_map_peer_buff(
					*(dbuff_smmu->data_buffer_base_iova +
						i),
					dbuff_smmu->data_buffer_size,
					!map,
					(dbuff_smmu->data_buffer_base +
						i),
					IPA_SMMU_CB_WLAN);
				}
				goto fail_map_buff;
			}
		}
	}

	IPADBG("exit\n");

	return 0;
fail_map_buff:
	result = ipa3_smmu_map_peer_buff(
		pipe_smmu->status_ring_base_iova, pipe_smmu->status_ring_size,
		!map, &pipe_smmu->status_ring_base,
		IPA_SMMU_CB_AP);
fail_status_ring:
	ipa3_smmu_map_peer_buff(
		pipe_smmu->desc_ring_base_iova, pipe_smmu->desc_ring_size,
		!map, &pipe_smmu->desc_ring_base,
		IPA_SMMU_CB_AP);
fail_desc_ring:
	ipa3_smmu_map_peer_reg(
		pipe_smmu->status_ring_HWTAIL_pa, !map, IPA_SMMU_CB_AP);
fail_status_HWTAIL:
	if (Rx)
		ipa3_smmu_map_peer_reg(pipe_smmu->status_ring_HWHEAD_pa,
			!map, IPA_SMMU_CB_UC);
fail_status_HWHEAD:
	ipa3_smmu_map_peer_reg(
		pipe_smmu->desc_ring_HWTAIL_pa, !map, IPA_SMMU_CB_AP);
fail_desc_HWTAIL:
	ipa3_smmu_map_peer_reg(
		pipe_smmu->desc_ring_HWHEAD_pa, !map, IPA_SMMU_CB_UC);
fail:
	return result;
}

static void ipa_gsi_chan_err_cb(struct gsi_chan_err_notify *notify)
{
	switch (notify->evt_id) {
	case GSI_CHAN_INVALID_TRE_ERR:
		IPAERR("Got GSI_CHAN_INVALID_TRE_ERR\n");
		break;
	case GSI_CHAN_NON_ALLOCATED_EVT_ACCESS_ERR:
		IPAERR("Got GSI_CHAN_NON_ALLOCATED_EVT_ACCESS_ERR\n");
		break;
	case GSI_CHAN_OUT_OF_BUFFERS_ERR:
		IPAERR("Got GSI_CHAN_OUT_OF_BUFFERS_ERR\n");
		break;
	case GSI_CHAN_OUT_OF_RESOURCES_ERR:
		IPAERR("Got GSI_CHAN_OUT_OF_RESOURCES_ERR\n");
		break;
	case GSI_CHAN_UNSUPPORTED_INTER_EE_OP_ERR:
		IPAERR("Got GSI_CHAN_UNSUPPORTED_INTER_EE_OP_ERR\n");
		break;
	case GSI_CHAN_HWO_1_ERR:
		IPAERR("Got GSI_CHAN_HWO_1_ERR\n");
		break;
	default:
		IPAERR("Unexpected err evt: %d\n", notify->evt_id);
	}
	ipa_assert();
}

static void ipa_gsi_evt_ring_err_cb(struct gsi_evt_err_notify *notify)
{
	switch (notify->evt_id) {
	case GSI_EVT_OUT_OF_BUFFERS_ERR:
		IPAERR("Got GSI_EVT_OUT_OF_BUFFERS_ERR\n");
		break;
	case GSI_EVT_OUT_OF_RESOURCES_ERR:
		IPAERR("Got GSI_EVT_OUT_OF_RESOURCES_ERR\n");
		break;
	case GSI_EVT_UNSUPPORTED_INTER_EE_OP_ERR:
		IPAERR("Got GSI_EVT_UNSUPPORTED_INTER_EE_OP_ERR\n");
		break;
	case GSI_EVT_EVT_RING_EMPTY_ERR:
		IPAERR("Got GSI_EVT_EVT_RING_EMPTY_ERR\n");
		break;
	default:
		IPAERR("Unexpected err evt: %d\n", notify->evt_id);
	}
	ipa_assert();
}

static int ipa3_wigig_config_gsi(bool Rx,
	bool smmu_en,
	void *pipe_info,
	void *buff,
	const struct ipa_gsi_ep_config *ep_gsi,
	struct ipa3_ep_context *ep)
{
	struct gsi_evt_ring_props evt_props;
	struct gsi_chan_props channel_props;
	union __packed gsi_channel_scratch gsi_scratch;
	int gsi_res;
	struct ipa_wigig_pipe_setup_info_smmu *pipe_smmu;
	struct ipa_wigig_pipe_setup_info *pipe;
	struct ipa_wigig_rx_pipe_data_buffer_info *rx_dbuff;
	struct ipa_wigig_rx_pipe_data_buffer_info_smmu *rx_dbuff_smmu;
	struct ipa_wigig_tx_pipe_data_buffer_info *tx_dbuff;
	struct ipa_wigig_tx_pipe_data_buffer_info_smmu *tx_dbuff_smmu;

	IPADBG("%s, %s\n", Rx ? "Rx" : "Tx", smmu_en ? "smmu en" : "smmu dis");

	/* alloc event ring */
	memset(&evt_props, 0, sizeof(evt_props));
	evt_props.intf = GSI_EVT_CHTYPE_11AD_EV;
	evt_props.re_size = GSI_EVT_RING_RE_SIZE_16B;
	evt_props.intr = GSI_INTR_MSI;
	evt_props.intvec = 0;
	evt_props.exclusive = true;
	evt_props.err_cb = ipa_gsi_evt_ring_err_cb;
	evt_props.user_data = NULL;
	evt_props.int_modc = 1;
	evt_props.int_modt = 1;
	evt_props.ring_base_vaddr = NULL;

	if (smmu_en) {
		pipe_smmu = (struct ipa_wigig_pipe_setup_info_smmu *)pipe_info;
		evt_props.ring_base_addr =
			pipe_smmu->desc_ring_base_iova;
		evt_props.ring_len = pipe_smmu->desc_ring_size;
		evt_props.msi_addr = pipe_smmu->desc_ring_HWTAIL_pa;
	} else {
		pipe = (struct ipa_wigig_pipe_setup_info *)pipe_info;
		evt_props.ring_base_addr = pipe->desc_ring_base_pa;
		evt_props.ring_len = pipe->desc_ring_size;
		evt_props.msi_addr = pipe->desc_ring_HWTAIL_pa;
	}

	gsi_res = gsi_alloc_evt_ring(&evt_props,
		ipa3_ctx->gsi_dev_hdl,
		&ep->gsi_evt_ring_hdl);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error allocating event ring: %d\n", gsi_res);
		return -EFAULT;
	}

	/* event scratch not configured by SW for TX channels */
	if (Rx) {
		union __packed gsi_evt_scratch evt_scratch;

		memset(&evt_scratch, 0, sizeof(evt_scratch));
		evt_scratch.w11ad.update_status_hwtail_mod_threshold = 1;
		gsi_res = gsi_write_evt_ring_scratch(ep->gsi_evt_ring_hdl,
			evt_scratch);
		if (gsi_res != GSI_STATUS_SUCCESS) {
			IPAERR("Error writing WIGIG event ring scratch: %d\n",
				gsi_res);
			goto fail_write_evt_scratch;
		}
	}

	ep->gsi_mem_info.evt_ring_len = evt_props.ring_len;
	ep->gsi_mem_info.evt_ring_base_addr = evt_props.ring_base_addr;
	ep->gsi_mem_info.evt_ring_base_vaddr = evt_props.ring_base_vaddr;

	/* alloc channel ring */
	memset(&channel_props, 0, sizeof(channel_props));
	memset(&gsi_scratch, 0, sizeof(gsi_scratch));

	if (Rx)
		channel_props.dir = GSI_CHAN_DIR_TO_GSI;
	else
		channel_props.dir = GSI_CHAN_DIR_FROM_GSI;

	channel_props.re_size = GSI_CHAN_RE_SIZE_16B;
	channel_props.prot = GSI_CHAN_PROT_11AD;
	channel_props.ch_id = ep_gsi->ipa_gsi_chan_num;
	channel_props.evt_ring_hdl = ep->gsi_evt_ring_hdl;
	channel_props.xfer_cb = NULL;

	channel_props.use_db_eng = GSI_CHAN_DB_MODE;
	channel_props.max_prefetch = GSI_ONE_PREFETCH_SEG;
	channel_props.prefetch_mode = ep_gsi->prefetch_mode;
	channel_props.low_weight = 1;
	channel_props.err_cb = ipa_gsi_chan_err_cb;

	channel_props.ring_base_vaddr = NULL;

	if (Rx) {
		if (smmu_en) {
			rx_dbuff_smmu =
			(struct ipa_wigig_rx_pipe_data_buffer_info_smmu *)buff;

			channel_props.ring_base_addr =
				pipe_smmu->status_ring_base_iova;
			channel_props.ring_len =
				pipe_smmu->status_ring_size;

			gsi_scratch.rx_11ad.status_ring_hwtail_address_lsb =
				IPA_WIGIG_LSB(
					pipe_smmu->status_ring_HWTAIL_pa);
			gsi_scratch.rx_11ad.status_ring_hwtail_address_msb =
				IPA_WIGIG_MSB(
					pipe_smmu->status_ring_HWTAIL_pa);

			gsi_scratch.rx_11ad.data_buffers_base_address_lsb =
				IPA_WIGIG_LSB(
					rx_dbuff_smmu->data_buffer_base_iova);
			gsi_scratch.rx_11ad.data_buffers_base_address_msb =
				IPA_WIGIG_MSB(
					rx_dbuff_smmu->data_buffer_base_iova);
			gsi_scratch.rx_11ad.fixed_data_buffer_size_pow_2 =
				ilog2(rx_dbuff_smmu->data_buffer_size);
		} else {
			rx_dbuff =
			(struct ipa_wigig_rx_pipe_data_buffer_info *)buff;

			channel_props.ring_base_addr =
				pipe->status_ring_base_pa;
			channel_props.ring_len = pipe->status_ring_size;

			gsi_scratch.rx_11ad.status_ring_hwtail_address_lsb =
				IPA_WIGIG_LSB(pipe->status_ring_HWTAIL_pa);
			gsi_scratch.rx_11ad.status_ring_hwtail_address_msb =
				IPA_WIGIG_MSB(pipe->status_ring_HWTAIL_pa);

			gsi_scratch.rx_11ad.data_buffers_base_address_lsb =
				IPA_WIGIG_LSB(rx_dbuff->data_buffer_base_pa);
			gsi_scratch.rx_11ad.data_buffers_base_address_msb =
				IPA_WIGIG_MSB(rx_dbuff->data_buffer_base_pa);
			gsi_scratch.rx_11ad.fixed_data_buffer_size_pow_2 =
				ilog2(rx_dbuff->data_buffer_size);
		}
		IPADBG("rx scratch: status_ring_hwtail_address_lsb 0x%X\n",
			gsi_scratch.rx_11ad.status_ring_hwtail_address_lsb);
		IPADBG("rx scratch: status_ring_hwtail_address_msb 0x%X\n",
			gsi_scratch.rx_11ad.status_ring_hwtail_address_msb);
		IPADBG("rx scratch: data_buffers_base_address_lsb 0x%X\n",
			gsi_scratch.rx_11ad.data_buffers_base_address_lsb);
		IPADBG("rx scratch: data_buffers_base_address_msb 0x%X\n",
			gsi_scratch.rx_11ad.data_buffers_base_address_msb);
		IPADBG("rx scratch: fixed_data_buffer_size_pow_2 %d\n",
			gsi_scratch.rx_11ad.fixed_data_buffer_size_pow_2);
		IPADBG("rx scratch 0x[%X][%X][%X][%X]\n",
			gsi_scratch.data.word1,
			gsi_scratch.data.word2,
			gsi_scratch.data.word3,
			gsi_scratch.data.word4);
	} else {
		if (smmu_en) {
			tx_dbuff_smmu =
			(struct ipa_wigig_tx_pipe_data_buffer_info_smmu *)buff;
			channel_props.ring_base_addr =
				pipe_smmu->desc_ring_base_iova;
			channel_props.ring_len =
				pipe_smmu->desc_ring_size;

			gsi_scratch.tx_11ad.status_ring_hwtail_address_lsb =
				IPA_WIGIG_LSB(
					pipe_smmu->status_ring_HWTAIL_pa);
			gsi_scratch.tx_11ad.status_ring_hwhead_address_lsb =
				IPA_WIGIG_LSB(
					pipe_smmu->status_ring_HWHEAD_pa);
			gsi_scratch.tx_11ad.status_ring_hwhead_hwtail_8_msb =
				IPA_WIGIG_8_MSB(
					pipe_smmu->status_ring_HWHEAD_pa);

			gsi_scratch.tx_11ad.fixed_data_buffer_size_pow_2 =
				ilog2(tx_dbuff_smmu->data_buffer_size);

			gsi_scratch.tx_11ad.status_ring_num_elem =
				pipe_smmu->status_ring_size /
				IPA_WIGIG_STATUS_RING_EL_SIZE;
		} else {
			tx_dbuff =
			(struct ipa_wigig_tx_pipe_data_buffer_info *)buff;

			channel_props.ring_base_addr = pipe->desc_ring_base_pa;
			channel_props.ring_len = pipe->desc_ring_size;

			gsi_scratch.tx_11ad.status_ring_hwtail_address_lsb =
				IPA_WIGIG_LSB(
					pipe->status_ring_HWTAIL_pa);
			gsi_scratch.tx_11ad.status_ring_hwhead_address_lsb =
				IPA_WIGIG_LSB(
					pipe->status_ring_HWHEAD_pa);
			gsi_scratch.tx_11ad.status_ring_hwhead_hwtail_8_msb =
				IPA_WIGIG_8_MSB(pipe->status_ring_HWHEAD_pa);

			gsi_scratch.tx_11ad.status_ring_num_elem =
				pipe->status_ring_size /
				IPA_WIGIG_STATUS_RING_EL_SIZE;

			gsi_scratch.tx_11ad.fixed_data_buffer_size_pow_2 =
				ilog2(tx_dbuff->data_buffer_size);
		}
		gsi_scratch.tx_11ad.update_status_hwtail_mod_threshold = 1;
		IPADBG("tx scratch: status_ring_hwtail_address_lsb 0x%X\n",
			gsi_scratch.tx_11ad.status_ring_hwtail_address_lsb);
		IPADBG("tx scratch: status_ring_hwhead_address_lsb 0x%X\n",
			gsi_scratch.tx_11ad.status_ring_hwhead_address_lsb);
		IPADBG("tx scratch: status_ring_hwhead_hwtail_8_msb 0x%X\n",
			gsi_scratch.tx_11ad.status_ring_hwhead_hwtail_8_msb);
		IPADBG("tx scratch:status_ring_num_elem %d\n",
			gsi_scratch.tx_11ad.status_ring_num_elem);
		IPADBG("tx scratch:fixed_data_buffer_size_pow_2 %d\n",
			gsi_scratch.tx_11ad.fixed_data_buffer_size_pow_2);
		IPADBG("tx scratch 0x[%X][%X][%X][%X]\n",
			gsi_scratch.data.word1,
			gsi_scratch.data.word2,
			gsi_scratch.data.word3,
			gsi_scratch.data.word4);
	}

	IPADBG("ch_id: %d\n", channel_props.ch_id);
	IPADBG("evt_ring_hdl: %ld\n", channel_props.evt_ring_hdl);
	IPADBG("re_size: %d\n", channel_props.re_size);
	IPADBG("GSI channel ring len: %d\n", channel_props.ring_len);
	IPADBG("channel ring  base addr = 0x%llX\n",
		(unsigned long long)channel_props.ring_base_addr);

	IPADBG("Allocating GSI channel\n");
	gsi_res = gsi_alloc_channel(&channel_props,
		ipa3_ctx->gsi_dev_hdl,
		&ep->gsi_chan_hdl);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("gsi_alloc_channel failed %d\n", gsi_res);
		goto fail_alloc_channel;
	}

	IPADBG("Writing Channel scratch\n");
	ep->gsi_mem_info.chan_ring_len = channel_props.ring_len;
	ep->gsi_mem_info.chan_ring_base_addr = channel_props.ring_base_addr;
	ep->gsi_mem_info.chan_ring_base_vaddr =
		channel_props.ring_base_vaddr;

	gsi_res = gsi_write_channel_scratch(ep->gsi_chan_hdl,
		gsi_scratch);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("gsi_write_channel_scratch failed %d\n",
			gsi_res);
		goto fail_write_channel_scratch;
	}

	IPADBG("exit\n");

	return 0;
fail_write_channel_scratch:
	gsi_dealloc_channel(ep->gsi_chan_hdl);
fail_alloc_channel:
fail_write_evt_scratch:
	gsi_dealloc_evt_ring(ep->gsi_evt_ring_hdl);
	return -EFAULT;
}

static int ipa3_wigig_config_uc(bool init,
	bool Rx,
	u8 wifi_ch,
	u8 gsi_ch,
	phys_addr_t HWHEAD)
{
	struct ipa_mem_buffer cmd;
	enum ipa_cpu_2_hw_offload_commands command;
	int result;

	IPADBG("%s\n", init ? "init" : "Deinit");
	if (init) {
		struct IpaHwOffloadSetUpCmdData_t_v4_0 *cmd_data;

		cmd.size = sizeof(*cmd_data);
		cmd.base = dma_alloc_coherent(ipa3_ctx->uc_pdev, cmd.size,
			&cmd.phys_base, GFP_KERNEL);
		if (cmd.base == NULL) {
			IPAERR("fail to get DMA memory.\n");
			return -ENOMEM;
		}

		cmd_data =
			(struct IpaHwOffloadSetUpCmdData_t_v4_0 *)cmd.base;

		cmd_data->protocol = IPA_HW_PROTOCOL_11ad;
		cmd_data->SetupCh_params.W11AdSetupCh_params.dir =
			Rx ? W11AD_RX : W11AD_TX;
		cmd_data->SetupCh_params.W11AdSetupCh_params.gsi_ch = gsi_ch;
		cmd_data->SetupCh_params.W11AdSetupCh_params.wifi_ch = wifi_ch;
		cmd_data->SetupCh_params.W11AdSetupCh_params.wifi_hp_addr_msb =
			IPA_WIGIG_MSB(HWHEAD);
		cmd_data->SetupCh_params.W11AdSetupCh_params.wifi_hp_addr_lsb =
			IPA_WIGIG_LSB(HWHEAD);
		command = IPA_CPU_2_HW_CMD_OFFLOAD_CHANNEL_SET_UP;

	} else {
		struct IpaHwOffloadCommonChCmdData_t_v4_0 *cmd_data;

		cmd.size = sizeof(*cmd_data);
		cmd.base = dma_alloc_coherent(ipa3_ctx->uc_pdev, cmd.size,
			&cmd.phys_base, GFP_KERNEL);
		if (cmd.base == NULL) {
			IPAERR("fail to get DMA memory.\n");
			return -ENOMEM;
		}

		cmd_data =
			(struct IpaHwOffloadCommonChCmdData_t_v4_0 *)cmd.base;

		cmd_data->protocol = IPA_HW_PROTOCOL_11ad;
		cmd_data->CommonCh_params.W11AdCommonCh_params.gsi_ch = gsi_ch;
		command = IPA_CPU_2_HW_CMD_OFFLOAD_TEAR_DOWN;
	}

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	result = ipa3_uc_send_cmd((u32)(cmd.phys_base),
		command,
		IPA_HW_2_CPU_OFFLOAD_CMD_STATUS_SUCCESS,
		false, 10 * HZ);
	if (result) {
		IPAERR("fail to %s uc for %s gsi channel %d\n",
			init ? "init" : "deinit",
			Rx ? "Rx" : "Tx", gsi_ch);
	}

	dma_free_coherent(ipa3_ctx->uc_pdev,
		cmd.size, cmd.base, cmd.phys_base);
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	IPADBG("exit\n");
	return result;
}

int ipa3_conn_wigig_rx_pipe_i(void *in, struct ipa_wigig_conn_out_params *out)
{
	int ipa_ep_idx;
	struct ipa3_ep_context *ep;
	struct ipa_ep_cfg ep_cfg;
	enum ipa_client_type rx_client = IPA_CLIENT_WIGIG_PROD;
	bool is_smmu_enabled;
	struct ipa_wigig_conn_rx_in_params_smmu *input_smmu = NULL;
	struct ipa_wigig_conn_rx_in_params *input = NULL;
	const struct ipa_gsi_ep_config *ep_gsi;
	void *pipe_info;
	void *buff;
	phys_addr_t status_ring_HWHEAD_pa;
	int result;

	IPADBG("\n");

	ipa_ep_idx = ipa_get_ep_mapping(rx_client);
	if (ipa_ep_idx == IPA_EP_NOT_ALLOCATED ||
		ipa_ep_idx >= IPA3_MAX_NUM_PIPES) {
		IPAERR("fail to get ep (IPA_CLIENT_WIGIG_PROD) %d.\n",
			ipa_ep_idx);
		return -EFAULT;
	}

	ep = &ipa3_ctx->ep[ipa_ep_idx];
	if (ep->valid) {
		IPAERR("EP %d already allocated.\n", ipa_ep_idx);
		return -EFAULT;
	}

	if (ep->gsi_offload_state) {
		IPAERR("WIGIG channel bad state 0x%X\n",
			ep->gsi_offload_state);
		return -EFAULT;
	}

	ep_gsi = ipa3_get_gsi_ep_info(rx_client);
	if (!ep_gsi) {
		IPAERR("Failed getting GSI EP info for client=%d\n",
			rx_client);
		return -EPERM;
	}

	memset(ep, 0, offsetof(struct ipa3_ep_context, sys));

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	/* setup rx ep cfg */
	ep->valid = 1;
	ep->client = rx_client;
	result = ipa3_disable_data_path(ipa_ep_idx);
	if (result) {
		IPAERR("disable data path failed res=%d clnt=%d.\n", result,
			ipa_ep_idx);
		IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
		return -EFAULT;
	}

	is_smmu_enabled = !ipa3_ctx->s1_bypass_arr[IPA_SMMU_CB_WLAN];
	if (is_smmu_enabled) {
		struct ipa_wigig_rx_pipe_data_buffer_info_smmu *dbuff_smmu;

		input_smmu = (struct ipa_wigig_conn_rx_in_params_smmu *)in;
		dbuff_smmu = &input_smmu->dbuff_smmu;
		ep->client_notify = input_smmu->notify;
		ep->priv = input_smmu->priv;

		IPADBG(
		"desc_ring_base %lld desc_ring_size %d status_ring_base %lld status_ring_size %d",
		(unsigned long long)input_smmu->pipe_smmu.desc_ring_base_iova,
		input_smmu->pipe_smmu.desc_ring_size,
		(unsigned long long)input_smmu->pipe_smmu.status_ring_base_iova,
		input_smmu->pipe_smmu.status_ring_size);
		IPADBG("data_buffer_base_iova %lld data_buffer_size %d",
			(unsigned long long)dbuff_smmu->data_buffer_base_iova,
			input_smmu->dbuff_smmu.data_buffer_size);

		if (IPA_WIGIG_MSB(
			dbuff_smmu->data_buffer_base_iova) &
			0xFFFFFF00) {
			IPAERR(
			"data_buffers_base_address_msb is over the 8 bit limit (%lld)\n",
			(unsigned long long)dbuff_smmu->data_buffer_base_iova);
			IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
			return -EFAULT;
		}
		if (dbuff_smmu->data_buffer_size >> 16) {
			IPAERR(
				"data_buffer_size is over the 16 bit limit (%d)\n"
				, dbuff_smmu->data_buffer_size);
			IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
			return -EFAULT;
		}
	} else {
		input = (struct ipa_wigig_conn_rx_in_params *)in;
		ep->client_notify = input->notify;
		ep->priv = input->priv;

		IPADBG(
			"desc_ring_base_pa %pa desc_ring_size %d status_ring_base_pa %pa status_ring_size %d",
			&input->pipe.desc_ring_base_pa,
			input->pipe.desc_ring_size,
			&input->pipe.status_ring_base_pa,
			input->pipe.status_ring_size);
		IPADBG("data_buffer_base_pa %pa data_buffer_size %d",
			&input->dbuff.data_buffer_base_pa,
			input->dbuff.data_buffer_size);

		if (
		IPA_WIGIG_MSB(input->dbuff.data_buffer_base_pa) & 0xFFFFFF00) {
			IPAERR(
				"data_buffers_base_address_msb is over the 8 bit limit (0xpa)\n"
				, &input->dbuff.data_buffer_base_pa);
			IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
			return -EFAULT;
		}
		if (input->dbuff.data_buffer_size >> 16) {
			IPAERR(
				"data_buffer_size is over the 16 bit limit (0x%X)\n"
				, input->dbuff.data_buffer_size);
			IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
			return -EFAULT;
		}
	}

	memset(&ep_cfg, 0, sizeof(ep_cfg));
	ep_cfg.nat.nat_en = IPA_SRC_NAT;
	ep_cfg.hdr.hdr_len = ETH_HLEN;
	ep_cfg.hdr.hdr_ofst_pkt_size_valid = 0;
	ep_cfg.hdr.hdr_ofst_pkt_size = 0;
	ep_cfg.hdr.hdr_additional_const_len = 0;
	ep_cfg.hdr_ext.hdr_little_endian = true;
	ep_cfg.hdr.hdr_ofst_metadata_valid = 0;
	ep_cfg.hdr.hdr_metadata_reg_valid = 1;
	ep_cfg.mode.mode = IPA_BASIC;


	if (ipa3_cfg_ep(ipa_ep_idx, &ep_cfg)) {
		IPAERR("fail to setup rx pipe cfg\n");
		result = -EFAULT;
		goto fail;
	}

	if (is_smmu_enabled) {
		result = ipa3_wigig_smmu_map_channel(true,
			&input_smmu->pipe_smmu,
			&input_smmu->dbuff_smmu,
			true);
		if (result) {
			IPAERR("failed to setup rx pipe smmu map\n");
			result = -EFAULT;
			goto fail;
		}

		pipe_info = &input_smmu->pipe_smmu;
		buff = &input_smmu->dbuff_smmu;
		status_ring_HWHEAD_pa =
			input_smmu->pipe_smmu.status_ring_HWHEAD_pa;
	} else {
		pipe_info = &input->pipe;
		buff = &input->dbuff;
		status_ring_HWHEAD_pa =
			input->pipe.status_ring_HWHEAD_pa;
	}

	result = ipa3_wigig_config_gsi(true,
		is_smmu_enabled,
		pipe_info,
		buff,
		ep_gsi, ep);
	if (result)
		goto fail_gsi;

	result = ipa3_wigig_config_uc(
		true, true, 0,
		ep_gsi->ipa_gsi_chan_num,
		status_ring_HWHEAD_pa);
	if (result)
		goto fail_uc_config;

	ipa3_install_dflt_flt_rules(ipa_ep_idx);

	out->client = IPA_CLIENT_WIGIG_PROD;
	ep->gsi_offload_state |= IPA_WIGIG_CONNECTED;

	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	IPADBG("wigig rx pipe connected successfully\n");
	IPADBG("exit\n");

	return 0;

fail_uc_config:
	/* Release channel and evt*/
	ipa3_release_gsi_channel(ipa_ep_idx);
fail_gsi:
	if (input_smmu)
		ipa3_wigig_smmu_map_channel(true, &input_smmu->pipe_smmu,
			&input_smmu->dbuff_smmu, false);
fail:
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	return result;
}

int ipa3_conn_wigig_client_i(void *in, struct ipa_wigig_conn_out_params *out)
{
	int ipa_ep_idx;
	struct ipa3_ep_context *ep;
	struct ipa_ep_cfg ep_cfg;
	enum ipa_client_type tx_client;
	bool is_smmu_enabled;
	struct ipa_wigig_conn_tx_in_params_smmu *input_smmu = NULL;
	struct ipa_wigig_conn_tx_in_params *input = NULL;
	const struct ipa_gsi_ep_config *ep_gsi;
	u32 aggr_byte_limit;
	int result;
	void *pipe_info;
	void *buff;
	phys_addr_t desc_ring_HWHEAD_pa;
	u8 wifi_ch;

	IPADBG("\n");

	is_smmu_enabled = !ipa3_ctx->s1_bypass_arr[IPA_SMMU_CB_WLAN];
	if (is_smmu_enabled) {
		input_smmu = (struct ipa_wigig_conn_tx_in_params_smmu *)in;

		IPADBG(
		"desc_ring_base %lld desc_ring_size %d status_ring_base %lld status_ring_size %d",
		(unsigned long long)input_smmu->pipe_smmu.desc_ring_base_iova,
		input_smmu->pipe_smmu.desc_ring_size,
		(unsigned long long)input_smmu->pipe_smmu.status_ring_base_iova,
		input_smmu->pipe_smmu.status_ring_size);
		IPADBG("num buffers %d, data buffer size %d\n",
			input_smmu->dbuff_smmu.num_buffers,
			input_smmu->dbuff_smmu.data_buffer_size);

		if (ipa3_wigig_tx_bit_to_ep(input_smmu->int_gen_tx_bit_num,
			&tx_client)) {
			return -EINVAL;
		}
		if (input_smmu->dbuff_smmu.data_buffer_size >> 16) {
			IPAERR(
				"data_buffer_size is over the 16 bit limit (0x%X)\n"
				, input_smmu->dbuff_smmu.data_buffer_size);
			return -EFAULT;
		}

		if (IPA_WIGIG_8_MSB(
			input_smmu->pipe_smmu.status_ring_HWHEAD_pa)
			!= IPA_WIGIG_8_MSB(
				input_smmu->pipe_smmu.status_ring_HWTAIL_pa)) {
			IPAERR(
				"status ring HWHEAD and HWTAIL differ in 8 MSbs head 0x%X tail 0x%X\n"
			, input_smmu->pipe_smmu.status_ring_HWHEAD_pa,
			input_smmu->pipe_smmu.status_ring_HWTAIL_pa);
			return -EFAULT;
		}

		wifi_ch = input_smmu->int_gen_tx_bit_num;

		/* convert to kBytes */
		aggr_byte_limit = IPA_ADJUST_AGGR_BYTE_HARD_LIMIT(
			input_smmu->dbuff_smmu.data_buffer_size);
	} else {
		input = (struct ipa_wigig_conn_tx_in_params *)in;

		IPADBG(
			"desc_ring_base_pa %pa desc_ring_size %d status_ring_base_pa %pa status_ring_size %d",
			&input->pipe.desc_ring_base_pa,
			input->pipe.desc_ring_size,
			&input->pipe.status_ring_base_pa,
			input->pipe.status_ring_size);
		IPADBG("data_buffer_size %d", input->dbuff.data_buffer_size);

		if (ipa3_wigig_tx_bit_to_ep(input->int_gen_tx_bit_num,
			&tx_client)) {
			return -EINVAL;
		}

		if (input->dbuff.data_buffer_size >> 16) {
			IPAERR(
				"data_buffer_size is over the 16 bit limit (0x%X)\n"
				, input->dbuff.data_buffer_size);
			return -EFAULT;
		}

		if (IPA_WIGIG_8_MSB(
			input->pipe.status_ring_HWHEAD_pa)
			!= IPA_WIGIG_8_MSB(
				input->pipe.status_ring_HWTAIL_pa)) {
			IPAERR(
				"status ring HWHEAD and HWTAIL differ in 8 MSbs head 0x%X tail 0x%X\n"
				, input->pipe.status_ring_HWHEAD_pa,
				input->pipe.status_ring_HWTAIL_pa);
			return -EFAULT;
		}

		wifi_ch = input->int_gen_tx_bit_num;

		/* convert to kBytes */
		aggr_byte_limit = IPA_ADJUST_AGGR_BYTE_HARD_LIMIT(
			input->dbuff.data_buffer_size);
	}
	IPADBG("client type is %d\n", tx_client);

	ipa_ep_idx = ipa_get_ep_mapping(tx_client);
	if (ipa_ep_idx == IPA_EP_NOT_ALLOCATED ||
		ipa_ep_idx >= IPA3_MAX_NUM_PIPES) {
		IPAERR("fail to get ep (%d) %d.\n",
			tx_client, ipa_ep_idx);
		return -EFAULT;
	}

	ep = &ipa3_ctx->ep[ipa_ep_idx];
	if (ep->valid) {
		IPAERR("EP %d already allocated.\n", ipa_ep_idx);
		return -EFAULT;
	}

	if (ep->gsi_offload_state) {
		IPAERR("WIGIG channel bad state 0x%X\n",
			ep->gsi_offload_state);
		return -EFAULT;
	}

	ep_gsi = ipa3_get_gsi_ep_info(tx_client);
	if (!ep_gsi) {
		IPAERR("Failed getting GSI EP info for client=%d\n",
			tx_client);
		return -EFAULT;
	}

	memset(ep, 0, offsetof(struct ipa3_ep_context, sys));
	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	/* setup tx ep cfg */
	ep->valid = 1;
	ep->client = tx_client;
	result = ipa3_disable_data_path(ipa_ep_idx);
	if (result) {
		IPAERR("disable data path failed res=%d clnt=%d.\n", result,
			ipa_ep_idx);
		goto fail;
	}

	ep->client_notify = NULL;
	ep->priv = NULL;

	memset(&ep_cfg, 0, sizeof(ep_cfg));
	ep_cfg.nat.nat_en = IPA_DST_NAT;
	ep_cfg.hdr.hdr_len = ETH_HLEN;
	ep_cfg.hdr.hdr_ofst_pkt_size_valid = 0;
	ep_cfg.hdr.hdr_ofst_pkt_size = 0;
	ep_cfg.hdr.hdr_additional_const_len = 0;
	ep_cfg.hdr_ext.hdr_little_endian = true;
	ep_cfg.mode.mode = IPA_BASIC;

	/* config hard byte limit, max is the buffer size (in kB)*/
	ep_cfg.aggr.aggr_en = IPA_ENABLE_AGGR;
	ep_cfg.aggr.aggr = IPA_GENERIC;
	ep_cfg.aggr.aggr_pkt_limit = 1;
	ep_cfg.aggr.aggr_byte_limit = aggr_byte_limit;
	ep_cfg.aggr.aggr_hard_byte_limit_en = IPA_ENABLE_AGGR;

	if (ipa3_cfg_ep(ipa_ep_idx, &ep_cfg)) {
		IPAERR("fail to setup rx pipe cfg\n");
		result = -EFAULT;
		goto fail;
	}

	if (is_smmu_enabled) {
		result = ipa3_wigig_smmu_map_channel(false,
			&input_smmu->pipe_smmu,
			&input_smmu->dbuff_smmu,
			true);
		if (result) {
			IPAERR(
				"failed to setup tx pipe smmu map client %d (ep %d)\n"
			, tx_client, ipa_ep_idx);
			result = -EFAULT;
			goto fail;
		}

		pipe_info = &input_smmu->pipe_smmu;
		buff = &input_smmu->dbuff_smmu;
		desc_ring_HWHEAD_pa =
			input_smmu->pipe_smmu.desc_ring_HWHEAD_pa;
	} else {
		pipe_info = &input->pipe;
		buff = &input->dbuff;
		desc_ring_HWHEAD_pa =
			input->pipe.desc_ring_HWHEAD_pa;
	}

	result = ipa3_wigig_config_gsi(false,
		is_smmu_enabled,
		pipe_info,
		buff,
		ep_gsi, ep);
	if (result)
		goto fail_gsi;

	result = ipa3_wigig_config_uc(
		true, false, wifi_ch,
		ep_gsi->ipa_gsi_chan_num,
		desc_ring_HWHEAD_pa);
	if (result)
		goto fail_uc_config;

	out->client = tx_client;
	ep->gsi_offload_state |= IPA_WIGIG_CONNECTED;

	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	IPADBG("wigig client %d (ep %d) connected successfully\n", tx_client,
		ipa_ep_idx);
	return 0;

fail_uc_config:
	/* Release channel and evt*/
	ipa3_release_gsi_channel(ipa_ep_idx);
fail_gsi:
	if (input_smmu)
		ipa3_wigig_smmu_map_channel(false, &input_smmu->pipe_smmu,
			&input_smmu->dbuff_smmu, false);
fail:
	ep->valid = 0;
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	return result;
}

int ipa3_disconn_wigig_pipe_i(enum ipa_client_type client,
	struct ipa_wigig_pipe_setup_info_smmu *pipe_smmu,
	void *dbuff)
{
	bool is_smmu_enabled;
	int ipa_ep_idx;
	struct ipa3_ep_context *ep;
	const struct ipa_gsi_ep_config *ep_gsi;
	int result;
	bool rx = false;

	IPADBG("\n");

	ipa_ep_idx = ipa_get_ep_mapping(client);
	if (ipa_ep_idx == IPA_EP_NOT_ALLOCATED ||
		ipa_ep_idx >= IPA3_MAX_NUM_PIPES) {
		IPAERR("fail to get ep (%d) %d.\n",
			client, ipa_ep_idx);
		return -EFAULT;
	}

	ep = &ipa3_ctx->ep[ipa_ep_idx];
	if (!ep->valid) {
		IPAERR("Invalid EP\n");
		return -EFAULT;
	}

	ep_gsi = ipa3_get_gsi_ep_info(client);
	if (!ep_gsi) {
		IPAERR("Failed getting GSI EP info for client=%d\n",
			client);
		return -EFAULT;
	}

	if (ep->gsi_offload_state != IPA_WIGIG_CONNECTED) {
		IPAERR("client in bad state(client %d) 0x%X\n",
			client, ep->gsi_offload_state);
		return -EFAULT;
	}

	if (client == IPA_CLIENT_WIGIG_PROD)
		rx = true;

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	/* Release channel and evt*/
	result = ipa3_release_gsi_channel(ipa_ep_idx);
	if (result) {
		IPAERR("failed to deallocate channel\n");
		goto fail;
	}

	is_smmu_enabled = !ipa3_ctx->s1_bypass_arr[IPA_SMMU_CB_WLAN];
	if (is_smmu_enabled) {
		if (!pipe_smmu || !dbuff) {
			IPAERR("smmu input is null %pK %pK\n",
				pipe_smmu, dbuff);
			WARN_ON(1);
		} else {
			result = ipa3_wigig_smmu_map_channel(rx,
				pipe_smmu,
				dbuff,
				false);
			if (result) {
				IPAERR(
					"failed to unmap pipe smmu %d (ep %d)\n"
					, client, ipa_ep_idx);
				result = -EFAULT;
				goto fail;
			}
		}
	} else if (pipe_smmu || dbuff) {
		IPAERR("smmu input is not null %pK %pK\n",
			pipe_smmu, dbuff);
		WARN_ON(1);
	}

	/* only gsi ch number and dir are necessary */
	result = ipa3_wigig_config_uc(
		false, rx, 0,
		ep_gsi->ipa_gsi_chan_num, 0);
	if (result) {
		IPAERR("failed uC channel teardown %d\n", result);
		WARN_ON(1);
	}

	memset(ep, 0, sizeof(struct ipa3_ep_context));

	ep->gsi_offload_state = 0;

	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	IPADBG("client (ep: %d) disconnected\n", ipa_ep_idx);

	IPADBG("exit\n");
	return 0;

fail:
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	return result;
}

int ipa3_wigig_uc_msi_init(bool init,
	phys_addr_t periph_baddr_pa,
	phys_addr_t pseudo_cause_pa,
	phys_addr_t int_gen_tx_pa,
	phys_addr_t int_gen_rx_pa,
	phys_addr_t dma_ep_misc_pa)
{
	int result;
	struct ipa_mem_buffer cmd;
	enum ipa_cpu_2_hw_offload_commands command;
	bool map = false;

	IPADBG("params: %s, %pa, %pa, %pa, %pa, %pa\n",
		init ? "init" : "deInit",
		&periph_baddr_pa,
		&pseudo_cause_pa,
		&int_gen_tx_pa,
		&int_gen_rx_pa,
		&dma_ep_misc_pa);

	/* first make sure registers are SMMU mapped if necessary*/
	if ((!ipa3_ctx->s1_bypass_arr[IPA_SMMU_CB_UC])) {
		if (init)
			map = true;

		IPADBG("SMMU enabled, map %d\n", map);

		result = ipa3_smmu_map_peer_reg(
			pseudo_cause_pa,
			map,
			IPA_SMMU_CB_UC);
		if (result) {
			IPAERR(
				"failed to %s pseudo_cause reg %d\n",
				map ? "map" : "unmap",
				result);
			goto fail;
		}

		result = ipa3_smmu_map_peer_reg(
			int_gen_tx_pa,
			map,
			IPA_SMMU_CB_UC);
		if (result) {
			IPAERR(
				"failed to %s int_gen_tx reg %d\n",
				map ? "map" : "unmap",
				result);
			goto fail_gen_tx;
		}

		result = ipa3_smmu_map_peer_reg(
			int_gen_rx_pa,
			map,
			IPA_SMMU_CB_UC);
		if (result) {
			IPAERR(
				"failed to %s int_gen_rx reg %d\n",
				map ? "map" : "unmap",
				result);
			goto fail_gen_rx;
		}

		result = ipa3_smmu_map_peer_reg(
			dma_ep_misc_pa,
			map,
			IPA_SMMU_CB_UC);
		if (result) {
			IPAERR(
				"failed to %s dma_ep_misc reg %d\n",
				map ? "map" : "unmap",
				result);
			goto fail_dma_ep_misc;
		}
	}

	/*  now send the wigig hw base address to uC*/
	if (init) {
		struct IpaHwPeripheralInitCmdData_t *cmd_data;

		cmd.size = sizeof(*cmd_data);
		cmd.base = dma_alloc_coherent(ipa3_ctx->uc_pdev, cmd.size,
			&cmd.phys_base, GFP_KERNEL);
		if (cmd.base == NULL) {
			IPAERR("fail to get DMA memory.\n");
			result = -ENOMEM;
			if (map)
				goto fail_alloc;
			return result;
		}
		cmd_data = (struct IpaHwPeripheralInitCmdData_t *)cmd.base;
		cmd_data->protocol = IPA_HW_PROTOCOL_11ad;
		cmd_data->Init_params.W11AdInit_params.periph_baddr_msb =
			IPA_WIGIG_MSB(periph_baddr_pa);
		cmd_data->Init_params.W11AdInit_params.periph_baddr_lsb =
			IPA_WIGIG_LSB(periph_baddr_pa);
		command = IPA_CPU_2_HW_CMD_PERIPHERAL_INIT;
	} else {
		struct IpaHwPeripheralDeinitCmdData_t *cmd_data;

		cmd.size = sizeof(*cmd_data);
		cmd.base = dma_alloc_coherent(ipa3_ctx->uc_pdev, cmd.size,
			&cmd.phys_base, GFP_KERNEL);
		if (cmd.base == NULL) {
			IPAERR("fail to get DMA memory.\n");
			result = -ENOMEM;
			if (map)
				goto fail_alloc;
			return result;
		}
		cmd_data = (struct IpaHwPeripheralDeinitCmdData_t *)cmd.base;
		cmd_data->protocol = IPA_HW_PROTOCOL_11ad;
		command = IPA_CPU_2_HW_CMD_PERIPHERAL_DEINIT;
	}

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	result = ipa3_uc_send_cmd((u32)(cmd.phys_base),
		command,
		IPA_HW_2_CPU_OFFLOAD_CMD_STATUS_SUCCESS,
		false, 10 * HZ);
	if (result) {
		IPAERR("fail to %s uc MSI config\n", init ? "init" : "deinit");
		goto fail_command;
	}

	dma_free_coherent(ipa3_ctx->uc_pdev, cmd.size,
		cmd.base, cmd.phys_base);
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	IPADBG("exit\n");

	return 0;
fail_command:
	dma_free_coherent(ipa3_ctx->uc_pdev,
		cmd.size,
		cmd.base, cmd.phys_base);
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
fail_alloc:
	ipa3_smmu_map_peer_reg(dma_ep_misc_pa, !map, IPA_SMMU_CB_UC);
fail_dma_ep_misc:
	ipa3_smmu_map_peer_reg(int_gen_rx_pa, !map, IPA_SMMU_CB_UC);
fail_gen_rx:
	ipa3_smmu_map_peer_reg(int_gen_tx_pa, !map, IPA_SMMU_CB_UC);
fail_gen_tx:
	ipa3_smmu_map_peer_reg(pseudo_cause_pa, !map, IPA_SMMU_CB_UC);
fail:
	return result;
}

int ipa3_enable_wigig_pipe_i(enum ipa_client_type client)
{
	int ipa_ep_idx, res;
	struct ipa3_ep_context *ep;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;
	int retry_cnt = 0;
	uint64_t val;

	IPADBG("\n");

	ipa_ep_idx = ipa_get_ep_mapping(client);
	if (ipa_ep_idx == IPA_EP_NOT_ALLOCATED ||
		ipa_ep_idx >= IPA3_MAX_NUM_PIPES) {
		IPAERR("fail to get ep (%d) %d.\n",
			client, ipa_ep_idx);
		return -EFAULT;
	}

	ep = &ipa3_ctx->ep[ipa_ep_idx];

	if (!ep->valid) {
		IPAERR("Invalid EP\n");
		return -EFAULT;
	}

	if (ep->gsi_offload_state != IPA_WIGIG_CONNECTED) {
		IPAERR("WIGIG channel bad state 0x%X\n",
			ep->gsi_offload_state);
		return -EFAULT;
	}

	IPA_ACTIVE_CLIENTS_INC_EP(client);

	res = ipa3_enable_data_path(ipa_ep_idx);
	if (res)
		goto fail_enable_datapath;

	memset(&ep_cfg_ctrl, 0, sizeof(struct ipa_ep_cfg_ctrl));
	ipa3_cfg_ep_ctrl(ipa_ep_idx, &ep_cfg_ctrl);

	/* ring the event db (outside the ring boundary)*/
	val = ep->gsi_mem_info.evt_ring_base_addr +
		ep->gsi_mem_info.evt_ring_len;
	res = gsi_ring_evt_ring_db(ep->gsi_evt_ring_hdl, val);
	if (res) {
		IPAERR(
			"fail to ring evt ring db %d. hdl=%lu wp=0x%llx\n"
			, res, ep->gsi_evt_ring_hdl,
			(unsigned long long)val);
		res = -EFAULT;
		goto fail_ring_evt;
	}

	IPADBG("start channel\n");
	res = gsi_start_channel(ep->gsi_chan_hdl);
	if (res != GSI_STATUS_SUCCESS) {
		IPAERR("gsi_start_channel failed %d\n", res);
		WARN_ON(1);
		res = -EFAULT;
		goto fail_gsi_start;
	}

	/* for TX we have to ring the channel db (last desc in the ring) */
	if (client != IPA_CLIENT_WIGIG_PROD) {
		uint64_t val;

		val  = ep->gsi_mem_info.chan_ring_base_addr +
			ep->gsi_mem_info.chan_ring_len -
			IPA_WIGIG_DESC_RING_EL_SIZE;

		IPADBG("ring ch doorbell (0x%llX) TX %d\n", val,
			ep->gsi_chan_hdl);
		res = gsi_ring_ch_ring_db(ep->gsi_chan_hdl, val);
		if (res) {
			IPAERR(
				"fail to ring channel db %d. hdl=%lu wp=0x%llx\n"
				, res, ep->gsi_chan_hdl,
				(unsigned long long)val);
			res = -EFAULT;
			goto fail_ring_ch;
		}
	}

	ep->gsi_offload_state |= IPA_WIGIG_ENABLED;

	IPADBG("exit\n");

	return 0;

fail_ring_ch:
	res = ipa3_stop_gsi_channel(ipa_ep_idx);
	if (res != 0 && res != -GSI_STATUS_AGAIN &&
		res != -GSI_STATUS_TIMED_OUT) {
		IPAERR("failed to stop channel res = %d\n", res);
	} else if (res == -GSI_STATUS_AGAIN) {
		IPADBG("GSI stop channel failed retry cnt = %d\n",
			retry_cnt);
		retry_cnt++;
		if (retry_cnt < GSI_STOP_MAX_RETRY_CNT)
			goto fail_ring_ch;
	} else {
		IPADBG("GSI channel %ld STOP\n", ep->gsi_chan_hdl);
	}
	res = -EFAULT;
fail_gsi_start:
fail_ring_evt:
	ipa3_disable_data_path(ipa_ep_idx);
fail_enable_datapath:
	IPA_ACTIVE_CLIENTS_DEC_EP(client);
	return res;
}

int ipa3_disable_wigig_pipe_i(enum ipa_client_type client)
{
	int ipa_ep_idx, res;
	struct ipa3_ep_context *ep;
	struct ipahal_ep_cfg_ctrl_scnd ep_ctrl_scnd = { 0 };
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;
	bool disable_force_clear = false;
	u32 source_pipe_bitmask = 0;
	int retry_cnt = 0;

	IPADBG("\n");

	ipa_ep_idx = ipa_get_ep_mapping(client);
	if (ipa_ep_idx == IPA_EP_NOT_ALLOCATED ||
		ipa_ep_idx >= IPA3_MAX_NUM_PIPES) {
		IPAERR("fail to get ep (%d) %d.\n",
			client, ipa_ep_idx);
		return -EFAULT;
	}
	if (ipa_ep_idx >= IPA3_MAX_NUM_PIPES) {
		IPAERR("ep %d out of range.\n", ipa_ep_idx);
		return -EFAULT;
	}

	ep = &ipa3_ctx->ep[ipa_ep_idx];

	if (!ep->valid) {
		IPAERR("Invalid EP\n");
		return -EFAULT;
	}

	if (ep->gsi_offload_state !=
		(IPA_WIGIG_CONNECTED | IPA_WIGIG_ENABLED)) {
		IPAERR("WIGIG channel bad state 0x%X\n",
			ep->gsi_offload_state);
		return -EFAULT;
	}

	IPADBG("pipe %d\n", ipa_ep_idx);
	source_pipe_bitmask = 1 << ipa_ep_idx;
	res = ipa3_enable_force_clear(ipa_ep_idx,
		false, source_pipe_bitmask);
	if (res) {
		/*
		 * assuming here modem SSR, AP can remove
		 * the delay in this case
		 */
		IPAERR("failed to force clear %d\n", res);
		IPAERR("remove delay from SCND reg\n");
		ep_ctrl_scnd.endp_delay = false;
		ipahal_write_reg_n_fields(
			IPA_ENDP_INIT_CTRL_SCND_n, ipa_ep_idx,
			&ep_ctrl_scnd);
	} else {
		disable_force_clear = true;
	}
retry_gsi_stop:
	res = ipa3_stop_gsi_channel(ipa_ep_idx);
	if (res != 0 && res != -GSI_STATUS_AGAIN &&
		res != -GSI_STATUS_TIMED_OUT) {
		IPAERR("failed to stop channel res = %d\n", res);
		goto fail_stop_channel;
	} else if (res == -GSI_STATUS_AGAIN) {
		IPADBG("GSI stop channel failed retry cnt = %d\n",
			retry_cnt);
		retry_cnt++;
		if (retry_cnt >= GSI_STOP_MAX_RETRY_CNT)
			goto fail_stop_channel;
		goto retry_gsi_stop;
	} else {
		IPADBG("GSI channel %ld STOP\n", ep->gsi_chan_hdl);
	}

	res = ipa3_reset_gsi_channel(ipa_ep_idx);
	if (res != GSI_STATUS_SUCCESS) {
		IPAERR("Failed to reset chan: %d.\n", res);
		goto fail_stop_channel;
	}

	if (disable_force_clear)
		ipa3_disable_force_clear(ipa_ep_idx);

	res = ipa3_disable_data_path(ipa_ep_idx);
	if (res) {
		WARN_ON(1);
		return res;
	}

	/* Set the delay after disabling IPA Producer pipe */
	if (IPA_CLIENT_IS_PROD(ep->client)) {
		memset(&ep_cfg_ctrl, 0, sizeof(struct ipa_ep_cfg_ctrl));
		ep_cfg_ctrl.ipa_ep_delay = true;
		ipa3_cfg_ep_ctrl(ipa_ep_idx, &ep_cfg_ctrl);
	}

	ep->gsi_offload_state &= ~IPA_WIGIG_ENABLED;

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(ipa_ep_idx));
	IPADBG("exit\n");
	return 0;

fail_stop_channel:
	ipa_assert();
	return res;
}
