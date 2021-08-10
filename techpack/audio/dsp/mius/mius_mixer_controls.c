/**
 * MI
 */

#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <sound/asound.h>
#include <sound/soc.h>
#include <sound/control.h>

#include <mius/mius_mixer_controls.h>
#include <mius/mius_data_io.h>
#include <mius/mius_device.h>

struct mius_system_configuration {
	union {
		uint8_t reserved[MIUS_SYSTEM_CONFIGURATION_SIZE];
	};
};

struct mius_system_configuration mius_system_configuration;


struct mius_system_configuration_parameter {
	enum mius_system_configuration_parameter_type type;
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
		int32_t rx_device;
	};
};

struct mius_system_configuration_parameters_cache
		mius_system_configuration_cache = { {0}, 0 };

static struct mius_engine_version_info
		mius_engine_version_cache = { 0xde, 0xad, 0xbe, 0xef };

struct mius_ultrasound_calibration_data {
	uint8_t  data[MIUS_CALIBRATION_FLOAT_DATA_SIZE];
};

static struct mius_ultrasound_calibration_data
		mius_ultrasound_calibration_data_cache;


struct mius_engine_calibration_data {
	union {
		uint8_t reserved[MIUS_CALIBRATION_DATA_SIZE];
	};
};

static struct mius_engine_calibration_data
	mius_engine_calibration_data_cache = { .reserved = {

0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe,
0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad,
0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde,
0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,
0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe,
0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef } };

struct mius_engine_calibration_v2_data {
	union {
		uint8_t reserved[MIUS_CALIBRATION_V2_DATA_SIZE];
	};
};

static struct mius_engine_calibration_v2_data
	mius_engine_calibration_v2_data_cache = { .reserved = {

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

struct mius_engine_diagnostics_data {
	union {
		uint8_t reserved[MIUS_DIAGNOSTICS_DATA_SIZE];
		uint32_t values32[MIUS_DIAGNOSTICS_DATA_SIZE >> 2];
	};
};

static struct mius_engine_diagnostics_data
	mius_engine_diagnostics_data_cache = { .reserved = {0} };

struct mius_engine_ml_data {
	union {
		uint8_t reserved[MIUS_ML_DATA_SIZE];
		uint32_t u32[MIUS_ML_DATA_SIZE >> 2];
	};
};

static struct mius_engine_ml_data
	mius_engine_ml_data_cache
		= { .reserved = {0} };

struct mius_engine_sensor_data {
	union {
		uint8_t reserved[MIUS_SENSOR_DATA_SIZE];
		uint32_t values32[MIUS_SENSOR_DATA_SIZE >> 2];
	};
};

static struct mius_engine_sensor_data
	mius_engine_sensor_data_cache = { .reserved = {0} };

struct mius_engine_branch_info {
	char build_branch[MIUS_BRANCH_INFO_MAX_SIZE];
};

static struct mius_engine_branch_info
	mius_engine_branch_cache = { { 0 } };

struct mius_engine_tag_info {
	char engine_tag[MIUS_TAG_INFO_SIZE];
};

static struct mius_engine_tag_info
	mius_engine_tag_cache = { { 0 } };

static struct mius_shared_data_block shared_data_blocks[] = {
	{ MIUS_OBJ_ID_CALIBRATION_DATA, MIUS_CALIBRATION_DATA_SIZE,
		&mius_engine_calibration_data_cache },

	{ MIUS_OBJ_ID_VERSION_INFO, MIUS_VERSION_INFO_SIZE,
		&mius_engine_version_cache },
	{ MIUS_OBJ_ID_BRANCH_INFO, MIUS_BRANCH_INFO_MAX_SIZE,
		&mius_engine_branch_cache },
	{ MIUS_OBJ_ID_CALIBRATION_V2_DATA,
		MIUS_CALIBRATION_V2_DATA_SIZE,
		&mius_engine_calibration_v2_data_cache },
	{ MIUS_OBJ_ID_DIAGNOSTICS_DATA, MIUS_DIAGNOSTICS_DATA_SIZE,
		&mius_engine_diagnostics_data_cache },
	{ MIUS_OBJ_ID_TAG_INFO, MIUS_TAG_INFO_SIZE,
		&mius_engine_tag_cache },
	{ MIUS_OBJ_ID_ML_DATA,
		MIUS_ML_DATA_SIZE,
		&mius_engine_ml_data_cache },
};

void mius_set_calibration_data(uint8_t *calib_data, size_t size)
{
	struct mius_shared_data_block *calibration_obj = NULL;

	if (size == MIUS_CALIBRATION_DATA_SIZE) {
		calibration_obj = mius_get_shared_obj(
			MIUS_OBJ_ID_CALIBRATION_DATA);
		memcpy((uint8_t *)&mius_engine_calibration_data_cache,
			calib_data, size);
	}
	if (size == MIUS_CALIBRATION_V2_DATA_SIZE) {
		calibration_obj = mius_get_shared_obj(
				MIUS_OBJ_ID_CALIBRATION_V2_DATA);
		memcpy((uint8_t *)&mius_engine_calibration_v2_data_cache,
			calib_data, size);
	}
	if (calibration_obj == NULL) {
		MI_PRINT_E(
			"ell..set_calibration_data() calib=NULL (%zu)", size);
		return;
	}
	memcpy((u8 *)calibration_obj->buffer, calib_data, size);
}

void mius_set_diagnostics_data(uint8_t *diag_data, size_t size)
{
	struct mius_shared_data_block *diagnostics_obj = NULL;

	if (size <= MIUS_DIAGNOSTICS_DATA_SIZE) {
		diagnostics_obj =
			mius_get_shared_obj(
				MIUS_OBJ_ID_DIAGNOSTICS_DATA);
		if (diagnostics_obj == NULL) {
			MI_PRINT_E("el..set_diagnostics_data() NULL (%zu)",
				size);
			return;
		}
		memcpy((uint8_t *)&mius_engine_diagnostics_data_cache,
			diag_data, size);
		memcpy((u8 *)diagnostics_obj->buffer, diag_data, size);
	}
}

static const size_t NUM_SHARED_RW_OBJS =
	sizeof(shared_data_blocks) / sizeof(struct mius_shared_data_block);

struct mius_shared_data_block *mius_get_shared_obj(uint32_t
	object_id)
{

	size_t i;

	for (i = 0; i < NUM_SHARED_RW_OBJS; ++i) {
		if (shared_data_blocks[i].object_id == object_id)
			return &shared_data_blocks[i];
	}

	return NULL;
}


static const char * const ultrasound_enable_texts[] = {"Off", "On"};

static const struct soc_enum mius_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ultrasound_enable_texts),
	ultrasound_enable_texts),
};

