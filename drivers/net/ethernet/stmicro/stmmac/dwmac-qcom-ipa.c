/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

static inline void *ethqos_get_priv(struct qcom_ethqos *ethqos)
{
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);

	return priv;
}

static int eth_ipa_send_msg(struct qcom_ethqos *ethqos,
			    enum ipa_peripheral_event event)
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

	emac_msg.ifindex = priv->dev->ifindex;
	strlcpy(emac_msg.name, priv->dev->name, IPA_RESOURCE_NAME_MAX);

	msg_meta.msg_type = event;
	msg_meta.msg_len = sizeof(struct ipa_ecm_msg);

	return ipa_send_msg(&msg_meta, &emac_msg, __ipa_eth_free_msg);
}

static int ethqos_alloc_ipa_tx_queue_struct(struct qcom_ethqos *ethqos)
{
	int ret = 0, chinx, cnt;
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);

	chinx = 0;
	eth_ipa_ctx.tx_queue =
		kzalloc(sizeof(struct ethqos_tx_queue),
			GFP_KERNEL);
	if (!eth_ipa_ctx.tx_queue) {
		ETHQOSERR("ERROR: Unable to allocate Tx queue structure\n");
		ret = -ENOMEM;
		goto err_out_tx_q_alloc_failed;
	}

	eth_ipa_ctx.tx_queue->desc_cnt = IPA_TX_DESC_CNT;

	/* Allocate tx_desc_ptrs */
	eth_ipa_ctx.tx_queue->tx_desc_ptrs =
		kcalloc(eth_ipa_ctx.tx_queue->desc_cnt,
			sizeof(struct dma_desc *), GFP_KERNEL);
	if (!eth_ipa_ctx.tx_queue->tx_desc_ptrs) {
		ETHQOSERR("ERROR: Unable to allocate Tx Desc ptrs\n");
		ret = -ENOMEM;
		goto err_out_tx_desc_ptrs_failed;
	}
	for (cnt = 0; cnt < eth_ipa_ctx.tx_queue->desc_cnt; cnt++) {
		eth_ipa_ctx.tx_queue->tx_desc_ptrs[cnt]
		= eth_ipa_ctx.tx_queue->tx_desc_ptrs[0] +
		(sizeof(struct dma_desc *) * cnt);
	}

	/* Allocate tx_desc_dma_addrs */
	eth_ipa_ctx.tx_queue->tx_desc_dma_addrs =
			kzalloc(sizeof(dma_addr_t) *
				eth_ipa_ctx.tx_queue->desc_cnt, GFP_KERNEL);
	if (!eth_ipa_ctx.tx_queue->tx_desc_dma_addrs) {
		ETHQOSERR("ERROR: Unable to allocate Tx Desc dma addrs\n");
		ret = -ENOMEM;
		goto err_out_tx_desc_dma_addrs_failed;
	}
	for (cnt = 0; cnt < eth_ipa_ctx.tx_queue->desc_cnt; cnt++) {
		eth_ipa_ctx.tx_queue->tx_desc_dma_addrs[cnt]
		= eth_ipa_ctx.tx_queue->tx_desc_dma_addrs[0] +
		(sizeof(dma_addr_t) * cnt);
	}

	/* Allocate tx_buf_ptrs */
	eth_ipa_ctx.tx_queue->skb =
		kzalloc(sizeof(struct sk_buff *) *
			eth_ipa_ctx.tx_queue->desc_cnt, GFP_KERNEL);
	if (!eth_ipa_ctx.tx_queue->skb) {
		ETHQOSERR("ERROR: Unable to allocate Tx buff ptrs\n");
		ret = -ENOMEM;
		goto err_out_tx_buf_ptrs_failed;
	}

	/* Allocate ipa_tx_buff_pool_va_addrs_base */
	eth_ipa_ctx.tx_queue->ipa_tx_buff_pool_va_addrs_base =
	kzalloc(sizeof(void *) * eth_ipa_ctx.tx_queue->desc_cnt, GFP_KERNEL);
	if (!eth_ipa_ctx.tx_queue->ipa_tx_buff_pool_va_addrs_base) {
		ETHQOSERR("ERROR: Unable to allocate Tx ipa buff addrs\n");
		ret = -ENOMEM;
		goto err_out_tx_buf_ptrs_failed;
	}

	eth_ipa_ctx.tx_queue->skb_dma =
	kzalloc(sizeof(dma_addr_t) * eth_ipa_ctx.tx_queue->desc_cnt,
		GFP_KERNEL);
	if (!eth_ipa_ctx.tx_queue->skb_dma) {
		ETHQOSERR("ERROR: Unable to allocate Tx ipa buff addrs\n");
		ret = -ENOMEM;
		goto err_out_tx_buf_ptrs_failed;
	}

	eth_ipa_ctx.tx_queue->ipa_tx_buff_phy_addr =
	kzalloc(sizeof(phys_addr_t) * eth_ipa_ctx.tx_queue->desc_cnt,
		GFP_KERNEL);
	if (!eth_ipa_ctx.tx_queue->ipa_tx_buff_phy_addr) {
		ETHQOSERR("ERROR: Unable to allocate Tx ipa buff  dma addrs\n");
		ret = -ENOMEM;
		goto err_out_tx_buf_ptrs_failed;
	}

	ETHQOSDBG("<--ethqos_alloc_tx_queue_struct\n");
	eth_ipa_ctx.tx_queue->tx_q = &priv->tx_queue[chinx];
	return ret;

err_out_tx_buf_ptrs_failed:
	kfree(eth_ipa_ctx.tx_queue->tx_desc_dma_addrs);
	eth_ipa_ctx.tx_queue->tx_desc_dma_addrs = NULL;
	kfree(eth_ipa_ctx.tx_queue->skb);
	eth_ipa_ctx.tx_queue->skb = NULL;
	kfree(eth_ipa_ctx.tx_queue->skb_dma);
	eth_ipa_ctx.tx_queue->skb_dma = NULL;
	kfree(eth_ipa_ctx.tx_queue->ipa_tx_buff_pool_va_addrs_base);
	eth_ipa_ctx.tx_queue->ipa_tx_buff_pool_va_addrs_base = NULL;
	kfree(eth_ipa_ctx.tx_queue->ipa_tx_buff_phy_addr);
	eth_ipa_ctx.tx_queue->ipa_tx_buff_phy_addr = NULL;

err_out_tx_desc_dma_addrs_failed:
	kfree(eth_ipa_ctx.tx_queue->tx_desc_ptrs);
	eth_ipa_ctx.tx_queue->tx_desc_ptrs = NULL;

err_out_tx_desc_ptrs_failed:
	kfree(eth_ipa_ctx.tx_queue);
	eth_ipa_ctx.tx_queue = NULL;

err_out_tx_q_alloc_failed:
	return ret;
}

static void ethqos_free_ipa_tx_queue_struct(struct qcom_ethqos *ethqos)
{
	kfree(eth_ipa_ctx.tx_queue->skb_dma);
	eth_ipa_ctx.tx_queue->skb_dma = NULL;

	kfree(eth_ipa_ctx.tx_queue->tx_desc_dma_addrs);
	eth_ipa_ctx.tx_queue->tx_desc_dma_addrs = NULL;

	kfree(eth_ipa_ctx.tx_queue->tx_desc_ptrs);
	eth_ipa_ctx.tx_queue->tx_desc_ptrs = NULL;

	kfree(eth_ipa_ctx.tx_queue->ipa_tx_buff_pool_va_addrs_base);
	eth_ipa_ctx.tx_queue->ipa_tx_buff_pool_va_addrs_base = NULL;

	kfree(eth_ipa_ctx.tx_queue->skb);
	eth_ipa_ctx.tx_queue->skb = NULL;
}

