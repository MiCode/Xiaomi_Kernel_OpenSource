/* Copyright (c) 2014, 2018 The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/reset.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <media/rc-core.h>
#include <linux/uaccess.h>

#define MSM_GENI_IR_DRIVER_NAME		"msm_geni_ir"
#define MSM_GENI_IR_RX_DEVICE_NAME	"msm_geni_ir_rx"
#define MSM_GENI_IR_TX_DEVICE_NAME	"msm_geni_ir_tx"

/* GENI Configuration registers */
#define IR_GENI_CLK_CTRL		0x0000
#define IR_GENI_FW_REVISION		0x0008
#define IR_GENI_S_FW_REVISION		0x000C
#define IR_GENI_FORCE_DEFAULT_REG	0x0010
#define IR_GENI_OUTPUT_CTRL		0x0014
#define IR_GENI_TEST_BUS_CTRL		0x0030
#define IR_GENI_SER_CLK_CFG		0x0034
#define IR_GENI_STOP_REG		0x0040
#define IR_GENI_CFG_REG(N)		(0x0100 + (0x4 * (N)))
#define IR_GENI_CFG_RAM(N)		(0x0200 + (0x4 * (N)))

/* GENI Data registers */
#define IR_GENI_M_CMD0			0x0C00
#define IR_GENI_S_CMD0			0x0C08
#define IR_GENI_IRQ_STATUS		0x0C20
#define IR_GENI_IRQ_ENABLE		0x0C24
#define IR_GENI_IRQ_CLEAR		0x0C28
#define IR_GENI_RX_FIFO(N)		(0x0C80 + (0x4 * (N)))
#define IR_GENI_RX_FIFO_STATUS		0x0CC4
#define IR_GENI_GP_OUTPUT_REG		0x0CCC
#define IR_GENI_RX_FIFO_STATUS_WC(X)	((X) & 0xF)

/* IR Configuration registers */
#define GENI_IR_CLK_MUX			0x0800
#define IR_RX_FILTER_VAL_MASK		0x0810
#define GENI_IR_AHB_MUX_SEL		0x0814
#define GENI_IR_RX_FILTER_EN		0x0818
#define GENI_IR_IRQ_STATUS		0x0820
#define GENI_IR_IRQ_ENABLE		0x0824
#define GENI_IR_IRQ_CLEAR		0x0828
#define GENI_IR_RX_FILTER_TABLE(N)	(0x0834 + (0x4 * (N)))

#define S_GENI_STOP                     0x2
#define M_GENI_STOP                     0x1

#define S_CMD_DONE                      0x2000
#define M_CMD_DONE                      0x1

#define RX_POL_LOW                      0x2

#define RX_SER_CLK_EN			0x1
#define TX_SER_CLK_EN			0x2
#define RX_CLK_DIV_VALUE(X)		(((X) & 0xFFF) << 2)
#define TX_CLK_DIV_VALUE(X)		(((X) & 0xFFF) << 14)

#define GENI_IR_RX_FIFO_WTRMRK_EN       0x400
#define GENI_IR_RX_NEC_REPEAT_DETECT    0x40000

#define GENI_IR_DEF_IRQ_EN              (GENI_IR_RX_FIFO_WTRMRK_EN | \
	GENI_IR_RX_NEC_REPEAT_DETECT)

#define GENI_IR_DEF_WAKEUP_MASK         0x00FFFFFF
#define GENI_IR_MAX_WAKEUP_CODES        10
#define RX_CLK_DIV                      0x258
#define TX_CLK_DIV                      0x58
#define RX_CLK_DIV_LP                   0x9
#define TX_BUF_SIZE                     256

struct geni_image {
	u32 reg;
	u32 val;
};

/* FW images */
static struct geni_image rc5_geni_rx_cfg_reg_image[] = {
	{ 7, 0x042F8101 }, { 8, 0x0000007F }, { 9, 0x0005C190 },
	{ 10, 0x0002E958 }, { 13, 0x00000000 }, { 14, 0x00000000 },
	{ 21, 0x00000300 }, { 22, 0x00000000 }, { 23, 0x00000000 },
	{ 24, 0x00000000 }, { 28, 0x00000200 }, { 30, 0x00000002 },
};