int get_mius_calibration_data(uint8_t *caldata, uint32_t max_size)
{
	uint32_t copied;

	copied = MIUS_CALIBRATION_DATA_SIZE;
	if (max_size < MIUS_CALIBRATION_DATA_SIZE) {
		copied = max_size;
		MI_PRINT_D("size mismatch : %u vs %u",
			(uint32_t)MIUS_CALIBRATION_DATA_SIZE, max_size);
	}

	memcpy(caldata, (uint8_t *)&mius_engine_calibration_data_cache,
		max_size);
	return copied;
}

int get_mius_calibration_v2_data(uint8_t *caldata, uint32_t max_size)
{
	uint32_t copied;

	copied = MIUS_CALIBRATION_V2_DATA_SIZE;
	if (max_size < MIUS_CALIBRATION_V2_DATA_SIZE) {
		copied = max_size;
		MI_PRINT_D("size mismatch : %u vs %u",
			(uint32_t)MIUS_CALIBRATION_V2_DATA_SIZE, max_size);
	}

	memcpy(caldata, (uint8_t *)&mius_engine_calibration_v2_data_cache,
		max_size);
	return copied;
}

int get_mius_diagnostics_data(uint8_t *diagdata, uint32_t max_size)
{
	uint32_t copied;

	copied = MIUS_DIAGNOSTICS_DATA_SIZE;
	if (max_size < MIUS_DIAGNOSTICS_DATA_SIZE) {
		copied = max_size;
		MI_PRINT_D("size mismatch : %u vs %u",
			(uint32_t)MIUS_DIAGNOSTICS_DATA_SIZE, max_size);
	}

	memcpy(diagdata, (uint8_t *)&mius_engine_diagnostics_data_cache,
		max_size);
	return copied;
}


static uint32_t ultrasound_enable_cache;

int mius_ultrasound_enable_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = ultrasound_enable_cache;
	return 0;
}

int mius_ultrasound_enable_set(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int32_t msg[4] = {0, 0, 0, 0};

	ultrasound_enable_cache = ucontrol->value.integer.value[0];

	msg[0] = ultrasound_enable_cache ? 1 : 0;

	return mius_data_write(
		MIUS_ULTRASOUND_ENABLE,
		(const char *)msg, sizeof(msg));
}

static uint32_t ultrasound_suspend_cache;

int mius_ultrasound_suspend_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = ultrasound_suspend_cache;
	return 0;
}

static uint32_t ultrasound_report_none_cache;

int mius_ultrasound_report_none_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = ultrasound_report_none_cache;
	return 0;
}

int mius_ultrasound_report_none_set(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int32_t msg[4] = {0, 0, 0, 0};

	ultrasound_report_none_cache = ucontrol->value.integer.value[0];

	msg[0] = ultrasound_report_none_cache ? 1 : 0;

	return mius_data_write(
		MIUS_ULTRASOUND_UPLOAD_NONE,
		(const char *)msg, sizeof(msg));
}

int mius_ultrasound_suspend_set(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int32_t msg[4] = {0, 0, 0, 0};

	ultrasound_suspend_cache = ucontrol->value.integer.value[0];

	msg[0] = ultrasound_suspend_cache ? 1 : 0;

	return mius_data_write(
		MIUS_ULTRASOUND_SUSPEND,
		(const char *)msg, sizeof(msg));
}

static uint32_t ultrasound_log_level_cache;

int mius_ultrasound_log_level_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = ultrasound_log_level_cache;
	return 0;
}

