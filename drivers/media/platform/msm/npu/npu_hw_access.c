/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

/* -------------------------------------------------------------------------
 * Includes
 * -------------------------------------------------------------------------
 */
#include <linux/msm_dma_iommu_mapping.h>
#include <soc/qcom/subsystem_restart.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include "npu_hw_access.h"
#include "npu_common.h"
#include "npu_hw.h"

/* -------------------------------------------------------------------------
 * Functions - Register
 * -------------------------------------------------------------------------
 */
uint32_t npu_core_reg_read(struct npu_device *npu_dev, uint32_t off)
{
	uint32_t ret = 0;

	ret = readl_relaxed(npu_dev->core_io.base + off);
	__iormb();
	return ret;
}

void npu_core_reg_write(struct npu_device *npu_dev, uint32_t off, uint32_t val)
{
	writel_relaxed(val, npu_dev->core_io.base + off);
	__iowmb();
}

uint32_t npu_bwmon_reg_read(struct npu_device *npu_dev, uint32_t off)
{
	uint32_t ret = 0;

	ret = readl_relaxed(npu_dev->bwmon_io.base + off);
	__iormb();
	return ret;
}

void npu_bwmon_reg_write(struct npu_device *npu_dev, uint32_t off,
	uint32_t val)
{
	writel_relaxed(val, npu_dev->bwmon_io.base + off);
	__iowmb();
}

uint32_t npu_qfprom_reg_read(struct npu_device *npu_dev, uint32_t off)
{
	uint32_t ret = 0;

	if (npu_dev->qfprom_io.base) {
		ret = readl_relaxed(npu_dev->qfprom_io.base + off);
		__iormb();
	}

	return ret;
}

/* -------------------------------------------------------------------------
 * Functions - Memory
 * -------------------------------------------------------------------------
 */
void npu_mem_write(struct npu_device *npu_dev, void *dst, void *src,
	uint32_t size)
{
	size_t dst_off = (size_t)dst;
	uint32_t *src_ptr32 = (uint32_t *)src;
	uint8_t *src_ptr8 = 0;
	uint32_t i = 0;
	uint32_t num = 0;

	num = size/4;
	for (i = 0; i < num; i++) {
		writel_relaxed(src_ptr32[i], npu_dev->tcm_io.base + dst_off);
		dst_off += 4;
	}

	if (size%4 != 0) {
		src_ptr8 = (uint8_t *)((size_t)src + (num*4));
		num = size%4;
		for (i = 0; i < num; i++) {
			writeb_relaxed(src_ptr8[i], npu_dev->tcm_io.base +
				dst_off);
			dst_off += 1;
		}
	}
}

int32_t npu_mem_read(struct npu_device *npu_dev, void *src, void *dst,
	uint32_t size)
{
	size_t src_off = (size_t)src;
	uint32_t *out32 = (uint32_t *)dst;
	uint8_t *out8 = 0;
	uint32_t i = 0;
	uint32_t num = 0;

	num = size/4;
	for (i = 0; i < num; i++) {
		out32[i] = readl_relaxed(npu_dev->tcm_io.base + src_off);
		src_off += 4;
	}

	if (size%4 != 0) {
		out8 = (uint8_t *)((size_t)dst + (num*4));
		num = size%4;
		for (i = 0; i < num; i++) {
			out8[i] = readb_relaxed(npu_dev->tcm_io.base + src_off);
			src_off += 1;
		}
	}
	return 0;
}

void *npu_ipc_addr(void)
{
	return (void *)(IPC_MEM_OFFSET_FROM_SSTCM);
}

/* -------------------------------------------------------------------------
 * Functions - Interrupt
 * -------------------------------------------------------------------------
 */
void npu_interrupt_ack(struct npu_device *npu_dev, uint32_t intr_num)
{
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	uint32_t wdg_irq_sts = 0, error_irq_sts = 0;

	/* Clear irq state */
	REGW(npu_dev, NPU_MASTERn_IPC_IRQ_OUT(0), 0x0);

	wdg_irq_sts = REGR(npu_dev, NPU_MASTERn_WDOG_IRQ_STATUS(0));
	if (wdg_irq_sts != 0) {
		pr_err("wdg irq %x\n", wdg_irq_sts);
		host_ctx->wdg_irq_sts |= wdg_irq_sts;
		host_ctx->fw_error = true;
	}

	error_irq_sts = REGR(npu_dev, NPU_MASTERn_ERROR_IRQ_STATUS(0));
	error_irq_sts &= REGR(npu_dev, NPU_MASTERn_ERROR_IRQ_ENABLE(0));
	if (error_irq_sts != 0) {
		REGW(npu_dev, NPU_MASTERn_ERROR_IRQ_CLEAR(0), error_irq_sts);
		pr_err("error irq %x\n", error_irq_sts);
		host_ctx->err_irq_sts |= error_irq_sts;
		host_ctx->fw_error = true;
	}
}

