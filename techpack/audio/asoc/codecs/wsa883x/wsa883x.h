/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _WSA883X_H
#define _WSA883X_H

#include <linux/regmap.h>
#include <sound/soc.h>
#include <sound/info.h>
#include "wsa883x-registers.h"

#define WSA883X_MAX_SWR_PORTS   4

#if IS_ENABLED(CONFIG_SND_SOC_WSA883X)
int wsa883x_set_channel_map(struct snd_soc_component *component,
				   u8 *port, u8 num_port, unsigned int *ch_mask,
				   unsigned int *ch_rate, u8 *port_type);


int wsa883x_codec_info_create_codec_entry(
					struct snd_info_entry *codec_root,
					struct snd_soc_component *component);
int wsa883x_codec_get_dev_num(struct snd_soc_component *component);
#else
static int wsa883x_set_channel_map(struct snd_soc_component *component,
				   u8 *port, u8 num_port, unsigned int *ch_mask,
				   unsigned int *ch_rate, u8 *port_type)
{
	return 0;
}

static int wsa883x_codec_info_create_codec_entry(
					struct snd_info_entry *codec_root,
					struct snd_soc_component *component)
{
	return 0;
}

static int wsa883x_codec_get_dev_num(struct snd_soc_component *component)
{
	return 0;
}
#endif

#endif /* _WSA883X_H */
