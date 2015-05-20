/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2013 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2014 Intel Mobile Communications GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * BSD LICENSE
 *
 * Copyright(c) 2012 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2014 Intel Mobile Communications GmbH
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/

#include <linux/kernel.h>
#include <linux/gfp.h>
#include <linux/log2.h>
#include <linux/mmc/sdio_func.h>
#include "sdio_internal.h"
#include "sdio_tx.h"
#include "sdio_tx_policy.h"
#include "iwl-prph.h"
#include "iwl-fh.h"
#include "iwl-csr.h"
#include "iwl-io.h"
#include "iwl-scd.h"

#include "mvm/fw-api-tx.h"

/* align to DW to the lower address */
#define ALIGN_DW_LOW(x) ((x) & (~3))

#define PAD_DTU_LEN (sizeof(struct iwl_sdio_tx_dtu_hdr) + \
		     sizeof(struct iwl_sdio_adma_desc))

static int iwl_sdio_txq_set_ratid_map(struct iwl_trans *trans, u16 ra_tid,
				      u16 txq_id)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	u32 tbl_dw_addr;
	u32 tbl_dw;
	u16 scd_q2ratid;

	scd_q2ratid = ra_tid & SCD_QUEUE_RA_TID_MAP_RATID_MSK;

	tbl_dw_addr = trans_sdio->scd_base_addr +
			SCD_TRANS_TBL_OFFSET_QUEUE(txq_id);

	tbl_dw = iwl_trans_read_mem32(trans, tbl_dw_addr);

	if (txq_id & 0x1)
		tbl_dw = (scd_q2ratid << 16) | (tbl_dw & 0x0000FFFF);
	else
		tbl_dw = scd_q2ratid | (tbl_dw & 0xFFFF0000);

	iwl_trans_write_mem32(trans, tbl_dw_addr, tbl_dw);

	return 0;
}

static void iwl_sdio_txq_disable_wk(struct work_struct *wk)
{
	struct iwl_trans_sdio *trans_sdio =
		container_of(wk, struct iwl_trans_sdio,
			     iwl_sdio_disable_txq_wk);
	struct iwl_trans *trans = trans_sdio->trans;
	static const u32 zero_data[4] = {};
	int txq_id;

	for (txq_id = 0; txq_id < ARRAY_SIZE(trans_sdio->txq); txq_id++) {
		if (!trans_sdio->txq[txq_id].disabling)
			continue;

		iwl_scd_txq_set_inactive(trans, txq_id);
		iwl_trans_write_mem(trans, trans_sdio->scd_base_addr +
				    SCD_TX_STTS_QUEUE_OFFSET(txq_id),
				    zero_data, ARRAY_SIZE(zero_data));
		trans_sdio->txq[txq_id].disabling = false;

		/* FIXME: any other configs? */
		iwl_slv_free_data_queue(trans, txq_id);

		IWL_DEBUG_TX_QUEUES(trans, "Deactivate queue %d finished\n",
				    txq_id);
	}
}

void iwl_trans_sdio_txq_disable(struct iwl_trans *trans, int txq_id,
				bool configure_scd)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);

	if (configure_scd) {
		trans_sdio->txq[txq_id].disabling = true;
		schedule_work(&trans_sdio->iwl_sdio_disable_txq_wk);

		IWL_DEBUG_TX_QUEUES(trans, "Deactivate queue %d spawned\n",
				    txq_id);
		return;
	}

	/* FIXME: any other configs? */
	iwl_slv_free_data_queue(trans, txq_id);

	IWL_DEBUG_TX_QUEUES(trans, "Deactivate queue %d finished\n", txq_id);
}

