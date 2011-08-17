/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MSM_AUDIO_QCP_H
#define __MSM_AUDIO_QCP_H

#include <linux/msm_audio.h>

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
	uint32_t cdma_rate;
	uint32_t min_bit_rate;
	uint32_t max_bit_rate;
};

struct msm_audio_evrc_enc_config {
	uint32_t cdma_rate;
	uint32_t min_bit_rate;
	uint32_t max_bit_rate;
};

#endif /* __MSM_AUDIO_QCP_H */
