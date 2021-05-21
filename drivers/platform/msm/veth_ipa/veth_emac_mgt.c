// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/etherdevice.h>
#include <linux/if_vlan.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/sched.h>
#include <linux/atomic.h>
#include <linux/of_device.h>
#include <linux/ipa_uc_offload.h>
#include <linux/if_ether.h>
#include <linux/msm_ipa.h>
#include <linux/habmm.h>
#include <linux/habmmid.h>
#include "veth_emac_mgt.h"
#include "veth_ipa.h"

#define NO_FLAGS 0x0

MODULE_LICENSE("GPL v2");

/** veth_alloc_emac_export_mem() - Called when allocating the
 *  exported memory to the BE driver.
 *  @veth_emac_mem: Consists of all the export memory and
 *  physical addresses of the exported memory
 *  @pdata : Consists of other net device information
 * This API is mainly used to allocate the required memory
 * for the BE driver on the QNX host side
 */
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

	VETH_IPA_DEBUG("%s - begin %p, %p\n", __func__, pdata, veth_emac_mem);
	/* Allocate TX DESC */
	veth_emac_mem->tx_desc_mem_va = dma_alloc_coherent(&pdata->pdev->dev,
		sizeof(struct s_TX_NORMAL_DESC) * VETH_TX_DESC_CNT,
		&tx_desc_mem_paddr,
		GFP_KERNEL | GFP_DMA);

	if (!veth_emac_mem->tx_desc_mem_va) {
		VETH_IPA_DEBUG("%s: No memory\n", __func__);
		goto free_tx_desc_mem_va;
		//return -ENOMEM;
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
	//Allocate RX DESC
	veth_emac_mem->rx_desc_mem_va = dma_alloc_coherent(&pdata->pdev->dev,
		sizeof(struct s_RX_NORMAL_DESC) * VETH_RX_DESC_CNT,
		&rx_desc_mem_paddr,
		GFP_KERNEL | GFP_DMA);

	if (!veth_emac_mem->rx_desc_mem_va) {
		VETH_IPA_DEBUG("%s: No memory\n", __func__);
		goto free_rx_desc_mem_va;
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

	//Allocate TX Buffers
	veth_emac_mem->tx_buf_mem_va = dma_alloc_coherent(&pdata->pdev->dev,
		VETH_ETH_FRAME_LEN_IPA * VETH_TX_DESC_CNT,
		&tx_buf_mem_paddr,
		GFP_KERNEL | GFP_DMA);

	if (!veth_emac_mem->tx_buf_mem_va) {
		VETH_IPA_DEBUG("%s: No memory\n", __func__);
		goto free_tx_buf_mem_va;
	}

	veth_emac_mem->tx_buf_mem_paddr = tx_buf_mem_paddr;

	VETH_IPA_DEBUG("%s: TX buf mem allocated %p\n",
		__func__, veth_emac_mem->tx_buf_mem_va);
	VETH_IPA_DEBUG("%s: physical addr: tx buf mem 0x%x\n",
		__func__, veth_emac_mem->tx_buf_mem_paddr);

	/*transport minimum 4k*/
	veth_emac_mem->tx_buff_pool_base_va = dma_alloc_coherent(&pdata->pdev->dev,
			sizeof(uint32_t) * (VETH_TX_DESC_CNT * 4),
			&tx_buf_pool_paddr,
			GFP_KERNEL | GFP_DMA);

	if (!veth_emac_mem->tx_buff_pool_base_va) {
		VETH_IPA_DEBUG("%s: No memory for rx_buf_mem_va\n", __func__);
		goto free_tx_buff_pool_base;
	}

	veth_emac_mem->tx_buff_pool_base_pa = tx_buf_pool_paddr;

	//Allocate RX buffers
	veth_emac_mem->rx_buf_mem_va = dma_alloc_coherent(&pdata->pdev->dev,
		VETH_ETH_FRAME_LEN_IPA * VETH_RX_DESC_CNT,
		&rx_buf_mem_paddr,
		GFP_KERNEL | GFP_DMA);


	if (!veth_emac_mem->rx_buf_mem_va) {
		VETH_IPA_DEBUG("%s: No memory for rx_buf_mem_va\n", __func__);
		goto free_rx_buf_mem_va;
	}

	veth_emac_mem->rx_buf_mem_paddr = rx_buf_mem_paddr;

	VETH_IPA_DEBUG("%s: RX buf mem allocated %p\n",
		__func__, veth_emac_mem->rx_buf_mem_va);
	VETH_IPA_DEBUG("%s: physical addr: rx_buf_mem_addr 0x%x\n",
		__func__, veth_emac_mem->rx_buf_mem_paddr);

	veth_emac_mem->rx_buff_pool_base_va = dma_alloc_coherent(&pdata->pdev->dev,
			sizeof(uint32_t) * VETH_RX_DESC_CNT*4,
			&rx_buf_pool_paddr,
			GFP_KERNEL | GFP_DMA | __GFP_ZERO);

	if (!veth_emac_mem->rx_buff_pool_base_va) {
		VETH_IPA_DEBUG("%s: No memory for rx_buf_mem_va\n", __func__);
		goto free_rx_buff_pool_base;
	}
	veth_emac_mem->rx_buff_pool_base_pa = rx_buf_pool_paddr;
	return 0;



free_rx_buff_pool_base:
	dma_free_coherent(&pdata->pdev->dev,
		VETH_ETH_FRAME_LEN_IPA * VETH_RX_DESC_CNT,
		veth_emac_mem->rx_buff_pool_base_va,
		rx_buf_pool_paddr);
free_rx_buf_mem_va:
	dma_free_coherent(&pdata->pdev->dev,
		VETH_ETH_FRAME_LEN_IPA * VETH_RX_DESC_CNT,
		veth_emac_mem->rx_buf_mem_va,
		rx_buf_mem_paddr);
free_tx_buff_pool_base:
	dma_free_coherent(&pdata->pdev->dev,
		sizeof(uint32_t) * VETH_TX_DESC_CNT,
		veth_emac_mem->tx_buff_pool_base_va,
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

/** veth_dealloc_emac_export_mem() - Called when deallocating
 *  the exported memory to the BE driver.
 *  @veth_emac_mem: Consists of all the export memory and
 *  physical addresses of the exported memory
 *  @pdata : Consists of other net device information
 *
 * This API is mainly used to free the memory from DDR space
 * during DOWN, CARRIER_DOWN, LPM or unmount event
 */
int veth_alloc_emac_dealloc_mem(
	struct veth_emac_export_mem *veth_emac_mem, struct veth_ipa_dev *pdata)
{
	/*1. Send stop offload to the BE
	 *2. Receive from BE
	 *4. Free the memory
	 *5. Close the HAB socket ?
	 */

	if (veth_emac_mem->rx_buf_mem_va) {
		VETH_IPA_DEBUG("%s: Freeing RX buf mem", __func__);
		dma_free_coherent(&pdata->pdev->dev,
			VETH_ETH_FRAME_LEN_IPA * VETH_RX_DESC_CNT,
			veth_emac_mem->rx_buf_mem_va,
			veth_emac_mem->rx_buf_mem_paddr);
	} else {
		VETH_IPA_ERROR("%s: RX buf not available", __func__);
	}

	if (veth_emac_mem->tx_buf_mem_va) {
		VETH_IPA_DEBUG("%s: Freeing TX buf mem", __func__);
		dma_free_coherent(&pdata->pdev->dev,
			VETH_ETH_FRAME_LEN_IPA * VETH_TX_DESC_CNT,
			veth_emac_mem->tx_buf_mem_va,
			veth_emac_mem->tx_buf_mem_paddr);
	} else {
		VETH_IPA_ERROR("%s: TX buf not available", __func__);
	}

	if (veth_emac_mem->rx_desc_mem_va) {
		VETH_IPA_DEBUG("%s: Freeing RX desc mem", __func__);
		dma_free_coherent(&pdata->pdev->dev,
			sizeof(struct s_TX_NORMAL_DESC) * VETH_TX_DESC_CNT,
			veth_emac_mem->rx_desc_mem_va,
			veth_emac_mem->rx_desc_mem_paddr);
	} else {
		VETH_IPA_ERROR("%s: RX desc mem not available", __func__);
	}

	if (veth_emac_mem->tx_desc_mem_va) {
		VETH_IPA_DEBUG("%s: Freeing TX desc mem", __func__);
		dma_free_coherent(&pdata->pdev->dev,
			sizeof(struct s_RX_NORMAL_DESC) * VETH_RX_DESC_CNT,
			veth_emac_mem->tx_desc_mem_va,
			veth_emac_mem->tx_desc_mem_paddr);
	} else {
		VETH_IPA_ERROR("%s: TX desc mem not available", __func__);
	}

	if (veth_emac_mem->rx_buff_pool_base_va) {
		VETH_IPA_DEBUG("%s: Freeing RX buff pool mem", __func__);
		dma_free_coherent(&pdata->pdev->dev,
			sizeof(uint32_t) * VETH_RX_DESC_CNT,
			veth_emac_mem->rx_buff_pool_base_va,
			veth_emac_mem->rx_buff_pool_base_pa);
	} else {
		VETH_IPA_ERROR("%s: RX buff pool base not available", __func__);
	}

	if (veth_emac_mem->tx_buff_pool_base_va) {
		VETH_IPA_DEBUG("%s: Freeing TX buff pool mem", __func__);
		dma_free_coherent(&pdata->pdev->dev,
			sizeof(uint32_t) * VETH_TX_DESC_CNT,
			veth_emac_mem->tx_buff_pool_base_va,
			veth_emac_mem->tx_buff_pool_base_pa);
	} else {
		VETH_IPA_ERROR("%s: TX buff pool base not available", __func__);
	}
	return 0;
}



/** veth_emac_ipa_hab_init() - This is called in the initial
 *  HAB init state where the BE and FE driver establish HAB
 *  communixation
 *  @mmid: This provides the physical channel used for HAB
 *  communication
 *
 */
int veth_emac_ipa_hab_init(int mmid)
{
	uint32_t timeout_ms = 0;
	int ret = 0;
	int vc_id = 0;
	char *pdata_send;
	uint32_t *pdata_recv;
	uint32_t veth_hab_pdata_size = 32;

	VETH_IPA_INFO("%s: Enter HAB init\n", __func__);
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

	VETH_IPA_INFO("%s: Opening hab socket\n", __func__);
	ret = habmm_socket_open(&vc_id, mmid, timeout_ms, 0);

	if (ret) {
		VETH_IPA_ERROR("%s: hab open failed %d returned\n",
			__func__, ret);
		ret = -1;
		goto err;
	}
	VETH_IPA_INFO("%s: vc_id = %d\n", __func__, vc_id);

	/*Send the INIT*/
	memset(pdata_send, 1, veth_hab_pdata_size);
	VETH_IPA_INFO("%s: Sending INIT\n", __func__);
	ret = habmm_socket_send(vc_id, pdata_send, veth_hab_pdata_size, 0);

	if (ret) {
		VETH_IPA_ERROR("%s: Send failed %d returned\n", __func__, ret);
		ret = -1;
		goto err;
	}

	/*Receive ACK*/
	memset(pdata_recv, 1, veth_hab_pdata_size);
	VETH_IPA_INFO("%s: Receiving ACK\n", __func__);
	ret = habmm_socket_recv(vc_id, &pdata_recv, &veth_hab_pdata_size, 0, 0);

	if (ret) {
		VETH_IPA_ERROR("%s: receive failed! ret %d, recv size %d\n",
			__func__, ret, veth_hab_pdata_size);
		ret = -1;
		goto err;
	}

	/*Send INITACK*/
	memset(pdata_send, 1, veth_hab_pdata_size);
	VETH_IPA_INFO("%s: INIT ACK\n", __func__);
	ret = habmm_socket_send(vc_id, pdata_send, veth_hab_pdata_size, 0);

	if (ret) {
		VETH_IPA_ERROR("%s: Send failed %d returned\n",
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


/** veth_emac_ipa_hab_export_tx_desc() - This API is called
 *  for exporting the TX desc memory to BE driver in QNX host
 *  @vcid: The virtual channel ID between BE and FE driver
 *
 *  @veth_emac_mem - Contains the virtual and physical addresses
 *  of the exported memory
 */
static int veth_emac_ipa_hab_export_tx_desc(
	int vc_id, struct veth_emac_export_mem *veth_emac_mem,
	struct veth_ipa_dev *pdata)
{
	int ret = 0;
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
			&veth_emac_mem->exp_id.tx_desc_exp_id, 0);

	if (ret) {
		VETH_IPA_ERROR("%s: Export failed %d returned, export id %d\n",
			__func__, ret, veth_emac_mem->exp_id.tx_desc_exp_id);
		ret = -1;
		goto err;
	}

	VETH_IPA_DEBUG("%s: Export memory location %p\n",
		__func__, veth_emac_mem->tx_desc_mem_va);

	return ret;
err:
	veth_alloc_emac_dealloc_mem(veth_emac_mem, pdata);
	return ret;
}

/** veth_emac_ipa_hab_export_rx_desc() - This API is called for
 *  exporting the RX desc memory to BE driver in QNX host
 *  @vcid: The virtual channel ID between BE and FE driver
 *
 *  @veth_emac_mem - Contains the virtual and physical addresses
 *  of the exported memory
 */
static int veth_emac_ipa_hab_export_rx_desc(
	int vc_id, struct veth_emac_export_mem *veth_emac_mem,
	struct veth_ipa_dev *pdata)
{
	int ret = 0;

	VETH_IPA_DEBUG("%s: Export RX desc memory TO VC_ID %d\n",
		__func__, vc_id);
	ret = habmm_export(vc_id,
			   veth_emac_mem->rx_desc_mem_va,
			   sizeof(struct s_RX_NORMAL_DESC) * VETH_RX_DESC_CNT,
			   &veth_emac_mem->exp_id.rx_desc_exp_id,
			   0);

	if (ret) {
		VETH_IPA_ERROR("%s: Export failed %d returned, export id %d\n",
			__func__, ret, veth_emac_mem->exp_id.rx_desc_exp_id);
		ret = -1;
		goto err;
	}

	VETH_IPA_DEBUG("%s: Export RX desc memory location %p\n",
		__func__, veth_emac_mem->rx_desc_mem_va);
	return ret;

err:
	veth_alloc_emac_dealloc_mem(veth_emac_mem, pdata);
	return ret;
}


/** veth_emac_ipa_hab_export_tx_buf() - This API is called for
 *  exporting the TX buf memory to BE driver in QNX host
 *  @vcid: The virtual channel ID between BE and FE driver
 *
 *  @veth_emac_mem - Contains the virtual and physical addresses
 *  of the exported memory
 */
static int veth_emac_ipa_hab_export_tx_buf(
	int vc_id, struct veth_emac_export_mem *veth_emac_mem,
	struct veth_ipa_dev *pdata)
{
	int ret = 0;

	VETH_IPA_DEBUG("%s: Export TX buf memory TO VC_ID %d\n",
		__func__, vc_id);
	ret = habmm_export(vc_id,
			   veth_emac_mem->tx_buf_mem_va,
			   VETH_ETH_FRAME_LEN_IPA * VETH_RX_DESC_CNT,
			   &veth_emac_mem->exp_id.tx_buff_exp_id,
			   0);

	if (ret) {
		VETH_IPA_ERROR("%s: Export failed %d returned, export id %d\n",
			__func__, ret, veth_emac_mem->exp_id.tx_buff_exp_id);
		ret = -1;
		goto err;
	}

	VETH_IPA_DEBUG("%s: Export TX buf memory location %p\n",
		__func__, veth_emac_mem->tx_buf_mem_va);

	return ret;

err:
	veth_alloc_emac_dealloc_mem(veth_emac_mem, pdata);
	return ret;
}

/** veth_emac_ipa_hab_export_tx_desc() - This API is called
 *  for exporting the RX buf memory to BE driver in QNX host
 *  @vcid: The virtual channel ID between BE and FE driver
 *
 *  @veth_emac_mem - Contains the virtual and physical addresses
 *  of the exported memory
 */
static int veth_emac_ipa_hab_export_rx_buf(
	int vc_id, struct veth_emac_export_mem *veth_emac_mem,
	struct veth_ipa_dev *pdata)
{
	int ret = 0;

	VETH_IPA_DEBUG("%s: Export RX buf memory TO VC_ID %d\n",
		__func__, vc_id);
	ret = habmm_export(vc_id,
			   veth_emac_mem->rx_buf_mem_va,
			   VETH_ETH_FRAME_LEN_IPA * VETH_RX_DESC_CNT,
			   &veth_emac_mem->exp_id.rx_buff_exp_id,
			   0);

	if (ret) {
		VETH_IPA_ERROR("%s: Export failed %d returned, export id %d\n",
			__func__, ret, veth_emac_mem->exp_id.rx_buff_exp_id);
		ret = -1
		;
		goto err;
	}

	VETH_IPA_DEBUG("%s: Export RX buf memory location %p\n",
		__func__, veth_emac_mem->rx_buf_mem_va);
	return ret;

err:
	veth_alloc_emac_dealloc_mem(veth_emac_mem, pdata);
	return ret;
}



/** emac_ipa_hab_export_tx_buf_pool() - This API is called
 *  for exporting the TX buf pool memory to BE driver in QNX host
 *  @vcid: The virtual channel ID between BE and FE driver
 *
 *  @veth_emac_mem - Contains the virtual and physical addresses
 *  of the exported memory
 */
int emac_ipa_hab_export_tx_buf_pool(
	int vc_id, struct veth_emac_export_mem *veth_emac_mem,
	struct veth_ipa_dev *pdata)
{
	int ret = 0;

	VETH_IPA_DEBUG("%s: Export TX buf pool memory TO VC_ID %d\n",
		__func__, vc_id);

	ret = habmm_export(
		vc_id,
		veth_emac_mem->tx_buff_pool_base_va,
		sizeof(uint32_t) * VETH_TX_DESC_CNT * 4,
		&veth_emac_mem->exp_id.tx_buf_pool_exp_id,
		0);

	if (ret) {
		VETH_IPA_ERROR("%s: Export failed %d returned, export id %d\n",
			__func__,
			ret,
			veth_emac_mem->exp_id.tx_buf_pool_exp_id);
		ret = -1;
		goto err;
	}

	pr_info("%s: Export TX buf pool memory location %p %d\n",
		__func__,
		veth_emac_mem->tx_buff_pool_base_va,
		veth_emac_mem->exp_id.tx_buf_pool_exp_id);
	return ret;

err:
	veth_alloc_emac_dealloc_mem(veth_emac_mem, pdata);
	return ret;
}


/** emac_ipa_hab_export_tx_buf_pool() - This API is called
 *  for exporting the TX buf pool memory to BE driver in QNX host
 *  @vcid: The virtual channel ID between BE and FE driver
 *
 *  @veth_emac_mem - Contains the virtual and physical addresses
 *  of the exported memory
 */
int emac_ipa_hab_export_rx_buf_pool(
	int vc_id, struct veth_emac_export_mem *veth_emac_mem,
	struct veth_ipa_dev *pdata)
{
	int ret = 0;

	VETH_IPA_DEBUG("%s: Export RX buf pool memory TO VC_ID %d\n",
		__func__, vc_id);

	ret = habmm_export(
		vc_id,
		veth_emac_mem->rx_buff_pool_base_va,
		sizeof(uint32_t) * (VETH_RX_DESC_CNT * 4),
		&veth_emac_mem->exp_id.rx_buf_pool_exp_id,
		0);

	if (ret) {
		VETH_IPA_ERROR("%s: Export failed %d returned, export id %d\n",
			__func__,
			ret,
			veth_emac_mem->exp_id.rx_buf_pool_exp_id);
		ret = -1
		;
		goto err;
	}

	pr_info("%s: Export RX buf pool memory location %p , %d\n",
		__func__,
		veth_emac_mem->rx_buff_pool_base_va,
		veth_emac_mem->exp_id.rx_buf_pool_exp_id);
	return ret;

err:
	veth_alloc_emac_dealloc_mem(veth_emac_mem, pdata);
	return ret;
}




/** veth_emac_ipa_send_exp_id() - This API is used to send the
 *  export IDs of all the exported memory to the BE driver in
 *  QNX host
 *  @vcid: The virtual channel ID between BE and FE driver
 *
 *  @veth_emac_mem - Contains the virtual and physical addresses
 *  of the exported memory
 */
int veth_emac_ipa_send_exp_id(
	int vc_id, struct veth_emac_export_mem *veth_emac_mem)
{
	int ret = 0;

	ret = habmm_socket_send(vc_id,
			&veth_emac_mem->exp_id,
			sizeof(veth_emac_mem->exp_id),
			NO_FLAGS);
	VETH_IPA_INFO("Sent export ids to the backend driver");
	VETH_IPA_INFO("TX Descriptor export id sent %x",
			veth_emac_mem->exp_id.tx_desc_exp_id);
	if (ret) {
		VETH_IPA_ERROR("%s: Send failed %d returned\n",
		__func__,
		ret);
		ret = -1;
		return ret;
	}
return 0;
}

int veth_emac_init(struct veth_emac_export_mem *veth_emac_mem,
				   struct veth_ipa_dev *pdata, bool smmu_s2_enb)
{
	int ret = 0;

	ret = veth_emac_ipa_hab_export_tx_desc(veth_emac_mem->vc_id,
						veth_emac_mem,
						pdata);

	if (ret < 0) {
		VETH_IPA_ERROR(
			"HAB export of TX desc mem failed, returning error");
		return -ENOMEM;
	}

	ret = veth_emac_ipa_hab_export_rx_desc(veth_emac_mem->vc_id,
						veth_emac_mem,
						pdata);

	if (ret < 0) {
		VETH_IPA_ERROR(
			"HAB export of RX desc mem failed, returning error");
		return -ENOMEM;
	}

	ret = veth_emac_ipa_hab_export_tx_buf(veth_emac_mem->vc_id,
						veth_emac_mem,
						pdata);

	if (ret < 0) {
		VETH_IPA_ERROR(
			"HAB export of TX buf mem failed, returning error");
		return -ENOMEM;
	}

	ret = veth_emac_ipa_hab_export_rx_buf(veth_emac_mem->vc_id,
						veth_emac_mem,
						pdata);

	if (ret < 0) {
		VETH_IPA_ERROR(
			"HAB export of RX buf mem failed, returning error");
		return -ENOMEM;
	}

	ret = emac_ipa_hab_export_tx_buf_pool(veth_emac_mem->vc_id,
						veth_emac_mem,
						pdata);

	if (ret < 0) {
		VETH_IPA_ERROR(
			"HAB export of TX buff pool mem failed, returning error");
		return -ENOMEM;
	}

	ret = emac_ipa_hab_export_rx_buf_pool(veth_emac_mem->vc_id,
						veth_emac_mem,
						pdata);

	if (ret < 0) {
		VETH_IPA_ERROR(
			"HAB export of RX buff pool mem failed, returning error");
		return -ENOMEM;
	}

	ret = veth_emac_ipa_send_exp_id(veth_emac_mem->vc_id,
					veth_emac_mem);

	if (ret < 0) {
		VETH_IPA_ERROR(
			"Sending exp id failed, returning error");
		return -ENOMEM;
	}

	VETH_IPA_INFO("%s: Memory export complete\n", __func__);
	return 0;
}


/** veth_emac_ipa_setup_complete() - This API is used to send
 *  the SETUP complete event to the BE driver. It is triggered
 *  when the memory export and the IPA set up is complete. It
 *  informs the BE that the set up is complete at the FE
 *
 *  @veth_emac_mem - Contains the virtual and physical addresses
 *  of the exported memory
 *
 *  @pdata - Contains the network device information
 */
int veth_emac_ipa_setup_complete(struct veth_emac_export_mem *veth_emac_mem,
				struct veth_ipa_dev *pdata)
{

	int ret = 0;

	veth_emac_mem->exp_id.event_id = VETH_IPA_SETUP_COMPLETE;
	VETH_IPA_DEBUG("%s: Send TX buf memory %d\n", __func__);

	ret = habmm_socket_send(veth_emac_mem->vc_id,
					&veth_emac_mem->exp_id,
					sizeof(veth_emac_mem->exp_id),
					NO_FLAGS);
	if (ret) {
		VETH_IPA_ERROR("%s: Send failed %d returned\n",
						__func__, ret);
		ret = -1;
		return ret;
	}

	return 0;
}


/** veth_emac_start_offload() - This API is used to send the
 *  START_OFFLOAD event to the BE driver. It is triggered when
 *  the IPA setup, memory export and emac side set up is
 *  complete. We turn on the net carrier and the start queue
 *
 *  @veth_emac_mem - Contains the virtual and physical addresses
 *  of the exported memory
 *
 *  @pdata - Contains the network device information
 */
int veth_emac_start_offload(struct veth_emac_export_mem *veth_emac_mem,
					struct veth_ipa_dev *pdata)
{

	int ret = 0;

	veth_emac_mem->exp_id.event_id = VETH_IPA_START_OFFLOAD;
	VETH_IPA_DEBUG("%s: Send TX buf memory %d\n", __func__);
	ret = habmm_socket_send(veth_emac_mem->vc_id,
					&veth_emac_mem->exp_id,
					sizeof(veth_emac_mem->exp_id),
					NO_FLAGS);
	if (ret) {
		VETH_IPA_ERROR("%s: Send failed %d returned\n",
						__func__, ret);
		ret = -1;
		return ret;
	}
	return 0;
}




/** veth_emac_stop_offload() - This API is used to send the STOP
 *  event to the BE driver. This event is triggered after the
 *  clean up at the FE driver an we instruct the BE driver to
 *  clean up the memory and start the clean up process
 *
 *  @veth_emac_mem - Contains the virtual and physical addresses
 *  of the exported memory
 *
 *  @pdata - Contains the network device information
 */
int veth_emac_stop_offload(struct veth_emac_export_mem *veth_emac_mem,
				struct veth_ipa_dev *pdata)
{
	int ret = 0;

	veth_emac_mem->exp_id.event_id = VETH_IPA_STOP_OFFLOAD;
	VETH_IPA_DEBUG("%s: Send TX buf memory %d\n", __func__);
	ret = habmm_socket_send(veth_emac_mem->vc_id,
					&veth_emac_mem->exp_id,
					sizeof(veth_emac_mem->exp_id),
					NO_FLAGS);
	if (ret) {
		VETH_IPA_ERROR("%s: Send failed %d returned\n",
						__func__, ret);
		ret = -1;
		return ret;
	}

	return 0;
}

/** veth_emac_setup_be() - This API is used to send
 *  the SETUP_OFFLOAD event to the BE driver. It is triggered
 *  to notify the BE driver to set up the memory in the BE
 *  driver before starting the offload
 *
 *  @veth_emac_mem - Contains the virtual and physical addresses
 *  of the exported memory
 *
 *  @pdata - Contains the network device information
 */
int veth_emac_setup_be(struct veth_emac_export_mem *veth_emac_mem,
					   struct veth_ipa_dev *pdata)
{
	int ret = 0;

	veth_emac_mem->exp_id.event_id = VETH_IPA_SETUP_OFFLOAD;
	VETH_IPA_DEBUG("%s: Send TX buf memory %d\n", __func__);
	ret = habmm_socket_send(veth_emac_mem->vc_id,
					&veth_emac_mem->exp_id,
					sizeof(veth_emac_mem->exp_id),
					NO_FLAGS);
	if (ret) {
		VETH_IPA_ERROR("%s: Send failed %d returned\n",
						__func__, ret);
		ret = -1;
		return ret;
	}
	return 0;
}


/** veth_emac_open_notify() - This API is used to send the
 *  OPEN_NOTIFY event to the BE driver. This event is sent to
 *  notify the BE driver that veth_ipa_open is called when the
 *  ifconfig up command is triggered on VETH0 iface
 *
 *  @veth_emac_mem - Contains the virtual and physical addresses
 *  of the exported memory
 *
 *  @pdata - Contains the network device information
 */
int veth_emac_open_notify(struct veth_emac_export_mem *veth_emac_mem,
						  struct veth_ipa_dev *pdata)
{
	int ret = 0;

	veth_emac_mem->exp_id.event_id = VETH_IPA_OPEN_EV;
	VETH_IPA_DEBUG("%s: Send TX buf memory %d\n", __func__);

	ret = habmm_socket_send(veth_emac_mem->vc_id,
					&veth_emac_mem->exp_id,
					sizeof(veth_emac_mem->exp_id),
					NO_FLAGS);
	if (ret) {
		VETH_IPA_ERROR("%s: Send failed %d returned\n",
						__func__, ret);
		ret = -1;
		return ret;
	}
	return 0;
}

