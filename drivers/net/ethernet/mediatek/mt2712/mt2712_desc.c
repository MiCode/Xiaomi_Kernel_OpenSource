/* Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "mt2712_yheader.h"
#include "mt2712_desc.h"
#include "mt2712_yregacc.h"

/* \brief API to free the transmit descriptor memory.
 *
 * \details This function is used to free the transmit descriptor memory.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \retval void.
 */

static void tx_desc_free_mem(struct prv_data *pdata, unsigned int tx_qcnt)
{
	struct tx_wrapper_descriptor *desc_data = NULL;
	unsigned int q_inx;

	for (q_inx = 0; q_inx < tx_qcnt; q_inx++) {
		desc_data = GET_TX_WRAPPER_DESC(q_inx);

		if (GET_TX_DESC_PTR(q_inx, 0)) {
			dma_free_coherent(&pdata->pdev->dev,
					  (sizeof(struct s_TX_NORMAL_DESC) * TX_DESC_CNT),
					  GET_TX_DESC_PTR(q_inx, 0),
					  GET_TX_DESC_DMA_ADDR(q_inx, 0));
			GET_TX_DESC_PTR(q_inx, 0) = NULL;
		}
	}
}

/* \brief API to free the receive descriptor memory.
 *
 * \details This function is used to free the receive descriptor memory.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \retval void.
 */

static void rx_desc_free_mem(struct prv_data *pdata, unsigned int rx_qcnt)
{
	struct rx_wrapper_descriptor *desc_data = NULL;
	unsigned int q_inx = 0;

	for (q_inx = 0; q_inx < rx_qcnt; q_inx++) {
		desc_data = GET_RX_WRAPPER_DESC(q_inx);

		if (GET_RX_DESC_PTR(q_inx, 0)) {
			dma_free_coherent(&pdata->pdev->dev,
					  (sizeof(struct s_RX_NORMAL_DESC) * RX_DESC_CNT),
					  GET_RX_DESC_PTR(q_inx, 0),
					  GET_RX_DESC_DMA_ADDR(q_inx, 0));
			GET_RX_DESC_PTR(q_inx, 0) = NULL;
		}
	}
}

/* \brief API to alloc the queue memory.
 *
 * \details This function allocates the queue structure memory.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \return integer
 *
 * \retval 0 on success & -ve number on failure.
 */

static int alloc_queue_struct(struct prv_data *pdata)
{
	int ret = 0;

	pdata->tx_queue =
		kzalloc(sizeof(struct tx_queue) * pdata->tx_queue_cnt, GFP_KERNEL);
	if (!pdata->tx_queue) {
		pr_err("ERROR: Unable to allocate Tx queue structure\n");
		ret = -ENOMEM;
		goto err_out_tx_q_alloc_failed;
	}

	pdata->rx_queue =
		kzalloc(sizeof(struct rx_queue) * pdata->rx_queue_cnt, GFP_KERNEL);
	if (!pdata->rx_queue) {
		pr_err("ERROR: Unable to allocate Rx queue structure\n");
		ret = -ENOMEM;
		goto err_out_rx_q_alloc_failed;
	}

	return ret;

err_out_rx_q_alloc_failed:
	kfree(pdata->tx_queue);

err_out_tx_q_alloc_failed:
	return ret;
}

/* \brief API to free the queue memory.
 *
 * \details This function free the queue structure memory.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \return void
 */

static void free_queue_struct(struct prv_data *pdata)
{
	kfree(pdata->tx_queue);
	pdata->tx_queue = NULL;
	kfree(pdata->rx_queue);
	pdata->rx_queue = NULL;
}

/* \brief API to allocate the memory for descriptor & buffers.
 *
 * \details This function is used to allocate the memory for device
 * descriptors & buffers
 * which are used by device for data transmission & reception.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \return integer
 *
 * \retval 0 on success & -ENOMEM number on failure.
 */

