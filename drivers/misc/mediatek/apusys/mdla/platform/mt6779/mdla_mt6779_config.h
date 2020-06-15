/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_MT6779_CONFIG_H__
#define __MDLA_MT6779_CONFIG_H__

#include <linux/types.h>

#include <common/mdla_power_ctrl.h>

#include <utilities/mdla_debug.h>
#include <utilities/mdla_util.h>


#define MDLA_TIMEOUT_DEFAULT       (6000) /* ms */
#define MDLA_POWEROFF_TIME_DEFAULT (2000) /* ms */
#define MDLA_POLLING_CMD_DONE_TIME (5)    /* ms */

#define MDLA_DBG_KLOG_DEFAULT      (MDLA_DBG_CMD | MDLA_DBG_TIMEOUT)

#define MDLA_GET_PMU_CNT_PERIOD_DEFAULT (1000) /* us */


/* Bypass power related functions */
//#define __APUSYS_MDLA_PWR_EARLY_PORTING__

/* Bypass memory related functions */
//#define __APUSYS_MDLA_MEM_EARLY_PORTING__

/* Bypass middleware related functions */
//#define __APUSYS_MDLA_MID_EARLY_PORTING__

/* Enable PMU function */
#define __APUSYS_MDLA_PMU_SUPPORT__

/* MicroP MDLA driver support */
//#define __APUSYS_MDLA_MICRO_P_SUPPORT__

#ifdef __APUSYS_MDLA_MEM_EARLY_PORTING__
#define mdla_reset_axi_ctrl(id, axi_ctrl, axi1_ctrl, mask)
#else
#define mdla_reset_axi_ctrl(id, axi_ctrl, axi1_ctrl, mask)	\
do {								\
	mdla_util_io_ops_get()->cfg.set_b(id, axi_ctrl, mask);	\
	mdla_util_io_ops_get()->cfg.set_b(id, axi1_ctrl, mask);	\
} while (0)
#endif


#ifdef __APUSYS_MDLA_PWR_EARLY_PORTING__
#define mdla_pwr_register(pdev, on, off_start, reset)\
			mdla_pwr_device_register(pdev, NULL, NULL, reset)
#define mdla_pwr_unregister(pdev) 0
#else
#define mdla_pwr_register(pdev, on, off_start, reset)\
			mdla_pwr_device_register(pdev, on, off_start, reset)
#define mdla_pwr_unregister(pdev)\
			mdla_pwr_device_unregister(pdev)
#endif

#ifdef CONFIG_FPGA_EARLY_PORTING
#include <mdla_cmd_proc.h>
#define mdla_fpga_reset()						\
do {									\
	int i;								\
	for_each_mdla_core(i)						\
		mdla_cmd_plat_cb()->hw_reset(i,				\
				mdla_dbg_get_reason_str(REASON_DRVINIT));\
} while (0)
#else
#define mdla_fpga_reset()
#endif

#endif /* __MDLA_MT6779_CONFIG_H__ */
