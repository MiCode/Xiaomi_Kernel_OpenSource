/*
 * Copyright (C) 2018, SI-IN, Yun Shi (yun.shi@si-in.com).
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


#ifndef _SIPA_REGMAP_H
#define _SIPA_REGMAP_H

#include <linux/regmap.h>

/******************************************************************************
* Name: CRC-4/ITU x4+x+1
* Poly: 0x03
* Init: 0x00
* Refin: True
* Refout: True
* Xorout: 0x00
* Note:
*****************************************************************************/
static inline uint8_t crc4_itu(uint8_t *data, uint32_t length)
{
	uint8_t i;
	uint8_t crc = 0;						// Initial value

	while (length--) {
		crc ^= *data++;						// crc ^= *data; data++;
		for (i = 0; i < 8; ++i)	{
			if (crc & 1)
				crc = (crc >> 1) ^ 0x0C;	// 0x0C = (reverse 0x03)>>(8-4)
			else
				crc = (crc >> 1);
		}
	}

	return crc;
}

/******************************************************************************
 * Name:    CRC-8/MAXIM         x8+x5+x4+1
 * Poly:    0x31
 * Init:    0x00
 * Refin:   True
 * Refout:  True
 * Xorout:  0x00
 * Alias:   DOW-CRC,CRC-8/IBUTTON
 *****************************************************************************/
static inline uint8_t crc8_maxim(uint8_t *data, uint32_t length)
{
	uint8_t i;
	uint8_t crc = 0;						// Initial value

	while (length--) {
		crc ^= *data++;						// crc ^= *data; data++;
		for (i = 0; i < 8; i++) {
			if (crc & 1)
				crc = (crc >> 1) ^ 0x8C;	// 0x8C = reverse 0x31
			else
				crc >>= 1;
		}
	}

	return crc;
}

/******************************************************************************
 * Name:    CRC-16/MAXIM        x16+x15+x2+1
 * Poly:    0x8005
 * Init:    0x0000
 * Refin:   True
 * Refout:  True
 * Xorout:  0xFFFF
 * Note:
 *****************************************************************************/
static inline uint16_t crc16_maxim(uint8_t *data, uint32_t length)
{
	uint8_t i;
	uint16_t crc = 0;						// Initial value

	while (length--) {
		crc ^= *data++;						// crc ^= *data; data++;
		for (i = 0; i < 8; ++i) {
			if (crc & 1)
				crc = (crc >> 1) ^ 0xA001;	// 0xA001 = reverse 0x8005
			else
				crc = (crc >> 1);
		}
	}

	return ~crc;							// crc^0xffff
}

#ifdef PLATFORM_TYPE_QCOM
typedef  unsigned short uint16_t;
typedef  unsigned int   uint32_t;
typedef  unsigned char  uint8_t;
#endif

int sipa_regmap_read(struct regmap *regmap,	unsigned int chip_type,
	unsigned int start_reg,	unsigned int reg_num, void *buf);
int sipa_regmap_write(struct regmap *regmap,	unsigned int chip_type,
	unsigned int start_reg,	unsigned int reg_num, const void *buf);

struct regmap *sipa_regmap_init(struct i2c_client *client,
	unsigned int ch, unsigned int chip_type);
void sipa_regmap_remove(sipa_dev_t *si_pa);

/* option interface function */
int sipa_regmap_check_chip_id(struct regmap *regmap,
	unsigned int ch, unsigned int chip_type);
void sipa_regmap_defaults(struct regmap *regmap,
	unsigned int chip_type, unsigned int scene, unsigned int channel_num);
bool sipa_regmap_set_chip_on(sipa_dev_t *si_pa);
bool sipa_regmap_set_chip_off(sipa_dev_t *si_pa);
bool sipa_regmap_get_chip_en(sipa_dev_t *si_pa);

void sipa_regmap_set_pvdd_limit(
	struct regmap *regmap, uint32_t chip_type, uint32_t ch, unsigned int vol);

void sipa_regmap_check_trimming(sipa_dev_t *si_pa);

int sia91xx_read_reg16(sipa_dev_t *si_pa, unsigned char subaddress,
   unsigned short *val);
int sia91xx_write_reg16(sipa_dev_t *si_pa, unsigned char subaddr,
	unsigned short val);
int sia91xx_write_reg16_bit_pos(sipa_dev_t *si_pa,
	uint8_t subaddr, uint16_t Val, uint8_t pos, uint8_t len);
int sia91xx_write_reg16_bit_msk(sipa_dev_t *si_pa,
	uint8_t subaddr, uint16_t msk, bool stat);

int sipa_reg_init(struct sipa_dev_s *si_pa);

#endif /* _SIPA_REGMAP_H */

