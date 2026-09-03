/* SPDX-License-Identifier: (GPL-2.0-only OR .*) */
/*
 * Unisoc SC2721 MASK Ctrl Register File
 *
 * Copyright C 2023, Unisoc Inc.
 */

#ifndef __DT_MFD_UNISOC_SC2721_GLB_H
#define __DT_MFD_UNISOC_SC2721_GLB_H

#define CTL_BASE_ANA_GLB                        0Xc00

#define ANA_REG_GLB_CHIP_ID_LOW                 (CTL_BASE_ANA_GLB + 0x0000)
#define ANA_REG_GLB_CHIP_ID_HIGH                (CTL_BASE_ANA_GLB + 0x0004)
#define ANA_REG_GLB_MODULE_EN0                  (CTL_BASE_ANA_GLB + 0x0008)
#define ANA_REG_GLB_ARM_CLK_EN0                 (CTL_BASE_ANA_GLB + 0x000C)
#define ANA_REG_GLB_RTC_CLK_EN0                 (CTL_BASE_ANA_GLB + 0x0010)
#define ANA_REG_GLB_SOFT_RST0                   (CTL_BASE_ANA_GLB + 0x0014)
#define ANA_REG_GLB_SOFT_RST1                   (CTL_BASE_ANA_GLB + 0x0018)
#define ANA_REG_GLB_POWER_PD_SW                 (CTL_BASE_ANA_GLB + 0x001C)
#define ANA_REG_GLB_POWER_PD_HW                 (CTL_BASE_ANA_GLB + 0x0020)
#define ANA_REG_GLB_SOFT_RST_HW                 (CTL_BASE_ANA_GLB + 0x0024)
#define ANA_REG_GLB_RESERVED_REG1               (CTL_BASE_ANA_GLB + 0x0028)
#define ANA_REG_GLB_RESERVED_REG2               (CTL_BASE_ANA_GLB + 0x002C)
#define ANA_REG_GLB_RESERVED_REG3               (CTL_BASE_ANA_GLB + 0x0030)
#define ANA_REG_GLB_RESERVED_REG4               (CTL_BASE_ANA_GLB + 0x0034)
#define ANA_REG_GLB_DCDC_CPU_REG0               (CTL_BASE_ANA_GLB + 0x0038)
#define ANA_REG_GLB_DCDC_CPU_REG1               (CTL_BASE_ANA_GLB + 0x003C)
#define ANA_REG_GLB_DCDC_CPU_REG2               (CTL_BASE_ANA_GLB + 0x0040)
#define ANA_REG_GLB_DCDC_CPU_VOL                (CTL_BASE_ANA_GLB + 0x0044)
#define ANA_REG_GLB_DCDC_CORE_REG0              (CTL_BASE_ANA_GLB + 0x0048)
#define ANA_REG_GLB_DCDC_CORE_REG1              (CTL_BASE_ANA_GLB + 0x004C)
#define ANA_REG_GLB_DCDC_CORE_REG2              (CTL_BASE_ANA_GLB + 0x0050)
#define ANA_REG_GLB_DCDC_CORE_VOL               (CTL_BASE_ANA_GLB + 0x0054)
#define ANA_REG_GLB_DCDC_MEM_REG0               (CTL_BASE_ANA_GLB + 0x0058)
#define ANA_REG_GLB_DCDC_MEM_REG1               (CTL_BASE_ANA_GLB + 0x005C)
#define ANA_REG_GLB_DCDC_MEM_REG2               (CTL_BASE_ANA_GLB + 0x0060)
#define ANA_REG_GLB_DCDC_MEM_VOL                (CTL_BASE_ANA_GLB + 0x0064)
#define ANA_REG_GLB_DCDC_GEN_REG0               (CTL_BASE_ANA_GLB + 0x0068)
#define ANA_REG_GLB_DCDC_GEN_REG1               (CTL_BASE_ANA_GLB + 0x006C)
#define ANA_REG_GLB_DCDC_GEN_REG2               (CTL_BASE_ANA_GLB + 0x0070)
#define ANA_REG_GLB_DCDC_GEN_VOL                (CTL_BASE_ANA_GLB + 0x0074)
#define ANA_REG_GLB_DCDC_WPA_REG0               (CTL_BASE_ANA_GLB + 0x0078)
#define ANA_REG_GLB_DCDC_WPA_REG1               (CTL_BASE_ANA_GLB + 0x007C)
#define ANA_REG_GLB_DCDC_WPA_REG2               (CTL_BASE_ANA_GLB + 0x0080)
#define ANA_REG_GLB_DCDC_WPA_REG3               (CTL_BASE_ANA_GLB + 0x0084)
#define ANA_REG_GLB_DCDC_WPA_VOL                (CTL_BASE_ANA_GLB + 0x0088)
#define ANA_REG_GLB_DCDC_WPA_DCM_HW             (CTL_BASE_ANA_GLB + 0x008C)
#define ANA_REG_GLB_DCDC_CH_CTRL                (CTL_BASE_ANA_GLB + 0x0090)
#define ANA_REG_GLB_DCDC_CTRL                   (CTL_BASE_ANA_GLB + 0x0094)
#define ANA_REG_GLB_DCDC_CLK_CTRL               (CTL_BASE_ANA_GLB + 0x0098)
#define ANA_REG_GLB_RESERVED_REG6               (CTL_BASE_ANA_GLB + 0x009C)
#define ANA_REG_GLB_RESERVED_REG7               (CTL_BASE_ANA_GLB + 0x00A0)
#define ANA_REG_GLB_RESERVED_REG8               (CTL_BASE_ANA_GLB + 0x00A4)
#define ANA_REG_GLB_RESERVED_REG9               (CTL_BASE_ANA_GLB + 0x00A8)
#define ANA_REG_GLB_RESERVED_REG10              (CTL_BASE_ANA_GLB + 0x00AC)
#define ANA_REG_GLB_LDO_CAMA_REG0               (CTL_BASE_ANA_GLB + 0x00B0)
#define ANA_REG_GLB_LDO_CAMA_REG1               (CTL_BASE_ANA_GLB + 0x00B4)
#define ANA_REG_GLB_LDO_CAMMOT_REG0             (CTL_BASE_ANA_GLB + 0x00B8)
#define ANA_REG_GLB_LDO_CAMMOT_REG1             (CTL_BASE_ANA_GLB + 0x00BC)
#define ANA_REG_GLB_LDO_SIM0_REG0               (CTL_BASE_ANA_GLB + 0x00C0)
#define ANA_REG_GLB_LDO_SIM0_PD_REG             (CTL_BASE_ANA_GLB + 0x00C4)
#define ANA_REG_GLB_LDO_SIM0_REG1               (CTL_BASE_ANA_GLB + 0x00C8)
#define ANA_REG_GLB_LDO_SIM1_REG0               (CTL_BASE_ANA_GLB + 0x00CC)
#define ANA_REG_GLB_LDO_SIM1_PD_REG             (CTL_BASE_ANA_GLB + 0x00D0)
#define ANA_REG_GLB_LDO_SIM1_REG1               (CTL_BASE_ANA_GLB + 0x00D4)
#define ANA_REG_GLB_LDO_SIM2_REG0               (CTL_BASE_ANA_GLB + 0x00D8)
#define ANA_REG_GLB_LDO_SIM2_PD_REG             (CTL_BASE_ANA_GLB + 0x00DC)
#define ANA_REG_GLB_LDO_SIM2_REG1               (CTL_BASE_ANA_GLB + 0x00E0)
#define ANA_REG_GLB_LDO_EMMCCORE_REG0           (CTL_BASE_ANA_GLB + 0x00E4)
#define ANA_REG_GLB_LDO_EMMCCORE_PD_REG         (CTL_BASE_ANA_GLB + 0x00E8)
#define ANA_REG_GLB_LDO_EMMCCORE_REG1           (CTL_BASE_ANA_GLB + 0x00EC)
#define ANA_REG_GLB_LDO_SD_REG0                 (CTL_BASE_ANA_GLB + 0x00F0)
#define ANA_REG_GLB_LDO_SD_PD_REG               (CTL_BASE_ANA_GLB + 0x00F4)
#define ANA_REG_GLB_LDO_SD_REG1                 (CTL_BASE_ANA_GLB + 0x00F8)
#define ANA_REG_GLB_LDO_SDIO_REG0               (CTL_BASE_ANA_GLB + 0x00FC)
#define ANA_REG_GLB_LDO_SDIO_PD_REG             (CTL_BASE_ANA_GLB + 0x0100)
#define ANA_REG_GLB_LDO_SDIO_REG1               (CTL_BASE_ANA_GLB + 0x0104)
#define ANA_REG_GLB_LDO_VDD28_REG0              (CTL_BASE_ANA_GLB + 0x0108)
#define ANA_REG_GLB_LDO_VDD28_REG1              (CTL_BASE_ANA_GLB + 0x010C)
#define ANA_REG_GLB_LDO_WIFIPA_REG0             (CTL_BASE_ANA_GLB + 0x0110)
#define ANA_REG_GLB_LDO_WIFIPA_REG1             (CTL_BASE_ANA_GLB + 0x0114)
#define ANA_REG_GLB_LDO_DCXO_REG0               (CTL_BASE_ANA_GLB + 0x0118)
#define ANA_REG_GLB_LDO_DCXO_REG1               (CTL_BASE_ANA_GLB + 0x011C)
#define ANA_REG_GLB_LDO_USB33_REG0              (CTL_BASE_ANA_GLB + 0x0120)
#define ANA_REG_GLB_LDO_USB33_PD_REG            (CTL_BASE_ANA_GLB + 0x0124)
#define ANA_REG_GLB_LDO_USB33_REG1              (CTL_BASE_ANA_GLB + 0x0128)
#define ANA_REG_GLB_LDO_CAMD_REG0               (CTL_BASE_ANA_GLB + 0x012C)
#define ANA_REG_GLB_LDO_CAMD_REG1               (CTL_BASE_ANA_GLB + 0x0130)
#define ANA_REG_GLB_LDO_CON_REG0                (CTL_BASE_ANA_GLB + 0x0134)
#define ANA_REG_GLB_LDO_CON_REG1                (CTL_BASE_ANA_GLB + 0x0138)
#define ANA_REG_GLB_LDO_CAMIO_REG0              (CTL_BASE_ANA_GLB + 0x013C)
#define ANA_REG_GLB_LDO_CAMIO_REG1              (CTL_BASE_ANA_GLB + 0x0140)
#define ANA_REG_GLB_LDO_AVDD18_REG0             (CTL_BASE_ANA_GLB + 0x0144)
#define ANA_REG_GLB_LDO_AVDD18_REG1             (CTL_BASE_ANA_GLB + 0x0148)
#define ANA_REG_GLB_LDO_VDDRF18_REG0            (CTL_BASE_ANA_GLB + 0x014C)
#define ANA_REG_GLB_LDO_VDDRF18_REG1            (CTL_BASE_ANA_GLB + 0x0150)
#define ANA_REG_GLB_LDO_VDDRF15_REG0            (CTL_BASE_ANA_GLB + 0x0154)
#define ANA_REG_GLB_LDO_VDDRF15_REG1            (CTL_BASE_ANA_GLB + 0x0158)
#define ANA_REG_GLB_LDO_MEM_REG0                (CTL_BASE_ANA_GLB + 0x015C)
#define ANA_REG_GLB_LDO_MEM_REG1                (CTL_BASE_ANA_GLB + 0x0160)
#define ANA_REG_GLB_RESERVED_REG17              (CTL_BASE_ANA_GLB + 0x0164)
#define ANA_REG_GLB_RESERVED_REG18              (CTL_BASE_ANA_GLB + 0x0168)
#define ANA_REG_GLB_RESERVED_REG19              (CTL_BASE_ANA_GLB + 0x016C)
#define ANA_REG_GLB_RESERVED_REG20              (CTL_BASE_ANA_GLB + 0x0170)
#define ANA_REG_GLB_RESERVED_REG21              (CTL_BASE_ANA_GLB + 0x0174)
#define ANA_REG_GLB_LDO_RTC_CTRL                (CTL_BASE_ANA_GLB + 0x0178)
#define ANA_REG_GLB_LDO_CH_CTRL                 (CTL_BASE_ANA_GLB + 0x017C)
#define ANA_REG_GLB_RESERVED_REG23              (CTL_BASE_ANA_GLB + 0x0180)
#define ANA_REG_GLB_RESERVED_REG24              (CTL_BASE_ANA_GLB + 0x0184)
#define ANA_REG_GLB_RESERVED_REG25              (CTL_BASE_ANA_GLB + 0x0188)
#define ANA_REG_GLB_RESERVED_REG26              (CTL_BASE_ANA_GLB + 0x018C)
#define ANA_REG_GLB_RESERVED_REG27              (CTL_BASE_ANA_GLB + 0x0190)
#define ANA_REG_GLB_SLP_WAIT_DCDCCPU            (CTL_BASE_ANA_GLB + 0x0194)
#define ANA_REG_GLB_SLP_CTRL                    (CTL_BASE_ANA_GLB + 0x0198)
#define ANA_REG_GLB_SLP_DCDC_PD_CTRL            (CTL_BASE_ANA_GLB + 0x019C)
#define ANA_REG_GLB_SLP_LDO_PD_CTRL0            (CTL_BASE_ANA_GLB + 0x01A0)
#define ANA_REG_GLB_SLP_LDO_PD_CTRL1            (CTL_BASE_ANA_GLB + 0x01A4)
#define ANA_REG_GLB_SLP_DCDC_LP_CTRL            (CTL_BASE_ANA_GLB + 0x01A8)
#define ANA_REG_GLB_SLP_LDO_LP_CTRL0            (CTL_BASE_ANA_GLB + 0x01AC)
#define ANA_REG_GLB_SLP_LDO_LP_CTRL1            (CTL_BASE_ANA_GLB + 0x01B0)
#define ANA_REG_GLB_DCDC_CORE_SLP_CTRL0         (CTL_BASE_ANA_GLB + 0x01B4)
#define ANA_REG_GLB_DCDC_CORE_SLP_CTRL1         (CTL_BASE_ANA_GLB + 0x01B8)
#define ANA_REG_GLB_DCDC_CORE_SLP_CTRL2         (CTL_BASE_ANA_GLB + 0x01BC)
#define ANA_REG_GLB_DCDC_CORE_SLP_CTRL3         (CTL_BASE_ANA_GLB + 0x01C0)
#define ANA_REG_GLB_DCDC_CORE_SLP_CTRL4         (CTL_BASE_ANA_GLB + 0x01C4)
#define ANA_REG_GLB_DCDC_CORE_SLP_CTRL5         (CTL_BASE_ANA_GLB + 0x01C8)
#define ANA_REG_GLB_DCDC_CPU_SLP_CTRL0          (CTL_BASE_ANA_GLB + 0x01CC)
#define ANA_REG_GLB_DCDC_CPU_SLP_CTRL1          (CTL_BASE_ANA_GLB + 0x01D0)
#define ANA_REG_GLB_DCDC_CPU_SLP_CTRL2          (CTL_BASE_ANA_GLB + 0x01D4)
#define ANA_REG_GLB_DCDC_CPU_SLP_CTRL3          (CTL_BASE_ANA_GLB + 0x01D8)
#define ANA_REG_GLB_DCDC_CPU_SLP_CTRL4          (CTL_BASE_ANA_GLB + 0x01DC)
#define ANA_REG_GLB_DCDC_CPU_SLP_CTRL5          (CTL_BASE_ANA_GLB + 0x01E0)
#define ANA_REG_GLB_DCDC_XTL_EN0                (CTL_BASE_ANA_GLB + 0x01E4)
#define ANA_REG_GLB_DCDC_XTL_EN1                (CTL_BASE_ANA_GLB + 0x01E8)
#define ANA_REG_GLB_DCDC_XTL_EN2                (CTL_BASE_ANA_GLB + 0x01EC)
#define ANA_REG_GLB_RESEVED_DCDC_XTL_EN4        (CTL_BASE_ANA_GLB + 0x01F0)
#define ANA_REG_GLB_LDO_XTL_EN0                 (CTL_BASE_ANA_GLB + 0x01F4)
#define ANA_REG_GLB_LDO_XTL_EN1                 (CTL_BASE_ANA_GLB + 0x01F8)
#define ANA_REG_GLB_LDO_XTL_EN2                 (CTL_BASE_ANA_GLB + 0x01FC)
#define ANA_REG_GLB_LDO_XTL_EN3                 (CTL_BASE_ANA_GLB + 0x0200)
#define ANA_REG_GLB_LDO_XTL_EN4                 (CTL_BASE_ANA_GLB + 0x0204)
#define ANA_REG_GLB_LDO_XTL_EN5                 (CTL_BASE_ANA_GLB + 0x0208)
#define ANA_REG_GLB_LDO_XTL_EN6                 (CTL_BASE_ANA_GLB + 0x020C)
#define ANA_REG_GLB_LDO_XTL_EN7                 (CTL_BASE_ANA_GLB + 0x0210)
#define ANA_REG_GLB_LDO_XTL_EN8                 (CTL_BASE_ANA_GLB + 0x0214)
#define ANA_REG_GLB_LDO_XTL_EN9                 (CTL_BASE_ANA_GLB + 0x0218)
#define ANA_REG_GLB_LDO_XTL_EN10                (CTL_BASE_ANA_GLB + 0x021C)
#define ANA_REG_GLB_DCXO_XTL_EN                 (CTL_BASE_ANA_GLB + 0x0220)
#define ANA_REG_GLB_RESERVED_REG30              (CTL_BASE_ANA_GLB + 0x0224)
#define ANA_REG_GLB_RESERVED_REG31              (CTL_BASE_ANA_GLB + 0x0228)
#define ANA_REG_GLB_RESERVED_REG32              (CTL_BASE_ANA_GLB + 0x022C)
#define ANA_REG_GLB_RESERVED_REG33              (CTL_BASE_ANA_GLB + 0x0230)
#define ANA_REG_GLB_RESERVED_REG34              (CTL_BASE_ANA_GLB + 0x0234)
#define ANA_REG_GLB_TSX_CTRL0                   (CTL_BASE_ANA_GLB + 0x0238)
#define ANA_REG_GLB_TSX_CTRL1                   (CTL_BASE_ANA_GLB + 0x023C)
#define ANA_REG_GLB_TSX_CTRL2                   (CTL_BASE_ANA_GLB + 0x0240)
#define ANA_REG_GLB_TSX_CTRL3                   (CTL_BASE_ANA_GLB + 0x0244)
#define ANA_REG_GLB_TSX_CTRL4                   (CTL_BASE_ANA_GLB + 0x0248)
#define ANA_REG_GLB_TSX_CTRL5                   (CTL_BASE_ANA_GLB + 0x024C)
#define ANA_REG_GLB_TSX_CTRL6                   (CTL_BASE_ANA_GLB + 0x0250)
#define ANA_REG_GLB_TSX_CTRL7                   (CTL_BASE_ANA_GLB + 0x0254)
#define ANA_REG_GLB_RESERVED_REG36              (CTL_BASE_ANA_GLB + 0x0258)
#define ANA_REG_GLB_RESERVED_REG37              (CTL_BASE_ANA_GLB + 0x025C)
#define ANA_REG_GLB_RESERVED_REG38              (CTL_BASE_ANA_GLB + 0x0260)
#define ANA_REG_GLB_TSEN_CTRL0                  (CTL_BASE_ANA_GLB + 0x0264)
#define ANA_REG_GLB_TSEN_CTRL1                  (CTL_BASE_ANA_GLB + 0x0268)
#define ANA_REG_GLB_TSEN_CTRL2                  (CTL_BASE_ANA_GLB + 0x026C)
#define ANA_REG_GLB_RESERVED_REG40              (CTL_BASE_ANA_GLB + 0x0270)
#define ANA_REG_GLB_RESERVED_REG41              (CTL_BASE_ANA_GLB + 0x0274)
#define ANA_REG_GLB_BG_CTRL                     (CTL_BASE_ANA_GLB + 0x0278)
#define ANA_REG_GLB_DCDC_VLG_SEL                (CTL_BASE_ANA_GLB + 0x027C)
#define ANA_REG_GLB_LDO_VLG_SEL0                (CTL_BASE_ANA_GLB + 0x0280)
#define ANA_REG_GLB_LDO_VLG_SEL1                (CTL_BASE_ANA_GLB + 0x0284)
#define ANA_REG_GLB_CLK32KLESS_CTRL0            (CTL_BASE_ANA_GLB + 0x0288)
#define ANA_REG_GLB_CLK32KLESS_CTRL1            (CTL_BASE_ANA_GLB + 0x028C)
#define ANA_REG_GLB_CLK32KLESS_CTRL2            (CTL_BASE_ANA_GLB + 0x0290)
#define ANA_REG_GLB_CLK32KLESS_CTRL3            (CTL_BASE_ANA_GLB + 0x0294)
#define ANA_REG_GLB_CLK32KLESS_CTRL4            (CTL_BASE_ANA_GLB + 0x0298)
#define ANA_REG_GLB_XTL_WAIT_CTRL0              (CTL_BASE_ANA_GLB + 0x029C)
#define ANA_REG_GLB_RGB_CTRL                    (CTL_BASE_ANA_GLB + 0x02A0)
#define ANA_REG_GLB_IB_CTRL                     (CTL_BASE_ANA_GLB + 0x02A4)
#define ANA_REG_GLB_FLASH_CTRL                  (CTL_BASE_ANA_GLB + 0x02A8)
#define ANA_REG_GLB_KPLED_CTRL0                 (CTL_BASE_ANA_GLB + 0x02AC)
#define ANA_REG_GLB_KPLED_CTRL1                 (CTL_BASE_ANA_GLB + 0x02B0)
#define ANA_REG_GLB_VIBR_CTRL0                  (CTL_BASE_ANA_GLB + 0x02B4)
#define ANA_REG_GLB_VIBR_CTRL1                  (CTL_BASE_ANA_GLB + 0x02B8)
#define ANA_REG_GLB_AUDIO_CTRL0                 (CTL_BASE_ANA_GLB + 0x02BC)
#define ANA_REG_GLB_CHGR_CTRL0                  (CTL_BASE_ANA_GLB + 0x02C0)
#define ANA_REG_GLB_CHGR_CTRL1                  (CTL_BASE_ANA_GLB + 0x02C4)
#define ANA_REG_GLB_CHGR_STATUS                 (CTL_BASE_ANA_GLB + 0x02C8)
#define ANA_REG_GLB_CHGR_DET_FGU_CTRL           (CTL_BASE_ANA_GLB + 0x02CC)
#define ANA_REG_GLB_OVLO_CTRL                   (CTL_BASE_ANA_GLB + 0x02D0)
#define ANA_REG_GLB_MIXED_CTRL                  (CTL_BASE_ANA_GLB + 0x02D4)
#define ANA_REG_GLB_POR_RST_MONITOR             (CTL_BASE_ANA_GLB + 0x02D8)
#define ANA_REG_GLB_WDG_RST_MONITOR             (CTL_BASE_ANA_GLB + 0x02DC)
#define ANA_REG_GLB_POR_PIN_RST_MONITOR         (CTL_BASE_ANA_GLB + 0x02E0)
#define ANA_REG_GLB_POR_SRC_FLAG                (CTL_BASE_ANA_GLB + 0x02E4)
#define ANA_REG_GLB_POR_OFF_FLAG                (CTL_BASE_ANA_GLB + 0x02E8)
#define ANA_REG_GLB_POR_7S_CTRL                 (CTL_BASE_ANA_GLB + 0x02EC)
#define ANA_REG_GLB_HWRST_RTC                   (CTL_BASE_ANA_GLB + 0x02F0)
#define ANA_REG_GLB_ARCH_EN                     (CTL_BASE_ANA_GLB + 0x02F4)
#define ANA_REG_GLB_MCU_WR_PROT_VALUE           (CTL_BASE_ANA_GLB + 0x02F8)
#define ANA_REG_GLB_PWR_WR_PROT_VALUE           (CTL_BASE_ANA_GLB + 0x02FC)
#define ANA_REG_GLB_SMPL_CTRL0                  (CTL_BASE_ANA_GLB + 0x0300)
#define ANA_REG_GLB_SMPL_CTRL1                  (CTL_BASE_ANA_GLB + 0x0304)
#define ANA_REG_GLB_RTC_RST0                    (CTL_BASE_ANA_GLB + 0x0308)
#define ANA_REG_GLB_RTC_RST1                    (CTL_BASE_ANA_GLB + 0x030C)
#define ANA_REG_GLB_RTC_RST2                    (CTL_BASE_ANA_GLB + 0x0310)
#define ANA_REG_GLB_RTC_CLK_STOP                (CTL_BASE_ANA_GLB + 0x0314)
#define ANA_REG_GLB_VBAT_DROP_CNT               (CTL_BASE_ANA_GLB + 0x0318)
#define ANA_REG_GLB_SWRST_CTRL0                 (CTL_BASE_ANA_GLB + 0x031C)
#define ANA_REG_GLB_SWRST_CTRL1                 (CTL_BASE_ANA_GLB + 0x0320)
#define ANA_REG_GLB_OTP_CTRL                    (CTL_BASE_ANA_GLB + 0x0324)
#define ANA_REG_GLB_FREE_TIMER_LOW              (CTL_BASE_ANA_GLB + 0x0328)
#define ANA_REG_GLB_FREE_TIMER_HIGH             (CTL_BASE_ANA_GLB + 0x032C)
#define ANA_REG_GLB_LOW_PWR_CLK32K_CTRL         (CTL_BASE_ANA_GLB + 0x0330)
#define ANA_REG_GLB_LOW_PWR_CLK32K_TEST0        (CTL_BASE_ANA_GLB + 0x0334)
#define ANA_REG_GLB_LOW_PWR_CLK32K_TEST1        (CTL_BASE_ANA_GLB + 0x0338)
#define ANA_REG_GLB_LOW_PWR_CLK32K_TEST2        (CTL_BASE_ANA_GLB + 0x033C)
#define ANA_REG_GLB_LOW_PWR_CLK32K_TEST3        (CTL_BASE_ANA_GLB + 0x0340)
#define ANA_REG_GLB_VOL_TUNE_CTRL_CORE          (CTL_BASE_ANA_GLB + 0x0344)
#define ANA_REG_GLB_VOL_TUNE_CTRL_CPU           (CTL_BASE_ANA_GLB + 0x0348)

