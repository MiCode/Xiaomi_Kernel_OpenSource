/*
 * linux/sound/soc-dapm.h -- ALSA SoC Dynamic Audio Power Management
 *
 * Author:		Liam Girdwood
 * Created:		Aug 11th 2005
 * Copyright:	Wolfson Microelectronics. PLC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_SND_SOC_DAPM_H
#define __LINUX_SND_SOC_DAPM_H

#include <linux/types.h>
#include <sound/control.h>
#include <uapi/sound/asoc.h>

struct device;

/* dapm stream operations */
#define SND_SOC_DAPM_STREAM_NOP			0x0
#define SND_SOC_DAPM_STREAM_START		0x1
#define SND_SOC_DAPM_STREAM_STOP		0x2
#define SND_SOC_DAPM_STREAM_SUSPEND		0x4
#define SND_SOC_DAPM_STREAM_RESUME		0x8
#define SND_SOC_DAPM_STREAM_PAUSE_PUSH	0x10
#define SND_SOC_DAPM_STREAM_PAUSE_RELEASE	0x20

/* dapm event types */
#define SND_SOC_DAPM_PRE_PMU	0x1 	/* before widget power up */
#define SND_SOC_DAPM_POST_PMU	0x2		/* after widget power up */
#define SND_SOC_DAPM_PRE_PMD	0x4 	/* before widget power down */
#define SND_SOC_DAPM_POST_PMD	0x8		/* after widget power down */
#define SND_SOC_DAPM_PRE_REG	0x10	/* before audio path setup */
#define SND_SOC_DAPM_POST_REG	0x20	/* after audio path setup */
#define SND_SOC_DAPM_WILL_PMU   0x40    /* called at start of sequence */
#define SND_SOC_DAPM_WILL_PMD   0x80    /* called at start of sequence */
#define SND_SOC_DAPM_PRE_POST_PMD \
				(SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD)

/* convenience event type detection */
#define SND_SOC_DAPM_EVENT_ON(e)	\
	(e & (SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU))
#define SND_SOC_DAPM_EVENT_OFF(e)	\
	(e & (SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD))

/* regulator widget flags */
#define SND_SOC_DAPM_REGULATOR_BYPASS     0x1     /* bypass when disabled */

struct snd_soc_dapm_widget;
enum snd_soc_dapm_type;
struct snd_soc_dapm_path;
struct snd_soc_dapm_pin;
struct snd_soc_dapm_route;
struct snd_soc_dapm_context;
struct regulator;
struct snd_soc_dapm_widget_list;
struct snd_soc_dapm_update;

int dapm_reg_event(struct snd_soc_dapm_widget *w,
		   struct snd_kcontrol *kcontrol, int event);
int dapm_regulator_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event);
int dapm_clock_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event);
bool dapm_kcontrol_set_value(const struct snd_kcontrol *kcontrol,
	unsigned int value);
unsigned int dapm_kcontrol_get_value(const struct snd_kcontrol *kcontrol);
struct snd_soc_dapm_widget_list *dapm_kcontrol_get_wlist(
	const struct snd_kcontrol *kcontrol);