static int ethqos_alloc_ipa_rx_queue_struct(struct qcom_ethqos *ethqos)
{
	int ret = 0, chinx, cnt;

	chinx = 0;
	eth_ipa_ctx.rx_queue =
		kzalloc(sizeof(struct ethqos_rx_queue),
			GFP_KERNEL);
	if (!eth_ipa_ctx.rx_queue) {
		ETHQOSERR("ERROR: Unable to allocate Rx queue structure\n");
		ret = -ENOMEM;
		goto err_out_rx_q_alloc_failed;
	}

	eth_ipa_ctx.rx_queue->desc_cnt = IPA_RX_DESC_CNT;

	/* Allocate rx_desc_ptrs */
	eth_ipa_ctx.rx_queue->rx_desc_ptrs =
		kcalloc(eth_ipa_ctx.rx_queue->desc_cnt,
			sizeof(struct dma_desc *), GFP_KERNEL);
	if (!eth_ipa_ctx.rx_queue->rx_desc_ptrs) {
		ETHQOSERR("ERROR: Unable to allocate Rx Desc ptrs\n");
		ret = -ENOMEM;
		goto err_out_rx_desc_ptrs_failed;
	}

	for (cnt = 0; cnt < eth_ipa_ctx.rx_queue->desc_cnt; cnt++) {
		eth_ipa_ctx.rx_queue->rx_desc_ptrs[cnt]
		= eth_ipa_ctx.rx_queue->rx_desc_ptrs[0] +
		(sizeof(struct dma_desc *) * cnt);
	}

	/* Allocate rx_desc_dma_addrs */
	eth_ipa_ctx.rx_queue->rx_desc_dma_addrs =
		kzalloc(sizeof(dma_addr_t) * eth_ipa_ctx.rx_queue->desc_cnt,
			GFP_KERNEL);
	if (!eth_ipa_ctx.rx_queue->rx_desc_dma_addrs) {
		ETHQOSERR("ERROR: Unable to allocate Rx Desc dma addr\n");
		ret = -ENOMEM;
		goto err_out_rx_desc_dma_addrs_failed;
	}

	for (cnt = 0; cnt < eth_ipa_ctx.rx_queue->desc_cnt; cnt++) {
		eth_ipa_ctx.rx_queue->rx_desc_dma_addrs[cnt]
		= eth_ipa_ctx.rx_queue->rx_desc_dma_addrs[0]
		+ (sizeof(dma_addr_t) * cnt);
	}
	/* Allocat rx_ipa_buff */
	eth_ipa_ctx.rx_queue->skb =
		kzalloc(sizeof(struct sk_buff *) *
			eth_ipa_ctx.rx_queue->desc_cnt, GFP_KERNEL);
	if (!eth_ipa_ctx.rx_queue->skb) {
		ETHQOSERR("ERROR: Unable to allocate Tx buff ptrs\n");
		ret = -ENOMEM;
		goto err_out_rx_buf_ptrs_failed;
	}

	eth_ipa_ctx.rx_queue->ipa_buff_va =
	kzalloc(sizeof(void *) *
		eth_ipa_ctx.rx_queue->desc_cnt, GFP_KERNEL);
	if (!eth_ipa_ctx.rx_queue->ipa_buff_va) {
		ETHQOSERR("ERROR: Unable to allocate Tx buff ptrs\n");
		ret = -ENOMEM;
		goto err_out_rx_buf_ptrs_failed;
	}

	/* Allocate ipa_rx_buff_pool_va_addrs_base */
	eth_ipa_ctx.rx_queue->ipa_rx_buff_pool_va_addrs_base =
		kzalloc(sizeof(void *) * eth_ipa_ctx.rx_queue->desc_cnt,
			GFP_KERNEL);
	if (!eth_ipa_ctx.rx_queue->ipa_rx_buff_pool_va_addrs_base) {
		ETHQOSERR("ERROR: Unable to allocate Rx ipa buff addrs\n");
		ret = -ENOMEM;
		goto err_out_rx_buf_ptrs_failed;
	}

	eth_ipa_ctx.rx_queue->skb_dma =
	kzalloc(sizeof(dma_addr_t) * eth_ipa_ctx.rx_queue->desc_cnt,
		GFP_KERNEL);
	if (!eth_ipa_ctx.rx_queue->skb_dma) {
		ETHQOSERR("ERROR: Unable to allocate rx ipa buff addrs\n");
		ret = -ENOMEM;
		goto err_out_rx_buf_ptrs_failed;
	}

	eth_ipa_ctx.rx_queue->ipa_rx_buff_phy_addr =
	kzalloc(sizeof(phys_addr_t) * eth_ipa_ctx.rx_queue->desc_cnt,
		GFP_KERNEL);
	if (!eth_ipa_ctx.rx_queue->ipa_rx_buff_phy_addr) {
		ETHQOSERR("ERROR: Unable to allocate rx ipa buff  dma addrs\n");
		ret = -ENOMEM;
		goto err_out_rx_buf_ptrs_failed;
	}

	ETHQOSDBG("<--ethqos_alloc_rx_queue_struct\n");
	return ret;

err_out_rx_buf_ptrs_failed:
	kfree(eth_ipa_ctx.rx_queue->rx_desc_dma_addrs);
	eth_ipa_ctx.rx_queue->rx_desc_dma_addrs = NULL;
	kfree(eth_ipa_ctx.rx_queue->skb);
	eth_ipa_ctx.rx_queue->skb = NULL;
	kfree(eth_ipa_ctx.rx_queue->skb_dma);
	eth_ipa_ctx.rx_queue->skb_dma = NULL;
	kfree(eth_ipa_ctx.rx_queue->ipa_rx_buff_pool_va_addrs_base);
	eth_ipa_ctx.rx_queue->ipa_rx_buff_pool_va_addrs_base = NULL;
	kfree(eth_ipa_ctx.rx_queue->ipa_rx_buff_phy_addr);
	eth_ipa_ctx.rx_queue->ipa_rx_buff_phy_addr = NULL;
	kfree(eth_ipa_ctx.rx_queue->ipa_buff_va);
	eth_ipa_ctx.rx_queue->ipa_buff_va = NULL;

err_out_rx_desc_dma_addrs_failed:
	kfree(eth_ipa_ctx.rx_queue->rx_desc_ptrs);
	eth_ipa_ctx.rx_queue->rx_desc_ptrs = NULL;

err_out_rx_desc_ptrs_failed:
	kfree(eth_ipa_ctx.rx_queue);
	eth_ipa_ctx.rx_queue = NULL;

err_out_rx_q_alloc_failed:
	return ret;
}

static void ethqos_free_ipa_rx_queue_struct(struct qcom_ethqos *ethqos)
{
	kfree(eth_ipa_ctx.rx_queue->skb);
	eth_ipa_ctx.rx_queue->skb = NULL;

	kfree(eth_ipa_ctx.rx_queue->rx_desc_dma_addrs);
	eth_ipa_ctx.rx_queue->rx_desc_dma_addrs = NULL;

	kfree(eth_ipa_ctx.rx_queue->rx_desc_ptrs);
	eth_ipa_ctx.rx_queue->rx_desc_ptrs = NULL;

	kfree(eth_ipa_ctx.rx_queue->ipa_rx_buff_pool_va_addrs_base);
	eth_ipa_ctx.rx_queue->ipa_rx_buff_pool_va_addrs_base = NULL;

	kfree(eth_ipa_ctx.rx_queue->skb);
	eth_ipa_ctx.rx_queue->skb = NULL;
}

static void ethqos_rx_buf_free_mem(struct qcom_ethqos *ethqos,
				   unsigned int rx_qcnt)
{
	struct net_device *ndev = dev_get_drvdata(&ethqos->pdev->dev);
	struct stmmac_priv *priv = netdev_priv(ndev);

		/* Deallocate RX Buffer Pool Structure */
		/* Free memory pool for RX offload path */
		/* Currently only IPA_DMA_RX_CH is supported */
	if (eth_ipa_ctx.rx_queue->ipa_rx_buff_pool_pa_addrs_base) {
		dma_free_coherent
		(GET_MEM_PDEV_DEV,
		 sizeof(dma_addr_t) * eth_ipa_ctx.rx_queue->desc_cnt,
		 eth_ipa_ctx.rx_queue->ipa_rx_buff_pool_pa_addrs_base,
		 eth_ipa_ctx.rx_queue->ipa_rx_buff_pool_pa_addrs_base_dmahndl);
		eth_ipa_ctx.rx_queue->ipa_rx_buff_pool_pa_addrs_base = NULL;
		eth_ipa_ctx.rx_queue->ipa_rx_buff_pool_pa_addrs_base_dmahndl
		= (dma_addr_t)NULL;
			ETHQOSDBG("Freed Rx Buffer Pool Structure for IPA\n");
		} else {
			ETHQOSERR("Unable to DeAlloc RX Buff structure\n");
		}

	ETHQOSDBG("\n");
}

static void ethqos_rx_desc_free_mem(struct qcom_ethqos *ethqos,
				    unsigned int rx_qcnt)
{
	struct net_device *ndev = dev_get_drvdata(&ethqos->pdev->dev);
	struct stmmac_priv *priv = netdev_priv(ndev);

	ETHQOSDBG("-->DWC_ETH_QOS_rx_desc_free_mem: rx_qcnt = %d\n", rx_qcnt);

	if (eth_ipa_ctx.rx_queue->rx_desc_ptrs[0]) {
		dma_free_coherent
		(GET_MEM_PDEV_DEV,
		 (sizeof(struct dma_desc) *
		 eth_ipa_ctx.rx_queue->desc_cnt),
		 eth_ipa_ctx.rx_queue->rx_desc_ptrs[0],
		 eth_ipa_ctx.rx_queue->rx_desc_dma_addrs[0]);
		eth_ipa_ctx.rx_queue->rx_desc_ptrs[0] = NULL;
	}

	ETHQOSDBG("\n");
}

static void ethqos_tx_buf_free_mem(struct qcom_ethqos *ethqos,
				   unsigned int tx_qcnt)
{
	unsigned int i = 0;
	struct net_device *ndev = dev_get_drvdata(&ethqos->pdev->dev);
	struct stmmac_priv *priv = netdev_priv(ndev);

	ETHQOSDBG(": tx_qcnt = %d\n", tx_qcnt);

	for (i = 0; i < eth_ipa_ctx.tx_queue->desc_cnt; i++) {
		dma_free_coherent
		(GET_MEM_PDEV_DEV,
		 ETHQOS_ETH_FRAME_LEN_IPA,
		 eth_ipa_ctx.tx_queue->ipa_tx_buff_pool_va_addrs_base[i],
		 eth_ipa_ctx.tx_queue->ipa_tx_buff_pool_pa_addrs_base[i]);
	}
	ETHQOSDBG("Freed the memory allocated for IPA_DMA_TX_CH\n");
	/* De-Allocate TX DMA Buffer Pool Structure */
	if (eth_ipa_ctx.tx_queue->ipa_tx_buff_pool_pa_addrs_base) {
		dma_free_coherent
		(GET_MEM_PDEV_DEV,
		 sizeof(dma_addr_t) * eth_ipa_ctx.tx_queue->desc_cnt,
		 eth_ipa_ctx.tx_queue->ipa_tx_buff_pool_pa_addrs_base,
		 eth_ipa_ctx.tx_queue->ipa_tx_buff_pool_pa_addrs_base_dmahndl);
		eth_ipa_ctx.tx_queue->ipa_tx_buff_pool_pa_addrs_base = NULL;
		eth_ipa_ctx.tx_queue->ipa_tx_buff_pool_pa_addrs_base_dmahndl
		= (dma_addr_t)NULL;
		ETHQOSDBG("Freed TX Buffer Pool Structure for IPA\n");
		} else {
			ETHQOSDBG("Unable to DeAlloc TX Buff structure\n");
	}
	ETHQOSDBG("\n");
}

