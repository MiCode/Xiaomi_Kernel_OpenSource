/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _MSDC_CUST_MT6739_H_
#define _MSDC_CUST_MT6739_H_

#include <dt-bindings/mmc/mt6739-msdc.h>
#include <dt-bindings/clock/mt6739-clk.h>

#include <mtk_spm_resource_req.h>

/**************************************************************/
/* Section 1: Device Tree                                     */
/**************************************************************/
/* Names used for device tree lookup */
#define DT_COMPATIBLE_NAME      "mediatek,msdc"
#define MSDC0_CLK_NAME          "msdc0-clock"
#define MSDC0_HCLK_NAME         "msdc0-hclock"
#define MSDC1_CLK_NAME          "msdc1-clock"
#define MSDC1_HCLK_NAME         "msdc1-hclock"
#define MSDC0_IOCFG_NAME        "mediatek,io_cfg_lt"
#define MSDC1_IOCFG_NAME        "mediatek,io_cfg_lb"


/**************************************************************/
/* Section 2: Power                                           */
/**************************************************************/
#define POWER_READY
#ifdef POWER_READY
#if !defined(FPGA_PLATFORM)
#include <mt-plat/upmu_common.h>

#define REG_VEMC_VOSEL_CAL      PMIC_RG_VEMC_VOCAL_ADDR
#define REG_VEMC_VOSEL          PMIC_RG_VEMC_VOSEL_ADDR
#define REG_VEMC_EN             PMIC_RG_LDO_VEMC_EN_ADDR /* TBC */

#define REG_VMC_VOSEL_CAL       PMIC_RG_VMC_VOCAL_ADDR
#define REG_VMC_VOSEL           PMIC_RG_VMC_VOSEL_ADDR
#define REG_VMC_EN              PMIC_RG_LDO_VMC_EN_ADDR /* TBC */
#define REG_VMCH_VOSEL_CAL      PMIC_RG_VMCH_VOCAL_ADDR
#define REG_VMCH_VOSEL          PMIC_RG_VMCH_VOSEL_ADDR
#define REG_VMCH_EN             PMIC_RG_LDO_VMCH_EN_ADDR /* TBC */

#define MASK_VEMC_VOSEL_CAL     PMIC_RG_VEMC_VOCAL_MASK
#define SHIFT_VEMC_VOSEL_CAL    PMIC_RG_VEMC_VOCAL_SHIFT
#define FIELD_VEMC_VOSEL_CAL    (MASK_VEMC_VOSEL_CAL \
					<< SHIFT_VEMC_VOSEL_CAL)

#define MASK_VEMC_VOSEL         PMIC_RG_VEMC_VOSEL_MASK
#define SHIFT_VEMC_VOSEL        PMIC_RG_VEMC_VOSEL_SHIFT
#define FIELD_VEMC_VOSEL        (MASK_VEMC_VOSEL << SHIFT_VEMC_VOSEL)

#define MASK_VEMC_EN            PMIC_RG_LDO_VEMC_EN_MASK
#define SHIFT_VEMC_EN           PMIC_RG_LDO_VEMC_EN_SHIFT
#define FIELD_VEMC_EN           (MASK_VEMC_EN << SHIFT_VEMC_EN)

#define MASK_VMC_VOSEL_CAL      PMIC_RG_VMC_VOCAL_MASK
#define SHIFT_VMC_VOSEL_CAL     PMIC_RG_VMC_VOCAL_SHIFT
#define FIELD_VMC_VOSEL_CAL     (MASK_VMC_VOSEL_CAL \
					<< SHIFT_VMC_VOSEL_CAL)

#define MASK_VMC_VOSEL          PMIC_RG_VMC_VOSEL_MASK
#define SHIFT_VMC_VOSEL         PMIC_RG_VMC_VOSEL_SHIFT
#define FIELD_VMC_VOSEL         (MASK_VMC_VOSEL << SHIFT_VMC_VOSEL)

#define MASK_VMC_EN             PMIC_RG_LDO_VMC_EN_MASK
#define SHIFT_VMC_EN            PMIC_RG_LDO_VMC_EN_SHIFT
#define FIELD_VMC_EN            (MASK_VMC_EN << SHIFT_VMC_EN)