/* dapm controls */
int snd_soc_dapm_put_volsw(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
int snd_soc_dapm_get_volsw(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
int snd_soc_dapm_get_enum_double(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
int snd_soc_dapm_put_enum_double(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
int snd_soc_dapm_get_enum_virt(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
int snd_soc_dapm_put_enum_virt(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
int snd_soc_dapm_get_value_enum_double(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
int snd_soc_dapm_put_value_enum_double(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
int snd_soc_dapm_info_pin_switch(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo);
int snd_soc_dapm_get_pin_switch(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *uncontrol);
int snd_soc_dapm_put_pin_switch(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *uncontrol);
int snd_soc_dapm_new_controls(struct snd_soc_dapm_context *dapm,
	const struct snd_soc_dapm_widget *widget,
	int num);
int snd_soc_dapm_new_dai_widgets(struct snd_soc_dapm_context *dapm,
				 struct snd_soc_dai *dai);
int snd_soc_dapm_link_dai_widgets(struct snd_soc_card *card);
void snd_soc_dapm_connect_dai_link_widgets(struct snd_soc_card *card);
int snd_soc_dapm_new_pcm(struct snd_soc_card *card,
			 const struct snd_soc_pcm_stream *params,
			 struct snd_soc_dapm_widget *source,
			 struct snd_soc_dapm_widget *sink);

/* dapm path setup */
int snd_soc_dapm_new_widgets(struct snd_soc_card *card);
void snd_soc_dapm_free(struct snd_soc_dapm_context *dapm);
int snd_soc_dapm_add_routes(struct snd_soc_dapm_context *dapm,
			    const struct snd_soc_dapm_route *route, int num);
int snd_soc_dapm_del_routes(struct snd_soc_dapm_context *dapm,
			    const struct snd_soc_dapm_route *route, int num);
int snd_soc_dapm_weak_routes(struct snd_soc_dapm_context *dapm,
			     const struct snd_soc_dapm_route *route, int num);

/* dapm events */
void snd_soc_dapm_stream_event(struct snd_soc_pcm_runtime *rtd, int stream,
	int event);
void snd_soc_dapm_shutdown(struct snd_soc_card *card);

/* external DAPM widget events */
int snd_soc_dapm_mixer_update_power(struct snd_soc_dapm_context *dapm,
		struct snd_kcontrol *kcontrol, int connect,
		struct snd_soc_dapm_update *update);
int snd_soc_dapm_mux_update_power(struct snd_soc_dapm_context *dapm,
		struct snd_kcontrol *kcontrol, int mux, struct soc_enum *e,
		struct snd_soc_dapm_update *update);

/* dapm sys fs - used by the core */
int snd_soc_dapm_sys_add(struct device *dev);
void snd_soc_dapm_debugfs_init(struct snd_soc_dapm_context *dapm,
				struct dentry *parent);

/* dapm audio pin control and status */
int snd_soc_dapm_enable_pin(struct snd_soc_dapm_context *dapm,
			    const char *pin);
int snd_soc_dapm_enable_pin_unlocked(struct snd_soc_dapm_context *dapm,
				     const char *pin);
int snd_soc_dapm_disable_pin(struct snd_soc_dapm_context *dapm,
			     const char *pin);
int snd_soc_dapm_disable_pin_unlocked(struct snd_soc_dapm_context *dapm,
				      const char *pin);
int snd_soc_dapm_nc_pin(struct snd_soc_dapm_context *dapm, const char *pin);
int snd_soc_dapm_nc_pin_unlocked(struct snd_soc_dapm_context *dapm,
				 const char *pin);
int snd_soc_dapm_get_pin_status(struct snd_soc_dapm_context *dapm,
				const char *pin);
int snd_soc_dapm_sync(struct snd_soc_dapm_context *dapm);
int snd_soc_dapm_sync_unlocked(struct snd_soc_dapm_context *dapm);
int snd_soc_dapm_force_enable_pin(struct snd_soc_dapm_context *dapm,
				  const char *pin);
int snd_soc_dapm_force_enable_pin_unlocked(struct snd_soc_dapm_context *dapm,
					   const char *pin);
int snd_soc_dapm_ignore_suspend(struct snd_soc_dapm_context *dapm,
				const char *pin);
void snd_soc_dapm_auto_nc_codec_pins(struct snd_soc_codec *codec);

/* Mostly internal - should not normally be used */
void dapm_mark_dirty(struct snd_soc_dapm_widget *w, const char *reason);
void dapm_mark_io_dirty(struct snd_soc_dapm_context *dapm);

/* dapm path query */
int snd_soc_dapm_dai_get_connected_widgets(struct snd_soc_dai *dai, int stream,
	struct snd_soc_dapm_widget_list **list);

struct snd_soc_codec *snd_soc_dapm_kcontrol_codec(struct snd_kcontrol *kcontrol);

/* dapm set/reset */
int snd_soc_dapm_state_set(struct snd_soc_card *card, bool reset_state);

enum snd_soc_dapm_subclass {
	SND_SOC_DAPM_CLASS_INIT		= 0,
	SND_SOC_DAPM_CLASS_RUNTIME	= 1,
};

/*
 * DAPM audio route definition.
 *
 * Defines an audio route originating at source via control and finishing
 * at sink.
 */
struct snd_soc_dapm_route {
	const char *sink;
	const char *control;
	const char *source;

	/* Note: currently only supported for links where source is a supply */
	int (*connected)(struct snd_soc_dapm_widget *source,
			 struct snd_soc_dapm_widget *sink);
};

/* dapm audio path between two widgets */
struct snd_soc_dapm_path {
	const char *name;

	/* source (input) and sink (output) widgets */
	struct snd_soc_dapm_widget *source;
	struct snd_soc_dapm_widget *sink;

	/* status */
	u32 connect:1;	/* source and sink widgets are connected */
	u32 walked:1;	/* path has been walked */
	u32 walking:1;  /* path is in the process of being walked */
	u32 weak:1;	/* path ignored for power management */

	int (*connected)(struct snd_soc_dapm_widget *source,
			 struct snd_soc_dapm_widget *sink);

	struct list_head list_source;
	struct list_head list_sink;
	struct list_head list_kcontrol;
	struct list_head list;

	u32 index;
};

/* dapm widget */
struct snd_soc_dapm_widget {
	enum snd_soc_dapm_type id;
	const char *name;		/* widget name */
	const char *sname;	/* stream name */
	struct snd_soc_codec *codec;
	struct snd_soc_platform *platform;
	struct list_head list;
	struct snd_soc_dapm_context *dapm;

	int pvt_data_len;			/* Len of widget specific data*/
	void *priv;				/* widget specific data */
	struct regulator *regulator;		/* attached regulator */
	const struct snd_soc_pcm_stream *params; /* params for dai links */

	/* dapm control */
	int reg;				/* negative reg = no direct dapm */
	unsigned char shift;			/* bits to shift */
	unsigned int mask;			/* non-shifted mask */
	unsigned int on_val;			/* on state value */
	unsigned int off_val;			/* off state value */
	unsigned char power:1;			/* block power status */
	unsigned char active:1;			/* active stream on DAC, ADC's */
	unsigned char connected:1;		/* connected codec pin */
	unsigned char new:1;			/* cnew complete */
	unsigned char ext:1;			/* has external widgets */
	unsigned char force:1;			/* force state */
	unsigned char ignore_suspend:1;         /* kept enabled over suspend */
	unsigned char new_power:1;		/* power from this run */
	unsigned char power_checked:1;		/* power checked this run */
	int subseq;				/* sort within widget type */

	int (*power_check)(struct snd_soc_dapm_widget *w);

	/* external events */
	unsigned short event_flags;		/* flags to specify event types */
	int (*event)(struct snd_soc_dapm_widget*, struct snd_kcontrol *, int);

	/* kcontrols that relate to this widget */
	int num_kcontrols;
	const struct snd_kcontrol_new *kcontrol_news;
	struct snd_kcontrol **kcontrols;
	u32 index;
	unsigned char kcontrol_enum:1;	/* this widget is an enum kcontrol*/

	/* widget input and outputs */
	struct list_head sources;
	struct list_head sinks;

	/* used during DAPM updates */
	struct list_head power_list;
	struct list_head dirty;
	int inputs;
	int outputs;

	struct clk *clk;
};

struct snd_soc_dapm_update {
	struct snd_kcontrol *kcontrol;
	int reg;
	int mask;
	int val;
};

/* DAPM context */
struct snd_soc_dapm_context {
	enum snd_soc_bias_level bias_level;
	enum snd_soc_bias_level suspend_bias_level;
	struct delayed_work delayed_work;
	unsigned int idle_bias_off:1; /* Use BIAS_OFF instead of STANDBY */

	void (*seq_notifier)(struct snd_soc_dapm_context *,
			     enum snd_soc_dapm_type, int);

	struct device *dev; /* from parent - for debug */
	struct snd_soc_codec *codec; /* parent codec */
	struct snd_soc_platform *platform; /* parent platform */
	struct snd_soc_card *card; /* parent card */

	/* used during DAPM updates */
	enum snd_soc_bias_level target_bias_level;
	struct list_head list;

	int (*stream_event)(struct snd_soc_dapm_context *dapm, int event);

#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_dapm;
#endif
};

/* A list of widgets associated with an object, typically a snd_kcontrol */
struct snd_soc_dapm_widget_list {
	int num_widgets;
	struct snd_soc_dapm_widget *widgets[0];
};

struct snd_soc_dapm_stats {
	int power_checks;
	int path_checks;
	int neighbour_checks;
};

#endif
