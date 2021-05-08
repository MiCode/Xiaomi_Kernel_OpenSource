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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */


/*******************************************************************************
 *
 * Filename:
 * ---------
 *  mt_sco_analog_type.h
 *
 * Project:
 * --------
 *   MT6583  Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio register
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *------------------------------------------------------------------------------
 *
 *
 ******************************************************************************/

#ifndef _AUDIO_ANALOG_TYPE_H
#define _AUDIO_ANALOG_TYPE_H


/*****************************************************************************
 *                ENUM DEFINITION
 *****************************************************************************/


enum ANA_GAIN_type {
	ANA_GAIN_HSOUTL = 0,
	ANA_GAIN_HSOUTR,
	ANA_GAIN_HPOUTL,
	ANA_GAIN_HPOUTR,
	ANA_GAIN_SPKL,
	ANA_GAIN_SPKR,
	ANA_GAIN_SPEAKER_HEADSET_R,
	ANA_GAIN_SPEAKER_HEADSET_L,
	ANA_GAIN_IV_BUFFER,
	ANA_GAIN_LINEOUTL,
	ANA_GAIN_LINEOUTR,
	ANA_GAIN_LINEINL,
	ANA_GAIN_LINEINR,
	ANA_GAIN_MICAMP1,
	ANA_GAIN_MICAMP2,
	ANA_GAIN_MICAMP3,
	ANA_GAIN_MICAMP4,
	ANA_GAIN_LEVELSHIFTL,
	ANA_GAIN_LEVELSHIFTR,
	ANA_GAIN_TYPE_MAX
};

/* mux seleciotn */
enum audio_analog_mux_type {
	AUDIO_ANALOG_MUX_VOICE = 0,
	AUDIO_ANALOG_MUX_AUDIO,
	AUDIO_ANALOG_MUX_IV_BUFFER,
	AUDIO_ANALOG_MUX_LINEIN_STEREO,
	AUDIO_ANALOG_MUX_LINEIN_L,
	AUDIO_ANALOG_MUX_LINEIN_R,
	AUDIO_ANALOG_MUX_LINEIN_AUDIO_MONO,
	AUDIO_ANALOG_MUX_LINEIN_AUDIO_STEREO,
	AUDIO_ANALOG_MUX_IN_MIC1,
	AUDIO_ANALOG_MUX_IN_MIC2,
	AUDIO_ANALOG_MUX_IN_MIC3,
	AUDIO_ANALOG_MUX_IN_MIC4,
	AUDIO_ANALOG_MUX_IN_LINE_IN,
	AUDIO_ANALOG_MUX_IN_PREAMP_1,
	AUDIO_ANALOG_MUX_IN_PREAMP_2,
	AUDIO_ANALOG_MUX_IN_PREAMP_3,
	AUDIO_ANALOG_MUX_IN_PREAMP_4,
	MICSOURCE_MUX_IN_1,
	MICSOURCE_MUX_IN_2,
	MICSOURCE_MUX_IN_3,
	MICSOURCE_MUX_IN_4,
	AUDIO_ANALOG_MUX_IN_LEVEL_SHIFT_BUFFER,
	AUDIO_ANALOG_MUX_MUTE,
	AUDIO_ANALOG_MUX_OPEN,
	AUDIO_ANALOG_MAX_MUX_TYPE
};

/* device power */
enum audio_analog_device_type {
	ANA_DEV_OUT_EARPIECER = 0,
	ANA_DEV_OUT_EARPIECEL = 1,
	ANA_DEV_OUT_HEADSETR = 2,
	ANA_DEV_OUT_HEADSETL = 3,
	ANA_DEV_OUT_SPEAKERR = 4,
	ANA_DEV_OUT_SPEAKERL = 5,
	ANA_DEV_OUT_SPEAKER_HEADSET_R = 6,
	ANA_DEV_OUT_SPEAKER_HEADSET_L = 7,
	ANA_DEV_OUT_LINEOUTR = 8,
	ANA_DEV_OUT_LINEOUTL = 9,
	ANA_DEV_OUT_EXTSPKAMP = 10,
	ANA_DEV_2IN1_SPK = 11,
	/* DEVICE_IN_LINEINR = 11, */
	/* DEVICE_IN_LINEINL = 12, */
	ANA_DEV_IN_ADC1 = 13,
	ANA_DEV_IN_ADC2 = 14,
	ANA_DEV_IN_ADC3 = 15,
	ANA_DEV_IN_ADC4 = 16,
	ANA_DEV_IN_PREAMP_L = 17,
	ANA_DEV_IN_PREAMP_R = 18,
	ANA_DEV_IN_DIGITAL_MIC = 19,
	ANA_DEV_RECEIVER_SPEAKER_SWITCH = 20,
	ANA_DEV_MAX
};

enum audio_analog_device_sample_rate {
	ANA_DEV_OUT_DAC,
	ANA_DEV_IN_ADC,
	ANA_DEV_IN_ADC_2,
	ANA_DEV_IN_OUT_MAX
};

enum audio_analog_audio_analog_input {
	AUDIO_ANALOG_AUDIOANALOG_INPUT_OPEN = 0,
	AUDIO_ANALOG_AUDIOANALOG_INPUT_ADC,
	AUDIO_ANALOG_AUDIOANALOG_INPUT_PREAMP,
};

enum audio_analog_channels {
	AUDIO_ANALOG_CHANNELS_LEFT1 = 0,
	AUDIO_ANALOG_CHANNELS_RIGHT1,
};

enum audio_analog_loopback {
	AUDIO_ANALOG_DAC_LOOP_DAC_HS_ON = 0,
	AUDIO_ANALOG_DAC_LOOP_DAC_HS_OFF,
	AUDIO_ANALOG_DAC_LOOP_DAC_HP_ON,
	AUDIO_ANALOG_DAC_LOOP_DAC_HP_OFF,
	AUDIO_ANALOG_DAC_LOOP_DAC_SPEAKER_ON,
	AUDIO_ANALOG_DAC_LOOP_DAC_SPEAKER_OFF,
};

enum audio_analog_ul_mode {
	ANA_UL_MODE_ACC = 0,
	ANA_UL_MODE_DCC,
	ANA_UL_MODE_DMIC,
	ANA_UL_MODE_DCCECMDIFF,
	ANA_UL_MODE_DCCECMSINGLE,
};

enum audio_offset_trim_mux {
	AUDIO_OFFSET_TRIM_MUX_OPEN = 0,
	AUDIO_OFFSET_TRIM_MUX_HPL,
	AUDIO_OFFSET_TRIM_MUX_HPR,
	AUDIO_OFFSET_TRIM_MUX_HSP,
	AUDIO_OFFSET_TRIM_MUX_HSN,
	AUDIO_OFFSET_TRIM_MUX_LOLP,
	AUDIO_OFFSET_TRIM_MUX_LOLN,
	AUDIO_OFFSET_TRIM_MUX_AU_REFN,
	AUDIO_OFFSET_TRIM_MUX_AVSS28,
	AUDIO_OFFSET_TRIM_MUX_AVSS28_2,
	AUDIO_OFFSET_TRIM_MUX_UNUSED,
	AUDIO_OFFSET_TRIM_MUX_GROUND,
};

struct mt6358_codec_priv {
	int ana_gain[ANA_GAIN_TYPE_MAX];
	int ana_mux[AUDIO_ANALOG_MAX_MUX_TYPE];
	int dev_power[ANA_DEV_MAX];
	int backup_dev_power[ANA_DEV_MAX];
};

#endif