void iwl_trans_sdio_txq_enable(struct iwl_trans *trans, int txq_id, u16 ssn,
			       const struct iwl_trans_txq_scd_cfg *cfg,
			       unsigned int wdg_timeout)
{
	struct iwl_trans_slv *trans_slv = IWL_TRANS_GET_SLV_TRANS(trans);
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);

	/* make sure we finished to disable the queues before allocation */
	flush_work(&trans_sdio->iwl_sdio_disable_txq_wk);

	if (cfg) {
		/* Disable the scheduler prior configuring the cmd queue */
		if (txq_id == trans_slv->cmd_queue)
			iwl_scd_enable_set_active(trans, 0);

		/* Stop this Tx queue before configuring it */
		iwl_scd_txq_set_inactive(trans, txq_id);

		/*
		 * Set this queue as a chain-building queue
		 * unless it is CMD queue
		 */
		if (txq_id != trans_slv->cmd_queue)
			iwl_scd_txq_set_chain(trans, txq_id);

		/*
		 * If this queue is mapped to a certain station:
		 * it is an AGG queue
		 */
		if (cfg->aggregate) {
			u16 ra_tid = BUILD_RAxTID(cfg->sta_id, cfg->tid);

			/* Map receiver-address / traffic-ID to this queue */
			iwl_sdio_txq_set_ratid_map(trans, ra_tid, txq_id);

			/* enable aggregations for the queue */
			iwl_scd_txq_enable_agg(trans, txq_id);
		} else {
			/*
			 * disable aggregations for the queue, this will also
			 * make the ra_tid mapping configuration irrelevant
			 * since it is now a non-AGG queue.
			 */
			iwl_scd_txq_disable_agg(trans, txq_id);
			ssn = trans_sdio->txq[txq_id].scd_write_ptr;
		}
	}

	trans_sdio->txq[txq_id].scd_write_ptr = ssn & (TFD_QUEUE_SIZE_MAX - 1);
	iwl_trans_slv_tx_set_ssn(trans, txq_id, ssn);

	/* Configure first TFD  */
	iwl_write_direct32(trans, HBUS_TARG_WRPTR,
			   (ssn & (TFD_QUEUE_SIZE_MAX - 1)) |
			   (txq_id << 8));

	if (cfg) {
		u8 frame_limit = cfg->frame_limit;

		iwl_write_prph(trans, SCD_QUEUE_RDPTR(txq_id), ssn);

		/* tx window and frame sizes for this queue */
		iwl_trans_write_mem32(trans, trans_sdio->scd_base_addr +
				      SCD_CONTEXT_QUEUE_OFFSET(txq_id), 0);
		iwl_trans_write_mem32(trans, trans_sdio->scd_base_addr +
			SCD_CONTEXT_QUEUE_OFFSET(txq_id) + sizeof(u32),
			((frame_limit << SCD_QUEUE_CTX_REG2_WIN_SIZE_POS) &
				SCD_QUEUE_CTX_REG2_WIN_SIZE_MSK) |
			((frame_limit << SCD_QUEUE_CTX_REG2_FRAME_LIMIT_POS) &
				SCD_QUEUE_CTX_REG2_FRAME_LIMIT_MSK));

		/*
		 * Set up Status area in SRAM, map to Tx DMA/FIFO,
		 * activate the queue
		 */
		iwl_write_prph(trans, SCD_QUEUE_STATUS_BITS(txq_id),
			       (1 << SCD_QUEUE_STTS_REG_POS_ACTIVE) |
			       (cfg->fifo << SCD_QUEUE_STTS_REG_POS_TXF) |
			       (1 << SCD_QUEUE_STTS_REG_POS_WSL) |
			       SCD_QUEUE_STTS_REG_MSK);

		/*
		 * Enable the scheduler by setting the cmd queue bit in
		 * ucode reg.
		 */
		if (txq_id == trans_slv->cmd_queue)
			iwl_scd_enable_set_active(trans, BIT(txq_id));

		IWL_DEBUG_TX_QUEUES(trans,
				    "Activate queue %d on FIFO %d WrPtr: %d\n",
				    txq_id, cfg->fifo,
				    ssn & (TFD_QUEUE_SIZE_MAX - 1));
	} else {
		IWL_DEBUG_TX_QUEUES(trans, "Activate queue %d WrPtr: %d\n",
				    txq_id, ssn & (TFD_QUEUE_SIZE_MAX - 1));
	}

	trans_sdio->txq[txq_id].ptfd_cur_row =
		IWL_SDIO_SRAM_TABLE_EMPTY_PTFD_CELL;
	trans_sdio->txq[txq_id].bye_count0 = 0;
	trans_sdio->txq[txq_id].bye_count1 = 0;
}

static void iwl_sdio_scd_reset(struct iwl_trans *trans, u32 scd_base_addr)
{
	u32 addr;

	addr = scd_base_addr + SCD_CONTEXT_MEM_LOWER_BOUND;

	/* reset context data memory */
	for (; addr < scd_base_addr + SCD_CONTEXT_MEM_UPPER_BOUND; addr += 4)
		iwl_trans_write_mem32(trans, addr, 0);

	/* reset tx status memory */
	for (; addr < scd_base_addr + SCD_TX_STTS_MEM_UPPER_BOUND; addr += 4)
		iwl_trans_write_mem32(trans, addr, 0);

	for (; addr < scd_base_addr +
	     SCD_TRANS_TBL_OFFSET_QUEUE(trans->cfg->base_params->num_of_queues);
	     addr += 4)
		iwl_trans_write_mem32(trans, addr, 0);
}

void iwl_sdio_tx_start(struct iwl_trans *trans, u32 scd_base_addr)
{
	struct iwl_trans_slv *trans_slv = IWL_TRANS_GET_SLV_TRANS(trans);
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	u32 i, data;

	trans_sdio->send_buf = kzalloc(IWL_SDIO_SEND_BUF_LEN, GFP_KERNEL);
	if (!trans_sdio->send_buf)
		return;

	trans_sdio->send_buf_idx = 0;

	trans_sdio->scd_base_addr = iwl_read_prph(trans, SCD_SRAM_BASE_ADDR);

	WARN_ON(scd_base_addr != 0 &&
		scd_base_addr != trans_sdio->scd_base_addr);

	iwl_sdio_scd_reset(trans, scd_base_addr);

	/* set BC table address */
	iwl_write_prph(trans, SCD_DRAM_BASE_ADDR,
		       trans_sdio->sf_mem_addresses->bc_base_addr >> 10);

	/* set SCD CB size to 64 */
	iwl_write_prph(trans, SCD_CB_SIZE, 0);

	iwl_set_bits_prph(trans, SCD_GP_CTRL,
			  SCD_GP_CTRL_AUTO_ACTIVE_MODE);

	if (trans->cfg->base_params->num_of_queues > 20)
		iwl_set_bits_prph(trans, SCD_GP_CTRL,
				  SCD_GP_CTRL_ENABLE_31_QUEUES);

	/* Set CB base pointer
	 * Transaction to FH maps from PTFD entry in SNF to the actual TFD;
	 * The FH assumes the data is address bits [35..8]
	 */
	for (i = 0; i < IWL_SDIO_CB_QUEUES_NUM; i++) {
		data = i << 5;
		iwl_write_direct32(trans, FH_MEM_CBBC_QUEUE(i), data);
	}

	/* enable command queue */
	iwl_trans_ac_txq_enable(trans, trans_slv->cmd_queue,
				trans_slv->cmd_fifo, 0);

	iwl_scd_activate_fifos(trans);

	for (i = 0; i < FH_TCSR_CHNL_NUM; i++) {
		data = FH_TCSR_TX_CONFIG_REG_VAL_DMA_CREDIT_ENABLE |
		       FH_TCSR_TX_CONFIG_REG_VAL_DMA_CHNL_ENABLE;

		iwl_write_direct32(trans, FH_TCSR_CHNL_TX_CONFIG_REG(i), data);
	}

	/* update chicken bits */
	data = iwl_read_prph(trans, FH_TX_CHICKEN_BITS_REG);
	iwl_write_prph(trans, FH_TX_CHICKEN_BITS_REG,
		       data | FH_TX_CHICKEN_BITS_SCD_AUTO_RETRY_EN);

	IWL_DEBUG_TX(trans, "Tx Started\n");
}

