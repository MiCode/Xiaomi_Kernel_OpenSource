/**
 * Elliptic Labs
 */

#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <sound/asound.h>
#include <sound/soc.h>
#include <sound/control.h>

#include <elliptic/elliptic_mixer_controls.h>
#include <elliptic/elliptic_data_io.h>
#include <elliptic/elliptic_device.h>

struct elliptic_system_configuration {
	union {
		uint8_t reserved[ELLIPTIC_SYSTEM_CONFIGURATION_SIZE];
	};
};

struct elliptic_system_configuration elliptic_system_configuration;


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
		int32_t custom_setting;
		int32_t engine_suspend;
		int32_t input_enabled;
		int32_t output_enabled;
		int32_t external_event;
		struct {
			int32_t calibration_method;
			int32_t calibration_timestamp;
		};
		int32_t debug_mode;
		int32_t context;
		int32_t capture;
		int32_t input_channels;
	};
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

struct elliptic_engine_calibration_v2_data {
	union {
		uint8_t reserved[ELLIPTIC_CALIBRATION_V2_DATA_SIZE];
	};
};

static struct elliptic_engine_calibration_v2_data
	elliptic_engine_calibration_v2_data_cache = { .reserved = {

0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe,
0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad,
0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde,
0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,
0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe,
0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,

0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe,
0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad,
0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde,
0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,
0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe,
0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,

0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe,
0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad,
0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde,
0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,
0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe,
0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,

0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe,
0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad,
0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde,
0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,
0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe,
0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,

0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe,
0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad,
0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde,
0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,
0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe,
0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,

0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe,
0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad,
0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde,
0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,
0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe,
0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,

0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe,
0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad,
0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde,
0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,
0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe,
0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,
} };

struct elliptic_engine_diagnostics_data {
	union {
		uint8_t reserved[ELLIPTIC_DIAGNOSTICS_DATA_SIZE];
		uint32_t values32[ELLIPTIC_DIAGNOSTICS_DATA_SIZE >> 2];
	};
};

static struct elliptic_engine_diagnostics_data
	elliptic_engine_diagnostics_data_cache = { .reserved = {0} };

struct elliptic_engine_ml_data {
	union {
		uint8_t reserved[ELLIPTIC_ML_DATA_SIZE];
		uint32_t u32[ELLIPTIC_ML_DATA_SIZE >> 2];
	};
};

static struct elliptic_engine_ml_data
	elliptic_engine_ml_data_cache
		= { .reserved = {0} };

struct elliptic_engine_sensor_data {
	union {
		uint8_t reserved[ELLIPTIC_SENSOR_DATA_SIZE];
		uint32_t values32[ELLIPTIC_SENSOR_DATA_SIZE >> 2];
	};
};

static struct elliptic_engine_sensor_data
	elliptic_engine_sensor_data_cache = { .reserved = {0} };

struct elliptic_engine_branch_info {
	char build_branch[ELLIPTIC_BRANCH_INFO_MAX_SIZE];
};

static struct elliptic_engine_branch_info
	elliptic_engine_branch_cache = { { 0 } };

struct elliptic_engine_tag_info {
	char engine_tag[ELLIPTIC_TAG_INFO_SIZE];
};

static struct elliptic_engine_tag_info
	elliptic_engine_tag_cache = { { 0 } };

static struct elliptic_shared_data_block shared_data_blocks[] = {
	{ ELLIPTIC_OBJ_ID_CALIBRATION_DATA, ELLIPTIC_CALIBRATION_DATA_SIZE,
		&elliptic_engine_calibration_data_cache },

