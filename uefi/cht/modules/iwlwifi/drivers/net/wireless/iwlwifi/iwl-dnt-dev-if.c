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
#include <linux/export.h>
#include <linux/vmalloc.h>

#include "iwl-debug.h"
#include "iwl-io.h"
#include "iwl-trans.h"
#include "iwl-tm-gnl.h"
#include "iwl-dnt-cfg.h"
#include "iwl-dnt-dev-if.h"
#include "iwl-prph.h"
#include "iwl-csr.h"

static void iwl_dnt_dev_if_configure_mipi(struct iwl_trans *trans)
{
	if (trans->cfg->device_family == IWL_DEVICE_FAMILY_8000) {
		iwl_trans_set_bits_mask(trans,
					trans->dbg_cfg.dbg_mipi_conf_reg,
					trans->dbg_cfg.dbg_mipi_conf_mask,
					trans->dbg_cfg.dbg_mipi_conf_mask);
		return;
	}

	/* ABB_CguDTClkCtrl - set system trace and mtm clock souce as PLLA */
	iowrite32(0x30303, (void __iomem *)0xe640110c);

	/* ABB_SpcuMemPower - set the power of the trace memory */
	iowrite32(0x1, (void __iomem *)0xe640201c);

	/* set MIPI2 PCL, PCL_26 - PCL_30 */
	iowrite32(0x10, (void __iomem *)0xe6300274);
	iowrite32(0x10, (void __iomem *)0xe6300278);
	iowrite32(0x10, (void __iomem *)0xe630027c);
	iowrite32(0x10, (void __iomem *)0xe6300280);
	iowrite32(0x10, (void __iomem *)0xe6300284);

	/* ARB0_CNF - enable generic arbiter */
	iowrite32(0xc0000000, (void __iomem *)0xe6700108);

	/* enable WLAN arbiter */
	iowrite32(0x80000006, (void __iomem *)0xe6700140);

#ifdef IWL_MIPI_IDI
	/* enable IDI arbiter for all channels - this code is
	 * needed in case we'd like to look on IDI bus logs
	 * via MIPI
	 */
	iowrite32(0xB0000004, (void __iomem *)0xe6700124);
	iowrite32(0xC0000000, (void __iomem *)0xe6700128);
#endif
}

static void iwl_dnt_dev_if_configure_marbh(struct iwl_trans *trans)
{
	struct iwl_dbg_cfg *cfg = &trans->dbg_cfg;
	u32 ret, reg_val = 0;

	if (cfg->dbg_marbh_access_type == ACCESS_TYPE_DIRECT) {
		iwl_trans_set_bits_mask(trans, cfg->dbg_marbh_conf_reg,
					cfg->dbg_marbh_conf_mask,
					cfg->dbg_marbh_conf_mask);
	} else if (cfg->dbg_marbh_access_type == ACCESS_TYPE_INDIRECT) {
		ret = iwl_trans_read_mem(trans, cfg->dbg_marbh_conf_reg,
					 &reg_val, 1);
		if (ret) {
			IWL_ERR(trans, "Failed to read MARBH conf reg\n");
			return;
		}
		reg_val |= cfg->dbg_marbh_conf_mask;
		ret = iwl_trans_write_mem(trans, cfg->dbg_marbh_conf_reg,
							 &reg_val, 1);
		if (ret) {
			IWL_ERR(trans, "Failed to write MARBH conf reg\n");
			return;
		}
	} else {
		IWL_ERR(trans, "Invalid MARBH access type\n");
	}
}

