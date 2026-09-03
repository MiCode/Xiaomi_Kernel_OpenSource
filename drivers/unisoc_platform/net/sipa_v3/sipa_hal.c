// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/sipa.h>
#include <linux/slab.h>

#include "sipa_hal.h"
#include "sipa_priv.h"

static int alloc_tx_fifo_ram(struct device *dev,
			     enum sipa_cmn_fifo_index index)
{
	dma_addr_t dma_addr = 0;
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);
	struct sipa_cmn_fifo_cfg_tag *fifo_cfg = &ipa->cmn_fifo_cfg[index];
	ssize_t size, node_size = sizeof(struct sipa_node_desc_tag);

	if (!fifo_cfg->tx_fifo.depth || fifo_cfg->tx_fifo.virt_addr)
		return 0;

	size = fifo_cfg->tx_fifo.depth * node_size;
	if (fifo_cfg->is_pam) {
		fifo_cfg->tx_fifo.virt_addr = dma_alloc_coherent(dev, size,
								 &dma_addr,
								 GFP_KERNEL);
		if (!fifo_cfg->tx_fifo.virt_addr)
			return -ENOMEM;
	} else {
		fifo_cfg->tx_fifo.virt_addr =
			(void *)__get_free_pages(GFP_KERNEL, get_order(size));
		if (!fifo_cfg->tx_fifo.virt_addr)
			return -ENOMEM;
		memset(fifo_cfg->tx_fifo.virt_addr, 0, size);
		dma_addr = dma_map_single(dev, fifo_cfg->tx_fifo.virt_addr,
					  size, DMA_FROM_DEVICE);
		if (dma_mapping_error(dev, dma_addr)) {
			free_pages((unsigned long)fifo_cfg->tx_fifo.virt_addr,
				   get_order(size));
			fifo_cfg->tx_fifo.virt_addr = NULL;
			return -ENOMEM;
		}
	}

	fifo_cfg->tx_fifo.fifo_base_addr_l = lower_32_bits(dma_addr);
	fifo_cfg->tx_fifo.fifo_base_addr_h = upper_32_bits(dma_addr);

	return 0;
}

static int alloc_rx_fifo_ram(struct device *dev,
			     enum sipa_cmn_fifo_index index)
{
	dma_addr_t dma_addr = 0;
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);
	ssize_t size, node_size = sizeof(struct sipa_node_desc_tag);
	struct sipa_cmn_fifo_cfg_tag *fifo_cfg = &ipa->cmn_fifo_cfg[index];

	if (!fifo_cfg->rx_fifo.depth || fifo_cfg->rx_fifo.virt_addr)
		return 0;

	size = fifo_cfg->rx_fifo.depth * node_size;
	if (fifo_cfg->is_pam) {
		fifo_cfg->rx_fifo.virt_addr = dma_alloc_coherent(dev, size,
								 &dma_addr,
								 GFP_KERNEL);
		if (!fifo_cfg->rx_fifo.virt_addr)
			return -ENOMEM;
	} else {
		fifo_cfg->rx_fifo.virt_addr =
			(void *)__get_free_pages(GFP_KERNEL, get_order(size));
		if (!fifo_cfg->rx_fifo.virt_addr)
			return -ENOMEM;
		dma_addr = dma_map_single(dev, fifo_cfg->rx_fifo.virt_addr,
					  size, DMA_TO_DEVICE);
		if (dma_mapping_error(dev, dma_addr)) {
			free_pages((unsigned long)fifo_cfg->rx_fifo.virt_addr,
				   get_order(size));
			fifo_cfg->rx_fifo.virt_addr = NULL;
			return -ENOMEM;
		}
	}

	fifo_cfg->rx_fifo.fifo_base_addr_l = lower_32_bits(dma_addr);
	fifo_cfg->rx_fifo.fifo_base_addr_h = upper_32_bits(dma_addr);

	return 0;
}

static void free_tx_fifo_ram(struct device *dev,
			     enum sipa_cmn_fifo_index index)
{
	dma_addr_t dma_addr = 0;
	ssize_t size;
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);
	struct sipa_cmn_fifo_cfg_tag *fifo_cfg = &ipa->cmn_fifo_cfg[index];

	if (!fifo_cfg->tx_fifo.virt_addr)
		return;

	size = fifo_cfg->tx_fifo.depth * sizeof(struct sipa_node_desc_tag);
	dma_addr = IPA_STI_64BIT(fifo_cfg->tx_fifo.fifo_base_addr_l,
				 fifo_cfg->tx_fifo.fifo_base_addr_h);
	if (!fifo_cfg->tx_fifo.in_iram && !fifo_cfg->is_pam &&
	    fifo_cfg->tx_fifo.virt_addr) {
		dma_unmap_single(dev, dma_addr, size, DMA_FROM_DEVICE);
		free_pages((unsigned long)fifo_cfg->tx_fifo.virt_addr,
			   get_order(size));
	} else if (fifo_cfg->is_pam) {
		dma_free_coherent(dev, size, fifo_cfg->tx_fifo.virt_addr,
				  dma_addr);
	}
}

static void free_rx_fifo_ram(struct device *dev,
			     enum sipa_cmn_fifo_index index)
{
	dma_addr_t dma_addr = 0;
	ssize_t size;
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);
	struct sipa_cmn_fifo_cfg_tag *fifo_cfg = &ipa->cmn_fifo_cfg[index];

	if (!fifo_cfg->rx_fifo.virt_addr)
		return;

	size = fifo_cfg->rx_fifo.depth * sizeof(struct sipa_node_desc_tag);
	dma_addr = IPA_STI_64BIT(fifo_cfg->rx_fifo.fifo_base_addr_l,
				 fifo_cfg->rx_fifo.fifo_base_addr_h);
	if (!fifo_cfg->rx_fifo.in_iram && !fifo_cfg->is_pam &&
	    fifo_cfg->rx_fifo.virt_addr) {
		dma_unmap_single(dev, dma_addr, size, DMA_TO_DEVICE);
		free_pages((unsigned long)fifo_cfg->rx_fifo.virt_addr,
			   get_order(size));
	} else if (fifo_cfg->is_pam) {
		dma_free_coherent(dev, size, fifo_cfg->rx_fifo.virt_addr,
				  dma_addr);
	}
}

static int sipa_init_fifo_addr(struct device *dev)
{
	int i, ret;

	for (i = 0; i < SIPA_FIFO_MAX; i++) {
		ret = alloc_rx_fifo_ram(dev, i);
		if (ret)
			return ret;

		ret = alloc_tx_fifo_ram(dev, i);
		if (ret)
			return ret;
	}

	return 0;
}

