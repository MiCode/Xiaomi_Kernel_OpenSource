/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __EP92_H__
#define __EP92_H__

/* EP92 register addresses */
/* BI = Basic Info */
#define   EP92_BI_VENDOR_ID_0                   0x00
#define   EP92_BI_VENDOR_ID_1                   0x01
#define   EP92_BI_DEVICE_ID_0                   0x02
#define   EP92_BI_DEVICE_ID_1                   0x03
#define   EP92_BI_VERSION_NUM                   0x04
#define   EP92_BI_VERSION_YEAR                  0x05
#define   EP92_BI_VERSION_MONTH                 0x06
#define   EP92_BI_VERSION_DATE                  0x07
#define   EP92_BI_GENERAL_INFO_0                0x08
#define   EP92_BI_GENERAL_INFO_1                0x09
#define   EP92_BI_GENERAL_INFO_2                0x0A
#define   EP92_BI_GENERAL_INFO_3                0x0B
#define   EP92_BI_GENERAL_INFO_4                0x0C
#define   EP92_BI_GENERAL_INFO_5                0x0D
#define   EP92_BI_GENERAL_INFO_6                0x0E

#define   EP92_ISP_MODE_ENTER_ISP               0x0F

#define   EP92_GENERAL_CONTROL_0                0x10
#define   EP92_GENERAL_CONTROL_1                0x11
#define   EP92_GENERAL_CONTROL_2                0x12
#define   EP92_GENERAL_CONTROL_3                0x13
#define   EP92_GENERAL_CONTROL_4                0x14

#define   EP92_CEC_EVENT_CODE                   0x15
#define   EP92_CEC_EVENT_PARAM_1                0x16
#define   EP92_CEC_EVENT_PARAM_2                0x17
#define   EP92_CEC_EVENT_PARAM_3                0x18
#define   EP92_CEC_EVENT_PARAM_4                0x19
/*        RESERVED                              0x1A */
/*        ...                                   ...  */
/*        RESERVED                              0x1F */
#define   EP92_AUDIO_INFO_SYSTEM_STATUS_0       0x20
#define   EP92_AUDIO_INFO_SYSTEM_STATUS_1       0x21
#define   EP92_AUDIO_INFO_AUDIO_STATUS          0x22
#define   EP92_AUDIO_INFO_CHANNEL_STATUS_0      0x23
#define   EP92_AUDIO_INFO_CHANNEL_STATUS_1      0x24
#define   EP92_AUDIO_INFO_CHANNEL_STATUS_2      0x25
#define   EP92_AUDIO_INFO_CHANNEL_STATUS_3      0x26
#define   EP92_AUDIO_INFO_CHANNEL_STATUS_4      0x27
#define   EP92_AUDIO_INFO_ADO_INFO_FRAME_0      0x28
#define   EP92_AUDIO_INFO_ADO_INFO_FRAME_1      0x29
#define   EP92_AUDIO_INFO_ADO_INFO_FRAME_2      0x2A
#define   EP92_AUDIO_INFO_ADO_INFO_FRAME_3      0x2B
#define   EP92_AUDIO_INFO_ADO_INFO_FRAME_4      0x2C
#define   EP92_AUDIO_INFO_ADO_INFO_FRAME_5      0x2D

#define   EP92_OTHER_PACKETS_HDMI_VS_0          0x2E
#define   EP92_OTHER_PACKETS_HDMI_VS_1          0x2F
#define   EP92_OTHER_PACKETS_ACP_PACKET         0x30
#define   EP92_OTHER_PACKETS_AVI_INFO_FRAME_0   0x31
#define   EP92_OTHER_PACKETS_AVI_INFO_FRAME_1   0x32
#define   EP92_OTHER_PACKETS_AVI_INFO_FRAME_2   0x33
#define   EP92_OTHER_PACKETS_AVI_INFO_FRAME_3   0x34
#define   EP92_OTHER_PACKETS_AVI_INFO_FRAME_4   0x35
#define   EP92_OTHER_PACKETS_GC_PACKET_0        0x36
#define   EP92_OTHER_PACKETS_GC_PACKET_1        0x37
#define   EP92_OTHER_PACKETS_GC_PACKET_2        0x38

#define   EP92_MAX_REGISTER_ADDR                EP92_OTHER_PACKETS_GC_PACKET_2

/* shift/masks for register bits
 * GI = General Info
 * GC = General Control
 * AI = Audio Info
 */