static void iwl_dnt_dev_if_configure_dbgc_registers(struct iwl_trans *trans,
						    u32 base_addr,
						    u32 end_addr)
{
	struct iwl_dbg_cfg *cfg = &trans->dbg_cfg;

	switch (trans->tmdev->dnt->cur_mon_type) {
	case SMEM:
		iwl_write_prph(trans, cfg->dbgc_hb_base_addr,
			       cfg->dbgc_hb_base_val_smem);
		iwl_write_prph(trans, cfg->dbgc_hb_end_addr,
			       cfg->dbgc_hb_end_val_smem);

		/*
		 * SMEM requires the same internal configuration as MARBH,
		 * which preceeded it.
		 */
		iwl_dnt_dev_if_configure_marbh(trans);
		break;

	case DMA:
	default:
		/*
		 * The given addresses are already shifted by 4 places so we
		 * need to shift by another 4.
		 * Note that in SfP the end addr points to the last block of
		 * data that the DBGC can write to, so when setting the end
		 * register we need to set it to 1 block before.
		 */
		iwl_write_prph(trans, cfg->dbgc_hb_base_addr, base_addr >> 4);
		iwl_write_prph(trans, cfg->dbgc_hb_end_addr,
			       (end_addr >> 4) - 1);
		break;
	};
}

static void iwl_dnt_dev_if_configure_dbgm_registers(struct iwl_trans *trans,
						    u32 base_addr,
						    u32 end_addr)
{
	struct iwl_dbg_cfg *cfg = &trans->dbg_cfg;

	/* If we're running a device that supports DBGC - use it */
	if (trans->cfg->device_family == IWL_DEVICE_FAMILY_8000) {
		iwl_dnt_dev_if_configure_dbgc_registers(trans, base_addr,
							end_addr);
		return;
	}

	/* configuring monitor */
	iwl_write_prph(trans, cfg->dbg_mon_buff_base_addr_reg_addr, base_addr);
	iwl_write_prph(trans, cfg->dbg_mon_buff_end_addr_reg_addr, end_addr);
	iwl_write_prph(trans, cfg->dbg_mon_data_sel_ctl_addr,
		       cfg->dbg_mon_data_sel_ctl_val);
	iwl_write_prph(trans, cfg->dbg_mon_mc_msk_addr,
		       cfg->dbg_mon_mc_msk_val);
	iwl_write_prph(trans, cfg->dbg_mon_sample_mask_addr,
		       cfg->dbg_mon_sample_mask_val);
	iwl_write_prph(trans, cfg->dbg_mon_start_mask_addr,
		       cfg->dbg_mon_start_mask_val);
	iwl_write_prph(trans, cfg->dbg_mon_end_threshold_addr,
		       cfg->dbg_mon_end_threshold_val);
	iwl_write_prph(trans, cfg->dbg_mon_end_mask_addr,
		       cfg->dbg_mon_end_mask_val);
	iwl_write_prph(trans, cfg->dbg_mon_sample_period_addr,
		       cfg->dbg_mon_sample_period_val);
	/* starting monitor */
	iwl_write_prph(trans, cfg->dbg_mon_sample_ctl_addr,
		       cfg->dbg_mon_sample_ctl_val);
}

