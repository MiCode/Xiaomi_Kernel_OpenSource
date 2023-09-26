/*
 * wm_adsp.h  --  Wolfson ADSP support
 *
 * Copyright 2012 Wolfson Microelectronics plc
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __WM_ADSP_H
#define __WM_ADSP_H

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/compress_driver.h>

#include "wmfw.h"

/* Return values for wm_adsp_compr_handle_irq */
#define WM_ADSP_COMPR_OK                 0
#define WM_ADSP_COMPR_VOICE_TRIGGER      1
#define WM_ADSP_MAX_CHANNEL_PER_DSP      2

#define WM_ADSP2_REGION_0 BIT(0)
#define WM_ADSP2_REGION_1 BIT(1)
#define WM_ADSP2_REGION_2 BIT(2)
#define WM_ADSP2_REGION_3 BIT(3)
#define WM_ADSP2_REGION_4 BIT(4)
#define WM_ADSP2_REGION_5 BIT(5)
#define WM_ADSP2_REGION_6 BIT(6)
#define WM_ADSP2_REGION_7 BIT(7)
#define WM_ADSP2_REGION_8 BIT(8)
#define WM_ADSP2_REGION_9 BIT(9)
#define WM_ADSP2_REGION_1_9 (WM_ADSP2_REGION_1 | \
		WM_ADSP2_REGION_2 | WM_ADSP2_REGION_3 | \
		WM_ADSP2_REGION_4 | WM_ADSP2_REGION_5 | \
		WM_ADSP2_REGION_6 | WM_ADSP2_REGION_7 | \
		WM_ADSP2_REGION_8 | WM_ADSP2_REGION_9)
#define WM_ADSP2_REGION_ALL (WM_ADSP2_REGION_0 | WM_ADSP2_REGION_1_9)

struct wm_adsp_region {
	int type;
	unsigned int base;
};

struct wm_adsp_alg_region {
	struct list_head list;
	unsigned int alg;
	int type;
	unsigned int base;
};

struct wm_adsp_compr;
struct wm_adsp_compr_buf;

struct wm_adsp_buffer_region_def {
	unsigned int mem_type;
	unsigned int base_offset;
	unsigned int size_offset;
};

struct wm_adsp_fw_caps {
	u32 id;
	struct snd_codec_desc desc;
	int num_regions;
	struct wm_adsp_buffer_region_def *region_defs;
};

struct wm_adsp_fw_defs {
	const char *file;
	const char *binfile;
	bool fullname;
	int compr_direction;
	int num_caps;
	struct wm_adsp_fw_caps *caps;
	bool voice_trigger;
};

struct wm_adsp {
	const char *part;
	const char *name;
	const char *fwf_name;
	int rev;
	int num;
	int type;
	struct device *dev;
	struct regmap *regmap;
	struct snd_soc_codec *codec;

	int base;
	int base_sysinfo;
	int sysclk_reg;
	int sysclk_mask;
	int sysclk_shift;

	struct list_head alg_regions;

	unsigned int fw_id;
	unsigned int fw_id_version;
	unsigned int fw_vendor_id;

	const struct wm_adsp_region *mem;
	int num_mems;

	int fw;
	int fw_ver;

	bool no_preloader;
	bool preloaded;
	bool booted;
	bool running;
	bool tuning_has_prefix;
	bool fatal_error;

	int num_firmwares;
	struct wm_adsp_fw_defs *firmwares;
	struct snd_kcontrol_new fw_ctrl;
	struct soc_enum fw_enum;

	struct list_head ctl_list;

	struct work_struct boot_work;

	int buf_num;
	struct wm_adsp_compr *compr[WM_ADSP_MAX_CHANNEL_PER_DSP];
	struct wm_adsp_compr_buf *buffer[WM_ADSP_MAX_CHANNEL_PER_DSP];

	struct mutex pwr_lock;

	unsigned int lock_regions;

	unsigned int n_rx_channels;
	unsigned int n_tx_channels;

	struct mutex *rate_lock;
	u8 *rx_rate_cache;
	u8 *tx_rate_cache;

#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_root;
	char *wmfw_file_name;
	char *bin_file_name;
#endif
	unsigned int data_word_mask;
	int data_word_size;

	void (*fwevent_cb)(struct wm_adsp *dsp, int eventid);
};

struct wm_adsp_compr {
	struct wm_adsp *dsp;
	struct wm_adsp_compr_buf *buf;

	struct snd_compr_stream *stream;
	struct snd_compressed_buffer size;

	u32 *raw_buf;
	unsigned int copied_total;

	unsigned int sample_rate;
	bool freed;
};

#define WM_ADSP_PRELOADER(wname, num, event_fn) \
{	.id = snd_soc_dapm_supply, .name = wname " Preloader", \
	.reg = SND_SOC_NOPM, .shift = num, .event = event_fn, \
	.event_flags = SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD, \
	.subseq = 100, /* Ensure we run after SYSCLK supply widget */ }

