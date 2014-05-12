/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/wait.h>

#include <sound/apr_audio-v2.h>
#include <mach/qdsp6v2/apr.h>
#include <sound/q6adm-v2.h>
#include <sound/q6audio-v2.h>
#include <sound/q6afe-v2.h>

#include "audio_acdb.h"


#define TIMEOUT_MS 1000

#define RESET_COPP_ID 99
#define INVALID_COPP_ID 0xFF
/* Used for inband payload copy, max size is 4k */
/* 2 is to account for module & param ID in payload */
#define ADM_GET_PARAMETER_LENGTH  (4096 - APR_HDR_SIZE - 2 * sizeof(uint32_t))

#define ULL_SUPPORTED_SAMPLE_RATE 48000
#define ULL_MAX_SUPPORTED_CHANNEL 2
enum {
	ADM_RX_AUDPROC_CAL,
	ADM_TX_AUDPROC_CAL,
	ADM_RX_AUDVOL_CAL,
	ADM_TX_AUDVOL_CAL,
	ADM_CUSTOM_TOP_CAL,
	ADM_RTAC,
	ADM_MAX_CAL_TYPES
};

struct adm_ctl {
	void *apr;
	atomic_t copp_id[AFE_MAX_PORTS];
	atomic_t copp_cnt[AFE_MAX_PORTS];
	atomic_t copp_low_latency_id[AFE_MAX_PORTS];
	atomic_t copp_low_latency_cnt[AFE_MAX_PORTS];
	atomic_t copp_perf_mode[AFE_MAX_PORTS];
	atomic_t copp_stat[AFE_MAX_PORTS];
	wait_queue_head_t wait[AFE_MAX_PORTS];

	struct acdb_cal_block mem_addr_audproc[MAX_AUDPROC_TYPES];
	struct acdb_cal_block mem_addr_audvol[MAX_AUDPROC_TYPES];

	atomic_t mem_map_cal_handles[ADM_MAX_CAL_TYPES];
	atomic_t mem_map_cal_index;

	int set_custom_topology;
	int ec_ref_rx;
};

static struct adm_ctl			this_adm;

struct adm_multi_ch_map {
	bool set_channel_map;
	char channel_mapping[PCM_FORMAT_MAX_NUM_CHANNEL];
};

static struct adm_multi_ch_map multi_ch_map = { false,
						{0, 0, 0, 0, 0, 0, 0, 0}
					      };

static int adm_get_parameters[ADM_GET_PARAMETER_LENGTH];

