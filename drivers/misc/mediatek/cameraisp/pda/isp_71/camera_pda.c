// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/clk.h>
#include <linux/pm_runtime.h>

#include <linux/time.h>		//do_gettimeofday()

// --------- DMA-BUF ----------
#include <linux/dma-heap.h>
#include <linux/dma-direction.h>
#include <linux/scatterlist.h>
#include <linux/dma-buf.h>
// ----------------------------

#include <soc/mediatek/smi.h>	// mtk_smi_larb_get()

#include "camera_pda.h"

// --------- define region --------
// #define FPGA_UT
// #define GET_PDA_TIME
// #define FOR_DEBUG
// --------------------------------

#define PDA_DEV_NAME "camera-pda"

#define LOG_INF(format, args...)                                               \
	pr_info(PDA_DEV_NAME " [%s] " format, __func__, ##args)

#ifndef MTRUE
#define MTRUE 1
#endif
#ifndef MFALSE
#define MFALSE 0
#endif

//define the write register function
#define mt_reg_sync_writel(v, a) \
	do {    \
		*(unsigned int *)(a) = (v);    \
		mb();  /*make sure register access in order */ \
	} while (0)
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

struct device *g_dev1, *g_dev2;

struct device *larb13, *larb25;
struct device *larb14, *larb26;

//static int g_porbe_count;

static spinlock_t g_PDA_SpinLock;

wait_queue_head_t g_wait_queue_head;

// PDA HW quantity
static int g_PDA_quantity;

// pda device information
// static struct PDA_device *PDA_devs;
static struct PDA_device PDA_devs[PDA_MAX_QUANTITY];

// clock relate
#define PDA_CLK_NUM 5
static const char * const clk_names[PDA_CLK_NUM] = {
	"PDA_TOP_MUX",
	"PDA2_TOP_MUX",
	"PDA0_CAM_MAIN",
	"PDA1_CAM_MAIN",
	"PDA2_CAM_MAIN"
};
struct PDA_CLK_STRUCT pda_clk[PDA_CLK_NUM];

// Enable clock count
static unsigned int g_u4EnableClockCount;

#ifdef GET_PDA_TIME
// Get PDA process time
struct timeval time_begin, time_end;
struct timeval Config_time_begin, Config_time_end;
struct timeval total_time_begin, total_time_end;
struct timeval pda_done_time_end;
#endif

// calculate 1024 roi data
unsigned int g_rgn_x_buf[45];
unsigned int g_rgn_y_buf[45];
unsigned int g_rgn_h_buf[45];
unsigned int g_rgn_w_buf[45];
unsigned int g_rgn_iw_buf[45];

#ifdef FOR_DEBUG
// buffer address
unsigned int *g_buf_LI_va;
unsigned int *g_buf_RI_va;
unsigned int *g_buf_LT_va;
unsigned int *g_buf_RT_va;
unsigned int *g_buf_Out_va;
#endif

// current  Process ROI number
unsigned int g_CurrentProcRoiNum[PDA_MAX_QUANTITY];

struct device *pda_init_larb(struct platform_device *pdev, int idx)
{
	struct device_node *node;
	struct platform_device *larb_pdev;

	/* get larb node from dts */
	node = of_parse_phandle(pdev->dev.of_node, "mediatek,larbs", idx);
	if (!node) {
		LOG_INF("fail to parse mediatek,larb\n");
		return NULL;
	}

	larb_pdev = of_find_device_by_node(node);
	if (WARN_ON(!larb_pdev)) {
		of_node_put(node);
		LOG_INF("no larb for idx %d\n", idx);
		return NULL;
	}
	of_node_put(node);

	LOG_INF("pdev %p idx %d\n", pdev, idx);

	return &larb_pdev->dev;
}

static inline void PDA_Prepare_Enable_ccf_clock(void)
{
	int ret, i;

	LOG_INF("clock begin");

#if IS_ENABLED(CONFIG_OF)
	/* consumer device starting work*/
	pm_runtime_get_sync(g_dev1); //Note: It‘s not larb's device.
	pm_runtime_get_sync(g_dev2); //Note: It‘s not larb's device.
	LOG_INF("pm_runtime_get_sync done\n");
#endif

	// enable smi larb
	ret = mtk_smi_larb_get(larb13);
	if (ret)
		LOG_INF("mtk_smi_larb13_get larbvdec fail %d\n", ret);
	ret = mtk_smi_larb_get(larb14);
	if (ret)
		LOG_INF("mtk_smi_larb14_get larbvdec fail %d\n", ret);

	ret = mtk_smi_larb_get(larb25);
	if (ret)
		LOG_INF("mtk_smi_larb25_get larbvdec fail %d\n", ret);
	ret = mtk_smi_larb_get(larb26);
	if (ret)
		LOG_INF("mtk_smi_larb26_get larbvdec fail %d\n", ret);

	for (i = 0; i < PDA_CLK_NUM; i++) {
		ret = clk_prepare_enable(pda_clk[i].CG_PDA_TOP_MUX);
		if (ret)
			LOG_INF("cannot enable clock (%s)\n", clk_names[i]);
		LOG_INF("clk_prepare_enable (%s) done", clk_names[i]);
	}
	LOG_INF("clk_prepare_enable done");
}

static inline void PDA_Disable_Unprepare_ccf_clock(void)
{
	int i;

	for (i = 0; i < PDA_CLK_NUM; i++) {
		clk_disable_unprepare(pda_clk[i].CG_PDA_TOP_MUX);
		LOG_INF("clk_disable_unprepare (%s) done\n", clk_names[i]);
	}

	// enable smi larb
	mtk_smi_larb_put(larb13);
	mtk_smi_larb_put(larb14);
	mtk_smi_larb_put(larb25);
	mtk_smi_larb_put(larb26);

#if IS_ENABLED(CONFIG_OF)
	/* consumer device starting work*/
	pm_runtime_put(g_dev1); //Note: It‘s not larb's device.
	pm_runtime_put(g_dev2); //Note: It‘s not larb's device.
	LOG_INF("pm_runtime_put done\n");
#endif
}

/**************************************************************
 *
 **************************************************************/
static void EnableClock(bool En)
{
	if (En) {			/* Enable clock. */

		// Enable clock count
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
	} else {			/* Disable clock. */

		// Enable clock count
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
	int i;

	for (i = 0; i < g_PDA_quantity; i++) {

		// reset HW status
		PDA_devs[i].HWstatus = 0;

		m_pda_base = PDA_devs[i].m_pda_base;

		// make reset
		PDA_WR32(PDA_PDA_DMA_RST_REG, PDA_MAKE_RESET);

		// read reset status
		while ((PDA_RD32(PDA_PDA_DMA_RST_REG) & MASK_BIT_ZERO) != PDA_RESET_VALUE) {
			if (nResetCount > 30) {
				LOG_INF("PDA reset fail\n");
				nResetCount = 0;
				break;
			}
			LOG_INF("Wait EMI done\n");
			nResetCount++;
		}

		// equivalent to hardware reset
		PDA_WR32(PDA_PDA_TOP_CTL_REG, PDA_HW_RESET);

		// clear reset signal
		PDA_WR32(PDA_PDA_DMA_RST_REG, PDA_CLEAR_REG);

		// clear hardware reset signal
		PDA_WR32(PDA_PDA_TOP_CTL_REG, PDA_CLEAR_REG);
	}
}

static int pda_get_dma_buffer(struct pda_mmu *mmu, int fd)
{
	struct dma_buf *buf;

#ifdef FOR_DEBUG
	LOG_INF("get_dma_buffer_fd= %d\n", fd);
#endif
	if (fd < 0)
		return -1;

	buf = dma_buf_get(fd);
	if (IS_ERR(buf))
		return -1;

	mmu->dma_buf = buf;
	mmu->attach = dma_buf_attach(mmu->dma_buf, g_dev1);
	if (IS_ERR(mmu->attach))
		goto err_attach;

#ifdef FOR_DEBUG
	LOG_INF("mmu->attach = %x\n", mmu->attach);
#endif

	mmu->sgt = dma_buf_map_attachment(mmu->attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(mmu->sgt))
		goto err_map;

#ifdef FOR_DEBUG
	LOG_INF("mmu->sgt = %x\n", mmu->sgt);
#endif

	return 0;
err_map:
	dma_buf_detach(mmu->dma_buf, mmu->attach);
	LOG_INF("err_map!\n");
err_attach:
	LOG_INF("err_attach!\n");
	dma_buf_put(mmu->dma_buf);
	return -1;
}

static int Get_Input_Addr_From_DMABUF(struct PDA_Data_t *pda_PdaConfig)
{
	int ret = 0;
	struct pda_mmu mmu;
	unsigned long nAddress;
	int i = 0;

	//Left image buffer
	ret = pda_get_dma_buffer(&mmu, pda_PdaConfig->FD_L_Image);
	if (ret < 0) {
		LOG_INF("Left image, pda_get_dma_buffer fail!\n");
		return ret;
	}
	nAddress = (unsigned long) sg_dma_address(mmu.sgt->sgl);
	pda_PdaConfig->PDA_PDAI_P1_BASE_ADDR = (unsigned int)nAddress;
	for (i = 0; i < g_PDA_quantity; i++) {
		m_pda_base = PDA_devs[i].m_pda_base;
		PDA_WR32(PDA_PDAI_P1_BASE_ADDR_MSB_REG, (unsigned int)(nAddress >> 32));
	}
#ifdef FOR_DEBUG
	LOG_INF("Left image MVA = 0x%x\n", pda_PdaConfig->PDA_PDAI_P1_BASE_ADDR);
	LOG_INF("Left image MVA MSB = 0x%x\n", PDA_RD32(PDA_PDAI_P1_BASE_ADDR_MSB_REG));
	LOG_INF("Left image whole MVA = 0x%lx\n", nAddress);
	// get kernel va
	g_buf_LI_va = dma_buf_vmap(mmu.dma_buf);
	if (!g_buf_LI_va) {
		LOG_INF("Left image map failed\n");
		return -1;
	}
	LOG_INF("Left image buffer va = %x\n", g_buf_LI_va);
	LOG_INF("Left image buffer va data = %x\n", *g_buf_LI_va);
#endif

	//Right image buffer
	ret = pda_get_dma_buffer(&mmu, pda_PdaConfig->FD_R_Image);
	if (ret < 0) {
		LOG_INF("Right image, pda_get_dma_buffer fail!\n");
		return ret;
	}
	nAddress = (unsigned long) sg_dma_address(mmu.sgt->sgl);
	pda_PdaConfig->PDA_PDAI_P2_BASE_ADDR = (unsigned int)nAddress;
	for (i = 0; i < g_PDA_quantity; i++) {
		m_pda_base = PDA_devs[i].m_pda_base;
		PDA_WR32(PDA_PDAI_P2_BASE_ADDR_MSB_REG, (unsigned int)(nAddress >> 32));
	}
#ifdef FOR_DEBUG
	LOG_INF("Right image MVA = 0x%x\n", pda_PdaConfig->PDA_PDAI_P2_BASE_ADDR);
	LOG_INF("Right image MVA MSB = 0x%x\n", PDA_RD32(PDA_PDAI_P2_BASE_ADDR_MSB_REG));
	LOG_INF("Right image whole MVA = 0x%lx\n", nAddress);
	// get kernel va
	g_buf_RI_va = dma_buf_vmap(mmu.dma_buf);
	if (!g_buf_RI_va) {
		LOG_INF("Right image map failed\n");
		return -1;
	}
	LOG_INF("Right image buffer va = %x\n", g_buf_RI_va);
	LOG_INF("Right image buffer va data = %x\n", *g_buf_RI_va);
#endif

	//Left table buffer
	ret = pda_get_dma_buffer(&mmu, pda_PdaConfig->FD_L_Table);
	if (ret < 0) {
		LOG_INF("Left table, pda_get_dma_buffer fail!\n");
		return ret;
	}
	nAddress = (unsigned long) sg_dma_address(mmu.sgt->sgl);
	pda_PdaConfig->PDA_PDATI_P1_BASE_ADDR = (unsigned int)nAddress;
	for (i = 0; i < g_PDA_quantity; i++) {
		m_pda_base = PDA_devs[i].m_pda_base;
		PDA_WR32(PDA_PDATI_P1_BASE_ADDR_MSB_REG, (unsigned int)(nAddress >> 32));
	}
#ifdef FOR_DEBUG
	LOG_INF("Left table MVA = 0x%x\n", pda_PdaConfig->PDA_PDATI_P1_BASE_ADDR);
	LOG_INF("Left table MVA MSB = 0x%x\n", PDA_RD32(PDA_PDATI_P1_BASE_ADDR_MSB_REG));
	LOG_INF("Left table whole MVA = 0x%lx\n", nAddress);
	// get kernel va
	g_buf_LT_va = dma_buf_vmap(mmu.dma_buf);
	if (!g_buf_LT_va) {
		LOG_INF("Left table map failed\n");
		return -1;
	}
	LOG_INF("Left table buffer va = %x\n", g_buf_LT_va);
	LOG_INF("Left table buffer va data = %x\n", *g_buf_LT_va);
#endif

	//Right table buffer
	ret = pda_get_dma_buffer(&mmu, pda_PdaConfig->FD_R_Table);
	if (ret < 0) {
		LOG_INF("Right table, pda_get_dma_buffer fail!\n");
		return ret;
	}
	nAddress = (unsigned long) sg_dma_address(mmu.sgt->sgl);
	pda_PdaConfig->PDA_PDATI_P2_BASE_ADDR = (unsigned int)nAddress;
	for (i = 0; i < g_PDA_quantity; i++) {
		m_pda_base = PDA_devs[i].m_pda_base;
		PDA_WR32(PDA_PDATI_P2_BASE_ADDR_MSB_REG, (unsigned int)(nAddress >> 32));
	}
#ifdef FOR_DEBUG
	LOG_INF("Right table MVA = 0x%x\n", pda_PdaConfig->PDA_PDATI_P2_BASE_ADDR);
	LOG_INF("Right table MVA MSB = 0x%x\n", PDA_RD32(PDA_PDATI_P2_BASE_ADDR_MSB_REG));
	LOG_INF("Right table whole MVA = 0x%lx\n", nAddress);
	// get kernel va
	g_buf_RT_va = dma_buf_vmap(mmu.dma_buf);
	if (!g_buf_RT_va) {
		LOG_INF("Right table map failed\n");
		return -1;
	}
	LOG_INF("Right table buffer va = %x\n", g_buf_RT_va);
	LOG_INF("Right table buffer va data = %x\n", *g_buf_RT_va);
#endif

	return ret;
}

static int Get_Output_Addr_From_DMABUF(struct PDA_Data_t *pda_PdaConfig)
{
	int ret = 0;
	struct pda_mmu mmu;
	unsigned long nAddress;
	int i = 0;

	//Output buffer
	ret = pda_get_dma_buffer(&mmu, pda_PdaConfig->FD_Output);
	if (ret < 0) {
		LOG_INF("Output, pda_get_dma_buffer fail!\n");
		return ret;
	}
	nAddress = (unsigned long) sg_dma_address(mmu.sgt->sgl);
	pda_PdaConfig->PDA_PDAO_P1_BASE_ADDR = (unsigned int)nAddress;
	for (i = 0; i < g_PDA_quantity; i++) {
		m_pda_base = PDA_devs[i].m_pda_base;
		PDA_WR32(PDA_PDAO_P1_BASE_ADDR_MSB_REG, (unsigned int)(nAddress >> 32));
	}
#ifdef FOR_DEBUG
	LOG_INF("Output MVA = 0x%x\n", pda_PdaConfig->PDA_PDAO_P1_BASE_ADDR);
	LOG_INF("Output MVA MSB = 0x%x\n", PDA_RD32(PDA_PDAO_P1_BASE_ADDR_MSB_REG));
	LOG_INF("Output whole MVA = 0x%lx\n", nAddress);
	// get kernel va
	g_buf_Out_va = dma_buf_vmap(mmu.dma_buf);
	if (!g_buf_Out_va) {
		LOG_INF("Output map failed\n");
		return -1;
	}
	LOG_INF("Output buffer va = %x\n", g_buf_Out_va);
	LOG_INF("Output buffer va data = %x\n", *g_buf_Out_va);
#endif

	return ret;
}

static void HWDMASettings(struct PDA_Data_t *pda_PdaConfig)
{
	int i;

	for (i = 0; i < g_PDA_quantity; i++) {
		m_pda_base = PDA_devs[i].m_pda_base;

		// --------- Frame setting part -----------
		PDA_WR32(PDA_CFG_0_REG, pda_PdaConfig->PDA_FrameSetting.PDA_CFG_0.Raw);
		PDA_WR32(PDA_CFG_1_REG, pda_PdaConfig->PDA_FrameSetting.PDA_CFG_1.Raw);
		// need set roi number every process
		// PDA_WR32(PDA_CFG_2_REG, pda_PdaConfig->PDA_FrameSetting.PDA_CFG_2.Raw);
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

		// --------- Input buffer address -------------
		PDA_WR32(PDA_PDAI_P1_BASE_ADDR_REG, pda_PdaConfig->PDA_PDAI_P1_BASE_ADDR);
		PDA_WR32(PDA_PDATI_P1_BASE_ADDR_REG, pda_PdaConfig->PDA_PDATI_P1_BASE_ADDR);
		PDA_WR32(PDA_PDAI_P2_BASE_ADDR_REG, pda_PdaConfig->PDA_PDAI_P2_BASE_ADDR);
		PDA_WR32(PDA_PDATI_P2_BASE_ADDR_REG, pda_PdaConfig->PDA_PDATI_P2_BASE_ADDR);

		// --------- DMA Secure part -------------
		PDA_WR32(PDA_PDA_SECURE_REG, 0x9daf851f);

		// --------- config setting hard code part --------------
		PDA_WR32(PDA_PDAI_STRIDE_REG, 0x3e0);

		// Left image
		PDA_WR32(PDA_PDAI_P1_CON0_REG, 0x10000134);
		PDA_WR32(PDA_PDAI_P1_CON1_REG, 0x104d004d);
		PDA_WR32(PDA_PDAI_P1_CON2_REG, 0x9a009a);
		PDA_WR32(PDA_PDAI_P1_CON3_REG, 0x80e700e7);
		PDA_WR32(PDA_PDAI_P1_CON4_REG, 0x809a009a);

		// Left table
		PDA_WR32(PDA_PDATI_P1_CON0_REG, 0x1000004c);
		PDA_WR32(PDA_PDATI_P1_CON1_REG, 0x10130013);
		PDA_WR32(PDA_PDATI_P1_CON2_REG, 0x260026);
		PDA_WR32(PDA_PDATI_P1_CON3_REG, 0x390039);
		PDA_WR32(PDA_PDATI_P1_CON4_REG, 0x260026);

		// Right image
		PDA_WR32(PDA_PDAI_P2_CON0_REG, 0x10000134);
		PDA_WR32(PDA_PDAI_P2_CON1_REG, 0x004d004d);
		PDA_WR32(PDA_PDAI_P2_CON2_REG, 0x009a009a);
		PDA_WR32(PDA_PDAI_P2_CON3_REG, 0x80e700e7);
		PDA_WR32(PDA_PDAI_P2_CON4_REG, 0x809a009a);

		// Right table
		PDA_WR32(PDA_PDATI_P2_CON0_REG, 0x1000004c);
		PDA_WR32(PDA_PDATI_P2_CON1_REG, 0x10130013);
		PDA_WR32(PDA_PDATI_P2_CON2_REG, 0x260026);
		PDA_WR32(PDA_PDATI_P2_CON3_REG, 0x80390039);
		PDA_WR32(PDA_PDATI_P2_CON4_REG, 0x00260026);

		PDA_WR32(PDA_PDAO_P1_XSIZE_REG, 0x0000057f);

		// Output
		PDA_WR32(PDA_PDAO_P1_CON0_REG, 0x10000040);
		PDA_WR32(PDA_PDAO_P1_CON1_REG, 0x100010);
		PDA_WR32(PDA_PDAO_P1_CON2_REG, 0x10200020);
		PDA_WR32(PDA_PDAO_P1_CON3_REG, 0x80300030);
		PDA_WR32(PDA_PDAO_P1_CON4_REG, 0x200020);

		PDA_WR32(PDA_PDA_DMA_EN_REG, 0x0000001f);
		PDA_WR32(PDA_PDA_DMA_RST_REG, 0x1);
		PDA_WR32(PDA_PDA_DMA_TOP_REG, 0x407);
		PDA_WR32(PDA_PDA_TILE_STATUS_REG, 0x2000);

		PDA_WR32(PDA_PDA_DCM_DIS_REG, 0x00000000);
		PDA_WR32(PDA_PDA_DCM_ST_REG, 0x00000000);

		PDA_WR32(PDA_PDAI_P1_ERR_STAT_REG, 0x584f0000);
		PDA_WR32(PDA_PDATI_P1_ERR_STAT_REG, 0x8e7d0000);
		PDA_WR32(PDA_PDAI_P2_ERR_STAT_REG, 0x68630000);
		PDA_WR32(PDA_PDATI_P2_ERR_STAT_REG, 0xccf00000);
		PDA_WR32(PDA_PDAO_P1_ERR_STAT_REG, 0xe4670000);

		PDA_WR32(PDA_PDA_TOP_CTL_REG, 0x00000000);
		PDA_WR32(PDA_PDA_DEBUG_SEL_REG, 0xf56bb1b2);
		PDA_WR32(PDA_PDA_IRQ_TRIG_REG, 0x16);

		// setting read clear
		PDA_WR32(PDA_PDA_ERR_STAT_EN_REG, 0x00000003);

		// read 0x3b4, avoid the impact of previous data
		PDA_RD32(PDA_PDA_ERR_STAT_REG);
	}
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
#ifdef FOR_DEBUG
		LOG_INF("Sequential ROI\n");
#endif
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

				// calculate done
				if (nLocalBufIndex >= (RoiProcNum-1))
					goto CALDONE;
			}	// for i

			// use for check if first x direction loop or not
			nXDirLoopCount++;
		}	// for j
	} else {
#ifdef FOR_DEBUG
		LOG_INF("Random ROI\n");
#endif
		for (i = 0; i < RoiProcNum; i++) {
			g_rgn_x_buf[i] = pda_data->rgn_x[ROIIndex+i];
			g_rgn_y_buf[i] = pda_data->rgn_y[ROIIndex+i];
			g_rgn_w_buf[i] = pda_data->rgn_w[ROIIndex+i];
			g_rgn_h_buf[i] = pda_data->rgn_h[ROIIndex+i];
			g_rgn_iw_buf[i] = pda_data->rgn_iw[ROIIndex+i];
		}
	}