void iwl_sdio_tx_stop(struct iwl_trans *trans)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	int i, ret;

	if (!trans_sdio->send_buf)
		return;

	iwl_scd_deactivate_fifos(trans);

	for (i = 0; i < FH_TCSR_CHNL_NUM; i++) {
		iwl_write_direct32(trans, FH_TCSR_CHNL_TX_CONFIG_REG(i), 0);
		ret = iwl_poll_direct_bit(trans, FH_TSSR_TX_STATUS_REG,
				  FH_TSSR_TX_STATUS_REG_MSK_CHNL_IDLE(i),
				  1000);
		if (ret < 0)
			IWL_ERR(trans,
				"Failing on timeout while stopping DMA channel %d [0x%08x]\n",
				i, iwl_read_direct32(trans,
						     FH_TSSR_TX_STATUS_REG));
	}

	kfree(trans_sdio->send_buf);
	trans_sdio->send_buf = NULL;

	flush_work(&trans_sdio->iwl_sdio_disable_txq_wk);

	IWL_DEBUG_TX(trans, "Tx Stopped\n");
}

void iwl_sdio_tx_free(struct iwl_trans *trans)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);

	if (WARN_ON_ONCE(!trans_sdio)) {
		IWL_ERR(trans, "No transport?\n");
		return;
	}

	if (trans_sdio->dtu_cfg_pool) {
		kmem_cache_destroy(trans_sdio->dtu_cfg_pool);
		trans_sdio->dtu_cfg_pool = NULL;
	}

	iwl_slv_al_mem_pool_deinit(&trans_sdio->slv_tx.tfd_pool);
	iwl_slv_al_mem_pool_deinit(&trans_sdio->slv_tx.tb_pool);
}

int iwl_sdio_tx_init(struct iwl_trans *trans)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	int ret;

	/* header, trailer and meta-data */
	trans_sdio->cfg_pool_size = sizeof(struct iwl_sdio_dtu_info) +
				    sizeof(struct iwl_sdio_tx_dtu_hdr) +
				    sizeof(struct iwl_sdio_adma_desc) *
				    IWL_SDIO_ADMA_DESC_MAX_NUM +
				    sizeof(struct iwl_sdio_tfd) +
				    IWL_SDIO_BLOCK_SIZE;
	trans_sdio->dtu_cfg_pool = kmem_cache_create("iwl_sdio_tx_dtu_cfg_pool",
						     trans_sdio->cfg_pool_size,
						     sizeof(void *), 0, NULL);
	if (unlikely(!trans_sdio->dtu_cfg_pool)) {
		ret = -ENOMEM;
		goto exit;
	}

	spin_lock_init(&trans_sdio->slv_tx.mem_rsrc_lock);
	INIT_WORK(&trans_sdio->iwl_sdio_disable_txq_wk,
		  iwl_sdio_txq_disable_wk);

	ret = iwl_slv_al_mem_pool_init(&trans_sdio->slv_tx.tfd_pool,
				       IWL_SDIO_TFD_POOL_SIZE);
	if (ret)
		goto error_free;

	ret = iwl_slv_al_mem_pool_init(&trans_sdio->slv_tx.tb_pool,
				       IWL_SDIO_DEFAULT_TB_POOL_SIZE);
	if (ret)
		goto error_free;

	return 0;

error_free:
	iwl_sdio_tx_free(trans);

exit:
	return ret;
}

void iwl_sdio_tx_calc_desc_num(struct iwl_trans *trans,
			       struct iwl_slv_tx_chunk_info *chunk_info)
{
	struct iwl_trans_slv *trans_slv = IWL_TRANS_GET_SLV_TRANS(trans);

	chunk_info->desc_num = DIV_ROUND_UP(chunk_info->len,
					    trans_slv->config.tb_size);
}

void iwl_sdio_tx_free_dtu_mem(struct iwl_trans *trans, void **data)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	struct iwl_sdio_dtu_info *dtu_info = *data;

	if (!dtu_info || !trans_sdio->dtu_cfg_pool)
		return;

	kmem_cache_free(trans_sdio->dtu_cfg_pool, dtu_info);
	*data = NULL;

	return;
}

void iwl_sdio_tx_clean_dtu(struct iwl_trans *trans, void *data)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	struct iwl_sdio_dtu_info *dtu_info = data;
	int ret, i;

	if (WARN_ON(!data))
		return;

	spin_lock_bh(&trans_sdio->slv_tx.mem_rsrc_lock);

	if (dtu_info->sram_alloc.tfd == IWL_SLV_AL_INVALID)
		goto exit;

	ret = iwl_slv_al_mem_pool_free(&trans_sdio->slv_tx,
				       &trans_sdio->slv_tx.tfd_pool,
				       dtu_info->sram_alloc.tfd);
	if (ret)
		IWL_WARN(trans, "%s failed to free TFD\n", __func__);
	dtu_info->sram_alloc.tfd = IWL_SLV_AL_INVALID;

	for (i = 0; i < dtu_info->sram_alloc.num_fragments; i++) {
		int k;

		for (k = 0; k < dtu_info->sram_alloc.fragments[i].size; k++) {
			ret = iwl_slv_al_mem_pool_free(
				&trans_sdio->slv_tx,
				&trans_sdio->slv_tx.tb_pool,
				dtu_info->sram_alloc.fragments[i].index + k);
			if (ret)
				IWL_WARN(trans,
					"failed to free TB pool resources\n");
		}
	}

