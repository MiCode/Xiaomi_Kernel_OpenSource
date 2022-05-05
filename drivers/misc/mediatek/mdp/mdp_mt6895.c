// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/of_address.h>
#include <linux/of_platform.h>

#include "cmdq_reg.h"
#include "mdp_common.h"
#ifdef CMDQ_MET_READY
#include <linux/met_drv.h>
#endif
#include <linux/slab.h>
#define MDP_IOMMU_DEBUG 1
#ifdef MDP_IOMMU_DEBUG
#include "iommu_debug.h"
#endif

#ifdef CMDQ_SECURE_PATH_SUPPORT
#include <cmdq-sec-iwc-common.h>
#endif

#undef MTK_M4U_ID
#include <dt-bindings/memory/mt6895-larb-port.h>
//#include <linux/interconnect-provider.h>
#include "mtk-interconnect.h"
#include <soc/mediatek/smi.h>

#include "mdp_engine_mt6895.h"
#include "mdp_base_mt6895.h"
#include <cmdq-util.h>

/* iommu larbs */
struct device *larb2;

/* support RDMA prebuilt access */
int gCmdqRdmaPrebuiltSupport;
/* support register MSB */
int gMdpRegMSBSupport = 1;
/* support vcp pq readback */
static int gVcpPQReadbackSupport;

/* use to generate [CMDQ_ENGINE_ENUM_id and name] mapping for status print */
#define CMDQ_FOREACH_MODULE_PRINT(ACTION)\
{		\
ACTION(CMDQ_ENG_MDP_RDMA0,  MDP_RDMA0)	\
ACTION(CMDQ_ENG_MDP_RDMA1,  MDP_RDMA1)	\
ACTION(CMDQ_ENG_MDP_RSZ0,   MDP_RSZ0)	\
ACTION(CMDQ_ENG_MDP_RSZ1,   MDP_RSZ1)	\
ACTION(CMDQ_ENG_MDP_TDSHP0, MDP_TDSHP0)	\
ACTION(CMDQ_ENG_MDP_TDSHP1, MDP_TDSHP1)	\
ACTION(CMDQ_ENG_MDP_COLOR0, MDP_COLOR0) \
ACTION(CMDQ_ENG_MDP_COLOR1, MDP_COLOR1) \
ACTION(CMDQ_ENG_MDP_WROT0,  MDP_WROT0)	\
ACTION(CMDQ_ENG_MDP_WROT1,  MDP_WROT1)	\
}

/* mdp */
static struct icc_path *path_mdp_rdma0[MDP_TOTAL_THREAD];
static struct icc_path *path_mdp_rdma1[MDP_TOTAL_THREAD];
static struct icc_path *path_mdp_wrot0[MDP_TOTAL_THREAD];
static struct icc_path *path_mdp_wrot1[MDP_TOTAL_THREAD];

#include "cmdq_device.h"
struct CmdqMdpModuleBaseVA {
	long MDP_RDMA0;
	long MDP_RDMA1;
	long MDP_FG0;
	long MDP_FG1;
	long MDP_HDR0;
	long MDP_HDR1;
	long MDP_COLOR0;
	long MDP_COLOR1;
	long MDP_AAL0;
	long MDP_AAL1;
	long MDP_RSZ0;
	long MDP_RSZ1;
	long MDP_TDSHP0;
	long MDP_TDSHP1;
	long MDP_WROT0;
	long MDP_WROT1;
	long MM_MUTEX;
};
static struct CmdqMdpModuleBaseVA gCmdqMdpModuleBaseVA;

/* FIXME: mt6873 only? */
struct mdp_base_pa {
	u32 aal0;
	u32 aal1;
	u32 hdr0;
	u32 hdr1;
};
static struct mdp_base_pa mdp_module_pa;

struct CmdqMdpModuleClock {
	struct clk *clk_APB;
	struct clk *clk_APMCU_GALS;
	struct clk *clk_MDP_MUTEX0;
	struct clk *clk_MDP_RDMA0;
	struct clk *clk_MDP_RDMA1;
	struct clk *clk_MDP_FG0;
	struct clk *clk_MDP_FG1;
	struct clk *clk_MDP_HDR0;
	struct clk *clk_MDP_HDR1;
	struct clk *clk_MDP_COLOR0;
	struct clk *clk_MDP_COLOR1;
	struct clk *clk_MDP_AAL0;
	struct clk *clk_MDP_AAL1;
	struct clk *clk_MDP_RSZ0;
	struct clk *clk_MDP_RSZ1;
	struct clk *clk_MDP_TDSHP0;
	struct clk *clk_MDP_TDSHP1;
	struct clk *clk_MDP_WROT0;
	struct clk *clk_MDP_WROT1;
};
static struct CmdqMdpModuleClock gCmdqMdpModuleClock;
#define IMP_ENABLE_MDP_HW_CLOCK(FN_NAME, HW_NAME)	\
uint32_t cmdq_mdp_enable_clock_##FN_NAME(bool enable)	\
{		\
	return cmdq_dev_enable_device_clock(enable,	\
		gCmdqMdpModuleClock.clk_##HW_NAME, #HW_NAME "-clk");	\
}
#define IMP_MDP_HW_CLOCK_IS_ENABLE(FN_NAME, HW_NAME)	\
bool cmdq_mdp_clock_is_enable_##FN_NAME(void)	\
{		\
	return cmdq_dev_device_clock_is_enable(		\
		gCmdqMdpModuleClock.clk_##HW_NAME);	\
}

IMP_ENABLE_MDP_HW_CLOCK(APB, APB);
IMP_ENABLE_MDP_HW_CLOCK(APMCU_GALS, APMCU_GALS);
IMP_ENABLE_MDP_HW_CLOCK(MDP_MUTEX0, MDP_MUTEX0);
IMP_ENABLE_MDP_HW_CLOCK(MDP_RDMA0, MDP_RDMA0);
IMP_ENABLE_MDP_HW_CLOCK(MDP_RDMA1, MDP_RDMA1);
IMP_ENABLE_MDP_HW_CLOCK(MDP_RSZ0, MDP_RSZ0);
IMP_ENABLE_MDP_HW_CLOCK(MDP_RSZ1, MDP_RSZ1);
IMP_ENABLE_MDP_HW_CLOCK(MDP_FG0, MDP_FG0);
IMP_ENABLE_MDP_HW_CLOCK(MDP_FG1, MDP_FG1);
IMP_ENABLE_MDP_HW_CLOCK(MDP_HDR0, MDP_HDR0);
IMP_ENABLE_MDP_HW_CLOCK(MDP_HDR1, MDP_HDR1);
IMP_ENABLE_MDP_HW_CLOCK(MDP_COLOR0, MDP_COLOR0);
IMP_ENABLE_MDP_HW_CLOCK(MDP_COLOR1, MDP_COLOR1);
IMP_ENABLE_MDP_HW_CLOCK(MDP_WROT0, MDP_WROT0);
IMP_ENABLE_MDP_HW_CLOCK(MDP_WROT1, MDP_WROT1);
IMP_ENABLE_MDP_HW_CLOCK(MDP_TDSHP0, MDP_TDSHP0);
IMP_ENABLE_MDP_HW_CLOCK(MDP_TDSHP1, MDP_TDSHP1);
IMP_ENABLE_MDP_HW_CLOCK(MDP_AAL0, MDP_AAL0);
IMP_ENABLE_MDP_HW_CLOCK(MDP_AAL1, MDP_AAL1);
IMP_MDP_HW_CLOCK_IS_ENABLE(APB, APB);
IMP_MDP_HW_CLOCK_IS_ENABLE(APMCU_GALS, APMCU_GALS);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_MUTEX0, MDP_MUTEX0);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_RDMA0, MDP_RDMA0);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_RDMA1, MDP_RDMA1);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_RSZ0, MDP_RSZ0);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_RSZ1, MDP_RSZ1);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_HDR0, MDP_HDR0);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_HDR1, MDP_HDR1);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_FG0, MDP_FG0);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_FG1, MDP_FG1);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_COLOR0, MDP_COLOR0);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_COLOR1, MDP_COLOR1);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_WROT0, MDP_WROT0);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_WROT1, MDP_WROT1);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_TDSHP0, MDP_TDSHP0);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_TDSHP1, MDP_TDSHP1);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_AAL0, MDP_AAL0);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_AAL1, MDP_AAL1);
#undef IMP_ENABLE_MDP_HW_CLOCK
#undef IMP_MDP_HW_CLOCK_IS_ENABLE

static const uint64_t gCmdqEngineGroupBits[CMDQ_MAX_GROUP_COUNT] = {
	CMDQ_ENG_MDP_GROUP_BITS
};

long cmdq_mdp_get_module_base_VA_MDP_RDMA0(void)
{
	return gCmdqMdpModuleBaseVA.MDP_RDMA0;
}

long cmdq_mdp_get_module_base_VA_MDP_RDMA1(void)
{
	return gCmdqMdpModuleBaseVA.MDP_RDMA1;
}

long cmdq_mdp_get_module_base_VA_MDP_RSZ0(void)
{
	return gCmdqMdpModuleBaseVA.MDP_RSZ0;
}

long cmdq_mdp_get_module_base_VA_MDP_RSZ1(void)
{
	return gCmdqMdpModuleBaseVA.MDP_RSZ1;
}

long cmdq_mdp_get_module_base_VA_MDP_TDSHP0(void)
{
	return gCmdqMdpModuleBaseVA.MDP_TDSHP0;
}

long cmdq_mdp_get_module_base_VA_MDP_TDSHP1(void)
{
	return gCmdqMdpModuleBaseVA.MDP_TDSHP1;
}

long cmdq_mdp_get_module_base_VA_MDP_COLOR0(void)
{
	return gCmdqMdpModuleBaseVA.MDP_COLOR0;
}

long cmdq_mdp_get_module_base_VA_MDP_COLOR1(void)
{
	return gCmdqMdpModuleBaseVA.MDP_COLOR1;
}

long cmdq_mdp_get_module_base_VA_MDP_WROT0(void)
{
	return gCmdqMdpModuleBaseVA.MDP_WROT0;
}

long cmdq_mdp_get_module_base_VA_MDP_WROT1(void)
{
	return gCmdqMdpModuleBaseVA.MDP_WROT1;
}

long cmdq_mdp_get_module_base_VA_MDP_AAL0(void)
{
	return gCmdqMdpModuleBaseVA.MDP_AAL0;
}

long cmdq_mdp_get_module_base_VA_MDP_AAL1(void)
{
	return gCmdqMdpModuleBaseVA.MDP_AAL1;
}

long cmdq_mdp_get_module_base_VA_MDP_HDR0(void)
{
	return gCmdqMdpModuleBaseVA.MDP_HDR0;
}

long cmdq_mdp_get_module_base_VA_MDP_HDR1(void)
{
	return gCmdqMdpModuleBaseVA.MDP_HDR1;
}

long cmdq_mdp_get_module_base_VA_MDP_FG0(void)
{
	return gCmdqMdpModuleBaseVA.MDP_FG0;
}

long cmdq_mdp_get_module_base_VA_MDP_FG1(void)
{
	return gCmdqMdpModuleBaseVA.MDP_FG1;
}

