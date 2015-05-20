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

#ifndef __fw_api_h__
#define __fw_api_h__

#define IWL_XVT_DEFAULT_TX_QUEUE	0
#define IWL_XVT_CMD_QUEUE	9

#define IWL_XVT_DEFAULT_TX_FIFO	0
#define IWL_XVT_CMD_FIFO	7

enum {
	XVT_ALIVE = 0x1,
	INIT_COMPLETE_NOTIF = 0x4,
	APMG_PD_SV_CMD = 0x43,

	/* Tx */
	TX_CMD = 0x1C,

	/* Phy */
	PHY_CONFIGURATION_CMD = 0x6a,
	CALIB_RES_NOTIF_PHY_DB = 0x6b,

	NVM_COMMIT_COMPLETE_NOTIFICATION = 0xad,

	/* NVM */
	NVM_ACCESS_CMD = 0x88,

	GET_SET_PHY_DB_CMD = 0x8f,

	/* BFE */
	REPLY_HD_PARAMS_CMD = 0xa6,

	/* Rx command */
	REPLY_RX_PHY_CMD = 0xc0,
	REPLY_RX_MPDU_CMD = 0xc1,
	REPLY_RX_DSP_EXT_INFO = 0xc4,

	DTS_MEASUREMENT_NOTIFICATION = 0xdd,

	/* Debug commands */
	REPLY_DEBUG_XVT_CMD = 0xf3,
	/* monitor data notification */
	DEBUG_LOG_MSG = 0xf7,

	REPLY_MAX = 0xff,
};

/*
 * Calibration control struct.
 * Sent as part of the phy configuration command.
 * @flow_trigger: bitmap for which calibrations to perform according to
 *		flow triggers.
 * @event_trigger: bitmap for which calibrations to perform according to
 *		event triggers.
 */
struct iwl_calib_ctrl {
	__le32 flow_trigger;
	__le32 event_trigger;
} __packed;

/*
 * Phy configuration command.
 */
struct iwl_phy_cfg_cmd {
	__le32	phy_cfg;
	struct iwl_calib_ctrl calib_control;
} __packed;

/* XVT_ALIVE 0x1 */

#define IWL_ALIVE_STATUS_ERR 0xDEAD
#define IWL_ALIVE_STATUS_OK 0xCAFE

struct xvt_alive_resp {
	__le16 status;
	__le16 flags;
	u8 ucode_minor;
	u8 ucode_major;
	__le16 id;
	u8 api_minor;
	u8 api_major;
	u8 ver_subtype;
	u8 ver_type;
	u8 mac;
	u8 opt;
	__le16 reserved2;
	__le32 timestamp;
	__le32 error_event_table_ptr;	/* SRAM address for error log */
	__le32 log_event_table_ptr;	/* SRAM address for event log */
	__le32 cpu_register_ptr;
	__le32 dbgm_config_ptr;
	__le32 alive_counter_ptr;
	__le32 scd_base_ptr;		/* SRAM address for SCD */
} __packed; /* ALIVE_RES_API_S_VER_1 */

struct xvt_alive_resp_ver2 {
	__le16 status;
	__le16 flags;
	u8 ucode_minor;
	u8 ucode_major;
	__le16 id;
	u8 api_minor;
	u8 api_major;
	u8 ver_subtype;
	u8 ver_type;
	u8 mac;
	u8 opt;
	__le16 reserved2;
	__le32 timestamp;
	__le32 error_event_table_ptr;	/* SRAM address for error log */
	__le32 log_event_table_ptr;	/* SRAM address for LMAC event log */
	__le32 cpu_register_ptr;
	__le32 dbgm_config_ptr;
	__le32 alive_counter_ptr;
	__le32 scd_base_ptr;		/* SRAM address for SCD */
	__le32 st_fwrd_addr;		/* pointer to Store and forward */
	__le32 st_fwrd_size;
	u8 umac_minor;			/* UMAC version: minor */
	u8 umac_major;			/* UMAC version: major */
	__le16 umac_id;			/* UMAC version: id */
	__le32 error_info_addr;		/* SRAM address for UMAC error log */
	__le32 dbg_print_buff_addr;
} __packed; /* ALIVE_RES_API_S_VER_2 */

struct xvt_alive_resp_ver3 {
	__le16 status;
	__le16 flags;
	u32 ucode_minor;
	u32 ucode_major;

	u8 ver_subtype;
	u8 ver_type;
	u8 mac;
	u8 opt;
	__le32 timestamp;

	__le32 error_event_table_ptr;	/* SRAM address for error log */
	__le32 log_event_table_ptr;	/* SRAM address for LMAC event log */
	__le32 cpu_register_ptr;
	__le32 dbgm_config_ptr;
	__le32 alive_counter_ptr;
	__le32 scd_base_ptr;		/* SRAM address for SCD */
	__le32 st_fwrd_addr;		/* pointer to Store and forward */
	__le32 st_fwrd_size;

