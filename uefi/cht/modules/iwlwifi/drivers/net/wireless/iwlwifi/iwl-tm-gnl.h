/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2010 - 2014 Intel Corporation. All rights reserved.
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
 * Copyright(c) 2010 - 2014 Intel Corporation. All rights reserved.
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
#ifndef __IWL_TM_GNL_H__
#define __IWL_TM_GNL_H__

#include <linux/types.h>


struct iwl_test_trace {
	u32 size;
	u8 *cpu_addr;
	dma_addr_t dma_addr;
	bool enabled;
};

struct iwl_test {
	struct iwl_test_trace trace;
	bool notify;
};


/**
 * struct iwl_tm_gnl_dev - Devices data base
 * @list:	  Linked list to all devices
 * @trans:	  Pointer to the owning transport
 * @dev_name:	  Pointer to the device name
 * @cmd_handlers: Operation mode specific command handlers.
 *
 * Used to retrieve a device op mode pointer.
 * Device identifier it's name.
 */
struct iwl_tm_gnl_dev {
	struct list_head list;
	struct iwl_test tst;
	struct iwl_dnt *dnt;
	struct iwl_trans *trans;
	const char *dev_name;
	u32 nl_events_portid;
};

/**
 * iwl_tm_data - A data packet for testmode usages
 * @data:   Pointer to be casted to relevant data type
 *          (According to usage)
 * @len:    Size of data in bytes
 *
 * This data structure is used for sending/receiving data packets
 * between internal testmode interfaces
 */
struct iwl_tm_data {
	void *data;
	u32 len;
};

#ifdef CPTCFG_IWLWIFI_DEVICE_TESTMODE

int iwl_tm_gnl_send_msg(struct iwl_trans *trans, u32 cmd, bool check_notify,
			void *data_out, u32 data_len, gfp_t flags);

void iwl_tm_gnl_add(struct iwl_trans *trans);
void iwl_tm_gnl_remove(struct iwl_trans *trans);

int iwl_tm_gnl_init(void);
int iwl_tm_gnl_exit(void);

#else

static inline int iwl_tm_gnl_send_msg(struct iwl_trans *trans, u32 cmd,
				      bool check_notify, void *data_out,
				      u32 data_len, gfp_t flags)
{
	return 0;
}

static inline void iwl_tm_gnl_add(struct iwl_trans *trans)
{
}

static inline void iwl_tm_gnl_remove(struct iwl_trans *trans)
{
}

static inline int iwl_tm_gnl_init(void)
{
	return 0;
}

static inline int iwl_tm_gnl_exit(void)
{
	return 0;
}

#endif

#define ADDR_IN_AL_MSK (0x80000000)
#define GET_AL_ADDR(ofs) (ofs & ~(ADDR_IN_AL_MSK))
#define IS_AL_ADDR(ofs) (!!(ofs & (ADDR_IN_AL_MSK)))

#endif