long cmdq_mdp_get_module_base_VA_MM_MUTEX(void)
{
	return gCmdqMdpModuleBaseVA.MM_MUTEX;
}

#define MMSYS_CONFIG_BASE	cmdq_mdp_get_module_base_VA_MMSYS_CONFIG()
#define MDP_RDMA0_BASE		cmdq_mdp_get_module_base_VA_MDP_RDMA0()
#define MDP_RDMA1_BASE		cmdq_mdp_get_module_base_VA_MDP_RDMA1()
#define MDP_AAL0_BASE		cmdq_mdp_get_module_base_VA_MDP_AAL0()
#define MDP_AAL1_BASE		cmdq_mdp_get_module_base_VA_MDP_AAL1()
#define MDP_HDR0_BASE		cmdq_mdp_get_module_base_VA_MDP_HDR0()
#define MDP_HDR1_BASE		cmdq_mdp_get_module_base_VA_MDP_HDR1()
#define MDP_RSZ0_BASE		cmdq_mdp_get_module_base_VA_MDP_RSZ0()
#define MDP_RSZ1_BASE		cmdq_mdp_get_module_base_VA_MDP_RSZ1()
#define MDP_TDSHP0_BASE		cmdq_mdp_get_module_base_VA_MDP_TDSHP0()
#define MDP_TDSHP1_BASE		cmdq_mdp_get_module_base_VA_MDP_TDSHP1()
#define MDP_COLOR0_BASE		cmdq_mdp_get_module_base_VA_MDP_COLOR0()
#define MDP_COLOR1_BASE		cmdq_mdp_get_module_base_VA_MDP_COLOR1()
#define MDP_WROT0_BASE		cmdq_mdp_get_module_base_VA_MDP_WROT0()
#define MDP_WROT1_BASE		cmdq_mdp_get_module_base_VA_MDP_WROT1()
#define MM_MUTEX_BASE		cmdq_mdp_get_module_base_VA_MM_MUTEX()
#define MDP_FG0_BASE		cmdq_mdp_get_module_base_VA_MDP_FG0()
#define MDP_FG1_BASE		cmdq_mdp_get_module_base_VA_MDP_FG1()

struct RegDef {
	int offset;
	const char *name;
};

void cmdq_mdp_dump_mmsys_config(const struct cmdqRecStruct *handle)
{
	int i = 0;
	uint32_t value = 0;

	static const struct RegDef configRegisters[] = {
		{0x000, "MDPSYS_INTEN"},
		{0x004, "MDPSYS_INTSTA"},
		{0x0F0, "MMSYS_MISC"},
		{0x0F4, "MDPSYS_MODULE_DBG"},
		{0x100,	"MDPSYS_CG_CON0"},
		{0x104,	"MDPSYS_CG_SET0"},
		{0x108,	"MDPSYS_CG_CLR0"},
		{0x110,	"MDPSYS_CG_CON1"},
		{0x114,	"MDPSYS_CG_SET1"},
		{0x118,	"MDPSYS_CG_CLR1"},
		{0x120,	"MDPSYS_CG_CON2"},
		{0x124,	"MDPSYS_CG_SET2"},
		{0x128,	"MDPSYS_CG_CLR2"},
		{0x130,	"MDPSYS_CG_CON3"},
		{0x134,	"MDPSYS_CG_SET3"},
		{0x138,	"MDPSYS_CG_CLR3"},
		{0x140,	"MDPSYS_CG_CON4"},
		{0x144,	"MDPSYS_CG_SET4"},
		{0x148,	"MDPSYS_CG_CLR4"},
		{0x1f4,	"MDPSYS_PROC_TRACK_EMI_BUSY_CON"},
		{0x220, "MDP_DL_IN_RELAY0_SIZE"},
		{0x224, "MDP_DL_IN_RELAY1_SIZE"},
		{0x228, "MDP_DL_OUT_RELAY0_SIZE"},
		{0x22c, "MDP_DL_OUT_RELAY1_SIZE"},
		{0x230, "MDP_DLO_ASYNC0_STATUS0"},
		{0x234, "MDP_DLO_ASYNC0_STATUS1"},
		{0x238, "MDP_DLO_ASYNC1_STATUS0"},
		{0x23C, "MDP_DLO_ASYNC1_STATUS1"},
		{0x240, "MDP_DLI_ASYNC0_STATUS0"},
		{0x244, "MDP_DLI_ASYNC0_STATUS1"},
		{0x248, "MDP_DLI_ASYNC1_STATUS0"},
		{0x24C, "MDP_DLI_ASYNC1_STATUS1"},
		{0x250, "MDPSYS_APMCU_GALS_ctrl"},
		{0x300, "MDPSYS_DEBUG_OUT_SEL"},
		{0x700,	"MDPSYS_SW0_RST_B"},
		{0x704,	"MDPSYS_SW1_RST_B"},
		{0x708,	"MDPSYS_SW2_RST_B"},
		{0x70C,	"MDPSYS_SW3_RST_B"},
		{0x710,	"MDPSYS_SW4_RST_B"},
		{0x804,	"MDPSYS_SMI_BIST"},
		{0x808,	"MDPSYS_SMI_TX_IDLE"},
		{0x8DC,	"MDPSYS_SMI_LARB_GREQ"},
		{0x8F0,	"MDPSYS_HRT_WEIGHT_READ"},
		{0x900,	"MDPSYS_PWR_METER_CTL0"},
		{0x904,	"MDPSYS_PWR_METER_CTL1"},
		{0xE00,	"MDPSYS_BUF_UNDERRUN"},
		{0xE04,	"MDPSYS_BUF_UNDERRUN_ID"},
		{0xF00,	"BYPASS_MUX_SHADOW"},
		{0xF04,	"MDPSYS_MOUT_RST"},
		{0xF10,	"MDPSYS_SECURITY_DISABLE"},
		{0xF14,	"MDP_DLI0_SEL_IN"},
		{0xF18,	"MDP_DLI1_SEL_IN"},
		{0xF20,	"MDP_RDMA0_MOUT_EN"},
		{0xF24,	"MDP_RDMA1_MOUT_EN"},
		{0xF30,	"MDP_PQ0_SEL_IN"},
		{0xF34,	"MDP_PQ1_SEL_IN"},
		{0xF70,	"MDP_WROT0_SEL_IN"},
		{0xF74,	"MDP_WROT1_SEL_IN"},
		{0xF80,	"MDP_PQ0_SOUT_SEL"},
		{0xF84,	"MDP_PQ1_SOUT_SEL"},
		{0xF88, "MDP_DLO0_SOUT_SEL"},
		{0xF8C, "MDP_DLO1_SOUT_SEL"},
		{0xF90,	"MDP_BYP0_MOUT_EN"},
		{0xF94,	"MDP_BYP1_MOUT_EN"},
		{0xF98,	"MDP_BYP0_SEL_IN"},
		{0xF9C,	"MDP_BYP1_SEL_IN"},
		{0xFA0, "MDP_RSZ2_SEL_IN"},
		{0xFA4, "MDP_RSZ3_SEL_IN"},
		{0xFA8, "MDP_AID_SEL"},
		{0xFAC, "MDP_AID_SEL_MODE"},
		{0xFD0, "MDPSYS_MOUT_MASK0"},
		{0xFD4, "MDPSYS_MOUT_MASK1"},
		{0xFD8, "MDPSYS_MOUT_MASK2"},
		{0xFE0, "MDPSYS_DL_VALID0"},
		{0xFE4, "MDPSYS_DL_VALID1"},
		{0xFE8, "MDPSYS_DL_VALID2"},
		{0xFF0, "MDPSYS_DL_READY0"},
		{0xFF4, "MDPSYS_DL_READY1"},
		{0xFF8, "MDPSYS_DL_READY2"},
	};

	if (!(handle->engineFlag & CMDQ_ENG_MDP_GROUP_BITS))
		return;

	CMDQ_ERR("============ [CMDQ] MMSYS_CONFIG ============\n");

	if (!MMSYS_CONFIG_BASE) {
		CMDQ_ERR("mmsys not porting\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(configRegisters); i++) {
		value = CMDQ_REG_GET32(MMSYS_CONFIG_BASE +
			configRegisters[i].offset);
		CMDQ_ERR("%s: 0x%08x\n", configRegisters[i].name, value);
	}

	/*DISP_MUTEX MOD*/
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x030);
	CMDQ_ERR("%s: 0x%08x\n", "MDP_MUTEX0_MOD0", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x034);
	CMDQ_ERR("%s: 0x%08x\n", "MDP_MUTEX0_MOD1", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x050);
	CMDQ_ERR("%s: 0x%08x\n", "MDP_MUTEX1_MOD0", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x054);
	CMDQ_ERR("%s: 0x%08x\n", "MDP_MUTEX1_MOD1", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x070);
	CMDQ_ERR("%s: 0x%08x\n", "MDP_MUTEX2_MOD0", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x074);
	CMDQ_ERR("%s: 0x%08x\n", "MDP_MUTEX2_MOD1", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x090);
	CMDQ_ERR("%s: 0x%08x\n", "MDP_MUTEX3_MOD0", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x094);
	CMDQ_ERR("%s: 0x%08x\n", "MDP_MUTEX3_MOD1", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x0B0);
	CMDQ_ERR("%s: 0x%08x\n", "MDP_MUTEX4_MOD0", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x0B4);
	CMDQ_ERR("%s: 0x%08x\n", "MDP_MUTEX4_MOD1", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x0D0);
	CMDQ_ERR("%s: 0x%08x\n", "MDP_MUTEX5_MOD0", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x0D4);
	CMDQ_ERR("%s: 0x%08x\n", "MDP_MUTEX5_MOD1", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x0F0);
	CMDQ_ERR("%s: 0x%08x\n", "MDP_MUTEX6_MOD0", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x0F4);
	CMDQ_ERR("%s: 0x%08x\n", "MDP_MUTEX6_MOD1", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x110);
	CMDQ_ERR("%s: 0x%08x\n", "MDP_MUTEX7_MOD0", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x114);
	CMDQ_ERR("%s: 0x%08x\n", "MDP_MUTEX7_MOD1", value);
}

