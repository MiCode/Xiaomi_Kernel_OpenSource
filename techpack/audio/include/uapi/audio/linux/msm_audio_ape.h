/* The following structure has been taken
 * from Monkey's Audio SDK with permission
 */

#ifndef _UAPI_MSM_AUDIO_APE_H
#define _UAPI_MSM_AUDIO_APE_H

#include <linux/types.h>

#define AUDIO_GET_APE_CONFIG  _IOR(AUDIO_IOCTL_MAGIC, \
	  (AUDIO_MAX_COMMON_IOCTL_NUM+0), struct msm_audio_ape_config)
#define AUDIO_SET_APE_CONFIG  _IOW(AUDIO_IOCTL_MAGIC, \
	  (AUDIO_MAX_COMMON_IOCTL_NUM+1), struct msm_audio_ape_config)

struct msm_audio_ape_config {
	__u16 compatibleVersion;
	__u16 compressionLevel;
	__u32 formatFlags;
	__u32 blocksPerFrame;
	__u32 finalFrameBlocks;
	__u32 totalFrames;
	__u16 bitsPerSample;
	__u16 numChannels;
	__u32 sampleRate;
	__u32 seekTablePresent;
};

#endif /* _UAPI_MSM_AUDIO_APE_H */
