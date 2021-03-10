// SPDX-License-Identifier: GPL-2.0-only
/* Atlantic Network Driver
 *
 * Copyright (C) 2018 aQuantia Corporation
 * Copyright (C) 2019-2020 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/etherdevice.h>
#include "atl_common.h"
#include "atl_desc.h"
#include "atl_fwd.h"
#include "atl_ring.h"

static u32 atl_skip_reglist[] = {
	/* PCIE */
	0x1030, 0x1200, 0x1514, 0x1F00, 0x1F04, 0x1F20, 0x1F24,

	/* INTR Registers */
	0x21A0, 0x21A4, 0x21A8,

	/* COM Registers */
	0x30A0, 0x30A8, 0x3900, 0x3920,

	/* MAC PHY Registers */
	0x4058, 0x4060, 0x4064, 0x40B0, 0x40B4, 0x40B8, 0x40BC,
	0x4120, 0x4140, 0x4144, 0x4190, 0x419C, 0x41A0, 0x41A4,
	0x41A8, 0x4270, 0x4288, 0x428C, 0x4290, 0x4294, 0x42E8,
	0x42EC, 0x434C, 0x4378, 0x4380, 0x4384, 0x4440, 0x4444,
	0x4970, 0x4B40, 0x4B44, 0x4B48, 0x4B4C,

	/* RX Registers */
	0x5108, 0x54CC, 0x55B0, 0x55B4, 0x55C0, 0x55E0, 0x55E4,
	0x5640, 0x5718, 0x5728, 0x5738, 0x5748, 0x5758, 0x5768,
	0x5778, 0x5788, 0x5808, 0x580C, 0x5A10, 0x5B14, 0x5B34,
	0x5B54, 0x5B74, 0x5B94, 0x5BB4, 0x5BD4, 0x5BF4, 0x5C14,
	0x5C34, 0x5C54, 0x5C74, 0x5C94, 0x5CB4, 0x5CD4, 0x5CF4,
	0x5D14, 0x5D34, 0x5D54, 0x5D74, 0x5D94, 0x5DB4, 0x5DD4,
	0x5DF4, 0x5E14, 0x5E34, 0x5E54, 0x5E74, 0x5E94, 0x5EB4,
	0x5ED4, 0x5EF4, 0x6900, 0x6F00, 0x6F20,

	/* TX Registers */
	0x7808, 0x7848, 0x784C, 0x78B0, 0x78B4, 0x7918, 0x7928,
	0x7938, 0x7948, 0x7958, 0x7968, 0x7978, 0x7988, 0x7A34,
	0x7B10, 0x7B14, 0x7B18, 0x7C14, 0x7C54, 0x7C94, 0x7CD4,
	0x7D14, 0x7D54, 0x7D94, 0x7DD4, 0x7E14, 0x7E54, 0x7E94,
	0x7ED4, 0x7F14, 0x7F54, 0x7F94, 0x7FD4, 0x8014, 0x8054,
	0x8094, 0x80D4, 0x8114, 0x8154, 0x8194, 0x81D4, 0x8214,
	0x8254, 0x8294, 0x82D4, 0x8314, 0x8354, 0x8394, 0x83D4,
	0x8900, 0x8904, 0x8F00, 0x8F20,
};

static const char *atl_fwd_dir_str(struct atl_fwd_ring *ring)
{
	return ring->flags & ATL_FWR_TX ? "Tx" : "Rx";
}

static int atl_fwd_ring_tx(struct atl_fwd_ring *ring)
{
	return !!(ring->flags & ATL_FWR_TX);
}

static void *atl_fwd_frag_vaddr(struct atl_fwd_buf_frag *frag,
	struct atl_fwd_mem_ops *ops)
{
	if (ops->alloc_buf)
		return frag->buf;

	return page_to_virt(frag->page);
}

static int atl_fwd_get_frag(struct atl_fwd_ring *ring, int idx)
{
	struct page *pg;
	dma_addr_t daddr = 0;
	struct device *dev = &ring->nic->hw.pdev->dev;
	struct atl_fwd_bufs *bufs = ring->bufs;
	struct atl_fwd_buf_frag *frag = &bufs->frags[idx];
	struct atl_fwd_mem_ops *ops = ring->mem_ops;

	if (ops->alloc_buf) {
		void *buf = ops->alloc_buf(dev, bufs->frag_size,
			&daddr, GFP_KERNEL, ops);

		if (!IS_ERR_OR_NULL(buf)) {
			frag->buf = buf;
			frag->daddr = daddr;
			return 0;
		} else
			return PTR_ERR(buf);
	}

	pg = __dev_alloc_pages(GFP_KERNEL, bufs->order);
	if (!pg)
		return -ENOMEM;

	if (!(ring->flags & ATL_FWR_DONT_DMA_MAP)) {
		daddr = dma_map_page(dev, pg, 0, PAGE_SIZE << bufs->order,
			DMA_FROM_DEVICE);

		if (dma_mapping_error(dev, daddr)) {
			__free_pages(pg, bufs->order);
			return -ENOMEM;
		}
	}

	frag->daddr = daddr;
	frag->page = pg;

	return 0;
}

