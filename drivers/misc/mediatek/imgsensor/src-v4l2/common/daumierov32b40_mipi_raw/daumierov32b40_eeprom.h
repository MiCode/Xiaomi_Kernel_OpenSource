/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __DAUMIEROV32B40_EEPROM_H__
#define __DAUMIEROV32B40_EEPROM_H__

#include "kd_camera_typedef.h"

#include "adaptor-subdrv.h"

/*
 * LRC
 *
 * @param data Buffer
 * @return size of data
 */
unsigned int read_daumierov32b40_LRC(struct subdrv_ctx *ctx, u8 *data);

/*
 * DCC
 *
 * @param data Buffer
 * @return size of data
 */
unsigned int read_daumierov32b40_DCC(struct subdrv_ctx *ctx, u8 *data);

#endif

