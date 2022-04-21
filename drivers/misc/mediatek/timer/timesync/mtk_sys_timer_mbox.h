/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#ifndef __MTK_SYS_TIMER_MBOX_H__
#define __MTK_SYS_TIMER_MBOX_H__

enum mbox_idx {
	MBOX_TICK_H = 0,
	MBOX_TICK_L,
	MBOX_TS_H,
	MBOX_TS_L,
	MBOX_DEBUG_TS_H,
	MBOX_DEBUG_TS_L,
};

#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
#if defined(SSPM_V2)
#include <sspm_mbox_pin.h>
#else
#include <sspm_ipi_mbox_layout.h>
#endif

#define SYS_TIMER_MBOX                      SHAREMBOX_NO_MCDI
#define SYS_TIMER_MBOX_OFFSET_BASE          SHAREMBOX_OFFSET_TIMESTAMP
#endif

#if defined(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT)
#include <mcupm_driver.h>

#define SYS_TIMER_MCUPM_MBOX                MCUPM_MBOX_NO_SUSPEND
#define SYS_TIMER_MCUPM_MBOX_OFFSET_BASE    MCUPM_MBOX_OFFSET_TIMESTAMP
#endif
#endif /* __MTK_SYS_TIMER_MBOX_H__ */

