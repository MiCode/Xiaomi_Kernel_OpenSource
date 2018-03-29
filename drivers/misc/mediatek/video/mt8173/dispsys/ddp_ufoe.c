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

#define LOG_TAG "UFOE"
#include "ddp_log.h"

#include <linux/clk.h>
#include <linux/delay.h>

#include "ddp_info.h"
#include "ddp_hal.h"
#include "ddp_reg.h"

static bool ufoe_enable;

static int ufoe_dump(DISP_MODULE_ENUM module, int level)
{
	DDPMSG("==DISP UFOE REGS==\n");
	DDPMSG("(0x000)UFOE_START =0x%x\n", DISP_REG_GET(DISP_REG_UFO_START));
	DDPMSG("(0x000)UFOE_CFG0 =0x%x\n", DISP_REG_GET(DISP_REG_UFO_CFG_0B));
	DDPMSG("(0x000)UFOE_CFG1 =0x%x\n", DISP_REG_GET(DISP_REG_UFO_CFG_1B));
	DDPMSG("(0x000)UFOE_WIDTH =0x%x\n", DISP_REG_GET(DISP_REG_UFO_FRAME_WIDTH));
	DDPMSG("(0x000)UFOE_HEIGHT =0x%x\n", DISP_REG_GET(DISP_REG_UFO_FRAME_HEIGHT));
	DDPMSG("(0x000)UFOE_PAD  =0x%x\n", DISP_REG_GET(DISP_REG_UFO_CR0P6_PAD));
	return 0;
}

static int ufoe_init(DISP_MODULE_ENUM module, void *handle)
{
	ddp_module_clock_enable(MM_CLK_DISP_UFOE, true);
	DDPMSG("ufoe_clock on CG 0x%x\n", DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0));
	return 0;
}

static int ufoe_deinit(DISP_MODULE_ENUM module, void *handle)
{
	ddp_module_clock_enable(MM_CLK_DISP_UFOE, false);
	DDPMSG("ufoe_clock off CG 0x%x\n", DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0));
	return 0;
}

int ufoe_start(DISP_MODULE_ENUM module, void *cmdq)
{
	if (ufoe_enable)
		DISP_REG_SET_FIELD(cmdq, START_FLD_DISP_UFO_START, DISP_REG_UFO_START, 1);

	DDPMSG("ufoe_start, ufoe_start:0x%x\n", DISP_REG_GET(DISP_REG_UFO_START));
	return 0;
}


int ufoe_stop(DISP_MODULE_ENUM module, void *cmdq_handle)
{
	DISP_REG_SET_FIELD(cmdq_handle, START_FLD_DISP_UFO_START, DISP_REG_UFO_START, 0);
	DDPMSG("ufoe_stop, ufoe_start:0x%x\n", DISP_REG_GET(DISP_REG_UFO_START));
	return 0;
}

static int ufoe_config(DISP_MODULE_ENUM module, disp_ddp_path_config *pConfig, void *handle)
{
	LCM_PARAMS *disp_if_config = &(pConfig->dispif_config);
	LCM_DSI_PARAMS *lcm_config = &(disp_if_config->dsi);

	if (lcm_config->ufoe_enable) {
		ufoe_enable = true;
#ifdef CONFIG_FPGA_EARLY_PORTING	/* FOR BRING_UP */
		DISP_REG_SET_FIELD(handle, START_FLD_DISP_UFO_BYPASS, DISP_REG_UFO_START, 1);
#else
		DISP_REG_SET_FIELD(handle, START_FLD_DISP_UFO_BYPASS, DISP_REG_UFO_START, 0);
#endif
/* DISP_REG_SET_FIELD(handle, START_FLD_DISP_UFO_START, DISP_REG_UFO_START, 1); */
		if (lcm_config->ufoe_params.lr_mode_en == 1) {
			DISP_REG_SET_FIELD(handle, START_FLD_DISP_UFO_LR_EN, DISP_REG_UFO_START, 1);
		} else {
			DISP_REG_SET_FIELD(handle, START_FLD_DISP_UFO_LR_EN, DISP_REG_UFO_START, 0);
			if (lcm_config->ufoe_params.compress_ratio == 3) {
				unsigned int internal_width =
				    disp_if_config->width + disp_if_config->width % 4;
				DISP_REG_SET_FIELD(handle, CFG_0B_FLD_DISP_UFO_CFG_COM_RATIO,
						   DISP_REG_UFO_CFG_0B, 1);
				if (internal_width % 6 != 0) {
					DISP_REG_SET_FIELD(handle,
							   CR0P6_PAD_FLD_DISP_UFO_STR_PAD_NUM,
							   DISP_REG_UFO_CR0P6_PAD,
							   ((internal_width / 6 + 1) * 6 -
							    internal_width));
				}
			} else
				DISP_REG_SET_FIELD(handle, CFG_0B_FLD_DISP_UFO_CFG_COM_RATIO,
						   DISP_REG_UFO_CFG_0B, 0);

			if (lcm_config->ufoe_params.vlc_disable) {
				DISP_REG_SET_FIELD(handle, CFG_0B_FLD_DISP_UFO_CFG_VLC_EN,
						   DISP_REG_UFO_CFG_0B, 0);
				DISP_REG_SET(handle, DISP_REG_UFO_CFG_1B, 0x1);
			} else {
				DISP_REG_SET_FIELD(handle, CFG_0B_FLD_DISP_UFO_CFG_VLC_EN,
						   DISP_REG_UFO_CFG_0B, 1);
				DISP_REG_SET(handle, DISP_REG_UFO_CFG_1B,
					     (lcm_config->ufoe_params.vlc_config ==
					      0) ? 5 : lcm_config->ufoe_params.vlc_config);
			}
		}
		DISP_REG_SET(handle, DISP_REG_UFO_FRAME_WIDTH, disp_if_config->width);
		DISP_REG_SET(handle, DISP_REG_UFO_FRAME_HEIGHT, disp_if_config->height);
	}
/* ufoe_dump(); */
	return 0;

}

static int ufoe_clock_on(DISP_MODULE_ENUM module, void *handle)
{

	ddp_module_clock_enable(MM_CLK_DISP_UFOE, true);
	DDPMSG("ufoe_clock on CG 0x%x\n", DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0));
	return 0;
}

static int ufoe_clock_off(DISP_MODULE_ENUM module, void *handle)
{
	ddp_module_clock_enable(MM_CLK_DISP_UFOE, false);
	DDPMSG("ufoe_clock off CG 0x%x\n", DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0));
	return 0;
}

static int ufoe_reset(DISP_MODULE_ENUM module, void *handle)
{
	DISP_REG_SET_FIELD(handle, START_FLD_DISP_UFO_SW_RST_ENGINE, DISP_REG_UFO_START, 1);
	DISP_REG_SET_FIELD(handle, START_FLD_DISP_UFO_SW_RST_ENGINE, DISP_REG_UFO_START, 0);
	DDPMSG("ufoe reset done\n");
	return 0;
}

/* ufoe */
DDP_MODULE_DRIVER ddp_driver_ufoe = {
	.init = ufoe_init,
	.deinit = ufoe_deinit,
	.config = ufoe_config,
	.start = ufoe_start,
	.trigger = NULL,
	.stop = ufoe_stop,
	.reset = ufoe_reset,
	.power_on = ufoe_clock_on,
	.power_off = ufoe_clock_off,
	.is_idle = NULL,
	.is_busy = NULL,
	.dump_info = ufoe_dump,
	.bypass = NULL,
	.build_cmdq = NULL,
	.set_lcm_utils = NULL,
};
