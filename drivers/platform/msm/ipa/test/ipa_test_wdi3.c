/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#include "ipa_ut_framework.h"
#include <linux/ipa_wdi3.h>
#include <linux/ipa.h>
#include <linux/delay.h>
#include "../ipa_v3/ipa_i.h"

#define NUM_TX_BUFS 10
#define NUM_RX_BUFS 10
#define NUM_REDUNDANT_TX_ELE 1
#define NUM_RX_TR_ELE NUM_RX_BUFS
#define NUM_RX_ER_ELE NUM_RX_BUFS
#define NUM_TX_TR_ELE (NUM_TX_BUFS + NUM_REDUNDANT_TX_ELE)
#define NUM_TX_ER_ELE (NUM_TX_BUFS + NUM_REDUNDANT_TX_ELE)

#define RX_METADATA_SIZE 4
#define PACKET_HEADER_SIZE 220
#define ETH_PACKET_SIZE 4
#define PACKET_CONTENT 0x12345678

#define PKT_SIZE 4096

#define DB_REGISTER_SIZE 4

#define NUM_MULTI_PKT 8

int multi_pkt_array[] = {0x12345678, 0x87654321,
	0x00112233, 0x01234567, 0x45454545, 0x80808080,
	0x13245678, 0x12345767, 0x43213456};

int rx_uc_db_local;
int tx_uc_db_local;
u8 tx_bf_idx;
u8 rx_bf_idx;

struct ipa_test_wdi3_context {
	struct ipa_mem_buffer tx_transfer_ring_addr;
	struct ipa_mem_buffer tx_event_ring_addr;
	struct ipa_mem_buffer rx_transfer_ring_addr;
	struct ipa_mem_buffer rx_event_ring_addr;
	struct ipa_mem_buffer tx_bufs[NUM_TX_BUFS];
	struct ipa_mem_buffer rx_bufs[NUM_RX_BUFS];
	struct ipa_mem_buffer tx_transfer_ring_db;
	struct ipa_mem_buffer tx_event_ring_db;
	struct ipa_mem_buffer rx_transfer_ring_db;
	struct ipa_mem_buffer rx_event_ring_db;
	dma_addr_t tx_uc_db_pa;
	dma_addr_t rx_uc_db_pa;
};

static struct ipa_test_wdi3_context *test_wdi3_ctx;

struct buffer_addr_info {
	u32 buffer_addr_low;
	u32 buffer_addr_high : 8;
	u32 return_buffer_manager : 3;
	u32 sw_buffer_cookie : 21;
} __packed;

struct tx_transfer_ring_ele {
	struct buffer_addr_info buf_or_link_desc_addr_info;
	u32 resv[6];
} __packed;

struct tx_event_ring_ele {
	u32 reserved_5;
	struct buffer_addr_info buf_or_link_desc_addr_info;
	u32 buf_or_ext_desc_type : 1;
	u32 epd : 1;
	u32 encap_type : 2;
	u32 encrypt_type : 4;
	u32 src_buffer_swap : 1;
	u32 link_meta_swap : 1;
	u32 hlos_tid : 4;
	u32 addrX_en : 1;
	u32 addrY_en : 1;
	u32 tcl_cmd_number : 16;
	u32 data_length : 16;
	u32 ipv4_checksum_en : 1;
	u32 udp_over_ipv4_checksum_en : 1;
	u32 udp_over_ipv6_checksum_en : 1;
	u32 tcp_over_ipv4_checksum_en : 1;
	u32 tcp_over_ipv6_checksum_en : 1;
	u32 to_fw : 1;
	u32 dscp_to_tid_priority_table_id : 1;
	u32 packet_offset : 9;
	u32 buffer_timestamp : 19;
	u32 buffer_timestamp_valid : 1;
	u32 reserved_4 : 12;
	u32 reserved_6;
	u32 reserved_7a : 20;
	u32 ring_id : 8;
	u32 looping_count : 4;
} __packed;

struct rx_mpdu_desc_info {
	u32 msdu_count : 8;
	u32 mpdu_sequence_number : 12;
	u32 fragment_flag : 1;
	u32 mpdu_retry_bit : 1;
	u32 ampdu_flag : 1;
	u32 bar_frame : 1;
	u32 pn_fields_contain_valid_info : 1;
	u32 sa_is_valid : 1;
	u32 sa_idx_timeout : 1;
	u32 da_is_valid : 1;
	u32 da_is_mcbc : 1;
	u32 da_idx_timeout : 1;
	u32 raw_mpdu : 1;
	u32 reserved : 1;
	u32 peer_meta_data;
} __packed;

struct rx_msdu_desc_info {
	u32 first_msdu_in_mpdu_flag : 1;
	u32 last_msdu_in_mpdu_flag : 1;
	u32 msdu_continuation : 1;
	u32 msdu_length : 14;
	u32 reo_destination_indication : 5;
	u32 msdu_drop : 1;
	u32 sa_is_valid : 1;
	u32 sa_idx_timeout : 1;
	u32 da_is_valid : 1;
	u32 da_is_mcbc : 1;
	u32 da_idx_timeout : 1;
	u32 reserved_0a : 4;
	u32 reserved_1a;
} __packed;

struct rx_transfer_ring_ele {
	struct buffer_addr_info buf_or_link_desc_addr_info;
	struct rx_mpdu_desc_info rx_mpdu_desc_info_details;
	struct rx_msdu_desc_info rx_msdu_desc_info_details;
	u32 rx_reo_queue_desc_addr_31_0;
	u32 rx_reo_queue_desc_addr_39_32 : 8;
	u32 reo_dest_buffer_type : 1;
	u32 reo_push_reason : 2;
	u32 reo_error_code : 5;
	u32 receive_queue_number : 16;
	u32 soft_reorder_info_valid : 1;
	u32 reorder_opcode : 4;
	u32 reorder_slot_index : 8;
	u32 reserved_8a : 19;
	u32 reserved_9a;
	u32 reserved_10a;
	u32 reserved_11a;
	u32 reserved_12a;
	u32 reserved_13a;
	u32 reserved_14a;
	u32 reserved_15a : 20;
	u32 ring_id : 8;
	u32 looping_count : 4;
} __packed;

struct rx_event_ring_ele {
	struct buffer_addr_info buf_or_link_desc_addr_info;
} __packed;

static void ipa_test_wdi3_free_dma_buff(struct ipa_mem_buffer *mem)
{
	if (!mem) {
		IPA_UT_ERR("empty pointer\n");
		return;
	}

	dma_free_coherent(ipa3_ctx->pdev, mem->size, mem->base,
		mem->phys_base);
}

