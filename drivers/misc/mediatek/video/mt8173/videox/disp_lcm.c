/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#include <linux/slab.h>
#include <linux/delay.h>

#include "disp_drv_log.h"
#include "lcm_drv.h"
#include "disp_drv_platform.h"
#include "ddp_manager.h"
#include "disp_lcm.h"
#include "ddp_log.h"
/* This macro and arrya is designed for multiple LCM support */
/* for multiple LCM, we should assign I/F Port id in lcm driver, such as DPI0, DSI0/1 */
/*static disp_lcm_handle _disp_lcm_driver[MAX_LCM_NUMBER] = { {0}, {0} };*/

/* these 2 variables are defined in mt65xx_lcm_list.c */

int _lcm_count(void)
{
	return lcm_count;
}

int _is_lcm_inited(disp_lcm_handle *plcm)
{
#if HDMI_MAIN_PATH
	return 0;
#endif
	if (plcm) {
		if (plcm->params && plcm->drv)
			return 1;
		else
			return 0;
	} else {
		DISPERR("WARNING, invalid lcm handle: 0x%08x\n", (unsigned int)(unsigned long)plcm);
		return 0;
	}
}

LCM_PARAMS *_get_lcm_params_by_handle(disp_lcm_handle *plcm)
{
	if (plcm)
		return plcm->params;

	DISPERR("WARNING, invalid lcm handle: 0x%08x\n", (unsigned int)(unsigned long)plcm);
	return NULL;
}

LCM_DRIVER *_get_lcm_driver_by_handle(disp_lcm_handle *plcm)
{
	if (plcm)
		return plcm->drv;

	DISPERR("WARNING, invalid lcm handle: 0x%08x\n", (unsigned int)(unsigned long)plcm);
	return NULL;
}