static int sipa_init_fifo_reg_base(struct device *dev)
{
	int i;
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	for (i = 0; i < SIPA_FIFO_MAX; i++) {
		ipa->cmn_fifo_cfg[i].fifo_reg_base = ipa->glb_virt_base +
			0x300 + (i * SIPA_FIFO_REG_SIZE);
		ipa->cmn_fifo_cfg[i].fifo_phy_addr = ipa->glb_phy +
			0x300 + (i * SIPA_FIFO_REG_SIZE);
	}

	return 0;
}

static void sipa_backup_open_fifo_params(struct device *dev,
					 enum sipa_cmn_fifo_index id,
					 struct sipa_comm_fifo_params *attr,
					 struct sipa_ext_fifo_params *ext_attr,
					 bool force_sw_intr,
					 sipa_hal_notify_cb cb,
					 void *priv)
{
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);
	struct sipa_open_fifo_param *fifo_param = &ipa->fifo_param[id];

	if (attr) {
		if (!fifo_param->attr) {
			fifo_param->attr = kzalloc(sizeof(*attr), GFP_KERNEL);
			if (!fifo_param->attr)
				return;
		}
		memcpy(fifo_param->attr, attr, sizeof(*attr));
	}

	if (ext_attr) {
		if (!fifo_param->ext_attr) {
			fifo_param->ext_attr = kzalloc(sizeof(*ext_attr),
						       GFP_KERNEL);
			if (!fifo_param->ext_attr)
				return;
		}
		memcpy(fifo_param->ext_attr, ext_attr, sizeof(*ext_attr));
	}

	fifo_param->force_sw_intr = force_sw_intr;
	fifo_param->cb = cb;
	fifo_param->priv = priv;
	fifo_param->open_flag = true;
}

static void sipa_remove_fifo_params(struct device *dev,
				    enum sipa_cmn_fifo_index id)
{
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);
	struct sipa_open_fifo_param *fifo_param = &ipa->fifo_param[id];

	kfree(fifo_param->attr);
	kfree(fifo_param->ext_attr);
	fifo_param->attr = NULL;
	fifo_param->ext_attr = NULL;

	fifo_param->open_flag = false;
}

int sipa_hal_init(struct device *dev)
{
	int i, ret;
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);
	struct cpumask *cpu_mask = &ipa->cpu_mask;

	sipa_glb_ops_init(&ipa->glb_ops);
	sipa_fifo_ops_init(&ipa->fifo_ops);

	ret = devm_request_irq(dev, ipa->general_intr,
			       sipa_multi_int_callback_func,
			       0, "sprd,general_sipa", ipa);
	if (ret) {
		dev_err(dev, "request general irq err ret = %d\n", ret);
		return ret;
	}
	enable_irq_wake(ipa->general_intr);

	for (i = 0; i < SIPA_RECV_QUEUES_MAX; i++) {
		ipa->multi_irq_name[i] = devm_kzalloc(dev, SIPA_IRQ_NAME_SIZE,
						      GFP_KERNEL);

		memset(ipa->multi_irq_name[i], 0x30, SIPA_IRQ_NAME_SIZE);
		sprintf(ipa->multi_irq_name[i], "sprd,multi-sipa-%d", i);
		ret = devm_request_irq(dev, ipa->multi_intr[i],
				       sipa_multi_int_callback_func, 0,
				       ipa->multi_irq_name[i], ipa);
		if (ret) {
			dev_err(dev, "request multi irq err = %d i = %d\n",
				ret, i);
			return ret;
		}

		enable_irq_wake(ipa->multi_intr[i]);
		memset(cpu_mask, 0, sizeof(*cpu_mask));
		cpumask_set_cpu(i, cpu_mask);
		irq_set_affinity_hint(ipa->multi_intr[i], cpu_mask);
	}

	ipa->cpu_num = 0;
	ipa->cpu_num_ano = 0;

	if (!ipa->glb_virt_base) {
		dev_err(dev, "remap glb_base fail\n");
		return -ENOMEM;
	}

	ret = sipa_init_fifo_addr(dev);
	if (ret) {
		dev_err(dev, "init fifo addr err ret = %d\n", ret);
		return ret;
	}

	ret = sipa_init_fifo_reg_base(dev);
	if (ret) {
		dev_err(dev, "init fifo reg base err ret = %d\n", ret);
		return ret;
	}

	return 0;
}

int sipa_hal_set_enabled(struct device *dev, bool enable)
{
	int ret = 0, i = 0;
	u32 val = 0;

	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	for (i = 0; i < SIPA_EB_NUM; i++) {
		val = enable ?
			ipa->eb_regs[i].enable_mask :
			(~ipa->eb_regs[i].enable_mask);
		if (ipa->eb_regs[i].enable_rmap) {
			ret = regmap_update_bits(ipa->eb_regs[i].enable_rmap,
						 ipa->eb_regs[i].enable_reg,
						 ipa->eb_regs[i].enable_mask,
						 val);
			if (ret < 0) {
				dev_err(dev, "regmap %d update bits failed\n",
					i);
				return ret;
			}
		}
	}
	return ret;
}
EXPORT_SYMBOL(sipa_hal_set_enabled);

