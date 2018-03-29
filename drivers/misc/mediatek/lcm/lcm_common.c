/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#if defined(MTK_LCM_DEVICE_TREE_SUPPORT)
#include <linux/string.h>
#include <linux/wait.h>
#include <linux/of.h>

#include "lcm_define.h"
#include "lcm_drv.h"
#include "lcm_common.h"
#include "lcm_gpio.h"
#include "lcm_i2c.h"
#include "lcm_pmic.h"
#include "lcm_util.h"


/* #define LCM_DEBUG */

/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */
static LCM_DTS _LCM_DTS;
static LCM_UTIL_FUNCS lcm_util;

#define FRAME_WIDTH  _LCM_DTS.params.width
#define FRAME_HEIGHT  _LCM_DTS.params.height

#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

/* --------------------------------------------------------------------------- */
/* LCM Driver Implementations */
/* --------------------------------------------------------------------------- */
void lcm_common_parse_dts(const LCM_DTS *DTS, unsigned char force_update)
{
	LCM_PARAMS *dts_params = &_LCM_DTS.params;
	LCM_DATA *dts_init = &(_LCM_DTS.init[0]);
	LCM_DATA *dts_compare_id = &(_LCM_DTS.compare_id[0]);
	LCM_DATA *dts_suspend = &(_LCM_DTS.suspend[0]);
	LCM_DATA *dts_backlight = &(_LCM_DTS.backlight[0]);
	LCM_DATA *dts_backlight_cmdq = &(_LCM_DTS.backlight_cmdq[0]);


	if ((_LCM_DTS.parsing != 0) && (force_update == 0)) {
		pr_debug("[LCM][ERROR] %s/%d: DTS has been parsed or non-force update: %d, %d\n",
		       __func__, __LINE__, _LCM_DTS.parsing, force_update);
		return;
	}
#if defined(LCM_DEBUG)
	/* LCM DTS parameter set */
	memset(dts_params, 0, sizeof(LCM_PARAMS));
	dts_params->type = LCM_TYPE_DSI;
	dts_params->width = 720;
	dts_params->height = 1280;
	dts_params->dbi.te_mode = LCM_DBI_TE_MODE_VSYNC_ONLY;
	dts_params->dbi.te_edge_polarity = LCM_POLARITY_RISING;
	dts_params->dsi.mode = CMD_MODE;
	dts_params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
	dts_params->dsi.LANE_NUM = LCM_FOUR_LANE;
	dts_params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	dts_params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	dts_params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	dts_params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;
	dts_params->dsi.packet_size = 256;
	dts_params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
	dts_params->dsi.PLL_CLOCK = 200;
	dts_params->dsi.clk_lp_per_line_enable = 0;
	dts_params->dsi.esd_check_enable = 0;
	dts_params->dsi.customization_esd_check_enable = 0;
	dts_params->dsi.lcm_esd_check_table[0].cmd = 0x53;
	dts_params->dsi.lcm_esd_check_table[0].count = 0x01;
	dts_params->dsi.lcm_esd_check_table[0].para_list[0] = 0x24;
#else
	memset(dts_params, 0, sizeof(LCM_PARAMS));
	memcpy(dts_params, &(DTS->params), sizeof(LCM_PARAMS));
#endif

#if defined(LCM_DEBUG)
	if (memcmp
	    ((unsigned char *)dts_params, (unsigned char *)(&(DTS->params)),
	     sizeof(LCM_PARAMS)) != 0x0) {
		pr_debug("[LCM][ERROR] %s/%d: DTS compare error\n", __func__, __LINE__);

		pr_debug("[LCM][ERROR] dts_params:\n");
		tmp = (unsigned char *)dts_params;
		tmp2 = (unsigned char *)(&(DTS->params));
		for (i = 0; i < sizeof(LCM_PARAMS); i += 8) {
			if (*(tmp + i) != *(tmp2 + i))
				pr_debug("data: 0x%x 0x%x, index: %d\n", *(tmp + i), *(tmp2 + i), i);
			if (*(tmp + i + 1) != *(tmp2 + i + 1))
				pr_debug("data: 0x%x 0x%x, index: %d\n", *(tmp + i + 1),
				       *(tmp2 + i + 1), i + 1);
			if (*(tmp + i + 2) != *(tmp2 + i + 2))
				pr_debug("data: 0x%x 0x%x, index: %d\n", *(tmp + i + 2),
				       *(tmp2 + i + 2), i + 2);
			if (*(tmp + i + 3) != *(tmp2 + i + 3))
				pr_debug("data: 0x%x 0x%x, index: %d\n", *(tmp + i + 3),
				       *(tmp2 + i + 3), i + 3);
			if (*(tmp + i + 4) != *(tmp2 + i + 4))
				pr_debug("data: 0x%x 0x%x, index: %d\n", *(tmp + i + 4),
				       *(tmp2 + i + 4), i + 4);
			if (*(tmp + i + 5) != *(tmp2 + i + 5))
				pr_debug("data: 0x%x 0x%x, index: %d\n", *(tmp + i + 5),
				       *(tmp2 + i + 5), i + 5);
			if (*(tmp + i + 6) != *(tmp2 + i + 6))
				pr_debug("data: 0x%x 0x%x, index: %d\n", *(tmp + i + 6),
				       *(tmp2 + i + 6), i + 6);
			if (*(tmp + i + 7) != *(tmp2 + i + 7))
				pr_debug("data: 0x%x 0x%x, index: %d\n", *(tmp + i + 7),
				       *(tmp2 + i + 7), i + 7);
		}

		pr_debug("[LCM][ERROR] DTS->params:\n");
		tmp = (unsigned char *)(&(DTS->params));
		for (i = 0; i < sizeof(LCM_PARAMS); i += 8) {
			pr_debug("0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", *(tmp + i),
			       *(tmp + i + 1), *(tmp + i + 2), *(tmp + i + 3), *(tmp + i + 4),
			       *(tmp + i + 5), *(tmp + i + 6), *(tmp + i + 7));
		}

		return;
	}
#endif

#if defined(LCM_DEBUG)
	/* LCM DTS init data set */
	_LCM_DTS.init_size = 0;
	memset(dts_init, 0, sizeof(LCM_DATA) * INIT_SIZE);
	dts_init->func = LCM_FUNC_UTIL;
	dts_init->type = LCM_UTIL_RESET;
	dts_init->size = 1;
	dts_init->data_t1.data = LCM_UTIL_RESET_HIGH;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_UTIL;
	dts_init->type = LCM_UTIL_RESET;
	dts_init->size = 1;
	dts_init->data_t1.data = LCM_UTIL_RESET_LOW;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_UTIL;
	dts_init->type = LCM_UTIL_MDELAY;
	dts_init->size = 1;
	dts_init->data_t1.data = 10;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_UTIL;
	dts_init->type = LCM_UTIL_RESET;
	dts_init->size = 1;
	dts_init->data_t1.data = LCM_UTIL_RESET_HIGH;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_UTIL;
	dts_init->type = LCM_UTIL_MDELAY;
	dts_init->size = 1;
	dts_init->data_t1.data = 20;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_CMD;
	dts_init->type = LCM_UTIL_WRITE_CMD_V2;
	dts_init->size = 2;
	dts_init->data_t3.cmd = 0x11;
	dts_init->data_t3.size = 0;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_UTIL;
	dts_init->type = LCM_UTIL_MDELAY;
	dts_init->size = 1;
	dts_init->data_t1.data = 120;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_CMD;
	dts_init->type = LCM_UTIL_WRITE_CMD_V2;
	dts_init->size = 5;
	dts_init->data_t3.cmd = 0xB9;
	dts_init->data_t3.size = 3;
	dts_init->data_t3.data[0] = 0xFF;
	dts_init->data_t3.data[1] = 0x83;
	dts_init->data_t3.data[2] = 0x92;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_UTIL;
	dts_init->type = LCM_UTIL_MDELAY;
	dts_init->size = 1;
	dts_init->data_t1.data = 10;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_CMD;
	dts_init->type = LCM_UTIL_WRITE_CMD_V2;
	dts_init->size = 4;
	dts_init->data_t3.cmd = 0xB0;
	dts_init->data_t3.size = 2;
	dts_init->data_t3.data[0] = 0x01;
	dts_init->data_t3.data[1] = 0x08;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_CMD;
	dts_init->type = LCM_UTIL_WRITE_CMD_V2;
	dts_init->size = 19;
	dts_init->data_t3.cmd = 0xBA;
	dts_init->data_t3.size = 17;
	dts_init->data_t3.data[0] = 0x13;
	dts_init->data_t3.data[1] = 0x83;
	dts_init->data_t3.data[2] = 0x00;
	dts_init->data_t3.data[3] = 0xD6;
	dts_init->data_t3.data[4] = 0xC5;
	dts_init->data_t3.data[5] = 0x10;
	dts_init->data_t3.data[6] = 0x09;
	dts_init->data_t3.data[7] = 0xFF;
	dts_init->data_t3.data[8] = 0x0F;
	dts_init->data_t3.data[9] = 0x27;
	dts_init->data_t3.data[10] = 0x03;
	dts_init->data_t3.data[11] = 0x21;
	dts_init->data_t3.data[12] = 0x27;
	dts_init->data_t3.data[13] = 0x25;
	dts_init->data_t3.data[14] = 0x20;
	dts_init->data_t3.data[15] = 0x00;
	dts_init->data_t3.data[16] = 0x10;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_CMD;
	dts_init->type = LCM_UTIL_WRITE_CMD_V2;
	dts_init->size = 15;
	dts_init->data_t3.cmd = 0xB1;
	dts_init->data_t3.size = 13;
	dts_init->data_t3.data[0] = 0x7C;
	dts_init->data_t3.data[1] = 0x00;
	dts_init->data_t3.data[2] = 0x43;
	dts_init->data_t3.data[3] = 0xBB;
	dts_init->data_t3.data[4] = 0x00;
	dts_init->data_t3.data[5] = 0x1A;
	dts_init->data_t3.data[6] = 0x1A;
	dts_init->data_t3.data[7] = 0x2F;
	dts_init->data_t3.data[8] = 0x36;
	dts_init->data_t3.data[9] = 0x3F;
	dts_init->data_t3.data[10] = 0x3F;
	dts_init->data_t3.data[11] = 0x42;
	dts_init->data_t3.data[12] = 0x7A;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_CMD;
	dts_init->type = LCM_UTIL_WRITE_CMD_V2;
	dts_init->size = 14;
	dts_init->data_t3.cmd = 0xB2;
	dts_init->data_t3.size = 12;
	dts_init->data_t3.data[0] = 0x08;
	dts_init->data_t3.data[1] = 0xC8;
	dts_init->data_t3.data[2] = 0x06;
	dts_init->data_t3.data[3] = 0x18;
	dts_init->data_t3.data[4] = 0x04;
	dts_init->data_t3.data[5] = 0x84;
	dts_init->data_t3.data[6] = 0x00;
	dts_init->data_t3.data[7] = 0xFF;
	dts_init->data_t3.data[8] = 0x06;
	dts_init->data_t3.data[9] = 0x06;
	dts_init->data_t3.data[10] = 0x04;
	dts_init->data_t3.data[11] = 0x20;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_CMD;
	dts_init->type = LCM_UTIL_WRITE_CMD_V2;
	dts_init->size = 25;
	dts_init->data_t3.cmd = 0xB4;
	dts_init->data_t3.size = 23;
	dts_init->data_t3.data[0] = 0x00;
	dts_init->data_t3.data[1] = 0x00;
	dts_init->data_t3.data[2] = 0x05;
	dts_init->data_t3.data[3] = 0x0A;
	dts_init->data_t3.data[4] = 0x8F;
	dts_init->data_t3.data[5] = 0x06;
	dts_init->data_t3.data[6] = 0x0A;
	dts_init->data_t3.data[7] = 0x95;
	dts_init->data_t3.data[8] = 0x01;
	dts_init->data_t3.data[9] = 0x07;
	dts_init->data_t3.data[10] = 0x06;
	dts_init->data_t3.data[11] = 0x0C;
	dts_init->data_t3.data[12] = 0x02;
	dts_init->data_t3.data[13] = 0x08;
	dts_init->data_t3.data[14] = 0x08;
	dts_init->data_t3.data[15] = 0x21;
	dts_init->data_t3.data[16] = 0x04;
	dts_init->data_t3.data[17] = 0x02;
	dts_init->data_t3.data[18] = 0x08;
	dts_init->data_t3.data[19] = 0x01;
	dts_init->data_t3.data[20] = 0x04;
	dts_init->data_t3.data[21] = 0x1A;
	dts_init->data_t3.data[22] = 0x95;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_CMD;
	dts_init->type = LCM_UTIL_WRITE_CMD_V2;
	dts_init->size = 3;
	dts_init->data_t3.cmd = 0x35;
	dts_init->data_t3.size = 1;
	dts_init->data_t3.data[0] = 0x00;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_CMD;
	dts_init->type = LCM_UTIL_WRITE_CMD_V2;
	dts_init->size = 6;
	dts_init->data_t3.cmd = 0xBF;
	dts_init->data_t3.size = 4;
	dts_init->data_t3.data[0] = 0x05;
	dts_init->data_t3.data[1] = 0x60;
	dts_init->data_t3.data[2] = 0x02;
	dts_init->data_t3.data[3] = 0x00;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_CMD;
	dts_init->type = LCM_UTIL_WRITE_CMD_V2;
	dts_init->size = 3;
	dts_init->data_t3.cmd = 0xB6;
	dts_init->data_t3.size = 1;
	dts_init->data_t3.data[0] = 0x6A;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_CMD;
	dts_init->type = LCM_UTIL_WRITE_CMD_V2;
	dts_init->size = 3;
	dts_init->data_t3.cmd = 0x36;
	dts_init->data_t3.size = 1;
	dts_init->data_t3.data[0] = 0x08;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_CMD;
	dts_init->type = LCM_UTIL_WRITE_CMD_V2;
	dts_init->size = 4;
	dts_init->data_t3.cmd = 0xC0;
	dts_init->data_t3.size = 2;
	dts_init->data_t3.data[0] = 0x03;
	dts_init->data_t3.data[1] = 0x94;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_CMD;
	dts_init->type = LCM_UTIL_WRITE_CMD_V2;
	dts_init->size = 3;
	dts_init->data_t3.cmd = 0xC2;
	dts_init->data_t3.size = 1;
	dts_init->data_t3.data[0] = 0x08;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_CMD;
	dts_init->type = LCM_UTIL_WRITE_CMD_V2;
	dts_init->size = 6;
	dts_init->data_t3.cmd = 0xC6;
	dts_init->data_t3.size = 4;
	dts_init->data_t3.data[0] = 0x35;
	dts_init->data_t3.data[1] = 0x00;
	dts_init->data_t3.data[2] = 0x20;
	dts_init->data_t3.data[3] = 0x04;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_CMD;
	dts_init->type = LCM_UTIL_WRITE_CMD_V2;
	dts_init->size = 3;
	dts_init->data_t3.cmd = 0xCC;
	dts_init->data_t3.size = 1;
	dts_init->data_t3.data[0] = 0x09;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_CMD;
	dts_init->type = LCM_UTIL_WRITE_CMD_V2;
	dts_init->size = 3;
	dts_init->data_t3.cmd = 0xD4;
	dts_init->data_t3.size = 1;
	dts_init->data_t3.data[0] = 0x00;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_CMD;
	dts_init->type = LCM_UTIL_WRITE_CMD_V2;
	dts_init->size = 25;
	dts_init->data_t3.cmd = 0xD5;
	dts_init->data_t3.size = 23;
	dts_init->data_t3.data[0] = 0x00;
	dts_init->data_t3.data[1] = 0x01;
	dts_init->data_t3.data[2] = 0x04;
	dts_init->data_t3.data[3] = 0x00;
	dts_init->data_t3.data[4] = 0x01;
	dts_init->data_t3.data[5] = 0x67;
	dts_init->data_t3.data[6] = 0x89;
	dts_init->data_t3.data[7] = 0xAB;
	dts_init->data_t3.data[8] = 0x45;
	dts_init->data_t3.data[9] = 0xCC;
	dts_init->data_t3.data[10] = 0xCC;
	dts_init->data_t3.data[11] = 0xCC;
	dts_init->data_t3.data[12] = 0x00;
	dts_init->data_t3.data[13] = 0x10;
	dts_init->data_t3.data[14] = 0x54;
	dts_init->data_t3.data[15] = 0xBA;
	dts_init->data_t3.data[16] = 0x98;
	dts_init->data_t3.data[17] = 0x76;
	dts_init->data_t3.data[18] = 0xCC;
	dts_init->data_t3.data[19] = 0xCC;
	dts_init->data_t3.data[20] = 0xCC;
	dts_init->data_t3.data[21] = 0x00;
	dts_init->data_t3.data[22] = 0x00;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_CMD;
	dts_init->type = LCM_UTIL_WRITE_CMD_V2;
	dts_init->size = 25;
	dts_init->data_t3.cmd = 0xD8;
	dts_init->data_t3.size = 23;
	dts_init->data_t3.data[0] = 0x00;
	dts_init->data_t3.data[1] = 0x00;
	dts_init->data_t3.data[2] = 0x05;
	dts_init->data_t3.data[3] = 0x00;
	dts_init->data_t3.data[4] = 0x9A;
	dts_init->data_t3.data[5] = 0x00;
	dts_init->data_t3.data[6] = 0x02;
	dts_init->data_t3.data[7] = 0x95;
	dts_init->data_t3.data[8] = 0x01;
	dts_init->data_t3.data[9] = 0x07;
	dts_init->data_t3.data[10] = 0x06;
	dts_init->data_t3.data[11] = 0x00;
	dts_init->data_t3.data[12] = 0x08;
	dts_init->data_t3.data[13] = 0x08;
	dts_init->data_t3.data[14] = 0x00;
	dts_init->data_t3.data[15] = 0x1D;
	dts_init->data_t3.data[16] = 0x08;
	dts_init->data_t3.data[17] = 0x08;
	dts_init->data_t3.data[18] = 0x08;
	dts_init->data_t3.data[19] = 0x00;
	dts_init->data_t3.data[20] = 0x00;
	dts_init->data_t3.data[21] = 0x00;
	dts_init->data_t3.data[22] = 0x77;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_CMD;
	dts_init->type = LCM_UTIL_WRITE_CMD_V2;
	dts_init->size = 36;
	dts_init->data_t3.cmd = 0xE0;
	dts_init->data_t3.size = 34;
	dts_init->data_t3.data[0] = 0x00;
	dts_init->data_t3.data[1] = 0x12;
	dts_init->data_t3.data[2] = 0x19;
	dts_init->data_t3.data[3] = 0x33;
	dts_init->data_t3.data[4] = 0x36;
	dts_init->data_t3.data[5] = 0x3F;
	dts_init->data_t3.data[6] = 0x28;
	dts_init->data_t3.data[7] = 0x47;
	dts_init->data_t3.data[8] = 0x06;
	dts_init->data_t3.data[9] = 0x0C;
	dts_init->data_t3.data[10] = 0x0E;
	dts_init->data_t3.data[11] = 0x12;
	dts_init->data_t3.data[12] = 0x14;
	dts_init->data_t3.data[13] = 0x12;
	dts_init->data_t3.data[14] = 0x14;
	dts_init->data_t3.data[15] = 0x12;
	dts_init->data_t3.data[16] = 0x1A;
	dts_init->data_t3.data[17] = 0x00;
	dts_init->data_t3.data[18] = 0x12;
	dts_init->data_t3.data[19] = 0x19;
	dts_init->data_t3.data[20] = 0x33;
	dts_init->data_t3.data[21] = 0x36;
	dts_init->data_t3.data[22] = 0x3F;
	dts_init->data_t3.data[23] = 0x28;
	dts_init->data_t3.data[24] = 0x47;
	dts_init->data_t3.data[25] = 0x06;
	dts_init->data_t3.data[26] = 0x0C;
	dts_init->data_t3.data[27] = 0x0E;
	dts_init->data_t3.data[28] = 0x12;
	dts_init->data_t3.data[29] = 0x14;
	dts_init->data_t3.data[30] = 0x12;
	dts_init->data_t3.data[31] = 0x14;
	dts_init->data_t3.data[32] = 0x12;
	dts_init->data_t3.data[33] = 0x1A;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_CMD;
	dts_init->type = LCM_UTIL_WRITE_CMD_V2;
	dts_init->size = 36;
	dts_init->data_t3.cmd = 0xE1;
	dts_init->data_t3.size = 34;
	dts_init->data_t3.data[0] = 0x00;
	dts_init->data_t3.data[1] = 0x12;
	dts_init->data_t3.data[2] = 0x19;
	dts_init->data_t3.data[3] = 0x33;
	dts_init->data_t3.data[4] = 0x36;
	dts_init->data_t3.data[5] = 0x3F;
	dts_init->data_t3.data[6] = 0x28;
	dts_init->data_t3.data[7] = 0x47;
	dts_init->data_t3.data[8] = 0x06;
	dts_init->data_t3.data[9] = 0x0C;
	dts_init->data_t3.data[10] = 0x0E;
	dts_init->data_t3.data[11] = 0x12;
	dts_init->data_t3.data[12] = 0x14;
	dts_init->data_t3.data[13] = 0x12;
	dts_init->data_t3.data[14] = 0x14;
	dts_init->data_t3.data[15] = 0x12;
	dts_init->data_t3.data[16] = 0x1A;
	dts_init->data_t3.data[17] = 0x00;
	dts_init->data_t3.data[18] = 0x12;
	dts_init->data_t3.data[19] = 0x19;
	dts_init->data_t3.data[20] = 0x33;
	dts_init->data_t3.data[21] = 0x36;
	dts_init->data_t3.data[22] = 0x3F;
	dts_init->data_t3.data[23] = 0x28;
	dts_init->data_t3.data[24] = 0x47;
	dts_init->data_t3.data[25] = 0x06;
	dts_init->data_t3.data[26] = 0x0C;
	dts_init->data_t3.data[27] = 0x0E;
	dts_init->data_t3.data[28] = 0x12;
	dts_init->data_t3.data[29] = 0x14;
	dts_init->data_t3.data[30] = 0x12;
	dts_init->data_t3.data[31] = 0x14;
	dts_init->data_t3.data[32] = 0x12;
	dts_init->data_t3.data[33] = 0x1A;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_CMD;
	dts_init->type = LCM_UTIL_WRITE_CMD_V2;
	dts_init->size = 36;
	dts_init->data_t3.cmd = 0xE2;
	dts_init->data_t3.size = 34;
	dts_init->data_t3.data[0] = 0x00;
	dts_init->data_t3.data[1] = 0x12;
	dts_init->data_t3.data[2] = 0x19;
	dts_init->data_t3.data[3] = 0x33;
	dts_init->data_t3.data[4] = 0x36;
	dts_init->data_t3.data[5] = 0x3F;
	dts_init->data_t3.data[6] = 0x28;
	dts_init->data_t3.data[7] = 0x47;
	dts_init->data_t3.data[8] = 0x06;
	dts_init->data_t3.data[9] = 0x0C;
	dts_init->data_t3.data[10] = 0x0E;
	dts_init->data_t3.data[11] = 0x12;
	dts_init->data_t3.data[12] = 0x14;
	dts_init->data_t3.data[13] = 0x12;
	dts_init->data_t3.data[14] = 0x14;
	dts_init->data_t3.data[15] = 0x12;
	dts_init->data_t3.data[16] = 0x1A;
	dts_init->data_t3.data[17] = 0x00;
	dts_init->data_t3.data[18] = 0x12;
	dts_init->data_t3.data[19] = 0x19;
	dts_init->data_t3.data[20] = 0x33;
	dts_init->data_t3.data[21] = 0x36;
	dts_init->data_t3.data[22] = 0x3F;
	dts_init->data_t3.data[23] = 0x28;
	dts_init->data_t3.data[24] = 0x47;
	dts_init->data_t3.data[25] = 0x06;
	dts_init->data_t3.data[26] = 0x0C;
	dts_init->data_t3.data[27] = 0x0E;
	dts_init->data_t3.data[28] = 0x12;
	dts_init->data_t3.data[29] = 0x14;
	dts_init->data_t3.data[30] = 0x12;
	dts_init->data_t3.data[31] = 0x14;
	dts_init->data_t3.data[32] = 0x12;
	dts_init->data_t3.data[33] = 0x1A;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_CMD;
	dts_init->type = LCM_UTIL_WRITE_CMD_V2;
	dts_init->size = 3;
	dts_init->data_t3.cmd = 0x3A;
	dts_init->data_t3.size = 1;
	dts_init->data_t3.data[0] = 0x77;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;

	dts_init->func = LCM_FUNC_CMD;
	dts_init->type = LCM_UTIL_WRITE_CMD_V2;
	dts_init->size = 2;
	dts_init->data_t3.cmd = 0x29;
	dts_init->data_t3.size = 0;
	dts_init = dts_init + 1;
	_LCM_DTS.init_size = _LCM_DTS.init_size + 1;
#else
	memset(dts_init, 0, sizeof(LCM_DATA) * (DTS->init_size));
	memcpy(dts_init, &(DTS->init[0]), sizeof(LCM_DATA) * (DTS->init_size));
	_LCM_DTS.init_size = DTS->init_size;
#endif

#if defined(LCM_DEBUG)
	dts_init = &(_LCM_DTS.init[0]);
	if (memcmp((unsigned char *)dts_init, (unsigned char *)(&(DTS->init[0])), sizeof(LCM_DATA))
	    != 0x0) {
		pr_debug("[LCM][ERROR] %s/%d: DTS compare error\n", __func__, __LINE__);

		pr_debug("[LCM][ERROR] dts_init:\n");
		tmp = (unsigned char *)dts_init;
		for (i = 0; i < _LCM_DTS.init_size; i++) {
			pr_debug("0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", *(tmp), *(tmp + 1),
			       *(tmp + 2), *(tmp + 3), *(tmp + 4), *(tmp + 5), *(tmp + 6),
			       *(tmp + 7));
			tmp = tmp + sizeof(LCM_DATA);
		}

		pr_debug("[LCM][ERROR] DTS->init:\n");
		tmp = (unsigned char *)(&(DTS->init[0]));
		for (i = 0; i < DTS->init_size; i++) {
			pr_debug("0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", *(tmp), *(tmp + 1),
			       *(tmp + 2), *(tmp + 3), *(tmp + 4), *(tmp + 5), *(tmp + 6),
			       *(tmp + 7));
			tmp = tmp + sizeof(LCM_DATA);
		}

		return;
	}
#endif

#if defined(LCM_DEBUG)
	/* LCM DTS compare_id data set */
	_LCM_DTS.compare_id_size = 0;
	memset(dts_compare_id, 0, sizeof(LCM_DATA) * COMPARE_ID_SIZE);
	dts_compare_id->func = LCM_FUNC_CMD;
	dts_compare_id->type = LCM_UTIL_WRITE_CMD_V1;
	dts_compare_id->size = 5;
	dts_compare_id->data_t5.size = 1;
	dts_compare_id->data_t5.cmd[0] = 0x00;
	dts_compare_id->data_t5.cmd[1] = 0x37;
	dts_compare_id->data_t5.cmd[2] = 0x01;
	dts_compare_id->data_t5.cmd[3] = 0x00;
	dts_compare_id = dts_compare_id + 1;
	_LCM_DTS.compare_id_size = _LCM_DTS.compare_id_size + 1;

	dts_compare_id->func = LCM_FUNC_CMD;
	dts_compare_id->type = LCM_UTIL_READ_CMD_V2;
	dts_compare_id->size = 3;
	dts_compare_id->data_t4.cmd = 0xF4;
	dts_compare_id->data_t4.location = 1;
	dts_compare_id->data_t4.data = 0x92;
	dts_compare_id = dts_compare_id + 1;
	_LCM_DTS.compare_id_size = _LCM_DTS.compare_id_size + 1;
#else
	memset(dts_compare_id, 0, sizeof(LCM_DATA) * (DTS->compare_id_size));
	memcpy(dts_compare_id, &(DTS->compare_id[0]), sizeof(LCM_DATA) * (DTS->compare_id_size));
	_LCM_DTS.compare_id_size = DTS->compare_id_size;
#endif

#if defined(LCM_DEBUG)
	/* LCM DTS suspend data set */
	_LCM_DTS.suspend_size = 0;
	memset(dts_suspend, 0, sizeof(LCM_DATA) * SUSPEND_SIZE);
	dts_suspend->func = LCM_FUNC_CMD;
	dts_suspend->type = LCM_UTIL_WRITE_CMD_V2;
	dts_suspend->size = 2;
	dts_suspend->data_t3.cmd = 0x10;
	dts_suspend->data_t3.size = 0;
	dts_suspend = dts_suspend + 1;
	_LCM_DTS.suspend_size = _LCM_DTS.suspend_size + 1;

	dts_suspend->func = LCM_FUNC_UTIL;
	dts_suspend->type = LCM_UTIL_MDELAY;
	dts_suspend->size = 1;
	dts_suspend->data_t1.data = 120;
	dts_suspend = dts_suspend + 1;
	_LCM_DTS.suspend_size = _LCM_DTS.suspend_size + 1;
#else
	memset(dts_suspend, 0, sizeof(LCM_DATA) * (DTS->suspend_size));
	memcpy(dts_suspend, &(DTS->suspend[0]), sizeof(LCM_DATA) * (DTS->suspend_size));
	_LCM_DTS.suspend_size = DTS->suspend_size;
#endif

#if defined(LCM_DEBUG)
	/* LCM DTS backlight data set */
	_LCM_DTS.backlight_size = 0;
	memset(dts_backlight, 0, sizeof(LCM_DATA) * BACKLIGHT_SIZE);
	dts_backlight->func = LCM_FUNC_CMD;
	dts_backlight->type = LCM_UTIL_WRITE_CMD_V2;
	dts_backlight->size = 3;
	dts_backlight->data_t3.cmd = 0x51;
	dts_backlight->data_t3.size = 1;
	dts_backlight->data_t3.data[0] = 0xFF;
	dts_backlight = dts_backlight + 1;
	_LCM_DTS.backlight_size = _LCM_DTS.backlight_size + 1;
#else
	memset(dts_backlight, 0, sizeof(LCM_DATA) * (DTS->backlight_size));
	memcpy(dts_backlight, &(DTS->backlight[0]), sizeof(LCM_DATA) * (DTS->backlight_size));
	_LCM_DTS.backlight_size = DTS->backlight_size;
#endif

#if defined(LCM_DEBUG)
	/* LCM DTS backlight cmdq data set */
	_LCM_DTS.backlight_cmdq_size = 0;
	memset(dts_backlight_cmdq, 0, sizeof(LCM_DATA) * BACKLIGHT_CMDQ_SIZE);
	dts_backlight_cmdq->func = LCM_FUNC_CMD;
	dts_backlight_cmdq->type = LCM_UTIL_WRITE_CMD_V2;
	dts_backlight_cmdq->size = 3;
	dts_backlight_cmdq->data_t3.cmd = 0x51;
	dts_backlight_cmdq->data_t3.size = 1;
	dts_backlight_cmdq->data_t3.data[0] = 0xFF;
	dts_backlight_cmdq = dts_backlight_cmdq + 1;
	_LCM_DTS.backlight_cmdq_size = _LCM_DTS.backlight_cmdq_size + 1;
#else
	memset(dts_backlight_cmdq, 0, sizeof(LCM_DATA) * (DTS->backlight_cmdq_size));
	memcpy(dts_backlight_cmdq, &(DTS->backlight_cmdq[0]),
	       sizeof(LCM_DATA) * (DTS->backlight_cmdq_size));
	_LCM_DTS.backlight_cmdq_size = DTS->backlight_cmdq_size;
#endif

	_LCM_DTS.parsing = 1;
}