static void ipa_test_wdi3_advance_uc_db(u32 *db, int steps,
	int num_words, int ring_size)
{
	*db = (*db + steps * num_words) % (ring_size / 4);
	IPA_UT_DBG("new db value: %u\n", *db);
}

static int ipa_test_wdi3_alloc_mmio(void)
{
	int ret = 0, i, j;
	int num_tx_alloc_bufs, num_rx_alloc_bufs;
	u32 size;

	if (!test_wdi3_ctx) {
		IPA_UT_ERR("test_wdi3_ctx is not initialized.\n");
		return -EFAULT;
	}

	/* allocate tx transfer ring memory */
	size = NUM_TX_TR_ELE * sizeof(struct tx_transfer_ring_ele);
	test_wdi3_ctx->tx_transfer_ring_addr.size = size;
	test_wdi3_ctx->tx_transfer_ring_addr.base =
		dma_alloc_coherent(ipa3_ctx->pdev, size,
			&test_wdi3_ctx->tx_transfer_ring_addr.phys_base,
			GFP_KERNEL);
	if (!test_wdi3_ctx->tx_transfer_ring_addr.phys_base) {
		IPA_UT_ERR("fail to alloc memory.\n");
		return -ENOMEM;
	}

	/* allocate tx event ring memory */
	size = NUM_TX_ER_ELE * sizeof(struct tx_event_ring_ele);
	test_wdi3_ctx->tx_event_ring_addr.size = size;
	test_wdi3_ctx->tx_event_ring_addr.base =
		dma_alloc_coherent(ipa3_ctx->pdev, size,
			&test_wdi3_ctx->tx_event_ring_addr.phys_base,
			GFP_KERNEL);
	if (!test_wdi3_ctx->tx_event_ring_addr.phys_base) {
		IPA_UT_ERR("fail to alloc memory.\n");
		ret = -ENOMEM;
		goto fail_tx_event_ring;
	}

	/* allocate rx transfer ring memory */
	size = NUM_RX_TR_ELE * sizeof(struct rx_transfer_ring_ele);
	test_wdi3_ctx->rx_transfer_ring_addr.size = size;
	test_wdi3_ctx->rx_transfer_ring_addr.base =
		dma_alloc_coherent(ipa3_ctx->pdev, size,
			&test_wdi3_ctx->rx_transfer_ring_addr.phys_base,
			GFP_KERNEL);
	if (!test_wdi3_ctx->rx_transfer_ring_addr.phys_base) {
		IPA_UT_ERR("fail to alloc memory.\n");
		ret = -ENOMEM;
		goto fail_rx_transfer_ring;
	}

	/* allocate rx event ring memory */
	size = NUM_RX_ER_ELE * sizeof(struct rx_event_ring_ele);
	test_wdi3_ctx->rx_event_ring_addr.size = size;
	test_wdi3_ctx->rx_event_ring_addr.base =
		dma_alloc_coherent(ipa3_ctx->pdev, size,
			&test_wdi3_ctx->rx_event_ring_addr.phys_base,
			GFP_KERNEL);
	if (!test_wdi3_ctx->rx_event_ring_addr.phys_base) {
		IPA_UT_ERR("fail to alloc memory.\n");
		ret = -ENOMEM;
		goto fail_rx_event_ring;
	}

	/* allocate tx buffers */
	num_tx_alloc_bufs = NUM_TX_BUFS;
	for (i = 0; i < NUM_TX_BUFS; i++) {
		size = ETH_PACKET_SIZE; //2kB buffer size;
		test_wdi3_ctx->tx_bufs[i].size = size;
		test_wdi3_ctx->tx_bufs[i].base =
			dma_alloc_coherent(ipa3_ctx->pdev, size,
				&test_wdi3_ctx->tx_bufs[i].phys_base,
				GFP_KERNEL);
		if (!test_wdi3_ctx->tx_bufs[i].phys_base) {
			IPA_UT_ERR("fail to alloc memory.\n");
			num_tx_alloc_bufs = i-1;
			ret = -ENOMEM;
			goto fail_tx_bufs;
		}
	}

	/* allocate rx buffers */
	num_rx_alloc_bufs = NUM_RX_BUFS;
	for (i = 0; i < NUM_RX_BUFS; i++) {
		size = ETH_PACKET_SIZE + PACKET_HEADER_SIZE; //2kB buffer size;
		test_wdi3_ctx->rx_bufs[i].size = size;
		test_wdi3_ctx->rx_bufs[i].base =
			dma_alloc_coherent(ipa3_ctx->pdev, size,
				&test_wdi3_ctx->rx_bufs[i].phys_base,
				GFP_KERNEL);
		if (!test_wdi3_ctx->rx_bufs[i].phys_base) {
			IPA_UT_ERR("fail to alloc memory.\n");
			num_rx_alloc_bufs = i-1;
			ret = -ENOMEM;
			goto fail_rx_bufs;
		}
	}

	/* allocate tx transfer ring db */
	test_wdi3_ctx->tx_transfer_ring_db.size = DB_REGISTER_SIZE;
	test_wdi3_ctx->tx_transfer_ring_db.base =
		dma_alloc_coherent(ipa3_ctx->pdev, DB_REGISTER_SIZE,
		&test_wdi3_ctx->tx_transfer_ring_db.phys_base, GFP_KERNEL);
	if (!test_wdi3_ctx->tx_transfer_ring_db.base) {
		IPA_UT_ERR("fail to alloc memory\n");
		ret = -ENOMEM;
		goto fail_tx_transfer_ring_db;
	}

	/* allocate tx event ring db */
	test_wdi3_ctx->tx_event_ring_db.size = DB_REGISTER_SIZE;
	test_wdi3_ctx->tx_event_ring_db.base =
		dma_alloc_coherent(ipa3_ctx->pdev, DB_REGISTER_SIZE,
		&test_wdi3_ctx->tx_event_ring_db.phys_base, GFP_KERNEL);
	if (!test_wdi3_ctx->tx_event_ring_db.base) {
		IPA_UT_ERR("fail to alloc memory\n");
		ret = -ENOMEM;
		goto fail_tx_event_ring_db;
	}

	/* allocate rx transfer ring db */
	test_wdi3_ctx->rx_transfer_ring_db.size = DB_REGISTER_SIZE;
	test_wdi3_ctx->rx_transfer_ring_db.base =
		dma_alloc_coherent(ipa3_ctx->pdev, DB_REGISTER_SIZE,
		&test_wdi3_ctx->rx_transfer_ring_db.phys_base, GFP_KERNEL);
	if (!test_wdi3_ctx->rx_transfer_ring_db.base) {
		IPA_UT_ERR("fail to alloc memory\n");
		ret = -ENOMEM;
		goto fail_rx_transfer_ring_db;
	}

	/* allocate rx event ring db */
	test_wdi3_ctx->rx_event_ring_db.size = DB_REGISTER_SIZE;
	test_wdi3_ctx->rx_event_ring_db.base =
		dma_alloc_coherent(ipa3_ctx->pdev, DB_REGISTER_SIZE,
		&test_wdi3_ctx->rx_event_ring_db.phys_base, GFP_KERNEL);
	if (!test_wdi3_ctx->rx_event_ring_db.base) {
		IPA_UT_ERR("fail to alloc memory\n");
		ret = -ENOMEM;
		goto fail_rx_event_ring_db;
	}

	return ret;

fail_rx_event_ring_db:
	ipa_test_wdi3_free_dma_buff(&test_wdi3_ctx->rx_transfer_ring_db);

fail_rx_transfer_ring_db:
	ipa_test_wdi3_free_dma_buff(&test_wdi3_ctx->tx_event_ring_db);

fail_tx_event_ring_db:
	ipa_test_wdi3_free_dma_buff(&test_wdi3_ctx->tx_transfer_ring_db);

fail_tx_transfer_ring_db:
fail_rx_bufs:
	for (j = 0; j <= num_rx_alloc_bufs; j++)
		ipa_test_wdi3_free_dma_buff(&test_wdi3_ctx->rx_bufs[j]);

fail_tx_bufs:
	ipa_test_wdi3_free_dma_buff(&test_wdi3_ctx->rx_event_ring_addr);

	for (j = 0; j <= num_tx_alloc_bufs; j++)
		ipa_test_wdi3_free_dma_buff(&test_wdi3_ctx->tx_bufs[j]);

fail_rx_event_ring:
	ipa_test_wdi3_free_dma_buff(&test_wdi3_ctx->rx_transfer_ring_addr);

fail_rx_transfer_ring:
	ipa_test_wdi3_free_dma_buff(&test_wdi3_ctx->tx_event_ring_addr);

fail_tx_event_ring:
	ipa_test_wdi3_free_dma_buff(&test_wdi3_ctx->tx_transfer_ring_addr);
	return ret;
}

