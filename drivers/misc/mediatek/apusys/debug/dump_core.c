// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/sched/clock.h>
#include <apusys_secure.h>
#include <apusys_plat.h>


static void set_dbg_sel(int val, int offset, int shift, int mask)
{
	void *target = apu_top + offset;
	u32 tmp;

	if (apu_top == NULL) /* Skip if apu_top is not valid */
		return;

	tmp = ioread32(target);

	tmp = (tmp & ~(mask << shift)) | (val << shift);
	iowrite32(tmp, target);
	tmp = ioread32(target);
}

u32 dbg_read(struct dbg_mux_sel_value sel)
{
	int i;
	int offset;
	int shift;
	int length;
	int mask;
	void *addr = apu_top + sel.status_reg_offset;
	struct dbg_mux_sel_info info;

	if (apu_top == NULL) /* Skip if apu_top is not valid */
		return 0;

	for (i = 0; i < DBG_MUX_SEL_COUNT; ++i) {
		if (sel.dbg_sel[i] >= 0) {
			info = info_table[i];
			offset = info.offset;
			shift = info.end_bit;
			length = info.start_bit - info.end_bit + 1;
			mask = (1 << length) - 1;

			set_dbg_sel(sel.dbg_sel[i], offset, shift, mask);
		}
	}

	return ioread32(addr);
}

static u32 gals_reg[TOTAL_DBG_MUX_COUNT];

void dump_gals_reg(bool dump_vpu)
{
	int i;

	for (i = 0; i < TOTAL_DBG_MUX_COUNT; ++i)
		gals_reg[i] = dbg_read(value_table[i]);
}


void apusys_reg_dump(void)
{
	int i;
	u32 offset, size;
	char *tmp = reg_all_mem;

	if (apu_top == NULL) /* Skip if apu_top is not valid */
		return;

	dump_gals_reg(false);

	for (i = 0; i < SEGMENT_COUNT; ++i) {
		offset = range_table[i].base - APUSYS_BASE;
		size = range_table[i].size;

		memcpy_fromio(tmp, apu_top + offset, size);
		tmp += size;
	}
}

static ssize_t gals_dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i;
	char *p = buf;

	if (apusys_dump_force)
		apusys_reg_dump();

	for (i = 0; i < TOTAL_DBG_MUX_COUNT; ++i)
		p += sprintf(p, "%s:0x%08x\n",
			value_table[i].name, gals_reg[i]);
	return p - buf;
}


static u32 find_next_offset(loff_t offset)
{
	u32 start, end;
	loff_t reg_mem_offset = 0;
	int i;

	offset += APUSYS_BASE;

	for (i = 0; i < SEGMENT_COUNT; i++) {
		start = range_table[i].base;
		end = start + range_table[i].size;

		if (offset < start)
			return start - offset;

		reg_mem_offset += range_table[i].size;
	}

	/* fail */
	return 0;
}


static u32 find_offset(loff_t offset, u32 *reg_mem_offset)
{
	u32 start, end;
	int i;
	*reg_mem_offset = 0;

	offset += APUSYS_BASE;

	for (i = 0; i < SEGMENT_COUNT; i++) {
		start = range_table[i].base;
		end = start + range_table[i].size;

		if (offset >= start && offset < end) {
			*reg_mem_offset += offset - start;
			return end - offset;
		}
		*reg_mem_offset += range_table[i].size;
	}

	/* fail */
	return 0;
}

static ssize_t
reg_read(struct file *filp, struct kobject *kobj,
	     struct bin_attribute *attr, char *buf,
	     loff_t offset, size_t count)
{
	u32 resid;
	u32 reg_mem_offset;
	u32 copy_size;

	resid = find_offset(offset, &reg_mem_offset);
	if (resid == 0) {
		resid = find_next_offset(offset);
		copy_size = count <= resid ? count : resid;
		memset(buf, 0, copy_size);
	} else {
		copy_size = count <= resid ? count : resid;
		memcpy(buf, reg_all_mem + reg_mem_offset, copy_size);
	}

	return copy_size;
}

static struct bin_attribute reg_dump_attr = {
	.attr = {.name = "reg_dump", .mode = (0400)},
	.size = APUSYS_REG_SIZE,
	.read = reg_read,
	.write = NULL,
	.mmap = NULL,
};

static DEVICE_ATTR_RO(gals_dump);
static DEVICE_BOOL_ATTR(force_dump, 0600, apusys_dump_force);
static struct attribute *reg_dump_attrs[] = {
	&dev_attr_force_dump.attr.attr,
	&dev_attr_gals_dump.attr,
	NULL,
};

static struct bin_attribute *reg_dump_bin_attrs[] = {
		&reg_dump_attr,
		NULL,
};

static struct attribute_group mdw_reg_dump_attr_group = {
	.name   = "debug",
	.attrs  = reg_dump_attrs,
	.bin_attrs = reg_dump_bin_attrs,
};

int apusys_dump_init(struct device *dev)
{
	int ret;
	int reg_mem_size = 0;
	int i;

	ret = sysfs_create_group(&dev->kobj, &mdw_reg_dump_attr_group);
	apu_top = ioremap_nocache(APUSYS_BASE, APUSYS_REG_SIZE);
	apu_to_infra_top = ioremap_nocache(INFRA_BASE, INFRA_SIZE);

	if (apu_top == NULL || apu_to_infra_top == NULL)
		return -EIO;


	/*Pre-allocate dump buffer size*/
	for (i = 0; i < SEGMENT_COUNT; i++)
		reg_mem_size += range_table[i].size;

	reg_all_mem = vzalloc(reg_mem_size);
	apusys_dump_force = false;

	return 0;
}


void apusys_dump_exit(struct device *dev)
{
	vfree(reg_all_mem);
	sysfs_remove_group(&dev->kobj, &mdw_reg_dump_attr_group);
	iounmap(apu_top);
	iounmap(apu_to_infra_top);
}
