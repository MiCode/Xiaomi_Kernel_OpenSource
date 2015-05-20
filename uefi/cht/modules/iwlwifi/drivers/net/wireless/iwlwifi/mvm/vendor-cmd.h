/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2012 - 2014 Intel Corporation. All rights reserved.
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
#ifndef __VENDOR_CMD_H__
#define __VENDOR_CMD_H__

#define INTEL_OUI	0x001735

/**
 * enum iwl_mvm_vendor_cmd - supported vendor commands
 * @IWL_MVM_VENDOR_CMD_SET_LOW_LATENCY: set low-latency mode for the given
 *	virtual interface
 * @IWL_MVM_VENDOR_CMD_GET_LOW_LATENCY: query low-latency mode
 * @IWL_MVM_VENDOR_CMD_TCM_EVENT: TCM event
 * @IWL_MVM_VENDOR_CMD_LTE_STATE: inform the LTE modem state
 * @IWL_MVM_VENDOR_CMD_LTE_COEX_CONFIG_INFO: configure LTE-Coex static
 *	parameters
 * @IWL_MVM_VENDOR_CMD_LTE_COEX_DYNAMIC_INFO: configure LTE dynamic parameters
 * @IWL_MVM_VENDOR_CMD_LTE_COEX_SPS_INFO: configure semi oersistent info
 * @IWL_MVM_VENDOR_CMD_LTE_COEX_WIFI_RPRTD_CHAN: Wifi reported channel as
 *	calculated by the coex-manager
 * @IWL_MVM_VENDOR_CMD_SET_COUNTRY: set a new mcc regulatory information
 * @IWL_MVM_VENDOR_CMD_PROXY_FRAME_FILTERING: filter GTK, gratuitous
 *	ARP & unsolicited NA
 * @IWL_MVM_VENDOR_CMD_TDLS_PEER_CACHE_ADD: add a peer to the TDLS peer cache
 * @IWL_MVM_VENDOR_CMD_TDLS_PEER_CACHE_DEL: delete a peer from the TDLS peer
 *	cache
 * @IWL_MVM_VENDOR_CMD_TDLS_PEER_CACHE_QUERY: query traffic statistics for a
 *	peer in the TDLS cache
 * @IWL_MVM_VENDOR_CMD_SET_NIC_TXPOWER_LIMIT: set the NIC's (SAR) TX power limit
 */

enum iwl_mvm_vendor_cmd {
	IWL_MVM_VENDOR_CMD_SET_LOW_LATENCY,
	IWL_MVM_VENDOR_CMD_GET_LOW_LATENCY,
	IWL_MVM_VENDOR_CMD_TCM_EVENT,
	IWL_MVM_VENDOR_CMD_LTE_STATE,
	IWL_MVM_VENDOR_CMD_LTE_COEX_CONFIG_INFO,
	IWL_MVM_VENDOR_CMD_LTE_COEX_DYNAMIC_INFO,
	IWL_MVM_VENDOR_CMD_LTE_COEX_SPS_INFO,
	IWL_MVM_VENDOR_CMD_LTE_COEX_WIFI_RPRTD_CHAN,
	IWL_MVM_VENDOR_CMD_SET_COUNTRY,
	IWL_MVM_VENDOR_CMD_PROXY_FRAME_FILTERING,
	IWL_MVM_VENDOR_CMD_TDLS_PEER_CACHE_ADD,
	IWL_MVM_VENDOR_CMD_TDLS_PEER_CACHE_DEL,
	IWL_MVM_VENDOR_CMD_TDLS_PEER_CACHE_QUERY,
	IWL_MVM_VENDOR_CMD_SET_NIC_TXPOWER_LIMIT,
};

/**
 * enum iwl_mvm_vendor_load - traffic load identifiers
 * @IWL_MVM_VENDOR_LOAD_LOW: low load: less than 10% airtime usage
 * @IWL_MVM_VENDOR_LOAD_MEDIUM: medium load: 10% or more, but less than 50%
 * @IWL_MVM_VENDOR_LOAD_HIGH: high load: 50% or more
 *
 * Traffic load is calculated based on the percentage of airtime used
 * (TX airtime is accounted as RTS+CTS+PPDU+ACK/BlockACK, RX airtime
 * is just the PPDU's time)
 */
