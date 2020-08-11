/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __IMX350_EEPROM_H__
#define __IMX350_EEPROM_H__

#include "kd_camera_typedef.h"

#include "adaptor-subdrv.h"

void imx350_read_SPC(struct subdrv_ctx *ctx, u8 *data);
void imx350_read_DCC(struct subdrv_ctx *ctx,
		kal_uint16 addr, u8 *data, kal_uint32 size);

#endif

