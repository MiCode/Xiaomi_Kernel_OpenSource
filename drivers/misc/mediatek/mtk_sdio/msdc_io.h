/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MSDC_IO_H_
#define _MSDC_IO_H_

/**************************************************************/
/* Section 1: Device Tree                                     */
/**************************************************************/
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include "mt_sd.h"

extern const struct of_device_id msdc_of_ids[];
extern unsigned int cd_gpio;

extern void __iomem *gpio_base;
extern void __iomem *infracfg_ao_reg_base;
extern void __iomem *pericfg_reg_base;
extern void __iomem *apmixed_reg_base;
extern void __iomem *topckgen_reg_base;

int msdc_dt_init(struct platform_device *pdev, struct mmc_host *mmc);

#ifdef FPGA_PLATFORM
void msdc_fpga_pwr_init(void);
#endif

/* Names used for device tree lookup */
#define MT2701_DT_COMPATIBLE_NAME      "mediatek,mt2701-sdio"
#define MT2712_DT_COMPATIBLE_NAME      "mediatek,mt2712-sdio"
#define MSDC0_CLK_NAME          "MSDC0-CLOCK"
#define MSDC1_CLK_NAME          "MSDC1-CLOCK"
#define MSDC2_CLK_NAME          "sdio-clock"
#define MSDC3_CLK_NAME          "sdio-clock"
#define MSDC0_IOCFG_NAME        "mediatek,iocfg_5"
#define MSDC1_IOCFG_NAME        "mediatek,iocfg_0"
#define MSDC2_IOCFG_NAME        "mediatek,iocfg_4"
#define MSDC3_IOCFG_NAME        "NOT EXIST"

/**************************************************************/
/* Section 2: Power                                           */
/**************************************************************/
#if 0
#define REG_VEMC_VOSEL_CAL              MT6351_PMIC_RG_VEMC_CAL_ADDR
#define REG_VEMC_VOSEL                  MT6351_PMIC_RG_VEMC_VOSEL_ADDR
#define REG_VEMC_EN                     MT6351_PMIC_RG_VEMC_EN_ADDR
#define REG_VMC_VOSEL_CAL               MT6351_PMIC_RG_VMC_CAL_ADDR
#define REG_VMC_VOSEL                   MT6351_PMIC_RG_VMC_VOSEL_ADDR
#define REG_VMC_EN                      MT6351_PMIC_RG_VMC_EN_ADDR
#define REG_VMCH_VOSEL_CAL              MT6351_PMIC_RG_VMCH_CAL_ADDR
#define REG_VMCH_VOSEL                  MT6351_PMIC_RG_VMCH_VOSEL_ADDR
#define REG_VMCH_EN                     MT6351_PMIC_RG_VMCH_EN_ADDR

#define MASK_VEMC_VOSEL_CAL             MT6351_PMIC_RG_VEMC_CAL_MASK
#define SHIFT_VEMC_VOSEL_CAL            MT6351_PMIC_RG_VEMC_CAL_SHIFT
#define FIELD_VEMC_VOSEL_CAL            (MASK_VEMC_VOSEL_CAL \
						<< SHIFT_VEMC_VOSEL_CAL)
#define MASK_VEMC_VOSEL                 MT6351_PMIC_RG_VEMC_VOSEL_MASK
#define SHIFT_VEMC_VOSEL                MT6351_PMIC_RG_VEMC_VOSEL_SHIFT
#define FIELD_VEMC_VOSEL                (MASK_VEMC_VOSEL << SHIFT_VEMC_VOSEL)
#define MASK_VEMC_EN                    MT6351_PMIC_RG_VEMC_EN_MASK
#define SHIFT_VEMC_EN                   MT6351_PMIC_RG_VEMC_EN_SHIFT
#define FIELD_VEMC_EN                   (MASK_VEMC_EN << SHIFT_VEMC_EN)
#define MASK_VMC_VOSEL_CAL              MT6351_PMIC_RG_VMC_CAL_MASK
#define SHIFT_VMC_VOSEL_CAL             MT6351_PMIC_RG_VMC_CAL_SHIFT
#define MASK_VMC_VOSEL                  MT6351_PMIC_RG_VMC_VOSEL_MASK
#define SHIFT_VMC_VOSEL                 MT6351_PMIC_RG_VMC_VOSEL_SHIFT
#define FIELD_VMC_VOSEL                 (MASK_VMC_VOSEL << SHIFT_VMC_VOSEL)
#define MASK_VMC_EN                     MT6351_PMIC_RG_VMC_EN_MASK
#define SHIFT_VMC_EN                    MT6351_PMIC_RG_VMC_EN_SHIFT
#define FIELD_VMC_EN                    (MASK_VMC_EN << SHIFT_VMC_EN)
#define MASK_VMCH_VOSEL_CAL             MT6351_PMIC_RG_VMCH_CAL_MASK
#define SHIFT_VMCH_VOSEL_CAL            MT6351_PMIC_RG_VMCH_CAL_SHIFT
#define FIELD_VMCH_VOSEL_CAL            (MASK_VMCH_VOSEL_CAL \
						<< SHIFT_VMCH_VOSEL_CAL)
