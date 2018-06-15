/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
#ifndef __SDSP_ANC_H__
#define __SDSP_ANC_H__

#include <sound/q6afe-v2.h>
#include <sound/apr_audio-v2.h>

#define AUD_MSVC_MODULE_AUDIO_DEV_RESOURCE_SHARE           0x0001028A
#define AUD_MSVC_PARAM_ID_PORT_SHARE_RESOURCE_CONFIG       0x00010297
#define AUD_MSVC_API_VERSION_SHARE_RESOURCE_CONFIG         0x1
#define AUD_MSVC_MODULE_AUDIO_DEV_ANC_REFS                 0x00010254
#define AUD_MSVC_PARAM_ID_DEV_ANC_REFS_CONFIG              0x00010286
#define AUD_MSVC_API_VERSION_DEV_ANC_REFS_CONFIG           0x1
#define AUD_MSVC_MODULE_AUDIO_DEV_ANC_ALGO                 0x00010234

struct aud_msvc_port_param_data_v2 {
	/* ID of the module to be configured.
	 * Supported values: Valid module ID
	 */
	u32 module_id;

	/* ID of the parameter corresponding to the supported parameters
	 * for the module ID.
	 * Supported values: Valid parameter ID
	 */
	u32 param_id;

	/* Actual size of the data for the
	 * module_id/param_id pair. The size is a
	 * multiple of four bytes.
	 * Supported values: > 0
	 */
	u16 param_size;

	/* This field must be set to zero.
	 */
	u16 reserved;
} __packed;


/*  Payload of the #AFE_PORT_CMD_SET_PARAM_V2 command's
 * configuration/calibration settings for the AFE port.
 */
struct aud_msvc_port_cmd_set_param_v2 {
	/* Port interface and direction (Rx or Tx) to start.
	 */
	u16 port_id;

	/* Actual size of the payload in bytes.
	 * This is used for parsing the parameter payload.
	 * Supported values: > 0
	 */
	u16 payload_size;

	/* LSW of 64 bit Payload address.
	 * Address should be 32-byte,
	 * 4kbyte aligned and must be contiguous memory.
	 */
	u32 payload_address_lsw;

	/* MSW of 64 bit Payload address.
	 * In case of 32-bit shared memory address,
	 * this field must be set to zero.
	 * In case of 36-bit shared memory address,
	 * bit-4 to bit-31 must be set to zero.
	 * Address should be 32-byte, 4kbyte aligned
	 * and must be contiguous memory.
	 */
	u32 payload_address_msw;

	/* Memory map handle returned by
	 * AFE_SERVICE_CMD_SHARED_MEM_MAP_REGIONS commands.
	 * Supported Values:
	 * - NULL -- Message. The parameter data is in-band.
	 * - Non-NULL -- The parameter data is Out-band.Pointer to
	 * the physical address
	 * in shared memory of the payload data.
	 * An optional field is available if parameter
	 * data is in-band:
	 * aud_msvc_param_data_v2 param_data[...].
	 * For detailed payload content, see the
	 * aud_msvc_port_param_data_v2 structure.
	 */
	u32 mem_map_handle;

} __packed;

/*  Payload of the #AFE_PORT_CMD_GET_PARAM_V2 command,
 * which queries for one post/preprocessing parameter of a
 * stream.
 */
struct aud_msvc_port_cmd_get_param_v2 {
	/* Port interface and direction (Rx or Tx) to start. */
	u16 port_id;

	/* Maximum data size of the parameter ID/module ID combination.
	 * This is a multiple of four bytes
	 * Supported values: > 0
	 */
	u16 payload_size;

	/* LSW of 64 bit Payload address. Address should be 32-byte,
	 * 4kbyte aligned and must be contig memory.
	 */
	u32 payload_address_lsw;

	/* MSW of 64 bit Payload address. In case of 32-bit shared
	 * memory address, this field must be set to zero. In case of 36-bit
	 * shared memory address, bit-4 to bit-31 must be set to zero.
	 * Address should be 32-byte, 4kbyte aligned and must be contiguous
	 * memory.
	 */
	u32 payload_address_msw;

	/* Memory map handle returned by
	 * AFE_SERVICE_CMD_SHARED_MEM_MAP_REGIONS commands.
	 * Supported Values: - NULL -- Message. The parameter data is
	 * in-band. - Non-NULL -- The parameter data is Out-band.Pointer to
	 * - the physical address in shared memory of the payload data.
	 * For detailed payload content, see the aud_msvc_port_param_data_v2
	 * structure
	 */
	u32 mem_map_handle;

	/* ID of the module to be queried.
	 * Supported values: Valid module ID
	 */
	 u32 module_id;

	 /* ID of the parameter to be queried.
	 * Supported values: Valid parameter ID
	 */
	u32 param_id;

} __packed;

