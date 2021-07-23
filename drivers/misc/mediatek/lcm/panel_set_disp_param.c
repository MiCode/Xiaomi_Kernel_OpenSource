/*
 * Copyright (C) 2016 Xiaomi Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "panel_set_disp_param.h"
#include "ddp_drv.h"
#include "lcm_drv.h"
#include "disp_recovery.h"

#define REGFLAG_DELAY           0xFE
#define REGFLAG_END_OF_TABLE    0xFF   /* END OF REGISTERS MARKER */

unsigned char xy_writepoint[16] = {0};

#ifdef CONFIG_BACKLIGHT_SUPPORT_LM36273
extern int hbm_brightness_set(int level);
#else
int hbm_brightness_set(int level) { return 0; }
#endif

static struct LCM_setting_table lcm_dsi_dispparam_cabcguion_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {
	{	{0x55, 1, {0x01}},
		{REGFLAG_END_OF_TABLE, 0x00, {}}
	},
};

static struct LCM_setting_table lcm_dsi_dispparam_cabcstillon_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {
	{	{0x55, 1, {0x02}},
		{REGFLAG_END_OF_TABLE, 0x00, {}}
	},
};

static struct LCM_setting_table lcm_dsi_dispparam_cabcmovieon_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {
	{	{0x55, 1, {0x03}},
		{REGFLAG_END_OF_TABLE, 0x00, {}}
	},
};

static struct LCM_setting_table lcm_dsi_dispparam_cabcoff_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {
	{	{0x55, 1, {0x00}},
		{REGFLAG_END_OF_TABLE, 0x00, {}}
	},
};

static struct LCM_setting_table lcm_dsi_dispparam_skin_ce_cabcguion_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {
	{	{0x55, 1, {0x01}},
		{REGFLAG_END_OF_TABLE, 0x00, {}}
	},
};

static struct LCM_setting_table lcm_dsi_dispparam_skin_ce_cabcstillon_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {
	{	{0x55, 1, {0x02}},
		{REGFLAG_END_OF_TABLE, 0x00, {}}
	},
};

static struct LCM_setting_table lcm_dsi_dispparam_skin_ce_cabcmovieon_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {
	{	{0x55, 1, {0x03}},
		{REGFLAG_END_OF_TABLE, 0x00, {}}
	},
};

static struct LCM_setting_table lcm_dsi_dispparam_skin_ce_cabcoff_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {
	{	{0x55, 1, {0x00}},
		{REGFLAG_END_OF_TABLE, 0x00, {}}
	},
};


static struct LCM_setting_table lcm_dsi_dispparam_dimmingon_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {
	{	{0x53, 1, {0x2C}},
		{REGFLAG_END_OF_TABLE, 0x00, {}}
	},
};

static struct LCM_setting_table lcm_dsi_dispparam_dimmingoff_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {

};

