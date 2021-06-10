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
#ifdef CONFIG_ADB_WRITE_PARAM_FEATURE

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "panel_set_disp_param.h"
#include "ddp_drv.h"
#include "lcm_drv.h"
#include "disp_recovery.h"
#include <linux/notifier.h>
#include <linux/fb_notifier.h>

#define REGFLAG_DELAY           0xFE
#define REGFLAG_END_OF_TABLE    0xFF   /* END OF REGISTERS MARKER */

unsigned char xy_writepoint[16] = {0};

#ifdef CONFIG_BACKLIGHT_SUPPORT_LM36273
extern int hbm_brightness_set(int level);
#else
int hbm_brightness_set(int level) { return 0; }
#endif

static struct LCM_setting_table lcm_dsi_dispparam_cabcguion_command[] = {
		{0x55, 1, {0x01}},
		{REGFLAG_END_OF_TABLE, 0x00, {}}

};

static struct LCM_setting_table lcm_dsi_dispparam_cabcstillon_command[] = {
		{0x55, 1, {0x02}},
		{REGFLAG_END_OF_TABLE, 0x00, {}}

};

static struct LCM_setting_table lcm_dsi_dispparam_cabcmovieon_command[] = {
		{0x55, 1, {0x03}},
		{REGFLAG_END_OF_TABLE, 0x00, {}}

};

static struct LCM_setting_table lcm_dsi_dispparam_cabcoff_command[] = {
		{0x55, 1, {0x00}},
		{REGFLAG_END_OF_TABLE, 0x00, {}}

};

static struct LCM_setting_table lcm_dsi_dispparam_skin_ce_cabcguion_command[] = {
		{0x55, 1, {0x01}},
		{REGFLAG_END_OF_TABLE, 0x00, {}}

};

static struct LCM_setting_table lcm_dsi_dispparam_skin_ce_cabcstillon_command[] = {
		{0x55, 1, {0x02}},
		{REGFLAG_END_OF_TABLE, 0x00, {}}

};

static struct LCM_setting_table lcm_dsi_dispparam_skin_ce_cabcmovieon_command[] = {
		{0x55, 1, {0x03}},
		{REGFLAG_END_OF_TABLE, 0x00, {}}

};

static struct LCM_setting_table lcm_dsi_dispparam_skin_ce_cabcoff_command[] = {
		{0x55, 1, {0x00}},
		{REGFLAG_END_OF_TABLE, 0x00, {}}

};
//static struct LCM_setting_table lcm_dsi_dispparam_hbmoff_command[] = {
//		{0x51, 2, {0x07,0xFF}},
//		{REGFLAG_END_OF_TABLE, 0x00, {}}

//};

//static struct LCM_setting_table lcm_dsi_dispparam_hbmon_command[] = {
//		{0x51, 2, {0x07,0xFF}},
//		{REGFLAG_END_OF_TABLE, 0x00, {}}

//};

static struct LCM_setting_table lcm_dsi_dispparam_dimmingon_command[] = {
		{0x53, 1, {0x2C}},
		{REGFLAG_END_OF_TABLE, 0x00, {}}

};

static struct LCM_setting_table lcm_dsi_dispparam_dimmingoff_command[] = {

};

static struct LCM_setting_table lcm_dsi_dispparam_idleon_command[] = {

};
/*
static struct LCM_setting_table lcm_dsi_dispparam_idleoff_command[PANEL_TPYE_MAX] = {

};
*/
static struct LCM_setting_table lcm_dsi_dispparam_warm_command[] = {
		{0xF0, 0x05, {0x55, 0xAA, 0x52, 0x08, 0x02} },
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

};

//static struct LCM_setting_table lcm_dsi_dispparam_normal1_command[] = {
//
//};

//static struct LCM_setting_table lcm_dsi_dispparam_normal2_command[] = {
//
//};

static struct LCM_setting_table lcm_dsi_dispparam_cold_command[] = {
		{0xF0, 0x05, {0x55, 0xAA, 0x52, 0x08, 0x02} },
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


};

static struct LCM_setting_table lcm_dsi_dispparam_papermode_command[] = {

};

static struct LCM_setting_table lcm_dsi_dispparam_papermode1_command[] = {

};

static struct LCM_setting_table lcm_dsi_dispparam_papermode2_command[] = {

};

static struct LCM_setting_table lcm_dsi_dispparam_papermode3_command[] = {

};

static struct LCM_setting_table lcm_dsi_dispparam_papermode4_command[] = {

};

static struct LCM_setting_table lcm_dsi_dispparam_papermode5_command[] = {

};

static struct LCM_setting_table lcm_dsi_dispparam_papermode6_command[] = {

};

static struct LCM_setting_table lcm_dsi_dispparam_papermode7_command[] = {

};

//static struct LCM_setting_table lcm_dsi_dispparam_srgb_command[] = {
//
//};

static struct LCM_setting_table lcm_dsi_dispparam_default_command[] = {

};

static struct LCM_setting_table lcm_dsi_dispparam_ceon_command[] = {
		{0xF0, 5, {0x55, 0xAA, 0x52, 0x08, 0x00} },
		{0xCE, 0x0C, {0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20} },
		{0xF0, 5, {0x55, 0xAA, 0x52, 0x00, 0x00} },
		{0x55, 1, {0x80}},
		{REGFLAG_END_OF_TABLE, 0x00, {}}

};