void _dump_lcm_info(disp_lcm_handle *plcm)
{
	/*int i = 0; */
	LCM_DRIVER *l = NULL;
	LCM_PARAMS *p = NULL;

	if (plcm == NULL) {
		DISPERR("plcm is null\n");
		return;
	}

	l = plcm->drv;
	p = plcm->params;

	if (l && p) {
		DISPCHECK("[LCM], name: %s\n", l->name);

		DISPCHECK("[LCM] resolution: %d x %d\n", p->width, p->height);
		DISPCHECK("[LCM] physical size: %d x %d\n", p->physical_width, p->physical_height);
		DISPCHECK("[LCM] physical size: %d x %d\n", p->physical_width, p->physical_height);

		switch (p->lcm_if) {
		case LCM_INTERFACE_DSI0:
			DISPCHECK("[LCM] interface: DSI0\n");
			break;
		case LCM_INTERFACE_DSI1:
			DISPCHECK("[LCM] interface: DSI1\n");
			break;
		case LCM_INTERFACE_DPI0:
			DISPCHECK("[LCM] interface: DPI0\n");
			break;
		case LCM_INTERFACE_DPI1:
			DISPCHECK("[LCM] interface: DPI1\n");
			break;
		case LCM_INTERFACE_DBI0:
			DISPCHECK("[LCM] interface: DBI0\n");
			break;
		default:
			DISPCHECK("[LCM] interface: unknown\n");
			break;
		}

		switch (p->type) {
		case LCM_TYPE_DBI:
			DISPCHECK("[LCM] Type: DBI\n");
			break;
		case LCM_TYPE_DSI:
			DISPCHECK("[LCM] Type: DSI\n");

			break;
		case LCM_TYPE_DPI:
			DISPCHECK("[LCM] Type: DPI\n");
			break;
		default:
			DISPCHECK("[LCM] TYPE: unknown\n");
			break;
		}

		if (p->type == LCM_TYPE_DSI) {
			switch (p->dsi.mode) {
			case CMD_MODE:
				DISPCHECK("[LCM] DSI Mode: CMD_MODE\n");
				break;
			case SYNC_PULSE_VDO_MODE:
				DISPCHECK("[LCM] DSI Mode: SYNC_PULSE_VDO_MODE\n");
				break;
			case SYNC_EVENT_VDO_MODE:
				DISPCHECK("[LCM] DSI Mode: SYNC_EVENT_VDO_MODE\n");
				break;
			case BURST_VDO_MODE:
				DISPCHECK("[LCM] DSI Mode: BURST_VDO_MODE\n");
				break;
			default:
				DISPCHECK("[LCM] DSI Mode: Unknown\n");
				break;
			}
		}

		if (p->type == LCM_TYPE_DSI) {
			DISPCHECK("[LCM] LANE_NUM: %d,data_format: %d\n",
				  p->dsi.LANE_NUM, (int)p->dsi.data_format.format);
#ifdef ROME_TODO
#error
#endif
			DISPCHECK
			    ("[LCM] vact: %d, vbp: %d, vfp: %d, vact_line: %d\n",
			     p->dsi.vertical_sync_active, p->dsi.vertical_backporch,
			     p->dsi.vertical_frontporch, p->dsi.vertical_active_line);
			DISPCHECK
			    ("[LCM] hact: %d, hbp: %d, hfp: %d, hblank: %d\n",
			     p->dsi.horizontal_sync_active, p->dsi.horizontal_backporch,
			     p->dsi.horizontal_frontporch, p->dsi.horizontal_blanking_pixel);
			DISPCHECK
			    ("[LCM] pll_select: %d, pll_div1: %d, pll_div2: %d, fbk_div: %d,fbk_sel: %d, rg_bir: %d\n",
			     p->dsi.pll_select, p->dsi.pll_div1, p->dsi.pll_div2, p->dsi.fbk_div,
			     p->dsi.fbk_sel, p->dsi.rg_bir);
			DISPCHECK
			    ("[LCM] rg_bic: %d, rg_bp: %d, PLL_CLOCK: %d, dsi_clock: %d, ssc_range: %d\n",
			     p->dsi.rg_bic, p->dsi.rg_bp, p->dsi.PLL_CLOCK, p->dsi.dsi_clock,
			     p->dsi.ssc_range);
			DISPCHECK
				("[LCM] ssc_disable: %d, compatibility_for_nvk: %d, cont_clock: %d\n",
			     p->dsi.ssc_disable, p->dsi.compatibility_for_nvk, p->dsi.cont_clock);
			DISPCHECK
			    ("[LCM] lcm_ext_te_enable: %d, noncont_clock: %d, noncont_clock_period: %d\n",
			     p->dsi.lcm_ext_te_enable, p->dsi.noncont_clock,
			     p->dsi.noncont_clock_period);
		}
	}


}

#ifdef CONFIG_FPGA_EARLY_PORTING	/* FOR BRING_UP */
static unsigned long dbi_regbase_va;

#define DDP_REG_BASE_DBI dbi_regbase_va
#define LCD_SCMD0 (*(volatile unsigned int *)(DDP_REG_BASE_DBI + 0xf80))
#define LCD_SDAT0 (*(volatile unsigned int *)(DDP_REG_BASE_DBI + 0xf90))

