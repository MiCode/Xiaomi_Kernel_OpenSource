/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _PLATFORM_MT6893_CONSYS_REG_OFFSET_H_
#define _PLATFORM_MT6893_CONSYS_REG_OFFSET_H_


/**********************************************************************/
/* Base: infracfg_ao (0x1000_1000) */
/**********************************************************************/
#define INFRA_TOPAXI_PROTECTEN_STA1_OFFSET	0x0228
#define INFRA_TOPAXI_PROTECTEN_SET_OFFSET	0x02a0
#define INFRA_TOPAXI_PROTECTEN_CLR_OFFSET	0x02a4
#define INFRA_TOPAXI_PROTECTEN2_CLR_OFFSET	0x0718
#define INFRA_TOPAXI_PROTECTEN2_SET_OFFSET	0x0714
#define INFRA_TOPAXI_PROTECTEN2_STA1_OFFSET	0x0724

/**********************************************************************/
/* Base: GPIO (0x1000_5000) */
/**********************************************************************/
#define GPIO_DIR5_SET	0x0054

#define GPIO_DOUT5_SET	0x0154

#define GPIO_MODE19	0x0430
#define GPIO_MODE21	0x0450
#define GPIO_MODE22	0x0460

/**********************************************************************/
/* Base: SPM (0x1000_6000) */
/**********************************************************************/
#define SPM_POWERON_CONFIG_EN		0x0000
#define SPM_PCM_REG13_DATA			0x0110
#define SPM_PCM_REG7_DATA			0x0100
#define SPM_SRC_REQ_STA_0			0x0114
#define SPM_BUS_PROTECT_RDY			0x0150
#define SPM_BUS_PROTECT2_RDY		0x0158
#define SPM_PWR_STATUS				0x016c
#define SPM_PWR_STATUS_2ND			0x0170
#define SPM_CONN_PWR_CON			0x0304
#define SPM_PLL_CON					0x044C
#define SPM_RC_CENTRAL_CFG1			0x0504
#define SPM_PCM_WDT_LATCH_SPARE_0	0x084C

#define SPM_RC_RC_M04_REQ_STA_0		0xE28
#define SPM_RC_RC_M05_REQ_STA_0		0xE2C
#define SPM_RC_RC_M06_REQ_STA_0		0xE30
#define SPM_RC_RC_M07_REQ_STA_0		0xE34

/**********************************************************************/
/* Base: TOP RGU (0x1000_7000) */
/**********************************************************************/
#define TOP_RGU_WDT_SWSYSRST	0x0018


/**********************************************************************/
/* Base: INFRACFG (0x1020_e000) */
/**********************************************************************/
#define INFRA_AP2MD_GALS_CTL		0x0504
#define INFRA_CONN2AP_GLAS_RC_ST	0x0804

/**********************************************************************/
/* Base: IOCFG_RT (0x11EA_0000) */
/**********************************************************************/
#define IOCFG_RT_DRV_CFG0	0x0000
#define IOCFG_RT_PD_CFG0_SET	0x0054
#define IOCFG_RT_PD_CFG0_CLR	0x0058
#define IOCFG_RT_PU_CFG0_SET	0x0074
#define IOCFG_RT_PU_CFG0_CLR	0x0078

/**********************************************************************/
/* Base: conn_infra_rgu (0x1800_0000) */
/**********************************************************************/
#define CONN_INFRA_RGU_BGFSYS_ON_TOP_PWR_CTL	0x0008
#define CONN_INFRA_RGU_SYSRAM_HWCTL_PDN		0x0038
#define CONN_INFRA_RGU_SYSRAM_HWCTL_SLP		0x003c
#define CONN_INFRA_RGU_CO_EXT_MEM_HWCTL_PDN	0x0050
#define CONN_INFRA_RGU_CO_EXT_MEM_HWCTL_SLP	0x0054
#define CONN_INFRA_RGU_DEBUG_SEL			0x0090

