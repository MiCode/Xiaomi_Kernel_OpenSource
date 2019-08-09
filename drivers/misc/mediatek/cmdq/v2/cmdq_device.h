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

#include "cmdq_def.h"
#include "cmdq_mdp_common.h"
#include <linux/device.h>
#include <linux/platform_device.h>

#if !defined(CONFIG_MTK_CLKMGR) && !defined(CMDQ_USE_CCF)
#define CMDQ_USE_CCF
#endif

#ifndef CMDQ_USE_CCF
#include <mach/mt_clkmgr.h>
#else
#include <linux/clk.h>
#endif /* !defined(CMDQ_USE_CCF) */

#define DECLARE_ENABLE_HW_CLOCK(HW_NAME)                                       \
	uint32_t cmdq_dev_enable_clock_##HW_NAME(bool enable)
DECLARE_ENABLE_HW_CLOCK(SMI_COMMON);
DECLARE_ENABLE_HW_CLOCK(SMI_LARB0);
#ifdef CMDQ_USE_LEGACY
DECLARE_ENABLE_HW_CLOCK(MUTEX_32K);
#endif
#undef DECLARE_ENABLE_HW_CLOCK

#ifndef CMDQ_USE_CCF
uint32_t cmdq_dev_enable_mtk_clock(bool enable,
	enum cg_clk_id  gateId, char *name);
bool cmdq_dev_mtk_clock_is_enable(enum cg_clk_id gateId);
/* For test case used */
void testcase_clkmgr_impl(enum cg_clk_id gateId, char *name,
			  const unsigned long testWriteReg,
			  const uint32_t testWriteValue,
			  const unsigned long testReadReg,
			  const bool verifyWriteResult);
#else
void cmdq_dev_get_module_clock_by_name(const char *name, const char *clkName,
				       struct clk **clk_module);
uint32_t cmdq_dev_enable_device_clock(bool enable, struct clk *clk_module,
				      const char *clkName);
#ifdef CONFIG_MTK_CMDQ_TAB
bool cmdq_dev_gce_clock_is_on(void);
#endif
bool cmdq_dev_device_clock_is_enable(struct clk *clk_module);
/* For test case used */
void testcase_clkmgr_impl(enum CMDQ_ENG_ENUM engine, char *name,
			  const unsigned long testWriteReg,
			  const uint32_t testWriteValue,
			  const unsigned long testReadReg,
			  const bool verifyWriteResult);
#endif /* !defined(CMDQ_USE_CCF) */

struct device *cmdq_dev_get(void);
/* interrupt index */
uint32_t cmdq_dev_get_irq_id(void);
uint32_t cmdq_dev_get_irq_secure_id(void);
/* GCE clock */
void cmdq_dev_enable_gce_clock(bool enable);
bool cmdq_dev_gce_clock_is_enable(void);
/* virtual address */
long cmdq_dev_get_module_base_VA_GCE(void);
long cmdq_dev_get_module_base_VA_MMSYS_CONFIG(void);
void cmdq_dev_set_module_base_VA_MMSYS_CONFIG(long value);
long cmdq_dev_alloc_module_base_VA_by_name(const char *name);
/* Other modules information */
void cmdq_dev_free_module_base_VA(const long VA);
long cmdq_dev_get_APXGPT2_count(void);
uint32_t cmdq_dev_get_mmsys_dummy_reg_offset(void);
/* physical address */
void cmdq_dev_get_module_PA(const char *name, int index, long *startPA,
			    long *endPA);
long cmdq_dev_get_module_base_PA_GCE(void);
/* GCE event */
void cmdq_dev_init_event_table(struct device_node *node);
void cmdq_dev_test_dts_correctness(void);
/* device initialization / deinitialization */
void cmdq_dev_init(struct platform_device *pDevice);
void cmdq_dev_deinit(void);
/* dma_set_mask result, to show in status */
int32_t cmdq_dev_get_dma_mask_result(void);

struct cmdq_dts_setting {
	uint32_t prefetch_thread_count;
	uint32_t prefetch_size[CMDQ_MAX_THREAD_COUNT];
};

/* callback when read resource from device tree */
typedef void (*CMDQ_DEV_INIT_RESOURCE_CB)(uint32_t engineFlag,
					  enum CMDQ_EVENT_ENUM resourceEvent);

void cmdq_dev_get_dts_setting(struct cmdq_dts_setting *dts_setting);
void cmdq_dev_init_resource(CMDQ_DEV_INIT_RESOURCE_CB init_cb);

#endif /* __CMDQ_DEVICE_H__ */