#define MASK_VMCH_VOSEL                 MT6351_PMIC_RG_VMCH_VOSEL_MASK
#define SHIFT_VMCH_VOSEL                MT6351_PMIC_RG_VMCH_VOSEL_SHIFT
#define FIELD_VMCH_VOSEL                (MASK_VMCH_VOSEL << SHIFT_VMCH_VOSEL)
#define MASK_VMCH_EN                    MT6351_PMIC_RG_VMCH_EN_MASK
#define SHIFT_VMCH_EN                   MT6351_PMIC_RG_VMCH_EN_SHIFT
#define FIELD_VMCH_EN                   (MASK_VMCH_EN << SHIFT_VMCH_EN)

#define REG_VMCH_OC_STATUS              MT6351_PMIC_OC_STATUS_VMCH_ADDR
#define MASK_VMCH_OC_STATUS             MT6351_PMIC_OC_STATUS_VMCH_MASK
#define SHIFT_VMCH_OC_STATUS            MT6351_PMIC_OC_STATUS_VMCH_SHIFT
#define FIELD_VMCH_OC_STATUS            (MASK_VMCH_OC_STATUS \
						<< SHIFT_VMCH_OC_STATUS)

#define VEMC_VOSEL_CAL_mV(cal)          ((cal <= 0) ? \
						((0-(cal))/20) : (32-(cal)/20))
#define VEMC_VOSEL_3V                   (0)
#define VEMC_VOSEL_3V3                  (1)
#define VMC_VOSEL_1V8                   (3)
#define VMC_VOSEL_2V8                   (5)
#define VMC_VOSEL_3V                    (6)
#define VMC_VOSEL_CAL_mV(cal)          ((cal <= 0) ? \
						((0-(cal))/20) : (32-(cal)/20))
#define VMCH_VOSEL_CAL_mV(cal)          ((cal <= 0) ? \
						((0-(cal))/20) : (32-(cal)/20))
#define VMCH_VOSEL_3V                   (0)
#define VMCH_VOSEL_3V3                  (1)

#define REG_VCORE_VOSEL_SW              MT6351_PMIC_BUCK_VCORE_VOSEL_ADDR
#define VCORE_VOSEL_SW_MASK             MT6351_PMIC_BUCK_VCORE_VOSEL_MASK
#define VCORE_VOSEL_SW_SHIFT            MT6351_PMIC_BUCK_VCORE_VOSEL_SHIFT
#define REG_VCORE_VOSEL_HW              MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR
#define VCORE_VOSEL_HW_MASK             MT6351_PMIC_BUCK_VCORE_VOSEL_ON_MASK
#define VCORE_VOSEL_HW_SHIFT            MT6351_PMIC_BUCK_VCORE_VOSEL_ON_SHIFT
#define REG_VIO18_CAL                   MT6351_PMIC_RG_VIO18_CAL_ADDR
#define VIO18_CAL_MASK                  MT6351_PMIC_RG_VIO18_CAL_MASK
#define VIO18_CAL_SHIFT                 MT6351_PMIC_RG_VIO18_CAL_SHIFT
#endif
#define VOL_3000			3000
#define CARD_VOL_ACTUAL                 VOL_2900

void msdc_sd_power_switch(struct msdc_host *host, u32 on);