int srs_trumedia_open(int port_id, int srs_tech_id, void *srs_params)
{
	struct adm_cmd_set_pp_params_inband_v5 *adm_params = NULL;
	int ret = 0, sz = 0;
	int index;

	pr_debug("SRS - %s", __func__);
	switch (srs_tech_id) {
	case SRS_ID_GLOBAL: {
		struct srs_trumedia_params_GLOBAL *glb_params = NULL;
		sz = sizeof(struct adm_cmd_set_pp_params_inband_v5) +
			sizeof(struct srs_trumedia_params_GLOBAL);
		adm_params = kzalloc(sz, GFP_KERNEL);
		if (!adm_params) {
			pr_err("%s, adm params memory alloc failed\n",
				__func__);
			return -ENOMEM;
		}
		adm_params->payload_size =
			sizeof(struct srs_trumedia_params_GLOBAL) +
			sizeof(struct adm_param_data_v5);
		adm_params->params.param_id = SRS_TRUMEDIA_PARAMS;
		adm_params->params.param_size =
				sizeof(struct srs_trumedia_params_GLOBAL);
		glb_params = (struct srs_trumedia_params_GLOBAL *)
			((u8 *)adm_params +
			sizeof(struct adm_cmd_set_pp_params_inband_v5));
		memcpy(glb_params, srs_params,
			sizeof(struct srs_trumedia_params_GLOBAL));
		pr_debug("SRS - %s: Global params - 1 = %x, 2 = %x, 3 = %x, 4 = %x, 5 = %x, 6 = %x, 7 = %x, 8 = %x\n",
				__func__, (int)glb_params->v1,
				(int)glb_params->v2, (int)glb_params->v3,
				(int)glb_params->v4, (int)glb_params->v5,
				(int)glb_params->v6, (int)glb_params->v7,
				(int)glb_params->v8);
		break;
	}
	case SRS_ID_WOWHD: {
		struct srs_trumedia_params_WOWHD *whd_params = NULL;
		sz = sizeof(struct adm_cmd_set_pp_params_inband_v5) +
			sizeof(struct srs_trumedia_params_WOWHD);
		adm_params = kzalloc(sz, GFP_KERNEL);
		if (!adm_params) {
			pr_err("%s, adm params memory alloc failed\n",
				__func__);
			return -ENOMEM;
		}
		adm_params->payload_size =
			sizeof(struct srs_trumedia_params_WOWHD) +
			sizeof(struct adm_param_data_v5);
		adm_params->params.param_id = SRS_TRUMEDIA_PARAMS_WOWHD;
		adm_params->params.param_size =
				sizeof(struct srs_trumedia_params_WOWHD);
		whd_params = (struct srs_trumedia_params_WOWHD *)
			((u8 *)adm_params +
			sizeof(struct adm_cmd_set_pp_params_inband_v5));
		memcpy(whd_params, srs_params,
				sizeof(struct srs_trumedia_params_WOWHD));
		pr_debug("SRS - %s: WOWHD params - 1 = %x, 2 = %x, 3 = %x, 4 = %x, 5 = %x, 6 = %x, 7 = %x, 8 = %x, 9 = %x, 10 = %x, 11 = %x\n",
			 __func__, (int)whd_params->v1,
			(int)whd_params->v2, (int)whd_params->v3,
			(int)whd_params->v4, (int)whd_params->v5,
			(int)whd_params->v6, (int)whd_params->v7,
			(int)whd_params->v8, (int)whd_params->v9,
			(int)whd_params->v10, (int)whd_params->v11);
		break;
	}
	case SRS_ID_CSHP: {
		struct srs_trumedia_params_CSHP *chp_params = NULL;
		sz = sizeof(struct adm_cmd_set_pp_params_inband_v5) +
			sizeof(struct srs_trumedia_params_CSHP);
		adm_params = kzalloc(sz, GFP_KERNEL);
		if (!adm_params) {
			pr_err("%s, adm params memory alloc failed\n",
				__func__);
			return -ENOMEM;
		}
		adm_params->payload_size =
			sizeof(struct srs_trumedia_params_CSHP) +
			sizeof(struct adm_param_data_v5);
		adm_params->params.param_id = SRS_TRUMEDIA_PARAMS_CSHP;
		adm_params->params.param_size =
				sizeof(struct srs_trumedia_params_CSHP);
		chp_params = (struct srs_trumedia_params_CSHP *)
			((u8 *)adm_params +
			sizeof(struct adm_cmd_set_pp_params_inband_v5));
		memcpy(chp_params, srs_params,
				sizeof(struct srs_trumedia_params_CSHP));
		pr_debug("SRS - %s: CSHP params - 1 = %x, 2 = %x, 3 = %x, 4 = %x, 5 = %x, 6 = %x, 7 = %x, 8 = %x, 9 = %x\n",
				__func__, (int)chp_params->v1,
				(int)chp_params->v2, (int)chp_params->v3,
				(int)chp_params->v4, (int)chp_params->v5,
				(int)chp_params->v6, (int)chp_params->v7,
				(int)chp_params->v8, (int)chp_params->v9);
		break;
	}
	case SRS_ID_HPF: {
		struct srs_trumedia_params_HPF *hpf_params = NULL;
		sz = sizeof(struct adm_cmd_set_pp_params_inband_v5) +
			sizeof(struct srs_trumedia_params_HPF);
		adm_params = kzalloc(sz, GFP_KERNEL);
		if (!adm_params) {
			pr_err("%s, adm params memory alloc failed\n",
				__func__);
			return -ENOMEM;
		}
		adm_params->payload_size =
			sizeof(struct srs_trumedia_params_HPF) +
			sizeof(struct adm_param_data_v5);
		adm_params->params.param_id = SRS_TRUMEDIA_PARAMS_HPF;
		adm_params->params.param_size =
				sizeof(struct srs_trumedia_params_HPF);
		hpf_params = (struct srs_trumedia_params_HPF *)
			((u8 *)adm_params +
			sizeof(struct adm_cmd_set_pp_params_inband_v5));
		memcpy(hpf_params, srs_params,
			sizeof(struct srs_trumedia_params_HPF));
		pr_debug("SRS - %s: HPF params - 1 = %x\n", __func__,
				(int)hpf_params->v1);
		break;
	}
	case SRS_ID_PEQ: {
		struct srs_trumedia_params_PEQ *peq_params = NULL;
		sz = sizeof(struct adm_cmd_set_pp_params_inband_v5) +
			sizeof(struct srs_trumedia_params_PEQ);
		adm_params = kzalloc(sz, GFP_KERNEL);
		if (!adm_params) {
			pr_err("%s, adm params memory alloc failed\n",
				__func__);
			return -ENOMEM;
		}
		adm_params->payload_size =
				sizeof(struct srs_trumedia_params_PEQ) +
				sizeof(struct adm_param_data_v5);
		adm_params->params.param_id = SRS_TRUMEDIA_PARAMS_PEQ;
		adm_params->params.param_size =
				sizeof(struct srs_trumedia_params_PEQ);
		peq_params = (struct srs_trumedia_params_PEQ *)
			((u8 *)adm_params +
			sizeof(struct adm_cmd_set_pp_params_inband_v5));
		memcpy(peq_params, srs_params,
				sizeof(struct srs_trumedia_params_PEQ));
		pr_debug("SRS - %s: PEQ params - 1 = %x 2 = %x, 3 = %x, 4 = %x\n",
			__func__, (int)peq_params->v1,
			(int)peq_params->v2, (int)peq_params->v3,
			(int)peq_params->v4);
		break;
	}
	case SRS_ID_HL: {
		struct srs_trumedia_params_HL *hl_params = NULL;
		sz = sizeof(struct adm_cmd_set_pp_params_inband_v5) +
			sizeof(struct srs_trumedia_params_HL);
		adm_params = kzalloc(sz, GFP_KERNEL);
		if (!adm_params) {
			pr_err("%s, adm params memory alloc failed\n",
				__func__);
			return -ENOMEM;
		}
		adm_params->payload_size =
			sizeof(struct srs_trumedia_params_HL) +
			sizeof(struct adm_param_data_v5);
		adm_params->params.param_id = SRS_TRUMEDIA_PARAMS_HL;
		adm_params->params.param_size =
			sizeof(struct srs_trumedia_params_HL);
		hl_params = (struct srs_trumedia_params_HL *)
			((u8 *)adm_params +
			sizeof(struct adm_cmd_set_pp_params_inband_v5));
		memcpy(hl_params, srs_params,
				sizeof(struct srs_trumedia_params_HL));
		pr_debug("SRS - %s: HL params - 1 = %x, 2 = %x, 3 = %x, 4 = %x, 5 = %x, 6 = %x, 7 = %x\n",
				__func__, (int)hl_params->v1,
				(int)hl_params->v2, (int)hl_params->v3,
				(int)hl_params->v4, (int)hl_params->v5,
				(int)hl_params->v6, (int)hl_params->v7);
		break;
	}
	default:
		goto fail_cmd;
	}

	adm_params->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	adm_params->hdr.pkt_size = sz;
	adm_params->hdr.src_svc = APR_SVC_ADM;
	adm_params->hdr.src_domain = APR_DOMAIN_APPS;
	adm_params->hdr.src_port = port_id;
	adm_params->hdr.dest_svc = APR_SVC_ADM;
	adm_params->hdr.dest_domain = APR_DOMAIN_ADSP;
	index = afe_get_port_index(port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: invalid port idx %d portid %#x\n",
				__func__, index, port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}
	adm_params->hdr.dest_port = atomic_read(&this_adm.copp_id[index]);
	adm_params->hdr.token = port_id;
	adm_params->hdr.opcode = ADM_CMD_SET_PP_PARAMS_V5;
	adm_params->payload_addr_lsw = 0;
	adm_params->payload_addr_msw = 0;
	adm_params->mem_map_handle = 0;

	adm_params->params.module_id = SRS_TRUMEDIA_MODULE_ID;
	adm_params->params.reserved = 0;

	pr_debug("SRS - %s: Command was sent now check Q6 - port id = %d, size %d, module id %x, param id %x.\n",
			__func__, adm_params->hdr.dest_port,
			adm_params->payload_size, adm_params->params.module_id,
			adm_params->params.param_id);

	ret = apr_send_pkt(this_adm.apr, (uint32_t *)adm_params);
	if (ret < 0) {
		pr_err("SRS - %s: ADM enable for port %d failed\n", __func__,
			port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}
	/* Wait for the callback with copp id */
	ret = wait_event_timeout(this_adm.wait[index], 1,
			msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: SRS set params timed out port = %d\n",
			__func__, port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}

fail_cmd:
	kfree(adm_params);
	return ret;
}

int adm_dolby_dap_send_params(int port_id, char *params, uint32_t params_length)
{
	struct adm_cmd_set_pp_params_v5	*adm_params = NULL;
	int sz, rc = 0, index = afe_get_port_index(port_id);

	pr_debug("%s\n", __func__);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: invalid port idx %d portid %#x\n",
			__func__, index, port_id);
		return -EINVAL;
	}
	sz = sizeof(struct adm_cmd_set_pp_params_v5) + params_length;
	adm_params = kzalloc(sz, GFP_KERNEL);
	if (!adm_params) {
		pr_err("%s, adm params memory alloc failed", __func__);
		return -ENOMEM;
	}

	memcpy(((u8 *)adm_params + sizeof(struct adm_cmd_set_pp_params_v5)),
			params, params_length);
	adm_params->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	adm_params->hdr.pkt_size = sz;
	adm_params->hdr.src_svc = APR_SVC_ADM;
	adm_params->hdr.src_domain = APR_DOMAIN_APPS;
	adm_params->hdr.src_port = port_id;
	adm_params->hdr.dest_svc = APR_SVC_ADM;
	adm_params->hdr.dest_domain = APR_DOMAIN_ADSP;
	adm_params->hdr.dest_port = atomic_read(&this_adm.copp_id[index]);
	adm_params->hdr.token = port_id;
	adm_params->hdr.opcode = ADM_CMD_SET_PP_PARAMS_V5;
	adm_params->payload_addr_lsw = 0;
	adm_params->payload_addr_msw = 0;
	adm_params->mem_map_handle = 0;
	adm_params->payload_size = params_length;

	atomic_set(&this_adm.copp_stat[index], 0);
	rc = apr_send_pkt(this_adm.apr, (uint32_t *)adm_params);
	if (rc < 0) {
		pr_err("%s: Set params failed port = %#x\n",
			__func__, port_id);
		rc = -EINVAL;
		goto dolby_dap_send_param_return;
	}
	/* Wait for the callback */
	rc = wait_event_timeout(this_adm.wait[index],
		atomic_read(&this_adm.copp_stat[index]),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!rc) {
		pr_err("%s: Set params timed out port = %#x\n",
			 __func__, port_id);
		rc = -EINVAL;
		goto dolby_dap_send_param_return;
	}
	rc = 0;
dolby_dap_send_param_return:
	kfree(adm_params);
	return rc;
}

int adm_get_params(int port_id, uint32_t module_id, uint32_t param_id,
		uint32_t params_length, char *params)
{
	struct adm_cmd_get_pp_params_v5 *adm_params = NULL;
	int sz, rc = 0, i = 0, index = afe_get_port_index(port_id);
	int *params_data = (int *)params;

	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: invalid port idx %d portid %#x\n",
			__func__, index, port_id);
		return -EINVAL;
	}
	sz = sizeof(struct adm_cmd_get_pp_params_v5) + params_length;
	adm_params = kzalloc(sz, GFP_KERNEL);
	if (!adm_params) {
		pr_err("%s, adm params memory alloc failed", __func__);
		return -ENOMEM;
	}

	memcpy(((u8 *)adm_params + sizeof(struct adm_cmd_get_pp_params_v5)),
		params, params_length);
	adm_params->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
	APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	adm_params->hdr.pkt_size = sz;
	adm_params->hdr.src_svc = APR_SVC_ADM;
	adm_params->hdr.src_domain = APR_DOMAIN_APPS;
	adm_params->hdr.src_port = port_id;
	adm_params->hdr.dest_svc = APR_SVC_ADM;
	adm_params->hdr.dest_domain = APR_DOMAIN_ADSP;
	adm_params->hdr.dest_port = atomic_read(&this_adm.copp_id[index]);
	adm_params->hdr.token = port_id;
	adm_params->hdr.opcode = ADM_CMD_GET_PP_PARAMS_V5;
	adm_params->data_payload_addr_lsw = 0;
	adm_params->data_payload_addr_msw = 0;
	adm_params->mem_map_handle = 0;
	adm_params->module_id = module_id;
	adm_params->param_id = param_id;
	adm_params->param_max_size = params_length;
	adm_params->reserved = 0;

	atomic_set(&this_adm.copp_stat[index], 0);
	rc = apr_send_pkt(this_adm.apr, (uint32_t *)adm_params);
	if (rc < 0) {
		pr_err("%s: Failed to Get Params on port %d\n", __func__,
			port_id);
		rc = -EINVAL;
		goto adm_get_param_return;
	}
	/* Wait for the callback with copp id */
	rc = wait_event_timeout(this_adm.wait[index],
	atomic_read(&this_adm.copp_stat[index]),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!rc) {
		pr_err("%s: get params timed out port = %d\n", __func__,
			port_id);
		rc = -EINVAL;
		goto adm_get_param_return;
	}
	if (params_data) {
		for (i = 0; i < adm_get_parameters[0]; i++)
			params_data[i] = adm_get_parameters[1+i];
	}
	rc = 0;
