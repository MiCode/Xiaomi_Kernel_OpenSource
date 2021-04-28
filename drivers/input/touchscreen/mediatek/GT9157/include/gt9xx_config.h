/* Copyright  (C)  2010 - 2016 Goodix., Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * Version: V2.6
 */

#ifndef _GT9XX_CONFIG_H_
#define _GT9XX_CONFIG_H_

/* ***************************PART2:TODO */
/* define********************************** */
/* STEP_1(REQUIRED):Change config table. */
#define CTP_CFG_GROUP0                                                         \
	{                                                                      \
		0x48, 0xD0, 0x02, 0x00, 0x05, 0x05, 0xB4, 0xD2, 0x01, 0xC8,    \
			0x23, 0x0F, 0x55, 0x41, 0x03, 0x05, 0x00, 0x00, 0x00,  \
			0x00, 0x00, 0x00, 0x0A, 0x16, 0x16, 0x20, 0x14, 0x8A,  \
			0x2B, 0x0C, 0x71, 0x73, 0xB2, 0x04, 0xFF, 0xFC, 0x00,  \
			0x64, 0x33, 0x11, 0x00, 0x80, 0x00, 0x00, 0x14, 0x19,  \
			0x11, 0x51, 0x25, 0x12, 0x5A, 0x46, 0xAA, 0x94, 0xD5,  \
			0x95, 0x00, 0x00, 0x14, 0x04, 0x85, 0x4C, 0x00, 0x81,  \
			0x5B, 0x00, 0x80, 0x6D, 0x00, 0x7F, 0x82, 0x00, 0x80,  \
			0x9C, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  \
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  \
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A,  \
			0x0C, 0x00, 0x00, 0x1E, 0x46, 0x28, 0x00, 0x00, 0x00,  \
			0x00, 0x00, 0x00, 0x18, 0x16, 0x14, 0x12, 0x10, 0x0E,  \
			0x0C, 0x0A, 0x08, 0x06, 0x04, 0x02, 0xFF, 0xFF, 0x00,  \
			0x03, 0x14, 0x19, 0x04, 0x04, 0x21, 0x43, 0xB8, 0x0F,  \
			0x10, 0x0A, 0x19, 0x2A, 0x40, 0x04, 0x1F, 0x1E, 0x20,  \
			0x1D, 0x21, 0x1C, 0x22, 0x18, 0x24, 0x16, 0x26, 0x0A,  \
			0x00, 0x0C, 0x02, 0x0F, 0x04, 0x10, 0x06, 0x12, 0x08,  \
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,  \
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  \
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  \
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  \
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  \
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  \
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14,  \
			0x7D, 0x01                                             \
	}

#define GTP_CFG_GROUP0_CHARGER                                                 \
	{                                                                      \
	}

/* TODO puts your group2 config info here,if need. */
#define CTP_CFG_GROUP1                                                         \
	{                                                                      \
	}

/* TODO puts your group2 config info here,if need. */
#define GTP_CFG_GROUP1_CHARGER                                                 \
	{                                                                      \
	}

/* TODO puts your group3 config info here,if need. */
#define CTP_CFG_GROUP2                                                         \
	{                                                                      \
	}

/* TODO puts your group3 config info here,if need. */
#define GTP_CFG_GROUP2_CHARGER                                                 \
	{                                                                      \
	}

/* TODO: define your config for Sensor_ID == 3 here, if needed */
#define CTP_CFG_GROUP3                                                         \
	{                                                                      \
	}

/* TODO puts your group3 config info here,if need. */
#define GTP_CFG_GROUP3_CHARGER                                                 \
	{                                                                      \
	}

/* TODO: define your config for Sensor_ID == 4 here, if needed */
#define CTP_CFG_GROUP4                                                         \
	{                                                                      \
	}

/* TODO puts your group4 config info here,if need. */
#define GTP_CFG_GROUP4_CHARGER                                                 \
	{                                                                      \
	}

/* TODO: define your config for Sensor_ID == 5 here, if needed */
#define CTP_CFG_GROUP5                                                         \
	{                                                                      \
	}

/* TODO puts your group5 config info here,if need. */
#define GTP_CFG_GROUP5_CHARGER                                                 \
	{                                                                      \
	}

#endif /* _GT1X_CONFIG_H_ */