exit:
	spin_unlock_bh(&trans_sdio->slv_tx.mem_rsrc_lock);
	iwl_sdio_tx_free_dtu_mem(trans, &data);

	return;
}

static int
iwl_sdio_tx_get_resources(struct iwl_trans *trans, u8 txq_id,
			  struct iwl_slv_txq_entry *txq_entry)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	struct iwl_trans_slv *trans_slv = IWL_TRANS_GET_SLV_TRANS(trans);
	struct iwl_sdio_dtu_info *dtu_info = txq_entry->reclaim_info;
	int free_tfds, free_tbs, ret;
	u8 tbs_to_allocate = DIV_ROUND_UP(txq_entry->dtu_meta.total_len,
				       trans_slv->config.tb_size);
	u8 order;
	u32 max_order;

	/*
	 * The length range is different in 8000 HW family starting from B-step
	 * than the other NICs
	 */
	if ((trans->cfg->device_family != IWL_DEVICE_FAMILY_8000) ||
	    (CSR_HW_REV_STEP(trans->hw_rev) == SILICON_A_STEP))
		max_order = IWL_SDIO_MAX_ORDER;
	else
		max_order = IWL_SDIO_MAX_ORDER_8000;

	spin_lock_bh(&trans_sdio->slv_tx.mem_rsrc_lock);

	/* check resources availability according to policy */
	free_tbs = iwl_slv_al_mem_pool_free_count(&trans_sdio->slv_tx,
						  &trans_sdio->slv_tx.tb_pool);
	free_tfds = iwl_slv_al_mem_pool_free_count(&trans_sdio->slv_tx,
						   &trans_sdio->slv_tx.tfd_pool);
	IWL_DEBUG_TX(trans, "q %d free_tfds %d free_tbs %d tbs_to_alloc %d\n",
		     txq_id, free_tfds, free_tbs, tbs_to_allocate);
	if (!iwl_sdio_policy_check_alloc(trans_slv, txq_id,
					 free_tfds, free_tbs, 1,
					 tbs_to_allocate)) {
		dtu_info->sram_alloc.tfd = IWL_SLV_AL_INVALID;
		ret = -ENOMEM;
		goto error;
	}

	/* Allocate ptfds up to the last row, not inclusive. The last row is
	 * shared with the first ptfds in the queue, since the rows are written
	 * in full, the first ptfds will be overwritten when ptfd_cur_row is
	 * initialized
	 */
	if (atomic_read(&trans_slv->txqs[txq_id].sent_count) >=
	    IWL_SDIO_CB_QUEUE_SIZE - sizeof(trans_sdio->txq[0].ptfd_cur_row)) {
		dtu_info->sram_alloc.tfd = IWL_SLV_AL_INVALID;
		ret = -ENOSPC;
		goto error;
	}

	dtu_info->sram_alloc.txq_id = txq_id;

	/* allocate TFD */
	ret = iwl_slv_al_mem_pool_alloc(&trans_sdio->slv_tx,
					&trans_sdio->slv_tx.tfd_pool);
	if (ret < 0) {
		IWL_WARN(trans, "mem_pool_alloc failed tfd %d\n", ret);
		dtu_info->sram_alloc.tfd = IWL_SLV_AL_INVALID;
		goto error;
	}
	dtu_info->sram_alloc.tfd = ret;

	/* allocate TBs */
	dtu_info->sram_alloc.num_fragments = 0;
	/* capped by bits reserved for size 32 - IWL_SDIO_DMA_DESC_LEN_SHIFT */
	order = min_t(u32, max_order, ilog2(tbs_to_allocate));
	while (tbs_to_allocate > 0 &&
	       dtu_info->sram_alloc.num_fragments < IWL_SDIO_MAX_TBS_NUM) {
		int idx;
		u16 size = 1 << order;

		ret = iwl_slv_al_mem_pool_alloc_range(
					&trans_sdio->slv_tx,
					&trans_sdio->slv_tx.tb_pool, order);
		if (ret < 0) {
			/* look for smaller region size */
			if (!order)
				break;
			order--;
			continue;
		}
		idx = dtu_info->sram_alloc.num_fragments;
		dtu_info->sram_alloc.fragments[idx].index = ret;
		dtu_info->sram_alloc.fragments[idx].size = size;
		dtu_info->sram_alloc.num_fragments++;
		tbs_to_allocate -= size;
		if (tbs_to_allocate && tbs_to_allocate < size)
			order = min_t(u32, max_order, ilog2(tbs_to_allocate));
	}

	if (tbs_to_allocate > 0) {
		IWL_DEBUG_TX(trans, "could not allocate %d tbs\n",
			     txq_entry->dtu_meta.total_desc_num);
		goto error;
	}

	spin_unlock_bh(&trans_sdio->slv_tx.mem_rsrc_lock);
	return 0;

error:
	spin_unlock_bh(&trans_sdio->slv_tx.mem_rsrc_lock);
	iwl_sdio_tx_clean_dtu(trans, txq_entry->reclaim_info);
	return ret;
}