#if !defined(FPGA_PLATFORM)
void msdc_power_calibration_init(struct msdc_host *host);
void msdc_oc_check(struct msdc_host *host);
void msdc_emmc_power(struct msdc_host *host, u32 on);
void msdc_sd_power(struct msdc_host *host, u32 on);
void msdc_sdio_power(struct msdc_host *host, u32 on);
void msdc_sd_power_off(void);
void msdc_dump_ldo_sts(struct msdc_host *host);
void msdc_HQA_set_vcore(struct msdc_host *host);
int msdc_regulator_set_and_enable(struct regulator *reg, int powerVolt);
#endif

#ifdef FPGA_PLATFORM
void msdc_fpga_power(struct msdc_host *host, u32 on);
bool hwPowerOn_fpga(void);
bool hwPowerDown_fpga(void);
bool hwPowerDown_fpga(void);
#define msdc_emmc_power                 msdc_fpga_power
#define msdc_sd_power                   msdc_fpga_power
#define msdc_sdio_power                 msdc_fpga_power
#define msdc_sd_power_off()
#define msdc_dump_ldo_sts(host)
#endif

extern u32 g_msdc0_io;
extern u32 g_msdc0_flash;
extern u32 g_msdc1_io;
extern u32 g_msdc1_flash;
extern u32 g_msdc2_io;
extern u32 g_msdc2_flash;
extern u32 g_msdc3_io;
extern u32 g_msdc3_flash;

/**************************************************************/
/* Section 3: Clock                                           */
/**************************************************************/
#if defined(FPGA_PLATFORM)
#define msdc_dump_clock_sts()
extern u32 hclks_msdc[];
#define msdc_get_hclks(host)            hclks_msdc
#define msdc_clk_enable(host)
#define msdc_clk_disable(host)
#endif

#if !defined(FPGA_PLATFORM)
void msdc_dump_clock_sts(void);
/* define clock related register macro */
#define MSDCPLL_CON0_OFFSET             (0x250)
#define MSDCPLL_CON1_OFFSET             (0x254)
#define MSDCPLL_PWR_CON0_OFFSET         (0x25C)

#define MSDC_CLK_UPDATE_OFFSET          (0x004)
#define MSDC_CLK_CFG_3_OFFSET           (0x070)
#define MSDC_CLK_CFG_4_OFFSET           (0x080)

#define MSDC_INFRA_PDN_SET1_OFFSET      (0x0088)
#define MSDC_INFRA_PDN_CLR1_OFFSET      (0x008c)
#define MSDC_INFRA_PDN_STA1_OFFSET      (0x0094)

extern u32 *hclks_msdc_all[];
#define msdc_get_hclks(id) hclks_msdc_all[id]

int msdc_get_ccf_clk_pointer(struct platform_device *pdev,
	struct msdc_host *host);
#define msdc_clk_enable(host) clk_enable(host->clock_control)
#define msdc_clk_disable(host) clk_disable(host->clock_control)

#endif

#define MSDC0_SRC_0             260000
#define MSDC0_SRC_1             400000000
#define MSDC0_SRC_2             200000000
#define MSDC0_SRC_3             156000000
#define MSDC0_SRC_4             182000000
#define MSDC0_SRC_5             156000000
#define MSDC0_SRC_6             100000000
#define MSDC0_SRC_7             624000000
#define MSDC0_SRC_8             312000000

#define MSDC1_SRC_0             260000
#define MSDC1_SRC_1             208000000
#define MSDC1_SRC_2             200000000
#define MSDC1_SRC_3             156000000
#define MSDC1_SRC_4             182000000
#define MSDC1_SRC_5             156000000
#define MSDC1_SRC_6             178000000
#define MSDC1_SRC_7             100000000

#define MSDC3_SRC_0             260000
#define MSDC3_SRC_1             50000000
#define MSDC3_SRC_2             200000000
#define MSDC3_SRC_3             156000000
#define MSDC3_SRC_4             48000000
#define MSDC3_SRC_5             156000000
#define MSDC3_SRC_6             178000000
#define MSDC3_SRC_7             54000000
#define MSDC3_SRC_8             25000000

#if 0
#define MSDC0_CG_NAME           MT_CG_INFRA0_MSDC0_CG_STA
#define MSDC1_CG_NAME           MT_CG_INFRA0_MSDC1_CG_STA
#define MSDC2_CG_NAME           MT_CG_INFRA0_MSDC2_CG_STA
#define MSDC3_CG_NAME           MT_CG_INFRA0_MSDC3_CG_STA
#endif

