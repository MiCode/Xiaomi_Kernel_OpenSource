// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#include "cmdq_device.h"
#include "cmdq_virtual.h"
#include "cmdq_helper_ext.h"

#ifdef CMDQ_CONFIG_SMI
#include "smi_public.h"
#endif

/* device tree */
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/clk-provider.h>

struct CmdqDeviceStruct {
	struct device *pDev;
	struct clk *clk_gce;
	struct clk *clk_gce_timer;
	struct clk *clk_mmsys_mtcmos;
	long regBaseVA;		/* considering 64 bit kernel, use long */
	phys_addr_t regBasePA;
	u32 irqId;
	u32 irqSecId;
	s32 dma_mask_result;
};
static struct CmdqDeviceStruct gCmdqDev;
static u32 gThreadCount;
static u32 gMMSYSDummyRegOffset;

struct device *cmdq_dev_get(void)
{
	return gCmdqDev.pDev;
}

u32 cmdq_dev_get_irq_id(void)
{
	return gCmdqDev.irqId;
}

u32 cmdq_dev_get_irq_secure_id(void)
{
	return gCmdqDev.irqSecId;
}

long cmdq_dev_get_module_base_VA_GCE(void)
{
	return gCmdqDev.regBaseVA;
}

phys_addr_t cmdq_dev_get_module_base_PA_GCE(void)
{
	return gCmdqDev.regBasePA;
}

s32 cmdq_dev_get_dma_mask_result(void)
{
	return gCmdqDev.dma_mask_result;
}

u32 cmdq_dev_get_thread_count(void)
{
	return gThreadCount;
}

u32 cmdq_dev_get_mmsys_dummy_reg_offset(void)
{
	return gMMSYSDummyRegOffset;
}

void cmdq_dev_init_module_base_VA(void)
{
	cmdq_mdp_get_func()->initModuleBaseVA();
}

void cmdq_dev_deinit_module_base_VA(void)
{
	cmdq_mdp_get_func()->deinitModuleBaseVA();
}

unsigned long cmdq_dev_alloc_reference_VA_by_name(const char *ref_name)
{
	unsigned long VA = 0L;
	struct device_node *node = NULL;

	node = of_parse_phandle(gCmdqDev.pDev->of_node, ref_name, 0);
	if (node) {
		VA = (unsigned long)of_iomap(node, 0);
		of_node_put(node);
	}
	CMDQ_LOG("DEV: VA ref(%s):0x%lx\n", ref_name, VA);
	return VA;
}


void cmdq_dev_free_module_base_VA(const long VA)
{
	iounmap((void *)VA);
}

phys_addr_t cmdq_dev_get_gce_node_PA(struct device_node *node, int index)
{
	struct resource res;
	phys_addr_t regBasePA = 0L;

	do {
		if (of_address_to_resource(node, index, &res) < 0)
			break;

		regBasePA = (0L | res.start);
	} while (0);

	return regBasePA;
}

