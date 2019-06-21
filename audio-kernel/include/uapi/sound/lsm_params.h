#ifndef _UAPI_LSM_PARAMS_H__
#define _UAPI_LSM_PARAMS_H__

#define LSM_POLLING_ENABLE_SUPPORT
#define LSM_EVENT_TIMESTAMP_MODE_SUPPORT

#include <linux/types.h>
#include <sound/asound.h>

#define SNDRV_LSM_VERSION SNDRV_PROTOCOL_VERSION(0, 3, 1)

#define LSM_MAX_STAGES_PER_SESSION 2
#define LSM_STAGE_INDEX_FIRST 0

#define LSM_OUT_FORMAT_PCM (0)
#define LSM_OUT_FORMAT_ADPCM (1 << 0)

#define LSM_OUT_DATA_RAW (0)
#define LSM_OUT_DATA_PACKED (1)

#define LSM_OUT_DATA_EVENTS_DISABLED (0)
#define LSM_OUT_DATA_EVENTS_ENABLED (1)

#define LSM_OUT_TRANSFER_MODE_RT (0)
#define LSM_OUT_TRANSFER_MODE_FTRT (1)

#define LSM_ENDPOINT_DETECT_THRESHOLD (0)
#define LSM_OPERATION_MODE (1)
#define LSM_GAIN (2)
#define LSM_MIN_CONFIDENCE_LEVELS (3)
#define LSM_REG_SND_MODEL (4)
#define LSM_DEREG_SND_MODEL (5)
#define LSM_CUSTOM_PARAMS (6)
#define LSM_POLLING_ENABLE (7)
#define LSM_DET_EVENT_TYPE (8)
#define LSM_LAB_CONTROL (9)
#define LSM_PARAMS_MAX (LSM_LAB_CONTROL + 1)

#define LSM_EVENT_NON_TIME_STAMP_MODE (0)
#define LSM_EVENT_TIME_STAMP_MODE (1)

#define LSM_DET_EVENT_TYPE_LEGACY (0)
#define LSM_DET_EVENT_TYPE_GENERIC (1)

/* Valid sample rates for input hw_params */
#define LSM_INPUT_SAMPLE_RATE_16K 16000
#define LSM_INPUT_SAMPLE_RATE_48K 48000

/* Valid bit-widths for input hw_params */
#define LSM_INPUT_BIT_WIDTH_16	16
#define LSM_INPUT_BIT_WIDTH_24	24

/* Min and Max channels for input hw_params */
#define LSM_INPUT_NUM_CHANNELS_MIN	1
#define LSM_INPUT_NUM_CHANNELS_MAX	9

enum lsm_app_id {
	LSM_VOICE_WAKEUP_APP_ID = 1,
	LSM_VOICE_WAKEUP_APP_ID_V2 = 2,
};

enum lsm_detection_mode {
	LSM_MODE_KEYWORD_ONLY_DETECTION = 1,
	LSM_MODE_USER_KEYWORD_DETECTION
};

enum lsm_vw_status {
	LSM_VOICE_WAKEUP_STATUS_RUNNING = 1,
	LSM_VOICE_WAKEUP_STATUS_DETECTED,
	LSM_VOICE_WAKEUP_STATUS_END_SPEECH,
	LSM_VOICE_WAKEUP_STATUS_REJECTED
};

/*
 * Data for LSM_ENDPOINT_DETECT_THRESHOLD param_type
 * @epd_begin: Begin threshold
 * @epd_end: End threshold
 */
struct snd_lsm_ep_det_thres {
	__u32 epd_begin;
	__u32 epd_end;
};

/*
 * Data for LSM_OPERATION_MODE param_type
 * @mode: The detection mode to be used
 * @detect_failure: Setting to enable failure detections.
 */
struct snd_lsm_detect_mode {
	enum lsm_detection_mode mode;
	bool detect_failure;
};

/*
 * Data for LSM_GAIN param_type
 * @gain: The gain to be applied on LSM
 */
struct snd_lsm_gain {
	__u16 gain;
};

/*
 * Data for LSM_POLLING_ENABLE param_type
 * @poll_en: Polling enable or disable
 */
