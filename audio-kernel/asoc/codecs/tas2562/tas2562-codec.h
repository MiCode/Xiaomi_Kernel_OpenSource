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
**     tas2562-codec.h
**
** Description:
**     header file for tas2562-codec.c
**
** =============================================================================
*/

#ifndef _TAS2562_CODEC_H
#define _TAS2562_CODEC_H

#include "tas2562.h"

int tas2562_register_codec(struct tas2562_priv *pTAS2562);
int tas2562_deregister_codec(struct tas2562_priv *pTAS2562);
int tas2562_LoadConfig(struct tas2562_priv *pTAS2562);

#endif /* _TAS2562_CODEC_H */
