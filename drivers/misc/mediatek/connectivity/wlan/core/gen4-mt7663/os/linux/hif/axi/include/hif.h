/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*! \file   "hif.h"
 *  \brief  Functions for the driver to register bus and setup the IRQ
 *
 *  Functions for the driver to register bus and setup the IRQ
 */

#ifndef _HIF_H
#define _HIF_H

#include "hif_pdma.h"
#include "connac.h"

#if defined(_HIF_AXI)
#define HIF_NAME "AXI"
#else
#error "No HIF defined!"
#endif

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#if CFG_MTK_ANDROID_WMT
struct MTK_WCN_WMT_WLAN_CB_INFO;
extern int mtk_wcn_wmt_wlan_reg(
	struct MTK_WCN_WMT_WLAN_CB_INFO *pWmtWlanCbInfo);
extern int mtk_wcn_wmt_wlan_unreg(void);
#endif

extern phys_addr_t gWifiRsvMemPhyBase;
extern unsigned long long gWifiRsvMemSize;

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define AXI_CFG_PREALLOC_MEMORY_BUFFER    1

#define AXI_TX_MAX_SIZE_PER_FRAME         (NIC_TX_MAX_SIZE_PER_FRAME +      \
					   NIC_TX_DESC_AND_PADDING_LENGTH + \
					   CONNAC_TX_DESC_APPEND_LENGTH)

#define AXI_TX_CMD_BUFF_SIZE              4096
#define AXI_WLAN_IRQ_NUMBER               16

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

struct GL_HIF_INFO;

struct HIF_MEM_OPS {
	void (*allocTxDesc)(struct GL_HIF_INFO *prHifInfo,
			    struct RTMP_DMABUF *prDescRing,
			    uint32_t u4Num);
	void (*allocRxDesc)(struct GL_HIF_INFO *prHifInfo,
			    struct RTMP_DMABUF *prDescRing,
			    uint32_t u4Num);
	void (*allocTxCmdBuf)(struct RTMP_DMABUF *prDmaBuf,
			      uint32_t u4Num, uint32_t u4Idx);
	void (*allocTxDataBuf)(struct MSDU_TOKEN_ENTRY *prToken,
			       uint32_t u4Idx);
	void *(*allocRxBuf)(struct GL_HIF_INFO *prHifInfo,
			    struct RTMP_DMABUF *prDmaBuf,
			    uint32_t u4Num, uint32_t u4Idx);
	void *(*allocRuntimeMem)(uint32_t u4SrcLen);
	bool (*copyCmd)(struct GL_HIF_INFO *prHifInfo,
			struct RTMP_DMACB *prTxCell, void *pucBuf,
			void *pucSrc1, uint32_t u4SrcLen1,
			void *pucSrc2, uint32_t u4SrcLen2);
	bool (*copyEvent)(struct GL_HIF_INFO *prHifInfo,
			  struct RTMP_DMACB *pRxCell,
			  struct RXD_STRUCT *pRxD,
			  struct RTMP_DMABUF *prDmaBuf,
			  uint8_t *pucDst, uint32_t u4Len);
	bool (*copyTxData)(struct MSDU_TOKEN_ENTRY *prToken,
			   void *pucSrc, uint32_t u4Len);
	bool (*copyRxData)(struct GL_HIF_INFO *prHifInfo,
			   struct RTMP_DMACB *pRxCell,
			   struct RTMP_DMABUF *prDmaBuf,
			   struct SW_RFB *prSwRfb);
	void (*flushCache)(struct GL_HIF_INFO *prHifInfo,
			   void *pucSrc, uint32_t u4Len);
	phys_addr_t (*mapTxBuf)(struct GL_HIF_INFO *prHifInfo,
			  void *pucBuf, uint32_t u4Offset, uint32_t u4Len);
	phys_addr_t (*mapRxBuf)(struct GL_HIF_INFO *prHifInfo,
			  void *pucBuf, uint32_t u4Offset, uint32_t u4Len);
	void (*unmapTxBuf)(struct GL_HIF_INFO *prHifInfo,
			   phys_addr_t rDmaAddr, uint32_t u4Len);
	void (*unmapRxBuf)(struct GL_HIF_INFO *prHifInfo,
			   phys_addr_t rDmaAddr, uint32_t u4Len);
	void (*freeDesc)(struct GL_HIF_INFO *prHifInfo,
			 struct RTMP_DMABUF *prDescRing);
	void (*freeBuf)(void *pucSrc, uint32_t u4Len);
	void (*freePacket)(void *pvPacket);
	void (*dumpTx)(struct GL_HIF_INFO *prHifInfo,
		       struct RTMP_TX_RING *prTxRing,
		       uint32_t u4Idx, uint32_t u4DumpLen);
	void (*dumpRx)(struct GL_HIF_INFO *prHifInfo,
		       struct RTMP_RX_RING *prRxRing,
		       uint32_t u4Idx, uint32_t u4DumpLen);
};

/* host interface's private data structure, which is attached to os glue
 ** layer info structure.
 */
struct GL_HIF_INFO {
	struct platform_device *pdev;
	struct device *prDmaDev;
	struct HIF_MEM_OPS rMemOps;

