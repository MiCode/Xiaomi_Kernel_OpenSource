/*
 * dbmdx-i2s.c  --  DBMDX I2S interface
 *
 * Copyright (C) 2014 DSP Group
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _DBMDX_I2S_H
#define _DBMDX_I2S_H

#include <sound/soc.h>

#define DBMDX_I2S_RATES			\
		(SNDRV_PCM_RATE_8000 |	\
		SNDRV_PCM_RATE_16000 |	\
		SNDRV_PCM_RATE_48000)

#define DBMDX_I2S_FORMATS		\
		(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S32_LE)

extern struct snd_soc_dai_ops dbmdx_i2s_dai_ops;

enum dbmdx_i2s_ports {
	DBMDX_I2S0 = 1,
	DBMDX_I2S1,
	DBMDX_I2S2,
	DBMDX_I2S3,
};

#endif
