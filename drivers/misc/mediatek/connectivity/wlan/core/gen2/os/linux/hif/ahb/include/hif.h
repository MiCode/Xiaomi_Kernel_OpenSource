/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#ifndef _HIF_H
#define _HIF_H

#include "gl_typedef.h"
#include "mtk_porting.h"

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/
#define CONF_MTK_AHB_DMA         1	/* PIO mode is default mode if DMA is disabled */

#define CONF_HIF_DEV_MISC        0	/* register as misc device */
#define CONF_HIF_LOOPBACK_AUTO   0	/* hif loopback test triggered by open() */
				    /* only for development test */

#define CONF_HIF_PMIC_TEST       0	/* test purpose: power on CONNSYS */

#define CONF_HIF_DMA_INT         0	/* DMA interrupt mode */

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
extern phys_addr_t gConEmiPhyBase;
extern BOOLEAN fgIsResetting;
extern UINT_32 IsrCnt, IsrPassCnt;
extern int kalDevLoopbkThread(IN void *data);

#ifdef CONFIG_MTK_PMIC_MT6397
#else
#ifdef CONFIG_OF		/*for MT6752 */
extern INT_32 mtk_wcn_consys_hw_wifi_paldo_ctrl(UINT_32 enable);
#else				/*for MT6572/82/92 */
extern void upmu_set_vcn33_on_ctrl_wifi(UINT_32 val);
#endif
#endif

#if (CONF_HIF_DEV_MISC == 1)
#else
/* extern INT32 mtk_wcn_consys_hw_reg_ctrl(UINT32 on, UINT32 co_clock_en); */
#endif

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#ifndef CONN_MCU_CONFIG_BASE
#define CONN_MCU_CONFIG_BASE         0xF8070000	/* MT6572 */
#endif /* CONN_MCU_CONFIG_BASE */

#define CONSYS_CPUPCR_REG		    (CONN_MCU_CONFIG_BASE + 0x00000160)
#define CONSYS_REG_READ(addr)       (*((volatile unsigned int *)(addr)))

#define CONN_MCU_DRV_BASE                0x18070000
#define CONN_MCU_CHIPID                  0x0008

#define CONN_MCU_REG_LENGTH              0x0200
#define CONN_MCU_CPUPCR                  0x0160

#define AP_MCU_DRV_BASE                  0x18090000
#define AP_MCU_TX_RX_LENGTH              0x10100
#define AP_MCU_TX_DESC_ADDR              0x0000
#define AP_MCU_RX_DESC_ADDR              0x8080
#define AP_MCU_BANK_OFFSET               0x1010

#if (CFG_SRAM_SIZE_OPTION == 0)
#define AP_MCU_TC_INDEX_4_ADDR          0x1E50
#elif (CFG_SRAM_SIZE_OPTION == 1)
#define AP_MCU_TC_INDEX_4_ADDR          0x2848
#elif (CFG_SRAM_SIZE_OPTION == 2)
#define AP_MCU_TC_INDEX_4_ADDR          0x2F28
#endif
#define AP_MCU_TC_INDEX_4_OFFSET        0x0018

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/* host interface's private data structure, which is attached to os glue
** layer info structure.
 */
typedef struct _GL_HIF_DMA_OPS_T {	/* DMA Operators */
	void (*DmaConfig)(IN PVOID HifInfo, IN PVOID Conf);

	void (*DmaStart)(IN PVOID HifInfo);

	void (*DmaStop)(IN PVOID HifInfo);

	INT32 (*DmaPollStart)(IN PVOID HifInfo);

	INT32 (*DmaPollIntr)(IN PVOID HifInfo);

	void (*DmaAckIntr)(IN PVOID HifInfo);

	void (*DmaClockCtrl)(IN UINT_32 FlgIsEnabled);

	void (*DmaRegDump)(IN PVOID HifInfo);

	void (*DmaReset)(IN PVOID HifInfo);

} GL_HIF_DMA_OPS_T;

typedef struct _GL_HIF_INFO_T {

	/* General */
	VOID *Dev;		/* struct device */

#define MTK_CHIP_ID_6571    0x6571
#define MTK_CHIP_ID_6572    0x6572
#define MTK_CHIP_ID_6582    0x6582
#define MTK_CHIP_ID_8127    0x8127
#define MTK_CHIP_ID_6752    0x6752
#define MTK_CHIP_ID_8163    0x8163
#define MTK_CHIP_ID_8167    0x8167
#define MTK_CHIP_ID_6735    0x6735
#define MTK_CHIP_ID_6570    0x6570
#define MTK_CHIP_ID_6580    0x6580
#define MTK_CHIP_ID_6755    0x6755
#define MTK_CHIP_ID_7623    0x7623

	UINT_32 ChipID;

	/* Control flag */
	BOOLEAN fgIntReadClear;
	BOOLEAN fgMbxReadClear;
	BOOLEAN fgDmaEnable;	/* TRUE: DMA mode is used (default) */
	BOOLEAN fgDmaUsleepEnable;	/* TRUE: While DMA pooling, usleep is used (default) */

	/* HIF related */
	UINT_8 *HifRegBaseAddr;	/* HIF register base */
	UINT_8 *McuRegBaseAddr;	/* CONN MCU register base */
	UINT_8 *APMcuRegBaseAddr;	/* APMcu register base */

#if (CONF_HIF_LOOPBACK_AUTO == 1)
	struct timer_list HifTmrLoopbkFn;	/* HIF loopback test trigger timer */
	wait_queue_head_t HifWaitq;
	UINT_32 HifLoopbkFlg;
	struct task_struct *HifTaskLoopbkFn;	/* HIF loopback test task */
#endif				/* CONF_HIF_LOOPBACK_AUTO */

#if (CONF_HIF_DMA_INT == 1)
	wait_queue_head_t HifDmaWaitq;
	UINT_32 HifDmaWaitFlg;
#endif				/* CONF_HIF_DMA_INT */

	/* DMA related */
#define AP_DMA_HIF_LOCK(_lock)	/* spin_lock_bh(&(_lock)->DdmaLock) */
#define AP_DMA_HIF_UNLOCK(_lock)	/* spin_unlock_bh(&(_lock)->DdmaLock) */
	spinlock_t DdmaLock;	/* protect DMA access */

	UINT_8 *DmaRegBaseAddr;	/* DMA register base */
	GL_HIF_DMA_OPS_T *DmaOps;	/* DMA Operators */

#if !defined(CONFIG_MTK_CLKMGR)
	struct clk *clk_wifi_dma;
#endif
} GL_HIF_INFO_T, *P_GL_HIF_INFO_T;