CALDONE:
#ifdef FOR_DEBUG
		LOG_INF("Calculate ROI done\n");
#endif

	return 0;
}

static int CheckDesignLimitation(struct PDA_Data_t *PDA_Data,
				unsigned int RoiProcNum,
				unsigned int ROIIndex)
{
	int i = 0;
	int nROIIndex = 0;
	int nTempVar = 0;

#ifdef FOR_DEBUG
		LOG_INF("Check Design Limitation\n");
#endif

	// frame constraint
	if (PDA_Data->PDA_FrameSetting.PDA_CFG_0.Bits.PDA_WIDTH % 4 != 0) {
		LOG_INF("Frame width must be multiple of 4\n");
		PDA_Data->Status = -4;
		return -1;
	}

	// ROI constraint
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

		// ROI boundary must be on the boundary of patch
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

		// ROI can't exceed the image region
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

		// Register Range Limitation check
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

	// modify roi number register, change [11:6] bit to RoiProcNum
	pda_PdaConfig->PDA_FrameSetting.PDA_CFG_2.Bits.PDA_RGN_NUM = RoiProcNum;

	// roi number register setting
	PDA_WR32(PDA_CFG_2_REG, pda_PdaConfig->PDA_FrameSetting.PDA_CFG_2.Raw);

	// 1024 ROI data sequentially fill to PDA_CFG[14] ~ PDA_CFG[126]
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

	// output buffer address setting
	PDA_WR32(PDA_PDAO_P1_BASE_ADDR_REG, OuputAddr);

#ifdef FOR_DEBUG
		LOG_INF("Fill Register Settings done\n");
#endif
}

