/*
 * Universal Interface for Intel High Definition Audio Codec
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

#ifndef __SOUND_HDA_CODEC_H
#define __SOUND_HDA_CODEC_H

#include <sound/info.h>
#include <sound/control.h>
#include <sound/pcm.h>
#include <sound/hwdep.h>
#include <sound/hda_verbs.h>

/*
 * generic arrays
 */
struct snd_array {
	unsigned int used;
	unsigned int alloced;
	unsigned int elem_size;
	unsigned int alloc_align;
	void *list;
};

void *snd_array_new(struct snd_array *array);
void snd_array_free(struct snd_array *array);
static inline void snd_array_init(struct snd_array *array, unsigned int size,
				  unsigned int align)
{
	array->elem_size = size;
	array->alloc_align = align;
}

static inline void *snd_array_elem(struct snd_array *array, unsigned int idx)
{
	return array->list + idx * array->elem_size;
}

static inline unsigned int snd_array_index(struct snd_array *array, void *ptr)
{
	return (unsigned long)(ptr - array->list) / array->elem_size;
}

/*
 * Structures
 */

struct hda_beep;
struct hda_codec;

/* NID type */
typedef u16 hda_nid_t;

/* record for amp information cache */
struct hda_cache_head {
	u32 key:31;		/* hash key */
	u32 dirty:1;
	u16 val;		/* assigned value */
	u16 next;
};

struct hda_amp_info {
	struct hda_cache_head head;
	u32 amp_caps;		/* amp capabilities */
	u16 vol[2];		/* current volume & mute */
};

struct hda_cache_rec {
	u16 hash[64];			/* hash table for index */
	struct snd_array buf;		/* record entries */
};

/* PCM types */
enum {
	HDA_PCM_TYPE_AUDIO,
	HDA_PCM_TYPE_SPDIF,
	HDA_PCM_TYPE_HDMI,
	HDA_PCM_TYPE_MODEM,
	HDA_PCM_NTYPES
};

/*
 * audio-converter setup caches
 */
struct hda_cvt_setup {
	hda_nid_t nid;
	u8 stream_tag;
	u8 channel_id;
	u16 format_id;
	unsigned char active;	/* cvt is currently used */
	unsigned char dirty;	/* setups should be cleared */
};

/**
 * struct hda_codec - Updated generic hda codec structure
 *			retaining common info
 *
 * @card - card pointer used for creating mixer controls
 * @exec_cmd - hook for sending hda codec commands on the codec link
 * @quirk_lookup - hook for pci quirk lookup
 * @dev - device pointer for the hda codec device
 *
 **/
struct hda_codec {
	struct snd_card *card;
	int (*exec_cmd)(struct hda_codec *codec,
			unsigned int cmd, int flags, unsigned int *res);
	const struct snd_pci_quirk * (*quirk_lookup)(struct hda_codec *codec,
			const struct snd_pci_quirk *list);
	struct device *dev;

	unsigned int addr;	/* codec addr*/

	hda_nid_t afg;	/* AFG node id */
	hda_nid_t mfg;	/* MFG node id */

	/* ids */
	u8 afg_function_id;
	u8 mfg_function_id;
	u8 afg_unsol;
	u8 mfg_unsol;
	u32 vendor_id;
	u32 subsystem_id;
	u32 revision_id;

	struct module *owner;
	int (*parser)(struct hda_codec *codec);
	const char *vendor_name;	/* codec vendor name */
	const char *chip_name;		/* codec chip name */
	const char *modelname;	/* model name for preset */

	/* codec specific info */
	void *spec;

	/* beep device */
	struct hda_beep *beep;
	unsigned int beep_mode;

	/* widget capabilities cache */
	unsigned int num_nodes;
	hda_nid_t start_nid;
	u32 *wcaps;

	struct snd_array mixers;	/* list of assigned mixer elements */
	struct snd_array nids;		/* list of mapped mixer elements */

	struct hda_cache_rec amp_cache;	/* cache for amp access */
	struct hda_cache_rec cmd_cache;	/* cache for other commands */

	struct list_head conn_list;	/* linked-list of connection-list */

	struct mutex cmd_mutex;
	struct mutex spdif_mutex;
	struct mutex control_mutex;
	struct mutex hash_mutex;
	struct snd_array spdif_out;
	unsigned int spdif_in_enable;	/* SPDIF input enable? */
	const hda_nid_t *slave_dig_outs; /* optional digital out slave widgets */
	struct snd_array init_pins;	/* initial (BIOS) pin configurations */
	struct snd_array driver_pins;	/* pin configs set by codec parser */
	struct snd_array cvt_setups;	/* audio convert setups */

