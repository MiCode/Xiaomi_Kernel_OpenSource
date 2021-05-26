/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/******************************************************************************
 *
 *
 * Filename:
 * ---------
 *   auddrv-def.h
 *
 * Project:
 * --------
 *   Audio Driver
 *
 * Description:
 * ------------
 *   Audio register
 *
 * Author:
 * -------
 *   Chipeng Chang (MTK02308)
 *
 *---------------------------------------------------------------------------
 *
 *****************************************************************************
 */

#ifndef AUDIO_DEF_H
#define AUDIO_DEF_H

#include "mtk-auddrv-type-def.h"
#ifdef CONFIG_MTK_AEE_FEATURE
#include <mt-plat/aee.h>
#endif

#define PM_MANAGER_API
#define AUDIO_MEMORY_SRAM
#define AUDIO_MEM_IOREMAP
#define AUDIO_DL2_ISR_COPY_SUPPORT

/* if need assert , use AUDIO_ASSERT(true) */
#define AUDIO_ASSERT(value) WARN_ON(value)

#ifdef CONFIG_MTK_AEE_FEATURE
#define AUDIO_AEE(message)                                                     \
	(aee_kernel_exception_api(__FILE__, __LINE__, DB_OPT_FTRACE, message,  \
				  "audio dump ftrace"))
#else
#define AUDIO_AEE(message)
#endif

/**********************************
 *  Other Definitions             *
 **********************************/
#define BIT_00 0x00000001 /* ---- ---- ---- ---- ---- ---- ---- ---1 */
#define BIT_01 0x00000002 /* ---- ---- ---- ---- ---- ---- ---- --1- */
#define BIT_02 0x00000004 /* ---- ---- ---- ---- ---- ---- ---- -1-- */
#define BIT_03 0x00000008 /* ---- ---- ---- ---- ---- ---- ---- 1--- */
#define BIT_04 0x00000010 /* ---- ---- ---- ---- ---- ---- ---1 ---- */
#define BIT_05 0x00000020 /* ---- ---- ---- ---- ---- ---- --1- ---- */
#define BIT_06 0x00000040 /* ---- ---- ---- ---- ---- ---- -1-- ---- */
#define BIT_07 0x00000080 /* ---- ---- ---- ---- ---- ---- 1--- ---- */
#define BIT_08 0x00000100 /* ---- ---- ---- ---- ---- ---1 ---- ---- */
#define BIT_09 0x00000200 /* ---- ---- ---- ---- ---- --1- ---- ---- */
#define BIT_10 0x00000400 /* ---- ---- ---- ---- ---- -1-- ---- ---- */
#define BIT_11 0x00000800 /* ---- ---- ---- ---- ---- 1--- ---- ---- */
#define BIT_12 0x00001000 /* ---- ---- ---- ---- ---1 ---- ---- ---- */
#define BIT_13 0x00002000 /* ---- ---- ---- ---- --1- ---- ---- ---- */
#define BIT_14 0x00004000 /* ---- ---- ---- ---- -1-- ---- ---- ---- */
#define BIT_15 0x00008000 /* ---- ---- ---- ---- 1--- ---- ---- ---- */
#define BIT_16 0x00010000 /* ---- ---- ---- ---1 ---- ---- ---- ---- */
#define BIT_17 0x00020000 /* ---- ---- ---- --1- ---- ---- ---- ---- */
#define BIT_18 0x00040000 /* ---- ---- ---- -1-- ---- ---- ---- ---- */
#define BIT_19 0x00080000 /* ---- ---- ---- 1--- ---- ---- ---- ---- */
#define BIT_20 0x00100000 /* ---- ---- ---1 ---- ---- ---- ---- ---- */
#define BIT_21 0x00200000 /* ---- ---- --1- ---- ---- ---- ---- ---- */
#define BIT_22 0x00400000 /* ---- ---- -1-- ---- ---- ---- ---- ---- */
#define BIT_23 0x00800000 /* ---- ---- 1--- ---- ---- ---- ---- ---- */
#define BIT_24 0x01000000 /* ---- ---1 ---- ---- ---- ---- ---- ---- */
#define BIT_25 0x02000000 /* ---- --1- ---- ---- ---- ---- ---- ---- */
#define BIT_26 0x04000000 /* ---- -1-- ---- ---- ---- ---- ---- ---- */
#define BIT_27 0x08000000 /* ---- 1--- ---- ---- ---- ---- ---- ---- */
#define BIT_28 0x10000000 /* ---1 ---- ---- ---- ---- ---- ---- ---- */
#define BIT_29 0x20000000 /* --1- ---- ---- ---- ---- ---- ---- ---- */
#define BIT_30 0x40000000 /* -1-- ---- ---- ---- ---- ---- ---- ---- */
#define BIT_31 0x80000000 /* 1--- ---- ---- ---- ---- ---- ---- ---- */
#define MASK_ALL (0xFFFFFFFF)

