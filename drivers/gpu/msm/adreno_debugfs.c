/* Copyright (c) 2002,2008-2011, Code Aurora Forum. All rights reserved.
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

#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#include "kgsl.h"
#include "adreno_postmortem.h"
#include "adreno.h"

#include "a2xx_reg.h"

unsigned int kgsl_cff_dump_enable;
int kgsl_pm_regs_enabled;

static uint32_t kgsl_ib_base;
static uint32_t kgsl_ib_size;

static struct dentry *pm_d_debugfs;

static int pm_dump_set(void *data, u64 val)
{
	struct kgsl_device *device = data;

	if (val) {
		mutex_lock(&device->mutex);
		adreno_postmortem_dump(device, 1);
		mutex_unlock(&device->mutex);
	}

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(pm_dump_fops,
			NULL,
			pm_dump_set, "%llu\n");

static int pm_regs_enabled_set(void *data, u64 val)
{
	kgsl_pm_regs_enabled = val ? 1 : 0;
	return 0;
}

static int pm_regs_enabled_get(void *data, u64 *val)
{
	*val = kgsl_pm_regs_enabled;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(pm_regs_enabled_fops,
			pm_regs_enabled_get,
			pm_regs_enabled_set, "%llu\n");


static int kgsl_cff_dump_enable_set(void *data, u64 val)
{
#ifdef CONFIG_MSM_KGSL_CFF_DUMP
	kgsl_cff_dump_enable = (val != 0);
	return 0;
#else
	return -EINVAL;
#endif
}

static int kgsl_cff_dump_enable_get(void *data, u64 *val)
{
	*val = kgsl_cff_dump_enable;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(kgsl_cff_dump_enable_fops, kgsl_cff_dump_enable_get,
			kgsl_cff_dump_enable_set, "%llu\n");

static int kgsl_dbgfs_open(struct inode *inode, struct file *file)
{
	file->f_mode &= ~(FMODE_PREAD | FMODE_PWRITE);
	file->private_data = inode->i_private;
	return 0;
}

static int kgsl_dbgfs_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int kgsl_hex_dump(const char *prefix, int c, uint8_t *data,
	int rowc, int linec, char __user *buff)
{
	int ss;
	/* Prefix of 20 chars max, 32 bytes per row, in groups of four - that's
	 * 8 groups at 8 chars per group plus a space, plus new-line, plus
	 * ending character */
	char linebuf[20 + 64 + 1 + 1];

	ss = snprintf(linebuf, sizeof(linebuf), prefix, c);
	hex_dump_to_buffer(data, linec, rowc, 4, linebuf+ss,
		sizeof(linebuf)-ss, 0);
	strlcat(linebuf, "\n", sizeof(linebuf));
	linebuf[sizeof(linebuf)-1] = 0;
	ss = strlen(linebuf);
	if (copy_to_user(buff, linebuf, ss+1))
		return -EFAULT;
	return ss;
}

static ssize_t kgsl_ib_dump_read(
	struct file *file,
	char __user *buff,
	size_t buff_count,
	loff_t *ppos)
{
	int i, count = kgsl_ib_size, remaining, pos = 0, tot = 0, ss;
	struct kgsl_device *device = file->private_data;
	const int rowc = 32;
	unsigned int pt_base, ib_memsize;
	uint8_t *base_addr;
	char linebuf[80];

	if (!ppos || !device || !kgsl_ib_base)
		return 0;

	kgsl_regread(device, MH_MMU_PT_BASE, &pt_base);
	base_addr = kgsl_sharedmem_convertaddr(device, pt_base, kgsl_ib_base,
		&ib_memsize);

	if (!base_addr)
		return 0;

	pr_info("%s ppos=%ld, buff_count=%d, count=%d\n", __func__, (long)*ppos,
		buff_count, count);
	ss = snprintf(linebuf, sizeof(linebuf), "IB: base=%08x(%08x"
		"), size=%d, memsize=%d\n", kgsl_ib_base,
		(uint32_t)base_addr, kgsl_ib_size, ib_memsize);
	if (*ppos == 0) {
		if (copy_to_user(buff, linebuf, ss+1))
			return -EFAULT;
		tot += ss;
		buff += ss;
		*ppos += ss;
	}
	pos += ss;
	remaining = count;
	for (i = 0; i < count; i += rowc) {
		int linec = min(remaining, rowc);

		remaining -= rowc;
		ss = kgsl_hex_dump("IB: %05x: ", i, base_addr, rowc, linec,
			buff);
		if (ss < 0)
			return ss;

		if (pos >= *ppos) {
			if (tot+ss >= buff_count) {
				ss = copy_to_user(buff, "", 1);
				return tot;
			}
			tot += ss;
			buff += ss;
			*ppos += ss;
		}
		pos += ss;
		base_addr += linec;
	}

	return tot;
}