int sipa_hal_open_cmn_fifo(struct device *dev,
			   enum sipa_cmn_fifo_index fifo,
			   struct sipa_comm_fifo_params *pa,
			   struct sipa_ext_fifo_params *ext_attr,
			   bool force_intr, sipa_hal_notify_cb cb,
			   void *priv)
{
	int ret;
	struct sipa_fifo_phy_ops *fifo_ops;
	struct sipa_cmn_fifo_cfg_tag *fifo_cfg;
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);
	ssize_t node_size = sizeof(struct sipa_node_desc_tag);

	if (!ipa) {
		dev_err(dev, "ipa is null\n");
		return -EINVAL;
	}

	fifo_ops = &ipa->fifo_ops;
	fifo_cfg = ipa->cmn_fifo_cfg;

	fifo_cfg[fifo].priv = priv;
	fifo_cfg[fifo].fifo_irq_callback = cb;

	dev_info(dev, "fifo_id = %d is_pam = %d is_recv = %d\n",
		 fifo_cfg[fifo].fifo_id,
		 fifo_cfg[fifo].is_pam,
		 fifo_cfg[fifo].is_recv);

	sipa_backup_open_fifo_params(dev, fifo, pa, ext_attr, force_intr,
				     cb, priv);

	if (fifo_cfg[fifo].tx_fifo.in_iram) {
		if (!kfifo_initialized(&fifo_cfg[fifo].tx_priv_fifo)) {
			ret = kfifo_alloc(&fifo_cfg[fifo].tx_priv_fifo,
					  fifo_cfg[fifo].tx_fifo.depth *
					  node_size, GFP_KERNEL);
			if (ret)
				return -ENOMEM;
		}
	}

	if (fifo_cfg[fifo].rx_fifo.in_iram) {
		if (!kfifo_initialized(&fifo_cfg[fifo].rx_priv_fifo)) {
			ret = kfifo_alloc(&fifo_cfg[fifo].rx_priv_fifo,
					  fifo_cfg[fifo].rx_fifo.depth *
					  node_size, GFP_KERNEL);
			if (ret) {
				kfifo_free(&fifo_cfg[fifo].tx_priv_fifo);
				return -ENOMEM;
			}
		}
	}

	if (ext_attr) {
		fifo_cfg[fifo].rx_fifo.depth = ext_attr->rx_depth;
		fifo_cfg[fifo].rx_fifo.fifo_base_addr_l = ext_attr->rx_fifo_pal;
		fifo_cfg[fifo].rx_fifo.fifo_base_addr_h = ext_attr->rx_fifo_pah;
		fifo_cfg[fifo].rx_fifo.virt_addr = ext_attr->rx_fifo_va;

		fifo_cfg[fifo].tx_fifo.depth = ext_attr->tx_depth;
		fifo_cfg[fifo].tx_fifo.fifo_base_addr_l = ext_attr->tx_fifo_pal;
		fifo_cfg[fifo].tx_fifo.fifo_base_addr_h = ext_attr->tx_fifo_pah;
		fifo_cfg[fifo].tx_fifo.virt_addr = ext_attr->tx_fifo_va;
	}

	if (!ipa->enable_cnt)
		return -EIO;

	ipa->fifo_ops.open(fifo, fifo_cfg, NULL);
	if (fifo_cfg[fifo].is_pam) {
		fifo_ops->set_hw_intr_thres(fifo, fifo_cfg, 1,
					    pa->tx_intr_threshold);
		fifo_ops->set_hw_intr_timeout(fifo, fifo_cfg, 1,
					      pa->tx_intr_delay_us);
	} else {
		if (pa->tx_intr_threshold)
			fifo_ops->set_intr_thres(fifo, fifo_cfg, 1,
						 pa->tx_intr_threshold);
		if (pa->tx_intr_delay_us)
			fifo_ops->set_intr_timeout(fifo, fifo_cfg, 1,
						   pa->tx_intr_delay_us);
	}

	if (fifo_cfg[fifo].is_recv)
		fifo_ops->set_flowctrl_mode(fifo, fifo_cfg,
					    pa->flow_ctrl_cfg,
					    pa->tx_enter_flowctrl_watermark,
					    pa->tx_leave_flowctrl_watermark,
					    pa->rx_enter_flowctrl_watermark,
					    pa->rx_leave_flowctrl_watermark);
	else
		fifo_ops->enable_flowctrl_irq(fifo, fifo_cfg, 1,
					      pa->flow_ctrl_irq_mode);

	if (pa->flowctrl_in_tx_full)
		fifo_ops->set_tx_full_intr(fifo, fifo_cfg, 1);
	else
		fifo_ops->set_tx_full_intr(fifo, fifo_cfg, 0);

	if (pa->errcode_intr)
		fifo_ops->set_intr_errcode(fifo, fifo_cfg, 1);
	else
		fifo_ops->set_intr_errcode(fifo, fifo_cfg, 0);

	if (force_intr)
		fifo_ops->set_node_intr(fifo, fifo_cfg, 1);
	else
		fifo_ops->set_node_intr(fifo, fifo_cfg, 0);

	return 0;
}

int sipa_hal_close_cmn_fifo(struct device *dev,
			    enum sipa_cmn_fifo_index fifo)
{
	struct sipa_cmn_fifo_cfg_tag *fifo_cfg;
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	fifo_cfg = ipa->cmn_fifo_cfg;

	ipa->fifo_ops.close(fifo, fifo_cfg);
	sipa_remove_fifo_params(dev, fifo);

	return 0;
}

int sipa_hal_config_irq_affinity(int channel, int dst_cpu)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	struct cpumask *cpu_mask = &ipa->cpu_mask;

	if (channel >= SIPA_RECV_QUEUES_MAX ||
	    dst_cpu >= num_possible_cpus()) {
		pr_info("%s channel or dstcpu err\n", __func__);
		return -EINVAL;
	}

	memset(cpu_mask, 0, sizeof(*cpu_mask));
	cpumask_set_cpu(dst_cpu, cpu_mask);
	irq_set_affinity_hint(ipa->multi_intr[channel], cpu_mask);

	return 0;
}

static int sipa_hal_resume_cmn_fifo_ptr(struct device *dev,
					enum sipa_cmn_fifo_index id)
{
	u32 depth;
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);
	struct sipa_cmn_fifo_cfg_tag *fifo_cfg = ipa->cmn_fifo_cfg;

	switch (id) {
	case SIPA_FIFO_MAP0_OUT ... SIPA_FIFO_MAP7_OUT:
		depth = fifo_cfg[id].rx_fifo.depth;
		sipa_init_free_fifo(ipa->receiver, depth, id);
		if (ipa->receiver)
			sipa_reinit_recv_array(dev);
		break;
	default:
		break;
	}

	return 0;
}

int sipa_hal_resume_cmn_fifo(struct device *dev)
{
	int i;
	struct sipa_open_fifo_param *iter;
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	for (i = 0; i < SIPA_FIFO_MAX; i++) {
		iter = &ipa->fifo_param[i];

		if (!iter->open_flag)
			continue;

		if (ipa->fifo_ops.get_tx_depth(i, ipa->cmn_fifo_cfg))
			continue;

		ipa->cmn_fifo_cfg[i].state = false;
		sipa_hal_open_cmn_fifo(dev, i, iter->attr,
				       iter->ext_attr, iter->force_sw_intr,
				       iter->cb, iter->priv);
		sipa_hal_resume_cmn_fifo_ptr(dev, i);
	}

	return 0;
}

bool sipa_hal_get_cmn_fifo_open_status(struct device *dev,
				       enum sipa_cmn_fifo_index fifo)
{
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);
	struct sipa_cmn_fifo_cfg_tag *fifo_cfg =
		&ipa->cmn_fifo_cfg[fifo];

	return fifo_cfg->state;
}