int fpga_ws1869_dbi_lcm_init(void)
{
	dbi_regbase_va = ioremap(0x15000000, 0x1000);
	DISPCHECK("%s %d dbi_lcm_init: 0x15000000 --> 0x%lx\n", __func__, __LINE__, dbi_regbase_va);
	/* /RSTB */
	*(volatile unsigned int *)(DDP_REG_BASE_DBI + 0x10) = 0x00000001;
	*(volatile unsigned int *)(DDP_REG_BASE_DBI + 0x10) = 0x00000000;
	msleep(20);		/* /1ms */
	*(volatile unsigned int *)(DDP_REG_BASE_DBI + 0x10) = 0x00000001;
	msleep(20);		/* /1ms */


	/* /config  LCD I/F */
	*(volatile unsigned int *)(DDP_REG_BASE_DBI + 0x14) = 0x00000001;
	*(volatile unsigned int *)(DDP_REG_BASE_DBI + 0x1c) = 0x00FFFFFF;
	*(volatile unsigned int *)(DDP_REG_BASE_DBI + 0x28) = (0x004040c0 | (0 << 0x18));

	/* / HX8369A LCM initial sequence with RGB interface 888 */
	*(volatile unsigned int *)(DDP_REG_BASE_DBI + 0x2c) = 0x00000001;
	*(volatile unsigned int *)(DDP_REG_BASE_DBI + 0x2c) = 0x00000000;

	LCD_SCMD0 = 0xB9;	/* SET password */
	LCD_SDAT0 = 0xFF;
	LCD_SDAT0 = 0x83;
	LCD_SDAT0 = 0x69;

	LCD_SCMD0 = 0xB1;
	LCD_SDAT0 = 0x85;
	LCD_SDAT0 = 0x00;
	LCD_SDAT0 = 0x34;
	LCD_SDAT0 = 0x07;
	LCD_SDAT0 = 0x00;
	LCD_SDAT0 = 0x0F;
	LCD_SDAT0 = 0x0F;
	LCD_SDAT0 = 0x2A;
	LCD_SDAT0 = 0x32;
	LCD_SDAT0 = 0x3F;
	LCD_SDAT0 = 0x3F;
	LCD_SDAT0 = 0x01;
	LCD_SDAT0 = 0x3A;
	LCD_SDAT0 = 0x01;
	LCD_SDAT0 = 0xE6;
	LCD_SDAT0 = 0xE6;
	LCD_SDAT0 = 0xE6;
	LCD_SDAT0 = 0xE6;
	LCD_SDAT0 = 0xE6;


	LCD_SCMD0 = 0xB2;
	LCD_SDAT0 = 0x00;
	LCD_SDAT0 = 0x28;
	LCD_SDAT0 = 0x05;
	LCD_SDAT0 = 0x05;
	LCD_SDAT0 = 0x70;
	LCD_SDAT0 = 0x00;
	LCD_SDAT0 = 0xFF;
	LCD_SDAT0 = 0x00;
	LCD_SDAT0 = 0x00;
	LCD_SDAT0 = 0x00;
	LCD_SDAT0 = 0x00;
	LCD_SDAT0 = 0x03;
	LCD_SDAT0 = 0x03;
	LCD_SDAT0 = 0x00;
	LCD_SDAT0 = 0x01;


	LCD_SCMD0 = 0xB4;
	LCD_SDAT0 = 0x00;
	LCD_SDAT0 = 0x18;
	LCD_SDAT0 = 0x80;
	LCD_SDAT0 = 0x06;
	LCD_SDAT0 = 0x02;

	LCD_SCMD0 = 0xB6;
	LCD_SDAT0 = 0x42;
	LCD_SDAT0 = 0x42;

	LCD_SCMD0 = 0xD5;
	LCD_SDAT0 = 0x00;
	LCD_SDAT0 = 0x04;
	LCD_SDAT0 = 0x03;
	LCD_SDAT0 = 0x00;
	LCD_SDAT0 = 0x01;
	LCD_SDAT0 = 0x05;
	LCD_SDAT0 = 0x28;
	LCD_SDAT0 = 0x70;
	LCD_SDAT0 = 0x01;
	LCD_SDAT0 = 0x03;
	LCD_SDAT0 = 0x00;
	LCD_SDAT0 = 0x00;
	LCD_SDAT0 = 0x40;
	LCD_SDAT0 = 0x06;
	LCD_SDAT0 = 0x51;
	LCD_SDAT0 = 0x07;
	LCD_SDAT0 = 0x00;
	LCD_SDAT0 = 0x00;
	LCD_SDAT0 = 0x41;
	LCD_SDAT0 = 0x06;
	LCD_SDAT0 = 0x50;
	LCD_SDAT0 = 0x07;
	LCD_SDAT0 = 0x07;
	LCD_SDAT0 = 0x0F;
	LCD_SDAT0 = 0x04;
	LCD_SDAT0 = 0x00;


	LCD_SCMD0 = 0xE0;
	LCD_SDAT0 = 0x00;
	LCD_SDAT0 = 0x13;
	LCD_SDAT0 = 0x19;
	LCD_SDAT0 = 0x38;
	LCD_SDAT0 = 0x3D;
	LCD_SDAT0 = 0x3F;
	LCD_SDAT0 = 0x28;
	LCD_SDAT0 = 0x46;
	LCD_SDAT0 = 0x07;
	LCD_SDAT0 = 0x0D;
	LCD_SDAT0 = 0x0E;
	LCD_SDAT0 = 0x12;
	LCD_SDAT0 = 0x15;
	LCD_SDAT0 = 0x12;
	LCD_SDAT0 = 0x14;
	LCD_SDAT0 = 0x0F;
	LCD_SDAT0 = 0x17;
	LCD_SDAT0 = 0x00;
	LCD_SDAT0 = 0x13;
	LCD_SDAT0 = 0x19;
	LCD_SDAT0 = 0x38;
	LCD_SDAT0 = 0x3D;
	LCD_SDAT0 = 0x3F;
	LCD_SDAT0 = 0x28;
	LCD_SDAT0 = 0x46;
	LCD_SDAT0 = 0x07;
	LCD_SDAT0 = 0x0D;
	LCD_SDAT0 = 0x0E;
	LCD_SDAT0 = 0x12;
	LCD_SDAT0 = 0x15;
	LCD_SDAT0 = 0x12;
	LCD_SDAT0 = 0x14;
	LCD_SDAT0 = 0x0F;
	LCD_SDAT0 = 0x17;

	msleep(20);		/* /1ms */


	LCD_SCMD0 = 0xC1;
	LCD_SDAT0 = 0x01;

	LCD_SDAT0 = 0x04;
	LCD_SDAT0 = 0x13;
	LCD_SDAT0 = 0x1a;
	LCD_SDAT0 = 0x20;
	LCD_SDAT0 = 0x27;
	LCD_SDAT0 = 0x2c;
	LCD_SDAT0 = 0x32;
	LCD_SDAT0 = 0x36;
	LCD_SDAT0 = 0x3f;
	LCD_SDAT0 = 0x47;
	LCD_SDAT0 = 0x50;
	LCD_SDAT0 = 0x59;
	LCD_SDAT0 = 0x60;
	LCD_SDAT0 = 0x68;
	LCD_SDAT0 = 0x71;
	LCD_SDAT0 = 0x7B;
	LCD_SDAT0 = 0x82;
	LCD_SDAT0 = 0x89;
	LCD_SDAT0 = 0x91;
	LCD_SDAT0 = 0x98;
	LCD_SDAT0 = 0xA0;
	LCD_SDAT0 = 0xA8;
	LCD_SDAT0 = 0xB0;
	LCD_SDAT0 = 0xB8;
	LCD_SDAT0 = 0xC1;
	LCD_SDAT0 = 0xC9;
	LCD_SDAT0 = 0xD0;
	LCD_SDAT0 = 0xD7;
	LCD_SDAT0 = 0xE0;
	LCD_SDAT0 = 0xE7;
	LCD_SDAT0 = 0xEF;
	LCD_SDAT0 = 0xF7;
	LCD_SDAT0 = 0xFE;
	LCD_SDAT0 = 0xCF;
	LCD_SDAT0 = 0x52;
	LCD_SDAT0 = 0x34;
	LCD_SDAT0 = 0xF8;
	LCD_SDAT0 = 0x51;
	LCD_SDAT0 = 0xF5;
	LCD_SDAT0 = 0x9D;
	LCD_SDAT0 = 0x75;
	LCD_SDAT0 = 0x00;

	LCD_SDAT0 = 0x04;
	LCD_SDAT0 = 0x13;
	LCD_SDAT0 = 0x1a;
	LCD_SDAT0 = 0x20;
	LCD_SDAT0 = 0x27;
	LCD_SDAT0 = 0x2c;
	LCD_SDAT0 = 0x32;
	LCD_SDAT0 = 0x36;
	LCD_SDAT0 = 0x3f;
	LCD_SDAT0 = 0x47;
	LCD_SDAT0 = 0x50;
	LCD_SDAT0 = 0x59;
	LCD_SDAT0 = 0x60;
	LCD_SDAT0 = 0x68;
	LCD_SDAT0 = 0x71;
	LCD_SDAT0 = 0x7B;
	LCD_SDAT0 = 0x82;
	LCD_SDAT0 = 0x89;
	LCD_SDAT0 = 0x91;
	LCD_SDAT0 = 0x98;
	LCD_SDAT0 = 0xA0;
	LCD_SDAT0 = 0xA8;
	LCD_SDAT0 = 0xB0;
	LCD_SDAT0 = 0xB8;
	LCD_SDAT0 = 0xC1;
	LCD_SDAT0 = 0xC9;
	LCD_SDAT0 = 0xD0;
	LCD_SDAT0 = 0xD7;
	LCD_SDAT0 = 0xE0;
	LCD_SDAT0 = 0xE7;
	LCD_SDAT0 = 0xEF;
	LCD_SDAT0 = 0xF7;
	LCD_SDAT0 = 0xFE;
	LCD_SDAT0 = 0xCF;
	LCD_SDAT0 = 0x52;
	LCD_SDAT0 = 0x34;
	LCD_SDAT0 = 0xF8;
	LCD_SDAT0 = 0x51;
	LCD_SDAT0 = 0xF5;
	LCD_SDAT0 = 0x9D;
	LCD_SDAT0 = 0x75;
	LCD_SDAT0 = 0x00;

	LCD_SDAT0 = 0x04;
	LCD_SDAT0 = 0x13;
	LCD_SDAT0 = 0x1a;
	LCD_SDAT0 = 0x20;
	LCD_SDAT0 = 0x27;
	LCD_SDAT0 = 0x2c;
	LCD_SDAT0 = 0x32;
	LCD_SDAT0 = 0x36;
	LCD_SDAT0 = 0x3f;
	LCD_SDAT0 = 0x47;
	LCD_SDAT0 = 0x50;
	LCD_SDAT0 = 0x59;
	LCD_SDAT0 = 0x60;
	LCD_SDAT0 = 0x68;
	LCD_SDAT0 = 0x71;
	LCD_SDAT0 = 0x7B;
	LCD_SDAT0 = 0x82;
	LCD_SDAT0 = 0x89;
	LCD_SDAT0 = 0x91;
	LCD_SDAT0 = 0x98;
	LCD_SDAT0 = 0xA0;
	LCD_SDAT0 = 0xA8;
	LCD_SDAT0 = 0xB0;
	LCD_SDAT0 = 0xB8;
	LCD_SDAT0 = 0xC1;
	LCD_SDAT0 = 0xC9;
	LCD_SDAT0 = 0xD0;
	LCD_SDAT0 = 0xD7;
	LCD_SDAT0 = 0xE0;
	LCD_SDAT0 = 0xE7;
	LCD_SDAT0 = 0xEF;
	LCD_SDAT0 = 0xF7;
	LCD_SDAT0 = 0xFE;
	LCD_SDAT0 = 0xCF;
	LCD_SDAT0 = 0x52;
	LCD_SDAT0 = 0x34;
	LCD_SDAT0 = 0xF8;
	LCD_SDAT0 = 0x51;
	LCD_SDAT0 = 0xF5;
	LCD_SDAT0 = 0x9D;
	LCD_SDAT0 = 0x75;
	LCD_SDAT0 = 0x00;

	msleep(20);		/* /1ms */

	LCD_SDAT0 = 0x77;
	LCD_SCMD0 = 0x3A;
	LCD_SCMD0 = 0x11;

	msleep(20);		/* /1ms */
	LCD_SCMD0 = 0x29;
	LCD_SCMD0 = 0x2C;
	*(volatile unsigned int *)(DDP_REG_BASE_DBI + 0x2c) = 0x3;

	return 0;
}