static ssize_t kgsl_ib_dump_write(
	struct file *file,
	const char __user *buff,
	size_t count,
	loff_t *ppos)
{
	char local_buff[64];

	if (count >= sizeof(local_buff))
		return -EFAULT;

	if (copy_from_user(local_buff, buff, count))
		return -EFAULT;

	local_buff[count] = 0;	/* end of string */
	sscanf(local_buff, "%x %d", &kgsl_ib_base, &kgsl_ib_size);

	pr_info("%s: base=%08X size=%d\n", __func__, kgsl_ib_base,
		kgsl_ib_size);

	return count;
}

static const struct file_operations kgsl_ib_dump_fops = {
	.open = kgsl_dbgfs_open,
	.release = kgsl_dbgfs_release,
	.read = kgsl_ib_dump_read,
	.write = kgsl_ib_dump_write,
};

static int kgsl_regread_nolock(struct kgsl_device *device,
	unsigned int offsetwords, unsigned int *value)
{
	unsigned int *reg;

	if (offsetwords*sizeof(uint32_t) >= device->regspace.sizebytes) {
		KGSL_DRV_ERR(device, "invalid offset %d\n", offsetwords);
		return -ERANGE;
	}

	reg = (unsigned int *)(device->regspace.mmio_virt_base
				+ (offsetwords << 2));
	*value = __raw_readl(reg);
	return 0;
}

#define ADRENO_ISTORE_START 0x5000
static ssize_t kgsl_istore_read(
	struct file *file,
	char __user *buff,
	size_t buff_count,
	loff_t *ppos)
{
	int i, count, remaining, pos = 0, tot = 0;
	struct kgsl_device *device = file->private_data;
	const int rowc = 8;
	struct adreno_device *adreno_dev;

	if (!ppos || !device)
		return 0;

	adreno_dev = ADRENO_DEVICE(device);
	count = adreno_dev->istore_size * ADRENO_ISTORE_WORDS;
	remaining = count;
	for (i = 0; i < count; i += rowc) {
		unsigned int vals[rowc];
		int j, ss;
		int linec = min(remaining, rowc);
		remaining -= rowc;

		if (pos >= *ppos) {
			for (j = 0; j < linec; ++j)
				kgsl_regread_nolock(device,
						    ADRENO_ISTORE_START + i + j,
						    vals + j);
		} else
			memset(vals, 0, sizeof(vals));

		ss = kgsl_hex_dump("IS: %04x: ", i, (uint8_t *)vals, rowc*4,
			linec*4, buff);
		if (ss < 0)
			return ss;

		if (pos >= *ppos) {
			if (tot+ss >= buff_count)
				return tot;
			tot += ss;
			buff += ss;
			*ppos += ss;
		}
		pos += ss;
	}

	return tot;
}

static const struct file_operations kgsl_istore_fops = {
	.open = kgsl_dbgfs_open,
	.release = kgsl_dbgfs_release,
	.read = kgsl_istore_read,
	.llseek = default_llseek,
};

typedef void (*reg_read_init_t)(struct kgsl_device *device);
typedef void (*reg_read_fill_t)(struct kgsl_device *device, int i,
	unsigned int *vals, int linec);
static ssize_t kgsl_reg_read(struct kgsl_device *device, int count,
	reg_read_init_t reg_read_init,
	reg_read_fill_t reg_read_fill, const char *prefix, char __user *buff,
	loff_t *ppos)
{
	int i, remaining;
	const int rowc = 8;

	if (!ppos || *ppos || !device)
		return 0;

	mutex_lock(&device->mutex);
	reg_read_init(device);
	remaining = count;
	for (i = 0; i < count; i += rowc) {
		unsigned int vals[rowc];
		int ss;
		int linec = min(remaining, rowc);
		remaining -= rowc;

		reg_read_fill(device, i, vals, linec);
		ss = kgsl_hex_dump(prefix, i, (uint8_t *)vals, rowc*4, linec*4,
			buff);
		if (ss < 0) {
			mutex_unlock(&device->mutex);
			return ss;
		}
		buff += ss;
		*ppos += ss;
	}
	mutex_unlock(&device->mutex);

	return *ppos;
}


static void kgsl_sx_reg_read_init(struct kgsl_device *device)
{
	kgsl_regwrite(device, REG_RBBM_PM_OVERRIDE2, 0xFF);
	kgsl_regwrite(device, REG_RBBM_DEBUG_CNTL, 0);
}

