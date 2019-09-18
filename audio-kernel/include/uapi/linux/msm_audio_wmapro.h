#ifndef _UAPI_MSM_AUDIO_WMAPRO_H
#define _UAPI_MSM_AUDIO_WMAPRO_H

#define AUDIO_GET_WMAPRO_CONFIG  _IOR(AUDIO_IOCTL_MAGIC, \
	  (AUDIO_MAX_COMMON_IOCTL_NUM+0), struct msm_audio_wmapro_config)
#define AUDIO_SET_WMAPRO_CONFIG  _IOW(AUDIO_IOCTL_MAGIC, \
	  (AUDIO_MAX_COMMON_IOCTL_NUM+1), struct msm_audio_wmapro_config)

struct msm_audio_wmapro_config {
	unsigned short  armdatareqthr;
	uint8_t         validbitspersample;
	uint8_t         numchannels;
	unsigned short  formattag;
	uint32_t        samplingrate;
	uint32_t        avgbytespersecond;
	unsigned short  asfpacketlength;
	uint32_t        channelmask;
	unsigned short  encodeopt;
	unsigned short  advancedencodeopt;
	uint32_t        advancedencodeopt2;
};
#endif /* _UAPI_MSM_AUDIO_WMAPRO_H */