static struct geni_image rc5_geni_rx_cfg_ram_image[] = {
	{ 1, 0x00000001 }, { 34, 0x0004D000 }, { 35, 0x000E48C0 },
	{ 36, 0x00000000 }, { 37, 0x00000000 }, { 38, 0x000C0035 },
	{ 39, 0x00081C4C }, { 40, 0x0008044E }, { 41, 0x00080E52 },
	{ 42, 0x000C000A }, { 43, 0x00081C5A }, { 44, 0x000800EE },
	{ 45, 0x00080456 }, { 46, 0x000C000B }, { 47, 0x00081E96 },
	{ 48, 0x0008045E }, { 49, 0x000C8014 }, { 50, 0x00081E6A },
	{ 51, 0x00080464 }, { 52, 0x000800EE }, { 53, 0x000A026E },
	{ 54, 0x000800E2 }, { 55, 0x000C0006 }, { 56, 0x00081CEE },
	{ 57, 0x00080470 }, { 58, 0x000C000B }, { 59, 0x00081C88 },
	{ 60, 0x00080476 }, { 61, 0x000C9004 }, { 62, 0x00081CEE },
	{ 63, 0x0008047C }, { 64, 0x000C000C }, { 65, 0x00081CA4 },
	{ 66, 0x00080482 }, { 67, 0x000800EE }, { 68, 0x000C8006 },
	{ 69, 0x00081EEE }, { 70, 0x0008048A }, { 71, 0x000C000B },
	{ 72, 0x00081E6A }, { 73, 0x00080490 }, { 74, 0x000800EE },
	{ 75, 0x000C9006 }, { 76, 0x00081CEE }, { 77, 0x00080498 },
	{ 78, 0x000C000C }, { 79, 0x00081CA4 }, { 80, 0x0008049E },
	{ 81, 0x000800EE }, { 82, 0x000A02A8 }, { 83, 0x000800D0 },
	{ 84, 0x000C0006 }, { 85, 0x00081EEE }, { 86, 0x000804AA },
	{ 87, 0x000C000B }, { 88, 0x00081EC2 }, { 89, 0x000804B0 },
	{ 90, 0x000C8004 }, { 91, 0x00081EEE }, { 92, 0x000804B6 },
	{ 93, 0x000C000C }, { 94, 0x00081E6A }, { 95, 0x000804BC },
	{ 96, 0x000800EE }, { 97, 0x000C9006 }, { 98, 0x00081CEE },
	{ 99, 0x000804C4 }, { 100, 0x000C000B }, { 101, 0x00081CA4 },
	{ 102, 0x000804CA }, { 103, 0x000800EE }, { 104, 0x000C0006 },
	{ 105, 0x00081EEE }, { 106, 0x000804D2 }, { 107, 0x000C000A },
	{ 108, 0x00081EDE }, { 109, 0x000804D8 }, { 110, 0x000800EE },
	{ 111, 0x000C0023 }, { 112, 0x000800E4 }, { 113, 0x000C0033 },
	{ 114, 0x00081CEE }, { 115, 0x000804E4 }, { 116, 0x000E0C05 },
	{ 117, 0x000C040C }, { 118, 0x00080052 }, { 119, 0x000E0E01 },
	{ 120, 0x000C040C }, { 121, 0x0008004C }, { 122, 0x00000000 },
	{ 123, 0x00000000 }, { 124, 0x00000000 }, { 125, 0x00000000 },
	{ 126, 0x00000000 }, { 127, 0x00000000 }, { 128, 0x00000000 },
	{ 129, 0x00000000 }, { 130, 0x00000000 }, { 131, 0x00000000 },
	{ 132, 0x00000000 }, { 133, 0x00000000 }, { 134, 0x00000000 },
	{ 135, 0x00000000 }, { 136, 0x00000000 }, { 137, 0x00000000 },
	{ 138, 0x00000000 }, { 139, 0x00000000 }, { 140, 0x00000000 },
	{ 141, 0x00000000 }, { 142, 0x00000000 }, { 143, 0x00000000 },
	{ 144, 0x00000000 }, { 145, 0x00000000 }, { 146, 0x00000000 },
	{ 147, 0x00000000 }, { 148, 0x00000000 }, { 149, 0x00000000 },
	{ 150, 0x00000000 }, { 151, 0x00000000 }, { 152, 0x00000000 },
	{ 153, 0x00000000 }, { 154, 0x00000000 }, { 155, 0x00000000 },
	{ 156, 0x00000000 }, { 157, 0x00000000 }, { 158, 0x00000000 },
	{ 159, 0x00000000 }, { 160, 0x00000000 },
};

static struct geni_image rc5_geni_tx_cfg_reg_image[] = {
	{ 0, 0x200009F9 }, { 1, 0x000170C8 }, { 2, 0x0000BAAC },
	{ 3, 0x00066210 }, { 4, 0x000040BB }, { 5, 0x00080000 },
	{ 6, 0x00088100 }, { 11, 0x0000AAA0 }, { 12, 0x00000000 },
	{ 15, 0x00402000 }, { 16, 0x00000000 }, { 17, 0x00000000 },
	{ 18, 0x00000000 }, { 19, 0x00000000 }, { 20, 0x00000000 },
	{ 25, 0x00000000 }, { 26, 0x00000000 }, { 27, 0x000000E0 },
	{ 29, 0x00000024 },
};

static struct geni_image rc5_geni_tx_cfg_ram_image[] = {
	{ 0, 0x00000001 }, { 2, 0x00142000 }, { 3, 0x001E48C0 },
	{ 4, 0x00000000 }, { 5, 0x00000000 }, { 6, 0x00000000 },
	{ 7, 0x00000000 }, { 8, 0x00000000 }, { 9, 0x00000000 },
	{ 10, 0x00000000 }, { 11, 0x00000000 }, { 12, 0x00000000 },
	{ 13, 0x00000000 }, { 14, 0x00000000 }, { 15, 0x00000000 },
	{ 16, 0x00000000 }, { 17, 0x00000000 }, { 18, 0x00000000 },
	{ 19, 0x00000000 }, { 20, 0x00000000 }, { 21, 0x00000000 },
	{ 22, 0x00000000 }, { 23, 0x00000000 }, { 24, 0x00000000 },
	{ 25, 0x00000000 }, { 26, 0x00000000 }, { 27, 0x00000000 },
	{ 28, 0x00000000 }, { 29, 0x00000000 }, { 30, 0x00000000 },
	{ 31, 0x00000000 }, { 32, 0x00000000 }, { 33, 0x00000000 },
	{ 161, 0x000C301E }, { 162, 0x00082544 }, { 163, 0x000C601D },
	{ 164, 0x00085548 }, { 165, 0x0008675E }, { 166, 0x000C601E },
	{ 167, 0x0008554E }, { 168, 0x000C301B }, { 169, 0x00082552 },
	{ 170, 0x000A3358 }, { 171, 0x00083172 }, { 172, 0x0008475C },
	{ 173, 0x0008314C }, { 174, 0x00003000 }, { 175, 0x000C301E },
	{ 176, 0x00082560 }, { 177, 0x000C601B }, { 178, 0x00085564 },
	{ 179, 0x000A636A }, { 180, 0x00086170 }, { 181, 0x0008776E },
	{ 182, 0x0008614C }, { 183, 0x0008615E }, { 184, 0x00086174 },
	{ 185, 0x00003000 }, { 186, 0x000C3052 }, { 187, 0x00082576 },
	{ 188, 0x00103000 }, { 189, 0x00000000 },
};

static struct geni_image rc6_geni_rx_cfg_reg_image[] = {
	{ 7, 0x042F8101 }, { 8, 0x0000027F }, { 9, 0x0007FA10 },
	{ 10, 0x000C44FE }, { 13, 0x00000000 }, { 14, 0x00000000 },
	{ 21, 0x00000300 }, { 22, 0x00000000 }, { 23, 0x00000000 },
	{ 24, 0x00000000 }, { 28, 0x00000200 }, { 30, 0x00000002 },
};

