/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_SPM_SLEEP_INTERNAL_H__
#define __MTK_SPM_SLEEP_INTERNAL_H__

/**************************************
 * only for internal debug
 **************************************/
#if IS_ENABLED(CONFIG_MTK_LDVT)
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

#define spm_is_wakesrc_invalid(wakesrc)	\
	(!!((u32)(wakesrc) & 0xc0003803))

extern struct regmap *pmic_regmap;

enum spm_suspend_step {
	SPM_SUSPEND_ENTER = 0x00000001,
	SPM_SUSPEND_ENTER_UART_SLEEP = 0x00000003,
	SPM_SUSPEND_ENTER_WFI = 0x000000ff,
	SPM_SUSPEND_LEAVE_WFI = 0x000001ff,
	SPM_SUSPEND_ENTER_UART_AWAKE = 0x000003ff,
	SPM_SUSPEND_LEAVE = 0x000007ff,
};

#define CPU_FOOTPRINT_SHIFT 24

bool spm_resource_req(unsigned int user, unsigned int req_mask);

extern void register_spm_resource_req_func(
	bool (*spm_resource_req_func)(unsigned int user, unsigned int req_mask));

extern int rtc_clock_enable(int enable);
#endif /* __MTK_SPM_SLEEP_INTERNAL_H__ */
