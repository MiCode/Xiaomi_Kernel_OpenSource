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

#include "iwl-io.h"
#include "sdio_internal.h"
#include "sdio_tx.h"
#include "sdio_tx_policy.h"

#define SDIO_CMD_QUOTA 2
#define SDIO_CMD_TFD_QUOTA 1
#define SDIO_CMD_TB_QUOTA 3

/**
 * iwl_sdio_tx_policy_check_alloc() - check according to available
 * and required resources that allocation request can be satisfied.
 * @trans:	the transport
 * @avail_tfds:	TFDs available in transport
 * @avail_tbs:	TBs available in transport
 * @req_tfds:	how much TFDs are requested
 * @req_tbs:	how much TBs are requested
 *
 * Since policy is only an algorithm and doesn't manages or holds any
 * resources, it should receive all the data required for the decision.
 * This function is usually called by transport.
 *
 * Retuns true if the resources can be allocated, false otherwise.
 */
bool iwl_sdio_policy_check_alloc(struct iwl_trans_slv *trans_slv,
				 u8 txq_id,
				 int avail_tfds, int avail_tbs,
				 int req_tfds, int req_tbs)
{
	if (txq_id != trans_slv->cmd_queue) {
		avail_tfds -= SDIO_CMD_TFD_QUOTA;
		avail_tbs -= SDIO_CMD_TB_QUOTA;
	}

	if ((req_tfds <= avail_tfds) && (req_tbs <= avail_tbs))
		return true;

	return false;
}

void iwl_sdio_tx_policy_trigger(struct work_struct *data)
{
	struct iwl_trans_slv *trans_slv =
			container_of(data, struct iwl_trans_slv,
				     policy_trigger);
	struct iwl_trans *trans = IWL_TRANS_SLV_GET_IWL_TRANS(trans_slv);
	int ret, qidx;

	while (1) {
		qidx = iwl_slv_get_next_queue(trans_slv);
		if (qidx < 0)
			break;
		ret = iwl_sdio_process_dtu(trans_slv, qidx);
		IWL_DEBUG_TX(trans, "TxQ %d, ret process_dtu %d\n", qidx, ret);
		if (ret)
			break;
	}

	iwl_sdio_flush_dtus(trans);

	return;
}