static int iwl_sdio_alloc_dtu_mem(struct iwl_trans *trans,
				  struct iwl_slv_txq_entry *txq_entry,
				  u8 txq_id)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);

	/* header & trailer for SDTM */
	txq_entry->reclaim_info = kmem_cache_alloc(trans_sdio->dtu_cfg_pool,
						   GFP_KERNEL);
	if (unlikely(!txq_entry->reclaim_info))
		return -ENOMEM;

	memset(txq_entry->reclaim_info, 0, trans_sdio->cfg_pool_size);

	/* PTFD, TFD, TB */
	return iwl_sdio_tx_get_resources(trans, txq_id, txq_entry);

}

/* set ADMA descriptors defining the distination of the data stream */
static void iwl_sdio_config_adma(struct iwl_trans *trans,
				 struct iwl_slv_txq_entry *txq_entry, u8 txq_id)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	struct iwl_sdio_dtu_info *dtu_info = txq_entry->reclaim_info;
	struct iwl_sdio_tx_dtu_hdr *dtu_hdr = dtu_info->ctrl_buf;
	struct iwl_sdio_adma_desc *adma_list = dtu_hdr->adma_list;
	struct iwl_sdio_tfd *tfd;
	u32 tfd_num = trans_sdio->txq[txq_id].scd_write_ptr &
			(IWL_SDIO_CB_QUEUE_SIZE - 1);
	u8 desc_idx;
	u32 bc_addr;
	u32 adma_dsc_mem_base =
		trans_sdio->sf_mem_addresses->adma_dsc_mem_base;
	int i;

	/*
	 * From 8000 HW family B-step the dma_desc is enlarged by 4 bytes on
	 * the expense of the reserved field that comes right after that
	 */
	if ((trans->cfg->device_family != IWL_DEVICE_FAMILY_8000) ||
	    (CSR_HW_REV_STEP(trans->hw_rev) == SILICON_A_STEP))
		dtu_hdr->dma_desc =
			cpu_to_le32(
			    adma_dsc_mem_base |
			    ((dtu_info->adma_desc_num *
			      sizeof(struct iwl_sdio_adma_desc)) << 20));
	else
		*((__le64 *)&(dtu_hdr->dma_desc)) =
			cpu_to_le64(
			    ((u64)adma_dsc_mem_base <<
			     IWL_SDIO_DMA_DESC_8000_ADDR_SHIFT) |
			    ((dtu_info->adma_desc_num *
			      sizeof(struct iwl_sdio_adma_desc)) <<
			     IWL_SDIO_DMA_DESC_8000_LEN_SHIFT));

	tfd = (void *)((u8 *)dtu_info->ctrl_buf +
		       sizeof(struct iwl_sdio_tx_dtu_hdr) +
		       sizeof(struct iwl_sdio_adma_desc) *
		       dtu_info->adma_desc_num);

	/* ADMA desc for TFD */
	desc_idx = 0;
	adma_list[desc_idx].attr =
		IWL_SDIO_ADMA_ATTR_VALID | IWL_SDIO_ADMA_ATTR_ACT2;
	adma_list[desc_idx].reserved = 0;
	adma_list[desc_idx].length =
		cpu_to_le16(sizeof(struct iwl_sdio_tfd));
	adma_list[desc_idx].addr =
		cpu_to_le32(trans_sdio->sf_mem_addresses->tfd_base_addr +
			    dtu_info->sram_alloc.tfd *
			    sizeof(struct iwl_sdio_tfd));
	desc_idx++;

	/* data mapping */
	for (i = 0; i < tfd->num_fragments; i++) {
		u32 tb_len, aligned_len;

		adma_list[desc_idx].attr =
			IWL_SDIO_ADMA_ATTR_VALID | IWL_SDIO_ADMA_ATTR_ACT2;
		adma_list[desc_idx].reserved = 0;
		tb_len = le32_to_cpu(tfd->tbs[i]) >>
			IWL_SDIO_DMA_DESC_LEN_SHIFT;
		aligned_len = round_up(tb_len, 4);
		dtu_info->data_pad_len = aligned_len - tb_len;
		adma_list[desc_idx].length = cpu_to_le16(aligned_len);

		adma_list[desc_idx].addr =
			cpu_to_le32(trans_sdio->sf_mem_addresses->tb_base_addr +
				    IWL_SDIO_TB_SIZE *
				    dtu_info->sram_alloc.fragments[i].index);
		desc_idx++;
	}

	/* PTFD descriptor */
	adma_list[desc_idx].attr =
		IWL_SDIO_ADMA_ATTR_VALID | IWL_SDIO_ADMA_ATTR_ACT2;
	adma_list[desc_idx].reserved = 0;
	adma_list[desc_idx].length = cpu_to_le16(4);
	adma_list[desc_idx].addr =
		cpu_to_le32(trans_sdio->sf_mem_addresses->tfdi_base_addr +
			    txq_id * IWL_SDIO_CB_QUEUE_SIZE +
			    ALIGN_DW_LOW(tfd_num));
	desc_idx++;

	/* SCD BC descriptor */
	bc_addr = ALIGN_DW_LOW(trans_sdio->sf_mem_addresses->bc_base_addr +
			       IWL_SDIO_BC_TABLE_ENTRY_SIZE_BYTES *
			       ((2 * txq_id) * IWL_SDIO_CB_QUEUE_SIZE + tfd_num));

	adma_list[desc_idx].attr =
		IWL_SDIO_ADMA_ATTR_VALID | IWL_SDIO_ADMA_ATTR_ACT2;
	adma_list[desc_idx].reserved = 0;
	adma_list[desc_idx].length = cpu_to_le16(4);
	adma_list[desc_idx].addr = cpu_to_le32(bc_addr);
	desc_idx++;

	bc_addr = ALIGN_DW_LOW(trans_sdio->sf_mem_addresses->bc_base_addr +
			       IWL_SDIO_BC_TABLE_ENTRY_SIZE_BYTES *
			       ((2 * txq_id + 1) * IWL_SDIO_CB_QUEUE_SIZE + tfd_num));

	adma_list[desc_idx].attr =
		IWL_SDIO_ADMA_ATTR_VALID | IWL_SDIO_ADMA_ATTR_ACT2;
	adma_list[desc_idx].reserved = 0;
	adma_list[desc_idx].length = cpu_to_le16(4);
	adma_list[desc_idx].addr = cpu_to_le32(bc_addr);
	desc_idx++;

	/* SCD WR PTR descriptor */
	adma_list[desc_idx].attr =
		IWL_SDIO_ADMA_ATTR_END | IWL_SDIO_ADMA_ATTR_VALID |
		IWL_SDIO_ADMA_ATTR_ACT2;
	adma_list[desc_idx].reserved = 0;
	adma_list[desc_idx].length = cpu_to_le16(4);
	adma_list[desc_idx].addr = cpu_to_le32(IWL_SDIO_SCD_WR_PTR_ADDR);
	desc_idx++;
}

