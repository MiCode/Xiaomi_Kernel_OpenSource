/**
 * Elliptic Labs
 */

#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/jiffies.h>
#include <sound/asound.h>
#include <sound/soc.h>
#include <sound/control.h>
#include "msm-pcm-routing-v2.h"
#include <dsp/q6audio-v2.h>
#include <dsp/apr_audio-v2.h>
#include "msm-elliptic.h"

#include "elliptic/elliptic_data_io.h"
#include "elliptic/elliptic_device.h"
#include "elliptic/elliptic_mixer_controls.h"

struct elliptic_system_configuration {
	union {
		uint8_t reserved[ELLIPTIC_SYSTEM_CONFIGURATION_SIZE];
	};
};

struct elliptic_system_configuration elliptic_system_configuration;

enum elliptic_system_configuration_parameter_type {

	ESCPT_SPEAKER_SCALING = 1,
	ESCPT_CHANNEL_SENSITIVITY,
	ESCPT_LATENCY,
	ESCPT_MICROPHONE_INDEX,
	ESCPT_OPERATION_MODE,
	ESCPT_OPERATION_MODE_FLAGS,
	ESCPT_COMPONENT_GAIN_CHANGE,
	ESCPT_CALIBRATION_STATE,
	ESCPT_ENGINE_VERSION,
	ESCPT_CALIBRATION_PROFILE,
	ESCPT_ULTRASOUND_GAIN,
	ESCPT_LOG_LEVEL,
};

struct elliptic_system_configuration_parameter {
	enum elliptic_system_configuration_parameter_type type;
	union {
		int32_t speaker_scaling[2];
		int32_t sensitivity;
		int32_t latency;
		int32_t microphone_index;
		int32_t operation_mode;
		int32_t operation_mode_flags;
		int32_t component_gain_change;
		int32_t calibration_state;
		int32_t engine_version;
		int32_t calibration_profile;
		int32_t ultrasound_gain;
		int32_t	log_level;
	};
};

struct elliptic_system_configuration_parameters_cache {
	int32_t speaker_scaling[2];
	int32_t sensitivity;
	int32_t latency;
	int32_t microphone_index;
	int32_t operation_mode;
	int32_t operation_mode_flags;
	int32_t component_gain_change;
	int32_t calibration_state;
	int32_t engine_version;
	int32_t calibration_profile;
	int32_t ultrasound_gain;
	int32_t	log_level;
};

struct elliptic_system_configuration_parameters_cache
		elliptic_system_configuration_cache = { {0}, 0 };

static struct elliptic_engine_version_info
		elliptic_engine_version_cache = { 0xde, 0xad, 0xbe, 0xef };

struct elliptic_engine_calibration_data {
	union {
		uint8_t reserved[ELLIPTIC_CALIBRATION_DATA_SIZE];
	};
};

static struct elliptic_engine_calibration_data
	elliptic_engine_calibration_data_cache = { .reserved = {

0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe,
0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad,
0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde,
0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,
0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe,
0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef } };

struct elliptic_engine_branch_info {
	char build_branch[ELLIPTIC_BRANCH_INFO_SIZE];
};

static struct elliptic_engine_branch_info
	elliptic_engine_branch_cache = { { 0 } };

static struct elliptic_shared_data_block shared_data_blocks[] = {
	{ ELLIPTIC_OBJ_ID_CALIBRATION_DATA, ELLIPTIC_CALIBRATION_DATA_SIZE,
		&elliptic_engine_calibration_data_cache },

	{ ELLIPTIC_OBJ_ID_VERSION_INFO,     ELLIPTIC_VERSION_INFO_SIZE,
		&elliptic_engine_version_cache },
	{ ELLIPTIC_OBJ_ID_BRANCH_INFO,      ELLIPTIC_BRANCH_INFO_SIZE,
		&elliptic_engine_branch_cache },
};


static const size_t NUM_SHARED_RW_OBJS =
	sizeof(shared_data_blocks) / sizeof(struct elliptic_shared_data_block);

struct elliptic_shared_data_block *elliptic_get_shared_obj(uint32_t
	object_id) {

	size_t i;

	for (i = 0; i < NUM_SHARED_RW_OBJS; ++i) {
		if (shared_data_blocks[i].object_id == object_id)
			return &shared_data_blocks[i];
	}

	return NULL;
}


static const char * const ultrasound_enable_texts[] = {"Off", "On"};

static const struct soc_enum elliptic_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ultrasound_enable_texts),
	ultrasound_enable_texts),
};

