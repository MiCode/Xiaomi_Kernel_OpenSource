// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#include "hfi_packetization.h"
#include "msm_cvp_debug.h"

/* Set up look-up tables to convert HAL_* to HFI_*.
 *
 * The tables below mostly take advantage of the fact that most
 * HAL_* types are defined bitwise. So if we index them normally
 * when declaring the tables, we end up with huge arrays with wasted
 * space.  So before indexing them, we apply log2 to use a more
 * sensible index.
 */

int cvp_create_pkt_cmd_sys_init(struct cvp_hfi_cmd_sys_init_packet *pkt,
			   u32 arch_type)
{
	int rc = 0;

	if (!pkt)
		return -EINVAL;

	pkt->packet_type = HFI_CMD_SYS_INIT;
	pkt->size = sizeof(struct cvp_hfi_cmd_sys_init_packet);
	pkt->arch_type = arch_type;
	return rc;
}

int cvp_create_pkt_cmd_sys_pc_prep(struct cvp_hfi_cmd_sys_pc_prep_packet *pkt)
{
	int rc = 0;

	if (!pkt)
		return -EINVAL;

	pkt->packet_type = HFI_CMD_SYS_PC_PREP;
	pkt->size = sizeof(struct cvp_hfi_cmd_sys_pc_prep_packet);
	return rc;
}

int cvp_create_pkt_cmd_sys_debug_config(
	struct cvp_hfi_cmd_sys_set_property_packet *pkt,
	u32 mode)
{
	struct cvp_hfi_debug_config *hfi;

	if (!pkt)
		return -EINVAL;

	pkt->size = sizeof(struct cvp_hfi_cmd_sys_set_property_packet) +
		sizeof(struct cvp_hfi_debug_config) + sizeof(u32);
	pkt->packet_type = HFI_CMD_SYS_SET_PROPERTY;
	pkt->num_properties = 1;
	pkt->rg_property_data[0] = HFI_PROPERTY_SYS_DEBUG_CONFIG;
	hfi = (struct cvp_hfi_debug_config *) &pkt->rg_property_data[1];
	hfi->debug_config = mode;
	hfi->debug_mode = HFI_DEBUG_MODE_QUEUE;
	if (msm_cvp_fw_debug_mode
			<= (HFI_DEBUG_MODE_QUEUE | HFI_DEBUG_MODE_QDSS))
		hfi->debug_mode = msm_cvp_fw_debug_mode;
	return 0;
}

int cvp_create_pkt_cmd_sys_coverage_config(
	struct cvp_hfi_cmd_sys_set_property_packet *pkt,
	u32 mode)
{
	if (!pkt) {
		dprintk(CVP_ERR, "In %s(), No input packet\n", __func__);
		return -EINVAL;
	}

	pkt->size = sizeof(struct cvp_hfi_cmd_sys_set_property_packet) +
		sizeof(u32);
	pkt->packet_type = HFI_CMD_SYS_SET_PROPERTY;
	pkt->num_properties = 1;
	pkt->rg_property_data[0] = HFI_PROPERTY_SYS_CONFIG_COVERAGE;
	pkt->rg_property_data[1] = mode;
	dprintk(CVP_DBG, "Firmware coverage mode %d\n",
			pkt->rg_property_data[1]);
	return 0;
}

int cvp_create_pkt_cmd_sys_set_idle_indicator(
	struct cvp_hfi_cmd_sys_set_property_packet *pkt,
	u32 mode)
{
	if (!pkt) {
		dprintk(CVP_ERR, "In %s(), No input packet\n", __func__);
		return -EINVAL;
	}

	pkt->size = sizeof(struct cvp_hfi_cmd_sys_set_property_packet) +
		sizeof(u32);
	pkt->packet_type = HFI_CMD_SYS_SET_PROPERTY;
	pkt->num_properties = 1;
	pkt->rg_property_data[0] = HFI_PROPERTY_SYS_IDLE_INDICATOR;
	pkt->rg_property_data[1] = mode;
	dprintk(CVP_DBG, "Firmware idle indicator mode %d\n",
			pkt->rg_property_data[1]);
	return 0;
}