static struct LCM_setting_table lcm_dsi_dispparam_idleon_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {

};
/*
static struct LCM_setting_table lcm_dsi_dispparam_idleoff_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {

};
*/
static struct LCM_setting_table lcm_dsi_dispparam_warm_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {
	{	{0xF0, 0x05, {0x55, 0xAA, 0x52, 0x08, 0x02} },
		{REGFLAG_DELAY, 1, {} },
		{0xD1, 0x10, {0x00, 0x00, 0x00, 0x72, 0x00, 0x9D, 0x00, 0xBB, 0x00, 0xD2, 0x00, 0xF7, 0x01, 0x14, 0x01, 0x41} },
		{0xD2, 0x10, {0x01, 0x65, 0x01, 0x9E, 0x01, 0xCA, 0x02, 0x0E, 0x02, 0x44, 0x02, 0x46, 0x02, 0x76, 0x02, 0xAA} },
		{0xD3, 0x10, {0x02, 0xCB, 0x02, 0xF7, 0x03, 0x16, 0x03, 0x43, 0x03, 0x5F, 0x03, 0x82, 0x03, 0x98, 0x03, 0xB4} },
		{0xD4, 0x04, {0x03, 0xEE, 0x03, 0xFF} },
		{REGFLAG_DELAY, 1, {} },
		{0xD5, 0x10, {0x00, 0xD3, 0x00, 0xDD, 0x00, 0xEF, 0x01, 0x01, 0x01, 0x0D, 0x01, 0x29, 0x01, 0x3A, 0x01, 0x61} },
		{0xD6, 0x10, {0x01, 0x7C, 0x01, 0xAF, 0x01, 0xD8, 0x02, 0x15, 0x02, 0x4B, 0x02, 0x4C, 0x02, 0x7A, 0x02, 0xAD} },
		{0xD7, 0x10, {0x02, 0xCE, 0x02, 0xFB, 0x03, 0x1B, 0x03, 0x45, 0x03, 0x60, 0x03, 0x82, 0x03, 0x98, 0x03, 0xB2} },
		{0xD8, 0x04, {0x03, 0xD3, 0x03, 0xFF} },
		{REGFLAG_DELAY, 1, {} },
		{0xD9, 0x10, {0x00, 0xE2, 0x00, 0xE9, 0x00, 0xF7, 0x01, 0x06, 0x01, 0x10, 0x01, 0x29, 0x01, 0x39, 0x01, 0x5E} },
		{0xDD, 0x10, {0x01, 0x78, 0x01, 0xAA, 0x01, 0xD4, 0x02, 0x12, 0x02, 0x48, 0x02, 0x49, 0x02, 0x78, 0x02, 0xAC} },
		{0xDE, 0x10, {0x02, 0xCE, 0x02, 0xFD, 0x03, 0x21, 0x03, 0x5D, 0x03, 0x75, 0x03, 0x8A, 0x03, 0x98, 0x03, 0xAC} },
		{0xDF, 0x04, {0x03, 0xE9, 0x03, 0xFF} },
		{REGFLAG_DELAY, 1, {} },
		{0xE0, 0x10, {0x00, 0x00, 0x00, 0x72, 0x00, 0x9D, 0x00, 0xBB, 0x00, 0xD2, 0x00, 0xF7, 0x01, 0x14, 0x01, 0x41} },
		{0xE1, 0x10, {0x01, 0x65, 0x01, 0x9E, 0x01, 0xCA, 0x02, 0x0E, 0x02, 0x44, 0x02, 0x46, 0x02, 0x76, 0x02, 0xAA} },
		{0xE2, 0x10, {0x02, 0xCB, 0x02, 0xF7, 0x03, 0x16, 0x03, 0x43, 0x03, 0x5F, 0x03, 0x82, 0x03, 0x98, 0x03, 0xB4} },
		{0xE3, 0x04, {0x03, 0xEE, 0x03, 0xFF} },
		{REGFLAG_DELAY, 1, {} },
		{0xE4, 0x10, {0x00, 0xD3, 0x00, 0xDD, 0x00, 0xEF, 0x01, 0x01, 0x01, 0x0D, 0x01, 0x29, 0x01, 0x3A, 0x01, 0x61} },
		{0xE5, 0x10, {0x01, 0x7C, 0x01, 0xAF, 0x01, 0xD8, 0x02, 0x15, 0x02, 0x4B, 0x02, 0x4C, 0x02, 0x7A, 0x02, 0xAD} },
		{0xE6, 0x10, {0x02, 0xCE, 0x02, 0xFB, 0x03, 0x1B, 0x03, 0x45, 0x03, 0x60, 0x03, 0x82, 0x03, 0x98, 0x03, 0xB2} },
		{0xE7, 0x04, {0x03, 0xD3, 0x03, 0xFF} },
		{REGFLAG_DELAY, 1, {} },
		{0xE8, 0x10, {0x00, 0xE2, 0x00, 0xE9, 0x00, 0xF7, 0x01, 0x06, 0x01, 0x10, 0x01, 0x29, 0x01, 0x39, 0x01, 0x5E} },
		{0xE9, 0x10, {0x01, 0x78, 0x01, 0xAA, 0x01, 0xD4, 0x02, 0x12, 0x02, 0x48, 0x02, 0x49, 0x02, 0x78, 0x02, 0xAC} },
		{0xEA, 0x10, {0x02, 0xCE, 0x02, 0xFD, 0x03, 0x21, 0x03, 0x5D, 0x03, 0x75, 0x03, 0x8A, 0x03, 0x98, 0x03, 0xAC} },
		{0xEB, 0x04, {0x03, 0xE9, 0x03, 0xFF} },
		{REGFLAG_DELAY, 1, {} },
		{0xF0, 0x05, {0x55, 0xAA, 0x52, 0x00, 0x00}},
		{REGFLAG_END_OF_TABLE, 0x00, {}}
	},
};

