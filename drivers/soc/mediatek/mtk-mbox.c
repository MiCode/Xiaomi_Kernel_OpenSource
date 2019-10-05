/*
 * Copyright (C) 2019 MediaTek Inc.
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


#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <mt-plat/mtk-mbox.h>

/*
 * memory copy to tiny
 * @param dest: dest address
 * @param src: src address
 * @param size: memory size
 */
void mtk_memcpy_to_tinysys(void __iomem *dest, const void *src, int size)
{
	int i;
	u32 __iomem *t = dest;
	const u32 *s = src;

	for (i = 0; i < ((size + 3) >> 2); i++)
		*t++ = *s++;
}

/*
 * memory copy from tiny
 * @param dest: dest address
 * @param src: src address
 * @param size: memory size
 */
void mtk_memcpy_from_tinysys(void *dest, const void __iomem *src, int size)
{
	int i;
	u32 *t = dest;
	const u32 __iomem *s = src;

	for (i = 0; i < ((size + 3) >> 2); i++)
		*t++ = *s++;
}

/*
 * write data to mbox with ipi msg header
 * function must in critical context
 */
int mtk_mbox_write_hd(struct mtk_mbox_device *mbdev, unsigned int mbox,
		unsigned int slot, void *msg)
{
	unsigned int slot_ofs, size;
	struct mtk_mbox_info *minfo;
	struct mtk_ipi_msg *ipimsg;
	void __iomem *base;
	int len;

	if (!mbdev || mbox >= mbdev->count || !msg) {
		pr_notice("[MBOX]write header fail, dev or ptr null");
		return MBOX_PARA_ERROR;
	}

	minfo = &(mbdev->info_table[mbox]);
	base = minfo->base;
	slot_ofs = slot * MBOX_SLOT_SIZE;
	size = minfo->slot;
	ipimsg = (struct mtk_ipi_msg *)msg;
	len = ipimsg->ipihd.len;

	/*ipi header and payload*/
	if (mbdev->memcpy_to_tiny) {
		mbdev->memcpy_to_tiny((void __iomem *)(base + slot_ofs),
			ipimsg, sizeof(struct mtk_ipi_msg_hd));
		mbdev->memcpy_to_tiny((void __iomem *)
			(base + slot_ofs + sizeof(struct mtk_ipi_msg_hd)),
			ipimsg->data, len);
	} else {
		mtk_memcpy_to_tinysys((void __iomem *)(base + slot_ofs),
			ipimsg, sizeof(struct mtk_ipi_msg_hd));
		mtk_memcpy_to_tinysys((void __iomem *)
		(base + slot_ofs + sizeof(struct mtk_ipi_msg_hd)),
			ipimsg->data, len);
	}

	return MBOX_DONE;
}

/*
 * read data from mbox with ipi msg header
 * function must in critical context
 */
int mtk_mbox_read_hd(struct mtk_mbox_device *mbdev, unsigned int mbox,
		unsigned int slot, void *dest)
{
	unsigned int slot_ofs, size;
	struct mtk_mbox_info *minfo;
	struct mtk_ipi_msg_hd *ipihd;
	void __iomem *base;

	if (!mbdev || mbox >= mbdev->count || !dest) {
		pr_notice("[MBOX]read header fail, dev or ptr null");
		return MBOX_PARA_ERROR;
	}

	minfo = &(mbdev->info_table[mbox]);
	base = minfo->base;
	slot_ofs = slot * MBOX_SLOT_SIZE;
	size = minfo->slot;
	ipihd = (struct mtk_ipi_msg_hd *)(base + slot_ofs);

	/*ipi header and payload*/
	if (mbdev->memcpy_from_tiny)
		mbdev->memcpy_from_tiny(dest, (void __iomem *)
			(base + slot_ofs + sizeof(struct mtk_ipi_msg_hd)),
			ipihd->len);
	else
		mtk_memcpy_from_tinysys(dest, (void __iomem *)
			(base + slot_ofs + sizeof(struct mtk_ipi_msg_hd)),
			ipihd->len);

	return MBOX_DONE;
}