/**************************************************************/
/* Section 4: GPIO and Pad                                    */
/**************************************************************/
/*******************************************************************************
 *PINMUX and GPIO definition
 ******************************************************************************/

#define MSDC_PIN_PULL_NONE      (0)
#define MSDC_PIN_PULL_DOWN      (1)
#define MSDC_PIN_PULL_UP        (2)
#define MSDC_PIN_KEEP           (3)

/* add pull down/up mode define */
#define MSDC_GPIO_PULL_UP       (0)
#define MSDC_GPIO_PULL_DOWN     (1)

#define MSDC_TDRDSEL_SLEEP      (0)
#define MSDC_TDRDSEL_3V         (1)
#define MSDC_TDRDSEL_1V8        (2)
#define MSDC_TDRDSEL_CUST       (3)

/*--------------------------------------------------------------------------*/
/* MSDC0~3 GPIO and IO Pad Configuration Base                               */
/*--------------------------------------------------------------------------*/
#define MSDC_GPIO_BASE          gpio_base
#define MSDC0_IO_PAD_BASE       (msdc_io_cfg_bases[0])
#define MSDC1_IO_PAD_BASE       (msdc_io_cfg_bases[1])
#define MSDC2_IO_PAD_BASE       (msdc_io_cfg_bases[2])
#define MSDC3_IO_PAD_BASE       (msdc_io_cfg_bases[3])

/*--------------------------------------------------------------------------*/
/* MSDC GPIO Related Register                                               */
/*--------------------------------------------------------------------------*/
#define MSDC0_GPIO_MODE18       (MSDC_GPIO_BASE + 0x410)
#define MSDC0_GPIO_MODE19       (MSDC_GPIO_BASE + 0x420)
#define MSDC0_GPIO_IES_ADDR     (MSDC0_IO_PAD_BASE + 0x000)
#define MSDC0_GPIO_SMT_ADDR     (MSDC0_IO_PAD_BASE + 0x010)
#define MSDC0_GPIO_TDSEL_ADDR   (MSDC0_IO_PAD_BASE + 0x020)
#define MSDC0_GPIO_RDSEL_ADDR   (MSDC0_IO_PAD_BASE + 0x040)
#define MSDC0_GPIO_SR_ADDR      (MSDC0_IO_PAD_BASE + 0x0a0)
#define MSDC0_GPIO_DRV_ADDR     (MSDC0_IO_PAD_BASE + 0x0a0)
#define MSDC0_GPIO_PUPD0_ADDR   (MSDC0_IO_PAD_BASE + 0x0c0)
#define MSDC0_GPIO_PUPD1_ADDR   (MSDC0_IO_PAD_BASE + 0x0d0)

#define MSDC1_GPIO_MODE4        (MSDC_GPIO_BASE + 0x330)
#define MSDC1_GPIO_IES_ADDR     (MSDC1_IO_PAD_BASE + 0x000)
#define MSDC1_GPIO_SMT_ADDR     (MSDC1_IO_PAD_BASE + 0x010)
#define MSDC1_GPIO_TDSEL_ADDR   (MSDC1_IO_PAD_BASE + 0x030)
#define MSDC1_GPIO_RDSEL0_ADDR  (MSDC1_IO_PAD_BASE + 0x040)
#define MSDC1_GPIO_RDSEL1_ADDR  (MSDC1_IO_PAD_BASE + 0x050)
#define MSDC1_GPIO_SR_ADDR      (MSDC1_IO_PAD_BASE + 0x0b0)
#define MSDC1_GPIO_DRV_ADDR     (MSDC1_IO_PAD_BASE + 0x0b0)
#define MSDC1_GPIO_PUPD_ADDR    (MSDC1_IO_PAD_BASE + 0x0c0)

#define MSDC2_GPIO_MODE14       (MSDC_GPIO_BASE + 0x3D0)
#define MSDC2_GPIO_IES_ADDR     (MSDC2_IO_PAD_BASE + 0x000)
#define MSDC2_GPIO_SMT_ADDR     (MSDC2_IO_PAD_BASE + 0x010)
#define MSDC2_GPIO_TDSEL_ADDR   (MSDC2_IO_PAD_BASE + 0x020)
#define MSDC2_GPIO_RDSEL_ADDR   (MSDC2_IO_PAD_BASE + 0x040)
#define MSDC2_GPIO_SR_ADDR      (MSDC2_IO_PAD_BASE + 0x0a0)
#define MSDC2_GPIO_DRV_ADDR     (MSDC2_IO_PAD_BASE + 0x0a0)
#define MSDC2_GPIO_PUPD_ADDR0   (MSDC2_IO_PAD_BASE + 0x0c0)
#define MSDC2_GPIO_PUPD_ADDR1   (MSDC2_IO_PAD_BASE + 0x0d0)

