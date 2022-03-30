/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __EDMA3_REG_H__
#define __EDMA3_REG_H__

#include <linux/kernel.h>
#include <linux/io.h>


#define EDMA30_REG_SHOW_RANGE			0x200

/* APU_EDMA2_CFG_0 */
#define EXT_IOMMU_READ				BIT(29)
#define EXT_IOMMU_WRITE				BIT(31)

#define EXT_CMDQ_INT_ENABLE			BIT(28)


#define APU_EDMA3_VERSION          0x000
#define APU_EDMA3_CTRL             0x004
#define APU_EDMA3_STATE            0x008
#define APU_EDMA3_ERR_STATUS       0x00C
#define APU_EDMA3_ERR_MASK         0x010
#define APU_EDMA3_ERR_DES_ID       0x014
#define APU_EDMA3_ERR_CH_INDEX     0x018
#define APU_EDMA3_DONE_STATUS      0x01C
#define APU_EDMA3_DONE_MASK        0x020
#define APU_EDMA3_HW_SYNC_A        0x024
#define APU_EDMA3_HW_SYNC_B        0x028
#define APU_EDMA3_PMU_CTRL         0x02C
#define APU_EDMA3_PMU_STATE        0x030
#define APU_EDMA3_UFBC_CTRL        0x080
#define APU_EDMA3_UFBC_GROUP_CTRL  0x084
#define APU_EDMA3_UFBC_DBG_STATE   0x088
#define APU_EDMA3_UFBC_MON_GMC     0x08C
#define APU_EDMA3_UFBC_CHECK_SUM   0x090
#define APU_EDMA3_UFBDC_CTRL       0x0C0
#define APU_EDMA3_UFBDC_GROUP_CTRL 0x0C4
#define APU_EDMA3_UFBDC_DBG_STATE  0x0C8
#define APU_EDMA3_UFBDC_MON_GMC    0x0CC
#define APU_EDMA3_UFBDC_VERSION_A  0x0D0
#define APU_EDMA3_UFBDC_VERSION_B  0x0D4
#define APU_EDMA3_UFBDC_VERSION_C  0x0D8
#define APU_EDMA3_UFBDC_VERSION_D  0x0DC
#define APU_EDMA3_CMDQ_CH0_LO_A    0x800
#define APU_EDMA3_CMDQ_CH0_LO_B    0x804
#define APU_EDMA3_CMDQ_CH0_LO_C    0x808
#define APU_EDMA3_CMDQ_CH0_LO_D    0x80C
#define APU_EDMA3_CMDQ_CH0_LO_E    0x810
#define APU_EDMA3_CMDQ_CH0_LO_F    0x814
#define APU_EDMA3_CMDQ_CH0_HI_A    0x880
#define APU_EDMA3_CMDQ_CH0_HI_B    0x884
#define APU_EDMA3_CMDQ_CH0_HI_C    0x888
#define APU_EDMA3_CMDQ_CH0_HI_D    0x88C
#define APU_EDMA3_CMDQ_CH0_HI_E    0x890
#define APU_EDMA3_CMDQ_CH0_HI_F    0x894
#define APU_EDMA3_CMDQ_CH1_LO_A    0x900
#define APU_EDMA3_CMDQ_CH1_LO_B    0x904
#define APU_EDMA3_CMDQ_CH1_LO_C    0x908
#define APU_EDMA3_CMDQ_CH1_LO_D    0x90C
#define APU_EDMA3_CMDQ_CH1_LO_E    0x910
#define APU_EDMA3_CMDQ_CH1_LO_F    0x914
#define APU_EDMA3_CMDQ_CH1_HI_A    0x980
#define APU_EDMA3_CMDQ_CH1_HI_B    0x984
#define APU_EDMA3_CMDQ_CH1_HI_C    0x988
#define APU_EDMA3_CMDQ_CH1_HI_D    0x98C
#define APU_EDMA3_CMDQ_CH1_HI_E    0x990
#define APU_EDMA3_CMDQ_CH1_HI_F    0x994
#define APU_EDMA3_CMDQ_CH2_LO_A    0xA00
#define APU_EDMA3_CMDQ_CH2_LO_B    0xA04
#define APU_EDMA3_CMDQ_CH2_LO_C    0xA08
#define APU_EDMA3_CMDQ_CH2_LO_D    0xA0C
#define APU_EDMA3_CMDQ_CH2_LO_E    0xA10
#define APU_EDMA3_CMDQ_CH2_LO_F    0xA14
#define APU_EDMA3_CMDQ_CH2_HI_A    0xA80
#define APU_EDMA3_CMDQ_CH2_HI_B    0xA84
#define APU_EDMA3_CMDQ_CH2_HI_C    0xA88
#define APU_EDMA3_CMDQ_CH2_HI_D    0xA8C
#define APU_EDMA3_CMDQ_CH2_HI_E    0xA90
#define APU_EDMA3_CMDQ_CH2_HI_F    0xA94
#define APU_EDMA3_CMDQ_CH3_LO_A    0xB00
#define APU_EDMA3_CMDQ_CH3_LO_B    0xB04
#define APU_EDMA3_CMDQ_CH3_LO_C    0xB08
#define APU_EDMA3_CMDQ_CH3_LO_D    0xB0C
#define APU_EDMA3_CMDQ_CH3_LO_E    0xB10
#define APU_EDMA3_CMDQ_CH3_LO_F    0xB14
#define APU_EDMA3_CMDQ_CH3_HI_A    0xB80
#define APU_EDMA3_CMDQ_CH3_HI_B    0xB84
#define APU_EDMA3_CMDQ_CH3_HI_C    0xB88
#define APU_EDMA3_CMDQ_CH3_HI_D    0xB8C
#define APU_EDMA3_CMDQ_CH3_HI_E    0xB90
#define APU_EDMA3_CMDQ_CH3_HI_F    0xB94
#define APU_EDMA3_CMDQ_CH4_LO_A    0xC00
#define APU_EDMA3_CMDQ_CH4_LO_B    0xC04
#define APU_EDMA3_CMDQ_CH4_LO_C    0xC08
#define APU_EDMA3_CMDQ_CH4_LO_D    0xC0C
#define APU_EDMA3_CMDQ_CH4_LO_E    0xC10
#define APU_EDMA3_CMDQ_CH4_LO_F    0xC14
#define APU_EDMA3_CMDQ_CH4_HI_A    0xC80
#define APU_EDMA3_CMDQ_CH4_HI_B    0xC84
#define APU_EDMA3_CMDQ_CH4_HI_C    0xC88
#define APU_EDMA3_CMDQ_CH4_HI_D    0xC8C
#define APU_EDMA3_CMDQ_CH4_HI_E    0xC90
#define APU_EDMA3_CMDQ_CH4_HI_F    0xC94
#define APU_EDMA3_CMDQ_CH5_LO_A    0xD00
#define APU_EDMA3_CMDQ_CH5_LO_B    0xD04
#define APU_EDMA3_CMDQ_CH5_LO_C    0xD08
#define APU_EDMA3_CMDQ_CH5_LO_D    0xD0C
#define APU_EDMA3_CMDQ_CH5_LO_E    0xD10
#define APU_EDMA3_CMDQ_CH5_LO_F    0xD14
#define APU_EDMA3_CMDQ_CH5_HI_A    0xD80
#define APU_EDMA3_CMDQ_CH5_HI_B    0xD84
#define APU_EDMA3_CMDQ_CH5_HI_C    0xD88
#define APU_EDMA3_CMDQ_CH5_HI_D    0xD8C
#define APU_EDMA3_CMDQ_CH5_HI_E    0xD90
#define APU_EDMA3_CMDQ_CH5_HI_F    0xD94
#define APU_EDMA3_CMDQ_CH6_LO_A    0xE00
#define APU_EDMA3_CMDQ_CH6_LO_B    0xE04
#define APU_EDMA3_CMDQ_CH6_LO_C    0xE08
#define APU_EDMA3_CMDQ_CH6_LO_D    0xE0C
#define APU_EDMA3_CMDQ_CH6_LO_E    0xE10
#define APU_EDMA3_CMDQ_CH6_LO_F    0xE14
#define APU_EDMA3_CMDQ_CH6_HI_A    0xE80
#define APU_EDMA3_CMDQ_CH6_HI_B    0xE84
#define APU_EDMA3_CMDQ_CH6_HI_C    0xE88
#define APU_EDMA3_CMDQ_CH6_HI_D    0xE8C
#define APU_EDMA3_CMDQ_CH6_HI_E    0xE90
#define APU_EDMA3_CMDQ_CH6_HI_F    0xE94
#define APU_EDMA3_CMDQ_CH7_LO_A    0xF00
#define APU_EDMA3_CMDQ_CH7_LO_B    0xF04
#define APU_EDMA3_CMDQ_CH7_LO_C    0xF08
#define APU_EDMA3_CMDQ_CH7_LO_D    0xF0C
#define APU_EDMA3_CMDQ_CH7_LO_E    0xF10
#define APU_EDMA3_CMDQ_CH7_LO_F    0xF14
#define APU_EDMA3_CMDQ_CH7_HI_A    0xF80
#define APU_EDMA3_CMDQ_CH7_HI_B    0xF84
#define APU_EDMA3_CMDQ_CH7_HI_C    0xF88
#define APU_EDMA3_CMDQ_CH7_HI_D    0xF8C
#define APU_EDMA3_CMDQ_CH7_HI_E    0xF90
#define APU_EDMA3_CMDQ_CH7_HI_F    0xF94



static inline unsigned int edma_read_reg32(void __iomem *edma_base,
					  unsigned int offset)
{
	return readl(edma_base + offset);
}

static inline void edma_write_reg32(void __iomem *edma_base,
					unsigned int offset,
					unsigned int val)
{
	writel(val, edma_base + offset);
}

static inline void edma_set_reg32(void __iomem *edma_base, unsigned int offset,
			  unsigned int bits)
{
	edma_write_reg32(edma_base, offset,
			(edma_read_reg32(edma_base, offset) | bits));
}

static inline void edma_clear_reg32(void __iomem *edma_base,
					unsigned int offset,
					unsigned int bits)
{
	edma_write_reg32(edma_base, offset,
			(edma_read_reg32(edma_base, offset) & ~bits));
}


#endif
