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

#ifndef __coex_h__
#define __coex_h__

#include <linux/types.h>

/* LTE-Coex protocol user space commands */

#define LTE_OFF	      0
#define LTE_IDLE      1
#define LTE_CONNECTED 2

/* LTE-Coex error codes */

#define LTE_OK 0
/* LTE state machine violation */
#define LTE_STATE_ERR 1
#define LTE_ILLEGAL_PARAMS 2
#define LTE_INVALID_DATA 3
#define LTE_OTHER_ERR 4


struct lte_coex_state_cmd {
	__u8 lte_state;
} __packed;

#define LTE_MWS_CONF_LENGTH 12
#define LTE_SAFE_PT_LENGTH 32
#define LTE_SAFE_PT_FIRST -128
#define LTE_SAFE_PT_LAST 127
struct lte_coex_config_info_cmd {
	__u32 mws_conf_data[LTE_MWS_CONF_LENGTH];
	__s8 safe_power_table[LTE_SAFE_PT_LENGTH];
} __packed;

#define LTE_CONNECTED_BANDS_LENGTH 8
#define LTE_FRAME_STRUCT_LENGTH 2
#define LTE_TX_POWER_LENGTH 14
#define LTE_FRQ_MIN 2400
#define LTE_FRQ_MAX 2500
#define LTE_MAX_TX_MIN 0
#define LTE_MAX_TX_MAX 31
struct lte_coex_dynamic_info_cmd {
	__u32 lte_connected_bands[LTE_CONNECTED_BANDS_LENGTH];
	__u32 lte_frame_structure[LTE_FRAME_STRUCT_LENGTH];
	__u16 wifi_tx_safe_freq_min;
	__u16 wifi_tx_safe_freq_max;
	__u16 wifi_rx_safe_freq_min;
	__u16 wifi_rx_safe_freq_max;
	__u8 wifi_max_tx_power[LTE_TX_POWER_LENGTH];
} __packed;

struct lte_coex_sps_info_cmd {
	__u32 sps_info;
} __packed;

#define LTE_RC_CHAN_MIN 1
#define LTE_RC_CHAN_MAX 14
#define LTE_RC_BW_MIN 0
#define LTE_RC_BW_MAX 3
struct lte_coex_wifi_reported_chan_cmd {
	__u8 chan;
	__u8 bandwidth;
} __packed;

#endif /* __coex_h__ */