static struct geni_image rc6_geni_rx_cfg_ram_image[] = {
	{ 1, 0x00000001 }, { 34, 0x0004D000 }, { 35, 0x000E48F0 },
	{ 36, 0x00000000 }, { 37, 0x00000000 }, { 38, 0x000C001E },
	{ 39, 0x00081C4C }, { 40, 0x0008044E }, { 41, 0x00080E52 },
	{ 42, 0x000C0022 }, { 43, 0x00081C5A }, { 44, 0x0008012A },
	{ 45, 0x00080456 }, { 46, 0x000C0014 }, { 47, 0x00081E64 },
	{ 48, 0x0008045E }, { 49, 0x0008012A }, { 50, 0x000C0012 },
	{ 51, 0x00081C6C }, { 52, 0x00080466 }, { 53, 0x0008012A },
	{ 54, 0x000C000A }, { 55, 0x00081E74 }, { 56, 0x0008046E },
	{ 57, 0x0008012A }, { 58, 0x000C0009 }, { 59, 0x00081E7A },
	{ 60, 0x0008012A }, { 61, 0x00080476 }, { 62, 0x000C0008 },
	{ 63, 0x00081C84 }, { 64, 0x0008047E }, { 65, 0x0008012A },
	{ 66, 0x000C000A }, { 67, 0x00081E8C }, { 68, 0x00080486 },
	{ 69, 0x0008012A }, { 70, 0x000C000A }, { 71, 0x00081C94 },
	{ 72, 0x0008048E }, { 73, 0x0008012A }, { 74, 0x000C000A },
	{ 75, 0x00081E9C }, { 76, 0x00080496 }, { 77, 0x0008012A },
	{ 78, 0x000C000A }, { 79, 0x00081CA4 }, { 80, 0x0008049E },
	{ 81, 0x0008012A }, { 82, 0x000C000A }, { 83, 0x00081EDE },
	{ 84, 0x000804A6 }, { 85, 0x000C9007 }, { 86, 0x00081CB0 },
	{ 87, 0x0008012A }, { 88, 0x000804AC }, { 89, 0x000C000A },
	{ 90, 0x00081EBA }, { 91, 0x000804B4 }, { 92, 0x0008012A },
	{ 93, 0x000C0011 }, { 94, 0x00081CD6 }, { 95, 0x000804BC },
	{ 96, 0x000C8008 }, { 97, 0x0008010C }, { 98, 0x000A02C8 },
	{ 99, 0x0008011E }, { 100, 0x000C0009 }, { 101, 0x00081CD6 },
	{ 102, 0x000804CA }, { 103, 0x000C8008 }, { 104, 0x00081CF8 },
	{ 105, 0x000804D0 }, { 106, 0x0008012A }, { 107, 0x000C900A },
	{ 108, 0x00081EC4 }, { 109, 0x000804D8 }, { 110, 0x0008012A },
	{ 111, 0x000C800A }, { 112, 0x00081EE4 }, { 113, 0x0008012A },
	{ 114, 0x000804E0 }, { 115, 0x000C0007 }, { 116, 0x00081CEE },
	{ 117, 0x000804E8 }, { 118, 0x0008012A }, { 119, 0x000C0011 },
	{ 120, 0x00081F0A }, { 121, 0x000804F0 }, { 122, 0x000C9008 },
	{ 123, 0x000800D8 }, { 124, 0x000A02FC }, { 125, 0x00080112 },
	{ 126, 0x000C0009 }, { 127, 0x00081F0A }, { 128, 0x000804FE },
	{ 129, 0x000C9008 }, { 130, 0x00081EC4 }, { 131, 0x00080504 },
	{ 132, 0x0008012A }, { 133, 0x000C800A }, { 134, 0x00081CF8 },
	{ 135, 0x0008050C }, { 136, 0x0008012A }, { 137, 0x000C0009 },
	{ 138, 0x00081F1A }, { 139, 0x00080514 }, { 140, 0x0008012A },
	{ 141, 0x000C001E }, { 142, 0x00080120 }, { 143, 0x000C0028 },
	{ 144, 0x00081D2A }, { 145, 0x00080520 }, { 146, 0x000E0C05 },
	{ 147, 0x000C040F }, { 148, 0x00080052 }, { 149, 0x000E0E01 },
	{ 150, 0x000C040F }, { 151, 0x0008004C }, { 152, 0x00000000 },
	{ 153, 0x00000000 }, { 154, 0x00000000 }, { 155, 0x00000000 },
	{ 156, 0x00000000 }, { 157, 0x00000000 }, { 158, 0x00000000 },
	{ 159, 0x00000000 }, { 160, 0x00000000 },
};

static struct geni_image rc6_geni_tx_cfg_reg_image[] = {
	{ 0, 0x200019F9 }, { 1, 0x0001FF08 }, { 2, 0x0000107F },
	{ 3, 0x0003F810 }, { 4, 0x000FFDFE }, { 5, 0x00880000 },
	{ 6, 0x00088100 }, { 11, 0x0000AAA0 }, { 12, 0x00000000 },
	{ 15, 0x00402000 }, { 16, 0x00000000 }, { 17, 0x00000000 },
	{ 18, 0x00000000 }, { 19, 0x00000000 }, { 20, 0x00000000 },
	{ 25, 0x00000000 }, { 26, 0x00000000 }, { 27, 0x000000E0 },
	{ 29, 0x00000024 },
};

static struct geni_image rc6_geni_tx_cfg_ram_image[] = {
	{ 0, 0x00000001 }, { 2, 0x00142000 }, { 3, 0x001E48F0 },
	{ 4, 0x00000000 }, { 5, 0x00000000 }, { 6, 0x00000000 },
	{ 7, 0x00000000 }, { 8, 0x00000000 }, { 9, 0x00000000 },
	{ 10, 0x00000000 }, { 11, 0x00000000 }, { 12, 0x00000000 },
	{ 13, 0x00000000 }, { 14, 0x00000000 }, { 15, 0x00000000 },
	{ 16, 0x00000000 }, { 17, 0x00000000 }, { 18, 0x00000000 },
	{ 19, 0x00000000 }, { 20, 0x00000000 }, { 21, 0x00000000 },
	{ 22, 0x00000000 }, { 23, 0x00000000 }, { 24, 0x00000000 },
	{ 25, 0x00000000 }, { 26, 0x00000000 }, { 27, 0x00000000 },
	{ 28, 0x00000000 }, { 29, 0x00000000 }, { 30, 0x00000000 },
	{ 31, 0x00000000 }, { 32, 0x00000000 }, { 33, 0x00000000 },
	{ 161, 0x000C605E }, { 162, 0x00085544 }, { 163, 0x000C301E },
	{ 164, 0x00082548 }, { 165, 0x00046E37 }, { 166, 0x00043E1F },
	{ 167, 0x00043E1F }, { 168, 0x00046E37 }, { 169, 0x00043E1F },
	{ 170, 0x00046E37 }, { 171, 0x00043E1F }, { 172, 0x00046E36 },
	{ 173, 0x00086774 }, { 174, 0x000C301E }, { 175, 0x0008255E },
	{ 176, 0x000C601D }, { 177, 0x00085562 }, { 178, 0x00087782 },
	{ 179, 0x00043E1F }, { 180, 0x00046E34 }, { 181, 0x000A636E },
	{ 182, 0x00086190 }, { 183, 0x00087772 }, { 184, 0x00086166 },
	{ 185, 0x00086182 }, { 186, 0x000C601E }, { 187, 0x00085576 },
	{ 188, 0x000C301C }, { 189, 0x0008257A }, { 190, 0x00084780 },
	{ 191, 0x00083166 }, { 192, 0x00003000 }, { 193, 0x00046E37 },
	{ 194, 0x00043E1C }, { 195, 0x000A338A }, { 196, 0x00083192 },
	{ 197, 0x0008478E }, { 198, 0x00083166 }, { 199, 0x00083182 },
	{ 200, 0x00086194 }, { 201, 0x00003000 }, { 202, 0x000C3067 },
	{ 203, 0x00082596 }, { 204, 0x00103000 }, { 205, 0x00000000 },
};

