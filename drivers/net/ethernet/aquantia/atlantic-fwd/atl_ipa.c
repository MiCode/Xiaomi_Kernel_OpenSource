// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/pci.h>

#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/pm_wakeup.h>

#include <linux/ipa_eth.h>
#include <linux/etherdevice.h>
#include <linux/ipa.h>
#include <linux/msm_ipa.h>
#include <linux/if_vlan.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/msi.h>
#include <uapi/linux/ip.h>

#include "atl_fwd.h"
#include "atl_ipa.h"

static LIST_HEAD(aqo_devices);
static DEFINE_MUTEX(aqo_devices_lock);

#define AQC_IPA_INST_ID 0
#define AQC_IPA_NUM_OF_PIPES 2
/* Descriptor size is 16 Bytes */
#define AQC_IPA_DESC_SIZE 16
/* No of Tx descriptor registers */
#define AQC_IPA_NUM_TX_DESC	1024
/* No of Rx descriptor registers */
#define AQC_IPA_NUM_RX_DESC	1024
#define AQC_BAR_MMIO 0
#define TOTAL_DESC_SIZE (1024 * 16)

/* Minimum power-of-2 size to hold 1500 payload */
#define AQC_IPA_RX_BUF_SIZE 2048
#define AQC_IPA_TX_BUF_SIZE 2048

#define AQC_MIN_IPA_MITIGATION_TIME 64
#define AQC_MAX_IPA_MITIGATION_TIME 128

#define AQO_FLT_LOC_CATCH_ALL 40

#define AQC_RX_TAIL_PTR_OFFSET 0x00005B10
#define AQC_RX_TAIL_PTR(base, idx) \
	((base) + AQC_RX_TAIL_PTR_OFFSET + ((idx) * 0x20))

#define AQC_RX_HEAD_PTR_OFFSET 0x00005B0C
#define AQC_RX_HEAD_PTR(base, idx) \
	((base) + AQC_RX_HEAD_PTR_OFFSET + ((idx) * 0x20))

#define AQC_TX_TAIL_PTR_OFFSET 0x00007C10
#define AQC_TX_TAIL_PTR(base, idx) \
	((base) + AQC_TX_TAIL_PTR_OFFSET + ((idx) * 0x40))

#define AQC_TX_HEAD_PTR_OFFSET 0x00007C0C
#define AQC_TX_HEAD_PTR(base, idx) \
	((base) + AQC_TX_HEAD_PTR_OFFSET + ((idx) * 0x40))

static bool enable_ipa_offload = true;
static const struct pci_device_id *pci_device_ids;

#define to_eth_dev(aqo_dev) ((aqo_dev)->eth_dev)
#define to_ndev(aqo_dev) to_eth_dev(aqo_dev)->net_dev
#define to_dev(aqo_dev) to_eth_dev(aqo_dev)->dev

#define AQO_LOG_PREFIX "[aqo] "

struct aqc_regs {
	ktime_t begin_ktime;
	ktime_t end_ktime;
	u64 duration_ns;
};

enum aqc_channel_dir {
	AQC_CH_DIR_RX,
	AQC_CH_DIR_TX,
};

union aqc_ipa_eth_hdr {
	struct ethhdr l2;
	struct vlan_ethhdr vlan;
};

struct aqc_ch_info {
	struct atl_fwd_ring *ring;
	struct ipa_eth_client_pipe_info pipe_info;
};

struct aqo_device {
	struct list_head device_list;
	struct ipa_eth_device *eth_dev;

	struct ipa_eth_client eth_client;
	struct ipa_eth_intf_info intf;
	struct aqc_regs regs_save;

	struct aqc_ch_info rx_info;
	struct aqc_ch_info tx_info;
	union aqc_ipa_eth_hdr hdr_v4;
	union aqc_ipa_eth_hdr hdr_v6;

	struct wakeup_source *ws;
	phys_addr_t mmio_phys_addr;
	unsigned long long exception_packets;
};

static struct device *aqo_get_dev(struct aqo_device *aqo_dev)
{
	struct ipa_eth_device *eth_dev =
				aqo_dev ? aqo_dev->eth_dev : NULL;
	struct device *dev = eth_dev ? eth_dev->dev : NULL;

	return dev;
}

static const char *aqo_get_netdev_name(struct aqo_device *aqo_dev)
{
	struct ipa_eth_device *eth_dev =
				aqo_dev ? aqo_dev->eth_dev : NULL;
	struct net_device *net_dev =
				eth_dev ? eth_dev->net_dev : NULL;
	const char *netdev_name =
			net_dev ? net_dev->name : "<unpaired>";

	return netdev_name;
}