static void pda_execute(void)
{
	int i;

#ifdef FOR_DEBUG
	LOG_INF("+\n");
#endif

	for (i = 0; i < g_PDA_quantity; i++) {
		m_pda_base = PDA_devs[i].m_pda_base;

		// PDA_TOP_CTL set 1'b1 to bit3, to load register from double buffer
		PDA_WR32(PDA_PDA_TOP_CTL_REG, PDA_DOUBLE_BUFFER);

		// PDA_TOP_CTL set 1'b1 to bit1, to trigger sof
		PDA_WR32(PDA_PDA_TOP_CTL_REG, PDA_TRIGGER);
	}

#ifdef GET_PDA_TIME
	// for compute pda process time
	do_gettimeofday(&time_begin);
#endif

#ifdef FOR_DEBUG
	LOG_INF("-\n");
#endif
}

static int check_pda_status(void)
{
	int i = 0;

	for (i = 0; i < g_PDA_quantity; i++) {
		if (PDA_devs[i].HWstatus == 0) {
#ifdef FOR_DEBUG
			LOG_INF("PDA%d HWstatus = %d\n", i, PDA_devs[i].HWstatus);
#endif
			return 0;
		}
	}
	return 1;
}

static inline unsigned int pda_ms_to_jiffies(unsigned int ms)
{
	return ((ms * HZ + 512) >> 10);
}

