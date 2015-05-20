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
#include <linux/kernel.h>
#include <linux/gfp.h>

#include <linux/mmc/sdio_func.h>

#include "iwl-op-mode.h"
#include "sdio_internal.h"
#include "iwl-csr.h"
#include "iwl-fh.h"
#include "iwl-io.h"
#include "shared.h"

#define ISR_MAX_LOOPS 64

/*
 * Checks the RX packet for validity.
 * If hte packet fails then an error status is returned indicating that the
 * RX packet data is probably not valid and should be ignored.
 */
static int iwl_sdio_validate_rx_packet(struct iwl_trans *trans,
				       void *rb, u32 rd_count)
{
	struct iwl_sdio_cmd_header *sdio_cmd_hdr = rb;
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);

	/* Hex dump the rx data if enabled */
	if (trans_sdio->print_rx_hex_dump) {
		IWL_ERR(trans, "Rx data:\n");
		iwl_print_hex_error(trans, rb, rd_count);
	}

	/* Validate the Signature*/
	if (le16_to_cpu(sdio_cmd_hdr->signature) !=
	    IWL_SDIO_CMD_HEADER_SIGNATURE) {
		IWL_ERR(trans, "RX: bad signature - 0x%.2x != 0x%.2x\n",
			sdio_cmd_hdr->signature,
			IWL_SDIO_CMD_HEADER_SIGNATURE);
		return -EIO;
	}

	/* The EOT bit *must* be set */
	if (!(sdio_cmd_hdr->op_code & IWL_SDIO_EOT_BIT))
		return -EIO;

	/* TODO: need to add size checks */

	return 0;
}

/*
 * Allocate a buffer for the RX message.
 *
 * Allocates the memory descriptor, and required page.
 * Adds the memory descriptor to an internal list and returns it.
 */
static struct iwl_sdio_rx_mem_desc *
iwl_sdio_alloc_rx_buffer(struct iwl_trans *trans, u32 rd_count)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	struct iwl_sdio_rx_mem_desc *rx_buff;
	gfp_t gfp_mask = GFP_KERNEL;

	/* Allocate mem descriptor */
	rx_buff = kmem_cache_alloc(trans_sdio->rx_mem_desc_pool, gfp_mask);
	if (unlikely(!rx_buff))
		return NULL;

	rx_buff->length = rd_count;
	rx_buff->page_order = get_order(rx_buff->length);

	/* Set the order and flags of the buffer */
	if (rx_buff->page_order > 0)
		gfp_mask |= __GFP_COMP;

	/* Allocate the page */
	rx_buff->page = alloc_pages(gfp_mask, rx_buff->page_order);
	if (!rx_buff->page) {
		kmem_cache_free(trans_sdio->rx_mem_desc_pool, rx_buff);
		return NULL;
	}

	memset(page_address(rx_buff->page), 0, rx_buff->length);

	return rx_buff;
}

/*
 * Free an RX buffer.
 *
 * Frees the page, removes the buffer from the list of allocated rx buffers
 * and returns the descriptor to the pool.
 */
static void iwl_sdio_free_rx_buffer(struct iwl_trans *trans,
				    struct iwl_sdio_rx_mem_desc *rx_buff)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);

	/* Free internal page */
	if (rx_buff->page) {
		__free_pages(rx_buff->page, get_order(rx_buff->length));
		rx_buff->page = NULL;
	}

	/* rx_buff is either in a temporary local list, rx_mem_buff_list just
	 * before freeing trans_sdio, or none at all. In all of these cases
	 * there's no need to remove it, so don't.
	 */

	/* Return the memory descriptor to the cache */
	kmem_cache_free(trans_sdio->rx_mem_desc_pool, rx_buff);
}

