/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_HW_BIU_V2_0_H__
#define __MDLA_HW_BIU_V2_0_H__

/**
 * mdla0_biu_v1p5
 * mdla1_biu_v1p5
 */
#define CFG_PMCR          (0x0E00)
#define PMU_CLK_CNT       (0x0E04)
#define PMU_START_TSTAMP  (0x0E08)
#define PMU_END_TSTAMP    (0x0E0C)
#define PMU1              (0x0E10)
#define PMU1_CNT          (0x0E14)
#define PMU1_CNT_LATCH    (0x0E18)

/* id = 0 ~ 14 (PMU counter 1 ~ 15): 0xE10 ~ 0xEF8 */
#define PMU(id)           (0x10 * (id) + PMU1)
#define PMU_CNT(id)       (0x10 * (id) + PMU1_CNT)
#define PMU_CNT_LATCH(id) (0x10 * (id) + PMU1_CNT_LATCH)

#define PMU_CMDID_LATCH   (0x0F00)


/* Register fields */

/* CFG_PMCR : 0xE00 */
#define PMU_CNT_EN(i)      BIT(17 + (i))
#define PMU_CCNT_EN        BIT(16)
#define PMU_PERCMD_MODE    BIT(5)
#define PMU_EXPORT         BIT(4)
#define PMU_CCNT_RESET     BIT(2)
#define PMU_CNT_RESET      BIT(1)
#define PMU_ENABLE         BIT(0)

/* FIXME: mt6885(0x1F021), need to confirm mt8198 value */
#define CFG_PMCR_DEFAULT   (PMU_ENABLE | PMU_CCNT_EN)


#endif /* __MDLA_HW_BIU_V2_0_H__ */
