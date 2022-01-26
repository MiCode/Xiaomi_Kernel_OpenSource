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
#ifndef __MTK_IR_CUS_RCMM_DEFINE_H__
#define __MTK_IR_CUS_RCMM_DEFINE_H__

#include "../inc/mtk_ir_core.h"

#ifdef MTK_LK_IRRX_SUPPORT
#include <platform/mtk_ir_lk_core.h>
#else
/* #include <media/rc-map.h> */
#endif

#define MTK_RCMM_CONFIG          (IRRX_CH_END_31 | IRRX_CH_IGB0 \
			| IRRX_CH_IGSYN | IRRX_CH_HWIR)
#define MTK_RCMM_SAPERIOD        (0x2)
#define MTK_RCMM_THRESHOLD       (0x200)
#define MTK_RCMM_THRESHOLD_REG   (0x80C10200)
#define MTK_RCMM_THRESHOLD_REG_0 (0x406)

#define MTK_RCMM_EXP_IRM_BIT_MASK	0xFFFDFFFF
#define MTK_RCMM_EXP_IRL_BIT_MASK	0xFFFFFFFF
#define MTK_RCMM_EXP_POWE_KEY1		0x30980058
#define MTK_RCMM_EXP_POWE_KEY2		0x00000000


#ifdef MTK_LK_IRRX_SUPPORT

static struct mtk_ir_lk_msg mtk_rcmm_lk_table[] = {
	{0x58, KEY_UP},
	{0x59, KEY_DOWN},
	{0x5c, KEY_ENTER},
};

#else

static struct rc_map_table mtk_rcmm_factory_table[] = {
	{0x10, KEY_VOLUMEUP},
	{0x11, KEY_VOLUMEDOWN},
	{0x5c, KEY_POWER},
};

#define MTK_IR_RCMM_CUSTOMER_CODE       0x50
#define MTK_IR_RCMM_KEYPRESS_TIMEOUT    140
#define MTK_IR_RCMM_GET_KEYCODE(bdata1)  \
		(((bdata1 & 0x03) << 6) | ((bdata1 & 0x0c) << 2) \
		| ((bdata1 & 0x30) >> 2) | ((bdata1 & 0xc0) >> 6))

/*
 * When MTK_IR_SUPPORT_MOUSE_INPUT set to 1, and then if IR receive key value
 * MTK_IR_MOUSE_RC5_SWITCH_CODE, it will switch to Mouse input mode.
 * If IR receive key value MTK_IR_MOUSE_RCMM_SWITCH_CODE again at Mouse input
 * mode, it will switch to IR input mode.
 * If you don't need Mouse input mode, please set
 * MTK_IR_MOUSE_MODE_DEFAULT = MTK_IR_AS_IRRX
 * MTK_IR_SUPPORT_MOUSE_INPUT = 0
 */
#define MTK_IR_MOUSE_MODE_DEFAULT       MTK_IR_AS_IRRX
#define MTK_IR_SUPPORT_MOUSE_INPUT      0
#define MTK_IR_MOUSE_RCMM_SWITCH_CODE   0x37
#define MOUSE_SMALL_X_STEP 10
#define MOUSE_SMALL_Y_STEP 10
#define MOUSE_LARGE_X_STEP 30
#define MOUSE_LARGE_Y_STEP 30

static struct rc_map_table mtk_rcmm_table[] = {
	{0x01, KEY_BACK},
	{0x00, KEY_0},
	{0x01, KEY_1},
	{0x02, KEY_2},
	{0x03, KEY_3},
	{0x04, KEY_4},
	{0x05, KEY_5},
	{0x06, KEY_6},
	{0x07, KEY_7},
	{0x08, KEY_8},
	{0x09, KEY_9},
	{0x0C, KEY_POWER},
	{0x0D, KEY_MUTE},
	{0x0F, KEY_INFO},
	{0x10, KEY_VOLUMEUP},
	{0x11, KEY_VOLUMEDOWN},
	{0x20, KEY_CHANNELUP},
	{0x21, KEY_CHANNELDOWN},
	{0x28, KEY_FASTFORWARD},
	{0x29, KEY_FASTREVERSE},
	{0x2C, KEY_PLAY},
	{0x30, KEY_PAUSE},
	{0x31, KEY_STOP},
	{0x44, KEY_RECORD},
	{0x54, KEY_MENU},
	{0x55, KEY_EXIT},
	{0x58, KEY_UP},
	{0x59, KEY_DOWN},
	{0x5A, KEY_LEFT},
	{0x5B, KEY_RIGHT},
	{0x5c, KEY_ENTER},
	{0x86, KEY_SEARCH},
	{0x9E, KEY_DELETE},
	{0xB1, KEY_OPTION},
	{0xffff, KEY_HELP},
};

#endif
#endif