int mius_ultrasound_log_level_set(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int32_t msg[4] = {0, 0, 0, 0};

	ultrasound_log_level_cache = ucontrol->value.integer.value[0];

	msg[0] = ultrasound_log_level_cache;

	return mius_data_write(
		MIUS_ULTRASOUND_DEBUG_LEVEL,
		(const char *)msg, sizeof(msg));
}

static uint32_t ultrasound_mode_cache;

int mius_ultrasound_mode_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = ultrasound_mode_cache;
	return 0;
}

int mius_ultrasound_mode_set(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int32_t msg[4] = {0, 0, 0, 0};

	ultrasound_mode_cache = ucontrol->value.integer.value[0];

	msg[0] = ultrasound_mode_cache;

	return mius_data_write(
		MIUS_ULTRASOUND_MODE,
		(const char *)msg, sizeof(msg));
}

static uint32_t ultrasound_tx_port_cache;

int mius_ultrasound_tx_port_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = ultrasound_tx_port_cache;
	return 0;
}

int mius_ultrasound_tx_port_set(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int ret;

	if (ultrasound_tx_port_cache == ucontrol->value.integer.value[0]) {
		MI_PRINT_E("ultrasound_tx_port_set: ignoring duplicate request");
		return 0;
	}

	ultrasound_tx_port_cache = ucontrol->value.integer.value[0];
	printk(KERN_DEBUG "[MIUS] Via ULTRASOUND_TX_PORT_ID enable=%d", ultrasound_tx_port_cache);
	if (ultrasound_tx_port_cache)
		ret = mius_open_port(ULTRASOUND_TX_PORT_ID);
	else
		ret = mius_close_port(ULTRASOUND_TX_PORT_ID);

	MI_PRINT_E("ultrasound_tx_port: enable=%d ret=%d",
		ultrasound_tx_port_cache, ret);

	return ret;
}

static uint32_t ultrasound_rx_port_cache;

int mius_ultrasound_rx_port_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = ultrasound_rx_port_cache;
	return 0;
}

int mius_ultrasound_rx_port_set(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int ret;

	if (ultrasound_rx_port_cache == ucontrol->value.integer.value[0]) {
		MI_PRINT_E("mi ultrasound_rx_port_set: ignoring duplicate request");
		return 0;
	}

	ultrasound_rx_port_cache = ucontrol->value.integer.value[0];
	printk(KERN_DEBUG "[MIUS] if we are here, it is wrong for mius, enable=%d",
			 ultrasound_rx_port_cache);
	if (ultrasound_rx_port_cache)
		ret = mius_open_port(ULTRASOUND_RX_PORT_ID);
	else
		ret = mius_close_port(ULTRASOUND_RX_PORT_ID);

	MI_PRINT_E("mi ultrasound_rx_port: enable=%d ret=%d",
		ultrasound_rx_port_cache, ret);

	return 0;
}

int mius_ultrasound_rampdown_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	/* Rampdown is a strobe, so always return Off */
	ucontrol->value.integer.value[0] = 0;
	return 0;
}

int mius_ultrasound_rampdown_set(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int32_t msg[2] = {1, 0};

	if (ucontrol->value.integer.value[0] == 0)
		return 0;

	return mius_data_write(MIUS_ULTRASOUND_RAMP_DOWN,
		(const char *)msg, sizeof(msg));
}

int mius_ultrasound_diagnostics_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	/* Diagnostics is a strobe, so always return Off */
	ucontrol->value.integer.value[0] = 0;
	return 0;
}

int mius_ultrasound_request_diagnostics(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int32_t msg[3] = {MSC_ENGINE_DIAGNOSTICS, 0, 0};

	if (ucontrol->value.integer.value[0] == 0)
		return 0;

	return mius_data_write(MIUS_ULTRASOUND_SET_PARAMS,
				(const char *)msg, sizeof(msg));
}

int mius_system_configuration_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	memcpy(ucontrol->value.bytes.data, &mius_system_configuration,
		   MIUS_SYSTEM_CONFIGURATION_SIZE);
	return 0;
}

int mius_system_configuration_put(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	memcpy(&mius_system_configuration, ucontrol->value.bytes.data,
		   MIUS_SYSTEM_CONFIGURATION_SIZE);

	return mius_data_write(MIUS_ULTRASOUND_SET_PARAMS,
				  (const char *)&mius_system_configuration,
				  MIUS_SYSTEM_CONFIGURATION_SIZE);
}

int mius_calibration_data_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	memcpy(ucontrol->value.bytes.data,
		&mius_engine_calibration_data_cache,
		MIUS_CALIBRATION_DATA_SIZE);
	return 0;
}

int mius_calibration_data_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	memcpy(mius_ultrasound_calibration_data_cache.data,
		ucontrol->value.bytes.data, MIUS_CALIBRATION_FLOAT_DATA_SIZE);

	return mius_data_write(MIUS_ULTRASOUND_CL_DATA,
		(const char *)&mius_ultrasound_calibration_data_cache,
		sizeof(mius_ultrasound_calibration_data_cache));
}