static void ethqos_tx_desc_free_mem(struct qcom_ethqos *ethqos,
				    unsigned int tx_qcnt)
{
	struct net_device *ndev = dev_get_drvdata(&ethqos->pdev->dev);
	struct stmmac_priv *priv = netdev_priv(ndev);

	ETHQOSDBG("tx_qcnt = %d\n", tx_qcnt);

	if (eth_ipa_ctx.tx_queue->tx_desc_ptrs[0]) {
		dma_free_coherent
		(GET_MEM_PDEV_DEV,
		 (sizeof(struct dma_desc) *
		 eth_ipa_ctx.tx_queue->desc_cnt),
		 eth_ipa_ctx.tx_queue->tx_desc_ptrs[0],
		 eth_ipa_ctx.tx_queue->tx_desc_dma_addrs[0]);
		eth_ipa_ctx.tx_queue->tx_desc_ptrs[0] = NULL;
	}

	ETHQOSDBG("\n");
}

static int allocate_ipa_buffer_and_desc(struct qcom_ethqos *ethqos)
{
	int ret = 0;
	unsigned int qinx = 0;
	struct net_device *ndev = dev_get_drvdata(&ethqos->pdev->dev);
	struct stmmac_priv *priv = netdev_priv(ndev);

	/* TX descriptors */
	eth_ipa_ctx.tx_queue->tx_desc_ptrs[0] = dma_alloc_coherent(
	   GET_MEM_PDEV_DEV,
	   (sizeof(struct dma_desc) * eth_ipa_ctx.tx_queue->desc_cnt),
		(eth_ipa_ctx.tx_queue->tx_desc_dma_addrs),
		GFP_KERNEL);
	if (!eth_ipa_ctx.tx_queue->tx_desc_ptrs[0]) {
		ret = -ENOMEM;
		goto err_out_tx_desc;
	}
	ETHQOSDBG("Tx Queue(%d) desc base dma address: %pK\n",
		  qinx, eth_ipa_ctx.tx_queue->tx_desc_dma_addrs);

	/* Allocate descriptors and buffers memory for all RX queues */
	/* RX descriptors */
	eth_ipa_ctx.rx_queue->rx_desc_ptrs[0] = dma_alloc_coherent(
	   GET_MEM_PDEV_DEV,
	   (sizeof(struct dma_desc) * eth_ipa_ctx.rx_queue->desc_cnt),
	   (eth_ipa_ctx.rx_queue->rx_desc_dma_addrs), GFP_KERNEL);
	if (!eth_ipa_ctx.rx_queue->rx_desc_ptrs[0]) {
		ret = -ENOMEM;
		goto rx_alloc_failure;
	}
	ETHQOSDBG("Rx Queue(%d) desc base dma address: %pK\n",
		  qinx, eth_ipa_ctx.rx_queue->rx_desc_dma_addrs);

	ETHQOSDBG("\n");

	return ret;

 rx_alloc_failure:
	ethqos_rx_desc_free_mem(ethqos, qinx);

 err_out_tx_desc:
	ethqos_tx_desc_free_mem(ethqos, qinx);

	return ret;
}