#define ANA_REG_GLB_ARM_MODULE_EN	        ANA_REG_GLB_MODULE_EN0
#define ANA_REG_GLB_ARM_CLK_EN		        ANA_REG_GLB_ARM_CLK_EN0
#define ANA_REG_GLB_RTC_CLK_EN		        ANA_REG_GLB_RTC_CLK_EN0
#define ANA_REG_GLB_XTL_WAIT_CTRL		ANA_REG_GLB_XTL_WAIT_CTRL0

#define BITS_CHIP_ID_LOW(x)                     (((x) & 0xFFFF) << 0)

#define BITS_CHIP_ID_HIGH(x)                    (((x) & 0xFFFF) << 0)

#define BIT_TYPEC_EN                            BIT(14)
#define BIT_CHG_WDG_EN                          BIT(13)
#define BIT_TMR_EN                              BIT(12)
#define BIT_FAST_CHG_EN                         BIT(11)
#define BIT_BLTC_EN                             BIT(9)
#define BIT_PINREG_EN                           BIT(8)
#define BIT_FGU_EN                              BIT(7)
#define BIT_EFS_EN                              BIT(6)
#define BIT_ADC_EN                              BIT(5)
#define BIT_AUD_EN                              BIT(4)
#define BIT_EIC_EN                              BIT(3)
#define BIT_WDG_EN                              BIT(2)
#define BIT_RTC_EN                              BIT(1)
#define BIT_CAL_EN                              BIT(0)