static int ipa_test_wdi3_free_mmio(void)
{
	int i;

	if (!test_wdi3_ctx) {
		IPA_UT_ERR("test_wdi3_ctx is not initialized.\n");
		return -EFAULT;
	}

	ipa_test_wdi3_free_dma_buff(&test_wdi3_ctx->rx_event_ring_db);

	ipa_test_wdi3_free_dma_buff(&test_wdi3_ctx->rx_transfer_ring_db);

	ipa_test_wdi3_free_dma_buff(&test_wdi3_ctx->tx_event_ring_db);

	ipa_test_wdi3_free_dma_buff(&test_wdi3_ctx->tx_transfer_ring_db);

	ipa_test_wdi3_free_dma_buff(&test_wdi3_ctx->rx_event_ring_addr);

	ipa_test_wdi3_free_dma_buff(&test_wdi3_ctx->rx_transfer_ring_addr);

	ipa_test_wdi3_free_dma_buff(&test_wdi3_ctx->tx_event_ring_addr);

	ipa_test_wdi3_free_dma_buff(&test_wdi3_ctx->tx_transfer_ring_addr);

	for (i = 0; i < NUM_RX_BUFS; i++)
		ipa_test_wdi3_free_dma_buff(&test_wdi3_ctx->rx_bufs[i]);

	for (i = 0; i < NUM_TX_BUFS; i++)
		ipa_test_wdi3_free_dma_buff(&test_wdi3_ctx->tx_bufs[i]);

	return 0;
}

static int ipa_test_wdi3_suite_setup(void **priv)
{
	int ret = 0;
	struct ipa_wdi_init_in_params in;
	struct ipa_wdi_init_out_params out;

	IPA_UT_DBG("Start WDI3 Setup\n");

	/* init ipa wdi ctx */
	in.wdi_notify = NULL;
	in.notify = NULL;
	in.priv = NULL;
	in.wdi_version = IPA_WDI_3;
	ipa_wdi_init(&in, &out);


	if (!ipa3_ctx) {
		IPA_UT_ERR("No IPA ctx\n");
		return -EINVAL;
	}

	test_wdi3_ctx = kzalloc(sizeof(struct ipa_test_wdi3_context),
		GFP_KERNEL);
	if (!test_wdi3_ctx) {
		IPA_UT_ERR("failed to allocate ctx\n");
		return -ENOMEM;
	}

	ret = ipa_test_wdi3_alloc_mmio();
	if (ret) {
		IPA_UT_ERR("failed to alloc mmio\n");
		goto fail_alloc_mmio;
	}

	*priv = test_wdi3_ctx;
	return 0;

fail_alloc_mmio:
	kfree(test_wdi3_ctx);
	test_wdi3_ctx = NULL;
	return ret;
}

static int ipa_test_wdi3_suite_teardown(void *priv)
{
	if (!test_wdi3_ctx)
		return  0;

	ipa_test_wdi3_free_mmio();
	kfree(test_wdi3_ctx);
	test_wdi3_ctx = NULL;

	return 0;
}

