// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

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
static uint32_t npu_reg_read(void __iomem *base, size_t size, uint32_t off)
{
	if (!base) {
		NPU_ERR("NULL base address\n");
		return 0;
	}

	if ((off % 4) != 0) {
		NPU_ERR("offset %x is not aligned\n", off);
		return 0;
	}

	if (off >= size) {
		NPU_ERR("offset exceeds io region %x:%x\n", off, size);
		return 0;
	}

	return readl_relaxed(base + off);
}

static void npu_reg_write(void __iomem *base, size_t size, uint32_t off,
	uint32_t val)
{
	if (!base) {
		NPU_ERR("NULL base address\n");
		return;
	}

	if ((off % 4) != 0) {
		NPU_ERR("offset %x is not aligned\n", off);
		return;
	}

	if (off >= size) {
		NPU_ERR("offset exceeds io region %x:%x\n", off, size);
		return;
	}

	writel_relaxed(val, base + off);
	__iowmb();
}

uint32_t npu_core_reg_read(struct npu_device *npu_dev, uint32_t off)
{
	return npu_reg_read(npu_dev->core_io.base, npu_dev->core_io.size, off);
}

void npu_core_reg_write(struct npu_device *npu_dev, uint32_t off, uint32_t val)
{
	npu_reg_write(npu_dev->core_io.base, npu_dev->core_io.size,
		off, val);
}

uint32_t npu_tcsr_reg_read(struct npu_device *npu_dev, uint32_t off)
{
	return npu_reg_read(npu_dev->tcsr_io.base, npu_dev->tcsr_io.size, off);
}

uint32_t npu_apss_shared_reg_read(struct npu_device *npu_dev, uint32_t off)
{
	return npu_reg_read(npu_dev->apss_shared_io.base,
		npu_dev->apss_shared_io.size, off);
}

void npu_apss_shared_reg_write(struct npu_device *npu_dev, uint32_t off,
	uint32_t val)
{
	npu_reg_write(npu_dev->apss_shared_io.base,
		npu_dev->apss_shared_io.size, off, val);
}

uint32_t npu_cc_reg_read(struct npu_device *npu_dev, uint32_t off)
{
	return npu_reg_read(npu_dev->cc_io.base, npu_dev->cc_io.size, off);
}

void npu_cc_reg_write(struct npu_device *npu_dev, uint32_t off,
	uint32_t val)
{
	npu_reg_write(npu_dev->cc_io.base, npu_dev->cc_io.size,
		off, val);
}

uint32_t npu_qfprom_reg_read(struct npu_device *npu_dev, uint32_t off)
{
	return npu_reg_read(npu_dev->qfprom_io.base,
		npu_dev->qfprom_io.size, off);
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

	if (dst_off >= npu_dev->tcm_io.size ||
		(npu_dev->tcm_io.size - dst_off) < size) {
		NPU_ERR("memory write exceeds io region %x:%x:%x\n",
			dst_off, size, npu_dev->tcm_io.size);
		return;
	}

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

	__iowmb();
}

int32_t npu_mem_read(struct npu_device *npu_dev, void *src, void *dst,
	uint32_t size)
{
	size_t src_off = (size_t)src;
	uint32_t *out32 = (uint32_t *)dst;
	uint8_t *out8 = 0;
	uint32_t i = 0;
	uint32_t num = 0;

	if (src_off >= npu_dev->tcm_io.size ||
		(npu_dev->tcm_io.size - src_off) < size) {
		NPU_ERR("memory read exceeds io region %x:%x:%x\n",
			src_off, size, npu_dev->tcm_io.size);
		return 0;
	}

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
}

int32_t npu_interrupt_raise_m0(struct npu_device *npu_dev)
{
	npu_apss_shared_reg_write(npu_dev, APSS_SHARED_IPC_INTERRUPT_1, 0x40);

	return 0;
}

int32_t npu_interrupt_raise_dsp(struct npu_device *npu_dev)
{
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
		NPU_ERR("ion buf has been mapped\n");
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
		NPU_ERR("fail to alloc npu_ion_buffer\n");
		ret = -ENOMEM;
		return ret;
	}

	smmu_ctx->attach_cnt++;

	ion_buf->dma_buf = dma_buf_get(ion_buf->fd);
	if (IS_ERR_OR_NULL(ion_buf->dma_buf)) {
		NPU_ERR("dma_buf_get failed %d\n", ion_buf->fd);
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
		NPU_ERR("npu dma_buf_map_attachment failed\n");
		ret = -ENOMEM;
		ion_buf->table = NULL;
		goto map_end;
	}

	ion_buf->iova = ion_buf->table->sgl->dma_address;
	ion_buf->size = ion_buf->dma_buf->size;
	*addr = ion_buf->iova;
	NPU_DBG("mapped mem addr:0x%llx size:0x%x\n", ion_buf->iova,
		ion_buf->size);
	NPU_DBG("physical address 0x%llx\n", sg_phys(ion_buf->table->sgl));
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
		NPU_ERR("cant find ion buf\n");
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
		NPU_ERR("could not find buffer\n");
		return;
	}

	if (ion_buf->iova != addr)
		NPU_WARN("unmap address %llu doesn't match %llu\n", addr,
			ion_buf->iova);

	if (ion_buf->table)
		dma_buf_unmap_attachment(ion_buf->attachment, ion_buf->table,
			DMA_BIDIRECTIONAL);
	if (ion_buf->dma_buf && ion_buf->attachment)
		dma_buf_detach(ion_buf->dma_buf, ion_buf->attachment);
	if (ion_buf->dma_buf)
		dma_buf_put(ion_buf->dma_buf);
	npu_dev->smmu_ctx.attach_cnt--;

	NPU_DBG("unmapped mem addr:0x%llx size:0x%x\n", ion_buf->iova,
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