static void atl_fwd_free_bufs(struct atl_fwd_ring *ring)
{
	struct atl_nic *nic = ring->nic;
	struct device *dev = &nic->hw.pdev->dev;
	struct atl_fwd_bufs *bufs = ring->bufs;
	struct atl_fwd_mem_ops *ops = ring->mem_ops;
	int ring_size = ring->hw.size;
	int order;
	size_t frag_size;
	int i;

	if (!bufs)
		return;

	order = bufs->order;
	frag_size = bufs->frag_size;

	if (bufs->daddr_vec)
		dma_free_coherent(dev, ring_size * sizeof(dma_addr_t),
			bufs->daddr_vec, bufs->daddr_vec_base);

	if (ring->flags & ATL_FWR_WANT_VIRT_BUF_VEC)
		kfree(bufs->vaddr_vec);

	for (i = 0; i < bufs->num_pages; i++) {
		struct atl_fwd_buf_frag *frag = &bufs->frags[i];

		if (ops->free_buf) {
			if (frag->buf)
				ops->free_buf(frag->buf, dev, frag_size,
					frag->daddr, ops);
			continue;
		}

		if (frag->page) {
			if (!(ring->flags & ATL_FWR_DONT_DMA_MAP))
				dma_unmap_page(dev, frag->daddr,
					PAGE_SIZE << order,
					DMA_FROM_DEVICE);
			__free_pages(frag->page, order);
		}
	}

	kfree(bufs);
	ring->bufs = NULL;
}

static int atl_fwd_alloc_bufs(struct atl_fwd_ring *ring, int order)
{
	struct atl_nic *nic = ring->nic;
	int flags = ring->flags;
	int ring_size = ring->hw.size;
	int buf_size = ring->buf_size;
	struct device *dev = &nic->hw.pdev->dev;
	struct atl_fwd_mem_ops *ops = ring->mem_ops;
	struct atl_fwd_buf_frag *frag;
	struct atl_fwd_bufs *bufs;
	int num_pages, i;
	int ret;
	unsigned int pg_off = 0;
	bool want_dvec = !!(flags & ATL_FWR_WANT_DMA_BUF_VEC);
	bool want_vvec = !!(flags & ATL_FWR_WANT_VIRT_BUF_VEC);
	size_t frag_size;

	if (!(flags & ATL_FWR_ALLOC_BUFS))
		return 0;

	if (flags & ATL_FWR_CONTIG_BUFS) {
		frag_size = buf_size * ring_size;
		order = get_order(frag_size);
		num_pages = 1;
	} else {
		int bufs_per_page;

		frag_size = PAGE_SIZE << order;
		bufs_per_page = frag_size / buf_size;
		num_pages = ring_size / bufs_per_page +
			!!(ring_size % bufs_per_page);
	}

	bufs = kzalloc(sizeof(*bufs) +
			sizeof(struct atl_fwd_buf_frag) * num_pages,
		GFP_KERNEL);
	if (!bufs) {
		atl_nic_err("%s: couldn't alloc buffers structure\n", __func__);
		return -ENOMEM;
	}

	ring->bufs = bufs;
	bufs->num_pages = num_pages;
	bufs->order = order;
	bufs->frag_size = frag_size;

	for (i = 0; i < num_pages; i++) {
		ret = atl_fwd_get_frag(ring, i);
		if (ret) {
			atl_nic_err("%s: couldn't alloc buffer page (order %d)\n",
				__func__, order);
			goto free;
		}
	}

	frag = &bufs->frags[0];

	if (want_dvec) {
		ret = -ENOMEM;
		bufs->daddr_vec = dma_alloc_coherent(dev,
			ring_size * sizeof(dma_addr_t),
			&bufs->daddr_vec_base, GFP_KERNEL);
		if (!bufs->daddr_vec) {
			atl_nic_err("%s: couldn't alloc DMA addr table\n",
				__func__);
			goto free;
		}
	} else
		bufs->daddr_vec_base = frag[0].daddr;

	if (want_vvec) {
		ret = -ENOMEM;
		bufs->vaddr_vec = kcalloc(ring_size, sizeof(void *),
			GFP_KERNEL);
		if (!bufs->vaddr_vec) {
			atl_nic_err("%s: couldn't alloc virtual addr table\n",
				__func__);
			goto free;
		}
	} else
		bufs->vaddr_vec = atl_fwd_frag_vaddr(frag, ops);

	if (!(want_dvec || want_vvec))
		return 0;

	for (i = 0; i < ring_size; i++) {
		dma_addr_t daddr = frag->daddr + pg_off;

		if (want_dvec)
			bufs->daddr_vec[i] = daddr;
		if (want_vvec)
			bufs->vaddr_vec[i] = atl_fwd_frag_vaddr(frag, ops) +
				pg_off;

		pg_off += buf_size;
		if (pg_off + buf_size <= frag_size)
			continue;

		frag++;
		pg_off = 0;
	}

	return 0;

free:
	atl_fwd_free_bufs(ring);
	return ret;
}

