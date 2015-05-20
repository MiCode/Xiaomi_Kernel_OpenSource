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

#ifndef __iwl_sdio_tx_h__
#define __iwl_sdio_tx_h__

#include "shared.h"
#include "sdio_al.h"

#define IWL_SDIO_TB_SIZE 256
#define IWL_SDIO_TB_SIZE_MASK (IWL_SDIO_TB_SIZE - 1)
#define IWL_SDIO_MAX_TBS_NUM 7
#define IWL_SDIO_CB_QUEUE_SIZE 64
#define IWL_SDIO_CB_QUEUES_NUM 32
#define IWL_SDIO_BC_TABLE_ENTRY_SIZE_BYTES 2
#define IWL_SDIO_SRAM_TABLE_EMPTY_PTFD_CELL 0xFFFFFFFF
#define IWL_SDIO_SRAM_TABLE_EMPTY_BC_CELL 0
#define IWL_SDIO_PTFDS_ROW_SIZE 4
#define IWL_SDIO_BC_ROW_SIZE 2
#define IWL_SDIO_SEND_BUF_LEN (32 * 1024)

#define FDL_DMA_DESC_ADDRESS	0x86000
#define FDL_DMA_DESC_ADDRESS_B	0x406000
#define FDL_DATA_ADDRESS	0x86050
#define FDL_DATA_ADDRESS_B	0x406050

#define FDL_FH_CONTROL_ADD1	0x1e20
#define FDL_FH_CONTROL_ADD2	0x1948
#define FDL_FH_CONTROL_ADD3	0x194c
#define FDL_FH_CONTROL_ADD4	0x19c8
#define FDL_FH_CONTROL_ADD5	0x1e28
#define FDL_FH_CONTROL_ADD6	0x1b00

#define FDL_FH_CONTROL_CMD1	0x101003
#define FDL_FH_CONTROL_CMD2	0x10400
#define FDL_FH_KICK		0x80000000
#define FDL_PAD_WORD		0xAC

#define FDL_NUM_OF_DMA_DESC	9
#define FDL_NUM_OF_FH_CONTROL_DESC	28

#define IWL_FDL_SDIO_MAX_PAYLOAD_SIZE 31744 /* 31K */
#define IWL_FDL_SDIO_MAX_BUFFER_SIZE (IWL_FDL_SDIO_MAX_PAYLOAD_SIZE + 800)

/**
 * struct iwl_sdio_sram_fragment - a fragment of contiguous tbs
 * @index:	starting tb indx
 * @size:	number of contiguous tbs
 */
struct iwl_sdio_sram_fragment {
	u16 index;
	u16 size;
};

/**
 * struct iwl_sdio_sram_alloc - internal representation  of sram allocations
 * @tfd:		tfd idx
 * @txq_id:		tx queue id
 * @num_fragments:	number tb fragments
 * @fragments:		starting index and length of tb fragment allocated

 */
struct iwl_sdio_sram_alloc {
	u16 tfd;
	u8 txq_id;
	u8 num_fragments;
	struct iwl_sdio_sram_fragment fragments[IWL_SDIO_MAX_TBS_NUM];
};

/**
 * struct iwl_sdio_tfd - compressed TFD for SDTM
 * @reserved:		unused
 * @num_fragments:	number of used fragments
 * @tbs:		bits [0..19] address, bits [20..31] length in bytes
 */
struct iwl_sdio_tfd {
	u8 reserved[3];
	u8 num_fragments;
	__le32 tbs[IWL_SDIO_MAX_TBS_NUM];
} __packed;

/* SD ADMA definitions;
 * According to paragraph 1.13.4 of SD HC specification v3.00
 */

#define IWL_SDIO_ADMA_ATTR_VALID 0x01
#define IWL_SDIO_ADMA_ATTR_END	 0x02
#define IWL_SDIO_ADMA_ATTR_ACT1	 0x10
#define IWL_SDIO_ADMA_ATTR_ACT2	 0x20