int cvp_create_pkt_cmd_sys_set_resource(
		struct cvp_hfi_cmd_sys_set_resource_packet *pkt,
		struct cvp_resource_hdr *res_hdr,
		void *res_value)
{
	int rc = 0;
	u32 i = 0;

	if (!pkt || !res_hdr || !res_value) {
		dprintk(CVP_ERR,
			"Invalid paramas pkt %pK res_hdr %pK res_value %pK\n",
				pkt, res_hdr, res_value);
		return -EINVAL;
	}

	pkt->packet_type = HFI_CMD_SYS_SET_RESOURCE;
	pkt->size = sizeof(struct cvp_hfi_cmd_sys_set_resource_packet);
	pkt->resource_handle = hash32_ptr(res_hdr->resource_handle);

	switch (res_hdr->resource_id) {
	case CVP_RESOURCE_SYSCACHE:
	{
		struct cvp_hfi_resource_syscache_info_type *res_sc_info =
			(struct cvp_hfi_resource_syscache_info_type *)res_value;
		struct cvp_hfi_resource_subcache_type *res_sc =
			(struct cvp_hfi_resource_subcache_type *)
				&(res_sc_info->rg_subcache_entries[0]);

		struct cvp_hfi_resource_syscache_info_type *hfi_sc_info =
			(struct cvp_hfi_resource_syscache_info_type *)
				&pkt->rg_resource_data[0];

		struct cvp_hfi_resource_subcache_type *hfi_sc =
			(struct cvp_hfi_resource_subcache_type *)
			&(hfi_sc_info->rg_subcache_entries[0]);

		pkt->resource_type = HFI_RESOURCE_SYSCACHE;
		hfi_sc_info->num_entries = res_sc_info->num_entries;

		pkt->size += (sizeof(struct cvp_hfi_resource_subcache_type))
				 * hfi_sc_info->num_entries;

		for (i = 0; i < hfi_sc_info->num_entries; i++) {
			hfi_sc[i] = res_sc[i];
		dprintk(CVP_DBG, "entry hfi#%d, sc_id %d, size %d\n",
				 i, hfi_sc[i].sc_id, hfi_sc[i].size);
		}
		break;
	}
	default:
		dprintk(CVP_ERR,
			"Invalid resource_id %d\n", res_hdr->resource_id);
		rc = -ENOTSUPP;
	}

	return rc;
}

int cvp_create_pkt_cmd_sys_release_resource(
		struct cvp_hfi_cmd_sys_release_resource_packet *pkt,
		struct cvp_resource_hdr *res_hdr)
{
	int rc = 0;

	if (!pkt || !res_hdr) {
		dprintk(CVP_ERR,
			"Invalid paramas pkt %pK res_hdr %pK\n",
				pkt, res_hdr);
		return -EINVAL;
	}

	pkt->size = sizeof(struct cvp_hfi_cmd_sys_release_resource_packet);
	pkt->packet_type = HFI_CMD_SYS_RELEASE_RESOURCE;
	pkt->resource_handle = hash32_ptr(res_hdr->resource_handle);

	switch (res_hdr->resource_id) {
	case CVP_RESOURCE_SYSCACHE:
		pkt->resource_type = HFI_RESOURCE_SYSCACHE;
		break;
	default:
		dprintk(CVP_ERR,
			 "Invalid resource_id %d\n", res_hdr->resource_id);
		rc = -ENOTSUPP;
	}

	dprintk(CVP_DBG,
		"rel_res: pkt_type 0x%x res_type 0x%x prepared\n",
		pkt->packet_type, pkt->resource_type);

	return rc;
}

inline int cvp_create_pkt_cmd_sys_session_init(
		struct cvp_hfi_cmd_sys_session_init_packet *pkt,
		struct cvp_hal_session *session)
{
	int rc = 0;
	struct msm_cvp_inst *inst = session->session_id;

	if (!pkt || !inst)
		return -EINVAL;

