// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/soc/mediatek/mtk-mbox.h>

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
	unsigned long flags;

	if (!mbdev) {
		pr_notice("[MBOX]write header fail, dev null");
		return MBOX_PLT_ERR;
	}

	if (mbox >= mbdev->count || !msg) {
		pr_notice("[MBOX]write header config err");
		return MBOX_PARA_ERR;
	}

	minfo = &(mbdev->info_table[mbox]);
	base = minfo->base;
	slot_ofs = slot * MBOX_SLOT_SIZE;
	size = minfo->slot;
	ipimsg = (struct mtk_ipi_msg *)msg;
	len = ipimsg->ipihd.len;

	if (len > size * MBOX_SLOT_SIZE)
		return MBOX_WRITE_SZ_ERR;

	spin_lock_irqsave(&mbdev->info_table[mbox].mbox_lock, flags);
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

	minfo->record.write_count++;
	spin_unlock_irqrestore(&mbdev->info_table[mbox].mbox_lock, flags);

	return MBOX_DONE;
}
EXPORT_SYMBOL_GPL(mtk_mbox_write_hd);

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
	unsigned long flags;

	if (!mbdev) {
		pr_notice("[MBOX]read header fail, dev null");
		return MBOX_PLT_ERR;
	}

	if (mbox >= mbdev->count || !dest) {
		pr_notice("[MBOX]read header config err");
		return MBOX_PARA_ERR;
	}

	minfo = &(mbdev->info_table[mbox]);
	base = minfo->base;
	slot_ofs = slot * MBOX_SLOT_SIZE;
	size = minfo->slot;
	ipihd = (struct mtk_ipi_msg_hd *)(base + slot_ofs);

	if (ipihd->len > size * MBOX_SLOT_SIZE)
		return MBOX_READ_SZ_ERR;

	spin_lock_irqsave(&mbdev->info_table[mbox].mbox_lock, flags);
	/*ipi header and payload*/
	if (mbdev->memcpy_from_tiny)
		mbdev->memcpy_from_tiny(dest, (void __iomem *)
			(base + slot_ofs + sizeof(struct mtk_ipi_msg_hd)),
			ipihd->len);
	else
		mtk_memcpy_from_tinysys(dest, (void __iomem *)
			(base + slot_ofs + sizeof(struct mtk_ipi_msg_hd)),
			ipihd->len);
	spin_unlock_irqrestore(&mbdev->info_table[mbox].mbox_lock, flags);

	return MBOX_DONE;
}
EXPORT_SYMBOL_GPL(mtk_mbox_read_hd);

/*
 * write data to mbox, function must in critical context
 */
int mtk_mbox_write(struct mtk_mbox_device *mbdev, unsigned int mbox,
		unsigned int slot, void *data, unsigned int len)
{
	unsigned int slot_ofs, size;
	struct mtk_mbox_info *minfo;
	void __iomem *base;
	unsigned long flags;

	if (!mbdev) {
		pr_notice("[MBOX]write fail, dev or ptr null");
		return MBOX_PLT_ERR;
	}

	if (mbox >= mbdev->count || !data)
		return MBOX_PARA_ERR;

	minfo = &(mbdev->info_table[mbox]);
	base = minfo->base;
	slot_ofs = slot * MBOX_SLOT_SIZE;
	size = minfo->slot;

	if (slot > size)
		return MBOX_WRITE_SZ_ERR;

	spin_lock_irqsave(&mbdev->info_table[mbox].mbox_lock, flags);
	if (mbdev->memcpy_to_tiny)
		mbdev->memcpy_to_tiny((void __iomem *)(base + slot_ofs),
			data, len);
	else
		mtk_memcpy_to_tinysys((void __iomem *)(base + slot_ofs),
			data, len);

	minfo->record.write_count++;
	spin_unlock_irqrestore(&mbdev->info_table[mbox].mbox_lock, flags);

	return MBOX_DONE;
}
EXPORT_SYMBOL_GPL(mtk_mbox_write);

/*
 * read data to user buffer, function must in critical context
 */