/* stop : true : stop recv false : start receive */
int sipa_hal_cmn_fifo_stop_recv(struct device *dev,
				enum sipa_cmn_fifo_index fifo_id,
				bool stop)
{
	u32 ret;
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	ret = ipa->fifo_ops.stop_recv(fifo_id, ipa->cmn_fifo_cfg, stop);

	if (ret)
		return 0;
	else
		return ret;
}

int sipa_hal_sync_node_from_tx_fifo(struct device *dev,
				    enum sipa_cmn_fifo_index fifo_id,
				    int budget)
{
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	return ipa->fifo_ops.sync_node_from_tx_fifo(dev, fifo_id,
						    ipa->cmn_fifo_cfg,
						    budget);
}

int sipa_hal_sync_node_to_rx_fifo(struct device *dev,
				  enum sipa_cmn_fifo_index fifo_id,
				  int budget)
{
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	return ipa->fifo_ops.sync_node_to_rx_fifo(dev, fifo_id,
						  ipa->cmn_fifo_cfg,
						  budget);
}

int sipa_hal_recv_node_from_tx_fifo(struct device *dev,
				    enum sipa_cmn_fifo_index fifo_id,
				    struct sipa_node_desc_tag *node,
				    int budget)
{
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	return ipa->fifo_ops.recv_node_from_tx_fifo(dev, fifo_id,
						    ipa->cmn_fifo_cfg,
						    node, budget);
}

int sipa_hal_add_tx_fifo_rptr(struct device *dev, enum sipa_cmn_fifo_index id,
			      u32 num)
{
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);
	struct sipa_cmn_fifo_cfg_tag *fifo_cfg = ipa->cmn_fifo_cfg;

	return ipa->fifo_ops.add_tx_fifo_rptr(id, fifo_cfg, num);
}

int sipa_hal_get_cmn_fifo_filled_depth(struct device *dev,
				       enum sipa_cmn_fifo_index fifo_id,
				       u32 *rx_filled, u32 *tx_filled)
{
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	return ipa->fifo_ops.get_filled_depth(fifo_id, ipa->cmn_fifo_cfg,
					      rx_filled, tx_filled);
}

void sipa_hal_enable_pcie_dl_dma(struct device *dev, bool eb)
{
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	ipa->glb_ops.dl_pcie_dma_en(ipa->glb_virt_base, eb);
}

/**
 * sipa_hal_reclaim_unuse_node() - reclaim that unfree description.
 * @hdl: &sipa_hal_ctx.
 * @fifo_id: the cmn fifo that need to reclaim.
 *
 * Some node descriptions that sent out may not be free normally,
 * so wo need use soft method to reclaim these node description.
 *
 * Return:
 *	0: reclaim success.
 *	negative value: reclaim fail.
 */
int sipa_hal_reclaim_unuse_node(struct device *dev,
				enum sipa_cmn_fifo_index fifo_id)
{
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	return ipa->fifo_ops.reclaim_cmn_fifo(fifo_id, ipa->cmn_fifo_cfg);
}

int sipa_hal_reclaim_unuse_intr(struct device *dev,
				enum sipa_cmn_fifo_index fifo_id)
{
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	return ipa->fifo_ops.traverse_int_bit(fifo_id,
					      ipa->cmn_fifo_cfg,
					      0);
}

int sipa_hal_put_node_to_rx_fifo(struct device *dev,
				 enum sipa_cmn_fifo_index fifo_id,
				 struct sipa_node_desc_tag *node,
				 int budget)
{
	int ret;
	struct sipa_cmn_fifo_cfg_tag *fifo_cfg;
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	fifo_cfg = ipa->cmn_fifo_cfg;

	ret = ipa->fifo_ops.put_node_to_rx_fifo(dev, fifo_id, fifo_cfg,
						    node, budget);
	if (ret < 0 || ret != budget) {
		dev_err(dev,
			"put node to rx fifo %d fail ret = %d budget = %d\n",
			fifo_id, ret, budget);
		return ret;
	}

	return 0;
}

int sipa_hal_put_node_to_tx_fifo(struct device *dev,
				 enum sipa_cmn_fifo_index fifo_id,
				 struct sipa_node_desc_tag *node,
				 int budget)
{
	int ret;
	struct sipa_cmn_fifo_cfg_tag *fifo_cfg;
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	fifo_cfg = ipa->cmn_fifo_cfg;

	ret = ipa->fifo_ops.put_node_to_tx_fifo(dev, fifo_id, fifo_cfg,
						    node, budget);
	if (ret < 0 || ret != budget) {
		dev_err(dev,
			"put node to tx fifo %d fail ret = %d budget = %d\n",
			fifo_id, ret, budget);
		return ret;
	}

	return 0;
}

struct sipa_node_desc_tag *sipa_hal_get_rx_node_wptr(struct device *dev,
						     enum sipa_cmn_fifo_index d,
						     u32 index)
{
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);
	struct sipa_cmn_fifo_cfg_tag *fifo_cfg = ipa->cmn_fifo_cfg;

	if (index > fifo_cfg[d].rx_fifo.depth) {
		dev_err(dev, "fifo id %d num is too big %d\n", d, index);
		return NULL;
	}

	return ipa->fifo_ops.get_rx_node_wptr(d, fifo_cfg, index);
}
EXPORT_SYMBOL(sipa_hal_get_rx_node_wptr);

struct sipa_node_desc_tag *sipa_hal_get_tx_node_rptr(struct device *dev,
						     enum sipa_cmn_fifo_index d,
						     u32 index)
{
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);
	struct sipa_cmn_fifo_cfg_tag *fifo_cfg = ipa->cmn_fifo_cfg;

	if (index > fifo_cfg[d].tx_fifo.depth) {
		dev_err(dev, "fifo id %d num is too big %d\n", d, index);
		return NULL;
	}

	return ipa->fifo_ops.get_tx_node_rptr(d, fifo_cfg, index);
}

int sipa_hal_add_rx_fifo_wptr(struct device *dev,
			      enum sipa_cmn_fifo_index fifo_id,
			      u32 num)
{
	int ret;
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);
	struct sipa_cmn_fifo_cfg_tag *fifo_cfg = ipa->cmn_fifo_cfg;

	if (num > fifo_cfg[fifo_id].rx_fifo.depth) {
		dev_err(dev, "fifo id %d num is too big %d\n",
			fifo_id, num);
		return -EINVAL;
	}

	/* ensure fifo has node before update wptr*/
	smp_wmb();
	ret = ipa->fifo_ops.add_rx_fifo_wptr(fifo_id, fifo_cfg, num);

	if (ret < 0) {
		dev_err(dev, "put node to rx fifo %d fail %d\n", fifo_id, ret);
		return ret;
	}

	return 0;
}