static void atl_fwd_update_im(struct atl_fwd_ring *ring)
{
	struct atl_hw *hw = &ring->nic->hw;
	int idx = ring->idx;
	uint32_t addr, tx_reg;

	if (hw->chip_id == ATL_ANTIGUA)
		tx_reg = ATL2_TX_INTR_MOD_CTRL(idx);
	else
		tx_reg = ATL_TX_INTR_MOD_CTRL(idx);
	addr = atl_fwd_ring_tx(ring) ? tx_reg :
		ATL_RX_INTR_MOD_CTRL(idx);

	atl_write(hw, addr, (ring->intr_mod_max / 2) << 0x10 |
		(ring->intr_mod_min / 2) << 8 | 2);
}

static void atl_fwd_init_descr(struct atl_fwd_ring *fwd_ring)
{
	struct atl_hw_ring *ring = &fwd_ring->hw;
	int dir_tx = atl_fwd_ring_tx(fwd_ring);
	struct atl_fwd_buf_frag *frag = NULL;
	int buf_size = fwd_ring->buf_size;
	int ring_size = ring->size;
	size_t frag_size = 0;
	unsigned int pg_off;
	int i;

	memset(ring->descs, 0, ring_size * sizeof(*ring->descs));

	if (!(fwd_ring->flags & ATL_FWR_ALLOC_BUFS))
		return;

	frag = &fwd_ring->bufs->frags[0];
	frag_size = fwd_ring->bufs->frag_size;

	if (!(fwd_ring->flags & ATL_FWR_DONT_DMA_MAP)) {
		for (pg_off = 0, i = 0; i < ring_size; i++) {
			union atl_desc *desc = &ring->descs[i];
			dma_addr_t daddr = frag->daddr + pg_off;

			if (dir_tx) {
				/* init both daddr and dd for both cases:
				* daddr for head pointer writeback
				* dd for the descriptor writeback */
				desc->tx.daddr = daddr;
				desc->tx.dd = 1;
			} else {
				desc->rx.daddr = daddr;
			}

			pg_off += buf_size;
			if (pg_off + buf_size <= frag_size)
				continue;

			frag++;
			pg_off = 0;
		}
	}
}

static void atl_fwd_init_ring(struct atl_fwd_ring *fwd_ring)
{
	struct atl_hw *hw = &fwd_ring->nic->hw;
	struct atl_hw_ring *ring = &fwd_ring->hw;
	unsigned int flags = fwd_ring->flags;
	int dir_tx = atl_fwd_ring_tx(fwd_ring);
	int idx = fwd_ring->idx;
	int lxo_bit = !!(flags & ATL_FWR_LXO);

	/* Reinit descriptors as they could be stale after hardware reset */
	atl_fwd_init_descr(fwd_ring);

	atl_write(hw, ATL_RING_BASE_LSW(ring), ring->daddr);
	atl_write(hw, ATL_RING_BASE_MSW(ring), upper_32_bits(ring->daddr));

	if (dir_tx) {
		atl_write(hw, ATL_TX_RING_THRESH(ring),
			8 << 8 | 8 << 0x10 | 24 << 0x18);
		atl_write(hw, ATL_TX_RING_CTL(ring), ring->size);

		atl_write_bit(hw, ATL_TX_LSO_CTRL, idx, lxo_bit);
	} else {
		uint32_t ctrl = ring->size |
			!!(flags & ATL_FWR_VLAN) << 29;

		atl_write(hw, ATL_RX_RING_BUF_SIZE(ring),
			fwd_ring->buf_size / 1024);
		atl_write(hw, ATL_RX_RING_THRESH(ring),
			8 << 0x10 | 24 << 0x18);
		atl_write(hw, ATL_RX_RING_TAIL(ring), ring->size - 1);
		atl_write(hw, ATL_RX_RING_CTL(ring), ctrl);

		if (lxo_bit)
			atl_write_bits(hw, ATL_RX_LRO_PKT_LIM(idx),
				(idx & 7) * 4, 2, 3);

		atl_write_bit(hw, ATL_RX_LRO_CTRL1, idx, lxo_bit);
		atl_write_bit(hw, ATL_INTR_RSC_EN, idx, lxo_bit);
	}

	atl_fwd_update_im(fwd_ring);
}

void atl_fwd_release_ring(struct atl_fwd_ring *ring)
{
	struct atl_nic *nic = ring->nic;
	int idx = ring->idx;
	int dir_tx = atl_fwd_ring_tx(ring);
	struct atl_fwd *fwd = &nic->fwd;
	unsigned long *map = &fwd->ring_map[dir_tx];
	struct atl_fwd_ring **rings = fwd->rings[dir_tx];
	struct atl_fwd_mem_ops *ops = ring->mem_ops;
	struct device *dev = &nic->hw.pdev->dev;
	struct atl_hw_ring *hwring = &ring->hw;

	atl_fwd_disable_ring(ring);

	if (ring->evt) {
		atl_fwd_disable_event(ring->evt);
		atl_fwd_release_event(ring->evt);
	}

	atl_do_reset(nic);

	__clear_bit(idx, map);
	rings[idx - ATL_FWD_RING_BASE] = NULL;
	atl_fwd_free_bufs(ring);
	if (ops->free_descs)
		ops->free_descs(hwring->descs, dev,
			hwring->size * sizeof(*hwring->descs), hwring->daddr,
			ops);
	else
		atl_free_descs(nic, &ring->hw, 0);
	kfree(ring);
}
EXPORT_SYMBOL(atl_fwd_release_ring);

