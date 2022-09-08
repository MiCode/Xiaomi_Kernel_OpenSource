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

//#undef MTK_M4U_ID
#include <dt-bindings/memory/mt6768-larb-port.h>
//#include <linux/interconnect-provider.h>
#include "mtk-interconnect.h"
#include <soc/mediatek/smi.h>

#include "mdp_engine_mt6768.h"
#include "mdp_base_mt6768.h"
#include <cmdq-util.h>

/* iommu larbs */
struct device *larb0;
/* support RDMA prebuilt access */
int gCmdqRdmaPrebuiltSupport;
/* support register MSB */
int gMdpRegMSBSupport;
static atomic_t mdp_smi_clk_usage;

/* use to generate [CMDQ_ENGINE_ENUM_id and name] mapping for status print */
#define CMDQ_FOREACH_MODULE_PRINT(ACTION)\
{		\
ACTION(CMDQ_ENG_ISP_IMGI,   ISP_IMGI)	\
ACTION(CMDQ_ENG_MDP_RDMA0,  MDP_RDMA0)	\
ACTION(CMDQ_ENG_MDP_RSZ0,   MDP_RSZ0)	\
ACTION(CMDQ_ENG_MDP_RSZ1,   MDP_RSZ1)	\
ACTION(CMDQ_ENG_MDP_TDSHP0, MDP_TDSHP0)	\
ACTION(CMDQ_ENG_MDP_WROT0,  MDP_WROT0)	\
ACTION(CMDQ_ENG_MDP_WDMA,  MDP_WDMA)	\
}

/* mdp */
static struct icc_path *path_mdp_rdma0[MDP_TOTAL_THREAD];
static struct icc_path *path_mdp_wrot0[MDP_TOTAL_THREAD];
static struct icc_path *path_mdp_wdma[MDP_TOTAL_THREAD];

/* isp can be ignored GKI2.0 because isp did not attend */
/*
 * static struct icc_path *path_imgi[MDP_TOTAL_THREAD];
 * static struct icc_path *path_imgci[MDP_TOTAL_THREAD];
 * static struct icc_path *path_ufdi[MDP_TOTAL_THREAD];
 * static struct icc_path *path_ufocw[MDP_TOTAL_THREAD];
 * static struct icc_path *path_lci[MDP_TOTAL_THREAD];
 * static struct icc_path *path_dmgi[MDP_TOTAL_THREAD];
 * static struct icc_path *path_ufoc2r[MDP_TOTAL_THREAD];
 * static struct icc_path *path_crzo[MDP_TOTAL_THREAD];
 * static struct icc_path *path_ufoyw[MDP_TOTAL_THREAD];
 * static struct icc_path *path_smti1[MDP_TOTAL_THREAD];
 * static struct icc_path *path_smto1[MDP_TOTAL_THREAD];
 * static struct icc_path *path_smto2[MDP_TOTAL_THREAD];
 */

#include "cmdq_device.h"
struct CmdqMdpModuleBaseVA {
	long MDP_RDMA0;
	long MDP_CCORR;
	long MDP_RSZ0;
	long MDP_RSZ1;
	long MDP_TDSHP;
	long MDP_COLOR;
	long MDP_WROT0;
	long MDP_WDMA;
	long VENC;
	long MM_MUTEX;
};
static struct CmdqMdpModuleBaseVA gCmdqMdpModuleBaseVA;

struct CmdqMdpModuleClock {
	struct clk *clk_CAM_MDP;
	struct clk *clk_IMG_DL_RELAY;
	struct clk *clk_IMG_DL_ASYNC_TOP;
	struct clk *clk_MDP_RDMA0;
	struct clk *clk_MDP_RSZ0;
	struct clk *clk_MDP_RSZ1;
	struct clk *clk_MDP_WDMA;
	struct clk *clk_MDP_WROT0;
	struct clk *clk_MDP_TDSHP;
	struct clk *clk_MDP_COLOR;
	struct clk *clk_MDP_CCORR;
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

IMP_ENABLE_MDP_HW_CLOCK(CAM_MDP, CAM_MDP);
IMP_ENABLE_MDP_HW_CLOCK(IMG_DL_RELAY, IMG_DL_RELAY);
IMP_ENABLE_MDP_HW_CLOCK(IMG_DL_ASYNC_TOP, IMG_DL_ASYNC_TOP);
IMP_ENABLE_MDP_HW_CLOCK(MDP_RDMA0, MDP_RDMA0);
IMP_ENABLE_MDP_HW_CLOCK(MDP_RSZ0, MDP_RSZ0);
IMP_ENABLE_MDP_HW_CLOCK(MDP_RSZ1, MDP_RSZ1);
IMP_ENABLE_MDP_HW_CLOCK(MDP_CCORR, MDP_CCORR);
IMP_ENABLE_MDP_HW_CLOCK(MDP_WDMA, MDP_WDMA);
IMP_ENABLE_MDP_HW_CLOCK(MDP_WROT0, MDP_WROT0);
IMP_ENABLE_MDP_HW_CLOCK(MDP_TDSHP0, MDP_TDSHP);
IMP_ENABLE_MDP_HW_CLOCK(MDP_COLOR0, MDP_COLOR);
IMP_MDP_HW_CLOCK_IS_ENABLE(CAM_MDP, CAM_MDP);
IMP_MDP_HW_CLOCK_IS_ENABLE(IMG_DL_RELAY, IMG_DL_RELAY);
IMP_MDP_HW_CLOCK_IS_ENABLE(IMG_DL_ASYNC_TOP, IMG_DL_ASYNC_TOP);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_RDMA0, MDP_RDMA0);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_RSZ0, MDP_RSZ0);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_RSZ1, MDP_RSZ1);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_CCORR, MDP_CCORR);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_WDMA, MDP_WDMA);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_WROT0, MDP_WROT0);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_TDSHP0, MDP_TDSHP);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_COLOR0, MDP_COLOR);
#undef IMP_ENABLE_MDP_HW_CLOCK
#undef IMP_MDP_HW_CLOCK_IS_ENABLE

