/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MSDC_CUST_MT6768_H_
#define _MSDC_CUST_MT6768_H_
#ifdef CONFIG_FPGA_EARLY_PORTING
#define FPGA_PLATFORM
#else
/* #define MTK_MSDC_BRINGUP_DEBUG */
#endif

#include <dt-bindings/mmc/mt6768-msdc.h>
/* #define CONFIG_MTK_MSDC_BRING_UP_BYPASS */
#if !defined(FPGA_PLATFORM)
#include <dt-bindings/clock/mt6768-clk.h>
#endif
#ifndef CONFIG_MTK_MSDC_BRING_UP_BYPASS
#include <mtk_spm_resource_req.h>
#endif
/**************************************************************/
/* Section 1: Device Tree                                     */
/**************************************************************/
/* Names used for device tree lookup */
#define DT_COMPATIBLE_NAME      "mediatek,msdc"
#define MSDC0_CLK_NAME          "msdc0-clock"
#if defined(CONFIG_MTK_HW_FDE) || defined(CONFIG_MMC_CRYPTO)
#define MSDC0_AES_CLK_NAME          "msdc0-aes-clock"
#endif
#define MSDC0_HCLK_NAME         "msdc0-hclock"
#define MSDC1_CLK_NAME          "msdc1-clock"
#define MSDC1_HCLK_NAME         "msdc1-hclock"
#define MSDC0_IOCFG_NAME        "mediatek,io_cfg_tl"
#define MSDC1_IOCFG_NAME        "mediatek,io_cfg_lb"


/**************************************************************/
/* Section 2: Power                                           */
/**************************************************************/
#define SD_POWER_DEFAULT_ON     (0)

#if !defined(FPGA_PLATFORM)
#include <mt-plat/upmu_common.h>

#define REG_VEMC_VOSEL_CAL      PMIC_RG_VEMC_VOCAL_ADDR
#define REG_VEMC_VOSEL          PMIC_RG_VEMC_VOSEL_ADDR
#define REG_VEMC_EN             PMIC_RG_LDO_VEMC_EN_ADDR

#define REG_VIO18_VOSEL_CAL     PMIC_RG_VIO18_VOCAL_ADDR
/* vio18 have no REG_VIO18_VOSEL, so not dump. */
#define REG_VIO18_EN            PMIC_RG_LDO_VIO18_EN_ADDR

#define REG_VMC_VOSEL_CAL       PMIC_RG_VMC_VOCAL_ADDR
#define REG_VMC_VOSEL           PMIC_RG_VMC_VOSEL_ADDR
#define REG_VMC_EN              PMIC_RG_LDO_VMC_EN_ADDR

#define REG_VMCH_VOSEL_CAL      PMIC_RG_VMCH_VOCAL_ADDR
#define REG_VMCH_VOSEL          PMIC_RG_VMCH_VOSEL_ADDR
#define REG_VMCH_EN             PMIC_RG_LDO_VMCH_EN_ADDR

#define MASK_VEMC_VOSEL_CAL     PMIC_RG_VEMC_VOCAL_MASK
#define SHIFT_VEMC_VOSEL_CAL    PMIC_RG_VEMC_VOCAL_SHIFT
#define FIELD_VEMC_VOSEL_CAL    (MASK_VEMC_VOSEL_CAL << SHIFT_VEMC_VOSEL_CAL)

#define MASK_VEMC_VOSEL         PMIC_RG_VEMC_VOSEL_MASK
#define SHIFT_VEMC_VOSEL        PMIC_RG_VEMC_VOSEL_SHIFT
#define FIELD_VEMC_VOSEL        (MASK_VEMC_VOSEL << SHIFT_VEMC_VOSEL)

#define MASK_VEMC_EN            PMIC_RG_LDO_VEMC_EN_MASK
#define SHIFT_VEMC_EN           PMIC_RG_LDO_VEMC_EN_SHIFT
#define FIELD_VEMC_EN           (MASK_VEMC_EN << SHIFT_VEMC_EN)

#define MASK_VIO18_VOSEL_CAL    PMIC_RG_VIO18_VOCAL_MASK
#define SHIFT_VIO18_VOSEL_CAL   PMIC_RG_VIO18_VOCAL_SHIFT
#define FIELD_VIO18_VOSEL_CAL   (MASK_VIO18_VOSEL_CAL << SHIFT_VIO18_VOSEL_CAL)