int32_t cmdq_mdp_reset_with_mmsys(const uint64_t engineToResetAgain)
{
	long MMSYS_SW0_RST_B_REG = MMSYS_CONFIG_BASE + (0x700);
	int i = 0;
	uint64_t reset_bits0 = 0ULL;
	int engineResetBit[32] = {
		-1,			/* bit  0 : mdp_mutex0 */
		-1,			/* bit  1 : apb_bus */
		-1,			/* bit  2 : smi0 */
		CMDQ_ENG_MDP_RDMA0,	/* bit	3 : mdp_rdma0 */
		CMDQ_ENG_MDP_FG0,	/* bit  4 : mdp_fg0 */
		CMDQ_ENG_MDP_HDR0,	/* bit  5 : mdp_hdr0  */
		CMDQ_ENG_MDP_AAL0,	/* bit  6 : mdp_aal0  */
		CMDQ_ENG_MDP_RSZ0,	/* bit  7 : mdp_rsz0 */
		CMDQ_ENG_MDP_TDSHP0,	/* bit  8 : mdp_tdshp0 */
		CMDQ_ENG_MDP_COLOR0,	/* bit  9 : mdp_color0 */
		CMDQ_ENG_MDP_WROT0,	/* bit 10 : mdp_wrot0 */
		-1,			/* bit 11 : mdp_fake_eng0 */
		-1,			/* bit 12 : img_dl_relay0 */
		-1,			/* bit 13 : img_dl_relay1 */
		-1,			/* bit 14 : empty */
		CMDQ_ENG_MDP_RDMA1,	/* bit 15 : mdp_rdma1 */
		CMDQ_ENG_MDP_FG1,	/* bit 16 : mdp_fg1 */
		CMDQ_ENG_MDP_HDR1,	/* bit 17 : mdp_hdr1  */
		CMDQ_ENG_MDP_AAL1,	/* bit 18 : mdp_aal1  */
		CMDQ_ENG_MDP_RSZ1,	/* bit 19 : mdp_rsz1 */
		CMDQ_ENG_MDP_TDSHP1,	/* bit 20 : mdp_tdshp1 */
		CMDQ_ENG_MDP_COLOR1,	/* bit 21 : mdp_color1 */
		CMDQ_ENG_MDP_WROT1,	/* bit 22 : mdp_wrot1 */
		-1,			/* bit 23 : empty */
		-1,			/* bit 24 : mdp_rsz2 */
		-1,			/* bit 25 : mdp_wrot2 */
		-1,			/* bit 26 : mdp_dlo_async0 */
		-1,			/* bit 27 : empty_27 */
		-1,			/* bit 28 : mdp_rsz3 */
		-1,			/* bit 29 : mdp_wrot3 */
		-1,			/* bit 30 : mdp_dlo_async1 */
		-1,			/* bit 31 : hre_top_mdpsys */
	};

	for (i = 0; i < 32; ++i) {
		if (engineResetBit[i] < 0)
			continue;

		if (engineToResetAgain & (1LL << engineResetBit[i]))
			reset_bits0 |= (1ULL << i);
	}

	if (reset_bits0 != 0) {
		/* 0: reset */
		/* 1: not reset */
		/* so we need to reverse the bits */
		reset_bits0 = ~reset_bits0;

		CMDQ_REG_SET32(MMSYS_SW0_RST_B_REG, reset_bits0);
		CMDQ_REG_SET32(MMSYS_SW0_RST_B_REG, ~0);
		/* This takes effect immediately, no need to poll state */
	}

	return 0;
}

#ifdef MDP_IOMMU_DEBUG
int cmdq_TranslationFault_callback(
	int port, dma_addr_t mva, void *data)
{
	char dispatchModel[MDP_DISPATCH_KEY_STR_LEN] = "MDP";

	CMDQ_ERR("================= [MDP M4U] Dump Begin ================\n");
	CMDQ_ERR("[MDP M4U]fault call port=%d, mva=%pa", port, &mva);

	cmdq_core_dump_tasks_info();

	CMDQ_ERR(
		"=============== [MDP] Frame Information Begin ===============================\n");
	/* find dispatch module and assign dispatch key */
	cmdq_mdp_check_TF_address(mva, dispatchModel);
	memcpy(data, dispatchModel, sizeof(dispatchModel));
	CMDQ_ERR(
		"=============== [MDP] Frame Information End =================================\n");
	CMDQ_ERR("================= [MDP M4U] Dump End ================\n");

	return 0;
}
#endif

void cmdq_mdp_init_module_base_VA(void)
{
	memset(&gCmdqMdpModuleBaseVA, 0, sizeof(struct CmdqMdpModuleBaseVA));

	gCmdqMdpModuleBaseVA.MDP_RDMA0 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_rdma0");
	gCmdqMdpModuleBaseVA.MDP_RDMA1 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_rdma1");
	gCmdqMdpModuleBaseVA.MDP_RSZ0 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_rsz0");
	gCmdqMdpModuleBaseVA.MDP_RSZ1 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_rsz1");
	gCmdqMdpModuleBaseVA.MDP_WROT0 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_wrot0");
	gCmdqMdpModuleBaseVA.MDP_WROT1 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_wrot1");
	gCmdqMdpModuleBaseVA.MDP_TDSHP0 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_tdshp0");
	gCmdqMdpModuleBaseVA.MDP_TDSHP1 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_tdshp1");
	gCmdqMdpModuleBaseVA.MDP_AAL0 =
		cmdq_dev_alloc_reference_by_name("mdp_aal0",
			&mdp_module_pa.aal0);
	gCmdqMdpModuleBaseVA.MDP_AAL1 =
		cmdq_dev_alloc_reference_by_name("mdp_aal1",
			&mdp_module_pa.aal1);
	gCmdqMdpModuleBaseVA.MDP_COLOR0 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_color0");
	gCmdqMdpModuleBaseVA.MDP_COLOR1 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_color1");
	gCmdqMdpModuleBaseVA.MDP_HDR0 =
		cmdq_dev_alloc_reference_by_name("mdp_hdr0",
		&mdp_module_pa.hdr0);
	gCmdqMdpModuleBaseVA.MDP_HDR1 =
		cmdq_dev_alloc_reference_by_name("mdp_hdr1",
		&mdp_module_pa.hdr1);
	gCmdqMdpModuleBaseVA.MDP_FG0 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_fg0");
	gCmdqMdpModuleBaseVA.MDP_FG1 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_fg1");
	gCmdqMdpModuleBaseVA.MM_MUTEX =
		cmdq_dev_alloc_reference_VA_by_name("mm_mutex");
}

void cmdq_mdp_deinit_module_base_VA(void)
{
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_RDMA0());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_RDMA1());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_RSZ0());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_RSZ1());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_WROT0());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_WROT1());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_TDSHP0());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_TDSHP1());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_COLOR0());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_COLOR1());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_HDR0());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_HDR1());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_FG0());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_FG1());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_AAL0());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_AAL1());

	memset(&gCmdqMdpModuleBaseVA, 0, sizeof(struct CmdqMdpModuleBaseVA));
}

bool cmdq_mdp_clock_is_on(u32 engine)
{
	switch (engine) {
	case CMDQ_ENG_MDP_RDMA0:
		return cmdq_mdp_clock_is_enable_MDP_RDMA0();
	case CMDQ_ENG_MDP_RDMA1:
		return cmdq_mdp_clock_is_enable_MDP_RDMA1();
	case CMDQ_ENG_MDP_RSZ0:
		return cmdq_mdp_clock_is_enable_MDP_RSZ0();
	case CMDQ_ENG_MDP_RSZ1:
		return cmdq_mdp_clock_is_enable_MDP_RSZ1();
	case CMDQ_ENG_MDP_WROT0:
		return cmdq_mdp_clock_is_enable_MDP_WROT0();
	case CMDQ_ENG_MDP_WROT1:
		return cmdq_mdp_clock_is_enable_MDP_WROT1();
	case CMDQ_ENG_MDP_TDSHP0:
		return cmdq_mdp_clock_is_enable_MDP_TDSHP0();
	case CMDQ_ENG_MDP_TDSHP1:
		return cmdq_mdp_clock_is_enable_MDP_TDSHP1();
	case CMDQ_ENG_MDP_AAL0:
		return cmdq_mdp_clock_is_enable_MDP_AAL0();
	case CMDQ_ENG_MDP_AAL1:
		return cmdq_mdp_clock_is_enable_MDP_AAL1();
	case CMDQ_ENG_MDP_COLOR0:
		return cmdq_mdp_clock_is_enable_MDP_COLOR0();
	case CMDQ_ENG_MDP_COLOR1:
		return cmdq_mdp_clock_is_enable_MDP_COLOR1();
	case CMDQ_ENG_MDP_HDR0:
		return cmdq_mdp_clock_is_enable_MDP_HDR0();
	case CMDQ_ENG_MDP_HDR1:
		return cmdq_mdp_clock_is_enable_MDP_HDR1();
	case CMDQ_ENG_MDP_FG0:
		return cmdq_mdp_clock_is_enable_MDP_FG0();
	case CMDQ_ENG_MDP_FG1:
		return cmdq_mdp_clock_is_enable_MDP_FG1();
	default:
		CMDQ_ERR("try to query unknown mdp clock");
		return false;
	}
}

void cmdq_mdp_enable_clock(bool enable, u32 engine)
{
	switch (engine) {
	case CMDQ_ENG_MDP_RDMA0:
		cmdq_mdp_enable_clock_MDP_RDMA0(enable);
		break;
	case CMDQ_ENG_MDP_RDMA1:
		cmdq_mdp_enable_clock_MDP_RDMA1(enable);
		break;
	case CMDQ_ENG_MDP_RSZ0:
		cmdq_mdp_enable_clock_MDP_RSZ0(enable);
		break;
	case CMDQ_ENG_MDP_RSZ1:
		cmdq_mdp_enable_clock_MDP_RSZ1(enable);
		break;
	case CMDQ_ENG_MDP_WROT0:
		cmdq_mdp_enable_clock_MDP_WROT0(enable);
		break;
	case CMDQ_ENG_MDP_WROT1:
		cmdq_mdp_enable_clock_MDP_WROT1(enable);
		break;
	case CMDQ_ENG_MDP_TDSHP0:
		cmdq_mdp_enable_clock_MDP_TDSHP0(enable);
		break;
	case CMDQ_ENG_MDP_TDSHP1:
		cmdq_mdp_enable_clock_MDP_TDSHP1(enable);
		break;
	case CMDQ_ENG_MDP_COLOR0:
		cmdq_mdp_enable_clock_MDP_COLOR0(enable);
		break;
	case CMDQ_ENG_MDP_COLOR1:
		cmdq_mdp_enable_clock_MDP_COLOR1(enable);
		break;
	case CMDQ_ENG_MDP_HDR0:
		cmdq_mdp_enable_clock_MDP_HDR0(enable);
		break;
	case CMDQ_ENG_MDP_HDR1:
		cmdq_mdp_enable_clock_MDP_HDR1(enable);
		break;
	case CMDQ_ENG_MDP_AAL0:
		cmdq_mdp_enable_clock_MDP_AAL0(enable);
		break;
	case CMDQ_ENG_MDP_AAL1:
		cmdq_mdp_enable_clock_MDP_AAL1(enable);
		break;
	case CMDQ_ENG_MDP_FG0:
		cmdq_mdp_enable_clock_MDP_FG0(enable);
		break;
	case CMDQ_ENG_MDP_FG1:
		cmdq_mdp_enable_clock_MDP_FG1(enable);
		break;
	default:
		CMDQ_ERR("try to enable unknown mdp clock");
		break;
	}
}

