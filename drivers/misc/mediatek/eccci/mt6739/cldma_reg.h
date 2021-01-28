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

#ifndef __CLDMA_REG_H__
#define __CLDMA_REG_H__

#include <mt-plat/sync_write.h>
/* INFRA */
#define INFRA_RST0_REG_PD (0x0150)/* rgu reset cldma reg */
#define INFRA_RST1_REG_PD (0x0154)/* rgu clear cldma reset reg */
#define CLDMA_PD_RST_MASK (1 << 2)
#define INFRA_RST0_REG_AO (0x0140)
#define INFRA_RST1_REG_AO (0x0144)
#define CLDMA_AO_RST_MASK (1 << 6)
#define INFRA_CLDMA_CTRL_REG (0x0C00)
#define CLDMA_IP_BUSY_MASK (1 << 1)

#define INFRA_AP2MD_DUMMY_REG	(0x370)
#define INFRA_AP2MD_DUMMY_BIT	0
#define	INFRA_MD2PERI_PROT_EN	(0x250)
#define	INFRA_MD2PERI_PROT_RDY	(0x258)
#define	INFRA_MD2PERI_PROT_SET	(0x2A8)
#define	INFRA_MD2PERI_PROT_CLR	(0x2AC)
#define	INFRA_MD2PERI_PROT_BIT	6

#define	INFRA_PERI2MD_PROT_EN	(0x220)
#define	INFRA_PERI2MD_PROT_RDY	(0x228)
#define	INFRA_PERI2MD_PROT_SET	(0x2A0)
#define	INFRA_PERI2MD_PROT_CLR	(0x2A4)
#define	INFRA_PERI2MD_PROT_BIT	7

