#ifndef _UAPI_MSM_AUDIO_AC3_H
#define _UAPI_MSM_AUDIO_AC3_H

#include <linux/msm_audio.h>

#define AUDIO_SET_AC3_CONFIG  _IOW(AUDIO_IOCTL_MAGIC, \
	(AUDIO_MAX_COMMON_IOCTL_NUM+0), unsigned)
#define AUDIO_GET_AC3_CONFIG  _IOR(AUDIO_IOCTL_MAGIC, \
	(AUDIO_MAX_COMMON_IOCTL_NUM+1), unsigned)

#define AUDAC3_DEF_WORDSIZE 0
#define AUDAC3_DEF_USER_DOWNMIX_FLAG 0x0
#define AUDAC3_DEF_USER_KARAOKE_FLAG 0x0
#define AUDAC3_DEF_ERROR_CONCEALMENT 0
#define AUDAC3_DEF_MAX_REPEAT_COUNT  0

struct msm_audio_ac3_config {
	unsigned short		numChans;
	unsigned short		wordSize;
	unsigned short		kCapableMode;
	unsigned short		compMode;
	unsigned short		outLfeOn;
	unsigned short		outputMode;
	unsigned short		stereoMode;
	unsigned short		dualMonoMode;
	unsigned short		fsCod;
	unsigned short		pcmScaleFac;
	unsigned short		dynRngScaleHi;
	unsigned short		dynRngScaleLow;
	unsigned short		user_downmix_flag;
	unsigned short		user_karaoke_flag;
	unsigned short		dm_address_high;
	unsigned short		dm_address_low;
	unsigned short		ko_address_high;
	unsigned short		ko_address_low;
	unsigned short		error_concealment;
	unsigned short		max_rep_count;
	unsigned short		channel_routing_mode[6];
};

#endif /* _UAPI_MSM_AUDIO_AC3_H */
