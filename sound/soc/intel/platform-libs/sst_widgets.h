/*
 *  sst_widgets.h - Intel helpers to generate FW widgets
 *
 *  Copyright (C) 2013 Intel Corp
 *  Author: Omair Mohammed Abdullah <omair.m.abdullah@intel.com>
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
 */

#ifndef __SST_WIDGETS_H__
#define __SST_WIDGETS_H__

#include <sound/soc.h>
#include <sound/tlv.h>
#include <uapi/sound/sst_v2_vendor.h>

#define SST_MODULE_GAIN 1
#define SST_MODULE_ALGO 2

#define SST_FMT_MONO 0
#define SST_FMT_STEREO 3

struct module {
	struct snd_kcontrol *kctl;
	struct list_head node;
};

struct sst_ids {
	u16 location_id;
	u16 module_id;
	u8  task_id;
	u8  format;
	u8  reg;
	const char *parent_wname;
	struct snd_soc_dapm_widget *parent_w;
	struct list_head algo_list;
	struct list_head gain_list;
	struct sst_pcm_format *pcm_fmt;
};


#define SST_AIF_IN(wname, wevent)							\
{	.id = snd_soc_dapm_aif_in, .name = wname, .sname = NULL,			\
	.reg = SND_SOC_NOPM, .shift = 0,					\
	.on_val = 1, .off_val = 0,							\
	.event = wevent, .event_flags = SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD,	\
	.priv = (void *)&(struct sst_ids) { .task_id = 0, .location_id = 0 }		\
}

#define SST_AIF_OUT(wname, wevent)							\
{	.id = snd_soc_dapm_aif_out, .name = wname, .sname = NULL,			\
	.reg = SND_SOC_NOPM, .shift = 0, 					\
	.on_val = 1, .off_val = 0,							\
	.event = wevent, .event_flags = SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD,	\
	.priv = (void *)&(struct sst_ids) { .task_id = 0, .location_id = 0 }		\
}

#define SST_INPUT(wname, wevent)							\
{	.id = snd_soc_dapm_input, .name = wname, .sname = NULL,				\
	.reg = SND_SOC_NOPM, .shift = 0, 					\
	.on_val = 1, .off_val = 0,							\
	.event = wevent, .event_flags = SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD,	\
	.priv = (void *)&(struct sst_ids) { .task_id = 0, .location_id = 0 }		\
}

#define SST_OUTPUT(wname, wevent)							\
{	.id = snd_soc_dapm_output, .name = wname, .sname = NULL,			\
	.reg = SND_SOC_NOPM, .shift = 0, 					\
	.on_val = 1, .off_val = 0,							\
	.event = wevent, .event_flags = SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD,	\
	.priv = (void *)&(struct sst_ids) { .task_id = 0, .location_id = 0 }		\
}

#define SST_DAPM_OUTPUT(wname, wloc_id, wtask_id, wformat, wevent)                      \
{	.id = snd_soc_dapm_output, .name = wname, .sname = NULL,                        \
	.reg = SND_SOC_NOPM, .shift = 0,                                    \
	.on_val = 1, .off_val = 0,							\
	.event = wevent, .event_flags = SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD,   \
	.priv = (void *)&(struct sst_ids) { .location_id = wloc_id, .task_id = wtask_id,\
						.pcm_fmt = wformat, }			\
}

#define SST_PATH(wname, wtask, wloc_id, wevent, wflags)					\
{	.id = snd_soc_dapm_pga, .name = wname, .reg = SND_SOC_NOPM, .shift = 0,		\
	 .kcontrol_news = NULL, .num_kcontrols = 0,				\
	.on_val = 1, .off_val = 0,							\
	.event = wevent, .event_flags = wflags,						\
	.priv = (void *)&(struct sst_ids) { .task_id = wtask, .location_id = wloc_id, }	\
}

#define SST_LINKED_PATH(wname, wtask, wloc_id, linked_wname, wevent, wflags)		\
{	.id = snd_soc_dapm_pga, .name = wname, .reg = SND_SOC_NOPM, .shift = 0,		\
	 .kcontrol_news = NULL, .num_kcontrols = 0,				\
	.on_val = 1, .off_val = 0,							\
	.event = wevent, .event_flags = wflags,						\
	.priv = (void *)&(struct sst_ids) { .task_id = wtask, .location_id = wloc_id,	\
					.parent_wname = linked_wname}			\
}