#define BIT_ANA_BLTC_EN                  BIT_BLTC_EN
#define BIT_ANA_PINREG_EN                BIT_PINREG_EN
#define BIT_ANA_FGU_EN                   BIT_FGU_EN
#define BIT_ANA_EFS_EN                   BIT_EFS_EN
#define BIT_ANA_ADC_EN                   BIT_ADC_EN
#define BIT_ANA_AUD_EN                   BIT_AUD_EN
#define BIT_ANA_EIC_EN                   BIT_EIC_EN
#define BIT_ANA_WDG_EN                   BIT_WDG_EN
#define BIT_ANA_RTC_EN                   BIT_RTC_EN
#define BIT_ANA_CAL_EN                   BIT_CAL_EN

#define BIT_CLK_TSEN_ADC_EN                     BIT(8)
#define BIT_CLK_AUXAD_EN                        BIT(6)
#define BIT_CLK_AUXADC_EN                       BIT(5)
#define BITS_CLK_CAL_SRC_SEL(x)                 (((x) & 0x3) << 3)
#define BIT_CLK_CAL_EN                          BIT(2)
#define BIT_CLK_AUD_IF_6P5M_EN                  BIT(1)
#define BIT_CLK_AUD_IF_EN                       BIT(0)

#define BIT_RTC_TYPEC_EN                        BIT(15)
#define BIT_RTC_CHG_WDG_EN                      BIT(14)
#define BIT_RTC_TMR_EN                          BIT(13)
#define BIT_RTC_FLASH_EN                        BIT(12)
#define BIT_RTC_EFS_EN                          BIT(11)
#define BIT_RTC_BLTC_EN                         BIT(7)
#define BIT_RTC_FGU_EN                          BIT(6)
#define BIT_RTC_FAST_CHG_EN                     BIT(4)
#define BIT_RTC_EIC_EN                          BIT(3)
#define BIT_RTC_WDG_EN                          BIT(2)
#define BIT_RTC_RTC_EN                          BIT(1)
#define BIT_RTC_ARCH_EN                         BIT(0)

#define BIT_AUDRX_SOFT_RST                      BIT(13)
#define BIT_AUDTX_SOFT_RST                      BIT(12)
#define BIT_BLTC_SOFT_RST                       BIT(9)
#define BIT_AUD_IF_SOFT_RST                     BIT(8)
#define BIT_EFS_SOFT_RST                        BIT(7)
#define BIT_ADC_SOFT_RST                        BIT(6)
#define BIT_FGU_SOFT_RST                        BIT(4)
#define BIT_EIC_SOFT_RST                        BIT(3)
#define BIT_WDG_SOFT_RST                        BIT(2)
#define BIT_RTC_SOFT_RST                        BIT(1)
#define BIT_CAL_SOFT_RST                        BIT(0)

#define BIT_ANA_AUDTX_SOFT_RST           BIT_AUDTX_SOFT_RST
#define BIT_ANA_AUDRX_SOFT_RST           BIT_AUDRX_SOFT_RST
#define BIT_ANA_EFS_SOFT_RST             BIT_EFS_SOFT_RST
#define BIT_ANA_ADC_SOFT_RST             BIT_ADC_SOFT_RST
#define BIT_ANA_FGU_SOFT_RST             BIT_FGU_SOFT_RST
#define BIT_ANA_EIC_SOFT_RST             BIT_EIC_SOFT_RST
#define BIT_ANA_WDG_SOFT_RST             BIT_WDG_SOFT_RST
#define BIT_ANA_RTC_SOFT_RST             BIT_RTC_SOFT_RST
#define BIT_ANA_CAL_SOFT_RST             BIT_CAL_SOFT_RST

#define BIT_TYPEC_SOFT_RST                      BIT(3)
#define BIT_CHG_WDG_SOFT_RST                    BIT(2)
#define BIT_TMR_SOFT_RST                        BIT(1)
#define BIT_FAST_CHG_SOFT_RST                   BIT(0)

#define BIT_LDO_DCXO_PD                         BIT(10)
#define BIT_LDO_EMM_PD                          BIT(9)
#define BIT_DCDC_TOPCLK6M_PD                    BIT(8)
#define BIT_DCDC_GEN_PD                         BIT(7)
#define BIT_DCDC_MEM_PD                         BIT(6)
#define BIT_DCDC_CORE_PD                        BIT(5)
#define BIT_DCDC_CPU_PD                         BIT(4)
#define BIT_LDO_MEM_PD                          BIT(3)
#define BIT_LDO_AVDD18_PD                       BIT(2)
#define BIT_LDO_VDD28_PD                        BIT(1)
#define BIT_BG_PD                               BIT(0)

#define BIT_PWR_OFF_SEQ_EN                      BIT(0)


#define BIT_REG_SOFT_RST                        BIT(0)


#define BITS_DCDC_CPU_DEADTIME(x)               (((x) & 0x3) << 14)
#define BITS_DCDC_CPU_PDRSLOW(x)                (((x) & 0xF) << 4)
#define BIT_DCDC_CPU_QKRSPS                     BIT(3)
#define BIT_DCDC_CPU_PFMB                       BIT(2)
#define BIT_DCDC_CPU_DCMB                       BIT(1)


#define BIT_DCDC_CPU_OSCSYCEN_SW                BIT(15)
#define BIT_DCDC_CPU_OSCSYCEN_HW_EN             BIT(14)
#define BIT_DCDC_CPU_OSCSYC_DIV_EN              BIT(13)
#define BITS_DCDC_CPU_OSCSYC_DIV(x)             (((x) & 0x1F) << 8)
#define BITS_DCDC_CPU_DISCHRG(x)                (((x) & 0x3) << 6)
#define BITS_DCDC_CPU_CF(x)                     (((x) & 0x3) << 4)
#define BITS_DCDC_CPU_CL_CTRL(x)                (((x) & 0x3) << 0)



#define BITS_DCDC_CPU_STBOP(x)                  (((x) & 0x7F) << 6)
#define BITS_DCDC_CPU_ZCD(x)                    (((x) & 0x3) << 4)
#define BITS_DCDC_CPU_PFMAD(x)                  (((x) & 0xF) << 0)


#define BITS_DCDC_CPU_CTRL(x)                   (((x) & 0x1F) << 5)
#define BITS_DCDC_CPU_CAL(x)                    (((x) & 0x1F) << 0)



