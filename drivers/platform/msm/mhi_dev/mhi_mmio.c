/* Copyright (c) 2015, 2017, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/platform_device.h>

#include "mhi.h"
#include "mhi_hwio.h"

int mhi_dev_mmio_read(struct mhi_dev *dev, uint32_t offset,
			uint32_t *reg_value)
{
	void __iomem *addr;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	addr = dev->mmio_base_addr + offset;

	*reg_value = readl_relaxed(addr);

	pr_debug("reg read:0x%x with value 0x%x\n", offset, *reg_value);

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_read);

int mhi_dev_mmio_write(struct mhi_dev *dev, uint32_t offset,
				uint32_t val)
{
	void __iomem *addr;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	addr = dev->mmio_base_addr + offset;

	writel_relaxed(val, addr);

	pr_debug("reg write:0x%x with value 0x%x\n", offset, val);

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_write);

int mhi_dev_mmio_masked_write(struct mhi_dev *dev, uint32_t offset,
						uint32_t mask, uint32_t shift,
						uint32_t val)
{
	uint32_t reg_val;
	int rc = 0;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	rc = mhi_dev_mmio_read(dev, offset, &reg_val);
	if (rc) {
		pr_err("Read error failed for offset:0x%x\n", offset);
		return rc;
	}

	reg_val &= ~mask;
	reg_val |= ((val << shift) & mask);

	rc = mhi_dev_mmio_write(dev, offset, reg_val);
	if (rc) {
		pr_err("Write error failed for offset:0x%x\n", offset);
		return rc;
	}

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_masked_write);

int mhi_dev_mmio_masked_read(struct mhi_dev *dev, uint32_t offset,
						uint32_t mask, uint32_t shift,
						uint32_t *reg_val)
{
	int rc = 0;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	rc = mhi_dev_mmio_read(dev, offset, reg_val);
	if (rc) {
		pr_err("Read error failed for offset:0x%x\n", offset);
		return rc;
	}

	*reg_val &= mask;
	*reg_val >>= shift;

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_masked_read);

static int mhi_dev_mmio_mask_set_chdb_int_a7(struct mhi_dev *dev,
						uint32_t chdb_id, bool enable)
{
	uint32_t chid_mask, chid_idx, chid_shft, val = 0;
	int rc = 0;

	chid_shft = chdb_id%32;
	chid_mask = (1 << chid_shft);
	chid_idx = chdb_id/32;

	if (chid_idx >= MHI_MASK_ROWS_CH_EV_DB) {
		pr_err("Invalid channel id:%d\n", chid_idx);
		return -EINVAL;
	}

	if (enable)
		val = 1;

	rc = mhi_dev_mmio_masked_write(dev, MHI_CHDB_INT_MASK_A7_n(chid_idx),
					chid_mask, chid_shft, val);
	if (rc) {
		pr_err("Write on channel db interrupt failed\n");
		return rc;
	}

	rc = mhi_dev_mmio_read(dev, MHI_CHDB_INT_MASK_A7_n(chid_idx),
						&dev->chdb[chid_idx].mask);
	if (rc) {
		pr_err("Read channel db INT on row:%d failed\n", chid_idx);
		return rc;
	}

	return rc;
}

int mhi_dev_mmio_enable_chdb_a7(struct mhi_dev *dev, uint32_t chdb_id)
{
	int rc = 0;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	rc = mhi_dev_mmio_mask_set_chdb_int_a7(dev, chdb_id, true);
	if (rc) {
		pr_err("Setting channel DB failed for ch_id:%d\n", chdb_id);
		return rc;
	}

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_enable_chdb_a7);

int mhi_dev_mmio_disable_chdb_a7(struct mhi_dev *dev, uint32_t chdb_id)
{
	int rc = 0;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	rc = mhi_dev_mmio_mask_set_chdb_int_a7(dev, chdb_id, false);
	if (rc) {
		pr_err("Disabling channel DB failed for ch_id:%d\n", chdb_id);
		return rc;
	}

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_disable_chdb_a7);

static int mhi_dev_mmio_set_erdb_int_a7(struct mhi_dev *dev,
					uint32_t erdb_ch_id, bool enable)
{
	uint32_t erdb_id_shft, erdb_id_mask, erdb_id_idx, val = 0;
	int rc = 0;

	erdb_id_shft = erdb_ch_id%32;
	erdb_id_mask = (1 << erdb_id_shft);
	erdb_id_idx = erdb_ch_id/32;

	if (enable)
		val = 1;

	rc = mhi_dev_mmio_masked_write(dev,
			MHI_ERDB_INT_MASK_A7_n(erdb_id_idx),
			erdb_id_mask, erdb_id_shft, val);
	if (rc) {
		pr_err("Error setting event ring db for %d\n", erdb_ch_id);
		return rc;
	}

	return rc;
}

int mhi_dev_mmio_enable_erdb_a7(struct mhi_dev *dev, uint32_t erdb_id)
{
	int rc = 0;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	rc = mhi_dev_mmio_set_erdb_int_a7(dev, erdb_id, true);
	if (rc) {
		pr_err("Error setting event ring db for %d\n", erdb_id);
		return rc;
	}

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_enable_erdb_a7);

int mhi_dev_mmio_disable_erdb_a7(struct mhi_dev *dev, uint32_t erdb_id)
{
	int rc = 0;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	rc = mhi_dev_mmio_set_erdb_int_a7(dev, erdb_id, false);
	if (rc) {
		pr_err("Error disabling event ring db for %d\n", erdb_id);
		return rc;
	}

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_disable_erdb_a7);

int mhi_dev_mmio_get_mhi_state(struct mhi_dev *dev, enum mhi_dev_state *state)
{
	uint32_t reg_value = 0;
	int rc = 0;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	rc = mhi_dev_mmio_masked_read(dev, MHICTRL,
		MHISTATUS_MHISTATE_MASK, MHISTATUS_MHISTATE_SHIFT, state);
	if (rc)
		return rc;

	rc = mhi_dev_mmio_read(dev, MHICTRL, &reg_value);
	if (rc)
		return rc;

	pr_debug("MHICTRL is 0x%x\n", reg_value);

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_get_mhi_state);

static int mhi_dev_mmio_set_chdb_interrupts(struct mhi_dev *dev, bool enable)
{
	uint32_t mask = 0, i = 0;
	int rc = 0;

	if (enable)
		mask = MHI_CHDB_INT_MASK_A7_n_MASK_MASK;

	for (i = 0; i < MHI_MASK_ROWS_CH_EV_DB; i++) {
		rc = mhi_dev_mmio_write(dev,
				MHI_CHDB_INT_MASK_A7_n(i), mask);
		if (rc) {
			pr_err("Set channel db on row:%d failed\n", i);
			return rc;
		}
		dev->chdb[i].mask = mask;
	}

	return rc;
}

int mhi_dev_mmio_enable_chdb_interrupts(struct mhi_dev *dev)
{
	int rc = 0;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	rc = mhi_dev_mmio_set_chdb_interrupts(dev, true);
	if (rc) {
		pr_err("Error setting channel db interrupts\n");
		return rc;
	}

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_enable_chdb_interrupts);

int mhi_dev_mmio_mask_chdb_interrupts(struct mhi_dev *dev)
{
	int rc = 0;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	rc = mhi_dev_mmio_set_chdb_interrupts(dev, false);
	if (rc) {
		pr_err("Error masking channel db interrupts\n");
		return rc;
	}

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_mask_chdb_interrupts);

int mhi_dev_mmio_read_chdb_status_interrupts(struct mhi_dev *dev)
{
	uint32_t i;
	int rc = 0;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	for (i = 0; i < MHI_MASK_ROWS_CH_EV_DB; i++) {
		rc = mhi_dev_mmio_read(dev,
			MHI_CHDB_INT_STATUS_A7_n(i), &dev->chdb[i].status);
		if (rc) {
			pr_err("Error reading chdb status for row:%d\n", i);
			return rc;
		}
	}

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_read_chdb_status_interrupts);

static int mhi_dev_mmio_set_erdb_interrupts(struct mhi_dev *dev, bool enable)
{
	uint32_t mask = 0, i;
	int rc = 0;

	if (enable)
		mask = MHI_ERDB_INT_MASK_A7_n_MASK_MASK;

	for (i = 0; i < MHI_MASK_ROWS_CH_EV_DB; i++) {
		rc = mhi_dev_mmio_write(dev,
				MHI_ERDB_INT_MASK_A7_n(i), mask);
		if (rc) {
			pr_err("Error setting erdb status for row:%d\n", i);
			return rc;
		}
	}

	return 0;
}

int mhi_dev_mmio_enable_erdb_interrupts(struct mhi_dev *dev)
{
	int rc = 0;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	rc = mhi_dev_mmio_set_erdb_interrupts(dev, true);
	if (rc) {
		pr_err("Error enabling all erdb interrupts\n");
		return rc;
	}

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_enable_erdb_interrupts);

int mhi_dev_mmio_mask_erdb_interrupts(struct mhi_dev *dev)
{
	int rc = 0;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	rc = mhi_dev_mmio_set_erdb_interrupts(dev, false);
	if (rc) {
		pr_err("Error masking all event db interrupt\n");
		return rc;
	}

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_mask_erdb_interrupts);

int mhi_dev_mmio_read_erdb_status_interrupts(struct mhi_dev *dev)
{
	uint32_t i;
	int rc = 0;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	for (i = 0; i < MHI_MASK_ROWS_CH_EV_DB; i++) {
		rc = mhi_dev_mmio_read(dev, MHI_ERDB_INT_STATUS_A7_n(i),
						&dev->evdb[i].status);
		if (rc) {
			pr_err("Error setting erdb status for row:%d\n", i);
			return rc;
		}
	}

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_read_erdb_status_interrupts);

int mhi_dev_mmio_enable_ctrl_interrupt(struct mhi_dev *dev)
{
	int rc = 0;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	rc = mhi_dev_mmio_masked_write(dev, MHI_CTRL_INT_MASK_A7,
			MHI_CTRL_MHICTRL_MASK, MHI_CTRL_MHICTRL_SHFT, 1);
	if (rc) {
		pr_err("Error enabling control interrupt\n");
		return rc;
	}

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_enable_ctrl_interrupt);

int mhi_dev_mmio_disable_ctrl_interrupt(struct mhi_dev *dev)
{
	int rc = 0;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	rc = mhi_dev_mmio_masked_write(dev, MHI_CTRL_INT_MASK_A7,
			MHI_CTRL_MHICTRL_MASK, MHI_CTRL_MHICTRL_SHFT, 0);
	if (rc) {
		pr_err("Error disabling control interrupt\n");
		return rc;
	}

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_disable_ctrl_interrupt);

int mhi_dev_mmio_read_ctrl_status_interrupt(struct mhi_dev *dev)
{
	int rc = 0;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	rc = mhi_dev_mmio_read(dev, MHI_CTRL_INT_STATUS_A7, &dev->ctrl_int);
	if (rc) {
		pr_err("Error reading control status interrupt\n");
		return rc;
	}

	dev->ctrl_int &= 0x1;

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_read_ctrl_status_interrupt);

int mhi_dev_mmio_read_cmdb_status_interrupt(struct mhi_dev *dev)
{
	int rc = 0;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	rc = mhi_dev_mmio_read(dev, MHI_CTRL_INT_STATUS_A7, &dev->cmd_int);
	if (rc) {
		pr_err("Error reading cmd status register\n");
		return rc;
	}

	dev->cmd_int &= 0x10;

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_read_cmdb_status_interrupt);

int mhi_dev_mmio_enable_cmdb_interrupt(struct mhi_dev *dev)
{
	int rc = 0;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	rc = mhi_dev_mmio_masked_write(dev, MHI_CTRL_INT_MASK_A7,
			MHI_CTRL_CRDB_MASK, MHI_CTRL_CRDB_SHFT, 1);
	if (rc)
		return rc;

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_enable_cmdb_interrupt);

int mhi_dev_mmio_disable_cmdb_interrupt(struct mhi_dev *dev)
{
	int rc = 0;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	rc = mhi_dev_mmio_masked_write(dev, MHI_CTRL_INT_MASK_A7,
			MHI_CTRL_CRDB_MASK, MHI_CTRL_CRDB_SHFT, 0);
	if (rc)
		return rc;

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_disable_cmdb_interrupt);

static void mhi_dev_mmio_mask_interrupts(struct mhi_dev *dev)
{
	int rc = 0;

	rc = mhi_dev_mmio_disable_ctrl_interrupt(dev);
	if (rc) {
		pr_err("Error disabling control interrupt\n");
		return;
	}

	rc = mhi_dev_mmio_disable_cmdb_interrupt(dev);
	if (rc) {
		pr_err("Error disabling command db interrupt\n");
		return;
	}

	rc = mhi_dev_mmio_mask_chdb_interrupts(dev);
	if (rc) {
		pr_err("Error masking all channel db interrupts\n");
		return;
	}

	rc = mhi_dev_mmio_mask_erdb_interrupts(dev);
	if (rc) {
		pr_err("Error masking all erdb interrupts\n");
		return;
	}
}

int mhi_dev_mmio_clear_interrupts(struct mhi_dev *dev)
{
	uint32_t i = 0;
	int rc = 0;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	for (i = 0; i < MHI_MASK_ROWS_CH_EV_DB; i++) {
		rc = mhi_dev_mmio_write(dev, MHI_CHDB_INT_CLEAR_A7_n(i),
				MHI_CHDB_INT_CLEAR_A7_n_CLEAR_MASK);
		if (rc)
			return rc;
	}

	for (i = 0; i < MHI_MASK_ROWS_CH_EV_DB; i++) {
		rc = mhi_dev_mmio_write(dev, MHI_ERDB_INT_CLEAR_A7_n(i),
				MHI_ERDB_INT_CLEAR_A7_n_CLEAR_MASK);
		if (rc)
			return rc;
	}

	rc = mhi_dev_mmio_write(dev, MHI_CTRL_INT_CLEAR_A7,
					MHI_CTRL_INT_CRDB_CLEAR);
	if (rc)
		return rc;

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_clear_interrupts);

int mhi_dev_mmio_get_chc_base(struct mhi_dev *dev)
{
	uint32_t ccabap_value = 0, offset = 0;
	int rc = 0;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	rc = mhi_dev_mmio_read(dev, CCABAP_HIGHER, &ccabap_value);
	if (rc)
		return rc;

	dev->ch_ctx_shadow.host_pa = ccabap_value;
	dev->ch_ctx_shadow.host_pa <<= 32;

	rc = mhi_dev_mmio_read(dev, CCABAP_LOWER, &ccabap_value);
	if (rc)
		return rc;

	dev->ch_ctx_shadow.host_pa |= ccabap_value;

	offset = (uint32_t)(dev->ch_ctx_shadow.host_pa -
					dev->ctrl_base.host_pa);

	dev->ch_ctx_shadow.device_pa = dev->ctrl_base.device_pa + offset;
	dev->ch_ctx_shadow.device_va = dev->ctrl_base.device_va + offset;

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_get_chc_base);

int mhi_dev_mmio_get_erc_base(struct mhi_dev *dev)
{
	uint32_t ecabap_value = 0, offset = 0;
	int rc = 0;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	rc = mhi_dev_mmio_read(dev, ECABAP_HIGHER, &ecabap_value);
	if (rc)
		return rc;

	dev->ev_ctx_shadow.host_pa = ecabap_value;
	dev->ev_ctx_shadow.host_pa <<= 32;

	rc = mhi_dev_mmio_read(dev, ECABAP_LOWER, &ecabap_value);
	if (rc)
		return rc;

	dev->ev_ctx_shadow.host_pa |= ecabap_value;

	offset = (uint32_t)(dev->ev_ctx_shadow.host_pa -
					dev->ctrl_base.host_pa);

	dev->ev_ctx_shadow.device_pa = dev->ctrl_base.device_pa + offset;
	dev->ev_ctx_shadow.device_va = dev->ctrl_base.device_va + offset;

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_get_erc_base);

int mhi_dev_mmio_get_crc_base(struct mhi_dev *dev)
{
	uint32_t crcbap_value = 0, offset = 0;
	int rc = 0;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	rc = mhi_dev_mmio_read(dev, CRCBAP_HIGHER, &crcbap_value);
	if (rc)
		return rc;

	dev->cmd_ctx_shadow.host_pa = crcbap_value;
	dev->cmd_ctx_shadow.host_pa <<= 32;

	rc = mhi_dev_mmio_read(dev, CRCBAP_LOWER, &crcbap_value);
	if (rc)
		return rc;

	dev->cmd_ctx_shadow.host_pa |= crcbap_value;

	offset = (uint32_t)(dev->cmd_ctx_shadow.host_pa -
					dev->ctrl_base.host_pa);

	dev->cmd_ctx_shadow.device_pa = dev->ctrl_base.device_pa + offset;
	dev->cmd_ctx_shadow.device_va = dev->ctrl_base.device_va + offset;

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_get_crc_base);

int mhi_dev_mmio_get_ch_db(struct mhi_dev_ring *ring, uint64_t *wr_offset)
{
	uint32_t value = 0, ch_start_idx = 0;
	int rc = 0;

	if (!ring) {
		pr_err("Invalid ring context\n");
		return -EINVAL;
	}

	ch_start_idx = ring->mhi_dev->ch_ring_start;

	rc = mhi_dev_mmio_read(ring->mhi_dev,
			CHDB_HIGHER_n(ring->id-ch_start_idx), &value);
	if (rc)
		return rc;

	*wr_offset = value;
	*wr_offset <<= 32;

	rc = mhi_dev_mmio_read(ring->mhi_dev,
			CHDB_LOWER_n(ring->id-ch_start_idx), &value);
	if (rc)
		return rc;

	*wr_offset |= value;

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_get_ch_db);

int mhi_dev_mmio_get_erc_db(struct mhi_dev_ring *ring, uint64_t *wr_offset)
{
	uint32_t value = 0, ev_idx_start = 0;
	int rc = 0;

	if (!ring) {
		pr_err("Invalid ring context\n");
		return -EINVAL;
	}

	ev_idx_start = ring->mhi_dev->ev_ring_start;
	rc = mhi_dev_mmio_read(ring->mhi_dev,
			ERDB_HIGHER_n(ring->id - ev_idx_start), &value);
	if (rc)
		return rc;

	*wr_offset = value;
	*wr_offset <<= 32;

	rc = mhi_dev_mmio_read(ring->mhi_dev,
			ERDB_LOWER_n(ring->id - ev_idx_start), &value);
	if (rc)
		return rc;

	*wr_offset |= value;

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_get_erc_db);

int mhi_dev_mmio_get_cmd_db(struct mhi_dev_ring *ring, uint64_t *wr_offset)
{
	uint32_t value = 0;
	int rc = 0;

	if (!ring) {
		pr_err("Invalid ring context\n");
		return -EINVAL;
	}

	rc = mhi_dev_mmio_read(ring->mhi_dev, CRDB_HIGHER, &value);
	if (rc)
		return rc;

	*wr_offset = value;
	*wr_offset <<= 32;

	rc = mhi_dev_mmio_read(ring->mhi_dev, CRDB_LOWER, &value);
	if (rc)
		return rc;

	*wr_offset |= value;

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_get_cmd_db);

int mhi_dev_mmio_set_env(struct mhi_dev *dev, uint32_t value)
{
	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	mhi_dev_mmio_write(dev, BHI_EXECENV, value);

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_set_env);

int mhi_dev_mmio_reset(struct mhi_dev *dev)
{

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	mhi_dev_mmio_write(dev, MHICTRL, 0);
	mhi_dev_mmio_write(dev, MHISTATUS, 0);
	mhi_dev_mmio_clear_interrupts(dev);

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_reset);

int mhi_dev_restore_mmio(struct mhi_dev *dev)
{
	uint32_t i, reg_cntl_value;
	void *reg_cntl_addr;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	mhi_dev_mmio_mask_interrupts(dev);

	for (i = 0; i < (MHI_DEV_MMIO_RANGE/4); i++) {
		reg_cntl_addr = dev->mmio_base_addr + (i * 4);
		reg_cntl_value = dev->mmio_backup[i];
		writel_relaxed(reg_cntl_value, reg_cntl_addr);
	}

	mhi_dev_mmio_clear_interrupts(dev);
	mhi_dev_mmio_enable_ctrl_interrupt(dev);

	/*Enable chdb interrupt*/
	mhi_dev_mmio_enable_chdb_interrupts(dev);

	/* Mask and enable control interrupt */
	mb();

	return 0;
}
EXPORT_SYMBOL(mhi_dev_restore_mmio);

