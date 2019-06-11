/* Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <sound/asound.h>
#include <dsp/msm-dts-srs-tm-config.h>
#include <dsp/apr_audio-v2.h>
#include <dsp/q6adm-v2.h>
#include <dsp/q6audio-v2.h>
#include <dsp/q6afe-v2.h>
#include <dsp/q6core.h>
#include <dsp/audio_cal_utils.h>
#include <dsp/q6common.h>
#include <ipc/apr.h>
#include "adsp_err.h"

#define TIMEOUT_MS 1000

#define RESET_COPP_ID 99
#define INVALID_COPP_ID 0xFF
/* Used for inband payload copy, max size is 4k */
/* 3 is to account for module, instance & param ID in payload */
#define ADM_GET_PARAMETER_LENGTH (4096 - APR_HDR_SIZE - 3 * sizeof(uint32_t))

#define ULL_SUPPORTED_BITS_PER_SAMPLE 16
#define ULL_SUPPORTED_SAMPLE_RATE 48000

#ifndef CONFIG_DOLBY_DAP
#undef DOLBY_ADM_COPP_TOPOLOGY_ID
#define DOLBY_ADM_COPP_TOPOLOGY_ID 0xFFFFFFFE
#endif

#ifndef CONFIG_DOLBY_DS2
#undef DS2_ADM_COPP_TOPOLOGY_ID
#define DS2_ADM_COPP_TOPOLOGY_ID 0xFFFFFFFF
#endif

/* ENUM for adm_status */
enum adm_cal_status {
	ADM_STATUS_CALIBRATION_REQUIRED = 0,
	ADM_STATUS_MAX,
};

struct adm_copp {

	atomic_t id[AFE_MAX_PORTS][MAX_COPPS_PER_PORT];
	atomic_t cnt[AFE_MAX_PORTS][MAX_COPPS_PER_PORT];
	atomic_t topology[AFE_MAX_PORTS][MAX_COPPS_PER_PORT];
	atomic_t mode[AFE_MAX_PORTS][MAX_COPPS_PER_PORT];
	atomic_t stat[AFE_MAX_PORTS][MAX_COPPS_PER_PORT];
	atomic_t rate[AFE_MAX_PORTS][MAX_COPPS_PER_PORT];
	atomic_t bit_width[AFE_MAX_PORTS][MAX_COPPS_PER_PORT];
	atomic_t channels[AFE_MAX_PORTS][MAX_COPPS_PER_PORT];
	atomic_t app_type[AFE_MAX_PORTS][MAX_COPPS_PER_PORT];
	atomic_t acdb_id[AFE_MAX_PORTS][MAX_COPPS_PER_PORT];
	wait_queue_head_t wait[AFE_MAX_PORTS][MAX_COPPS_PER_PORT];
	wait_queue_head_t adm_delay_wait[AFE_MAX_PORTS][MAX_COPPS_PER_PORT];
	atomic_t adm_delay_stat[AFE_MAX_PORTS][MAX_COPPS_PER_PORT];
	uint32_t adm_delay[AFE_MAX_PORTS][MAX_COPPS_PER_PORT];
	unsigned long adm_status[AFE_MAX_PORTS][MAX_COPPS_PER_PORT];
};

struct source_tracking_data {
	struct dma_buf *dma_buf;
	struct param_outband memmap;
	int apr_cmd_status;
};

struct adm_ctl {
	void *apr;

	struct adm_copp copp;

	atomic_t matrix_map_stat;
	wait_queue_head_t matrix_map_wait;

	atomic_t adm_stat;
	wait_queue_head_t adm_wait;

	struct cal_type_data *cal_data[ADM_MAX_CAL_TYPES];

	atomic_t mem_map_handles[ADM_MEM_MAP_INDEX_MAX];
	atomic_t mem_map_index;

	struct param_outband outband_memmap;
	struct source_tracking_data sourceTrackingData;

	int set_custom_topology;
	int ec_ref_rx;
	int num_ec_ref_rx_chans;
	int ec_ref_rx_bit_width;
	int ec_ref_rx_sampling_rate;

	int native_mode;
};

static struct adm_ctl			this_adm;

struct adm_multi_ch_map {
	bool set_channel_map;
	char channel_mapping[PCM_FORMAT_MAX_NUM_CHANNEL_V8];
};

#define ADM_MCH_MAP_IDX_PLAYBACK 0
#define ADM_MCH_MAP_IDX_REC 1
static struct adm_multi_ch_map multi_ch_maps[2] = {
			{ false,
			{0, 0, 0, 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0, 0, 0, 0}
			},
			{ false,
			{0, 0, 0, 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0, 0, 0, 0}
			}
};

static int adm_get_parameters[MAX_COPPS_PER_PORT * ADM_GET_PARAMETER_LENGTH];
static int adm_module_topo_list[MAX_COPPS_PER_PORT *
				ADM_GET_TOPO_MODULE_INSTANCE_LIST_LENGTH];
static struct mutex dts_srs_lock;

void msm_dts_srs_acquire_lock(void)
{
	mutex_lock(&dts_srs_lock);
}

void msm_dts_srs_release_lock(void)
{
	mutex_unlock(&dts_srs_lock);
}

/**
 * adm_validate_and_get_port_index -
 *        validate given port id
 *
 * @port_id: Port ID number
 *
 * Returns valid index on success or error on failure
 */
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
EXPORT_SYMBOL(adm_validate_and_get_port_index);

/**
 * adm_get_default_copp_idx -
 *        retrieve default copp_idx for given port
 *
 * @port_id: Port ID number
 *
 * Returns valid value on success or error on failure
 */
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
EXPORT_SYMBOL(adm_get_default_copp_idx);

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

/**
 * adm_get_topology_for_port_copp_idx -
 *        retrieve topology of given port/copp_idx
 *
 * @port_id: Port ID number
 * @copp_idx: copp index of ADM copp
 *
 * Returns valid value on success or 0 on failure
 */
int adm_get_topology_for_port_copp_idx(int port_id, int copp_idx)
{
	int port_idx = adm_validate_and_get_port_index(port_id);

	if (port_idx < 0 || copp_idx >= MAX_COPPS_PER_PORT) {
		pr_err("%s: Invalid port: 0x%x copp id: 0x%x",
				__func__, port_id, copp_idx);
		return 0;
	}
	return atomic_read(&this_adm.copp.topology[port_idx][copp_idx]);
}
EXPORT_SYMBOL(adm_get_topology_for_port_copp_idx);

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

/**
 * srs_trumedia_open -
 *        command to set SRS trumedia open
 *
 * @port_id: Port ID number
 * @copp_idx: copp index of ADM copp
 * @srs_tech_id: SRS tech index
 * @srs_params: params pointer
 *
 * Returns 0 on success or error on failure
 */
int srs_trumedia_open(int port_id, int copp_idx, __s32 srs_tech_id,
		      void *srs_params)
{
	struct param_hdr_v3 param_hdr;
	struct mem_mapping_hdr mem_hdr;
	u32 total_param_size = 0;
	bool outband = false;
	int port_idx;
	int ret = 0;

	pr_debug("SRS - %s", __func__);

	memset(&param_hdr, 0, sizeof(param_hdr));
	memset(&mem_hdr, 0, sizeof(mem_hdr));
	port_id = afe_convert_virtual_to_portid(port_id);
	port_idx = adm_validate_and_get_port_index(port_id);
	if (port_idx < 0) {
		pr_err("%s: Invalid port_id %#x\n", __func__, port_id);
		return -EINVAL;
	}

	param_hdr.module_id = SRS_TRUMEDIA_MODULE_ID;
	param_hdr.instance_id = INSTANCE_ID_0;

	switch (srs_tech_id) {
	case SRS_ID_GLOBAL: {
		param_hdr.param_id = SRS_TRUMEDIA_PARAMS;
		param_hdr.param_size =
			sizeof(struct srs_trumedia_params_GLOBAL);
		break;
	}
	case SRS_ID_WOWHD: {
		param_hdr.param_id = SRS_TRUMEDIA_PARAMS_WOWHD;
		param_hdr.param_size = sizeof(struct srs_trumedia_params_WOWHD);
		break;
	}
	case SRS_ID_CSHP: {
		param_hdr.param_id = SRS_TRUMEDIA_PARAMS_CSHP;
		param_hdr.param_size = sizeof(struct srs_trumedia_params_CSHP);
		break;
	}
	case SRS_ID_HPF: {
		param_hdr.param_id = SRS_TRUMEDIA_PARAMS_HPF;
		param_hdr.param_size = sizeof(struct srs_trumedia_params_HPF);
		break;
	}
	case SRS_ID_AEQ: {
		u8 *update_params_ptr = (u8 *) this_adm.outband_memmap.kvaddr;

		outband = true;

		if (update_params_ptr == NULL) {
			pr_err("ADM_SRS_TRUMEDIA - %s: null memmap for AEQ params\n",
				__func__);
			ret = -EINVAL;
			goto fail_cmd;
		}

		param_hdr.param_id = SRS_TRUMEDIA_PARAMS_AEQ;
		param_hdr.param_size = sizeof(struct srs_trumedia_params_AEQ);

		ret = q6common_pack_pp_params(update_params_ptr, &param_hdr,
					      srs_params, &total_param_size);
		if (ret) {
			pr_err("%s: Failed to pack param header and data, error %d\n",
			       __func__, ret);
			goto fail_cmd;
		}
		break;
	}
	case SRS_ID_HL: {
		param_hdr.param_id = SRS_TRUMEDIA_PARAMS_HL;
		param_hdr.param_size = sizeof(struct srs_trumedia_params_HL);
		break;
	}
	case SRS_ID_GEQ: {
		param_hdr.param_id = SRS_TRUMEDIA_PARAMS_GEQ;
		param_hdr.param_size = sizeof(struct srs_trumedia_params_GEQ);
		break;
	}
	default:
		goto fail_cmd;
	}

	if (outband && this_adm.outband_memmap.paddr) {
		mem_hdr.data_payload_addr_lsw =
			lower_32_bits(this_adm.outband_memmap.paddr);
		mem_hdr.data_payload_addr_msw =
			msm_audio_populate_upper_32_bits(
				this_adm.outband_memmap.paddr);
		mem_hdr.mem_map_handle = atomic_read(
			&this_adm.mem_map_handles[ADM_SRS_TRUMEDIA]);

		ret = adm_set_pp_params(port_id, copp_idx, &mem_hdr, NULL,
					total_param_size);
	} else {
		ret = adm_pack_and_set_one_pp_param(port_id, copp_idx,
						    param_hdr,
						    (u8 *) srs_params);
	}

	if (ret < 0)
		pr_err("SRS - %s: ADM enable for port %d failed\n", __func__,
			port_id);

fail_cmd:
	return ret;
}
EXPORT_SYMBOL(srs_trumedia_open);

static int adm_populate_channel_weight(u16 *ptr,
					struct msm_pcm_channel_mixer *ch_mixer,
					int channel_index)
{
	u16 i, j, start_index = 0;

	if (channel_index > ch_mixer->output_channel) {
		pr_err("%s: channel index %d is larger than output_channel %d\n",
			 __func__, channel_index, ch_mixer->output_channel);
		return -EINVAL;
	}

	for (i = 0; i < ch_mixer->output_channel; i++) {
		pr_debug("%s: weight for output %d:", __func__, i);
		for (j = 0; j < ADM_MAX_CHANNELS; j++)
			pr_debug(" %d",
				ch_mixer->channel_weight[i][j]);
		pr_debug("\n");
	}

	for (i = 0; i < channel_index; ++i)
		start_index += ch_mixer->input_channels[i];

	for (i = 0; i < ch_mixer->output_channel; ++i) {
		for (j = start_index;
			j < start_index +
			ch_mixer->input_channels[channel_index]; j++) {
			*ptr = ch_mixer->channel_weight[i][j];
			 pr_debug("%s: ptr[%d][%d] = %d\n",
				__func__, i, j, *ptr);
			 ptr++;
		}
	}

	return 0;
}

/*
 * adm_programable_channel_mixer
 *
 * Receives port_id, copp_idx, session_id, session_type, ch_mixer
 * and channel_index to send ADM command to mix COPP data.
 *
 * port_id - Passed value, port_id for which backend is wanted
 * copp_idx - Passed value, copp_idx for which COPP is wanted
 * session_id - Passed value, session_id for which session is needed
 * session_type - Passed value, session_type for RX or TX
 * ch_mixer - Passed value, ch_mixer for which channel mixer config is needed
 * channel_index - Passed value, channel_index for which channel is needed
 */
int adm_programable_channel_mixer(int port_id, int copp_idx, int session_id,
				  int session_type,
				  struct msm_pcm_channel_mixer *ch_mixer,
				  int channel_index)
{
	struct adm_cmd_set_pspd_mtmx_strtr_params_v5 *adm_params = NULL;
	struct param_hdr_v1 data_v5;
	int ret = 0, port_idx, sz = 0, param_size = 0;
	u16 *adm_pspd_params;
	u16 *ptr;
	int index = 0;

	pr_debug("%s: port_id = %d\n", __func__, port_id);
	port_id = afe_convert_virtual_to_portid(port_id);
	port_idx = adm_validate_and_get_port_index(port_id);
	if (port_idx < 0) {
		pr_err("%s: Invalid port_id %#x\n", __func__, port_id);
		return -EINVAL;
	}
	/*
	 * First 8 bytes are 4 bytes as rule number, 2 bytes as output
	 * channel and 2 bytes as input channel.
	 * 2 * ch_mixer->output_channel means output channel mapping.
	 * 2 * ch_mixer->input_channels[channel_index]) means input
	 * channel mapping.
	 * 2 * ch_mixer->input_channels[channel_index] *
	 * ch_mixer->output_channel) means the channel mixer weighting
	 * coefficients.
	 * param_size needs to be a multiple of 4 bytes.
	 */

	param_size = 2 * (4 + ch_mixer->output_channel +
			ch_mixer->input_channels[channel_index] +
			ch_mixer->input_channels[channel_index] *
			ch_mixer->output_channel);
	param_size = roundup(param_size, 4);

	sz = sizeof(struct adm_cmd_set_pspd_mtmx_strtr_params_v5) +
	     sizeof(struct default_chmixer_param_id_coeff) +
	     sizeof(struct param_hdr_v1) + param_size;
	pr_debug("%s: sz = %d\n", __func__, sz);
	adm_params = kzalloc(sz, GFP_KERNEL);
	if (!adm_params)
		return -ENOMEM;

	adm_params->payload_addr_lsw = 0;
	adm_params->payload_addr_msw = 0;
	adm_params->mem_map_handle = 0;
	adm_params->direction = session_type;
	adm_params->sessionid = session_id;
	pr_debug("%s: copp_id = %d, session id  %d\n", __func__,
		atomic_read(&this_adm.copp.id[port_idx][copp_idx]),
			session_id);
	adm_params->deviceid = atomic_read(
				&this_adm.copp.id[port_idx][copp_idx]);
	adm_params->reserved = 0;

	/*
	 * This module is internal to ADSP and cannot be configured with
	 * an instance id
	 */
	data_v5.module_id = MTMX_MODULE_ID_DEFAULT_CHMIXER;
	data_v5.param_id =  DEFAULT_CHMIXER_PARAM_ID_COEFF;
	data_v5.reserved = 0;
	data_v5.param_size = param_size;
	adm_params->payload_size =
		sizeof(struct default_chmixer_param_id_coeff) +
		sizeof(struct param_hdr_v1) + data_v5.param_size;
	adm_pspd_params = (u16 *)((u8 *)adm_params +
			sizeof(struct adm_cmd_set_pspd_mtmx_strtr_params_v5));
	memcpy(adm_pspd_params, &data_v5, sizeof(data_v5));

	adm_pspd_params = (u16 *)((u8 *)adm_params +
			sizeof(struct adm_cmd_set_pspd_mtmx_strtr_params_v5)
			+ sizeof(data_v5));

	adm_pspd_params[0] = ch_mixer->rule;
	adm_pspd_params[2] = ch_mixer->output_channel;
	adm_pspd_params[3] = ch_mixer->input_channels[channel_index];
	index = 4;

	if (ch_mixer->output_channel == 1) {
		adm_pspd_params[index] = PCM_CHANNEL_FC;
	} else if (ch_mixer->output_channel == 2) {
		adm_pspd_params[index] = PCM_CHANNEL_FL;
		adm_pspd_params[index + 1] = PCM_CHANNEL_FR;
	} else if (ch_mixer->output_channel == 3) {
		adm_pspd_params[index] = PCM_CHANNEL_FL;
		adm_pspd_params[index + 1] = PCM_CHANNEL_FR;
		adm_pspd_params[index + 2] = PCM_CHANNEL_FC;
	} else if (ch_mixer->output_channel == 4) {
		adm_pspd_params[index] = PCM_CHANNEL_FL;
		adm_pspd_params[index + 1] = PCM_CHANNEL_FR;
		adm_pspd_params[index + 2] = PCM_CHANNEL_LS;
		adm_pspd_params[index + 3] = PCM_CHANNEL_RS;
	} else if (ch_mixer->output_channel == 5) {
		adm_pspd_params[index] = PCM_CHANNEL_FL;
		adm_pspd_params[index + 1] = PCM_CHANNEL_FR;
		adm_pspd_params[index + 2] = PCM_CHANNEL_FC;
		adm_pspd_params[index + 3] = PCM_CHANNEL_LS;
		adm_pspd_params[index + 4] = PCM_CHANNEL_RS;
	} else if (ch_mixer->output_channel == 6) {
		adm_pspd_params[index] = PCM_CHANNEL_FL;
		adm_pspd_params[index + 1] = PCM_CHANNEL_FR;
		adm_pspd_params[index + 2] = PCM_CHANNEL_LFE;
		adm_pspd_params[index + 3] = PCM_CHANNEL_FC;
		adm_pspd_params[index + 4] = PCM_CHANNEL_LS;
		adm_pspd_params[index + 5] = PCM_CHANNEL_RS;
	} else if (ch_mixer->output_channel == 8) {
		adm_pspd_params[index] = PCM_CHANNEL_FL;
		adm_pspd_params[index + 1] = PCM_CHANNEL_FR;
		adm_pspd_params[index + 2] = PCM_CHANNEL_LFE;
		adm_pspd_params[index + 3] = PCM_CHANNEL_FC;
		adm_pspd_params[index + 4] = PCM_CHANNEL_LS;
		adm_pspd_params[index + 5] = PCM_CHANNEL_RS;
		adm_pspd_params[index + 6] = PCM_CHANNEL_LB;
		adm_pspd_params[index + 7] = PCM_CHANNEL_RB;
	}

	index = index + ch_mixer->output_channel;
	if (ch_mixer->input_channels[channel_index] == 1) {
		adm_pspd_params[index] = PCM_CHANNEL_FC;
	} else if (ch_mixer->input_channels[channel_index] == 2) {
		adm_pspd_params[index] = PCM_CHANNEL_FL;
		adm_pspd_params[index + 1] = PCM_CHANNEL_FR;
	} else if (ch_mixer->input_channels[channel_index] == 3) {
		adm_pspd_params[index] = PCM_CHANNEL_FL;
		adm_pspd_params[index + 1] = PCM_CHANNEL_FR;
		adm_pspd_params[index + 2] = PCM_CHANNEL_FC;
	} else if (ch_mixer->input_channels[channel_index] == 4) {
		adm_pspd_params[index] = PCM_CHANNEL_FL;
		adm_pspd_params[index + 1] = PCM_CHANNEL_FR;
		adm_pspd_params[index + 2] = PCM_CHANNEL_LS;
		adm_pspd_params[index + 3] = PCM_CHANNEL_RS;
	} else if (ch_mixer->input_channels[channel_index] == 5) {
		adm_pspd_params[index] = PCM_CHANNEL_FL;
		adm_pspd_params[index + 1] = PCM_CHANNEL_FR;
		adm_pspd_params[index + 2] = PCM_CHANNEL_FC;
		adm_pspd_params[index + 3] = PCM_CHANNEL_LS;
		adm_pspd_params[index + 4] = PCM_CHANNEL_RS;
	} else if (ch_mixer->input_channels[channel_index] == 6) {
		adm_pspd_params[index] = PCM_CHANNEL_FL;
		adm_pspd_params[index + 1] = PCM_CHANNEL_FR;
		adm_pspd_params[index + 2] = PCM_CHANNEL_LFE;
		adm_pspd_params[index + 3] = PCM_CHANNEL_FC;
		adm_pspd_params[index + 4] = PCM_CHANNEL_LS;
		adm_pspd_params[index + 5] = PCM_CHANNEL_RS;
	} else if (ch_mixer->input_channels[channel_index] == 8) {
		adm_pspd_params[index] = PCM_CHANNEL_FL;
		adm_pspd_params[index + 1] = PCM_CHANNEL_FR;
		adm_pspd_params[index + 2] = PCM_CHANNEL_LFE;
		adm_pspd_params[index + 3] = PCM_CHANNEL_FC;
		adm_pspd_params[index + 4] = PCM_CHANNEL_LS;
		adm_pspd_params[index + 5] = PCM_CHANNEL_RS;
		adm_pspd_params[index + 6] = PCM_CHANNEL_LB;
		adm_pspd_params[index + 7] = PCM_CHANNEL_RB;
	}

	index = index + ch_mixer->input_channels[channel_index];
	ret = adm_populate_channel_weight(&adm_pspd_params[index],
					ch_mixer, channel_index);
	if (ret) {
		pr_err("%s: fail to get channel weight with error %d\n",
			__func__, ret);
		goto fail_cmd;
	}

	adm_params->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	adm_params->hdr.src_svc = APR_SVC_ADM;
	adm_params->hdr.src_domain = APR_DOMAIN_APPS;
	adm_params->hdr.src_port = port_id;
	adm_params->hdr.dest_svc = APR_SVC_ADM;
	adm_params->hdr.dest_domain = APR_DOMAIN_ADSP;
	adm_params->hdr.dest_port =
			atomic_read(&this_adm.copp.id[port_idx][copp_idx]);
	adm_params->hdr.token = port_idx << 16 | copp_idx;
	adm_params->hdr.opcode = ADM_CMD_SET_PSPD_MTMX_STRTR_PARAMS_V5;
	adm_params->hdr.pkt_size = sz;
	adm_params->payload_addr_lsw = 0;
	adm_params->payload_addr_msw = 0;
	adm_params->mem_map_handle = 0;
	adm_params->reserved = 0;

	ptr = (u16 *)adm_params;
	for (index = 0; index < (sz / 2); index++)
		pr_debug("%s: adm_params[%d] = 0x%x\n",
			__func__, index, (unsigned int)ptr[index]);

	atomic_set(&this_adm.copp.stat[port_idx][copp_idx], 0);
	ret = apr_send_pkt(this_adm.apr, (uint32_t *)adm_params);
	if (ret < 0) {
		pr_err("%s: Set params failed port %d rc %d\n", __func__,
			port_id, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = wait_event_timeout(this_adm.copp.wait[port_idx][copp_idx],
			atomic_read(
			&this_adm.copp.stat[port_idx][copp_idx]) >= 0,
			msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: set params timed out port = %d\n",
			__func__, port_id);
		ret = -ETIMEDOUT;
		goto fail_cmd;
	}
	ret = 0;
fail_cmd:
	kfree(adm_params);

	return ret;
}
EXPORT_SYMBOL(adm_programable_channel_mixer);

/**
 * adm_set_stereo_to_custom_stereo -
 *        command to update custom stereo
 *
 * @port_id: Port ID number
 * @copp_idx: copp index of ADM copp
 * @session_id: session id to be updated
 * @params: params pointer
 * @param_length: length of params
 *
 * Returns 0 on success or error on failure
 */
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
	adm_params->hdr.token = port_idx << 16 | copp_idx;
	adm_params->hdr.opcode = ADM_CMD_SET_PSPD_MTMX_STRTR_PARAMS_V5;
	adm_params->payload_addr_lsw = 0;
	adm_params->payload_addr_msw = 0;
	adm_params->mem_map_handle = 0;
	adm_params->payload_size = params_length;
	/* direction RX as 0 */
	adm_params->direction = ADM_MATRIX_ID_AUDIO_RX;
	/* session id for this cmd to be applied on */
	adm_params->sessionid = session_id;
	adm_params->deviceid =
			atomic_read(&this_adm.copp.id[port_idx][copp_idx]);
	adm_params->reserved = 0;
	pr_debug("%s: deviceid %d, session_id %d, src_port %d, dest_port %d\n",
		__func__, adm_params->deviceid, adm_params->sessionid,
		adm_params->hdr.src_port, adm_params->hdr.dest_port);
	atomic_set(&this_adm.copp.stat[port_idx][copp_idx], -1);
	rc = apr_send_pkt(this_adm.apr, (uint32_t *)adm_params);
	if (rc < 0) {
		pr_err("%s: Set params failed port = 0x%x rc %d\n",
			__func__, port_id, rc);
		rc = -EINVAL;
		goto set_stereo_to_custom_stereo_return;
	}
	/* Wait for the callback */
	rc = wait_event_timeout(this_adm.copp.wait[port_idx][copp_idx],
				atomic_read(&this_adm.copp.stat
				[port_idx][copp_idx]) >= 0,
				msecs_to_jiffies(TIMEOUT_MS));
	if (!rc) {
		pr_err("%s: Set params timed out port = 0x%x\n", __func__,
			port_id);
		rc = -EINVAL;
		goto set_stereo_to_custom_stereo_return;
	} else if (atomic_read(&this_adm.copp.stat
				[port_idx][copp_idx]) > 0) {
		pr_err("%s: DSP returned error[%s]\n", __func__,
			adsp_err_get_err_str(atomic_read(
			&this_adm.copp.stat
			[port_idx][copp_idx])));
		rc = adsp_err_get_lnx_err_code(
				atomic_read(&this_adm.copp.stat
					[port_idx][copp_idx]));
		goto set_stereo_to_custom_stereo_return;
	}
	rc = 0;
set_stereo_to_custom_stereo_return:
	kfree(adm_params);
	return rc;
}
EXPORT_SYMBOL(adm_set_stereo_to_custom_stereo);