#define MASK_VMCH_VOSEL_CAL     PMIC_RG_VMCH_VOCAL_MASK
#define SHIFT_VMCH_VOSEL_CAL    PMIC_RG_VMCH_VOCAL_SHIFT
#define FIELD_VMCH_VOSEL_CAL    (MASK_VMCH_VOSEL_CAL \
					<< SHIFT_VMCH_VOSEL_CAL)

#define MASK_VMCH_VOSEL         PMIC_RG_VMCH_VOSEL_MASK
#define SHIFT_VMCH_VOSEL        PMIC_RG_VMCH_VOSEL_SHIFT
#define FIELD_VMCH_VOSEL        (MASK_VMCH_VOSEL << SHIFT_VMCH_VOSEL)

#define MASK_VMCH_EN            PMIC_RG_LDO_VMCH_EN_MASK
#define SHIFT_VMCH_EN           PMIC_RG_LDO_VMCH_EN_SHIFT
#define FIELD_VMCH_EN           (MASK_VMCH_EN << SHIFT_VMCH_EN)

#define REG_VMCH_OC_RAW_STATUS      PMIC_RG_INT_RAW_STATUS_VMCH_OC_ADDR
#define MASK_VMCH_OC_RAW_STATUS     PMIC_RG_INT_RAW_STATUS_VMCH_OC_MASK
#define SHIFT_VMCH_OC_RAW_STATUS    PMIC_RG_INT_RAW_STATUS_VMCH_OC_SHIFT
#define FIELD_VMCH_OC_RAW_STATUS    (MASK_VMCH_OC_RAW_STATUS << SHIFT_VMCH_OC_RAW_STATUS)

#define REG_VMCH_OC_STATUS      PMIC_RG_INT_STATUS_VMCH_OC_ADDR
#define MASK_VMCH_OC_STATUS     PMIC_RG_INT_STATUS_VMCH_OC_MASK
#define SHIFT_VMCH_OC_STATUS    PMIC_RG_INT_STATUS_VMCH_OC_SHIFT
#define FIELD_VMCH_OC_STATUS    (MASK_VMCH_OC_STATUS << SHIFT_VMCH_OC_STATUS)

#define REG_VMCH_OC_EN      PMIC_RG_INT_EN_VMCH_OC_ADDR
#define MASK_VMCH_OC_EN     PMIC_RG_INT_EN_VMCH_OC_MASK
#define SHIFT_VMCH_OC_EN    PMIC_RG_INT_EN_VMCH_OC_SHIFT
#define FIELD_VMCH_OC_EN    (MASK_VMCH_OC_EN << SHIFT_VMCH_OC_EN)

#define REG_VMCH_OC_MASK      PMIC_RG_INT_MASK_VMCH_OC_ADDR
#define MASK_VMCH_OC_MASK     PMIC_RG_INT_MASK_VMCH_OC_MASK
#define SHIFT_VMCH_OC_MASK    PMIC_RG_INT_MASK_VMCH_OC_SHIFT
#define FIELD_VMCH_OC_MASK    (MASK_VMCH_OC_MASK << SHIFT_VMCH_OC_MASK)

#define VEMC_VOSEL_CAL_mV(cal)  ((cal >= 0) ? ((cal)/10) : 0)
#define VEMC_VOSEL_2V9          (0x2)
#define VEMC_VOSEL_3V           (0x3)
#define VEMC_VOSEL_3V3          (0x5)
#define VMC_VOSEL_CAL_mV(cal)   ((cal >= 0) ? ((cal)/10) : 0)
#define VMC_VOSEL_1V8           (0x4)
#define VMC_VOSEL_2V9           (0xa)
#define VMC_VOSEL_3V            (0xb)
#define VMC_VOSEL_3V3           (0xd)
#define VMCH_VOSEL_CAL_mV(cal)  ((cal >= 0) ? ((cal)/10) : 0)
#define VMCH_VOSEL_2V9          (0x2)
#define VMCH_VOSEL_3V           (0x3)
#define VMCH_VOSEL_3V3          (0x5)

#define REG_VCORE_VOSEL         0x1522
#define MASK_VCORE_VOSEL        0x7F
#define SHIFT_VCORE_VOSEL       0
#define VCORE_MIN_UV            518750
#define VCORE_STEP_UV           6250