static int iwl_dnt_dev_if_retrieve_dma_monitor_data(struct iwl_dnt *dnt,
						    struct iwl_trans *trans,
						    void *buffer,
						    u32 buffer_size)
{
	struct iwl_dbg_cfg *cfg = &trans->dbg_cfg;
	u32 wr_ptr;
	bool dont_reorder = false;
	/* FIXME send stop command to FW */
	if (WARN_ON_ONCE(!dnt->mon_buf_cpu_addr)) {
		IWL_ERR(trans, "Can't retrieve data - DMA wasn't allocated\n");
		return -ENOMEM;
	}

	/* If we're running a device that supports DBGC - use it */
	if (trans->cfg->device_family == IWL_DEVICE_FAMILY_8000)
		wr_ptr = iwl_read_prph(trans, cfg->dbgc_dram_wrptr_addr);
	else
		wr_ptr = iwl_read_prph(trans, cfg->dbg_mon_wr_ptr_addr);
	/* iwl_read_prph returns 0x5a5a5a5a when it fails to grab nic access */
	if (wr_ptr == 0x5a5a5a5a) {
		IWL_ERR(trans,
			"Can't read write pointer - not reordering buffer\n");
		dont_reorder = true;
	}

	/* If we're running a device that supports DBGC.... */
	if (trans->cfg->device_family == IWL_DEVICE_FAMILY_8000) {
		if (CSR_HW_REV_STEP(trans->hw_rev) == 0) /* A-step */
			/*
			 * Here the write pointer points to the chunk previously
			 * written, and in this function we refer to it as
			 * pointing to the oldest data in the buffer, so we
			 * need to also increment the value we're using by a
			 * chunk (256 bytes).
			 */
			wr_ptr = ((wr_ptr - (dnt->mon_base_addr >> 6)) << 6) +
				 256;
		else
			/*
			 * In the B-step, wr_ptr is given relative to the base
			 * address, in DWORD granularity, and points to the
			 * next chunk to write to - i.e., the oldest data in
			 * the buffer.
			 */
			wr_ptr <<= 2;
	} else {
		wr_ptr = (wr_ptr << 4) - dnt->mon_base_addr;
	}

	/* Misunderstanding wr_ptr can cause a page fault, so validate it... */
	if (wr_ptr > dnt->mon_buf_size) {
		IWL_ERR(trans,
			"Write pointer DMA monitor register points to invalid data - setting to 0\n");
		dont_reorder = true;
	}

	/* We have a problem with the wr_ptr, so just return the memory as-is */
	if (dont_reorder)
		wr_ptr = 0;

	memcpy(buffer, dnt->mon_buf_cpu_addr + wr_ptr,
	       dnt->mon_buf_size - wr_ptr);
	memcpy(buffer + dnt->mon_buf_size - wr_ptr, dnt->mon_buf_cpu_addr,
	       wr_ptr);

	return dnt->mon_buf_size;
}

static int iwl_dnt_dev_if_retrieve_marbh_monitor_data(struct iwl_dnt *dnt,
						      struct iwl_trans *trans,
						      u8 *buffer,
						      u32 buffer_size)
{
	struct iwl_dbg_cfg *cfg = &trans->dbg_cfg;
	int buf_size_in_dwords, buf_index, i;
	u32 wr_ptr, read_val;

	/* FIXME send stop command to FW */

	wr_ptr = iwl_read_prph(trans, cfg->dbg_mon_wr_ptr_addr);
	/* iwl_read_prph returns 0x5a5a5a5a when it fails to grab nic access */
	if (wr_ptr == 0x5a5a5a5a) {
		IWL_ERR(trans, "Can't read write pointer\n");
		return -ENODEV;
	}

	read_val = iwl_read_prph(trans, cfg->dbg_mon_buff_base_addr_reg_addr);
	if (read_val == 0x5a5a5a5a) {
		IWL_ERR(trans, "Can't read monitor base address\n");
		return -ENODEV;
	}
	dnt->mon_base_addr = read_val;

	read_val = iwl_read_prph(trans, cfg->dbg_mon_buff_end_addr_reg_addr);
	if (read_val == 0x5a5a5a5a) {
		IWL_ERR(trans, "Can't read monitor end address\n");
		return -ENODEV;
	}
	dnt->mon_end_addr = read_val;

	wr_ptr = wr_ptr - dnt->mon_base_addr;
	iwl_write_prph(trans, cfg->dbg_mon_dmarb_rd_ctl_addr, 0x00000001);

	/* buf size includes the end_addr as well */
	buf_size_in_dwords = dnt->mon_end_addr - dnt->mon_base_addr + 1;
	for (i = 0; i < buf_size_in_dwords; i++) {
		/* reordering cyclic buffer */
		buf_index = (i + (buf_size_in_dwords - wr_ptr)) %
			    buf_size_in_dwords;
		read_val = iwl_read_prph(trans,
					 cfg->dbg_mon_dmarb_rd_data_addr);
		memcpy(&buffer[buf_index * sizeof(u32)], &read_val,
		       sizeof(u32));
	}
	iwl_write_prph(trans, cfg->dbg_mon_dmarb_rd_ctl_addr, 0x00000000);

	return buf_size_in_dwords * sizeof(u32);
}

