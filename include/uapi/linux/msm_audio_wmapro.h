#ifndef _UAPI_MSM_AUDIO_WMAPRO_H
#define _UAPI_MSM_AUDIO_WMAPRO_H

#define AUDIO_GET_WMAPRO_CONFIG  _IOR(AUDIO_IOCTL_MAGIC, \
	  (AUDIO_MAX_COMMON_IOCTL_NUM+0), unsigned)
#define AUDIO_SET_WMAPRO_CONFIG  _IOW(AUDIO_IOCTL_MAGIC, \
	  (AUDIO_MAX_COMMON_IOCTL_NUM+1), unsigned)

struct msm_audio_wmapro_config {
	unsigned short 	armdatareqthr;
	uint8_t         validbitspersample;
	uint8_t         numchannels;
	unsigned short  formattag;
	unsigned short  samplingrate;
	unsigned short  avgbytespersecond;
	unsigned short  asfpacketlength;
	unsigned short 	channelmask;
	unsigned short 	encodeopt;
	unsigned short	advancedencodeopt;
	uint32_t	advancedencodeopt2;
};
#endif /* _UAPI_MSM_AUDIO_WMAPRO_H */