	uint32_t u4IrqId;
	int32_t u4HifCnt;

	/* AXI MMIO Base Address, all access will use */
	void *CSRBaseAddress;

	/* Shared memory of all 1st pre-allocated
	 * TxBuf associated with each TXD
	 */
	/* Shared memory for Tx descriptors */
	struct RTMP_DMABUF TxDescRing[NUM_OF_TX_RING];
	struct RTMP_TX_RING TxRing[NUM_OF_TX_RING];	/* AC0~3 + HCCA */

	/* Shared memory for RX descriptors */
	struct RTMP_DMABUF RxDescRing[NUM_OF_RX_RING];
	struct RTMP_RX_RING RxRing[NUM_OF_RX_RING];

	u_int8_t fgIntReadClear;
	u_int8_t fgMbxReadClear;

	uint32_t u4IntStatus;

	struct MSDU_TOKEN_INFO rTokenInfo;

	struct ERR_RECOVERY_CTRL_T rErrRecoveryCtl;
	u_int8_t fgIsErrRecovery;
	struct timer_list rSerTimer;
	struct list_head rTxCmdQ;
	struct list_head rTxDataQ;
	uint32_t u4TxDataQLen;

	bool fgIsPowerOff;
	bool fgIsDumpLog;
};

struct BUS_INFO {
	const unsigned int top_cfg_base;	/* TOP_CFG_BASE address */
	const struct PCIE_CHIP_CR_MAPPING *bus2chip;
	const unsigned int tx_ring_cmd_idx;
	const unsigned int tx_ring_fwdl_idx;
	const unsigned int tx_ring_data_idx;
	const bool fgCheckDriverOwnInt;
	const bool fgInitPCIeInt;
	const uint32_t u4DmaMask;

	void (*pdmaSetup)(struct GLUE_INFO *prGlueInfo, u_int8_t enable);
	void (*enableInterrupt)(struct ADAPTER *prAdapter);
	void (*disableInterrupt)(struct ADAPTER *prAdapter);
	void (*lowPowerOwnRead)(struct ADAPTER *prAdapter, u_int8_t *pfgResult);
	void (*lowPowerOwnSet)(struct ADAPTER *prAdapter, u_int8_t *pfgResult);
	void (*lowPowerOwnClear)(struct ADAPTER *prAdapter,
		u_int8_t *pfgResult);
	void (*wakeUpWiFi)(struct ADAPTER *prAdapter);
	bool (*isValidRegAccess)(struct ADAPTER *prAdapter,
				 uint32_t u4Register);
	void (*getMailboxStatus)(struct ADAPTER *prAdapter, uint32_t *pu4Val);
	void (*setDummyReg)(struct GLUE_INFO *prGlueInfo);
	void (*checkDummyReg)(struct GLUE_INFO *prGlueInfo);
};

struct HIF_MEM {
	phys_addr_t pa;
	void *va;
};

struct HIF_PREALLOC_MEM {
	struct HIF_MEM rTxDesc[NUM_OF_TX_RING];
	struct HIF_MEM rRxDesc[NUM_OF_RX_RING];
	struct HIF_MEM rTxCmdBuf[TX_RING_SIZE];
	struct HIF_MEM rRxDataBuf[RX_RING0_SIZE];
	struct HIF_MEM rRxEventBuf[RX_RING1_SIZE];
#if HIF_TX_PREALLOC_DATA_BUFFER
	struct HIF_MEM rMsduBuf[HIF_TX_MSDU_TOKEN_NUM];
#endif
	phys_addr_t pucRsvMemBase;
	uint64_t u4RsvMemSize;
	uint32_t u4Offset;
};

#if CFG_MTK_ANDROID_WMT
struct MTK_WCN_WMT_WLAN_CB_INFO {
	int (*wlan_probe_cb)(void);
	int (*wlan_remove_cb)(void);
	int (*wlan_bus_cnt_get_cb)(void);
	int (*wlan_bus_cnt_clr_cb)(void);
	int (*wlan_emi_mpu_set_protection_cb)(bool);
};
#endif

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

#define axi_resource_start(d, v)  (0x18000000)
#define axi_resource_len(d, v)    (0x100000)
#define axi_name(d)               ("AXI-BUS")

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

uint32_t glRegisterBus(probe_card pfProbe, remove_card pfRemove);

void glUnregisterBus(remove_card pfRemove);

void glSetHifInfo(struct GLUE_INFO *prGlueInfo, unsigned long ulCookie);

void glClearHifInfo(struct GLUE_INFO *prGlueInfo);

u_int8_t glBusInit(void *pvData);

void glBusRelease(void *pData);

int32_t glBusSetIrq(void *pvData, void *pfnIsr, void *pvCookie);

void glBusFreeIrq(void *pvData, void *pvCookie);

void glSetPowerState(IN struct GLUE_INFO *prGlueInfo, IN uint32_t ePowerMode);

void glGetDev(void *ctx, struct device **dev);

void glGetHifDev(struct GL_HIF_INFO *prHif, struct device **dev);

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
#endif /* _HIF_H */