/* Common Clock Framework */
void cmdq_mdp_init_module_clk(void)
{
	cmdq_dev_get_module_clock_by_name("mmsys_config", "MDP_APB_BUS",
		&gCmdqMdpModuleClock.clk_APB);
	cmdq_dev_get_module_clock_by_name("mm_mutex", "MDP_MUTEX0",
		&gCmdqMdpModuleClock.clk_MDP_MUTEX0);
	cmdq_dev_get_module_clock_by_name("mdp_rdma0", "MDP_RDMA0",
		&gCmdqMdpModuleClock.clk_MDP_RDMA0);
	cmdq_dev_get_module_clock_by_name("mdp_rdma1", "MDP_RDMA1",
		&gCmdqMdpModuleClock.clk_MDP_RDMA1);
	cmdq_dev_get_module_clock_by_name("mdp_rsz0", "MDP_RSZ0",
		&gCmdqMdpModuleClock.clk_MDP_RSZ0);
	cmdq_dev_get_module_clock_by_name("mdp_rsz1", "MDP_RSZ1",
		&gCmdqMdpModuleClock.clk_MDP_RSZ1);
	cmdq_dev_get_module_clock_by_name("mdp_wrot0", "MDP_WROT0",
		&gCmdqMdpModuleClock.clk_MDP_WROT0);
	cmdq_dev_get_module_clock_by_name("mdp_wrot1", "MDP_WROT1",
		&gCmdqMdpModuleClock.clk_MDP_WROT1);
	cmdq_dev_get_module_clock_by_name("mdp_tdshp0", "MDP_TDSHP0",
		&gCmdqMdpModuleClock.clk_MDP_TDSHP0);
	cmdq_dev_get_module_clock_by_name("mdp_tdshp1", "MDP_TDSHP1",
		&gCmdqMdpModuleClock.clk_MDP_TDSHP1);
	cmdq_dev_get_module_clock_by_name("mdp_color0", "MDP_COLOR0",
		&gCmdqMdpModuleClock.clk_MDP_COLOR0);
	cmdq_dev_get_module_clock_by_name("mdp_color1", "MDP_COLOR1",
		&gCmdqMdpModuleClock.clk_MDP_COLOR1);
	cmdq_dev_get_module_clock_by_name("mdp_aal0", "MDP_AAL0",
		&gCmdqMdpModuleClock.clk_MDP_AAL0);
	cmdq_dev_get_module_clock_by_name("mdp_aal1", "MDP_AAL1",
		&gCmdqMdpModuleClock.clk_MDP_AAL1);
	cmdq_dev_get_module_clock_by_name("mdp_hdr0", "MDP_HDR0",
		&gCmdqMdpModuleClock.clk_MDP_HDR0);
	cmdq_dev_get_module_clock_by_name("mdp_hdr1", "MDP_HDR1",
		&gCmdqMdpModuleClock.clk_MDP_HDR1);
	cmdq_dev_get_module_clock_by_name("mdp_fg0", "MDP_FG0",
		&gCmdqMdpModuleClock.clk_MDP_FG0);
	cmdq_dev_get_module_clock_by_name("mdp_fg1", "MDP_FG1",
		&gCmdqMdpModuleClock.clk_MDP_FG1);

}

/* MDP engine dump */
void cmdq_mdp_dump_rsz(const unsigned long base, const char *label)
{
	uint32_t value[38] = { 0 };
	uint32_t request[4] = { 0 };
	uint32_t state = 0;

	value[0] = CMDQ_REG_GET32(base + 0x000);    /*  RSZ_ENABLE*/
	value[1] = CMDQ_REG_GET32(base + 0x004);    /*  RSZ_CONTROL_1   */
	value[2] = CMDQ_REG_GET32(base + 0x008);    /*  RSZ_CONTROL_2   */
	value[3] = CMDQ_REG_GET32(base + 0x010);    /*  RSZ_INPUT_IMAGE */
	value[4] = CMDQ_REG_GET32(base + 0x014);    /*  RSZ_OUTPUT_IMAGE    */
	value[5] = CMDQ_REG_GET32(base + 0x018);/*  RSZ_HORIZONTAL_COEFF_STEP */
	value[6] = CMDQ_REG_GET32(base + 0x01C);/*  RSZ_VERTICAL_COEFF_STEP */
	CMDQ_REG_SET32(base + 0x044, 0x00000001);
	value[7] = CMDQ_REG_GET32(base + 0x048);    /*  RSZ_DEBUG_1    */
	CMDQ_REG_SET32(base + 0x044, 0x00000002);
	value[8] = CMDQ_REG_GET32(base + 0x048);    /*  RSZ_DEBUG_2 */
	CMDQ_REG_SET32(base + 0x044, 0x00000003);
	value[9] = CMDQ_REG_GET32(base + 0x048);    /*  RSZ_DEBUG_3 */
	CMDQ_REG_SET32(base + 0x044, 0x00000009);
	value[10] = CMDQ_REG_GET32(base + 0x048);   /*  RSZ_DEBUG_9  */
	CMDQ_REG_SET32(base + 0x044, 0x0000000A);
	value[11] = CMDQ_REG_GET32(base + 0x048);   /*  RSZ_DEBUG_10    */
	CMDQ_REG_SET32(base + 0x044, 0x0000000B);
	value[12] = CMDQ_REG_GET32(base + 0x048);   /*  RSZ_DEBUG_11    */
	CMDQ_REG_SET32(base + 0x044, 0x0000000D);
	value[13] = CMDQ_REG_GET32(base + 0x048);   /*  RSZ_DEBUG_13    */
	CMDQ_REG_SET32(base + 0x044, 0x0000000E);
	value[14] = CMDQ_REG_GET32(base + 0x048);   /*  RSZ_DEBUG_14    */
	value[15] = CMDQ_REG_GET32(base + 0x100);   /*  PAT1_GEN_SET    */
	value[16] = CMDQ_REG_GET32(base + 0x200);   /*  PAT2_GEN_SET    */
	value[17] = CMDQ_REG_GET32(base + 0x00C);   /*  RSZ_INT_FLAG    */
	/*  RSZ_LUMA_HORIZONTAL_INTEGER_OFFSET  */
	value[18] = CMDQ_REG_GET32(base + 0x020);
	/*  RSZ_LUMA_HORIZONTAL_SUBPIXEL_OFFSET */
	value[19] = CMDQ_REG_GET32(base + 0x024);
	/*  RSZ_LUMA_VERTICAL_INTEGER_OFFSET    */
	value[20] = CMDQ_REG_GET32(base + 0x028);
	/*  RSZ_LUMA_VERTICAL_SUBPIXEL_OFFSET   */
	value[21] = CMDQ_REG_GET32(base + 0x02C);
	/*  RSZ_CHROMA_HORIZONTAL_INTEGER_OFFSET    */
	value[22] = CMDQ_REG_GET32(base + 0x030);
	/*  RSZ_CHROMA_HORIZONTAL_SUBPIXEL_OFFSET   */
	value[23] = CMDQ_REG_GET32(base + 0x034);
	/*  RSZ_RSV */
	value[24] = CMDQ_REG_GET32(base + 0x040);
	value[25] = CMDQ_REG_GET32(base + 0x04C);   /*  RSZ_TAP_ADAPT	*/
	value[26] = CMDQ_REG_GET32(base + 0x050);   /*  RSZ_IBSE_SOFTCLIP */
	value[27] = CMDQ_REG_GET32(base + 0x22C);   /*  RSZ_ETC_CONTROL	*/
	/*  RSZ_ETC_SWITCH_MAX_MIN_1	*/
	value[28] = CMDQ_REG_GET32(base + 0x230);
	/*  RSZ_ETC_SWITCH_MAX_MIN_2	*/
	value[29] = CMDQ_REG_GET32(base + 0x234);
	/*  RSZ_ETC_RING_CONTROL	*/
	value[30] = CMDQ_REG_GET32(base + 0x238);
	/*  RSZ_ETC_RING_CONTROL_GAINCONTROL_1	*/
	value[31] = CMDQ_REG_GET32(base + 0x23C);
	/*  RSZ_ETC_RING_CONTROL_GAINCONTROL_2	*/
	value[32] = CMDQ_REG_GET32(base + 0x240);
	/*  RSZ_ETC_RING_CONTROL_GAINCONTROL_3	*/
	value[33] = CMDQ_REG_GET32(base + 0x244);
	/*  RSZ_ETC_SIMILARITY_PROTECTION_GAINCONTROL_1	*/
	value[34] = CMDQ_REG_GET32(base + 0x248);
	/*  RSZ_ETC_SIMILARITY_PROTECTION_GAINCONTROL_2	*/
	value[35] = CMDQ_REG_GET32(base + 0x24C);
	/*  RSZ_ETC_SIMILARITY_PROTECTION_GAINCONTROL_3	*/
	value[36] = CMDQ_REG_GET32(base + 0x250);
	value[37] = CMDQ_REG_GET32(base + 0x254);   /*  RSZ_ETC_BLEND	*/

	CMDQ_ERR(
		"=============== [CMDQ] %s Status ====================================\n",
		label);
	CMDQ_ERR(
		"RSZ_ENABLE: 0x%08x, RSZ_CONTROL_1: 0x%08x, RSZ_CONTROL_2: 0x%08x, RSZ_INPUT_IMAGE: 0x%08x, RSZ_OUTPUT_IMAGE: 0x%08x\n",
		value[0], value[1], value[2], value[3],  value[4]);
	CMDQ_ERR(
		"RSZ_HORIZONTAL_COEFF_STEP: 0x%08x, RSZ_VERTICAL_COEFF_STEP: 0x%08x\n",
		value[5], value[6]);
	CMDQ_ERR(
		"RSZ_DEBUG_1: 0x%08x, RSZ_DEBUG_2: 0x%08x, RSZ_DEBUG_3: 0x%08x\n",
		value[7], value[8], value[9]);
	CMDQ_ERR(
		"RSZ_DEBUG_9: 0x%08x, RSZ_DEBUG_10: 0x%08x, RSZ_DEBUG_11: 0x%08x\n",
		value[10], value[11], value[12]);
	CMDQ_ERR(
		"RSZ_DEBUG_13: 0x%08x, RSZ_DEBUG_14: 0x%08x\n",
		value[13], value[14]);
	CMDQ_ERR("PAT1_GEN_SET: 0x%08x, PAT2_GEN_SET: 0x%08x\n",
		value[15], value[16]);
	CMDQ_ERR(
		"RSZ_INT_FLAG: 0x%08x, RSZ_LUMA_HORIZONTAL_INTEGER_OFFSET: 0x%08x, RSZ_LUMA_HORIZONTAL_SUBPIXEL_OFFSET: 0x%08x\n",
		value[17], value[18], value[19]);
	CMDQ_ERR(
		"RSZ_LUMA_VERTICAL_INTEGER_OFFSET: 0x%08x, RSZ_LUMA_VERTICAL_SUBPIXEL_OFFSET: 0x%08x, RSZ_CHROMA_HORIZONTAL_INTEGER_OFFSET: 0x%08x\n",
		value[20], value[21], value[22]);
	CMDQ_ERR(
		"RSZ_CHROMA_HORIZONTAL_SUBPIXEL_OFFSET: 0x%08x, RSZ_RSV: 0x%08x, RSZ_TAP_ADAPT: 0x%08x\n",
		value[23], value[24], value[25]);
	CMDQ_ERR(
		"RSZ_IBSE_SOFTCLIP: 0x%08x, RSZ_ETC_CONTROL: 0x%08x, RSZ_ETC_SWITCH_MAX_MIN_1: 0x%08x\n",
		value[26], value[27], value[28]);
	CMDQ_ERR(
		"RSZ_ETC_SWITCH_MAX_MIN_2: 0x%08x, RSZ_ETC_RING_CONTROL: 0x%08x, RSZ_ETC_RING_CONTROL_GAINCONTROL_1: 0x%08x\n",
		value[29], value[30], value[31]);
	CMDQ_ERR(
		"RSZ_ETC_RING_CONTROL_GAINCONTROL_2: 0x%08x, RSZ_ETC_RING_CONTROL_GAINCONTROL_3: 0x%08x, RSZ_ETC_SIMILARITY_PROTECTION_GAINCONTROL_1: 0x%08x\n",
		value[32], value[33], value[34]);
	CMDQ_ERR(
		"RSZ_ETC_SIMILARITY_PROTECTION_GAINCONTROL_2: 0x%08x, RSZ_ETC_SIMILARITY_PROTECTION_GAINCONTROL_3: 0x%08x, RSZ_ETC_BLEND: 0x%08x\n",
		value[35], value[36], value[37]);
	/* parse state */
	/* .valid=1/request=1: upstream module sends data */
	/* .ready=1: downstream module receives data */
	state = value[8] & 0xF;
	request[0] = state & (0x1);	/* out valid */
	request[1] = (state & (0x1 << 1)) >> 1;	/* out ready */
	request[2] = (state & (0x1 << 2)) >> 2;	/* in valid */
	request[3] = (state & (0x1 << 3)) >> 3;	/* in ready */
	CMDQ_ERR("RSZ inRdy,inRsq,outRdy,outRsq: %d,%d,%d,%d (%s)\n",
		request[3], request[2], request[1], request[0],
		cmdq_mdp_get_rsz_state(state));
}