static signed int pda_wait_irq(struct PDA_Data_t *pda_data)
{
	int ret = 0, i = 0;

	/* start to wait signal */
	ret = wait_event_interruptible_timeout(g_wait_queue_head,
						check_pda_status(),
						pda_ms_to_jiffies(pda_data->Timeout));

#ifdef GET_PDA_TIME
	// for compute pda process time and kernel driver process time
	do_gettimeofday(&time_end);
#endif
	if (ret == 0) {
		// time out error
		LOG_INF("wait_event_interruptible_timeout Fail");
		pda_data->Status = -2;
		return -1;
	} else if (ret == -ERESTARTSYS) {
		LOG_INF("Interrupted by a signal\n");
		pda_data->Status = -3;
		return -1;
	}

	for (i = 0; i < g_PDA_quantity; i++) {
		// update status to user
		pda_data->Status = PDA_devs[i].HWstatus;

		if (PDA_devs[i].HWstatus < 0) {
			LOG_INF("PDA%d HW error", i);

			// reset flow
			pda_reset();
			break;
		}
#ifdef FOR_DEBUG
		LOG_INF("PDA%d HW done", i);
#endif
	}

	return ret;
}

static irqreturn_t pda_irqhandle(signed int Irq, void *DeviceId)
{
	unsigned int nPdaStatus = 0;

	m_pda_base = PDA_devs[0].m_pda_base;

	// read pda status
	nPdaStatus = PDA_RD32(PDA_PDA_ERR_STAT_REG) & PDA_STATUS_REG;

	// for WCL=1 case, write 1 to clear pda done status
	// PDA_WR32(PDA_PDA_ERR_STAT_REG, 0x00000001);

	if (nPdaStatus == PDA_DONE) {
		PDA_devs[0].HWstatus = 1;

#ifdef GET_PDA_TIME
		// for compute pda process time
		do_gettimeofday(&pda_done_time_end);
#endif
	} else if (nPdaStatus == PDA_ERROR) {
		PDA_devs[0].HWstatus = -1;
	} else {
		// reserve
		PDA_devs[0].HWstatus = 0;
	}

	// wake up user space WAIT_IRQ flag
	wake_up_interruptible(&g_wait_queue_head);
	return IRQ_HANDLED;
}