static unsigned int atl_fwd_rx_mod_max = 25, atl_fwd_rx_mod_min = 15,
	atl_fwd_tx_mod_max = 25, atl_fwd_tx_mod_min = 15;
atl_module_param(fwd_rx_mod_max, uint, 0644);
atl_module_param(fwd_rx_mod_min, uint, 0644);
atl_module_param(fwd_tx_mod_max, uint, 0644);
atl_module_param(fwd_tx_mod_min, uint, 0644);

static struct atl_fwd_mem_ops null_ops;

struct atl_fwd_ring *atl_fwd_request_ring(struct net_device *ndev,
	int flags, int ring_size, int buf_size, int page_order,
	struct atl_fwd_mem_ops *ops)
{
	struct atl_nic *nic = netdev_priv(ndev);
	struct atl_fwd *fwd = &nic->fwd;
	int dir_tx = !!(flags & ATL_FWR_TX);
	unsigned long *map = &fwd->ring_map[dir_tx];
	struct atl_fwd_ring **rings = fwd->rings[dir_tx], *ring;
	struct device *dev = &nic->hw.pdev->dev;
	struct atl_hw_ring *hwring;
	int ret = -ENOMEM;
	int idx;

	if (ring_size & 7 || ring_size > ATL_MAX_RING_SIZE) {
		atl_nic_err("%s: bad ring size %d, must be no more than %d and a multiple of 8\n",
			__func__, ring_size, ATL_MAX_RING_SIZE);
		return ERR_PTR(-EINVAL);
	}

	if (buf_size & 1023 || buf_size > 16 * 1024) {
		atl_nic_err("%s: bad buffer size %d, must be no more than 16k and a multiple of 1024\n",
			__func__, buf_size);
		return ERR_PTR(-EINVAL);
	}

	if (!ops)
		ops = &null_ops;

	if ((ops->alloc_buf && !ops->free_buf) ||
		(ops->free_buf && !ops->alloc_buf)) {
		atl_nic_err("%s: must provide either both buffer allocator and deallocator or none\n",
			__func__);
		return ERR_PTR(-EINVAL);
	}

	if ((ops->alloc_descs && !ops->free_descs) ||
		(ops->free_descs && !ops->alloc_descs)) {
		atl_nic_err("%s: must provide either both descriptor ring allocator and deallocator or none\n",
			__func__);
		return ERR_PTR(-EINVAL);
	}

	if ((flags & (ATL_FWR_WANT_DMA_BUF_VEC | ATL_FWR_DONT_DMA_MAP)) ==
		(ATL_FWR_WANT_DMA_BUF_VEC | ATL_FWR_DONT_DMA_MAP)) {
		atl_nic_err("%s: ATL_FWR_WANT_DMA_BUF_VEC and ATL_FWR_DONT_DMA_MAP flags are mutually exclusive\n",
			__func__);
		return ERR_PTR(-EINVAL);
	}

	idx = find_next_zero_bit(map, ATL_FWD_RING_BASE + ATL_NUM_FWD_RINGS,
		ATL_FWD_RING_BASE);
	if (idx >= ATL_FWD_RING_BASE + ATL_NUM_FWD_RINGS) {
		atl_nic_err("%s: no more rings available\n", __func__);
		return ERR_PTR(ret);
	}

	ring = kzalloc(sizeof(*ring), GFP_KERNEL);
	if (!ring) {
		atl_nic_err("%s: couldn't alloc ring structure\n", __func__);
		return ERR_PTR(ret);
	}

	ring->nic = nic;
	ring->idx = idx;
	ring->flags = flags;
	hwring = &ring->hw;
	hwring->size = ring_size;
	ring->buf_size = buf_size;
	ring->mem_ops = ops;

	if (ops->alloc_descs) {
		void *descs;
		dma_addr_t daddr;

		descs = ops->alloc_descs(dev,
			ring_size * sizeof(*hwring->descs), &daddr, GFP_KERNEL,
			ops);
		if (!IS_ERR_OR_NULL(descs)) {
			hwring->descs = descs;
			hwring->daddr = daddr;
			ret = 0;
		} else
			ret = PTR_ERR(descs);
	} else
		ret = atl_alloc_descs(nic, hwring, 0);

	if (ret) {
		atl_nic_err("%s: couldn't alloc the ring\n", __func__);
		goto free_ring;
	}

	hwring->reg_base = dir_tx ? ATL_TX_RING(idx) : ATL_RX_RING(idx);

	ret = atl_fwd_alloc_bufs(ring, page_order);
	if (ret)
		goto free_descs;

	__set_bit(idx, map);
	rings[idx - ATL_FWD_RING_BASE] = ring;

	if (dir_tx) {
		ring->intr_mod_max = atl_fwd_tx_mod_max;
		ring->intr_mod_min = atl_fwd_tx_mod_min;
	} else {
		ring->intr_mod_max = atl_fwd_rx_mod_max;
		ring->intr_mod_min = atl_fwd_rx_mod_min;
	}