#endif

disp_lcm_handle *disp_lcm_probe(char *plcm_name, LCM_INTERFACE_ID lcm_id)
{
	/*DISPFUNC(); */

	/*int ret = 0; */
	bool isLCMFound = false;
	bool isLCMInited = false;

	LCM_DRIVER *lcm_drv = NULL;
	LCM_PARAMS *lcm_param = NULL;
	disp_lcm_handle *plcm = NULL;

	DISPFUNC();
	DISPCHECK("plcm_name=%s\n", plcm_name);
	if (_lcm_count() == 0) {
		DISPERR("no lcm driver defined in linux kernel driver\n");
		return NULL;
	} else if (_lcm_count() == 1) {
		if (plcm_name == NULL) {
			lcm_drv = lcm_driver_list[0];

			isLCMFound = true;
			isLCMInited = false;
		} else {
			lcm_drv = lcm_driver_list[0];
			if (strcmp(lcm_drv->name, plcm_name)) {
				DISPERR
				    ("FATAL ERROR!!!LCM Driver defined in kernel is different with LK\n");
				return NULL;
			}

			isLCMInited = false;	/* true; */
			isLCMFound = true;
		}
	} else {
		if (plcm_name == NULL) {
			/* TODO: we need to detect all the lcm driver */
		} else {
			int i = 0;

			for (i = 0; i < _lcm_count(); i++) {
				lcm_drv = lcm_driver_list[i];
				if (!strcmp(lcm_drv->name, plcm_name)) {
					isLCMFound = true;
					isLCMInited = true;
					break;
				}
			}

		}
		/* TODO: */
	}

	if (isLCMFound == false) {
		DISPERR("FATAL ERROR!!!No LCM Driver defined\n");
		return NULL;
	}

	plcm = kzalloc(sizeof(uint8_t *) * sizeof(disp_lcm_handle), GFP_KERNEL);
	lcm_param = kzalloc(sizeof(uint8_t *) * sizeof(LCM_PARAMS), GFP_KERNEL);
	if (plcm && lcm_param) {
		plcm->params = lcm_param;
		plcm->drv = lcm_drv;
		plcm->is_inited = isLCMInited;
	} else {
		DISPERR("FATAL ERROR!!!kzalloc plcm and plcm->params failed\n");
		goto FAIL;
	}

	{
		plcm->drv->get_params(plcm->params);
		plcm->lcm_if_id = plcm->params->lcm_if;

		/* below code is for lcm driver forward compatible */
		if (plcm->params->type == LCM_TYPE_DSI
		    && plcm->params->lcm_if == LCM_INTERFACE_NOTDEFINED)
			plcm->lcm_if_id = LCM_INTERFACE_DSI0;
		if (plcm->params->type == LCM_TYPE_DPI
		    && plcm->params->lcm_if == LCM_INTERFACE_NOTDEFINED)
			plcm->lcm_if_id = LCM_INTERFACE_DPI0;
		if (plcm->params->type == LCM_TYPE_DBI
		    && plcm->params->lcm_if == LCM_INTERFACE_NOTDEFINED)
			plcm->lcm_if_id = LCM_INTERFACE_DBI0;

		if ((lcm_id == LCM_INTERFACE_NOTDEFINED) || lcm_id == plcm->lcm_if_id) {
			plcm->lcm_original_width = plcm->params->width;
			plcm->lcm_original_height = plcm->params->height;
			_dump_lcm_info(plcm);
			return plcm;
		}

		DISPERR("the specific LCM Interface [%d] didn't define any lcm driver\n",
				lcm_id);
		goto FAIL;
	}

FAIL:

		kfree(plcm);
		kfree(lcm_param);
	return NULL;
}


