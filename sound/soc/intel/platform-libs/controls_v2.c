
/*
 *  controls_v2.c - Intel MID Platform driver ALSA controls for Mrfld
 *
 *  Copyright (C) 2012 Intel Corp
 *  Author: Vinod Koul <vinod.koul@ilinux.intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <sound/soc.h>
#include <sound/asound.h>
#include <asm/platform_sst_audio.h>
#include "../platform_ipc_v2.h"
#include "../sst_platform.h"
#include "../sst_platform_pvt.h"
#include "ipc_lib.h"
#include "controls_v2.h"


#define SST_ALGO_KCONTROL_INT(xname, xreg, xshift, xmax, xinvert,\
	xhandler_get, xhandler_put, xmod, xpipe, xinstance, default_val) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = sst_algo_int_ctl_info, \
	.get = xhandler_get, .put = xhandler_put, \
	.private_value = (unsigned long)&(struct sst_algo_int_control_v2) \
		{.mc.reg = xreg, .mc.rreg = xreg, .mc.shift = xshift, \
		.mc.rshift = xshift, .mc.max = xmax, .mc.platform_max = xmax, \
		.mc.invert = xinvert, .module_id = xmod, .pipe_id = xpipe, \
		.instance_id = xinstance, .value = default_val } }
/* Thresholds for Low Latency & Deep Buffer*/
#define DEFAULT_LOW_LATENCY 10 /* In Ms */
#define DEFAULT_DEEP_BUFFER 96

unsigned long ll_threshold = DEFAULT_LOW_LATENCY;
unsigned long db_threshold = DEFAULT_DEEP_BUFFER;

int sst_algo_int_ctl_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	struct sst_algo_int_control_v2 *amc = (void *)kcontrol->private_value;
	struct soc_mixer_control *mc = &amc->mc;
	int platform_max;

	if (!mc->platform_max)
		mc->platform_max = mc->max;
	platform_max = mc->platform_max;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = platform_max;
	return 0;
}

unsigned int sst_soc_read(struct snd_soc_platform *platform,
			unsigned int reg)
{
	struct sst_data *sst = snd_soc_platform_get_drvdata(platform);

	pr_debug("%s: reg[%d] = %#x\n", __func__, reg, sst->widget[reg]);
	BUG_ON(reg > (SST_NUM_WIDGETS - 1));
	return sst->widget[reg];
}

int sst_soc_write(struct snd_soc_platform *platform,
		  unsigned int reg, unsigned int val)
{
	struct sst_data *sst = snd_soc_platform_get_drvdata(platform);

	pr_debug("%s: reg[%d] = %#x\n", __func__, reg, val);
	BUG_ON(reg > (SST_NUM_WIDGETS - 1));
	sst->widget[reg] = val;
	return 0;
}

unsigned int sst_reg_read(struct sst_data *sst, unsigned int reg,
			  unsigned int shift, unsigned int max)
{
	unsigned int mask = (1 << fls(max)) - 1;

	return (sst->widget[reg] >> shift) & mask;
}

unsigned int sst_reg_write(struct sst_data *sst, unsigned int reg,
			   unsigned int shift, unsigned int max, unsigned int val)
{
	unsigned int mask = (1 << fls(max)) - 1;

	val &= mask;
	val <<= shift;
	sst->widget[reg] &= ~(mask << shift);
	sst->widget[reg] |= val;
	return val;
}

int sst_mix_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct sst_data *sst = snd_soc_platform_get_drvdata(widget->platform);
	unsigned int mask = (1 << fls(mc->max)) - 1;
	unsigned int val;
	int connect;
	struct snd_soc_dapm_update update;

	pr_debug("%s called set %#lx for %s\n", __func__,
			ucontrol->value.integer.value[0], widget->name);
	val = sst_reg_write(sst, mc->reg, mc->shift, mc->max, ucontrol->value.integer.value[0]);
	connect = !!val;

	dapm_kcontrol_set_value(kcontrol, val);
	update.kcontrol = kcontrol;
	update.reg = mc->reg;
	update.mask = mask;
	update.val = val;

	snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, connect, &update);
	return 0;
}

int sst_mix_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *w = wlist->widgets[0];
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct sst_data *sst = snd_soc_platform_get_drvdata(w->platform);

	ucontrol->value.integer.value[0] = !!sst_reg_read(sst, mc->reg, mc->shift, mc->max);
	return 0;
}