/**********************************************************************/
/* Base: conn_infra_cfg (0x1800_1000) */
/**********************************************************************/
#define CONN_HW_VER_OFFSET		0x0000
#define CONN_CFG_ID_OFFSET		0x0004
#define CONN_INFRA_CFG_LIGHT_SECURITY_CTRL	0x00f0

#define CONN_INFRA_CFG_GALS_AP2CONN_GALS_DBG	0x0160
#define CONN_INFRA_CFG_GALS_CONN2AP_TX_SLP_CTRL	0x0630
#define CONN_INFRA_CFG_GALS_CONN2AP_RX_SLP_CTRL	0x0634
#define CONN_INFRA_CFG_GALS_GPS2CONN_SLP_CTRL	0x061C
#define CONN_INFRA_CFG_GALS_CONN2GPS_SLP_CTRL	0x0618
#define CONN_INFRA_CFG_GALS_BT2CONN_SLP_CTRL	0x0614
#define CONN_INFRA_CFG_GALS_CONN2BT_SLP_CTRL	0x0610

#define CONN_INFRA_CFG_WF_SLP_CTRL				0x0620
#define CONN_INFRA_CFG_ON_BUS_SLP_CTRL			0x0628
#define CONN_INFRA_CFG_OFF_BUS_SLP_CTRL			0x062C


#define CONN_INFRA_CFG_OSC_CTL_0	0x0800
#define CONN_INFRA_CFG_OSC_CTL_1	0x0804
#define CONN_INFRA_CFG_OSC_STATUS	0x080C
#define CONN_INFRA_CFG_PLL_STATUS	0x0810

#define AP2CONN_EFUSE_DATA			0x0818
#define CONN_INFRA_CFG_RC_CTL_0		0x0834
#define CONN_INFRA_CFG_RC_CTL_0_GPS	0x0838
#define CONN_INFRA_CFG_RC_CTL_1_GPS	0x083C
#define CONN_INFRA_CFG_RC_CTL_0_BT	0x0840
#define CONN_INFRA_CFG_RC_CTL_1_BT	0x0844
#define CONN_INFRA_CFG_RC_CTL_0_WF	0x0848
#define CONN_INFRA_CFG_RC_CTL_1_WF	0x084c
#define CONN_INFRA_CFG_RC_CTL_1_TOP	0x0854
#define CONN_INFRA_CFG_RC_CTL_0_TOP	0x0850
#define CONN_INFRA_CFG_PWRCTRL0		0x0860
#define CONN_INFRA_CFG_ADIE_CTL		0x0900
#define CONN_INFRA_CFG_CKGEN_BUS	0x0a00
#define CONN_INFRA_CFG_DBG_MUX_SEL	0x0b00
#define CONN_INFRA_CFG_EMI_CTL_0	0x0c00

#define CONN_HW_VER	0x20010101
#define CONN_CFG_ID	0x3

/**********************************************************************/
/* Base: conn_top_therm_ctl (0x1800_2000) */
/**********************************************************************/
#define CONN_TOP_THERM_CTL_THERMCR1		0x0004
#define CONN_TOP_THERM_CTL_THERM_AADDR	0x0018
#define CONN_TOP_THERM_CTL_THERM_CAL_EN	0x0024

/**********************************************************************/
/* Base: conn_afe_ctl(0x1800_3000) */
/**********************************************************************/
#define CONN_AFE_CTL_RG_DIG_EN_01	0x0000
#define CONN_AFE_CTL_RG_DIG_EN_02	0x0004
#define CONN_AFE_CTL_RG_DIG_EN_03	0x0008
#define CONN_AFE_CTL_RG_WBG_AFE_01	0x0010
#define CONN_AFE_CTL_RG_WBG_RCK_01	0x0018
#define CONN_AFE_CTL_RG_WBG_GL1_01	0x0040
#define CONN_AFE_CTL_RG_WBG_BT_TX_03	0x0058
#define CONN_AFE_CTL_RG_WBG_WF0_TX_03	0x0078
#define CONN_AFE_CTL_RG_WBG_WF1_TX_03	0x0094
#define CONN_AFE_CTL_RG_PLL_STB_TIME	0x00f4
#define CONN_AFE_CTL_RG_WBG_GL5_01	0x0100