/* cpu dai name */
#define MT_SOC_DAI_NAME "mt-soc-dai-driver"
#define MT_SOC_DL1DAI_NAME "mt-soc-dl1dai-driver"
#define MT_SOC_DL2DAI_NAME "mt-soc-dl2dai-driver"
#define MT_SOC_EXTSPKDAI_NAME "mt-soc-extspkdai-driver"
#define MT_SOC_DL1DATA2DAI_NAME "mt-soc-dl1data2dai-driver"
#define MT_SOC_UL1DAI_NAME "mt-soc-ul1dai-driver"
#define MT_SOC_UL1DATA2_NAME "mt-soc-ul1data2dai-driver"
#define MT_SOC_UL2DAI_NAME "mt-soc-ul2dai-driver"
#define MT_SOC_I2S0AWBDAI_NAME "mt-soc-i2s0awbdai-driver"
#define MT_SOC_I2S2ADC2DAI_NAME "mt-soc-i2s2adc2dai-driver"
#define MT_SOC_VOICE_MD1_NAME "mt-soc-voicemd1dai-driver"
#define MT_SOC_VOICE_MD1_BT_NAME "mt-soc-voicemd1-btdai-driver"
#define MT_SOC_VOICE_MD2_NAME "mt-soc-voicemd2dai-driver"
#define MT_SOC_VOICE_MD2_BT_NAME "mt-soc-voicemd2-btdai-driver"
#define MT_SOC_VOIP_CALL_BT_OUT_NAME "mt-soc-voipcall-btdai-out-driver"
#define MT_SOC_VOIP_CALL_BT_IN_NAME "mt-soc-voipcall-btdai-in-driver"
#define MT_SOC_ULDLLOOPBACK_NAME "mt-soc-uldlloopbackdai-driver"
#define MT_SOC_HDMI_NAME "mt-soc-hdmidai-driver"
#define MT_SOC_I2S0_NAME "mt-soc-i2s0dai-driver"
#define MT_SOC_I2S0DL1_NAME "mt-soc-i2s0dl1dai-driver"
#define MT_SOC_DL1SCPSPK_NAME "mt-soc-dl1scpspkdai-driver"
#define MT_SOC_SCPVOICE_NAME "mt-soc-scpvoicedai-driver"
#define MT_SOC_MRGRX_NAME "mt-soc-mrgrxdai-driver"
#define MT_SOC_MRGRXCAPTURE_NAME "mt-soc-mrgrxcapturedai-driver"
#define MT_SOC_DL1AWB_NAME "mt-soc-dl1awbdai-driver"
#define MT_SOC_FM_MRGTX_NAME "mt-soc-fmmrgtxdai-driver"
#define MT_SOC_TDMRX_NAME "mt-soc-tdmrxdai-driver"
#define MT_SOC_HP_IMPEDANCE_NAME "mt-soc-hpimpedancedai-driver"
#define MT_SOC_FM_I2S_NAME "mt-soc-fmi2S-driver"
#define MT_SOC_FM_I2S_CAPTURE_NAME "mt-soc-fmi2Scapturedai-driver"
#define MT_SOC_BTCVSD_RX_DAI_NAME "mt-soc-btcvsd-rx-dai-driver"
#define MT_SOC_BTCVSD_TX_DAI_NAME "mt-soc-btcvsd-tx-dai-driver"
#define MT_SOC_BTCVSD_DAI_NAME "mt-soc-btcvsd-dai-driver"
#define MT_SOC_MOD_DAI_NAME "mt-soc-moddai-driver"
#define MT_SOC_ANC_NAME "mt-soc-anc-driver"
#define MT_SOC_ANC_RECORD_DAI_NAME "mt-soc-anc-record-dai-driver"
#define MT_SOC_OFFLOAD_PLAYBACK_DAI_NAME "mt-soc-offload-playback-dai-driver"

