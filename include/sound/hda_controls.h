/*
 * Universal Interface for Intel High Definition Audio Codec
 *
 * HDA Controls helper functions
 *
 * Copyright (c) 2014 Intel Corporation
 * Copyright (c) 2004 Takashi Iwai <tiwai@suse.de>
 *
 * Modified by: Lakshmi Vinnakota <lakshmi.n.vinnakota@intel.com>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 */

#ifndef __SOUND_HDA_CONTROLS_H
#define __SOUND_HDA_CONTROLS_H

#include <sound/control.h>
/* We abuse kcontrol_new.subdev field to pass the NID corresponding to
 * the given new control.  If id.subdev has a bit flag HDA_SUBDEV_NID_FLAG,
 * snd_hda_ctl_add() takes the lower-bit subdev value as a valid NID.
 *
 * Note that the subdevice field is cleared again before the real registration
 * in snd_hda_ctl_add(), so that this value won't appear in the outside.
 */
#define HDA_SUBDEV_NID_FLAG	(1U << 31)
#define HDA_SUBDEV_AMP_FLAG	(1U << 30)

/*
 * for mixer controls
 */
#define HDA_COMPOSE_AMP_VAL_OFS(nid, chs, idx, dir, ofs)		\
	((nid) | ((chs)<<16) | ((dir)<<18) | ((idx)<<19) | ((ofs)<<23))
#define HDA_AMP_VAL_MIN_MUTE (1<<29)
#define HDA_COMPOSE_AMP_VAL(nid, chs, idx, dir) \
	HDA_COMPOSE_AMP_VAL_OFS(nid, chs, idx, dir, 0)
/* mono volume with index (index=0,1,...) (channel=1,2) */
#define HDA_CODEC_VOLUME_MONO_IDX(xname, xcidx, nid, channel, xindex, dir, flags) \
	{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = xcidx,  \
	  .subdevice = HDA_SUBDEV_AMP_FLAG, \
	  .access = SNDRV_CTL_ELEM_ACCESS_READWRITE | \
			SNDRV_CTL_ELEM_ACCESS_TLV_READ | \
			SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK, \
	  .info = snd_hda_mixer_amp_volume_info, \
	  .get = snd_hda_mixer_amp_volume_get, \
	  .put = snd_hda_mixer_amp_volume_put, \
	  .tlv = { .c = snd_hda_mixer_amp_tlv },		\
	  .private_value = HDA_COMPOSE_AMP_VAL(nid, channel, xindex, dir) | flags }
/* stereo volume with index */
#define HDA_CODEC_VOLUME_IDX(xname, xcidx, nid, xindex, direction) \
	HDA_CODEC_VOLUME_MONO_IDX(xname, xcidx, nid, 3, xindex, direction, 0)
/* mono volume */
#define HDA_CODEC_VOLUME_MONO(xname, nid, channel, xindex, direction) \
	HDA_CODEC_VOLUME_MONO_IDX(xname, 0, nid, channel, xindex, direction, 0)
/* stereo volume */
#define HDA_CODEC_VOLUME(xname, nid, xindex, direction) \
	HDA_CODEC_VOLUME_MONO(xname, nid, 3, xindex, direction)
/* stereo volume with min=mute */
#define HDA_CODEC_VOLUME_MIN_MUTE(xname, nid, xindex, direction) \
	HDA_CODEC_VOLUME_MONO_IDX(xname, 0, nid, 3, xindex, direction, \
				  HDA_AMP_VAL_MIN_MUTE)
/* mono mute switch with index (index=0,1,...) (channel=1,2) */
#define HDA_CODEC_MUTE_MONO_IDX(xname, xcidx, nid, channel, xindex, direction) \
	{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = xcidx, \
	  .subdevice = HDA_SUBDEV_AMP_FLAG, \
	  .info = snd_hda_mixer_amp_switch_info, \
	  .get = snd_hda_mixer_amp_switch_get, \
	  .put = snd_hda_mixer_amp_switch_put, \
	  .private_value = HDA_COMPOSE_AMP_VAL(nid, channel, xindex, direction) }