/**
 * iwl_sdio_comp_ptfd() - compute ptfd row
 * @trans_sdio: transport
 * @txq_id: the queue index
 *
 * Since ADMA can work only with dword aligned addresses, each time
 * the PTFD table is updated - the whole row should be written.
 */
static u32 iwl_sdio_comp_ptfd(struct iwl_trans *trans,
			      struct iwl_slv_txq_entry *txq_entry,
			      u8 txq_id)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	struct iwl_sdio_dtu_info *dtu_info = (void *)txq_entry->reclaim_info;
	u32 ptfd_cur_row = trans_sdio->txq[txq_id].ptfd_cur_row;
	u8 shift = (trans_sdio->txq[txq_id].scd_write_ptr & 0x3)
		    * BITS_PER_BYTE;

	if (!(trans_sdio->txq[txq_id].scd_write_ptr & 0x3))
		ptfd_cur_row = IWL_SDIO_SRAM_TABLE_EMPTY_PTFD_CELL;

	WARN_ON_ONCE(dtu_info->sram_alloc.tfd & ~0xff);

	/* clear the byte that we need to write and write the tfd index */
	ptfd_cur_row &= ~(0xff << shift);
	ptfd_cur_row |= (dtu_info->sram_alloc.tfd & 0xff) << shift;

	trans_sdio->txq[txq_id].ptfd_cur_row = ptfd_cur_row;

	return ptfd_cur_row;
}

#define IWL_TX_CRC_SIZE 4
#define IWL_TX_DELIMITER_SIZE 4

/**
 * iwl_sdio_comp_bc() - compute BC row
 * @trans_sdio:
 * @txq_id:
 *
 * BC should be in dwords instead of bytes. Each row contains two BCs
 * because of the ADMA address alignment requirement. The BC row layout
 * is as follows:
 * 0..11 bc 12..15 sta_id 16..27 bc 28..31 sta_id
 *
 */
static void iwl_sdio_comp_bc(struct iwl_trans *trans,
			     struct iwl_slv_txq_entry *txq_entry, u8 txq_id)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	struct iwl_device_cmd *cmd =
		(void *)txq_entry->dtu_meta.chunk_info[0].addr;
	struct iwl_tx_cmd *tx_cmd = (void *)cmd->payload;
	u16 len, val;

	len = le16_to_cpu(tx_cmd->len) +
			IWL_TX_CRC_SIZE +
			IWL_TX_DELIMITER_SIZE;

	switch (tx_cmd->sec_ctl & TX_CMD_SEC_MSK) {
	case TX_CMD_SEC_CCM:
		len += IEEE80211_CCMP_MIC_LEN;
		break;
	case TX_CMD_SEC_TKIP:
		len += IEEE80211_TKIP_ICV_LEN;
		break;
	case TX_CMD_SEC_WEP:
		len += IEEE80211_WEP_IV_LEN + IEEE80211_WEP_ICV_LEN;
		break;
	}

	if (trans_sdio->bc_table_dword)
		len = DIV_ROUND_UP(len, 4);

	val = len | (tx_cmd->sta_id << 12);

	if (trans_sdio->txq[txq_id].scd_write_ptr & 0x1)
		trans_sdio->txq[txq_id].bye_count1 = val;
	else {
		trans_sdio->txq[txq_id].bye_count0 = val;
		trans_sdio->txq[txq_id].bye_count1 = 0;
	}
}

static inline void iwl_sdio_inc_scd_wr_ptr(struct iwl_trans *trans,
					   u8 txq_id)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);

	trans_sdio->txq[txq_id].scd_write_ptr++;
	trans_sdio->txq[txq_id].scd_write_ptr &= (TFD_QUEUE_SIZE_MAX - 1);
}

static void iwl_sdio_config_dtu_trailer(struct iwl_trans *trans,
					struct iwl_slv_txq_entry *txq_entry,
					u8 txq_id)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	struct iwl_sdio_dtu_info *dtu_info = txq_entry->reclaim_info;
	struct iwl_sdio_tx_dtu_trailer *dtu_trailer;

	u32 val;

	dtu_trailer = (struct iwl_sdio_tx_dtu_trailer *)
		      ((u8 *)dtu_info->ctrl_buf +
		       sizeof(struct iwl_sdio_tx_dtu_hdr) +
		       sizeof(struct iwl_sdio_adma_desc) *
		       dtu_info->adma_desc_num +
		       sizeof(struct iwl_sdio_tfd));

	val = iwl_sdio_comp_ptfd(trans, txq_entry, txq_id);
	dtu_trailer->ptfd = cpu_to_le32(val);

	iwl_sdio_comp_bc(trans, txq_entry, txq_id);
	dtu_trailer->scd_bc0_dup = dtu_trailer->scd_bc0 =
		cpu_to_le16(trans_sdio->txq[txq_id].bye_count0);
	dtu_trailer->scd_bc1_dup = dtu_trailer->scd_bc1 =
		cpu_to_le16(trans_sdio->txq[txq_id].bye_count1);

	iwl_sdio_inc_scd_wr_ptr(trans, txq_id);

	dtu_trailer->scd_wr_ptr =
		cpu_to_le32(trans_sdio->txq[txq_id].scd_write_ptr |
			    (txq_id << 8));
}