void lcm_common_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}


void lcm_common_get_params(LCM_PARAMS *params)
{
	if (params == NULL) {
		pr_debug("[LCM][ERROR] %s/%d: NULL parameter\n", __func__, __LINE__);
		return;
	}

	if (_LCM_DTS.parsing != 0) {
		memset(params, 0, sizeof(LCM_PARAMS));
		memcpy(params, &(_LCM_DTS.params), sizeof(LCM_PARAMS));
	} else {
		pr_debug("[LCM][ERROR] %s/%d: DTS is not parsed\n", __func__, __LINE__);
		return;
	}
}


void lcm_common_init(void)
{
	if (_LCM_DTS.init_size > INIT_SIZE) {
		pr_debug("[LCM][ERROR] %s/%d: Init table overflow %d\n", __func__, __LINE__,
		       _LCM_DTS.init_size);
		return;
	}

	if (_LCM_DTS.parsing != 0) {
		unsigned int i;
		LCM_DATA *init;

		for (i = 0; i < _LCM_DTS.init_size; i++) {
			init = &(_LCM_DTS.init[i]);
			switch (init->func) {
			case LCM_FUNC_GPIO:
				lcm_gpio_set_data(init->type, &init->data_t1);
				break;

			case LCM_FUNC_I2C:
				lcm_i2c_set_data(init->type, &init->data_t2);
				break;

			case LCM_FUNC_UTIL:
				lcm_util_set_data(&lcm_util, init->type, &init->data_t1);
				break;

			case LCM_FUNC_CMD:
				switch (init->type) {
				case LCM_UTIL_WRITE_CMD_V1:
					lcm_util_set_write_cmd_v1(&lcm_util, &init->data_t5, 1);
					break;

				case LCM_UTIL_WRITE_CMD_V2:
					lcm_util_set_write_cmd_v2(&lcm_util, &init->data_t3, 1);
					break;

				default:
					pr_debug("[LCM][ERROR] %s/%d: %d\n", __func__, __LINE__,
					       init->type);
					return;
				}
				break;

			default:
				pr_debug("[LCM][ERROR] %s/%d: %d\n", __func__, __LINE__, init->func);
				return;
			}
		}
	} else {
		pr_debug("[LCM][ERROR] %s/%d: DTS is not parsed\n", __func__, __LINE__);
		return;
	}
}