static int iwl_dnt_dev_if_retrieve_smem_monitor_data(struct iwl_dnt *dnt,
						     struct iwl_trans *trans,
						     u8 *buffer,
						     u32 buffer_size)
{
	struct iwl_dbg_cfg *cfg = &trans->dbg_cfg;
	u32 i, bytes_to_end, calc_size;
	u32 base_addr, end_addr, wr_ptr_addr, wr_ptr_shift;
	u32 base, end, wr_ptr, pos, chunks_num, wr_ptr_offset;
	u8 *temp_buffer;

	if (CSR_HW_REV_STEP(trans->hw_rev) != SILICON_A_STEP) {
		/* assuming B-step or C-step */
		base_addr = cfg->dbg_mon_buff_base_addr_reg_addr_b_step;
		end_addr = cfg->dbg_mon_buff_end_addr_reg_addr_b_step;
		wr_ptr_addr = cfg->dbg_mon_wr_ptr_addr_b_step;
		wr_ptr_shift = 2;
	} else {
		/* assuming A-step */
		base_addr = cfg->dbg_mon_buff_base_addr_reg_addr;
		end_addr = cfg->dbg_mon_buff_end_addr_reg_addr;
		wr_ptr_addr = cfg->dbg_mon_wr_ptr_addr;
		wr_ptr_shift = 0;
	}

	base = iwl_read_prph(trans, base_addr);
	/* iwl_read_prph returns 0x5a5a5a5a when it fails to grab nic access */
	if (base == 0x5a5a5a5a) {
		IWL_ERR(trans, "Can't read base addr\n");
		return -ENODEV;
	}

	end = iwl_read_prph(trans, end_addr);
	/* iwl_read_prph returns 0x5a5a5a5a when it fails to grab nic access */
	if (end == 0x5a5a5a5a) {
		IWL_ERR(trans, "Can't read end addr\n");
		return -ENODEV;
	}

	if (base == end) {
		IWL_ERR(trans, "Invalid base and end values\n");
		return -ENODEV;
	}

	wr_ptr = iwl_read_prph(trans, wr_ptr_addr);
	/* iwl_read_prph returns 0x5a5a5a5a when it fails to grab nic access */
	if (wr_ptr == 0x5a5a5a5a) {
		IWL_ERR(trans, "Can't read write pointer, not re-aligning\n");
		wr_ptr = base << 8;
	}

	pos = base << 8;
	calc_size = (end - base + 1) << 8;
	wr_ptr <<= wr_ptr_shift;
	bytes_to_end = ((end + 1) << 8) - wr_ptr;
	chunks_num = calc_size / DNT_CHUNK_SIZE;
	wr_ptr_offset = wr_ptr - pos;

	if (wr_ptr_offset > calc_size) {
		IWL_ERR(trans, "Invalid wr_ptr value, not re-aligning\n");
		wr_ptr_offset = 0;
	}

	if (calc_size > buffer_size) {
		IWL_ERR(trans, "Invalid buffer size\n");
		return -EINVAL;
	}

	temp_buffer = kzalloc(calc_size, GFP_KERNEL);
	if (!temp_buffer)
		return -ENOMEM;

	for (i = 0; i < chunks_num; i++)
		iwl_trans_read_mem(trans, pos + (i * DNT_CHUNK_SIZE),
				   temp_buffer + (i * DNT_CHUNK_SIZE),
				   DNT_CHUNK_SIZE / sizeof(u32));

	if (calc_size % DNT_CHUNK_SIZE)
		iwl_trans_read_mem(trans, pos + (chunks_num * DNT_CHUNK_SIZE),
				   temp_buffer + (chunks_num * DNT_CHUNK_SIZE),
				   (calc_size - (chunks_num * DNT_CHUNK_SIZE)) /
				   sizeof(u32));

	memcpy(buffer, temp_buffer + wr_ptr_offset, bytes_to_end);
	memcpy(buffer + bytes_to_end, temp_buffer, wr_ptr_offset);

	kfree(temp_buffer);

	return calc_size;
}

int iwl_dnt_dev_if_configure_monitor(struct iwl_dnt *dnt,
				     struct iwl_trans *trans)
{
	u32 base_addr, end_addr;

