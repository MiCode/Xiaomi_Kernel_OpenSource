/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#include <linux/err.h>
#include <linux/module.h>
#include <linux/bitops.h>
#include <sound/control.h>
#include <sound/q6adm-v2.h>

#include "msm-ds2-dap-config.h"
#include "msm-pcm-routing-v2.h"
#include <sound/q6core.h>


#ifdef CONFIG_DOLBY_DS2

/* ramp up/down for 30ms    */
#define DOLBY_SOFT_VOLUME_PERIOD	40
/* Step value 0ms or 0us */
#define DOLBY_SOFT_VOLUME_STEP		1000
#define DOLBY_ADDITIONAL_RAMP_WAIT	10
#define SOFT_VOLUME_PARAM_SIZE		3
#define PARAM_PAYLOAD_SIZE		3

enum {
	DOLBY_SOFT_VOLUME_CURVE_LINEAR = 0,
	DOLBY_SOFT_VOLUME_CURVE_EXP,
	DOLBY_SOFT_VOLUME_CURVE_LOG,
};

#define VOLUME_ZERO_GAIN     0x0
#define VOLUME_UNITY_GAIN    0x2000
/* Wait time for module enable/disble */
#define DOLBY_MODULE_ENABLE_PERIOD     50

/* DOLBY device definitions end */
enum {
	DOLBY_OFF_CACHE = 0,
	DOLBY_SPEKAER_CACHE,
	DOLBY_HEADPHONE_CACHE,
	DOLBY_HDMI_CACHE,
	DOLBY_WFD_CACHE,
	DOLBY_FM_CACHE,
	DOLBY_MAX_CACHE,
};

enum {
	DAP_SOFT_BYPASS = 0,
	DAP_HARD_BYPASS,
};

enum {
	MODULE_DISABLE = 0,
	MODULE_ENABLE,
};
/* dolby param ids to/from dsp */
static uint32_t	ds2_dap_params_id[MAX_DS2_PARAMS] = {
	DOLBY_PARAM_ID_VDHE, DOLBY_PARAM_ID_VSPE, DOLBY_PARAM_ID_DSSF,
	DOLBY_PARAM_ID_DVLI, DOLBY_PARAM_ID_DVLO, DOLBY_PARAM_ID_DVLE,
	DOLBY_PARAM_ID_DVMC, DOLBY_PARAM_ID_DVME, DOLBY_PARAM_ID_IENB,
	DOLBY_PARAM_ID_IEBF, DOLBY_PARAM_ID_IEON, DOLBY_PARAM_ID_DEON,
	DOLBY_PARAM_ID_NGON, DOLBY_PARAM_ID_GEON, DOLBY_PARAM_ID_GENB,
	DOLBY_PARAM_ID_GEBF, DOLBY_PARAM_ID_AONB, DOLBY_PARAM_ID_AOBF,
	DOLBY_PARAM_ID_AOBG, DOLBY_PARAM_ID_AOON, DOLBY_PARAM_ID_ARNB,
	DOLBY_PARAM_ID_ARBF, DOLBY_PARAM_ID_PLB,  DOLBY_PARAM_ID_PLMD,
	DOLBY_PARAM_ID_DHSB, DOLBY_PARAM_ID_DHRG, DOLBY_PARAM_ID_DSSB,
	DOLBY_PARAM_ID_DSSA, DOLBY_PARAM_ID_DVLA, DOLBY_PARAM_ID_IEBT,
	DOLBY_PARAM_ID_IEA,  DOLBY_PARAM_ID_DEA,  DOLBY_PARAM_ID_DED,
	DOLBY_PARAM_ID_GEBG, DOLBY_PARAM_ID_AOCC, DOLBY_PARAM_ID_ARBI,
	DOLBY_PARAM_ID_ARBL, DOLBY_PARAM_ID_ARBH, DOLBY_PARAM_ID_AROD,
	DOLBY_PARAM_ID_ARTP, DOLBY_PARAM_ID_VMON, DOLBY_PARAM_ID_VMB,
	DOLBY_PARAM_ID_VCNB, DOLBY_PARAM_ID_VCBF, DOLBY_PARAM_ID_PREG,
	DOLBY_PARAM_ID_VEN,  DOLBY_PARAM_ID_PSTG, DOLBY_PARAM_ID_INIT_ENDP,
};

/* modifed state:	0x00000000 - Not updated
*			> 0x00000000 && < 0x00010000
*				Updated and not commited to DSP
*			0x00010001 - Updated and commited to DSP
*			> 0x00010001 - Modified the commited value
*/
/* param offset */
static uint32_t	ds2_dap_params_offset[MAX_DS2_PARAMS] = {
	DOLBY_PARAM_VDHE_OFFSET, DOLBY_PARAM_VSPE_OFFSET,
	DOLBY_PARAM_DSSF_OFFSET, DOLBY_PARAM_DVLI_OFFSET,
	DOLBY_PARAM_DVLO_OFFSET, DOLBY_PARAM_DVLE_OFFSET,
	DOLBY_PARAM_DVMC_OFFSET, DOLBY_PARAM_DVME_OFFSET,
	DOLBY_PARAM_IENB_OFFSET, DOLBY_PARAM_IEBF_OFFSET,
	DOLBY_PARAM_IEON_OFFSET, DOLBY_PARAM_DEON_OFFSET,
	DOLBY_PARAM_NGON_OFFSET, DOLBY_PARAM_GEON_OFFSET,
	DOLBY_PARAM_GENB_OFFSET, DOLBY_PARAM_GEBF_OFFSET,
	DOLBY_PARAM_AONB_OFFSET, DOLBY_PARAM_AOBF_OFFSET,
	DOLBY_PARAM_AOBG_OFFSET, DOLBY_PARAM_AOON_OFFSET,
	DOLBY_PARAM_ARNB_OFFSET, DOLBY_PARAM_ARBF_OFFSET,
	DOLBY_PARAM_PLB_OFFSET,  DOLBY_PARAM_PLMD_OFFSET,
	DOLBY_PARAM_DHSB_OFFSET, DOLBY_PARAM_DHRG_OFFSET,
	DOLBY_PARAM_DSSB_OFFSET, DOLBY_PARAM_DSSA_OFFSET,
	DOLBY_PARAM_DVLA_OFFSET, DOLBY_PARAM_IEBT_OFFSET,
	DOLBY_PARAM_IEA_OFFSET,  DOLBY_PARAM_DEA_OFFSET,
	DOLBY_PARAM_DED_OFFSET,  DOLBY_PARAM_GEBG_OFFSET,
	DOLBY_PARAM_AOCC_OFFSET, DOLBY_PARAM_ARBI_OFFSET,
	DOLBY_PARAM_ARBL_OFFSET, DOLBY_PARAM_ARBH_OFFSET,
	DOLBY_PARAM_AROD_OFFSET, DOLBY_PARAM_ARTP_OFFSET,
	DOLBY_PARAM_VMON_OFFSET, DOLBY_PARAM_VMB_OFFSET,
	DOLBY_PARAM_VCNB_OFFSET, DOLBY_PARAM_VCBF_OFFSET,
	DOLBY_PARAM_PREG_OFFSET, DOLBY_PARAM_VEN_OFFSET,
	DOLBY_PARAM_PSTG_OFFSET, DOLBY_PARAM_INT_ENDP_OFFSET,
};
/* param_length */
static uint32_t	ds2_dap_params_length[MAX_DS2_PARAMS] = {
	DOLBY_PARAM_VDHE_LENGTH, DOLBY_PARAM_VSPE_LENGTH,
	DOLBY_PARAM_DSSF_LENGTH, DOLBY_PARAM_DVLI_LENGTH,
	DOLBY_PARAM_DVLO_LENGTH, DOLBY_PARAM_DVLE_LENGTH,
	DOLBY_PARAM_DVMC_LENGTH, DOLBY_PARAM_DVME_LENGTH,
	DOLBY_PARAM_IENB_LENGTH, DOLBY_PARAM_IEBF_LENGTH,
	DOLBY_PARAM_IEON_LENGTH, DOLBY_PARAM_DEON_LENGTH,
	DOLBY_PARAM_NGON_LENGTH, DOLBY_PARAM_GEON_LENGTH,
	DOLBY_PARAM_GENB_LENGTH, DOLBY_PARAM_GEBF_LENGTH,
	DOLBY_PARAM_AONB_LENGTH, DOLBY_PARAM_AOBF_LENGTH,
	DOLBY_PARAM_AOBG_LENGTH, DOLBY_PARAM_AOON_LENGTH,
	DOLBY_PARAM_ARNB_LENGTH, DOLBY_PARAM_ARBF_LENGTH,
	DOLBY_PARAM_PLB_LENGTH,  DOLBY_PARAM_PLMD_LENGTH,
	DOLBY_PARAM_DHSB_LENGTH, DOLBY_PARAM_DHRG_LENGTH,
	DOLBY_PARAM_DSSB_LENGTH, DOLBY_PARAM_DSSA_LENGTH,
	DOLBY_PARAM_DVLA_LENGTH, DOLBY_PARAM_IEBT_LENGTH,
	DOLBY_PARAM_IEA_LENGTH,  DOLBY_PARAM_DEA_LENGTH,
	DOLBY_PARAM_DED_LENGTH,  DOLBY_PARAM_GEBG_LENGTH,
	DOLBY_PARAM_AOCC_LENGTH, DOLBY_PARAM_ARBI_LENGTH,
	DOLBY_PARAM_ARBL_LENGTH, DOLBY_PARAM_ARBH_LENGTH,
	DOLBY_PARAM_AROD_LENGTH, DOLBY_PARAM_ARTP_LENGTH,
	DOLBY_PARAM_VMON_LENGTH, DOLBY_PARAM_VMB_LENGTH,
	DOLBY_PARAM_VCNB_LENGTH, DOLBY_PARAM_VCBF_LENGTH,
	DOLBY_PARAM_PREG_LENGTH, DOLBY_PARAM_VEN_LENGTH,
	DOLBY_PARAM_PSTG_LENGTH, DOLBY_PARAM_INT_ENDP_LENGTH,
};

struct ds2_dap_params_s {
	int32_t params_val[TOTAL_LENGTH_DS2_PARAM];
	int32_t dap_params_modified[MAX_DS2_PARAMS];
};

struct audio_rx_cal_data {
	char aud_proc_data[AUD_PROC_BLOCK_SIZE];
	int32_t  aud_proc_size;
	char aud_vol_data[AUD_VOL_BLOCK_SIZE];
	int32_t aud_vol_size;
};

static struct ds2_dap_params_s ds2_dap_params[DOLBY_MAX_CACHE];

struct ds2_device_mapping {
	int32_t device_id;
	int port_id;
	/*Only one Dolby COPP  for a specific port*/
	int copp_idx;
	int cache_dev;
	uint32_t stream_ref_count;
	bool active;
	void *cal_data;
};

static struct ds2_device_mapping dev_map[DS2_DEVICES_ALL];

struct ds2_dap_params_states_s {
	bool use_cache;
	bool dap_bypass;
	bool dap_bypass_type;
	bool node_opened;
	int32_t  device;
	bool custom_stereo_onoff;
};