static const struct snd_kcontrol_new sst_mix_modem_controls[] = {
	SOC_SINGLE_EXT("Modem", SST_MIX_MODEM, 0, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("BT", SST_MIX_MODEM, 1, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec0", SST_MIX_MODEM, 2, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec1", SST_MIX_MODEM, 3, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sprot_L0", SST_MIX_MODEM, 4, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L1", SST_MIX_MODEM, 5, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L2", SST_MIX_MODEM, 6, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Probe", SST_MIX_MODEM, 7, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sidetone", SST_MIX_MODEM, 8, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Tx", SST_MIX_MODEM, 9, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Rx", SST_MIX_MODEM, 10, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Tone", SST_MIX_MODEM, 11, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Voip", SST_MIX_MODEM, 12, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM0", SST_MIX_MODEM, 13, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM1", SST_MIX_MODEM, 14, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media0", SST_MIX_MODEM, 15, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media1", SST_MIX_MODEM, 16, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media2", SST_MIX_MODEM, 17, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("FM", SST_MIX_MODEM, 18, 1, 0,
		sst_mix_get, sst_mix_put),
};

static const struct snd_kcontrol_new sst_mix_codec0_controls[] = {
	SOC_SINGLE_EXT("Modem", SST_MIX_CODEC0, 0, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("BT", SST_MIX_CODEC0, 1, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec0", SST_MIX_CODEC0, 2, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec1", SST_MIX_CODEC0, 3, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sprot_L0", SST_MIX_CODEC0, 4, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L1", SST_MIX_CODEC0, 5, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L2", SST_MIX_CODEC0, 6, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Probe", SST_MIX_CODEC0, 7, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sidetone", SST_MIX_CODEC0, 8, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Tx", SST_MIX_CODEC0, 9, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Rx", SST_MIX_CODEC0, 10, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Tone", SST_MIX_CODEC0, 11, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Voip", SST_MIX_CODEC0, 12, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM0", SST_MIX_CODEC0, 13, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM1", SST_MIX_CODEC0, 14, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media0", SST_MIX_CODEC0, 15, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media1", SST_MIX_CODEC0, 16, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media2", SST_MIX_CODEC0, 17, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("FM", SST_MIX_CODEC0, 18, 1, 0,
		sst_mix_get, sst_mix_put),
};

static const struct snd_kcontrol_new sst_mix_codec1_controls[] = {
	SOC_SINGLE_EXT("Modem", SST_MIX_CODEC1, 0, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("BT", SST_MIX_CODEC1, 1, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec0", SST_MIX_CODEC1, 2, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec1", SST_MIX_CODEC1, 3, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sprot_L0", SST_MIX_CODEC1, 4, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L1", SST_MIX_CODEC1, 5, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L2", SST_MIX_CODEC1, 6, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Probe", SST_MIX_CODEC1, 7, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sidetone", SST_MIX_CODEC1, 8, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Tx", SST_MIX_CODEC1, 9, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Rx", SST_MIX_CODEC1, 10, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Tone", SST_MIX_CODEC1, 11, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Voip", SST_MIX_CODEC1, 12, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM0", SST_MIX_CODEC1, 13, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM1", SST_MIX_CODEC1, 14, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media0", SST_MIX_CODEC1, 15, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media1", SST_MIX_CODEC1, 16, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media2", SST_MIX_CODEC1, 17, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("FM", SST_MIX_CODEC1, 18, 1, 0,
		sst_mix_get, sst_mix_put),
};

static const struct snd_kcontrol_new sst_mix_sprot_l0_controls[] = {
	SOC_SINGLE_EXT("Modem", SST_MIX_LOOP0, 0, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("BT", SST_MIX_LOOP0, 1, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec0", SST_MIX_LOOP0, 2, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec1", SST_MIX_LOOP0, 3, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sprot_L0", SST_MIX_LOOP0, 4, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L1", SST_MIX_LOOP0, 5, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L2", SST_MIX_LOOP0, 6, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Probe", SST_MIX_LOOP0, 7, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sidetone", SST_MIX_LOOP0, 8, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Tx", SST_MIX_LOOP0, 9, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Rx", SST_MIX_LOOP0, 10, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Tone", SST_MIX_LOOP0, 11, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Voip", SST_MIX_LOOP0, 12, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM0", SST_MIX_LOOP0, 13, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM1", SST_MIX_LOOP0, 14, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media0", SST_MIX_LOOP0, 15, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media1", SST_MIX_LOOP0, 16, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media2", SST_MIX_LOOP0, 17, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("FM", SST_MIX_LOOP0, 18, 1, 0,
		sst_mix_get, sst_mix_put),
};

static const struct snd_kcontrol_new sst_mix_media_l1_controls[] = {
	SOC_SINGLE_EXT("Modem", SST_MIX_LOOP1, 0, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("BT", SST_MIX_LOOP1, 1, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec0", SST_MIX_LOOP1, 2, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec1", SST_MIX_LOOP1, 3, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sprot_L0", SST_MIX_LOOP1, 4, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L1", SST_MIX_LOOP1, 5, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L2", SST_MIX_LOOP1, 6, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Probe", SST_MIX_LOOP1, 7, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sidetone", SST_MIX_LOOP1, 8, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Tx", SST_MIX_LOOP1, 9, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Rx", SST_MIX_LOOP1, 10, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Tone", SST_MIX_LOOP1, 11, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Voip", SST_MIX_LOOP1, 12, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM0", SST_MIX_LOOP1, 13, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM1", SST_MIX_LOOP1, 14, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media0", SST_MIX_LOOP1, 15, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media1", SST_MIX_LOOP1, 16, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media2", SST_MIX_LOOP1, 17, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("FM", SST_MIX_LOOP1, 18, 1, 0,
		sst_mix_get, sst_mix_put),
};

static const struct snd_kcontrol_new sst_mix_media_l2_controls[] = {
	SOC_SINGLE_EXT("Modem", SST_MIX_LOOP2, 0, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("BT", SST_MIX_LOOP2, 1, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec0", SST_MIX_LOOP2, 2, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec1", SST_MIX_LOOP2, 3, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sprot_L0", SST_MIX_LOOP2, 4, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L1", SST_MIX_LOOP2, 5, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L2", SST_MIX_LOOP2, 6, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Probe", SST_MIX_LOOP2, 7, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sidetone", SST_MIX_LOOP2, 8, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Tx", SST_MIX_LOOP2, 9, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Rx", SST_MIX_LOOP2, 10, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Tone", SST_MIX_LOOP2, 11, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Voip", SST_MIX_LOOP2, 12, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM0", SST_MIX_LOOP2, 13, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM1", SST_MIX_LOOP2, 14, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media0", SST_MIX_LOOP2, 15, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media1", SST_MIX_LOOP2, 16, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media2", SST_MIX_LOOP2, 17, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("FM", SST_MIX_LOOP2, 18, 1, 0,
		sst_mix_get, sst_mix_put),
};

static const struct snd_kcontrol_new sst_mix_speech_tx_controls[] = {
	SOC_SINGLE_EXT("Modem", SST_MIX_SPEECH, 0, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("BT", SST_MIX_SPEECH, 1, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec0", SST_MIX_SPEECH, 2, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec1", SST_MIX_SPEECH, 3, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sprot_L0", SST_MIX_SPEECH, 4, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L1", SST_MIX_SPEECH, 5, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L2", SST_MIX_SPEECH, 6, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Probe", SST_MIX_SPEECH, 7, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sidetone", SST_MIX_SPEECH, 8, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Tx", SST_MIX_SPEECH, 9, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Rx", SST_MIX_SPEECH, 10, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Tone", SST_MIX_SPEECH, 11, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Voip", SST_MIX_SPEECH, 12, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM0", SST_MIX_SPEECH, 13, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM1", SST_MIX_SPEECH, 14, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media0", SST_MIX_SPEECH, 15, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media1", SST_MIX_SPEECH, 16, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media2", SST_MIX_SPEECH, 17, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("FM", SST_MIX_SPEECH, 18, 1, 0,
		sst_mix_get, sst_mix_put),
};

static const struct snd_kcontrol_new sst_mix_speech_rx_controls[] = {
	SOC_SINGLE_EXT("Modem", SST_MIX_RXSPEECH, 0, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("BT", SST_MIX_RXSPEECH, 1, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec0", SST_MIX_RXSPEECH, 2, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec1", SST_MIX_RXSPEECH, 3, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sprot_L0", SST_MIX_RXSPEECH, 4, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L1", SST_MIX_RXSPEECH, 5, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L2", SST_MIX_RXSPEECH, 6, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Probe", SST_MIX_RXSPEECH, 7, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sidetone", SST_MIX_RXSPEECH, 8, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Tx", SST_MIX_RXSPEECH, 9, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Rx", SST_MIX_RXSPEECH, 10, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Tone", SST_MIX_RXSPEECH, 11, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Voip", SST_MIX_RXSPEECH, 12, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM0", SST_MIX_RXSPEECH, 13, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM1", SST_MIX_RXSPEECH, 14, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media0", SST_MIX_RXSPEECH, 15, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media1", SST_MIX_RXSPEECH, 16, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media2", SST_MIX_RXSPEECH, 17, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("FM", SST_MIX_RXSPEECH, 18, 1, 0,
		sst_mix_get, sst_mix_put),
};

static const struct snd_kcontrol_new sst_mix_voip_controls[] = {
	SOC_SINGLE_EXT("Modem", SST_MIX_VOIP, 0, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("BT", SST_MIX_VOIP, 1, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec0", SST_MIX_VOIP, 2, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec1", SST_MIX_VOIP, 3, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sprot_L0", SST_MIX_VOIP, 4, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L1", SST_MIX_VOIP, 5, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L2", SST_MIX_VOIP, 6, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Probe", SST_MIX_VOIP, 7, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sidetone", SST_MIX_VOIP, 8, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Tx", SST_MIX_VOIP, 9, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Rx", SST_MIX_VOIP, 10, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Tone", SST_MIX_VOIP, 11, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Voip", SST_MIX_VOIP, 12, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM0", SST_MIX_VOIP, 13, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM1", SST_MIX_VOIP, 14, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media0", SST_MIX_VOIP, 15, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media1", SST_MIX_VOIP, 16, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media2", SST_MIX_VOIP, 17, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("FM", SST_MIX_VOIP, 18, 1, 0,
		sst_mix_get, sst_mix_put),
};

static const struct snd_kcontrol_new sst_mix_pcm0_controls[] = {
	SOC_SINGLE_EXT("Modem", SST_MIX_PCM0, 0, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("BT", SST_MIX_PCM0, 1, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec0", SST_MIX_PCM0, 2, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec1", SST_MIX_PCM0, 3, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sprot_L0", SST_MIX_PCM0, 4, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L1", SST_MIX_PCM0, 5, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L2", SST_MIX_PCM0, 6, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Probe", SST_MIX_PCM0, 7, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sidetone", SST_MIX_PCM0, 8, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Tx", SST_MIX_PCM0, 9, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Rx", SST_MIX_PCM0, 10, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Tone", SST_MIX_PCM0, 11, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Voip", SST_MIX_PCM0, 12, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM0", SST_MIX_PCM0, 13, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM1", SST_MIX_PCM0, 14, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media0", SST_MIX_PCM0, 15, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media1", SST_MIX_PCM0, 16, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media2", SST_MIX_PCM0, 17, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("FM", SST_MIX_PCM0, 18, 1, 0,
		sst_mix_get, sst_mix_put),
};

static const struct snd_kcontrol_new sst_mix_pcm1_controls[] = {
	SOC_SINGLE_EXT("Modem", SST_MIX_PCM1, 0, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("BT", SST_MIX_PCM1, 1, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec0", SST_MIX_PCM1, 2, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec1", SST_MIX_PCM1, 3, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sprot_L0", SST_MIX_PCM1, 4, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L1", SST_MIX_PCM1, 5, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L2", SST_MIX_PCM1, 6, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Probe", SST_MIX_PCM1, 7, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sidetone", SST_MIX_PCM1, 8, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Tx", SST_MIX_PCM1, 9, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Rx", SST_MIX_PCM1, 10, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Tone", SST_MIX_PCM1, 11, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Voip", SST_MIX_PCM1, 12, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM0", SST_MIX_PCM1, 13, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM1", SST_MIX_PCM1, 14, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media0", SST_MIX_PCM1, 15, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media1", SST_MIX_PCM1, 16, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media2", SST_MIX_PCM1, 17, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("FM", SST_MIX_PCM1, 18, 1, 0,
		sst_mix_get, sst_mix_put),
};

static const struct snd_kcontrol_new sst_mix_pcm2_controls[] = {
	SOC_SINGLE_EXT("Modem", SST_MIX_PCM2, 0, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("BT", SST_MIX_PCM2, 1, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec0", SST_MIX_PCM2, 2, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec1", SST_MIX_PCM2, 3, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sprot_L0", SST_MIX_PCM2, 4, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L1", SST_MIX_PCM2, 5, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L2", SST_MIX_PCM2, 6, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Probe", SST_MIX_PCM2, 7, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sidetone", SST_MIX_PCM2, 8, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Tx", SST_MIX_PCM2, 9, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Rx", SST_MIX_PCM2, 10, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Tone", SST_MIX_PCM2, 11, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Voip", SST_MIX_PCM2, 12, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM0", SST_MIX_PCM2, 13, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM1", SST_MIX_PCM2, 14, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media0", SST_MIX_PCM2, 15, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media1", SST_MIX_PCM2, 16, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media2", SST_MIX_PCM2, 17, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("FM", SST_MIX_PCM2, 18, 1, 0,
		sst_mix_get, sst_mix_put),
};

static const struct snd_kcontrol_new sst_mix_aware_controls[] = {
	SOC_SINGLE_EXT("Modem", SST_MIX_AWARE, 0, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("BT", SST_MIX_AWARE, 1, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec0", SST_MIX_AWARE, 2, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec1", SST_MIX_AWARE, 3, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sprot_L0", SST_MIX_AWARE, 4, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L1", SST_MIX_AWARE, 5, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L2", SST_MIX_AWARE, 6, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Probe", SST_MIX_AWARE, 7, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sidetone", SST_MIX_AWARE, 8, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Tx", SST_MIX_AWARE, 9, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Rx", SST_MIX_AWARE, 10, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Tone", SST_MIX_AWARE, 11, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Voip", SST_MIX_AWARE, 12, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM0", SST_MIX_AWARE, 13, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM1", SST_MIX_AWARE, 14, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media0", SST_MIX_AWARE, 15, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media1", SST_MIX_AWARE, 16, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media2", SST_MIX_AWARE, 17, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("FM", SST_MIX_AWARE, 18, 1, 0,
		sst_mix_get, sst_mix_put),
};

static const struct snd_kcontrol_new sst_mix_vad_controls[] = {
	SOC_SINGLE_EXT("Modem", SST_MIX_VAD, 0, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("BT", SST_MIX_VAD, 1, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec0", SST_MIX_VAD, 2, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec1", SST_MIX_VAD, 3, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sprot_L0", SST_MIX_VAD, 4, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L1", SST_MIX_VAD, 5, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L2", SST_MIX_VAD, 6, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Probe", SST_MIX_VAD, 7, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sidetone", SST_MIX_VAD, 8, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Tx", SST_MIX_VAD, 9, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Rx", SST_MIX_VAD, 10, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Tone", SST_MIX_VAD, 11, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Voip", SST_MIX_VAD, 12, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM0", SST_MIX_VAD, 13, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM1", SST_MIX_VAD, 14, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media0", SST_MIX_VAD, 15, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media1", SST_MIX_VAD, 16, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media2", SST_MIX_VAD, 17, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("FM", SST_MIX_VAD, 18, 1, 0,
		sst_mix_get, sst_mix_put),
};

static const struct snd_kcontrol_new sst_mix_media0_controls[] = {
	SOC_SINGLE_EXT("Modem", SST_MIX_MEDIA0, 0, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("BT", SST_MIX_MEDIA0, 1, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec0", SST_MIX_MEDIA0, 2, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec1", SST_MIX_MEDIA0, 3, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sprot_L0", SST_MIX_MEDIA0, 4, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L1", SST_MIX_MEDIA0, 5, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L2", SST_MIX_MEDIA0, 6, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Probe", SST_MIX_MEDIA0, 7, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sidetone", SST_MIX_MEDIA0, 8, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Tx", SST_MIX_MEDIA0, 9, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Rx", SST_MIX_MEDIA0, 10, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Tone", SST_MIX_MEDIA0, 11, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Voip", SST_MIX_MEDIA0, 12, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM0", SST_MIX_MEDIA0, 13, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM1", SST_MIX_MEDIA0, 14, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media0", SST_MIX_MEDIA0, 15, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media1", SST_MIX_MEDIA0, 16, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media2", SST_MIX_MEDIA0, 17, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("FM", SST_MIX_MEDIA0, 18, 1, 0,
		sst_mix_get, sst_mix_put),
};

static const struct snd_kcontrol_new sst_mix_media1_controls[] = {
	SOC_SINGLE_EXT("Modem", SST_MIX_MEDIA1, 0, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("BT", SST_MIX_MEDIA1, 1, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec0", SST_MIX_MEDIA1, 2, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec1", SST_MIX_MEDIA1, 3, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sprot_L0", SST_MIX_MEDIA1, 4, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L1", SST_MIX_MEDIA1, 5, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L2", SST_MIX_MEDIA1, 6, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Probe", SST_MIX_MEDIA1, 7, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sidetone", SST_MIX_MEDIA1, 8, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Tx", SST_MIX_MEDIA1, 9, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Rx", SST_MIX_MEDIA1, 10, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Tone", SST_MIX_MEDIA1, 11, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Voip", SST_MIX_MEDIA1, 12, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM0", SST_MIX_MEDIA1, 13, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM1", SST_MIX_MEDIA1, 14, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media0", SST_MIX_MEDIA1, 15, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media1", SST_MIX_MEDIA1, 16, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media2", SST_MIX_MEDIA1, 17, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("FM", SST_MIX_MEDIA1, 18, 1, 0,
		sst_mix_get, sst_mix_put),
};

static const struct snd_kcontrol_new sst_mix_fm_controls[] = {
	SOC_SINGLE_EXT("Modem", SST_MIX_FM, 0, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("BT", SST_MIX_FM, 1, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec0", SST_MIX_FM, 2, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Codec1", SST_MIX_FM, 3, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sprot_L0", SST_MIX_FM, 4, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L1", SST_MIX_FM, 5, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media_L2", SST_MIX_FM, 6, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Probe", SST_MIX_FM, 7, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Sidetone", SST_MIX_FM, 8, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Tx", SST_MIX_FM, 9, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Speech_Rx", SST_MIX_FM, 10, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Tone", SST_MIX_FM, 11, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Voip", SST_MIX_FM, 12, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM0", SST_MIX_FM, 13, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("PCM1", SST_MIX_FM, 14, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media0", SST_MIX_FM, 15, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media1", SST_MIX_FM, 16, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("Media2", SST_MIX_FM, 17, 1, 0,
		sst_mix_get, sst_mix_put),
	SOC_SINGLE_EXT("FM", SST_MIX_FM, 18, 1, 0,
		sst_mix_get, sst_mix_put),
};

static const struct snd_kcontrol_new sst_mix_sw_modem =
	SOC_SINGLE_EXT("Switch", SST_MIX_SWITCH, 0, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_mix_sw_codec0 =
	SOC_SINGLE_EXT("Switch", SST_MIX_SWITCH, 1, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_mix_sw_codec1 =
	SOC_SINGLE_EXT("Switch", SST_MIX_SWITCH, 2, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_mix_sw_sprot_l0 =
	SOC_SINGLE_EXT("Switch", SST_MIX_SWITCH, 3, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_mix_sw_media_l1 =
	SOC_SINGLE_EXT("Switch", SST_MIX_SWITCH, 4, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_mix_sw_media_l2 =
	SOC_SINGLE_EXT("Switch", SST_MIX_SWITCH, 5, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_mix_sw_speech_tx =
	SOC_SINGLE_EXT("Switch", SST_MIX_SWITCH, 6, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_mix_sw_speech_rx =
	SOC_SINGLE_EXT("Switch", SST_MIX_SWITCH, 7, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_mix_sw_voip =
	SOC_SINGLE_EXT("Switch", SST_MIX_SWITCH, 8, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_mix_sw_pcm0 =
	SOC_SINGLE_EXT("Switch", SST_MIX_SWITCH, 9, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_mix_sw_pcm1 =
	SOC_SINGLE_EXT("Switch", SST_MIX_SWITCH, 10, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_mix_sw_pcm2 =
	SOC_SINGLE_EXT("Switch", SST_MIX_SWITCH, 11, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_mix_sw_aware =
	SOC_SINGLE_EXT("Switch", SST_MIX_SWITCH, 12, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_mix_sw_vad =
	SOC_SINGLE_EXT("Switch", SST_MIX_SWITCH, 13, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_mix_sw_media0 =
	SOC_SINGLE_EXT("Switch", SST_MIX_SWITCH, 14, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_mix_sw_media1 =
	SOC_SINGLE_EXT("Switch", SST_MIX_SWITCH, 15, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_mix_sw_fm =
	SOC_SINGLE_EXT("Switch", SST_MIX_SWITCH, 16, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_out_sw_modem =
	SOC_SINGLE_EXT("Switch", SST_OUT_SWITCH, 0, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_out_sw_codec0 =
	SOC_SINGLE_EXT("Switch", SST_OUT_SWITCH, 1, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_out_sw_codec1 =
	SOC_SINGLE_EXT("Switch", SST_OUT_SWITCH, 2, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_out_sw_speech_tx =
	SOC_SINGLE_EXT("Switch", SST_OUT_SWITCH, 6, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_out_sw_speech_rx =
	SOC_SINGLE_EXT("Switch", SST_OUT_SWITCH, 7, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_out_sw_voip =
	SOC_SINGLE_EXT("Switch", SST_OUT_SWITCH, 8, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_out_sw_pcm0 =
	SOC_SINGLE_EXT("Switch", SST_OUT_SWITCH, 9, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_out_sw_pcm1 =
	SOC_SINGLE_EXT("Switch", SST_OUT_SWITCH, 10, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_out_sw_pcm2 =
	SOC_SINGLE_EXT("Switch", SST_OUT_SWITCH, 11, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_out_sw_aware =
	SOC_SINGLE_EXT("Switch", SST_OUT_SWITCH, 12, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_out_sw_vad =
	SOC_SINGLE_EXT("Switch", SST_OUT_SWITCH, 13, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_out_sw_media0 =
	SOC_SINGLE_EXT("Switch", SST_OUT_SWITCH, 14, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_out_sw_media1 =
	SOC_SINGLE_EXT("Switch", SST_OUT_SWITCH, 15, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_out_sw_fm =
	SOC_SINGLE_EXT("Switch", SST_OUT_SWITCH, 16, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_in_sw_modem =
	SOC_SINGLE_EXT("Switch", SST_IN_SWITCH, 0, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_in_sw_codec0 =
	SOC_SINGLE_EXT("Switch", SST_IN_SWITCH, 1, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_in_sw_codec1 =
	SOC_SINGLE_EXT("Switch", SST_IN_SWITCH, 2, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_in_sw_sidetone =
	SOC_SINGLE_EXT("Switch", SST_IN_SWITCH, 3, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_in_sw_speech_tx =
	SOC_SINGLE_EXT("Switch", SST_IN_SWITCH, 4, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_in_sw_speech_rx  =
	SOC_SINGLE_EXT("Switch", SST_IN_SWITCH, 5, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_in_sw_tone =
	SOC_SINGLE_EXT("Switch", SST_IN_SWITCH, 6, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_in_sw_voip =
	SOC_SINGLE_EXT("Switch", SST_IN_SWITCH, 7, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_in_sw_pcm0 =
	SOC_SINGLE_EXT("Switch", SST_IN_SWITCH, 8, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_in_sw_pcm1 =
	SOC_SINGLE_EXT("Switch", SST_IN_SWITCH, 9, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_in_sw_media0 =
	SOC_SINGLE_EXT("Switch", SST_IN_SWITCH, 10, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_in_sw_media1 =
	SOC_SINGLE_EXT("Switch", SST_IN_SWITCH, 11, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_in_sw_media2 =
	SOC_SINGLE_EXT("Switch", SST_IN_SWITCH, 12, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_in_sw_fm =
	SOC_SINGLE_EXT("Switch", SST_IN_SWITCH, 13, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_soc_dapm_widget sst_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("Modem IN"),
	SND_SOC_DAPM_INPUT("Codec IN0"),
	SND_SOC_DAPM_INPUT("Codec IN1"),
	SND_SOC_DAPM_INPUT("Tone IN"),
	SND_SOC_DAPM_INPUT("FM IN"),
	SND_SOC_DAPM_OUTPUT("Modem OUT"),
	SND_SOC_DAPM_OUTPUT("Codec OUT0"),
	SND_SOC_DAPM_OUTPUT("Codec OUT1"),
	SND_SOC_DAPM_OUTPUT("FM OUT"),
	SND_SOC_DAPM_AIF_IN("Voip IN", "VoIP", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("Media IN0", "Compress", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("Media IN1", "PCM", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("Voip OUT", "VoIP", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("PCM1 OUT", "Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("Aware OUT", "Aware", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("VAD OUT", "VAD", 0, SND_SOC_NOPM, 0, 0),

	/* output mixers */
	SND_SOC_DAPM_MIXER("MIX Modem", SND_SOC_NOPM, 0, 0,
		sst_mix_modem_controls, ARRAY_SIZE(sst_mix_modem_controls)),
	SND_SOC_DAPM_MIXER("MIX Codec0", SND_SOC_NOPM, 0, 0,
		sst_mix_codec0_controls , ARRAY_SIZE(sst_mix_codec0_controls)),
	SND_SOC_DAPM_MIXER("MIX Codec1", SND_SOC_NOPM, 0, 0,
		sst_mix_codec1_controls, ARRAY_SIZE(sst_mix_codec1_controls)),
	SND_SOC_DAPM_MIXER("MIX Sprot L0", SND_SOC_NOPM, 0, 0,
		sst_mix_sprot_l0_controls, ARRAY_SIZE(sst_mix_sprot_l0_controls)),
	SND_SOC_DAPM_MIXER("MIX Media L1", SND_SOC_NOPM, 0, 0,
		sst_mix_media_l1_controls, ARRAY_SIZE(sst_mix_media_l1_controls)),
	SND_SOC_DAPM_MIXER("MIX Media L2", SND_SOC_NOPM, 0, 0,
		sst_mix_media_l2_controls, ARRAY_SIZE(sst_mix_media_l2_controls)),
	SND_SOC_DAPM_MIXER("MIX Speech Tx", SND_SOC_NOPM, 0, 0,
		sst_mix_speech_tx_controls, ARRAY_SIZE(sst_mix_speech_tx_controls)),
	SND_SOC_DAPM_MIXER("MIX Speech Rx", SND_SOC_NOPM, 0, 0,
		sst_mix_speech_rx_controls, ARRAY_SIZE(sst_mix_speech_rx_controls)),
	SND_SOC_DAPM_MIXER("MIX Voip", SND_SOC_NOPM, 0, 0,
		sst_mix_voip_controls, ARRAY_SIZE(sst_mix_voip_controls)),
	SND_SOC_DAPM_MIXER("MIX PCM0", SND_SOC_NOPM, 0, 0,
		sst_mix_pcm0_controls, ARRAY_SIZE(sst_mix_pcm0_controls)),
	SND_SOC_DAPM_MIXER("MIX PCM1", SND_SOC_NOPM, 0, 0,
		sst_mix_pcm1_controls, ARRAY_SIZE(sst_mix_pcm1_controls)),
	SND_SOC_DAPM_MIXER("MIX PCM2", SND_SOC_NOPM, 0, 0,
		sst_mix_pcm2_controls, ARRAY_SIZE(sst_mix_pcm2_controls)),
	SND_SOC_DAPM_MIXER("MIX Aware", SND_SOC_NOPM, 0, 0,
		sst_mix_aware_controls, ARRAY_SIZE(sst_mix_aware_controls)),
	SND_SOC_DAPM_MIXER("MIX VAD", SND_SOC_NOPM, 0, 0,
		sst_mix_vad_controls, ARRAY_SIZE(sst_mix_vad_controls)),
	SND_SOC_DAPM_MIXER("MIX Media0", SND_SOC_NOPM, 0, 0,
		sst_mix_media0_controls, ARRAY_SIZE(sst_mix_media0_controls)),
	SND_SOC_DAPM_MIXER("MIX Media1", SND_SOC_NOPM, 0, 0,
		sst_mix_media1_controls, ARRAY_SIZE(sst_mix_media1_controls)),
	SND_SOC_DAPM_MIXER("MIX FM", SND_SOC_NOPM, 0, 0,
		sst_mix_fm_controls, ARRAY_SIZE(sst_mix_fm_controls)),

	/* switches for mixer outputs */
	SND_SOC_DAPM_SWITCH("Mix Modem Switch", SND_SOC_NOPM, 0, 0,
			&sst_mix_sw_modem),
	SND_SOC_DAPM_SWITCH("Mix Codec0 Switch", SND_SOC_NOPM, 0, 0,
			&sst_mix_sw_codec0),
	SND_SOC_DAPM_SWITCH("Mix Codec1 Switch", SND_SOC_NOPM, 0, 0,
			&sst_mix_sw_codec1),
	SND_SOC_DAPM_SWITCH("Mix Sprot L0 Switch", SND_SOC_NOPM, 0, 0,
			&sst_mix_sw_sprot_l0),
	SND_SOC_DAPM_SWITCH("Mix Media L1 Switch", SND_SOC_NOPM, 0, 0,
			&sst_mix_sw_media_l1),
	SND_SOC_DAPM_SWITCH("Mix Media L2 Switch", SND_SOC_NOPM, 0, 0,
			&sst_mix_sw_media_l2),
	SND_SOC_DAPM_SWITCH("Mix Speech Tx Switch", SND_SOC_NOPM, 0, 0,
			&sst_mix_sw_speech_tx),
	SND_SOC_DAPM_SWITCH("Mix Speech Rx Switch", SND_SOC_NOPM, 0, 0,
			&sst_mix_sw_speech_rx),
	SND_SOC_DAPM_SWITCH("Mix Voip Switch", SND_SOC_NOPM, 0, 0,
			&sst_mix_sw_voip),
	SND_SOC_DAPM_SWITCH("Mix PCM0 Switch", SND_SOC_NOPM, 0, 0,
			&sst_mix_sw_pcm0),
	SND_SOC_DAPM_SWITCH("Mix PCM1 Switch", SND_SOC_NOPM, 0, 0,
			&sst_mix_sw_pcm1),
	SND_SOC_DAPM_SWITCH("Mix PCM2 Switch", SND_SOC_NOPM, 0, 0,
			&sst_mix_sw_pcm2),
	SND_SOC_DAPM_SWITCH("Mix Aware Switch", SND_SOC_NOPM, 0, 0,
			&sst_mix_sw_aware),
	SND_SOC_DAPM_SWITCH("Mix VAD Switch", SND_SOC_NOPM, 0, 0,
			&sst_mix_sw_vad),
	SND_SOC_DAPM_SWITCH("Mix Media0 Switch", SND_SOC_NOPM, 0, 0,
			&sst_mix_sw_media0),
	SND_SOC_DAPM_SWITCH("Mix Media1 Switch", SND_SOC_NOPM, 0, 0,
			&sst_mix_sw_media1),
	SND_SOC_DAPM_SWITCH("Mix FM Switch", SND_SOC_NOPM, 0, 0,
			&sst_mix_sw_fm),

	/* output pipeline switches */
	SND_SOC_DAPM_SWITCH("Out Modem Switch", SND_SOC_NOPM, 0, 0,
			&sst_out_sw_modem),
	SND_SOC_DAPM_SWITCH("Out Codec0 Switch", SND_SOC_NOPM, 0, 0,
			&sst_out_sw_codec0),
	SND_SOC_DAPM_SWITCH("Out Codec1 Switch", SND_SOC_NOPM, 0, 0,
			&sst_out_sw_codec1),
	SND_SOC_DAPM_SWITCH("Out Speech Tx Switch", SND_SOC_NOPM, 0, 0,
			&sst_out_sw_speech_tx),
	SND_SOC_DAPM_SWITCH("Out Speech Rx Switch", SND_SOC_NOPM, 0, 0,
			&sst_out_sw_speech_rx),
	SND_SOC_DAPM_SWITCH("Out Voip Switch", SND_SOC_NOPM, 0, 0,
			&sst_out_sw_voip),
	SND_SOC_DAPM_SWITCH("Out PCM0 Switch", SND_SOC_NOPM, 0, 0,
			&sst_out_sw_pcm0),
	SND_SOC_DAPM_SWITCH("Out PCM1 Switch", SND_SOC_NOPM, 0, 0,
			&sst_out_sw_pcm1),
	SND_SOC_DAPM_SWITCH("Out PCM2 Switch", SND_SOC_NOPM, 0, 0,
			&sst_out_sw_pcm2),
	SND_SOC_DAPM_SWITCH("Out Aware Switch", SND_SOC_NOPM, 0, 0,
			&sst_out_sw_aware),
	SND_SOC_DAPM_SWITCH("Out VAD Switch", SND_SOC_NOPM, 0, 0,
			&sst_out_sw_vad),
	SND_SOC_DAPM_SWITCH("Out Media0 Switch", SND_SOC_NOPM, 0, 0,
			&sst_out_sw_media0),
	SND_SOC_DAPM_SWITCH("Out Media1 Switch", SND_SOC_NOPM, 0, 0,
			&sst_out_sw_media1),
	SND_SOC_DAPM_SWITCH("Out FM Switch", SND_SOC_NOPM, 0, 0,
			&sst_out_sw_fm),

	/* Input pipeline switches */
	SND_SOC_DAPM_SWITCH("In Modem Switch", SND_SOC_NOPM, 0, 0,
		       &sst_in_sw_modem),
	SND_SOC_DAPM_SWITCH("In Codec0 Switch", SND_SOC_NOPM, 0, 0,
		       &sst_in_sw_codec0),
	SND_SOC_DAPM_SWITCH("In Codec1 Switch", SND_SOC_NOPM, 0, 0,
		       &sst_in_sw_codec1),
	SND_SOC_DAPM_SWITCH("In Speech Tx Switch", SND_SOC_NOPM, 0, 0,
		       &sst_in_sw_speech_tx),
	SND_SOC_DAPM_SWITCH("In Speech Rx Switch", SND_SOC_NOPM, 0, 0,
		       &sst_in_sw_speech_rx),
	SND_SOC_DAPM_SWITCH("In Tone Switch", SND_SOC_NOPM, 0, 0,
		       &sst_in_sw_tone),
	SND_SOC_DAPM_SWITCH("In Voip Switch", SND_SOC_NOPM, 0, 0,
		       &sst_in_sw_voip),
	SND_SOC_DAPM_SWITCH("In PCM0 Switch", SND_SOC_NOPM, 0, 0,
		       &sst_in_sw_pcm0),
	SND_SOC_DAPM_SWITCH("In PCM1 Switch", SND_SOC_NOPM, 0, 0,
		       &sst_in_sw_pcm1),
	SND_SOC_DAPM_SWITCH("In Media0 Switch", SND_SOC_NOPM, 0, 0,
		       &sst_in_sw_media0),
	SND_SOC_DAPM_SWITCH("In Media1 Switch", SND_SOC_NOPM, 0, 0,
		       &sst_in_sw_media1),
	SND_SOC_DAPM_SWITCH("In Media2 Switch", SND_SOC_NOPM, 0, 0,
		       &sst_in_sw_media2),
	SND_SOC_DAPM_SWITCH("In FM Switch", SND_SOC_NOPM, 0, 0,
		       &sst_in_sw_fm),
};

static const struct snd_soc_dapm_route intercon[] = {
	/* media mixer settings */
	{ "In Media0 Switch", "Switch", "Media IN0"},
	{ "In Media1 Switch", "Switch", "Media IN1"},
	{ "MIX Media0", "Media0", "In Media0 Switch"},
	{ "MIX Media0", "Media1", "In Media1 Switch"},
	{ "MIX Media0", "Media2", "In Media2 Switch"},
	{ "MIX Media1", "Media0", "In Media0 Switch"},
	{ "MIX Media1", "Media1", "In Media1 Switch"},
	{ "MIX Media1", "Media2", "In Media2 Switch"},

	/* media to main mixer intercon */
	/* two media paths from media to main */
	{ "Mix Media0 Switch", "Switch", "MIX Media0"},
	{ "Out Media0 Switch", "Switch", "Mix Media0 Switch"},
	{ "In PCM0 Switch", "Switch", "Out Media0 Switch"},
	{ "Mix Media1 Switch", "Switch", "MIX Media1"},
	{ "Out Media1 Switch", "Switch", "Mix Media1 Switch"},
	{ "In PCM1 Switch", "Switch", "Out Media1 Switch"},
	/* one back from main to media */
	{ "Mix PCM0 Switch", "Switch", "MIX PCM0"},
	{ "Out PCM0 Switch", "Switch", "Mix PCM0 Switch"},
	{ "In Media2 Switch", "Switch", "Out PCM0 Switch"},

	/* main mixer inputs - all inputs connect to mixer */
	{ "MIX Modem", "Modem", "In Modem Switch"},
	{ "MIX Modem", "Codec0", "In Codec0 Switch"},
	{ "MIX Modem", "Codec1", "In Codec1 Switch"},
	{ "MIX Modem", "Speech_Tx", "In Speech Tx Switch"},
	{ "MIX Modem", "Speech_Rx", "In Speech Rx Switch"},
	{ "MIX Modem", "Tone", "In Tone Switch"},
	{ "MIX Modem", "Voip", "In Voip Switch"},
	{ "MIX Modem", "PCM0", "In PCM0 Switch"},
	{ "MIX Modem", "PCM1", "In PCM1 Switch"},
	{ "MIX Modem", "FM", "In FM Switch"},
	/* loops have output switches coming back to mixers */
	{ "MIX Modem", "Sprot_L0", "Mix Sprot L0 Switch"},
	{ "MIX Modem", "Media_L1", "Mix Media L1 Switch"},
	{ "MIX Modem", "Media_L2", "Mix Media L2 Switch"},
	/* sidetone comes from speech out */
	{ "MIX Modem", "Sidetone", "Mix Speech Tx Switch"},

	{ "MIX Codec0", "Modem", "In Modem Switch"},
	{ "MIX Codec0", "Codec0", "In Codec0 Switch"},
	{ "MIX Codec0", "Codec1", "In Codec1 Switch"},
	{ "MIX Codec0", "Speech_Tx", "In Speech Tx Switch"},
	{ "MIX Codec0", "Speech_Rx", "In Speech Rx Switch"},
	{ "MIX Codec0", "Tone", "In Tone Switch"},
	{ "MIX Codec0", "Voip", "In Voip Switch"},
	{ "MIX Codec0", "PCM0", "In PCM0 Switch"},
	{ "MIX Codec0", "PCM1", "In PCM1 Switch"},
	{ "MIX Codec0", "FM", "In FM Switch"},
	/* loops have output switches coming back to mixers */
	{ "MIX Codec0", "Sprot_L0", "Mix Sprot L0 Switch"},
	{ "MIX Codec0", "Media_L1", "Mix Media L1 Switch"},
	{ "MIX Codec0", "Media_L2", "Mix Media L2 Switch"},
	/* sidetone comes from speech out */
	{ "MIX Codec0", "Sidetone", "Mix Speech Tx Switch"},

	{ "MIX Codec1", "Modem", "In Modem Switch"},
	{ "MIX Codec1", "Codec0", "In Codec0 Switch"},
	{ "MIX Codec1", "Codec1", "In Codec1 Switch"},
	{ "MIX Codec1", "Speech_Tx", "In Speech Tx Switch"},
	{ "MIX Codec1", "Speech_Rx", "In Speech Rx Switch"},
	{ "MIX Codec1", "Tone", "In Tone Switch"},
	{ "MIX Codec1", "Voip", "In Voip Switch"},
	{ "MIX Codec1", "PCM0", "In PCM0 Switch"},
	{ "MIX Codec1", "PCM1", "In PCM1 Switch"},
	{ "MIX Codec1", "FM", "In FM Switch"},
	/* loops have output switches coming back to mixers */
	{ "MIX Codec1", "Sprot_L0", "Mix Sprot L0 Switch"},
	{ "MIX Codec1", "Media_L1", "Mix Media L1 Switch"},
	{ "MIX Codec1", "Media_L2", "Mix Media L2 Switch"},
	/* sidetone comes from speech out */
	{ "MIX Codec1", "Sidetone", "Mix Speech Tx Switch"},

	{ "MIX Sprot L0", "Modem", "In Modem Switch"},
	{ "MIX Sprot L0", "Codec0", "In Codec0 Switch"},
	{ "MIX Sprot L0", "Codec1", "In Codec1 Switch"},
	{ "MIX Sprot L0", "Speech_Tx", "In Speech Tx Switch"},
	{ "MIX Sprot L0", "Speech_Rx", "In Speech Rx Switch"},
	{ "MIX Sprot L0", "Tone", "In Tone Switch"},
	{ "MIX Sprot L0", "Voip", "In Voip Switch"},
	{ "MIX Sprot L0", "PCM0", "In PCM0 Switch"},
	{ "MIX Sprot L0", "PCM1", "In PCM1 Switch"},
	{ "MIX Sprot L0", "FM", "In FM Switch"},
	/* loops have output switches coming back to mixers */
	{ "MIX Sprot L0", "Sprot_L0", "Mix Sprot L0 Switch"},
	{ "MIX Sprot L0", "Media_L1", "Mix Media L1 Switch"},
	{ "MIX Sprot L0", "Media_L2", "Mix Media L2 Switch"},
	/* sidetone comes from speech out */
	{ "MIX Sprot L0", "Sidetone", "Mix Speech Tx Switch"},

	{ "MIX Media L1", "Modem", "In Modem Switch"},
	{ "MIX Media L1", "Codec0", "In Codec0 Switch"},
	{ "MIX Media L1", "Codec1", "In Codec1 Switch"},
	{ "MIX Media L1", "Speech_Tx", "In Speech Tx Switch"},
	{ "MIX Media L1", "Speech_Rx", "In Speech Rx Switch"},
	{ "MIX Media L1", "Tone", "In Tone Switch"},
	{ "MIX Media L1", "Voip", "In Voip Switch"},
	{ "MIX Media L1", "PCM0", "In PCM0 Switch"},
	{ "MIX Media L1", "PCM1", "In PCM1 Switch"},
	{ "MIX Media L1", "FM", "In FM Switch"},
	/* loops have output switches coming back to mixers */
	{ "MIX Media L1", "Sprot_L0", "Mix Sprot L0 Switch"},
	{ "MIX Media L1", "Media_L1", "Mix Media L1 Switch"},
	{ "MIX Media L1", "Media_L2", "Mix Media L2 Switch"},
	/* sidetone comes from speech out */
	{ "MIX Media L1", "Sidetone", "Mix Speech Tx Switch"},

	{ "MIX Media L2", "Modem", "In Modem Switch"},
	{ "MIX Media L2", "Codec0", "In Codec0 Switch"},
	{ "MIX Media L2", "Codec1", "In Codec1 Switch"},
	{ "MIX Media L2", "Speech_Tx", "In Speech Tx Switch"},
	{ "MIX Media L2", "Speech_Rx", "In Speech Rx Switch"},
	{ "MIX Media L2", "Tone", "In Tone Switch"},
	{ "MIX Media L2", "Voip", "In Voip Switch"},
	{ "MIX Media L2", "PCM0", "In PCM0 Switch"},
	{ "MIX Media L2", "PCM1", "In PCM1 Switch"},
	{ "MIX Media L2", "FM", "In FM Switch"},
	/* loops have output switches coming back to mixers */
	{ "MIX Media L2", "Sprot_L0", "Mix Sprot L0 Switch"},
	{ "MIX Media L2", "Media_L1", "Mix Media L1 Switch"},
	{ "MIX Media L2", "Media_L2", "Mix Media L2 Switch"},
	/* sidetone comes from speech out */
	{ "MIX Media L2", "Sidetone", "Mix Speech Tx Switch"},

	{ "MIX Speech Rx", "Modem", "In Modem Switch"},
	{ "MIX Speech Rx", "Codec0", "In Codec0 Switch"},
	{ "MIX Speech Rx", "Codec1", "In Codec1 Switch"},
	{ "MIX Speech Rx", "Speech_Tx", "In Speech Tx Switch"},
	{ "MIX Speech Rx", "Speech_Rx", "In Speech Rx Switch"},
	{ "MIX Speech Rx", "Tone", "In Tone Switch"},
	{ "MIX Speech Rx", "Voip", "In Voip Switch"},
	{ "MIX Speech Rx", "PCM0", "In PCM0 Switch"},
	{ "MIX Speech Rx", "PCM1", "In PCM1 Switch"},
	{ "MIX Speech Rx", "FM", "In FM Switch"},
	/* loops have output switches coming back to mixers */
	{ "MIX Speech Rx", "Sprot_L0", "Mix Sprot L0 Switch"},
	{ "MIX Speech Rx", "Media_L1", "Mix Media L1 Switch"},
	{ "MIX Speech Rx", "Media_L2", "Mix Media L2 Switch"},
	/* sidetone comes from speech out */
	{ "MIX Speech Rx", "Sidetone", "Mix Speech Tx Switch"},

	{ "MIX Speech Tx", "Modem", "In Modem Switch"},
	{ "MIX Speech Tx", "Codec0", "In Codec0 Switch"},
	{ "MIX Speech Tx", "Codec1", "In Codec1 Switch"},
	{ "MIX Speech Tx", "Speech_Tx", "In Speech Tx Switch"},
	{ "MIX Speech Tx", "Speech_Rx", "In Speech Rx Switch"},
	{ "MIX Speech Tx", "Tone", "In Tone Switch"},
	{ "MIX Speech Tx", "Voip", "In Voip Switch"},
	{ "MIX Speech Tx", "PCM0", "In PCM0 Switch"},
	{ "MIX Speech Tx", "PCM1", "In PCM1 Switch"},
	{ "MIX Speech Tx", "FM", "In FM Switch"},
	/* loops have output switches coming back to mixers */
	{ "MIX Speech Tx", "Sprot_L0", "Mix Sprot L0 Switch"},
	{ "MIX Speech Tx", "Media_L1", "Mix Media L1 Switch"},
	{ "MIX Speech Tx", "Media_L2", "Mix Media L2 Switch"},
	/* sidetone comes from speech out */
	{ "MIX Speech Tx", "Sidetone", "Mix Speech Tx Switch"},

	{ "MIX Voip", "Modem", "In Modem Switch"},
	{ "MIX Voip", "Codec0", "In Codec0 Switch"},
	{ "MIX Voip", "Codec1", "In Codec1 Switch"},
	{ "MIX Voip", "Speech_Tx", "In Speech Tx Switch"},
	{ "MIX Voip", "Speech_Rx", "In Speech Rx Switch"},
	{ "MIX Voip", "Tone", "In Tone Switch"},
	{ "MIX Voip", "Voip", "In Voip Switch"},
	{ "MIX Voip", "PCM0", "In PCM0 Switch"},
	{ "MIX Voip", "PCM1", "In PCM1 Switch"},
	{ "MIX Voip", "FM", "In FM Switch"},
	/* loops have output switches coming back to mixers */
	{ "MIX Voip", "Sprot_L0", "Mix Sprot L0 Switch"},
	{ "MIX Voip", "Media_L1", "Mix Media L1 Switch"},
	{ "MIX Voip", "Media_L2", "Mix Media L2 Switch"},
	/* sidetone comes from speech out */
	{ "MIX Voip", "Sidetone", "Mix Speech Tx Switch"},

	{ "MIX PCM0", "Modem", "In Modem Switch"},
	{ "MIX PCM0", "Codec0", "In Codec0 Switch"},
	{ "MIX PCM0", "Codec1", "In Codec1 Switch"},
	{ "MIX PCM0", "Speech_Tx", "In Speech Tx Switch"},
	{ "MIX PCM0", "Speech_Rx", "In Speech Rx Switch"},
	{ "MIX PCM0", "Tone", "In Tone Switch"},
	{ "MIX PCM0", "Voip", "In Voip Switch"},
	{ "MIX PCM0", "PCM0", "In PCM0 Switch"},
	{ "MIX PCM0", "PCM1", "In PCM1 Switch"},
	{ "MIX PCM0", "FM", "In FM Switch"},
	/* loops have output switches coming back to mixers */
	{ "MIX PCM0", "Sprot_L0", "Mix Sprot L0 Switch"},
	{ "MIX PCM0", "Media_L1", "Mix Media L1 Switch"},
	{ "MIX PCM0", "Media_L2", "Mix Media L2 Switch"},
	/* sidetone comes from speech out */
	{ "MIX PCM0", "Sidetone", "Mix Speech Tx Switch"},

	{ "MIX PCM1", "Modem", "In Modem Switch"},
	{ "MIX PCM1", "Codec0", "In Codec0 Switch"},
	{ "MIX PCM1", "Codec1", "In Codec1 Switch"},
	{ "MIX PCM1", "Speech_Tx", "In Speech Tx Switch"},
	{ "MIX PCM1", "Speech_Rx", "In Speech Rx Switch"},
	{ "MIX PCM1", "Tone", "In Tone Switch"},
	{ "MIX PCM1", "Voip", "In Voip Switch"},
	{ "MIX PCM1", "PCM0", "In PCM0 Switch"},
	{ "MIX PCM1", "PCM1", "In PCM1 Switch"},
	{ "MIX PCM1", "FM", "In FM Switch"},
	/* loops have output switches coming back to mixers */
	{ "MIX PCM1", "Sprot_L0", "Mix Sprot L0 Switch"},
	{ "MIX PCM1", "Media_L1", "Mix Media L1 Switch"},
	{ "MIX PCM1", "Media_L2", "Mix Media L2 Switch"},
	/* sidetone comes from speech out */
	{ "MIX PCM1", "Sidetone", "Mix Speech Tx Switch"},

	{ "MIX PCM2", "Modem", "In Modem Switch"},
	{ "MIX PCM2", "Codec0", "In Codec0 Switch"},
	{ "MIX PCM2", "Codec1", "In Codec1 Switch"},
	{ "MIX PCM2", "Speech_Tx", "In Speech Tx Switch"},
	{ "MIX PCM2", "Speech_Rx", "In Speech Rx Switch"},
	{ "MIX PCM2", "Tone", "In Tone Switch"},
	{ "MIX PCM2", "Voip", "In Voip Switch"},
	{ "MIX PCM2", "PCM0", "In PCM0 Switch"},
	{ "MIX PCM2", "PCM1", "In PCM1 Switch"},
	{ "MIX PCM2", "FM", "In FM Switch"},
	/* loops have output switches coming back to mixers */
	{ "MIX PCM2", "Sprot_L0", "Mix Sprot L0 Switch"},
	{ "MIX PCM2", "Media_L1", "Mix Media L1 Switch"},
	{ "MIX PCM2", "Media_L2", "Mix Media L2 Switch"},
	/* sidetone comes from speech out */
	{ "MIX PCM2", "Sidetone", "Mix Speech Tx Switch"},

	{ "MIX Aware", "Modem", "In Modem Switch"},
	{ "MIX Aware", "Codec0", "In Codec0 Switch"},
	{ "MIX Aware", "Codec1", "In Codec1 Switch"},
	{ "MIX Aware", "Speech_Tx", "In Speech Tx Switch"},
	{ "MIX Aware", "Speech_Rx", "In Speech Rx Switch"},
	{ "MIX Aware", "Tone", "In Tone Switch"},
	{ "MIX Aware", "Voip", "In Voip Switch"},
	{ "MIX Aware", "PCM0", "In PCM0 Switch"},
	{ "MIX Aware", "PCM1", "In PCM1 Switch"},
	{ "MIX Aware", "FM", "In FM Switch"},
	/* loops have output switches coming back to mixers */
	{ "MIX Aware", "Sprot_L0", "Mix Sprot L0 Switch"},
	{ "MIX Aware", "Media_L1", "Mix Media L1 Switch"},
	{ "MIX Aware", "Media_L2", "Mix Media L2 Switch"},
	/* sidetone comes from speech out */
	{ "MIX Aware", "Sidetone", "Mix Speech Tx Switch"},

	{ "MIX VAD", "Modem", "In Modem Switch"},
	{ "MIX VAD", "Codec0", "In Codec0 Switch"},
	{ "MIX VAD", "Codec1", "In Codec1 Switch"},
	{ "MIX VAD", "Speech_Tx", "In Speech Tx Switch"},
	{ "MIX VAD", "Speech_Rx", "In Speech Rx Switch"},
	{ "MIX VAD", "Tone", "In Tone Switch"},
	{ "MIX VAD", "Voip", "In Voip Switch"},
	{ "MIX VAD", "PCM0", "In PCM0 Switch"},
	{ "MIX VAD", "PCM1", "In PCM1 Switch"},
	{ "MIX VAD", "FM", "In FM Switch"},
	/* loops have output switches coming back to mixers */
	{ "MIX VAD", "Sprot_L0", "Mix Sprot L0 Switch"},
	{ "MIX VAD", "Media_L1", "Mix Media L1 Switch"},
	{ "MIX VAD", "Media_L2", "Mix Media L2 Switch"},
	/* sidetone comes from speech out */
	{ "MIX VAD", "Sidetone", "Mix Speech Tx Switch"},

	{ "MIX FM", "Modem", "In Modem Switch"},
	{ "MIX FM", "Codec0", "In Codec0 Switch"},
	{ "MIX FM", "Codec1", "In Codec1 Switch"},
	{ "MIX FM", "Speech_Tx", "In Speech Tx Switch"},
	{ "MIX FM", "Speech_Rx", "In Speech Rx Switch"},
	{ "MIX FM", "Tone", "In Tone Switch"},
	{ "MIX FM", "Voip", "In Voip Switch"},
	{ "MIX FM", "PCM0", "In PCM0 Switch"},
	{ "MIX FM", "PCM1", "In PCM1 Switch"},
	{ "MIX FM", "FM", "In FM Switch"},
	/* loops have output switches coming back to mixers */
	{ "MIX FM", "Sprot_L0", "Mix Sprot L0 Switch"},
	{ "MIX FM", "Media_L1", "Mix Media L1 Switch"},
	{ "MIX FM", "Media_L2", "Mix Media L2 Switch"},
	/* sidetone comes from speech out */
	{ "MIX FM", "Sidetone", "Mix Speech Tx Switch"},

	/* now connect the mixers to output switches */
	{ "Mix Modem Switch", "Switch", "MIX Modem"},
	{ "Out Modem Switch", "Switch", "Mix Modem Switch"},
	{ "Mix Codec0 Switch", "Switch", "MIX Codec0"},
	{ "Out Codec0 Switch", "Switch", "Mix Codec0 Switch"},
	{ "Mix Codec1 Switch", "Switch", "MIX Codec1"},
	{ "Out Codec1 Switch", "Switch", "Mix Codec1 Switch"},
	{ "Mix Speech Tx Switch", "Switch", "MIX Speech Tx"},
	{ "Out Speech Tx Switch", "Switch", "Mix Speech Tx Switch"},
	{ "Mix Speech Rx Switch", "Switch", "MIX Speech Rx"},
	{ "Out Speech Rx Switch", "Switch", "Mix Speech Rx Switch"},
	{ "Mix Voip Switch", "Switch", "MIX Voip"},
	{ "Out Voip Switch", "Switch", "Mix Voip Switch"},
	{ "Mix Aware Switch", "Switch", "MIX Aware"},
	{ "Out Aware Switch", "Switch", "Mix Aware Switch"},
	{ "Mix VAD Switch", "Switch", "MIX VAD"},
	{ "Out VAD Switch", "Switch", "Mix VAD Switch"},
	{ "Mix FM Switch", "Switch", "MIX FM"},
	{ "Out FM Switch", "Switch", "Mix FM Switch"},
	{ "Mix PCM1 Switch", "Switch", "MIX PCM1"},
	{ "Out PCM1 Switch", "Switch", "Mix PCM1 Switch"},
	{ "Mix PCM2 Switch", "Switch", "MIX PCM2"},
	{ "Out PCM2 Switch", "Switch", "Mix PCM2 Switch"},

	/* the loops
	 * media loops dont have i/p o/p switches, just mixer enable
	 */
	{ "Mix Sprot L0 Switch", "Switch", "MIX Sprot L0"},
	{ "Mix Media L1 Switch", "Switch", "MIX Media L1"},
	{ "Mix Media L2 Switch", "Switch", "MIX Media L2"},
	/* so no need as mixer switches are
	 * inputs to all mixers
	 * need to connect speech loops here
	 */
	{ "In Speech Rx Switch", "Switch", "Out Speech Rx Switch"},
	{ "In Speech Tx Switch", "Switch", "Out Speech Tx Switch"},
	/* last one, connect the output switches to ip's
	 * and op's. Also connect the AIFs
	 */
	{ "In Modem Switch", "Switch", "Modem IN"},
	{ "In Codec0 Switch", "Switch", "Codec IN0"},
	{ "In Codec1 Switch", "Switch", "Codec IN1"},
	{ "In Tone Switch", "Switch", "Tone IN"},
	{ "In FM Switch", "Switch", "FM IN"},

	{ "Modem OUT", NULL, "Out Modem Switch"},
	{ "Codec OUT0", NULL, "Out Codec0 Switch"},
	{ "Codec OUT1", NULL, "Out Codec1 Switch"},
	{ "FM OUT", NULL, "Out FM Switch"},

	{ "In Voip Switch", "Switch", "Voip IN"},

	{ "Voip OUT", NULL, "Out Voip Switch"},
	{ "PCM1 OUT", NULL, "Out PCM1 Switch"},
	{ "Aware OUT", NULL, "Out Aware Switch"},
	{ "VAD OUT", NULL, "Out VAD Switch"},
};

int sst_byte_control_get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct sst_data *sst = snd_soc_platform_get_drvdata(platform);

	pr_debug("in %s\n", __func__);
	memcpy(ucontrol->value.bytes.data, sst->byte_stream, SST_MAX_BIN_BYTES);
	print_hex_dump_bytes(__func__, DUMP_PREFIX_OFFSET,
			     (const void *)sst->byte_stream, 32);
	return 0;
}

static int sst_check_binary_input(char *stream)
{
	struct snd_sst_bytes_v2 *bytes = (struct snd_sst_bytes_v2 *)stream;

	if (bytes->len == 0 || bytes->len > 1000) {
		pr_err("length out of bounds %d\n", bytes->len);
		return -EINVAL;
	}
	if (bytes->type == 0 || bytes->type > SND_SST_BYTES_GET) {
		pr_err("type out of bounds: %d\n", bytes->type);
		return -EINVAL;
	}
	if (bytes->block > 1) {
		pr_err("block invalid %d\n", bytes->block);
		return -EINVAL;
	}
	if (bytes->task_id == SST_TASK_ID_NONE || bytes->task_id > SST_TASK_ID_MAX) {
		pr_err("taskid invalid %d\n", bytes->task_id);
		return -EINVAL;
	}

	return 0;
}

int sst_byte_control_set(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct sst_data *sst = snd_soc_platform_get_drvdata(platform);
	int ret = 0;

	pr_debug("in %s\n", __func__);
	mutex_lock(&sst->lock);
	memcpy(sst->byte_stream, ucontrol->value.bytes.data, SST_MAX_BIN_BYTES);
	if (0 != sst_check_binary_input(sst->byte_stream)) {
		mutex_unlock(&sst->lock);
		return -EINVAL;
	}
	print_hex_dump_bytes(__func__, DUMP_PREFIX_OFFSET,
			     (const void *)sst->byte_stream, 32);
	ret = sst_dsp->ops->set_generic_params(SST_SET_BYTE_STREAM, sst->byte_stream);
	mutex_unlock(&sst->lock);

	return ret;
}

static int sst_pipe_id_control_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct sst_data *sst = snd_soc_platform_get_drvdata(platform);
	int ret = 0;

	ucontrol->value.integer.value[0] = sst->pipe_id;

	return ret;
}

static int sst_pipe_id_control_set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct sst_data *sst = snd_soc_platform_get_drvdata(platform);
	int ret = 0;

	sst->pipe_id = ucontrol->value.integer.value[0];
	pr_debug("%s: pipe_id %d", __func__, sst->pipe_id);

	return ret;
}

/* dB range for mrfld compress volume is -144dB to +36dB.
 * Gain library expects user input in terms of 0.1dB, for example,
 * 60 (in decimal) represents 6dB.
 * MW will pass 2's complement value for negative dB values.
 */
static int sst_compr_vol_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct sst_data *sst = snd_soc_platform_get_drvdata(platform);
	struct sst_algo_int_control_v2 *amc = (void *)kcontrol->private_value;
	u16 gain;
	unsigned int gain_offset, ret;

	sst_create_compr_vol_ipc(sst->byte_stream, SND_SST_BYTES_GET, amc);
	mutex_lock(&sst->lock);
	ret = sst_dsp->ops->set_generic_params(SST_SET_BYTE_STREAM,
						sst->byte_stream);
	mutex_unlock(&sst->lock);
	if (ret) {
		pr_err("failed to get compress vol from fw: %d\n", ret);
		return ret;
	}
	gain_offset = sizeof(struct snd_sst_bytes_v2) +
				sizeof(struct ipc_dsp_hdr);

	/* Get params format for vol ctrl lib, size 6 bytes :
	 * u16 left_gain, u16 right_gain, u16 ramp
	 */
	memcpy(&gain,
		(unsigned int *)(sst->byte_stream + gain_offset),
		sizeof(u16));
	pr_debug("%s: cell_gain = %d\n", __func__, gain);
	amc->value = gain;
	ucontrol->value.integer.value[0] = gain;
	return 0;
}

static int sst_compr_vol_set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct sst_data *sst = snd_soc_platform_get_drvdata(platform);
	struct sst_algo_int_control_v2 *amc = (void *)kcontrol->private_value;
	int ret = 0;
	unsigned int old_val;

	pr_debug("%s: cell_gain = %ld\n", __func__,\
				ucontrol->value.integer.value[0]);
	old_val = amc->value;
	amc->value = ucontrol->value.integer.value[0];
	sst_create_compr_vol_ipc(sst->byte_stream, SND_SST_BYTES_SET,
					amc);

	mutex_lock(&sst->lock);
	ret = sst_dsp->ops->set_generic_params(SST_SET_BYTE_STREAM,
						sst->byte_stream);
	mutex_unlock(&sst->lock);
	if (ret) {
		pr_err("failed to set compress vol in fw: %d\n", ret);
		amc->value = old_val;
		return ret;
	}
	return 0;
}

int sst_vtsv_enroll_set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct sst_data *sst = snd_soc_platform_get_drvdata(platform);
	int ret = 0;

	sst->vtsv_enroll = ucontrol->value.integer.value[0];
	mutex_lock(&sst->lock);
	if (sst->vtsv_enroll)
		ret = sst_dsp->ops->set_generic_params(SST_SET_VTSV_INFO,
					(void *)&sst->vtsv_enroll);
	mutex_unlock(&sst->lock);
	return ret;
}

int sst_vtsv_enroll_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct sst_data *sst = snd_soc_platform_get_drvdata(platform);

	ucontrol->value.integer.value[0] = sst->vtsv_enroll;
	return 0;
}

/* This value corresponds to two's complement value of -10 or -1dB */
#define SST_COMPR_VOL_MAX_INTEG_GAIN 0xFFF6
#define SST_COMPR_VOL_MUTE 0xFA60 /* 2's complement of -1440 or -144dB*/


static const struct snd_kcontrol_new sst_mrfld_controls[] = {
	SND_SOC_BYTES_EXT("SST Byte control", SST_MAX_BIN_BYTES,
		       sst_byte_control_get, sst_byte_control_set),
	SOC_SINGLE_EXT("SST Pipe_id control", SST_PIPE_CONTROL, 0, 0x9A, 0,
		sst_pipe_id_control_get, sst_pipe_id_control_set),
	SST_ALGO_KCONTROL_INT("Compress Volume", SST_COMPRESS_VOL,
		0, SST_COMPR_VOL_MAX_INTEG_GAIN, 0,
		sst_compr_vol_get, sst_compr_vol_set,
		SST_ALGO_VOLUME_CONTROL, PIPE_MEDIA0_IN, 0,
		SST_COMPR_VOL_MUTE),
	SOC_SINGLE_BOOL_EXT("SST VTSV Enroll", 0, sst_vtsv_enroll_get,
		       sst_vtsv_enroll_set),
};

static DEVICE_ULONG_ATTR(low_latency_threshold, 0644, ll_threshold);
static DEVICE_ULONG_ATTR(deep_buffer_threshold, 0644, db_threshold);

static struct attribute *device_sysfs_attrs[] = {
	&dev_attr_low_latency_threshold.attr.attr,
	&dev_attr_deep_buffer_threshold.attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = device_sysfs_attrs,
};

int sst_dsp_init(struct snd_soc_platform *platform)
{
	struct sst_data *sst = snd_soc_platform_get_drvdata(platform);
	int error = 0;

	sst->byte_stream = devm_kzalloc(platform->dev,
			SST_MAX_BIN_BYTES, GFP_KERNEL);
	if (sst->byte_stream == NULL) {
		pr_err("kzalloc failed\n");
		return -ENOMEM;
	}

	sst->widget = devm_kzalloc(platform->dev,
				   SST_NUM_WIDGETS * sizeof(*sst->widget),
				   GFP_KERNEL);
	if (sst->widget == NULL) {
		pr_err("kzalloc failed\n");
		return -ENOMEM;
	}

	sst->vtsv_enroll = false;
	/* Assign the pointer variables */
	sst->ll_db.low_latency = &ll_threshold;
	sst->ll_db.deep_buffer = &db_threshold;

	pr_debug("Default ll thres %lu db thres %lu\n", ll_threshold, db_threshold);

	snd_soc_dapm_new_controls(&platform->dapm, sst_dapm_widgets,
			ARRAY_SIZE(sst_dapm_widgets));
	snd_soc_dapm_add_routes(&platform->dapm, intercon,
			ARRAY_SIZE(intercon));
	snd_soc_dapm_new_widgets(platform->dapm.card);
	snd_soc_add_platform_controls(platform, sst_mrfld_controls,
			ARRAY_SIZE(sst_mrfld_controls));

	error = sysfs_create_group(&platform->dev->kobj, &attr_group);
	if (error)
		pr_err("failed to create sysfs files  %d\n", error);

	return error;
}
