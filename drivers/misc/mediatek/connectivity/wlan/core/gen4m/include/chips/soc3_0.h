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

/*! \file  soc3_0.h
*    \brief This file contains the info of soc3_0
*/

#ifdef SOC3_0

#ifndef _SOC3_0_H
#define _SOC3_0_H
#if (CFG_SUPPORT_CONNINFRA == 1)
#include "conninfra.h"
#endif
/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define CONNAC2X_TOP_HCR 0x88000000  /*no use, set HCR = HVR*/
#define CONNAC2X_TOP_HVR 0x88000000
#define CONNAC2X_TOP_FVR 0x88000004
#define CONNAC2x_CONN_CFG_ON_BASE	0x7C060000
#define CONNAC2x_CONN_CFG_ON_CONN_ON_MISC_ADDR \
	(CONNAC2x_CONN_CFG_ON_BASE + 0xF0)
#define CONNAC2x_CONN_CFG_ON_CONN_ON_MISC_DRV_FM_STAT_SYNC_SHFT         0

#define SOC3_0_CHIP_ID                 (0x7915)
#define SOC3_0_SW_SYNC0                CONNAC2x_CONN_CFG_ON_CONN_ON_MISC_ADDR
#define SOC3_0_SW_SYNC0_RDY_OFFSET \
	CONNAC2x_CONN_CFG_ON_CONN_ON_MISC_DRV_FM_STAT_SYNC_SHFT
#define SOC3_0_PATCH_START_ADDR        (0x00100000)
#define SOC3_0_TOP_CFG_BASE			CONN_CFG_BASE
#define SOC3_0_TX_DESC_APPEND_LENGTH   32
#define SOC3_0_RX_DESC_LENGTH   24
#define SOC3_0_ARB_AC_MODE_ADDR (0x820e3020)
#define MTK_CUSTOM_OID_INTERFACE_VERSION     0x00000200	/* for WPDWifi DLL */
#define MTK_EM_INTERFACE_VERSION		0x0001

#define CONN_HOST_CSR_TOP_BASE_ADDR 0x18060000
#define CONN_INFRA_CFG_BASE_ADDR 0x18001000
#define CONN_INFRA_RGU_BASE_ADDR 0x18000000
#define CONN_INFRA_BRCM_BASE_ADDR 0x1800E000
#define WFDMA_AXI0_R2A_CTRL_0	0x7c027500

#define WF_TOP_MISC_OFF_BASE_ADDR 0x184B0000
#define BID_CHK_BYP_EN_MASK 0x00000800

#define WF2CONN_SLPPROT_IDLE_ADDR 0x1800F004

#define CONN_INFRA_WAKEUP_WF_ADDR (CONN_HOST_CSR_TOP_BASE_ADDR + 0x01A4)
#define CONN_INFRA_ON2OFF_SLP_PROT_ACK_ADDR \
	(CONN_HOST_CSR_TOP_BASE_ADDR + 0x0184)
#define CONN_HW_VER_ADDR (CONN_INFRA_CFG_BASE_ADDR + 0x0000)
#define WFSYS_SW_RST_B_ADDR (CONN_INFRA_RGU_BASE_ADDR + 0x0018)
#define WFSYS_CPU_SW_RST_B_ADDR (CONN_INFRA_RGU_BASE_ADDR + 0x0010)
#define WFSYS_ON_TOP_PWR_CTL_ADDR (CONN_INFRA_RGU_BASE_ADDR + 0x0000)
#define TOP_DBG_DUMMY_3_CONNSYS_PWR_STATUS_ADDR \
	(CONN_HOST_CSR_TOP_BASE_ADDR + 0x02CC)
#define CONN_INFRA_WF_SLP_CTRL_R_ADDR (CONN_INFRA_CFG_BASE_ADDR + 0x0620)
#define CONN_INFRA_WFDMA_SLP_CTRL_R_ADDR (CONN_INFRA_CFG_BASE_ADDR + 0x0624)
#define CONN_INFRA_WFSYS_EMI_REQ_ADDR (CONN_INFRA_CFG_BASE_ADDR + 0x0c14)

