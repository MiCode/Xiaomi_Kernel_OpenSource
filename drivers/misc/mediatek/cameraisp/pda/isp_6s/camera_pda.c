// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <mt-plat/sync_write.h> /* For mt_reg_sync_writel(). */

#include <smi_public.h>
#include <linux/clk.h>

#include "camera_pda.h"

#include <linux/pm_runtime.h>

#include <linux/time.h>		//do_gettimeofday()

#include <smi_public.h>


#ifdef CONFIG_MTK_IOMMU_V2
#include <mach/mt_iommu.h>
#include "mach/pseudo_m4u.h"
#else /* CONFIG_MTK_IOMMU_V2 */
#include <m4u.h>
#endif /* CONFIG_MTK_IOMMU_V2 */


//#define FPGA_UT
//#define multi_frame_rst			/*used for test case multi_frame_rst*/
//#define GET_PDA_TIME

#define PDA_DEV_NAME "camera-pda"

#define LOG_INF(format, args...)                                               \
	pr_info(PDA_DEV_NAME " [%s] " format, __func__, ##args)

#ifndef MTRUE
#define MTRUE 1
#endif
#ifndef MFALSE
#define MFALSE 0
#endif

#define PDA_WR32(addr, data) mt_reg_sync_writel(data, addr)
#define PDA_RD32(addr) ioread32(addr)

#define CAMSYS_MAIN_BASE_ADDR CAMSYS_CONFIG_BASE

#define REG_CAMSYS_CG_SET               (CAMSYS_MAIN_BASE_ADDR + 0x4)
#define REG_CAMSYS_CG_CLR               (CAMSYS_MAIN_BASE_ADDR + 0x8)

#define PDA_DONE 0x00000001
#define PDA_ERROR 0x00000002
#define PDA_STATUS_REG 0x00000003
#define PDA_CLEAR_REG 0x00000000
#define PDA_TRIGGER 0x00000003
#define PDA_DOUBLE_BUFFER 0x00000009
#define PDA_MAKE_RESET 0x00000002
#define MASK_BIT_ZERO 0x00000001
#define PDA_RESET_VALUE 0x00000001
#define PDA_HW_RESET 0x00000004

#ifdef CONFIG_MTK_IOMMU_V2
static int PDA_MEM_USE_VIRTUL = 1;
#endif

static spinlock_t g_PDA_SpinLock;

wait_queue_head_t g_wait_queue_head;

int g_HWstatus;		//1  = pda done
			//-1 = error
			//0  = reset

struct PDA_CLK_STRUCT {
	struct clk *CG_PDA_TOP_MUX;
} pda_clk;

//Enable clock count
static unsigned int g_u4EnableClockCount;

#ifdef GET_PDA_TIME
// Get PDA process time
struct timeval time_begin, time_end;
struct timeval Config_time_begin, Config_time_end;
struct timeval total_time_begin, total_time_end;
struct timeval pda_done_time_end;
#endif

static inline void PDA_Prepare_Enable_ccf_clock(void)
{
	int ret;

	LOG_INF("clock begin");

	smi_bus_prepare_enable(SMI_LARB13, PDA_DEV_NAME);

	ret = clk_prepare_enable(pda_clk.CG_PDA_TOP_MUX);
	if (ret)
		LOG_INF("cannot prepare and enable CG_PDA_TOP_MUX clock\n");
	LOG_INF("clk_prepare_enable done");
}

static inline void PDA_Disable_Unprepare_ccf_clock(void)
{
	clk_disable_unprepare(pda_clk.CG_PDA_TOP_MUX);
	LOG_INF("clk_disable_unprepare: pda_clk.CG_PDA_TOP_MUX ");

	//smi_bus_disable_unprepare(SMI_LARB13, PDA_DEV_NAME);
}

#ifdef CONFIG_MTK_IOMMU_V2
static inline int m4u_control_iommu_port(void)
{
	struct M4U_PORT_STRUCT sPort;
	int ret = 0;

	/* LARB13 */
	int count_of_ports = 0;
	int i = 0;

	count_of_ports = M4U_PORT_L13_CAM_PDAO -
		M4U_PORT_L13_CAM_PADI0 + 1;

	for (i = 0; i < count_of_ports; i++) {
		sPort.ePortID = M4U_PORT_L13_CAM_PADI0+i;
		sPort.Virtuality = PDA_MEM_USE_VIRTUL;
		//LOG_INF("config M4U Port ePortID=%d\n", sPort.ePortID);
		#if defined(CONFIG_MTK_M4U) || defined(CONFIG_MTK_PSEUDO_M4U)
		ret = m4u_config_port(&sPort);
		if (ret == 0) {
			LOG_INF("config M4U Port %s to %s SUCCESS\n",
			iommu_get_port_name(M4U_PORT_L13_CAM_PADI0+i),
			PDA_MEM_USE_VIRTUL ? "virtual" : "physical");
		} else {
			LOG_INF("config M4U Port %s to %s FAIL(ret=%d)\n",
			iommu_get_port_name(M4U_PORT_L13_CAM_PADI0+i),
			PDA_MEM_USE_VIRTUL ? "virtual" : "physical", ret);
			ret = -1;
		}
		#endif
	}
	return ret;
}
#endif
/**************************************************************
 *
 **************************************************************/
static void EnableClock(bool En)
{

#ifdef CONFIG_MTK_IOMMU_V2
	int ret = 0;
#endif

	if (En) {			/* Enable clock. */

		//Enable clock count
		switch (g_u4EnableClockCount) {
		case 0:
			g_u4EnableClockCount++;

#ifndef FPGA_UT
		LOG_INF("It's real ic load, Enable Clock");
		PDA_Prepare_Enable_ccf_clock();
#else
		// Enable clock by hardcode:
		LOG_INF("It's LDVT load, Enable Clock");
		PDA_WR32(REG_CAMSYS_CG_CLR, 0xFFFFFFFF);
#endif

			break;
		default:
			g_u4EnableClockCount++;
			break;
		}

#ifdef CONFIG_MTK_IOMMU_V2
		ret = m4u_control_iommu_port();
		if (ret)
			LOG_INF("cannot config M4U IOMMU PORTS\n");
#endif

	} else {			/* Disable clock. */

		//Enable clock count
		g_u4EnableClockCount--;
		switch (g_u4EnableClockCount) {
		case 0:

#ifndef FPGA_UT
		LOG_INF("It's real ic load, Disable Clock");
		PDA_Disable_Unprepare_ccf_clock();
#else
		// Disable clock by hardcode:
		LOG_INF("It's LDVT load, Disable Clock");
		PDA_WR32(REG_CAMSYS_CG_SET, 0xFFFFFFFF);
#endif

			break;
		default:
			break;
		}

	}
}

static void pda_reset(void)
{
	//reset HW status
	g_HWstatus = 0;

	//make reset
	PDA_WR32(PDA_PDA_DMA_RST_REG, PDA_MAKE_RESET);

	//read reset status
	while ((PDA_RD32(PDA_PDA_DMA_RST_REG) & MASK_BIT_ZERO) != PDA_RESET_VALUE)
		LOG_INF("PDA resetting...\n");

	//equivalent to hardware reset
	PDA_WR32(PDA_PDA_TOP_CTL_REG, PDA_HW_RESET);

	//clear reset signal
	PDA_WR32(PDA_PDA_DMA_RST_REG, PDA_CLEAR_REG);

	//clear hardware reset signal
	PDA_WR32(PDA_PDA_TOP_CTL_REG, PDA_CLEAR_REG);
}

static void FillRegSettings(struct PDA_Config pda_PdaConfig)
{
	PDA_WR32(PDA_CFG_0_REG, pda_PdaConfig.PDA_CFG[0]);
	PDA_WR32(PDA_CFG_1_REG, pda_PdaConfig.PDA_CFG[1]);
	PDA_WR32(PDA_CFG_2_REG, pda_PdaConfig.PDA_CFG[2]);
	PDA_WR32(PDA_CFG_3_REG, pda_PdaConfig.PDA_CFG[3]);
	PDA_WR32(PDA_CFG_4_REG, pda_PdaConfig.PDA_CFG[4]);
	PDA_WR32(PDA_CFG_5_REG, pda_PdaConfig.PDA_CFG[5]);
	PDA_WR32(PDA_CFG_6_REG, pda_PdaConfig.PDA_CFG[6]);
	PDA_WR32(PDA_CFG_7_REG, pda_PdaConfig.PDA_CFG[7]);
	PDA_WR32(PDA_CFG_8_REG, pda_PdaConfig.PDA_CFG[8]);
	PDA_WR32(PDA_CFG_9_REG, pda_PdaConfig.PDA_CFG[9]);
	PDA_WR32(PDA_CFG_10_REG, pda_PdaConfig.PDA_CFG[10]);
	PDA_WR32(PDA_CFG_11_REG, pda_PdaConfig.PDA_CFG[11]);
	PDA_WR32(PDA_CFG_12_REG, pda_PdaConfig.PDA_CFG[12]);
	PDA_WR32(PDA_CFG_13_REG, pda_PdaConfig.PDA_CFG[13]);
	PDA_WR32(PDA_CFG_14_REG, pda_PdaConfig.PDA_CFG[14]);
	PDA_WR32(PDA_CFG_15_REG, pda_PdaConfig.PDA_CFG[15]);
	PDA_WR32(PDA_CFG_16_REG, pda_PdaConfig.PDA_CFG[16]);
	PDA_WR32(PDA_CFG_17_REG, pda_PdaConfig.PDA_CFG[17]);
	PDA_WR32(PDA_CFG_18_REG, pda_PdaConfig.PDA_CFG[18]);
	PDA_WR32(PDA_CFG_19_REG, pda_PdaConfig.PDA_CFG[19]);
	PDA_WR32(PDA_CFG_20_REG, pda_PdaConfig.PDA_CFG[20]);
	PDA_WR32(PDA_CFG_21_REG, pda_PdaConfig.PDA_CFG[21]);
	PDA_WR32(PDA_CFG_22_REG, pda_PdaConfig.PDA_CFG[22]);
	PDA_WR32(PDA_CFG_23_REG, pda_PdaConfig.PDA_CFG[23]);
	PDA_WR32(PDA_CFG_24_REG, pda_PdaConfig.PDA_CFG[24]);
	PDA_WR32(PDA_CFG_25_REG, pda_PdaConfig.PDA_CFG[25]);
	PDA_WR32(PDA_CFG_26_REG, pda_PdaConfig.PDA_CFG[26]);
	PDA_WR32(PDA_CFG_27_REG, pda_PdaConfig.PDA_CFG[27]);
	PDA_WR32(PDA_CFG_28_REG, pda_PdaConfig.PDA_CFG[28]);
	PDA_WR32(PDA_CFG_29_REG, pda_PdaConfig.PDA_CFG[29]);
	PDA_WR32(PDA_CFG_30_REG, pda_PdaConfig.PDA_CFG[30]);
	PDA_WR32(PDA_CFG_31_REG, pda_PdaConfig.PDA_CFG[31]);
	PDA_WR32(PDA_CFG_32_REG, pda_PdaConfig.PDA_CFG[32]);
	PDA_WR32(PDA_CFG_33_REG, pda_PdaConfig.PDA_CFG[33]);
	PDA_WR32(PDA_CFG_34_REG, pda_PdaConfig.PDA_CFG[34]);
	PDA_WR32(PDA_CFG_35_REG, pda_PdaConfig.PDA_CFG[35]);
	PDA_WR32(PDA_CFG_36_REG, pda_PdaConfig.PDA_CFG[36]);
	PDA_WR32(PDA_CFG_37_REG, pda_PdaConfig.PDA_CFG[37]);
	PDA_WR32(PDA_CFG_38_REG, pda_PdaConfig.PDA_CFG[38]);
	PDA_WR32(PDA_CFG_39_REG, pda_PdaConfig.PDA_CFG[39]);
	PDA_WR32(PDA_CFG_40_REG, pda_PdaConfig.PDA_CFG[40]);
	PDA_WR32(PDA_CFG_41_REG, pda_PdaConfig.PDA_CFG[41]);
	PDA_WR32(PDA_CFG_42_REG, pda_PdaConfig.PDA_CFG[42]);
	PDA_WR32(PDA_CFG_43_REG, pda_PdaConfig.PDA_CFG[43]);
	PDA_WR32(PDA_CFG_44_REG, pda_PdaConfig.PDA_CFG[44]);
	PDA_WR32(PDA_CFG_45_REG, pda_PdaConfig.PDA_CFG[45]);
	PDA_WR32(PDA_CFG_46_REG, pda_PdaConfig.PDA_CFG[46]);
	PDA_WR32(PDA_CFG_47_REG, pda_PdaConfig.PDA_CFG[47]);
	PDA_WR32(PDA_CFG_48_REG, pda_PdaConfig.PDA_CFG[48]);
	PDA_WR32(PDA_CFG_49_REG, pda_PdaConfig.PDA_CFG[49]);
	PDA_WR32(PDA_CFG_50_REG, pda_PdaConfig.PDA_CFG[50]);
	PDA_WR32(PDA_CFG_51_REG, pda_PdaConfig.PDA_CFG[51]);
	PDA_WR32(PDA_CFG_52_REG, pda_PdaConfig.PDA_CFG[52]);
	PDA_WR32(PDA_CFG_53_REG, pda_PdaConfig.PDA_CFG[53]);
	PDA_WR32(PDA_CFG_54_REG, pda_PdaConfig.PDA_CFG[54]);
	PDA_WR32(PDA_CFG_55_REG, pda_PdaConfig.PDA_CFG[55]);
	PDA_WR32(PDA_CFG_56_REG, pda_PdaConfig.PDA_CFG[56]);
	PDA_WR32(PDA_CFG_57_REG, pda_PdaConfig.PDA_CFG[57]);
	PDA_WR32(PDA_CFG_58_REG, pda_PdaConfig.PDA_CFG[58]);
	PDA_WR32(PDA_CFG_59_REG, pda_PdaConfig.PDA_CFG[59]);
	PDA_WR32(PDA_CFG_60_REG, pda_PdaConfig.PDA_CFG[60]);
	PDA_WR32(PDA_CFG_61_REG, pda_PdaConfig.PDA_CFG[61]);
	PDA_WR32(PDA_CFG_62_REG, pda_PdaConfig.PDA_CFG[62]);
	PDA_WR32(PDA_CFG_63_REG, pda_PdaConfig.PDA_CFG[63]);
	PDA_WR32(PDA_CFG_64_REG, pda_PdaConfig.PDA_CFG[64]);
	PDA_WR32(PDA_CFG_65_REG, pda_PdaConfig.PDA_CFG[65]);
	PDA_WR32(PDA_CFG_66_REG, pda_PdaConfig.PDA_CFG[66]);
	PDA_WR32(PDA_CFG_67_REG, pda_PdaConfig.PDA_CFG[67]);
	PDA_WR32(PDA_CFG_68_REG, pda_PdaConfig.PDA_CFG[68]);
	PDA_WR32(PDA_CFG_69_REG, pda_PdaConfig.PDA_CFG[69]);
	PDA_WR32(PDA_CFG_70_REG, pda_PdaConfig.PDA_CFG[70]);
	PDA_WR32(PDA_CFG_71_REG, pda_PdaConfig.PDA_CFG[71]);
	PDA_WR32(PDA_CFG_72_REG, pda_PdaConfig.PDA_CFG[72]);
	PDA_WR32(PDA_CFG_73_REG, pda_PdaConfig.PDA_CFG[73]);
	PDA_WR32(PDA_CFG_74_REG, pda_PdaConfig.PDA_CFG[74]);
	PDA_WR32(PDA_CFG_75_REG, pda_PdaConfig.PDA_CFG[75]);
	PDA_WR32(PDA_CFG_76_REG, pda_PdaConfig.PDA_CFG[76]);
	PDA_WR32(PDA_CFG_77_REG, pda_PdaConfig.PDA_CFG[77]);
	PDA_WR32(PDA_CFG_78_REG, pda_PdaConfig.PDA_CFG[78]);
	PDA_WR32(PDA_CFG_79_REG, pda_PdaConfig.PDA_CFG[79]);
	PDA_WR32(PDA_CFG_80_REG, pda_PdaConfig.PDA_CFG[80]);
	PDA_WR32(PDA_CFG_81_REG, pda_PdaConfig.PDA_CFG[81]);
	PDA_WR32(PDA_CFG_82_REG, pda_PdaConfig.PDA_CFG[82]);
	PDA_WR32(PDA_CFG_83_REG, pda_PdaConfig.PDA_CFG[83]);
	PDA_WR32(PDA_CFG_84_REG, pda_PdaConfig.PDA_CFG[84]);
	PDA_WR32(PDA_CFG_85_REG, pda_PdaConfig.PDA_CFG[85]);
	PDA_WR32(PDA_CFG_86_REG, pda_PdaConfig.PDA_CFG[86]);
	PDA_WR32(PDA_CFG_87_REG, pda_PdaConfig.PDA_CFG[87]);
	PDA_WR32(PDA_CFG_88_REG, pda_PdaConfig.PDA_CFG[88]);
	PDA_WR32(PDA_CFG_89_REG, pda_PdaConfig.PDA_CFG[89]);
	PDA_WR32(PDA_CFG_90_REG, pda_PdaConfig.PDA_CFG[90]);
	PDA_WR32(PDA_CFG_91_REG, pda_PdaConfig.PDA_CFG[91]);
	PDA_WR32(PDA_CFG_92_REG, pda_PdaConfig.PDA_CFG[92]);
	PDA_WR32(PDA_CFG_93_REG, pda_PdaConfig.PDA_CFG[93]);
	PDA_WR32(PDA_CFG_94_REG, pda_PdaConfig.PDA_CFG[94]);
	PDA_WR32(PDA_CFG_95_REG, pda_PdaConfig.PDA_CFG[95]);
	PDA_WR32(PDA_CFG_96_REG, pda_PdaConfig.PDA_CFG[96]);
	PDA_WR32(PDA_CFG_97_REG, pda_PdaConfig.PDA_CFG[97]);
	PDA_WR32(PDA_CFG_98_REG, pda_PdaConfig.PDA_CFG[98]);
	PDA_WR32(PDA_CFG_99_REG, pda_PdaConfig.PDA_CFG[99]);
	PDA_WR32(PDA_CFG_100_REG, pda_PdaConfig.PDA_CFG[100]);
	PDA_WR32(PDA_CFG_101_REG, pda_PdaConfig.PDA_CFG[101]);
	PDA_WR32(PDA_CFG_102_REG, pda_PdaConfig.PDA_CFG[102]);
	PDA_WR32(PDA_CFG_103_REG, pda_PdaConfig.PDA_CFG[103]);
	PDA_WR32(PDA_CFG_104_REG, pda_PdaConfig.PDA_CFG[104]);
	PDA_WR32(PDA_CFG_105_REG, pda_PdaConfig.PDA_CFG[105]);
	PDA_WR32(PDA_CFG_106_REG, pda_PdaConfig.PDA_CFG[106]);
	PDA_WR32(PDA_CFG_107_REG, pda_PdaConfig.PDA_CFG[107]);
	PDA_WR32(PDA_CFG_108_REG, pda_PdaConfig.PDA_CFG[108]);
	PDA_WR32(PDA_CFG_109_REG, pda_PdaConfig.PDA_CFG[109]);
	PDA_WR32(PDA_CFG_110_REG, pda_PdaConfig.PDA_CFG[110]);
	PDA_WR32(PDA_CFG_111_REG, pda_PdaConfig.PDA_CFG[111]);
	PDA_WR32(PDA_CFG_112_REG, pda_PdaConfig.PDA_CFG[112]);
	PDA_WR32(PDA_CFG_113_REG, pda_PdaConfig.PDA_CFG[113]);
	PDA_WR32(PDA_CFG_114_REG, pda_PdaConfig.PDA_CFG[114]);
	PDA_WR32(PDA_CFG_115_REG, pda_PdaConfig.PDA_CFG[115]);
	PDA_WR32(PDA_CFG_116_REG, pda_PdaConfig.PDA_CFG[116]);
	PDA_WR32(PDA_CFG_117_REG, pda_PdaConfig.PDA_CFG[117]);
	PDA_WR32(PDA_CFG_118_REG, pda_PdaConfig.PDA_CFG[118]);
	PDA_WR32(PDA_CFG_119_REG, pda_PdaConfig.PDA_CFG[119]);
	PDA_WR32(PDA_CFG_120_REG, pda_PdaConfig.PDA_CFG[120]);
	PDA_WR32(PDA_CFG_121_REG, pda_PdaConfig.PDA_CFG[121]);
	PDA_WR32(PDA_CFG_122_REG, pda_PdaConfig.PDA_CFG[122]);
	PDA_WR32(PDA_CFG_123_REG, pda_PdaConfig.PDA_CFG[123]);
	PDA_WR32(PDA_CFG_124_REG, pda_PdaConfig.PDA_CFG[124]);
	PDA_WR32(PDA_CFG_125_REG, pda_PdaConfig.PDA_CFG[125]);
	PDA_WR32(PDA_CFG_126_REG, pda_PdaConfig.PDA_CFG[126]);
	PDA_WR32(PDA_PDAI_P1_BASE_ADDR_REG, pda_PdaConfig.PDA_PDAI_P1_BASE_ADDR);
	PDA_WR32(PDA_PDATI_P1_BASE_ADDR_REG, pda_PdaConfig.PDA_PDATI_P1_BASE_ADDR);
	PDA_WR32(PDA_PDAI_P2_BASE_ADDR_REG, pda_PdaConfig.PDA_PDAI_P2_BASE_ADDR);
	PDA_WR32(PDA_PDATI_P2_BASE_ADDR_REG, pda_PdaConfig.PDA_PDATI_P2_BASE_ADDR);
	PDA_WR32(PDA_PDAI_STRIDE_REG, pda_PdaConfig.PDA_PDAI_STRIDE);
	PDA_WR32(PDA_PDAI_P1_CON0_REG, pda_PdaConfig.PDA_PDAI_P1_CON0);
	PDA_WR32(PDA_PDAI_P1_CON1_REG, pda_PdaConfig.PDA_PDAI_P1_CON1);
	PDA_WR32(PDA_PDAI_P1_CON2_REG, pda_PdaConfig.PDA_PDAI_P1_CON2);
	PDA_WR32(PDA_PDAI_P1_CON3_REG, pda_PdaConfig.PDA_PDAI_P1_CON3);
	PDA_WR32(PDA_PDAI_P1_CON4_REG, pda_PdaConfig.PDA_PDAI_P1_CON4);
	PDA_WR32(PDA_PDATI_P1_CON0_REG, pda_PdaConfig.PDA_PDATI_P1_CON0);
	PDA_WR32(PDA_PDATI_P1_CON1_REG, pda_PdaConfig.PDA_PDATI_P1_CON1);
	PDA_WR32(PDA_PDATI_P1_CON2_REG, pda_PdaConfig.PDA_PDATI_P1_CON2);
	PDA_WR32(PDA_PDATI_P1_CON3_REG, pda_PdaConfig.PDA_PDATI_P1_CON3);
	PDA_WR32(PDA_PDATI_P1_CON4_REG, pda_PdaConfig.PDA_PDATI_P1_CON4);
	PDA_WR32(PDA_PDAI_P2_CON0_REG, pda_PdaConfig.PDA_PDAI_P2_CON0);
	PDA_WR32(PDA_PDAI_P2_CON1_REG, pda_PdaConfig.PDA_PDAI_P2_CON1);
	PDA_WR32(PDA_PDAI_P2_CON2_REG, pda_PdaConfig.PDA_PDAI_P2_CON2);
	PDA_WR32(PDA_PDAI_P2_CON3_REG, pda_PdaConfig.PDA_PDAI_P2_CON3);
	PDA_WR32(PDA_PDAI_P2_CON4_REG, pda_PdaConfig.PDA_PDAI_P2_CON4);
	PDA_WR32(PDA_PDATI_P2_CON0_REG, pda_PdaConfig.PDA_PDATI_P2_CON0);
	PDA_WR32(PDA_PDATI_P2_CON1_REG, pda_PdaConfig.PDA_PDATI_P2_CON1);
	PDA_WR32(PDA_PDATI_P2_CON2_REG, pda_PdaConfig.PDA_PDATI_P2_CON2);
	PDA_WR32(PDA_PDATI_P2_CON3_REG, pda_PdaConfig.PDA_PDATI_P2_CON3);
	PDA_WR32(PDA_PDATI_P2_CON4_REG, pda_PdaConfig.PDA_PDATI_P2_CON4);
	PDA_WR32(PDA_PDAO_P1_BASE_ADDR_REG, pda_PdaConfig.PDA_PDAO_P1_BASE_ADDR);
	PDA_WR32(PDA_PDAO_P1_XSIZE_REG, pda_PdaConfig.PDA_PDAO_P1_XSIZE);
	PDA_WR32(PDA_PDAO_P1_CON0_REG, pda_PdaConfig.PDA_PDAO_P1_CON0);
	PDA_WR32(PDA_PDAO_P1_CON1_REG, pda_PdaConfig.PDA_PDAO_P1_CON1);
	PDA_WR32(PDA_PDAO_P1_CON2_REG, pda_PdaConfig.PDA_PDAO_P1_CON2);
	PDA_WR32(PDA_PDAO_P1_CON3_REG, pda_PdaConfig.PDA_PDAO_P1_CON3);
	PDA_WR32(PDA_PDAO_P1_CON4_REG, pda_PdaConfig.PDA_PDAO_P1_CON4);
	PDA_WR32(PDA_PDA_DMA_EN_REG, pda_PdaConfig.PDA_PDA_DMA_EN);
	PDA_WR32(PDA_PDA_DMA_RST_REG, pda_PdaConfig.PDA_PDA_DMA_RST);
	PDA_WR32(PDA_PDA_DMA_TOP_REG, pda_PdaConfig.PDA_PDA_DMA_TOP);
	PDA_WR32(PDA_PDA_SECURE_REG, pda_PdaConfig.PDA_PDA_SECURE);
	PDA_WR32(PDA_PDA_TILE_STATUS_REG, pda_PdaConfig.PDA_PDA_TILE_STATUS);
	PDA_WR32(PDA_PDA_DCM_DIS_REG, pda_PdaConfig.PDA_PDA_DCM_DIS);
	PDA_WR32(PDA_PDA_DCM_ST_REG, pda_PdaConfig.PDA_PDA_DCM_ST);
	PDA_WR32(PDA_PDAI_P1_ERR_STAT_REG, pda_PdaConfig.PDA_PDAI_P1_ERR_STAT);
	PDA_WR32(PDA_PDATI_P1_ERR_STAT_REG, pda_PdaConfig.PDA_PDATI_P1_ERR_STAT);
	PDA_WR32(PDA_PDAI_P2_ERR_STAT_REG, pda_PdaConfig.PDA_PDAI_P2_ERR_STAT);
	PDA_WR32(PDA_PDATI_P2_ERR_STAT_REG, pda_PdaConfig.PDA_PDATI_P2_ERR_STAT);
	PDA_WR32(PDA_PDAO_P1_ERR_STAT_REG, pda_PdaConfig.PDA_PDAO_P1_ERR_STAT);
	PDA_WR32(PDA_PDA_ERR_STAT_EN_REG, pda_PdaConfig.PDA_PDA_ERR_STAT_EN);
	PDA_WR32(PDA_PDA_ERR_STAT_REG, pda_PdaConfig.PDA_PDA_ERR_STAT);
	PDA_WR32(PDA_PDA_TOP_CTL_REG, pda_PdaConfig.PDA_PDA_TOP_CTL);
	PDA_WR32(PDA_PDA_DEBUG_SEL_REG, pda_PdaConfig.PDA_PDA_DEBUG_SEL);
	PDA_WR32(PDA_PDA_IRQ_TRIG_REG, pda_PdaConfig.PDA_PDA_IRQ_TRIG);
	PDA_WR32(PDA_PDA_SPARE1_REG, pda_PdaConfig.PDA_PDA_SPARE1);
	PDA_WR32(PDA_PDA_SPARE2_REG, pda_PdaConfig.PDA_PDA_SPARE2);
	PDA_WR32(PDA_PDA_SPARE3_REG, pda_PdaConfig.PDA_PDA_SPARE3);
	PDA_WR32(PDA_PDA_SPARE4_REG, pda_PdaConfig.PDA_PDA_SPARE4);
	PDA_WR32(PDA_PDA_SPARE5_REG, pda_PdaConfig.PDA_PDA_SPARE5);
	PDA_WR32(PDA_PDA_SPARE6_REG, pda_PdaConfig.PDA_PDA_SPARE6);
	PDA_WR32(PDA_PDA_SPARE7_REG, pda_PdaConfig.PDA_PDA_SPARE7);
}

static void pda_execute(void)
{
	//PDA_TOP_CTL set 1'b1 to bit3, to load register from double buffer
	PDA_WR32(PDA_PDA_TOP_CTL_REG, PDA_DOUBLE_BUFFER);

	//PDA_TOP_CTL set 1'b1 to bit1, to trigger sof
	PDA_WR32(PDA_PDA_TOP_CTL_REG, PDA_TRIGGER);

#ifdef GET_PDA_TIME
	//for compute pda process time
	do_gettimeofday(&time_begin);
#endif
}

static inline unsigned int pda_ms_to_jiffies(unsigned int ms)
{
	return ((ms * HZ + 512) >> 10);
}

static signed int pda_wait_irq(struct PDA_WAIT_IRQ_STRUCT *wait_irq)
{
	int ret = 0;

	/* start to wait signal */
	ret = wait_event_interruptible_timeout(g_wait_queue_head,
						g_HWstatus != 0,
						pda_ms_to_jiffies(wait_irq->Timeout));

	if (ret == 0) {
		//time out error
		LOG_INF("wait_event_interruptible_timeout Fail");
		wait_irq->Status = g_HWstatus;
		return -1;
	}

#ifdef GET_PDA_TIME
	//for compute pda process time and kernel driver process time
	do_gettimeofday(&time_end);
#endif

	if (g_HWstatus < 0)
		LOG_INF("PDA HW error");
	else
		LOG_INF("PDA HW done");

	//irq done
	wait_irq->Status = g_HWstatus;
	return ret;
}

static irqreturn_t pda_irqhandle(signed int Irq, void *DeviceId)
{
	unsigned int nPdaStatus = 0;

	//read pda status
	nPdaStatus = PDA_RD32(PDA_PDA_ERR_STAT_REG) & PDA_STATUS_REG;

	if (nPdaStatus == PDA_DONE) {
		g_HWstatus = 1;

#ifdef GET_PDA_TIME
		//for compute pda process time
		do_gettimeofday(&pda_done_time_end);
#endif
	} else if (nPdaStatus == PDA_ERROR) {
		//reset flow
		pda_reset();
		g_HWstatus = -1;
	} else {
		//reserve
		g_HWstatus = 0;
	}

	//wake up user space WAIT_IRQ flag
	wake_up_interruptible(&g_wait_queue_head);
	return IRQ_HANDLED;
}

static long PDA_Ioctl(struct file *a_pstFile, unsigned int a_u4Command, unsigned long a_u4Param)
{
	long nRet = 0;
	struct PDA_Config pda_PdaConfig;
	struct PDA_WAIT_IRQ_STRUCT pda_irq_info;

#ifdef multi_frame_rst
	static int nFrame_no;

	nFrame_no = 0;
#endif

	if (g_u4EnableClockCount == 0) {
		LOG_INF("Cannot process without enable pda clock\n");
		return -1;
	}

	switch (a_u4Command) {
	case PDA_RESET:
		pda_reset();
		break;

	case PDA_ENQUE:

#ifdef GET_PDA_TIME
		//compute total kernel driver process time
		do_gettimeofday(&total_time_begin);
#endif

		//reset HW status
		g_HWstatus = 0;

		if (copy_from_user(&pda_PdaConfig,
				   (void *)a_u4Param,
				   sizeof(struct PDA_Config)) == 0) {

#ifdef GET_PDA_TIME
			//compute config time
			do_gettimeofday(&Config_time_begin);
#endif

			// Fill PDA register config
			FillRegSettings(pda_PdaConfig);

#ifdef GET_PDA_TIME
			//compute config time
			do_gettimeofday(&Config_time_end);
#endif

			// trigger PDA work
			pda_execute();

#ifdef multi_frame_rst
			if (nFrame_no == 1)
				pda_reset();
			nFrame_no++;
			nFrame_no %= 3;
#endif
		} else {
			LOG_INF("PDA_ENQUE copy_from_user failed\n");
		}
		break;
	case PDA_DEQUE:
		//reserve
		break;
	case PDA_WAIT_IRQ:

		if (copy_from_user(&pda_irq_info,
				   (void *)a_u4Param,
				   sizeof(struct PDA_WAIT_IRQ_STRUCT)) == 0) {

			nRet = pda_wait_irq(&pda_irq_info);
			if (nRet < 0)
				LOG_INF("pda_wait_irq Fail (%d)\n", nRet);

			//write 0 after trigger
			PDA_WR32(PDA_PDA_TOP_CTL_REG, PDA_CLEAR_REG);

			if (copy_to_user((void *)a_u4Param,
			    &pda_irq_info,
			    sizeof(struct PDA_WAIT_IRQ_STRUCT)) != 0) {
				LOG_INF("copy_to_user failed\n");
				nRet = -EFAULT;
			}
		} else {
			LOG_INF("PDA_WAIT_IRQ copy_from_user failed");
			nRet = -EFAULT;
		}

#ifdef GET_PDA_TIME
		//compute total kernel driver process time
		do_gettimeofday(&total_time_end);

		//show all of time log here //
		LOG_INF("time_begin time: %d usec", time_begin.tv_usec);
		LOG_INF("time_end time: %d usec", time_end.tv_usec);
		LOG_INF("pda execute time: (%d) usec",
			(time_end.tv_usec - time_begin.tv_usec));
		LOG_INF("pda_done_time_end time: %d usec", pda_done_time_end.tv_usec);
		LOG_INF("Fill register setting, cost time: (%d) usec",
			(Config_time_end.tv_usec-Config_time_begin.tv_usec));
		LOG_INF("total_time_begin time: %d", total_time_begin.tv_usec);
		LOG_INF("total_time_end time: %d", total_time_end.tv_usec);
		LOG_INF("kernel driver total cost time: (%d) usec",
			(total_time_end.tv_usec-total_time_begin.tv_usec));
		LOG_INF("Config_time_begin time: %d", Config_time_begin.tv_usec);
		LOG_INF("Config_time_end time: %d", Config_time_end.tv_usec);
#endif
		break;
	default:
		LOG_INF("Unknown Cmd(%d)\n", a_u4Command);
		break;
	}
	return nRet;
}

#ifdef CONFIG_COMPAT
static long PDA_Ioctl_Compat(struct file *a_pstFile, unsigned int a_u4Command,
			    unsigned long a_u4Param)
{
	long i4RetValue = 0;
	return i4RetValue;
}
#endif

static int PDA_Open(struct inode *a_pstInode, struct file *a_pstFile)
{
	//Enable clock
	EnableClock(MTRUE);

	LOG_INF("PDA open g_u4EnableClockCount: %d", g_u4EnableClockCount);
	return 0;
}

static int PDA_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	//Disable clock
	EnableClock(MFALSE);
	LOG_INF("PDA release g_u4EnableClockCount: %d", g_u4EnableClockCount);
	return 0;
}

