/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/linux/hif/sdio/include/hif.h#1
 */

/*
 * ! \file   "hif.h"
 * \brief  Functions for the driver to register bus and setup the IRQ
 *
 *  Functions for the driver to register bus and setup the IRQ
 */

#ifndef _HIF_H
#define _HIF_H

#include "gl_typedef.h"

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/
#define CONF_MTK_AHB_DMA         1	/* PIO mode is default mode if DMA is disabled */

#define CONF_HIF_DEV_MISC        0	/* register as misc device */
#define CONF_HIF_LOOPBACK_AUTO   0	/* hif loopback test triggered by open() */
				    /* only for development test */

#define CONF_HIF_PMIC_TEST       0	/* test purpose: power on CONNSYS */
#define CONF_HIF_CONNSYS_DBG     0	/* test purpose: when you want to use ICE on CONNSYS */

#define CONF_HIF_DMA_INT         0	/* DMA interrupt mode */

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
extern phys_addr_t gConEmiPhyBase;

extern INT_32 mtk_wcn_consys_hw_wifi_paldo_ctrl(UINT_32 enable);

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define HIF_MOD_NAME                "AHB_SLAVE_HIF"

/* Vary between projects, already replaced by device tree */
/* default value is for MT6797 */
#define HIF_DRV_BASE                 0x180F0000
#define HIF_DRV_LENGTH               0x1100

/* CONN HIF Debug CR */
#define CONN_HIF_DBGCR00	     0x0300
#define CONN_HIF_DBGCR01	     0x0304
#define CONN_HIF_DBGCR02	     0x0308
#define CONN_HIF_DBGCR04	     0x0310
#define CONN_HIF_DBGCR08	     0x0320
#define CONN_HIF_DBGCR10	     0x0328
#define CONN_HIF_DBGCR11	     0x032C
#define CONN_HIF_DBGCR12	     0x0330

/* CONN REMAP CFG CR */
#define CONN_REMAP_CONF_BASE         0x180E0000
#define CONN_REMAP_CONF_LENGTH       0x0070
#define CONN_REMAP_PSE_CLIENT_DBGCR  0x006c

/* MT6797 REMAP CR */
#define DYNAMIC_REMAP_CONF_BASE      0x10001340
#define DYNAMIC_REMAP_CONF_LENGTH    0x4
#define DYNAMIC_REMAP_BASE           0x180E0000

/* INFRA AO Debug CR */
#define INFRA_AO_DRV_BASE            0x10001000
#define INFRA_AO_DRV_LENGTH	     0x1000

#define INFRA_TOPAXI_PROTECTEN       0x0220
#define INFRA_TOPAXI_PROTECTEN_STA1  0x0228
#define INFRA_REMAP_TABLE_PSE_CLIENT 0x0340

#define MT_WF_HIF_IRQ_ID             283
#define MT_WF_HIF_DMA_IRQ_ID         97	/* AP_DMA_HIF0_IRQ */

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*
 * host interface's private data structure, which is attached to os glue
 * layer info structure.
 */
typedef struct _GL_HIF_DMA_OPS_T GL_HIF_DMA_OPS_T;

typedef struct _GL_HIF_INFO_T {

	/* General */
	struct device *Dev;		/* struct device */

	/* Control flag */
	BOOLEAN fgIntReadClear;
	BOOLEAN fgMbxReadClear;
	BOOLEAN fgDmaEnable;	/* TRUE: DMA mode is used (default) */

	/* HIF related */
	UINT_8 *HifRegBaseAddr;	/* HIF register base */
	ULONG HifRegPhyBase;	/* HIF register base physical addr */
	UINT_8 *ConnCfgRegBaseAddr; /* CONN remap config register base */
	UINT_8 *InfraRegBaseAddr; /* the AP infra remap configure CR base */
	UINT_32 *confRegBaseAddr; /* the connsys/ap remap configure CR base */
	UINT_32 HifIRQ;
#if (CONF_HIF_LOOPBACK_AUTO == 1)
	struct timer_list HifTmrLoopbkFn;	/* HIF loopback test trigger timer */
	wait_queue_head_t HifWaitq;
	UINT_32 HifLoopbkFlg;
	struct task_struct *HifTaskLoopbkFn;	/* HIF loopback test task */
#endif				/* CONF_HIF_LOOPBACK_AUTO */

	/* DMA related */
#if (CONF_HIF_DMA_INT == 1)
	wait_queue_head_t HifDmaWaitq;
	ULONG HifDmaFinishFlag;
	UINT_32 HifDmaIRQ;
#endif				/* CONF_HIF_DMA_INT */

	UINT_8 *DmaRegBaseAddr;	/* DMA register base */
	GL_HIF_DMA_OPS_T *DmaOps;	/* DMA Operators */

#if !defined(CONFIG_MTK_CLKMGR)
	struct clk *clk_wifi_dma;
#endif
} GL_HIF_INFO_T, *P_GL_HIF_INFO_T;

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
	UINT_32 Fix_en;
	UINT_32 Ratio;
	UINT_32 Connect;
	ULONG Src;
	ULONG Dst;
} MTK_WCN_HIF_DMA_CONF;