int mhi_dev_backup_mmio(struct mhi_dev *dev)
{
	uint32_t i = 0;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	for (i = 0; i < MHI_DEV_MMIO_RANGE/4; i++)
		dev->mmio_backup[i] =
				readl_relaxed(dev->mmio_base_addr + (i * 4));

	return 0;
}
EXPORT_SYMBOL(mhi_dev_backup_mmio);

int mhi_dev_get_mhi_addr(struct mhi_dev *dev)
{
	uint32_t data_value = 0;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	mhi_dev_mmio_read(dev, MHICTRLBASE_LOWER, &data_value);
	dev->host_addr.ctrl_base_lsb = data_value;

	mhi_dev_mmio_read(dev, MHICTRLBASE_HIGHER, &data_value);
	dev->host_addr.ctrl_base_msb = data_value;

	mhi_dev_mmio_read(dev, MHICTRLLIMIT_LOWER, &data_value);
	dev->host_addr.ctrl_limit_lsb = data_value;

	mhi_dev_mmio_read(dev, MHICTRLLIMIT_HIGHER, &data_value);
	dev->host_addr.ctrl_limit_msb = data_value;

	mhi_dev_mmio_read(dev, MHIDATABASE_LOWER, &data_value);
	dev->host_addr.data_base_lsb = data_value;

	mhi_dev_mmio_read(dev, MHIDATABASE_HIGHER, &data_value);
	dev->host_addr.data_base_msb = data_value;

	mhi_dev_mmio_read(dev, MHIDATALIMIT_LOWER, &data_value);
	dev->host_addr.data_limit_lsb = data_value;

	mhi_dev_mmio_read(dev, MHIDATALIMIT_HIGHER, &data_value);
	dev->host_addr.data_limit_msb = data_value;

	return 0;
}
EXPORT_SYMBOL(mhi_dev_get_mhi_addr);