/**********************************************************************/
/* Base: conn_rf_spi_mst_reg(0x1800_4000) */
/**********************************************************************/
#define CONN_RF_SPI_MST_REG_SPI_STA		0x0000
#define CONN_RF_SPI_MST_REG_SPI_CRTL		0x0004
#define CONN_RF_SPI_MST_REG_FM_CTRL		0x000c
#define CONN_RF_SPI_MST_REG_SPI_WF_ADDR		0x0010
#define CONN_RF_SPI_MST_REG_SPI_WF_WDAT		0x0014
#define CONN_RF_SPI_MST_REG_SPI_WF_RDAT		0x0018
#define CONN_RF_SPI_MST_REG_SPI_BT_ADDR		0x0020
#define CONN_RF_SPI_MST_REG_SPI_BT_WDAT		0x0024
#define CONN_RF_SPI_MST_REG_SPI_BT_RDAT		0x0028
#define CONN_RF_SPI_MST_REG_SPI_FM_ADDR		0x0030
#define CONN_RF_SPI_MST_REG_SPI_FM_WDAT		0x0034
#define CONN_RF_SPI_MST_REG_SPI_FM_RDAT		0x0038
#define CONN_RF_SPI_MST_REG_SPI_TOP_ADDR	0x0050
#define CONN_RF_SPI_MST_REG_SPI_TOP_WDAT	0x0054
#define CONN_RF_SPI_MST_REG_SPI_TOP_RDAT	0x0058
#define CONN_RF_SPI_MST_REG_SPI_GPS_GPS_ADDR	0x0210
#define CONN_RF_SPI_MST_REG_SPI_GPS_GPS_WDAT	0x0214
#define CONN_RF_SPI_MST_REG_SPI_GPS_GPS_RDAT	0x0218