int mius_calibration_v2_data_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct mius_shared_data_block *calibration_obj =
		mius_get_shared_obj(MIUS_OBJ_ID_CALIBRATION_V2_DATA);

	if (calibration_obj == NULL) {
		MI_PRINT_E("calibration_obj is NULL");
		return -EINVAL;
	}

	memcpy(ucontrol->value.bytes.data,
		calibration_obj->buffer,
		MIUS_CALIBRATION_V2_DATA_SIZE);
	return 0;
}

int mius_calibration_v2_data_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	memcpy(&mius_engine_calibration_v2_data_cache,
	ucontrol->value.bytes.data, MIUS_CALIBRATION_V2_DATA_SIZE);

	return mius_data_write(MIUS_ULTRASOUND_SET_PARAMS,
		(const char *)&mius_engine_calibration_v2_data_cache,
		MIUS_CALIBRATION_V2_DATA_SIZE);
}

int mius_diagnostics_data_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	memcpy(ucontrol->value.bytes.data,
		&mius_engine_diagnostics_data_cache,
		MIUS_DIAGNOSTICS_DATA_SIZE);
	return 0;
}

int mius_diagnostics_data_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

int mius_ml_data_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct mius_shared_data_block *ml_obj =
		mius_get_shared_obj(MIUS_OBJ_ID_ML_DATA);

	if (ml_obj == NULL) {
		MI_PRINT_E("ml_obj is NULL");
		return -EINVAL;
	}

	memcpy(ucontrol->value.bytes.data,
		ml_obj->buffer,
		MIUS_ML_DATA_SIZE);
	return 0;
}

int mius_ml_data_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	return mius_data_write(MIUS_ULTRASOUND_SET_PARAMS,
		(const char *)ucontrol->value.bytes.data,
		MIUS_ML_DATA_SIZE);
}

int mius_sensor_data_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	memcpy(ucontrol->value.bytes.data,
		&mius_engine_sensor_data_cache,
		MIUS_SENSOR_DATA_SIZE);
	return 0;
}

int mius_sensor_data_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	memcpy(&mius_engine_sensor_data_cache,
	ucontrol->value.bytes.data, MIUS_SENSOR_DATA_SIZE);

	return mius_data_write(MIUS_ULTRASOUND_SET_PARAMS,
		(const char *)&mius_engine_sensor_data_cache,
		MIUS_SENSOR_DATA_SIZE);
}

int mius_version_data_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	memcpy(ucontrol->value.bytes.data, &mius_engine_version_cache,
		   MIUS_VERSION_INFO_SIZE);
	return 0;
}

int mius_version_data_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

int mius_branch_data_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	memcpy(ucontrol->value.bytes.data, &mius_engine_branch_cache,
		   MIUS_BRANCH_INFO_MAX_SIZE);
	return 0;
}

int mius_branch_data_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

int mius_tag_data_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	memcpy(ucontrol->value.bytes.data, &mius_engine_tag_cache,
		   MIUS_TAG_INFO_SIZE);
	return 0;
}

int mius_tag_data_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

int mius_calibration_param_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	if (mc->reg != MIUS_CALIBRATION)
		return -EINVAL;

	switch (mc->shift) {
	case MIUS_CALIBRATION_STATE:
		ucontrol->value.integer.value[0] =
			mius_system_configuration_cache.calibration_state;
		break;

	case MIUS_CALIBRATION_PROFILE:
		ucontrol->value.integer.value[0] =
			mius_system_configuration_cache.calibration_profile;
		break;

	case MIUS_ULTRASOUND_GAIN:
		ucontrol->value.integer.value[0] =
			mius_system_configuration_cache.ultrasound_gain;
		break;

	default:
		return -EINVAL;
	}

	return 1;
}

int mius_calibration_param_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct mius_system_configuration_parameter param;

	if (mc->reg != MIUS_CALIBRATION)
		return -EINVAL;

	switch (mc->shift) {
	case MIUS_CALIBRATION_STATE:
		mius_system_configuration_cache.calibration_state =
			ucontrol->value.integer.value[0];

		param.type = MSC_CALIBRATION_STATE;
		param.calibration_state =
			mius_system_configuration_cache.calibration_state;
		break;

	case MIUS_CALIBRATION_PROFILE:
		mius_system_configuration_cache.calibration_profile =
			ucontrol->value.integer.value[0];

		param.type = MSC_CALIBRATION_PROFILE;
		param.calibration_profile =
			mius_system_configuration_cache.calibration_profile;
		break;

	case MIUS_ULTRASOUND_GAIN:
		mius_system_configuration_cache.ultrasound_gain =
			ucontrol->value.integer.value[0];
		param.type = MSC_ULTRASOUND_GAIN;
		param.ultrasound_gain =
			mius_system_configuration_cache.ultrasound_gain;
		break;

	default:
		return -EINVAL;
	}

	return mius_data_write(MIUS_ULTRASOUND_SET_PARAMS,
				  (u8 *)&param, sizeof(param));
}

