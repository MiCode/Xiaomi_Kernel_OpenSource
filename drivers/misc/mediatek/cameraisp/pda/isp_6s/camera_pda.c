// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/jiffies.h>

#include <linux/time.h>		//do_gettimeofday()

#include "mtk-interconnect.h"

// --------- DMA-BUF ----------
#include <linux/dma-heap.h>
#include <linux/dma-direction.h>
#include <linux/scatterlist.h>
#include <linux/dma-buf.h>
// ----------------------------

#include "camera_pda.h"

// --------- define region --------
// #define FPGA_UT
// #define GET_PDA_TIME
// #define FOR_DEBUG
// #define SMI_LOG
#define CHECK_IRQ_COUNT
#define PDA_MMQOS
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

void __iomem *CAMSYS_CONFIG_BASE;
#define CAMSYS_MAIN_BASE_ADDR CAMSYS_CONFIG_BASE
#define REG_CAMSYS_CG_SET               (CAMSYS_MAIN_BASE_ADDR + 0x4)
#define REG_CAMSYS_CG_CLR               (CAMSYS_MAIN_BASE_ADDR + 0x8)
#define REG_CAMSYS_SW_RST               (CAMSYS_MAIN_BASE_ADDR + 0xC)

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

static spinlock_t g_PDA_SpinLock;

wait_queue_head_t g_wait_queue_head;

// PDA HW quantity
static unsigned int g_PDA_quantity;

#ifdef CHECK_IRQ_COUNT
// Calculate reasonable irq counts
static unsigned int g_reasonable_IRQCount;
static int g_PDA0_IRQCount;
static int g_PDA1_IRQCount;
#endif

static struct PDA_Data_t g_pda_Pdadata;

// pda device information
static struct PDA_device PDA_devs[PDA_MAX_QUANTITY];

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
unsigned int g_rgn_x_buf[PDA_MAXROI_PER_ROUND];
unsigned int g_rgn_y_buf[PDA_MAXROI_PER_ROUND];
unsigned int g_rgn_h_buf[PDA_MAXROI_PER_ROUND];
unsigned int g_rgn_w_buf[PDA_MAXROI_PER_ROUND];
unsigned int g_rgn_iw_buf[PDA_MAXROI_PER_ROUND];

// buffer mmu
struct pda_mmu g_image_mmu;
struct pda_mmu g_table_mmu;
struct pda_mmu g_output_mmu;

// Output buffer
unsigned long g_Address_LI;
unsigned long g_Address_RI;
unsigned long g_Address_LT;
unsigned long g_Address_RT;
static unsigned long g_OutputBufferAddr;
static unsigned long g_OutputBufferOffset;

// current Process ROI number
unsigned int g_CurrentProcRoiNum[PDA_MAX_QUANTITY];

static inline void PDA_Prepare_Enable_ccf_clock(void)
{
#if IS_ENABLED(CONFIG_OF)
	/* consumer device starting work*/
	if (g_PDA_quantity > 0)
		pm_runtime_get_sync(g_dev1); //Note: It‘s not larb's device.
	if (g_PDA_quantity > 1)
		pm_runtime_get_sync(g_dev2); //Note: It‘s not larb's device.
#ifdef FOR_DEBUG
	LOG_INF("pm_runtime_get_sync done\n");
#endif
#endif

	pda_clk_prepare_enable();
}

static inline void PDA_Disable_Unprepare_ccf_clock(void)
{
	pda_clk_disable_unprepare();

#if IS_ENABLED(CONFIG_OF)
	if (g_PDA_quantity > 1)
		pm_runtime_put_sync(g_dev2);
	if (g_PDA_quantity > 0)
		pm_runtime_put_sync(g_dev1);
#ifdef FOR_DEBUG
	LOG_INF("pm_runtime_put_sync done\n");
#endif
#endif
}
/**************************************************************
 *
 **************************************************************/
static void EnableClock(bool En)
{
	if (En) {			/* Enable clock. */

		//Enable clock count
		spin_lock(&g_PDA_SpinLock);
		switch (g_u4EnableClockCount) {
		case 0:
			g_u4EnableClockCount++;
			spin_unlock(&g_PDA_SpinLock);

#ifndef FPGA_UT
#ifdef FOR_DEBUG
		LOG_INF("It's real ic load, Enable Clock");
#endif
		PDA_Prepare_Enable_ccf_clock();
#else
		// Enable clock by hardcode:
		LOG_INF("It's LDVT load, Enable Clock");
		PDA_WR32(REG_CAMSYS_CG_CLR, 0xFFFFFFFF);
#endif

			break;
		default:
			g_u4EnableClockCount++;
			spin_unlock(&g_PDA_SpinLock);
			break;
		}
	} else {			/* Disable clock. */

		// Disable clock count
		spin_lock(&g_PDA_SpinLock);
		g_u4EnableClockCount--;
		switch (g_u4EnableClockCount) {
		case 0:
			spin_unlock(&g_PDA_SpinLock);
#ifndef FPGA_UT
#ifdef FOR_DEBUG
		LOG_INF("It's real ic load, Disable Clock");
#endif
		PDA_Disable_Unprepare_ccf_clock();
#else
		// Disable clock by hardcode:
		LOG_INF("It's LDVT load, Disable Clock");
		PDA_WR32(REG_CAMSYS_CG_SET, 0xFFFFFFFF);
#endif

			break;
		default:
			spin_unlock(&g_PDA_SpinLock);
			break;
		}
	}
}

static void pda_reset(unsigned int PDA_Index)
{
	unsigned long end = 0;

	end = jiffies + msecs_to_jiffies(100);

	// reset HW status
	PDA_devs[PDA_Index].HWstatus = 0;

	// clear dma_soft_rst_stat
	PDA_WR32(PDA_devs[PDA_Index].m_pda_base + PDA_PDA_DMA_RST_REG,
		PDA_CLEAR_REG);
	// make reset
	PDA_WR32(PDA_devs[PDA_Index].m_pda_base + PDA_PDA_DMA_RST_REG,
		PDA_MAKE_RESET);
	wmb(); /* TBC */

	while (time_before(jiffies, end)) {
		if ((PDA_RD32(PDA_devs[PDA_Index].m_pda_base + PDA_PDA_DMA_RST_REG) &
			MASK_BIT_ZERO)) {
			// equivalent to hardware reset
			PDA_WR32(PDA_devs[PDA_Index].m_pda_base + PDA_PDA_TOP_CTL_REG,
				PDA_HW_RESET);
			// clear reset signal
			PDA_WR32(PDA_devs[PDA_Index].m_pda_base + PDA_PDA_DMA_RST_REG,
				PDA_CLEAR_REG);
			wmb(); /* TBC */
			// clear hardware reset signal
			PDA_WR32(PDA_devs[PDA_Index].m_pda_base + PDA_PDA_TOP_CTL_REG,
				PDA_CLEAR_REG);
			// LOG_INF("reset PDA%d hw success\n", PDA_Index);
			return;
		}

		LOG_INF("PDA%d Wait EMI request, DMA_RST:0x%x\n",
			PDA_Index,
			PDA_RD32(PDA_devs[PDA_Index].m_pda_base + PDA_PDA_DMA_RST_REG));

		usleep_range(10, 20);
	}

	LOG_INF("reset PDA%d hw timeout\n", PDA_Index);
}