void lcm_common_suspend(void)
{
	if (_LCM_DTS.suspend_size > SUSPEND_SIZE) {
		pr_debug("[LCM][ERROR] %s/%d: Suspend table overflow %d\n", __func__, __LINE__,
		       _LCM_DTS.suspend_size);
		return;
	}

	if (_LCM_DTS.parsing != 0) {
		unsigned int i;
		LCM_DATA *suspend;

		for (i = 0; i < _LCM_DTS.suspend_size; i++) {
			suspend = &(_LCM_DTS.suspend[i]);
			switch (suspend->func) {
			case LCM_FUNC_GPIO:
				lcm_gpio_set_data(suspend->type, &suspend->data_t1);
				break;

			case LCM_FUNC_I2C:
				lcm_i2c_set_data(suspend->type, &suspend->data_t2);
				break;

			case LCM_FUNC_UTIL:
				lcm_util_set_data(&lcm_util, suspend->type, &suspend->data_t1);
				break;

			case LCM_FUNC_CMD:
				switch (suspend->type) {
				case LCM_UTIL_WRITE_CMD_V1:
					lcm_util_set_write_cmd_v1(&lcm_util, &suspend->data_t5, 1);
					break;

				case LCM_UTIL_WRITE_CMD_V2:
					lcm_util_set_write_cmd_v2(&lcm_util, &suspend->data_t3, 1);
					break;

				default:
					pr_debug("[LCM][ERROR] %s/%d: %d\n", __func__, __LINE__,
					       suspend->type);
					return;
				}
				break;

			default:
				pr_debug("[LCM][ERROR] %s/%d: %d\n", __func__, __LINE__,
				       suspend->func);
				return;
			}
		}
	} else {
		pr_debug("[LCM][ERROR] %s/%d: DTS is not parsed\n", __func__, __LINE__);
		return;
	}
}