/*
 * write data to mbox, function must in critical context
 */
int mtk_mbox_write(struct mtk_mbox_device *mbdev, unsigned int mbox,
		unsigned int slot, void *data, unsigned int len)
{
	unsigned int slot_ofs, size;
	struct mtk_mbox_info *minfo;
	void __iomem *base;

	if (!mbdev || !data) {
		pr_notice("[MBOX]write fail, dev or ptr null");
		return MBOX_PARA_ERROR;
	}

	if (mbox >= mbdev->count)
		return MBOX_PARA_ERROR;

	minfo = &(mbdev->info_table[mbox]);
	base = minfo->base;
	slot_ofs = slot * MBOX_SLOT_SIZE;
	size = minfo->slot;

	if (slot > size)
		return MBOX_PARA_ERROR;

	if (mbdev->memcpy_to_tiny)
		mbdev->memcpy_to_tiny((void __iomem *)(base + slot_ofs),
			data, len);
	else
		mtk_memcpy_to_tinysys((void __iomem *)(base + slot_ofs),
			data, len);


	return MBOX_DONE;
}

/*
 * read data to user buffer, function must in critical context
 */
int mtk_mbox_read(struct mtk_mbox_device *mbdev, unsigned int mbox,
		unsigned int slot, void *data, unsigned int len)
{
	unsigned int slot_ofs, size;
	struct mtk_mbox_info *minfo;
	void __iomem *base;

	if (!mbdev || !data) {
		pr_notice("[MBOX]read fail,dev or ptr null");
		return MBOX_PARA_ERROR;
	}

	if (mbox >= mbdev->count)
		return MBOX_PARA_ERROR;

	minfo = &(mbdev->info_table[mbox]);
	base = minfo->base;
	slot_ofs = slot * MBOX_SLOT_SIZE;
	size = minfo->slot;

	if (slot > size)
		return MBOX_PARA_ERROR;

	if (mbdev->memcpy_from_tiny)
		mbdev->memcpy_from_tiny(data,
			(void __iomem *)(base + slot_ofs), len);
	else
		mtk_memcpy_from_tinysys(data,
			(void __iomem *)(base + slot_ofs), len);

	return MBOX_DONE;
}

/*
 * clear mbox irq,
 * with read/write function must in critical context
 */
int mtk_mbox_clr_irq(struct mtk_mbox_device *mbdev, unsigned int mbox,
		unsigned int irq)
{
	struct mtk_mbox_info *minfo;

	if (!mbdev)
		return MBOX_PARA_ERROR;

	if (mbox >= mbdev->count)
		return MBOX_PARA_ERROR;

	minfo = &(mbdev->info_table[mbox]);
	writel(irq, minfo->clr_irq_reg);

	return MBOX_DONE;
}

/*
 * trigger mbox irq,
 * with read/write function must in critical context
 */
int mtk_mbox_trigger_irq(struct mtk_mbox_device *mbdev, unsigned int mbox,
		unsigned int irq)
{
	struct mtk_mbox_info *minfo;

	if (!mbdev)
		return MBOX_PARA_ERROR;

	if (mbox >= mbdev->count)
		return MBOX_PARA_ERROR;

	minfo = &(mbdev->info_table[mbox]);
	writel(irq, minfo->set_irq_reg);

	return MBOX_DONE;
}

/*
 * check mbox 32bits set irq reg status
 * with read/write function must in critical context
 * @return irq status 0: not triggered , other: irq triggered
 */
unsigned int mtk_mbox_check_send_irq(struct mtk_mbox_device *mbdev,
		unsigned int mbox, unsigned int pin_index)
{
	struct mtk_mbox_info *minfo;
	unsigned int reg, irq_state;

	if (!mbdev)
		return 0;

	if (mbox >= mbdev->count)
		return 0;

	irq_state = 0;
	minfo = &(mbdev->info_table[mbox]);
	if (minfo->send_status_reg)
		reg = readl(minfo->send_status_reg);
	else
		reg = readl(minfo->set_irq_reg);
	irq_state = (reg & (0x1 << pin_index));

	return irq_state;
}