#define BITS_DCDC_CORE_DEADTIME(x)              (((x) & 0x3) << 14)
#define BITS_DCDC_CORE_PDRSLOW(x)               (((x) & 0xF) << 4)
#define BIT_DCDC_CORE_QKRSPS                    BIT(3)
#define BIT_DCDC_CORE_PFMB                      BIT(2)
#define BIT_DCDC_CORE_DCMB                      BIT(1)



#define BIT_DCDC_CORE_OSCSYCEN_SW               BIT(15)
#define BIT_DCDC_CORE_OSCSYCEN_HW_EN            BIT(14)
#define BIT_DCDC_CORE_OSCSYC_DIV_EN             BIT(13)
#define BITS_DCDC_CORE_OSCSYC_DIV(x)            (((x) & 0x1F) << 8)
#define BITS_DCDC_CORE_DISCHRG(x)               (((x) & 0x3) << 6)
#define BITS_DCDC_CORE_CF(x)                    (((x) & 0x3) << 4)
#define BITS_DCDC_CORE_CL_CTRL(x)               (((x) & 0x3) << 0)



#define BITS_DCDC_CORE_STBOP(x)                 (((x) & 0x7F) << 6)
#define BITS_DCDC_CORE_ZCD(x)                   (((x) & 0x3) << 4)
#define BITS_DCDC_CORE_PFMAD(x)                 (((x) & 0xF) << 0)



#define BITS_DCDC_CORE_CTRL(x)                  (((x) & 0x1F) << 5)
#define BITS_DCDC_CORE_CAL(x)                   (((x) & 0x1F) << 0)



#define BITS_DCDC_MEM_DEADTIME(x)               (((x) & 0x3) << 14)
#define BITS_DCDC_MEM_PDRSLOW(x)                (((x) & 0xF) << 4)
#define BIT_DCDC_MEM_QKRSPS                     BIT(3)
#define BIT_DCDC_MEM_PFMB                       BIT(2)
#define BIT_DCDC_MEM_DCMB                       BIT(1)



#define BITS_DCDC_MEM_DISCHRG(x)                (((x) & 0x3) << 14)
#define BIT_DCDC_MEM_OSCSYCEN_SW                BIT(13)
#define BIT_DCDC_MEM_OSCSYCEN_HW_EN             BIT(12)
#define BIT_DCDC_MEM_OSCSYC_DIV_EN              BIT(11)
#define BITS_DCDC_MEM_OSCSYC_DIV(x)             (((x) & 0x1F) << 6)
#define BITS_DCDC_MEM_CF(x)                     (((x) & 0x3) << 4)
#define BITS_DCDC_MEM_CL_CTRL(x)                (((x) & 0x3) << 0)



#define BITS_DCDC_MEM_STBOP(x)                  (((x) & 0x7F) << 6)
#define BITS_DCDC_MEM_ZCD(x)                    (((x) & 0x3) << 4)
#define BITS_DCDC_MEM_PFMAD(x)                  (((x) & 0xF) << 0)



#define BITS_DCDC_MEM_CTRL(x)                   (((x) & 0x1F) << 5)
#define BITS_DCDC_MEM_CAL(x)                    (((x) & 0x1F) << 0)



#define BITS_DCDC_GEN_DEADTIME(x)               (((x) & 0x3) << 14)
#define BITS_DCDC_GEN_PDRSLOW(x)                (((x) & 0xF) << 4)
#define BIT_DCDC_GEN_QKRSPS                     BIT(3)
#define BIT_DCDC_GEN_PFMB                       BIT(2)
#define BIT_DCDC_GEN_DCMB                       BIT(1)



#define BITS_DCDC_GEN_DISCHRG(x)                (((x) & 0x3) << 14)
#define BIT_DCDC_GEN_OSCSYCEN_SW                BIT(13)
#define BIT_DCDC_GEN_OSCSYCEN_HW_EN             BIT(12)
#define BIT_DCDC_GEN_OSCSYC_DIV_EN              BIT(11)
#define BITS_DCDC_GEN_OSCSYC_DIV(x)             (((x) & 0x1F) << 6)
#define BITS_DCDC_GEN_CF(x)                     (((x) & 0x3) << 4)
#define BITS_DCDC_GEN_CL_CTRL(x)                (((x) & 0x3) << 0)



#define BITS_DCDC_GEN_STBOP(x)                  (((x) & 0x7F) << 6)
#define BITS_DCDC_GEN_ZCD(x)                    (((x) & 0x3) << 4)
#define BITS_DCDC_GEN_PFMAD(x)                  (((x) & 0xF) << 0)



#define BITS_DCDC_GEN_CTRL(x)                   (((x) & 0x1F) << 5)
#define BITS_DCDC_GEN_CAL(x)                    (((x) & 0x1F) << 0)



#define BITS_DCDC_WPA_DEADTIME(x)               (((x) & 0x3) << 14)
#define BITS_DCDC_WPA_PDRSLOW(x)                (((x) & 0xF) << 4)
#define BIT_DCDC_WPA_QKRSPS                     BIT(3)
#define BIT_DCDC_WPA_PFMB                       BIT(2)
#define BIT_DCDC_WPA_APT2P5XEN                  BIT(1)



#define BIT_DCDC_WPA_BPOUT_SOFTW                BIT(15)
#define BITS_DCDC_WPA_VBAT_DIV(x)               (((x) & 0x7) << 12)
#define BIT_DCDC_WPA_BPEN                       BIT(11)
#define BIT_DCDC_WPA_BPMODE                     BIT(10)
#define BIT_DCDC_WPA_DEGEN                      BIT(9)
#define BIT_DCDC_WPA_APTEN                      BIT(8)
#define BITS_DCDC_WPA_DEBC_SEL(x)               (((x) & 0x3) << 6)
#define BITS_DCDC_WPA_CF(x)                     (((x) & 0x3) << 4)
#define BITS_DCDC_WPA_CL_CTRL(x)                (((x) & 0x3) << 0)



#define BIT_DCDC_WPA_PD                         BIT(13)
#define BIT_DCDC_WPA_OSCSYCEN_SW                BIT(12)
#define BIT_DCDC_WPA_OSCSYCEN_HW_EN             BIT(11)
#define BIT_DCDC_WPA_OSCSYC_DIV_EN              BIT(10)
#define BITS_DCDC_WPA_OSCSYC_DIV(x)             (((x) & 0x1F) << 5)
#define BITS_DCDC_WPA_DISCHRG(x)                (((x) & 0x3) << 3)



#define BITS_DCDC_WPA_STBOP(x)                  (((x) & 0x7F) << 6)
#define BITS_DCDC_WPA_ZCD(x)                    (((x) & 0x3) << 4)
#define BITS_DCDC_WPA_PFMAD(x)                  (((x) & 0xF) << 0)


#define BITS_DCDC_WPA_STBOP(x)                  (((x) & 0x7F) << 6)
#define BITS_DCDC_WPA_ZCD(x)                    (((x) & 0x3) << 4)
#define BITS_DCDC_WPA_PFMAD(x)                  (((x) & 0xF) << 0)



#define BITS_DCDC_WPA_CTRL(x)                   (((x) & 0x1F) << 5)
#define BITS_DCDC_WPA_CAL(x)                    (((x) & 0x1F) << 0)



#define BIT_DCDC_WPA_DCMB_HW                    BIT(0)



#define BITS_DCDC_CAL_SEL(x)                    (((x) & 0xF) << 12)



#define BITS_DCDC_ILIMITCAL(x)                  (((x) & 0x1F) << 6)
#define BIT_DCDC_CLSEL                          BIT(5)
#define BITS_DCDC_VREF_CAL(x)                   (((x) & 0x1F) << 0)



#define BIT_DCDC_3MCLKCAL_EN                    BIT(13)
#define BIT_DCDC_2MCLKCAL_EN                    BIT(12)
#define BITS_DCDC_6MFRECAL_SW(x)                (((x) & 0x1F) << 7)
#define BITS_DCDC_4MFRECAL_SW(x)                (((x) & 0x1F) << 2)
#define BIT_DCDC_CLK_SP_SEL                     BIT(1)
#define BIT_DCDC_CLK_SP_EN                      BIT(0)


#define BITS_LDO_CAMA_REFTRIM(x)                (((x) & 0x1F) << 3)
#define BIT_LDO_CAMA_EADBIAS_EN                 BIT(2)
#define BIT_LDO_CAMA_SHPT_PD                    BIT(1)
#define BIT_LDO_CAMA_PD                         BIT(0)



#define BITS_LDO_CAMA_V(x)                      (((x) & 0xFF) << 0)



#define BITS_LDO_CAMMOT_REFTRIM(x)              (((x) & 0x1F) << 3)
#define BIT_LDO_CAMMOT_EADBIAS_EN               BIT(2)
#define BIT_LDO_CAMMOT_SHPT_PD                  BIT(1)
#define BIT_LDO_CAMMOT_PD                       BIT(0)



#define BITS_LDO_CAMMOT_V(x)                    (((x) & 0xFF) << 0)



#define BITS_LDO_SIM0_REFTRIM(x)                (((x) & 0x1F) << 3)
#define BIT_LDO_SIM0_EADBIAS_EN                 BIT(2)
#define BIT_LDO_SIM0_SHPT_PD                    BIT(1)



#define BIT_LDO_SIM0_PD                         BIT(0)



#define BITS_LDO_SIM0_V(x)                      (((x) & 0xFF) << 0)



#define BITS_LDO_SIM1_REFTRIM(x)                (((x) & 0x1F) << 3)
#define BIT_LDO_SIM1_EADBIAS_EN                 BIT(2)
#define BIT_LDO_SIM1_SHPT_PD                    BIT(1)



#define BIT_LDO_SIM1_PD                         BIT(0)



#define BITS_LDO_SIM1_V(x)                      (((x) & 0xFF) << 0)



#define BITS_LDO_SIM2_REFTRIM(x)                (((x) & 0x1F) << 3)
#define BIT_LDO_SIM2_EADBIAS_EN                 BIT(2)
#define BIT_LDO_SIM2_SHPT_PD                    BIT(1)



#define BIT_LDO_SIM2_PD                         BIT(0)



#define BITS_LDO_SIM2_V(x)                      (((x) & 0xFF) << 0)



#define BITS_LDO_EMMCCORE_REFTRIM(x)            (((x) & 0x1F) << 3)
#define BIT_LDO_EMMCCORE_EADBIAS_EN             BIT(2)
#define BIT_LDO_EMMCCORE_SHPT_PD                BIT(1)



#define BIT_LDO_EMMCCORE_PD                     BIT(0)



#define BITS_LDO_EMMCCORE_V(x)                  (((x) & 0xFF) << 0)



#define BITS_LDO_SDCORE_REFTRIM(x)              (((x) & 0x1F) << 3)
#define BIT_LDO_SDCORE_EADBIAS_EN               BIT(2)
#define BIT_LDO_SDCORE_SHPT_PD                  BIT(1)



#define BIT_LDO_SDCORE_PD                       BIT(0)



#define BITS_LDO_SDCORE_V(x)                    (((x) & 0xFF) << 0)



#define BITS_LDO_SDIO_REFTRIM(x)                (((x) & 0x1F) << 3)
#define BIT_LDO_SDIO_EADBIAS_EN                 BIT(2)
#define BIT_LDO_SDIO_SHPT_PD                    BIT(1)



#define BIT_LDO_SDIO_PD                         BIT(0)



#define BITS_LDO_SDIO_V(x)                      (((x) & 0xFF) << 0)



#define BITS_LDO_VDD28_REFTRIM(x)               (((x) & 0x1F) << 3)
#define BIT_LDO_VDD28_EADBIAS_EN                BIT(2)
#define BIT_LDO_VDD28_SHPT_PD                   BIT(1)