struct snd_lsm_poll_enable {
	bool poll_en;
};

/*
 * Data for LSM_DET_EVENT_TYPE param_type
 * @event_type: LSM_DET_EVENT_TYPE_LEGACY or LSM_DET_EVENT_TYPE_GENERIC
 * @mode: Type of information in detection event payload
 */
struct snd_lsm_det_event_type {
	__u32 event_type;
	__u32 mode;
};

struct snd_lsm_sound_model_v2 {
	__u8 __user *data;
	__u8 *confidence_level;
	__u32 data_size;
	enum lsm_detection_mode detection_mode;
	__u8 num_confidence_levels;
	bool detect_failure;
};

struct snd_lsm_session_data {
	enum lsm_app_id app_id;
};

/*
 * Stage info for multi-stage session
 * @app_type: acdb app_type to be used to map topology/cal for the stage
 * @lpi_enable: low power island mode applicable for the stage
 */
struct snd_lsm_stage_info {
	__u32 app_type;
	__u32 lpi_enable;
};

/*
 * Session info for multi-stage session
 * @app_id: VoiceWakeup engine id, this is now used to just validate input arg
 * @num_stages: number of detection stages to be used
 * @stage_info: stage info for each of the stage being used, ordered by index
 */
struct snd_lsm_session_data_v2 {
	enum lsm_app_id app_id;
	__u32 num_stages;
	struct snd_lsm_stage_info stage_info[LSM_MAX_STAGES_PER_SESSION];
};

/*
 * Data for LSM_LAB_CONTROL param_type
 * @enable: lab enable or disable
 */
struct snd_lsm_lab_control {
	__u32 enable;
};

struct snd_lsm_event_status {
	__u16 status;
	__u16 payload_size;
	__u8 payload[0];
};

struct snd_lsm_event_status_v3 {
	__u32 timestamp_lsw;
	__u32 timestamp_msw;
	__u16 status;
	__u16 payload_size;
	__u8 payload[0];
};

struct snd_lsm_detection_params {
	__u8 *conf_level;
	enum lsm_detection_mode detect_mode;
	__u8 num_confidence_levels;
	bool detect_failure;
	bool poll_enable;
};

/*
 * Param info for each parameter type
 * @module_id: Module to which parameter is to be set
 * @param_id: Parameter that is to be set
 * @param_size: size (in number of bytes) for the data
 *		in param_data.
 *		For confidence levels, this is num_conf_levels
 *		For REG_SND_MODEL, this is size of sound model
 *		For CUSTOM_PARAMS, this is size of the entire blob of data
 * @param_data: Data for the parameter.
 *		For some param_types this is a structure defined, ex: LSM_GAIN
 *		For CONFIDENCE_LEVELS, this is array of confidence levels
 *		For REG_SND_MODEL, this is the sound model data
 *		For CUSTOM_PARAMS, this is the blob of custom data.
 * @param_type: Parameter type as defined in values upto LSM_PARAMS_MAX
 */
struct lsm_params_info {
	__u32 module_id;
	__u32 param_id;
	__u32 param_size;
	__u8 __user *param_data;
	uint32_t param_type;
};

/*
 * Param info(version 2) for each parameter type
 *
 * Existing member variables:
 * @module_id: Module to which parameter is to be set
 * @param_id: Parameter that is to be set
 * @param_size: size (in number of bytes) for the data
 *		in param_data.
 *		For confidence levels, this is num_conf_levels
 *		For REG_SND_MODEL, this is size of sound model
 *		For CUSTOM_PARAMS, this is size of the entire blob of data
 * @param_data: Data for the parameter.
 *		For some param_types this is a structure defined, ex: LSM_GAIN
 *		For CONFIDENCE_LEVELS, this is array of confidence levels
 *		For REG_SND_MODEL, this is the sound model data
 *		For CUSTOM_PARAMS, this is the blob of custom data.
 * @param_type: Parameter type as defined in values upto LSM_PARAMS_MAX
 *
 * Member variables applicable only to V2:
 * @instance_id: instance id of the param to which parameter is to be set
 * @stage_idx: detection stage for which the param is applicable
 */