static int ipa_wdi3_setup_pipes(void)
{
	struct ipa_wdi_conn_in_params *in_param;
	struct ipa_wdi_conn_out_params *out_param;
	struct tx_transfer_ring_ele *tx_transfer, *tx_transfer_base;
	struct rx_transfer_ring_ele *rx_transfer;
	void __iomem *rx_uc_db;
	void __iomem *tx_uc_db;
	int i, index;

	if (!test_wdi3_ctx) {
		IPA_UT_ERR("context is empty.\n");
		return -EFAULT;
	}

	in_param = kzalloc(sizeof(struct ipa_wdi_conn_in_params),
		GFP_KERNEL);
	if (!in_param) {
		IPA_UT_ERR("failed to allocate in_param\n");
		return -ENOMEM;
	}

	out_param = kzalloc(sizeof(struct ipa_wdi_conn_out_params),
		GFP_KERNEL);
	if (!out_param) {
		IPA_UT_ERR("failed to allocate out_param\n");
		kfree(in_param);
		return -ENOMEM;
	}

	memset(in_param, 0, sizeof(struct ipa_wdi_conn_in_params));
	memset(out_param, 0, sizeof(struct ipa_wdi_conn_out_params));

	/* setup tx parameters */
	in_param->is_smmu_enabled = false;
	in_param->u_tx.tx.client = IPA_CLIENT_WLAN2_CONS;
	in_param->u_tx.tx.transfer_ring_base_pa =
		test_wdi3_ctx->tx_transfer_ring_addr.phys_base;
	in_param->u_tx.tx.transfer_ring_size =
		test_wdi3_ctx->tx_transfer_ring_addr.size;
	in_param->u_tx.tx.transfer_ring_doorbell_pa =
		test_wdi3_ctx->tx_transfer_ring_db.phys_base;

	in_param->notify = NULL;
	in_param->u_tx.tx.event_ring_base_pa =
		test_wdi3_ctx->tx_event_ring_addr.phys_base;
	in_param->u_tx.tx.event_ring_size =
		test_wdi3_ctx->tx_event_ring_addr.size;
	in_param->u_tx.tx.event_ring_doorbell_pa =
		test_wdi3_ctx->tx_event_ring_db.phys_base;
	IPA_UT_DBG("tx_event_ring_db.phys_base %llu\n",
		test_wdi3_ctx->tx_event_ring_db.phys_base);
	IPA_UT_DBG("tx_event_ring_db.base %pK\n",
		test_wdi3_ctx->tx_event_ring_db.base);
	IPA_UT_DBG("tx_event_ring.phys_base %llu\n",
		test_wdi3_ctx->tx_event_ring_addr.phys_base);
	IPA_UT_DBG("tx_event_ring.base %pK\n",
		test_wdi3_ctx->tx_event_ring_addr.base);

	in_param->u_tx.tx.num_pkt_buffers = NUM_TX_BUFS;

	/* setup rx parameters */
	in_param->u_rx.rx.client = IPA_CLIENT_WLAN2_PROD;
	in_param->u_rx.rx.transfer_ring_base_pa =
		test_wdi3_ctx->rx_transfer_ring_addr.phys_base;
	in_param->u_rx.rx.transfer_ring_size =
		test_wdi3_ctx->rx_transfer_ring_addr.size;
	in_param->u_rx.rx.transfer_ring_doorbell_pa =
		test_wdi3_ctx->rx_transfer_ring_db.phys_base;
	in_param->u_rx.rx.pkt_offset = PACKET_HEADER_SIZE;


	in_param->u_rx.rx.event_ring_base_pa =
		test_wdi3_ctx->rx_event_ring_addr.phys_base;
	in_param->u_rx.rx.event_ring_size =
		test_wdi3_ctx->rx_event_ring_addr.size;
	in_param->u_rx.rx.event_ring_doorbell_pa =
		test_wdi3_ctx->rx_event_ring_db.phys_base;

	IPA_UT_DBG("rx_event_ring_db.phys_base %llu\n",
		in_param->u_rx.rx.event_ring_doorbell_pa);
	IPA_UT_DBG("rx_event_ring_db.base %pK\n",
		test_wdi3_ctx->rx_event_ring_addr.base);

	in_param->u_rx.rx.num_pkt_buffers = NUM_RX_BUFS;
	if (ipa_wdi_conn_pipes(in_param, out_param)) {
		IPA_UT_ERR("fail to conn wdi3 pipes.\n");
		kfree(in_param);
		kfree(out_param);
		return -EFAULT;
	}
	if (ipa_wdi_enable_pipes()) {
		IPA_UT_ERR("fail to enable wdi3 pipes.\n");
		ipa_wdi_disconn_pipes();
		kfree(in_param);
		kfree(out_param);
		return -EFAULT;
	}
	test_wdi3_ctx->tx_uc_db_pa = out_param->tx_uc_db_pa;
	test_wdi3_ctx->rx_uc_db_pa = out_param->rx_uc_db_pa;
	IPA_UT_DBG("tx_uc_db_pa %llu, rx_uc_db_pa %llu.\n",
		test_wdi3_ctx->tx_uc_db_pa, test_wdi3_ctx->rx_uc_db_pa);

	rx_uc_db = ioremap(test_wdi3_ctx->rx_uc_db_pa, DB_REGISTER_SIZE);
	tx_uc_db = ioremap(test_wdi3_ctx->tx_uc_db_pa, DB_REGISTER_SIZE);

	/* setup db registers */
	*(u32 *)test_wdi3_ctx->rx_transfer_ring_db.base = rx_uc_db_local;
	*(u32 *)test_wdi3_ctx->rx_event_ring_db.base = 0;

	*(u32 *)test_wdi3_ctx->tx_transfer_ring_db.base = tx_uc_db_local;
	*(u32 *)test_wdi3_ctx->tx_event_ring_db.base = 0;

	rx_transfer = (struct rx_transfer_ring_ele *)
		test_wdi3_ctx->rx_transfer_ring_addr.base;
	for (i = 0; i < NUM_TX_BUFS; i++) {
		rx_transfer->buf_or_link_desc_addr_info.buffer_addr_low =
			(u64)test_wdi3_ctx->rx_bufs[i].phys_base & 0xFFFFFFFF;
		rx_transfer->buf_or_link_desc_addr_info.buffer_addr_high =
			((u64)test_wdi3_ctx->rx_bufs[i].phys_base >> 32)
			& 0xFFFFFFFF;
		rx_transfer++;
	}

	tx_transfer_base = (struct tx_transfer_ring_ele *)
		test_wdi3_ctx->tx_transfer_ring_addr.base;
	index = tx_uc_db_local;
	for (i = 0; i < NUM_TX_BUFS; i++) {
		tx_transfer = tx_transfer_base + index;
		tx_transfer->buf_or_link_desc_addr_info.buffer_addr_low =
			(u64)test_wdi3_ctx->tx_bufs[i].phys_base & 0xFFFFFFFF;
		tx_transfer->buf_or_link_desc_addr_info.buffer_addr_high =
			((u64)test_wdi3_ctx->tx_bufs[i].phys_base >> 32)
			& 0xFFFFFFFF;
		index = (index + 1) % NUM_TX_TR_ELE;
	}
	ipa_test_wdi3_advance_uc_db(&tx_uc_db_local, NUM_TX_BUFS,
		sizeof(struct tx_transfer_ring_ele)/4,
		test_wdi3_ctx->tx_transfer_ring_addr.size);
	iowrite32(tx_uc_db_local, tx_uc_db);
	kfree(in_param);
	kfree(out_param);
	return 0;
}

static int ipa_wdi3_teardown_pipes(void)
{
	ipa_wdi_disable_pipes();
	ipa_wdi_disconn_pipes();
	rx_bf_idx = 0;
	tx_bf_idx = 0;
	rx_uc_db_local = 0;
	tx_uc_db_local = 0;
	return 0;
}

