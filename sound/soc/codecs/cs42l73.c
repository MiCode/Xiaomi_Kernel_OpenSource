/*
 * cs42l73.c  --  CS42L73 ALSA Soc Audio driver
 *
 * Copyright 2011 Cirrus Logic, Inc.
 *
 * Authors: Georgi Vlaev, Nucleus Systems Ltd, <joe@nucleusys.com>
 *	    Brian Austin, Cirrus Logic Inc, <brian.austin@cirrus.com>
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/i2c/cs42l73.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include "cs42l73.h"

struct sp_config {
	u8 spc, mmcc, spfs;
	u32 srate;
};
struct  cs42l73_private {
	struct regulator_dev *micbias[2];
	struct snd_soc_codec *codec;
	struct sp_config config[3];
	struct regmap *regmap;
	u32 sysclk;
	u8 mclksel;
	u32 mclk;
	int shutdwn_delay;
};

static const struct reg_default cs42l73_reg_defaults[] = {
	{ 1, 0x42 },	/* r01	- Device ID A&B */
	{ 2, 0xA7 },	/* r02	- Device ID C&D */
	{ 3, 0x30 },	/* r03	- Device ID E */
	{ 6, 0xF1 },	/* r06	- Power Ctl 1 */
	{ 7, 0xDF },	/* r07	- Power Ctl 2 */
	{ 8, 0x3F },	/* r08	- Power Ctl 3 */
	{ 9, 0x50 },	/* r09	- Charge Pump Freq */
	{ 10, 0x53 },	/* r0A	- Output Load MicBias Short Detect */
	{ 11, 0x00 },	/* r0B	- DMIC Master Clock Ctl */
	{ 12, 0x00 },	/* r0C	- Aux PCM Ctl */
	{ 13, 0x15 },	/* r0D	- Aux PCM Master Clock Ctl */
	{ 14, 0x00 },	/* r0E	- Audio PCM Ctl */
	{ 15, 0x15 },	/* r0F	- Audio PCM Master Clock Ctl */
	{ 16, 0x00 },	/* r10	- Voice PCM Ctl */
	{ 17, 0x15 },	/* r11	- Voice PCM Master Clock Ctl */
	{ 18, 0x00 },	/* r12	- Voice/Aux Sample Rate */
	{ 19, 0x06 },	/* r13	- Misc I/O Path Ctl */
	{ 20, 0x00 },	/* r14	- ADC Input Path Ctl */
	{ 21, 0x00 },	/* r15	- MICA Preamp, PGA Volume */
	{ 22, 0x00 },	/* r16	- MICB Preamp, PGA Volume */
	{ 23, 0x00 },	/* r17	- Input Path A Digital Volume */
	{ 24, 0x00 },	/* r18	- Input Path B Digital Volume */
	{ 25, 0x00 },	/* r19	- Playback Digital Ctl */
	{ 26, 0x00 },	/* r1A	- HP/LO Left Digital Volume */
	{ 27, 0x00 },	/* r1B	- HP/LO Right Digital Volume */
	{ 28, 0x00 },	/* r1C	- Speakerphone Digital Volume */
	{ 29, 0x00 },	/* r1D	- Ear/SPKLO Digital Volume */
	{ 30, 0x00 },	/* r1E	- HP Left Analog Volume */
	{ 31, 0x00 },	/* r1F	- HP Right Analog Volume */
	{ 32, 0x00 },	/* r20	- LO Left Analog Volume */
	{ 33, 0x00 },	/* r21	- LO Right Analog Volume */
	{ 34, 0x00 },	/* r22	- Stereo Input Path Advisory Volume */
	{ 35, 0x00 },	/* r23	- Aux PCM Input Advisory Volume */
	{ 36, 0x00 },	/* r24	- Audio PCM Input Advisory Volume */
	{ 37, 0x00 },	/* r25	- Voice PCM Input Advisory Volume */
	{ 38, 0x00 },	/* r26	- Limiter Attack Rate HP/LO */
	{ 39, 0x7F },	/* r27	- Limter Ctl, Release Rate HP/LO */
	{ 40, 0x00 },	/* r28	- Limter Threshold HP/LO */
	{ 41, 0x00 },	/* r29	- Limiter Attack Rate Speakerphone */
	{ 42, 0x3F },	/* r2A	- Limter Ctl, Release Rate Speakerphone */
	{ 43, 0x00 },	/* r2B	- Limter Threshold Speakerphone */
	{ 44, 0x00 },	/* r2C	- Limiter Attack Rate Ear/SPKLO */
	{ 45, 0x3F },	/* r2D	- Limter Ctl, Release Rate Ear/SPKLO */
	{ 46, 0x00 },	/* r2E	- Limter Threshold Ear/SPKLO */
	{ 47, 0x00 },	/* r2F	- ALC Enable, Attack Rate Left/Right */
	{ 48, 0x3F },	/* r30	- ALC Release Rate Left/Right */
	{ 49, 0x00 },	/* r31	- ALC Threshold Left/Right */
	{ 50, 0x00 },	/* r32	- Noise Gate Ctl Left/Right */
	{ 51, 0x00 },	/* r33	- ALC/NG Misc Ctl */
	{ 52, 0x18 },	/* r34	- Mixer Ctl */
	{ 53, 0x3F },	/* r35	- HP/LO Left Mixer Input Path Volume */
	{ 54, 0x3F },	/* r36	- HP/LO Right Mixer Input Path Volume */
	{ 55, 0x3F },	/* r37	- HP/LO Left Mixer Aux PCM Volume */
	{ 56, 0x3F },	/* r38	- HP/LO Right Mixer Aux PCM Volume */
	{ 57, 0x3F },	/* r39	- HP/LO Left Mixer Audio PCM Volume */
	{ 58, 0x3F },	/* r3A	- HP/LO Right Mixer Audio PCM Volume */
	{ 59, 0x3F },	/* r3B	- HP/LO Left Mixer Voice PCM Mono Volume */
	{ 60, 0x3F },	/* r3C	- HP/LO Right Mixer Voice PCM Mono Volume */
	{ 61, 0x3F },	/* r3D	- Aux PCM Left Mixer Input Path Volume */
	{ 62, 0x3F },	/* r3E	- Aux PCM Right Mixer Input Path Volume */
	{ 63, 0x3F },	/* r3F	- Aux PCM Left Mixer Volume */
	{ 64, 0x3F },	/* r40	- Aux PCM Left Mixer Volume */
	{ 65, 0x3F },	/* r41	- Aux PCM Left Mixer Audio PCM L Volume */
	{ 66, 0x3F },	/* r42	- Aux PCM Right Mixer Audio PCM R Volume */
	{ 67, 0x3F },	/* r43	- Aux PCM Left Mixer Voice PCM Volume */
	{ 68, 0x3F },	/* r44	- Aux PCM Right Mixer Voice PCM Volume */
	{ 69, 0x3F },	/* r45	- Audio PCM Left Input Path Volume */
	{ 70, 0x3F },	/* r46	- Audio PCM Right Input Path Volume */
	{ 71, 0x3F },	/* r47	- Audio PCM Left Mixer Aux PCM L Volume */
	{ 72, 0x3F },	/* r48	- Audio PCM Right Mixer Aux PCM R Volume */
	{ 73, 0x3F },	/* r49	- Audio PCM Left Mixer Volume */
	{ 74, 0x3F },	/* r4A	- Audio PCM Right Mixer Volume */
	{ 75, 0x3F },	/* r4B	- Audio PCM Left Mixer Voice PCM Volume */
	{ 76, 0x3F },	/* r4C	- Audio PCM Right Mixer Voice PCM Volume */
	{ 77, 0x3F },	/* r4D	- Voice PCM Left Input Path Volume */
	{ 78, 0x3F },	/* r4E	- Voice PCM Right Input Path Volume */
	{ 79, 0x3F },	/* r4F	- Voice PCM Left Mixer Aux PCM L Volume */
	{ 80, 0x3F },	/* r50	- Voice PCM Right Mixer Aux PCM R Volume */
	{ 81, 0x3F },	/* r51	- Voice PCM Left Mixer Audio PCM L Volume */
	{ 82, 0x3F },	/* r52	- Voice PCM Right Mixer Audio PCM R Volume */
	{ 83, 0x3F },	/* r53	- Voice PCM Left Mixer Voice PCM Volume */
	{ 84, 0x3F },	/* r54	- Voice PCM Right Mixer Voice PCM Volume */
	{ 85, 0xAA },	/* r55	- Mono Mixer Ctl */
	{ 86, 0x3F },	/* r56	- SPK Mono Mixer Input Path Volume */
	{ 87, 0x3F },	/* r57	- SPK Mono Mixer Aux PCM Mono/L/R Volume */
	{ 88, 0x3F },	/* r58	- SPK Mono Mixer Audio PCM Mono/L/R Volume */
	{ 89, 0x3F },	/* r59	- SPK Mono Mixer Voice PCM Mono Volume */
	{ 90, 0x3F },	/* r5A	- SPKLO Mono Mixer Input Path Mono Volume */
	{ 91, 0x3F },	/* r5B	- SPKLO Mono Mixer Aux Mono/L/R Volume */
	{ 92, 0x3F },	/* r5C	- SPKLO Mono Mixer Audio Mono/L/R Volume */
	{ 93, 0x3F },	/* r5D	- SPKLO Mono Mixer Voice Mono Volume */
	{ 94, 0x00 },	/* r5E	- Interrupt Mask 1 */
	{ 95, 0x00 },	/* r5F	- Interrupt Mask 2 */
};