static const uint64_t gCmdqEngineGroupBits[CMDQ_MAX_GROUP_COUNT] = {
	CMDQ_ENG_ISP_GROUP_BITS,
	CMDQ_ENG_MDP_GROUP_BITS,
	CMDQ_ENG_DISP_GROUP_BITS,
	CMDQ_ENG_JPEG_GROUP_BITS,
	CMDQ_ENG_VENC_GROUP_BITS,
	CMDQ_ENG_DPE_GROUP_BITS,
	CMDQ_ENG_RSC_GROUP_BITS,
	CMDQ_ENG_GEPF_GROUP_BITS,
	CMDQ_ENG_EAF_GROUP_BITS
};


long cmdq_mdp_get_module_base_VA_MDP_RDMA0(void)
{
	return gCmdqMdpModuleBaseVA.MDP_RDMA0;
}

long cmdq_mdp_get_module_base_VA_MDP_RSZ0(void)
{
	return gCmdqMdpModuleBaseVA.MDP_RSZ0;
}

long cmdq_mdp_get_module_base_VA_MDP_RSZ1(void)
{
	return gCmdqMdpModuleBaseVA.MDP_RSZ1;
}

long cmdq_mdp_get_module_base_VA_MDP_TDSHP(void)
{
	return gCmdqMdpModuleBaseVA.MDP_TDSHP;
}

long cmdq_mdp_get_module_base_VA_MDP_COLOR(void)
{
	return gCmdqMdpModuleBaseVA.MDP_COLOR;
}

long cmdq_mdp_get_module_base_VA_MDP_CCORR(void)
{
	return gCmdqMdpModuleBaseVA.MDP_CCORR;
}

long cmdq_mdp_get_module_base_VA_MDP_WROT0(void)
{
	return gCmdqMdpModuleBaseVA.MDP_WROT0;
}

long cmdq_mdp_get_module_base_VA_MDP_WDMA(void)
{
	return gCmdqMdpModuleBaseVA.MDP_WDMA;
}

long cmdq_mdp_get_module_base_VA_VENC(void)
{
	return gCmdqMdpModuleBaseVA.VENC;
}

long cmdq_mdp_get_module_base_VA_MM_MUTEX(void)
{
	return gCmdqMdpModuleBaseVA.MM_MUTEX;
}

#define MMSYS_CONFIG_BASE	cmdq_mdp_get_module_base_VA_MMSYS_CONFIG()
#define MDP_RDMA0_BASE		cmdq_mdp_get_module_base_VA_MDP_RDMA0()
#define MDP_CCORR_BASE		cmdq_mdp_get_module_base_VA_MDP_CCORR()
#define MDP_RSZ0_BASE		cmdq_mdp_get_module_base_VA_MDP_RSZ0()
#define MDP_RSZ1_BASE		cmdq_mdp_get_module_base_VA_MDP_RSZ1()
#define MDP_TDSHP_BASE		cmdq_mdp_get_module_base_VA_MDP_TDSHP()
#define MDP_COLOR_BASE		cmdq_mdp_get_module_base_VA_MDP_COLOR()
#define MDP_WROT0_BASE		cmdq_mdp_get_module_base_VA_MDP_WROT0()
#define MDP_WDMA_BASE		cmdq_mdp_get_module_base_VA_MDP_WDMA()
#define VENC_BASE			cmdq_mdp_get_module_base_VA_VENC()
#define MM_MUTEX_BASE		cmdq_mdp_get_module_base_VA_MM_MUTEX()

struct RegDef {
	int offset;
	const char *name;
};

