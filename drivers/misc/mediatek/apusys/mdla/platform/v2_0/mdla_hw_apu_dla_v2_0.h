/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_HW_APU_DLA_V2_0_H__
#define __MDLA_HW_APU_DLA_V2_0_H__

/**
 * apu_dla_0_config
 * apu_dla_1_config
 */
#define MDLA_CG_CON                     (0x000)
#define MDLA_CG_SET                     (0x004)
#define MDLA_CG_CLR                     (0x008)
#define MDLA_SW_RST                     (0x00C)
#define MDLA_MBIST_MODE0                (0x010)
#define MDLA_MBIST_MODE1                (0x014)
#define MDLA_MBIST_CTL                  (0x018)
#define MDLA_AXI_BIST_CTL               (0x01C)
#define MDLA_AXI_BIST_STA               (0x020)
#define MDLA_MBIST_DEFAULT_DELSEL       (0x024)
#define MDLA_SRAM_DELSEL_CMDE           (0x028)
#define MDLA_SRAM_DELSEL_CONV           (0x02C)
#define MDLA_SRAM_DELSEL_CBLD           (0x030)
#define MDLA_SRAM_DELSEL_CB_0           (0x034)
#define MDLA_SRAM_DELSEL_CB_1           (0x038)
#define MDLA_SRAM_DELSEL_STE            (0x03C)
#define MDLA_SRAM_DELSEL_SB_0           (0x040)
#define MDLA_SRAM_DELSEL_SB_1           (0x044)
#define MDLA_SRAM_DELSEL_EWE_0          (0x048)
#define MDLA_SRAM_DELSEL_EWE_1          (0x04C)
#define MDLA_SRAM_DELSEL_EWE_2          (0x050)
#define MDLA_SRAM_DELSEL_WDEC           (0x054)
#define MDLA_SRAM_DELSEL_RQU_0          (0x058)
#define MDLA_SRAM_DELSEL_RQU_1          (0x05C)
#define MDLA_RP_RST                     (0x060)
#define MDLA_RP_CON                     (0x064)
#define MDLA_RP_PRE_FUSE_0              (0x068)
#define MDLA_RP_PRE_FUSE_1              (0x06C)
#define MDLA_RP_PRE_FUSE_2              (0x070)
#define MDLA_RP_PRE_FUSE_3              (0x074)
#define MDLA_RP_PRE_FUSE_4              (0x078)
#define MDLA_RP_PRE_FUSE_5              (0x07C)
#define MDLA_SPARE_0                    (0x080)
#define MDLA_SPARE_1                    (0x084)
#define MDLA_SPARE_2                    (0x088)
#define MDLA_SPARE_3                    (0x08C)
#define MDLA_SPARE_4                    (0x090)
#define MDLA_SPARE_5                    (0x094)
#define MDLA_SPARE_6                    (0x098)
#define MDLA_SPARE_7                    (0x09C)
#define MDLA_CTRL                       (0x110)
#define MDLA_CSYSREQ                    (0x114)
#define MDLA_CSYSACK                    (0x118)
#define MDLA_AXI_CTRL                   (0x120)
#define MDLA_AXI1_CTRL                  (0x124)
#define MDLA_APB_CTRL                   (0x128)
#define MDLA_APB_STA                    (0x12C)
#define MDLA_GALS_M0_CTRL               (0x130)
#define MDLA_GALS_M1_CTRL               (0x134)
#define MDLA_GALS_STA                   (0x138)
#define MDLA_AXI_GALS_DBG               (0x13C)
#define MDLA_SES_CTL0                   (0x140)
#define MDLA_SES_CTL1                   (0x144)
#define MDLA_SES_CTL2                   (0x148)


/* Register fields */

/* MDLA_SW_RST : 0x00C */
#define MDLA_SW_RST_SETETING       (0x3F)
#define MDLA_SW_MDLA_RST_MASK      BIT(0)
#define MDLA_SW_APB_RST_MASK       BIT(6)
#define MDLA_SW_RST_MASK           (MDLA_SW_MDLA_RST_MASK|MDLA_SW_APB_RST_MASK)

/* MDLA_AXI_CTRL, MDLA_AXI1_CTRL : 0x120, 0x124 */
#define AWUSER_M1_ACP_EN                BIT(0)
#define AWUSER_M1_MMU_EN                BIT(1)
#define ARUSER_M1_ACP_EN                BIT(9)
#define ARUSER_M1_MMU_EN                BIT(10)

#define MDLA_AXI_CTRL_MASK              (AWUSER_M1_MMU_EN | ARUSER_M1_MMU_EN)


#endif /* __MDLA_HW_APU_DLA_V2_0_H__ */