static void pda_nontransaction_reset(unsigned int PDA_Index)
{
	unsigned int MRAW_reset_value = 0;
	unsigned int Reset_Bitmask = 0;

	// equivalent to hardware reset
	PDA_WR32(PDA_devs[PDA_Index].m_pda_base + PDA_PDA_TOP_CTL_REG,
		PDA_HW_RESET);

	// clear hardware reset signal
	PDA_WR32(PDA_devs[PDA_Index].m_pda_base + PDA_PDA_TOP_CTL_REG,
		PDA_CLEAR_REG);

	//MRAW PDA reset
	MRAW_reset_value = PDA_RD32(REG_CAMSYS_SW_RST);

	Reset_Bitmask = GetResetBitMask(PDA_Index);

	// LOG_INF("before, MRAW_reset_value: %x\n", MRAW_reset_value);
	MRAW_reset_value |= Reset_Bitmask;
	PDA_WR32(REG_CAMSYS_SW_RST, MRAW_reset_value);
	// LOG_INF("after, MRAW_reset_value: %x\n", PDA_RD32(REG_CAMSYS_SW_RST));
	MRAW_reset_value &= (!Reset_Bitmask);
	PDA_WR32(REG_CAMSYS_SW_RST, MRAW_reset_value);
	// LOG_INF("clear bit, MRAW_reset_value: %x\n", PDA_RD32(REG_CAMSYS_SW_RST));
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

static void pda_put_dma_buffer(struct pda_mmu *mmu)
{
	if (mmu->attach == NULL || mmu->sgt == NULL) {
		LOG_INF("attach or sgt is null, no need to free iova\n");
		return;
	}

	if (mmu->dma_buf) {
		dma_buf_unmap_attachment(mmu->attach, mmu->sgt, DMA_BIDIRECTIONAL);
		dma_buf_detach(mmu->dma_buf, mmu->attach);
		dma_buf_put(mmu->dma_buf);
	}
}

static int Get_Input_Addr_From_DMABUF(struct PDA_Data_t *pda_PdaConfig)
{
	int ret = 0;
	unsigned int i = 0;

	// Left image buffer
	ret = pda_get_dma_buffer(&g_image_mmu, pda_PdaConfig->FD_L_Image);
	if (ret < 0) {
		LOG_INF("Left image, pda_get_dma_buffer fail!\n");
		return ret;
	}
	g_Address_LI = (unsigned long) sg_dma_address(g_image_mmu.sgt->sgl);
	pda_PdaConfig->PDA_PDAI_P1_BASE_ADDR = (unsigned int)g_Address_LI;
	for (i = 0; i < g_PDA_quantity; i++) {
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAI_P1_BASE_ADDR_MSB_REG,
			(unsigned int)(g_Address_LI >> 32));
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAI_P1_BASE_ADDR_REG,
			(unsigned int)(g_Address_LI));
	}
#ifdef FOR_DEBUG
	LOG_INF("Left image MVA MSB = 0x%lx\n", g_Address_LI);
	for (i = 0; i < g_PDA_quantity; i++) {
		LOG_INF("Left image MVA MSB = 0x%x\n",
			PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P1_BASE_ADDR_MSB_REG));
		LOG_INF("Left image MVA = 0x%x\n",
			PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P1_BASE_ADDR_REG));
	}
#endif

	// Right image buffer
	g_Address_RI = g_Address_LI + pda_PdaConfig->ImageSize;
	pda_PdaConfig->PDA_PDAI_P2_BASE_ADDR = (unsigned int)(g_Address_RI);
	for (i = 0; i < g_PDA_quantity; i++) {
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAI_P2_BASE_ADDR_MSB_REG,
			(unsigned int)(g_Address_RI >> 32));
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAI_P2_BASE_ADDR_REG,
			(unsigned int)(g_Address_RI));
	}
#ifdef FOR_DEBUG
	LOG_INF("Right image MVA MSB = 0x%lx\n", g_Address_RI);
	for (i = 0; i < g_PDA_quantity; i++) {
		LOG_INF("Right image MVA MSB = 0x%x\n",
			PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P2_BASE_ADDR_MSB_REG));
		LOG_INF("Right image MVA = 0x%x\n",
			PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P2_BASE_ADDR_REG));
	}
#endif

	// Left table buffer
	ret = pda_get_dma_buffer(&g_table_mmu, pda_PdaConfig->FD_L_Table);
	if (ret < 0) {
		LOG_INF("Left table, pda_get_dma_buffer fail!\n");
		return ret;
	}
	g_Address_LT = (unsigned long) sg_dma_address(g_table_mmu.sgt->sgl);
	pda_PdaConfig->PDA_PDATI_P1_BASE_ADDR = (unsigned int)g_Address_LT;
	for (i = 0; i < g_PDA_quantity; i++) {
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDATI_P1_BASE_ADDR_MSB_REG,
			(unsigned int)(g_Address_LT >> 32));
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDATI_P1_BASE_ADDR_REG,
			(unsigned int)(g_Address_LT));
	}
#ifdef FOR_DEBUG
	LOG_INF("Left table MVA MSB = 0x%lx\n", g_Address_LT);
	for (i = 0; i < g_PDA_quantity; i++) {
		LOG_INF("Left table MVA MSB = 0x%x\n",
			PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P1_BASE_ADDR_MSB_REG));
		LOG_INF("Left table MVA = 0x%x\n",
			PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P1_BASE_ADDR_REG));
	}
#endif

	// Right table buffer
	g_Address_RT = g_Address_LT + pda_PdaConfig->TableSize;
	pda_PdaConfig->PDA_PDATI_P2_BASE_ADDR = (unsigned int)(g_Address_RT);
	for (i = 0; i < g_PDA_quantity; i++) {
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDATI_P2_BASE_ADDR_MSB_REG,
			(unsigned int)(g_Address_RT >> 32));
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDATI_P2_BASE_ADDR_REG,
			(unsigned int)(g_Address_RT));
	}
#ifdef FOR_DEBUG
	LOG_INF("Right table MVA MSB = 0x%lx\n", g_Address_RT);
	for (i = 0; i < g_PDA_quantity; i++) {
		LOG_INF("Right table MVA MSB = 0x%x\n",
			PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P2_BASE_ADDR_MSB_REG));
		LOG_INF("Right table MVA = 0x%x\n",
			PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P2_BASE_ADDR_REG));
	}
#endif

	return ret;
}

static int Get_Output_Addr_From_DMABUF(struct PDA_Data_t *pda_PdaConfig)
{
	int ret = 0;
	unsigned int i = 0;

	// Output buffer
	ret = pda_get_dma_buffer(&g_output_mmu, pda_PdaConfig->FD_Output);
	if (ret < 0) {
		LOG_INF("Output, pda_get_dma_buffer fail!\n");
		return ret;
	}
	g_OutputBufferAddr = (unsigned long) sg_dma_address(g_output_mmu.sgt->sgl);
	pda_PdaConfig->PDA_PDAO_P1_BASE_ADDR = (unsigned int)g_OutputBufferAddr;
	for (i = 0; i < g_PDA_quantity; i++) {
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAO_P1_BASE_ADDR_MSB_REG,
			(unsigned int)(g_OutputBufferAddr >> 32));
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAO_P1_BASE_ADDR_REG,
			(unsigned int)(g_OutputBufferAddr));
	}
#ifdef FOR_DEBUG
	LOG_INF("Output buffer MVA MSB = 0x%lx\n", g_OutputBufferAddr);
	for (i = 0; i < g_PDA_quantity; i++) {
		LOG_INF("Output buffer MVA MSB = 0x%x\n",
			PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAO_P1_BASE_ADDR_MSB_REG));
		LOG_INF("Output buffer MVA = 0x%x\n",
			PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAO_P1_BASE_ADDR_REG));
	}
#endif

	return ret;
}