void cmdq_mdp_dump_mmsys_config(const struct cmdqRecStruct *handle)
{
	int i = 0;
	uint32_t value = 0;

	static const struct RegDef configRegisters[] = {
		{0x100, "MMSYS Clock Gating Config_0"},
		{0x110, "MMSYS Clock Gating Config_1"},
		{0xF04, "ISP_MOUT_EN"},
		{0xF08, "MDP_RDMA0_MOUT_EN"},
		{0xF0C, "MDP_CCORR_MOUT_EN"},
		{0xF10, "MDP_PRZ0_MOUT_EN"},
		{0xF14, "MDP_PRZ1_MOUT_EN"},
		{0xF18, "MDP_TDSHP_SOUT_SEL"},
		{0xF1C, "MDP_COLOR_MOUT_EN"},
		{0xF20, "MDP_CCORR_SEL_IN"},
		{0xF24, "MDP_PRZ0_SEL_IN"},
		{0xF28, "MDP_PRZ1_SEL_IN"},
		{0xF2C, "MDP_TDSHP_SEL_IN"},
		{0xF30, "MDP_COLOR_OUT_SEL_IN"},
		{0xF34, "MDP_WDMA_SEL_IN"},
		{0xF38, "MDP_WROT0_SEL_IN"},
		{0xFA0, "MMSYS_MOUT_MASK0"},
		{0xFA4, "MMSYS_MOUT_MASK1"},
		{0xFB0, "MDP_DL_VALID_0"},
		{0xFB4, "MDP_DL_VALID_1"},
		{0xFB8, "MDP_DL_VALID_2"},
		{0xFC0, "MDP_DL_READY_0"},
		{0xFC4, "MDP_DL_READY_1"},
		{0xFC8, "MDP_DL_READY_2"},
		{0x934, "MDP_ASYNC_CFG_WD"},
		{0x938, "MDP_DL_CFG_RD"},
		{0x940, "MDP_DL_ASYNC_CFG_RD0"},
		{0x94C, "MDP_DL_ASYNC_CFG_RD1"},
		/*{0xF24, "COLOR0_SEL_IN"},*/
		/*{0xF18, "MDP_TDSHP_MOUT_EN"},*/
		/*{0xF0C, "DISP_OVL0_MOUT_EN"},*/
		/*{0xF04, "DISP_OVL0_2L_MOUT_EN"},*/
		/*{0xF08, "DISP_OVL1_2L_MOUT_EN"},*/
		/*{0xF4C, "DISP_DITHER0_MOUT_EN"},*/
		/*{0xF10, "DISP_RSZ_MOUT_EN"},*/
		/* {0x040, "DISP_UFOE_MOUT_EN"}, */
		/* {0x040, "MMSYS_MOUT_RST"}, */
		/*{0xFA0, "DISP_TO_WROT_SOUT_SEL"},*/
		/*{0xFA4, "MDP_COLOR_IN_SOUT_SEL"},*/
		/*{0xFB0, "MDP_TDSHP_SOUT_SEL"},*/
		{0xF6C, "DISP_WDMA0_SEL_IN"},
		/*{0xFDC, "MDP_COLOR_SEL_IN"},*/
		/*{0xF20, "DISP_COLOR_OUT_SEL_IN"},*/
		/*{0xFD8, "MDP_COLOR_OUT_SEL_IN"},*/
		/*{0xFDC, "MDP_COLOR_SEL_IN "},*/
		/* {0xFDC, "DISP_COLOR_SEL_IN"},*/
		/*{0xFE0, "MDP_PATH0_SEL_IN"},*/
		/*{0xFE4, "MDP_PATH1_SEL_IN"},*/
		/*{0x070, "DISP_WDMA1_SEL_IN"},*/
		/*{0x074, "DISP_UFOE_SEL_IN"},*/
		{0xF68, "DSI0_SEL_IN"},
		/*{0xF30, "DPI1_SEL_IN"},*/
		/*{0xF50, "DISP_RDMA0_SOUT_SEL_IN"},*/
		/*{0xF54, "DISP_RDMA1_SOUT_SEL_IN"},*/
		{0x0F0, "MMSYS_MISC"}
		/* ACK and REQ related */
		/*{0xF58, "DISP_DL_VALID_0"},*/
		/*{0xF5C, "DISP_DL_VALID_1"},*/
		/*{0xF60, "DISP_DL_READY_0"},*/
		/*{0xF64, "DISP_DL_READY_1"},*/
	};
	if (!(handle->engineFlag & CMDQ_ENG_MDP_GROUP_BITS))
		return;

	CMDQ_ERR("==========[CMDQ] MMSYS_CONFIG ==========\n");

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
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x0D0);
	CMDQ_ERR("%s: 0x%08x\n", "DISP_MUTEX5_MOD0", value);
}

int32_t cmdq_mdp_reset_with_mmsys(const uint64_t engineToResetAgain)
{
	long MMSYS_SW0_RST_B_REG = MMSYS_CONFIG_BASE + (0x140);

	int i = 0;
	uint32_t reset_bits = 0L;
	int engineResetBit[32] = {
		CMDQ_ENG_MDP_RDMA0,	/* bit  0 : MDP_RDMA0 */
		CMDQ_ENG_MDP_CCORR0,	/* bit  1 : MDP_CCORR0 */
		CMDQ_ENG_MDP_RSZ0,	/* bit  2 : MDP_RSZ0  */
		CMDQ_ENG_MDP_RSZ1,	/* bit  3 : MDP_RSZ1 */
		CMDQ_ENG_MDP_TDSHP0, /* bit  4 : MDP_TDSHP0 */
		CMDQ_ENG_MDP_WROT0,	 /* bit  5 : MDP_WROT0 */
		CMDQ_ENG_MDP_WDMA,	/* bit  6 : MDP_WDMA0 */
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,	/* bit  12 : COLOR0 */
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		CMDQ_ENG_MDP_CAMIN,	/* bit  23 : CAM_MDP */
		[24 ... 31] = -1
	};

	for (i = 0; i < 32; ++i) {
		if (engineResetBit[i] < 0)
			continue;

		if (engineToResetAgain & (1LL << engineResetBit[i]))
			reset_bits |= (1 << i);
	}

	if (reset_bits != 0) {
		/* 0: reset */
		/* 1: not reset */
		/* so we need to reverse the bits */
		reset_bits = ~reset_bits;

		CMDQ_REG_SET32(MMSYS_SW0_RST_B_REG, reset_bits);
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
		"=============== [MDP] Frame Information Begin ====================================\n");
	/* find dispatch module and assign dispatch key */
	cmdq_mdp_check_TF_address(mva, dispatchModel);
	CMDQ_ERR(
		"=============== [MDP] Frame Information End ====================================\n");
	CMDQ_ERR("================= [MDP M4U] Dump End ================\n");

	return 0;
}
#endif

int32_t cmdqVEncDumpInfo(uint64_t engineFlag, int logLevel)
{
	if (engineFlag & (1LL << CMDQ_ENG_VIDEO_ENC))
		cmdq_mdp_dump_venc(VENC_BASE, "VENC");

	return 0;
}

void cmdq_mdp_init_module_base_VA(void)
{
	memset(&gCmdqMdpModuleBaseVA, 0, sizeof(struct CmdqMdpModuleBaseVA));

	gCmdqMdpModuleBaseVA.MDP_RDMA0 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_rdma0");
	gCmdqMdpModuleBaseVA.MDP_RSZ0 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_rsz0");
	gCmdqMdpModuleBaseVA.MDP_RSZ1 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_rsz1");
	gCmdqMdpModuleBaseVA.MDP_WDMA =
		cmdq_dev_alloc_reference_VA_by_name("mdp_wdma0");
	gCmdqMdpModuleBaseVA.MDP_WROT0 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_wrot0");
	gCmdqMdpModuleBaseVA.MDP_TDSHP =
		cmdq_dev_alloc_reference_VA_by_name("mdp_tdshp0");
	gCmdqMdpModuleBaseVA.MDP_COLOR =
		cmdq_dev_alloc_reference_VA_by_name("mdp_color0");
	gCmdqMdpModuleBaseVA.MDP_CCORR =
		cmdq_dev_alloc_reference_VA_by_name("mdp_ccorr0");
	gCmdqMdpModuleBaseVA.VENC =
		cmdq_dev_alloc_reference_VA_by_name("venc");
	gCmdqMdpModuleBaseVA.MM_MUTEX =
		cmdq_dev_alloc_reference_VA_by_name("mm_mutex");
}