	{ ELLIPTIC_OBJ_ID_VERSION_INFO, ELLIPTIC_VERSION_INFO_SIZE,
		&elliptic_engine_version_cache },
	{ ELLIPTIC_OBJ_ID_BRANCH_INFO, ELLIPTIC_BRANCH_INFO_MAX_SIZE,
		&elliptic_engine_branch_cache },
	{ ELLIPTIC_OBJ_ID_CALIBRATION_V2_DATA,
		ELLIPTIC_CALIBRATION_V2_DATA_SIZE,
		&elliptic_engine_calibration_v2_data_cache },
	{ ELLIPTIC_OBJ_ID_DIAGNOSTICS_DATA, ELLIPTIC_DIAGNOSTICS_DATA_SIZE,
		&elliptic_engine_diagnostics_data_cache },
	{ ELLIPTIC_OBJ_ID_TAG_INFO, ELLIPTIC_TAG_INFO_SIZE,
		&elliptic_engine_tag_cache },
	{ ELLIPTIC_OBJ_ID_ML_DATA,
		ELLIPTIC_ML_DATA_SIZE,
		&elliptic_engine_ml_data_cache },
};

void elliptic_set_calibration_data(uint8_t *calib_data, size_t size)
{
	struct elliptic_shared_data_block *calibration_obj = NULL;

	if (size == ELLIPTIC_CALIBRATION_DATA_SIZE) {
		calibration_obj = elliptic_get_shared_obj(
			ELLIPTIC_OBJ_ID_CALIBRATION_DATA);
		memcpy((uint8_t *)&elliptic_engine_calibration_data_cache,
			calib_data, size);
	}
	if (size == ELLIPTIC_CALIBRATION_V2_DATA_SIZE) {
		calibration_obj = elliptic_get_shared_obj(
				ELLIPTIC_OBJ_ID_CALIBRATION_V2_DATA);
		memcpy((uint8_t *)&elliptic_engine_calibration_v2_data_cache,
			calib_data, size);
	}
	if (calibration_obj == NULL) {
		EL_PRINT_E(
			"ell..set_calibration_data() calib=NULL (%zu)", size);
		return;
	}
	memcpy((u8 *)calibration_obj->buffer, calib_data, size);
}

void elliptic_set_diagnostics_data(uint8_t *diag_data, size_t size)
{
	struct elliptic_shared_data_block *diagnostics_obj = NULL;

	if (size <= ELLIPTIC_DIAGNOSTICS_DATA_SIZE) {
		diagnostics_obj =
			elliptic_get_shared_obj(
				ELLIPTIC_OBJ_ID_DIAGNOSTICS_DATA);
		if (diagnostics_obj == NULL) {
			EL_PRINT_E("el..set_diagnostics_data() NULL (%zu)",
				size);
			return;
		}
		memcpy((uint8_t *)&elliptic_engine_diagnostics_data_cache,
			diag_data, size);
		memcpy((u8 *)diagnostics_obj->buffer, diag_data, size);
	}
}

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
	uint32_t copied;

	copied = ELLIPTIC_CALIBRATION_DATA_SIZE;
	if (max_size < ELLIPTIC_CALIBRATION_DATA_SIZE) {
		copied = max_size;
		EL_PRINT_D("size mismatch : %u vs %u",
			(uint32_t)ELLIPTIC_CALIBRATION_DATA_SIZE, max_size);
	}

	memcpy(caldata, (uint8_t *)&elliptic_engine_calibration_data_cache,
		max_size);
	return copied;
}

int get_elliptic_calibration_v2_data(uint8_t *caldata, uint32_t max_size)
{
	uint32_t copied;

	copied = ELLIPTIC_CALIBRATION_V2_DATA_SIZE;
	if (max_size < ELLIPTIC_CALIBRATION_V2_DATA_SIZE) {
		copied = max_size;
		EL_PRINT_D("size mismatch : %u vs %u",
			(uint32_t)ELLIPTIC_CALIBRATION_V2_DATA_SIZE, max_size);
	}

	memcpy(caldata, (uint8_t *)&elliptic_engine_calibration_v2_data_cache,
		max_size);
	return copied;
}

int get_elliptic_diagnostics_data(uint8_t *diagdata, uint32_t max_size)
{
	uint32_t copied;

	copied = ELLIPTIC_DIAGNOSTICS_DATA_SIZE;
	if (max_size < ELLIPTIC_DIAGNOSTICS_DATA_SIZE) {
		copied = max_size;
		EL_PRINT_D("size mismatch : %u vs %u",
			(uint32_t)ELLIPTIC_DIAGNOSTICS_DATA_SIZE, max_size);
	}

	memcpy(diagdata, (uint8_t *)&elliptic_engine_diagnostics_data_cache,
		max_size);
	return copied;
}