static int allocate_buffer_and_desc(struct prv_data *pdata)
{
	int ret = 0;
	unsigned int q_inx;

	/* Allocate descriptors and buffers memory for all TX queues */
	for (q_inx = 0; q_inx < TX_QUEUE_CNT; q_inx++) {
		/* TX descriptors */
		GET_TX_DESC_PTR(q_inx, 0) = dma_alloc_coherent(
						&pdata->pdev->dev,
						(sizeof(struct s_TX_NORMAL_DESC) * TX_DESC_CNT),
						&(GET_TX_DESC_DMA_ADDR(q_inx, 0)),
						GFP_KERNEL);
		if (!GET_TX_DESC_PTR(q_inx, 0)) {
			ret = -ENOMEM;
			goto err_out_tx_desc;
		}
	}

	for (q_inx = 0; q_inx < TX_QUEUE_CNT; q_inx++) {
		/* TX wrapper buffer */
		GET_TX_BUF_PTR(q_inx, 0) =
			kzalloc((sizeof(struct tx_buffer) * TX_DESC_CNT), GFP_KERNEL);
		if (!GET_TX_BUF_PTR(q_inx, 0)) {
			ret = -ENOMEM;
			goto err_out_tx_buf;
		}
	}

	/* Allocate descriptors and buffers memory for all RX queues */
	for (q_inx = 0; q_inx < RX_QUEUE_CNT; q_inx++) {
		/* RX descriptors */
		GET_RX_DESC_PTR(q_inx, 0) = dma_alloc_coherent(
						&pdata->pdev->dev,
						(sizeof(struct s_RX_NORMAL_DESC) * RX_DESC_CNT),
						&(GET_RX_DESC_DMA_ADDR(q_inx, 0)),
						GFP_KERNEL);
		if (!GET_RX_DESC_PTR(q_inx, 0)) {
			ret = -ENOMEM;
			goto rx_alloc_failure;
		}
	}

	for (q_inx = 0; q_inx < RX_QUEUE_CNT; q_inx++) {
		/* RX wrapper buffer */
		GET_RX_BUF_PTR(q_inx, 0) =
			kzalloc((sizeof(struct rx_buffer) * RX_DESC_CNT),
				GFP_KERNEL);
		if (!GET_RX_BUF_PTR(q_inx, 0)) {
			ret = -ENOMEM;
			goto err_out_rx_buf;
		}
	}

	return ret;

 err_out_rx_buf:
	rx_buf_free_mem(pdata, q_inx);
	q_inx = RX_QUEUE_CNT;

 rx_alloc_failure:
	rx_desc_free_mem(pdata, q_inx);
	q_inx = TX_QUEUE_CNT;

 err_out_tx_buf:
	tx_buf_free_mem(pdata, q_inx);
	q_inx = TX_QUEUE_CNT;

 err_out_tx_desc:
	tx_desc_free_mem(pdata, q_inx);

	return ret;
}

/* \brief API to initialize the transmit descriptors.
 *
 * \details This function is used to initialize transmit descriptors.
 * Each descriptors are assigned a buffer. The base/starting address
 * of the descriptors is updated in device register if required & all
 * the private data structure variables related to transmit
 * descriptor handling are updated in this function.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \return void.
 */

static void wrapper_tx_descriptor_init_single_q(
			struct prv_data *pdata,
			unsigned int q_inx)
{
	int i;
	struct tx_wrapper_descriptor *desc_data =
	    GET_TX_WRAPPER_DESC(q_inx);
	struct tx_buffer *buffer = GET_TX_BUF_PTR(q_inx, 0);
	struct s_TX_NORMAL_DESC *desc = GET_TX_DESC_PTR(q_inx, 0);
	dma_addr_t desc_dma = GET_TX_DESC_DMA_ADDR(q_inx, 0);
	struct hw_if_struct *hw_if = &pdata->hw_if;

	for (i = 0; i < TX_DESC_CNT; i++) {
		GET_TX_DESC_PTR(q_inx, i) = &desc[i];
		GET_TX_DESC_DMA_ADDR(q_inx, i) =
		    (desc_dma + sizeof(struct s_TX_NORMAL_DESC) * i);
		GET_TX_BUF_PTR(q_inx, i) = &buffer[i];
	}

	desc_data->cur_tx = 0;
	desc_data->dirty_tx = 0;
	desc_data->queue_stopped = 0;
	desc_data->tx_pkt_queued = 0;
	desc_data->packet_count = 0;
	desc_data->free_desc_cnt = TX_DESC_CNT;

	hw_if->tx_desc_init(pdata, q_inx);

	desc_data->cur_tx = 0;
}