void cmdq_mdp_deinit_module_base_VA(void)
{
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_RDMA0());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_RSZ0());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_RSZ1());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_WDMA());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_WROT0());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_TDSHP());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_COLOR());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_CCORR());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_VENC());

	memset(&gCmdqMdpModuleBaseVA, 0, sizeof(struct CmdqMdpModuleBaseVA));
}

bool cmdq_mdp_clock_is_on(u32 engine)
{
	switch (engine) {
	case CMDQ_ENG_MDP_CAMIN:
		return cmdq_mdp_clock_is_enable_CAM_MDP() &&
				cmdq_mdp_clock_is_enable_IMG_DL_RELAY() &&
				cmdq_mdp_clock_is_enable_IMG_DL_ASYNC_TOP();
	case CMDQ_ENG_MDP_RDMA0:
		return cmdq_mdp_clock_is_enable_MDP_RDMA0();
	case CMDQ_ENG_MDP_RSZ0:
		return cmdq_mdp_clock_is_enable_MDP_RSZ0();
	case CMDQ_ENG_MDP_RSZ1:
		return cmdq_mdp_clock_is_enable_MDP_RSZ1();
	case CMDQ_ENG_MDP_WDMA:
		return cmdq_mdp_clock_is_enable_MDP_WDMA();
	case CMDQ_ENG_MDP_WROT0:
		return cmdq_mdp_clock_is_enable_MDP_WROT0();
	case CMDQ_ENG_MDP_TDSHP0:
		return cmdq_mdp_clock_is_enable_MDP_TDSHP0();
	case CMDQ_ENG_MDP_COLOR0:
		return cmdq_mdp_clock_is_enable_MDP_COLOR0();
	case CMDQ_ENG_MDP_CCORR0:
		return cmdq_mdp_clock_is_enable_MDP_CCORR();
	default:
		CMDQ_ERR("try to query unknown mdp clock");
		return false;
	}
}

void cmdq_mdp_enable_clock(bool enable, u32 engine)
{
	unsigned long register_address;
	uint32_t register_value;

	switch (engine) {
	case CMDQ_ENG_MDP_CAMIN:
		cmdq_mdp_enable_clock_CAM_MDP(enable);
		cmdq_mdp_enable_clock_IMG_DL_RELAY(enable);
		cmdq_mdp_enable_clock_IMG_DL_ASYNC_TOP(enable);
		break;
	case CMDQ_ENG_MDP_RDMA0:
		cmdq_mdp_enable_clock_MDP_RDMA0(enable);
		if (enable) {
			/* Set MDP_RDMA0 DCM enable */
			register_address = MDP_RDMA0_BASE + 0x0;
			register_value = CMDQ_REG_GET32(register_address);
			/* DCM_EN is bit 4 */
			register_value |= (0x1 << 4);
			CMDQ_REG_SET32(register_address, register_value);
		}
		break;
	case CMDQ_ENG_MDP_RSZ0:
		cmdq_mdp_enable_clock_MDP_RSZ0(enable);
		break;
	case CMDQ_ENG_MDP_RSZ1:
		cmdq_mdp_enable_clock_MDP_RSZ1(enable);
		break;
	case CMDQ_ENG_MDP_WDMA:
		cmdq_mdp_enable_clock_MDP_WDMA(enable);
		if (enable) {
			/* Set MDP_WDMA DCM enable */
			register_address = MDP_WDMA_BASE + 0x8;
			register_value = CMDQ_REG_GET32(register_address);
			/* DCM_EN is bit 31 */
			register_value |= (0x1 << 31);
			CMDQ_REG_SET32(register_address, register_value);
		}
		break;
	case CMDQ_ENG_MDP_WROT0:
#if IS_ENABLED(CONFIG_MTK_SMI_EXT)
		if (enable)
			smi_bus_prepare_enable(SMI_LARB0, "MDPSRAM");
#endif
		cmdq_mdp_enable_clock_MDP_WROT0(enable);
		if (enable) {
			/* Set MDP_WROT0 DCM enable */
			register_address = MDP_WROT0_BASE + 0x7C;
			register_value = CMDQ_REG_GET32(register_address);
			/* DCM_EN is bit 16 */
			register_value |= (0x1 << 16);
			CMDQ_REG_SET32(register_address, register_value);
		}
#if IS_ENABLED(CONFIG_MTK_SMI_EXT)
		if (!enable)
			smi_bus_disable_unprepare(SMI_LARB0, "MDPSRAM");
#endif
		break;
	case CMDQ_ENG_MDP_TDSHP0:
		cmdq_mdp_enable_clock_MDP_TDSHP0(enable);
		break;
	case CMDQ_ENG_MDP_COLOR0:
#ifdef CMDQ_MDP_COLOR
		cmdq_mdp_enable_clock_MDP_COLOR0(enable);
#endif
		break;
	case CMDQ_ENG_MDP_CCORR0:
		cmdq_mdp_enable_clock_MDP_CCORR(enable);
		break;
	default:
		CMDQ_ERR("try to enable unknown mdp clock");
		break;
	}
}