#define SST_PATH_MEDIA_LOOP(wname, wtask, wloc_id, wformat, wevent, wflags)             \
{	.id = snd_soc_dapm_pga, .name = wname, .reg = SND_SOC_NOPM, .shift = 0,         \
	 .kcontrol_news = NULL, .num_kcontrols = 0,                         \
	.event = wevent, .event_flags = wflags,                                         \
	.priv = (void *)&(struct sst_ids) { .task_id = wtask, .location_id = wloc_id,	\
					    .format = wformat,}				\
}

/* output is triggered before input */
#define SST_PATH_INPUT(name, task_id, loc_id, event)					\
	SST_PATH(name, task_id, loc_id, event, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD)

#define SST_PATH_LINKED_INPUT(name, task_id, loc_id, linked_wname, event)		\
	SST_LINKED_PATH(name, task_id, loc_id, linked_wname, event,			\
					SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD)

#define SST_PATH_OUTPUT(name, task_id, loc_id, event)					\
	SST_PATH(name, task_id, loc_id, event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD)

#define SST_PATH_LINKED_OUTPUT(name, task_id, loc_id, linked_wname, event)		\
	SST_LINKED_PATH(name, task_id, loc_id, linked_wname, event,			\
					SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD)

#define SST_PATH_MEDIA_LOOP_OUTPUT(name, task_id, loc_id, format, event)		\
	SST_PATH_MEDIA_LOOP(name, task_id, loc_id, format, event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD)


#define SST_SWM_MIXER(wname, wreg, wtask, wloc_id, wcontrols, wevent)			\
{	.id = snd_soc_dapm_mixer, .name = wname, .reg = SND_SOC_NOPM, .shift = 0,	\
	 .kcontrol_news = wcontrols, .num_kcontrols = ARRAY_SIZE(wcontrols),\
	.event = wevent, .event_flags = SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD |	\
					SND_SOC_DAPM_POST_REG,				\
	.priv = (void *)&(struct sst_ids) { .task_id = wtask, .location_id = wloc_id,	\
					    .reg = wreg }				\
}

struct sst_gain_data {
	bool stereo;
	enum sst_gain_kcontrol_type type;
	struct sst_gain_value *gain_val;
	int max;
	int min;
	u16 instance_id;
	u16 module_id;
	u16 pipe_id;
	u16 task_id;
	char pname[44];
	struct snd_soc_dapm_widget *w;
};

struct sst_gain_value {
	u16 ramp_duration;
	s16 l_gain;
	s16 r_gain;
	bool mute;
};

#define SST_GAIN_VOLUME_DEFAULT		(-1440)
#define SST_GAIN_RAMP_DURATION_DEFAULT	5 /* timeconstant */
#define SST_GAIN_MUTE_DEFAULT		true

#define SST_GAIN_KCONTROL_TLV(xname, xhandler_get, xhandler_put, \
			      xmod, xpipe, xinstance, xtask, tlv_array, xgain_val, \
			      xmin, xmax, xpname) \
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.access = SNDRV_CTL_ELEM_ACCESS_TLV_READ | \
		  SNDRV_CTL_ELEM_ACCESS_READWRITE, \
	.tlv.p = (tlv_array), \
	.info = sst_gain_ctl_info,\
	.get = xhandler_get, .put = xhandler_put, \
	.private_value = (unsigned long)&(struct soc_mixer_control) \
	{ .reg = xmin, .rreg = xmax, .min = xmin, .max = xmax,\
		.pvt_data = (char *)&(struct sst_gain_data)\
		{ .stereo = true, .max = xmax, .min = xmin, .type = SST_GAIN_TLV, \
		.module_id = xmod, .pipe_id = xpipe, .task_id = xtask,\
		.instance_id = xinstance, .gain_val = xgain_val, .pname = xpname} }

