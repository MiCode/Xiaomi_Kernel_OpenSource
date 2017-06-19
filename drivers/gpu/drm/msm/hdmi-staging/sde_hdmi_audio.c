/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include <linux/slab.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/iopoll.h>
#include <linux/types.h>
#include <linux/switch.h>
#include <linux/gcd.h>

#include "drm_edid.h"
#include "sde_kms.h"
#include "sde_hdmi.h"
#include "sde_hdmi_regs.h"
#include "hdmi.h"

#define HDMI_AUDIO_INFO_FRAME_PACKET_HEADER 0x84
#define HDMI_AUDIO_INFO_FRAME_PACKET_VERSION 0x1
#define HDMI_AUDIO_INFO_FRAME_PACKET_LENGTH 0x0A

#define HDMI_ACR_N_MULTIPLIER 128
#define DEFAULT_AUDIO_SAMPLE_RATE_HZ 48000

/* Supported HDMI Audio channels */
enum hdmi_audio_channels {
	AUDIO_CHANNEL_2 = 2,
	AUDIO_CHANNEL_3,
	AUDIO_CHANNEL_4,
	AUDIO_CHANNEL_5,
	AUDIO_CHANNEL_6,
	AUDIO_CHANNEL_7,
	AUDIO_CHANNEL_8,
};

/* parameters for clock regeneration */
struct hdmi_audio_acr {
	u32 n;
	u32 cts;
};

enum hdmi_audio_sample_rates {
	AUDIO_SAMPLE_RATE_32KHZ,
	AUDIO_SAMPLE_RATE_44_1KHZ,
	AUDIO_SAMPLE_RATE_48KHZ,
	AUDIO_SAMPLE_RATE_88_2KHZ,
	AUDIO_SAMPLE_RATE_96KHZ,
	AUDIO_SAMPLE_RATE_176_4KHZ,
	AUDIO_SAMPLE_RATE_192KHZ,
	AUDIO_SAMPLE_RATE_MAX
};

struct sde_hdmi_audio {
	struct hdmi *hdmi;
	struct msm_ext_disp_audio_setup_params params;
	u32 pclk;
};

static void _sde_hdmi_audio_get_audio_sample_rate(u32 *sample_rate_hz)
{
	u32 rate = *sample_rate_hz;

	switch (rate) {
	case 32000:
		*sample_rate_hz = AUDIO_SAMPLE_RATE_32KHZ;
		break;
	case 44100:
		*sample_rate_hz = AUDIO_SAMPLE_RATE_44_1KHZ;
		break;
	case 48000:
		*sample_rate_hz = AUDIO_SAMPLE_RATE_48KHZ;
		break;
	case 88200:
		*sample_rate_hz = AUDIO_SAMPLE_RATE_88_2KHZ;
		break;
	case 96000:
		*sample_rate_hz = AUDIO_SAMPLE_RATE_96KHZ;
		break;
	case 176400:
		*sample_rate_hz = AUDIO_SAMPLE_RATE_176_4KHZ;
		break;
	case 192000:
		*sample_rate_hz = AUDIO_SAMPLE_RATE_192KHZ;
		break;
	default:
		SDE_ERROR("%d unchanged\n", rate);
		break;
	}
}

static void _sde_hdmi_audio_get_acr_param(u32 pclk, u32 fs,
	struct hdmi_audio_acr *acr)
{
	u32 div, mul;

	if (!acr) {
		SDE_ERROR("invalid data\n");
		return;
	}

	/*
	 * as per HDMI specification, N/CTS = (128*fs)/pclk.
	 * get the ratio using this formula.
	 */
	acr->n = HDMI_ACR_N_MULTIPLIER * fs;
	acr->cts = pclk;

	/* get the greatest common divisor for the ratio */
	div = gcd(acr->n, acr->cts);

	/* get the n and cts values wrt N/CTS formula */
	acr->n /= div;
	acr->cts /= div;

	/*
	 * as per HDMI specification, 300 <= 128*fs/N <= 1500
	 * with a target of 128*fs/N = 1000. To get closest
	 * value without truncating fractional values, find
	 * the corresponding multiplier
	 */
	mul = ((HDMI_ACR_N_MULTIPLIER * fs / HDMI_KHZ_TO_HZ)
		+ (acr->n - 1)) / acr->n;

	acr->n *= mul;
	acr->cts *= mul;
}