static void HWDMASettings(struct PDA_Data_t *pda_PdaConfig)
{
	unsigned int i;

	for (i = 0; i < g_PDA_quantity; i++) {
		//--------- Frame setting part -----------
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_CFG_0_REG,
			pda_PdaConfig->PDA_FrameSetting.PDA_CFG_0.Raw);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_CFG_1_REG,
			pda_PdaConfig->PDA_FrameSetting.PDA_CFG_1.Raw);
		// need set roi number every process
		// PDA_WR32(PDA_devs[i].m_pda_base + PDA_CFG_2_REG,
		//     pda_PdaConfig->PDA_FrameSetting.PDA_CFG_2.Raw);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_CFG_3_REG,
			pda_PdaConfig->PDA_FrameSetting.PDA_CFG_3.Raw);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_CFG_4_REG,
			pda_PdaConfig->PDA_FrameSetting.PDA_CFG_4.Raw);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_CFG_5_REG,
			pda_PdaConfig->PDA_FrameSetting.PDA_CFG_5.Raw);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_CFG_6_REG,
			pda_PdaConfig->PDA_FrameSetting.PDA_CFG_6.Raw);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_CFG_7_REG,
			pda_PdaConfig->PDA_FrameSetting.PDA_CFG_7.Raw);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_CFG_8_REG,
			pda_PdaConfig->PDA_FrameSetting.PDA_CFG_8.Raw);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_CFG_9_REG,
			pda_PdaConfig->PDA_FrameSetting.PDA_CFG_9.Raw);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_CFG_10_REG,
			pda_PdaConfig->PDA_FrameSetting.PDA_CFG_10.Raw);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_CFG_11_REG,
			pda_PdaConfig->PDA_FrameSetting.PDA_CFG_11.Raw);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_CFG_12_REG,
			pda_PdaConfig->PDA_FrameSetting.PDA_CFG_12.Raw);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_CFG_13_REG,
			pda_PdaConfig->PDA_FrameSetting.PDA_CFG_13.Raw);


	//--------- Input buffer address -------------
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAI_P1_BASE_ADDR_REG,
			pda_PdaConfig->PDA_PDAI_P1_BASE_ADDR);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDATI_P1_BASE_ADDR_REG,
			pda_PdaConfig->PDA_PDATI_P1_BASE_ADDR);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAI_P2_BASE_ADDR_REG,
			pda_PdaConfig->PDA_PDAI_P2_BASE_ADDR);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDATI_P2_BASE_ADDR_REG,
			pda_PdaConfig->PDA_PDATI_P2_BASE_ADDR);

		//--------- DMA Secure part -------------
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDA_SECURE_REG, 0x00000000);

		// --------- config setting hard code part --------------
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAI_STRIDE_REG, 0x580);

		// Left image
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAI_P1_CON0_REG, 0x80000134);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAI_P1_CON1_REG, 0x104d004d);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAI_P1_CON2_REG, 0x009a009a);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAI_P1_CON3_REG, 0x00e700e7);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAI_P1_CON4_REG, 0x009a009a);

		// Left table
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDATI_P1_CON0_REG, 0x8000004c);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDATI_P1_CON1_REG, 0x10130013);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDATI_P1_CON2_REG, 0x00260026);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDATI_P1_CON3_REG, 0x00390039);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDATI_P1_CON4_REG, 0x00260026);

		// Right image
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAI_P2_CON0_REG, 0x80000134);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAI_P2_CON1_REG, 0x104d004d);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAI_P2_CON2_REG, 0x009a009a);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAI_P2_CON3_REG, 0x00e700e7);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAI_P2_CON4_REG, 0x009a009a);

		// Right table
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDATI_P2_CON0_REG, 0x8000004c);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDATI_P2_CON1_REG, 0x10130013);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDATI_P2_CON2_REG, 0x00260026);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDATI_P2_CON3_REG, 0x00390039);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDATI_P2_CON4_REG, 0x00260026);

		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAO_P1_XSIZE_REG, 0x0000057f);

		// Output
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAO_P1_CON0_REG, 0x80000040);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAO_P1_CON1_REG, 0x10100010);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAO_P1_CON2_REG, 0x00200020);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAO_P1_CON3_REG, 0x00300030);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAO_P1_CON4_REG, 0x00200020);

		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDA_DMA_EN_REG, 0x0000001f);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDA_DMA_RST_REG, 0x1);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDA_DMA_TOP_REG, 0x407);

		// DCM all off: 0x0000007F
		// DCM all on:  0x00000000
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDA_DCM_DIS_REG, 0x0000007F);

		//disable dma error irq: 0x00000000
		//enable dma error irq: 0xffff0000
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAI_P1_ERR_STAT_REG,
			0x00000000);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDATI_P1_ERR_STAT_REG,
			0x00000000);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAI_P2_ERR_STAT_REG,
			0x00000000);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDATI_P2_ERR_STAT_REG,
			0x00000000);
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDAO_P1_ERR_STAT_REG,
			0x00000000);

		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDA_TOP_CTL_REG, 0x00000000);

		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDA_IRQ_TRIG_REG, 0x0);

		// setting read clear
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDA_ERR_STAT_EN_REG, 0x00000001);

		// read 0x3b4, avoid the impact of previous data
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDA_ERR_STAT_REG);
		// read clear dma status
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P1_ERR_STAT_REG);
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P1_ERR_STAT_REG);
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P2_ERR_STAT_REG);
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P2_ERR_STAT_REG);
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAO_P1_ERR_STAT_REG);
	}
}