#define REG_VIO18_VOCAL         0x1C4C
#define MASK_VIO18_VOCAL        0xF
#define SHIFT_VIO18_VOCAL       0
#define VIO18_VOCAL_mV(cal)     ((cal >= 0) ? ((cal)/10) : 0)
#define VIO18_VOSEL_1V8         (0xc)
/* Note: 6357 does not support 1.7V VIO18 */

#endif
#endif /* POWER_READY */

#define SD_POWER_DEFAULT_ON     (0)
#define EMMC_VOL_ACTUAL         VOL_3000
#define SD_VOL_ACTUAL           VOL_3000
#define SD1V8_VOL_ACTUAL        VOL_1860


/**************************************************************/
/* Section 3: Clock                                           */
/**************************************************************/
#if !defined(FPGA_PLATFORM)
/* MSDCPLL register offset */
#define MSDCPLL_CON0_OFFSET     (0x250)
#define MSDCPLL_CON1_OFFSET     (0x254)
#define MSDCPLL_CON2_OFFSET     (0x258)
#define MSDCPLL_PWR_CON0_OFFSET (0x25c)
#endif

#define MSDCPLL_FREQ            400000000

#define MSDC0_SRC_0             260000
#define MSDC0_SRC_1             MSDCPLL_FREQ
#define MSDC0_SRC_2             182000000
#define MSDC0_SRC_3             78000000
#define MSDC0_SRC_4             312000000
#define MSDC0_SRC_5             273000000
#define MSDC0_SRC_6             249600000
#define MSDC0_SRC_7             156000000

#define MSDC1_SRC_0             260000
#define MSDC1_SRC_1             (MSDCPLL_FREQ/2)
#define MSDC1_SRC_2             208000000
#define MSDC1_SRC_3             182000000
#define MSDC1_SRC_4             136500000
#define MSDC1_SRC_5             156000000
#define MSDC1_SRC_6             48000000
#define MSDC1_SRC_7             91000000

#define MSDC_SRC_FPGA           12000000

/* FIX ME */
#define CLOCK_READY
#ifdef CLOCK_READY
#define MSDC0_CG_NAME           MTK_CG_PERI2_RG_MSDC0_CK_PDN_AP_NORM_STA
#define MSDC1_CG_NAME           MTK_CG_PERI2_RG_MSDC1_CK_PDN_STA
#endif


/**************************************************************/
/* Section 4: GPIO and Pad                                    */
/**************************************************************/
/*--------------------------------------------------------------------------*/
/* MSDC0~1 GPIO and IO Pad Configuration Base                               */
/*--------------------------------------------------------------------------*/
#define MSDC_GPIO_BASE          gpio_base               /* 0x10005000 */
#define MSDC0_IO_CFG_BASE       (msdc_io_cfg_bases[0])  /* 0x10002000 */
#define MSDC1_IO_CFG_BASE       (msdc_io_cfg_bases[1])  /* 0x10002400 */

/*--------------------------------------------------------------------------*/
/* MSDC GPIO Related Register                                               */
/*--------------------------------------------------------------------------*/
/* MSDC0 */
#define MSDC0_GPIO_MODE4        (MSDC_GPIO_BASE + 0x340)
#define MSDC0_GPIO_MODE5        (MSDC_GPIO_BASE + 0x350)
#define MSDC0_GPIO_MODE6        (MSDC_GPIO_BASE + 0x360)
#define MSDC0_GPIO_DRV0_ADDR    (MSDC0_IO_CFG_BASE + 0x0)
#define MSDC0_GPIO_DRV1_ADDR    (MSDC0_IO_CFG_BASE + 0x10)
#define MSDC0_GPIO_IES_ADDR     (MSDC0_IO_CFG_BASE + 0x30)
#define MSDC0_GPIO_PUPD_ADDR    (MSDC0_IO_CFG_BASE + 0x40)
#define MSDC0_GPIO_R0_ADDR      (MSDC0_IO_CFG_BASE + 0x50)
#define MSDC0_GPIO_R1_ADDR      (MSDC0_IO_CFG_BASE + 0x60)
#define MSDC0_GPIO_RDSEL0_ADDR  (MSDC0_IO_CFG_BASE + 0x70)
#define MSDC0_GPIO_RDSEL1_ADDR  (MSDC0_IO_CFG_BASE + 0x80)
#define MSDC0_GPIO_SMT_ADDR     (MSDC0_IO_CFG_BASE + 0x90)
#define MSDC0_GPIO_SR_ADDR      (MSDC1_IO_CFG_BASE + 0xA0)
#define MSDC0_GPIO_TDSEL_ADDR   (MSDC0_IO_CFG_BASE + 0xB0)