struct aud_audioif_config_command {
	struct apr_hdr hdr;
	struct aud_msvc_port_cmd_set_param_v2 param;
	struct aud_msvc_port_param_data_v2    pdata;
	union afe_port_config            port;
} __packed;

struct aud_msvc_param_id_dev_share_resource_cfg {
	u32                  minor_version;
	u16                  rddma_idx;
	u16                  wrdma_idx;
	u32                  lpm_start_addr;
	u32                  lpm_length;
} __packed;

struct aud_msvc_param_id_dev_anc_refs_cfg {
	u32                  minor_version;
	u16                  port_id;
	u16                  num_channel;
	u32                  sample_rate;
	u32                  bit_width;
} __packed;

struct anc_share_resource_command {
	struct apr_hdr hdr;
	struct aud_msvc_port_cmd_set_param_v2 param;
	struct aud_msvc_port_param_data_v2    pdata;
	struct aud_msvc_param_id_dev_share_resource_cfg resource;
} __packed;

struct anc_config_ref_command {
	struct apr_hdr hdr;
	struct aud_msvc_port_cmd_set_param_v2 param;
	struct aud_msvc_port_param_data_v2    pdata;
	struct aud_msvc_param_id_dev_anc_refs_cfg refs;
} __packed;

#define AUD_MSVC_PARAM_ID_PORT_ANC_ALGO_MODULE_ID      0x0001023A

struct aud_msvc_param_id_dev_anc_algo_module_id {
	uint32_t                  minor_version;
	uint32_t                  module_id;
} __packed;

struct anc_set_algo_module_id_command {
	struct apr_hdr hdr;
	struct aud_msvc_port_cmd_set_param_v2 param;
	struct aud_msvc_port_param_data_v2    pdata;
	struct aud_msvc_param_id_dev_anc_algo_module_id set_algo_module_id;
} __packed;


#define AUD_MSVC_PARAM_ID_PORT_ANC_MIC_SPKR_LAYOUT_INFO   0x0001029C

#define AUD_MSVC_API_VERSION_DEV_ANC_MIC_SPKR_LAYOUT_INFO        0x1

#define AUD_MSVC_ANC_MAX_NUM_OF_MICS              16
#define AUD_MSVC_ANC_MAX_NUM_OF_SPKRS             16

struct aud_msvc_param_id_dev_anc_mic_spkr_layout_info {
	uint32_t minor_version;
	uint16_t mic_layout_array[AUD_MSVC_ANC_MAX_NUM_OF_MICS];
	uint16_t spkr_layout_array[AUD_MSVC_ANC_MAX_NUM_OF_SPKRS];
	uint16_t num_anc_mic;
	uint16_t num_anc_spkr;
	uint16_t num_add_mic_signal;
	uint16_t num_add_spkr_signal;
} __packed;

struct anc_set_mic_spkr_layout_info_command {
	struct apr_hdr hdr;
	struct aud_msvc_port_cmd_set_param_v2 param;
	struct aud_msvc_port_param_data_v2    pdata;
	struct aud_msvc_param_id_dev_anc_mic_spkr_layout_info
		set_mic_spkr_layout;
} __packed;

struct anc_set_algo_module_cali_data_command {
	struct apr_hdr hdr;
	struct aud_msvc_port_cmd_set_param_v2 param;
	struct aud_msvc_port_param_data_v2    pdata;
	/*
	 * calibration data payload followed
	 */
} __packed;

struct anc_get_algo_module_cali_data_command {
	struct apr_hdr hdr;
	struct aud_msvc_port_cmd_get_param_v2 param;
	struct aud_msvc_port_param_data_v2    pdata;
	/*
	 * calibration data payload followed
	 */
} __packed;

struct anc_get_algo_module_cali_data_resp {
	uint32_t status;
	struct aud_msvc_port_param_data_v2 pdata;
	uint32_t payload[128];
} __packed;

int anc_if_tdm_port_start(u16 port_id, struct afe_tdm_port_config *tdm_port);

int anc_if_tdm_port_stop(u16 port_id);

int anc_if_share_resource(u16 port_id, u16 rddma_idx, u16 wrdma_idx,
		u32 lpm_start_addr, u32 lpm_length);

int anc_if_config_ref(u16 port_id, u32 sample_rate, u32 bit_width,
		u16 num_channel);

int anc_if_set_algo_module_id(u16 port_id, u32 module_id);

int anc_if_set_anc_mic_spkr_layout(u16 port_id,
struct aud_msvc_param_id_dev_anc_mic_spkr_layout_info *set_mic_spkr_layout_p);

int anc_if_set_algo_module_cali_data(u16 port_id, void *data_p);

int anc_if_get_algo_module_cali_data(u16 port_id, void *data_p);

int anc_if_shared_mem_map(void);

int anc_if_shared_mem_unmap(void);

#endif /* __SDSP_ANC_H__ */