	atl_fwd_init_ring(ring);
	return ring;

free_descs:
	if (ops->free_descs)
		ops->free_descs(hwring->descs, dev,
			hwring->size * sizeof(*hwring->descs), hwring->daddr,
			ops);
	else
		atl_free_descs(nic, hwring, 0);

free_ring:
	kfree(ring);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(atl_fwd_request_ring);

int atl_fwd_set_ring_intr_mod(struct atl_fwd_ring *ring, int min, int max)
{
	struct atl_nic *nic = ring->nic;
	if (atl_fwd_ring_tx(ring) && ring->evt &&
		ring->evt->flags & ATL_FWD_EVT_TXWB) {
		struct atl_nic *nic = ring->nic;

		atl_nic_err("%s: Interrupt moderation not supported for head pointer writeback events\n",
			__func__);
		return -EINVAL;
	}

	if (min >= 0) {
		if (min > 511) {
			atl_nic_err("%s: min delay out of range (0..511): %d\n",
				__func__, min);
			return -EINVAL;
		}
		ring->intr_mod_min = min;
	}

	if (max >= 0) {
		if (max > 1023) {
			atl_nic_err("%s: max delay out of range (0..1023): %d\n",
				__func__, max);
			return -EINVAL;
		}
		ring->intr_mod_max = max;
	}

	atl_fwd_update_im(ring);
	return 0;
}
EXPORT_SYMBOL(atl_fwd_set_ring_intr_mod);

void atl_fwd_release_rings(struct atl_nic *nic)
{
	struct atl_fwd_ring *ring;
	int i;

	for (i = 0; i < ATL_NUM_FWD_RINGS * ATL_FWDIR_NUM; i++) {
		ring = nic->fwd.rings[i % ATL_FWDIR_NUM][i / ATL_FWDIR_NUM];
		if (ring)
			atl_fwd_release_ring(ring);
	}
}

int atl_fwd_enable_ring(struct atl_fwd_ring *ring)
{
	struct atl_hw *hw = &ring->nic->hw;

	atl_set_bits(hw, ATL_RING_CTL(&ring->hw), BIT(31));
	atl_clear_bits(hw, ATL_RING_CTL(&ring->hw), BIT(30));
	ring->state |= ATL_FWR_ST_ENABLED;

	return 0;
}
EXPORT_SYMBOL(atl_fwd_enable_ring);

void atl_fwd_disable_ring(struct atl_fwd_ring *ring)
{
	struct atl_hw *hw = &ring->nic->hw;

	if (!(ring->state & ATL_FWR_ST_ENABLED))
		return;

	atl_clear_bits(hw, ATL_RING_CTL(&ring->hw), BIT(31));
	atl_set_bits(hw, ATL_RING_CTL(&ring->hw), BIT(30));
	ring->state &= ~ATL_FWR_ST_ENABLED;
}
EXPORT_SYMBOL(atl_fwd_disable_ring);

static void __iomem *atl_msix_bar(struct atl_nic *nic)
{
	struct pci_dev *pdev = nic->hw.pdev;
	struct msi_desc *msi;

	if (!pdev->msix_enabled)
		return NULL;

	msi = list_first_entry(dev_to_msi_list(&pdev->dev),
		struct msi_desc, list);
	return msi->mask_base;
}

static int atl_fwd_set_msix_vec(struct atl_nic *nic, struct atl_fwd_event *evt)
{
	int idx = evt->idx;
	uint64_t addr = evt->msi_addr;
	uint32_t data = evt->msi_data;
	uint32_t ctrl;
	void __iomem *desc = atl_msix_bar(nic);

	if (!desc)
		return -EIO;

	desc += idx * PCI_MSIX_ENTRY_SIZE;

	/* MSI-X table updates must be atomic, so mask first */
	ctrl = readl(desc + PCI_MSIX_ENTRY_VECTOR_CTRL);
	writel(ctrl | PCI_MSIX_ENTRY_CTRL_MASKBIT,
		desc + PCI_MSIX_ENTRY_VECTOR_CTRL);

	/* Program the vector */
	writel(addr & (BIT_ULL(32) - 1), desc + PCI_MSIX_ENTRY_LOWER_ADDR);
	writel(addr >> 32, desc + PCI_MSIX_ENTRY_UPPER_ADDR);
	writel(data, desc + PCI_MSIX_ENTRY_DATA);

	/* Unmask */
	writel(ctrl & ~PCI_MSIX_ENTRY_CTRL_MASKBIT,
		desc + PCI_MSIX_ENTRY_VECTOR_CTRL);

	return 0;
}

void atl_fwd_release_event(struct atl_fwd_event *evt)
{
	struct atl_fwd_ring *ring = evt->ring;
	struct atl_nic *nic = ring->nic;
	unsigned long *map = &nic->fwd.msi_map;
	int idx = evt->idx;

	if (ring->evt != evt) {
		atl_nic_err("%s: attempt to release unset event\n", __func__);
		return;
	}

	atl_fwd_disable_event(evt);

	ring->evt = NULL;

	if (evt->flags & ATL_FWD_EVT_TXWB)
		return;

	__clear_bit(idx, map);
	atl_set_intr_bits(&nic->hw, ring->idx,
		atl_fwd_ring_tx(ring) ? -1 : ATL_NUM_MSI_VECS,
		atl_fwd_ring_tx(ring) ? ATL_NUM_MSI_VECS : -1);
}
EXPORT_SYMBOL(atl_fwd_release_event);

static int atl_fwd_init_event(struct atl_fwd_event *evt)
{
	struct atl_fwd_ring *ring = evt->ring;
	int dir_tx = atl_fwd_ring_tx(ring);
	struct atl_nic *nic = ring->nic;
	struct atl_hw *hw = &nic->hw;
	bool tx_wb = !!(evt->flags & ATL_FWD_EVT_TXWB);
	int idx;
	int ret;

	if (tx_wb) {
		struct atl_hw_ring *hwring = &ring->hw;

		atl_write(hw, ATL_TX_RING_HEAD_WB_LSW(hwring),
			  evt->tx_head_wrb);
		atl_write(hw, ATL_TX_RING_HEAD_WB_MSW(hwring),
			  upper_32_bits(evt->tx_head_wrb));
		return 0;
	}

	idx = evt->idx;

	ret = atl_fwd_set_msix_vec(nic, evt);
	if (ret)
		return ret;

	atl_set_intr_bits(&nic->hw, ring->idx,
			  dir_tx ? -1 : idx,
			  dir_tx ? idx : -1);

	atl_write_bit(hw, ATL_INTR_AUTO_CLEAR, idx, 1);
	atl_write_bit(hw, ATL_INTR_AUTO_MASK, idx,
		      !!(evt->flags & ATL_FWD_EVT_AUTOMASK));

	return 0;
}

int atl_fwd_request_event(struct atl_fwd_event *evt)
{
	struct atl_fwd_ring *ring = evt->ring;
	struct atl_nic *nic = ring->nic;
	unsigned long *map = &nic->fwd.msi_map;
	bool tx_wb = !!(evt->flags & ATL_FWD_EVT_TXWB);
	int idx;
	int ret;

	if (ring->evt) {
		atl_nic_err("%s: event already set for %s ring %d\n",
			__func__, atl_fwd_dir_str(ring), ring->idx);
		return -EEXIST;
	}

	if (!tx_wb && !(nic->flags & ATL_FL_MULTIPLE_VECTORS)) {
		atl_nic_err("%s: MSI-X interrupts are disabled\n", __func__);
		return -EINVAL;
	}

	if (tx_wb && !atl_fwd_ring_tx(ring)) {
		atl_nic_err("%s: head pointer writeback events supported "
			"on Tx rings only\n", __func__);
		return -EINVAL;
	}

	if ((evt->flags & (ATL_FWD_EVT_TXWB | ATL_FWD_EVT_AUTOMASK)) ==
		(ATL_FWD_EVT_TXWB | ATL_FWD_EVT_AUTOMASK)) {
		atl_nic_err("%s: event automasking supported "
			"for MSI events only\n", __func__);
		return -EINVAL;
	}

	ring->evt = evt;

	if (tx_wb) {
		ret = atl_fwd_init_event(evt);
		if (ret)
			goto fail;

		return 0;
	}

	idx = find_next_zero_bit(map, ATL_NUM_MSI_VECS, ATL_FWD_MSI_BASE);
	if (idx >= ATL_NUM_MSI_VECS) {
		atl_nic_err("%s: no MSI vectors left\n", __func__);
		ret = -ENOMEM;
		goto fail;
	}

	evt->idx = idx;

	ret = atl_fwd_init_event(evt);
	if (ret)
		goto fail;

	__set_bit(idx, map);

	return 0;

fail:
	ring->evt = NULL;
	return ret;
}
EXPORT_SYMBOL(atl_fwd_request_event);

int atl_fwd_enable_event(struct atl_fwd_event *evt)
{
	struct atl_fwd_ring *ring = evt->ring;
	struct atl_hw *hw = &ring->nic->hw;

	if (evt->flags & ATL_FWD_EVT_TXWB) {
		if (ring->state & ATL_FWR_ST_ENABLED)
			return -EINVAL;

		atl_write_bit(hw, ATL_TX_RING_CTL(&ring->hw), 28, 1);
		ring->state |= ATL_FWR_ST_EVT_ENABLED;
		return 0;
	}

	atl_intr_enable(hw, BIT(evt->idx));
	ring->state |= ATL_FWR_ST_EVT_ENABLED;
	return 0;
}
EXPORT_SYMBOL(atl_fwd_enable_event);

int atl_fwd_disable_event(struct atl_fwd_event *evt)
{
	struct atl_fwd_ring *ring = evt->ring;
	struct atl_hw *hw = &ring->nic->hw;

	if (evt->flags & ATL_FWD_EVT_TXWB) {
		if (ring->state & ATL_FWR_ST_ENABLED)
			return -EINVAL;

		atl_write_bit(hw, ATL_TX_RING_CTL(&ring->hw), 28, 0);
		ring->state &= ~ATL_FWR_ST_EVT_ENABLED;
		return 0;
	}

	atl_intr_disable(hw, BIT(evt->idx));
	ring->state &= ~ATL_FWR_ST_EVT_ENABLED;
	return 0;
}
EXPORT_SYMBOL(atl_fwd_disable_event);

int atl_fwd_receive_skb(struct net_device *ndev, struct sk_buff *skb)
{
	struct atl_nic *nic = netdev_priv(ndev);

	nic->stats.rx_fwd.packets++;
	nic->stats.rx_fwd.bytes += skb->len;

	skb->protocol = eth_type_trans(skb, ndev);
	return netif_rx(skb);
}
EXPORT_SYMBOL(atl_fwd_receive_skb);

int atl_fwd_napi_receive_skb(struct net_device *ndev, struct sk_buff *skb)
{
	struct atl_nic *nic = netdev_priv(ndev);

	nic->stats.rx_fwd.packets++;
	nic->stats.rx_fwd.bytes += skb->len;

	skb->protocol = eth_type_trans(skb, ndev);
	return netif_receive_skb(skb);
}
EXPORT_SYMBOL(atl_fwd_napi_receive_skb);

int atl_fwd_transmit_skb(struct net_device *ndev, struct sk_buff *skb)
{
	skb->dev = ndev;
	return dev_queue_xmit(skb);
}
EXPORT_SYMBOL(atl_fwd_transmit_skb);

int atl_fwd_register_notifier(struct net_device *ndev,
			      struct notifier_block *n)
{
	struct atl_nic *nic = netdev_priv(ndev);
	struct atl_fwd *fwd = &nic->fwd;

