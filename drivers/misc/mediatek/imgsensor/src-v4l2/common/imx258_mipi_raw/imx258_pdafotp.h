/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __IMX258_PDAFOTP_H__
#define __IMX258_PDAFOTP_H__

#include "kd_camera_typedef.h"

#include "adaptor-subdrv.h"

bool read_imx258_pdaf(struct subdrv_ctx *ctx,
		kal_uint16 addr, BYTE *data, kal_uint32 size);

bool read_imx258_eeprom(struct subdrv_ctx *ctx,
		kal_uint16 addr, BYTE *data, kal_uint32 size);

bool read_imx258_eeprom_SPC(struct subdrv_ctx *ctx,
		kal_uint16 addr, BYTE *data, kal_uint32 size);

#endif

