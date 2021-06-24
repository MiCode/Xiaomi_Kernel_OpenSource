/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_RV_H__
#define __MDLA_RV_H__

/*************************
 *    HW Version : v2    *
 *************************/
/* Same as uP v2 enum MDLA_DEBUG_MASK */
enum V2_DEBUG_MASK {
	V2_DBG_DRV         = 0x01,
	V2_DBG_CMD         = 0x04,
	V2_DBG_PMU         = 0x08,
	V2_DBG_PERF        = 0x10,
	V2_DBG_TIMEOUT     = 0x40,
	V2_DBG_DVFS        = 0x80,
	V2_DBG_TIMEOUT_ALL = 0x100,
	V2_DBG_ERROR       = 0x200,
};

/*************************
 *    HW Version : v3    *
 *************************/
/* Same as uP v3 enum MDLA_DBG_LOG_MASK */
enum V3_DBG_LOG_MASK {
	V3_DBG_DRV,
	V3_DBG_CMD,
	V3_DBG_PMU,
	V3_DBG_PERF,
	V3_DBG_TIMEOUT,
	V3_DBG_PWR,
	V3_DBG_MEM,
	V3_DBG_IPI,
	V3_DBG_QUEUE,
	V3_DBG_LOCK,
	V3_DBG_TMR,
	V3_DBG_FW,

	NR_V3_DBG_LOG_MASK
};

#endif /* __MDLA_RV_H__ */