int disp_lcm_init(struct platform_device *dev, disp_lcm_handle *plcm, int force)
{
	/*DISPFUNC(); */
	LCM_DRIVER *lcm_drv = NULL;

	DISPFUNC();

	if (_is_lcm_inited(plcm) || force) {
		lcm_drv = plcm->drv;

		if (lcm_drv->init_power) {
			if (!disp_lcm_is_inited(plcm) || force)
				lcm_drv->init_power();
		}

		if (lcm_drv->init) {
			if (!disp_lcm_is_inited(plcm) || force) {
#ifdef CONFIG_FPGA_EARLY_PORTING	/* FOR BRING_UP */
				fpga_ws1869_dbi_lcm_init();
#endif
				/*lcm_drv->init(dev);*/
				lcm_drv->init();
			}
		} else {
			DISPERR("FATAL ERROR, lcm_drv->init is null\n");
			return -1;
		}

		return 0;
	}
	DISPERR("plcm is null\n");
	return -1;
}

LCM_PARAMS *disp_lcm_get_params(disp_lcm_handle *plcm)
{
	/* DISPFUNC(); */
#if HDMI_MAIN_PATH
	return NULL;
#endif

	if (_is_lcm_inited(plcm))
		return plcm->params;
	else
		return NULL;
}

LCM_INTERFACE_ID disp_lcm_get_interface_id(disp_lcm_handle *plcm)
{
	DISPFUNC();

	if (_is_lcm_inited(plcm))
		return plcm->lcm_if_id;
	else
		return LCM_INTERFACE_NOTDEFINED;
}

