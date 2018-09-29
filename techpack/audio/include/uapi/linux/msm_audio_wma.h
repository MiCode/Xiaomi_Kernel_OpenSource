#ifndef _UAPI_MSM_AUDIO_WMA_H
#define _UAPI_MSM_AUDIO_WMA_H

#define AUDIO_GET_WMA_CONFIG  _IOR(AUDIO_IOCTL_MAGIC, \
	  (AUDIO_MAX_COMMON_IOCTL_NUM+0), unsigned int)
#define AUDIO_SET_WMA_CONFIG  _IOW(AUDIO_IOCTL_MAGIC, \
	  (AUDIO_MAX_COMMON_IOCTL_NUM+1), unsigned int)

#define AUDIO_GET_WMA_CONFIG_V2  _IOR(AUDIO_IOCTL_MAGIC, \
	  (AUDIO_MAX_COMMON_IOCTL_NUM+2), struct msm_audio_wma_config_v2)
#define AUDIO_SET_WMA_CONFIG_V2  _IOW(AUDIO_IOCTL_MAGIC, \
	  (AUDIO_MAX_COMMON_IOCTL_NUM+3), struct msm_audio_wma_config_v2)

struct msm_audio_wma_config {
	unsigned short	armdatareqthr;
	unsigned short	channelsdecoded;
	unsigned short	wmabytespersec;
	unsigned short	wmasamplingfreq;
	unsigned short	wmaencoderopts;
};

struct msm_audio_wma_config_v2 {
	unsigned short	format_tag;
	unsigned short	numchannels;
	uint32_t	samplingrate;
	uint32_t	avgbytespersecond;
	unsigned short	block_align;
	unsigned short  validbitspersample;
	uint32_t	channelmask;
	unsigned short	encodeopt;
};

#endif /* _UAPI_MSM_AUDIO_WMA_H */