static struct LCM_setting_table lcm_dsi_dispparam_cecabc_on_command[] = {
		{0xF0, 5, {0x55, 0xAA, 0x52, 0x08, 0x00} },
		{0xCE, 0x0C, {0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20} },
		{0xF0, 5, {0x55, 0xAA, 0x52, 0x00, 0x00} },
		{0x55, 1, {0x81}},
		{REGFLAG_END_OF_TABLE, 0x00, {}}

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
	if (PANEL_SUMSUNG_FT3418 == panel_type) {
		//switch F0 write 5A A5
		write_tab.dtype = 0xF0;
		write_tab.dlen = 1;
		write_tab.payload = vmalloc(2 * sizeof(unsigned char));
		write_tab.payload[0] = 0x5A;
		write_tab.payload[1] = 0xA5;
		write_tab.vc = 0;
		write_tab.link_state = 1;

		//switch B0 write 11 A1
		write_tab.dtype = 0xB0;
		write_tab.dlen = 1;
		write_tab.payload = vmalloc(2 * sizeof(unsigned char));
		write_tab.payload[0] = 0x11;
		write_tab.payload[1] = 0xA1;
		write_tab.vc = 0;
		write_tab.link_state = 1;

		/*read xy writepoint info*/
		memset(&read_tab, 0, sizeof(struct dsi_cmd_desc));
		read_tab.dtype = 0xA1;
		read_tab.payload = kmalloc(2 * sizeof(unsigned char), GFP_KERNEL);
		memset(read_tab.payload, 0, 2);
		read_tab.dlen = 1;

		do_lcm_vdo_lp_write_without_lock(&write_tab, 1);
		do_lcm_vdo_lp_read_without_lock(&read_tab, 1);

		read_tab.dtype = 0xA1;
		read_tab.payload++;
		read_tab.dlen = 1;
		do_lcm_vdo_lp_read_without_lock(&read_tab, 1);

		read_tab.payload--;
		pr_debug("[%s]read xy writepoint from panel\n", __func__);
		for (i = 0; i < 2; i++)
			pr_debug("[%s]0x%x\n",  __func__, read_tab.payload[i]);

		//switch F0 write 5A A5
		write_tab.dtype = 0xF0;
		write_tab.dlen = 1;
		write_tab.payload = vmalloc(2 * sizeof(unsigned char));
		write_tab.payload[0] = 0x5A;
		write_tab.payload[1] = 0xA5;
		write_tab.vc = 0;
		write_tab.link_state = 1;

		//switch B0 write 11 A1
		write_tab.dtype = 0xB0;
		write_tab.dlen = 1;
		write_tab.payload = vmalloc(2 * sizeof(unsigned char));
		write_tab.payload[0] = 0x11;
		write_tab.payload[1] = 0xA1;
		write_tab.vc = 0;
		write_tab.link_state = 1;
		do_lcm_vdo_lp_write_without_lock(&write_tab, 1);

		value2str(read_tab.payload);

		kfree(read_tab.payload);
		vfree(write_tab.payload);
	}
	return ;
}


int panel_disp_backlight_send_lock(unsigned int level)
{
	struct dsi_cmd_desc backlight_set;
	static int Compensation_level=0x00;

	if(level >=0 && level <= 31 && Compensation_level!=0x1D){

		Compensation_level = 0x1D;
		backlight_set.payload = vmalloc(sizeof(unsigned char));
		backlight_set.vc = 0;
		backlight_set.dlen = 1;
		backlight_set.link_state = 1;
		backlight_set.dtype = 0xFE;
		backlight_set.payload[0] = 0x20;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);

		backlight_set.dtype = 0xB8;
		backlight_set.payload[0] = 0x1D;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);

		backlight_set.dtype = 0xB9;
		backlight_set.payload[0] = 0x1D;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);

		backlight_set.dtype = 0xBA;
		backlight_set.payload[0] = 0x1D;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);

		backlight_set.dtype = 0xFE;
		backlight_set.payload[0] = 0x00;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);

		//pr_info("[LCM]%s,Normal1_backlight: level = %d lcd_bl_en = %d\n", __func__, level,lcd_bl_en);
	}
	else if(level > 31 && level <= 509 && Compensation_level!=0x12){

		Compensation_level = 0x12;
		backlight_set.payload = vmalloc(sizeof(unsigned char));
		backlight_set.vc = 0;
		backlight_set.dlen = 1;
		backlight_set.link_state = 1;
		backlight_set.dtype = 0xFE;
		backlight_set.payload[0] = 0x20;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);

		backlight_set.dtype = 0xB8;
		backlight_set.payload[0] = 0x12;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);

		backlight_set.dtype = 0xB9;
		backlight_set.payload[0] = 0x12;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);

		backlight_set.dtype = 0xBA;
		backlight_set.payload[0] = 0x12;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);

		backlight_set.dtype = 0xFE;
		backlight_set.payload[0] = 0x00;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);

		//pr_info("[LCM]%s,Normal1_backlight: level = %d lcd_bl_en = %d\n", __func__, level,lcd_bl_en);
	}
	else if(level >=510 && level <= 767 && Compensation_level!=0x0D){

		Compensation_level = 0x0D;
		backlight_set.payload = vmalloc(sizeof(unsigned char));
		backlight_set.vc = 0;
		backlight_set.dlen = 1;
		backlight_set.link_state = 1;
		backlight_set.dtype = 0xFE;
		backlight_set.payload[0] = 0x20;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);

		backlight_set.dtype = 0xB8;
		backlight_set.payload[0] = 0x0D;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);

		backlight_set.dtype = 0xB9;
		backlight_set.payload[0] = 0x0D;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);

		backlight_set.dtype = 0xBA;
		backlight_set.payload[0] = 0x0D;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);
		
		backlight_set.dtype = 0xFE;
		backlight_set.payload[0] = 0x00;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);

		pr_info("normal3 %d",level);
	}
	else if(level >=768 && level <= 1023 && Compensation_level!=0x0B){

		Compensation_level = 0x0B;
		backlight_set.payload = vmalloc(sizeof(unsigned char));
		backlight_set.vc = 0;
		backlight_set.dlen = 1;
		backlight_set.link_state = 1;
		backlight_set.dtype = 0xFE;
		backlight_set.payload[0] = 0x20;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);

		backlight_set.dtype = 0xB8;
		backlight_set.payload[0] = 0x0B;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);

		backlight_set.dtype = 0xB9;
		backlight_set.payload[0] = 0x0B;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);
		
		backlight_set.dtype = 0xBA;
		backlight_set.payload[0] = 0x0B;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);
		
		backlight_set.dtype = 0xFE;
		backlight_set.payload[0] = 0x00;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);

		pr_info("normal3 %d",level);
	}
	else if(level >=1024 && level <= 1279 && Compensation_level!=0x0A){

		Compensation_level = 0x0A;
		backlight_set.payload = vmalloc(sizeof(unsigned char));
		backlight_set.vc = 0;
		backlight_set.dlen = 1;
		backlight_set.link_state = 1;
		backlight_set.dtype = 0xFE;
		backlight_set.payload[0] = 0x20;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);
		
		backlight_set.dtype = 0xB8;
		backlight_set.payload[0] = 0x0A;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);
		
		backlight_set.dtype = 0xB9;
		backlight_set.payload[0] = 0x0A;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);
		
		backlight_set.dtype = 0xBA;
		backlight_set.payload[0] = 0x0A;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);
		
		backlight_set.dtype = 0xFE;
		backlight_set.payload[0] = 0x00;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);

		pr_info("[LCM]Normal4_backlight: level = %d\n", level);
	}
	else if(level >=1280 && level <= 1535 && Compensation_level!=0x09){

		Compensation_level = 0x09;
		backlight_set.payload = vmalloc(sizeof(unsigned char));
		backlight_set.vc = 0;
		backlight_set.dlen = 1;
		backlight_set.link_state = 1;
		backlight_set.dtype = 0xFE;
		backlight_set.payload[0] = 0x20;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);
		
		backlight_set.dtype = 0xB8;
		backlight_set.payload[0] = 0x09;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);
		
		backlight_set.dtype = 0xB9;
		backlight_set.payload[0] = 0x09;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);
		
		backlight_set.dtype = 0xBA;
		backlight_set.payload[0] = 0x09;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);
		
		backlight_set.dtype = 0xFE;
		backlight_set.payload[0] = 0x00;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);

		pr_info("[Normal5_backlight: level = %d\n",level);
	}
	else if(level >=1536 && Compensation_level!=0x08){

		Compensation_level = 0x08;
		backlight_set.payload = vmalloc(sizeof(unsigned char));
		backlight_set.vc = 0;
		backlight_set.dlen = 1;
		backlight_set.link_state = 1;
		backlight_set.dtype = 0xFE;
		backlight_set.payload[0] = 0x20;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);
		
		backlight_set.dtype = 0xB8;
		backlight_set.payload[0] = 0x08;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);
		
		backlight_set.dtype = 0xB9;
		backlight_set.payload[0] = 0x08;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);
		
		backlight_set.dtype = 0xBA;
		backlight_set.payload[0] = 0x08;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);
		
		backlight_set.dtype = 0xFE;
		backlight_set.payload[0] = 0x00;
		do_lcm_vdo_lp_brief_write_without_lock(&backlight_set,1);

		pr_info("[LCM]Normal6_backlight: level = %d lcd_bl_en = %d\n",level);
	}
	vfree(backlight_set.payload);
	return 0;
}

