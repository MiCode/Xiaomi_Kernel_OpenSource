/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
 */

#ifndef _WSA881X_H
#define _WSA881X_H

#include <linux/regmap.h>
#include <sound/soc.h>
#include <sound/info.h>
#include "wsa881x-registers.h"

#define WSA881X_MAX_SWR_PORTS   4

#if IS_ENABLED(CONFIG_SND_SOC_WSA881X)
extern int wsa881x_set_channel_map(struct snd_soc_component *component,
				   u8 *port, u8 num_port, unsigned int *ch_mask,
				   unsigned int *ch_rate, u8 *port_type);

extern const u8 wsa881x_reg_readable[WSA881X_CACHE_SIZE];
extern struct regmap_config wsa881x_regmap_config;
extern int wsa881x_codec_info_create_codec_entry(
					struct snd_info_entry *codec_root,
					struct snd_soc_component *component);
void wsa881x_regmap_defaults(struct regmap *regmap, u8 version);

#else
extern int wsa881x_set_channel_map(struct snd_soc_component *component,
				   u8 *port, u8 num_port, unsigned int *ch_mask,
				   unsigned int *ch_rate, u8 *port_type)
{
	return 0;
}

extern int wsa881x_codec_info_create_codec_entry(
					struct snd_info_entry *codec_root,
					struct snd_soc_component *component)
{
	return 0;
}

void wsa881x_regmap_defaults(struct regmap *regmap, u8 version)
{
}

#endif

#endif /* _WSA881X_H */
