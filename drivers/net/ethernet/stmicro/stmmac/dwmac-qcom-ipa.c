/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/phy.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/mii.h>
#include <linux/of_mdio.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/debugfs.h>
#include <asm/dma-iommu.h>
#include <linux/iommu.h>
#include <linux/ipa.h>
#include <linux/ipa_uc_offload.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/msm_eth.h>

#include "stmmac.h"
#include "stmmac_platform.h"
#include "dwmac-qcom-ethqos.h"
#include "dwmac-qcom-ipa-offload.h"
#include "dwmac-qcom-ipa.h"

#include "stmmac_ptp.h"

#define NTN_IPA_DBG_MAX_MSG_LEN 3000
static char buf[3000];
static struct ethqos_prv_ipa_data eth_ipa_ctx;
static void __ipa_eth_free_msg(void *buff, u32 len, u32 type) {}

/* Network driver specific init for hw offload */
static void eth_ipa_net_drv_init(void)
{
	struct platform_device *pdev = eth_ipa_ctx.ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);

	priv->hw_offload_enabled = true;
	priv->rx_queue[IPA_DMA_RX_CH_BE].skip_sw = true;
	priv->tx_queue[IPA_DMA_TX_CH_BE].skip_sw = true;
	priv->hw->mac->map_mtl_to_dma(priv->hw, IPA_DMA_RX_CH_BE, 1);

	if (eth_ipa_ctx.ethqos->cv2x_mode == CV2X_MODE_MDM) {
		priv->rx_queue[IPA_DMA_RX_CH_CV2X].skip_sw = true;
		priv->tx_queue[IPA_DMA_TX_CH_CV2X].skip_sw = true;
		priv->hw->mac->map_mtl_to_dma(priv->hw, IPA_DMA_RX_CH_CV2X, 1);
	}
}

/* IPA ctx initialization */
static void eth_ipa_ctx_init(void)
{
	struct platform_device *pdev = eth_ipa_ctx.ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);

	mutex_init(&eth_ipa_ctx.ipa_lock);
	eth_ipa_ctx.ethqos->ipa_enabled = true;

	/* set queue enabled */
	eth_ipa_ctx.queue_enabled[IPA_QUEUE_BE] = true;
	if (eth_ipa_ctx.ethqos->cv2x_mode == CV2X_MODE_MDM)
		eth_ipa_ctx.queue_enabled[IPA_QUEUE_CV2X] = true;

	/* set queue/chan numbers */
	eth_ipa_ctx.rx_queue_num[IPA_QUEUE_BE] = IPA_DMA_RX_CH_BE;
	eth_ipa_ctx.tx_queue_num[IPA_QUEUE_BE] = IPA_DMA_TX_CH_BE;
	eth_ipa_ctx.rx_queue_num[IPA_QUEUE_CV2X] = IPA_DMA_RX_CH_CV2X;
	eth_ipa_ctx.tx_queue_num[IPA_QUEUE_CV2X] = IPA_DMA_TX_CH_CV2X;

	/* set desc count for BE queues */
	if (eth_ipa_ctx.queue_enabled[IPA_QUEUE_BE]) {
		if (of_property_read_u32(
			eth_ipa_ctx.ethqos->pdev->dev.of_node,
			"ipa-dma-rx-desc-cnt",
			&eth_ipa_ctx.ipa_dma_rx_desc_cnt[IPA_QUEUE_BE])) {
			ETHQOSDBG(":resource ipa-dma-rx-desc-cnt not in dt\n");
			eth_ipa_ctx.ipa_dma_rx_desc_cnt[IPA_QUEUE_BE] =
				IPA_RX_DESC_CNT_BE;
		}

		if (of_property_read_u32(
			eth_ipa_ctx.ethqos->pdev->dev.of_node,
			"ipa-dma-tx-desc-cnt",
			&eth_ipa_ctx.ipa_dma_tx_desc_cnt[IPA_QUEUE_BE])) {
			ETHQOSDBG(":resource ipa-dma-tx-desc-cnt not in dt\n");
			eth_ipa_ctx.ipa_dma_tx_desc_cnt[IPA_QUEUE_BE] =
				IPA_RX_DESC_CNT_BE;
		}
	}

	/* set desc count for CV2X queues */
	if (eth_ipa_ctx.queue_enabled[IPA_QUEUE_CV2X]) {
		if (of_property_read_u32(
			eth_ipa_ctx.ethqos->pdev->dev.of_node,
			"ipa-dma-rx-desc-cnt-cv2x",
			&eth_ipa_ctx.ipa_dma_rx_desc_cnt[IPA_QUEUE_CV2X])) {
			ETHQOSDBG(":resource ipa-dma-rx-desc-cnt not in dt\n");
			eth_ipa_ctx.ipa_dma_rx_desc_cnt[IPA_QUEUE_CV2X] =
				IPA_RX_DESC_CNT_CV2X;
		}

		if (of_property_read_u32(
			eth_ipa_ctx.ethqos->pdev->dev.of_node,
			"ipa-dma-tx-desc-cnt-cv2x",
			&eth_ipa_ctx.ipa_dma_tx_desc_cnt[IPA_QUEUE_CV2X])) {
			ETHQOSDBG(":resource ipa-dma-tx-desc-cnt not in dt\n");
			eth_ipa_ctx.ipa_dma_tx_desc_cnt[IPA_QUEUE_CV2X] =
				IPA_TX_DESC_CNT_CV2X;
		}
	}

	/* set interrupt routing mode */
	if (eth_ipa_ctx.ethqos->cv2x_mode) {
		eth_ipa_ctx.tx_intr_route_mode[IPA_QUEUE_BE] =
			IPA_INTR_ROUTE_DB;
		eth_ipa_ctx.rx_intr_route_mode[IPA_QUEUE_BE] =
			IPA_INTR_ROUTE_DB;
		eth_ipa_ctx.tx_intr_route_mode[IPA_QUEUE_CV2X] =
			IPA_INTR_ROUTE_HW;
		eth_ipa_ctx.rx_intr_route_mode[IPA_QUEUE_CV2X] =
			IPA_INTR_ROUTE_HW;
	} else {
		eth_ipa_ctx.tx_intr_route_mode[IPA_QUEUE_BE] =
			IPA_INTR_ROUTE_HW;
		eth_ipa_ctx.rx_intr_route_mode[IPA_QUEUE_BE] =
			IPA_INTR_ROUTE_HW;
	}

	/* set buf len */
	eth_ipa_ctx.buf_len[IPA_QUEUE_BE] = ETHQOS_ETH_FRAME_LEN_IPA_BE;
	eth_ipa_ctx.buf_len[IPA_QUEUE_CV2X] = ETHQOS_ETH_FRAME_LEN_IPA_CV2X;

	/* set ipa_notify_cb */
	eth_ipa_ctx.ipa_notify_cb[IPA_QUEUE_BE] = ntn_ipa_notify_cb_be;
	eth_ipa_ctx.ipa_notify_cb[IPA_QUEUE_CV2X] = ntn_ipa_notify_cb_cv2x;

	/* set proto */
	eth_ipa_ctx.ipa_proto[IPA_QUEUE_BE] = IPA_UC_NTN;
	eth_ipa_ctx.ipa_proto[IPA_QUEUE_CV2X] = IPA_UC_NTN_V2X;

	/* set ipa tx client */
	eth_ipa_ctx.tx_client[IPA_QUEUE_BE] = IPA_CLIENT_ETHERNET_CONS;
	eth_ipa_ctx.tx_client[IPA_QUEUE_CV2X] = IPA_CLIENT_ETHERNET2_CONS;

	/* set ipa rx client */
	eth_ipa_ctx.rx_client[IPA_QUEUE_BE] = IPA_CLIENT_ETHERNET_PROD;
	eth_ipa_ctx.rx_client[IPA_QUEUE_CV2X] = IPA_CLIENT_ETHERNET2_PROD;

	eth_ipa_ctx.rx_reg_base_ptr_pa[IPA_QUEUE_BE] =
		(((phys_addr_t)(DMA_CR0_RGOFFADDR - BASE_ADDRESS)) +
		 (phys_addr_t)eth_ipa_ctx.ethqos->emac_mem_base);

	eth_ipa_ctx.rx_reg_base_ptr_pa[IPA_QUEUE_CV2X] =
		(((phys_addr_t)(DMA_CR3_RGOFFADDR - BASE_ADDRESS)) +
		 (phys_addr_t)eth_ipa_ctx.ethqos->emac_mem_base);

	eth_ipa_ctx.tx_reg_base_ptr_pa[IPA_QUEUE_BE] =
		(((phys_addr_t)(DMA_CR0_RGOFFADDR - BASE_ADDRESS)) +
		 (phys_addr_t)eth_ipa_ctx.ethqos->emac_mem_base);

	eth_ipa_ctx.tx_reg_base_ptr_pa[IPA_QUEUE_CV2X] =
		(((phys_addr_t)(DMA_CR3_RGOFFADDR - BASE_ADDRESS)) +
		 (phys_addr_t)eth_ipa_ctx.ethqos->emac_mem_base);

	eth_ipa_ctx.need_send_msg[IPA_QUEUE_BE] = true;
	eth_ipa_ctx.need_send_msg[IPA_QUEUE_CV2X] = false;

	strlcpy(eth_ipa_ctx.netdev_name[IPA_QUEUE_BE],
		priv->dev->name, ETH_DEV_NAME_LEN);
	strlcpy(eth_ipa_ctx.netdev_name[IPA_QUEUE_CV2X],
		"eth.cv2x", ETH_DEV_NAME_LEN);

	ETHQOSDBG("eth_ipa_ctx.netdev_name[IPA_QUEUE_BE] %s\n",
		  eth_ipa_ctx.netdev_name[IPA_QUEUE_BE]);
	ETHQOSDBG("eth_ipa_ctx.netdev_name[IPA_QUEUE_CV2X] %s\n",
		  eth_ipa_ctx.netdev_name[IPA_QUEUE_CV2X]);

	memcpy(eth_ipa_ctx.netdev_addr[IPA_QUEUE_BE],
	       priv->dev->dev_addr, ETH_ALEN);
	memcpy(eth_ipa_ctx.netdev_addr[IPA_QUEUE_CV2X],
	       eth_ipa_ctx.ethqos->cv2x_dev_addr, ETH_ALEN);

	eth_ipa_ctx.netdev_index[IPA_QUEUE_BE] = priv->dev->ifindex;
	eth_ipa_ctx.netdev_index[IPA_QUEUE_CV2X] = 0;

	eth_ipa_ctx.rx_intr_mod_cnt[IPA_QUEUE_BE] = 0;
	eth_ipa_ctx.rx_intr_mod_cnt[IPA_QUEUE_CV2X] = 1;
}

static inline bool eth_ipa_queue_type_supported(enum ipa_queue_type type)
{
	return (type >= 0 && type < IPA_QUEUE_MAX);
}

static inline bool eth_ipa_queue_type_enabled(enum ipa_queue_type type)
{
	WARN_ON(!eth_ipa_queue_type_supported(type));
	return eth_ipa_ctx.queue_enabled[type];
}

static inline char *eth_ipa_queue_type_to_device_name(enum ipa_queue_type type)
{
	WARN_ON(!eth_ipa_queue_type_supported(type));
	return eth_ipa_ctx.netdev_name[type];
}

static inline u8 *eth_ipa_queue_type_to_device_addr(enum ipa_queue_type type)
{
	WARN_ON(!eth_ipa_queue_type_supported(type));
	return eth_ipa_ctx.netdev_addr[type];
}

static inline u8 eth_ipa_queue_type_to_if_index(enum ipa_queue_type type)
{
	WARN_ON(!eth_ipa_queue_type_supported(type));
	return eth_ipa_ctx.netdev_index[type];
}

static inline enum ipa_intr_route_type
	eth_ipa_queue_type_to_tx_intr_route(enum ipa_queue_type type)
{
	WARN_ON(!eth_ipa_queue_type_supported(type));
	return eth_ipa_ctx.tx_intr_route_mode[type];
}

static inline enum ipa_intr_route_type
	eth_ipa_queue_type_to_rx_intr_route(enum ipa_queue_type type)
{
	WARN_ON(!eth_ipa_queue_type_supported(type));
	return eth_ipa_ctx.rx_intr_route_mode[type];
}

static inline u8 eth_ipa_queue_type_to_tx_queue(enum ipa_queue_type type)
{
	WARN_ON(!eth_ipa_queue_type_supported(type));
	return eth_ipa_ctx.tx_queue_num[type];
}

static inline u8 eth_ipa_queue_type_to_rx_queue(enum ipa_queue_type type)
{
	WARN_ON(!eth_ipa_queue_type_supported(type));
	return eth_ipa_ctx.rx_queue_num[type];
}

static inline u32 eth_ipa_queue_type_to_tx_desc_count(enum ipa_queue_type type)
{
	WARN_ON(!eth_ipa_queue_type_supported(type));
	return eth_ipa_ctx.ipa_dma_tx_desc_cnt[type];
}

static inline u32 eth_ipa_queue_type_to_rx_desc_count(enum ipa_queue_type type)
{
	WARN_ON(!eth_ipa_queue_type_supported(type));
	return eth_ipa_ctx.ipa_dma_rx_desc_cnt[type];
}

static inline u32
eth_ipa_queue_type_to_rx_intr_mod_cnt(enum ipa_queue_type type)
{
	WARN_ON(!eth_ipa_queue_type_supported(type));
	return eth_ipa_ctx.rx_intr_mod_cnt[type];
}

/* One common function for TX and RX buf lengths,
 * to be changed if TX and RX have different buf lengths
 */
static inline u32 eth_ipa_queue_type_to_buf_length(enum ipa_queue_type type)
{
	WARN_ON(!eth_ipa_queue_type_supported(type));
	return eth_ipa_ctx.buf_len[type];
}

static inline ipa_notify_cb
	eth_ipa_queue_type_to_ipa_notify_cb(enum ipa_queue_type type)
{
	WARN_ON(!eth_ipa_queue_type_supported(type));
	return eth_ipa_ctx.ipa_notify_cb[type];
}

static inline u32 eth_ipa_queue_type_to_proto(enum ipa_queue_type type)
{
	WARN_ON(!eth_ipa_queue_type_supported(type));
	return eth_ipa_ctx.ipa_proto[type];
}

static inline u32 eth_ipa_queue_type_to_tx_client(enum ipa_queue_type type)
{
	WARN_ON(!eth_ipa_queue_type_supported(type));
	return eth_ipa_ctx.tx_client[type];
}

static inline u32 eth_ipa_queue_type_to_rx_client(enum ipa_queue_type type)
{
	WARN_ON(!eth_ipa_queue_type_supported(type));
	return eth_ipa_ctx.rx_client[type];
}

static inline bool eth_ipa_queue_type_to_ipa_vlan_mode(enum ipa_queue_type type)
{
	bool ipa_vlan_mode = false;

	WARN_ON(!eth_ipa_queue_type_supported(type));

	switch (type) {
	case IPA_QUEUE_BE:
		if (ipa_is_vlan_mode(IPA_VLAN_IF_EMAC, &ipa_vlan_mode)) {
			ETHQOSERR("Could not read ipa_vlan_mode\n");
			/* In case of failure, fallback to non vlan mode */
			ipa_vlan_mode = false;
		}
		return ipa_vlan_mode;
	case IPA_QUEUE_CV2X:
		return true;
	default:
		return false;
	}

	return false;
}

static inline phys_addr_t eth_ipa_queue_type_to_rx_reg_base_ptr_pa(
	enum ipa_queue_type type)
{
	WARN_ON(!eth_ipa_queue_type_supported(type));
	return eth_ipa_ctx.rx_reg_base_ptr_pa[type];
}

static inline phys_addr_t eth_ipa_queue_type_to_tx_reg_base_ptr_pa(
	enum ipa_queue_type type)
{
	WARN_ON(!eth_ipa_queue_type_supported(type));
	return eth_ipa_ctx.tx_reg_base_ptr_pa[type];
}