#define BITS_LDO_VDD28_V(x)                     (((x) & 0xFF) << 0)



#define BITS_LDO_WIFIPA_REFTRIM(x)              (((x) & 0x1F) << 3)
#define BIT_LDO_WIFIPA_EADBIAS_EN               BIT(2)
#define BIT_LDO_WIFIPA_SHPT_PD                  BIT(1)
#define BIT_LDO_WIFIPA_PD                       BIT(0)



#define BITS_LDO_WIFIPA_V(x)                    (((x) & 0xFF) << 0)



#define BITS_LDO_DCXO_REFTRIM(x)                (((x) & 0x1F) << 3)
#define BIT_LDO_DCXO_EADBIAS_EN                 BIT(2)
#define BIT_LDO_DCXO_SHPT_PD                    BIT(1)



#define BITS_LDO_DCXO_V(x)                      (((x) & 0xFF) << 0)



#define BITS_LDO_USB33_REFTRIM(x)               (((x) & 0x1F) << 3)
#define BIT_LDO_USB33_EADBIAS_EN                BIT(2)
#define BIT_LDO_USB33_SHPT_PD                   BIT(1)



#define BIT_LDO_USB33_PD                        BIT(0)



#define BITS_LDO_USB33_V(x)                     (((x) & 0xFF) << 0)



#define BITS_LDO_CAMD_REFTRIM(x)                (((x) & 0x1F) << 3)
#define BIT_LDO_CAMD_EADBIAS_EN                 BIT(2)
#define BIT_LDO_CAMD_SHPT_PD                    BIT(1)
#define BIT_LDO_CAMD_PD                         BIT(0)


#define BITS_LDO_CAMD_V(x)                      (((x) & 0x7F) << 0)



#define BITS_LDO_CON_REFTRIM(x)                 (((x) & 0x1F) << 3)
#define BIT_LDO_CON_EADBIAS_EN                  BIT(2)
#define BIT_LDO_CON_SHPT_PD                     BIT(1)
#define BIT_LDO_CON_PD                          BIT(0)



#define BITS_LDO_CON_V(x)                       (((x) & 0x7F) << 0)



#define BITS_LDO_CAMIO_REFTRIM(x)               (((x) & 0x1F) << 3)
#define BIT_LDO_CAMIO_EADBIAS_EN                BIT(2)
#define BIT_LDO_CAMIO_SHPT_PD                   BIT(1)
#define BIT_LDO_CAMIO_PD                        BIT(0)



#define BITS_LDO_CAMIO_V(x)                     (((x) & 0x7F) << 0)



#define BITS_LDO_AVDD18_REFTRIM(x)              (((x) & 0x1F) << 3)
#define BIT_LDO_AVDD18_EADBIAS_EN               BIT(2)
#define BIT_LDO_AVDD18_SHPT_PD                  BIT(1)



#define BITS_LDO_AVDD18_V(x)                    (((x) & 0x7F) << 0)



#define BITS_LDO_RF18_REFTRIM(x)                (((x) & 0x1F) << 3)
#define BIT_LDO_RF18_EADBIAS_EN                 BIT(2)
#define BIT_LDO_RF18_SHPT_PD                    BIT(1)
#define BIT_LDO_RF18_PD                         BIT(0)



#define BITS_LDO_RF18_V(x)                      (((x) & 0x7F) << 0)



#define BITS_LDO_RF15_REFTRIM(x)                (((x) & 0x1F) << 3)
#define BIT_LDO_RF15_EADBIAS_EN                 BIT(2)
#define BIT_LDO_RF15_SHPT_PD                    BIT(1)
#define BIT_LDO_RF15_PD                         BIT(0)



#define BITS_LDO_RF15_V(x)                      (((x) & 0x7F) << 0)



#define BITS_LDO_MEM_REFTRIM(x)                 (((x) & 0x1F) << 3)
#define BIT_LDO_MEM_EADBIAS_EN                  BIT(2)
#define BIT_LDO_MEM_SHPT_PD                     BIT(1)



#define BITS_LDO_MEM_V(x)                       (((x) & 0x7F) << 0)


















#define BITS_LDO_RTC_CAL(x)                     (((x) & 0x1F) << 4)
#define BITS_LDO_RTC_V(x)                       (((x) & 0x3) << 2)
#define BITS_VBATBK_V(x)                        (((x) & 0x3) << 0)



#define BITS_LDOB_CAL_SEL(x)                    (((x) & 0x7) << 8)
#define BITS_LDOA_CAL_SEL(x)                    (((x) & 0x7) << 5)
#define BITS_LDODCDC_CAL_SEL(x)                 (((x) & 0x7) << 0)


















#define BITS_SLP_IN_WAIT_DCDCCPU(x)             (((x) & 0xFF) << 8)
#define BITS_SLP_OUT_WAIT_DCDCCPU(x)            (((x) & 0xFF) << 0)



#define BIT_LDO_XTL_EN                          BIT(2)
#define BIT_SLP_IO_EN                           BIT(1)
#define BIT_SLP_LDO_PD_EN                       BIT(0)



#define BIT_SLP_DCDCCORE_DROP_EN                BIT(3)
#define BIT_SLP_DCDCWPA_PD_EN                   BIT(2)
#define BIT_SLP_DCDCCPU_PD_EN                   BIT(1)
#define BIT_SLP_DCDCMEM_PD_EN                   BIT(0)



#define BIT_SLP_LDORF15_PD_EN                   BIT(14)
#define BIT_SLP_LDORF18_PD_EN                   BIT(13)
#define BIT_SLP_LDOEMMCCORE_PD_EN               BIT(12)
#define BIT_SLP_LDODCXO_PD_EN                   BIT(11)
#define BIT_SLP_LDOWIFIPA_PD_EN                 BIT(10)
#define BIT_SLP_LDOVDD28_PD_EN                  BIT(9)
#define BIT_SLP_LDOSDCORE_PD_EN                 BIT(8)
#define BIT_SLP_LDOSDIO_PD_EN                   BIT(7)
#define BIT_SLP_LDOUSB33_PD_EN                  BIT(6)
#define BIT_SLP_LDOCAMMOT_PD_EN                 BIT(5)
#define BIT_SLP_LDOCAMIO_PD_EN                  BIT(4)
#define BIT_SLP_LDOCAMD_PD_EN                   BIT(3)
#define BIT_SLP_LDOCAMA_PD_EN                   BIT(2)
#define BIT_SLP_LDOSIM2_PD_EN                   BIT(1)
#define BIT_SLP_LDOSIM1_PD_EN                   BIT(0)



#define BIT_SLP_LDOCON_PD_EN                    BIT(3)
#define BIT_SLP_LDOSIM0_PD_EN                   BIT(2)
#define BIT_SLP_LDOAVDD18_PD_EN                 BIT(1)
#define BIT_SLP_LDOMEM_PD_EN                    BIT(0)



#define BIT_SLP_DCDCCORE_LP_EN                  BIT(4)
#define BIT_SLP_DCDCMEM_LP_EN                   BIT(3)
#define BIT_SLP_DCDCCPU_LP_EN                   BIT(2)
#define BIT_SLP_DCDCGEN_LP_EN                   BIT(1)
#define BIT_SLP_DCDCWPA_LP_EN                   BIT(0)



#define BIT_SLP_LDORF15_LP_EN                   BIT(14)
#define BIT_SLP_LDORF18_LP_EN                   BIT(13)
#define BIT_SLP_LDOEMMCCORE_LP_EN               BIT(12)
#define BIT_SLP_LDODCXO_LP_EN                   BIT(11)
#define BIT_SLP_LDOWIFIPA_LP_EN                 BIT(10)
#define BIT_SLP_LDOVDD28_LP_EN                  BIT(9)
#define BIT_SLP_LDOSDCORE_LP_EN                 BIT(8)
#define BIT_SLP_LDOSDIO_LP_EN                   BIT(7)
#define BIT_SLP_LDOUSB33_LP_EN                  BIT(6)
#define BIT_SLP_LDOCAMMOT_LP_EN                 BIT(5)
#define BIT_SLP_LDOCAMIO_LP_EN                  BIT(4)
#define BIT_SLP_LDOCAMD_LP_EN                   BIT(3)
#define BIT_SLP_LDOCAMA_LP_EN                   BIT(2)
#define BIT_SLP_LDOSIM2_LP_EN                   BIT(1)
#define BIT_SLP_LDOSIM1_LP_EN                   BIT(0)



#define BIT_SLP_LDOCON_LP_EN                    BIT(3)
#define BIT_SLP_LDOSIM0_LP_EN                   BIT(2)
#define BIT_SLP_LDOAVDD18_LP_EN                 BIT(1)
#define BIT_SLP_LDOMEM_LP_EN                    BIT(0)



#define BIT_DCDC_CORE_SLP_OUT_STEP_EN           BIT(1)
#define BIT_DCDC_CORE_SLP_IN_STEP_EN            BIT(0)



#define BITS_DCDC_CORE_CAL_DS_SW(x)             (((x) & 0x1F) << 5)
#define BITS_DCDC_CORE_CTRL_DS_SW(x)            (((x) & 0x1F) << 0)



#define BITS_DCDC_CORE_CTRL_SLP_STEP3(x)        (((x) & 0x1F) << 10)
#define BITS_DCDC_CORE_CTRL_SLP_STEP2(x)        (((x) & 0x1F) << 5)
#define BITS_DCDC_CORE_CTRL_SLP_STEP1(x)        (((x) & 0x1F) << 0)



#define BITS_DCDC_CORE_CTRL_SLP_STEP5(x)        (((x) & 0x1F) << 5)
#define BITS_DCDC_CORE_CTRL_SLP_STEP4(x)        (((x) & 0x1F) << 0)



#define BITS_DCDC_CORE_CAL_SLP_STEP3(x)         (((x) & 0x1F) << 10)
#define BITS_DCDC_CORE_CAL_SLP_STEP2(x)         (((x) & 0x1F) << 5)
#define BITS_DCDC_CORE_CAL_SLP_STEP1(x)         (((x) & 0x1F) << 0)



#define BITS_DCDC_CORE_CAL_SLP_STEP5(x)         (((x) & 0x1F) << 5)
#define BITS_DCDC_CORE_CAL_SLP_STEP4(x)         (((x) & 0x1F) << 0)



#define BIT_DCDC_CPU_EN_CTRL                    BIT(2)
#define BIT_DCDC_CPU_SLP_OUT_STEP_EN            BIT(1)
#define BIT_DCDC_CPU_SLP_IN_STEP_EN             BIT(0)



#define BITS_DCDC_CPU_CAL_DS_SW(x)              (((x) & 0x1F) << 5)
#define BITS_DCDC_CPU_CTRL_DS_SW(x)             (((x) & 0x1F) << 0)



#define BITS_DCDC_CPU_CTRL_SLP_STEP3(x)         (((x) & 0x1F) << 10)
#define BITS_DCDC_CPU_CTRL_SLP_STEP2(x)         (((x) & 0x1F) << 5)
#define BITS_DCDC_CPU_CTRL_SLP_STEP1(x)         (((x) & 0x1F) << 0)



#define BITS_DCDC_CPU_CTRL_SLP_STEP5(x)         (((x) & 0x1F) << 5)
#define BITS_DCDC_CPU_CTRL_SLP_STEP4(x)         (((x) & 0x1F) << 0)



