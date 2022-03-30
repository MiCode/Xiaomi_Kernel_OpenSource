// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

/*
 *=============================================================
 * Include files
 *=============================================================
 */

/* system includes */
#include <linux/printk.h>
#include <linux/seq_file.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/string.h>
#include <linux/device.h>


#if IS_ENABLED(CONFIG_OF)
#include <linux/cpu.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#endif

#include "debug_plat_api.h"
#include "apusys_core.h"
#include "debug_drv.h"
#include "apusys_debug_api.h"
#include <debug_platform.h>

#define FORCE_REG_DUMP_ENABLE (0)


struct debug_plat_drv debug_drv;

#define STRMAX 128
struct dump_data {
	char module_name[STRMAX];
	char *reg_all_mem;
	u32 *gals_reg;
};


static struct dump_data data;

int debug_log_level;
bool apusys_dump_force;
bool apusys_dump_skip_gals;
static void *apu_top;
struct apusys_core_info *dbg_core_info;

void apusys_reg_dump_skip_gals(int onoff)
{
	LOG_DEBUG("+\n");

	if (onoff)
		apusys_dump_skip_gals = true;
	else
		apusys_dump_skip_gals = false;

	LOG_DEBUG("-\n");
}
//
//void apusys_reg_dump(char *module_name, bool dump_vpu)
//{
//	LOG_DEBUG("+\n");
//
//	LOG_DEBUG("caller name:%s dump_vpu:%d", module_name, dump_vpu);
//
//
//	if (data.reg_all_mem != NULL) {
//		LOG_INFO("dump is in process, skip this dump call\n");
//		goto out;
//	}
//
//	data.reg_all_mem = vzalloc(debug_drv.apusys_reg_size);
//	if (data.reg_all_mem == NULL)
//		goto out;
//
//	data.gals_reg = vzalloc(debug_drv.total_dbg_mux_count);
//	if (data.gals_reg == NULL)
//		goto out;
//
//	if (module_name != NULL)
//		strncpy(data.module_name, module_name, STRMAX);
//
//	debug_drv.reg_dump(apu_top, dump_vpu, data.reg_all_mem,
//		apusys_dump_skip_gals, data.gals_reg, debug_drv.platform_idx);
//
//out:
//
//	LOG_DEBUG("-\n");
//}