/* Handle an RX data path packet. */
static void iwl_sdio_rx_handle_rb(struct iwl_trans *trans,
				  struct iwl_sdio_rx_mem_desc *rxmd)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	struct iwl_sdio_rx_buffer *rxb = page_address(rxmd->page);
	u32 len;
	bool page_stolen = false;
	u32 offset = 0;

	offset = sizeof(struct iwl_sdio_rx_cmd);
	while (offset < le32_to_cpu(rxb->rx_cmd.length)) {
		struct iwl_rx_cmd_buffer rxcb = {
			._offset = offset,
			._page = rxmd->page,
			._page_stolen = false,
			._rx_page_order = rxmd->page_order,
			.truesize = PAGE_SIZE << rxmd->page_order,
		};
		struct iwl_rx_packet *pkt = rxb_addr(&rxcb);

		if (pkt->len_n_flags == cpu_to_le32(FH_RSCSR_FRAME_INVALID))
			break;

		IWL_DEBUG_RX(trans,
			     "Handling RX packet %s (#%x, ofs %d, seq 0x%x)\n",
			     get_cmd_string(trans_sdio, pkt->hdr.cmd),
			     pkt->hdr.cmd, offset,
			     le16_to_cpu(pkt->hdr.sequence));

		if (pkt->hdr.cmd == 0xAC) {
			IWL_ERR(trans, "Rx error - reached 0xAC in RB\n");
			break;
		}

		/* Calculate length and trace */
		len = iwl_rx_packet_len(pkt);
		len += sizeof(u32); /* account for status word */

		/* Dispatch to op mode rx handler */
		iwl_slv_rx_handle_dispatch(trans, &rxcb);

		page_stolen |= rxcb._page_stolen;
		offset += len;
	}

	if (page_stolen) {
		__free_pages(rxmd->page, get_order(rxmd->length));
		rxmd->page = NULL;
	}

	/* Make sure all packets eventually get processed */
	if (trans_sdio->napi.poll) {
		local_bh_disable();
		napi_gro_flush(&trans_sdio->napi, false);
		local_bh_enable();
	}
}

void iwl_sdio_rx_work(struct work_struct *work)
{
	struct iwl_trans_sdio *trans_sdio = container_of(work,
							 struct iwl_trans_sdio,
							 rx_work);
	struct iwl_trans *trans = trans_sdio->trans;
	struct list_head local_list;
	struct iwl_sdio_rx_mem_desc *rx_buff, *next;
	void *page_addr;
	struct iwl_sdio_cmd_header *sdio_cmd_hdr;

	INIT_LIST_HEAD(&local_list);
	mutex_lock(&trans_sdio->rx_buff_mtx);
	list_splice_init(&trans_sdio->rx_mem_buff_list, &local_list);
	mutex_unlock(&trans_sdio->rx_buff_mtx);

	list_for_each_entry_safe(rx_buff, next, &local_list, list) {
		/* According to the OP code decide on the mode of operation */
		page_addr = page_address(rx_buff->page);
		sdio_cmd_hdr = page_addr;
		switch (sdio_cmd_hdr->op_code & IWL_SDIO_OP_CODE_MSK) {
		case IWL_SDIO_OP_CODE_READ:
			WARN_ON(rx_buff->length != IWL_SDIO_BLOCK_SIZE);
			iwl_sdio_handle_ta_read_ready(trans, page_addr);
			break;
		case IWL_SDIO_OP_CODE_RX_DATA:
			iwl_sdio_rx_handle_rb(trans, rx_buff);
			break;
		default:
			WARN(true, "BAD OP_CODE in Rx: %d\n",
			     sdio_cmd_hdr->op_code);
		}

		/* FIXME: reuse this page when possible (like PCIe?), just free
		 * now
		 */
		iwl_sdio_free_rx_buffer(trans, rx_buff);
	}
}

/*
 * Handle the data notification ready from the SDTM
 *
 * Reads the byte count and pulls the data from the LMAC.
 * Redirects to the required handler according to op code in RX buffer.
 */
static int iwl_sdio_handle_data_ready(struct iwl_trans *trans)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	struct iwl_sdio_rx_mem_desc *rx_buff;
	u32 rd_count;
	void *page_addr;
	int ret;

	/* Read the byte count of the rx data */
	rd_count = iwl_sdio_read8(trans, IWL_SDIO_READ_COUNT_BYTE_1, &ret);
	if (ret) {
		IWL_ERR(trans, "Failed to read READ_COUNT register\n");
		return -EIO;
	}
	IWL_DEBUG_RX(trans, "SDIO RB read count: %d KB\n", rd_count >> 2);

	/*
	 * We didn't read the LSB since we know that it is 0, shift by 8
	 * to get real size, and align to 512 bytes.
	 */
	rd_count = ALIGN(rd_count << 8, IWL_SDIO_BLOCK_SIZE);

	/* Allocate the new receive buffer */
	rx_buff = iwl_sdio_alloc_rx_buffer(trans, rd_count);
	if (!rx_buff)
		return -ENOMEM;

	/*
	 * Read the data from the SDTM.
	 * TODO: windows zero the first DWORD...
	 */
	page_addr = page_address(rx_buff->page);
	ret = sdio_readsb(IWL_TRANS_SDIO_GET_FUNC(trans), page_addr,
			  IWL_SDIO_DATA_ADDR, rd_count);
	if (WARN_ON(ret)) {
		ret = -EIO;
		goto error;
	}
	/* Validate the RX packet */
	if (iwl_sdio_validate_rx_packet(trans, page_addr, rd_count)) {
		ret = -EINVAL;
		goto error;
	}

	/* Add the allocated buffer to the list */
	mutex_lock(&trans_sdio->rx_buff_mtx);
	list_add_tail(&rx_buff->list, &trans_sdio->rx_mem_buff_list);
	mutex_unlock(&trans_sdio->rx_buff_mtx);

	schedule_work(&trans_sdio->rx_work);
	return 0;