bool sipa_hal_get_rx_fifo_empty_status(struct device *dev,
				       enum sipa_cmn_fifo_index fifo_id)
{
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	return ipa->fifo_ops.get_rx_empty_status(fifo_id, ipa->cmn_fifo_cfg);
}

bool sipa_hal_get_tx_fifo_empty_status(struct device *dev,
				       enum sipa_cmn_fifo_index fifo_id)
{
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	return ipa->fifo_ops.get_tx_empty_status(fifo_id, ipa->cmn_fifo_cfg);
}

bool sipa_hal_check_rx_priv_fifo_is_empty(struct device *dev,
					  enum sipa_cmn_fifo_index fifo_id)
{
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);
	struct sipa_cmn_fifo_cfg_tag *fifo_cfg = &ipa->cmn_fifo_cfg[fifo_id];

	return kfifo_is_empty(&fifo_cfg->rx_priv_fifo);
}

bool sipa_hal_check_rx_priv_fifo_is_full(struct device *dev,
					 enum sipa_cmn_fifo_index fifo_id)
{
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);
	struct sipa_cmn_fifo_cfg_tag *fifo_cfg = &ipa->cmn_fifo_cfg[fifo_id];

	return kfifo_is_full(&fifo_cfg->rx_priv_fifo);
}

bool sipa_hal_is_rx_fifo_full(struct device *dev,
			      enum sipa_cmn_fifo_index fifo_id)
{
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);
	struct sipa_cmn_fifo_cfg_tag *fifo_cfg = ipa->cmn_fifo_cfg;

	return ipa->fifo_ops.get_rx_full_status(fifo_id, fifo_cfg);
}

bool sipa_hal_bk_fifo_node(struct device *dev,
			   enum sipa_cmn_fifo_index fifo_id)
{
	u32 ret;
	ssize_t node_size = sizeof(struct sipa_node_desc_tag);
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);
	struct sipa_cmn_fifo_cfg_tag *fifo_cfg = &ipa->cmn_fifo_cfg[fifo_id];

	if (!fifo_cfg->state || !fifo_cfg->is_pam)
		return false;

	if (kfifo_initialized(&fifo_cfg->rx_priv_fifo)) {
		kfifo_reset(&fifo_cfg->rx_priv_fifo);
		ret = kfifo_in(&fifo_cfg->rx_priv_fifo,
			       fifo_cfg->rx_fifo.virt_addr,
			       fifo_cfg->rx_fifo.depth * node_size);
		if (ret != fifo_cfg->rx_fifo.depth * node_size)
			dev_err(dev, "backup %d rx fifo failed ret = %d\n",
				fifo_id, ret);
	}

	if (kfifo_initialized(&fifo_cfg->tx_priv_fifo)) {
		kfifo_reset(&fifo_cfg->tx_priv_fifo);
		ret = kfifo_in(&fifo_cfg->tx_priv_fifo,
			       fifo_cfg->tx_fifo.virt_addr,
			       fifo_cfg->tx_fifo.depth * node_size);
		if (ret != fifo_cfg->tx_fifo.depth * node_size)
			dev_err(dev, "backup %d tx fifo failed ret = %d\n",
				fifo_id, ret);
	}

	return true;
}

bool sipa_hal_resume_fifo_node(struct device *dev,
			       enum sipa_cmn_fifo_index fifo_id)
{
	u32 ret;
	ssize_t node_size = sizeof(struct sipa_node_desc_tag);
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);
	struct sipa_cmn_fifo_cfg_tag *fifo_cfg = &ipa->cmn_fifo_cfg[fifo_id];

	if (!fifo_cfg->is_pam)
		return false;

	if (!kfifo_is_full(&fifo_cfg->rx_priv_fifo) ||
	    !kfifo_is_full(&fifo_cfg->tx_priv_fifo))
		return false;

	if (!ipa->fifo_ops.get_tx_depth(fifo_id, ipa->cmn_fifo_cfg))
		return false;

	if (kfifo_initialized(&fifo_cfg->rx_priv_fifo)) {
		ret = kfifo_out(&fifo_cfg->rx_priv_fifo,
				fifo_cfg->rx_fifo.virt_addr,
				fifo_cfg->rx_fifo.depth * node_size);
		if (ret != fifo_cfg->rx_fifo.depth * node_size)
			dev_err(dev, "resume %d rx fifo node failed\n",
				fifo_id);
	}

	if (kfifo_initialized(&fifo_cfg->tx_priv_fifo)) {
		ret = kfifo_out(&fifo_cfg->tx_priv_fifo,
				fifo_cfg->tx_fifo.virt_addr,
				fifo_cfg->tx_fifo.depth * node_size);
		if (ret != fifo_cfg->tx_fifo.depth * node_size)
			dev_err(dev, "resume %d tx fifo node failed\n",
				fifo_id);
	}

	return true;
}

int sipa_hal_cmn_fifo_get_filled_depth(struct device *dev,
				       enum sipa_cmn_fifo_index fifo_id,
				       u32 *rx, u32 *tx)
{
	struct sipa_cmn_fifo_cfg_tag *fifo_cfg;
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	fifo_cfg = ipa->cmn_fifo_cfg;
	if (!ipa->enable_cnt)
		return true;

	return ipa->fifo_ops.get_filled_depth(fifo_id, fifo_cfg, rx, tx);
}

/**
 * sipa_hal_check_send_cmn_fifo_com() - Check cmn fifo send and free whether
 * completion
 * @hdl: &sipa_hal_ctx
 * @fifo_id: The cmn fifo id that need to be check.
 *
 * If fifo is not CP_DL, its tx/rx fifo's wptr/rptr should be equal,
 * if fifo is CP_DL, its wptr of tx fifo should be greated than rx fifo
 * wptr and rptr total depth.
 *
 * Return:
 *	true: send and free complete.
 *	false: send or free not complete.
 */
