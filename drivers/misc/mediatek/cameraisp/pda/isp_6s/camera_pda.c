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

#ifdef CONFIG_MTK_IOMMU_V2
#include <mach/mt_iommu.h>
#include "mach/pseudo_m4u.h"
#else /* CONFIG_MTK_IOMMU_V2 */
#include <m4u.h>
#endif /* CONFIG_MTK_IOMMU_V2 */


//#define FPGA_UT
//#define GET_PDA_TIME
//#define FOR_DEBUG

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

#ifdef FPGA_UT
void __iomem *CAMSYS_CONFIG_BASE;
#define CAMSYS_MAIN_BASE_ADDR CAMSYS_CONFIG_BASE
#define REG_CAMSYS_CG_SET               (CAMSYS_MAIN_BASE_ADDR + 0x4)
#define REG_CAMSYS_CG_CLR               (CAMSYS_MAIN_BASE_ADDR + 0x8)
#endif

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

struct device *g_dev;

static spinlock_t g_PDA_SpinLock;

wait_queue_head_t g_wait_queue_head;

int g_HWstatus;

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

//calculate 1024 roi data
unsigned int g_rgn_x_buf[45];
unsigned int g_rgn_y_buf[45];
unsigned int g_rgn_h_buf[45];
unsigned int g_rgn_w_buf[45];
unsigned int g_rgn_iw_buf[45];

static inline void PDA_Prepare_Enable_ccf_clock(void)
{
	int ret;

	LOG_INF("clock begin");

#ifdef CONFIG_OF
	/* consumer device starting work*/
	pm_runtime_get_sync(g_dev); //Note: It‘s not larb's device.
	LOG_INF("pm_runtime_get_sync done\n");
#endif

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

	smi_bus_disable_unprepare(SMI_LARB13, PDA_DEV_NAME);

#ifdef CONFIG_OF
	/* consumer device starting work*/
	pm_runtime_put(g_dev); //Note: It‘s not larb's device.
	LOG_INF("pm_runtime_put done\n");
#endif
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
		if (g_u4EnableClockCount == 1) {
			ret = m4u_control_iommu_port();
			if (ret)
				LOG_INF("cannot config M4U IOMMU PORTS\n");
		}
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
	int nResetCount = 0;

	//reset HW status
	g_HWstatus = 0;

	//make reset
	PDA_WR32(PDA_PDA_DMA_RST_REG, PDA_MAKE_RESET);

	//read reset status
	while ((PDA_RD32(PDA_PDA_DMA_RST_REG) & MASK_BIT_ZERO) != PDA_RESET_VALUE) {
		if (nResetCount > 30) {
			LOG_INF("PDA reset fail\n");
			break;
		}
		LOG_INF("Wait EMI done\n");
		nResetCount++;
	}

	//equivalent to hardware reset
	PDA_WR32(PDA_PDA_TOP_CTL_REG, PDA_HW_RESET);

	//clear reset signal
	PDA_WR32(PDA_PDA_DMA_RST_REG, PDA_CLEAR_REG);

	//clear hardware reset signal
	PDA_WR32(PDA_PDA_TOP_CTL_REG, PDA_CLEAR_REG);
}