/* \brief API to initialize the receive descriptors.
 *
 * \details This function is used to initialize receive descriptors.
 * skb buffer is allocated & assigned for each descriptors. The base/starting
 * address of the descriptors is updated in device register if required and
 * all the private data structure variables related to receive descriptor
 * handling are updated in this function.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \return void.
 */

static void wrapper_rx_descriptor_init_single_q(
			struct prv_data *pdata,
			unsigned int q_inx)
{
	int i;
	struct rx_wrapper_descriptor *desc_data =
	    GET_RX_WRAPPER_DESC(q_inx);
	struct rx_buffer *buffer = GET_RX_BUF_PTR(q_inx, 0);
	struct s_RX_NORMAL_DESC *desc = GET_RX_DESC_PTR(q_inx, 0);
	dma_addr_t desc_dma = GET_RX_DESC_DMA_ADDR(q_inx, 0);
	struct hw_if_struct *hw_if = &pdata->hw_if;

	memset(buffer, 0, (sizeof(struct rx_buffer) * RX_DESC_CNT));

	for (i = 0; i < RX_DESC_CNT; i++) {
		GET_RX_DESC_PTR(q_inx, i) = &desc[i];
		GET_RX_DESC_DMA_ADDR(q_inx, i) =
		    (desc_dma + sizeof(struct s_RX_NORMAL_DESC) * i);
		GET_RX_BUF_PTR(q_inx, i) = &buffer[i];

		/* allocate skb & assign to each desc */
		if (pdata->alloc_rx_buf(pdata, GET_RX_BUF_PTR(q_inx, i), GFP_KERNEL))
			break;

		wmb(); /* rx buf alloc done */
	}

	desc_data->cur_rx = 0;
	desc_data->dirty_rx = 0;
	desc_data->skb_realloc_idx = 0;
	desc_data->skb_realloc_threshold = MIN_RX_DESC_CNT;
	desc_data->pkt_received = 0;

	hw_if->rx_desc_init(pdata, q_inx);

	desc_data->cur_rx = 0;
}

static void wrapper_tx_descriptor_init(struct prv_data *pdata)
{
	unsigned int q_inx;

	for (q_inx = 0; q_inx < TX_QUEUE_CNT; q_inx++)
		wrapper_tx_descriptor_init_single_q(pdata, q_inx);
}

static void wrapper_rx_descriptor_init(struct prv_data *pdata)
{
	struct rx_queue *rx_queue = NULL;
	unsigned int q_inx;

	for (q_inx = 0; q_inx < RX_QUEUE_CNT; q_inx++) {
		rx_queue = GET_RX_QUEUE_PTR(q_inx);
		rx_queue->pdata = pdata;

		wrapper_rx_descriptor_init_single_q(pdata, q_inx);
	}
}

/* \brief API to free the receive descriptor & buffer memory.
 *
 * \details This function is used to free the receive descriptor & buffer memory.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \retval void.
 */

static void rx_free_mem(struct prv_data *pdata)
{
	/* free RX descriptor */
	rx_desc_free_mem(pdata, RX_QUEUE_CNT);

	/* free RX skb's */
	rx_skb_free_mem(pdata, RX_QUEUE_CNT);

	/* free RX wrapper buffer */
	rx_buf_free_mem(pdata, RX_QUEUE_CNT);
}