/*
 * adm_set_custom_chmix_cfg:
 *	Set the custom channel mixer configuration for ADM
 *
 * @port_id: Backend port id
 * @copp_idx: ADM copp index
 * @session_id: ID of the requesting session
 * @params: Expected packaged params for channel mixer
 * @params_length: Length of the params to be set
 * @direction: RX or TX direction
 * @stream_type: Audio or Listen stream type
 */
int adm_set_custom_chmix_cfg(int port_id, int copp_idx,
			     unsigned int session_id, char *params,
			     uint32_t params_length, int direction,
			     int stream_type)
{
	struct adm_cmd_set_pspd_mtmx_strtr_params_v6 *adm_params = NULL;
	int sz, rc = 0, port_idx;

	port_id = afe_convert_virtual_to_portid(port_id);
	port_idx = adm_validate_and_get_port_index(port_id);
	if (port_idx < 0) {
		pr_err("%s: Invalid port_id 0x%x\n", __func__, port_id);
		return -EINVAL;
	}

	sz = sizeof(struct adm_cmd_set_pspd_mtmx_strtr_params_v6) +
		params_length;
	adm_params = kzalloc(sz, GFP_KERNEL);
	if (!adm_params) {
		pr_err("%s, adm params memory alloc failed\n", __func__);
		return -ENOMEM;
	}

	memcpy(((u8 *)adm_params +
		sizeof(struct adm_cmd_set_pspd_mtmx_strtr_params_v6)),
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
	adm_params->hdr.token = port_idx << 16 | copp_idx;
	adm_params->hdr.opcode = ADM_CMD_SET_PSPD_MTMX_STRTR_PARAMS_V6;
	adm_params->payload_addr_lsw = 0;
	adm_params->payload_addr_msw = 0;
	adm_params->mem_map_handle = 0;
	adm_params->payload_size = params_length;
	adm_params->direction = direction;
	/* session id for this cmd to be applied on */
	adm_params->sessionid = session_id;
	adm_params->deviceid =
			atomic_read(&this_adm.copp.id[port_idx][copp_idx]);
	/* connecting stream type i.e. lsm or asm */
	adm_params->stream_type = stream_type;
	pr_debug("%s: deviceid %d, session_id %d, src_port %d, dest_port %d\n",
		__func__, adm_params->deviceid, adm_params->sessionid,
		adm_params->hdr.src_port, adm_params->hdr.dest_port);
	atomic_set(&this_adm.copp.stat[port_idx][copp_idx], -1);
	rc = apr_send_pkt(this_adm.apr, (uint32_t *)adm_params);
	if (rc < 0) {
		pr_err("%s: Set params failed port = 0x%x rc %d\n",
			__func__, port_id, rc);
		rc = -EINVAL;
		goto exit;
	}
	/* Wait for the callback */
	rc = wait_event_timeout(this_adm.copp.wait[port_idx][copp_idx],
				atomic_read(&this_adm.copp.stat
				[port_idx][copp_idx]),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!rc) {
		pr_err("%s: Set params timed out port = 0x%x\n", __func__,
			port_id);
		rc = -EINVAL;
		goto exit;
	} else if (atomic_read(&this_adm.copp.stat
				[port_idx][copp_idx]) > 0) {
		pr_err("%s: DSP returned error[%s]\n", __func__,
			adsp_err_get_err_str(atomic_read(
				&this_adm.copp.stat
				[port_idx][copp_idx])));
		rc = adsp_err_get_lnx_err_code(
				atomic_read(&this_adm.copp.stat
					[port_idx][copp_idx]));
		goto exit;
	}

	rc = 0;
exit:
	kfree(adm_params);
	return rc;
}
EXPORT_SYMBOL(adm_set_custom_chmix_cfg);

/*
 * With pre-packed data, only the opcode differes from V5 and V6.
 * Use q6common_pack_pp_params to pack the data correctly.
 */
int adm_set_pp_params(int port_id, int copp_idx,
		      struct mem_mapping_hdr *mem_hdr, u8 *param_data,
		      u32 param_size)
{
	struct adm_cmd_set_pp_params *adm_set_params = NULL;
	int size = 0;
	int port_idx = 0;
	atomic_t *copp_stat = NULL;
	int ret = 0;

	port_id = afe_convert_virtual_to_portid(port_id);
	port_idx = adm_validate_and_get_port_index(port_id);
	if (port_idx < 0 || port_idx >= AFE_MAX_PORTS) {
		pr_err("%s: Invalid port_idx 0x%x\n", __func__, port_idx);
		return -EINVAL;
	} else if (copp_idx < 0 || copp_idx >= MAX_COPPS_PER_PORT) {
		pr_err("%s: Invalid copp_idx 0x%x\n", __func__, copp_idx);
		return -EINVAL;
	}

	/* Only add params_size in inband case */
	size = sizeof(struct adm_cmd_set_pp_params);
	if (param_data != NULL)
		size += param_size;
	adm_set_params = kzalloc(size, GFP_KERNEL);
	if (!adm_set_params)
		return -ENOMEM;

	adm_set_params->apr_hdr.hdr_field =
		APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD, APR_HDR_LEN(APR_HDR_SIZE),
			      APR_PKT_VER);
	adm_set_params->apr_hdr.pkt_size = size;
	adm_set_params->apr_hdr.src_svc = APR_SVC_ADM;
	adm_set_params->apr_hdr.src_domain = APR_DOMAIN_APPS;
	adm_set_params->apr_hdr.src_port = port_id;
	adm_set_params->apr_hdr.dest_svc = APR_SVC_ADM;
	adm_set_params->apr_hdr.dest_domain = APR_DOMAIN_ADSP;
	adm_set_params->apr_hdr.dest_port =
		atomic_read(&this_adm.copp.id[port_idx][copp_idx]);
	adm_set_params->apr_hdr.token = port_idx << 16 | copp_idx;

	if (q6common_is_instance_id_supported())
		adm_set_params->apr_hdr.opcode = ADM_CMD_SET_PP_PARAMS_V6;
	else
		adm_set_params->apr_hdr.opcode = ADM_CMD_SET_PP_PARAMS_V5;

	adm_set_params->payload_size = param_size;

	if (mem_hdr != NULL) {
		/* Out of Band Case */
		adm_set_params->mem_hdr = *mem_hdr;
	} else if (param_data != NULL) {
		/*
		 * In band case. Parameter data must be pre-packed with its
		 * header before calling this function. Use
		 * q6common_pack_pp_params to pack parameter data and header
		 * correctly.
		 */
		memcpy(&adm_set_params->param_data, param_data, param_size);
	} else {
		pr_err("%s: Received NULL pointers for both memory header and param data\n",
		       __func__);
		ret = -EINVAL;
		goto done;
	}

	copp_stat = &this_adm.copp.stat[port_idx][copp_idx];
	atomic_set(copp_stat, -1);
	ret = apr_send_pkt(this_adm.apr, (uint32_t *) adm_set_params);
	if (ret < 0) {
		pr_err("%s: Set params APR send failed port = 0x%x ret %d\n",
		       __func__, port_id, ret);
		goto done;
	}
	ret = wait_event_timeout(this_adm.copp.wait[port_idx][copp_idx],
				 atomic_read(copp_stat) >= 0,
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: Set params timed out port = 0x%x\n", __func__,
		       port_id);
		ret = -ETIMEDOUT;
		goto done;
	}
	if (atomic_read(copp_stat) > 0) {
		pr_err("%s: DSP returned error[%s]\n", __func__,
		       adsp_err_get_err_str(atomic_read(copp_stat)));
		ret = adsp_err_get_lnx_err_code(atomic_read(copp_stat));
		goto done;
	}

	ret = 0;
done:
	kfree(adm_set_params);
	return ret;
}
EXPORT_SYMBOL(adm_set_pp_params);

int adm_pack_and_set_one_pp_param(int port_id, int copp_idx,
				  struct param_hdr_v3 param_hdr, u8 *param_data)
{
	u8 *packed_data = NULL;
	u32 total_size = 0;
	int ret = 0;

	total_size = sizeof(union param_hdrs) + param_hdr.param_size;
	packed_data = kzalloc(total_size, GFP_KERNEL);
	if (!packed_data)
		return -ENOMEM;

	ret = q6common_pack_pp_params(packed_data, &param_hdr, param_data,
				      &total_size);
	if (ret) {
		pr_err("%s: Failed to pack parameter data, error %d\n",
		       __func__, ret);
		goto done;
	}

	ret = adm_set_pp_params(port_id, copp_idx, NULL, packed_data,
				total_size);
	if (ret)
		pr_err("%s: Failed to set parameter data, error %d\n", __func__,
		       ret);
done:
	kfree(packed_data);
	return ret;
}
EXPORT_SYMBOL(adm_pack_and_set_one_pp_param);

/*
 * Only one parameter can be requested at a time. Therefore, packing and sending
 * the request can be handled locally.
 */
int adm_get_pp_params(int port_id, int copp_idx, uint32_t client_id,
		      struct mem_mapping_hdr *mem_hdr,
		      struct param_hdr_v3 *param_hdr, u8 *returned_param_data)
{
	struct adm_cmd_get_pp_params adm_get_params;
	int total_size = 0;
	int get_param_array_sz = ARRAY_SIZE(adm_get_parameters);
	int returned_param_size = 0;
	int returned_param_size_in_bytes = 0;
	int port_idx = 0;
	int idx = 0;
	atomic_t *copp_stat = NULL;
	int ret = 0;

	if (param_hdr == NULL) {
		pr_err("%s: Received NULL pointer for parameter header\n",
		       __func__);
		return -EINVAL;
	}

	port_id = afe_convert_virtual_to_portid(port_id);
	port_idx = adm_validate_and_get_port_index(port_id);
	if (port_idx < 0 || port_idx >= AFE_MAX_PORTS) {
		pr_err("%s: Invalid port_idx 0x%x\n", __func__, port_idx);
		return -EINVAL;
	}
	if (copp_idx < 0 || copp_idx >= MAX_COPPS_PER_PORT) {
		pr_err("%s: Invalid copp_idx 0x%x\n", __func__, copp_idx);
		return -EINVAL;
	}

	memset(&adm_get_params, 0, sizeof(adm_get_params));

	if (mem_hdr != NULL)
		adm_get_params.mem_hdr = *mem_hdr;

	q6common_pack_pp_params((u8 *) &adm_get_params.param_hdr, param_hdr,
				NULL, &total_size);

	/* Pack APR header after filling body so total_size has correct value */
	adm_get_params.apr_hdr.hdr_field =
		APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD, APR_HDR_LEN(APR_HDR_SIZE),
			      APR_PKT_VER);
	adm_get_params.apr_hdr.pkt_size = sizeof(adm_get_params);
	adm_get_params.apr_hdr.src_svc = APR_SVC_ADM;
	adm_get_params.apr_hdr.src_domain = APR_DOMAIN_APPS;
	adm_get_params.apr_hdr.src_port = port_id;
	adm_get_params.apr_hdr.dest_svc = APR_SVC_ADM;
	adm_get_params.apr_hdr.dest_domain = APR_DOMAIN_ADSP;
	adm_get_params.apr_hdr.dest_port =
		atomic_read(&this_adm.copp.id[port_idx][copp_idx]);
	adm_get_params.apr_hdr.token =
		port_idx << 16 | client_id << 8 | copp_idx;

	if (q6common_is_instance_id_supported())
		adm_get_params.apr_hdr.opcode = ADM_CMD_GET_PP_PARAMS_V6;
	else
		adm_get_params.apr_hdr.opcode = ADM_CMD_GET_PP_PARAMS_V5;

	copp_stat = &this_adm.copp.stat[port_idx][copp_idx];
	atomic_set(copp_stat, -1);

	ret = apr_send_pkt(this_adm.apr, (uint32_t *) &adm_get_params);
	if (ret < 0) {
		pr_err("%s: Get params APR send failed port = 0x%x ret %d\n",
		       __func__, port_id, ret);
		ret = -EINVAL;
		goto done;
	}
	ret = wait_event_timeout(this_adm.copp.wait[port_idx][copp_idx],
				 atomic_read(copp_stat) >= 0,
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: Get params timed out port = 0x%x\n", __func__,
		       port_id);
		ret = -ETIMEDOUT;
		goto done;
	}
	if (atomic_read(copp_stat) > 0) {
		pr_err("%s: DSP returned error[%s]\n", __func__,
		       adsp_err_get_err_str(atomic_read(copp_stat)));
		ret = adsp_err_get_lnx_err_code(atomic_read(copp_stat));
		goto done;
	}

	ret = 0;

	/* Copy data to caller if sent in band */
	if (!returned_param_data) {
		pr_debug("%s: Received NULL pointer for param destination, not copying payload\n",
			__func__);
		return 0;
	}

	idx = ADM_GET_PARAMETER_LENGTH * copp_idx;
	returned_param_size = adm_get_parameters[idx];
	if (returned_param_size < 0 ||
	    returned_param_size + idx + 1 > get_param_array_sz) {
		pr_err("%s: Invalid parameter size %d\n", __func__,
		       returned_param_size);
		return -EINVAL;
	}

	returned_param_size_in_bytes = returned_param_size * sizeof(uint32_t);
	if (param_hdr->param_size < returned_param_size_in_bytes) {
		pr_err("%s: Provided buffer is not big enough, provided buffer size(%d) size needed(%d)\n",
		       __func__, param_hdr->param_size,
		       returned_param_size_in_bytes);
		return -EINVAL;
	}

	memcpy(returned_param_data, &adm_get_parameters[idx + 1],
	       returned_param_size_in_bytes);
done:
	return ret;
}
EXPORT_SYMBOL(adm_get_pp_params);

int adm_get_pp_topo_module_list_v2(int port_id, int copp_idx,
				   int32_t param_length,
				   int32_t *returned_params)
{
	struct adm_cmd_get_pp_topo_module_list adm_get_module_list;
	bool iid_supported = q6common_is_instance_id_supported();
	int *topo_list;
	int num_modules = 0;
	int list_size = 0;
	int port_idx, idx;
	int i = 0;
	atomic_t *copp_stat = NULL;
	int ret = 0;

	pr_debug("%s : port_id %x", __func__, port_id);
	port_id = afe_convert_virtual_to_portid(port_id);
	port_idx = adm_validate_and_get_port_index(port_id);
	if (port_idx < 0) {
		pr_err("%s: Invalid port_id 0x%x\n", __func__, port_id);
		return -EINVAL;
	}

	if (copp_idx < 0 || copp_idx >= MAX_COPPS_PER_PORT) {
		pr_err("%s: Invalid copp_num: %d\n", __func__, copp_idx);
		return -EINVAL;
	}

	memset(&adm_get_module_list, 0, sizeof(adm_get_module_list));

	adm_get_module_list.apr_hdr.pkt_size = sizeof(adm_get_module_list);
	adm_get_module_list.apr_hdr.src_svc = APR_SVC_ADM;
	adm_get_module_list.apr_hdr.src_domain = APR_DOMAIN_APPS;
	adm_get_module_list.apr_hdr.src_port = port_id;
	adm_get_module_list.apr_hdr.dest_svc = APR_SVC_ADM;
	adm_get_module_list.apr_hdr.dest_domain = APR_DOMAIN_ADSP;
	adm_get_module_list.apr_hdr.dest_port =
		atomic_read(&this_adm.copp.id[port_idx][copp_idx]);
	adm_get_module_list.apr_hdr.token = port_idx << 16 | copp_idx;
	/*
	 * Out of band functionality is not currently utilized.
	 * Assume in band.
	 */
	if (iid_supported) {
		adm_get_module_list.apr_hdr.opcode =
			ADM_CMD_GET_PP_TOPO_MODULE_LIST_V2;
		adm_get_module_list.param_max_size = param_length;
	} else {
		adm_get_module_list.apr_hdr.opcode =
			ADM_CMD_GET_PP_TOPO_MODULE_LIST;

		if (param_length > U16_MAX) {
			pr_err("%s: Invalid param length for V1 %d\n", __func__,
			       param_length);
			return -EINVAL;
		}
		adm_get_module_list.param_max_size = param_length << 16;
	}

	copp_stat = &this_adm.copp.stat[port_idx][copp_idx];
	atomic_set(copp_stat, -1);
	ret = apr_send_pkt(this_adm.apr, (uint32_t *) &adm_get_module_list);
	if (ret < 0) {
		pr_err("%s: APR send pkt failed for port_id: 0x%x failed ret %d\n",
		       __func__, port_id, ret);
		ret = -EINVAL;
		goto done;
	}
	ret = wait_event_timeout(this_adm.copp.wait[port_idx][copp_idx],
				 atomic_read(copp_stat) >= 0,
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: Timeout for port_id: 0x%x\n", __func__, port_id);
		ret = -ETIMEDOUT;
		goto done;
	}
	if (atomic_read(copp_stat) > 0) {
		pr_err("%s: DSP returned error[%s]\n", __func__,
		       adsp_err_get_err_str(atomic_read(copp_stat)));
		ret = adsp_err_get_lnx_err_code(atomic_read(copp_stat));
		goto done;
	}

	ret = 0;

	if (returned_params) {
		/*
		 * When processing ADM_CMDRSP_GET_PP_TOPO_MODULE_LIST IID is
		 * added since it is not present. Therefore, there is no need to
		 * do anything different if IID is not supported here as it is
		 * already taken care of.
		 */
		idx = ADM_GET_TOPO_MODULE_INSTANCE_LIST_LENGTH * copp_idx;
		num_modules = adm_module_topo_list[idx];
		if (num_modules < 0 || num_modules > MAX_MODULES_IN_TOPO) {
			pr_err("%s: Invalid number of modules returned %d\n",
			       __func__, num_modules);
			return -EINVAL;
		}

		list_size = num_modules * sizeof(struct module_instance_info);
		if (param_length < list_size) {
			pr_err("%s: Provided buffer not big enough to hold module-instance list, provided size %d, needed size %d\n",
			       __func__, param_length, list_size);
			return -EINVAL;
		}

		topo_list = (int32_t *) (&adm_module_topo_list[idx]);
		memcpy(returned_params, topo_list, list_size);
		for (i = 1; i <= num_modules; i += 2) {
			pr_debug("module = 0x%x instance = 0x%x\n",
				 returned_params[i], returned_params[i + 1]);
		}
	}
done:
	return ret;
}
EXPORT_SYMBOL(adm_get_pp_topo_module_list_v2);

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

