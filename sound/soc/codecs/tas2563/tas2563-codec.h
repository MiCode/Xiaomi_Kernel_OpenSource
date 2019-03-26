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
** FOR A PARTICULAR PURPOSE.See the GNU General Public License for more details.
**
** File:
**     tas2563-codec.h
**
** Description:
**     header file for tas2563-codec.c
**
** =============================================================================
*/

#ifndef _TAS2563_CODEC_H
#define _TAS2563_CODEC_H

#include "tas2563.h"

int tas2563_register_codec(struct tas2563_priv *pTAS2563);
int tas2563_deregister_codec(struct tas2563_priv *pTAS2563);
int tas2563_set_program(struct tas2563_priv *pTAS2563, unsigned int nProgram, int nConfig);
int tas2563_LoadConfig(struct tas2563_priv *pTAS2563, bool bPowerOn);
void tas2563_fw_ready(const struct firmware *pFW, void *pContext);
int tas2563_set_config(struct tas2563_priv *pTAS2563, int config);

#endif /* _TAS2563_CODEC_H */
