/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef AUDIO_DEF_H
#define AUDIO_DEF_H

/*#include "AudDrv_Type_Def.h"*/
#define COMMON_CLOCK_FRAMEWORK_API
/*#define IDLE_TASK_DRIVER_API*/
/*#ifdef CONFIG_MTK_CLKMGR*/
#define PM_MANAGER_API
/*#endif*/
#define AUDIO_MEMORY_SRAM
#define AUDIO_MEM_IOREMAP
#define AUDIO_IOREMAP_FROM_DT

/* below for audio debugging */
/* #define DEBUG_AUDDRV */
/* #define DEBUG_AFE_REG */
/* #define DEBUG_ANA_REG */
/* #define DEBUG_AUD_CLK */
/* #define DEBUG_AUD_HDMI */
/* #define DEBUG_AUD_FMTX */
/* #define DEBUG_AUD_UL2 */
/* #define DEBUG_AUD_UL1 */
/* #define DEBUG_AUD_DL1 */
/* #define DEBUG_AUD_DAI */

#ifdef DEBUG_AUDDRV
#define PRINTK_AUDDRV(format, args...) pr_debug(format, ##args)
#else
#define PRINTK_AUDDRV(format, args...)
#endif

#ifdef DEBUG_AFE_REG
#define PRINTK_AFE_REG(format, args...) pr_debug(format, ##args)
#else
#define PRINTK_AFE_REG(format, args...)
#endif

#ifdef DEBUG_ANA_REG
#define PRINTK_ANA_REG(format, args...) pr_debug(format, ##args)
#else
#define PRINTK_ANA_REG(format, args...)
#endif

#ifdef DEBUG_AUD_CLK
#define PRINTK_AUD_CLK(format, args...)  pr_debug(format, ##args)
#else
#define PRINTK_AUD_CLK(format, args...)
#endif

#ifdef DEBUG_AUD_DL1
#define PRINTK_AUD_DL1(format, args...)  pr_debug(format, ##args)
#else
#define PRINTK_AUD_DL1(format, args...)
#endif

#ifdef DEBUG_AUD_DL2
#define PRINTK_AUD_DL2(format, args...)  pr_debug(format, ##args)
#else
#define PRINTK_AUD_DL2(format, args...)
#endif

#ifdef DEBUG_AUD_FMTX
#define PRINTK_AUD_FMTX(format, args...)  pr_debug(format, ##args)
#else
#define PRINTK_AUD_FMTX(format, args...)
#endif

#ifdef DEBUG_AUD_HDMI
#define PRINTK_AUD_HDMI(format, args...)  pr_debug(format, ##args)
#else
#define PRINTK_AUD_HDMI(format, args...)
#endif

#ifdef DEBUG_AUD_HDMI2
#define PRINTK_AUD_HDMI2(format, args...)  pr_debug(format, ##args)
#else
#define PRINTK_AUD_HDMI2(format, args...)
#endif


#ifdef DEBUG_AUD_UL1
#define PRINTK_AUD_UL1(format, args...)  pr_debug(format, ##args)
#else
#define PRINTK_AUD_UL1(format, args...)
#endif

#ifdef DEBUG_AUD_UL2
#define PRINTK_AUD_UL2(format, args...)  pr_debug(format, ##args)
#else
#define PRINTK_AUD_UL2(format, args...)
#endif

#ifdef DEBUG_AUD_UL3
#define PRINTK_AUD_UL3(format, args...)  pr_debug(format, ##args)
#else
#define PRINTK_AUD_UL3(format, args...)
#endif

#ifdef DEBUG_AUD_AWB
#define PRINTK_AUD_AWB(format, args...)  pr_debug(format, ##args)
#else
#define PRINTK_AUD_AWB(format, args...)
#endif

#ifdef DEBUG_AUD_DAI
#define PRINTK_AUD_DAI(format, args...)  pr_debug(format, ##args)
#else
#define PRINTK_AUD_DAI(format, args...)
#endif

#ifdef DEBUG_AUD_MODDAI
#define PRINTK_AUD_MODDAI(format, args...)  pr_debug(format, ##args)
#else
#define PRINTK_AUD_MODDAI(format, args...)
#endif

#ifdef DEBUG_AUD_DAI
#define PRINTK_AUD_DAI(format, args...)  pr_debug(format, ##args)
#else
#define PRINTK_AUD_DAI(format, args...)
#endif


#define PRINTK_AUD_ERROR(format, args...)  pr_debug(format, ##args)

