#ifndef __MT_GIC_H
#define __MT_GIC_H

enum
{
	IRQ_MASK_HEADER = 0xF1F1F1F1,
	IRQ_MASK_FOOTER = 0xF2F2F2F2
};

#define MT_EDGE_SENSITIVE 0
#define MT_LEVEL_SENSITIVE 1

struct mtk_irq_mask
{
	unsigned int header;   /* for error checking */
	__u32 mask0;
	__u32 mask1;
	__u32 mask2;
	__u32 mask3;
	__u32 mask4;
	__u32 mask5;
	__u32 mask6;
	__u32 mask7;
	__u32 mask8;
	unsigned int footer;   /* for error checking */
};


void mt_irq_unmask_for_sleep(unsigned int irq);
void mt_irq_mask_for_sleep(unsigned int irq);
int mt_irq_mask_all(struct mtk_irq_mask *mask);
int mt_irq_mask_restore(struct mtk_irq_mask *mask);
void mt_irq_set_pending_for_sleep(unsigned int irq);


#endif