/* platform name */
#define MT_SOC_DL1_PCM "mt-soc-dl1-pcm"
#define MT_SOC_HP_IMPEDANCE_PCM "mt-soc-hp-impedence-pcm"
#define MT_SOC_DEEP_BUFFER_DL_PCM "mt-soc-deep-buffer-dl-pcm"
#define MT_SOC_DL2_PCM "mt-soc-dl2-pcm"
#define MT_SOC_UL1_PCM "mt-soc-ul1-pcm"
#define MT_SOC_UL2_PCM "mt-soc-ul2-pcm"
#define MT_SOC_I2S0_AWB_PCM "mt-soc-i2s0awb-pcm"
#define MT_SOC_AWB_PCM "mt-soc-awb-pcm"
#define MT_SOC_MRGRX_AWB_PCM "mt-soc-mrgrx-awb-pcm"
#define MT_SOC_DL1_AWB_PCM "mt-soc-dl1-awb-pcm"
#define MT_SOC_DAI_PCM "mt-soc-DAI-pcm"
#define MT_SOC_HDMI_PCM "mt-soc-hdmi-pcm"
#define MT_SOC_I2S0_PCM "mt-soc-i2s0-pcm"
#define MT_SOC_MRGRX_PCM "mt-soc-mrgrx-pcm"
#define MT_SOC_SCP_VOICE_PCM "mt-soc-scp-voice-pcm"
#define MT_SOC_I2S0DL1_PCM "mt-soc-i2s0dl1-pcm"
#define MT_SOC_DL1SCPSPK_PCM "mt-soc-dl1scpspk-pcm"
#define MT_SOC_MODDAI_PCM "mt-soc-MODDAI-pcm"
#define MT_SOC_VOICE_MD1 "mt-soc-voicemd1"
#define MT_SOC_VOICE_MD2 "mt-soc-voicemd2"
#define MT_SOC_VOICE_MD1_BT "mt-soc-voicemd1-bt"
#define MT_SOC_VOICE_MD2_BT "mt-soc-voicemd2-bt"
#define MT_SOC_VOICE_ULTRA "mt-soc-voice-ultra"
#define MT_SOC_VOICE_USB "mt-soc-voice-usb"
#define MT_SOC_VOICE_USB_ECHOREF "mt-soc-voice-usb-echoref"
#define MT_SOC_VOIP_BT_OUT "mt-soc-voip-bt-out"
#define MT_SOC_VOIP_BT_IN "mt-soc-voip-bt-in"
#define MT_SOC_IFMI2S2 "mt-soc-fm-i2s2"
#define MT_SOC_DUMMY_PCM "mt-soc-dummy-pcm"
#define MT_SOC_ULDLLOOPBACK_PCM "mt-soc-uldlloopback-pcm"
#define MT_SOC_ROUTING_PCM "mt-soc-routing-pcm"
#define MT_SOC_FM_MRGTX_PCM "mt-soc-fmmrgtx-pcm"
#define MT_SOC_TDMRX_PCM "mt-soc-tdmrx-pcm"
#define MT_SOC_MOD_ADCI2S_PCM "mt-soc-mod2adci2s-pcm"
#define MT_SOC_I2S2_ADC2_PCM "mt-soc-i2s2_adc2-pcm"
#define MT_SOC_IO2_DAI_PCM "mt-soc-io2dai-pcm"
#define MT_SOC_FM_I2S_PCM "mt-soc-fm-i2s-pcm"
#define MT_SOC_FM_I2S_AWB_PCM "mt-soc-fm-i2s-awb-pcm"
#define MT_SOC_BTCVSD_RX_PCM "mt-soc-btcvsd-rx-pcm"
#define MT_SOC_BTCVSD_TX_PCM "mt-soc-btcvsd-tx-pcm"
#define MT_SOC_MOD_DAI_PCM "mt-soc-MODDAI-pcm"
#define MT_SOC_ANC_PCM "mt-soc-anc-pcm"
#define MT_SOC_PLAYBACK_OFFLOAD "mt-soc-playback-offload"

