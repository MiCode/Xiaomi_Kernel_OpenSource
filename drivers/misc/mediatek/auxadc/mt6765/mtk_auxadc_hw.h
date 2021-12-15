/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _MTK_ADC_HW_H
#define _MTK_ADC_HW_H

#ifdef CONFIG_OF
extern void __iomem *auxadc_base;
extern void __iomem *auxadc_apmix_base;
extern void __iomem *auxadc_efuse_base;
#undef AUXADC_BASE
#undef APMIXED_BASE
#undef EFUSEC_BASE
#define AUXADC_BASE auxadc_base
#define APMIXED_BASE auxadc_apmix_base
#define EFUSEC_BASE auxadc_efuse_base
#else
#include <mach/mt_reg_base.h>
#endif

/* For calibration */
#define EFUSE_CALI
#define AUXADC_INDEX 106
#define ADC_GE_A_MASK 0x000ffc00
#define ADC_GE_A_SHIFT 10
#define ADC_OE_A_MASK 0x000003ff
#define ADC_OE_A_SHIFT 0
#define ADC_CALI_EN_A_MASK 0x100000
#define ADC_CALI_EN_A_SHIFT 20
#define ADC_CALI_EN_A_REG		(EFUSEC_BASE + 0x0198)
#define ADC_GE_A_REG            (EFUSEC_BASE + 0x0198)
#define ADC_OE_A_REG            (EFUSEC_BASE + 0x0198)


#define ADC_CHANNEL_MAX 16

#define MT_PDN_PERI_AUXADC MT_CG_PERI_AUXADC

#define AUXADC_NODE "mediatek,auxadc"
#define AUXADC_APMIX_NODE "mediatek,apmixedsys"

#define AUXADC_CON0             (AUXADC_BASE + 0x000)
#define AUXADC_CON1             (AUXADC_BASE + 0x004)
#define AUXADC_CON1_SET         (AUXADC_BASE + 0x008)
#define AUXADC_CON1_CLR         (AUXADC_BASE + 0x00C)
#define AUXADC_CON2             (AUXADC_BASE + 0x010)
#define AUXADC_DAT0             (AUXADC_BASE + 0x014)
#define AUXADC_TP_CMD           (AUXADC_BASE + 0x005c)
#define AUXADC_TP_ADDR          (AUXADC_BASE + 0x0060)
#define AUXADC_TP_CON0          (AUXADC_BASE + 0x0064)
#define AUXADC_TP_DATA0         (AUXADC_BASE + 0x0074)
#define AUXADC_DET_VOLT         (AUXADC_BASE + 0x084)
#define AUXADC_DET_SEL          (AUXADC_BASE + 0x088)
#define AUXADC_DET_PERIOD       (AUXADC_BASE + 0x08C)
#define AUXADC_DET_DEBT         (AUXADC_BASE + 0x090)

/* For RTP, but hw don't support now */
#define PAD_AUX_XP				16
#define PAD_AUX_YM				16
#define TP_CMD_ADDR_X			0x0005
#define AUXADC_CON_RTP			(APMIXED_BASE + 0x0404)
/* SPM not use auxadc ,so mask it ****
 * #define AUXADC_CLOCK_BY_SPM
 *************/
#endif   /*_MTK_ADC_HW_H*/