int32_t npu_interrupt_raise_m0(struct npu_device *npu_dev)
{
	/* Bit 4 is setting IRQ_SOURCE_SELECT to local
	 * and we're triggering a pulse to NPU_MASTER0_IPC_IN_IRQ0
	 */
	npu_core_reg_write(npu_dev, NPU_MASTERn_IPC_IRQ_IN_CTRL(0), 0x1
		<< NPU_MASTER0_IPC_IRQ_IN_CTRL__IRQ_SOURCE_SELECT___S | 0x1);

	return 0;
}

int32_t npu_interrupt_raise_dsp(struct npu_device *npu_dev)
{
	npu_core_reg_write(npu_dev, NPU_MASTERn_IPC_IRQ_OUT_CTRL(1), 0x8);

	return 0;
}

/* -------------------------------------------------------------------------
 * Functions - ION Memory
 * -------------------------------------------------------------------------
 */
static struct npu_ion_buf *npu_alloc_npu_ion_buffer(struct npu_client
	*client, int buf_hdl, uint32_t size)
{
	struct npu_ion_buf *ret_val = NULL, *tmp;
	struct list_head *pos = NULL;

	mutex_lock(&client->list_lock);
	list_for_each(pos, &(client->mapped_buffer_list)) {
		tmp = list_entry(pos, struct npu_ion_buf, list);
		if (tmp->fd == buf_hdl) {
			ret_val = tmp;
			break;
		}
	}

	if (ret_val) {
		/* mapped already, treat as invalid request */
		pr_err("ion buf %x has been mapped\n");
		ret_val = NULL;
	} else {
		ret_val = kzalloc(sizeof(*ret_val), GFP_KERNEL);
		if (ret_val) {
			ret_val->fd = buf_hdl;
			ret_val->size = size;
			ret_val->iova = 0;
			list_add(&(ret_val->list),
				&(client->mapped_buffer_list));
		}
	}
	mutex_unlock(&client->list_lock);

	return ret_val;
}

static struct npu_ion_buf *npu_get_npu_ion_buffer(struct npu_client
	*client, int buf_hdl)
{
	struct list_head *pos = NULL;
	struct npu_ion_buf *ret_val = NULL, *tmp;

	mutex_lock(&client->list_lock);
	list_for_each(pos, &(client->mapped_buffer_list)) {
		tmp = list_entry(pos, struct npu_ion_buf, list);
		if (tmp->fd == buf_hdl) {
			ret_val = tmp;
			break;
		}
	}
	mutex_unlock(&client->list_lock);

	return ret_val;
}

static void npu_free_npu_ion_buffer(struct npu_client
	*client, int buf_hdl)
{
	struct list_head *pos = NULL;
	struct npu_ion_buf *npu_ion_buf = NULL;

	mutex_lock(&client->list_lock);
	list_for_each(pos, &(client->mapped_buffer_list)) {
		npu_ion_buf = list_entry(pos, struct npu_ion_buf, list);
		if (npu_ion_buf->fd == buf_hdl) {
			list_del(&npu_ion_buf->list);
			kfree(npu_ion_buf);
			break;
		}
	}
	mutex_unlock(&client->list_lock);
}

int npu_mem_map(struct npu_client *client, int buf_hdl, uint32_t size,
	uint64_t *addr)
{
	int ret = 0;
	struct npu_device *npu_dev = client->npu_dev;
	struct npu_ion_buf *ion_buf = NULL;
	struct npu_smmu_ctx *smmu_ctx = &npu_dev->smmu_ctx;

	if (buf_hdl == 0)
		return -EINVAL;

	ion_buf = npu_alloc_npu_ion_buffer(client, buf_hdl, size);
	if (!ion_buf) {
		pr_err("%s fail to alloc npu_ion_buffer\n", __func__);
		ret = -ENOMEM;
		return ret;
	}

	smmu_ctx->attach_cnt++;

	ion_buf->dma_buf = dma_buf_get(ion_buf->fd);
	if (IS_ERR_OR_NULL(ion_buf->dma_buf)) {
		pr_err("dma_buf_get failed %d\n", ion_buf->fd);
		ret = -ENOMEM;
		ion_buf->dma_buf = NULL;
		goto map_end;
	}

	ion_buf->attachment = dma_buf_attach(ion_buf->dma_buf,
			&(npu_dev->pdev->dev));
	if (IS_ERR(ion_buf->attachment)) {
		ret = -ENOMEM;
		ion_buf->attachment = NULL;
		goto map_end;
	}

	ion_buf->attachment->dma_map_attrs = DMA_ATTR_IOMMU_USE_UPSTREAM_HINT;

	ion_buf->table = dma_buf_map_attachment(ion_buf->attachment,
			DMA_BIDIRECTIONAL);
	if (IS_ERR(ion_buf->table)) {
		pr_err("npu dma_buf_map_attachment failed\n");
		ret = -ENOMEM;
		ion_buf->table = NULL;
		goto map_end;
	}

	dma_sync_sg_for_device(&(npu_dev->pdev->dev), ion_buf->table->sgl,
		ion_buf->table->nents, DMA_BIDIRECTIONAL);
	ion_buf->iova = ion_buf->table->sgl->dma_address;
	ion_buf->size = ion_buf->dma_buf->size;
	*addr = ion_buf->iova;
	pr_debug("mapped mem addr:0x%llx size:0x%x\n", ion_buf->iova,
		ion_buf->size);
map_end:
	if (ret)
		npu_mem_unmap(client, buf_hdl, 0);

	return ret;
}

