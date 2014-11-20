/*
 * BIOS auto-parser helper functions for HD-audio
 *
 * Copyright (c) 2012 Takashi Iwai <tiwai@suse.de>
 *
 * This driver is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __SOUND_HDA_AUTO_PARSER_H
#define __SOUND_HDA_AUTO_PARSER_H

/*
 * Helper for automatic pin configuration
 */

enum {
	AUTO_PIN_MIC,
	AUTO_PIN_LINE_IN,
	AUTO_PIN_CD,
	AUTO_PIN_AUX,
	AUTO_PIN_LAST
};

enum {
	AUTO_PIN_LINE_OUT,
	AUTO_PIN_SPEAKER_OUT,
	AUTO_PIN_HP_OUT
};

#define HDA_MAX_OUTS	5
#define HDA_MAX_NUM_INPUTS	16

/*
 * Multi-channel / digital-out info
 */

enum { HDA_FRONT, HDA_REAR, HDA_CLFE, HDA_SIDE }; /* index for dac_nidx */
enum { HDA_DIG_NONE, HDA_DIG_EXCLUSIVE, HDA_DIG_ANALOG_DUP }; /* dig_out_used */

struct hda_multi_out {
	int num_dacs;		/* # of DACs, must be more than 1 */
	const hda_nid_t *dac_nids;	/* DAC list */
	hda_nid_t hp_nid;	/* optional DAC for HP, 0 when not exists */
	hda_nid_t hp_out_nid[HDA_MAX_OUTS];	/* DACs for multiple HPs */
	hda_nid_t extra_out_nid[HDA_MAX_OUTS];	/* other (e.g. speaker) DACs */
	hda_nid_t dig_out_nid;	/* digital out audio widget */
	const hda_nid_t *slave_dig_outs;
	int max_channels;	/* currently supported analog channels */
	int dig_out_used;	/* current usage of digital out (HDA_DIG_XXX) */
	int no_share_stream;	/* don't share a stream with multiple pins */
	int share_spdif;	/* share SPDIF pin */
	/* PCM information for both analog and SPDIF DACs */
	unsigned int analog_rates;
	unsigned int analog_maxbps;
	u64 analog_formats;
	unsigned int spdif_rates;
	unsigned int spdif_maxbps;
	u64 spdif_formats;
};

int snd_hda_create_spdif_share_sw(struct hda_codec *codec,
				  struct hda_multi_out *mout);
/*
 * generic codec parser
 */
int snd_hda_parse_generic_codec(struct hda_codec *codec);
int snd_hda_parse_hdmi_codec(struct hda_codec *codec);


#define AUTO_CFG_MAX_OUTS	HDA_MAX_OUTS
#define AUTO_CFG_MAX_INS	8

struct auto_pin_cfg_item {
	hda_nid_t pin;
	int type;
	unsigned int is_headset_mic:1;
	unsigned int is_headphone_mic:1; /* Mic-only in headphone jack */
};

struct auto_pin_cfg;
const char *hda_get_autocfg_input_label(struct hda_codec *codec,
					const struct auto_pin_cfg *cfg,
					int input);
int snd_hda_get_pin_label(struct hda_codec *codec, hda_nid_t nid,
			  const struct auto_pin_cfg *cfg,
			  char *label, int maxlen, int *indexp);

enum {
	INPUT_PIN_ATTR_UNUSED,	/* pin not connected */
	INPUT_PIN_ATTR_INT,	/* internal mic/line-in */
	INPUT_PIN_ATTR_DOCK,	/* docking mic/line-in */
	INPUT_PIN_ATTR_NORMAL,	/* mic/line-in jack */
	INPUT_PIN_ATTR_REAR,	/* mic/line-in jack in rear */
	INPUT_PIN_ATTR_FRONT,	/* mic/line-in jack in front */
	INPUT_PIN_ATTR_LAST = INPUT_PIN_ATTR_FRONT,
};

int snd_hda_get_input_pin_attr(unsigned int def_conf);

struct auto_pin_cfg {
	int line_outs;
	/* sorted in the order of Front/Surr/CLFE/Side */
	hda_nid_t line_out_pins[AUTO_CFG_MAX_OUTS];
	int speaker_outs;
	hda_nid_t speaker_pins[AUTO_CFG_MAX_OUTS];
	int hp_outs;
	int line_out_type;	/* AUTO_PIN_XXX_OUT */
	hda_nid_t hp_pins[AUTO_CFG_MAX_OUTS];
	int num_inputs;
	struct auto_pin_cfg_item inputs[AUTO_CFG_MAX_INS];
	int dig_outs;
	hda_nid_t dig_out_pins[2];
	hda_nid_t dig_in_pin;
	hda_nid_t mono_out_pin;
	int dig_out_type[2]; /* HDA_PCM_TYPE_XXX */
	int dig_in_type; /* HDA_PCM_TYPE_XXX */
};