#define BITS_DCDC_CPU_CAL_SLP_STEP3(x)          (((x) & 0x1F) << 10)
#define BITS_DCDC_CPU_CAL_SLP_STEP2(x)          (((x) & 0x1F) << 5)
#define BITS_DCDC_CPU_CAL_SLP_STEP1(x)          (((x) & 0x1F) << 0)



#define BITS_DCDC_CPU_CAL_SLP_STEP5(x)          (((x) & 0x1F) << 5)
#define BITS_DCDC_CPU_CAL_SLP_STEP4(x)          (((x) & 0x1F) << 0)



#define BIT_DCDC_CORE_EXT_XTL0_EN               BIT(15)
#define BIT_DCDC_CORE_EXT_XTL1_EN               BIT(14)
#define BIT_DCDC_CORE_EXT_XTL2_EN               BIT(13)
#define BIT_DCDC_CORE_EXT_XTL3_EN               BIT(12)
#define BIT_DCDC_WPA_EXT_XTL0_EN                BIT(3)
#define BIT_DCDC_WPA_EXT_XTL1_EN                BIT(2)
#define BIT_DCDC_WPA_EXT_XTL2_EN                BIT(1)
#define BIT_DCDC_WPA_EXT_XTL3_EN                BIT(0)



#define BIT_DCDC_CPU_EXT_XTL0_EN                BIT(15)
#define BIT_DCDC_CPU_EXT_XTL1_EN                BIT(14)
#define BIT_DCDC_CPU_EXT_XTL2_EN                BIT(13)
#define BIT_DCDC_CPU_EXT_XTL3_EN                BIT(12)



#define BIT_DCDC_MEM_EXT_XTL0_EN                BIT(15)
#define BIT_DCDC_MEM_EXT_XTL1_EN                BIT(14)
#define BIT_DCDC_MEM_EXT_XTL2_EN                BIT(13)
#define BIT_DCDC_MEM_EXT_XTL3_EN                BIT(12)
#define BIT_DCDC_GEN_EXT_XTL0_EN                BIT(3)
#define BIT_DCDC_GEN_EXT_XTL1_EN                BIT(2)
#define BIT_DCDC_GEN_EXT_XTL2_EN                BIT(1)
#define BIT_DCDC_GEN_EXT_XTL3_EN                BIT(0)






#define BIT_LDO_DCXO_EXT_XTL0_EN                BIT(15)
#define BIT_LDO_DCXO_EXT_XTL1_EN                BIT(14)
#define BIT_LDO_DCXO_EXT_XTL2_EN                BIT(13)
#define BIT_LDO_DCXO_EXT_XTL3_EN                BIT(12)
#define BIT_LDO_VDD28_EXT_XTL0_EN               BIT(3)
#define BIT_LDO_VDD28_EXT_XTL1_EN               BIT(2)
#define BIT_LDO_VDD28_EXT_XTL2_EN               BIT(1)
#define BIT_LDO_VDD28_EXT_XTL3_EN               BIT(0)



#define BIT_LDO_RF15_EXT_XTL0_EN                BIT(15)
#define BIT_LDO_RF15_EXT_XTL1_EN                BIT(14)
#define BIT_LDO_RF15_EXT_XTL2_EN                BIT(13)
#define BIT_LDO_RF15_EXT_XTL3_EN                BIT(12)
#define BIT_LDO_RF18_EXT_XTL0_EN                BIT(3)
#define BIT_LDO_RF18_EXT_XTL1_EN                BIT(2)
#define BIT_LDO_RF18_EXT_XTL2_EN                BIT(1)
#define BIT_LDO_RF18_EXT_XTL3_EN                BIT(0)



#define BIT_LDO_SIM0_EXT_XTL0_EN                BIT(15)
#define BIT_LDO_SIM0_EXT_XTL1_EN                BIT(14)
#define BIT_LDO_SIM0_EXT_XTL2_EN                BIT(13)
#define BIT_LDO_SIM0_EXT_XTL3_EN                BIT(12)
#define BIT_LDO_SIM1_EXT_XTL0_EN                BIT(3)
#define BIT_LDO_SIM1_EXT_XTL1_EN                BIT(2)
#define BIT_LDO_SIM1_EXT_XTL2_EN                BIT(1)
#define BIT_LDO_SIM1_EXT_XTL3_EN                BIT(0)



#define BIT_LDO_SIM2_EXT_XTL0_EN                BIT(15)
#define BIT_LDO_SIM2_EXT_XTL1_EN                BIT(14)
#define BIT_LDO_SIM2_EXT_XTL2_EN                BIT(13)
#define BIT_LDO_SIM2_EXT_XTL3_EN                BIT(12)
#define BIT_LDO_MEM_EXT_XTL0_EN                 BIT(3)
#define BIT_LDO_MEM_EXT_XTL1_EN                 BIT(2)
#define BIT_LDO_MEM_EXT_XTL2_EN                 BIT(1)
#define BIT_LDO_MEM_EXT_XTL3_EN                 BIT(0)



#define BIT_LDO_CAMMOT_EXT_XTL0_EN              BIT(15)
#define BIT_LDO_CAMMOT_EXT_XTL1_EN              BIT(14)
#define BIT_LDO_CAMMOT_EXT_XTL2_EN              BIT(13)
#define BIT_LDO_CAMMOT_EXT_XTL3_EN              BIT(12)
#define BIT_LDO_CAMIO_EXT_XTL0_EN               BIT(3)
#define BIT_LDO_CAMIO_EXT_XTL1_EN               BIT(2)
#define BIT_LDO_CAMIO_EXT_XTL2_EN               BIT(1)
#define BIT_LDO_CAMIO_EXT_XTL3_EN               BIT(0)



#define BIT_LDO_CAMA_EXT_XTL0_EN                BIT(15)
#define BIT_LDO_CAMA_EXT_XTL1_EN                BIT(14)
#define BIT_LDO_CAMA_EXT_XTL2_EN                BIT(13)
#define BIT_LDO_CAMA_EXT_XTL3_EN                BIT(12)
#define BIT_LDO_CAMD_EXT_XTL0_EN                BIT(3)
#define BIT_LDO_CAMD_EXT_XTL1_EN                BIT(2)
#define BIT_LDO_CAMD_EXT_XTL2_EN                BIT(1)
#define BIT_LDO_CAMD_EXT_XTL3_EN                BIT(0)



#define BIT_LDO_SDIO_EXT_XTL0_EN                BIT(15)
#define BIT_LDO_SDIO_EXT_XTL1_EN                BIT(14)
#define BIT_LDO_SDIO_EXT_XTL2_EN                BIT(13)
#define BIT_LDO_SDIO_EXT_XTL3_EN                BIT(12)
#define BIT_LDO_SDCORE_EXT_XTL0_EN              BIT(3)
#define BIT_LDO_SDCORE_EXT_XTL1_EN              BIT(2)
#define BIT_LDO_SDCORE_EXT_XTL2_EN              BIT(1)
#define BIT_LDO_SDCORE_EXT_XTL3_EN              BIT(0)



#define BIT_LDO_EMMCCORE_EXT_XTL0_EN            BIT(15)
#define BIT_LDO_EMMCCORE_EXT_XTL1_EN            BIT(14)
#define BIT_LDO_EMMCCORE_EXT_XTL2_EN            BIT(13)
#define BIT_LDO_EMMCCORE_EXT_XTL3_EN            BIT(12)
#define BIT_LDO_USB33_EXT_XTL0_EN               BIT(3)
#define BIT_LDO_USB33_EXT_XTL1_EN               BIT(2)
#define BIT_LDO_USB33_EXT_XTL2_EN               BIT(1)
#define BIT_LDO_USB33_EXT_XTL3_EN               BIT(0)



#define BIT_LDO_KPLED_EXT_XTL0_EN               BIT(15)
#define BIT_LDO_KPLED_EXT_XTL1_EN               BIT(14)
#define BIT_LDO_KPLED_EXT_XTL2_EN               BIT(13)
#define BIT_LDO_KPLED_EXT_XTL3_EN               BIT(12)
#define BIT_LDO_VIBR_EXT_XTL0_EN                BIT(3)
#define BIT_LDO_VIBR_EXT_XTL1_EN                BIT(2)
#define BIT_LDO_VIBR_EXT_XTL2_EN                BIT(1)
#define BIT_LDO_VIBR_EXT_XTL3_EN                BIT(0)



#define BIT_LDO_CON_EXT_XTL0_EN                 BIT(15)
#define BIT_LDO_CON_EXT_XTL1_EN                 BIT(14)
#define BIT_LDO_CON_EXT_XTL2_EN                 BIT(13)
#define BIT_LDO_CON_EXT_XTL3_EN                 BIT(12)
#define BIT_LDO_AVDD18_EXT_XTL0_EN              BIT(3)
#define BIT_LDO_AVDD18_EXT_XTL1_EN              BIT(2)
#define BIT_LDO_AVDD18_EXT_XTL2_EN              BIT(1)
#define BIT_LDO_AVDD18_EXT_XTL3_EN              BIT(0)



#define BIT_LDO_WIFIPA_EXT_XTL0_EN              BIT(3)
#define BIT_LDO_WIFIPA_EXT_XTL1_EN              BIT(2)
#define BIT_LDO_WIFIPA_EXT_XTL2_EN              BIT(1)
#define BIT_LDO_WIFIPA_EXT_XTL3_EN              BIT(0)



#define BIT_TSX_XO_EXT_XTL0_EN                  BIT(15)
#define BIT_TSX_XO_EXT_XTL1_EN                  BIT(14)
#define BIT_TSX_XO_EXT_XTL2_EN                  BIT(13)
#define BIT_TSX_XO_EXT_XTL3_EN                  BIT(12)
#define BIT_XO_EXT_XTL0_EN                      BIT(3)
#define BIT_XO_EXT_XTL1_EN                      BIT(2)
#define BIT_XO_EXT_XTL2_EN                      BIT(1)
#define BIT_XO_EXT_XTL3_EN                      BIT(0)


















#define BIT_DCXO_26M_REF_OUT3_PIN_EXT_XTL3_EN   BIT(15)
#define BIT_DCXO_26M_REF_OUT2_PIN_EXT_XTL3_EN   BIT(14)
#define BIT_DCXO_26M_REF_OUT1_PIN_EXT_XTL3_EN   BIT(13)
#define BIT_DCXO_26M_REF_OUT0_PIN_EXT_XTL3_EN   BIT(12)
#define BIT_DCXO_26M_REF_OUT3_PIN_EXT_XTL2_EN   BIT(11)
#define BIT_DCXO_26M_REF_OUT2_PIN_EXT_XTL2_EN   BIT(10)
#define BIT_DCXO_26M_REF_OUT1_PIN_EXT_XTL2_EN   BIT(9)
#define BIT_DCXO_26M_REF_OUT0_PIN_EXT_XTL2_EN   BIT(8)
#define BIT_SLP_DCXO_26M_REF_OUT3_EN            BIT(7)
#define BIT_SLP_DCXO_26M_REF_OUT2_EN            BIT(6)
#define BIT_SLP_DCXO_26M_REF_OUT1_EN            BIT(5)
#define BIT_SLP_DCXO_26M_REF_OUT0_EN            BIT(4)
#define BIT_DCXO_26M_REF_OUT3_EN_SW             BIT(3)
#define BIT_DCXO_26M_REF_OUT2_EN_SW             BIT(2)
#define BIT_DCXO_26M_REF_OUT1_EN_SW             BIT(1)
#define BIT_DCXO_26M_REF_OUT0_EN_SW             BIT(0)

