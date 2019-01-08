/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2018 aQuantia Corporation. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#include <linux/etherdevice.h>
#include "atl_common.h"
#include "atl_desc.h"

static const char *atl_fwd_dir_str(struct atl_fwd_ring *ring)
{
	return ring->flags & ATL_FWR_TX ? "Tx" : "Rx";
}

static int atl_fwd_ring_tx(struct atl_fwd_ring *ring)
{
	return !!(ring->flags & ATL_FWR_TX);
}

static int atl_fwd_get_page(struct atl_fwd_buf_page *bpg, struct device *dev,
	int order)
{
	struct page *pg = dev_alloc_pages(order);
	dma_addr_t daddr;

	if (!pg)
		return -ENOMEM;

	daddr = dma_map_page(dev, pg, 0, PAGE_SIZE << order, DMA_FROM_DEVICE);

	if (dma_mapping_error(dev, daddr)) {
		__free_pages(pg, order);
		return -ENOMEM;
	}

	bpg->daddr = daddr;
	bpg->page = pg;

	return 0;
}

static void atl_fwd_free_bufs(struct atl_fwd_ring *ring)
{
	struct atl_nic *nic = ring->nic;
	struct device *dev = &nic->hw.pdev->dev;
	struct atl_fwd_bufs *bufs = ring->bufs;
	int ring_size = ring->hw.size;
	int order = bufs->order;
	int i;

	if (!bufs)
		return;

	if (bufs->daddr_vec) {
		dma_free_coherent(dev, ring_size * sizeof(dma_addr_t),
			bufs->daddr_vec, bufs->daddr_vec_base);
		kfree(bufs->vaddr_vec);
	}


	for (i = 0; i < bufs->num_pages; i++) {
		struct atl_fwd_buf_page *bpg = &bufs->bpgs[i];

		if (bpg->page) {
			dma_unmap_page(dev, bpg->daddr,
				PAGE_SIZE << order,
				DMA_FROM_DEVICE);
			__free_pages(bpg->page, order);
		}
	}

	kfree(bufs);
	ring->bufs = NULL;
}

