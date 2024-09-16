/*  SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _CONNSYS_COREDUMP_EMI_H_
#define _CONNSYS_COREDUMP_EMI_H_

#include <linux/types.h>
#include <linux/compiler.h>


/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/


/* Block and size definition */
#define CONNSYS_DUMP_CTRL_BLOCK_SIZE	0x80
#define CONNSYS_DUMP_DEBUG_BLOCK_SIZE	0x80
#define CONNSYS_DUMP_PRINT_BUFF_SIZE	0x7f00
#define CONNSYS_DUMP_DUMP_BUFF_SIZE	0x8000
#define CONNSYS_DUMP_CR_REGION_SIZE	0x8000

#define CONNSYS_DUMP_CTRL_BLOCK_OFFSET	0x0
#define CONNSYS_DUMP_DEBUG_BLOCK_OFFSET	(CONNSYS_DUMP_CTRL_BLOCK_OFFSET + CONNSYS_DUMP_CTRL_BLOCK_SIZE)
#define CONNSYS_DUMP_PRINT_BUFF_OFFSET	(CONNSYS_DUMP_DEBUG_BLOCK_OFFSET + CONNSYS_DUMP_DEBUG_BLOCK_SIZE)
#define CONNSYS_DUMP_DUMP_BUFF_OFFSET	(CONNSYS_DUMP_PRINT_BUFF_OFFSET + CONNSYS_DUMP_PRINT_BUFF_SIZE)
#define CONNSYS_DUMP_CR_REGION_OFFSET	(CONNSYS_DUMP_DUMP_BUFF_OFFSET + CONNSYS_DUMP_DUMP_BUFF_SIZE)

/* Control block definition */
enum connsys_dump_ctrl_block_offset {
	EXP_CTRL_DUMP_STATE = 0x4,
	EXP_CTRL_OUTBAND_ASSERT_W1 = 0x8,
	EXP_CTRL_PRINT_BUFF_IDX = 0xC,
	EXP_CTRL_DUMP_BUFF_IDX = 0x10,
	EXP_CTRL_CR_REGION_IDX = 0x14,
	EXP_CTRL_TOTAL_MEM_REGION = 0x18,
	EXP_CTRL_MEM_REGION_NAME_1 = 0x1C,
	EXP_CTRL_MEM_REGION_START_1 = 0x20,
	EXP_CTRL_MEM_REGION_LEN_1 = 0x24,
	EXP_CTRL_MEM_REGION_NAME_2 = 0x28,
	EXP_CTRL_MEM_REGION_START_2 = 0x2c,
	EXP_CTRL_MEM_REGION_LEN_2 = 0x30,
	EXP_CTRL_MEM_REGION_NAME_3 = 0x34,
	EXP_CTRL_MEM_REGION_START_3 = 0x38,
	EXP_CTRL_MEM_REGION_LEN_3 = 0x3c,
	EXP_CTRL_MEM_REGION_NAME_4 = 0x40,
	EXP_CTRL_MEM_REGION_START_4 = 0x44,
	EXP_CTRL_MEM_REGION_LEN_4 = 0x48,
	EXP_CTRL_MEM_REGION_NAME_5 = 0x4C,
	EXP_CTRL_MEM_REGION_START_5 = 0x50,
	EXP_CTRL_MEM_REGION_LEN_5 = 0x54,
};


/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/


#endif /*_CONNSYS_COREDUMP_EMI_H_*/