/*
 * check mbox 32bits clr irq reg status
 * with read/write function must in critical context
 * @return irq status 0: not triggered , other: irq triggered
 */
unsigned int mbox_read_recv_irq(struct mtk_mbox_device *mbdev,
		unsigned int mbox)
{
	struct mtk_mbox_info *minfo;
	unsigned int reg;

	if (!mbdev)
		return 0;

	if (mbox >= mbdev->count)
		return 0;

	minfo = &(mbdev->info_table[mbox]);
	if (minfo->recv_status_reg)
		reg = readl(minfo->recv_status_reg);
	else
		reg = readl(minfo->clr_irq_reg);

	return reg;
}

/*
 * set mbox base address to init register
 *
 */
int mtk_mbox_set_base_reg(struct mtk_mbox_device *mbdev, unsigned int mbox,
		unsigned int addr)
{
	struct mtk_mbox_info *minfo;

	if (!mbdev)
		return MBOX_PARA_ERROR;

	if (mbox >= mbdev->count)
		return MBOX_PARA_ERROR;

	minfo = &(mbdev->info_table[mbox]);
	writel(addr, minfo->init_base_reg);

	return MBOX_DONE;
}

/*
 * set mbox base address, task context
 *
 */
int mtk_mbox_set_base_addr(struct mtk_mbox_device *mbdev, unsigned int mbox,
		unsigned int addr)
{
	struct mtk_mbox_info *minfo;
	unsigned long flags;
	int ret;

	if (!mbdev)
		return MBOX_PARA_ERROR;

	if (mbox >= mbdev->count)
		return MBOX_PARA_ERROR;

	spin_lock_irqsave(&mbdev->info_table[mbox].mbox_lock, flags);

	ret = mtk_mbox_set_base_reg(mbdev, mbox, addr);

	if (ret != MBOX_DONE) {
		spin_unlock_irqrestore(
			&mbdev->info_table[mbox].mbox_lock, flags);
		return ret;
	}

	minfo = &(mbdev->info_table[mbox]);
	writel(addr, minfo->base);

	spin_unlock_irqrestore(&mbdev->info_table[mbox].mbox_lock, flags);

	return MBOX_DONE;
}

/*
 * mtk_mbox_cb_register, register callback function
 *
 */
int mtk_mbox_cb_register(struct mtk_mbox_device *mbdev, unsigned int pin_offset,
		mbox_pin_cb_t mbox_pin_cb, void *prdata)
{
	struct mtk_mbox_pin_recv *pin_recv;

	if (!mbdev)
		return MBOX_PARA_ERROR;

	pin_recv = &(mbdev->pin_recv_table[pin_offset]);
	pin_recv->mbox_pin_cb = mbox_pin_cb;
	pin_recv->prdata = prdata;

	return MBOX_DONE;
}

/*
 * mbox polling, context is protected by mbox_lock
 */
