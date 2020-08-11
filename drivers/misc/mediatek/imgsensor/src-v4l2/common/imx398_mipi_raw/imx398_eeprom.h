/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _IMX398_EEPROM_H
#define _IMX398_EEPROM_H

#include "adaptor-subdrv.h"

void read_imx398_DCC(struct subdrv_ctx *ctx,
		kal_uint16 addr, u8 *data, kal_uint32 size);

#endif
