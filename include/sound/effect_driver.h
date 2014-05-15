/*
 *  effect_driver.h - effect offload driver APIs
 *
 *  Copyright (C) 2013 Intel Corporation
 *  Authors:	Vinod Koul <vinod.koul@intel.com>
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
 */
#ifndef __EFFECT_DRIVER_H
#define __EFFECT_DRIVER_H

#include <sound/effect_offload.h>

struct snd_effect_ops {
	int (*create)(struct snd_card *card, struct snd_effect *effect);
	int (*destroy)(struct snd_card *card, struct snd_effect *effect);
	int (*set_params)(struct snd_card *card,
				struct snd_effect_params *params);
	int (*get_params)(struct snd_card *card,
				struct snd_effect_params *params);
	int (*query_num_effects)(struct snd_card *card);
	int (*query_effect_caps)(struct snd_card *card,
					struct snd_effect_caps *caps);
};

#if IS_ENABLED(CONFIG_SND_EFFECTS_OFFLOAD)
int snd_effect_register(struct snd_card *card, struct snd_effect_ops *ops);
int snd_effect_deregister(struct snd_card *card);
#else
static inline int snd_effect_register(struct snd_card *card,
					struct snd_effect_ops *ops)
{
	return -ENODEV;
}
static inline int snd_effect_deregister(struct snd_card *card)
{
	return -ENODEV;
}
#endif

/* IOCTL fns */
int snd_ctl_effect_create(struct snd_card *card, void *arg);
int snd_ctl_effect_destroy(struct snd_card *card, void *arg);
int snd_ctl_effect_set_params(struct snd_card *card, void *arg);
int snd_ctl_effect_get_params(struct snd_card *card, void *arg);
int snd_ctl_effect_query_num_effects(struct snd_card *card, void *arg);
int snd_ctl_effect_query_effect_caps(struct snd_card *card, void *arg);
#endif