error:
	iwl_sdio_free_rx_buffer(trans, rx_buff);
	return ret;
}

static const char *get_msg_string(int cmd)
{
#define IWL_CASE(x) case x: return #x

	switch (cmd) {
	IWL_CASE(IWL_SDIO_MSG_SW_GP0);
	IWL_CASE(IWL_SDIO_MSG_SW_GP1);
	IWL_CASE(IWL_SDIO_MSG_SW_GP2);
	IWL_CASE(IWL_SDIO_MSG_SW_GP3);
	IWL_CASE(IWL_SDIO_MSG_SW_GP4);
	IWL_CASE(IWL_SDIO_MSG_SW_GP5);
	IWL_CASE(IWL_SDIO_MSG_SW_GP6);
	IWL_CASE(IWL_SDIO_MSR_RFKILL);
	IWL_CASE(IWL_SDIO_MSG_WR_IN_LOW_RETENTION);
	IWL_CASE(IWL_SDIO_MSG_WR_ABORT);
	IWL_CASE(IWL_SDIO_MSG_RD_ABORT);
	IWL_CASE(IWL_SDIO_MSG_TARG_BAD_LEN);
	IWL_CASE(IWL_SDIO_MSG_TARG_BAD_ADDR);
	IWL_CASE(IWL_SDIO_MSG_TRANS_BAD_SIZE);
	IWL_CASE(IWL_SDIO_MSG_H2D_WDT_EXPIRE);
	IWL_CASE(IWL_SDIO_MSG_TARG_IN_PROGRESS);
	IWL_CASE(IWL_SDIO_MSG_BAD_OP_CODE);
	IWL_CASE(IWL_SDIO_MSG_BAD_SIG);
	IWL_CASE(IWL_SDIO_MSG_GP_INT);
	IWL_CASE(IWL_SDIO_MSG_LMAC_SW_ERROR);
	IWL_CASE(IWL_SDIO_MSG_SCD_ERROR);
	IWL_CASE(IWL_SDIO_MSG_FH_TX_INT);
	IWL_CASE(IWL_SDIO_MSG_LMAC_HW_ERROR);
	default:
		return "UNKNOWN";
	}
}
#undef IWL_CASE

/* Called for HW or SW error interrupt from card */
static void iwl_sdio_handle_error(struct iwl_trans *trans)
{
	struct iwl_trans_slv *trans_slv = IWL_TRANS_GET_SLV_TRANS(trans);

	iwl_dump_fh(trans, NULL);

	iwl_trans_fw_error(trans);

	clear_bit(STATUS_SYNC_HCMD_ACTIVE, &trans->status);
	wake_up(&trans_slv->wait_command_queue);
}