/* if need assert , use AUDIO_ASSERT(true) */
#define AUDIO_ASSERT(value) BUG_ON(false)


/**********************************
 *  Other Definitions             *
 **********************************/
#define BIT_00	0x00000001        /* ---- ---- ---- ---- ---- ---- ---- ---1 */
#define BIT_01	0x00000002        /* ---- ---- ---- ---- ---- ---- ---- --1- */
#define BIT_02	0x00000004        /* ---- ---- ---- ---- ---- ---- ---- -1-- */
#define BIT_03	0x00000008        /* ---- ---- ---- ---- ---- ---- ---- 1--- */
#define BIT_04	0x00000010        /* ---- ---- ---- ---- ---- ---- ---1 ---- */
#define BIT_05	0x00000020        /* ---- ---- ---- ---- ---- ---- --1- ---- */
#define BIT_06	0x00000040        /* ---- ---- ---- ---- ---- ---- -1-- ---- */
#define BIT_07	0x00000080        /* ---- ---- ---- ---- ---- ---- 1--- ---- */
#define BIT_08	0x00000100        /* ---- ---- ---- ---- ---- ---1 ---- ---- */
#define BIT_09	0x00000200        /* ---- ---- ---- ---- ---- --1- ---- ---- */
#define BIT_10	0x00000400        /* ---- ---- ---- ---- ---- -1-- ---- ---- */
#define BIT_11	0x00000800        /* ---- ---- ---- ---- ---- 1--- ---- ---- */
#define BIT_12	0x00001000        /* ---- ---- ---- ---- ---1 ---- ---- ---- */
#define BIT_13	0x00002000        /* ---- ---- ---- ---- --1- ---- ---- ---- */
#define BIT_14	0x00004000        /* ---- ---- ---- ---- -1-- ---- ---- ---- */
#define BIT_15	0x00008000        /* ---- ---- ---- ---- 1--- ---- ---- ---- */
#define BIT_16	0x00010000        /* ---- ---- ---- ---1 ---- ---- ---- ---- */
#define BIT_17	0x00020000        /* ---- ---- ---- --1- ---- ---- ---- ---- */
#define BIT_18	0x00040000        /* ---- ---- ---- -1-- ---- ---- ---- ---- */
#define BIT_19	0x00080000        /* ---- ---- ---- 1--- ---- ---- ---- ---- */
#define BIT_20	0x00100000        /* ---- ---- ---1 ---- ---- ---- ---- ---- */
#define BIT_21	0x00200000        /* ---- ---- --1- ---- ---- ---- ---- ---- */
#define BIT_22	0x00400000        /* ---- ---- -1-- ---- ---- ---- ---- ---- */
#define BIT_23	0x00800000        /* ---- ---- 1--- ---- ---- ---- ---- ---- */
#define BIT_24	0x01000000        /* ---- ---1 ---- ---- ---- ---- ---- ---- */
#define BIT_25	0x02000000        /* ---- --1- ---- ---- ---- ---- ---- ---- */
#define BIT_26	0x04000000        /* ---- -1-- ---- ---- ---- ---- ---- ---- */
#define BIT_27	0x08000000        /* ---- 1--- ---- ---- ---- ---- ---- ---- */
#define BIT_28	0x10000000        /* ---1 ---- ---- ---- ---- ---- ---- ---- */
#define BIT_29	0x20000000        /* --1- ---- ---- ---- ---- ---- ---- ---- */
#define BIT_30	0x40000000        /* -1-- ---- ---- ---- ---- ---- ---- ---- */
#define BIT_31	0x80000000        /* 1--- ---- ---- ---- ---- ---- ---- ---- */
#define MASK_ALL          (0xFFFFFFFF)