/* MSDC1 */
#define MSDC1_GPIO_MODE8        (MSDC_GPIO_BASE + 0x380)
#define MSDC1_GPIO_MODE9        (MSDC_GPIO_BASE + 0x390)
#define MSDC1_GPIO_DRV_ADDR     (MSDC1_IO_CFG_BASE + 0x0)
#define MSDC1_GPIO_IES_ADDR     (MSDC1_IO_CFG_BASE + 0x20)
#define MSDC1_GPIO_PUPD_ADDR    (MSDC1_IO_CFG_BASE + 0x30)
#define MSDC1_GPIO_R0_ADDR      (MSDC1_IO_CFG_BASE + 0x50)
#define MSDC1_GPIO_R1_ADDR      (MSDC1_IO_CFG_BASE + 0x60)
#define MSDC1_GPIO_RDSEL_ADDR   (MSDC1_IO_CFG_BASE + 0x70)
#define MSDC1_GPIO_SMT_ADDR     (MSDC1_IO_CFG_BASE + 0x90)
#define MSDC1_GPIO_SR_ADDR      (MSDC1_IO_CFG_BASE + 0xA0)
#define MSDC1_GPIO_TDSEL_ADDR   (MSDC1_IO_CFG_BASE + 0xB0)


/*
 * MSDC0 GPIO and PAD register and bitfields definition
 */
/* MSDC0_GPIO_MODE4, 001b is msdc mode */
#define MSDC0_MODE_CMD_MASK     (0x7 << 24)
#define MSDC0_MODE_DSL_MASK     (0x7 << 28)
/* MSDC0_GPIO_MODE5, 001b is msdc mode*/
#define MSDC0_MODE_CLK_MASK     (0x7 << 0)
#define MSDC0_MODE_DAT0_MASK    (0x7 << 4)
#define MSDC0_MODE_DAT1_MASK    (0x7 << 8)
#define MSDC0_MODE_DAT2_MASK    (0x7 << 12)
#define MSDC0_MODE_DAT3_MASK    (0x7 << 16)
#define MSDC0_MODE_DAT4_MASK    (0x7 << 20)
#define MSDC0_MODE_DAT5_MASK    (0x7 << 24)
#define MSDC0_MODE_DAT6_MASK    (0x7 << 28)
/* MSDC0_GPIO_MODE6, 001b is msdc mode */
#define MSDC0_MODE_DAT7_MASK    (0x7 << 0)
#define MSDC0_MODE_RSTB_MASK    (0x7 << 4)