int get_elliptic_calibration_data(uint8_t *caldata, uint32_t max_size)
{
	uint32_t copied = ELLIPTIC_CALIBRATION_DATA_SIZE;

	if (max_size < ELLIPTIC_CALIBRATION_DATA_SIZE) {
		copied = max_size;
		EL_PRINT_D("size mismatch : %d vs %d",
			ELLIPTIC_CALIBRATION_DATA_SIZE, max_size);
	}

	memcpy(caldata, (uint8_t *)&elliptic_engine_calibration_data_cache,
		max_size);
	return copied;
}


static uint32_t ultrasound_enable_cache;

int msm_routing_get_ultrasound_enable(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = ultrasound_enable_cache;
	return 0;
}

int msm_routing_set_ultrasound_enable(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int32_t ret = 0;

	ultrasound_enable_cache = ucontrol->value.integer.value[0];

	msm_pcm_routing_acquire_lock();
	ret = ultrasound_apr_set(ELLIPTIC_PORT_ID, &ultrasound_enable_cache,
		NULL, 0);
	msm_pcm_routing_release_lock();
	return 0;
}

int msm_routing_get_ultrasound_rampdown(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	/* Rampdown is a strobe, so always return Off */
	ucontrol->value.integer.value[0] = 0;
	return 0;
}

int msm_routing_set_ultrasound_rampdown(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int32_t ret = 0;
	uint32_t filter_set = ELLIPTIC_ULTRASOUND_RAMP_DOWN;

	if (ucontrol->value.integer.value[0] == 0)
		return 0;

	msm_pcm_routing_acquire_lock();
	pr_debug("ELUS in %s", __func__);
	ret = ultrasound_apr_set(ELLIPTIC_PORT_ID, &filter_set, NULL, 0);
	msm_pcm_routing_release_lock();
	return 0;
}

int elliptic_system_configuration_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	memcpy(ucontrol->value.bytes.data, &elliptic_system_configuration,
		   ELLIPTIC_SYSTEM_CONFIGURATION_SIZE);
	return 0;
}

int elliptic_system_configuration_put(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int32_t param_id = ELLIPTIC_ULTRASOUND_SET_PARAMS;

	memcpy(&elliptic_system_configuration, ucontrol->value.bytes.data,
		   ELLIPTIC_SYSTEM_CONFIGURATION_SIZE);
	return ultrasound_apr_set(ELLIPTIC_PORT_ID,
				  &param_id,
				  (u8 *)&elliptic_system_configuration,
				  ELLIPTIC_SYSTEM_CONFIGURATION_SIZE);
}

int elliptic_calibration_data_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	memcpy(ucontrol->value.bytes.data,
		&elliptic_engine_calibration_data_cache,
		ELLIPTIC_CALIBRATION_DATA_SIZE);
	return 0;
}

int elliptic_calibration_data_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int32_t param_id = ELLIPTIC_ULTRASOUND_SET_PARAMS;

	memcpy(&elliptic_engine_calibration_data_cache,
		ucontrol->value.bytes.data, ELLIPTIC_CALIBRATION_DATA_SIZE);

	return ultrasound_apr_set(ELLIPTIC_PORT_ID,
				  &param_id,
				  (u8 *)&elliptic_engine_calibration_data_cache,
				  ELLIPTIC_CALIBRATION_DATA_SIZE);
}

int elliptic_version_data_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	memcpy(ucontrol->value.bytes.data, &elliptic_engine_version_cache,
		   ELLIPTIC_VERSION_INFO_SIZE);
	return 0;
}

int elliptic_version_data_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

int elliptic_branch_data_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	memcpy(ucontrol->value.bytes.data, &elliptic_engine_branch_cache,
		   ELLIPTIC_BRANCH_INFO_SIZE);
	return 0;
}

int elliptic_branch_data_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

int elliptic_calibration_param_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	pr_err("%s: reg: %d shift: %d\n", __func__, mc->reg, mc->shift);

	if (mc->reg != ELLIPTIC_CALIBRATION)
		return -EINVAL;

	switch (mc->shift) {
	case ELLIPTIC_CALIBRATION_STATE:
		ucontrol->value.integer.value[0] =
			elliptic_system_configuration_cache.calibration_state;
		break;

	case ELLIPTIC_CALIBRATION_PROFILE:
		ucontrol->value.integer.value[0] =
			elliptic_system_configuration_cache.calibration_profile;
		break;

	case ELLIPTIC_ULTRASOUND_GAIN:
		ucontrol->value.integer.value[0] =
			elliptic_system_configuration_cache.ultrasound_gain;
		break;

	default:
		return -EINVAL;
	}

	return 1;
}

