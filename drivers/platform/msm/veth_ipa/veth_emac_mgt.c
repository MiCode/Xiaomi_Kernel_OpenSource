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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/habmm.h>
#include <linux/habmmid.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/dma-mapping.h>
#include <linux/memory.h>
#include <linux/platform_device.h>
#include "veth_emac_mgt.h"

uint32_t veth_hab_pdata_size = 32;

int veth_alloc_emac_export_mem(
	struct veth_emac_export_mem *veth_emac_mem, struct veth_ipa_dev *pdata)
{
	phys_addr_t  tx_desc_mem_paddr;
	phys_addr_t  rx_desc_mem_paddr;
	phys_addr_t  tx_buf_mem_paddr;
	phys_addr_t  rx_buf_mem_paddr;
	phys_addr_t  tx_buf_pool_paddr;
	phys_addr_t  rx_buf_pool_paddr;

	int i = 0;

	VETH_IPA_DEBUG("%s - begin\n", __func__);

	/* Allocate TX DESC */
	veth_emac_mem->tx_desc_mem_va = dma_alloc_coherent(&pdata->pdev->dev,
		sizeof(struct s_TX_NORMAL_DESC) * VETH_TX_DESC_CNT,
		&tx_desc_mem_paddr,
		GFP_KERNEL | GFP_DMA);

	if (!veth_emac_mem->tx_desc_mem_va) {
		VETH_IPA_DEBUG("%s: No memory\n", __func__);
		return -ENOMEM;
	}

	veth_emac_mem->tx_desc_mem_paddr = tx_desc_mem_paddr;

	VETH_IPA_DEBUG("%s: TX desc base mem allocated %p\n",
		__func__, veth_emac_mem->tx_desc_mem_va);
	VETH_IPA_DEBUG("%s: physical addr: tx desc mem 0x%x\n",
		__func__, veth_emac_mem->tx_desc_mem_paddr);

	for (i = 0; i < VETH_TX_DESC_CNT; i++) {
		veth_emac_mem->tx_desc_ring_base[i] =
			veth_emac_mem->tx_desc_mem_paddr +
			(i * sizeof(struct s_TX_NORMAL_DESC));
		VETH_IPA_DEBUG(
			"%s: veth_emac_mem->tx_desc_ring_base [i] 0x%x\n",
			__func__, i, veth_emac_mem->tx_desc_ring_base[i]);
	}

	/*Allocate RX DESC */
	veth_emac_mem->rx_desc_mem_va = dma_alloc_coherent(&pdata->pdev->dev,
		sizeof(struct s_RX_NORMAL_DESC) * VETH_RX_DESC_CNT,
		&rx_desc_mem_paddr,
		GFP_KERNEL | GFP_DMA);

	if (!veth_emac_mem->rx_desc_mem_va) {
		VETH_IPA_DEBUG("%s: No memory\n", __func__);
		goto free_tx_desc_mem_va;
	}

	VETH_IPA_DEBUG("%s: RX desc mem allocated %p\n",
		__func__, veth_emac_mem->rx_desc_mem_va);
	veth_emac_mem->rx_desc_mem_paddr = rx_desc_mem_paddr;

	for (i = 0; i < VETH_RX_DESC_CNT; i++) {
		veth_emac_mem->rx_desc_ring_base[i] =
			veth_emac_mem->rx_desc_mem_paddr +
			(i * sizeof(struct s_RX_NORMAL_DESC));
		VETH_IPA_DEBUG(
			"%s: veth_emac_mem->rx_desc_ring_base [i] 0x%x\n",
			__func__, i, veth_emac_mem->rx_desc_ring_base[i]);
	}

	VETH_IPA_DEBUG("%s: physical addr: rx desc mem 0x%x\n",
		__func__, veth_emac_mem->rx_desc_mem_paddr);

	/*Allocate TX Buffers*/
	veth_emac_mem->tx_buf_mem_va = dma_alloc_coherent(&pdata->pdev->dev,
		VETH_ETH_FRAME_LEN_IPA * VETH_TX_DESC_CNT,
		&tx_buf_mem_paddr,
		GFP_KERNEL | GFP_DMA);

