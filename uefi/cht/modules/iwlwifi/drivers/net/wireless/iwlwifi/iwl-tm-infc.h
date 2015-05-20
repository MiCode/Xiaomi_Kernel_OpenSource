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
#ifndef __iwl_tm_infc__
#define __iwl_tm_infc__

#include <linux/types.h>

/*
 * Testmode GNL family command.
 * There is only one NL command, not to be
 * confused with testmode commands
 */
enum iwl_tm_gnl_cmd_t {
	IWL_TM_GNL_CMD_EXECUTE = 0,
	IWL_TM_GNL_CMD_SUBSCRIBE_EVENTS,
};


/* uCode trace buffer */
#define TRACE_BUFF_SIZE_MAX	0x200000
#define TRACE_BUFF_SIZE_MIN	0x1000
#define TRACE_BUFF_SIZE_DEF	0x20000

#define TM_CMD_BASE		0x100
#define TM_CMD_NOTIF_BASE	0x200
#define XVT_CMD_BASE		0x300
#define XVT_CMD_NOTIF_BASE	0x400
#define XVT_BUS_TESTER_BASE	0x500

/*
 * Periphery registers absolute lower bound. This is used in order to
 * differentiate registery access through HBUS_TARG_PRPH_.* and
 * HBUS_TARG_MEM_* accesses.
 */
#define IWL_ABS_PRPH_START (0xA00000)

/* User-Driver interface commands */
enum {
	IWL_TM_USER_CMD_HCMD = TM_CMD_BASE,
	IWL_TM_USER_CMD_REG_ACCESS,
	IWL_TM_USER_CMD_SRAM_WRITE,
	IWL_TM_USER_CMD_SRAM_READ,
	IWL_TM_USER_CMD_GET_DEVICE_INFO,
	IWL_TM_USER_CMD_GET_DEVICE_STATUS,
	IWL_TM_USER_CMD_BEGIN_TRACE,
	IWL_TM_USER_CMD_END_TRACE,
	IWL_TM_USER_CMD_TRACE_DUMP,
	IWL_TM_USER_CMD_NOTIFICATIONS,
	IWL_TM_USER_CMD_SWITCH_OP_MODE,
	IWL_TM_USER_CMD_GET_SIL_STEP,
	IWL_TM_USER_CMD_GET_DRIVER_BUILD_INFO,

	IWL_TM_USER_CMD_NOTIF_UCODE_RX_PKT = TM_CMD_NOTIF_BASE,
	IWL_TM_USER_CMD_NOTIF_DRIVER,
	IWL_TM_USER_CMD_NOTIF_RX_HDR,
	IWL_TM_USER_CMD_NOTIF_COMMIT_STATISTICS,
	IWL_TM_USER_CMD_NOTIF_PHY_DB,
	IWL_TM_USER_CMD_NOTIF_DTS_MEASUREMENTS,
	IWL_TM_USER_CMD_NOTIF_MONITOR_DATA,
	IWL_TM_USER_CMD_NOTIF_UCODE_MSGS_DATA,
	IWL_TM_USER_CMD_NOTIF_APMG_PD,
	IWL_TM_USER_CMD_NOTIF_RETRIEVE_MONITOR,
	IWL_TM_USER_CMD_NOTIF_CRASH_DATA,
	IWL_TM_USER_CMD_NOTIF_BFE,
};

/*
 * xVT commands indeces start where common
 * testmode commands indeces end
 */
enum {
	IWL_XVT_CMD_START = XVT_CMD_BASE,
	IWL_XVT_CMD_STOP,
	IWL_XVT_CMD_CONTINUE_INIT,
	IWL_XVT_CMD_GET_PHY_DB_ENTRY,
	IWL_XVT_CMD_SET_CONFIG,
	IWL_XVT_CMD_GET_CONFIG,
	IWL_XVT_CMD_MOD_TX,
	IWL_XVT_CMD_RX_HDRS_MODE,
	IWL_XVT_CMD_ALLOC_DMA,
	IWL_XVT_CMD_GET_DMA,
	IWL_XVT_CMD_FREE_DMA,
	IWL_XVT_CMD_GET_CHIP_ID,
	IWL_XVT_CMD_APMG_PD_MODE,

	/* Driver notifications */
	IWL_XVT_CMD_SEND_REPLY_ALIVE = XVT_CMD_NOTIF_BASE,
	IWL_XVT_CMD_SEND_RFKILL,
	IWL_XVT_CMD_SEND_NIC_ERROR,

	/* Bus Tester Commands*/
	IWL_TM_USER_CMD_SV_BUS_CONFIG = XVT_BUS_TESTER_BASE,
	IWL_TM_USER_CMD_SV_BUS_RESET,
	IWL_TM_USER_CMD_SV_IO_TOGGLE,
	IWL_TM_USER_CMD_SV_GET_STATUS,
	IWL_TM_USER_CMD_SV_RD_WR_UINT8,
	IWL_TM_USER_CMD_SV_RD_WR_UINT32,
	IWL_TM_USER_CMD_SV_RD_WR_BUFFER,
};