#define INFRA_AO_MD_SRCCLKENA    (0xF0C) /* SRC CLK ENA */
/*===========================CLDMA_AO_UL_CFG: 10014004-1001402C==================================*/
/* The start address of first TGPD descriptor for power-down back-up. */
#define CLDMA_AP_UL_START_ADDR_BK_0      (0x0004)
#define CLDMA_AP_UL_START_ADDR_BK_1      (0x0008)
#define CLDMA_AP_UL_START_ADDR_BK_2      (0x000C)
#define CLDMA_AP_UL_START_ADDR_BK_3      (0x0010)
#define CLDMA_AP_UL_START_ADDR_BK_4MSB   (0x0014)
/* The address of current processing TGPD descriptor for power-down back-up. */
#define CLDMA_AP_UL_CURRENT_ADDR_BK_0    (0x0018)
#define CLDMA_AP_UL_CURRENT_ADDR_BK_1    (0x001C)
#define CLDMA_AP_UL_CURRENT_ADDR_BK_2    (0x0020)
#define CLDMA_AP_UL_CURRENT_ADDR_BK_3    (0x0024)
#define CLDMA_AP_UL_CURRENT_ADDR_BK_4MSB (0x0028)
/*===========================CLDMA_AO_DL_CFG:10014400-1001441C==================================*/
#define CLDMA_AP_SO_CFG                     (0x0400)	/* Operation Configuration */
#define CLDMA_AP_SO_START_ADDR_0            (0x0404)	/* The start address of first RGPD descriptor */
#define CLDMA_AP_SO_CURRENT_ADDR_0          (0x0408)	/* The address of current processing RGPD descriptor. */
#define CLDMA_AP_SO_START_ADDR_4MSB         (0x040C)	/* MSB 4 bits(35~32) of first RGPD */
#define CLDMA_AP_SO_CURRENT_ADDR_4MSB       (0x0410)	/* MSB 4 bits(35~32) of current processing RGPD*/
#define CLDMA_AP_SO_STATUS                  (0x0414)	/* SME OUT SBDMA operation status. */
#define CLDMA_AP_DL_MTU_SIZE                (0x0418)	/* maximu MTU size */
/*=======================CLDMA_AO_MISC:  10014800-1001480C==================================*/
#define CLDMA_AP_L2RIMR0                    (0x0800)	/* Level 2 Interrupt Mask Register (RX Part) */
#define CLDMA_AP_L2RIMCR0                   (0x0804)	/* Level 2 Interrupt Mask Clear Register (RX Part) */
#define CLDMA_AP_L2RIMSR0                   (0x0808)	/* Level 2 Interrupt Mask Set Register (RX Part) */
/*=======================CLDMA_PD_UL_CFG: 1021B000-1021B070 ==================================*/
#define CLDMA_AP_UL_START_ADDR_0            (0x0000)	/* The start address of first TGPD descriptor */
#define CLDMA_AP_UL_START_ADDR_1            (0x0004)	/* The start address of first TGPD descriptor */
#define CLDMA_AP_UL_START_ADDR_2            (0x0008)	/* The start address of first TGPD descriptor */
#define CLDMA_AP_UL_START_ADDR_3            (0x000C)	/* The start address of first TGPD descriptor */
#define CLDMA_AP_UL_START_ADDR_4MSB         (0x0010)
#define CLDMA_AP_UL_CURRENT_ADDR_0          (0x0014)	/* The address of current processing TGPD descriptor. */
#define CLDMA_AP_UL_CURRENT_ADDR_1          (0x0018)	/* The address of current processing TGPD descriptor. */
#define CLDMA_AP_UL_CURRENT_ADDR_2          (0x001C)	/* The address of current processing TGPD descriptor. */
#define CLDMA_AP_UL_CURRENT_ADDR_3          (0x0020)	/* The address of current processing TGPD descriptor. */
#define CLDMA_AP_UL_CURR_ADDR_4MSB          (0x0024)	/* The last updated address[35:32] 4MSB BIT of TGPD */
#define CLDMA_AP_UL_STATUS                  (0x0028)	/* UL SBDMA operation status. */
#define CLDMA_AP_UL_START_CMD               (0x0030)	/* UL START SBDMA command. */
#define CLDMA_AP_UL_RESUME_CMD              (0x0034)	/* UL RESUME SBDMA command. */
#define CLDMA_AP_UL_STOP_CMD                (0x0038)	/* UL STOP SBDMA command. */
#define CLDMA_AP_UL_ERROR                   (0x003C)	/* ERROR */
#define CLDMA_AP_UL_CFG                     (0x0040)	/* Operation Configuration */
#define CLDMA_AP_UL_LAST_UPDATE_ADDR_0      (0x0044)	/* The address of last updated TGPD descriptor. */
#define CLDMA_AP_UL_LAST_UPDATE_ADDR_1      (0x0048)	/* The address of last updated TGPD descriptor. */
#define CLDMA_AP_UL_LAST_UPDATE_ADDR_2      (0x004C)	/* The address of last updated TGPD descriptor. */
#define CLDMA_AP_UL_LAST_UPDATE_ADDR_3      (0x0050)	/* The address of last updated TGPD descriptor. */
#define CLDMA_AP_UL_LAST_UPDATE_ADDR_4MSB   (0x0054)	/* The last updated address[35:32] 4MSB BIT of TGPD */
#define CLDMA_AP_UL_DEBUG_0                 (0x0060)
#define CLDMA_AP_UL_DEBUG_1                 (0x0064)
#define CLDMA_AP_UL_DEBUG_2                 (0x0068)
#define CLDMA_AP_UL_DEBUG_3                 (0x006C)
/*========================CLDMA_PD_DL_CFG:1021B400-1021B420==================================*/
#define CLDMA_AP_SO_ERROR                   (0x0400)	/* ERROR */
#define CLDMA_AP_SO_START_CMD               (0x0404)	/* SME OUT SBDMA START command. */
#define CLDMA_AP_SO_RESUME_CMD              (0x0408)	/* SO OUTDMA RESUME command. */
#define CLDMA_AP_SO_STOP_CMD                (0x040C)	/* SO OUTDMA STOP command. */
#define CLDMA_AP_DL_DEBUG_0                 (0x0410)
#define CLDMA_AP_DL_DEBUG_1                 (0x0414)
#define CLDMA_AP_DL_DEBUG_2                 (0x0418)
#define CLDMA_AP_DL_DEBUG_3                 (0x041C)
/*===========================CLDMA_PD_AP_CFG:  1021B800-1021B8CC================================*/
#define CLDMA_AP_L2TISAR0		(0x0800) /* Level 2 Interrupt Status and Acknowledgment Register (TX Part) */
#define CLDMA_AP_L2TIMR0		(0x0804) /* Level 2 Interrupt Mask Register (TX Part) */
#define CLDMA_AP_L2TIMCR0		(0x0808) /* Level 2 Interrupt Mask Clear Register (TX Part) */
#define CLDMA_AP_L2TIMSR0		(0x080C) /* Level 2 Interrupt Mask Set Register (TX Part) */
#define CLDMA_AP_L3TISAR0		(0x0810) /* Level 3 Interrupt Status and Acknowledgment Register (TX Part) */
#define CLDMA_AP_L3TISAR1		(0x0814) /* Level 3 Interrupt Status and Acknowledgment Register (TX Part) */
#define CLDMA_AP_L3TIMR0		(0x0818) /* Level 3 Interrupt Mask Register (TX Part) */
#define CLDMA_AP_L3TIMR1		(0x081C) /* Level 3 Interrupt Mask Register (TX Part) */
#define CLDMA_AP_L3TIMCR0		(0x0820) /* Level 3 Interrupt Mask Clear Register (TX Part) */
#define CLDMA_AP_L3TIMCR1		(0x0824) /* Level 3 Interrupt Mask Clear Register (TX Part) */
#define CLDMA_AP_L3TIMSR0		(0x0828) /* Level 3 Interrupt Mask Set Register (TX Part) */
#define CLDMA_AP_L3TIMSR1		(0x082C) /* Level 3 Interrupt Mask Set Register (TX Part) */
#define CLDMA_AP_L2RISAR0		(0x0830) /* Level 2 Interrupt Status and Acknowledgment Register (RX Part) */
#define CLDMA_AP_L2RISAR1		(0x0834) /* Level 2 Interrupt Status and Acknowledgment Register (RX Part) */
#define CLDMA_AP_L3RISAR0		(0x0838) /* Level 3 Interrupt Status and Acknowledgment Register (RX Part) */
#define CLDMA_AP_L3RISAR1		(0x083C) /* Level 3 Interrupt Status and Acknowledgment Register (RX Part) */
#define CLDMA_AP_L3RIMR0		(0x0840) /* Level 3 Interrupt Mask Register (RX Part) */
#define CLDMA_AP_L3RIMR1		(0x0844) /* Level 3 Interrupt Mask Register (RX Part) */
#define CLDMA_AP_L3RIMCR0		(0x0848) /* Level 3 Interrupt Mask Clear Register (RX Part) */
#define CLDMA_AP_L3RIMCR1		(0x084C) /* Level 3 Interrupt Mask Clear Register (RX Part) */
#define CLDMA_AP_L3RIMSR0		(0x0850) /* Level 3 Interrupt MaskSet Register (RX Part) */
#define CLDMA_AP_L3RIMSR1		(0x0854) /* Level 3 Interrupt Mask Set Register (RX Part) */
#define CLDMA_AP_AXI_MI			(0x0858)
#define CLDMA_AP_CLDMA_IP_BUSY          (0x0860) /* CLDMA IP busy */
#define CLDMA_AP_HW_DVFS_CTRL		(0x0864) /*Hardware DVFS control Register*/
#define CLDMA_AP_QOS			(0x0868)
#define CLDMA_AP_DMA_ERR		(0x0870) /* exception status */
#define CLDMA_AP_DMA_ERR_MASK		(0x0874) /* exception mask */
#define CLDMA_AP_DEBUG0                 (0x0878)