/**
 * adm_set_multi_ch_map -
 *        Update multi channel map info
 *
 * @channel_map: pointer with channel map info
 * @path: direction or ADM path type
 *
 * Returns 0 on success or error on failure
 */
int adm_set_multi_ch_map(char *channel_map, int path)
{
	int idx;

	if (path == ADM_PATH_PLAYBACK) {
		idx = ADM_MCH_MAP_IDX_PLAYBACK;
	} else if (path == ADM_PATH_LIVE_REC) {
		idx = ADM_MCH_MAP_IDX_REC;
	} else {
		pr_err("%s: invalid attempt to set path %d\n", __func__, path);
		return -EINVAL;
	}

	memcpy(multi_ch_maps[idx].channel_mapping, channel_map,
			PCM_FORMAT_MAX_NUM_CHANNEL_V8);
	multi_ch_maps[idx].set_channel_map = true;

	return 0;
}
EXPORT_SYMBOL(adm_set_multi_ch_map);

/**
 * adm_get_multi_ch_map -
 *        Retrieves multi channel map info
 *
 * @channel_map: pointer to be updated with channel map
 * @path: direction or ADM path type
 *
 * Returns 0 on success or error on failure
 */
int adm_get_multi_ch_map(char *channel_map, int path)
{
	int idx;

	if (path == ADM_PATH_PLAYBACK) {
		idx = ADM_MCH_MAP_IDX_PLAYBACK;
	} else if (path == ADM_PATH_LIVE_REC) {
		idx = ADM_MCH_MAP_IDX_REC;
	} else {
		pr_err("%s: invalid attempt to get path %d\n", __func__, path);
		return -EINVAL;
	}

	if (multi_ch_maps[idx].set_channel_map) {
		memcpy(channel_map, multi_ch_maps[idx].channel_mapping,
				PCM_FORMAT_MAX_NUM_CHANNEL_V8);
	}

	return 0;
}
EXPORT_SYMBOL(adm_get_multi_ch_map);

static int adm_process_get_param_response(u32 opcode, u32 idx, u32 *payload,
					  u32 payload_size)
{
	struct adm_cmd_rsp_get_pp_params_v5 *v5_rsp = NULL;
	struct adm_cmd_rsp_get_pp_params_v6 *v6_rsp = NULL;
	u32 *param_data = NULL;
	int data_size = 0;
	int struct_size = 0;

	if (payload == NULL) {
		pr_err("%s: Payload is NULL\n", __func__);
		return -EINVAL;
	}

	switch (opcode) {
	case ADM_CMDRSP_GET_PP_PARAMS_V5:
		struct_size = sizeof(struct adm_cmd_rsp_get_pp_params_v5);
		if (payload_size < struct_size) {
			pr_err("%s: payload size %d < expected size %d\n",
				__func__, payload_size, struct_size);
			break;
		}
		v5_rsp = (struct adm_cmd_rsp_get_pp_params_v5 *) payload;
		data_size = v5_rsp->param_hdr.param_size;
		param_data = v5_rsp->param_data;
		break;
	case ADM_CMDRSP_GET_PP_PARAMS_V6:
		struct_size = sizeof(struct adm_cmd_rsp_get_pp_params_v6);
		if (payload_size < struct_size) {
			pr_err("%s: payload size %d < expected size %d\n",
				__func__, payload_size, struct_size);
			break;
		}
		v6_rsp = (struct adm_cmd_rsp_get_pp_params_v6 *) payload;
		data_size = v6_rsp->param_hdr.param_size;
		param_data = v6_rsp->param_data;
		break;
	default:
		pr_err("%s: Invalid opcode %d\n", __func__, opcode);
		return -EINVAL;
	}

	/*
	 * Just store the returned parameter data, not the header. The calling
	 * function is expected to know what it asked for. Therefore, there is
	 * no difference between V5 and V6.
	 */
	if ((payload_size >= struct_size + data_size) &&
	    (ARRAY_SIZE(adm_get_parameters) > idx) &&
	    (ARRAY_SIZE(adm_get_parameters) > idx + 1 + data_size)) {
		pr_debug("%s: Received parameter data in band\n",
					__func__);
		/*
		 * data_size is expressed in number of bytes, store in number of
		 * ints
		 */
		adm_get_parameters[idx] =
			data_size / sizeof(*adm_get_parameters);
		pr_debug("%s: GET_PP PARAM: received parameter length: 0x%x\n",
			 __func__, adm_get_parameters[idx]);
		/* store params after param_size */
		memcpy(&adm_get_parameters[idx + 1], param_data, data_size);
	} else if (payload_size == sizeof(uint32_t)) {
		adm_get_parameters[idx] = -1;
		pr_debug("%s: Out of band case, setting size to %d\n",
			 __func__, adm_get_parameters[idx]);
	} else {
		pr_err("%s: Invalid parameter combination, payload_size %d, idx %d\n",
		       __func__, payload_size, idx);
		return -EINVAL;
	}
	return 0;
}

static int adm_process_get_topo_list_response(u32 opcode, int copp_idx,
					      u32 num_modules, u32 *payload,
					      u32 payload_size)
{
	u32 *fill_list = NULL;
	int idx = 0;
	int i = 0;
	int j = 0;

	if (payload == NULL) {
		pr_err("%s: Payload is NULL\n", __func__);
		return -EINVAL;
	} else if (copp_idx < 0 || copp_idx >= MAX_COPPS_PER_PORT) {
		pr_err("%s: Invalid COPP index %d\n", __func__, copp_idx);
		return -EINVAL;
	}

	idx = ADM_GET_TOPO_MODULE_INSTANCE_LIST_LENGTH * copp_idx;
	fill_list = adm_module_topo_list + idx;
	*fill_list++ = num_modules;
	for (i = 0; i < num_modules; i++) {
		if (j > payload_size / sizeof(u32)) {
			pr_err("%s: Invalid number of modules specified %d\n",
			       __func__, num_modules);
			return -EINVAL;
		}

		/* store module ID */
		*fill_list++ = payload[j];
		j++;

		switch (opcode) {
		case ADM_CMDRSP_GET_PP_TOPO_MODULE_LIST_V2:
			/* store instance ID */
			*fill_list++ = payload[j];
			j++;
			break;
		case ADM_CMDRSP_GET_PP_TOPO_MODULE_LIST:
			/* Insert IID 0 when repacking */
			*fill_list++ = INSTANCE_ID_0;
			break;
		default:
			pr_err("%s: Invalid opcode %d\n", __func__, opcode);
			return -EINVAL;
		}
	}

	return 0;
}

static void adm_reset_data(void)
{
	int i, j;

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
				&this_adm.copp.channels[i][j],
				   0);
			atomic_set(
			    &this_adm.copp.bit_width[i][j], 0);
			atomic_set(
			    &this_adm.copp.app_type[i][j], 0);
			atomic_set(
			   &this_adm.copp.acdb_id[i][j], 0);
			this_adm.copp.adm_status[i][j] =
				ADM_STATUS_CALIBRATION_REQUIRED;
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
	/*
	 * Free the ION memory and clear the map handles
	 * for Source Tracking
	 */
	if (this_adm.sourceTrackingData.memmap.paddr != 0) {
		msm_audio_ion_free(
			this_adm.sourceTrackingData.dma_buf);
		this_adm.sourceTrackingData.dma_buf = NULL;
		this_adm.sourceTrackingData.memmap.size = 0;
		this_adm.sourceTrackingData.memmap.kvaddr =
							 NULL;
		this_adm.sourceTrackingData.memmap.paddr = 0;
		this_adm.sourceTrackingData.apr_cmd_status = -1;
		atomic_set(&this_adm.mem_map_handles[
			ADM_MEM_MAP_INDEX_SOURCE_TRACKING], 0);
	}
}