void iwl_sdio_d2h_work(struct work_struct *work)
{
	struct iwl_trans_sdio *trans_sdio = container_of(work,
							 struct iwl_trans_sdio,
							 d2h_work);
	struct iwl_trans *trans = trans_sdio->trans;
	struct sdio_func *func = IWL_TRANS_SDIO_GET_FUNC(trans);
	u32 err_val = 0;
	u8 *p_err_val = (u8 *)&err_val;
	int i;
	int ret;

	/* Read the D2H message byte-by-byte due to a HW issue */
	sdio_claim_host(func);
	for (i = 0; i < sizeof(u32); i++) {
		p_err_val[i] = iwl_sdio_read8(trans, IWL_SDIO_D2H_GP_REG + i,
					      &ret);
		if (ret) {
			IWL_ERR(trans,
				"Failed to read the D2H GP MSG, ret %d\n",
				ret);
			sdio_release_host(func);
			return;
		}
	}
	sdio_release_host(func);

	/* D2H messages must be acklowedged, even if they are unhandled, or all
	 * messages will be masked.
	 * This is done by clearing the matching bits from CSR_INT by writing to
	 * them. Not all D2H messages have corresponding bits in CSR_INT, but
	 * the ones that do have the same offsets.
	 * The sdio host will be claimed again, but the lock can be acquired
	 * recursively
	 */
	iwl_write_direct32(trans, CSR_INT, err_val & IWL_SDIO_MSG_INTA_CSR_ALL);

	/* Decode the message */
	IWL_DEBUG_ISR(trans, "D2H Message (0x%08x):\n", err_val);
	for (i = 0; i < sizeof(u32) * BITS_PER_BYTE; i++) {
		if (!(err_val & BIT(i)))
			continue;
		IWL_DEBUG_ISR(trans, "%s\n",
			      get_msg_string(err_val & BIT(i)));
	}

	if (err_val & IWL_SDIO_MSG_LMAC_SW_ERROR) {
		IWL_ERR(trans, "SW error detected\n");
		iwl_sdio_handle_error(trans);
	}

	if (err_val & IWL_SDIO_MSG_LMAC_HW_ERROR) {
		IWL_ERR(trans, "HW error detected\n");
		iwl_sdio_handle_error(trans);
	}
}

/**
 * iwl_sdio_isr() - ISR handler function.
 * @func: The SDIO HW function bus driver.
 */
void iwl_sdio_isr(struct sdio_func *func)
{
	struct iwl_trans *trans = sdio_get_drvdata(func);
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	u8 val;
	int count = 0;
	int ret;

	do {
		/* Read the interrupt cause */
		val = iwl_sdio_read8(trans, IWL_SDIO_INTR_CAUSE_REG, &ret);
		if (ret) {
			IWL_ERR(trans,
				"Failed to read from INTR_CAUSE_REG, ret 0x%x\n",
				ret);
			return;
		}
		IWL_DEBUG_ISR(trans, "SDIO interrupt called with value %d\n",
			      val);

		if (!val) {
			if (!count)
				IWL_ERR(trans,
					"An interrupt was called but with no cause\n");
			return;
		}

		/*
		 * SDTM CSR register to enable read optimization on 8000 family
		 * B-step and onwards is currently disabled due to some HW/FW
		 * issue. In this optimization there is no need to clear the
		 * IWL_SDIO_INTR_CAUSE_REG
		 */
		ret = iwl_sdio_write8(trans, IWL_SDIO_INTR_CAUSE_REG, val);
		if (ret)
			IWL_ERR(trans,
				"Failed to clear the int val %d, reason %d\n",
				val, ret);


		if (val & IWL_SDIO_INTR_READ_ERROR) {
			IWL_ERR(trans, "Got err interrupt...\n");
			return;
		}

		if (!(val & IWL_SDIO_INTR_CAUSE_VALID_MASK)) {
			IWL_ERR(trans, "No known interrupt occurred 0x08%x\n",
				val);
			return;
		}

		/*
		 * Interrupts Cause handling
		 */

		/* Device has message to the host */
		if (val & IWL_SDIO_INTR_D2H_GPR_MSG) {
			IWL_ERR(trans, "Got D2H message, queuing work\n");
			schedule_work(&trans_sdio->d2h_work);
		}

		/* Device Acked last message */
		if (val & IWL_SDIO_INTR_H2D_GPR_MSG_ACK)
			IWL_DEBUG_ISR(trans, "Device Acked the message\n");

		/* Target Access read/RX data ready */
		if (val & IWL_SDIO_INTR_DATA_READY) {
			ret = iwl_sdio_handle_data_ready(trans);
			if (ret) {
				IWL_ERR(trans, "Error in handling RX\n");
				return;
			}
		}
	} while (++count < ISR_MAX_LOOPS);
}

/*
 * Free all of the RX memory used in the driver SDIO flows.
 *
 * Must be done before freeing the cache pool of the RX memory  descriptors.
 */
void iwl_sdio_free_rx_mem(struct iwl_trans *trans)
{
	struct iwl_sdio_rx_mem_desc *pos, *next;
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);

	/* Free all the left RX memory descriptors*/
	list_for_each_entry_safe(pos, next, &trans_sdio->rx_mem_buff_list, list)
		iwl_sdio_free_rx_buffer(trans, pos);

}