void lcm_common_resume(void)
{
	lcm_common_init();
}


void lcm_common_update(unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
	unsigned char y0_LSB = (y0 & 0xFF);
	unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
	unsigned char y1_LSB = (y1 & 0xFF);

	LCM_DATA update;
	LCM_DATA_T5 *data_t5 = &(update.data_t5);
	LCM_PARAMS *dts_params = &_LCM_DTS.params;

	if (_LCM_DTS.parsing != 0) {
		if (dts_params->dsi.mode == CMD_MODE) {
			data_t5->size = 3;
			data_t5->cmd[0] = 0x02;
			data_t5->cmd[1] = 0x39;
			data_t5->cmd[2] = 0x05;
			data_t5->cmd[3] = 0x00;

			data_t5->cmd[4] = 0x2a;
			data_t5->cmd[5] = x0_MSB;
			data_t5->cmd[6] = x0_LSB;
			data_t5->cmd[7] = x1_MSB;

			data_t5->cmd[8] = x1_LSB;
			data_t5->cmd[9] = 0x00;
			data_t5->cmd[10] = 0x00;
			data_t5->cmd[11] = 0x00;
			lcm_util_set_write_cmd_v1(&lcm_util, data_t5, 1);

			data_t5->size = 3;
			data_t5->cmd[0] = 0x02;
			data_t5->cmd[1] = 0x39;
			data_t5->cmd[2] = 0x05;
			data_t5->cmd[3] = 0x00;

			data_t5->cmd[4] = 0x2b;
			data_t5->cmd[5] = y0_MSB;
			data_t5->cmd[6] = y0_LSB;
			data_t5->cmd[7] = y1_MSB;

			data_t5->cmd[8] = y1_LSB;
			data_t5->cmd[9] = 0x00;
			data_t5->cmd[10] = 0x00;
			data_t5->cmd[11] = 0x00;
			lcm_util_set_write_cmd_v1(&lcm_util, data_t5, 1);

			data_t5->size = 1;
			data_t5->cmd[0] = 0x09;
			data_t5->cmd[1] = 0x39;
			data_t5->cmd[2] = 0x2c;
			data_t5->cmd[3] = 0x00;
			lcm_util_set_write_cmd_v1(&lcm_util, data_t5, 0);
		}
	}
}


