/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _TYPEC_REG_H
#define _TYPEC_REG_H

#if FPGA_PLATFORM
	#define CC_REG_BASE 0x0
#elif defined(CONFIG_MTK_PMIC_CHIP_MT6353)
	#define CC_REG_BASE 0x100
#else
	#error
#endif

/* CC_REG REGISTER DEFINITION */

#define TYPE_C_PHY_RG_0                           (CC_REG_BASE+0x0000)
#define TYPE_C_PHY_RG_CC_RESERVE_CSR              (CC_REG_BASE+0x0002)
#define TYPE_C_VCMP_CTRL                          (CC_REG_BASE+0x0004)
#define TYPE_C_CTRL                               (CC_REG_BASE+0x0006)
#define TYPE_C_CC_SW_CTRL                         (CC_REG_BASE+0x000A)
#define TYPE_C_CC_VOL_PERIODIC_MEAS_VAL           (CC_REG_BASE+0x000C)
#define TYPE_C_CC_VOL_DEBOUCE_CNT_VAL             (CC_REG_BASE+0x000E)
#define TYPE_C_DRP_SRC_CNT_VAL_0                  (CC_REG_BASE+0x0010)
#define TYPE_C_DRP_SNK_CNT_VAL_0                  (CC_REG_BASE+0x0014)
#define TYPE_C_DRP_TRY_CNT_VAL_0                  (CC_REG_BASE+0x0018)
#define TYPE_C_DRP_TRY_WAIT_CNT_VAL_0             (CC_REG_BASE+0x001C)
#define TYPE_C_DRP_TRY_WAIT_CNT_VAL_1             (CC_REG_BASE+0x001E)
#define TYPE_C_CC_SRC_DEFAULT_DAC_VAL             (CC_REG_BASE+0x0020)
#define TYPE_C_CC_SRC_15_DAC_VAL                  (CC_REG_BASE+0x0022)
#define TYPE_C_CC_SRC_30_DAC_VAL                  (CC_REG_BASE+0x0024)
#define TYPE_C_CC_SNK_DAC_VAL_0                   (CC_REG_BASE+0x0028)
#define TYPE_C_CC_SNK_DAC_VAL_1                   (CC_REG_BASE+0x002A)
#define TYPE_C_INTR_EN_0                          (CC_REG_BASE+0x0030)
#define TYPE_C_INTR_EN_2                          (CC_REG_BASE+0x0034)
#define TYPE_C_INTR_0                             (CC_REG_BASE+0x0038)
#define TYPE_C_INTR_2                             (CC_REG_BASE+0x003C)
#define TYPE_C_CC_STATUS                          (CC_REG_BASE+0x0040)
#define TYPE_C_PWR_STATUS                         (CC_REG_BASE+0x0042)
#define TYPE_C_PHY_RG_CC1_RESISTENCE_0            (CC_REG_BASE+0x0044)
#define TYPE_C_PHY_RG_CC1_RESISTENCE_1            (CC_REG_BASE+0x0046)
#define TYPE_C_PHY_RG_CC2_RESISTENCE_0            (CC_REG_BASE+0x0048)
#define TYPE_C_PHY_RG_CC2_RESISTENCE_1            (CC_REG_BASE+0x004A)
#define TYPE_C_CC_SW_FORCE_MODE_ENABLE            (CC_REG_BASE+0x0060)
#define TYPE_C_CC_SW_FORCE_MODE_VAL_0             (CC_REG_BASE+0x0064)
#define TYPE_C_CC_SW_FORCE_MODE_VAL_1             (CC_REG_BASE+0x0066)
#define TYPE_C_CC_DAC_CALI_CTRL                   (CC_REG_BASE+0x0070)
#define TYPE_C_CC_DAC_CALI_RESULT                 (CC_REG_BASE+0x0072)
#define TYPE_C_DEBUG_PORT_SELECT_0                (CC_REG_BASE+0x0080)
#define TYPE_C_DEBUG_PORT_SELECT_1                (CC_REG_BASE+0x0082)
#define TYPE_C_DEBUG_MODE_SELECT                  (CC_REG_BASE+0x0084)
#define TYPE_C_DEBUG_OUT_READ_0                   (CC_REG_BASE+0x0088)
#define TYPE_C_DEBUG_OUT_READ_1                   (CC_REG_BASE+0x008A)
#define TYPE_C_FPGA_CTRL                          (CC_REG_BASE+0x00F0)
#define TYPE_C_FPGA_STATUS                        (CC_REG_BASE+0x00F2)
#define TYPE_C_FPGA_HW_VERSION                    (CC_REG_BASE+0x00F4)

/* CC_REG FIELD DEFINITION */

/* TYPE_C_PHY_RG_0 */
#define REG_TYPE_C_PHY_RG_CC_MPX_SEL              (0xff<<8) /* 15:8 */
#define REG_TYPE_C_PHY_RG_PD_TXSLEW_I             (0x7<<4) /* 6:4 */
#define REG_TYPE_C_PHY_RG_PD_TX_SLEW_CALEN        (0x1<<3) /* 3:3 */
#define TYPE_C_PHY_RG_CC_RP_SEL                   (0x3<<0) /* 1:0 */