/*
 * MSDC0 GPIO and PAD register and bitfields definition
 */
/*MSDC0_GPIO_MODE19, Set as pinmux value as 1*/
#define MSDC0_MODE_DAT7_MASK            (7 << 25)
#define MSDC0_MODE_DAT6_MASK            (7 <<  9)
#define MSDC0_MODE_DAT5_MASK            (7 <<  6)
#define MSDC0_MODE_DAT4_MASK            (7 << 12)
#define MSDC0_MODE_DAT3_MASK            (7 << 22)
#define MSDC0_MODE_DAT2_MASK            (7 << 19)
#define MSDC0_MODE_DAT1_MASK            (7 <<  3)
#define MSDC0_MODE_CMD_MASK             (7 <<  0)
#define MSDC0_MODE_DSL_MASK             (7 << 28)
#define MSDC0_MODE_RST_MASK             (7 << 16)

/*MSDC0_GPIO_MODE18, Set as pinmux value as 1*/
#define MSDC0_MODE_DAT0_MASK            (7 << 25)
#define MSDC0_MODE_CLK_MASK             (7 << 28)

/* MSDC0 IES mask*/
#define MSDC0_IES_DAT_MASK              (0x1  <<  0)
#define MSDC0_IES_CLK_MASK              (0x1  <<  1)
#define MSDC0_IES_CMD_MASK              (0x1  <<  2)
#define MSDC0_IES_RSTB_MASK             (0x1  <<  3)
#define MSDC0_IES_DSL_MASK              (0x1  <<  4)
#define MSDC0_IES_ALL_MASK              (0x1F <<  0)

/* MSDC0 SMT mask*/
#define MSDC0_SMT_DAT_MASK              (0x1  <<  0)
#define MSDC0_SMT_CLK_MASK              (0x1  <<  1)
#define MSDC0_SMT_CMD_MASK              (0x1  <<  2)
#define MSDC0_SMT_RSTB_MASK             (0x1  <<  3)
#define MSDC0_SMT_DSL_MASK              (0x1  <<  4)
#define MSDC0_SMT_ALL_MASK              (0x1F <<  0)

/* MSDC0 TDSEL mask*/
#define MSDC0_TDSEL_DAT_MASK            (0xF  <<  0)
#define MSDC0_TDSEL_CLK_MASK            (0xF  <<  4)
#define MSDC0_TDSEL_CMD_MASK            (0xF  <<  8)
#define MSDC0_TDSEL_RSTB_MASK           (0xF  << 12)
#define MSDC0_TDSEL_DSL_MASK            (0xF  << 16)
#define MSDC0_TDSEL_ALL_MASK            (0xFFFFF << 0)

/* MSDC0 RDSEL mask*/
#define MSDC0_RDSEL_DAT_MASK            (0x3F <<  0)
#define MSDC0_RDSEL_CLK_MASK            (0x3F <<  6)
#define MSDC0_RDSEL_CMD_MASK            (0x3F << 12)
#define MSDC0_RDSEL_RSTB_MASK           (0x3F << 18)
#define MSDC0_RDSEL_DSL_MASK            (0x3F << 24)
#define MSDC0_RDSEL_ALL_MASK            (0x3FFFFFFF << 0)

/* MSDC0 SR mask*/
#define MSDC0_SR_DAT_MASK               (0x1  <<  3)
#define MSDC0_SR_CLK_MASK               (0x1  <<  7)
#define MSDC0_SR_CMD_MASK               (0x1  << 11)
#define MSDC0_SR_RSTB_MASK              (0x1  << 15)
#define MSDC0_SR_DSL_MASK               (0x1  << 19)
/*Attention: bits are not continuous, shall not define MSDC0_SR_ALL_MASK*/

