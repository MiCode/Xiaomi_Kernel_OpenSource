/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_PLAT_API_H__
#define __MDLA_PLAT_API_H__

#include <linux/types.h>
#include <linux/of_device.h>

u32 mdla_plat_get_core_num(void);

/* sw configuration */
bool mdla_plat_pwr_drv_ready(void);
bool mdla_plat_iommu_enable(void);
bool mdla_plat_nn_pmu_support(void);
bool mdla_plat_sw_preemption_support(void);
bool mdla_plat_micro_p_support(void);
int mdla_plat_get_prof_ver(void);

#ifdef CONFIG_OF
const struct of_device_id *mdla_plat_get_device(void);
#else
#define mdla_plat_get_device() NULL
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

int mdla_plat_init(struct platform_device *pdev);
void mdla_plat_deinit(struct platform_device *pdev);

#endif /* __MDLA_PLAT_API_H__ */