bool sipa_hal_check_send_cmn_fifo_com(struct device *dev,
				      enum sipa_cmn_fifo_index fifo_id)
{
	bool status;
	u32 ret, tx_rptr, tx_wptr, rx_rptr, rx_wptr, depth;
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);
	struct sipa_cmn_fifo_cfg_tag *fifo_cfg = ipa->cmn_fifo_cfg;

	ret = ipa->fifo_ops.get_tx_empty_status(fifo_id, fifo_cfg);
	ipa->fifo_ops.get_tx_ptr(fifo_id, fifo_cfg, &tx_wptr, &tx_rptr);
	ipa->fifo_ops.get_rx_ptr(fifo_id, fifo_cfg, &rx_wptr, &rx_rptr);
	fifo_cfg += fifo_id;
	switch (fifo_id) {
	case SIPA_FIFO_MAP0_OUT ... SIPA_FIFO_MAP7_OUT:
		depth = fifo_cfg->rx_fifo.depth;
		status = (tx_wptr == tx_rptr) &&
			(((depth | (depth - 1)) & (tx_wptr + depth)) ==
			 rx_wptr);
		break;
	default:
		status = tx_wptr == tx_rptr && tx_rptr == rx_wptr &&
			rx_wptr == rx_rptr;
		break;
	}

	if (status && ret)
		return true;

	return false;
}

int sipa_hal_free_tx_rx_fifo_buf(struct device *dev)
{
	int i = 0;

	for (i = 0; i < SIPA_FIFO_MAX; i++) {
		free_rx_fifo_ram(dev, i);
		free_tx_fifo_ram(dev, i);
	}

	return 0;
}

int sipa_hal_init_pam_param(enum sipa_cmn_fifo_index dl_idx,
			    enum sipa_cmn_fifo_index ul_idx,
			    struct sipa_to_pam_info *out)
{
	struct sipa_cmn_fifo_cfg_tag *dl_fifo_cfg, *ul_fifo_cfg;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	dl_fifo_cfg = &ipa->cmn_fifo_cfg[dl_idx];
	ul_fifo_cfg = &ipa->cmn_fifo_cfg[ul_idx];

	out->dl_fifo.tx_fifo_base_addr =
		IPA_STI_64BIT(dl_fifo_cfg->tx_fifo.fifo_base_addr_l,
			      dl_fifo_cfg->tx_fifo.fifo_base_addr_h);
	out->dl_fifo.rx_fifo_base_addr =
		IPA_STI_64BIT(dl_fifo_cfg->rx_fifo.fifo_base_addr_l,
			      dl_fifo_cfg->rx_fifo.fifo_base_addr_h);

	out->ul_fifo.tx_fifo_base_addr =
		IPA_STI_64BIT(ul_fifo_cfg->tx_fifo.fifo_base_addr_l,
			      ul_fifo_cfg->tx_fifo.fifo_base_addr_h);
	out->ul_fifo.rx_fifo_base_addr =
		IPA_STI_64BIT(ul_fifo_cfg->rx_fifo.fifo_base_addr_l,
			      ul_fifo_cfg->rx_fifo.fifo_base_addr_h);

	out->dl_fifo.fifo_sts_addr = dl_fifo_cfg->fifo_phy_addr;
	out->ul_fifo.fifo_sts_addr = ul_fifo_cfg->fifo_phy_addr;

	out->dl_fifo.fifo_depth = dl_fifo_cfg->tx_fifo.depth;
	out->ul_fifo.fifo_depth = ul_fifo_cfg->tx_fifo.depth;

	return 0;
}

int sipa_hal_ctrl_ipa_action(u32 enable)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	return ipa->glb_ops.ctrl_ipa_action(ipa->glb_virt_base,
					    enable);
}

bool sipa_hal_get_pause_status(void)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	return ipa->glb_ops.get_pause_status(ipa->glb_virt_base);
}

bool sipa_hal_get_resume_status(void)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	return ipa->glb_ops.get_resume_status(ipa->glb_virt_base);
}

/**
 * sipa_hal_set_hash_sync_req() - set hash sync req.
 */
int sipa_hal_set_hash_sync_req(void)
{
	unsigned long flags;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	if (!ipa)
		return -ENODEV;

	spin_lock_irqsave(&ipa->enable_lock, flags);
	/* no need to sync cache if ipa is powered off,
	 * because cache is flushed when powered off
	 */
	if (!ipa->enable_cnt) {
		spin_unlock_irqrestore(&ipa->enable_lock, flags);
		return -EBUSY;
	}

	ipa->glb_ops.set_cache_sync_req(ipa->glb_virt_base);
	spin_unlock_irqrestore(&ipa->enable_lock, flags);

	return 0;
}
EXPORT_SYMBOL(sipa_hal_set_hash_sync_req);

void sipa_hal_tcp_special_leave_to_ap(void)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	ipa->glb_ops.tcp_special_leave_to_ap(ipa->glb_virt_base);
}

void sipa_hal_resume_glb_reg_cfg(struct device *dev)
{
	unsigned long flags;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	void __iomem *glb_base = ipa->glb_virt_base;

	spin_lock_irqsave(&ipa->mode_lock, flags);
	ipa->glb_ops.set_work_mode(glb_base, ipa->is_bypass);
	spin_unlock_irqrestore(&ipa->mode_lock, flags);

	ipa->glb_ops.dl_pcie_dma_en(glb_base, (u32)ipa->pcie_dl_dma);
	ipa->glb_ops.cp_dl_dst_term_num(glb_base, SIPA_TERM_AP);

	ipa->glb_ops.enable_def_interrupt_src(glb_base);
	ipa->glb_ops.set_def_flow_ctl_to_src_blk(glb_base);
	ipa->glb_ops.set_wifi_ul_map0_int_sel(glb_base, true);
	ipa->glb_ops.set_map_flow_ctl_to_wifi(glb_base, true);
	ipa->glb_ops.map_multi_fifo_mode_en(glb_base, false);
	ipa->multi_mode = false;
	ipa->glb_ops.set_map_fifo_cnt(glb_base, SIPA_RECV_QUEUES_MAX);
	ipa->glb_ops.map_fifo_sel_mode(glb_base, true);
	ipa->glb_ops.out_map_en(glb_base, 0xff);
	ipa->glb_ops.ctrl_def_hash_en(glb_base);
	ipa->glb_ops.ctrl_def_chksum_en(glb_base);
	ipa->glb_ops.input_filter_en(glb_base, true);
	ipa->glb_ops.output_filter_en(glb_base, true);

	sipa_hal_tcp_special_leave_to_ap();
}