enum iwl_mvm_vendor_load {
	IWL_MVM_VENDOR_LOAD_LOW,
	IWL_MVM_VENDOR_LOAD_MEDIUM,
	IWL_MVM_VENDOR_LOAD_HIGH,
};

/**
 * enum iwl_mvm_vendor_attr - attributes used in vendor commands
 * @__IWL_MVM_VENDOR_ATTR_INVALID: attribute 0 is invalid
 * @IWL_MVM_VENDOR_ATTR_LOW_LATENCY: low-latency flag attribute
 * @IWL_MVM_VENDOR_ATTR_VIF_ADDR: interface MAC address
 * @IWL_MVM_VENDOR_ATTR_VIF_LL: vif-low-latency (u8, 0/1)
 * @IWL_MVM_VENDOR_ATTR_LL: global low-latency (u8, 0/1)
 * @IWL_MVM_VENDOR_ATTR_VIF_LOAD: vif traffic load (u8, see load enum)
 * @IWL_MVM_VENDOR_ATTR_LOAD: global traffic load (u8, see load enum)
 * @IWL_MVM_VENDOR_ATTR_COUNTRY: MCC to set, for regulatory information (u16)
 * IWL_MVM_VENDOR_ATTR_FILTER_ARP_NA: filter gratuitous ARP and unsolicited
 *	Neighbor Advertisement frames
 * IWL_MVM_VENDOR_ATTR_FILTER_GTK: filter Filtering Frames Encrypted using
 *	the GTK
 * @IWL_MVM_VENDOR_ATTR_ADDR: MAC address
 * @IWL_MVM_VENDOR_ATTR_TX_BYTES: number of bytes transmitted to peer
 * @IWL_MVM_VENDOR_ATTR_RX_BYTES: number of bytes received from peer
 * @IWL_MVM_VENDOR_ATTR_TXP_LIMIT_24: TX power limit for 2.4 GHz
 *	(s32 in units of 1/8 dBm)
 * @IWL_MVM_VENDOR_ATTR_TXP_LIMIT_52L: TX power limit for 5.2 GHz low (as 2.4)
 * @IWL_MVM_VENDOR_ATTR_TXP_LIMIT_52H: TX power limit for 5.2 GHz high (as 2.4)
 *
 * @NUM_IWL_MVM_VENDOR_ATTR: number of vendor attributes
 * @MAX_IWL_MVM_VENDOR_ATTR: highest vendor attribute number
 */
enum iwl_mvm_vendor_attr {
	__IWL_MVM_VENDOR_ATTR_INVALID,
	IWL_MVM_VENDOR_ATTR_LOW_LATENCY,
	IWL_MVM_VENDOR_ATTR_VIF_ADDR,
	IWL_MVM_VENDOR_ATTR_VIF_LL,
	IWL_MVM_VENDOR_ATTR_LL,
	IWL_MVM_VENDOR_ATTR_VIF_LOAD,
	IWL_MVM_VENDOR_ATTR_LOAD,
	IWL_MVM_VENDOR_ATTR_COUNTRY,
	IWL_MVM_VENDOR_ATTR_FILTER_ARP_NA,
	IWL_MVM_VENDOR_ATTR_FILTER_GTK,
	IWL_MVM_VENDOR_ATTR_ADDR,
	IWL_MVM_VENDOR_ATTR_TX_BYTES,
	IWL_MVM_VENDOR_ATTR_RX_BYTES,
	IWL_MVM_VENDOR_ATTR_TXP_LIMIT_24,
	IWL_MVM_VENDOR_ATTR_TXP_LIMIT_52L,
	IWL_MVM_VENDOR_ATTR_TXP_LIMIT_52H,

	NUM_IWL_MVM_VENDOR_ATTR,
	MAX_IWL_MVM_VENDOR_ATTR = NUM_IWL_MVM_VENDOR_ATTR - 1,
};
#define IWL_MVM_VENDOR_FILTER_ARP_NA IWL_MVM_VENDOR_ATTR_FILTER_ARP_NA
#define IWL_MVM_VENDOR_FILTER_GTK IWL_MVM_VENDOR_ATTR_FILTER_GTK
#endif /* __VENDOR_CMD_H__ */