adm_get_param_return:
	kfree(adm_params);

	return rc;
}


static void adm_callback_debug_print(struct apr_client_data *data)
{
	uint32_t *payload;
	payload = data->payload;

	if (data->payload_size >= 8)
		pr_debug("%s: code = 0x%x PL#0[%x], PL#1[%x], size = %d\n",
			__func__, data->opcode, payload[0], payload[1],
			data->payload_size);
	else if (data->payload_size >= 4)
		pr_debug("%s: code = 0x%x PL#0[%x], size = %d\n",
			__func__, data->opcode, payload[0],
			data->payload_size);
	else
		pr_debug("%s: code = 0x%x, size = %d\n",
			__func__, data->opcode, data->payload_size);
}

void adm_set_multi_ch_map(char *channel_map)
{
	memcpy(multi_ch_map.channel_mapping, channel_map,
		PCM_FORMAT_MAX_NUM_CHANNEL);
	multi_ch_map.set_channel_map = true;
}

void adm_get_multi_ch_map(char *channel_map)
{
	if (multi_ch_map.set_channel_map) {
		memcpy(channel_map, multi_ch_map.channel_mapping,
			PCM_FORMAT_MAX_NUM_CHANNEL);
	}
}

static int32_t adm_callback(struct apr_client_data *data, void *priv)
{
	uint32_t *payload;
	int i, index;

	if (data == NULL) {
		pr_err("%s: data paramter is null\n", __func__);
		return -EINVAL;
	}

	payload = data->payload;

	if (data->opcode == RESET_EVENTS) {
		pr_debug("adm_callback: Reset event is received: %d %d apr[%p]\n",
				data->reset_event, data->reset_proc,
				this_adm.apr);
		if (this_adm.apr) {
			apr_reset(this_adm.apr);
			for (i = 0; i < AFE_MAX_PORTS; i++) {
				atomic_set(&this_adm.copp_id[i],
							RESET_COPP_ID);
				atomic_set(&this_adm.copp_low_latency_id[i],
							RESET_COPP_ID);
				atomic_set(&this_adm.copp_cnt[i], 0);
				atomic_set(&this_adm.copp_low_latency_cnt[i],
						0);
				atomic_set(&this_adm.copp_perf_mode[i], 0);
				atomic_set(&this_adm.copp_stat[i], 0);
			}
			this_adm.apr = NULL;
			reset_custom_topology_flags();
			this_adm.set_custom_topology = 1;
			for (i = 0; i < ADM_MAX_CAL_TYPES; i++)
				atomic_set(&this_adm.mem_map_cal_handles[i],
					0);
			rtac_clear_mapping(ADM_RTAC_CAL);
		}
		pr_debug("Resetting calibration blocks");
		for (i = 0; i < MAX_AUDPROC_TYPES; i++) {
			/* Device calibration */
			this_adm.mem_addr_audproc[i].cal_size = 0;
			this_adm.mem_addr_audproc[i].cal_kvaddr = 0;
			this_adm.mem_addr_audproc[i].cal_paddr = 0;

			/* Volume calibration */
			this_adm.mem_addr_audvol[i].cal_size = 0;
			this_adm.mem_addr_audvol[i].cal_kvaddr = 0;
			this_adm.mem_addr_audvol[i].cal_paddr = 0;
		}
		return 0;
	}

	adm_callback_debug_print(data);
	if (data->payload_size) {
		index = q6audio_get_port_index(data->token);
		if (index < 0 || index >= AFE_MAX_PORTS) {
			pr_err("%s: invalid port idx %d token %d\n",
					__func__, index, data->token);
			return 0;
		}
		if (data->opcode == APR_BASIC_RSP_RESULT) {
			pr_debug("APR_BASIC_RSP_RESULT id %x\n", payload[0]);
			if (payload[1] != 0) {
				pr_err("%s: cmd = 0x%x returned error = 0x%x\n",
					__func__, payload[0], payload[1]);
			}
			switch (payload[0]) {
			case ADM_CMD_SET_PP_PARAMS_V5:
				pr_debug("%s: ADM_CMD_SET_PP_PARAMS_V5\n",
					__func__);
				if (rtac_make_adm_callback(
					payload, data->payload_size)) {
					break;
				}
			case ADM_CMD_DEVICE_CLOSE_V5:
			case ADM_CMD_SHARED_MEM_UNMAP_REGIONS:
			case ADM_CMD_MATRIX_MAP_ROUTINGS_V5:
			case ADM_CMD_ADD_TOPOLOGIES:
				pr_debug("%s: Basic callback received, wake up.\n",
					__func__);
				atomic_set(&this_adm.copp_stat[index], 1);
				wake_up(&this_adm.wait[index]);
				break;
			case ADM_CMD_SHARED_MEM_MAP_REGIONS:
				pr_debug("%s: ADM_CMD_SHARED_MEM_MAP_REGIONS\n",
					__func__);
				/* Should only come here if there is an APR */
				/* error or malformed APR packet. Otherwise */
				/* response will be returned as */
				if (payload[1] != 0) {
					pr_err("%s: ADM map error, resuming\n",
						__func__);
					atomic_set(&this_adm.copp_stat[index],
							1);
					wake_up(&this_adm.wait[index]);
				}
				break;
			case ADM_CMD_GET_PP_PARAMS_V5:
				pr_debug("%s: ADM_CMD_GET_PP_PARAMS_V5\n",
					__func__);
				/* Should only come here if there is an APR */
				/* error or malformed APR packet. Otherwise */
				/* response will be returned as */
				/* ADM_CMDRSP_GET_PP_PARAMS_V5 */
				if (payload[1] != 0) {
					pr_err("%s: ADM get param error = %d, resuming\n",
						__func__, payload[1]);
					rtac_make_adm_callback(payload,
						data->payload_size);
				}
				break;
			default:
				pr_err("%s: Unknown Cmd: 0x%x\n", __func__,
								payload[0]);
				break;
			}
			return 0;
		}

		switch (data->opcode) {
		case ADM_CMDRSP_DEVICE_OPEN_V5: {
			struct adm_cmd_rsp_device_open_v5 *open =
			(struct adm_cmd_rsp_device_open_v5 *)data->payload;
			if (open->copp_id == INVALID_COPP_ID) {
				pr_err("%s: invalid coppid rxed %d\n",
					__func__, open->copp_id);
				atomic_set(&this_adm.copp_stat[index], 1);
				wake_up(&this_adm.wait[index]);
				break;
			}
			if (atomic_read(&this_adm.copp_perf_mode[index])) {
				atomic_set(&this_adm.copp_low_latency_id[index],
						open->copp_id);
			} else {
				atomic_set(&this_adm.copp_id[index],
					open->copp_id);
			}
			atomic_set(&this_adm.copp_stat[index], 1);
			pr_debug("%s: coppid rxed=%d\n", __func__,
							open->copp_id);
			wake_up(&this_adm.wait[index]);
			}
			break;
		case ADM_CMDRSP_GET_PP_PARAMS_V5:
			pr_debug("%s: ADM_CMDRSP_GET_PP_PARAMS_V5\n", __func__);
			if (payload[0] != 0)
				pr_err("%s: ADM_CMDRSP_GET_PP_PARAMS_V5 returned error = 0x%x\n",
					__func__, payload[0]);
			if (rtac_make_adm_callback(payload,
					data->payload_size))
				break;

			if (data->payload_size > (4 * sizeof(uint32_t))) {
				adm_get_parameters[0] = payload[3];
				pr_debug("GET_PP PARAM:received parameter length: %x\n",
						adm_get_parameters[0]);
				/* storing param size then params */
				for (i = 0; i < payload[3]; i++)
					adm_get_parameters[1+i] = payload[4+i];
			}
			atomic_set(&this_adm.copp_stat[index], 1);
			wake_up(&this_adm.wait[index]);
			break;
		case ADM_CMDRSP_SHARED_MEM_MAP_REGIONS:
			pr_debug("%s: ADM_CMDRSP_SHARED_MEM_MAP_REGIONS\n",
				__func__);
			atomic_set(&this_adm.mem_map_cal_handles[
				atomic_read(&this_adm.mem_map_cal_index)],
				*payload);
			atomic_set(&this_adm.copp_stat[index], 1);
			wake_up(&this_adm.wait[index]);
			break;
		default:
			pr_err("%s: Unknown cmd:0x%x\n", __func__,
							data->opcode);
			break;
		}
	}
	return 0;
}

