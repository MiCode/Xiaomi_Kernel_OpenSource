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

#ifndef __MT2712_DESC_H__
#define __MT2712_DESC_H__

static int allocate_buffer_and_desc(struct prv_data *);

static void wrapper_tx_descriptor_init(struct prv_data *pdata);

static void wrapper_tx_descriptor_init_single_q(struct prv_data *pdata, unsigned int q_inx);

static void wrapper_rx_descriptor_init(struct prv_data *pdata);

static void wrapper_rx_descriptor_init_single_q(struct prv_data *pdata, unsigned int q_inx);

static void tx_free_mem(struct prv_data *);

static void rx_free_mem(struct prv_data *);

static unsigned int map_skb(struct net_device *, struct sk_buff *);

static void unmap_tx_skb(struct prv_data *, struct tx_buffer *);

static void unmap_rx_skb(struct prv_data *, struct rx_buffer *);

static void re_alloc_skb(struct prv_data *pdata, unsigned int q_inx);

static void tx_desc_free_mem(struct prv_data *pdata, unsigned int tx_q_cnt);

static void tx_buf_free_mem(struct prv_data *pdata, unsigned int tx_q_cnt);

static void rx_desc_free_mem(struct prv_data *pdata, unsigned int rx_q_cnt);

static void rx_buf_free_mem(struct prv_data *pdata, unsigned int rx_q_cnt);

static void rx_skb_free_mem(struct prv_data *pdata, unsigned int rx_q_cnx);

static void tx_skb_free_mem(struct prv_data *pdata, unsigned int tx_q_cnx);

#endif

