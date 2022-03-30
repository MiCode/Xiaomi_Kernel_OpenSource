// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include "apu.h"
#include "apu_mbox.h"

#define _INBOX(_m)		(_m->apu_mbox + 0x0)
#define _OUTBOX(_m)		(_m->apu_mbox + 0x20)
#define _DUMMY(_m)		(_m->apu_mbox + 0x40)
#define _INBOX_IRQ(_m)		(_m->apu_mbox + 0xc0)
#define _OUTBOX_IRQ(_m)		(_m->apu_mbox + 0xc4)
#define _INBOX_IRQ_MASK(_m)	(_m->apu_mbox + 0xd0)
#define _OUTBOX_IRQ_MASK(_m)	(_m->apu_mbox + 0xd8)

#define WAIT_INBOX_TMO_MS	1000

void apu_mbox_ack_outbox(struct mtk_apu *apu)
{
	iowrite32(ioread32(_OUTBOX_IRQ(apu)),
		  _OUTBOX_IRQ(apu));
}

void apu_mbox_read_outbox(struct mtk_apu *apu, struct apu_mbox_hdr *hdr)
{
	unsigned int i, val;

	for (i = 0; i < APU_MBOX_HDR_SLOTS; i++) {
		val = ioread32(_OUTBOX(apu) + i * APU_MBOX_SLOT_SIZE);
		((unsigned int *)hdr)[i] = val;
	}
}

int apu_mbox_wait_inbox(struct mtk_apu *apu)
{
	unsigned long timeout;
	unsigned char irq, mask;

	timeout = jiffies + msecs_to_jiffies(WAIT_INBOX_TMO_MS);
	do {
		irq = ioread32(_INBOX_IRQ(apu));
		mask = ioread32(_INBOX_IRQ_MASK(apu));
		if (!(irq & ~mask))
			return 0;

		if (time_after(jiffies, timeout))
			break;
	} while (irq & ~mask);

	/* second chance */
	irq = ioread32(_INBOX_IRQ(apu));
	mask = ioread32(_INBOX_IRQ_MASK(apu));
	if (!(irq & ~mask))
		return 0;

	dev_info(apu->dev, "timeout. irq = 0x%x, mask = 0x%x\n",
		irq, mask);
	return -EBUSY;
}

void apu_mbox_write_inbox(struct mtk_apu *apu, struct apu_mbox_hdr *hdr)
{
	unsigned int i, val;

	for (i = 0; i < APU_MBOX_HDR_SLOTS; i++) {
		val = ((unsigned int *)hdr)[i];
		iowrite32(val, _INBOX(apu) +  i * APU_MBOX_SLOT_SIZE);
	}
}

void apu_mbox_inbox_init(struct mtk_apu *apu)
{
	iowrite32(~(1 << (APU_MBOX_HDR_SLOTS - 1)),
		  _INBOX_IRQ_MASK(apu));
}

void apu_mbox_hw_init(struct mtk_apu *apu)
{
	apu_mbox_inbox_init(apu);

	/* clear outbox IRQ */
	apu_mbox_ack_outbox(apu);
}

void apu_mbox_hw_exit(struct mtk_apu *apu)
{
	/* mask inbox IRQ */
	iowrite32(0xff, _INBOX_IRQ_MASK(apu));
}