static struct ds2_dap_params_states_s ds2_dap_params_states = {true, false,
				false, DEVICE_NONE};

static int all_supported_devices = EARPIECE|SPEAKER|WIRED_HEADSET|
			WIRED_HEADPHONE|BLUETOOTH_SCO|AUX_DIGITAL|
			ANLG_DOCK_HEADSET|DGTL_DOCK_HEADSET|
			REMOTE_SUBMIX|ANC_HEADSET|ANC_HEADPHONE|
			PROXY|FM|FM_TX|DEVICE_NONE|
			BLUETOOTH_SCO_HEADSET|BLUETOOTH_SCO_CARKIT;


static void msm_ds2_dap_check_and_update_ramp_wait(int port_id, int copp_idx,
						   int *ramp_wait)
{

	int32_t *update_params_value = NULL;
	uint32_t params_length = SOFT_VOLUME_PARAM_SIZE * sizeof(uint32_t);
	uint32_t param_payload_len = PARAM_PAYLOAD_SIZE * sizeof(uint32_t);
	int rc = 0;

	update_params_value = kzalloc(params_length, GFP_KERNEL);
	if (!update_params_value) {
		pr_err("%s: params memory alloc failed\n", __func__);
		goto end;
	}
	rc = adm_get_params(port_id, copp_idx,
			    AUDPROC_MODULE_ID_VOL_CTRL,
			    AUDPROC_PARAM_ID_SOFT_VOL_STEPPING_PARAMETERS,
			    params_length + param_payload_len,
			    (char *) update_params_value);
	if (rc == 0) {
		pr_debug("%s: params_value [0x%x, 0x%x, 0x%x]\n",
			__func__, update_params_value[0],
			update_params_value[1],
			update_params_value[2]);
		*ramp_wait = update_params_value[0];
	}
end:
	kfree(update_params_value);
	/*
	 * No error returned as we do not need to error out from dap on/dap
	 * bypass. The default ramp parameter will be used to wait during
	 * ramp down.
	 */
	return;
}

static int msm_ds2_dap_set_vspe_vdhe(int dev_map_idx,
				     bool is_custom_stereo_enabled)
{
	int32_t *update_params_value = NULL;
	int32_t *param_val = NULL;
	int idx, i, j, rc = 0, cdev;
	uint32_t params_length = (TOTAL_LENGTH_DOLBY_PARAM +
				2 * DOLBY_PARAM_PAYLOAD_SIZE) *
				sizeof(uint32_t);

	if (dev_map_idx < 0 || dev_map_idx >= DS2_DEVICES_ALL) {
		pr_err("%s: invalid dev map index %d\n", __func__, dev_map_idx);
		rc = -EINVAL;
		goto end;
	}

	if (dev_map[dev_map_idx].port_id == DOLBY_INVALID_PORT_ID) {
		pr_err("%s: Invalid port id\n", __func__);
		rc = -EINVAL;
		goto end;
	}

	if ((dev_map[dev_map_idx].copp_idx < 0) ||
		(dev_map[dev_map_idx].copp_idx >= MAX_COPPS_PER_PORT)) {
		pr_err("%s: Invalid copp_idx\n", __func__);
		rc = -EINVAL;
		goto end;
	}

	if ((dev_map[dev_map_idx].port_id != SLIMBUS_0_RX) &&
	     (dev_map[dev_map_idx].port_id != RT_PROXY_PORT_001_RX)) {
		pr_debug("%s:No Custom stereo for port:0x%x\n",
			 __func__, dev_map[dev_map_idx].port_id);
		goto end;
	}

	update_params_value = kzalloc(params_length, GFP_KERNEL);
	if (!update_params_value) {
		pr_err("%s: params memory alloc failed\n", __func__);
		rc = -ENOMEM;
		goto end;
	}
	params_length = 0;
	param_val = update_params_value;
	cdev = dev_map[dev_map_idx].cache_dev;
	/* for VDHE and VSPE DAP params at index 0 and 1 in table */
	for (i = 0; i < 2; i++) {
		*update_params_value++ = DOLBY_BUNDLE_MODULE_ID;
		*update_params_value++ = ds2_dap_params_id[i];
		*update_params_value++ = ds2_dap_params_length[i] *
					sizeof(uint32_t);
		idx = ds2_dap_params_offset[i];
		for (j = 0; j < ds2_dap_params_length[i]; j++) {
			if (is_custom_stereo_enabled)
				*update_params_value++ = 0;
			else
				*update_params_value++ =
					ds2_dap_params[cdev].params_val[idx+j];
		}
		params_length += (DOLBY_PARAM_PAYLOAD_SIZE +
				  ds2_dap_params_length[i]) *
				  sizeof(uint32_t);
	}

	pr_debug("%s: valid param length: %d\n", __func__, params_length);
	if (params_length) {
		rc = adm_dolby_dap_send_params(dev_map[dev_map_idx].port_id,
					       dev_map[dev_map_idx].copp_idx,
					       (char *)param_val,
					       params_length);
		if (rc) {
			pr_err("%s: send vdhe/vspe params failed with rc=%d\n",
				__func__, rc);
			rc = -EINVAL;
			goto end;
		}
	}
end:
	kfree(param_val);
	return rc;
}

int qti_set_custom_stereo_on(int port_id, int copp_idx,
			     bool is_custom_stereo_on)
{

	uint16_t op_FL_ip_FL_weight;
	uint16_t op_FL_ip_FR_weight;
	uint16_t op_FR_ip_FL_weight;
	uint16_t op_FR_ip_FR_weight;

	int32_t *update_params_value32 = NULL, rc = 0;
	int32_t *param_val = NULL;
	int16_t *update_params_value16 = 0;
	uint32_t params_length_bytes = CUSTOM_STEREO_PAYLOAD_SIZE *
				       sizeof(uint32_t);
	uint32_t avail_length = params_length_bytes;

	if ((port_id != SLIMBUS_0_RX) &&
	     (port_id != RT_PROXY_PORT_001_RX)) {
		pr_debug("%s:No Custom stereo for port:0x%x\n",
			 __func__, port_id);
		goto skip_send_cmd;
	}

	pr_debug("%s: port 0x%x, copp_idx %d, is_custom_stereo_on %d\n",
		 __func__, port_id, copp_idx, is_custom_stereo_on);
	if (is_custom_stereo_on) {
		op_FL_ip_FL_weight =
			Q14_GAIN_ZERO_POINT_FIVE;
		op_FL_ip_FR_weight =
			Q14_GAIN_ZERO_POINT_FIVE;
		op_FR_ip_FL_weight =
			Q14_GAIN_ZERO_POINT_FIVE;
		op_FR_ip_FR_weight =
			Q14_GAIN_ZERO_POINT_FIVE;
	} else {
		op_FL_ip_FL_weight = Q14_GAIN_UNITY;
		op_FL_ip_FR_weight = 0;
		op_FR_ip_FL_weight = 0;
		op_FR_ip_FR_weight = Q14_GAIN_UNITY;
	}

	update_params_value32 = kzalloc(params_length_bytes, GFP_KERNEL);
	if (!update_params_value32) {
		pr_err("%s, params memory alloc failed\n", __func__);
		rc = -ENOMEM;
		goto skip_send_cmd;
	}
	param_val = update_params_value32;
	if (avail_length < 2 * sizeof(uint32_t))
		goto skip_send_cmd;
	*update_params_value32++ = MTMX_MODULE_ID_DEFAULT_CHMIXER;
	*update_params_value32++ = DEFAULT_CHMIXER_PARAM_ID_COEFF;
	avail_length = avail_length - (2 * sizeof(uint32_t));

	update_params_value16 = (int16_t *)update_params_value32;
	if (avail_length < 10 * sizeof(uint16_t))
		goto skip_send_cmd;
	*update_params_value16++ = CUSTOM_STEREO_CMD_PARAM_SIZE;
	/* for alignment only*/
	*update_params_value16++ = 0;
	/* index is 32-bit param in little endian*/
	*update_params_value16++ = CUSTOM_STEREO_INDEX_PARAM;
	*update_params_value16++ = 0;
	/* for stereo mixing num out ch*/
	*update_params_value16++ = CUSTOM_STEREO_NUM_OUT_CH;
	/* for stereo mixing num in ch*/
	*update_params_value16++ = CUSTOM_STEREO_NUM_IN_CH;

	/* Out ch map FL/FR*/
	*update_params_value16++ = PCM_CHANNEL_FL;
	*update_params_value16++ = PCM_CHANNEL_FR;

	/* In ch map FL/FR*/
	*update_params_value16++ = PCM_CHANNEL_FL;
	*update_params_value16++ = PCM_CHANNEL_FR;
	avail_length = avail_length - (10 * sizeof(uint16_t));
	/* weighting coefficients as name suggests,
	mixing will be done according to these coefficients*/
	if (avail_length < 4 * sizeof(uint16_t))
		goto skip_send_cmd;
	*update_params_value16++ = op_FL_ip_FL_weight;
	*update_params_value16++ = op_FL_ip_FR_weight;
	*update_params_value16++ = op_FR_ip_FL_weight;
	*update_params_value16++ = op_FR_ip_FR_weight;
	avail_length = avail_length - (4 * sizeof(uint16_t));
	if (params_length_bytes != 0) {
		rc = adm_dolby_dap_send_params(port_id, copp_idx,
				(char *)param_val,
				params_length_bytes);
		if (rc) {
			pr_err("%s: send params failed rc=%d\n", __func__, rc);
			rc = -EINVAL;
			goto skip_send_cmd;
		}
	}
	kfree(param_val);
	return 0;
skip_send_cmd:
		pr_err("%s: insufficient memory, send cmd failed\n",
			__func__);
		kfree(param_val);
		return rc;
}
static int dap_set_custom_stereo_onoff(int dev_map_idx,
					bool is_custom_stereo_enabled)
{

	int32_t *update_params_value = NULL, rc = 0;
	int32_t *param_val = NULL;
	uint32_t params_length_bytes = (TOTAL_LENGTH_DOLBY_PARAM +
				DOLBY_PARAM_PAYLOAD_SIZE) * sizeof(uint32_t);
	if ((dev_map[dev_map_idx].port_id != SLIMBUS_0_RX) &&
	     (dev_map[dev_map_idx].port_id != RT_PROXY_PORT_001_RX)) {
		pr_debug("%s:No Custom stereo for port:0x%x\n",
			 __func__, dev_map[dev_map_idx].port_id);
		goto end;
	}

	if ((dev_map[dev_map_idx].copp_idx < 0) ||
		(dev_map[dev_map_idx].copp_idx >= MAX_COPPS_PER_PORT)) {
		rc = -EINVAL;
		goto end;
	}

	/* DAP custom stereo */
	msm_ds2_dap_set_vspe_vdhe(dev_map_idx,
				  is_custom_stereo_enabled);
	update_params_value = kzalloc(params_length_bytes, GFP_KERNEL);
	if (!update_params_value) {
		pr_err("%s: params memory alloc failed\n", __func__);
		rc = -ENOMEM;
		goto end;
	}
	params_length_bytes = 0;
	param_val = update_params_value;
	*update_params_value++ = DOLBY_BUNDLE_MODULE_ID;
	*update_params_value++ = DOLBY_ENABLE_CUSTOM_STEREO;
	*update_params_value++ = sizeof(uint32_t);
	if (is_custom_stereo_enabled)
		*update_params_value++ = 1;
	else
		*update_params_value++ = 0;
	params_length_bytes += (DOLBY_PARAM_PAYLOAD_SIZE + 1) *
				sizeof(uint32_t);
	pr_debug("%s: valid param length: %d\n", __func__, params_length_bytes);
	if (params_length_bytes) {
		rc = adm_dolby_dap_send_params(dev_map[dev_map_idx].port_id,
					       dev_map[dev_map_idx].copp_idx,
					       (char *)param_val,
					       params_length_bytes);
		if (rc) {
			pr_err("%s: custom stereo param failed with rc=%d\n",
				__func__, rc);
			rc = -EINVAL;
			goto end;
		}
	}
end:
	kfree(param_val);
	return rc;

}