/* MSDC0 DRV mask*/
#define MSDC0_DRV_DAT_MASK              (0x7  <<  0)
#define MSDC0_DRV_CLK_MASK              (0x7  <<  4)
#define MSDC0_DRV_CMD_MASK              (0x7  <<  8)
#define MSDC0_DRV_RSTB_MASK             (0x7  << 12)
#define MSDC0_DRV_DSL_MASK              (0x7  << 16)
/*Attention: bits are not continuous, shall not define MSDC0_DRV_ALL_MASK*/

/* MSDC0 PUPD mask*/
#define MSDC0_PUPD_DAT0_MASK            (0x7  <<  0)
#define MSDC0_PUPD_CLK_MASK             (0x7  <<  4)
#define MSDC0_PUPD_CMD_MASK             (0x7  <<  8)
#define MSDC0_PUPD_DAT1_MASK            (0x7  << 12)
#define MSDC0_PUPD_DAT5_MASK            (0x7  << 16)
#define MSDC0_PUPD_DAT6_MASK            (0x7  << 20)
#define MSDC0_PUPD_DAT4_MASK            (0x7  << 24)
#define MSDC0_PUPD_RSTB_MASK            (0x7  << 28)
#define MSDC0_PUPD0_MASK_WITH_RSTB      (0x77777777 << 0)
#define MSDC0_PUPD0_MASK                (0x7777777 << 0)

#define MSDC0_PUPD_DAT2_MASK            (0x7  <<  0)
#define MSDC0_PUPD_DAT3_MASK            (0x7  <<  4)
#define MSDC0_PUPD_DAT7_MASK            (0x7  <<  8)
#define MSDC0_PUPD_DSL_MASK             (0x7  << 12)
#define MSDC0_PUPD1_MASK                (0x7777 << 0)

/*
 * MSDC1 GPIO and PAD register and bitfields definition
 */
/*MSDC0_GPIO_MODE4, Set as pinmux value as 1*/
#define MSDC1_MODE_CLK_MASK             (7 <<  0)
#define MSDC1_MODE_CMD_MASK             (7 <<  6)
#define MSDC1_MODE_DAT0_MASK            (7 <<  9)
#define MSDC1_MODE_DAT1_MASK            (7 << 16)
#define MSDC1_MODE_DAT2_MASK            (7 << 12)
#define MSDC1_MODE_DAT3_MASK            (7 <<  3)

/* MSDC1 IES mask*/
#define MSDC1_IES_CLK_MASK              (0x1 <<  8)
#define MSDC1_IES_CMD_MASK              (0x1 <<  9)
#define MSDC1_IES_DAT_MASK              (0x1 << 10)
#define MSDC1_IES_ALL_MASK              (0x7 <<  8)

/* MSDC1 SMT mask*/
#define MSDC1_SMT_CLK_MASK              (0x1 <<  8)
#define MSDC1_SMT_CMD_MASK              (0x1 <<  9)
#define MSDC1_SMT_DAT_MASK              (0x1 << 10)
#define MSDC1_SMT_ALL_MASK              (0x7 <<  8)

/* MSDC1 TDSEL mask*/
#define MSDC1_TDSEL_CLK_MASK            (0xF <<  0)
#define MSDC1_TDSEL_CMD_MASK            (0xF <<  4)
#define MSDC1_TDSEL_DAT_MASK            (0xF <<  8)
#define MSDC1_TDSEL_ALL_MASK            (0xFFF << 0)

/* MSDC1 RDSEL mask*/
/*MSDC1_GPIO_RDSEL0*/
#define MSDC1_RDSEL_CLK_MASK            (0x3F << 16)
#define MSDC1_RDSEL_CMD_MASK            (0x3F << 22)
/*MSDC1_GPIO_RDSEL1*/
#define MSDC1_RDSEL_DAT_MASK            (0x3F <<  0)
#define MSDC1_RDSEL0_ALL_MASK           (0xFFF << 16)
#define MSDC1_RDSEL1_ALL_MASK           (0x3F <<  0)
/*Attention: not the same address, shall not define MSDC1_RDSEL_ALL_MASK*/

/* MSDC1 SR mask*/
#define MSDC1_SR_CLK_MASK               (0x1 <<  3)
#define MSDC1_SR_CMD_MASK               (0x1 <<  7)
#define MSDC1_SR_DAT_MASK               (0x1 << 11)
/*Attention: bits are not continuous, shall not define MSDC1_SR_ALL_MASK*/