int mtk_mbox_polling(struct mtk_mbox_device *mbdev, unsigned int mbox,
		void *data, struct mtk_mbox_pin_recv *pin_recv)
{
	struct mtk_mbox_info *minfo;
	unsigned long flags;
	unsigned int reg, irq_state;
	unsigned int recv_pin_index;
	int ret;

	recv_pin_index = pin_recv->pin_index;
	minfo = &(mbdev->info_table[mbox]);

	spin_lock_irqsave(&mbdev->info_table[mbox].mbox_lock, flags);
	/*check lock for */
	if (pin_recv->lock == MBOX_PIN_BUSY) {
		spin_unlock_irqrestore(
			&mbdev->info_table[mbox].mbox_lock, flags);
		return MBOX_PIN_BUSY;
	}
	/*check bit*/
	reg = mbox_read_recv_irq(mbdev, mbox);
	irq_state = (reg & (0x1 << recv_pin_index));

	if (irq_state > 0) {
		/*clear bit*/
		ret = mtk_mbox_clr_irq(mbdev, mbox, irq_state);
	} else {
		spin_unlock_irqrestore(
			&mbdev->info_table[mbox].mbox_lock, flags);
		return MBOX_PIN_BUSY;
	}

	spin_unlock_irqrestore(&mbdev->info_table[mbox].mbox_lock, flags);
	/*copy data*/
	mtk_mbox_read(mbdev, mbox, pin_recv->offset, data,
		pin_recv->msg_size * MBOX_SLOT_SIZE);

	return MBOX_DONE;
}


/*
 * set lock status
 */
static void mtk_mbox_set_lock(struct mtk_mbox_device *mbdev, unsigned int lock)
{
	struct mtk_mbox_pin_recv *pin_recv;
	int i;

	for (i = 0; i < mbdev->recv_count; i++) {
		pin_recv = &(mbdev->pin_recv_table[i]);
		pin_recv->lock = lock;
	}
}


/*
 * mbox driver isr, in isr context
 */
static irqreturn_t mtk_mbox_isr(int irq, void *dev_id)
{
	unsigned int mbox, irq_status, irq_temp;
	struct mtk_mbox_pin_recv *pin_recv;
	struct mtk_mbox_info *minfo = (struct mtk_mbox_info *)dev_id;
	struct mtk_mbox_device *mbdev = minfo->mbdev;
	struct mtk_ipi_msg_hd *ipihead;
	unsigned long flags;
	//void *user_data;
	int ret;
	int i;

	mbox = minfo->id;
	ret = MBOX_DONE;

	spin_lock_irqsave(&minfo->mbox_lock, flags);
	/*lock pin*/
	mtk_mbox_set_lock(mbdev, MBOX_PIN_BUSY);
	/*get irq status*/
	irq_status = mbox_read_recv_irq(mbdev, mbox);
	irq_temp = 0;
	spin_unlock_irqrestore(&minfo->mbox_lock, flags);

	if (mbdev->pre_cb)
		mbdev->pre_cb(mbdev->prdata);

	/*execute all receive pin handler*/
	for (i = 0; i < mbdev->recv_count; i++) {
		pin_recv = &(mbdev->pin_recv_table[i]);
		if (pin_recv->mbox != mbox)
			continue;
		/*recv irq trigger*/
		if (((0x1 << pin_recv->pin_index) & irq_status) > 0x0) {
			irq_temp = irq_temp | (0x1 << pin_recv->pin_index);
			/*check user buf*/
			if (!pin_recv->pin_buf) {
				pr_err("[MBOX Error]null ptr mbox_dev=%d ipi_id=%d",
					mbdev->id, pin_recv->chan_id);
				BUG_ON(1);
			}
			if (minfo->opt == MBOX_OPT_QUEUE_DIR ||
			 minfo->opt == MBOX_OPT_QUEUE_SMEM) {
				/*queue mode*/
				ipihead = (struct mtk_ipi_msg_hd *)(minfo->base
					+ (pin_recv->offset * MBOX_SLOT_SIZE));
				ret = mtk_mbox_read_hd(mbdev, mbox,
					pin_recv->offset, pin_recv->pin_buf);

				if (pin_recv->recv_opt == MBOX_RECV
					&& pin_recv->cb_ctx_opt
						== MBOX_CB_IN_ISR
					&& pin_recv->mbox_pin_cb
					&& ret == MBOX_DONE) {
					pin_recv->mbox_pin_cb(ipihead->id,
					pin_recv->prdata, pin_recv->pin_buf,
					(unsigned int)ipihead->len);
				}
			} else {
				/*direct mode*/
				ret = mtk_mbox_read(mbdev, mbox,
					pin_recv->offset, pin_recv->pin_buf,
					pin_recv->msg_size * MBOX_SLOT_SIZE);

				if (pin_recv->recv_opt == MBOX_RECV
					&& pin_recv->cb_ctx_opt
						== MBOX_CB_IN_ISR
					&& pin_recv->mbox_pin_cb
					&& ret == MBOX_DONE)
					pin_recv->mbox_pin_cb(pin_recv->chan_id,
					pin_recv->prdata, pin_recv->pin_buf,
					pin_recv->msg_size * MBOX_SLOT_SIZE);
			}
			/*notify task*/
			if (ret == MBOX_DONE && mbdev->ipi_cb)
				mbdev->ipi_cb(pin_recv);
		}
	}

	if (ret != MBOX_DONE)
		pr_notice("[MBOX ISR]read fail,dev=%d chan=%d",
			mbdev->id, pin_recv->chan_id);

	if (mbdev->post_cb)
		mbdev->post_cb(mbdev->prdata);

	/*clear irq status*/
	spin_lock_irqsave(&minfo->mbox_lock, flags);
	mtk_mbox_clr_irq(mbdev, mbox, irq_temp);
	/*release pin*/
	mtk_mbox_set_lock(mbdev, MBOX_DONE);
	spin_unlock_irqrestore(&minfo->mbox_lock, flags);

	return IRQ_HANDLED;
}