static uint32_t ultrasound_enable_cache;

int elliptic_ultrasound_enable_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = ultrasound_enable_cache;
	return 0;
}

int elliptic_ultrasound_enable_set(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	static bool triggered_engine_info;
	int32_t msg[4] = {0, 0, 0, 0};

	ultrasound_enable_cache = ucontrol->value.integer.value[0];

	if (!triggered_engine_info && ultrasound_enable_cache) {
		triggered_engine_info = true;
		elliptic_trigger_version_msg();
		elliptic_trigger_branch_msg();
		elliptic_trigger_tag_msg();
	}

	msg[0] = ultrasound_enable_cache ? 1 : 0;

	return elliptic_data_write(
		ELLIPTIC_ULTRASOUND_SET_PARAMS,
		(const char *)msg, sizeof(msg));
}

static uint32_t ultrasound_tx_port_cache;

int elliptic_ultrasound_tx_port_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = ultrasound_tx_port_cache;
	return 0;
}

int elliptic_ultrasound_tx_port_set(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int ret;

	if (ultrasound_tx_port_cache == ucontrol->value.integer.value[0]) {
		EL_PRINT_E("ultrasound_tx_port_set: ignoring duplicate request");
		return 0;
	}

	ultrasound_tx_port_cache = ucontrol->value.integer.value[0];
	if (ultrasound_tx_port_cache)
		ret = elliptic_open_port(ULTRASOUND_TX_PORT_ID);
	else
		ret = elliptic_close_port(ULTRASOUND_TX_PORT_ID);

	EL_PRINT_E("ultrasound_tx_port: enable=%d ret=%d",
		ultrasound_tx_port_cache, ret);

	return ret;
}

static uint32_t ultrasound_rx_port_cache;

int elliptic_ultrasound_rx_port_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = ultrasound_rx_port_cache;
	return 0;
}

int elliptic_ultrasound_rx_port_set(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int ret;

	if (ultrasound_rx_port_cache == ucontrol->value.integer.value[0]) {
		EL_PRINT_E("ultrasound_rx_port_set: ignoring duplicate request");
		return 0;
	}

	ultrasound_rx_port_cache = ucontrol->value.integer.value[0];
	if (ultrasound_rx_port_cache)
		ret = elliptic_open_port(ULTRASOUND_RX_PORT_ID);
	else
		ret = elliptic_close_port(ULTRASOUND_RX_PORT_ID);

	EL_PRINT_E("ultrasound_rx_port: enable=%d ret=%d",
		ultrasound_tx_port_cache, ret);

	return 0;
}

int elliptic_ultrasound_rampdown_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	/* Rampdown is a strobe, so always return Off */
	ucontrol->value.integer.value[0] = 0;
	return 0;
}

int elliptic_ultrasound_rampdown_set(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int32_t msg[4] = {-1, 0, 0, 0};

	if (ucontrol->value.integer.value[0] == 0)
		return 0;

	return elliptic_data_write(ELLIPTIC_ULTRASOUND_SET_PARAMS,
		(const char *)msg, sizeof(msg));
}

int elliptic_ultrasound_diagnostics_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	/* Diagnostics is a strobe, so always return Off */
	ucontrol->value.integer.value[0] = 0;
	return 0;
}