int disp_lcm_update(disp_lcm_handle *plcm, int x, int y, int w, int h, int force)
{
	/*DISPDBGFUNC(); */
	LCM_DRIVER *lcm_drv = NULL;
	/*
	   LCM_INTERFACE_ID lcm_id = LCM_INTERFACE_NOTDEFINED;
	   LCM_PARAMS *plcm_param = NULL;
	 */
	DISPDBGFUNC();

	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;
		if (lcm_drv->update) {
			lcm_drv->update(x, y, w, h);
		} else {
			if (!disp_lcm_is_video_mode(plcm))
				DISPERR("FATAL ERROR, lcm is cmd mode lcm_drv->update is null\n");

			return -1;
		}

		return 0;
	}

	DISPERR("lcm_drv is null\n");
	return -1;
}

/* return 1: esd check fail */
/* return 0: esd check pass */
int disp_lcm_esd_check(disp_lcm_handle *plcm)
{
	/*DISPFUNC(); */
	LCM_DRIVER *lcm_drv = NULL;

	DISPFUNC();

	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;
		if (lcm_drv->esd_check)
			return lcm_drv->esd_check();

		DISPERR("FATAL ERROR, lcm_drv->esd_check is null\n");
		return 0;

	}
	DISPERR("lcm_drv is null\n");
	return 0;
}