int elliptic_calibration_param_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct elliptic_system_configuration_parameter param;
	uint32_t param_id = ELLIPTIC_ULTRASOUND_SET_PARAMS;

	if (mc->reg != ELLIPTIC_CALIBRATION)
		return -EINVAL;

	switch (mc->shift) {
	case ELLIPTIC_CALIBRATION_STATE:
		elliptic_system_configuration_cache.calibration_state =
			ucontrol->value.integer.value[0];

		param.type = ESCPT_CALIBRATION_STATE;
		param.calibration_state =
			elliptic_system_configuration_cache.calibration_state;
		break;

	case ELLIPTIC_CALIBRATION_PROFILE:
		elliptic_system_configuration_cache.calibration_profile =
			ucontrol->value.integer.value[0];

		param.type = ESCPT_CALIBRATION_PROFILE;
		param.calibration_profile =
			elliptic_system_configuration_cache.calibration_profile;
		break;

	case ELLIPTIC_ULTRASOUND_GAIN:
		elliptic_system_configuration_cache.ultrasound_gain =
			ucontrol->value.integer.value[0];
		param.type = ESCPT_ULTRASOUND_GAIN;
		param.ultrasound_gain =
			elliptic_system_configuration_cache.ultrasound_gain;
		break;

	default:
		return -EINVAL;
	}

	return ultrasound_apr_set(ELLIPTIC_PORT_ID, &param_id,
				  (u8 *)&param, sizeof(param));
}

int elliptic_system_configuration_param_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	pr_err("%s: reg: %d shift: %d\n", __func__, mc->reg, mc->shift);

	if (mc->reg != ELLIPTIC_SYSTEM_CONFIGURATION)
		return -EINVAL;

	switch (mc->shift) {
	case ELLIPTIC_SYSTEM_CONFIGURATION_LATENCY:
		ucontrol->value.integer.value[0] =
			elliptic_system_configuration_cache.latency;
		break;

	case ELLIPTIC_SYSTEM_CONFIGURATION_SENSITIVITY:
		ucontrol->value.integer.value[0] =
			elliptic_system_configuration_cache.sensitivity;
		break;

	case ELLIPTIC_SYSTEM_CONFIGURATION_SPEAKER_SCALING:
		ucontrol->value.integer.value[0] =
			elliptic_system_configuration_cache.speaker_scaling[0];
		ucontrol->value.integer.value[1] =
			elliptic_system_configuration_cache.speaker_scaling[1];
		break;

	case ELLIPTIC_SYSTEM_CONFIGURATION_MICROPHONE_INDEX:
		ucontrol->value.integer.value[0] =
			elliptic_system_configuration_cache.microphone_index;
		break;

	case ELLIPTIC_SYSTEM_CONFIGURATION_OPERATION_MODE:
		ucontrol->value.integer.value[0] =
			elliptic_system_configuration_cache.operation_mode;
		break;

	case ELLIPTIC_SYSTEM_CONFIGURATION_OPERATION_MODE_FLAGS:
		ucontrol->value.integer.value[0] =
		elliptic_system_configuration_cache.operation_mode_flags;
		break;

	case ELLIPTIC_SYSTEM_CONFIGURATION_LOG_LEVEL:
		ucontrol->value.integer.value[0] =
			elliptic_system_configuration_cache.log_level;
		break;

	default:
		return -EINVAL;
	}

	return 1;
}



int elliptic_system_configuration_param_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct elliptic_system_configuration_parameter param;
	uint32_t param_id = ELLIPTIC_ULTRASOUND_SET_PARAMS;

	if (mc->reg != ELLIPTIC_SYSTEM_CONFIGURATION)
		return -EINVAL;

	switch (mc->shift) {
	case ELLIPTIC_SYSTEM_CONFIGURATION_LATENCY:
		elliptic_system_configuration_cache.latency =
			ucontrol->value.integer.value[0];
		param.type = ESCPT_LATENCY;
		param.latency = elliptic_system_configuration_cache.latency;
		break;

	case ELLIPTIC_SYSTEM_CONFIGURATION_SENSITIVITY:
		elliptic_system_configuration_cache.sensitivity =
			ucontrol->value.integer.value[0];
		param.type = ESCPT_CHANNEL_SENSITIVITY;
		param.sensitivity =
			elliptic_system_configuration_cache.sensitivity;
		break;

	case ELLIPTIC_SYSTEM_CONFIGURATION_SPEAKER_SCALING:
		elliptic_system_configuration_cache.speaker_scaling[0] =
			ucontrol->value.integer.value[0];
		elliptic_system_configuration_cache.speaker_scaling[1] =
			ucontrol->value.integer.value[1];
		param.type = ESCPT_SPEAKER_SCALING;
		param.speaker_scaling[0] =
			elliptic_system_configuration_cache.speaker_scaling[0];
		param.speaker_scaling[1] =
			elliptic_system_configuration_cache.speaker_scaling[1];
		break;

	case ELLIPTIC_SYSTEM_CONFIGURATION_MICROPHONE_INDEX:
		elliptic_system_configuration_cache.microphone_index =
			ucontrol->value.integer.value[0];
		param.type = ESCPT_MICROPHONE_INDEX;
		param.microphone_index =
			elliptic_system_configuration_cache.microphone_index;
		break;

	case ELLIPTIC_SYSTEM_CONFIGURATION_OPERATION_MODE:
		elliptic_system_configuration_cache.operation_mode =
			ucontrol->value.integer.value[0];
		param.type = ESCPT_OPERATION_MODE;
		param.operation_mode =
			elliptic_system_configuration_cache.operation_mode;
		break;

	case ELLIPTIC_SYSTEM_CONFIGURATION_OPERATION_MODE_FLAGS:
		elliptic_system_configuration_cache.operation_mode_flags =
			ucontrol->value.integer.value[0];
		param.type = ESCPT_OPERATION_MODE_FLAGS;
		param.operation_mode_flags =
		elliptic_system_configuration_cache.operation_mode_flags;
		break;

	case ELLIPTIC_SYSTEM_CONFIGURATION_LOG_LEVEL:
		elliptic_system_configuration_cache.log_level =
			ucontrol->value.integer.value[0];
		param.type = ESCPT_LOG_LEVEL;
		param.log_level =
		elliptic_system_configuration_cache.log_level;
		break;

	default:
		return -EINVAL;
	}

	return ultrasound_apr_set(ELLIPTIC_PORT_ID, &param_id,
				  (u8 *)&param, sizeof(param));
}