int elliptic_ultrasound_request_diagnostics(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int32_t msg[3] = {ESCPT_ENGINE_DIAGNOSTICS, 0, 0};

	if (ucontrol->value.integer.value[0] == 0)
		return 0;

	return elliptic_data_write(ELLIPTIC_ULTRASOUND_SET_PARAMS,
				(const char *)msg, sizeof(msg));
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
	memcpy(&elliptic_system_configuration, ucontrol->value.bytes.data,
		   ELLIPTIC_SYSTEM_CONFIGURATION_SIZE);

	return elliptic_data_write(ELLIPTIC_ULTRASOUND_SET_PARAMS,
				  (const char *)&elliptic_system_configuration,
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
	memcpy(&elliptic_engine_calibration_data_cache,
		ucontrol->value.bytes.data, ELLIPTIC_CALIBRATION_DATA_SIZE);

	return elliptic_data_write(ELLIPTIC_ULTRASOUND_SET_PARAMS,
			(const char *)&elliptic_engine_calibration_data_cache,
			ELLIPTIC_CALIBRATION_DATA_SIZE);
}

int elliptic_calibration_v2_data_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct elliptic_shared_data_block *calibration_obj =
		elliptic_get_shared_obj(ELLIPTIC_OBJ_ID_CALIBRATION_V2_DATA);

	if (calibration_obj == NULL) {
		EL_PRINT_E("calibration_obj is NULL");
		return -EINVAL;
	}

	memcpy(ucontrol->value.bytes.data,
		calibration_obj->buffer,
		ELLIPTIC_CALIBRATION_V2_DATA_SIZE);
	return 0;
}

int elliptic_calibration_v2_data_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	memcpy(&elliptic_engine_calibration_v2_data_cache,
	ucontrol->value.bytes.data, ELLIPTIC_CALIBRATION_V2_DATA_SIZE);

	return elliptic_data_write(ELLIPTIC_ULTRASOUND_SET_PARAMS,
		(const char *)&elliptic_engine_calibration_v2_data_cache,
		ELLIPTIC_CALIBRATION_V2_DATA_SIZE);
}

int elliptic_diagnostics_data_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	memcpy(ucontrol->value.bytes.data,
		&elliptic_engine_diagnostics_data_cache,
		ELLIPTIC_DIAGNOSTICS_DATA_SIZE);
	return 0;
}

int elliptic_diagnostics_data_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

int elliptic_ml_data_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct elliptic_shared_data_block *ml_obj =
		elliptic_get_shared_obj(ELLIPTIC_OBJ_ID_ML_DATA);

	if (ml_obj == NULL) {
		EL_PRINT_E("ml_obj is NULL");
		return -EINVAL;
	}

	memcpy(ucontrol->value.bytes.data,
		ml_obj->buffer,
		ELLIPTIC_ML_DATA_SIZE);
	return 0;
}

int elliptic_ml_data_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	return elliptic_data_write(ELLIPTIC_ULTRASOUND_SET_PARAMS,
		(const char *)ucontrol->value.bytes.data,
		ELLIPTIC_ML_DATA_SIZE);
}

int elliptic_sensor_data_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	memcpy(ucontrol->value.bytes.data,
		&elliptic_engine_sensor_data_cache,
		ELLIPTIC_SENSOR_DATA_SIZE);
	return 0;
}

int elliptic_sensor_data_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	memcpy(&elliptic_engine_sensor_data_cache,
	ucontrol->value.bytes.data, ELLIPTIC_SENSOR_DATA_SIZE);

	return elliptic_data_write(ELLIPTIC_ULTRASOUND_SET_PARAMS,
		(const char *)&elliptic_engine_sensor_data_cache,
		ELLIPTIC_SENSOR_DATA_SIZE);
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
		   ELLIPTIC_BRANCH_INFO_MAX_SIZE);
	return 0;
}

int elliptic_branch_data_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

int elliptic_tag_data_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	memcpy(ucontrol->value.bytes.data, &elliptic_engine_tag_cache,
		   ELLIPTIC_TAG_INFO_SIZE);
	return 0;
}