int mius_system_configuration_param_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	pr_err("%s: reg: %d shift: %d\n", __func__, mc->reg, mc->shift);

	if (mc->reg != MIUS_SYSTEM_CONFIGURATION)
		return -EINVAL;

	if (mc->shift >= MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_0 &&
		mc->shift <= MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_15){
		MI_PRINT_E("get MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_%02d",
			mc->shift - MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_0);
		ucontrol->value.integer.value[0] = 0;
		return 1;
	}

	switch (mc->shift) {
	case MIUS_SYSTEM_CONFIGURATION_LATENCY:
		ucontrol->value.integer.value[0] =
			mius_system_configuration_cache.latency;
		break;

	case MIUS_SYSTEM_CONFIGURATION_SENSITIVITY:
		ucontrol->value.integer.value[0] =
			mius_system_configuration_cache.sensitivity;
		break;

	case MIUS_SYSTEM_CONFIGURATION_SPEAKER_SCALING:
		ucontrol->value.integer.value[0] =
			mius_system_configuration_cache.speaker_scaling[0];
		ucontrol->value.integer.value[1] =
			mius_system_configuration_cache.speaker_scaling[1];
		break;

	case MIUS_SYSTEM_CONFIGURATION_MICROPHONE_INDEX:
		ucontrol->value.integer.value[0] =
			mius_system_configuration_cache.microphone_index;
		break;

	case MIUS_SYSTEM_CONFIGURATION_OPERATION_MODE:
		ucontrol->value.integer.value[0] =
			mius_system_configuration_cache.operation_mode;
		break;

	case MIUS_SYSTEM_CONFIGURATION_OPERATION_MODE_FLAGS:
		ucontrol->value.integer.value[0] =
		mius_system_configuration_cache.operation_mode_flags;
		break;

	case MIUS_SYSTEM_CONFIGURATION_LOG_LEVEL:
		ucontrol->value.integer.value[0] =
			mius_system_configuration_cache.log_level;
		break;
	case MIUS_SYSTEM_CONFIGURATION_SUSPEND:
		ucontrol->value.integer.value[0] =
			mius_system_configuration_cache.engine_suspend;
		break;
	case MIUS_SYSTEM_CONFIGURATION_INPUT_ENABLED:
		ucontrol->value.integer.value[0] =
			mius_system_configuration_cache.input_enabled;
		break;
	case MIUS_SYSTEM_CONFIGURATION_OUTPUT_ENABLED:
		ucontrol->value.integer.value[0] =
			mius_system_configuration_cache.output_enabled;
		break;
	case MIUS_SYSTEM_CONFIGURATION_EXTERNAL_EVENT:
		ucontrol->value.integer.value[0] =
			mius_system_configuration_cache.external_event;
		break;
	case MIUS_SYSTEM_CONFIGURATION_CALIBRATION_METHOD:
		ucontrol->value.integer.value[0] =
			mius_system_configuration_cache.calibration_method;
	case MIUS_SYSTEM_CONFIGURATION_DEBUG_MODE:
		ucontrol->value.integer.value[0] =
			mius_system_configuration_cache.debug_mode;
		break;
	case MIUS_SYSTEM_CONFIGURATION_CONTEXT:
		ucontrol->value.integer.value[0] =
			mius_system_configuration_cache.context;
		break;
	case MIUS_SYSTEM_CONFIGURATION_CAPTURE:
		ucontrol->value.integer.value[0] =
			mius_system_configuration_cache.capture;
		break;
	case MIUS_SYSTEM_CONFIGURATION_INPUT_CHANNELS:
		ucontrol->value.integer.value[0] =
			mius_system_configuration_cache.input_channels;
		break;
	case MIUS_SYSTEM_CONFIGURATION_RX_DEVICE:
		ucontrol->value.integer.value[0] =
			mius_system_configuration_cache.rx_device;
		break;

	default:
		MI_PRINT_E("Invalid mixer control");
		return -EINVAL;
	}

	return 1;
}