void lcm_common_setbacklight(unsigned int level)
{
	unsigned int default_level = 145;
	unsigned int mapped_level = 0;

	/* for LGE backlight IC mapping table */
	if (level > 255)
		level = 255;

	if (level > 0)
		mapped_level = default_level + (level) * (255 - default_level) / (255);
	else
		mapped_level = 0;

	if (_LCM_DTS.backlight_size > BACKLIGHT_SIZE) {
		pr_debug("[LCM][ERROR] %s/%d: Backlight table overflow %d\n", __func__, __LINE__,
		       _LCM_DTS.backlight_size);
		return;
	}

	if (_LCM_DTS.parsing != 0) {
		unsigned int i;
		LCM_DATA *backlight;
		LCM_DATA_T3 *backlight_data_t3;

		for (i = 0; i < _LCM_DTS.backlight_size; i++) {
			if (i == (_LCM_DTS.backlight_size - 1)) {
				backlight = &(_LCM_DTS.backlight[i]);
				backlight_data_t3 = &(backlight->data_t3);
				backlight_data_t3->data[i] = mapped_level;
			} else
				backlight = &(_LCM_DTS.backlight[i]);

			switch (backlight->func) {
			case LCM_FUNC_GPIO:
				lcm_gpio_set_data(backlight->type, &backlight->data_t1);
				break;

			case LCM_FUNC_I2C:
				lcm_i2c_set_data(backlight->type, &backlight->data_t2);
				break;

			case LCM_FUNC_UTIL:
				lcm_util_set_data(&lcm_util, backlight->type, &backlight->data_t1);
				break;

			case LCM_FUNC_CMD:
				switch (backlight->type) {
				case LCM_UTIL_WRITE_CMD_V1:
					lcm_util_set_write_cmd_v1(&lcm_util, &backlight->data_t5,
								  1);
					break;

				case LCM_UTIL_WRITE_CMD_V2:
					lcm_util_set_write_cmd_v2(&lcm_util, &backlight->data_t3,
								  1);
					break;

				default:
					pr_debug("[LCM][ERROR] %s/%d: %d\n", __func__, __LINE__,
					       (unsigned int)backlight->type);
					return;
				}
				break;

			default:
				pr_debug("[LCM][ERROR] %s/%d: %d\n", __func__, __LINE__,
				       (unsigned int)backlight->func);
				return;
			}
		}
	} else {
		pr_debug("[LCM][ERROR] %s/%d: DTS is not parsed\n", __func__, __LINE__);
		return;
	}
}