#define BIT_DCXO_26M_REF_OUT3_EXT_XTL3_EN       BIT(15)
#define BIT_DCXO_26M_REF_OUT2_EXT_XTL3_EN       BIT(14)
#define BIT_DCXO_26M_REF_OUT1_EXT_XTL3_EN       BIT(13)
#define BIT_DCXO_26M_REF_OUT0_EXT_XTL3_EN       BIT(12)
#define BIT_DCXO_26M_REF_OUT3_EXT_XTL2_EN       BIT(11)
#define BIT_DCXO_26M_REF_OUT2_EXT_XTL2_EN       BIT(10)
#define BIT_DCXO_26M_REF_OUT1_EXT_XTL2_EN       BIT(9)
#define BIT_DCXO_26M_REF_OUT0_EXT_XTL2_EN       BIT(8)
#define BIT_DCXO_26M_REF_OUT3_EXT_XTL1_EN       BIT(7)
#define BIT_DCXO_26M_REF_OUT2_EXT_XTL1_EN       BIT(6)
#define BIT_DCXO_26M_REF_OUT1_EXT_XTL1_EN       BIT(5)
#define BIT_DCXO_26M_REF_OUT0_EXT_XTL1_EN       BIT(4)
#define BIT_DCXO_26M_REF_OUT3_EXT_XTL0_EN       BIT(3)
#define BIT_DCXO_26M_REF_OUT2_EXT_XTL0_EN       BIT(2)
#define BIT_DCXO_26M_REF_OUT1_EXT_XTL0_EN       BIT(1)
#define BIT_DCXO_26M_REF_OUT0_EXT_XTL0_EN       BIT(0)



#define BIT_DCXO_LP_EN                          BIT(14)
#define BIT_SLP_DCXO_LP_EN                      BIT(13)
#define BITS_DCXO_LP_REF_CTRL(x)                (((x) & 0x7) << 10)
#define BIT_DCXO_GM_HELPER                      BIT(9)
#define BITS_DCXO_CBANK(x)                      (((x) & 0x7F) << 2)
#define BIT_DCXO_CBANKMSB                       BIT(1)
#define BIT_DCXO_BUF_MODE_SEL                   BIT(0)



#define BITS_DCXO_DIV_RATIO_FRAC_HC(x)          (((x) & 0xFFF) << 0)



#define BITS_DCXO_DIV_RATIO_FRAC_LC(x)          (((x) & 0xFFF) << 0)



#define BITS_DCXO_REF_BUF3_LEVEL_PUSH(x)        (((x) & 0x7) << 7)
#define BIT_DCXO_REF_BUF3_CCHARGE_2P            BIT(6)
#define BITS_DCXO_REF_BUF3_LEVEL_RSOURCE(x)     (((x) & 0x7) << 3)
#define BITS_DCXO_REF_BUF3_LEVEL_RCHARGE(x)     (((x) & 0x7) << 0)



#define BITS_DCXO_DIV32K_RES_SW(x)              (((x) & 0x7) << 3)
#define BITS_DCXO_OUT32K_RES_SW(x)              (((x) & 0x7) << 0)



#define BITS_DCXO_RESERVED(x)                   (((x) & 0x3F) << 10)
#define BITS_DCXO_CAL(x)                        (((x) & 0xF) << 6)
#define BIT_AML_TUNE_OK_FLAG                    BIT(5)
#define BITS_DCXO_AML_CTRL(x)                   (((x) & 0xF) << 1)
#define BIT_DCXO_AML_EN                         BIT(0)












#define BIT_TSEN_ADCLDO_EN                      BIT(15)
#define BITS_TSEN_SDADC_BIAS(x)                 (((x) & 0x3) << 13)
#define BITS_TSEN_CLKSEL(x)                     (((x) & 0x3) << 11)
#define BITS_TSEN_CHOP_CLKSEL(x)                (((x) & 0x3) << 9)
#define BITS_TSEN_ADCLDO_V(x)                   (((x) & 0xF) << 5)
#define BITS_TSEN_ADCLDOREF(x)                  (((x) & 0x1F) << 0)



#define BIT_VOL_MODE                            BIT(8)
#define BIT_DCDC_CPU_VOL_SEL                    BIT(7)
#define BIT_DCDC_CORE_VOL_SEL                   BIT(6)
#define BIT_DCDC_WPA_SW_SEL                     BIT(5)
#define BIT_DCDC_GEN_SW_SEL                     BIT(4)
#define BIT_DCDC_MEM_SW_SEL                     BIT(3)
#define BIT_DCDC_CPU_SW_SEL                     BIT(2)
#define BIT_DCDC_CORE_SLP_SW_SEL                BIT(1)
#define BIT_DCDC_CORE_NOR_SW_SEL                BIT(0)



#define BIT_LDO_SIM0_SW_SEL                     BIT(15)
#define BIT_LDO_SIM1_SW_SEL                     BIT(14)
#define BIT_LDO_RF15_SW_SEL                     BIT(13)
#define BIT_LDO_USB33_SW_SEL                    BIT(12)
#define BIT_LDO_EMMCCORE_SW_SEL                 BIT(11)
#define BIT_LDO_AVDD18_SW_SEL                   BIT(10)
#define BIT_LDO_VDD28_SW_SEL                    BIT(9)
#define BIT_LDO_DCXO_SW_SEL                     BIT(8)
#define BIT_LDO_SDCORE_SW_SEL                   BIT(7)
#define BIT_LDO_SDIO_SW_SEL                     BIT(6)
#define BIT_LDO_CAMMOT_SW_SEL                   BIT(5)
#define BIT_LDO_CAMIO_SW_SEL                    BIT(4)
#define BIT_LDO_CAMA_SW_SEL                     BIT(3)
#define BIT_LDO_RF18_SW_SEL                     BIT(2)
#define BIT_LDO_CAMD_SW_SEL                     BIT(1)



#define BIT_LDO_RTC_CAL_SW_SEL                  BIT(11)
#define BIT_DCDC_CPU_PFMAD_SW_SEL               BIT(10)
#define BIT_DCDC_CORE_PFMAD_SW_SEL              BIT(9)
#define BIT_DCDC_GEN_PFMAD_SW_SEL               BIT(8)
#define BIT_DCDC_MEM_PFMAD_SW_SEL               BIT(7)
#define BIT_DCDC_WPA_PFMAD_SW_SEL               BIT(6)
#define BIT_LDO_KPLED_SW_SEL                    BIT(5)
#define BIT_LDO_VIBR_SW_SEL                     BIT(4)
#define BIT_LDO_CON_SW_SEL                      BIT(3)
#define BIT_LDO_WIFIPA_SW_SEL                   BIT(2)
#define BIT_LDO_MEM_SW_SEL                      BIT(1)
#define BIT_LDO_SIM2_SW_SEL                     BIT(0)



#define BIT_TSX_MODE                            BIT(15)
#define BIT_RC_MODE_WR_ACK_FLAG                 BIT(14)
#define BIT_XO_LOW_CUR_FLAG                     BIT(13)
#define BIT_XO_LOW_CUR_FRC_RTCSET               BIT(12)
#define BIT_XO_LOW_CUR_FRC_RTCCLR               BIT(11)
#define BIT_RC_MODE_WR_ACK_FLAG_CLR             BIT(10)
#define BIT_XO_LOW_CUR_FLAG_CLR                 BIT(9)
#define BIT_XO_LOW_CUR_CNT_CLR                  BIT(8)
#define BIT_LDO_DCXO_LP_EN_RTCSET               BIT(7)
#define BIT_LDO_DCXO_LP_EN_RTCCLR               BIT(6)
#define BIT_SLP_XO_LOW_CUR_EN                   BIT(5)
#define BIT_RTC_MODE                            BIT(4)
#define BIT_RC_32K_SEL                          BIT(1)
#define BIT_RC_32K_EN                           BIT(0)



#define BITS_RC_MODE(x)                         (((x) & 0xFFFF) << 0)



#define BITS_XO_LOW_CUR_CNT_LOW(x)              (((x) & 0xFFFF) << 0)



#define BITS_XO_LOW_CUR_CNT_HIGH(x)             (((x) & 0xFFFF) << 0)



#define BITS_KO_AML_CAL(x)                      (((x) & 0xF) << 11)
#define BIT_KO_AML_EN                           BIT(10)
#define BITS_OSC_32K_CMP_ICTRL(x)               (((x) & 0x3) << 8)
#define BITS_RC1M_CAL(x)                        (((x) & 0x7F) << 0)



#define BIT_RSV                                 BIT(15)
#define BIT_CUR_SEL                             BIT(14)
#define BIT_EXT_XTL3_FOR_26M_EN                 BIT(13)
#define BIT_EXT_XTL2_FOR_26M_EN                 BIT(12)
#define BIT_EXT_XTL1_FOR_26M_EN                 BIT(11)
#define BIT_EXT_XTL0_FOR_26M_EN                 BIT(10)
#define BIT_SLP_XTLBUF_PD_EN                    BIT(9)
#define BIT_XTL_EN                              BIT(8)
#define BITS_XTL_WAIT(x)                        (((x) & 0xFF) << 0)



#define BITS_RGB_V(x)                           (((x) & 0x1F) << 4)
#define BIT_SLP_RGB_PD_EN                       BIT(2)
#define BIT_RGB_PD_HW_EN                        BIT(1)
#define BIT_RGB_PD_SW                           BIT(0)



#define BIT_BATDET_CUR_EN                       BIT(13)
#define BITS_BATDET_CUR_I(x)                    (((x) & 0xF) << 9)
#define BITS_IB_TRIM(x)                         (((x) & 0x7F) << 2)
#define BIT_IB_TRIM_EM_SEL                      BIT(1)
#define BIT_IB_REX_EN                           BIT(0)



#define BIT_FLASH_PON                           BIT(15)
#define BIT_FLASH_V_HW_EN                       BIT(6)
#define BITS_FLASH_V_HW_STEP(x)                 (((x) & 0x3) << 4)
#define BITS_FLASH_V_SW(x)                      (((x) & 0xF) << 0)



#define BITS_KPLED_V(x)                         (((x) & 0xF) << 12)
#define BIT_KPLED_PD                            BIT(11)
#define BIT_KPLED_PULLDOWN_EN                   BIT(10)
#define BIT_SLP_LDOKPLED_PD_EN                  BIT(9)



#define BIT_LDO_KPLED_PD                        BIT(15)
#define BITS_LDO_KPLED_V(x)                     (((x) & 0xFF) << 7)
#define BITS_LDO_KPLED_REFTRIM(x)               (((x) & 0x1F) << 2)
#define BIT_LDO_KPLED_EADBIAS_EN                BIT(1)
#define BIT_LDO_KPLED_SHPT_PD                   BIT(0)



#define BIT_LDO_VIBR_SHPT_PD                    BIT(10)
#define BIT_SLP_LDOVIBR_PD_EN                   BIT(9)
#define BIT_LDO_VIBR_PD                         BIT(8)
#define BITS_LDO_VIBR_V(x)                      (((x) & 0xFF) << 0)



#define BITS_LDO_VIBR_REFTRIM(x)                (((x) & 0x1F) << 1)
#define BIT_LDO_VIBR_EADBIAS_EN                 BIT(0)



#define BIT_CLK_AUD_IF_TX_INV_EN                BIT(3)
#define BIT_CLK_AUD_IF_RX_INV_EN                BIT(2)
#define BIT_CLK_AUD_IF_6P5M_TX_INV_EN           BIT(1)
#define BIT_CLK_AUD_IF_6P5M_RX_INV_EN           BIT(0)