static void eth_ipa_handle_rx_interrupt(unsigned int qinx)
{
	int type;

	if (!eth_ipa_ctx.ipa_offload_conn) {
		ETHQOSERR("IPA Offload not connected\n");
		return;
	}

	for (type = 0; type < IPA_QUEUE_MAX; type++) {
		if (eth_ipa_queue_type_enabled(type) &&
		    (eth_ipa_queue_type_to_rx_intr_route(type) ==
		     IPA_INTR_ROUTE_DB) &&
		    (eth_ipa_queue_type_to_rx_queue(type) == qinx)) {
			ETHQOSDBG("writing for qinx %d db=%x\n",
				  qinx, eth_ipa_ctx.uc_db_rx_addr[type]);
			writel_relaxed(1,  eth_ipa_ctx.uc_db_rx_addr[type]);
			break;
		}
	}
}

static void eth_ipa_handle_tx_interrupt(unsigned int qinx)
{
	int type;

	if (!eth_ipa_ctx.ipa_offload_conn) {
		ETHQOSERR("IPA Offload not connected\n");
		return;
	}

	for (type = 0; type < IPA_QUEUE_MAX; type++) {
		if (eth_ipa_queue_type_enabled(type) &&
		    (eth_ipa_queue_type_to_tx_intr_route(type) ==
		     IPA_INTR_ROUTE_DB) &&
		    (eth_ipa_queue_type_to_tx_queue(type) == qinx)) {
			ETHQOSDBG("writing for qinx %d db=%x\n",
				  qinx, eth_ipa_ctx.uc_db_tx_addr[type]);
			writel_relaxed(1,  eth_ipa_ctx.uc_db_tx_addr[type]);
			break;
		}
	}
}

static inline bool
eth_ipa_queue_type_to_send_msg_needed(enum ipa_queue_type type)
{
	WARN_ON(!eth_ipa_queue_type_supported(type));
	return eth_ipa_ctx.need_send_msg[type];
}

static inline void *ethqos_get_priv(struct qcom_ethqos *ethqos)
{
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);

	return priv;
}

static int eth_ipa_send_msg(struct qcom_ethqos *ethqos,
			    enum ipa_peripheral_event event,
			    enum ipa_queue_type type)
{
	struct ipa_msg_meta msg_meta;
	struct ipa_ecm_msg emac_msg;
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);

	if (!priv || !priv->dev)
		return -EFAULT;

	memset(&msg_meta, 0, sizeof(msg_meta));
	memset(&emac_msg, 0, sizeof(emac_msg));

	emac_msg.ifindex = eth_ipa_queue_type_to_if_index(type);
	strlcpy(emac_msg.name, eth_ipa_queue_type_to_device_name(type),
		IPA_RESOURCE_NAME_MAX);

	msg_meta.msg_type = event;
	msg_meta.msg_len = sizeof(struct ipa_ecm_msg);

	return ipa_send_msg(&msg_meta, &emac_msg, __ipa_eth_free_msg);
}

static int ethqos_alloc_ipa_tx_queue_struct(struct qcom_ethqos *ethqos,
					    enum ipa_queue_type type)
{
	int ret = 0, chinx, cnt;
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);

	chinx = eth_ipa_queue_type_to_tx_queue(type);

	eth_ipa_ctx.tx_queue[type] =
		kzalloc(sizeof(struct ethqos_tx_queue),
			GFP_KERNEL);
	if (!eth_ipa_ctx.tx_queue[type]) {
		ETHQOSERR("ERR: Unable to allocate Tx queue struct for %d\n",
			  type);
		ret = -ENOMEM;
		goto err_out_tx_q_alloc_failed;
	}

	eth_ipa_ctx.tx_queue[type]->desc_cnt =
		eth_ipa_queue_type_to_tx_desc_count(type);

	/* Allocate tx_desc_ptrs */
	eth_ipa_ctx.tx_queue[type]->tx_desc_ptrs =
		kcalloc(eth_ipa_ctx.tx_queue[type]->desc_cnt,
			sizeof(struct dma_desc *), GFP_KERNEL);
	if (!eth_ipa_ctx.tx_queue[type]->tx_desc_ptrs) {
		ETHQOSERR("ERR: Unable to allocate Tx Desc ptrs for %d\n",
			  type);
		ret = -ENOMEM;
		goto err_out_tx_desc_ptrs_failed;
	}
	for (cnt = 0; cnt < eth_ipa_ctx.tx_queue[type]->desc_cnt; cnt++) {
		eth_ipa_ctx.tx_queue[type]->tx_desc_ptrs[cnt]
		= eth_ipa_ctx.tx_queue[type]->tx_desc_ptrs[0] +
		(sizeof(struct dma_desc *) * cnt);
	}

	/* Allocate tx_desc_dma_addrs */
	eth_ipa_ctx.tx_queue[type]->tx_desc_dma_addrs =
			kzalloc(sizeof(dma_addr_t) *
				eth_ipa_ctx.tx_queue[type]->desc_cnt,
				GFP_KERNEL);
	if (!eth_ipa_ctx.tx_queue[type]->tx_desc_dma_addrs) {
		ETHQOSERR("ERR: Unable to allocate Tx Desc dma addrs for %d\n",
			  type);
		ret = -ENOMEM;
		goto err_out_tx_desc_dma_addrs_failed;
	}
	for (cnt = 0; cnt < eth_ipa_ctx.tx_queue[type]->desc_cnt; cnt++) {
		eth_ipa_ctx.tx_queue[type]->tx_desc_dma_addrs[cnt]
		= eth_ipa_ctx.tx_queue[type]->tx_desc_dma_addrs[0] +
		(sizeof(dma_addr_t) * cnt);
	}

	/* Allocate tx_buf_ptrs */
	eth_ipa_ctx.tx_queue[type]->skb =
		kzalloc(sizeof(struct sk_buff *) *
			eth_ipa_ctx.tx_queue[type]->desc_cnt, GFP_KERNEL);
	if (!eth_ipa_ctx.tx_queue[type]->skb) {
		ETHQOSERR("ERR: Unable to allocate Tx buff ptrs for %d\n",
			  type);
		ret = -ENOMEM;
		goto err_out_tx_buf_ptrs_failed;
	}

	/* Allocate ipa_tx_buff_pool_va_addrs_base */
	eth_ipa_ctx.tx_queue[type]->ipa_tx_buff_pool_va_addrs_base =
	kzalloc(sizeof(void *) * eth_ipa_ctx.tx_queue[type]->desc_cnt,
		GFP_KERNEL);
	if (!eth_ipa_ctx.tx_queue[type]->ipa_tx_buff_pool_va_addrs_base) {
		ETHQOSERR("ERR: Unable to allocate Tx ipa buff addrs for %d\n",
			  type);
		ret = -ENOMEM;
		goto err_out_tx_buf_ptrs_failed;
	}

	eth_ipa_ctx.tx_queue[type]->skb_dma =
	kzalloc(sizeof(dma_addr_t) * eth_ipa_ctx.tx_queue[type]->desc_cnt,
		GFP_KERNEL);
	if (!eth_ipa_ctx.tx_queue[type]->skb_dma) {
		ETHQOSERR("ERR: Unable to allocate Tx ipa buff addrs for %d\n",
			  type);
		ret = -ENOMEM;
		goto err_out_tx_buf_ptrs_failed;
	}

	eth_ipa_ctx.tx_queue[type]->ipa_tx_phy_addr =
	kzalloc(sizeof(phys_addr_t) * eth_ipa_ctx.tx_queue[type]->desc_cnt,
		GFP_KERNEL);
	if (!eth_ipa_ctx.tx_queue[type]->ipa_tx_phy_addr) {
		ETHQOSERR("ERROR: Unable to allocate Tx ipa buff dma addrs\n");
		ret = -ENOMEM;
		goto err_out_tx_buf_ptrs_failed;
	}

	ETHQOSDBG("<--ethqos_alloc_tx_queue_struct\n");
	eth_ipa_ctx.tx_queue[type]->tx_q = &priv->tx_queue[chinx];
	return ret;

err_out_tx_buf_ptrs_failed:
	kfree(eth_ipa_ctx.tx_queue[type]->tx_desc_dma_addrs);
	eth_ipa_ctx.tx_queue[type]->tx_desc_dma_addrs = NULL;
	kfree(eth_ipa_ctx.tx_queue[type]->skb);
	eth_ipa_ctx.tx_queue[type]->skb = NULL;
	kfree(eth_ipa_ctx.tx_queue[type]->skb_dma);
	eth_ipa_ctx.tx_queue[type]->skb_dma = NULL;
	kfree(eth_ipa_ctx.tx_queue[type]->ipa_tx_buff_pool_va_addrs_base);
	eth_ipa_ctx.tx_queue[type]->ipa_tx_buff_pool_va_addrs_base = NULL;
	kfree(eth_ipa_ctx.tx_queue[type]->ipa_tx_phy_addr);
	eth_ipa_ctx.tx_queue[type]->ipa_tx_phy_addr = NULL;

err_out_tx_desc_dma_addrs_failed:
	kfree(eth_ipa_ctx.tx_queue[type]->tx_desc_ptrs);
	eth_ipa_ctx.tx_queue[type]->tx_desc_ptrs = NULL;

err_out_tx_desc_ptrs_failed:
	kfree(eth_ipa_ctx.tx_queue[type]);
	eth_ipa_ctx.tx_queue[type] = NULL;

err_out_tx_q_alloc_failed:
	return ret;
}

static void ethqos_free_ipa_tx_queue_struct(struct qcom_ethqos *ethqos,
					    enum ipa_queue_type type)
{
	kfree(eth_ipa_ctx.tx_queue[type]->skb_dma);
	eth_ipa_ctx.tx_queue[type]->skb_dma = NULL;

	kfree(eth_ipa_ctx.tx_queue[type]->tx_desc_dma_addrs);
	eth_ipa_ctx.tx_queue[type]->tx_desc_dma_addrs = NULL;

	kfree(eth_ipa_ctx.tx_queue[type]->tx_desc_ptrs);
	eth_ipa_ctx.tx_queue[type]->tx_desc_ptrs = NULL;

	kfree(eth_ipa_ctx.tx_queue[type]->ipa_tx_buff_pool_va_addrs_base);
	eth_ipa_ctx.tx_queue[type]->ipa_tx_buff_pool_va_addrs_base = NULL;

	kfree(eth_ipa_ctx.tx_queue[type]->skb);
	eth_ipa_ctx.tx_queue[type]->skb = NULL;
}

static int ethqos_alloc_ipa_rx_queue_struct(struct qcom_ethqos *ethqos,
					    enum ipa_queue_type type)
{
	int ret = 0, chinx, cnt;

	chinx = eth_ipa_queue_type_to_rx_queue(type);

	eth_ipa_ctx.rx_queue[type] =
		kzalloc(sizeof(struct ethqos_rx_queue),
			GFP_KERNEL);
	if (!eth_ipa_ctx.rx_queue[type]) {
		ETHQOSERR("ERROR: Unable to allocate Rx queue structure\n");
		ret = -ENOMEM;
		goto err_out_rx_q_alloc_failed;
	}

	eth_ipa_ctx.rx_queue[type]->desc_cnt =
		eth_ipa_queue_type_to_rx_desc_count(type);

	/* Allocate rx_desc_ptrs */
	eth_ipa_ctx.rx_queue[type]->rx_desc_ptrs =
		kcalloc(eth_ipa_ctx.rx_queue[type]->desc_cnt,
			sizeof(struct dma_desc *), GFP_KERNEL);
	if (!eth_ipa_ctx.rx_queue[type]->rx_desc_ptrs) {
		ETHQOSERR("ERROR: Unable to allocate Rx Desc ptrs\n");
		ret = -ENOMEM;
		goto err_out_rx_desc_ptrs_failed;
	}

	for (cnt = 0; cnt < eth_ipa_ctx.rx_queue[type]->desc_cnt; cnt++) {
		eth_ipa_ctx.rx_queue[type]->rx_desc_ptrs[cnt]
		= eth_ipa_ctx.rx_queue[type]->rx_desc_ptrs[0] +
		(sizeof(struct dma_desc *) * cnt);
	}

	/* Allocate rx_desc_dma_addrs */
	eth_ipa_ctx.rx_queue[type]->rx_desc_dma_addrs =
	kzalloc(sizeof(dma_addr_t) * eth_ipa_ctx.rx_queue[type]->desc_cnt,
		GFP_KERNEL);
	if (!eth_ipa_ctx.rx_queue[type]->rx_desc_dma_addrs) {
		ETHQOSERR("ERROR: Unable to allocate Rx Desc dma addr\n");
		ret = -ENOMEM;
		goto err_out_rx_desc_dma_addrs_failed;
	}

	for (cnt = 0; cnt < eth_ipa_ctx.rx_queue[type]->desc_cnt; cnt++) {
		eth_ipa_ctx.rx_queue[type]->rx_desc_dma_addrs[cnt]
		= eth_ipa_ctx.rx_queue[type]->rx_desc_dma_addrs[0]
		+ (sizeof(dma_addr_t) * cnt);
	}
	/* Allocat rx_ipa_buff */
	eth_ipa_ctx.rx_queue[type]->skb =
		kzalloc(sizeof(struct sk_buff *) *
			eth_ipa_ctx.rx_queue[type]->desc_cnt, GFP_KERNEL);
	if (!eth_ipa_ctx.rx_queue[type]->skb) {
		ETHQOSERR("ERROR: Unable to allocate Tx buff ptrs\n");
		ret = -ENOMEM;
		goto err_out_rx_buf_ptrs_failed;
	}

	eth_ipa_ctx.rx_queue[type]->ipa_buff_va =
	kzalloc(sizeof(void *) *
		eth_ipa_ctx.rx_queue[type]->desc_cnt, GFP_KERNEL);
	if (!eth_ipa_ctx.rx_queue[type]->ipa_buff_va) {
		ETHQOSERR("ERROR: Unable to allocate Tx buff ptrs\n");
		ret = -ENOMEM;
		goto err_out_rx_buf_ptrs_failed;
	}

	/* Allocate ipa_rx_buff_pool_va_addrs_base */
	eth_ipa_ctx.rx_queue[type]->ipa_rx_buff_pool_va_addrs_base =
		kzalloc(sizeof(void *) * eth_ipa_ctx.rx_queue[type]->desc_cnt,
			GFP_KERNEL);
	if (!eth_ipa_ctx.rx_queue[type]->ipa_rx_buff_pool_va_addrs_base) {
		ETHQOSERR("ERROR: Unable to allocate Rx ipa buff addrs\n");
		ret = -ENOMEM;
		goto err_out_rx_buf_ptrs_failed;
	}

	eth_ipa_ctx.rx_queue[type]->skb_dma =
	kzalloc(sizeof(dma_addr_t) * eth_ipa_ctx.rx_queue[type]->desc_cnt,
		GFP_KERNEL);
	if (!eth_ipa_ctx.rx_queue[type]->skb_dma) {
		ETHQOSERR("ERROR: Unable to allocate rx ipa buff addrs\n");
		ret = -ENOMEM;
		goto err_out_rx_buf_ptrs_failed;
	}

	eth_ipa_ctx.rx_queue[type]->ipa_rx_buff_phy_addr =
	kzalloc(sizeof(phys_addr_t) * eth_ipa_ctx.rx_queue[type]->desc_cnt,
		GFP_KERNEL);
	if (!eth_ipa_ctx.rx_queue[type]->ipa_rx_buff_phy_addr) {
		ETHQOSERR("ERROR: Unable to allocate rx ipa buff  dma addrs\n");
		ret = -ENOMEM;
		goto err_out_rx_buf_ptrs_failed;
	}