	struct mutex user_mutex;
#ifdef CONFIG_SND_HDA_RECONFIG
	struct snd_array init_verbs;	/* additional init verbs */
	struct snd_array hints;		/* additional hints */
	struct snd_array user_pins;	/* default pin configs to override */
#endif

#ifdef CONFIG_SND_CORE_HDA_HWDEP
	struct snd_hwdep *hwdep;	/* assigned hwdep device */
#endif

	/* misc flags */
	unsigned int spdif_status_reset:1; /* needs to toggle SPDIF for each
					     * status change
					     * (e.g. Realtek codecs)
					     */
	unsigned int pin_amp_workaround:1; /* pin out-amp takes index
					    * (e.g. Conexant codecs)
					    */
	unsigned int single_adc_amp:1; /* adc in-amp takes no index
					* (e.g. CX20549 codec)
					*/
	unsigned int no_sticky_stream:1; /* no sticky-PCM stream assignment */
	unsigned int pins_shutup:1;	/* pins are shut up */
	unsigned int no_trigger_sense:1; /* don't trigger at pin-sensing */
	unsigned int no_jack_detect:1;	/* Machine has no jack-detection */
	unsigned int inv_eapd:1; /* broken h/w: inverted EAPD control */
	unsigned int inv_jack_detect:1;	/* broken h/w: inverted detection bit */
	unsigned int pcm_format_first:1; /* PCM format must be set first */
	unsigned int epss:1;		/* supporting EPSS? */
	unsigned int cached_write:1;	/* write only to caches */
	unsigned int dp_mst:1; /* support DP1.2 Multi-stream transport */
	unsigned int dump_coef:1; /* dump processing coefs in codec proc file */

	/* filter the requested power state per nid */
	unsigned int (*power_filter)(struct hda_codec *codec, hda_nid_t nid,
				     unsigned int power_state);

	/* codec-specific additional proc output */
	void (*proc_widget_hook)(struct snd_info_buffer *buffer,
				 struct hda_codec *codec, hda_nid_t nid);

	/* jack detection */
	struct snd_array jacktbl;
	unsigned long jackpoll_interval; /* In jiffies. Zero means no poll, rely on unsol events */
	struct delayed_work jackpoll_work;

#ifdef CONFIG_SND_HDA_INPUT_JACK
	/* jack detection */
	struct snd_array jacks;
#endif

	int depop_delay; /* depop delay in ms, -1 for default delay time */

	/* fix-up list */
	int fixup_id;
	const struct hda_fixup *fixup_list;
	const char *fixup_name;

	/* additional init verbs */
	struct snd_array verbs;
};

/* direction */
enum {
	HDA_INPUT, HDA_OUTPUT
};

/* snd_hda_codec_read/write optional flags */
#define HDA_RW_NO_RESPONSE_FALLBACK	(1 << 0)

/*
 * constructors
 */
int snd_hda_codec_alloc(struct hda_codec **codecp);
void snd_hda_codec_free(struct hda_codec *codec);
int snd_hda_codec_init(unsigned int codec_addr, struct hda_codec *codec);
int snd_hda_codec_reset(struct hda_codec *codec);
int snd_hda_codec_configure(struct hda_codec *codec);

/*
 * Codec Access functions
 */
unsigned int snd_hda_codec_read(struct hda_codec *codec, hda_nid_t nid,
				int flags,
				unsigned int verb, unsigned int parm);
int snd_hda_codec_write(struct hda_codec *codec, hda_nid_t nid, int flags,
			unsigned int verb, unsigned int parm);
#define snd_hda_param_read(codec, nid, param) \
	snd_hda_codec_read(codec, nid, 0, AC_VERB_PARAMETERS, param)

struct hda_verb {
	hda_nid_t nid;
	u32 verb;
	u32 param;
};
void snd_hda_sequence_write(struct hda_codec *codec,
			    const struct hda_verb *seq);
/*
 *  Cache Helpers
 */
int snd_hda_codec_write_cache(struct hda_codec *codec, hda_nid_t nid,
			      int flags, unsigned int verb, unsigned int parm);
void snd_hda_sequence_write_cache(struct hda_codec *codec,
				  const struct hda_verb *seq);
int snd_hda_codec_update_cache(struct hda_codec *codec, hda_nid_t nid,
			      int flags, unsigned int verb, unsigned int parm);