/* TYPE_C_PHY_RG_CC_RESERVE_CSR */
#define REG_TYPE_C_PHY_RG_CC_RESERVE              (0xff<<0) /* 7:0 */

/* TYPE_C_VCMP_CTRL */
#define REG_TYPE_C_VCMP_DAC_EN_ST_CNT_VAL         (0x1<<7) /* 7:7 */
#define REG_TYPE_C_VCMP_BIAS_EN_ST_CNT_VAL        (0x3<<5) /* 6:5 */
#define REG_TYPE_C_VCMP_CC2_SW_SEL_ST_CNT_VAL     (0x1f<<0) /* 4:0 */

/* TYPE_C_CTRL */
#define REG_TYPE_C_DISABLE_ST_RD_EN               (0x1<<10) /* 10:10 */
#define REG_TYPE_C_ATTACH_SRC_OPEN_PDDEBOUNCE_EN  (0x1<<9) /* 9:9 */
#define REG_TYPE_C_PD2CC_DET_DISABLE_EN           (0x1<<8) /* 8:8 */
#define REG_TYPE_C_ATTACH_SRC_2_TRY_WAIT_SNK_ST_EN (0x1<<7) /* 7:7 */
#define REG_TYPE_C_TRY_SRC_ST_EN                  (0x1<<6) /* 6:6 */
#define REG_TYPE_C_DEBUG_ACC_EN                   (0x1<<5) /* 5:5 */
#define REG_TYPE_C_AUDIO_ACC_EN                   (0x1<<4) /* 4:4 */
#define REG_TYPE_C_ACC_EN                         (0x1<<3) /* 3:3 */
#define REG_TYPE_C_ADC_EN                         (0x1<<2) /* 2:2 */
#define REG_TYPE_C_PORT_SUPPORT_ROLE              (0x3<<0) /* 1:0 */

/* TYPE_C_CC_SW_CTRL */
#define W1_TYPE_C_SW_ENT_SNK_PWR_REDETECT_CMD     (0x1<<15) /* 15:15 */
#define TYPE_C_SW_PD_EN                           (0x1<<12) /* 12:12 */
#define TYPE_C_SW_CC_DET_DIS                      (0x1<<11) /* 11:11 */
#define TYPE_C_SW_VBUS_DET_DIS                    (0x1<<10) /* 10:10 */
#define TYPE_C_SW_DA_DRIVE_VCONN_EN               (0x1<<9) /* 9:9 */
#define TYPE_C_SW_VBUS_PRESENT                    (0x1<<8) /* 8:8 */
#define W1_TYPE_C_SW_ADC_RESULT_MET_VRD_30_CMD    (0x1<<7) /* 7:7 */
#define W1_TYPE_C_SW_ADC_RESULT_MET_VRD_15_CMD    (0x1<<6) /* 6:6 */
#define W1_TYPE_C_SW_ADC_RESULT_MET_VRD_DEFAULT_CMD (0x1<<5) /* 5:5 */
#define W1_TYPE_C_SW_VCONN_SWAP_INDICATE_CMD      (0x1<<4) /* 4:4 */
#define W1_TYPE_C_SW_PR_SWAP_INDICATE_CMD         (0x1<<3) /* 3:3 */
#define W1_TYPE_C_SW_ENT_UNATCH_SNK_CMD           (0x1<<2) /* 2:2 */
#define W1_TYPE_C_SW_ENT_UNATCH_SRC_CMD           (0x1<<1) /* 1:1 */
#define W1_TYPE_C_SW_ENT_DISABLE_CMD              (0x1<<0) /* 0:0 */

/* TYPE_C_CC_VOL_PERIODIC_MEAS_VAL */
#define REG_TYPE_C_CC_VOL_PERIODIC_MEAS_VAL       (0x1fff<<0) /* 12:0 */

/* TYPE_C_CC_VOL_DEBOUCE_CNT_VAL */
#define REG_TYPE_C_CC_VOL_PD_DEBOUNCE_CNT_VAL     (0x1f<<8) /* 12:8 */
#define REG_TYPE_C_CC_VOL_CC_DEBOUNCE_CNT_VAL     (0xff<<0) /* 7:0 */

/* TYPE_C_DRP_SRC_CNT_VAL_0 */
#define REG_TYPE_C_DRP_SRC_CNT_VAL_0              (0xffff<<0) /* 15:0 */

/* TYPE_C_DRP_SNK_CNT_VAL_0 */
#define REG_TYPE_C_DRP_SNK_CNT_VAL_0              (0xffff<<0) /* 15:0 */

/* TYPE_C_DRP_TRY_CNT_VAL_0 */
#define REG_TYPE_C_DRP_TRY_CNT_VAL_0              (0xffff<<0) /* 15:0 */

