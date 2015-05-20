/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2014 Intel Mobile Communications GmbH
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
 * Copyright(c) 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2014 Intel Mobile Communications GmbH
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
#ifndef __iwl_dnt_dev_if_h__
#define __iwl_dnt_dev_if_h__

#include "iwl-drv.h"
#include "iwl-trans.h"
#include "iwl-op-mode.h"
#include "iwl-config.h"

struct iwl_dnt;

#define DNT_LDBG_CMD_SIZE	80
#define DNT_MARBH_BUF_SIZE	(0x3cff * sizeof(u32))
#define DNT_SMEM_BUF_SIZE	(0x18004)

#define DNT_CHUNK_SIZE 512

/* marbh access types */
enum {
	ACCESS_TYPE_DIRECT = 0,
	ACCESS_TYPE_INDIRECT,
};

/**
 * iwl_dnt_dev_if_configure_monitor - configure monitor.
 *
 * configure the correct monitor configuration - depends on the monitor mode.
 */
int iwl_dnt_dev_if_configure_monitor(struct iwl_dnt *dnt,
				     struct iwl_trans *trans);

/**
 * iwl_dnt_dev_if_retrieve_monitor_data - retreive monitor data.
 *
 * retreive monitor data - depends on the monitor mode.
 * Note: monitor must be stopped in order to retreive data.
 */
int iwl_dnt_dev_if_retrieve_monitor_data(struct iwl_dnt *dnt,
					 struct iwl_trans *trans, u8 *buffer,
					 u32 buffer_size);
/**
 * iwl_dnt_dev_if_start_monitor - start monitor data.
 *
 * starts monitor - sends command to start monitor.
 */
int iwl_dnt_dev_if_start_monitor(struct iwl_dnt *dnt,
				 struct iwl_trans *trans);
/**
 * iwl_dnt_dev_if_set_log_level - set ucode messgaes log level.
 */
int iwl_dnt_dev_if_set_log_level(struct iwl_dnt *dnt,
				 struct iwl_trans *trans);

int iwl_dnt_dev_if_read_sram(struct iwl_dnt *dnt, struct iwl_trans *trans);

int iwl_dnt_dev_if_read_rx(struct iwl_dnt *dnt, struct iwl_trans *trans);

#endif