/* cpu dai name */
#define MT_SOC_DAI_NAME "mt8127-soc-dai-driver"
#define MT_SOC_DL1DAI_NAME "mt8127-soc-dl1dai-driver"
#define MT_SOC_DL1DATA2DAI_NAME "mt8127-soc-dl1data2dai-driver"
#define MT_SOC_UL1DAI_NAME "mt8127-soc-ul1dai-driver"
#define MT_SOC_UL1DATA2_NAME "mt8127-soc-ul1data2dai-driver"
#define MT_SOC_UL2DAI_NAME "mt8127-soc-ul2dai-driver"
#define MT_SOC_I2S0AWBDAI_NAME "mt8127-soc-i2s0awbdai-driver"
#define MT_SOC_VOICE_MD1_NAME "mt8127-soc-voicemd1dai-driver"
#define MT_SOC_VOICE_MD1_BT_NAME "mt8127-soc-voicemd1-btdai-driver"
#define MT_SOC_VOICE_MD2_NAME "mt8127-soc-voicemd2dai-driver"
#define MT_SOC_VOICE_MD2_BT_NAME "mt8127-soc-voicemd2-btdai-driver"
#define MT_SOC_VOIP_CALL_BT_OUT_NAME "mt8127-soc-voipcall-btdai-out-driver"
#define MT_SOC_VOIP_CALL_BT_IN_NAME "mt8127-soc-voipcall-btdai-in-driver"
#define MT_SOC_ULDLLOOPBACK_NAME "mt8127-soc-uldlloopbackdai-driver"
#define MT_SOC_HDMI_NAME "mt8127-soc-hdmidai-driver"
#define MT_SOC_I2S0_NAME "mt8127-soc-i2s0dai-driver"
#define MT_SOC_I2S0DL1_NAME "mt8127-soc-i2s0dl1dai-driver"
#define MT_SOC_MRGRX_NAME "mt8127-soc-mrgrxdai-driver"
#define MT_SOC_MRGRXCAPTURE_NAME "mt8127-soc-mrgrxcapturedai-driver"
#define MT_SOC_DL1AWB_NAME "mt8127-soc-dl1awbdai-driver"
#define MT_SOC_FM_MRGTX_NAME  "mt8127-soc-fmmrgtxdai-driver"
#define MT_SOC_TDMRX_NAME "mt8127-soc-tdmrxdai-driver"
#define MT_SOC_MODADCI2SDAI_NAME "mt8127-soc-mod2adci2s-driver"
#define MT_SOC_ADC2AWBDAI_NAME "mt8127-soc-adc2awb-driver"
#define MT_SOC_IO2DAIDAI_NAME "mt8127-soc-io2dai-driver"
#define MT_SOC_HP_IMPEDANCE_NAME "mt8127-soc-hpimpedancedai-driver"
#define MT_SOC_FM_I2S_NAME "mt8127-soc-fmi2S-driver"
#define MT_SOC_FM_I2S_CAPTURE_NAME "mt8127-soc-fmi2Scapturedai-driver"
#define MT_SOC_OFFLOAD_GDMA_NAME "mt8127-soc-offload-gdma-driver"


/* machine name */
#define MT_SOC_MACHINE_NAME "mt8127-soc-machine"

/* platform name */
#define MT_SOC_DL1_PCM   "mt8127-soc-dl1-pcm"
#define MT_SOC_HP_IMPEDANCE_PCM   "mt8127-soc-hp-impedence-pcm"
#define MT_SOC_DL1DATA2_PCM   "mt8127-soc-dl1_data2-pcm"
#define MT_SOC_DL2_PCM   "mt8127-soc-dl2-pcm"
#define MT_SOC_UL1_PCM   "mt8127-soc-ul1-pcm"
#define MT_SOC_UL2_PCM   "mt8127-soc-ul2-pcm"
#define MT_SOC_I2S0_AWB_PCM   "mt8127-soc-i2s0awb-pcm"
#define MT_SOC_AWB_PCM   "mt8127-soc-awb-pcm"
#define MT_SOC_MRGRX_AWB_PCM   "mt8127-soc-mrgrx-awb-pcm"
#define MT_SOC_DL1_AWB_PCM   "mt8127-soc-dl1-awb-pcm"
#define MT_SOC_DAI_PCM   "mt8127-soc-DAI-pcm"
#define MT_SOC_HDMI_PCM  "mt8127-soc-hdmi-pcm"
#define MT_SOC_I2S0_PCM  "mt8127-soc-i2s0-pcm"
#define MT_SOC_MRGRX_PCM  "mt8127-soc-mrgrx-pcm"
#define MT_SOC_I2S0DL1_PCM  "mt8127-soc-i2s0dl1-pcm"
#define MT_SOC_MODDAI_PCM   "mt8127-soc-MODDAI-pcm"
#define MT_SOC_VOICE_MD1  "mt8127-soc-voicemd1"
#define MT_SOC_VOICE_MD2  "mt8127-soc-voicemd2"
#define MT_SOC_VOICE_MD1_BT "mt8127-soc-voicemd1-bt"
#define MT_SOC_VOICE_MD2_BT "mt8127-soc-voicemd2-bt"
#define MT_SOC_VOIP_BT_OUT "mt8127-soc-voip-bt-out"
#define MT_SOC_VOIP_BT_IN "mt8127-soc-voip-bt-in"
#define MT_SOC_IFMI2S2  "mt8127-soc-fm-i2s2"
#define MT_SOC_DUMMY_PCM  "mt8127-soc-dummy-pcm"
#define MT_SOC_ULDLLOOPBACK_PCM  "mt8127-soc-uldlloopback-pcm"
#define MT_SOC_ROUTING_PCM  "mt8127-soc-routing-pcm"
#define MT_SOC_FM_MRGTX_PCM "mt8127-soc-fmmrgtx-pcm"
#define MT_SOC_TDMRX_PCM "mt8127-soc-tdmrx-pcm"
#define MT_SOC_MOD_ADCI2S_PCM "mt8127-soc-mod2adci2s-pcm"
#define MT_SOC_ADC2_AWB_PCM "mt8127-soc-adc2awb-pcm"
#define MT_SOC_IO2_DAI_PCM "mt8127-soc-io2dai-pcm"
#define MT_SOC_FM_I2S_PCM  "mt8127-soc-fm-i2s-pcm"
#define MT_SOC_FM_I2S_AWB_PCM  "mt8127-soc-fm-i2s-awb-pcm"
#define MT_SOC_OFFLOAD_GDMA_PCM "mt8127-soc-offload-gdma-pcm"


