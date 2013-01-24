/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef MDSS_QPIC_PANEL_H
#define MDSS_QPIC_PANEL_H

#include <linux/list.h>
#include <mach/sps.h>

#include "mdss_panel.h"

#define LCDC_INTERNAL_BUFFER_SIZE   30

/**
   Macros for coding MIPI commands
*/
#define INV_SIZE             0xFFFF
/* Size of argument to MIPI command is variable */
#define OP_SIZE_PAIR(op, size)    ((op<<16) | size)
/* MIPI {command, argument size} tuple */
#define LCDC_EXTRACT_OP_SIZE(op_identifier)    ((op_identifier&0xFFFF))
/* extract size from command identifier */
#define LCDC_EXTRACT_OP_CMD(op_identifier)    (((op_identifier>>16)&0xFFFF))
/* extract command id from command identifier */


/* MIPI standard efinitions */
#define LCDC_ADDRESS_MODE_ORDER_BOTTOM_TO_TOP                0x80
#define LCDC_ADDRESS_MODE_ORDER_RIGHT_TO_LEFT                0x40
#define LCDC_ADDRESS_MODE_ORDER_REVERSE                      0x20
#define LCDC_ADDRESS_MODE_ORDER_REFRESH_BOTTOM_TO_TOP        0x10
#define LCDC_ADDRESS_MODE_ORDER_BGER_RGB                     0x08
#define LCDC_ADDRESS_MODE_ORDER_REFERESH_RIGHT_TO_LEFT       0x04
#define LCDC_ADDRESS_MODE_FLIP_HORIZONTAL                    0x02
#define LCDC_ADDRESS_MODE_FLIP_VERTICAL                      0x01

#define LCDC_PIXEL_FORMAT_3_BITS_PER_PIXEL    0x1
#define LCDC_PIXEL_FORMAT_8_BITS_PER_PIXEL    0x2
#define LCDC_PIXEL_FORMAT_12_BITS_PER_PIXEL   0x3
#define LCDC_PIXEL_FORMAT_16_BITS_PER_PIXEL   0x5
#define LCDC_PIXEL_FORMAT_18_BITS_PER_PIXEL   0x6
#define LCDC_PIXEL_FORMAT_24_BITS_PER_PIXEL   0x7

#define LCDC_CREATE_PIXEL_FORMAT(dpi_format, dbi_format) \
	(dpi_format | (dpi_format<<4))

#define POWER_MODE_IDLE_ON       0x40
#define POWER_MODE_PARTIAL_ON    0x20
#define POWER_MODE_SLEEP_ON      0x10
#define POWER_MODE_NORMAL_ON     0x08
#define POWER_MODE_DISPLAY_ON    0x04

#define LCDC_DISPLAY_MODE_SCROLLING_ON       0x80
#define LCDC_DISPLAY_MODE_INVERSION_ON       0x20
#define LCDC_DISPLAY_MODE_GAMMA_MASK         0x07

/**
 * LDCc MIPI Type B supported commands
 */
enum {
	OP_ENTER_IDLE_MODE        = OP_SIZE_PAIR(0x39, 0),
	OP_ENTER_INVERT_MODE      = OP_SIZE_PAIR(0x21, 0),
	OP_ENTER_NORMAL_MODE      = OP_SIZE_PAIR(0x13, 0),
	OP_ENTER_PARTIAL_MODE     = OP_SIZE_PAIR(0x12, 0),
	OP_ENTER_SLEEP_MODE       = OP_SIZE_PAIR(0x10, 0),
	OP_EXIT_INVERT_MODE       = OP_SIZE_PAIR(0x20, 0),
	OP_EXIT_SLEEP_MODE        = OP_SIZE_PAIR(0x11, 0),
	OP_EXIT_IDLE_MODE         = OP_SIZE_PAIR(0x38, 0),
	OP_GET_ADDRESS_MODE       = OP_SIZE_PAIR(0x0B, 1),
	OP_GET_BLUE_CHANNEL       = OP_SIZE_PAIR(0x08, 1),
	OP_GET_DIAGNOSTIC_RESULT  = OP_SIZE_PAIR(0x0F, 2),
	OP_GET_DISPLAY_MODE       = OP_SIZE_PAIR(0x0D, 1),
	OP_GET_GREEN_CHANNEL      = OP_SIZE_PAIR(0x07, 1),
	OP_GET_PIXEL_FORMAT       = OP_SIZE_PAIR(0x0C, 1),
	OP_GET_POWER_MODE         = OP_SIZE_PAIR(0x0A, 1),
	OP_GET_RED_CHANNEL        = OP_SIZE_PAIR(0x06, 1),
	OP_GET_SCANLINE           = OP_SIZE_PAIR(0x45, 2),
	OP_GET_SIGNAL_MODE        = OP_SIZE_PAIR(0x0E, 1),
	OP_NOP                    = OP_SIZE_PAIR(0x00, 0),
	OP_READ_DDB_CONTINUE      = OP_SIZE_PAIR(0xA8, INV_SIZE),
	OP_READ_DDB_START         = OP_SIZE_PAIR(0xA1, INV_SIZE),
	OP_READ_MEMORY_CONTINUE   = OP_SIZE_PAIR(0x3E, INV_SIZE),
	OP_READ_MEMORY_START      = OP_SIZE_PAIR(0x2E, INV_SIZE),
	OP_SET_ADDRESS_MODE       = OP_SIZE_PAIR(0x36, 1),
	OP_SET_COLUMN_ADDRESS     = OP_SIZE_PAIR(0x2A, 4),
	OP_SET_DISPLAY_OFF        = OP_SIZE_PAIR(0x28, 0),
	OP_SET_DISPLAY_ON         = OP_SIZE_PAIR(0x29, 0),
	OP_SET_GAMMA_CURVE        = OP_SIZE_PAIR(0x26, 1),
	OP_SET_PAGE_ADDRESS       = OP_SIZE_PAIR(0x2B, 4),
	OP_SET_PARTIAL_COLUMNS    = OP_SIZE_PAIR(0x31, 4),
	OP_SET_PARTIAL_ROWS       = OP_SIZE_PAIR(0x30, 4),
	OP_SET_PIXEL_FORMAT       = OP_SIZE_PAIR(0x3A, 1),
	OP_SOFT_RESET             = OP_SIZE_PAIR(0x01, 0),
	OP_WRITE_MEMORY_CONTINUE  = OP_SIZE_PAIR(0x3C, INV_SIZE),
	OP_WRITE_MEMORY_START     = OP_SIZE_PAIR(0x2C, INV_SIZE),
};

u32 qpic_panel_set_cmd_only(u32 command);
u32 qpic_send_panel_cmd(u32 cmd, u32 *val, u32 length);
int ili9341_on(void);
void ili9341_off(void);
int ili9341_init(struct platform_device *pdev,
			struct device_node *np);
#endif /* MDSS_QPIC_PANEL_H */