static int32_t adm_callback(struct apr_client_data *data, void *priv)
{
	uint32_t *payload;
	int port_idx, copp_idx, idx, client_id;
	int num_modules;
	int ret;

	if (data == NULL) {
		pr_err("%s: data parameter is null\n", __func__);
		return -EINVAL;
	}

	payload = data->payload;

	if (data->opcode == RESET_EVENTS) {
		pr_debug("%s: Reset event is received: %d %d apr[%pK]\n",
			__func__,
			data->reset_event, data->reset_proc, this_adm.apr);
		if (this_adm.apr)
			adm_reset_data();
		return 0;
	}

	adm_callback_debug_print(data);
	if (data->payload_size) {
		copp_idx = (data->token) & 0XFF;
		port_idx = ((data->token) >> 16) & 0xFF;
		client_id = ((data->token) >> 8) & 0xFF;
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
		if (client_id < 0 || client_id >= ADM_CLIENT_ID_MAX) {
			pr_err("%s: Invalid client id %d\n", __func__,
				client_id);
			return 0;
		}
		if (data->opcode == APR_BASIC_RSP_RESULT) {
			if (data->payload_size < (2 * sizeof(uint32_t))) {
				pr_err("%s: Invalid payload size %d\n", __func__,
					data->payload_size);
				return 0;
			}
			pr_debug("%s: APR_BASIC_RSP_RESULT id 0x%x\n",
				__func__, payload[0]);
			if (payload[1] != 0) {
				pr_err("%s: cmd = 0x%x returned error = 0x%x\n",
					__func__, payload[0], payload[1]);
			}
			switch (payload[0]) {
			case ADM_CMD_SET_PP_PARAMS_V5:
			case ADM_CMD_SET_PP_PARAMS_V6:
				pr_debug("%s: ADM_CMD_SET_PP_PARAMS\n",
					 __func__);
				if (client_id == ADM_CLIENT_ID_SOURCE_TRACKING)
					this_adm.sourceTrackingData.
						apr_cmd_status = payload[1];
				else if (rtac_make_adm_callback(payload,
							data->payload_size))
					break;
				/*
				 * if soft volume is called and already
				 * interrupted break out of the sequence here
				 */
			case ADM_CMD_DEVICE_OPEN_V5:
			case ADM_CMD_DEVICE_CLOSE_V5:
			case ADM_CMD_DEVICE_OPEN_V6:
			case ADM_CMD_DEVICE_OPEN_V8:
				pr_debug("%s: Basic callback received, wake up.\n",
					__func__);
				atomic_set(&this_adm.copp.stat[port_idx]
						[copp_idx], payload[1]);
				wake_up(
				&this_adm.copp.wait[port_idx][copp_idx]);
				break;
			case ADM_CMD_ADD_TOPOLOGIES:
				pr_debug("%s: callback received, ADM_CMD_ADD_TOPOLOGIES.\n",
					__func__);
				atomic_set(&this_adm.adm_stat, payload[1]);
				wake_up(&this_adm.adm_wait);
				break;
			case ADM_CMD_MATRIX_MAP_ROUTINGS_V5:
			case ADM_CMD_STREAM_DEVICE_MAP_ROUTINGS_V5:
				pr_debug("%s: Basic callback received, wake up.\n",
					__func__);
				atomic_set(&this_adm.matrix_map_stat,
					payload[1]);
				wake_up(&this_adm.matrix_map_wait);
				break;
			case ADM_CMD_SHARED_MEM_UNMAP_REGIONS:
				pr_debug("%s: ADM_CMD_SHARED_MEM_UNMAP_REGIONS\n",
					__func__);
				atomic_set(&this_adm.adm_stat, payload[1]);
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
					atomic_set(&this_adm.adm_stat,
						payload[1]);
					wake_up(&this_adm.adm_wait);
				}
				break;
			case ADM_CMD_GET_PP_PARAMS_V5:
			case ADM_CMD_GET_PP_PARAMS_V6:
				pr_debug("%s: ADM_CMD_GET_PP_PARAMS\n",
					 __func__);
				/* Should only come here if there is an APR */
				/* error or malformed APR packet. Otherwise */
				/* response will be returned as */
				/* ADM_CMDRSP_GET_PP_PARAMS_V5 */
				if (client_id ==
					ADM_CLIENT_ID_SOURCE_TRACKING) {
					this_adm.sourceTrackingData.
						apr_cmd_status = payload[1];
					if (payload[1] != 0)
						pr_err("%s: ADM get param error = %d\n",
							__func__, payload[1]);

					atomic_set(&this_adm.copp.stat
						[port_idx][copp_idx],
						payload[1]);
					wake_up(&this_adm.copp.wait
							[port_idx][copp_idx]);
				} else {
					if (payload[1] != 0) {
						pr_err("%s: ADM get param error = %d, resuming\n",
							__func__, payload[1]);

						rtac_make_adm_callback(payload,
							data->payload_size);
					}
				}
				break;
			case ADM_CMD_SET_PSPD_MTMX_STRTR_PARAMS_V5:
			case ADM_CMD_SET_PSPD_MTMX_STRTR_PARAMS_V6:
				pr_debug("%s:callback received PSPD MTMX, wake up\n",
					__func__);
				atomic_set(&this_adm.copp.stat[port_idx]
						[copp_idx], payload[1]);
				wake_up(
				&this_adm.copp.wait[port_idx][copp_idx]);
				break;
			case ADM_CMD_GET_PP_TOPO_MODULE_LIST:
			case ADM_CMD_GET_PP_TOPO_MODULE_LIST_V2:
				pr_debug("%s:ADM_CMD_GET_PP_TOPO_MODULE_LIST\n",
					 __func__);
				if (payload[1] != 0)
					pr_err("%s: ADM get topo list error = %d\n",
					       __func__, payload[1]);
				break;
			default:
				pr_err("%s: Unknown Cmd: 0x%x\n", __func__,
								payload[0]);
				break;
			}
			return 0;
		}

		switch (data->opcode) {
		case ADM_CMDRSP_DEVICE_OPEN_V5:
		case ADM_CMDRSP_DEVICE_OPEN_V6:
		case ADM_CMDRSP_DEVICE_OPEN_V8: {
			struct adm_cmd_rsp_device_open_v5 *open = NULL;
			if (data->payload_size <
				sizeof(struct adm_cmd_rsp_device_open_v5)) {
				pr_err("%s: Invalid payload size %d\n", __func__,
					data->payload_size);
				return 0;
			}
			open = (struct adm_cmd_rsp_device_open_v5 *)data->payload;
			if (open->copp_id == INVALID_COPP_ID) {
				pr_err("%s: invalid coppid rxed %d\n",
					__func__, open->copp_id);
				atomic_set(&this_adm.copp.stat[port_idx]
						[copp_idx], ADSP_EBADPARAM);
				wake_up(
				&this_adm.copp.wait[port_idx][copp_idx]);
				break;
			}
			atomic_set(&this_adm.copp.stat
				[port_idx][copp_idx], payload[0]);
			atomic_set(&this_adm.copp.id[port_idx][copp_idx],
				   open->copp_id);
			pr_debug("%s: coppid rxed=%d\n", __func__,
				 open->copp_id);
			wake_up(&this_adm.copp.wait[port_idx][copp_idx]);
			}
			break;
		case ADM_CMDRSP_GET_PP_PARAMS_V5:
		case ADM_CMDRSP_GET_PP_PARAMS_V6:
			pr_debug("%s: ADM_CMDRSP_GET_PP_PARAMS\n", __func__);
			if (client_id == ADM_CLIENT_ID_SOURCE_TRACKING)
				this_adm.sourceTrackingData.apr_cmd_status =
					payload[0];
			else if (rtac_make_adm_callback(payload,
							data->payload_size))
				break;

			idx = ADM_GET_PARAMETER_LENGTH * copp_idx;
			if (payload[0] == 0 && data->payload_size > 0) {
				ret = adm_process_get_param_response(
					data->opcode, idx, payload,
					data->payload_size);
				if (ret)
					pr_err("%s: Failed to process get param response, error %d\n",
					       __func__, ret);
			} else {
				adm_get_parameters[idx] = -1;
				pr_err("%s: ADM_CMDRSP_GET_PP_PARAMS returned error 0x%x\n",
				       __func__, payload[0]);
			}
			atomic_set(&this_adm.copp.stat[port_idx][copp_idx],
				   payload[0]);
			wake_up(&this_adm.copp.wait[port_idx][copp_idx]);
			break;
		case ADM_CMDRSP_GET_PP_TOPO_MODULE_LIST:
		case ADM_CMDRSP_GET_PP_TOPO_MODULE_LIST_V2:
			pr_debug("%s: ADM_CMDRSP_GET_PP_TOPO_MODULE_LIST\n",
				 __func__);
			num_modules = payload[1];
			pr_debug("%s: Num modules %d\n", __func__, num_modules);
			if (payload[0]) {
				pr_err("%s: ADM_CMDRSP_GET_PP_TOPO_MODULE_LIST, error = %d\n",
				       __func__, payload[0]);
			} else if (num_modules > MAX_MODULES_IN_TOPO) {
				pr_err("%s: ADM_CMDRSP_GET_PP_TOPO_MODULE_LIST invalid num modules received, num modules = %d\n",
				       __func__, num_modules);
			} else {
				ret = adm_process_get_topo_list_response(
					data->opcode, copp_idx, num_modules,
					payload, data->payload_size);
				if (ret)
					pr_err("%s: Failed to process get topo modules list response, error %d\n",
					       __func__, ret);
			}
			atomic_set(&this_adm.copp.stat[port_idx][copp_idx],
				   payload[0]);
			wake_up(&this_adm.copp.wait[port_idx][copp_idx]);
			break;
		case ADM_CMDRSP_SHARED_MEM_MAP_REGIONS:
			pr_debug("%s: ADM_CMDRSP_SHARED_MEM_MAP_REGIONS\n",
				__func__);
			atomic_set(&this_adm.mem_map_handles[
				   atomic_read(&this_adm.mem_map_index)],
				   *payload);
			atomic_set(&this_adm.adm_stat, 0);
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
	if (!mmap_region_cmd)
		return -ENOMEM;

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
		mregions->shm_addr_msw =
				msm_audio_populate_upper_32_bits(buf_add[i]);
		mregions->mem_size_bytes = bufsz[i];
		++mregions;
	}

	atomic_set(&this_adm.adm_stat, -1);
	ret = apr_send_pkt(this_adm.apr, (uint32_t *) mmap_region_cmd);
	if (ret < 0) {
		pr_err("%s: mmap_regions op[0x%x]rc[%d]\n", __func__,
					mmap_regions->hdr.opcode, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = wait_event_timeout(this_adm.adm_wait,
				 atomic_read(&this_adm.adm_stat) >= 0,
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: timeout. waited for memory_map\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	} else if (atomic_read(&this_adm.adm_stat) > 0) {
		pr_err("%s: DSP returned error[%s]\n",
				__func__, adsp_err_get_err_str(
				atomic_read(&this_adm.adm_stat)));
		ret = adsp_err_get_lnx_err_code(
				atomic_read(&this_adm.adm_stat));
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
		mem_map_handles[atomic_read(&this_adm.mem_map_index)]);
	atomic_set(&this_adm.adm_stat, -1);
	ret = apr_send_pkt(this_adm.apr, (uint32_t *) &unmap_regions);
	if (ret < 0) {
		pr_err("%s: mmap_regions op[0x%x]rc[%d]\n", __func__,
				unmap_regions.hdr.opcode, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = wait_event_timeout(this_adm.adm_wait,
				 atomic_read(&this_adm.adm_stat) >= 0,
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: timeout. waited for memory_unmap\n",
		       __func__);
		ret = -EINVAL;
		goto fail_cmd;
	} else if (atomic_read(&this_adm.adm_stat) > 0) {
		pr_err("%s: DSP returned error[%s]\n",
				__func__, adsp_err_get_err_str(
				atomic_read(&this_adm.adm_stat)));
		ret = adsp_err_get_lnx_err_code(
				atomic_read(&this_adm.adm_stat));
		goto fail_cmd;
	} else {
		pr_debug("%s: Unmap handle 0x%x succeeded\n", __func__,
			 unmap_regions.mem_map_handle);
	}
fail_cmd:
	return ret;
}

static int remap_cal_data(struct cal_block_data *cal_block, int cal_index)
{
	int ret = 0;

	if (cal_block->map_data.dma_buf == NULL) {
		pr_err("%s: No ION allocation for cal index %d!\n",
			__func__, cal_index);
		ret = -EINVAL;
		goto done;
	}

	if ((cal_block->map_data.map_size > 0) &&
		(cal_block->map_data.q6map_handle == 0)) {
		atomic_set(&this_adm.mem_map_index, cal_index);
		ret = adm_memory_map_regions(&cal_block->cal_data.paddr, 0,
				(uint32_t *)&cal_block->map_data.map_size, 1);
		if (ret < 0) {
			pr_err("%s: ADM mmap did not work! size = %zd ret %d\n",
				__func__,
				cal_block->map_data.map_size, ret);
			pr_debug("%s: ADM mmap did not work! addr = 0x%pK, size = %zd ret %d\n",
				__func__,
				&cal_block->cal_data.paddr,
				cal_block->map_data.map_size, ret);
			goto done;
		}
		cal_block->map_data.q6map_handle = atomic_read(&this_adm.
			mem_map_handles[cal_index]);
	}
done:
	return ret;
}

static void send_adm_custom_topology(void)
{
	struct cal_block_data *cal_block = NULL;
	struct cmd_set_topologies adm_top;
	int cal_index = ADM_CUSTOM_TOP_CAL;
	int result;

	if (this_adm.cal_data[cal_index] == NULL)
		goto done;

	mutex_lock(&this_adm.cal_data[cal_index]->lock);
	if (!this_adm.set_custom_topology)
		goto unlock;
	this_adm.set_custom_topology = 0;

	cal_block = cal_utils_get_only_cal_block(this_adm.cal_data[cal_index]);
	if (cal_block == NULL || cal_utils_is_cal_stale(cal_block))
		goto unlock;

	pr_debug("%s: Sending cal_index %d\n", __func__, cal_index);

	result = remap_cal_data(cal_block, cal_index);
	if (result) {
		pr_err("%s: Remap_cal_data failed for cal %d!\n",
			__func__, cal_index);
		goto unlock;
	}
	atomic_set(&this_adm.mem_map_index, cal_index);
	atomic_set(&this_adm.mem_map_handles[cal_index],
		cal_block->map_data.q6map_handle);

	if (cal_block->cal_data.size == 0) {
		pr_debug("%s: No ADM cal to send\n", __func__);
		goto unlock;
	}

	adm_top.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
		APR_HDR_LEN(20), APR_PKT_VER);
	adm_top.hdr.pkt_size = sizeof(adm_top);
	adm_top.hdr.src_svc = APR_SVC_ADM;
	adm_top.hdr.src_domain = APR_DOMAIN_APPS;
	adm_top.hdr.src_port = 0;
	adm_top.hdr.dest_svc = APR_SVC_ADM;
	adm_top.hdr.dest_domain = APR_DOMAIN_ADSP;
	adm_top.hdr.dest_port = 0;
	adm_top.hdr.token = 0;
	adm_top.hdr.opcode = ADM_CMD_ADD_TOPOLOGIES;
	adm_top.payload_addr_lsw = lower_32_bits(cal_block->cal_data.paddr);
	adm_top.payload_addr_msw = msm_audio_populate_upper_32_bits(
						cal_block->cal_data.paddr);
	adm_top.mem_map_handle = cal_block->map_data.q6map_handle;
	adm_top.payload_size = cal_block->cal_data.size;

	atomic_set(&this_adm.adm_stat, -1);
	pr_debug("%s: Sending ADM_CMD_ADD_TOPOLOGIES payload = 0x%pK, size = %d\n",
		__func__, &cal_block->cal_data.paddr,
		adm_top.payload_size);
	result = apr_send_pkt(this_adm.apr, (uint32_t *)&adm_top);
	if (result < 0) {
		pr_err("%s: Set topologies failed payload size = %zd result %d\n",
			__func__, cal_block->cal_data.size, result);
		goto unlock;
	}
	/* Wait for the callback */
	result = wait_event_timeout(this_adm.adm_wait,
				    atomic_read(&this_adm.adm_stat) >= 0,
				    msecs_to_jiffies(TIMEOUT_MS));
	if (!result) {
		pr_err("%s: Set topologies timed out payload size = %zd\n",
			__func__, cal_block->cal_data.size);
		goto unlock;
	} else if (atomic_read(&this_adm.adm_stat) > 0) {
		pr_err("%s: DSP returned error[%s]\n",
				__func__, adsp_err_get_err_str(
				atomic_read(&this_adm.adm_stat)));
		result = adsp_err_get_lnx_err_code(
				atomic_read(&this_adm.adm_stat));
		goto unlock;
	}
unlock:
	mutex_unlock(&this_adm.cal_data[cal_index]->lock);
done:
	return;
}

static int send_adm_cal_block(int port_id, int copp_idx,
			      struct cal_block_data *cal_block, int perf_mode)
{
	struct mem_mapping_hdr mem_hdr;
	int payload_size = 0;
	int port_idx = 0;
	int topology = 0;
	int result = 0;

	pr_debug("%s: Port id 0x%x,\n", __func__, port_id);

	if (!cal_block) {
		pr_debug("%s: No ADM cal to send for port_id = 0x%x!\n",
			__func__, port_id);
		result = -EINVAL;
		goto done;
	}
	if (cal_block->cal_data.size <= 0) {
		pr_debug("%s: No ADM cal sent for port_id = 0x%x!\n", __func__,
			 port_id);
		result = -EINVAL;
		goto done;
	}

	memset(&mem_hdr, 0, sizeof(mem_hdr));
	port_id = afe_convert_virtual_to_portid(port_id);
	port_idx = adm_validate_and_get_port_index(port_id);
	if (port_idx < 0 || port_idx >= AFE_MAX_PORTS) {
		pr_err("%s: Invalid port_id 0x%x\n", __func__, port_id);
		return -EINVAL;
	} else if (copp_idx < 0 || copp_idx >= MAX_COPPS_PER_PORT) {
		pr_err("%s: Invalid copp_idx 0x%x\n", __func__, copp_idx);
		return -EINVAL;
	}

	topology = atomic_read(&this_adm.copp.topology[port_idx][copp_idx]);
	if (perf_mode == LEGACY_PCM_MODE &&
	    topology == DS2_ADM_COPP_TOPOLOGY_ID) {
		pr_err("%s: perf_mode %d, topology 0x%x\n", __func__, perf_mode,
		       topology);
		goto done;
	}

	mem_hdr.data_payload_addr_lsw =
		lower_32_bits(cal_block->cal_data.paddr);
	mem_hdr.data_payload_addr_msw =
		msm_audio_populate_upper_32_bits(cal_block->cal_data.paddr);
	mem_hdr.mem_map_handle = cal_block->map_data.q6map_handle;
	payload_size = cal_block->cal_data.size;

	adm_set_pp_params(port_id, copp_idx, &mem_hdr, NULL, payload_size);

done:
	return result;
}

static struct cal_block_data *adm_find_cal_by_path(int cal_index, int path)
{
	struct list_head *ptr, *next;
	struct cal_block_data *cal_block = NULL;
	struct audio_cal_info_audproc *audproc_cal_info = NULL;
	struct audio_cal_info_audvol *audvol_cal_info = NULL;

	pr_debug("%s:\n", __func__);

	list_for_each_safe(ptr, next,
		&this_adm.cal_data[cal_index]->cal_blocks) {

		cal_block = list_entry(ptr,
			struct cal_block_data, list);

		if (cal_utils_is_cal_stale(cal_block))
			continue;

		if (cal_index == ADM_AUDPROC_CAL ||
		    cal_index == ADM_LSM_AUDPROC_CAL ||
		    cal_index == ADM_LSM_AUDPROC_PERSISTENT_CAL) {
			audproc_cal_info = cal_block->cal_info;
			if ((audproc_cal_info->path == path) &&
			    (cal_block->cal_data.size > 0))
				return cal_block;
		} else if (cal_index == ADM_AUDVOL_CAL) {
			audvol_cal_info = cal_block->cal_info;
			if ((audvol_cal_info->path == path) &&
			    (cal_block->cal_data.size > 0))
				return cal_block;
		}
	}
	pr_debug("%s: Can't find ADM cal for cal_index %d, path %d\n",
		__func__, cal_index, path);
	return NULL;
}

static struct cal_block_data *adm_find_cal_by_app_type(int cal_index, int path,
								int app_type)
{
	struct list_head *ptr, *next;
	struct cal_block_data *cal_block = NULL;
	struct audio_cal_info_audproc *audproc_cal_info = NULL;
	struct audio_cal_info_audvol *audvol_cal_info = NULL;

	pr_debug("%s\n", __func__);

	list_for_each_safe(ptr, next,
		&this_adm.cal_data[cal_index]->cal_blocks) {

		cal_block = list_entry(ptr,
			struct cal_block_data, list);

		if (cal_utils_is_cal_stale(cal_block))
			continue;

		if (cal_index == ADM_AUDPROC_CAL ||
		    cal_index == ADM_LSM_AUDPROC_CAL ||
		    cal_index == ADM_LSM_AUDPROC_PERSISTENT_CAL) {
			audproc_cal_info = cal_block->cal_info;
			if ((audproc_cal_info->path == path) &&
			    (audproc_cal_info->app_type == app_type) &&
			    (cal_block->cal_data.size > 0))
				return cal_block;
		} else if (cal_index == ADM_AUDVOL_CAL) {
			audvol_cal_info = cal_block->cal_info;
			if ((audvol_cal_info->path == path) &&
			    (audvol_cal_info->app_type == app_type) &&
			    (cal_block->cal_data.size > 0))
				return cal_block;
		}
	}
	pr_debug("%s: Can't find ADM cali for cal_index %d, path %d, app %d, defaulting to search by path\n",
		__func__, cal_index, path, app_type);
	return adm_find_cal_by_path(cal_index, path);
}


static struct cal_block_data *adm_find_cal(int cal_index, int path,
					   int app_type, int acdb_id,
					   int sample_rate)
{
	struct list_head *ptr, *next;
	struct cal_block_data *cal_block = NULL;
	struct audio_cal_info_audproc *audproc_cal_info = NULL;
	struct audio_cal_info_audvol *audvol_cal_info = NULL;

	pr_debug("%s:\n", __func__);

	list_for_each_safe(ptr, next,
		&this_adm.cal_data[cal_index]->cal_blocks) {

		cal_block = list_entry(ptr,
			struct cal_block_data, list);
		if (cal_utils_is_cal_stale(cal_block))
			continue;

		if (cal_index == ADM_AUDPROC_CAL ||
		    cal_index == ADM_LSM_AUDPROC_CAL ||
		    cal_index == ADM_LSM_AUDPROC_PERSISTENT_CAL) {
			audproc_cal_info = cal_block->cal_info;
			if ((audproc_cal_info->path == path) &&
			    (audproc_cal_info->app_type == app_type) &&
			    (audproc_cal_info->acdb_id == acdb_id) &&
			    (audproc_cal_info->sample_rate == sample_rate) &&
			    (cal_block->cal_data.size > 0))
				return cal_block;
		} else if (cal_index == ADM_AUDVOL_CAL) {
			audvol_cal_info = cal_block->cal_info;
			if ((audvol_cal_info->path == path) &&
			    (audvol_cal_info->app_type == app_type) &&
			    (audvol_cal_info->acdb_id == acdb_id) &&
			    (cal_block->cal_data.size > 0))
				return cal_block;
		}
	}
	pr_debug("%s: Can't find ADM cal for cal_index %d, path %d, app %d, acdb_id %d sample_rate %d defaulting to search by app type\n",
		__func__, cal_index, path, app_type, acdb_id, sample_rate);
	return adm_find_cal_by_app_type(cal_index, path, app_type);
}

static int adm_remap_and_send_cal_block(int cal_index, int port_id,
	int copp_idx, struct cal_block_data *cal_block, int perf_mode,
	int app_type, int acdb_id, int sample_rate)
{
	int ret = 0;

	pr_debug("%s: Sending cal_index cal %d\n", __func__, cal_index);
	ret = remap_cal_data(cal_block, cal_index);
	if (ret) {
		pr_err("%s: Remap_cal_data failed for cal %d!\n",
			__func__, cal_index);
		goto done;
	}
	ret = send_adm_cal_block(port_id, copp_idx, cal_block, perf_mode);
	if (ret < 0)
		pr_debug("%s: No cal sent for cal_index %d, port_id = 0x%x! ret %d sample_rate %d\n",
			__func__, cal_index, port_id, ret, sample_rate);
done:
	return ret;
}

static void send_adm_cal_type(int cal_index, int path, int port_id,
			      int copp_idx, int perf_mode, int app_type,
			      int acdb_id, int sample_rate)
{
	struct cal_block_data		*cal_block = NULL;
	int ret;

	pr_debug("%s: cal index %d\n", __func__, cal_index);

	if (this_adm.cal_data[cal_index] == NULL) {
		pr_debug("%s: cal_index %d not allocated!\n",
			__func__, cal_index);
		goto done;
	}

	mutex_lock(&this_adm.cal_data[cal_index]->lock);
	cal_block = adm_find_cal(cal_index, path, app_type, acdb_id,
				sample_rate);
	if (cal_block == NULL)
		goto unlock;

	ret = adm_remap_and_send_cal_block(cal_index, port_id, copp_idx,
		cal_block, perf_mode, app_type, acdb_id, sample_rate);

	cal_utils_mark_cal_used(cal_block);
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
			 int app_type, int acdb_id, int sample_rate,
			 int passthr_mode)
{
	pr_debug("%s: port id 0x%x copp_idx %d\n", __func__, port_id, copp_idx);

	if (passthr_mode != LISTEN) {
		send_adm_cal_type(ADM_AUDPROC_CAL, path, port_id, copp_idx,
				perf_mode, app_type, acdb_id, sample_rate);
	} else {
		send_adm_cal_type(ADM_LSM_AUDPROC_CAL, path, port_id, copp_idx,
				  perf_mode, app_type, acdb_id, sample_rate);

		send_adm_cal_type(ADM_LSM_AUDPROC_PERSISTENT_CAL, path,
				  port_id, copp_idx, perf_mode, app_type,
				  acdb_id, sample_rate);
	}

	send_adm_cal_type(ADM_AUDVOL_CAL, path, port_id, copp_idx, perf_mode,
			  app_type, acdb_id, sample_rate);
}

/**
 * adm_connect_afe_port -
 *        command to send ADM connect AFE port
 *
 * @mode: value of mode for ADM connect AFE
 * @session_id: session active to connect
 * @port_id: Port ID number
 *
 * Returns 0 on success or error on failure
 */
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

	atomic_set(&this_adm.copp.stat[port_idx][copp_idx], -1);
	ret = apr_send_pkt(this_adm.apr, (uint32_t *)&cmd);
	if (ret < 0) {
		pr_err("%s: ADM enable for port_id: 0x%x failed ret %d\n",
					__func__, port_id, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}
	/* Wait for the callback with copp id */
	ret = wait_event_timeout(this_adm.copp.wait[port_idx][copp_idx],
		atomic_read(&this_adm.copp.stat[port_idx][copp_idx]) >= 0,
		msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: ADM connect timedout for port_id: 0x%x\n",
			__func__, port_id);
		ret = -EINVAL;
		goto fail_cmd;
	} else if (atomic_read(&this_adm.copp.stat
				[port_idx][copp_idx]) > 0) {
		pr_err("%s: DSP returned error[%s]\n",
				__func__, adsp_err_get_err_str(
				atomic_read(&this_adm.copp.stat
				[port_idx][copp_idx])));
		ret = adsp_err_get_lnx_err_code(
				atomic_read(&this_adm.copp.stat
					[port_idx][copp_idx]));
		goto fail_cmd;
	}
	atomic_inc(&this_adm.copp.cnt[port_idx][copp_idx]);
	return 0;

fail_cmd:

	return ret;
}
EXPORT_SYMBOL(adm_connect_afe_port);

int adm_arrange_mch_map(struct adm_cmd_device_open_v5 *open, int path,
			 int channel_mode)
{
	int rc = 0, idx;

	pr_debug("%s: channel mode %d", __func__, channel_mode);

	memset(open->dev_channel_mapping, 0, PCM_FORMAT_MAX_NUM_CHANNEL);
	switch (path) {
	case ADM_PATH_PLAYBACK:
		idx = ADM_MCH_MAP_IDX_PLAYBACK;
		break;
	case ADM_PATH_LIVE_REC:
	case ADM_PATH_NONLIVE_REC:
		idx = ADM_MCH_MAP_IDX_REC;
		break;
	default:
		goto non_mch_path;
	};
	if ((open->dev_num_channel > 2) && multi_ch_maps[idx].set_channel_map) {
		memcpy(open->dev_channel_mapping,
			multi_ch_maps[idx].channel_mapping,
			PCM_FORMAT_MAX_NUM_CHANNEL);
	} else {
		if (channel_mode == 1) {
			open->dev_channel_mapping[0] = PCM_CHANNEL_FC;
		} else if (channel_mode == 2) {
			open->dev_channel_mapping[0] = PCM_CHANNEL_FL;
			open->dev_channel_mapping[1] = PCM_CHANNEL_FR;
		} else if (channel_mode == 3) {
			open->dev_channel_mapping[0] = PCM_CHANNEL_FL;
			open->dev_channel_mapping[1] = PCM_CHANNEL_FR;
			open->dev_channel_mapping[2] = PCM_CHANNEL_FC;
		} else if (channel_mode == 4) {
			open->dev_channel_mapping[0] = PCM_CHANNEL_FL;
			open->dev_channel_mapping[1] = PCM_CHANNEL_FR;
			open->dev_channel_mapping[2] = PCM_CHANNEL_LS;
			open->dev_channel_mapping[3] = PCM_CHANNEL_RS;
		} else if (channel_mode == 5) {
			open->dev_channel_mapping[0] = PCM_CHANNEL_FL;
			open->dev_channel_mapping[1] = PCM_CHANNEL_FR;
			open->dev_channel_mapping[2] = PCM_CHANNEL_FC;
			open->dev_channel_mapping[3] = PCM_CHANNEL_LS;
			open->dev_channel_mapping[4] = PCM_CHANNEL_RS;
		} else if (channel_mode == 6) {
			open->dev_channel_mapping[0] = PCM_CHANNEL_FL;
			open->dev_channel_mapping[1] = PCM_CHANNEL_FR;
			open->dev_channel_mapping[2] = PCM_CHANNEL_LFE;
			open->dev_channel_mapping[3] = PCM_CHANNEL_FC;
			open->dev_channel_mapping[4] = PCM_CHANNEL_LS;
			open->dev_channel_mapping[5] = PCM_CHANNEL_RS;
		} else if (channel_mode == 7) {
			open->dev_channel_mapping[0] = PCM_CHANNEL_FL;
			open->dev_channel_mapping[1] = PCM_CHANNEL_FR;
			open->dev_channel_mapping[2] = PCM_CHANNEL_FC;
			open->dev_channel_mapping[3] = PCM_CHANNEL_LFE;
			open->dev_channel_mapping[4] = PCM_CHANNEL_LB;
			open->dev_channel_mapping[5] = PCM_CHANNEL_RB;
			open->dev_channel_mapping[6] = PCM_CHANNEL_CS;
		} else if (channel_mode == 8) {
			open->dev_channel_mapping[0] = PCM_CHANNEL_FL;
			open->dev_channel_mapping[1] = PCM_CHANNEL_FR;
			open->dev_channel_mapping[2] = PCM_CHANNEL_LFE;
			open->dev_channel_mapping[3] = PCM_CHANNEL_FC;
			open->dev_channel_mapping[4] = PCM_CHANNEL_LS;
			open->dev_channel_mapping[5] = PCM_CHANNEL_RS;
			open->dev_channel_mapping[6] = PCM_CHANNEL_LB;
			open->dev_channel_mapping[7] = PCM_CHANNEL_RB;
		} else {
			pr_err("%s: invalid num_chan %d\n", __func__,
				channel_mode);
			rc = -EINVAL;
			goto inval_ch_mod;
		}
	}

non_mch_path:
inval_ch_mod:
	return rc;
}

int adm_arrange_mch_ep2_map(struct adm_cmd_device_open_v6 *open_v6,
			 int channel_mode)
{
	int rc = 0;

	memset(open_v6->dev_channel_mapping_eid2, 0,
	       PCM_FORMAT_MAX_NUM_CHANNEL);

	if (channel_mode == 1)	{
		open_v6->dev_channel_mapping_eid2[0] = PCM_CHANNEL_FC;
	} else if (channel_mode == 2) {
		open_v6->dev_channel_mapping_eid2[0] = PCM_CHANNEL_FL;
		open_v6->dev_channel_mapping_eid2[1] = PCM_CHANNEL_FR;
	} else if (channel_mode == 3)	{
		open_v6->dev_channel_mapping_eid2[0] = PCM_CHANNEL_FL;
		open_v6->dev_channel_mapping_eid2[1] = PCM_CHANNEL_FR;
		open_v6->dev_channel_mapping_eid2[2] = PCM_CHANNEL_FC;
	} else if (channel_mode == 4) {
		open_v6->dev_channel_mapping_eid2[0] = PCM_CHANNEL_FL;
		open_v6->dev_channel_mapping_eid2[1] = PCM_CHANNEL_FR;
		open_v6->dev_channel_mapping_eid2[2] = PCM_CHANNEL_LS;
		open_v6->dev_channel_mapping_eid2[3] = PCM_CHANNEL_RS;
	} else if (channel_mode == 5) {
		open_v6->dev_channel_mapping_eid2[0] = PCM_CHANNEL_FL;
		open_v6->dev_channel_mapping_eid2[1] = PCM_CHANNEL_FR;
		open_v6->dev_channel_mapping_eid2[2] = PCM_CHANNEL_FC;
		open_v6->dev_channel_mapping_eid2[3] = PCM_CHANNEL_LS;
		open_v6->dev_channel_mapping_eid2[4] = PCM_CHANNEL_RS;
	} else if (channel_mode == 6) {
		open_v6->dev_channel_mapping_eid2[0] = PCM_CHANNEL_FL;
		open_v6->dev_channel_mapping_eid2[1] = PCM_CHANNEL_FR;
		open_v6->dev_channel_mapping_eid2[2] = PCM_CHANNEL_LFE;
		open_v6->dev_channel_mapping_eid2[3] = PCM_CHANNEL_FC;
		open_v6->dev_channel_mapping_eid2[4] = PCM_CHANNEL_LS;
		open_v6->dev_channel_mapping_eid2[5] = PCM_CHANNEL_RS;
	} else if (channel_mode == 8) {
		open_v6->dev_channel_mapping_eid2[0] = PCM_CHANNEL_FL;
		open_v6->dev_channel_mapping_eid2[1] = PCM_CHANNEL_FR;
		open_v6->dev_channel_mapping_eid2[2] = PCM_CHANNEL_LFE;
		open_v6->dev_channel_mapping_eid2[3] = PCM_CHANNEL_FC;
		open_v6->dev_channel_mapping_eid2[4] = PCM_CHANNEL_LS;
		open_v6->dev_channel_mapping_eid2[5] = PCM_CHANNEL_RS;
		open_v6->dev_channel_mapping_eid2[6] = PCM_CHANNEL_LB;
		open_v6->dev_channel_mapping_eid2[7] = PCM_CHANNEL_RB;
	} else {
		pr_err("%s: invalid num_chan %d\n", __func__,
			channel_mode);
		rc = -EINVAL;
	}

	return rc;
}

static int adm_arrange_mch_map_v8(
		struct adm_device_endpoint_payload *ep_payload,
		int path,
		int channel_mode)
{
	int rc = 0, idx;

	memset(ep_payload->dev_channel_mapping,
			0, PCM_FORMAT_MAX_NUM_CHANNEL_V8);
	switch (path) {
	case ADM_PATH_PLAYBACK:
		idx = ADM_MCH_MAP_IDX_PLAYBACK;
		break;
	case ADM_PATH_LIVE_REC:
	case ADM_PATH_NONLIVE_REC:
		idx = ADM_MCH_MAP_IDX_REC;
		break;
	default:
		goto non_mch_path;
	};

	if ((ep_payload->dev_num_channel > 2) &&
			multi_ch_maps[idx].set_channel_map) {
		memcpy(ep_payload->dev_channel_mapping,
			multi_ch_maps[idx].channel_mapping,
			PCM_FORMAT_MAX_NUM_CHANNEL_V8);
	} else {
		if (channel_mode == 1) {
			ep_payload->dev_channel_mapping[0] = PCM_CHANNEL_FC;
		} else if (channel_mode == 2) {
			ep_payload->dev_channel_mapping[0] = PCM_CHANNEL_FL;
			ep_payload->dev_channel_mapping[1] = PCM_CHANNEL_FR;
		} else if (channel_mode == 3) {
			ep_payload->dev_channel_mapping[0] = PCM_CHANNEL_FL;
			ep_payload->dev_channel_mapping[1] = PCM_CHANNEL_FR;
			ep_payload->dev_channel_mapping[2] = PCM_CHANNEL_FC;
		} else if (channel_mode == 4) {
			ep_payload->dev_channel_mapping[0] = PCM_CHANNEL_FL;
			ep_payload->dev_channel_mapping[1] = PCM_CHANNEL_FR;
			ep_payload->dev_channel_mapping[2] = PCM_CHANNEL_LS;
			ep_payload->dev_channel_mapping[3] = PCM_CHANNEL_RS;
		} else if (channel_mode == 5) {
			ep_payload->dev_channel_mapping[0] = PCM_CHANNEL_FL;
			ep_payload->dev_channel_mapping[1] = PCM_CHANNEL_FR;
			ep_payload->dev_channel_mapping[2] = PCM_CHANNEL_FC;
			ep_payload->dev_channel_mapping[3] = PCM_CHANNEL_LS;
			ep_payload->dev_channel_mapping[4] = PCM_CHANNEL_RS;
		} else if (channel_mode == 6) {
			ep_payload->dev_channel_mapping[0] = PCM_CHANNEL_FL;
			ep_payload->dev_channel_mapping[1] = PCM_CHANNEL_FR;
			ep_payload->dev_channel_mapping[2] = PCM_CHANNEL_LFE;
			ep_payload->dev_channel_mapping[3] = PCM_CHANNEL_FC;
			ep_payload->dev_channel_mapping[4] = PCM_CHANNEL_LS;
			ep_payload->dev_channel_mapping[5] = PCM_CHANNEL_RS;
		} else if (channel_mode == 7) {
			ep_payload->dev_channel_mapping[0] = PCM_CHANNEL_FL;
			ep_payload->dev_channel_mapping[1] = PCM_CHANNEL_FR;
			ep_payload->dev_channel_mapping[2] = PCM_CHANNEL_FC;
			ep_payload->dev_channel_mapping[3] = PCM_CHANNEL_LFE;
			ep_payload->dev_channel_mapping[4] = PCM_CHANNEL_LB;
			ep_payload->dev_channel_mapping[5] = PCM_CHANNEL_RB;
			ep_payload->dev_channel_mapping[6] = PCM_CHANNEL_CS;
		} else if (channel_mode == 8) {
			ep_payload->dev_channel_mapping[0] = PCM_CHANNEL_FL;
			ep_payload->dev_channel_mapping[1] = PCM_CHANNEL_FR;
			ep_payload->dev_channel_mapping[2] = PCM_CHANNEL_LFE;
			ep_payload->dev_channel_mapping[3] = PCM_CHANNEL_FC;
			ep_payload->dev_channel_mapping[4] = PCM_CHANNEL_LS;
			ep_payload->dev_channel_mapping[5] = PCM_CHANNEL_RS;
			ep_payload->dev_channel_mapping[6] = PCM_CHANNEL_LB;
			ep_payload->dev_channel_mapping[7] = PCM_CHANNEL_RB;
		} else if (channel_mode == 10) {
			ep_payload->dev_channel_mapping[0] = PCM_CHANNEL_FL;
			ep_payload->dev_channel_mapping[1] = PCM_CHANNEL_FR;
			ep_payload->dev_channel_mapping[2] = PCM_CHANNEL_LFE;
			ep_payload->dev_channel_mapping[3] = PCM_CHANNEL_FC;
			ep_payload->dev_channel_mapping[4] = PCM_CHANNEL_LB;
			ep_payload->dev_channel_mapping[5] = PCM_CHANNEL_RB;
			ep_payload->dev_channel_mapping[6] = PCM_CHANNEL_LS;
			ep_payload->dev_channel_mapping[7] = PCM_CHANNEL_RS;
			ep_payload->dev_channel_mapping[8] = PCM_CHANNEL_TFL;
			ep_payload->dev_channel_mapping[9] = PCM_CHANNEL_TFR;
		} else if (channel_mode == 12) {
			ep_payload->dev_channel_mapping[0] = PCM_CHANNEL_FL;
			ep_payload->dev_channel_mapping[1] = PCM_CHANNEL_FR;
			ep_payload->dev_channel_mapping[2] = PCM_CHANNEL_LFE;
			ep_payload->dev_channel_mapping[3] = PCM_CHANNEL_FC;
			ep_payload->dev_channel_mapping[4] = PCM_CHANNEL_LB;
			ep_payload->dev_channel_mapping[5] = PCM_CHANNEL_RB;
			ep_payload->dev_channel_mapping[6] = PCM_CHANNEL_LS;
			ep_payload->dev_channel_mapping[7] = PCM_CHANNEL_RS;
			ep_payload->dev_channel_mapping[8] = PCM_CHANNEL_TFL;
			ep_payload->dev_channel_mapping[9] = PCM_CHANNEL_TFR;
			ep_payload->dev_channel_mapping[10] = PCM_CHANNEL_TSL;
			ep_payload->dev_channel_mapping[11] = PCM_CHANNEL_TSR;
		} else if (channel_mode == 16) {
			ep_payload->dev_channel_mapping[0] = PCM_CHANNEL_FL;
			ep_payload->dev_channel_mapping[1] = PCM_CHANNEL_FR;
			ep_payload->dev_channel_mapping[2] = PCM_CHANNEL_LFE;
			ep_payload->dev_channel_mapping[3] = PCM_CHANNEL_FC;
			ep_payload->dev_channel_mapping[4] = PCM_CHANNEL_LB;
			ep_payload->dev_channel_mapping[5] = PCM_CHANNEL_RB;
			ep_payload->dev_channel_mapping[6] = PCM_CHANNEL_LS;
			ep_payload->dev_channel_mapping[7] = PCM_CHANNEL_RS;
			ep_payload->dev_channel_mapping[8] = PCM_CHANNEL_TFL;
			ep_payload->dev_channel_mapping[9] = PCM_CHANNEL_TFR;
			ep_payload->dev_channel_mapping[10] = PCM_CHANNEL_TSL;
			ep_payload->dev_channel_mapping[11] = PCM_CHANNEL_TSR;
			ep_payload->dev_channel_mapping[12] = PCM_CHANNEL_FLC;
			ep_payload->dev_channel_mapping[13] = PCM_CHANNEL_FRC;
			ep_payload->dev_channel_mapping[14] = PCM_CHANNEL_RLC;
			ep_payload->dev_channel_mapping[15] = PCM_CHANNEL_RRC;
		} else {
			pr_err("%s: invalid num_chan %d\n", __func__,
				channel_mode);
			rc = -EINVAL;
			goto inval_ch_mod;
		}
	}

non_mch_path:
inval_ch_mod:
	return rc;
}

static int adm_arrange_mch_ep2_map_v8(
		struct adm_device_endpoint_payload *ep_payload,
		int channel_mode)
{
	int rc = 0;

	memset(ep_payload->dev_channel_mapping, 0,
	       PCM_FORMAT_MAX_NUM_CHANNEL_V8);

	if (channel_mode == 1) {
		ep_payload->dev_channel_mapping[0] = PCM_CHANNEL_FC;
	} else if (channel_mode == 2) {
		ep_payload->dev_channel_mapping[0] = PCM_CHANNEL_FL;
		ep_payload->dev_channel_mapping[1] = PCM_CHANNEL_FR;
	} else if (channel_mode == 3) {
		ep_payload->dev_channel_mapping[0] = PCM_CHANNEL_FL;
		ep_payload->dev_channel_mapping[1] = PCM_CHANNEL_FR;
		ep_payload->dev_channel_mapping[2] = PCM_CHANNEL_FC;
	} else if (channel_mode == 4) {
		ep_payload->dev_channel_mapping[0] = PCM_CHANNEL_FL;
		ep_payload->dev_channel_mapping[1] = PCM_CHANNEL_FR;
		ep_payload->dev_channel_mapping[2] = PCM_CHANNEL_LS;
		ep_payload->dev_channel_mapping[3] = PCM_CHANNEL_RS;
	} else if (channel_mode == 5) {
		ep_payload->dev_channel_mapping[0] = PCM_CHANNEL_FL;
		ep_payload->dev_channel_mapping[1] = PCM_CHANNEL_FR;
		ep_payload->dev_channel_mapping[2] = PCM_CHANNEL_FC;
		ep_payload->dev_channel_mapping[3] = PCM_CHANNEL_LS;
		ep_payload->dev_channel_mapping[4] = PCM_CHANNEL_RS;
	} else if (channel_mode == 6) {
		ep_payload->dev_channel_mapping[0] = PCM_CHANNEL_FL;
		ep_payload->dev_channel_mapping[1] = PCM_CHANNEL_FR;
		ep_payload->dev_channel_mapping[2] = PCM_CHANNEL_LFE;
		ep_payload->dev_channel_mapping[3] = PCM_CHANNEL_FC;
		ep_payload->dev_channel_mapping[4] = PCM_CHANNEL_LS;
		ep_payload->dev_channel_mapping[5] = PCM_CHANNEL_RS;
	} else if (channel_mode == 8) {
		ep_payload->dev_channel_mapping[0] = PCM_CHANNEL_FL;
		ep_payload->dev_channel_mapping[1] = PCM_CHANNEL_FR;
		ep_payload->dev_channel_mapping[2] = PCM_CHANNEL_LFE;
		ep_payload->dev_channel_mapping[3] = PCM_CHANNEL_FC;
		ep_payload->dev_channel_mapping[4] = PCM_CHANNEL_LS;
		ep_payload->dev_channel_mapping[5] = PCM_CHANNEL_RS;
		ep_payload->dev_channel_mapping[6] = PCM_CHANNEL_LB;
		ep_payload->dev_channel_mapping[7] = PCM_CHANNEL_RB;
	}  else if (channel_mode == 10) {
		ep_payload->dev_channel_mapping[0] = PCM_CHANNEL_FL;
		ep_payload->dev_channel_mapping[1] = PCM_CHANNEL_FR;
		ep_payload->dev_channel_mapping[2] = PCM_CHANNEL_LFE;
		ep_payload->dev_channel_mapping[3] = PCM_CHANNEL_FC;
		ep_payload->dev_channel_mapping[4] = PCM_CHANNEL_LS;
		ep_payload->dev_channel_mapping[5] = PCM_CHANNEL_RS;
		ep_payload->dev_channel_mapping[6] = PCM_CHANNEL_LB;
		ep_payload->dev_channel_mapping[7] = PCM_CHANNEL_RB;
		ep_payload->dev_channel_mapping[8] = PCM_CHANNEL_CS;
		ep_payload->dev_channel_mapping[9] = PCM_CHANNELS;
	} else if (channel_mode == 12) {
		ep_payload->dev_channel_mapping[0] = PCM_CHANNEL_FL;
		ep_payload->dev_channel_mapping[1] = PCM_CHANNEL_FR;
		ep_payload->dev_channel_mapping[2] = PCM_CHANNEL_LFE;
		ep_payload->dev_channel_mapping[3] = PCM_CHANNEL_FC;
		ep_payload->dev_channel_mapping[4] = PCM_CHANNEL_LS;
		ep_payload->dev_channel_mapping[5] = PCM_CHANNEL_RS;
		ep_payload->dev_channel_mapping[6] = PCM_CHANNEL_LB;
		ep_payload->dev_channel_mapping[7] = PCM_CHANNEL_RB;
		ep_payload->dev_channel_mapping[8] = PCM_CHANNEL_TFL;
		ep_payload->dev_channel_mapping[9] = PCM_CHANNEL_TFR;
		ep_payload->dev_channel_mapping[10] = PCM_CHANNEL_TSL;
		ep_payload->dev_channel_mapping[11] = PCM_CHANNEL_TSR;
	} else if (channel_mode == 16) {
		ep_payload->dev_channel_mapping[0] = PCM_CHANNEL_FL;
		ep_payload->dev_channel_mapping[1] = PCM_CHANNEL_FR;
		ep_payload->dev_channel_mapping[2] = PCM_CHANNEL_LFE;
		ep_payload->dev_channel_mapping[3] = PCM_CHANNEL_FC;
		ep_payload->dev_channel_mapping[4] = PCM_CHANNEL_LS;
		ep_payload->dev_channel_mapping[5] = PCM_CHANNEL_RS;
		ep_payload->dev_channel_mapping[6] = PCM_CHANNEL_LB;
		ep_payload->dev_channel_mapping[7] = PCM_CHANNEL_RB;
		ep_payload->dev_channel_mapping[8] = PCM_CHANNEL_CS;
		ep_payload->dev_channel_mapping[9] = PCM_CHANNELS;
		ep_payload->dev_channel_mapping[10] = PCM_CHANNEL_CVH;
		ep_payload->dev_channel_mapping[11] = PCM_CHANNEL_MS;
		ep_payload->dev_channel_mapping[12] = PCM_CHANNEL_FLC;
		ep_payload->dev_channel_mapping[13] = PCM_CHANNEL_FRC;
		ep_payload->dev_channel_mapping[14] = PCM_CHANNEL_RLC;
		ep_payload->dev_channel_mapping[15] = PCM_CHANNEL_RRC;
	} else {
		pr_err("%s: invalid num_chan %d\n", __func__,
			channel_mode);
		rc = -EINVAL;
	}

	return rc;
}
/**
 * adm_open -
 *        command to send ADM open
 *
 * @port_id: port id number
 * @path: direction or ADM path type
 * @rate: sample rate of session
 * @channel_mode: number of channels set
 * @topology: topology active for this session
 * @perf_mode: performance mode like LL/ULL/..
 * @bit_width: bit width to set for copp
 * @app_type: App type used for this session
 * @acdb_id: ACDB ID of this device
 *
 * Returns 0 on success or error on failure
 */
int adm_open(int port_id, int path, int rate, int channel_mode, int topology,
	     int perf_mode, uint16_t bit_width, int app_type, int acdb_id)
{
	struct adm_cmd_device_open_v5	open;
	struct adm_cmd_device_open_v6	open_v6;
	struct adm_cmd_device_open_v8	open_v8;
	struct adm_device_endpoint_payload ep1_payload;
	struct adm_device_endpoint_payload ep2_payload;
	int ep1_payload_size = 0;
	int ep2_payload_size = 0;
	int ret = 0;
	int port_idx, flags;
	int copp_idx = -1;
	int tmp_port = q6audio_get_port_id(port_id);
	void *adm_params = NULL;
	int param_size;

	pr_debug("%s:port %#x path:%d rate:%d mode:%d perf_mode:%d,topo_id %d\n",
		 __func__, port_id, path, rate, channel_mode, perf_mode,
		 topology);

	port_id = q6audio_convert_virtual_to_portid(port_id);
	port_idx = adm_validate_and_get_port_index(port_id);
	if (port_idx < 0) {
		pr_err("%s: Invalid port_id 0x%x\n", __func__, port_id);
		return -EINVAL;
	}
	if (channel_mode < 0 || channel_mode > 32) {
		pr_err("%s: Invalid channel number 0x%x\n",
				__func__, channel_mode);
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

	if (perf_mode == ULL_POST_PROCESSING_PCM_MODE) {
		flags = ADM_ULL_POST_PROCESSING_DEVICE_SESSION;
		if ((topology == DOLBY_ADM_COPP_TOPOLOGY_ID) ||
		    (topology == DS2_ADM_COPP_TOPOLOGY_ID) ||
		    (topology == SRS_TRUMEDIA_TOPOLOGY_ID))
			topology = DEFAULT_COPP_TOPOLOGY;
	} else if (perf_mode == ULTRA_LOW_LATENCY_PCM_MODE) {
		flags = ADM_ULTRA_LOW_LATENCY_DEVICE_SESSION;
		topology = NULL_COPP_TOPOLOGY;
		rate = ULL_SUPPORTED_SAMPLE_RATE;
		bit_width = ULL_SUPPORTED_BITS_PER_SAMPLE;
	} else if (perf_mode == LOW_LATENCY_PCM_MODE) {
		flags = ADM_LOW_LATENCY_DEVICE_SESSION;
		if ((topology == DOLBY_ADM_COPP_TOPOLOGY_ID) ||
		    (topology == DS2_ADM_COPP_TOPOLOGY_ID) ||
		    (topology == SRS_TRUMEDIA_TOPOLOGY_ID))
			topology = DEFAULT_COPP_TOPOLOGY;
	} else {
		if ((path == ADM_PATH_COMPRESSED_RX) ||
		    (path == ADM_PATH_COMPRESSED_TX))
			flags = 0;
		else
			flags = ADM_LEGACY_DEVICE_SESSION;
	}

	if ((topology == VPM_TX_SM_ECNS_V2_COPP_TOPOLOGY) ||
	    (topology == VPM_TX_DM_FLUENCE_COPP_TOPOLOGY) ||
	    (topology == VPM_TX_DM_RFECNS_COPP_TOPOLOGY)||
	    (topology == VPM_TX_DM_FLUENCE_EF_COPP_TOPOLOGY)) {
		if ((rate != ADM_CMD_COPP_OPEN_SAMPLE_RATE_8K) &&
		    (rate != ADM_CMD_COPP_OPEN_SAMPLE_RATE_16K) &&
		    (rate != ADM_CMD_COPP_OPEN_SAMPLE_RATE_32K) &&
		    (rate != ADM_CMD_COPP_OPEN_SAMPLE_RATE_48K))
		rate = 16000;
	}

	if (topology == VPM_TX_VOICE_SMECNS_V2_COPP_TOPOLOGY ||
		topology == ADM_TOPOLOGY_ID_AUDIO_RX_FVSAM) {
		pr_debug("%s: set channel_mode as 1 for topology=%d\n", __func__, topology);
		channel_mode = 1;
	}

	/*
	 * Routing driver reuses the same adm for streams with the same
	 * app_type, sample_rate etc.
	 * This isn't allowed for ULL streams as per the DSP interface
	 */
	if (perf_mode != ULTRA_LOW_LATENCY_PCM_MODE)
		copp_idx = adm_get_idx_if_copp_exists(port_idx, topology,
						      perf_mode,
						      rate, bit_width,
						      app_type);

	if (copp_idx < 0) {
		copp_idx = adm_get_next_available_copp(port_idx);
		if (copp_idx >= MAX_COPPS_PER_PORT) {
			pr_err("%s: exceeded copp id %d\n",
				 __func__, copp_idx);
			return -EINVAL;
		}
		atomic_set(&this_adm.copp.cnt[port_idx][copp_idx], 0);
		atomic_set(&this_adm.copp.topology[port_idx][copp_idx],
			   topology);
		atomic_set(&this_adm.copp.mode[port_idx][copp_idx],
			   perf_mode);
		atomic_set(&this_adm.copp.rate[port_idx][copp_idx],
			   rate);
		atomic_set(&this_adm.copp.channels[port_idx][copp_idx],
			   channel_mode);
		atomic_set(&this_adm.copp.bit_width[port_idx][copp_idx],
			   bit_width);
		atomic_set(&this_adm.copp.app_type[port_idx][copp_idx],
			   app_type);
		atomic_set(&this_adm.copp.acdb_id[port_idx][copp_idx],
			   acdb_id);
		set_bit(ADM_STATUS_CALIBRATION_REQUIRED,
		(void *)&this_adm.copp.adm_status[port_idx][copp_idx]);
		if ((path != ADM_PATH_COMPRESSED_RX) &&
		    (path != ADM_PATH_COMPRESSED_TX))
			send_adm_custom_topology();
	}

	if (this_adm.copp.adm_delay[port_idx][copp_idx] &&
		perf_mode == LEGACY_PCM_MODE) {
		atomic_set(&this_adm.copp.adm_delay_stat[port_idx][copp_idx],
			   1);
		this_adm.copp.adm_delay[port_idx][copp_idx] = 0;
		wake_up(&this_adm.copp.adm_delay_wait[port_idx][copp_idx]);
	}

	/* Create a COPP if port id are not enabled */
	if (atomic_read(&this_adm.copp.cnt[port_idx][copp_idx]) == 0) {
		pr_debug("%s: open ADM: port_idx: %d, copp_idx: %d\n", __func__,
			 port_idx, copp_idx);
		if ((topology == SRS_TRUMEDIA_TOPOLOGY_ID) &&
		      perf_mode == LEGACY_PCM_MODE) {
			int res;

			atomic_set(&this_adm.mem_map_index, ADM_SRS_TRUMEDIA);
			msm_dts_srs_tm_ion_memmap(&this_adm.outband_memmap);
			res = adm_memory_map_regions(
					&this_adm.outband_memmap.paddr, 0,
			(uint32_t *)&this_adm.outband_memmap.size, 1);
			if (res < 0) {
				pr_err("%s: SRS adm_memory_map_regions failed! addr = 0x%pK, size = %d\n",
					__func__,
					(void *)this_adm.outband_memmap.paddr,
					(uint32_t)this_adm.outband_memmap.size);
			}
		}


		if ((q6core_get_avcs_api_version_per_service(
				APRV2_IDS_SERVICE_ID_ADSP_ADM_V) >=
					ADSP_ADM_API_VERSION_V3) &&
					q6core_use_Q6_32ch_support()) {
			memset(&open_v8, 0, sizeof(open_v8));
			memset(&ep1_payload, 0, sizeof(ep1_payload));
			memset(&ep2_payload, 0, sizeof(ep2_payload));

			open_v8.hdr.hdr_field = APR_HDR_FIELD(
					APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE),
					APR_PKT_VER);
			open_v8.hdr.src_svc = APR_SVC_ADM;
			open_v8.hdr.src_domain = APR_DOMAIN_APPS;
			open_v8.hdr.src_port = tmp_port;
			open_v8.hdr.dest_svc = APR_SVC_ADM;
			open_v8.hdr.dest_domain = APR_DOMAIN_ADSP;
			open_v8.hdr.dest_port = tmp_port;
			open_v8.hdr.token = port_idx << 16 | copp_idx;
			open_v8.hdr.opcode = ADM_CMD_DEVICE_OPEN_V8;

			if (this_adm.native_mode != 0) {
				open_v8.flags = flags |
					(this_adm.native_mode << 11);
				this_adm.native_mode = 0;
			} else {
				open_v8.flags = flags;
			}
			open_v8.mode_of_operation = path;
			open_v8.endpoint_id_1 = tmp_port;
			open_v8.endpoint_id_2 = 0xFFFF;
			open_v8.endpoint_id_3 = 0xFFFF;
			open_v8.topology_id = topology;
			open_v8.reserved = 0;

			/* variable endpoint payload */
			ep1_payload.dev_num_channel = channel_mode & 0x00FF;
			ep1_payload.bit_width = bit_width;
			ep1_payload.sample_rate  = rate;
			ret = adm_arrange_mch_map_v8(&ep1_payload, path,
					channel_mode);
			if (ret)
				return ret;

			pr_debug("%s: port_id=0x%x %x %x topology_id=0x%X flags %x ref_ch %x\n",
				__func__, open_v8.endpoint_id_1,
				open_v8.endpoint_id_2,
				open_v8.endpoint_id_3,
				open_v8.topology_id,
				open_v8.flags,
				this_adm.num_ec_ref_rx_chans);

			ep1_payload_size = 8 +
				roundup(ep1_payload.dev_num_channel, 4);
			param_size = sizeof(struct adm_cmd_device_open_v8)
				+ ep1_payload_size;
			atomic_set(&this_adm.copp.stat[port_idx][copp_idx], -1);

			if ((this_adm.num_ec_ref_rx_chans != 0)
				&& (path != ADM_PATH_PLAYBACK)
				&& (open_v8.endpoint_id_2 != 0xFFFF)) {
				open_v8.endpoint_id_2 = this_adm.ec_ref_rx;
				this_adm.ec_ref_rx = -1;
				ep2_payload.dev_num_channel =
					this_adm.num_ec_ref_rx_chans;
				this_adm.num_ec_ref_rx_chans = 0;

				if (this_adm.ec_ref_rx_bit_width != 0) {
					ep2_payload.bit_width =
						this_adm.ec_ref_rx_bit_width;
				} else {
					ep2_payload.bit_width = bit_width;
				}

				if (this_adm.ec_ref_rx_sampling_rate != 0) {
					ep2_payload.sample_rate =
					this_adm.ec_ref_rx_sampling_rate;
				} else {
					ep2_payload.sample_rate = rate;
				}

				pr_debug("%s: adm open_v8 eid2_channels=%d eid2_bit_width=%d eid2_rate=%d\n",
					__func__,
					ep2_payload.dev_num_channel,
					ep2_payload.bit_width,
					ep2_payload.sample_rate);

				ret = adm_arrange_mch_ep2_map_v8(&ep2_payload,
					ep2_payload.dev_num_channel);

				if (ret)
					return ret;
				ep2_payload_size = 8 +
					roundup(ep2_payload.dev_num_channel, 4);
				param_size += ep2_payload_size;
			}

			open_v8.hdr.pkt_size = param_size;
			adm_params = kzalloc(param_size, GFP_KERNEL);
			if (!adm_params)
				return -ENOMEM;
			memcpy(adm_params, &open_v8, sizeof(open_v8));
			memcpy(adm_params + sizeof(open_v8),
					(void *)&ep1_payload,
					ep1_payload_size);

			if ((this_adm.num_ec_ref_rx_chans != 0)
				&& (path != ADM_PATH_PLAYBACK)
				&& (open_v8.endpoint_id_2 != 0xFFFF)) {
				memcpy(adm_params + sizeof(open_v8)
						+ ep1_payload_size,
						(void *)&ep2_payload,
						ep2_payload_size);
			}

			ret = apr_send_pkt(this_adm.apr,
					(uint32_t *)adm_params);
			if (ret < 0) {
				pr_err("%s: port_id: 0x%x for[0x%x] failed %d for open_v8\n",
					__func__, tmp_port, port_id, ret);
				return -EINVAL;
			}
			kfree(adm_params);
		} else {

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
			open.endpoint_id_2 = 0xFFFF;

			if (this_adm.ec_ref_rx && (path != 1)) {
				open.endpoint_id_2 = this_adm.ec_ref_rx;
			}

			open.topology_id = topology;

			open.dev_num_channel = channel_mode & 0x00FF;
			open.bit_width = bit_width;
			WARN_ON((perf_mode == ULTRA_LOW_LATENCY_PCM_MODE) &&
				(rate != ULL_SUPPORTED_SAMPLE_RATE));
			open.sample_rate  = rate;

			ret = adm_arrange_mch_map(&open, path, channel_mode);

			if (ret)
				return ret;

			pr_debug("%s: port_id=0x%x rate=%d topology_id=0x%X\n",
				__func__, open.endpoint_id_1, open.sample_rate,
				open.topology_id);

			atomic_set(&this_adm.copp.stat[port_idx][copp_idx], -1);

			if ((this_adm.num_ec_ref_rx_chans != 0) &&
				(path != 1) && (open.endpoint_id_2 != 0xFFFF)) {
				memset(&open_v6, 0,
					sizeof(struct adm_cmd_device_open_v6));
				memcpy(&open_v6, &open,
					sizeof(struct adm_cmd_device_open_v5));
				open_v6.hdr.opcode = ADM_CMD_DEVICE_OPEN_V6;
				open_v6.hdr.pkt_size = sizeof(open_v6);
				open_v6.dev_num_channel_eid2 =
					this_adm.num_ec_ref_rx_chans;

				if (this_adm.ec_ref_rx_bit_width != 0) {
					open_v6.bit_width_eid2 =
						this_adm.ec_ref_rx_bit_width;
				} else {
					open_v6.bit_width_eid2 = bit_width;
				}

				if (this_adm.ec_ref_rx_sampling_rate != 0) {
					open_v6.sample_rate_eid2 =
					       this_adm.ec_ref_rx_sampling_rate;
				} else {
					open_v6.sample_rate_eid2 = rate;
				}

				pr_debug("%s: eid2_channels=%d eid2_bit_width=%d eid2_rate=%d\n",
					__func__, open_v6.dev_num_channel_eid2,
					open_v6.bit_width_eid2,
					open_v6.sample_rate_eid2);

				ret = adm_arrange_mch_ep2_map(&open_v6,
					open_v6.dev_num_channel_eid2);

				if (ret)
					return ret;

				ret = apr_send_pkt(this_adm.apr,
					(uint32_t *)&open_v6);
			} else {
				ret = apr_send_pkt(this_adm.apr,
					(uint32_t *)&open);
			}
			if (ret < 0) {
				pr_err("%s: port_id: 0x%x for[0x%x] failed %d\n",
					__func__, tmp_port, port_id, ret);
				return -EINVAL;
			}
		}

		/* Wait for the callback with copp id */
		ret = wait_event_timeout(this_adm.copp.wait[port_idx][copp_idx],
			atomic_read(&this_adm.copp.stat
			[port_idx][copp_idx]) >= 0,
			msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: ADM open timedout for port_id: 0x%x for [0x%x]\n",
						__func__, tmp_port, port_id);
			return -EINVAL;
		} else if (atomic_read(&this_adm.copp.stat
					[port_idx][copp_idx]) > 0) {
			pr_err("%s: DSP returned error[%s]\n",
				__func__, adsp_err_get_err_str(
				atomic_read(&this_adm.copp.stat
				[port_idx][copp_idx])));
			return adsp_err_get_lnx_err_code(
					atomic_read(&this_adm.copp.stat
						[port_idx][copp_idx]));
		}
	}
	atomic_inc(&this_adm.copp.cnt[port_idx][copp_idx]);
	return copp_idx;
}
EXPORT_SYMBOL(adm_open);