/* MSDC0 IES mask*/
#define MSDC0_IES_CLK_MASK      (0x1  <<  6)
#define MSDC0_IES_CMD_MASK      (0x1  <<  7)
#define MSDC0_IES_DAT_MASK      (0xff <<  8)
#define MSDC0_IES_DAT0_MASK     (0x1  <<  8)
#define MSDC0_IES_DAT1_MASK     (0x1  <<  9)
#define MSDC0_IES_DAT2_MASK     (0x1  << 10)
#define MSDC0_IES_DAT3_MASK     (0x1  << 11)
#define MSDC0_IES_DAT4_MASK     (0x1  << 12)
#define MSDC0_IES_DAT5_MASK     (0x1  << 13)
#define MSDC0_IES_DAT6_MASK     (0x1  << 14)
#define MSDC0_IES_DAT7_MASK     (0x1  << 15)
#define MSDC0_IES_DSL_MASK      (0x1  <<  16)
#define MSDC0_IES_RSTB_MASK     (0x1  <<  17)
#define MSDC0_IES_ALL_MASK      (0xfff <<  6)
/* MSDC0 SMT mask*/
#define MSDC0_SMT_CLK_MASK      (0x1 <<   1)
#define MSDC0_SMT_CMD_MASK      (0x1 <<   2)
#define MSDC0_SMT_DAT_MASK      (0x1 <<   3)
#define MSDC0_SMT_DSL_MASK      (0x1 <<   4)
#define MSDC0_SMT_RSTB_MASK     (0x1 <<   5)
#define MSDC0_SMT_ALL_MASK      (0x1f <<  1)
/* MSDC0 TDSEL mask*/
#define MSDC0_TDSEL_CLK_MASK    (0xf  <<   4)
#define MSDC0_TDSEL_CMD_MASK    (0xf  <<   8)
#define MSDC0_TDSEL_DAT_MASK    (0xf  <<  12)
#define MSDC0_TDSEL_DSL_MASK    (0xf  <<  16)
#define MSDC0_TDSEL_RSTB_MASK   (0xf  <<  20)
#define MSDC0_TDSEL_ALL_MASK    (0xfffff << 4)
/* MSDC0 RDSEL mask*/
#define MSDC0_RDSEL_CLK_MASK    (0x3f <<  6)
#define MSDC0_RDSEL_CMD_MASK    (0x3f << 12)
#define MSDC0_RDSEL_DAT_MASK    (0x3f << 18)
#define MSDC0_RDSEL_DSL_MASK    (0x3f << 24)
#define MSDC0_RDSEL0_ALL_MASK   (0xffffff << 6)
#define MSDC0_RDSEL_RSTB_MASK   (0x3f <<  0)
#define MSDC0_RDSEL1_ALL_MASK   (0x3f <<  0)
/* MSDC0 SR mask*/
#define MSDC0_SR_CLK_MASK       (0x1  <<  6)
#define MSDC0_SR_CMD_MASK       (0x1  <<  7)
#define MSDC0_SR_DAT_MASK       (0xff <<  8)
#define MSDC0_SR_DAT0_MASK      (0x1  <<  8)
#define MSDC0_SR_DAT1_MASK      (0x1  <<  9)
#define MSDC0_SR_DAT2_MASK      (0x1  << 10)
#define MSDC0_SR_DAT3_MASK      (0x1  << 11)
#define MSDC0_SR_DAT4_MASK      (0x1  << 12)
#define MSDC0_SR_DAT5_MASK      (0x1  << 13)
#define MSDC0_SR_DAT6_MASK      (0x1  << 14)
#define MSDC0_SR_DAT7_MASK      (0x1  << 15)
#define MSDC0_SR_DSL_MASK       (0x1  <<  16)
#define MSDC0_SR_RSTB_MASK      (0x1  <<  17)
#define MSDC0_SR_ALL_MASK       (0xfff <<  6)
/* MSDC0 DRV mask*/
#define MSDC0_DRV_CMD_MASK      (0x7  << 18)
#define MSDC0_DRV_CLK_MASK      (0x7  << 21)
#define MSDC0_DRV_DAT0_MASK     (0x7  << 24)
#define MSDC0_DRV_DAT1_MASK     (0x7  << 27)
#define MSDC0_DRV0_ALL_MASK     (0xfff << 18)
#define MSDC0_DRV_DAT2_MASK     (0x7  <<  0)
#define MSDC0_DRV_DAT3_MASK     (0x7  <<  3)
#define MSDC0_DRV_DAT4_MASK     (0x7  <<  6)
#define MSDC0_DRV_DAT5_MASK     (0x7  <<  9)
#define MSDC0_DRV_DAT6_MASK     (0x7  << 12)
#define MSDC0_DRV_DAT7_MASK     (0x7  << 15)
#define MSDC0_DRV_DSL_MASK      (0x7  << 18)
#define MSDC0_DRV_RSTB_MASK     (0x7  <<  21)
#define MSDC0_DRV1_ALL_MASK     (0xffffff << 0)
/* MSDC0 PUPD mask */
#define MSDC0_PUPD_CLK_MASK     (0x1  <<  6)
#define MSDC0_PUPD_CMD_MASK     (0x1  <<  7)
#define MSDC0_PUPD_DAT_MASK     (0xff <<  8)
#define MSDC0_PUPD_DAT0_MASK    (0x1  <<  8)
#define MSDC0_PUPD_DAT1_MASK    (0x1  <<  9)
#define MSDC0_PUPD_DAT2_MASK    (0x1  << 10)
#define MSDC0_PUPD_DAT3_MASK    (0x1  << 11)
#define MSDC0_PUPD_DAT4_MASK    (0x1  << 12)
#define MSDC0_PUPD_DAT5_MASK    (0x1  << 13)
#define MSDC0_PUPD_DAT6_MASK    (0x1  << 14)
#define MSDC0_PUPD_DAT7_MASK    (0x1  << 15)
#define MSDC0_PUPD_DSL_MASK     (0x7  << 16)
#define MSDC0_PUPD_RSTB_MASK    (0x7  << 17)
#define MSDC0_PUPD_MASK_WITH_RSTB       (0xFFF << 6)
#define MSDC0_PUPD_ALL_MASK     (0x7FF << 6)
/* MSDC0 R0 mask */
#define MSDC0_R0_CLK_MASK       (0x1  <<  6)
#define MSDC0_R0_CMD_MASK       (0x1  <<  7)
#define MSDC0_R0_DAT_MASK       (0xff <<  8)
#define MSDC0_R0_DAT0_MASK      (0x1  <<  8)
#define MSDC0_R0_DAT1_MASK      (0x1  <<  9)
#define MSDC0_R0_DAT2_MASK      (0x1  << 10)
#define MSDC0_R0_DAT3_MASK      (0x1  << 11)
#define MSDC0_R0_DAT4_MASK      (0x1  << 12)
#define MSDC0_R0_DAT5_MASK      (0x1  << 13)
#define MSDC0_R0_DAT6_MASK      (0x1  << 14)
#define MSDC0_R0_DAT7_MASK      (0x1  << 15)
#define MSDC0_R0_DSL_MASK       (0x7  << 16)
#define MSDC0_R0_RSTB_MASK      (0x7  << 17)
#define MSDC0_R0_MASK_WITH_RSTB (0xFFF << 6)
#define MSDC0_R0_ALL_MASK       (0x7FF << 6)
/* MSDC0 R1 mask */
#define MSDC0_R1_CLK_MASK       (0x1  <<  6)
#define MSDC0_R1_CMD_MASK       (0x1  <<  7)
#define MSDC0_R1_DAT_MASK       (0xff <<  8)
#define MSDC0_R1_DAT0_MASK      (0x1  <<  8)
#define MSDC0_R1_DAT1_MASK      (0x1  <<  9)
#define MSDC0_R1_DAT2_MASK      (0x1  << 10)
#define MSDC0_R1_DAT3_MASK      (0x1  << 11)
#define MSDC0_R1_DAT4_MASK      (0x1  << 12)
#define MSDC0_R1_DAT5_MASK      (0x1  << 13)
#define MSDC0_R1_DAT6_MASK      (0x1  << 14)
#define MSDC0_R1_DAT7_MASK      (0x1  << 15)
#define MSDC0_R1_DSL_MASK       (0x7  << 16)
#define MSDC0_R1_RSTB_MASK      (0x7  << 17)
#define MSDC0_R1_MASK_WITH_RSTB (0xFFF << 6)
#define MSDC0_R1_ALL_MASK       (0x7FF << 6)