	if (!veth_emac_mem->tx_buf_mem_va) {
		VETH_IPA_DEBUG("%s: No memory\n", __func__);
		goto free_rx_desc_mem_va;
	}

	veth_emac_mem->tx_buf_mem_paddr = tx_buf_mem_paddr;

	VETH_IPA_DEBUG("%s: TX buf mem allocated %p\n",
		__func__, veth_emac_mem->tx_buf_mem_va);
	VETH_IPA_DEBUG("%s: physical addr: tx buf mem 0x%x\n",
		__func__, veth_emac_mem->tx_buf_mem_paddr);

	veth_emac_mem->tx_buff_pool_base =
		(uint32_t *)dma_zalloc_coherent(&pdata->pdev->dev,
			sizeof(uint32_t) * VETH_TX_DESC_CNT,
			&tx_buf_pool_paddr,
			GFP_KERNEL);

	if (!veth_emac_mem->tx_buff_pool_base) {
		VETH_IPA_DEBUG("%s: No memory for rx_buf_mem_va\n", __func__);
		goto free_tx_buf_mem_va;
	}

	veth_emac_mem->tx_buff_pool_base[0] = veth_emac_mem->tx_buf_mem_paddr;

	for (i = 0; i < VETH_TX_DESC_CNT; i++) {
		veth_emac_mem->tx_buff_pool_base[i] =
			veth_emac_mem->tx_buff_pool_base[0] +
			i*VETH_ETH_FRAME_LEN_IPA;
		VETH_IPA_DEBUG(
			"%s: veth_emac_mem->tx_buff_pool_base[%d] 0x%x\n",
			__func__, i, veth_emac_mem->tx_buff_pool_base[i]);
	}

	veth_emac_mem->tx_buff_pool_base_pa = tx_buf_pool_paddr;

	/* Allocate RX buffers*/
	veth_emac_mem->rx_buf_mem_va = dma_alloc_coherent(&pdata->pdev->dev,
		VETH_ETH_FRAME_LEN_IPA * VETH_RX_DESC_CNT,
		&rx_buf_mem_paddr,
		GFP_KERNEL | GFP_DMA);


	if (!veth_emac_mem->rx_buf_mem_va) {
		VETH_IPA_DEBUG("%s: No memory for rx_buf_mem_va\n", __func__);
		goto free_tx_buff_pool_base;
	}

	veth_emac_mem->rx_buf_mem_paddr = rx_buf_mem_paddr;

	VETH_IPA_DEBUG("%s: RX buf mem allocated %p\n",
		__func__, veth_emac_mem->rx_buf_mem_va);
	VETH_IPA_DEBUG("%s: physical addr: rx_buf_mem_addr 0x%x\n",
		__func__, veth_emac_mem->rx_buf_mem_paddr);

	veth_emac_mem->rx_buff_pool_base =
		(uint32_t *)dma_zalloc_coherent(&pdata->pdev->dev,
			sizeof(uint32_t) * VETH_RX_DESC_CNT,
			&rx_buf_pool_paddr,
			GFP_KERNEL);

	if (!veth_emac_mem->rx_buff_pool_base) {
		VETH_IPA_DEBUG("%s: No memory for rx_buf_mem_va\n", __func__);
		goto free_rx_buf_mem_va;
	}

	veth_emac_mem->rx_buff_pool_base[0] = veth_emac_mem->rx_buf_mem_paddr;

	for (i = 0; i < VETH_RX_DESC_CNT; i++) {
		veth_emac_mem->rx_buff_pool_base[i] =
			veth_emac_mem->rx_buff_pool_base[0] +
			i*VETH_ETH_FRAME_LEN_IPA;
		VETH_IPA_DEBUG(
			"%s: veth_emac_mem->rx_buff_pool_base[%d] 0x%x\n",
			__func__, i, veth_emac_mem->rx_buff_pool_base[i]);
	}

	veth_emac_mem->rx_buff_pool_base_pa = rx_buf_pool_paddr;