static irqreturn_t pda2_irqhandle(signed int Irq, void *DeviceId)
{
	unsigned int nPdaStatus = 0;

	m_pda_base = PDA_devs[1].m_pda_base;

	// read pda status
	nPdaStatus = PDA_RD32(PDA_PDA_ERR_STAT_REG) & PDA_STATUS_REG;

	// for WCL=1 case, write 1 to clear pda done status
	// PDA_WR32(PDA_PDA_ERR_STAT_REG, 0x00000001);

	if (nPdaStatus == PDA_DONE) {
		PDA_devs[1].HWstatus = 1;

#ifdef GET_PDA_TIME
		// for compute pda process time
		do_gettimeofday(&pda_done_time_end);
#endif
	} else if (nPdaStatus == PDA_ERROR) {
		PDA_devs[1].HWstatus = -1;
	} else {
		// reserve
		PDA_devs[1].HWstatus = 0;
	}

	// wake up user space WAIT_IRQ flag
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
	unsigned int nCurrentProcRoiIndex = 0;
	unsigned int nUserROINumber = 0;
	int i;
	unsigned int nRemainder = 0, nFactor = 0;
	unsigned int nOneRoundProcROI = 0;

	if (g_u4EnableClockCount == 0) {
		LOG_INF("Cannot process without enable pda clock\n");
		return -1;
	}

	switch (a_u4Command) {
	case PDA_RESET:
		pda_reset();
		break;
	case PDA_ENQUE_WAITIRQ:

		// reset HW status
		for (i = 0; i < g_PDA_quantity; i++)
			PDA_devs[i].HWstatus = 0;

		if (copy_from_user(&pda_Pdadata,
				   (void *)a_u4Param,
				   sizeof(struct PDA_Data_t)) != 0) {
			LOG_INF("PDA_ENQUE_WAITIRQ copy_from_user failed\n");
			nRet = -EFAULT;
			break;
		}

		// read user's ROI number
		nUserROINumber = pda_Pdadata.ROInumber;
#ifdef FOR_DEBUG
		LOG_INF("nUserROINumber = %d\n", nUserROINumber);
		LOG_INF("g_PDA_quantity = %d\n", g_PDA_quantity);
#endif
		// Init ROI count which needed to process
		nROIcount = nUserROINumber;

		if (Get_Input_Addr_From_DMABUF(&pda_Pdadata) < 0) {
			pda_Pdadata.Status = -26;
			LOG_INF("Get_Input_Addr_From_DMABUF fail\n");
			goto EXIT;
		}

			// output buffer mapping iova
		if (Get_Output_Addr_From_DMABUF(&pda_Pdadata) < 0) {
			pda_Pdadata.Status = -27;
			LOG_INF("Get_Output_Addr_From_DMABUF fail\n");
			goto EXIT;
		}

		// PDA HW and DMA setting
		HWDMASettings(&pda_Pdadata);

		// ---------------- this part will run many times --------------
		while (nROIcount != 0 && nROIcount > 0) {

			// read 0x3b4, avoid the impact of previous data
			// LOG_INF("PDA status before process = %d\n",
			//	PDA_RD32(PDA_PDA_ERR_STAT_REG));
#ifdef FOR_DEBUG
			LOG_INF("nROIcount = %d\n", nROIcount);
#endif
			// reset HW status
			for (i = 0; i < g_PDA_quantity; i++)
				PDA_devs[i].HWstatus = 0;

			// reset local variable
			nOneRoundProcROI = 0;

			// assign strategy, used for multi-engine
			if (nROIcount >= (45 * g_PDA_quantity)) {
				for (i = 0; i < g_PDA_quantity; i++) {
					g_CurrentProcRoiNum[i] = 45;
					nOneRoundProcROI += g_CurrentProcRoiNum[i];
#ifdef FOR_DEBUG
					LOG_INF("g_CurrentProcRoiNum[%d] = %d\n",
						i, g_CurrentProcRoiNum[i]);
					LOG_INF("OneRoundProcROI = %d\n", nOneRoundProcROI);
#endif
				}
			} else {
				if (g_PDA_quantity == 0) {
					LOG_INF("Fail: g_PDA_quantity is zero\n");
					goto EXIT;
				}
				nRemainder = nROIcount % g_PDA_quantity;
				nFactor = nROIcount / g_PDA_quantity;

				for (i = 0; i < g_PDA_quantity; i++) {
					g_CurrentProcRoiNum[i] = nFactor;
					if (nRemainder > 0) {
						g_CurrentProcRoiNum[i]++;
						nRemainder--;
					}
					nOneRoundProcROI += g_CurrentProcRoiNum[i];
#ifdef FOR_DEBUG
					LOG_INF("g_CurrentProcRoiNum[%d] = %d\n",
						i, g_CurrentProcRoiNum[i]);
					LOG_INF("OneRoundProcROI = %d\n", nOneRoundProcROI);
#endif
				}
			}

			for (i = 0; i < g_PDA_quantity; i++) {
				// update base address
				m_pda_base = PDA_devs[i].m_pda_base;

				// current process ROI index
				if (i == 0)
					nCurrentProcRoiIndex = (nUserROINumber - nROIcount);
				else if (i > 0)
					nCurrentProcRoiIndex += g_CurrentProcRoiNum[i - 1];
				else
					LOG_INF("Index is out of range, i = %d\n", i);
#ifdef FOR_DEBUG
				LOG_INF("nCurrentProcRoiIndex = %d\n",
					nCurrentProcRoiIndex);
#endif

				// calculate 1024 ROI data
				if (ProcessROIData(&pda_Pdadata,
						g_CurrentProcRoiNum[i],
						nCurrentProcRoiIndex,
						pda_Pdadata.nNumerousROI) < 0) {
					LOG_INF("ProcessROIData Fail\n");
					pda_Pdadata.Status = -24;
					goto EXIT;
				}

				if (CheckDesignLimitation(&pda_Pdadata,
						g_CurrentProcRoiNum[i],
						nCurrentProcRoiIndex) < 0) {
					LOG_INF("CheckDesignLimitation Fail\n");
					goto EXIT;
				}

				// output address is equal to
				// total ROI number multiple by 1408
				nOutputAddr = pda_Pdadata.PDA_PDAO_P1_BASE_ADDR;
				nOutputAddr += nCurrentProcRoiIndex * 1408;

				FillRegSettings(&pda_Pdadata,
					g_CurrentProcRoiNum[i],
					nOutputAddr);
			}

			// trigger PDA work
			pda_execute();

			nirqRet = pda_wait_irq(&pda_Pdadata);

			// write 0 after trigger
			for (i = 0; i < g_PDA_quantity; i++) {
				m_pda_base = PDA_devs[i].m_pda_base;
				PDA_WR32(PDA_PDA_TOP_CTL_REG, PDA_CLEAR_REG);
			}

			if (nirqRet < 0) {
				LOG_INF("pda_wait_irq Fail (%d)\n", nirqRet);
				break;
			}

			// update roi count which needed to process
			nROIcount -= nOneRoundProcROI;
		}
//////////////////////////////////////////////////////////////////////////////
EXIT:

		if (copy_to_user((void *)a_u4Param,
		    &pda_Pdadata,
		    sizeof(struct PDA_Data_t)) != 0) {
			LOG_INF("copy_to_user failed\n");
			nRet = -EFAULT;
		}

		break;
	default:
		LOG_INF("Unknown Cmd(%d)\n", a_u4Command);
		break;
	}
	return nRet;
}