static void set_dbg_sel(int val, int offset, int shift, int mask)
{
	void *target = apu_top + offset;
	u32 tmp = ioread32(target);

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
	struct dbg_hw_info *hw_info;
	struct dbg_mux_sel_info *info_table;
	struct dbg_mux_sel_info info;

	hw_info = &hw_info_set[debug_drv.platform_idx];
	info_table = hw_info->mux_sel_tbl;

	for (i = 0; i < hw_info->mux_sel_count; ++i) {
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

void dump_gals_reg(bool dump_vpu)
{
	int i;
	struct dbg_hw_info *hw_info;
	struct dbg_mux_sel_value *value_table;

	hw_info = &hw_info_set[debug_drv.platform_idx];
	value_table = hw_info->value_tbl;

	for (i = 0; i < debug_drv.total_dbg_mux_count; ++i)
		data.gals_reg[i] = dbg_read(value_table[i]);
}

void apusys_reg_dump(char *module_name, bool dump_vpu)
{
	int i;
	u32 offset, size;
	struct reg_dump_info *range_table;
	struct dbg_hw_info *hw_info;
	char *tmp;

	hw_info = &hw_info_set[debug_drv.platform_idx];
	range_table = hw_info->range_tbl;
	tmp =  data.reg_all_mem;

	LOG_DEBUG("caller name:%s\n", module_name);

	dump_gals_reg(false);

	for (i = 0; i < hw_info->seg_count; ++i) {
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
	struct dbg_mux_sel_value *value_table;
	struct dbg_hw_info *hw_info;
	char *p = buf;

	hw_info = &hw_info_set[debug_drv.platform_idx];
	value_table = hw_info->value_tbl;


	if (apusys_dump_force)
		apusys_reg_dump("force_dump", false);

	for (i = 0; i < debug_drv.total_dbg_mux_count; ++i)
		p += sprintf(p, "%s:0x%08x\n",
			value_table[i].name, data.gals_reg[i]);
	return p - buf;
}

static u32 find_next_offset(loff_t offset)
{
	u32 start, end;
	loff_t reg_mem_offset = 0;
	int i;
	struct reg_dump_info *range_table;
	struct dbg_hw_info *hw_info;


	hw_info = &hw_info_set[debug_drv.platform_idx];
	range_table = hw_info->range_tbl;

	offset += APUSYS_BASE;

	for (i = 0; i < hw_info->seg_count; i++) {
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
	struct reg_dump_info *range_table;
	struct dbg_hw_info *hw_info;

	*reg_mem_offset = 0;

	hw_info = &hw_info_set[debug_drv.platform_idx];
	range_table = hw_info->range_tbl;

	offset += APUSYS_BASE;

	for (i = 0; i < hw_info->seg_count; i++) {
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
		memcpy(buf, data.reg_all_mem + reg_mem_offset, copy_size);
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

static int debug_probe(struct platform_device *pdev)
{
	int reg_mem_size = 0;
	int ret = 0;
	int i;
	struct reg_dump_info *range_table;
	struct dbg_hw_info *hw_info;

	debug_log_level = 0;
	LOG_DEBUG("+\n");

	debug_drv = *(struct debug_plat_drv *)of_device_get_match_data(&pdev->dev);
	hw_info = &hw_info_set[debug_drv.platform_idx];
	range_table = hw_info->range_tbl;

	ret = sysfs_create_group(&pdev->dev.kobj, &mdw_reg_dump_attr_group);

	if (ret) {
		LOG_ERR("failed to create debugfs dir\n");
		return -1;
	}

	apu_top = ioremap(debug_drv.apusys_base,
					debug_drv.apusys_reg_size);
	if (apu_top == NULL) {
		LOG_ERR("could not allocate iomem base(0x%x) size(0x%x)\n",
			debug_drv.apusys_base, debug_drv.apusys_base);
		return -EIO;
	}

	/*Pre-allocate dump buffer size*/
	for (i = 0; i < hw_info->seg_count; i++)
		reg_mem_size += range_table[i].size;

	data.reg_all_mem = vzalloc(reg_mem_size);
	data.gals_reg = vzalloc(debug_drv.total_dbg_mux_count);
	memset(data.module_name, 0, STRMAX);

	apusys_dump_force = false;
	apusys_dump_skip_gals = false;

	LOG_DEBUG("-\n");

	return ret;
}

static int debug_remove(struct platform_device *pdev)
{
	LOG_DEBUG("+\n");

	vfree(data.reg_all_mem);
	vfree(data.gals_reg);
	iounmap(apu_top);
	sysfs_remove_group(&pdev->dev.kobj, &mdw_reg_dump_attr_group);

	LOG_DEBUG("-\n");

	return 0;
}

static struct platform_driver debug_driver = {
	.probe		= debug_probe,
	.remove		= debug_remove,
	.driver		= {
		.name	= APUSYS_DEBUG_DEV_NAME,
		.owner	= THIS_MODULE,
	},
};

int debug_init(struct apusys_core_info *info)
{
	LOG_DEBUG("debug driver init start\n");

	dbg_core_info = info;

	memset(&debug_drv, 0, sizeof(struct debug_plat_drv));

	debug_driver.driver.of_match_table = debug_plat_get_device();

	if (platform_driver_register(&debug_driver)) {
		LOG_ERR("failed to register %s driver", APUSYS_DEBUG_DEV_NAME);
		return -ENODEV;
	}

	return 0;
}

void debug_exit(void)
{
	LOG_DEBUG("+\n");
	platform_driver_unregister(&debug_driver);
	LOG_DEBUG("-\n");
}
