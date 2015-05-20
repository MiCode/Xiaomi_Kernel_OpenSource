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
#include "iwl-debug.h"
#include "iwl-io.h"

#include "fw-api.h"
#include "xvt.h"

int iwl_xvt_send_cmd(struct iwl_xvt *xvt, struct iwl_host_cmd *cmd)
{
	/*
	 * Synchronous commands from this op-mode must hold
	 * the mutex, this ensures we don't try to send two
	 * (or more) synchronous commands at a time.
	 */
	if (!(cmd->flags & CMD_ASYNC))
		lockdep_assert_held(&xvt->mutex);

	return iwl_trans_send_cmd(xvt->trans, cmd);
}

int iwl_xvt_send_cmd_pdu(struct iwl_xvt *xvt, u8 id,
			 u32 flags, u16 len, const void *data)
{
	struct iwl_host_cmd cmd = {
		.id = id,
		.len = { len, },
		.data = { data, },
		.flags = flags,
	};

	return iwl_xvt_send_cmd(xvt, &cmd);
}

static struct {
	char *name;
	u8 num;
} advanced_lookup[] = {
	{ "NMI_INTERRUPT_WDG", 0x34 },
	{ "SYSASSERT", 0x35 },
	{ "UCODE_VERSION_MISMATCH", 0x37 },
	{ "BAD_COMMAND", 0x38 },
	{ "NMI_INTERRUPT_DATA_ACTION_PT", 0x3C },
	{ "FATAL_ERROR", 0x3D },
	{ "NMI_TRM_HW_ERR", 0x46 },
	{ "NMI_INTERRUPT_TRM", 0x4C },
	{ "NMI_INTERRUPT_BREAK_POINT", 0x54 },
	{ "NMI_INTERRUPT_WDG_RXF_FULL", 0x5C },
	{ "NMI_INTERRUPT_WDG_NO_RBD_RXF_FULL", 0x64 },
	{ "NMI_INTERRUPT_HOST", 0x66 },
	{ "NMI_INTERRUPT_ACTION_PT", 0x7C },
	{ "NMI_INTERRUPT_UNKNOWN", 0x84 },
	{ "NMI_INTERRUPT_INST_ACTION_PT", 0x86 },
	{ "ADVANCED_SYSASSERT", 0 },
};

static const char *desc_lookup(u32 num)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(advanced_lookup) - 1; i++)
		if (advanced_lookup[i].num == num)
			return advanced_lookup[i].name;

	/* No entry matches 'num', so it is the last: ADVANCED_SYSASSERT */
	return advanced_lookup[i].name;
}

#define ERROR_START_OFFSET  (1 * sizeof(u32))
#define ERROR_ELEM_SIZE     (7 * sizeof(u32))

void iwl_xvt_get_nic_error_log_v1(struct iwl_xvt *xvt,
				  struct iwl_error_event_table_v1 *table)
{
	struct iwl_trans *trans = xvt->trans;
	u32 base;

	base = xvt->error_event_table;
	if (xvt->cur_ucode == IWL_UCODE_INIT) {
		if (!base)
			base = xvt->fw->init_errlog_ptr;
	} else {
		if (!base)
			base = xvt->fw->inst_errlog_ptr;
	}

	iwl_trans_read_mem_bytes(trans, base, table, sizeof(*table));
}

void iwl_xvt_dump_nic_error_log_v1(struct iwl_xvt *xvt,
				   struct iwl_error_event_table_v1 *table)
{
	IWL_ERR(xvt, "0x%08X | %-28s\n", table->error_id,
		desc_lookup(table->error_id));
	IWL_ERR(xvt, "0x%08X | uPc\n", table->pc);
	IWL_ERR(xvt, "0x%08X | branchlink1\n", table->blink1);
	IWL_ERR(xvt, "0x%08X | branchlink2\n", table->blink2);
	IWL_ERR(xvt, "0x%08X | interruptlink1\n", table->ilink1);
	IWL_ERR(xvt, "0x%08X | interruptlink2\n", table->ilink2);
	IWL_ERR(xvt, "0x%08X | data1\n", table->data1);
	IWL_ERR(xvt, "0x%08X | data2\n", table->data2);
	IWL_ERR(xvt, "0x%08X | data3\n", table->data3);
	IWL_ERR(xvt, "0x%08X | beacon time\n", table->bcon_time);
	IWL_ERR(xvt, "0x%08X | tsf low\n", table->tsf_low);
	IWL_ERR(xvt, "0x%08X | tsf hi\n", table->tsf_hi);
	IWL_ERR(xvt, "0x%08X | time gp1\n", table->gp1);
	IWL_ERR(xvt, "0x%08X | time gp2\n", table->gp2);
	IWL_ERR(xvt, "0x%08X | time gp3\n", table->gp3);
	IWL_ERR(xvt, "0x%08X | uCode version\n", table->ucode_ver);
	IWL_ERR(xvt, "0x%08X | hw version\n", table->hw_ver);
	IWL_ERR(xvt, "0x%08X | board version\n", table->brd_ver);
	IWL_ERR(xvt, "0x%08X | hcmd\n", table->hcmd);
	IWL_ERR(xvt, "0x%08X | isr0\n", table->isr0);
	IWL_ERR(xvt, "0x%08X | isr1\n", table->isr1);
	IWL_ERR(xvt, "0x%08X | isr2\n", table->isr2);
	IWL_ERR(xvt, "0x%08X | isr3\n", table->isr3);
	IWL_ERR(xvt, "0x%08X | isr4\n", table->isr4);
	IWL_ERR(xvt, "0x%08X | isr_pref\n", table->isr_pref);
	IWL_ERR(xvt, "0x%08X | wait_event\n", table->wait_event);
	IWL_ERR(xvt, "0x%08X | l2p_control\n", table->l2p_control);
	IWL_ERR(xvt, "0x%08X | l2p_duration\n", table->l2p_duration);
	IWL_ERR(xvt, "0x%08X | l2p_mhvalid\n", table->l2p_mhvalid);
	IWL_ERR(xvt, "0x%08X | l2p_addr_match\n", table->l2p_addr_match);
	IWL_ERR(xvt, "0x%08X | lmpm_pmg_sel\n", table->lmpm_pmg_sel);
	IWL_ERR(xvt, "0x%08X | timestamp\n", table->u_timestamp);
	IWL_ERR(xvt, "0x%08X | flow_handler\n", table->flow_handler);
}