/**
 * adm_copp_mfc_cfg -
 *        command to send ADM MFC config
 *
 * @port_id: Port ID number
 * @copp_idx: copp index assigned
 * @dst_sample_rate: sink sample rate
 *
 */
void adm_copp_mfc_cfg(int port_id, int copp_idx, int dst_sample_rate)
{
	struct audproc_mfc_param_media_fmt mfc_cfg;
	struct adm_cmd_device_open_v5 open;
	struct param_hdr_v3 param_hdr;
	int port_idx;
	int rc  = 0;
	int i  = 0;

	port_id = q6audio_convert_virtual_to_portid(port_id);
	port_idx = adm_validate_and_get_port_index(port_id);

	if (port_idx < 0) {
		pr_err("%s: Invalid port_id %#x\n", __func__, port_id);
		goto fail_cmd;
	}

	if (copp_idx < 0 || copp_idx >= MAX_COPPS_PER_PORT) {
		pr_err("%s: Invalid copp_num: %d\n", __func__, copp_idx);
		goto fail_cmd;
	}

	memset(&mfc_cfg, 0, sizeof(mfc_cfg));
	memset(&open, 0, sizeof(open));
	memset(&param_hdr, 0, sizeof(param_hdr));

	param_hdr.module_id = AUDPROC_MODULE_ID_MFC;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AUDPROC_PARAM_ID_MFC_OUTPUT_MEDIA_FORMAT;
	param_hdr.param_size = sizeof(mfc_cfg);

	mfc_cfg.sampling_rate = dst_sample_rate;
	mfc_cfg.bits_per_sample =
		atomic_read(&this_adm.copp.bit_width[port_idx][copp_idx]);
	open.dev_num_channel = mfc_cfg.num_channels =
		atomic_read(&this_adm.copp.channels[port_idx][copp_idx]);

	rc = adm_arrange_mch_map(&open, ADM_PATH_PLAYBACK,
		mfc_cfg.num_channels);
	if (rc < 0) {
		pr_err("%s: unable to get channal map\n", __func__);
		goto fail_cmd;
	}

	for (i = 0; i < mfc_cfg.num_channels; i++)
		mfc_cfg.channel_type[i] =
			(uint16_t) open.dev_channel_mapping[i];

	atomic_set(&this_adm.copp.stat[port_idx][copp_idx], -1);

	pr_debug("%s: mfc config: port_idx %d copp_idx  %d copp SR %d copp BW %d copp chan %d o/p SR %d\n",
			__func__, port_idx, copp_idx,
			atomic_read(&this_adm.copp.rate[port_idx][copp_idx]),
			mfc_cfg.bits_per_sample, mfc_cfg.num_channels,
			mfc_cfg.sampling_rate);

	rc = adm_pack_and_set_one_pp_param(port_id, copp_idx, param_hdr,
					   (uint8_t *) &mfc_cfg);
	if (rc)
		pr_err("%s: Failed to set media format configuration data, err %d\n",
		       __func__, rc);

fail_cmd:
	return;
}
EXPORT_SYMBOL(adm_copp_mfc_cfg);