	ETHQOSDBG("<--ethqos_alloc_rx_queue_struct\n");
	return ret;

err_out_rx_buf_ptrs_failed:
	kfree(eth_ipa_ctx.rx_queue[type]->rx_desc_dma_addrs);
	eth_ipa_ctx.rx_queue[type]->rx_desc_dma_addrs = NULL;
	kfree(eth_ipa_ctx.rx_queue[type]->skb);
	eth_ipa_ctx.rx_queue[type]->skb = NULL;
	kfree(eth_ipa_ctx.rx_queue[type]->skb_dma);
	eth_ipa_ctx.rx_queue[type]->skb_dma = NULL;
	kfree(eth_ipa_ctx.rx_queue[type]->ipa_rx_buff_pool_va_addrs_base);
	eth_ipa_ctx.rx_queue[type]->ipa_rx_buff_pool_va_addrs_base = NULL;
	kfree(eth_ipa_ctx.rx_queue[type]->ipa_rx_buff_phy_addr);
	eth_ipa_ctx.rx_queue[type]->ipa_rx_buff_phy_addr = NULL;
	kfree(eth_ipa_ctx.rx_queue[type]->ipa_buff_va);
	eth_ipa_ctx.rx_queue[type]->ipa_buff_va = NULL;

err_out_rx_desc_dma_addrs_failed:
	kfree(eth_ipa_ctx.rx_queue[type]->rx_desc_ptrs);
	eth_ipa_ctx.rx_queue[type]->rx_desc_ptrs = NULL;

err_out_rx_desc_ptrs_failed:
	kfree(eth_ipa_ctx.rx_queue[type]);
	eth_ipa_ctx.rx_queue[type] = NULL;

err_out_rx_q_alloc_failed:
	return ret;
}

static void ethqos_free_ipa_rx_queue_struct(struct qcom_ethqos *ethqos,
					    enum ipa_queue_type type)
{
	kfree(eth_ipa_ctx.rx_queue[type]->skb);
	eth_ipa_ctx.rx_queue[type]->skb = NULL;

	kfree(eth_ipa_ctx.rx_queue[type]->rx_desc_dma_addrs);
	eth_ipa_ctx.rx_queue[type]->rx_desc_dma_addrs = NULL;

	kfree(eth_ipa_ctx.rx_queue[type]->rx_desc_ptrs);
	eth_ipa_ctx.rx_queue[type]->rx_desc_ptrs = NULL;

	kfree(eth_ipa_ctx.rx_queue[type]->ipa_rx_buff_pool_va_addrs_base);
	eth_ipa_ctx.rx_queue[type]->ipa_rx_buff_pool_va_addrs_base = NULL;

	kfree(eth_ipa_ctx.rx_queue[type]->skb);
	eth_ipa_ctx.rx_queue[type]->skb = NULL;
}

static void ethqos_rx_buf_free_mem(struct qcom_ethqos *ethqos,
				   enum ipa_queue_type type)
{
	struct net_device *ndev = dev_get_drvdata(&ethqos->pdev->dev);
	struct stmmac_priv *priv = netdev_priv(ndev);

		/* Deallocate RX Buffer Pool Structure */
		/* Free memory pool for RX offload path */
	if (eth_ipa_ctx.rx_queue[type]->ipa_rx_pa_addrs_base) {
		dma_free_coherent
		(GET_MEM_PDEV_DEV,
		 sizeof(dma_addr_t) * eth_ipa_ctx.rx_queue[type]->desc_cnt,
		 eth_ipa_ctx.rx_queue[type]->ipa_rx_pa_addrs_base,
		 eth_ipa_ctx.rx_queue[type]->ipa_rx_pa_addrs_base_dmahndl);

		eth_ipa_ctx.rx_queue[type]->ipa_rx_pa_addrs_base =
			NULL;
		eth_ipa_ctx.rx_queue[type]->ipa_rx_pa_addrs_base_dmahndl
		= (dma_addr_t)NULL;
			ETHQOSDBG("Freed Rx Buffer Pool Structure for IPA\n");
		} else {
			ETHQOSERR("Unable to DeAlloc RX Buff structure\n");
		}
}

static void ethqos_rx_desc_free_mem(struct qcom_ethqos *ethqos,
				    enum ipa_queue_type type)
{
	struct net_device *ndev = dev_get_drvdata(&ethqos->pdev->dev);
	struct stmmac_priv *priv = netdev_priv(ndev);

	ETHQOSDBG("rx_queue = %d\n", eth_ipa_queue_type_to_rx_queue(type));

	if (eth_ipa_ctx.rx_queue[type]->rx_desc_ptrs[0]) {
		dma_free_coherent
		(GET_MEM_PDEV_DEV,
		 (sizeof(struct dma_desc) *
		 eth_ipa_ctx.rx_queue[type]->desc_cnt),
		 eth_ipa_ctx.rx_queue[type]->rx_desc_ptrs[0],
		 eth_ipa_ctx.rx_queue[type]->rx_desc_dma_addrs[0]);
		eth_ipa_ctx.rx_queue[type]->rx_desc_ptrs[0] = NULL;
	}

	ETHQOSDBG("\n");
}

static void ethqos_tx_buf_free_mem(struct qcom_ethqos *ethqos,
				  enum ipa_queue_type type)
{
	unsigned int i = 0;
	struct net_device *ndev = dev_get_drvdata(&ethqos->pdev->dev);
	struct stmmac_priv *priv = netdev_priv(ndev);

	ETHQOSDBG("tx_queue = %d\n", eth_ipa_queue_type_to_tx_queue(type));

	for (i = 0; i < eth_ipa_ctx.tx_queue[type]->desc_cnt; i++) {
		dma_free_coherent
		(GET_MEM_PDEV_DEV,
		 eth_ipa_queue_type_to_buf_length(type),
		 eth_ipa_ctx.tx_queue[type]->ipa_tx_buff_pool_va_addrs_base[i],
		 eth_ipa_ctx.tx_queue[type]->ipa_tx_pa_addrs_base[i]);
	}
	ETHQOSDBG("Freed the memory allocated for %d\n",
		  eth_ipa_queue_type_to_tx_queue(type));
	/* De-Allocate TX DMA Buffer Pool Structure */
	if (eth_ipa_ctx.tx_queue[type]->ipa_tx_pa_addrs_base) {
		dma_free_coherent
		(GET_MEM_PDEV_DEV,
		 sizeof(dma_addr_t) *
		 eth_ipa_ctx.tx_queue[type]->desc_cnt,
		 eth_ipa_ctx.tx_queue[type]->ipa_tx_pa_addrs_base,
		 eth_ipa_ctx.tx_queue[type]->ipa_tx_pa_addrs_base_dmahndl);

		eth_ipa_ctx.tx_queue[type]->ipa_tx_pa_addrs_base = NULL;
		eth_ipa_ctx.tx_queue[type]->ipa_tx_pa_addrs_base_dmahndl
		= (dma_addr_t)NULL;
		ETHQOSERR("Freed TX Buffer Pool Structure for IPA\n");
		} else {
			ETHQOSERR("Unable to DeAlloc TX Buff structure\n");
	}
	ETHQOSDBG("\n");
}

static void ethqos_tx_desc_free_mem(struct qcom_ethqos *ethqos,
				    enum ipa_queue_type type)
{
	struct net_device *ndev = dev_get_drvdata(&ethqos->pdev->dev);
	struct stmmac_priv *priv = netdev_priv(ndev);

	ETHQOSDBG("tx_queue = %d\n", eth_ipa_queue_type_to_tx_queue(type));

	if (eth_ipa_ctx.tx_queue[type]->tx_desc_ptrs[0]) {
		dma_free_coherent
		(GET_MEM_PDEV_DEV,
		 (sizeof(struct dma_desc) *
		 eth_ipa_ctx.tx_queue[type]->desc_cnt),
		 eth_ipa_ctx.tx_queue[type]->tx_desc_ptrs[0],
		 eth_ipa_ctx.tx_queue[type]->tx_desc_dma_addrs[0]);

		eth_ipa_ctx.tx_queue[type]->tx_desc_ptrs[0] = NULL;
	}

	ETHQOSDBG("\n");
}

static int allocate_ipa_buffer_and_desc(
	struct qcom_ethqos *ethqos, enum ipa_queue_type type)
{
	int ret = 0;
	unsigned int qinx = 0;
	struct net_device *ndev = dev_get_drvdata(&ethqos->pdev->dev);
	struct stmmac_priv *priv = netdev_priv(ndev);

	/* TX descriptors */
	eth_ipa_ctx.tx_queue[type]->tx_desc_ptrs[0] = dma_alloc_coherent(
	   GET_MEM_PDEV_DEV,
	   (sizeof(struct dma_desc) * eth_ipa_ctx.tx_queue[type]->desc_cnt),
		(eth_ipa_ctx.tx_queue[type]->tx_desc_dma_addrs),
		GFP_KERNEL);
	if (!eth_ipa_ctx.tx_queue[type]->tx_desc_ptrs[0]) {
		ret = -ENOMEM;
		goto err_out_tx_desc;
	}
	ETHQOSERR("Tx Queue(%d) desc base dma address: %pK\n",
		  qinx, eth_ipa_ctx.tx_queue[type]->tx_desc_dma_addrs);

	/* Allocate descriptors and buffers memory for all RX queues */
	/* RX descriptors */
	eth_ipa_ctx.rx_queue[type]->rx_desc_ptrs[0] = dma_alloc_coherent(
	   GET_MEM_PDEV_DEV,
	   (sizeof(struct dma_desc) * eth_ipa_ctx.rx_queue[type]->desc_cnt),
	   (eth_ipa_ctx.rx_queue[type]->rx_desc_dma_addrs), GFP_KERNEL);
	if (!eth_ipa_ctx.rx_queue[type]->rx_desc_ptrs[0]) {
		ret = -ENOMEM;
		goto rx_alloc_failure;
	}
	ETHQOSDBG("Rx Queue(%d) desc base dma address: %pK\n",
		  qinx, eth_ipa_ctx.rx_queue[type]->rx_desc_dma_addrs);

	ETHQOSDBG("\n");

	return ret;

 rx_alloc_failure:
	ethqos_rx_desc_free_mem(ethqos, type);

 err_out_tx_desc:
	ethqos_tx_desc_free_mem(ethqos, type);

	return ret;
}

static void ethqos_ipa_tx_desc_init(struct qcom_ethqos *ethqos,
				    enum ipa_queue_type type)
{
	int i = 0;
	struct dma_desc *TX_NORMAL_DESC;
	unsigned int qinx = eth_ipa_queue_type_to_tx_queue(type);

	ETHQOSDBG("-->tx_descriptor_init\n");

	/* initialze all descriptors. */

	for (i = 0; i < eth_ipa_ctx.tx_queue[type]->desc_cnt; i++) {
		TX_NORMAL_DESC = eth_ipa_ctx.tx_queue[type]->tx_desc_ptrs[i];
		TX_NORMAL_DESC->des0 = 0;
		/* update buffer 2 address pointer to zero */
		TX_NORMAL_DESC->des1 = 0;
		/* set all other control bits (IC, TTSE, B2L & B1L) to zero */
		TX_NORMAL_DESC->des2 = 0;
		/* set all other control bits (OWN, CTXT, FD, LD, CPC, CIC etc)
		 * to zero
		 */
		TX_NORMAL_DESC->des3 = 0;
	}
	/* update the total no of Tx descriptors count */
	DMA_TDRLR_RGWR(qinx, (eth_ipa_ctx.tx_queue[type]->desc_cnt - 1));
	/* update the starting address of desc chain/ring */
	DMA_TDLAR_RGWR(qinx, eth_ipa_ctx.tx_queue[type]->tx_desc_dma_addrs[0]);

	ETHQOSDBG("\n");
}

static void ethqos_ipa_rx_desc_init(struct qcom_ethqos *ethqos,
				    enum ipa_queue_type type)
{
	int cur_rx = 0;
	struct dma_desc *RX_NORMAL_DESC;
	int i;
	int start_index = cur_rx;
	int last_index;
	unsigned int VARRDES3 = 0;
	unsigned int qinx = eth_ipa_queue_type_to_rx_queue(type);

	ETHQOSDBG("\n");

	if (!eth_ipa_ctx.rx_queue[type]->desc_cnt)
		return;

	/* initialize all desc */
	for (i = 0; i < eth_ipa_ctx.rx_queue[type]->desc_cnt; i++) {
		RX_NORMAL_DESC = eth_ipa_ctx.rx_queue[type]->rx_desc_ptrs[i];
		memset(RX_NORMAL_DESC, 0, sizeof(struct dma_desc));
		/* update buffer 1 address pointer */
		RX_NORMAL_DESC->des0
		= cpu_to_le32(eth_ipa_ctx.rx_queue[type]->skb_dma[i]);

		/* set to zero  */
		RX_NORMAL_DESC->des1 = 0;

		/* set buffer 2 address pointer to zero */
		RX_NORMAL_DESC->des2 = 0;
		/* set control bits - OWN, INTE and BUF1V */
		RX_NORMAL_DESC->des3 = cpu_to_le32(0xc1000000);

		/* Don't Set the IOC bit for IPA controlled Desc */
		VARRDES3 = le32_to_cpu(RX_NORMAL_DESC->des3);

		/* reset IOC as per rx intr moderation count */
		if (!eth_ipa_ctx.rx_intr_mod_cnt[type] ||
		    (eth_ipa_ctx.rx_intr_mod_cnt[type] &&
		     (i  % eth_ipa_ctx.rx_intr_mod_cnt[type])))
			RX_NORMAL_DESC->des3 = cpu_to_le32(VARRDES3 &
							   ~(1 << 30));
	}

	/* update the total no of Rx descriptors count */
	DMA_RDRLR_RGWR(qinx, (eth_ipa_ctx.rx_queue[type]->desc_cnt - 1));
	/* update the Rx Descriptor Tail Pointer */
	last_index =
		GET_RX_CURRENT_RCVD_LAST_DESC_INDEX(
		start_index, 0,
		eth_ipa_ctx.rx_queue[type]->desc_cnt);
	DMA_RDTP_RPDR_RGWR(
		qinx,
		eth_ipa_ctx.rx_queue[type]->rx_desc_dma_addrs[last_index]);
	/* update the starting address of desc chain/ring */
	DMA_RDLAR_RGWR(
		qinx,
		eth_ipa_ctx.rx_queue[type]->rx_desc_dma_addrs[start_index]);

	ETHQOSDBG("\n");
}

static int ethqos_alloc_ipa_rx_buf(
	struct qcom_ethqos *ethqos, unsigned int i, gfp_t gfp,
	enum ipa_queue_type type)
{
	unsigned int rx_buffer_len;
	dma_addr_t ipa_rx_buf_dma_addr;
	struct sg_table *buff_sgt;
	int ret = 0;
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);

	ETHQOSDBG("\n");

	rx_buffer_len = eth_ipa_queue_type_to_buf_length(type);
	eth_ipa_ctx.rx_queue[type]->ipa_buff_va[i] = dma_alloc_coherent(
	   GET_MEM_PDEV_DEV, rx_buffer_len,
	   &ipa_rx_buf_dma_addr, GFP_KERNEL);

	if (!eth_ipa_ctx.rx_queue[type]->ipa_buff_va[i]) {
		dev_alert(&ethqos->pdev->dev,
			  "Failed to allocate RX dma buf for IPA\n");
		return -ENOMEM;
	}

	eth_ipa_ctx.rx_queue[type]->skb_dma[i] = ipa_rx_buf_dma_addr;

	buff_sgt = kzalloc(sizeof(*buff_sgt), GFP_KERNEL);
	if (buff_sgt) {
		ret = dma_get_sgtable(
			GET_MEM_PDEV_DEV, buff_sgt,
			eth_ipa_ctx.rx_queue[type]->ipa_buff_va[i],
			ipa_rx_buf_dma_addr,
			rx_buffer_len);
		if (ret == 0) {
			eth_ipa_ctx.rx_queue[type]->ipa_rx_buff_phy_addr[i]
			= sg_phys(buff_sgt->sgl);
			sg_free_table(buff_sgt);
		} else {
			ETHQOSERR("Failed to get sgtable for RX buffer\n");
		}
		kfree(buff_sgt);
		buff_sgt = NULL;
	} else {
		ETHQOSERR("Failed to allocate memory for RX buff sgtable.\n");
	}
	return 0;
	ETHQOSDBG("\n");
}