void cmdq_mdp_dump_tdshp(const unsigned long base, const char *label)
{
	uint32_t value[10] = { 0 };

	value[0] = CMDQ_REG_GET32(base + 0x114);
	value[1] = CMDQ_REG_GET32(base + 0x11C);
	value[2] = CMDQ_REG_GET32(base + 0x104);
	value[3] = CMDQ_REG_GET32(base + 0x108);
	value[4] = CMDQ_REG_GET32(base + 0x10C);
	value[5] = CMDQ_REG_GET32(base + 0x110);
	value[6] = CMDQ_REG_GET32(base + 0x120);
	value[7] = CMDQ_REG_GET32(base + 0x124);
	value[8] = CMDQ_REG_GET32(base + 0x128);
	value[9] = CMDQ_REG_GET32(base + 0x12C);
	CMDQ_ERR(
		"=============== [CMDQ] %s Status ====================================\n",
		label);
	CMDQ_ERR("TDSHP INPUT_CNT: 0x%08x, OUTPUT_CNT: 0x%08x\n",
		value[0], value[1]);
	CMDQ_ERR("TDSHP INTEN: 0x%08x, INTSTA: 0x%08x, STATUS: 0x%08x\n",
		value[2], value[3], value[4]);
	CMDQ_ERR("TDSHP CFG: 0x%08x, IN_SIZE: 0x%08x, OUT_SIZE: 0x%08x\n",
		value[5], value[6], value[8]);
	CMDQ_ERR("TDSHP OUTPUT_OFFSET: 0x%08x, BLANK_WIDTH: 0x%08x\n",
		value[7], value[9]);
}

void cmdq_mdp_dump_aal(const unsigned long base, const char *label)
{
	uint32_t value[9] = { 0 };

	value[0] = CMDQ_REG_GET32(base + 0x00C);    /* MDP_AAL_INTSTA       */
	value[1] = CMDQ_REG_GET32(base + 0x010);    /* MDP_AAL_STATUS       */
	value[2] = CMDQ_REG_GET32(base + 0x024);    /* MDP_AAL_INPUT_COUNT  */
	value[3] = CMDQ_REG_GET32(base + 0x028);    /* MDP_AAL_OUTPUT_COUNT */
	value[4] = CMDQ_REG_GET32(base + 0x030);    /* MDP_AAL_SIZE         */
	value[5] = CMDQ_REG_GET32(base + 0x034);    /* MDP_AAL_OUTPUT_SIZE  */
	value[6] = CMDQ_REG_GET32(base + 0x038);    /* MDP_AAL_OUTPUT_OFFSET*/
	value[7] = CMDQ_REG_GET32(base + 0x4EC);    /* MDP_AAL_TILE_00      */
	value[8] = CMDQ_REG_GET32(base + 0x4F0);    /* MDP_AAL_TILE_01      */
	CMDQ_ERR(
		"=============== [CMDQ] %s Status ====================================\n",
		label);
	CMDQ_ERR("AAL_INTSTA: 0x%08x, AAL_STATUS: 0x%08x\n",
		value[0], value[1]);
	CMDQ_ERR(
		"AAL_INPUT_COUNT: 0x%08x, AAL_OUTPUT_COUNT: 0x%08x, AAL_SIZE: 0x%08x\n",
		value[2], value[3], value[4]);
	CMDQ_ERR("AAL_OUTPUT_SIZE: 0x%08x, AAL_OUTPUT_OFFSET: 0x%08x\n",
		value[5], value[6]);
	CMDQ_ERR("AAL_TILE_00: 0x%08x, AAL_TILE_01: 0x%08x\n",
		value[7], value[8]);
}

void cmdq_mdp_dump_hdr(const unsigned long base, const char *label)
{
	uint32_t value[15] = { 0 };

	value[0] = CMDQ_REG_GET32(base + 0x000);    /* MDP_HDR_TOP            */
	value[1] = CMDQ_REG_GET32(base + 0x004);    /* MDP_HDR_RELAY          */
	value[2] = CMDQ_REG_GET32(base + 0x00C);    /* MDP_HDR_INTSTA         */
	value[3] = CMDQ_REG_GET32(base + 0x010);    /* MDP_HDR_ENGSTA         */
	value[4] = CMDQ_REG_GET32(base + 0x020);    /* MDP_HDR_HIST_CTRL_0    */
	value[5] = CMDQ_REG_GET32(base + 0x024);    /* MDP_HDR_HIST_CTRL_1    */
	value[6] = CMDQ_REG_GET32(base + 0x014);    /* MDP_HDR_SIZE_0         */
	value[7] = CMDQ_REG_GET32(base + 0x018);    /* MDP_HDR_SIZE_1         */
	value[8] = CMDQ_REG_GET32(base + 0x01C);    /* MDP_HDR_SIZE_2         */
	value[9] = CMDQ_REG_GET32(base + 0x10C);    /* MDP_HDR_CURSOR_CTRL    */
	value[10] = CMDQ_REG_GET32(base + 0x110);   /* MDP_HDR_CURSOR_POS     */
	value[11] = CMDQ_REG_GET32(base + 0x114);   /* MDP_HDR_CURSOR_COLOR   */
	value[12] = CMDQ_REG_GET32(base + 0x118);   /* MDP_HDR_TILE_POS       */
	value[13] = CMDQ_REG_GET32(base + 0x11C);   /* MDP_HDR_CURSOR_BUF0    */
	value[14] = CMDQ_REG_GET32(base + 0x120);   /* MDP_HDR_CURSOR_BUF1    */
	CMDQ_ERR(
		"=============== [CMDQ] %s Status ====================================\n",
		label);
	CMDQ_ERR("HDR_TOP: 0x%08x, HDR_RELAY: 0x%08x, HDR_INTSTA: 0x%08x\n",
		value[0], value[1], value[2]);
	CMDQ_ERR(
		"HDR_ENGSTA: 0x%08x, HDR_HIST_CTRL0: 0x%08x, HDR_HIST_CTRL1: 0x%08x\n",
		value[3], value[4], value[5]);
	CMDQ_ERR("HDR_SIZE_0: 0x%08x, HDR_SIZE_1: 0x%08x, HDR_SIZE_2: 0x%08x\n",
		value[6], value[7], value[8]);
	CMDQ_ERR(
		"HDR_CURSOR_CTRL: 0x%08x, HDR_CURSOR_POS: 0x%08x, HDR_CURSOR_COLOR: 0x%08x\n",
		value[9], value[10], value[11]);
	CMDQ_ERR(
		"HDR_TILE_POS: 0x%08x, HDR_CURSOR_BUF0: 0x%08x, HDR_CURSOR_BUF1: 0x%08x\n",
		value[12], value[13], value[14]);
}

void cmdq_mdp_dump_fg(const unsigned long base, const char *label)
{
	uint32_t value[16] = { 0 };

	value[0] = CMDQ_REG_GET32(base + 0x000);    /* MDP_FG_TRIGGER   */
	value[1] = CMDQ_REG_GET32(base + 0x004);    /* MDP_FG_STATUS    */
	value[2] = CMDQ_REG_GET32(base + 0x020);    /* MDP_FG_FG_CTRL_0 */
	value[3] = CMDQ_REG_GET32(base + 0x024);    /* MDP_FG_FG_CK_EN  */
	value[4] = CMDQ_REG_GET32(base + 0x02C);    /* MDP_FG_BACK_DOOR_0*/
	value[5] = CMDQ_REG_GET32(base + 0x400);    /* MDP_FG_PIC_INFO_0*/
	value[6] = CMDQ_REG_GET32(base + 0x404);    /* MDP_FG_PIC_INFO_1*/
	value[7] = CMDQ_REG_GET32(base + 0x418);    /* MDP_FG_TILE_INFO_0*/
	value[8] = CMDQ_REG_GET32(base + 0x41C);    /* MDP_FG_TILE_INFO_1*/
	value[9] = CMDQ_REG_GET32(base + 0x500);    /* MDP_FG_DEBUG_0   */
	value[10] = CMDQ_REG_GET32(base + 0x504);   /* MDP_FG_DEBUG_1   */
	value[11] = CMDQ_REG_GET32(base + 0x508);   /* MDP_FG_DEBUG_2   */
	value[12] = CMDQ_REG_GET32(base + 0x50C);   /* MDP_FG_DEBUG_3   */
	value[13] = CMDQ_REG_GET32(base + 0x510);   /* MDP_FG_DEBUG_4   */
	value[14] = CMDQ_REG_GET32(base + 0x514);   /* MDP_FG_DEBUG_5   */
	value[15] = CMDQ_REG_GET32(base + 0x518);   /* MDP_FG_DEBUG_6   */

	CMDQ_ERR(
		"=============== [CMDQ] %s Status ====================================\n",
		label);
	CMDQ_ERR(
		"MDP_FG_TRIGGER 0x%08x, MDP_FG_STATUS 0x%08x, MDP_FG_FG_CTRL_0 0x%08x\n",
		value[0], value[1], value[2]);
	CMDQ_ERR(
		"MDP_FG_FG_CK_EN 0x%08x, MDP_FG_BACK_DOOR_0 0x%08x, MDP_FG_PIC_INFO_0 0x%08x\n",
		value[3], value[4], value[5]);
	CMDQ_ERR(
		"MDP_FG_PIC_INFO_1 0x%08x, MDP_FG_TILE_INFO_0 0x%08x, MDP_FG_TILE_INFO_1 0x%08x\n",
		value[6], value[7], value[8]);
	CMDQ_ERR(
		"MDP_FG_DEBUG_0x%08x, MDP_FG_DEBUG_1 0x%08x, MDP_FG_DEBUG_2 0x%08x\n",
		value[9], value[10], value[11]);
	CMDQ_ERR(
		"MDP_FG_DEBUG_3 0x%08x, MDP_FG_DEBUG_4 0x%08x, MDP_FG_DEBUG_5 0x%08x\n",
		value[12], value[13], value[14]);
	CMDQ_ERR("MDP_FG_DEBUG_6 0x%08x\n",
		value[15]);
}

