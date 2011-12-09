#ifndef __MSM_AUDIO_AMRWB_H
#define __MSM_AUDIO_AMRWB_H

#include <linux/msm_audio.h>

#define AUDIO_GET_AMRWB_ENC_CONFIG _IOW(AUDIO_IOCTL_MAGIC, \
	(AUDIO_MAX_COMMON_IOCTL_NUM+0), \
	struct msm_audio_amrwb_enc_config)
#define AUDIO_SET_AMRWB_ENC_CONFIG  _IOR(AUDIO_IOCTL_MAGIC, \
	(AUDIO_MAX_COMMON_IOCTL_NUM+1), \
	struct msm_audio_amrwb_enc_config)

struct msm_audio_amrwb_enc_config {
	uint32_t band_mode;
	uint32_t dtx_enable;
	uint32_t frame_format;
};
#endif /* __MSM_AUDIO_AMRWB_H */