/* bit-flags for snd_hda_parse_pin_def_config() behavior */
#define HDA_PINCFG_NO_HP_FIXUP   (1 << 0) /* no HP-split */
#define HDA_PINCFG_NO_LO_FIXUP   (1 << 1) /* don't take other outs as LO */
#define HDA_PINCFG_HEADSET_MIC   (1 << 2) /* Try to find headset mic; mark seq number as 0xc to trigger */
#define HDA_PINCFG_HEADPHONE_MIC (1 << 3) /* Try to find headphone mic; mark seq number as 0xd to trigger */

int snd_hda_parse_pin_defcfg(struct hda_codec *codec,
			     struct auto_pin_cfg *cfg,
			     const hda_nid_t *ignore_nids,
			     unsigned int cond_flags);

/* older function */
#define snd_hda_parse_pin_def_config(codec, cfg, ignore) \
	snd_hda_parse_pin_defcfg(codec, cfg, ignore, 0)

static inline int auto_cfg_hp_outs(const struct auto_pin_cfg *cfg)
{
	return (cfg->line_out_type == AUTO_PIN_HP_OUT) ?
	       cfg->line_outs : cfg->hp_outs;
}
static inline const hda_nid_t *auto_cfg_hp_pins(const struct auto_pin_cfg *cfg)
{
	return (cfg->line_out_type == AUTO_PIN_HP_OUT) ?
	       cfg->line_out_pins : cfg->hp_pins;
}
static inline int auto_cfg_speaker_outs(const struct auto_pin_cfg *cfg)
{
	return (cfg->line_out_type == AUTO_PIN_SPEAKER_OUT) ?
	       cfg->line_outs : cfg->speaker_outs;
}
static inline const hda_nid_t *auto_cfg_speaker_pins(const struct auto_pin_cfg *cfg)
{
	return (cfg->line_out_type == AUTO_PIN_SPEAKER_OUT) ?
	       cfg->line_out_pins : cfg->speaker_pins;
}

/*
 * Fix-up pin default configurations and add default verbs
 */

struct hda_pintbl {
	hda_nid_t nid;
	u32 val;
};

struct hda_model_fixup {
	const int id;
	const char *name;
};

struct hda_fixup {
	int type;
	bool chained:1;		/* call the chained fixup(s) after this */
	bool chained_before:1;	/* call the chained fixup(s) before this */
	int chain_id;
	union {
		const struct hda_pintbl *pins;
		const struct hda_verb *verbs;
		void (*func)(struct hda_codec *codec,
			     const struct hda_fixup *fix,
			     int action);
	} v;
};

/* fixup types */
enum {
	HDA_FIXUP_INVALID,
	HDA_FIXUP_PINS,
	HDA_FIXUP_VERBS,
	HDA_FIXUP_FUNC,
	HDA_FIXUP_PINCTLS,
};

/* fixup action definitions */
enum {
	HDA_FIXUP_ACT_PRE_PROBE,
	HDA_FIXUP_ACT_PROBE,
	HDA_FIXUP_ACT_INIT,
	HDA_FIXUP_ACT_BUILD,
	HDA_FIXUP_ACT_FREE,
};

int snd_hda_add_verbs(struct hda_codec *codec, const struct hda_verb *list);
void snd_hda_apply_verbs(struct hda_codec *codec);
void snd_hda_apply_pincfgs(struct hda_codec *codec,
			   const struct hda_pintbl *cfg);
void snd_hda_apply_fixup(struct hda_codec *codec, int action);
void snd_hda_pick_fixup(struct hda_codec *codec,
			const struct hda_model_fixup *models,
			const struct snd_pci_quirk *quirk,
			const struct hda_fixup *fixlist);

/* helper macros to retrieve pin default-config values */
#define get_defcfg_connect(cfg) \
	((cfg & AC_DEFCFG_PORT_CONN) >> AC_DEFCFG_PORT_CONN_SHIFT)
#define get_defcfg_association(cfg) \
	((cfg & AC_DEFCFG_DEF_ASSOC) >> AC_DEFCFG_ASSOC_SHIFT)
#define get_defcfg_location(cfg) \
	((cfg & AC_DEFCFG_LOCATION) >> AC_DEFCFG_LOCATION_SHIFT)
#define get_defcfg_sequence(cfg) \
	(cfg & AC_DEFCFG_SEQUENCE)
#define get_defcfg_device(cfg) \
	((cfg & AC_DEFCFG_DEVICE) >> AC_DEFCFG_DEVICE_SHIFT)
#define get_defcfg_misc(cfg) \
	((cfg & AC_DEFCFG_MISC) >> AC_DEFCFG_MISC_SHIFT)