/**********************************************************************/
/* Base: conn_wt_slp_ctl_reg(0x1800_5000) */
/**********************************************************************/
#define CONN_WTSLP_CTL_REG_WB_STA		0x0008
#define CONN_WT_SLP_CTL_REG_WB_SLP_CTL		0x0004
#define CONN_WT_SLP_CTL_REG_WB_BG_ADDR1		0x0010
#define CONN_WT_SLP_CTL_REG_WB_BG_ADDR2		0x0014
#define CONN_WT_SLP_CTL_REG_WB_BG_ADDR3		0x0018
#define CONN_WT_SLP_CTL_REG_WB_BG_ADDR4		0x001c
#define CONN_WT_SLP_CTL_REG_WB_BG_ADDR5		0x0020
#define CONN_WT_SLP_CTL_REG_WB_BG_ADDR6		0x0024
#define CONN_WT_SLP_CTL_REG_WB_BG_ADDR7		0x0028
#define CONN_WT_SLP_CTL_REG_WB_BG_ADDR8		0x002c
#define CONN_WT_SLP_CTL_REG_WB_BG_ON1		0x0030
#define CONN_WT_SLP_CTL_REG_WB_BG_ON2		0x0034
#define CONN_WT_SLP_CTL_REG_WB_BG_ON3		0x0038
#define CONN_WT_SLP_CTL_REG_WB_BG_ON4		0x003c
#define CONN_WT_SLP_CTL_REG_WB_BG_ON5		0x0040
#define CONN_WT_SLP_CTL_REG_WB_BG_ON6		0x0044
#define CONN_WT_SLP_CTL_REG_WB_BG_ON7		0x0048
#define CONN_WT_SLP_CTL_REG_WB_BG_ON8		0x004c
#define CONN_WT_SLP_CTL_REG_WB_BG_OFF1		0x0050
#define CONN_WT_SLP_CTL_REG_WB_BG_OFF2		0x0054
#define CONN_WT_SLP_CTL_REG_WB_BG_OFF3		0x0058
#define CONN_WT_SLP_CTL_REG_WB_BG_OFF4		0x005c
#define CONN_WT_SLP_CTL_REG_WB_BG_OFF5		0x0060
#define CONN_WT_SLP_CTL_REG_WB_BG_OFF6		0x0064
#define CONN_WT_SLP_CTL_REG_WB_BG_OFF7		0x0068
#define CONN_WT_SLP_CTL_REG_WB_BG_OFF8		0x006c
#define CONN_WT_SLP_CTL_REG_WB_WF_CK_ADDR	0x0070
#define CONN_WT_SLP_CTL_REG_WB_WF_WAKE_ADDR	0x0074
#define CONN_WT_SLP_CTL_REG_WB_WF_ZPS_ADDR	0x0078
#define CONN_WT_SLP_CTL_REG_WB_BT_CK_ADDR	0x007c
#define CONN_WT_SLP_CTL_REG_WB_BT_WAKE_ADDR	0x0080
#define CONN_WT_SLP_CTL_REG_WB_TOP_CK_ADDR	0x0084
#define CONN_WT_SLP_CTL_REG_WB_GPS_CK_ADDR	0x0088
#define CONN_WT_SLP_CTL_REG_WB_WF_B0_CMD_ADDR	0x008c
#define CONN_WT_SLP_CTL_REG_WB_WF_B1_CMD_ADDR	0x0090
#define CONN_WT_SLP_CTL_REG_WB_GPS_RFBUF_ADDR	0x0094
#define CONN_WT_SLP_CTL_REG_WB_GPS_L5_EN_ADDR	0x0098

/**********************************************************************/
/* Base: GPT2 timer (0x1800_7000) */
/**********************************************************************/
#define CONN_GPT2_CTRL_BASE			0x18007000
#define CONN_GPT2_CTRL_THERMAL_EN	0x38

/**********************************************************************/
/* Base: debug_ctrl (0x1800_f000) */
/**********************************************************************/
#define CONN_DEBUG_CTRL_REG_OFFSET	0x0000


/**********************************************************************/
/* Base: conn_infra_sysram(0x1805_0000) */
/**********************************************************************/
#define CONN_INFRA_SYSRAM_SW_CR_A_DIE_CHIP_ID		0x2800
#define CONN_INFRA_SYSRAM_SW_CR_A_DIE_EFUSE_DATA_0	0x2804
#define CONN_INFRA_SYSRAM_SW_CR_A_DIE_EFUSE_DATA_1	0x2808
#define CONN_INFRA_SYSRAM_SW_CR_A_DIE_EFUSE_DATA_2	0x280C
#define CONN_INFRA_SYSRAM_SW_CR_A_DIE_EFUSE_DATA_3	0x2810

#define CONN_INFRA_SYSRAM_SW_CR_D_DIE_EFUSE		0x2820

#define CONN_INFRA_SYSRAM_SW_CR_A_DIE_TOP_CK_EN_CTRL	0x2830
#define CONN_INFRA_SYSRAM_SW_CR_RADIO_STATUS		0x2834
#define CONN_INFRA_SYSRAM_SW_CR_BUILD_MODE		0x2838

#define CONN_INFRA_SYSRAM_SIZE					(16 * 1024)


