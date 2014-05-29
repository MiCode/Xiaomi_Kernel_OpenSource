/*
 *  sst_debug.c - Intel SST Driver debugfs support
 *
 *  Copyright (C) 2012	Intel Corp
 *  Authors:	Vinod Koul <vinod.koul@intel.com>
 *		Omair Mohammed Abdullah <omair.m.abdullah@linux.intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This file contains all debugfs functions
 *  Support includes:
 *   - Disabling/Enabling runtime PM for SST
 *   - Reading/Writing SST SHIM registers
 *   - Reading/Enabling Input OSC Clock
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": debugfs: " fmt

#include <linux/fs.h>
#include <linux/pci.h>
#include <linux/ctype.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/pm_runtime.h>
#include <linux/uaccess.h>
#include "../sst_platform.h"
#include "../platform_ipc_v2.h"
#include "sst.h"

#define DMA_NUM_CH	8
#define DEBUGFS_SSP_BUF_SIZE	300  /* 22 chars * 12 reg*/
#define DEBUGFS_DMA_BUF_SIZE	2500 /* 32 chars * 78 regs*/

/* Register Offsets of SSP3 and LPE DMA */
u32 ssp_reg_off[] = {0x0, 0x4, 0x8, 0xC, 0x10, 0x28, 0x2C, 0x30, 0x34, 0x38,
			0x3C, 0x40};
/* Excludes the channel registers */
u32 dma_reg_off[] = {0x2C0, 0x2C8, 0x2D0, 0x2D8, 0x2E0, 0x2E8,
		0x2F0, 0x2F8, 0x300, 0x308, 0x310, 0x318, 0x320, 0x328, 0x330,
		0x338, 0x340, 0x348, 0x350, 0x358, 0x360, 0x368, 0x370, 0x378,
		0x380, 0x388, 0x390, 0x398, 0x3A0, 0x3A8, 0x3B0, 0x3C8, 0x3D0,
		0x3D8, 0x3E0, 0x3E8, 0x3F0, 0x3F8};

static inline int is_fw_running(struct intel_sst_drv *drv);

static ssize_t sst_debug_shim_read(struct file *file, char __user *user_buf,
				   size_t count, loff_t *ppos)
{
	struct intel_sst_drv *drv = file->private_data;
	unsigned long long val = 0;
	unsigned int addr;
	char buf[512];
	char name[8];
	int pos = 0, ret = 0;

	buf[0] = 0;

	ret = is_fw_running(drv);
	if (ret) {
		pr_err("FW not running, cannot read SHIM registers\n");
		return ret;
	}

	for (addr = SST_SHIM_BEGIN; addr <= SST_SHIM_END; addr += 8) {
		switch (drv->pci_id) {
		case SST_CLV_PCI_ID:
			val = sst_shim_read(drv->shim, addr);
			break;
		case SST_MRFLD_PCI_ID:
		case SST_BYT_PCI_ID:
		case SST_CHT_PCI_ID:
			val = sst_shim_read64(drv->shim, addr);
			break;
		}

		name[0] = 0;
		switch (addr) {
		case SST_ISRX:
			strcpy(name, "ISRX"); break;
		case SST_ISRD:
			strcpy(name, "ISRD"); break;
		case SST_IPCX:
			strcpy(name, "IPCX"); break;
		case SST_IPCD:
			strcpy(name, "IPCD"); break;
		case SST_IMRX:
			strcpy(name, "IMRX"); break;
		case SST_IMRD:
			strcpy(name, "IMRD"); break;
		}
		pos += sprintf(buf + pos, "0x%.2x: %.8llx  %s\n", addr, val, name);
	}

	sst_pm_runtime_put(drv);
	return simple_read_from_buffer(user_buf, count, ppos,
			buf, strlen(buf));
}



static ssize_t sst_debug_shim_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct intel_sst_drv *drv = file->private_data;
	char buf[32];
	char *start = buf, *end;
	unsigned long long value;
	unsigned long reg_addr;
	int ret_val = 0;
	size_t buf_size = min(count, sizeof(buf)-1);

	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;

	ret_val = is_fw_running(drv);
	if (ret_val) {
		pr_err("FW not running, cannot read SHIM registers\n");
		return ret_val;
	}

	while (*start == ' ')
		start++;
	end = start;
	while (isalnum(*end))
		end++;
	*end = 0;

	ret_val = kstrtoul(start, 16, &reg_addr);
	if (ret_val) {
		pr_err("kstrtoul failed, ret_val = %d\n", ret_val);
		goto put_pm_runtime;
	}
	if (!(SST_SHIM_BEGIN <= reg_addr && reg_addr < SST_SHIM_END)) {
		pr_err("invalid shim address: 0x%lx\n", reg_addr);
		ret_val = -EINVAL;
		goto put_pm_runtime;
	}

	start = end + 1;
	while (*start == ' ')
		start++;

	ret_val = kstrtoull(start, 16, &value);
	if (ret_val) {
		pr_err("kstrtoul failed, ret_val = %d\n", ret_val);
		goto put_pm_runtime;
	}

	pr_debug("writing shim: 0x%.2lx=0x%.8llx", reg_addr, value);

	if (drv->pci_id == SST_CLV_PCI_ID)
		sst_shim_write(drv->shim, reg_addr, (u32) value);
	else if ((drv->pci_id == SST_MRFLD_PCI_ID) ||
			(drv->pci_id == SST_BYT_PCI_ID))
		sst_shim_write64(drv->shim, reg_addr, (u64) value);

	/* Userspace has been fiddling around behind the kernel's back */
	add_taint(TAINT_USER, LOCKDEP_NOW_UNRELIABLE);
	ret_val = buf_size;

put_pm_runtime:
	sst_pm_runtime_put(drv);
	return ret_val;
}

static const struct file_operations sst_debug_shim_ops = {
	.open = simple_open,
	.read = sst_debug_shim_read,
	.write = sst_debug_shim_write,
	.llseek = default_llseek,
};

