/* Copyright (c) 2013-2018, The Linux Foundation. All rights reserved.
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
#include <sound/compress_params.h>
#include <sound/devdep_params.h>
#include <dsp/apr_audio-v2.h>
#include <dsp/q6asm-v2.h>
#include <dsp/msm-audio-effects-q6-v2.h>
#include <dsp/q6common.h>

#define MAX_ENABLE_CMD_SIZE 32

#define GET_NEXT(ptr, upper_limit, rc)                                  \
({                                                                      \
	if (((ptr) + 1) > (upper_limit)) {                              \
		pr_err("%s: param list out of boundary\n", __func__);   \
		(rc) = -EINVAL;                                         \
	}                                                               \
	((rc) == 0) ? *(ptr)++ :  -EINVAL;                              \
})

#define CHECK_PARAM_LEN(len, max_len, tag, rc)                          \
do {                                                                    \
	if ((len) > (max_len)) {                                        \
		pr_err("%s: params length overflows\n", (tag));         \
		(rc) = -EINVAL;                                         \
	}                                                               \
} while (0)


/**
 * msm_audio_effects_is_effmodule_supp_in_top -
 *        Checks if given topology and module in effects
 *
 */
bool msm_audio_effects_is_effmodule_supp_in_top(int effect_module,
						int topology)
{
	switch (effect_module) {
	case VIRTUALIZER_MODULE:
	case REVERB_MODULE:
	case BASS_BOOST_MODULE:
	case PBE_MODULE:
	case EQ_MODULE:
		switch (topology) {
		case ASM_STREAM_POSTPROC_TOPO_ID_SA_PLUS:
			return true;
		default:
			return false;
		}
	default:
		return false;
	}
}
EXPORT_SYMBOL(msm_audio_effects_is_effmodule_supp_in_top);

int msm_audio_effects_enable_extn(struct audio_client *ac,
				struct msm_nt_eff_all_config *effects,
				bool flag)
{
	u32 flag_param = flag ? 1 : 0;
	struct param_hdr_v3 param_hdr;
	int rc = 0;

	pr_debug("%s\n", __func__);
	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = AUDPROC_MODULE_ID_VIRTUALIZER;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AUDPROC_PARAM_ID_ENABLE;
	param_hdr.param_size = VIRTUALIZER_ENABLE_PARAM_SZ;
	if (effects->virtualizer.enable_flag)
		rc = q6asm_pack_and_set_pp_param_in_band(ac, param_hdr,
							 (u8 *) &flag_param);

	param_hdr.module_id = AUDPROC_MODULE_ID_BASS_BOOST;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AUDPROC_PARAM_ID_ENABLE;
	param_hdr.param_size = BASS_BOOST_ENABLE_PARAM_SZ;
	if (effects->bass_boost.enable_flag)
		rc = q6asm_pack_and_set_pp_param_in_band(ac, param_hdr,
							 (u8 *) &flag_param);

	param_hdr.module_id = AUDPROC_MODULE_ID_POPLESS_EQUALIZER;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AUDPROC_PARAM_ID_ENABLE;
	param_hdr.param_size = EQ_ENABLE_PARAM_SZ;
	if (effects->equalizer.enable_flag)
		rc = q6asm_pack_and_set_pp_param_in_band(ac, param_hdr,
							 (u8 *) &flag_param);

	return rc;
}

/**
 * msm_audio_effects_virtualizer_handler -
 *        Audio effects handler for virtualizer
 *
 * @ac: audio client handle
 * @pbe: virtualizer params
 * @values: values to be updated
 *
 * Return 0 on success or error on failure
 */
