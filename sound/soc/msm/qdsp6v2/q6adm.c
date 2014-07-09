/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/qdsp6v2/apr.h>
#include <sound/q6adm-v2.h>
#include <sound/q6audio-v2.h>
#include <sound/q6afe-v2.h>
#include "audio_cal_utils.h"

#include <sound/asound.h>
#include "msm-dts-eagle.h"

#define TIMEOUT_MS 1000

#define RESET_COPP_ID 99
#define INVALID_COPP_ID 0xFF
/* Used for inband payload copy, max size is 4k */
/* 2 is to account for module & param ID in payload */
#define ADM_GET_PARAMETER_LENGTH  (4096 - APR_HDR_SIZE - 2 * sizeof(uint32_t))

#define ULL_SUPPORTED_BITS_PER_SAMPLE 16
#define ULL_SUPPORTED_SAMPLE_RATE 48000

#define CMD_GET_HDR_SZ 16

enum {
	ADM_CUSTOM_TOP_CAL = 0,
	ADM_AUDPROC_CAL,
	ADM_AUDVOL_CAL,
	ADM_RTAC_INFO_CAL,
	ADM_RTAC_APR_CAL,
	ADM_DTS_EAGLE,
	ADM_MAX_CAL_TYPES
};

struct adm_copp {

	atomic_t id[AFE_MAX_PORTS][MAX_COPPS_PER_PORT];
	atomic_t cnt[AFE_MAX_PORTS][MAX_COPPS_PER_PORT];
	atomic_t topology[AFE_MAX_PORTS][MAX_COPPS_PER_PORT];
	atomic_t mode[AFE_MAX_PORTS][MAX_COPPS_PER_PORT];
	atomic_t stat[AFE_MAX_PORTS][MAX_COPPS_PER_PORT];
	atomic_t rate[AFE_MAX_PORTS][MAX_COPPS_PER_PORT];
	atomic_t bit_width[AFE_MAX_PORTS][MAX_COPPS_PER_PORT];
	atomic_t app_type[AFE_MAX_PORTS][MAX_COPPS_PER_PORT];
	wait_queue_head_t wait[AFE_MAX_PORTS][MAX_COPPS_PER_PORT];
};

struct adm_ctl {
	void *apr;

	struct adm_copp copp;

	atomic_t matrix_map_stat;
	wait_queue_head_t matrix_map_wait;

	atomic_t adm_stat;
	wait_queue_head_t adm_wait;

	struct cal_type_data *cal_data[ADM_MAX_CAL_TYPES];

	atomic_t mem_map_cal_handles[ADM_MAX_CAL_TYPES];
	atomic_t mem_map_cal_index;

	struct param_outband outband_memmap;

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

static int adm_get_parameters[MAX_COPPS_PER_PORT*ADM_GET_PARAMETER_LENGTH];

int adm_validate_and_get_port_index(int port_id)
{
	int index;
	int ret;

	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s: port validation failed id 0x%x ret %d\n",
			__func__, port_id, ret);
		return -EINVAL;
	}

	index = afe_get_port_index(port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: Invalid port idx %d port_id 0x%x\n",
			__func__, index,
			port_id);
		return -EINVAL;
	}
	pr_debug("%s: port_idx- %d\n", __func__, index);
	return index;
}

int adm_get_default_copp_idx(int port_id)
{
	int port_idx = adm_validate_and_get_port_index(port_id), idx;

	if (port_idx < 0) {
		pr_err("%s: Invalid port id: 0x%x", __func__, port_id);
		return -EINVAL;
	}
	pr_debug("%s: port_idx:%d\n", __func__, port_idx);
	for (idx = 0; idx < MAX_COPPS_PER_PORT; idx++) {
		if (atomic_read(&this_adm.copp.id[port_idx][idx]) !=
			RESET_COPP_ID)
			return idx;
	}
	return -EINVAL;
}

int adm_get_topology_for_port_from_copp_id(int port_id, int copp_id)
{
	int port_idx = adm_validate_and_get_port_index(port_id), idx;
	if (port_idx < 0) {
		pr_err("%s: Invalid port id: 0x%x", __func__, port_id);
		return 0;
	}
	for (idx = 0; idx < MAX_COPPS_PER_PORT; idx++)
		if (atomic_read(&this_adm.copp.id[port_idx][idx]) == copp_id)
			return atomic_read(&this_adm.copp.topology[port_idx]
								  [idx]);
	pr_err("%s: Invalid copp_id %d port_id 0x%x\n",
		__func__, copp_id, port_id);
	return 0;
}

int adm_get_topology_for_port_copp_idx(int port_id, int copp_idx)
{
	int port_idx = adm_validate_and_get_port_index(port_id);
	if (port_idx < 0) {
		pr_err("%s: Invalid port id: 0x%x", __func__, port_id);
		return 0;
	}
	return atomic_read(&this_adm.copp.topology[port_idx][copp_idx]);
}

int adm_get_indexes_from_copp_id(int copp_id, int *copp_idx, int *port_idx)
{
	int p_idx, c_idx;
	for (p_idx = 0; p_idx < AFE_MAX_PORTS; p_idx++) {
		for (c_idx = 0; c_idx < MAX_COPPS_PER_PORT; c_idx++) {
			if (atomic_read(&this_adm.copp.id[p_idx][c_idx])
								== copp_id) {
				if (copp_idx != NULL)
					*copp_idx = c_idx;
				if (port_idx != NULL)
					*port_idx = p_idx;
				return 0;
			}
		}
	}
	return -EINVAL;
}

static int adm_get_copp_id(int port_idx, int copp_idx)
{
	pr_debug("%s: port_idx:%d copp_idx:%d\n", __func__, port_idx, copp_idx);

	if (copp_idx < 0 || copp_idx >= MAX_COPPS_PER_PORT) {
		pr_err("%s: Invalid copp_num: %d\n", __func__, copp_idx);
		return -EINVAL;
	}
	return atomic_read(&this_adm.copp.id[port_idx][copp_idx]);
}

static int adm_get_idx_if_copp_exists(int port_idx, int topology, int mode,
				 int rate, int bit_width, int app_type)
{
	int idx;

	pr_debug("%s: port_idx-%d, topology-0x%x, mode-%d, rate-%d, bit_width-%d\n",
		 __func__, port_idx, topology, mode, rate, bit_width);

	for (idx = 0; idx < MAX_COPPS_PER_PORT; idx++)
		if ((topology ==
			atomic_read(&this_adm.copp.topology[port_idx][idx])) &&
		    (mode == atomic_read(&this_adm.copp.mode[port_idx][idx])) &&
		    (rate == atomic_read(&this_adm.copp.rate[port_idx][idx])) &&
		    (bit_width ==
			atomic_read(&this_adm.copp.bit_width[port_idx][idx])) &&
		    (app_type ==
			atomic_read(&this_adm.copp.app_type[port_idx][idx])))
			return idx;
	return -EINVAL;
}

static int adm_get_next_available_copp(int port_idx)
{
	int idx;

	pr_debug("%s:\n", __func__);
	for (idx = 0; idx < MAX_COPPS_PER_PORT; idx++) {
		pr_debug("%s: copp_id:0x%x port_idx:%d idx:%d\n", __func__,
			 atomic_read(&this_adm.copp.id[port_idx][idx]),
			 port_idx, idx);
		if (atomic_read(&this_adm.copp.id[port_idx][idx]) ==
								RESET_COPP_ID)
			break;
	}
	return idx;
}