#define RESVD_DUMP_SZ		40
#define IA_LPE_MAILBOX_DUMP_SZ	100
#define LPE_IA_MAILBOX_DUMP_SZ	100
#define SCU_LPE_MAILBOX_DUMP_SZ	256
#define LPE_SCU_MAILBOX_DUMP_SZ	256

static inline int is_fw_running(struct intel_sst_drv *drv)
{
	pm_runtime_get_sync(drv->dev);
	atomic_inc(&drv->pm_usage_count);
	if (drv->sst_state != SST_FW_RUNNING) {
		pr_err("FW not running\n");
		sst_pm_runtime_put(drv);
		return -EFAULT;
	}
	return 0;
}

static inline int read_buffer_fromio(char *dest, unsigned int sz,
				     const u32 __iomem *from,
				     unsigned int num_dwords)
{
	int i;
	const unsigned int rowsz = 16, groupsz = 4;
	const unsigned int size = num_dwords * sizeof(u32);
	unsigned int linelen, printed = 0, remaining = size;

	u8 *tmp = kmalloc(size, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;
	memcpy_fromio(tmp, from, size);
	for (i = 0; i < size; i += rowsz) {
		linelen = min(remaining, rowsz);
		remaining -= rowsz;
		hex_dump_to_buffer(tmp + i, linelen, rowsz, groupsz,
				   dest + printed, sz - printed, false);
		printed += linelen * 2 + linelen / groupsz - 1;
		*(dest + printed++) = '\n';
		*(dest + printed) = 0;
	}
	kfree(tmp);
	return 0;
}

static inline int copy_sram_to_user_buffer(char __user *user_buf, size_t count, loff_t *ppos,
					   unsigned int num_dwords, const u32 __iomem *from,
					   u32 offset)
{
	ssize_t bytes_read;
	char *buf;
	int pos;
	unsigned int bufsz = 48 + sizeof(u32) * num_dwords * (2 + 1) + 1;

	buf = kmalloc(bufsz, GFP_KERNEL);
	if (!buf) {
		pr_err("%s: no memory\n", __func__);
		return -ENOMEM;
	}
	*buf = 0;
	pos = scnprintf(buf, 48, "Reading %u dwords from offset %#x\n",
			num_dwords, offset);
	read_buffer_fromio(buf + pos, bufsz - pos, from, num_dwords);
	bytes_read = simple_read_from_buffer(user_buf, count, ppos,
					     buf, strlen(buf));
	kfree(buf);
	return bytes_read;
}

static ssize_t sst_debug_sram_lpe_debug_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{

	struct intel_sst_drv *drv = file->private_data;
	int ret = 0;

	ret = is_fw_running(drv);
	if (ret)
		return ret;

	ret = copy_sram_to_user_buffer(user_buf, count, ppos, RESVD_DUMP_SZ,
				       (u32 *)(drv->mailbox + SST_RESERVED_OFFSET),
				       SST_RESERVED_OFFSET);
	sst_pm_runtime_put(drv);
	return ret;
}

static const struct file_operations sst_debug_sram_lpe_debug_ops = {
	.open = simple_open,
	.read = sst_debug_sram_lpe_debug_read,
	.llseek = default_llseek,
};

static ssize_t sst_debug_sram_lpe_checkpoint_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{

	struct intel_sst_drv *drv = file->private_data;
	int ret = 0;
	u32 offset;

	ret = is_fw_running(drv);
	if (ret)
		return ret;

	offset = sst_drv_ctx->pdata->debugfs_data->checkpoint_offset;

	ret = copy_sram_to_user_buffer(user_buf, count, ppos,
				sst_drv_ctx->pdata->debugfs_data->checkpoint_size,
				(u32 *)(drv->mailbox + offset), offset);
	sst_pm_runtime_put(drv);
	return ret;
}

static const struct file_operations sst_debug_sram_lpe_checkpoint_ops = {
	.open = simple_open,
	.read = sst_debug_sram_lpe_checkpoint_read,
	.llseek = default_llseek,
};

static ssize_t sst_debug_sram_ia_lpe_mbox_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{

	struct intel_sst_drv *drv = file->private_data;
	int ret = 0;

	ret = is_fw_running(drv);
	if (ret)
		return ret;
	ret = copy_sram_to_user_buffer(user_buf, count, ppos, IA_LPE_MAILBOX_DUMP_SZ,
			       (u32 *)(drv->ipc_mailbox + SST_MAILBOX_SEND),
			       SST_MAILBOX_SEND);
	sst_pm_runtime_put(drv);
	return ret;
}

static const struct file_operations sst_debug_sram_ia_lpe_mbox_ops = {
	.open = simple_open,
	.read = sst_debug_sram_ia_lpe_mbox_read,
	.llseek = default_llseek,
};

static ssize_t sst_debug_sram_lpe_ia_mbox_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{

	struct intel_sst_drv *drv = file->private_data;
	int ret = 0;

	ret = is_fw_running(drv);
	if (ret)
		return ret;

	ret = copy_sram_to_user_buffer(user_buf, count, ppos, LPE_IA_MAILBOX_DUMP_SZ,
		       (u32 *)(drv->ipc_mailbox + drv->mailbox_recv_offset),
		       drv->mailbox_recv_offset);
	sst_pm_runtime_put(drv);
	return ret;
}

static const struct file_operations sst_debug_sram_lpe_ia_mbox_ops = {
	.open = simple_open,
	.read = sst_debug_sram_lpe_ia_mbox_read,
	.llseek = default_llseek,
};

static ssize_t sst_debug_sram_lpe_scu_mbox_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	struct intel_sst_drv *drv = file->private_data;
	int ret = 0;

	ret = is_fw_running(drv);
	if (ret)
		return ret;
	ret = copy_sram_to_user_buffer(user_buf, count, ppos, LPE_SCU_MAILBOX_DUMP_SZ,
				       (u32 *)(drv->mailbox + SST_LPE_SCU_MAILBOX),
				       SST_LPE_SCU_MAILBOX);
	sst_pm_runtime_put(drv);
	return ret;
}