int msm_audio_effects_virtualizer_handler(struct audio_client *ac,
				struct virtualizer_params *virtualizer,
				long *values)
{
	long *param_max_offset = values + MAX_PP_PARAMS_SZ - 1;
	char *params = NULL;
	u8 *updt_params;
	int rc = 0;
	int devices = GET_NEXT(values, param_max_offset, rc);
	int num_commands = GET_NEXT(values, param_max_offset, rc);
	int i, prev_enable_flag;
	uint32_t max_params_length = 0;
	uint32_t params_length = 0;
	struct param_hdr_v3 param_hdr;
	u8 *param_data = NULL;
	u32 packed_data_size = 0;

	pr_debug("%s\n", __func__);
	if (!ac || (devices == -EINVAL) || (num_commands == -EINVAL)) {
		pr_err("%s: cannot set audio effects\n", __func__);
		return -EINVAL;
	}
	params = kzalloc(MAX_INBAND_PARAM_SZ, GFP_KERNEL);
	if (!params) {
		pr_err("%s, params memory alloc failed\n", __func__);
		return -ENOMEM;
	}
	pr_debug("%s: device: %d\n", __func__, devices);
	updt_params = (u8 *) params;
	/* Set MID and IID once at top and only update param specific fields*/
	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = AUDPROC_MODULE_ID_VIRTUALIZER;
	param_hdr.instance_id = INSTANCE_ID_0;
	for (i = 0; i < num_commands; i++) {
		uint32_t command_id =
			GET_NEXT(values, param_max_offset, rc);
		uint32_t command_config_state =
			GET_NEXT(values, param_max_offset, rc);
		uint32_t index_offset =
			GET_NEXT(values, param_max_offset, rc);
		uint32_t length =
			GET_NEXT(values, param_max_offset, rc);
		switch (command_id) {
		case VIRTUALIZER_ENABLE:
			if (length != 1 || index_offset != 0) {
				pr_err("VIRT ENABLE:invalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			prev_enable_flag = virtualizer->enable_flag;
			virtualizer->enable_flag =
				GET_NEXT(values, param_max_offset, rc);
			pr_debug("%s:VIRT ENABLE prev:%d, new:%d\n", __func__,
				prev_enable_flag, virtualizer->enable_flag);
			if (prev_enable_flag == virtualizer->enable_flag)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    VIRTUALIZER_ENABLE_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"VIRT ENABLE", rc);
			if (rc != 0)
				break;
			param_hdr.param_id =
				AUDPROC_PARAM_ID_VIRTUALIZER_ENABLE;
			param_hdr.param_size = VIRTUALIZER_ENABLE_PARAM_SZ;
			param_data = (u8 *) &virtualizer->enable_flag;
			break;
		case VIRTUALIZER_STRENGTH:
			if (length != 1 || index_offset != 0) {
				pr_err("VIRT STRENGTH:invalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			virtualizer->strength =
				GET_NEXT(values, param_max_offset, rc);
			pr_debug("%s: VIRT STRENGTH val: %d\n",
					__func__, virtualizer->strength);
			if (command_config_state != CONFIG_SET)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    VIRTUALIZER_STRENGTH_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"VIRT STRENGTH", rc);
			if (rc != 0)
				break;
			param_hdr.param_id =
				AUDPROC_PARAM_ID_VIRTUALIZER_STRENGTH;
			param_hdr.param_size = VIRTUALIZER_STRENGTH_PARAM_SZ;
			param_data = (u8 *) &virtualizer->strength;
			break;
		case VIRTUALIZER_OUT_TYPE:
			if (length != 1 || index_offset != 0) {
				pr_err("VIRT OUT_TYPE:invalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			virtualizer->out_type =
				GET_NEXT(values, param_max_offset, rc);
			pr_debug("%s: VIRT OUT_TYPE val:%d\n",
				__func__, virtualizer->out_type);
			if (command_config_state != CONFIG_SET)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    VIRTUALIZER_OUT_TYPE_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"VIRT OUT_TYPE", rc);
			if (rc != 0)
				break;
			param_hdr.param_id =
				AUDPROC_PARAM_ID_VIRTUALIZER_OUT_TYPE;
			param_hdr.param_size = VIRTUALIZER_OUT_TYPE_PARAM_SZ;
			param_data = (u8 *) &virtualizer->out_type;
			break;
		case VIRTUALIZER_GAIN_ADJUST:
			if (length != 1 || index_offset != 0) {
				pr_err("VIRT GAIN_ADJUST: invalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			virtualizer->gain_adjust =
				GET_NEXT(values, param_max_offset, rc);
			pr_debug("%s: VIRT GAIN_ADJUST val:%d\n",
				__func__, virtualizer->gain_adjust);
			if (command_config_state != CONFIG_SET)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    VIRTUALIZER_GAIN_ADJUST_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"VIRT GAIN_ADJUST", rc);
			if (rc != 0)
				break;
			param_hdr.param_id =
				AUDPROC_PARAM_ID_VIRTUALIZER_GAIN_ADJUST;
			param_hdr.param_size = VIRTUALIZER_GAIN_ADJUST_PARAM_SZ;
			param_data = (u8 *) &virtualizer->gain_adjust;
			break;
		default:
			pr_err("%s: Invalid command to set config\n", __func__);
			continue;
		}
		if (rc)
			goto invalid_config;

		rc = q6common_pack_pp_params(updt_params, &param_hdr,
					     param_data, &packed_data_size);
		if (rc) {
			pr_err("%s: Failed to pack params, error %d\n",
			       __func__, rc);
			goto invalid_config;
		}

		updt_params += packed_data_size;
		params_length += packed_data_size;
	}
	if (params_length && (rc == 0))
		q6asm_set_pp_params(ac, NULL, params, params_length);
	else
		pr_debug("%s: did not send pp params\n", __func__);
invalid_config:
	kfree(params);
	return rc;
}
EXPORT_SYMBOL(msm_audio_effects_virtualizer_handler);

/**
 * msm_audio_effects_reverb_handler -
 *        Audio effects handler for reverb
 *
 * @ac: audio client handle
 * @pbe: reverb params
 * @values: values to be updated
 *
 * Return 0 on success or error on failure
 */
int msm_audio_effects_reverb_handler(struct audio_client *ac,
				     struct reverb_params *reverb,
				     long *values)
{
	long *param_max_offset = values + MAX_PP_PARAMS_SZ - 1;
	char *params = NULL;
	u8 *updt_params;
	int rc = 0;
	int devices = GET_NEXT(values, param_max_offset, rc);
	int num_commands = GET_NEXT(values, param_max_offset, rc);
	int i, prev_enable_flag;
	uint32_t max_params_length = 0;
	uint32_t params_length = 0;
	struct param_hdr_v3 param_hdr;
	u8 *param_data = NULL;
	u32 packed_data_size = 0;

	pr_debug("%s\n", __func__);
	if (!ac || (devices == -EINVAL) || (num_commands == -EINVAL)) {
		pr_err("%s: cannot set audio effects\n", __func__);
		return -EINVAL;
	}
	params = kzalloc(MAX_INBAND_PARAM_SZ, GFP_KERNEL);
	if (!params) {
		pr_err("%s, params memory alloc failed\n", __func__);
		return -ENOMEM;
	}
	pr_debug("%s: device: %d\n", __func__, devices);
	updt_params = (u8 *) params;
	/* Set MID and IID once at top and only update param specific fields*/
	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = AUDPROC_MODULE_ID_REVERB;
	param_hdr.instance_id = INSTANCE_ID_0;
	for (i = 0; i < num_commands; i++) {
		uint32_t command_id =
			GET_NEXT(values, param_max_offset, rc);
		uint32_t command_config_state =
			GET_NEXT(values, param_max_offset, rc);
		uint32_t index_offset =
			GET_NEXT(values, param_max_offset, rc);
		uint32_t length =
			GET_NEXT(values, param_max_offset, rc);
		switch (command_id) {
		case REVERB_ENABLE:
			if (length != 1 || index_offset != 0) {
				pr_err("REVERB_ENABLE:invalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			prev_enable_flag = reverb->enable_flag;
			reverb->enable_flag =
				GET_NEXT(values, param_max_offset, rc);
			pr_debug("%s:REVERB_ENABLE prev:%d,new:%d\n", __func__,
					prev_enable_flag, reverb->enable_flag);
			if (prev_enable_flag == reverb->enable_flag)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    REVERB_ENABLE_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"REVERB_ENABLE", rc);
			if (rc != 0)
				break;
			param_hdr.param_id = AUDPROC_PARAM_ID_REVERB_ENABLE;
			param_hdr.param_size = REVERB_ENABLE_PARAM_SZ;
			param_data = (u8 *) &reverb->enable_flag;
			break;
		case REVERB_MODE:
			if (length != 1 || index_offset != 0) {
				pr_err("REVERB_MODE:invalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			reverb->mode =
				GET_NEXT(values, param_max_offset, rc);
			pr_debug("%s: REVERB_MODE val:%d\n",
				__func__, reverb->mode);
			if (command_config_state != CONFIG_SET)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    REVERB_MODE_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"REVERB_MODE", rc);
			if (rc != 0)
				break;
			param_hdr.param_id = AUDPROC_PARAM_ID_REVERB_MODE;
			param_hdr.param_size = REVERB_MODE_PARAM_SZ;
			param_data = (u8 *) &reverb->mode;
			break;
		case REVERB_PRESET:
			if (length != 1 || index_offset != 0) {
				pr_err("REVERB_PRESET:invalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			reverb->preset =
				GET_NEXT(values, param_max_offset, rc);
			pr_debug("%s: REVERB_PRESET val:%d\n",
					__func__, reverb->preset);
			if (command_config_state != CONFIG_SET)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    REVERB_PRESET_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"REVERB_PRESET", rc);
			if (rc != 0)
				break;
			param_hdr.param_id = AUDPROC_PARAM_ID_REVERB_PRESET;
			param_hdr.param_size = REVERB_PRESET_PARAM_SZ;
			param_data = (u8 *) &reverb->preset;
			break;
		case REVERB_WET_MIX:
			if (length != 1 || index_offset != 0) {
				pr_err("REVERB_WET_MIX:invalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			reverb->wet_mix =
				GET_NEXT(values, param_max_offset, rc);
			pr_debug("%s: REVERB_WET_MIX val:%d\n",
				__func__, reverb->wet_mix);
			if (command_config_state != CONFIG_SET)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    REVERB_WET_MIX_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"REVERB_WET_MIX", rc);
			if (rc != 0)
				break;
			param_hdr.param_id = AUDPROC_PARAM_ID_REVERB_WET_MIX;
			param_hdr.param_size = REVERB_WET_MIX_PARAM_SZ;
			param_data = (u8 *) &reverb->wet_mix;
			break;
		case REVERB_GAIN_ADJUST:
			if (length != 1 || index_offset != 0) {
				pr_err("REVERB_GAIN_ADJUST:invalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			reverb->gain_adjust =
				GET_NEXT(values, param_max_offset, rc);
			pr_debug("%s: REVERB_GAIN_ADJUST val:%d\n",
					__func__, reverb->gain_adjust);
			if (command_config_state != CONFIG_SET)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    REVERB_GAIN_ADJUST_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"REVERB_GAIN_ADJUST", rc);
			if (rc != 0)
				break;
			param_hdr.param_id =
				AUDPROC_PARAM_ID_REVERB_GAIN_ADJUST;
			param_hdr.param_size = REVERB_GAIN_ADJUST_PARAM_SZ;
			param_data = (u8 *) &reverb->gain_adjust;
			break;
		case REVERB_ROOM_LEVEL:
			if (length != 1 || index_offset != 0) {
				pr_err("REVERB_ROOM_LEVEL:invalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			reverb->room_level =
				GET_NEXT(values, param_max_offset, rc);
			pr_debug("%s: REVERB_ROOM_LEVEL val:%d\n",
				__func__, reverb->room_level);
			if (command_config_state != CONFIG_SET)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    REVERB_ROOM_LEVEL_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"REVERB_ROOM_LEVEL", rc);
			if (rc != 0)
				break;
			param_hdr.param_id = AUDPROC_PARAM_ID_REVERB_ROOM_LEVEL;
			param_hdr.param_size = REVERB_ROOM_LEVEL_PARAM_SZ;
			param_data = (u8 *) &reverb->room_level;
			break;
		case REVERB_ROOM_HF_LEVEL:
			if (length != 1 || index_offset != 0) {
				pr_err("REVERB_ROOM_HF_LEVEL:invalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			reverb->room_hf_level =
				GET_NEXT(values, param_max_offset, rc);
			pr_debug("%s: REVERB_ROOM_HF_LEVEL val%d\n",
				__func__, reverb->room_hf_level);
			if (command_config_state != CONFIG_SET)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    REVERB_ROOM_HF_LEVEL_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"REVERB_ROOM_HF_LEVEL", rc);
			if (rc != 0)
				break;
			param_hdr.param_id =
				AUDPROC_PARAM_ID_REVERB_ROOM_HF_LEVEL;
			param_hdr.param_size = REVERB_ROOM_HF_LEVEL_PARAM_SZ;
			param_data = (u8 *) &reverb->room_hf_level;
			break;
		case REVERB_DECAY_TIME:
			if (length != 1 || index_offset != 0) {
				pr_err("REVERB_DECAY_TIME:invalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			reverb->decay_time =
				GET_NEXT(values, param_max_offset, rc);
			pr_debug("%s: REVERB_DECAY_TIME val:%d\n",
				__func__, reverb->decay_time);
			if (command_config_state != CONFIG_SET)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    REVERB_DECAY_TIME_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"REVERB_DECAY_TIME", rc);
			if (rc != 0)
				break;
			param_hdr.param_id = AUDPROC_PARAM_ID_REVERB_DECAY_TIME;
			param_hdr.param_size = REVERB_DECAY_TIME_PARAM_SZ;
			param_data = (u8 *) &reverb->decay_time;
			break;
		case REVERB_DECAY_HF_RATIO:
			if (length != 1 || index_offset != 0) {
				pr_err("REVERB_DECAY_HF_RATIOinvalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			reverb->decay_hf_ratio =
				GET_NEXT(values, param_max_offset, rc);
			pr_debug("%s: REVERB_DECAY_HF_RATIO val%d\n",
				__func__, reverb->decay_hf_ratio);
			if (command_config_state != CONFIG_SET)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    REVERB_DECAY_HF_RATIO_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"REVERB_DECAY_HF_RATIO", rc);
			if (rc != 0)
				break;
			param_hdr.param_id =
				AUDPROC_PARAM_ID_REVERB_DECAY_HF_RATIO;
			param_hdr.param_size = REVERB_DECAY_HF_RATIO_PARAM_SZ;
			param_data = (u8 *) &reverb->decay_hf_ratio;
			break;
		case REVERB_REFLECTIONS_LEVEL:
			if (length != 1 || index_offset != 0) {
				pr_err("REVERB_REFLECTION_LVLinvalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			reverb->reflections_level =
				GET_NEXT(values, param_max_offset, rc);
			pr_debug("%s: REVERB_REFLECTIONS_LEVEL val:%d\n",
				__func__, reverb->reflections_level);
			if (command_config_state != CONFIG_SET)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    REVERB_REFLECTIONS_LEVEL_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"REVERB_REFLECTIONS_LEVEL", rc);
			if (rc != 0)
				break;
			param_hdr.param_id =
				AUDPROC_PARAM_ID_REVERB_REFLECTIONS_LEVEL;
			param_hdr.param_size =
				REVERB_REFLECTIONS_LEVEL_PARAM_SZ;
			param_data = (u8 *) &reverb->reflections_level;
			break;
		case REVERB_REFLECTIONS_DELAY:
			if (length != 1 || index_offset != 0) {
				pr_err("REVERB_REFLECTION_DLYinvalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			reverb->reflections_delay =
				GET_NEXT(values, param_max_offset, rc);
			pr_debug("%s: REVERB_REFLECTIONS_DELAY val:%d\n",
				__func__, reverb->reflections_delay);
			if (command_config_state != CONFIG_SET)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    REVERB_REFLECTIONS_DELAY_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"REVERB_REFLECTIONS_DELAY", rc);
			if (rc != 0)
				break;
			param_hdr.param_id =
				AUDPROC_PARAM_ID_REVERB_REFLECTIONS_DELAY;
			param_hdr.param_size =
				REVERB_REFLECTIONS_DELAY_PARAM_SZ;
			param_data = (u8 *) &reverb->reflections_delay;
			break;
		case REVERB_LEVEL:
			if (length != 1 || index_offset != 0) {
				pr_err("REVERB_LEVEL:invalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			reverb->level =
				GET_NEXT(values, param_max_offset, rc);
			pr_debug("%s: REVERB_LEVEL val:%d\n",
				__func__, reverb->level);
			if (command_config_state != CONFIG_SET)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    REVERB_LEVEL_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"REVERB_LEVEL", rc);
			if (rc != 0)
				break;
			param_hdr.param_id = AUDPROC_PARAM_ID_REVERB_LEVEL;
			param_hdr.param_size = REVERB_LEVEL_PARAM_SZ;
			param_data = (u8 *) &reverb->level;
			break;
		case REVERB_DELAY:
			if (length != 1 || index_offset != 0) {
				pr_err("REVERB_DELAY:invalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			reverb->delay =
				GET_NEXT(values, param_max_offset, rc);
			pr_debug("%s:REVERB_DELAY val:%d\n",
					__func__, reverb->delay);
			if (command_config_state != CONFIG_SET)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    REVERB_DELAY_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"REVERB_DELAY", rc);
			if (rc != 0)
				break;
			param_hdr.param_id = AUDPROC_PARAM_ID_REVERB_DELAY;
			param_hdr.param_size = REVERB_DELAY_PARAM_SZ;
			param_data = (u8 *) &reverb->delay;
			break;
		case REVERB_DIFFUSION:
			if (length != 1 || index_offset != 0) {
				pr_err("REVERB_DIFFUSION:invalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			reverb->diffusion =
				GET_NEXT(values, param_max_offset, rc);
			pr_debug("%s: REVERB_DIFFUSION val:%d\n",
				__func__, reverb->diffusion);
			if (command_config_state != CONFIG_SET)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    REVERB_DIFFUSION_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"REVERB_DIFFUSION", rc);
			if (rc != 0)
				break;
			param_hdr.param_id = AUDPROC_PARAM_ID_REVERB_DIFFUSION;
			param_hdr.param_size = REVERB_DIFFUSION_PARAM_SZ;
			param_data = (u8 *) &reverb->diffusion;
			break;
		case REVERB_DENSITY:
			if (length != 1 || index_offset != 0) {
				pr_err("REVERB_DENSITY:invalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			reverb->density =
				GET_NEXT(values, param_max_offset, rc);
			pr_debug("%s: REVERB_DENSITY val:%d\n",
				__func__, reverb->density);
			if (command_config_state != CONFIG_SET)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    REVERB_DENSITY_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"REVERB_DENSITY", rc);
			if (rc != 0)
				break;
			param_hdr.param_id = AUDPROC_PARAM_ID_REVERB_DENSITY;
			param_hdr.param_size = REVERB_DENSITY_PARAM_SZ;
			param_data = (u8 *) &reverb->density;
			break;
		default:
			pr_err("%s: Invalid command to set config\n", __func__);
			continue;
		}
		if (rc)
			goto invalid_config;

		rc = q6common_pack_pp_params(updt_params, &param_hdr,
					     param_data, &packed_data_size);
		if (rc) {
			pr_err("%s: Failed to pack params, error %d\n",
			       __func__, rc);
			goto invalid_config;
		}

		updt_params += packed_data_size;
		params_length += packed_data_size;
	}
	if (params_length && (rc == 0))
		q6asm_set_pp_params(ac, NULL, params, params_length);
	else
		pr_debug("%s: did not send pp params\n", __func__);
invalid_config:
	kfree(params);
	return rc;
}
EXPORT_SYMBOL(msm_audio_effects_reverb_handler);

/**
 * msm_audio_effects_bass_boost_handler -
 *        Audio effects handler for bass_boost
 *
 * @ac: audio client handle
 * @bass_boost: bass_boost params
 * @values: values to be updated
 *
 * Return 0 on success or error on failure
 */
int msm_audio_effects_bass_boost_handler(struct audio_client *ac,
					struct bass_boost_params *bass_boost,
					long *values)
{
	long *param_max_offset = values + MAX_PP_PARAMS_SZ - 1;
	char *params = NULL;
	u8 *updt_params;
	int rc = 0;
	int devices = GET_NEXT(values, param_max_offset, rc);
	int num_commands = GET_NEXT(values, param_max_offset, rc);
	int i, prev_enable_flag;
	uint32_t max_params_length = 0;
	uint32_t params_length = 0;
	struct param_hdr_v3 param_hdr;
	u8 *param_data = NULL;
	u32 packed_data_size = 0;

	pr_debug("%s\n", __func__);
	if (!ac || (devices == -EINVAL) || (num_commands == -EINVAL)) {
		pr_err("%s: cannot set audio effects\n", __func__);
		return -EINVAL;
	}
	params = kzalloc(MAX_INBAND_PARAM_SZ, GFP_KERNEL);
	if (!params) {
		pr_err("%s, params memory alloc failed\n", __func__);
		return -ENOMEM;
	}
	pr_debug("%s: device: %d\n", __func__, devices);
	updt_params = (u8 *) params;
	/* Set MID and IID once at top and only update param specific fields*/
	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = AUDPROC_MODULE_ID_BASS_BOOST;
	param_hdr.instance_id = INSTANCE_ID_0;
	for (i = 0; i < num_commands; i++) {
		uint32_t command_id =
			GET_NEXT(values, param_max_offset, rc);
		uint32_t command_config_state =
			GET_NEXT(values, param_max_offset, rc);
		uint32_t index_offset =
			GET_NEXT(values, param_max_offset, rc);
		uint32_t length =
			GET_NEXT(values, param_max_offset, rc);
		switch (command_id) {
		case BASS_BOOST_ENABLE:
			if (length != 1 || index_offset != 0) {
				pr_err("BASS_BOOST_ENABLE:invalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			prev_enable_flag = bass_boost->enable_flag;
			bass_boost->enable_flag =
				GET_NEXT(values, param_max_offset, rc);
			pr_debug("%s: BASS_BOOST_ENABLE prev:%d new:%d\n",
				__func__, prev_enable_flag,
				bass_boost->enable_flag);
			if (prev_enable_flag == bass_boost->enable_flag)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    BASS_BOOST_ENABLE_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"BASS_BOOST_ENABLE", rc);
			if (rc != 0)
				break;
			param_hdr.param_id = AUDPROC_PARAM_ID_BASS_BOOST_ENABLE;
			param_hdr.param_size = BASS_BOOST_ENABLE_PARAM_SZ;
			param_data = (u8 *) &bass_boost->enable_flag;
			break;
		case BASS_BOOST_MODE:
			if (length != 1 || index_offset != 0) {
				pr_err("BASS_BOOST_MODE:invalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			bass_boost->mode =
				GET_NEXT(values, param_max_offset, rc);
			pr_debug("%s: BASS_BOOST_MODE val:%d\n",
				__func__, bass_boost->mode);
			if (command_config_state != CONFIG_SET)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    BASS_BOOST_MODE_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"BASS_BOOST_MODE", rc);
			if (rc != 0)
				break;
			param_hdr.param_id = AUDPROC_PARAM_ID_BASS_BOOST_MODE;
			param_hdr.param_size = BASS_BOOST_MODE_PARAM_SZ;
			param_data = (u8 *) &bass_boost->mode;
			break;
		case BASS_BOOST_STRENGTH:
			if (length != 1 || index_offset != 0) {
				pr_err("BASS_BOOST_STRENGTH:invalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			bass_boost->strength =
				GET_NEXT(values, param_max_offset, rc);
			pr_debug("%s: BASS_BOOST_STRENGTH val:%d\n",
				__func__, bass_boost->strength);
			if (command_config_state != CONFIG_SET)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    BASS_BOOST_STRENGTH_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"BASS_BOOST_STRENGTH", rc);
			if (rc != 0)
				break;
			param_hdr.param_id =
				AUDPROC_PARAM_ID_BASS_BOOST_STRENGTH;
			param_hdr.param_size = BASS_BOOST_STRENGTH_PARAM_SZ;
			param_data = (u8 *) &bass_boost->strength;
			break;
		default:
			pr_err("%s: Invalid command to set config\n", __func__);
			continue;
		}
		if (rc)
			goto invalid_config;

		rc = q6common_pack_pp_params(updt_params, &param_hdr,
					     param_data, &packed_data_size);
		if (rc) {
			pr_err("%s: Failed to pack params, error %d\n",
			       __func__, rc);
			goto invalid_config;
		}

		updt_params += packed_data_size;
		params_length += packed_data_size;
	}
	if (params_length && (rc == 0))
		q6asm_set_pp_params(ac, NULL, params, params_length);
	else
		pr_debug("%s: did not send pp params\n", __func__);
invalid_config:
	kfree(params);
	return rc;
}
EXPORT_SYMBOL(msm_audio_effects_bass_boost_handler);

/**
 * msm_audio_effects_pbe_handler -
 *        Audio effects handler for pbe
 *
 * @ac: audio client handle
 * @pbe: pbe params
 * @values: values to be updated
 *
 * Return 0 on success or error on failure
 */
int msm_audio_effects_pbe_handler(struct audio_client *ac,
					struct pbe_params *pbe,
					long *values)
{
	long *param_max_offset = values + MAX_PP_PARAMS_SZ - 1;
	char *params = NULL;
	u8 *updt_params;
	int rc = 0;
	int devices = GET_NEXT(values, param_max_offset, rc);
	int num_commands = GET_NEXT(values, param_max_offset, rc);
	int i, prev_enable_flag;
	uint32_t max_params_length = 0;
	uint32_t params_length = 0;
	struct param_hdr_v3 param_hdr;
	u8 *param_data = NULL;
	u32 packed_data_size = 0;

	pr_debug("%s\n", __func__);
	if (!ac || (devices == -EINVAL) || (num_commands == -EINVAL)) {
		pr_err("%s: cannot set audio effects\n", __func__);
		return -EINVAL;
	}
	params = kzalloc(MAX_INBAND_PARAM_SZ, GFP_KERNEL);
	if (!params) {
		pr_err("%s, params memory alloc failed\n", __func__);
		return -ENOMEM;
	}
	pr_debug("%s: device: %d\n", __func__, devices);
	updt_params = (u8 *) params;
	/* Set MID and IID once at top and only update param specific fields*/
	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = AUDPROC_MODULE_ID_PBE;
	param_hdr.instance_id = INSTANCE_ID_0;
	for (i = 0; i < num_commands; i++) {
		uint32_t command_id =
			GET_NEXT(values, param_max_offset, rc);
		uint32_t command_config_state =
			GET_NEXT(values, param_max_offset, rc);
		uint32_t index_offset =
			GET_NEXT(values, param_max_offset, rc);
		uint32_t length =
			GET_NEXT(values, param_max_offset, rc);
		switch (command_id) {
		case PBE_ENABLE:
			pr_debug("%s: PBE_ENABLE\n", __func__);
			if (length != 1 || index_offset != 0) {
				pr_err("no valid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			prev_enable_flag = pbe->enable_flag;
			pbe->enable_flag =
				GET_NEXT(values, param_max_offset, rc);
			if (prev_enable_flag == pbe->enable_flag)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    PBE_ENABLE_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"PBE_ENABLE", rc);
			if (rc != 0)
				break;
			param_hdr.param_id = AUDPROC_PARAM_ID_PBE_ENABLE;
			param_hdr.param_size = PBE_ENABLE_PARAM_SZ;
			param_data = (u8 *) &pbe->enable_flag;
			break;
		case PBE_CONFIG:
			pr_debug("%s: PBE_PARAM length %u\n", __func__, length);
			if (length > sizeof(struct pbe_config_t) ||
				length < PBE_CONFIG_PARAM_LEN ||
				index_offset != 0) {
				pr_err("no valid params, len %d\n", length);
				rc = -EINVAL;
				goto invalid_config;
			}
			if (command_config_state != CONFIG_SET)
				break;
			max_params_length =
				params_length + COMMAND_IID_PAYLOAD_SZ + length;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"PBE_PARAM", rc);
			if (rc != 0)
				break;
			param_hdr.param_id = AUDPROC_PARAM_ID_PBE_PARAM_CONFIG;
			param_hdr.param_size = length;
			param_data = (u8 *) values;
			break;
		default:
			pr_err("%s: Invalid command to set config\n", __func__);
			continue;
		}
		if (rc)
			goto invalid_config;

		rc = q6common_pack_pp_params(updt_params, &param_hdr,
					     param_data, &packed_data_size);
		if (rc) {
			pr_err("%s: Failed to pack params, error %d\n",
			       __func__, rc);
			goto invalid_config;
		}

		updt_params += packed_data_size;
		params_length += packed_data_size;
	}
	if (params_length && (rc == 0))
		q6asm_set_pp_params(ac, NULL, params, params_length);
invalid_config:
	kfree(params);
	return rc;
}
EXPORT_SYMBOL(msm_audio_effects_pbe_handler);

/**
 * msm_audio_effects_popless_eq_handler -
 *        Audio effects handler for popless equalizer
 *
 * @ac: audio client handle
 * @eq: equalizer params
 * @values: values to be updated
 *
 * Return 0 on success or error on failure
 */
int msm_audio_effects_popless_eq_handler(struct audio_client *ac,
					 struct eq_params *eq,
					 long *values)
{
	long *param_max_offset = values + MAX_PP_PARAMS_SZ - 1;
	char *params = NULL;
	u8 *updt_params = NULL;
	int rc = 0;
	int devices = GET_NEXT(values, param_max_offset, rc);
	int num_commands = GET_NEXT(values, param_max_offset, rc);
	int i, prev_enable_flag;
	uint32_t max_params_length = 0;
	uint32_t params_length = 0;
	struct param_hdr_v3 param_hdr;
	u8 *param_data = NULL;
	u32 packed_data_size = 0;
	u8 *eq_config_data = NULL;
	u32 *updt_config_data = NULL;
	int config_param_length;

	pr_debug("%s\n", __func__);
	if (!ac || (devices == -EINVAL) || (num_commands == -EINVAL)) {
		pr_err("%s: cannot set audio effects\n", __func__);
		return -EINVAL;
	}
	params = kzalloc(MAX_INBAND_PARAM_SZ, GFP_KERNEL);
	if (!params) {
		pr_err("%s, params memory alloc failed\n", __func__);
		return -ENOMEM;
	}
	pr_debug("%s: device: %d\n", __func__, devices);
	updt_params = (u8 *) params;
	/* Set MID and IID once at top and only update param specific fields*/
	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = AUDPROC_MODULE_ID_POPLESS_EQUALIZER;
	param_hdr.instance_id = INSTANCE_ID_0;
	for (i = 0; i < num_commands; i++) {
		uint32_t command_id =
			GET_NEXT(values, param_max_offset, rc);
		uint32_t command_config_state =
			GET_NEXT(values, param_max_offset, rc);
		uint32_t index_offset =
			GET_NEXT(values, param_max_offset, rc);
		uint32_t length =
			GET_NEXT(values, param_max_offset, rc);
		uint32_t idx;
		int j;

		switch (command_id) {
		case EQ_ENABLE:
			if (length != 1 || index_offset != 0) {
				pr_err("EQ_ENABLE:invalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			prev_enable_flag = eq->enable_flag;
			eq->enable_flag =
				GET_NEXT(values, param_max_offset, rc);
			pr_debug("%s: EQ_ENABLE prev:%d new:%d\n", __func__,
				prev_enable_flag, eq->enable_flag);
			if (prev_enable_flag == eq->enable_flag)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    EQ_ENABLE_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"EQ_ENABLE", rc);
			if (rc != 0)
				break;
			param_hdr.param_id = AUDPROC_PARAM_ID_EQ_ENABLE;
			param_hdr.param_size = EQ_ENABLE_PARAM_SZ;
			param_data = (u8 *) &eq->enable_flag;
			break;
		case EQ_CONFIG:
			if (length < EQ_CONFIG_PARAM_LEN || index_offset != 0) {
				pr_err("EQ_CONFIG:invalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			pr_debug("%s: EQ_CONFIG bands:%d, pgain:%d, pset:%d\n",
				 __func__, eq->config.num_bands,
				eq->config.eq_pregain, eq->config.preset_id);
			for (idx = 0; idx < MAX_EQ_BANDS; idx++)
				eq->per_band_cfg[idx].band_idx = -1;
			eq->config.eq_pregain =
				GET_NEXT(values, param_max_offset, rc);
			eq->config.preset_id =
				GET_NEXT(values, param_max_offset, rc);
			eq->config.num_bands =
				GET_NEXT(values, param_max_offset, rc);
			if (eq->config.num_bands > MAX_EQ_BANDS) {
				pr_err("EQ_CONFIG:invalid num of bands\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			if (eq->config.num_bands &&
			    (((length - EQ_CONFIG_PARAM_LEN)/
				EQ_CONFIG_PER_BAND_PARAM_LEN)
				!= eq->config.num_bands)) {
				pr_err("EQ_CONFIG:invalid length per band\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			for (j = 0; j < eq->config.num_bands; j++) {
				idx = GET_NEXT(values, param_max_offset, rc);
				if (idx >= MAX_EQ_BANDS) {
					pr_err("EQ_CONFIG:invalid band index\n");
					rc = -EINVAL;
					goto invalid_config;
				}
				eq->per_band_cfg[idx].band_idx = idx;
				eq->per_band_cfg[idx].filter_type =
					GET_NEXT(values, param_max_offset, rc);
				eq->per_band_cfg[idx].freq_millihertz =
					GET_NEXT(values, param_max_offset, rc);
				eq->per_band_cfg[idx].gain_millibels =
					GET_NEXT(values, param_max_offset, rc);
				eq->per_band_cfg[idx].quality_factor =
					GET_NEXT(values, param_max_offset, rc);
			}
			if (command_config_state != CONFIG_SET)
				break;
			config_param_length = EQ_CONFIG_PARAM_SZ +
					      (EQ_CONFIG_PER_BAND_PARAM_SZ *
					       eq->config.num_bands);
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    config_param_length;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"EQ_CONFIG", rc);
			if (rc != 0)
				break;
			param_hdr.param_id = AUDPROC_PARAM_ID_EQ_CONFIG;
			param_hdr.param_size = config_param_length;

			if (!eq_config_data)
				eq_config_data = kzalloc(config_param_length,
							 GFP_KERNEL);
			else
				memset(eq_config_data, 0, config_param_length);
			if (!eq_config_data) {
				pr_err("%s, EQ_CONFIG:memory alloc failed\n",
					__func__);
				rc = -ENOMEM;
				goto invalid_config;
			}
			param_data = eq_config_data;
			updt_config_data = (u32 *) eq_config_data;
			*updt_config_data++ = eq->config.eq_pregain;
			*updt_config_data++ = eq->config.preset_id;
			*updt_config_data++ = eq->config.num_bands;
			for (idx = 0; idx < MAX_EQ_BANDS; idx++) {
				if (eq->per_band_cfg[idx].band_idx < 0)
					continue;
				*updt_config_data++ =
					eq->per_band_cfg[idx].filter_type;
				*updt_config_data++ =
					eq->per_band_cfg[idx].freq_millihertz;
				*updt_config_data++ =
					eq->per_band_cfg[idx].gain_millibels;
				*updt_config_data++ =
					eq->per_band_cfg[idx].quality_factor;
				*updt_config_data++ =
					eq->per_band_cfg[idx].band_idx;
			}
			break;
		case EQ_BAND_INDEX:
			if (length != 1 || index_offset != 0) {
				pr_err("EQ_BAND_INDEX:invalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			idx = GET_NEXT(values, param_max_offset, rc);
			if (idx > MAX_EQ_BANDS) {
				pr_err("EQ_BAND_INDEX:invalid band index\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			eq->band_index = idx;
			pr_debug("%s: EQ_BAND_INDEX val:%d\n",
				__func__, eq->band_index);
			if (command_config_state != CONFIG_SET)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    EQ_BAND_INDEX_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"EQ_BAND_INDEX", rc);
			if (rc != 0)
				break;
			param_hdr.param_id = AUDPROC_PARAM_ID_EQ_BAND_INDEX;
			param_hdr.param_size = EQ_BAND_INDEX_PARAM_SZ;
			param_data = (u8 *) &eq->band_index;
			break;
		case EQ_SINGLE_BAND_FREQ:
			if (length != 1 || index_offset != 0) {
				pr_err("EQ_SINGLE_BAND_FREQ:invalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			if (eq->band_index > MAX_EQ_BANDS) {
				pr_err("EQ_SINGLE_BAND_FREQ:invalid index\n");
				break;
			}
			eq->freq_millihertz =
				GET_NEXT(values, param_max_offset, rc);
			pr_debug("%s: EQ_SINGLE_BAND_FREQ idx:%d, val:%d\n",
				__func__, eq->band_index, eq->freq_millihertz);
			if (command_config_state != CONFIG_SET)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    EQ_SINGLE_BAND_FREQ_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"EQ_SINGLE_BAND_FREQ", rc);
			if (rc != 0)
				break;
			param_hdr.param_id =
				AUDPROC_PARAM_ID_EQ_SINGLE_BAND_FREQ;
			param_hdr.param_size = EQ_SINGLE_BAND_FREQ_PARAM_SZ;
			param_data = (u8 *) &eq->freq_millihertz;
			break;
		default:
			pr_err("%s: Invalid command to set config\n", __func__);
			continue;
		}
		if (rc)
			goto invalid_config;

		rc = q6common_pack_pp_params(updt_params, &param_hdr,
					     param_data, &packed_data_size);
		if (rc) {
			pr_err("%s: Failed to pack params, error %d\n",
			       __func__, rc);
			goto invalid_config;
		}

		updt_params += packed_data_size;
		params_length += packed_data_size;
	}
	if (params_length && (rc == 0))
		q6asm_set_pp_params(ac, NULL, params, params_length);
	else
		pr_debug("%s: did not send pp params\n", __func__);
invalid_config:
	kfree(params);
	kfree(eq_config_data);
	return rc;
}
EXPORT_SYMBOL(msm_audio_effects_popless_eq_handler);

static int __msm_audio_effects_volume_handler(struct audio_client *ac,
					      struct soft_volume_params *vol,
					      long *values,
					      int instance)
{
	int devices;
	int num_commands;
	char *params = NULL;
	u8 *updt_params;
	int i;
	uint32_t vol_gain_2ch = 0;
	uint32_t max_params_length = 0;
	uint32_t params_length = 0;
	struct param_hdr_v3 param_hdr;
	u32 packed_data_size = 0;
	long *param_max_offset;
	int rc = 0;

	pr_debug("%s: instance: %d\n", __func__, instance);
	if (!values) {
		pr_err("%s: set audio effects failed, no valid data\n",
			__func__);
		return -EINVAL;
	}
	param_max_offset = values + MAX_PP_PARAMS_SZ - 1;
	devices = GET_NEXT(values, param_max_offset, rc);
	num_commands = GET_NEXT(values, param_max_offset, rc);
	if (!ac || (devices == -EINVAL) || (num_commands == -EINVAL)) {
		pr_err("%s: cannot set audio effects\n", __func__);
		return -EINVAL;
	}
	params = kzalloc(MAX_INBAND_PARAM_SZ, GFP_KERNEL);
	if (!params) {
		pr_err("%s, params memory alloc failed\n", __func__);
		return -ENOMEM;
	}
	updt_params = (u8 *) params;
	/* Set MID and IID once at top and only update param specific fields*/
	memset(&param_hdr, 0, sizeof(param_hdr));
	q6asm_set_soft_volume_module_instance_ids(instance, &param_hdr);
	for (i = 0; i < num_commands; i++) {
		uint32_t command_id =
			GET_NEXT(values, param_max_offset, rc);
		uint32_t command_config_state =
			GET_NEXT(values, param_max_offset, rc);
		uint32_t index_offset =
			GET_NEXT(values, param_max_offset, rc);
		uint32_t length =
			GET_NEXT(values, param_max_offset, rc);
		switch (command_id) {
		case SOFT_VOLUME_GAIN_2CH:
		case SOFT_VOLUME2_GAIN_2CH:
			if (length != 2 || index_offset != 0) {
				pr_err("VOLUME_GAIN_2CH: invalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			vol->left_gain = GET_NEXT(values, param_max_offset, rc);
			vol->right_gain =
				GET_NEXT(values, param_max_offset, rc);
			vol->master_gain = 0x2000;
			if (command_config_state != CONFIG_SET)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    SOFT_VOLUME_GAIN_2CH_PARAM_SZ +
					    COMMAND_IID_PAYLOAD_SZ +
					    SOFT_VOLUME_GAIN_MASTER_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"VOLUME/VOLUME2_GAIN_2CH", rc);
			break;
		case SOFT_VOLUME_GAIN_MASTER:
		case SOFT_VOLUME2_GAIN_MASTER:
			if (length != 1 || index_offset != 0) {
				pr_err("VOLUME_GAIN_MASTER: invalid params\n");
				rc = -EINVAL;
				goto invalid_config;
			}
			vol->left_gain = 0x2000;
			vol->right_gain = 0x2000;
			vol->master_gain =
				GET_NEXT(values, param_max_offset, rc);
			if (command_config_state != CONFIG_SET)
				break;
			max_params_length = params_length +
					    COMMAND_IID_PAYLOAD_SZ +
					    SOFT_VOLUME_GAIN_2CH_PARAM_SZ +
					    COMMAND_IID_PAYLOAD_SZ +
					    SOFT_VOLUME_GAIN_MASTER_PARAM_SZ;
			CHECK_PARAM_LEN(max_params_length, MAX_INBAND_PARAM_SZ,
					"VOLUME/VOLUME2_GAIN_MASTER", rc);
			break;
		default:
			pr_err("%s: Invalid command id: %d to set config\n",
				__func__, command_id);
			continue;
		}
		if (rc)
			continue;

		/* Set Volume Control for Left/Right */
		param_hdr.param_id = ASM_PARAM_ID_VOL_CTRL_LR_CHANNEL_GAIN;
		param_hdr.param_size = SOFT_VOLUME_GAIN_2CH_PARAM_SZ;
		vol_gain_2ch = (vol->left_gain << 16) | vol->right_gain;
		rc = q6common_pack_pp_params(updt_params, &param_hdr,
					     (u8 *) &vol_gain_2ch,
					     &packed_data_size);
		if (rc) {
			pr_err("%s: Failed to pack params, error %d\n",
			       __func__, rc);
			goto invalid_config;
		}

		updt_params += packed_data_size;
		params_length += packed_data_size;

		/* Set Master Volume Control */
		param_hdr.param_id = ASM_PARAM_ID_VOL_CTRL_MASTER_GAIN;
		param_hdr.param_size = SOFT_VOLUME_GAIN_MASTER_PARAM_SZ;
		rc = q6common_pack_pp_params(updt_params, &param_hdr,
					     (u8 *) &vol->master_gain,
					     &packed_data_size);
		if (rc) {
			pr_err("%s: Failed to pack params, error %d\n",
			       __func__, rc);
			goto invalid_config;
		}

		updt_params += packed_data_size;
		params_length += packed_data_size;
	}
	if (params_length && (rc == 0))
		q6asm_set_pp_params(ac, NULL, params, params_length);
invalid_config:
	kfree(params);
	return rc;
}

int msm_audio_effects_volume_handler(struct audio_client *ac,
				     struct soft_volume_params *vol,
				     long *values)
{
	return __msm_audio_effects_volume_handler(ac, vol, values,
						  SOFT_VOLUME_INSTANCE_1);
}

/**
 * msm_audio_effects_volume_handler_v2 -
 *        Audio effects handler for volume
 *
 * @ac: audio client handle
 * @vol: volume params
 * @values: values to be updated
 * @instance: instance to update
 *
 * Return 0 on success or error on failure
 */
int msm_audio_effects_volume_handler_v2(struct audio_client *ac,
					struct soft_volume_params *vol,
					long *values, int instance)
{
	return __msm_audio_effects_volume_handler(ac, vol, values, instance);
}
EXPORT_SYMBOL(msm_audio_effects_volume_handler_v2);
