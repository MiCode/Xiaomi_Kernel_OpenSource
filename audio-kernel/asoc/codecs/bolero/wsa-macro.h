/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef WSA_MACRO_H
#define WSA_MACRO_H

/*
 * Selects compander and smart boost settings
 * for a given speaker mode
 */
enum {
	WSA_MACRO_SPKR_MODE_DEFAULT,
	WSA_MACRO_SPKR_MODE_1, /* COMP Gain = 12dB, Smartboost Max = 5.5V */
};

/* Rx path gain offsets */
enum {
	WSA_MACRO_GAIN_OFFSET_M1P5_DB,
	WSA_MACRO_GAIN_OFFSET_0_DB,
};


#if IS_ENABLED(CONFIG_WSA_MACRO)
extern int wsa_macro_set_spkr_mode(struct snd_soc_codec *codec, int mode);
extern int wsa_macro_set_spkr_gain_offset(struct snd_soc_codec *codec,
					  int offset);
#else /* CONFIG_WSA_MACRO */
static inline int wsa_macro_set_spkr_mode(struct snd_soc_codec *codec, int mode)
{
	return 0;
}
static inline int wsa_macro_set_spkr_gain_offset(struct snd_soc_codec *codec,
						 int offset);
{
	return 0;
}
#endif /* CONFIG_WSA_MACRO */
#endif