/* stereo mute switch with index */
#define HDA_CODEC_MUTE_IDX(xname, xcidx, nid, xindex, direction) \
	HDA_CODEC_MUTE_MONO_IDX(xname, xcidx, nid, 3, xindex, direction)
/* mono mute switch */
#define HDA_CODEC_MUTE_MONO(xname, nid, channel, xindex, direction) \
	HDA_CODEC_MUTE_MONO_IDX(xname, 0, nid, channel, xindex, direction)
/* stereo mute switch */
#define HDA_CODEC_MUTE(xname, nid, xindex, direction) \
	HDA_CODEC_MUTE_MONO(xname, nid, 3, xindex, direction)
#ifdef CONFIG_SND_CORE_HDA_INPUT_BEEP
/* special beep mono mute switch with index (index=0,1,...) (channel=1,2) */
#define HDA_CODEC_MUTE_BEEP_MONO_IDX(xname, xcidx, nid, channel, xindex, direction) \
	{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = xcidx, \
	  .subdevice = HDA_SUBDEV_AMP_FLAG, \
	  .info = snd_hda_mixer_amp_switch_info, \
	  .get = snd_hda_mixer_amp_switch_get_beep, \
	  .put = snd_hda_mixer_amp_switch_put_beep, \
	  .private_value = HDA_COMPOSE_AMP_VAL(nid, channel, xindex, direction) }
#else
/* no digital beep - just the standard one */
#define HDA_CODEC_MUTE_BEEP_MONO_IDX(xname, xcidx, nid, ch, xidx, dir) \
	HDA_CODEC_MUTE_MONO_IDX(xname, xcidx, nid, ch, xidx, dir)
#endif /* CONFIG_SND_CORE_HDA_INPUT_BEEP */
/* special beep mono mute switch */
#define HDA_CODEC_MUTE_BEEP_MONO(xname, nid, channel, xindex, direction) \
	HDA_CODEC_MUTE_BEEP_MONO_IDX(xname, 0, nid, channel, xindex, direction)
/* special beep stereo mute switch */
#define HDA_CODEC_MUTE_BEEP(xname, nid, xindex, direction) \
	HDA_CODEC_MUTE_BEEP_MONO(xname, nid, 3, xindex, direction)

int snd_hda_mixer_amp_volume_info(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_info *uinfo);
int snd_hda_mixer_amp_volume_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol);
int snd_hda_mixer_amp_volume_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol);
int snd_hda_mixer_amp_tlv(struct snd_kcontrol *kcontrol, int op_flag,
			  unsigned int size, unsigned int __user *tlv);
int snd_hda_mixer_amp_switch_info(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_info *uinfo);
int snd_hda_mixer_amp_switch_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol);
int snd_hda_mixer_amp_switch_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol);
#ifdef CONFIG_SND_CORE_HDA_INPUT_BEEP
int snd_hda_mixer_amp_switch_get_beep(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol);
int snd_hda_mixer_amp_switch_put_beep(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol);
#endif
/* lowlevel accessor with caching; use carefully */
int snd_hda_codec_amp_read(struct hda_codec *codec, hda_nid_t nid, int ch,
			   int direction, int index);
int snd_hda_codec_amp_update(struct hda_codec *codec, hda_nid_t nid, int ch,
			     int direction, int idx, int mask, int val);
int snd_hda_codec_amp_stereo(struct hda_codec *codec, hda_nid_t nid,
			     int dir, int idx, int mask, int val);
int snd_hda_codec_amp_init(struct hda_codec *codec, hda_nid_t nid, int ch,
			   int direction, int idx, int mask, int val);
int snd_hda_codec_amp_init_stereo(struct hda_codec *codec, hda_nid_t nid,
				  int dir, int idx, int mask, int val);
void snd_hda_codec_resume_amp(struct hda_codec *codec);

void snd_hda_set_vmaster_tlv(struct hda_codec *codec, hda_nid_t nid, int dir,
			     unsigned int *tlv);
struct snd_kcontrol *snd_hda_find_mixer_ctl(struct hda_codec *codec,
					    const char *name);