static void ethqos_wrapper_tx_descriptor_init_single_q(
			struct qcom_ethqos *ethqos,
			enum ipa_queue_type type)
{
	int i;
	struct dma_desc *desc = eth_ipa_ctx.tx_queue[type]->tx_desc_ptrs[0];
	dma_addr_t desc_dma = eth_ipa_ctx.tx_queue[type]->tx_desc_dma_addrs[0];
	void *ipa_tx_buf_vaddr;
	dma_addr_t ipa_tx_buf_dma_addr;
	struct sg_table *buff_sgt;
	int ret  = 0;
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);
	unsigned int qinx = eth_ipa_queue_type_to_tx_queue(type);
	ETHQOSDBG("qinx = %u\n", qinx);

	/* Allocate TX Buffer Pool Structure */
	eth_ipa_ctx.tx_queue[type]->ipa_tx_pa_addrs_base =
	dma_zalloc_coherent
	(GET_MEM_PDEV_DEV, sizeof(dma_addr_t) *
	 eth_ipa_ctx.tx_queue[type]->desc_cnt,
	 &eth_ipa_ctx.tx_queue[type]->ipa_tx_pa_addrs_base_dmahndl,
	 GFP_KERNEL);
	if (!eth_ipa_ctx.tx_queue[type]->ipa_tx_pa_addrs_base) {
		ETHQOSERR("ERROR: Unable to allocate IPA TX Buff structure\n");
		return;
	}

	ETHQOSDBG("IPA tx_dma_buff_addrs = %pK\n",
		  eth_ipa_ctx.tx_queue[type]->ipa_tx_pa_addrs_base);

	for (i = 0; i < eth_ipa_ctx.tx_queue[type]->desc_cnt; i++) {
		eth_ipa_ctx.tx_queue[type]->tx_desc_ptrs[i] = &desc[i];
		eth_ipa_ctx.tx_queue[type]->tx_desc_dma_addrs[i] =
		    (desc_dma + sizeof(struct dma_desc) * i);

		/* Create a memory pool for TX offload path */
		ipa_tx_buf_vaddr
		= dma_alloc_coherent(
			GET_MEM_PDEV_DEV,
			eth_ipa_queue_type_to_buf_length(type),
			&ipa_tx_buf_dma_addr, GFP_KERNEL);
		if (!ipa_tx_buf_vaddr) {
			ETHQOSERR("Failed to allocate TX buf for IPA\n");
			return;
		}
		eth_ipa_ctx.tx_queue[type]->ipa_tx_buff_pool_va_addrs_base[i]
		= ipa_tx_buf_vaddr;
		eth_ipa_ctx.tx_queue[type]->ipa_tx_pa_addrs_base[i]
		= ipa_tx_buf_dma_addr;
		buff_sgt = kzalloc(sizeof(*buff_sgt), GFP_KERNEL);
		if (buff_sgt) {
			ret = dma_get_sgtable(
				GET_MEM_PDEV_DEV, buff_sgt,
				ipa_tx_buf_vaddr,
				ipa_tx_buf_dma_addr,
				eth_ipa_queue_type_to_buf_length(type));
			if (ret == 0) {
				eth_ipa_ctx.tx_queue[type]->ipa_tx_phy_addr[i]
				= sg_phys(buff_sgt->sgl);
				sg_free_table(buff_sgt);
			} else {
				ETHQOSERR("Failed to get sg RX Table\n");
			}
			kfree(buff_sgt);
			buff_sgt = NULL;
		} else {
			ETHQOSERR("Failed to get  RX sg buffer.\n");
		}
	}
	if (ethqos->ipa_enabled &&
	    qinx == eth_ipa_queue_type_to_tx_queue(type))
		ETHQOSDBG("DMA MAPed the virtual address for %d descs\n",
			  eth_ipa_ctx.tx_queue[type]->desc_cnt);

	ethqos_ipa_tx_desc_init(ethqos, type);
}

static void ethqos_wrapper_rx_descriptor_init_single_q(
			struct qcom_ethqos *ethqos,
			enum ipa_queue_type type)
{
	int i;
	struct dma_desc *desc = eth_ipa_ctx.rx_queue[type]->rx_desc_ptrs[0];
	dma_addr_t desc_dma = eth_ipa_ctx.rx_queue[type]->rx_desc_dma_addrs[0];
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);
	unsigned int qinx = eth_ipa_queue_type_to_rx_queue(type);

	ETHQOSDBG("\n");
	/* Allocate RX Buffer Pool Structure */
	if (!eth_ipa_ctx.rx_queue[type]->ipa_rx_pa_addrs_base) {
		eth_ipa_ctx.rx_queue[type]->ipa_rx_pa_addrs_base =
		dma_zalloc_coherent
		(GET_MEM_PDEV_DEV,
		 sizeof(dma_addr_t) *
		 eth_ipa_ctx.rx_queue[type]->desc_cnt,
		 &eth_ipa_ctx.rx_queue[type]->ipa_rx_pa_addrs_base_dmahndl,
		 GFP_KERNEL);
		if (!eth_ipa_ctx.rx_queue[type]->ipa_rx_pa_addrs_base) {
			ETHQOSERR("Unable to allocate structure\n");
			return;
		}

		ETHQOSERR
		("IPA rx_buff_addrs %pK\n",
		 eth_ipa_ctx.rx_queue[type]->ipa_rx_pa_addrs_base);
	}

	for (i = 0; i < eth_ipa_ctx.rx_queue[type]->desc_cnt; i++) {
		eth_ipa_ctx.rx_queue[type]->rx_desc_ptrs[i] = &desc[i];
		eth_ipa_ctx.rx_queue[type]->rx_desc_dma_addrs[i] =
		    (desc_dma + sizeof(struct dma_desc) * i);

		/* allocate skb & assign to each desc */
		if (ethqos_alloc_ipa_rx_buf(ethqos, i, GFP_KERNEL, type))
			break;

		/* Assign the RX memory pool for offload data path */
		eth_ipa_ctx.rx_queue[type]->ipa_rx_buff_pool_va_addrs_base[i]
		= eth_ipa_ctx.rx_queue[type]->ipa_buff_va[i];
		eth_ipa_ctx.rx_queue[type]->ipa_rx_pa_addrs_base[i]
		= eth_ipa_ctx.rx_queue[type]->skb_dma[i];

		/* alloc_rx_buf */
		wmb();
	}

	ETHQOSDBG(
		"Allocated %d buffers for RX Channel: %d\n",
		eth_ipa_ctx.rx_queue[type]->desc_cnt, qinx);
	ETHQOSDBG(
		"virtual memory pool address for RX for %d desc\n",
		eth_ipa_ctx.rx_queue[type]->desc_cnt);

	ethqos_ipa_rx_desc_init(ethqos, type);
}

static void ethqos_rx_skb_free_mem(struct qcom_ethqos *ethqos,
				   enum ipa_queue_type type)
{
	struct net_device *ndev;
	struct stmmac_priv *priv;
	int i;

	if (!ethqos) {
		ETHQOSERR("Null parameter");
		return;
	}

	ndev = dev_get_drvdata(&ethqos->pdev->dev);
	priv = netdev_priv(ndev);

	for (i = 0; i < eth_ipa_ctx.rx_queue[type]->desc_cnt; i++) {
		dma_free_coherent
		 (GET_MEM_PDEV_DEV,
		 eth_ipa_queue_type_to_buf_length(type),
		 eth_ipa_ctx.rx_queue[type]->ipa_rx_buff_pool_va_addrs_base[i],
		 eth_ipa_ctx.rx_queue[type]->ipa_rx_pa_addrs_base[i]);
	}
}

static void ethqos_free_ipa_queue_mem(struct qcom_ethqos *ethqos)
{
	int type;

	for (type = 0; type < IPA_QUEUE_MAX; type++) {
		if (eth_ipa_queue_type_enabled(type)) {
			ethqos_rx_desc_free_mem(ethqos, type);
			ethqos_tx_desc_free_mem(ethqos, type);
			ethqos_rx_skb_free_mem(ethqos, type);
			ethqos_rx_buf_free_mem(ethqos, type);
			ethqos_tx_buf_free_mem(ethqos, type);
			ethqos_free_ipa_rx_queue_struct(ethqos, type);
			ethqos_free_ipa_tx_queue_struct(ethqos, type);
		}
	}
}

static int ethqos_set_ul_dl_smmu_ipa_params(struct qcom_ethqos *ethqos,
					    struct ipa_ntn_setup_info *ul,
					    struct ipa_ntn_setup_info *dl,
						enum ipa_queue_type type)
{
	int ret = 0;
	struct platform_device *pdev;
	struct net_device *dev;
	struct stmmac_priv *priv;

	if (!ethqos) {
		ETHQOSERR("Null Param\n");
		ret = -1;
		return ret;
	}

	if (!ul || !dl) {
		ETHQOSERR("Null UL DL params\n");
		ret = -1;
		return ret;
	}

	pdev = ethqos->pdev;
	dev = platform_get_drvdata(pdev);
	priv = netdev_priv(dev);

	ul->ring_base_sgt = kzalloc(sizeof(ul->ring_base_sgt), GFP_KERNEL);
	if (!ul->ring_base_sgt) {
		ETHQOSERR("Failed to allocate memory for IPA UL ring sgt\n");
	return -ENOMEM;
	}

	ret = dma_get_sgtable(GET_MEM_PDEV_DEV, ul->ring_base_sgt,
			      eth_ipa_ctx.rx_queue[type]->rx_desc_ptrs[0],
			      eth_ipa_ctx.rx_queue[type]->rx_desc_dma_addrs[0],
			      (sizeof(struct dma_desc) *
			      eth_ipa_ctx.rx_queue[type]->desc_cnt));
	if (ret) {
		ETHQOSERR("Failed to get IPA UL ring sgtable.\n");
		kfree(ul->ring_base_sgt);
		ul->ring_base_sgt = NULL;
		ret = -1;
		goto fail;
	} else {
		ul->ring_base_pa = sg_phys(ul->ring_base_sgt->sgl);
	}

	ul->buff_pool_base_sgt = kzalloc(sizeof(ul->buff_pool_base_sgt),
					 GFP_KERNEL);
	if (!ul->buff_pool_base_sgt) {
		ETHQOSERR("Failed to allocate memory for IPA UL buff pool\n");
		return -ENOMEM;
	}

	ret = dma_get_sgtable
		(GET_MEM_PDEV_DEV, ul->buff_pool_base_sgt,
		 eth_ipa_ctx.rx_queue[type]->ipa_rx_pa_addrs_base,
		 eth_ipa_ctx.rx_queue[type]->ipa_rx_pa_addrs_base_dmahndl,
		 (sizeof(struct dma_desc) *
		 eth_ipa_ctx.rx_queue[type]->desc_cnt));
	if (ret) {
		ETHQOSERR("Failed to get IPA UL buff pool sgtable.\n");
		kfree(ul->buff_pool_base_sgt);
		ul->buff_pool_base_sgt = NULL;
		ret = -1;
		goto fail;
	} else {
		ul->buff_pool_base_pa = sg_phys(ul->buff_pool_base_sgt->sgl);
	}

	dl->ring_base_sgt = kzalloc(sizeof(dl->ring_base_sgt),
				    GFP_KERNEL);
	if (!dl->ring_base_sgt) {
		ETHQOSERR("Failed to allocate memory for IPA DL ring sgt\n");
		return -ENOMEM;
	}

	ret = dma_get_sgtable(GET_MEM_PDEV_DEV, dl->ring_base_sgt,
			      eth_ipa_ctx.tx_queue[type]->tx_desc_ptrs[0],
			      eth_ipa_ctx.tx_queue[type]->tx_desc_dma_addrs[0],
			      (sizeof(struct dma_desc) *
			      eth_ipa_ctx.tx_queue[type]->desc_cnt));
	if (ret) {
		ETHQOSERR("Failed to get IPA DL ring sgtable.\n");
		kfree(dl->ring_base_sgt);
		dl->ring_base_sgt = NULL;
		ret = -1;
		goto fail;
	} else {
		dl->ring_base_pa = sg_phys(dl->ring_base_sgt->sgl);
	}

	dl->buff_pool_base_sgt = kzalloc(sizeof(dl->buff_pool_base_sgt),
					 GFP_KERNEL);
	if (!dl->buff_pool_base_sgt) {
		ETHQOSERR("Failed to allocate memory for IPA DL buff pool\n");
		return -ENOMEM;
	}
	ret = dma_get_sgtable
		(GET_MEM_PDEV_DEV, dl->buff_pool_base_sgt,
		 eth_ipa_ctx.tx_queue[type]->ipa_tx_pa_addrs_base,
		 eth_ipa_ctx.tx_queue[type]->ipa_tx_pa_addrs_base_dmahndl,
		 (sizeof(struct dma_desc) *
		 eth_ipa_ctx.tx_queue[type]->desc_cnt));
	if (ret) {
		ETHQOSERR("Failed to get IPA DL buff pool sgtable.\n");
		kfree(dl->buff_pool_base_sgt);
		dl->buff_pool_base_sgt = NULL;
		ret = -1;
		goto fail;
	} else {
		dl->buff_pool_base_pa = sg_phys(dl->buff_pool_base_sgt->sgl);
	}
fail:
	return ret;
}

static int enable_tx_dma_interrupts(
	unsigned int QINX, struct qcom_ethqos *ethqos,
	bool enable_sw_intr)
{
	unsigned int tmp;
	unsigned long VARDMA_SR;
	unsigned long VARDMA_IER;
	unsigned long DMA_TX_INT_MASK = 0xFC07;
	unsigned long DMA_TX_INT_RESET_MASK = 0xFBC0;

	/* clear all the interrupts which are set */
	DMA_SR_RGRD(QINX, VARDMA_SR);
	tmp = VARDMA_SR & DMA_TX_INT_MASK;
	DMA_SR_RGWR(QINX, tmp);

	/* Enable following interrupts for Queue */
	/* NIE - Normal Interrupt Summary Enable */
	/* AIE - Abnormal Interrupt Summary Enable */
	/* FBE - Fatal Bus Error Enable */
	DMA_IER_RGRD(QINX, VARDMA_IER);
	/* Reset all Tx interrupt bits */
	VARDMA_IER = VARDMA_IER & DMA_TX_INT_RESET_MASK;

	VARDMA_IER = VARDMA_IER | ((0x1) << 12) | ((0x1) << 14) |
		     ((0x1) << 15);

	if (enable_sw_intr)
		VARDMA_IER |= ((0x1) << 0);

	DMA_IER_RGWR(QINX, VARDMA_IER);

	return 0;
}

static int enable_rx_dma_interrupts(
	unsigned int QINX, struct qcom_ethqos *ethqos,
	bool enable_sw_intr)
{
	unsigned int tmp;
	unsigned long VARDMA_SR;
	unsigned long VARDMA_IER;
	unsigned long DMA_RX_INT_MASK = 0xFBC0;
	unsigned long DMA_RX_INT_RESET_MASK = 0xF407;

	/* clear all the interrupts which are set */
	DMA_SR_RGRD(QINX, VARDMA_SR);
	tmp = VARDMA_SR & DMA_RX_INT_MASK;
	DMA_SR_RGWR(QINX, tmp);
	/* Enable following interrupts for Queue 0 */
	/* NIE - Normal Interrupt Summary Enable */
	/* AIE - Abnormal Interrupt Summary Enable */
	/* FBE - Fatal Bus Error Enable */
	/* RIE - Receive Interrupt Enable */
	/* RSE - Receive Stopped Enable */
	DMA_IER_RGRD(QINX, VARDMA_IER);
	/* Reset all Rx interrupt bits */
	VARDMA_IER = VARDMA_IER & (unsigned long)(DMA_RX_INT_RESET_MASK);

	VARDMA_IER = VARDMA_IER | ((0x1) << 14) | ((0x1) << 12) |
	    ((0x1) << 15);

	if (enable_sw_intr)
		VARDMA_IER |= ((0x1) << 6);

	DMA_IER_RGWR(QINX, VARDMA_IER);

	return 0;
}

