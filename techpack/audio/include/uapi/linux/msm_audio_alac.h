#ifndef _UAPI_MSM_AUDIO_ALAC_H
#define _UAPI_MSM_AUDIO_ALAC_H

#define AUDIO_GET_ALAC_CONFIG  _IOR(AUDIO_IOCTL_MAGIC, \
	  (AUDIO_MAX_COMMON_IOCTL_NUM+0), struct msm_audio_alac_config)
#define AUDIO_SET_ALAC_CONFIG  _IOW(AUDIO_IOCTL_MAGIC, \
	  (AUDIO_MAX_COMMON_IOCTL_NUM+1), struct msm_audio_alac_config)

struct msm_audio_alac_config {
	uint32_t frameLength;
	uint8_t compatVersion;
	uint8_t bitDepth;
	uint8_t pb; /* currently unused */
	uint8_t mb; /* currently unused */
	uint8_t kb; /* currently unused */
	uint8_t channelCount;
	uint16_t maxRun; /* currently unused */
	uint32_t maxSize;
	uint32_t averageBitRate;
	uint32_t sampleRate;
	uint32_t channelLayout;
};

#endif /* _UAPI_MSM_AUDIO_ALAC_H */