#define WF_VDNR_EN_ADDR (CONN_INFRA_BRCM_BASE_ADDR + 0x6C)
#define WFSYS_VERSION_ID_ADDR (WF_TOP_MISC_OFF_BASE_ADDR + 0x10)
#define CONN_CFG_AP2WF_REMAP_1_ADDR (CONN_INFRA_CFG_BASE_ADDR + 0x0120)
#define CONN_MCU_CONFG_HS_BASE 0x89040000
#define WFSYS_VERSION_ID  0x20010000
#define WF_DYNAMIC_BASE 0x18500000
#define MCU_EMI_ENTRY_OFFSET 0x01DC
#define WF_EMI_ENTRY_OFFSET 0x01E0
#define CONN_AFE_CTL_RG_DIG_EN_ADDR (0x18003008)

#define CONNSYS_ROM_DONE_CHECK  0x00001D1E

#define WF_ROM_CODE_INDEX_ADDR 0x184C1604

#define WF_TRIGGER_AP2CONN_EINT 0x10001F00
#define WF_CONN_INFA_BUS_CLOCK_RATE 0x1000123C

#define WFSYS_CPUPCR_ADDR (CONNAC2x_CONN_CFG_ON_BASE + 0x0204)
#define WFSYS_LP_ADDR (CONNAC2x_CONN_CFG_ON_BASE + 0x0208)

#define WMMCU_ROM_PATCH_DATE_ADDR 0xF027F0D0
#define WMMCU_MCU_ROM_EMI_DATE_ADDR 0xF027F0E0
#define WMMCU_WIFI_ROM_EMI_DATE_ADDR 0xF027F0F0
#define DATE_CODE_SIZE 16

union soc3_0_WPDMA_INT_MASK {

	struct {
		uint32_t wfdma1_rx_done_0:1;
		uint32_t wfdma1_rx_done_1:1;
		uint32_t wfdma1_rx_done_2:1;
		uint32_t wfdma1_rx_done_3:1;
		uint32_t wfdma1_tx_done_0:1;
		uint32_t wfdma1_tx_done_1:1;
		uint32_t wfdma1_tx_done_2:1;
		uint32_t wfdma1_tx_done_3:1;
		uint32_t wfdma1_tx_done_4:1;
		uint32_t wfdma1_tx_done_5:1;
		uint32_t wfdma1_tx_done_6:1;
		uint32_t wfdma1_tx_done_7:1;
		uint32_t wfdma1_tx_done_8:1;
		uint32_t wfdma1_tx_done_9:1;
		uint32_t wfdma1_tx_done_10:1;
		uint32_t wfdma1_tx_done_11:1;
		uint32_t wfdma1_tx_done_12:1;
		uint32_t wfdma1_tx_done_13:1;
		uint32_t wfdma1_tx_done_14:1;
		uint32_t reserved19:1;
		uint32_t wfdma1_rx_coherent:1;
		uint32_t wfdma1_tx_coherent:1;
		uint32_t reserved:2;
		uint32_t wpdma2host_err_int_en:1;
		uint32_t reserved25:1;
		uint32_t wfdma1_tx_done_16:1;
		uint32_t wfdma1_tx_done_17:1;
		uint32_t wfdma1_subsys_int_en:1;
		uint32_t wfdma1_mcu2host_sw_int_en:1;
		uint32_t wfdma1_tx_done_18:1;
		uint32_t reserved31:1;
	} field_wfdma1_ena;

