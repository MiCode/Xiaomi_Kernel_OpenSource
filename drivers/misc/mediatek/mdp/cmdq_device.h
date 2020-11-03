/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#ifndef __CMDQ_DEVICE_H__
#define __CMDQ_DEVICE_H__

#include <linux/platform_device.h>
#include <linux/device.h>
#include "mdp_def.h"
#include "mdp_common.h"
#include <linux/clk.h>

#define DECLARE_ENABLE_HW_CLOCK(HW_NAME) \
	u32 cmdq_dev_enable_clock_##HW_NAME(bool enable)
DECLARE_ENABLE_HW_CLOCK(SMI_COMMON);
DECLARE_ENABLE_HW_CLOCK(SMI_LARB0);
#ifdef CMDQ_USE_LEGACY
DECLARE_ENABLE_HW_CLOCK(MUTEX_32K);
#endif
#undef DECLARE_ENABLE_HW_CLOCK

void cmdq_dev_get_module_clock_by_name(const char *ref_name,
	const char *clkName, struct clk **clk_module);
u32 cmdq_dev_enable_device_clock(bool enable, struct clk *clk_module,
	const char *clkName);
bool cmdq_dev_device_clock_is_enable(struct clk *clk_module);
/* For test case used */
void testcase_clkmgr_impl(u32 engine,
	char *name, const unsigned long testWriteReg,
	const u32 testWriteValue,
	const unsigned long testReadReg, const bool verifyWriteResult);

struct device *cmdq_dev_get(void);
/* interrupt index */
u32 cmdq_dev_get_irq_id(void);
u32 cmdq_dev_get_irq_secure_id(void);
/* GCE clock */
void cmdq_dev_enable_gce_clock(bool enable);
bool cmdq_dev_gce_clock_is_enable(void);
bool cmdq_dev_mmsys_clock_is_enable(void);
/* virtual address */
long cmdq_dev_get_module_base_VA_GCE(void);
unsigned long cmdq_dev_alloc_reference_VA_by_name(const char *ref_name);
unsigned long cmdq_dev_alloc_reference_by_name(const char *ref_name,
	uint32_t *pa);
/* Other modules information */
void cmdq_dev_free_module_base_VA(const long VA);
u32 cmdq_dev_get_mmsys_dummy_reg_offset(void);
/* physical address */
phys_addr_t cmdq_dev_get_reference_PA(const char *ref_name, int index);
phys_addr_t cmdq_dev_get_module_base_PA_GCE(void);
#if IS_ENABLED(CONFIG_MACH_MT6885)
unsigned long cmdq_dev_get_va2(void);
phys_addr_t cmdq_dev_get_pa2(void);
#endif
/* GCE event */
void cmdq_dev_init_event_table(struct device_node *node);
void cmdq_dev_test_dts_correctness(void);
/* device initialization / deinitialization */
void cmdq_dev_init(struct platform_device *pDevice);
void cmdq_dev_deinit(void);
/* dma_set_mask result, to show in status */
s32 cmdq_dev_get_dma_mask_result(void);
u32 cmdq_dev_get_thread_count(void);

/* callback when read resource from device tree */
typedef void(*CMDQ_DEV_INIT_RESOURCE_CB) (u32 engineFlag,
	enum cmdq_event resourceEvent);

void cmdq_dev_init_resource(CMDQ_DEV_INIT_RESOURCE_CB init_cb);





#endif				/* __CMDQ_DEVICE_H__ */
