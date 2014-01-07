/*
 * Intel Smart Sound Technology (SST) DSP Core Driver
 *
 * Copyright (C) 2013, Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/slab.h>
#include <linux/export.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "sst-dsp.h"
#include "sst-dsp-priv.h"

#define CREATE_TRACE_POINTS
#include <trace/events/sst.h>

/* Public API */
void sst_dsp_shim_write(struct sst_dsp *sst, u32 offset, u32 value)
{
	unsigned long flags;

	spin_lock_irqsave(&sst->spinlock, flags);
	sst->ops->write(sst->addr.shim, offset, value);
	spin_unlock_irqrestore(&sst->spinlock, flags);
}
EXPORT_SYMBOL(sst_dsp_shim_write);

u32 sst_dsp_shim_read(struct sst_dsp *sst, u32 offset)
{
	unsigned long flags;
	u32 val;

	spin_lock_irqsave(&sst->spinlock, flags);
	val = sst->ops->read(sst->addr.shim, offset);
	spin_unlock_irqrestore(&sst->spinlock, flags);

	return val;
}
EXPORT_SYMBOL(sst_dsp_shim_read);

void sst_dsp_shim_write64(struct sst_dsp *sst, u32 offset, u64 value)
{
	unsigned long flags;

	spin_lock_irqsave(&sst->spinlock, flags);
	sst->ops->write64(sst->addr.shim, offset, value);
	spin_unlock_irqrestore(&sst->spinlock, flags);
}
EXPORT_SYMBOL(sst_dsp_shim_write64);

u64 sst_dsp_shim_read64(struct sst_dsp *sst, u32 offset)
{
	unsigned long flags;
	u64 val;

	spin_lock_irqsave(&sst->spinlock, flags);
	val = sst->ops->read64(sst->addr.shim, offset);
	spin_unlock_irqrestore(&sst->spinlock, flags);

	return val;
}
EXPORT_SYMBOL(sst_dsp_shim_read64);

void sst_dsp_shim_write_unlocked(struct sst_dsp *sst, u32 offset, u32 value)
{
	sst->ops->write(sst->addr.shim, offset, value);
}
EXPORT_SYMBOL(sst_dsp_shim_write_unlocked);

u32 sst_dsp_shim_read_unlocked(struct sst_dsp *sst, u32 offset)
{
	return sst->ops->read(sst->addr.shim, offset);
}
EXPORT_SYMBOL(sst_dsp_shim_read_unlocked);

void sst_dsp_shim_write64_unlocked(struct sst_dsp *sst, u32 offset, u64 value)
{
	sst->ops->write64(sst->addr.shim, offset, value);
}
EXPORT_SYMBOL(sst_dsp_shim_write64_unlocked);

u64 sst_dsp_shim_read64_unlocked(struct sst_dsp *sst, u32 offset)
{
	return sst->ops->read64(sst->addr.shim, offset);
}
EXPORT_SYMBOL(sst_dsp_shim_read64_unlocked);

int sst_dsp_shim_update_bits(struct sst_dsp *sst, u32 offset,
				u32 mask, u32 value)
{
	unsigned long flags;
	bool change;
	u32 old, new;

	spin_lock_irqsave(&sst->spinlock, flags);
	old = sst_dsp_shim_read_unlocked(sst, offset);

	new = (old & (~mask)) | (value & mask);

	change = (old != new);
	if (change)
		sst_dsp_shim_write_unlocked(sst, offset, new);

	spin_unlock_irqrestore(&sst->spinlock, flags);
	return change;
}
EXPORT_SYMBOL(sst_dsp_shim_update_bits);

int sst_dsp_shim_update_bits64(struct sst_dsp *sst, u32 offset,
				u64 mask, u64 value)
{
	unsigned long flags;
	bool change;
	u64 old, new;

	spin_lock_irqsave(&sst->spinlock, flags);
	old = sst_dsp_shim_read64_unlocked(sst, offset);

	new = (old & (~mask)) | (value & mask);

	change = (old != new);
	if (change)
		sst_dsp_shim_write64_unlocked(sst, offset, new);

	spin_unlock_irqrestore(&sst->spinlock, flags);
	return change;
}
EXPORT_SYMBOL(sst_dsp_shim_update_bits64);