	struct {
		uint32_t wfdma0_rx_done_0:1;
		uint32_t wfdma0_rx_done_1:1;
		uint32_t wfdma0_rx_done_2:1;
		uint32_t wfdma0_rx_done_3:1;
		uint32_t wfdma0_tx_done_0:1;
		uint32_t wfdma0_tx_done_1:1;
		uint32_t wfdma0_tx_done_2:1;
		uint32_t wfdma0_tx_done_3:1;
		uint32_t reserved8:11;
		uint32_t wfdma0_rx_done_6:1;
		uint32_t wfdma0_rx_coherent:1;
		uint32_t wfdma0_tx_coherent:1;
		uint32_t wfdma0_rx_done_4:1;
		uint32_t wfdma0_rx_done_5:1;
		uint32_t wpdma2host_err_int_en:1;
		uint32_t wfdma0_rx_done_7:1;
		uint32_t reserved26:2;
		uint32_t wfdma0_subsys_int_en:1;
		uint32_t wfdma0_mcu2host_sw_int_en:1;
		uint32_t reserved30:2;
	} field_wfdma0_ena;
	uint32_t word;
};
/*******************************************************************************
*                         D A T A   T Y P E S
********************************************************************************
*/
enum ENUM_WLAN_POWER_ON_DOWNLOAD {
	ENUM_WLAN_POWER_ON_DOWNLOAD_EMI = 0,
	ENUM_WLAN_POWER_ON_DOWNLOAD_ROM_PATCH = 1,
	ENUM_WLAN_POWER_ON_DOWNLOAD_WIFI_RAM_CODE = 2
};

struct ROM_EMI_HEADER {
	uint8_t ucDateTime[16];
	uint8_t ucPLat[4];
	uint16_t u2HwVer;
	uint16_t u2SwVer;
	uint32_t u4PatchAddr;
	uint32_t u4PatchType;
	uint32_t u4CRC[4];
};

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
extern struct platform_device *g_prPlatDev;
#if (CFG_SUPPORT_CONNINFRA == 1)
extern u_int8_t g_IsWfsysBusHang;
extern struct completion g_triggerComp;
extern bool g_IsTriggerTimeout;
extern u_int8_t fgIsResetting;
extern u_int8_t g_fgRstRecover;
extern struct regmap *g_regmap;
#endif

#if (CFG_ANDORID_CONNINFRA_COREDUMP_SUPPORT == 1)
extern u_int8_t g_IsNeedWaitCoredump;
#endif
/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
void soc3_0_show_ple_info(
	struct ADAPTER *prAdapter,
	u_int8_t fgDumpTxd);
void soc3_0_show_pse_info(
	struct ADAPTER *prAdapter);

void soc3_0_show_wfdma_info(
	IN struct ADAPTER *prAdapter);

void soc3_0_show_wfdma_info_by_type(
	IN struct ADAPTER *prAdapter,
	bool bShowWFDMA_type);

void soc3_0_show_wfdma_info_by_type_without_adapter(
	bool bIsHostDMA);

void soc3_0_DumpWFDMACr(struct ADAPTER *prAdapter);

void soc3_0_show_dmashdl_info(
	IN struct ADAPTER *prAdapter);
void soc3_0_dump_mac_info(
	IN struct ADAPTER *prAdapter);
void soc3_0EnableInterrupt(
	struct ADAPTER *prAdapter);

void soc3_0EnableInterrupt(
	struct ADAPTER *prAdapter);
extern void kalConstructDefaultFirmwarePrio(
				struct GLUE_INFO	*prGlueInfo,
				uint8_t **apucNameTable,
				uint8_t **apucName,
				uint8_t *pucNameIdx,
				uint8_t ucMaxNameIdx);

extern uint32_t kalFirmwareOpen(
				IN struct GLUE_INFO *prGlueInfo,
				IN uint8_t **apucNameTable);


extern uint32_t kalFirmwareSize(
				IN struct GLUE_INFO *prGlueInfo,
				OUT uint32_t *pu4Size);

extern uint32_t kalFirmwareLoad(
			IN struct GLUE_INFO *prGlueInfo,
			OUT void *prBuf, IN uint32_t u4Offset,
			OUT uint32_t *pu4Size);

extern uint32_t kalFirmwareClose(
			IN struct GLUE_INFO *prGlueInfo);

extern void wlanWakeLockInit(
	struct GLUE_INFO *prGlueInfo);

extern void wlanWakeLockUninit(
	struct GLUE_INFO *prGlueInfo);