static void ethqos_ipa_tx_desc_init(struct qcom_ethqos *ethqos,
				    unsigned int QINX)
{
	int i = 0;
	struct dma_desc *TX_NORMAL_DESC;

	ETHQOSDBG("-->tx_descriptor_init\n");

	/* initialze all descriptors. */

	for (i = 0; i < eth_ipa_ctx.tx_queue->desc_cnt; i++) {
		TX_NORMAL_DESC = eth_ipa_ctx.tx_queue->tx_desc_ptrs[i];
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
	DMA_TDRLR_RGWR(QINX, (eth_ipa_ctx.tx_queue->desc_cnt - 1));
	/* update the starting address of desc chain/ring */
	DMA_TDLAR_RGWR(QINX, eth_ipa_ctx.tx_queue->tx_desc_dma_addrs[0]);

	ETHQOSDBG("\n");
}

static void ethqos_ipa_rx_desc_init(struct qcom_ethqos *ethqos,
				    unsigned int QINX)
{
	int cur_rx = 0;
	struct dma_desc *RX_NORMAL_DESC;
	int i;
	int start_index = cur_rx;
	int last_index;
	unsigned int VARRDES3 = 0;

	ETHQOSDBG("\n");

	/* initialize all desc */

	for (i = 0; i < eth_ipa_ctx.rx_queue->desc_cnt; i++) {
		RX_NORMAL_DESC = eth_ipa_ctx.rx_queue->rx_desc_ptrs[i];
		memset(RX_NORMAL_DESC, 0, sizeof(struct dma_desc));
		/* update buffer 1 address pointer */
		RX_NORMAL_DESC->des0
		= cpu_to_le32(eth_ipa_ctx.rx_queue->skb_dma[i]);

		/* set to zero  */
		RX_NORMAL_DESC->des1 = 0;

		/* set buffer 2 address pointer to zero */
		RX_NORMAL_DESC->des2 = 0;
		/* set control bits - OWN, INTE and BUF1V */
		RX_NORMAL_DESC->des3 = cpu_to_le32(0xc1000000);

		/* Don't Set the IOC bit for IPA controlled Desc */
		VARRDES3 = le32_to_cpu(RX_NORMAL_DESC->des3);
			/* reset IOC for all buffers */
		RX_NORMAL_DESC->des3 = cpu_to_le32(VARRDES3 & ~(1 << 30));
	}

	/* update the total no of Rx descriptors count */
	DMA_RDRLR_RGWR(QINX, (eth_ipa_ctx.rx_queue->desc_cnt - 1));
	/* update the Rx Descriptor Tail Pointer */
	last_index
	= GET_RX_CURRENT_RCVD_LAST_DESC_INDEX(start_index, 0,
					      eth_ipa_ctx.rx_queue->desc_cnt);
	DMA_RDTP_RPDR_RGWR(QINX,
			   eth_ipa_ctx.rx_queue->rx_desc_dma_addrs[last_index]);
	/* update the starting address of desc chain/ring */
	DMA_RDLAR_RGWR(QINX,
		       eth_ipa_ctx.rx_queue->rx_desc_dma_addrs[start_index]);

	ETHQOSDBG("\n");
}

static int ethqos_alloc_ipa_rx_buf(struct qcom_ethqos *ethqos,
				   unsigned int i, gfp_t gfp)
{
	unsigned int rx_buffer_len;
	dma_addr_t ipa_rx_buf_dma_addr;
	struct sg_table *buff_sgt;
	int ret = 0;
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);

	ETHQOSDBG("\n");

	rx_buffer_len = ETHQOS_ETH_FRAME_LEN_IPA;
	eth_ipa_ctx.rx_queue->ipa_buff_va[i] = dma_alloc_coherent(
	   GET_MEM_PDEV_DEV, rx_buffer_len,
	   &ipa_rx_buf_dma_addr, GFP_KERNEL);

	if (!eth_ipa_ctx.rx_queue->ipa_buff_va[i]) {
		dev_alert(&ethqos->pdev->dev,
			  "Failed to allocate RX dma buf for IPA\n");
		return -ENOMEM;
	}

	eth_ipa_ctx.rx_queue->skb_dma[i] = ipa_rx_buf_dma_addr;

	buff_sgt = kzalloc(sizeof(*buff_sgt), GFP_KERNEL);
	if (buff_sgt) {
		ret = dma_get_sgtable(GET_MEM_PDEV_DEV, buff_sgt,
				      eth_ipa_ctx.rx_queue->ipa_buff_va[i],
				      ipa_rx_buf_dma_addr,
				      rx_buffer_len);
		if (ret == 0) {
			eth_ipa_ctx.rx_queue->ipa_rx_buff_phy_addr[i]
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
			unsigned int qinx)
{
	int i;
	struct dma_desc *desc = eth_ipa_ctx.tx_queue->tx_desc_ptrs[0];
	dma_addr_t desc_dma = eth_ipa_ctx.tx_queue->tx_desc_dma_addrs[0];
	void *ipa_tx_buf_vaddr;
	dma_addr_t ipa_tx_buf_dma_addr;
	struct sg_table *buff_sgt;
	int ret  = 0;
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);

	ETHQOSDBG("qinx = %u\n", qinx);

	/* Allocate TX Buffer Pool Structure */
	eth_ipa_ctx.tx_queue->ipa_tx_buff_pool_pa_addrs_base =
	dma_zalloc_coherent
	(GET_MEM_PDEV_DEV, sizeof(dma_addr_t) *
	 eth_ipa_ctx.tx_queue->desc_cnt,
	 &eth_ipa_ctx.tx_queue->ipa_tx_buff_pool_pa_addrs_base_dmahndl,
	 GFP_KERNEL);
	if (!eth_ipa_ctx.tx_queue->ipa_tx_buff_pool_pa_addrs_base) {
		ETHQOSERR("ERROR: Unable to allocate IPA TX Buff structure\n");
		return;
	}

	ETHQOSDBG("IPA tx_dma_buff_addrs = %pK\n",
		  eth_ipa_ctx.tx_queue->ipa_tx_buff_pool_pa_addrs_base);

	for (i = 0; i < eth_ipa_ctx.tx_queue->desc_cnt; i++) {
		eth_ipa_ctx.tx_queue->tx_desc_ptrs[i] = &desc[i];
		eth_ipa_ctx.tx_queue->tx_desc_dma_addrs[i] =
		    (desc_dma + sizeof(struct dma_desc) * i);

		/* Create a memory pool for TX offload path */
		/* Currently only IPA_DMA_TX_CH is supported */
		ipa_tx_buf_vaddr
		= dma_alloc_coherent(GET_MEM_PDEV_DEV, ETHQOS_ETH_FRAME_LEN_IPA,
				     &ipa_tx_buf_dma_addr, GFP_KERNEL);
		if (!ipa_tx_buf_vaddr) {
			ETHQOSERR("Failed to allocate TX buf for IPA\n");
			return;
		}
		eth_ipa_ctx.tx_queue->ipa_tx_buff_pool_va_addrs_base[i]
		= ipa_tx_buf_vaddr;
		eth_ipa_ctx.tx_queue->ipa_tx_buff_pool_pa_addrs_base[i]
		= ipa_tx_buf_dma_addr;
		buff_sgt = kzalloc(sizeof(*buff_sgt), GFP_KERNEL);
		if (buff_sgt) {
			ret = dma_get_sgtable(GET_MEM_PDEV_DEV, buff_sgt,
					      ipa_tx_buf_vaddr,
					      ipa_tx_buf_dma_addr,
					      ETHQOS_ETH_FRAME_LEN_IPA);
			if (ret == 0) {
				eth_ipa_ctx.tx_queue->ipa_tx_buff_phy_addr[i] =
				sg_phys(buff_sgt->sgl);
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
	if (ethqos->ipa_enabled && qinx == IPA_DMA_TX_CH)
		ETHQOSDBG("DMA MAPed the virtual address for %d descs\n",
			  eth_ipa_ctx.tx_queue[IPA_DMA_TX_CH].desc_cnt);

	ethqos_ipa_tx_desc_init(ethqos, qinx);
}

static void ethqos_wrapper_rx_descriptor_init_single_q(
			struct qcom_ethqos *ethqos,
			unsigned int qinx)
{
	int i;
	struct dma_desc *desc = eth_ipa_ctx.rx_queue->rx_desc_ptrs[0];
	dma_addr_t desc_dma = eth_ipa_ctx.rx_queue->rx_desc_dma_addrs[0];
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);

	ETHQOSDBG("\n");
	/* Allocate RX Buffer Pool Structure */
	if (!eth_ipa_ctx.rx_queue->ipa_rx_buff_pool_pa_addrs_base) {
		eth_ipa_ctx.rx_queue->ipa_rx_buff_pool_pa_addrs_base
		= dma_zalloc_coherent
		(GET_MEM_PDEV_DEV,
		 sizeof(dma_addr_t) *
		 eth_ipa_ctx.rx_queue->desc_cnt,
		 &eth_ipa_ctx.rx_queue->ipa_rx_buff_pool_pa_addrs_base_dmahndl,
		 GFP_KERNEL);
		if (!eth_ipa_ctx.rx_queue->ipa_rx_buff_pool_pa_addrs_base) {
			ETHQOSERR("Unable to allocate structure\n");
			return;
		}

		ETHQOSDBG
		("IPA rx_buff_addrs %pK\n",
		 eth_ipa_ctx.rx_queue->ipa_rx_buff_pool_pa_addrs_base);
	}

	for (i = 0; i < eth_ipa_ctx.rx_queue[qinx].desc_cnt; i++) {
		eth_ipa_ctx.rx_queue->rx_desc_ptrs[i] = &desc[i];
		eth_ipa_ctx.rx_queue->rx_desc_dma_addrs[i] =
		    (desc_dma + sizeof(struct dma_desc) * i);

		/* allocate skb & assign to each desc */
		if (ethqos_alloc_ipa_rx_buf(ethqos, i, GFP_KERNEL))
			break;

		/* Assign the RX memory pool for offload data path */
		eth_ipa_ctx.rx_queue->ipa_rx_buff_pool_va_addrs_base[i]
		= eth_ipa_ctx.rx_queue->ipa_buff_va[i];
		eth_ipa_ctx.rx_queue->ipa_rx_buff_pool_pa_addrs_base[i]
		= eth_ipa_ctx.rx_queue->skb_dma[i];

		/* alloc_rx_buf */
		wmb();
	}

	ETHQOSDBG("Allocated %d buffers for RX Channel: %d\n",
		  eth_ipa_ctx.rx_queue[qinx].desc_cnt, qinx);
	if (ethqos->ipa_enabled && qinx == IPA_DMA_RX_CH)
		ETHQOSDBG("virtual memory pool address for RX for %d desc\n",
			  eth_ipa_ctx.rx_queue[IPA_DMA_RX_CH].desc_cnt);

	ethqos_ipa_rx_desc_init(ethqos, qinx);
}

static int ethqos_set_ul_dl_smmu_ipa_params(struct qcom_ethqos *ethqos,
					    struct ipa_ntn_setup_info *ul,
					    struct ipa_ntn_setup_info *dl)
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
			      eth_ipa_ctx.rx_queue->rx_desc_ptrs[0],
			      eth_ipa_ctx.rx_queue->rx_desc_dma_addrs[0],
			      (sizeof(struct dma_desc) *
			      eth_ipa_ctx.rx_queue->desc_cnt));
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
	       eth_ipa_ctx.rx_queue->ipa_rx_buff_pool_pa_addrs_base,
	       eth_ipa_ctx.rx_queue->ipa_rx_buff_pool_pa_addrs_base_dmahndl,
	       (sizeof(struct dma_desc) *
	       eth_ipa_ctx.rx_queue->desc_cnt));
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
			      eth_ipa_ctx.tx_queue->tx_desc_ptrs[0],
			      eth_ipa_ctx.tx_queue->tx_desc_dma_addrs[0],
			      (sizeof(struct dma_desc) *
			      eth_ipa_ctx.tx_queue->desc_cnt));
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
	       eth_ipa_ctx.tx_queue->ipa_tx_buff_pool_pa_addrs_base,
	       eth_ipa_ctx.tx_queue->ipa_tx_buff_pool_pa_addrs_base_dmahndl,
	       (sizeof(struct dma_desc) *
	       eth_ipa_ctx.tx_queue[IPA_DMA_TX_CH].desc_cnt));
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

static int enable_tx_dma_interrupts(unsigned int QINX,
				    struct qcom_ethqos *ethqos)
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

	/* Enable following interrupts for Queue 0 */
	/* NIE - Normal Interrupt Summary Enable */
	/* AIE - Abnormal Interrupt Summary Enable */
	/* FBE - Fatal Bus Error Enable */
	/* TXSE - Transmit Stopped Enable */
	DMA_IER_RGRD(QINX, VARDMA_IER);
	/* Reset all Tx interrupt bits */
	VARDMA_IER = VARDMA_IER & DMA_TX_INT_RESET_MASK;

	VARDMA_IER = VARDMA_IER | ((0x1) << 1) |
	     ((0x1) << 12) | ((0x1) << 14) | ((0x1) << 15);

	DMA_IER_RGWR(QINX, VARDMA_IER);

	return 0;
}

static int enable_rx_dma_interrupts(unsigned int QINX,
				    struct qcom_ethqos *ethqos)
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

	DMA_IER_RGWR(QINX, VARDMA_IER);

	return 0;
}

static void ethqos_ipa_config_queues(struct qcom_ethqos *ethqos)
{
	ethqos_alloc_ipa_tx_queue_struct(ethqos);
	ethqos_alloc_ipa_rx_queue_struct(ethqos);
	allocate_ipa_buffer_and_desc(ethqos);
	ethqos_wrapper_tx_descriptor_init_single_q(ethqos, 0);
	ethqos_wrapper_rx_descriptor_init_single_q(ethqos, 0);
}

static void ethqos_configure_ipa_tx_dma_channel(unsigned int QINX,
						struct qcom_ethqos *ethqos)
{
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);

	ETHQOSDBG("\n");

	enable_tx_dma_interrupts(QINX, ethqos);
	/* Enable Operate on Second Packet for better tputs */
	DMA_TCR_OSP_UDFWR(QINX, 0x1);

	/* start TX DMA */
	priv->hw->dma->start_tx(priv->ioaddr, IPA_DMA_TX_CH);

	ETHQOSDBG("\n");
}

static void ethqos_configure_ipa_rx_dma_channel(unsigned int QINX,
						struct qcom_ethqos *ethqos)
{
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);

	ETHQOSDBG("\n");
	/*Select Rx Buffer size = 2048bytes */

	DMA_RCR_RBSZ_UDFWR(QINX, ETHQOS_ETH_FRAME_LEN_IPA);

	enable_rx_dma_interrupts(QINX, ethqos);

	/* start RX DMA */
	priv->hw->dma->start_rx(priv->ioaddr, IPA_DMA_RX_CH);

	ETHQOSDBG("\n");
}

static int ethqos_init_offload(struct qcom_ethqos *ethqos)
{
	struct stmmac_priv *priv = ethqos_get_priv(ethqos);

	ETHQOSDBG("\n");

	ethqos_configure_ipa_tx_dma_channel(IPA_DMA_TX_CH, ethqos);
	priv->hw->mac->map_mtl_to_dma(priv->hw, 0, 0);
	ethqos_configure_ipa_rx_dma_channel(IPA_DMA_RX_CH, ethqos);

	ETHQOSDBG("\n");
	return 0;
}