unsigned int lcm_common_compare_id(void)
{
	/* default: skip compare id */
	unsigned int compare = 1;

	if (_LCM_DTS.compare_id_size > COMPARE_ID_SIZE) {
		pr_debug("[LCM][ERROR] %s/%d: Compare table overflow %d\n", __func__, __LINE__,
		       _LCM_DTS.compare_id_size);
		return 0;
	}

	if (_LCM_DTS.parsing != 0) {
		unsigned int i;
		LCM_DATA *compare_id;

		for (i = 0; i < _LCM_DTS.compare_id_size; i++) {
			compare_id = &(_LCM_DTS.compare_id[i]);
			switch (compare_id->func) {
			case LCM_FUNC_GPIO:
				lcm_gpio_set_data(compare_id->type, &compare_id->data_t1);
				break;

			case LCM_FUNC_I2C:
				lcm_i2c_set_data(compare_id->type, &compare_id->data_t2);
				break;

			case LCM_FUNC_UTIL:
				lcm_util_set_data(&lcm_util, compare_id->type,
						  &compare_id->data_t1);
				break;

			case LCM_FUNC_CMD:
				switch (compare_id->type) {
				case LCM_UTIL_WRITE_CMD_V1:
					lcm_util_set_write_cmd_v1(&lcm_util, &compare_id->data_t5,
								  1);
					break;

				case LCM_UTIL_WRITE_CMD_V2:
					lcm_util_set_write_cmd_v2(&lcm_util, &compare_id->data_t3,
								  1);
					break;

				case LCM_UTIL_READ_CMD_V2:
					lcm_util_set_read_cmd_v2(&lcm_util, &compare_id->data_t4,
								 &compare);
					break;

				default:
					pr_debug("[LCM][ERROR] %s/%d: %d\n", __func__, __LINE__,
					       compare_id->type);
					return 0;
				}
				break;

			default:
				pr_debug("[LCM][ERROR] %s/%d: %d\n", __func__, __LINE__,
				       compare_id->func);
				return 0;
			}
		}
	} else {
		pr_debug("[LCM][ERROR] %s/%d: DTS is not parsed\n", __func__, __LINE__);
		return 0;
	}

	return compare;
}