/*
 * MSDC1 GPIO and PAD register and bitfields definition
 */
/* MSDC1_GPIO_MODE8, 0001b is msdc mode */
#define MSDC1_MODE_CMD_MASK     (0x7 << 28)
/* MSDC1_GPIO_MODE9, 0001b is msdc mode */
#define MSDC1_MODE_CLK_MASK     (0x7 << 0)
#define MSDC1_MODE_DAT0_MASK    (0x7 << 4)
#define MSDC1_MODE_DAT1_MASK    (0x7 << 8)
#define MSDC1_MODE_DAT2_MASK    (0x7 << 12)
#define MSDC1_MODE_DAT3_MASK    (0x7 << 16)

/* MSDC1 IES mask*/
#define MSDC1_IES_CLK_MASK      (0x1 <<  2)
#define MSDC1_IES_CMD_MASK      (0x1 <<  3)
#define MSDC1_IES_DAT_MASK      (0xf <<  4)
#define MSDC1_IES_DAT0_MASK     (0x1 <<  4)
#define MSDC1_IES_DAT1_MASK     (0x1 <<  5)
#define MSDC1_IES_DAT2_MASK     (0x1 <<  6)
#define MSDC1_IES_DAT3_MASK     (0x1 <<  7)
#define MSDC1_IES_ALL_MASK      (0x3f <<  2)
/* MSDC1 SMT mask*/
#define MSDC1_SMT_CLK_MASK      (0x1 <<  1)
#define MSDC1_SMT_CMD_MASK      (0x1 <<  2)
#define MSDC1_SMT_DAT_MASK      (0x1 <<  3)
#define MSDC1_SMT_ALL_MASK      (0x7 <<  1)
/* MSDC1 TDSEL mask*/
#define MSDC1_TDSEL_DAT_MASK    (0xf <<  4)
#define MSDC1_TDSEL_CMD_MASK    (0xf <<  8)
#define MSDC1_TDSEL_CLK_MASK    (0xf << 12)
#define MSDC1_TDSEL_ALL_MASK    (0xfff << 4)
/* MSDC1 RDSEL mask*/
#define MSDC1_RDSEL_CLK_MASK    (0x3f <<  6)
#define MSDC1_RDSEL_CMD_MASK    (0x3f << 12)
#define MSDC1_RDSEL_DAT_MASK    (0x3f << 18)
#define MSDC1_RDSEL_ALL_MASK    (0x3ffff << 6)
/* MSDC1 SR mask*/
#define MSDC1_SR_CLK_MASK       (0x1 << 2)
#define MSDC1_SR_CMD_MASK       (0x1 << 3)
#define MSDC1_SR_DAT_MASK       (0x1 << 4)
#define MSDC1_SR_DAT0_MASK      (0x1 << 4)
#define MSDC1_SR_DAT1_MASK      (0x1 << 5)
#define MSDC1_SR_DAT2_MASK      (0x1 << 6)
#define MSDC1_SR_DAT3_MASK      (0x1 << 7)
#define MSDC1_SR_ALL_MASK       (0x3f << 2)
/* MSDC1 DRV mask*/
#define MSDC1_DRV_CLK_MASK      (0x7 <<  3)
#define MSDC1_DRV_CMD_MASK      (0x7 <<  6)
#define MSDC1_DRV_DAT_MASK      (0x7 <<  9)
#define MSDC1_DRV_ALL_MASK      (0x1ff << 3)
/* MSDC1 PUPD mask */
#define MSDC1_PUPD_CLK_MASK     (0x7 << 2)
#define MSDC1_PUPD_CMD_MASK     (0x7 << 3)
#define MSDC1_PUPD_DAT0_MASK    (0x7 << 4)
#define MSDC1_PUPD_DAT1_MASK    (0x7 << 5)
#define MSDC1_PUPD_DAT2_MASK    (0x7 << 6)
#define MSDC1_PUPD_DAT3_MASK    (0x7 << 7)
#define MSDC1_PUPD_ALL_MASK     (0x3F << 2)
/* MSDC1 R0 mask */
#define MSDC1_R0_CLK_MASK       (0x7 << 2)
#define MSDC1_R0_CMD_MASK       (0x7 << 3)
#define MSDC1_R0_DAT0_MASK      (0x7 << 4)
#define MSDC1_R0_DAT1_MASK      (0x7 << 5)
#define MSDC1_R0_DAT2_MASK      (0x7 << 6)
#define MSDC1_R0_DAT3_MASK      (0x7 << 7)
#define MSDC1_R0_ALL_MASK       (0x3F << 2)
/* MSDC1 R1 mask */
#define MSDC1_R1_CLK_MASK       (0x7 << 2)
#define MSDC1_R1_CMD_MASK       (0x7 << 3)
#define MSDC1_R1_DAT0_MASK      (0x7 << 4)
#define MSDC1_R1_DAT1_MASK      (0x7 << 5)
#define MSDC1_R1_DAT2_MASK      (0x7 << 6)
#define MSDC1_R1_DAT3_MASK      (0x7 << 7)
#define MSDC1_R1_ALL_MASK       (0x3F << 2)