void send_adm_custom_topology(int port_id)
{
	struct acdb_cal_block		cal_block;
	struct cmd_set_topologies	adm_top;
	int				index;
	int				result;
	int				size = 4096;

	get_adm_custom_topology(&cal_block);
	if (cal_block.cal_size == 0) {
		pr_debug("%s: no cal to send addr= 0x%x\n",
				__func__, cal_block.cal_paddr);
		goto done;
	}

	index = afe_get_port_index(port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: invalid port idx %d portid %#x\n",
				__func__, index, port_id);
		goto done;
	}

	if (this_adm.set_custom_topology) {
		/* specific index 4 for adm topology memory */
		atomic_set(&this_adm.mem_map_cal_index, ADM_CUSTOM_TOP_CAL);

		/* Only call this once */
		this_adm.set_custom_topology = 0;

		result = adm_memory_map_regions(port_id,
				&cal_block.cal_paddr, 0, &size, 1);
		if (result < 0) {
			pr_err("%s: mmap did not work! addr = 0x%x, size = %d\n",
				__func__, cal_block.cal_paddr,
			       cal_block.cal_size);
			goto done;
		}
	}


	adm_top.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
		APR_HDR_LEN(20), APR_PKT_VER);
	adm_top.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		sizeof(adm_top));
	adm_top.hdr.src_svc = APR_SVC_ADM;
	adm_top.hdr.src_domain = APR_DOMAIN_APPS;
	adm_top.hdr.src_port = port_id;
	adm_top.hdr.dest_svc = APR_SVC_ADM;
	adm_top.hdr.dest_domain = APR_DOMAIN_ADSP;
	adm_top.hdr.dest_port = atomic_read(&this_adm.copp_id[index]);
	adm_top.hdr.token = port_id;
	adm_top.hdr.opcode = ADM_CMD_ADD_TOPOLOGIES;
	adm_top.payload_addr_lsw = cal_block.cal_paddr;
	adm_top.payload_addr_msw = 0;
	adm_top.mem_map_handle =
		atomic_read(&this_adm.mem_map_cal_handles[ADM_CUSTOM_TOP_CAL]);
	adm_top.payload_size = cal_block.cal_size;

	atomic_set(&this_adm.copp_stat[index], 0);
	pr_debug("%s: Sending ADM_CMD_ADD_TOPOLOGIES payload = 0x%x, size = %d\n",
		__func__, adm_top.payload_addr_lsw,
		adm_top.payload_size);
	result = apr_send_pkt(this_adm.apr, (uint32_t *)&adm_top);
	if (result < 0) {
		pr_err("%s: Set topologies failed port = 0x%x payload = 0x%x\n",
			__func__, port_id, cal_block.cal_paddr);
		goto done;
	}
	/* Wait for the callback */
	result = wait_event_timeout(this_adm.wait[index],
		atomic_read(&this_adm.copp_stat[index]),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!result) {
		pr_err("%s: Set topologies timed out port = 0x%x, payload = 0x%x\n",
			__func__, port_id, cal_block.cal_paddr);
		goto done;
	}

done:
	return;
}

static int send_adm_cal_block(int port_id, struct acdb_cal_block *aud_cal,
			      int perf_mode)
{
	s32				result = 0;
	struct adm_cmd_set_pp_params_v5	adm_params;
	int index = afe_get_port_index(port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: invalid port idx %d portid %#x\n",
				__func__, index, port_id);
		return 0;
	}

	pr_debug("%s: Port id %#x, index %d\n", __func__, port_id, index);

	if (!aud_cal || aud_cal->cal_size == 0) {
		pr_debug("%s: No ADM cal to send for port_id = %#x!\n",
			__func__, port_id);
		result = -EINVAL;
		goto done;
	}

	adm_params.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
		APR_HDR_LEN(20), APR_PKT_VER);
	adm_params.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		sizeof(adm_params));
	adm_params.hdr.src_svc = APR_SVC_ADM;
	adm_params.hdr.src_domain = APR_DOMAIN_APPS;
	adm_params.hdr.src_port = port_id;
	adm_params.hdr.dest_svc = APR_SVC_ADM;
	adm_params.hdr.dest_domain = APR_DOMAIN_ADSP;

	if (perf_mode == LEGACY_PCM_MODE)
		adm_params.hdr.dest_port =
			atomic_read(&this_adm.copp_id[index]);
	else
		adm_params.hdr.dest_port =
			atomic_read(&this_adm.copp_low_latency_id[index]);

	adm_params.hdr.token = port_id;
	adm_params.hdr.opcode = ADM_CMD_SET_PP_PARAMS_V5;
	adm_params.payload_addr_lsw = aud_cal->cal_paddr;
	adm_params.payload_addr_msw = 0;
	adm_params.mem_map_handle = atomic_read(&this_adm.mem_map_cal_handles[
				atomic_read(&this_adm.mem_map_cal_index)]);
	adm_params.payload_size = aud_cal->cal_size;

	atomic_set(&this_adm.copp_stat[index], 0);
	pr_debug("%s: Sending SET_PARAMS payload = 0x%x, size = %d\n",
		__func__, adm_params.payload_addr_lsw,
		adm_params.payload_size);
	result = apr_send_pkt(this_adm.apr, (uint32_t *)&adm_params);
	if (result < 0) {
		pr_err("%s: Set params failed port = %#x payload = 0x%x\n",
			__func__, port_id, aud_cal->cal_paddr);
		result = -EINVAL;
		goto done;
	}
	/* Wait for the callback */
	result = wait_event_timeout(this_adm.wait[index],
		atomic_read(&this_adm.copp_stat[index]),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!result) {
		pr_err("%s: Set params timed out port = %#x, payload = 0x%x\n",
			__func__, port_id, aud_cal->cal_paddr);
		result = -EINVAL;
		goto done;
	}

	result = 0;
done:
	return result;
}

static void send_adm_cal(int port_id, int path, int perf_mode)
{
	int			result = 0;
	s32			acdb_path;
	struct acdb_cal_block	aud_cal;
	int			size;
	pr_debug("%s\n", __func__);

	/* Maps audio_dev_ctrl path definition to ACDB definition */
	acdb_path = path - 1;
	if (acdb_path == TX_CAL)
		size = 4096 * 4;
	else
		size = 4096;

	pr_debug("%s: Sending audproc cal\n", __func__);
	get_audproc_cal(acdb_path, &aud_cal);

	/* map & cache buffers used */
	atomic_set(&this_adm.mem_map_cal_index, acdb_path);
	if (((this_adm.mem_addr_audproc[acdb_path].cal_paddr !=
		aud_cal.cal_paddr)  && (aud_cal.cal_size > 0)) ||
		(aud_cal.cal_size >
		this_adm.mem_addr_audproc[acdb_path].cal_size)) {

		if (this_adm.mem_addr_audproc[acdb_path].cal_paddr != 0)
			adm_memory_unmap_regions(port_id);

		result = adm_memory_map_regions(port_id, &aud_cal.cal_paddr,
						0, &size, 1);
		if (result < 0) {
			pr_err("ADM audproc mmap did not work! path = %d, addr = 0x%x, size = %d\n",
				acdb_path, aud_cal.cal_paddr,
				aud_cal.cal_size);
		} else {
			this_adm.mem_addr_audproc[acdb_path].cal_paddr =
							aud_cal.cal_paddr;
			this_adm.mem_addr_audproc[acdb_path].cal_size = size;
		}
	}

	if (!send_adm_cal_block(port_id, &aud_cal, perf_mode))
		pr_debug("%s: Audproc cal sent for port id: %#x, path %d\n",
			__func__, port_id, acdb_path);
	else
		pr_debug("%s: Audproc cal not sent for port id: %#x, path %d\n",
			__func__, port_id, acdb_path);

	pr_debug("%s: Sending audvol cal\n", __func__);
	get_audvol_cal(acdb_path, &aud_cal);

	/* map & cache buffers used */
	atomic_set(&this_adm.mem_map_cal_index,
		(acdb_path + MAX_AUDPROC_TYPES));
	if (((this_adm.mem_addr_audvol[acdb_path].cal_paddr !=
		aud_cal.cal_paddr)  && (aud_cal.cal_size > 0))  ||
		(aud_cal.cal_size >
		this_adm.mem_addr_audvol[acdb_path].cal_size)) {

		if (this_adm.mem_addr_audvol[acdb_path].cal_paddr != 0)
			adm_memory_unmap_regions(port_id);

		result = adm_memory_map_regions(port_id, &aud_cal.cal_paddr,
						0, &size, 1);
		if (result < 0) {
			pr_err("ADM audvol mmap did not work! path = %d, addr = 0x%x, size = %d\n",
				acdb_path, aud_cal.cal_paddr,
				aud_cal.cal_size);
		} else {
			this_adm.mem_addr_audvol[acdb_path].cal_paddr =
							aud_cal.cal_paddr;
			this_adm.mem_addr_audvol[acdb_path].cal_size = size;
		}
	}

	if (!send_adm_cal_block(port_id, &aud_cal, perf_mode))
		pr_debug("%s: Audvol cal sent for port id: %#x, path %d\n",
			__func__, port_id, acdb_path);
	else
		pr_debug("%s: Audvol cal not sent for port id: %#x, path %d\n",
			__func__, port_id, acdb_path);
}