/* TYPE_C_DRP_TRY_WAIT_CNT_VAL_0 */
#define REG_TYPE_C_DRP_TRY_WAIT_CNT_VAL_0         (0xffff<<0) /* 15:0 */

/* TYPE_C_DRP_TRY_WAIT_CNT_VAL_1 */
#define REG_TYPE_C_DRP_TRY_WAIT_CNT_VAL_1         (0xf<<0) /* 3:0 */

/* TYPE_C_CC_SRC_DEFAULT_DAC_VAL */
#define REG_TYPE_C_CC_SRC_VRD_DEFAULT_DAC_VAL     (0x3f<<8) /* 13:8 */
#define REG_TYPE_C_CC_SRC_VOPEN_DEFAULT_DAC_VAL   (0x3f<<0) /* 5:0 */

/* TYPE_C_CC_SRC_15_DAC_VAL */
#define REG_TYPE_C_CC_SRC_VRD_15_DAC_VAL          (0x3f<<8) /* 13:8 */
#define REG_TYPE_C_CC_SRC_VOPEN_15_DAC_VAL        (0x3f<<0) /* 5:0 */

/* TYPE_C_CC_SRC_30_DAC_VAL */
#define REG_TYPE_C_CC_SRC_VRD_30_DAC_VAL          (0x3f<<8) /* 13:8 */
#define REG_TYPE_C_CC_SRC_VOPEN_30_DAC_VAL        (0x3f<<0) /* 5:0 */

/* TYPE_C_CC_SNK_DAC_VAL_0 */
#define REG_TYPE_C_CC_SNK_VRP15_DAC_VAL           (0x3f<<8) /* 13:8 */
#define REG_TYPE_C_CC_SNK_VRP30_DAC_VAL           (0x3f<<0) /* 5:0 */

/* TYPE_C_CC_SNK_DAC_VAL_1 */
#define REG_TYPE_C_CC_SNK_VRPUSB_DAC_VAL          (0x3f<<0) /* 5:0 */

/* TYPE_C_INTR_EN_0 */
#define REG_TYPE_C_CC_ENT_ATTACH_WAIT_ACC_INTR_EN (0x1<<12) /* 12:12 */
#define REG_TYPE_C_CC_ENT_UNATTACH_ACC_INTR_EN    (0x1<<11) /* 11:11 */
#define REG_TYPE_C_CC_ENT_TRY_WAIT_SNK_INTR_EN    (0x1<<10) /* 10:10 */
#define REG_TYPE_C_CC_ENT_TRY_SRC_INTR_EN         (0x1<<9) /* 9:9 */
#define REG_TYPE_C_CC_ENT_ATTACH_WAIT_SNK_INTR_EN (0x1<<8) /* 8:8 */
#define REG_TYPE_C_CC_ENT_ATTACH_WAIT_SRC_INTR_EN (0x1<<7) /* 7:7 */
#define REG_TYPE_C_CC_ENT_UNATTACH_SNK_INTR_EN    (0x1<<6) /* 6:6 */
#define REG_TYPE_C_CC_ENT_UNATTACH_SRC_INTR_EN    (0x1<<5) /* 5:5 */
#define REG_TYPE_C_CC_ENT_DISABLE_INTR_EN         (0x1<<4) /* 4:4 */
#define REG_TYPE_C_CC_ENT_DBG_ACC_INTR_EN         (0x1<<3) /* 3:3 */
#define REG_TYPE_C_CC_ENT_AUDIO_ACC_INTR_EN       (0x1<<2) /* 2:2 */
#define REG_TYPE_C_CC_ENT_ATTACH_SNK_INTR_EN      (0x1<<1) /* 1:1 */
#define REG_TYPE_C_CC_ENT_ATTACH_SRC_INTR_EN      (0x1<<0) /* 0:0 */

/* TYPE_C_INTR_EN_2 */
#define REG_TYPE_C_CC_ENT_SNK_PWR_REDETECT_INTR_EN (0x1<<4) /* 4:4 */
#define REG_TYPE_C_CC_ENT_SNK_PWR_30_INTR_EN      (0x1<<3) /* 3:3 */
#define REG_TYPE_C_CC_ENT_SNK_PWR_15_INTR_EN      (0x1<<2) /* 2:2 */
#define REG_TYPE_C_CC_ENT_SNK_PWR_DEFAULT_INTR_EN (0x1<<1) /* 1:1 */
#define REG_TYPE_C_CC_ENT_SNK_PWR_IDLE_INTR_EN    (0x1<<0) /* 0:0 */