static void ethqos_ipa_config_queues(struct qcom_ethqos *ethqos)
{
	int type;

	for (type = 0; type < IPA_QUEUE_MAX; type++) {
		if (eth_ipa_queue_type_enabled(type)) {
			ethqos_alloc_ipa_tx_queue_struct(ethqos, type);
			ethqos_alloc_ipa_rx_queue_struct(ethqos, type);
			allocate_ipa_buffer_and_desc(ethqos, type);
			ethqos_wrapper_tx_descriptor_init_single_q(
				ethqos, type);
			ethqos_wrapper_rx_descriptor_init_single_q(
				ethqos, type);
		}
	}
}

static void ethqos_configure_ipa_tx_dma_channel(
	struct qcom_ethqos *ethqos, enum ipa_queue_type type)
{
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);
	unsigned int qinx = eth_ipa_queue_type_to_tx_queue(type);

	ETHQOSDBG("\n");

	enable_tx_dma_interrupts(
		qinx, ethqos,
		eth_ipa_queue_type_to_tx_intr_route(type) == IPA_INTR_ROUTE_DB);
	/* Enable Operate on Second Packet for better tputs */
	DMA_TCR_OSP_UDFWR(qinx, 0x1);

	/* start TX DMA */
	priv->hw->dma->start_tx(priv->ioaddr, qinx);

	ETHQOSDBG("\n");
}

static void ethqos_configure_ipa_rx_dma_channel(
	struct qcom_ethqos *ethqos, enum ipa_queue_type type)
{
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);
	unsigned int qinx = eth_ipa_queue_type_to_rx_queue(type);

	ETHQOSERR("\n");

	/* Select Rx Buffer size */
	DMA_RCR_RBSZ_UDFWR(qinx, eth_ipa_queue_type_to_buf_length(type));

	enable_rx_dma_interrupts(
		qinx,
		ethqos,
		eth_ipa_queue_type_to_rx_intr_route(type) == IPA_INTR_ROUTE_DB);

	/* start RX DMA */
	priv->hw->dma->start_rx(priv->ioaddr, qinx);

	ETHQOSERR("\n");
}

static int ethqos_init_offload(struct qcom_ethqos *ethqos,
			       enum ipa_queue_type type)
{
	struct stmmac_priv *priv = ethqos_get_priv(ethqos);

	ETHQOSDBG("\n");

	ethqos_configure_ipa_tx_dma_channel(ethqos, type);
	priv->hw->mac->map_mtl_to_dma(
		priv->hw,
		eth_ipa_queue_type_to_rx_queue(type),
		eth_ipa_queue_type_to_rx_queue(type));
	ethqos_configure_ipa_rx_dma_channel(ethqos, type);

	ETHQOSDBG("\n");
	return 0;
}

static void ntn_ipa_notify_cb_cv2x(
	void *priv, enum ipa_dp_evt_type evt, unsigned long data)
{
	WARN_ON(1);
}

static void ntn_ipa_notify_cb_be(
	void *priv, enum ipa_dp_evt_type evt, unsigned long data)
{
	struct qcom_ethqos *ethqos = eth_ipa_ctx.ethqos;
	struct ethqos_prv_ipa_data *eth_ipa = &eth_ipa_ctx;
	struct sk_buff *skb = (struct sk_buff *)data;
	struct iphdr *iph = NULL;
	int stat = NET_RX_SUCCESS;
	struct platform_device *pdev;
	struct net_device *dev;
	struct stmmac_priv *pdata;

	if (!ethqos || !skb) {
		ETHQOSERR("Null Param pdata %p skb %pK\n",  ethqos, skb);
		return;
	}

	if (!eth_ipa) {
		ETHQOSERR("Null Param eth_ipa %pK\n", eth_ipa);
		return;
	}

	if (!eth_ipa->ipa_offload_conn) {
		ETHQOSERR("ipa_cb before offload is ready %d\n",
			  eth_ipa->ipa_offload_conn);
		return;
	}

	pdev = ethqos->pdev;
	dev =  platform_get_drvdata(pdev);
	pdata = netdev_priv(dev);

	if (evt == IPA_RECEIVE) {
		/*Exception packets to network stack*/
		skb->dev = dev;
		skb_record_rx_queue(
			skb,
			eth_ipa_queue_type_to_rx_queue(IPA_QUEUE_BE));

		if (true == *(u8 *)(skb->cb + 4)) {
			skb->protocol = htons(ETH_P_IP);
			iph = (struct iphdr *)skb->data;
		} else {
			if (ethqos->current_loopback > DISABLE_LOOPBACK)
				swap_ip_port(skb, ETH_P_IP);
			skb->protocol = eth_type_trans(skb, skb->dev);
			iph = (struct iphdr *)(skb_mac_header(skb) + ETH_HLEN);
		}

		/* Submit packet to network stack */
		/* If its a ping packet submit it via rx_ni else use rx */
		if (iph->protocol == IPPROTO_ICMP) {
			stat = netif_rx_ni(skb);
		} else if ((dev->stats.rx_packets %
				16) == 0){
			stat = netif_rx_ni(skb);
		} else {
			stat = netif_rx(skb);
		}

		if (stat == NET_RX_DROP) {
			dev->stats.rx_dropped++;
		} else {
			/* Update Statistics */
			eth_ipa_ctx.ipa_stats[IPA_QUEUE_BE].ipa_ul_exception++;
			dev->stats.rx_packets++;
			dev->stats.rx_bytes += skb->len;
		}
	} else {
		ETHQOSERR("Unhandled Evt %d ", evt);
		dev_kfree_skb_any(skb);
		skb = NULL;
		dev->stats.tx_dropped++;
	}
}

static int ethqos_ipa_offload_init(
	struct qcom_ethqos *pdata, enum ipa_queue_type type)
{
	struct ipa_uc_offload_intf_params in;
	struct ipa_uc_offload_out_params out;
	struct ethqos_prv_ipa_data *eth_ipa = &eth_ipa_ctx;
	struct ethhdr eth_l2_hdr_v4;
	struct ethhdr eth_l2_hdr_v6;
#ifdef ETHQOS_IPA_OFFLOAD_VLAN
	struct vlan_ethhdr eth_vlan_hdr_v4;
	struct vlan_ethhdr eth_vlan_hdr_v6;
#endif
	bool ipa_vlan_mode;
	int ret;

	if (!pdata) {
		ETHQOSERR("Null Param\n");
		ret =  -1;
		return ret;
	}

	ipa_vlan_mode = eth_ipa_queue_type_to_ipa_vlan_mode(type);

	ETHQOSDBG("IPA VLAN mode %d\n", ipa_vlan_mode);

	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));

	/* Building ETH Header */
	if (!eth_ipa_ctx.vlan_id || !ipa_vlan_mode) {
		memset(&eth_l2_hdr_v4, 0, sizeof(eth_l2_hdr_v4));
		memset(&eth_l2_hdr_v6, 0, sizeof(eth_l2_hdr_v6));
		memcpy(
			&eth_l2_hdr_v4.h_source,
			eth_ipa_queue_type_to_device_addr(type), ETH_ALEN);
		eth_l2_hdr_v4.h_proto = htons(ETH_P_IP);
		memcpy(
			&eth_l2_hdr_v6.h_source,
			eth_ipa_queue_type_to_device_addr(type), ETH_ALEN);
		eth_l2_hdr_v6.h_proto = htons(ETH_P_IPV6);
		in.hdr_info[0].hdr = (u8 *)&eth_l2_hdr_v4;
		in.hdr_info[0].hdr_len = ETH_HLEN;
		in.hdr_info[0].hdr_type = IPA_HDR_L2_ETHERNET_II;
		in.hdr_info[1].hdr = (u8 *)&eth_l2_hdr_v6;
		in.hdr_info[1].hdr_len = ETH_HLEN;
		in.hdr_info[1].hdr_type = IPA_HDR_L2_ETHERNET_II;
	}

#ifdef ETHQOS_IPA_OFFLOAD_VLAN
	if (ipa_vlan_mode) {
		memset(&eth_vlan_hdr_v4, 0, sizeof(eth_vlan_hdr_v4));
		memset(&eth_vlan_hdr_v6, 0, sizeof(eth_vlan_hdr_v6));
		memcpy(
			&eth_vlan_hdr_v4.h_source,
			eth_ipa_queue_type_to_device_addr(type), ETH_ALEN);
		eth_vlan_hdr_v4.h_vlan_proto = htons(ETH_P_8021Q);
		eth_vlan_hdr_v4.h_vlan_encapsulated_proto = htons(ETH_P_IP);
		in.hdr_info[0].hdr = (u8 *)&eth_vlan_hdr_v4;
		in.hdr_info[0].hdr_len = VLAN_ETH_HLEN;
		in.hdr_info[0].hdr_type = IPA_HDR_L2_802_1Q;
		memcpy(
			&eth_vlan_hdr_v6.h_source,
			eth_ipa_queue_type_to_device_addr(type), ETH_ALEN);
		eth_vlan_hdr_v6.h_vlan_proto = htons(ETH_P_8021Q);
		eth_vlan_hdr_v6.h_vlan_encapsulated_proto = htons(ETH_P_IPV6);
		in.hdr_info[1].hdr = (u8 *)&eth_vlan_hdr_v6;
		in.hdr_info[1].hdr_len = VLAN_ETH_HLEN;
		in.hdr_info[1].hdr_type = IPA_HDR_L2_802_1Q;
	}
#endif

	/* Building IN params */
	in.netdev_name = eth_ipa_queue_type_to_device_name(type);
	in.priv = pdata;
	in.notify = eth_ipa_queue_type_to_ipa_notify_cb(type);
	in.proto = eth_ipa_queue_type_to_proto(type);
	in.hdr_info[0].dst_mac_addr_offset = 0;
	in.hdr_info[1].dst_mac_addr_offset = 0;

	ret = ipa_uc_offload_reg_intf(&in, &out);
	if (ret) {
		ETHQOSERR("Could not register offload interface ret %d\n",
			  ret);
		ret = -1;
		return ret;
	}
	eth_ipa->ipa_client_hndl[type] = out.clnt_hndl;
	ETHQOSDBG("Recevied IPA Offload Client Handle %d",
		  eth_ipa->ipa_client_hndl[type]);

	return 0;
}

static int ethqos_ipa_offload_cleanup(
	struct qcom_ethqos *ethqos, enum ipa_queue_type type)
{
	struct ethqos_prv_ipa_data *eth_ipa = &eth_ipa_ctx;
	int ret = 0;

	ETHQOSERR("begin\n");

	if (!ethqos) {
		ETHQOSERR("Null Param\n");
		return -ENOMEM;
	}

	if (!eth_ipa->ipa_client_hndl[type]) {
		ETHQOSERR("cleanup called with NULL IPA client handle\n");
		return -ENOMEM;
	}

	ret = ipa_uc_offload_cleanup(eth_ipa->ipa_client_hndl[type]);
	if (ret) {
		ETHQOSERR("Could not cleanup IPA Offload ret %d\n", ret);
		ret = -1;
	} else {
		if (eth_ipa_queue_type_to_send_msg_needed(type))
			eth_ipa_send_msg(
				ethqos, IPA_PERIPHERAL_DISCONNECT, type);
	}

	ETHQOSERR("end\n");

	return ret;
}

static ssize_t read_ipa_offload_status(struct file *file,
				       char __user *user_buf, size_t count,
				       loff_t *ppos)
{
	unsigned int len = 0, buf_len = NTN_IPA_DBG_MAX_MSG_LEN;
	struct qcom_ethqos *ethqos = file->private_data;

	if (qcom_ethqos_is_phy_link_up(ethqos)) {
		if (eth_ipa_ctx.ipa_offload_susp)
			len += scnprintf(buf + len, buf_len - len,
					 "IPA Offload suspended\n");
		else
			len += scnprintf(buf + len, buf_len - len,
					 "IPA Offload enabled\n");
	} else {
		len += scnprintf(buf + len, buf_len - len,
				 "Cannot read status, No PHY link\n");
	}

	if (len > buf_len)
		len = buf_len;

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

/* Generic Bit descirption; reset = 0, set = 1*/
static char * const bit_status_string[] = {
	"Reset",
	"Set",
};

/* Generic Bit Mask Description; masked = 0, enabled = 1*/
static char * const bit_mask_string[] = {
	"Masked",
	"Enable",
};

static ssize_t suspend_resume_ipa_offload(struct file *file,
					  const char __user *user_buf,
					  size_t count, loff_t *ppos)
{
	s8 option = 0;
	char in_buf[2];
	unsigned long ret;
	struct qcom_ethqos *ethqos = file->private_data;
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);

	if (sizeof(in_buf) < 2)
		return -EFAULT;

	ret = copy_from_user(in_buf, user_buf, 1);
	if (ret)
		return -EFAULT;

	in_buf[1] = '\0';
	if (kstrtos8(in_buf, 0, &option))
		return -EFAULT;

	if (qcom_ethqos_is_phy_link_up(ethqos)) {
		if (option == 1)
			ethqos_ipa_offload_event_handler(priv, EV_USR_SUSPEND);
		else if (option == 0)
			ethqos_ipa_offload_event_handler(priv, EV_USR_RESUME);
	} else {
		ETHQOSERR("Operation not permitted, No PHY link");
	}

	return count;
}

static ssize_t read_ipa_stats(struct file *file,
			      char __user *user_buf, size_t count,
			      loff_t *ppos)
{
	struct qcom_ethqos *ethqos = file->private_data;
	struct ethqos_prv_ipa_data *eth_ipa = &eth_ipa_ctx;
	char *buf;
	unsigned int len = 0, buf_len = 2000;
	ssize_t ret_cnt;

	if (!ethqos || !eth_ipa) {
		ETHQOSERR("NULL Pointer\n");
		return -EINVAL;
	}

	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += scnprintf(buf + len, buf_len - len, "\n\n");
	len += scnprintf(buf + len, buf_len - len, "%25s\n",
		"NTN IPA Stats");
	len += scnprintf(buf + len, buf_len - len, "%25s\n\n",
		"==================================================");

	len += scnprintf(buf + len, buf_len - len, "%-25s %10llu\n",
			 "IPA RX Packets: ",
			 eth_ipa_ctx.ipa_stats[IPA_QUEUE_BE].ipa_ul_exception);
	len += scnprintf(buf + len, buf_len - len, "\n");

	if (len > buf_len)
		len = buf_len;

	ret_cnt = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret_cnt;
}

static void ethqos_ipa_stats_read(struct qcom_ethqos *ethqos,
				  enum ipa_queue_type type)
{
	struct ethqos_ipa_stats *dma_stats = &eth_ipa_ctx.ipa_stats[type];
	unsigned int data;

	if (!eth_ipa_ctx.rx_queue[type] || !eth_ipa_ctx.tx_queue[type])
		return;

	dma_stats->ipa_rx_desc_ring_base
	= eth_ipa_ctx.rx_queue[type]->rx_desc_dma_addrs[0];
	dma_stats->ipa_rx_desc_ring_size
	= eth_ipa_ctx.rx_queue[type]->desc_cnt;
	dma_stats->ipa_rx_buff_ring_base
	= eth_ipa_ctx.rx_queue[type]->ipa_rx_pa_addrs_base_dmahndl;
	dma_stats->ipa_rx_buff_ring_size
	= eth_ipa_ctx.rx_queue[type]->desc_cnt - 1;

	dma_stats->ipa_rx_db_int_raised = 0;

	DMA_CHRDR_RGRD(eth_ipa_queue_type_to_rx_queue(type), data);
	dma_stats->ipa_rx_cur_desc_ptr_indx = GET_RX_DESC_IDX(
		type, data);

	DMA_RDTP_RPDR_RGRD(eth_ipa_queue_type_to_rx_queue(type), data);
	dma_stats->ipa_rx_tail_ptr_indx = GET_RX_DESC_IDX(
		type, data);