int adm_map_rtac_block(struct rtac_cal_block_data *cal_block)
{
	int	result = 0;
	pr_debug("%s\n", __func__);

	if (cal_block == NULL) {
		pr_err("%s: cal_block is NULL!\n",
			__func__);
		result = -EINVAL;
		goto done;
	}

	if (cal_block->cal_data.paddr == 0) {
		pr_debug("%s: No address to map!\n",
			__func__);
		result = -EINVAL;
		goto done;
	}

	if (cal_block->map_data.map_size == 0) {
		pr_debug("%s: map size is 0!\n",
			__func__);
		result = -EINVAL;
		goto done;
	}

	/* valid port ID needed for callback use primary I2S */
	atomic_set(&this_adm.mem_map_cal_index, ADM_RTAC);
	result = adm_memory_map_regions(PRIMARY_I2S_RX,
			&cal_block->cal_data.paddr, 0,
			&cal_block->map_data.map_size, 1);
	if (result < 0) {
		pr_err("%s: RTAC mmap did not work! addr = 0x%x, size = %d\n",
			__func__, cal_block->cal_data.paddr,
			cal_block->map_data.map_size);
		goto done;
	}

	cal_block->map_data.map_handle = atomic_read(
		&this_adm.mem_map_cal_handles[ADM_RTAC]);
done:
	return result;
}

int adm_unmap_rtac_block(uint32_t *mem_map_handle)
{
	int	result = 0;
	pr_debug("%s\n", __func__);

	if (mem_map_handle == NULL) {
		pr_debug("%s: Map handle is NULL, nothing to unmap\n",
			__func__);
		goto done;
	}

	if (*mem_map_handle == 0) {
		pr_debug("%s: Map handle is 0, nothing to unmap\n",
			__func__);
		goto done;
	}

	if (*mem_map_handle != atomic_read(
			&this_adm.mem_map_cal_handles[ADM_RTAC])) {
		pr_err("%s: Map handles do not match! Unmapping RTAC, RTAC map 0x%x, ADM map 0x%x\n",
			__func__, *mem_map_handle, atomic_read(
			&this_adm.mem_map_cal_handles[ADM_RTAC]));

		/* if mismatch use handle passed in to unmap */
		atomic_set(&this_adm.mem_map_cal_handles[ADM_RTAC],
			   *mem_map_handle);
	}

	/* valid port ID needed for callback use primary I2S */
	atomic_set(&this_adm.mem_map_cal_index, ADM_RTAC);
	result = adm_memory_unmap_regions(PRIMARY_I2S_RX);
	if (result < 0) {
		pr_debug("%s: adm_memory_unmap_regions failed, error %d\n",
			__func__, result);
	} else {
		atomic_set(&this_adm.mem_map_cal_handles[ADM_RTAC], 0);
		*mem_map_handle = 0;
	}
done:
	return result;
}

int adm_unmap_cal_blocks(void)
{
	int	i;
	int	result = 0;
	int	result2 = 0;

	for (i = 0; i < ADM_MAX_CAL_TYPES; i++) {
		if (atomic_read(&this_adm.mem_map_cal_handles[i]) != 0) {

			if (i <= ADM_TX_AUDPROC_CAL) {
				this_adm.mem_addr_audproc[i].cal_paddr = 0;
				this_adm.mem_addr_audproc[i].cal_size = 0;
			} else if (i <= ADM_TX_AUDVOL_CAL) {
				this_adm.mem_addr_audvol
					[(i - ADM_RX_AUDVOL_CAL)].cal_paddr
					= 0;
				this_adm.mem_addr_audvol
					[(i - ADM_RX_AUDVOL_CAL)].cal_size
					= 0;
			} else if (i == ADM_CUSTOM_TOP_CAL) {
				this_adm.set_custom_topology = 1;
			} else {
				continue;
			}

			/* valid port ID needed for callback use primary I2S */
			atomic_set(&this_adm.mem_map_cal_index, i);
			result2 = adm_memory_unmap_regions(PRIMARY_I2S_RX);
			if (result2 < 0) {
				pr_err("%s: adm_memory_unmap_regions failed, err %d\n",
						__func__, result2);
				result = result2;
			} else {
				atomic_set(&this_adm.mem_map_cal_handles[i],
					0);
			}
		}
	}
	return result;
}