/* amp values */
#define AMP_IN_MUTE(idx)	(0x7080 | ((idx)<<8))
#define AMP_IN_UNMUTE(idx)	(0x7000 | ((idx)<<8))
#define AMP_OUT_MUTE		0xb080
#define AMP_OUT_UNMUTE		0xb000
#define AMP_OUT_ZERO		0xb000
/* pinctl values */
#define PIN_IN			(AC_PINCTL_IN_EN)
#define PIN_VREFHIZ		(AC_PINCTL_IN_EN | AC_PINCTL_VREF_HIZ)
#define PIN_VREF50		(AC_PINCTL_IN_EN | AC_PINCTL_VREF_50)
#define PIN_VREFGRD		(AC_PINCTL_IN_EN | AC_PINCTL_VREF_GRD)
#define PIN_VREF80		(AC_PINCTL_IN_EN | AC_PINCTL_VREF_80)
#define PIN_VREF100		(AC_PINCTL_IN_EN | AC_PINCTL_VREF_100)
#define PIN_OUT			(AC_PINCTL_OUT_EN)
#define PIN_HP			(AC_PINCTL_OUT_EN | AC_PINCTL_HP_EN)
#define PIN_HP_AMP		(AC_PINCTL_HP_EN)

unsigned int snd_hda_get_default_vref(struct hda_codec *codec, hda_nid_t pin);
unsigned int snd_hda_correct_pin_ctl(struct hda_codec *codec,
				     hda_nid_t pin, unsigned int val);
int _snd_hda_set_pin_ctl(struct hda_codec *codec, hda_nid_t pin,
			 unsigned int val, bool cached);

/**
 * _snd_hda_set_pin_ctl - Set a pin-control value safely
 * @codec: the codec instance
 * @pin: the pin NID to set the control
 * @val: the pin-control value (AC_PINCTL_* bits)
 *
 * This function sets the pin-control value to the given pin, but
 * filters out the invalid pin-control bits when the pin has no such
 * capabilities.  For example, when PIN_HP is passed but the pin has no
 * HP-drive capability, the HP bit is omitted.
 *
 * The function doesn't check the input VREF capability bits, though.
 * Use snd_hda_get_default_vref() to guess the right value.
 * Also, this function is only for analog pins, not for HDMI pins.
 */
static inline int
snd_hda_set_pin_ctl(struct hda_codec *codec, hda_nid_t pin, unsigned int val)
{
	return _snd_hda_set_pin_ctl(codec, pin, val, false);
}

/**
 * snd_hda_set_pin_ctl_cache - Set a pin-control value safely
 * @codec: the codec instance
 * @pin: the pin NID to set the control
 * @val: the pin-control value (AC_PINCTL_* bits)
 *
 * Just like snd_hda_set_pin_ctl() but write to cache as well.
 */
static inline int
snd_hda_set_pin_ctl_cache(struct hda_codec *codec, hda_nid_t pin,
			  unsigned int val)
{
	return _snd_hda_set_pin_ctl(codec, pin, val, true);
}

int snd_hda_codec_get_pin_target(struct hda_codec *codec, hda_nid_t nid);
int snd_hda_codec_set_pin_target(struct hda_codec *codec, hda_nid_t nid,
				 unsigned int val);

/*
 * get widget capabilities
 */
static inline u32 get_wcaps(struct hda_codec *codec, hda_nid_t nid)
{
	if (nid < codec->start_nid ||
	    nid >= codec->start_nid + codec->num_nodes)
		return 0;
	return codec->wcaps[nid - codec->start_nid];
}

/* get the widget type from widget capability bits */
static inline int get_wcaps_type(unsigned int wcaps)
{
	if (!wcaps)
		return -1; /* invalid type */
	return (wcaps & AC_WCAP_TYPE) >> AC_WCAP_TYPE_SHIFT;
}

static inline unsigned int get_wcaps_channels(u32 wcaps)
{
	unsigned int chans;

	chans = (wcaps & AC_WCAP_CHAN_CNT_EXT) >> 13;
	chans = ((chans << 1) | 1) + 1;

	return chans;
}

static inline void snd_hda_override_wcaps(struct hda_codec *codec,
					  hda_nid_t nid, u32 val)
{
	if (nid >= codec->start_nid &&
	    nid < codec->start_nid + codec->num_nodes)
		codec->wcaps[nid - codec->start_nid] = val;
}

u32 query_amp_caps(struct hda_codec *codec, hda_nid_t nid, int direction);
int snd_hda_override_amp_caps(struct hda_codec *codec, hda_nid_t nid, int dir,
			      unsigned int caps);
u32 snd_hda_query_pin_caps(struct hda_codec *codec, hda_nid_t nid);
int snd_hda_override_pin_caps(struct hda_codec *codec, hda_nid_t nid,
			      unsigned int caps);

/* flags for hda_nid_item */
#define HDA_NID_ITEM_AMP	(1<<0)

struct hda_nid_item {
	struct snd_kcontrol *kctl;
	unsigned int index;
	hda_nid_t nid;
	unsigned short flags;
};

#endif /* __SOUND_HDA_AUTO_PARSER_H */