/*assistant macros*/
#define CLDMA_AP_TQSAR(i)  (CLDMA_AP_UL_START_ADDR_0   + (4 * (i)))
#define CLDMA_AP_TQCPR(i)  (CLDMA_AP_UL_CURRENT_ADDR_0 + (4 * (i)))
#define CLDMA_AP_RQSAR(i)  (CLDMA_AP_SO_START_ADDR_0   + (4 * (i)))
#define CLDMA_AP_RQCPR(i)  (CLDMA_AP_SO_CURRENT_ADDR_0 + (4 * (i)))
#define CLDMA_AP_TQSABAK(i)  (CLDMA_AP_UL_START_ADDR_BK_0 + (4 * (i)))
#define CLDMA_AP_TQCPBAK(i)  (CLDMA_AP_UL_CURRENT_ADDR_BK_0 + (4 * (i)))

#define cldma_write32(b, a, v)			mt_reg_sync_writel(v, (b)+(a))
#define cldma_write16(b, a, v)			mt_reg_sync_writew(v, (b)+(a))
#define cldma_write8(b, a, v)			mt_reg_sync_writeb(v, (b)+(a))

#define cldma_read32(b, a)			ioread32((void __iomem *)((b)+(a)))
#define cldma_read16(b, a)			ioread16((void __iomem *)((b)+(a)))
#define cldma_read8(b, a)			ioread8((void __iomem *)((b)+(a)))


