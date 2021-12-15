#ifndef _UAPI_MSM_AUDIO_AMRWB_H
#define _UAPI_MSM_AUDIO_AMRWB_H

#include <audio/linux/msm_audio.h>
#include <linux/types.h>

#define AUDIO_GET_AMRWB_ENC_CONFIG _IOW(AUDIO_IOCTL_MAGIC, \
	(AUDIO_MAX_COMMON_IOCTL_NUM+0), \
	struct msm_audio_amrwb_enc_config)
#define AUDIO_SET_AMRWB_ENC_CONFIG  _IOR(AUDIO_IOCTL_MAGIC, \
	(AUDIO_MAX_COMMON_IOCTL_NUM+1), \
	struct msm_audio_amrwb_enc_config)

struct msm_audio_amrwb_enc_config {
	__u32 band_mode;
	__u32 dtx_enable;
	__u32 frame_format;
};
#endif /* _UAPI_MSM_AUDIO_AMRWB_H */
