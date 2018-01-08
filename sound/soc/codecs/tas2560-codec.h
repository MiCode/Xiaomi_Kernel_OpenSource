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
**     tas2560-codec.h
**
** Description:
**     header file for tas2560-codec.c
**
** =============================================================================
*/

#ifndef _TAS2560_CODEC_H
#define _TAS2560_CODEC_H

#include "tas2560.h"

int tas2560_register_codec(struct tas2560_priv *pTAS2560);
int tas2560_deregister_codec(struct tas2560_priv *pTAS2560);

#endif /* _TAS2560_CODEC_H */