	return blocking_notifier_chain_register(&fwd->nh_clients, n);
}
EXPORT_SYMBOL(atl_fwd_register_notifier);

int atl_fwd_unregister_notifier(struct net_device *ndev,
				struct notifier_block *n)
{
	struct atl_nic *nic = netdev_priv(ndev);
	struct atl_fwd *fwd = &nic->fwd;

	return blocking_notifier_chain_unregister(&fwd->nh_clients, n);
}
EXPORT_SYMBOL(atl_fwd_unregister_notifier);

void atl_fwd_notify(struct atl_nic *nic, enum atl_fwd_notify notif, void *data)
{
	blocking_notifier_call_chain(&nic->fwd.nh_clients, notif, data);
}

int atl_fwd_reconfigure_rings(struct atl_nic *nic)
{
	struct atl_fwd_ring *ring;
	int i;
	int ret;

	for (i = 0; i < ATL_NUM_FWD_RINGS * ATL_FWDIR_NUM; i++) {
		ring = nic->fwd.rings[i % ATL_FWDIR_NUM][i / ATL_FWDIR_NUM];

		if (!ring)
			continue;

		atl_fwd_init_ring(ring);

		if (ring->evt) {
			ret = atl_fwd_init_event(ring->evt);
			if (ret)
				return ret;

			if (ring->state & ATL_FWR_ST_EVT_ENABLED) {
				ret = atl_fwd_enable_event(ring->evt);
				if (ret)
					return ret;
			}
		}
	}

	return 0;
}

int atl_fwd_suspend_rings(struct atl_nic *nic)
{
	atl_fwd_notify(nic, ATL_FWD_NOTIFY_RESET_PREPARE, NULL);

	return 0;
}

int atl_fwd_resume_rings(struct atl_nic *nic)
{
	struct atl_fwd_ring *ring;
	int i;
	int ret;

	ret = atl_fwd_reconfigure_rings(nic);
	if (ret)
		goto err;

	atl_fwd_notify(nic, ATL_FWD_NOTIFY_RESET_COMPLETE, NULL);

	for (i = 0; i < ATL_NUM_FWD_RINGS * ATL_FWDIR_NUM; i++) {
		ring = nic->fwd.rings[i % ATL_FWDIR_NUM][i / ATL_FWDIR_NUM];

		if (!ring)
			continue;

		if ((ring->state & ATL_FWR_ST_ENABLED))
			atl_fwd_enable_ring(ring);
	}
err:
	return ret;
}

int atl_get_ext_stats(struct net_device *ndev, struct atl_ext_stats *stats)
{
	struct atl_nic *nic = netdev_priv(ndev);

	if (!stats)
		return -EINVAL;

	atl_update_eth_stats(nic);
	atl_update_global_stats(nic);

	memcpy(&stats->rx, &nic->stats.rx, sizeof(stats->rx));
	memcpy(&stats->tx, &nic->stats.tx, sizeof(stats->tx));
	memcpy(&stats->rx_fwd, &nic->stats.rx_fwd, sizeof(stats->rx_fwd));
	memcpy(&stats->eth, &nic->stats.eth, sizeof(stats->eth));

	return 0;
}
EXPORT_SYMBOL(atl_get_ext_stats);

static bool atl_skip_register(u32 reg)
{
	int i, count;

	count = sizeof(atl_skip_reglist) / sizeof(u32);
	for (i = 0; i < count; i++)
		if (atl_skip_reglist[i] == reg)
			return true;

	return false;
}

static int atl_get_crash_dump_regs(struct atl_hw *hw, struct atl_crash_dump_regs *section)
{
	int addr, index;

	section->type = atl_crash_dump_type_regs;
	section->length = sizeof(struct atl_crash_dump_regs);

	/* prefill with 'skip' value */
	for (addr = 0; addr < sizeof(section->regs_data); addr++)
		section->regs_data[addr] = 0xFFFFFFFF;

	for (addr = 0x1000, index = 0x1000 / 4; addr < 0x9000; addr += 4, index++) {
		if (atl_skip_register(addr))
			continue;
		section->regs_data[index] = atl_read(hw, addr);
	}

	return section->length;
}

static int atl_get_crash_dump_fwiface(struct atl_hw *hw, struct atl_crash_dump_fwiface *section)
{
	int i;

	section->type = atl_crash_dump_type_fwiface;
	section->length = sizeof(struct atl_crash_dump_fwiface);

	for (i = 0; i < ARRAY_SIZE(section->fw_interface_in); i++)
		section->fw_interface_in[i] = atl_read(hw, ATL2_MIF_SHARED_BUFFER_IN(i));

	for (i = 0; i < ARRAY_SIZE(section->fw_interface_out); i++)
		section->fw_interface_out[i] = atl_read(hw, ATL2_MIF_SHARED_BUFFER_OUT(i));

	return section->length;
}

static int atl_get_crash_dump_act_res(struct atl_hw *hw, struct atl_crash_dump_act_res *section)
{
	int i, idx, err;

	section->type = atl_crash_dump_type_act_res;
	section->length = sizeof(struct atl_crash_dump_act_res);

	err = atl_hwsem_get(hw, ATL2_MCP_SEM_ACT_RSLVR);
	if (err) {
		printk("Failed to aquire act_res semaphore\n");
		goto ret;
	}

	for (i = 0, idx = 0; i < ATL_ACT_RES_TABLE_SIZE / 3; i++) {
		section->act_res_data[idx++] = atl_read(hw, ATL2_RPF_ACT_RSLVR_REQ_TAG(i));
		section->act_res_data[idx++] = atl_read(hw, ATL2_RPF_ACT_RSLVR_TAG_MASK(i));
		section->act_res_data[idx++] = atl_read(hw, ATL2_RPF_ACT_RSLVR_ACTN(i));
	}

	atl_hwsem_put(hw, ATL2_MCP_SEM_ACT_RSLVR);

ret:
	return section->length;
}

static int atl_get_crash_dump_ring(struct atl_nic *nic, int idx,
				   struct atl_crash_dump_ring *section)
{
	int size = sizeof(struct atl_crash_dump_ring), offset;
	struct atl_queue_vec *qvec = &nic->qvecs[idx];
	struct atl_hw_ring *hwring;