int32_t cmdqMdpClockOn(uint64_t engineFlag)
{
	CMDQ_MSG("%s: Enable MDP(0x%llx) clock begin\n", __func__, engineFlag);
#ifdef CMDQ_PWR_AWARE
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_RDMA0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_RDMA1);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_RSZ0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_RSZ1);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_AAL0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_AAL1);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_TDSHP0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_TDSHP1);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_COLOR0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_COLOR1);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_HDR0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_HDR1);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_FG0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_FG1);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_WROT0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_WROT1);
#else
	CMDQ_MSG("Force MDP clock all on\n");

	/* enable all bits in MMSYS_CG_CLR0 and MMSYS_CG_CLR1 */
	CMDQ_REG_SET32(MMSYS_CONFIG_BASE + 0x108, 0xFFFFFFFF);
	CMDQ_REG_SET32(MMSYS_CONFIG_BASE + 0x118, 0xFFFFFFFF);

#endif				/* #ifdef CMDQ_PWR_AWARE */

	CMDQ_MSG("%s: cmdq_util_prebuilt_init(0)\n", __func__);
	cmdq_util_prebuilt_init(0);

	CMDQ_MSG("%s: set BYPASS_MUX_SHADOW bit0 as 0x1\n", __func__);
	CMDQ_REG_SET32(MMSYS_CONFIG_BASE + 0xF00, 0x1);


	CMDQ_MSG("%s: Enable MDP(0x%llx) clock end\n", __func__, engineFlag);
	return 0;
}

struct MODULE_BASE {
	uint64_t engine;
	/* considering 64 bit kernel, use long type to store base addr */
	long base;
	const char *name;
};

#define DEFINE_MODULE(eng, base) {eng, base, #eng}

int32_t cmdqMdpDumpInfo(uint64_t engineFlag, int logLevel)
{
	if (engineFlag & (1LL << CMDQ_ENG_MDP_RDMA0))
		cmdq_mdp_dump_rdma(MDP_RDMA0_BASE, "RDMA0");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RDMA1))
		cmdq_mdp_dump_rdma(MDP_RDMA1_BASE, "RDMA1");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_FG0))
		cmdq_mdp_dump_fg(MDP_FG0_BASE, "FG0");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_FG1))
		cmdq_mdp_dump_fg(MDP_FG1_BASE, "FG1");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RSZ0))
		cmdq_mdp_get_func()->mdpDumpRsz(MDP_RSZ0_BASE, "RSZ0");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RSZ1))
		cmdq_mdp_get_func()->mdpDumpRsz(MDP_RSZ1_BASE, "RSZ1");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_TDSHP0))
		cmdq_mdp_get_func()->mdpDumpTdshp(MDP_TDSHP0_BASE, "TDSHP0");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_TDSHP1))
		cmdq_mdp_get_func()->mdpDumpTdshp(MDP_TDSHP1_BASE, "TDSHP1");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_COLOR0))
		cmdq_mdp_dump_color(MDP_COLOR0_BASE, "COLOR0");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_COLOR1))
		cmdq_mdp_dump_color(MDP_COLOR1_BASE, "COLOR1");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_WROT0))
		cmdq_mdp_dump_rot(MDP_WROT0_BASE, "WROT0");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_WROT1))
		cmdq_mdp_dump_rot(MDP_WROT1_BASE, "WROT1");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_HDR0))
		cmdq_mdp_dump_hdr(MDP_HDR0_BASE, "HDR0");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_HDR1))
		cmdq_mdp_dump_hdr(MDP_HDR1_BASE, "HDR1");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_AAL0))
		cmdq_mdp_dump_aal(MDP_AAL0_BASE, "AAL0");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_AAL1))
		cmdq_mdp_dump_aal(MDP_AAL1_BASE, "AAL1");

	/* verbose case, dump entire 1KB HW register region */
	/* for each enabled HW module. */
	if (logLevel >= 1) {
		int inner = 0;

		const struct MODULE_BASE bases[] = {
			DEFINE_MODULE(CMDQ_ENG_MDP_RDMA0, MDP_RDMA0_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_RDMA1, MDP_RDMA1_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_RSZ0, MDP_RSZ0_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_RSZ1, MDP_RSZ1_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_TDSHP0, MDP_TDSHP0_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_TDSHP1, MDP_TDSHP1_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_COLOR0, MDP_COLOR0_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_COLOR1, MDP_COLOR1_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_FG0, MDP_FG0_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_FG1, MDP_FG1_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_HDR0, MDP_HDR0_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_HDR1, MDP_HDR1_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_WROT0, MDP_WROT0_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_WROT1, MDP_WROT1_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_AAL0, MDP_AAL0_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_AAL1, MDP_AAL1_BASE),
		};

		for (inner = 0; inner < ARRAY_SIZE(bases); ++inner) {
			if (engineFlag & (1LL << bases[inner].engine)) {
				CMDQ_ERR(
					"========= [CMDQ] %s dump base 0x%lx ========\n",
					bases[inner].name, bases[inner].base);
				print_hex_dump(KERN_ERR, "",
					DUMP_PREFIX_ADDRESS, 32, 4,
					(void *)bases[inner].base, 1024,
					false);
			}
		}
	}

	return 0;
}


enum MOUT_BITS {
	MOUT_BITS_MDP_BYP0	=  0,  /* bit  0: mdp_byp0_mout multiple outupt reset */
	MOUT_BITS_MDP_BYP1	=  1,  /* bit  1: mdp_byp1_mout multiple outupt reset */
	MOUT_BITS_MDP_RDMA0	=  2,  /* bit  2: mdp_rdma0_mout multiple outupt reset */
	MOUT_BITS_MDP_RDMA1	=  3,  /* bit  3: mdp_rdma1_mout multiple outupt reset */
};

int32_t cmdqMdpResetEng(uint64_t engineFlag)
{
#ifndef CMDQ_PWR_AWARE
	return 0;
#else
	int status = 0;
	int64_t engineToResetAgain = 0LL;
	uint32_t mout_bits_old = 0L;
	uint32_t mout_bits = 0L;

	long MMSYS_MOUT_RST_REG = MMSYS_CONFIG_BASE + (0xF04);

	CMDQ_PROF_START(0, "MDP_Rst");
	CMDQ_VERBOSE("Reset MDP(engineFlag: 0x%llx) begin\n", engineFlag);

	/* After resetting each component, */
	/* we need also reset corresponding MOUT config. */
	mout_bits_old = CMDQ_REG_GET32(MMSYS_MOUT_RST_REG);
	mout_bits = 0;

	if (engineFlag & (1LL << CMDQ_ENG_MDP_DLI0_SEL))
		mout_bits |= (1 << MOUT_BITS_MDP_RDMA0);

	if (engineFlag & (1LL << CMDQ_ENG_MDP_DLI1_SEL))
		mout_bits |= (1 << MOUT_BITS_MDP_RDMA1);

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RDMA0)) {
		mout_bits |= (1 << MOUT_BITS_MDP_BYP0);
		engineToResetAgain |= (1LL << CMDQ_ENG_MDP_RDMA0);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RDMA1)) {
		mout_bits |= (1 << MOUT_BITS_MDP_BYP1);
		engineToResetAgain |= (1LL << CMDQ_ENG_MDP_RDMA1);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_TDSHP0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_TDSHP0)) {
			CMDQ_REG_SET32(MDP_TDSHP0_BASE + 0x100, 0x0);
			CMDQ_REG_SET32(MDP_TDSHP0_BASE + 0x100, 0x2);
			CMDQ_REG_SET32(MDP_TDSHP0_BASE + 0x100, 0x0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_TDSHP1)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_TDSHP1)) {
			CMDQ_REG_SET32(MDP_TDSHP1_BASE + 0x100, 0x0);
			CMDQ_REG_SET32(MDP_TDSHP1_BASE + 0x100, 0x2);
			CMDQ_REG_SET32(MDP_TDSHP1_BASE + 0x100, 0x0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_WROT0)) {
		status = cmdq_mdp_loop_reset(CMDQ_ENG_MDP_WROT0,
			MDP_WROT0_BASE + 0x010, MDP_WROT0_BASE + 0x014,
			0x1, 0x1, true);
		if (status != 0)
			engineToResetAgain |= (1LL << CMDQ_ENG_MDP_WROT0);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_WROT1)) {
		status = cmdq_mdp_loop_reset(CMDQ_ENG_MDP_WROT1,
			MDP_WROT1_BASE + 0x010, MDP_WROT1_BASE + 0x014,
			0x1, 0x1, true);
		if (status != 0)
			engineToResetAgain |= (1LL << CMDQ_ENG_MDP_WROT1);
	}

//TODO:
// AAL HDR TCC FG
	/*
	 * when MDP engines fail to reset,
	 * 1. print SMI debug log
	 * 2. try resetting from MMSYS to restore system state
	 * 3. report to QA by raising AEE warning
	 * this reset will reset all registers to power on state.
	 * but DpFramework always reconfigures register values,
	 * so there is no need to backup registers.
	 */
	if (engineToResetAgain != 0) {
		/* check SMI state immediately */
		/* if (1 == is_smi_larb_busy(0)) */
		/* { */
		/* smi_hanging_debug(5); */
		/* } */

		CMDQ_MSG(
			"Reset failed MDP engines(0x%llx), reset again with MMSYS_SW0_RST_B\n",
			 engineToResetAgain);

		cmdq_mdp_reset_with_mmsys(engineToResetAgain);
		cmdqMdpDumpInfo(engineToResetAgain, 0);
	}
	/* MOUT configuration reset */
	CMDQ_REG_SET32(MMSYS_MOUT_RST_REG, (mout_bits_old & (~mout_bits)));
	CMDQ_REG_SET32(MMSYS_MOUT_RST_REG, (mout_bits_old | mout_bits));
	CMDQ_REG_SET32(MMSYS_MOUT_RST_REG, (mout_bits_old & (~mout_bits)));

	CMDQ_MSG("Reset MDP(0x%llx) end\n", engineFlag);
	CMDQ_PROF_END(0, "MDP_Rst");

	return status;

#endif				/* #ifdef CMDQ_PWR_AWARE */

}