int panel_disp_param_send_lock(enum panel_type panel_type,unsigned int param, send_cmd p_func)
{
	struct dsi_cmd_desc dimming_hbm_off;
	struct dsi_cmd_desc dimming_hbm_on;
	struct dsi_cmd_desc hbm_on;
	struct dsi_cmd_desc level_key_enable_tb;
	struct dsi_cmd_desc level_key_disable_tb;
	struct dsi_cmd_desc crc_enable_tb;
	struct dsi_cmd_desc crc_bypass_tb;
	struct dsi_cmd_desc crc_srgb_offset_tb;
	struct dsi_cmd_desc crc_p3_offset_tb;
	struct dsi_cmd_desc crc_p3_tb;
	struct dsi_cmd_desc crc_srgb_tb;
	struct dsi_cmd_desc flat_gamma_tb;
	struct dsi_cmd_desc flat_gamma_value_tb;
	struct dsi_cmd_desc flat_mode_01;
	struct dsi_cmd_desc flat_mode_02;
	struct dsi_cmd_desc flat_mode_03;
	struct dsi_cmd_desc flat_mode_04;
	struct dsi_cmd_desc doze_brightness_set;
	struct fb_drm_notify_data g_notify_data;

	unsigned int tmp;
	int event = FB_DRM_BLANK_POWERDOWN;
	send_cmd push_table;
	push_table = p_func;
	g_notify_data.data = &event;
	pr_info("[%s]panel_type = %d,param = 0x%x\n",__func__,panel_type,param);

	tmp = param & 0x0000000F;

	switch (tmp) {
	case DISPPARAM_WARM:		/* warm */
		pr_info("warm\n");
		push_table(lcm_dsi_dispparam_warm_command,
			sizeof(lcm_dsi_dispparam_warm_command) / sizeof(struct LCM_setting_table), 1);
		break;
	case DISPPARAM_DEFAULT:		/* normal */
		pr_info("default\n");
		push_table(lcm_dsi_dispparam_default_command,
			sizeof(lcm_dsi_dispparam_default_command) / sizeof(struct LCM_setting_table), 1);
		break;
	case DISPPARAM_COLD:		/* cold */
		pr_info("cold\n");
		push_table(lcm_dsi_dispparam_cold_command,
			sizeof(lcm_dsi_dispparam_cold_command) / sizeof(struct LCM_setting_table), 1);
		break;
	case DISPPARAM_PAPERMODE8:
		pr_info("paper mode\n");
		push_table(lcm_dsi_dispparam_papermode_command,
			sizeof(lcm_dsi_dispparam_papermode_command) / sizeof(struct LCM_setting_table), 1);
		break;
	case DISPPARAM_PAPERMODE1:
		pr_info("paper mode1\n");
		push_table(lcm_dsi_dispparam_papermode1_command,
			sizeof(lcm_dsi_dispparam_papermode1_command) / sizeof(struct LCM_setting_table), 1);
		break;
	case DISPPARAM_PAPERMODE2:
		pr_info("paper mode2\n");
		push_table(lcm_dsi_dispparam_papermode2_command,
			sizeof(lcm_dsi_dispparam_papermode2_command) / sizeof(struct LCM_setting_table), 1);
		break;
	case DISPPARAM_PAPERMODE3:
		pr_info("paper mode3\n");
		push_table(lcm_dsi_dispparam_papermode3_command,
			sizeof(lcm_dsi_dispparam_papermode3_command) / sizeof(struct LCM_setting_table), 1);
		break;
	case DISPPARAM_PAPERMODE4:
		pr_info("paper mode4\n");
		push_table(lcm_dsi_dispparam_papermode4_command,
			sizeof(lcm_dsi_dispparam_papermode4_command) / sizeof(struct LCM_setting_table), 1);
		break;
	case DISPPARAM_PAPERMODE5:
		pr_info("paper mode5\n");
		push_table(lcm_dsi_dispparam_papermode5_command,
			sizeof(lcm_dsi_dispparam_papermode5_command) / sizeof(struct LCM_setting_table), 1);
		break;
	case DISPPARAM_PAPERMODE6:
		pr_info("paper mode6\n");
		push_table(lcm_dsi_dispparam_papermode6_command,
			sizeof(lcm_dsi_dispparam_papermode6_command) / sizeof(struct LCM_setting_table), 1);
		break;
	case DISPPARAM_PAPERMODE7:
		pr_info("paper mode7\n");
		push_table(lcm_dsi_dispparam_papermode7_command,
			sizeof(lcm_dsi_dispparam_papermode7_command) / sizeof(struct LCM_setting_table), 1);
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
		pr_info("ce on\n");
		push_table(lcm_dsi_dispparam_ceon_command,
			sizeof(lcm_dsi_dispparam_ceon_command) / sizeof(struct LCM_setting_table), 1);
		break;
	case DISPPARAM_CE_OFF:
		pr_info("ce&cabc on\n");
		push_table(lcm_dsi_dispparam_cecabc_on_command,
			sizeof(lcm_dsi_dispparam_cecabc_on_command) / sizeof(struct LCM_setting_table), 1);
		break;
	default:
		break;
	}

	tmp = param & 0x00000F00;
	switch (tmp) {
	case DISPPARAM_CABCUI_ON:
		pr_info("cabc on\n");
		push_table(lcm_dsi_dispparam_cabcguion_command,
			sizeof(lcm_dsi_dispparam_cabcguion_command) / sizeof(struct LCM_setting_table), 1);
		break;
	case DISPPARAM_CABCSTILL_ON:
		pr_info("cabc still on\n");
		push_table(lcm_dsi_dispparam_cabcstillon_command,
			sizeof(lcm_dsi_dispparam_cabcstillon_command) / sizeof(struct LCM_setting_table), 1);
		break;
	case DISPPARAM_CABCMOVIE_ON:
		pr_info("cabc movie on\n");
		push_table(lcm_dsi_dispparam_cabcmovieon_command,
			sizeof(lcm_dsi_dispparam_cabcmovieon_command) / sizeof(struct LCM_setting_table), 1);
		break;
	case DISPPARAM_CABC_OFF:
		pr_info("cabc movie on\n");
		push_table(lcm_dsi_dispparam_cabcoff_command,
			sizeof(lcm_dsi_dispparam_cabcoff_command) / sizeof(struct LCM_setting_table), 1);
		break;
	case DISPPARAM_SKIN_CE_CABCUI_ON:
		pr_info("cabc on\n");
		push_table(lcm_dsi_dispparam_skin_ce_cabcguion_command,
			sizeof(lcm_dsi_dispparam_skin_ce_cabcguion_command) / sizeof(struct LCM_setting_table), 1);
		break;
	case DISPPARAM_SKIN_CE_CABCSTILL_ON:
		pr_info("cabc still on\n");
		push_table(lcm_dsi_dispparam_skin_ce_cabcstillon_command,
			sizeof(lcm_dsi_dispparam_skin_ce_cabcstillon_command) / sizeof(struct LCM_setting_table), 1);
		break;
	case DISPPARAM_SKIN_CE_CABCMOVIE_ON:
		pr_info("cabc movie on\n");
		push_table(lcm_dsi_dispparam_skin_ce_cabcmovieon_command,
			sizeof(lcm_dsi_dispparam_skin_ce_cabcmovieon_command) / sizeof(struct LCM_setting_table), 1);
		break;
	case DISPPARAM_SKIN_CE_CABC_OFF:
		pr_info("cabc movie on\n");
		push_table(lcm_dsi_dispparam_skin_ce_cabcoff_command,
			sizeof(lcm_dsi_dispparam_skin_ce_cabcoff_command) / sizeof(struct LCM_setting_table), 1);
		break;
	case DISPPARAM_DIMMING_OFF:
		pr_info("dimming on\n");
		push_table(lcm_dsi_dispparam_dimmingoff_command,
			sizeof(lcm_dsi_dispparam_dimmingoff_command) / sizeof(struct LCM_setting_table), 1);
		break;
	case DISPPARAM_DIMMING:
		pr_info("smart contrast on\n");
		push_table(lcm_dsi_dispparam_dimmingon_command,
			sizeof(lcm_dsi_dispparam_dimmingon_command) / sizeof(struct LCM_setting_table), 1);
		break;
	default:
		break;
	}

	tmp = param & 0x000F0000;
	switch (tmp) {
	case 0xA0000:
		pr_info("idle on\n");
		push_table(lcm_dsi_dispparam_idleon_command,
			sizeof(lcm_dsi_dispparam_idleon_command) / sizeof(struct LCM_setting_table), 1);
		break;
	case DISPPARAM_HBM_ON:
		pr_info("hbm l1 on\n");
		if(panel_type == PANEL_SUMSUNG_FT3418)
		{
		dimming_hbm_on.payload = vmalloc(sizeof(unsigned char));
		dimming_hbm_on.vc = 0;
		dimming_hbm_on.dlen = 1;
		dimming_hbm_on.link_state = 1;
		dimming_hbm_on.dtype = 0x53;
		dimming_hbm_on.payload[0] = 0xE8;
		do_lcm_vdo_lp_write_without_lock(&dimming_hbm_on,1);

		hbm_on.payload = vmalloc(2 * sizeof(unsigned char));
		hbm_on.vc = 0;
		hbm_on.dlen = 2;
		hbm_on.link_state = 1;
		hbm_on.dtype = 0x51;
		hbm_on.payload[0] = 0x07;
		hbm_on.payload[1] = 0xFF;

		do_lcm_vdo_lp_write_without_lock(&hbm_on,1);

		}
		else if(panel_type == PANEL_VISIONOX)
		{
		dimming_hbm_on.payload = vmalloc(sizeof(unsigned char));
		dimming_hbm_on.vc = 0;
		dimming_hbm_on.dlen = 1;
		dimming_hbm_on.link_state = 1;
		dimming_hbm_on.dtype = 0xFE;
		dimming_hbm_on.payload[0] = 0x00;
		do_lcm_vdo_lp_write_without_lock(&dimming_hbm_on,1);

		hbm_on.payload = vmalloc(2 * sizeof(unsigned char));
		hbm_on.vc = 0;
		hbm_on.dlen = 2;
		hbm_on.link_state = 1;
		hbm_on.dtype = 0x51;
		hbm_on.payload[0] = 0x0F;
		hbm_on.payload[1] = 0xFF;

		do_lcm_vdo_lp_write_without_lock(&hbm_on,1);
		}
		vfree(hbm_on.payload);
		vfree(dimming_hbm_on.payload);
		break;
	case DISPPARAM_HBM_OFF:
		pr_info("hbm off\n");
		if(panel_type == PANEL_SUMSUNG_FT3418)
		{
		dimming_hbm_off.payload = vmalloc(sizeof(unsigned char));
		dimming_hbm_off.vc = 0;
		dimming_hbm_off.dlen = 1;
		dimming_hbm_off.link_state = 1;
		dimming_hbm_off.dtype = 0x53;
		*(dimming_hbm_off.payload) = 0x28;
		do_lcm_vdo_lp_write_without_lock(&dimming_hbm_off,1);

		}
		else if(panel_type == PANEL_VISIONOX)
		{
		dimming_hbm_off.payload = vmalloc(sizeof(unsigned char));
		dimming_hbm_off.vc = 0;
		dimming_hbm_off.dlen = 1;
		dimming_hbm_off.link_state = 1;
		dimming_hbm_off.dtype = 0xFE;
		*(dimming_hbm_off.payload) = 0x00;
		do_lcm_vdo_lp_write_without_lock(&dimming_hbm_off,1);

		}
		vfree(dimming_hbm_off.payload);
		break;
	default:
		break;
	}

	tmp = param & 0x00F00000;
	switch (tmp) {
	case DISPPARAM_CRC_OFF:
		pr_info("flat mode off begin\n");
		
		flat_mode_01.payload = vmalloc(2 * sizeof(unsigned char));
		flat_mode_01.vc = 0;
		flat_mode_01.dlen = 2;
		flat_mode_01.link_state = 1;
		flat_mode_01.dtype = 0xF0;
		flat_mode_01.payload[0] = 0x5A;
		flat_mode_01.payload[1] = 0x5A;
		do_lcm_vdo_lp_write_without_lock(&flat_mode_01,1);
		
		flat_mode_02.payload = vmalloc(2 * sizeof(unsigned char));
		flat_mode_02.vc = 0;
		flat_mode_02.dlen = 2;
		flat_mode_02.link_state = 1;
		flat_mode_02.dtype = 0xB0;
		flat_mode_02.payload[0] = 0x01;
		flat_mode_02.payload[1] = 0xB8;
		do_lcm_vdo_lp_write_without_lock(&flat_mode_02,1);
		
		flat_mode_03.payload = vmalloc(sizeof(unsigned char));
		flat_mode_03.vc = 0;
		flat_mode_03.dlen = 1;
		flat_mode_03.link_state = 1;
		flat_mode_03.dtype = 0xB8;
		flat_mode_03.payload[0] = 0x09;
		do_lcm_vdo_lp_write_without_lock(&flat_mode_03,1);

		flat_mode_04.payload = vmalloc(2 * sizeof(unsigned char));
		flat_mode_04.vc = 0;
		flat_mode_04.dlen = 2;
		flat_mode_04.link_state = 1;
		flat_mode_04.dtype = 0xF0;
		flat_mode_04.payload[0] = 0xA5;
		flat_mode_04.payload[1] = 0xA5;
		do_lcm_vdo_lp_write_without_lock(&flat_mode_04,1);
		
		vfree(flat_mode_01.payload);
		vfree(flat_mode_02.payload);
		vfree(flat_mode_03.payload);
		vfree(flat_mode_04.payload);
		pr_info("flat mode off end\n");

		pr_info("DISPPARAM_NORMALMODE1 CRC off\n");

		/*level_key_enable_tb 0xf0 -> 0x5a 0x5a*/
		level_key_enable_tb.payload = vmalloc(2 * sizeof(unsigned char));
		level_key_enable_tb.vc = 0;
		level_key_enable_tb.dlen = 2;
		level_key_enable_tb.link_state = 1;
		level_key_enable_tb.dtype = 0xF0;
		level_key_enable_tb.payload[0] = 0x5A;
		level_key_enable_tb.payload[1] = 0x5A;
		do_lcm_vdo_lp_write_without_lock(&level_key_enable_tb,1);

		/*crc_enable_tb[] = {0x80, 0x01};*/
		crc_enable_tb.payload = vmalloc(sizeof(unsigned char));
		crc_enable_tb.vc = 0;
		crc_enable_tb.dlen = 1;
		crc_enable_tb.link_state = 1;
		crc_enable_tb.dtype = 0x80;
		crc_enable_tb.payload[0] = 0x01;
		do_lcm_vdo_lp_write_without_lock(&crc_enable_tb,1);

		/*crc_bypass_tb[] = {0xB1, 0x01};*/
		crc_bypass_tb.payload = vmalloc(sizeof(unsigned char));
		crc_bypass_tb.vc = 0;
		crc_bypass_tb.dlen = 1;
		crc_bypass_tb.link_state = 1;
		crc_bypass_tb.dtype = 0xB1;
		crc_bypass_tb.payload[0] = 0x01;
		do_lcm_vdo_lp_write_without_lock(&crc_bypass_tb,1);

		/*flat_gamma_tb[] = {0xB0, 0x01, 0xB8};*/
		flat_gamma_tb.payload = vmalloc(2 * sizeof(unsigned char));
		flat_gamma_tb.vc = 0;
		flat_gamma_tb.dlen = 2;
		flat_gamma_tb.link_state = 1;
		flat_gamma_tb.dtype = 0xB0;
		flat_gamma_tb.payload[0] = 0x01;
		flat_gamma_tb.payload[1] = 0xB8;
		do_lcm_vdo_lp_write_without_lock(&flat_gamma_tb,1);

		/*flat_gamma_value_tb[] = {0xB8, 0x09};*/
		flat_gamma_value_tb.payload = vmalloc(sizeof(unsigned char));
		flat_gamma_value_tb.vc = 0;
		flat_gamma_value_tb.dlen = 1;
		flat_gamma_value_tb.link_state = 1;
		flat_gamma_value_tb.dtype = 0xB0;
		flat_gamma_value_tb.payload[0] = 0x09;
		do_lcm_vdo_lp_write_without_lock(&flat_gamma_value_tb,1);

		/*level_key_disable_tb[] = {0xF0, 0xA5, 0xA5};*/
		level_key_disable_tb.payload = vmalloc(2 * sizeof(unsigned char));
		level_key_disable_tb.vc = 0;
		level_key_disable_tb.dlen = 2;
		level_key_disable_tb.link_state = 1;
		level_key_disable_tb.dtype = 0xF0;
		level_key_disable_tb.payload[0] = 0xA5;
		level_key_disable_tb.payload[1] = 0xA5;
		do_lcm_vdo_lp_write_without_lock(&level_key_disable_tb,1);

		vfree(crc_enable_tb.payload);
		vfree(level_key_enable_tb.payload);
		vfree(crc_bypass_tb.payload);
		vfree(flat_gamma_tb.payload);
		vfree(flat_gamma_value_tb.payload);
		vfree(level_key_disable_tb.payload);
		break;
	case DISPPARAM_P3:
		pr_info("flat mode off begin\n");
		
		flat_mode_01.payload = vmalloc(2 * sizeof(unsigned char));
		flat_mode_01.vc = 0;
		flat_mode_01.dlen = 2;
		flat_mode_01.link_state = 1;
		flat_mode_01.dtype = 0xF0;
		flat_mode_01.payload[0] = 0x5A;
		flat_mode_01.payload[1] = 0x5A;
		do_lcm_vdo_lp_write_without_lock(&flat_mode_01,1);
		
		flat_mode_02.payload = vmalloc(2 * sizeof(unsigned char));
		flat_mode_02.vc = 0;
		flat_mode_02.dlen = 2;
		flat_mode_02.link_state = 1;
		flat_mode_02.dtype = 0xB0;
		flat_mode_02.payload[0] = 0x01;
		flat_mode_02.payload[1] = 0xB8;
		do_lcm_vdo_lp_write_without_lock(&flat_mode_02,1);
		
		flat_mode_03.payload = vmalloc(sizeof(unsigned char));
		flat_mode_03.vc = 0;
		flat_mode_03.dlen = 1;
		flat_mode_03.link_state = 1;
		flat_mode_03.dtype = 0xB8;
		flat_mode_03.payload[0] = 0x09;
		do_lcm_vdo_lp_write_without_lock(&flat_mode_03,1);

		flat_mode_04.payload = vmalloc(2 * sizeof(unsigned char));
		flat_mode_04.vc = 0;
		flat_mode_04.dlen = 2;
		flat_mode_04.link_state = 1;
		flat_mode_04.dtype = 0xF0;
		flat_mode_04.payload[0] = 0xA5;
		flat_mode_04.payload[1] = 0xA5;
		do_lcm_vdo_lp_write_without_lock(&flat_mode_04,1);
		
		vfree(flat_mode_01.payload);
		vfree(flat_mode_02.payload);
		vfree(flat_mode_03.payload);
		vfree(flat_mode_04.payload);
		pr_info("flat mode off end\n");

		pr_info("DISPPARAM_P3 CRC p3\n");

		crc_enable_tb.payload = vmalloc(sizeof(unsigned char));
		crc_enable_tb.vc = 0;
		crc_enable_tb.dlen = 1;
		crc_enable_tb.link_state = 1;
		crc_enable_tb.dtype = 0x80;
		crc_enable_tb.payload[0] = 0x91;
		do_lcm_vdo_lp_write_without_lock(&crc_enable_tb,1);

		/*level_key_enable_tb 0xf0 -> 0x5a 0x5a*/
		level_key_enable_tb.payload = vmalloc(2 * sizeof(unsigned char));
		level_key_enable_tb.vc = 0;
		level_key_enable_tb.dlen = 2;
		level_key_enable_tb.link_state = 1;
		level_key_enable_tb.dtype = 0xF0;
		level_key_enable_tb.payload[0] = 0x5A;
		level_key_enable_tb.payload[1] = 0x5A;
		do_lcm_vdo_lp_write_without_lock(&level_key_enable_tb,1);

		/*crc_bypass_tb {0xB1, 0x00};*/
		crc_bypass_tb.payload = vmalloc(sizeof(unsigned char));
		crc_bypass_tb.vc = 0;
		crc_bypass_tb.dlen = 1;
		crc_bypass_tb.link_state = 1;
		crc_bypass_tb.dtype = 0xB1;
		crc_bypass_tb.payload[0] = 0x00;
		do_lcm_vdo_lp_write_without_lock(&crc_bypass_tb,1);

		/*crc_srgb_offset_tb[] = {0xB0, 0x01, 0xB1};*/
		crc_p3_offset_tb.payload = vmalloc(2 * sizeof(unsigned char));
		crc_p3_offset_tb.vc = 0;
		crc_p3_offset_tb.dlen = 1;
		crc_p3_offset_tb.link_state = 1;
		crc_p3_offset_tb.dtype = 0xB0;
		crc_p3_offset_tb.payload[0] = 0x16;
		crc_p3_offset_tb.payload[1] = 0xB1;
		do_lcm_vdo_lp_write_without_lock(&crc_p3_offset_tb,1);

		/*crc_p3_tb[] = {0xB1, 0xD7, 0x00, 0x00, 0x0F, 0xC9, 0x02, 0x0A, 0x06, 0xC2,
					0x1D, 0xF1, 0xDC, 0xFC, 0x00, 0xE1, 0xEC, 0xDA, 0x03, 0xFF, 0xFF, 0xFF};*/
		crc_p3_tb.payload = vmalloc(21 * sizeof(unsigned char));
		crc_p3_tb.vc = 0;
		crc_p3_tb.dlen = 21;
		crc_p3_tb.link_state = 1;
		crc_p3_tb.dtype = 0xB1;
		crc_p3_tb.payload[0] = 0xD7;
		crc_p3_tb.payload[1] = 0x00;
		crc_p3_tb.payload[2] = 0x00;
		crc_p3_tb.payload[3] = 0x0F;
		crc_p3_tb.payload[4] = 0xC9;
		crc_p3_tb.payload[5] = 0x02;
		crc_p3_tb.payload[6] = 0x0A;
		crc_p3_tb.payload[7] = 0x06;
		crc_p3_tb.payload[8] = 0xC2;
		crc_p3_tb.payload[9] = 0x1D;
		crc_p3_tb.payload[10] = 0xF1;
		crc_p3_tb.payload[11] = 0xDC;
		crc_p3_tb.payload[12] = 0xFC;
		crc_p3_tb.payload[13] = 0x00;
		crc_p3_tb.payload[14] = 0xE1;
		crc_p3_tb.payload[15] = 0xEC;
		crc_p3_tb.payload[16] = 0xDA;
		crc_p3_tb.payload[17] = 0x03;
		crc_p3_tb.payload[18] = 0xFF;
		crc_p3_tb.payload[19] = 0xFF;
		crc_p3_tb.payload[20] = 0xFF;
		do_lcm_vdo_lp_write_without_lock(&crc_p3_tb,1);

		/*flat_gamma_tb[] = {0xB0, 0x01, 0xB8};*/
		flat_gamma_tb.payload = vmalloc(2 * sizeof(unsigned char));
		flat_gamma_tb.vc = 0;
		flat_gamma_tb.dlen = 2;
		flat_gamma_tb.link_state = 1;
		flat_gamma_tb.dtype = 0xB0;
		flat_gamma_tb.payload[0] = 0x01;
		flat_gamma_tb.payload[1] = 0xB8;
		do_lcm_vdo_lp_write_without_lock(&flat_gamma_tb,1);

		/*flat_gamma_value_tb[] = {0xB8, 0x09};*/
		flat_gamma_value_tb.payload = vmalloc(sizeof(unsigned char));
		flat_gamma_value_tb.vc = 0;
		flat_gamma_value_tb.dlen = 1;
		flat_gamma_value_tb.link_state = 1;
		flat_gamma_value_tb.dtype = 0xB0;
		flat_gamma_value_tb.payload[0] = 0x09;
		do_lcm_vdo_lp_write_without_lock(&flat_gamma_value_tb,1);

		/*level_key_disable_tb[] = {0xF0, 0xA5, 0xA5};*/
		level_key_disable_tb.payload = vmalloc(2 * sizeof(unsigned char));
		level_key_disable_tb.vc = 0;
		level_key_disable_tb.dlen = 2;
		level_key_disable_tb.link_state = 1;
		level_key_disable_tb.dtype = 0xF0;
		level_key_disable_tb.payload[0] = 0xA5;
		level_key_disable_tb.payload[1] = 0xA5;
		do_lcm_vdo_lp_write_without_lock(&level_key_disable_tb,1);

		vfree(crc_enable_tb.payload);
		vfree(level_key_enable_tb.payload);
		vfree(crc_bypass_tb.payload);
		vfree(crc_p3_offset_tb.payload);
		vfree(crc_p3_tb.payload);
		vfree(flat_gamma_tb.payload);
		vfree(flat_gamma_value_tb.payload);
		vfree(level_key_disable_tb.payload);
		break;
	case DISPPARAM_SRGB:
		pr_info("flat mode on begin\n");
#if 0
		flat_mode_01.payload = vmalloc(2 * sizeof(unsigned char));
		flat_mode_01.vc = 0;
		flat_mode_01.dlen = 2;
		flat_mode_01.link_state = 1;
		flat_mode_01.dtype = 0xF0;
		flat_mode_01.payload[0] = 0x5A;
		flat_mode_01.payload[1] = 0x5A;
		do_lcm_vdo_lp_write_without_lock(&flat_mode_01,1);
		
		flat_mode_02.payload = vmalloc(2 * sizeof(unsigned char));
		flat_mode_02.vc = 0;
		flat_mode_02.dlen = 2;
		flat_mode_02.link_state = 1;
		flat_mode_02.dtype = 0xB0;
		flat_mode_02.payload[0] = 0x01;
		flat_mode_02.payload[1] = 0xB8;
		do_lcm_vdo_lp_write_without_lock(&flat_mode_02,1);
		
		flat_mode_03.payload = vmalloc(sizeof(unsigned char));
		flat_mode_03.vc = 0;
		flat_mode_03.dlen = 1;
		flat_mode_03.link_state = 1;
		flat_mode_03.dtype = 0xB8;
		flat_mode_03.payload[0] = 0x29;
		do_lcm_vdo_lp_write_without_lock(&flat_mode_03,1);

		flat_mode_04.payload = vmalloc(2 * sizeof(unsigned char));
		flat_mode_04.vc = 0;
		flat_mode_04.dlen = 2;
		flat_mode_04.link_state = 1;
		flat_mode_04.dtype = 0xF0;
		flat_mode_04.payload[0] = 0xA5;
		flat_mode_04.payload[1] = 0xA5;
		do_lcm_vdo_lp_write_without_lock(&flat_mode_04,1);
		
		vfree(flat_mode_01.payload);
		vfree(flat_mode_02.payload);
		vfree(flat_mode_03.payload);
		vfree(flat_mode_04.payload);
		pr_info("flat mode on end\n");
#endif
		pr_info("CRC sRGB\n");

            	/*level_key_enable_tb 0xf0 -> 0x5a 0x5a*/
		level_key_enable_tb.payload = vmalloc(2 * sizeof(unsigned char));
		level_key_enable_tb.vc = 0;
		level_key_enable_tb.dlen = 2;
		level_key_enable_tb.link_state = 1;
		level_key_enable_tb.dtype = 0xF0;
		level_key_enable_tb.payload[0] = 0x5A;
		level_key_enable_tb.payload[1] = 0x5A;
		do_lcm_vdo_lp_write_without_lock(&level_key_enable_tb,1);
            
		crc_enable_tb.payload = vmalloc(sizeof(unsigned char));
		crc_enable_tb.vc = 0;
		crc_enable_tb.dlen = 1;
		crc_enable_tb.link_state = 1;
		crc_enable_tb.dtype = 0x80;
		crc_enable_tb.payload[0] = 0x90;
		do_lcm_vdo_lp_write_without_lock(&crc_enable_tb,1);

		/*crc_bypass_tb {0xB1, 0x00};*/
		crc_bypass_tb.payload = vmalloc(sizeof(unsigned char));
		crc_bypass_tb.vc = 0;
		crc_bypass_tb.dlen = 1;
		crc_bypass_tb.link_state = 1;
		crc_bypass_tb.dtype = 0xB1;
		crc_bypass_tb.payload[0] = 0x00;
		do_lcm_vdo_lp_write_without_lock(&crc_bypass_tb,1);

		/*crc_srgb_offset_tb[] = {0xB0, 0x01, 0xB1};*/
		crc_srgb_offset_tb.payload = vmalloc(2 * sizeof(unsigned char));
		crc_srgb_offset_tb.vc = 0;
		crc_srgb_offset_tb.dlen = 1;
		crc_srgb_offset_tb.link_state = 1;
		crc_srgb_offset_tb.dtype = 0xB0;
		crc_srgb_offset_tb.payload[0] = 0x01;
		crc_srgb_offset_tb.payload[1] = 0xB1;
		do_lcm_vdo_lp_write_without_lock(&crc_srgb_offset_tb,1);

		/*crc_srgb_tb[] = {0xB1, 0xA3, 0x00, 0x04, 0x3C, 0xC1, 0x15, 0x08, 0x05, 0xAB,
					0x4C, 0xE0, 0xCF, 0xC2, 0x06, 0xBD, 0xE5, 0xD3, 0x1D, 0xFF, 0xED, 0xE0};*/
		crc_srgb_tb.payload = vmalloc(21 * sizeof(unsigned char));
		crc_srgb_tb.vc = 0;
		crc_srgb_tb.dlen = 21;
		crc_srgb_tb.link_state = 1;
		crc_srgb_tb.dtype = 0xB1;
		crc_srgb_tb.payload[0] = 0xAD;
		crc_srgb_tb.payload[1] = 0x0F;
		crc_srgb_tb.payload[2] = 0x04;
		crc_srgb_tb.payload[3] = 0x3D;
		crc_srgb_tb.payload[4] = 0xBD;
		crc_srgb_tb.payload[5] = 0x14;
		crc_srgb_tb.payload[6] = 0x05;
		crc_srgb_tb.payload[7] = 0x08;
		crc_srgb_tb.payload[8] = 0xA8;
		crc_srgb_tb.payload[9] = 0x4B;
		crc_srgb_tb.payload[10] = 0xDA;
		crc_srgb_tb.payload[11] = 0xD0;
		crc_srgb_tb.payload[12] = 0xCB;
		crc_srgb_tb.payload[13] = 0x1D;
		crc_srgb_tb.payload[14] = 0xCD;
		crc_srgb_tb.payload[15] = 0xE5;
		crc_srgb_tb.payload[16] = 0xD7;
		crc_srgb_tb.payload[17] = 0x1B;
		crc_srgb_tb.payload[18] = 0xFF;
		crc_srgb_tb.payload[19] = 0xF4;
		crc_srgb_tb.payload[20] = 0xE0;
		do_lcm_vdo_lp_write_without_lock(&crc_srgb_tb,1);

		/*flat_gamma_tb[] = {0xB0, 0x01, 0xB8};*/
		flat_gamma_tb.payload = vmalloc(2 * sizeof(unsigned char));
		flat_gamma_tb.vc = 0;
		flat_gamma_tb.dlen = 2;
		flat_gamma_tb.link_state = 1;
		flat_gamma_tb.dtype = 0xB0;
		flat_gamma_tb.payload[0] = 0x01;
		flat_gamma_tb.payload[1] = 0xB8;
		do_lcm_vdo_lp_write_without_lock(&flat_gamma_tb,1);

		/*flat_gamma_value_tb[] = {0xB8, 0x09};*/
		flat_gamma_value_tb.payload = vmalloc(sizeof(unsigned char));
		flat_gamma_value_tb.vc = 0;
		flat_gamma_value_tb.dlen = 1;
		flat_gamma_value_tb.link_state = 1;
		flat_gamma_value_tb.dtype = 0xB8;
		flat_gamma_value_tb.payload[0] = 0x09;
		do_lcm_vdo_lp_write_without_lock(&flat_gamma_value_tb,1);

		/*level_key_disable_tb[] = {0xF0, 0xA5, 0xA5};*/
		level_key_disable_tb.payload = vmalloc(2 * sizeof(unsigned char));
		level_key_disable_tb.vc = 0;
		level_key_disable_tb.dlen = 2;
		level_key_disable_tb.link_state = 1;
		level_key_disable_tb.dtype = 0xF0;
		level_key_disable_tb.payload[0] = 0xA5;
		level_key_disable_tb.payload[1] = 0xA5;
		do_lcm_vdo_lp_write_without_lock(&level_key_disable_tb,1);

		vfree(crc_srgb_tb.payload);
		vfree(level_key_enable_tb.payload);
		vfree(crc_bypass_tb.payload);
		vfree(crc_srgb_offset_tb.payload);
		vfree(crc_enable_tb.payload);
		vfree(flat_gamma_tb.payload);
		vfree(flat_gamma_value_tb.payload);
		vfree(level_key_disable_tb.payload);
		break;
	case DISPPARAM_DOZE_BRIGHTNESS_HBM:
		pr_info("DISPPARAM_DOZE_BRIGHTNESS_HBM\n");
		//START tp fb suspend
		printk("-----FTS----primary_display_suspend_early_aod1");
		fb_drm_notifier_call_chain(FB_DRM_EVENT_BLANK, &g_notify_data);

		printk("-----FTS----primary_display_suspend_aod1");
		fb_drm_notifier_call_chain(FB_DRM_EARLY_EVENT_BLANK, &g_notify_data);
		doze_brightness_set.payload = vmalloc(sizeof(unsigned char));
		doze_brightness_set.vc = 0;
		doze_brightness_set.dlen = 1;
		doze_brightness_set.link_state = 1;
		doze_brightness_set.dtype = 0x51;
		doze_brightness_set.payload[0] = 0xFF;
		do_lcm_vdo_lp_write_without_lock(&doze_brightness_set,1);
		pr_info("In %s the doze_brightness value:%x\n", __func__, DISPPARAM_DOZE_BRIGHTNESS_HBM);
		vfree(doze_brightness_set.payload);
		break;
	case DISPPARAM_DOZE_BRIGHTNESS_LBM:
		pr_info("DISPPARAM_DOZE_BRIGHTNESS_LBM\n");
		printk("-----FTS----primary_display_suspend_aod1");
		fb_drm_notifier_call_chain(FB_DRM_EVENT_BLANK, &g_notify_data);

		printk("-----FTS----primary_display_suspend_aod1");
		fb_drm_notifier_call_chain(FB_DRM_EARLY_EVENT_BLANK, &g_notify_data);
		doze_brightness_set.payload = vmalloc(sizeof(unsigned char));
		doze_brightness_set.vc = 0;
		doze_brightness_set.dlen = 1;
		doze_brightness_set.link_state = 1;
		doze_brightness_set.dtype = 0x51;
		doze_brightness_set.payload[0] = 0x2F;
		do_lcm_vdo_lp_write_without_lock(&doze_brightness_set,1);

		pr_info("In %s the doze_brightness value:%x\n", __func__, DISPPARAM_DOZE_BRIGHTNESS_LBM);
		vfree(doze_brightness_set.payload);
		break;
	case DISPPARAM_DOZE_OFF:
		pr_info("DISPPARAM_DOZE_OFF\n");
		pr_info("In %s the doze_brightness value:%x\n", __func__, DISPPARAM_DOZE_OFF);
		break;
	default:
		break;
	}

	return 0;
}

#endif