static void ntn_ipa_notify_cb(void *priv, enum ipa_dp_evt_type evt,
			      unsigned long data)
{
	struct qcom_ethqos *ethqos = eth_ipa_ctx.ethqos;
	struct ethqos_prv_ipa_data *eth_ipa = &eth_ipa_ctx;
	struct sk_buff *skb = (struct sk_buff *)data;
	struct iphdr *iph = NULL;
	int stat = NET_RX_SUCCESS;
	struct platform_device *pdev;
	struct net_device *dev;

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

	if (evt == IPA_RECEIVE) {
		/*Exception packets to network stack*/
		skb->dev = dev;
		skb_record_rx_queue(skb, IPA_DMA_RX_CH);

		if (true == *(u8 *)(skb->cb + 4)) {
			skb->protocol = htons(ETH_P_IP);
			iph = (struct iphdr *)skb->data;
		} else {
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
			eth_ipa_ctx.ipa_stats.ipa_ul_exception++;
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

static int ethqos_ipa_offload_init(struct qcom_ethqos *pdata)
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
	struct net_device *ndev = dev_get_drvdata(&pdata->pdev->dev);

	if (!pdata) {
		ETHQOSERR("Null Param\n");
		ret =  -1;
		return ret;
	}

	ret = ipa_is_vlan_mode(IPA_VLAN_IF_EMAC, &ipa_vlan_mode);
	if (ret) {
		ETHQOSERR("Could not read ipa_vlan_mode\n");
		/* In case of failure, fallback to non vlan mode */
		ipa_vlan_mode = 0;
	}

	ETHQOSDBG("IPA VLAN mode %d\n", ipa_vlan_mode);

	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));

	/* Building ETH Header */
	if (!eth_ipa_ctx.vlan_id || !ipa_vlan_mode) {
		memset(&eth_l2_hdr_v4, 0, sizeof(eth_l2_hdr_v4));
		memset(&eth_l2_hdr_v6, 0, sizeof(eth_l2_hdr_v6));
		memcpy(&eth_l2_hdr_v4.h_source, ndev->dev_addr, ETH_ALEN);
		eth_l2_hdr_v4.h_proto = htons(ETH_P_IP);
		memcpy(&eth_l2_hdr_v6.h_source, ndev->dev_addr, ETH_ALEN);
		eth_l2_hdr_v6.h_proto = htons(ETH_P_IPV6);
		in.hdr_info[0].hdr = (u8 *)&eth_l2_hdr_v4;
		in.hdr_info[0].hdr_len = ETH_HLEN;
		in.hdr_info[1].hdr = (u8 *)&eth_l2_hdr_v6;
		in.hdr_info[1].hdr_len = ETH_HLEN;
	}

#ifdef ETHQOS_IPA_OFFLOAD_VLAN
	if ((eth_ipa_ctx.vlan_id > MIN_VLAN_ID && eth_ipa_ctx.vlan_id <=
	    MAX_VLAN_ID) || ipa_vlan_mode) {
		memset(&eth_vlan_hdr_v4, 0, sizeof(eth_vlan_hdr_v4));
		memset(&eth_vlan_hdr_v6, 0, sizeof(eth_vlan_hdr_v6));
		memcpy(&eth_vlan_hdr_v4.h_source, ndev->dev_addr, ETH_ALEN);
		eth_vlan_hdr_v4.h_vlan_proto = htons(ETH_P_8021Q);
		eth_vlan_hdr_v4.h_vlan_encapsulated_proto = htons(ETH_P_IP);
		in.hdr_info[0].hdr = (u8 *)&eth_vlan_hdr_v4;
		in.hdr_info[0].hdr_len = VLAN_ETH_HLEN;
		in.hdr_info[0].hdr_type = IPA_HDR_L2_802_1Q;
		memcpy(&eth_vlan_hdr_v6.h_source, ndev->dev_addr, ETH_ALEN);
		eth_vlan_hdr_v6.h_vlan_proto = htons(ETH_P_8021Q);
		eth_vlan_hdr_v6.h_vlan_encapsulated_proto = htons(ETH_P_IPV6);
		in.hdr_info[1].hdr = (u8 *)&eth_vlan_hdr_v6;
		in.hdr_info[1].hdr_len = VLAN_ETH_HLEN;
		in.hdr_info[1].hdr_type = IPA_HDR_L2_802_1Q;
	}
#endif

	/* Building IN params */
	in.netdev_name = ndev->name;
	in.priv = pdata;
	in.notify = ntn_ipa_notify_cb;
	in.proto = IPA_UC_NTN;
	in.hdr_info[0].dst_mac_addr_offset = 0;
	in.hdr_info[0].hdr_type = IPA_HDR_L2_ETHERNET_II;
	in.hdr_info[1].dst_mac_addr_offset = 0;
	in.hdr_info[1].hdr_type = IPA_HDR_L2_ETHERNET_II;

	ret = ipa_uc_offload_reg_intf(&in, &out);
	if (ret) {
		ETHQOSERR("Could not register offload interface ret %d\n",
			  ret);
		ret = -1;
		return ret;
	}
	eth_ipa->ipa_client_hndl = out.clnt_hndl;
	ETHQOSDBG("Recevied IPA Offload Client Handle %d",
		  eth_ipa->ipa_client_hndl);

	pdata->ipa_enabled = true;
	return 0;
}

static int ethqos_ipa_offload_cleanup(struct qcom_ethqos *ethqos)
{
	struct ethqos_prv_ipa_data *eth_ipa = &eth_ipa_ctx;
	int ret = 0;

	ETHQOSDBG("begin\n");

	if (!ethqos) {
		ETHQOSERR("Null Param\n");
		return -ENOMEM;
	}

	if (!eth_ipa->ipa_client_hndl) {
		ETHQOSERR("cleanup called with NULL IPA client handle\n");
		return -ENOMEM;
	}

	ret = ipa_uc_offload_cleanup(eth_ipa->ipa_client_hndl);
	if (ret) {
		ETHQOSERR("Could not cleanup IPA Offload ret %d\n", ret);
		ret = -1;
	} else {
		eth_ipa_send_msg(ethqos, IPA_PERIPHERAL_DISCONNECT);
	}

	ETHQOSDBG("end\n");

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
		"IPA RX Packets: ", eth_ipa_ctx.ipa_stats.ipa_ul_exception);
	len += scnprintf(buf + len, buf_len - len, "\n");

	if (len > buf_len)
		len = buf_len;

	ret_cnt = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret_cnt;
}

