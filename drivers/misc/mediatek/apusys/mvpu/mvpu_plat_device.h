/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MVPU_PLAT_API_H__
#define __MVPU_PLAT_API_H__

#include <linux/types.h>
#include <linux/of_device.h>
#include "apu.h"
#include "apu_config.h"

#define MAX_CORE_NUM 2
#define PREEMPT_L1_BUFFER (512 * 1024)
#define PREEMPT_ITCM_BUFFER (128 * 1024)

static u32 nr_core_ids;
static u32 mvpu_ver;
static u32 sw_preemption_level;

static uint32_t *itcm_kernel_addr_core_0[5];
static uint32_t *l1_kernel_addr_core_0[5];
static uint32_t *itcm_kernel_addr_core_1[5];
static uint32_t *l1_kernel_addr_core_1[5];

struct mvpu_plat_drv {
	unsigned int sw_preemption_level;
};

int mvpu_plat_info_init(struct platform_device *pdev);
int mvpu_plat_init(struct platform_device *pdev);
const struct of_device_id *mvpu_plat_get_device(void);
int mvpu_config_init(struct mtk_apu *apu);
int mvpu_config_remove(struct mtk_apu *apu);

#endif /* __MDLA_PLAT_API_H__ */