/* TYPE_C_INTR_0 */
#define TYPE_C_CC_ENT_ATTACH_WAIT_ACC_INTR        (0x1<<12) /* 12:12 */
#define TYPE_C_CC_ENT_UNATTACH_ACC_INTR           (0x1<<11) /* 11:11 */
#define TYPE_C_CC_ENT_TRY_WAIT_SNK_INTR           (0x1<<10) /* 10:10 */
#define TYPE_C_CC_ENT_TRY_SRC_INTR                (0x1<<9) /* 9:9 */
#define TYPE_C_CC_ENT_ATTACH_WAIT_SNK_INTR        (0x1<<8) /* 8:8 */
#define TYPE_C_CC_ENT_ATTACH_WAIT_SRC_INTR        (0x1<<7) /* 7:7 */
#define TYPE_C_CC_ENT_UNATTACH_SNK_INTR           (0x1<<6) /* 6:6 */
#define TYPE_C_CC_ENT_UNATTACH_SRC_INTR           (0x1<<5) /* 5:5 */
#define TYPE_C_CC_ENT_DISABLE_INTR                (0x1<<4) /* 4:4 */
#define TYPE_C_CC_ENT_DBG_ACC_INTR                (0x1<<3) /* 3:3 */
#define TYPE_C_CC_ENT_AUDIO_ACC_INTR              (0x1<<2) /* 2:2 */
#define TYPE_C_CC_ENT_ATTACH_SNK_INTR             (0x1<<1) /* 1:1 */
#define TYPE_C_CC_ENT_ATTACH_SRC_INTR             (0x1<<0) /* 0:0 */

/* TYPE_C_INTR_2 */
#define TYPE_C_CC_ENT_SNK_PWR_REDETECT_INTR       (0x1<<4) /* 4:4 */
#define TYPE_C_CC_ENT_SNK_PWR_30_INTR             (0x1<<3) /* 3:3 */
#define TYPE_C_CC_ENT_SNK_PWR_15_INTR             (0x1<<2) /* 2:2 */
#define TYPE_C_CC_ENT_SNK_PWR_DEFAULT_INTR        (0x1<<1) /* 1:1 */
#define TYPE_C_CC_ENT_SNK_PWR_IDLE_INTR           (0x1<<0) /* 0:0 */

/* TYPE_C_CC_STATUS */
#define RO_TYPE_C_ROUTED_CC                       (0x1<<15) /* 15:15 */
#define RO_TYPE_C_CC_ST                           (0xf<<0) /* 3:0 */

/* TYPE_C_PWR_STATUS */
#define RO_AD_CC_VUSB33_RDY                       (0x1<<7) /* 7:7 */
#define RO_TYPE_C_AD_CC_CMP_OUT                   (0x1<<6) /* 6:6 */
#define RO_TYPE_C_DRIVE_VCONN_CAPABLE             (0x1<<5) /* 5:5 */
#define RO_TYPE_C_CC_PWR_ROLE                     (0x1<<4) /* 4:4 */
#define RO_TYPE_C_CC_SNK_PWR_ST                   (0x7<<0) /* 2:0 */

/* TYPE_C_PHY_RG_CC1_RESISTENCE_0 */
#define REG_TYPE_C_PHY_RG_CC1_RP15                (0x1f<<8) /* 12:8 */
#define REG_TYPE_C_PHY_RG_CC1_RPDE                (0x7<<0) /* 2:0 */

/* TYPE_C_PHY_RG_CC1_RESISTENCE_1 */
#define REG_TYPE_C_PHY_RG_CC1_RD                  (0x1f<<8) /* 12:8 */
#define REG_TYPE_C_PHY_RG_CC1_RP3                 (0x1f<<0) /* 4:0 */

/* TYPE_C_PHY_RG_CC2_RESISTENCE_0 */
#define REG_TYPE_C_PHY_RG_CC2_RP15                (0x1f<<8) /* 12:8 */
#define REG_TYPE_C_PHY_RG_CC2_RPDE                (0x7<<0) /* 2:0 */

/* TYPE_C_PHY_RG_CC2_RESISTENCE_1 */
#define REG_TYPE_C_PHY_RG_CC2_RD                  (0x1f<<8) /* 12:8 */
#define REG_TYPE_C_PHY_RG_CC2_RP3                 (0x1f<<0) /* 4:0 */

/* TYPE_C_CC_SW_FORCE_MODE_ENABLE */
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_DAC_GAIN_CAL (0x1<<12) /* 12:12 */
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_DAC_CAL (0x1<<11) /* 11:11 */
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_DAC_IN  (0x1<<10) /* 10:10 */
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_SACLK   (0x1<<9) /* 9:9 */
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_DAC_EN  (0x1<<8) /* 8:8 */
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_SASW_EN (0x1<<7) /* 7:7 */
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_ADCSW_EN (0x1<<6) /* 6:6 */
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_LPF_EN  (0x1<<5) /* 5:5 */
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_BIAS_EN (0x1<<4) /* 4:4 */
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_SW_SEL  (0x1<<3) /* 3:3 */
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_LEV_EN  (0x1<<2) /* 2:2 */
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_RCC2    (0x1<<1) /* 1:1 */
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_RCC1    (0x1<<0) /* 0:0 */