irqreturn_t sipa_multi_int_callback_func(int irq, void *cookie)
{
	struct sipa_plat_drv_cfg *ipa = cookie;

	ipa->fifo_ops.traverse_int_bit(SIPA_FIFO_MAP_IN,
				       ipa->cmn_fifo_cfg, irq);

	ipa->fifo_ops.traverse_int_bit(SIPA_FIFO_MAP0_OUT + irq -
				       ipa->multi_intr[0],
				       ipa->cmn_fifo_cfg, irq);

	return IRQ_HANDLED;
}

/**
 * sipa_prepare_modem_power_off() - disable the cp endpoint of ipa.
 */
void sipa_prepare_modem_power_off(void)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	if (ipa->enable_cnt && ipa->power_flag)
		ipa->glb_ops.cp_work_status(ipa->glb_virt_base, false);
}
EXPORT_SYMBOL(sipa_prepare_modem_power_off);

/**
 * sipa_prepare_modem_power_on() - enable the cp endpoint of ipa.
 */
void sipa_prepare_modem_power_on(void)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	if (ipa->enable_cnt && ipa->power_flag)
		ipa->glb_ops.cp_work_status(ipa->glb_virt_base, true);
}
EXPORT_SYMBOL(sipa_prepare_modem_power_on);

/**
 * sipa_swap_hash_table() - configure hash rules.
 * @new_tbl: contains the length and address of the hash table.
 */
int sipa_swap_hash_table(struct sipa_hash_table *new_tbl)
{
	int ret;
	u32 addrl, addrh;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	if (!ipa)
		return -ENODEV;

	if (!ipa->enable_cnt || !ipa->power_flag)
		return -EBUSY;

	ret = sipa_set_enabled(true);
	if (ret) {
		sipa_set_enabled(false);
		return ret;
	}

	ipa->glb_ops.set_cache_sync_req(ipa->glb_virt_base);
	if (new_tbl) {
		addrl = lower_32_bits(new_tbl->tbl_phy_addr);
		addrh = upper_32_bits(new_tbl->tbl_phy_addr);
		ipa->glb_ops.set_htable(ipa->glb_virt_base,
					addrl, addrh, new_tbl->depth);
	}

	return sipa_set_enabled(false);
}
EXPORT_SYMBOL(sipa_swap_hash_table);

/**
 * sipa_config_ifilter() - config input filter prerule
 * @ifilter: input filter
 */
int sipa_config_ifilter(struct sipa_filter_tbl *ifilter)
{
	int ret = 0;
	u32 depth = ifilter->depth;
	int i, j;
	u32 bytes, max_num, tmp;
	void __iomem *glb_base;
	u32 *filter = NULL;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	if (!ipa)
		return -ENODEV;

	if (!ipa->enable_cnt || !ipa->power_flag)
		return -EBUSY;

	ret = sipa_set_enabled(true);
	if (ret)
		goto bail;

	glb_base = ipa->glb_virt_base;

	ipa->glb_ops.ifilter_ctrl(glb_base, true);
	ret = ipa->glb_ops.get_ifilter_locked_status(glb_base, true);
	if (ret)
		goto bail;

	if (ifilter->is_ipv4) {
		ipa->glb_ops.set_ifilter_depth_ipv4(glb_base, depth);
		bytes = 32;
		max_num = 64 * bytes;
		i = bytes;
		while (i <= max_num) {
			for (j = i; j > i - bytes; j -= 4) {
				filter = (u32 *)(ifilter->filter_pre_rule
						 + j - 4);
				tmp = ntohl(*filter);
				ipa->glb_ops.fill_ifilter_ipv4(glb_base, tmp);
			}

			i += bytes;
		}
	} else if (ifilter->is_ipv6) {
		ipa->glb_ops.set_ifilter_depth_ipv6(glb_base, depth);
		bytes = 52;
		max_num = 64 * bytes;
		i = bytes;
		while (i <= max_num) {
			for (j = i; j > i - bytes; j -= 4) {
				filter = (u32 *)(ifilter->filter_pre_rule
						 + j - 4);
				tmp = ntohl(*filter);
				ipa->glb_ops.fill_ifilter_ipv6(glb_base, tmp);
			}

			i += bytes;
		}
	}

	ipa->glb_ops.ifilter_ctrl(glb_base, false);
	ret = ipa->glb_ops.get_ifilter_locked_status(glb_base, false);
	if (ret)
		goto bail;

bail:
	sipa_set_enabled(false);
	return ret;
}
EXPORT_SYMBOL(sipa_config_ifilter);

/**
 * sipa_config_filter() - config output filter prerule
 * @ofilter: output filter
 */
int sipa_config_ofilter(struct sipa_filter_tbl *ofilter)
{
	int ret = 0;
	u32 depth = ofilter->depth;
	int i, j;
	u32 bytes, max_num, tmp;
	void __iomem *glb_base;
	u32 *filter = NULL;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	if (!ipa)
		return -ENODEV;

	if (!ipa->enable_cnt || !ipa->power_flag)
		return -EBUSY;

	ret = sipa_set_enabled(true);
	if (ret)
		goto bail;

	glb_base = ipa->glb_virt_base;

	ipa->glb_ops.ofilter_ctrl(glb_base, true);
	ret = ipa->glb_ops.get_ofilter_locked_status(glb_base, true);
	if (ret)
		goto bail;

	if (ofilter->is_ipv4) {
		ipa->glb_ops.set_ofilter_depth_ipv4(glb_base, depth);
		bytes = 32;
		max_num = 64 * bytes;
		i = bytes;
		while (i <= max_num) {
			for (j = i; j > i - bytes; j -= 4) {
				filter = (u32 *)(ofilter->filter_pre_rule
						 + j - 4);
				tmp = ntohl(*filter);
				ipa->glb_ops.fill_ofilter_ipv4(glb_base, tmp);
			}

			i += bytes;
		}
	} else if (ofilter->is_ipv6) {
		ipa->glb_ops.set_ofilter_depth_ipv6(glb_base, depth);
		bytes = 52;
		max_num = 64 * bytes;
		i = bytes;
		while (i <= max_num) {
			for (j = i; j > i - bytes; j -= 4) {
				filter = (u32 *)(ofilter->filter_pre_rule
						 + j - 4);
				tmp = ntohl(*filter);
				ipa->glb_ops.fill_ofilter_ipv6(glb_base, tmp);
			}
			i += bytes;
		}
	}

	ipa->glb_ops.ofilter_ctrl(glb_base, false);
	ret = ipa->glb_ops.get_ofilter_locked_status(glb_base, false);
	if (ret)
		goto bail;

bail:
	sipa_set_enabled(false);
	return ret;
}
EXPORT_SYMBOL(sipa_config_ofilter);