/* MSDC1 DRV mask*/
#define MSDC1_DRV_CLK_MASK              (0x7 <<  0)
#define MSDC1_DRV_CMD_MASK              (0x7 <<  4)
#define MSDC1_DRV_DAT_MASK              (0x7 <<  8)
/*Attention: bits are not continuous, shall not define MSDC1_DRV_ALL_MASK*/

/* MSDC1 PUPD mask*/
#define MSDC1_PUPD_CMD_MASK             (0x7  <<  0)
#define MSDC1_PUPD_CLK_MASK             (0x7  <<  4)
#define MSDC1_PUPD_DAT0_MASK            (0x7  <<  8)
#define MSDC1_PUPD_DAT1_MASK            (0x7  << 12)
#define MSDC1_PUPD_DAT2_MASK            (0x7  << 16)
#define MSDC1_PUPD_DAT3_MASK            (0x7  << 20)
#define MSDC1_PUPD_MASK                 (0x777777 << 0)

/*
 * MSDC2 GPIO and PAD register and bitfields definition
 */
/*MSDC2_GPIO_MODE14, Set as pinmux value as 2*/
#define MSDC2_MODE_CLK_MASK             (7 <<  9)
#define MSDC2_MODE_CMD_MASK             (7 <<  6)
#define MSDC2_MODE_DAT0_MASK            (7 << 16)
#define MSDC2_MODE_DAT1_MASK            (7 <<  3)
#define MSDC2_MODE_DAT2_MASK            (7 << 19)
#define MSDC2_MODE_DAT3_MASK            (7 << 12)

/* MSDC2 IES mask*/
#define MSDC2_IES_CLK_MASK              (0x1 << 5)
#define MSDC2_IES_CMD_MASK              (0x1 << 4)
#define MSDC2_IES_DAT_MASK              (0x1 << 3)
#define MSDC2_IES_ALL_MASK              (0x7 << 3)

/* MSDC2 SMT mask*/
#define MSDC2_SMT_CLK_MASK              (0x1 << 5)
#define MSDC2_SMT_CMD_MASK              (0x1 << 4)
#define MSDC2_SMT_DAT_MASK              (0x1 << 3)
#define MSDC2_SMT_ALL_MASK              (0x7 << 3)

/* MSDC2 TDSEL mask*/
#define MSDC2_TDSEL_CLK_MASK            (0xF << 20)
#define MSDC2_TDSEL_CMD_MASK            (0xF << 16)
#define MSDC2_TDSEL_DAT_MASK            (0xF << 12)
#define MSDC2_TDSEL_ALL_MASK            (0xFFF << 12)

/* MSDC2 RDSEL mask*/
#define MSDC2_RDSEL_CLK_MASK            (0x3 << 10)
#define MSDC2_RDSEL_CMD_MASK            (0x3 << 8)
#define MSDC2_RDSEL_DAT_MASK            (0x3 << 6)
#define MSDC2_RDSEL_ALL_MASK            (0x3F << 6)

/* MSDC2 SR mask*/
#define MSDC2_SR_CLK_MASK               (0x1 << 23)
#define MSDC2_SR_CMD_MASK               (0x1 << 19)
#define MSDC2_SR_DAT_MASK               (0x1 << 15)

/* MSDC2 DRV mask*/
#define MSDC2_DRV_CLK_MASK              (0x7 << 20)
#define MSDC2_DRV_CMD_MASK              (0x7 << 16)
#define MSDC2_DRV_DAT_MASK              (0x7 << 12)

/* MSDC2 PUPD mask*/
#define MSDC2_PUPD_CLK_MASK             (0x7 << 20)
#define MSDC2_PUPD_CMD_MASK             (0x7 << 16)
#define MSDC2_PUPD_DAT1_MASK            (0x7 << 12)
#define MSDC2_PUPD_DAT3_MASK            (0x7 << 24)
#define MSDC2_PUPD_DAT0_MASK            (0x7 << 28)
#define MSDC2_PUPD_MASK0                (0xFFFFF << 12)

#define MSDC2_PUPD_DAT2_MASK            (0x7 << 0)
#define MSDC2_PUPD_MASK1                (0xF << 0)

#define BIT_GPIO_MSDC_CCIRBOND		(1 << 2)
#define BIT_GPIO_MSDC_PINMUXSEL		(1 << 0)