#define HIF_MOD_NAME                "AHB_SLAVE_HIF"

#define HIF_DRV_BASE                0x180F0000
#define HIF_DRV_LENGTH				0x005c

typedef enum _MTK_WCN_HIF_BURST_LEN {
	HIF_BURST_1DW = 0,
	HIF_BURST_4DW,
	HIF_BURST_8DW
} MTK_WCN_HIF_BURST_LEN;

typedef enum _MTK_WCN_HIF_TXRX_TARGET {
	HIF_TARGET_TXD0 = 0,
	HIF_TARGET_TXD1,
	HIF_TARGET_RXD0,
	HIF_TARGET_RXD1,
	HIF_TARGET_WHISR
} MTK_WCN_HIF_TXRX_TARGET;

typedef enum _MTK_WCN_HIF_DMA_DIR {
	HIF_DMA_DIR_TX = 0,
	HIF_DMA_DIR_RX
} MTK_WCN_HIF_DMA_DIR;

typedef struct _MTK_WCN_HIF_DMA_CONF {
	UINT_32 Count;
	MTK_WCN_HIF_DMA_DIR Dir;
	UINT_32 Burst;
	UINT_32 Wsize;
	UINT_32 Ratio;
	UINT_32 Connect;
	UINT_32 Fix_en;
	ULONG Src;
	ULONG Dst;
} MTK_WCN_HIF_DMA_CONF;

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#define MCU_REG_READL(_hif, _addr)          \
	    readl((volatile UINT_32 *)((_hif)->McuRegBaseAddr + _addr))

/* PIO mode HIF register read/write */
#define HIF_REG_READL(_hif, _addr)          \
	    readl((volatile UINT_32 *)((_hif)->HifRegBaseAddr + _addr))

#define HIF_REG_WRITEL(_hif, _addr, _val)   \
	    writel(_val, ((volatile UINT_32 *)((_hif)->HifRegBaseAddr + _addr)))

#define HIF_REG_WRITEB(_hif, _addr, _val)   \
	    writeb(_val, ((volatile UINT_32 *)((_hif)->HifRegBaseAddr + _addr)))

/* PIO mode DMA register read/write */
#define HIF_DMAR_READL(_hif, _addr)          \
	    readl((volatile UINT_32 *)((_hif)->DmaRegBaseAddr + _addr))

#define HIF_DMAR_WRITEL(_hif, _addr, _val)   \
	    writel(_val, ((volatile UINT_32 *)((_hif)->DmaRegBaseAddr + _addr)))

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

#ifndef MODULE_AHB_DMA
VOID HifDumpEnhanceModeData(P_ADAPTER_T prAdapter);

VOID HifRegDump(P_ADAPTER_T prAdapter);

BOOLEAN HifIsFwOwn(P_ADAPTER_T prAdapter);

WLAN_STATUS glRegisterBus(probe_card pfProbe, remove_card pfRemove);

VOID glUnregisterBus(remove_card pfRemove);

VOID glResetHif(GLUE_INFO_T *GlueInfo);

VOID glSetHifInfo(P_GLUE_INFO_T prGlueInfo, ULONG ulCookie);

VOID glClearHifInfo(P_GLUE_INFO_T prGlueInfo);

VOID glGetChipInfo(GLUE_INFO_T *GlueInfo, UINT_8 *pucChipBuf);

#if CFG_SPM_WORKAROUND_FOR_HOTSPOT
BOOLEAN glIsChipNeedWakelock(GLUE_INFO_T *GlueInfo);
#endif

BOOLEAN glBusInit(PVOID pvData);

VOID glBusRelease(PVOID pData);

INT_32 glBusSetIrq(PVOID pvData, PVOID pfnIsr, PVOID pvCookie);

VOID glBusFreeIrq(PVOID pvData, PVOID pvCookie);

VOID glSetPowerState(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 ePowerMode);

VOID glDumpConnSysCpuInfo(P_GLUE_INFO_T prGlueInfo);

#endif /* MODULE_AHB_DMA */

/*----------------------------------------------------------------------------*/
/*!
* \brief Config GDMA TX/RX.
*
* \param[in] DmaRegBaseAddr     Pointer to the IO register base.
* \param[in] Conf               Pointer to the DMA operator.
*
* \retval NONE
*/
/*----------------------------------------------------------------------------*/
VOID HifGdmaInit(GL_HIF_INFO_T *HifInfo);

/*----------------------------------------------------------------------------*/
/*!
* \brief Config PDMA TX/RX.
*
* \param[in] DmaRegBaseAddr     Pointer to the IO register base.
* \param[in] Conf               Pointer to the DMA operator.
*
* \retval NONE
*/
/*----------------------------------------------------------------------------*/
VOID HifPdmaInit(GL_HIF_INFO_T *HifInfo);

int HifAhbSetMpuProtect(bool enable);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
#endif /* _HIF_H */