int mius_system_configuration_param_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct mius_system_configuration_parameter param;

	if (mc->reg != MIUS_SYSTEM_CONFIGURATION)
		return -EINVAL;

	if (mc->shift >= MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_0 &&
		mc->shift <= MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_15){
		const size_t csi =
			mc->shift -
			MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_0;
		MI_PRINT_E("MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_XX csi:%zu", csi);
		if (csi >=
			ARRAY_SIZE(mius_system_configuration_cache.custom_settings))
			return -EINVAL;
		MI_PRINT_E("ucontrol->value.integer.value[0]:%ld", ucontrol->value.integer.value[0]);
		mius_system_configuration_cache.custom_settings[csi] =
			ucontrol->value.integer.value[0];
		param.type = MSC_ENGINE_CUSTOM_SETTING_0 + csi;
		param.custom_setting = ucontrol->value.integer.value[0];
		MI_PRINT_E("calling mius_data_write(custom_setting) csi:%zu", csi);
		return mius_data_write(MIUS_ULTRASOUND_SET_PARAMS,
				  (const char *)&param, sizeof(param));
	}


	switch (mc->shift) {
	case MIUS_SYSTEM_CONFIGURATION_LATENCY:
		mius_system_configuration_cache.latency =
			ucontrol->value.integer.value[0];
		param.type = MSC_LATENCY;
		param.latency = mius_system_configuration_cache.latency;
		break;

	case MIUS_SYSTEM_CONFIGURATION_SENSITIVITY:
		mius_system_configuration_cache.sensitivity =
			ucontrol->value.integer.value[0];
		param.type = MSC_CHANNEL_SENSITIVITY;
		param.sensitivity =
			mius_system_configuration_cache.sensitivity;
		break;

	case MIUS_SYSTEM_CONFIGURATION_SPEAKER_SCALING:
		mius_system_configuration_cache.speaker_scaling[0] =
			ucontrol->value.integer.value[0];
		mius_system_configuration_cache.speaker_scaling[1] =
			ucontrol->value.integer.value[1];
		param.type = MSC_SPEAKER_SCALING;
		param.speaker_scaling[0] =
			mius_system_configuration_cache.speaker_scaling[0];
		param.speaker_scaling[1] =
			mius_system_configuration_cache.speaker_scaling[1];
		break;

	case MIUS_SYSTEM_CONFIGURATION_MICROPHONE_INDEX:
		mius_system_configuration_cache.microphone_index =
			ucontrol->value.integer.value[0];
		param.type = MSC_MICROPHONE_INDEX;
		param.microphone_index =
			mius_system_configuration_cache.microphone_index;
		break;

	case MIUS_SYSTEM_CONFIGURATION_OPERATION_MODE:
		mius_system_configuration_cache.operation_mode =
			ucontrol->value.integer.value[0];
		param.type = MSC_OPERATION_MODE;
		param.operation_mode =
			mius_system_configuration_cache.operation_mode;
		break;

	case MIUS_SYSTEM_CONFIGURATION_OPERATION_MODE_FLAGS:
		mius_system_configuration_cache.operation_mode_flags =
			ucontrol->value.integer.value[0];
		param.type = MSC_OPERATION_MODE_FLAGS;
		param.operation_mode_flags =
		mius_system_configuration_cache.operation_mode_flags;
		break;

	case MIUS_SYSTEM_CONFIGURATION_LOG_LEVEL:
		mius_system_configuration_cache.log_level =
			ucontrol->value.integer.value[0];
		param.type = MSC_LOG_LEVEL;
		param.log_level =
		mius_system_configuration_cache.log_level;
		break;
	case MIUS_SYSTEM_CONFIGURATION_INPUT_ENABLED:
		mius_system_configuration_cache.input_enabled =
			ucontrol->value.integer.value[0];
		param.type = MSC_INPUT_ENABLED;
		param.input_enabled =
		mius_system_configuration_cache.input_enabled;
		break;
	case MIUS_SYSTEM_CONFIGURATION_OUTPUT_ENABLED:
		mius_system_configuration_cache.output_enabled =
			ucontrol->value.integer.value[0];
		param.type = MSC_OUTPUT_ENABLED;
		param.output_enabled =
		mius_system_configuration_cache.output_enabled;
		break;
	case MIUS_SYSTEM_CONFIGURATION_SUSPEND:
		mius_system_configuration_cache.engine_suspend =
			ucontrol->value.integer.value[0];
		param.type = MSC_SUSPEND;
		param.engine_suspend =
		mius_system_configuration_cache.engine_suspend;
		break;
	case MIUS_SYSTEM_CONFIGURATION_EXTERNAL_EVENT:
		mius_system_configuration_cache.external_event =
			ucontrol->value.integer.value[0];
		param.type = MSC_EXTERNAL_EVENT;
		param.external_event =
		mius_system_configuration_cache.external_event;
		break;
	case MIUS_SYSTEM_CONFIGURATION_CALIBRATION_METHOD:
		mius_system_configuration_cache.calibration_method =
			ucontrol->value.integer.value[0];
		param.type = MSC_CALIBRATION_METHOD;
		param.calibration_method =
		mius_system_configuration_cache.calibration_method;
		break;
	case MIUS_SYSTEM_CONFIGURATION_DEBUG_MODE:
		mius_system_configuration_cache.debug_mode =
			ucontrol->value.integer.value[0];
		param.type = MSC_DEBUG_MODE;
		param.debug_mode =
		mius_system_configuration_cache.debug_mode;
		break;
	case MIUS_SYSTEM_CONFIGURATION_CONTEXT:
		mius_system_configuration_cache.context =
			ucontrol->value.integer.value[0];
		param.type = MSC_CONTEXT;
		param.context =
		mius_system_configuration_cache.context;
		break;
	case MIUS_SYSTEM_CONFIGURATION_CAPTURE:
		mius_system_configuration_cache.capture =
			ucontrol->value.integer.value[0];
		param.type = MSC_CAPTURE;
		param.context =
		mius_system_configuration_cache.capture;
		break;
	case MIUS_SYSTEM_CONFIGURATION_INPUT_CHANNELS:
		mius_system_configuration_cache.input_channels =
			ucontrol->value.integer.value[0];
		param.type = MSC_INPUT_CHANNELS;
		param.context =
		mius_system_configuration_cache.input_channels;
		break;
	case MIUS_SYSTEM_CONFIGURATION_RX_DEVICE:
		mius_system_configuration_cache.rx_device =
			ucontrol->value.integer.value[0];
		param.type = MSC_RX_DEVICE;
		param.rx_device =
		mius_system_configuration_cache.rx_device;
		break;

	default:
		return -EINVAL;
	}

	return mius_data_write(MIUS_ULTRASOUND_SET_PARAMS,
				  (const char *)&param, sizeof(param));
}