int adm_connect_afe_port(int mode, int session_id, int port_id)
{
	struct adm_cmd_connect_afe_port_v5	cmd;
	int ret = 0;
	int index;

	pr_debug("%s: port %d session id:%d mode:%d\n", __func__,
				port_id, session_id, mode);

	port_id = afe_convert_virtual_to_portid(port_id);

	if (afe_validate_port(port_id) < 0) {
		pr_err("%s port idi[%d] is invalid\n", __func__, port_id);
		return -ENODEV;
	}
	if (this_adm.apr == NULL) {
		this_adm.apr = apr_register("ADSP", "ADM", adm_callback,
						0xFFFFFFFF, &this_adm);
		if (this_adm.apr == NULL) {
			pr_err("%s: Unable to register ADM\n", __func__);
			ret = -ENODEV;
			return ret;
		}
		rtac_set_adm_handle(this_adm.apr);
	}
	index = afe_get_port_index(port_id);
	pr_debug("%s: Port ID %#x, index %d\n", __func__, port_id, index);

	cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cmd.hdr.pkt_size = sizeof(cmd);
	cmd.hdr.src_svc = APR_SVC_ADM;
	cmd.hdr.src_domain = APR_DOMAIN_APPS;
	cmd.hdr.src_port = port_id;
	cmd.hdr.dest_svc = APR_SVC_ADM;
	cmd.hdr.dest_domain = APR_DOMAIN_ADSP;
	cmd.hdr.dest_port = port_id;
	cmd.hdr.token = port_id;
	cmd.hdr.opcode = ADM_CMD_CONNECT_AFE_PORT_V5;

	cmd.mode = mode;
	cmd.session_id = session_id;
	cmd.afe_port_id = port_id;

	atomic_set(&this_adm.copp_stat[index], 0);
	ret = apr_send_pkt(this_adm.apr, (uint32_t *)&cmd);
	if (ret < 0) {
		pr_err("%s:ADM enable for port %#x failed\n",
					__func__, port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}
	/* Wait for the callback with copp id */
	ret = wait_event_timeout(this_adm.wait[index],
		atomic_read(&this_adm.copp_stat[index]),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s ADM connect AFE failed for port %#x\n", __func__,
							port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}
	atomic_inc(&this_adm.copp_cnt[index]);
	return 0;

fail_cmd:

	return ret;
}

int adm_open(int port_id, int path, int rate, int channel_mode, int topology,
			int perf_mode, uint16_t bits_per_sample)
{
	struct adm_cmd_device_open_v5	open;
	int ret = 0;
	int index;
	int tmp_port = q6audio_get_port_id(port_id);

	pr_debug("%s: port %#x path:%d rate:%d mode:%d perf_mode:%d\n",
		 __func__, port_id, path, rate, channel_mode, perf_mode);

	port_id = q6audio_convert_virtual_to_portid(port_id);

	if (q6audio_validate_port(port_id) < 0) {
		pr_err("%s port idi[%#x] is invalid\n", __func__, port_id);
		return -ENODEV;
	}

	index = q6audio_get_port_index(port_id);
	pr_debug("%s: Port ID %#x, index %d\n", __func__, port_id, index);

	if (this_adm.apr == NULL) {
		this_adm.apr = apr_register("ADSP", "ADM", adm_callback,
						0xFFFFFFFF, &this_adm);
		if (this_adm.apr == NULL) {
			pr_err("%s: Unable to register ADM\n", __func__);
			ret = -ENODEV;
			return ret;
		}
		rtac_set_adm_handle(this_adm.apr);
	}

	if (perf_mode == LEGACY_PCM_MODE) {
		atomic_set(&this_adm.copp_perf_mode[index], 0);
		send_adm_custom_topology(port_id);
	} else {
		atomic_set(&this_adm.copp_perf_mode[index], 1);
	}

	/* Create a COPP if port id are not enabled */
	if ((perf_mode == LEGACY_PCM_MODE &&
		(atomic_read(&this_adm.copp_cnt[index]) == 0)) ||
		(perf_mode != LEGACY_PCM_MODE &&
		(atomic_read(&this_adm.copp_low_latency_cnt[index]) == 0))) {
		pr_debug("%s:opening ADM: perf_mode: %d\n", __func__,
			perf_mode);
		open.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
		open.hdr.pkt_size = sizeof(open);
		open.hdr.src_svc = APR_SVC_ADM;
		open.hdr.src_domain = APR_DOMAIN_APPS;
		open.hdr.src_port = tmp_port;
		open.hdr.dest_svc = APR_SVC_ADM;
		open.hdr.dest_domain = APR_DOMAIN_ADSP;
		open.hdr.dest_port = tmp_port;
		open.hdr.token = port_id;
		open.hdr.opcode = ADM_CMD_DEVICE_OPEN_V5;
		if (perf_mode == ULTRA_LOW_LATENCY_PCM_MODE)
			open.flags = ADM_ULTRA_LOW_LATENCY_DEVICE_SESSION;
		else if (perf_mode == LOW_LATENCY_PCM_MODE)
			open.flags = ADM_LOW_LATENCY_DEVICE_SESSION;
		else
			open.flags = ADM_LEGACY_DEVICE_SESSION;

		open.mode_of_operation = path;
		open.endpoint_id_1 = tmp_port;

		if (this_adm.ec_ref_rx == -1) {
			open.endpoint_id_2 = 0xFFFF;
		} else if (this_adm.ec_ref_rx && (path != 1)) {
			open.endpoint_id_2 = this_adm.ec_ref_rx;
			this_adm.ec_ref_rx = -1;
		}

		open.topology_id = topology;
		if ((open.topology_id == VPM_TX_SM_ECNS_COPP_TOPOLOGY) ||
			(open.topology_id == VPM_TX_DM_FLUENCE_COPP_TOPOLOGY) ||
			(open.topology_id == VPM_TX_DM_RFECNS_COPP_TOPOLOGY))
				rate = 16000;

		if (perf_mode == ULTRA_LOW_LATENCY_PCM_MODE) {
			open.topology_id = NULL_COPP_TOPOLOGY;
			rate = ULL_SUPPORTED_SAMPLE_RATE;
			if(channel_mode > ULL_MAX_SUPPORTED_CHANNEL)
				channel_mode = ULL_MAX_SUPPORTED_CHANNEL;
		} else if (perf_mode == LOW_LATENCY_PCM_MODE) {
			if ((open.topology_id == DOLBY_ADM_COPP_TOPOLOGY_ID) ||
			    (open.topology_id == SRS_TRUMEDIA_TOPOLOGY_ID))
				open.topology_id = DEFAULT_COPP_TOPOLOGY;
		}
		open.dev_num_channel = channel_mode & 0x00FF;
		open.bit_width = bits_per_sample;
		WARN_ON(perf_mode == ULTRA_LOW_LATENCY_PCM_MODE &&
							(rate != 48000));
		open.sample_rate  = rate;
		memset(open.dev_channel_mapping, 0, 8);

		if (channel_mode == 1)	{
			open.dev_channel_mapping[0] = PCM_CHANNEL_FC;
		} else if (channel_mode == 2) {
			open.dev_channel_mapping[0] = PCM_CHANNEL_FL;
			open.dev_channel_mapping[1] = PCM_CHANNEL_FR;
		} else if (channel_mode == 3)	{
			open.dev_channel_mapping[0] = PCM_CHANNEL_FL;
			open.dev_channel_mapping[1] = PCM_CHANNEL_FR;
			open.dev_channel_mapping[2] = PCM_CHANNEL_FC;
		} else if (channel_mode == 4) {
			open.dev_channel_mapping[0] = PCM_CHANNEL_FL;
			open.dev_channel_mapping[1] = PCM_CHANNEL_FR;
			open.dev_channel_mapping[2] = PCM_CHANNEL_RB;
			open.dev_channel_mapping[3] = PCM_CHANNEL_LB;
		} else if (channel_mode == 5) {
			open.dev_channel_mapping[0] = PCM_CHANNEL_FL;
			open.dev_channel_mapping[1] = PCM_CHANNEL_FR;
			open.dev_channel_mapping[2] = PCM_CHANNEL_FC;
			open.dev_channel_mapping[3] = PCM_CHANNEL_LB;
			open.dev_channel_mapping[4] = PCM_CHANNEL_RB;
		} else if (channel_mode == 6) {
			open.dev_channel_mapping[0] = PCM_CHANNEL_FL;
			open.dev_channel_mapping[1] = PCM_CHANNEL_FR;
			open.dev_channel_mapping[2] = PCM_CHANNEL_LFE;
			open.dev_channel_mapping[3] = PCM_CHANNEL_FC;
			open.dev_channel_mapping[4] = PCM_CHANNEL_LS;
			open.dev_channel_mapping[5] = PCM_CHANNEL_RS;
		} else if (channel_mode == 8) {
			open.dev_channel_mapping[0] = PCM_CHANNEL_FL;
			open.dev_channel_mapping[1] = PCM_CHANNEL_FR;
			open.dev_channel_mapping[2] = PCM_CHANNEL_LFE;
			open.dev_channel_mapping[3] = PCM_CHANNEL_FC;
			open.dev_channel_mapping[4] = PCM_CHANNEL_LB;
			open.dev_channel_mapping[5] = PCM_CHANNEL_RB;
			open.dev_channel_mapping[6] = PCM_CHANNEL_FLC;
			open.dev_channel_mapping[7] = PCM_CHANNEL_FRC;
		} else {
			pr_err("%s invalid num_chan %d\n", __func__,
					channel_mode);
			return -EINVAL;
		}
		if ((open.dev_num_channel > 2) &&
			multi_ch_map.set_channel_map)
			memcpy(open.dev_channel_mapping,
				multi_ch_map.channel_mapping,
				PCM_FORMAT_MAX_NUM_CHANNEL);

		pr_debug("%s: port_id=%#x rate=%d topology_id=0x%X\n",
			__func__, open.endpoint_id_1, open.sample_rate,
			open.topology_id);

		atomic_set(&this_adm.copp_stat[index], 0);

		ret = apr_send_pkt(this_adm.apr, (uint32_t *)&open);
		if (ret < 0) {
			pr_err("%s:ADM enable for port %#x for[%d] failed\n",
						__func__, tmp_port, port_id);
			ret = -EINVAL;
			goto fail_cmd;
		}
		/* Wait for the callback with copp id */
		ret = wait_event_timeout(this_adm.wait[index],
			atomic_read(&this_adm.copp_stat[index]),
			msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s ADM open failed for port %#x for [%d]\n",
						__func__, tmp_port, port_id);
			ret = -EINVAL;
			goto fail_cmd;
		}
	}
	if (perf_mode == ULTRA_LOW_LATENCY_PCM_MODE ||
			perf_mode == LOW_LATENCY_PCM_MODE) {
		atomic_inc(&this_adm.copp_low_latency_cnt[index]);
		pr_debug("%s: index: %d coppid: %d", __func__, index,
			atomic_read(&this_adm.copp_low_latency_id[index]));
	} else {
		atomic_inc(&this_adm.copp_cnt[index]);
		pr_debug("%s: index: %d coppid: %d", __func__, index,
			atomic_read(&this_adm.copp_id[index]));
	}
	return 0;

fail_cmd:

	return ret;
}