static void HWDMASettings(struct PDA_Data_t *pda_PdaConfig)
{
	//--------- Frame setting part -----------
	PDA_WR32(PDA_CFG_0_REG, pda_PdaConfig->PDA_FrameSetting.PDA_CFG_0.Raw);
	PDA_WR32(PDA_CFG_1_REG, pda_PdaConfig->PDA_FrameSetting.PDA_CFG_1.Raw);
	//need set roi number every process
	//PDA_WR32(PDA_CFG_2_REG, pda_PdaConfig->PDA_FrameSetting.PDA_CFG_2.Raw);
	PDA_WR32(PDA_CFG_3_REG, pda_PdaConfig->PDA_FrameSetting.PDA_CFG_3.Raw);
	PDA_WR32(PDA_CFG_4_REG, pda_PdaConfig->PDA_FrameSetting.PDA_CFG_4.Raw);
	PDA_WR32(PDA_CFG_5_REG, pda_PdaConfig->PDA_FrameSetting.PDA_CFG_5.Raw);
	PDA_WR32(PDA_CFG_6_REG, pda_PdaConfig->PDA_FrameSetting.PDA_CFG_6.Raw);
	PDA_WR32(PDA_CFG_7_REG, pda_PdaConfig->PDA_FrameSetting.PDA_CFG_7.Raw);
	PDA_WR32(PDA_CFG_8_REG, pda_PdaConfig->PDA_FrameSetting.PDA_CFG_8.Raw);
	PDA_WR32(PDA_CFG_9_REG, pda_PdaConfig->PDA_FrameSetting.PDA_CFG_9.Raw);
	PDA_WR32(PDA_CFG_10_REG, pda_PdaConfig->PDA_FrameSetting.PDA_CFG_10.Raw);
	PDA_WR32(PDA_CFG_11_REG, pda_PdaConfig->PDA_FrameSetting.PDA_CFG_11.Raw);
	PDA_WR32(PDA_CFG_12_REG, pda_PdaConfig->PDA_FrameSetting.PDA_CFG_12.Raw);
	PDA_WR32(PDA_CFG_13_REG, pda_PdaConfig->PDA_FrameSetting.PDA_CFG_13.Raw);

	//--------- Input buffer address -------------
	PDA_WR32(PDA_PDAI_P1_BASE_ADDR_REG, pda_PdaConfig->PDA_PDAI_P1_BASE_ADDR);
	PDA_WR32(PDA_PDATI_P1_BASE_ADDR_REG, pda_PdaConfig->PDA_PDATI_P1_BASE_ADDR);
	PDA_WR32(PDA_PDAI_P2_BASE_ADDR_REG, pda_PdaConfig->PDA_PDAI_P2_BASE_ADDR);
	PDA_WR32(PDA_PDATI_P2_BASE_ADDR_REG, pda_PdaConfig->PDA_PDATI_P2_BASE_ADDR);

	//--------- DMA Secure part -------------
	PDA_WR32(PDA_PDA_SECURE_REG, 0x00000000);

	//--------- config setting hard code part --------------
	PDA_WR32(PDA_PDAI_STRIDE_REG, 0x00000240);
	PDA_WR32(PDA_PDAI_P1_CON0_REG, 0x80000134);
	PDA_WR32(PDA_PDAI_P1_CON1_REG, 0x004d004d);
	PDA_WR32(PDA_PDAI_P1_CON2_REG, 0x109a009a);
	PDA_WR32(PDA_PDAI_P1_CON3_REG, 0x80e700e7);
	PDA_WR32(PDA_PDAI_P1_CON4_REG, 0x809a009a);
	PDA_WR32(PDA_PDATI_P1_CON0_REG, 0x8000004c);
	PDA_WR32(PDA_PDATI_P1_CON1_REG, 0x00130013);
	PDA_WR32(PDA_PDATI_P1_CON2_REG, 0x10260026);
	PDA_WR32(PDA_PDATI_P1_CON3_REG, 0x80390039);
	PDA_WR32(PDA_PDATI_P1_CON4_REG, 0x80260026);
	PDA_WR32(PDA_PDAI_P2_CON0_REG, 0x80000134);
	PDA_WR32(PDA_PDAI_P2_CON1_REG, 0x004d004d);
	PDA_WR32(PDA_PDAI_P2_CON2_REG, 0x009a009a);
	PDA_WR32(PDA_PDAI_P2_CON3_REG, 0x80e700e7);
	PDA_WR32(PDA_PDAI_P2_CON4_REG, 0x009a009a);
	PDA_WR32(PDA_PDATI_P2_CON0_REG, 0x8000004c);
	PDA_WR32(PDA_PDATI_P2_CON1_REG, 0x10130013);
	PDA_WR32(PDA_PDATI_P2_CON2_REG, 0x10260026);
	PDA_WR32(PDA_PDATI_P2_CON3_REG, 0x80390039);
	PDA_WR32(PDA_PDATI_P2_CON4_REG, 0x00260026);
	PDA_WR32(PDA_PDAO_P1_XSIZE_REG, 0x0000057f);
	PDA_WR32(PDA_PDAO_P1_CON0_REG, 0x80000040);
	PDA_WR32(PDA_PDAO_P1_CON1_REG, 0x10100010);
	PDA_WR32(PDA_PDAO_P1_CON2_REG, 0x10200020);
	PDA_WR32(PDA_PDAO_P1_CON3_REG, 0x00300030);
	PDA_WR32(PDA_PDAO_P1_CON4_REG, 0x80200020);
	PDA_WR32(PDA_PDA_DMA_EN_REG, 0x0000001f);
	PDA_WR32(PDA_PDA_DMA_RST_REG, 0x00000000);
	PDA_WR32(PDA_PDA_DMA_TOP_REG, 0x00000002);
	//PDA_WR32(PDA_PDA_SECURE_REG, pda_PdaConfig->PDA_PDA_SECURE);
	PDA_WR32(PDA_PDA_TILE_STATUS_REG, 0x00000000);
	PDA_WR32(PDA_PDA_DCM_DIS_REG, 0x00000000);
	PDA_WR32(PDA_PDA_DCM_ST_REG, 0x00000000);
	PDA_WR32(PDA_PDAI_P1_ERR_STAT_REG, 0x00000000);
	PDA_WR32(PDA_PDATI_P1_ERR_STAT_REG, 0x00000000);
	PDA_WR32(PDA_PDAI_P2_ERR_STAT_REG, 0x00000000);
	PDA_WR32(PDA_PDATI_P2_ERR_STAT_REG, 0x00000000);
	PDA_WR32(PDA_PDAO_P1_ERR_STAT_REG, 0x00000000);
	PDA_WR32(PDA_PDA_ERR_STAT_EN_REG, 0x00000003);
	PDA_WR32(PDA_PDA_TOP_CTL_REG, 0x00000000);
	PDA_WR32(PDA_PDA_DEBUG_SEL_REG, 0x00000000);
	PDA_WR32(PDA_PDA_IRQ_TRIG_REG, 0x00000000);
	PDA_WR32(PDA_PDA_SPARE1_REG, 0x00000000);
	PDA_WR32(PDA_PDA_SPARE2_REG, 0x00000000);
	PDA_WR32(PDA_PDA_SPARE3_REG, 0x00000000);
	PDA_WR32(PDA_PDA_SPARE4_REG, 0x00000000);
	PDA_WR32(PDA_PDA_SPARE5_REG, 0x00000000);
	PDA_WR32(PDA_PDA_SPARE6_REG, 0x00000000);
	PDA_WR32(PDA_PDA_SPARE7_REG, 0x00000000);

	// read 0x3b4, avoid the impact of previous data
	PDA_RD32(PDA_PDA_ERR_STAT_REG);
}