static const struct snd_kcontrol_new ultrasound_filter_mixer_controls[] = {
	SOC_ENUM_EXT("Mi_Ultrasound Enable",
	mius_enum[0],
	mius_ultrasound_enable_get,
	mius_ultrasound_enable_set),
	SOC_ENUM_EXT("Mi_Ultrasound RampDown",
	mius_enum[0],
	mius_ultrasound_rampdown_get,
	mius_ultrasound_rampdown_set),
	SOC_SINGLE_EXT("Mi_Ultrasound Latency",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_LATENCY,
	10000,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),
	SOC_SINGLE_EXT("Mi_Ultrasound Sensitivity",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_SENSITIVITY,
	1000000,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),
	SOC_DOUBLE_EXT("Mi_Ultrasound Speaker Scaling",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_SPEAKER_SCALING,
	0,
	1000000,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),
	SOC_SINGLE_EXT("Mi_Ultrasound Microphone Index",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_MICROPHONE_INDEX,
	20,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),
	SOC_SINGLE_EXT("Mi_Ultrasound Mode",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_OPERATION_MODE,
	255,
	0,
	mius_ultrasound_mode_get,
	mius_ultrasound_mode_set),
	SOC_SINGLE_EXT("Mi_Ultrasound Mode Flags",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_OPERATION_MODE_FLAGS,
	256,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),
	SOC_SINGLE_EXT("Mi_Ultrasound Calibration Profile",
	MIUS_CALIBRATION,
	MIUS_CALIBRATION_PROFILE,
	256,
	0,
	mius_calibration_param_get,
	mius_calibration_param_put),
	SOC_SINGLE_EXT("Mi_Ultrasound Gain",
	MIUS_CALIBRATION,
	MIUS_ULTRASOUND_GAIN,
	256,
	0,
	mius_calibration_param_get,
	mius_calibration_param_put),

	SOC_SINGLE_EXT("Mi_Ultrasound Calibration State",
	MIUS_CALIBRATION,
	MIUS_CALIBRATION_STATE,
	256,
	0,
	mius_calibration_param_get,
	mius_calibration_param_put),

	SND_SOC_BYTES_EXT("Mi_Ultrasound System Configuration",
	MIUS_SYSTEM_CONFIGURATION_SIZE,
	mius_system_configuration_get,
	mius_system_configuration_put),
	SND_SOC_BYTES_EXT("Mi_Ultrasound Calibration Data",
	MIUS_CALIBRATION_FLOAT_DATA_SIZE,
	mius_calibration_data_get,
	mius_calibration_data_put),
	SND_SOC_BYTES_EXT("Mi_Ultrasound Version",
	MIUS_VERSION_INFO_SIZE,
	mius_version_data_get,
	mius_version_data_put),
	SND_SOC_BYTES_EXT("Mi_Ultrasound Branch",
	MIUS_BRANCH_INFO_MAX_SIZE,
	mius_branch_data_get,
	mius_branch_data_put),
	SND_SOC_BYTES_EXT("Mi_Ultrasound Tag",
	MIUS_TAG_INFO_SIZE,
	mius_tag_data_get,
	mius_tag_data_put),
	SOC_SINGLE_EXT("Mi_Ultrasound Log Level",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_LOG_LEVEL,
	5,
	0,
	mius_ultrasound_log_level_get,
	mius_ultrasound_log_level_set),

	SND_SOC_BYTES_EXT("Mi_Ultrasound Calibration Ext Data",
	MIUS_CALIBRATION_V2_DATA_SIZE,
	mius_calibration_v2_data_get,
	mius_calibration_v2_data_put),

	SND_SOC_BYTES_EXT("Mi_Ultrasound Diagnostics Data",
	MIUS_DIAGNOSTICS_DATA_SIZE,
	mius_diagnostics_data_get,
	mius_diagnostics_data_put),

	SOC_ENUM_EXT("Mi_Ultrasound Diagnostics Request",
	mius_enum[0],
	mius_ultrasound_diagnostics_get,
	mius_ultrasound_request_diagnostics),

	SOC_SINGLE_EXT("Mi_Ultrasound Custom Setting 0",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_0,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),
	SOC_SINGLE_EXT("Mi_Ultrasound Custom Setting 1",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_1,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),
	SOC_SINGLE_EXT("Mi_Ultrasound Custom Setting 2",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_2,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),
	SOC_SINGLE_EXT("Mi_Ultrasound Custom Setting 3",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_3,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),
	SOC_SINGLE_EXT("Mi_Ultrasound Custom Setting 4",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_4,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),
	SOC_SINGLE_EXT("Mi_Ultrasound Custom Setting 5",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_5,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),
	SOC_SINGLE_EXT("Mi_Ultrasound Custom Setting 6",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_6,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),
	SOC_SINGLE_EXT("Mi_Ultrasound Custom Setting 7",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_7,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),
	SOC_SINGLE_EXT("Mi_Ultrasound Custom Setting 8",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_8,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),
	SOC_SINGLE_EXT("Mi_Ultrasound Custom Setting 9",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_9,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),
	SOC_SINGLE_EXT("Mi_Ultrasound Custom Setting 10",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_10,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),
	SOC_SINGLE_EXT("Mi_Ultrasound Custom Setting 11",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_11,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),
	SOC_SINGLE_EXT("Mi_Ultrasound Custom Setting 12",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_12,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),
	SOC_SINGLE_EXT("Mi_Ultrasound Custom Setting 13",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_13,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),
	SOC_SINGLE_EXT("Mi_Ultrasound Custom Setting 14",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_14,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),
	SOC_SINGLE_EXT("Mi_Ultrasound Custom Setting 15",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_15,
	MIUS_SYSTEM_CONFIGURATION_CUSTOM_SETTING_MAX_VALUE,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),
	SOC_ENUM_EXT("Mi_Ultrasound Tx Port",
	mius_enum[0],
	mius_ultrasound_tx_port_get,
	mius_ultrasound_tx_port_set),
	SOC_ENUM_EXT("Mi_Ultrasound Rx Port",
	mius_enum[0],
	mius_ultrasound_rx_port_get,
	mius_ultrasound_rx_port_set),
	SOC_SINGLE_EXT("Mi_Ultrasound Suspend",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_SUSPEND,
	1,
	0,
	mius_ultrasound_suspend_get,
	mius_ultrasound_suspend_set),
	SOC_SINGLE_EXT("Mi_Ultrasound ReportNone",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_REPORT_NONE,
	1,
	0,
	mius_ultrasound_report_none_get,
	mius_ultrasound_report_none_set),
	SOC_SINGLE_EXT("Mi_Ultrasound Input",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_INPUT_ENABLED,
	1,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),
	SOC_SINGLE_EXT("Mi_Ultrasound Output",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_OUTPUT_ENABLED,
	1,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),
	SOC_SINGLE_EXT("Mi_Ultrasound Event",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_EXTERNAL_EVENT,
	256,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),
	SOC_SINGLE_EXT("Mi_Ultrasound Calibration Method",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_CALIBRATION_METHOD,
	256,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),

	SND_SOC_BYTES_EXT("Mi_Ultrasound ML",
	MIUS_ML_DATA_SIZE,
	mius_ml_data_get,
	mius_ml_data_put),

	SOC_SINGLE_EXT("Mi_Ultrasound Debug Mode",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_DEBUG_MODE,
	256,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),
	SOC_SINGLE_EXT("Mi_Ultrasound Context",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_CONTEXT,
	MIUS_SYSTEM_CONFIGURATION_MAX_CONTEXT_VALUE,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),
	SND_SOC_BYTES_EXT("Mi_Ultrasound Sensor Data",
	MIUS_SENSOR_DATA_SIZE,
	mius_sensor_data_get,
	mius_sensor_data_put),

	SOC_SINGLE_EXT("Mi_Ultrasound Capture",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_CAPTURE,
	256,
	-1,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),

	SOC_SINGLE_EXT("Mi_Ultrasound Tx Channels",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_INPUT_CHANNELS,
	16,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),
	SOC_SINGLE_EXT("Mi_Ultrasound Rx Device",
	MIUS_SYSTEM_CONFIGURATION,
	MIUS_SYSTEM_CONFIGURATION_RX_DEVICE,
	5,
	0,
	mius_system_configuration_param_get,
	mius_system_configuration_param_put),

};