/* TYPE_C_CC_SW_FORCE_MODE_VAL_0 */
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_SACLK      (0x1<<13) /* 13:13 */
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_DAC_EN     (0x1<<12) /* 12:12 */
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_SASW_EN    (0x1<<11) /* 11:11 */
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_ADCSW_EN   (0x1<<10) /* 10:10 */
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_LPF_EN     (0x1<<9) /* 9:9 */
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_BIAS_EN    (0x1<<8) /* 8:8 */
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_SW_SEL     (0x1<<7) /* 7:7 */
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_LEV_EN     (0x1<<6) /* 6:6 */
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_RACC2_EN   (0x1<<5) /* 5:5 */
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_RDCC2_EN   (0x1<<4) /* 4:4 */
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_RPCC2_EN   (0x1<<3) /* 3:3 */
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_RACC1_EN   (0x1<<2) /* 2:2 */
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_RDCC1_EN   (0x1<<1) /* 1:1 */
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_RPCC1_EN   (0x1<<0) /* 0:0 */

/* TYPE_C_CC_SW_FORCE_MODE_VAL_1 */
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_DAC_GAIN_CAL (0xf<<12) /* 15:12 */
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_DAC_CAL    (0xf<<8) /* 11:8 */
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_DAC_IN     (0x3f<<0) /* 5:0 */

/* TYPE_C_CC_DAC_CALI_CTRL */
#define RO_TYPE_C_DAC_FAIL                        (0x1<<5) /* 5:5 */
#define RO_TYPE_C_DAC_OK                          (0x1<<4) /* 4:4 */
#define REG_TYPE_C_DAC_CAL_STAGE                  (0x1<<1) /* 1:1 */
#define TYPE_C_DAC_CAL_START                      (0x1<<0) /* 0:0 */

/* TYPE_C_CC_DAC_CALI_RESULT */
#define RO_DA_CC_DAC_GAIN_CAL                     (0xf<<4) /* 7:4 */
#define RO_DA_CC_DAC_CAL                          (0xf<<0) /* 3:0 */

/* TYPE_C_DEBUG_PORT_SELECT_0 */
#define REG_TYPE_C_DBG_PORT_SEL_0                 (0xffff<<0) /* 15:0 */

/* TYPE_C_DEBUG_PORT_SELECT_1 */
#define REG_TYPE_C_DBG_PORT_SEL_1                 (0xffff<<0) /* 15:0 */

/* TYPE_C_DEBUG_MODE_SELECT */
#define REG_TYPE_C_DBG_MOD_SEL                    (0xffff<<0) /* 15:0 */

/* TYPE_C_DEBUG_OUT_READ_0 */
#define RO_TYPE_C_DBG_OUT_READ_0                  (0xffff<<0) /* 15:0 */

/* TYPE_C_DEBUG_OUT_READ_1 */
#define RO_TYPE_C_DBG_OUT_READ_1                  (0xffff<<0) /* 15:0 */

/* TYPE_C_FPGA_CTRL */
#define TYPE_C_FPGA_DBG_PORT_SEL_B3               (0x1<<7) /* 7:7 */
#define TYPE_C_FPGA_DBG_PORT_SEL_B2               (0x1<<6) /* 6:6 */
#define TYPE_C_FPGA_DBG_PORT_SEL_B1               (0x1<<5) /* 5:5 */
#define TYPE_C_FPGA_DBG_PORT_SEL_B0               (0x1<<4) /* 4:4 */
#define TYPE_C_FPGA_VBSU_PWR_EN                   (0x1<<1) /* 1:1 */
#define TYPE_C_FPGA_DAC_CLR_N                     (0x1<<0) /* 0:0 */

/* TYPE_C_FPGA_STATUS */
#define TYPE_C_FPGA_VBUS_VSAFE_0V_MON_N           (0x1<<3) /* 3:3 */
#define TYPE_C_FPGA_VBUS_VSAFE_5V_MON_N           (0x1<<2) /* 2:2 */
#define TYPE_C_FPGA_VBUS_PWR_FAULT_N              (0x1<<1) /* 1:1 */
#define TYPE_C_FPGA_VCONN_PWR_FAULT_N             (0x1<<0) /* 0:0 */

/* TYPE_C_FPGA_HW_VERSION */
#define TYPE_C_FPGA_HW_VERSION_RG                   (0x1<<0) /* 0:0 */


/* CC_REG FIELD OFFSET DEFINITION */

/* TYPE_C_PHY_RG_0 */
#define REG_TYPE_C_PHY_RG_CC_MPX_SEL_OFST         (8)
#define REG_TYPE_C_PHY_RG_PD_TXSLEW_I_OFST        (4)
#define REG_TYPE_C_PHY_RG_PD_TX_SLEW_CALEN_OFST   (3)
#define TYPE_C_PHY_RG_CC_RP_SEL_OFST              (0)

/* TYPE_C_PHY_RG_CC_RESERVE_CSR */
#define REG_TYPE_C_PHY_RG_CC_RESERVE_OFST         (0)

/* TYPE_C_VCMP_CTRL */
#define REG_TYPE_C_VCMP_DAC_EN_ST_CNT_VAL_OFST    (7)
#define REG_TYPE_C_VCMP_BIAS_EN_ST_CNT_VAL_OFST   (5)
#define REG_TYPE_C_VCMP_CC2_SW_SEL_ST_CNT_VAL_OFST (0)