static void ethqos_ipa_stats_read(struct qcom_ethqos *ethqos)
{
	struct ethqos_ipa_stats *dma_stats = &eth_ipa_ctx.ipa_stats;
	unsigned int data;

	if (!eth_ipa_ctx.rx_queue || !eth_ipa_ctx.tx_queue)
		return;

	dma_stats->ipa_rx_desc_ring_base
	= eth_ipa_ctx.rx_queue->rx_desc_dma_addrs[0];
	dma_stats->ipa_rx_desc_ring_size
	= eth_ipa_ctx.rx_queue[IPA_DMA_RX_CH].desc_cnt;
	dma_stats->ipa_rx_buff_ring_base
	= eth_ipa_ctx.rx_queue->ipa_rx_buff_pool_pa_addrs_base_dmahndl;
	dma_stats->ipa_rx_buff_ring_size
	= eth_ipa_ctx.rx_queue[IPA_DMA_RX_CH].desc_cnt - 1;

	//@RK: IPA_INTEG Need Rx db received cnt from IPA uC
	dma_stats->ipa_rx_db_int_raised = 0;

	DMA_CHRDR_RGRD(IPA_DMA_RX_CH, data);
	dma_stats->ipa_rx_cur_desc_ptr_indx = GET_RX_DESC_IDX(IPA_DMA_RX_CH,
							      data);

	DMA_RDTP_RPDR_RGRD(IPA_DMA_RX_CH, data);
	dma_stats->ipa_rx_tail_ptr_indx = GET_RX_DESC_IDX(IPA_DMA_RX_CH, data);

	DMA_SR_RGRD(IPA_DMA_RX_CH, data);
	dma_stats->ipa_rx_dma_status = data;

	dma_stats->ipa_rx_dma_ch_underflow =
	GET_VALUE(data, DMA_SR_RBU_LPOS, DMA_SR_RBU_LPOS, DMA_SR_RBU_HPOS);

	dma_stats->ipa_rx_dma_ch_stopped =
	GET_VALUE(data, DMA_SR_RPS_LPOS, DMA_SR_RPS_LPOS, DMA_SR_RPS_HPOS);

	dma_stats->ipa_rx_dma_ch_complete =
	GET_VALUE(data, DMA_SR_RI_LPOS, DMA_SR_RI_LPOS, DMA_SR_RI_HPOS);

	DMA_IER_RGRD(IPA_DMA_RX_CH, dma_stats->ipa_rx_int_mask);

	DMA_IER_RBUE_UDFRD(IPA_DMA_RX_CH, dma_stats->ipa_rx_underflow_irq);
	DMA_IER_ETIE_UDFRD(IPA_DMA_RX_CH,
			   dma_stats->ipa_rx_early_trans_comp_irq);

	dma_stats->ipa_tx_desc_ring_base
	= eth_ipa_ctx.tx_queue->tx_desc_dma_addrs[0];
	dma_stats->ipa_tx_desc_ring_size
	= eth_ipa_ctx.tx_queue[IPA_DMA_TX_CH].desc_cnt;
	dma_stats->ipa_tx_buff_ring_base
	= eth_ipa_ctx.tx_queue->ipa_tx_buff_pool_pa_addrs_base_dmahndl;
	dma_stats->ipa_tx_buff_ring_size
	= eth_ipa_ctx.tx_queue[IPA_DMA_TX_CH].desc_cnt - 1;

	//@RK: IPA_INTEG Need Tx db received cnt from IPA uC
	dma_stats->ipa_tx_db_int_raised = 0;

	DMA_CHTDR_RGRD(IPA_DMA_TX_CH, data);
	dma_stats->ipa_tx_curr_desc_ptr_indx = GET_TX_DESC_IDX
					       (IPA_DMA_TX_CH, data);

	DMA_TDTP_TPDR_RGRD(IPA_DMA_TX_CH, data);
	dma_stats->ipa_tx_tail_ptr_indx = GET_TX_DESC_IDX(IPA_DMA_TX_CH, data);

	DMA_SR_RGRD(IPA_DMA_TX_CH, data);
	dma_stats->ipa_tx_dma_status = data;

	dma_stats->ipa_tx_dma_ch_underflow =
	GET_VALUE(data, DMA_SR_TBU_LPOS, DMA_SR_TBU_LPOS, DMA_SR_TBU_HPOS);

	dma_stats->ipa_tx_dma_transfer_stopped =
	GET_VALUE(data, DMA_SR_TPS_LPOS, DMA_SR_TPS_LPOS, DMA_SR_TPS_HPOS);

	dma_stats->ipa_tx_dma_transfer_complete =
	GET_VALUE(data, DMA_SR_TI_LPOS, DMA_SR_TI_LPOS, DMA_SR_TI_HPOS);

	DMA_IER_RGRD(IPA_DMA_TX_CH, dma_stats->ipa_tx_int_mask);
	DMA_IER_TIE_UDFRD(IPA_DMA_TX_CH,
			  dma_stats->ipa_tx_transfer_complete_irq);

	DMA_IER_TXSE_UDFRD(IPA_DMA_TX_CH,
			   dma_stats->ipa_tx_transfer_stopped_irq);

	DMA_IER_TBUE_UDFRD(IPA_DMA_TX_CH,
			   dma_stats->ipa_tx_underflow_irq);

	DMA_IER_ETIE_UDFRD(IPA_DMA_TX_CH,
			   dma_stats->ipa_tx_early_trans_cmp_irq);
	DMA_IER_FBEE_UDFRD(IPA_DMA_TX_CH, dma_stats->ipa_tx_fatal_err_irq);
	DMA_IER_CDEE_UDFRD(IPA_DMA_TX_CH, dma_stats->ipa_tx_desc_err_irq);
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
	struct ethqos_ipa_stats *dma_stats = &eth_ipa_ctx.ipa_stats;
	char *buf;
	unsigned int len = 0, buf_len = 3000;
	ssize_t ret_cnt;

	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ethqos_ipa_stats_read(ethqos);

	len += scnprintf(buf + len, buf_len - len, "\n\n");
	len += scnprintf(buf + len, buf_len - len, "%25s\n",
		"NTN DMA Stats");
	len += scnprintf(buf + len, buf_len - len, "%25s\n\n",
		"==================================================");

	len += scnprintf(buf + len, buf_len - len, "%-50s 0x%x\n",
		"RX Desc Ring Base: ", dma_stats->ipa_rx_desc_ring_base);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10d\n",
		"RX Desc Ring Size: ", dma_stats->ipa_rx_desc_ring_size);
	len += scnprintf(buf + len, buf_len - len, "%-50s 0x%x\n",
		"RX Buff Ring Base: ", dma_stats->ipa_rx_buff_ring_base);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10d\n",
		"RX Buff Ring Size: ", dma_stats->ipa_rx_buff_ring_size);
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
			 "RX Doorbell Address: ", eth_ipa->uc_db_rx_addr);
	len += scnprintf(buf + len, buf_len - len, "\n");

	len += scnprintf(buf + len, buf_len - len, "%-50s 0x%x\n",
			 "RX DMA Status: ", dma_stats->ipa_rx_dma_status);

	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
			 "RX DMA Status - RX DMA Underflow : ",
			 bit_status_string
			 [dma_stats->ipa_rx_dma_ch_underflow]);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
			 "RX DMA Status - RX DMA Stopped : ",
			 bit_status_string[dma_stats->ipa_rx_dma_ch_stopped]);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
			 "RX DMA Status - RX DMA Complete : ",
			 bit_status_string[dma_stats->ipa_rx_dma_ch_complete]);
	len += scnprintf(buf + len, buf_len - len, "\n");

	len += scnprintf(buf + len, buf_len - len, "%-50s 0x%x\n",
			 "RX DMA CH0 INT Mask: ", dma_stats->ipa_rx_int_mask);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
			 "RXDMACH0 INTMASK - Transfer Complete IRQ : ",
			 bit_mask_string
			 [dma_stats->ipa_rx_transfer_complete_irq]);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
			 "RXDMACH0 INTMASK - Transfer Stopped IRQ : ",
			 bit_mask_string
			 [dma_stats->ipa_rx_transfer_stopped_irq]);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
			 "RXDMACH0 INTMASK - Underflow IRQ : ",
			 bit_mask_string[dma_stats->ipa_rx_underflow_irq]);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
			 "RXDMACH0 INTMASK - Early Transmit Complete IRQ : ",
			 bit_mask_string
			 [dma_stats->ipa_rx_early_trans_comp_irq]);
	len += scnprintf(buf + len, buf_len - len, "\n");

	len += scnprintf(buf + len, buf_len - len, "%-50s 0x%x\n",
		"TX Desc Ring Base: ", dma_stats->ipa_tx_desc_ring_base);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10d\n",
		"TX Desc Ring Size: ", dma_stats->ipa_tx_desc_ring_size);
	len += scnprintf(buf + len, buf_len - len, "%-50s 0x%x\n",
		"TX Buff Ring Base: ", dma_stats->ipa_tx_buff_ring_base);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10d\n",
		"TX Buff Ring Size: ", dma_stats->ipa_tx_buff_ring_size);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10u\n",
			 "TX Doorbell Interrupts Raised: ",
			 dma_stats->ipa_tx_db_int_raised);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10lu\n",
			 "TX Current Desc Pointer Index: ",
			 dma_stats->ipa_tx_curr_desc_ptr_indx);

	len += scnprintf(buf + len, buf_len - len, "%-50s %10lu\n",
		"TX Tail Pointer Index: ", dma_stats->ipa_tx_tail_ptr_indx);
	len += scnprintf(buf + len, buf_len - len, "%-50s 0x%x\n",
		"TX Doorbell Address: ", eth_ipa->uc_db_tx_addr);
	len += scnprintf(buf + len, buf_len - len, "\n");

	len += scnprintf(buf + len, buf_len - len, "%-50s 0x%x\n",
		"TX DMA Status: ", dma_stats->ipa_tx_dma_status);

	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
			 "TX DMA Status - TX DMA Underflow : ",
			 bit_status_string
			 [dma_stats->ipa_tx_dma_ch_underflow]);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
			 "TX DMA Status - TX DMA Transfer Stopped : ",
			 bit_status_string
			 [dma_stats->ipa_tx_dma_transfer_stopped]);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
			 "TX DMA Status - TX DMA Transfer Complete : ",
			 bit_status_string
			 [dma_stats->ipa_tx_dma_transfer_complete]);
	len += scnprintf(buf + len, buf_len - len, "\n");

	len += scnprintf(buf + len, buf_len - len, "%-50s 0x%x\n",
			 "TX DMA CH2 INT Mask: ", dma_stats->ipa_tx_int_mask);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
			 "TXDMACH2 INTMASK - Transfer Complete IRQ : ",
			 bit_mask_string
			 [dma_stats->ipa_tx_transfer_complete_irq]);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
			 "TXDMACH2 INTMASK - Transfer Stopped IRQ : ",
			 bit_mask_string
			 [dma_stats->ipa_tx_transfer_stopped_irq]);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
			 "TXDMACH2 INTMASK - Underflow IRQ : ",
			 bit_mask_string[dma_stats->ipa_tx_underflow_irq]);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
			 "TXDMACH2 INTMASK - Early Transmit Complete IRQ : ",
			 bit_mask_string
			 [dma_stats->ipa_tx_early_trans_cmp_irq]);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
			 "TXDMACH2 INTMASK - Fatal Bus Error IRQ : ",
			 bit_mask_string[dma_stats->ipa_tx_fatal_err_irq]);
	len += scnprintf(buf + len, buf_len - len, "%-50s %10s\n",
			 "TXDMACH2 INTMASK - CNTX Desc Error IRQ : ",
			 bit_mask_string[dma_stats->ipa_tx_desc_err_irq]);

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

	ETHQOSDBG("IPA debugfs Deleted Successfully\n");
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
	if (!eth_ipa->debugfs_suspend_ipa_offload ||
	    IS_ERR(eth_ipa->debugfs_suspend_ipa_offload)) {
		ETHQOSERR("Cannot create debugfs_dma_stats %d\n",
			  (int)eth_ipa->debugfs_dma_stats);
		goto fail;
	}

	return 0;

fail:
	ethqos_ipa_cleanup_debugfs(ethqos);
	return -ENOMEM;
}