#define SST_GAIN_KCONTROL_INT(xname, xhandler_get, xhandler_put, \
			      xmod, xpipe, xinstance, xtask, xtype, xgain_val, \
			      xmin, xmax, xpname) \
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = sst_gain_ctl_info, \
	.get = xhandler_get, .put = xhandler_put, \
	.private_value = (unsigned long)&(struct soc_mixer_control) \
	{ .min = xmin, .max = xmax,\
		.pvt_data = (char *)&(struct sst_gain_data)\
		{ .stereo = false, .max = xmax, .min = xmin, .type = xtype, \
		.module_id = xmod, .pipe_id = xpipe, .task_id = xtask,\
		.instance_id = xinstance, .gain_val = xgain_val, .pname =  xpname} }

#define SST_GAIN_KCONTROL_BOOL(xname, xhandler_get, xhandler_put,\
			       xmod, xpipe, xinstance, xtask, xgain_val, xpname) \
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = snd_soc_info_bool_ext, \
	.get = xhandler_get, .put = xhandler_put, \
	.private_value = (unsigned long)&(struct soc_mixer_control) \
	{ .pvt_data = (char *)&(struct sst_gain_data)\
		{ .stereo = false, .type = SST_GAIN_MUTE, \
		.module_id = xmod, .pipe_id = xpipe, .task_id = xtask,\
		.instance_id = xinstance, .gain_val = xgain_val, .pname = xpname} }

#define SST_CONTROL_NAME(xpname, xmname, xinstance, xtype) \
	xpname " " xmname " " #xinstance " " xtype

#define SST_COMBO_CONTROL_NAME(xpname, xmname, xinstance, xtype, xsubmodule) \
	xpname " " xmname " " #xinstance " " xtype " " xsubmodule

/*
 * 3 Controls for each Gain module
 * e.g.	- pcm0_in gain 0 volume
 *	- pcm0_in gain 0 rampduration
 *	- pcm0_in gain 0 mute
 */
#define SST_GAIN_KCONTROLS(xpname, xmname, xmin_gain, xmax_gain, xmin_tc, xmax_tc, \
			   xhandler_get, xhandler_put, \
			   xmod, xpipe, xinstance, xtask, tlv_array, xgain_val) \
	{ SST_GAIN_KCONTROL_INT(SST_CONTROL_NAME(xpname, xmname, xinstance, "rampduration"), \
		xhandler_get, xhandler_put, xmod, xpipe, xinstance, xtask, SST_GAIN_RAMP_DURATION, \
		xgain_val, xmin_tc, xmax_tc, xpname) }, \
	{ SST_GAIN_KCONTROL_BOOL(SST_CONTROL_NAME(xpname, xmname, xinstance, "mute"), \
		xhandler_get, xhandler_put, xmod, xpipe, xinstance, xtask, \
		xgain_val, xpname) } ,\
	{ SST_GAIN_KCONTROL_TLV(SST_CONTROL_NAME(xpname, xmname, xinstance, "volume"), \
		xhandler_get, xhandler_put, xmod, xpipe, xinstance, xtask, tlv_array, \
		xgain_val, xmin_gain, xmax_gain, xpname) }

#define SST_GAIN_TC_MIN		5
#define SST_GAIN_TC_MAX		5000
#define SST_GAIN_MIN_VALUE	-1440 /* in 0.1 DB units */
#define SST_GAIN_MAX_VALUE	360

struct sst_algo_data {
	enum sst_algo_kcontrol_type type;
	int max;
	u16 module_id;
	u16 pipe_id;
	u16 task_id;
	u16 cmd_id;
	bool bypass;
	unsigned char *params;
	struct snd_soc_dapm_widget *w;
};

/* size of the control = size of params + size of length field */
#define SST_ALGO_CTL_VALUE(xcount, xtype, xpipe, xmod, xtask, xcmd)			\
	(struct soc_bytes_ext) {.max = xcount + sizeof(u16),							\
		.pvt_data = (char *) &(struct sst_algo_data)				\
		{.max = xcount + sizeof(u16), .type = xtype, .module_id = xmod,		\
			.pipe_id = xpipe, .task_id = xtask, .cmd_id = xcmd,		\
		}									\
	}

#define SST_ALGO_KCONTROL(xname, xcount, xmod, xpipe,					\
			  xtask, xcmd, xtype, xinfo, xget, xput)			\
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,						\
	.name =  xname,									\
	.info = xinfo, .get = xget, .put = xput,					\
	.private_value = (unsigned long)&						\
			SST_ALGO_CTL_VALUE(xcount, xtype, xpipe,			\
					   xmod, xtask, xcmd),				\
}