enum {
	NOTIFICATIONS_DISABLE = 0,
	NOTIFICATIONS_ENABLE = 1,
};

/**
 * struct iwl_tm_cmd_request - Host command request
 * @id:		Command ID
 * @want_resp:	True value if response is required, else false (0)
 * @len:	Data length
 * @data:	For command in, casted to iwl_host_cmd
 *		rx packet when structure is used for command response.
 */
struct iwl_tm_cmd_request {
	__u32 id;
	__u32 want_resp;
	__u32 len;
	__u8 data[];
} __packed __aligned(4);

/**
 * struct iwl_svt_sdio_enable - SV Tester SDIO bus enable command
 * @enable:	Function enable/disable 1/0
 */
struct iwl_tm_sdio_io_toggle {
	__u32 enable;
} __packed __aligned(4);

/* Register operations - Operation type */
enum {
	IWL_TM_REG_OP_READ = 0,
	IWL_TM_REG_OP_WRITE,
	IWL_TM_REG_OP_MAX
};

/**
 * struct iwl_tm_reg_op - Single register operation data
 * @op_type:	READ/WRITE
 * @address:	Register address
 * @value:	Write value, or read result
 */
struct iwl_tm_reg_op {
	__u32 op_type;
	__u32 address;
	__u32 value;
} __packed __aligned(4);

/**
 * struct iwl_tm_regs_request - Register operation request data
 * @num:	number of operations in struct
 * @reg_ops:	Array of register operations
 */
struct iwl_tm_regs_request {
	__u32 num;
	struct iwl_tm_reg_op reg_ops[];
} __packed __aligned(4);

/**
 * struct iwl_tm_trace_request - Data for trace begin requests
 * @size:	Requested size of trace buffer
 * @addr:	Resulting DMA address of trace buffer LSB
 */
struct iwl_tm_trace_request {
	__u64 addr;
	__u32 size;
} __packed __aligned(4);

/**
 * struct iwl_tm_sram_write_request
 * @offest:	Address offset
 * @length:	input data length
 * @buffer:	input data
 */
struct iwl_tm_sram_write_request {
	__u32 offset;
	__u32 len;
	__u8 buffer[];
} __packed __aligned(4);

/**
 * struct iwl_tm_sram_read_request
 * @offest:	Address offset
 * @length:	data length
 */
struct iwl_tm_sram_read_request {
	__u32 offset;
	__u32 length;
} __packed __aligned(4);

/**
 * struct iwl_tm_dev_info_req - Request data for get info request
 * @read_sv: rather or not read sv_srop
 */
struct iwl_tm_dev_info_req {
	__u32 read_sv;
} __packed __aligned(4);

/**
 * struct iwl_tm_dev_info - Result data for get info request
 * @dev_id:
 * @vendor_id:
 * @silicon:
 * @fw_ver:
 * @driver_ver:
 * @build_ver:
 */
struct iwl_tm_dev_info {
	__u32 dev_id;
	__u32 vendor_id;
	__u32 silicon_step;
	__u32 fw_ver;
	__u32 build_ver;
	__u8 driver_ver[];
} __packed __aligned(4);

/*
 * struct iwl_tm_thrshld_md - tx packet metadata that crosses a thrshld
 *
 * @monitor_collec_wind: the size of the window to collect the logs
 * @seq: packet sequence
 * @pkt_start: start time of triggering pkt
 * @pkt_end: end time of triggering pkt
 * @msrmnt: the tx latency of the pkt
 * @tid: tid of the pkt
 * @mode: recording mode (internal buffer or continuos recording).
 */
struct iwl_tm_thrshld_md {
	__u16 monitor_collec_wind;
	__u16 seq;
	__u32 pkt_start;
	__u32 pkt_end;
	__u32 msrmnt;
	__u16 tid;
	__u8 mode;
} __packed __aligned(4);

#define MAX_OP_MODE_LENGTH	16
/**
 * struct iwl_switch_op_mode - switch op_mode
 * @new_op_mode:	size of data
 */
struct iwl_switch_op_mode {
	__u8 new_op_mode[MAX_OP_MODE_LENGTH];
} __packed __aligned(4);

/**
 * struct iwl_sil_step - holds the silicon step
 * @silicon_step: the device silicon step
 */
struct iwl_sil_step {
	__u32 silicon_step;
} __packed __aligned(4);

#define MAX_DRIVER_VERSION_LEN	256
#define MAX_BUILD_DATE_LEN	32
/**
 * struct iwl_tm_build_info - Result data for get driver build info request
 * @driver_version: driver version in tree:branch:build:sha1
 * @branch_time: branch creation time
 * @build_time: build time
 */
