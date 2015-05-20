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
#ifndef __iwl_xvt_h__
#define __iwl_xvt_h__

#include "iwl-drv.h"
#include "iwl-trans.h"
#include "iwl-op-mode.h"
#include "iwl-fw.h"
#include "iwl-config.h"
#include "fw-api.h"
#include "iwl-notif-wait.h"

enum iwl_xvt_state {
	IWL_XVT_STATE_UNINITIALIZED = 0,
	IWL_XVT_STATE_NO_FW,
	IWL_XVT_STATE_INIT_STARTED,
	IWL_XVT_STATE_OPERATIONAL,
};

#define IWL_XVT_LOAD_MASK_INIT BIT(0)
#define IWL_XVT_LOAD_MASK_RUNTIME BIT(1)

/**
 * iwl_sw_stack_config - Holds active SW stack config as set from user space
 * @load_mask: Which FW are to be loaded during SW stack up
 * @iwl_phy_cfg_cmd: Which calibrations should be done
 */
struct iwl_sw_stack_config {
	u32 load_mask;
	u32 calib_override_mask;
	u32 fw_dbg_flags;
	struct iwl_phy_cfg_cmd fw_calib_cmd_cfg[IWL_UCODE_TYPE_MAX];
};

/* Note: This structure is read from the device with IO accesses,
 * and the reading already does the endian conversion. As it is
 * read with u32-sized accesses, any members with a different size
 * need to be ordered correctly though!
 */
struct iwl_error_event_table_v1 {
	u32 valid;		/* (nonzero) valid, (0) log is empty */
	u32 error_id;		/* type of error */
	u32 pc;			/* program counter */
	u32 blink1;		/* branch link */
	u32 blink2;		/* branch link */
	u32 ilink1;		/* interrupt link */
	u32 ilink2;		/* interrupt link */
	u32 data1;		/* error-specific data */
	u32 data2;		/* error-specific data */
	u32 data3;		/* error-specific data */
	u32 bcon_time;		/* beacon timer */
	u32 tsf_low;		/* network timestamp function timer */
	u32 tsf_hi;		/* network timestamp function timer */
	u32 gp1;		/* GP1 timer register */
	u32 gp2;		/* GP2 timer register */
	u32 gp3;		/* GP3 timer register */
	u32 ucode_ver;		/* uCode version */
	u32 hw_ver;		/* HW Silicon version */
	u32 brd_ver;		/* HW board version */
	u32 log_pc;		/* log program counter */
	u32 frame_ptr;		/* frame pointer */
	u32 stack_ptr;		/* stack pointer */
	u32 hcmd;		/* last host command header */
	u32 isr0;		/* isr status register LMPM_NIC_ISR0:
				 * rxtx_flag
				 */
	u32 isr1;		/* isr status register LMPM_NIC_ISR1:
				 * host_flag
				 */
	u32 isr2;		/* isr status register LMPM_NIC_ISR2:
				 * enc_flag
				 */
	u32 isr3;		/* isr status register LMPM_NIC_ISR3:
				 * time_flag
				 */
	u32 isr4;		/* isr status register LMPM_NIC_ISR4:
				 * wico interrupt
				 */
	u32 isr_pref;		/* isr status register LMPM_NIC_PREF_STAT */
	u32 wait_event;		/* wait event() caller address */
	u32 l2p_control;	/* L2pControlField */
	u32 l2p_duration;	/* L2pDurationField */
	u32 l2p_mhvalid;	/* L2pMhValidBits */
	u32 l2p_addr_match;	/* L2pAddrMatchStat */
	u32 lmpm_pmg_sel;	/* indicate which clocks are turned on
				 * (LMPM_PMG_SEL)
				 */
	u32 u_timestamp;	/* indicate when the date and time of the
				 * compilation
				 */
	u32 flow_handler;	/* FH read/write pointers, RX credit */
} __packed;

/* Note: This structure is read from the device with IO accesses,
 * and the reading already does the endian conversion. As it is
 * read with u32-sized accesses, any members with a different size
 * need to be ordered correctly though!
 */