int disp_lcm_esd_recover(disp_lcm_handle *plcm)
{
	/*DISPFUNC(); */
	LCM_DRIVER *lcm_drv = NULL;

	DISPFUNC();

	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;
		if (lcm_drv->esd_recover) {
			lcm_drv->esd_recover();
		} else {
			DISPERR("FATAL ERROR, lcm_drv->esd_check is null\n");
			return -1;
		}

		return 0;
	}
	DISPERR("lcm_drv is null\n");
	return -1;
}

int disp_lcm_suspend(disp_lcm_handle *plcm)
{
	/*DISPFUNC(); */
	LCM_DRIVER *lcm_drv = NULL;

	DISPFUNC();

	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;
		if (lcm_drv->suspend) {
			lcm_drv->suspend();
		} else {
			DISPERR("FATAL ERROR, lcm_drv->suspend is null\n");
			return -1;
		}

		if (lcm_drv->suspend_power)
			lcm_drv->suspend_power();


		return 0;
	}
	DISPERR("lcm_drv is null\n");
	return -1;
}

int disp_lcm_resume_power(disp_lcm_handle *plcm)
{
	/*DISPFUNC(); */
	LCM_DRIVER *lcm_drv = NULL;
	int ret = 0;

	DISPFUNC();

	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;

		if (lcm_drv->resume_power)
			lcm_drv->resume_power();

	} else {
		ret = -1;
		DISPERR("lcm_drv is null\n");
	}

	return ret;
}

int disp_lcm_resume(disp_lcm_handle *plcm)
{
	/*DISPFUNC(); */
	LCM_DRIVER *lcm_drv = NULL;

	DISPFUNC();

	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;

		/*
		if (lcm_drv->resume_power)
			lcm_drv->resume_power();
		*/

		if (lcm_drv->resume) {
			lcm_drv->resume();
		} else {
			DISPERR("FATAL ERROR, lcm_drv->resume is null\n");
			return -1;
		}

		return 0;
	}
	DISPERR("lcm_drv is null\n");
	return -1;
}