	return 0;

free_rx_buf_mem_va:
	dma_free_coherent(&pdata->pdev->dev,
		VETH_ETH_FRAME_LEN_IPA * VETH_RX_DESC_CNT,
		veth_emac_mem->rx_buf_mem_va,
		rx_buf_mem_paddr);
free_tx_buff_pool_base:
	dma_free_coherent(&pdata->pdev->dev,
		sizeof(uint32_t) * VETH_TX_DESC_CNT,
		veth_emac_mem->tx_buff_pool_base,
		tx_buf_pool_paddr);
free_tx_buf_mem_va:
	dma_free_coherent(&pdata->pdev->dev,
		VETH_ETH_FRAME_LEN_IPA * VETH_TX_DESC_CNT,
		veth_emac_mem->tx_buf_mem_va,
		tx_buf_mem_paddr);
free_rx_desc_mem_va:
	dma_free_coherent(&pdata->pdev->dev,
		sizeof(struct s_RX_NORMAL_DESC) * VETH_RX_DESC_CNT,
		veth_emac_mem->rx_desc_mem_va,
		rx_desc_mem_paddr);
free_tx_desc_mem_va:
	dma_free_coherent(&pdata->pdev->dev,
		sizeof(struct s_TX_NORMAL_DESC) * VETH_TX_DESC_CNT,
		veth_emac_mem->tx_desc_mem_va,
		tx_desc_mem_paddr);
	return -ENOMEM;
}

int emac_ipa_hab_init(int mmid)
{
	uint32_t timeout_ms = 100;
	int ret = 0;
	int vc_id = 0;
	char *pdata_send;
	char *pdata_recv;

	pdata_send = kmalloc(veth_hab_pdata_size, GFP_KERNEL);
	if (!pdata_send) {
		VETH_IPA_ERROR("%s: failed to alloc pdata_send\n", __func__);
		return -ENOMEM;
	}

	pdata_recv = kmalloc(veth_hab_pdata_size, GFP_KERNEL);
	if (!pdata_recv) {
		kfree(pdata_send);
		VETH_IPA_ERROR("%s: failed to alloc pdata_recv\n", __func__);
		return -ENOMEM;
	}

	VETH_IPA_DEBUG("%s: Opening hab socket\n", __func__);
	ret = habmm_socket_open(&vc_id, mmid, timeout_ms, 0);

	if (ret) {
		VETH_IPA_ERROR("%s: hab open failed %d returned\n",
			__func__, ret);
		ret = -1;
		goto err;
	}
	VETH_IPA_DEBUG("%s: vc_id = %d\n", __func__, vc_id);

	/*Send the INIT*/
	memset(pdata_send, 1, veth_hab_pdata_size);
	VETH_IPA_DEBUG("%s: Sending INIT\n", __func__);
	ret = habmm_socket_send(vc_id, pdata_send, veth_hab_pdata_size, 0);

	if (ret) {
		VETH_IPA_ERROR("%s: Send failed %d returned\n", __func__, ret);
		ret = -1;
		goto err;
	}

	/*Receive ACK*/
	memset(pdata_recv, 1, veth_hab_pdata_size);
	VETH_IPA_DEBUG("%s: Receiving ACK\n", __func__);
	ret = habmm_socket_recv(vc_id, pdata_recv, &veth_hab_pdata_size, 0, 0);

	if (ret) {
		VETH_IPA_ERROR("%s: receive failed! ret %d, recv size %d\n",
			__func__, ret, veth_hab_pdata_size);
		ret = -1;
		goto err;
	}

	/*Send INITACK*/
	memset(pdata_send, 1, veth_hab_pdata_size);
	VETH_IPA_DEBUG("%s: INIT ACK\n", __func__);
	ret = habmm_socket_send(vc_id, pdata_send, veth_hab_pdata_size, 0);

	if (ret) {
		VETH_IPA_ERROR("%s: Send failed failed %d returned\n",
			__func__, ret);
		ret = -1;
		goto err;
	}

	return vc_id;
err:
	kfree(pdata_send);
	kfree(pdata_recv);
	if (vc_id > 0)
		habmm_socket_close(vc_id);
	return ret;
}

