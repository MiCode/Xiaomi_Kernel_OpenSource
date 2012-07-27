/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef _VCD_CORE_H_
#define _VCD_CORE_H_

#include <linux/ion.h>
#include <media/msm/vcd_api.h>
#include "vcd_ddl_api.h"

#include "vcd_util.h"
#include "vcd_client_sm.h"
#include "vcd_power_sm.h"

#define VCD_SIGNATURE                        0x75017591U

#define VCD_MIN_PERF_LEVEL                   37900

#define VCD_DRIVER_CLIENTS_MAX              6

#define VCD_MAX_CLIENT_TRANSACTIONS          32

#define VCD_MAX_BUFFER_ENTRIES               32

#define VCD_SEQ_HDR_PADDING_BYTES            256

#define VCD_DEC_NUM_INTERLACED_FIELDS        2

#define VCD_TIMESTAMP_RESOLUTION             1000000
#define VCD_DEC_INITIAL_FRAME_RATE           30
#define VCD_MAXPERF_FPS_THRESHOLD_X_1000     (59*1000)

#define VCD_FIRST_IP_RCVD                    0x00000004
#define VCD_FIRST_OP_RCVD                    0x00000008
#define VCD_EOS_PREV_VALID                   0x00000010
#define VCD_EOS_WAIT_OP_BUF                  0x00000020
#define VCD_CLEANING_UP                      0x00000040
#define VCD_STOP_PENDING                     0x00000080
#define VCD_CLOSE_PENDING                    0x00000100
#define VCD_IN_RECONFIG                      0x00000200
#define VCD_FIRST_IP_DONE                    0x00000400

enum vcd_command {
	VCD_CMD_NONE,
	VCD_CMD_DEVICE_INIT,
	VCD_CMD_DEVICE_TERM,
	VCD_CMD_DEVICE_RESET,
	VCD_CMD_CODEC_START,
	VCD_CMD_CODEC_STOP,
	VCD_CMD_CODE_FRAME,
	VCD_CMD_OUTPUT_FLUSH,
	VCD_CMD_CLIENT_CLOSE
};

enum vcd_core_type {
    VCD_CORE_1080P,
    VCD_CORE_720P
};

struct vcd_cmd_q_element {
	enum vcd_command pending_cmd;
};

struct vcd_buffer_entry {
	struct list_head sched_list;
	struct list_head list;
	u32 valid;
	u8 *alloc;
	u8 *virtual;
	u8 *physical;
	size_t sz;
	u32 allocated;
	u32 in_use;
	struct vcd_frame_data frame;

};

struct vcd_buffer_pool {
	struct vcd_buffer_entry *entries;
	u32 count;
	struct vcd_buffer_requirement buf_req;
	u32 validated;
	u32 allocated;
	u32 in_use;
	struct list_head queue;
	u16 q_len;
};

struct vcd_transc {
	u32 in_use;
	enum vcd_command type;
	struct vcd_clnt_ctxt *cctxt;

	struct vcd_buffer_entry *ip_buf_entry;

	s64 time_stamp;
	u32 flags;
	u32 ip_frm_tag;
	enum vcd_frame frame;

	struct vcd_buffer_entry *op_buf_entry;

	u32 input_done;
	u32 frame_done;
};

struct vcd_dev_ctxt {
	u32 ddl_cmd_concurrency;
	u32 ddl_frame_ch_depth;
	u32 ddl_cmd_ch_depth;
	u32 ddl_frame_ch_interim;
	u32 ddl_cmd_ch_interim;
	u32 ddl_frame_ch_free;
	u32 ddl_cmd_ch_free;

	struct list_head sched_clnt_list;

	struct vcd_init_config config;

	u32 driver_ids[VCD_DRIVER_CLIENTS_MAX];
	u32 refs;
	u8 *device_base_addr;
	void *hw_timer_handle;
	u32               hw_time_out;
	struct vcd_clnt_ctxt *cctxt_list_head;

	enum vcd_command pending_cmd;

	u32 command_continue;

	struct vcd_transc *trans_tbl;
	u32 trans_tbl_size;

	enum vcd_power_state pwr_state;
	enum vcd_pwr_clk_state pwr_clk_state;
	u32 active_clnts;
	u32 max_perf_lvl;
	u32 reqd_perf_lvl;
	u32 curr_perf_lvl;
	u32 set_perf_lvl_pending;
	u32 turbo_mode_set;
};

struct vcd_clnt_status {
	u32 req_perf_lvl;
	u32 frame_submitted;
	u32 frame_delayed;
	u32 cmd_submitted;
	u32 int_field_cnt;
	s64 first_ts;
	s64 prev_ts;
	u64 time_elapsed;
	struct vcd_frame_data eos_trig_ip_frm;
	struct ddl_frame_data_tag eos_prev_op_frm;
	u32 eos_prev_op_frm_status;
	u32	last_err;
	u32	last_evt;
	u32 mask;
};

struct vcd_sched_clnt_ctx {
	struct list_head list;
	u32 clnt_active;
	void *clnt_data;
	u32 tkns;
	u32 round_perfrm;
	u32 rounds;
	struct list_head ip_frm_list;
};

struct vcd_clnt_ctxt {
	u32 signature;
	struct vcd_clnt_state_ctxt clnt_state;

	s32 driver_id;

	u32 live;
	u32 decoding;
	u32 bframe;
	u32 num_slices;

	struct vcd_property_frame_rate frm_rate;
	u32 frm_p_units;
	u32 reqd_perf_lvl;
	u32 time_resoln;
	u32 time_frame_delta;

	struct vcd_buffer_pool in_buf_pool;
	struct vcd_buffer_pool out_buf_pool;

	void (*callback) (u32 event, u32 status, void *info, size_t sz,
			  void *handle, void *const client_data);
	void *client_data;
	struct vcd_sched_clnt_ctx *sched_clnt_hdl;
	u32	ddl_hdl_valid;
	u32 *ddl_handle;
	struct vcd_dev_ctxt *dev_ctxt;
	struct vcd_cmd_q_element cmd_q;
	struct vcd_sequence_hdr seq_hdr;
	u8 *seq_hdr_phy_addr;
	struct vcd_clnt_status status;
	struct ion_client *vcd_ion_client;
	u32 vcd_enable_ion;
	struct vcd_clnt_ctxt *next;
	u32 meta_mode;
	int perf_set_by_client;
	int secure;
};

#define VCD_BUFFERPOOL_INUSE_DECREMENT(val) \
do { \
	if ((val) > 0) \
		val--; \
	else { \
		VCD_MSG_ERROR("%s(): Inconsistent val given in " \
			" VCD_BUFFERPOOL_INUSE_DECREMENT\n", __func__); \
	} \
} while (0)

#endif