	if (!test_bit(ATL_ST_UP, &nic->hw.state)) {
		memset(section, 0, size);
		goto ret;
	}

	section->type = atl_crash_dump_type_ring;
	section->index = qvec->idx;
	section->length = size;
	section->rx_head = qvec->rx.head;
	section->rx_tail = qvec->rx.tail;
	section->tx_head = qvec->tx.head;
	section->tx_tail = qvec->tx.tail;
	hwring = &qvec->rx.hw;
	section->rx_ring_size = hwring->size;
	memcpy(section->ring_data, hwring->descs, hwring->size * sizeof(*hwring->descs));
	offset = hwring->size * sizeof(*hwring->descs);
	hwring = &qvec->tx.hw;
	section->tx_ring_size = hwring->size;
	memcpy(section->ring_data + offset, hwring->descs, hwring->size * sizeof(*hwring->descs));

ret:
	return size;
}


int atl_get_crash_dump(struct net_device *ndev, struct atl_crash_dump *crash_dump)
{
	struct atl_nic *nic = netdev_priv(ndev);
	u32 fw_rev = nic->hw.mcp.fw_rev;
	int recorded_sz = 0, i, nvecs;
	u8 *section;

	if (!crash_dump)
		return sizeof(struct atl_crash_dump);

	crash_dump->length = sizeof(*crash_dump);
	crash_dump->sections_count = 0;

	strlcpy(crash_dump->drv_version, ATL_VERSION, sizeof(crash_dump->drv_version));
	snprintf(crash_dump->fw_version, sizeof(crash_dump->fw_version),
		"%d.%d.%d", fw_rev >> 24, fw_rev >> 16 & 0xff, fw_rev & 0xffff);

	if (nic->hw.chip_id == ATL_ANTIGUA)
		section = (void *)&crash_dump->antigua.regs;
	else
		section = (void *)&crash_dump->atlantic.regs;

	recorded_sz = atl_get_crash_dump_regs(&nic->hw, (void *)section);
	crash_dump->sections_count++;
	crash_dump->length += recorded_sz;
	section += recorded_sz;

	if (nic->hw.chip_id == ATL_ANTIGUA) {
		recorded_sz = atl_get_crash_dump_fwiface(&nic->hw, (void *)section);
		crash_dump->sections_count++;
		crash_dump->length += recorded_sz;
		section += recorded_sz;

		recorded_sz = atl_get_crash_dump_act_res(&nic->hw, (void *)section);
		crash_dump->sections_count++;
		crash_dump->length += recorded_sz;
		section += recorded_sz;
	}

	nvecs = min_t(int, nic->nvecs, ATL_MAX_QUEUES);
	for (i = 0; i < nvecs; i++) {
		recorded_sz = atl_get_crash_dump_ring(nic, i, (void *)section);
		crash_dump->sections_count++;
		crash_dump->length += recorded_sz;
		section += recorded_sz;
	}

	return crash_dump->length;
}
EXPORT_SYMBOL(atl_get_crash_dump);
