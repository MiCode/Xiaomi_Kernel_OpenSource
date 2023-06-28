/*
 * Copyright (C) 2020, SI-IN, Yun Shi (yun.shi@si-in.com).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#ifndef _SIPA_PARAMETER_H
#define _SIPA_PARAMETER_H

#include <linux/device.h>
#include "sipa_parameter_typedef.h"

/******************************************************************************
 * Name:    CRC-32  x32+x26+x23+x22+x16+x12+x11+x10+x8+x7+x5+x4+x2+x+1
 * Poly:    0x4C11DB7
 * Init:    0xFFFFFFF
 * Refin:   True
 * Refout:  True
 * Xorout:  0xFFFFFFF
 * Alias:   CRC_32/ADCCP
 * Use:     WinRAR,ect.
 *****************************************************************************/
static inline uint32_t crc32(uint8_t *data, uint32_t length)
{
	uint8_t i;
	uint32_t crc = 0xffffffff;					// Initial value

	while (length--) {
		crc ^= *data++;							// crc ^= *data; data++;
		for (i = 0; i < 8; ++i) {
			if (crc & 1)
				crc = (crc >> 1) ^ 0xEDB88320;	// 0xEDB88320= reverse 0x04C11DB7
			else
				crc = (crc >> 1);
		}
	}

	return ~crc;
}

void sipa_param_load_fw(struct device *dev);
void sipa_param_release(void);

void *sipa_param_read_chip_cfg(
	uint32_t ch, uint32_t chip_type,
	SIPA_CHIP_CFG *cfg);

int sipa_param_read_extra_cfg(
	uint32_t ch, SIPA_EXTRA_CFG *cfg);

const SIPA_PARAM *sipa_param_instance(void);
bool sipa_param_is_loaded(void);
void sipa_param_print(void);

#endif