int adm_multi_ch_copp_open(int port_id, int path, int rate, int channel_mode,
			int topology, int perf_mode, uint16_t bits_per_sample)
{
	int ret = 0;

	ret = adm_open(port_id, path, rate, channel_mode,
				   topology, perf_mode, bits_per_sample);

	return ret;
}

int adm_matrix_map(int session_id, int path, int num_copps,
			unsigned int *port_id, int copp_id, int perf_mode)
{
	struct adm_cmd_matrix_map_routings_v5	*route;
	struct adm_session_map_node_v5 *node;
	uint16_t *copps_list;
	int cmd_size = 0;
	int ret = 0, i = 0;
	void *payload = NULL;
	void *matrix_map = NULL;

	/* Assumes port_ids have already been validated during adm_open */
	int index = q6audio_get_port_index(copp_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: invalid port idx %d token %d\n",
					__func__, index, copp_id);
		return 0;
	}
	cmd_size = (sizeof(struct adm_cmd_matrix_map_routings_v5) +
			sizeof(struct adm_session_map_node_v5) +
			(sizeof(uint32_t) * num_copps));
	matrix_map = kzalloc(cmd_size, GFP_KERNEL);
	if (matrix_map == NULL) {
		pr_err("%s: Mem alloc failed\n", __func__);
		ret = -EINVAL;
		return ret;
	}
	route = (struct adm_cmd_matrix_map_routings_v5 *)matrix_map;

	pr_debug("%s: session 0x%x path:%d num_copps:%d port_id[0]:%#x coppid[%d]\n",
		 __func__, session_id, path, num_copps, port_id[0], copp_id);

	route->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	route->hdr.pkt_size = cmd_size;
	route->hdr.src_svc = 0;
	route->hdr.src_domain = APR_DOMAIN_APPS;
	route->hdr.src_port = copp_id;
	route->hdr.dest_svc = APR_SVC_ADM;
	route->hdr.dest_domain = APR_DOMAIN_ADSP;
	if (perf_mode == ULTRA_LOW_LATENCY_PCM_MODE ||
			perf_mode == LOW_LATENCY_PCM_MODE) {
		route->hdr.dest_port =
			atomic_read(&this_adm.copp_low_latency_id[index]);
	} else {
		route->hdr.dest_port = atomic_read(&this_adm.copp_id[index]);
	}
	route->hdr.token = copp_id;
	route->hdr.opcode = ADM_CMD_MATRIX_MAP_ROUTINGS_V5;
	route->num_sessions = 1;

	switch (path) {
	case 0x1:
		route->matrix_id = ADM_MATRIX_ID_AUDIO_RX;
		break;
	case 0x2:
	case 0x3:
		route->matrix_id = ADM_MATRIX_ID_AUDIO_TX;
		break;
	default:
		pr_err("%s: Wrong path set[%d]\n", __func__, path);
		break;
	}
	payload = ((u8 *)matrix_map +
			sizeof(struct adm_cmd_matrix_map_routings_v5));
	node = (struct adm_session_map_node_v5 *)payload;

	node->session_id = session_id;
	node->num_copps = num_copps;
	payload = (u8 *)node + sizeof(struct adm_session_map_node_v5);
	copps_list = (uint16_t *)payload;
	for (i = 0; i < num_copps; i++) {
		int tmp;
		port_id[i] = q6audio_convert_virtual_to_portid(port_id[i]);

		tmp = q6audio_get_port_index(port_id[i]);


		if (tmp >= 0 && tmp < AFE_MAX_PORTS) {
			if (perf_mode == ULTRA_LOW_LATENCY_PCM_MODE ||
					perf_mode == LOW_LATENCY_PCM_MODE)
				copps_list[i] =
				atomic_read(&this_adm.copp_low_latency_id[tmp]);
			else
				copps_list[i] =
					atomic_read(&this_adm.copp_id[tmp]);
		}
		else
			continue;
		pr_debug("%s: port_id[%#x]: %d, index: %d act coppid[0x%x]\n",
			__func__, i, port_id[i], tmp, copps_list[i]);
	}
	atomic_set(&this_adm.copp_stat[index], 0);

	ret = apr_send_pkt(this_adm.apr, (uint32_t *)matrix_map);
	if (ret < 0) {
		pr_err("%s: ADM routing for port %#x failed\n",
					__func__, port_id[0]);
		ret = -EINVAL;
		goto fail_cmd;
	}
	ret = wait_event_timeout(this_adm.wait[index],
				atomic_read(&this_adm.copp_stat[index]),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: ADM cmd Route failed for port %#x\n",
					__func__, port_id[0]);
		ret = -EINVAL;
		goto fail_cmd;
	}

	if (perf_mode != ULTRA_LOW_LATENCY_PCM_MODE) {
		for (i = 0; i < num_copps; i++)
			send_adm_cal(port_id[i], path, perf_mode);

		for (i = 0; i < num_copps; i++) {
			int tmp, copp_id;
			tmp = afe_get_port_index(port_id[i]);
			if (tmp >= 0 && tmp < AFE_MAX_PORTS) {
				if (perf_mode == LEGACY_PCM_MODE)
					copp_id = atomic_read(
					&this_adm.copp_id[tmp]);
				else
					copp_id = atomic_read(
					&this_adm.copp_low_latency_id[tmp]);
				rtac_add_adm_device(port_id[i],
						copp_id, path, session_id);
				pr_debug("%s, copp_id: %d\n",
							__func__, copp_id);
			} else
				pr_debug("%s: Invalid port index %d",
							__func__, tmp);
		}
	}

fail_cmd:
	kfree(matrix_map);
	return ret;
}