int sst_dsp_shim_update_bits_unlocked(struct sst_dsp *sst, u32 offset,
				u32 mask, u32 value)
{
	bool change;
	unsigned int old, new;
	u32 ret;

	ret = sst_dsp_shim_read_unlocked(sst, offset);

	old = ret;
	new = (old & (~mask)) | (value & mask);

	change = (old != new);
	if (change)
		sst_dsp_shim_write_unlocked(sst, offset, new);

	return change;
}
EXPORT_SYMBOL(sst_dsp_shim_update_bits_unlocked);

int sst_dsp_shim_update_bits64_unlocked(struct sst_dsp *sst, u32 offset,
				u64 mask, u64 value)
{
	bool change;
	u64 old, new;

	old = sst_dsp_shim_read64_unlocked(sst, offset);

	new = (old & (~mask)) | (value & mask);

	change = (old != new);
	if (change)
		sst_dsp_shim_write64_unlocked(sst, offset, new);

	return change;
}
EXPORT_SYMBOL(sst_dsp_shim_update_bits64_unlocked);

void sst_dsp_dump(struct sst_dsp *sst)
{
	sst->ops->dump(sst);
}
EXPORT_SYMBOL(sst_dsp_dump);

void sst_dsp_reset(struct sst_dsp *sst)
{
	sst->ops->reset(sst);
}
EXPORT_SYMBOL(sst_dsp_reset);

int sst_dsp_boot(struct sst_dsp *sst)
{
	sst->ops->boot(sst);
	return 0;
}
EXPORT_SYMBOL(sst_dsp_boot);

void sst_dsp_ipc_msg_tx(struct sst_dsp *dsp, u32 msg)
{
	sst_dsp_shim_write_unlocked(dsp, SST_IPCX, msg | SST_IPCX_BUSY);
	trace_sst_ipc_msg_tx(msg);
}
EXPORT_SYMBOL_GPL(sst_dsp_ipc_msg_tx);

u32 sst_dsp_ipc_msg_rx(struct sst_dsp *dsp)
{
	u32 msg;

	msg = sst_dsp_shim_read_unlocked(dsp, SST_IPCX);
	trace_sst_ipc_msg_rx(msg);

	return msg;
}
EXPORT_SYMBOL_GPL(sst_dsp_ipc_msg_rx);

void sst_dsp_write(struct sst_dsp *sst, void *src, u32 dest_offset,
	size_t bytes)
{
	sst->ops->ram_write(sst, sst->addr.lpe + dest_offset, src, bytes);
}
EXPORT_SYMBOL(sst_dsp_write);

void sst_dsp_read(struct sst_dsp *sst, void *dest, u32 src_offset,
	size_t bytes)
{
	sst->ops->ram_read(sst, dest, sst->addr.lpe + src_offset, bytes);
}
EXPORT_SYMBOL(sst_dsp_read);

int sst_dsp_mailbox_init(struct sst_dsp *sst, u32 inbox_offset, size_t inbox_size,
	u32 outbox_offset, size_t outbox_size)
{
	sst->mailbox.in_base = sst->addr.lpe + inbox_offset;
	sst->mailbox.out_base = sst->addr.lpe + outbox_offset;
	sst->mailbox.in_size = inbox_size;
	sst->mailbox.out_size = outbox_size;
	return 0;
}
EXPORT_SYMBOL(sst_dsp_mailbox_init);

void sst_dsp_outbox_write(struct sst_dsp *sst, void *message, size_t bytes)
{
	int i;

	trace_sst_ipc_outbox_write(bytes);

	memcpy_toio(sst->mailbox.out_base, message, bytes);

	for (i = 0; i < bytes; i += 4)
		trace_sst_ipc_outbox_wdata(i, *(uint32_t *)(message + i));
}
EXPORT_SYMBOL(sst_dsp_outbox_write);

