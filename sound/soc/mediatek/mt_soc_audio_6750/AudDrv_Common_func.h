/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/******************************************************************************
*
 *
 * Filename:
 * ---------
 *   AudDrv_Common_func.h
 *
 * Project:
 * --------
 *   MT6583 FPGA LDVT Audio Driver
 *
 * Description:
 * ------------
 *   Audio register
 *
 * Author:
 * -------
 *   George
 *
 *---------------------------------------------------------------------------
---
 *
 *

*******************************************************************************/

#ifndef AUDIO_COMMON_FUNC_H
#define AUDIO_COMMON_FUNC_H

bool get_voice_bt_status(void);
bool get_voice_status(void);
bool get_voice_md2_bt_status(void);
bool get_voice_md2_status(void);
void Auddrv_Read_Efuse_HPOffset(void);
bool get_internalmd_status(void);

#ifdef CONFIG_OF
int GetGPIO_Info(int type, int *pin, int *pinmode);
#endif

#endif