void snd_hda_codec_resume_cache(struct hda_codec *codec);
/* both for cmd & amp caches */
void snd_hda_codec_flush_cache(struct hda_codec *codec);

/*
 * Codec Parse helpers
 */
int snd_hda_get_sub_nodes(struct hda_codec *codec, hda_nid_t nid,
			  hda_nid_t *start_id);
int snd_hda_get_connections(struct hda_codec *codec, hda_nid_t nid,
			    hda_nid_t *conn_list, int max_conns);
static inline int
snd_hda_get_num_conns(struct hda_codec *codec, hda_nid_t nid)
{
	return snd_hda_get_connections(codec, nid, NULL, 0);
}
int snd_hda_get_num_raw_conns(struct hda_codec *codec, hda_nid_t nid);
int snd_hda_get_raw_connections(struct hda_codec *codec, hda_nid_t nid,
			    hda_nid_t *conn_list, int max_conns);
int snd_hda_get_conn_list(struct hda_codec *codec, hda_nid_t nid,
			  const hda_nid_t **listp);
int snd_hda_override_conn_list(struct hda_codec *codec, hda_nid_t nid, int nums,
			  const hda_nid_t *list);
int snd_hda_get_conn_index(struct hda_codec *codec, hda_nid_t mux,
			   hda_nid_t nid, int recursive);
int snd_hda_get_devices(struct hda_codec *codec, hda_nid_t nid,
			u8 *dev_list, int max_devices);

/* the struct for codec->pin_configs */
struct hda_pincfg {
	hda_nid_t nid;
	unsigned char ctrl;	/* original pin control value */
	unsigned char target;	/* target pin control value */
	unsigned int cfg;	/* default configuration */
};

unsigned int snd_hda_codec_get_pincfg(struct hda_codec *codec, hda_nid_t nid);
int snd_hda_codec_set_pincfg(struct hda_codec *codec, hda_nid_t nid,
			     unsigned int cfg);
int snd_hda_add_pincfg(struct hda_codec *codec, struct snd_array *list,
		       hda_nid_t nid, unsigned int cfg); /* for hwdep */
void snd_hda_shutup_pins(struct hda_codec *codec);

/* SPDIF controls */
struct hda_spdif_out {
	hda_nid_t nid;		/* Converter nid values relate to */
	unsigned int status;	/* IEC958 status bits */
	unsigned short ctls;	/* SPDIF control bits */
};
struct hda_spdif_out *snd_hda_spdif_out_of_nid(struct hda_codec *codec,
					       hda_nid_t nid);
void snd_hda_spdif_ctls_unassign(struct hda_codec *codec, int idx);
void snd_hda_spdif_ctls_assign(struct hda_codec *codec, int idx, hda_nid_t nid);


/*
 * PCM related helpers
 */
void snd_hda_codec_setup_stream(struct hda_codec *codec, hda_nid_t nid,
				u32 stream_tag,
				int channel_id, int format);
void __snd_hda_codec_cleanup_stream(struct hda_codec *codec, hda_nid_t nid,
				    int do_now);
#define snd_hda_codec_cleanup_stream(codec, nid) \
	__snd_hda_codec_cleanup_stream(codec, nid, 0)
unsigned int snd_hda_calc_stream_format(unsigned int rate,
					unsigned int channels,
					unsigned int format,
					unsigned int maxbps,
					unsigned short spdif_ctls);
int snd_hda_is_supported_format(struct hda_codec *codec, hda_nid_t nid,
				unsigned int format);
int snd_hda_query_supported_pcm(struct hda_codec *codec, hda_nid_t nid,
				u32 *ratesp, u64 *formatsp, unsigned int *bpsp);
struct hda_cvt_setup *
snd_hda_get_cvt_setup(struct hda_codec *codec, hda_nid_t nid);
void snd_hda_restore_shutup_pins(struct hda_codec *codec);

/*
 * Misc
 */

int snd_hda_lock_devices(struct hda_codec *codec);
void snd_hda_unlock_devices(struct hda_codec *codec);

/*
 * codec power helpers
 */
unsigned int snd_hda_set_power_state(struct hda_codec *codec, hda_nid_t nid,
					unsigned int power_state);
/* check whether the actual power state matches with the target state */
bool snd_hda_check_power_state(struct hda_codec *codec, hda_nid_t nid,
			  unsigned int target_state);

unsigned int snd_hda_codec_eapd_power_filter(struct hda_codec *codec,
					     hda_nid_t nid,
					     unsigned int power_state);
void snd_hda_codec_set_power_to_all(struct hda_codec *codec,  hda_nid_t fg,
				    unsigned int power_state);