static const struct snd_kcontrol_new ultrasound_filter_mixer_controls[] = {
	SOC_ENUM_EXT("Ultrasound Enable",
	elliptic_enum[0],
	msm_routing_get_ultrasound_enable,
	msm_routing_set_ultrasound_enable),
	SOC_ENUM_EXT("Ultrasound RampDown",
	elliptic_enum[0],
	msm_routing_get_ultrasound_rampdown,
	msm_routing_set_ultrasound_rampdown),
	SOC_SINGLE_EXT("Ultrasound Latency",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_LATENCY,
	10000,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SOC_SINGLE_EXT("Ultrasound Sensitivity",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_SENSITIVITY,
	1000000,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SOC_DOUBLE_EXT("Ultrasound Speaker Scaling",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_SPEAKER_SCALING,
	0,
	1000000,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SOC_SINGLE_EXT("Ultrasound Microphone Index",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_MICROPHONE_INDEX,
	20,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SOC_SINGLE_EXT("Ultrasound Mode",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_OPERATION_MODE,
	255,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SOC_SINGLE_EXT("Ultrasound Mode Flags",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_OPERATION_MODE_FLAGS,
	1,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SOC_SINGLE_EXT("Ultrasound Calibration Profile",
	ELLIPTIC_CALIBRATION,
	ELLIPTIC_CALIBRATION_PROFILE,
	256,
	0,
	elliptic_calibration_param_get,
	elliptic_calibration_param_put),
	SOC_SINGLE_EXT("Ultrasound Gain",
	ELLIPTIC_CALIBRATION,
	ELLIPTIC_ULTRASOUND_GAIN,
	256,
	0,
	elliptic_calibration_param_get,
	elliptic_calibration_param_put),
	SOC_SINGLE_EXT("Ultrasound Calibration State",
	ELLIPTIC_CALIBRATION,
	ELLIPTIC_CALIBRATION_STATE,
	256,
	0,
	elliptic_calibration_param_get,
	elliptic_calibration_param_put),
	SND_SOC_BYTES_EXT("Ultrasound System Configuration",
	ELLIPTIC_SYSTEM_CONFIGURATION_SIZE,
	elliptic_system_configuration_get,
	elliptic_system_configuration_put),
	SND_SOC_BYTES_EXT("Ultrasound Calibration Data",
	ELLIPTIC_CALIBRATION_DATA_SIZE,
	elliptic_calibration_data_get,
	elliptic_calibration_data_put),
	SND_SOC_BYTES_EXT("Ultrasound Version",
	ELLIPTIC_VERSION_INFO_SIZE,
	elliptic_version_data_get,
	elliptic_version_data_put),
	SND_SOC_BYTES_EXT("Ultrasound Branch",
	ELLIPTIC_BRANCH_INFO_SIZE,
	elliptic_branch_data_get,
	elliptic_branch_data_put),
	SOC_SINGLE_EXT("Ultrasound Log Level",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_LOG_LEVEL,
	7,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
};



unsigned int elliptic_add_platform_controls(void *platform)
{
	const unsigned int num_controls =
		ARRAY_SIZE(ultrasound_filter_mixer_controls);

	if (platform != NULL) {
		snd_soc_add_platform_controls(
			(struct snd_soc_platform *)platform,
			ultrasound_filter_mixer_controls,
			num_controls);
	} else {
		pr_debug("[ELUS]: pointer is NULL %s\n", __func__);
	}

	return num_controls;
}
