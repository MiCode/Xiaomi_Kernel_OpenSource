/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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
#ifndef __Q6_ADM_V2_H__
#define __Q6_ADM_V2_H__


#define ADM_PATH_PLAYBACK 0x1
#define ADM_PATH_LIVE_REC 0x2
#define ADM_PATH_NONLIVE_REC 0x3
#define ADM_PATH_COMPRESSED_RX 0x5
#include <linux/qdsp6v2/rtac.h>
#include <sound/q6afe-v2.h>
#include <sound/q6audio-v2.h>

#define MAX_MODULES_IN_TOPO 16
#define ADM_GET_TOPO_MODULE_LIST_LENGTH\
		((MAX_MODULES_IN_TOPO + 1) * sizeof(uint32_t))
#define AUD_PROC_BLOCK_SIZE	4096
#define AUD_VOL_BLOCK_SIZE	4096
#define AUDIO_RX_CALIBRATION_SIZE	(AUD_PROC_BLOCK_SIZE + \
						AUD_VOL_BLOCK_SIZE)
enum {
	ADM_CUSTOM_TOP_CAL = 0,
	ADM_AUDPROC_CAL,
	ADM_AUDVOL_CAL,
	ADM_RTAC_INFO_CAL,
	ADM_RTAC_APR_CAL,
	ADM_DTS_EAGLE,
	ADM_SRS_TRUMEDIA,
	ADM_RTAC_AUDVOL_CAL,
	ADM_MAX_CAL_TYPES
};

enum {
	ADM_MEM_MAP_INDEX_SOURCE_TRACKING = ADM_MAX_CAL_TYPES,
	ADM_MEM_MAP_INDEX_MAX
};

enum {
	ADM_CLIENT_ID_DEFAULT = 0,
	ADM_CLIENT_ID_SOURCE_TRACKING,
	ADM_CLIENT_ID_MAX,
};

#define MAX_COPPS_PER_PORT 0x8
#define ADM_MAX_CHANNELS 8

/* multiple copp per stream. */
struct route_payload {
	unsigned int copp_idx[MAX_COPPS_PER_PORT];
	unsigned int port_id[MAX_COPPS_PER_PORT];
	int app_type[MAX_COPPS_PER_PORT];
	int acdb_dev_id[MAX_COPPS_PER_PORT];
	int sample_rate[MAX_COPPS_PER_PORT];
	unsigned short num_copps;
	unsigned int session_id;
};

int srs_trumedia_open(int port_id, int copp_idx, __s32 srs_tech_id,
		      void *srs_params);

int adm_dts_eagle_set(int port_id, int copp_idx, int param_id,
		      void *data, uint32_t size);

int adm_dts_eagle_get(int port_id, int copp_idx, int param_id,
		      void *data, uint32_t size);

void adm_copp_mfc_cfg(int port_id, int copp_idx, int dst_sample_rate);

int adm_get_params(int port_id, int copp_idx, uint32_t module_id,
		   uint32_t param_id, uint32_t params_length, char *params);

int adm_send_params_v5(int port_id, int copp_idx, char *params,
			      uint32_t params_length);

int adm_dolby_dap_send_params(int port_id, int copp_idx, char *params,
			      uint32_t params_length);

int adm_open(int port, int path, int rate, int mode, int topology,
			   int perf_mode, uint16_t bits_per_sample,
			   int app_type, int acdbdev_id);

int adm_map_rtac_block(struct rtac_cal_block_data *cal_block);

int adm_unmap_rtac_block(uint32_t *mem_map_handle);

int adm_close(int port, int topology, int perf_mode);

int adm_matrix_map(int path, struct route_payload payload_map,
		   int perf_mode, uint32_t passthr_mode);

int adm_connect_afe_port(int mode, int session_id, int port_id);

void adm_ec_ref_rx_id(int  port_id);

void adm_num_ec_ref_rx_chans(int num_chans);

void adm_ec_ref_rx_bit_width(int bit_width);

void adm_ec_ref_rx_sampling_rate(int sampling_rate);

int adm_get_lowlatency_copp_id(int port_id);

int adm_set_multi_ch_map(char *channel_map, int path);

int adm_get_multi_ch_map(char *channel_map, int path);

int adm_validate_and_get_port_index(int port_id);

int adm_get_default_copp_idx(int port_id);

int adm_get_topology_for_port_from_copp_id(int port_id, int copp_id);

int adm_get_topology_for_port_copp_idx(int port_id, int copp_idx);

int adm_get_indexes_from_copp_id(int copp_id, int *port_idx, int *copp_idx);

int adm_set_stereo_to_custom_stereo(int port_id, int copp_idx,
				    unsigned int session_id,
				    char *params, uint32_t params_length);

int adm_get_pp_topo_module_list(int port_id, int copp_idx, int32_t param_length,
				char *params);

int adm_set_volume(int port_id, int copp_idx, int volume);

int adm_set_softvolume(int port_id, int copp_idx,
		       struct audproc_softvolume_params *softvol_param);

int adm_set_mic_gain(int port_id, int copp_idx, int volume);

int adm_send_set_multichannel_ec_primary_mic_ch(int port_id, int copp_idx,
				int primary_mic_ch);

int adm_param_enable(int port_id, int copp_idx, int module_id,  int enable);

int adm_send_calibration(int port_id, int copp_idx, int path, int perf_mode,
			 int cal_type, char *params, int size);

int adm_set_wait_parameters(int port_id, int copp_idx);

int adm_reset_wait_parameters(int port_id, int copp_idx);

int adm_wait_timeout(int port_id, int copp_idx, int wait_time);

int adm_store_cal_data(int port_id, int copp_idx, int path, int perf_mode,
		       int cal_type, char *params, int *size);

int adm_send_compressed_device_mute(int port_id, int copp_idx, bool mute_on);

int adm_send_compressed_device_latency(int port_id, int copp_idx, int latency);
int adm_set_sound_focus(int port_id, int copp_idx,
			struct sound_focus_param soundFocusData);
int adm_get_sound_focus(int port_id, int copp_idx,
			struct sound_focus_param *soundFocusData);
int adm_get_source_tracking(int port_id, int copp_idx,
			    struct source_tracking_param *sourceTrackingData);
int adm_swap_speaker_channels(int port_id, int copp_idx, int sample_rate,
				bool spk_swap);
#endif /* __Q6_ADM_V2_H__ */