static const struct file_operations sst_debug_sram_lpe_scu_mbox_ops = {
	.open = simple_open,
	.read = sst_debug_sram_lpe_scu_mbox_read,
	.llseek = default_llseek,
};

static ssize_t sst_debug_sram_scu_lpe_mbox_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{	struct intel_sst_drv *drv = file->private_data;
	int ret = 0;

	ret = is_fw_running(drv);
	if (ret)
		return ret;
	ret = copy_sram_to_user_buffer(user_buf, count, ppos, SCU_LPE_MAILBOX_DUMP_SZ,
				       (u32 *)(drv->mailbox + SST_SCU_LPE_MAILBOX),
				       SST_SCU_LPE_MAILBOX);
	sst_pm_runtime_put(drv);
	return ret;
}

static const struct file_operations sst_debug_sram_scu_lpe_mbox_ops = {
	.open = simple_open,
	.read = sst_debug_sram_scu_lpe_mbox_read,
	.llseek = default_llseek,
};

static ssize_t sst_debug_lpe_log_enable_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct intel_sst_drv *drv = file->private_data;
	struct ipc_post *msg = NULL;
	char buf[32];
	int str_id = 0;	/* DUMMY, required by post message */
	struct snd_sst_lpe_log_params params;
	int ret_val = 0;
	char *start = buf, *end;
	int i = 0;
	u8 *addr;
	unsigned long tmp;

	size_t buf_size = min(count, sizeof(buf)-1);
	memset(&params, 0, sizeof(params));

	ret_val = is_fw_running(drv);
	if (ret_val)
		return ret_val;

	if (copy_from_user(buf, user_buf, buf_size)) {
		ret_val = -EFAULT;
		goto put_pm_runtime;
	}

	buf[buf_size] = 0;

	addr = &params.dbg_type;
	for (i = 0; i < (sizeof(params) - sizeof(u8)); i++) {
		while (*start == ' ')
			start++;
		end = start;
		while (isalnum(*end))
			end++;
		*end = 0;
		ret_val = kstrtoul(start, 16, &tmp);
		if (ret_val) {
			pr_err("kstrtoul failed, ret_val = %d\n", ret_val);
			goto put_pm_runtime;
		}
		*addr++ = (u8)tmp;
		start = end + 1;
	}

	pr_debug("dbg_type = %d module_id = %d log_level = %d\n",
			params.dbg_type, params.module_id, params.log_level);

	if (params.dbg_type < NO_DEBUG || params.dbg_type > PTI_DEBUG) {
		ret_val = -EINVAL;
		goto put_pm_runtime;
	}

	ret_val = sst_create_ipc_msg(&msg, true);
	if (ret_val != 0)
		goto put_pm_runtime;

	if ((sst_drv_ctx->pci_id != SST_MRFLD_PCI_ID) /* &&
		(sst_drv_ctx->pci_id != PCI_DEVICE_ID_INTEL_SST_MOOR) */) {
		sst_fill_header(&msg->header, IPC_IA_DBG_LOG_ENABLE, 1,
							str_id);
		msg->header.part.data = sizeof(u32) + sizeof(params);
		memcpy(msg->mailbox_data, &msg->header.full, sizeof(u32));
		memcpy(msg->mailbox_data + sizeof(u32), &params,
							sizeof(params));
	}
	drv->ops->sync_post_message(msg);
	ret_val = buf_size;
put_pm_runtime:
	sst_pm_runtime_put(drv);
	return ret_val;
}

/*
 * Circular buffer hdr -> 0x1000
 * log data starts at 0x1010
 */
static ssize_t sst_debug_lpe_log_enable_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	struct intel_sst_drv *drv = file->private_data;
	struct lpe_log_buf_hdr buf_hdr;
	size_t size1, size2, offset, bytes_read;
	char *buf = NULL;
	int ret;

	ret = is_fw_running(drv);
	if (ret)
		return ret;

	/* Get the sram lpe log buffer header */
	memcpy_fromio(&buf_hdr, (u32 *)(drv->mailbox + SST_SCU_LPE_MAILBOX),
							sizeof(buf_hdr));
	if (buf_hdr.rd_addr == buf_hdr.wr_addr) {
		pr_err("SRAM emptry\n");
		ret = -ENODATA;
		goto put_pm_runtime;
	} else if (buf_hdr.rd_addr < buf_hdr.wr_addr) {
		size1 = buf_hdr.wr_addr - buf_hdr.rd_addr;
		offset = (buf_hdr.rd_addr - buf_hdr.base_addr)
						+ SST_SCU_LPE_LOG_BUF;
		pr_debug("Size = %zu, offset = %zx\n", size1, offset);
		buf = vmalloc(size1);
		if (buf == NULL) {
			pr_err("Not enough memory to allocate\n");
			ret = -ENOMEM;
			goto put_pm_runtime;
		}
		memcpy_fromio(buf, (u32 *)(drv->mailbox + offset), size1);
		bytes_read = simple_read_from_buffer(user_buf, count, ppos,
							buf, size1);

		buf_hdr.rd_addr = buf_hdr.rd_addr + bytes_read;

	} else {
		/* Read including the end address as well */
		size1 = buf_hdr.end_addr - buf_hdr.rd_addr + 1;
		offset = (buf_hdr.rd_addr - buf_hdr.base_addr)
						+ SST_SCU_LPE_LOG_BUF;
		pr_debug("Size = %zu, offset = %zx\n", size1, offset);
		buf = vmalloc(size1);
		if (buf == NULL) {
			pr_err("Not enough memory to allocate\n");
			ret = -ENOMEM;
			goto put_pm_runtime;
		}
		memcpy_fromio(buf, (u32 *)(drv->mailbox + offset), size1);
		bytes_read = simple_read_from_buffer(user_buf, count, ppos,
							buf, size1);
		if (bytes_read != size1) {
			buf_hdr.rd_addr = buf_hdr.rd_addr + bytes_read;
			goto update_rd_ptr;
		}

		/* Wrap around lpe log buffer here */
		vfree(buf);
		buf = NULL;
		size2 = (buf_hdr.wr_addr - buf_hdr.base_addr);
		offset = SST_SCU_LPE_LOG_BUF;
		pr_debug("Size = %zu, offset = %zx\n", size2, offset);
		buf = vmalloc(size2);
		if (buf == NULL) {
			pr_err("Not enough memory to allocate\n");
			ret = -ENOMEM;
			goto put_pm_runtime;
		}
		memcpy_fromio(buf, (u32 *)(drv->mailbox + offset), size2);
		bytes_read += simple_read_from_buffer(user_buf,
				(count - bytes_read), ppos, buf, size2);
		buf_hdr.rd_addr = buf_hdr.base_addr + bytes_read - size1;

	}