/* codec dai name */
#define MT_SOC_CODEC_TXDAI_NAME "mt8127-soc-codec-tx-dai"
#define MT_SOC_CODEC_RXDAI_NAME "mt8127-soc-codec-rx-dai"
#define MT_SOC_CODEC_RXDAI2_NAME "mt8127-soc-codec-rx-dai2"
#define MT_SOC_CODEC_I2S0AWB_NAME "mt8127-soc-codec-i2s0awb-dai"
#define MT_SOC_CODEC_I2S0TXDAI_NAME "mt8127-soc-codec-I2s0tx-dai"
#define MT_SOC_CODEC_DL1AWBDAI_NAME "mt8127-soc-codec-dl1awb-dai"
#define MT_SOC_CODEC_VOICE_MD1DAI_NAME "mt8127-soc-codec-voicemd1-dai"
#define MT_SOC_CODEC_VOICE_MD2DAI_NAME "mt8127-soc-codec-voicemd2-dai"
#define MT_SOC_CODEC_VOICE_MD1_BTDAI_NAME "mt8127-soc-codec-voicemd1-bt-dai"
#define MT_SOC_CODEC_VOICE_MD2_BTDAI_NAME "mt8127-soc-codec-voicemd2-bt-dai"
#define MT_SOC_CODEC_VOIPCALLBTOUTDAI_NAME "mt8127-soc-codec-voipcall-btout-dai"
#define MT_SOC_CODEC_VOIPCALLBTINDAI_NAME "mt8127-soc-codec-voipcall-btin-dai"
#define MT_SOC_CODEC_TDMRX_DAI_NAME "mt8127-soc-tdmrx-dai-codec"
#define MT_SOC_CODEC_HP_IMPEDANCE_NAME "mt8127-soc-codec-hp-impedance-dai"


#define MT_SOC_CODEC_FMI2S2TXDAI_NAME "mt8127-soc-codec-fmi2s2tx-dai"
#define MT_SOC_CODEC_FMI2S2RXDAI_NAME "mt8127-soc-codec-fmi2s2rx-dai"
#define MT_SOC_CODEC_ULDLLOOPBACK_NAME "mt8127-soc-codec-uldlloopback-dai"
#define MT_SOC_ROUTING_DAI_NAME "Routing-Control"
#define MT_SOC_CODEC_STUB_NAME "mt8127-soc-codec-stub"
#define MT_SOC_CODEC_NAME "mt8127-soc-codec"
#define MT_SOC_CODEC_DUMMY_NAME "mt8127-soc-dummy-codec"
#define MT_SOC_CODEC_DUMMY_DAI_NAME "mt8127-soc-dummy-dai-codec"
#define MT_SOC_CODEC_HDMI_DUMMY_DAI_NAME "mt8127-soc-hdmi-dummy-dai-codec"
#define MT_SOC_CODEC_I2S0_DUMMY_DAI_NAME "mt8127-soc-i2s0-dummy-dai-codec"
#define MT_SOC_CODEC_MRGRX_DUMMY_DAI_NAME "mt8127-soc-mrgrx-dummy-dai-codec"
#define MT_SOC_CODEC_MRGRX_DAI_NAME "mt8127-soc-mrgrx-dai-codec"
#define MT_SOC_CODEC_FMMRGTXDAI_DUMMY_DAI_NAME "mt8127-soc-fmmrg2tx-dummy-dai-codec"
#define MT_SOC_CODEC_MODADCI2S_DUMMY_DAI_NAME "mt8127-soc-mod2adci2s-dummy-dai-codec"
#define MT_SOC_CODEC_ADC2AWB_DUMMY_DAI_NAME "mt8127-soc-adc2awb-dummy-dai-codec"
#define MT_SOC_CODEC_IO2DAI_DUMMY_DAI_NAME "mt8127-soc-io2dai-dummy-dai-codec"
#define MT_SOC_CODEC_FM_I2S_DUMMY_DAI_NAME "mt8127-soc-fm-i2s-dummy-dai-codec"
#define MT_SOC_CODEC_FM_I2S_DAI_NAME "mt8127-soc-fm-i2s-dai-codec"
#define MT_SOC_CODEC_OFFLOAD_GDMA_DAI_NAME "mt8127-soc-offload-gdma-dai-codec"

