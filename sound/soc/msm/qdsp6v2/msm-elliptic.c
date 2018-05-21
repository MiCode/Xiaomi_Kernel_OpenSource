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
#include <sound/q6audio-v2.h>
#include <sound/apr_audio-v2.h>
#include "msm-elliptic.h"

extern struct mutex *ptr_routing_lock;
extern atomic_t *ptr_elusAprState;
struct afe_ultrasound_config_command *config = NULL;
static struct afe_ultrasound_get_calib *calib_resp;
extern atomic_t *ptr_elusAprState;
extern void **ptr_apr;
extern atomic_t *ptr_status;
extern atomic_t *ptr_state;
extern wait_queue_head_t *ptr_wait;
extern int afe_timeout_ms;
extern struct afe_ultrasound_calib_get_resp *ptr_ultrasound_calib_data;

int msm_routing_set_ultrasound_enable(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
int msm_routing_get_ultrasound_enable(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
int msm_routing_set_ultrasound_rampdown(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
int elliptic_system_configuration_param_get(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
int elliptic_system_configuration_param_put(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
int elliptic_calibration_param_get(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
int elliptic_calibration_param_put(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
int elliptic_system_configuration_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
int elliptic_system_configuration_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
int elliptic_calibration_data_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
int elliptic_calibration_data_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
int elliptic_version_data_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);
int elliptic_version_data_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);

DECLARE_WAIT_QUEUE_HEAD(ultraSoundAPRWaitQueue);

struct elliptic_system_configuration {
	union {
		uint8_t reserved[ELLIPTIC_SYSTEM_CONFIGURATION_SIZE];
	};
};

struct elliptic_system_configuration elliptic_system_configuration;

enum elliptic_system_configuration_parameter_type {
	ESCPT_OPERATION_MODE,
	ESCPT_SPEAKER_SCALING,
	ESCPT_CHANNEL_SENSITIVITY,
	ESCPT_LATENCY,
	ESCPT_MICROPHONE_INDEX,
	ESCPT_MODE,
	ESCPT_ADAPTIVE_REFERENCE,
	ESCPT_COMPONENT_GAIN_CHANGE,
	ESCPT_CALIBRATION_STATE,
	ESCPT_ENGINE_VERSION,
	ESCPT_CALIBRATION_PROFILE,
	ESCPT_ULTRASOUND_GAIN,
};

struct elliptic_system_configuration_parameter {
	enum elliptic_system_configuration_parameter_type type;
	union {
		int32_t operation_mode;
		int32_t speaker_scaling[2];
		int32_t sensitivity;
		int32_t latency;
		int32_t microphone_index;
		int32_t mode;
		int32_t adaptive_reference;
		int32_t component_gain_change;
		int32_t calibration_state;
		int32_t engine_version;
		int32_t calibration_profile;
		int32_t ultrasound_gain;
	};
};

struct elliptic_system_configuration_parameters_cache {
	int32_t operation_mode;
	int32_t speaker_scaling[2];
	int32_t sensitivity;
	int32_t latency;
	int32_t microphone_index;
	int32_t mode;
	int32_t adaptive_reference;
	int32_t component_gain_change;
	int32_t calibration_state;
	int32_t engine_version;
	int32_t calibration_profile;
	int32_t ultrasound_gain;
};

struct elliptic_system_configuration_parameters_cache
		elliptic_system_configuration_cache = { 0 };

struct elliptic_engine_version_info {
	uint32_t major;
	uint32_t minor;
	uint32_t build;
	uint32_t revision;
};

static struct elliptic_engine_version_info
	elliptic_engine_version_cache = { 0 };

struct elliptic_calibration_data {
	union {
		uint8_t reserved[ELLIPTIC_CALIBRATION_DATA_SIZE];
	};
};

static struct elliptic_calibration_data elliptic_calibration_data;


static const char * const ultrasound_enable_texts[] = {"Off", "On"};

static const struct soc_enum elliptic_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ultrasound_enable_texts),
			    ultrasound_enable_texts),
};

static const struct snd_kcontrol_new ultrasound_filter_mixer_controls[] = {
	SOC_ENUM_EXT("Ultrasound Enable",
		     elliptic_enum[0],
		     msm_routing_get_ultrasound_enable,
		     msm_routing_set_ultrasound_enable),
	SOC_ENUM_EXT("Ultrasound RampDown",
		     elliptic_enum[0],
		     msm_routing_set_ultrasound_rampdown,
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
		       ELLIPTIC_SYSTEM_CONFIGURATION_MODE,
		       255,
		       0,
		       elliptic_system_configuration_param_get,
		       elliptic_system_configuration_param_put),
	SOC_SINGLE_EXT("Ultrasound Adaptive Reference",
		       ELLIPTIC_SYSTEM_CONFIGURATION,
		       ELLIPTIC_SYSTEM_CONFIGURATION_ADAPTIVE_REFS,
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
};

void get_elliptic_engine_version(uint32_t *mjr, uint32_t *min, uint32_t *bld, uint32_t *rev)
{
	*mjr = elliptic_engine_version_cache.major;
	*min = elliptic_engine_version_cache.minor;
	*bld = elliptic_engine_version_cache.build;
	*rev = elliptic_engine_version_cache.revision;
}

int get_elliptic_calibration_data(uint8_t *caldata, uint32_t max_size)
{
	uint32_t copied = ELLIPTIC_CALIBRATION_DATA_SIZE;

	if (max_size < ELLIPTIC_CALIBRATION_DATA_SIZE) {
		copied = max_size;
		pr_debug("[ELUS]: %s get_elliptic_calibration_data mismatch : %d vs %d\n",
			       __func__, ELLIPTIC_CALIBRATION_DATA_SIZE, max_size);
	}
	memcpy(caldata, (uint8_t *)&elliptic_calibration_data, max_size);
	return copied;
}

int32_t ultrasound_open(int32_t port_id, uint32_t *param_id,
			u8 *user_params, int32_t length)
{
	int32_t  ret = 0;
	uint32_t module_id;

	if (port_id == ELLIPTIC_PORT_ID)
		module_id = ELLIPTIC_ULTRASOUND_MODULE_TX;
	else
		module_id = ELLIPTIC_ULTRASOUND_MODULE_RX;

	switch (*param_id) {
	/* Elliptic tinymix controls */
	case ELLIPTIC_ULTRASOUND_ENABLE:
	{
		int32_t array[4] = {1, 0, 0, 0};

		ret = afe_ultrasound_set_calib_data(port_id,
			*param_id, module_id,
			(struct afe_ultrasound_set_params_t *)array,
			ELLIPTIC_ENABLE_APR_SIZE);
	}
	break;
	case ELLIPTIC_ULTRASOUND_DISABLE:
	{
		int32_t array[4] = {0, 0, 0, 0};

		ret = afe_ultrasound_set_calib_data(port_id,
			*param_id, module_id,
			(struct afe_ultrasound_set_params_t *)array,
			ELLIPTIC_ENABLE_APR_SIZE);
	}
	break;
	case ELLIPTIC_ULTRASOUND_RAMP_DOWN:
	{
		int32_t array[4] = {-1, 0, 0, 0};

		ret = afe_ultrasound_set_calib_data(port_id,
			*param_id, module_id,
			(struct afe_ultrasound_set_params_t *)array,
			ELLIPTIC_ENABLE_APR_SIZE);
	}
	break;
	case ELLIPTIC_ULTRASOUND_GET_PARAMS:
	{
		pr_debug("[ELUS]: inside get param %s\n", __func__);
		if (calib_resp == NULL) {
			calib_resp =
				kzalloc(sizeof(struct afe_ultrasound_get_calib),
					GFP_KERNEL);
			if (calib_resp == NULL) {
				ret = -ENOMEM;
				break;
			}
		}
		if (user_params == NULL) {
			ret = -EFAULT;
			break;
		}
		ret = afe_ultrasound_get_calib_data(calib_resp,
						    *param_id, module_id);
		if (ret == 0) {
			pr_debug("[ELUS]: get_calib_data returns 0, %s\n",
				  __func__);
			if (length > sizeof(struct afe_ultrasound_calib_get_resp))
				length = sizeof(struct afe_ultrasound_calib_get_resp);
			memcpy(user_params, &calib_resp->res_cfg.payload[0],
			       length);
			ret = length;
			pr_debug("[ELUS]: ret size = %d", ret);
		} else {
			pr_err("[ELUS]: %s get_calib_data failed : %d",
			       __func__, ret);
			ret = 0;
		}
		break;
	}
	case ELLIPTIC_ULTRASOUND_SET_PARAMS:
	{
		afe_ultrasound_set_calib_data(port_id,
			*param_id, module_id,
			(struct afe_ultrasound_set_params_t *)user_params,
			length);
		break;
	}
	default:
		goto fail_cmd;
	}

fail_cmd:
	return ret;
}

int elliptic_data_io(uint32_t filter_set, uint32_t elliptic_port_id,
		     char *buff, size_t length)
{
	int32_t retSize = 0;

	retSize = ultrasound_open(ELLIPTIC_PORT_ID, &filter_set, buff, length);
	return retSize;
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

	mutex_lock(ptr_routing_lock);
	ret = ultrasound_open(ELLIPTIC_PORT_ID, &ultrasound_enable_cache, NULL, 0);
	mutex_unlock(ptr_routing_lock);
	return 0;
}

int msm_routing_set_ultrasound_rampdown(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int32_t ret = 0;
	uint32_t filter_set = ELLIPTIC_ULTRASOUND_RAMP_DOWN;

	mutex_lock(ptr_routing_lock);
	pr_debug("ELUS in %s", __func__);
	ret = ultrasound_open(ELLIPTIC_PORT_ID, &filter_set, NULL, 0);
	mutex_unlock(ptr_routing_lock);
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
	return ultrasound_open(ELLIPTIC_PORT_ID,
			       &param_id,
			       (u8 *)&elliptic_system_configuration,
			       ELLIPTIC_SYSTEM_CONFIGURATION_SIZE);
}

int elliptic_calibration_data_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	memcpy(ucontrol->value.bytes.data, &elliptic_calibration_data,
	       ELLIPTIC_CALIBRATION_DATA_SIZE);
	return 0;
}

int elliptic_calibration_data_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int32_t param_id = ELLIPTIC_ULTRASOUND_SET_PARAMS;

	memcpy(&elliptic_calibration_data, ucontrol->value.bytes.data,
	       ELLIPTIC_CALIBRATION_DATA_SIZE);
	return ultrasound_open(ELLIPTIC_PORT_ID,
			       &param_id,
			       (u8 *)&elliptic_calibration_data,
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

int elliptic_calibration_param_get(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	pr_err("%s: \n", __func__);

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
		elliptic_system_configuration_cache.calibration_state = ucontrol->value.integer.value[0];
		param.type = ESCPT_CALIBRATION_STATE;
		param.calibration_state =
			elliptic_system_configuration_cache.calibration_state;
		break;
	case ELLIPTIC_CALIBRATION_PROFILE:
		elliptic_system_configuration_cache.calibration_profile = ucontrol->value.integer.value[0];
		param.type = ESCPT_CALIBRATION_PROFILE;
		param.calibration_profile =
			elliptic_system_configuration_cache.calibration_profile;
		break;
	case ELLIPTIC_ULTRASOUND_GAIN:
		elliptic_system_configuration_cache.ultrasound_gain = ucontrol->value.integer.value[0];
		param.type = ESCPT_ULTRASOUND_GAIN;
		param.ultrasound_gain =
			elliptic_system_configuration_cache.ultrasound_gain;
		break;
	default:
		return -EINVAL;
	}

	return ultrasound_open(ELLIPTIC_PORT_ID, &param_id,
			       (u8 *)&param, sizeof(param));
}

int elliptic_system_configuration_param_get(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	pr_err("%s: \n", __func__);

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
	case ELLIPTIC_SYSTEM_CONFIGURATION_MODE:
		ucontrol->value.integer.value[0] =
			elliptic_system_configuration_cache.mode;
		break;
	case ELLIPTIC_SYSTEM_CONFIGURATION_ADAPTIVE_REFS:
		ucontrol->value.integer.value[0] =
			elliptic_system_configuration_cache.adaptive_reference;
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
	case ELLIPTIC_SYSTEM_CONFIGURATION_MODE:
		elliptic_system_configuration_cache.mode =
			ucontrol->value.integer.value[0];
		param.type = ESCPT_MODE;
		param.mode = elliptic_system_configuration_cache.mode;
		break;
	case ELLIPTIC_SYSTEM_CONFIGURATION_ADAPTIVE_REFS:
		elliptic_system_configuration_cache.adaptive_reference =
			ucontrol->value.integer.value[0];
		param.type = ESCPT_ADAPTIVE_REFERENCE;
		param.adaptive_reference =
			elliptic_system_configuration_cache.adaptive_reference;
		break;
	default:
		return -EINVAL;
	}

	return ultrasound_open(ELLIPTIC_PORT_ID, &param_id,
			       (u8 *)&param, sizeof(param));
}

int elliptic_data_io_cancel(void)
{
	atomic_set(ptr_elusAprState, ELLIPTIC_DATA_READ_CANCEL);
	wake_up_interruptible(&ultraSoundAPRWaitQueue);
	return 0;
}

int afe_ultrasound_set_calib_data(int port,
		int param_id,
		int module_id,
		struct afe_ultrasound_set_params_t *prot_config,
		uint32_t length)
{
	int ret = -EINVAL;
	int index = 0;
	struct afe_ultrasound_config_command configV;
	struct afe_ultrasound_config_command *config;
	config = &configV;
	pr_debug("[ELUS]: inside %s\n", __func__);
	memset(config, 0 , sizeof(struct afe_ultrasound_config_command));
	if (!prot_config) {
		pr_err("%s Invalid params\n", __func__);
		goto fail_cmd;
	}
	if ((q6audio_validate_port(port) < 0)) {
		pr_err("%s invalid port %d\n", __func__, port);
		goto fail_cmd;
	}
	index = q6audio_get_port_index(port);
	config->pdata.module_id = module_id;
	config->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					      APR_HDR_LEN(APR_HDR_SIZE),
					      APR_PKT_VER);
	config->hdr.pkt_size = sizeof(struct afe_ultrasound_config_command);
	config->hdr.src_port = 0;
	config->hdr.dest_port = 0;
	config->hdr.token = index;
	config->hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;
	config->param.port_id = q6audio_get_port_id(port);
	config->param.payload_size =
			sizeof(struct afe_ultrasound_config_command) -
			sizeof(config->hdr) - sizeof(config->param);
	config->pdata.param_id = param_id;
	config->pdata.param_size = length;
	pr_debug("[ELUS]: param_size %d\n", length);
	memcpy(config->prot_config.payload, prot_config,
	       sizeof(struct afe_ultrasound_set_params_t));
	atomic_set(ptr_state, 1);
	ret = apr_send_pkt(*ptr_apr, (uint32_t *) config);
	if (ret < 0) {
		pr_err("%s: Setting param for port %d param[0x%x]failed\n",
		       __func__, port, param_id);
		goto fail_cmd;
	}
	ret = wait_event_timeout(ptr_wait[index],
		(atomic_read(ptr_state) == 0),
		msecs_to_jiffies(afe_timeout_ms*10));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
	if (atomic_read(ptr_status) != 0) {
		pr_err("%s: config cmd failed\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
	ret = 0;
fail_cmd:
	pr_debug("%s config->pdata.param_id %x status %d\n",
	__func__, config->pdata.param_id, ret);
	return ret;
}

int afe_ultrasound_get_calib_data(struct afe_ultrasound_get_calib *calib_resp,
				  uint32_t param_id, uint32_t module_id)
{
	int ret;
	int elusState = atomic_read(ptr_elusAprState);
	if ((elusState == ELLIPTIC_DATA_READ_OK) ||
	    (elusState == ELLIPTIC_DATA_READ_CANCEL)) {
		if (ELLIPTIC_DATA_READ_CANCEL == elusState) {
			atomic_set(ptr_elusAprState,
				   ELLIPTIC_DATA_READ_BUSY);
			return -ECANCELED;
		}
		memcpy(&calib_resp->res_cfg,
		       &ptr_ultrasound_calib_data->res_cfg,
		       sizeof(struct afe_ultrasound_calib_get_resp));
		atomic_set(ptr_elusAprState, ELLIPTIC_DATA_READ_BUSY);
		return 0;
	} else {
		pr_debug("[ELUS]: Entering wait state #%d",
			 atomic_read(ptr_elusAprState));
		ret = wait_event_interruptible(ultraSoundAPRWaitQueue,
		      atomic_read(ptr_elusAprState) != ELLIPTIC_DATA_READ_BUSY);
		if (0 == ret) {
			/* Wakeup */
			if (ELLIPTIC_DATA_READ_CANCEL == atomic_read(ptr_elusAprState))
				return -ECANCELED;

			memcpy(&calib_resp->res_cfg,
			       &ptr_ultrasound_calib_data->res_cfg,
			       sizeof(struct afe_ultrasound_calib_get_resp));

			atomic_set(ptr_elusAprState, ELLIPTIC_DATA_READ_BUSY);
			pr_debug("[ELUS]: %d %d %d %d\ninterrupted wait sig[0:1] = 0x%08x 0x%08x\n",
				 (unsigned int)current->pending.signal.sig[0],
				 (unsigned int)current->pending.signal.sig[1],
				 calib_resp->res_cfg.payload[0],
				 calib_resp->res_cfg.payload[1],
				 calib_resp->res_cfg.payload[2],
				 calib_resp->res_cfg.payload[3]);
		} else if (-ERESTARTSYS == ret) {
			/* Interrupted */
			pr_debug("[ELUS] interrupted wait sig[0:1] = 0x%08x 0x%08x #%d\n",
				 (unsigned int)current->pending.signal.sig[0],
				 (unsigned int)current->pending.signal.sig[1],
				 atomic_read(ptr_elusAprState));
		} else {
			pr_debug("[ELUS] wait failed %d\n", ret);
		}
	}
	return ret;
}

unsigned int elliptic_add_platform_controls(void *platform)
{
	const unsigned int num_controls =
		ARRAY_SIZE(ultrasound_filter_mixer_controls);

	if (platform != NULL) {
		snd_soc_add_platform_controls((struct snd_soc_platform *)platform,
				ultrasound_filter_mixer_controls,
				num_controls);
	} else {
		pr_debug("[ELUS]: pointer is NULL %s\n", __func__);
	}

	return num_controls;
}

int32_t process_us_payload(uint32_t *payload)
{
	uint32_t payload_size = 0;
	int32_t  ret = -1;

	if (payload[0] == ELLIPTIC_ULTRASOUND_MODULE_TX) {
		/* payload format
		   payload[0] = Module ID
		   payload[1] = Param ID
		   payload[2] = LSB - payload size
				MSB - reserved(TBD)
		   payload[3] = US data payload starts from here */
		payload_size = payload[2] & 0xFFFF;
		/* pr_debug("[ELUS]: playload type=%d size = %d, data 0x%x 0x%x 0x%x ...\n",
				 payload[1], payload_size, payload[3], payload[4], payload[5]);*/
		switch (payload[1]) {
		case ELLIPTIC_ULTRASOUND_PARAM_ID_ENGINE_VERSION:
			if (payload_size >= ELLIPTIC_VERSION_INFO_SIZE) {
				pr_debug("[ELUS]: elliptic_version copied to local AP cache");
				memcpy((u8 *)&elliptic_engine_version_cache, &payload[3], ELLIPTIC_VERSION_INFO_SIZE);
				ret = ELLIPTIC_VERSION_INFO_SIZE;
			}
			break;
		case ELLIPTIC_ULTRASOUND_PARAM_ID_CALIBRATION_DATA:
			if (payload_size >= ELLIPTIC_CALIBRATION_DATA_SIZE) {
				pr_debug("[ELUS]: calibration_data copied to local AP cache");
				memcpy((u8 *)&elliptic_calibration_data, &payload[3], ELLIPTIC_CALIBRATION_DATA_SIZE);
				ret = ELLIPTIC_CALIBRATION_DATA_SIZE;
			}
			break;
		case ELLIPTIC_ULTRASOUND_PARAM_ID_UPS_DATA:
		default:
			if (payload_size <= sizeof(struct afe_ultrasound_calib_get_resp)) {
				/* pr_debug("[ELUS]: data is copied to ultrasound_calib_data struct"); */
				memcpy(ptr_ultrasound_calib_data, &payload[3], payload_size);
				atomic_set(ptr_elusAprState, ELLIPTIC_DATA_READ_OK);
				wake_up_interruptible(&ultraSoundAPRWaitQueue);
				ret = payload_size;
				elliptic_keep_sensor_system_awake();
			}
			break;
		}
	} else {
		pr_debug("[ELUS]: Invalid Ultrasound Module ID %d\n", payload[0]);
	}
	return ret;
}