/* \brief API to free the transmit descriptor & buffer memory.
 *
 * \details This function is used to free the transmit descriptor
 * & buffer memory.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \retval void.
 */

static void tx_free_mem(struct prv_data *pdata)
{
	/* free TX descriptor */
	tx_desc_free_mem(pdata, TX_QUEUE_CNT);

	/* free TX buffer */
	tx_buf_free_mem(pdata, TX_QUEUE_CNT);
}

/* \details This function is invoked by other function to free
 * the tx socket buffers.
 *
 * \param[in] pdata – pointer to private data structure.
 *
 * \return void
 */

static void tx_skb_free_mem_single_q(struct prv_data *pdata, unsigned int q_inx)
{
	unsigned int i;

	for (i = 0; i < TX_DESC_CNT; i++)
		unmap_tx_skb(pdata, GET_TX_BUF_PTR(q_inx, i));
}

/* \brief API to free the transmit descriptor skb memory.
 *
 * \details This function is used to free the transmit descriptor skb memory.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \retval void.
 */

static void tx_skb_free_mem(struct prv_data *pdata, unsigned int tx_qcnt)
{
	unsigned int q_inx;

	for (q_inx = 0; q_inx < tx_qcnt; q_inx++)
		tx_skb_free_mem_single_q(pdata, q_inx);
}

/* \details This function is invoked by other function to free
 * the rx socket buffers.
 *
 * \param[in] pdata – pointer to private data structure.
 *
 * \return void
 */

static void rx_skb_free_mem_single_q(struct prv_data *pdata, unsigned int q_inx)
{
	unsigned int i;

	for (i = 0; i < RX_DESC_CNT; i++)
		unmap_rx_skb(pdata, GET_RX_BUF_PTR(q_inx, i));
}

/* \brief API to free the receive descriptor skb memory.
 *
 * \details This function is used to free the receive descriptor skb memory.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \retval void.
 */

static void rx_skb_free_mem(struct prv_data *pdata, unsigned int rx_qcnt)
{
	unsigned int q_inx;

	for (q_inx = 0; q_inx < rx_qcnt; q_inx++)
		rx_skb_free_mem_single_q(pdata, q_inx);
}

/* \brief API to free the transmit descriptor wrapper buffer memory.
 *
 * \details This function is used to free the transmit descriptor wrapper buffer memory.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \retval void.
 */

static void tx_buf_free_mem(struct prv_data *pdata, unsigned int tx_qcnt)
{
	unsigned int q_inx;

	for (q_inx = 0; q_inx < tx_qcnt; q_inx++) {
		/* free TX buffer */
		if (GET_TX_BUF_PTR(q_inx, 0)) {
			kfree(GET_TX_BUF_PTR(q_inx, 0));
			GET_TX_BUF_PTR(q_inx, 0) = NULL;
		}
	}
}

/* \brief API to free the receive descriptor wrapper buffer memory.
 *
 * \details This function is used to free the receive descriptor wrapper buffer memory.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \retval void.
 */

static void rx_buf_free_mem(struct prv_data *pdata, unsigned int rx_qcnt)
{
	unsigned int q_inx = 0;

	for (q_inx = 0; q_inx < rx_qcnt; q_inx++) {
		if (GET_RX_BUF_PTR(q_inx, 0)) {
			kfree(GET_RX_BUF_PTR(q_inx, 0));
			GET_RX_BUF_PTR(q_inx, 0) = NULL;
		}
	}
}