#define MASK_VIO18_EN           PMIC_RG_LDO_VIO18_EN_MASK
#define SHIFT_VIO18_EN          PMIC_RG_LDO_VIO18_EN_SHIFT
#define FIELD_VIO18_EN          (MASK_VIO18_EN << SHIFT_VIO18_EN)

#define MASK_VMC_VOSEL_CAL      PMIC_RG_VMC_VOCAL_MASK
#define SHIFT_VMC_VOSEL_CAL     PMIC_RG_VMC_VOCAL_SHIFT
#define FIELD_VMC_VOSEL_CAL     (MASK_VMC_VOSEL_CAL << SHIFT_VMC_VOSEL_CAL)

#define MASK_VMC_VOSEL          PMIC_RG_VMC_VOSEL_MASK
#define SHIFT_VMC_VOSEL         PMIC_RG_VMC_VOSEL_SHIFT
#define FIELD_VMC_VOSEL         (MASK_VMC_VOSEL << SHIFT_VMC_VOSEL)

#define MASK_VMC_EN             PMIC_RG_LDO_VMC_EN_MASK
#define SHIFT_VMC_EN            PMIC_RG_LDO_VMC_EN_SHIFT
#define FIELD_VMC_EN            (MASK_VMC_EN << SHIFT_VMC_EN)

#define MASK_VMCH_VOSEL_CAL     PMIC_RG_VMCH_VOCAL_MASK
#define SHIFT_VMCH_VOSEL_CAL    PMIC_RG_VMCH_VOCAL_SHIFT
#define FIELD_VMCH_VOSEL_CAL    (MASK_VMCH_VOSEL_CAL << SHIFT_VMCH_VOSEL_CAL)

#define MASK_VMCH_VOSEL         PMIC_RG_VMCH_VOSEL_MASK
#define SHIFT_VMCH_VOSEL        PMIC_RG_VMCH_VOSEL_SHIFT
#define FIELD_VMCH_VOSEL        (MASK_VMCH_VOSEL << SHIFT_VMCH_VOSEL)

#define MASK_VMCH_EN            PMIC_RG_LDO_VMCH_EN_MASK
#define SHIFT_VMCH_EN           PMIC_RG_LDO_VMCH_EN_SHIFT
#define FIELD_VMCH_EN           (MASK_VMCH_EN << SHIFT_VMCH_EN)

#define VEMC_VOSEL_3V           (11)
#define VEMC_VOSEL_3V3          (13)
#define VMC_VOSEL_1V8           (4)
#define VMC_VOSEL_2V8           (9)
#define VMC_VOSEL_3V            (11)
#define VMCH_VOSEL_3V           (11)
#define VMCH_VOSEL_3V3          (13)

#define REG_VCORE_VOSEL_SW      PMIC_RG_BUCK_VCORE_VOSEL_ADDR
#define VCORE_VOSEL_SW_MASK     PMIC_RG_BUCK_VCORE_VOSEL_MASK
#define VCORE_VOSEL_SW_SHIFT    PMIC_RG_BUCK_VCORE_VOSEL_SHIFT

#define REG_VIO_VOCAL_SW      PMIC_RG_VIO18_VOCAL_ADDR
#define VIO_VOCAL_SW_MASK     PMIC_RG_VIO18_VOCAL_MASK
#define VIO_VOCAL_SW_SHIFT    PMIC_RG_VIO18_VOCAL_SHIFT

#endif

#define EMMC_VOL_ACTUAL         VOL_3000
#define SD_VOL_ACTUAL           VOL_3000


/**************************************************************/
/* Section 3: Clock                                           */
/**************************************************************/
#define MSDCPLL_FREQ            800000000

/* list the other value by clock owners' clock table doc if needed */
#define MSDC0_SRC_0             260000
#define MSDC0_SRC_1             (MSDCPLL_FREQ/2)

#define MSDC1_SRC_0             260000
#define MSDC1_SRC_1             208000000
#define MSDC1_SRC_2             (MSDCPLL_FREQ/4)