/* FOR msdc_io_check() */
#define MSDC1_PUPD_DAT0_ADDR    (MSDC1_GPIO_PUPD_ADDR)
#define MSDC1_PUPD_DAT1_ADDR    (MSDC1_GPIO_PUPD_ADDR)
#define MSDC1_PUPD_DAT2_ADDR    (MSDC1_GPIO_PUPD_ADDR)


/**************************************************************/
/* Section 5: Adjustable Driver Parameter                     */
/**************************************************************/
#define HOST_MAX_BLKSZ          (2048)

#define MSDC_OCR_AVAIL          (MMC_VDD_28_29 | MMC_VDD_29_30 | MMC_VDD_30_31 | MMC_VDD_31_32 | MMC_VDD_32_33)
/* data timeout counter. 1048576 * 3 sclk. */
#define DEFAULT_DTOC            (3)

#define MAX_DMA_CNT             (4 * 1024 * 1024)
/* a WIFI transaction may be 50K */
#define MAX_DMA_CNT_SDIO        (0xFFFFFFFF - 255)
/* a LTE  transaction may be 128K */

#define MAX_HW_SGMTS            (MAX_BD_NUM)
#define MAX_PHY_SGMTS           (MAX_BD_NUM)
#define MAX_SGMT_SZ             (MAX_DMA_CNT)
#define MAX_SGMT_SZ_SDIO        (MAX_DMA_CNT_SDIO)