struct _GL_HIF_DMA_OPS_T {	/* DMA Operators */
	VOID (*DmaConfig)(GL_HIF_INFO_T *HifInfo, MTK_WCN_HIF_DMA_CONF *Conf);

	VOID (*DmaStart)(GL_HIF_INFO_T *HifInfo);

	VOID (*DmaStop)(GL_HIF_INFO_T *HifInfo);

	BOOL (*DmaPollStart)(GL_HIF_INFO_T *HifInfo);

	BOOL (*DmaPollIntr)(GL_HIF_INFO_T *HifInfo);

	VOID (*DmaAckIntr)(GL_HIF_INFO_T *HifInfo);

	VOID (*DmaClockCtrl)(GL_HIF_INFO_T *HifInfo, BOOL fgEnable);

	VOID (*DmaRegDump)(GL_HIF_INFO_T *HifInfo);

	VOID (*DmaReset)(GL_HIF_INFO_T *HifInfo);

};

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
/* PIO mode HIF register read/write */
#define HIF_REG_READL(_hif, _addr) \
	sdio_cr_readl((_hif)->HifRegBaseAddr, _addr)

#define HIF_REG_WRITEL(_hif, _addr, _val) \
	sdio_cr_writel(_val, (_hif)->HifRegBaseAddr, _addr)

/* PIO mode DMA register read/write */
#define HIF_DMAR_READL(_hif, _addr) \
	readl((volatile UINT_32 *)((_hif)->DmaRegBaseAddr + _addr))

#define HIF_DMAR_WRITEL(_hif, _addr, _val) \
	writel(_val, (volatile UINT_32 *)((_hif)->DmaRegBaseAddr + _addr))

#define my_sdio_disable(__lock) \
{\
	spin_lock_bh(&__lock); \
}

#define my_sdio_enable(__lock) \
{\
	spin_unlock_bh(&__lock); \
}

#define CONNSYS_REG_READ(base_addr, offset) \
	readl((PUINT_32)((PUINT_8)base_addr + offset))
#define CONNSYS_REG_WRITE(base_addr, offset, _val) \
	writel(_val, (PUINT_32)((PUINT_8)base_addr + offset))

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

WLAN_STATUS glRegisterBus(probe_card pfProbe, remove_card pfRemove);

VOID glUnregisterBus(remove_card pfRemove);

VOID glResetHif(GLUE_INFO_T *GlueInfo);

VOID glSetHifInfo(P_GLUE_INFO_T prGlueInfo, ULONG ulCookie);

VOID glClearHifInfo(P_GLUE_INFO_T prGlueInfo);

BOOLEAN glBusInit(PVOID pvData);

VOID glBusRelease(PVOID pData);

INT_32 glBusSetIrq(PVOID pvData, PVOID pfnIsr, PVOID pvCookie);

VOID glBusFreeIrq(PVOID pvData, PVOID pvCookie);

VOID glSetPowerState(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 ePowerMode);

#if defined(MT6797)

PUINT_8 glRemapConnsysAddr(P_GLUE_INFO_T prGlueInfo, UINT_32 consysAddr, UINT_32 remapLength);

VOID glUnmapConnsysAddr(P_GLUE_INFO_T prGlueInfo, PUINT_8 remapAddr, UINT_32 consysAddr);

#endif

VOID HifDmaInit(GL_HIF_INFO_T *HifInfo);

VOID HifDmaUnInit(GL_HIF_INFO_T *HifInfo);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
#endif /* _HIF_H */