static int atl_fwd_alloc_bufs(struct atl_fwd_ring *ring,
	int order)
{
	struct atl_nic *nic = ring->nic;
	int flags = ring->flags;
	int ring_size = ring->hw.size;
	int buf_size = ring->buf_size;
	struct device *dev = &nic->hw.pdev->dev;
	struct atl_fwd_buf_page *bpg;
	struct atl_fwd_bufs *bufs;
	int num_pages, i;
	int ret;
	unsigned int pg_off = 0;
	bool want_vecs = !!(flags & ATL_FWR_WANT_BUF_VECS);

	if (!(flags & ATL_FWR_ALLOC_BUFS))
		return 0;

	if (flags & ATL_FWR_CONTIG_BUFS) {
		order = get_order(buf_size * ring_size);
		num_pages = 1;
	} else {
		int bufs_per_page = (PAGE_SIZE << order) / buf_size;
		num_pages = ring_size / bufs_per_page +
			!!(ring_size % bufs_per_page);
	}

	bufs = kzalloc(sizeof(*bufs) +
			sizeof(struct atl_fwd_buf_page) * num_pages,
		GFP_KERNEL);
	if (!bufs)
		return -ENOMEM;

	ring->bufs = bufs;
	bufs->num_pages = num_pages;
	bufs->order = order;

	bpg = bufs->bpgs;
	for (i = 0; i < num_pages; i++) {
		ret = atl_fwd_get_page(&bpg[i], dev, order);
		if (ret)
			goto free;
	}

	if (want_vecs) {
		ret = -ENOMEM;
		bufs->daddr_vec = dma_alloc_coherent(dev,
			ring_size * sizeof(dma_addr_t),
			&bufs->daddr_vec_base, GFP_KERNEL);
		if (!bufs->daddr_vec)
			goto free;

		bufs->vaddr_vec = kcalloc(ring_size, sizeof(void *),
			GFP_KERNEL);
		if (!bufs->vaddr_vec)
			goto free;
	} else {
		bufs->daddr_vec_base = bpg[0].daddr;
		bufs->vaddr_vec = page_to_virt(bpg[0].page);
	}

	bufs->paddr = page_to_phys(bpg[0].page);

	bpg = bufs->bpgs;
	for (i = 0; i < ring_size; i++) {
		union atl_desc *desc = &ring->hw.descs[i];
		dma_addr_t daddr = bpg->daddr + pg_off;

		if (want_vecs) {
			bufs->daddr_vec[i] = daddr;
			bufs->vaddr_vec[i] = page_to_virt(bpg->page) + pg_off;
		}

		if (atl_fwd_ring_tx(ring))
			desc->tx.daddr = daddr;
		else
			desc->rx.daddr = daddr;

		pg_off += buf_size;
		if (pg_off + buf_size <= (PAGE_SIZE << order))
			continue;

		bpg++;
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
	uint32_t addr;

	addr = atl_fwd_ring_tx(ring) ? ATL_TX_INTR_MOD_CTRL(idx) :
		ATL_RX_INTR_MOD_CTRL(idx);

	atl_write(hw, addr, (ring->intr_mod_max / 2) << 0x10 |
		(ring->intr_mod_min / 2) << 8 | 2);
}

static void atl_fwd_init_ring(struct atl_fwd_ring *fwd_ring)
{
	struct atl_hw *hw = &fwd_ring->nic->hw;
	struct atl_hw_ring *ring = &fwd_ring->hw;
	unsigned int flags = fwd_ring->flags;
	int dir_tx = atl_fwd_ring_tx(fwd_ring);
	int idx = fwd_ring->idx;
	int lxo_bit = !!(flags & ATL_FWR_LXO);

	atl_write(hw, ATL_RING_BASE_LSW(ring), ring->daddr);
	atl_write(hw, ATL_RING_BASE_MSW(ring), ring->daddr >> 32);

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

	atl_fwd_disable_ring(ring);

	if (ring->evt) {
		atl_fwd_disable_event(ring->evt);
		atl_fwd_release_event(ring->evt);
	}

	__clear_bit(idx, map);
	rings[idx - ATL_FWD_RING_BASE] = NULL;
	atl_fwd_free_bufs(ring);
	atl_free_descs(nic, &ring->hw);
	kfree(ring);
}
EXPORT_SYMBOL(atl_fwd_release_ring);

static phys_addr_t atl_dma_coherent_virt_to_phys(void *vaddr)
{
	if (is_vmalloc_addr(vaddr))
		return page_to_phys(vmalloc_to_page(vaddr));
	else
		return virt_to_phys(vaddr);
}

static unsigned int atl_fwd_rx_mod_max = 25, atl_fwd_rx_mod_min = 15,
	atl_fwd_tx_mod_max = 25, atl_fwd_tx_mod_min = 15;
atl_module_param(fwd_rx_mod_max, uint, 0644);
atl_module_param(fwd_rx_mod_min, uint, 0644);
atl_module_param(fwd_tx_mod_max, uint, 0644);
atl_module_param(fwd_tx_mod_min, uint, 0644);

struct atl_fwd_ring *atl_fwd_request_ring(struct net_device *ndev,
	int flags, int ring_size, int buf_size, int page_order)
{
	struct atl_nic *nic = netdev_priv(ndev);
	struct atl_fwd *fwd = &nic->fwd;
	int dir_tx = !!(flags & ATL_FWR_TX);
	unsigned long *map = &fwd->ring_map[dir_tx];
	struct atl_fwd_ring **rings = fwd->rings[dir_tx], *ring;
	int ret = -ENOMEM;
	int idx;

	if (ring_size & 7 || ring_size > ATL_MAX_RING_SIZE) {
		atl_nic_err("%s: bad ring size %d, must be no more than %d "
			"and a multiple of 8\n", __func__, ring_size,
			ATL_MAX_RING_SIZE);
		return ERR_PTR(-EINVAL);
	}

	if (buf_size & 1023 || buf_size > 16 * 1024) {
		atl_nic_err("%s: bad buffer size %d, must be no more than 16k "
			"and a multiple of 1024\n",
			__func__, buf_size);
		return ERR_PTR(-EINVAL);
	}

	idx = find_next_zero_bit(map, ATL_FWD_RING_BASE + ATL_NUM_FWD_RINGS,
		ATL_FWD_RING_BASE);
	if (idx >= ATL_FWD_RING_BASE + ATL_NUM_FWD_RINGS)
		return ERR_PTR(ret);

	ring = kzalloc(sizeof(*ring), GFP_KERNEL);
	if (!ring)
		return ERR_PTR(ret);

	ring->nic = nic;
	ring->idx = idx;
	ring->flags = flags;
	ring->hw.size = ring_size;
	ring->buf_size = buf_size;

	ret = atl_alloc_descs(nic, &ring->hw);
	if (ret)
		goto free_ring;

	ring->hw.reg_base = dir_tx ? ATL_TX_RING(idx) : ATL_RX_RING(idx);

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

	ring->desc_paddr = atl_dma_coherent_virt_to_phys(ring->hw.descs);

	atl_fwd_init_ring(ring);
	return ring;

free_descs:
	atl_free_descs(nic, &ring->hw);

free_ring:
	kfree(ring);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(atl_fwd_request_ring);

int atl_fwd_set_ring_intr_mod(struct atl_fwd_ring *ring, int min, int max)
{
	if (atl_fwd_ring_tx(ring) && ring->evt &&
		ring->evt->flags & ATL_FWD_EVT_TXWB) {
		struct atl_nic *nic = ring->nic;

		atl_nic_err("%s: Interrupt moderation not supported for head pointer writeback events\n",
			__func__);
		return -EINVAL;
	}

	if (min >= 0)
		ring->intr_mod_min = min;

	if (max >= 0)
		ring->intr_mod_max = max;

	atl_fwd_update_im(ring);
	return 0;
}
EXPORT_SYMBOL(atl_fwd_set_ring_intr_mod);

void atl_fwd_release_rings(struct atl_nic *nic)
{
	struct atl_fwd_ring **rings = nic->fwd.rings[0];
	int i;

	for (i = 0; i < ATL_NUM_FWD_RINGS * 2; i++)
		if (rings[i])
			atl_fwd_release_ring(rings[i]);
}

static void atl_fwd_reset_ring(struct atl_fwd_ring *fwd_ring)
{
	struct atl_hw *hw = &fwd_ring->nic->hw;
	struct atl_hw_ring *ring = &fwd_ring->hw;

	atl_write(hw, ATL_RING_CTL(ring), BIT(19));
	udelay(10);
	atl_write(hw, ATL_RING_CTL(ring), 0);
}

int atl_fwd_enable_ring(struct atl_fwd_ring *ring)
{
	struct atl_hw *hw = &ring->nic->hw;

	atl_set_bits(hw, ATL_RING_CTL(&ring->hw), BIT(31));
	ring->state |= ATL_FWR_ST_ENABLED;

	return 0;
}
EXPORT_SYMBOL(atl_fwd_enable_ring);

void atl_fwd_disable_ring(struct atl_fwd_ring *ring)
{
	if (!(ring->state & ATL_FWR_ST_ENABLED))
		return;

	atl_fwd_reset_ring(ring);
	atl_fwd_init_ring(ring);
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

	if (evt->flags & ATL_FWD_EVT_TXWB)
		return;

	__clear_bit(idx, map);
	atl_set_intr_bits(&nic->hw, ring->idx,
		atl_fwd_ring_tx(ring) ? -1 : ATL_NUM_MSI_VECS,
		atl_fwd_ring_tx(ring) ? ATL_NUM_MSI_VECS : -1);
}
EXPORT_SYMBOL(atl_fwd_release_event);

int atl_fwd_request_event(struct atl_fwd_event *evt)
{
	struct atl_fwd_ring *ring = evt->ring;
	int dir_tx = atl_fwd_ring_tx(ring);
	struct atl_nic *nic = ring->nic;
	struct atl_hw *hw = &nic->hw;
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
		struct atl_hw_ring *hwring = &ring->hw;

		atl_write(hw, ATL_TX_RING_HEAD_WB_LSW(hwring),
			evt->tx_head_wrb);
		atl_write(hw, ATL_TX_RING_HEAD_WB_MSW(hwring),
			evt->tx_head_wrb >> 32);
		return 0;
	}

	idx = find_next_zero_bit(map, ATL_NUM_MSI_VECS, ATL_FWD_MSI_BASE);
	if (idx >= ATL_NUM_MSI_VECS) {
		atl_nic_err("%s: no MSI vectors left\n", __func__);
		ret = -ENOMEM;
		goto fail;
	}

	evt->idx = idx;

	ret = atl_fwd_set_msix_vec(nic, evt);
	if (ret)
		goto fail;

	__set_bit(idx, map);

	atl_set_intr_bits(&nic->hw, ring->idx,
		dir_tx ? -1 : idx,
		dir_tx ? idx : -1);

	atl_write_bit(hw, ATL_INTR_AUTO_CLEAR, idx, 1);
	atl_write_bit(hw, ATL_INTR_AUTO_MASK, idx,
		!!(evt->flags & ATL_FWD_EVT_AUTOMASK));

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
		return 0;
	}

	atl_intr_enable(hw, BIT(evt->idx));
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
		return 0;
	}

	atl_intr_disable(hw, BIT(evt->idx));
	return 0;
}
EXPORT_SYMBOL(atl_fwd_disable_event);

int atl_fwd_receive_skb(struct net_device *ndev, struct sk_buff *skb)
{
	skb->protocol = eth_type_trans(skb, ndev);
	return netif_rx(skb);
}
EXPORT_SYMBOL(atl_fwd_receive_skb);

int atl_fwd_transmit_skb(struct net_device *ndev, struct sk_buff *skb)
{
	skb->dev = ndev;
	return dev_queue_xmit(skb);
}
EXPORT_SYMBOL(atl_fwd_transmit_skb);