int emac_ipa_hab_export_tx_desc(
	int vc_id, struct veth_emac_export_mem *veth_emac_mem)
{
	uint32_t exp_id = 0;
	int ret = 0;
	char *pdata_send;
	char *pdata_recv;

	pdata_send = kmalloc(veth_hab_pdata_size, GFP_KERNEL);
	if (!pdata_send) {
		VETH_IPA_ERROR("%s: failed to alloc pdata_send\n", __func__);
		return -ENOMEM;
	}

	pdata_recv = kmalloc(veth_hab_pdata_size, GFP_KERNEL);
	if (!pdata_recv) {
		kfree(pdata_send);
		VETH_IPA_ERROR("%s: failed to alloc pdata_recv\n", __func__);
		return -ENOMEM;
	}

	/*export the memory*/
	VETH_IPA_DEBUG("%s: Export TX desc memory TO VC_ID %d\n",
		__func__, vc_id);
	VETH_IPA_DEBUG("%s: veth_emac_mem->tx_desc_mem_va  %p\n",
		__func__, veth_emac_mem->tx_desc_mem_va);
	VETH_IPA_DEBUG("%s: size  %d\n",
		__func__, sizeof(struct s_TX_NORMAL_DESC) * VETH_TX_DESC_CNT);
	ret = habmm_export(vc_id,
		veth_emac_mem->tx_desc_mem_va,
		sizeof(struct s_TX_NORMAL_DESC) * VETH_TX_DESC_CNT,
		&exp_id, 0);

	if (ret) {
		VETH_IPA_ERROR("%s: Export failed %d returned, export id %d\n",
			__func__, ret, exp_id);
		ret = -1;
		goto err;
	}

	VETH_IPA_DEBUG("%s: Export memory location %p\n",
		__func__, veth_emac_mem->tx_desc_mem_va);

	/*Send the export ID*/
	VETH_IPA_DEBUG("%s: Send TX desc export memory %d\n",
		__func__, exp_id);
	ret = habmm_socket_send(vc_id, &exp_id, veth_hab_pdata_size, 0);

	if (ret) {
		VETH_IPA_ERROR("%s: Send failed failed %d returned\n",
			__func__, ret);
		ret = -1;
		goto err;
	}

	/*Receive ACK*/
	memset(pdata_recv, 0, veth_hab_pdata_size);
	ret = habmm_socket_recv(vc_id, pdata_recv, &veth_hab_pdata_size, 0, 0);

	if (ret) {
		VETH_IPA_ERROR("%s: Receive failed failed %d returned\n",
			__func__, ret);
		ret = -1;
		goto err;
	}

	kfree(pdata_send);
	kfree(pdata_recv);
	return ret;
err:
	kfree(pdata_send);
	kfree(pdata_recv);
	kfree(veth_emac_mem->tx_desc_mem_va);
	kfree(veth_emac_mem->rx_desc_mem_va);
	kfree(veth_emac_mem->tx_buf_mem_va);
	kfree(veth_emac_mem->rx_buf_mem_va);
	return ret;
}