static bool cs42l73_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case CS42L73_IS1:
	case CS42L73_IS2:
		return true;
	default:
		return false;
	}
}

static bool cs42l73_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case CS42L73_DEVID_AB:
	case CS42L73_DEVID_CD:
	case CS42L73_DEVID_E:
	case CS42L73_REVID:
	case CS42L73_PWRCTL1:
	case CS42L73_PWRCTL2:
	case CS42L73_PWRCTL3:
	case CS42L73_CPFCHC:
	case CS42L73_OLMBMSDC:
	case CS42L73_DMMCC:
	case CS42L73_XSPC:
	case CS42L73_XSPMMCC:
	case CS42L73_ASPC:
	case CS42L73_ASPMMCC:
	case CS42L73_VSPC:
	case CS42L73_VSPMMCC:
	case CS42L73_VXSPFS:
	case CS42L73_MIOPC:
	case CS42L73_ADCIPC:
	case CS42L73_MICAPREPGAAVOL:
	case CS42L73_MICBPREPGABVOL:
	case CS42L73_IPADVOL:
	case CS42L73_IPBDVOL:
	case CS42L73_PBDC:
	case CS42L73_HLADVOL:
	case CS42L73_HLBDVOL:
	case CS42L73_SPKDVOL:
	case CS42L73_ESLDVOL:
	case CS42L73_HPAAVOL:
	case CS42L73_HPBAVOL:
	case CS42L73_LOAAVOL:
	case CS42L73_LOBAVOL:
	case CS42L73_STRINV:
	case CS42L73_XSPINV:
	case CS42L73_ASPINV:
	case CS42L73_VSPINV:
	case CS42L73_LIMARATEHL:
	case CS42L73_LIMRRATEHL:
	case CS42L73_LMAXHL:
	case CS42L73_LIMARATESPK:
	case CS42L73_LIMRRATESPK:
	case CS42L73_LMAXSPK:
	case CS42L73_LIMARATEESL:
	case CS42L73_LIMRRATEESL:
	case CS42L73_LMAXESL:
	case CS42L73_ALCARATE:
	case CS42L73_ALCRRATE:
	case CS42L73_ALCMINMAX:
	case CS42L73_NGCAB:
	case CS42L73_ALCNGMC:
	case CS42L73_MIXERCTL:
	case CS42L73_HLAIPAA:
	case CS42L73_HLBIPBA:
	case CS42L73_HLAXSPAA:
	case CS42L73_HLBXSPBA:
	case CS42L73_HLAASPAA:
	case CS42L73_HLBASPBA:
	case CS42L73_HLAVSPMA:
	case CS42L73_HLBVSPMA:
	case CS42L73_XSPAIPAA:
	case CS42L73_XSPBIPBA:
	case CS42L73_XSPAXSPAA:
	case CS42L73_XSPBXSPBA:
	case CS42L73_XSPAASPAA:
	case CS42L73_XSPBASPBA:
	case CS42L73_XSPAVSPMA:
	case CS42L73_XSPBVSPMA:
	case CS42L73_ASPAIPAA:
	case CS42L73_ASPBIPBA:
	case CS42L73_ASPAXSPAA:
	case CS42L73_ASPBXSPBA:
	case CS42L73_ASPAASPAA:
	case CS42L73_ASPBASPBA:
	case CS42L73_ASPAVSPMA:
	case CS42L73_ASPBVSPMA:
	case CS42L73_VSPAIPAA:
	case CS42L73_VSPBIPBA:
	case CS42L73_VSPAXSPAA:
	case CS42L73_VSPBXSPBA:
	case CS42L73_VSPAASPAA:
	case CS42L73_VSPBASPBA:
	case CS42L73_VSPAVSPMA:
	case CS42L73_VSPBVSPMA:
	case CS42L73_MMIXCTL:
	case CS42L73_SPKMIPMA:
	case CS42L73_SPKMXSPA:
	case CS42L73_SPKMASPA:
	case CS42L73_SPKMVSPMA:
	case CS42L73_ESLMIPMA:
	case CS42L73_ESLMXSPA:
	case CS42L73_ESLMASPA:
	case CS42L73_ESLMVSPMA:
	case CS42L73_IM1:
	case CS42L73_IM2:
	case CS42L73_IS1:
	case CS42L73_IS2:
		return true;
	default:
		return false;
	}
}

static const unsigned int hpaloa_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0, 13, TLV_DB_SCALE_ITEM(-7600, 200, 0),
	14, 75, TLV_DB_SCALE_ITEM(-4900, 100, 0),
};

static DECLARE_TLV_DB_SCALE(adc_boost_tlv, 0, 2000, 0);

static DECLARE_TLV_DB_SCALE(hl_tlv, -10200, 50, 0);

static DECLARE_TLV_DB_SCALE(ipd_tlv, -9600, 100, 0);

static DECLARE_TLV_DB_SCALE(micpga_tlv, -600, 50, 0);

static const unsigned int limiter_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0, 2, TLV_DB_SCALE_ITEM(-3000, 600, 0),
	3, 7, TLV_DB_SCALE_ITEM(-1200, 300, 0),
};

static const DECLARE_TLV_DB_SCALE(attn_tlv, -6300, 100, 1);

static const char * const cs42l73_pgaa_text[] = { "Line A", "Mic 1" };
static const char * const cs42l73_pgab_text[] = { "Line B", "Mic 2" };

static const struct soc_enum pgaa_enum =
	SOC_ENUM_SINGLE(CS42L73_ADCIPC, 3,
		ARRAY_SIZE(cs42l73_pgaa_text), cs42l73_pgaa_text);

static const struct soc_enum pgab_enum =
	SOC_ENUM_SINGLE(CS42L73_ADCIPC, 7,
		ARRAY_SIZE(cs42l73_pgab_text), cs42l73_pgab_text);

static const struct snd_kcontrol_new pgaa_mux =
	SOC_DAPM_ENUM("Left Analog Input Capture Mux", pgaa_enum);

static const struct snd_kcontrol_new pgab_mux =
	SOC_DAPM_ENUM("Right Analog Input Capture Mux", pgab_enum);

static const char * const cs42l73_ng_delay_text[] = {
	"50ms", "100ms", "150ms", "200ms" };

static const struct soc_enum ng_delay_enum =
	SOC_ENUM_SINGLE(CS42L73_NGCAB, 0,
		ARRAY_SIZE(cs42l73_ng_delay_text), cs42l73_ng_delay_text);

static const char * const charge_pump_freq_text[] = {
	"0", "1", "2", "3", "4",
	"5", "6", "7", "8", "9",
	"10", "11", "12", "13", "14", "15" };

static const struct soc_enum charge_pump_enum =
	SOC_ENUM_SINGLE(CS42L73_CPFCHC, 4,
		ARRAY_SIZE(charge_pump_freq_text), charge_pump_freq_text);

static const char * const cs42l73_mono_mix_texts[] = {
	"Left", "Right", "Mono Mix"};

static const unsigned int cs42l73_mono_mix_values[] = { 0, 1, 2 };

static const struct soc_enum spk_asp_enum =
	SOC_VALUE_ENUM_SINGLE(CS42L73_MMIXCTL, 6, 3,
			      ARRAY_SIZE(cs42l73_mono_mix_texts),
			      cs42l73_mono_mix_texts,
			      cs42l73_mono_mix_values);

static const struct snd_kcontrol_new spk_asp_mux =
	SOC_DAPM_ENUM("Route", spk_asp_enum);

static const struct soc_enum spk_xsp_enum =
	SOC_VALUE_ENUM_SINGLE(CS42L73_MMIXCTL, 4, 3,
			      ARRAY_SIZE(cs42l73_mono_mix_texts),
			      cs42l73_mono_mix_texts,
			      cs42l73_mono_mix_values);

static const struct snd_kcontrol_new spk_xsp_mux =
	SOC_DAPM_ENUM("Route", spk_xsp_enum);

static const struct soc_enum esl_asp_enum =
	SOC_VALUE_ENUM_SINGLE(CS42L73_MMIXCTL, 2, 3,
			      ARRAY_SIZE(cs42l73_mono_mix_texts),
			      cs42l73_mono_mix_texts,
			      cs42l73_mono_mix_values);

static const struct snd_kcontrol_new esl_asp_mux =
	SOC_DAPM_ENUM("Route", esl_asp_enum);

static const struct soc_enum esl_xsp_enum =
	SOC_VALUE_ENUM_SINGLE(CS42L73_MMIXCTL, 0, 3,
			      ARRAY_SIZE(cs42l73_mono_mix_texts),
			      cs42l73_mono_mix_texts,
			      cs42l73_mono_mix_values);

static const struct snd_kcontrol_new esl_xsp_mux =
	SOC_DAPM_ENUM("Route", esl_xsp_enum);

static const char * const cs42l73_ip_swap_text[] = {
	"Stereo", "Mono A", "Mono B", "Swap A-B"};

static const struct soc_enum ip_swap_enum =
	SOC_ENUM_SINGLE(CS42L73_MIOPC, 6,
		ARRAY_SIZE(cs42l73_ip_swap_text), cs42l73_ip_swap_text);

static const char * const cs42l73_spo_mixer_text[] = {"Mono", "Stereo"};

static const struct soc_enum vsp_output_mux_enum =
	SOC_ENUM_SINGLE(CS42L73_MIXERCTL, 5,
		ARRAY_SIZE(cs42l73_spo_mixer_text), cs42l73_spo_mixer_text);