int elliptic_tag_data_put(struct snd_kcontrol *kcontrol,
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

	return elliptic_data_write(ELLIPTIC_ULTRASOUND_SET_PARAMS,
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

	if (mc->shift >= ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_0 &&
		mc->shift <= ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_15){
		EL_PRINT_E("get ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_%02d",
			mc->shift - ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_0);
		ucontrol->value.integer.value[0] = 0;
		return 1;
	}

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
	case ELLIPTIC_SYSTEM_CONFIGURATION_SUSPEND:
		ucontrol->value.integer.value[0] =
			elliptic_system_configuration_cache.engine_suspend;
		break;
	case ELLIPTIC_SYSTEM_CONFIGURATION_INPUT_ENABLED:
		ucontrol->value.integer.value[0] =
			elliptic_system_configuration_cache.input_enabled;
		break;
	case ELLIPTIC_SYSTEM_CONFIGURATION_OUTPUT_ENABLED:
		ucontrol->value.integer.value[0] =
			elliptic_system_configuration_cache.output_enabled;
		break;
	case ELLIPTIC_SYSTEM_CONFIGURATION_EXTERNAL_EVENT:
		ucontrol->value.integer.value[0] =
			elliptic_system_configuration_cache.external_event;
		break;
	case ELLIPTIC_SYSTEM_CONFIGURATION_CALIBRATION_METHOD:
		ucontrol->value.integer.value[0] =
			elliptic_system_configuration_cache.calibration_method;
	case ELLIPTIC_SYSTEM_CONFIGURATION_DEBUG_MODE:
		ucontrol->value.integer.value[0] =
			elliptic_system_configuration_cache.debug_mode;
		break;
	case ELLIPTIC_SYSTEM_CONFIGURATION_CONTEXT:
		ucontrol->value.integer.value[0] =
			elliptic_system_configuration_cache.context;
		break;
	case ELLIPTIC_SYSTEM_CONFIGURATION_CAPTURE:
		ucontrol->value.integer.value[0] =
			elliptic_system_configuration_cache.capture;
		break;
	case ELLIPTIC_SYSTEM_CONFIGURATION_INPUT_CHANNELS:
		ucontrol->value.integer.value[0] =
			elliptic_system_configuration_cache.input_channels;
		break;

	default:
		EL_PRINT_E("Invalid mixer control");
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
	struct timeval tv;

	if (mc->reg != ELLIPTIC_SYSTEM_CONFIGURATION)
		return -EINVAL;

	if (mc->shift >= ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_0 &&
		mc->shift <= ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_15){
		const size_t csi =
			mc->shift -
			ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_0;
		EL_PRINT_E("ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_XX csi:%zu", csi);
		if (csi >=
			ARRAY_SIZE(elliptic_system_configuration_cache.custom_settings))
			return -EINVAL;
		EL_PRINT_E("ucontrol->value.integer.value[0]:%ld", ucontrol->value.integer.value[0]);
		elliptic_system_configuration_cache.custom_settings[csi] =
			ucontrol->value.integer.value[0];
		param.type = ESCPT_ENGINE_CUSTOM_SETTING_0 + csi;
		param.custom_setting = ucontrol->value.integer.value[0];
		EL_PRINT_E("calling elliptic_data_write(custom_setting) csi:%zu", csi);
		return elliptic_data_write(ELLIPTIC_ULTRASOUND_SET_PARAMS,
				  (const char *)&param, sizeof(param));
	}


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
	case ELLIPTIC_SYSTEM_CONFIGURATION_INPUT_ENABLED:
		elliptic_system_configuration_cache.input_enabled =
			ucontrol->value.integer.value[0];
		param.type = ESCPT_INPUT_ENABLED;
		param.input_enabled =
		elliptic_system_configuration_cache.input_enabled;
		break;
	case ELLIPTIC_SYSTEM_CONFIGURATION_OUTPUT_ENABLED:
		elliptic_system_configuration_cache.output_enabled =
			ucontrol->value.integer.value[0];
		param.type = ESCPT_OUTPUT_ENABLED;
		param.output_enabled =
		elliptic_system_configuration_cache.output_enabled;
		break;
	case ELLIPTIC_SYSTEM_CONFIGURATION_SUSPEND:
		elliptic_system_configuration_cache.engine_suspend =
			ucontrol->value.integer.value[0];
		param.type = ESCPT_SUSPEND;
		param.engine_suspend =
		elliptic_system_configuration_cache.engine_suspend;
		break;
	case ELLIPTIC_SYSTEM_CONFIGURATION_EXTERNAL_EVENT:
		elliptic_system_configuration_cache.external_event =
			ucontrol->value.integer.value[0];
		param.type = ESCPT_EXTERNAL_EVENT;
		param.external_event =
		elliptic_system_configuration_cache.external_event;
		break;
	case ELLIPTIC_SYSTEM_CONFIGURATION_CALIBRATION_METHOD:
		elliptic_system_configuration_cache.calibration_method =
			ucontrol->value.integer.value[0];
		param.type = ESCPT_CALIBRATION_METHOD;
		param.calibration_method =
		elliptic_system_configuration_cache.calibration_method;
		do_gettimeofday(&tv);
		param.calibration_timestamp = (int32_t)tv.tv_sec;
		break;
	case ELLIPTIC_SYSTEM_CONFIGURATION_DEBUG_MODE:
		elliptic_system_configuration_cache.debug_mode =
			ucontrol->value.integer.value[0];
		param.type = ESCPT_DEBUG_MODE;
		param.debug_mode =
		elliptic_system_configuration_cache.debug_mode;
		break;
	case ELLIPTIC_SYSTEM_CONFIGURATION_CONTEXT:
		elliptic_system_configuration_cache.context =
			ucontrol->value.integer.value[0];
		param.type = ESCPT_CONTEXT;
		param.context =
		elliptic_system_configuration_cache.context;
		break;
	case ELLIPTIC_SYSTEM_CONFIGURATION_CAPTURE:
		elliptic_system_configuration_cache.capture =
			ucontrol->value.integer.value[0];
		param.type = ESCPT_CAPTURE;
		param.context =
		elliptic_system_configuration_cache.capture;
		break;
	case ELLIPTIC_SYSTEM_CONFIGURATION_INPUT_CHANNELS:
		elliptic_system_configuration_cache.input_channels =
			ucontrol->value.integer.value[0];
		param.type = ESCPT_INPUT_CHANNELS;
		param.context =
		elliptic_system_configuration_cache.input_channels;
		break;

	default:
		return -EINVAL;
	}

	return elliptic_data_write(ELLIPTIC_ULTRASOUND_SET_PARAMS,
				  (const char *)&param, sizeof(param));
}


