/* Copyright (c) 2016, 2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/slab.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/iopoll.h>
#include <linux/types.h>
#include <linux/extcon.h>
#include <linux/gcd.h>

#include "mdss_hdmi_audio.h"
#include "mdss_hdmi_util.h"

#define HDMI_AUDIO_INFO_FRAME_PACKET_HEADER 0x84
#define HDMI_AUDIO_INFO_FRAME_PACKET_VERSION 0x1
#define HDMI_AUDIO_INFO_FRAME_PACKET_LENGTH 0x0A

#define HDMI_KHZ_TO_HZ 1000
#define HDMI_MHZ_TO_HZ 1000000
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

struct hdmi_audio {
	struct dss_io_data *io;
	struct msm_hdmi_audio_setup_params params;
	struct extcon_dev sdev;
	u32 pclk;
	bool ack_enabled;
	bool audio_ack_enabled;
	atomic_t ack_pending;
};

static void hdmi_audio_get_audio_sample_rate(u32 *sample_rate_hz)
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
		pr_debug("%d unchanged\n", rate);
		break;
	}
}

static void hdmi_audio_get_acr_param(u32 pclk, u32 fs,
	struct hdmi_audio_acr *acr)
{
	u32 div, mul;

	if (!acr) {
		pr_err("invalid data\n");
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

static void hdmi_audio_acr_enable(struct hdmi_audio *audio)
{
	struct dss_io_data *io;
	struct hdmi_audio_acr acr;
	struct msm_hdmi_audio_setup_params *params;
	u32 pclk, layout, multiplier = 1, sample_rate;
	u32 acr_pkt_ctl, aud_pkt_ctl2, acr_reg_cts, acr_reg_n;

	if (!audio) {
		pr_err("invalid input\n");
		return;
	}

	io = audio->io;
	params = &audio->params;
	pclk = audio->pclk;
	sample_rate = params->sample_rate_hz;

	hdmi_audio_get_acr_param(pclk * HDMI_KHZ_TO_HZ, sample_rate, &acr);
	hdmi_audio_get_audio_sample_rate(&sample_rate);

	layout = params->num_of_channels == AUDIO_CHANNEL_2 ? 0 : 1;

	pr_debug("n=%u, cts=%u, layout=%u\n", acr.n, acr.cts, layout);

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

	DSS_REG_W(io, acr_reg_cts, acr.cts);
	DSS_REG_W(io, acr_reg_n, acr.n);
	DSS_REG_W(io, HDMI_ACR_PKT_CTRL, acr_pkt_ctl);
	DSS_REG_W(io, HDMI_AUDIO_PKT_CTRL2, aud_pkt_ctl2);
}

static void hdmi_audio_acr_setup(struct hdmi_audio *audio, bool on)
{
	if (on)
		hdmi_audio_acr_enable(audio);
	else
		DSS_REG_W(audio->io, HDMI_ACR_PKT_CTRL, 0);
}

static void hdmi_audio_infoframe_setup(struct hdmi_audio *audio, bool enabled)
{
	struct dss_io_data *io = NULL;
	u32 channels, channel_allocation, level_shift, down_mix, layout;
	u32 hdmi_debug_reg = 0, audio_info_0_reg = 0, audio_info_1_reg = 0;
	u32 audio_info_ctrl_reg, aud_pck_ctrl_2_reg;
	u32 check_sum, sample_present;

	if (!audio) {
		pr_err("invalid input\n");
		return;
	}

	io = audio->io;
	if (!io->base) {
		pr_err("core io not inititalized\n");
		return;
	}

	audio_info_ctrl_reg = DSS_REG_R(io, HDMI_INFOFRAME_CTRL0);
	audio_info_ctrl_reg &= ~0xF0;

	if (!enabled)
		goto end;

	channels           = audio->params.num_of_channels - 1;
	channel_allocation = audio->params.channel_allocation;
	level_shift        = audio->params.level_shift;
	down_mix           = audio->params.down_mix;
	sample_present     = audio->params.sample_present;

	layout = audio->params.num_of_channels == AUDIO_CHANNEL_2 ? 0 : 1;
	aud_pck_ctrl_2_reg = BIT(0) | (layout << 1);
	DSS_REG_W(io, HDMI_AUDIO_PKT_CTRL2, aud_pck_ctrl_2_reg);

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
	DSS_REG_W(io, HDMI_DEBUG, hdmi_debug_reg);
	DSS_REG_W(io, HDMI_AUDIO_INFO0, audio_info_0_reg);
	DSS_REG_W(io, HDMI_AUDIO_INFO1, audio_info_1_reg);
	DSS_REG_W(io, HDMI_INFOFRAME_CTRL0, audio_info_ctrl_reg);
}

static int hdmi_audio_on(void *ctx, u32 pclk,
	struct msm_hdmi_audio_setup_params *params)
{
	struct hdmi_audio *audio = ctx;
	int rc = 0;

	if (!audio) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	audio->pclk = pclk;
	audio->params = *params;

	if (!audio->params.num_of_channels) {
		audio->params.sample_rate_hz = DEFAULT_AUDIO_SAMPLE_RATE_HZ;
		audio->params.num_of_channels = AUDIO_CHANNEL_2;
	}

	hdmi_audio_acr_setup(audio, true);
	hdmi_audio_infoframe_setup(audio, true);

	pr_debug("HDMI Audio: Enabled\n");
end:
	return rc;
}

static void hdmi_audio_off(void *ctx)
{
	struct hdmi_audio *audio = ctx;

	if (!audio) {
		pr_err("invalid input\n");
		return;
	}

	hdmi_audio_infoframe_setup(audio, false);
	hdmi_audio_acr_setup(audio, false);

	pr_debug("HDMI Audio: Disabled\n");
}

static void hdmi_audio_notify(void *ctx, int val)
{
	struct hdmi_audio *audio = ctx;
	int state = 0;
	bool switched;

	if (!audio) {
		pr_err("invalid input\n");
		return;
	}

	state = audio->sdev.state;
	if (state == val)
		return;

	if (audio->ack_enabled &&
		atomic_read(&audio->ack_pending)) {
		pr_err("%s ack pending, not notifying %s\n",
			state ? "connect" : "disconnect",
			val ? "connect" : "disconnect");
		return;
	}

	extcon_set_state_sync(&audio->sdev, 0, val);
	switched = audio->sdev.state != state;

	if (audio->ack_enabled && switched)
		atomic_set(&audio->ack_pending, 1);

	pr_debug("audio %s %s\n", switched ? "switched to" : "same as",
		audio->sdev.state ? "HDMI" : "SPKR");
}

static void hdmi_audio_ack(void *ctx, u32 ack, u32 hpd)
{
	struct hdmi_audio *audio = ctx;
	u32 ack_hpd;

	if (!audio) {
		pr_err("invalid input\n");
		return;
	}

	if (ack & AUDIO_ACK_SET_ENABLE) {
		audio->ack_enabled = ack & AUDIO_ACK_ENABLE ?
			true : false;

		pr_debug("audio ack feature %s\n",
			audio->ack_enabled ? "enabled" : "disabled");
		return;
	}

	if (!audio->ack_enabled)
		return;

	atomic_set(&audio->ack_pending, 0);

	ack_hpd = ack & AUDIO_ACK_CONNECT;

	pr_debug("acknowledging %s\n",
		ack_hpd ? "connect" : "disconnect");

	if (ack_hpd != hpd) {
		pr_debug("unbalanced audio state, ack %d, hpd %d\n",
			ack_hpd, hpd);

		hdmi_audio_notify(ctx, hpd);
	}
}

static void hdmi_audio_reset(void *ctx)
{
	struct hdmi_audio *audio = ctx;

	if (!audio) {
		pr_err("invalid input\n");
		return;
	}

	atomic_set(&audio->ack_pending, 0);
}

static void hdmi_audio_status(void *ctx, struct hdmi_audio_status *status)
{
	struct hdmi_audio *audio = ctx;

	if (!audio || !status) {
		pr_err("invalid input\n");
		return;
	}

	status->ack_enabled = audio->ack_enabled;
	status->ack_pending = atomic_read(&audio->ack_pending);
	status->switched = audio->sdev.state;
}

/**
 * hdmi_audio_register() - audio registeration function
 * @data: registeration initialization data
 *
 * This API configures audio module for client to use HDMI audio.
 * Provides audio functionalities which client can call.
 * Initializes internal data structures.
 *
 * Return: pointer to audio data that client needs to pass on
 * calling audio functions.
 */
void *hdmi_audio_register(struct hdmi_audio_init_data *data)
{
	struct hdmi_audio *audio = NULL;
	int rc = 0;

	if (!data)
		goto end;

	audio = kzalloc(sizeof(*audio), GFP_KERNEL);
	if (!audio)
		goto end;

	audio->sdev.name = "hdmi_audio";
	rc = extcon_dev_register(&audio->sdev);
	if (rc) {
		pr_err("audio switch registration failed\n");
		kzfree(audio);
		goto end;
	}

	audio->io = data->io;

	data->ops->on     = hdmi_audio_on;
	data->ops->off    = hdmi_audio_off;
	data->ops->notify = hdmi_audio_notify;
	data->ops->ack    = hdmi_audio_ack;
	data->ops->reset  = hdmi_audio_reset;
	data->ops->status = hdmi_audio_status;
end:
	return audio;
}

/**
 * hdmi_audio_unregister() - unregister audio module
 * @ctx: audio module's data
 *
 * Delete audio module's instance and allocated resources
 */
void hdmi_audio_unregister(void *ctx)
{
	struct hdmi_audio *audio = ctx;

	if (audio) {
		extcon_dev_unregister(&audio->sdev);
		kfree(ctx);
	}
}