void npu_mem_invalidate(struct npu_client *client, int buf_hdl)
{
	struct npu_device *npu_dev = client->npu_dev;
	struct npu_ion_buf *ion_buf = npu_get_npu_ion_buffer(client,
		buf_hdl);

	if (!ion_buf)
		pr_err("%s cant find ion buf\n", __func__);
	else
		dma_sync_sg_for_cpu(&(npu_dev->pdev->dev), ion_buf->table->sgl,
			ion_buf->table->nents, DMA_BIDIRECTIONAL);
}

bool npu_mem_verify_addr(struct npu_client *client, uint64_t addr)
{
	struct npu_ion_buf *ion_buf = 0;
	struct list_head *pos = NULL;
	bool valid = false;

	mutex_lock(&client->list_lock);
	list_for_each(pos, &(client->mapped_buffer_list)) {
		ion_buf = list_entry(pos, struct npu_ion_buf, list);
		if (ion_buf->iova == addr) {
			valid = true;
			break;
		}
	}
	mutex_unlock(&client->list_lock);

	return valid;
}

void npu_mem_unmap(struct npu_client *client, int buf_hdl,  uint64_t addr)
{
	struct npu_device *npu_dev = client->npu_dev;
	struct npu_ion_buf *ion_buf = 0;

	/* clear entry and retrieve the corresponding buffer */
	ion_buf = npu_get_npu_ion_buffer(client, buf_hdl);
	if (!ion_buf) {
		pr_err("%s could not find buffer\n", __func__);
		return;
	}

	if (ion_buf->iova != addr)
		pr_warn("unmap address %lu doesn't match %lu\n", addr,
			ion_buf->iova);

	if (ion_buf->table)
		dma_buf_unmap_attachment(ion_buf->attachment, ion_buf->table,
			DMA_BIDIRECTIONAL);
	if (ion_buf->dma_buf && ion_buf->attachment)
		dma_buf_detach(ion_buf->dma_buf, ion_buf->attachment);
	if (ion_buf->dma_buf)
		dma_buf_put(ion_buf->dma_buf);
	npu_dev->smmu_ctx.attach_cnt--;

	pr_debug("unmapped mem addr:0x%llx size:0x%x\n", ion_buf->iova,
		ion_buf->size);
	npu_free_npu_ion_buffer(client, buf_hdl);
}

/* -------------------------------------------------------------------------
 * Functions - Features
 * -------------------------------------------------------------------------
 */
uint8_t npu_hw_clk_gating_enabled(void)
{
	return 1;
}

uint8_t npu_hw_log_enabled(void)
{
	return 1;
}

/* -------------------------------------------------------------------------
 * Functions - Subsystem/PIL
 * -------------------------------------------------------------------------
 */
void *subsystem_get_local(char *sub_system)
{
	return subsystem_get(sub_system);
}

void subsystem_put_local(void *sub_system_handle)
{
	return subsystem_put(sub_system_handle);
}

/* -------------------------------------------------------------------------
 * Functions - Log
 * -------------------------------------------------------------------------
 */
void npu_process_log_message(struct npu_device *npu_dev, uint32_t *message,
	uint32_t size)
{
	struct npu_debugfs_ctx *debugfs = &npu_dev->debugfs_ctx;

	/* mutex log lock */
	mutex_lock(&debugfs->log_lock);

	if ((debugfs->log_num_bytes_buffered + size) >
		debugfs->log_buf_size) {
		/* No more space, invalidate it all and start over */
		debugfs->log_read_index = 0;
		debugfs->log_write_index = size;
		debugfs->log_num_bytes_buffered = size;
		memcpy(debugfs->log_buf, message, size);
	} else {
		if ((debugfs->log_write_index + size) >
			debugfs->log_buf_size) {
			/* Wrap around case */
			uint8_t *src_addr = (uint8_t *)message;
			uint8_t *dst_addr = 0;
			uint32_t remaining_to_end = debugfs->log_buf_size -
				debugfs->log_write_index + 1;
			dst_addr = debugfs->log_buf + debugfs->log_write_index;
			memcpy(dst_addr, src_addr, remaining_to_end);
			src_addr = &(src_addr[remaining_to_end]);
			dst_addr = debugfs->log_buf;
			memcpy(dst_addr, src_addr, size-remaining_to_end);
			debugfs->log_write_index = size-remaining_to_end;
		} else {
			memcpy((debugfs->log_buf + debugfs->log_write_index),
				message, size);
			debugfs->log_write_index += size;
			if (debugfs->log_write_index == debugfs->log_buf_size)
				debugfs->log_write_index = 0;
		}
		debugfs->log_num_bytes_buffered += size;
	}

	/* mutex log unlock */
	mutex_unlock(&debugfs->log_lock);
}