#define BIT_CHGR_PD_EXT                         BIT(15)
#define BITS_CHGR_DPM(x)                        (((x) & 0x3) << 13)
#define BIT_CHGR_CC_EN                          BIT(12)
#define BIT_RECHG                               BIT(11)
#define BITS_CHGR_CV_V(x)                       (((x) & 0x3F) << 5)
#define BITS_CHGR_END_V(x)                      (((x) & 0x3) << 3)
#define BITS_CHGR_ITERM(x)                      (((x) & 0x3) << 1)
#define BIT_CHGR_PD                             BIT(0)



#define BIT_CHGR_POWER_DEVICE                   BIT(15)
#define BITS_CHGR_CC_I(x)                       (((x) & 0x1F) << 10)
#define BITS_VBAT_OVP_V(x)                      (((x) & 0xF) << 6)
#define BITS_VCHG_OVP_V(x)                      (((x) & 0x3F) << 0)



#define BIT_CHGR_INT_EN                         BIT(13)
#define BIT_NON_DCP_INT                         BIT(12)
#define BIT_CHG_DET_DONE                        BIT(11)
#define BIT_DP_LOW                              BIT(10)
#define BIT_DCP_DET                             BIT(9)
#define BIT_CHG_DET                             BIT(8)
#define BIT_SDP_INT                             BIT(7)
#define BIT_DCP_INT                             BIT(6)
#define BIT_CDP_INT                             BIT(5)
#define BIT_CHGR_CV_STATUS                      BIT(4)
#define BIT_CHGR_ON                             BIT(3)
#define BIT_CHGR_INT                            BIT(2)
#define BIT_VBAT_OVI                            BIT(1)
#define BIT_VCHG_OVI                            BIT(0)



#define BIT_FGUA_SOFT_RST                       BIT(13)
#define BIT_LDO_FGU_PD                          BIT(12)
#define BITS_CHG_INT_DELAY(x)                   (((x) & 0x7) << 9)
#define BIT_SD_CHOP_CAP_EN                      BIT(8)
#define BITS_SD_CLK_P(x)                        (((x) & 0x3) << 6)
#define BIT_SD_DCOFFSET_EN                      BIT(5)
#define BIT_SD_CHOP_EN                          BIT(4)
#define BIT_DP_DM_FC_ENB                        BIT(2)
#define BIT_DP_DM_AUX_EN                        BIT(1)
#define BIT_DP_DM_BC_ENB                        BIT(0)



#define BITS_VBAT_CRASH_V(x)                    (((x) & 0x3) << 10)
#define BIT_OVLO_EN                             BIT(9)
#define BITS_OVLO_V(x)                          (((x) & 0x3) << 2)
#define BITS_OVLO_T(x)                          (((x) & 0x3) << 0)



#define BITS_XOSC32K_CTL(x)                     (((x) & 0xF) << 12)
#define BITS_BATON_T(x)                         (((x) & 0x3) << 10)
#define BIT_BATDET_LDO_SEL                      BIT(9)
#define BIT_BATDET_OK                           BIT(8)
#define BIT_VBAT_OK                             BIT(4)
#define BIT_ALL_GPI_DEB                         BIT(3)
#define BIT_GPI_DEBUG_EN                        BIT(2)
#define BIT_ALL_INT_DEB                         BIT(1)
#define BIT_INT_DEBUG_EN                        BIT(0)



#define BITS_POR_RST_MONITOR(x)                 (((x) & 0xFFFF) << 0)



#define BITS_WDG_RST_MONITOR(x)                 (((x) & 0xFFFF) << 0)



#define BITS_POR_PIN_RST_MONITOR(x)             (((x) & 0xFFFF) << 0)



#define BIT_POR_SW_FORCE_ON                     BIT(15)
#define BIT_REG_SOFT_RST_FLG_CLR                BIT(14)
#define BIT_REG_RST_FLG                         BIT(13)
#define BITS_POR_SRC_FLAG(x)                    (((x) & 0x3FFF) << 0)



#define BIT_POR_CHIP_PD_FLAG                    BIT(13)
#define BIT_POR_CHIP_PD_FLAG_CLR                BIT(12)
#define BIT_UVLO_CHIP_PD_FLAG                   BIT(11)
#define BIT_UVLO_CHIP_PD_FLAG_CLR               BIT(10)
#define BIT_HARD_7S_CHIP_PD_FLAG                BIT(9)
#define BIT_HARD_7S_CHIP_PD_FLAG_CLR            BIT(8)
#define BIT_SW_CHIP_PD_FLAG                     BIT(7)
#define BIT_SW_CHIP_PD_FLAG_CLR                 BIT(6)
#define BIT_HW_CHIP_PD_FLAG                     BIT(5)
#define BIT_HW_CHIP_PD_FLAG_CLR                 BIT(4)
#define BIT_OTP_CHIP_PD_FLAG                    BIT(3)
#define BIT_OTP_CHIP_PD_FLAG_CLR                BIT(2)


#define BIT_PBINT_7S_FLAG_CLR                   BIT(15)
#define BIT_EXT_RSTN_FLAG_CLR                   BIT(14)
#define BIT_CHGR_INT_FLAG_CLR                   BIT(13)
#define BIT_PBINT2_FLAG_CLR                     BIT(12)
#define BIT_PBINT_FLAG_CLR                      BIT(11)
#define BIT_KEY2_7S_RST_EN                      BIT(9)
#define BIT_PBINT_7S_RST_SWMODE                 BIT(8)
#define BITS_PBINT_7S_RST_THRESHOLD(x)          (((x) & 0xF) << 4)
#define BIT_EXT_RSTN_MODE                       BIT(3)
#define BIT_PBINT_7S_AUTO_ON_EN                 BIT(2)
#define BIT_PBINT_7S_RST_DISABLE                BIT(1)
#define BIT_PBINT_7S_RST_MODE                   BIT(0)



#define BITS_HWRST_RTC_REG_STS(x)               (((x) & 0xFF) << 8)
#define BITS_HWRST_RTC_REG_SET(x)               (((x) & 0xFF) << 0)



#define BIT_ARCH_EN                             BIT(0)



#define BIT_MCU_WR_PROT                         BIT(15)
#define BITS_MCU_WR_PROT_VALUE(x)               (((x) & 0x7FFF) << 0)



#define BIT_PWR_WR_PROT                         BIT(15)
#define BITS_PWR_WR_PROT_VALUE(x)               (((x) & 0x7FFF) << 0)



#define BITS_SMPL_MODE(x)                       (((x) & 0xFFFF) << 0)



#define BIT_SMPL_PWR_ON_FLAG                    BIT(15)
#define BIT_SMPL_MODE_WR_ACK_FLAG               BIT(14)
#define BIT_SMPL_PWR_ON_FLAG_CLR                BIT(13)
#define BIT_SMPL_MODE_WR_ACK_FLAG_CLR           BIT(12)
#define BIT_SMPL_PWR_ON_SET                     BIT(11)
#define BIT_SMPL_EN                             BIT(0)



#define BITS_RTC_CLK_FLAG_SET(x)                (((x) & 0xFFFF) << 0)



#define BITS_RTC_CLK_FLAG_CLR(x)                (((x) & 0xFFFF) << 0)



#define BITS_RTC_CLK_FLAG_RTC(x)                (((x) & 0xFFFF) << 0)



#define BIT_RTC_CLK_STOP_FLAG                   BIT(7)
#define BITS_RTC_CLK_STOP_THRESHOLD(x)          (((x) & 0x7F) << 0)



#define BITS_VBAT_DROP_CNT(x)                   (((x) & 0xFFF) << 0)



#define BIT_EXT_RSTN_PD_EN                      BIT(10)
#define BIT_PB_7S_RST_PD_EN                     BIT(9)
#define BIT_REG_RST_PD_EN                       BIT(8)
#define BIT_WDG_RST_PD_EN                       BIT(7)
#define BIT_REG_RST_EN                          BIT(4)
#define BITS_SW_RST_PD_THRESHOLD(x)             (((x) & 0xF) << 0)



#define BIT_SW_RST_DCDCMEM_PD_EN                BIT(11)
#define BIT_SW_RST_DCDCGEN_PD_EN                BIT(10)
#define BIT_SW_RST_DCDCCORE_PD_EN               BIT(9)
#define BIT_SW_RST_MEM_PD_EN                    BIT(8)
#define BIT_SW_RST_DCXO_PD_EN                   BIT(7)
#define BIT_SW_RST_VDD28_PD_EN                  BIT(6)
#define BIT_SW_RST_AVDD18_PD_EN                 BIT(5)
#define BIT_SW_RST_RF18_PD_EN                   BIT(4)
#define BIT_SW_RST_USB33_PD_EN                  BIT(3)
#define BIT_SW_RST_EMMCCORE_PD_EN               BIT(2)
#define BIT_SW_RST_SDIO_PD_EN                   BIT(1)
#define BIT_SW_RST_SDCORE_PD_EN                 BIT(0)



#define BITS_OTP_RESERVED(x)                    (((x) & 0xF) << 3)
#define BITS_OTP_OP(x)                          (((x) & 0x3) << 1)
#define BIT_OTP_EN                              BIT(0)



#define BITS_TIMER_LOW(x)                       (((x) & 0xFFFF) << 0)



#define BITS_TIMER_HIGH(x)                      (((x) & 0xFFFF) << 0)



#define BIT_DCXO_EXTRA_LOW_CUR_MODE_EN          BIT(15)
#define BIT_DCXO_LOW_CUR_MODE_EN                BIT(14)
#define BIT_LDO_DCXO_LP_AUTO_EN                 BIT(13)
#define BITS_TIME_BETWEEN_CALIBRATION(x)        (((x) & 0xF) << 9)
#define BITS_TIME_FOR_CALIBRATION(x)            (((x) & 0x7) << 6)
#define BITS_TIME_FOR_DCXO_STABLE(x)            (((x) & 0xF) << 2)
#define BIT_LOW_PWR_CLK32K_EN                   BIT(0)



#define BITS_CAL_RESULT_1(x)                    (((x) & 0xFFFF) << 0)



#define BITS_CAL_RESULT_0(x)                    (((x) & 0xFFFF) << 0)



#define BITS_DIV_FAC_FRAC(x)                    (((x) & 0xFFFF) << 0)



#define BIT_SWITCH_FOR_CLK                      BIT(5)
#define BIT_LOW_PWR_CLK32K_EN_DBG               BIT(4)
#define BITS_DIV_FAC_INT(x)                     (((x) & 0xF) << 0)



#define BITS_CORE_STEP_DELAY(x)                 (((x) & 0x3) << 12)
#define BITS_CORE_STEP_NUM(x)                   (((x) & 0xF) << 8)
#define BITS_CORE_STEP_VOL(x)                   (((x) & 0x1F) << 3)
#define BIT_CORE_VOL_TUNE_START                 BIT(2)
#define BIT_CORE_VOL_TUNE_FLAG                  BIT(1)
#define BIT_CORE_VOL_TUNE_EN                    BIT(0)



#define BITS_CPU_STEP_DELAY(x)                  (((x) & 0x3) << 12)
#define BITS_CPU_STEP_NUM(x)                    (((x) & 0xF) << 8)
#define BITS_CPU_STEP_VOL(x)                    (((x) & 0x1F) << 3)
#define BIT_CPU_VOL_TUNE_START                  BIT(2)
#define BIT_CPU_VOL_TUNE_FLAG                   BIT(1)
#define BIT_CPU_VOL_TUNE_EN                     BIT(0)

#endif /* __DT_MFD_UNISOC_SC2721_GLB_H */