update_rd_ptr:
	if (bytes_read != 0) {
		memcpy_toio((u32 *)(drv->mailbox + SST_SCU_LPE_MAILBOX +
				2 * sizeof(u32)), &(buf_hdr.rd_addr), sizeof(u32));
		pr_debug("read pointer restored\n");
	}
	vfree(buf);
	buf = NULL;
	ret = bytes_read;
put_pm_runtime:
	sst_pm_runtime_put(drv);
	return ret;
}

static const struct file_operations sst_debug_lpe_log_enable_ops = {
	.open = simple_open,
	.write = sst_debug_lpe_log_enable_write,
	.read = sst_debug_lpe_log_enable_read,
	.llseek = default_llseek,
};

static ssize_t sst_debug_rtpm_read(struct file *file, char __user *user_buf,
				   size_t count, loff_t *ppos)
{
	struct intel_sst_drv *drv = file->private_data;
	char *status;

	int usage = atomic_read(&drv->pm_usage_count);

	pr_debug("RTPM usage: %d\n", usage);
	status = drv->debugfs.runtime_pm_status ? "enabled\n" : "disabled\n";
	return simple_read_from_buffer(user_buf, count, ppos,
			status, strlen(status));
}

static ssize_t sst_debug_rtpm_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct intel_sst_drv *drv = file->private_data;
	char buf[16];
	int sz = min(count, sizeof(buf)-1);

	int usage = atomic_read(&drv->pm_usage_count);

	pr_debug("RTPM Usage: %d\n", usage);
	if (copy_from_user(buf, user_buf, sz))
		return -EFAULT;
	buf[sz] = 0;

	if (!strncmp(buf, "enable\n", sz)) {
		/* already enabled? */
		if (drv->debugfs.runtime_pm_status)
			return -EINVAL;
		drv->debugfs.runtime_pm_status = 1;
		pm_runtime_allow(drv->dev);
		sz = 6; /* strlen("enable") */
	} else if (!strncmp(buf, "disable\n", sz)) {
		if (!drv->debugfs.runtime_pm_status)
			return -EINVAL;
		drv->debugfs.runtime_pm_status = 0;
		pm_runtime_forbid(drv->dev);
		sz = 7; /* strlen("disable") */
	} else
		return -EINVAL;
	return sz;
}

static const struct file_operations sst_debug_rtpm_ops = {
	.open = simple_open,
	.read = sst_debug_rtpm_read,
	.write = sst_debug_rtpm_write,
	.llseek = default_llseek,
};


static ssize_t sst_debug_readme_read(struct file *file, char __user *user_buf,
				   size_t count, loff_t *ppos)
{
	const char *buf =
		"\nAll files can be read using 'cat'\n"
		"1. 'echo disable > runtime_pm' disables runtime PM and will prevent SST from suspending.\n"
		"To enable runtime PM, echo 'enable' to runtime_pm. Dmesg will print the runtime pm usage\n"
		"if logs are enabled.\n"
		"2. Write to shim register using 'echo <addr> <value> > shim_dump'.\n"
		"Valid address range is between 0x00 to 0x80 in increments of 8.\n"
		"3. echo 1 > fw_clear_context , This sets the flag to skip the context restore\n"
		"4. echo 1 > fw_clear_cache , This sets the flag to clear the cached copy of firmware\n"
		"5. echo 1 > fw_reset_state ,This sets the fw state to RESET\n"
		"6. echo memcpy > fw_dwnld_mode, This will set the firmware download mode to memcpy\n"
		"   echo lli > fw_dwnld_mode, This will set the firmware download mode to\n"
					"dma lli mode\n"
		"   echo dma > fw_dwnld_mode, This will set the firmware download mode to\n"
					"dma single block mode\n"
		"7. iram_dump, dram_dump, interfaces provide mmap support to\n"
		"get the iram and dram dump, these buffers will have data only\n"
		"after the recovery is triggered\n"
		"8. lpe_stack, dumps lpe stack area."
		"Valid only when LPE is active\n";

	const char *ctp_buf =
		"8. Enable input clock by 'echo enable > osc_clk0'.\n"
		"This prevents the input OSC clock from switching off till it is disabled by\n"
		"'echo disable > osc_clk0'. The status of the clock indicated who are using it.\n"
		"9. lpe_log_enable usage:\n"
		"	echo <dbg_type> <module_id> <log_level> > lpe_log_enable.\n"
		"10. cat fw_ssp_reg,This will dump the ssp register contents\n"
		"11. cat fw_dma_reg,This will dump the dma register contents\n";

	const char *mrfld_buf =
		"8. lpe_log_enable usage:\n"
		"	echo <dbg_type> <module_id> <log_level> > lpe_log_enable.\n"
		"9. cat fw_ssp_reg,This will dump the ssp register contents\n"
		"10. cat fw_dma_reg,This will dump the dma register contents\n"
		"11. ddr_imr_dump interface provides mmap support to get the imr dump,\n"
		"this buffer will have data only after the recovery is triggered\n"
		"12. ipc usage:\n"
		"\t ipc file works only in binary mode. The ipc format is <IPC hdr><dsp hdr><payload>.\n"
		"\t drv_id in the ipc header will be overwritten with unique driver id in the driver\n";

	char *readme = NULL;
	const char *buf2 = NULL;
	int size, ret = 0;

	switch (sst_drv_ctx->pci_id) {
	case SST_CLV_PCI_ID:
		size = strlen(buf) + strlen(ctp_buf) + 2;
		buf2 = ctp_buf;
		break;
	case SST_MRFLD_PCI_ID:
		size = strlen(buf) + strlen(mrfld_buf) + 2;
		buf2 = mrfld_buf;
		break;
	default:
		size = strlen(buf) + 1;
	};

	readme = kmalloc(size, GFP_KERNEL);
	if (readme == NULL) {
		pr_err("%s: no memory\n", __func__);
		return -ENOMEM;
	}

	if (buf2)
		sprintf(readme, "%s%s\n", buf, buf2);
	else
		sprintf(readme, "%s\n", buf);

	ret = simple_read_from_buffer(user_buf, count, ppos,
			readme, strlen(readme));
	kfree(readme);
	return ret;
}