static int set_custom_stereo_onoff(int dev_map_idx,
					bool is_custom_stereo_enabled)
{
	int rc = 0;
	pr_debug("%s: map index %d, custom stereo %d\n", __func__, dev_map_idx,
		 is_custom_stereo_enabled);

	if (dev_map_idx < 0 || dev_map_idx >= DS2_DEVICES_ALL) {
		pr_err("%s: invalid dev map index %d\n", __func__, dev_map_idx);
		rc = -EINVAL;
		goto end;
	}

	if (dev_map[dev_map_idx].port_id == DOLBY_INVALID_PORT_ID) {
		pr_err("%s: invalid port id\n", __func__);
		rc = -EINVAL;
		goto end;
	}

	if ((dev_map[dev_map_idx].copp_idx < 0) ||
		(dev_map[dev_map_idx].copp_idx >= MAX_COPPS_PER_PORT)) {
		pr_err("%s: invalid copp idx\n", __func__);
		rc = -EINVAL;
		goto end;
	}

	if (ds2_dap_params_states.dap_bypass == true &&
		ds2_dap_params_states.dap_bypass_type == DAP_HARD_BYPASS) {

		rc = qti_set_custom_stereo_on(dev_map[dev_map_idx].port_id,
					      dev_map[dev_map_idx].copp_idx,
					      is_custom_stereo_enabled);
		if (rc < 0) {
			pr_err("%s:qti_set_custom_stereo_on_copp failed C.S %d",
				__func__, is_custom_stereo_enabled);
		}
		goto end;

	}

	if (ds2_dap_params_states.dap_bypass == false) {
		rc = dap_set_custom_stereo_onoff(dev_map_idx,
						 is_custom_stereo_enabled);
		if (rc < 0) {
			pr_err("%s:qti_set_custom_stereo_on_copp failed C.S %d",
				__func__, is_custom_stereo_enabled);
		}
		goto end;
	}
end:
	return rc;
}

static int msm_ds2_dap_alloc_and_store_cal_data(int dev_map_idx, int path,
					    int perf_mode)
{
	int rc = 0;
	struct audio_rx_cal_data *aud_cal_data;
	pr_debug("%s: path %d, perf_mode %d, dev_map_idx %d\n",
		__func__, path, perf_mode, dev_map_idx);

	if (dev_map_idx < 0 || dev_map_idx >= DS2_DEVICES_ALL) {
		pr_err("%s: invalid dev map index %d\n", __func__, dev_map_idx);
		rc = -EINVAL;
		goto end;
	}

	aud_cal_data = kzalloc(sizeof(struct audio_rx_cal_data), GFP_KERNEL);
	if (!aud_cal_data) {
		pr_err("%s, param memory alloc failed\n", __func__);
		rc = -ENOMEM;
		goto end;
	}

	rc = adm_store_cal_data(dev_map[dev_map_idx].port_id,
				dev_map[dev_map_idx].copp_idx, path, perf_mode,
				ADM_AUDPROC_CAL, aud_cal_data->aud_proc_data,
				&aud_cal_data->aud_proc_size);
	if (rc < 0) {
		pr_err("%s: store cal data err %d\n", __func__, rc);
		kfree(aud_cal_data);
		goto end;
	}

	rc = adm_store_cal_data(dev_map[dev_map_idx].port_id,
				dev_map[dev_map_idx].copp_idx, path, perf_mode,
				ADM_AUDVOL_CAL, aud_cal_data->aud_vol_data,
				&aud_cal_data->aud_vol_size);
	if (rc < 0) {
		pr_err("%s: store cal data err %d\n", __func__, rc);
		kfree(aud_cal_data);
		goto end;
	}

	dev_map[dev_map_idx].cal_data = (void *)aud_cal_data;

end:
	pr_debug("%s: ret %d\n", __func__, rc);
	return rc;
}

static int msm_ds2_dap_free_cal_data(int dev_map_idx)
{
	int rc = 0;
	struct audio_rx_cal_data *aud_cal_data;

	pr_debug("%s: dev_map_idx %d\n", __func__, dev_map_idx);
	if (dev_map_idx < 0 || dev_map_idx >= DS2_DEVICES_ALL) {
		pr_err("%s: invalid dev map index %d\n", __func__, dev_map_idx);
		rc = -EINVAL;
		goto end;
	}
	aud_cal_data = (struct audio_rx_cal_data *)
				dev_map[dev_map_idx].cal_data;
	kfree(aud_cal_data);
	dev_map[dev_map_idx].cal_data = NULL;

end:
	return rc;
}

static int msm_ds2_dap_send_cal_data(int dev_map_idx)
{
	int rc = 0;
	struct audio_rx_cal_data *aud_cal_data = NULL;

	pr_debug("%s: devmap index %d\n", __func__, dev_map_idx);
	if (dev_map_idx < 0 || dev_map_idx >= DS2_DEVICES_ALL) {
		pr_err("%s: invalid dev map index %d\n", __func__, dev_map_idx);
		rc = -EINVAL;
		goto end;
	}

	if (dev_map[dev_map_idx].cal_data == NULL) {
		pr_err("%s: No valid calibration data stored for idx %d\n",
			__func__, dev_map_idx);
		rc = -EINVAL;
		goto end;
	}

	/* send aud proc cal */
	aud_cal_data = (struct audio_rx_cal_data *)
				dev_map[dev_map_idx].cal_data;
	rc = adm_send_calibration(dev_map[dev_map_idx].port_id,
				  dev_map[dev_map_idx].copp_idx,
				  ADM_PATH_PLAYBACK, 0,
				  ADM_AUDPROC_CAL,
				  aud_cal_data->aud_proc_data,
				  aud_cal_data->aud_proc_size);
	if (rc < 0) {
		pr_err("%s: adm_send_calibration failed %d\n", __func__, rc);
		goto end;
	}

	/* send aud volume cal*/
	rc = adm_send_calibration(dev_map[dev_map_idx].port_id,
				  dev_map[dev_map_idx].copp_idx,
				  ADM_PATH_PLAYBACK, 0,
				  ADM_AUDVOL_CAL,
				  aud_cal_data->aud_vol_data,
				  aud_cal_data->aud_vol_size);
	if (rc < 0)
		pr_err("%s: adm_send_calibration failed %d\n", __func__, rc);
end:
	pr_debug("%s: return  %d\n", __func__, rc);
	return rc;
}

static inline int msm_ds2_dap_can_enable_module(int32_t module_id)
{
	if (module_id == MTMX_MODULE_ID_DEFAULT_CHMIXER ||
		module_id == AUDPROC_MODULE_ID_RESAMPLER ||
		module_id == AUDPROC_MODULE_ID_VOL_CTRL) {
		return false;
	}
	return true;
}

static int msm_ds2_dap_init_modules_in_topology(int dev_map_idx)
{
	int rc = 0, i = 0, port_id, copp_idx;
	/* Account for 32 bit interger allocation */
	int32_t param_sz = (ADM_GET_TOPO_MODULE_LIST_LENGTH / sizeof(uint32_t));
	int32_t *update_param_val = NULL;

	if (dev_map_idx < 0 || dev_map_idx >= DS2_DEVICES_ALL) {
		pr_err("%s: invalid dev map index %d\n", __func__, dev_map_idx);
		rc = -EINVAL;
		goto end;
	}

	port_id = dev_map[dev_map_idx].port_id;
	copp_idx = dev_map[dev_map_idx].copp_idx;
	pr_debug("%s: port_id 0x%x copp_idx %d\n", __func__, port_id, copp_idx);
	update_param_val = kzalloc(ADM_GET_TOPO_MODULE_LIST_LENGTH, GFP_KERNEL);
	if (!update_param_val) {
		pr_err("%s, param memory alloc failed\n", __func__);
		rc = -ENOMEM;
		goto end;
	}

	if (!ds2_dap_params_states.dap_bypass) {
		/* get modules from dsp */
		rc = adm_get_pp_topo_module_list(port_id, copp_idx,
			ADM_GET_TOPO_MODULE_LIST_LENGTH,
			(char *)update_param_val);
		if (rc < 0) {
			pr_err("%s:topo list port %d, err %d,copp_idx %d\n",
				__func__, port_id, copp_idx, rc);
			goto end;
		}

		if (update_param_val[0] > (param_sz - 1)) {
			pr_err("%s:max modules exp/ret [%d: %d]\n",
				__func__, (param_sz - 1),
				update_param_val[0]);
			rc = -EINVAL;
			goto end;
		}
		/* Turn off modules */
		for (i = 1; i < update_param_val[0]; i++) {
			if (!msm_ds2_dap_can_enable_module(
				update_param_val[i]) ||
				(update_param_val[i] == DS2_MODULE_ID)) {
				pr_debug("%s: Do not enable/disable %d\n",
					 __func__, update_param_val[i]);
				continue;
			}

			pr_debug("%s: param disable %d\n",
				__func__, update_param_val[i]);
			adm_param_enable(port_id, copp_idx, update_param_val[i],
					 MODULE_DISABLE);
		}
	} else {
		msm_ds2_dap_send_cal_data(dev_map_idx);

	}
	adm_param_enable(port_id, copp_idx, DS2_MODULE_ID,
			 !ds2_dap_params_states.dap_bypass);
end:
	kfree(update_param_val);
	return rc;
}

static bool msm_ds2_dap_check_is_param_modified(int32_t *dap_params_modified,
				    int32_t idx, int32_t commit)
{
	if ((dap_params_modified[idx] == 0) ||
		(commit &&
		((dap_params_modified[idx] & 0x00010000) &&
		((dap_params_modified[idx] & 0x0000FFFF) <= 1)))) {
		pr_debug("%s: not modified at idx %d\n", __func__, idx);
		return false;
	}
	pr_debug("%s: modified at idx %d\n", __func__, idx);
	return true;
}