struct iwl_sdio_adma_desc {
	u8 attr;
	u8 reserved;
	__le16 length;
	__le32 addr;
} __packed;

struct iwl_sdio_fh_desc {
	__le32 fh_first_word;
	__le32 ptr_in_sf;
	__le32 image_size_in_byte;
	__le32 ptr_in_sram;
	__le32 fh_cmd;
	__le32 const_0;
	__le32 f_kick;
	u32 pad[0];
} __packed;

#define IWL_SDIO_ADMA_DESC_MAX_NUM 13

/**
 * struct iwl_sdio_tx_dtu_hdr - the header for DTU
 * @hdr:	sdio command header
 * @dma_desc:	destination address and length of ADMA descriptors sequence.
 *		On 8000 HW family from B-step this field is 8 bytes, taking 4
 *		additional bytes from the reserved field
 * @reserved:
 * @adma_list:	ADMA descriptors, one for each destination in DTU
 */
struct iwl_sdio_tx_dtu_hdr {
	struct iwl_sdio_cmd_header hdr;
	__le32 dma_desc;
	u8 reserved[8];
	struct iwl_sdio_adma_desc adma_list[0];
} __packed;

/**
 * struct iwl_sdio_tx_dtu_trailer - the trailer of DTU
 * @ptfd:	ptfd index
 * @scd_bc:	byte count in double words
 * @scd_bc_dup:	dupped byte count in double words
 * @scd_wr_ptr:	updated scheduler write pointer
 * @padding:	aligned to the size of the SDIO block, filled with 0xAC
 */
struct iwl_sdio_tx_dtu_trailer {
	__le32 ptfd;
	__le16 scd_bc0;
	__le16 scd_bc1;
	__le16 scd_bc0_dup;
	__le16 scd_bc1_dup;
	__le32 scd_wr_ptr;
	u8 padding[0];
} __packed;

/**
 * struct iwl_sdio_dtu_info - internal met data for DTU
 * @sram_alloc:		allocation indices
 * @data_pad_len:	ADMA descriptor can handle only data length aligned
 *			to 4 bytes. If the length of the data to be sent is not
 *			aligned, only the last TB (and corresponding ADMA desc)
 *			will suffer; In this case, ADMA desc will be configured
 *			to move more data (up to 3 bytes). This is the length of
 *			this padding.
 * @adma_desc_num:	total number of ADMA descriptors
 * @ctrl_buf:	holds DTU header, trailer and TFD
 */
struct iwl_sdio_dtu_info {
	struct iwl_sdio_sram_alloc sram_alloc;
	u32 data_pad_len;
	u8 adma_desc_num;
	struct iwl_sdio_tx_dtu_hdr ctrl_buf[0];
};

void iwl_sdio_tx_stop(struct iwl_trans *trans);
void iwl_sdio_tx_free(struct iwl_trans *trans);
void iwl_sdio_tx_start(struct iwl_trans *trans, u32 scd_base_addr);
int iwl_sdio_process_dtu(struct iwl_trans_slv *trans_slv, u8 txq_id);
void iwl_sdio_tx_calc_desc_num(struct iwl_trans *trans,
			       struct iwl_slv_tx_chunk_info *chunk_info);
int iwl_sdio_flush_dtus(struct iwl_trans *trans);
void iwl_sdio_tx_free_dtu_mem(struct iwl_trans *trans, void **data);
void iwl_sdio_tx_clean_dtu(struct iwl_trans *trans, void *data);
void iwl_trans_sdio_txq_enable(struct iwl_trans *trans, int txq_id, u16 ssn,
			       const struct iwl_trans_txq_scd_cfg *cfg,
			       unsigned int wdg_timeout);
void iwl_trans_sdio_txq_disable(struct iwl_trans *trans, int txq_id,
				bool configure_scd);

#endif