/* Common Clock Framework */
void cmdq_mdp_init_module_clk(void)
{
	cmdq_dev_get_module_clock_by_name("mdpsys_config", "CAM_MDP",
		&gCmdqMdpModuleClock.clk_CAM_MDP);
	cmdq_dev_get_module_clock_by_name("mdpsys_config", "IMG_DL_RELAY",
		&gCmdqMdpModuleClock.clk_IMG_DL_RELAY);
	cmdq_dev_get_module_clock_by_name("mdpsys_config", "IMG_DL_ASYNC_TOP",
		&gCmdqMdpModuleClock.clk_IMG_DL_ASYNC_TOP);
	cmdq_dev_get_module_clock_by_name("mdp_rdma0", "MDP_RDMA0",
		&gCmdqMdpModuleClock.clk_MDP_RDMA0);
	cmdq_dev_get_module_clock_by_name("mdp_rsz0", "MDP_RSZ0",
		&gCmdqMdpModuleClock.clk_MDP_RSZ0);
	cmdq_dev_get_module_clock_by_name("mdp_rsz1", "MDP_RSZ1",
		&gCmdqMdpModuleClock.clk_MDP_RSZ1);
	cmdq_dev_get_module_clock_by_name("mdp_wdma0", "MDP_WDMA",
		&gCmdqMdpModuleClock.clk_MDP_WDMA);
	cmdq_dev_get_module_clock_by_name("mdp_wrot0", "MDP_WROT0",
		&gCmdqMdpModuleClock.clk_MDP_WROT0);
	cmdq_dev_get_module_clock_by_name("mdp_tdshp0", "MDP_TDSHP",
		&gCmdqMdpModuleClock.clk_MDP_TDSHP);
	cmdq_dev_get_module_clock_by_name("mdp_ccorr0", "MDP_CCORR",
		&gCmdqMdpModuleClock.clk_MDP_CCORR);
#ifdef CMDQ_MDP_COLOR
	cmdq_dev_get_module_clock_by_name("mdp_color0", "MDP_COLOR",
		&gCmdqMdpModuleClock.clk_MDP_COLOR);
#endif
}
/* MDP engine dump */
void cmdq_mdp_dump_rsz(const unsigned long base, const char *label)
{
	uint32_t value[11] = { 0 };
	uint32_t request[4] = { 0 };
	uint32_t state = 0;

	value[0] = CMDQ_REG_GET32(base + 0x004);
	value[1] = CMDQ_REG_GET32(base + 0x008);
	value[2] = CMDQ_REG_GET32(base + 0x010);
	value[3] = CMDQ_REG_GET32(base + 0x014);
	value[4] = CMDQ_REG_GET32(base + 0x018);
	value[5] = CMDQ_REG_GET32(base + 0x01C);
	CMDQ_REG_SET32(base + 0x044, 0x00000001);
	value[6] = CMDQ_REG_GET32(base + 0x048);
	CMDQ_REG_SET32(base + 0x044, 0x00000002);
	value[7] = CMDQ_REG_GET32(base + 0x048);
	CMDQ_REG_SET32(base + 0x044, 0x00000003);
	value[8] = CMDQ_REG_GET32(base + 0x048);
	value[9] = CMDQ_REG_GET32(base + 0x100);
	value[10] = CMDQ_REG_GET32(base + 0x200);
	CMDQ_ERR(
		"=============== [CMDQ] %s Status ====================================\n",
		label);
	CMDQ_ERR(
		"RSZ_CONTROL_1: 0x%08x, RSZ_CONTROL_2: 0x%08x, RSZ_INPUT_IMAGE: 0x%08x, RSZ_OUTPUT_IMAGE: 0x%08x\n",
		value[0], value[1], value[2], value[3]);
	CMDQ_ERR(
		"RSZ_HORIZONTAL_COEFF_STEP: 0x%08x, RSZ_VERTICAL_COEFF_STEP: 0x%08x\n",
		value[4], value[5]);
	CMDQ_ERR(
		"RSZ_DEBUG_1: 0x%08x, RSZ_DEBUG_2: 0x%08x, RSZ_DEBUG_3: 0x%08x\n",
		value[6], value[7], value[8]);
	CMDQ_ERR("PAT1_GEN_SET: 0x%08x, PAT2_GEN_SET: 0x%08x\n",
		value[9], value[10]);
	/* parse state */
	/* .valid=1/request=1: upstream module sends data */
	/* .ready=1: downstream module receives data */
	state = value[7] & 0xF;
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

int32_t cmdqMdpClockOn(uint64_t engineFlag)
{
	CMDQ_MSG("Enable MDP(0x%llx) clock begin\n", engineFlag);
#ifdef CMDQ_PWR_AWARE
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_CAMIN);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_RDMA0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_CCORR0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_RSZ0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_RSZ1);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_TDSHP0);
#ifdef CMDQ_MDP_COLOR
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_COLOR0);
#endif
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_WROT0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_WDMA);
#else
	CMDQ_MSG("Force MDP clock all on\n");

	/* enable all bits in MMSYS_CG_CLR0 and MMSYS_CG_CLR1 */
	CMDQ_REG_SET32(MMSYS_CONFIG_BASE + 0x108, 0xFFFFFFFF);
	CMDQ_REG_SET32(MMSYS_CONFIG_BASE + 0x118, 0xFFFFFFFF);

#endif				/* #ifdef CMDQ_PWR_AWARE */

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

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RSZ0))
		cmdq_mdp_get_func()->mdpDumpRsz(MDP_RSZ0_BASE, "RSZ0");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RSZ1))
		cmdq_mdp_get_func()->mdpDumpRsz(MDP_RSZ1_BASE, "RSZ1");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_TDSHP0))
		cmdq_mdp_get_func()->mdpDumpTdshp(MDP_TDSHP_BASE, "TDSHP");

#ifdef CMDQ_MDP_COLOR
	if (engineFlag & (1LL << CMDQ_ENG_MDP_COLOR0)) {
		CMDQ_ERR("COLOR : %s\n", "MDP");
		cmdq_mdp_dump_color(MDP_COLOR_BASE, "COLOR0");
	}
#else
	if (engineFlag & (1LL << CMDQ_ENG_MDP_COLOR0)) {
		CMDQ_ERR("COLOR : %s\n", "DISP");
		cmdq_mdp_dump_color(MDP_COLOR_BASE, "COLOR0");
	}