static const struct soc_enum xsp_output_mux_enum =
	SOC_ENUM_SINGLE(CS42L73_MIXERCTL, 4,
		ARRAY_SIZE(cs42l73_spo_mixer_text), cs42l73_spo_mixer_text);

static const struct snd_kcontrol_new hl_left_mixer[] = {
	SOC_DAPM_SINGLE_TLV("HL-IP Left Volume",
			CS42L73_HLAIPAA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("HL-XSP Left Volume",
			CS42L73_HLAXSPAA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("HL-ASP Left Volume",
			CS42L73_HLAASPAA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("HL-VSP Left Volume",
			CS42L73_HLAVSPMA, 0, 0x3F, 1, attn_tlv),
};

static const struct snd_kcontrol_new hl_right_mixer[] = {
	SOC_DAPM_SINGLE_TLV("HL-IP Right Volume",
			CS42L73_HLBIPBA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("HL-XSP Right Volume",
			CS42L73_HLBXSPBA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("HL-ASP Right Volume",
			CS42L73_HLBASPBA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("HL-VSP Right Volume",
			CS42L73_HLBVSPMA, 0, 0x3F, 1, attn_tlv),
};

static const struct snd_kcontrol_new xsp_left_mixer[] = {
	SOC_DAPM_SINGLE_TLV("XSP-IP Left Volume",
			CS42L73_XSPAIPAA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("XSP-XSP Left Volume",
			CS42L73_XSPAXSPAA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("XSP-ASP Left Volume",
			CS42L73_XSPAASPAA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("XSP-VSP Left Volume",
			CS42L73_XSPAVSPMA, 0, 0x3F, 1, attn_tlv),
};

static const struct snd_kcontrol_new xsp_right_mixer[] = {
	SOC_DAPM_SINGLE_TLV("XSP-IP Right Volume",
			CS42L73_XSPBIPBA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("XSP-XSP Right Volume",
			CS42L73_XSPBXSPBA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("XSP-ASP Right Volume",
			CS42L73_XSPBASPBA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("XSP-VSP Right Volume",
			CS42L73_XSPBVSPMA, 0, 0x3F, 1, attn_tlv),
};

static const struct snd_kcontrol_new asp_left_mixer[] = {
	SOC_DAPM_SINGLE_TLV("ASP-IP Left Volume",
			CS42L73_ASPAIPAA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("ASP-XSP Left Volume",
			CS42L73_ASPAXSPAA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("ASP-ASP Left Volume",
			CS42L73_ASPAASPAA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("ASP-VSP Left Volume",
			CS42L73_ASPAVSPMA, 0, 0x3F, 1, attn_tlv),
};

static const struct snd_kcontrol_new asp_right_mixer[] = {
	SOC_DAPM_SINGLE_TLV("ASP-IP Right Volume",
			CS42L73_ASPBIPBA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("ASP-XSP Right Volume",
			CS42L73_ASPBXSPBA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("ASP-ASP Right Volume",
			CS42L73_ASPBASPBA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("ASP-VSP Right Volume",
			CS42L73_ASPBVSPMA, 0, 0x3F, 1, attn_tlv),
};

static const struct snd_kcontrol_new vsp_left_mixer[] = {
	SOC_DAPM_SINGLE_TLV("VSP-IP Left Volume",
			CS42L73_VSPAIPAA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("VSP-XSP Left Volume",
			CS42L73_VSPAXSPAA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("VSP-ASP Left Volume",
			CS42L73_VSPAASPAA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("VSP-VSP Left Volume",
			CS42L73_VSPAVSPMA, 0, 0x3F, 1, attn_tlv),
};

static const struct snd_kcontrol_new vsp_right_mixer[] = {
	SOC_DAPM_SINGLE_TLV("VSP-IP Right Volume",
			CS42L73_VSPBIPBA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("VSP-XSP Right Volume",
			CS42L73_VSPBXSPBA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("VSP-ASP Right Volume",
			CS42L73_VSPBASPBA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("VSP-VSP Right Volume",
			CS42L73_VSPBVSPMA, 0, 0x3F, 1, attn_tlv),
};

static const struct snd_kcontrol_new spk_mixer[] = {
	SOC_DAPM_SINGLE_TLV("SPK-IP Mono Volume",
			CS42L73_SPKMIPMA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("SPK-XSP Mono Volume",
			CS42L73_SPKMXSPA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("SPK-ASP Mono Volume",
			CS42L73_SPKMASPA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("SPK-VSP Mono Volume",
			CS42L73_SPKMVSPMA, 0, 0x3F, 1, attn_tlv),
};

static const struct snd_kcontrol_new esl_mixer[] = {
	SOC_DAPM_SINGLE_TLV("ESL-IP Mono Volume",
			CS42L73_ESLMIPMA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("ESL-XSP Mono Volume",
			CS42L73_ESLMXSPA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("ESL-ASP Mono Volume",
			CS42L73_ESLMASPA, 0, 0x3F, 1, attn_tlv),
	SOC_DAPM_SINGLE_TLV("ESL-VSP Mono Volume",
			CS42L73_ESLMVSPMA, 0, 0x3F, 1, attn_tlv),
};

static const struct snd_kcontrol_new cs42l73_snd_controls[] = {
	SOC_DOUBLE_R_SX_TLV("Headphone Analog Playback Volume",
			CS42L73_HPAAVOL, CS42L73_HPBAVOL, 7,
			0xffffffC1, 0x0C, hpaloa_tlv),

	SOC_DOUBLE_R_SX_TLV("LineOut Analog Playback Volume", CS42L73_LOAAVOL,
			CS42L73_LOBAVOL, 7, 0xffffffC1, 0x0C, hpaloa_tlv),

	SOC_DOUBLE_R_SX_TLV("Input PGA Analog Volume", CS42L73_MICAPREPGAAVOL,
			CS42L73_MICBPREPGABVOL, 6, 0xffffffF4,
			0x18, micpga_tlv),

	SOC_DOUBLE_R("MIC Preamp Switch", CS42L73_MICAPREPGAAVOL,
			CS42L73_MICBPREPGABVOL, 6, 1, 0),

	SOC_DOUBLE_R_SX_TLV("Input Path Digital Volume", CS42L73_IPADVOL,
			CS42L73_IPBDVOL, 8, 0xffffffA0, 0x0C, ipd_tlv),

	SOC_DOUBLE_R_SX_TLV("HL Digital Playback Volume",
			CS42L73_HLADVOL, CS42L73_HLBDVOL, 8, 0xffffff34,
			0x18, hl_tlv),

	SOC_SINGLE_TLV("ADC A Boost Volume",
			CS42L73_ADCIPC, 2, 0x01, 0, adc_boost_tlv),

	SOC_SINGLE_TLV("ADC B Boost Volume",
			CS42L73_ADCIPC, 6, 0x01, 0, adc_boost_tlv),

	SOC_DOUBLE_R_SX_TLV("Speakerphone Digital Playback Volume",
			CS42L73_SPKDVOL, CS42L73_SPKDVOL, 8, 0xffffff34, 0x18, hl_tlv),

	SOC_DOUBLE_R_SX_TLV("Ear Speaker Digital Playback Volume",
			CS42L73_ESLDVOL, CS42L73_ESLDVOL, 8, 0xffffff34, 0x18, hl_tlv),

	SOC_DOUBLE("Input Path Digital Switch", CS42L73_ADCIPC, 0, 4, 1, 1),
	SOC_SINGLE("PGA Soft-Ramp Switch", CS42L73_MIOPC, 3, 1, 0),
	SOC_SINGLE("Analog Zero Cross Switch", CS42L73_MIOPC, 2, 1, 0),
	SOC_SINGLE("Digital Soft-Ramp Switch", CS42L73_MIOPC, 1, 1, 0),
	SOC_SINGLE("Analog Output Soft-Ramp Switch", CS42L73_MIOPC, 0, 1, 0),

	SOC_DOUBLE("ADC Signal Polarity Switch", CS42L73_ADCIPC, 1, 5, 1,
			0),

	SOC_SINGLE("HL Limiter Attack Rate", CS42L73_LIMARATEHL, 0, 0x3F,
			0),
	SOC_SINGLE("HL Limiter Release Rate", CS42L73_LIMRRATEHL, 0,
			0x3F, 0),


	SOC_SINGLE("HL Limiter Switch", CS42L73_LIMRRATEHL, 7, 1, 0),
	SOC_SINGLE("HL Limiter All Channels Switch", CS42L73_LIMRRATEHL, 6, 1,
			0),

	SOC_SINGLE_TLV("HL Limiter Max Threshold Volume", CS42L73_LMAXHL, 5, 7,
			1, limiter_tlv),

	SOC_SINGLE_TLV("HL Limiter Cushion Volume", CS42L73_LMAXHL, 2, 7, 1,
			limiter_tlv),

	SOC_SINGLE("SPK Limiter Attack Rate Volume", CS42L73_LIMARATESPK, 0,
			0x3F, 0),
	SOC_SINGLE("SPK Limiter Release Rate Volume", CS42L73_LIMRRATESPK, 0,
			0x3F, 0),
	SOC_SINGLE("SPK Limiter Switch", CS42L73_LIMRRATESPK, 7, 1, 0),
	SOC_SINGLE("SPK Limiter All Channels Switch", CS42L73_LIMRRATESPK,
			6, 1, 0),
	SOC_SINGLE_TLV("SPK Limiter Max Threshold Volume", CS42L73_LMAXSPK, 5,
			7, 1, limiter_tlv),

	SOC_SINGLE_TLV("SPK Limiter Cushion Volume", CS42L73_LMAXSPK, 2, 7, 1,
			limiter_tlv),

	SOC_SINGLE("ESL Limiter Attack Rate Volume", CS42L73_LIMARATEESL, 0,
			0x3F, 0),
	SOC_SINGLE("ESL Limiter Release Rate Volume", CS42L73_LIMRRATEESL, 0,
			0x3F, 0),
	SOC_SINGLE("ESL Limiter Switch", CS42L73_LIMRRATEESL, 7, 1, 0),
	SOC_SINGLE_TLV("ESL Limiter Max Threshold Volume", CS42L73_LMAXESL, 5,
			7, 1, limiter_tlv),

	SOC_SINGLE_TLV("ESL Limiter Cushion Volume", CS42L73_LMAXESL, 2, 7, 1,
			limiter_tlv),

	SOC_SINGLE("ALC Attack Rate Volume", CS42L73_ALCARATE, 0, 0x3F, 0),
	SOC_SINGLE("ALC Release Rate Volume", CS42L73_ALCRRATE, 0, 0x3F, 0),
	SOC_DOUBLE("ALC Switch", CS42L73_ALCARATE, 6, 7, 1, 0),
	SOC_SINGLE_TLV("ALC Max Threshold Volume", CS42L73_ALCMINMAX, 5, 7, 1,
			limiter_tlv),
	SOC_SINGLE_TLV("ALC Min Threshold Volume", CS42L73_ALCMINMAX, 2, 7, 1,
			limiter_tlv),

	SOC_DOUBLE("NG Enable Switch", CS42L73_NGCAB, 6, 7, 1, 0),
	SOC_SINGLE("NG Boost Switch", CS42L73_NGCAB, 5, 1, 0),
	/*
	    NG Threshold depends on NG_BOOTSAB, which selects
	    between two threshold scales in decibels.
	    Set linear values for now ..
	*/
	SOC_SINGLE("NG Threshold", CS42L73_NGCAB, 2, 7, 0),
	SOC_ENUM("NG Delay", ng_delay_enum),

	SOC_ENUM("Charge Pump Frequency", charge_pump_enum),

	SOC_ENUM("IP Digital Swap/Mono Select", ip_swap_enum),

	SOC_ENUM("VSPOUT Mono/Stereo Select", vsp_output_mux_enum),
	SOC_ENUM("XSPOUT Mono/Stereo Select", xsp_output_mux_enum),
};

static int cs42l73_spklo_amp_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct cs42l73_private *priv = snd_soc_codec_get_drvdata(codec);
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		msleep(140);
		snd_soc_update_bits(codec, CS42L73_PBDC, ESLD_MUTE, 0);
		codec->pmdown_time = 300;
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, CS42L73_PBDC, ESLD_MUTE, ESLD_MUTE);
		/* 150 ms delay between setting PDN and MCLKDIS */
		priv->shutdwn_delay = 150;
		break;
	default:
		dev_err(codec->dev, "Invalid event = 0x%x\n", event);
	}
	return 0;
}

static int cs42l73_spk_amp_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct cs42l73_private *priv = snd_soc_codec_get_drvdata(codec);
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		msleep(140);
		snd_soc_update_bits(codec, CS42L73_PBDC, SPKD_MUTE, 0);
		codec->pmdown_time = 300;
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, CS42L73_PBDC, SPKD_MUTE, SPKD_MUTE);
		/* 150 ms delay between setting PDN and MCLKDIS */
		priv->shutdwn_delay = 150;
		break;
	default:
		dev_err(codec->dev, "Invalid event = 0x%x\n", event);
	}
	return 0;
}

static int cs42l73_ear_amp_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct cs42l73_private *priv = snd_soc_codec_get_drvdata(codec);
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		msleep(46);
		snd_soc_update_bits(codec, CS42L73_PBDC, ESLD_MUTE, 0);
		codec->pmdown_time = 100;
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, CS42L73_PBDC, ESLD_MUTE, ESLD_MUTE);
		/* 50 ms delay between setting PDN and MCLKDIS */
		if (priv->shutdwn_delay < 50)
			priv->shutdwn_delay = 50;
		break;
	default:
		dev_err(codec->dev, "Invalid event = 0x%x\n", event);
	}
	return 0;
}


static int cs42l73_hp_amp_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct cs42l73_private *priv = snd_soc_codec_get_drvdata(codec);
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		msleep(70);
		snd_soc_update_bits(codec, CS42L73_HPAAVOL, HPA_MUTE, 0);
		snd_soc_update_bits(codec, CS42L73_HPBAVOL, HPA_MUTE, 0);
		snd_soc_update_bits(codec, CS42L73_PBDC, HLAD_MUTE, 0);
		snd_soc_update_bits(codec, CS42L73_PBDC, HLBD_MUTE, 0);
		codec->pmdown_time = 100;
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, CS42L73_HPAAVOL, HPA_MUTE, HPA_MUTE);
		snd_soc_update_bits(codec, CS42L73_HPBAVOL, HPA_MUTE, HPA_MUTE);
		snd_soc_update_bits(codec, CS42L73_PBDC, HLAD_MUTE, HLAD_MUTE);
		snd_soc_update_bits(codec, CS42L73_PBDC, HLBD_MUTE, HLBD_MUTE);
		/* 30 ms delay between setting PDN and MCLKDIS */
		if (priv->shutdwn_delay < 30)
			priv->shutdwn_delay = 30;
		break;
	default:
		dev_err(codec->dev, "Invalid event = 0x%x\n", event);
	}
	return 0;
}

static int cs42l73_lo_amp_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct cs42l73_private *priv = snd_soc_codec_get_drvdata(codec);
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		msleep(70);
		snd_soc_update_bits(codec, CS42L73_LOAAVOL, LOA_MUTE, 0);
		snd_soc_update_bits(codec, CS42L73_LOBAVOL, LOA_MUTE, 0);
		snd_soc_update_bits(codec, CS42L73_PBDC, HLAD_MUTE, 0);
		snd_soc_update_bits(codec, CS42L73_PBDC, HLBD_MUTE, 0);
		codec->pmdown_time = 100;
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, CS42L73_LOAAVOL, LOA_MUTE, LOA_MUTE);
		snd_soc_update_bits(codec, CS42L73_LOBAVOL, LOA_MUTE, LOA_MUTE);
		snd_soc_update_bits(codec, CS42L73_PBDC, HLAD_MUTE, HLAD_MUTE);
		snd_soc_update_bits(codec, CS42L73_PBDC, HLBD_MUTE, HLBD_MUTE);
		/* 50 ms delay between setting PDN and MCLKDIS */
		if (priv->shutdwn_delay < 50)
			priv->shutdwn_delay = 50;
		break;
	default:
		dev_err(codec->dev, "Invalid event = 0x%x\n", event);
	}
	return 0;
}

static const struct snd_soc_dapm_widget cs42l73_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("DMICA"),
	SND_SOC_DAPM_INPUT("DMICB"),
	SND_SOC_DAPM_INPUT("LINEINA"),
	SND_SOC_DAPM_INPUT("LINEINB"),
	SND_SOC_DAPM_INPUT("MIC1"),
	SND_SOC_DAPM_REGULATOR_SUPPLY("MIC1 Bias", 0),
	SND_SOC_DAPM_INPUT("MIC2"),
	SND_SOC_DAPM_REGULATOR_SUPPLY("MIC2 Bias", 0),
	SND_SOC_DAPM_SUPPLY("MCLK", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("VSPINOUT", CS42L73_PWRCTL2, 4, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY("ASPIN", CS42L73_PWRCTL2, 2, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY("ASPOUT", CS42L73_PWRCTL2, 3, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY("XSPIN", CS42L73_PWRCTL2, 0, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY("XSPOUT", CS42L73_PWRCTL2, 1, 1, NULL, 0),

	SND_SOC_DAPM_AIF_OUT("XSPOUTL", "XSP Capture",  0,
			SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("XSPOUTR", "XSP Capture",  0,
			SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("ASPOUTL", "ASP Capture",  0,
			SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("ASPOUTR", "ASP Capture",  0,
			SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("VSPOUTL", "VSP Capture",  0,
			SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("VSPOUTR", "VSP Capture",  0,
			SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_PGA("PGA Left", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("PGA Right", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MUX("PGA Left Mux", SND_SOC_NOPM, 0, 0, &pgaa_mux),
	SND_SOC_DAPM_MUX("PGA Right Mux", SND_SOC_NOPM, 0, 0, &pgab_mux),

	SND_SOC_DAPM_ADC("ADC Left", NULL, CS42L73_PWRCTL1, 5, 1),
	SND_SOC_DAPM_ADC("ADC Right", NULL, CS42L73_PWRCTL1, 7, 1),
	SND_SOC_DAPM_ADC("DMIC Left", NULL, CS42L73_PWRCTL1, 4, 1),
	SND_SOC_DAPM_ADC("DMIC Right", NULL, CS42L73_PWRCTL1, 6, 1),

	SND_SOC_DAPM_MIXER("Input Left Capture", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Input Right Capture", SND_SOC_NOPM, 0, 0, NULL, 0),

	SOC_MIXER_NAMED_CTL_ARRAY("ASPL Output Mixer", SND_SOC_NOPM, 0, 0, asp_left_mixer),
	SOC_MIXER_NAMED_CTL_ARRAY("ASPR Output Mixer", SND_SOC_NOPM, 0, 0, asp_right_mixer),
	SOC_MIXER_NAMED_CTL_ARRAY("XSPL Output Mixer", SND_SOC_NOPM, 0, 0, xsp_left_mixer),
	SOC_MIXER_NAMED_CTL_ARRAY("XSPR Output Mixer", SND_SOC_NOPM, 0, 0, xsp_right_mixer),
	SOC_MIXER_NAMED_CTL_ARRAY("VSPL Output Mixer", SND_SOC_NOPM, 0, 0, vsp_left_mixer),
	SOC_MIXER_NAMED_CTL_ARRAY("VSPR Output Mixer", SND_SOC_NOPM, 0, 0, vsp_right_mixer),

	SND_SOC_DAPM_AIF_IN("XSPINL", "XSP Playback", 0,
				SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("XSPINR", "XSP Playback", 0,
				SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("XSPINM", "XSP Playback", 0,
				SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_IN("ASPINL", "ASP Playback", 0,
				SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("ASPINR", "ASP Playback", 0,
				SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("ASPINM", "ASP Playback", 0,
				SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_IN("VSPIN", "VSP Playback", 0,
				SND_SOC_NOPM, 0, 0),

	SOC_MIXER_NAMED_CTL_ARRAY("HL Left Mixer", SND_SOC_NOPM, 0, 0, hl_left_mixer),
	SOC_MIXER_NAMED_CTL_ARRAY("HL Right Mixer", SND_SOC_NOPM, 0, 0, hl_right_mixer),
	SOC_MIXER_NAMED_CTL_ARRAY("SPK Mixer", SND_SOC_NOPM, 0, 0, spk_mixer),
	SOC_MIXER_NAMED_CTL_ARRAY("ESL Mixer", SND_SOC_NOPM, 0, 0, esl_mixer),

	SND_SOC_DAPM_MUX("ESL-XSP Mux", SND_SOC_NOPM,
			 0, 0, &esl_xsp_mux),

	SND_SOC_DAPM_MUX("ESL-ASP Mux", SND_SOC_NOPM,
			 0, 0, &esl_asp_mux),

	SND_SOC_DAPM_MUX("SPK-ASP Mux", SND_SOC_NOPM,
			 0, 0, &spk_asp_mux),

	SND_SOC_DAPM_MUX("SPK-XSP Mux", SND_SOC_NOPM,
			 0, 0, &spk_xsp_mux),

	SND_SOC_DAPM_DAC("HL Left DAC", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("HL Right DAC", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("SPK DAC", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("ESL DAC", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_PGA_E("HP Amp",  CS42L73_PWRCTL3, 0, 1,
			NULL, 0, cs42l73_hp_amp_event,
			SND_SOC_DAPM_POST_PMU|SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_E("LO Amp", CS42L73_PWRCTL3, 1, 1,
			NULL, 0, cs42l73_lo_amp_event,
			SND_SOC_DAPM_POST_PMU|SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_E("SPK Amp", CS42L73_PWRCTL3, 2, 1,
			NULL, 0, cs42l73_spk_amp_event,
			SND_SOC_DAPM_POST_PMU|SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_E("EAR Amp", CS42L73_PWRCTL3, 3, 1,
			NULL, 0, cs42l73_ear_amp_event,
			SND_SOC_DAPM_POST_PMU|SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_E("SPKLO Amp", CS42L73_PWRCTL3, 4, 1,
			NULL, 0, cs42l73_spklo_amp_event,
			SND_SOC_DAPM_POST_PMU|SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_OUTPUT("HPOUTA"),
	SND_SOC_DAPM_OUTPUT("HPOUTB"),
	SND_SOC_DAPM_OUTPUT("LINEOUTA"),
	SND_SOC_DAPM_OUTPUT("LINEOUTB"),
	SND_SOC_DAPM_OUTPUT("EAROUT"),
	SND_SOC_DAPM_OUTPUT("SPKOUT"),
	SND_SOC_DAPM_OUTPUT("SPKLINEOUT"),
};

static const struct snd_soc_dapm_route cs42l73_audio_map[] = {

	/* SPKLO EARSPK Paths */
	{"EAROUT", NULL, "EAR Amp"},
	{"SPKLINEOUT", NULL, "SPKLO Amp"},

	{"EAR Amp", NULL, "ESL DAC"},
	{"SPKLO Amp", NULL, "ESL DAC"},

	{"ESL DAC", NULL, "ESL Mixer"},

	{"ESL Mixer", "ESL-ASP Mono Volume", "ESL-ASP Mux"},
	{"ESL Mixer", "ESL-XSP Mono Volume", "ESL-XSP Mux"},
	{"ESL Mixer", "ESL-VSP Mono Volume", "VSPIN"},
	/* Loopback */
	{"ESL Mixer", "ESL-IP Mono Volume", "Input Left Capture"},
	{"ESL Mixer", "ESL-IP Mono Volume", "Input Right Capture"},

	{"ESL-ASP Mux", "Left", "ASPINL"},
	{"ESL-ASP Mux", "Right", "ASPINR"},
	{"ESL-ASP Mux", "Mono Mix", "ASPINM"},

	{"ESL-XSP Mux", "Left", "XSPINL"},
	{"ESL-XSP Mux", "Right", "XSPINR"},
	{"ESL-XSP Mux", "Mono Mix", "XSPINM"},

	/* Speakerphone Paths */
	{"SPKOUT", NULL, "SPK Amp"},
	{"SPK Amp", NULL, "SPK DAC"},

	{"SPK DAC", NULL, "SPK Mixer"},

	{"SPK Mixer", "SPK-ASP Mono Volume", "SPK-ASP Mux"},
	{"SPK Mixer", "SPK-XSP Mono Volume", "SPK-XSP Mux"},
	{"SPK Mixer", "SPK-VSP Mono Volume", "VSPIN"},
	/* Loopback */
	{"SPK Mixer", "SPK-IP Mono Volume", "Input Left Capture"},
	{"SPK Mixer", "SPK-IP Mono Volume", "Input Right Capture"},

	{"SPK-ASP Mux", "Left", "ASPINL"},
	{"SPK-ASP Mux", "Mono Mix", "ASPINM"},
	{"SPK-ASP Mux", "Right", "ASPINR"},

	{"SPK-XSP Mux", "Left", "XSPINL"},
	{"SPK-XSP Mux", "Mono Mix", "XSPINM"},
	{"SPK-XSP Mux", "Right", "XSPINR"},

	/* HP LineOUT Paths */
	{"HPOUTA", NULL, "HP Amp"},
	{"HPOUTB", NULL, "HP Amp"},
	{"LINEOUTA", NULL, "LO Amp"},
	{"LINEOUTB", NULL, "LO Amp"},

	{"HP Amp", NULL, "HL Left DAC"},
	{"HP Amp", NULL, "HL Right DAC"},
	{"LO Amp", NULL, "HL Left DAC"},
	{"LO Amp", NULL, "HL Right DAC"},

	{"HL Left DAC", NULL, "HL Left Mixer"},
	{"HL Right DAC", NULL, "HL Right Mixer"},

	{"HL Left Mixer", "HL-XSP Left Volume", "XSPINL"},
	{"HL Right Mixer", "HL-XSP Right Volume", "XSPINR"},
	{"HL Left Mixer", "HL-ASP Left Volume", "ASPINL"},
	{"HL Right Mixer", "HL-ASP Right Volume", "ASPINR"},
	{"HL Left Mixer", "HL-VSP Left Volume", "VSPIN"},
	{"HL Right Mixer", "HL-VSP Right Volume", "VSPIN"},
	/* Loopback */
	{"HL Left Mixer", "HL-IP Left Volume", "Input Left Capture"},
	{"HL Right Mixer", "HL-IP Right Volume", "Input Right Capture"},

	{"ASPINL", NULL, "ASPIN"},
	{"ASPINR", NULL, "ASPIN"},
	{"ASPINM", NULL, "ASPIN"},
	{"XSPINL", NULL, "XSPIN"},
	{"XSPINR", NULL, "XSPIN"},
	{"XSPINM", NULL, "XSPIN"},
	{"VSPIN", NULL, "VSPINOUT"},

	/* Capture Paths */
	{"MIC1", NULL, "MIC1 Bias"},
	{"PGA Left Mux", "Mic 1", "MIC1"},
	{"MIC2", NULL, "MIC2 Bias"},
	{"PGA Right Mux", "Mic 2", "MIC2"},

	{"PGA Left Mux", "Line A", "LINEINA"},
	{"PGA Right Mux", "Line B", "LINEINB"},

	{"PGA Left", NULL, "PGA Left Mux"},
	{"PGA Right", NULL, "PGA Right Mux"},

	{"ADC Left", NULL, "PGA Left"},
	{"ADC Right", NULL, "PGA Right"},
	{"DMIC Left", NULL, "DMICA"},
	{"DMIC Right", NULL, "DMICB"},

	{"Input Left Capture", NULL, "ADC Left"},
	{"Input Right Capture", NULL, "ADC Right"},
	{"Input Left Capture", NULL, "DMIC Left"},
	{"Input Right Capture", NULL, "DMIC Right"},

	/* Audio Capture */
	{"ASPL Output Mixer", "ASP-IP Left Volume", "Input Left Capture"},
	{"ASPR Output Mixer", "ASP-IP Right Volume", "Input Right Capture"},
	{"ASPL Output Mixer", "ASP-XSP Left Volume", "XSPINL"},
	{"ASPR Output Mixer", "ASP-XSP Right Volume", "XSPINR"},
	{"ASPL Output Mixer", "ASP-ASP Left Volume", "ASPINL"},
	{"ASPR Output Mixer", "ASP-ASP Right Volume", "ASPINR"},
	{"ASPL Output Mixer", "ASP-VSP Left Volume", "VSPIN"},
	{"ASPR Output Mixer", "ASP-VSP Right Volume", "VSPIN"},

	{"ASPOUTL", NULL, "ASPL Output Mixer"},
	{"ASPOUTR", NULL, "ASPR Output Mixer"},

	{"ASPOUTL", NULL, "ASPOUT"},
	{"ASPOUTR", NULL, "ASPOUT"},

	/* Auxillary Capture */
	{"XSPL Output Mixer", "XSP-IP Left Volume", "Input Left Capture"},
	{"XSPR Output Mixer", "XSP-IP Right Volume", "Input Right Capture"},
	{"XSPL Output Mixer", "XSP-XSP Left Volume", "XSPINL"},
	{"XSPR Output Mixer", "XSP-XSP Right Volume", "XSPINR"},
	{"XSPL Output Mixer", "XSP-ASP Left Volume", "ASPINL"},
	{"XSPR Output Mixer", "XSP-ASP Right Volume", "ASPINR"},
	{"XSPL Output Mixer", "XSP-VSP Left Volume", "VSPIN"},
	{"XSPR Output Mixer", "XSP-VSP Right Volume", "VSPIN"},

	{"XSPOUTL", NULL, "XSPL Output Mixer"},
	{"XSPOUTR", NULL, "XSPR Output Mixer"},

	{"XSPOUTL", NULL, "XSPOUT"},
	{"XSPOUTR", NULL, "XSPOUT"},

	/* Voice Capture */
	{"VSPL Output Mixer", "VSP-IP Left Volume", "Input Left Capture"},
	{"VSPR Output Mixer", "VSP-IP Right Volume", "Input Right Capture"},
	{"VSPL Output Mixer", "VSP-XSP Left Volume", "XSPINL"},
	{"VSPR Output Mixer", "VSP-XSP Right Volume", "XSPINR"},
	{"VSPL Output Mixer", "VSP-ASP Left Volume", "ASPINL"},
	{"VSPR Output Mixer", "VSP-ASP Right Volume", "ASPINR"},
	{"VSPL Output Mixer", "VSP-VSP Left Volume", "VSPIN"},
	{"VSPR Output Mixer", "VSP-VSP Right Volume", "VSPIN"},

	{"VSPOUTL", NULL, "VSPL Output Mixer"},
	{"VSPOUTR", NULL, "VSPR Output Mixer"},

	{"VSPOUTL", NULL, "VSPINOUT"},
	{"VSPOUTR", NULL, "VSPINOUT"},
};

struct cs42l73_mclk_div {
	u32 mclk;
	u32 srate;
	u8 mmcc;
};

static struct cs42l73_mclk_div cs42l73_mclk_coeffs[] = {
	/* MCLK, Sample Rate, xMMCC[5:0] */
	{5644800, 11025, 0x30},
	{5644800, 22050, 0x20},
	{5644800, 44100, 0x10},

	{6000000,  8000, 0x39},
	{6000000, 11025, 0x33},
	{6000000, 12000, 0x31},
	{6000000, 16000, 0x29},
	{6000000, 22050, 0x23},
	{6000000, 24000, 0x21},
	{6000000, 32000, 0x19},
	{6000000, 44100, 0x13},
	{6000000, 48000, 0x11},

	{6144000,  8000, 0x38},
	{6144000, 12000, 0x30},
	{6144000, 16000, 0x28},
	{6144000, 24000, 0x20},
	{6144000, 32000, 0x18},
	{6144000, 48000, 0x10},

	{6500000,  8000, 0x3C},
	{6500000, 11025, 0x35},
	{6500000, 12000, 0x34},
	{6500000, 16000, 0x2C},
	{6500000, 22050, 0x25},
	{6500000, 24000, 0x24},
	{6500000, 32000, 0x1C},
	{6500000, 44100, 0x15},
	{6500000, 48000, 0x14},

	{6400000,  8000, 0x3E},
	{6400000, 11025, 0x37},
	{6400000, 12000, 0x36},
	{6400000, 16000, 0x2E},
	{6400000, 22050, 0x27},
	{6400000, 24000, 0x26},
	{6400000, 32000, 0x1E},
	{6400000, 44100, 0x17},
	{6400000, 48000, 0x16},
};

struct cs42l73_mclkx_div {
	u32 mclkx;
	u8 ratio;
	u8 mclkdiv;
};

static struct cs42l73_mclkx_div cs42l73_mclkx_coeffs[] = {
	{5644800,  1, 0},	/* 5644800 */
	{6000000,  1, 0},	/* 6000000 */
	{6144000,  1, 0},	/* 6144000 */
	{11289600, 2, 2},	/* 5644800 */
	{12288000, 2, 2},	/* 6144000 */
	{12000000, 2, 2},	/* 6000000 */
	{13000000, 2, 2},	/* 6500000 */
	{19200000, 3, 3},	/* 6400000 */
	{24000000, 4, 4},	/* 6000000 */
	{26000000, 4, 4},	/* 6500000 */
	{38400000, 6, 5}	/* 6400000 */
};

static int cs42l73_get_mclkx_coeff(int mclkx)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cs42l73_mclkx_coeffs); i++) {
		if (cs42l73_mclkx_coeffs[i].mclkx == mclkx)
			return i;
	}
	return -EINVAL;
}

static int cs42l73_get_mclk_coeff(int mclk, int srate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cs42l73_mclk_coeffs); i++) {
		if (cs42l73_mclk_coeffs[i].mclk == mclk &&
		    cs42l73_mclk_coeffs[i].srate == srate)
			return i;
	}
	return -EINVAL;

}

static int cs42l73_set_mclk(struct snd_soc_dai *dai, unsigned int freq)
{
	struct snd_soc_codec *codec = dai->codec;
	struct cs42l73_private *priv = snd_soc_codec_get_drvdata(codec);

	int mclkx_coeff;
	u32 mclk = 0;
	u8 dmmcc = 0;

	/* MCLKX -> MCLK */
	mclkx_coeff = cs42l73_get_mclkx_coeff(freq);
	if (mclkx_coeff < 0)
		return mclkx_coeff;

	mclk = cs42l73_mclkx_coeffs[mclkx_coeff].mclkx /
		cs42l73_mclkx_coeffs[mclkx_coeff].ratio;

	dev_dbg(codec->dev, "MCLK%u %u  <-> internal MCLK %u\n",
		 priv->mclksel + 1, cs42l73_mclkx_coeffs[mclkx_coeff].mclkx,
		 mclk);

	dmmcc = (priv->mclksel << 4) |
		(cs42l73_mclkx_coeffs[mclkx_coeff].mclkdiv << 1);

	snd_soc_update_bits(codec, CS42L73_DMMCC, 0x1e, dmmcc);

	priv->sysclk = mclkx_coeff;
	priv->mclk = mclk;

	return 0;
}

static int cs42l73_set_sysclk(struct snd_soc_dai *dai,
			      int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct cs42l73_private *priv = snd_soc_codec_get_drvdata(codec);

	switch (clk_id) {
	case CS42L73_CLKID_MCLK1:
		break;
	case CS42L73_CLKID_MCLK2:
		break;
	default:
		return -EINVAL;
	}

	priv->mclksel = clk_id;
	if ((cs42l73_set_mclk(dai, freq)) < 0) {
		dev_err(codec->dev, "Unable to set MCLK for dai %s\n",
			dai->name);
		return -EINVAL;
	}

	return 0;
}

static int cs42l73_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct cs42l73_private *priv = snd_soc_codec_get_drvdata(codec);
	u8 id = codec_dai->id;
	unsigned int inv, format;
	u8 spc, mmcc;

	spc = snd_soc_read(codec, CS42L73_SPC(id));
	mmcc = snd_soc_read(codec, CS42L73_MMCC(id));

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		mmcc |= MS_MASTER;
		break;

	case SND_SOC_DAIFMT_CBS_CFS:
		mmcc &= ~MS_MASTER;
		break;

	default:
		return -EINVAL;
	}

	format = (fmt & SND_SOC_DAIFMT_FORMAT_MASK);
	inv = (fmt & SND_SOC_DAIFMT_INV_MASK);

	switch (format) {
	case SND_SOC_DAIFMT_I2S:
		spc &= ~SPDIF_PCM;
		break;
	case SND_SOC_DAIFMT_DSP_A:
	case SND_SOC_DAIFMT_DSP_B:
		if (mmcc & MS_MASTER) {
			dev_err(codec->dev,
				"PCM format in slave mode only\n");
			return -EINVAL;
		}
		if (id == CS42L73_ASP) {
			dev_err(codec->dev,
				"PCM format is not supported on ASP port\n");
			return -EINVAL;
		}
		spc |= SPDIF_PCM;
		break;
	default:
		return -EINVAL;
	}

	if (spc & SPDIF_PCM) {
		/* Clear PCM mode, clear PCM_BIT_ORDER bit for MSB->LSB */
		spc &= ~(PCM_MODE_MASK | PCM_BIT_ORDER);
		switch (format) {
		case SND_SOC_DAIFMT_DSP_B:
			if (inv == SND_SOC_DAIFMT_IB_IF)
				spc |= PCM_MODE0;
			if (inv == SND_SOC_DAIFMT_IB_NF)
				spc |= PCM_MODE1;
		break;
		case SND_SOC_DAIFMT_DSP_A:
			if (inv == SND_SOC_DAIFMT_IB_IF)
				spc |= PCM_MODE1;
			break;
		default:
			return -EINVAL;
		}
	}

	priv->config[id].spc = spc;
	priv->config[id].mmcc = mmcc;

	return 0;
}

static u32 cs42l73_asrc_rates[] = {
	8000, 11025, 12000, 16000, 22050,
	24000, 32000, 44100, 48000
};

static unsigned int cs42l73_get_xspfs_coeff(u32 rate)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(cs42l73_asrc_rates); i++) {
		if (cs42l73_asrc_rates[i] == rate)
			return i + 1;
	}
	return 0;		/* 0 = Don't know */
}

static void cs42l73_update_asrc(struct snd_soc_codec *codec, int id, int srate)
{
	u8 spfs = 0;

	if (srate > 0)
		spfs = cs42l73_get_xspfs_coeff(srate);

	switch (id) {
	case CS42L73_XSP:
		snd_soc_update_bits(codec, CS42L73_VXSPFS, 0x0f, spfs);
	break;
	case CS42L73_ASP:
		snd_soc_update_bits(codec, CS42L73_ASPC, 0x3c, spfs << 2);
	break;
	case CS42L73_VSP:
		snd_soc_update_bits(codec, CS42L73_VXSPFS, 0xf0, spfs << 4);
	break;
	default:
	break;
	}
}

static int cs42l73_pcm_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct cs42l73_private *priv = snd_soc_codec_get_drvdata(codec);
	int id = dai->id;
	int mclk_coeff;
	int srate = params_rate(params);

	if (priv->config[id].mmcc & MS_MASTER) {
		/* CS42L73 Master */
		/* MCLK -> srate */
		mclk_coeff =
		    cs42l73_get_mclk_coeff(priv->mclk, srate);

		if (mclk_coeff < 0)
			return -EINVAL;

		dev_dbg(codec->dev,
			 "DAI[%d]: MCLK %u, srate %u, MMCC[5:0] = %x\n",
			 id, priv->mclk, srate,
			 cs42l73_mclk_coeffs[mclk_coeff].mmcc);

		priv->config[id].mmcc &= 0xC0;
		priv->config[id].mmcc |= cs42l73_mclk_coeffs[mclk_coeff].mmcc;
		priv->config[id].spc &= 0xFC;
		priv->config[id].spc |= MCK_SCLK_MCLK;
	} else {
		/* CS42L73 Slave */
		priv->config[id].spc &= 0xFC;
		priv->config[id].spc |= MCK_SCLK_64FS;
	}
	/* Update ASRCs */
	priv->config[id].srate = srate;

	snd_soc_write(codec, CS42L73_SPC(id), priv->config[id].spc);
	snd_soc_write(codec, CS42L73_MMCC(id), priv->config[id].mmcc);

	cs42l73_update_asrc(codec, id, srate);

	return 0;
}

static int cs42l73_set_bias_level(struct snd_soc_codec *codec,
				  enum snd_soc_bias_level level)
{
	struct cs42l73_private *cs42l73 = snd_soc_codec_get_drvdata(codec);
	struct cs42l73_platform_data *pdata = codec->dev->platform_data;

	pr_info("%s(): set bias level %d\n", __func__, level);

	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
		break;

	case SND_SOC_BIAS_STANDBY:
		if (codec->dapm.bias_level == SND_SOC_BIAS_OFF) {
			if (pdata->clk_enable)
				pdata->clk_enable();
			regcache_cache_only(cs42l73->regmap, false);
			regcache_sync(cs42l73->regmap);
		}
		snd_soc_update_bits(codec, CS42L73_DMMCC, MCLKDIS, 0);
		break;

	case SND_SOC_BIAS_OFF:
		if (cs42l73->shutdwn_delay > 0) {
			msleep(cs42l73->shutdwn_delay);
			cs42l73->shutdwn_delay = 0;
		} else {
			msleep(15); /* Min amount of time requred to power
				     * down.
				     */
		}
		snd_soc_update_bits(codec, CS42L73_DMMCC, MCLKDIS, 1);
		if (codec->dapm.bias_level != SND_SOC_BIAS_OFF) {
			if (pdata->clk_disable)
				pdata->clk_disable();
		}
		break;
	}
	codec->dapm.bias_level = level;
	return 0;
}

static int cs42l73_set_tristate(struct snd_soc_dai *dai, int tristate)
{
	struct snd_soc_codec *codec = dai->codec;
	int id = dai->id;

	return snd_soc_update_bits(codec, CS42L73_SPC(id),
					0x80, tristate << 7);
}

static struct snd_pcm_hw_constraint_list constraints_12_24 = {
	.count  = ARRAY_SIZE(cs42l73_asrc_rates),
	.list   = cs42l73_asrc_rates,
};

static int cs42l73_pcm_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	snd_pcm_hw_constraint_list(substream->runtime, 0,
					SNDRV_PCM_HW_PARAM_RATE,
					&constraints_12_24);
	return 0;
}

/* SNDRV_PCM_RATE_KNOT -> 12000, 24000 Hz, limit with constraint list */
#define CS42L73_RATES (SNDRV_PCM_RATE_8000_48000 | SNDRV_PCM_RATE_KNOT)


#define CS42L73_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
	SNDRV_PCM_FMTBIT_S24_LE)

static const struct snd_soc_dai_ops cs42l73_ops = {
	.startup = cs42l73_pcm_startup,
	.hw_params = cs42l73_pcm_hw_params,
	.set_fmt = cs42l73_set_dai_fmt,
	.set_sysclk = cs42l73_set_sysclk,
	.set_tristate = cs42l73_set_tristate,
};

static struct snd_soc_dai_driver cs42l73_dai[] = {
	{
		.name = "cs42l73-xsp",
		.id = CS42L73_XSP,
		.playback = {
			.stream_name = "XSP Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = CS42L73_RATES,
			.formats = CS42L73_FORMATS,
		},
		.capture = {
			.stream_name = "XSP Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = CS42L73_RATES,
			.formats = CS42L73_FORMATS,
		},
		.ops = &cs42l73_ops,
		.symmetric_rates = 1,
	 },
	{
		.name = "cs42l73-asp",
		.id = CS42L73_ASP,
		.playback = {
			.stream_name = "ASP Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = CS42L73_RATES,
			.formats = CS42L73_FORMATS,
		},
		.capture = {
			.stream_name = "ASP Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = CS42L73_RATES,
			.formats = CS42L73_FORMATS,
		},
		.ops = &cs42l73_ops,
		.symmetric_rates = 1,
	 },
	{
		.name = "cs42l73-vsp",
		.id = CS42L73_VSP,
		.playback = {
			.stream_name = "VSP Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = CS42L73_RATES,
			.formats = CS42L73_FORMATS,
		},
		.capture = {
			.stream_name = "VSP Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = CS42L73_RATES,
			.formats = CS42L73_FORMATS,
		},
		.ops = &cs42l73_ops,
		.symmetric_rates = 1,
	 }
};

static int cs42l73_probe(struct snd_soc_codec *codec)
{
	int ret;
	struct cs42l73_private *cs42l73 = snd_soc_codec_get_drvdata(codec);

	codec->control_data = cs42l73->regmap;
	cs42l73->codec = codec;

	ret = snd_soc_codec_set_cache_io(codec, 8, 8, SND_SOC_REGMAP);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	regcache_cache_only(cs42l73->regmap, true);

	cs42l73->mclksel = CS42L73_CLKID_MCLK1;	/* MCLK1 as master clk */
	cs42l73->mclk = 0;

	return ret;
}

static int cs42l73_remove(struct snd_soc_codec *codec)
{
	cs42l73_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_cs42l73 = {
	.probe = cs42l73_probe,
	.remove = cs42l73_remove,
	.set_bias_level = cs42l73_set_bias_level,
	.idle_bias_off = true,

	.dapm_widgets = cs42l73_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(cs42l73_dapm_widgets),
	.dapm_routes = cs42l73_audio_map,
	.num_dapm_routes = ARRAY_SIZE(cs42l73_audio_map),

	.controls = cs42l73_snd_controls,
	.num_controls = ARRAY_SIZE(cs42l73_snd_controls),
};

static struct regmap_config cs42l73_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = CS42L73_MAX_REGISTER,
	.reg_defaults = cs42l73_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(cs42l73_reg_defaults),
	.volatile_reg = cs42l73_volatile_register,
	.readable_reg = cs42l73_readable_register,
	.cache_type = REGCACHE_RBTREE,
};

static bool cs42l73_enable_mclk(struct snd_soc_codec *codec)
{
	bool restore = false;

	if (codec->dapm.bias_level == SND_SOC_BIAS_OFF) {
		snd_soc_dapm_force_enable_pin(&codec->dapm, "MCLK");
		snd_soc_dapm_sync(&codec->dapm);
		restore = true;
	}

	return restore;
}

static void cs42l73_disable_mclk(struct snd_soc_codec *codec)
{
	snd_soc_dapm_disable_pin(&codec->dapm, "MCLK");
	snd_soc_dapm_sync(&codec->dapm);
}

static int cs42l73_micbias_enable(struct regulator_dev *dev)
{
	struct cs42l73_private *priv = rdev_get_drvdata(dev);
	unsigned int mask = rdev_get_id(dev);
	bool restore;

	dev_info(&dev->dev, "%s %02x\n", __func__, mask);

	/* need clock to propagate the change */
	restore = cs42l73_enable_mclk(priv->codec);
	regmap_update_bits(priv->regmap, CS42L73_PWRCTL2, mask, 0);
	if (restore)
		cs42l73_disable_mclk(priv->codec);

	return 0;
}

static int cs42l73_micbias_disable(struct regulator_dev *dev)
{
	struct cs42l73_private *priv = rdev_get_drvdata(dev);
	unsigned int mask = rdev_get_id(dev);
	bool restore;

	dev_info(&dev->dev, "%s %02x\n", __func__, mask);

	/* need clock to propagate the change */
	restore = cs42l73_enable_mclk(priv->codec);
	regmap_update_bits(priv->regmap, CS42L73_PWRCTL2, mask, mask);
	if (restore)
		cs42l73_disable_mclk(priv->codec);

	return 0;
}

static int cs42l73_micbias_is_enabled(struct regulator_dev *dev)
{
	struct cs42l73_private *priv = rdev_get_drvdata(dev);
	unsigned int mask = rdev_get_id(dev);
	unsigned int val = 0;

	regmap_read(priv->regmap, CS42L73_PWRCTL2, &val);
	return !(val & mask);
}

static int cs42l73_micbias_list_voltage(struct regulator_dev *dev, unsigned selector)
{
	int ret;

	ret = selector ? CS42L73_MICBIAS_VTYP1 : CS42L73_MICBIAS_VTYP0;
	dev_info(&dev->dev, "%s: selector %d, %duV\n", __func__, selector, ret);

	return ret;
}

static int cs42l73_micbias_set_voltage_sel(struct regulator_dev *dev, unsigned selector)
{
	struct cs42l73_private *priv = rdev_get_drvdata(dev);
	int ret;

	if (selector == 0) {
		ret = regmap_update_bits(priv->regmap, CS42L73_OLMBMSDC, MIC_BIAS_CTRL, 0);
		if (!ret)
			dev_info(&dev->dev, "%s: selector %d supported\n", __func__, selector);
	} else if (selector == 1) {
		ret = regmap_update_bits(priv->regmap, CS42L73_OLMBMSDC, MIC_BIAS_CTRL, MIC_BIAS_CTRL);
		if (!ret)
			dev_info(&dev->dev, "%s: selector %d supported\n", __func__, selector);
	} else {
		dev_info(&dev->dev, "%s: selector %d not supported\n", __func__, selector);
		ret = -EINVAL;
	}

	return ret;
}

static struct regulator_ops cs42l73_micbias_ops = {
	.enable = cs42l73_micbias_enable,
	.disable = cs42l73_micbias_disable,
	.is_enabled = cs42l73_micbias_is_enabled,
	.list_voltage = cs42l73_micbias_list_voltage,
	.set_voltage_sel = cs42l73_micbias_set_voltage_sel,
};

static struct regulator_desc cs42l73_micbias_desc[] = {
	{
		.name = "MIC1 Bias",
		.id = PDN_MIC1_BIAS,
		.n_voltages = CS42L73_MICBIAS_VOLTAGES_NUM,
		.ops = &cs42l73_micbias_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "MIC2 Bias",
		.id = PDN_MIC2_BIAS,
		.n_voltages = CS42L73_MICBIAS_VOLTAGES_NUM,
		.ops = &cs42l73_micbias_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	}
};

static struct regulator_init_data cs42l73_micbias_data = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
		.min_uV = CS42L73_MICBIAS_VMIN0,
		.max_uV = CS42L73_MICBIAS_VMAX1,
	},
};

static __devinit int cs42l73_i2c_probe(struct i2c_client *i2c_client,
				       const struct i2c_device_id *id)
{
	struct cs42l73_platform_data *pdata;
	struct cs42l73_private *cs42l73;
	int i, ret;
	unsigned int devid = 0;
	unsigned int reg;

	pdata = i2c_client->dev.platform_data;
	if (pdata && pdata->setup) {
		ret = pdata->setup();
		if (ret < 0)
			return ret;
	}

	cs42l73 = devm_kzalloc(&i2c_client->dev, sizeof(struct cs42l73_private),
			       GFP_KERNEL);
	if (!cs42l73) {
		dev_err(&i2c_client->dev, "could not allocate codec\n");
		ret = -ENOMEM;
		goto err;
	}

	i2c_set_clientdata(i2c_client, cs42l73);

	cs42l73->regmap = regmap_init_i2c(i2c_client, &cs42l73_regmap);
	if (IS_ERR(cs42l73->regmap)) {
		ret = PTR_ERR(cs42l73->regmap);
		dev_err(&i2c_client->dev, "regmap_init() failed: %d\n", ret);
		goto err;
	}
	/* initialize codec */
	ret = regmap_read(cs42l73->regmap, CS42L73_DEVID_AB, &reg);
	devid = (reg & 0xFF) << 12;

	ret = regmap_read(cs42l73->regmap, CS42L73_DEVID_CD, &reg);
	devid |= (reg & 0xFF) << 4;

	ret = regmap_read(cs42l73->regmap, CS42L73_DEVID_E, &reg);
	devid |= (reg & 0xF0) >> 4;


	if (devid != CS42L73_DEVID) {
		ret = -ENODEV;
		dev_err(&i2c_client->dev,
			"CS42L73 Device ID (%X). Expected %X\n",
			devid, CS42L73_DEVID);
		goto err_regmap;
	}

	ret = regmap_read(cs42l73->regmap, CS42L73_REVID, &reg);
	if (ret < 0) {
		dev_err(&i2c_client->dev, "Get Revision ID failed\n");
		goto err_regmap;
	}

	dev_info(&i2c_client->dev,
		 "Cirrus Logic CS42L73, Revision: %02X\n", reg & 0xFF);

	regmap_update_bits(cs42l73->regmap, CS42L73_PWRCTL1, PDN, 0);
	regcache_cache_only(cs42l73->regmap, true);

	for (i = 0; i < ARRAY_SIZE(cs42l73_micbias_desc); i++) {
		cs42l73->micbias[i] = regulator_register(&cs42l73_micbias_desc[i],
				&i2c_client->dev, &cs42l73_micbias_data, cs42l73, NULL);
		if (IS_ERR(cs42l73->micbias[i])) {
			ret = PTR_ERR(cs42l73->micbias[i]);
			dev_err(&i2c_client->dev, "Failed to register %s: %d\n",
				cs42l73_micbias_desc[i].name, ret);
			goto err_regulator;
		}
	}

	ret =  snd_soc_register_codec(&i2c_client->dev,
			&soc_codec_dev_cs42l73, cs42l73_dai,
			ARRAY_SIZE(cs42l73_dai));
	if (ret < 0)
		goto err_regulator;
	return 0;

err_regulator:
	while (--i >= 0)
		regulator_unregister(cs42l73->micbias[i]);
err_regmap:
	regmap_exit(cs42l73->regmap);
err:
	if (pdata && pdata->teardown)
		pdata->teardown();
	return ret;
}

static __devexit int cs42l73_i2c_remove(struct i2c_client *client)
{
	int i;
	struct cs42l73_private *cs42l73 = i2c_get_clientdata(client);
	struct cs42l73_platform_data *pdata = client->dev.platform_data;

	snd_soc_unregister_codec(&client->dev);

	for (i = 0; i < ARRAY_SIZE(cs42l73_micbias_desc); i++)
		regulator_unregister(cs42l73->micbias[i]);

	regmap_exit(cs42l73->regmap);

	if (pdata && pdata->teardown)
		pdata->teardown();

	return 0;
}

#ifdef CONFIG_TEGRA_I2C_RECOVERY
static int cs42l73_i2c_reset(struct i2c_client *client)
{
/*	struct cs42l73_platform_data *pdata = client->dev.platform_data;
	dev_info(&client->dev, "%s(): teardown\n", __func__);
	if (pdata && pdata->teardown)
		pdata->teardown();
	dev_info(&client->dev, "%s(): setup\n", __func__);
	if (pdata && pdata->setup)
		return pdata->setup();
*/
	return 0;
}
#endif

static const struct i2c_device_id cs42l73_id[] = {
	{"cs42l73", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, cs42l73_id);

static struct i2c_driver cs42l73_i2c_driver = {
	.driver = {
		   .name = "cs42l73",
		   .owner = THIS_MODULE,
		   },
	.id_table = cs42l73_id,
	.probe = cs42l73_i2c_probe,
	.remove = __devexit_p(cs42l73_i2c_remove),
#ifdef CONFIG_TEGRA_I2C_RECOVERY
	.reset = cs42l73_i2c_reset,
#endif
};

static int __init cs42l73_modinit(void)
{
	int ret;
	ret = i2c_add_driver(&cs42l73_i2c_driver);
	if (ret != 0) {
		pr_err("Failed to register CS42L73 I2C driver: %d\n", ret);
		return ret;
	}
	return 0;
}

module_init(cs42l73_modinit);

static void __exit cs42l73_exit(void)
{
	i2c_del_driver(&cs42l73_i2c_driver);
}

module_exit(cs42l73_exit);

MODULE_DESCRIPTION("ASoC CS42L73 driver");
MODULE_AUTHOR("Georgi Vlaev, Nucleus Systems Ltd, <joe@nucleusys.com>");
MODULE_AUTHOR("Brian Austin, Cirrus Logic Inc, <brian.austin@cirrus.com>");
MODULE_LICENSE("GPL");