static int msm_ds2_dap_map_device_to_dolby_cache_devices(int32_t device_id)
{
	int32_t cache_dev = -1;
	switch (device_id) {
	case DEVICE_NONE:
		cache_dev = DOLBY_OFF_CACHE;
		break;
	case EARPIECE:
	case SPEAKER:
		cache_dev = DOLBY_SPEKAER_CACHE;
		break;
	case WIRED_HEADSET:
	case WIRED_HEADPHONE:
	case ANLG_DOCK_HEADSET:
	case DGTL_DOCK_HEADSET:
	case ANC_HEADSET:
	case ANC_HEADPHONE:
	case BLUETOOTH_SCO:
	case BLUETOOTH_SCO_HEADSET:
	case BLUETOOTH_SCO_CARKIT:
		cache_dev = DOLBY_HEADPHONE_CACHE;
		break;
	case FM:
	case FM_TX:
		cache_dev = DOLBY_FM_CACHE;
		break;
	case AUX_DIGITAL:
		cache_dev = DOLBY_HDMI_CACHE;
		break;
	case PROXY:
	case REMOTE_SUBMIX:
		cache_dev = DOLBY_WFD_CACHE;
		break;
	default:
		pr_err("%s: invalid cache device\n", __func__);
	}
	pr_debug("%s: cache device %d\n", __func__, cache_dev);
	return cache_dev;
}

static int msm_ds2_dap_update_num_devices(struct dolby_param_data *dolby_data,
				      int32_t *num_device, int32_t *dev_arr,
				      int32_t array_size)
{
	int32_t idx = 0;
	int supported_devices = 0;

	if (!array_size) {
		pr_err("%s: array size zero\n", __func__);
		return -EINVAL;
	}

	if (dolby_data->device_id == DEVICE_OUT_ALL ||
		dolby_data->device_id == DEVICE_OUT_DEFAULT)
		supported_devices = all_supported_devices;
	else
		supported_devices = dolby_data->device_id;

	if ((idx < array_size) && (supported_devices & EARPIECE))
		dev_arr[idx++] = EARPIECE;
	if ((idx < array_size) && (supported_devices & SPEAKER))
		dev_arr[idx++] = SPEAKER;
	if ((idx < array_size) && (supported_devices & WIRED_HEADSET))
		dev_arr[idx++] = WIRED_HEADSET;
	if ((idx < array_size) && (supported_devices & WIRED_HEADPHONE))
		dev_arr[idx++] = WIRED_HEADPHONE;
	if ((idx < array_size) && (supported_devices & BLUETOOTH_SCO))
		dev_arr[idx++] = BLUETOOTH_SCO;
	if ((idx < array_size) && (supported_devices & BLUETOOTH_SCO_CARKIT))
		dev_arr[idx++] = BLUETOOTH_SCO_CARKIT;
	if ((idx < array_size) && (supported_devices & BLUETOOTH_SCO_HEADSET))
		dev_arr[idx++] = BLUETOOTH_SCO_HEADSET;
	if ((idx < array_size) && (supported_devices & AUX_DIGITAL))
		dev_arr[idx++] = AUX_DIGITAL;
	if ((idx < array_size) && (supported_devices & ANLG_DOCK_HEADSET))
		dev_arr[idx++] = ANLG_DOCK_HEADSET;
	if ((idx < array_size) && (supported_devices & DGTL_DOCK_HEADSET))
		dev_arr[idx++] = DGTL_DOCK_HEADSET;
	if ((idx < array_size) && (supported_devices & REMOTE_SUBMIX))
		dev_arr[idx++] = REMOTE_SUBMIX;
	if ((idx < array_size) && (supported_devices & ANC_HEADSET))
		dev_arr[idx++] = ANC_HEADSET;
	if ((idx < array_size) && (supported_devices & ANC_HEADPHONE))
		dev_arr[idx++] = ANC_HEADPHONE;
	if ((idx < array_size) && (supported_devices & PROXY))
		dev_arr[idx++] = PROXY;
	if ((idx < array_size) && (supported_devices & FM))
		dev_arr[idx++] = FM;
	if ((idx < array_size) && (supported_devices & FM_TX))
		dev_arr[idx++] = FM_TX;
	/* CHECK device none separately */
	if ((idx < array_size) && (supported_devices == DEVICE_NONE))
		dev_arr[idx++] = DEVICE_NONE;
	pr_debug("%s: dev id 0x%x, idx %d\n", __func__,
		 supported_devices, idx);
	*num_device = idx;
	return 0;
}

static int msm_ds2_dap_get_port_id(
		int32_t device_id, int32_t be_id)
{
	struct msm_pcm_routing_bdai_data bedais;
	int port_id = DOLBY_INVALID_PORT_ID;
	int port_type = 0;

	if (be_id < 0) {
		port_id = -1;
		goto end;
	}

	msm_pcm_routing_get_bedai_info(be_id, &bedais);
	pr_debug("%s: be port_id %d\n", __func__, bedais.port_id);
	port_id = bedais.port_id;
	port_type = afe_get_port_type(bedais.port_id);
	if (port_type != MSM_AFE_PORT_TYPE_RX)
		port_id = DOLBY_INVALID_PORT_ID;
end:
	pr_debug("%s: device_id 0x%x, be_id %d, port_id %d\n",
		 __func__, device_id, be_id, port_id);
	return port_id;
}

static int msm_ds2_dap_update_dev_map_port_id(int32_t device_id, int port_id)
{
	int i;
	for (i = 0; i < DS2_DEVICES_ALL; i++) {
		if (dev_map[i].device_id == device_id)
			dev_map[i].port_id = port_id;
	}
	pr_debug("%s: port_id %d, device_id 0x%x\n",
		 __func__, port_id, device_id);
	return 0;
}

static int msm_ds2_dap_handle_bypass_wait(int port_id, int copp_idx,
					  int wait_time)
{
	int ret = 0;
	adm_set_wait_parameters(port_id, copp_idx);
	msm_pcm_routing_release_lock();
	ret = adm_wait_timeout(port_id, copp_idx, wait_time);
	msm_pcm_routing_acquire_lock();
	/* Reset the parameters if wait has timed out */
	if (ret == 0)
		adm_reset_wait_parameters(port_id, copp_idx);
	return ret;
}

static int msm_ds2_dap_handle_bypass(struct dolby_param_data *dolby_data)
{
	int rc = 0, i = 0, j = 0;
	/*Account for 32 bit interger allocation  */
	int32_t param_sz = (ADM_GET_TOPO_MODULE_LIST_LENGTH / sizeof(uint32_t));
	int32_t *mod_list = NULL;
	int port_id = 0, copp_idx = -1;
	bool cs_onoff = ds2_dap_params_states.custom_stereo_onoff;
	int ramp_wait = DOLBY_SOFT_VOLUME_PERIOD;

	pr_debug("%s: bypass type %d bypass %d custom stereo %d\n", __func__,
		 ds2_dap_params_states.dap_bypass_type,
		 ds2_dap_params_states.dap_bypass,
		 ds2_dap_params_states.custom_stereo_onoff);
	mod_list = kzalloc(ADM_GET_TOPO_MODULE_LIST_LENGTH, GFP_KERNEL);
	if (!mod_list) {
		pr_err("%s: param memory alloc failed\n", __func__);
		rc = -ENOMEM;
		goto end;
	}

	for (i = 0; i < DS2_DEVICES_ALL; i++) {
		pr_debug("%s: active dev %d\n", __func__, dev_map[i].active);
		if (dev_map[i].active) {
			port_id = dev_map[i].port_id;
			copp_idx = dev_map[i].copp_idx;

			if (port_id == DOLBY_INVALID_PORT_ID) {
				pr_err("%s: invalid port\n", __func__);
				rc = 0;
				goto end;
			}

			if ((copp_idx < 0) ||
				(copp_idx >= MAX_COPPS_PER_PORT)) {
				pr_err("%s: Invalid copp_idx\n", __func__);
				rc = 0;
				goto end;
			}

			/* getmodules from dsp */
			rc = adm_get_pp_topo_module_list(port_id, copp_idx,
				    ADM_GET_TOPO_MODULE_LIST_LENGTH,
				    (char *)mod_list);
			if (rc < 0) {
				pr_err("%s:adm get topo list port %d",
					__func__, port_id);
				pr_err("copp_idx %d, err %d\n",
					copp_idx, rc);
				goto end;
			}
			if (mod_list[0] > (param_sz - 1)) {
				pr_err("%s:max modules exp/ret [%d: %d]\n",
					__func__, (param_sz - 1),
					mod_list[0]);
				rc = -EINVAL;
				goto end;
			}
			/*
			 * get ramp parameters
			 * check for change in ramp parameters
			 * update ramp wait
			 */
			msm_ds2_dap_check_and_update_ramp_wait(port_id,
							       copp_idx,
							       &ramp_wait);

			/* Mute before switching modules */
			rc = adm_set_volume(port_id, copp_idx,
					    VOLUME_ZERO_GAIN);
			if (rc < 0) {
				/*
				 * Not Fatal can continue bypass operations.
				 * Do not need to block playback
				 */
				pr_info("%s :Set volume port_id %d",
					__func__, port_id);
				pr_info("copp_idx %d, error %d\n",
					copp_idx, rc);
			}

			rc = msm_ds2_dap_handle_bypass_wait(port_id, copp_idx,
					    (ramp_wait +
					     DOLBY_ADDITIONAL_RAMP_WAIT));
			if (rc == -EINTR) {
				pr_info("%s:bypass interupted-ignore,port %d",
					__func__, port_id);
				pr_info("copp_idx %d\n", copp_idx);
				rc = 0;
				continue;
			}

			/* if dap bypass is set */
			if (ds2_dap_params_states.dap_bypass) {
				/* Turn off dap module */
				adm_param_enable(port_id, copp_idx,
						 DS2_MODULE_ID, MODULE_DISABLE);
				/*
				 * If custom stereo is on at the time of bypass,
				 * switch off custom stereo on dap and turn on
				 * custom stereo on qti channel mixer.
				 */
				if (cs_onoff) {
					rc = dap_set_custom_stereo_onoff(i,
								!cs_onoff);
					if (rc < 0) {
						pr_info("%s:D_CS i %d,rc %d\n",
							__func__, i, rc);
					}
					rc = qti_set_custom_stereo_on(port_id,
								      copp_idx,
								      cs_onoff);
					if (rc < 0) {
						pr_info("%s:Q_CS port id 0x%x",
							 __func__, port_id);
						pr_info("copp idx %d, rc %d\n",
							copp_idx, rc);
					}
				}
				/* Add adm api to resend calibration on port */
				rc = msm_ds2_dap_send_cal_data(i);
				if (rc < 0) {
					/*
					 * Not fatal,continue bypass operations.
					 * Do not need to block playback
					 */
					pr_info("%s:send cal err %d index %d\n",
						__func__, rc, i);
				}
			} else {
				/* Turn off qti modules */
				for (j = 1; j < mod_list[0]; j++) {
					if (!msm_ds2_dap_can_enable_module(
						mod_list[j]) ||
						mod_list[j] ==
						DS2_MODULE_ID)
						continue;
					pr_debug("%s: param disable %d\n",
						__func__, mod_list[j]);
					adm_param_enable(port_id, copp_idx,
							 mod_list[j],
							 MODULE_DISABLE);
				}

				/* Enable DAP modules */
				pr_debug("%s:DS2 param enable\n", __func__);
				adm_param_enable(port_id, copp_idx,
						 DS2_MODULE_ID, MODULE_ENABLE);
				/*
				 * If custom stereo is on at the time of dap on,
				 * switch off custom stereo on qti channel mixer
				 * and turn on custom stereo on DAP.
				 * mixer(qti).
				 */
				if (cs_onoff) {
					rc = qti_set_custom_stereo_on(port_id,
								copp_idx,
								!cs_onoff);
					if (rc < 0) {
						pr_info("%s:Q_CS port_id 0x%x",
							__func__, port_id);
						pr_info("copp_idx %d rc %d\n",
							copp_idx, rc);
					}
					rc = dap_set_custom_stereo_onoff(i,
								cs_onoff);
					if (rc < 0) {
						pr_info("%s:D_CS i %d,rc %d\n",
							__func__, i, rc);
					}
				}
			}

			rc = msm_ds2_dap_handle_bypass_wait(port_id, copp_idx,
				DOLBY_MODULE_ENABLE_PERIOD);
			if (rc == -EINTR) {
				pr_info("%s:bypass interupted port_id %d copp_idx %d\n",
					__func__, port_id, copp_idx);
				/* Interrupted ignore bypass */
				rc = 0;
				continue;
			}

			/* set volume to unity gain after module on/off */
			rc = adm_set_volume(port_id, copp_idx,
					    VOLUME_UNITY_GAIN);
			if (rc < 0) {
				/*
				 * Not Fatal can continue bypass operations.
				 * Do not need to block playback
				 */
				pr_info("%s: Set vol port %d copp %d, rc %d\n",
					__func__, port_id, copp_idx, rc);
				rc = 0;
			}
		}
	}

end:
	kfree(mod_list);
	pr_debug("%s:return rc=%d\n", __func__, rc);
	return rc;
}