#endif

	if (engineFlag & (1LL << CMDQ_ENG_MDP_WROT0))
		cmdq_mdp_dump_rot(MDP_WROT0_BASE, "WROT0");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_WDMA))
		cmdq_mdp_dump_wdma(MDP_WDMA_BASE, "WDMA");

	/* verbose case, dump entire 1KB HW register region */
	/* for each enabled HW module. */
	if (logLevel >= 1) {
		int inner = 0;

		const struct MODULE_BASE bases[] = {
			DEFINE_MODULE(CMDQ_ENG_MDP_RDMA0, MDP_RDMA0_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_RSZ0, MDP_RSZ0_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_RSZ1, MDP_RSZ1_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_TDSHP0, MDP_TDSHP_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_COLOR0, MDP_COLOR_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_WROT0, MDP_WROT0_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_WDMA, MDP_WDMA_BASE),
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
	MOUT_BITS_ISP_MDP = 4,	/* bit  4: ISP_MDP multiple outupt reset */
	MOUT_BITS_MDP_CCORR = 5,/* bit  5: MDP_CCORR multiple outupt reset */
	MOUT_BITS_MDP_COLOR = 6, /* bit  6: MDP_COLOR multiple outupt reset */
	MOUT_BITS_MDP_RDMA0 = 7, /* bit  7: MDP_RDMA0 multiple outupt reset */
	MOUT_BITS_MDP_PRZ0 = 8,	/* bit  8: MDP_PRZ0 multiple outupt reset */
	MOUT_BITS_MDP_PRZ1 = 9,	/* bit  9: MDP_PRZ1 multiple outupt reset */
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

	long MMSYS_MOUT_RST_REG = MMSYS_CONFIG_BASE + (0x048);

	CMDQ_PROF_START(0, "MDP_Rst");
	CMDQ_VERBOSE("Reset MDP(0x%llx) begin\n", engineFlag);

	/* After resetting each component, */
	/* we need also reset corresponding MOUT config. */
	mout_bits_old = CMDQ_REG_GET32(MMSYS_MOUT_RST_REG);
	mout_bits = 0;

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RDMA0)) {
		mout_bits |= (1 << MOUT_BITS_MDP_RDMA0);

		status = cmdq_mdp_loop_reset(CMDQ_ENG_MDP_RDMA0,
			MDP_RDMA0_BASE + 0x8, MDP_RDMA0_BASE + 0x408,
			0x7FF00, 0x100, false);
		if (status != 0)
			engineToResetAgain |= (1LL << CMDQ_ENG_MDP_RDMA0);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RSZ0)) {
		mout_bits |= (1 << MOUT_BITS_MDP_PRZ0);
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_RSZ0)) {
			CMDQ_REG_SET32(MDP_RSZ0_BASE, 0x0);
			CMDQ_REG_SET32(MDP_RSZ0_BASE, 0x10000);
			CMDQ_REG_SET32(MDP_RSZ0_BASE, 0x0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RSZ1)) {
		mout_bits |= (1 << MOUT_BITS_MDP_PRZ1);
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_RSZ1)) {
			CMDQ_REG_SET32(MDP_RSZ1_BASE, 0x0);
			CMDQ_REG_SET32(MDP_RSZ1_BASE, 0x10000);
			CMDQ_REG_SET32(MDP_RSZ1_BASE, 0x0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_TDSHP0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_TDSHP0)) {
			CMDQ_REG_SET32(MDP_TDSHP_BASE + 0x100, 0x0);
			CMDQ_REG_SET32(MDP_TDSHP_BASE + 0x100, 0x2);
			CMDQ_REG_SET32(MDP_TDSHP_BASE + 0x100, 0x0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_COLOR0))
		mout_bits |= (1 << MOUT_BITS_MDP_COLOR);

	if (engineFlag & (1LL << CMDQ_ENG_MDP_WROT0)) {
		status = cmdq_mdp_loop_reset(CMDQ_ENG_MDP_WROT0,
			MDP_WROT0_BASE + 0x010, MDP_WROT0_BASE + 0x014,
			0x1, 0x1, true);
		if (status != 0)
			engineToResetAgain |= (1LL << CMDQ_ENG_MDP_WROT0);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_WDMA)) {
		status = cmdq_mdp_loop_reset(CMDQ_ENG_MDP_WDMA,
			MDP_WDMA_BASE + 0x00C, MDP_WDMA_BASE + 0x0A0,
			0x3FF, 0x1, false);
		if (status != 0)
			engineToResetAgain |= (1LL << CMDQ_ENG_MDP_WDMA);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_CAMIN)) {
		/* MDP_CAMIN can only reset by mmsys, */
		/* so this is not a "error" */
		cmdq_mdp_reset_with_mmsys((1LL << CMDQ_ENG_MDP_CAMIN));
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_CCORR0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_CCORR0)) {
			CMDQ_REG_SET32(MDP_CCORR_BASE + 0x04, 0x1);
			CMDQ_REG_SET32(MDP_CCORR_BASE + 0x04, 0x0);
		}
	}

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

		CMDQ_ERR(
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

	CMDQ_MSG("Disable MDP(0x%llx) clock begin\n", engineFlag);
	if (engineFlag & (1LL << CMDQ_ENG_MDP_WDMA)) {
		cmdq_mdp_loop_off(CMDQ_ENG_MDP_WDMA,
			MDP_WDMA_BASE + 0x00C, MDP_WDMA_BASE + 0X0A0,
			0x3FF, 0x1, false);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_WROT0)) {
		cmdq_mdp_loop_off(CMDQ_ENG_MDP_WROT0,
			MDP_WROT0_BASE + 0X010, MDP_WROT0_BASE + 0X014,
			0x1, 0x1, true);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_TDSHP0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_TDSHP0)) {
			CMDQ_REG_SET32(MDP_TDSHP_BASE + 0x100, 0x0);
			CMDQ_REG_SET32(MDP_TDSHP_BASE + 0x100, 0x2);
			CMDQ_REG_SET32(MDP_TDSHP_BASE + 0x100, 0x0);
			CMDQ_MSG("Disable MDP_TDSHP0 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_TDSHP0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_CCORR0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_CCORR0)) {
			CMDQ_MSG("Disable MDP_CCORR clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_CCORR0);
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

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RDMA0)) {
		cmdq_mdp_loop_off(CMDQ_ENG_MDP_RDMA0, MDP_RDMA0_BASE + 0x008,
			MDP_RDMA0_BASE + 0x408, 0x7FF00, 0x100, false);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_CAMIN)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_CAMIN)) {
			cmdq_mdp_reset_with_mmsys((1LL << CMDQ_ENG_MDP_CAMIN));
			CMDQ_MSG("Disable MDP_CAMIN clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_CAMIN);
		}
	}