int emac_ipa_hab_export_rx_desc(
	int vc_id, struct veth_emac_export_mem *veth_emac_mem)
{
	uint32_t exp_id = 0;
	int ret = 0;
	char *pdata_send;
	char *pdata_recv;

	pdata_send = kmalloc(veth_hab_pdata_size, GFP_KERNEL);
	if (!pdata_send) {
		VETH_IPA_ERROR("%s: failed to alloc pdata_send\n", __func__);
		return -ENOMEM;
	}

	pdata_recv = kmalloc(veth_hab_pdata_size, GFP_KERNEL);
	if (!pdata_recv) {
		kfree(pdata_send);
		VETH_IPA_ERROR("%s: failed to alloc pdata_recv\n", __func__);
		return -ENOMEM;
	}

	VETH_IPA_DEBUG("%s: Export RX desc memory TO VC_ID %d\n",
		__func__, vc_id);
	ret = habmm_export(vc_id,
		veth_emac_mem->rx_desc_mem_va,
		sizeof(struct s_RX_NORMAL_DESC) * VETH_RX_DESC_CNT,
		&exp_id,
		0);

	if (ret) {
		VETH_IPA_ERROR("%s: Export failed %d returned, export id %d\n",
			__func__, ret, exp_id);
		ret = -1;
		goto err;
	}

	VETH_IPA_DEBUG("%s: Export RX desc memory location %p\n",
		__func__, veth_emac_mem->rx_desc_mem_va);

	/*Send the export ID*/
	VETH_IPA_DEBUG("%s: Export RX desc memory %d\n", __func__, exp_id);
	ret = habmm_socket_send(vc_id, &exp_id, veth_hab_pdata_size, 0);

	if (ret) {
		VETH_IPA_ERROR("%s: Send failed failed %d returned\n",
			__func__, ret);
		ret = -1;
		goto err;
	}

	ret = habmm_socket_recv(vc_id, pdata_recv, &veth_hab_pdata_size, 0, 0);

	if (ret) {
		VETH_IPA_ERROR("%s: Receive failed failed %d returned\n",
			__func__, ret);
		ret = -1;
		goto err;
	}

	kfree(pdata_send);
	kfree(pdata_recv);
	return ret;

err:
	kfree(pdata_send);
	kfree(pdata_recv);
	kfree(veth_emac_mem->tx_desc_mem_va);
	kfree(veth_emac_mem->rx_desc_mem_va);
	kfree(veth_emac_mem->tx_buf_mem_va);
	kfree(veth_emac_mem->rx_buf_mem_va);
	return ret;
}

int emac_ipa_hab_export_tx_buf(
	int vc_id, struct veth_emac_export_mem *veth_emac_mem)
{
	uint32_t exp_id = 0;
	int ret = 0;
	char *pdata_send;
	char *pdata_recv;

	pdata_send = kmalloc(veth_hab_pdata_size, GFP_KERNEL);
	if (!pdata_send) {
		VETH_IPA_ERROR("%s: failed to alloc pdata_send\n", __func__);
		return -ENOMEM;
	}

	pdata_recv = kmalloc(veth_hab_pdata_size, GFP_KERNEL);
	if (!pdata_recv) {
		kfree(pdata_send);
		VETH_IPA_ERROR("%s: failed to alloc pdata_recv\n", __func__);
		return -ENOMEM;
	}

	VETH_IPA_DEBUG("%s: Export TX buf memory TO VC_ID %d\n",
		__func__, vc_id);
	ret = habmm_export(
		vc_id,
		veth_emac_mem->tx_buf_mem_va,
		VETH_ETH_FRAME_LEN_IPA * VETH_RX_DESC_CNT,
		&exp_id,
		0);

	if (ret) {
		VETH_IPA_ERROR("%s: Export failed %d returned, export id %d\n",
			__func__, ret, exp_id);
		ret = -1;
		goto err;
	}

	VETH_IPA_DEBUG("%s: Export TX buf memory location %p\n",
		__func__, veth_emac_mem->tx_buf_mem_va);

	/*Send the export ID*/
	VETH_IPA_DEBUG("%s: Send TX buf memory %d\n", __func__, exp_id);
	ret = habmm_socket_send(vc_id, &exp_id, veth_hab_pdata_size, 0);

	if (ret) {
		VETH_IPA_ERROR("%s: Send failed failed %d returned\n",
			__func__, ret);
		ret = -1;
		goto err;
	}

	ret = habmm_socket_recv(vc_id, pdata_recv, &veth_hab_pdata_size, 0, 0);

	if (ret) {
		VETH_IPA_ERROR("%s: Receive  failed failed %d returned\n",
			__func__, ret);
		ret = -1;
		goto err;
	}

	kfree(pdata_send);
	kfree(pdata_recv);
	return ret;

err:
	kfree(pdata_send);
	kfree(pdata_recv);
	kfree(veth_emac_mem->tx_desc_mem_va);
	kfree(veth_emac_mem->rx_desc_mem_va);
	kfree(veth_emac_mem->tx_buf_mem_va);
	kfree(veth_emac_mem->rx_buf_mem_va);
	return ret;
}