/* returns 0 on success and -ve on failure */
static int map_non_page_buffs(struct prv_data *pdata,
			      struct tx_buffer *buffer,
			      struct tx_buffer *prev_buffer,
			      struct sk_buff *skb,
			      unsigned int offset,
			      unsigned int size)
{
	if (size > MAX_DATA_PER_TX_BUF) {
		if (prev_buffer && !prev_buffer->dma2) {
			/* fill the first buffer pointer in prev_buffer->dma2 */
			prev_buffer->dma2 = dma_map_single((&pdata->pdev->dev),
							(skb->data + offset),
							MAX_DATA_PER_TX_BUF,
							DMA_TO_DEVICE);
			if (dma_mapping_error((&pdata->pdev->dev), prev_buffer->dma2)) {
				pr_err("failed to do the dma map\n");
				return -ENOMEM;
			}
			prev_buffer->len2 = MAX_DATA_PER_TX_BUF;
			prev_buffer->buf2_mapped_as_page = Y_FALSE;

			/* fill the second buffer pointer in buffer->dma */
			buffer->dma = dma_map_single((&pdata->pdev->dev),
						(skb->data + offset + MAX_DATA_PER_TX_BUF),
						(size - MAX_DATA_PER_TX_BUF),
						DMA_TO_DEVICE);
			if (dma_mapping_error((&pdata->pdev->dev), buffer->dma)) {
				pr_err("failed to do the dma map\n");
				return -ENOMEM;
			}
			buffer->len = (size - MAX_DATA_PER_TX_BUF);
			buffer->buf1_mapped_as_page = Y_FALSE;
			buffer->dma2 = 0;
			buffer->len2 = 0;
		} else {
			/* fill the first buffer pointer in buffer->dma */
			buffer->dma = dma_map_single((&pdata->pdev->dev),
					(skb->data + offset),
					MAX_DATA_PER_TX_BUF,
					DMA_TO_DEVICE);
			if (dma_mapping_error((&pdata->pdev->dev), buffer->dma)) {
				pr_err("failed to do the dma map\n");
				return -ENOMEM;
			}
			buffer->len = MAX_DATA_PER_TX_BUF;
			buffer->buf1_mapped_as_page = Y_FALSE;

			/* fill the second buffer pointer in buffer->dma2 */
			buffer->dma2 = dma_map_single((&pdata->pdev->dev),
					(skb->data + offset + MAX_DATA_PER_TX_BUF),
					(size - MAX_DATA_PER_TX_BUF),
					DMA_TO_DEVICE);
			if (dma_mapping_error((&pdata->pdev->dev), buffer->dma2)) {
				pr_err("failed to do the dma map\n");
				return -ENOMEM;
			}
			buffer->len2 = (size - MAX_DATA_PER_TX_BUF);
			buffer->buf2_mapped_as_page = Y_FALSE;
		}
	} else {
		if (prev_buffer && !prev_buffer->dma2) {
			/* fill the first buffer pointer in prev_buffer->dma2 */
			prev_buffer->dma2 = dma_map_single((&pdata->pdev->dev),
						(skb->data + offset),
						size, DMA_TO_DEVICE);
			if (dma_mapping_error((&pdata->pdev->dev), prev_buffer->dma2)) {
				pr_err("failed to do the dma map\n");
				return -ENOMEM;
			}
			prev_buffer->len2 = size;
			prev_buffer->buf2_mapped_as_page = Y_FALSE;

			/* indicate current buffer struct is not used */
			buffer->dma = 0;
			buffer->len = 0;
			buffer->dma2 = 0;
			buffer->len2 = 0;
		} else {
			/* fill the first buffer pointer in buffer->dma */
			buffer->dma = dma_map_single((&pdata->pdev->dev),
						(skb->data + offset),
						size, DMA_TO_DEVICE);
			if (dma_mapping_error((&pdata->pdev->dev), buffer->dma)) {
				pr_err("failed to do the dma map\n");
				return -ENOMEM;
			}
			buffer->len = size;
			buffer->buf1_mapped_as_page = Y_FALSE;
			buffer->dma2 = 0;
			buffer->len2 = 0;
		}
	}

	return 0;
}