static struct LCM_setting_table lcm_dsi_dispparam_normal1_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {

};

static struct LCM_setting_table lcm_dsi_dispparam_normal2_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {

};

static struct LCM_setting_table lcm_dsi_dispparam_cold_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {
	{	{0xF0, 0x05, {0x55, 0xAA, 0x52, 0x08, 0x02} },
		{REGFLAG_DELAY, 1, {} },
		{0xD1, 0x10, {0x01, 0x23, 0x01, 0x29, 0x01, 0x35, 0x01, 0x42, 0x01, 0x4A, 0x01, 0x5C, 0x01, 0x6E, 0x01, 0x8A} },
		{0xD2, 0x10, {0x01, 0xA7, 0x01, 0xCF, 0x01, 0xF3, 0x02, 0x2E, 0x02, 0x5E, 0x02, 0x5F, 0x02, 0x8C, 0x02, 0xC1} },
		{0xD3, 0x10, {0x02, 0xE5, 0x03, 0x16, 0x03, 0x36, 0x03, 0x60, 0x03, 0x7C, 0x03, 0x9C, 0x03, 0xAE, 0x03, 0xC6} },
		{0xD4, 0x04, {0x03, 0xFC, 0x03, 0xFF} },
		{REGFLAG_DELAY, 1, {} },
		{0xD5, 0x10, {0x01, 0x0D, 0x01, 0x14, 0x01, 0x21, 0x01, 0x2D, 0x01, 0x38, 0x01, 0x4C, 0x01, 0x5E, 0x01, 0x7E} },
		{0xD6, 0x10, {0x01, 0x99, 0x01, 0xC6, 0x01, 0xEB, 0x02, 0x27, 0x02, 0x59, 0x02, 0x5A, 0x02, 0x88, 0x02, 0xBE} },
		{0xD7, 0x10, {0x02, 0xE2, 0x03, 0x13, 0x03, 0x34, 0x03, 0x5F, 0x03, 0x79, 0x03, 0x99, 0x03, 0xAD, 0x03, 0xC8} },
		{0xD8, 0x04, {0x03, 0xEF, 0x03, 0xFF} },
		{REGFLAG_DELAY, 1, {} },
		{0xD9, 0x10, {0x00, 0x00, 0x00, 0x2D, 0x00, 0x63, 0x00, 0x8D, 0x00, 0xA3, 0x00, 0xCE, 0x00, 0xF2, 0x01, 0x26} },
		{0xDD, 0x10, {0x01, 0x55, 0x01, 0x91, 0x01, 0xC3, 0x02, 0x0F, 0x02, 0x48, 0x02, 0x4A, 0x02, 0x7C, 0x02, 0xB4} },
		{0xDE, 0x10, {0x02, 0xD9, 0x03, 0x0E, 0x03, 0x33, 0x03, 0x6F, 0x03, 0x93, 0x03, 0xD7, 0x03, 0xDE, 0x03, 0xE0} },
		{0xDF, 0x04, {0x03, 0xEC, 0x03, 0xFF} },
		{REGFLAG_DELAY, 1, {} },
		{0xE0, 0x10, {0x01, 0x23, 0x01, 0x29, 0x01, 0x35, 0x01, 0x42, 0x01, 0x4A, 0x01, 0x5C, 0x01, 0x6E, 0x01, 0x8A} },
		{0xE1, 0x10, {0x01, 0xA7, 0x01, 0xCF, 0x01, 0xF3, 0x02, 0x2E, 0x02, 0x5E, 0x02, 0x5F, 0x02, 0x8C, 0x02, 0xC1} },
		{0xE2, 0x10, {0x02, 0xE5, 0x03, 0x16, 0x03, 0x36, 0x03, 0x60, 0x03, 0x7C, 0x03, 0x9C, 0x03, 0xAE, 0x03, 0xC6} },
		{0xE3, 0x04, {0x03, 0xFC, 0x03, 0xFF} },
		{REGFLAG_DELAY, 1, {} },
		{0xE4, 0x10, {0x01, 0x0D, 0x01, 0x14, 0x01, 0x21, 0x01, 0x2D, 0x01, 0x38, 0x01, 0x4C, 0x01, 0x5E, 0x01, 0x7E} },
		{0xE5, 0x10, {0x01, 0x99, 0x01, 0xC6, 0x01, 0xEB, 0x02, 0x27, 0x02, 0x59, 0x02, 0x5A, 0x02, 0x88, 0x02, 0xBE} },
		{0xE6, 0x10, {0x02, 0xE2, 0x03, 0x13, 0x03, 0x34, 0x03, 0x5F, 0x03, 0x79, 0x03, 0x99, 0x03, 0xAD, 0x03, 0xC8} },
		{0xE7, 0x04, {0x03, 0xEF, 0x03, 0xFF} },
		{REGFLAG_DELAY, 1, {} },
		{0xE8, 0x10, {0x00, 0x00, 0x00, 0x2D, 0x00, 0x63, 0x00, 0x8D, 0x00, 0xA3, 0x00, 0xCE, 0x00, 0xF2, 0x01, 0x26} },
		{0xE9, 0x10, {0x01, 0x55, 0x01, 0x91, 0x01, 0xC3, 0x02, 0x0F, 0x02, 0x48, 0x02, 0x4A, 0x02, 0x7C, 0x02, 0xB4} },
		{0xEA, 0x10, {0x02, 0xD9, 0x03, 0x0E, 0x03, 0x33, 0x03, 0x6F, 0x03, 0x93, 0x03, 0xD7, 0x03, 0xDE, 0x03, 0xE0} },
		{0xEB, 0x04, {0x03, 0xEC, 0x03, 0xFF} },
		{REGFLAG_DELAY, 1, {} },
		{0xF0, 0x05, {0x55, 0xAA, 0x52, 0x00, 0x00}},
		{REGFLAG_END_OF_TABLE, 0x00, {}}
	},

};

