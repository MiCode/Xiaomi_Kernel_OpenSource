/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#ifndef __APU_HW_SEMA_H__
#define __APU_HW_SEMA_H__

enum subsys_id {
	SYS_APMCU = 0,
	SYS_APU,
	SYS_SCP_LP,
	SYS_SCP_NP,
	SYS_MAX,
};

uint32_t apu_boot_host(void);

#endif