static void route_set_opcode_matrix_id(
			struct adm_cmd_matrix_map_routings_v5 **route_addr,
			int path, uint32_t passthr_mode)
{
	struct adm_cmd_matrix_map_routings_v5 *route = *route_addr;

	switch (path) {
	case ADM_PATH_PLAYBACK:
		route->hdr.opcode = ADM_CMD_MATRIX_MAP_ROUTINGS_V5;
		route->matrix_id = ADM_MATRIX_ID_AUDIO_RX;
		break;
	case ADM_PATH_LIVE_REC:
		if (passthr_mode == LISTEN) {
			route->hdr.opcode =
				ADM_CMD_STREAM_DEVICE_MAP_ROUTINGS_V5;
			route->matrix_id = ADM_MATRIX_ID_LISTEN_TX;
			break;
		}
		/* fall through to set matrix id for non-listen case */
	case ADM_PATH_NONLIVE_REC:
		route->hdr.opcode = ADM_CMD_MATRIX_MAP_ROUTINGS_V5;
		route->matrix_id = ADM_MATRIX_ID_AUDIO_TX;
		break;
	case ADM_PATH_COMPRESSED_RX:
		route->hdr.opcode = ADM_CMD_STREAM_DEVICE_MAP_ROUTINGS_V5;
		route->matrix_id = ADM_MATRIX_ID_COMPRESSED_AUDIO_RX;
		break;
	case ADM_PATH_COMPRESSED_TX:
		route->hdr.opcode = ADM_CMD_STREAM_DEVICE_MAP_ROUTINGS_V5;
		route->matrix_id = ADM_MATRIX_ID_COMPRESSED_AUDIO_TX;
		break;
	default:
		pr_err("%s: Wrong path set[%d]\n", __func__, path);
		break;
	}
	pr_debug("%s: opcode 0x%x, matrix id %d\n",
		 __func__, route->hdr.opcode, route->matrix_id);
}

/**
 * adm_matrix_map -
 *        command to send ADM matrix map for ADM copp list
 *
 * @path: direction or ADM path type
 * @payload_map: have info of session id and associated copp_idx/num_copps
 * @perf_mode: performance mode like LL/ULL/..
 * @passthr_mode: flag to indicate passthrough mode
 *
 * Returns 0 on success or error on failure
 */
int adm_matrix_map(int path, struct route_payload payload_map, int perf_mode,
			uint32_t passthr_mode)
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
	route->num_sessions = 1;
	route_set_opcode_matrix_id(&route, path, passthr_mode);

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
			ret = -EINVAL;
			goto fail_cmd;
		}
		copp_idx = payload_map.copp_idx[i];
		copps_list[i] = atomic_read(&this_adm.copp.id[port_idx]
							     [copp_idx]);
	}
	atomic_set(&this_adm.matrix_map_stat, -1);

	ret = apr_send_pkt(this_adm.apr, (uint32_t *)matrix_map);
	if (ret < 0) {
		pr_err("%s: routing for syream %d failed ret %d\n",
			__func__, payload_map.session_id, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}
	ret = wait_event_timeout(this_adm.matrix_map_wait,
				atomic_read(&this_adm.matrix_map_stat) >= 0,
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: routing for syream %d failed\n", __func__,
			payload_map.session_id);
		ret = -EINVAL;
		goto fail_cmd;
	} else if (atomic_read(&this_adm.matrix_map_stat) > 0) {
		pr_err("%s: DSP returned error[%s]\n", __func__,
			adsp_err_get_err_str(atomic_read(
			&this_adm.matrix_map_stat)));
		ret = adsp_err_get_lnx_err_code(
				atomic_read(&this_adm.matrix_map_stat));
		goto fail_cmd;
	}

	if ((perf_mode != ULTRA_LOW_LATENCY_PCM_MODE) &&
		 (path != ADM_PATH_COMPRESSED_RX)) {
		for (i = 0; i < payload_map.num_copps; i++) {
			port_idx = afe_get_port_index(payload_map.port_id[i]);
			copp_idx = payload_map.copp_idx[i];
			if (port_idx < 0 || copp_idx < 0 ||
			    (copp_idx > MAX_COPPS_PER_PORT - 1)) {
				pr_err("%s: Invalid idx port_idx %d copp_idx %d\n",
					__func__, port_idx, copp_idx);
				continue;
			}
			rtac_add_adm_device(payload_map.port_id[i],
					    atomic_read(&this_adm.copp.id
							[port_idx][copp_idx]),
					    get_cal_path(path),
					    payload_map.session_id,
					    payload_map.app_type[i],
					    payload_map.acdb_dev_id[i]);

			if (!test_bit(ADM_STATUS_CALIBRATION_REQUIRED,
				(void *)&this_adm.copp.adm_status[port_idx]
								[copp_idx])) {
				pr_debug("%s: adm copp[0x%x][%d] already sent",
						__func__, port_idx, copp_idx);
				continue;
			}
			send_adm_cal(payload_map.port_id[i], copp_idx,
				     get_cal_path(path), perf_mode,
				     payload_map.app_type[i],
				     payload_map.acdb_dev_id[i],
				     payload_map.sample_rate[i],
				     passthr_mode);
			/* ADM COPP calibration is already sent */
			clear_bit(ADM_STATUS_CALIBRATION_REQUIRED,
				(void *)&this_adm.copp.
				adm_status[port_idx][copp_idx]);
			pr_debug("%s: copp_id: %d\n", __func__,
				 atomic_read(&this_adm.copp.id[port_idx]
							      [copp_idx]));
		}
	}

fail_cmd:
	kfree(matrix_map);
	return ret;
}
EXPORT_SYMBOL(adm_matrix_map);

/**
 * adm_ec_ref_rx_id -
 *        Update EC ref port ID
 *
 */
void adm_ec_ref_rx_id(int port_id)
{
	this_adm.ec_ref_rx = port_id;
	pr_debug("%s: ec_ref_rx:%d\n", __func__, this_adm.ec_ref_rx);
}
EXPORT_SYMBOL(adm_ec_ref_rx_id);

/**
 * adm_num_ec_ref_rx_chans -
 *        Update EC ref number of channels
 *
 */
void adm_num_ec_ref_rx_chans(int num_chans)
{
	this_adm.num_ec_ref_rx_chans = num_chans;
	pr_debug("%s: num_ec_ref_rx_chans:%d\n",
		__func__, this_adm.num_ec_ref_rx_chans);
}
EXPORT_SYMBOL(adm_num_ec_ref_rx_chans);

/**
 * adm_ec_ref_rx_bit_width -
 *        Update EC ref bit_width
 *
 */
void adm_ec_ref_rx_bit_width(int bit_width)
{
	this_adm.ec_ref_rx_bit_width = bit_width;
	pr_debug("%s: ec_ref_rx_bit_width:%d\n",
		__func__, this_adm.ec_ref_rx_bit_width);
}
EXPORT_SYMBOL(adm_ec_ref_rx_bit_width);

/**
 * adm_ec_ref_rx_sampling_rate -
 *        Update EC ref sample rate
 *
 */
void adm_ec_ref_rx_sampling_rate(int sampling_rate)
{
	this_adm.ec_ref_rx_sampling_rate = sampling_rate;
	pr_debug("%s: ec_ref_rx_sampling_rate:%d\n",
		__func__, this_adm.ec_ref_rx_sampling_rate);
}
EXPORT_SYMBOL(adm_ec_ref_rx_sampling_rate);

/**
 * adm_set_native_mode -
 *      Set adm channel native mode.
 *      If enabled matrix mixer will be
 *      running in native mode for channel
 *      configuration for this device session.
 *
 */
void adm_set_native_mode(int mode)
{
	this_adm.native_mode = mode;
	pr_debug("%s: enable native_mode :%d\n",
		__func__, this_adm.native_mode);
}
EXPORT_SYMBOL(adm_set_native_mode);

/**
 * adm_close -
 *        command to close ADM copp
 *
 * @port_id: Port ID number
 * @perf_mode: performance mode like LL/ULL/..
 * @copp_idx: copp index assigned
 *
 * Returns 0 on success or error on failure
 */
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

	if (this_adm.copp.adm_delay[port_idx][copp_idx] && perf_mode
		== LEGACY_PCM_MODE) {
		atomic_set(&this_adm.copp.adm_delay_stat[port_idx][copp_idx],
			   1);
		this_adm.copp.adm_delay[port_idx][copp_idx] = 0;
		wake_up(&this_adm.copp.adm_delay_wait[port_idx][copp_idx]);
	}

	atomic_dec(&this_adm.copp.cnt[port_idx][copp_idx]);
	if (!(atomic_read(&this_adm.copp.cnt[port_idx][copp_idx]))) {
		copp_id = adm_get_copp_id(port_idx, copp_idx);
		pr_debug("%s: Closing ADM port_idx:%d copp_idx:%d copp_id:0x%x\n",
			 __func__, port_idx, copp_idx, copp_id);
		if ((!perf_mode) && (this_adm.outband_memmap.paddr != 0) &&
		    (atomic_read(&this_adm.copp.topology[port_idx][copp_idx]) ==
			SRS_TRUMEDIA_TOPOLOGY_ID)) {
			atomic_set(&this_adm.mem_map_index,
				ADM_SRS_TRUMEDIA);
			ret = adm_memory_unmap_regions();
			if (ret < 0) {
				pr_err("%s: adm mem unmmap err %d",
					__func__, ret);
			} else {
				atomic_set(&this_adm.mem_map_handles
					   [ADM_SRS_TRUMEDIA], 0);
			}
		}


		if ((afe_get_port_type(port_id) == MSM_AFE_PORT_TYPE_TX) &&
		    this_adm.sourceTrackingData.memmap.paddr) {
			atomic_set(&this_adm.mem_map_index,
				   ADM_MEM_MAP_INDEX_SOURCE_TRACKING);
			ret = adm_memory_unmap_regions();
			if (ret < 0) {
				pr_err("%s: adm mem unmmap err %d",
					__func__, ret);
			}
			msm_audio_ion_free(
				this_adm.sourceTrackingData.dma_buf);
			this_adm.sourceTrackingData.dma_buf = NULL;
			this_adm.sourceTrackingData.memmap.size = 0;
			this_adm.sourceTrackingData.memmap.kvaddr = NULL;
			this_adm.sourceTrackingData.memmap.paddr = 0;
			this_adm.sourceTrackingData.apr_cmd_status = -1;
			atomic_set(&this_adm.mem_map_handles[
					ADM_MEM_MAP_INDEX_SOURCE_TRACKING], 0);
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
		atomic_set(&this_adm.copp.stat[port_idx][copp_idx], -1);
		atomic_set(&this_adm.copp.rate[port_idx][copp_idx], 0);
		atomic_set(&this_adm.copp.channels[port_idx][copp_idx], 0);
		atomic_set(&this_adm.copp.bit_width[port_idx][copp_idx], 0);
		atomic_set(&this_adm.copp.app_type[port_idx][copp_idx], 0);

		clear_bit(ADM_STATUS_CALIBRATION_REQUIRED,
			(void *)&this_adm.copp.adm_status[port_idx][copp_idx]);

		ret = apr_send_pkt(this_adm.apr, (uint32_t *)&close);
		if (ret < 0) {
			pr_err("%s: ADM close failed %d\n", __func__, ret);
			return -EINVAL;
		}

		ret = wait_event_timeout(this_adm.copp.wait[port_idx][copp_idx],
			atomic_read(&this_adm.copp.stat
			[port_idx][copp_idx]) >= 0,
			msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: ADM cmd Route timedout for port 0x%x\n",
				__func__, port_id);
			return -EINVAL;
		} else if (atomic_read(&this_adm.copp.stat
					[port_idx][copp_idx]) > 0) {
			pr_err("%s: DSP returned error[%s]\n",
				__func__, adsp_err_get_err_str(
				atomic_read(&this_adm.copp.stat
				[port_idx][copp_idx])));
			return adsp_err_get_lnx_err_code(
					atomic_read(&this_adm.copp.stat
						[port_idx][copp_idx]));
		}
	}

	if (perf_mode != ULTRA_LOW_LATENCY_PCM_MODE) {
		pr_debug("%s: remove adm device from rtac\n", __func__);
		rtac_remove_adm_device(port_id, copp_id);
	}
	return 0;
}
EXPORT_SYMBOL(adm_close);

