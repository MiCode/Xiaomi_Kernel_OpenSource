/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_PLAT_API_H__
#define __MDLA_PLAT_API_H__

#include <linux/types.h>
#include <linux/of_device.h>

u32 mdla_plat_get_core_num(void);

/**
 * [0:7]   : minor version number
 * [8:15]  : major version number
 * [16:31] : project
 */
#define get_minor_num(v) ((v) & 0xFF)
#define get_major_num(v) (((v) >> 8) & 0xFF)
#define get_proj_code(v) (((v) >> 16) & 0xFFFF)
u32 mdla_plat_get_version(void);

/* sw configuration */
bool mdla_plat_pwr_drv_ready(void);
bool mdla_plat_iommu_enable(void);
bool mdla_plat_nn_pmu_support(void);
bool mdla_plat_sw_preemption_support(void);
bool mdla_plat_hw_preemption_support(void);
bool mdla_plat_micro_p_support(void);
int mdla_plat_get_prof_ver(void);

#if IS_ENABLED(CONFIG_OF)
const struct of_device_id *mdla_plat_get_device(void);
#else
#define mdla_plat_get_device() NULL
#endif

int mdla_plat_init(struct platform_device *pdev);
void mdla_plat_deinit(struct platform_device *pdev);

void mdla_plat_up_init(void);

#endif /* __MDLA_PLAT_API_H__ */
