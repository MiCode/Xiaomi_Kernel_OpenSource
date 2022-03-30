/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef __EDMA_REG_H__
#define __EDMA_REG_H__

#include <linux/kernel.h>
#include <linux/io.h>

#define EDMA_REG_SHOW_RANGE			0x150

#define EDMA_REG_EX_R1			0xC00

#define EDMA_REG_EX_R2			0xC5C


#define DESP_WRITE_POINTER_MASK			0x00000030
#define NUM_DESP_MASK				0x00000007
#define DESP_NUM_INCR_MASK			0x00070000
#define DMA_IDLE_MASK				0x00000200
#define YUVRGB_MAT_MASK				0x7E000000

#define APU_EDMA2_DESP_OFFSET			0x100
#define APU_EDMA2_EX_DESP_OFFSET		0x60

/* APU_EDMA2_CTL_0 */
#define CLK_ENABLE				BIT(0)
#define DMA_SW_RST				BIT(4)
#define AXI_PROT_EN			    BIT(12)
#define RST_PROT_IDLE			BIT(14)
#define EDMA_DESCRIPTOR_MODE			BIT(16)

/* APU_EDMA2_CFG_0 */
#define EXT_DESP_START				BIT(12)
#define DESP_NUM_INCR				BIT(16)

/* APU_EDMA2_INT_STATUS */
#define DESP0_DONE_STATUS			BIT(0)
#define DESP1_DONE_STATUS			BIT(1)
#define DESP2_DONE_STATUS			BIT(2)
#define DESP3_DONE_STATUS			BIT(3)
#define EXT_DESP_DONE_STATUS			BIT(4)
#define DESP0_DONE_INT_STATUS			BIT(16)
#define DESP1_DONE_INT_STATUS			BIT(17)
#define DESP2_DONE_INT_STATUS			BIT(18)
#define DESP3_DONE_INT_STATUS			BIT(19)
#define EXT_DESP_DONE_INT_STATUS		BIT(20)

/* APU_EDMA2_EXT_DESP_CFG_0 */
#define EXT_DESP_INT_ENABLE			BIT(16)

#define DESP0_OUT_FILL_MODE			BIT(20)

#define DESP0_INT_ENABLE			BIT(28)

#define EXT_DESP_USER_IOMMU			BIT(12)

#define DESP0_DMA_AWUSER_IOMMU			BIT(6)
#define DESP0_DMA_ARUSER_IOMMU			BIT(1)

#define APU_EDMA2_CTL_0				0x000
#define APU_EDMA2_CFG_0				0x004
#define APU_EDMA2_INT_MASK			0x008
#define APU_EDMA2_ERR_INT_MASK			0x00C
#define APU_EDMA2_INT_STATUS			0x010
#define APU_EDMA2_ERR_STATUS			0x014
#define APU_EDMA2_FILL_VALUE			0x018
#define APU_EDMA2_UFBC_CFG_0			0x020
#define APU_EDMA2_UFBC_CFG_1			0x024
#define APU_EDMA2_UFBDC_CFG_0			0x030
#define APU_EDMA2_UFBDC_CFG_1			0x034
#define APU_EDMA2_UFBDC_INFO_0			0x038
#define APU_EDMA2_AFBC_DBG_SEL			0x080
#define APU_EDMA2_UFBC_DBG_INFO			0x084
#define APU_EDMA2_UFBDC_DBG_INFO		0x088
#define APU_EDMA2_UFBDC_VCODE_0			0x090
#define APU_EDMA2_UFBDC_VCODE_1			0x094
#define APU_EDMA2_UFBDC_VCODE_2			0x098
#define APU_EDMA2_UFBDC_VCODE_3			0x09C
#define APU_EDMA2_EXT_DESP_CFG_0		0x0A0
#define APU_EDMA2_EXT_DESP_CFG_1		0x0A4
#define APU_EDMA2_PMU_CTL			0x100
#define APU_EDMA2_DBG_PMU_SEL			0x104
#define APU_EDMA2_DBG_PMU_INFO			0x108
#define APU_EDMA2_DESP0_0			0x800
#define APU_EDMA2_DESP0_4			0x804
#define APU_EDMA2_DESP0_8			0x808
#define APU_EDMA2_DESP0_C			0x80C
#define APU_EDMA2_DESP0_10			0x810
#define APU_EDMA2_DESP0_14			0x814
#define APU_EDMA2_DESP0_18			0x818
#define APU_EDMA2_DESP0_1C			0x81C
#define APU_EDMA2_DESP0_20			0x820
#define APU_EDMA2_DESP0_24			0x824
#define APU_EDMA2_DESP0_28			0x828
#define APU_EDMA2_DESP0_2C			0x82C
#define APU_EDMA2_DESP0_30			0x830
#define APU_EDMA2_DESP0_34			0x834
#define APU_EDMA2_DESP0_38			0x838
#define APU_EDMA2_DESP0_3C			0x83C
#define APU_EDMA2_DESP0_40			0x840
#define APU_EDMA2_DESP0_44			0x844
#define APU_EDMA2_DESP0_48			0x848
#define APU_EDMA2_DESP0_4C			0x84C
#define APU_EDMA2_DESP0_50			0x850
#define APU_EDMA2_DESP0_54			0x854
#define APU_EDMA2_DESP0_58			0x858
#define APU_EDMA2_DESP0_5C			0x85C


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

#endif /* __EDMA_REG_H__ */
