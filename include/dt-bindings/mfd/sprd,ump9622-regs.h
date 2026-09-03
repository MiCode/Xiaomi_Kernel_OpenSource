/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
/*
 * Copyright (C) 2020 UNISOC Technologies Co., Ltd.
 *
 * This file is dual-licensed: you can use it either under the terms
 * of the GPL or the X11 license, at your option. Note that this dual
 * licensing only applies to this file, and not this project as a
 * whole.
 *
 ********************************************************************
 * Auto generated c code from ASIC Documentation, PLEASE DONOT EDIT *
 ********************************************************************
 */

#ifndef __DT_BINDINGS_UNISOC_UMP9622_REGS_H
#define __DT_BINDINGS_UNISOC_UMP9622_REGS_H

#define REG_ANA_UMP9622_GLB_BASE                            0x2000

#define REG_ANA_UMP9622_CHIP_ID_LOW                         (REG_ANA_UMP9622_GLB_BASE + 0x0000)
#define REG_ANA_UMP9622_CHIP_ID_HIGH                        (REG_ANA_UMP9622_GLB_BASE + 0x0004)
#define REG_ANA_UMP9622_MODULE_EN0                          (REG_ANA_UMP9622_GLB_BASE + 0x0008)
#define REG_ANA_UMP9622_ARM_CLK_EN0                         (REG_ANA_UMP9622_GLB_BASE + 0x000C)
#define REG_ANA_UMP9622_RESERVED_REG1                       (REG_ANA_UMP9622_GLB_BASE + 0x0010)
#define REG_ANA_UMP9622_SOFT_RST0                           (REG_ANA_UMP9622_GLB_BASE + 0x0014)
#define REG_ANA_UMP9622_ARCH_EN                             (REG_ANA_UMP9622_GLB_BASE + 0x0018)
#define REG_ANA_UMP9622_MCU_WR_PROT_VALUE                   (REG_ANA_UMP9622_GLB_BASE + 0x001C)
#define REG_ANA_UMP9622_PWR_OFF_SEQ_CTRL                    (REG_ANA_UMP9622_GLB_BASE + 0x0020)
#define REG_ANA_UMP9622_DEBUG_REG0                          (REG_ANA_UMP9622_GLB_BASE + 0x0024)
#define REG_ANA_UMP9622_DEBUG_REG1                          (REG_ANA_UMP9622_GLB_BASE + 0x0028)
#define REG_ANA_UMP9622_ADI_ACK_BIT_OPT_CTRL                (REG_ANA_UMP9622_GLB_BASE + 0x002C)
#define REG_ANA_UMP9622_RESERVED_REG6                       (REG_ANA_UMP9622_GLB_BASE + 0x0030)
#define REG_ANA_UMP9622_RESERVED_REG7                       (REG_ANA_UMP9622_GLB_BASE + 0x0034)
#define REG_ANA_UMP9622_RESERVED_REG8                       (REG_ANA_UMP9622_GLB_BASE + 0x0038)
#define REG_ANA_UMP9622_RESERVED_REG9                       (REG_ANA_UMP9622_GLB_BASE + 0x003C)
#define REG_ANA_UMP9622_RESERVED_REG10                      (REG_ANA_UMP9622_GLB_BASE + 0x0040)
#define REG_ANA_UMP9622_RESERVED_REG11                      (REG_ANA_UMP9622_GLB_BASE + 0x0044)
#define REG_ANA_UMP9622_RESERVED_REG12                      (REG_ANA_UMP9622_GLB_BASE + 0x0048)
#define REG_ANA_UMP9622_LDO_RTC_CTRL                        (REG_ANA_UMP9622_GLB_BASE + 0x004C)
#define REG_ANA_UMP9622_RESERVED_REG13                      (REG_ANA_UMP9622_GLB_BASE + 0x0050)
#define REG_ANA_UMP9622_RESERVED_REG14                      (REG_ANA_UMP9622_GLB_BASE + 0x0054)
#define REG_ANA_UMP9622_RESERVED_REG15                      (REG_ANA_UMP9622_GLB_BASE + 0x0058)
#define REG_ANA_UMP9622_RESERVED_REG16                      (REG_ANA_UMP9622_GLB_BASE + 0x005C)
#define REG_ANA_UMP9622_RESERVED_REG17                      (REG_ANA_UMP9622_GLB_BASE + 0x0060)
#define REG_ANA_UMP9622_SLP_CTRL                            (REG_ANA_UMP9622_GLB_BASE + 0x0064)
#define REG_ANA_UMP9622_RESERVED_REG23                      (REG_ANA_UMP9622_GLB_BASE + 0x0068)
#define REG_ANA_UMP9622_RESERVED_REG18                      (REG_ANA_UMP9622_GLB_BASE + 0x006C)
#define REG_ANA_UMP9622_RESERVED_REG19                      (REG_ANA_UMP9622_GLB_BASE + 0x0070)
#define REG_ANA_UMP9622_RESERVED_REG20                      (REG_ANA_UMP9622_GLB_BASE + 0x0074)
#define REG_ANA_UMP9622_RESERVED_REG21                      (REG_ANA_UMP9622_GLB_BASE + 0x0078)
#define REG_ANA_UMP9622_DCXO_SW_CTRL                        (REG_ANA_UMP9622_GLB_BASE + 0x007C)
#define REG_ANA_UMP9622_TSX_CTRL0                           (REG_ANA_UMP9622_GLB_BASE + 0x0080)
#define REG_ANA_UMP9622_RESERVED_REG24                      (REG_ANA_UMP9622_GLB_BASE + 0x0084)
#define REG_ANA_UMP9622_RC1M_CAL_CTRL                       (REG_ANA_UMP9622_GLB_BASE + 0x0088)
#define REG_ANA_UMP9622_TSX_CTRL3                           (REG_ANA_UMP9622_GLB_BASE + 0x008C)
#define REG_ANA_UMP9622_TSX_CTRL4                           (REG_ANA_UMP9622_GLB_BASE + 0x0090)
#define REG_ANA_UMP9622_TSX_CTRL5                           (REG_ANA_UMP9622_GLB_BASE + 0x0094)
#define REG_ANA_UMP9622_TSX_CTRL6                           (REG_ANA_UMP9622_GLB_BASE + 0x0098)
#define REG_ANA_UMP9622_TSX_CTRL7                           (REG_ANA_UMP9622_GLB_BASE + 0x009C)
#define REG_ANA_UMP9622_TSX_CTRL8                           (REG_ANA_UMP9622_GLB_BASE + 0x00A0)
#define REG_ANA_UMP9622_TSX_CTRL9                           (REG_ANA_UMP9622_GLB_BASE + 0x00A4)
#define REG_ANA_UMP9622_TSX_CTRL10                          (REG_ANA_UMP9622_GLB_BASE + 0x00A8)
#define REG_ANA_UMP9622_TSX_CTRL11                          (REG_ANA_UMP9622_GLB_BASE + 0x00AC)
#define REG_ANA_UMP9622_TSX_CTRL12                          (REG_ANA_UMP9622_GLB_BASE + 0x00B0)
#define REG_ANA_UMP9622_TSX_CTRL13                          (REG_ANA_UMP9622_GLB_BASE + 0x00B4)
#define REG_ANA_UMP9622_TSX_CTRL14                          (REG_ANA_UMP9622_GLB_BASE + 0x00B8)
#define REG_ANA_UMP9622_TSX_CTRL15                          (REG_ANA_UMP9622_GLB_BASE + 0x00BC)
#define REG_ANA_UMP9622_EXT_XTL_RX_CTRL0                    (REG_ANA_UMP9622_GLB_BASE + 0x00C0)
#define REG_ANA_UMP9622_EXT_XTL_RX_CTRL1                    (REG_ANA_UMP9622_GLB_BASE + 0x00C4)
#define REG_ANA_UMP9622_RESERVED_REG_CORE                   (REG_ANA_UMP9622_GLB_BASE + 0x00C8)
#define REG_ANA_UMP9622_RESERVED_REG_RTC                    (REG_ANA_UMP9622_GLB_BASE + 0x00CC)
#define REG_ANA_UMP9622_CLK32KLESS_CTRL0                    (REG_ANA_UMP9622_GLB_BASE + 0x00D0)
#define REG_ANA_UMP9622_CLK32KLESS_CTRL1                    (REG_ANA_UMP9622_GLB_BASE + 0x00D4)
#define REG_ANA_UMP9622_CLK32KLESS_CTRL2                    (REG_ANA_UMP9622_GLB_BASE + 0x00D8)
#define REG_ANA_UMP9622_CLK32KLESS_CTRL3                    (REG_ANA_UMP9622_GLB_BASE + 0x00DC)
#define REG_ANA_UMP9622_XTL_WAIT_CTRL0                      (REG_ANA_UMP9622_GLB_BASE + 0x00E0)
#define REG_ANA_UMP9622_RESERVED_REG30                      (REG_ANA_UMP9622_GLB_BASE + 0x00E4)
#define REG_ANA_UMP9622_EXT_XTL_RX_CTRL2                    (REG_ANA_UMP9622_GLB_BASE + 0x00E8)
#define REG_ANA_UMP9622_TSX_WR_PROT_VALUE                   (REG_ANA_UMP9622_GLB_BASE + 0x00EC)
#define REG_ANA_UMP9622_LOW_PWR_CLK32K_CTRL                 (REG_ANA_UMP9622_GLB_BASE + 0x00F0)
#define REG_ANA_UMP9622_POR_CTRL                            (REG_ANA_UMP9622_GLB_BASE + 0x00F4)
#define REG_ANA_UMP9622_TSEN_CTRL0                          (REG_ANA_UMP9622_GLB_BASE + 0x00F8)
#define REG_ANA_UMP9622_TSEN_CTRL1                          (REG_ANA_UMP9622_GLB_BASE + 0x00FC)
#define REG_ANA_UMP9622_TSEN_CTRL2                          (REG_ANA_UMP9622_GLB_BASE + 0x0100)
#define REG_ANA_UMP9622_TSEN_CTRL3                          (REG_ANA_UMP9622_GLB_BASE + 0x0104)
#define REG_ANA_UMP9622_TSEN_CTRL4                          (REG_ANA_UMP9622_GLB_BASE + 0x0108)
#define REG_ANA_UMP9622_TSEN_CTRL5                          (REG_ANA_UMP9622_GLB_BASE + 0x010C)
#define REG_ANA_UMP9622_TSEN_CTRL6                          (REG_ANA_UMP9622_GLB_BASE + 0x0110)
#define REG_ANA_UMP9622_TSEN_CTRL7                          (REG_ANA_UMP9622_GLB_BASE + 0x0114)

#endif /* __DT_BINDINGS_UNISOC_UMP9622_REGS_H */