#define HOST_MAX_NUM            (2)
#ifdef CONFIG_PWR_LOSS_MTK_TEST
#define MAX_REQ_SZ              (512 * 65536)
#else
#define MAX_REQ_SZ              (512 * 1024)
#endif

#ifdef FPGA_PLATFORM
#define HOST_MAX_MCLK           (200000000)
#else
#define HOST_MAX_MCLK           (200000000)
#endif
#define HOST_MIN_MCLK           (260000)

/* SD card, bad card handling settings */

/* if continuous data timeout reach the limit */
/* driver will force remove card */
#define MSDC_MAX_DATA_TIMEOUT_CONTINUOUS (100)

/* if continuous power cycle fail reach the limit */
/* driver will force remove card */
#define MSDC_MAX_POWER_CYCLE_FAIL_CONTINUOUS (3)

/* sdcard esd recovery */
/* power reset sdcard when sdcard hang from esd */
#define SDCARD_ESD_RECOVERY

/* #define MSDC_HQA */
/* #define SDIO_HQA */

/**************************************************************/
/* Section 6: BBChip-depenent Tunnig Parameter                */
/**************************************************************/
#define EMMC_MAX_FREQ_DIV               4 /* lower frequence to 12.5M */
#define MSDC_CLKTXDLY                   0

#define MSDC0_DDR50_DDRCKD              1 /* FIX ME: may be removed */

#define VOL_CHG_CNT_DEFAULT_VAL         0x1F4 /* =500 */

#define MSDC_PB0_DEFAULT_VAL            0x403C0007
#define MSDC_PB1_DEFAULT_VAL            0xFFE60349

#define MSDC_PB2_DEFAULT_RESPWAITCNT    0x3
#define MSDC_PB2_DEFAULT_RESPSTENSEL    0x1
#define MSDC_PB2_DEFAULT_CRCSTSENSEL    0x1

#if defined(FPGA_PLATFORM)
#undef MSDC_PB2_DEFAULT_CRCSTSENSEL
#define MSDC_PB2_DEFAULT_CRCSTSENSEL    0
#endif

#define EMMC50_CFG_END_BIT_CHK_CNT      0x3

/**************************************************************/
/* Section 7: SDIO host                                       */
/**************************************************************/
#ifdef CONFIG_MTK_COMBO_COMM
#include <mt-plat/mtk_wcn_cmb_stub.h>
#define CFG_DEV_SDIO                    3
#endif

/**************************************************************/
/* Section 8: ECO Variation                                   */
/**************************************************************/
#if !defined(FPGA_PLATFORM)
#include <mt-plat/mtk_chip.h>
#else
#define mt_get_chip_hw_ver()            0
/* #define CHIP_SW_VER_01                  0 */
#endif

/* FIXME: check if the following lines are used on MT6739 */
#define ENABLE_HW_DVFS_WITH_CLK_OFF() \
	MSDC_SET_BIT32(MSDC_CFG, MSDC_CFG_DVFS_IDLE)
#define DISABLE_HW_DVFS_WITH_CLK_OFF() \
	MSDC_CLR_BIT32(MSDC_CFG, MSDC_CFG_DVFS_IDLE)

#define ENABLE_SDC_RX_ENH() \
	MSDC_SET_BIT32(SDC_ADV_CFG0, SDC_ADV_CFG0_SDC_RX_ENH_EN)
#define DISABLE_SDC_RX_ENH() \
	MSDC_CLR_BIT32(SDC_ADV_CFG0, SDC_ADV_CFG0_SDC_RX_ENH_EN)

#define SET_EMMC50_CFG_END_BIT_CHK_CNT(CNT) \
	MSDC_SET_FIELD(EMMC50_CFG0, MSDC_EMMC50_CFG_END_BIT_CHK_CNT, CNT)

#endif /* _MSDC_CUST_MT6739_H_ */