static int ProcessROIData(struct PDA_Data_t *pda_data,
				unsigned int RoiProcNum,
				unsigned int ROIIndex,
				unsigned int nNumerousROI)
{
	int i = 0, j = 0;
	unsigned int xnum = 0, ynum = 0;
	unsigned int woverlap = 0, hoverlap = 0;
	unsigned int x0, y0, w0, h0;
	unsigned int nWidth = 0, nHeight = 0;
	int nLocalBufIndex = 0;
	int nXDirLoopCount = 0;

	if (nNumerousROI == 1) {

		xnum = pda_data->xnum;
		ynum = pda_data->ynum;
		woverlap = (int)pda_data->woverlap;
		hoverlap = (int)pda_data->hoverlap;
		x0 = pda_data->rgn_x[0];
		y0 = pda_data->rgn_y[0];
		w0 = pda_data->rgn_w[0];
		h0 = pda_data->rgn_h[0];
		nWidth = w0 / xnum;
		nHeight = h0 / ynum;

		if (xnum <= 0 || ynum <= 0) {
			LOG_INF("xnum(%d) or ynum(%d) value is invalid\n", xnum, ynum);
			return -1;
		}

		if (w0 < xnum || h0 < ynum) {
			LOG_INF("w0(%d)/h0(%d) can't less than xnum(%d)/ynum(%d)\n",
				w0, h0, xnum, ynum);
			return -1;
		}

		for (j = (ROIIndex / xnum); j < ynum; j++) {
			i = (nXDirLoopCount == 0 ? (ROIIndex % xnum) : 0);
			for (; i < xnum; i++) {
				nLocalBufIndex = j * xnum + i - ROIIndex;

				if (nLocalBufIndex >= 45 || nLocalBufIndex < 0) {
					LOG_INF("nLocalBufIndex out of range (%d)\n",
						nLocalBufIndex);
					return -1;
				}

				if (i != 0 && i != xnum - 1) {
					g_rgn_x_buf[nLocalBufIndex] =
						x0 + nWidth * i - nWidth * woverlap / 100;
					g_rgn_w_buf[nLocalBufIndex] =
						nWidth + 2 * nWidth * woverlap / 100;
				} else if (i == 0) {
					g_rgn_x_buf[nLocalBufIndex] = x0 + nWidth * i;
					g_rgn_w_buf[nLocalBufIndex] =
						nWidth + nWidth * woverlap / 100;
				} else {
					g_rgn_x_buf[nLocalBufIndex] =
						x0 + nWidth * i - nWidth * woverlap / 100;
					g_rgn_w_buf[nLocalBufIndex] =
						nWidth + nWidth * woverlap / 100;
				}

				if (j != 0 && j != ynum - 1) {
					g_rgn_y_buf[nLocalBufIndex] =
						y0 + nHeight * j - nHeight * hoverlap / 100;
					g_rgn_h_buf[nLocalBufIndex] =
						nHeight + 2 * nHeight * hoverlap / 100;
				} else if (j == 0) {
					g_rgn_y_buf[nLocalBufIndex] = y0 + nHeight * j;
					g_rgn_h_buf[nLocalBufIndex] =
						nHeight + nHeight * hoverlap / 100;
				} else {
					g_rgn_y_buf[nLocalBufIndex] =
						y0 + nHeight * j - nHeight * hoverlap / 100;
					g_rgn_h_buf[nLocalBufIndex] =
						nHeight + nHeight * hoverlap / 100;
				}

				g_rgn_iw_buf[nLocalBufIndex] =
					g_rgn_w_buf[nLocalBufIndex] *
					pda_data->PDA_FrameSetting.PDA_CFG_1.Bits.PDA_PAT_WIDTH;

				//calculate done
				if (nLocalBufIndex >= (RoiProcNum-1))
					goto CALDONE;
			}	//for i

			//use for check if first x direction loop or not
			nXDirLoopCount++;
		}	//for j

CALDONE:
		LOG_INF("Calculate ROI done\n");
	} else {
		for (i = 0; i < RoiProcNum; i++) {
			g_rgn_x_buf[i] = pda_data->rgn_x[ROIIndex+i];
			g_rgn_y_buf[i] = pda_data->rgn_y[ROIIndex+i];
			g_rgn_w_buf[i] = pda_data->rgn_w[ROIIndex+i];
			g_rgn_h_buf[i] = pda_data->rgn_h[ROIIndex+i];
			g_rgn_iw_buf[i] = pda_data->rgn_iw[ROIIndex+i];
		}
	}

	return 0;
}

