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
#include <linux/types.h>
#include <linux/kthread.h>
#include <linux/export.h>

#include "iwl-drv.h"
#include "iwl-config.h"
#include "iwl-debug.h"
#include "iwl-tm-gnl.h"
#include "iwl-dnt-cfg.h"
#include "iwl-dnt-dispatch.h"
#include "iwl-dnt-dev-if.h"

#ifdef CPTCFG_IWLWIFI_DEBUGFS

/*
 * iwl_dnt_debugfs_log_read - returns ucodeMessages to the user.
 * The logs are returned in binary format until the buffer of debugfs is
 * exhausted. If a log can't be copied to user (due to general error, not
 * a size problem) it is totally discarded and lost.
 */
static ssize_t iwl_dnt_debugfs_log_read(struct file *file,
					char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct iwl_trans *trans = file->private_data;
	unsigned char *temp_buf;
	int ret = 0;

	temp_buf = kzalloc(count, GFP_KERNEL);
	if (!temp_buf)
		return -ENOMEM;

	ret = iwl_dnt_dispatch_pull(trans, temp_buf, count, UCODE_MESSAGES);
	if (ret < 0) {
		IWL_DEBUG_INFO(trans, "Failed to retrieve debug data\n");
		goto free_buf;
	}

	ret = simple_read_from_buffer(user_buf, ret, ppos, temp_buf, count);
free_buf:
	kfree(temp_buf);
	return ret;
}

static const struct file_operations iwl_dnt_debugfs_log_ops = {
	.read = iwl_dnt_debugfs_log_read,
	.open = simple_open,
	.llseek = generic_file_llseek,
};

static bool iwl_dnt_register_debugfs_entries(struct iwl_trans *trans,
					    struct dentry *dbgfs_dir)
{
	struct iwl_dnt *dnt = trans->tmdev->dnt;

	dnt->debugfs_entry = debugfs_create_dir("dbgm", dbgfs_dir);
	if (!dnt->debugfs_entry)
		return false;

	if (!debugfs_create_file("log", S_IRUSR, dnt->debugfs_entry,
				 trans, &iwl_dnt_debugfs_log_ops))
		return false;
	return true;
}
#endif

static bool iwl_dnt_configure_prepare_dma(struct iwl_dnt *dnt,
					  struct iwl_trans *trans)
{
	struct iwl_dbg_cfg *dbg_cfg = &trans->dbg_cfg;

	if (dbg_cfg->dbm_destination_path != DMA || !dbg_cfg->dbgm_mem_power)
		return true;

	dnt->mon_buf_size = 0x800 << dbg_cfg->dbgm_mem_power;
	dnt->mon_buf_cpu_addr =
		dma_alloc_coherent(trans->dev, dnt->mon_buf_size,
				   &dnt->mon_dma_addr, GFP_KERNEL);
	if (!dnt->mon_buf_cpu_addr)
		return false;

	dnt->mon_base_addr = (u64) dnt->mon_dma_addr;
	dnt->mon_end_addr = dnt->mon_base_addr + dnt->mon_buf_size;
	dnt->iwl_dnt_status |= IWL_DNT_STATUS_DMA_BUFFER_ALLOCATED;

	return true;
}

static bool iwl_dnt_validate_configuration(struct iwl_trans *trans)
{
	struct iwl_dbg_cfg *dbg_cfg = &trans->dbg_cfg;

	if (!strcmp(trans->dev->bus->name, BUS_TYPE_PCI))
		return dbg_cfg->dbm_destination_path == DMA ||
		       dbg_cfg->dbm_destination_path == MARBH_ADC ||
		       dbg_cfg->dbm_destination_path == MARBH_DBG ||
		       dbg_cfg->dbm_destination_path == MIPI;
	else if (!strcmp(trans->dev->bus->name, BUS_TYPE_IDI))
		return dbg_cfg->dbm_destination_path == INTERFACE ||
		       dbg_cfg->dbm_destination_path == MARBH_ADC ||
		       dbg_cfg->dbm_destination_path == MARBH_DBG ||
		       dbg_cfg->dbm_destination_path == MIPI;
	else if (!strcmp(trans->dev->bus->name, BUS_TYPE_SDIO))
		return dbg_cfg->dbm_destination_path == MARBH_ADC ||
		       dbg_cfg->dbm_destination_path == MARBH_DBG ||
		       dbg_cfg->dbm_destination_path == MIPI;

	return false;
}

static int iwl_dnt_conf_monitor(struct iwl_trans *trans, u32 output,
				u32 monitor_type, u32 target_mon_mode)
{
	struct iwl_dnt *dnt = trans->tmdev->dnt;

	if (dnt->cur_input_mask & MONITOR_INPUT_MODE_MASK) {
		IWL_INFO(trans, "DNT: Resetting deivce configuration\n");
		return iwl_dnt_dev_if_configure_monitor(dnt, trans);
	}

	dnt->cur_input_mask |= MONITOR;
	dnt->dispatch.mon_output = output;
	dnt->cur_mon_type = monitor_type;
	dnt->cur_mon_mode = target_mon_mode;
	if (monitor_type == INTERFACE) {
		if (output == NETLINK || output == FTRACE) {
			/* setting PUSH out mode */
			dnt->dispatch.mon_out_mode = PUSH;
			dnt->dispatch.mon_in_mode = COLLECT;
		} else {
			dnt->dispatch.dbgm_db =
				iwl_dnt_dispatch_allocate_collect_db(dnt);
			if (!dnt->dispatch.dbgm_db)
				return -ENOMEM;
			dnt->dispatch.mon_in_mode = RETRIEVE;
		}
	} else {
		dnt->dispatch.mon_out_mode = PULL;
		dnt->dispatch.mon_in_mode = RETRIEVE;

		/*
		 * If we're running a device that supports DBGC and monitor
		 * was given value as MARBH, it should be interpreted as SMEM
		 */
		if ((trans->cfg->device_family == IWL_DEVICE_FAMILY_8000) &&
		    (monitor_type == MARBH_ADC || monitor_type == MARBH_DBG))
			dnt->cur_mon_type = SMEM;
	}
	return iwl_dnt_dev_if_configure_monitor(dnt, trans);
}

