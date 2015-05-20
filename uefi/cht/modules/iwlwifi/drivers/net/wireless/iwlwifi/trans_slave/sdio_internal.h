/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2007 - 2014 Intel Corporation. All rights reserved.
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
 * Copyright(c) 2005 - 2014 Intel Corporation. All rights reserved.
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

#ifndef __iwl_trans_int_sdio_h__
#define __iwl_trans_int_sdio_h__

#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_func.h>
#include <linux/list.h>

#include "shared.h"
#include "iwl-drv.h"
#include "iwl-trans.h"
#include "iwl-debug.h"
#include "sdio_al.h"
#include "sdio_tx.h"

/* Extractors */
#define IWL_TRANS_GET_SDIO_TRANS(_iwl_trans) ((struct iwl_trans_sdio *)\
		((IWL_TRANS_GET_SLV_TRANS(_iwl_trans))->bus_specific))

#define IWL_TRANS_SDIO_GET_FUNC(_iwl_trans) \
		dev_to_sdio_func(((struct iwl_trans *)(_iwl_trans))->dev)

#define IWL_TRANS_SLV_GET_SDIO_TRANS(_slv_trans)\
	((struct iwl_trans_sdio *)((_slv_trans)->bus_specific))

/*
 * SDIO properties configurations
 */
#define IWL_SDIO_READ_VAL_ERR			0xFFFFFFFF

/*
 * SDIO configuration for the FH
 */
#define IWL_SDIO_FH_TSSR_TX_CONFIG		0x7ff8001

/*
 * SDIO configuration
 */
#define IWL_SDIO_CSR_HW_CONFIG_IF_BIT		0x10
#define IWL_SDIO_CSR_HW_COFIG \
				(0x8400 | IWL_SDIO_CSR_HW_CONFIG_IF_BIT);

/* SDIO RBD size*/
enum iwl_sdio_rb_size {
	IWL_SDIO_RB_SIZE_1K,
	IWL_SDIO_RB_SIZE_2K,
	IWL_SDIO_RB_SIZE_4K,
	IWL_SDIO_RB_SIZE_8K,
	IWL_SDIO_RB_SIZE_16K,
	IWL_SDIO_RB_SIZE_32K,
	IWL_SDIO_MAX_RB_SIZE = IWL_SDIO_RB_SIZE_32K,
};

/**
 * struct iwl_sdio_txq - a Tx queue
 * @ptfd_cur_row:	since ADMA descriptors can handle only 4 bytes aligned
 *			data, whenever driver needs to update ptfd it writes 4
 *			bytes to the PTFD table, thus it needs to store the
 *			previous values in the current row
 * @bc_cur_row:		byte count values, same as ptfd table
 * @scd_write_ptr:	per queue scheduler write pointer
 * @disabling:		true if op_mode required to disable the tx queue and
 *			the tx queue hasn't been disable yet
 */
struct iwl_sdio_txq {
	u32 ptfd_cur_row;
	u16 bye_count0;
	u16 bye_count1;
	u32 scd_write_ptr;
	bool disabling;
};

/*
 * struct iwl_sdio_plat_data - sdio OOB irq platform data
 * @gpio: gpio used for OOB interrupt
 * @irq: the actual irq (e.g. mapped from the gpio) used for OOB interrupt.
 */
struct iwl_sdio_plat_data {
	int gpio;
	int irq;
};

/*
 * SDIO specific transport structure.
 *
 * @bc_table_dword: true if the BC table expects DWORD (as opposed to bytes)
 * @slv_tx:
 * @dtu_cfg_pool:	pool for a memory buffer to hold DTU control (header,
 *			trailer and TFD) and internal meta data
 * @txq:		the txqs
 * @scd_base_addr:	the base address of scheduler region
 * @cfg_pool_size:	the size of dtu_cfg_pool buffers
 * @send_buf:		the buffer for copying DTU to be sent
 * @sdio_adma_addr: the default address to set for the ADMA in SDIO mode until
 *	we get the ALIVE from the uCode
 */
struct iwl_trans_sdio {
	struct iwl_drv *drv;
	struct sdio_func *func;
	struct iwl_trans *trans;

	/* Shared variables for target access read/write */
	struct mutex target_access_mtx;
	struct iwl_sdio_cmd_buffer ta_buff;
	wait_queue_head_t wait_target_access;
	u8 ta_read_seq;
	u8 ta_write_seq;