int __snd_hda_add_vmaster(struct hda_codec *codec, char *name,
			  unsigned int *tlv, const char * const *slaves,
			  const char *suffix, bool init_slave_vol,
			  struct snd_kcontrol **ctl_ret);
#define snd_hda_add_vmaster(codec, name, tlv, slaves, suffix) \
	__snd_hda_add_vmaster(codec, name, tlv, slaves, suffix, true, NULL)
int snd_hda_codec_reset(struct hda_codec *codec);

enum {
	HDA_VMUTE_OFF,
	HDA_VMUTE_ON,
	HDA_VMUTE_FOLLOW_MASTER,
};

struct hda_vmaster_mute_hook {
	/* below two fields must be filled by the caller of
	 * snd_hda_add_vmaster_hook() beforehand
	 */
	struct snd_kcontrol *sw_kctl;
	void (*hook)(void *, int);
	/* below are initialized automatically */
	unsigned int mute_mode; /* HDA_VMUTE_XXX */
	struct hda_codec *codec;
};

int snd_hda_add_vmaster_hook(struct hda_codec *codec,
			     struct hda_vmaster_mute_hook *hook,
			     bool expose_enum_ctl);
void snd_hda_sync_vmaster_hook(struct hda_vmaster_mute_hook *hook);

/* amp value bits */
#define HDA_AMP_MUTE	0x80
#define HDA_AMP_UNMUTE	0x00
#define HDA_AMP_VOLMASK	0x7f

/* mono switch binding multiple inputs */
#define HDA_BIND_MUTE_MONO(xname, nid, channel, indices, direction) \
	{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = 0,  \
	  .info = snd_hda_mixer_amp_switch_info, \
	  .get = snd_hda_mixer_bind_switch_get, \
	  .put = snd_hda_mixer_bind_switch_put, \
	  .private_value = HDA_COMPOSE_AMP_VAL(nid, channel, indices, direction) }

/* stereo switch binding multiple inputs */
#define HDA_BIND_MUTE(xname, nid, indices, dir) \
	HDA_BIND_MUTE_MONO(xname, nid, 3, indices, dir)

int snd_hda_mixer_bind_switch_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol);
int snd_hda_mixer_bind_switch_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol);

/* more generic bound controls */
struct hda_ctl_ops {
	snd_kcontrol_info_t *info;
	snd_kcontrol_get_t *get;
	snd_kcontrol_put_t *put;
	snd_kcontrol_tlv_rw_t *tlv;
};

extern struct hda_ctl_ops snd_hda_bind_vol;	/* for bind-volume with TLV */
extern struct hda_ctl_ops snd_hda_bind_sw;	/* for bind-switch */

struct hda_bind_ctls {
	struct hda_ctl_ops *ops;
	unsigned long values[];
};

int snd_hda_mixer_bind_ctls_info(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_info *uinfo);
int snd_hda_mixer_bind_ctls_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol);
int snd_hda_mixer_bind_ctls_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol);
int snd_hda_mixer_bind_tlv(struct snd_kcontrol *kcontrol, int op_flag,
			   unsigned int size, unsigned int __user *tlv);

#define HDA_BIND_VOL(xname, bindrec) \
	{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
	  .name = xname, \
	  .access = SNDRV_CTL_ELEM_ACCESS_READWRITE |\
			  SNDRV_CTL_ELEM_ACCESS_TLV_READ |\
			  SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK,\
	  .info = snd_hda_mixer_bind_ctls_info,\
	  .get =  snd_hda_mixer_bind_ctls_get,\
	  .put = snd_hda_mixer_bind_ctls_put,\
	  .tlv = { .c = snd_hda_mixer_bind_tlv },\
	  .private_value = (long) (bindrec) }
#define HDA_BIND_SW(xname, bindrec) \
	{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER,\
	  .name = xname, \
	  .info = snd_hda_mixer_bind_ctls_info,\
	  .get =  snd_hda_mixer_bind_ctls_get,\
	  .put = snd_hda_mixer_bind_ctls_put,\
	  .private_value = (long) (bindrec) }