#define EP92_GI_ADO_CHF_MASK        0x01
#define EP92_GI_CEC_ECF_MASK        0x02
#define EP92_GI_TX_HOT_PLUG_SHIFT   7
#define EP92_GI_TX_HOT_PLUG_MASK    0x80
#define EP92_GI_VIDEO_LATENCY_SHIFT 0
#define EP92_GI_VIDEO_LATENCY_MASK  0xff

#define EP92_GC_POWER_SHIFT      7
#define EP92_GC_POWER_MASK       0x80
#define EP92_GC_AUDIO_PATH_SHIFT 5
#define EP92_GC_AUDIO_PATH_MASK  0x20
#define EP92_GC_CEC_MUTE_SHIFT   1
#define EP92_GC_CEC_MUTE_MASK    0x02
#define EP92_GC_ARC_EN_SHIFT     0
#define EP92_GC_ARC_EN_MASK      0x01
#define EP92_GC_ARC_DIS_SHIFT    6
#define EP92_GC_ARC_DIS_MASK     0x40
#define EP92_GC_RX_SEL_SHIFT     0
#define EP92_GC_RX_SEL_MASK      0x07
#define EP92_GC_CEC_VOLUME_SHIFT 0
#define EP92_GC_CEC_VOLUME_MASK  0xff
#define EP92_GC_LINK_ON0_SHIFT   0
#define EP92_GC_LINK_ON0_MASK    0x01
#define EP92_GC_LINK_ON1_SHIFT   1
#define EP92_GC_LINK_ON1_MASK    0x02
#define EP92_GC_LINK_ON2_SHIFT   2
#define EP92_GC_LINK_ON2_MASK    0x04

#define EP92_AI_MCLK_ON_SHIFT    6
#define EP92_AI_MCLK_ON_MASK     0x40
#define EP92_AI_AVMUTE_SHIFT     5
#define EP92_AI_AVMUTE_MASK      0x20
#define EP92_AI_LAYOUT_SHIFT     0
#define EP92_AI_LAYOUT_MASK      0x01
#define EP92_AI_HBR_ADO_SHIFT    5
#define EP92_AI_HBR_ADO_MASK     0x20
#define EP92_AI_STD_ADO_SHIFT    3
#define EP92_AI_STD_ADO_MASK     0x08
#define EP92_AI_RATE_MASK        0x07
#define EP92_AI_NPCM_MASK        0x02
#define EP92_AI_PREEMPH_SHIFT    3
#define EP92_AI_PREEMPH_MASK     0x38
#define EP92_AI_CH_COUNT_MASK    0x07
#define EP92_AI_CH_ALLOC_MASK    0xff
#define EP92_AI_DSD_ADO_SHIFT    4
#define EP92_AI_DSD_ADO_MASK     0x10
#define EP92_AI_DSD_RATE_SHIFT   4
#define EP92_AI_DSD_RATE_MASK    0x30

#define EP92_2CHOICE_MASK        1
#define EP92_GC_CEC_VOLUME_MIN   0
#define EP92_GC_CEC_VOLUME_MAX   100
#define EP92_AI_RATE_MIN         0
#define EP92_AI_RATE_MAX         768000
#define EP92_AI_CH_COUNT_MIN     0
#define EP92_AI_CH_COUNT_MAX     8
#define EP92_AI_CH_ALLOC_MIN     0
#define EP92_AI_CH_ALLOC_MAX     0xff

#define EP92_STATUS_NO_SIGNAL           0
#define EP92_STATUS_AUDIO_ACTIVE        1

/* kcontrol storage indices */
enum {
	EP92_KCTL_POWER = 0,
	EP92_KCTL_AUDIO_PATH,
	EP92_KCTL_CEC_MUTE,
	EP92_KCTL_ARC_EN,
	EP92_KCTL_RX_SEL,
	EP92_KCTL_CEC_VOLUME,
	EP92_KCTL_STATE,
	EP92_KCTL_AVMUTE,
	EP92_KCTL_LAYOUT,
	EP92_KCTL_MODE,
	EP92_KCTL_RATE,
	EP92_KCTL_CH_COUNT,
	EP92_KCTL_CH_ALLOC,
	EP92_KCTL_MAX
};

int ep92_set_ext_mclk(struct snd_soc_codec *codec, uint32_t mclk_freq);

#endif /* __EP92_H__ */