//{ HWIO_PHYS(GENI_IR(S_FW_REVISION)), 0x0280 },
static struct geni_image nec_geni_rx_cfg_reg_image[] = {
	{ 7, 0x042F8101 }, { 8, 0x0000007F }, { 9, 0x00083B0E },
	{ 10, 0x0000390E }, { 13, 0x00000000 }, { 14, 0x00000000 },
	{ 21, 0x00000180 }, { 22, 0x00000000 }, { 23, 0x00000000 },
	{ 24, 0x00000000 }, { 28, 0x00000100 }, { 30, 0x00000002 },
};

// Program ram
static struct geni_image nec_geni_rx_cfg_ram_image[] = {
	{ 1, 0x00000001 }, { 34, 0x0004D000 }, { 35, 0x000009F0 },
	{ 36, 0x00000000 }, { 37, 0x00000000 }, { 38, 0x000C005F },
	{ 39, 0x00081C4C }, { 40, 0x0008044E }, { 41, 0x00080E52 },
	{ 42, 0x000C005F }, { 43, 0x00081C5A }, { 44, 0x000800BA },
	{ 45, 0x00080456 }, { 46, 0x000C005F }, { 47, 0x00081E64 },
	{ 48, 0x0008045E }, { 49, 0x000800BA }, { 50, 0x000C0017 },
	{ 51, 0x00081CBA }, { 52, 0x00080466 }, { 53, 0x000C001D },
	{ 54, 0x00081C78 }, { 55, 0x0008046C }, { 56, 0x000C0020 },
	{ 57, 0x00081C84 }, { 58, 0x00080472 }, { 59, 0x000800BA },
	{ 60, 0x000C0010 }, { 61, 0x00081E80 }, { 62, 0x0008047A },
	{ 63, 0x000800BA }, { 64, 0x000E0D00 }, { 65, 0x0008004C },
	{ 66, 0x000C0010 }, { 67, 0x00081E8C }, { 68, 0x00080486 },
	{ 69, 0x000800BA }, { 70, 0x000C0010 }, { 71, 0x00081C9A },
	{ 72, 0x0008048E }, { 73, 0x000C0014 }, { 74, 0x00081CA0 },
	{ 75, 0x00080494 }, { 76, 0x000800CC }, { 77, 0x000A029E },
	{ 78, 0x000800A6 }, { 79, 0x00087084 }, { 80, 0x000A02A4 },
	{ 81, 0x000800B0 }, { 82, 0x00088084 }, { 83, 0x000C0010 },
	{ 84, 0x00081EAE }, { 85, 0x000804A8 }, { 86, 0x000800BA },
	{ 87, 0x000870C0 }, { 88, 0x000C0010 }, { 89, 0x00081EB8 },
	{ 90, 0x000804B2 }, { 91, 0x000800BA }, { 92, 0x000880C0 },
	{ 93, 0x000E0E01 }, { 94, 0x000C041F }, { 95, 0x0008004C },
	{ 96, 0x000C005E }, { 97, 0x00081CBA }, { 98, 0x000804C2 },
	{ 99, 0x000E0C05 }, { 100, 0x000C041F }, { 101, 0x00080052 },
	{ 102, 0x000E0E01 }, { 103, 0x000C041F }, { 104, 0x000C003B },
	{ 105, 0x00081C4C }, { 106, 0x000804D2 }, { 107, 0x00080052 },
	{ 108, 0x00000000 }, { 109, 0x00000000 }, { 110, 0x00000000 },
	{ 111, 0x00000000 }, { 112, 0x00000000 }, { 113, 0x00000000 },
	{ 114, 0x00000000 }, { 115, 0x00000000 }, { 116, 0x00000000 },
	{ 117, 0x00000000 }, { 118, 0x00000000 }, { 119, 0x00000000 },
	{ 120, 0x00000000 }, { 121, 0x00000000 }, { 122, 0x00000000 },
	{ 123, 0x00000000 }, { 124, 0x00000000 }, { 125, 0x00000000 },
	{ 126, 0x00000000 }, { 127, 0x00000000 }, { 128, 0x00000000 },
	{ 129, 0x00000000 }, { 130, 0x00000000 }, { 131, 0x00000000 },
	{ 132, 0x00000000 }, { 133, 0x00000000 }, { 134, 0x00000000 },
	{ 135, 0x00000000 }, { 136, 0x00000000 }, { 137, 0x00000000 },
	{ 138, 0x00000000 }, { 139, 0x00000000 }, { 140, 0x00000000 },
	{ 141, 0x00000000 }, { 142, 0x00000000 }, { 143, 0x00000000 },
	{ 144, 0x00000000 }, { 145, 0x00000000 }, { 146, 0x00000000 },
	{ 147, 0x00000000 }, { 148, 0x00000000 }, { 149, 0x00000000 },
	{ 150, 0x00000000 }, { 151, 0x00000000 }, { 152, 0x00000000 },
	{ 153, 0x00000000 }, { 154, 0x00000000 }, { 155, 0x00000000 },
	{ 156, 0x00000000 }, { 157, 0x00000000 }, { 158, 0x00000000 },
	{ 159, 0x00000000 }, { 160, 0x00000000 },
};
/* IR controller modes */
enum geni_ir_mode {
	GENI_IR_NORMAL_MODE	= 0,
	GENI_IR_LOW_POWER_MODE	= 1,	/* RX filter is the source */
};