#define IMP_ENABLE_HW_CLOCK(FN_NAME, HW_NAME)	\
u32 cmdq_dev_enable_clock_##FN_NAME(bool enable)	\
{	\
	return cmdq_dev_enable_device_clock(enable,	\
		gCmdqModuleClock.clk_##HW_NAME, #HW_NAME "-clk");	\
}

void cmdq_dev_get_module_clock_by_dev(struct device *dev,
	const char *clkName, struct clk **clk_module)
{
	*clk_module = devm_clk_get(dev, clkName);
	if (IS_ERR(*clk_module)) {
		/* error status print */
		CMDQ_ERR("DEV: cannot get module clock:%s\n", clkName);
	} else {
		/* message print */
		CMDQ_MSG("DEV: get module clock:%s\n", clkName);
	}
}

void cmdq_dev_get_module_clock_by_name(const char *ref_name,
	const char *clkName, struct clk **clk_module)
{
	struct device_node *node = NULL;

	node = of_parse_phandle(gCmdqDev.pDev->of_node, ref_name, 0);

	*clk_module = of_clk_get_by_name(node, clkName);
	if (IS_ERR(*clk_module)) {
		/* error status print */
		CMDQ_ERR("%s (%s): clock:%s err:%d\n", __func__, ref_name,
			clkName, (int)PTR_ERR(*clk_module));
	} else {
		/* message print */
		CMDQ_MSG("%s (%s): clock:%s\n", __func__, ref_name, clkName);
	}
}

u32 cmdq_dev_enable_device_clock(bool enable,
	struct clk *clk_module, const char *clkName)
{
	int result = 0;

	if (IS_ERR(clk_module)) {
		CMDQ_LOG("[WARN]clock module does not support:%s\n", clkName);
		return PTR_ERR(clk_module);
	}

	if (enable) {
		result = clk_prepare_enable(clk_module);
		if (result)
			CMDQ_ERR("enable clock with module:%s result:%d\n",
				clkName, result);
		else
			CMDQ_MSG("enable clock with module:%s result:%d\n",
				clkName, result);
	} else {
		clk_disable_unprepare(clk_module);
		CMDQ_MSG("disable clock with module:%s\n", clkName);
	}

	return result;
}

bool cmdq_dev_device_clock_is_enable(struct clk *clk_module)
{
	return true;
}

/* Common Clock Framework */
void cmdq_dev_init_module_clk(void)
{
	cmdq_mdp_get_func()->initModuleCLK();
}

void cmdq_dev_enable_gce_clock(bool enable)
{
	cmdq_dev_enable_device_clock(enable, gCmdqDev.clk_gce, "gce-clk");
	if (!IS_ERR(gCmdqDev.clk_gce_timer))
		cmdq_dev_enable_device_clock(enable, gCmdqDev.clk_gce_timer,
			"gce-clk-timer");
}

bool cmdq_dev_gce_clock_is_enable(void)
{
	return cmdq_dev_device_clock_is_enable(gCmdqDev.clk_gce);
}

bool cmdq_dev_mmsys_clock_is_enable(void)
{
	if (IS_ERR(gCmdqDev.clk_mmsys_mtcmos)) {
		CMDQ_ERR("MMSYS_MTCMOS clk not support\n");
		return false;
	}

	return __clk_is_enabled(gCmdqDev.clk_mmsys_mtcmos);
}

phys_addr_t cmdq_dev_get_reference_PA(const char *ref_name, int index)
{
	int status;
	struct device_node *node = NULL;
	struct resource res;
	phys_addr_t start_pa = 0;

	do {
		node = of_parse_phandle(gCmdqDev.pDev->of_node, ref_name, 0);
		if (!node)
			break;

		status = of_address_to_resource(node, index, &res);
		if (status < 0)
			break;

		start_pa = res.start;
		CMDQ_LOG("DEV: PA ref(%s): start:%pa\n",
			ref_name, &start_pa);
	} while (0);

	if (node)
		of_node_put(node);
	return start_pa;
}

/* Get MDP base address to user space */
void cmdq_dev_init_MDP_PA(struct device_node *node)
{
	u32 *pMDPBaseAddress = cmdq_core_get_dts_data()->MDPBaseAddress;
	phys_addr_t module_pa_start = 0;

	module_pa_start = cmdq_dev_get_reference_PA("mm_mutex", 0);

	if (!module_pa_start)
		CMDQ_ERR("DEV: init mm_mutex PA fail!!\n");
	else
		pMDPBaseAddress[CMDQ_MDP_PA_BASE_MM_MUTEX] = module_pa_start;
	CMDQ_MSG("MM_MUTEX PA: start:0x%x\n",
		pMDPBaseAddress[CMDQ_MDP_PA_BASE_MM_MUTEX]);
}

void cmdq_dev_get_subsys_by_name(struct device_node *node,
	enum CMDQ_SUBSYS_ENUM subsys,
	const char *grp_name, const char *dts_name)
{
	int status;
	u32 gceSubsys[3] = { 0, 0, 0 };
	struct SubsysStruct *gceSubsysStruct = NULL;

	do {
		if (subsys < 0 || subsys >= (u32)CMDQ_SUBSYS_MAX_COUNT)
			break;

		gceSubsysStruct = cmdq_core_get_dts_data()->subsys;

		status = of_property_read_u32_array(node, dts_name,
			gceSubsys, ARRAY_SIZE(gceSubsys));
		if (status < 0) {
			gceSubsysStruct[subsys].subsysID = -1;
			break;
		}

		gceSubsysStruct[subsys].msb = gceSubsys[0] & gceSubsys[2];
		gceSubsysStruct[subsys].subsysID = gceSubsys[1];
		gceSubsysStruct[subsys].mask = gceSubsys[2];
		strncpy(gceSubsysStruct[subsys].grpName, grp_name,
			CMDQ_SUBSYS_GRPNAME_MAX-1);
	} while (0);
}

void cmdq_dev_test_subsys_correctness_impl(enum CMDQ_SUBSYS_ENUM subsys)
{
	struct SubsysStruct *gceSubsysStruct = NULL;

	if (subsys >= 0 && subsys < CMDQ_SUBSYS_MAX_COUNT) {
		gceSubsysStruct = cmdq_core_get_dts_data()->subsys;

		if (gceSubsysStruct[subsys].subsysID != -1) {
			/* print subsys information from device tree */
			CMDQ_LOG("(%s) msb:0x%08x, %d, 0x%08x\n",
				gceSubsysStruct[subsys].grpName,
				gceSubsysStruct[subsys].msb,
				gceSubsysStruct[subsys].subsysID,
				gceSubsysStruct[subsys].mask);
		}
	}
}

void cmdq_dev_init_subsys(struct device_node *node)
{
	u32 i;
	struct cmdq_subsys_dts_name *subsys = cmdq_subsys_get_dts();

	for (i = 0; i < cmdq_subsys_get_size(); i++)
		if (subsys[i].name)
			cmdq_dev_get_subsys_by_name(node, i, subsys[i].group,
				subsys[i].name);
}

void cmdq_dev_get_event_value_by_name(struct device_node *node,
	enum cmdq_event event, const char *dts_name)
{
	s32 status;
	s32 event_value = -1;

	do {
		if (event < 0 || event >= (u32)CMDQ_MAX_HW_EVENT_COUNT)
			break;

		status = of_property_read_u32(node, dts_name, &event_value);
		if (status < 0)
			break;

		cmdq_core_set_event_table(event, event_value);
	} while (0);
}

void cmdq_dev_test_event_correctness_impl(enum cmdq_event event,
	const char *dts_name, const char *event_name)
{
	s32 eventValue = cmdq_core_get_event_value(event);

	if (eventValue >= 0 && eventValue < (u32)CMDQ_SYNC_TOKEN_MAX) {
		/* print event name from device tree */
		if (event < (u32)CMDQ_MAX_HW_EVENT_COUNT)
			CMDQ_LOG("%s = %d\n", dts_name, eventValue);
		else
			CMDQ_LOG("%s = %d\n", event_name, eventValue);
	}
}

void cmdq_dev_init_event_table(struct device_node *node)
{
	struct cmdq_event_table *events = cmdq_event_get_table();
	u32 table_size = cmdq_event_get_table_size();
	u32 i = 0;

	for (i = 0; i < table_size; i++) {
		if (events[i].event == (u32)CMDQ_MAX_HW_EVENT_COUNT)
			break;
		cmdq_dev_get_event_value_by_name(node, events[i].event,
			events[i].dts_name);
	}
}

void cmdq_dev_test_dts_correctness(void)
{
	struct cmdq_event_table *events = cmdq_event_get_table();
	struct cmdq_subsys_dts_name *subsys = cmdq_subsys_get_dts();
	u32 i;

	for (i = 0; i < cmdq_event_get_table_size(); i++)
		cmdq_dev_test_event_correctness_impl(events[i].event,
			events[i].dts_name, events[i].event_name);
	for (i = 0; i < cmdq_subsys_get_size(); i++)
		if (subsys[i].name)
			cmdq_dev_test_subsys_correctness_impl(i);
}

void cmdq_dev_get_dts_setting(struct cmdq_dts_setting *dts_setting)
{
	s32 ret = -1;
	u32 sram_size_cpr_64 = 0;

	ret = of_property_read_u32(gCmdqDev.pDev->of_node, "sram_size_cpr_64",
		&sram_size_cpr_64);
	if (ret != 0 || !sram_size_cpr_64) {
		dts_setting->cpr_size = gThreadCount * CMDQ_THR_FREE_CPR_MAX;
		CMDQ_ERR("sram_size_cpr_64 not support, default:%u\n",
			dts_setting->cpr_size);
	} else {
		/* CPRs are 32bit register, device tree count in 64bit */
		dts_setting->cpr_size = sram_size_cpr_64 * 2;
		CMDQ_LOG("free CPR size:%u thread:%u\n",
			dts_setting->cpr_size, gThreadCount);
	}

	ret = of_property_read_u32(gCmdqDev.pDev->of_node,
		"max_prefetch_cnt", &dts_setting->prefetch_thread_count);
	if (ret == 0) {
		/* read prefetch array base on count */
		ret = of_property_read_u32_array(gCmdqDev.pDev->of_node,
			"prefetch_size", dts_setting->prefetch_size,
			dts_setting->prefetch_thread_count);
		if (ret != 0) {
			/* print log but do notify error hw setting */
			CMDQ_ERR("read prefetch size fail\n");
		}
	}

	ret = of_property_read_u32(gCmdqDev.pDev->of_node, "ctl_int0",
		&dts_setting->ctl_int0);
	if (ret != 0) {
		/* debug only feature */
		CMDQ_VERBOSE("ctl_int0 not support\n");
	}
}

void cmdq_dev_init_resource(CMDQ_DEV_INIT_RESOURCE_CB init_cb)
{
	int status, index;
	u32 count;

	status = of_property_read_u32(gCmdqDev.pDev->of_node,
		"sram_share_cnt", &count);
	if (status < 0)
		return;

	for (index = 0; index < count; index++) {
		u32 engine, event;

		status = of_property_read_u32_index(
			gCmdqDev.pDev->of_node, "sram_share_engine",
			index, &engine);
		if (status < 0)
			return;
		status = of_property_read_u32_index(
			gCmdqDev.pDev->of_node, "sram_share_event",
			index, &event);
		if (status < 0)
			return;
		if (init_cb != NULL)
			init_cb(engine, event);
	}
}

void cmdq_dev_init_device_tree(struct device_node *node)
{
	int status;
	u32 mmsys_dummy_reg_offset_value = 0;
	u32 thread_count = CMDQ_MAX_THREAD_COUNT;
	struct cmdq_dts_setting *dts_setting = cmdq_core_get_dts_setting();

	gThreadCount = thread_count;
	gMMSYSDummyRegOffset = 0;
	cmdq_core_init_dts_data();
	status = of_property_read_u32(node, "thread_count", &thread_count);
	if (status >= 0)
		gThreadCount = thread_count;
	/* init GCE subsys */
	cmdq_dev_init_subsys(node);
	/* init event table */
	cmdq_dev_init_event_table(node);
	/* init MDP PA address */
	cmdq_dev_init_MDP_PA(node);

	/* read dummy register offset from device tree,
	 * usually DUMMY_3 because DUMMY_0/1 is CLKMGR SW.
	 */
	status = of_property_read_u32(node, "mmsys_dummy_reg_offset",
		&mmsys_dummy_reg_offset_value);
	if (status < 0)
		mmsys_dummy_reg_offset_value = 0x89C;

	gMMSYSDummyRegOffset = mmsys_dummy_reg_offset_value;

	/* Initialize DTS Setting structure */
	memset(dts_setting, 0x0, sizeof(struct cmdq_dts_setting));
	/* Initialize setting for legacy chip */
	dts_setting->prefetch_thread_count = 3;
	dts_setting->prefetch_size = kcalloc(gThreadCount,
		sizeof(*dts_setting->prefetch_size), GFP_KERNEL);
	cmdq_dev_get_dts_setting(dts_setting);
}

void cmdq_dev_init(struct platform_device *pDevice)
{
	struct device_node *node = pDevice->dev.of_node;
	u32 dma_mask_bit = 0;
	s32 ret;

	/* init cmdq device dependent data */
	do {
		memset(&gCmdqDev, 0x0, sizeof(struct CmdqDeviceStruct));

		gCmdqDev.pDev = &pDevice->dev;
		gCmdqDev.regBaseVA = (unsigned long)of_iomap(node, 0);
		gCmdqDev.regBasePA = cmdq_dev_get_gce_node_PA(node, 0);
		gCmdqDev.irqId = irq_of_parse_and_map(node, 0);
		gCmdqDev.irqSecId = irq_of_parse_and_map(node, 1);
		gCmdqDev.clk_gce = devm_clk_get(&pDevice->dev, "GCE");
		gCmdqDev.clk_gce_timer = devm_clk_get(&pDevice->dev,
			"GCE_TIMER");
		gCmdqDev.clk_mmsys_mtcmos = devm_clk_get(&pDevice->dev,
			"MMSYS_MTCMOS");

		CMDQ_LOG(
			"[CMDQ] platform_dev: dev:%p PA:%pa VA:%lx irqId:%d irqSecId:%d\n",
			gCmdqDev.pDev, &gCmdqDev.regBasePA,
			gCmdqDev.regBaseVA, gCmdqDev.irqId,
			gCmdqDev.irqSecId);
	} while (0);

	ret = of_property_read_u32(gCmdqDev.pDev->of_node, "dma_mask_bit",
		&dma_mask_bit);
	/* if not assign from dts, give default 32bit for legacy chip */
	if (ret != 0 || !dma_mask_bit)
		dma_mask_bit = 32;
	gCmdqDev.dma_mask_result = dma_set_coherent_mask(
		&pDevice->dev, DMA_BIT_MASK(dma_mask_bit));
	CMDQ_LOG("set dma mask bit:%u result:%d\n",
		dma_mask_bit, gCmdqDev.dma_mask_result);

	/* map MMSYS VA */
	cmdq_mdp_map_mmsys_VA();
	/* init module VA */
	cmdq_dev_init_module_base_VA();
	/* init module clock */
	cmdq_dev_init_module_clk();
	/* init module PA for instruction count */
	cmdq_get_func()->initModulePAStat();
	/* init load HW information from device tree */
	cmdq_dev_init_device_tree(node);
}

void cmdq_dev_deinit(void)
{
	/* unmap MMSYS VA */
	cmdq_mdp_unmap_mmsys_VA();
	cmdq_dev_deinit_module_base_VA();

	/* deinit cmdq device dependent data */
	do {
		cmdq_dev_free_module_base_VA(
			cmdq_dev_get_module_base_VA_GCE());
		gCmdqDev.regBaseVA = 0;
	} while (0);
}