	/* Target access read variables */
	void *ta_read_buff;

	/* RX data path memory */
	struct kmem_cache *rx_mem_desc_pool;
	char rx_mem_desc_pool_name[50];
	struct list_head rx_mem_buff_list;
	struct mutex rx_buff_mtx; /* locks the rx mem buffer list */

	/* Tx data path */
	struct iwl_trans_slv_tx slv_tx;
	struct kmem_cache *dtu_cfg_pool;
	struct iwl_sdio_txq txq[IWL_SDIO_CB_QUEUES_NUM];
	struct work_struct iwl_sdio_disable_txq_wk;
	u32 scd_base_addr;
	u32 cfg_pool_size;
	u8 *send_buf;
	u32 send_buf_idx;

	/* Work definitions */
	struct work_struct d2h_work;
	struct work_struct rx_work;

	/* Misc configurations */
	u32 rx_page_order;
	bool rx_buf_size_8k;
	bool bc_table_dword;
	const char *const *command_names;

	struct iwl_sdio_plat_data plat_data;

	/* suspend flow */
	bool suspended;
	bool pending_irq;

	/* Debug variables */
	bool print_rx_hex_dump;
	const struct iwl_sdio_sf_mem_addresses *sf_mem_addresses;
	struct net_device napi_dev;
	struct napi_struct napi;
	struct iwl_sdio_sf_mem_addresses mem_addresses;

	u32 sdio_adma_addr;
};

/*
 * A RX buffer memory descriptor.
 *
 * Used to keep a RX buffer memory allocation pointer and required fields.
 * @page: The allocated page for theeee buffer.
 * @length: The length of the buffer, also used to calculate the page_order.
 */
struct iwl_sdio_rx_mem_desc {
	struct list_head list;
	struct page *page;
	u32 page_order;
	u32 length;
};

/*
 * SDIO API
 */
struct iwl_trans *iwl_trans_sdio_alloc(struct sdio_func *func,
				       const struct sdio_device_id *id,
				       const struct iwl_cfg *cfg);
void iwl_trans_sdio_free(struct iwl_trans *trans);

/*****************************************************
* RX
******************************************************/
void iwl_sdio_d2h_work(struct work_struct *work);
void iwl_sdio_rx_work(struct work_struct *work);
void iwl_sdio_isr(struct sdio_func *func);
void iwl_sdio_free_rx_mem(struct iwl_trans *trans);

/*
 * Returns a string representation of the received RX command/reply
 */
static inline const char *get_cmd_string(struct iwl_trans_sdio *trans_sdio,
					 u8 cmd)
{
	if (!trans_sdio->command_names || !trans_sdio->command_names[cmd])
		return "UNKNOWN";
	return trans_sdio->command_names[cmd];
}

/*****************************************************
* TX
******************************************************/
int iwl_sdio_tx_init(struct iwl_trans *trans);

/*****************************************************
* Target Access
******************************************************/
void iwl_sdio_handle_ta_read_ready(struct iwl_trans *trans,
				   struct iwl_sdio_cmd_buffer *ta_buff);
int iwl_sdio_read_hw_rev_nic_off(struct iwl_trans *trans);

#ifdef CONFIG_PM_SLEEP
void _iwl_sdio_suspend(struct iwl_trans *trans);
void _iwl_sdio_resume(struct iwl_trans *trans);
#endif

/*
 * INTERNAL API
 *
 * Used to read/write data to and from the SDIO bus.
 * Should only be used internally by this transport.
 * Returns the transaction return value.
 * Uses trace command for logging.
 *
 * FOR EXTERNAL USE PLEASE USE : iwl_read/iwl_write.
 */
int iwl_sdio_write8(struct iwl_trans *trans, u32 ofs, u8 val);
int iwl_sdio_write32(struct iwl_trans *trans, u32 ofs, u32 val);
u8 iwl_sdio_read8(struct iwl_trans *trans, u32 ofs, int *ret);
u8 iwl_sdio_f0_read8(struct iwl_trans *trans, u32 ofs, int *ret);
u32 iwl_sdio_read32(struct iwl_trans *trans, u32 ofs, int *ret);
u8 iwl_sdio_get_cmd_seq(struct iwl_trans_sdio *trans_sdio, bool write);
#endif /* __iwl_trans_int_sdio_h__ */
