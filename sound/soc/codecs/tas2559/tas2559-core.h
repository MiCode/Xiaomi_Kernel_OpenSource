/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
** Copyright (C) 2017 XiaoMi, Inc.
**
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software
** Foundation; version 2.
**
** This program is distributed in the hope that it will be useful, but WITHOUT
** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
**
** File:
**     tas2559-core.h
**
** Description:
**     header file for tas2559-core.c
**
** =============================================================================
*/

#ifndef _TAS2559_CORE_H
#define _TAS2559_CORE_H

#include "tas2559.h"

#define TAS2559_YRAM_BOOK1				140	/* 0x8c */

#define TAS2559_YRAM1_PAGE				42	/* 0x2a */
#define TAS2559_YRAM1_START_REG			88
#define TAS2559_YRAM1_END_REG			127

#define TAS2559_YRAM2_START_PAGE		43	/* 0x2b */
#define TAS2559_YRAM2_END_PAGE			55
#define TAS2559_YRAM2_START_REG			8
#define TAS2559_YRAM2_END_REG			127

#define TAS2559_YRAM3_PAGE				56	/* 0x38 */
#define TAS2559_YRAM3_START_REG			8
#define TAS2559_YRAM3_END_REG			47

/* should not include B0_P53_R44-R47 */
#define TAS2559_YRAM_BOOK2				0
#define TAS2559_YRAM4_START_PAGE		50	/* 0x32 */
#define TAS2559_YRAM4_END_PAGE			60	/* 0x3c */
#define TAS2559_YRAM4_START_REG			8
#define TAS2559_YRAM4_END_REG			127

#define TAS2559_YRAM5_PAGE				61	/* 0x3d */
#define TAS2559_YRAM5_START_REG			8
#define TAS2559_YRAM5_END_REG			27

#define TAS2559_YRAM_BOOK3				120	/* 0x78 */
#define TAS2559_YRAM6_PAGE				12	/* 0x0c */
#define TAS2559_YRAM6_START_REG			104	/* 0x68 */
#define TAS2559_YRAM6_END_REG			107	/* 0x6b */

#define TAS2559_SAFE_GUARD_PATTERN		0x5a
#define LOW_TEMPERATURE_CHECK_PERIOD	5000	/* 5 second */

struct TYCRC {
	unsigned char mnOffset;
	unsigned char mnLen;
};

int tas2559_DevMuteStatus(struct tas2559_priv *pTAS2559, enum channel dev, bool *pMute);
int tas2559_DevMute(struct tas2559_priv *pTAS2559, enum channel dev, bool mute);
int tas2559_SA_ctl_echoRef(struct tas2559_priv *pTAS2559);
int tas2559_set_VBoost(struct tas2559_priv *pTAS2559, int vboost, bool bPowerUp);
int tas2559_get_VBoost(struct tas2559_priv *pTAS2559, int *pVBoost);
int tas2559_SA_DevChnSetup(struct tas2559_priv *pTAS2559, unsigned int mode);
int tas2559_get_Cali_prm_r0(struct tas2559_priv *pTAS2559, enum channel chl, int *prm_r0);
int tas2559_get_die_temperature(struct tas2559_priv *pTAS2559, int *pTemperature);
int tas2559_enable(struct tas2559_priv *pTAS2559, bool bEnable);
int tas2559_set_sampling_rate(struct tas2559_priv *pTAS2559,
			      unsigned int nSamplingRate);
int tas2559_set_bit_rate(struct tas2559_priv *pTAS2559, unsigned int nBitRate);
int tas2559_get_bit_rate(struct tas2559_priv *pTAS2559, unsigned char *pBitRate);
int tas2559_set_config(struct tas2559_priv *pTAS2559, int config);
void tas2559_fw_ready(const struct firmware *pFW, void *pContext);
int tas2559_set_program(struct tas2559_priv *pTAS2559,
			unsigned int nProgram, int nConfig);
int tas2559_set_calibration(struct tas2559_priv *pTAS2559,
			    int nCalibration);
int tas2559_load_default(struct tas2559_priv *pTAS2559);
int tas2559_parse_dt(struct device *dev, struct tas2559_priv *pTAS2559);
int tas2559_get_DAC_gain(struct tas2559_priv *pTAS2559,
			 enum channel chl, unsigned char *pnGain);
int tas2559_set_DAC_gain(struct tas2559_priv *pTAS2559,
			 enum channel chl, unsigned int nGain);
int tas2559_configIRQ(struct tas2559_priv *pTAS2559, enum channel dev);

#endif /* _TAS2559_CORE_H */