#define aqo_log(aqo_dev, fmt, args...) \
	do { \
		struct aqo_device *__aqo_dev = aqo_dev; \
		const char *__netdev_name = aqo_get_netdev_name(__aqo_dev); \
		\
		dev_dbg(aqo_get_dev(__aqo_dev), AQO_LOG_PREFIX "(%s) " fmt "\n", \
			__netdev_name, ## args); \
		ipa_eth_ipc_log(AQO_LOG_PREFIX "(%s) " fmt, \
				__netdev_name, ##args); \
	} while (0)

#define aqo_log_err(aqo_dev, fmt, args...) \
	do { \
		struct aqo_device *__aqo_dev = aqo_dev; \
		const char *__netdev_name = aqo_get_netdev_name(__aqo_dev); \
		\
		dev_err(aqo_get_dev(__aqo_dev), AQO_LOG_PREFIX "(%s) ERROR: " fmt "\n", \
			__netdev_name, ## args); \
		ipa_eth_ipc_log(AQO_LOG_PREFIX "(%s) ERROR: " fmt, \
				__netdev_name, ##args); \
	} while (0)

#ifdef DEBUG
#define aqo_log_bug(aqo_dev, fmt, args...) \
	do { \
		aqo_log_err(aqo_dev, "BUG: " fmt, ##args); \
		WARN_ON(); \
	} while (0)

#define aqo_log_dbg(aqo_dev, fmt, args...) \
	do { \
		struct aqo_device *__aqo_dev = aqo_dev; \
		const char *__netdev_name = aqo_get_netdev_name(__aqo_dev); \
		\
		dev_dbg(aqo_get_dev(__aqo_dev), AQO_LOG_PREFIX "(%s) " fmt "\n", \
			__netdev_name, ## args); \
		ipa_eth_ipc_dbg(AQO_LOG_PREFIX "(%s) DEBUG: " fmt, \
				__netdev_name, ##args); \
	} while (0)
#else
#define aqo_log_bug(aqo_dev, fmt, args...) \
	do { \
		aqo_log_err(aqo_dev, "BUG: " fmt, ##args); \
		dump_stack(); \
	} while (0)

#define aqo_log_dbg(aqo_dev, fmt, args...) \
	do { \
		const char *__netdev_name = aqo_get_netdev_name(aqo_dev); \
		\
		ipa_eth_ipc_dbg(AQO_LOG_PREFIX "(%s) DEBUG: " fmt, \
				__netdev_name, ##args); \
	} while (0)
#endif /* DEBUG */

static void __iomem *aqc_msix_bar(struct atl_nic *nic)
{
	struct pci_dev *pdev = nic->hw.pdev;
	struct msi_desc *msi;

	if (!pdev->msix_enabled)
		return NULL;

	msi = list_first_entry(dev_to_msi_list(&pdev->dev),
			       struct msi_desc, list);
	return msi->mask_base;
}

static void aqc_log_msix(struct atl_nic *nic)
{
	int i;
	void __iomem *desc = aqc_msix_bar(nic);

	if (!desc)
		return;

	for (i = 0; i < 32; i++) {
		void __iomem *msix = desc + (i * PCI_MSIX_ENTRY_SIZE);

		aqo_log(NULL, "MSI[%d]: %x %x %x %x",
			i,
			readl_relaxed(msix + PCI_MSIX_ENTRY_UPPER_ADDR),
			readl_relaxed(msix + PCI_MSIX_ENTRY_LOWER_ADDR),
			readl_relaxed(msix + PCI_MSIX_ENTRY_DATA),
			readl_relaxed(msix + PCI_MSIX_ENTRY_VECTOR_CTRL)
			);
	}
}

static void *atl_ipa_alloc_descs(struct device *dev, size_t size,
				 dma_addr_t *daddr, gfp_t gfp,
				 struct atl_fwd_mem_ops *ops)
{
	return dma_alloc_coherent(dev, size, daddr, gfp);
}

static void *atl_ipa_alloc_buf(struct device *dev, size_t size,
			       dma_addr_t *daddr, gfp_t gfp,
			       struct atl_fwd_mem_ops *ops)
{
	return dma_alloc_coherent(dev, size, daddr, gfp);
}

static void atl_ipa_free_descs(void *buf, struct device *dev, size_t size,
			       dma_addr_t daddr, struct atl_fwd_mem_ops *ops)
{
	return dma_free_coherent(dev, size, buf, daddr);
}

static void atl_ipa_free_buf(void *buf, struct device *dev, size_t size,
			     dma_addr_t daddr, struct atl_fwd_mem_ops *ops)
{
	return dma_free_coherent(dev, size, buf, daddr);
}

static void aqc_ipa_notify_cb(void *priv,
			      enum ipa_dp_evt_type evt,
			      unsigned long data)
{
	struct aqo_device *aqo_dev = priv;
	struct sk_buff *skb = (struct sk_buff *)data;
	struct net_device *net_dev = to_ndev(aqo_dev);

	if (evt != IPA_RECEIVE)
		return;

	if (!aqo_dev || !skb) {
		aqo_log_err(aqo_dev, "Null Param: pdata %p, skb %p",
			    aqo_dev, skb);
		return;
	}

	skb->protocol = eth_type_trans(skb, net_dev);
	aqo_dev->exception_packets++;

	netif_rx_ni(skb);
}

static int aqc_match_pci(struct device *dev)
{
	struct pci_dev *pci_dev = container_of(dev, struct pci_dev, dev);

	if (dev->bus != &pci_bus_type) {
		aqo_log(NULL, "Device bus type is not PCI");
		return -EINVAL;
	}

	if (!pci_match_id(pci_device_ids, pci_dev)) {
		aqo_log(NULL, "Device PCI ID is not compatible");
		return -ENODEV;
	}

	return 0;
}

static int aqo_pair(struct ipa_eth_device *eth_dev)
{
	int rc = 0;
	struct device *dev = eth_dev->dev;
	struct aqo_device *aqo_dev = eth_dev->od_priv;

	aqo_log(NULL, "Pairing started");

	if (!eth_dev || !dev) {
		aqo_log_err(NULL, "Invalid ethernet device structure");
		return -EFAULT;
	}

	rc = aqc_match_pci(dev);
	if (rc) {
		aqo_log(NULL, "Failed to parse device");
		goto err_parse;
	}

	aqo_dev->mmio_phys_addr = pci_resource_start(to_pci_dev(dev),
						     AQC_BAR_MMIO);

	aqo_log(aqo_dev, "Successfully paired new device");

	return 0;

err_parse:
	return rc;
}

static void aqo_unpair(struct ipa_eth_device *eth_dev)
{
	struct aqo_device *aqo_dev = eth_dev->od_priv;

	aqo_log(aqo_dev, "Successfully unpaired");
}

static void aqc_deinit_pipe_info(struct ipa_eth_client_pipe_info *pipe_info)
{
	kfree(pipe_info->info.data_buff_list);
	sg_free_table(pipe_info->info.transfer_ring_sgt);
	kfree(pipe_info->info.transfer_ring_sgt);
	memset(pipe_info, 0, sizeof(*pipe_info));
}

static int aqc_init_pipe_info(struct aqo_device *aqo_dev,
			      struct aqc_ch_info *ch_info)
{
	struct device *dev = to_dev(aqo_dev);
	struct atl_fwd_ring *aqc_ring = ch_info->ring;
	struct ipa_eth_client_pipe_info *pipe_info = &ch_info->pipe_info;
	struct ipa_eth_aqc_setup_info *aqc_info =
					&pipe_info->info.client_info.aqc;

	int dir_tx = !!(aqc_ring->flags & ATL_FWR_TX);
	int ret, i;

	INIT_LIST_HEAD(&pipe_info->link);
	if (dir_tx)
		pipe_info->dir = IPA_ETH_PIPE_DIR_TX;
	else
		pipe_info->dir = IPA_ETH_PIPE_DIR_RX;

	pipe_info->client_info = &aqo_dev->eth_client;
	pipe_info->info.is_transfer_ring_valid = true;
	pipe_info->info.transfer_ring_base = (dma_addr_t)aqc_ring->hw.daddr;
	pipe_info->info.transfer_ring_size = TOTAL_DESC_SIZE;
	pipe_info->info.fix_buffer_size = aqc_ring->buf_size;
	pipe_info->info.transfer_ring_sgt =
		kzalloc(sizeof(pipe_info->info.transfer_ring_sgt), GFP_KERNEL);

	if (!pipe_info->info.transfer_ring_sgt) {
		aqo_log_err(aqo_dev,
			    "Failed to alloc transfer ring sgt buffer");
		goto ring_sgt_alloc_err;
	}

	ret = dma_get_sgtable(dev, pipe_info->info.transfer_ring_sgt,
			      aqc_ring->hw.descs,
			      aqc_ring->hw.daddr,
			TOTAL_DESC_SIZE);
	if (ret)
		goto sgtable_err;

	pipe_info->info.data_buff_list_size = aqc_ring->hw.size;
	pipe_info->info.data_buff_list =
		kcalloc(aqc_ring->hw.size, sizeof(*pipe_info->info.data_buff_list),
			GFP_KERNEL);

	if (!pipe_info->info.data_buff_list) {
		aqo_log_err(aqo_dev, "Failed to alloc data buff list");
		goto buff_alloc_err;
	}

	for (i = 0; i < aqc_ring->hw.size; i++) {
		void *vaddr = (void *)aqc_ring->bufs->vaddr_vec;

		pipe_info->info.data_buff_list[i].iova =
			aqc_ring->bufs->daddr_vec_base + (AQC_IPA_RX_BUF_SIZE * i);
		pipe_info->info.data_buff_list[i].pa =
			page_to_phys(vmalloc_to_page(vaddr + (AQC_IPA_RX_BUF_SIZE * i))) |
				((phys_addr_t)(vaddr + (AQC_IPA_RX_BUF_SIZE * i)) & ~PAGE_MASK);
	}

	aqc_info->bar_addr = aqo_dev->mmio_phys_addr;
	if (dir_tx) {
		aqc_info->head_ptr_offs = AQC_TX_HEAD_PTR(0, aqc_ring->idx);
		aqc_info->dest_tail_ptr_offs = AQC_TX_TAIL_PTR(0, aqc_ring->idx);
	} else {
		aqc_info->head_ptr_offs = AQC_RX_HEAD_PTR(0, aqc_ring->idx);
		aqc_info->dest_tail_ptr_offs = AQC_RX_TAIL_PTR(0, aqc_ring->idx);
		pipe_info->info.notify = aqc_ipa_notify_cb;
		pipe_info->info.priv = aqo_dev;
	}
	aqc_info->aqc_ch = aqc_ring->idx;

	return 0;

buff_alloc_err:
	sg_free_table(pipe_info->info.transfer_ring_sgt);
sgtable_err:
	kfree(pipe_info->info.transfer_ring_sgt);
ring_sgt_alloc_err:
	memset(pipe_info, 0, sizeof(*pipe_info));
	return -EINVAL;
}

static void aqc_ipa_init_intf_info_eth_hdr(struct aqo_device *aqo_dev)
{
	struct ipa_eth_intf_info *intf_info = &aqo_dev->intf;
	struct net_device *net_dev = to_ndev(aqo_dev);

	memcpy(&aqo_dev->hdr_v4.l2.h_source, net_dev->dev_addr, ETH_ALEN);
	aqo_dev->hdr_v4.l2.h_proto = htons(ETH_P_IP);
	intf_info->hdr[0].hdr = (u8 *)&aqo_dev->hdr_v4.l2;
	intf_info->hdr[0].hdr_len = ETH_HLEN;
	intf_info->hdr[0].hdr_type = IPA_HDR_L2_ETHERNET_II;

	memcpy(&aqo_dev->hdr_v6.l2.h_source, net_dev->dev_addr, ETH_ALEN);
	aqo_dev->hdr_v6.l2.h_proto = htons(ETH_P_IPV6);
	intf_info->hdr[1].hdr = (u8 *)&aqo_dev->hdr_v6.l2;
	intf_info->hdr[1].hdr_len = ETH_HLEN;
	intf_info->hdr[1].hdr_type = IPA_HDR_L2_ETHERNET_II;
}

static void aqc_ipa_init_intf_info_vlan_hdr(struct aqo_device *aqo_dev)
{
	struct ipa_eth_intf_info *intf_info = &aqo_dev->intf;
	struct net_device *net_dev = to_ndev(aqo_dev);

	memcpy(&aqo_dev->hdr_v4.vlan.h_source, net_dev->dev_addr, ETH_ALEN);
	aqo_dev->hdr_v4.vlan.h_vlan_proto = htons(ETH_P_8021Q);
	aqo_dev->hdr_v4.vlan.h_vlan_encapsulated_proto = htons(ETH_P_IP);
	intf_info->hdr[0].hdr = (u8 *)&aqo_dev->hdr_v4.vlan;
	intf_info->hdr[0].hdr_len = VLAN_ETH_HLEN;
	intf_info->hdr[0].hdr_type = IPA_HDR_L2_802_1Q;

	memcpy(&aqo_dev->hdr_v6.vlan.h_source, net_dev->dev_addr, ETH_ALEN);
	aqo_dev->hdr_v6.vlan.h_vlan_proto = htons(ETH_P_8021Q);
	aqo_dev->hdr_v6.vlan.h_vlan_encapsulated_proto = htons(ETH_P_IPV6);
	intf_info->hdr[1].hdr = (u8 *)&aqo_dev->hdr_v6.vlan;
	intf_info->hdr[1].hdr_len = VLAN_ETH_HLEN;
	intf_info->hdr[1].hdr_type = IPA_HDR_L2_802_1Q;
}

static void aqc_ipa_deinit_intf_info_hdrs(struct ipa_eth_intf_info *intf_info)
{
	memset(&intf_info->hdr[1].hdr, 0, sizeof(struct ipa_eth_hdr_info));
	memset(&intf_info->hdr[0].hdr, 0, sizeof(struct ipa_eth_hdr_info));
}

static int aqc_ipa_init_intf_info_hdrs(struct aqo_device *aqo_dev)
{
	bool ipa_vlan_mode = false;

	if (ipa_is_vlan_mode(IPA_VLAN_IF_EMAC, &ipa_vlan_mode)) {
		aqo_log_err(aqo_dev, "Failed to get vlan mode");
		return -EINVAL;
	}

	if (ipa_vlan_mode)
		aqc_ipa_init_intf_info_vlan_hdr(aqo_dev);
	else
		aqc_ipa_init_intf_info_eth_hdr(aqo_dev);

	return 0;
}

static int aqc_send_ipa_ecm_msg(struct aqo_device *aqo_dev,
				bool connected)
{
	struct ipa_ecm_msg msg;
	struct net_device *net_dev = to_ndev(aqo_dev);

	strlcpy(msg.name, net_dev->name, sizeof(msg.name));
	msg.ifindex = net_dev->ifindex;

	return connected ?
		 ipa_eth_client_conn_evt(&msg) :
		 ipa_eth_client_disconn_evt(&msg);
}

static void aqc_teardown_ring(struct aqo_device *aqo_dev,
			      struct aqc_ch_info *ch_info)
{
	struct atl_fwd_mem_ops *mem_ops = ch_info->ring->mem_ops;

	atl_fwd_disable_ring(ch_info->ring);
	aqc_deinit_pipe_info(&ch_info->pipe_info);
	atl_fwd_release_ring(ch_info->ring);
	kzfree(mem_ops);
	ch_info->ring = NULL;
}

static int aqc_setup_ring(struct aqo_device *aqo_dev, int ring_flags,
			  unsigned int ring_size,
			  unsigned int buff_size,
			  struct aqc_ch_info *ch_info)
{
	struct net_device *ndev = to_ndev(aqo_dev);
	struct atl_fwd_mem_ops *mem_ops = NULL;

	mem_ops = kzalloc(sizeof(*mem_ops), GFP_KERNEL);
	if (!mem_ops)
		return -ENOMEM;

	mem_ops->alloc_descs = atl_ipa_alloc_descs;
	mem_ops->alloc_buf = atl_ipa_alloc_buf;
	mem_ops->free_descs = atl_ipa_free_descs;
	mem_ops->free_buf = atl_ipa_free_buf;

	ch_info->ring = atl_fwd_request_ring(ndev, ring_flags, AQC_IPA_NUM_RX_DESC,
					     AQC_IPA_RX_BUF_SIZE, 1, mem_ops);

	if (!ch_info->ring) {
		aqo_log_err(aqo_dev, "Request ring failed");
		goto err_req_ring;
	}
	if (aqc_init_pipe_info(aqo_dev, ch_info)) {
		aqo_log_err(aqo_dev, "Failed to allocate pipe info");
		goto err_init_pipe_info;
	}

	return 0;

err_init_pipe_info:
	atl_fwd_release_ring(ch_info->ring);
	ch_info->ring = NULL;
err_req_ring:
	return -EINVAL;
}

static void aqc_teardown_rings(struct aqo_device *aqo_dev)
{
	aqo_log_dbg(aqo_dev, "Tearing down rx and tx rings");

	aqc_teardown_ring(aqo_dev, &aqo_dev->rx_info);
	aqc_teardown_ring(aqo_dev, &aqo_dev->tx_info);
}

static int setup_aqc_rings(struct aqo_device *aqo_dev)
{
	int ret = 0;
	enum atl_fwd_ring_flags ring_flags = 0;

	ring_flags |= ATL_FWR_ALLOC_BUFS;
	ring_flags |= ATL_FWR_CONTIG_BUFS;

	aqo_log_dbg(aqo_dev, "Setting rx and tx rings");

	if (aqc_setup_ring(aqo_dev, ring_flags,
			   AQC_IPA_NUM_RX_DESC,
			   AQC_IPA_RX_BUF_SIZE,
			   &aqo_dev->rx_info)) {
		aqo_log_err(aqo_dev, "Failed to setup rx ring");
		goto err;
	}

	ring_flags |= ATL_FWR_TX;

	if (aqc_setup_ring(aqo_dev, ring_flags,
			   AQC_IPA_NUM_TX_DESC,
			   AQC_IPA_TX_BUF_SIZE,
			   &aqo_dev->tx_info)) {
		aqo_log_err(aqo_dev, "Failed to setup tx ring");
		goto err_tx_ring;
	}

	return ret;
err_tx_ring:
	aqc_teardown_ring(aqo_dev, &aqo_dev->rx_info);
err:
	ret = -1;
	return ret;
}

static void aqc_teardown_event(struct aqo_device *aqo_dev,
			       struct aqc_ch_info *ch_info)
{
	atl_fwd_disable_event(ch_info->ring->evt);
	atl_fwd_release_event(ch_info->ring->evt);
}

static int aqc_setup_event(struct aqo_device *aqo_dev,
			   enum aqc_channel_dir direction)
{
	struct atl_fwd_event *event = NULL;
	struct atl_fwd_event atl_event = {0};
	struct device *dev = to_dev(aqo_dev);
	struct aqc_ch_info *ch_info;
	int ret = 0;

	if (direction == AQC_CH_DIR_RX) {
		ch_info = &aqo_dev->rx_info;

		atl_event.msi_addr = dma_map_resource(dev, ch_info->pipe_info.info.db_pa,
						      sizeof(u32), DMA_FROM_DEVICE, 0);

		if (dma_mapping_error(dev, atl_event.msi_addr)) {
			aqo_log_err(aqo_dev, "DMA mapping error for IPA DB");
			return -ENOMEM;
	}

		aqo_log_dbg(aqo_dev, "phy db-addr = %pap, dma addr = %pad",
			    &ch_info->pipe_info.info.db_pa, &atl_event.msi_addr);

		atl_event.msi_data = (u32)ch_info->pipe_info.info.db_val;
	} else if (direction == AQC_CH_DIR_TX) {
		ch_info = &aqo_dev->tx_info;
		atl_event.flags = ATL_FWD_EVT_TXWB;

		atl_event.tx_head_wrb = dma_map_resource(dev, ch_info->pipe_info.info.db_pa,
							 sizeof(u32), DMA_FROM_DEVICE, 0);

		if (dma_mapping_error(dev, atl_event.tx_head_wrb)) {
			aqo_log_err(aqo_dev, "DMA mapping error for IPA DB");
			return -ENOMEM;
		}

		aqo_log_dbg(aqo_dev, "phy db-addr = %pap, dma addr = %pad",
			    &ch_info->pipe_info.info.db_pa, &atl_event.tx_head_wrb);
	} else {
		aqo_log_err(aqo_dev, "Unknown data path direction");
		return -EINVAL;
	}

	event = kzalloc(sizeof(*event), GFP_KERNEL);
	if (!event)
		return -ENOMEM;

	*event = atl_event;
	event->ring = ch_info->ring;

	ret = atl_fwd_request_event(event);
	if (ret) {
		aqo_log_err(aqo_dev, "request event failed");
		goto err_req_event;
	}

	if (direction == AQC_CH_DIR_RX) {
		if (atl_fwd_set_ring_intr_mod(ch_info->ring, AQC_MIN_IPA_MITIGATION_TIME,
					      AQC_MAX_IPA_MITIGATION_TIME)) {
			aqo_log_err(aqo_dev, "set_ring_intr_mod rx : Failed");
			goto err_event;
		}
	}

	if (atl_fwd_enable_event(event)) {
		aqo_log_err(aqo_dev, "enable event failed");
		goto err_event;
	}

	if (atl_fwd_enable_ring(ch_info->ring)) {
		aqo_log_err(aqo_dev, "enable ring Failed");
		goto err_enable_ring;
	}

	return ret;

err_enable_ring:
	atl_fwd_disable_event(event);
err_event:
	atl_fwd_release_event(event);
err_req_event:
	return -EINVAL;
}

static int aqc_vote_ipa_bw(struct aqo_device *aqo_dev)
{
	struct ipa_eth_perf_profile profile;

	aqo_log_dbg(aqo_dev, "Voting IPA bandwidth");

	memset(&profile, 0, sizeof(profile));
	profile.max_supported_bw_mbps = SPEED_10000;

	if (ipa_eth_client_set_perf_profile(&aqo_dev->eth_client, &profile)) {
		aqo_log_err(aqo_dev, "Failed to set voting on bandwidth");
		return -EINVAL;
	}

	return 0;
}

static void aqc_del_client_pipe_list(struct aqo_device *aqo_dev)
{
	struct ipa_eth_client *eth_client = &aqo_dev->eth_client;
	struct ipa_eth_client_pipe_info *pipe_info, *tmp;

	list_for_each_entry_safe(pipe_info, tmp, &eth_client->pipe_list, link)
		list_del(&pipe_info->link);
}

static void aqc_teardown_ipa_pipes(struct aqo_device *aqo_dev)
{
	if (ipa_eth_client_disconn_pipes(&aqo_dev->eth_client))
		aqo_log_err(aqo_dev, "ipa_eth_client_disconn_pipes Failed");

	aqc_del_client_pipe_list(aqo_dev);
}

static int setup_aqc_ipa_pipes(struct aqo_device *aqo_dev)
{
	struct ipa_eth_client_pipe_info *rx_pipe_info =
						&aqo_dev->rx_info.pipe_info;
	struct ipa_eth_client_pipe_info *tx_pipe_info =
						&aqo_dev->tx_info.pipe_info;

	aqo_log_dbg(aqo_dev, "Setting up IPA pipes");

	INIT_LIST_HEAD(&aqo_dev->eth_client.pipe_list);
	list_add(&rx_pipe_info->link, &aqo_dev->eth_client.pipe_list);
	list_add(&tx_pipe_info->link, &aqo_dev->eth_client.pipe_list);

	if (ipa_eth_client_conn_pipes(&aqo_dev->eth_client) != 0) {
		aqo_log_err(aqo_dev, "IPA connection pipe error");
		aqc_del_client_pipe_list(aqo_dev);
		return -EINVAL;
	}

	if (aqc_vote_ipa_bw(aqo_dev))
		goto err_vote_bw;

	return 0;

err_vote_bw:
	aqc_teardown_rings(aqo_dev);
	return -EINVAL;
}

static void aqc_teardown_ipa_intf(struct aqo_device *aqo_dev)
{
	struct ipa_eth_intf_info *intf_info = &aqo_dev->intf;

	if (aqc_send_ipa_ecm_msg(aqo_dev, false))
		aqo_log_err(aqo_dev, "Failed to send ecm");

	if (ipa_eth_client_unreg_intf(intf_info))
		aqo_log_err(aqo_dev, "ipa_eth_client_unreg_intf Failed");

	kfree(intf_info->pipe_hdl_list);
	intf_info->pipe_hdl_list = NULL;

	aqc_ipa_deinit_intf_info_hdrs(intf_info);
	memset(intf_info, 0, sizeof(*intf_info));
}

static int setup_ipa_intf(struct aqo_device *aqo_dev)
{
	struct net_device *net_dev = to_ndev(aqo_dev);
	struct ipa_eth_intf_info *intf_info = &aqo_dev->intf;

	intf_info->netdev_name = net_dev->name;
	if (aqc_ipa_init_intf_info_hdrs(aqo_dev))
		goto err_fill_hdrs;

	intf_info->pipe_hdl_list =
		kcalloc(AQC_IPA_NUM_OF_PIPES,
			sizeof(*intf_info->pipe_hdl_list), GFP_KERNEL);
	if (!intf_info->pipe_hdl_list) {
		aqo_log_err(aqo_dev, "Failed to alloc pipe handle list");
		goto err_alloc_pipe_hndls;
	}

	intf_info->pipe_hdl_list[0] = aqo_dev->rx_info.pipe_info.pipe_hdl;
	intf_info->pipe_hdl_list[1] = aqo_dev->tx_info.pipe_info.pipe_hdl;
	intf_info->pipe_hdl_list_size = AQC_IPA_NUM_OF_PIPES;

	if (ipa_eth_client_reg_intf(intf_info)) {
		aqo_log_err(aqo_dev, "Failed to register IPA interface");
		goto err_reg_intf;
	}

	if (aqc_send_ipa_ecm_msg(aqo_dev, true)) {
		aqo_log_err(aqo_dev, "Failed to send ecm");
		goto err_ecm_conn;
	}

	return 0;

err_ecm_conn:
	ipa_eth_client_unreg_intf(intf_info);
err_reg_intf:
	kfree(intf_info->pipe_hdl_list);
	intf_info->pipe_hdl_list = NULL;
err_alloc_pipe_hndls:
	aqc_ipa_deinit_intf_info_hdrs(intf_info);
err_fill_hdrs:
	memset(intf_info, 0, sizeof(*intf_info));
	return -EINVAL;
}

static void aqc_teardown_events(struct aqo_device *aqo_dev)
{
	aqo_log_dbg(aqo_dev, "Tearing down rx and tx events");

	aqc_teardown_event(aqo_dev, &aqo_dev->rx_info);
	aqc_teardown_event(aqo_dev, &aqo_dev->tx_info);
}

static int setup_aqc_events(struct aqo_device *aqo_dev)
{
	int ret = 0;

	aqo_log_dbg(aqo_dev, "Setting rx and tx events");

	if (aqc_setup_event(aqo_dev, AQC_CH_DIR_RX)) {
		aqo_log_err(aqo_dev, "Failed to setup rx event");
		goto err_rx_event;
	}

	if (aqc_setup_event(aqo_dev, AQC_CH_DIR_TX)) {
		aqo_log_err(aqo_dev, "Failed to setup tx event");
		goto err_tx_event;
	}

	return ret;

err_tx_event:
	aqc_teardown_event(aqo_dev, &aqo_dev->rx_info);
err_rx_event:
	return -EINVAL;
}

static int aqo_init_tx(struct ipa_eth_device *eth_dev)
{
	return 0;
}

/**
 * __config_catchall_filter() - Installs or removes the catch-all AQC Rx filter
 *
 * @aqo_dev: aqo_device pointer
 * @insert: If true, inserts rule at %AQO_FLT_LOC_CATCH_ALL. Otherwise removes
 *          any filter installed at the same location.
 *
 * Configure the catch-all filter on AQC NIC. When installed, the catch-all
 * filter directs all incoming traffic to Rx queue associated with offload path.
 * Catch-all filter in Aquantia is implemented using Flex Filter that matches
 * the packet with zero bitmask (effectively matching any packet) and the filter
 * is applied on any packet that is not already matched by other filters (except
 * RSS filter).
 *
 * Returns 0 on success, non-zero otherwise.
 */
static int __config_catchall_filter(struct aqo_device *aqo_dev, bool insert)
{
	struct ethtool_rxnfc rxnfc;
	struct net_device *net_dev = to_ndev(aqo_dev);

	if (!net_dev) {
		aqo_log_err(aqo_dev, "Net device information is missing");
		return -EFAULT;
	}

	if (!net_dev->ethtool_ops || !net_dev->ethtool_ops->set_rxnfc) {
		aqo_log_err(aqo_dev,
			    "set_rxnfc is not supported by the network driver");
		return -EFAULT;
	}

	memset(&rxnfc, 0, sizeof(rxnfc));

	rxnfc.cmd = insert ? ETHTOOL_SRXCLSRLINS : ETHTOOL_SRXCLSRLDEL;

	rxnfc.fs.ring_cookie = aqo_dev->rx_info.ring->idx;
	rxnfc.fs.location = AQO_FLT_LOC_CATCH_ALL;

	return net_dev->ethtool_ops->set_rxnfc(net_dev, &rxnfc);
}

int aqo_netdev_rxflow_set(struct aqo_device *aqo_dev)
{
	int rc = __config_catchall_filter(aqo_dev, true);

	if (rc)
		aqo_log_err(aqo_dev, "Failed to install catch-all filter");
	else
		aqo_log(aqo_dev, "Installed Rx catch-all filter");

	return rc;
}

int aqo_netdev_rxflow_reset(struct aqo_device *aqo_dev)
{
	int rc = __config_catchall_filter(aqo_dev, false);

	if (rc)
		aqo_log_err(aqo_dev, "Failed to remove catch-all filter");
	else
		aqo_log(aqo_dev, "Removed Rx catch-all filter");

	return rc;
}

static int aqc_start_tx(struct ipa_eth_device *eth_dev)
{
	struct aqo_device *aqo_dev = eth_dev->od_priv;
	struct atl_nic *nic = (struct atl_nic *)dev_get_drvdata(eth_dev->dev);

	if (setup_aqc_rings(aqo_dev)) {
		aqo_log_err(aqo_dev, "setup_aqc_rings Failed");
		goto err_aqc_rings;
	}

	if (setup_aqc_ipa_pipes(aqo_dev)) {
		aqo_log_err(aqo_dev, "setup_aqc_ipa_pipes Failed");
		goto err_ipa_conn;
		}

	if (setup_aqc_events(aqo_dev)) {
		aqo_log_err(aqo_dev, "setup_aqc_events Failed");
		goto err_aqc_events;
	}

	if (setup_ipa_intf(aqo_dev)) {
		aqo_log_err(aqo_dev, "setup_ipa_intf Failed");
		goto err_ipa_intf;
		}

	if (aqo_netdev_rxflow_set(aqo_dev)) {
		aqo_log_err(aqo_dev, "Failed to set Rx flow to IPA");
		goto err_rxflow;
	}

	aqc_log_msix(nic);

	return 0;

err_rxflow:
	aqc_teardown_ipa_intf(aqo_dev);
err_ipa_intf:
	aqc_teardown_events(aqo_dev);
err_aqc_events:
	aqc_teardown_ipa_pipes(aqo_dev);
err_ipa_conn:
	aqc_teardown_rings(aqo_dev);
err_aqc_rings:
	return -EINVAL;
}

static int aqc_stop_tx(struct ipa_eth_device *eth_dev)
{
	struct aqo_device *aqo_dev = eth_dev->od_priv;

	aqo_log(aqo_dev, "Stopping IPA offload");

	aqo_netdev_rxflow_reset(aqo_dev);
	aqc_teardown_ipa_intf(aqo_dev);
	aqc_teardown_events(aqo_dev);
	aqc_teardown_ipa_pipes(aqo_dev);
	aqc_teardown_rings(aqo_dev);

	return 0;
}

static int aqo_deinit_tx(struct ipa_eth_device *eth_dev)
{
	return 0;
}

static int aqo_init_rx(struct ipa_eth_device *eth_dev)
{
	return 0;
}

static int aqo_start_rx(struct ipa_eth_device *eth_dev)
{
	return 0;
}

static int aqo_stop_rx(struct ipa_eth_device *eth_dev)
{
	return 0;
}

static int aqo_deinit_rx(struct ipa_eth_device *eth_dev)
{
	return 0;
}

static int aqo_get_stats(struct ipa_eth_device *eth_dev,
			 struct ipa_eth_offload_stats *stats)
{
	memset(&stats, 0, sizeof(stats));

	return 0;
}

static int aqo_clear_stats(struct ipa_eth_device *eth_dev)
{
	return 0;
}

static int aqo_save_regs(struct ipa_eth_device *eth_dev,
			 void **regs, size_t *size)
{
	return 0;
}

static int atl_ipa_open_device(struct ipa_eth_device *eth_dev)
{
	struct aqo_device *aqo_dev;
	struct atl_nic *nic = (struct atl_nic *)dev_get_drvdata(eth_dev->dev);

	if (!eth_dev || !eth_dev->dev) {
		pr_err("%s: Invalid ethernet device structure\n", __func__);
		return -EFAULT;
	}

	if (!nic || !nic->ndev) {
		dev_err(eth_dev->dev, "Invalid atl_nic\n");
		return -ENODEV;
	}

	aqo_dev = kzalloc(sizeof(*aqo_dev), GFP_KERNEL);
	if (!aqo_dev)
		return -ENOMEM;

	aqo_dev->ws = wakeup_source_register(eth_dev->dev, "aqc-ipa");
	if (!aqo_dev->ws) {
		aqo_log_err(aqo_dev, "Error in initializing wake up source\n");
		kfree(aqo_dev);
		return -ENOMEM;
	}

	aqo_dev->eth_client.inst_id = AQC_IPA_INST_ID;
	aqo_dev->eth_client.client_type = IPA_ETH_CLIENT_AQC107;
	aqo_dev->eth_client.traffic_type = IPA_ETH_PIPE_BEST_EFFORT;
	aqo_dev->eth_client.priv = aqo_dev;

	eth_dev->od_priv = aqo_dev;
	eth_dev->net_dev = nic->ndev;
	aqo_dev->eth_dev = eth_dev;

	__pm_stay_awake(aqo_dev->ws);

	return 0;
}

static void atl_ipa_close_device(struct ipa_eth_device *eth_dev)
{
	struct aqo_device *aqo_dev = eth_dev->od_priv;

	__pm_relax(aqo_dev->ws);
	wakeup_source_unregister(aqo_dev->ws);

	eth_dev->od_priv = NULL;
	eth_dev->net_dev = NULL;

	memset(aqo_dev, 0, sizeof(*aqo_dev));
	kfree(aqo_dev);
}

struct ipa_eth_net_ops atl_net_ops = {
	.open_device = atl_ipa_open_device,
	.close_device = atl_ipa_close_device,
};

static struct ipa_eth_net_driver atl_net_driver = {
	.events =
		IPA_ETH_DEV_EV_RX_INT |
		IPA_ETH_DEV_EV_TX_INT |
		IPA_ETH_DEV_EV_TX_PTR,
	.features =
		IPA_ETH_DEV_F_L2_CSUM |
		IPA_ETH_DEV_F_L3_CSUM |
		IPA_ETH_DEV_F_TCP_CSUM |
		IPA_ETH_DEV_F_UDP_CSUM |
		IPA_ETH_DEV_F_LSO |
		IPA_ETH_DEV_F_LRO |
		IPA_ETH_DEV_F_VLAN |
		IPA_ETH_DEV_F_MODC |
		IPA_ETH_DEV_F_MODT,
	.bus = &pci_bus_type,
	.ops = &atl_net_ops,
};

static struct ipa_eth_offload_ops aqo_offload_ops = {
	.pair = aqo_pair,
	.unpair = aqo_unpair,

	.init_tx = aqo_init_tx,
	.start_tx = aqc_start_tx,
	.stop_tx = aqc_stop_tx,
	.deinit_tx = aqo_deinit_tx,

	.init_rx = aqo_init_rx,
	.start_rx = aqo_start_rx,
	.stop_rx = aqo_stop_rx,
	.deinit_rx = aqo_deinit_rx,

	.get_stats = aqo_get_stats,
	.clear_stats = aqo_clear_stats,

	.save_regs = aqo_save_regs,
};

static struct ipa_eth_offload_driver aqo_offload_driver = {
	.bus = &pci_bus_type,
	.ops = &aqo_offload_ops,
};

int atl_ipa_register(struct pci_driver *pdrv)
{
	if (!enable_ipa_offload)
		return 0;

	if (!pci_device_ids)
		pci_device_ids = pdrv->id_table;

	if (!atl_net_driver.name)
		atl_net_driver.name = pdrv->name;

	if (!atl_net_driver.driver)
		atl_net_driver.driver = &pdrv->driver;

	if (ipa_eth_register_net_driver(&atl_net_driver) != 0)
		return -EINVAL;

	if (!aqo_offload_driver.name)
		aqo_offload_driver.name = pdrv->name;

	if (ipa_eth_register_offload_driver(&aqo_offload_driver) != 0) {
		ipa_eth_unregister_net_driver(&atl_net_driver);
		return -EINVAL;
	}

	return 0;
}

void atl_ipa_unregister(struct pci_driver *pdrv)
{
	if (!enable_ipa_offload)
		return;

	ipa_eth_unregister_offload_driver(&aqo_offload_driver);
	ipa_eth_unregister_net_driver(&atl_net_driver);
}