static int ipa_wdi3_send_one_packet(void)
{
	void __iomem *rx_uc_db;
	void __iomem *tx_uc_db;
	u32 *tx_event_ring_db, *rx_transfer_ring_db, *rx_event_ring_db;
	u32 orig_tx_event_ring_db;
	u32 orig_rx_event_ring_db;
	u32 orig_tx_trans_ring_db;
	u32 *packet;
	u32 *packet_recv = NULL;
	struct rx_transfer_ring_ele *rx_transfer;
	struct rx_event_ring_ele *rx_event;
	struct tx_event_ring_ele *tx_event;
	struct tx_transfer_ring_ele *tx_transfer;
	struct buffer_addr_info rx_buf;
	dma_addr_t recv_packet_addr;
	int loop_cnt, i, num_words;
	int idx;

	/* populate packet content */
	rx_uc_db = ioremap(test_wdi3_ctx->rx_uc_db_pa, DB_REGISTER_SIZE);
	num_words = sizeof(struct rx_transfer_ring_ele) / 4;
	idx = rx_uc_db_local / num_words;
	packet = (u32 *)test_wdi3_ctx->rx_bufs[rx_bf_idx].base +
		PACKET_HEADER_SIZE/4;
	*packet = PACKET_CONTENT;
	IPA_UT_DBG("local rx uc db: %u, rx buffer index %d\n",
		rx_uc_db_local, rx_bf_idx);
	rx_bf_idx = (rx_bf_idx  + 1) % NUM_RX_BUFS;
	/* update rx_transfer_ring_ele */
	rx_transfer = (struct rx_transfer_ring_ele *)
		(test_wdi3_ctx->rx_transfer_ring_addr.base) +
		idx;

	ipa_test_wdi3_advance_uc_db(&rx_uc_db_local, 1,
		sizeof(struct rx_transfer_ring_ele)/4,
		test_wdi3_ctx->rx_transfer_ring_addr.size);
	rx_transfer->rx_msdu_desc_info_details.msdu_length =
		ETH_PACKET_SIZE + PACKET_HEADER_SIZE;

	rx_buf.buffer_addr_low =
		rx_transfer->buf_or_link_desc_addr_info.buffer_addr_low;
	rx_buf.buffer_addr_high =
		rx_transfer->buf_or_link_desc_addr_info.buffer_addr_high;

	tx_event_ring_db = (u32 *)test_wdi3_ctx->tx_event_ring_db.base;
	orig_tx_event_ring_db = *tx_event_ring_db;
	IPA_UT_DBG("original tx event ring db: %u\n",
		orig_tx_event_ring_db);

	rx_event_ring_db = (u32 *)test_wdi3_ctx->rx_event_ring_db.base;
	orig_rx_event_ring_db = *rx_event_ring_db;
	IPA_UT_DBG("original rx event ring db: %u\n",
		orig_rx_event_ring_db);

	rx_transfer_ring_db
		= (u32 *)test_wdi3_ctx->rx_transfer_ring_db.base;
	orig_tx_trans_ring_db = *rx_transfer_ring_db;
	IPA_UT_DBG("original rx transfer ring db: %u\n",
		*rx_transfer_ring_db);

	/* ring uc db */
	iowrite32(rx_uc_db_local, rx_uc_db);
	IPA_UT_DBG("rx db local: %u\n", rx_uc_db_local);

	loop_cnt = 0;
	while (orig_tx_event_ring_db == *tx_event_ring_db ||
		*rx_event_ring_db == orig_rx_event_ring_db) {
		loop_cnt++;
		IPA_UT_DBG("loop count: %d tx\n", loop_cnt);
		IPA_UT_DBG("orig_tx_event_ring_db: %u tx_event_ring_db: %u\n",
			orig_tx_event_ring_db, *tx_event_ring_db);
		IPA_UT_DBG("rx_transfer_ring_db: %u rx db local: %u\n",
			*rx_transfer_ring_db, rx_uc_db_local);
		IPA_UT_DBG("orig_rx_event_ring_db: %u rx_event_ring_db %u\n",
			orig_rx_event_ring_db, *rx_event_ring_db);
		if (loop_cnt == 1000) {
			IPA_UT_ERR("transfer timeout!\n");
			gsi_wdi3_dump_register(1);
			gsi_wdi3_dump_register(9);
			BUG();
			return -EFAULT;
		}
		usleep_range(1000, 1001);
	}
	IPA_UT_DBG("rx_transfer_ring_db: %u\n", *rx_transfer_ring_db);
	IPA_UT_DBG("tx_event_ring_db: %u\n", *tx_event_ring_db);
	num_words = sizeof(struct rx_event_ring_ele)/4;
	rx_event = (struct rx_event_ring_ele *)
		(test_wdi3_ctx->rx_event_ring_addr.base) +
		(*rx_event_ring_db/num_words - 1 + NUM_RX_ER_ELE) %
		NUM_RX_ER_ELE;
	IPA_UT_DBG("rx_event offset: %u\n",
		(*rx_event_ring_db/num_words - 1 + NUM_RX_ER_ELE) %
		NUM_RX_ER_ELE);
	IPA_UT_DBG("rx_event va: %pK\n", rx_event);
	IPA_UT_DBG("rx event low: %u rx event high: %u\n",
		rx_event->buf_or_link_desc_addr_info.buffer_addr_low,
		rx_event->buf_or_link_desc_addr_info.buffer_addr_high);
	IPA_UT_DBG("rx buf low: %u rx buf high: %u\n",
		rx_buf.buffer_addr_low, rx_buf.buffer_addr_high);
	if (rx_event->buf_or_link_desc_addr_info.buffer_addr_low !=
		rx_buf.buffer_addr_low ||
		rx_event->buf_or_link_desc_addr_info.buffer_addr_high !=
		rx_buf.buffer_addr_high) {
		IPA_UT_ERR("rx event ring buf addr doesn't match.\n");
		BUG();
		return -EFAULT;
	}

	num_words = sizeof(struct tx_event_ring_ele)/4;
	tx_event = (struct tx_event_ring_ele *)
		test_wdi3_ctx->tx_event_ring_addr.base +
		(*tx_event_ring_db/num_words - 1 + NUM_TX_ER_ELE) %
		NUM_TX_ER_ELE;
	IPA_UT_DBG("tx_event va: %pK\n", tx_event);
	IPA_UT_DBG("tx event offset: %u\n",
		(*tx_event_ring_db/num_words - 1 + NUM_TX_ER_ELE) %
		NUM_TX_ER_ELE);
	IPA_UT_DBG("recv addr low: %u recv_addr high: %u\n",
		tx_event->buf_or_link_desc_addr_info.buffer_addr_low,
		tx_event->buf_or_link_desc_addr_info.buffer_addr_high);
	recv_packet_addr =
		((u64)tx_event->buf_or_link_desc_addr_info.buffer_addr_high
		 << 32) |
		(u64)tx_event->buf_or_link_desc_addr_info.buffer_addr_low;
	IPA_UT_DBG("high: %llu low: %llu all: %llu\n",
		(u64)tx_event->buf_or_link_desc_addr_info.buffer_addr_high
			   << 32,
		(u64)tx_event->buf_or_link_desc_addr_info.buffer_addr_low,
		recv_packet_addr);
	for (i = 0; i < NUM_TX_BUFS; i++)
		if (recv_packet_addr == test_wdi3_ctx->tx_bufs[i].phys_base) {
			IPA_UT_DBG("found buf at position %d\n", i);
			packet_recv = (u32 *)test_wdi3_ctx->tx_bufs[i].base;
		}
	IPA_UT_DBG("packet_recv addr: %pK\n", packet_recv);
	if (*packet_recv != PACKET_CONTENT) {
		IPA_UT_ERR("recv packet doesn't match.\n");
		IPA_UT_ERR("packet: %d packet_recv: %d\n", PACKET_CONTENT,
			*packet_recv);
		return -EFAULT;
	}
	IPA_UT_INFO("recv packet matches!! Recycling the buffer ...\n");
	/* recycle buffer */
	tx_uc_db = ioremap(test_wdi3_ctx->tx_uc_db_pa, DB_REGISTER_SIZE);
	num_words = sizeof(struct tx_transfer_ring_ele) / 4;
	idx = tx_uc_db_local / num_words;
	IPA_UT_DBG("tx_db_local: %u idx %d\n", tx_uc_db_local, idx);
	tx_transfer = (struct tx_transfer_ring_ele *)
		test_wdi3_ctx->tx_transfer_ring_addr.base + idx;
	tx_transfer->buf_or_link_desc_addr_info.buffer_addr_low =
		tx_event->buf_or_link_desc_addr_info.buffer_addr_low;
	tx_transfer->buf_or_link_desc_addr_info.buffer_addr_high =
		tx_event->buf_or_link_desc_addr_info.buffer_addr_high;
	ipa_test_wdi3_advance_uc_db(&tx_uc_db_local, 1,
		sizeof(struct tx_transfer_ring_ele)/4,
		test_wdi3_ctx->tx_transfer_ring_addr.size);
	iowrite32(tx_uc_db_local, tx_uc_db);
	tx_bf_idx = (tx_bf_idx + 1) % NUM_TX_BUFS;
	return 0;
}