static int ProcessROIData(struct PDA_Data_t *pda_data,
				unsigned int RoiProcNum,
				unsigned int ROIIndex,
				unsigned int isFixROI,
				unsigned int p_index)
{
	unsigned int i = 0, j = 0;
	unsigned int xnum = 0, ynum = 0;
	unsigned int woverlap = 0, hoverlap = 0;
	unsigned int x_p, y_p, w_p, h_p;
	unsigned int nWidth = 0, nHeight = 0;
	int nLocalBufIndex = 0;
	unsigned int nXDirLoopCount = 0;

	if (isFixROI) {
#ifdef FOR_DEBUG
		LOG_INF("Sequential ROI\n");
#endif
		xnum = pda_data->xnum[p_index];
		ynum = pda_data->ynum[p_index];
		woverlap = (int)pda_data->woverlap[p_index];
		hoverlap = (int)pda_data->hoverlap[p_index];
		x_p = pda_data->fix_rgn_x[p_index];
		y_p = pda_data->fix_rgn_y[p_index];
		w_p = pda_data->fix_rgn_w[p_index];
		h_p = pda_data->fix_rgn_h[p_index];

		if (xnum <= 0 || ynum <= 0) {
			LOG_INF("xnum(%d) or ynum(%d) value is invalid\n", xnum, ynum);
			return -1;
		}

		nWidth = w_p / xnum;
		nHeight = h_p / ynum;

#ifdef FOR_DEBUG
		LOG_INF("p_index(%d), nWidth(%d), nHeight(%d)\n", p_index, nWidth, nHeight);
		LOG_INF("xnum(%d), ynum(%d)\n", xnum, ynum);
		LOG_INF("woverlap(%d), hoverlap(%d)\n", woverlap, hoverlap);
		LOG_INF("x_p(%d), y_p(%d), w_p(%d), h_p(%d)\n", x_p, y_p, w_p, h_p);
#endif

		if (w_p < xnum || h_p < ynum) {
			LOG_INF("w_p(%d)/h_p(%d) can't less than xnum(%d)/ynum(%d)\n",
				w_p, h_p, xnum, ynum);
			return -1;
		}

		if (pda_data->PDA_FrameSetting.PDA_CFG_1.Bits.PDA_PR_XNUM == 0) {
			LOG_INF("Fail: PDA_PR_XNUM is zero\n");
			return -1;
		}

		for (j = (ROIIndex / xnum); j < ynum; j++) {
			i = (nXDirLoopCount == 0 ? (ROIIndex % xnum) : 0);
			for (; i < xnum; i++) {
				nLocalBufIndex = j * xnum + i - ROIIndex;

				if (nLocalBufIndex >= PDA_MAXROI_PER_ROUND || nLocalBufIndex < 0) {
					LOG_INF("nLocalBufIndex out of range (%d)\n",
						nLocalBufIndex);
					return -1;
				}

				if (i != 0 && i != xnum - 1) {
					g_rgn_x_buf[nLocalBufIndex] =
						x_p + nWidth * i - nWidth * woverlap / 100;
					g_rgn_w_buf[nLocalBufIndex] =
						nWidth + 2 * nWidth * woverlap / 100;
				} else if (i == 0) {
					g_rgn_x_buf[nLocalBufIndex] = x_p + nWidth * i;
					g_rgn_w_buf[nLocalBufIndex] =
						nWidth + nWidth * woverlap / 100;
				} else {
					g_rgn_x_buf[nLocalBufIndex] =
						x_p + nWidth * i - nWidth * woverlap / 100;
					g_rgn_w_buf[nLocalBufIndex] =
						nWidth + nWidth * woverlap / 100;
				}

				if (j != 0 && j != ynum - 1) {
					g_rgn_y_buf[nLocalBufIndex] =
						y_p + nHeight * j - nHeight * hoverlap / 100;
					g_rgn_h_buf[nLocalBufIndex] =
						nHeight + 2 * nHeight * hoverlap / 100;
				} else if (j == 0) {
					g_rgn_y_buf[nLocalBufIndex] = y_p + nHeight * j;
					g_rgn_h_buf[nLocalBufIndex] =
						nHeight + nHeight * hoverlap / 100;
				} else {
					g_rgn_y_buf[nLocalBufIndex] =
						y_p + nHeight * j - nHeight * hoverlap / 100;
					g_rgn_h_buf[nLocalBufIndex] =
						nHeight + nHeight * hoverlap / 100;
				}

				g_rgn_iw_buf[nLocalBufIndex] =
					g_rgn_w_buf[nLocalBufIndex] *
					(pda_data->PDA_FrameSetting.PDA_CFG_1.Bits.PDA_PAT_WIDTH /
					pda_data->PDA_FrameSetting.PDA_CFG_1.Bits.PDA_PR_XNUM);

#ifdef FOR_DEBUG
				LOG_INF("ROI_IW_%d, iw:%d, w:%d, pat_width:%d\n",
					nLocalBufIndex,
					g_rgn_iw_buf[nLocalBufIndex],
					g_rgn_w_buf[nLocalBufIndex],
					pda_data->PDA_FrameSetting.PDA_CFG_1.Bits.PDA_PAT_WIDTH);
#endif

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
	unsigned int i = 0;
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

#ifdef FOR_DEBUG
		LOG_INF("nROIIndex:%d, i:%d\n", nROIIndex, i);
#endif

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
		if ((g_rgn_x_buf[i]+g_rgn_w_buf[i]) > nTempVar) {
			LOG_INF("ROI_%d ROI exceed the image region\n", nROIIndex);
			PDA_Data->Status = -11;
			return -1;
		}

		nTempVar = PDA_Data->PDA_FrameSetting.PDA_CFG_0.Bits.PDA_HEIGHT;
		if ((g_rgn_y_buf[i]+g_rgn_h_buf[i]) > nTempVar) {
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
			LOG_INF("ROI_IW_%d (%d) out of range, w:%d, pat_width:%d\n",
				nROIIndex,
				g_rgn_iw_buf[i],
				g_rgn_w_buf[i],
				PDA_Data->PDA_FrameSetting.PDA_CFG_1.Bits.PDA_PAT_WIDTH);
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
				unsigned long OuputAddr,
				unsigned int PDA_Index)
{
	unsigned int RegIndex = 14;
	unsigned int ROI_MAX_INDEX = RoiProcNum-1;
	unsigned int pair = 0;

	// modify roi number register, change [11:6] bit to RoiProcNum
	pda_PdaConfig->PDA_FrameSetting.PDA_CFG_2.Bits.PDA_RGN_NUM = RoiProcNum;

	// roi number register setting
	PDA_WR32(PDA_devs[PDA_Index].m_pda_base + PDA_CFG_2_REG,
		pda_PdaConfig->PDA_FrameSetting.PDA_CFG_2.Raw);

	// 1024 ROI data sequentially fill to PDA_CFG[14] ~ PDA_CFG[126]
	for (pair = 0; pair <= ROI_MAX_INDEX; pair += 2) {
		PDA_WR32((PDA_devs[PDA_Index].m_pda_base + 0x004*(RegIndex++)),
			g_rgn_y_buf[pair]*65536 + g_rgn_x_buf[pair]);
		PDA_WR32((PDA_devs[PDA_Index].m_pda_base + 0x004*(RegIndex++)),
			g_rgn_h_buf[pair]*65536 + g_rgn_w_buf[pair]);
		if (pair == ROI_MAX_INDEX && pair%2 == 0) {
			PDA_WR32((PDA_devs[PDA_Index].m_pda_base + 0x004*(RegIndex++)),
				g_rgn_iw_buf[pair]);
		} else {
			PDA_WR32((PDA_devs[PDA_Index].m_pda_base + 0x004*(RegIndex++)),
				g_rgn_x_buf[pair+1]*65536 + g_rgn_iw_buf[pair]);
			PDA_WR32((PDA_devs[PDA_Index].m_pda_base + 0x004*(RegIndex++)),
				g_rgn_w_buf[pair+1]*65536 + g_rgn_y_buf[pair+1]);
			PDA_WR32((PDA_devs[PDA_Index].m_pda_base + 0x004*(RegIndex++)),
				g_rgn_iw_buf[pair+1]*65536 + g_rgn_h_buf[pair+1]);
		}
	}

	// output buffer address setting
	PDA_WR32(PDA_devs[PDA_Index].m_pda_base + PDA_PDAO_P1_BASE_ADDR_MSB_REG,
		(unsigned int)(OuputAddr >> 32));
	PDA_WR32(PDA_devs[PDA_Index].m_pda_base + PDA_PDAO_P1_BASE_ADDR_REG,
		(unsigned int)(OuputAddr));

#ifdef FOR_DEBUG
		LOG_INF("Fill Register Settings done\n");
#endif
}

static void LOGHWRegister(unsigned int i)
{
	LOG_INF("CFG_0/1/2/3/4/5/6: 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x\n",
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_0_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_1_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_2_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_3_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_4_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_5_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_6_REG));

	LOG_INF("CFG_7/8/9/10/11/12/13: 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x\n",
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_7_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_8_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_9_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_10_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_11_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_12_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_13_REG));

	LOG_INF("CFG_14/15/16/17/18/19/20: 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x\n",
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_14_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_15_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_16_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_17_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_18_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_19_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_20_REG));

	LOG_INF("CFG_21/22/23/24/25/26/27/28: 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x\n",
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_21_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_22_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_23_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_24_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_25_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_26_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_27_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_CFG_28_REG));

	LOG_INF("I_P1/TI_P1/I_P2/TI_P2/Out: 0x%x/0x%x/0x%x/0x%x/0x%x\n",
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P1_BASE_ADDR_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P1_BASE_ADDR_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P2_BASE_ADDR_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P2_BASE_ADDR_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAO_P1_BASE_ADDR_REG));

	LOG_INF("SECURE/I_STRIDE/O_P1_XSIZE/TILE_STATUS: 0x%x/0x%x/0x%x/0x%x\n",
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDA_SECURE_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_STRIDE_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAO_P1_XSIZE_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDA_TILE_STATUS_REG));

	LOG_INF("PDAI_P1_CON0/CON1/CON2/CON3/CON4: 0x%x/0x%x/0x%x/0x%x/0x%x\n",
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P1_CON0_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P1_CON1_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P1_CON2_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P1_CON3_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P1_CON4_REG));

	LOG_INF("PDATI_P1_CON0/CON1/CON2/CON3/CON4: 0x%x/0x%x/0x%x/0x%x/0x%x\n",
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P1_CON0_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P1_CON1_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P1_CON2_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P1_CON3_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P1_CON4_REG));

	LOG_INF("PDAI_P2_CON0/CON1/CON2/CON3/CON4: 0x%x/0x%x/0x%x/0x%x/0x%x\n",
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P2_CON0_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P2_CON1_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P2_CON2_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P2_CON3_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P2_CON4_REG));

	LOG_INF("PDATI_P2_CON0/CON1/CON2/CON3/CON4: 0x%x/0x%x/0x%x/0x%x/0x%x\n",
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P2_CON0_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P2_CON1_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P2_CON2_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P2_CON3_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P2_CON4_REG));

	LOG_INF("PDAO_P1_CON0/CON1/CON2/CON3/CON4: 0x%x/0x%x/0x%x/0x%x/0x%x\n",
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAO_P1_CON0_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAO_P1_CON1_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAO_P1_CON2_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAO_P1_CON3_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAO_P1_CON4_REG));

	LOG_INF("DMA_EN/DMA_RST/DMA_TOP/DCM_DIS/DCM_ST: 0x%x/0x%x/0x%x/0x%x/0x%x\n",
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDA_DMA_EN_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDA_DMA_RST_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDA_DMA_TOP_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDA_DCM_DIS_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDA_DCM_ST_REG));

	LOG_INF("[ERR_STAT]I_P1/TI_P1/I_P2/TI_P2/O_P1: 0x%x/0x%x/0x%x/0x%x/0x%x\n",
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P1_ERR_STAT_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P1_ERR_STAT_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P2_ERR_STAT_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P2_ERR_STAT_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAO_P1_ERR_STAT_REG));

	LOG_INF("ERR_STAT_EN/ERR_STAT/TOP_CTL: 0x%x/0x%x/0x%x\n",
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDA_ERR_STAT_EN_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDA_ERR_STAT_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDA_TOP_CTL_REG));

	LOG_INF("IRQ_TRIG/PDAO_DMA_EXISTED_ECO: 0x%x/0x%x\n",
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDA_IRQ_TRIG_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAO_DMA_EXISTED_ECO_REG));

	LOG_INF("[MSB]I_P1/TI_P1/I_P2/TI_P2/O_P1: 0x%x/0x%x/0x%x/0x%x/0x%x\n",
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P1_BASE_ADDR_MSB_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P1_BASE_ADDR_MSB_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P2_BASE_ADDR_MSB_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P2_BASE_ADDR_MSB_REG),
		PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAO_P1_BASE_ADDR_MSB_REG));
}