/* \details This function is invoked by start_xmit functions. This function
 * will get the dma/physical address of the packet to be transmitted and
 * its length. All this information about the packet to be transmitted is
 * stored in private data structure and same is used later in the driver to
 * setup the descriptor for transmission.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] skb – pointer to socket buffer structure.
 *
 * \return unsigned int
 *
 * \retval count – number of packet to be programmed in the descriptor or
 * zero on failure.
 */

static unsigned int map_skb(struct net_device *dev, struct sk_buff *skb)
{
	struct prv_data *pdata = netdev_priv(dev);
	unsigned int q_inx = skb_get_queue_mapping(skb);
	struct tx_wrapper_descriptor *desc_data =
	    GET_TX_WRAPPER_DESC(q_inx);
	struct tx_buffer *buffer =
	    GET_TX_BUF_PTR(q_inx, desc_data->cur_tx);
	struct tx_buffer *prev_buffer = NULL;
	int index = (int)desc_data->cur_tx;
	unsigned int count = 0, offset = 0, size;
	int len;
	int ret;

	len = (skb->len - skb->data_len);

	while (len) {
		size = min(len, MAX_DATA_PER_TXD);

		buffer = GET_TX_BUF_PTR(q_inx, index);
		ret = map_non_page_buffs(pdata, buffer,
					 prev_buffer,
					 skb, offset, size);
		if (ret < 0)
			goto err_out_dma_map_fail;

		len -= size;
		offset += size;
		prev_buffer = buffer;
		INCR_TX_DESC_INDEX(index, 1);
		count++;
	}

	buffer->skb = skb;

	return count;

 err_out_dma_map_fail:
	pr_err("Tx DMA map failed\n");

	for (; count > 0; count--) {
		DECR_TX_DESC_INDEX(index);
		buffer = GET_TX_BUF_PTR(q_inx, index);
		unmap_tx_skb(pdata, buffer);
	}

	return 0;
}

/* \brief API to release the skb.
 *
 * \details This function is called in *_tx_interrupt function to release
 * the skb for the successfully transmited packets.
 *
 * \param[in] pdata - pointer to private data structure.
 * \param[in] buffer - pointer to *_tx_buffer structure
 *
 * \return void
 */

static void unmap_tx_skb(struct prv_data *pdata, struct tx_buffer *buffer)
{
	if (buffer->dma) {
		if (buffer->buf1_mapped_as_page == Y_TRUE)
			dma_unmap_page(&pdata->pdev->dev, buffer->dma,
				       buffer->len, DMA_TO_DEVICE);
		else
			dma_unmap_single(&pdata->pdev->dev, buffer->dma,
					 buffer->len, DMA_TO_DEVICE);

		buffer->dma = 0;
		buffer->len = 0;
	}

	if (buffer->dma2) {
		if (buffer->buf2_mapped_as_page == Y_TRUE)
			dma_unmap_page(&pdata->pdev->dev, buffer->dma2,
				       buffer->len2, DMA_TO_DEVICE);
		else
			dma_unmap_single(&pdata->pdev->dev, buffer->dma2,
					 buffer->len2, DMA_TO_DEVICE);

		buffer->dma2 = 0;
		buffer->len2 = 0;
	}

	if (buffer->skb) {
		dev_kfree_skb_any(buffer->skb);
		buffer->skb = NULL;
	}
}

/* \details This function is invoked by other function for releasing the socket
 * buffer which are received by device and passed to upper layer.
 *
 * \param[in] pdata – pointer to private device structure.
 * \param[in] buffer – pointer to rx wrapper buffer structure.
 *
 * \return void
 */