static int CheckDesignLimitation(struct PDA_Data_t *PDA_Data,
				unsigned int RoiProcNum,
				unsigned int ROIIndex)
{
	int i = 0;
	int nROIIndex = 0;
	int nTempVar = 0;

	//frame constraint
	if (PDA_Data->PDA_FrameSetting.PDA_CFG_0.Bits.PDA_WIDTH % 4 != 0) {
		LOG_INF("Frame width must be multiple of 4\n");
		PDA_Data->Status = -4;
		return -1;
	}

	//ROI constraint
	for (i = 0; i < RoiProcNum; i++) {
		nROIIndex = i + ROIIndex;

		if (g_rgn_w_buf[i] % 4 != 0) {
			LOG_INF("ROI_%d width(%d) must be multiple of 4\n",
				nROIIndex,
				g_rgn_w_buf[i]);
			PDA_Data->Status = -5;
			return -1;
		}

		if (g_rgn_w_buf[i] > 3280) {
			LOG_INF("ROI_%d width(%d) must be less than 3280\n",
				nROIIndex,
				g_rgn_w_buf[i]);
			PDA_Data->Status = -6;
			return -1;
		}

		if (g_rgn_x_buf[i] % 2 != 0) {
			LOG_INF("ROI_%d xoffset(%d) must be multiple of 2\n",
				nROIIndex,
				g_rgn_x_buf[i]);
			PDA_Data->Status = -7;
			return -1;
		}

		if (g_rgn_iw_buf[i] < 41) {
			LOG_INF("ROI_%d IW(%d) must be greater than 41\n",
				nROIIndex,
				g_rgn_iw_buf[i]);
			PDA_Data->Status = -8;
			return -1;
		}

		nTempVar = (1 << PDA_Data->PDA_FrameSetting.PDA_CFG_1.Bits.PDA_BIN_FCTR);
		if (g_rgn_h_buf[i] % nTempVar != 0) {
			LOG_INF("ROI_%d height(%d) must be multiple of Binning number(%d)\n",
				i,
				g_rgn_h_buf[i],
				nTempVar);
			PDA_Data->Status = -9;
			return -1;
		}

		//ROI boundary must be on the boundary of patch
		nTempVar = PDA_Data->PDA_FrameSetting.PDA_CFG_1.Bits.PDA_PR_XNUM;
		if (g_rgn_x_buf[i] % nTempVar != 0) {
			LOG_INF("ROI_%d ROI boundary must be on the boundary of patch\n",
				nROIIndex);
			LOG_INF("ROI_%d_x: %d, PDA_PR_XNUM: %d\n",
				nROIIndex,
				g_rgn_x_buf[i],
				nTempVar);
			PDA_Data->Status = -10;
			return -1;
		}

		if (g_rgn_w_buf[i] % nTempVar != 0) {
			LOG_INF("ROI_%d ROI boundary must be on the boundary of patch\n",
				nROIIndex);
			LOG_INF("ROI_%d_w: %d, PDA_PR_XNUM: %d\n",
				nROIIndex,
				g_rgn_w_buf[i],
				nTempVar);
			PDA_Data->Status = -10;
			return -1;
		}

		nTempVar = PDA_Data->PDA_FrameSetting.PDA_CFG_1.Bits.PDA_PR_YNUM;
		if (g_rgn_y_buf[i] % nTempVar != 0) {
			LOG_INF("ROI_%d ROI boundary must be on the boundary of patch\n",
				nROIIndex);
			LOG_INF("ROI_%d_y: %d, PDA_PR_YNUM: %d\n",
				nROIIndex,
				g_rgn_y_buf[i],
				nTempVar);
			PDA_Data->Status = -10;
			return -1;
		}

		if (g_rgn_h_buf[i] % nTempVar != 0) {
			LOG_INF("ROI_%d ROI boundary must be on the boundary of patch\n",
				nROIIndex);
			LOG_INF("ROI_%d_h: %d, PDA_PR_YNUM: %d\n",
				nROIIndex,
				g_rgn_h_buf[i],
				nTempVar);
			PDA_Data->Status = -10;
			return -1;
		}

		//ROI can't exceed the image region
		nTempVar = PDA_Data->PDA_FrameSetting.PDA_CFG_0.Bits.PDA_WIDTH;
		if (g_rgn_x_buf[i] < 0 || (g_rgn_x_buf[i]+g_rgn_w_buf[i]) > nTempVar) {
			LOG_INF("ROI_%d ROI exceed the image region\n", nROIIndex);
			PDA_Data->Status = -11;
			return -1;
		}

		nTempVar = PDA_Data->PDA_FrameSetting.PDA_CFG_0.Bits.PDA_HEIGHT;
		if (g_rgn_y_buf[i] < 0 || (g_rgn_y_buf[i]+g_rgn_h_buf[i]) > nTempVar) {
			LOG_INF("ROI_%d ROI exceed the image region\n", nROIIndex);
			PDA_Data->Status = -11;
			return -1;
		}

		//Register Range Limitation check
		if (g_rgn_x_buf[i] > 8191) {
			LOG_INF("ROI_X_%d (%d) out of range\n", nROIIndex, g_rgn_x_buf[i]);
			PDA_Data->Status = -19;
			return -1;
		}

		if (g_rgn_y_buf[i] > 8191) {
			LOG_INF("ROI_Y_%d (%d) out of range\n", nROIIndex, g_rgn_y_buf[i]);
			PDA_Data->Status = -20;
			return -1;
		}

		if (g_rgn_w_buf[i] < 20) {
			LOG_INF("ROI_W_%d (%d) out of range\n", nROIIndex, g_rgn_w_buf[i]);
			PDA_Data->Status = -21;
			return -1;
		}

		if (g_rgn_h_buf[i] < 4 || g_rgn_h_buf[i] > 4092) {
			LOG_INF("ROI_H_%d (%d) out of range\n", nROIIndex, g_rgn_h_buf[i]);
			PDA_Data->Status = -22;
			return -1;
		}

		if (g_rgn_iw_buf[i] > 3280) {
			LOG_INF("ROI_IW_%d (%d) out of range\n", nROIIndex, g_rgn_iw_buf[i]);
			PDA_Data->Status = -23;
			return -1;
		}
	}

	// Register Range Limitation check
	nTempVar = PDA_Data->PDA_FrameSetting.PDA_CFG_0.Bits.PDA_WIDTH;
	if (nTempVar < 20 || nTempVar > 8191) {
		LOG_INF("Frame width (%d) out of range\n", nTempVar);
		PDA_Data->Status = -12;
		return -1;
	}

	nTempVar = PDA_Data->PDA_FrameSetting.PDA_CFG_0.Bits.PDA_HEIGHT;
	if (nTempVar < 4 || nTempVar > 8191) {
		LOG_INF("Frame height (%d) out of range\n", nTempVar);
		PDA_Data->Status = -13;
		return -1;
	}

	nTempVar = PDA_Data->PDA_FrameSetting.PDA_CFG_1.Bits.PDA_PR_XNUM;
	if (nTempVar < 1 || nTempVar > 16) {
		LOG_INF("PDA_PR_XNUM (%d) out of range\n", nTempVar);
		PDA_Data->Status = -14;
		return -1;
	}

	nTempVar = PDA_Data->PDA_FrameSetting.PDA_CFG_1.Bits.PDA_PR_YNUM;
	if (nTempVar < 1 || nTempVar > 16) {
		LOG_INF("PDA_PR_YNUM (%d) out of range\n", nTempVar);
		PDA_Data->Status = -15;
		return -1;
	}

	nTempVar = PDA_Data->PDA_FrameSetting.PDA_CFG_1.Bits.PDA_PAT_WIDTH;
	if (nTempVar < 1 || nTempVar > 512) {
		LOG_INF("PDA_PAT_WIDTH (%d) out of range\n", nTempVar);
		PDA_Data->Status = -16;
		return -1;
	}

	nTempVar = PDA_Data->PDA_FrameSetting.PDA_CFG_1.Bits.PDA_RNG_ST;
	if (nTempVar > 40) {
		LOG_INF("PDA_RNG_ST (%d) out of range\n", nTempVar);
		PDA_Data->Status = -17;
		return -1;
	}

	nTempVar = PDA_Data->PDA_FrameSetting.PDA_CFG_2.Bits.PDA_TBL_STRIDE;
	if (nTempVar < 5 || nTempVar > 2048) {
		LOG_INF("PDA_TBL_STRIDE (%d) out of range\n", nTempVar);
		PDA_Data->Status = -18;
		return -1;
	}

	return 0;
}

