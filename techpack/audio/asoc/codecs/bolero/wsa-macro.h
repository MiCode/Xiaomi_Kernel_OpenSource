/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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
extern int wsa_macro_set_spkr_mode(struct snd_soc_component *component,
				   int mode);
extern int wsa_macro_set_spkr_gain_offset(struct snd_soc_component *component,
					  int offset);
#else /* CONFIG_WSA_MACRO */
static inline int wsa_macro_set_spkr_mode(struct snd_soc_component *component,
					  int mode)
{
	return 0;
}
static inline int wsa_macro_set_spkr_gain_offset(
				struct snd_soc_component *component,
				int offset)
{
	return 0;
}
#endif /* CONFIG_WSA_MACRO */
#endif