	switch (dnt->cur_mon_type) {
	case NO_MONITOR:
		IWL_INFO(trans, "Monitor is disabled\n");
		dnt->iwl_dnt_status &= ~IWL_DNT_STATUS_MON_CONFIGURED;
		break;
	case MARBH_ADC:
	case MARBH_DBG:
		iwl_dnt_dev_if_configure_marbh(trans);
		dnt->mon_buf_size = DNT_MARBH_BUF_SIZE;
		break;
	case DMA:
		if (!dnt->mon_buf_cpu_addr) {
			IWL_ERR(trans,
				"Can't configure DMA monitor: no cpu addr\n");
			return -ENOMEM;
		}
		base_addr = dnt->mon_base_addr >> 4;
		end_addr = dnt->mon_end_addr >> 4;
		iwl_dnt_dev_if_configure_dbgm_registers(trans, base_addr,
							end_addr);
		break;
	case MIPI:
		iwl_dnt_dev_if_configure_mipi(trans);
		break;
	case SMEM:
		base_addr = 0;
		end_addr = 0;
		iwl_dnt_dev_if_configure_dbgm_registers(trans, base_addr,
							end_addr);
		dnt->mon_buf_size = DNT_SMEM_BUF_SIZE;
		break;
	case INTERFACE:
		base_addr = 0;
		end_addr = 0x400;
		iwl_dnt_dev_if_configure_dbgm_registers(trans, base_addr,
							end_addr);
		break;
	default:
		dnt->iwl_dnt_status &= ~IWL_DNT_STATUS_MON_CONFIGURED;
		IWL_INFO(trans, "Invalid monitor type\n");
		return -EINVAL;
	}


	dnt->iwl_dnt_status |= IWL_DNT_STATUS_MON_CONFIGURED;

	return 0;
}

static int iwl_dnt_dev_if_send_dbgm(struct iwl_dnt *dnt,
				    struct iwl_trans *trans)
{
	struct iwl_dbg_cfg *cfg = &trans->dbg_cfg;
	struct iwl_host_cmd host_cmd = {
		.id = cfg->dbg_conf_monitor_cmd_id,
		.data[0] = cfg->dbg_conf_monitor_host_command.data,
		.len[0] = cfg->dbg_conf_monitor_host_command.len,
		.dataflags[0] = IWL_HCMD_DFL_NOCOPY,
		.flags = CMD_WANT_SKB,
	};
	int ret;

	ret = iwl_trans_send_cmd(trans, &host_cmd);
	if (ret) {
		IWL_ERR(trans, "Failed to send monitor command\n");
		dnt->iwl_dnt_status |= IWL_DNT_STATUS_FAILED_START_MONITOR;
	}

	return ret;
}

static int iwl_dnt_dev_if_send_ldbg(struct iwl_dnt *dnt,
				    struct iwl_trans *trans,
				    int cmd_index)
{
	struct iwl_dbg_cfg *cfg = &trans->dbg_cfg;
	struct iwl_host_cmd host_cmd = {
		.id = cfg->dbg_conf_monitor_cmd_id,
		.data[0] = cfg->ldbg_cmd[cmd_index].data,
		.len[0] = DNT_LDBG_CMD_SIZE,
		.dataflags[0] = IWL_HCMD_DFL_NOCOPY,
		.flags = CMD_WANT_SKB,
	};


	return iwl_trans_send_cmd(trans, &host_cmd);
}

int iwl_dnt_dev_if_start_monitor(struct iwl_dnt *dnt,
				 struct iwl_trans *trans)
{
	struct iwl_dbg_cfg *cfg = &trans->dbg_cfg;
	int i, ret;

	switch (cfg->dbgm_enable_mode) {
	case DEBUG:
		return iwl_dnt_dev_if_send_dbgm(dnt, trans);
	case SNIFFER:
		ret = 0;
		for (i = 0; i < cfg->ldbg_cmd_nums; i++) {
			ret = iwl_dnt_dev_if_send_ldbg(dnt, trans, i);
			if (ret) {
				IWL_ERR(trans,
					"Failed to send ldbg command\n");
				break;
			}
		}
		return ret;
	default:
		WARN_ONCE(1, "invalid option: %d\n", cfg->dbgm_enable_mode);
		return -EINVAL;
	}
}