static void kgsl_sx_reg_read_fill(struct kgsl_device *device, int i,
	unsigned int *vals, int linec)
{
	int j;

	for (j = 0; j < linec; ++j) {
		kgsl_regwrite(device, REG_RBBM_DEBUG_CNTL, 0x1B00 | i);
		kgsl_regread(device, REG_RBBM_DEBUG_OUT, vals+j);
	}
}

static ssize_t kgsl_sx_debug_read(
	struct file *file,
	char __user *buff,
	size_t buff_count,
	loff_t *ppos)
{
	struct kgsl_device *device = file->private_data;
	return kgsl_reg_read(device, 0x1B, kgsl_sx_reg_read_init,
			     kgsl_sx_reg_read_fill, "SX: %02x: ", buff, ppos);
}

static const struct file_operations kgsl_sx_debug_fops = {
	.open = kgsl_dbgfs_open,
	.release = kgsl_dbgfs_release,
	.read = kgsl_sx_debug_read,
};

static void kgsl_cp_reg_read_init(struct kgsl_device *device)
{
	kgsl_regwrite(device, REG_RBBM_DEBUG_CNTL, 0);
}

static void kgsl_cp_reg_read_fill(struct kgsl_device *device, int i,
	unsigned int *vals, int linec)
{
	int j;

	for (j = 0; j < linec; ++j) {
		kgsl_regwrite(device, REG_RBBM_DEBUG_CNTL, 0x1628);
		kgsl_regread(device, REG_RBBM_DEBUG_OUT, vals+j);
		msleep(100);
	}
}

static ssize_t kgsl_cp_debug_read(
	struct file *file,
	char __user *buff,
	size_t buff_count,
	loff_t *ppos)
{
	struct kgsl_device *device = file->private_data;
	return kgsl_reg_read(device, 20, kgsl_cp_reg_read_init,
		kgsl_cp_reg_read_fill,
		"CP: %02x: ", buff, ppos);
}

static const struct file_operations kgsl_cp_debug_fops = {
	.open = kgsl_dbgfs_open,
	.release = kgsl_dbgfs_release,
	.read = kgsl_cp_debug_read,
};

static void kgsl_mh_reg_read_init(struct kgsl_device *device)
{
	kgsl_regwrite(device, REG_RBBM_DEBUG_CNTL, 0);
}

static void kgsl_mh_reg_read_fill(struct kgsl_device *device, int i,
	unsigned int *vals, int linec)
{
	int j;

	for (j = 0; j < linec; ++j) {
		kgsl_regwrite(device, MH_DEBUG_CTRL, i+j);
		kgsl_regread(device, MH_DEBUG_DATA, vals+j);
	}
}

static ssize_t kgsl_mh_debug_read(
	struct file *file,
	char __user *buff,
	size_t buff_count,
	loff_t *ppos)
{
	struct kgsl_device *device = file->private_data;
	return kgsl_reg_read(device, 0x40, kgsl_mh_reg_read_init,
		kgsl_mh_reg_read_fill,
		"MH: %02x: ", buff, ppos);
}

static const struct file_operations kgsl_mh_debug_fops = {
	.open = kgsl_dbgfs_open,
	.release = kgsl_dbgfs_release,
	.read = kgsl_mh_debug_read,
};

void adreno_debugfs_init(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if (!device->d_debugfs || IS_ERR(device->d_debugfs))
		return;

	debugfs_create_file("ib_dump",  0600, device->d_debugfs, device,
			    &kgsl_ib_dump_fops);
	debugfs_create_file("istore",   0400, device->d_debugfs, device,
			    &kgsl_istore_fops);
	debugfs_create_file("sx_debug", 0400, device->d_debugfs, device,
			    &kgsl_sx_debug_fops);
	debugfs_create_file("cp_debug", 0400, device->d_debugfs, device,
			    &kgsl_cp_debug_fops);
	debugfs_create_file("mh_debug", 0400, device->d_debugfs, device,
			    &kgsl_mh_debug_fops);
	debugfs_create_file("cff_dump", 0644, device->d_debugfs, device,
			    &kgsl_cff_dump_enable_fops);
	debugfs_create_u32("wait_timeout", 0644, device->d_debugfs,
		&adreno_dev->wait_timeout);

	/* Create post mortem control files */

	pm_d_debugfs = debugfs_create_dir("postmortem", device->d_debugfs);

	if (IS_ERR(pm_d_debugfs))
		return;

	debugfs_create_file("dump",  0600, pm_d_debugfs, device,
			    &pm_dump_fops);
	debugfs_create_file("regs_enabled", 0644, pm_d_debugfs, device,
			    &pm_regs_enabled_fops);
}