/*
 *  mtk_smem_init, initial share memory
 *
 */
int mtk_smem_init(struct platform_device *pdev, struct mtk_mbox_device *mbdev,
		unsigned int mbox, void __iomem *base,
		void __iomem *set_irq_reg, void __iomem *clr_irq_reg,
		void __iomem *send_status_reg, void __iomem *recv_status_reg)
{
	struct mtk_mbox_info *minfo;
	char name[32];
	int ret;

	minfo = &(mbdev->info_table[mbox]);

	minfo->base = base;
	minfo->set_irq_reg = set_irq_reg;
	minfo->clr_irq_reg = clr_irq_reg;
	minfo->send_status_reg = send_status_reg;
	minfo->recv_status_reg = recv_status_reg;
	minfo->enable = true;
	minfo->id = mbox;
	minfo->mbdev = mbdev;
	minfo->is64d = 0;
	spin_lock_init(&minfo->mbox_lock);

	snprintf(name, sizeof(name), "mbox%d", mbox);
	minfo->irq_num = platform_get_irq_byname(pdev, name);
	if (minfo->irq_num < 0) {
		pr_err("MBOX %d can't find IRQ\n", mbox);
		goto smem_fail;
	}

	ret = request_irq(minfo->irq_num, mtk_mbox_isr,	IRQF_TRIGGER_NONE,
		"MBOX_ISR", (void *) minfo);
	if (ret) {
		pr_err("MBOX %d request irq Failed\n", mbox);
		goto smem_fail;
	}

	return MBOX_DONE;

smem_fail:
	return MBOX_CONFIG_ERROR;
}

/*
 * mtk_mbox_probe , porbe and initial mbox
 *
 */
