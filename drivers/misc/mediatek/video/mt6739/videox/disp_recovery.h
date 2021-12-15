/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _DISP_RECOVERY_H_
#define _DISP_RECOVERY_H_

#define GPIO_EINT_MODE 0
#define GPIO_DSI_MODE 1

/* defined in mtkfb.c should move to mtkfb.h*/
extern unsigned int islcmconnected;

void primary_display_check_recovery_init(void);
void primary_display_esd_check_enable(int enable);
unsigned int need_wait_esd_eof(void);
void set_esd_check_mode(unsigned int mode);

#endif