	pkt->size = sizeof(struct cvp_hfi_cmd_sys_session_init_packet);
	pkt->packet_type = HFI_CMD_SYS_SESSION_INIT;
	pkt->session_id = hash32_ptr(session);
	pkt->session_type = inst->prop.type;
	pkt->session_kmask = inst->prop.kernel_mask;
	pkt->session_prio = inst->prop.priority;
	pkt->is_secure = inst->prop.is_secure;
	pkt->dsp_ac_mask = inst->prop.dsp_mask;

	return rc;
}

static int create_pkt_cmd_sys_ubwc_config(
		struct cvp_hfi_cmd_sys_set_property_packet *pkt,
		struct msm_cvp_ubwc_config_data *ubwc_config)
{
	int rc = 0;
	struct cvp_hfi_cmd_sys_set_ubwc_config_packet_type *hfi;

	if (!pkt)
		return -EINVAL;

	pkt->size = sizeof(struct cvp_hfi_cmd_sys_set_property_packet) +
		sizeof(struct cvp_hfi_cmd_sys_set_ubwc_config_packet_type)
		+ sizeof(u32);

	pkt->packet_type = HFI_CMD_SYS_SET_PROPERTY;
	pkt->num_properties = 1;
	pkt->rg_property_data[0] = HFI_PROPERTY_SYS_UBWC_CONFIG;
	hfi = (struct cvp_hfi_cmd_sys_set_ubwc_config_packet_type *)
		&pkt->rg_property_data[1];

	hfi->max_channels = ubwc_config->max_channels;
	hfi->override_bit_info.max_channel_override =
		ubwc_config->override_bit_info.max_channel_override;

	hfi->mal_length = ubwc_config->mal_length;
	hfi->override_bit_info.mal_length_override =
		ubwc_config->override_bit_info.mal_length_override;

	hfi->highest_bank_bit = ubwc_config->highest_bank_bit;
	hfi->override_bit_info.hb_override =
		ubwc_config->override_bit_info.hb_override;

	hfi->bank_swzl_level = ubwc_config->bank_swzl_level;
	hfi->override_bit_info.bank_swzl_level_override =
		ubwc_config->override_bit_info.bank_swzl_level_override;

	hfi->bank_spreading = ubwc_config->bank_spreading;
	hfi->override_bit_info.bank_spreading_override =
		ubwc_config->override_bit_info.bank_spreading_override;
	hfi->size = sizeof(struct cvp_hfi_cmd_sys_set_ubwc_config_packet_type);

	return rc;
}

int cvp_create_pkt_cmd_session_cmd(struct cvp_hal_session_cmd_pkt *pkt,
			int pkt_type, struct cvp_hal_session *session)
{
	int rc = 0;

	if (!pkt)
		return -EINVAL;

	pkt->size = sizeof(struct cvp_hal_session_cmd_pkt);
	pkt->packet_type = pkt_type;
	pkt->session_id = hash32_ptr(session);

	return rc;
}

int cvp_create_pkt_cmd_sys_power_control(
	struct cvp_hfi_cmd_sys_set_property_packet *pkt, u32 enable)
{
	struct cvp_hfi_enable *hfi;

	if (!pkt) {
		dprintk(CVP_ERR, "No input packet\n");
		return -EINVAL;
	}

	pkt->size = sizeof(struct cvp_hfi_cmd_sys_set_property_packet) +
		sizeof(struct cvp_hfi_enable) + sizeof(u32);
	pkt->packet_type = HFI_CMD_SYS_SET_PROPERTY;
	pkt->num_properties = 1;
	pkt->rg_property_data[0] = HFI_PROPERTY_SYS_CODEC_POWER_PLANE_CTRL;
	hfi = (struct cvp_hfi_enable *) &pkt->rg_property_data[1];
	hfi->enable = enable;
	return 0;
}

int cvp_create_pkt_cmd_session_set_buffers(
		void *cmd,
		struct cvp_hal_session *session,
		struct cvp_buffer_addr_info *buffer_info)
{
	int rc = 0;
	unsigned int ver;

	ver = get_hfi_version();
	ver = (ver & HFI_VERSION_MINOR_MASK) >> HFI_VERSION_MINOR_SHIFT;