	DMA_SR_RGRD(eth_ipa_queue_type_to_rx_queue(type), data);
	dma_stats->ipa_rx_dma_status = data;

	dma_stats->ipa_rx_dma_ch_underflow =
	GET_VALUE(data, DMA_SR_RBU_LPOS, DMA_SR_RBU_LPOS, DMA_SR_RBU_HPOS);

	dma_stats->ipa_rx_dma_ch_stopped =
	GET_VALUE(data, DMA_SR_RPS_LPOS, DMA_SR_RPS_LPOS, DMA_SR_RPS_HPOS);

	dma_stats->ipa_rx_dma_ch_complete =
	GET_VALUE(data, DMA_SR_RI_LPOS, DMA_SR_RI_LPOS, DMA_SR_RI_HPOS);

	DMA_IER_RGRD(
		eth_ipa_queue_type_to_rx_queue(type),
		dma_stats->ipa_rx_int_mask);

	DMA_IER_RBUE_UDFRD(
		eth_ipa_queue_type_to_rx_queue(type),
		dma_stats->ipa_rx_underflow_irq);
	DMA_IER_ETIE_UDFRD(
		eth_ipa_queue_type_to_rx_queue(type),
		dma_stats->ipa_rx_early_trans_comp_irq);

	dma_stats->ipa_tx_desc_ring_base
	= eth_ipa_ctx.tx_queue[type]->tx_desc_dma_addrs[0];
	dma_stats->ipa_tx_desc_ring_size
	= eth_ipa_ctx.tx_queue[type]->desc_cnt;
	dma_stats->ipa_tx_buff_ring_base
	= eth_ipa_ctx.tx_queue[type]->ipa_tx_pa_addrs_base_dmahndl;
	dma_stats->ipa_tx_buff_ring_size
	= eth_ipa_ctx.tx_queue[type]->desc_cnt - 1;

	dma_stats->ipa_tx_db_int_raised = 0;

	DMA_CHTDR_RGRD(eth_ipa_queue_type_to_tx_queue(type), data);
	dma_stats->ipa_tx_curr_desc_ptr_indx =
		GET_TX_DESC_IDX(type, data);

	DMA_TDTP_TPDR_RGRD(eth_ipa_queue_type_to_tx_queue(type), data);
	dma_stats->ipa_tx_tail_ptr_indx =
		GET_TX_DESC_IDX(type, data);

	DMA_SR_RGRD(eth_ipa_queue_type_to_tx_queue(type), data);
	dma_stats->ipa_tx_dma_status = data;

	dma_stats->ipa_tx_dma_ch_underflow =
	GET_VALUE(data, DMA_SR_TBU_LPOS, DMA_SR_TBU_LPOS, DMA_SR_TBU_HPOS);

	dma_stats->ipa_tx_dma_transfer_stopped =
	GET_VALUE(data, DMA_SR_TPS_LPOS, DMA_SR_TPS_LPOS, DMA_SR_TPS_HPOS);

	dma_stats->ipa_tx_dma_transfer_complete =
	GET_VALUE(data, DMA_SR_TI_LPOS, DMA_SR_TI_LPOS, DMA_SR_TI_HPOS);

	DMA_IER_RGRD(
		eth_ipa_queue_type_to_tx_queue(type),
		dma_stats->ipa_tx_int_mask);
	DMA_IER_TIE_UDFRD(
		eth_ipa_queue_type_to_tx_queue(type),
		dma_stats->ipa_tx_transfer_complete_irq);

	DMA_IER_TXSE_UDFRD(
		eth_ipa_queue_type_to_tx_queue(type),
		dma_stats->ipa_tx_transfer_stopped_irq);

	DMA_IER_TBUE_UDFRD(
		eth_ipa_queue_type_to_tx_queue(type),
		dma_stats->ipa_tx_underflow_irq);

	DMA_IER_ETIE_UDFRD(
		eth_ipa_queue_type_to_tx_queue(type),
		dma_stats->ipa_tx_early_trans_cmp_irq);

	DMA_IER_FBEE_UDFRD(
		eth_ipa_queue_type_to_tx_queue(type),
		dma_stats->ipa_tx_fatal_err_irq);

	DMA_IER_CDEE_UDFRD(
		eth_ipa_queue_type_to_tx_queue(type),
		dma_stats->ipa_tx_desc_err_irq);
}

/**
 * read_ntn_dma_stats() - Debugfs read command for NTN DMA statistics
 * Only read DMA Stats for IPA Control Channels
 *
 */
static ssize_t read_ntn_dma_stats(struct file *file,
				  char __user *user_buf, size_t count,
				  loff_t *ppos)
{
	struct qcom_ethqos *ethqos = file->private_data;
	struct ethqos_prv_ipa_data *eth_ipa = &eth_ipa_ctx;
	struct ethqos_ipa_stats *dma_stats;
	char *buf;
	unsigned int len = 0, buf_len = 6000;
	ssize_t ret_cnt;
	int type;

	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (type = 0; type < IPA_QUEUE_MAX; type++) {
		if (!eth_ipa_queue_type_enabled(type))
			continue;

		ethqos_ipa_stats_read(ethqos, type);
		dma_stats = &eth_ipa_ctx.ipa_stats[type];

		len += scnprintf(buf + len, buf_len - len, "\n\n");
		len += scnprintf(buf + len, buf_len - len, "%25s\n",
				 "NTN DMA Stats");
		len += scnprintf(buf + len, buf_len - len, "%25s\n\n",
			"==================================================");

		len += scnprintf(buf + len, buf_len - len, "%-50s 0x%x\n",
				 "RX Desc Ring Base: ",
				 dma_stats->ipa_rx_desc_ring_base);
		len += scnprintf(buf + len, buf_len - len, "%-50s %10d\n",
				 "RX Desc Ring Size: ",
				 dma_stats->ipa_rx_desc_ring_size);
		len += scnprintf(buf + len, buf_len - len, "%-50s 0x%x\n",
				 "RX Buff Ring Base: ",
				 dma_stats->ipa_rx_buff_ring_base);
		len += scnprintf(buf + len, buf_len - len, "%-50s %10d\n",
				 "RX Buff Ring Size: ",
				 dma_stats->ipa_rx_buff_ring_size);
		len += scnprintf(buf + len, buf_len - len, "%-50s %10u\n",
				 "RX Doorbell Interrupts Raised: ",
				 dma_stats->ipa_rx_db_int_raised);
		len += scnprintf(buf + len, buf_len - len, "%-50s %10d\n",
				 "RX Current Desc Pointer Index: ",
				 dma_stats->ipa_rx_cur_desc_ptr_indx);
		len += scnprintf(buf + len, buf_len - len, "%-50s %10d\n",
				 "RX Tail Pointer Index: ",
				 dma_stats->ipa_rx_tail_ptr_indx);
		len += scnprintf(buf + len, buf_len - len, "%-50s 0x%x\n",
				 "RX Doorbell Address: ",
				 eth_ipa->uc_db_rx_addr[type]);
		len += scnprintf(buf + len, buf_len - len, "\n");

		len += scnprintf(buf + len, buf_len - len, "%-50s 0x%x\n",
				 "RX DMA Status: ",
				 dma_stats->ipa_rx_dma_status);

		len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
				 "RX DMA Status - RX DMA Underflow : ",
				 bit_status_string
				 [dma_stats->ipa_rx_dma_ch_underflow]);
		len += scnprintf(
			buf + len, buf_len - len, "%-50s %10s\n",
			"RX DMA Status - RX DMA Stopped : ",
			bit_status_string[dma_stats->ipa_rx_dma_ch_stopped]);
		len += scnprintf(
			buf + len, buf_len - len, "%-50s %10s\n",
			"RX DMA Status - RX DMA Complete : ",
			bit_status_string[dma_stats->ipa_rx_dma_ch_complete]);
		len += scnprintf(buf + len, buf_len - len, "\n");

		len += scnprintf(buf + len, buf_len - len, "%-50s 0x%x\n",
				 "RX DMA CH0 INT Mask: ",
				 dma_stats->ipa_rx_int_mask);
		len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
				 "RXDMACH0 INTMASK - Transfer Complete IRQ : ",
				 bit_mask_string
				 [dma_stats->ipa_rx_transfer_complete_irq]);
		len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
				 "RXDMACH0 INTMASK - Transfer Stopped IRQ : ",
				 bit_mask_string
				 [dma_stats->ipa_rx_transfer_stopped_irq]);
		len += scnprintf(
			buf + len, buf_len - len, "%-50s %10s\n",
			"RXDMACH0 INTMASK - Underflow IRQ : ",
			bit_mask_string[dma_stats->ipa_rx_underflow_irq]);
		len += scnprintf(
			buf + len, buf_len - len, "%-50s %10s\n",
			"RXDMACH0 INTMASK - Early Transmit Complete IRQ : ",
			bit_mask_string
			[dma_stats->ipa_rx_early_trans_comp_irq]);
		len += scnprintf(buf + len, buf_len - len, "\n");

		len += scnprintf(
			buf + len, buf_len - len, "%-50s 0x%x\n",
			"TX Desc Ring Base: ",
			dma_stats->ipa_tx_desc_ring_base);
		len += scnprintf(
			buf + len, buf_len - len, "%-50s %10d\n",
			"TX Desc Ring Size: ",
			dma_stats->ipa_tx_desc_ring_size);
		len += scnprintf(
			buf + len, buf_len - len, "%-50s 0x%x\n",
			"TX Buff Ring Base: ",
			dma_stats->ipa_tx_buff_ring_base);
		len += scnprintf(
			buf + len, buf_len - len, "%-50s %10d\n",
			"TX Buff Ring Size: ",
			dma_stats->ipa_tx_buff_ring_size);
		len += scnprintf(
			buf + len, buf_len - len, "%-50s %10u\n",
			"TX Doorbell Interrupts Raised: ",
			dma_stats->ipa_tx_db_int_raised);
		len += scnprintf(
			buf + len, buf_len - len, "%-50s %10lu\n",
			"TX Current Desc Pointer Index: ",
			dma_stats->ipa_tx_curr_desc_ptr_indx);

		len += scnprintf(
			buf + len, buf_len - len, "%-50s %10lu\n",
			"TX Tail Pointer Index: ",
			dma_stats->ipa_tx_tail_ptr_indx);
		len += scnprintf(
			buf + len, buf_len - len, "%-50s 0x%x\n",
			"TX Doorbell Address: ", eth_ipa->uc_db_tx_addr[type]);
		len += scnprintf(buf + len, buf_len - len, "\n");

		len += scnprintf(
			buf + len, buf_len - len, "%-50s 0x%x\n",
			"TX DMA Status: ", dma_stats->ipa_tx_dma_status);

		len += scnprintf(
			buf + len, buf_len - len, "%-50s %10s\n",
			"TX DMA Status - TX DMA Underflow : ",
			bit_status_string
			[dma_stats->ipa_tx_dma_ch_underflow]);
		len += scnprintf(
			buf + len, buf_len - len, "%-50s %10s\n",
			"TX DMA Status - TX DMA Transfer Stopped : ",
			bit_status_string
			[dma_stats->ipa_tx_dma_transfer_stopped]);
		len += scnprintf(
			buf + len, buf_len - len, "%-50s %10s\n",
			"TX DMA Status - TX DMA Transfer Complete : ",
			bit_status_string
			[dma_stats->ipa_tx_dma_transfer_complete]);
		len += scnprintf(buf + len, buf_len - len, "\n");

		len += scnprintf(
			buf + len, buf_len - len, "%-50s 0x%x\n",
			"TX DMA CH2 INT Mask: ", dma_stats->ipa_tx_int_mask);
		len += scnprintf(
			buf + len, buf_len - len, "%-50s %10s\n",
			"TXDMACH2 INTMASK - Transfer Complete IRQ : ",
			bit_mask_string
			[dma_stats->ipa_tx_transfer_complete_irq]);
		len += scnprintf(
			buf + len, buf_len - len, "%-50s %10s\n",
			"TXDMACH2 INTMASK - Transfer Stopped IRQ : ",
			bit_mask_string
			[dma_stats->ipa_tx_transfer_stopped_irq]);
		len += scnprintf(
			buf + len, buf_len - len, "%-50s %10s\n",
			"TXDMACH2 INTMASK - Underflow IRQ : ",
			bit_mask_string[dma_stats->ipa_tx_underflow_irq]);
		len += scnprintf(
			buf + len, buf_len - len, "%-50s %10s\n",
			"TXDMACH2 INTMASK - Early Transmit Complete IRQ : ",
			bit_mask_string
			[dma_stats->ipa_tx_early_trans_cmp_irq]);
		len += scnprintf(
			buf + len, buf_len - len, "%-50s %10s\n",
			"TXDMACH2 INTMASK - Fatal Bus Error IRQ : ",
			bit_mask_string[dma_stats->ipa_tx_fatal_err_irq]);
		len += scnprintf(
			buf + len, buf_len - len, "%-50s %10s\n",
			"TXDMACH2 INTMASK - CNTX Desc Error IRQ : ",
			bit_mask_string[dma_stats->ipa_tx_desc_err_irq]);
	}

	if (len > buf_len)
		len = buf_len;

	ret_cnt = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret_cnt;
}