/**
 * Description: block flow ctrl status to another fifo for map,
 * when map fifo enter flow ctrl,notify other fifo.
 * @status: enable or disable
 * true for enable
 * false for disable
 */
void sipa_hal_set_map_flow_ctl_to_wifi(bool status)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	void __iomem *glb_base = ipa->glb_virt_base;

	ipa->glb_ops.set_map_flow_ctl_to_wifi(glb_base, status);
}
EXPORT_SYMBOL(sipa_hal_set_map_flow_ctl_to_wifi);

/**
 * Description: set ipa map0 fifo interrupt source status;
 * this operation can make others fifo interrupt appear on map0 fifo,
 * but map0 fifo interrupt reg do not record it, we should clear interrupt
 * in corresponding fifo reg.
 * default:map0 out map0
 * @status:enable or disable
 * true for enable
 * false for disable
 */
void sipa_hal_set_wifi_ul_map0_int_sel(bool status)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	void __iomem *glb_base = ipa->glb_virt_base;

	ipa->glb_ops.set_wifi_ul_map0_int_sel(glb_base, status);
}
EXPORT_SYMBOL(sipa_hal_set_wifi_ul_map0_int_sel);

/**
 * Description: if true ipa will continue receive node when enter
 * flow ctrl,but the node will send to free fifo directly;
 * if false ipa will stop receive node,should software
 * recover it to receive node when exit flow ctrl.
 * @id: common fifo id
 * @status: true or false
 * @return 0 set success else set fail
 */
int sipa_hal_cmn_fifo_non_stop_on_flowctl(enum sipa_cmn_fifo_index id,
					  bool status)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	void __iomem *fifo_base = NULL;

	if (id >= SIPA_FIFO_MAX) {
		pr_err("fifo id is invalid!\n");
		return -EINVAL;
	}

	fifo_base = ipa->cmn_fifo_cfg[id].fifo_reg_base;

	ipa->fifo_ops.cmn_fifo_non_stop_on_flowctl(fifo_base, status);

	return 0;
}
EXPORT_SYMBOL(sipa_hal_cmn_fifo_non_stop_on_flowctl);

int sipa_hal_cmn_fifo_flowctl_recover(enum sipa_cmn_fifo_index id)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	void __iomem *fifo_base = NULL;

	if (id >= SIPA_FIFO_MAX) {
		pr_err("fifo id is invalid!\n");
		return -EINVAL;
	}

	fifo_base = ipa->cmn_fifo_cfg[id].fifo_reg_base;
	ipa->fifo_ops.cmn_fifo_flowctl_recover(fifo_base);

	return 0;
}
EXPORT_SYMBOL(sipa_hal_cmn_fifo_flowctl_recover);

int sipa_hal_check_cmn_fifo_flowctl(enum sipa_cmn_fifo_index id)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	void __iomem *fifo_base = NULL;
	int ret;

	if (id >= SIPA_FIFO_MAX) {
		pr_err("fifo id is invalid!\n");
		return -EINVAL;
	}

	fifo_base = ipa->cmn_fifo_cfg[id].fifo_reg_base;
	ret = ipa->fifo_ops.check_cmn_fifo_flowctl(fifo_base);
	if (ret)
		return true;

	return false;
}
EXPORT_SYMBOL(sipa_hal_check_cmn_fifo_flowctl);

int sipa_hal_check_cmn_fifo_enter_flowctl(enum sipa_cmn_fifo_index id)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	void __iomem *fifo_base = NULL;
	int ret;

	if (id >= SIPA_FIFO_MAX) {
		pr_err("fifo id is invalid!\n");
		return -EINVAL;
	}

	fifo_base = ipa->cmn_fifo_cfg[id].fifo_reg_base;
	ret = ipa->fifo_ops.check_cmn_fifo_enter_flowctl(fifo_base);
	if (ret)
		return true;

	return false;
}
EXPORT_SYMBOL(sipa_hal_check_cmn_fifo_enter_flowctl);

int sipa_hal_check_cmn_fifo_exit_flowctl(enum sipa_cmn_fifo_index id)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	void __iomem *fifo_base = NULL;
	int ret;

	if (id >= SIPA_FIFO_MAX) {
		pr_err("fifo id is invalid!\n");
		return -EINVAL;
	}

	fifo_base = ipa->cmn_fifo_cfg[id].fifo_reg_base;
	ret = ipa->fifo_ops.check_cmn_fifo_exit_flowctl(fifo_base);
	if (ret)
		return true;

	return false;
}
EXPORT_SYMBOL(sipa_hal_check_cmn_fifo_exit_flowctl);

int sipa_hal_clr_cmn_fifo_flowctl_interrupt(enum sipa_cmn_fifo_index id)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	void __iomem *fifo_base = NULL;

	if (id >= SIPA_FIFO_MAX) {
		pr_err("fifo id is invalid!\n");
		return -EINVAL;
	}

	fifo_base = ipa->cmn_fifo_cfg[id].fifo_reg_base;
	ipa->fifo_ops.clr_cmn_fifo_flowctl_interrupt(fifo_base);

	return 0;
}
EXPORT_SYMBOL(sipa_hal_clr_cmn_fifo_flowctl_interrupt);

int sipa_hal_clr_cfifo_flowctl_enter_inter(enum sipa_cmn_fifo_index id)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	void __iomem *fifo_base = NULL;

	if (id >= SIPA_FIFO_MAX) {
		pr_err("fifo id is invalid!\n");
		return -EINVAL;
	}

	fifo_base = ipa->cmn_fifo_cfg[id].fifo_reg_base;
	ipa->fifo_ops.clr_cfifo_flowctl_enter_inter(fifo_base);

	return 0;
}
EXPORT_SYMBOL(sipa_hal_clr_cfifo_flowctl_enter_inter);

int sipa_hal_clr_cfifo_flowctl_exit_inter(enum sipa_cmn_fifo_index id)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	void __iomem *fifo_base = NULL;

	if (id >= SIPA_FIFO_MAX) {
		pr_err("fifo id is invalid!\n");
		return -EINVAL;
	}

	fifo_base = ipa->cmn_fifo_cfg[id].fifo_reg_base;
	ipa->fifo_ops.clr_cfifo_flowctl_exit_inter(fifo_base);

	return 0;
}
EXPORT_SYMBOL(sipa_hal_clr_cfifo_flowctl_exit_inter);
