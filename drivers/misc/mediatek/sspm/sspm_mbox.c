/*
 * Copyright (C) 2011-2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <mt-plat/sync_write.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_fdt.h>
#include <linux/delay.h>
#include "sspm_define.h"
#include "sspm_mbox.h"
#include "sspm_helper.h"

struct sspm_mbox {
	int id;
	void __iomem *in_out;
	void __iomem *base;
	unsigned int size;
	unsigned int enable;
	sspm_ipi_isr isr;
	int irq_num;
};

struct sspm_mbox sspmmbox[SSPM_MBOX_MAX];
static unsigned int sspm_mbox_cnt;
static spinlock_t lock_mbox[SSPM_MBOX_MAX];

unsigned int sspm_mbox_size(int mbox)
{
	if (mbox >= sspm_mbox_cnt)
		return 0;

	return sspmmbox[mbox].size / MBOX_SLOT_SIZE;
}

uint32_t *sspm_mbox_addr(unsigned int mbox, unsigned int slot)
{
	struct sspm_mbox *desc = &sspmmbox[mbox];

	return (uint32_t *)(desc->base + (MBOX_SLOT_SIZE * slot));
}

int sspm_mbox_read(unsigned int mbox, unsigned int slot, void *data,
	unsigned int len)
{
	struct sspm_mbox *desc = &sspmmbox[mbox];

	if (data)
		memcpy_from_sspm(data, desc->base + (MBOX_SLOT_SIZE * slot),
				MBOX_SLOT_SIZE*len);

	return 0;
}

int sspm_mbox_write(unsigned int mbox, unsigned int slot, void *data,
	unsigned int len)
{
	struct sspm_mbox *desc = &sspmmbox[mbox];

	if (data)
		memcpy_to_sspm(desc->base + (MBOX_SLOT_SIZE * slot), data,
				len * MBOX_SLOT_SIZE);

	return 0;
}

int sspm_mbox_polling(unsigned int mbox, unsigned int irq, unsigned int slot,
	unsigned int *retdata, unsigned int retlen, unsigned int retries)
{
	struct sspm_mbox *desc;
	void __iomem *out_irq;
	unsigned int irqs = 0;
	unsigned long flags = 0;

	desc = &sspmmbox[mbox];

	irq = 0x1 << irq;

	out_irq = desc->in_out + MBOX_OUT_IRQ_OFS;

	spin_lock_irqsave(&lock_mbox[mbox], flags);
#if 0
	while (retries-- > 0) {
		irqs = readl(out_irq);

		if (irqs & irq)
			break;

		udelay(1);
	}
#else
	irqs = readl(out_irq);
#endif

	if (irqs & irq) {
		writel(irq, out_irq);
		spin_unlock_irqrestore(&lock_mbox[mbox], flags);

		if (retdata)
			memcpy_from_sspm(retdata, desc->base +
				(MBOX_SLOT_SIZE * slot), MBOX_SLOT_SIZE*retlen);

		return 0;
	}

	spin_unlock_irqrestore(&lock_mbox[mbox], flags);
	return -1;
}

int sspm_mbox_send(unsigned int mbox, unsigned int slot, unsigned int irq,
	void *data, unsigned int len)
{
	struct sspm_mbox *desc;
	unsigned int size;
	void __iomem *in_irq, *out_irq;
	unsigned long flags = 0;

	if (mbox >= sspm_mbox_cnt)
		return -1;

	size = sspm_mbox_size(mbox);

	if (slot > size || (slot + len) > size)
		return -1;

	desc = &sspmmbox[mbox];
	in_irq = desc->in_out + MBOX_IN_IRQ_OFS;
	out_irq = desc->in_out + MBOX_OUT_IRQ_OFS;

	spin_lock_irqsave(&lock_mbox[mbox], flags);
	if (readl(out_irq) & (0x1 << irq)) {
		pr_err("%s: MBOX%d[%d] out_irq is not clear!\n", __func__,
			mbox, irq);
		spin_unlock_irqrestore(&lock_mbox[desc->id], flags);
		return -1;
	}
	spin_unlock_irqrestore(&lock_mbox[desc->id], flags);

	if (!desc->enable)
		return -1;

	/* we only copy data to portion of mbox here .... */
	/* len:0, mean send ack (OUT_IRQ) only no data transfer */
	if (len > 0)
		memcpy_to_sspm(desc->base + (MBOX_SLOT_SIZE * slot), data,
				len * MBOX_SLOT_SIZE);

	spin_lock_irqsave(&lock_mbox[mbox], flags);
	writel(0x1 << irq, in_irq);
	spin_unlock_irqrestore(&lock_mbox[desc->id], flags);

	return 0;
}