static const struct file_operations sst_debug_readme_ops = {
	.open = simple_open,
	.read = sst_debug_readme_read,
};

static ssize_t sst_debug_osc_clk0_read(struct file *file, char __user *user_buf,
				   size_t count, loff_t *ppos)
{
	char status[16];
	int mode = -1;
#ifdef CONFIG_INTEL_SCU_IPC_UTIL
	mode = intel_scu_ipc_set_osc_clk0(0, CLK0_QUERY);
#endif

	snprintf(status, 16, "0x%x\n", mode);
	return simple_read_from_buffer(user_buf, count, ppos,
			status, strlen(status));
}

static ssize_t sst_debug_osc_clk0_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[16];
	int sz = min(count, sizeof(buf)-1);

	if (copy_from_user(buf, user_buf, sz))
		return -EFAULT;
	buf[sz] = 0;

#ifdef CONFIG_INTEL_SCU_IPC_UTIL
	if (!strncmp(buf, "enable\n", sz)) {
		intel_scu_ipc_set_osc_clk0(true, CLK0_DEBUG);
		sz = 6; /* strlen("enable") */
	} else if (!strncmp(buf, "disable\n", sz)) {
		intel_scu_ipc_set_osc_clk0(false, CLK0_DEBUG);
		sz = 7; /* strlen("disable") */
	} else
		return -EINVAL;
#endif
	return sz;
}

static const struct file_operations sst_debug_osc_clk0_ops = {
	.open = simple_open,
	.read = sst_debug_osc_clk0_read,
	.write = sst_debug_osc_clk0_write,
};

#define VLV2_BASE 0xFED0306C
static ssize_t sst_debug_fw_clear_cntx_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	char *status;
	void __iomem *addr;
	u32 value;

	addr = ioremap_nocache(VLV2_BASE, 0x100);
	if (!addr) {
		printk("Unable to MAP VLV2 CLK\n");
		return -EINVAL;
	}
	value = readl(addr);
	printk("VLV2 Value = %d\n", value);
#if 1
	status = atomic_read(&sst_drv_ctx->fw_clear_context) ? \
			"clear fw cntx\n" : "do not clear fw cntx\n";
#endif
	iounmap(addr);
	return simple_read_from_buffer(user_buf, count, ppos,
			status, strlen(status));

}

static ssize_t sst_debug_fw_clear_cntx_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)

{
	char buf[16];
	void __iomem *addr;
	int sz = min(count, sizeof(buf)-1);

	addr = ioremap_nocache(VLV2_BASE, 0x100);
	if (!addr) {
		printk("Unable to MAP VLV2 CLK\n");
		return -EINVAL;
	}

	if (copy_from_user(buf, user_buf, sz))
		return -EFAULT;
	buf[sz] = 0;
#if 0
	if (!strncmp(buf, "1\n", sz))
		atomic_set(&sst_drv_ctx->fw_clear_context, 1);
	else
		atomic_set(&sst_drv_ctx->fw_clear_context, 0);
#endif
	printk("Writing to address\n");
	if (!strncmp(buf, "1\n", sz))
		writel(0x1, addr);
	else
		 writel(0x2, addr);

	iounmap(addr);
	return sz;

}

static const struct file_operations sst_debug_fw_clear_cntx = {
	.open = simple_open,
	.read = sst_debug_fw_clear_cntx_read,
	.write = sst_debug_fw_clear_cntx_write,
};

static ssize_t sst_debug_fw_clear_cache_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	char *status;

	status = atomic_read(&sst_drv_ctx->fw_clear_cache) ? \
			"cache clear flag set\n" : "cache clear flag not set\n";

	return simple_read_from_buffer(user_buf, count, ppos,
			status, strlen(status));

}

static ssize_t sst_debug_fw_clear_cache_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)

{
	char buf[16];
	int sz = min(count, sizeof(buf)-1);

	if (copy_from_user(buf, user_buf, sz))
		return -EFAULT;
	buf[sz] = 0;

	if (!strncmp(buf, "1\n", sz))
		atomic_set(&sst_drv_ctx->fw_clear_cache, 1);
	else
		return -EINVAL;

	return sz;
}

static const struct file_operations sst_debug_fw_clear_cache = {
	.open = simple_open,
	.read = sst_debug_fw_clear_cache_read,
	.write = sst_debug_fw_clear_cache_write,
};

static ssize_t sst_debug_fw_reset_state_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	char state[16];

	sprintf(state, "%d\n", sst_drv_ctx->sst_state);

	return simple_read_from_buffer(user_buf, count, ppos,
			state, strlen(state));

}

static ssize_t sst_debug_fw_reset_state_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)