#define GPIO_MODE_245_249		(MSDC_GPIO_BASE + 0xA70)
#define GPIO_MODE_250_254		(MSDC_GPIO_BASE + 0xA80)
#define GPIO_MODE_255_259		(MSDC_GPIO_BASE + 0xA90)
#define GPIO_MODE_260_264		(MSDC_GPIO_BASE + 0xAA0)

#define GPIO_PUPD_MSDC3_D7D4		(MSDC_GPIO_BASE + 0x130)
#define GPIO_PUPD_MSDC3_D3D0		(MSDC_GPIO_BASE + 0xF40)

#define GPIO_MSDC2_V_CTRLCCIRBOND	(MSDC_GPIO_BASE + 0x710)
#define GPIO_MSDC2_V_CTRLPINMUXSEL	(MSDC_GPIO_BASE + 0xEF0)

#ifndef FPGA_PLATFORM
void msdc_set_driving_by_id(u32 id, struct msdc_hw *hw, bool sd_18);
void msdc_set_driving(struct msdc_host *host, struct msdc_hw *hw, bool sd_18);
void msdc_get_driving_by_id(u32 id, struct msdc_hw *hw);
void msdc_set_ies_by_id(u32 id, int set_ies);
void msdc_set_sr_by_id(u32 id, int clk, int cmd, int dat, int rst, int ds);
void msdc_set_smt_by_id(u32 id, int set_smt);
void msdc_set_tdsel_by_id(u32 id, u32 flag, u32 value);
void msdc_set_rdsel_by_id(u32 id, u32 flag, u32 value);
void msdc_get_tdsel_by_id(u32 id, u32 *value);
void msdc_get_rdsel_by_id(u32 id, u32 *value);
void msdc_dump_padctl_by_id(u32 id);
void msdc_pin_config_by_id(u32 id, u32 mode);
#endif

#ifdef FPGA_PLATFORM
#define msdc_set_driving_by_id(id, hw, sd_18)
/*#define msdc_set_driving(host, hw, sd_18)*/
#define msdc_get_driving_by_id(id, hw)
#define msdc_set_ies_by_id(id, set_ies)
#define msdc_set_sr_by_id(id, clk, cmd, dat, rst, ds)
#define msdc_set_smt_by_id(id, set_smt)
#define msdc_set_tdsel_by_id(id, flag, value)
#define msdc_set_rdsel_by_id(id, flag, value)
#define msdc_get_tdsel_by_id(id, value)
#define msdc_get_rdsel_by_id(id, value)
#define msdc_dump_padctl_by_id(id)
#define msdc_pin_config_by_id(id, mode)
#define msdc_set_pin_mode(host)

void msdc_set_driving(struct msdc_host *host, struct msdc_hw *hw, bool sd_18)
{
	/* do nothing, but for build pass */
}
#endif

#define msdc_get_driving(host, hw) \
	msdc_get_driving_by_id(host->id, hw)

/* #define msdc_set_driving(host, hw, sd_18)
 * msdc_set_driving_by_id(host->id, hw, sd_18)
 */

#define msdc_set_ies(host, set_ies) \
	msdc_set_ies_by_id(host->id, set_ies)

#define msdc_set_sr(host, clk, cmd, dat, rst, ds) \
	msdc_set_sr_by_id(host->id, clk, cmd, dat, rst, ds)

#define msdc_set_smt(host, set_smt) \
	msdc_set_smt_by_id(host->id, set_smt)

#define msdc_set_tdsel(host, flag, value) \
	msdc_set_tdsel_by_id(host->id, flag, value)

#define msdc_set_rdsel(host, flag, value) \
	msdc_set_rdsel_by_id(host->id, flag, value)

#define msdc_get_tdsel(host, value) \
	msdc_get_tdsel_by_id(host->id, value)

#define msdc_get_rdsel(host, value) \
	msdc_get_rdsel_by_id(host->id, value)

#define msdc_dump_padctl(host) \
	msdc_dump_padctl_by_id(host->id)

#define msdc_pin_config(host, mode) \
	msdc_pin_config_by_id(host->id, mode)

/**************************************************************/
/* Section 5: MISC                                            */
/**************************************************************/
void msdc_polling_axi_status(int line, int dead);

#endif /* end of _MSDC_IO_H_ */