unsigned int mius_add_component_controls(void *component)
{
	const unsigned int num_controls =
		ARRAY_SIZE(ultrasound_filter_mixer_controls);

	if (component != NULL) {
		snd_soc_add_component_controls(
			(struct snd_soc_component *)component,
			ultrasound_filter_mixer_controls,
			num_controls);
	} else {
		MI_PRINT_E("pointer is NULL");
	}

	return num_controls;
}
EXPORT_SYMBOL(mius_add_component_controls);

int mius_trigger_version_msg(void)
{
	int32_t msg[3] = {MSC_ENGINE_VERSION, 0, 0};

	return mius_data_write(
		MIUS_ULTRASOUND_SET_PARAMS,
		(const char *)msg, sizeof(msg));
}

int mius_trigger_branch_msg(void)
{
	int32_t msg[3] = {MSC_BUILD_BRANCH, 0, 0};

	return mius_data_write(
		MIUS_ULTRASOUND_SET_PARAMS,
		(const char *)msg, sizeof(msg));
}

int mius_trigger_tag_msg(void)
{
	int32_t msg[3] = {MSC_ENGINE_TAG, 0, 0};

	return mius_data_write(
		MIUS_ULTRASOUND_SET_PARAMS,
		(const char *)msg, sizeof(msg));
}

int mius_trigger_diagnostics_msg(void)
{
	int32_t msg[3] = {MSC_ENGINE_DIAGNOSTICS, 0, 0};

	return mius_data_write(
		MIUS_ULTRASOUND_SET_PARAMS,
		(const char *)msg, sizeof(msg));
}