struct iwl_tm_build_info {
	__u8 driver_version[MAX_DRIVER_VERSION_LEN];
	__u8 branch_time[MAX_BUILD_DATE_LEN];
	__u8 build_time[MAX_BUILD_DATE_LEN];
} __packed __aligned(4);

/* xVT defeinitions */

#define IWL_XVT_RFKILL_OFF	0
#define IWL_XVT_RFKILL_ON	1

struct iwl_xvt_user_calib_ctrl {
	__u32 flow_trigger;
	__u32 event_trigger;
} __packed __aligned(4);

#define IWL_USER_FW_IMAGE_IDX_INIT	0
#define IWL_USER_FW_IMAGE_IDX_REGULAR	1
#define IWL_USER_FW_IMAGE_IDX_WOWLAN	2
#define IWL_USER_FW_IMAGE_IDX_TYPE_MAX	3

/**
 * iwl_xvt_sw_cfg_request - Data for set SW stack configuration request
 * @load_mask: bit[0] = Init FW
 *             bit[1] = Runtime FW
 * @cfg_mask:  Mask for which calibrations to regard
 * @phy_config: PHY_CONFIGURATION command paramter
 *              Used only for "Get SW CFG"
 * @get_calib_type: Used only in "Get SW CFG"
 *                  0: Get FW original calib ctrl
 *                  1: Get actual calib ctrl
 * @calib_ctrl: Calibration control for each FW
 */
enum {
	IWL_XVT_GET_CALIB_TYPE_DEF = 0,
	IWL_XVT_GET_CALIB_TYPE_RUNTIME
};

struct iwl_xvt_sw_cfg_request {
	__u32 load_mask;
	__u32 cfg_mask;
	__u32 phy_config;
	__u32 get_calib_type;
	__u32 dbg_flags;
	struct iwl_xvt_user_calib_ctrl calib_ctrl[IWL_UCODE_TYPE_MAX];
} __packed __aligned(4);

/**
 * iwl_xvt_sw_cfg_request - Data for set SW stack configuration request
 * @type:	Type of DB section
 * @chg_id:	Channel Group ID, relevant only when
 *		type is CHG PAPD or CHG TXP calibrations
 * @size:	Size of result data, in bytes
 * @data:	Result entry data
 */
struct iwl_xvt_phy_db_request {
	__u32 type;
	__u32 chg_id;
	__u32 size;
	__u8 data[];
} __packed __aligned(4);

#define IWL_TM_STATION_COUNT	16

/**
 * struct iwl_tm_tx_request - Data transmission request
 * @times:	  Number of times to transmit the data.
 * @delay_us:	  Delay between frames
 * @pa_detect_en: Flag. When True, enable PA detector
 * @trigger_led:  Flag. When true, light led when transmitting
 * @len:	  Size of data buffer
 * @rate_flags:	  Tx Configuration rate flags
 * @sta_id:	  Station ID
 * @data:	  Data to transmit
 */
struct iwl_tm_mod_tx_request {
	__u32 times;
	__u32 delay_us;
	__u32 pa_detect_en;
	__u32 trigger_led;
	__u32 len;
	__u32 rate_flags;
	__u8 sta_id;
	__u8 data[];
} __packed __aligned(4);

/**
 * struct iwl_xvt_rx_hdrs_mode_request - Start/Stop gathering headers info.
 * @mode: 0 - stop
 *        1 - start
 */
struct iwl_xvt_rx_hdrs_mode_request {
	__u32 mode;
} __packed __aligned(4);

/**
 * struct iwl_xvt_apmg_pd_mode_request - Start/Stop gathering apmg_pd info.
 * @mode: 0 - stop
 *        1 - start
 */
struct iwl_xvt_apmg_pd_mode_request {
	__u32 mode;
} __packed __aligned(4);

/**
 * struct iwl_xvt_alloc_dma - Data for alloc dma requests
 * @addr:	Resulting DMA address of trace buffer LSB
 * @size:	Requested size of dma buffer
 */
struct iwl_xvt_alloc_dma {
	__u64 addr;
	__u32 size;
} __packed __aligned(4);

/**
 * struct iwl_xvt_alloc_dma - Data for alloc dma requests
 * @size:	size of data
 * @data:	Data to transmit
 */
struct iwl_xvt_get_dma {
	__u32 size;
	__u8 data[];
} __packed __aligned(4);

/**
 * struct iwl_xvt_chip_id - get the chip id from SCU
 * @registers:	an array of registers to hold the chip id data
 */
struct iwl_xvt_chip_id {
	__u32 registers[3];
} __packed __aligned(4);

/**
 * struct iwl_tm_crash_data - Notifications containing crash data
 * @data_type:	type of the data
 * @size:	data size
 * @data:	data
 */
struct iwl_tm_crash_data {
	__u32 size;
	__u8 data[];
} __packed __aligned(4);

#endif