static int ethqos_ipa_offload_connect(struct qcom_ethqos *ethqos)
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
	RGMII_GPIO_CFG_TX_INT_UDFWR(IPA_DMA_TX_CH);

	/* Configure interrupt route for ETHQOS RX DMA channel to IPA */
	RGMII_GPIO_CFG_RX_INT_UDFWR(IPA_DMA_RX_CH);

	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));
	memset(&profile, 0, sizeof(profile));

	in.clnt_hndl = eth_ipa->ipa_client_hndl;

	/* Uplink Setup */
	if (stmmac_emb_smmu_ctx.valid)
		rx_setup_info.smmu_enabled = true;
	else
		rx_setup_info.smmu_enabled = false;

	rx_setup_info.client = IPA_CLIENT_ETHERNET_PROD;
	if (!rx_setup_info.smmu_enabled)
		rx_setup_info.ring_base_pa
		= (phys_addr_t)eth_ipa_ctx.rx_queue->rx_desc_dma_addrs[0];
	rx_setup_info.ring_base_iova
	= eth_ipa_ctx.rx_queue->rx_desc_dma_addrs[0];
	rx_setup_info.ntn_ring_size
	= eth_ipa_ctx.rx_queue->desc_cnt;
	if (!rx_setup_info.smmu_enabled)
		rx_setup_info.buff_pool_base_pa
		= eth_ipa_ctx.rx_queue->ipa_rx_buff_pool_pa_addrs_base_dmahndl;
	rx_setup_info.buff_pool_base_iova
	= eth_ipa_ctx.rx_queue->ipa_rx_buff_pool_pa_addrs_base_dmahndl;
	rx_setup_info.num_buffers
	= eth_ipa_ctx.rx_queue[IPA_DMA_RX_CH].desc_cnt - 1;
	rx_setup_info.data_buff_size = ETHQOS_ETH_FRAME_LEN_IPA;

	/* Base address here is the address of ETHQOS_DMA_CH0_CONTROL
	 * in ETHQOS resgister space
	 */
	rx_setup_info.ntn_reg_base_ptr_pa = (phys_addr_t)
	(((unsigned long)(DMA_CR0_RGOFFADDR - BASE_ADDRESS))  +
	 (unsigned long)ethqos->emac_mem_base);
	/* Downlink Setup */
	if (stmmac_emb_smmu_ctx.valid)
		tx_setup_info.smmu_enabled = true;
	else
		tx_setup_info.smmu_enabled = false;

	tx_setup_info.client = IPA_CLIENT_ETHERNET_CONS;
	if (!tx_setup_info.smmu_enabled)
		tx_setup_info.ring_base_pa
		= (phys_addr_t)eth_ipa_ctx.tx_queue->tx_desc_dma_addrs[0];

	tx_setup_info.ring_base_iova
	= eth_ipa_ctx.tx_queue->tx_desc_dma_addrs[0];
	tx_setup_info.ntn_ring_size = eth_ipa_ctx.tx_queue->desc_cnt;
	if (!tx_setup_info.smmu_enabled)
		tx_setup_info.buff_pool_base_pa
		= eth_ipa_ctx.tx_queue->ipa_tx_buff_pool_pa_addrs_base_dmahndl;
	tx_setup_info.buff_pool_base_iova
	= eth_ipa_ctx.tx_queue->ipa_tx_buff_pool_pa_addrs_base_dmahndl;
	tx_setup_info.num_buffers
	= eth_ipa_ctx.tx_queue->desc_cnt - 1;
	tx_setup_info.data_buff_size = ETHQOS_ETH_FRAME_LEN_IPA;

	/* Base address here is the address of ETHQOS_DMA_CH0_CONTROL
	 * in ETHQOS resgister space
	 */
	tx_setup_info.ntn_reg_base_ptr_pa = (phys_addr_t)
	(((unsigned long)(DMA_CR0_RGOFFADDR - BASE_ADDRESS))
	  + (unsigned long)ethqos->emac_mem_base);

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
		rx_setup_info.data_buff_list[i].iova
		= eth_ipa_ctx.rx_queue->ipa_rx_buff_pool_pa_addrs_base[i];
		if (!rx_setup_info.smmu_enabled) {
			rx_setup_info.data_buff_list[i].pa
			= rx_setup_info.data_buff_list[i].iova;
		} else {
			rx_setup_info.data_buff_list[i].pa
			= eth_ipa_ctx.rx_queue->ipa_rx_buff_phy_addr[i];
		}
	}
	for (i = 0; i < tx_setup_info.num_buffers; i++) {
		tx_setup_info.data_buff_list[i].iova
		= eth_ipa_ctx.tx_queue->ipa_tx_buff_pool_pa_addrs_base[i];
		if (!tx_setup_info.smmu_enabled)
			tx_setup_info.data_buff_list[i].pa
			= tx_setup_info.data_buff_list[i].iova;
		else
			tx_setup_info.data_buff_list[i].pa
			= eth_ipa_ctx.tx_queue->ipa_tx_buff_phy_addr[i];
	}

	if (stmmac_emb_smmu_ctx.valid) {
		ret = ethqos_set_ul_dl_smmu_ipa_params(ethqos, &rx_setup_info,
						       &tx_setup_info);
		if (ret) {
			ETHQOSERR("Failed to build ipa_setup_info :%d\n", ret);
			ret = -1;
			goto mem_free;
		}
	}

	in.u.ntn.ul = rx_setup_info;
	in.u.ntn.dl = tx_setup_info;

	ret = ipa_uc_offload_conn_pipes(&in, &out);
	if (ret) {
		ETHQOSERR("Could not connect IPA Offload Pipes %d\n", ret);
		ret = -1;
		goto mem_free;
	}

	eth_ipa->uc_db_rx_addr = out.u.ntn.ul_uc_db_pa;
	eth_ipa->uc_db_tx_addr = out.u.ntn.dl_uc_db_pa;

	/* Set Perf Profile For PROD/CONS Pipes */
	profile.max_supported_bw_mbps = ethqos->speed;
	profile.client = IPA_CLIENT_ETHERNET_PROD;
	ret = ipa_set_perf_profile(&profile);
	if (ret) {
		ETHQOSERR("Err to set BW: IPA_RM_RESOURCE_ETHERNET_PROD :%d\n",
			  ret);
		ret = -1;
		goto mem_free;
	}

	profile.client = IPA_CLIENT_ETHERNET_CONS;
	ret = ipa_set_perf_profile(&profile);
	if (ret) {
		ETHQOSERR("Err to set BW: IPA_RM_RESOURCE_ETHERNET_CONS :%d\n",
			  ret);
		ret = -1;
		goto mem_free;
	}
	eth_ipa_send_msg(ethqos, IPA_PERIPHERAL_CONNECT);
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
	return 0;
}

static int ethqos_ipa_offload_disconnect(struct qcom_ethqos *ethqos)
{
	struct ethqos_prv_ipa_data *eth_ipa = &eth_ipa_ctx;
	int ret = 0;

	ETHQOSDBG("- begin\n");

	if (!ethqos) {
		ETHQOSERR("Null Param\n");
		return -ENOMEM;
	}

	ret = ipa_uc_offload_disconn_pipes(eth_ipa->ipa_client_hndl);
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

	ETHQOSDBG("Suspend/disable IPA offload\n");

	priv->hw->dma->stop_rx(priv->ioaddr, IPA_DMA_RX_CH);
	if (ret != 0) {
		ETHQOSERR("stop_dma_rx failed %d\n", ret);
		return ret;
	}

	/* Disconnect IPA offload */
	if (eth_ipa_ctx.ipa_offload_conn) {
		ret = ethqos_ipa_offload_disconnect(ethqos);
		if (ret) {
			ETHQOSERR("IPA Offload Disconnect Failed :%d\n", ret);
			return ret;
		}
		eth_ipa_ctx.ipa_offload_conn = false;
		ETHQOSDBG("IPA Offload Disconnect Successfully\n");
	}

	priv->hw->dma->stop_tx(priv->ioaddr, IPA_DMA_TX_CH);

	if (ret != 0) {
		ETHQOSERR("stop_dma_tx failed %d\n", ret);
		return ret;
	}

	if (eth_ipa_ctx.ipa_uc_ready) {
		profile.max_supported_bw_mbps = 0;
		profile.client = IPA_CLIENT_ETHERNET_CONS;
		ret = ipa_set_perf_profile(&profile);
		if (ret)
			ETHQOSERR("Err set IPA_RM_RESOURCE_ETHERNET_CONS:%d\n",
				  ret);
	}

	if (eth_ipa_ctx.ipa_offload_init) {
		ret = ethqos_ipa_offload_cleanup(ethqos);
		if (ret) {
			ETHQOSERR("IPA Offload Cleanup Failed, err: %d\n", ret);
			return ret;
		}
		ETHQOSDBG("IPA Offload Cleanup Success\n");
		eth_ipa_ctx.ipa_offload_init = false;
	}

	return ret;
}