/* codec dai name */
#define MT_SOC_CODEC_TXDAI_NAME "mt-soc-codec-tx-dai"
#define MT_SOC_CODEC_TXDAI2_NAME "mt-soc-codec-tx-dai2"
#define MT_SOC_CODEC_RXDAI_NAME "mt-soc-codec-rx-dai"
#define MT_SOC_CODEC_RXDAI2_NAME "mt-soc-codec-rx-dai2"
#define MT_SOC_CODEC_I2S0AWB_NAME "mt-soc-codec-i2s0awb-dai"
#define MT_SOC_CODEC_I2S0TXDAI_NAME "mt-soc-codec-I2s0tx-dai"
#define MT_SOC_CODEC_DEEPBUFFER_TX_DAI_NAME "mt-soc-codec-deepbuffer-tx-dai"
#define MT_SOC_CODEC_SPKSCPTXDAI_NAME "mt-soc-codec-spkscptx-dai"
#define MT_SOC_CODEC_DL1AWBDAI_NAME "mt-soc-codec-dl1awb-dai"
#define MT_SOC_CODEC_VOICE_MD1DAI_NAME "mt-soc-codec-voicemd1-dai"
#define MT_SOC_CODEC_VOICE_MD2DAI_NAME "mt-soc-codec-voicemd2-dai"
#define MT_SOC_CODEC_VOICE_MD1_BTDAI_NAME "mt-soc-codec-voicemd1-bt-dai"
#define MT_SOC_CODEC_VOICE_MD2_BTDAI_NAME "mt-soc-codec-voicemd2-bt-dai"
#define MT_SOC_CODEC_VOICE_ULTRADAI_NAME "mt-soc-codec-voiceultra-dai"
#define MT_SOC_CODEC_VOICE_USBDAI_NAME "mt-soc-codec-voiceusb-dai"
#define MT_SOC_CODEC_VOICE_USB_ECHOREF_DAI_NAME                                \
	"mt-soc-codec-voiceusb-echoref-dai"
#define MT_SOC_CODEC_VOIPCALLBTOUTDAI_NAME "mt-soc-codec-voipcall-btout-dai"
#define MT_SOC_CODEC_VOIPCALLBTINDAI_NAME "mt-soc-codec-voipcall-btin-dai"
#define MT_SOC_CODEC_TDMRX_DAI_NAME "mt-soc-tdmrx-dai-codec"
#define MT_SOC_CODEC_HP_IMPEDANCE_NAME "mt-soc-codec-hp-impedance-dai"
#define MT_SOC_CODEC_OFFLOAD_NAME "mt-soc-codec-offload-dai"
#define MT_SOC_CODEC_ANC_NAME "mt-soc-codec-anc-dai"

#define MT_SOC_CODEC_FMI2S2TXDAI_NAME "mt-soc-codec-fmi2s2tx-dai"
#define MT_SOC_CODEC_FMI2S2RXDAI_NAME "mt-soc-codec-fmi2s2rx-dai"
#define MT_SOC_CODEC_ULDLLOOPBACK_NAME "mt-soc-codec-uldlloopback-dai"
#define MT_SOC_ROUTING_DAI_NAME "Routing-Control"
#define MT_SOC_CODEC_STUB_NAME "mt-soc-codec-stub"
#define MT_SOC_CODEC_NAME "mt-soc-codec"
#define MT_SOC_CODEC_DUMMY_NAME "mt-soc-dummy-codec"
#define MT_SOC_CODEC_DUMMY_DAI_NAME "mt-soc-dummy-dai-codec"
#define MT_SOC_CODEC_HDMI_DUMMY_DAI_NAME "mt-soc-hdmi-dummy-dai-codec"
#define MT_SOC_CODEC_I2S0_DUMMY_DAI_NAME "mt-soc-i2s0-dummy-dai-codec"
#define MT_SOC_CODEC_MRGRX_DUMMY_DAI_NAME "mt-soc-mrgrx-dummy-dai-codec"
#define MT_SOC_CODEC_MRGRX_DAI_NAME "mt-soc-mrgrx-dai-codec"
#define MT_SOC_CODEC_FMMRGTXDAI_DUMMY_DAI_NAME "mt-soc-fmmrg2tx-dummy-dai-codec"
#define MT_SOC_CODEC_FM_I2S_DUMMY_DAI_NAME "mt-soc-fm-i2s-dummy-dai-codec"
#define MT_SOC_CODEC_FM_I2S_DAI_NAME "mt-soc-fm-i2s-dai-codec"
#define MT_SOC_CODEC_BTCVSD_RX_DAI_NAME "mt-soc-codec-btcvsd-rx-dai"
#define MT_SOC_CODEC_BTCVSD_TX_DAI_NAME "mt-soc-codec-btcvsd-tx-dai"
#define MT_SOC_CODEC_BTCVSD_DAI_NAME "mt-soc-codec-btcvsd-dai"
#define MT_SOC_CODEC_MOD_DAI_NAME "mt-soc-mod-dai-codec"