static void TF_dump_log(unsigned int hw_trigger_num)
{
	unsigned int i = 0;
	unsigned int sel_index = 0;

	unsigned int Debug_Sel[] = {0x00008120, 0x0000400e, 0x0000c000,
		0x11000000, 0x12000000, 0x13000000, 0x14000000, 0x15000000, 0x16000000,
		0x14000000, 0x14100000, 0x14200000, 0x14300000, 0x14400000, 0x14500000,
		0x21000000, 0x22000000, 0x23000000, 0x24000000, 0x25000000,
		0x24000000, 0x24100000, 0x24200000, 0x24300000, 0x24400000, 0x24500000,
		0x31000000, 0x32000000, 0x33000000, 0x34000000, 0x35000000,
		0x34000000, 0x34100000, 0x34200000, 0x34300000, 0x34400000, 0x34500000,
		0x41000000, 0x42000000, 0x43000000, 0x44000000, 0x45000000,
		0x44000000, 0x44100000, 0x44200000, 0x44300000, 0x44400000, 0x44500000,
		0x51000000, 0x52000000, 0x53000000, 0x54000000, 0x55000000,
		0x54000000, 0x54100000, 0x54200000, 0x54300000, 0x54400000, 0x54500000};
	unsigned int Length_Arr = sizeof(Debug_Sel)/sizeof(*Debug_Sel);

#ifdef SMI_LOG
	// SMI log
	mtk_smi_dbg_hang_detect("PDA device");
#endif

	// check debug data
	for (i = 0; i < hw_trigger_num; i++) {
		for (sel_index = 0; sel_index < Length_Arr; ++sel_index) {
			PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDA_DEBUG_SEL_REG,
				Debug_Sel[sel_index]);
			LOG_INF("PDA_%d DEBUG_SEL/DEBUG_DATA: 0x%x/0x%x\n",
				i, PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDA_DEBUG_SEL_REG),
				PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDA_DEBUG_DATA_REG));
		}
	}

	// check hw register setting
	for (i = 0; i < hw_trigger_num; i++) {
		LOG_INF("[outer] PDA_%d register LOG +++++\n", i);
		LOGHWRegister(i);
	}
}

static void TimeoutHandler(unsigned int hw_trigger_num)
{
	unsigned int i = 0;

	TF_dump_log(hw_trigger_num);

	// reset flow
	for (i = 0; i < hw_trigger_num; i++) {
		pda_reset(i);
		pda_nontransaction_reset(i);
	}
}

static void pda_execute(unsigned int hw_trigger_num)
{
	unsigned int i;

#ifdef FOR_DEBUG
	LOG_INF("+\n");
#endif

	for (i = 0; i < hw_trigger_num; i++) {
		// PDA_TOP_CTL set 1'b1 to bit3, to load register from double buffer
		PDA_WR32(PDA_devs[i].m_pda_base  + PDA_PDA_TOP_CTL_REG, PDA_DOUBLE_BUFFER);

		// make sure all the pda setting take effect
		wmb();

		// PDA_TOP_CTL set 1'b1 to bit1, to trigger sof
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDA_TOP_CTL_REG, PDA_TRIGGER);

		// make sure all the pda setting take effect
		wmb();

		// write 0 after trigger
		PDA_WR32(PDA_devs[i].m_pda_base + PDA_PDA_TOP_CTL_REG, PDA_CLEAR_REG);

#ifdef GET_PDA_TIME
		if (i == 0)
			ktime_get_real_ts64(&pda1_done_b);
		else
			ktime_get_real_ts64(&pda2_done_b);
#endif
	}

#ifdef FOR_DEBUG
	LOG_INF("-\n");
#endif
}

static int check_pda_status(unsigned int hw_trigger_num)
{
	unsigned int i = 0;

	for (i = 0; i < hw_trigger_num; i++) {
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

static signed int pda_wait_irq(struct PDA_Data_t *pda_data, unsigned int hw_trigger_num)
{
	int ret = 0;
	unsigned int i = 0;

	/* start to wait signal */
	ret = wait_event_interruptible_timeout(g_wait_queue_head,
						check_pda_status(hw_trigger_num),
						pda_ms_to_jiffies(pda_data->Timeout));

	if (ret == 0) {
		TimeoutHandler(hw_trigger_num);

		// timeout error
		LOG_INF("wait_event_interruptible_timeout Fail\n");
		pda_data->Status = -2;
		return -1;
	} else if (ret < 0) {
		LOG_INF("wait_event return value:%d\n", ret);
		if (ret == -ERESTARTSYS)
			LOG_INF("Interrupted by a signal\n");
		pda_data->Status = -3;
		for (i = 0; i < hw_trigger_num; i++) {
			pda_reset(i);
			pda_nontransaction_reset(i);
		}
		return -1;
	}

#ifdef GET_PDA_TIME
	ktime_get_real_ts64(&time_end);
#endif

	// pda status
	pda_data->Status = 1;

	// update status to user
	for (i = 0; i < hw_trigger_num; i++) {
		if (PDA_devs[i].HWstatus < 0) {
			pda_data->Status = PDA_devs[i].HWstatus;

			LOG_INF("PDA%d HW error (%d)", i, PDA_devs[i].HWstatus);

			LOG_INF("PDA%d PDA_PDAI_P1_ERR_STAT_REG = 0x%x",
				i, PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P1_ERR_STAT_REG));
			LOG_INF("PDA%d PDA_PDATI_P1_ERR_STAT_REG = 0x%x",
				i, PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P1_ERR_STAT_REG));
			LOG_INF("PDA%d PDA_PDAI_P2_ERR_STAT_REG = 0x%x",
				i, PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P2_ERR_STAT_REG));
			LOG_INF("PDA%d PDA_PDATI_P2_ERR_STAT_REG = 0x%x",
				i, PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P2_ERR_STAT_REG));
			LOG_INF("PDA%d PDA_PDAO_P1_ERR_STAT_REG = 0x%x",
				i, PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAO_P1_ERR_STAT_REG));

			// reset flow
			pda_nontransaction_reset(i);
		}
	}

	return ret;
}

