/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __CCCI_FSM_SYS_H__
#define __CCCI_FSM_SYS_H__

#define AED_STR_LEN		(2048)

struct mdee_info_collect {
	spinlock_t mdee_info_lock;
	char mdee_info[AED_STR_LEN];
};

void fsm_sys_mdee_info_notify(const char *buf);
int fsm_sys_init(void);
extern void md_cd_lock_modem_clock_src(int locked);
#endif /* __CCCI_FSM_SYS_H__ */