unsigned int lcm_common_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	return 1;
#endif
}


void lcm_common_setbacklight_cmdq(void *handle, unsigned int level)
{
	if (_LCM_DTS.backlight_cmdq_size > BACKLIGHT_CMDQ_SIZE) {
		pr_debug("[LCM][ERROR] %s/%d: Backlight cmdq table overflow %d\n", __func__, __LINE__,
			 _LCM_DTS.backlight_cmdq_size);
		return;
	}

	if (_LCM_DTS.parsing != 0) {
		unsigned int i;
		LCM_DATA *backlight_cmdq;
		LCM_DATA_T3 *backlight_cmdq_data_t3;

		for (i = 0; i < _LCM_DTS.backlight_cmdq_size; i++) {
			if (i == (_LCM_DTS.backlight_cmdq_size - 1)) {
				backlight_cmdq = &(_LCM_DTS.backlight_cmdq[i]);
				backlight_cmdq_data_t3 = &(backlight_cmdq->data_t3);
				backlight_cmdq_data_t3->data[i] = level;
			} else
				backlight_cmdq = &(_LCM_DTS.backlight_cmdq[i]);

			switch (backlight_cmdq->func) {
			case LCM_FUNC_GPIO:
				lcm_gpio_set_data(backlight_cmdq->type, &backlight_cmdq->data_t1);
				break;

			case LCM_FUNC_I2C:
				lcm_i2c_set_data(backlight_cmdq->type, &backlight_cmdq->data_t2);
				break;

			case LCM_FUNC_UTIL:
				lcm_util_set_data(&lcm_util, backlight_cmdq->type,
						  &backlight_cmdq->data_t1);
				break;

			case LCM_FUNC_CMD:
				switch (backlight_cmdq->type) {
				case LCM_UTIL_WRITE_CMD_V23:
					lcm_util_set_write_cmd_v23(&lcm_util, handle,
								   &backlight_cmdq->data_t3, 1);
					break;

				default:
					pr_debug("[LCM][ERROR] %s/%d: %d\n", __func__, __LINE__,
						 (unsigned int)backlight_cmdq->type);
					return;
				}
				break;

			default:
				pr_debug("[LCM][ERROR] %s/%d: %d\n", __func__, __LINE__,
					 (unsigned int)backlight_cmdq->func);
				return;
			}
		}
	} else {
		pr_debug("[LCM][ERROR] %s/%d: DTS is not parsed\n", __func__, __LINE__);
		return;
	}
}