/* TYPE_C_CTRL */
#define REG_TYPE_C_DISABLE_ST_RD_EN_OFST          (10)
#define REG_TYPE_C_ATTACH_SRC_OPEN_PDDEBOUNCE_EN_OFST (9)
#define REG_TYPE_C_PD2CC_DET_DISABLE_EN_OFST      (8)
#define REG_TYPE_C_ATTACH_SRC_2_TRY_WAIT_SNK_ST_EN_OFST (7)
#define REG_TYPE_C_TRY_SRC_ST_EN_OFST             (6)
#define REG_TYPE_C_DEBUG_ACC_EN_OFST              (5)
#define REG_TYPE_C_AUDIO_ACC_EN_OFST              (4)
#define REG_TYPE_C_ACC_EN_OFST                    (3)
#define REG_TYPE_C_ADC_EN_OFST                    (2)
#define REG_TYPE_C_PORT_SUPPORT_ROLE_OFST         (0)

/* TYPE_C_CC_SW_CTRL */
#define W1_TYPE_C_SW_ENT_SNK_PWR_REDETECT_CMD_OFST (15)
#define TYPE_C_SW_PD_EN_OFST                      (12)
#define TYPE_C_SW_CC_DET_DIS_OFST                 (11)
#define TYPE_C_SW_VBUS_DET_DIS_OFST               (10)
#define TYPE_C_SW_DA_DRIVE_VCONN_EN_OFST          (9)
#define TYPE_C_SW_VBUS_PRESENT_OFST               (8)
#define W1_TYPE_C_SW_ADC_RESULT_MET_VRD_30_CMD_OFST (7)
#define W1_TYPE_C_SW_ADC_RESULT_MET_VRD_15_CMD_OFST (6)
#define W1_TYPE_C_SW_ADC_RESULT_MET_VRD_DEFAULT_CMD_OFST (5)
#define W1_TYPE_C_SW_VCONN_SWAP_INDICATE_CMD_OFST (4)
#define W1_TYPE_C_SW_PR_SWAP_INDICATE_CMD_OFST    (3)
#define W1_TYPE_C_SW_ENT_UNATCH_SNK_CMD_OFST      (2)
#define W1_TYPE_C_SW_ENT_UNATCH_SRC_CMD_OFST      (1)
#define W1_TYPE_C_SW_ENT_DISABLE_CMD_OFST         (0)

/* TYPE_C_CC_VOL_PERIODIC_MEAS_VAL */
#define REG_TYPE_C_CC_VOL_PERIODIC_MEAS_VAL_OFST  (0)

/* TYPE_C_CC_VOL_DEBOUCE_CNT_VAL */
#define REG_TYPE_C_CC_VOL_PD_DEBOUNCE_CNT_VAL_OFST (8)
#define REG_TYPE_C_CC_VOL_CC_DEBOUNCE_CNT_VAL_OFST (0)

/* TYPE_C_DRP_SRC_CNT_VAL_0 */
#define REG_TYPE_C_DRP_SRC_CNT_VAL_0_OFST         (0)

/* TYPE_C_DRP_SNK_CNT_VAL_0 */
#define REG_TYPE_C_DRP_SNK_CNT_VAL_0_OFST         (0)

/* TYPE_C_DRP_TRY_CNT_VAL_0 */
#define REG_TYPE_C_DRP_TRY_CNT_VAL_0_OFST         (0)

/* TYPE_C_DRP_TRY_WAIT_CNT_VAL_0 */
#define REG_TYPE_C_DRP_TRY_WAIT_CNT_VAL_0_OFST    (0)

/* TYPE_C_DRP_TRY_WAIT_CNT_VAL_1 */
#define REG_TYPE_C_DRP_TRY_WAIT_CNT_VAL_1_OFST    (0)

/* TYPE_C_CC_SRC_DEFAULT_DAC_VAL */
#define REG_TYPE_C_CC_SRC_VRD_DEFAULT_DAC_VAL_OFST (8)
#define REG_TYPE_C_CC_SRC_VOPEN_DEFAULT_DAC_VAL_OFST (0)

/* TYPE_C_CC_SRC_15_DAC_VAL */
#define REG_TYPE_C_CC_SRC_VRD_15_DAC_VAL_OFST     (8)
#define REG_TYPE_C_CC_SRC_VOPEN_15_DAC_VAL_OFST   (0)

/* TYPE_C_CC_SRC_30_DAC_VAL */
#define REG_TYPE_C_CC_SRC_VRD_30_DAC_VAL_OFST     (8)
#define REG_TYPE_C_CC_SRC_VOPEN_30_DAC_VAL_OFST   (0)

/* TYPE_C_CC_SNK_DAC_VAL_0 */
#define REG_TYPE_C_CC_SNK_VRP15_DAC_VAL_OFST      (8)
#define REG_TYPE_C_CC_SNK_VRP30_DAC_VAL_OFST      (0)

