#ifndef _UAPI_MSM_AUDIO_WMAPRO_H
#define _UAPI_MSM_AUDIO_WMAPRO_H

#include <linux/types.h>

#define AUDIO_GET_WMAPRO_CONFIG  _IOR(AUDIO_IOCTL_MAGIC, \
	  (AUDIO_MAX_COMMON_IOCTL_NUM+0), struct msm_audio_wmapro_config)
#define AUDIO_SET_WMAPRO_CONFIG  _IOW(AUDIO_IOCTL_MAGIC, \
	  (AUDIO_MAX_COMMON_IOCTL_NUM+1), struct msm_audio_wmapro_config)

struct msm_audio_wmapro_config {
	unsigned short  armdatareqthr;
	__u8         validbitspersample;
	__u8         numchannels;
	unsigned short  formattag;
	__u32        samplingrate;
	__u32        avgbytespersecond;
	unsigned short  asfpacketlength;
	__u32        channelmask;
	unsigned short  encodeopt;
	unsigned short  advancedencodeopt;
	__u32        advancedencodeopt2;
};
#endif /* _UAPI_MSM_AUDIO_WMAPRO_H */