static struct LCM_setting_table lcm_dsi_dispparam_papermode_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {

};

static struct LCM_setting_table lcm_dsi_dispparam_papermode1_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {

};

static struct LCM_setting_table lcm_dsi_dispparam_papermode2_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {

};

static struct LCM_setting_table lcm_dsi_dispparam_papermode3_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {

};

static struct LCM_setting_table lcm_dsi_dispparam_papermode4_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {

};

static struct LCM_setting_table lcm_dsi_dispparam_papermode5_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {

};

static struct LCM_setting_table lcm_dsi_dispparam_papermode6_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {

};

static struct LCM_setting_table lcm_dsi_dispparam_papermode7_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {

};

static struct LCM_setting_table lcm_dsi_dispparam_srgb_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {

};

static struct LCM_setting_table lcm_dsi_dispparam_default_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {

};

static struct LCM_setting_table lcm_dsi_dispparam_ceon_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {
	{	{0xF0, 5, {0x55, 0xAA, 0x52, 0x08, 0x00} },
		{0xCE, 0x0C, {0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20} },
		{0xF0, 5, {0x55, 0xAA, 0x52, 0x00, 0x00} },
		{0x55, 1, {0x80}},
		{REGFLAG_END_OF_TABLE, 0x00, {}}
	},
};

static struct LCM_setting_table lcm_dsi_dispparam_cecabc_on_command[PANEL_TPYE_MAX][PANEL_COMMAND_MAX] = {
	{	{0xF0, 5, {0x55, 0xAA, 0x52, 0x08, 0x00} },
		{0xCE, 0x0C, {0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20} },
		{0xF0, 5, {0x55, 0xAA, 0x52, 0x00, 0x00} },
		{0x55, 1, {0x81}},
		{REGFLAG_END_OF_TABLE, 0x00, {}}
	},
};

