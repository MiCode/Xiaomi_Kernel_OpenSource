#ifndef __LSM_PARAMS_H__
#define __LSM_PARAMS_H__

#include <linux/types.h>
#include <sound/asound.h>

#define SNDRV_LSM_VERSION SNDRV_PROTOCOL_VERSION(0, 1, 0)

enum lsm_detection_mode {
	LSM_MODE_KEYWORD_ONLY_DETECTION = 1,
	LSM_MODE_USER_KEYWORD_DETECTION
};

struct snd_lsm_sound_model {
	__u8 *data;
	__u32 data_size;
	enum lsm_detection_mode detection_mode;
	__u16 min_keyw_confidence;
	__u16 min_user_confidence;
	bool detect_failure;
};

struct snd_lsm_event_status {
	__u16 status;
	__u16 payload_size;
	__u8 payload[0];
};

#define SNDRV_LSM_REG_SND_MODEL	 _IOW('U', 0x00, struct snd_lsm_sound_model)
#define SNDRV_LSM_DEREG_SND_MODEL _IOW('U', 0x01, int)
#define SNDRV_LSM_EVENT_STATUS	_IOW('U', 0x02, struct snd_lsm_event_status)
#define SNDRV_LSM_ABORT_EVENT	_IOW('U', 0x03, int)
#define SNDRV_LSM_START		_IOW('U', 0x04, int)
#define SNDRV_LSM_STOP		_IOW('U', 0x05, int)

#endif