static void iwl_sdio_config_tfd(struct iwl_trans *trans,
				struct iwl_slv_txq_entry *txq_entry)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	struct iwl_sdio_dtu_info *dtu_info =
		(struct iwl_sdio_dtu_info *)txq_entry->reclaim_info;
	struct iwl_sdio_tfd *tfd;
	u32 tb_addr, tb_len, last_tb, tb_base;
	int cur_len;
	int idx;

	/*
	 * In 8000 HW family starting from B/C-step the tb_base is relative
	 * rather than absolute as the other NICs and steps.
	 */
	tb_base = trans_sdio->sf_mem_addresses->tb_base_addr;
	if ((trans->cfg->device_family == IWL_DEVICE_FAMILY_8000) &&
	    (CSR_HW_REV_STEP(trans->hw_rev) != SILICON_A_STEP))
		tb_base -= IWL_SDIO_8000B_SF_MEM_BASE_ADDR;

	tfd = (void *)((u8 *)dtu_info->ctrl_buf +
		       sizeof(struct iwl_sdio_tx_dtu_hdr) +
		       sizeof(struct iwl_sdio_adma_desc) *
		       dtu_info->adma_desc_num);

	tfd->num_fragments = dtu_info->sram_alloc.num_fragments;
	for (idx = 0; idx < tfd->num_fragments; idx++) {
		cur_len = IWL_SDIO_TB_SIZE *
			  dtu_info->sram_alloc.fragments[idx].size;
		if (idx == tfd->num_fragments - 1) {
			/*
			 * last tb len isn't necessarily congrouent to tb size
			 * but sums up exactly to the packet length.
			 */
			last_tb = (txq_entry->dtu_meta.total_len - 1) %
				   IWL_SDIO_TB_SIZE + 1;
			cur_len -= IWL_SDIO_TB_SIZE;
			cur_len += last_tb;
		}
		tb_len = cur_len << IWL_SDIO_DMA_DESC_LEN_SHIFT;
		tb_addr = tb_base +
			  dtu_info->sram_alloc.fragments[idx].index *
			  IWL_SDIO_TB_SIZE;
		tfd->tbs[idx] = cpu_to_le32(tb_addr | tb_len);
	}
}

static void iwl_sdio_build_dtu(struct iwl_trans *trans,
			       struct iwl_slv_txq_entry *txq_entry,
			       u8 txq_id)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);

	struct iwl_sdio_dtu_info *dtu_info = txq_entry->reclaim_info;
	struct iwl_sdio_tx_dtu_hdr *dtu_hdr = dtu_info->ctrl_buf;

	dtu_hdr->hdr.op_code = IWL_SDIO_OP_CODE_TX_DATA | IWL_SDIO_EOT_BIT;
	dtu_hdr->hdr.seq_number = iwl_sdio_get_cmd_seq(trans_sdio, true);
	dtu_hdr->hdr.signature = cpu_to_le16(IWL_SDIO_CMD_HEADER_SIGNATURE);

	/* the number of system (not data) fields in DTU is 5 */
	dtu_info->adma_desc_num = dtu_info->sram_alloc.num_fragments + 5;

	iwl_sdio_config_tfd(trans, txq_entry);
	iwl_sdio_config_adma(trans, txq_entry, txq_id);
	iwl_sdio_config_dtu_trailer(trans, txq_entry, txq_id);
}