void iwl_xvt_get_nic_error_log_v2(struct iwl_xvt *xvt,
				  struct iwl_error_event_table_v2 *table)
{
	struct iwl_trans *trans = xvt->trans;
	u32 base;

	base = xvt->error_event_table;
	if (xvt->cur_ucode == IWL_UCODE_INIT) {
		if (!base)
			base = xvt->fw->init_errlog_ptr;
	} else {
		if (!base)
			base = xvt->fw->inst_errlog_ptr;
	}

	iwl_trans_read_mem_bytes(trans, base, table, sizeof(*table));
}

void iwl_xvt_dump_nic_error_log_v2(struct iwl_xvt *xvt,
				   struct iwl_error_event_table_v2 *table)
{
	IWL_ERR(xvt, "0x%08X | %-28s\n", table->error_id,
		desc_lookup(table->error_id));
	IWL_ERR(xvt, "0x%08X | uPc\n", table->pc);
	IWL_ERR(xvt, "0x%08X | branchlink1\n", table->blink1);
	IWL_ERR(xvt, "0x%08X | branchlink2\n", table->blink2);
	IWL_ERR(xvt, "0x%08X | interruptlink1\n", table->ilink1);
	IWL_ERR(xvt, "0x%08X | interruptlink2\n", table->ilink2);
	IWL_ERR(xvt, "0x%08X | data1\n", table->data1);
	IWL_ERR(xvt, "0x%08X | data2\n", table->data2);
	IWL_ERR(xvt, "0x%08X | data3\n", table->data3);
	IWL_ERR(xvt, "0x%08X | beacon time\n", table->bcon_time);
	IWL_ERR(xvt, "0x%08X | tsf low\n", table->tsf_low);
	IWL_ERR(xvt, "0x%08X | tsf hi\n", table->tsf_hi);
	IWL_ERR(xvt, "0x%08X | time gp1\n", table->gp1);
	IWL_ERR(xvt, "0x%08X | time gp2\n", table->gp2);
	IWL_ERR(xvt, "0x%08X | time gp3\n", table->gp3);
	IWL_ERR(xvt, "0x%08X | uCode version major\n", table->major);
	IWL_ERR(xvt, "0x%08X | uCode version minor\n", table->minor);
	IWL_ERR(xvt, "0x%08X | hw version\n", table->hw_ver);
	IWL_ERR(xvt, "0x%08X | board version\n", table->brd_ver);
	IWL_ERR(xvt, "0x%08X | hcmd\n", table->hcmd);
	IWL_ERR(xvt, "0x%08X | isr0\n", table->isr0);
	IWL_ERR(xvt, "0x%08X | isr1\n", table->isr1);
	IWL_ERR(xvt, "0x%08X | isr2\n", table->isr2);
	IWL_ERR(xvt, "0x%08X | isr3\n", table->isr3);
	IWL_ERR(xvt, "0x%08X | isr4\n", table->isr4);
	IWL_ERR(xvt, "0x%08X | isr_pref\n", table->isr_pref);
	IWL_ERR(xvt, "0x%08X | wait_event\n", table->wait_event);
	IWL_ERR(xvt, "0x%08X | l2p_control\n", table->l2p_control);
	IWL_ERR(xvt, "0x%08X | l2p_duration\n", table->l2p_duration);
	IWL_ERR(xvt, "0x%08X | l2p_mhvalid\n", table->l2p_mhvalid);
	IWL_ERR(xvt, "0x%08X | l2p_addr_match\n", table->l2p_addr_match);
	IWL_ERR(xvt, "0x%08X | lmpm_pmg_sel\n", table->lmpm_pmg_sel);
	IWL_ERR(xvt, "0x%08X | timestamp\n", table->u_timestamp);
	IWL_ERR(xvt, "0x%08X | flow_handler\n", table->flow_handler);
}
