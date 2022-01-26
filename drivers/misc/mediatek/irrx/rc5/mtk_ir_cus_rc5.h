/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef __MTK_IR_CUS_RC5_DEFINE_H__
#define __MTK_IR_CUS_RC5_DEFINE_H__

#include "../inc/mtk_ir_core.h"

#ifdef MTK_LK_IRRX_SUPPORT
#include <platform/mtk_ir_lk_core.h>
#else
/* #include <media/rc-map.h> */
#endif

#define MTK_RC5_CONFIG		(IRRX_CH_END_15 | IRRX_CH_IGB0 | IRRX_CH_IGSYN \
			| IRRX_CH_HWIR | IRRX_CH_ORDINV | IRRX_CH_RC5)
#define MTK_RC5_SAPERIOD	(0x1E)
#define MTK_RC5_THRESHOLD	(0x1)

#define MTK_RC5_EXP_IRM_BIT_MASK	0xFFFFFF7F
#define MTK_RC5_EXP_IRL_BIT_MASK	0xFFFFFFFF
#define MTK_RC5_EXP_POWE_KEY1		0x00E03F2F
#define MTK_RC5_EXP_POWE_KEY2		0x00000000

#ifdef MTK_LK_IRRX_SUPPORT

static struct mtk_ir_lk_msg mtk_rc5_lk_table[] = {
	{0xF3, KEY_POWER},
};

#else

static struct rc_map_table mtk_rc5_factory_table[] = {
	{0xF3, KEY_POWER},
};

#define MTK_IR_RC5_CUSTOMER_CODE        0x46
#define MTK_IR_RC5_KEYPRESS_TIMEOUT     140

/*
 * When MTK_IR_SUPPORT_MOUSE_INPUT set to 1, and then if IR receive key value
 * MTK_IR_MOUSE_RC5_SWITCH_CODE, it will switch to Mouse input mode.
 * If IR receive key value MTK_IR_MOUSE_RC5_SWITCH_CODE again at Mouse input
 * mode, it will switch to IR input mode.
 * If you don't need Mouse input mode, please set
 * MTK_IR_MOUSE_MODE_DEFAULT = MTK_IR_AS_IRRX
 * MTK_IR_SUPPORT_MOUSE_INPUT = 0
 */
#define MTK_IR_MOUSE_MODE_DEFAULT       MTK_IR_AS_IRRX
#define MTK_IR_SUPPORT_MOUSE_INPUT      0
#define MTK_IR_MOUSE_RC5_SWITCH_CODE    0x70
#define MOUSE_SMALL_X_STEP 10
#define MOUSE_SMALL_Y_STEP 10
#define MOUSE_LARGE_X_STEP 30
#define MOUSE_LARGE_Y_STEP 30

static struct rc_map_table mtk_rc5_table[] = {
	{0xFF, KEY_0},
	{0xFE, KEY_1},
	{0xFD, KEY_2},
	{0xFC, KEY_3},
	{0xFB, KEY_4},
	{0xFA, KEY_5},
	{0xF9, KEY_6},
	{0xF8, KEY_7},
	{0xF7, KEY_8},
	{0xF6, KEY_9},
	{0xF3, KEY_POWER},
	{0xDF, KEY_FORWARD},
	{0xDE, KEY_BACK},
	{0xCF, KEY_PAUSE},
	{0xCD, KEY_FASTREVERSE},
	{0xCB, KEY_FASTFORWARD},
	{0xCA, KEY_PLAY},
	{0xC9, KEY_STOP},
	{0xffff, KEY_HELP},
};

#endif
#endif
