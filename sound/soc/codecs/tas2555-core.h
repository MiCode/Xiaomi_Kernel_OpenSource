/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
** Copyright (C) 2016 XiaoMi, Inc.
**
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software
** Foundation; version 2.
**
** This program is distributed in the hope that it will be useful, but WITHOUT
** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License along with
** this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
** Street, Fifth Floor, Boston, MA 02110-1301, USA.
**
** File:
**     tas2555-core.h
**
** Description:
**     header file for tas2555-core.c
**
** =============================================================================
*/

#ifndef _TAS2555_CORE_H
#define _TAS2555_CORE_H

#include "tas2555.h"

extern void tas2555_enable(struct tas2555_priv *pTAS2555, bool bEnable);
extern int tas2555_set_sampling_rate(struct tas2555_priv *pTAS2555,
	unsigned int nSamplingRate);
extern int tas2555_set_configuration(struct tas2555_priv *pTAS2555, int config);
extern void tas2555_load_fs_firmware(struct tas2555_priv *pTAS2555,
	char *pFileName);
extern void tas2555_fw_ready(const struct firmware *pFW, void *pContext);
extern int tas2555_set_program(struct tas2555_priv *pTAS2555,
	unsigned int nProgram);
extern int tas2555_set_mode(struct tas2555_priv *pTAS2555, int mode);
extern int tas2555_set_calibration(struct tas2555_priv *pTAS2555,
	unsigned int nCalibration);
extern int tas2555_load_default(struct tas2555_priv *pTAS2555);
extern int tas2555_get_speaker_id(int pin);
extern int tas2555_set_pinctrl(struct tas2555_priv *pTAS2555, bool active);
extern int tas2555_set_mclk_rate(struct tas2555_priv *pTAS2555, unsigned long rate);
extern int tas2555_mclk_enable(struct tas2555_priv *pTAS2555, bool enable);

#endif /* _TAS2555_CORE_H */