static int ipa_wdi3_test_reg_intf(void)
{
	struct ipa_wdi_reg_intf_in_params in;
	char netdev_name[IPA_RESOURCE_NAME_MAX] = {0};
	u8 hdr_content = 1;

	memset(&in, 0, sizeof(in));
	snprintf(netdev_name, sizeof(netdev_name), "wdi3_test");
	in.netdev_name = netdev_name;
	in.is_meta_data_valid = 0;
	in.hdr_info[0].hdr = &hdr_content;
	in.hdr_info[0].hdr_len = 1;
	in.hdr_info[0].dst_mac_addr_offset = 0;
	in.hdr_info[0].hdr_type = IPA_HDR_L2_ETHERNET_II;
	in.hdr_info[1].hdr = &hdr_content;
	in.hdr_info[1].hdr_len = 1;
	in.hdr_info[1].dst_mac_addr_offset = 0;
	in.hdr_info[1].hdr_type = IPA_HDR_L2_ETHERNET_II;

	return ipa_wdi_reg_intf(&in);
}

static int ipa_wdi3_test_dereg_intf(void)
{
	char netdev_name[IPA_RESOURCE_NAME_MAX] = {0};

	snprintf(netdev_name, sizeof(netdev_name), "wdi3_test");
	IPA_UT_INFO("netdev name: %s strlen: %lu\n", netdev_name,
				strlen(netdev_name));

	return ipa_wdi_dereg_intf(netdev_name);
}

static int ipa_wdi3_test_single_transfer(void *priv)
{
	struct ipa_ep_cfg ep_cfg = { {0} };

	if (ipa_wdi3_test_reg_intf()) {
		IPA_UT_ERR("fail to register intf.\n");
		return -EFAULT;
	}

	if (ipa_wdi3_setup_pipes()) {
		IPA_UT_ERR("fail to setup wdi3 pipes.\n");
		return -EFAULT;
	}

	/* configure WLAN RX EP in DMA mode */
	ep_cfg.mode.mode = IPA_DMA;
	ep_cfg.mode.dst = IPA_CLIENT_WLAN2_CONS;

	ep_cfg.seq.set_dynamic = true;

	ipa_cfg_ep(ipa_get_ep_mapping(IPA_CLIENT_WLAN2_PROD), &ep_cfg);

	if (ipa_wdi3_send_one_packet()) {
		IPA_UT_ERR("fail to transfer packet.\n");
		ipa_wdi3_teardown_pipes();
		return -EFAULT;
	}

	if (ipa_wdi3_teardown_pipes()) {
		IPA_UT_ERR("fail to tear down pipes.\n");
		return -EFAULT;
	}

	IPA_UT_INFO("pipes were torn down!\n");

	if (ipa_wdi3_test_dereg_intf()) {
		IPA_UT_ERR("fail to deregister interface.\n");
		return -EFAULT;
	}

	return 0;
}