static const struct file_operations fops_ipa_stats = {
	.read = read_ipa_stats,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static const struct file_operations fops_ntn_dma_stats = {
	.read = read_ntn_dma_stats,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static const struct file_operations fops_ntn_ipa_offload_en = {
	.read = read_ipa_offload_status,
	.write = suspend_resume_ipa_offload,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static int ethqos_ipa_cleanup_debugfs(struct qcom_ethqos *ethqos)
{
	struct ethqos_prv_ipa_data *eth_ipa = &eth_ipa_ctx;

	if (!ethqos || !eth_ipa) {
		ETHQOSERR("Null Param\n");
		return -ENOMEM;
	}

	if (ethqos->debugfs_dir) {
		debugfs_remove(eth_ipa->debugfs_ipa_stats);
		eth_ipa->debugfs_ipa_stats = NULL;

		debugfs_remove(eth_ipa->debugfs_dma_stats);
		eth_ipa->debugfs_dma_stats = NULL;

		debugfs_remove(eth_ipa->debugfs_suspend_ipa_offload);
		eth_ipa->debugfs_suspend_ipa_offload = NULL;
	}

	ETHQOSERR("IPA debugfs Deleted Successfully\n");
	return 0;
}

/**
 * DWC_ETH_QOS_ipa_create_debugfs() - Called to create debugfs node
 * for offload data path debugging.
 *
 * IN: @pdata: NTN dirver private structure.
 * OUT: 0 on success and -1 on failure
 */
static int ethqos_ipa_create_debugfs(struct qcom_ethqos *ethqos)
{
	struct ethqos_prv_ipa_data *eth_ipa = &eth_ipa_ctx;

	eth_ipa->debugfs_suspend_ipa_offload =
		debugfs_create_file("suspend_ipa_offload", 0600,
				    ethqos->debugfs_dir, ethqos,
				    &fops_ntn_ipa_offload_en);
	if (!eth_ipa->debugfs_suspend_ipa_offload ||
	    IS_ERR(eth_ipa->debugfs_suspend_ipa_offload)) {
		ETHQOSERR("Cannot create debugfs ipa_offload_en %d\n",
			  (int)eth_ipa->debugfs_suspend_ipa_offload);
		goto fail;
	}

	eth_ipa->debugfs_ipa_stats =
		debugfs_create_file("ipa_stats", 0600,
				    ethqos->debugfs_dir, ethqos,
				    &fops_ipa_stats);
	if (!eth_ipa->debugfs_ipa_stats ||
	    IS_ERR(eth_ipa->debugfs_ipa_stats)) {
		ETHQOSERR("Cannot create debugfs_ipa_stats %d\n",
			  (int)eth_ipa->debugfs_ipa_stats);
		goto fail;
	}

	eth_ipa->debugfs_dma_stats =
		debugfs_create_file("dma_stats", 0600,
				    ethqos->debugfs_dir, ethqos,
				    &fops_ntn_dma_stats);
	if (!eth_ipa->debugfs_dma_stats ||
	    IS_ERR(eth_ipa->debugfs_dma_stats)) {
		ETHQOSERR("Cannot create debugfs_dma_stats %d\n",
			  (int)eth_ipa->debugfs_dma_stats);
		goto fail;
	}

	return 0;

fail:
	ethqos_ipa_cleanup_debugfs(ethqos);
	return -ENOMEM;
}

static int ethqos_ipa_offload_connect(
	struct qcom_ethqos *ethqos, enum ipa_queue_type type)
{
	struct ethqos_prv_ipa_data *eth_ipa = &eth_ipa_ctx;
	struct ipa_uc_offload_conn_in_params in;
	struct ipa_uc_offload_conn_out_params out;
	struct ipa_ntn_setup_info rx_setup_info = {0};
	struct ipa_ntn_setup_info tx_setup_info = {0};
	struct ipa_perf_profile profile;
	int ret = 0;
	int i = 0;

	ETHQOSDBG("begin\n");
	if (!ethqos) {
		ETHQOSERR("Null Param\n");
		ret = -1;
		return ret;
	}

	/* Configure interrupt route for ETHQOS TX DMA channel to IPA */
	/* Currently, HW route is supported only for one DMA channel */
	if (eth_ipa_queue_type_to_tx_intr_route(type) == IPA_INTR_ROUTE_HW)
		RGMII_GPIO_CFG_TX_INT_UDFWR(
			eth_ipa_queue_type_to_tx_queue(type));

	/* Configure interrupt route for ETHQOS RX DMA channel to IPA */
	/* Currently, HW route is supported only for one DMA channel */
	if (eth_ipa_queue_type_to_rx_intr_route(type) == IPA_INTR_ROUTE_HW)
		RGMII_GPIO_CFG_RX_INT_UDFWR(
			eth_ipa_queue_type_to_rx_queue(type));

	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));
	memset(&profile, 0, sizeof(profile));

	in.clnt_hndl = eth_ipa->ipa_client_hndl[type];

	/* Uplink Setup */
	if (stmmac_emb_smmu_ctx.valid)
		rx_setup_info.smmu_enabled = true;
	else
		rx_setup_info.smmu_enabled = false;

	rx_setup_info.client = eth_ipa_queue_type_to_rx_client(type);
	rx_setup_info.db_mode = eth_ipa_queue_type_to_rx_intr_route(type);
	if (!rx_setup_info.smmu_enabled)
		rx_setup_info.ring_base_pa =
		(phys_addr_t)eth_ipa_ctx.rx_queue[type]->rx_desc_dma_addrs[0];
	rx_setup_info.ring_base_iova
	= eth_ipa_ctx.rx_queue[type]->rx_desc_dma_addrs[0];
	rx_setup_info.ntn_ring_size
	= eth_ipa_ctx.rx_queue[type]->desc_cnt;
	if (!rx_setup_info.smmu_enabled)
		rx_setup_info.buff_pool_base_pa =
		eth_ipa_ctx.rx_queue[type]->ipa_rx_pa_addrs_base_dmahndl;
	rx_setup_info.buff_pool_base_iova =
	eth_ipa_ctx.rx_queue[type]->ipa_rx_pa_addrs_base_dmahndl;
	rx_setup_info.num_buffers =
		eth_ipa_ctx.rx_queue[type]->desc_cnt - 1;
	rx_setup_info.data_buff_size = eth_ipa_queue_type_to_buf_length(type);

	/* Base address here is the address of ETHQOS_DMA_CH(i)_CONTROL
	 * in ETHQOS resgister space
	 */
	rx_setup_info.ntn_reg_base_ptr_pa =
		eth_ipa_queue_type_to_rx_reg_base_ptr_pa(type);

	/* Downlink Setup */
	if (stmmac_emb_smmu_ctx.valid)
		tx_setup_info.smmu_enabled = true;
	else
		tx_setup_info.smmu_enabled = false;

	tx_setup_info.client = eth_ipa_queue_type_to_tx_client(type);
	tx_setup_info.db_mode = eth_ipa_queue_type_to_tx_intr_route(type);
	if (!tx_setup_info.smmu_enabled)
		tx_setup_info.ring_base_pa =
		(phys_addr_t)eth_ipa_ctx.tx_queue[type]->tx_desc_dma_addrs[0];

	tx_setup_info.ring_base_iova
	= eth_ipa_ctx.tx_queue[type]->tx_desc_dma_addrs[0];
	tx_setup_info.ntn_ring_size = eth_ipa_ctx.tx_queue[type]->desc_cnt;
	if (!tx_setup_info.smmu_enabled)
		tx_setup_info.buff_pool_base_pa =
		eth_ipa_ctx.tx_queue[type]->ipa_tx_pa_addrs_base_dmahndl;

	tx_setup_info.buff_pool_base_iova =
	eth_ipa_ctx.tx_queue[type]->ipa_tx_pa_addrs_base_dmahndl;

	tx_setup_info.num_buffers =
		eth_ipa_ctx.tx_queue[type]->desc_cnt - 1;
	tx_setup_info.data_buff_size = eth_ipa_queue_type_to_buf_length(type);

	/* Base address here is the address of ETHQOS_DMA_CH(i)_CONTROL
	 * in ETHQOS resgister space
	 */
	tx_setup_info.ntn_reg_base_ptr_pa =
		eth_ipa_queue_type_to_tx_reg_base_ptr_pa(type);

	rx_setup_info.data_buff_list
	= kcalloc(rx_setup_info.num_buffers, sizeof(struct ntn_buff_smmu_map),
		  GFP_KERNEL);
	if (!rx_setup_info.data_buff_list) {
		ETHQOSERR("Failed to allocate mem for Rx data_buff_list");
		ret = -ENOMEM;
		goto mem_free;
	}
	tx_setup_info.data_buff_list
	= kcalloc(tx_setup_info.num_buffers, sizeof(struct ntn_buff_smmu_map),
		  GFP_KERNEL);
	if (!tx_setup_info.data_buff_list) {
		ETHQOSERR("Failed to allocate mem for Tx data_buff_list");
		ret = -ENOMEM;
		goto mem_free;
	}

	for (i = 0; i < rx_setup_info.num_buffers; i++) {
		rx_setup_info.data_buff_list[i].iova =
		eth_ipa_ctx.rx_queue[type]->ipa_rx_pa_addrs_base[i];

		if (!rx_setup_info.smmu_enabled) {
			rx_setup_info.data_buff_list[i].pa =
			rx_setup_info.data_buff_list[i].iova;
		} else {
			rx_setup_info.data_buff_list[i].pa =
			eth_ipa_ctx.rx_queue[type]->ipa_rx_buff_phy_addr[i];
		}
	}
	for (i = 0; i < tx_setup_info.num_buffers; i++) {
		tx_setup_info.data_buff_list[i].iova =
		eth_ipa_ctx.tx_queue[type]->ipa_tx_pa_addrs_base[i];

		if (!tx_setup_info.smmu_enabled)
			tx_setup_info.data_buff_list[i].pa =
			tx_setup_info.data_buff_list[i].iova;
		else
			tx_setup_info.data_buff_list[i].pa =
			eth_ipa_ctx.tx_queue[type]->ipa_tx_phy_addr[i];
	}

	if (stmmac_emb_smmu_ctx.valid) {
		ret = ethqos_set_ul_dl_smmu_ipa_params(ethqos, &rx_setup_info,
						       &tx_setup_info, type);
		if (ret) {
			ETHQOSERR("Failed to build ipa_setup_info :%d\n", ret);
			ret = -1;
			goto mem_free;
		}
	}

	// IPA <- IPAUC <- EMAC <- CLIENT
	in.u.ntn.ul = rx_setup_info;
	// IPA -> IPAUC -> EMAC -> CLIENT
	in.u.ntn.dl = tx_setup_info;

	ret = ipa_uc_offload_conn_pipes(&in, &out);
	if (ret) {
		ETHQOSERR("Could not connect IPA Offload Pipes %d\n", ret);
		ret = -1;
		goto mem_free;
	}

	eth_ipa->uc_db_rx_addr[type] = out.u.ntn.ul_uc_db_iomem;
	eth_ipa->uc_db_tx_addr[type] = out.u.ntn.dl_uc_db_iomem;

	ETHQOSDBG("type=%d rx_db=%x\n", type, eth_ipa_ctx.uc_db_rx_addr[type]);
	ETHQOSDBG("type=%d tx_db=%x\n", type, eth_ipa_ctx.uc_db_tx_addr[type]);

	/* Set Perf Profile For PROD/CONS Pipes */
	profile.max_supported_bw_mbps = ethqos->speed;
	profile.client = eth_ipa_queue_type_to_rx_client(type);
	profile.proto =  eth_ipa_queue_type_to_proto(type);
	ret = ipa_set_perf_profile(&profile);
	if (ret) {
		ETHQOSERR("%s: Err set IPA_RM_RESOURCE_ETHERNET_PROD :%d\n",
			  __func__, ret);
		ret = -1;
		goto mem_free;
	}

	profile.client = eth_ipa_queue_type_to_tx_client(type);
	profile.proto =  eth_ipa_queue_type_to_proto(type);
	ret = ipa_set_perf_profile(&profile);
	if (ret) {
		ETHQOSERR("%s: Err set IPA_RM_RESOURCE_ETHERNET_CONS :%d\n",
			  __func__, ret);
		ret = -1;
		goto mem_free;
	}

	if (eth_ipa_queue_type_to_send_msg_needed(type))
		eth_ipa_send_msg(ethqos, IPA_PERIPHERAL_CONNECT, type);
 mem_free:
	kfree(rx_setup_info.data_buff_list);
	rx_setup_info.data_buff_list = NULL;

	kfree(tx_setup_info.data_buff_list);
	tx_setup_info.data_buff_list = NULL;

	if (stmmac_emb_smmu_ctx.valid) {
		if (rx_setup_info.ring_base_sgt) {
			sg_free_table(rx_setup_info.ring_base_sgt);
			kfree(rx_setup_info.ring_base_sgt);
			rx_setup_info.ring_base_sgt = NULL;
		}
		if (tx_setup_info.ring_base_sgt) {
			sg_free_table(tx_setup_info.ring_base_sgt);
			kfree(tx_setup_info.ring_base_sgt);
			tx_setup_info.ring_base_sgt = NULL;
		}
		if (rx_setup_info.buff_pool_base_sgt) {
			sg_free_table(rx_setup_info.buff_pool_base_sgt);
			kfree(rx_setup_info.buff_pool_base_sgt);
			rx_setup_info.buff_pool_base_sgt = NULL;
		}
		if (tx_setup_info.buff_pool_base_sgt) {
			sg_free_table(tx_setup_info.buff_pool_base_sgt);
			kfree(tx_setup_info.buff_pool_base_sgt);
			tx_setup_info.buff_pool_base_sgt = NULL;
		}
	}

	ETHQOSDBG("end\n");
	return ret;
}

static int ethqos_ipa_offload_disconnect(
	struct qcom_ethqos *ethqos, enum ipa_queue_type type)
{
	struct ethqos_prv_ipa_data *eth_ipa = &eth_ipa_ctx;
	int ret = 0;

	ETHQOSDBG("- begin\n");

	if (!ethqos) {
		ETHQOSERR("Null Param\n");
		return -ENOMEM;
	}

	ret = ipa_uc_offload_disconn_pipes(eth_ipa->ipa_client_hndl[type]);
	if (ret) {
		ETHQOSERR("Could not cleanup IPA Offload ret %d\n", ret);
		return ret;
	}

	ETHQOSDBG("end\n");
	return 0;
}

static int ethqos_ipa_offload_suspend(struct qcom_ethqos *ethqos)
{
	int ret = 0;
	struct ipa_perf_profile profile;
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);
	int type;

	ETHQOSDBG("Suspend/disable IPA offload\n");

	for (type = 0; type < IPA_QUEUE_MAX; type++) {
		if (eth_ipa_queue_type_enabled(type)) {
			priv->hw->dma->stop_rx(
				priv->ioaddr,
				eth_ipa_queue_type_to_rx_queue(type));

			if (ret != 0) {
				ETHQOSERR("%s: stop_dma_rx failed %d\n",
					  __func__, ret);
				return ret;
			}
		}
	}

	/* Disconnect IPA offload */
	if (eth_ipa_ctx.ipa_offload_conn) {
		for (type = 0; type < IPA_QUEUE_MAX; type++) {
			if (eth_ipa_queue_type_enabled(type)) {
				ret = ethqos_ipa_offload_disconnect(ethqos,
								    type);
				if (ret) {
					ETHQOSERR("%s: Disconnect Failed %d\n",
						  __func__, ret);
					return ret;
				}
			}
			eth_ipa_ctx.ipa_offload_conn = false;
		}
		ETHQOSERR("IPA Offload Disconnect Successfully\n");
	}

	for (type = 0; type < IPA_QUEUE_MAX; type++) {
		if (eth_ipa_queue_type_enabled(type)) {
			priv->hw->dma->stop_tx(
				priv->ioaddr,
				eth_ipa_queue_type_to_tx_queue(type));

			if (ret != 0) {
				ETHQOSERR("%s: stop_dma_tx failed %d\n",
					  __func__, ret);
				return ret;
			}
		}
	}

	if (eth_ipa_ctx.ipa_uc_ready) {
		profile.max_supported_bw_mbps = 0;
		for (type = 0; type < IPA_QUEUE_MAX; type++) {
			if (eth_ipa_queue_type_enabled(type)) {
				profile.client =
					eth_ipa_queue_type_to_tx_client(type);
				profile.proto =
					eth_ipa_queue_type_to_proto(type);
				ret = ipa_set_perf_profile(&profile);
				if (ret)
					ETHQOSERR("%s: Err set BW for TX %d\n",
						  __func__, ret);
			}
		}
	}

	if (eth_ipa_ctx.ipa_offload_init) {
		for (type = 0; type < IPA_QUEUE_MAX; type++) {
			if (eth_ipa_queue_type_enabled(type)) {
				ret = ethqos_ipa_offload_cleanup(ethqos, type);
				if (ret) {
					ETHQOSERR("%s: Cleanup Failed, %d\n",
						  __func__, ret);
					return ret;
				}
			}
		}
		ETHQOSINFO("IPA Offload Cleanup Success\n");
		eth_ipa_ctx.ipa_offload_init = false;
	}

	return ret;
}

static int ethqos_ipa_offload_resume(struct qcom_ethqos *ethqos)
{
	int ret = 1;
	struct ipa_perf_profile profile;
	int type;

	ETHQOSDBG("Enter\n");

	if (!eth_ipa_ctx.ipa_offload_init) {
		for (type = 0; type < IPA_QUEUE_MAX; type++) {
			if (eth_ipa_queue_type_enabled(type)) {
				eth_ipa_ctx.ipa_offload_init =
					!ethqos_ipa_offload_init(ethqos, type);
				if (!eth_ipa_ctx.ipa_offload_init)
					ETHQOSERR("%s: Init Failed for %d\n",
						  __func__, type);
			}
		}
	}

	/* Initialze descriptors before IPA connect */
	/* Set IPA owned DMA channels to reset state */
	for (type = 0; type < IPA_QUEUE_MAX; type++) {
		if (eth_ipa_queue_type_enabled(type)) {
			ethqos_ipa_tx_desc_init(ethqos, type);
			ethqos_ipa_rx_desc_init(ethqos, type);
		}
	}

	ETHQOSERR("DWC_ETH_QOS_ipa_offload_connect\n");
	for (type = 0; type < IPA_QUEUE_MAX; type++) {
		if (eth_ipa_queue_type_enabled(type)) {
			ret = ethqos_ipa_offload_connect(ethqos, type);
			if (ret != 0)
				goto fail;
			else
				eth_ipa_ctx.ipa_offload_conn = true;
		}
	}

	profile.max_supported_bw_mbps = ethqos->speed;
	for (type = 0; type < IPA_QUEUE_MAX; type++) {
		if (eth_ipa_queue_type_enabled(type)) {
			profile.client = eth_ipa_queue_type_to_tx_client(type);
			profile.proto =  eth_ipa_queue_type_to_proto(type);
			ret = ipa_set_perf_profile(&profile);
			if (ret)
				ETHQOSERR("%s: Err set BW for TX: %d\n",
					  __func__, ret);

			/*Initialize DMA CHs for offload*/
			ethqos_init_offload(ethqos, type);
			if (ret) {
				ETHQOSERR("Offload channel Init Failed\n");
				return ret;
			}
		}
	}

	ETHQOSDBG("Exit\n");

fail:
	return ret;
}