/* Firmware image */
struct msm_geni_fw_data {
	u32                      rx_revision;
	struct geni_image       *rx_cfg_data; /* Configuration data */
	u32                      rx_cfg_len;
	struct geni_image       *rx_ram_data; /* RAM data */
	u32                      rx_ram_len;

	u32                      tx_revision;
	struct geni_image       *tx_cfg_data; /* Configuration data */
	u32                      tx_cfg_len;
	struct geni_image       *tx_ram_data; /* RAM data */
	u32                      tx_ram_len;
};

struct msm_geni_ir {
	struct device		*dev;
	struct rc_dev		*rcdev;

	struct mutex             lock;
	void __iomem		*base;

	unsigned int             gpio_rx;

	struct clk              *ahb_clk;
	struct clk		*serial_clk;
	struct reset_control    *reset_core;
	unsigned int             irq;
	unsigned int             wakeup_irq;

	const struct msm_geni_fw_data *image_requested;
	const struct msm_geni_fw_data *image_loaded;

#ifdef CONFIG_IR_MSM_GENI_TX
	struct miscdevice        misc;
	u32                      tx_data[TX_BUF_SIZE];

	unsigned int             gpio_tx;
#endif
	u32                      wakeup_mask;
	u32                      rx_data;
	u32                      wakeup_codes[GENI_IR_MAX_WAKEUP_CODES];
	u8                       num_wakeup_codes;
};

static struct msm_geni_fw_data rc5_geni_image = {
	.rx_revision    = 0x180,
	.rx_cfg_data	= rc5_geni_rx_cfg_reg_image,
	.rx_cfg_len	= ARRAY_SIZE(rc5_geni_rx_cfg_reg_image),
	.rx_ram_data	= rc5_geni_rx_cfg_ram_image,
	.rx_ram_len	= ARRAY_SIZE(rc5_geni_rx_cfg_ram_image),

	.tx_revision    = 0x180,
	.tx_cfg_data	= rc5_geni_tx_cfg_reg_image,
	.tx_cfg_len	= ARRAY_SIZE(rc5_geni_tx_cfg_reg_image),
	.tx_ram_data	= rc5_geni_tx_cfg_ram_image,
	.tx_ram_len	= ARRAY_SIZE(rc5_geni_tx_cfg_ram_image),
};

static struct msm_geni_fw_data rc6_geni_image = {
	.rx_revision    = 0x1c0,
	.rx_cfg_data	= rc6_geni_rx_cfg_reg_image,
	.rx_cfg_len	= ARRAY_SIZE(rc6_geni_rx_cfg_reg_image),
	.rx_ram_data	= rc6_geni_rx_cfg_ram_image,
	.rx_ram_len	= ARRAY_SIZE(rc6_geni_rx_cfg_ram_image),

	.tx_revision    = 0x1c0,
	.tx_cfg_data	= rc6_geni_tx_cfg_reg_image,
	.tx_cfg_len	= ARRAY_SIZE(rc6_geni_tx_cfg_reg_image),
	.tx_ram_data	= rc6_geni_tx_cfg_ram_image,
	.tx_ram_len	= ARRAY_SIZE(rc6_geni_tx_cfg_ram_image),
};

static struct msm_geni_fw_data nec_geni_image = {
	.rx_revision    = 0x0280,
	.rx_cfg_data    = nec_geni_rx_cfg_reg_image,
	.rx_cfg_len     = ARRAY_SIZE(nec_geni_rx_cfg_reg_image),
	.rx_ram_data    = nec_geni_rx_cfg_ram_image,
	.rx_ram_len     = ARRAY_SIZE(nec_geni_rx_cfg_ram_image),
};

/* loads the GENI ir firmware */
static void msm_geni_ir_load_firmware(struct msm_geni_ir *ir)
{
	const struct msm_geni_fw_data *fw = ir->image_requested;
	struct geni_image *img;
	u32 i, clk_cfg;

	pr_debug("protocol Load Firmware\n");

	/* enable High-Z on GENI output pad */
	writel_relaxed(0x0, ir->base + IR_GENI_OUTPUT_CTRL);

	/* switch program-ram clock muxing to ahb clock */
	writel_relaxed(0x0, ir->base + IR_GENI_CLK_CTRL);

	/* write firmware revisions */
	writel_relaxed(fw->tx_revision, ir->base + IR_GENI_FW_REVISION);

	writel_relaxed(fw->rx_revision, ir->base + IR_GENI_S_FW_REVISION);

	/* write firmware image */
	img = fw->rx_cfg_data;
	for (i = 0; i < fw->rx_cfg_len; i++, img++)
		writel_relaxed(img->val, ir->base + IR_GENI_CFG_REG(img->reg));

	img = fw->rx_ram_data;
	for (i = 0; i < fw->rx_ram_len; i++, img++)
		writel_relaxed(img->val, ir->base + IR_GENI_CFG_RAM(img->reg));

#ifdef CONFIG_IR_MSM_GENI_TX

	img = fw->tx_cfg_data;
	for (i = 0; i < fw->tx_cfg_len; i++, img++)
		writel_relaxed(img->val, ir->base + IR_GENI_CFG_REG(img->reg));

	img = fw->tx_ram_data;
	for (i = 0; i < fw->tx_ram_len; i++, img++)
		writel_relaxed(img->val, ir->base + IR_GENI_CFG_RAM(img->reg));
#endif

	/* force default values on GENI output pad */
	writel_relaxed(0x1, ir->base + IR_GENI_FORCE_DEFAULT_REG);

	/* switch program-ram clock muxing to serial clock */
	writel_relaxed(0x1, ir->base + IR_GENI_CLK_CTRL);

	/* disable High-Z on GENI output pad */
	writel_relaxed(0x3, ir->base + IR_GENI_OUTPUT_CTRL);
	/*write memory barrier*/
	wmb();

	/* configure serial clock */
	clk_cfg = RX_CLK_DIV_VALUE(RX_CLK_DIV) | RX_SER_CLK_EN;
	clk_cfg |= (TX_CLK_DIV_VALUE(TX_CLK_DIV) | TX_SER_CLK_EN);
	//writel_relaxed(clk_cfg, ir->base + IR_GENI_SER_CLK_CFG);
	writel_relaxed(0x961, ir->base + IR_GENI_SER_CLK_CFG);

	/* set rx polarization to active low */
	writel_relaxed(RX_POL_LOW, ir->base + IR_GENI_GP_OUTPUT_REG);
	/*write memory barrier*/
	wmb();

	/* enable RX */
	writel_relaxed(0, ir->base + IR_GENI_S_CMD0);

	writel_relaxed(0x0A, ir->base + IR_GENI_TEST_BUS_CTRL);

	/* enable interrupts */
	writel_relaxed(GENI_IR_DEF_IRQ_EN, ir->base + IR_GENI_IRQ_ENABLE);
	/*write memory barrier*/
	wmb();

	pr_debug("Load Firmware done\n");
}
EXPORT_SYMBOL(msm_geni_ir_load_firmware);