static void value2str(char *pbuf)
{
	int i = 0;
	int param_nb = 0;
	int write_len = 0;
	unsigned char *pTemp = &xy_writepoint[0];

	/* pbuf - buffer size must >= 256 */
	for (i = 0; i < 2; i++) {
		write_len = scnprintf(pTemp, 8, "p%d=%d", param_nb, pbuf[i]);
		pTemp += write_len;
		param_nb++;
	}
	pr_info("read %s  from panel\n", xy_writepoint);
}

static void lcm_dsi_dispparam_xy_writepoint(enum panel_type panel_type)
{
	struct dsi_cmd_desc read_tab;
	struct dsi_cmd_desc write_tab;
	unsigned int i = 0;
	if ((PANEL_CSOT_NT36672A == panel_type) || (PANEL_TIANMA_NT36672A == panel_type)) {
		//switch to cmd2 page 1
		write_tab.dtype = 0xFF;
		write_tab.dlen = 1;
		write_tab.payload = vmalloc(1 * sizeof(unsigned char));
		write_tab.payload[0] = 0x20;
		write_tab.vc = 0;
		write_tab.link_state = 1;

		/*read xy writepoint info*/
		memset(&read_tab, 0, sizeof(struct dsi_cmd_desc));
		read_tab.dtype = 0x44;
		read_tab.payload = kmalloc(2 * sizeof(unsigned char), GFP_KERNEL);
		memset(read_tab.payload, 0, 2);
		read_tab.dlen = 1;

		do_lcm_vdo_lp_write_without_lock(&write_tab, 1);
		do_lcm_vdo_lp_read_without_lock(&read_tab, 1);

		read_tab.dtype = 0x45;
		read_tab.payload++;
		read_tab.dlen = 1;
		do_lcm_vdo_lp_read_without_lock(&read_tab, 1);

		read_tab.payload--;
		pr_debug("[%s]read xy writepoint from panel\n", __func__);
		for (i = 0; i < 2; i++)
			pr_debug("[%s]0x%x\n",  __func__, read_tab.payload[i]);

		//switch to cmd1
		write_tab.dtype = 0xFF;
		write_tab.dlen = 1;
		write_tab.payload[0] = 0x10;
		write_tab.vc = 0;
		write_tab.link_state = 1;
		do_lcm_vdo_lp_write_without_lock(&write_tab, 1);

		value2str(read_tab.payload);

		kfree(read_tab.payload);
		vfree(write_tab.payload);
	} else if (PANEL_EBBG_FT8719 == panel_type) {
		write_tab.dtype = 0x00;
		write_tab.dlen = 1;
		write_tab.payload = vmalloc(1 * sizeof(unsigned char));
		write_tab.payload[0] = 0x00;
		write_tab.vc = 0;
		write_tab.link_state = 1;

		/*read xy writepoint info*/
		memset(&read_tab, 0, sizeof(struct dsi_cmd_desc));
		read_tab.dtype = 0xA1;
		read_tab.payload = kmalloc(2 * sizeof(unsigned char), GFP_KERNEL);
		memset(read_tab.payload, 0, 2);
		read_tab.dlen = 2;

		do_lcm_vdo_lp_write_without_lock(&write_tab, 1);
		do_lcm_vdo_lp_read_without_lock(&read_tab, 1);

		pr_debug("[%s]read xy writepoint from panel\n", __func__);
		for (i = 0; i < 2; i++)
			pr_debug("[%s]0x%x\n",  __func__, read_tab.payload[i]);

		value2str(read_tab.payload);

		kfree(read_tab.payload);
		vfree(write_tab.payload);
	}
	return ;
}