	u32 umac_minor;			/* UMAC version: minor */
	u32 umac_major;			/* UMAC version: major */

	__le32 error_info_addr;		/* SRAM address for UMAC error log */
	__le32 dbg_print_buff_addr;
} __packed; /* ALIVE_RES_API_S_VER_3 */

#define TX_CMD_LIFE_TIME_INFINITE	0xFFFFFFFF

#define TX_CMD_FLG_ACK_MSK cpu_to_le32(1 << 3)

struct iwl_tx_cmd {
	__le16 len;
	__le16 next_frame_len;
	__le32 tx_flags;
	/* DRAM_SCRATCH_API_U_VER_1 */
	u8 try_cnt;
	u8 btkill_cnt;
	__le16 reserved;
	__le32 rate_n_flags;
	u8 sta_id;
	u8 sec_ctl;
	u8 initial_rate_index;
	u8 reserved2;
	u8 key[16];
	__le16 next_frame_flags;
	__le16 reserved3;
	__le32 life_time;
	__le32 dram_lsb_ptr;
	u8 dram_msb_ptr;
	u8 rts_retry_limit;
	u8 data_retry_limit;
	u8 tid_tspec;
	__le16 pm_frame_timeout;
	__le16 driver_txop;
	u8 payload[0];
	struct ieee80211_hdr hdr[0];
} __packed; /* TX_CMD_API_S_VER_3 */

struct agg_tx_status {
	__le16 status;
	__le16 sequence;
} __packed;

struct iwl_xvt_tx_resp {
	u8 frame_count;
	u8 bt_kill_count;
	u8 failure_rts;
	u8 failure_frame;
	__le32 initial_rate;
	__le16 wireless_media_time;

	u8 pa_status;
	u8 pa_integ_res_a[3];
	u8 pa_integ_res_b[3];
	u8 pa_integ_res_c[3];
	__le16 measurement_req_id;
	__le16 reserved;

	__le32 tfd_info;
	__le16 seq_ctl;
	__le16 byte_cnt;
	u8 tlc_info;
	u8 ra_tid;
	__le16 frame_ctrl;

	struct agg_tx_status status;
} __packed; /* TX_RSP_API_S_VER_3 */

enum {
	XVT_DBG_GET_SVDROP_VER_OP = 0x01,
};

struct xvt_debug_cmd {
	__le32 opcode;
	__le32 dw_num;
}; /* DEBUG_XVT_CMD_API_S_VER_1 */

struct xvt_debug_res {
	__le32 dw_num;
	__le32 data[0];
}; /* DEBUG_XVT_RES_API_S_VER_1 */

static inline u32 iwl_xvt_get_scd_ssn(struct iwl_xvt_tx_resp *tx_resp)
{
	return le32_to_cpup((__le32 *)&tx_resp->status +
			    tx_resp->frame_count) & 0xfff;
}

/* Section types for NVM_ACCESS_CMD */
enum {
	NVM_SECTION_TYPE_HW = 0,
	NVM_SECTION_TYPE_SW,
	NVM_SECTION_TYPE_PAPD,
	NVM_SECTION_TYPE_BT,
	NVM_SECTION_TYPE_CALIBRATION,
	NVM_SECTION_TYPE_PRODUCTION,
	NVM_SECTION_TYPE_POST_FCS_CALIB,
	NVM_NUM_OF_SECTIONS,
};

/**
 * struct iwl_nvm_access_cmd_ver2 - Request the device to send an NVM section
 * @op_code: 0 - read, 1 - write
 * @target: NVM_ACCESS_TARGET_*
 * @type: NVM_SECTION_TYPE_*
 * @offset: offset in bytes into the section
 * @length: in bytes, to read/write
 * @data: if write operation, the data to write. On read its empty
 */
struct iwl_nvm_access_cmd {
	u8 op_code;
	u8 target;
	__le16 type;
	__le16 offset;
	__le16 length;
	u8 data[];
} __packed; /* NVM_ACCESS_CMD_API_S_VER_2 */

/**
 * struct iwl_nvm_access_resp_ver2 - response to NVM_ACCESS_CMD
 * @offset: offset in bytes into the section
 * @length: in bytes, either how much was written or read
 * @type: NVM_SECTION_TYPE_*
 * @status: 0 for success, fail otherwise
 * @data: if read operation, the data returned. Empty on write.
 */
struct iwl_nvm_access_resp {
	__le16 offset;
	__le16 length;
	__le16 type;
	__le16 status;
	u8 data[];
} __packed; /* NVM_ACCESS_CMD_RESP_API_S_VER_2 */

#endif /* __fw_api_h__ */
