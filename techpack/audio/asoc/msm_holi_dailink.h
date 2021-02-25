/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <sound/soc.h>

/* FE dai-links */
SND_SOC_DAILINK_DEFS(multimedia1,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia1")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dsp.0")));

SND_SOC_DAILINK_DEFS(multimedia2,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia2")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dsp.0")));

SND_SOC_DAILINK_DEFS(voicemmode1,
	DAILINK_COMP_ARRAY(COMP_CPU("VoiceMMode1")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-voice")));

SND_SOC_DAILINK_DEFS(msmvoip,
	DAILINK_COMP_ARRAY(COMP_CPU("VoIP")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-voip-dsp")));

SND_SOC_DAILINK_DEFS(multimedia3,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia3")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dsp.2")));

SND_SOC_DAILINK_DEFS(afepcm_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.241")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-afe")));

SND_SOC_DAILINK_DEFS(afepcm_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.240")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-afe")));

SND_SOC_DAILINK_DEFS(multimedia4,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia4")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-compress-dsp")));

SND_SOC_DAILINK_DEFS(auxpcm_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("AUXPCM_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(multimedia5,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia5")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dsp.1")));

SND_SOC_DAILINK_DEFS(listen1,
	DAILINK_COMP_ARRAY(COMP_CPU("LSM1")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-lsm-client")));

SND_SOC_DAILINK_DEFS(multimedia7,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia7")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-compress-dsp")));

SND_SOC_DAILINK_DEFS(multimedia10,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia10")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dsp.1")));

SND_SOC_DAILINK_DEFS(multimedia8,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia8")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dsp-noirq")));