/*******************************************************************************
 *
 ******************************************************************************/

static dev_t g_PDA_devno;
static struct cdev *g_pPDA_CharDrv;
static struct class *actuator_class;
static struct device *lens_device;

static const struct file_operations g_stPDA_fops = {
	.owner = THIS_MODULE,
	.open = PDA_Open,
	.release = PDA_Release,
	.unlocked_ioctl = PDA_Ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = PDA_Ioctl_Compat,
#endif
};

static inline int PDA_RegCharDev(void)
{
	int nRet = 0;

	LOG_INF("Register char driver Start\n");

	/* Allocate char driver no. */
	nRet = alloc_chrdev_region(&g_PDA_devno, 0, 1, PDA_DEV_NAME);
	if (nRet < 0) {
		LOG_INF("Allocate device no failed\n");
		return nRet;
	}

	/* Allocate driver */
	g_pPDA_CharDrv = cdev_alloc();
	if (g_pPDA_CharDrv == NULL) {
		unregister_chrdev_region(g_PDA_devno, 1);
		LOG_INF("cdev_alloc failed\n");
		return nRet;
	}

	/* Attatch file operation. */
	cdev_init(g_pPDA_CharDrv, &g_stPDA_fops);

	g_pPDA_CharDrv->owner = THIS_MODULE;

	/* Add to system */
	nRet = cdev_add(g_pPDA_CharDrv, g_PDA_devno, 1);
	if (nRet < 0) {
		LOG_INF("Attatch file operation failed\n");
		unregister_chrdev_region(g_PDA_devno, 1);
		return nRet;
	}

	actuator_class = class_create(THIS_MODULE, "PDAdrv");
	if (IS_ERR(actuator_class)) {
		int ret = PTR_ERR(actuator_class);

		LOG_INF("Unable to create class, err = %d\n", ret);
		//unregister_chrdev_region(g_PDA_devno, 1);
		return ret;
	}

	lens_device = device_create(actuator_class, NULL, g_PDA_devno, NULL,
				    PDA_DEV_NAME);

	if (IS_ERR(lens_device)) {
		int ret = PTR_ERR(lens_device);

		LOG_INF("create dev err: /dev/%s, err = %d\n", PDA_DEV_NAME, ret);
		//unregister_chrdev_region(g_PDA_devno, 1);
		return ret;
	}

	LOG_INF("Register char driver End\n");
	return nRet;
}

