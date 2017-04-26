/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
 * Copyright (C) 2016 XiaoMi, Inc.
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
**     tas2555-codec.h
**
** Description:
**     header file for tas2555-codec.c
**
** =============================================================================
*/

#ifndef _TAS2555_CODEC_H
#define _TAS2555_CODEC_H

#include "tas2555.h"

extern int tas2555_register_codec(struct tas2555_priv *pTAS2555);
extern int tas2555_deregister_codec(struct tas2555_priv *pTAS2555);

#endif /* _TAS2555_CODEC_H */
