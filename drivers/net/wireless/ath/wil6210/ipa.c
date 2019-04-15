// SPDX-License-Identifier: ISC
/* Copyright (c) 2019 The Linux Foundation. All rights reserved.
 */

#include <linux/moduleparam.h>
#include <linux/pci.h>
#include <linux/msi.h>
#include <linux/irq.h>
#include <linux/ipa_wigig.h>
#include <linux/iommu.h>
#include "wil6210.h"
#include "txrx_edma.h"
#include "txrx.h"
#include "ipa.h"

#define WIL_IPA_MSI_CAP	(0x50)
#define WIL_IPA_MSI_MSG_DATA 0xCAFE

#define WIL_IPA_BCAST_POLL_MS 20
#define WIL_IPA_READY_TO_MS 2000

/* IPA requires power of 2 buffer size */
#define WIL_IPA_RX_BUFF_SIZE (8 * 1024)
#define WIL_IPA_TX_BUFF_SIZE (2 * 1024)

u8 ipa_offload;
module_param(ipa_offload, byte, 0444);
MODULE_PARM_DESC(ipa_offload, " Enable IPA offload, default - disabled");

/* template IP headers */
static struct ethhdr wil_ipa_v4_hdr = {
	{0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
	{0x01, 0x02, 0x03, 0x04, 0x05, 0x07},
	0x0008
};

static struct ethhdr wil_ipa_v6_hdr = {
	{0x01, 0x02, 0x03, 0x04, 0x05, 0x08},
	{0x01, 0x02, 0x03, 0x04, 0x05, 0x09},
	0xDD86
};

int wil_ipa_get_bcast_sring_id(struct wil6210_priv *wil)
{
	struct wil_ipa *ipa = (struct wil_ipa *)wil->ipa_handle;

	return ipa->bcast_sring_id;
}

void wil_ipa_set_bcast_sring_id(struct wil6210_priv *wil, int bcast_sring_id)
{
	struct wil_ipa *ipa = (struct wil_ipa *)wil->ipa_handle;

	ipa->bcast_sring_id = bcast_sring_id;
}

static phys_addr_t wil_ipa_get_bar_base_pa(struct wil6210_priv *wil)
{
	struct pci_dev *pdev = wil->pdev;
	struct resource *res = &pdev->resource[0];

	return res->start;
}

static void
wil_ipa_calc_head_tail(struct wil6210_priv *wil, int ring_id, int sring_id,
		       phys_addr_t *desc_hwhead, phys_addr_t *desc_hwtail,
		       phys_addr_t *status_hwhead, phys_addr_t *status_hwtail)
{
	phys_addr_t bar_base_pa = wil_ipa_get_bar_base_pa(wil);

	/* HWTAIL / HWHEAD calculation:
	 * for descriptor rings:
	 * 1. SW should update the SUBQ producer pointer by writing to
	 *    SUBQ_RD_PTR (table of 64 entries, one for each Q), to update the
	 *    HW that there are descriptors waiting
	 * 2. SW should read the SUBQ_CONS information, in order to track the
	 *    used registers. SUBQ_CONS is a table of 32 entries, one for
	 *    each Q pair. lower 16bits are for even ring_id and upper 16bits
	 *    are for odd ring_id
	 * for status rings:
	 * 1. SW should read the COMPQ_PROD information
	 * 2. SW should write the COMPQ_RD_PTR which is actually the completion
	 *    queue consumer pointer
	 */

	*desc_hwhead = bar_base_pa +
		HOSTADDR(RGF_DMA_SCM_SUBQ_CONS + 4 * (ring_id / 2));
	*status_hwhead = bar_base_pa +
		HOSTADDR(RGF_DMA_SCM_COMPQ_PROD + 4 * (sring_id / 2));

	*desc_hwtail = bar_base_pa +
		HOSTADDR(RGF_SCM_PTRS_SUBQ_RD_PTR + 4 * ring_id);
	*status_hwtail = bar_base_pa +
		HOSTADDR(RGF_SCM_PTRS_COMPQ_RD_PTR + 4 * sring_id);

	wil_dbg_misc(wil,
		     "desc_hwhead %pad desc_hwtail %pad status_hwhead %pad status_hwtail %pad\n",
		     desc_hwhead, desc_hwtail, status_hwhead, status_hwtail);
}

static void wil_ipa_rx(struct wil_ipa *ipa, struct sk_buff *skb)
{
	struct wil6210_priv *wil = ipa->wil;
	struct net_device *ndev = wil->main_ndev;
	struct wil6210_vif *vif = ndev_to_vif(ndev);
	int cid, len = skb->len;
	u8 *sa = wil_skb_get_sa(skb);
	struct wil_net_stats *stats;

	wil_dbg_txrx(wil, "ipa_rx %d bytes\n", len);
	wil_hex_dump_txrx("Rx ", DUMP_PREFIX_OFFSET, 16, 1,
			  skb->data, skb_headlen(skb), false);

	if (unlikely(len < ETH_HLEN)) {
		wil_err(wil, "ipa rx packet too short %d\n", len);
		goto drop;
	}

	cid = wil_find_cid(wil, vif->mid, sa);
	if (unlikely(cid < 0)) {
		wil_err_ratelimited(wil, "cid not found for sa %pM\n", sa);
		goto drop;
	}

	stats = &wil->sta[cid].stats;

	wil_netif_rx(skb, ndev, cid, stats, false);

	return;

drop:
	dev_kfree_skb(skb);
	ndev->stats.rx_dropped++;
}

static void wil_ipa_notify_cb(void *priv, enum ipa_dp_evt_type evt,
			      unsigned long data)
{
	struct wil_ipa *ipa = (struct wil_ipa *)priv;
	struct sk_buff *skb;

	switch (evt) {
	case IPA_RECEIVE:
		skb = (struct sk_buff *)data;
		wil_ipa_rx(ipa, skb);
		break;
	default:
		wil_dbg_misc(ipa->wil, "unhandled ipa evt %d\n", evt);
		break;
	}
}

static int wil_ipa_wigig_conn_rx_pipe_smmu(struct wil_ipa *ipa)
{
	struct wil6210_priv *wil = ipa->wil;
	struct device *dev = wil_to_dev(wil);
	struct ipa_wigig_conn_rx_in_params_smmu in = {
		.notify = wil_ipa_notify_cb,
		.priv = ipa,
	};
	struct ipa_wigig_conn_out_params out = {0};
	int rc;
	struct wil_ring *rx_ring = &wil->ring_rx;
	struct wil_status_ring *rx_sring = &wil->srings[wil->rx_sring_idx];

	rc = dma_get_sgtable(dev, &in.pipe_smmu.desc_ring_base,
			     (void *)rx_ring->va, rx_ring->pa,
			     rx_ring->size * sizeof(rx_ring->va[0]));
	if (rc < 0) {
		wil_err(wil, "dma_get_sgtable for desc ring failed %d\n", rc);
		return rc;
	}

	in.pipe_smmu.desc_ring_base_iova = rx_ring->pa;
	in.pipe_smmu.desc_ring_size = rx_ring->size * sizeof(rx_ring->va[0]);

	rc = dma_get_sgtable(dev, &in.pipe_smmu.status_ring_base,
			     rx_sring->va, rx_sring->pa,
			     rx_sring->size * rx_sring->elem_size);
	if (rc < 0) {
		wil_err(wil, "dma_get_sgtable for status ring failed %d\n", rc);
		return rc;
	}
	in.pipe_smmu.status_ring_base_iova = rx_sring->pa;
	in.pipe_smmu.status_ring_size = rx_sring->size * rx_sring->elem_size;

	rc = dma_get_sgtable(dev, &in.dbuff_smmu.data_buffer_base,
			     ipa->rx_buf.va, ipa->rx_buf.pa, ipa->rx_buf.sz);
	if (rc < 0) {
		wil_err(wil, "dma_get_sgtable for data buffer failed %d\n", rc);
		return rc;
	}

	wil_ipa_calc_head_tail(wil, WIL_RX_DESC_RING_ID, wil->rx_sring_idx,
			       &in.pipe_smmu.desc_ring_HWHEAD_pa,
			       &in.pipe_smmu.desc_ring_HWTAIL_pa,
			       &in.pipe_smmu.status_ring_HWHEAD_pa,
			       &in.pipe_smmu.status_ring_HWTAIL_pa);

	in.dbuff_smmu.data_buffer_base_iova = ipa->rx_buf.pa;
	in.dbuff_smmu.data_buffer_size = wil->rx_buf_len;

	rc = ipa_wigig_conn_rx_pipe_smmu(&in, &out);
	if (rc) {
		wil_err(wil, "ipa_wigig_conn_rx_pipe_smmu failed %d\n", rc);
		return rc;
	}

	ipa->rx_client_type = out.client;

	return 0;
}

static int wil_ipa_wigig_conn_rx_pipe(struct wil_ipa *ipa)
{
	struct wil6210_priv *wil = ipa->wil;
	struct ipa_wigig_conn_rx_in_params in = {
		.notify = wil_ipa_notify_cb,
		.priv = ipa,
	};
	struct ipa_wigig_conn_out_params out = {0};
	int rc;
	struct wil_ring *rx_ring = &wil->ring_rx;
	struct wil_status_ring *rx_sring = &wil->srings[wil->rx_sring_idx];

	in.pipe.desc_ring_base_pa = rx_ring->pa;
	in.pipe.desc_ring_size = rx_ring->size * sizeof(rx_ring->va[0]);
	in.pipe.status_ring_base_pa = rx_sring->pa;
	in.pipe.status_ring_size = rx_sring->size * rx_sring->elem_size;

	wil_ipa_calc_head_tail(wil, WIL_RX_DESC_RING_ID, wil->rx_sring_idx,
			       &in.pipe.desc_ring_HWHEAD_pa,
			       &in.pipe.desc_ring_HWTAIL_pa,
			       &in.pipe.status_ring_HWHEAD_pa,
			       &in.pipe.status_ring_HWTAIL_pa);

	in.dbuff.data_buffer_base_pa = ipa->rx_buf.pa;
	in.dbuff.data_buffer_size = wil->rx_buf_len;

	wil_dbg_misc(wil,
		     "calling ipa_wigig_conn_rx_pipe, desc_ring_base_pa %pad status_ring_base_pa %pad data_buffer_base_pa %pad\n",
		     &in.pipe.desc_ring_base_pa, &in.pipe.status_ring_base_pa,
		     &in.dbuff.data_buffer_base_pa);

	rc = ipa_wigig_conn_rx_pipe(&in, &out);
	if (rc) {
		wil_err(wil, "ipa_wigig_conn_rx_pipe failed %d\n", rc);
		return rc;
	}

	ipa->rx_client_type = out.client;

	return 0;
}

static int wil_ipa_wigig_reg_intf(struct wil_ipa *ipa)
{
	struct wil6210_priv *wil = ipa->wil;
	struct net_device *ndev = wil->main_ndev;
	struct ipa_wigig_reg_intf_in_params in = {
		.netdev_name = ndev->name,
	};
	int rc;

	ether_addr_copy(in.netdev_mac, ndev->dev_addr);
	ether_addr_copy(wil_ipa_v4_hdr.h_source, ndev->dev_addr);
	ether_addr_copy(wil_ipa_v6_hdr.h_source, ndev->dev_addr);

	in.hdr_info[IPA_IP_v4].hdr = (u8 *)&wil_ipa_v4_hdr;
	in.hdr_info[IPA_IP_v4].hdr_len = sizeof(wil_ipa_v4_hdr);
	in.hdr_info[IPA_IP_v4].dst_mac_addr_offset =
		offsetof(struct ethhdr, h_dest);
	in.hdr_info[IPA_IP_v4].hdr_type = IPA_HDR_L2_ETHERNET_II;
	in.hdr_info[IPA_IP_v6].hdr = (u8 *)&wil_ipa_v6_hdr;
	in.hdr_info[IPA_IP_v6].hdr_len = sizeof(wil_ipa_v6_hdr);
	in.hdr_info[IPA_IP_v6].dst_mac_addr_offset =
		offsetof(struct ethhdr, h_dest);
	in.hdr_info[IPA_IP_v6].hdr_type = IPA_HDR_L2_ETHERNET_II;

	rc = ipa_wigig_reg_intf(&in);
	if (rc)
		wil_err(wil, "ipa_wigig_reg_intf failed %d\n", rc);

	return rc;
}

static void wil_ipa_set_ring_buf(struct wil6210_priv *wil,
				 struct wil_ring *ring, u32 i)
{
	struct wil_ipa *ipa = (struct wil_ipa *)wil->ipa_handle;
	size_t sz = wil->rx_buf_len;
	dma_addr_t pa;
	struct wil_rx_enhanced_desc dd, *d = &dd;
	struct wil_rx_enhanced_desc *_d = (struct wil_rx_enhanced_desc *)
		&ring->va[i].rx.enhanced;