static int ipa_wdi3_send_multi_packet(void)
{
	void __iomem *rx_uc_db;
	void __iomem *tx_uc_db;
	u32 *tx_event_ring_db, *rx_transfer_ring_db, *rx_event_ring_db;
	u32 orig_tx_event_ring_db;
	u32 orig_rx_event_ring_db;
	u32 *packet;
	u32 *packet_recv = NULL;
	struct rx_transfer_ring_ele *rx_transfer;
	struct rx_event_ring_ele *rx_event;
	struct tx_event_ring_ele *tx_event;
	struct tx_transfer_ring_ele *tx_transfer;
	struct buffer_addr_info rx_buf;
	dma_addr_t recv_packet_addr;
	int loop_cnt, i, num_words;
	int idx;

	/* populate packet content */
	num_words = sizeof(struct rx_transfer_ring_ele) / 4;
	rx_uc_db = ioremap(test_wdi3_ctx->rx_uc_db_pa, DB_REGISTER_SIZE);
	for (i = 0; i < NUM_MULTI_PKT; i++) {
		idx = rx_uc_db_local / num_words;
		packet = (u32 *)test_wdi3_ctx->rx_bufs[rx_bf_idx].base
			+ PACKET_HEADER_SIZE / 4;
		*packet = multi_pkt_array[i];
		IPA_UT_DBG("rx_db_local: %u rx_bf_idx: %d\n",
			rx_uc_db_local, rx_bf_idx);
		rx_bf_idx = (rx_bf_idx  + 1) % NUM_RX_BUFS;
		/* update rx_transfer_ring_ele */
		rx_transfer = (struct rx_transfer_ring_ele *)
			test_wdi3_ctx->rx_transfer_ring_addr.base + idx;
		ipa_test_wdi3_advance_uc_db(&rx_uc_db_local, 1,
			sizeof(struct rx_transfer_ring_ele)/4,
			test_wdi3_ctx->rx_transfer_ring_addr.size);
		rx_transfer->rx_msdu_desc_info_details.msdu_length =
			ETH_PACKET_SIZE + PACKET_HEADER_SIZE;
		rx_buf.buffer_addr_low =
		rx_transfer->buf_or_link_desc_addr_info.buffer_addr_low;
		rx_buf.buffer_addr_high =
		rx_transfer->buf_or_link_desc_addr_info.buffer_addr_high;
	}

	tx_event_ring_db = (u32 *)test_wdi3_ctx->tx_event_ring_db.base;
	orig_tx_event_ring_db = *tx_event_ring_db;
	IPA_UT_DBG("original tx event ring db: %u\n", orig_tx_event_ring_db);

	rx_event_ring_db = (u32 *)test_wdi3_ctx->rx_event_ring_db.base;
	orig_rx_event_ring_db = *rx_event_ring_db;
	IPA_UT_DBG("original rx event ring db: %u\n", orig_rx_event_ring_db);

	rx_transfer_ring_db = (u32 *)test_wdi3_ctx->rx_transfer_ring_db.base;
	IPA_UT_DBG("original rx transfer ring db: %u\n", *rx_transfer_ring_db);

	/* ring uc db */
	iowrite32(rx_uc_db_local, rx_uc_db);
	IPA_UT_DBG("rx db local: %u\n", rx_uc_db_local);

	loop_cnt = 0;
	while (orig_tx_event_ring_db == *tx_event_ring_db ||
		*rx_transfer_ring_db != rx_uc_db_local ||
		orig_rx_event_ring_db == *rx_event_ring_db) {
		loop_cnt++;
		IPA_UT_DBG("loop count: %d tx\n", loop_cnt);
		IPA_UT_DBG("orig_tx_event_ring_db: %u tx_event_ring_db: %u\n",
			orig_tx_event_ring_db, *tx_event_ring_db);
		IPA_UT_DBG("rx_transfer_ring_db: %u rx db local: %u\n",
			*rx_transfer_ring_db, rx_uc_db_local);
		IPA_UT_DBG("orig_rx_event_ring_db: %u rx_event_ring_db %u\n",
			orig_rx_event_ring_db, *rx_event_ring_db);
		if (loop_cnt == 1000) {
			IPA_UT_ERR("transfer timeout!\n");
			BUG();
			return -EFAULT;
		}
		usleep_range(1000, 1001);
	}

	IPA_UT_DBG("rx_transfer_ring_db: %u\n", *rx_transfer_ring_db);
	IPA_UT_DBG("tx_event_ring_db: %u\n", *tx_event_ring_db);
	num_words = sizeof(struct rx_event_ring_ele)/4;
	rx_event = (struct rx_event_ring_ele *)
		test_wdi3_ctx->rx_event_ring_addr.base +
		(*rx_event_ring_db/num_words - 1 + NUM_RX_ER_ELE) %
		NUM_RX_ER_ELE;
	IPA_UT_DBG("rx_event va: %pK\n", rx_event);

	IPA_UT_DBG("rx event low: %u rx event high: %u\n",
		rx_event->buf_or_link_desc_addr_info.buffer_addr_low,
		rx_event->buf_or_link_desc_addr_info.buffer_addr_high);
	IPA_UT_DBG("rx buf low: %u rx buf high: %u\n",
		rx_buf.buffer_addr_low, rx_buf.buffer_addr_high);

	if (rx_event->buf_or_link_desc_addr_info.buffer_addr_low !=
		rx_buf.buffer_addr_low ||
		rx_event->buf_or_link_desc_addr_info.buffer_addr_high !=
		rx_buf.buffer_addr_high) {
		IPA_UT_ERR("rx event ring buf addr doesn't match.\n");
		return -EFAULT;
	}
	num_words = sizeof(struct tx_event_ring_ele)/4;
	tx_event = (struct tx_event_ring_ele *)
		test_wdi3_ctx->tx_event_ring_addr.base +
		(*tx_event_ring_db/num_words - NUM_MULTI_PKT + NUM_TX_ER_ELE) %
		NUM_TX_ER_ELE;
	IPA_UT_DBG("tx_event va: %pK\n", tx_event);
	IPA_UT_DBG("recv addr low: %u recv_addr high: %u\n",
		tx_event->buf_or_link_desc_addr_info.buffer_addr_low,
		tx_event->buf_or_link_desc_addr_info.buffer_addr_high);
	recv_packet_addr =
		((u64)tx_event->buf_or_link_desc_addr_info.buffer_addr_high
		 << 32) |
		(u64)tx_event->buf_or_link_desc_addr_info.buffer_addr_low;
	IPA_UT_DBG("high: %llu low: %llu all: %llu\n",
		(u64)tx_event->buf_or_link_desc_addr_info.buffer_addr_high
			   << 32,
		(u64)tx_event->buf_or_link_desc_addr_info.buffer_addr_low,
		recv_packet_addr);
	for (i = 0; i < NUM_TX_BUFS; i++)
		if (recv_packet_addr == test_wdi3_ctx->tx_bufs[i].phys_base) {
			IPA_UT_INFO("found buf at position %d\n", i);
			packet_recv = (u32 *)test_wdi3_ctx->tx_bufs[i].base;
		}

	if (*packet_recv != multi_pkt_array[0]) {
		IPA_UT_ERR("recv packet doesn't match.\n");
		IPA_UT_ERR("packet: %d packet_recv: %d\n",
			multi_pkt_array[0], *packet_recv);
		return -EFAULT;
	}

	IPA_UT_INFO("recv packet matches.\n");

	/* recycle buffer */
	tx_uc_db = ioremap(test_wdi3_ctx->tx_uc_db_pa, DB_REGISTER_SIZE);
	num_words = sizeof(struct tx_transfer_ring_ele) / 4;

	for (i = 0; i < NUM_MULTI_PKT; i++) {
		idx = tx_uc_db_local / num_words;
		IPA_UT_DBG("tx_db_local: %u idx %d\n", tx_uc_db_local, idx);
		tx_event = (struct tx_event_ring_ele *)
			test_wdi3_ctx->tx_event_ring_addr.base +
			(*tx_event_ring_db/num_words - NUM_MULTI_PKT
			+ i + NUM_TX_ER_ELE) % NUM_TX_ER_ELE;
		tx_transfer = (struct tx_transfer_ring_ele *)
			test_wdi3_ctx->tx_transfer_ring_addr.base + idx;
		tx_transfer->buf_or_link_desc_addr_info.buffer_addr_low =
			tx_event->buf_or_link_desc_addr_info.buffer_addr_low;
		tx_transfer->buf_or_link_desc_addr_info.buffer_addr_high =
			tx_event->buf_or_link_desc_addr_info.buffer_addr_high;
		ipa_test_wdi3_advance_uc_db(&tx_uc_db_local, 1,
			sizeof(struct tx_transfer_ring_ele)/4,
			test_wdi3_ctx->tx_transfer_ring_addr.size);
	}
	iowrite32(tx_uc_db_local, tx_uc_db);
	tx_bf_idx = (tx_bf_idx + NUM_MULTI_PKT) % NUM_TX_BUFS;
	return 0;
}