int mhi_dev_mmio_init(struct mhi_dev *dev)
{
	int rc = 0;

	if (!dev) {
		pr_err("Invalid MHI dev context\n");
		return -EINVAL;
	}

	rc = mhi_dev_mmio_read(dev, MHIREGLEN, &dev->cfg.mhi_reg_len);
	if (rc)
		return rc;

	rc = mhi_dev_mmio_masked_read(dev, MHICFG, MHICFG_NER_MASK,
				MHICFG_NER_SHIFT, &dev->cfg.event_rings);
	if (rc)
		return rc;

	rc = mhi_dev_mmio_read(dev, CHDBOFF, &dev->cfg.chdb_offset);
	if (rc)
		return rc;

	rc = mhi_dev_mmio_read(dev, ERDBOFF, &dev->cfg.erdb_offset);
	if (rc)
		return rc;

	dev->cfg.channels = NUM_CHANNELS;

	if (!dev->mmio_initialized) {
		rc = mhi_dev_mmio_reset(dev);
		if (rc) {
			pr_err("Error resetting MMIO\n");
			return rc;
		}
	}

	return 0;
}
EXPORT_SYMBOL(mhi_dev_mmio_init);

int mhi_dev_update_ner(struct mhi_dev *dev)
{
	int rc = 0;

	rc = mhi_dev_mmio_masked_read(dev, MHICFG, MHICFG_NER_MASK,
				  MHICFG_NER_SHIFT, &dev->cfg.event_rings);
	if (rc) {
		pr_err("Error update NER\n");
		return rc;
	}

	pr_debug("NER in HW :%d\n", dev->cfg.event_rings);
	return 0;
}
EXPORT_SYMBOL(mhi_dev_update_ner);

int mhi_dev_dump_mmio(struct mhi_dev *dev)
{
	uint32_t r1, r2, r3, r4, i, offset = 0;
	int rc = 0;

	for (i = 0; i < MHI_DEV_MMIO_RANGE/4; i += 4) {
		rc = mhi_dev_mmio_read(dev, offset, &r1);
		if (rc)
			return rc;

		rc = mhi_dev_mmio_read(dev, offset+4, &r2);
		if (rc)
			return rc;

		rc = mhi_dev_mmio_read(dev, offset+8, &r3);
		if (rc)
			return rc;

		rc = mhi_dev_mmio_read(dev, offset+0xC, &r4);
		if (rc)
			return rc;

		offset += 0x10;
		pr_debug("0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
				offset, r1, r2, r3, r4);
	}

	return rc;
}
EXPORT_SYMBOL(mhi_dev_dump_mmio);