static irqreturn_t pda_irqhandle(signed int Irq, void *DeviceId)
{
	unsigned int nPdaStatus = 0;

	// read pda status
	nPdaStatus = PDA_RD32(PDA_devs[0].m_pda_base + PDA_PDA_ERR_STAT_REG) &
		PDA_STATUS_REG;

#ifdef FOR_DEBUG
	LOG_INF("PDA0 PDA_PDA_ERR_STAT_REG = 0x%x", nPdaStatus);
#endif

	// for WCL=1 case, write 1 to clear pda done status
	// PDA_WR32(PDA_devs[0].m_pda_base + PDA_PDA_ERR_STAT_REG, 0x00000001);

	PDA_devs[0].HWstatus = 1;

#ifdef GET_PDA_TIME
	ktime_get_real_ts64(&pda1_done_e);
#endif

#ifdef CHECK_IRQ_COUNT
	++g_PDA0_IRQCount;
	if (g_PDA0_IRQCount > g_reasonable_IRQCount) {
		PDA_devs[0].HWstatus = -29;
		pda_nontransaction_reset(0);
	}
#endif

	// wake up user space WAIT_IRQ flag
	wake_up_interruptible(&g_wait_queue_head);
	return IRQ_HANDLED;
}

static irqreturn_t pda2_irqhandle(signed int Irq, void *DeviceId)
{
	unsigned int nPdaStatus = 0;

	// read pda status
	nPdaStatus = PDA_RD32(PDA_devs[1].m_pda_base + PDA_PDA_ERR_STAT_REG) &
		PDA_STATUS_REG;

#ifdef FOR_DEBUG
	LOG_INF("PDA1 PDA_PDA_ERR_STAT_REG = 0x%x", nPdaStatus);
#endif

	// for WCL=1 case, write 1 to clear pda done status
	// PDA_WR32(PDA_devs[1].m_pda_base + PDA_PDA_ERR_STAT_REG, 0x00000001);

	PDA_devs[1].HWstatus = 1;

#ifdef GET_PDA_TIME
	ktime_get_real_ts64(&pda2_done_e);
#endif

#ifdef CHECK_IRQ_COUNT
	++g_PDA1_IRQCount;
	if (g_PDA1_IRQCount > g_reasonable_IRQCount) {
		PDA_devs[1].HWstatus = -29;
		pda_nontransaction_reset(1);
	}
#endif

	// wake up user space WAIT_IRQ flag
	wake_up_interruptible(&g_wait_queue_head);
	return IRQ_HANDLED;
}

static int PDAProcessFunction(unsigned int nUserROINumber,
				unsigned int nROIcount,
				unsigned int isFixROI,
				unsigned int p_index)
{
	unsigned int i = 0;
	unsigned int nOneRoundProcROI = 0;
	unsigned int hw_trigger_num = 0;
	unsigned int nRemainder = 0, nFactor = 0;
	unsigned int nCurrentProcRoiIndex = 0;
	unsigned long nOutputAddr = 0;
	int nirqRet = 0;
	unsigned int CheckAddress = 0, CheckAddressMSB = 0;

	while (nROIcount > 0) {

		// read 0x3b4, avoid the impact of previous data
		// LOG_INF("PDA status before process = %d\n",
		//	PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDA_ERR_STAT_REG));
#ifdef FOR_DEBUG
		LOG_INF("nROIcount = %d\n", nROIcount);
#endif
		// reset HW status
		for (i = 0; i < g_PDA_quantity; i++)
			PDA_devs[i].HWstatus = 0;

		// reset local variable
		nOneRoundProcROI = 0;

		//reset hw trigger number
		hw_trigger_num = g_PDA_quantity;

		// assign strategy, used for multi-engine
		if (nROIcount >= (PDA_MAXROI_PER_ROUND * g_PDA_quantity)) {
			for (i = 0; i < g_PDA_quantity; i++) {
				g_CurrentProcRoiNum[i] = PDA_MAXROI_PER_ROUND;
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
				return -1;
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

		// data preparing
		for (i = 0; i < g_PDA_quantity; i++) {
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

			if (g_CurrentProcRoiNum[i] == 0) {
				hw_trigger_num = i;
#ifdef FOR_DEBUG
				LOG_INF("assign roi number is zero, no need to process\n");
#endif
				break;
			}

#ifdef FOR_DEBUG
			LOG_INF("i:%d, g_CurrentProcRoiNum:%d, nCurrentProcRoiIndex:%d\n",
				i,
				g_CurrentProcRoiNum[i],
				nCurrentProcRoiIndex);
#endif

			// calculate 1024 ROI data
			if (ProcessROIData(&g_pda_Pdadata,
					g_CurrentProcRoiNum[i],
					nCurrentProcRoiIndex,
					isFixROI,
					p_index) < 0) {
				LOG_INF("ProcessROIData Fail\n");
				g_pda_Pdadata.Status = -24;
				return -1;
			}

			if (CheckDesignLimitation(&g_pda_Pdadata,
					g_CurrentProcRoiNum[i],
					nCurrentProcRoiIndex) < 0) {
				LOG_INF("CheckDesignLimitation Fail\n");
				return -1;
			}

			// output address is equal to
			// total ROI number multiple by 1408
			nOutputAddr = g_OutputBufferAddr;
			nOutputAddr += nCurrentProcRoiIndex * OUT_BYTE_PER_ROI;

			if (g_OutputBufferOffset + (g_CurrentProcRoiNum[i]*OUT_BYTE_PER_ROI) >
				g_pda_Pdadata.OutputSize) {
				LOG_INF("fail, output buffer out of range\n");
				LOG_INF("Current output buffer addr: 0x%lx\n",
					nOutputAddr);
				LOG_INF("Base output buffer addr: 0x%lx\n",
					nOutputAddr);
				LOG_INF("PDA%d ROI process num: %d\n",
					i, g_CurrentProcRoiNum[i]);
				LOG_INF("Output buffer size: %d\n",
					g_pda_Pdadata.OutputSize);
				LOG_INF("Current process ROI index: %d\n",
					nCurrentProcRoiIndex);
				LOG_INF("nUserROINumber: %d\n",
					nUserROINumber);
				LOG_INF("nROIcount: %d\n", nROIcount);
				g_pda_Pdadata.Status = -30;
				return -1;
			}

#ifdef FOR_DEBUG
			LOG_INF("(nOutputAddr+g_OutputBufferOffset): 0x%lx\n",
				(nOutputAddr+g_OutputBufferOffset));
#endif

			FillRegSettings(&g_pda_Pdadata,
				g_CurrentProcRoiNum[i],
				(nOutputAddr+g_OutputBufferOffset),
				i);
		}

		// check input/output buffer address is valid
		for (i = 0; i < hw_trigger_num; i++) {
			CheckAddress =
			PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P1_BASE_ADDR_REG);
			CheckAddressMSB =
			PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P1_BASE_ADDR_MSB_REG);
			if (CheckAddress == 0 && CheckAddressMSB == 0) {
				LOG_INF("PDA_%d PDA_PDAI_P1_BASE_ADDR is zero\n", i);
				g_pda_Pdadata.Status = -30;
				return -1;
			}

			CheckAddress =
			PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P1_BASE_ADDR_REG);
			CheckAddressMSB =
			PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P1_BASE_ADDR_MSB_REG);
			if (CheckAddress == 0 && CheckAddressMSB == 0) {
				LOG_INF("PDA_%d PDA_PDATI_P1_BASE_ADDR is zero\n", i);
				g_pda_Pdadata.Status = -30;
				return -1;
			}

			CheckAddress =
			PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P2_BASE_ADDR_REG);
			CheckAddressMSB =
			PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAI_P2_BASE_ADDR_MSB_REG);
			if (CheckAddress == 0 && CheckAddressMSB == 0) {
				LOG_INF("PDA_%d PDA_PDAI_P2_BASE_ADDR is zero\n", i);
				g_pda_Pdadata.Status = -30;
				return -1;
			}

			CheckAddress =
			PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P2_BASE_ADDR_REG);
			CheckAddressMSB =
			PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDATI_P2_BASE_ADDR_MSB_REG);
			if (CheckAddress == 0 && CheckAddressMSB == 0) {
				LOG_INF("PDA_%d PDA_PDATI_P2_BASE_ADDR is zero\n", i);
				g_pda_Pdadata.Status = -30;
				return -1;
			}

			CheckAddress =
			PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAO_P1_BASE_ADDR_REG);
			CheckAddressMSB =
			PDA_RD32(PDA_devs[i].m_pda_base + PDA_PDAO_P1_BASE_ADDR_MSB_REG);
			if (CheckAddress == 0 && CheckAddressMSB == 0) {
				LOG_INF("PDA_%d PDA_PDAO_P1_BASE_ADDR is zero\n", i);
				g_pda_Pdadata.Status = -30;
				return -1;
			}
		}

		// trigger PDA work
		pda_execute(hw_trigger_num);

		nirqRet = pda_wait_irq(&g_pda_Pdadata, hw_trigger_num);

		if (nirqRet < 0) {
			LOG_INF("pda_wait_irq Fail (%d)\n", nirqRet);
			return -1;
		}

		// update roi count which needed to process
		nROIcount -= nOneRoundProcROI;
	}
	return 1;
}