/* stream name */
#define MT_SOC_DL1_STREAM_NAME "MultiMedia1_PLayback"
#define MT_SOC_DL1DATA2_STREAM_NAME "MultiMedia1data2_PLayback"
#define MT_SOC_DL2_STREAM_NAME "MultiMedia2_PLayback"
#define MT_SOC_VOICE_MD1_STREAM_NAME "Voice_MD1_PLayback"
#define MT_SOC_VOICE_MD2_STREAM_NAME "Voice_MD2_PLayback"
#define MT_SOC_VOICE_MD1_BT_STREAM_NAME "Voice_MD1_BT_Playback"
#define MT_SOC_VOICE_MD2_BT_STREAM_NAME "Voice_MD2_BT_Playback"
#define MT_SOC_VOIP_BT_OUT_STREAM_NAME "VOIP_Call_BT_Playback"
#define MT_SOC_VOIP_BT_IN_STREAM_NAME "VOIP_Call_BT_Capture"
#define MT_SOC_HDMI_STREAM_NAME "HMDI_PLayback"
#define MT_SOC_I2S0_STREAM_NAME "I2S0_PLayback"
#define MT_SOC_I2SDL1_STREAM_NAME "I2S0DL1_PLayback"
#define MT_SOC_MRGRX_STREAM_NAME "MRGRX_PLayback"
#define MT_SOC_MRGRX_CAPTURE_STREAM_NAME "MRGRX_CAPTURE"
#define MT_SOC_FM_I2S2_STREAM_NAME "FM_I2S2_PLayback"
#define MT_SOC_ULDLLOOPBACK_STREAM_NAME "ULDL_Loopback"
#define MT_SOC_FM_I2S2_RECORD_STREAM_NAME "FM_I2S2_Record"
#define MT_SOC_DL1_AWB_RECORD_STREAM_NAME "DL1_AWB_Record"
#define MT_SOC_UL1_STREAM_NAME "MultiMedia1_Capture"
#define MT_SOC_UL1DATA2_STREAM_NAME "MultiMediaData2_Capture"
#define MT_SOC_I2S0AWB_STREAM_NAME "I2S0AWB_Capture"
#define MT_SOC_AWB_STREAM_NAME "MultiMedia_awb_Capture"
#define MT_SOC_DAI_STREAM_NAME "MultiMedia_dai_Capture"
#define MT_SOC_MODDAI_STREAM_NAME "MultiMedia_Moddai_Capture"
#define MT_SOC_ROUTING_STREAM_NAME "MultiMedia_Routing"
#define MT_SOC_HP_IMPEDANCE_STREAM_NAME "HP_IMPEDANCE_Playback"
#define MT_SOC_FM_MRGTX_STREAM_NAME "FM_MRGTX_Playback"
#define MT_SOC_TDM_CAPTURE_STREAM_NAME "TDM_Debug_Record"
#define MT_SOC_MODADCI2S_STREAM_NAME "ANC_Debug_Record_MOD"
#define MT_SOC_ADC2AWB_STREAM_NAME "ANC_Debug_Record_ADC2"
#define MT_SOC_IO2DAI_STREAM_NAME "ANC_Debug_Record_IO2"
#define MT_SOC_FM_I2S_PLAYBACK_STREAM_NAME "FM_I2S_Playback"
#define MT_SOC_FM_I2S_CAPTURE_STREAM_NAME "FM_I2S_Capture"
#define MT_SOC_OFFLOAD_GDMA_STREAM_NAME "OFFLOAD_GDMA_Playback"

#endif