{
	char buf[16];
	int sz = min(count, sizeof(buf)-1);

	if (copy_from_user(buf, user_buf, sz))
		return -EFAULT;
	buf[sz] = 0;

	if (!strncmp(buf, "1\n", sz))
		sst_set_fw_state_locked(sst_drv_ctx, SST_RESET);
	else
		return -EINVAL;

	return sz;

}

static const struct file_operations sst_debug_fw_reset_state = {
	.open = simple_open,
	.read = sst_debug_fw_reset_state_read,
	.write = sst_debug_fw_reset_state_write,
};

static ssize_t sst_debug_dwnld_mode_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	char *state = "error\n";

	if (sst_drv_ctx->use_dma == 0) {
		state = "memcpy\n";
	} else if (sst_drv_ctx->use_dma == 1) {
		state = sst_drv_ctx->use_lli ? \
				"lli\n" : "dma\n";

	}

	return simple_read_from_buffer(user_buf, count, ppos,
			state, strlen(state));

}

static ssize_t sst_debug_dwnld_mode_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)

{
	char buf[16];
	int sz = min(count, sizeof(buf)-1);

	if (atomic_read(&sst_drv_ctx->pm_usage_count) &&
	    sst_drv_ctx->sst_state != SST_RESET) {
		pr_err("FW should be in suspended/RESET state\n");
		return -EFAULT;
	}

	if (copy_from_user(buf, user_buf, sz))
		return -EFAULT;
	buf[sz] = '\0';

	/* Firmware needs to be downloaded again to populate the lists */
	atomic_set(&sst_drv_ctx->fw_clear_cache, 1);

	if (!strncmp(buf, "memcpy\n", sz)) {
		sst_drv_ctx->use_dma = 0;
	} else if (!strncmp(buf, "lli\n", sz)) {
		sst_drv_ctx->use_dma = 1;
		sst_drv_ctx->use_lli = 1;
	} else if (!strncmp(buf, "dma\n", sz)) {
		sst_drv_ctx->use_dma = 1;
		sst_drv_ctx->use_lli = 0;
	}
	return sz;

}

static const struct file_operations sst_debug_dwnld_mode = {
	.open = simple_open,
	.read = sst_debug_dwnld_mode_read,
	.write = sst_debug_dwnld_mode_write,
};

static int dump_ssp_port(void __iomem *ssp_base, char *buf, int pos)
{
	int index = 0;

	while (index < ARRAY_SIZE(ssp_reg_off)) {
		pos += sprintf(buf + pos, "Reg: 0x%x: 0x%x\n", ssp_reg_off[index],
			sst_reg_read(ssp_base, ssp_reg_off[index]));
		index++;
	}
	return pos;
}

static ssize_t sst_debug_ssp_reg_read(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	char *buf;
	int i, pos = 0, off = 0;
	struct intel_sst_drv *drv = file->private_data;
	int num_ssp, buf_size, ret;

	num_ssp = sst_drv_ctx->pdata->debugfs_data->num_ssp;
	buf_size = DEBUGFS_SSP_BUF_SIZE * num_ssp;

	buf = kmalloc(buf_size, GFP_KERNEL);
	if (!buf) {
		pr_err("%s: no memory\n", __func__);
		return -ENOMEM;
	}

	ret = is_fw_running(drv);
	if (ret)
		goto err;

	buf[0] = 0;

	for (i = 0; i < num_ssp ; i++) {
		if (!sst_drv_ctx->debugfs.ssp[i]) {
			pr_err("ssp %d port not mapped\n", i);
			continue;
		}
		off = sst_drv_ctx->pdata->debugfs_data->ssp_reg_size * i;
		pos = dump_ssp_port((sst_drv_ctx->debugfs.ssp[i]), buf, pos);
	}
	sst_pm_runtime_put(drv);

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, pos);
err:
	kfree(buf);
	return ret;
}

static const struct file_operations sst_debug_ssp_reg = {
		.open = simple_open,
		.read = sst_debug_ssp_reg_read,
};

static int dump_dma_reg(char *buf, int pos, int dma)
{
	int i, index = 0;
	int off = 0 ;
	void __iomem *dma_reg;

	if (!sst_drv_ctx->debugfs.dma_reg[dma]) {
		pr_err("dma %d not mapped\n", dma);
		return pos;
	}

	pos += sprintf(buf + pos, "\nDump DMA%d Reg\n\n", dma);

	dma_reg = sst_drv_ctx->debugfs.dma_reg[dma];

	/* Dump the DMA channel registers */
	for (i = 0; i < DMA_NUM_CH; i++) {
		pos += sprintf(buf + pos, "SAR%d: 0x%x: 0x%llx\n", i, off,
			sst_reg_read64(dma_reg, off));
		off += 8;

		pos += sprintf(buf + pos, "DAR%d: 0x%x: 0x%llx\n", i, off,
			sst_reg_read64(dma_reg, off));
		off += 8;

		pos += sprintf(buf + pos, "LLP%d: 0x%x: 0x%llx\n", i, off,
			sst_reg_read64(dma_reg, off));
		off += 8;

		pos += sprintf(buf + pos, "CTL%d: 0x%x: 0x%llx\n", i, off,
			sst_reg_read64(dma_reg, off));
		off += 0x28;

		pos += sprintf(buf + pos, "CFG%d: 0x%x: 0x%llx\n", i, off,
			sst_reg_read64(dma_reg, off));
		off += 0x18;
	}

	/* Dump the remaining DMA registers */
	while (index < ARRAY_SIZE(dma_reg_off)) {
		pos += sprintf(buf + pos, "Reg: 0x%x: 0x%llx\n", dma_reg_off[index],
				sst_reg_read64(dma_reg, dma_reg_off[index]));
		index++;
	}
	return pos;
}

