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

#ifndef __CCCI_FSM_SYS_H__
#define __CCCI_FSM_SYS_H__

#define AED_STR_LEN		(2048)

struct mdee_info_collect {
	spinlock_t mdee_info_lock;
	char mdee_info[AED_STR_LEN];
};

void fsm_sys_mdee_info_notify(char *buf);
int fsm_sys_init(void);

#endif /* __CCCI_FSM_SYS_H__ */