#define MSDC3_SRC_0             260000
#define MSDC3_SRC_1             208000000
#define MSDC3_SRC_2             (MSDCPLL_FREQ/2)
#define MSDC3_SRC_3             156000000
#define MSDC3_SRC_4             182000000
#define MSDC3_SRC_5             312000000
#define MSDC3_SRC_6             364000000
#define MSDC3_SRC_7             (MSDCPLL_FREQ/4)

#define MSDC_SRC_FPGA           12000000

/**************************************************************/
/* Section 4: GPIO and Pad                                    */
/**************************************************************/
/*--------------------------------------------------------------------------*/
/* MSDC0~1 GPIO and IO Pad Configuration Base                               */
/*--------------------------------------------------------------------------*/
/* 0x10005000 */
#define MSDC_GPIO_BASE          gpio_base
/* 0x10002E00 */
#define MSDC0_IO_PAD_BASE       (msdc_io_cfg_bases[0])
/* 0x10002400 */
#define MSDC1_IO_PAD_BASE       (msdc_io_cfg_bases[1])

/*--------------------------------------------------------------------------*/
/* MSDC GPIO Related Register                                               */
/*--------------------------------------------------------------------------*/
/* MSDC0 */
#define MSDC0_GPIO_MODE0       (MSDC_GPIO_BASE + 0x3f0)
#define MSDC0_GPIO_MODE1       (MSDC_GPIO_BASE + 0x400)
#define MSDC0_GPIO_IES     (MSDC0_IO_PAD_BASE + 0x10)
#define MSDC0_GPIO_SMT     (MSDC0_IO_PAD_BASE + 0x60)
#define MSDC0_GPIO_TDSEL   (MSDC0_IO_PAD_BASE + 0x70)
#define MSDC0_GPIO_RDSEL   (MSDC0_IO_PAD_BASE + 0x50)
#define MSDC0_GPIO_DRV     (MSDC0_IO_PAD_BASE + 0)
#define MSDC0_GPIO_PUPD   (MSDC0_IO_PAD_BASE + 0x20)
#define MSDC0_GPIO_R0   (MSDC0_IO_PAD_BASE + 0x30)
#define MSDC0_GPIO_R1   (MSDC0_IO_PAD_BASE + 0x40)

/* MSDC1 */
#define MSDC1_GPIO_MODE0       (MSDC_GPIO_BASE + 0x440)
#define MSDC1_GPIO_MODE1       (MSDC_GPIO_BASE + 0x450)

#define MSDC1_GPIO_IES     (MSDC1_IO_PAD_BASE + 0x10)
#define MSDC1_GPIO_SMT     (MSDC1_IO_PAD_BASE + 0x80)
#define MSDC1_GPIO_TDSEL   (MSDC1_IO_PAD_BASE + 0xa0)
#define MSDC1_GPIO_RDSEL   (MSDC1_IO_PAD_BASE + 0x60)
#define MSDC1_GPIO_DRV     (MSDC1_IO_PAD_BASE + 0)
#define MSDC1_GPIO_SR     (MSDC1_IO_PAD_BASE + 0x90)
#define MSDC1_GPIO_PUPD   (MSDC1_IO_PAD_BASE + 0x30)
#define MSDC1_GPIO_R0   (MSDC1_IO_PAD_BASE + 0x40)
#define MSDC1_GPIO_R1   (MSDC1_IO_PAD_BASE + 0x50)

/**************************************************************/
/* Section 5: Adjustable Driver Parameter                     */
/**************************************************************/
#define HOST_MAX_BLKSZ          (2048)

#define MSDC_OCR_AVAIL\
	(MMC_VDD_28_29 | MMC_VDD_29_30 | MMC_VDD_30_31 \
	| MMC_VDD_31_32 | MMC_VDD_32_33)
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

/* hw diff: 0xB0[0]=1 */
#define MSDC_PB0_DEFAULT_VAL            0x403C0007
#define MSDC_PB1_DEFAULT_VAL            0xFFFA0349


#define MSDC_PB2_DEFAULT_RESPWAITCNT    0x3
#define MSDC_PB2_DEFAULT_RESPSTENSEL    0x1
#define MSDC_PB2_DEFAULT_CRCSTSENSEL    0x1

#endif /* _MSDC_CUST_MT6768_H_ */
