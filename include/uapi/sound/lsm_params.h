#ifndef _UAPI_LSM_PARAMS_H__
#define _UAPI_LSM_PARAMS_H__

#include <linux/types.h>
#include <sound/asound.h>

#define SNDRV_LSM_VERSION SNDRV_PROTOCOL_VERSION(0, 1, 0)

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

struct snd_lsm_sound_model {
	__u8 __user *data;
	__u32 data_size;
	enum lsm_detection_mode detection_mode;
	__u16 min_keyw_confidence;
	__u16 min_user_confidence;
	bool detect_failure;
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

struct snd_lsm_event_status {
	__u16 status;
	__u16 payload_size;
	__u8 payload[0];
};

struct snd_lsm_detection_params {
	__u8 *conf_level;
	enum lsm_detection_mode detect_mode;
	__u8 num_confidence_levels;
	bool detect_failure;
};

#define SNDRV_LSM_REG_SND_MODEL	 _IOW('U', 0x00, struct snd_lsm_sound_model)
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

#endif