/* stop GENI IR */
static void msm_geni_ir_stop(struct msm_geni_ir *ir)
{
	u32 i, status, cnt;
	int rc;

	/* stop main and secondary sequencer */
	writel_relaxed((S_GENI_STOP | M_GENI_STOP),
		       ir->base + IR_GENI_STOP_REG);

	/* poll every 1000us till 10ms for S_CMD_DONE bit to set */
	rc = readl_poll_timeout((ir->base + IR_GENI_IRQ_STATUS), status,
				(status & S_CMD_DONE), 100, 1000);
	if (rc)
		dev_err(ir->dev, "failed to stop\n");

	writel_relaxed((status & ~M_CMD_DONE), ir->base + IR_GENI_IRQ_CLEAR);
	/*write memory barrier*/
	wmb();

	/* flush the RX FIFO */
	status = readl_relaxed(ir->base + IR_GENI_RX_FIFO_STATUS);
	cnt = IR_GENI_RX_FIFO_STATUS_WC(status);
	for (i = 0; i < cnt; ++i)
		readl_relaxed(ir->base + IR_GENI_RX_FIFO(i));
}

/* sets the core for the specified protocol */
static int msm_geni_ir_change_protocol(struct rc_dev *dev, u64 *rc_type)
{
	struct msm_geni_ir *ir = dev->priv;
	const struct msm_geni_fw_data *image = NULL;
	int rc;

	pr_debug("protocol 0x%llx\n", *rc_type);
	/* select either rc6, rc5 or unknown (disable) in that order */
	if (*rc_type & RC_PROTO_BIT_RC6_0) {
		pr_debug("Loading RC6\n");
		image = &rc6_geni_image;
		*rc_type = RC_PROTO_RC6_0;
	} else if (*rc_type & RC_PROTO_BIT_RC5) {
		pr_debug("Loading RC5\n");
		image = &rc5_geni_image;
		*rc_type = RC_PROTO_RC5;
	} else if (*rc_type & RC_PROTO_BIT_NEC) {
		pr_debug("Loading NEC\n");
		image = &nec_geni_image;
		*rc_type = RC_PROTO_NEC;
	} else if (*rc_type & RC_PROTO_BIT_UNKNOWN) {
		pr_debug("Unknown proto\n");
		image = NULL;
		*rc_type = RC_PROTO_UNKNOWN;
	} else {
		pr_debug("Invalid proto\n");
		return -EINVAL;
	}

	if (image == ir->image_requested)
		return 0;

	ir->image_requested = image;

	mutex_lock(&ir->lock);

	if (image) {
		if (ir->image_loaded == NULL) {
			pr_debug("enable ahb clk\n");
			rc = clk_prepare_enable(ir->ahb_clk);
			if (rc) {
				pr_err("ahb clk enable failed %d\n", rc);
				mutex_unlock(&ir->lock);
				return rc;
			}
			pr_debug("enable serial clk\n");
			rc = clk_prepare_enable(ir->serial_clk);
			if (rc) {
				pr_err("serial clk enable failed %d\n", rc);
				mutex_unlock(&ir->lock);
				return rc;
		}
		} else {
			pr_debug("Disable interrupts\n");
			/* disable interrupts */
			writel_relaxed(0, ir->base + IR_GENI_IRQ_ENABLE);
			/*write memory barrier*/
			wmb();
			synchronize_irq(ir->irq);
		}

		/* load requested fw */
		msm_geni_ir_load_firmware(ir);
	} else {
		/* stop GENI IR */
		pr_debug("Stop GENI IR\n");
		msm_geni_ir_stop(ir);

		clk_disable_unprepare(ir->ahb_clk);
		clk_disable_unprepare(ir->serial_clk);
	}

	ir->image_loaded = image;
	mutex_unlock(&ir->lock);

	pr_debug("protocol 0x%llx\n", *rc_type);
	return 0;
}
EXPORT_SYMBOL(msm_geni_ir_change_protocol);

static irqreturn_t geni_ir_wakeup_handler(int irq, void *data)
{
	pr_debug("%s:Received wake up Interrupt\n", __func__);
	return IRQ_HANDLED;
}

