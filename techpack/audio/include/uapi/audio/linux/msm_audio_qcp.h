#ifndef _UAPI_MSM_AUDIO_QCP_H
#define _UAPI_MSM_AUDIO_QCP_H

#include <audio/linux/msm_audio.h>
#include <linux/types.h>

#define AUDIO_SET_QCELP_ENC_CONFIG  _IOW(AUDIO_IOCTL_MAGIC, \
	0, struct msm_audio_qcelp_enc_config)

#define AUDIO_GET_QCELP_ENC_CONFIG  _IOR(AUDIO_IOCTL_MAGIC, \
	1, struct msm_audio_qcelp_enc_config)

#define AUDIO_SET_EVRC_ENC_CONFIG  _IOW(AUDIO_IOCTL_MAGIC, \
	2, struct msm_audio_evrc_enc_config)

#define AUDIO_GET_EVRC_ENC_CONFIG  _IOR(AUDIO_IOCTL_MAGIC, \
	3, struct msm_audio_evrc_enc_config)

#define CDMA_RATE_BLANK		0x00
#define CDMA_RATE_EIGHTH	0x01
#define CDMA_RATE_QUARTER	0x02
#define CDMA_RATE_HALF		0x03
#define CDMA_RATE_FULL		0x04
#define CDMA_RATE_ERASURE	0x05

struct msm_audio_qcelp_enc_config {
	__u32 cdma_rate;
	__u32 min_bit_rate;
	__u32 max_bit_rate;
};

struct msm_audio_evrc_enc_config {
	__u32 cdma_rate;
	__u32 min_bit_rate;
	__u32 max_bit_rate;
};

#endif /* _UAPI_MSM_AUDIO_QCP_H */