static int msm_ds2_dap_send_end_point(int dev_map_idx, int endp_idx)
{
	int rc = 0;
	int32_t  *update_params_value = NULL, *params_value = NULL;
	uint32_t params_length = (DOLBY_PARAM_INT_ENDP_LENGTH +
				DOLBY_PARAM_PAYLOAD_SIZE) * sizeof(uint32_t);
	int cache_device = 0;
	struct ds2_dap_params_s *ds2_ap_params_obj = NULL;
	int32_t *modified_param = NULL;

	if (dev_map_idx < 0 || dev_map_idx >= DS2_DEVICES_ALL) {
		pr_err("%s: invalid dev map index %d\n", __func__, dev_map_idx);
		rc = -EINVAL;
		goto end;
	}
	cache_device = dev_map[dev_map_idx].cache_dev;

	ds2_ap_params_obj = &ds2_dap_params[cache_device];
	pr_debug("%s: cache dev %d, dev_map_idx %d\n", __func__,
		 cache_device, dev_map_idx);
	pr_debug("%s: endp - %p %p\n",  __func__,
		 &ds2_dap_params[cache_device], ds2_ap_params_obj);

	params_value = kzalloc(params_length, GFP_KERNEL);
	if (!params_value) {
		pr_err("%s: params memory alloc failed\n", __func__);
		rc = -ENOMEM;
		goto end;
	}

	if (dev_map[dev_map_idx].port_id == DOLBY_INVALID_PORT_ID) {
		pr_err("%s: invalid port\n", __func__);
		rc = -EINVAL;
		goto end;
	}

	if ((dev_map[dev_map_idx].copp_idx < 0) ||
		(dev_map[dev_map_idx].copp_idx >= MAX_COPPS_PER_PORT)) {
		pr_err("%s: Invalid copp_idx\n", __func__);
		rc = -EINVAL;
		goto end;
	}

	update_params_value = params_value;
	*update_params_value++ = DOLBY_BUNDLE_MODULE_ID;
	*update_params_value++ = DOLBY_PARAM_ID_INIT_ENDP;
	*update_params_value++ = DOLBY_PARAM_INT_ENDP_LENGTH * sizeof(uint32_t);
	*update_params_value++ = ds2_ap_params_obj->params_val[
					ds2_dap_params_offset[endp_idx]];
	pr_debug("%s: off %d, length %d\n", __func__,
		 ds2_dap_params_offset[endp_idx],
		 ds2_dap_params_length[endp_idx]);
	pr_debug("%s: param 0x%x, param val %d\n", __func__,
		 ds2_dap_params_id[endp_idx], ds2_ap_params_obj->
		 params_val[ds2_dap_params_offset[endp_idx]]);
	rc = adm_dolby_dap_send_params(dev_map[dev_map_idx].port_id,
				       dev_map[dev_map_idx].copp_idx,
				       (char *)params_value, params_length);
	if (rc) {
		pr_err("%s: send dolby params failed rc %d\n", __func__, rc);
		rc = -EINVAL;
	}
	modified_param = ds2_ap_params_obj->dap_params_modified;
	if (modified_param == NULL) {
		pr_err("%s: modified param structure invalid\n",
		       __func__);
		rc = -EINVAL;
		goto end;
	}

	if (msm_ds2_dap_check_is_param_modified(modified_param, endp_idx, 0))
		ds2_ap_params_obj->dap_params_modified[endp_idx] = 0x00010001;

end:
	kfree(params_value);
	return rc;
}