void sst_dsp_outbox_read(struct sst_dsp *sst, void *message, size_t bytes)
{
	int i;

	trace_sst_ipc_outbox_read(bytes);

	memcpy_fromio(message, sst->mailbox.out_base, bytes);

	for (i = 0; i < bytes; i += 4)
		trace_sst_ipc_outbox_rdata(i, *(uint32_t *)(message + i));
}
EXPORT_SYMBOL(sst_dsp_outbox_read);

void sst_dsp_inbox_write(struct sst_dsp *sst, void *message, size_t bytes)
{
	int i;

	trace_sst_ipc_inbox_write(bytes);

	memcpy_toio(sst->mailbox.in_base, message, bytes);

	for (i = 0; i < bytes; i += 4)
		trace_sst_ipc_inbox_wdata(i, *(uint32_t *)(message + i));
}
EXPORT_SYMBOL(sst_dsp_inbox_write);

void sst_dsp_inbox_read(struct sst_dsp *sst, void *message, size_t bytes)
{
	int i;

	trace_sst_ipc_inbox_read(bytes);

	memcpy_fromio(message, sst->mailbox.in_base, bytes);

	for (i = 0; i < bytes; i += 4)
		trace_sst_ipc_inbox_rdata(i, *(uint32_t *)(message + i));
}
EXPORT_SYMBOL(sst_dsp_inbox_read);

void *sst_dsp_get_thread_context(struct sst_dsp *sst)
{
	return sst->thread_context;
}
EXPORT_SYMBOL(sst_dsp_get_thread_context);

struct sst_dsp *sst_dsp_new(struct device *dev,
	struct sst_dsp_device *sst_dev, struct sst_pdata *pdata)
{
	struct sst_dsp *sst;
	int err;

	dev_dbg(dev, "initialising audio DSP id 0x%x\n", pdata->id);

	sst = devm_kzalloc(dev, sizeof(*sst), GFP_KERNEL);
	if (sst == NULL)
		return NULL;

	spin_lock_init(&sst->spinlock);
	mutex_init(&sst->mutex);
	sst->dev = dev;
	sst->thread_context = sst_dev->thread_context;
	sst->sst_dev = sst_dev;
	sst->id = pdata->id;
	sst->irq = pdata->irq;
	sst->ops = sst_dev->ops;
	sst->pdata = pdata;
	INIT_LIST_HEAD(&sst->used_block_list);
	INIT_LIST_HEAD(&sst->free_block_list);
	INIT_LIST_HEAD(&sst->module_list);
	INIT_LIST_HEAD(&sst->fw_list);

	/* Initialise SST Audio DSP */
	if (sst->ops->init) {
		err = sst->ops->init(sst, pdata);
		if (err < 0)
			return NULL;
	}

	/* Register the ISR */
	err = request_threaded_irq(sst->irq, sst->ops->irq_handler,
		sst_dev->thread, IRQF_SHARED, "AudioDSP", sst);
	if (err)
		goto irq_err;

	/* Register the FW loader DMA controller if we have one */
	if (pdata->dma_engine)
		err = sst_dma_new(sst);
	if (err)
		goto dma_err;

	return sst;

dma_err:
	free_irq(sst->irq, sst);
irq_err:
	if (sst->ops->free)
		sst->ops->free(sst);

	return NULL;
}
EXPORT_SYMBOL(sst_dsp_new);

void sst_dsp_free(struct sst_dsp *sst)
{
	if (sst->pdata->dma_engine)
		sst_dma_free(sst->dma);
	free_irq(sst->irq, sst);
	if (sst->ops->free)
		sst->ops->free(sst);
}
EXPORT_SYMBOL(sst_dsp_free);

/* Module information */
MODULE_AUTHOR("Liam Girdwood");
MODULE_DESCRIPTION("Intel SST Core");
MODULE_LICENSE("GPL v2");