#define SST_ALGO_KCONTROL_BYTES(xpname, xmname, xcount, xmod,				\
				xpipe, xinstance, xtask, xcmd)				\
	SST_ALGO_KCONTROL(SST_CONTROL_NAME(xpname, xmname, xinstance, "params"),	\
			  xcount, xmod, xpipe, xtask, xcmd, SST_ALGO_PARAMS,		\
			  sst_algo_bytes_ctl_info,					\
			  sst_algo_control_get, sst_algo_control_set)

#define SST_ALGO_KCONTROL_BOOL(xpname, xmname, xmod, xpipe, xinstance, xtask)		\
	SST_ALGO_KCONTROL(SST_CONTROL_NAME(xpname, xmname, xinstance, "bypass"),	\
			  0, xmod, xpipe, xtask, 0, SST_ALGO_BYPASS,			\
			  snd_soc_info_bool_ext,					\
			  sst_algo_control_get, sst_algo_control_set)

#define SST_ALGO_BYPASS_PARAMS(xpname, xmname, xcount, xmod, xpipe,			\
				xinstance, xtask, xcmd)					\
	SST_ALGO_KCONTROL_BOOL(xpname, xmname, xmod, xpipe, xinstance, xtask),		\
	SST_ALGO_KCONTROL_BYTES(xpname, xmname, xcount, xmod, xpipe, xinstance, xtask, xcmd)

#define SST_COMBO_ALGO_KCONTROL_BYTES(xpname, xmname, xsubmod, xcount, xmod,		\
				      xpipe, xinstance, xtask, xcmd)			\
	SST_ALGO_KCONTROL(SST_COMBO_CONTROL_NAME(xpname, xmname, xinstance, "params",	\
						 xsubmod),				\
			  xcount, xmod, xpipe, xtask, xcmd, SST_ALGO_PARAMS,		\
			  sst_algo_bytes_ctl_info,					\
			  sst_algo_control_get, sst_algo_control_set)


struct sst_enum {
	bool tx;
	unsigned short reg;
	unsigned int max;
	const char * const *texts;
	struct snd_soc_dapm_widget *w;
};

/* only 4 slots/channels supported atm */
#define SST_SSP_SLOT_ENUM(s_ch_no, is_tx, xtexts) \
	(struct sst_enum){ .reg = s_ch_no, .tx = is_tx, .max = 4+1, .texts = xtexts, }

#define SST_SLOT_CTL_NAME(xpname, xmname, s_ch_name) \
	xpname " " xmname " " s_ch_name

#define SST_SSP_SLOT_CTL(xpname, xmname, s_ch_name, s_ch_no, is_tx, xtexts, xget, xput) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
	.name = SST_SLOT_CTL_NAME(xpname, xmname, s_ch_name), \
	.info = sst_slot_enum_info, \
	.get = xget, .put = xput, \
	.private_value = (unsigned long)&SST_SSP_SLOT_ENUM(s_ch_no, is_tx, xtexts), \
}

#define SST_MUX_CTL_NAME(xpname, xinstance) \
	xpname " " #xinstance

#define SST_SSP_MUX_ENUM(xreg, xshift, xtexts) \
	(struct soc_enum){ .reg = xreg, .texts = xtexts, .shift_l = xshift, \
			   .shift_r = xshift, .items = ARRAY_SIZE(xtexts), }

#define SST_SSP_MUX_CTL(xpname, xinstance, xreg, xshift, xtexts, xget, xput) \
	SOC_DAPM_ENUM_EXT(SST_MUX_CTL_NAME(xpname, xinstance), \
			  SST_SSP_MUX_ENUM(xreg, xshift, xtexts), \
			  xget, xput)

struct sst_probe_value {
	unsigned int val;
	const struct soc_enum *p_enum;
};

#define SST_PROBE_CTL_NAME(dir, num, type) \
	dir #num " " type

#define SST_PROBE_ENUM(xname, xenum, xhandler_get, xhandler_put) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = sst_probe_enum_info, \
	.get = xhandler_get, .put = xhandler_put, \
	.private_value = (unsigned long)&(struct sst_probe_value) \
	{ .val = 0, .p_enum = &xenum } }

#endif