static int ethqos_disable_ipa_offload(struct qcom_ethqos *ethqos)
{
	int ret = 0;

	ETHQOSDBG("Enter\n");

	/* De-configure IPA Related Stuff */
	/* Not user requested suspend, do not set ipa_offload_susp */
	if (!eth_ipa_ctx.ipa_offload_susp &&
	    eth_ipa_ctx.ipa_offload_conn) {
		ret = ethqos_ipa_offload_suspend(ethqos);
		if (ret) {
			ETHQOSERR("IPA Suspend Failed, err:%d\n", ret);
			return ret;
		}
	}

	if (eth_ipa_ctx.ipa_debugfs_exists) {
		if (ethqos_ipa_cleanup_debugfs(ethqos))
			ETHQOSERR("Unable to delete IPA debugfs\n");
		else
			eth_ipa_ctx.ipa_debugfs_exists = false;
	}

	ETHQOSDBG("Exit\n");

	return ret;
}

static int ethqos_enable_ipa_offload(struct qcom_ethqos *ethqos)
{
	int ret = 0;
	int type;

	if (!eth_ipa_ctx.ipa_offload_init) {
		for (type = 0; type < IPA_QUEUE_MAX; type++) {
			if (eth_ipa_queue_type_enabled(type)) {
				ret = ethqos_ipa_offload_init(ethqos, type);
				if (ret) {
					ETHQOSERR("%s: Init Failed\n",
						  __func__);
					eth_ipa_ctx.ipa_offload_init = false;
					goto fail;
				}
			}
		}
		ETHQOSINFO("IPA Offload Initialized Successfully\n");
		eth_ipa_ctx.ipa_offload_init = true;
	}
	if (!eth_ipa_ctx.ipa_offload_conn &&
	    !eth_ipa_ctx.ipa_offload_susp) {
		for (type = 0; type < IPA_QUEUE_MAX; type++) {
			if (eth_ipa_queue_type_enabled(type)) {
				ret = ethqos_ipa_offload_connect(ethqos, type);
				if (ret) {
					ETHQOSERR("Connect Failed, type %d\n",
						  type);
					eth_ipa_ctx.ipa_offload_conn = false;
					goto fail;
				}
			}
		}
		ETHQOSINFO("IPA Offload Connect Successfully\n");
		eth_ipa_ctx.ipa_offload_conn = true;

		for (type = 0; type < IPA_QUEUE_MAX; type++) {
			if (eth_ipa_queue_type_enabled(type)) {
				/*Initialize DMA CHs for offload*/
				ret = ethqos_init_offload(ethqos, type);
				if (ret) {
					ETHQOSERR("%s: channel Init Failed\n",
						  __func__);
					goto fail;
				}
			}
		}
	}

	if (!eth_ipa_ctx.ipa_debugfs_exists) {
		if (!ethqos_ipa_create_debugfs(ethqos)) {
			ETHQOSERR("eMAC Debugfs created\n");
			eth_ipa_ctx.ipa_debugfs_exists = true;
		} else {
			ETHQOSERR("eMAC Debugfs failed\n");
		}
	}

	ETHQOSINFO("IPA Offload Enabled successfully\n");
	return ret;

fail:

	if (eth_ipa_ctx.ipa_offload_conn) {
		for (type = 0; type < IPA_QUEUE_MAX; type++) {
			if (eth_ipa_queue_type_enabled(type)) {
				if (ethqos_ipa_offload_disconnect(ethqos,
								  type))
					ETHQOSERR(
						"IPA Offload Disconnect Failed\n");
				else
					ETHQOSERR(
						"IPA Offload Disconnect Successfully\n");
			}
			eth_ipa_ctx.ipa_offload_conn = false;
		}
	}

	if (eth_ipa_ctx.ipa_offload_init) {
		for (type = 0; type < IPA_QUEUE_MAX; type++) {
			if (eth_ipa_queue_type_enabled(type)) {
				if (ethqos_ipa_offload_cleanup(ethqos, type))
					ETHQOSERR(
						"IPA Offload Cleanup Failed\n");
				else
					ETHQOSERR(
						"IPA Offload Cleanup Success\n");
			}
			eth_ipa_ctx.ipa_offload_init = false;
		}
	}

	return ret;
}

static void ethqos_ipa_ready_wq(struct work_struct *work)
{
	struct qcom_ethqos *ethqos = eth_ipa_ctx.ethqos;
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);

	ETHQOSDBG("\n");
	ethqos_ipa_offload_event_handler(priv, EV_IPA_READY);
}

static void ethqos_ipa_ready_cb(void *user_data)
{
	ETHQOSDBG("\n");

	INIT_WORK(&eth_ipa_ctx.ntn_ipa_rdy_work, ethqos_ipa_ready_wq);
	queue_work(system_unbound_wq, &eth_ipa_ctx.ntn_ipa_rdy_work);
}

static int ethqos_ipa_ready(struct qcom_ethqos *pdata)
{
	int ret = 0;

	ETHQOSDBG("Enter\n");

	ret = ipa_register_ipa_ready_cb(ethqos_ipa_ready_cb, (void *)pdata);
	if (ret == -ENXIO) {
		ETHQOSERR("IPA driver context is not even ready\n");
		return ret;
	}

	if (ret != -EEXIST) {
		ETHQOSERR("register ipa ready cb\n");
		return ret;
	}

	eth_ipa_ctx.ipa_ready = true;
	ETHQOSDBG("Exit\n");
	return ret;
}

static void ethqos_ipaucrdy_wq(struct work_struct *work)
{
	struct qcom_ethqos *ethqos = eth_ipa_ctx.ethqos;
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);

	ETHQOSDBG("DWC_ETH_QOS_ipa_uc_ready_wq\n");
	ethqos_ipa_offload_event_handler(priv, EV_IPA_UC_READY);
}

static void ethqos_ipa_uc_ready_cb(void *user_data)
{
	struct qcom_ethqos *pdata = (struct qcom_ethqos *)user_data;
	struct ethqos_prv_ipa_data *eth_ipa = &eth_ipa_ctx;

	if (!pdata) {
		ETHQOSERR("Null Param pdata %pK\n", pdata);
		return;
	}

	ETHQOSINFO("Received IPA UC ready callback\n");
	INIT_WORK(&eth_ipa->ntn_ipa_rdy_work, ethqos_ipaucrdy_wq);
	queue_work(system_unbound_wq, &eth_ipa->ntn_ipa_rdy_work);
}

static int ethqos_ipa_uc_ready(struct qcom_ethqos *pdata)
{
	struct ipa_uc_ready_params param;
	int ret, type;

	ETHQOSDBG("Enter\n");

	param.is_uC_ready = false;
	param.priv = pdata;
	param.notify = ethqos_ipa_uc_ready_cb;

	for (type = 0; type < IPA_QUEUE_MAX; type++) {
		if (eth_ipa_queue_type_enabled(type)) {
			// Register only for one enabled proto.
			// Do not need for all protos
			param.proto = eth_ipa_queue_type_to_proto(type);
			break;
		}
	}

	ret = ipa_uc_offload_reg_rdyCB(&param);
	if (ret == 0 && param.is_uC_ready) {
		ETHQOSINFO("ipa uc ready\n");
		eth_ipa_ctx.ipa_uc_ready = true;
	}
	ETHQOSDBG("Exit\n");
	return ret;
}

void ethqos_ipa_offload_event_handler(void *data,
				      int ev)
{
	int type;

	ETHQOSDBG("Enter: event=%d\n", ev);

	/* Handle events outside IPA lock */
	switch (ev) {
	case EV_PROBE_INIT:
		{
			eth_ipa_ctx.ethqos = data;
			eth_ipa_ctx_init();
			eth_ipa_net_drv_init();
			return;
		}
		break;
	case EV_IPA_HANDLE_RX_INTR:
		{
			eth_ipa_handle_rx_interrupt(*(int *)data);
			return;
		}
		break;
	case EV_IPA_HANDLE_TX_INTR:
		{
			eth_ipa_handle_tx_interrupt(*(int *)data);
			return;
		}
		break;
	default:
		break;
	}

	IPA_LOCK();

	switch (ev) {
	case EV_PHY_LINK_DOWN:
		if (!eth_ipa_ctx.emac_dev_ready ||
		    !eth_ipa_ctx.ipa_uc_ready ||
		    eth_ipa_ctx.ipa_offload_link_down ||
		    eth_ipa_ctx.ipa_offload_susp ||
		    !eth_ipa_ctx.ipa_offload_conn)
			break;

		if (!ethqos_ipa_offload_suspend(eth_ipa_ctx.ethqos))
			eth_ipa_ctx.ipa_offload_link_down = true;

		break;
	case EV_PHY_LINK_UP:
		if (!eth_ipa_ctx.emac_dev_ready ||
		    !eth_ipa_ctx.ipa_uc_ready ||
		    eth_ipa_ctx.ipa_offload_susp)
			break;

		/* Link up event is expected only after link down */
		if (eth_ipa_ctx.ipa_offload_link_down)
			ethqos_ipa_offload_resume(eth_ipa_ctx.ethqos);
		else if (eth_ipa_ctx.emac_dev_ready &&
			 eth_ipa_ctx.ipa_uc_ready)
			ethqos_enable_ipa_offload(eth_ipa_ctx.ethqos);

		eth_ipa_ctx.ipa_offload_link_down = false;

		break;
	case EV_DEV_OPEN:
		ethqos_ipa_config_queues(eth_ipa_ctx.ethqos);

		eth_ipa_ctx.emac_dev_ready = true;

		if (!eth_ipa_ctx.ipa_ready)
			ethqos_ipa_ready(eth_ipa_ctx.ethqos);

		if (!eth_ipa_ctx.ipa_uc_ready)
			ethqos_ipa_uc_ready(eth_ipa_ctx.ethqos);

		break;
	case EV_IPA_READY:
		eth_ipa_ctx.ipa_ready = true;

		if (!eth_ipa_ctx.ipa_uc_ready)
			ethqos_ipa_uc_ready(eth_ipa_ctx.ethqos);

		if (eth_ipa_ctx.ipa_uc_ready &&
		    qcom_ethqos_is_phy_link_up(eth_ipa_ctx.ethqos))
			ethqos_enable_ipa_offload(eth_ipa_ctx.ethqos);

		break;
	case EV_IPA_UC_READY:
		eth_ipa_ctx.ipa_uc_ready = true;
		ETHQOSINFO("ipa uC is ready\n");

		if (!eth_ipa_ctx.emac_dev_ready)
			break;
		if (eth_ipa_ctx.ipa_ready && !eth_ipa_ctx.ipa_offload_init) {
			for (type = 0; type < IPA_QUEUE_MAX; type++) {
				if (eth_ipa_queue_type_enabled(type)) {
					eth_ipa_ctx.ipa_offload_init =
						!ethqos_ipa_offload_init(
							eth_ipa_ctx.ethqos,
							type);
				}
			}
		}
		if (qcom_ethqos_is_phy_link_up(eth_ipa_ctx.ethqos))
			ethqos_enable_ipa_offload(eth_ipa_ctx.ethqos);

		break;
	case EV_DEV_CLOSE:
		eth_ipa_ctx.emac_dev_ready = false;

		for (type = 0; type < IPA_QUEUE_MAX; type++) {
			if (eth_ipa_queue_type_enabled(type)) {
				/* Deregister only for 1st enabled proto */
				if (eth_ipa_ctx.ipa_uc_ready) {
					ipa_uc_offload_dereg_rdyCB(
						eth_ipa_queue_type_to_proto(
						   type));
					break;
				}
			}
		}

		ethqos_disable_ipa_offload(eth_ipa_ctx.ethqos);

		/* reset link down on dev close */
		eth_ipa_ctx.ipa_offload_link_down = 0;
		ethqos_free_ipa_queue_mem(eth_ipa_ctx.ethqos);

		break;
	case EV_DPM_SUSPEND:
		if (eth_ipa_ctx.ipa_offload_conn)
			*(int *)data = false;
		else
			*(int *)data = true;

		break;
	case EV_USR_SUSPEND:
		if (!eth_ipa_ctx.ipa_offload_susp &&
		    !eth_ipa_ctx.ipa_offload_link_down)
			if (!ethqos_ipa_offload_suspend(eth_ipa_ctx.ethqos))
				eth_ipa_ctx.ipa_offload_susp = true;

		break;
	case EV_DPM_RESUME:
		if (qcom_ethqos_is_phy_link_up(eth_ipa_ctx.ethqos)) {
			if (!ethqos_ipa_offload_resume(eth_ipa_ctx.ethqos))
				eth_ipa_ctx.ipa_offload_susp = false;
		} else {
			/* Reset flag here to allow connection
			 * of pipes on next PHY link up
			 */
			eth_ipa_ctx.ipa_offload_susp = false;
			/* PHY link is down at resume */
			/* Reset flag here to allow connection
			 * of pipes on next PHY link up
			 */
			eth_ipa_ctx.ipa_offload_link_down = true;
		}

		break;
	case EV_USR_RESUME:
		if (eth_ipa_ctx.ipa_offload_susp)
			if (!ethqos_ipa_offload_resume(eth_ipa_ctx.ethqos))
				eth_ipa_ctx.ipa_offload_susp = false;

		break;
	case EV_IPA_OFFLOAD_REMOVE:
		ethqos_free_ipa_queue_mem(eth_ipa_ctx.ethqos);

		break;
	case EV_QTI_GET_CONN_STATUS:
		if (eth_ipa_ctx.queue_enabled[IPA_QUEUE_CV2X])
			*(u8 *)data = eth_ipa_ctx.ipa_offload_conn ?
			ETH_EVT_CV2X_PIPE_CONNECTED :
			ETH_EVT_CV2X_PIPE_DISCONNECTED;
		else
			*(u8 *)data = ETH_EVT_CV2X_MODE_NOT_ENABLED;

		break;
	case EV_QTI_CHECK_CONN_UPDATE:
		/* check if status is updated */
		if (eth_ipa_ctx.ipa_offload_conn_prev !=
			eth_ipa_ctx.ipa_offload_conn) {
			*(int *)data = true;
			eth_ipa_ctx.ipa_offload_conn_prev =
				eth_ipa_ctx.ipa_offload_conn;
		} else {
			*(int *)data = false;
		}

		break;
	case EV_INVALID:
	default:
		break;
	}

	/* Wake up the /dev/emac queue if there is connect status changed */
	/* This should be done only if CV2X queue is enabled */
	/* Do only for event which can actually alter pipe connection */
	if (eth_ipa_ctx.queue_enabled[IPA_QUEUE_CV2X] &&
	    (ev == EV_USR_SUSPEND || ev == EV_USR_RESUME ||
	     ev == EV_DEV_CLOSE || ev == EV_DEV_OPEN ||
	     ev == EV_PHY_LINK_DOWN || ev ==  EV_PHY_LINK_UP)) {
		if (eth_ipa_ctx.ipa_offload_conn_prev !=
		    eth_ipa_ctx.ipa_offload_conn)
			ETHQOSDBG("need-status-updated\n");
		ethqos_wakeup_dev_emac_queue();
	}

	ETHQOSDBG("Exit: event=%d\n", ev);
	IPA_UNLOCK();
}