	memset(&dd, 0, sizeof(dd));

	pa = ipa->rx_buf.pa + i * sz;

	wil_desc_set_addr_edma(&d->dma.addr, &d->dma.addr_high_high, pa);
	d->dma.length = cpu_to_le16(sz);
	d->mac.buff_id = cpu_to_le16(i);
	*_d = *d;
}

static void wil_ipa_rx_refill(struct wil6210_priv *wil)
{
	struct wil_ring *ring = &wil->ring_rx;
	int i;

	for (i = 0; i < ring->size; i++)
		wil_ipa_set_ring_buf(wil, ring, i);
	ring->swhead = ring->size - 1;

	/* commit to HW */
	wil_w(wil, ring->hwtail, ring->swhead);
}

static int wil_ipa_rx_alloc(struct wil_ipa *ipa)
{
	struct wil6210_priv *wil = ipa->wil;
	struct device *dev = wil_to_dev(wil);
	u16 status_ring_size, desc_ring_size;
	struct wil_ring *ring = &wil->ring_rx;
	int rc, sring_id;
	size_t elem_size = sizeof(struct wil_rx_status_compressed);

	/* in IPA offload must use compressed_rx_status and hw_reordering */
	if (!wil->use_compressed_rx_status || !wil->use_rx_hw_reordering) {
		wil_err(wil, "invalid config for IPA offload (compressed %d, hw_reorder %d)\n",
			wil->use_compressed_rx_status,
			wil->use_rx_hw_reordering);
		return -EINVAL;
	}

	if (wil->num_rx_status_rings != 1) {
		wil_err(wil, "invalid config for IPA offload (num_rx_status_rings %d)\n",
			wil->num_rx_status_rings);
		return -EINVAL;
	}

	/* hard coded sizes for IPA offload */
	desc_ring_size = WIL_IPA_DESC_RING_SIZE;
	status_ring_size = WIL_IPA_STATUS_RING_SIZE;
	wil->rx_buf_len = WIL_IPA_RX_BUFF_SIZE;

	wil_dbg_misc(wil,
		     "rx_alloc, desc_ring_size=%u, status_ring_size=%u, elem_size=%zu\n",
		     desc_ring_size, status_ring_size, elem_size);

	rc = wil_wmi_cfg_def_rx_offload(wil, wil->rx_buf_len, false);
	if (rc)
		return rc;

	/* Allocate status ring */
	sring_id = wil_find_free_sring(wil);
	if (sring_id < 0)
		return -EFAULT;
	wil->rx_sring_idx = sring_id;
	rc = wil_init_rx_sring(wil, status_ring_size, elem_size, sring_id);
	if (rc)
		return rc;

	/* Allocate descriptor ring */
	rc = wil_init_rx_desc_ring(wil, desc_ring_size, wil->rx_sring_idx);
	if (rc)
		goto err_free_status;

	/* Allocate contiguous memory for Rx buffers, zero initialized */
	ipa->rx_buf.sz = desc_ring_size * wil->rx_buf_len;
	ipa->rx_buf.va =
		dma_alloc_attrs(dev, ipa->rx_buf.sz, &ipa->rx_buf.pa,
				GFP_KERNEL | __GFP_ZERO,
				DMA_ATTR_FORCE_CONTIGUOUS);
	if (!ipa->rx_buf.va) {
		rc = -ENOMEM;
		goto err_free_desc;
	}

	/* Fill descriptor ring with credits */
	wil_ipa_rx_refill(wil);

	return 0;

err_free_desc:
	wil_ring_free_edma(wil, ring);
err_free_status:
	wil_sring_free(wil, &wil->srings[sring_id]);

	return rc;
}

static void wil_ipa_rx_free(struct wil_ipa *ipa)
{
	struct wil6210_priv *wil = (struct wil6210_priv *)ipa->wil;
	struct wil_ring *ring = &wil->ring_rx;

	wil_dbg_misc(wil, "ipa_rx_free\n");

	wil_ring_free_edma(wil, ring);
	wil_sring_free(wil, &wil->srings[wil->rx_sring_idx]);

	if (ipa->rx_buf.va) {
		dma_free_attrs(wil_to_dev(wil), ipa->rx_buf.sz,
			       ipa->rx_buf.va, ipa->rx_buf.pa,
			       DMA_ATTR_FORCE_CONTIGUOUS);
		ipa->rx_buf.va = NULL;
	}
}

int wil_ipa_start_ap(void *ipa_handle)
{
	struct wil_ipa *ipa = ipa_handle;
	struct wil6210_priv *wil = ipa->wil;
	struct pci_dev *pdev = wil->pdev;
	struct msi_msg msi_msg = {0};
	int rc;
	u16 msi_data;

	/* store original MSI info to be restored upon uninit */
	if (pci_read_config_dword(pdev, WIL_IPA_MSI_CAP + PCI_MSI_ADDRESS_LO,
				  &ipa->orig_msi_msg.address_lo) ||
	    pci_read_config_dword(pdev, WIL_IPA_MSI_CAP + PCI_MSI_ADDRESS_HI,
				  &ipa->orig_msi_msg.address_hi) ||
	    pci_read_config_word(pdev, WIL_IPA_MSI_CAP + PCI_MSI_DATA_64,
				 &msi_data)) {
		wil_err(wil, "fail to read MSI address\n");
		return -EINVAL;
	}
	ipa->orig_msi_msg.data = msi_data;

	rc = wil_ipa_rx_alloc(ipa);
	if (rc)
		return rc;

	if (ipa->smmu_enabled)
		rc = wil_ipa_wigig_conn_rx_pipe_smmu(ipa);
	else
		rc = wil_ipa_wigig_conn_rx_pipe(ipa);
	if (rc)
		goto rx_free;

	rc = ipa_wigig_set_perf_profile(WIL_MAX_BUS_REQUEST_KBPS * 8 / 1024);
	if (rc) {
		wil_err(wil, "ipa_wigig_set_perf_profile failed %d\n", rc);
		goto err_disconn;
	}

	rc = ipa_wigig_enable_pipe(ipa->rx_client_type);
	if (rc)
		goto err_disconn;

	rc = wil_ipa_wigig_reg_intf(ipa);
	if (rc)
		goto err_disable;

	wil_info(wil, "ipa intf registered. client %d\n", ipa->rx_client_type);

	/* route MSI to IPA uC */
	msi_msg.address_lo = lower_32_bits(ipa->uc_db_pa);
	msi_msg.address_hi = upper_32_bits(ipa->uc_db_pa);
	msi_msg.data = WIL_IPA_MSI_MSG_DATA;
	pci_write_msi_msg(pdev->irq, &msi_msg);

	return 0;

err_disable:
	ipa_wigig_disable_pipe(ipa->rx_client_type);
err_disconn:
	ipa_wigig_disconn_pipe(ipa->rx_client_type);
	ipa->rx_client_type = IPA_CLIENT_MAX;
rx_free:
	wil_ipa_rx_free(ipa);

	return rc;
}

static int wil_ipa_wigig_conn_client_smmu(struct wil_ipa *ipa, int cid,
					  int ring_id, int sring_id)
{
	struct wil6210_priv *wil = ipa->wil;
	struct device *dev = wil_to_dev(wil);
	struct ipa_wigig_conn_tx_in_params_smmu in = { { {0} } };
	struct ipa_wigig_conn_out_params out = {0};
	struct wil_ring *ring = &wil->ring_tx[ring_id];
	struct wil_status_ring *sring = &wil->srings[sring_id];
	struct wil_sta_info *sta = &wil->sta[cid];
	struct wil_ipa_conn *ipa_conn = &ipa->conn[cid];
	int rc, i;
	size_t sz;

	rc = dma_get_sgtable(dev, &in.pipe_smmu.desc_ring_base,
			     (void *)ring->va, ring->pa,
			     ring->size * sizeof(ring->va[0]));
	if (rc < 0) {
		wil_err(wil, "dma_get_sgtable for desc ring failed %d\n", rc);
		return rc;
	}

	in.pipe_smmu.desc_ring_base_iova = ring->pa;
	in.pipe_smmu.desc_ring_size = ring->size * sizeof(ring->va[0]);

	rc = dma_get_sgtable(dev, &in.pipe_smmu.status_ring_base,
			     sring->va, sring->pa,
			     sring->size * sring->elem_size);
	if (rc < 0) {
		wil_err(wil, "dma_get_sgtable for status ring failed %d\n", rc);
		return rc;
	}
	in.pipe_smmu.status_ring_base_iova = sring->pa;
	in.pipe_smmu.status_ring_size = sring->size * sring->elem_size;

	wil_ipa_calc_head_tail(wil, ring_id, sring_id,
			       &in.pipe_smmu.desc_ring_HWHEAD_pa,
			       &in.pipe_smmu.desc_ring_HWTAIL_pa,
			       &in.pipe_smmu.status_ring_HWHEAD_pa,
			       &in.pipe_smmu.status_ring_HWTAIL_pa);

	sz = ring->size * sizeof(in.dbuff_smmu.data_buffer_base[0]);
	in.dbuff_smmu.data_buffer_base = kzalloc(sz, GFP_KERNEL);
	if (!in.dbuff_smmu.data_buffer_base)
		return -ENOMEM;
	sz = ring->size * sizeof(in.dbuff_smmu.data_buffer_base_iova[0]);
	in.dbuff_smmu.data_buffer_base_iova = kzalloc(sz, GFP_KERNEL);
	if (!in.dbuff_smmu.data_buffer_base_iova) {
		rc = -ENOMEM;
		goto out;
	}

	in.dbuff_smmu.num_buffers = ring->size;
	for (i = 0; i < in.dbuff_smmu.num_buffers; i++) {
		struct wil_dma_map_info *tx_buf = &ipa_conn->tx_bufs_addr[i];

		rc = dma_get_sgtable(dev, &in.dbuff_smmu.data_buffer_base[i],
				     tx_buf->va, tx_buf->pa,
				     WIL_IPA_TX_BUFF_SIZE);
		if (rc < 0) {
			wil_err(wil, "sgtable tx buf %d failed %d\n", i, rc);
			goto out;
		}
		in.dbuff_smmu.data_buffer_base_iova[i] = tx_buf->pa;
	}
	in.dbuff_smmu.data_buffer_size = WIL_IPA_TX_BUFF_SIZE;

	in.int_gen_tx_bit_num = ring_id;
	ether_addr_copy(in.client_mac, sta->addr);

	rc = ipa_wigig_conn_client_smmu(&in, &out);
	if (rc) {
		wil_err(wil, "ipa_wigig_conn_client failed %d\n", rc);
		goto out;
	}

	ipa->conn[cid].ipa_client = out.client;

out:
	kfree(in.dbuff_smmu.data_buffer_base);
	kfree(in.dbuff_smmu.data_buffer_base_iova);

	return rc;
}

static int wil_ipa_wigig_conn_client(struct wil_ipa *ipa, int cid, int ring_id,
				     int sring_id)
{
	struct wil6210_priv *wil = ipa->wil;
	struct ipa_wigig_conn_tx_in_params in = { {0} };
	struct ipa_wigig_conn_out_params out = {0};
	struct wil_ring *ring = &wil->ring_tx[ring_id];
	struct wil_status_ring *sring = &wil->srings[sring_id];
	struct wil_sta_info *sta = &wil->sta[cid];
	int rc;

	in.pipe.desc_ring_base_pa = ring->pa;
	in.pipe.desc_ring_size = ring->size * sizeof(ring->va[0]);
	in.pipe.status_ring_base_pa = sring->pa;
	in.pipe.status_ring_size = sring->size * sring->elem_size;

	wil_ipa_calc_head_tail(wil, ring_id, sring_id,
			       &in.pipe.desc_ring_HWHEAD_pa,
			       &in.pipe.desc_ring_HWTAIL_pa,
			       &in.pipe.status_ring_HWHEAD_pa,
			       &in.pipe.status_ring_HWTAIL_pa);

	in.dbuff.data_buffer_size = WIL_IPA_TX_BUFF_SIZE;

	in.int_gen_tx_bit_num = ring_id;
	ether_addr_copy(in.client_mac, sta->addr);

	rc = ipa_wigig_conn_client(&in, &out);
	if (rc) {
		wil_err(wil, "ipa_wigig_conn_client failed %d\n", rc);
		return rc;
	}

	ipa->conn[cid].ipa_client = out.client;

	return 0;
}

static void wil_ipa_free_tx_bufs(struct wil_ipa *ipa, int cid)
{
	struct wil6210_priv *wil = ipa->wil;
	struct device *dev = wil_to_dev(wil);
	struct wil_ipa_conn *conn = &ipa->conn[cid];
	int i;

	for (i = 0; i < conn->tx_bufs_count; i++) {
		struct wil_dma_map_info *tx_buf_addr = &conn->tx_bufs_addr[i];

		if (tx_buf_addr->va)
			dma_free_coherent(dev, WIL_IPA_TX_BUFF_SIZE,
					  tx_buf_addr->va, tx_buf_addr->pa);
	}

	kfree(conn->tx_bufs_addr);
	conn->tx_bufs_addr = NULL;
}

static int wil_ipa_alloc_tx_bufs(struct wil_ipa *ipa, int cid, int ring_id)
{
	struct wil6210_priv *wil = ipa->wil;
	struct device *dev = wil_to_dev(wil);
	struct wil_ring *ring = &wil->ring_tx[ring_id];
	struct wil_ipa_conn *conn = &ipa->conn[cid];
	size_t sz;
	int rc, i;

	sz = ring->size * sizeof(conn->tx_bufs_addr[0]);
	conn->tx_bufs_addr = kzalloc(sz, GFP_KERNEL);
	if (!conn->tx_bufs_addr)
		return -ENOMEM;

	conn->tx_bufs_count = ring->size;
	for (i = 0; i < ring->size; i++) {
		struct wil_tx_enhanced_desc *d;

		/* note: IPA requires Tx buffers to be 256B aligned */
		conn->tx_bufs_addr[i].va =
			dma_alloc_coherent(dev, WIL_IPA_TX_BUFF_SIZE,
					   &conn->tx_bufs_addr[i].pa,
					   GFP_KERNEL | __GFP_ZERO);
		if (!conn->tx_bufs_addr[i].va) {
			wil_err(wil, "tx buf DMA alloc error (i %d)\n", i);
			rc = -ENOMEM;
			goto err;
		}

		d = (struct wil_tx_enhanced_desc *)&ring->va[i].tx.enhanced;
		wil_tx_desc_map_edma((union wil_tx_desc *)d,
				     conn->tx_bufs_addr[i].pa,
				     WIL_IPA_TX_BUFF_SIZE, ring_id);
		wil_tx_desc_set_nr_frags(&((union wil_tx_desc *)d)->legacy, 1);
	}

	return 0;

err:
	wil_ipa_free_tx_bufs(ipa, cid);

	return rc;
}

int wil_ipa_conn_client(void *ipa_handle, int cid, int ring_id, int sring_id)
{
	struct wil_ipa *ipa = (struct wil_ipa *)ipa_handle;
	struct wil6210_priv *wil = ipa->wil;
	struct wil_ipa_conn *ipa_conn;
	int rc;

	wil_info(wil, "ipa connect cid %d ring_id %d sring_id %d\n",
		 cid, ring_id, sring_id);

	rc = wil_ipa_alloc_tx_bufs(ipa, cid, ring_id);
	if (rc)
		return rc;

	if (ipa->smmu_enabled)
		rc = wil_ipa_wigig_conn_client_smmu(ipa, cid, ring_id,
						    sring_id);
	else
		rc = wil_ipa_wigig_conn_client(ipa, cid, ring_id, sring_id);
	if (rc)
		goto err_free;

	ipa_conn = &ipa->conn[cid];
	rc = ipa_wigig_enable_pipe(ipa_conn->ipa_client);
	if (rc)
		goto err_disconn;

	wil_dbg_misc(wil, "ipa pipe enabled (client %d)\n",
		     ipa_conn->ipa_client);

	return 0;

err_disconn:
	ipa_wigig_disconn_pipe(ipa_conn->ipa_client);
	ipa_conn->ipa_client = IPA_CLIENT_MAX;

err_free:
	wil_ipa_free_tx_bufs(ipa, cid);

	return rc;
}

static void wil_ipa_start_bcast_timer(struct wil_ipa *ipa)
{
	struct wil6210_priv *wil = ipa->wil;

	if (timer_pending(&ipa->bcast_timer))
		return;

	wil_dbg_txrx(wil, "ipa start bcast timer\n");

	mod_timer(&ipa->bcast_timer,
		  jiffies + msecs_to_jiffies(WIL_IPA_BCAST_POLL_MS));
}

int wil_ipa_tx(void *ipa_handle, struct wil_ring *ring, struct sk_buff *skb)
{
	struct wil_ipa *ipa = (struct wil_ipa *)ipa_handle;
	struct wil6210_priv *wil = ipa->wil;
	struct net_device *ndev = wil->main_ndev;
	struct wil_ipa_conn *ipa_conn;
	struct wil_net_stats *stats;
	int cid, rc;
	const u8 *da;
	unsigned int len = skb->len;

	wil_hex_dump_txrx("Tx ", DUMP_PREFIX_OFFSET, 16, 1,
			  skb->data, skb_headlen(skb), false);

	da = wil_skb_get_da(skb);
	if (is_multicast_ether_addr(da)) {
		wil_ipa_start_bcast_timer(ipa);
		/* let wil_start_xmit() continue handling this (mcast) packet */
		return -EPROTONOSUPPORT;
	}

	cid = wil_get_cid_by_ring(wil, ring);
	if (cid >= max_assoc_sta) {
		wil_dbg_txrx(wil, "ipa_tx invalid cid %d\n", cid);
		return -EINVAL;
	}

	ipa_conn = &ipa->conn[cid];

	rc = ipa_wigig_tx_dp(ipa_conn->ipa_client, skb);
	if (rc)
		return rc;
	/* skb could be freed after this point */

	stats = &wil->sta[cid].stats;
	stats->tx_packets++;
	stats->tx_bytes += len;
	ndev->stats.tx_packets++;
	ndev->stats.tx_bytes += len;

	return 0;
}

void wil_ipa_disconn_client(void *ipa_handle, int cid)
{
	struct wil_ipa *ipa = (struct wil_ipa *)ipa_handle;
	struct wil6210_priv *wil = ipa->wil;
	struct wil_ipa_conn *ipa_conn = &ipa->conn[cid];

	if (ipa_conn->ipa_client >= IPA_CLIENT_MAX)
		return;

	wil_info(wil, "ipa disconnect cid %d (client %d)\n",
		 cid, ipa_conn->ipa_client);

	ipa_wigig_disable_pipe(ipa_conn->ipa_client);
	ipa_wigig_disconn_pipe(ipa_conn->ipa_client);
	wil_ipa_free_tx_bufs(ipa, cid);

	ipa_conn->ipa_client = IPA_CLIENT_MAX;
}

static void wil_ipa_uc_ready_cb(void *priv)
{
	struct wil_ipa *ipa = (struct wil_ipa *)priv;
	struct wil6210_priv *wil = ipa->wil;

	wil_dbg_misc(wil, "got ipa uC ready cb\n");

	complete(&ipa->ipa_uc_ready_comp);
}

static void wil_ipa_wigig_misc_cb(void *priv)
{
	struct wil_ipa *ipa = (struct wil_ipa *)priv;
	struct wil6210_priv *wil = ipa->wil;

	wil_dbg_irq(wil, "got MISC callback from IPA\n");
	if (wil6210_irq_misc(0, wil) == IRQ_WAKE_THREAD)
		wil6210_irq_misc_thread(0, wil);
}

static int wil_ipa_wigig_init(struct wil_ipa *ipa)
{
	struct wil6210_priv *wil = ipa->wil;
	struct device *dev = wil_to_dev(wil);
	struct ipa_wigig_init_in_params in = {0};
	struct ipa_wigig_init_out_params out = {0};
	phys_addr_t bar_base_pa = wil_ipa_get_bar_base_pa(wil);
	int rc;
	u8 wil_smmu_en;

	ipa->domain = iommu_get_domain_for_dev(dev);
	if (!ipa->domain) {
		wil_err(wil, "iommu_get_domain failed\n");
		return -EINVAL;
	}

	init_completion(&ipa->ipa_uc_ready_comp);
	in.periph_baddr_pa = bar_base_pa;
	in.pseudo_cause_pa = bar_base_pa + HOSTADDR(RGF_DMA_PSEUDO_CAUSE);
	in.int_gen_tx_pa = bar_base_pa + HOSTADDR(RGF_INT_GEN_TX_ICR) +
			   offsetof(struct RGF_ICR, ICR);
	in.int_gen_rx_pa = bar_base_pa + HOSTADDR(RGF_INT_GEN_RX_ICR) +
			   offsetof(struct RGF_ICR, ICR);
	in.dma_ep_misc_pa = bar_base_pa + HOSTADDR(RGF_DMA_EP_MISC_ICR) +
			    offsetof(struct RGF_ICR, ICR);
	in.notify = wil_ipa_uc_ready_cb;
	in.int_notify = wil_ipa_wigig_misc_cb;
	in.priv = ipa;

	rc = ipa_wigig_init(&in, &out);
	if (rc) {
		wil_err(wil, "ipa_wigig_init failed %d\n", rc);
		return rc;
	}

	if (!out.is_uc_ready) {
		ulong to, left;

		to = msecs_to_jiffies(WIL_IPA_READY_TO_MS);
		left = wait_for_completion_timeout(&ipa->ipa_uc_ready_comp, to);
		if (left == 0) {
			wil_err(wil, "IPA uC ready timeout\n");
			rc = -ETIME;
			goto err;
		}
	}

	ipa->smmu_enabled = ipa_wigig_is_smmu_enabled();
	wil_smmu_en = test_bit(WIL_PLATFORM_CAPA_SMMU, wil->platform_capa);
	if (ipa->smmu_enabled != wil_smmu_en) {
		wil_err(wil, "smmu disagreement (ipa %d wil %d)\n",
			ipa->smmu_enabled, wil_smmu_en);
		rc = -EINVAL;
		goto err;
	}

	wil_info(wil, "IPA uC ready (early %d, uc_db_pa %pad, smmu %d)\n",
		 out.is_uc_ready, &out.uc_db_pa, ipa->smmu_enabled);

	if (ipa->smmu_enabled) {
		phys_addr_t pa;

		pa = rounddown(out.uc_db_pa, PAGE_SIZE);
		rc = iommu_map(ipa->domain, pa, pa, PAGE_SIZE,
			       IOMMU_READ | IOMMU_WRITE | IOMMU_MMIO);
		if (rc) {
			wil_err(wil, "iommu_map failed %d\n", rc);
			goto err;
		}
	}

	ipa->uc_db_pa = out.uc_db_pa;

	return 0;

err:
	if (ipa->smmu_enabled && ipa->uc_db_pa) {
		iommu_unmap(ipa->domain, ipa->uc_db_pa, PAGE_SIZE);
		ipa->uc_db_pa = 0;
	}
	ipa_wigig_cleanup();

	return rc;
}

static void wil_ipa_bcast_fn(struct timer_list *t)
{
	struct wil_ipa *ipa = from_timer(ipa, t, bcast_timer);
	struct wil6210_priv *wil = ipa->wil;

	wil_dbg_txrx(wil, "NAPI(Tx bcast) schedule\n");
	napi_schedule(&wil->napi_tx);
}

void *wil_ipa_init(struct wil6210_priv *wil)
{
	struct wil_ipa *ipa;
	int rc, i;

	wil_dbg_misc(wil, "wil_ipa init\n");

	if (wil->max_vifs > 1) {
		wil_err(wil, "IPA offload not supported with multi-VIF\n");
		return NULL;
	}

	if (!test_bit(WMI_FW_CAPABILITY_IPA, wil->fw_capabilities)) {
		wil_err(wil, "IPA offload not supported by FW\n");
		return NULL;
	}

	ipa = kzalloc(sizeof(*ipa), GFP_KERNEL);
	if (!ipa)
		return NULL;

	ipa->wil = wil;
	ipa->bcast_sring_id = WIL6210_MAX_STATUS_RINGS;
	ipa->rx_client_type = IPA_CLIENT_MAX;
	for (i = 0; i < WIL6210_MAX_CID; i++)
		ipa->conn[i].ipa_client = IPA_CLIENT_MAX;

	timer_setup(&ipa->bcast_timer, wil_ipa_bcast_fn, 0);

	rc = wil_ipa_wigig_init(ipa);
	if (rc)
		goto err;

	return ipa;

err:
	kfree(ipa);

	return NULL;
}

void wil_ipa_uninit(void *ipa_handle)
{
	struct wil_ipa *ipa = (struct wil_ipa *)ipa_handle;
	struct wil6210_priv *wil;
	struct device *dev;
	int i;

	if (!ipa_handle)
		return;

	wil = ipa->wil;
	dev = wil_to_dev(wil);
	wil_info(ipa->wil, "wil_ipa uninit\n");

	del_timer_sync(&ipa->bcast_timer);

	for (i = 0; i < WIL6210_MAX_CID; i++) {
		struct wil_ipa_conn *conn = &ipa->conn[i];

		if (conn->ipa_client < IPA_CLIENT_MAX) {
			ipa_wigig_disable_pipe(conn->ipa_client);
			ipa_wigig_disconn_pipe(conn->ipa_client);

			wil_ipa_free_tx_bufs(ipa, i);
		}
	}

	if (ipa->rx_client_type < IPA_CLIENT_MAX) {
		struct net_device *ndev = wil->main_ndev;
		struct pci_dev *pdev = wil->pdev;

		ipa_wigig_dereg_intf(ndev->name);
		ipa_wigig_disable_pipe(ipa->rx_client_type);
		ipa_wigig_disconn_pipe(ipa->rx_client_type);
		pci_write_msi_msg(pdev->irq, &ipa->orig_msi_msg);

		wil_ipa_rx_free(ipa);
	}

	if (ipa->smmu_enabled && ipa->uc_db_pa)
		iommu_unmap(ipa->domain, ipa->uc_db_pa, PAGE_SIZE);

	ipa_wigig_cleanup();
	kfree(ipa_handle);
}
