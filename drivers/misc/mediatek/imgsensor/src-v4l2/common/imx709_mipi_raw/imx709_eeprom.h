/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 imx709o_eeprom.h
 *
 * Project:
 * --------
 * Description:
 * ------------
 *	 Add APIs to read from EEPROM
 *
 ****************************************************************************/
#ifndef __IMX709_EEPROM_H__
#define __IMX709_EEPROM_H__

#include "kd_camera_typedef.h"

#include "adaptor-subdrv.h"

#define EEPROM_READY 0	// FIX ME
#if EEPROM_READY

struct EEPROM_PDAF_INFO {
	kal_uint16 LRC_addr;
	unsigned int LRC_size;
};

/*
 * LRC
 *
 * @param data Buffer
 * @return size of data
 */
unsigned int read_imx709_LRC(struct subdrv_ctx *ctx, BYTE *data);

/*
 * DCC
 *
 * @param data Buffer
 * @return size of data
 */
unsigned int read_imx709_DCC(struct subdrv_ctx *ctx, BYTE *data);
#endif
#endif
