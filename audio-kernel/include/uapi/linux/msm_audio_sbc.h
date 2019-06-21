#ifndef _UAPI_MSM_AUDIO_SBC_H
#define _UAPI_MSM_AUDIO_SBC_H

#include <linux/msm_audio.h>

#define AUDIO_SET_SBC_ENC_CONFIG  _IOW(AUDIO_IOCTL_MAGIC, \
	(AUDIO_MAX_COMMON_IOCTL_NUM+0), struct msm_audio_sbc_enc_config)

#define AUDIO_GET_SBC_ENC_CONFIG  _IOR(AUDIO_IOCTL_MAGIC, \
	(AUDIO_MAX_COMMON_IOCTL_NUM+1), struct msm_audio_sbc_enc_config)

#define AUDIO_SBC_BA_LOUDNESS		0x0
#define AUDIO_SBC_BA_SNR		0x1

#define AUDIO_SBC_MODE_MONO		0x0
#define AUDIO_SBC_MODE_DUAL		0x1
#define AUDIO_SBC_MODE_STEREO		0x2
#define AUDIO_SBC_MODE_JSTEREO		0x3

#define AUDIO_SBC_BANDS_8		0x1

#define AUDIO_SBC_BLOCKS_4		0x0
#define AUDIO_SBC_BLOCKS_8		0x1
#define AUDIO_SBC_BLOCKS_12		0x2
#define AUDIO_SBC_BLOCKS_16		0x3

struct msm_audio_sbc_enc_config {
	uint32_t channels;
	uint32_t sample_rate;
	uint32_t bit_allocation;
	uint32_t number_of_subbands;
	uint32_t number_of_blocks;
	uint32_t bit_rate;
	uint32_t mode;
};
#endif /* _UAPI_MSM_AUDIO_SBC_H */