extern struct wireless_dev *wlanNetCreate(
		void *pvData,
		void *pvDriverData);

extern void wlanNetDestroy(
	struct wireless_dev *prWdev);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#if (CFG_DOWNLOAD_DYN_MEMORY_MAP == 1)
uint32_t soc3_0_DownloadByDynMemMap(IN struct ADAPTER *prAdapter,
	IN uint32_t u4Addr, IN uint32_t u4Len,
	IN uint8_t *pucStartPtr, IN enum ENUM_IMG_DL_IDX_T eDlIdx);
#endif
void soc3_0_DumpWfsyscpupcr(struct ADAPTER *prAdapter);
void soc3_0_WfdmaAxiCtrl(struct ADAPTER *prAdapter);

int hifWmmcuPwrOn(void);
int hifWmmcuPwrOff(void);
int wf_ioremap_read(size_t addr, unsigned int *val);

int wf_ioremap_write(phys_addr_t addr, unsigned int val);
int soc3_0_Trigger_fw_assert(void);
int soc3_0_CheckBusHang(void *adapter,
	uint8_t ucWfResetEnable);
void soc3_0_DumpBusHangCr(struct ADAPTER *prAdapter);

void wlanCoAntWiFi(void);
void wlanCoAntMD(void);
void wlanCoAntVFE28En(IN struct ADAPTER *prAdapter);
void wlanCoAntVFE28Dis(void);

#if (CFG_SUPPORT_CONNINFRA == 1)
int wlanConnacPccifon(void);
int wlanConnacPccifoff(void);
int soc3_0_Trigger_whole_chip_rst(char *reason);
void soc3_0_Sw_interrupt_handler(struct ADAPTER *prAdapter);
void soc3_0_Conninfra_cb_register(void);
extern void update_driver_reset_status(uint8_t fgIsResetting);
extern int32_t get_wifi_process_status(void);
extern int32_t get_wifi_powered_status(void);
extern void update_pre_cal_status(uint8_t fgIsPreCal);
extern int8_t get_pre_cal_status(void);
#endif
void soc3_0_DumpWfsysdebugflag(void);
#if (CFG_POWER_ON_DOWNLOAD_EMI_ROM_PATCH == 1)
void soc3_0_ConstructFirmwarePrio(struct GLUE_INFO *prGlueInfo,
	uint8_t **apucNameTable, uint8_t **apucName,
	uint8_t *pucNameIdx, uint8_t ucMaxNameIdx);
void *
soc3_0_kalFirmwareImageMapping(IN struct GLUE_INFO *prGlueInfo,
			OUT void **ppvMapFileBuf, OUT uint32_t *pu4FileLength,
			IN enum ENUM_IMG_DL_IDX_T eDlIdx);
uint32_t soc3_0_wlanImageSectionDownloadStage(
	IN struct ADAPTER *prAdapter, IN void *pvFwImageMapFile,
	IN uint32_t u4FwImageFileLength, IN uint8_t ucSectionNumber,
	IN enum ENUM_IMG_DL_IDX_T eDlIdx);
uint32_t soc3_0_wlanPowerOnDownload(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucDownloadItem);
int32_t soc3_0_wlanPowerOnInit(
	enum ENUM_WLAN_POWER_ON_DOWNLOAD eDownloadItem);
#endif

#if (CFG_SUPPORT_PRE_ON_PHY_ACTION == 1)
uint32_t soc3_0_wlanPhyAction(IN struct ADAPTER *prAdapter);
int soc3_0_wlanPreCalPwrOn(void);
int soc3_0_wlanPreCal(void);
uint8_t *soc3_0_wlanGetCalResult(uint32_t *prCalSize);
void soc3_0_wlanCalDebugCmd(uint32_t cmd, uint32_t para);
#endif /* (CFG_SUPPORT_PRE_ON_PHY_ACTION == 1) */

void soc3_0_icapRiseVcoreClockRate(void);
void soc3_0_icapDownVcoreClockRate(void);

#endif /* _SOC3_0_H */

#endif  /* soc3_0 */