int iwl_sdio_flush_dtus(struct iwl_trans *trans)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	struct sdio_func *func = IWL_TRANS_SDIO_GET_FUNC(trans);
	int ret = 0;

	if (!trans_sdio->send_buf_idx)
		goto out;

	/* Only block size transfers are supported. In order to pad the whole
	 * transfer, add a trailing command for writing padding bytes to the
	 * trash buffer register
	 */
	if (!IS_ALIGNED(trans_sdio->send_buf_idx, IWL_SDIO_BLOCK_SIZE)) {
		u8 *cur;
		struct iwl_sdio_tx_dtu_hdr *dtu_hdr;
		struct iwl_sdio_adma_desc *dma_desc;
		u32 adma_dsc_mem_base =
			trans_sdio->sf_mem_addresses->adma_dsc_mem_base;
		int len, aligned_len;

		len = trans_sdio->send_buf_idx + PAD_DTU_LEN;
		aligned_len = ALIGN(len, IWL_SDIO_BLOCK_SIZE);

		cur = (u8 *)&trans_sdio->send_buf[trans_sdio->send_buf_idx];
		dtu_hdr = (struct iwl_sdio_tx_dtu_hdr *)cur;
		dtu_hdr->hdr.op_code = IWL_SDIO_OP_CODE_TX_DATA | IWL_SDIO_EOT_BIT;
		dtu_hdr->hdr.seq_number = iwl_sdio_get_cmd_seq(trans_sdio, true);
		dtu_hdr->hdr.signature = cpu_to_le16(IWL_SDIO_CMD_HEADER_SIGNATURE);
		memset(dtu_hdr->reserved, 0, sizeof(dtu_hdr->reserved));
		/*
		 * From 8000 HW family B-step the dma_desc is enlarged by 4
		 * bytes on the expense of the reserved field that comes right
		 * after that
		 */
		if ((trans->cfg->device_family != IWL_DEVICE_FAMILY_8000) ||
		    (CSR_HW_REV_STEP(trans->hw_rev) == SILICON_A_STEP))
			dtu_hdr->dma_desc =
				cpu_to_le32(adma_dsc_mem_base |
					    (sizeof(*dma_desc) <<
					     IWL_SDIO_DMA_DESC_LEN_SHIFT));
		else
			*((__le64 *)&(dtu_hdr->dma_desc)) =
				cpu_to_le64(((u64)adma_dsc_mem_base <<
					    IWL_SDIO_DMA_DESC_8000_ADDR_SHIFT) |
					    (sizeof(*dma_desc) <<
					     IWL_SDIO_DMA_DESC_8000_LEN_SHIFT));

		cur += sizeof(*dtu_hdr);
		dma_desc = (struct iwl_sdio_adma_desc *)cur;

		dma_desc->attr = IWL_SDIO_ADMA_ATTR_END |
				 IWL_SDIO_ADMA_ATTR_VALID |
				 IWL_SDIO_ADMA_ATTR_ACT2;
		dma_desc->reserved = 0;
		dma_desc->length = cpu_to_le16(aligned_len - len);
		dma_desc->addr = cpu_to_le32(IWL_SDIO_TRASH_BUF_REG);

		trans_sdio->send_buf_idx = aligned_len;
		memset(&trans_sdio->send_buf[len],
		       IWL_SDIO_CMD_PAD_BYTE, aligned_len - len);
	}

	sdio_claim_host(func);

	ret = sdio_writesb(func, IWL_SDIO_DATA_ADDR, trans_sdio->send_buf,
			   trans_sdio->send_buf_idx);
	if (ret)
		IWL_ERR(trans, "Cannot send buffer %d\n", ret);
	else
		trans_sdio->send_buf_idx = 0;

	sdio_release_host(func);

out:
	return ret;
}

static int iwl_sdio_send_dtu(struct iwl_trans *trans,
			     struct iwl_slv_txq_entry *txq_entry,
			     u8 txq_id)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	struct iwl_sdio_dtu_info *dtu_info = txq_entry->reclaim_info;
	u8 *cur, *dtu;
	int i, send_len, ret;
	int adma_desc_len;

	adma_desc_len = sizeof(struct iwl_sdio_adma_desc) *
			dtu_info->adma_desc_num;
	send_len = txq_entry->dtu_meta.total_len +
		dtu_info->data_pad_len +
		sizeof(struct iwl_sdio_tx_dtu_hdr) +
		adma_desc_len +
		sizeof(struct iwl_sdio_tfd) +
		sizeof(struct iwl_sdio_tx_dtu_trailer);

	if (trans_sdio->send_buf_idx + send_len + PAD_DTU_LEN >
	    IWL_SDIO_SEND_BUF_LEN) {
		IWL_DEBUG_TX(trans, "send buf too big. flushing\n");
		ret = iwl_sdio_flush_dtus(trans);
		if (ret)
			return ret;
	}

	cur = &trans_sdio->send_buf[trans_sdio->send_buf_idx];

	trans_sdio->send_buf_idx += send_len;

	dtu = (u8 *)dtu_info->ctrl_buf;
	memcpy(cur, dtu, sizeof(struct iwl_sdio_tx_dtu_hdr));
	dtu += sizeof(struct iwl_sdio_tx_dtu_hdr);
	cur += sizeof(struct iwl_sdio_tx_dtu_hdr);

	memcpy(cur, dtu, adma_desc_len);
	dtu += adma_desc_len;
	cur += adma_desc_len;

	memcpy(cur, dtu, sizeof(struct iwl_sdio_tfd));
	dtu += sizeof(struct iwl_sdio_tfd);
	cur += sizeof(struct iwl_sdio_tfd);

	for (i = 0; i < txq_entry->dtu_meta.chunks_num; i++) {
		memcpy(cur, txq_entry->dtu_meta.chunk_info[i].addr,
		       txq_entry->dtu_meta.chunk_info[i].len);
		cur += txq_entry->dtu_meta.chunk_info[i].len;
	}

	memset(cur, 0xac, dtu_info->data_pad_len);
	cur += dtu_info->data_pad_len;

	memcpy(cur, dtu, sizeof(struct iwl_sdio_tx_dtu_trailer));

	return 0;
}

/*
 * send a single skb at a time.
 * This is done in coherence with the QoS enablement logic.
 */
int iwl_sdio_process_dtu(struct iwl_trans_slv *trans_slv, u8 txq_id)
{
	struct iwl_trans_sdio *trans_sdio =
		IWL_TRANS_SLV_GET_SDIO_TRANS(trans_slv);
	struct iwl_slv_txq_entry *txq_entry;
	int ret;


	txq_entry = iwl_slv_txq_pop_entry(trans_slv, txq_id);
	if (!txq_entry)
		return -ENOENT;

	ret = iwl_sdio_alloc_dtu_mem(trans_sdio->trans, txq_entry, txq_id);
	if (ret) {
		iwl_slv_txq_pushback_entry(trans_slv, txq_id, txq_entry);
		return ret;
	}

	iwl_sdio_build_dtu(trans_sdio->trans, txq_entry, txq_id);

	ret = iwl_sdio_send_dtu(trans_sdio->trans, txq_entry, txq_id);
	/* FIXME: what if there is a failure here ? */

	iwl_slv_txq_add_to_sent(trans_slv, txq_id, txq_entry);

	return ret;
}
