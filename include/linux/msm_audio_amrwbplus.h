#ifndef __MSM_AUDIO_AMR_WB_PLUS_H
#define __MSM_AUDIO_AMR_WB_PLUS_H

#define AUDIO_GET_AMRWBPLUS_CONFIG_V2  _IOR(AUDIO_IOCTL_MAGIC, \
	(AUDIO_MAX_COMMON_IOCTL_NUM+2), struct msm_audio_amrwbplus_config_v2)
#define AUDIO_SET_AMRWBPLUS_CONFIG_V2  _IOW(AUDIO_IOCTL_MAGIC, \
	(AUDIO_MAX_COMMON_IOCTL_NUM+3), struct msm_audio_amrwbplus_config_v2)

struct msm_audio_amrwbplus_config_v2 {
	unsigned int size_bytes;
	unsigned int version;
	unsigned int num_channels;
	unsigned int amr_band_mode;
	unsigned int amr_dtx_mode;
	unsigned int amr_frame_fmt;
	unsigned int amr_lsf_idx;
};
#endif /* __MSM_AUDIO_AMR_WB_PLUS_H */