int adm_dts_eagle_set(int port_id, int copp_idx, int param_id,
		      void *data, int size)
{
	struct adm_cmd_set_pp_params_v5	admp;
	int p_idx, ret = 0, *update_params_value;

	pr_debug("DTS_EAGLE_ADM - %s: port id %i, copp idx %i, param id 0x%X\n",
		__func__, port_id, copp_idx, param_id);

	port_id = afe_convert_virtual_to_portid(port_id);
	p_idx = adm_validate_and_get_port_index(port_id);
	pr_debug("DTS_EAGLE_ADM - %s: after lookup, port id %i, port idx %i\n",
		__func__, port_id, p_idx);

	if (p_idx < 0) {
		pr_err("DTS_EAGLE_ADM - %s: invalid port index %i, port id %i, copp idx %i\n",
			__func__, p_idx, port_id, copp_idx);
		return -EINVAL;
	}

	update_params_value = (int *)this_adm.outband_memmap.kvaddr;
	if (update_params_value == NULL) {
		pr_err("DTS_EAGLE_ADM - %s: NULL memmap. Non Eagle topology selected?\n",
				__func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
	*update_params_value++ = AUDPROC_MODULE_ID_DTS_HPX_POSTMIX;
	*update_params_value++ = param_id;
	*update_params_value++ = size;
	memcpy(update_params_value, data, size);

	admp.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
		APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	admp.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE, sizeof(admp));
	admp.hdr.src_svc = APR_SVC_ADM;
	admp.hdr.src_domain = APR_DOMAIN_APPS;
	admp.hdr.src_port = port_id;
	admp.hdr.dest_svc = APR_SVC_ADM;
	admp.hdr.dest_domain = APR_DOMAIN_ADSP;
	admp.hdr.dest_port = atomic_read(&this_adm.copp.id[p_idx][copp_idx]);
	admp.hdr.token = p_idx << 16 | copp_idx;
	admp.hdr.opcode = ADM_CMD_SET_PP_PARAMS_V5;
	admp.payload_addr_lsw = lower_32_bits(this_adm.outband_memmap.paddr);
	admp.payload_addr_msw = upper_32_bits(this_adm.outband_memmap.paddr);
	admp.mem_map_handle = atomic_read(&this_adm.mem_map_cal_handles[
				atomic_read(&this_adm.mem_map_cal_index)]);
	admp.payload_size = size + 12 /*see update_params_value header above*/;

	pr_debug("DTS_EAGLE_ADM - %s: Command was sent now check Q6 - port id = %d, size %d, module id %x, param id %x.\n",
			__func__, admp.hdr.dest_port,
			admp.payload_size, AUDPROC_MODULE_ID_DTS_HPX_POSTMIX,
			param_id);

	ret = apr_send_pkt(this_adm.apr, (uint32_t *)&admp);
	if (ret < 0) {
		pr_err("DTS_EAGLE_ADM - %s: ADM enable for port %d failed\n",
			__func__, port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}
	ret = wait_event_timeout(this_adm.copp.wait[p_idx][copp_idx], 1,
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("DTS_EAGLE_ADM - %s: set params timed out port = %d\n",
			__func__, port_id);
		ret = -EINVAL;
	}

fail_cmd:
	return ret;
}

int adm_dts_eagle_get(int port_id, int copp_idx, int param_id,
		      void *data, int size)
{
	struct adm_cmd_get_pp_params_v5	*admp = NULL;
	int p_idx, sz, ret = 0, orig_size = size;

	pr_debug("DTS_EAGLE_ADM - %s: port id %i, copp idx %i, param id 0x%X\n",
		 __func__, port_id, copp_idx, param_id);

	port_id = afe_convert_virtual_to_portid(port_id);
	p_idx = adm_validate_and_get_port_index(port_id);
	if (p_idx < 0) {
		pr_err("DTS_EAGLE_ADM - %s: invalid port index %i, port id %i, copp idx %i\n",
				__func__, p_idx, port_id, copp_idx);
		return -EINVAL;
	}

	if (size <= 0 || !data) {
		pr_err("DTS_EAGLE_ADM - %s: invalid size %i or pointer %p.\n",
			__func__, size, data);
		return -EINVAL;
	}

	size = (size+3) & 0xFFFFFFFC;

	sz = sizeof(struct adm_cmd_get_pp_params_v5) + size + CMD_GET_HDR_SZ;
	admp = kzalloc(sz, GFP_KERNEL);
	if (!admp) {
		pr_err("DTS_EAGLE_ADM - %s, adm params memory alloc failed",
			__func__);
		return -ENOMEM;
	}

	admp->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	admp->hdr.pkt_size = sz;
	admp->hdr.src_svc = APR_SVC_ADM;
	admp->hdr.src_domain = APR_DOMAIN_APPS;
	admp->hdr.src_port = port_id;
	admp->hdr.dest_svc = APR_SVC_ADM;
	admp->hdr.dest_domain = APR_DOMAIN_ADSP;
	admp->hdr.dest_port = atomic_read(&this_adm.copp.id[p_idx][copp_idx]);
	admp->hdr.token = p_idx << 16 | copp_idx;
	admp->hdr.opcode = ADM_CMD_GET_PP_PARAMS_V5;
	admp->data_payload_addr_lsw = 0;
	admp->data_payload_addr_msw = 0;
	admp->mem_map_handle = 0;
	admp->module_id = AUDPROC_MODULE_ID_DTS_HPX_POSTMIX;
	admp->param_id = param_id;
	admp->param_max_size = size + CMD_GET_HDR_SZ;
	admp->reserved = 0;

	ret = apr_send_pkt(this_adm.apr, (uint32_t *)admp);
	if (ret < 0) {
		pr_err("DTS_EAGLE_ADM - %s: Failed to get EAGLE Params on port %d\n",
			__func__, port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}
	ret = wait_event_timeout(this_adm.copp.wait[p_idx][copp_idx], 1,
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("DTS_EAGLE_ADM - %s: EAGLE get params timed out port = %d\n",
			__func__, port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}

	if (adm_get_parameters[0] > 0 &&
	    (adm_get_parameters[0] * sizeof(int)) == orig_size) {
		ret = 0;
		memcpy(data, &adm_get_parameters[1], orig_size);
	} else {
		ret = -EINVAL;
		pr_err("DTS_EAGLE_ADM - %s: EAGLE get params problem getting data, size was %zu, expected was %i - check callback error value",
				__func__, (adm_get_parameters[0] * sizeof(int)),
				orig_size);
	}
fail_cmd:
	kfree(admp);
	return ret;
}

int srs_trumedia_open(int port_id, int copp_idx, int srs_tech_id,
		      void *srs_params)
{
	struct adm_cmd_set_pp_params_inband_v5 *adm_params = NULL;
	int ret = 0, sz = 0;
	int port_idx;

	pr_debug("SRS - %s", __func__);

	port_id = afe_convert_virtual_to_portid(port_id);
	port_idx = adm_validate_and_get_port_index(port_id);
	if (port_idx < 0) {
		pr_err("%s: Invalid port_id 0x%x\n", __func__, port_id);
		return -EINVAL;
	}
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
	adm_params->hdr.dest_port =
			atomic_read(&this_adm.copp.id[port_idx][copp_idx]);
	adm_params->hdr.token = port_idx << 16 | copp_idx;
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
		pr_err("SRS - %s: ADM enable for port %d failed %d\n",
			__func__, port_id, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}
	/* Wait for the callback with copp id */
	ret = wait_event_timeout(this_adm.copp.wait[port_idx][copp_idx], 1,
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

int adm_set_stereo_to_custom_stereo(int port_id, int copp_idx,
				    unsigned int session_id, char *params,
				    uint32_t params_length)
{
	struct adm_cmd_set_pspd_mtmx_strtr_params_v5 *adm_params = NULL;
	int sz, rc = 0, port_idx;

	pr_debug("%s:\n", __func__);
	port_id = afe_convert_virtual_to_portid(port_id);
	port_idx = adm_validate_and_get_port_index(port_id);
	if (port_idx < 0) {
		pr_err("%s: Invalid port_id 0x%x\n", __func__, port_id);
		return -EINVAL;
	}

	sz = sizeof(struct adm_cmd_set_pspd_mtmx_strtr_params_v5) +
		params_length;
	adm_params = kzalloc(sz, GFP_KERNEL);
	if (!adm_params) {
		pr_err("%s, adm params memory alloc failed\n", __func__);
		return -ENOMEM;
	}

	memcpy(((u8 *)adm_params +
		sizeof(struct adm_cmd_set_pspd_mtmx_strtr_params_v5)),
		params, params_length);
	adm_params->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	adm_params->hdr.pkt_size = sz;
	adm_params->hdr.src_svc = APR_SVC_ADM;
	adm_params->hdr.src_domain = APR_DOMAIN_APPS;
	adm_params->hdr.src_port = port_id;
	adm_params->hdr.dest_svc = APR_SVC_ADM;
	adm_params->hdr.dest_domain = APR_DOMAIN_ADSP;
	adm_params->hdr.dest_port = 0; /* Ignored */;
	adm_params->hdr.token = 0;
	adm_params->hdr.opcode = ADM_CMD_SET_PSPD_MTMX_STRTR_PARAMS_V5;
	adm_params->payload_addr_lsw = 0;
	adm_params->payload_addr_msw = 0;
	adm_params->mem_map_handle = 0;
	adm_params->payload_size = params_length;
	/* direction RX as 0 */
	adm_params->direction = ADM_PATH_PLAYBACK;
	/* session id for this cmd to be applied on */
	adm_params->sessionid = session_id;
	adm_params->deviceid =
			atomic_read(&this_adm.copp.id[port_idx][copp_idx]);
	adm_params->reserved = 0;
	pr_debug("%s: deviceid %d, session_id %d, src_port %d, dest_port %d\n",
		__func__, adm_params->deviceid, adm_params->sessionid,
		adm_params->hdr.src_port, adm_params->hdr.dest_port);
	atomic_set(&this_adm.copp.stat[port_idx][copp_idx], 0);
	rc = apr_send_pkt(this_adm.apr, (uint32_t *)adm_params);
	if (rc < 0) {
		pr_err("%s: Set params failed port = 0x%x rc %d\n",
			__func__, port_id, rc);
		rc = -EINVAL;
		goto set_stereo_to_custom_stereo_return;
	}
	/* Wait for the callback */
	rc = wait_event_timeout(this_adm.matrix_map_wait,
				atomic_read(&this_adm.matrix_map_stat),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!rc) {
		pr_err("%s: Set params timed out port = 0x%x\n", __func__,
			port_id);
		rc = -EINVAL;
		goto set_stereo_to_custom_stereo_return;
	}
	rc = 0;
set_stereo_to_custom_stereo_return:
	kfree(adm_params);
	return rc;
}

int adm_dolby_dap_send_params(int port_id, int copp_idx, char *params,
			      uint32_t params_length)
{
	struct adm_cmd_set_pp_params_v5	*adm_params = NULL;
	int sz, rc = 0;
	int port_idx;

	pr_debug("%s:\n", __func__);
	port_id = afe_convert_virtual_to_portid(port_id);
	port_idx = adm_validate_and_get_port_index(port_id);
	if (port_idx < 0) {
		pr_err("%s: Invalid port_id 0x%x\n", __func__, port_id);
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
	adm_params->hdr.dest_port =
			atomic_read(&this_adm.copp.id[port_idx][copp_idx]);
	adm_params->hdr.token = port_idx << 16 | copp_idx;
	adm_params->hdr.opcode = ADM_CMD_SET_PP_PARAMS_V5;
	adm_params->payload_addr_lsw = 0;
	adm_params->payload_addr_msw = 0;
	adm_params->mem_map_handle = 0;
	adm_params->payload_size = params_length;

	atomic_set(&this_adm.copp.stat[port_idx][copp_idx], 0);
	rc = apr_send_pkt(this_adm.apr, (uint32_t *)adm_params);
	if (rc < 0) {
		pr_err("%s: Set params failed port = 0x%x rc %d\n",
			__func__, port_id, rc);
		rc = -EINVAL;
		goto dolby_dap_send_param_return;
	}
	/* Wait for the callback */
	rc = wait_event_timeout(this_adm.copp.wait[port_idx][copp_idx],
		atomic_read(&this_adm.copp.stat[port_idx][copp_idx]),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!rc) {
		pr_err("%s: Set params timed out port = 0x%x\n",
			 __func__, port_id);
		rc = -EINVAL;
		goto dolby_dap_send_param_return;
	}
	rc = 0;
dolby_dap_send_param_return:
	kfree(adm_params);
	return rc;
}

int adm_get_params(int port_id, int copp_idx, uint32_t module_id,
		   uint32_t param_id, uint32_t params_length, char *params)
{
	struct adm_cmd_get_pp_params_v5 *adm_params = NULL;
	int sz, rc = 0, i = 0;
	int port_idx, idx;
	int *params_data = (int *)params;

	port_id = afe_convert_virtual_to_portid(port_id);
	port_idx = adm_validate_and_get_port_index(port_id);
	if (port_idx < 0) {
		pr_err("%s: Invalid port_id 0x%x\n", __func__, port_id);
		return -EINVAL;
	}

	sz = sizeof(struct adm_cmd_get_pp_params_v5) + params_length;
	adm_params = kzalloc(sz, GFP_KERNEL);
	if (!adm_params) {
		pr_err("%s: adm params memory alloc failed", __func__);
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
	adm_params->hdr.dest_port =
			atomic_read(&this_adm.copp.id[port_idx][copp_idx]);
	adm_params->hdr.token = port_idx << 16 | copp_idx;
	adm_params->hdr.opcode = ADM_CMD_GET_PP_PARAMS_V5;
	adm_params->data_payload_addr_lsw = 0;
	adm_params->data_payload_addr_msw = 0;
	adm_params->mem_map_handle = 0;
	adm_params->module_id = module_id;
	adm_params->param_id = param_id;
	adm_params->param_max_size = params_length;
	adm_params->reserved = 0;

	atomic_set(&this_adm.copp.stat[port_idx][copp_idx], 0);
	rc = apr_send_pkt(this_adm.apr, (uint32_t *)adm_params);
	if (rc < 0) {
		pr_err("%s: Failed to Get Params on port_id 0x%x %d\n",
			__func__, port_id, rc);
		rc = -EINVAL;
		goto adm_get_param_return;
	}
	/* Wait for the callback with copp id */
	rc = wait_event_timeout(this_adm.copp.wait[port_idx][copp_idx],
	atomic_read(&this_adm.copp.stat[port_idx][copp_idx]),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!rc) {
		pr_err("%s: get params timed out port_id = 0x%x\n", __func__,
			port_id);
		rc = -EINVAL;
		goto adm_get_param_return;
	}

	idx = ADM_GET_PARAMETER_LENGTH * copp_idx;

	if (adm_get_parameters[idx] < 0) {
		pr_err("%s: Size is invalid %d\n", __func__,
			adm_get_parameters[idx]);
		rc = -EINVAL;
		goto adm_get_param_return;
	}
	if (params_data) {
		for (i = 0; i < adm_get_parameters[idx]; i++)
			params_data[i] = adm_get_parameters[1+i+idx];

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
		pr_debug("%s: code = 0x%x PL#0[0x%x], PL#1[0x%x], size = %d\n",
			__func__, data->opcode, payload[0], payload[1],
			data->payload_size);
	else if (data->payload_size >= 4)
		pr_debug("%s: code = 0x%x PL#0[0x%x], size = %d\n",
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
	int i, j, port_idx, copp_idx, idx;

	if (data == NULL) {
		pr_err("%s: data paramter is null\n", __func__);
		return -EINVAL;
	}

	payload = data->payload;

	if (data->opcode == RESET_EVENTS) {
		pr_debug("%s: Reset event is received: %d %d apr[%p]\n",
			__func__,
			data->reset_event, data->reset_proc, this_adm.apr);
		if (this_adm.apr) {
			apr_reset(this_adm.apr);
			for (i = 0; i < AFE_MAX_PORTS; i++) {
				for (j = 0; j < MAX_COPPS_PER_PORT; j++) {
					atomic_set(&this_adm.copp.id[i][j],
						   RESET_COPP_ID);
					atomic_set(&this_adm.copp.cnt[i][j], 0);
					atomic_set(
					   &this_adm.copp.topology[i][j], 0);
					atomic_set(&this_adm.copp.mode[i][j],
						   0);
					atomic_set(&this_adm.copp.stat[i][j],
						   0);
					atomic_set(&this_adm.copp.rate[i][j],
						   0);
					atomic_set(
					    &this_adm.copp.bit_width[i][j], 0);
					atomic_set(
					    &this_adm.copp.app_type[i][j], 0);
				}
			}
			this_adm.apr = NULL;
			cal_utils_clear_cal_block_q6maps(ADM_MAX_CAL_TYPES,
				this_adm.cal_data);
			mutex_lock(&this_adm.cal_data
				[ADM_CUSTOM_TOP_CAL]->lock);
			this_adm.set_custom_topology = 1;
			mutex_unlock(&this_adm.cal_data[
				ADM_CUSTOM_TOP_CAL]->lock);
			rtac_clear_mapping(ADM_RTAC_CAL);
		}
		return 0;
	}

	adm_callback_debug_print(data);
	if (data->payload_size) {
		copp_idx = (data->token) & 0XFF;
		port_idx = ((data->token) >> 16) & 0xFF;
		if (port_idx < 0 || port_idx >= AFE_MAX_PORTS) {
			pr_err("%s: Invalid port idx %d token %d\n",
				__func__, port_idx, data->token);
			return 0;
		}
		if (copp_idx < 0 || copp_idx >= MAX_COPPS_PER_PORT) {
			pr_err("%s: Invalid copp idx %d token %d\n",
				__func__, copp_idx, data->token);
			return 0;
		}
		if (data->opcode == APR_BASIC_RSP_RESULT) {
			pr_debug("%s: APR_BASIC_RSP_RESULT id 0x%x\n",
				__func__, payload[0]);
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
				pr_debug("%s: Basic callback received, wake up.\n",
					__func__);
				atomic_set(&this_adm.copp.stat[port_idx]
							      [copp_idx], 1);
				wake_up(
				&this_adm.copp.wait[port_idx][copp_idx]);
				break;
			case ADM_CMD_ADD_TOPOLOGIES:
				pr_debug("%s: callback received, ADM_CMD_ADD_TOPOLOGIES.\n",
					__func__);
				atomic_set(&this_adm.adm_stat, 1);
				wake_up(&this_adm.adm_wait);
				break;
			case ADM_CMD_MATRIX_MAP_ROUTINGS_V5:
				pr_debug("%s: Basic callback received, wake up.\n",
					__func__);
				atomic_set(&this_adm.matrix_map_stat, 1);
				wake_up(&this_adm.matrix_map_wait);
				break;
			case ADM_CMD_SHARED_MEM_UNMAP_REGIONS:
				pr_debug("%s: ADM_CMD_SHARED_MEM_UNMAP_REGIONS\n",
					__func__);
				atomic_set(&this_adm.adm_stat, 1);
				wake_up(&this_adm.adm_wait);
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
					atomic_set(&this_adm.adm_stat, 1);
					wake_up(&this_adm.adm_wait);
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
			case ADM_CMD_SET_PSPD_MTMX_STRTR_PARAMS_V5:
				pr_debug("%s: ADM_CMD_SET_PSPD_MTMX_STRTR_PARAMS_V5\n",
					__func__);
				atomic_set(&this_adm.copp.stat[port_idx]
							      [copp_idx], 1);
				wake_up(
				&this_adm.copp.wait[port_idx][copp_idx]);
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
				atomic_set(&this_adm.copp.stat[port_idx]
							      [copp_idx], 1);
				wake_up(
				&this_adm.copp.wait[port_idx][copp_idx]);
				break;
			}
			atomic_set(&this_adm.copp.stat[port_idx][copp_idx], 1);
			atomic_set(&this_adm.copp.id[port_idx][copp_idx],
				   open->copp_id);
			pr_debug("%s: coppid rxed=%d\n", __func__,
				 open->copp_id);
			wake_up(&this_adm.copp.wait[port_idx][copp_idx]);
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

			idx = ADM_GET_PARAMETER_LENGTH * copp_idx;
			if (payload[0] == 0) {
				if (data->payload_size >
				    (4 * sizeof(uint32_t))) {
					adm_get_parameters[idx] = payload[3];
					pr_debug("GET_PP PARAM:received parameter length: 0x%x\n",
						adm_get_parameters[idx]);
					/* storing param size then params */
					for (i = 0; i < payload[3]; i++)
						adm_get_parameters[idx+1+i] =
								payload[4+i];
				}
			} else {
				adm_get_parameters[idx] = -1;
				pr_err("%s: GET_PP_PARAMS failed, setting size to %d\n",
					__func__, adm_get_parameters[idx]);
			}
			atomic_set(&this_adm.copp.stat[port_idx][copp_idx], 1);
			wake_up(&this_adm.copp.wait[port_idx][copp_idx]);
			break;
		case ADM_CMDRSP_SHARED_MEM_MAP_REGIONS:
			pr_debug("%s: ADM_CMDRSP_SHARED_MEM_MAP_REGIONS\n",
				__func__);
			atomic_set(&this_adm.mem_map_cal_handles[
				   atomic_read(&this_adm.mem_map_cal_index)],
				   *payload);
			atomic_set(&this_adm.adm_stat, 1);
			wake_up(&this_adm.adm_wait);
			break;
		default:
			pr_err("%s: Unknown cmd:0x%x\n", __func__,
				data->opcode);
			break;
		}
	}
	return 0;
}

static int adm_memory_map_regions(phys_addr_t *buf_add, uint32_t mempool_id,
			   uint32_t *bufsz, uint32_t bufcnt)
{
	struct  avs_cmd_shared_mem_map_regions *mmap_regions = NULL;
	struct  avs_shared_map_region_payload *mregions = NULL;
	void    *mmap_region_cmd = NULL;
	void    *payload = NULL;
	int     ret = 0;
	int     i = 0;
	int     cmd_size = 0;

	pr_debug("%s:\n", __func__);
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

	mmap_regions->hdr.dest_port = 0;
	mmap_regions->hdr.token = 0;
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
		mregions->shm_addr_lsw = lower_32_bits(buf_add[i]);
		mregions->shm_addr_msw = upper_32_bits(buf_add[i]);
		mregions->mem_size_bytes = bufsz[i];
		++mregions;
	}

	atomic_set(&this_adm.adm_stat, 0);
	ret = apr_send_pkt(this_adm.apr, (uint32_t *) mmap_region_cmd);
	if (ret < 0) {
		pr_err("%s: mmap_regions op[0x%x]rc[%d]\n", __func__,
					mmap_regions->hdr.opcode, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = wait_event_timeout(this_adm.adm_wait,
				 atomic_read(&this_adm.adm_stat),
				 5 * HZ);
	if (!ret) {
		pr_err("%s: timeout. waited for memory_map\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
fail_cmd:
	kfree(mmap_region_cmd);
	return ret;
}

static int adm_memory_unmap_regions(void)
{
	struct  avs_cmd_shared_mem_unmap_regions unmap_regions;
	int     ret = 0;

	pr_debug("%s:\n", __func__);
	if (this_adm.apr == NULL) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}

	unmap_regions.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
							APR_PKT_VER);
	unmap_regions.hdr.pkt_size = sizeof(unmap_regions);
	unmap_regions.hdr.src_port = 0;
	unmap_regions.hdr.dest_port = 0;
	unmap_regions.hdr.token = 0;
	unmap_regions.hdr.opcode = ADM_CMD_SHARED_MEM_UNMAP_REGIONS;
	unmap_regions.mem_map_handle = atomic_read(&this_adm.
		mem_map_cal_handles[atomic_read(&this_adm.mem_map_cal_index)]);
	atomic_set(&this_adm.adm_stat, 0);
	ret = apr_send_pkt(this_adm.apr, (uint32_t *) &unmap_regions);
	if (ret < 0) {
		pr_err("%s: mmap_regions op[0x%x]rc[%d]\n", __func__,
				unmap_regions.hdr.opcode, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = wait_event_timeout(this_adm.adm_wait,
				 atomic_read(&this_adm.adm_stat),
				 5 * HZ);
	if (!ret) {
		pr_err("%s: timeout. waited for memory_unmap\n",
		       __func__);
		ret = -EINVAL;
		goto fail_cmd;
	} else {
		pr_debug("%s: Unmap handle 0x%x succeeded\n", __func__,
			 unmap_regions.mem_map_handle);
	}
fail_cmd:
	return ret;
}

static void remap_cal_data(struct cal_block_data *cal_block, int cal_index)
{
	int ret = 0;

	if ((cal_block->map_data.map_size > 0) &&
		(cal_block->map_data.q6map_handle == 0)) {
		atomic_set(&this_adm.mem_map_cal_index, cal_index);
		ret = adm_memory_map_regions(&cal_block->cal_data.paddr, 0,
				(uint32_t *)&cal_block->map_data.map_size, 1);
		if (ret < 0) {
			pr_err("%s: ADM mmap did not work! size = %zd ret %d\n",
				__func__,
				cal_block->map_data.map_size, ret);
			pr_debug("%s: ADM mmap did not work! addr = 0x%pa, size = %zd ret %d\n",
				__func__,
				&cal_block->cal_data.paddr,
				cal_block->map_data.map_size, ret);
			goto done;
		}
		cal_block->map_data.q6map_handle = atomic_read(&this_adm.
			mem_map_cal_handles[cal_index]);
	}
done:
	return;
}

static void send_adm_custom_topology(void)
{
	struct cal_block_data		*cal_block = NULL;
	struct cmd_set_topologies	adm_top;
	int				cal_index = ADM_CUSTOM_TOP_CAL;
	int				result;

	if (this_adm.cal_data[cal_index] == NULL)
		goto done;

	mutex_lock(&this_adm.cal_data[cal_index]->lock);
	if (!this_adm.set_custom_topology)
		goto unlock;
	this_adm.set_custom_topology = 0;

	cal_block = cal_utils_get_only_cal_block(this_adm.cal_data[cal_index]);
	if (cal_block == NULL)
		goto unlock;

	pr_debug("%s: Sending cal_index %d\n", __func__, cal_index);

	remap_cal_data(cal_block, cal_index);
	atomic_set(&this_adm.mem_map_cal_index, cal_index);
	atomic_set(&this_adm.mem_map_cal_handles[cal_index],
		cal_block->map_data.q6map_handle);

	if (cal_block->cal_data.size == 0) {
		pr_debug("%s: No ADM cal to send\n", __func__);
		goto unlock;
	}

	adm_top.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
		APR_HDR_LEN(20), APR_PKT_VER);
	adm_top.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		sizeof(adm_top));
	adm_top.hdr.src_svc = APR_SVC_ADM;
	adm_top.hdr.src_domain = APR_DOMAIN_APPS;
	adm_top.hdr.src_port = 0;
	adm_top.hdr.dest_svc = APR_SVC_ADM;
	adm_top.hdr.dest_domain = APR_DOMAIN_ADSP;
	adm_top.hdr.dest_port = 0;
	adm_top.hdr.token = 0;
	adm_top.hdr.opcode = ADM_CMD_ADD_TOPOLOGIES;
	adm_top.payload_addr_lsw = lower_32_bits(cal_block->cal_data.paddr);
	adm_top.payload_addr_msw = upper_32_bits(cal_block->cal_data.paddr);
	adm_top.mem_map_handle = cal_block->map_data.q6map_handle;
	adm_top.payload_size = cal_block->cal_data.size;

	atomic_set(&this_adm.adm_stat, 0);
	pr_debug("%s: Sending ADM_CMD_ADD_TOPOLOGIES payload = 0x%pa, size = %d\n",
		__func__, &cal_block->cal_data.paddr,
		adm_top.payload_size);
	result = apr_send_pkt(this_adm.apr, (uint32_t *)&adm_top);
	if (result < 0) {
		pr_err("%s: Set topologies failed payload size = 0x%zd result %d\n",
			__func__, cal_block->cal_data.size, result);
		goto unlock;
	}
	/* Wait for the callback */
	result = wait_event_timeout(this_adm.adm_wait,
				    atomic_read(&this_adm.adm_stat),
				    msecs_to_jiffies(TIMEOUT_MS));
	if (!result) {
		pr_err("%s: Set topologies timed out payload size = 0x%zd\n",
			__func__, cal_block->cal_data.size);
		goto unlock;
	}
unlock:
	mutex_unlock(&this_adm.cal_data[cal_index]->lock);
done:
	return;
}

static int send_adm_cal_block(int port_id, int copp_idx,
			      struct cal_block_data *cal_block, int perf_mode,
			      int app_type, int acdb_id)
{
	s32				result = 0;
	struct adm_cmd_set_pp_params_v5	adm_params;
	int port_idx;

	pr_debug("%s: Port id 0x%x,\n", __func__, port_id);
	port_id = afe_convert_virtual_to_portid(port_id);
	port_idx = adm_validate_and_get_port_index(port_id);
	if (port_idx < 0) {
		pr_err("%s: Invalid port_id 0x%x\n", __func__, port_id);
		return -EINVAL;
	}
	if (!cal_block) {
		pr_debug("%s: No ADM cal to send for port_id = 0x%x!\n",
			__func__, port_id);
		result = -EINVAL;
		goto done;
	}
	if (cal_block->cal_data.size <= 0) {
		pr_debug("%s: No ADM cal send for port_id = 0x%x!\n",
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

	adm_params.hdr.token = port_idx << 16 | copp_idx;
	adm_params.hdr.dest_port =
			atomic_read(&this_adm.copp.id[port_idx][copp_idx]);
	adm_params.hdr.opcode = ADM_CMD_SET_PP_PARAMS_V5;
	adm_params.payload_addr_lsw = lower_32_bits(cal_block->cal_data.paddr);
	adm_params.payload_addr_msw = upper_32_bits(cal_block->cal_data.paddr);
	adm_params.mem_map_handle = cal_block->map_data.q6map_handle;
	adm_params.payload_size = cal_block->cal_data.size;

	atomic_set(&this_adm.copp.stat[port_idx][copp_idx], 0);
	pr_debug("%s: Sending SET_PARAMS payload = 0x%pa, size = %d\n",
		__func__, &cal_block->cal_data.paddr,
		adm_params.payload_size);
	result = apr_send_pkt(this_adm.apr, (uint32_t *)&adm_params);
	if (result < 0) {
		pr_err("%s: Set params failed port 0x%x result %d\n",
				__func__, port_id, result);
		pr_debug("%s: Set params failed port = 0x%x payload = 0x%pa result %d\n",
			__func__, port_id, &cal_block->cal_data.paddr, result);
		result = -EINVAL;
		goto done;
	}
	/* Wait for the callback */
	result = wait_event_timeout(this_adm.copp.wait[port_idx][copp_idx],
		atomic_read(&this_adm.copp.stat[port_idx][copp_idx]),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!result) {
		pr_err("%s: Set params timed out port = 0x%x\n",
				__func__, port_id);
		pr_debug("%s: Set params timed out port = 0x%x, payload = 0x%pa\n",
			__func__, port_id, &cal_block->cal_data.paddr);
		result = -EINVAL;
		goto done;
	}

done:
	return result;
}

static struct cal_block_data *adm_find_cal_by_path(int cal_index, int path)
{
	struct list_head		*ptr, *next;
	struct cal_block_data		*cal_block = NULL;
	struct audio_cal_info_audproc	*audproc_cal_info = NULL;
	struct audio_cal_info_audvol	*audvol_cal_info = NULL;
	pr_debug("%s:\n", __func__);

	list_for_each_safe(ptr, next,
		&this_adm.cal_data[cal_index]->cal_blocks) {

		cal_block = list_entry(ptr,
			struct cal_block_data, list);

		if (cal_index == ADM_AUDPROC_CAL) {
			audproc_cal_info = cal_block->cal_info;
			if (audproc_cal_info->path == path)
				return cal_block;
		} else if (cal_index == ADM_AUDVOL_CAL) {
			audvol_cal_info = cal_block->cal_info;
			if (audvol_cal_info->path == path)
				return cal_block;
		}
	}
	pr_debug("%s: Can't find topology for cal_index %d, path %d\n",
		__func__, cal_index, path);
	return NULL;
}

static struct cal_block_data *adm_find_cal(int cal_index, int path,
					   int app_type, int acdb_id)
{
	struct list_head		*ptr, *next;
	struct cal_block_data		*cal_block = NULL;
	struct audio_cal_info_audproc	*audproc_cal_info = NULL;
	struct audio_cal_info_audvol	*audvol_cal_info = NULL;
	pr_debug("%s:\n", __func__);

	list_for_each_safe(ptr, next,
		&this_adm.cal_data[cal_index]->cal_blocks) {

		cal_block = list_entry(ptr,
			struct cal_block_data, list);

		if (cal_index == ADM_AUDPROC_CAL) {
			audproc_cal_info = cal_block->cal_info;
			if ((audproc_cal_info->path == path) &&
			    (audproc_cal_info->app_type == app_type) &&
			    (audproc_cal_info->acdb_id == acdb_id))
				return cal_block;
		} else if (cal_index == ADM_AUDVOL_CAL) {
			audvol_cal_info = cal_block->cal_info;
			if ((audvol_cal_info->path == path) &&
			    (audvol_cal_info->app_type == app_type) &&
			    (audvol_cal_info->acdb_id == acdb_id))
				return cal_block;
		}
	}
	pr_debug("%s: Can't find topology for cal_index %d, path %d, app %d, acdb_id %d defaulting to search by path\n",
		__func__, cal_index, path, app_type, acdb_id);
	return adm_find_cal_by_path(cal_index, path);
}

static void send_adm_cal_type(int cal_index, int path, int port_id,
			      int copp_idx, int perf_mode, int app_type,
			      int acdb_id)
{
	struct cal_block_data		*cal_block = NULL;
	int ret;

	pr_debug("%s:\n, cal index %d", __func__, cal_index);

	if (this_adm.cal_data[cal_index] == NULL) {
		pr_debug("%s: cal_index %d not allocated!\n",
			__func__, cal_index);
		goto done;
	}

	mutex_lock(&this_adm.cal_data[cal_index]->lock);
	cal_block = adm_find_cal(cal_index, path, app_type, acdb_id);
	if (cal_block == NULL)
		goto unlock;

	pr_debug("%s: Sending cal_index cal %d\n", __func__, cal_index);
	remap_cal_data(cal_block, cal_index);
	ret = send_adm_cal_block(port_id, copp_idx, cal_block, perf_mode,
				app_type, acdb_id);
	if (ret < 0)
		pr_debug("%s: No cal sent for cal_index %d, port_id = 0x%x! ret %d\n",
			__func__, cal_index, port_id, ret);
unlock:
	mutex_unlock(&this_adm.cal_data[cal_index]->lock);
done:
	return;
}

static int get_cal_path(int path)
{
	if (path == 0x1)
		return RX_DEVICE;
	else
		return TX_DEVICE;
}

static void send_adm_cal(int port_id, int copp_idx, int path, int perf_mode,
			 int app_type, int acdb_id)
{
	pr_debug("%s:\n", __func__);

	send_adm_cal_type(ADM_AUDPROC_CAL, path, port_id, copp_idx, perf_mode,
			  app_type, acdb_id);
	send_adm_cal_type(ADM_AUDVOL_CAL, path, port_id, copp_idx, perf_mode,
			  app_type, acdb_id);
	return;
}

int adm_connect_afe_port(int mode, int session_id, int port_id)
{
	struct adm_cmd_connect_afe_port_v5	cmd;
	int ret = 0;
	int port_idx, copp_idx = 0;

	pr_debug("%s: port_id: 0x%x session id:%d mode:%d\n", __func__,
				port_id, session_id, mode);

	port_id = afe_convert_virtual_to_portid(port_id);
	port_idx = adm_validate_and_get_port_index(port_id);
	if (port_idx < 0) {
		pr_err("%s: Invalid port_id 0x%x\n", __func__, port_id);
		return -EINVAL;
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
	pr_debug("%s: Port ID 0x%x, index %d\n", __func__, port_id, port_idx);

	cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cmd.hdr.pkt_size = sizeof(cmd);
	cmd.hdr.src_svc = APR_SVC_ADM;
	cmd.hdr.src_domain = APR_DOMAIN_APPS;
	cmd.hdr.src_port = port_id;
	cmd.hdr.dest_svc = APR_SVC_ADM;
	cmd.hdr.dest_domain = APR_DOMAIN_ADSP;
	cmd.hdr.dest_port = 0; /* Ignored */
	cmd.hdr.token = port_idx << 16 | copp_idx;
	cmd.hdr.opcode = ADM_CMD_CONNECT_AFE_PORT_V5;

	cmd.mode = mode;
	cmd.session_id = session_id;
	cmd.afe_port_id = port_id;

	atomic_set(&this_adm.copp.stat[port_idx][copp_idx], 0);
	ret = apr_send_pkt(this_adm.apr, (uint32_t *)&cmd);
	if (ret < 0) {
		pr_err("%s: ADM enable for port_id: 0x%x failed ret %d\n",
					__func__, port_id, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}
	/* Wait for the callback with copp id */
	ret = wait_event_timeout(this_adm.copp.wait[port_idx][copp_idx],
		atomic_read(&this_adm.copp.stat[port_idx][copp_idx]),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: ADM connect timedout for port_id: 0x%x\n",
			__func__, port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}
	atomic_inc(&this_adm.copp.cnt[port_idx][copp_idx]);
	return 0;

fail_cmd:

	return ret;
}

int adm_open(int port_id, int path, int rate, int channel_mode,
	int topology, int perf_mode, uint16_t bit_width, int app_type)
{
	struct adm_cmd_device_open_v5	open;
	int ret = 0;
	int port_idx, copp_idx, flags;
	int tmp_port = q6audio_get_port_id(port_id);

	pr_debug("%s: port_id: 0x%x path:%d rate:%d mode:%d perf_mode:%d\n",
		 __func__, port_id, path, rate, channel_mode, perf_mode);

	port_id = q6audio_convert_virtual_to_portid(port_id);
	port_idx = adm_validate_and_get_port_index(port_id);
	if (port_idx < 0) {
		pr_err("%s: Invalid port_id 0x%x\n", __func__, port_id);
		return -EINVAL;
	}

	if (this_adm.apr == NULL) {
		this_adm.apr = apr_register("ADSP", "ADM", adm_callback,
						0xFFFFFFFF, &this_adm);
		if (this_adm.apr == NULL) {
			pr_err("%s: Unable to register ADM\n", __func__);
			return -ENODEV;
		}
		rtac_set_adm_handle(this_adm.apr);
	}

	if (perf_mode == ULTRA_LOW_LATENCY_PCM_MODE) {
		flags = ADM_ULTRA_LOW_LATENCY_DEVICE_SESSION;
		topology = NULL_COPP_TOPOLOGY;
		rate = ULL_SUPPORTED_SAMPLE_RATE;
		bit_width = ULL_SUPPORTED_BITS_PER_SAMPLE;
	} else if (perf_mode == LOW_LATENCY_PCM_MODE) {
		flags = ADM_LOW_LATENCY_DEVICE_SESSION;
		if ((topology == DOLBY_ADM_COPP_TOPOLOGY_ID) ||
		    (topology == SRS_TRUMEDIA_TOPOLOGY_ID))
			topology = DEFAULT_COPP_TOPOLOGY;
	} else
		flags = ADM_LEGACY_DEVICE_SESSION;

	if ((topology == VPM_TX_SM_ECNS_COPP_TOPOLOGY) ||
	    (topology == VPM_TX_DM_FLUENCE_COPP_TOPOLOGY) ||
	    (topology == VPM_TX_DM_RFECNS_COPP_TOPOLOGY))
		rate = 16000;

	copp_idx = adm_get_idx_if_copp_exists(port_idx, topology, perf_mode,
						rate, bit_width, app_type);
	if (copp_idx < 0) {
		copp_idx = adm_get_next_available_copp(port_idx);
		if (copp_idx >= MAX_COPPS_PER_PORT) {
			pr_err("%s: exceeded copp id %d\n",
				 __func__, copp_idx);
			return -EINVAL;
		} else {
			atomic_set(&this_adm.copp.cnt[port_idx][copp_idx], 0);
			atomic_set(&this_adm.copp.topology[port_idx][copp_idx],
				   topology);
			atomic_set(&this_adm.copp.mode[port_idx][copp_idx],
				   perf_mode);
			atomic_set(&this_adm.copp.rate[port_idx][copp_idx],
				   rate);
			atomic_set(&this_adm.copp.bit_width[port_idx][copp_idx],
				   bit_width);
			send_adm_custom_topology();
		}
	}

	if ((topology == ADM_CMD_COPP_OPEN_TOPOLOGY_ID_DTS_HPX_0 ||
	     topology == ADM_CMD_COPP_OPEN_TOPOLOGY_ID_DTS_HPX_1) &&
	    !perf_mode) {
		int res;
		atomic_set(&this_adm.mem_map_cal_index, ADM_DTS_EAGLE);
		msm_dts_ion_memmap(&this_adm.outband_memmap);
		res = adm_memory_map_regions(&this_adm.outband_memmap.paddr, 0,
				(uint32_t *)&this_adm.outband_memmap.size, 1);
		if (res < 0)
			pr_err("%s: DTS_EAGLE mmap did not work!", __func__);
	}

	/* Create a COPP if port id are not enabled */
	if (atomic_read(&this_adm.copp.cnt[port_idx][copp_idx]) == 0) {
		pr_debug("%s: open ADM: port_idx: %d, copp_idx: %d\n", __func__,
			 port_idx, copp_idx);
		open.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						   APR_HDR_LEN(APR_HDR_SIZE),
						   APR_PKT_VER);
		open.hdr.pkt_size = sizeof(open);
		open.hdr.src_svc = APR_SVC_ADM;
		open.hdr.src_domain = APR_DOMAIN_APPS;
		open.hdr.src_port = tmp_port;
		open.hdr.dest_svc = APR_SVC_ADM;
		open.hdr.dest_domain = APR_DOMAIN_ADSP;
		open.hdr.dest_port = tmp_port;
		open.hdr.token = port_idx << 16 | copp_idx;
		open.hdr.opcode = ADM_CMD_DEVICE_OPEN_V5;
		open.flags = flags;
		open.mode_of_operation = path;
		open.endpoint_id_1 = tmp_port;

		if (this_adm.ec_ref_rx == -1) {
			open.endpoint_id_2 = 0xFFFF;
		} else if (this_adm.ec_ref_rx && (path != 1)) {
			open.endpoint_id_2 = this_adm.ec_ref_rx;
			this_adm.ec_ref_rx = -1;
		}

		open.topology_id = topology;
		open.dev_num_channel = channel_mode & 0x00FF;
		open.bit_width = bit_width;
		WARN_ON((perf_mode == ULTRA_LOW_LATENCY_PCM_MODE) &&
			(rate != ULL_SUPPORTED_SAMPLE_RATE));
		open.sample_rate  = rate;
		memset(open.dev_channel_mapping, 0, PCM_FORMAT_MAX_NUM_CHANNEL);

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
			pr_err("%s: invalid num_chan %d\n", __func__,
					channel_mode);
			return -EINVAL;
		}
		if ((open.dev_num_channel > 2) && multi_ch_map.set_channel_map)
			memcpy(open.dev_channel_mapping,
			       multi_ch_map.channel_mapping,
			       PCM_FORMAT_MAX_NUM_CHANNEL);

		pr_debug("%s: port_id=0x%x rate=%d topology_id=0x%X\n",
			__func__, open.endpoint_id_1, open.sample_rate,
			open.topology_id);

		atomic_set(&this_adm.copp.stat[port_idx][copp_idx], 0);

		ret = apr_send_pkt(this_adm.apr, (uint32_t *)&open);
		if (ret < 0) {
			pr_err("%s: port_id: 0x%x for[0x%x] failed %d\n",
			__func__, tmp_port, port_id, ret);
			return -EINVAL;
		}
		/* Wait for the callback with copp id */
		ret = wait_event_timeout(this_adm.copp.wait[port_idx][copp_idx],
			atomic_read(&this_adm.copp.stat[port_idx][copp_idx]),
			msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: ADM open timedout for port_id: 0x%x for [0x%x]\n",
						__func__, tmp_port, port_id);
			return -EINVAL;
		}
	}
	atomic_inc(&this_adm.copp.cnt[port_idx][copp_idx]);
	return copp_idx;
}

int adm_matrix_map(int path, struct route_payload payload_map, int perf_mode)
{
	struct adm_cmd_matrix_map_routings_v5	*route;
	struct adm_session_map_node_v5 *node;
	uint16_t *copps_list;
	int cmd_size = 0;
	int ret = 0, i = 0;
	void *payload = NULL;
	void *matrix_map = NULL;
	int port_idx, copp_idx;

	/* Assumes port_ids have already been validated during adm_open */
	cmd_size = (sizeof(struct adm_cmd_matrix_map_routings_v5) +
			sizeof(struct adm_session_map_node_v5) +
			(sizeof(uint32_t) * payload_map.num_copps));
	matrix_map = kzalloc(cmd_size, GFP_KERNEL);
	if (matrix_map == NULL) {
		pr_err("%s: Mem alloc failed\n", __func__);
		ret = -EINVAL;
		return ret;
	}
	route = (struct adm_cmd_matrix_map_routings_v5 *)matrix_map;

	route->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	route->hdr.pkt_size = cmd_size;
	route->hdr.src_svc = 0;
	route->hdr.src_domain = APR_DOMAIN_APPS;
	route->hdr.src_port = 0; /* Ignored */;
	route->hdr.dest_svc = APR_SVC_ADM;
	route->hdr.dest_domain = APR_DOMAIN_ADSP;
	route->hdr.dest_port = 0; /* Ignored */;
	route->hdr.token = 0;
	route->hdr.opcode = ADM_CMD_MATRIX_MAP_ROUTINGS_V5;
	route->num_sessions = 1;

	switch (path) {
	case ADM_PATH_PLAYBACK:
		route->matrix_id = ADM_MATRIX_ID_AUDIO_RX;
		break;
	case ADM_PATH_LIVE_REC:
	case ADM_PATH_NONLIVE_REC:
		route->matrix_id = ADM_MATRIX_ID_AUDIO_TX;
		break;
	default:
		pr_err("%s: Wrong path set[%d]\n", __func__, path);
		break;
	}
	payload = ((u8 *)matrix_map +
			sizeof(struct adm_cmd_matrix_map_routings_v5));
	node = (struct adm_session_map_node_v5 *)payload;

	node->session_id = payload_map.session_id;
	node->num_copps = payload_map.num_copps;
	payload = (u8 *)node + sizeof(struct adm_session_map_node_v5);
	copps_list = (uint16_t *)payload;
	for (i = 0; i < payload_map.num_copps; i++) {
		port_idx =
		adm_validate_and_get_port_index(payload_map.port_id[i]);
		if (port_idx < 0) {
			pr_err("%s: Invalid port_id 0x%x\n", __func__,
				payload_map.port_id[i]);
			return -EINVAL;
		}
		copp_idx = payload_map.copp_idx[i];
		copps_list[i] = atomic_read(&this_adm.copp.id[port_idx]
							     [copp_idx]);
	}
	atomic_set(&this_adm.matrix_map_stat, 0);

	ret = apr_send_pkt(this_adm.apr, (uint32_t *)matrix_map);
	if (ret < 0) {
		pr_err("%s: routing for syream %d failed ret %d\n",
			__func__, payload_map.session_id, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}
	ret = wait_event_timeout(this_adm.matrix_map_wait,
				atomic_read(&this_adm.matrix_map_stat),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: routing for syream %d failed\n", __func__,
			payload_map.session_id);
		ret = -EINVAL;
		goto fail_cmd;
	}

	if (perf_mode != ULTRA_LOW_LATENCY_PCM_MODE) {
		for (i = 0; i < payload_map.num_copps; i++) {
			port_idx = afe_get_port_index(payload_map.port_id[i]);
			copp_idx = payload_map.copp_idx[i];
			if ((atomic_read(
				&this_adm.copp.topology[port_idx][copp_idx]) !=
				ADM_CMD_COPP_OPEN_TOPOLOGY_ID_DTS_HPX_0) ||
			    (atomic_read(
				&this_adm.copp.topology[port_idx][copp_idx]) !=
				ADM_CMD_COPP_OPEN_TOPOLOGY_ID_DTS_HPX_1))
				continue;
			rtac_add_adm_device(payload_map.port_id[i],
					    atomic_read(&this_adm.copp.id
							[port_idx][copp_idx]),
					    get_cal_path(path),
					    payload_map.session_id);
			send_adm_cal(payload_map.port_id[i], copp_idx,
				     get_cal_path(path), perf_mode,
				     payload_map.app_type,
				     payload_map.acdb_dev_id);
			pr_debug("%s: copp_id: %d\n", __func__,
				 atomic_read(&this_adm.copp.id[port_idx]
							      [copp_idx]));
		}
	}

fail_cmd:
	kfree(matrix_map);
	return ret;
}

void adm_ec_ref_rx_id(int port_id)
{
	this_adm.ec_ref_rx = port_id;
	pr_debug("%s: ec_ref_rx:%d", __func__, this_adm.ec_ref_rx);
}

int adm_close(int port_id, int perf_mode, int copp_idx)
{
	struct apr_hdr close;

	int ret = 0, port_idx;
	int copp_id = RESET_COPP_ID;

	pr_debug("%s: port_id=0x%x perf_mode: %d copp_idx: %d\n", __func__,
		 port_id, perf_mode, copp_idx);

	port_id = q6audio_convert_virtual_to_portid(port_id);
	port_idx = adm_validate_and_get_port_index(port_id);
	if (port_idx < 0) {
		pr_err("%s: Invalid port_id 0x%x\n",
			__func__, port_id);
		return -EINVAL;
	}

	if ((copp_idx < 0) || (copp_idx >= MAX_COPPS_PER_PORT)) {
		pr_err("%s: Invalid copp idx: %d\n", __func__, copp_idx);
		return -EINVAL;
	}
	atomic_dec(&this_adm.copp.cnt[port_idx][copp_idx]);
	if (!(atomic_read(&this_adm.copp.cnt[port_idx][copp_idx]))) {
		copp_id = adm_get_copp_id(port_idx, copp_idx);
		pr_debug("%s: Closing ADM port_idx:%d copp_idx:%d copp_id:0x%x\n",
			 __func__, port_idx, copp_idx, copp_id);

		if ((!perf_mode) && (this_adm.outband_memmap.paddr != 0)) {
			atomic_set(&this_adm.mem_map_cal_index, ADM_DTS_EAGLE);
			ret = adm_memory_unmap_regions();
			if (ret < 0) {
				pr_err("%s: adm mem unmmap err %d",
					__func__, ret);
			} else {
				atomic_set(&this_adm.mem_map_cal_handles
					   [ADM_DTS_EAGLE], 0);
			}
		}

		close.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
		close.pkt_size = sizeof(close);
		close.src_svc = APR_SVC_ADM;
		close.src_domain = APR_DOMAIN_APPS;
		close.src_port = port_id;
		close.dest_svc = APR_SVC_ADM;
		close.dest_domain = APR_DOMAIN_ADSP;
		close.dest_port = copp_id;
		close.token = port_idx << 16 | copp_idx;
		close.opcode = ADM_CMD_DEVICE_CLOSE_V5;

		atomic_set(&this_adm.copp.id[port_idx][copp_idx],
			   RESET_COPP_ID);
		atomic_set(&this_adm.copp.cnt[port_idx][copp_idx], 0);
		atomic_set(&this_adm.copp.topology[port_idx][copp_idx], 0);
		atomic_set(&this_adm.copp.mode[port_idx][copp_idx], 0);
		atomic_set(&this_adm.copp.stat[port_idx][copp_idx], 0);
		atomic_set(&this_adm.copp.bit_width[port_idx][copp_idx], 0);
		atomic_set(&this_adm.copp.app_type[port_idx][copp_idx], 0);

		ret = apr_send_pkt(this_adm.apr, (uint32_t *)&close);
		if (ret < 0) {
			pr_err("%s: ADM close failed %d\n", __func__, ret);
			return -EINVAL;
		}

		ret = wait_event_timeout(this_adm.copp.wait[port_idx][copp_idx],
			atomic_read(&this_adm.copp.stat[port_idx][copp_idx]),
			msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: ADM cmd Route timedout for port 0x%x\n",
				__func__, port_id);
			return -EINVAL;
		}
	}

	if (perf_mode != ULTRA_LOW_LATENCY_PCM_MODE) {
		pr_debug("%s: remove adm device from rtac\n", __func__);
		rtac_remove_adm_device(port_id, copp_id);
	}
	return 0;
}

int adm_map_rtac_block(struct rtac_cal_block_data *cal_block)
{
	int	result = 0;
	pr_debug("%s:\n", __func__);

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
	atomic_set(&this_adm.mem_map_cal_index, ADM_RTAC_APR_CAL);
	result = adm_memory_map_regions(&cal_block->cal_data.paddr, 0,
					&cal_block->map_data.map_size, 1);
	if (result < 0) {
		pr_err("%s: RTAC mmap did not work! size = %d result %d\n",
			__func__,
			cal_block->map_data.map_size, result);
		pr_debug("%s: RTAC mmap did not work! addr = 0x%pa, size = %d\n",
			__func__,
			&cal_block->cal_data.paddr,
			cal_block->map_data.map_size);
		goto done;
	}

	cal_block->map_data.map_handle = atomic_read(
		&this_adm.mem_map_cal_handles[ADM_RTAC_APR_CAL]);
done:
	return result;
}

int adm_unmap_rtac_block(uint32_t *mem_map_handle)
{
	int	result = 0;
	pr_debug("%s:\n", __func__);

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
			&this_adm.mem_map_cal_handles[ADM_RTAC_APR_CAL])) {
		pr_err("%s: Map handles do not match! Unmapping RTAC, RTAC map 0x%x, ADM map 0x%x\n",
			__func__, *mem_map_handle, atomic_read(
			&this_adm.mem_map_cal_handles[ADM_RTAC_APR_CAL]));

		/* if mismatch use handle passed in to unmap */
		atomic_set(&this_adm.mem_map_cal_handles[ADM_RTAC_APR_CAL],
			   *mem_map_handle);
	}

	/* valid port ID needed for callback use primary I2S */
	atomic_set(&this_adm.mem_map_cal_index, ADM_RTAC_APR_CAL);
	result = adm_memory_unmap_regions();
	if (result < 0) {
		pr_debug("%s: adm_memory_unmap_regions failed, error %d\n",
			__func__, result);
	} else {
		atomic_set(&this_adm.mem_map_cal_handles[ADM_RTAC_APR_CAL], 0);
		*mem_map_handle = 0;
	}
done:
	return result;
}

static int get_cal_type_index(int32_t cal_type)
{
	int ret = -EINVAL;

	switch (cal_type) {
	case ADM_AUDPROC_CAL_TYPE:
		ret = ADM_AUDPROC_CAL;
		break;
	case ADM_AUDVOL_CAL_TYPE:
		ret = ADM_AUDVOL_CAL;
		break;
	case ADM_CUST_TOPOLOGY_CAL_TYPE:
		ret = ADM_CUSTOM_TOP_CAL;
		break;
	case ADM_RTAC_INFO_CAL_TYPE:
		ret = ADM_RTAC_INFO_CAL;
		break;
	case ADM_RTAC_APR_CAL_TYPE:
		ret = ADM_RTAC_APR_CAL;
		break;
	default:
		pr_err("%s: invalid cal type %d!\n", __func__, cal_type);
	}
	return ret;
}

static int adm_alloc_cal(int32_t cal_type, size_t data_size, void *data)
{
	int				ret = 0;
	int				cal_index;
	pr_debug("%s:\n", __func__);

	cal_index = get_cal_type_index(cal_type);
	if (cal_index < 0) {
		pr_err("%s: could not get cal index %d!\n",
			__func__, cal_index);
		ret = -EINVAL;
		goto done;
	}

	ret = cal_utils_alloc_cal(data_size, data,
		this_adm.cal_data[cal_index], 0, NULL);
	if (ret < 0) {
		pr_err("%s: cal_utils_alloc_block failed, ret = %d, cal type = %d!\n",
			__func__, ret, cal_type);
		ret = -EINVAL;
		goto done;
	}
done:
	return ret;
}

static int adm_dealloc_cal(int32_t cal_type, size_t data_size, void *data)
{
	int				ret = 0;
	int				cal_index;
	pr_debug("%s:\n", __func__);

	cal_index = get_cal_type_index(cal_type);
	if (cal_index < 0) {
		pr_err("%s: could not get cal index %d!\n",
			__func__, cal_index);
		ret = -EINVAL;
		goto done;
	}

	ret = cal_utils_dealloc_cal(data_size, data,
		this_adm.cal_data[cal_index]);
	if (ret < 0) {
		pr_err("%s: cal_utils_dealloc_block failed, ret = %d, cal type = %d!\n",
			__func__, ret, cal_type);
		ret = -EINVAL;
		goto done;
	}
done:
	return ret;
}

static int adm_set_cal(int32_t cal_type, size_t data_size, void *data)
{
	int				ret = 0;
	int				cal_index;
	pr_debug("%s:\n", __func__);

	cal_index = get_cal_type_index(cal_type);
	if (cal_index < 0) {
		pr_err("%s: could not get cal index %d!\n",
			__func__, cal_index);
		ret = -EINVAL;
		goto done;
	}

	ret = cal_utils_set_cal(data_size, data,
		this_adm.cal_data[cal_index], 0, NULL);
	if (ret < 0) {
		pr_err("%s: cal_utils_set_cal failed, ret = %d, cal type = %d!\n",
			__func__, ret, cal_type);
		ret = -EINVAL;
		goto done;
	}

	if (cal_index == ADM_CUSTOM_TOP_CAL) {
		mutex_lock(&this_adm.cal_data[ADM_CUSTOM_TOP_CAL]->lock);
		this_adm.set_custom_topology = 1;
		mutex_unlock(&this_adm.cal_data[ADM_CUSTOM_TOP_CAL]->lock);
	}
done:
	return ret;
}

static int adm_map_cal_data(int32_t cal_type,
			struct cal_block_data *cal_block)
{
	int ret = 0;
	int cal_index;
	pr_debug("%s:\n", __func__);

	cal_index = get_cal_type_index(cal_type);
	if (cal_index < 0) {
		pr_err("%s: could not get cal index %d!\n",
			__func__, cal_index);
		ret = -EINVAL;
		goto done;
	}

	atomic_set(&this_adm.mem_map_cal_index, cal_index);
	ret = adm_memory_map_regions(&cal_block->cal_data.paddr, 0,
		(uint32_t *)&cal_block->map_data.map_size, 1);
	if (ret < 0) {
		pr_err("%s: map did not work! cal_type %i ret %d\n",
			__func__, cal_index, ret);
		ret = -ENODEV;
		goto done;
	}
	cal_block->map_data.q6map_handle = atomic_read(&this_adm.
		mem_map_cal_handles[cal_index]);
done:
	return ret;
}

static int adm_unmap_cal_data(int32_t cal_type,
			struct cal_block_data *cal_block)
{
	int ret = 0;
	int cal_index;
	pr_debug("%s:\n", __func__);

	cal_index = get_cal_type_index(cal_type);
	if (cal_index < 0) {
		pr_err("%s: could not get cal index %d!\n",
			__func__, cal_index);
		ret = -EINVAL;
		goto done;
	}

	atomic_set(&this_adm.mem_map_cal_handles[cal_index],
		cal_block->map_data.q6map_handle);
	atomic_set(&this_adm.mem_map_cal_index, cal_index);
	ret = adm_memory_unmap_regions();
	if (ret < 0) {
		pr_err("%s: unmap did not work! cal_type %i ret %d\n",
			__func__, cal_index, ret);
		ret = -ENODEV;
		goto done;
	}
	cal_block->map_data.q6map_handle = 0;
done:
	return ret;
}

static void adm_delete_cal_data(void)
{
	pr_debug("%s:\n", __func__);

	cal_utils_destroy_cal_types(ADM_MAX_CAL_TYPES, this_adm.cal_data);

	return;
}

static int adm_init_cal_data(void)
{
	int ret = 0;
	struct cal_type_info	cal_type_info[] = {
		{{ADM_CUST_TOPOLOGY_CAL_TYPE,
		{adm_alloc_cal, adm_dealloc_cal, NULL,
		adm_set_cal, NULL, NULL} },
		{adm_map_cal_data, adm_unmap_cal_data,
		cal_utils_match_ion_map} },

		{{ADM_AUDPROC_CAL_TYPE,
		{adm_alloc_cal, adm_dealloc_cal, NULL,
		adm_set_cal, NULL, NULL} },
		{adm_map_cal_data, adm_unmap_cal_data,
		cal_utils_match_ion_map} },

		{{ADM_AUDVOL_CAL_TYPE,
		{adm_alloc_cal, adm_dealloc_cal, NULL,
		adm_set_cal, NULL, NULL} },
		{adm_map_cal_data, adm_unmap_cal_data,
		cal_utils_match_ion_map} },

		{{ADM_RTAC_INFO_CAL_TYPE,
		{NULL, NULL, NULL, NULL, NULL, NULL} },
		{NULL, NULL, cal_utils_match_only_block} },

		{{ADM_RTAC_APR_CAL_TYPE,
		{NULL, NULL, NULL, NULL, NULL, NULL} },
		{NULL, NULL, cal_utils_match_only_block} }
	};
	pr_debug("%s:\n", __func__);

	ret = cal_utils_create_cal_types(ADM_MAX_CAL_TYPES, this_adm.cal_data,
		cal_type_info);
	if (ret < 0) {
		pr_err("%s: could not create cal type! ret %d\n",
			__func__, ret);
		ret = -EINVAL;
		goto err;
	}

	return ret;
err:
	adm_delete_cal_data();
	return ret;
}

static int __init adm_init(void)
{
	int i = 0, j;
	this_adm.apr = NULL;
	this_adm.ec_ref_rx = -1;
	atomic_set(&this_adm.matrix_map_stat, 0);
	init_waitqueue_head(&this_adm.matrix_map_wait);
	atomic_set(&this_adm.adm_stat, 0);
	init_waitqueue_head(&this_adm.adm_wait);

	for (i = 0; i < AFE_MAX_PORTS; i++) {
		for (j = 0; j < MAX_COPPS_PER_PORT; j++) {
			atomic_set(&this_adm.copp.id[i][j], RESET_COPP_ID);
			atomic_set(&this_adm.copp.cnt[i][j], 0);
			atomic_set(&this_adm.copp.topology[i][j], 0);
			atomic_set(&this_adm.copp.mode[i][j], 0);
			atomic_set(&this_adm.copp.stat[i][j], 0);
			atomic_set(&this_adm.copp.rate[i][j], 0);
			atomic_set(&this_adm.copp.bit_width[i][j], 0);
			atomic_set(&this_adm.copp.app_type[i][j], 0);
			init_waitqueue_head(&this_adm.copp.wait[i][j]);
		}
	}

	if (adm_init_cal_data())
		pr_err("%s: could not init cal data!\n", __func__);

	return 0;
}

static void __exit adm_exit(void)
{
	adm_delete_cal_data();
}

device_initcall(adm_init);
module_exit(adm_exit);