static void FillRegSettings(struct PDA_Data_t *pda_PdaConfig,
				unsigned int RoiProcNum,
				unsigned int OuputAddr)
{
	int RegIndex = 14;
	int ROI_MAX_INDEX = RoiProcNum-1;
	int pair = 0;

	//modify roi number register, change [11:6] bit to RoiProcNum
	pda_PdaConfig->PDA_FrameSetting.PDA_CFG_2.Bits.PDA_RGN_NUM = RoiProcNum;

	//roi number register setting
	PDA_WR32(PDA_CFG_2_REG, pda_PdaConfig->PDA_FrameSetting.PDA_CFG_2.Raw);

	//1024 ROI data sequentially fill to PDA_CFG[14] ~ PDA_CFG[126]
	for (pair = 0; pair <= ROI_MAX_INDEX; pair += 2) {
		PDA_WR32((PDA_BASE_HW + 0x004*(RegIndex++)),
			g_rgn_y_buf[pair]*65536 + g_rgn_x_buf[pair]);
		PDA_WR32((PDA_BASE_HW + 0x004*(RegIndex++)),
			g_rgn_h_buf[pair]*65536 + g_rgn_w_buf[pair]);
		if (pair == ROI_MAX_INDEX && pair%2 == 0) {
			PDA_WR32((PDA_BASE_HW + 0x004*(RegIndex++)), g_rgn_iw_buf[pair]);
		} else {
			PDA_WR32((PDA_BASE_HW + 0x004*(RegIndex++)),
				g_rgn_x_buf[pair+1]*65536 + g_rgn_iw_buf[pair]);
			PDA_WR32((PDA_BASE_HW + 0x004*(RegIndex++)),
				g_rgn_w_buf[pair+1]*65536 + g_rgn_y_buf[pair+1]);
			PDA_WR32((PDA_BASE_HW + 0x004*(RegIndex++)),
				g_rgn_iw_buf[pair+1]*65536 + g_rgn_h_buf[pair+1]);
		}
	}

	//output buffer address setting
	PDA_WR32(PDA_PDAO_P1_BASE_ADDR_REG, OuputAddr);
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

static signed int pda_wait_irq(struct PDA_Data_t *pda_data)
{
	int ret = 0;

	/* start to wait signal */
	ret = wait_event_interruptible_timeout(g_wait_queue_head,
						g_HWstatus != 0,
						pda_ms_to_jiffies(pda_data->Timeout));
#ifdef GET_PDA_TIME
	//for compute pda process time and kernel driver process time
	do_gettimeofday(&time_end);
#endif
	if (ret == 0) {
		//time out error
		LOG_INF("wait_event_interruptible_timeout Fail");
		pda_data->Status = -2;
		return -1;
	} else if (ret == -ERESTARTSYS) {
		LOG_INF("Interrupted by a signal\n");
		pda_data->Status = -3;
		return -1;
	}

	if (g_HWstatus < 0)
		LOG_INF("PDA HW error");
	else
		LOG_INF("PDA HW done");

	//irq done
	pda_data->Status = g_HWstatus;
	return ret;
}

static irqreturn_t pda_irqhandle(signed int Irq, void *DeviceId)
{
	unsigned int nPdaStatus = 0;

	//read pda status
	nPdaStatus = PDA_RD32(PDA_PDA_ERR_STAT_REG) & PDA_STATUS_REG;

	//for WCL=1 case, write 1 to clear pda done status
	//PDA_WR32(PDA_PDA_ERR_STAT_REG, 0x00000001);

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

static long PDA_Ioctl(struct file *a_pstFile,
			unsigned int a_u4Command,
			unsigned long a_u4Param)
{
	long nRet = 0;
	long nirqRet = 0;
	struct PDA_Data_t pda_Pdadata;
	int nROIcount = 0;
	unsigned int nOutputAddr = 0;
	unsigned int nCurrentProcRoiNum = 0;
	unsigned int nCurrentProcRoiIndex = 0;
	unsigned int nUserROINumber = 0;

	if (g_u4EnableClockCount == 0) {
		LOG_INF("Cannot process without enable pda clock\n");
		return -1;
	}

	switch (a_u4Command) {
	case PDA_RESET:
		pda_reset();
		break;
	case PDA_ENQUE_WAITIRQ:

		//reset HW status
		g_HWstatus = 0;

		if (copy_from_user(&pda_Pdadata,
				   (void *)a_u4Param,
				   sizeof(struct PDA_Data_t)) == 0) {

			//read user's ROI number
			nUserROINumber = pda_Pdadata.ROInumber;
			//LOG_INF("nUserROINumber = %d\n", nUserROINumber);

			//Init ROI count which needed to process
			nROIcount = nUserROINumber;

			// PDA HW and DMA setting
			HWDMASettings(&pda_Pdadata);

////////////////////////////this part will run many times//////////////////////
			while (nROIcount != 0 && nROIcount > 0) {

				// read 0x3b4, avoid the impact of previous data
				//LOG_INF("PDA status before process = %d\n",
				//		PDA_RD32(PDA_PDA_ERR_STAT_REG));

				//reset HW status
				g_HWstatus = 0;

				if (nROIcount > 45)
					nCurrentProcRoiNum = 45;
				else
					nCurrentProcRoiNum = nROIcount;

				//current process ROI index
				nCurrentProcRoiIndex = (nUserROINumber - nROIcount);

				//calculate 1024 ROI data
				if (ProcessROIData(&pda_Pdadata,
						nCurrentProcRoiNum,
						nCurrentProcRoiIndex,
						pda_Pdadata.nNumerousROI) < 0) {
					LOG_INF("ProcessROIData Fail\n");
					pda_Pdadata.Status = -24;
					goto EXIT;	//break;
				}

				if (CheckDesignLimitation(&pda_Pdadata,
						nCurrentProcRoiNum,
						nCurrentProcRoiIndex) < 0) {
					LOG_INF("CheckDesignLimitation Fail\n");
					goto EXIT;	//break;
				}

				//output address is equal to total ROI number multiple by 1408
				nOutputAddr = pda_Pdadata.PDA_PDAO_P1_BASE_ADDR;
				nOutputAddr += nCurrentProcRoiIndex * 1408;

				FillRegSettings(&pda_Pdadata, nCurrentProcRoiNum, nOutputAddr);

				// trigger PDA work
				pda_execute();

				nirqRet = pda_wait_irq(&pda_Pdadata);

				//write 0 after trigger
				PDA_WR32(PDA_PDA_TOP_CTL_REG, PDA_CLEAR_REG);

				if (nirqRet < 0) {
					LOG_INF("pda_wait_irq Fail (%d)\n", nirqRet);
					break;
				}

				//update roi count which needed to process
				nROIcount -= nCurrentProcRoiNum;
			}
///////////////////////////////////////////////////////////////////////////////
EXIT:

			if (copy_to_user((void *)a_u4Param,
			    &pda_Pdadata,
			    sizeof(struct PDA_Data_t)) != 0) {
				LOG_INF("copy_to_user failed\n");
				nRet = -EFAULT;
			}

		} else {
			LOG_INF("PDA_ENQUE_WAITIRQ copy_from_user failed\n");
			nRet = -EFAULT;
		}
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
	unsigned int irq_info[3];	/* Record interrupts info from device tree */
	struct device_node *node;
#ifdef FPGA_UT
	struct device_node *camsys_node;
#endif

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
	g_dev = &pDev->dev;

	//power on smi
	/* consumer driver probe*/
	pm_runtime_enable(g_dev); //Note: It‘s not larb's device.
	LOG_INF("pm_runtime_enable pda1 done\n");
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

#ifdef FPGA_UT
	//camsys node
	camsys_node = of_find_compatible_node(NULL, NULL, "mediatek,isp_unit_test");
	if (!camsys_node) {
		LOG_INF("find camsys_config node failed\n");
		return -1;
	}
	LOG_INF("find camsys_config node done\n");

	CAMSYS_CONFIG_BASE = of_iomap(camsys_node, 2);
	if (!CAMSYS_CONFIG_BASE)
		LOG_INF("base CAMSYS_CONFIG_BASE failed\n");
	LOG_INF("of_iomap CAMSYS_CONFIG_BASE done (0x%x)\n", CAMSYS_CONFIG_BASE);
#endif

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