/* stream name */
#define MT_SOC_DL1_STREAM_NAME "MultiMedia1_PLayback"
#define MT_SOC_SPEAKER_STREAM_NAME "Speaker_PLayback"
#define MT_SOC_HEADPHONE_STREAM_NAME "Headphone_PLayback"
#define MT_SOC_DEEP_BUFFER_DL_STREAM_NAME "Deep_Buffer_PLayback"
#define MT_SOC_DL2_STREAM_NAME "MultiMedia2_PLayback"
#define MT_SOC_DL3_STREAM_NAME "MultiMedia3_PLayback"
#define MT_SOC_VOICE_MD1_STREAM_NAME "Voice_MD1_PLayback"
#define MT_SOC_VOICE_MD2_STREAM_NAME "Voice_MD2_PLayback"
#define MT_SOC_VOICE_MD1_BT_STREAM_NAME "Voice_MD1_BT_Playback"
#define MT_SOC_VOICE_MD2_BT_STREAM_NAME "Voice_MD2_BT_Playback"
#define MT_SOC_VOICE_ULTRA_STREAM_NAME "Voice_ULTRA_PLayback"
#define MT_SOC_VOICE_USB_STREAM_NAME "Voice_USB_PLayback"
#define MT_SOC_VOICE_USB_ECHOREF_STREAM_NAME "Voice_USB_EchoRef"
#define MT_SOC_VOIP_BT_OUT_STREAM_NAME "VOIP_Call_BT_Playback"
#define MT_SOC_VOIP_BT_IN_STREAM_NAME "VOIP_Call_BT_Capture"
#define MT_SOC_HDMI_STREAM_NAME "HMDI_PLayback"
#define MT_SOC_I2S0_STREAM_NAME "I2S0_PLayback"
#define MT_SOC_I2SDL1_STREAM_NAME "I2S0DL1_PLayback"
#define MT_SOC_DL1SCPSPK_STREAM_NAME "DL1SCPSPK_PLayback"
#define MT_SOC_SCPVOICE_STREAM_NAME "SCPVoice_PLayback"
#define MT_SOC_MRGRX_STREAM_NAME "MRGRX_PLayback"
#define MT_SOC_MRGRX_CAPTURE_STREAM_NAME "MRGRX_CAPTURE"
#define MT_SOC_FM_I2S2_STREAM_NAME "FM_I2S2_PLayback"
#define MT_SOC_ULDLLOOPBACK_STREAM_NAME "ULDL_Loopback"
#define MT_SOC_FM_I2S2_RECORD_STREAM_NAME "FM_I2S2_Record"
#define MT_SOC_DL1_AWB_RECORD_STREAM_NAME "DL1_AWB_Record"
#define MT_SOC_UL1_STREAM_NAME "MultiMedia1_Capture"
#define MT_SOC_UL1DATA2_STREAM_NAME "MultiMediaData2_Capture"
#define MT_SOC_I2S0AWB_STREAM_NAME "I2S0AWB_Capture"
#define MT_SOC_I2S2ADC2_STREAM_NAME "I2S2ADC2_Capture"
#define MT_SOC_AWB_STREAM_NAME "MultiMedia_awb_Capture"
#define MT_SOC_DAI_STREAM_NAME "MultiMedia_dai_Capture"
#define MT_SOC_MODDAI_STREAM_NAME "Voice_Dai_Capture"
#define MT_SOC_ROUTING_STREAM_NAME "MultiMedia_Routing"
#define MT_SOC_HP_IMPEDANCE_STREAM_NAME "HP_IMPEDANCE_Playback"
#define MT_SOC_FM_MRGTX_STREAM_NAME "FM_MRGTX_Playback"
#define MT_SOC_TDM_CAPTURE_STREAM_NAME "TDM_Debug_Record"
#define MT_SOC_FM_I2S_PLAYBACK_STREAM_NAME "FM_I2S_Playback"
#define MT_SOC_FM_I2S_CAPTURE_STREAM_NAME "FM_I2S_Capture"
#define MT_SOC_BTCVSD_CAPTURE_STREAM_NAME "BTCVSD_Capture"
#define MT_SOC_BTCVSD_PLAYBACK_STREAM_NAME "BTCVSD_Playback"
#define MT_SOC_OFFLOAD_STREAM_NAME "Offload_Playback"
#define MT_SOC_ANC_STREAM_NAME "ANC_Playback"
#define MT_SOC_ANC_RECORD_STREAM_NAME "ANC_Record"

#endif