int emac_ipa_hab_export_rx_buf(
	int vc_id, struct veth_emac_export_mem *veth_emac_mem)
{
	int ret = 0;
	char *pdata_recv;
	uint32_t exp_id = 0;

	VETH_IPA_DEBUG("%s: Export RX buf memory TO VC_ID %d\n",
		__func__, vc_id);
	ret = habmm_export(vc_id,
		veth_emac_mem->rx_buf_mem_va,
		VETH_ETH_FRAME_LEN_IPA * VETH_RX_DESC_CNT,
		&exp_id,
		0);

	if (ret) {
		VETH_IPA_ERROR("%s: Export failed %d returned, export id %d\n",
			__func__, ret, exp_id);
		ret = -1
		;
		goto err;
	}

	VETH_IPA_DEBUG("%s: Export RX buf memory location %p\n",
		__func__, veth_emac_mem->rx_buf_mem_va);

	/*Send the export ID*/
	VETH_IPA_DEBUG("%s: Send TX buf memory %d\n", __func__, exp_id);
	ret = habmm_socket_send(vc_id, &exp_id, veth_hab_pdata_size, 0);

	if (ret) {
		VETH_IPA_ERROR("%s: Send failed failed %d returned\n",
			__func__, ret);
		ret = -1;
		goto err;
	}

	ret = habmm_socket_recv(vc_id, pdata_recv, &veth_hab_pdata_size, 0, 0);

	if (ret) {
		VETH_IPA_ERROR("%s: Receive  failed failed %d returned\n",
			__func__, ret);
		ret = -1;
		goto err;
	}

	return ret;

err:
	kfree(veth_emac_mem->tx_desc_mem_va);
	kfree(veth_emac_mem->rx_desc_mem_va);
	kfree(veth_emac_mem->tx_buf_mem_va);
	kfree(veth_emac_mem->rx_buf_mem_va);
	return ret;
}

int veth_emac_init(struct veth_emac_export_mem *veth_emac_mem)
{
	int ret = 0;
	int vc_id = 0;
	unsigned int mmid = HAB_MMID_CREATE(MM_MISC, MM_MISC_VM_HAB_MINOR_ID);

	/*Open the FD*/
	VETH_IPA_INFO("%s: %d\n", __func__, mmid);

	vc_id = emac_ipa_hab_init(mmid);
	if (vc_id < 0) {
		VETH_IPA_ERROR("%s: HAB init failed, returning error\n",
			__func__);
		return -EAGAIN;
	}

	VETH_IPA_DEBUG("%s: emac_ipa vc_id = %d\n", __func__, vc_id);
	VETH_IPA_DEBUG("%s: VETH_EMAC_MEM ptr = %p\n",
		__func__, veth_emac_mem);
	ret = emac_ipa_hab_export_tx_desc(vc_id, veth_emac_mem);

	if (ret < 0) {
		VETH_IPA_ERROR(
			"HAB export of TX desc mem failed, returning error");
		return -EAGAIN;
	}

	ret = emac_ipa_hab_export_rx_desc(vc_id, veth_emac_mem);

	if (ret < 0) {
		VETH_IPA_ERROR(
			"HAB export of RX desc mem failed, returning error");
		return -EAGAIN;
	}

	ret = emac_ipa_hab_export_tx_buf(vc_id, veth_emac_mem);

	if (ret < 0) {
		VETH_IPA_ERROR(
			"HAB export of TX buf mem failed, returning error");
		return -EAGAIN;
	}

	ret = emac_ipa_hab_export_rx_buf(vc_id, veth_emac_mem);

	if (ret < 0) {
		VETH_IPA_ERROR(
			"HAB export of RX buf mem failed, returning error");
		return -EAGAIN;
	}

	VETH_IPA_INFO("%s: Memory export complete\n", __func__);
	return 0;
}