int send_rtac_audvol_cal(void)
{
	int ret = 0;
	int ret2 = 0;
	int i = 0;
	int copp_idx, port_idx, acdb_id, app_id, path;
	struct cal_block_data *cal_block = NULL;
	struct audio_cal_info_audvol *audvol_cal_info = NULL;
	struct rtac_adm rtac_adm_data;

	mutex_lock(&this_adm.cal_data[ADM_RTAC_AUDVOL_CAL]->lock);

	cal_block = cal_utils_get_only_cal_block(
		this_adm.cal_data[ADM_RTAC_AUDVOL_CAL]);
	if (cal_block == NULL || cal_utils_is_cal_stale(cal_block)) {
		pr_err("%s: can't find cal block!\n", __func__);
		goto unlock;
	}

	audvol_cal_info = cal_block->cal_info;
	if (audvol_cal_info == NULL) {
		pr_err("%s: audvol_cal_info is NULL!\n", __func__);
		goto unlock;
	}

	get_rtac_adm_data(&rtac_adm_data);
	for (; i < rtac_adm_data.num_of_dev; i++) {

		acdb_id = rtac_adm_data.device[i].acdb_dev_id;
		if (acdb_id == 0)
			acdb_id = audvol_cal_info->acdb_id;

		app_id = rtac_adm_data.device[i].app_type;
		if (app_id == 0)
			app_id = audvol_cal_info->app_type;

		path = afe_get_port_type(rtac_adm_data.device[i].afe_port);
		if ((acdb_id == audvol_cal_info->acdb_id) &&
			(app_id == audvol_cal_info->app_type) &&
			(path == audvol_cal_info->path)) {

			if (adm_get_indexes_from_copp_id(rtac_adm_data.
				device[i].copp, &copp_idx, &port_idx) != 0) {
				pr_debug("%s: Copp Id %d is not active\n",
					__func__,
					rtac_adm_data.device[i].copp);
				continue;
			}

			ret2 = adm_remap_and_send_cal_block(ADM_RTAC_AUDVOL_CAL,
				rtac_adm_data.device[i].afe_port,
				copp_idx, cal_block,
				atomic_read(&this_adm.copp.
				mode[port_idx][copp_idx]),
				audvol_cal_info->app_type,
				audvol_cal_info->acdb_id,
				atomic_read(&this_adm.copp.
				rate[port_idx][copp_idx]));
			if (ret2 < 0) {
				pr_debug("%s: remap and send failed for copp Id %d, acdb id %d, app type %d, path %d\n",
					__func__, rtac_adm_data.device[i].copp,
					audvol_cal_info->acdb_id,
					audvol_cal_info->app_type,
					audvol_cal_info->path);
				ret = ret2;
			}
		}
	}
unlock:
	mutex_unlock(&this_adm.cal_data[ADM_RTAC_AUDVOL_CAL]->lock);
	return ret;
}

int adm_map_rtac_block(struct rtac_cal_block_data *cal_block)
{
	int result = 0;

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
	atomic_set(&this_adm.mem_map_index, ADM_RTAC_APR_CAL);
	result = adm_memory_map_regions(&cal_block->cal_data.paddr, 0,
					&cal_block->map_data.map_size, 1);
	if (result < 0) {
		pr_err("%s: RTAC mmap did not work! size = %d result %d\n",
			__func__,
			cal_block->map_data.map_size, result);
		pr_debug("%s: RTAC mmap did not work! addr = 0x%pK, size = %d\n",
			__func__,
			&cal_block->cal_data.paddr,
			cal_block->map_data.map_size);
		goto done;
	}

	cal_block->map_data.map_handle = atomic_read(
		&this_adm.mem_map_handles[ADM_RTAC_APR_CAL]);
done:
	return result;
}

int adm_unmap_rtac_block(uint32_t *mem_map_handle)
{
	int result = 0;

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
			&this_adm.mem_map_handles[ADM_RTAC_APR_CAL])) {
		pr_err("%s: Map handles do not match! Unmapping RTAC, RTAC map 0x%x, ADM map 0x%x\n",
			__func__, *mem_map_handle, atomic_read(
			&this_adm.mem_map_handles[ADM_RTAC_APR_CAL]));

		/* if mismatch use handle passed in to unmap */
		atomic_set(&this_adm.mem_map_handles[ADM_RTAC_APR_CAL],
			   *mem_map_handle);
	}

	/* valid port ID needed for callback use primary I2S */
	atomic_set(&this_adm.mem_map_index, ADM_RTAC_APR_CAL);
	result = adm_memory_unmap_regions();
	if (result < 0) {
		pr_debug("%s: adm_memory_unmap_regions failed, error %d\n",
			__func__, result);
	} else {
		atomic_set(&this_adm.mem_map_handles[ADM_RTAC_APR_CAL], 0);
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
	case ADM_LSM_AUDPROC_CAL_TYPE:
		ret = ADM_LSM_AUDPROC_CAL;
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
	case ADM_RTAC_AUDVOL_CAL_TYPE:
		ret = ADM_RTAC_AUDVOL_CAL;
		break;
	case ADM_LSM_AUDPROC_PERSISTENT_CAL_TYPE:
		ret = ADM_LSM_AUDPROC_PERSISTENT_CAL;
		break;
	default:
		pr_err("%s: invalid cal type %d!\n", __func__, cal_type);
	}
	return ret;
}

static int adm_alloc_cal(int32_t cal_type, size_t data_size, void *data)
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
	} else if (cal_index == ADM_RTAC_AUDVOL_CAL) {
		send_rtac_audvol_cal();
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

	atomic_set(&this_adm.mem_map_index, cal_index);
	ret = adm_memory_map_regions(&cal_block->cal_data.paddr, 0,
		(uint32_t *)&cal_block->map_data.map_size, 1);
	if (ret < 0) {
		pr_err("%s: map did not work! cal_type %i ret %d\n",
			__func__, cal_index, ret);
		ret = -ENODEV;
		goto done;
	}
	cal_block->map_data.q6map_handle = atomic_read(&this_adm.
		mem_map_handles[cal_index]);
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

	if (cal_block == NULL) {
		pr_err("%s: Cal block is NULL!\n",
						__func__);
		goto done;
	}

	if (cal_block->map_data.q6map_handle == 0) {
		pr_err("%s: Map handle is NULL, nothing to unmap\n",
				__func__);
		goto done;
	}

	atomic_set(&this_adm.mem_map_handles[cal_index],
		cal_block->map_data.q6map_handle);
	atomic_set(&this_adm.mem_map_index, cal_index);
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
}