struct iwl_error_event_table_v2 {
	u32 valid;		/* (nonzero) valid, (0) log is empty */
	u32 error_id;		/* type of error */
	u32 pc;			/* program counter */
	u32 blink1;		/* branch link */
	u32 blink2;		/* branch link */
	u32 ilink1;		/* interrupt link */
	u32 ilink2;		/* interrupt link */
	u32 data1;		/* error-specific data */
	u32 data2;		/* error-specific data */
	u32 data3;		/* error-specific data */
	u32 bcon_time;		/* beacon timer */
	u32 tsf_low;		/* network timestamp function timer */
	u32 tsf_hi;		/* network timestamp function timer */
	u32 gp1;		/* GP1 timer register */
	u32 gp2;		/* GP2 timer register */
	u32 gp3;		/* GP3 timer register */
	u32 major;		/* uCode version major */
	u32 minor;		/* uCode version minor */
	u32 hw_ver;		/* HW Silicon version */
	u32 brd_ver;		/* HW board version */
	u32 log_pc;		/* log program counter */
	u32 frame_ptr;		/* frame pointer */
	u32 stack_ptr;		/* stack pointer */
	u32 hcmd;		/* last host command header */
	u32 isr0;		/* isr status register LMPM_NIC_ISR0:
				 * rxtx_flag
				 */
	u32 isr1;		/* isr status register LMPM_NIC_ISR1:
				 * host_flag
				 */
	u32 isr2;		/* isr status register LMPM_NIC_ISR2:
				 * enc_flag
				 */
	u32 isr3;		/* isr status register LMPM_NIC_ISR3:
				 * time_flag
				 */
	u32 isr4;		/* isr status register LMPM_NIC_ISR4:
				 * wico interrupt
				 */
	u32 isr_pref;		/* isr status register LMPM_NIC_PREF_STAT */
	u32 wait_event;		/* wait event() caller address */
	u32 l2p_control;	/* L2pControlField */
	u32 l2p_duration;	/* L2pDurationField */
	u32 l2p_mhvalid;	/* L2pMhValidBits */
	u32 l2p_addr_match;	/* L2pAddrMatchStat */
	u32 lmpm_pmg_sel;	/* indicate which clocks are turned on
				 * (LMPM_PMG_SEL)
				 */
	u32 u_timestamp;	/* indicate when the date and time of the
				 * compilation
				 */
	u32 flow_handler;	/* FH read/write pointers, RX credit */
} __packed;

/**
 * struct iwl_xvt - the xvt op_mode
 *
 * @trans: pointer to the transport layer
 * @cfg: pointer to the driver's configuration
 * @fw: a pointer to the fw object
 * @dev: pointer to struct device for printing purposes
 */
struct iwl_xvt {
	struct iwl_trans *trans;
	const struct iwl_cfg *cfg;
	struct iwl_phy_db *phy_db;
	const struct iwl_fw *fw;
	struct device *dev;

	struct mutex mutex;	/* Protects access to xVT struct */
	enum iwl_xvt_state state;
	bool txq_full;
	bool fw_error;

	struct iwl_notif_wait_data notif_wait;

	enum iwl_ucode_type cur_ucode;
	u32 error_event_table;
	bool fw_running;
	struct iwl_sf_region sf_space;

	struct iwl_sw_stack_config sw_stack_cfg;
	bool rx_hdr_enabled;

	wait_queue_head_t mod_tx_wq;
	bool apmg_pd_en;
	/* DMA buffer information */
	u32 dma_buffer_size;
	u8 *dma_cpu_addr;
	dma_addr_t dma_addr;
	/* TX done conter */
	u32 tx_counter;
	u32 tot_tx;
	wait_queue_head_t mod_tx_done_wq;
};

#define IWL_OP_MODE_GET_XVT(_op_mode) \
	((struct iwl_xvt *)((_op_mode)->op_mode_specific))

/******************
 * XVT Methods
 ******************/

/* Host Commands */
int __must_check iwl_xvt_send_cmd(struct iwl_xvt *xvt,
				  struct iwl_host_cmd *cmd);
int __must_check iwl_xvt_send_cmd_pdu(struct iwl_xvt *xvt, u8 id,
				      u32 flags, u16 len, const void *data);

/* Utils */
void iwl_xvt_get_nic_error_log_v1(struct iwl_xvt *xvt,
				  struct iwl_error_event_table_v1 *table);
void iwl_xvt_dump_nic_error_log_v1(struct iwl_xvt *xvt,
				   struct iwl_error_event_table_v1 *table);
void iwl_xvt_get_nic_error_log_v2(struct iwl_xvt *xvt,
				  struct iwl_error_event_table_v2 *table);
void iwl_xvt_dump_nic_error_log_v2(struct iwl_xvt *xvt,
				   struct iwl_error_event_table_v2 *table);

/* User interface */
int iwl_xvt_user_cmd_execute(struct iwl_op_mode *op_mode, u32 cmd,
			     struct iwl_tm_data *data_in,
			     struct iwl_tm_data *data_out);

/* FW */
int iwl_xvt_run_fw(struct iwl_xvt *xvt, u32 ucode_type);

/* NVM */
int iwl_xvt_nvm_init(struct iwl_xvt *xvt);

#endif