#ifdef CMDQ_MDP_COLOR
	if (engineFlag & (1LL << CMDQ_ENG_MDP_COLOR0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_COLOR0)) {
			CMDQ_MSG("Disable MDP_COLOR0 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_COLOR0);
		}
	}
#endif
	CMDQ_MSG("Disable MDP(0x%llx) clock end\n", engineFlag);
#endif				/* #ifdef CMDQ_PWR_AWARE */

	return 0;
}

static s32 mdp_is_mod_suspend(struct EngineStruct *engine_list)
{
	s32 status = 0;
	int i;
	enum CMDQ_ENG_ENUM e = 0;

	u32 non_suspend_engine[] = {
		CMDQ_ENG_ISP_IMGI,
		CMDQ_ENG_MDP_RDMA0,
		CMDQ_ENG_MDP_RSZ0,
		CMDQ_ENG_MDP_RSZ2,
		CMDQ_ENG_MDP_TDSHP0,
		CMDQ_ENG_MDP_WROT0,
		CMDQ_ENG_MDP_WDMA,
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
	return (handle->engineFlag & CMDQ_ENG_MTEE_GROUP_BITS);
#else
	return false;
#endif
}

static bool mdp_is_isp_img(struct cmdqRecStruct *handle)
{
	return (handle->engineFlag & (1LL << CMDQ_ENG_ISP_IMGI) &&
		handle->engineFlag & (1LL << CMDQ_ENG_ISP_IMGO) &&
		handle->engineFlag & (1LL << CMDQ_ENG_ISP_IMG2O));
}

static bool mdp_is_isp_camin(struct cmdqRecStruct *handle)
{
	return (handle->engineFlag &
		((1LL << CMDQ_ENG_MDP_CAMIN) | CMDQ_ENG_ISP_GROUP_BITS));
}

struct device *mdp_init_larb(struct platform_device *pdev, u8 idx)
{
	struct device_node *node;
	struct platform_device *larb_pdev;

	CMDQ_LOG("%s start\n", __func__);

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

	/* Register M4U Translation Fault function */
	mtk_iommu_register_fault_callback(M4U_PORT_MDP_RDMA0,
		cmdq_TranslationFault_callback, (void *)pdev, false);
	mtk_iommu_register_fault_callback(M4U_PORT_MDP_WDMA0,
		cmdq_TranslationFault_callback, (void *)pdev, false);
	mtk_iommu_register_fault_callback(M4U_PORT_MDP_WROT0,
		cmdq_TranslationFault_callback, (void *)pdev, false);
#endif

	/* must porting in dts */
	larb0 = mdp_init_larb(pdev, 0);
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
	const u32 ISP_ONLY[2] = {
		((1LL << CMDQ_ENG_ISP_IMGI) | (1LL << CMDQ_ENG_ISP_IMG2O)),
		((1LL << CMDQ_ENG_ISP_IMGI) | (1LL << CMDQ_ENG_ISP_IMG2O) |
		 (1LL << CMDQ_ENG_ISP_IMGO))
	};

	/* common part for both normal and secure path */
	/* for JPEG scenario, use HW flag is sufficient */
	if (task->engineFlag & (1LL << CMDQ_ENG_JPEG_ENC))
		module = "JPGENC";
	else if (task->engineFlag & (1LL << CMDQ_ENG_JPEG_DEC))
		module = "JPGDEC";
	else if ((ISP_ONLY[0] == task->engineFlag) ||
		(ISP_ONLY[1] == task->engineFlag))
		module = "DIP_ONLY";

	/* for secure path, use HW flag is sufficient */
	do {
		if (module != NULL)
			break;

		if (!task->secData.is_secure) {
			/* normal path,
			 * need parse current running instruciton
			 * for more detail
			 */
			break;
		} else if (CMDQ_ENG_MDP_GROUP_FLAG(task->engineFlag)) {
			module = "MDP";
			break;
		} else if (CMDQ_ENG_DPE_GROUP_FLAG(task->engineFlag)) {
			module = "DPE";
			break;
		} else if (CMDQ_ENG_RSC_GROUP_FLAG(task->engineFlag)) {
			module = "RSC";
			break;
		} else if (CMDQ_ENG_GEPF_GROUP_FLAG(task->engineFlag)) {
			module = "GEPF";
			break;
		} else if (CMDQ_ENG_EAF_GROUP_FLAG(task->engineFlag)) {
			module = "EAF";
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
	s32 mdp_clk_usage;

	if (!larb) {
		CMDQ_ERR("%s smi larb not support\n", __func__);
		return;
	}

	if (enable) {
		int ret = 0;

		mdp_clk_usage = atomic_inc_return(&mdp_smi_clk_usage);

		if (mdp_clk_usage == 1) {
			ret = mtk_smi_larb_get(larb);
			if (ret)
				CMDQ_ERR("%s enable larb fail ret:%d\n", __func__, ret);
			CMDQ_LOG_CLOCK("%s enable, mdp_smi_clk_usage:%d\n",
				__func__, mdp_clk_usage);
		}
	} else {

		mdp_clk_usage = atomic_dec_return(&mdp_smi_clk_usage);

		if (mdp_clk_usage == 0) {
			mtk_smi_larb_put(larb);
			CMDQ_LOG_CLOCK("%s disable, mdp_smi_clk_usage:%d\n",
				__func__, mdp_clk_usage);
		}
	}
#endif
}

static void cmdq_mdp_enable_common_clock(bool enable, u64 engine_flag)
{
	CMDQ_LOG_CLOCK("%s enable:%d, engine_flag:%llx\n", __func__, enable, engine_flag);

	if (engine_flag & MDP_ENG_LARB0)
		mdp_enable_larb(enable, larb0);
}

static void cmdq_mdp_check_hw_status(struct cmdqRecStruct *handle)
{
	/* Nothing to do */
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

	/* ISP */
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, ISP_IMGI);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, ISP_VIPI);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, ISP_LCEI);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, ISP_IMG2O);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, ISP_IMG3O);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, DPE);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, FDVT);

	return sec_eng_flag;
}
#endif