/**********************************************************************/
/* Base: conn_host_csr_top (0x1806_0000) */
/**********************************************************************/
#define CONN_HOST_CSR_TOP_CSR_DEADFEED_EN_CR				0x0124
#define CONN_HOST_CSR_TOP_CONN_INFRA_DEBUG_AO_DEBUGSYS		0x0128
#define CONN_HOST_CSR_TOP_CONN_INFRA_DEBUG_CTRL_AO2SYS_OUT	0x0148
#define CONN_HOST_CSR_TOP_CONN_SLP_PROT_CTRL				0x0184
#define CONN_HOST_CSR_TOP_CONN_INFRA_WAKEPU_TOP_CONN_INFRA_WAKEPU_TOP	0x01a0
/* remap */
#define CONN2AP_REMAP_MCU_EMI_BASE_ADDR_OFFSET				0x01c4
#define CONN2AP_REMAP_MD_SHARE_EMI_BASE_ADDR_OFFSET			0x01cc
#define CONN2AP_REMAP_GPS_EMI_BASE_ADDR_OFFSET				0x01d0
#define CONN2AP_REMAP_WF_PERI_BASE_ADDR_OFFSET				0x01d4
#define CONN2AP_REMAP_BT_PERI_BASE_ADDR_OFFSET				0x01d8
#define CONN2AP_REMAP_GPS_PERI_BASE_ADDR_OFFSET				0x01dc

#define CONN_HOST_CSR_WM_MCU_PC_DBG							0x0204
#define CONN_HOST_CSR_WM_MCU_GPR_DBG						0x0208
#define CONN_HOST_CSR_BGF_MCU_PC_DBG						0x022C

#define CONN_HOST_CSR_DBG_DUMMY_0							0x02C0
#define CONN_HOST_CSR_DBG_DUMMY_2							0x02C8
#define CONN_HOST_CSR_DBG_DUMMY_3							0x02CC
#define CONN_HOST_CSR_DBG_DUMMY_4							0x02D0
#define CONN_HOST_CSR_TOP_BUS_TIMEOUT_IRQ					0x02d4

#define TOP_BUS_MUC_STAT_HCLK_FR_CK_DETECT_BIT (0x1 << 1)
#define TOP_BUS_MUC_STAT_OSC_CLK_DETECT_BIT (0x1 << 2)
#define TOP_SLP_PROT_CTRL_CONN_INFRA_ON2OFF_SLP_PROT_ACK_BIT (0x1 << 5)

/**********************************************************************/
/* Base: conn_semaphore(0x1807_0000) */
/**********************************************************************/
#define CONN_SEMA_OWN_BY_M0_STA_REP		0x0400
#define CONN_SEMA_OWN_BY_M1_STA_REP		0x1400
#define CONN_SEMAPHORE_M2_OWN_STA		0x2000
#define CONN_SEMAPHORE_M2_OWN_REL		0x2200
#define CONN_SEMA_OWN_BY_M2_STA_REP		0x2400
#define CONN_SEMA_OWN_BY_M3_STA_REP		0x3400

/**********************************************************************/
/* A-die CR */
/**********************************************************************/
#define ATOP_CHIP_ID			0x02c
#define ATOP_RG_TOP_THADC_BG		0x034
#define ATOP_RG_TOP_THADC		0x038
#define ATOP_WRI_CTR2			0x064
#define ATOP_RG_ENCAL_WBTAC_IF_SW	0x070
#define ATOP_SMCTK11			0x0BC
#define ATOP_EFUSE_CTRL			0x108
#define ATOP_EFUSE_RDATA0		0x130
#define ATOP_EFUSE_RDATA1		0x134
#define ATOP_EFUSE_RDATA2		0x138
#define ATOP_EFUSE_RDATA3		0x13c
#define ATOP_RG_WF0_TOP_01		0x380
#define ATOP_RG_WF0_BG			0x384
#define ATOP_RG_WF1_BG			0x394
#define ATOP_RG_WF1_TOP_01		0x390
#define ATOP_RG_TOP_XTAL_01		0xA18
#define ATOP_RG_TOP_XTAL_02		0xA1C


#endif				/* _PLATFORM_MT6893_CONSSY_REG_OFFSET_H_ */