#ifdef ROME_TODO
#error "maybe CABC can be moved into lcm_ioctl??"
#endif
int disp_lcm_set_backlight(disp_lcm_handle *plcm, int level)
{
	/*DISPFUNC(); */
	LCM_DRIVER *lcm_drv = NULL;

	DISPFUNC();

	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;
		if (lcm_drv->set_backlight) {
			lcm_drv->set_backlight(level);
		} else {
			DISPERR("FATAL ERROR, lcm_drv->set_backlight is null\n");
			return -1;
		}

		return 0;
	}
	DISPERR("lcm_drv is null\n");
	return -1;
}




int disp_lcm_ioctl(disp_lcm_handle *plcm, LCM_IOCTL ioctl, unsigned int arg)
{
	return 0;
}

int disp_lcm_is_inited(disp_lcm_handle *plcm)
{
	if (_is_lcm_inited(plcm))
		return plcm->is_inited;
	else
		return 0;
}

unsigned int disp_lcm_ATA(disp_lcm_handle *plcm)
{
	unsigned int ret = 0;
	LCM_DRIVER *lcm_drv = NULL;

	DISPFUNC();

	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;
		if (lcm_drv->ata_check) {
			ret = lcm_drv->ata_check(NULL);
		} else {
			DISPERR("FATAL ERROR, lcm_drv->ata_check is null\n");
			return 0;
		}

		return ret;
	}
	DISPERR("lcm_drv is null\n");
	return 0;

}

void *disp_lcm_switch_mode(disp_lcm_handle *plcm, int mode)
{
	/*unsigned int ret = 0; */
/* DISPFUNC(); */
	LCM_DRIVER *lcm_drv = NULL;
	LCM_DSI_MODE_SWITCH_CMD *lcm_cmd = NULL;

	if (_is_lcm_inited(plcm)) {
		if (plcm->params->dsi.switch_mode_enable == 0) {
			DISPERR(" ERROR, Not enable switch in lcm_get_params function\n");
			return NULL;
		}
		lcm_drv = plcm->drv;
		if (lcm_drv->switch_mode) {
			lcm_cmd = (LCM_DSI_MODE_SWITCH_CMD *) lcm_drv->switch_mode(mode);
			lcm_cmd->cmd_if = (unsigned int)(plcm->params->lcm_cmd_if);
		} else {
			DISPERR("FATAL ERROR, lcm_drv->switch_mode is null\n");
			return NULL;
		}

		return (void *)(lcm_cmd);
	}
	DISPERR("lcm_drv is null\n");
	return NULL;
}

int disp_lcm_is_video_mode(disp_lcm_handle *plcm)
{
	/* DISPFUNC(); */
	LCM_PARAMS *lcm_param = NULL;
#if HDMI_MAIN_PATH
	return true;
#endif
	/*LCM_INTERFACE_ID lcm_id = LCM_INTERFACE_NOTDEFINED; */

	if (_is_lcm_inited(plcm))
		lcm_param = plcm->params;
	else
		ASSERT(0);

	switch (lcm_param->type) {
	case LCM_TYPE_DBI:
		return false;
	case LCM_TYPE_DSI:
		break;
	case LCM_TYPE_DPI:
		return true;
	default:
		DISPMSG("[LCM] TYPE: unknown\n");
		break;
	}

	if (lcm_param->type == LCM_TYPE_DSI) {
		switch (lcm_param->dsi.mode) {
		case CMD_MODE:
			return false;
		case SYNC_PULSE_VDO_MODE:
		case SYNC_EVENT_VDO_MODE:
		case BURST_VDO_MODE:
			return true;
		default:
			DISPMSG("[LCM] DSI Mode: Unknown\n");
			break;
		}
	}

	ASSERT(0);
}

int disp_lcm_set_param(disp_lcm_handle *plcm, unsigned int param)
{
	/*DISPFUNC(); */
	LCM_DRIVER *lcm_drv = NULL;
	int ret = 0;

	DISPFUNC();

	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;
		if (lcm_drv->set_disp_param) {
			lcm_drv->set_disp_param(param);
			ret = 0;
		} else {
			DISPERR("FATAL ERROR, lcm_drv->set_backlight is null\n");
			ret = -1;
		}
	} else {
		DISPERR("lcm_drv is null\n");
		ret = -1;
	}

	return ret;
}