int panel_disp_param_send_lock(enum panel_type panel_type,unsigned int param, send_cmd p_func)
{
	unsigned int tmp;
	send_cmd push_table;
	push_table = p_func;
	pr_info("[%s]panel_type = %d,param = 0x%x\n",__func__,panel_type,param);

	tmp = param & 0x0000000F;

	switch (tmp) {
	case DISPPARAM_WARM:		/* warm */
		if (sizeof(lcm_dsi_dispparam_warm_command[panel_type])) {
			pr_info("warm\n");
			push_table(NULL,lcm_dsi_dispparam_warm_command[panel_type],
				sizeof(lcm_dsi_dispparam_warm_command[panel_type]) / sizeof(struct LCM_setting_table), 1);
		}
		break;
	case DISPPARAM_DEFAULT:		/* normal */
		if (sizeof(lcm_dsi_dispparam_default_command[panel_type])) {
			pr_info("default\n");
			push_table(NULL,lcm_dsi_dispparam_default_command[panel_type],
				sizeof(lcm_dsi_dispparam_default_command[panel_type]) / sizeof(struct LCM_setting_table), 1);
		}
		break;
	case DISPPARAM_COLD:		/* cold */
		if (sizeof(lcm_dsi_dispparam_cold_command[panel_type])) {
			pr_info("cold\n");
			push_table(NULL,lcm_dsi_dispparam_cold_command[panel_type],
				sizeof(lcm_dsi_dispparam_cold_command[panel_type]) / sizeof(struct LCM_setting_table), 1);
		}
		break;
	case DISPPARAM_PAPERMODE8:
		if (sizeof(lcm_dsi_dispparam_papermode_command[panel_type])) {
			pr_info("paper mode\n");
			push_table(NULL,lcm_dsi_dispparam_papermode_command[panel_type],
				sizeof(lcm_dsi_dispparam_papermode_command[panel_type]) / sizeof(struct LCM_setting_table), 1);
		}
		break;
	case DISPPARAM_PAPERMODE1:
		if (sizeof(lcm_dsi_dispparam_papermode1_command[panel_type])) {
			pr_info("paper mode1\n");
			push_table(NULL,lcm_dsi_dispparam_papermode1_command[panel_type],
				sizeof(lcm_dsi_dispparam_papermode1_command[panel_type]) / sizeof(struct LCM_setting_table), 1);
		}
		break;
	case DISPPARAM_PAPERMODE2:
		if (sizeof(lcm_dsi_dispparam_papermode2_command[panel_type])) {
			pr_info("paper mode2\n");
			push_table(NULL,lcm_dsi_dispparam_papermode2_command[panel_type],
				sizeof(lcm_dsi_dispparam_papermode2_command[panel_type]) / sizeof(struct LCM_setting_table), 1);
		}
		break;
	case DISPPARAM_PAPERMODE3:
		if (sizeof(lcm_dsi_dispparam_papermode3_command[panel_type])) {
			pr_info("paper mode3\n");
			push_table(NULL,lcm_dsi_dispparam_papermode3_command[panel_type],
				sizeof(lcm_dsi_dispparam_papermode3_command[panel_type]) / sizeof(struct LCM_setting_table), 1);
		}
		break;
	case DISPPARAM_PAPERMODE4:
		if (sizeof(lcm_dsi_dispparam_papermode4_command[panel_type])) {
			pr_info("paper mode4\n");
			push_table(NULL,lcm_dsi_dispparam_papermode4_command[panel_type],
				sizeof(lcm_dsi_dispparam_papermode4_command[panel_type]) / sizeof(struct LCM_setting_table), 1);
		}
		break;
	case DISPPARAM_PAPERMODE5:
		if (sizeof(lcm_dsi_dispparam_papermode5_command[panel_type])) {
			pr_info("paper mode5\n");
			push_table(NULL,lcm_dsi_dispparam_papermode5_command[panel_type],
				sizeof(lcm_dsi_dispparam_papermode5_command[panel_type]) / sizeof(struct LCM_setting_table), 1);
		}
		break;
	case DISPPARAM_PAPERMODE6:
		if (sizeof(lcm_dsi_dispparam_papermode6_command[panel_type])) {
			pr_info("paper mode6\n");
			push_table(NULL,lcm_dsi_dispparam_papermode6_command[panel_type],
				sizeof(lcm_dsi_dispparam_papermode6_command[panel_type]) / sizeof(struct LCM_setting_table), 1);
		}
		break;
	case DISPPARAM_PAPERMODE7:
		if (sizeof(lcm_dsi_dispparam_papermode7_command[panel_type])) {
			pr_info("paper mode7\n");
			push_table(NULL,lcm_dsi_dispparam_papermode7_command[panel_type],
				sizeof(lcm_dsi_dispparam_papermode7_command[panel_type]) / sizeof(struct LCM_setting_table), 1);
		}
		break;
	case DISPPARAM_WHITEPOINT_XY:
		lcm_dsi_dispparam_xy_writepoint(panel_type);
		break;
	default:
		break;
	}

	tmp = param & 0x000000F0;
	switch (tmp) {
	case DISPPARAM_CE_ON:
		if (sizeof(lcm_dsi_dispparam_ceon_command[panel_type])) {
			pr_info("ce on\n");
			push_table(NULL,lcm_dsi_dispparam_ceon_command[panel_type],
				sizeof(lcm_dsi_dispparam_ceon_command[panel_type]) / sizeof(struct LCM_setting_table), 1);
		}
		break;
	case DISPPARAM_CE_OFF:
		if (sizeof(lcm_dsi_dispparam_cecabc_on_command[panel_type])) {
			pr_info("ce&cabc on\n");
			push_table(NULL,lcm_dsi_dispparam_cecabc_on_command[panel_type],
				sizeof(lcm_dsi_dispparam_cecabc_on_command[panel_type]) / sizeof(struct LCM_setting_table), 1);
		}
		break;
	default:
		break;
	}

	tmp = param & 0x00000F00;
	switch (tmp) {
	case DISPPARAM_CABCUI_ON:
		if (sizeof(lcm_dsi_dispparam_cabcguion_command[panel_type])) {
			pr_info("cabc on\n");
			push_table(NULL,lcm_dsi_dispparam_cabcguion_command[panel_type],
				sizeof(lcm_dsi_dispparam_cabcguion_command[panel_type][100]) / sizeof(struct LCM_setting_table), 1);
		}
		break;
	case DISPPARAM_CABCSTILL_ON:
		if (sizeof(lcm_dsi_dispparam_cabcstillon_command[panel_type])) {
			pr_info("cabc still on\n");
			push_table(NULL,lcm_dsi_dispparam_cabcstillon_command[panel_type],
				sizeof(lcm_dsi_dispparam_cabcstillon_command[panel_type]) / sizeof(struct LCM_setting_table), 1);
		}
		break;
	case DISPPARAM_CABCMOVIE_ON:
		if (sizeof(lcm_dsi_dispparam_cabcmovieon_command[panel_type])) {
			pr_info("cabc movie on\n");
			push_table(NULL,lcm_dsi_dispparam_cabcmovieon_command[panel_type],
				sizeof(lcm_dsi_dispparam_cabcmovieon_command[panel_type]) / sizeof(struct LCM_setting_table), 1);
		}
		break;
	case DISPPARAM_CABC_OFF:
		if (sizeof(lcm_dsi_dispparam_cabcoff_command[panel_type])) {
			pr_info("cabc movie on\n");
			push_table(NULL,lcm_dsi_dispparam_cabcoff_command[panel_type],
				sizeof(lcm_dsi_dispparam_cabcoff_command[panel_type]) / sizeof(struct LCM_setting_table), 1);
		}
		break;
	case DISPPARAM_SKIN_CE_CABCUI_ON:
		if (sizeof(lcm_dsi_dispparam_skin_ce_cabcguion_command[panel_type])) {
			pr_info("cabc on\n");
			push_table(NULL,lcm_dsi_dispparam_skin_ce_cabcguion_command[panel_type],
				sizeof(lcm_dsi_dispparam_skin_ce_cabcguion_command[panel_type]) / sizeof(struct LCM_setting_table), 1);
		}
		break;
	case DISPPARAM_SKIN_CE_CABCSTILL_ON:
		if (sizeof(lcm_dsi_dispparam_skin_ce_cabcstillon_command[panel_type])) {
			pr_info("cabc still on\n");
			push_table(NULL,lcm_dsi_dispparam_skin_ce_cabcstillon_command[panel_type],
				sizeof(lcm_dsi_dispparam_skin_ce_cabcstillon_command[panel_type]) / sizeof(struct LCM_setting_table), 1);
		}
		break;
	case DISPPARAM_SKIN_CE_CABCMOVIE_ON:
		if (sizeof(lcm_dsi_dispparam_skin_ce_cabcmovieon_command[panel_type])) {
			pr_info("cabc movie on\n");
			push_table(NULL,lcm_dsi_dispparam_skin_ce_cabcmovieon_command[panel_type],
				sizeof(lcm_dsi_dispparam_skin_ce_cabcmovieon_command[panel_type]) / sizeof(struct LCM_setting_table), 1);
		}
		break;
	case DISPPARAM_SKIN_CE_CABC_OFF:
		if (sizeof(lcm_dsi_dispparam_skin_ce_cabcoff_command[panel_type])) {
			pr_info("cabc movie on\n");
			push_table(NULL,lcm_dsi_dispparam_skin_ce_cabcoff_command[panel_type],
				sizeof(lcm_dsi_dispparam_skin_ce_cabcoff_command[panel_type]) / sizeof(struct LCM_setting_table), 1);
		}
		break;
	case DISPPARAM_DIMMING_OFF:
		if (sizeof(lcm_dsi_dispparam_dimmingoff_command[panel_type])) {
			pr_info("dimming on\n");
			push_table(NULL,lcm_dsi_dispparam_dimmingoff_command[panel_type],
				sizeof(lcm_dsi_dispparam_dimmingoff_command[panel_type]) / sizeof(struct LCM_setting_table), 1);
		}
		break;
	case DISPPARAM_DIMMING:
		if (sizeof(lcm_dsi_dispparam_dimmingon_command[panel_type])) {
			pr_info("smart contrast on\n");
			push_table(NULL,lcm_dsi_dispparam_dimmingon_command[panel_type],
				sizeof(lcm_dsi_dispparam_dimmingon_command[panel_type]) / sizeof(struct LCM_setting_table), 1);
		}
		break;
	default:
		break;
	}

	tmp = param & 0x000F0000;
	switch (tmp) {
	case 0xA0000:
		if (sizeof(lcm_dsi_dispparam_idleon_command[panel_type])) {
			pr_info("idle on\n");
			push_table(NULL,lcm_dsi_dispparam_idleon_command[panel_type],
				sizeof(lcm_dsi_dispparam_idleon_command[panel_type]) / sizeof(struct LCM_setting_table), 1);
		}
		break;
	case DISPPARAM_LCD_HBM_L1_ON:
		pr_info("hbm l1 on\n");
		hbm_brightness_set(DISPPARAM_LCD_HBM_L1_ON);
		break;
	case DISPPARAM_LCD_HBM_L2_ON:
		pr_info("hbm l2 on\n");
		hbm_brightness_set(DISPPARAM_LCD_HBM_L2_ON);
		break;
	case DISPPARAM_LCD_HBM_L3_ON:
		pr_info("hbm l3 on\n");
		hbm_brightness_set(DISPPARAM_LCD_HBM_L3_ON);
		break;
	case DISPPARAM_LCD_HBM_OFF:
		pr_info("hbm off\n");
		hbm_brightness_set(DISPPARAM_LCD_HBM_OFF);
		break;
	case 0xF0000:
		break;
	default:
		break;
	}

	tmp = param & 0x00F00000;
	switch (tmp) {
	case DISPPARAM_NORMALMODE1:
		if (sizeof(lcm_dsi_dispparam_normal1_command[panel_type])) {
			pr_info("normal1\n");
			push_table(NULL,lcm_dsi_dispparam_normal1_command[panel_type],
				sizeof(lcm_dsi_dispparam_normal1_command[panel_type]) / sizeof(struct LCM_setting_table), 1);
		}
		break;
	case DISPPARAM_P3:
		if (sizeof(lcm_dsi_dispparam_normal2_command[panel_type])) {
			pr_info("normal2\n");
			push_table(NULL,lcm_dsi_dispparam_normal2_command[panel_type],
				sizeof(lcm_dsi_dispparam_normal2_command[panel_type]) / sizeof(struct LCM_setting_table), 1);
		}
		break;
	case DISPPARAM_SRGB:
		if (sizeof(lcm_dsi_dispparam_srgb_command[panel_type])) {
			pr_info("sRGB\n");
			push_table(NULL,lcm_dsi_dispparam_srgb_command[panel_type],
				sizeof(lcm_dsi_dispparam_srgb_command[panel_type]) / sizeof(struct LCM_setting_table), 1);
		}
		break;
	default:
		break;
	}

	return 0;
}
