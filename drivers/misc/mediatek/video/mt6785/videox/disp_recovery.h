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

#ifndef _DISP_RECOVERY_H_
#define _DISP_RECOVERY_H_

#define GPIO_EINT_MODE	0
#define GPIO_DSI_MODE	1

/* defined in mtkfb.c should move to mtkfb.h*/
extern unsigned int islcmconnected;

void primary_display_check_recovery_init(void);
void primary_display_esd_check_enable(int enable);
unsigned int need_wait_esd_eof(void);

void external_display_check_recovery_init(void);
void external_display_esd_check_enable(int enable);

void set_esd_check_mode(unsigned int mode);
int do_lcm_vdo_lp_read(struct dsi_cmd_desc *cmd_tab,
			unsigned int count);
int do_lcm_vdo_lp_write(struct dsi_cmd_desc *write_table,
			unsigned int count);

int primary_display_ovl_recovery(void);
void primary_display_set_recovery_module(enum DISP_MODULE_ENUM module);

#endif /* _DISP_RECOVERY_H_ */
