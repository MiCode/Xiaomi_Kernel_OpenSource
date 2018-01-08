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
**     tas2560-core.h
**
** Description:
**     header file for tas2560-core.c
**
** =============================================================================
*/

#ifndef _TAS2560_CORE_H
#define _TAS2560_CORE_H

#include "tas2560.h"

int tas2560_set_bit_rate(struct tas2560_priv *pTAS2560, unsigned int nBitRate);
int tas2560_get_bit_rate(struct tas2560_priv *pTAS2560);
int tas2560_set_ASI_fmt(struct tas2560_priv *pTAS2560, unsigned int fmt);
int tas2560_setLoad(struct tas2560_priv *pTAS2560, enum channel chn, int load);
int tas2560_getLoad(struct tas2560_priv *pTAS2560);
int tas2560_get_volume(struct tas2560_priv *pTAS2560);
int tas2560_set_volume(struct tas2560_priv *pTAS2560, int volume);
int tas2560_set_SampleRate(struct tas2560_priv *pTAS2560, unsigned int nSamplingRate);
int tas2560_setupPLL(struct tas2560_priv *pTAS2560, int pll_clkin);
int tas2560_set_pll_clkin(struct tas2560_priv *pTAS2560, int clk_id, unsigned int freq);
int tas2560_parse_dt(struct device *dev, struct tas2560_priv *pTAS2560);
void tas2560_dr_boost(struct tas2560_priv *pTAS2560);
void tas2560_sw_shutdown(struct tas2560_priv *pTAS2560, int sw_shutdown);
void tas2560_enable(struct tas2560_priv *pTAS2560, bool bEnable, enum channel echannel);
int spk_id_get(struct device_node *np);
int tas2560_irq_detect(struct tas2560_priv *pTAS2560);

#endif /* _TAS2560_CORE_H */
