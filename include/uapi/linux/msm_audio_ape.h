#ifndef _UAPI_MSM_AUDIO_APE_H
#define _UAPI_MSM_AUDIO_APE_H

#define AUDIO_GET_APE_CONFIG  _IOR(AUDIO_IOCTL_MAGIC, \
	  (AUDIO_MAX_COMMON_IOCTL_NUM+0), struct msm_audio_ape_config)
#define AUDIO_SET_APE_CONFIG  _IOW(AUDIO_IOCTL_MAGIC, \
	  (AUDIO_MAX_COMMON_IOCTL_NUM+1), struct msm_audio_ape_config)

struct msm_audio_ape_config {
	uint16_t compatibleVersion;
	uint16_t compressionLevel;
	uint32_t formatFlags;
	uint32_t blocksPerFrame;
	uint32_t finalFrameBlocks;
	uint32_t totalFrames;
	uint16_t bitsPerSample;
	uint16_t numChannels;
	uint32_t sampleRate;
	uint32_t seekTablePresent;
};

#endif /* _UAPI_MSM_AUDIO_APE_H */