static int adm_init_cal_data(void)
{
	int ret = 0;
	struct cal_type_info	cal_type_info[] = {
		{{ADM_CUST_TOPOLOGY_CAL_TYPE,
		{adm_alloc_cal, adm_dealloc_cal, NULL,
		adm_set_cal, NULL, NULL} },
		{adm_map_cal_data, adm_unmap_cal_data,
		cal_utils_match_buf_num} },

		{{ADM_AUDPROC_CAL_TYPE,
		{adm_alloc_cal, adm_dealloc_cal, NULL,
		adm_set_cal, NULL, NULL} },
		{adm_map_cal_data, adm_unmap_cal_data,
		cal_utils_match_buf_num} },

		{{ADM_LSM_AUDPROC_CAL_TYPE,
		{adm_alloc_cal, adm_dealloc_cal, NULL,
		adm_set_cal, NULL, NULL} },
		{adm_map_cal_data, adm_unmap_cal_data,
		cal_utils_match_buf_num} },

		{{ADM_AUDVOL_CAL_TYPE,
		{adm_alloc_cal, adm_dealloc_cal, NULL,
		adm_set_cal, NULL, NULL} },
		{adm_map_cal_data, adm_unmap_cal_data,
		cal_utils_match_buf_num} },

		{{ADM_RTAC_INFO_CAL_TYPE,
		{NULL, NULL, NULL, NULL, NULL, NULL} },
		{NULL, NULL, cal_utils_match_buf_num} },

		{{ADM_RTAC_APR_CAL_TYPE,
		{NULL, NULL, NULL, NULL, NULL, NULL} },
		{NULL, NULL, cal_utils_match_buf_num} },

		{{SRS_TRUMEDIA_CAL_TYPE,
		{NULL, NULL, NULL, NULL, NULL, NULL} },
		{NULL, NULL, cal_utils_match_buf_num} },

		{{ADM_RTAC_AUDVOL_CAL_TYPE,
		{adm_alloc_cal, adm_dealloc_cal, NULL,
		adm_set_cal, NULL, NULL} },
		{adm_map_cal_data, adm_unmap_cal_data,
		cal_utils_match_buf_num} },

		{{ADM_LSM_AUDPROC_PERSISTENT_CAL_TYPE,
		 {adm_alloc_cal, adm_dealloc_cal, NULL,
		  adm_set_cal, NULL, NULL} },
		 {adm_map_cal_data, adm_unmap_cal_data,
		  cal_utils_match_buf_num} },
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

/**
 * adm_set_volume -
 *        command to set volume on ADM copp
 *
 * @port_id: Port ID number
 * @copp_idx: copp index assigned
 * @volume: gain value to set
 *
 * Returns 0 on success or error on failure
 */
int adm_set_volume(int port_id, int copp_idx, int volume)
{
	struct audproc_volume_ctrl_master_gain audproc_vol;
	struct param_hdr_v3 param_hdr;
	int rc  = 0;

	pr_debug("%s: port_id %d, volume %d\n", __func__, port_id, volume);

	memset(&audproc_vol, 0, sizeof(audproc_vol));
	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = AUDPROC_MODULE_ID_VOL_CTRL;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AUDPROC_PARAM_ID_VOL_CTRL_MASTER_GAIN;
	param_hdr.param_size = sizeof(audproc_vol);

	audproc_vol.master_gain = volume;

	rc = adm_pack_and_set_one_pp_param(port_id, copp_idx, param_hdr,
					   (uint8_t *) &audproc_vol);
	if (rc)
		pr_err("%s: Failed to set volume, err %d\n", __func__, rc);

	return rc;
}
EXPORT_SYMBOL(adm_set_volume);

/**
 * adm_set_softvolume -
 *        command to set softvolume
 *
 * @port_id: Port ID number
 * @copp_idx: copp index assigned
 * @softvol_param: Params to set for softvolume
 *
 * Returns 0 on success or error on failure
 */
int adm_set_softvolume(int port_id, int copp_idx,
			struct audproc_softvolume_params *softvol_param)
{
	struct audproc_soft_step_volume_params audproc_softvol;
	struct param_hdr_v3 param_hdr;
	int rc  = 0;

	pr_debug("%s: period %d step %d curve %d\n", __func__,
		 softvol_param->period, softvol_param->step,
		 softvol_param->rampingcurve);

	memset(&audproc_softvol, 0, sizeof(audproc_softvol));
	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = AUDPROC_MODULE_ID_VOL_CTRL;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AUDPROC_PARAM_ID_SOFT_VOL_STEPPING_PARAMETERS;
	param_hdr.param_size = sizeof(audproc_softvol);

	audproc_softvol.period = softvol_param->period;
	audproc_softvol.step = softvol_param->step;
	audproc_softvol.ramping_curve = softvol_param->rampingcurve;

	pr_debug("%s: period %d, step %d, curve %d\n", __func__,
		 audproc_softvol.period, audproc_softvol.step,
		 audproc_softvol.ramping_curve);

	rc = adm_pack_and_set_one_pp_param(port_id, copp_idx, param_hdr,
					   (uint8_t *) &audproc_softvol);
	if (rc)
		pr_err("%s: Failed to set soft volume, err %d\n", __func__, rc);

	return rc;
}
EXPORT_SYMBOL(adm_set_softvolume);

/**
 * adm_set_mic_gain -
 *        command to set MIC gain
 *
 * @port_id: Port ID number
 * @copp_idx: copp index assigned
 * @volume: gain value to set
 *
 * Returns 0 on success or error on failure
 */
int adm_set_mic_gain(int port_id, int copp_idx, int volume)
{
	struct admx_mic_gain mic_gain_params;
	struct param_hdr_v3 param_hdr;
	int rc = 0;

	pr_debug("%s: Setting mic gain to %d at port_id 0x%x\n", __func__,
		 volume, port_id);

	memset(&mic_gain_params, 0, sizeof(mic_gain_params));
	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = ADM_MODULE_IDX_MIC_GAIN_CTRL;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = ADM_PARAM_IDX_MIC_GAIN;
	param_hdr.param_size = sizeof(mic_gain_params);

	mic_gain_params.tx_mic_gain = volume;

	rc = adm_pack_and_set_one_pp_param(port_id, copp_idx, param_hdr,
					   (uint8_t *) &mic_gain_params);
	if (rc)
		pr_err("%s: Failed to set mic gain, err %d\n", __func__, rc);

	return rc;
}
EXPORT_SYMBOL(adm_set_mic_gain);

/**
 * adm_send_set_multichannel_ec_primary_mic_ch -
 *        command to set multi-ch EC primary mic
 *
 * @port_id: Port ID number
 * @copp_idx: copp index assigned
 * @primary_mic_ch: channel number of primary mic
 *
 * Returns 0 on success or error on failure
 */
int adm_send_set_multichannel_ec_primary_mic_ch(int port_id, int copp_idx,
			int primary_mic_ch)
{
	struct admx_sec_primary_mic_ch sec_primary_ch_params;
	struct param_hdr_v3 param_hdr;
	int rc = 0;

	pr_debug("%s port_id 0x%x, copp_idx 0x%x, primary_mic_ch %d\n",
			__func__, port_id,  copp_idx,  primary_mic_ch);

	memset(&sec_primary_ch_params, 0, sizeof(sec_primary_ch_params));
	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = AUDPROC_MODULE_ID_VOICE_TX_SECNS;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AUDPROC_PARAM_IDX_SEC_PRIMARY_MIC_CH;
	param_hdr.param_size = sizeof(sec_primary_ch_params);

	sec_primary_ch_params.version = 0;
	sec_primary_ch_params.sec_primary_mic_ch = primary_mic_ch;

	rc = adm_pack_and_set_one_pp_param(port_id, copp_idx, param_hdr,
					   (uint8_t *) &sec_primary_ch_params);
	if (rc)
		pr_err("%s: Failed to set primary mic chanel, err %d\n",
		       __func__, rc);

	return rc;
}
EXPORT_SYMBOL(adm_send_set_multichannel_ec_primary_mic_ch);

/**
 * adm_param_enable -
 *      command to send params to ADM for given module
 *
 * @port_id: Port ID number
 * @copp_idx: copp index assigned
 * @module_id: ADM module
 * @enable: flag to enable or disable module
 *
 * Returns 0 on success or error on failure
 */
int adm_param_enable(int port_id, int copp_idx, int module_id,  int enable)
{
	struct module_instance_info mod_inst_info;

	memset(&mod_inst_info, 0, sizeof(mod_inst_info));
	mod_inst_info.module_id = module_id;
	mod_inst_info.instance_id = INSTANCE_ID_0;

	return adm_param_enable_v2(port_id, copp_idx, mod_inst_info, enable);
}
EXPORT_SYMBOL(adm_param_enable);

/**
 * adm_param_enable_v2 -
 *      command to send params to ADM for given module
 *
 * @port_id: Port ID number
 * @copp_idx: copp index assigned
 * @mod_inst_info: module and instance ID info
 * @enable: flag to enable or disable module
 *
 * Returns 0 on success or error on failure
 */
int adm_param_enable_v2(int port_id, int copp_idx,
			struct module_instance_info mod_inst_info, int enable)
{
	uint32_t enable_param;
	struct param_hdr_v3 param_hdr;
	int rc = 0;

	if (enable < 0 || enable > 1) {
		pr_err("%s: Invalid value for enable %d\n", __func__, enable);
		return -EINVAL;
	}

	pr_debug("%s port_id %d, module_id 0x%x, instance_id 0x%x, enable %d\n",
		 __func__, port_id, mod_inst_info.module_id,
		 mod_inst_info.instance_id, enable);

	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = mod_inst_info.module_id;
	param_hdr.instance_id = mod_inst_info.instance_id;
	param_hdr.param_id = AUDPROC_PARAM_ID_ENABLE;
	param_hdr.param_size = sizeof(enable_param);

	enable_param = enable;

	rc = adm_pack_and_set_one_pp_param(port_id, copp_idx, param_hdr,
					   (uint8_t *) &enable_param);
	if (rc)
		pr_err("%s: Failed to set enable of module(%d) instance(%d) to %d, err %d\n",
		       __func__, mod_inst_info.module_id,
		       mod_inst_info.instance_id, enable, rc);

	return rc;

}
EXPORT_SYMBOL(adm_param_enable_v2);

/**
 * adm_send_calibration -
 *        send ADM calibration to DSP
 *
 * @port_id: Port ID number
 * @copp_idx: copp index assigned
 * @path: direction or ADM path type
 * @perf_mode: performance mode like LL/ULL/..
 * @cal_type: calibration type to use
 * @params: pointer with cal data
 * @size: cal size
 *
 * Returns 0 on success or error on failure
 */
int adm_send_calibration(int port_id, int copp_idx, int path, int perf_mode,
			 int cal_type, char *params, int size)
{

	int rc = 0;

	pr_debug("%s:port_id %d, path %d, perf_mode %d, cal_type %d, size %d\n",
		 __func__, port_id, path, perf_mode, cal_type, size);

	/* Maps audio_dev_ctrl path definition to ACDB definition */
	if (get_cal_path(path) != RX_DEVICE) {
		pr_err("%s: acdb_path %d\n", __func__, path);
		rc = -EINVAL;
		goto end;
	}

	rc = adm_set_pp_params(port_id, copp_idx, NULL, (u8 *) params, size);

end:
	return rc;
}
EXPORT_SYMBOL(adm_send_calibration);

/*
 * adm_update_wait_parameters must be called with routing driver locks.
 * adm_reset_wait_parameters must be called with routing driver locks.
 * set and reset parmeters are separated to make sure it is always called
 * under routing driver lock.
 * adm_wait_timeout is to block until timeout or interrupted. Timeout is
 * not a an error.
 */
int adm_set_wait_parameters(int port_id, int copp_idx)
{

	int ret = 0, port_idx;

	pr_debug("%s: port_id 0x%x, copp_idx %d\n", __func__, port_id,
		 copp_idx);
	port_id = afe_convert_virtual_to_portid(port_id);
	port_idx = adm_validate_and_get_port_index(port_id);
	if (port_idx < 0) {
		pr_err("%s: Invalid port_id %#x\n", __func__, port_id);
		ret = -EINVAL;
		goto end;
	}

	if (copp_idx < 0 || copp_idx >= MAX_COPPS_PER_PORT) {
		pr_err("%s: Invalid copp_num: %d\n", __func__, copp_idx);
		return -EINVAL;
	}

	this_adm.copp.adm_delay[port_idx][copp_idx] = 1;
	atomic_set(&this_adm.copp.adm_delay_stat[port_idx][copp_idx], 0);

end:
	return ret;

}
EXPORT_SYMBOL(adm_set_wait_parameters);

/**
 * adm_reset_wait_parameters -
 *        reset wait parameters or ADM delay value
 *
 * @port_id: Port ID number
 * @copp_idx: copp index assigned
 *
 * Returns 0 on success or error on failure
 */
int adm_reset_wait_parameters(int port_id, int copp_idx)
{
	int ret = 0, port_idx;

	pr_debug("%s: port_id 0x%x copp_idx %d\n", __func__, port_id,
		 copp_idx);
	port_id = afe_convert_virtual_to_portid(port_id);
	port_idx = adm_validate_and_get_port_index(port_id);
	if (port_idx < 0) {
		pr_err("%s: Invalid port_id %#x\n", __func__, port_id);
		ret = -EINVAL;
		goto end;
	}

	if (copp_idx < 0 || copp_idx >= MAX_COPPS_PER_PORT) {
		pr_err("%s: Invalid copp_num: %d\n", __func__, copp_idx);
		return -EINVAL;
	}

	atomic_set(&this_adm.copp.adm_delay_stat[port_idx][copp_idx], 1);
	this_adm.copp.adm_delay[port_idx][copp_idx] = 0;

end:
	return ret;
}
EXPORT_SYMBOL(adm_reset_wait_parameters);

/**
 * adm_wait_timeout -
 *        ADM wait command after command send to DSP
 *
 * @port_id: Port ID number
 * @copp_idx: copp index assigned
 * @wait_time: value in ms for command timeout
 *
 * Returns 0 on success or error on failure
 */
int adm_wait_timeout(int port_id, int copp_idx, int wait_time)
{
	int ret = 0, port_idx;

	pr_debug("%s: port_id 0x%x, copp_idx %d, wait_time %d\n", __func__,
		 port_id, copp_idx, wait_time);
	port_id = afe_convert_virtual_to_portid(port_id);
	port_idx = adm_validate_and_get_port_index(port_id);
	if (port_idx < 0) {
		pr_err("%s: Invalid port_id %#x\n", __func__, port_id);
		ret = -EINVAL;
		goto end;
	}

	if (copp_idx < 0 || copp_idx >= MAX_COPPS_PER_PORT) {
		pr_err("%s: Invalid copp_num: %d\n", __func__, copp_idx);
		return -EINVAL;
	}

	ret = wait_event_timeout(
		this_adm.copp.adm_delay_wait[port_idx][copp_idx],
		atomic_read(&this_adm.copp.adm_delay_stat[port_idx][copp_idx]),
		msecs_to_jiffies(wait_time));
	pr_debug("%s: return %d\n", __func__, ret);
	if (ret != 0)
		ret = -EINTR;
end:
	pr_debug("%s: return %d--\n", __func__, ret);
	return ret;
}
EXPORT_SYMBOL(adm_wait_timeout);

/**
 * adm_store_cal_data -
 *        Retrieve calibration data for ADM copp device
 *
 * @port_id: Port ID number
 * @copp_idx: copp index assigned
 * @path: direction or copp type
 * @perf_mode: performance mode like LL/ULL/..
 * @cal_index: calibration index to use
 * @params: pointer to store cal data
 * @size: pointer to fill with cal size
 *
 * Returns 0 on success or error on failure
 */
int adm_store_cal_data(int port_id, int copp_idx, int path, int perf_mode,
		       int cal_index, char *params, int *size)
{
	int rc = 0;
	struct cal_block_data		*cal_block = NULL;
	int app_type, acdb_id, port_idx, sample_rate;

	if (this_adm.cal_data[cal_index] == NULL) {
		pr_debug("%s: cal_index %d not allocated!\n",
			__func__, cal_index);
		goto end;
	}

	if (get_cal_path(path) != RX_DEVICE) {
		pr_debug("%s: Invalid path to store calibration %d\n",
			 __func__, path);
		rc = -EINVAL;
		goto end;
	}

	port_id = afe_convert_virtual_to_portid(port_id);
	port_idx = adm_validate_and_get_port_index(port_id);
	if (port_idx < 0) {
		pr_err("%s: Invalid port_id 0x%x\n", __func__, port_id);
		rc = -EINVAL;
		goto end;
	}

	if (copp_idx < 0 || copp_idx >= MAX_COPPS_PER_PORT) {
		pr_err("%s: Invalid copp_num: %d\n", __func__, copp_idx);
		return -EINVAL;
	}

	acdb_id = atomic_read(&this_adm.copp.acdb_id[port_idx][copp_idx]);
	app_type = atomic_read(&this_adm.copp.app_type[port_idx][copp_idx]);
	sample_rate = atomic_read(&this_adm.copp.rate[port_idx][copp_idx]);

	mutex_lock(&this_adm.cal_data[cal_index]->lock);
	cal_block = adm_find_cal(cal_index, get_cal_path(path), app_type,
				acdb_id, sample_rate);
	if (cal_block == NULL)
		goto unlock;

	if (cal_block->cal_data.size <= 0) {
		pr_debug("%s: No ADM cal send for port_id = 0x%x!\n",
			__func__, port_id);
		rc = -EINVAL;
		goto unlock;
	}

	if (cal_index == ADM_AUDPROC_CAL || cal_index == ADM_LSM_AUDPROC_CAL) {
		if (cal_block->cal_data.size > AUD_PROC_BLOCK_SIZE) {
			pr_err("%s:audproc:invalid size exp/actual[%zd, %d]\n",
				__func__, cal_block->cal_data.size, *size);
			rc = -ENOMEM;
			goto unlock;
		}
	} else if (cal_index == ADM_LSM_AUDPROC_PERSISTENT_CAL) {
		if (cal_block->cal_data.size > AUD_PROC_PERSIST_BLOCK_SIZE) {
			pr_err("%s:persist invalid size exp/actual[%zd, %d]\n",
				__func__, cal_block->cal_data.size, *size);
			rc = -ENOMEM;
			goto unlock;
		}
	} else if (cal_index == ADM_AUDVOL_CAL) {
		if (cal_block->cal_data.size > AUD_VOL_BLOCK_SIZE) {
			pr_err("%s:aud_vol:invalid size exp/actual[%zd, %d]\n",
				__func__, cal_block->cal_data.size, *size);
			rc = -ENOMEM;
			goto unlock;
		}
	} else {
		pr_debug("%s: Not valid calibration for dolby topolgy\n",
			 __func__);
		rc = -EINVAL;
		goto unlock;
	}
	memcpy(params, cal_block->cal_data.kvaddr, cal_block->cal_data.size);
	*size = cal_block->cal_data.size;

	pr_debug("%s:port_id %d, copp_idx %d, path %d",
		 __func__, port_id, copp_idx, path);
	pr_debug("perf_mode %d, cal_type %d, size %d\n",
		 perf_mode, cal_index, *size);

unlock:
	mutex_unlock(&this_adm.cal_data[cal_index]->lock);
end:
	return rc;
}
EXPORT_SYMBOL(adm_store_cal_data);

/**
 * adm_send_compressed_device_mute -
 *        command to send mute for compressed device
 *
 * @port_id: Port ID number
 * @copp_idx: copp index assigned
 * @mute_on: flag to indicate mute or unmute
 *
 * Returns 0 on success or error on failure
 */
int adm_send_compressed_device_mute(int port_id, int copp_idx, bool mute_on)
{
	u32 mute_param = mute_on ? 1 : 0;
	struct param_hdr_v3 param_hdr;
	int ret = 0;

	pr_debug("%s port_id: 0x%x, copp_idx %d, mute_on: %d\n",
		 __func__, port_id, copp_idx, mute_on);

	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = AUDPROC_MODULE_ID_COMPRESSED_MUTE;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AUDPROC_PARAM_ID_COMPRESSED_MUTE;
	param_hdr.param_size = sizeof(mute_param);

	ret = adm_pack_and_set_one_pp_param(port_id, copp_idx, param_hdr,
					    (uint8_t *) &mute_param);
	if (ret)
		pr_err("%s: Failed to set mute, err %d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(adm_send_compressed_device_mute);

/**
 * adm_send_compressed_device_latency -
 *        command to send latency for compressed device
 *
 * @port_id: Port ID number
 * @copp_idx: copp index assigned
 * @latency: latency value to pass
 *
 * Returns 0 on success or error on failure
 */
int adm_send_compressed_device_latency(int port_id, int copp_idx, int latency)
{
	u32 latency_param;
	struct param_hdr_v3 param_hdr;
	int ret = 0;

	pr_debug("%s port_id: 0x%x, copp_idx %d latency: %d\n", __func__,
		 port_id, copp_idx, latency);

	if (latency < 0) {
		pr_err("%s: Invalid value for latency %d", __func__, latency);
		return -EINVAL;
	}

	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = AUDPROC_MODULE_ID_COMPRESSED_LATENCY;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AUDPROC_PARAM_ID_COMPRESSED_LATENCY;
	param_hdr.param_size = sizeof(latency_param);

	latency_param = latency;

	ret = adm_pack_and_set_one_pp_param(port_id, copp_idx, param_hdr,
					    (uint8_t *) &latency_param);
	if (ret)
		pr_err("%s: Failed to set latency, err %d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(adm_send_compressed_device_latency);

/**
 * adm_swap_speaker_channels
 *
 * Receives port_id, copp_idx, sample rate, spk_swap and
 * send MFC command to swap speaker channel.
 * Return zero on success. On failure returns nonzero.
 *
 * port_id - Passed value, port_id for which channels swap is wanted
 * copp_idx - Passed value, copp_idx for which channels swap is wanted
 * sample_rate - Passed value, sample rate used by app type config
 * spk_swap  - Passed value, spk_swap for check if swap flag is set
 */
int adm_swap_speaker_channels(int port_id, int copp_idx,
			int sample_rate, bool spk_swap)
{
	struct audproc_mfc_param_media_fmt mfc_cfg;
	struct param_hdr_v3 param_hdr;
	uint16_t num_channels;
	int port_idx = 0;
	int ret  = 0;

	pr_debug("%s: Enter, port_id %d, copp_idx %d\n",
		  __func__, port_id, copp_idx);
	port_id = q6audio_convert_virtual_to_portid(port_id);
	port_idx = adm_validate_and_get_port_index(port_id);
	if (port_idx < 0 || port_idx >= AFE_MAX_PORTS) {
		pr_err("%s: Invalid port_id %#x\n", __func__, port_id);
		return -EINVAL;
	} else if (copp_idx < 0 || copp_idx >= MAX_COPPS_PER_PORT) {
		pr_err("%s: Invalid copp_idx 0x%x\n", __func__, copp_idx);
		return -EINVAL;
	}

	num_channels = atomic_read(&this_adm.copp.channels[port_idx][copp_idx]);
	if (num_channels != 2) {
		pr_debug("%s: Invalid number of channels: %d\n",
			__func__, num_channels);
		return -EINVAL;
	}

	memset(&mfc_cfg, 0, sizeof(mfc_cfg));
	memset(&param_hdr, 0, sizeof(param_hdr));

	param_hdr.module_id = AUDPROC_MODULE_ID_MFC;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AUDPROC_PARAM_ID_MFC_OUTPUT_MEDIA_FORMAT;
	param_hdr.param_size = sizeof(mfc_cfg);

	mfc_cfg.sampling_rate = sample_rate;
	mfc_cfg.bits_per_sample =
		atomic_read(&this_adm.copp.bit_width[port_idx][copp_idx]);
	mfc_cfg.num_channels = num_channels;

	/* Currently applying speaker swap for only 2 channel use case */
	if (spk_swap) {
		mfc_cfg.channel_type[0] =
			(uint16_t) PCM_CHANNEL_FR;
		mfc_cfg.channel_type[1] =
			(uint16_t) PCM_CHANNEL_FL;
	} else {
		mfc_cfg.channel_type[0] =
			(uint16_t) PCM_CHANNEL_FL;
		mfc_cfg.channel_type[1] =
			(uint16_t) PCM_CHANNEL_FR;
	}

	ret = adm_pack_and_set_one_pp_param(port_id, copp_idx, param_hdr,
					    (u8 *) &mfc_cfg);
	if (ret < 0) {
		pr_err("%s: Failed to set swap speaker channels on port[0x%x] failed %d\n",
		       __func__, port_id, ret);
		return ret;
	}

	pr_debug("%s: mfc_cfg Set params returned success", __func__);
	return 0;
}
EXPORT_SYMBOL(adm_swap_speaker_channels);

/**
 * adm_set_sound_focus -
 *       Update sound focus info
 *
 * @port_id: Port ID number
 * @copp_idx: copp index assigned
 * @soundFocusData: sound focus data to pass
 *
 * Returns 0 on success or error on failure
 */
int adm_set_sound_focus(int port_id, int copp_idx,
			struct sound_focus_param soundFocusData)
{
	struct adm_param_fluence_soundfocus_t soundfocus_params;
	struct param_hdr_v3 param_hdr;
	int ret  = 0;
	int i;

	pr_debug("%s: Enter, port_id %d, copp_idx %d\n",
		  __func__, port_id, copp_idx);

	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = VOICEPROC_MODULE_ID_FLUENCE_PRO_VC_TX;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = VOICEPROC_PARAM_ID_FLUENCE_SOUNDFOCUS;
	param_hdr.param_size = sizeof(soundfocus_params);

	memset(&(soundfocus_params), 0xFF, sizeof(soundfocus_params));
	for (i = 0; i < MAX_SECTORS; i++) {
		soundfocus_params.start_angles[i] =
			soundFocusData.start_angle[i];
		soundfocus_params.enables[i] = soundFocusData.enable[i];
		pr_debug("%s: start_angle[%d] = %d\n",
			  __func__, i, soundFocusData.start_angle[i]);
		pr_debug("%s: enable[%d] = %d\n",
			  __func__, i, soundFocusData.enable[i]);
	}
	soundfocus_params.gain_step = soundFocusData.gain_step;
	pr_debug("%s: gain_step = %d\n", __func__, soundFocusData.gain_step);

	soundfocus_params.reserved = 0;

	ret = adm_pack_and_set_one_pp_param(port_id, copp_idx, param_hdr,
					    (uint8_t *) &soundfocus_params);
	if (ret)
		pr_err("%s: Failed to set sound focus params, err %d\n",
		       __func__, ret);

	pr_debug("%s: Exit, ret=%d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(adm_set_sound_focus);

/**
 * adm_get_sound_focus -
 *        Retrieve sound focus info
 *
 * @port_id: Port ID number
 * @copp_idx: copp index assigned
 * @soundFocusData: pointer for sound focus data to be updated with
 *
 * Returns 0 on success or error on failure
 */
int adm_get_sound_focus(int port_id, int copp_idx,
			struct sound_focus_param *soundFocusData)
{
	int ret = 0, i;
	char *params_value;
	uint32_t max_param_size = 0;
	struct adm_param_fluence_soundfocus_t *soundfocus_params = NULL;
	struct param_hdr_v3 param_hdr;

	pr_debug("%s: Enter, port_id %d, copp_idx %d\n",
		  __func__, port_id, copp_idx);

	max_param_size = sizeof(struct adm_param_fluence_soundfocus_t) +
			 sizeof(union param_hdrs);
	params_value = kzalloc(max_param_size, GFP_KERNEL);
	if (!params_value)
		return -ENOMEM;

	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = VOICEPROC_MODULE_ID_FLUENCE_PRO_VC_TX;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = VOICEPROC_PARAM_ID_FLUENCE_SOUNDFOCUS;
	param_hdr.param_size = max_param_size;
	ret = adm_get_pp_params(port_id, copp_idx,
				ADM_CLIENT_ID_SOURCE_TRACKING, NULL, &param_hdr,
				params_value);
	if (ret) {
		pr_err("%s: get parameters failed ret:%d\n", __func__, ret);
		ret = -EINVAL;
		goto done;
	}

	if (this_adm.sourceTrackingData.apr_cmd_status != 0) {
		pr_err("%s - get params returned error [%s]\n",
			__func__, adsp_err_get_err_str(
			this_adm.sourceTrackingData.apr_cmd_status));
		ret = adsp_err_get_lnx_err_code(
				this_adm.sourceTrackingData.apr_cmd_status);
		goto done;
	}

	soundfocus_params = (struct adm_param_fluence_soundfocus_t *)
								params_value;
	for (i = 0; i < MAX_SECTORS; i++) {
		soundFocusData->start_angle[i] =
					soundfocus_params->start_angles[i];
		soundFocusData->enable[i] = soundfocus_params->enables[i];
		pr_debug("%s: start_angle[%d] = %d\n",
			  __func__, i, soundFocusData->start_angle[i]);
		pr_debug("%s: enable[%d] = %d\n",
			  __func__, i, soundFocusData->enable[i]);
	}
	soundFocusData->gain_step = soundfocus_params->gain_step;
	pr_debug("%s: gain_step = %d\n", __func__, soundFocusData->gain_step);

done:
	pr_debug("%s: Exit, ret = %d\n", __func__, ret);

	kfree(params_value);
	return ret;
}
EXPORT_SYMBOL(adm_get_sound_focus);

static int adm_source_tracking_alloc_map_memory(void)
{
	int ret;

	pr_debug("%s: Enter\n", __func__);

	ret = msm_audio_ion_alloc(&this_adm.sourceTrackingData.dma_buf,
				  AUD_PROC_BLOCK_SIZE,
				  &this_adm.sourceTrackingData.memmap.paddr,
				  &this_adm.sourceTrackingData.memmap.size,
				  &this_adm.sourceTrackingData.memmap.kvaddr);
	if (ret) {
		pr_err("%s: failed to allocate memory\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	atomic_set(&this_adm.mem_map_index, ADM_MEM_MAP_INDEX_SOURCE_TRACKING);
	ret = adm_memory_map_regions(&this_adm.sourceTrackingData.memmap.paddr,
			0,
			(uint32_t *)&this_adm.sourceTrackingData.memmap.size,
			1);
	if (ret < 0) {
		pr_err("%s: failed to map memory, paddr = 0x%pK, size = %d\n",
			__func__,
			(void *)this_adm.sourceTrackingData.memmap.paddr,
			(uint32_t)this_adm.sourceTrackingData.memmap.size);

		msm_audio_ion_free(this_adm.sourceTrackingData.dma_buf);
		this_adm.sourceTrackingData.dma_buf = NULL;
		this_adm.sourceTrackingData.memmap.size = 0;
		this_adm.sourceTrackingData.memmap.kvaddr = NULL;
		this_adm.sourceTrackingData.memmap.paddr = 0;
		this_adm.sourceTrackingData.apr_cmd_status = -1;
		atomic_set(&this_adm.mem_map_handles
				[ADM_MEM_MAP_INDEX_SOURCE_TRACKING], 0);

		ret = -EINVAL;
		goto done;
	}
	ret = 0;
	pr_debug("%s: paddr = 0x%pK, size = %d, mem_map_handle = 0x%x\n",
		  __func__, (void *)this_adm.sourceTrackingData.memmap.paddr,
		  (uint32_t)this_adm.sourceTrackingData.memmap.size,
		  atomic_read(&this_adm.mem_map_handles
			      [ADM_MEM_MAP_INDEX_SOURCE_TRACKING]));

done:
	pr_debug("%s: Exit, ret = %d\n", __func__, ret);

	return ret;
}

/**
 * adm_get_source_tracking -
 *        Retrieve source tracking info
 *
 * @port_id: Port ID number
 * @copp_idx: copp index assigned
 * @sourceTrackingData: pointer for source track data to be updated with
 *
 * Returns 0 on success or error on failure
 */
int adm_get_source_tracking(int port_id, int copp_idx,
			    struct source_tracking_param *sourceTrackingData)
{
	struct adm_param_fluence_sourcetracking_t *source_tracking_params =
		NULL;
	struct mem_mapping_hdr mem_hdr;
	struct param_hdr_v3 param_hdr;
	int i = 0;
	int ret = 0;

	pr_debug("%s: Enter, port_id %d, copp_idx %d\n",
		  __func__, port_id, copp_idx);

	if (!this_adm.sourceTrackingData.memmap.paddr) {
		/* Allocate and map shared memory for out of band usage */
		ret = adm_source_tracking_alloc_map_memory();
		if (ret != 0) {
			ret = -EINVAL;
			goto done;
		}
	}

	memset(&mem_hdr, 0, sizeof(mem_hdr));
	memset(&param_hdr, 0, sizeof(param_hdr));
	mem_hdr.data_payload_addr_lsw =
		lower_32_bits(this_adm.sourceTrackingData.memmap.paddr);
	mem_hdr.data_payload_addr_msw = msm_audio_populate_upper_32_bits(
		this_adm.sourceTrackingData.memmap.paddr);
	mem_hdr.mem_map_handle = atomic_read(
		&this_adm.mem_map_handles[ADM_MEM_MAP_INDEX_SOURCE_TRACKING]);

	param_hdr.module_id = VOICEPROC_MODULE_ID_FLUENCE_PRO_VC_TX;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = VOICEPROC_PARAM_ID_FLUENCE_SOURCETRACKING;
	/*
	 * This size should be the max size of the calibration data + header.
	 * Use the union size to ensure max size is used.
	 */
	param_hdr.param_size =
		sizeof(struct adm_param_fluence_sourcetracking_t) +
		sizeof(union param_hdrs);

	/*
	 * Retrieving parameters out of band, so no need to provide a buffer for
	 * the returned parameter data as it will be at the memory location
	 * provided.
	 */
	ret = adm_get_pp_params(port_id, copp_idx,
				ADM_CLIENT_ID_SOURCE_TRACKING, &mem_hdr,
				&param_hdr, NULL);
	if (ret) {
		pr_err("%s: Failed to get params, error %d\n", __func__, ret);
		goto done;
	}

	if (this_adm.sourceTrackingData.apr_cmd_status != 0) {
		pr_err("%s - get params returned error [%s]\n",
			__func__, adsp_err_get_err_str(
			this_adm.sourceTrackingData.apr_cmd_status));

		ret = adsp_err_get_lnx_err_code(
				this_adm.sourceTrackingData.apr_cmd_status);
		goto done;
	}

	/* How do we know what the param data was retrieved with for hdr size */
	source_tracking_params =
		(struct adm_param_fluence_sourcetracking_t
			 *) (this_adm.sourceTrackingData.memmap.kvaddr +
			     sizeof(struct param_hdr_v1));
	for (i = 0; i < MAX_SECTORS; i++) {
		sourceTrackingData->vad[i] = source_tracking_params->vad[i];
		pr_debug("%s: vad[%d] = %d\n",
			  __func__, i, sourceTrackingData->vad[i]);
	}
	sourceTrackingData->doa_speech = source_tracking_params->doa_speech;
	pr_debug("%s: doa_speech = %d\n",
		  __func__, sourceTrackingData->doa_speech);

	for (i = 0; i < MAX_NOISE_SOURCE_INDICATORS; i++) {
		sourceTrackingData->doa_noise[i] =
					source_tracking_params->doa_noise[i];
		pr_debug("%s: doa_noise[%d] = %d\n",
			  __func__, i, sourceTrackingData->doa_noise[i]);
	}
	for (i = 0; i < MAX_POLAR_ACTIVITY_INDICATORS; i++) {
		sourceTrackingData->polar_activity[i] =
				source_tracking_params->polar_activity[i];
		pr_debug("%s: polar_activity[%d] = %d\n",
			  __func__, i, sourceTrackingData->polar_activity[i]);
	}

	ret = 0;

done:
	pr_debug("%s: Exit, ret=%d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(adm_get_source_tracking);

int __init adm_init(void)
{
	int i = 0, j;

	this_adm.ec_ref_rx = -1;
	init_waitqueue_head(&this_adm.matrix_map_wait);
	init_waitqueue_head(&this_adm.adm_wait);

	for (i = 0; i < AFE_MAX_PORTS; i++) {
		for (j = 0; j < MAX_COPPS_PER_PORT; j++) {
			atomic_set(&this_adm.copp.id[i][j], RESET_COPP_ID);
			init_waitqueue_head(&this_adm.copp.wait[i][j]);
			init_waitqueue_head(
				&this_adm.copp.adm_delay_wait[i][j]);
		}
	}

	if (adm_init_cal_data())
		pr_err("%s: could not init cal data!\n", __func__);

	this_adm.sourceTrackingData.dma_buf = NULL;
	this_adm.sourceTrackingData.memmap.size = 0;
	this_adm.sourceTrackingData.memmap.kvaddr = NULL;
	this_adm.sourceTrackingData.memmap.paddr = 0;
	this_adm.sourceTrackingData.apr_cmd_status = -1;

	return 0;
}

void adm_exit(void)
{
	if (this_adm.apr)
		adm_reset_data();
	adm_delete_cal_data();
}