	if (!cmd || !session)
		return -EINVAL;

	if (ver < 1) {
		struct cvp_hfi_cmd_session_set_buffers_packet_d *pkt;

		pkt = (struct cvp_hfi_cmd_session_set_buffers_packet_d *)cmd;
		pkt->packet_type = HFI_CMD_SESSION_CVP_SET_BUFFERS;
		pkt->session_id = hash32_ptr(session);
		pkt->buffer_addr = buffer_info->align_device_addr;
		pkt->buffer_size = buffer_info->buffer_size;
		pkt->size =
			sizeof(struct cvp_hfi_cmd_session_set_buffers_packet_d);
	} else {
		struct cvp_hfi_cmd_session_set_buffers_packet *pkt;

		pkt = (struct cvp_hfi_cmd_session_set_buffers_packet *)cmd;
		pkt->packet_type = HFI_CMD_SESSION_CVP_SET_BUFFERS;
		pkt->session_id = hash32_ptr(session);
		pkt->buf_type.fd = buffer_info->align_device_addr;
		pkt->buf_type.size = buffer_info->buffer_size;
		pkt->size =
			sizeof(struct cvp_hfi_cmd_session_set_buffers_packet);
	}

	return rc;
}

int cvp_create_pkt_cmd_session_release_buffers(
		void *cmd,
		struct cvp_hal_session *session,
		struct cvp_buffer_addr_info *buffer_info)
{
	unsigned int ver;

	ver = get_hfi_version();
	ver = (ver & HFI_VERSION_MINOR_MASK) >> HFI_VERSION_MINOR_SHIFT;

	if (!cmd || !session)
		return -EINVAL;

	if (ver < 1) {
		struct cvp_session_release_buffers_packet_d *pkt;

		pkt = (struct cvp_session_release_buffers_packet_d *)cmd;
		pkt->packet_type = HFI_CMD_SESSION_CVP_RELEASE_BUFFERS;
		pkt->session_id = hash32_ptr(session);
		pkt->num_buffers = buffer_info->num_buffers;
		pkt->buffer_type = buffer_info->buffer_type;
		pkt->size =
			sizeof(struct cvp_session_release_buffers_packet_d) +
			((buffer_info->num_buffers - 1) * sizeof(u32));
	} else {
		struct cvp_session_release_buffers_packet *pkt;

		pkt = (struct cvp_session_release_buffers_packet *)cmd;
		pkt->packet_type = HFI_CMD_SESSION_CVP_RELEASE_BUFFERS;
		pkt->session_id = hash32_ptr(session);
		pkt->num_buffers = buffer_info->num_buffers;
		pkt->buffer_type = buffer_info->buffer_type;
		pkt->size =
			sizeof(struct cvp_session_release_buffers_packet) +
			((buffer_info->num_buffers - 1) * sizeof(u32));
	}

	if (buffer_info->buffer_type == HAL_BUFFER_OUTPUT ||
		buffer_info->buffer_type == HAL_BUFFER_OUTPUT2) {
		dprintk(CVP_ERR, "%s: deprecated buffer_type\n", __func__);
		return -EINVAL;
	}

	return 0;
}

int cvp_create_pkt_cmd_session_send(
		struct cvp_kmd_hfi_packet *out_pkt,
		struct cvp_hal_session *session,
		struct cvp_kmd_hfi_packet *in_pkt)
{
	int def_idx;
	struct cvp_hal_session_cmd_pkt *ptr =
		(struct cvp_hal_session_cmd_pkt *)in_pkt;

	if (!out_pkt || !in_pkt || !session)
		return -EINVAL;

	if (ptr->size > MAX_HFI_PKT_SIZE * sizeof(unsigned int))
		goto error_hfi_packet;

	if (ptr->session_id != hash32_ptr(session))
		goto error_hfi_packet;

	def_idx = get_pkt_index(ptr);
	if (def_idx < 0) {
		memcpy(out_pkt, in_pkt, ptr->size);
		return 0;
	}

	if (cvp_hfi_defs[def_idx].type != ptr->packet_type)
		goto error_hfi_packet;