static void _sde_hdmi_audio_acr_enable(struct sde_hdmi_audio *audio)
{
	struct hdmi_audio_acr acr;
	struct msm_ext_disp_audio_setup_params *params;
	u32 pclk, layout, multiplier = 1, sample_rate;
	u32 acr_pkt_ctl, aud_pkt_ctl2, acr_reg_cts, acr_reg_n;
	struct hdmi *hdmi;

	hdmi = audio->hdmi;
	params = &audio->params;
	pclk = audio->pclk;
	sample_rate = params->sample_rate_hz;

	_sde_hdmi_audio_get_acr_param(pclk, sample_rate, &acr);
	_sde_hdmi_audio_get_audio_sample_rate(&sample_rate);

	layout = (params->num_of_channels == AUDIO_CHANNEL_2) ? 0 : 1;

	SDE_DEBUG("n=%u, cts=%u, layout=%u\n", acr.n, acr.cts, layout);

	/* AUDIO_PRIORITY | SOURCE */
	acr_pkt_ctl = BIT(31) | BIT(8);

	switch (sample_rate) {
	case AUDIO_SAMPLE_RATE_44_1KHZ:
		acr_pkt_ctl |= 0x2 << 4;
		acr.cts <<= 12;

		acr_reg_cts = HDMI_ACR_44_0;
		acr_reg_n = HDMI_ACR_44_1;
		break;
	case AUDIO_SAMPLE_RATE_48KHZ:
		acr_pkt_ctl |= 0x3 << 4;
		acr.cts <<= 12;

		acr_reg_cts = HDMI_ACR_48_0;
		acr_reg_n = HDMI_ACR_48_1;
		break;
	case AUDIO_SAMPLE_RATE_192KHZ:
		multiplier = 4;
		acr.n >>= 2;

		acr_pkt_ctl |= 0x3 << 4;
		acr.cts <<= 12;

		acr_reg_cts = HDMI_ACR_48_0;
		acr_reg_n = HDMI_ACR_48_1;
		break;
	case AUDIO_SAMPLE_RATE_176_4KHZ:
		multiplier = 4;
		acr.n >>= 2;

		acr_pkt_ctl |= 0x2 << 4;
		acr.cts <<= 12;

		acr_reg_cts = HDMI_ACR_44_0;
		acr_reg_n = HDMI_ACR_44_1;
		break;
	case AUDIO_SAMPLE_RATE_96KHZ:
		multiplier = 2;
		acr.n >>= 1;

		acr_pkt_ctl |= 0x3 << 4;
		acr.cts <<= 12;

		acr_reg_cts = HDMI_ACR_48_0;
		acr_reg_n = HDMI_ACR_48_1;
		break;
	case AUDIO_SAMPLE_RATE_88_2KHZ:
		multiplier = 2;
		acr.n >>= 1;

		acr_pkt_ctl |= 0x2 << 4;
		acr.cts <<= 12;

		acr_reg_cts = HDMI_ACR_44_0;
		acr_reg_n = HDMI_ACR_44_1;
		break;
	default:
		multiplier = 1;

		acr_pkt_ctl |= 0x1 << 4;
		acr.cts <<= 12;

		acr_reg_cts = HDMI_ACR_32_0;
		acr_reg_n = HDMI_ACR_32_1;
		break;
	}

	aud_pkt_ctl2 = BIT(0) | (layout << 1);

	/* N_MULTIPLE(multiplier) */
	acr_pkt_ctl &= ~(7 << 16);
	acr_pkt_ctl |= (multiplier & 0x7) << 16;

	/* SEND | CONT */
	acr_pkt_ctl |= BIT(0) | BIT(1);

	hdmi_write(hdmi, acr_reg_cts, acr.cts);
	hdmi_write(hdmi, acr_reg_n, acr.n);
	hdmi_write(hdmi, HDMI_ACR_PKT_CTRL, acr_pkt_ctl);
	hdmi_write(hdmi, HDMI_AUDIO_PKT_CTRL2, aud_pkt_ctl2);
}

static void _sde_hdmi_audio_acr_setup(struct sde_hdmi_audio *audio, bool on)
{
	if (on)
		_sde_hdmi_audio_acr_enable(audio);
	else
		hdmi_write(audio->hdmi, HDMI_ACR_PKT_CTRL, 0);
}