int adm_memory_map_regions(int port_id,
		uint32_t *buf_add, uint32_t mempool_id,
		uint32_t *bufsz, uint32_t bufcnt)
{
	struct  avs_cmd_shared_mem_map_regions *mmap_regions = NULL;
	struct  avs_shared_map_region_payload *mregions = NULL;
	void    *mmap_region_cmd = NULL;
	void    *payload = NULL;
	int     ret = 0;
	int     i = 0;
	int     cmd_size = 0;
	int     index = 0;

	pr_debug("%s\n", __func__);
	if (this_adm.apr == NULL) {
		this_adm.apr = apr_register("ADSP", "ADM", adm_callback,
						0xFFFFFFFF, &this_adm);
		if (this_adm.apr == NULL) {
			pr_err("%s: Unable to register ADM\n", __func__);
			ret = -ENODEV;
			return ret;
		}
		rtac_set_adm_handle(this_adm.apr);
	}

	port_id = q6audio_convert_virtual_to_portid(port_id);

	if (q6audio_validate_port(port_id) < 0) {
		pr_err("%s port id[%#x] is invalid\n", __func__, port_id);
		return -ENODEV;
	}

	index = q6audio_get_port_index(port_id);

	cmd_size = sizeof(struct avs_cmd_shared_mem_map_regions)
			+ sizeof(struct avs_shared_map_region_payload)
			* bufcnt;

	mmap_region_cmd = kzalloc(cmd_size, GFP_KERNEL);
	if (!mmap_region_cmd) {
		pr_err("%s: allocate mmap_region_cmd failed\n", __func__);
		return -ENOMEM;
	}
	mmap_regions = (struct avs_cmd_shared_mem_map_regions *)mmap_region_cmd;
	mmap_regions->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
								APR_PKT_VER);
	mmap_regions->hdr.pkt_size = cmd_size;
	mmap_regions->hdr.src_port = 0;
	mmap_regions->hdr.dest_port = atomic_read(&this_adm.copp_id[index]);
	mmap_regions->hdr.token = port_id;
	mmap_regions->hdr.opcode = ADM_CMD_SHARED_MEM_MAP_REGIONS;
	mmap_regions->mem_pool_id = ADSP_MEMORY_MAP_SHMEM8_4K_POOL & 0x00ff;
	mmap_regions->num_regions = bufcnt & 0x00ff;
	mmap_regions->property_flag = 0x00;

	pr_debug("%s: map_regions->num_regions = %d\n", __func__,
				mmap_regions->num_regions);
	payload = ((u8 *) mmap_region_cmd +
				sizeof(struct avs_cmd_shared_mem_map_regions));
	mregions = (struct avs_shared_map_region_payload *)payload;

	for (i = 0; i < bufcnt; i++) {
		mregions->shm_addr_lsw = buf_add[i];
		mregions->shm_addr_msw = 0x00;
		mregions->mem_size_bytes = bufsz[i];
		++mregions;
	}

	atomic_set(&this_adm.copp_stat[index], 0);
	ret = apr_send_pkt(this_adm.apr, (uint32_t *) mmap_region_cmd);
	if (ret < 0) {
		pr_err("%s: mmap_regions op[0x%x]rc[%d]\n", __func__,
					mmap_regions->hdr.opcode, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = wait_event_timeout(this_adm.wait[index],
			atomic_read(&this_adm.copp_stat[index]), 5 * HZ);
	if (!ret) {
		pr_err("%s: timeout. waited for memory_map\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
fail_cmd:
	kfree(mmap_region_cmd);
	return ret;
}

int adm_memory_unmap_regions(int32_t port_id)
{
	struct  avs_cmd_shared_mem_unmap_regions unmap_regions;
	int     ret = 0;
	int     index = 0;

	pr_debug("%s\n", __func__);

	if (this_adm.apr == NULL) {
		pr_err("%s APR handle NULL\n", __func__);
		return -EINVAL;
	}
	port_id = q6audio_convert_virtual_to_portid(port_id);

	if (q6audio_validate_port(port_id) < 0) {
		pr_err("%s port idi[%d] is invalid\n", __func__, port_id);
		return -ENODEV;
	}

	index = q6audio_get_port_index(port_id);

	unmap_regions.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
							APR_PKT_VER);
	unmap_regions.hdr.pkt_size = sizeof(unmap_regions);
	unmap_regions.hdr.src_port = 0;
	unmap_regions.hdr.dest_port = atomic_read(&this_adm.copp_id[index]);
	unmap_regions.hdr.token = port_id;
	unmap_regions.hdr.opcode = ADM_CMD_SHARED_MEM_UNMAP_REGIONS;
	unmap_regions.mem_map_handle = atomic_read(&this_adm.
		mem_map_cal_handles[atomic_read(&this_adm.mem_map_cal_index)]);
	atomic_set(&this_adm.copp_stat[index], 0);
	ret = apr_send_pkt(this_adm.apr, (uint32_t *) &unmap_regions);
	if (ret < 0) {
		pr_err("%s: mmap_regions op[0x%x]rc[%d]\n", __func__,
				unmap_regions.hdr.opcode, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = wait_event_timeout(this_adm.wait[index],
				 atomic_read(&this_adm.copp_stat[index]),
				 5 * HZ);
	if (!ret) {
		pr_err("%s: timeout. waited for memory_unmap index %d\n",
		       __func__, index);
		ret = -EINVAL;
		goto fail_cmd;
	} else {
		pr_debug("%s: Unmap handle 0x%x succeeded\n", __func__,
			 unmap_regions.mem_map_handle);
	}
fail_cmd:
	return ret;
}

#ifdef CONFIG_RTAC
int adm_get_copp_id(int port_index)
{
	int copp_id;
	pr_debug("%s\n", __func__);

	if (port_index < 0) {
		pr_err("%s: invalid port_id = %d\n", __func__, port_index);
		return -EINVAL;
	}

	copp_id = atomic_read(&this_adm.copp_id[port_index]);
	if (copp_id == RESET_COPP_ID)
		copp_id = atomic_read(
			&this_adm.copp_low_latency_id[port_index]);
	return copp_id;
}

int adm_get_lowlatency_copp_id(int port_index)
{
	pr_debug("%s\n", __func__);

	if (port_index < 0) {
		pr_err("%s: invalid port_id = %d\n", __func__, port_index);
		return -EINVAL;
	}

	return atomic_read(&this_adm.copp_low_latency_id[port_index]);
}
#else
int adm_get_copp_id(int port_index)
{
	return -EINVAL;
}

int adm_get_lowlatency_copp_id(int port_index)
{
	return -EINVAL;
}
#endif /* #ifdef CONFIG_RTAC */

void adm_ec_ref_rx_id(int port_id)
{
	this_adm.ec_ref_rx = port_id;
	pr_debug("%s ec_ref_rx:%d", __func__, this_adm.ec_ref_rx);
}

int adm_close(int port_id, int perf_mode)
{
	struct apr_hdr close;

	int ret = 0;
	int index = 0;
	int copp_id = RESET_COPP_ID;

	port_id = q6audio_convert_virtual_to_portid(port_id);

	index = q6audio_get_port_index(port_id);
	if (q6audio_validate_port(port_id) < 0)
		return -EINVAL;

	pr_debug("%s port_id=%#x index %d perf_mode: %d\n", __func__, port_id,
		index, perf_mode);

	if (perf_mode == ULTRA_LOW_LATENCY_PCM_MODE ||
				perf_mode == LOW_LATENCY_PCM_MODE) {
		if (!(atomic_read(&this_adm.copp_low_latency_cnt[index]))) {
			pr_err("%s: copp count for port[%#x]is 0\n", __func__,
				port_id);
			goto fail_cmd;
		}
		atomic_dec(&this_adm.copp_low_latency_cnt[index]);
	} else {
		if (!(atomic_read(&this_adm.copp_cnt[index]))) {
			pr_err("%s: copp count for port[%#x]is 0\n", __func__,
				port_id);
			goto fail_cmd;
		}
		atomic_dec(&this_adm.copp_cnt[index]);
	}
	if ((perf_mode == LEGACY_PCM_MODE &&
		!(atomic_read(&this_adm.copp_cnt[index]))) ||
		((perf_mode != LEGACY_PCM_MODE) &&
		!(atomic_read(&this_adm.copp_low_latency_cnt[index])))) {

		pr_debug("%s:Closing ADM: perf_mode: %d\n", __func__,
				perf_mode);
		close.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
		close.pkt_size = sizeof(close);
		close.src_svc = APR_SVC_ADM;
		close.src_domain = APR_DOMAIN_APPS;
		close.src_port = port_id;
		close.dest_svc = APR_SVC_ADM;
		close.dest_domain = APR_DOMAIN_ADSP;
		if (perf_mode == ULTRA_LOW_LATENCY_PCM_MODE ||
				perf_mode == LOW_LATENCY_PCM_MODE)
			close.dest_port =
			     atomic_read(&this_adm.copp_low_latency_id[index]);
		else
			close.dest_port = atomic_read(&this_adm.copp_id[index]);
		close.token = port_id;
		close.opcode = ADM_CMD_DEVICE_CLOSE_V5;

		atomic_set(&this_adm.copp_stat[index], 0);

		if (perf_mode == ULTRA_LOW_LATENCY_PCM_MODE ||
				perf_mode == LOW_LATENCY_PCM_MODE) {
			copp_id = atomic_read(
				&this_adm.copp_low_latency_id[index]);
			pr_debug("%s:coppid %d portid=%#x index=%d coppcnt=%d\n",
				__func__,
				copp_id,
				port_id, index,
				atomic_read(
					&this_adm.copp_low_latency_cnt[index]));
			atomic_set(&this_adm.copp_low_latency_id[index],
				RESET_COPP_ID);
		} else {
			copp_id = atomic_read(&this_adm.copp_id[index]);
			pr_debug("%s:coppid %d portid=%#x index=%d coppcnt=%d\n",
				__func__,
				copp_id,
				port_id, index,
				atomic_read(&this_adm.copp_cnt[index]));
			atomic_set(&this_adm.copp_id[index],
				RESET_COPP_ID);
		}

		ret = apr_send_pkt(this_adm.apr, (uint32_t *)&close);
		if (ret < 0) {
			pr_err("%s ADM close failed\n", __func__);
			ret = -EINVAL;
			goto fail_cmd;
		}

		ret = wait_event_timeout(this_adm.wait[index],
				atomic_read(&this_adm.copp_stat[index]),
				msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: ADM cmd Route failed for port %#x\n",
						__func__, port_id);
			ret = -EINVAL;
			goto fail_cmd;
		}
	}

	if (perf_mode != ULTRA_LOW_LATENCY_PCM_MODE) {
		pr_debug("%s: remove adm device from rtac\n", __func__);
		rtac_remove_adm_device(port_id, copp_id);
	}

fail_cmd:
	return ret;
}

static int __init adm_init(void)
{
	int i = 0;
	this_adm.apr = NULL;
	this_adm.set_custom_topology = 1;
	this_adm.ec_ref_rx = -1;

	for (i = 0; i < AFE_MAX_PORTS; i++) {
		atomic_set(&this_adm.copp_id[i], RESET_COPP_ID);
		atomic_set(&this_adm.copp_low_latency_id[i], RESET_COPP_ID);
		atomic_set(&this_adm.copp_cnt[i], 0);
		atomic_set(&this_adm.copp_low_latency_cnt[i], 0);
		atomic_set(&this_adm.copp_stat[i], 0);
		atomic_set(&this_adm.copp_perf_mode[i], 0);
		init_waitqueue_head(&this_adm.wait[i]);
	}
	return 0;
}

device_initcall(adm_init);