/* interrupt routine */
static irqreturn_t geni_ir_interrupt(int irq, void *data)
{
	struct msm_geni_ir *ir = data;
	u32 i, irq_sts, fifo_sts, cnt;

	pr_debug("Received Interrupt\n");
	/* read irq status */
	irq_sts = readl_relaxed(ir->base + IR_GENI_IRQ_STATUS);
	pr_debug("irq_status 0x%x\n", irq_sts);

	/* read RX fifo */
	fifo_sts = readl_relaxed(ir->base + IR_GENI_RX_FIFO_STATUS);
	pr_debug("status - irq 0x%x fifo 0x%x\n", irq_sts, fifo_sts);

	cnt = IR_GENI_RX_FIFO_STATUS_WC(fifo_sts);
	for (i = 0; i < cnt; i++) {
		u8 command, system, toggle = 0, address1, address2;
		u32 scancode;

		ir->rx_data = readl_relaxed(ir->base + IR_GENI_RX_FIFO(i));
		if (ir->image_loaded == &rc5_geni_image) {
			/* RC5 */
			command  = (ir->rx_data & 0x00003F);
			system   = (ir->rx_data & 0x0007C0) >> 6;
			toggle   = (ir->rx_data & 0x000800) ? 1 : 0;
			command += (ir->rx_data & 0x001000) ? 0 : 0x40;
			scancode = system << 8 | command;
			rc_keydown(ir->rcdev, RC_PROTO_RC5, scancode, toggle);
		} else if (ir->image_loaded == &rc6_geni_image) {
			/* RC6 */
			scancode = (ir->rx_data & 0x00FFFF);
			toggle   = (ir->rx_data & 0x010000) ? 1 : 0;
			rc_keydown(ir->rcdev, RC_PROTO_RC6_0, scancode, toggle);
		} else {
			/*NEC*/
			scancode = (ir->rx_data & 0x0000FF);
			scancode = ~scancode;
			scancode = (scancode & 0x0000FF);
			address2 = (ir->rx_data & 0x00FF0000);
			address1 = (ir->rx_data & 0xFF000000);
			rc_keydown(ir->rcdev, RC_PROTO_NEC, scancode, 0);
		}

		pr_debug("rcvd code 0x%x scancode 0x%x toggle %d\n",
			ir->rx_data, scancode, toggle);

	}
	if ((irq_sts & GENI_IR_RX_NEC_REPEAT_DETECT) &&
		(ir->image_loaded == &nec_geni_image)) {
		u32 nec_scancode;
			nec_scancode = (ir->rx_data & 0x0000FF);
			nec_scancode = ~nec_scancode;
			nec_scancode = (nec_scancode & 0x0000FF);
			rc_keydown(ir->rcdev, RC_PROTO_NEC, nec_scancode, 0);
	}
	/* ack irq */
	writel_relaxed((irq_sts & ~M_CMD_DONE), ir->base + IR_GENI_IRQ_CLEAR);
	/*write memory barrier*/
	wmb();

	return IRQ_HANDLED;
}

#ifdef CONFIG_IR_MSM_GENI_TX
/* transmit IR code */
static int msm_geni_ir_tx_code(struct msm_geni_ir *ir, u32 code)
{
	u32 val;
	int rc;

	/* write code to be transmitted */
	writel_relaxed(code, ir->base + IR_GENI_M_CMD0);

	/* poll every 10ms till 100ms for M_CMD_DONE bit to set */
	rc = readl_poll_timeout((ir->base + IR_GENI_IRQ_STATUS), val,
				(val & M_CMD_DONE), 10000, 100000);
	if (rc) {
		pr_err("failed to transmit code 0x%x\n", code);
		return rc;
	}

	/* clear M_CMD_DONE interrupt */
	writel_relaxed(M_CMD_DONE, ir->base + IR_GENI_IRQ_CLEAR);
	/*write memory barrier*/
	wmb();

	dev_debug(ir->dev, "transmitted code 0x%x\n", code);

	return 0;
}

/* receive IR codes to transmit and transmit them */
static ssize_t msm_geni_ir_tx_write(struct file *fp, const char __user *buf,
				    size_t count, loff_t *pos)
{
	struct msm_geni_ir *ir = container_of(fp->private_data,
					      struct msm_geni_ir, misc);
	u8 i, cnt = 0;
	int rc;

	if (count % sizeof(u32)) {
		pr_err("invalid no of bytes %d\n", count);
		return -EINVAL;
	}

	/* process a maximum of TX_BUF_SIZE bytes at a time */
	count = count > TX_BUF_SIZE ? TX_BUF_SIZE : count;
	rc = copy_from_user(ir->tx_data, buf, count);
	if (rc) {
		pr_err("could not copy %i bytes\n", rc);
		return -EINVAL;
	}

	mutex_lock(&ir->lock);
	if (ir->image_loaded == NULL) {
		mutex_unlock(&ir->lock);
		return -ENODEV;
	}

	cnt = count / sizeof(u32);
	for (i = 0; i < cnt; i++)
		msm_geni_ir_tx_code(ir, ir->tx_data[i]);

	mutex_unlock(&ir->lock);
	return count;
}

static int msm_geni_ir_tx_open(struct inode *ip, struct file *fp)
{
	return 0;
}

static const struct file_operations msm_geni_ir_tx_fops = {
	.owner = THIS_MODULE,
	.write = msm_geni_ir_tx_write,
	.open = msm_geni_ir_tx_open,
};
#endif

/* get resources */
static int msm_geni_ir_get_res(struct platform_device *pdev,
			       struct msm_geni_ir *ir)
{
	struct device_node *node = pdev->dev.of_node;
	struct resource *res;
	int rc;

#ifdef CONFIG_IR_MSM_GENI_TX
	rc = of_get_named_gpio(node, "qcom,geni-ir-gpio-tx", 0);
	if (rc < 0) {
		pr_err("could not get tx gpio %d\n", rc);
		return rc;
	}

	ir->gpio_tx = rc;
#endif

	rc = of_get_named_gpio(node, "qcom,geni-ir-wakeup-gpio", 0);
	if (rc < 0) {
		pr_err("could not get wakeup gpio %d\n", rc);
		return rc;
	}

	ir->wakeup_irq = gpio_to_irq(rc);
	rc = platform_get_irq_byname(pdev, "geni-ir-core-irq");
	if (rc < 0) {
		pr_err("could not get core irq %d\n", rc);
		return rc;
	}

	ir->irq = rc;
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base");
	if (!res) {
		pr_err("missing memory resource\n");
		return -EINVAL;
	}
	ir->base = ioremap(res->start, resource_size(res));
	if (!ir->base) {
		pr_err("ioremap failed\n");
		return -ENOMEM;
	}
	pr_debug("ir->base: 0x%lx\n", (unsigned long int)ir->base);
	ir->ahb_clk = clk_get(&pdev->dev, "iface_clk");
	ir->serial_clk = clk_get(&pdev->dev, "serial_clk");
	if (IS_ERR(ir->ahb_clk)) {
		rc = PTR_ERR(ir->ahb_clk);
		pr_err("could not get ahb clk %d\n", rc);
		iounmap(ir->base);
		return rc;
	}
	return 0;
}

/* release resources */
static void msm_geni_ir_rel_res(struct platform_device *pdev,
				struct msm_geni_ir *ir)
{
	if (ir->ahb_clk)
		clk_put(ir->ahb_clk);

	if (ir->base)
		iounmap(ir->base);
}