int iwl_dnt_dev_if_set_log_level(struct iwl_dnt *dnt,
				 struct iwl_trans *trans)
{
	struct iwl_dbg_cfg *cfg = &trans->dbg_cfg;
	struct iwl_host_cmd host_cmd = {
		.id = cfg->log_level_cmd_id,
		.data[0] = cfg->log_level_cmd.data,
		.len[0] = cfg->log_level_cmd.len,
		.dataflags[0] = IWL_HCMD_DFL_NOCOPY,
		.flags = CMD_WANT_SKB,
	};
	int ret;

	ret = iwl_trans_send_cmd(trans, &host_cmd);
	if (ret)
		IWL_ERR(trans, "Failed to send log level cmd\n");

	return ret;
}

int iwl_dnt_dev_if_retrieve_monitor_data(struct iwl_dnt *dnt,
					 struct iwl_trans *trans,
					 u8 *buffer, u32 buffer_size)
{
	switch (dnt->cur_mon_type) {
	case DMA:
		return iwl_dnt_dev_if_retrieve_dma_monitor_data(dnt, trans,
								buffer,
								buffer_size);
	case MARBH_ADC:
	case MARBH_DBG:
		return iwl_dnt_dev_if_retrieve_marbh_monitor_data(dnt, trans,
								  buffer,
								  buffer_size);
	case SMEM:
		return iwl_dnt_dev_if_retrieve_smem_monitor_data(dnt, trans,
								 buffer,
								 buffer_size);
	case INTERFACE:
	default:
		WARN_ONCE(1, "invalid option: %d\n", dnt->cur_mon_type);
		return -EINVAL;
	}
}

int iwl_dnt_dev_if_read_sram(struct iwl_dnt *dnt, struct iwl_trans *trans)
{
	struct dnt_crash_data *crash = &dnt->dispatch.crash;
	int ofs, len = 0;

	ofs = dnt->image->sec[IWL_UCODE_SECTION_DATA].offset;
	len = dnt->image->sec[IWL_UCODE_SECTION_DATA].len;

	crash->sram =  vmalloc(len);
	if (!crash->sram)
		return -ENOMEM;

	crash->sram_buf_size = len;
	return iwl_trans_read_mem(trans, ofs, crash->sram, len / sizeof(u32));
}
IWL_EXPORT_SYMBOL(iwl_dnt_dev_if_read_sram);

int iwl_dnt_dev_if_read_rx(struct iwl_dnt *dnt, struct iwl_trans *trans)
{
	struct dnt_crash_data *crash = &dnt->dispatch.crash;
	int i, reg_val;
	u32 buf32_size, offset = 0;
	u32 *buf32;
	unsigned long flags;

	/* reading buffer size */
	reg_val = iwl_trans_read_prph(trans, RXF_SIZE_ADDR);
	crash->rx_buf_size =
		(reg_val & RXF_SIZE_BYTE_CNT_MSK) >> RXF_SIZE_BYTE_CND_POS;

	/* the register holds the value divided by 128 */
	crash->rx_buf_size = crash->rx_buf_size << 7;

	if (!crash->rx_buf_size)
		return -ENOMEM;

	buf32_size = crash->rx_buf_size / sizeof(u32);

	crash->rx =  vmalloc(crash->rx_buf_size);
	if (!crash->rx)
		return -ENOMEM;

	buf32 = (u32 *)crash->rx;

	if (!iwl_trans_grab_nic_access(trans, false, &flags)) {
		vfree(crash->rx);
		return -EBUSY;
	}
	for (i = 0; i < buf32_size; i++) {
		iwl_trans_write_prph(trans, RXF_LD_FENCE_OFFSET_ADDR, offset);
		offset += sizeof(u32);
		buf32[i] = iwl_trans_read_prph(trans, RXF_FIFO_RD_FENCE_ADDR);
	}
	iwl_trans_release_nic_access(trans, &flags);

	return 0;
}
IWL_EXPORT_SYMBOL(iwl_dnt_dev_if_read_rx);
