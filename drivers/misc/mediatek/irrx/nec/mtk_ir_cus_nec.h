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
#ifndef __MTK_IR_CUS_NEC_DEFINE_H__
#define __MTK_IR_CUS_NEC_DEFINE_H__

#include "mtk_ir_core.h"

#ifdef MTK_LK_IRRX_SUPPORT
#include <platform/mtk_ir_lk_core.h>
#else
/* #include <media/rc-map.h> */
#endif

#define MTK_NEC_CONFIG      (IRRX_CH_END_15 | IRRX_CH_IGB0 \
			| IRRX_CH_IGSYN | IRRX_CH_HWIR)
#define MTK_NEC_SAPERIOD    (0x0F)
#define MTK_NEC_THRESHOLD   (0x1)

#define MTK_NEC_EXP_IRM_BIT_MASK	0xFFFFFFFF
#define MTK_NEC_EXP_IRL_BIT_MASK	0xFFFFFFFF
#define MTK_NEC_EXP_POWE_KEY1		0xF10EFF00
#define MTK_NEC_EXP_POWE_KEY2		0x00000000


#ifdef MTK_LK_IRRX_SUPPORT

static struct mtk_ir_lk_msg mtk_nec_lk_table[] = {
	{0x44, KEY_UP},
	{0x1d, KEY_DOWN},
	{0x5c, KEY_ENTER},
};

#else

static struct rc_map_table mtk_nec_factory_table[] = {
	{0x44, KEY_VOLUMEUP},
	{0x1d, KEY_VOLUMEDOWN},
	{0x5c, KEY_POWER},
};

#define MTK_IR_NEC_CUSTOMER_CODE        0xFF00
#define MTK_IR_NEC_KEYPRESS_TIMEOUT     140

/*
 * When MTK_IR_SUPPORT_MOUSE_INPUT set to 1, and then if IR receive key value
 * MTK_IR_MOUSE_RC5_SWITCH_CODE, it will switch to Mouse input mode.
 * If IR receive key value MTK_IR_MOUSE_NEC_SWITCH_CODE again at Mouse input
 * mode, it will switch to IR input mode.
 * If you don't need Mouse input mode, please set
 * MTK_IR_MOUSE_MODE_DEFAULT = MTK_IR_AS_IRRX
 * MTK_IR_SUPPORT_MOUSE_INPUT = 0
 */
#define MTK_IR_MOUSE_MODE_DEFAULT       MTK_IR_AS_IRRX
#define MTK_IR_SUPPORT_MOUSE_INPUT      0
#define MTK_IR_MOUSE_NEC_SWITCH_CODE    0x54
#define MOUSE_SMALL_X_STEP 10
#define MOUSE_SMALL_Y_STEP 10
#define MOUSE_LARGE_X_STEP 30
#define MOUSE_LARGE_Y_STEP 30

static struct rc_map_table mtk_nec_table[] = {
	{0x00, KEY_X},
	{0x01, KEY_BACK},
	{0x05, KEY_0},
	{0x13, KEY_1},
	{0x10, KEY_2},
	{0x11, KEY_3},
	{0x0f, KEY_4},
	{0x0c, KEY_5},
	{0x0d, KEY_6},
	{0x0b, KEY_7},
	{0x08, KEY_8},
	{0x09, KEY_9},
	{0x1c, KEY_LEFT},
	{0x48, KEY_RIGHT},
	{0x44, KEY_UP},
	{0x1d, KEY_DOWN},
	{0x5c, KEY_ENTER},
	{0x06, KEY_V},
	{0x1E, KEY_B},

	{0x0e, KEY_POWER},
	{0x4c, KEY_HOMEPAGE},
	{0x45, KEY_BACKSPACE},
	{0x5D, KEY_FASTFORWARD},
	{0x12, KEY_T},
	{0x50, KEY_Y},
	{0x54, KEY_U},
	{0x16, KEY_I},
	{0x18, KEY_PREVIOUSSONG},
	{0x47, KEY_P},
	{0x4b, KEY_A},
	{0x19, KEY_NEXTSONG},
	{0x1f, KEY_REWIND},
	{0x1b, KEY_PAUSECD},
	{0x51, KEY_PLAYCD},
	{0x03, KEY_J},
	{0x04, KEY_K},
	{0x59, KEY_STOPCD},
	{0x49, KEY_M},
	{0x07, KEY_L},
	{0x02, KEY_W},
	{0x58, KEY_Z},
	{0x14, KEY_N},
	{0x17, KEY_E},
	{0x1a, KEY_D},
	{0x40, KEY_C},
	{0xffff, KEY_HELP},
};

#endif
#endif
