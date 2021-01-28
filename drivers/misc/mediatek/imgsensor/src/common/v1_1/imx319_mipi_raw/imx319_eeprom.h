/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __IMX319_EEPROM_H__
#define __IMX319_EEPROM_H__

#include "kd_camera_typedef.h"

/*
 * LRC
 *
 * @param data Buffer
 * @return size of data
 */
unsigned int read_imx319_LRC(BYTE *data);

/*
 * DCC
 *
 * @param data Buffer
 * @return size of data
 */
unsigned int read_imx319_DCC(BYTE *data);

#endif

