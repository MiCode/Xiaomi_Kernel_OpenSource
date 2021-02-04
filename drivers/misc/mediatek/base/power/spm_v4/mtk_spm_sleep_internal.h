/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __MTK_SPM_SLEEP_INTERNAL_H__
#define __MTK_SPM_SLEEP_INTERNAL_H__

/**************************************
 * only for internal debug
 **************************************/
#ifdef CONFIG_MTK_LDVT
#define SPM_PWAKE_EN            0
#define SPM_PCMWDT_EN           0
#define SPM_BYPASS_SYSPWREQ     1
#define SPM_PMIC_EN             1
#define SPM_PMIC_DEBUG          1
#else
#define SPM_PWAKE_EN            1
#define SPM_PCMWDT_EN           1
#define SPM_BYPASS_SYSPWREQ     1
#define SPM_PMIC_EN             1
#define SPM_PMIC_DEBUG          0
#endif

/**************************************
 * SW code for suspend
 **************************************/
#define SPM_SYSCLK_SETTLE       99	/* 3ms */

#define WAIT_UART_ACK_TIMES     10	/* 10 * 10us */

#define spm_is_wakesrc_invalid(wakesrc)     (!!((u32)(wakesrc) & 0xc0003803))

enum spm_suspend_step {
	SPM_SUSPEND_ENTER = 0x00000001,
	SPM_SUSPEND_ENTER_UART_SLEEP = 0x00000003,
	SPM_SUSPEND_ENTER_WFI = 0x000000ff,
	SPM_SUSPEND_LEAVE_WFI = 0x000001ff,
	SPM_SUSPEND_ENTER_UART_AWAKE = 0x000003ff,
	SPM_SUSPEND_LEAVE = 0x000007ff,
};

#define CPU_FOOTPRINT_SHIFT 24

#endif /* __MTK_SPM_SLEEP_INTERNAL_H__ */