static void _sde_hdmi_audio_infoframe_setup(struct sde_hdmi_audio *audio,
	bool enabled)
{
	struct hdmi *hdmi = audio->hdmi;
	u32 channels, channel_allocation, level_shift, down_mix, layout;
	u32 hdmi_debug_reg = 0, audio_info_0_reg = 0, audio_info_1_reg = 0;
	u32 audio_info_ctrl_reg, aud_pck_ctrl_2_reg;
	u32 check_sum, sample_present;

	audio_info_ctrl_reg = hdmi_read(hdmi, HDMI_INFOFRAME_CTRL0);
	audio_info_ctrl_reg &= ~0xF0;

	if (!enabled)
		goto end;

	channels           = audio->params.num_of_channels - 1;
	channel_allocation = audio->params.channel_allocation;
	level_shift        = audio->params.level_shift;
	down_mix           = audio->params.down_mix;
	sample_present     = audio->params.sample_present;

	layout = (audio->params.num_of_channels == AUDIO_CHANNEL_2) ? 0 : 1;
	aud_pck_ctrl_2_reg = BIT(0) | (layout << 1);
	hdmi_write(hdmi, HDMI_AUDIO_PKT_CTRL2, aud_pck_ctrl_2_reg);

	audio_info_1_reg |= channel_allocation & 0xFF;
	audio_info_1_reg |= ((level_shift & 0xF) << 11);
	audio_info_1_reg |= ((down_mix & 0x1) << 15);

	check_sum = 0;
	check_sum += HDMI_AUDIO_INFO_FRAME_PACKET_HEADER;
	check_sum += HDMI_AUDIO_INFO_FRAME_PACKET_VERSION;
	check_sum += HDMI_AUDIO_INFO_FRAME_PACKET_LENGTH;
	check_sum += channels;
	check_sum += channel_allocation;
	check_sum += (level_shift & 0xF) << 3 | (down_mix & 0x1) << 7;
	check_sum &= 0xFF;
	check_sum = (u8) (256 - check_sum);

	audio_info_0_reg |= check_sum & 0xFF;
	audio_info_0_reg |= ((channels & 0x7) << 8);

	/* Enable Audio InfoFrame Transmission */
	audio_info_ctrl_reg |= 0xF0;

	if (layout) {
		/* Set the Layout bit */
		hdmi_debug_reg |= BIT(4);

		/* Set the Sample Present bits */
		hdmi_debug_reg |= sample_present & 0xF;
	}
end:
	hdmi_write(hdmi, HDMI_DEBUG, hdmi_debug_reg);
	hdmi_write(hdmi, HDMI_AUDIO_INFO0, audio_info_0_reg);
	hdmi_write(hdmi, HDMI_AUDIO_INFO1, audio_info_1_reg);
	hdmi_write(hdmi, HDMI_INFOFRAME_CTRL0, audio_info_ctrl_reg);
}

int sde_hdmi_audio_on(struct hdmi *hdmi,
	struct msm_ext_disp_audio_setup_params *params)
{
	struct sde_hdmi_audio audio;
	int rc = 0;

	if (!hdmi) {
		SDE_ERROR("invalid HDMI Ctrl\n");
		rc = -ENODEV;
		goto end;
	}

	audio.pclk = hdmi->pixclock;
	audio.params = *params;
	audio.hdmi = hdmi;

	if (!audio.params.num_of_channels) {
		audio.params.sample_rate_hz = DEFAULT_AUDIO_SAMPLE_RATE_HZ;
		audio.params.num_of_channels = AUDIO_CHANNEL_2;
	}

	_sde_hdmi_audio_acr_setup(&audio, true);
	_sde_hdmi_audio_infoframe_setup(&audio, true);

	SDE_DEBUG("HDMI Audio: Enabled\n");
end:
	return rc;
}

void sde_hdmi_audio_off(struct hdmi *hdmi)
{
	struct sde_hdmi_audio audio;
	int rc = 0;

	if (!hdmi) {
		SDE_ERROR("invalid HDMI Ctrl\n");
		rc = -ENODEV;
		return;
	}

	audio.hdmi = hdmi;

	_sde_hdmi_audio_infoframe_setup(&audio, false);
	_sde_hdmi_audio_acr_setup(&audio, false);

	SDE_DEBUG("HDMI Audio: Disabled\n");
}