int mtk_mbox_probe(struct platform_device *pdev, struct mtk_mbox_device *mbdev,
		unsigned int mbox)
{
	struct mtk_mbox_info *minfo;
	char name[32];
	int ret;
	struct device *dev = &pdev->dev;
	struct resource *res;

	minfo = &(mbdev->info_table[mbox]);

	if (pdev) {
		snprintf(name, sizeof(name), "mbox%d_base", mbox);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
		minfo->base = devm_ioremap_resource(dev, res);

		if (IS_ERR((void const *) minfo->base))
			pr_err("MBOX %d can't remap base\n", mbox);

		minfo->slot = (unsigned int)resource_size(res)/MBOX_SLOT_SIZE;

		/*init reg*/
		snprintf(name, sizeof(name), "mbox%d_init", mbox);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
		minfo->init_base_reg = devm_ioremap_resource(dev, res);
		if (IS_ERR((void const *) minfo->init_base_reg))
			pr_err("MBOX %d can't find init reg\n", mbox);
		/*set irq reg*/
		snprintf(name, sizeof(name), "mbox%d_set", mbox);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
		minfo->set_irq_reg = devm_ioremap_resource(dev, res);
		if (IS_ERR((void const *) minfo->set_irq_reg)) {
			pr_err("MBOX %d can't find set reg\n", mbox);
			goto mtk_mbox_probe_fail;
		}
		/*clear reg*/
		snprintf(name, sizeof(name), "mbox%d_clr", mbox);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
		minfo->clr_irq_reg = devm_ioremap_resource(dev, res);
		if (IS_ERR((void const *) minfo->clr_irq_reg)) {
			pr_err("MBOX %d can't find clr reg\n", mbox);
			goto mtk_mbox_probe_fail;
		}
		/*send status reg*/
		snprintf(name, sizeof(name), "mbox%d_send", mbox);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
		minfo->send_status_reg = devm_ioremap_resource(dev, res);
		if (IS_ERR((void const *) minfo->send_status_reg)) {
			pr_notice("MBOX %d can't find send status reg\n", mbox);
			minfo->send_status_reg = NULL;
		}
		/*recv status reg*/
		snprintf(name, sizeof(name), "mbox%d_recv", mbox);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
		minfo->recv_status_reg = devm_ioremap_resource(dev, res);
		if (IS_ERR((void const *) minfo->recv_status_reg)) {
			pr_notice("MBOX %d can't find recv status reg\n", mbox);
			minfo->recv_status_reg = NULL;
		}

		snprintf(name, sizeof(name), "mbox%d", mbox);
		minfo->irq_num = platform_get_irq_byname(pdev, name);
		if (minfo->irq_num < 0) {
			pr_err("MBOX %d can't find IRQ\n", mbox);
			goto mtk_mbox_probe_fail;
		}

		minfo->enable = true;
		minfo->id = mbox;
		minfo->mbdev = mbdev;
		spin_lock_init(&minfo->mbox_lock);

		ret = request_irq(minfo->irq_num, mtk_mbox_isr,
				IRQF_TRIGGER_NONE, "MBOX_ISR", (void *) minfo);
		if (ret) {
			pr_err("MBOX %d request irq Failed\n", mbox);
			goto mtk_mbox_probe_fail;
		}
	}

	return MBOX_DONE;

mtk_mbox_probe_fail:
	return MBOX_CONFIG_ERROR;
}

/*
 *mbox information dump
 */
void mtk_mbox_info_dump(struct mtk_mbox_device *mbdev)
{
	struct mtk_mbox_pin_recv *pin_recv;
	struct mtk_mbox_pin_send *pin_send;
	int i;

	pr_notice("[MBOX]dev=%u recv count=%u send count=%u",
		mbdev->id, mbdev->recv_count, mbdev->send_count);

	for (i = 0; i < mbdev->recv_count; i++) {
		pin_recv = &(mbdev->pin_recv_table[i]);
		pr_notice("[MBOX]recv pin:%d mbox=%u offset=%u recv_opt=%u cb_ctx_opt=%u msg_size=%u pin_index=%u chan_id=%u",
			i, pin_recv->mbox, pin_recv->offset, pin_recv->recv_opt,
			pin_recv->cb_ctx_opt, pin_recv->msg_size,
			pin_recv->pin_index, pin_recv->chan_id);
	}

	for (i = 0; i < mbdev->send_count; i++) {
		pin_send = &(mbdev->pin_send_table[i]);
		pr_notice("[MBOX]send pin:%d mbox=%u offset=%u send_opt=%u msg_size=%u pin_index=%u chan_id=%u",
			i, pin_send->mbox, pin_send->offset, pin_send->send_opt,
			pin_send->msg_size, pin_send->pin_index,
			pin_send->chan_id);
	}
}