	memcpy(out_pkt, in_pkt, ptr->size);

	return 0;

error_hfi_packet:
	dprintk(CVP_ERR, "%s incorrect packet: size=%d type=%d sessionid=%d\n",
		__func__, ptr->size, ptr->packet_type, ptr->session_id);

	return -EINVAL;
}

static int get_hfi_ssr_type(enum hal_ssr_trigger_type type)
{
	int rc = HFI_TEST_SSR_HW_WDOG_IRQ;

	switch (type) {
	case SSR_ERR_FATAL:
		rc = HFI_TEST_SSR_SW_ERR_FATAL;
		break;
	case SSR_SW_DIV_BY_ZERO:
		rc = HFI_TEST_SSR_SW_DIV_BY_ZERO;
		break;
	case SSR_HW_WDOG_IRQ:
		rc = HFI_TEST_SSR_HW_WDOG_IRQ;
		break;
	default:
		dprintk(CVP_WARN,
			"SSR trigger type not recognized, using WDOG.\n");
	}
	return rc;
}

int cvp_create_pkt_ssr_cmd(enum hal_ssr_trigger_type type,
		struct cvp_hfi_cmd_sys_test_ssr_packet *pkt)
{
	if (!pkt) {
		dprintk(CVP_ERR, "Invalid params, device: %pK\n", pkt);
		return -EINVAL;
	}
	pkt->size = sizeof(struct cvp_hfi_cmd_sys_test_ssr_packet);
	pkt->packet_type = HFI_CMD_SYS_TEST_SSR;
	pkt->trigger_type = get_hfi_ssr_type(type);
	return 0;
}

int cvp_create_pkt_cmd_sys_image_version(
		struct cvp_hfi_cmd_sys_get_property_packet *pkt)
{
	if (!pkt) {
		dprintk(CVP_ERR, "%s invalid param :%pK\n", __func__, pkt);
		return -EINVAL;
	}
	pkt->size = sizeof(struct cvp_hfi_cmd_sys_get_property_packet);
	pkt->packet_type = HFI_CMD_SYS_GET_PROPERTY;
	pkt->num_properties = 1;
	pkt->rg_property_data[0] = HFI_PROPERTY_SYS_IMAGE_VERSION;
	return 0;
}

static struct cvp_hfi_packetization_ops hfi_default = {
	.sys_init = cvp_create_pkt_cmd_sys_init,
	.sys_pc_prep = cvp_create_pkt_cmd_sys_pc_prep,
	.sys_power_control = cvp_create_pkt_cmd_sys_power_control,
	.sys_set_resource = cvp_create_pkt_cmd_sys_set_resource,
	.sys_debug_config = cvp_create_pkt_cmd_sys_debug_config,
	.sys_coverage_config = cvp_create_pkt_cmd_sys_coverage_config,
	.sys_set_idle_indicator = cvp_create_pkt_cmd_sys_set_idle_indicator,
	.sys_release_resource = cvp_create_pkt_cmd_sys_release_resource,
	.sys_image_version = cvp_create_pkt_cmd_sys_image_version,
	.sys_ubwc_config = create_pkt_cmd_sys_ubwc_config,
	.ssr_cmd = cvp_create_pkt_ssr_cmd,
	.session_init = cvp_create_pkt_cmd_sys_session_init,
	.session_cmd = cvp_create_pkt_cmd_session_cmd,
	.session_set_buffers =
		cvp_create_pkt_cmd_session_set_buffers,
	.session_release_buffers =
		cvp_create_pkt_cmd_session_release_buffers,
	.session_send = cvp_create_pkt_cmd_session_send,
};

struct cvp_hfi_packetization_ops *cvp_hfi_get_pkt_ops_handle(
			enum hfi_packetization_type type)
{
	dprintk(CVP_DBG, "%s selected\n",
		type == HFI_PACKETIZATION_4XX ?
		"4xx packetization" : "Unknown hfi");

	switch (type) {
	case HFI_PACKETIZATION_4XX:
		return &hfi_default;
	}

	return NULL;
}