/*bitmap*/
#define CLDMA_BM_INT_ALL			0xFFFFFFFF
/* L2 interrupt */
#define CLDMA_TX_INT_ERROR		0x00000F00 /* error occurred on the specified queue, check L3 for detail */
#define CLDMA_TX_INT_QUEUE_EMPTY	0x000000F0 /* when there is no GPD to be transmitted on the specified queue */
#define CLDMA_TX_QE_OFFSET 4
#define CLDMA_TX_INT_DONE		0x0000000F /* when the transmission if the GPD on the specified queue is done */
#define CLDMA_RX_INT_ERROR		0x00000004 /* error occurred on the specified queue, check L3 for detail */
#define CLDMA_RX_INT_QUEUE_EMPTY	0x00000002 /* when there is no GPD to be transmitted on the specified queue */
#define CLDMA_RX_QE_OFFSET 1
#define CLDMA_RX_INT_DONE		0x00000001 /* when the transmission if the GPD on the specified queue is done */

#define CLDMA_BM_ALL_QUEUE 0x0F	/* all queues */

static inline void cldma_reg_set_4msb_val(void *base, unsigned int reg_4msb_offset, int idx, dma_addr_t addr)
{
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	unsigned int val = 0;

	val = cldma_read32(base, reg_4msb_offset);
	val &= ~(0xF << (idx * 4)); /*clear 0xF << i*4 bit */
	val |= (((addr >> 32) & 0xF) << (idx * 4)); /*set 4msb bit*/
	cldma_write32(base, reg_4msb_offset, val);
#endif
}
static inline unsigned int cldma_reg_get_4msb_val(void *base, unsigned int reg_4msb_offset, int idx)
{
	unsigned int val = 0;

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	val = cldma_read32(base, reg_4msb_offset);
	val &= (0xF << (idx * 4)); /*clear no need bit, keep 0xF << i*4 bit */
	val >>= (idx * 4); /* get 4msb bit*/
#endif
	return val;
}
static inline void cldma_reg_set_tx_start_addr(void *base, int idx, dma_addr_t addr)
{
	cldma_write32(base, CLDMA_AP_TQSAR(idx), (u32)addr);
	cldma_reg_set_4msb_val(base, CLDMA_AP_UL_START_ADDR_4MSB, idx, addr);
}
static inline void cldma_reg_set_tx_start_addr_bk(void *base, int idx, dma_addr_t addr)
{
	/*Set start bak address in power on domain*/
	cldma_write32(base, CLDMA_AP_TQSABAK(idx), (u32)addr);
	cldma_reg_set_4msb_val(base, CLDMA_AP_UL_START_ADDR_BK_4MSB, idx, addr);
}

static inline void cldma_reg_set_rx_start_addr(void *base, int idx, dma_addr_t addr)
{
	cldma_write32(base, CLDMA_AP_RQSAR(idx), (u32)addr);
	cldma_reg_set_4msb_val(base, CLDMA_AP_SO_START_ADDR_4MSB, idx, addr);
}
#endif				/* __CLDMA_REG_H__ */