void snd_hda_sync_power_up_states(struct hda_codec *codec);
void snd_hda_schedule_power_save(struct hda_codec *codec);

struct hda_amp_list {
	hda_nid_t nid;
	unsigned char dir;
	unsigned char idx;
};

struct hda_loopback_check {
	const struct hda_amp_list *amplist;
	int power_on;
};

int snd_hda_check_amp_list_power(struct hda_codec *codec,
				 struct hda_loopback_check *check,
				 hda_nid_t nid);


/*
 * get widget information
 */
const char *snd_hda_get_jack_connectivity(u32 cfg);
const char *snd_hda_get_jack_type(u32 cfg);
const char *snd_hda_get_jack_location(u32 cfg);

/*
 * power saving
 */
void snd_hda_power_save(struct hda_codec *codec, int delta, bool d3wait);

/**
 * snd_hda_power_up - Power-up the codec
 * @codec: HD-audio codec
 *
 * Increment the power-up counter and power up the hardware really when
 * not turned on yet.
 */
static inline void snd_hda_power_up(struct hda_codec *codec)
{
	snd_hda_power_save(codec, 1, false);
}

/**
 * snd_hda_power_up_d3wait - Power-up the codec after waiting for any pending
 *   D3 transition to complete.  This differs from snd_hda_power_up() when
 *   power_transition == -1.  snd_hda_power_up sees this case as a nop,
 *   snd_hda_power_up_d3wait waits for the D3 transition to complete then powers
 *   back up.
 * @codec: HD-audio codec
 *
 * Cancel any power down operation hapenning on the work queue, then power up.
 */
static inline void snd_hda_power_up_d3wait(struct hda_codec *codec)
{
	snd_hda_power_save(codec, 1, true);
}

/**
 * snd_hda_power_down - Power-down the codec
 * @codec: HD-audio codec
 *
 * Decrement the power-up counter and schedules the power-off work if
 * the counter rearches to zero.
 */
static inline void snd_hda_power_down(struct hda_codec *codec)
{
	snd_hda_power_save(codec, -1, false);
}

/**
 * snd_hda_power_sync - Synchronize the power-save status
 * @codec: HD-audio codec
 *
 * Synchronize the actual power state with the power account;
 * called when power_save parameter is changed
 */
static inline void snd_hda_power_sync(struct hda_codec *codec)
{
	snd_hda_power_save(codec, 0, false);
}

/*
 * generic proc interface
 */
#ifdef CONFIG_PROC_FS
int snd_hda_codec_proc_new(struct hda_codec *codec);
#else
static inline int snd_hda_codec_proc_new(struct hda_codec *codec) { return 0; }
#endif

#define SND_PRINT_BITS_ADVISED_BUFSIZE	16
void snd_print_pcm_bits(int pcm, char *buf, int buflen);

/*
 * hwdep interface
 */
#ifdef CONFIG_SND_CORE_HDA_HWDEP
int snd_hda_create_hwdep(struct hda_codec *codec);
#else
static inline int snd_hda_create_hwdep(struct hda_codec *codec) { return 0; }
#endif

void snd_hda_sysfs_init(struct hda_codec *codec);
void snd_hda_sysfs_clear(struct hda_codec *codec);

extern const struct attribute_group *snd_hda_dev_attr_groups[];

#ifdef CONFIG_SND_HDA_RECONFIG
const char *snd_hda_get_hint(struct hda_codec *codec, const char *key);
int snd_hda_get_bool_hint(struct hda_codec *codec, const char *key);
int snd_hda_get_int_hint(struct hda_codec *codec, const char *key, int *valp);
#else
static inline
const char *snd_hda_get_hint(struct hda_codec *codec, const char *key)
{
	return NULL;
}

static inline
int snd_hda_get_bool_hint(struct hda_codec *codec, const char *key)
{
	return -ENOENT;
}

static inline
int snd_hda_get_int_hint(struct hda_codec *codec, const char *key, int *valp)
{
	return -ENOENT;
}
#endif

/*
 * Debug macros
 */
#define codec_err(codec, fmt, args...) dev_err((codec)->dev, fmt, ##args)
#define codec_warn(codec, fmt, args...) dev_warn((codec)->dev, fmt, ##args)
#define codec_info(codec, fmt, args...) dev_info((codec)->dev, fmt, ##args)
#define codec_dbg(codec, fmt, args...) dev_dbg((codec)->dev, fmt, ##args)
#endif /* __SOUND_HDA_CODEC_H */