/* TYPE_C_CC_SNK_DAC_VAL_1 */
#define REG_TYPE_C_CC_SNK_VRPUSB_DAC_VAL_OFST     (0)

/* TYPE_C_INTR_EN_0 */
#define REG_TYPE_C_CC_ENT_ATTACH_WAIT_ACC_INTR_EN_OFST (12)
#define REG_TYPE_C_CC_ENT_UNATTACH_ACC_INTR_EN_OFST (11)
#define REG_TYPE_C_CC_ENT_TRY_WAIT_SNK_INTR_EN_OFST (10)
#define REG_TYPE_C_CC_ENT_TRY_SRC_INTR_EN_OFST    (9)
#define REG_TYPE_C_CC_ENT_ATTACH_WAIT_SNK_INTR_EN_OFST (8)
#define REG_TYPE_C_CC_ENT_ATTACH_WAIT_SRC_INTR_EN_OFST (7)
#define REG_TYPE_C_CC_ENT_UNATTACH_SNK_INTR_EN_OFST (6)
#define REG_TYPE_C_CC_ENT_UNATTACH_SRC_INTR_EN_OFST (5)
#define REG_TYPE_C_CC_ENT_DISABLE_INTR_EN_OFST    (4)
#define REG_TYPE_C_CC_ENT_DBG_ACC_INTR_EN_OFST    (3)
#define REG_TYPE_C_CC_ENT_AUDIO_ACC_INTR_EN_OFST  (2)
#define REG_TYPE_C_CC_ENT_ATTACH_SNK_INTR_EN_OFST (1)
#define REG_TYPE_C_CC_ENT_ATTACH_SRC_INTR_EN_OFST (0)

/* TYPE_C_INTR_EN_2 */
#define REG_TYPE_C_CC_ENT_SNK_PWR_REDETECT_INTR_EN_OFST (4)
#define REG_TYPE_C_CC_ENT_SNK_PWR_30_INTR_EN_OFST (3)
#define REG_TYPE_C_CC_ENT_SNK_PWR_15_INTR_EN_OFST (2)
#define REG_TYPE_C_CC_ENT_SNK_PWR_DEFAULT_INTR_EN_OFST (1)
#define REG_TYPE_C_CC_ENT_SNK_PWR_IDLE_INTR_EN_OFST (0)

/* TYPE_C_INTR_0 */
#define TYPE_C_CC_ENT_ATTACH_WAIT_ACC_INTR_OFST   (12)
#define TYPE_C_CC_ENT_UNATTACH_ACC_INTR_OFST      (11)
#define TYPE_C_CC_ENT_TRY_WAIT_SNK_INTR_OFST      (10)
#define TYPE_C_CC_ENT_TRY_SRC_INTR_OFST           (9)
#define TYPE_C_CC_ENT_ATTACH_WAIT_SNK_INTR_OFST   (8)
#define TYPE_C_CC_ENT_ATTACH_WAIT_SRC_INTR_OFST   (7)
#define TYPE_C_CC_ENT_UNATTACH_SNK_INTR_OFST      (6)
#define TYPE_C_CC_ENT_UNATTACH_SRC_INTR_OFST      (5)
#define TYPE_C_CC_ENT_DISABLE_INTR_OFST           (4)
#define TYPE_C_CC_ENT_DBG_ACC_INTR_OFST           (3)
#define TYPE_C_CC_ENT_AUDIO_ACC_INTR_OFST         (2)
#define TYPE_C_CC_ENT_ATTACH_SNK_INTR_OFST        (1)
#define TYPE_C_CC_ENT_ATTACH_SRC_INTR_OFST        (0)

/* TYPE_C_INTR_2 */
#define TYPE_C_CC_ENT_SNK_PWR_REDETECT_INTR_OFST  (4)
#define TYPE_C_CC_ENT_SNK_PWR_30_INTR_OFST        (3)
#define TYPE_C_CC_ENT_SNK_PWR_15_INTR_OFST        (2)
#define TYPE_C_CC_ENT_SNK_PWR_DEFAULT_INTR_OFST   (1)
#define TYPE_C_CC_ENT_SNK_PWR_IDLE_INTR_OFST      (0)

/* TYPE_C_CC_STATUS */
#define RO_TYPE_C_ROUTED_CC_OFST                  (15)
#define RO_TYPE_C_CC_ST_OFST                      (0)

/* TYPE_C_PWR_STATUS */
#define RO_AD_CC_VUSB33_RDY_OFST                  (7)
#define RO_TYPE_C_AD_CC_CMP_OUT_OFST              (6)
#define RO_TYPE_C_DRIVE_VCONN_CAPABLE_OFST        (5)
#define RO_TYPE_C_CC_PWR_ROLE_OFST                (4)
#define RO_TYPE_C_CC_SNK_PWR_ST_OFST              (0)