#if defined(R63419_WQHD_TRULY_PHANTOM_2K_CMD_OK)
#define PARTIAL_WIDTH_ALIGN_LINE
static inline int align_to(int value, int n, int lower_align)
{
	int x = value;

	value = (((x) + ((n) - 1)) & ~((n) - 1));

	if (lower_align) {
		if (value > x)
			value -= n;
	} else {
#ifndef PARTIAL_WIDTH_ALIGN_LINE
		if (value <= x)
			value += n;
#else
		if (value < x)
			value += n;
#endif
	}
	return value;
}

static void r63419_lcm_validate_roi(int *x, int *y, int *width, int *height)
{
	int x1 = *x;
	int x2 = *width + x1 - 1;
	int y1 = *y;
	int y2 = *height + y1 - 1;
	int w = *width;
	int h = *height;
	int lcm_w = _LCM_DTS.params.dsi.horizontal_active_pixel;

	/*  comfine  SP & EP value */
#ifndef PARTIAL_WIDTH_ALIGN_LINE
	x1 = 0;
	y1 = align_to(y1, 2, 1);
	y2 = align_to(y2, 2, 0) - 1;
	w = lcm_w;
#else
	int ya_align = align_to(y2, 2, 0);
	int lcm_half = lcm_w >> 1;
	int roi_half = 0;

	if (w == 0 || w == lcm_w) {
		w = lcm_w;
	} else {
		y1 = align_to(y1, 2, 1);

		if (ya_align == y2)
			ya_align += 1;
		else
			ya_align -= 1;
		y2 = ya_align;

		if (lcm_half >= x2) {
			roi_half = lcm_half - x1;
		} else if (x1 >= lcm_half) {
			roi_half = x2 - lcm_half;
		} else {
			int left = lcm_half - x1;
			int right = x2 - lcm_half;

			roi_half = left > right ? left : right;
		}
		if (roi_half < 16)
			roi_half = 16;

		roi_half = align_to(roi_half, 16, 0);
		roi_half += 16;
		if (roi_half > lcm_half)
			roi_half = lcm_half;

		x1 = lcm_half - roi_half;

		w = roi_half << 1;
	}
#endif

	if (h == 0) {
		h = 6;
	} else {
		if (y2 - y1 < 6) {
			if (y1 > 6)
				y1 -= 6;
			else
				y2 += 6;
		}
		h = y2 - y1 + 1;
	}
	/*
	   LCD_DEBUG("roi(%d,%d,%d,%d) to (%d,%d,%d,%d)\n",
	 *x, *y, *width, *height, x1, y1, w, h);
	 */
	*x = x1;
	*y = y1;
	*width = w;
	*height = h;
}
#endif

LCM_DRIVER lcm_common_drv = {
	.name = NULL,
	.set_util_funcs = lcm_common_set_util_funcs,
	.get_params = lcm_common_get_params,
	.init = lcm_common_init,
	.suspend = lcm_common_suspend,
	.resume = lcm_common_resume,
	.compare_id = lcm_common_compare_id,
	.set_backlight = lcm_common_setbacklight,
	.update = lcm_common_update,
	.ata_check = lcm_common_ata_check,
	.set_backlight_cmdq = lcm_common_setbacklight_cmdq,
	.parse_dts = lcm_common_parse_dts,
#if defined(R63419_WQHD_TRULY_PHANTOM_2K_CMD_OK)
	.validate_roi = r63419_lcm_validate_roi,
#endif
};
#endif