struct lsm_params_info_v2 {
	__u32 module_id;
	__u32 param_id;
	__u32 param_size;
	__u8 __user *param_data;
	uint32_t param_type;
	__u16 instance_id;
	__u16 stage_idx;
};

/*
 * Data passed to the SET_PARAM_V2 IOCTL
 * @num_params: Number of params that are to be set
 *		should not be greater than LSM_PARAMS_MAX
 * @params: Points to an array of lsm_params_info
 *	    Each entry points to one parameter to set
 * @data_size: size (in bytes) for params
 *	       should be equal to
 *	       num_params * sizeof(struct lsm_parms_info)
 */
struct snd_lsm_module_params {
	__u8 __user *params;
	__u32 num_params;
	__u32 data_size;
};

/*
 * Data passed to LSM_OUT_FORMAT_CFG IOCTL
 * @format: The media format enum
 * @packing: indicates the packing method used for data path
 * @events: indicates whether data path events need to be enabled
 * @transfer_mode: indicates whether FTRT mode or RT mode.
 */
struct snd_lsm_output_format_cfg {
	__u8 format;
	__u8 packing;
	__u8 events;
	__u8 mode;
};

/*
 * Data passed to SNDRV_LSM_SET_INPUT_HW_PARAMS ioctl
 *
 * @sample_rate: Sample rate of input to lsm.
 *		 valid values are 16000 and 48000
 * @bit_width: Bit width of audio samples input to lsm.
 *	       valid values are 16 and 24
 * @num_channels: Number of channels input to lsm.
 *		  valid values are range from 1 to 16
 */
struct snd_lsm_input_hw_params {
	__u32 sample_rate;
	__u16 bit_width;
	__u16 num_channels;
} __packed;

#define SNDRV_LSM_DEREG_SND_MODEL _IOW('U', 0x01, int)
#define SNDRV_LSM_EVENT_STATUS	_IOW('U', 0x02, struct snd_lsm_event_status)
#define SNDRV_LSM_ABORT_EVENT	_IOW('U', 0x03, int)
#define SNDRV_LSM_START		_IOW('U', 0x04, int)
#define SNDRV_LSM_STOP		_IOW('U', 0x05, int)
#define SNDRV_LSM_SET_SESSION_DATA _IOW('U', 0x06, struct snd_lsm_session_data)
#define SNDRV_LSM_REG_SND_MODEL_V2 _IOW('U', 0x07,\
					struct snd_lsm_sound_model_v2)
#define SNDRV_LSM_LAB_CONTROL	_IOW('U', 0x08, uint32_t)
#define SNDRV_LSM_STOP_LAB	_IO('U', 0x09)
#define SNDRV_LSM_SET_PARAMS	_IOW('U', 0x0A, \
					struct snd_lsm_detection_params)
#define SNDRV_LSM_SET_MODULE_PARAMS	_IOW('U', 0x0B, \
					struct snd_lsm_module_params)
#define SNDRV_LSM_OUT_FORMAT_CFG _IOW('U', 0x0C, \
				      struct snd_lsm_output_format_cfg)
#define SNDRV_LSM_SET_PORT	_IO('U', 0x0D)
#define SNDRV_LSM_SET_FWK_MODE_CONFIG	_IOW('U', 0x0E, uint32_t)
#define SNDRV_LSM_EVENT_STATUS_V3	_IOW('U', 0x0F, \
					struct snd_lsm_event_status_v3)
#define SNDRV_LSM_GENERIC_DET_EVENT	_IOW('U', 0x10, struct snd_lsm_event_status)
#define SNDRV_LSM_SET_INPUT_HW_PARAMS	_IOW('U', 0x11,	\
					     struct snd_lsm_input_hw_params)
#define SNDRV_LSM_SET_SESSION_DATA_V2 _IOW('U', 0x12, \
					struct snd_lsm_session_data_v2)
#define SNDRV_LSM_SET_MODULE_PARAMS_V2 _IOW('U', 0x13, \
					struct snd_lsm_module_params)

#endif