static ssize_t sst_debug_dma_reg_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	char *buf;
	int pos = 0;
	int ret, i;
	struct intel_sst_drv *drv = file->private_data;
	int num_dma, buf_size;

	num_dma = sst_drv_ctx->pdata->debugfs_data->num_dma;
	buf_size = DEBUGFS_DMA_BUF_SIZE * num_dma;

	buf = kmalloc(buf_size, GFP_KERNEL);
	if (!buf) {
		pr_err("%s: no memory\n", __func__);
		return -ENOMEM;
	}

	ret = is_fw_running(drv);
	if (ret)
		goto err;

	buf[0] = 0;

	for (i = 0; i < num_dma; i++)
		pos = dump_dma_reg(buf, pos, i);

	sst_pm_runtime_put(drv);

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, pos);
err:
	kfree(buf);
	return ret;
}

static const struct file_operations sst_debug_dma_reg = {
		.open = simple_open,
		.read = sst_debug_dma_reg_read,
};

static ssize_t sst_debug_lpe_stack_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	int retval = 0;
	struct intel_sst_drv *sst = file->private_data;
	void __iomem *addr;


	retval = is_fw_running(sst);
	if (retval)
		return retval;

	addr = sst->dram + SST_LPE_STACK_OFFSET;

	pr_debug("Dumping DCCM from %p, num_dwrds %d...\n",
		 (u32 *)addr, SST_LPE_STACK_SIZE);

	retval = copy_sram_to_user_buffer(user_buf, count, ppos,
			SST_LPE_STACK_SIZE/(sizeof(u32)), (u32 *)(addr), 0);
	sst_pm_runtime_put(sst);


	return retval;
}

static const struct file_operations sst_debug_lpe_stack_dump = {
		.open = simple_open,
		.read = sst_debug_lpe_stack_read,
};

/**
 * sst_debug_remap - function remaps the iram/dram buff to userspace
 *
 * @vma: vm_area_struct passed from userspace
 * @buf: Physical addr of the pointer to be remapped
 * @type: type of the buffer
 *
 * Remaps the kernel buffer to the userspace
 */
static int sst_debug_remap(struct vm_area_struct *vma, char *buf,
					enum sst_ram_type type)
{
	int retval, length;
	void *mem_area;

	if (!buf)
		return -EIO;

	length = vma->vm_end - vma->vm_start;
	pr_debug("iram length 0x%x\n", length);

	/* round it up to the page bondary  */
	mem_area = (void *)PAGE_ALIGN((unsigned long)buf);

	/* map the whole physically contiguous area in one piece  */
	retval = remap_pfn_range(vma,
			vma->vm_start,
			virt_to_phys((void *)mem_area) >> PAGE_SHIFT,
			length,
			vma->vm_page_prot);
	if (retval)
		pr_err("mapping failed %d ", retval);
	return retval;
}

int sst_debug_iram_dump_mmap(struct file *file, struct vm_area_struct *vma)
{
	int retval;
	struct intel_sst_drv *sst = sst_drv_ctx;

	retval = sst_debug_remap(vma, sst->dump_buf.iram_buf.buf, SST_IRAM);

	return retval;
}

static const struct file_operations sst_debug_iram_dump = {
	.open = simple_open,
	.mmap = sst_debug_iram_dump_mmap,
};

int sst_debug_dram_dump_mmap(struct file *file, struct vm_area_struct *vma)
{
	int retval;
	struct intel_sst_drv *sst = sst_drv_ctx;

	retval = sst_debug_remap(vma, sst->dump_buf.dram_buf.buf, SST_DRAM);

	return retval;
}

static const struct file_operations sst_debug_dram_dump = {
	.open = simple_open,
	.mmap = sst_debug_dram_dump_mmap,
};

int sst_debug_ddr_imr_dump_mmap(struct file *file, struct vm_area_struct *vma)
{
	int retval;
	struct intel_sst_drv *sst = sst_drv_ctx;

	retval = sst_debug_remap(vma, sst->ddr, 0);

	return retval;
}

static const struct file_operations sst_debug_ddr_imr_dump = {
	.open = simple_open,
	.mmap = sst_debug_ddr_imr_dump_mmap,
};

static ssize_t sst_debug_ipc_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct intel_sst_drv *ctx = (struct intel_sst_drv *)file->private_data;
	unsigned char *buf;
	struct sst_block *block = NULL;
	struct ipc_dsp_hdr *dsp_hdr;
	struct ipc_post *msg = NULL;
	int ret, res_rqd, msg_id, drv_id;
	u32 low_payload;

	if (count > 1024)
		return -EINVAL;

	ret = is_fw_running(ctx);
	if (ret)
		return ret;

	buf = kzalloc((sizeof(unsigned char) * (count)), GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		goto put_pm_runtime;
	}
	if (copy_from_user(buf, user_buf, count)) {
		ret = -EFAULT;
		goto free_mem;
	}

	if (sst_create_ipc_msg(&msg, true)) {
		ret = -ENOMEM;
		goto free_mem;
	}

	msg->mrfld_header.full = *((u64 *)buf);
	pr_debug("ipc hdr: %llx\n", msg->mrfld_header.full);

	/* Override the drv id with unique drv id */
	drv_id = sst_assign_pvt_id(ctx);
	msg->mrfld_header.p.header_high.part.drv_id = drv_id;

	res_rqd = msg->mrfld_header.p.header_high.part.res_rqd;
	msg_id = msg->mrfld_header.p.header_high.part.msg_id;
	pr_debug("res_rqd: %d, msg_id: %d, drv_id: %d\n",
					res_rqd, msg_id, drv_id);
	if (res_rqd) {
		block = sst_create_block(ctx, msg_id, drv_id);
		if (block == NULL) {
			ret = -ENOMEM;
			kfree(msg);
			goto free_mem;
		}
	}

	dsp_hdr = (struct ipc_dsp_hdr *)(buf + 8);
	pr_debug("dsp hdr: %llx\n", *((u64 *)(dsp_hdr)));
	low_payload = msg->mrfld_header.p.header_low_payload;
	if (low_payload > (1024 - sizeof(union ipc_header_mrfld))) {
		pr_err("Invalid low payload length: %x\n", low_payload);
		ret = -EINVAL;
		kfree(msg);
		goto free_block;
	}

	memcpy(msg->mailbox_data, (buf+(sizeof(union ipc_header_mrfld))),
			low_payload);
	sst_add_to_dispatch_list_and_post(ctx, msg);
	if (res_rqd) {
		ret = sst_wait_timeout(ctx, block);
		if (ret) {
			pr_err("%s: fw returned err %d\n", __func__, ret);
			goto free_block;
		}

		if (msg_id == IPC_GET_PARAMS) {
			unsigned char *r = block->data;
			/* Copy the IPC header first and then append dsp header
			 * and payload data*/
			memcpy(ctx->debugfs.get_params_data, &msg->mrfld_header.full, sizeof(msg->mrfld_header.full));
			memcpy(ctx->debugfs.get_params_data + sizeof(msg->mrfld_header.full), r, dsp_hdr->length);
			ctx->debugfs.get_params_len = sizeof(msg->mrfld_header.full) + dsp_hdr->length;
		}

	}
	ret = count;
