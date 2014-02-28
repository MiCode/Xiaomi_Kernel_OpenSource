/*
 * Based on linux/arch/arm/include/asm/dma.h
 */
#ifndef __ASM_ARM_DMA_H
#define __ASM_ARM_DMA_H

/*
 * This is the maximum virtual address which can be DMA'd from.
 */
#define MAX_DMA_ADDRESS	(~0ULL)

#ifdef CONFIG_PCI
extern int isa_dma_bridge_buggy;
#else
#define isa_dma_bridge_buggy    (0)
#endif

#endif /* __ASM_ARM_DMA_H */