int32_t cmdqMdpClockOff(uint64_t engineFlag)
{
	CMDQ_MSG("%s, engineFlag: 0x%llx\n", __func__, engineFlag);

#ifdef CMDQ_PWR_AWARE

	CMDQ_MSG("%s: Disable MDP(0x%llx) clock begin\n", __func__, engineFlag);
	if (engineFlag & (1LL << CMDQ_ENG_MDP_WROT0)) {
		cmdq_mdp_loop_off(CMDQ_ENG_MDP_WROT0,
			MDP_WROT0_BASE + 0X010, MDP_WROT0_BASE + 0X014,
			0x1, 0x1, true);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_WROT1)) {
		cmdq_mdp_loop_off(CMDQ_ENG_MDP_WROT1,
			MDP_WROT1_BASE + 0X010, MDP_WROT1_BASE + 0X014,
			0x1, 0x1, true);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_TDSHP0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_TDSHP0)) {
			CMDQ_REG_SET32(MDP_TDSHP0_BASE + 0x100, 0x0);
			CMDQ_REG_SET32(MDP_TDSHP0_BASE + 0x100, 0x2);
			CMDQ_REG_SET32(MDP_TDSHP0_BASE + 0x100, 0x0);
			CMDQ_MSG("Disable MDP_TDSHP0 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_TDSHP0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_TDSHP1)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_TDSHP1)) {
			CMDQ_REG_SET32(MDP_TDSHP1_BASE + 0x100, 0x0);
			CMDQ_REG_SET32(MDP_TDSHP1_BASE + 0x100, 0x2);
			CMDQ_REG_SET32(MDP_TDSHP1_BASE + 0x100, 0x0);
			CMDQ_MSG("Disable MDP_TDSHP1 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_TDSHP1);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RSZ0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_RSZ0)) {
			CMDQ_REG_SET32(MDP_RSZ0_BASE, 0x0);
			CMDQ_REG_SET32(MDP_RSZ0_BASE, 0x10000);
			CMDQ_REG_SET32(MDP_RSZ0_BASE, 0x0);

			CMDQ_MSG("Disable MDP_RSZ0 clock\n");

			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_RSZ0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RSZ1)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_RSZ1)) {
			CMDQ_REG_SET32(MDP_RSZ1_BASE, 0x0);
			CMDQ_REG_SET32(MDP_RSZ1_BASE, 0x10000);
			CMDQ_REG_SET32(MDP_RSZ1_BASE, 0x0);

			CMDQ_MSG("Disable MDP_RSZ1 clock\n");

			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_RSZ1);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RDMA0)) {
		cmdq_mdp_get_func()->enableMdpClock(false, CMDQ_ENG_MDP_RDMA0);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RDMA1)) {
		cmdq_mdp_get_func()->enableMdpClock(false, CMDQ_ENG_MDP_RDMA1);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_COLOR0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_COLOR0)) {
			CMDQ_MSG("Disable MDP_COLOR0 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_COLOR0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_COLOR1)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_COLOR1)) {
			CMDQ_MSG("Disable MDP_COLOR1 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_COLOR1);
		}
	}

	/* AAL */
	if (engineFlag & (1LL << CMDQ_ENG_MDP_AAL0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_AAL0)) {
			CMDQ_MSG("Disable MDP_AAL0 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_AAL0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_AAL1)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_AAL1)) {
			CMDQ_MSG("Disable MDP_AAL1 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_AAL1);
		}
	}

	/* HDR */
	if (engineFlag & (1LL << CMDQ_ENG_MDP_HDR0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_HDR0)) {
			CMDQ_MSG("Disable MDP_HDR0 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_HDR0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_HDR1)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_HDR1)) {
			CMDQ_MSG("Disable MDP_HDR1 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_HDR1);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_FG0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_FG0)) {
			CMDQ_MSG("Disable MDP_FG0 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_FG0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_FG1)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_FG1)) {
			CMDQ_MSG("Disable MDP_FG1 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_FG1);
		}
	}

	CMDQ_MSG("%s: Disable MDP(0x%llx) clock end\n", __func__, engineFlag);
#endif				/* #ifdef CMDQ_PWR_AWARE */

	return 0;
}

static s32 mdp_is_mod_suspend(struct EngineStruct *engine_list)
{
	s32 status = 0;
	int i;
	enum CMDQ_ENG_ENUM e = 0;

	u32 non_suspend_engine[] = {
		CMDQ_ENG_MDP_RDMA0,
		CMDQ_ENG_MDP_RDMA1,
		CMDQ_ENG_MDP_RSZ0,
		CMDQ_ENG_MDP_RSZ1,
		CMDQ_ENG_MDP_TDSHP0,
		CMDQ_ENG_MDP_TDSHP1,
		CMDQ_ENG_MDP_COLOR0,
		CMDQ_ENG_MDP_COLOR1,
		CMDQ_ENG_MDP_WROT0,
		CMDQ_ENG_MDP_WROT1,
	};

	for (i = 0; i < ARRAY_SIZE(non_suspend_engine); i++) {
		e = non_suspend_engine[i];
		if (engine_list[e].userCount != 0) {
			CMDQ_ERR(
				"suspend but engine:%d count:%d owner:%d\n",
				e, engine_list[e].userCount,
				engine_list[e].currOwner);
			status = -EBUSY;
		}
	}

	return status;
}

static s32 mdp_dump_engine_usage(struct EngineStruct *engine_list)
{
	struct EngineStruct *engine;
	const u32 engine_enum[] =
		CMDQ_FOREACH_MODULE_PRINT(GENERATE_ENUM);
	static const char *const engine_names[] =
		CMDQ_FOREACH_MODULE_PRINT(GENERATE_STRING);
	u32 i;

	CMDQ_ERR("====== Engine Usage =======\n");
	for (i = 0; i < ARRAY_SIZE(engine_enum); i++) {
		engine = &engine_list[engine_enum[i]];
		if (engine->userCount ||
			engine->currOwner != CMDQ_INVALID_THREAD ||
			engine->failCount || engine->resetCount)
			CMDQ_ERR("%s: count:%d owner:%d fail:%d reset:%d\n",
				engine_names[i], engine->userCount,
				engine->currOwner, engine->failCount,
				engine->resetCount);
	}

	return 0;
}

static bool mdp_is_mtee(struct cmdqRecStruct *handle)
{
#ifdef CMDQ_ENG_MTEE_GROUP_BITS
	return bool(handle->engineFlag & CMDQ_ENG_MTEE_GROUP_BITS);
#else
	return false;
#endif
}

struct device *mdp_init_larb(struct platform_device *pdev, u8 idx)
{
	struct device_node *node;
	struct platform_device *larb_pdev;

	/* get larb node from dts */
	node = of_parse_phandle(pdev->dev.of_node, "mediatek,larb", idx);
	if (!node) {
		CMDQ_ERR("%s fail to parse mediatek,larb\n", __func__);
		return NULL;
	}

	larb_pdev = of_find_device_by_node(node);
	if (WARN_ON(!larb_pdev)) {
		of_node_put(node);
		CMDQ_ERR("%s no larb for idx %hhu\n", __func__, idx);
		return NULL;
	}
	of_node_put(node);

	CMDQ_LOG("%s pdev %p idx %hhu\n", __func__, pdev, idx);

	return &larb_pdev->dev;
}

void cmdqMdpInitialSetting(struct platform_device *pdev)
{
#ifdef MDP_IOMMU_DEBUG
	CMDQ_LOG("[MDP] %s\n", __func__);

	/* Register ION Translation Fault function */
	mtk_iommu_register_fault_callback(M4U_LARB2_PORT0,
		cmdq_TranslationFault_callback, (void *)pdev, false);
	mtk_iommu_register_fault_callback(M4U_LARB2_PORT1,
		cmdq_TranslationFault_callback, (void *)pdev, false);
	mtk_iommu_register_fault_callback(M4U_LARB2_PORT2,
		cmdq_TranslationFault_callback, (void *)pdev, false);
	mtk_iommu_register_fault_callback(M4U_LARB2_PORT3,
		cmdq_TranslationFault_callback, (void *)pdev, false);
#endif

	/* must porting in dts */
	larb2 = mdp_init_larb(pdev, 0);

	/* Query vcp pq readback setting in dts */
	gVcpPQReadbackSupport = of_property_read_bool(pdev->dev.of_node, "vcp_pq_readback");

}

uint32_t cmdq_mdp_rdma_get_reg_offset_src_addr(void)
{
	return 0xF00;
}

uint32_t cmdq_mdp_wrot_get_reg_offset_dst_addr(void)
{
	return 0xF00;
}

uint32_t cmdq_mdp_wdma_get_reg_offset_dst_addr(void)
{
	return 0xF00;
}

const char *cmdq_mdp_parse_error_module(const struct cmdqRecStruct *task)
{
	const char *module = NULL;

	/* common part for both normal and secure path */

	/* for secure path, use HW flag is sufficient */
	do {
		if (!task->secData.is_secure) {
			/* normal path,
			 * need parse current running instruciton
			 * for more detail
			 */
			break;
		} else if (CMDQ_ENG_MDP_GROUP_FLAG(task->engineFlag)) {
			module = "MDP";
			break;
		}

		module = "CMDQ";
	} while (0);

	return module;
}

u64 cmdq_mdp_get_engine_group_bits(u32 engine_group)
{
	return gCmdqEngineGroupBits[engine_group];
}

static void mdp_enable_larb(bool enable, struct device *larb)
{
#if IS_ENABLED(CONFIG_MTK_SMI)
	if (!larb) {
		CMDQ_ERR("%s smi larb not support\n", __func__);
		return;
	}

	if (enable) {
		int ret = mtk_smi_larb_get(larb);

		cmdq_mdp_enable_clock_APB(enable);
		cmdq_mdp_enable_clock_MDP_MUTEX0(enable);

		if (ret)
			CMDQ_ERR("%s enable fail ret:%d\n",
				__func__, ret);
	} else {
		/* disable, reverse the sequence */
		cmdq_mdp_enable_clock_MDP_MUTEX0(enable);
		cmdq_mdp_enable_clock_APB(enable);
		mtk_smi_larb_put(larb);
	}


	if (gVcpPQReadbackSupport) {
		if (enable)
			cmdq_vcp_enable(true);
		else
			cmdq_vcp_enable(false);
	}

#endif
}

static void cmdq_mdp_enable_common_clock(bool enable, u64 engine_flag)
{
	if (engine_flag & MDP_ENG_LARB2)
		mdp_enable_larb(enable, larb2);
}

static void cmdq_mdp_check_hw_status(struct cmdqRecStruct *handle)
{
}

#ifdef CMDQ_SECURE_PATH_SUPPORT
#define CMDQ_ENGINE_TRANS(eng_flags, eng_flags_sec, ENGINE) \
	do {	\
		if ((1LL << CMDQ_ENG_##ENGINE) & (eng_flags)) \
			(eng_flags_sec) |= (1LL << CMDQ_SEC_##ENGINE); \
	} while (0)

u64 cmdq_mdp_get_secure_engine(u64 engine_flags)
{
	u64 sec_eng_flag = 0;

	/* MDP engines */
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, MDP_RDMA0);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, MDP_RDMA1);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, MDP_WROT0);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, MDP_WROT1);

	return sec_eng_flag;
}
#endif

static u32 cmdq_mdp_qos_translate_port(u32 engine_id)
{
	switch (engine_id) {
	case CMDQ_ENG_MDP_RDMA0:
		return M4U_LARB2_PORT0;
	case CMDQ_ENG_MDP_RDMA1:
		return M4U_LARB2_PORT1;
	case CMDQ_ENG_MDP_WROT0:
		return M4U_LARB2_PORT2;
	case CMDQ_ENG_MDP_WROT1:
		return M4U_LARB2_PORT3;
	}

	return 0;
}

#define MDP_ICC_GET(port) do { \
	path_##port[thread_id] = of_mtk_icc_get(dev, #port);		\
	if (!path_##port[thread_id])					\
		CMDQ_ERR("%s port:%s icc fail\n", __func__, #port);	\
} while (0)

static void mdp_qos_init(struct platform_device *pdev, u32 thread_id)
{
	struct device *dev = &pdev->dev;

	CMDQ_LOG("%s thread %u\n", __func__, thread_id);

	MDP_ICC_GET(mdp_rdma0);
	MDP_ICC_GET(mdp_rdma1);
	MDP_ICC_GET(mdp_wrot0);
	MDP_ICC_GET(mdp_wrot1);
}

static void *mdp_qos_get_path(u32 thread_id, u32 port)
{
	if (!port)
		return NULL;

	switch (port) {
	/* mdp part */
	case M4U_LARB2_PORT0:
		return path_mdp_rdma0[thread_id];
	case M4U_LARB2_PORT1:
		return path_mdp_rdma1[thread_id];
	case M4U_LARB2_PORT2:
		return path_mdp_wrot0[thread_id];
	case M4U_LARB2_PORT3:
		return path_mdp_wrot1[thread_id];
	}

	CMDQ_ERR("%s pmqos invalid port %d\n", __func__, port);
	return NULL;
}

static void mdp_qos_clear_all(u32 thread_id)
{
	mtk_icc_set_bw(path_mdp_rdma0[thread_id], 0, 0);
	mtk_icc_set_bw(path_mdp_rdma1[thread_id], 0, 0);
	mtk_icc_set_bw(path_mdp_wrot0[thread_id], 0, 0);
	mtk_icc_set_bw(path_mdp_wrot1[thread_id], 0, 0);
}

static u32 mdp_get_group_max(void)
{
	return CMDQ_MAX_GROUP_COUNT;
}

static u32 mdp_get_group_mdp(void)
{
	return CMDQ_GROUP_MDP;
}

static const char **const mdp_get_engine_group_name(void)
{
	static const char *const engineGroupName[] = {
		CMDQ_FOREACH_GROUP(GENERATE_STRING)
	};

	return (const char **const)engineGroupName;
}

phys_addr_t *mdp_engine_base_get(void)
{
	return (phys_addr_t *)mdp_base;
}

u32 mdp_engine_base_count(void)
{
	return (u32)ENGBASE_COUNT;
}

static void mdp_readback_aal_by_engine(struct cmdqRecStruct *handle,
	u16 engine, dma_addr_t pa, u32 param)
{
	phys_addr_t base;
	u32 pipe;

	switch (engine) {
	case CMDQ_ENG_MDP_AAL0:
		base = mdp_module_pa.aal0;
		pipe = 0;
		break;
	case CMDQ_ENG_MDP_AAL1:
		base = mdp_module_pa.aal1;
		pipe = 1;
		break;
	default:
		CMDQ_ERR("%s not support\n", __func__);
		return;
	}

	cmdq_mdp_get_func()->mdpReadbackAal(handle, engine, base, pa, param, pipe);
}

static void mdp_readback_hdr_by_engine(struct cmdqRecStruct *handle,
	u16 engine, dma_addr_t pa, u32 param)
{
	phys_addr_t base;
	u32 pipe;

	switch (engine) {
	case CMDQ_ENG_MDP_HDR0:
		base = mdp_module_pa.hdr0;
		pipe = 0;
		break;
	case CMDQ_ENG_MDP_HDR1:
		base = mdp_module_pa.hdr1;
		pipe = 1;
		break;
	default:
		CMDQ_ERR("%s not support\n", __func__);
		return;
	}

	cmdq_mdp_get_func()->mdpReadbackHdr(handle, engine, base, pa, param, pipe);
}

void cmdq_mdp_compose_readback(struct cmdqRecStruct *handle,
	u16 engine, dma_addr_t addr, u32 param)
{
	switch (engine) {
	case CMDQ_ENG_MDP_AAL0:
	case CMDQ_ENG_MDP_AAL1:
		mdp_readback_aal_by_engine(handle, engine, addr, param);
		break;
	case CMDQ_ENG_MDP_HDR0:
	case CMDQ_ENG_MDP_HDR1:
		mdp_readback_hdr_by_engine(handle, engine, addr, param);
		break;
	default:
		CMDQ_ERR("%s engine not support:%hu\n", __func__, engine);
		break;
	}
}

static s32 mdp_get_rdma_idx(u32 eng_base)
{
	s32 rdma_idx = -1;

	switch (eng_base) {
	case ENGBASE_MDP_RDMA0:
		rdma_idx = 0;
		break;
	case ENGBASE_MDP_RDMA1:
		rdma_idx = 1;
		break;
	default:
		CMDQ_ERR("%s engine not support:%d\n", __func__, eng_base);
		break;
	}

	return rdma_idx;
}

static u16 mdp_get_reg_msb_offset(u32 eng_base, u16 offset)
{
	u16 reg_msb_offset = 0x0;

	if ((eng_base == ENGBASE_MDP_RDMA0) || (eng_base == ENGBASE_MDP_RDMA1)) {
		if (offset == 0xF00)
			reg_msb_offset = 0xF30;
		else if (offset == 0xF08)
			reg_msb_offset = 0xF34;
		else if (offset == 0xF10)
			reg_msb_offset = 0xF38;
		else if (offset == 0xF20)
			reg_msb_offset = 0xF3C;
		else if (offset == 0xF28)
			reg_msb_offset = 0xF40;
		else
			CMDQ_ERR("%s offset not support:0x%x\n", __func__, offset);

	} else if ((eng_base == ENGBASE_MDP_WROT0) || (eng_base == ENGBASE_MDP_WROT1)) {
		if (offset == 0xF00)
			reg_msb_offset = 0xF34;
		else if (offset == 0xF04)
			reg_msb_offset = 0xF38;
		else if (offset == 0xF08)
			reg_msb_offset = 0xF3C;
		else
			CMDQ_ERR("%s offset not support:0x%x\n", __func__, offset);
	} else {
		CMDQ_ERR("%s engine not support:%d\n", __func__, eng_base);
	}

	CMDQ_MSG("%s enginet:%d, reg_msb_offset:0x%x\n", __func__, eng_base, reg_msb_offset);

	return reg_msb_offset;
}

static bool mdp_check_camin_support_virtual(void)
{
	return false;
}


static bool mdp_vcp_pq_readback_support(void)
{
	return gVcpPQReadbackSupport;
}

void mdp_vcp_pq_readback_impl(struct cmdqRecStruct *handle,
	u16 engine, u32 vcp_offset, u32 count)
{
	switch (engine) {
	case CMDQ_ENG_MDP_AAL0:
		cmdq_pkt_readback(handle->pkt, CMDQ_VCP_ENG_MDP_AAL0,
			vcp_offset, count, CMDQ_GPR_R12, NULL, NULL);
		break;
	case CMDQ_ENG_MDP_AAL1:
		cmdq_pkt_readback(handle->pkt, CMDQ_VCP_ENG_MDP_AAL1,
			vcp_offset, count, CMDQ_GPR_R14, NULL, NULL);
		break;
	case CMDQ_ENG_MDP_HDR0:
		cmdq_pkt_readback(handle->pkt, CMDQ_VCP_ENG_MDP_HDR0,
			vcp_offset, count, CMDQ_GPR_R12, NULL, NULL);
		break;
	case CMDQ_ENG_MDP_HDR1:
		cmdq_pkt_readback(handle->pkt, CMDQ_VCP_ENG_MDP_HDR1,
			vcp_offset, count, CMDQ_GPR_R14, NULL, NULL);
		break;
	default:
		CMDQ_ERR("%s engine not support:%hu\n", __func__, engine);
		break;
	}

}

void cmdq_mdp_platform_function_setting(void)
{
	struct cmdqMDPFuncStruct *pFunc = cmdq_mdp_get_func();

	pFunc->dumpMMSYSConfig = cmdq_mdp_dump_mmsys_config;

	pFunc->initModuleBaseVA = cmdq_mdp_init_module_base_VA;
	pFunc->deinitModuleBaseVA = cmdq_mdp_deinit_module_base_VA;

	pFunc->mdpClockIsOn = cmdq_mdp_clock_is_on;
	pFunc->enableMdpClock = cmdq_mdp_enable_clock;
	pFunc->initModuleCLK = cmdq_mdp_init_module_clk;
	pFunc->mdpDumpRsz = cmdq_mdp_dump_rsz;
	pFunc->mdpDumpTdshp = cmdq_mdp_dump_tdshp;
	pFunc->mdpClockOn = cmdqMdpClockOn;
	pFunc->mdpDumpInfo = cmdqMdpDumpInfo;
	pFunc->mdpResetEng = cmdqMdpResetEng;
	pFunc->mdpClockOff = cmdqMdpClockOff;
	pFunc->mdpIsModuleSuspend = mdp_is_mod_suspend;
	pFunc->mdpDumpEngineUsage = mdp_dump_engine_usage;

	pFunc->mdpIsMtee = mdp_is_mtee;
	pFunc->mdpInitialSet = cmdqMdpInitialSetting;

	pFunc->rdmaGetRegOffsetSrcAddr = cmdq_mdp_rdma_get_reg_offset_src_addr;
	pFunc->wrotGetRegOffsetDstAddr = cmdq_mdp_wrot_get_reg_offset_dst_addr;
	pFunc->wdmaGetRegOffsetDstAddr = cmdq_mdp_wdma_get_reg_offset_dst_addr;
	pFunc->parseErrModByEngFlag = cmdq_mdp_parse_error_module;
	// pFunc->parseHandleErrModByEngFlag = cmdq_mdp_parse_handle_error_module;
	pFunc->getEngineGroupBits = cmdq_mdp_get_engine_group_bits;
	pFunc->mdpEnableCommonClock = cmdq_mdp_enable_common_clock;
	pFunc->CheckHwStatus = cmdq_mdp_check_hw_status;
#ifdef CMDQ_SECURE_PATH_SUPPORT
	pFunc->mdpGetSecEngine = cmdq_mdp_get_secure_engine;
#endif
	// pFunc->resolve_token = cmdq_mdp_resolve_token;
	pFunc->qosTransPort = cmdq_mdp_qos_translate_port;
	pFunc->qosInit = mdp_qos_init;
	pFunc->qosGetPath = mdp_qos_get_path;
	pFunc->qosClearAll = mdp_qos_clear_all;
	pFunc->getGroupMax = mdp_get_group_max;
	pFunc->getGroupMdp = mdp_get_group_mdp;
	pFunc->getEngineBase = mdp_engine_base_get;
	pFunc->getEngineBaseCount = mdp_engine_base_count;
	pFunc->getEngineGroupName = mdp_get_engine_group_name;
	pFunc->mdpComposeReadback = cmdq_mdp_compose_readback;
	pFunc->getRDMAIndex = mdp_get_rdma_idx;
	pFunc->getRegMSBOffset = mdp_get_reg_msb_offset;
	pFunc->mdpIsCaminSupport = mdp_check_camin_support_virtual;
	pFunc->mdpVcpPQReadbackSupport = mdp_vcp_pq_readback_support;
	pFunc->mdpVcpPQReadback = mdp_vcp_pq_readback_impl;

}

MODULE_LICENSE("GPL");

