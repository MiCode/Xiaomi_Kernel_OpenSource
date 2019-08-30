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
**     tas2559-codec.h
**
** Description:
**     header file for tas2559-codec.c
**
** =============================================================================
*/

#ifndef _TAS2559_CODEC_H
#define _TAS2559_CODEC_H

#include "tas2559.h"

int tas2559_register_codec(struct tas2559_priv *pTAS2559);
int tas2559_deregister_codec(struct tas2559_priv *pTAS2559);

#endif /* _TAS2559_CODEC_H */
