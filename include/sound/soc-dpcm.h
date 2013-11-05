/*
 * linux/sound/soc-dpcm.h -- ALSA SoC Dynamic PCM Support
 *
 * Author:		Liam Girdwood <lrg@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_SND_SOC_DPCM_H
#define __LINUX_SND_SOC_DPCM_H

#include <linux/slab.h>
#include <sound/pcm.h>

/*
 * Types of runtime_update to perform (e.g. originated from FE PCM ops
 * or audio route changes triggered by muxes/mixers.
 */
#define SND_SOC_DPCM_UPDATE_NO	0
#define SND_SOC_DPCM_UPDATE_BE	1
#define SND_SOC_DPCM_UPDATE_FE	2

/*
 * Dynamic PCM Frontend -> Backend link state.
 */
enum snd_soc_dpcm_link_state {
	SND_SOC_DPCM_LINK_STATE_NEW	= 0,	/* newly created path */
	SND_SOC_DPCM_LINK_STATE_FREE,			/* path to be dismantled */
};

/*
 * Dynamic PCM params link
 * This links together a FE and BE DAI at runtime and stores the link
 * state information and the hw_params configuration.
 */
struct snd_soc_dpcm_params {
	/* FE and BE DAIs*/
	struct snd_soc_pcm_runtime *be;
	struct snd_soc_pcm_runtime *fe;

	/* link state */
	enum snd_soc_dpcm_link_state state;

	struct list_head list_be;
	struct list_head list_fe;

	/* hw params for this link - may be different for each link */
	struct snd_pcm_hw_params hw_params;

#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_state;
#endif
};

/*
 * Bespoke Trigger() Helper API
 */

/* is the PCM operation for this FE ? */
static inline int snd_soc_dpcm_fe_can_update(struct snd_soc_pcm_runtime *fe,
		int stream)
{
	return (fe->dpcm[stream].runtime_update == SND_SOC_DPCM_UPDATE_FE);
}

/* is the PCM operation for this BE ? */
static inline int snd_soc_dpcm_be_can_update(struct snd_soc_pcm_runtime *fe,
		struct snd_soc_pcm_runtime *be, int stream)
{
	if ((fe->dpcm[stream].runtime_update == SND_SOC_DPCM_UPDATE_FE) ||
	    ((fe->dpcm[stream].runtime_update == SND_SOC_DPCM_UPDATE_BE) &&
		  be->dpcm[stream].runtime_update))
		return 1;
	else
		return 0;
}

/* trigger platform driver only */
static inline int
	snd_soc_dpcm_platform_trigger(struct snd_pcm_substream *substream,
	int cmd, struct snd_soc_platform *platform)
{
	if (platform->driver->ops->trigger)
		return platform->driver->ops->trigger(substream, cmd);
	return 0;
}

int snd_soc_dpcm_can_be_free_stop(struct snd_soc_pcm_runtime *fe,
		struct snd_soc_pcm_runtime *be, int stream);

static inline struct snd_pcm_substream *
	snd_soc_dpcm_get_substream(struct snd_soc_pcm_runtime *be, int stream)
{
	return be->pcm->streams[stream].substream;
}

static inline enum snd_soc_dpcm_state
	snd_soc_dpcm_be_get_state(struct snd_soc_pcm_runtime *be, int stream)
{
	return be->dpcm[stream].state;
}

static inline void snd_soc_dpcm_be_set_state(struct snd_soc_pcm_runtime *be,
		int stream, enum snd_soc_dpcm_state state)
{
	be->dpcm[stream].state = state;
}

int dpcm_path_get(struct snd_soc_pcm_runtime *fe,
       int stream, struct snd_soc_dapm_widget_list **list_);
int dpcm_process_paths(struct snd_soc_pcm_runtime *fe,
       int stream, struct snd_soc_dapm_widget_list **list, int new);
int dpcm_be_dai_startup(struct snd_soc_pcm_runtime *fe, int stream);
int dpcm_be_dai_shutdown(struct snd_soc_pcm_runtime *fe, int stream);
void dpcm_be_disconnect(struct snd_soc_pcm_runtime *fe, int stream);
void dpcm_clear_pending_state(struct snd_soc_pcm_runtime *fe, int stream);
int dpcm_be_dai_hw_free(struct snd_soc_pcm_runtime *fe, int stream);
int dpcm_be_dai_hw_params(struct snd_soc_pcm_runtime *fe, int tream);
int dpcm_be_dai_trigger(struct snd_soc_pcm_runtime *fe, int stream, int cmd);
int dpcm_be_dai_prepare(struct snd_soc_pcm_runtime *fe, int stream);
int dpcm_dapm_stream_event(struct snd_soc_pcm_runtime *fe,
	int dir, const char *stream, int event);

static inline void dpcm_path_put(struct snd_soc_dapm_widget_list **list)
{
       kfree(*list);
}


#endif