static int ethqos_ipa_offload_resume(struct qcom_ethqos *ethqos)
{
	int ret = 1;
	struct ipa_perf_profile profile;

	ETHQOSDBG("Enter\n");

	if (!eth_ipa_ctx.ipa_offload_init) {
		if (!ethqos_ipa_offload_init(ethqos)) {
			eth_ipa_ctx.ipa_offload_init = true;
		} else {
			eth_ipa_ctx.ipa_offload_init = false;
			ETHQOSERR("PA Offload Init Failed\n");
		}
	}

	/* Initialze descriptors before IPA connect */
	/* Set IPA owned DMA channels to reset state */
	ethqos_ipa_tx_desc_init(ethqos, IPA_DMA_TX_CH);
	ethqos_ipa_rx_desc_init(ethqos, IPA_DMA_RX_CH);

	ETHQOSDBG("DWC_ETH_QOS_ipa_offload_connect\n");
	ret = ethqos_ipa_offload_connect(ethqos);
	if (ret != 0)
		goto fail;
	else
		eth_ipa_ctx.ipa_offload_conn = true;

	profile.max_supported_bw_mbps = ethqos->speed;
	profile.client = IPA_CLIENT_ETHERNET_CONS;
	ret = ipa_set_perf_profile(&profile);
	if (ret)
		ETHQOSERR("Err to set BW: IPA_RM_RESOURCE_ETHERNET_CONS :%d\n",
			  ret);
	/*Initialize DMA CHs for offload*/
	ethqos_init_offload(ethqos);
	if (ret) {
		ETHQOSERR("Offload channel Init Failed\n");
		return ret;
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

	if (!eth_ipa_ctx.ipa_offload_init) {
		ret = ethqos_ipa_offload_init(ethqos);
		if (ret) {
			eth_ipa_ctx.ipa_offload_init = false;
			ETHQOSERR("IPA Offload Init Failed\n");
			goto fail;
		}
		ETHQOSDBG("IPA Offload Initialized Successfully\n");
		eth_ipa_ctx.ipa_offload_init = true;
	}
	if (!eth_ipa_ctx.ipa_offload_conn &&
	    !eth_ipa_ctx.ipa_offload_susp) {
		ret = ethqos_ipa_offload_connect(ethqos);
		if (ret) {
			ETHQOSERR("IPA Offload Connect Failed\n");
			eth_ipa_ctx.ipa_offload_conn = false;
			goto fail;
		}
		ETHQOSDBG("IPA Offload Connect Successfully\n");
		eth_ipa_ctx.ipa_offload_conn = true;

		/*Initialize DMA CHs for offload*/
		ret = ethqos_init_offload(ethqos);
		if (ret) {
			ETHQOSERR("Offload channel Init Failed\n");
			goto fail;
		}
	}

	if (!eth_ipa_ctx.ipa_debugfs_exists) {
		if (!ethqos_ipa_create_debugfs(ethqos)) {
			ETHQOSDBG("eMAC Debugfs created\n");
			eth_ipa_ctx.ipa_debugfs_exists = true;
		} else {
			ETHQOSERR("eMAC Debugfs failed\n");
		}
	}

	ETHQOSDBG("IPA Offload Enabled successfully\n");
	return ret;

fail:

	if (eth_ipa_ctx.ipa_offload_conn) {
		if (ethqos_ipa_offload_disconnect(ethqos))
			ETHQOSERR("IPA Offload Disconnect Failed\n");
		else
			ETHQOSDBG("IPA Offload Disconnect Successfully\n");
		eth_ipa_ctx.ipa_offload_conn = false;
	}

	if (eth_ipa_ctx.ipa_offload_init) {
		if (ethqos_ipa_offload_cleanup(ethqos))
			ETHQOSERR("IPA Offload Cleanup Failed\n");
		else
			ETHQOSDBG("IPA Offload Cleanup Success\n");
		eth_ipa_ctx.ipa_offload_init = false;
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

	ETHQOSDBG("Received IPA UC ready callback\n");
	INIT_WORK(&eth_ipa->ntn_ipa_rdy_work, ethqos_ipaucrdy_wq);
	queue_work(system_unbound_wq, &eth_ipa->ntn_ipa_rdy_work);
}

static int ethqos_ipa_uc_ready(struct qcom_ethqos *pdata)
{
	struct ipa_uc_ready_params param;
	int ret;

	ETHQOSDBG("Enter\n");

	param.is_uC_ready = false;
	param.priv = pdata;
	param.notify = ethqos_ipa_uc_ready_cb;
	param.proto = IPA_UC_NTN;

	ret = ipa_uc_offload_reg_rdyCB(&param);
	if (ret == 0 && param.is_uC_ready) {
		ETHQOSDBG("ipa uc ready\n");
		eth_ipa_ctx.ipa_uc_ready = true;
	}
	ETHQOSDBG("Exit\n");
	return ret;
}

void ethqos_ipa_offload_event_handler(void *data,
				      int ev)
{
	ETHQOSDBG("Enter: event=%d\n", ev);

	if (ev == EV_PROBE_INIT) {
		eth_ipa_ctx.ethqos = data;
		mutex_init(&eth_ipa_ctx.ipa_lock);
		return;
	}

	IPA_LOCK();

	switch (ev) {
	case EV_PHY_LINK_DOWN:
		{
			if (!eth_ipa_ctx.emac_dev_ready ||
			    !eth_ipa_ctx.ipa_uc_ready ||
			    eth_ipa_ctx.ipa_offload_link_down ||
			    eth_ipa_ctx.ipa_offload_susp ||
			    !eth_ipa_ctx.ipa_offload_conn)
				break;

			if (!ethqos_ipa_offload_suspend(eth_ipa_ctx.ethqos))
				eth_ipa_ctx.ipa_offload_link_down = true;
		}
		break;
	case EV_PHY_LINK_UP:
		{
			if (!eth_ipa_ctx.emac_dev_ready ||
			    !eth_ipa_ctx.ipa_uc_ready ||
			    eth_ipa_ctx.ipa_offload_susp)
				break;

			/* Link up event is expected only after link down */
			if (eth_ipa_ctx.ipa_offload_link_down) {
				ethqos_ipa_offload_resume(eth_ipa_ctx.ethqos);
			} else if (eth_ipa_ctx.emac_dev_ready &&
				eth_ipa_ctx.ipa_uc_ready) {
				ethqos_enable_ipa_offload(eth_ipa_ctx.ethqos);
			}

			eth_ipa_ctx.ipa_offload_link_down = false;
		}
		break;
	case EV_DEV_OPEN:
		{
			ethqos_ipa_config_queues(eth_ipa_ctx.ethqos);
			eth_ipa_ctx.emac_dev_ready = true;

			if (!eth_ipa_ctx.ipa_ready)
				ethqos_ipa_ready(eth_ipa_ctx.ethqos);

			if (!eth_ipa_ctx.ipa_uc_ready)
				ethqos_ipa_uc_ready(eth_ipa_ctx.ethqos);
		}
		break;
	case EV_IPA_READY:
		{
			eth_ipa_ctx.ipa_ready = true;

			if (!eth_ipa_ctx.ipa_uc_ready)
				ethqos_ipa_uc_ready(eth_ipa_ctx.ethqos);

			if (eth_ipa_ctx.ipa_uc_ready &&
			    qcom_ethqos_is_phy_link_up(eth_ipa_ctx.ethqos))
				ethqos_enable_ipa_offload(eth_ipa_ctx.ethqos);
		}
		break;
	case EV_IPA_UC_READY:
		{
			eth_ipa_ctx.ipa_uc_ready = true;
			ETHQOSDBG("ipa uC is ready\n");

			if (!eth_ipa_ctx.emac_dev_ready)
				break;
			if (eth_ipa_ctx.ipa_ready) {
				if (!eth_ipa_ctx.ipa_offload_init) {
					if (!ethqos_ipa_offload_init(
							eth_ipa_ctx.ethqos))
						eth_ipa_ctx.ipa_offload_init
						= true;
					}
				}
			if (qcom_ethqos_is_phy_link_up(eth_ipa_ctx.ethqos))
				ethqos_enable_ipa_offload(eth_ipa_ctx.ethqos);
		}
		break;
	case EV_DEV_CLOSE:
		{
			eth_ipa_ctx.emac_dev_ready = false;

			if (eth_ipa_ctx.ipa_uc_ready)
				ipa_uc_offload_dereg_rdyCB(IPA_UC_NTN);

			ethqos_disable_ipa_offload(eth_ipa_ctx.ethqos);

			/* reset link down on dev close */
			eth_ipa_ctx.ipa_offload_link_down = 0;
		}
		break;
	case EV_DPM_SUSPEND:
		{
			if (eth_ipa_ctx.ipa_offload_conn)
				*(int *)data = false;
			else
				*(int *)data = true;
		}
		break;
	case EV_USR_SUSPEND:
		{
			if (!eth_ipa_ctx.ipa_offload_susp &&
				!eth_ipa_ctx.ipa_offload_link_down)
				if (!ethqos_ipa_offload_suspend(
						eth_ipa_ctx.ethqos))
					eth_ipa_ctx.ipa_offload_susp
					= true;
		}
		break;
	case EV_DPM_RESUME:
		{
			if (qcom_ethqos_is_phy_link_up(eth_ipa_ctx.ethqos)) {
				if (!ethqos_ipa_offload_resume(
						eth_ipa_ctx.ethqos))
					eth_ipa_ctx.ipa_offload_susp
					= false;
			} else {
				/* Reset flag here to allow connection
				 * of pipes on next PHY link up
				 */
				eth_ipa_ctx.ipa_offload_susp
				= false;
				/* PHY link is down at resume */
				/* Reset flag here to allow connection
				 * of pipes on next PHY link up
				 */
				eth_ipa_ctx.ipa_offload_link_down
				= true;
			}
	}
		break;
	case EV_USR_RESUME:
		{
			if (eth_ipa_ctx.ipa_offload_susp) {
				if (!ethqos_ipa_offload_resume(
					eth_ipa_ctx.ethqos))
					eth_ipa_ctx.ipa_offload_susp
					= false;
			}
		}
		break;
	case EV_IPA_OFFLOAD_REMOVE:
		{
		   ethqos_rx_buf_free_mem(eth_ipa_ctx.ethqos, IPA_DMA_RX_CH);
		   ethqos_tx_buf_free_mem(eth_ipa_ctx.ethqos, IPA_DMA_TX_CH);
		   ethqos_rx_desc_free_mem(eth_ipa_ctx.ethqos, IPA_DMA_RX_CH);
		   ethqos_tx_desc_free_mem(eth_ipa_ctx.ethqos, IPA_DMA_TX_CH);
		   ethqos_free_ipa_rx_queue_struct(eth_ipa_ctx.ethqos);
		   ethqos_free_ipa_tx_queue_struct(eth_ipa_ctx.ethqos);
		}
		break;
	case EV_INVALID:
	default:
		{
		}
		break;
	}

	ETHQOSDBG("Exit: event=%d\n", ev);
	IPA_UNLOCK();
}