static u32 cmdq_mdp_qos_translate_port(u32 engine_id)
{
	switch (engine_id) {
	case CMDQ_ENG_MDP_RDMA0:
		return  M4U_PORT_MDP_RDMA0;
	case CMDQ_ENG_MDP_WROT0:
		return M4U_PORT_MDP_WROT0;
	case CMDQ_ENG_MDP_WDMA:
		return M4U_PORT_MDP_WDMA0;
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
	MDP_ICC_GET(mdp_wrot0);
	MDP_ICC_GET(mdp_wdma);
}

static void *mdp_qos_get_path(u32 thread_id, u32 port)
{
	if (!port)
		return NULL;

	switch (port) {
	/* mdp part */
	case M4U_PORT_MDP_RDMA0:
		return path_mdp_rdma0[thread_id];
	case M4U_PORT_MDP_WROT0:
		return path_mdp_wrot0[thread_id];
	case M4U_PORT_MDP_WDMA0:
		return path_mdp_wdma[thread_id];
	}

	CMDQ_ERR("%s pmqos invalid port %d\n", __func__, port);
	return NULL;
}

static void mdp_qos_clear_all(u32 thread_id)
{
	mtk_icc_set_bw(path_mdp_rdma0[thread_id], 0, 0);
	mtk_icc_set_bw(path_mdp_wrot0[thread_id], 0, 0);
	mtk_icc_set_bw(path_mdp_wdma[thread_id], 0, 0);
}

static void mdp_qos_clear_all_isp(u32 thread_id)
{
	/* TO DO */
}

static u32 mdp_get_group_max(void)
{
	return CMDQ_MAX_GROUP_COUNT;
}

static u32 mdp_get_group_isp_plat(void)
{
	return CMDQ_GROUP_ISP;
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

void cmdq_mdp_compose_readback(struct cmdqRecStruct *handle,
	u16 engine, dma_addr_t addr, u32 param)
{
	/* cmdq v3 implement this function, but do nothing */
	CMDQ_ERR("%s engine not support:%hu\n", __func__, engine);
}

static s32 mdp_get_rdma_idx(u32 eng_base)
{
	s32 rdma_idx = -1;

	switch (eng_base) {
	case ENGBASE_MDP_RDMA0:
		rdma_idx = 0;
		break;
	default:
		CMDQ_ERR("%s engine not support:%d\n", __func__, eng_base);
		break;
	}

	return rdma_idx;
}

static bool mdp_check_camin_support_virtual(void)
{
	/* Camera not attend GKI2.0 about mt6768 */
	return false;
}

static bool mdp_svp_support_meta_data(void)
{
	/* early GKI2.0 about mt6768 not support secure */
	return false;
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
	/* related mdp debug */
	pFunc->mdpIsModuleSuspend = mdp_is_mod_suspend;
	pFunc->mdpDumpEngineUsage = mdp_dump_engine_usage;

	pFunc->mdpIsMtee = mdp_is_mtee;
	pFunc->mdpIsIspImg = mdp_is_isp_img;
	pFunc->mdpIsIspCamin = mdp_is_isp_camin;

	pFunc->mdpInitialSet = cmdqMdpInitialSetting;

	pFunc->rdmaGetRegOffsetSrcAddr = cmdq_mdp_rdma_get_reg_offset_src_addr;
	pFunc->wrotGetRegOffsetDstAddr = cmdq_mdp_wrot_get_reg_offset_dst_addr;
	pFunc->wdmaGetRegOffsetDstAddr = cmdq_mdp_wdma_get_reg_offset_dst_addr;
	pFunc->parseErrModByEngFlag = cmdq_mdp_parse_error_module;
	pFunc->getEngineGroupBits = cmdq_mdp_get_engine_group_bits;
	pFunc->mdpEnableCommonClock = cmdq_mdp_enable_common_clock;
	pFunc->CheckHwStatus = cmdq_mdp_check_hw_status;
#ifdef CMDQ_SECURE_PATH_SUPPORT
	pFunc->mdpGetSecEngine = cmdq_mdp_get_secure_engine;
#endif
	/* related pmqos */
	pFunc->qosTransPort = cmdq_mdp_qos_translate_port;
	pFunc->qosInit = mdp_qos_init;
	pFunc->qosGetPath = mdp_qos_get_path;
	pFunc->qosClearAll = mdp_qos_clear_all;
	pFunc->qosClearAllIsp = mdp_qos_clear_all_isp;
	/* related CMDQ_ENG_GROUP_BITS */
	pFunc->getGroupMax = mdp_get_group_max;
	pFunc->getGroupIsp = mdp_get_group_isp_plat;
	pFunc->getGroupMdp = mdp_get_group_mdp;
	/* related MDP engines*/
	pFunc->getEngineBase = mdp_engine_base_get;
	pFunc->getEngineBaseCount = mdp_engine_base_count;
	pFunc->getEngineGroupName = mdp_get_engine_group_name;

	pFunc->mdpComposeReadback = cmdq_mdp_compose_readback;
	pFunc->getRDMAIndex = mdp_get_rdma_idx;
	/* feature support about mdp */
	pFunc->mdpIsCaminSupport = mdp_check_camin_support_virtual;
	pFunc->mdpSvpSupportMetaData = mdp_svp_support_meta_data;

}
MODULE_LICENSE("GPL");