static const struct snd_kcontrol_new ultrasound_filter_mixer_controls[] = {
	SOC_ENUM_EXT("Ultrasound Enable",
	elliptic_enum[0],
	elliptic_ultrasound_enable_get,
	elliptic_ultrasound_enable_set),
	SOC_ENUM_EXT("Ultrasound RampDown",
	elliptic_enum[0],
	elliptic_ultrasound_rampdown_get,
	elliptic_ultrasound_rampdown_set),
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
	256,
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
	ELLIPTIC_BRANCH_INFO_MAX_SIZE,
	elliptic_branch_data_get,
	elliptic_branch_data_put),
	SND_SOC_BYTES_EXT("Ultrasound Tag",
	ELLIPTIC_TAG_INFO_SIZE,
	elliptic_tag_data_get,
	elliptic_tag_data_put),
	SOC_SINGLE_EXT("Ultrasound Log Level",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_LOG_LEVEL,
	7,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),

	SND_SOC_BYTES_EXT("Ultrasound Calibration Ext Data",
	ELLIPTIC_CALIBRATION_V2_DATA_SIZE,
	elliptic_calibration_v2_data_get,
	elliptic_calibration_v2_data_put),

	SND_SOC_BYTES_EXT("Ultrasound Diagnostics Data",
	ELLIPTIC_DIAGNOSTICS_DATA_SIZE,
	elliptic_diagnostics_data_get,
	elliptic_diagnostics_data_put),

	SOC_ENUM_EXT("Ultrasound Diagnostics Request",
	elliptic_enum[0],
	elliptic_ultrasound_diagnostics_get,
	elliptic_ultrasound_request_diagnostics),

	SOC_SINGLE_EXT("Ultrasound Custom Setting 0",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_0,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SOC_SINGLE_EXT("Ultrasound Custom Setting 1",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_1,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SOC_SINGLE_EXT("Ultrasound Custom Setting 2",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_2,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SOC_SINGLE_EXT("Ultrasound Custom Setting 3",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_3,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SOC_SINGLE_EXT("Ultrasound Custom Setting 4",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_4,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SOC_SINGLE_EXT("Ultrasound Custom Setting 5",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_5,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SOC_SINGLE_EXT("Ultrasound Custom Setting 6",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_6,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SOC_SINGLE_EXT("Ultrasound Custom Setting 7",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_7,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SOC_SINGLE_EXT("Ultrasound Custom Setting 8",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_8,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SOC_SINGLE_EXT("Ultrasound Custom Setting 9",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_9,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SOC_SINGLE_EXT("Ultrasound Custom Setting 10",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_10,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SOC_SINGLE_EXT("Ultrasound Custom Setting 11",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_11,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SOC_SINGLE_EXT("Ultrasound Custom Setting 12",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_12,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SOC_SINGLE_EXT("Ultrasound Custom Setting 13",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_13,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SOC_SINGLE_EXT("Ultrasound Custom Setting 14",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_14,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SOC_SINGLE_EXT("Ultrasound Custom Setting 15",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_15,
	ELLIPTIC_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SOC_ENUM_EXT("Ultrasound Tx Port",
	elliptic_enum[0],
	elliptic_ultrasound_tx_port_get,
	elliptic_ultrasound_tx_port_set),
	SOC_ENUM_EXT("Ultrasound Rx Port",
	elliptic_enum[0],
	elliptic_ultrasound_rx_port_get,
	elliptic_ultrasound_rx_port_set),
	SOC_SINGLE_EXT("Ultrasound Suspend",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_SUSPEND,
	1,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SOC_SINGLE_EXT("Ultrasound Input",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_INPUT_ENABLED,
	1,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SOC_SINGLE_EXT("Ultrasound Output",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_OUTPUT_ENABLED,
	1,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SOC_SINGLE_EXT("Ultrasound Event",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_EXTERNAL_EVENT,
	256,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SOC_SINGLE_EXT("Ultrasound Calibration Method",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_CALIBRATION_METHOD,
	256,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),

	SND_SOC_BYTES_EXT("Ultrasound ML",
	ELLIPTIC_ML_DATA_SIZE,
	elliptic_ml_data_get,
	elliptic_ml_data_put),

	SOC_SINGLE_EXT("Ultrasound Debug Mode",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_DEBUG_MODE,
	256,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SOC_SINGLE_EXT("Ultrasound Context",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_CONTEXT,
	ELLIPTIC_SYSTEM_CONFIGURATION_MAX_CONTEXT_VALUE,
	0,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),
	SND_SOC_BYTES_EXT("Ultrasound Sensor Data",
	ELLIPTIC_SENSOR_DATA_SIZE,
	elliptic_sensor_data_get,
	elliptic_sensor_data_put),

	SOC_SINGLE_EXT("Ultrasound Capture",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_CAPTURE,
	256,
	-1,
	elliptic_system_configuration_param_get,
	elliptic_system_configuration_param_put),

	SOC_SINGLE_EXT("Ultrasound Tx Channels",
	ELLIPTIC_SYSTEM_CONFIGURATION,
	ELLIPTIC_SYSTEM_CONFIGURATION_INPUT_CHANNELS,
	16,
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
		EL_PRINT_E("pointer is NULL");
	}

	return num_controls;
}
EXPORT_SYMBOL(elliptic_add_platform_controls);

int elliptic_trigger_version_msg(void)
{
	int32_t msg[3] = {ESCPT_ENGINE_VERSION, 0, 0};

	return elliptic_data_write(
		ELLIPTIC_ULTRASOUND_SET_PARAMS,
		(const char *)msg, sizeof(msg));
}

int elliptic_trigger_branch_msg(void)
{
	int32_t msg[3] = {ESCPT_BUILD_BRANCH, 0, 0};

	return elliptic_data_write(
		ELLIPTIC_ULTRASOUND_SET_PARAMS,
		(const char *)msg, sizeof(msg));
}

int elliptic_trigger_tag_msg(void)
{
	int32_t msg[3] = {ESCPT_ENGINE_TAG, 0, 0};

	return elliptic_data_write(
		ELLIPTIC_ULTRASOUND_SET_PARAMS,
		(const char *)msg, sizeof(msg));
}

int elliptic_trigger_diagnostics_msg(void)
{
	int32_t msg[3] = {ESCPT_ENGINE_DIAGNOSTICS, 0, 0};

	return elliptic_data_write(
		ELLIPTIC_ULTRASOUND_SET_PARAMS,
		(const char *)msg, sizeof(msg));
}
