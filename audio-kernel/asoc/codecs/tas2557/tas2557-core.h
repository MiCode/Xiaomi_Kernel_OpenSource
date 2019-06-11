/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
** Copyright (C) 2019 XiaoMi, Inc.
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
**     tas2557-core.h
**
** Description:
**     header file for tas2557-core.c
**
** =============================================================================
*/

#ifndef _TAS2557_CORE_H
#define _TAS2557_CORE_H

#include "tas2557.h"

#define TAS2557_YRAM_BOOK1				140

#define TAS2557_YRAM1_PAGE				42
#define TAS2557_YRAM1_START_REG			88
#define TAS2557_YRAM1_END_REG			127

#define TAS2557_YRAM2_START_PAGE		43
#define TAS2557_YRAM2_END_PAGE			49
#define TAS2557_YRAM2_START_REG			8
#define TAS2557_YRAM2_END_REG			127

#define TAS2557_YRAM3_PAGE				50
#define TAS2557_YRAM3_START_REG			8
#define TAS2557_YRAM3_END_REG			27

/* should not include B0_P53_R44-R47 */
#define TAS2557_YRAM_BOOK2				0
#define TAS2557_YRAM4_START_PAGE		50
#define TAS2557_YRAM4_END_PAGE			60
#define TAS2557_YRAM4_START_REG			8
#define TAS2557_YRAM4_END_REG			127

#define TAS2557_YRAM5_PAGE				61
#define TAS2557_YRAM5_START_REG			8
#define TAS2557_YRAM5_END_REG			27

#define TAS2557_COEFFICIENT_TMAX	0x7fffffff
#define TAS2557_SAFE_GUARD_PATTERN		0x5a
#define LOW_TEMPERATURE_CHECK_PERIOD 5000	/* 5 second */

struct TYCRC {
	unsigned char mnOffset;
	unsigned char mnLen;
};

int tas2557_enable(struct tas2557_priv *pTAS2557, bool bEnable);
int tas2557_permanent_mute(struct tas2557_priv *pTAS2557, bool bmute);
int tas2557_SA_DevChnSetup(struct tas2557_priv *pTAS2557, unsigned int mode);
int tas2557_get_die_temperature(struct tas2557_priv *pTAS2557, int *pTemperature);
int tas2557_set_sampling_rate(struct tas2557_priv *pTAS2557, unsigned int nSamplingRate);
int tas2557_set_bit_rate(struct tas2557_priv *pTAS2557, unsigned int nBitRate);
int tas2557_get_bit_rate(struct tas2557_priv *pTAS2557, unsigned char *pBitRate);
int tas2557_set_config(struct tas2557_priv *pTAS2557, int config);
void tas2557_fw_ready(const struct firmware *pFW, void *pContext);
bool tas2557_get_Cali_prm_r0(struct tas2557_priv *pTAS2557, int *prm_r0);
int tas2557_set_program(struct tas2557_priv *pTAS2557, unsigned int nProgram, int nConfig);
int tas2557_set_calibration(struct tas2557_priv *pTAS2557, int nCalibration);
int tas2557_load_default(struct tas2557_priv *pTAS2557);
int tas2557_parse_dt(struct device *dev, struct tas2557_priv *pTAS2557);
int tas2557_get_DAC_gain(struct tas2557_priv *pTAS2557, unsigned char *pnGain);
int tas2557_set_DAC_gain(struct tas2557_priv *pTAS2557, unsigned int nGain);
int tas2557_configIRQ(struct tas2557_priv *pTAS2557);
int spk_id_get(struct device_node *np);

#endif /* _TAS2557_CORE_H */
