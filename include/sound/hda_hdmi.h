/*
 * Universal Interface for Intel High Definition Audio Codec
 *
 * HDMI Codec helper functions
 *
 * Copyright (c) 2014 Intel Corporation
 * Copyright (c) 2004 Takashi Iwai <tiwai@suse.de>
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

#ifndef __SOUND_HDA_HDMI_H
#define __SOUND_HDA_HDMI_H

/*
 * CEA Short Audio Descriptor data
 */
struct cea_sad {
	int	channels;
	int	format;		/* (format == 0) indicates invalid SAD */
	int	rates;
	int	sample_bits;	/* for LPCM */
	int	max_bitrate;	/* for AC3...ATRAC */
	int	profile;	/* for WMAPRO */
};

#define ELD_FIXED_BYTES	20
#define ELD_MAX_SIZE    256
#define ELD_MAX_MNL	16
#define ELD_MAX_SAD	16

/*
 * ELD: EDID Like Data
 */
struct parsed_hdmi_eld {
	/*
	 * all fields will be cleared before updating ELD
	 */
	int	baseline_len;
	int	eld_ver;
	int	cea_edid_ver;
	char	monitor_name[ELD_MAX_MNL + 1];
	int	manufacture_id;
	int	product_id;
	u64	port_id;
	int	support_hdcp;
	int	support_ai;
	int	conn_type;
	int	aud_synch_delay;
	int	spk_alloc;
	int	sad_count;
	struct cea_sad sad[ELD_MAX_SAD];
};

struct hdmi_eld {
	bool	monitor_present;
	bool	eld_valid;
	int	eld_size;
	char    eld_buffer[ELD_MAX_SIZE];
	struct parsed_hdmi_eld info;
};

int snd_hdmi_get_eld_size(struct hda_codec *codec, hda_nid_t nid);
int snd_hdmi_get_eld(struct hda_codec *codec, hda_nid_t nid,
		     unsigned char *buf, int *eld_size);
int snd_hdmi_parse_eld(struct parsed_hdmi_eld *e,
		       const unsigned char *buf, int size);
void snd_hdmi_show_eld(struct parsed_hdmi_eld *e);

int snd_hdmi_get_eld_ati(struct hda_codec *codec, hda_nid_t nid,
			 unsigned char *buf, int *eld_size,
			 bool rev3_or_later);

#ifdef CONFIG_PROC_FS
void snd_hdmi_print_eld_info(struct hdmi_eld *eld,
			     struct snd_info_buffer *buffer);
void snd_hdmi_write_eld_info(struct hdmi_eld *eld,
			     struct snd_info_buffer *buffer);
#endif

#define SND_PRINT_CHANNEL_ALLOCATION_ADVISED_BUFSIZE 80
void snd_print_channel_allocation(int spk_alloc, char *buf, int buflen);

#endif /* __SOUND_HDA_HDMI_H */