free_block:
	if (res_rqd)
		sst_free_block(sst_drv_ctx, block);
free_mem:
	kfree(buf);
put_pm_runtime:
	sst_pm_runtime_put(ctx);
	return ret;
}

static ssize_t sst_debug_ipc_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	struct intel_sst_drv *ctx = (struct intel_sst_drv *)file->private_data;
	return simple_read_from_buffer(user_buf, count, ppos,
			ctx->debugfs.get_params_data,
			ctx->debugfs.get_params_len);
}

static const struct file_operations sst_debug_ipc_ops = {
	.open = simple_open,
	.write = sst_debug_ipc_write,
	.read = sst_debug_ipc_read,
	.llseek = default_llseek,
};

struct sst_debug {
	const char *name;
	const struct file_operations *fops;
	umode_t mode;
};

static const struct sst_debug sst_common_dbg_entries[] = {
	{"runtime_pm", &sst_debug_rtpm_ops, 0600},
	{"shim_dump", &sst_debug_shim_ops, 0600},
	{"fw_clear_context", &sst_debug_fw_clear_cntx, 0600},
	{"fw_clear_cache", &sst_debug_fw_clear_cache, 0600},
	{"fw_reset_state", &sst_debug_fw_reset_state, 0600},
	{"fw_dwnld_mode", &sst_debug_dwnld_mode, 0600},
	{"iram_dump", &sst_debug_iram_dump, 0400},
	{"dram_dump", &sst_debug_dram_dump, 0400},
	{"sram_ia_lpe_mailbox", &sst_debug_sram_ia_lpe_mbox_ops, 0400},
	{"sram_lpe_ia_mailbox", &sst_debug_sram_lpe_ia_mbox_ops, 0400},
	{"README", &sst_debug_readme_ops, 0400},
	{"lpe_stack", &sst_debug_lpe_stack_dump, 0400},
};

static const struct sst_debug ctp_dbg_entries[] = {
	{"sram_lpe_debug", &sst_debug_sram_lpe_debug_ops, 0400},
	{"sram_lpe_checkpoint", &sst_debug_sram_lpe_checkpoint_ops, 0400},
	{"sram_lpe_scu_mailbox", &sst_debug_sram_lpe_scu_mbox_ops, 0400},
	{"sram_scu_lpe_mailbox", &sst_debug_sram_scu_lpe_mbox_ops, 0400},
	{"lpe_log_enable", &sst_debug_lpe_log_enable_ops, 0400},
	{"fw_ssp_reg", &sst_debug_ssp_reg, 0400},
	{"fw_dma_reg", &sst_debug_dma_reg, 0400},
	{"osc_clk0", &sst_debug_osc_clk0_ops, 0600},
};

static const struct sst_debug mrfld_dbg_entries[] = {
	{"sram_lpe_checkpoint", &sst_debug_sram_lpe_checkpoint_ops, 0400},
	{"fw_ssp_reg", &sst_debug_ssp_reg, 0400},
	{"fw_dma_reg", &sst_debug_dma_reg, 0400},
	{"ddr_imr_dump", &sst_debug_ddr_imr_dump, 0400},
	{"ipc", &sst_debug_ipc_ops, 0400},
};

void sst_debugfs_create_files(struct intel_sst_drv *sst,
			const struct sst_debug *entries, int size)
{
	int i;

	for (i = 0; i < size; i++) {
		struct dentry *dentry;
		const struct sst_debug *entry = &entries[i];

		dentry = debugfs_create_file(entry->name, entry->mode,
				sst->debugfs.root, sst, entry->fops);
		if (dentry == NULL) {
			pr_err("Failed to create %s file\n", entry->name);
			return;
		}
	}
}

void sst_debugfs_init(struct intel_sst_drv *sst)
{
	int size = 0;
	const struct sst_debug *debug = NULL;

	sst->debugfs.root = debugfs_create_dir("sst", NULL);
	if (IS_ERR(sst->debugfs.root) || !sst->debugfs.root) {
		pr_err("Failed to create debugfs directory\n");
		return;
	}

	sst_debugfs_create_files(sst, sst_common_dbg_entries,
				ARRAY_SIZE(sst_common_dbg_entries));

	/* Initial status is enabled */
	sst->debugfs.runtime_pm_status = 1;

	if ((sst->pci_id == SST_MRFLD_PCI_ID) /* ||
			(sst->pci_id == PCI_DEVICE_ID_INTEL_SST_MOOR) */) {
		debug = mrfld_dbg_entries;
		size = ARRAY_SIZE(mrfld_dbg_entries);
	} else if (sst->pci_id == SST_CLV_PCI_ID) {
		debug = ctp_dbg_entries;
		size = ARRAY_SIZE(ctp_dbg_entries);
	}

	if (debug)
		sst_debugfs_create_files(sst, debug, size);

}

void sst_debugfs_exit(struct intel_sst_drv *sst)
{
	if (sst->debugfs.runtime_pm_status)
		pm_runtime_allow(sst->dev);
	debugfs_remove_recursive(sst->debugfs.root);
}