static void unmap_rx_skb(struct prv_data *pdata, struct rx_buffer *buffer)
{
	/* unmap the first buffer */
	if (buffer->dma) {
		dma_unmap_single(&pdata->pdev->dev, buffer->dma,
				 pdata->rx_buffer_len, DMA_FROM_DEVICE);
		buffer->dma = 0;
	}

	/* unmap the second buffer */
	if (buffer->dma2) {
		dma_unmap_page(&pdata->pdev->dev, buffer->dma2,
			       PAGE_SIZE, DMA_FROM_DEVICE);
		buffer->dma2 = 0;
	}

	/* page1 will be present only if JUMBO is enabled */
	if (buffer->page) {
		put_page(buffer->page);
		buffer->page = NULL;
	}
	/* page2 will be present if JUMBO/SPLIT HDR is enabled */
	if (buffer->page2) {
		put_page(buffer->page2);
		buffer->page2 = NULL;
	}

	if (buffer->skb) {
		dev_kfree_skb_any(buffer->skb);
		buffer->skb = NULL;
	}
}

/* \brief API to re-allocate the new skb to rx descriptors.
 *
 * \details This function is used to re-allocate & re-assign the new skb to
 * receive descriptors from which driver has read the data. Also ownership bit
 * and other bits are reset so that device can reuse the descriptors.
 *
 * \param[in] pdata - pointer to private data structure.
 *
 * \return void.
 */

static void re_alloc_skb(struct prv_data *pdata, unsigned int q_inx)
{
	int i;
	struct rx_wrapper_descriptor *desc_data =
	    GET_RX_WRAPPER_DESC(q_inx);
	struct rx_buffer *buffer = NULL;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	int tail_idx;

	for (i = 0; i < desc_data->dirty_rx; i++) {
		buffer = GET_RX_BUF_PTR(q_inx, desc_data->skb_realloc_idx);
		/* allocate skb & assign to each desc */
		if (pdata->alloc_rx_buf(pdata, buffer, GFP_ATOMIC)) {
			pr_err("Failed to re allocate skb\n");
			pdata->xstats.q_re_alloc_rx_buf_failed[q_inx]++;
			break;
		}

		wmb(); /* alloc done */
		hw_if->rx_desc_reset(desc_data->skb_realloc_idx, pdata,
				     buffer->inte, q_inx);
		INCR_RX_DESC_INDEX(desc_data->skb_realloc_idx, 1);
	}
	tail_idx = desc_data->skb_realloc_idx;
	DECR_RX_DESC_INDEX(tail_idx);
	hw_if->update_rx_tail_ptr(q_inx,
		GET_RX_DESC_DMA_ADDR(q_inx, tail_idx));
	desc_data->dirty_rx = 0;
}

/* \brief API to initialize the function pointers.
 *
 * \details This function is called in probe to initialize all the function
 * pointers which are used in other functions to manage edscriptors.
 *
 * \param[in] desc_if - pointer to desc_if_struct structure.
 *
 * \return void.
 */

void init_function_ptrs_desc(struct desc_if_struct *desc_if)
{
	desc_if->alloc_queue_struct = alloc_queue_struct;
	desc_if->free_queue_struct = free_queue_struct;
	desc_if->alloc_buff_and_desc = allocate_buffer_and_desc;
	desc_if->realloc_skb = re_alloc_skb;
	desc_if->unmap_rx_skb = unmap_rx_skb;
	desc_if->unmap_tx_skb = unmap_tx_skb;
	desc_if->map_tx_skb = map_skb;
	desc_if->tx_free_mem = tx_free_mem;
	desc_if->rx_free_mem = rx_free_mem;
	desc_if->wrapper_tx_desc_init = wrapper_tx_descriptor_init;
	desc_if->wrapper_tx_desc_init_single_q = wrapper_tx_descriptor_init_single_q;
	desc_if->wrapper_rx_desc_init = wrapper_rx_descriptor_init;
	desc_if->wrapper_rx_desc_init_single_q = wrapper_rx_descriptor_init_single_q;

	desc_if->rx_skb_free_mem = rx_skb_free_mem;
	desc_if->rx_skb_free_mem_single_q = rx_skb_free_mem_single_q;
	desc_if->tx_skb_free_mem = tx_skb_free_mem;
	desc_if->tx_skb_free_mem_single_q = tx_skb_free_mem_single_q;
}
