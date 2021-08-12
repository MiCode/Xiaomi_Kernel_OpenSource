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
	V2_DBG_DRV,
	V2_DBG_CMD,
	V2_DBG_PMU,
	V2_DBG_PERF,
	V2_DBG_TIMEOUT,
	V2_DBG_PWR,
	V2_DBG_MEM,
	V2_DBG_IPI,

	NR_V2_DBG_LOG_MASK
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

int mdla_plat_load_data(struct device *dev, unsigned int *cfg0, unsigned int *cfg1);
void mdla_plat_unload_data(struct device *dev);

#endif /* __MDLA_RV_H__ */