int mtk_mbox_read(struct mtk_mbox_device *mbdev, unsigned int mbox,
		unsigned int slot, void *data, unsigned int len)
{
	unsigned int slot_ofs, size;
	struct mtk_mbox_info *minfo;
	void __iomem *base;
	unsigned long flags;

	if (!mbdev || !data) {
		pr_notice("[MBOX]read fail,dev or ptr null");
		return MBOX_PLT_ERR;
	}

	if (mbox >= mbdev->count)
		return MBOX_PARA_ERR;

	minfo = &(mbdev->info_table[mbox]);
	base = minfo->base;
	slot_ofs = slot * MBOX_SLOT_SIZE;
	size = minfo->slot;

	if (slot > size)
		return MBOX_READ_SZ_ERR;

	spin_lock_irqsave(&mbdev->info_table[mbox].mbox_lock, flags);
	if (mbdev->memcpy_from_tiny)
		mbdev->memcpy_from_tiny(data,
			(void __iomem *)(base + slot_ofs), len);
	else
		mtk_memcpy_from_tinysys(data,
			(void __iomem *)(base + slot_ofs), len);
	spin_unlock_irqrestore(&mbdev->info_table[mbox].mbox_lock, flags);

	return MBOX_DONE;
}
EXPORT_SYMBOL_GPL(mtk_mbox_read);

/*
 * clear mbox irq,
 * with read/write function must in critical context
 */
int mtk_mbox_clr_irq(struct mtk_mbox_device *mbdev, unsigned int mbox,
		unsigned int irq)
{
	struct mtk_mbox_info *minfo;

	if (!mbdev)
		return MBOX_PLT_ERR;

	if (mbox >= mbdev->count)
		return MBOX_IRQ_ERR;

	minfo = &(mbdev->info_table[mbox]);
	writel(irq, minfo->clr_irq_reg);

	return MBOX_DONE;
}
EXPORT_SYMBOL_GPL(mtk_mbox_clr_irq);

/*
 * trigger mbox irq,
 * with read/write function must in critical context
 */
int mtk_mbox_trigger_irq(struct mtk_mbox_device *mbdev, unsigned int mbox,
		unsigned int irq)
{
	struct mtk_mbox_info *minfo;
	unsigned long flags;

	if (!mbdev)
		return MBOX_PLT_ERR;

	if (mbox >= mbdev->count)
		return MBOX_IRQ_ERR;

	minfo = &(mbdev->info_table[mbox]);
	spin_lock_irqsave(&mbdev->info_table[mbox].mbox_lock, flags);
	writel(irq, minfo->set_irq_reg);
	minfo->record.trig_irq_count++;
	spin_unlock_irqrestore(&mbdev->info_table[mbox].mbox_lock, flags);

	return MBOX_DONE;
}
EXPORT_SYMBOL_GPL(mtk_mbox_trigger_irq);

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
	unsigned long flags;

	if (!mbdev)
		return 0;

	if (mbox >= mbdev->count)
		return 0;

	spin_lock_irqsave(&mbdev->info_table[mbox].mbox_lock, flags);
	irq_state = 0;
	minfo = &(mbdev->info_table[mbox]);
	if (minfo->send_status_reg)
		reg = readl(minfo->send_status_reg);
	else
		reg = readl(minfo->set_irq_reg);

	irq_state = (reg & (0x1 << pin_index));

	if (irq_state)
		minfo->record.busy_count++;

	spin_unlock_irqrestore(&mbdev->info_table[mbox].mbox_lock, flags);

	return irq_state;
}
EXPORT_SYMBOL_GPL(mtk_mbox_check_send_irq);

/*
 * check mbox 32bits clr irq reg status
 * with read/write function must in critical context
 * @return irq status 0: not triggered , other: irq triggered
 */