SND_SOC_DAILINK_DEFS(hdmi_rx_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("HDMI_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(voicemmode2,
	DAILINK_COMP_ARRAY(COMP_CPU("VoiceMMode2")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-voice")));

SND_SOC_DAILINK_DEFS(listen2,
	DAILINK_COMP_ARRAY(COMP_CPU("LSM2")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-lsm-client")));

SND_SOC_DAILINK_DEFS(listen3,
	DAILINK_COMP_ARRAY(COMP_CPU("LSM3")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-lsm-client")));

SND_SOC_DAILINK_DEFS(listen4,
	DAILINK_COMP_ARRAY(COMP_CPU("LSM4")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-lsm-client")));

SND_SOC_DAILINK_DEFS(listen5,
	DAILINK_COMP_ARRAY(COMP_CPU("LSM5")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-lsm-client")));

SND_SOC_DAILINK_DEFS(listen6,
	DAILINK_COMP_ARRAY(COMP_CPU("LSM6")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-lsm-client")));

SND_SOC_DAILINK_DEFS(listen7,
	DAILINK_COMP_ARRAY(COMP_CPU("LSM7")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-lsm-client")));

SND_SOC_DAILINK_DEFS(listen8,
	DAILINK_COMP_ARRAY(COMP_CPU("LSM8")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-lsm-client")));

SND_SOC_DAILINK_DEFS(multimedia9,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia9")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dsp.0")));

SND_SOC_DAILINK_DEFS(multimedia11,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia11")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-compress-dsp")));

SND_SOC_DAILINK_DEFS(multimedia12,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia12")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-compress-dsp")));

SND_SOC_DAILINK_DEFS(multimedia13,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia13")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-compress-dsp")));

SND_SOC_DAILINK_DEFS(multimedia14,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia14")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-compress-dsp")));

SND_SOC_DAILINK_DEFS(multimedia15,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia15")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-compress-dsp")));

SND_SOC_DAILINK_DEFS(multimedia16,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia16")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dsp-noirq")));

SND_SOC_DAILINK_DEFS(cdcdma_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("CDC_DMA_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(tx3_cdcdma_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("TX3_CDC_DMA_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(tert_mi2s_tx_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("TERT_MI2S_TX_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(multimedia6,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia6")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-loopback")));

SND_SOC_DAILINK_DEFS(usbaudio_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("USBAUDIO_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(slimbus7_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("SLIMBUS7_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(multimedia17,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia17")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-compress-dsp")));

SND_SOC_DAILINK_DEFS(slimbus8_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("SLIMBUS8_HOSTLESS")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(tx_cdcdma5_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45115")),
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "tx_macro_tx3"),
			   COMP_CODEC("wcd938x_codec", "wcd938x_cdc")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-hostless")));

SND_SOC_DAILINK_DEFS(multimedia31,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia31")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dsp.0")));

SND_SOC_DAILINK_DEFS(multimedia32,
	DAILINK_COMP_ARRAY(COMP_CPU("MultiMedia32")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-dsp.0")));

SND_SOC_DAILINK_DEFS(afepcm_tx1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.242")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-afe")));

/* BE dai-links */
SND_SOC_DAILINK_DEFS(afe_pcm_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.224")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(afe_pcm_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.225")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(incall_record_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.32772")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(incall_record_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.32771")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(voice_playback_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.32773")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(voice2_playback_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.32770")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(usb_audio_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.28672")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(usb_audio_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.28673")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(pri_tdm_rx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36864")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(pri_tdm_tx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36865")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sec_tdm_rx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36880")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sec_tdm_tx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36881")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(tert_tdm_rx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36896")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(tert_tdm_tx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36897")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quat_tdm_rx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36912")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quat_tdm_tx_0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-tdm.36913")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(slimbus_7_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.16398")),
	DAILINK_COMP_ARRAY(COMP_CODEC("btfmslim_slave",
			"btfm_bt_sco_a2dp_slim_rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(slimbus_7_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.16399")),
	DAILINK_COMP_ARRAY(COMP_CODEC("btfmslim_slave",
			"btfm_bt_sco_slim_tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(slimbus_8_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.16401")),
	DAILINK_COMP_ARRAY(COMP_CODEC("btfmslim_slave",
			"btfm_fm_slim_tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(pri_mi2s_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-mi2s.0")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(pri_mi2s_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-mi2s.0")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sec_mi2s_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-mi2s.1")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sec_mi2s_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-mi2s.1")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(tert_mi2s_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-mi2s.2")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(tert_mi2s_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-mi2s.2")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quat_mi2s_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-mi2s.3")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quat_mi2s_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-mi2s.3")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(auxpcm_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-auxpcm.1")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(auxpcm_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-auxpcm.1")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sec_auxpcm_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-auxpcm.2")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(sec_auxpcm_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-auxpcm.2")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(tert_auxpcm_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-auxpcm.3")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(tert_auxpcm_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-auxpcm.3")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quat_auxpcm_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-auxpcm.4")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(quat_auxpcm_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-auxpcm.4")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(rx_dma_rx0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45104")),
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "rx_macro_rx1"),
			   COMP_CODEC("wcd938x_codec", "wcd938x_cdc"),
			   COMP_CODEC("wcd937x_codec", "wcd937x_cdc")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(rx_dma_rx1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45106")),
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "rx_macro_rx2"),
			   COMP_CODEC("wsa-codec0", "wsa_rx0"),
			   COMP_CODEC("wsa-codec1", "wsa_rx1"),
			   COMP_CODEC("wcd938x_codec", "wcd938x_cdc"),
			   COMP_CODEC("wcd937x_codec", "wcd937x_cdc")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(rx_dma_rx2,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45108")),
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "rx_macro_rx3"),
			   COMP_CODEC("wcd938x_codec", "wcd938x_cdc"),
			   COMP_CODEC("wcd937x_codec", "wcd937x_cdc")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(rx_dma_rx3,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45110")),
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "rx_macro_rx4"),
			   COMP_CODEC("wcd938x_codec", "wcd938x_cdc"),
			   COMP_CODEC("wcd937x_codec", "wcd937x_cdc")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(tx_dma_tx3,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45111")),
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "tx_macro_tx1"),
			   COMP_CODEC("wcd938x_codec", "wcd938x_cdc"),
			   COMP_CODEC("wcd937x_codec", "wcd937x_cdc")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(tx_dma_tx4,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45113")),
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "tx_macro_tx2"),
			   COMP_CODEC("wcd938x_codec", "wcd938x_cdc"),
			   COMP_CODEC("wcd937x_codec", "wcd937x_cdc")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(va_dma_tx0,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45089")),
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "va_macro_tx1")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(va_dma_tx1,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45091")),
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "va_macro_tx2")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(va_dma_tx2,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-cdc-dma-dev.45093")),
	DAILINK_COMP_ARRAY(COMP_CODEC("bolero_codec", "va_macro_tx3")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(afe_loopback_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.24577")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(proxy_tx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.8195")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-tx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));

SND_SOC_DAILINK_DEFS(proxy_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("msm-dai-q6-dev.8194")),
	DAILINK_COMP_ARRAY(COMP_CODEC("msm-stub-codec.1", "msm-stub-rx")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("msm-pcm-routing")));