#define WM_ADSP1(wname, num) \
	SND_SOC_DAPM_PGA_E(wname, SND_SOC_NOPM, num, 0, NULL, 0, \
		wm_adsp1_event, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD)

#define WM_ADSP2_PRELOAD_SWITCH(wname, num) \
	SOC_SINGLE_EXT(wname " Preload Switch", SND_SOC_NOPM, num, 1, 0, \
		wm_adsp2_preloader_get, wm_adsp2_preloader_put)

#define WM_ADSP2(wname, num, event_fn) \
	(SND_SOC_DAPM_SPK(wname " Preload", NULL), \
	WM_ADSP_PRELOADER(wname, num, event_fn),\
{	.id = snd_soc_dapm_out_drv, .name = wname, \
	.reg = SND_SOC_NOPM, .shift = num, .event = wm_adsp2_event, \
	.event_flags = SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD })

#define WM_HALO(wname, num, event_fn) \
	(SND_SOC_DAPM_SPK(wname " Preload", NULL), \
	WM_ADSP_PRELOADER(wname, num, event_fn),\
{	.id = snd_soc_dapm_out_drv, .name = wname, \
	.reg = SND_SOC_NOPM, .shift = num, .event = wm_halo_event, \
	.event_flags = SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD })

#define WM_ADSP_FW_CONTROL(dspname, num) \
	SOC_ENUM_EXT(dspname " Firmware", wm_adsp_fw_enum[num], \
		     wm_adsp_fw_get, wm_adsp_fw_put)

extern const struct soc_enum wm_adsp_fw_enum[];

int wm_adsp1_init(struct wm_adsp *dsp);
int wm_adsp2_init(struct wm_adsp *dsp);
void wm_adsp2_remove(struct wm_adsp *dsp);
int wm_adsp2_codec_probe(struct wm_adsp *dsp, struct snd_soc_codec *codec);
int wm_adsp2_codec_remove(struct wm_adsp *dsp, struct snd_soc_codec *codec);
void wm_adsp_queue_boot_work(struct wm_adsp *dsp);
int wm_vpu_setup_algs(struct wm_adsp *vpu);
int wm_vpu_init(struct wm_adsp *vpu);
int wm_halo_init(struct wm_adsp *dsp, struct mutex *rate_lock);
int wm_adsp1_event(struct snd_soc_dapm_widget *w,
		   struct snd_kcontrol *kcontrol, int event);

int wm_adsp2_early_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event,
			 unsigned int freq);

int wm_adsp2_lock(struct wm_adsp *adsp, unsigned int regions);
irqreturn_t wm_adsp2_bus_error(struct wm_adsp *adsp);
irqreturn_t wm_halo_bus_error(struct wm_adsp *dsp);
irqreturn_t wm_halo_wdt_expire(int irq, void *data);

int wm_adsp2_event(struct snd_soc_dapm_widget *w,
		   struct snd_kcontrol *kcontrol, int event);

int wm_halo_early_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event);
int wm_halo_event(struct snd_soc_dapm_widget *w,
		   struct snd_kcontrol *kcontrol, int event);

int wm_adsp2_preloader_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol);
int wm_adsp2_preloader_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol);
int wm_adsp_fw_get(struct snd_kcontrol *kcontrol,
		   struct snd_ctl_elem_value *ucontrol);
int wm_adsp_fw_put(struct snd_kcontrol *kcontrol,
		   struct snd_ctl_elem_value *ucontrol);

int wm_adsp_compr_open(struct wm_adsp *dsp,
			      struct snd_compr_stream *stream,
			      int channel);
int wm_adsp_compr_free(struct snd_compr_stream *stream);
int wm_adsp_compr_set_params(struct snd_compr_stream *stream,
			     struct snd_compr_params *params);
int wm_adsp_compr_get_caps(struct snd_compr_stream *stream,
			   struct snd_compr_caps *caps);
int wm_adsp_compr_trigger(struct snd_compr_stream *stream, int cmd);
int wm_adsp_compr_handle_irq(struct wm_adsp *dsp, int channel);
int wm_adsp_compr_pointer(struct snd_compr_stream *stream,
			  struct snd_compr_tstamp *tstamp);
int wm_adsp_compr_copy(struct snd_compr_stream *stream,
		       char __user *buf, size_t count);

int wm_adsp_write_ctl(struct wm_adsp *dsp, const char *name, const void *buf,
		      size_t len);
int wm_adsp_read_ctl(struct wm_adsp *dsp, const char *name, void *buf,
		     size_t len);

extern int wm_adsp_handle_fw_event(struct wm_adsp *dsp);

#endif