void iwl_dnt_start(struct iwl_trans *trans)
{
	struct iwl_dnt *dnt = trans->tmdev->dnt;
	struct iwl_dbg_cfg *dbg_cfg = &trans->dbg_cfg;

	if (!dnt)
		return;

	if ((dnt->iwl_dnt_status & IWL_DNT_STATUS_MON_CONFIGURED) &&
	    dbg_cfg->dbg_conf_monitor_cmd_id)
		iwl_dnt_dev_if_start_monitor(dnt, trans);

	if ((dnt->iwl_dnt_status & IWL_DNT_STATUS_UCODE_MSGS_CONFIGURED) &&
	    dbg_cfg->log_level_cmd_id)
		iwl_dnt_dev_if_set_log_level(dnt, trans);
}
IWL_EXPORT_SYMBOL(iwl_dnt_start);

#ifdef CPTCFG_IWLWIFI_DEBUGFS
static int iwl_dnt_conf_ucode_msgs_via_rx(struct iwl_trans *trans, u32 output)
{
	struct iwl_dnt *dnt = trans->tmdev->dnt;

	dnt->cur_input_mask |= UCODE_MESSAGES;
	dnt->dispatch.ucode_msgs_output = output;

	if (output == NETLINK || output == FTRACE) {
		/* setting PUSH out mode */
		dnt->dispatch.ucode_msgs_out_mode = PUSH;
	} else {
		dnt->dispatch.um_db =
				iwl_dnt_dispatch_allocate_collect_db(dnt);
		if (!dnt->dispatch.um_db)
			return -ENOMEM;
		dnt->dispatch.ucode_msgs_out_mode = RETRIEVE;
	}
	/* setting COLLECT in mode */
	dnt->dispatch.ucode_msgs_in_mode = COLLECT;
	dnt->iwl_dnt_status |= IWL_DNT_STATUS_UCODE_MSGS_CONFIGURED;

	return 0;
}
#endif

void iwl_dnt_init(struct iwl_trans *trans, struct dentry *dbgfs_dir)
{
	struct iwl_dnt *dnt;
	bool __maybe_unused ret;
	int __maybe_unused err;

	dnt = kzalloc(sizeof(struct iwl_dnt), GFP_KERNEL);
	if (!dnt)
		return;

	trans->tmdev->dnt = dnt;

	dnt->dev = trans->dev;

#ifdef CPTCFG_IWLWIFI_DEBUGFS
	ret = iwl_dnt_register_debugfs_entries(trans, dbgfs_dir);
	if (!ret) {
		IWL_ERR(trans, "Failed to create dnt debugfs entries\n");
		return;
	}
	err = iwl_dnt_conf_ucode_msgs_via_rx(trans, DEBUGFS);
	if (err)
		IWL_DEBUG_INFO(trans, "Failed to configure uCodeMessages\n");
#endif

	if (!iwl_dnt_validate_configuration(trans)) {
		dnt->iwl_dnt_status |= IWL_DNT_STATUS_INVALID_MONITOR_CONF;
		return;
	}
	/* allocate DMA if needed */
	if (!iwl_dnt_configure_prepare_dma(dnt, trans)) {
		IWL_ERR(trans, "Failed to prepare DMA\n");
		dnt->iwl_dnt_status |= IWL_DNT_STATUS_FAILED_TO_ALLOCATE_DMA;
	}
}
IWL_EXPORT_SYMBOL(iwl_dnt_init);

void iwl_dnt_free(struct iwl_trans *trans)
{
	struct iwl_dnt *dnt = trans->tmdev->dnt;

	if (!dnt)
		return;

	iwl_dnt_dispatch_free(dnt, trans);
#ifdef CPTCFG_IWLWIFI_DEBUGFS
	debugfs_remove_recursive(dnt->debugfs_entry);
#endif
	kfree(dnt);
}
IWL_EXPORT_SYMBOL(iwl_dnt_free);

void iwl_dnt_configure(struct iwl_trans *trans, const struct fw_img *image)
{
	struct iwl_dnt *dnt = trans->tmdev->dnt;
	struct iwl_dbg_cfg *dbg_cfg = &trans->dbg_cfg;
	bool is_conf_invalid;

	if (!dnt)
		return;

	dnt->image = image;

	is_conf_invalid = (dnt->iwl_dnt_status &
			   IWL_DNT_STATUS_INVALID_MONITOR_CONF);

	if (is_conf_invalid)
		return;

	switch (dbg_cfg->dbm_destination_path) {
	case DMA:
		if (!dnt->mon_buf_cpu_addr) {
			IWL_ERR(trans, "DMA buffer wasn't allocated\n");
			return;
		}
	case NO_MONITOR:
	case MIPI:
	case INTERFACE:
	case MARBH_ADC:
	case MARBH_DBG:
		iwl_dnt_conf_monitor(trans, dbg_cfg->dnt_out_mode,
				     dbg_cfg->dbm_destination_path,
				     dbg_cfg->dbgm_enable_mode);
		break;
	default:
		IWL_INFO(trans, "Invalid monitor type\n");
		return;
	}

	dnt->dispatch.crash_out_mode |= dbg_cfg->dnt_out_mode;
}