#if IS_ENABLED(CONFIG_COMPAT)
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

/*****************************************************************************
 *
 ****************************************************************************/

static dev_t g_PDA_devno;
static struct cdev *g_pPDA_CharDrv;
static struct class *actuator_class;
static struct device *lens_device;

static const struct file_operations g_stPDA_fops = {
	.owner = THIS_MODULE,
	.open = PDA_Open,
	.release = PDA_Release,
	.unlocked_ioctl = PDA_Ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
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

	/* Attach file operation. */
	cdev_init(g_pPDA_CharDrv, &g_stPDA_fops);

	g_pPDA_CharDrv->owner = THIS_MODULE;

	/* Add to system */
	nRet = cdev_add(g_pPDA_CharDrv, g_PDA_devno, 1);
	if (nRet < 0) {
		LOG_INF("Attach file operation failed\n");
		unregister_chrdev_region(g_PDA_devno, 1);
		return nRet;
	}

	actuator_class = class_create(THIS_MODULE, "PDAdrv");
	if (IS_ERR(actuator_class)) {
		int ret = PTR_ERR(actuator_class);

		LOG_INF("Unable to create class, err = %d\n", ret);
		// unregister_chrdev_region(g_PDA_devno, 1);
		return ret;
	}

	lens_device = device_create(actuator_class, NULL, g_PDA_devno, NULL,
				    PDA_DEV_NAME);

	if (IS_ERR(lens_device)) {
		int ret = PTR_ERR(lens_device);

		LOG_INF("create dev err: /dev/%s, err = %d\n", PDA_DEV_NAME, ret);
		// unregister_chrdev_region(g_PDA_devno, 1);
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

/*****************************************************************************
 *
 ****************************************************************************/
static int PDA_probe(struct platform_device *pDev)
{
	int nRet = 0;
	unsigned int irq_info[3];	/* Record interrupts info from device tree */
	struct device_node *node;
	int i;

#ifdef FPGA_UT
	struct device_node *camsys_node;
#endif

	LOG_INF("probe Start\n");

	// init pda quantity
	g_PDA_quantity = 0;

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

	// must porting in dts
	larb13 = pda_init_larb(pDev, 0);
	larb25 = pda_init_larb(pDev, 1);

#if IS_ENABLED(CONFIG_OF)
	g_dev1 = &pDev->dev;

	if (dma_set_mask_and_coherent(g_dev1, DMA_BIT_MASK(34)))
		LOG_INF("No suitable DMA available\n");

	//power on smi
	/* consumer driver probe*/
	pm_runtime_enable(g_dev1); //Note: It‘s not larb's device.
	LOG_INF("pm_runtime_enable pda1 done\n");
#endif

	// get PDA node
	node = of_find_compatible_node(NULL, NULL, "mediatek,camera-pda");
	if (!node) {
		LOG_INF("find camera-pda node failed\n");
		return -1;
	}
	LOG_INF("find camera-pda node done\n");

	for (i = 0; i < PDA_CLK_NUM; ++i) {
		// CCF: Grab clock pointer (struct clk*)
		LOG_INF("index: %d, clock name: %s\n", i, clk_names[i]);
		pda_clk[i].CG_PDA_TOP_MUX = devm_clk_get(&pDev->dev, clk_names[i]);
		if (IS_ERR(pda_clk[i].CG_PDA_TOP_MUX)) {
			LOG_INF("cannot get %s clock\n", clk_names[i]);
			return PTR_ERR(pda_clk[i].CG_PDA_TOP_MUX);
		}
	}

	// get PDA address, and PDA quantity
	PDA_devs[0].m_pda_base = of_iomap(node, 0);
	if (!PDA_devs[0].m_pda_base)
		LOG_INF("PDA0 base m_pda_base failed\n");

	// get IRQ ID and request IRQ
	PDA_devs[0].irq = irq_of_parse_and_map(node, 0);
	LOG_INF("PDA_dev[0]->irq: %d", PDA_devs[0].irq);

	if (PDA_devs[0].irq != 0)
		g_PDA_quantity++;
	LOG_INF("PDA quantity: %d\n", g_PDA_quantity);

#ifdef FPGA_UT
	// camsys node
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

	// ======================== here need to be modified =====================
	if (PDA_devs[0].irq > 0) {
		if (PDA_devs[0].irq > 0 && g_PDA_quantity > 0) {
			// Get IRQ Flag from device node
			nRet = of_property_read_u32_array(node,
								"interrupts",
								irq_info,
								ARRAY_SIZE(irq_info));
			if (nRet) {
				LOG_INF("PDA1 get irq flags from DTS fail!!\n");
				return -ENODEV;
			}
			LOG_INF("PDA1 irq_info: %d\n", irq_info[2]);
			nRet = request_irq(PDA_devs[0].irq,
					(irq_handler_t) pda_irqhandle,
					irq_info[2],
					(const char *)node->name,
					NULL);
			if (nRet) {
				LOG_INF("PDA1 request_irq Fail: %d\n", nRet);
				return nRet;
			}
		} else {
			LOG_INF("PDA1 get IRQ ID Fail or No IRQ: %d\n", PDA_devs[0].irq);
		}
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

static int PDA2_probe(struct platform_device *pdev)
{
	int nRet = 0;
	struct device_node *node;
	unsigned int irq_info[3];

	LOG_INF("PDA2 probe Start\n");

	// must porting in dts
	larb14 = pda_init_larb(pdev, 0);
	larb26 = pda_init_larb(pdev, 1);

#if IS_ENABLED(CONFIG_OF)
	g_dev2 = &pdev->dev;

	if (dma_set_mask_and_coherent(g_dev2, DMA_BIT_MASK(34)))
		LOG_INF("No suitable DMA available\n");

	//power on smi
	// consumer driver probe
	pm_runtime_enable(g_dev2); //Note: It‘s not larb's device.
	LOG_INF("pm_runtime_enable pda2 done\n");
#endif

	// get PDA node
	node = of_find_compatible_node(NULL, NULL, "mediatek,camera-pda2");
	if (!node) {
		LOG_INF("find camera-pda node failed\n");
		return -1;
	}
	LOG_INF("find camera-pda node done\n");

	// get PDA address, and PDA quantity
	PDA_devs[1].m_pda_base = of_iomap(node, 0);
	if (!PDA_devs[1].m_pda_base)
		LOG_INF("base m_pda_base failed, index: %d\n", 1);

	// get IRQ ID and request IRQ
	PDA_devs[1].irq = irq_of_parse_and_map(node, 0);
	LOG_INF("PDA_dev[1]->irq: %d", PDA_devs[1].irq);

	if (PDA_devs[1].irq != 0)
		g_PDA_quantity++;
	LOG_INF("PDA quantity: %d\n", g_PDA_quantity);

	if (PDA_devs[1].irq > 0) {
		if (PDA_devs[1].irq > 0 && g_PDA_quantity > 1) {
			// Get IRQ Flag from device node
			nRet = of_property_read_u32_array(node,
								"interrupts",
								irq_info,
								ARRAY_SIZE(irq_info));
			if (nRet) {
				LOG_INF("PDA2 get irq flags from DTS fail!!\n");
				return -ENODEV;
			}
			LOG_INF("PDA2 irq_info: %d\n", irq_info[2]);
			nRet = request_irq(PDA_devs[1].irq,
					(irq_handler_t) pda2_irqhandle,
					irq_info[2],
					(const char *)node->name,
					NULL);
			if (nRet) {
				LOG_INF("PDA2 request_irq Fail: %d\n", nRet);
				return nRet;
			}
		} else {
			LOG_INF("PDA2 get IRQ ID Fail or No IRQ: %d\n", PDA_devs[1].irq);
		}
	}

	LOG_INF("PDA2 probe End\n");

	return nRet;
}

static int PDA2_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
	return 0;
}

//////////////////////////////////////// PDA driver //////////////////////////
#if IS_ENABLED(CONFIG_OF)
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
#if IS_ENABLED(CONFIG_OF)
		   .of_match_table = of_match_ptr(gpda_of_device_id),
#endif
		}
};

//////////////////////////////////////// PDA 2 driver ////////////////////////
#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id gpda2_of_device_id[] = {
	{.compatible = "mediatek,camera-pda2",},
	{}
};
#endif

MODULE_DEVICE_TABLE(of, gpda2_of_device_id);

static struct platform_driver PDA2Driver = {
	.probe = PDA2_probe,
	.remove = PDA2_remove,
	.driver = {
		   .name = "camera-pda2",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(gpda2_of_device_id),
	}
};

static int __init camera_pda_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&PDADriver);
	if (ret < 0) {
		LOG_INF("platform_driver_register PDADriver\n");
		return ret;
	}
	ret = platform_driver_register(&PDA2Driver);
	if (ret < 0) {
		LOG_INF("platform_driver_register PDADriver\n");
		return ret;
	}
	return ret;
}

static void __exit camera_pda_exit(void)
{
	platform_driver_unregister(&PDADriver);
	platform_driver_unregister(&PDA2Driver);
}
/****************************************************************************
 *
 ****************************************************************************/
module_init(camera_pda_init);
module_exit(camera_pda_exit);
MODULE_DESCRIPTION("Camera PDA driver");
MODULE_AUTHOR("MM6SW3");
MODULE_LICENSE("GPL");