static inline void PDA_UnRegCharDev(void)
{
	LOG_INF("UnRegCharDev Start\n");

	/* Release char driver */
	cdev_del(g_pPDA_CharDrv);

	unregister_chrdev_region(g_PDA_devno, 1);

	device_destroy(actuator_class, g_PDA_devno);

	class_destroy(actuator_class);

	LOG_INF("UnRegCharDev End\n");
}

/*******************************************************************************
 *
 ******************************************************************************/
static int PDA_probe(struct platform_device *pDev)
{
	int nRet = 0;
	int nIrq = 0;
	int nIrqSecond = 0;
	unsigned int irq_info[3];		/* Record interrupts info from device tree */
	struct device_node *node;		//for test get node
	struct device_node *camsys_node;

	LOG_INF("probe Start\n");

	/* Register char driver */
	nRet = PDA_RegCharDev();
	if (nRet < 0) {
		LOG_INF(" register char device failed!\n");
		return nRet;
	}

	LOG_INF("probe - register char driver\n");

	spin_lock_init(&g_PDA_SpinLock);
	LOG_INF("spin_lock_init done\n");

	init_waitqueue_head(&g_wait_queue_head);
	LOG_INF("init_waitqueue_head done\n");

#ifdef CONFIG_OF
	//power on smi
	/* consumer driver probe*/
	pm_runtime_enable(&pDev->dev); //Note: It‘s not larb's device.
	LOG_INF("pm_runtime_enable done\n");

	/* consumer device starting work*/
	pm_runtime_get_sync(&pDev->dev); //Note: It‘s not larb's device.
	LOG_INF("pm_runtime_get_sync done\n");
#endif

	//PDA node
	node = of_find_compatible_node(NULL, NULL, "mediatek,camera-pda");
	if (!node) {
		LOG_INF("find camera-pda node failed\n");
		return -1;
	}
	LOG_INF("find camera-pda node done\n");

	m_pda_base = of_iomap(node, 0);
	if (!m_pda_base)
		LOG_INF("base m_pda_base failed\n");
	LOG_INF("of_iomap m_pda_base done (0x%x)\n", m_pda_base);

	//camsys node
#ifdef FPGA_UT
	camsys_node = of_find_compatible_node(NULL, NULL, "mediatek,isp_unit_test");
#else
	camsys_node = of_find_compatible_node(NULL, NULL, "mediatek,mt6877-camsys_main");
#endif
	if (!camsys_node) {
		LOG_INF("find camsys_config node failed\n");
		return -1;
	}
	LOG_INF("find camsys_config node done\n");

#ifdef FPGA_UT
	CAMSYS_CONFIG_BASE = of_iomap(camsys_node, 2);
#else
	CAMSYS_CONFIG_BASE = of_iomap(camsys_node, 0);
#endif
	if (!CAMSYS_CONFIG_BASE)
		LOG_INF("base CAMSYS_CONFIG_BASE failed\n");
	LOG_INF("of_iomap CAMSYS_CONFIG_BASE done (0x%x)\n", CAMSYS_CONFIG_BASE);

	//CCF: Grab clock pointer (struct clk*)
	pda_clk.CG_PDA_TOP_MUX = devm_clk_get(&pDev->dev, "PDA_TOP_MUX");
	if (IS_ERR(pda_clk.CG_PDA_TOP_MUX)) {
		LOG_INF("cannot get CG_PDA_TOP_MUX clock\n");
		return PTR_ERR(pda_clk.CG_PDA_TOP_MUX);
	}

	/* get IRQ ID and request IRQ */
	nIrq = irq_of_parse_and_map(node, 0);

	LOG_INF("PDA_dev->irq: %d", nIrq);

	LOG_INF("node->name: %s\n", node->name);

	if (nIrq > 0) {
		/* Get IRQ Flag from device node */
		nIrqSecond = of_property_read_u32_array(node,
							"interrupts",
							irq_info,
							ARRAY_SIZE(irq_info));

		if (nIrqSecond) {
			LOG_INF("get irq flags from DTS fail!!\n");
			return -ENODEV;
		}

		nRet = request_irq(nIrq,
				(irq_handler_t) pda_irqhandle,
				irq_info[2],
				(const char *)node->name,
				NULL);

		if (nRet) {
			LOG_INF("request_irq Fail: %d\n", nRet);
			return nRet;
		}
	} else {
		LOG_INF("get IRQ ID Fail or No IRQ: %d\n", nIrq);
	}

	LOG_INF("Attached!!\n");
	LOG_INF("probe End\n");

	return nRet;
}

static int PDA_remove(struct platform_device *pdev)
{
	PDA_UnRegCharDev();
	pm_runtime_disable(&pdev->dev);
	return 0;
}

static int PDA_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int PDA_resume(struct platform_device *pdev)
{
	return 0;
}

//////////////////////////////////////// PDA driver ////////////////////////////////////

#ifdef CONFIG_OF
static const struct of_device_id gpda_of_device_id[] = {
	{.compatible = "mediatek,camera-pda",},
	{}
};
#endif

MODULE_DEVICE_TABLE(of, gpda_of_device_id);

static struct platform_driver PDADriver = {
	.probe = PDA_probe,
	.remove = PDA_remove,
	.suspend = PDA_suspend,
	.resume = PDA_resume,
	.driver = {
		   .name = PDA_DEV_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(gpda_of_device_id),
#endif
		}
};

/******************************************************************************
 *
 ******************************************************************************/
module_platform_driver(PDADriver);
MODULE_DESCRIPTION("Camera PDA driver");
MODULE_AUTHOR("MM6SW3");
MODULE_LICENSE("GPL");
