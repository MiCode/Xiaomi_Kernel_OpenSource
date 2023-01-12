/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _ROULEUR_H
#define _ROULEUR_H

#define ROULEUR_DRV_NAME "rouleur_codec"

#ifdef CONFIG_SND_SOC_ROULEUR
extern int rouleur_info_create_codec_entry(struct snd_info_entry *codec_root,
				    struct snd_soc_component *component);
#else
extern int rouleur_info_create_codec_entry(struct snd_info_entry *codec_root,
				    struct snd_soc_component *component)
{
	return 0;
}
#endif /* CONFIG_SND_SOC_ROULEUR */

#endif