int msm_geni_ir_probe(struct platform_device *pdev)
{
	struct rc_dev *rcdev;
	struct msm_geni_ir *ir;
	int rc;

	ir = kzalloc(sizeof(struct msm_geni_ir), GFP_KERNEL);
	if (!ir)
		return -ENOMEM;

	platform_set_drvdata(pdev, ir);
	rc = msm_geni_ir_get_res(pdev, ir);
	if (rc)
		goto resource_err;

#ifdef CONFIG_IR_MSM_GENI_TX
	rc = gpio_request(ir->gpio_tx, "qcom,geni-ir-gpio-tx");
	if (rc) {
		pr_err("tx gpio request failed %d\n", rc);
		goto gpio_err;
	}
#endif
	rc = gpio_request(ir->gpio_rx, "qcom,geni-ir-gpio-rx");
	if (rc) {
		pr_err("rx gpio request failed %d\n", rc);
		goto gpio_err;
	}
	rc = request_irq(ir->irq, geni_ir_interrupt,
			 IRQ_TYPE_LEVEL_HIGH, "geni-ir-core-irq", ir);
	if (rc) {
		pr_err("core irq request failed %d\n", rc);
		goto core_irq_err;
	}
	rc = request_irq(ir->wakeup_irq, geni_ir_wakeup_handler,
		IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT
		| IRQF_NO_SUSPEND, "geni-ir-wakeup-irq", ir);
	if (rc) {
		pr_err("wakeup irq request failed %d\n", rc);
		goto rc_alloc_err;
	}

	mutex_init(&ir->lock);
	rcdev = rc_allocate_device(RC_DRIVER_SCANCODE);
	if (!rcdev) {
		pr_err("failed to allocate rc device\n");
		rc = -ENOMEM;
		goto rc_alloc_err;
	}

	rcdev->priv = ir;
	rcdev->driver_type = RC_DRIVER_SCANCODE;
	rcdev->allowed_protocols = RC_PROTO_RC5 | RC_PROTO_RC6_0 | RC_PROTO_NEC;
	rcdev->enabled_protocols = RC_PROTO_RC5 | RC_PROTO_RC6_0 | RC_PROTO_NEC;
	rcdev->driver_name = MSM_GENI_IR_DRIVER_NAME;
	rcdev->device_name = MSM_GENI_IR_RX_DEVICE_NAME;
	rcdev->change_protocol = msm_geni_ir_change_protocol;
	rcdev->input_id.bustype = BUS_HOST;

	rcdev->map_name = RC_MAP_EMPTY;
	rcdev->dev.parent = &pdev->dev;
	ir->dev = &pdev->dev;
	ir->rcdev = rcdev;

	rc = rc_register_device(rcdev);
	if (rc < 0) {
		pr_err("failed to register rc device %d\n", rc);
		goto rc_register_err;
	}


#ifdef CONFIG_IR_MSM_GENI_TX
	ir->misc.minor = MISC_DYNAMIC_MINOR;
	ir->misc.name = MSM_GENI_IR_TX_DEVICE_NAME;
	ir->misc.fops = &msm_geni_ir_tx_fops;
	ir->misc.parent = &pdev->dev;
#endif

	return 0;

rc_register_err:
	rc_free_device(rcdev);
	mutex_destroy(&ir->lock);
rc_alloc_err:
	irq_set_irq_wake(ir->wakeup_irq, 0);
core_irq_err:
#ifdef CONFIG_IR_MSM_GENI_TX
	gpio_free(ir->gpio_tx);
#endif
	gpio_free(ir->gpio_rx);
gpio_err:
	msm_geni_ir_rel_res(pdev, ir);
resource_err:
	platform_set_drvdata(pdev, NULL);
	kfree(ir);
	return rc;
}
EXPORT_SYMBOL(msm_geni_ir_probe);

static int msm_geni_ir_remove(struct platform_device *pdev)
{
	struct msm_geni_ir *ir = platform_get_drvdata(pdev);

	rc_unregister_device(ir->rcdev);
	rc_free_device(ir->rcdev);
	mutex_destroy(&ir->lock);
	irq_set_irq_wake(ir->wakeup_irq, 0);
	if (ir->image_loaded != NULL)
		clk_disable_unprepare(ir->ahb_clk);

	free_irq(ir->wakeup_irq, ir);
	free_irq(ir->irq, ir);
#ifdef CONFIG_IR_MSM_GENI_TX
	gpio_free(ir->gpio_tx);
#endif
	gpio_free(ir->gpio_rx);
	msm_geni_ir_rel_res(pdev, ir);
	platform_set_drvdata(pdev, NULL);
	kfree(ir);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int msm_geni_ir_suspend(struct device *dev)
{
	struct msm_geni_ir *ir = platform_get_drvdata(to_platform_device(dev));

	enable_irq_wake(ir->wakeup_irq);

	return 0;
}

static int msm_geni_ir_resume(struct device *dev)
{
	struct msm_geni_ir *ir = platform_get_drvdata(to_platform_device(dev));
	u32 status;

	disable_irq_wake(ir->wakeup_irq);

	/* clear wakeup irq */
	status = readl_relaxed(ir->base + GENI_IR_IRQ_STATUS);
	writel_relaxed(status, ir->base + GENI_IR_IRQ_CLEAR);
	/*write memory barrier*/
	wmb();

	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static const struct dev_pm_ops msm_geni_ir_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		msm_geni_ir_suspend,
		msm_geni_ir_resume
	)
};

static const struct of_device_id msm_geni_ir_match_table[] = {
	{	.compatible = "qcom,msm-geni-ir",
	},
	{}
};

static struct platform_driver msm_geni_ir_driver = {
	.probe		= msm_geni_ir_probe,
	.remove		= msm_geni_ir_remove,
	.driver		= {
		.name	= "GENI Driver",
		.pm     = &msm_geni_ir_dev_pm_ops,
		.of_match_table = msm_geni_ir_match_table,
	},
};

static int __init msm_geni_ir_init(void)
{
	return platform_driver_register(&msm_geni_ir_driver);
}
module_init(msm_geni_ir_init);

static void __exit msm_geni_ir_exit(void)
{
	platform_driver_unregister(&msm_geni_ir_driver);
}
module_exit(msm_geni_ir_exit)

MODULE_DESCRIPTION("GENI IR Driver");
MODULE_LICENSE("GPL v2");