static long PDA_Ioctl(struct file *a_pstFile,
			unsigned int a_u4Command,
			unsigned long a_u4Param)
{
	long nRet = 0;
	int nROIcount = 0;
	unsigned int nUserROINumber = 0;
	unsigned int i;
	int ret = 0;
	struct PDA_Init_Data Init_Data;

	if (g_PDA_quantity == 0) {
		LOG_INF("no PDA support\n");
		return -1;
	}

	switch (a_u4Command) {
	case PDA_RESET:
		for (i = 0; i < g_PDA_quantity; i++)
			pda_reset(i);
		break;
	case PDA_GET_VERSION:

		if (copy_from_user(&Init_Data,
				   (void *)a_u4Param,
				   sizeof(struct PDA_Init_Data)) != 0) {
			LOG_INF("PDA_GET_VERSION copy_from_user failed\n");
			nRet = -EFAULT;
			break;
		}

		Init_Data.Kversion = KERNEL_VERSION;
		LOG_INF("kernel version: %d\n", Init_Data.Kversion);

		if (copy_to_user((void *)a_u4Param,
		    &Init_Data,
		    sizeof(struct PDA_Init_Data)) != 0) {
			LOG_INF("PDA_GET_VERSION copy_to_user failed\n");
			nRet = -EFAULT;
		}

		break;
	case PDA_ENQUE_WAITIRQ:

		spin_lock(&g_PDA_SpinLock);
		if (g_u4EnableClockCount == 0) {
			LOG_INF("Cannot process without enable pda clock\n");
			spin_unlock(&g_PDA_SpinLock);
			return -1;
		}
		spin_unlock(&g_PDA_SpinLock);

#ifdef GET_PDA_TIME
		ktime_get_real_ts64(&total_time_begin);
#endif

		// MRAW PDA reset
		for (i = 0; i < g_PDA_quantity; i++)
			pda_nontransaction_reset(i);

		// reset HW status
		for (i = 0; i < g_PDA_quantity; i++)
			PDA_devs[i].HWstatus = 0;

#ifdef CHECK_IRQ_COUNT
		// reset PDA0/PDA1 IRQ count
		g_PDA0_IRQCount = 0;
		g_PDA1_IRQCount = 0;
#ifdef FOR_DEBUG
		LOG_INF("PDA0_IRQCount = %d, PDA1_IRQCount = %d\n",
			g_PDA0_IRQCount, g_PDA1_IRQCount);
#endif
#endif

		// reset output buffer address offset
		g_OutputBufferOffset = 0;

		if (copy_from_user(&g_pda_Pdadata,
				   (void *)a_u4Param,
				   sizeof(struct PDA_Data_t)) != 0) {
			LOG_INF("PDA_ENQUE_WAITIRQ copy_from_user failed\n");
			nRet = -EFAULT;
			break;
		}

		ret = g_pda_Pdadata.ROInumber == 0 && g_pda_Pdadata.nNumerousROI == 0;
		if (g_pda_Pdadata.ROInumber > PDAROIARRAYMAX || ret) {
			g_pda_Pdadata.Status = -28;
			LOG_INF("ROI number out of range, ROInumber/nNumerousROI:%d/%d\n",
				g_pda_Pdadata.ROInumber,
				g_pda_Pdadata.nNumerousROI);
			LOG_INF("rgn:%d,fix:%d,xnum:%d,siz:%d,FD:%d,cfg0:0x%x,sta:%d/%d\n",
				g_pda_Pdadata.rgn_h[0],
				g_pda_Pdadata.fix_rgn_h[0],
				g_pda_Pdadata.xnum[0],
				g_pda_Pdadata.OutputSize,
				g_pda_Pdadata.FD_Output,
				g_pda_Pdadata.PDA_FrameSetting.PDA_CFG_0.Raw,
				g_pda_Pdadata.Status,
				g_pda_Pdadata.Timeout);
			goto EXIT_WITHOUT_FREE_IOVA;
		}

		if (Get_Input_Addr_From_DMABUF(&g_pda_Pdadata) < 0) {
			g_pda_Pdadata.Status = -26;
			LOG_INF("Get_Input_Addr_From_DMABUF fail\n");
			goto EXIT_WITHOUT_FREE_IOVA;
		}

			// output buffer mapping iova
		if (Get_Output_Addr_From_DMABUF(&g_pda_Pdadata) < 0) {
			g_pda_Pdadata.Status = -27;
			LOG_INF("Get_Output_Addr_From_DMABUF fail\n");
			goto INPUT_BUFFER_FREE_IOVA;
		}

		// PDA HW and DMA setting
		HWDMASettings(&g_pda_Pdadata);

		// ------------------------ PDA pre-process done -----------------

		// Process flexible roi
		nUserROINumber = g_pda_Pdadata.ROInumber;
#ifdef FOR_DEBUG
		LOG_INF("nUserROINumber = %d\n", nUserROINumber);
		LOG_INF("g_PDA_quantity = %d\n", g_PDA_quantity);
#endif

		if (nUserROINumber == 0) {
#ifdef FOR_DEBUG
			LOG_INF("no flexible roi needed\n");
#endif
			goto FIX_ROI;
		}

#ifdef CHECK_IRQ_COUNT
		// setting reasonable IRQ count
		g_reasonable_IRQCount = 1 +
			(int)((nUserROINumber-1)/(PDA_MAXROI_PER_ROUND*g_PDA_quantity));
		// reset PDA0/PDA1 IRQ count
		g_PDA0_IRQCount = 0;
		g_PDA1_IRQCount = 0;
#ifdef FOR_DEBUG
		LOG_INF("g_reasonable_IRQCount = %d\n", g_reasonable_IRQCount);
#endif
#endif

		// Init ROI count which needed to process
		nROIcount = nUserROINumber;

		if (PDAProcessFunction(nUserROINumber, nROIcount, MFALSE, 0) < 0) {
			LOG_INF("Flexible ROI, PDA process fail\n");
			goto EXIT;
		}

		g_OutputBufferOffset += nUserROINumber*OUT_BYTE_PER_ROI;
#ifdef FOR_DEBUG
		LOG_INF("g_OutputBufferOffset: %d\n", g_OutputBufferOffset);
#endif

FIX_ROI:
		// Process fix roi
#ifdef FOR_DEBUG
		LOG_INF("nNumerousROI:%d\n", g_pda_Pdadata.nNumerousROI);
#endif

		if (g_pda_Pdadata.nNumerousROI > FIXROIARRAYMAX) {
			g_pda_Pdadata.Status = -28;
			LOG_INF("ROI number out of range,nNumerousROI:%d\n",
				g_pda_Pdadata.nNumerousROI);
			goto EXIT;
		}

		for (i = 0; i < g_pda_Pdadata.nNumerousROI; ++i) {
			nUserROINumber = g_pda_Pdadata.xnum[i]*g_pda_Pdadata.ynum[i];

#ifdef FOR_DEBUG
			LOG_INF("nUserROINumber = %d\n", nUserROINumber);
#endif

			if (nUserROINumber == 0 || nUserROINumber > 1024) {
				g_pda_Pdadata.Status = -28;
				LOG_INF("ROI number out of range,idx/ROI/x/ynum:%d/%d/%d/%d\n",
					i,
					nUserROINumber,
					g_pda_Pdadata.xnum[i],
					g_pda_Pdadata.ynum[i]);
				goto EXIT;
			}

#ifdef CHECK_IRQ_COUNT
			// setting reasonable IRQ count
			g_reasonable_IRQCount = 1 +
				(int)((nUserROINumber-1)/(PDA_MAXROI_PER_ROUND*g_PDA_quantity));
			// reset PDA0/PDA1 IRQ count
			g_PDA0_IRQCount = 0;
			g_PDA1_IRQCount = 0;
#ifdef FOR_DEBUG
			LOG_INF("g_reasonable_IRQCount = %d\n", g_reasonable_IRQCount);
#endif
#endif

			// Init ROI count which needed to process
			nROIcount = nUserROINumber;

			if (PDAProcessFunction(nUserROINumber, nROIcount, MTRUE, i) < 0) {
				LOG_INF("Fix ROI, PDA process fail\n");
				goto EXIT;
			}

			g_OutputBufferOffset += nUserROINumber*OUT_BYTE_PER_ROI;
#ifdef FOR_DEBUG
			LOG_INF("g_OutputBufferOffset: %d\n", g_OutputBufferOffset);
#endif
		}


//////////////////////////////////////////////////////////////////////////////
EXIT:
		// free output iova
#ifdef FOR_DEBUG
		LOG_INF("free output iova\n");
#endif
		pda_put_dma_buffer(&g_output_mmu);

INPUT_BUFFER_FREE_IOVA:
		//free input iova
#ifdef FOR_DEBUG
		LOG_INF("free input iova\n");
#endif
		pda_put_dma_buffer(&g_image_mmu);
		pda_put_dma_buffer(&g_table_mmu);

EXIT_WITHOUT_FREE_IOVA:
#ifdef FOR_DEBUG
		LOG_INF("Exit\n");
#endif

		// reset flow
		for (i = 0; i < g_PDA_quantity; i++) {
			pda_reset(i);
			pda_nontransaction_reset(i);
		}

#ifdef GET_PDA_TIME
		// for compute pda process time
		ktime_get_real_ts64(&total_time_end);

		LOG_INF("PDA 1 execute time (%d)\n",
			(pda1_done_e.tv_nsec - pda1_done_b.tv_nsec)/1000);
		LOG_INF("PDA 2 execute time (%d)\n",
			(pda2_done_e.tv_nsec - pda2_done_b.tv_nsec)/1000);
		LOG_INF("SW wait time (%d)\n",
			(time_end.tv_nsec - pda1_done_b.tv_nsec)/1000);
		LOG_INF("kernel total cost time (%d)\n",
		(total_time_end.tv_nsec-total_time_begin.tv_nsec)/1000);
#endif

		if (copy_to_user((void *)a_u4Param,
		    &g_pda_Pdadata,
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

#ifdef PDA_MMQOS
	pda_mmqos_bw_set();
#endif

#ifdef CHECK_IRQ_COUNT
	g_reasonable_IRQCount = 0;
	g_PDA0_IRQCount = 0;
	g_PDA1_IRQCount = 0;
#ifdef FOR_DEBUG
	LOG_INF("IRQCount, Reasonable = %d, PDA0 = %d, PDA1 = %d\n",
		g_reasonable_IRQCount, g_PDA0_IRQCount, g_PDA1_IRQCount);
#endif
#endif

	return 0;
}

static int PDA_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
#ifdef PDA_MMQOS
	pda_mmqos_bw_reset();
#endif

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
static int PDA_probe(struct platform_device *pdev)
{
	int nRet = 0;
	unsigned int irq_info[3];	/* Record interrupts info from device tree */
	struct device_node *node;

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

	//PDA node
	node = of_find_compatible_node(NULL, NULL, "mediatek,camera-pda");
	if (!node) {
		LOG_INF("find camera-pda node failed\n");
		return -1;
	}
	LOG_INF("find camera-pda node done\n");

	// must porting in dts
	pda_init_larb(pdev);

#if IS_ENABLED(CONFIG_OF)
	g_dev1 = &pdev->dev;

	if (dma_set_mask_and_coherent(g_dev1, DMA_BIT_MASK(34)))
		LOG_INF("No suitable DMA available\n");

	//power on smi
	/* consumer driver probe*/
	pm_runtime_enable(g_dev1); //Note: It‘s not larb's device.
	LOG_INF("pm_runtime_enable pda1 done\n");
#endif

	if (pda_devm_clk_get(pdev))
		return -1;

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

	CAMSYS_CONFIG_BASE = pda_get_camsys_address();
	if (!CAMSYS_CONFIG_BASE)
		LOG_INF("base CAMSYS_CONFIG_BASE failed\n");

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

#ifdef PDA_MMQOS
	pda_mmqos_init(g_dev1);
#endif

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

	// get PDA node
	node = of_find_compatible_node(NULL, NULL, "mediatek,camera-pda2");
	if (!node) {
		LOG_INF("find camera-pda node failed\n");
		return -1;
	}
	LOG_INF("find camera-pda node done\n");

	// must porting in dts
	pda_init_larb(pdev);

#if IS_ENABLED(CONFIG_OF)
	g_dev2 = &pdev->dev;

	if (dma_set_mask_and_coherent(g_dev2, DMA_BIT_MASK(34)))
		LOG_INF("No suitable DMA available\n");

	//power on smi
	// consumer driver probe
	pm_runtime_enable(g_dev2); //Note: It‘s not larb's device.
	LOG_INF("pm_runtime_enable pda2 done\n");
#endif

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
/******************************************************************************
 *
 ******************************************************************************/
module_init(camera_pda_init);
module_exit(camera_pda_exit);
MODULE_DESCRIPTION("Camera PDA driver");
MODULE_AUTHOR("MM6SW3");
MODULE_LICENSE("GPL");