/*
 * dispatch sspm mbox irq and then reset mbox out irq
 * @param irq:      irq id
 * @param dev_id:   pointer point to sspmmbox
 };
 */
static irqreturn_t sspm_mbox_irq_handler(int irq, void *dev_id)
{
	struct sspm_mbox *desc = (struct sspm_mbox *) dev_id;
	unsigned int irqs;
	void __iomem *out_irq;

	out_irq = desc->in_out + MBOX_OUT_IRQ_OFS;

	spin_lock(&lock_mbox[desc->id]);
	irqs = readl(out_irq);
	writel(irqs, out_irq);
	spin_unlock(&lock_mbox[desc->id]);

	if (desc->isr)
		desc->isr(desc->id, desc->base, irqs);

	return IRQ_HANDLED;
}

static int sspm_mbox_group_activate(int mbox, unsigned int is64d,
	sspm_ipi_isr isr)
{
	struct sspm_mbox *desc;
	char name[32];
	int ret;
	struct device *dev = &sspm_pdev->dev;
	struct resource *res;

	desc = &sspmmbox[mbox];

	desc->size = (is64d) ? SSPM_MBOX_8BYTE : SSPM_MBOX_4BYTE;
	desc->isr = isr;

	if (sspm_pdev) {
		snprintf(name, sizeof(name), "mbox%d_base", mbox);
		res = platform_get_resource_byname(sspm_pdev,
						IORESOURCE_MEM, name);
		desc->base = devm_ioremap_resource(dev, res);

		if (IS_ERR((void const *) desc->base)) {
			pr_err("[SSPM] MBOX %d can't remap BASE\n", mbox);
			goto fail;
		}

		snprintf(name, sizeof(name), "mbox%d_ctrl", mbox);
		res = platform_get_resource_byname(sspm_pdev, IORESOURCE_MEM,
						name);
		desc->in_out = devm_ioremap_resource(dev, res);
		if (IS_ERR((void const *) desc->in_out)) {
			pr_err("[SSPM] MBOX %d can't find IN_OUT_IRQ\n", mbox);
			goto fail;
		}

		snprintf(name, sizeof(name), "mbox%d", mbox);
		desc->irq_num = platform_get_irq_byname(sspm_pdev, name);
		if (desc->irq_num < 0) {
			pr_err("[SSPM] MBOX %d can't find IRQ\n", mbox);
			goto fail;
		}

		ret = request_irq(desc->irq_num, sspm_mbox_irq_handler,
				IRQF_TRIGGER_NONE, "SSPM_MBOX", (void *) desc);
		if (ret) {
			pr_err("[SSPM] MBOX %d request irq Failed\n", mbox);
			goto fail;
		}

		desc->enable = 1;
		desc->id = mbox;

	}

	return 0;

fail:
	return -1;
}

/*
 * parse device tree and mapping iomem
 * @return: 0 if success
 */
unsigned int sspm_mbox_init(unsigned int mode, unsigned int count,
	sspm_ipi_isr isr)
{
	int mbox;

	if (count > SSPM_MBOX_MAX) {
		pr_debug("[SSPM] %s(): count (%u) too large, set to %u\n",
			__func__, count, SSPM_MBOX_MAX);
		count = SSPM_MBOX_MAX;
	}

	for (mbox = 0; mbox < count; mbox++) {

		spin_lock_init(&lock_mbox[mbox]);

		if (sspm_mbox_group_activate(mbox, mode & 0x1, isr))
			goto fail;

		mode >>= 1;
	}

	sspm_mbox_cnt = mbox;

	pr_debug("[SSPM] Find %d MBOX\n", sspm_mbox_cnt);

	return 0;

fail:
	while (mbox >= 0) {
		if (sspmmbox[mbox].irq_num > 0) {
			free_irq(sspmmbox[mbox].irq_num, &sspmmbox[mbox]);
			sspmmbox[mbox].irq_num = 0;
		}

		mbox--;
	}
	return -1;
}
