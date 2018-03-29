/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __CMDQ_DEVICE_H__
#define __CMDQ_DEVICE_H__

#include <linux/platform_device.h>
#include <linux/device.h>

struct device *cmdq_dev_get(void);
extern const uint32_t cmdq_dev_get_irq_id(void);
extern const uint32_t cmdq_dev_get_irq_secure_id(void);
extern const long cmdq_dev_get_module_base_VA_GCE(void);
extern const long cmdq_dev_get_module_base_PA_GCE(void);

extern const long cmdq_dev_get_module_base_VA_MMSYS_CONFIG(void);

const long cmdq_dev_alloc_module_base_VA_by_name(const char *name);
void cmdq_dev_free_module_base_VA(const long VA);

void cmdq_dev_init(struct platform_device *pDevice);
void cmdq_dev_deinit(void);

#define MMSYS_CONFIG_BASE_VA	cmdq_dev_get_module_base_VA_MMSYS_CONFIG()
#define GCE_BASE_VA				cmdq_dev_get_module_base_VA_GCE()



#define GCE_BASE_PA				cmdq_dev_get_module_base_PA_GCE()
/* #define GCE_BASE_PA                           0x10212000 */
#define DISP_PWM0_PA			0x1401E000
#define MMSYS_CONFIG_BASE_PA	0x14000000
#define MDP_RDMA0_BASE_PA		0x14001000
#define MDP_RDMA1_BASE_PA		0x14002000
#define MDP_RSZ0_BASE_PA		0x14003000
#define MDP_RSZ1_BASE_PA		0x14004000
#define MDP_RSZ2_BASE_PA		0x14005000
#define MDP_WDMA_BASE_PA		0x14006000
#define MDP_WROT0_BASE_PA		0x14007000
#define MDP_WROT1_BASE_PA		0x14008000
#define MDP_TDSHP0_BASE_PA		0x14009000
#define MDP_TDSHP1_BASE_PA		0x1400A000
#define MM_MUTEX_BASE_PA		0x14020000
#define VENC_BASE_PA			0x18002000
#define MSDC0_BASE_PA			0x11230000
#define AUDIO_BASE_PA			0x11220000



#define CMDQ_TEST_DISP_PWM0_DUMMY_PA    (DISP_PWM0_PA + 0x0030)

#endif				/* __CMDQ_DEVICE_H__ */