static int ipa_wdi3_test_multi_transfer(void *priv)
{
	struct ipa_ep_cfg ep_cfg = { {0} };

	if (ipa_wdi3_test_reg_intf()) {
		IPA_UT_ERR("fail to register intf.\n");
		return -EFAULT;
	}

	if (ipa_wdi3_setup_pipes()) {
		IPA_UT_ERR("fail to setup wdi3 pipes.\n");
		return -EFAULT;
	}

	/* configure WLAN RX EP in DMA mode */
	ep_cfg.mode.mode = IPA_DMA;
	ep_cfg.mode.dst = IPA_CLIENT_WLAN2_CONS;

	ep_cfg.seq.set_dynamic = true;

	ipa_cfg_ep(ipa_get_ep_mapping(IPA_CLIENT_WLAN2_PROD), &ep_cfg);

	if (ipa_wdi3_send_multi_packet()) {
		IPA_UT_ERR("fail to transfer packet.\n");
		ipa_wdi3_teardown_pipes();
		return -EFAULT;
	}

	if (ipa_wdi3_teardown_pipes()) {
		IPA_UT_ERR("fail to tear down pipes.\n");
		return -EFAULT;
	}

	IPA_UT_INFO("pipes were torn down!\n");

	if (ipa_wdi3_test_dereg_intf()) {
		IPA_UT_ERR("fail to deregister interface.\n");
		return -EFAULT;
	}

	return 0;
}

static int ipa_wdi3_test_multi_transfer2(void *priv)
{
	struct ipa_ep_cfg ep_cfg = { {0} };
	int i;

	if (ipa_wdi3_test_reg_intf()) {
		IPA_UT_ERR("fail to register intf.\n");
		return -EFAULT;
	}

	if (ipa_wdi3_setup_pipes()) {
		IPA_UT_ERR("fail to setup wdi3 pipes.\n");
		return -EFAULT;
	}

	/* configure WLAN RX EP in DMA mode */
	ep_cfg.mode.mode = IPA_DMA;
	ep_cfg.mode.dst = IPA_CLIENT_WLAN2_CONS;

	ep_cfg.seq.set_dynamic = true;

	ipa_cfg_ep(ipa_get_ep_mapping(IPA_CLIENT_WLAN2_PROD), &ep_cfg);

	IPA_UT_DBG("-----start transfer 32 pkt----\n");
	for (i = 0; i < 32; i++) {
		IPA_UT_DBG("--transferring num #%d pkt--\n", i + 1);
		if (ipa_wdi3_send_one_packet()) {
			IPA_UT_ERR("fail to transfer packet.\n");
			ipa_wdi3_teardown_pipes();
			return -EFAULT;
		}
	}

	if (ipa_wdi3_teardown_pipes()) {
		IPA_UT_ERR("fail to tear down pipes.\n");
		return -EFAULT;
	}

	IPA_UT_ERR("pipes were torn down!\n");

	if (ipa_wdi3_test_dereg_intf()) {
		IPA_UT_ERR("fail to deregister interface.\n");
		return -EFAULT;
	}

	return 0;
}

static int ipa_wdi3_test_multi_transfer3(void *priv)
{
	struct ipa_ep_cfg ep_cfg = { {0} };
	int i;

	if (ipa_wdi3_test_reg_intf()) {
		IPA_UT_ERR("fail to register intf.\n");
		return -EFAULT;
	}

	if (ipa_wdi3_setup_pipes()) {
		IPA_UT_ERR("fail to setup wdi3 pipes.\n");
		return -EFAULT;
	}

	/* configure WLAN RX EP in DMA mode */
	ep_cfg.mode.mode = IPA_DMA;
	ep_cfg.mode.dst = IPA_CLIENT_WLAN2_CONS;

	ep_cfg.seq.set_dynamic = true;

	ipa_cfg_ep(ipa_get_ep_mapping(IPA_CLIENT_WLAN2_PROD), &ep_cfg);

	IPA_UT_DBG("-----start transfer 256 pkt----\n");
	for (i = 0; i < 32; i++) {
		IPA_UT_DBG("--transferring num # %d to num # %d pkt--\n",
			(i + 1) * 8, (i + 2) * 8 - 1);
		if (ipa_wdi3_send_multi_packet()) {
			IPA_UT_ERR("fail to transfer packet.\n");
			ipa_wdi3_teardown_pipes();
			return -EFAULT;
		}
	}

	if (ipa_wdi3_teardown_pipes()) {
		IPA_UT_ERR("fail to tear down pipes.\n");
		return -EFAULT;
	}

	IPA_UT_ERR("pipes were torn down!\n");

	if (ipa_wdi3_test_dereg_intf()) {
		IPA_UT_ERR("fail to deregister interface.\n");
		return -EFAULT;
	}

	return 0;
}


/* Suite definition block */
IPA_UT_DEFINE_SUITE_START(wdi3, "WDI3 tests",
	ipa_test_wdi3_suite_setup, ipa_test_wdi3_suite_teardown)
{
	IPA_UT_ADD_TEST(single_transfer,
		"single data transfer",
		ipa_wdi3_test_single_transfer,
		true, IPA_HW_v3_0, IPA_HW_MAX),

	IPA_UT_ADD_TEST(multi_transfer,
		"multiple data transfer",
		ipa_wdi3_test_multi_transfer,
		true, IPA_HW_v3_0, IPA_HW_MAX),

	IPA_UT_ADD_TEST(multi_transfer2,
		"multiple data transfer with data wrap around",
		ipa_wdi3_test_multi_transfer2,
		true, IPA_HW_v3_0, IPA_HW_MAX),

	IPA_UT_ADD_TEST(multi_transfer3,
		"multiple data transfer with data wrap around2",
		ipa_wdi3_test_multi_transfer3,
		true, IPA_HW_v3_0, IPA_HW_MAX)
} IPA_UT_DEFINE_SUITE_END(wdi3);


