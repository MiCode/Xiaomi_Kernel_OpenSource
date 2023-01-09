/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __IMX686spec25m_EEPROM_H__
#define __IMX686spec25m_EEPROM_H__

#include "kd_camera_typedef.h"

/*
 * LRC
 *
 * @param data Buffer
 * @return size of data
 */
unsigned int read_imx686spec25m_LRC(BYTE *data);

/*
 * DCC
 *
 * @param data Buffer
 * @return size of data
 */
unsigned int read_imx686spec25m_DCC(BYTE *data);

#endif