/*
 * SPDIF I/O
 */
int snd_hda_create_dig_out_ctls(struct hda_codec *codec,
				hda_nid_t associated_nid,
				hda_nid_t cvt_nid, int type);
#define snd_hda_create_spdif_out_ctls(codec, anid, cnid) \
	snd_hda_create_dig_out_ctls(codec, anid, cnid, HDA_PCM_TYPE_SPDIF)
int snd_hda_create_spdif_in_ctls(struct hda_codec *codec, hda_nid_t nid);

/*
 * input MUX helper
 */
struct hda_input_mux_item {
	char label[32];
	unsigned int index;
};
struct hda_input_mux {
	unsigned int num_items;
	struct hda_input_mux_item items[HDA_MAX_NUM_INPUTS];
};

int snd_hda_input_mux_info(const struct hda_input_mux *imux,
			   struct snd_ctl_elem_info *uinfo);
int snd_hda_input_mux_put(struct hda_codec *codec,
			  const struct hda_input_mux *imux,
			  struct snd_ctl_elem_value *ucontrol, hda_nid_t nid,
			  unsigned int *cur_val);
int snd_hda_add_imux_item(struct hda_input_mux *imux, const char *label,
			  int index, int *type_index_ret);

/*
 * Channel mode helper
 */
struct hda_channel_mode {
	int channels;
	const struct hda_verb *sequence;
};

int snd_hda_ch_mode_info(struct hda_codec *codec,
			 struct snd_ctl_elem_info *uinfo,
			 const struct hda_channel_mode *chmode,
			 int num_chmodes);
int snd_hda_ch_mode_get(struct hda_codec *codec,
			struct snd_ctl_elem_value *ucontrol,
			const struct hda_channel_mode *chmode,
			int num_chmodes,
			int max_channels);
int snd_hda_ch_mode_put(struct hda_codec *codec,
			struct snd_ctl_elem_value *ucontrol,
			const struct hda_channel_mode *chmode,
			int num_chmodes,
			int *max_channelsp);

/*
 * Misc
 */
int snd_hda_check_board_config(struct hda_codec *codec, int num_configs,
			       const char * const *modelnames,
			       const struct snd_pci_quirk *pci_list);
int snd_hda_check_board_codec_sid_config(struct hda_codec *codec,
				int num_configs, const char * const *models,
				const struct snd_pci_quirk *tbl);
int snd_hda_add_new_ctls(struct hda_codec *codec,
			 const struct snd_kcontrol_new *knew);

int snd_hda_ctl_add(struct hda_codec *codec, hda_nid_t nid,
		    struct snd_kcontrol *kctl);
int snd_hda_add_nid(struct hda_codec *codec, struct snd_kcontrol *kctl,
		    unsigned int index, hda_nid_t nid);
void snd_hda_ctls_clear(struct hda_codec *codec);

/*
 * AMP control callbacks
 */
/* retrieve parameters from private_value */
#define get_amp_nid_(pv)	((pv) & 0xffff)
#define get_amp_nid(kc)		get_amp_nid_((kc)->private_value)
#define get_amp_channels(kc)	(((kc)->private_value >> 16) & 0x3)
#define get_amp_direction_(pv)	(((pv) >> 18) & 0x1)
#define get_amp_direction(kc)	get_amp_direction_((kc)->private_value)
#define get_amp_index_(pv)	(((pv) >> 19) & 0xf)
#define get_amp_index(kc)	get_amp_index_((kc)->private_value)
#define get_amp_offset(kc)	(((kc)->private_value >> 23) & 0x3f)
#define get_amp_min_mute(kc)	(((kc)->private_value >> 29) & 0x1)

/*
 * enum control helper
 */
int snd_hda_enum_helper_info(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_info *uinfo,
			     int num_entries, const char * const *texts);
#define snd_hda_enum_bool_helper_info(kcontrol, uinfo) \
	snd_hda_enum_helper_info(kcontrol, uinfo, 0, NULL)


#endif /* __SOUND_HDA_CONTROLS_H */
