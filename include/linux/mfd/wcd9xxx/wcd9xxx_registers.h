/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#ifndef WCD9XXX_CODEC_DIGITAL_H

#define WCD9XXX_CODEC_DIGITAL_H

#define WCD9XXX_A_CHIP_CTL			(0x00)
#define WCD9XXX_A_CHIP_CTL__POR			(0x00000000)
#define WCD9XXX_A_CHIP_STATUS			(0x01)
#define WCD9XXX_A_CHIP_STATUS__POR			(0x00000000)
#define WCD9XXX_A_CHIP_ID_BYTE_0			(0x04)
#define WCD9XXX_A_CHIP_ID_BYTE_0__POR			(0x00000000)
#define WCD9XXX_A_CHIP_ID_BYTE_1			(0x05)
#define WCD9XXX_A_CHIP_ID_BYTE_1__POR			(0x00000000)
#define WCD9XXX_A_CHIP_ID_BYTE_2			(0x06)
#define WCD9XXX_A_CHIP_ID_BYTE_2__POR			(0x00000000)
#define WCD9XXX_A_CHIP_ID_BYTE_3			(0x07)
#define WCD9XXX_A_CHIP_ID_BYTE_3__POR			(0x00000001)
#define WCD9XXX_A_CHIP_VERSION			(0x08)
#define WCD9XXX_A_CHIP_VERSION__POR			(0x00000020)
#define WCD9XXX_A_SB_VERSION			(0x09)
#define WCD9XXX_A_SB_VERSION__POR			(0x00000010)
#define WCD9XXX_A_SLAVE_ID_1			(0x0C)
#define WCD9XXX_A_SLAVE_ID_1__POR			(0x00000077)
#define WCD9XXX_A_SLAVE_ID_2			(0x0D)
#define WCD9XXX_A_SLAVE_ID_2__POR			(0x00000066)
#define WCD9XXX_A_SLAVE_ID_3			(0x0E)
#define WCD9XXX_A_SLAVE_ID_3__POR			(0x00000055)
#define WCD9XXX_A_CDC_CTL			(0x80)
#define WCD9XXX_A_CDC_CTL__POR			(0x00000000)
#define WCD9XXX_A_LEAKAGE_CTL			(0x88)
#define WCD9XXX_A_LEAKAGE_CTL__POR			(0x00000004)
#endif