static int msm_ds2_dap_send_cached_params(int dev_map_idx,
					  int commit)
{
	int32_t *update_params_value = NULL, *params_value = NULL;
	uint32_t idx, i, j, ret = 0;
	uint32_t params_length = (TOTAL_LENGTH_DOLBY_PARAM +
				(MAX_DS2_PARAMS - 1) *
				DOLBY_PARAM_PAYLOAD_SIZE) *
				sizeof(uint32_t);
	int cache_device = 0;
	struct ds2_dap_params_s *ds2_ap_params_obj = NULL;
	int32_t *modified_param = NULL;

	if (dev_map_idx < 0 || dev_map_idx >= DS2_DEVICES_ALL) {
		pr_err("%s: invalid dev map index %d\n", __func__, dev_map_idx);
		ret = -EINVAL;
		goto end;
	}
	cache_device = dev_map[dev_map_idx].cache_dev;

	/* Use off profile cache in only for soft bypass */
	if (ds2_dap_params_states.dap_bypass_type == DAP_SOFT_BYPASS &&
		ds2_dap_params_states.dap_bypass == true) {
		pr_debug("%s: use bypass cache 0\n", __func__);
		cache_device =  dev_map[0].cache_dev;
	}

	ds2_ap_params_obj = &ds2_dap_params[cache_device];
	pr_debug("%s: cached param - %p %p, cache_device %d\n", __func__,
		 &ds2_dap_params[cache_device], ds2_ap_params_obj,
		 cache_device);
	params_value = kzalloc(params_length, GFP_KERNEL);
	if (!params_value) {
		pr_err("%s: params memory alloc failed\n", __func__);
		ret =  -ENOMEM;
		goto end;
	}

	if (dev_map[dev_map_idx].port_id == DOLBY_INVALID_PORT_ID) {
		pr_err("%s: invalid port id\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	if ((dev_map[dev_map_idx].copp_idx < 0) ||
		(dev_map[dev_map_idx].copp_idx >= MAX_COPPS_PER_PORT)) {
		pr_err("%s: Invalid copp_idx\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	update_params_value = params_value;
	params_length = 0;
	for (i = 0; i < (MAX_DS2_PARAMS-1); i++) {
		/*get the pointer to the param modified array in the cache*/
		modified_param = ds2_ap_params_obj->dap_params_modified;
		if (modified_param == NULL) {
			pr_err("%s: modified param structure invalid\n",
			       __func__);
			ret = -EINVAL;
			goto end;
		}
		if (!msm_ds2_dap_check_is_param_modified(modified_param, i,
							 commit))
			continue;
		*update_params_value++ = DOLBY_BUNDLE_MODULE_ID;
		*update_params_value++ = ds2_dap_params_id[i];
		*update_params_value++ = ds2_dap_params_length[i] *
						sizeof(uint32_t);
		idx = ds2_dap_params_offset[i];
		for (j = 0; j < ds2_dap_params_length[i]; j++) {
			*update_params_value++ =
					ds2_ap_params_obj->params_val[idx+j];
			pr_debug("%s: id 0x%x,val %d\n", __func__,
				 ds2_dap_params_id[i],
				 ds2_ap_params_obj->params_val[idx+j]);
		}
		params_length += (DOLBY_PARAM_PAYLOAD_SIZE +
				ds2_dap_params_length[i]) * sizeof(uint32_t);
	}

	pr_debug("%s: valid param length: %d\n", __func__, params_length);
	if (params_length) {
		ret = adm_dolby_dap_send_params(dev_map[dev_map_idx].port_id,
						dev_map[dev_map_idx].copp_idx,
						(char *)params_value,
						params_length);
		if (ret) {
			pr_err("%s: send dolby params failed ret %d\n",
				__func__, ret);
			ret = -EINVAL;
			goto end;
		}
		for (i = 0; i < MAX_DS2_PARAMS-1; i++) {
			/*get pointer to the param modified array in the cache*/
			modified_param = ds2_ap_params_obj->dap_params_modified;
			if (modified_param == NULL) {
				pr_err("%s: modified param struct invalid\n",
					__func__);
				ret = -EINVAL;
				goto end;
			}
			if (!msm_ds2_dap_check_is_param_modified(
					modified_param, i, commit))
				continue;
			ds2_ap_params_obj->dap_params_modified[i] = 0x00010001;
		}
	}
end:
	kfree(params_value);
	return ret;
}

static int msm_ds2_dap_commit_params(struct dolby_param_data *dolby_data,
				 int commit)
{
	int ret = 0, i, idx;
	struct ds2_dap_params_s *ds2_ap_params_obj =  NULL;
	int32_t *modified_param = NULL;

	/* Do not commit params if in hard bypass */
	if (ds2_dap_params_states.dap_bypass_type == DAP_HARD_BYPASS &&
		ds2_dap_params_states.dap_bypass == true) {
		pr_debug("%s: called in bypass", __func__);
		ret = -EINVAL;
		goto end;
	}
	for (idx = 0; idx < MAX_DS2_PARAMS; idx++) {
		if (DOLBY_PARAM_ID_INIT_ENDP == ds2_dap_params_id[idx])
			break;
	}
	if (idx >= MAX_DS2_PARAMS || idx < 0) {
		pr_err("%s: index of DS2 Param not found idx %d\n",
			__func__, idx);
		ret = -EINVAL;
		goto end;
	}
	pr_debug("%s: found endp - idx %d 0x%x\n", __func__, idx,
		ds2_dap_params_id[idx]);
	for (i = 0; i < DS2_DEVICES_ALL; i++) {
		pr_debug("%s:dev[0x%x,0x%x],i:%d,active:%d,bypass:%d,type:%d\n",
			__func__, dolby_data->device_id, dev_map[i].device_id,
			i, dev_map[i].active, ds2_dap_params_states.dap_bypass,
			ds2_dap_params_states.dap_bypass_type);

		if (((dev_map[i].device_id & ds2_dap_params_states.device) ||
			((ds2_dap_params_states.dap_bypass_type ==
			DAP_SOFT_BYPASS) &&
			(ds2_dap_params_states.dap_bypass == true))) &&
			(dev_map[i].active == true)) {

			/*get ptr to the cache storing the params for device*/
			if ((ds2_dap_params_states.dap_bypass_type ==
				DAP_SOFT_BYPASS) &&
				(ds2_dap_params_states.dap_bypass == true))
				ds2_ap_params_obj =
					&ds2_dap_params[dev_map[0].cache_dev];
			else
				ds2_ap_params_obj =
					&ds2_dap_params[dev_map[i].cache_dev];

			/*get the pointer to the param modified array in cache*/
			modified_param = ds2_ap_params_obj->dap_params_modified;
			if (modified_param == NULL) {
				pr_err("%s: modified_param NULL\n", __func__);
				ret = -EINVAL;
				goto end;
			}

			/*
			 * Send the endp param if use cache is set
			 * or if param is modified
			 */
			if (!commit || msm_ds2_dap_check_is_param_modified(
					modified_param, idx, commit)) {
				msm_ds2_dap_send_end_point(i, idx);
				commit = 0;
			}
			ret = msm_ds2_dap_send_cached_params(i, commit);
			if (ret < 0) {
				pr_err("%s: send cached param %d\n",
					__func__, ret);
				goto end;
			}
		}
	}
end:
	return ret;
}

static int msm_ds2_dap_handle_commands(u32 cmd, void *arg)
{
	int ret  = 0, port_id = 0;
	struct dolby_param_data *dolby_data = (struct dolby_param_data *)arg;

	pr_debug("%s: param_id %d,be_id %d,device_id 0x%x,length %d,data %d\n",
		 __func__, dolby_data->param_id, dolby_data->be_id,
		dolby_data->device_id, dolby_data->length, dolby_data->data[0]);

	switch (dolby_data->param_id) {
	case DAP_CMD_COMMIT_ALL:
		msm_ds2_dap_commit_params(dolby_data, 0);
	break;

	case DAP_CMD_COMMIT_CHANGED:
		msm_ds2_dap_commit_params(dolby_data, 1);
	break;

	case DAP_CMD_USE_CACHE_FOR_INIT:
		ds2_dap_params_states.use_cache = dolby_data->data[0];
	break;

	case DAP_CMD_SET_BYPASS:
		pr_debug("%s: bypass %d bypass type %d, data %d\n", __func__,
			 ds2_dap_params_states.dap_bypass,
			 ds2_dap_params_states.dap_bypass_type,
			 dolby_data->data[0]);
		/* Do not perform bypass operation if bypass state is same*/
		if (ds2_dap_params_states.dap_bypass == dolby_data->data[0])
			break;
		ds2_dap_params_states.dap_bypass = dolby_data->data[0];
		/* hard bypass */
		if (ds2_dap_params_states.dap_bypass_type == DAP_HARD_BYPASS)
			msm_ds2_dap_handle_bypass(dolby_data);
		/* soft bypass */
		msm_ds2_dap_commit_params(dolby_data, 0);
	break;

	case DAP_CMD_SET_BYPASS_TYPE:
		if (dolby_data->data[0] == true)
			ds2_dap_params_states.dap_bypass_type =
				DAP_HARD_BYPASS;
		else
			ds2_dap_params_states.dap_bypass_type =
				DAP_SOFT_BYPASS;
		pr_debug("%s: bypass type %d", __func__,
			 ds2_dap_params_states.dap_bypass_type);
	break;

	case DAP_CMD_SET_ACTIVE_DEVICE:
		pr_debug("%s: DAP_CMD_SET_ACTIVE_DEVICE length %d\n",
			__func__, dolby_data->length);
		/* TODO: need to handle multiple instance*/
		ds2_dap_params_states.device |= dolby_data->device_id;
		port_id = msm_ds2_dap_get_port_id(
						  dolby_data->device_id,
						  dolby_data->be_id);
		pr_debug("%s: device id 0x%x all_dev 0x%x port_id %d\n",
			__func__, dolby_data->device_id,
			ds2_dap_params_states.device, port_id);
		msm_ds2_dap_update_dev_map_port_id(dolby_data->device_id,
					   port_id);
		if (port_id == DOLBY_INVALID_PORT_ID) {
			pr_err("%s: invalid port id %d\n", __func__, port_id);
			ret = -EINVAL;
			goto end;
		}
	break;
	}
end:
	return ret;

}

static int msm_ds2_dap_set_param(u32 cmd, void *arg)
{
	int rc = 0, idx, i, j, off, port_id = 0, cdev = 0;
	int32_t num_device = 0;
	int32_t dev_arr[DS2_DSP_SUPPORTED_ENDP_DEVICE] = {0};
	struct dolby_param_data *dolby_data =  (struct dolby_param_data *)arg;

	rc = msm_ds2_dap_update_num_devices(dolby_data, &num_device, dev_arr,
				   DS2_DSP_SUPPORTED_ENDP_DEVICE);
	if (num_device == 0 || rc < 0) {
		pr_err("%s: num devices 0\n", __func__);
		rc = -EINVAL;
		goto end;
	}
	for (i = 0; i < num_device; i++) {
		port_id = msm_ds2_dap_get_port_id(dev_arr[i],
						  dolby_data->be_id);
		if (port_id != DOLBY_INVALID_PORT_ID)
			msm_ds2_dap_update_dev_map_port_id(dev_arr[i], port_id);

		cdev = msm_ds2_dap_map_device_to_dolby_cache_devices(
							  dev_arr[i]);
		if (cdev < 0 || cdev >= DOLBY_MAX_CACHE) {
			pr_err("%s: Invalide cache device %d for device 0x%x\n",
				__func__, cdev, dev_arr[i]);
			rc = -EINVAL;
			goto end;
		}
		pr_debug("%s:port:%d,be:%d,dev:0x%x,cdev:%d,param:0x%x,len:%d\n"
			 , __func__, port_id, dolby_data->be_id, dev_arr[i],
			 cdev, dolby_data->param_id, dolby_data->length);
		for (idx = 0; idx < MAX_DS2_PARAMS; idx++) {
			/*paramid from user space*/
			if (dolby_data->param_id == ds2_dap_params_id[idx])
				break;
		}
		if (idx > MAX_DS2_PARAMS-1) {
			pr_err("%s: invalid param id 0x%x at idx %d\n",
				__func__, dolby_data->param_id, idx);
			rc = -EINVAL;
			goto end;
		}

		/* cache the parameters */
		ds2_dap_params[cdev].dap_params_modified[idx] += 1;
		for (j = 0; j <  dolby_data->length; j++) {
			off = ds2_dap_params_offset[idx];
			ds2_dap_params[cdev].params_val[off + j] =
							dolby_data->data[j];
				pr_debug("%s:off %d,val[i/p:o/p]-[%d / %d]\n",
					 __func__, off, dolby_data->data[j],
					 ds2_dap_params[cdev].
					 params_val[off + j]);
		}
	}
end:
	return rc;
}

static int msm_ds2_dap_get_param(u32 cmd, void *arg)
{
	int rc = 0, i, port_id = 0, copp_idx = -1;
	struct dolby_param_data *dolby_data = (struct dolby_param_data *)arg;
	int32_t *update_params_value = NULL, *params_value = NULL;
	uint32_t params_length = DOLBY_MAX_LENGTH_INDIVIDUAL_PARAM *
					sizeof(uint32_t);
	uint32_t param_payload_len =
			DOLBY_PARAM_PAYLOAD_SIZE * sizeof(uint32_t);

	/* Return error on get param in soft or hard bypass */
	if (ds2_dap_params_states.dap_bypass == true) {
		pr_err("%s: called in bypass_type %d bypass %d\n", __func__,
			ds2_dap_params_states.dap_bypass_type,
			ds2_dap_params_states.dap_bypass);
		rc = -EINVAL;
		goto end;
	}

	for (i = 0; i < DS2_DEVICES_ALL; i++) {
		if ((dev_map[i].active) &&
			(dev_map[i].device_id & dolby_data->device_id)) {
			port_id = dev_map[i].port_id;
			copp_idx = dev_map[i].copp_idx;
			break;
		}
	}

	if (port_id == DOLBY_INVALID_PORT_ID) {
		pr_err("%s: Invalid port\n", __func__);
		rc = -EINVAL;
		goto end;
	}

	if ((copp_idx < 0) || (copp_idx >= MAX_COPPS_PER_PORT)) {
		pr_err("%s: Invalid copp_idx\n", __func__);
		rc = -EINVAL;
		goto end;
	}

	pr_debug("%s: port_id 0x%x, copp_idx %d, dev_map[i].device_id %x\n",
		 __func__, port_id, copp_idx, dev_map[i].device_id);

	params_value = kzalloc(params_length, GFP_KERNEL);
	if (!params_value) {
		pr_err("%s: params memory alloc failed\n", __func__);
		rc = -ENOMEM;
		goto end;
	}

	if (dolby_data->param_id == DOLBY_PARAM_ID_VER) {
		rc = adm_get_params(port_id, copp_idx,
				    DOLBY_BUNDLE_MODULE_ID,
				    DOLBY_PARAM_ID_VER,
				    params_length + param_payload_len,
				    (char *)params_value);
	} else {
		for (i = 0; i < MAX_DS2_PARAMS; i++)
			if (ds2_dap_params_id[i] ==
				dolby_data->param_id)
				break;
		if (i > MAX_DS2_PARAMS-1) {
			pr_err("%s: invalid param id 0x%x at id %d\n", __func__,
				dolby_data->param_id, i);
			rc = -EINVAL;
			goto end;
		} else {
			params_length = (ds2_dap_params_length[i] +
						DOLBY_PARAM_PAYLOAD_SIZE) *
						sizeof(uint32_t);
			rc = adm_get_params(port_id, copp_idx,
					    DOLBY_BUNDLE_MODULE_ID,
					    ds2_dap_params_id[i],
					    params_length +
					    param_payload_len,
					    (char *)params_value);
		}
	}
	if (rc) {
		pr_err("%s: get parameters failed rc %d\n", __func__, rc);
		rc = -EINVAL;
		goto end;
	}
	update_params_value = params_value;
	if (copy_to_user((void *)dolby_data->data,
			&update_params_value[DOLBY_PARAM_PAYLOAD_SIZE],
			(dolby_data->length * sizeof(uint32_t)))) {
		pr_err("%s: error getting param\n", __func__);
		rc = -EFAULT;
		goto end;
	}
end:
	kfree(params_value);
	return rc;
}

static int msm_ds2_dap_param_visualizer_control_get(u32 cmd, void *arg)
{
	int32_t *visualizer_data = NULL;
	int  i = 0, ret = 0, port_id = -1, cache_dev = -1, copp_idx = -1;
	int32_t *update_visualizer_data = NULL;
	struct dolby_param_data *dolby_data = (struct dolby_param_data *)arg;
	uint32_t offset, length, params_length;
	uint32_t param_payload_len =
		DOLBY_PARAM_PAYLOAD_SIZE * sizeof(uint32_t);

	for (i = 0; i < DS2_DEVICES_ALL; i++) {
		if ((dev_map[i].active))  {
			port_id = dev_map[i].port_id;
			cache_dev = dev_map[i].cache_dev;
			copp_idx = dev_map[i].copp_idx;
			break;
		}
	}

	if (port_id == DOLBY_INVALID_PORT_ID ||
		(copp_idx < 0) || (copp_idx >= MAX_COPPS_PER_PORT)) {
		ret = 0;
		dolby_data->length = 0;
		pr_err("%s: no device active\n", __func__);
		goto end;
	}

	length = ds2_dap_params[cache_dev].params_val[DOLBY_PARAM_VCNB_OFFSET];
	params_length = (2*length + DOLBY_VIS_PARAM_HEADER_SIZE) *
							 sizeof(uint32_t);

	visualizer_data = kzalloc(params_length, GFP_KERNEL);
	if (!visualizer_data) {
		pr_err("%s: params memory alloc failed\n", __func__);
		ret = -ENOMEM;
		dolby_data->length = 0;
		goto end;
	}
	memset(visualizer_data, 0x0, params_length);

	/* Return error on get param in soft or hard bypass */
	if (ds2_dap_params_states.dap_bypass == true) {
		pr_debug("%s: visualizer called in bypass, return 0\n",
			 __func__);
		ret = 0;
		dolby_data->length = 0;
		goto end;
	}

	offset = 0;
	params_length = length * sizeof(uint32_t);
	ret = adm_get_params(port_id, copp_idx,
			    DOLBY_BUNDLE_MODULE_ID,
			    DOLBY_PARAM_ID_VCBG,
			    params_length + param_payload_len,
			    (((char *)(visualizer_data)) + offset));
	if (ret) {
		pr_err("%s: get parameters failed ret %d\n", __func__, ret);
		ret = -EINVAL;
		dolby_data->length = 0;
		goto end;
	}
	offset = length * sizeof(uint32_t);
	ret = adm_get_params(port_id, copp_idx,
			    DOLBY_BUNDLE_MODULE_ID,
			    DOLBY_PARAM_ID_VCBE,
			    params_length + param_payload_len,
			    (((char *)(visualizer_data)) + offset));
	if (ret) {
		pr_err("%s: get parameters failed ret %d\n", __func__, ret);
		ret = -EINVAL;
		dolby_data->length = 0;
		goto end;
	}
	update_visualizer_data = visualizer_data;
	dolby_data->length = 2 * length;

	if (copy_to_user((void *)dolby_data->data,
			(void *)update_visualizer_data,
			(dolby_data->length * sizeof(uint32_t)))) {
		pr_err("%s: copy to user failed for data\n", __func__);
		dolby_data->length = 0;
		ret = -EFAULT;
		goto end;
	}

end:
	kfree(visualizer_data);
	return ret;
}

int msm_ds2_dap_set_security_control(u32 cmd, void *arg)
{
	struct dolby_param_license *dolby_license =
				 ((struct dolby_param_license *)arg);
	pr_err("%s: dmid %d license key %d\n", __func__,
		dolby_license->dmid, dolby_license->license_key);
	core_set_dolby_manufacturer_id(dolby_license->dmid);
	core_set_license(dolby_license->license_key, DOLBY_DS1_LICENSE_ID);
	return 0;
}

int msm_ds2_dap_update_port_parameters(struct snd_hwdep *hw,  struct file *file,
				       bool open)
{
	int  i = 0, dev_id = 0;
	pr_debug("%s: open %d\n", __func__, open);
	ds2_dap_params_states.node_opened = open;
	ds2_dap_params_states.dap_bypass = true;
	ds2_dap_params_states.dap_bypass_type = 0;
	ds2_dap_params_states.use_cache = 0;
	ds2_dap_params_states.device = 0;
	ds2_dap_params_states.custom_stereo_onoff = 0;
	for (i = 0; i < DS2_DEVICES_ALL; i++) {
		if (i == 0)
			dev_map[i].device_id = 0;
		else {
			dev_id = (1 << (i-1));
			if (all_supported_devices & dev_id)
				dev_map[i].device_id = dev_id;
			else
				continue;
		}
		dev_map[i].cache_dev =
			msm_ds2_dap_map_device_to_dolby_cache_devices(
				    dev_map[i].device_id);
		if (dev_map[i].cache_dev < 0 ||
				dev_map[i].cache_dev >= DOLBY_MAX_CACHE)
			pr_err("%s: Invalide cache device %d for device 0x%x\n",
						__func__,
						dev_map[i].cache_dev,
						dev_map[i].device_id);
		dev_map[i].port_id = -1;
		dev_map[i].active = false;
		dev_map[i].stream_ref_count = 0;
		dev_map[i].cal_data = NULL;
		dev_map[i].copp_idx = -1;
		pr_debug("%s: device_id 0x%x, cache_dev %d act  %d\n", __func__,
			 dev_map[i].device_id, dev_map[i].cache_dev,
			 dev_map[i].active);
	}
	return 0;

}

int msm_ds2_dap_ioctl_shared(struct snd_hwdep *hw, struct file *file,
			     u32 cmd, void *arg)
{
	int ret = 0;
	pr_debug("%s: cmd: 0x%x\n", __func__, cmd);
	switch (cmd) {
	case SNDRV_DEVDEP_DAP_IOCTL_SET_PARAM:
		ret = msm_ds2_dap_set_param(cmd, arg);
	break;
	case SNDRV_DEVDEP_DAP_IOCTL_GET_PARAM:
		ret = msm_ds2_dap_get_param(cmd, arg);
	break;
	case SNDRV_DEVDEP_DAP_IOCTL_DAP_COMMAND:
		ret = msm_ds2_dap_handle_commands(cmd, arg);
	break;
	case SNDRV_DEVDEP_DAP_IOCTL_DAP_LICENSE:
		ret = msm_ds2_dap_set_security_control(cmd, arg);
	break;
	case SNDRV_DEVDEP_DAP_IOCTL_GET_VISUALIZER:
		ret = msm_ds2_dap_param_visualizer_control_get(cmd, arg);
	break;
	default:
		pr_err("%s: called with invalid control 0x%x\n", __func__, cmd);
		ret = -EINVAL;
	}
	return ret;
}

int msm_ds2_dap_ioctl(struct snd_hwdep *hw, struct file *file,
		      u32 cmd, void *arg)
{

	int ret = 0;
	pr_debug("%s: cmd: 0x%x\n", __func__, cmd);
	if (!arg) {
		pr_err("%s: Invalid params event status\n", __func__);
		ret = -EINVAL;
		goto end;
	}
	switch (cmd) {
	case SNDRV_DEVDEP_DAP_IOCTL_SET_PARAM:
	case SNDRV_DEVDEP_DAP_IOCTL_DAP_COMMAND: {
		struct dolby_param_data dolby_data;
		if (copy_from_user((void *)&dolby_data, (void *)arg,
				sizeof(struct dolby_param_data))) {
			pr_err("%s: Copy from user failed\n", __func__);
			ret =  -EFAULT;
			goto end;
		}
		ret = msm_ds2_dap_ioctl_shared(hw, file, cmd, &dolby_data);
		break;
	}
	case SNDRV_DEVDEP_DAP_IOCTL_DAP_LICENSE: {
		struct dolby_param_license dolby_license;
		if (copy_from_user((void *)&dolby_license, (void *)arg,
				sizeof(struct dolby_param_license))) {
			pr_err("%s: Copy from user failed\n", __func__);
			ret = -EFAULT;
			goto end;
		}
		ret = msm_ds2_dap_ioctl_shared(hw, file, cmd, &dolby_license);
		break;
	}
	case SNDRV_DEVDEP_DAP_IOCTL_GET_PARAM:
	case SNDRV_DEVDEP_DAP_IOCTL_GET_VISUALIZER: {
		struct dolby_param_data dolby_data;
		if (copy_from_user((void *)&dolby_data, (void *)arg,
				sizeof(struct dolby_param_data))) {
			pr_err("%s: Copy from user failed\n", __func__);
			ret =  -EFAULT;
			goto end;
		}
		ret = msm_ds2_dap_ioctl_shared(hw, file, cmd, &dolby_data);
		if (ret < 0)
			pr_err("%s: ioctl cmd %d returned err %d\n",
				__func__, cmd, ret);
		if (copy_to_user((void *)arg, &dolby_data,
			sizeof(struct dolby_param_data))) {
			pr_err("%s: Copy to user failed\n", __func__);
			ret = -EFAULT;
			goto end;
		}
		break;
	}
	default:
		pr_err("%s: called with invalid control 0x%x\n", __func__, cmd);
		ret = -EINVAL;
	}
end:
	return ret;

}
#ifdef CONFIG_COMPAT
int msm_ds2_dap_compat_ioctl(struct snd_hwdep *hw, struct file *file,
			     u32 cmd, void *arg)
{
	int ret = 0;
	pr_debug("%s: cmd: 0x%x\n", __func__, cmd);
	switch (cmd) {
	case SNDRV_DEVDEP_DAP_IOCTL_SET_PARAM32:
		cmd = SNDRV_DEVDEP_DAP_IOCTL_SET_PARAM;
		goto handle_set_ioctl;
	case SNDRV_DEVDEP_DAP_IOCTL_DAP_COMMAND32:
		cmd = SNDRV_DEVDEP_DAP_IOCTL_DAP_COMMAND;
handle_set_ioctl:
	{
		struct dolby_param_data32 dolby_data32;
		struct dolby_param_data dolby_data;
		memset(&dolby_data32, 0, sizeof(dolby_data32));
		memset(&dolby_data, 0, sizeof(dolby_data));
		if (copy_from_user(&dolby_data32, (void *)arg,
				sizeof(struct dolby_param_data32))) {
			pr_err("%s: Copy from user failed\n", __func__);
			ret =  -EFAULT;
			goto end;
		}
		dolby_data.version = dolby_data32.version;
		dolby_data.device_id = dolby_data32.device_id;
		dolby_data.be_id = dolby_data32.be_id;
		dolby_data.param_id = dolby_data32.param_id;
		dolby_data.length = dolby_data32.length;
		dolby_data.data = compat_ptr(dolby_data32.data);

		ret = msm_ds2_dap_ioctl_shared(hw, file, cmd, &dolby_data);
		break;
	}
	case SNDRV_DEVDEP_DAP_IOCTL_GET_PARAM32:
		cmd = SNDRV_DEVDEP_DAP_IOCTL_GET_PARAM;
		goto handle_get_ioctl;
	case SNDRV_DEVDEP_DAP_IOCTL_GET_VISUALIZER32:
		cmd = SNDRV_DEVDEP_DAP_IOCTL_GET_VISUALIZER;
handle_get_ioctl:
	{
		struct dolby_param_data32 dolby_data32;
		struct dolby_param_data dolby_data;
		memset(&dolby_data32, 0, sizeof(dolby_data32));
		memset(&dolby_data, 0, sizeof(dolby_data));
		if (copy_from_user(&dolby_data32, (void *)arg,
				sizeof(struct dolby_param_data32))) {
			pr_err("%s: Copy from user failed\n", __func__);
			ret =  -EFAULT;
			goto end;
		}
		dolby_data.version = dolby_data32.version;
		dolby_data.device_id = dolby_data32.device_id;
		dolby_data.be_id = dolby_data32.be_id;
		dolby_data.param_id = dolby_data32.param_id;
		dolby_data.length = dolby_data32.length;
		dolby_data.data = compat_ptr(dolby_data32.data);

		ret = msm_ds2_dap_ioctl_shared(hw, file, cmd, &dolby_data);
		if (ret < 0)
			pr_err("%s: ioctl cmd %d, returned err %d\n",
				__func__, cmd, ret);
		dolby_data32.length = dolby_data.length;
		if (copy_to_user((void *)arg, &dolby_data32,
			sizeof(struct dolby_param_data32))) {
			pr_err("%s: Copy to user failed\n", __func__);
			ret = -EFAULT;
			goto end;
		}
		break;
	}
	case SNDRV_DEVDEP_DAP_IOCTL_DAP_LICENSE32: {
		struct dolby_param_license32 dolby_license32;
		struct dolby_param_license dolby_license;
		cmd = SNDRV_DEVDEP_DAP_IOCTL_DAP_LICENSE;
		if (copy_from_user((void *)&dolby_license32, (void *)arg,
			sizeof(struct dolby_param_license32))) {
			pr_err("%s: Copy from user failed\n", __func__);
			ret = -EFAULT;
			goto end;
		}
		dolby_license.dmid = dolby_license32.dmid;
		dolby_license.license_key = dolby_license32.license_key;
		ret = msm_ds2_dap_ioctl_shared(hw, file, cmd, &dolby_license);
		break;
	}
	default:
		pr_err("%s: called with invalid control 0x%x\n",
			__func__, cmd);
		ret = -EINVAL;
	}
end:
	return ret;

}
#endif

int msm_ds2_dap_init(int port_id, int copp_idx, int channels,
		     bool is_custom_stereo_on)
{
	int ret = 0, idx = -1, i;
	struct dolby_param_data dolby_data;

	struct audproc_softvolume_params softvol = {
		.period = DOLBY_SOFT_VOLUME_PERIOD,
		.step = DOLBY_SOFT_VOLUME_STEP,
		.rampingcurve = DOLBY_SOFT_VOLUME_CURVE_EXP,
	};

	pr_debug("%s: port id  %d, copp_idx %d\n", __func__, port_id, copp_idx);

	if (port_id != DOLBY_INVALID_PORT_ID) {
		for (i = 0; i < DS2_DEVICES_ALL; i++) {
			if ((dev_map[i].port_id == port_id) &&
				/* device part of active device */
				(dev_map[i].device_id &
				ds2_dap_params_states.device)) {
				idx = i;
				if (dev_map[i].device_id == SPEAKER)
					continue;
				else
					break;
			}
		}
		if (idx < 0) {
			pr_err("%s: invalid index for port %d\n",
				__func__, port_id);
			ret = -EINVAL;
			goto end;
		}
		pr_debug("%s:index %d, dev[0x%x,0x%x]\n", __func__, idx,
			 dev_map[idx].device_id, ds2_dap_params_states.device);
		dev_map[idx].active = true;
		dev_map[idx].copp_idx = copp_idx;
		dolby_data.param_id = DOLBY_COMMIT_ALL_TO_DSP;
		dolby_data.length = 0;
		dolby_data.data = NULL;
		dolby_data.device_id = dev_map[idx].device_id;
		pr_debug("%s:  idx  %d, active %d, dev id 0x%x, ref count %d\n",
			 __func__, idx, dev_map[idx].active,
			 dev_map[idx].device_id,
			 dev_map[idx].stream_ref_count);
		if (dev_map[idx].stream_ref_count == 0) {
			/*perform next 3 func only if hard bypass enabled*/
			if (ds2_dap_params_states.dap_bypass_type ==
				DAP_HARD_BYPASS) {
				ret = msm_ds2_dap_alloc_and_store_cal_data(idx,
						       ADM_PATH_PLAYBACK, 0);
				if (ret < 0)
					goto end;
				ret = adm_set_softvolume(port_id, copp_idx,
							 &softvol);
				if (ret < 0) {
					pr_err("%s: Soft volume ret error %d\n",
						__func__, ret);
					goto end;
				}
				ret =
					msm_ds2_dap_init_modules_in_topology(
							idx);
				if (ret < 0)
					goto end;
			}
			ret =  msm_ds2_dap_commit_params(&dolby_data, 0);
			if (ret < 0) {
				pr_debug("%s: commit params ret %d\n",
					__func__, ret);
				ret = 0;
			}
		}
		dev_map[idx].stream_ref_count++;
		if (is_custom_stereo_on) {
			ds2_dap_params_states.custom_stereo_onoff =
				is_custom_stereo_on;
			set_custom_stereo_onoff(idx,
						is_custom_stereo_on);
		}
	}

end:
	return ret;
}

void msm_ds2_dap_deinit(int port_id)
{
	/*
	 * Get the active port corrresponding to the active device
	 * Check if this is same as incoming port
	 * Set it to invalid
	 */
	int idx = -1, i;
	pr_debug("%s: port_id %d\n", __func__, port_id);
	if (port_id != DOLBY_INVALID_PORT_ID) {
		for (i = 0; i < DS2_DEVICES_ALL; i++) {
			/* Active port */
			if ((dev_map[i].port_id == port_id) &&
				/* device part of active device */
				(dev_map[i].device_id &
				ds2_dap_params_states.device) &&
				/*
				 * Need this check to avoid race condition of
				 * active device being set and playback
				 * instance opened
				 */
				/* active device*/
				dev_map[i].active) {
				idx = i;
				if (dev_map[i].device_id == SPEAKER)
					continue;
				else
					break;
			}
		}
		if (idx < 0) {
			pr_err("%s: invalid index for port %d\n",
				__func__, port_id);
			return;
		}
		pr_debug("%s:index %d, dev [0x%x, 0x%x]\n", __func__, idx,
			 dev_map[idx].device_id, ds2_dap_params_states.device);
		dev_map[idx].stream_ref_count--;
		if (dev_map[idx].stream_ref_count == 0) {
			/*perform next func only if hard bypass enabled*/
			if (ds2_dap_params_states.dap_bypass_type ==
				DAP_HARD_BYPASS) {
				msm_ds2_dap_free_cal_data(idx);
			}
			ds2_dap_params_states.device &= ~dev_map[idx].device_id;
			dev_map[idx].active = false;
			dev_map[idx].copp_idx = -1;
		}
		pr_debug("%s:idx  %d, active %d, dev id 0x%x ref count %d\n",
			 __func__, idx, dev_map[idx].active,
			 dev_map[idx].device_id, dev_map[idx].stream_ref_count);
	}
}

int msm_ds2_dap_set_custom_stereo_onoff(int port_id, int copp_idx,
					bool is_custom_stereo_enabled)
{
	int idx = -1, rc = 0, i;
	pr_debug("%s: port_id %d\n", __func__, port_id);
	if (port_id != DOLBY_INVALID_PORT_ID) {
		for (i = 0; i < DS2_DEVICES_ALL; i++) {
			if ((dev_map[i].port_id == port_id) &&
				/* device part of active device */
				(dev_map[i].device_id &
				ds2_dap_params_states.device)) {
				idx = i;
				if (dev_map[i].device_id == SPEAKER)
					continue;
				else
					break;
			}
		}
		if (idx < 0) {
			pr_err("%s: invalid index for port %d\n",
				__func__, port_id);
			return rc;
		}
		ds2_dap_params_states.custom_stereo_onoff =
			is_custom_stereo_enabled;
		rc = set_custom_stereo_onoff(idx,
					is_custom_stereo_enabled);
		if (rc < 0) {
			pr_err("%s: Custom stereo err %d on port %d\n",
				__func__, rc, port_id);
		}
	}
	return rc;
}

#else

static int msm_ds2_dap_alloc_and_store_cal_data(int dev_map_idx, int path,
					    int perf_mode)
{
	return 0;
}

static int msm_ds2_dap_free_cal_data(int dev_map_idx)
{
	return 0;
}

static int msm_ds2_dap_send_cal_data(int dev_map_idx)
{
	return 0;
}

static int msm_ds2_dap_can_enable_module(int32_t module_id)
{
	return 0;
}

static int msm_ds2_dap_init_modules_in_topology(int dev_map_idx)
{
	return 0;
}

static bool msm_ds2_dap_check_is_param_modified(int32_t *dap_params_modified,
				    int32_t idx, int32_t commit)
{
	return false;
}


static int msm_ds2_dap_map_device_to_dolby_cache_devices(int32_t device_id)
{
	return 0;
}

static int msm_ds2_dap_update_num_devices(struct dolby_param_data *dolby_data,
				      int32_t *num_device, int32_t *dev_arr,
				      int32_t array_size)
{
	return 0;
}

static int msm_ds2_dap_commit_params(struct dolby_param_data *dolby_data,
				 int commit)
{
	return 0;
}

static int msm_ds2_dap_handle_commands(u32 cmd, void *arg)
{
	return 0;
}

static int msm_ds2_dap_set_param(u32 cmd, void *arg)
{
	return 0;
}

static int msm_ds2_dap_get_param(u32 cmd, void *arg)
{
	return 0;
}

static int msm_ds2_dap_send_end_point(int dev_map_idx, int endp_idx)
{
	return 0;
}

static int msm_ds2_dap_send_cached_params(int dev_map_idx,
					  int commit)
{
	return 0;
}

static int msm_ds2_dap_set_vspe_vdhe(int dev_map_idx,
				     bool is_custom_stereo_enabled)
{
	return 0;
}

static int msm_ds2_dap_param_visualizer_control_get(
			u32 cmd, void *arg,
			struct msm_pcm_routing_bdai_data *bedais)
{
	return 0;
}

static int msm_ds2_dap_set_security_control(u32 cmd, void *arg)
{
	return 0
}

static int msm_ds2_dap_update_dev_map_port_id(int32_t device_id, int port_id)
{
	return 0;
}

static int32_t msm_ds2_dap_get_port_id(
		int32_t device_id, int32_t be_id)
{
	return 0;
}

static int msm_ds2_dap_handle_bypass(struct dolby_param_data *dolby_data)
{
	return 0;
}

static int msm_ds2_dap_handle_bypass_wait(int port_id, int copp_idx,
					  int wait_time)
{
	return 0;
}

static int dap_set_custom_stereo_onoff(int dev_map_idx,
					bool is_custom_stereo_enabled)
{
	return 0;
}
int qti_set_custom_stereo_on(int port_id, int copp_idx,
			     bool is_custom_stereo_on)
{
	return 0;
}
int set_custom_stereo_onoff(int dev_map_idx,
			    bool is_custom_stereo_enabled)
{
	return 0;
}
int msm_ds2_dap_ioctl_shared(struct snd_hwdep *hw, struct file *file,
			     u32 cmd, void *arg)
{
	return 0;
}
#endif /*CONFIG_DOLBY_DS2*/