/* TYPE_C_PHY_RG_CC1_RESISTENCE_0 */
#define REG_TYPE_C_PHY_RG_CC1_RP15_OFST           (8)
#define REG_TYPE_C_PHY_RG_CC1_RPDE_OFST           (0)

/* TYPE_C_PHY_RG_CC1_RESISTENCE_1 */
#define REG_TYPE_C_PHY_RG_CC1_RD_OFST             (8)
#define REG_TYPE_C_PHY_RG_CC1_RP3_OFST            (0)

/* TYPE_C_PHY_RG_CC2_RESISTENCE_0 */
#define REG_TYPE_C_PHY_RG_CC2_RP15_OFST           (8)
#define REG_TYPE_C_PHY_RG_CC2_RPDE_OFST           (0)

/* TYPE_C_PHY_RG_CC2_RESISTENCE_1 */
#define REG_TYPE_C_PHY_RG_CC2_RD_OFST             (8)
#define REG_TYPE_C_PHY_RG_CC2_RP3_OFST            (0)

/* TYPE_C_CC_SW_FORCE_MODE_ENABLE */
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_DAC_GAIN_CAL_OFST (12)
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_DAC_CAL_OFST (11)
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_DAC_IN_OFST (10)
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_SACLK_OFST (9)
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_DAC_EN_OFST (8)
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_SASW_EN_OFST (7)
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_ADCSW_EN_OFST (6)
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_LPF_EN_OFST (5)
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_BIAS_EN_OFST (4)
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_SW_SEL_OFST (3)
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_LEV_EN_OFST (2)
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_RCC2_OFST (1)
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_RCC1_OFST (0)

/* TYPE_C_CC_SW_FORCE_MODE_VAL_0 */
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_SACLK_OFST (13)
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_DAC_EN_OFST (12)
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_SASW_EN_OFST (11)
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_ADCSW_EN_OFST (10)
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_LPF_EN_OFST (9)
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_BIAS_EN_OFST (8)
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_SW_SEL_OFST (7)
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_LEV_EN_OFST (6)
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_RACC2_EN_OFST (5)
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_RDCC2_EN_OFST (4)
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_RPCC2_EN_OFST (3)
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_RACC1_EN_OFST (2)
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_RDCC1_EN_OFST (1)
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_RPCC1_EN_OFST (0)

/* TYPE_C_CC_SW_FORCE_MODE_VAL_1 */
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_DAC_GAIN_CAL_OFST (12)
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_DAC_CAL_OFST (8)
#define REG_TYPE_C_SW_FORCE_MODE_DA_CC_DAC_IN_OFST (0)

/* TYPE_C_CC_DAC_CALI_CTRL */
#define RO_TYPE_C_DAC_FAIL_OFST                   (5)
#define RO_TYPE_C_DAC_OK_OFST                     (4)
#define REG_TYPE_C_DAC_CAL_STAGE_OFST             (1)
#define TYPE_C_DAC_CAL_START_OFST                 (0)

/* TYPE_C_CC_DAC_CALI_RESULT */
#define RO_DA_CC_DAC_GAIN_CAL_OFST                (4)
#define RO_DA_CC_DAC_CAL_OFST                     (0)

/* TYPE_C_DEBUG_PORT_SELECT_0 */
#define REG_TYPE_C_DBG_PORT_SEL_0_OFST            (0)

/* TYPE_C_DEBUG_PORT_SELECT_1 */
#define REG_TYPE_C_DBG_PORT_SEL_1_OFST            (0)

/* TYPE_C_DEBUG_MODE_SELECT */
#define REG_TYPE_C_DBG_MOD_SEL_OFST               (0)

/* TYPE_C_DEBUG_OUT_READ_0 */
#define RO_TYPE_C_DBG_OUT_READ_0_OFST             (0)

/* TYPE_C_DEBUG_OUT_READ_1 */
#define RO_TYPE_C_DBG_OUT_READ_1_OFST             (0)

/* TYPE_C_FPGA_CTRL */
#define TYPE_C_FPGA_DBG_PORT_SEL_B3_OFST          (7)
#define TYPE_C_FPGA_DBG_PORT_SEL_B2_OFST          (6)
#define TYPE_C_FPGA_DBG_PORT_SEL_B1_OFST          (5)
#define TYPE_C_FPGA_DBG_PORT_SEL_B0_OFST          (4)
#define TYPE_C_FPGA_VBSU_PWR_EN_OFST              (1)
#define TYPE_C_FPGA_DAC_CLR_N_OFST                (0)

/* TYPE_C_FPGA_STATUS */
#define TYPE_C_FPGA_VBUS_VSAFE_0V_MON_N_OFST      (3)
#define TYPE_C_FPGA_VBUS_VSAFE_5V_MON_N_OFST      (2)
#define TYPE_C_FPGA_VBUS_PWR_FAULT_N_OFST         (1)
#define TYPE_C_FPGA_VCONN_PWR_FAULT_N_OFST        (0)

/* TYPE_C_FPGA_HW_VERSION */
#define TYPE_C_FPGA_HW_VERSION_OFST               (0)

#endif