unsigned int mtk_mbox_read_recv_irq(struct mtk_mbox_device *mbdev,
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
EXPORT_SYMBOL_GPL(mtk_mbox_read_recv_irq);

/*
 * set mbox base address to init register
 *
 */
int mtk_mbox_set_base_reg(struct mtk_mbox_device *mbdev, unsigned int mbox,
		unsigned int addr)
{
	struct mtk_mbox_info *minfo;

	if (!mbdev)
		return MBOX_PLT_ERR;

	if (mbox >= mbdev->count)
		return MBOX_PARA_ERR;

	minfo = &(mbdev->info_table[mbox]);
	writel(addr, minfo->init_base_reg);


	return MBOX_DONE;
}
EXPORT_SYMBOL_GPL(mtk_mbox_set_base_reg);

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
		return MBOX_PLT_ERR;

	if (mbox >= mbdev->count)
		return MBOX_PARA_ERR;

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
EXPORT_SYMBOL_GPL(mtk_mbox_set_base_addr);

/*
 * mtk_mbox_cb_register, register callback function
 *
 */
int mtk_mbox_cb_register(struct mtk_mbox_device *mbdev, unsigned int pin_offset,
		mbox_pin_cb_t mbox_pin_cb, void *prdata)
{
	struct mtk_mbox_pin_recv *pin_recv;

	if (!mbdev)
		return MBOX_PLT_ERR;

	pin_recv = &(mbdev->pin_recv_table[pin_offset]);
	pin_recv->mbox_pin_cb = mbox_pin_cb;
	pin_recv->prdata = prdata;

	return MBOX_DONE;
}
EXPORT_SYMBOL_GPL(mtk_mbox_cb_register);

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

	if (!mbdev)
		return MBOX_PLT_ERR;

	if (mbox >= mbdev->count)
		return MBOX_PARA_ERR;

	recv_pin_index = pin_recv->pin_index;
	minfo = &(mbdev->info_table[mbox]);

	spin_lock_irqsave(&mbdev->info_table[mbox].mbox_lock, flags);
	/*check lock for */
	if (pin_recv->lock == MBOX_PIN_BUSY) {
		spin_unlock_irqrestore(
			&mbdev->info_table[mbox].mbox_lock, flags);
		minfo->record.busy_count++;
		return MBOX_PIN_BUSY;
	}
	/*check bit*/
	reg = mtk_mbox_read_recv_irq(mbdev, mbox);
	irq_state = (reg & (0x1 << recv_pin_index));

	if (irq_state > 0) {
		/*clear bit*/
		ret = mtk_mbox_clr_irq(mbdev, mbox, irq_state);
	} else {
		spin_unlock_irqrestore(
			&mbdev->info_table[mbox].mbox_lock, flags);
		minfo->record.busy_count++;
		return MBOX_PIN_BUSY;
	}

	spin_unlock_irqrestore(&mbdev->info_table[mbox].mbox_lock, flags);
	/*copy data*/
	ret = mtk_mbox_read(mbdev, mbox, pin_recv->offset, data,
		pin_recv->msg_size * MBOX_SLOT_SIZE);

	if (ret != MBOX_DONE)
		return ret;

	pin_recv->recv_record.poll_count++;

	/*dump recv info*/
	if (mbdev->log_enable)
		mtk_mbox_dump_recv_pin(mbdev, pin_recv);

	return MBOX_DONE;
}
EXPORT_SYMBOL_GPL(mtk_mbox_polling);

/*
 * set lock status
 */
static void mtk_mbox_set_lock(struct mtk_mbox_device *mbdev, unsigned int mbox,
		unsigned int lock)
{
	struct mtk_mbox_pin_recv *pin_recv;
	int i;

	for (i = 0; i < mbdev->recv_count; i++) {
		pin_recv = &(mbdev->pin_recv_table[i]);
		if (pin_recv->mbox != mbox)
			continue;
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
	const uint64_t timeout_time = 50 * 1000 * 1000; /* 50ms */
	uint64_t start_time, end_time, cbtimediff;
	uint32_t execute_count = 0;
	//void *user_data;
	int ret;
	int i;

	start_time = cpu_clock(0);
	mbox = minfo->id;
	ret = MBOX_DONE;

	spin_lock_irqsave(&minfo->mbox_lock, flags);
	/*lock pin*/
	mtk_mbox_set_lock(mbdev, mbox, MBOX_PIN_BUSY);
	/*get irq status*/
	irq_status = mtk_mbox_read_recv_irq(mbdev, mbox);
	irq_temp = 0;
	spin_unlock_irqrestore(&minfo->mbox_lock, flags);

	if (mbdev->pre_cb && mbdev->pre_cb(mbdev->prdata)) {
		ret = MBOX_PRE_CB_ERR;
		goto skip;
	}

	/*execute all receive pin handler*/
	for (i = 0; i < mbdev->recv_count; i++) {
		pin_recv = &(mbdev->pin_recv_table[i]);
		if (pin_recv->mbox != mbox)
			continue;
		/*recv irq trigger*/
		if (((0x1 << pin_recv->pin_index) & irq_status) > 0x0) {
			pin_recv->recv_record.recv_irq_count++;
			irq_temp = irq_temp | (0x1 << pin_recv->pin_index);
			/*check user buf*/
			if (!pin_recv->pin_buf) {
				pr_err("[MBOX Error]null ptr dev=%s ipi_id=%d",
					mbdev->name, pin_recv->chan_id);
				BUG_ON(1);
			}
			if (minfo->opt == MBOX_OPT_QUEUE_DIR ||
			 minfo->opt == MBOX_OPT_QUEUE_SMEM) {
				/*queue mode*/
				ipihead = (struct mtk_ipi_msg_hd *)(minfo->base
					+ (pin_recv->offset * MBOX_SLOT_SIZE));
				ret = mtk_mbox_read_hd(mbdev, mbox,
					pin_recv->offset, pin_recv->pin_buf);

				if (pin_recv->recv_opt == MBOX_RECV_MESSAGE
					&& pin_recv->cb_ctx_opt
						== MBOX_CB_IN_ISR
					&& pin_recv->mbox_pin_cb
					&& ret == MBOX_DONE) {
					pin_recv->recv_record.pre_timestamp
						= cpu_clock(0);
					pin_recv->mbox_pin_cb(ipihead->id,
					pin_recv->prdata, pin_recv->pin_buf,
					(unsigned int)ipihead->len);
					pin_recv->recv_record.post_timestamp
						= cpu_clock(0);
					pin_recv->recv_record.cb_count++;
					execute_count++;
					cbtimediff = pin_recv->recv_record.post_timestamp
						- pin_recv->recv_record.pre_timestamp;
					if (cbtimediff > timeout_time) {
						pr_notice("[MBOX Error]dev=%s ipi_id=%d, timeout=%llu\n",
							mbdev->name, pin_recv->chan_id, cbtimediff);
					}

				}
			} else {
				/*direct mode*/
				ret = mtk_mbox_read(mbdev, mbox,
					pin_recv->offset, pin_recv->pin_buf,
					pin_recv->msg_size * MBOX_SLOT_SIZE);

				if (pin_recv->recv_opt == MBOX_RECV_MESSAGE
					&& pin_recv->cb_ctx_opt
						== MBOX_CB_IN_ISR
					&& pin_recv->mbox_pin_cb
					&& ret == MBOX_DONE) {
					pin_recv->recv_record.pre_timestamp
						= cpu_clock(0);
					pin_recv->mbox_pin_cb(pin_recv->chan_id,
					pin_recv->prdata, pin_recv->pin_buf,
					pin_recv->msg_size * MBOX_SLOT_SIZE);
					pin_recv->recv_record.post_timestamp
						= cpu_clock(0);
					pin_recv->recv_record.cb_count++;
					execute_count++;
					cbtimediff = pin_recv->recv_record.post_timestamp
						- pin_recv->recv_record.pre_timestamp;
					if (cbtimediff > timeout_time) {
						pr_notice("[MBOX Error]dev=%s ipi_id=%d, timeout=%llu\n",
							mbdev->name, pin_recv->chan_id, cbtimediff);
					}
				}
			}

			if (ret != MBOX_DONE)
				pr_err("[MBOX ISR]cp to buf fail,dev=%s chan=%d ret=%d",
				mbdev->name, pin_recv->chan_id, ret);

			/*dump recv info*/
			if (mbdev->log_enable)
				mtk_mbox_dump_recv(mbdev, i);
		}
	}

	if (mbdev->post_cb && mbdev->post_cb(mbdev->prdata))
		ret = MBOX_POST_CB_ERR;
skip:
	if (ret == MBOX_PRE_CB_ERR)
		pr_notice("[MBOX ISR] pre_cb error, skip cb handle, dev=%s ret=%d",
			mbdev->name, ret);
	if (ret == MBOX_POST_CB_ERR)
		pr_notice("[MBOX ISR] post_cb error, dev=%s ret=%d",
			mbdev->name, ret);

	/*clear irq status*/
	spin_lock_irqsave(&minfo->mbox_lock, flags);
	mtk_mbox_clr_irq(mbdev, mbox, irq_temp);
	/*release pin*/
	mtk_mbox_set_lock(mbdev, mbox, MBOX_DONE);
	spin_unlock_irqrestore(&minfo->mbox_lock, flags);

	if (irq_temp == 0 && irq_status != 0) {
		pr_err("[MBOX ISR]dev=%s pin table err, status=%x",
			mbdev->name, irq_status);
		for (i = 0; i < mbdev->recv_count; i++) {
			pin_recv = &(mbdev->pin_recv_table[i]);
			mtk_mbox_dump_recv_pin(mbdev, pin_recv);
		}
	}

	/*notify all receive pin handler*/
	for (i = 0; i < mbdev->recv_count; i++) {
		pin_recv = &(mbdev->pin_recv_table[i]);
		if (pin_recv->mbox != mbox)
			continue;
		/*recv irq trigger*/
		if (((0x1 << pin_recv->pin_index) & irq_status) > 0x0) {
			/*notify task*/
			if (mbdev->ipi_cb) {
				mbdev->ipi_cb(pin_recv, mbdev->ipi_priv);
				pin_recv->recv_record.notify_count++;
			}
		}
	}
	end_time = cpu_clock(0);
	if (end_time - start_time > timeout_time) {
		pr_notice("[MBOX Error]start=%llu, end=%llu diff=%llu, count=%u\n",
			start_time, end_time, end_time - start_time, execute_count);
	}

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
	return MBOX_CONFIG_ERR;
}
EXPORT_SYMBOL_GPL(mtk_smem_init);

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
		if (IS_ERR_OR_NULL(res)) {
			pr_info("MBOX %s:get resource %s failed!\n",
				__func__, name);
		} else {
			minfo->base = devm_ioremap_resource(dev, res);

			if (IS_ERR((void const *) minfo->base))
				pr_info("MBOX %d can't remap base\n", mbox);

			minfo->slot = (unsigned int)resource_size(res)/MBOX_SLOT_SIZE;
		}
		/*init reg*/
		snprintf(name, sizeof(name), "mbox%d_init", mbox);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
		if (IS_ERR_OR_NULL(res)) {
			pr_info("MBOX %s:get resource %s failed!\n",
				__func__, name);
		} else {
			minfo->init_base_reg = devm_ioremap_resource(dev, res);
			if (IS_ERR((void const *) minfo->init_base_reg))
				pr_info("MBOX %d can't find init reg\n", mbox);
		}
		/*set irq reg*/
		snprintf(name, sizeof(name), "mbox%d_set", mbox);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
		if (IS_ERR_OR_NULL(res)) {
			pr_info("MBOX %s:get resource %s failed!\n",
				__func__, name);
		} else {
			minfo->set_irq_reg = devm_ioremap_resource(dev, res);
			if (IS_ERR((void const *) minfo->set_irq_reg)) {
				pr_info("MBOX %d can't find set reg\n", mbox);
				goto mtk_mbox_probe_fail;
			}
		}
		/*clear reg*/
		snprintf(name, sizeof(name), "mbox%d_clr", mbox);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
		if (IS_ERR_OR_NULL(res)) {
			pr_info("MBOX %s:get resource %s failed!\n",
				__func__, name);
		} else {
			minfo->clr_irq_reg = devm_ioremap_resource(dev, res);
			if (IS_ERR((void const *) minfo->clr_irq_reg)) {
				pr_info("MBOX %d can't find clr reg\n", mbox);
				goto mtk_mbox_probe_fail;
			}
		}
		/*send status reg*/
		snprintf(name, sizeof(name), "mbox%d_send", mbox);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
		if (IS_ERR_OR_NULL(res)) {
			pr_info("MBOX %s:get resource %s failed!\n",
				__func__, name);
		} else {
			minfo->send_status_reg = devm_ioremap_resource(dev, res);
			if (IS_ERR((void const *) minfo->send_status_reg)) {
				pr_notice("MBOX %d can't find send status reg\n", mbox);
				minfo->send_status_reg = NULL;
			}
		}
		/*recv status reg*/
		snprintf(name, sizeof(name), "mbox%d_recv", mbox);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
		if (IS_ERR_OR_NULL(res)) {
			pr_info("MBOX %s:get resource %s failed!\n",
				__func__, name);
		} else {
			minfo->recv_status_reg = devm_ioremap_resource(dev, res);
			if (IS_ERR((void const *) minfo->recv_status_reg)) {
				pr_notice("MBOX %d can't find recv status reg\n", mbox);
				minfo->recv_status_reg = NULL;
			}
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
	return MBOX_CONFIG_ERR;
}
EXPORT_SYMBOL_GPL(mtk_mbox_probe);

/*
 *mbox print receive pin function
 */
void mtk_mbox_print_recv(struct mtk_mbox_device *mbdev,
	struct mtk_mbox_pin_recv *pin_recv)
{
	pr_notice("[MBOX]dev=%s recv mbox=%u off=%u cv_opt=%u ctx_opt=%u mg_sz=%u p_idx=%u id=%u\n"
		, mbdev->name
		, pin_recv->mbox
		, pin_recv->offset
		, pin_recv->recv_opt
		, pin_recv->cb_ctx_opt
		, pin_recv->msg_size
		, pin_recv->pin_index
		, pin_recv->chan_id);

	pr_notice("[MBOX]dev=%s recv id=%u poll=%u cv_irq=%u noti=%u cb=%u pre=%lld po=%lld\n"
		, mbdev->name
		, pin_recv->chan_id
		, pin_recv->recv_record.poll_count
		, pin_recv->recv_record.recv_irq_count
		, pin_recv->recv_record.notify_count
		, pin_recv->recv_record.cb_count
		, pin_recv->recv_record.pre_timestamp
		, pin_recv->recv_record.post_timestamp);
}
EXPORT_SYMBOL_GPL(mtk_mbox_print_recv);

/*
 *mbox print send pin function
 */
void mtk_mbox_print_send(struct mtk_mbox_device *mbdev,
	struct mtk_mbox_pin_send *pin_send)
{
	pr_notice("[MBOX]dev=%s send mbox=%u off=%u s_opt=%u mg_sz=%u p_idx=%u id=%u\n"
		, mbdev->name
		, pin_send->mbox
		, pin_send->offset
		, pin_send->send_opt
		, pin_send->msg_size
		, pin_send->pin_index
		, pin_send->chan_id);
}
EXPORT_SYMBOL_GPL(mtk_mbox_print_send);

/*
 *mbox print mbox function
 */
void mtk_mbox_print_minfo(struct mtk_mbox_device *mbdev,
	struct mtk_mbox_info *minfo)
{
	pr_notice("[MBOX]dev=%s mbox id=%u slot=%u opt=%u base=%p set_reg=%p clr_reg=%p init_reg=%p s_sta=%p cv_sta=%p\n"
		, mbdev->name
		, minfo->id
		, minfo->slot
		, minfo->opt
		, minfo->base
		, minfo->set_irq_reg
		, minfo->clr_irq_reg
		, minfo->init_base_reg
		, minfo->send_status_reg
		, minfo->recv_status_reg);

	pr_notice("[MBOX]dev=%s write=%u busy=%u tri_irq=%u\n"
		, mbdev->name
		, minfo->record.write_count
		, minfo->record.busy_count
		, minfo->record.trig_irq_count);
}
EXPORT_SYMBOL_GPL(mtk_mbox_print_minfo);

/*
 *mbox information dump
 */
void mtk_mbox_dump_all(struct mtk_mbox_device *mbdev)
{
	struct mtk_mbox_pin_recv *pin_recv;
	struct mtk_mbox_pin_send *pin_send;
	struct mtk_mbox_info *minfo;
	int i;

	if (!mbdev)
		return;

	pr_notice("[MBOX]dev=%s recv count=%u send count=%u\n",
		mbdev->name, mbdev->recv_count, mbdev->send_count);

	for (i = 0; i < mbdev->recv_count; i++) {
		pin_recv = &(mbdev->pin_recv_table[i]);
		mtk_mbox_print_recv(mbdev, pin_recv);
	}

	for (i = 0; i < mbdev->send_count; i++) {
		pin_send = &(mbdev->pin_send_table[i]);
		mtk_mbox_print_send(mbdev, pin_send);
	}

	for (i = 0; i < mbdev->count; i++) {
		minfo = &(mbdev->info_table[i]);
		mtk_mbox_print_minfo(mbdev, minfo);
	}
}
EXPORT_SYMBOL_GPL(mtk_mbox_dump_all);

/*
 *mbox single receive pin information dump
 */
void mtk_mbox_dump_recv(struct mtk_mbox_device *mbdev, unsigned int pin)
{
	struct mtk_mbox_pin_recv *pin_recv;

	if (mbdev) {
		if (pin < mbdev->recv_count) {
			pin_recv = &(mbdev->pin_recv_table[pin]);
			mtk_mbox_print_recv(mbdev, pin_recv);
		}
	}
}
EXPORT_SYMBOL_GPL(mtk_mbox_dump_recv);

/*
 *mbox single receive pin information dump
 */
void mtk_mbox_dump_recv_pin(struct mtk_mbox_device *mbdev,
	struct mtk_mbox_pin_recv *pin_recv)
{
	unsigned int irq_reg;

	if (mbdev && pin_recv) {
		irq_reg = mtk_mbox_read_recv_irq(mbdev, pin_recv->mbox);
		pr_err("[MBOX]dev=%s mbox=%u recv irq status=%x\n",
			mbdev->name, pin_recv->mbox, irq_reg);
		mtk_mbox_print_recv(mbdev, pin_recv);
	}
}
EXPORT_SYMBOL_GPL(mtk_mbox_dump_recv_pin);

/*
 *mbox single send pin information dump
 */
void mtk_mbox_dump_send(struct mtk_mbox_device *mbdev, unsigned int pin)
{
	struct mtk_mbox_pin_send *pin_send;

	if (mbdev) {
		if (pin < mbdev->send_count) {
			pin_send = &(mbdev->pin_send_table[pin]);
			mtk_mbox_print_send(mbdev, pin_send);
		}
	}
}
EXPORT_SYMBOL_GPL(mtk_mbox_dump_send);

/*
 *mbox single mbox information dump
 */
void mtk_mbox_dump(struct mtk_mbox_device *mbdev, unsigned int mbox)
{
	struct mtk_mbox_info *minfo;

	if (mbdev) {
		if (mbox < mbdev->count) {
			minfo = &(mbdev->info_table[mbox]);
			mtk_mbox_print_minfo(mbdev, minfo);
		}
	}
}
EXPORT_SYMBOL_GPL(mtk_mbox_dump);

/*
 *mbox log enable function
 */
int mtk_mbox_log_enable(struct mtk_mbox_device *mbdev, bool enable)
{
	if (!mbdev)
		return MBOX_PLT_ERR;

	mbdev->log_enable = enable;
	return MBOX_DONE;
}
EXPORT_SYMBOL_GPL(mtk_mbox_log_enable);

/*
 *mbox reset record
 */
void mtk_mbox_reset_record(struct mtk_mbox_device *mbdev)
{
	struct mtk_mbox_pin_recv *pin_recv;
	struct mtk_mbox_info *minfo;
	int i;

	if (!mbdev)
		return;

	for (i = 0; i < mbdev->recv_count; i++) {
		pin_recv = &(mbdev->pin_recv_table[i]);
		pin_recv->recv_record.poll_count = 0;
		pin_recv->recv_record.recv_irq_count = 0;
		pin_recv->recv_record.notify_count = 0;
		pin_recv->recv_record.cb_count = 0;
		pin_recv->recv_record.pre_timestamp = 0;
		pin_recv->recv_record.post_timestamp = 0;
	}

	for (i = 0; i < mbdev->count; i++) {
		minfo = &(mbdev->info_table[i]);
		minfo->record.write_count = 0;
		minfo->record.busy_count = 0;
		minfo->record.trig_irq_count = 0;
	}

}
EXPORT_SYMBOL_GPL(mtk_mbox_reset_record);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek Tinysys Mbox driver");
