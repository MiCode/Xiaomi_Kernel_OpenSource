// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/of_device.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>

#include <common/mdla_power_ctrl.h>

#include <utilities/mdla_debug.h>
#include <utilities/mdla_util.h>
#include <utilities/mdla_ipi.h>

#include <platform/mdla_plat_api.h>

#include "mdla_rv.h"

/* remove after lk/atf bootup flow ready */
#define LK_BOOT_RDY 0

#define DBGFS_USAGE_NAME    "help"
#define DBGFS_MEM_NAME      "dbg_mem"

static struct mdla_dev *mdla_plat_devices;

#define DEFINE_IPI_DBGFS_ATTRIBUTE(name, TYPE_0, TYPE_1, fmt)		\
static int name ## _set(void *data, u64 val)				\
{									\
	mdla_ipi_send(TYPE_0, TYPE_1, val);				\
	*(u64 *)data = val;						\
	return 0;							\
}									\
static int name ## _get(void *data, u64 *val)				\
{									\
	mdla_ipi_recv(TYPE_0, TYPE_1, val);				\
	*(u64 *)data = *val;						\
	return 0;							\
}									\
static int name ## _open(struct inode *i, struct file *f)		\
{									\
	__simple_attr_check_format(fmt, 0ull);				\
	return simple_attr_open(i, f, name ## _get, name ## _set, fmt);	\
}									\
static const struct file_operations name ## _fops = {			\
	.owner	 = THIS_MODULE,						\
	.open	 = name ## _open,					\
	.release = simple_attr_release,					\
	.read	 = debugfs_attr_read,					\
	.write	 = debugfs_attr_write,					\
	.llseek  = no_llseek,						\
}

DEFINE_IPI_DBGFS_ATTRIBUTE(ulog,           MDLA_IPI_ULOG,           0, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(timeout,        MDLA_IPI_TIMEOUT,        0, "%llu ms\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(pwrtime,        MDLA_IPI_PWR_TIME,       0, "%llu ms\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(cmd_check,      MDLA_IPI_CMD_CHECK,      0, "%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(pmu_trace,      MDLA_IPI_TRACE_ENABLE,   0, "%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C1,             MDLA_IPI_PMU_COUNT,      1, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C2,             MDLA_IPI_PMU_COUNT,      2, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C3,             MDLA_IPI_PMU_COUNT,      3, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C4,             MDLA_IPI_PMU_COUNT,      4, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C5,             MDLA_IPI_PMU_COUNT,      5, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C6,             MDLA_IPI_PMU_COUNT,      6, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C7,             MDLA_IPI_PMU_COUNT,      7, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C8,             MDLA_IPI_PMU_COUNT,      8, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C9,             MDLA_IPI_PMU_COUNT,      9, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C10,            MDLA_IPI_PMU_COUNT,     10, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C11,            MDLA_IPI_PMU_COUNT,     11, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C12,            MDLA_IPI_PMU_COUNT,     12, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C13,            MDLA_IPI_PMU_COUNT,     13, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C14,            MDLA_IPI_PMU_COUNT,     14, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C15,            MDLA_IPI_PMU_COUNT,     15, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(preempt_times,  MDLA_IPI_PREEMPT_CNT,    0, "%llu\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(force_pwr_on,   MDLA_IPI_FORCE_PWR_ON,   0, "%d\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(profiling,      MDLA_IPI_PROFILE_EN,     0, "%d\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(dump_cmdbuf_en, MDLA_IPI_DUMP_CMDBUF_EN, 0, "%d\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(info,           MDLA_IPI_INFO,           0, "%d\n");

struct mdla_dbgfs_ipi_file {
	int type0;
	int type1;
	u32 sup_mask;			/* 1'b << IP major version */
	umode_t mode;
	char *str;
	const struct file_operations *fops;
	u64 val;				/* update by ipi/debugfs */
};

static struct mdla_dbgfs_ipi_file ipi_dbgfs_file[] = {
	{MDLA_IPI_PWR_TIME,       0, 0xC, 0660,  "poweroff_time",        &pwrtime_fops, 0},
	{MDLA_IPI_TIMEOUT,        0, 0xC, 0660,        "timeout",        &timeout_fops, 0},
	{MDLA_IPI_ULOG,           0, 0xC, 0660,           "ulog",           &ulog_fops, 0},
	{MDLA_IPI_CMD_CHECK,      0, 0x4, 0660,      "cmd_check",      &cmd_check_fops, 0},
	{MDLA_IPI_TRACE_ENABLE,   0, 0x4, 0660,      "pmu_trace",      &pmu_trace_fops, 0},
	{MDLA_IPI_PMU_COUNT,      1, 0x4, 0660,             "c1",             &C1_fops, 0},
	{MDLA_IPI_PMU_COUNT,      2, 0x4, 0660,             "c2",             &C2_fops, 0},
	{MDLA_IPI_PMU_COUNT,      3, 0x4, 0660,             "c3",             &C3_fops, 0},
	{MDLA_IPI_PMU_COUNT,      4, 0x4, 0660,             "c4",             &C4_fops, 0},
	{MDLA_IPI_PMU_COUNT,      5, 0x4, 0660,             "c5",             &C5_fops, 0},
	{MDLA_IPI_PMU_COUNT,      6, 0x4, 0660,             "c6",             &C6_fops, 0},
	{MDLA_IPI_PMU_COUNT,      7, 0x4, 0660,             "c7",             &C7_fops, 0},
	{MDLA_IPI_PMU_COUNT,      8, 0x4, 0660,             "c8",             &C8_fops, 0},
	{MDLA_IPI_PMU_COUNT,      9, 0x4, 0660,             "c9",             &C9_fops, 0},
	{MDLA_IPI_PMU_COUNT,     10, 0x4, 0660,            "c10",            &C10_fops, 0},
	{MDLA_IPI_PMU_COUNT,     11, 0x4, 0660,            "c11",            &C11_fops, 0},
	{MDLA_IPI_PMU_COUNT,     12, 0x4, 0660,            "c12",            &C12_fops, 0},
	{MDLA_IPI_PMU_COUNT,     13, 0x4, 0660,            "c13",            &C13_fops, 0},
	{MDLA_IPI_PMU_COUNT,     14, 0x4, 0660,            "c14",            &C14_fops, 0},
	{MDLA_IPI_PMU_COUNT,     15, 0x4, 0660,            "c15",            &C15_fops, 0},
	{MDLA_IPI_PREEMPT_CNT,    0, 0xC, 0660,  "preempt_times",  &preempt_times_fops, 0},
	{MDLA_IPI_FORCE_PWR_ON,   0, 0xC, 0660,   "force_pwr_on",   &force_pwr_on_fops, 0},
	{MDLA_IPI_PROFILE_EN,     0, 0x8, 0660,      "profiling",      &profiling_fops, 0},
	{MDLA_IPI_DUMP_CMDBUF_EN, 0, 0xC, 0660, "dump_cmdbuf_en", &dump_cmdbuf_en_fops, 0},
	{MDLA_IPI_INFO,           0, 0xC, 0660,           "info",           &info_fops, 0},
	{NF_MDLA_IPI_TYPE_0,      0,   0,    0,             NULL,                 NULL, 0}
};

static u32 cfg0;
static u32 cfg1;

struct mdla_rv_mem {
	void *buf;
	dma_addr_t da;
	size_t size;
};

#define DEFAULT_DBG_SZ 0x1000
static struct mdla_rv_mem dbg_mem;
static struct mdla_rv_mem backup_mem;

static char *mdla_plat_get_ipi_str(int idx)
{
	u32 i;

	for (i = 0; ipi_dbgfs_file[i].str != NULL; i++) {
		if (ipi_dbgfs_file[i].type0 == idx)
			return ipi_dbgfs_file[i].str;
	}
	return "unknown";
}

static void mdla_plat_v2_dbgfs_usage(struct seq_file *s, void *data)
{
	seq_puts(s, "\n------------- Set uP debug log mask -------------\n");
	seq_printf(s, "echo [mask(hex)] > /d/mdla/%s\n", mdla_plat_get_ipi_str(MDLA_IPI_ULOG));
	seq_printf(s, "\tMDLA_DBG_DRV         = 0x%x\n", 1U << V2_DBG_DRV);
	seq_printf(s, "\tMDLA_DBG_CMD         = 0x%x\n", 1U << V2_DBG_CMD);
	seq_printf(s, "\tMDLA_DBG_PMU         = 0x%x\n", 1U << V2_DBG_PMU);
	seq_printf(s, "\tMDLA_DBG_PERF        = 0x%x\n", 1U << V2_DBG_PERF);
	seq_printf(s, "\tMDLA_DBG_TIMEOUT     = 0x%x\n", 1U << V2_DBG_TIMEOUT);
	seq_printf(s, "\tMDLA_DBG_PWR         = 0x%x\n", 1U << V2_DBG_PWR);
	seq_printf(s, "\tMDLA_DBG_MEM         = 0x%x\n", 1U << V2_DBG_MEM);
	seq_printf(s, "\tMDLA_DBG_IPI         = 0x%x\n", 1U << V2_DBG_IPI);

	seq_puts(s, "\n--------------- power control ---------------\n");
	seq_printf(s, "echo [0|1] > /d/mdla/%s\n", mdla_plat_get_ipi_str(MDLA_IPI_FORCE_PWR_ON));
	seq_puts(s, "\t0: force power down\n");
	seq_puts(s, "\t1: force power up\n");

	seq_puts(s, "\n------------- show information ------------------\n");
	seq_printf(s, "echo [item] > /d/mdla/%s\n", mdla_plat_get_ipi_str(MDLA_IPI_INFO));
	seq_printf(s, "\t%d: show register value\n", MDLA_IPI_INFO_REG);
	seq_printf(s, "\t%d: show the last cmdbuf (if dump_cmdbuf_en != 0)\n",
				MDLA_IPI_INFO_CMDBUF);

	seq_puts(s, "\n----------- allocate debug memory ---------------\n");
	seq_printf(s, "echo [size(dec)] > /d/mdla/%s\n", DBGFS_MEM_NAME);
}


static void mdla_plat_v3_dbgfs_usage(struct seq_file *s, void *data)
{
	seq_puts(s, "\n----------- Set uP debug log mask -----------\n");
	seq_printf(s, "echo [mask(hex)] > /d/mdla/%s\n", mdla_plat_get_ipi_str(MDLA_IPI_ULOG));
	seq_printf(s, "\tMDLA_DBG_DRV     = 0x%x\n", 1U << V3_DBG_DRV);
	seq_printf(s, "\tMDLA_DBG_CMD     = 0x%x\n", 1U << V3_DBG_CMD);
	seq_printf(s, "\tMDLA_DBG_PMU     = 0x%x\n", 1U << V3_DBG_PMU);
	seq_printf(s, "\tMDLA_DBG_PERF    = 0x%x\n", 1U << V3_DBG_PERF);
	seq_printf(s, "\tMDLA_DBG_TIMEOUT = 0x%x\n", 1U << V3_DBG_TIMEOUT);
	seq_printf(s, "\tMDLA_DBG_PWR     = 0x%x\n", 1U << V3_DBG_PWR);
	seq_printf(s, "\tMDLA_DBG_MEM     = 0x%x\n", 1U << V3_DBG_MEM);
	seq_printf(s, "\tMDLA_DBG_IPI     = 0x%x\n", 1U << V3_DBG_IPI);
	seq_printf(s, "\tMDLA_DBG_QUEUE   = 0x%x\n", 1U << V3_DBG_QUEUE);
	seq_printf(s, "\tMDLA_DBG_LOCK    = 0x%x\n", 1U << V3_DBG_LOCK);
	seq_printf(s, "\tMDLA_DBG_TMR     = 0x%x\n", 1U << V3_DBG_TMR);
	seq_printf(s, "\tMDLA_DBG_FW      = 0x%x\n", 1U << V3_DBG_FW);

	seq_puts(s, "\n--------------- power control ---------------\n");
	seq_printf(s, "echo [0|1] > /d/mdla/%s\n", mdla_plat_get_ipi_str(MDLA_IPI_FORCE_PWR_ON));
	seq_puts(s, "\t0: force power down and reset command queue\n");
	seq_puts(s, "\t1: force power up and keep power on\n");

	seq_puts(s, "\n--------------- profile control ---------------\n");
	seq_printf(s, "echo [0|1] > /d/mdla/%s\n", mdla_plat_get_ipi_str(MDLA_IPI_PROFILE_EN));
	seq_puts(s, "\t0: stop profiling\n");
	seq_puts(s, "\t1: start to profile\n");

	seq_puts(s, "\n------------- show information -------------\n");
	seq_printf(s, "echo [item] > /d/mdla/%s\n", mdla_plat_get_ipi_str(MDLA_IPI_INFO));
	seq_puts(s, "and then cat /proc/apusys_logger/seq_log\n");
	seq_printf(s, "\t%d: show power status\n", MDLA_IPI_INFO_PWR);
	seq_printf(s, "\t%d: show register value\n", MDLA_IPI_INFO_REG);
	seq_printf(s, "\t%d: show the last cmdbuf (if dump_cmdbuf_en != 0)\n",
				MDLA_IPI_INFO_CMDBUF);
	seq_printf(s, "\t%d: show profiling result\n", MDLA_IPI_INFO_PROF);

	seq_puts(s, "\n----------- allocate debug memory -----------\n");
	seq_printf(s, "echo [size(dec)] > /d/mdla/%s\n", DBGFS_MEM_NAME);
}

static int mdla_plat_dbgfs_usage(struct seq_file *s, void *data)
{
	/* Common */
	seq_puts(s, "\n---------- Command timeout setting ----------\n");
	seq_printf(s, "echo [ms(dec)] > /d/mdla/%s\n", ipi_dbgfs_file[MDLA_IPI_TIMEOUT].str);

	seq_puts(s, "\n-------- Set delay time of power off --------\n");
	seq_printf(s, "echo [ms(dec)] > /d/mdla/%s\n", ipi_dbgfs_file[MDLA_IPI_PWR_TIME].str);

	/* IP */
	if (get_major_num(mdla_plat_get_version()) == 2)
		mdla_plat_v2_dbgfs_usage(s, data);
	else if (get_major_num(mdla_plat_get_version()) == 3)
		mdla_plat_v3_dbgfs_usage(s, data);

	return 0;
}

static int mdla_plat_alloc_mem(struct mdla_rv_mem *m, unsigned int size)
{
	struct device *dev;

	if (mdla_plat_devices && mdla_plat_devices[0].dev)
		dev = mdla_plat_devices[0].dev;
	else
		return -ENXIO;

	m->buf = dma_alloc_coherent(dev, size, &m->da, GFP_KERNEL);
	if (m->buf == NULL || m->da == 0) {
		dev_info(dev, "%s() dma_alloc_coherent fail\n", __func__);
		return -1;
	}

	m->size = size;
	memset(m->buf, 0, size);

	return 0;
}

static void mdla_plat_free_mem(struct mdla_rv_mem *m)
{
	struct device *dev;

	if (mdla_plat_devices && mdla_plat_devices[0].dev)
		dev = mdla_plat_devices[0].dev;
	else
		return;

	if (m->buf && m->da && m->size) {
		dma_free_coherent(dev, m->size, m->buf, m->da);
		m->buf  = NULL;
		m->size = 0;
		m->da   = 0;
	}
}

static int mdla_rv_dbg_mem_show(struct seq_file *s, void *data)
{
	u32 i = 0, *buf;

	if (!dbg_mem.buf || !dbg_mem.da || !dbg_mem.size) {
		seq_puts(s, "No debug data!\n");
		return 0;
	}

	buf = (u32 *)dbg_mem.buf;

	for (i = 0; i < dbg_mem.size / 4; i += 4) {
		seq_printf(s, "0x%08x: %08x %08x %08x %08x\n",
				4 * i,
				buf[i],
				buf[i + 1],
				buf[i + 2],
				buf[i + 3]);
	}

	return 0;
}

static int mdla_rv_dbg_mem_open(struct inode *inode, struct file *file)
{
	return single_open(file, mdla_rv_dbg_mem_show, inode->i_private);
}

static int mdla_plat_send_addr_info(void *arg)
{
	msleep(1000);

	if (cfg0 && cfg1) {
		mdla_verbose("%s(): send ipi for fw addr(0x%08x, 0x%08x)\n", __func__,
				cfg0, cfg1);
		mdla_ipi_send(MDLA_IPI_ADDR, MDLA_IPI_ADDR_BOOT, (u64)cfg0);
		mdla_ipi_send(MDLA_IPI_ADDR, MDLA_IPI_ADDR_MAIN, (u64)cfg1);
	}

	if (backup_mem.da) {
		mdla_ipi_send(MDLA_IPI_ADDR, MDLA_IPI_ADDR_BACKUP_DATA, (u64)backup_mem.da);
		mdla_ipi_send(MDLA_IPI_ADDR, MDLA_IPI_ADDR_BACKUP_DATA_SZ, (u64)backup_mem.size);
	}

	if (dbg_mem.da) {
		mdla_ipi_send(MDLA_IPI_ADDR, MDLA_IPI_ADDR_DBG_DATA, (u64)dbg_mem.da);
		mdla_ipi_send(MDLA_IPI_ADDR, MDLA_IPI_ADDR_DBG_DATA_SZ, (u64)dbg_mem.size);
	}

	return 0;
}

static ssize_t mdla_rv_dbg_mem_write(struct file *flip,
		const char __user *buffer,
		size_t count, loff_t *f_pos)
{
	char *buf;
	u32 size;
	int ret;

	buf = kzalloc(count + 1, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = copy_from_user(buf, buffer, count);
	if (ret)
		goto out;

	buf[count] = '\0';

	if (kstrtouint(buf, 10, &size) != 0) {
		count = -EINVAL;
		goto out;
	}

	if (size > 0x200000 || size < 0x10)
		goto out;

	mdla_plat_free_mem(&dbg_mem);

	if (mdla_plat_alloc_mem(&dbg_mem, size) == 0) {
		mdla_ipi_send(MDLA_IPI_ADDR, MDLA_IPI_ADDR_DBG_DATA, (u64)dbg_mem.da);
		mdla_ipi_send(MDLA_IPI_ADDR, MDLA_IPI_ADDR_DBG_DATA_SZ, (u64)dbg_mem.size);
	}

out:
	kfree(buf);
	return count;
}

static const struct file_operations mdla_rv_dbg_mem_fops = {
	.open = mdla_rv_dbg_mem_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = mdla_rv_dbg_mem_write,
};

static void mdla_plat_dbgfs_init(struct device *dev, struct dentry *parent)
{
	struct mdla_dbgfs_ipi_file *file;
	u32 mask;
	u32 i;

	if (!dev || !parent)
		return;

	debugfs_create_devm_seqfile(dev, DBGFS_USAGE_NAME, parent,
				mdla_plat_dbgfs_usage);

	mask = BIT(get_major_num(mdla_plat_get_version()));

	for (i = 0; ipi_dbgfs_file[i].fops != NULL; i++) {
		file = &ipi_dbgfs_file[i];
		if ((mask & file->sup_mask) != 0)
			debugfs_create_file(file->str, file->mode, parent, &file->val, file->fops);
	}

	debugfs_create_file(DBGFS_MEM_NAME, 0644, parent, NULL,
				&mdla_rv_dbg_mem_fops);
}

static void mdla_plat_memory_show(struct seq_file *s)
{
	seq_puts(s, "------- dump debug info  -------\n");
	mdla_rv_dbg_mem_show(s, NULL);
}


void mdla_plat_up_init(void)
{
	struct task_struct *init_task;

	init_task = kthread_run(mdla_plat_send_addr_info, NULL, "mdla uP init");

	if (IS_ERR(init_task))
		mdla_err("create uP init thread failed\n");
}

/* platform public functions */
int mdla_rv_init(struct platform_device *pdev)
{
	int i;
	u32 nr_core_ids = mdla_util_get_core_num();

	dev_info(&pdev->dev, "%s()\n", __func__);

	mdla_plat_devices = devm_kzalloc(&pdev->dev,
				nr_core_ids * sizeof(struct mdla_dev),
				GFP_KERNEL);

	if (!mdla_plat_devices)
		return -1;

	mdla_set_device(mdla_plat_devices, nr_core_ids);

	for (i = 0; i < nr_core_ids; i++) {
		mdla_plat_devices[i].mdla_id = i;
		mdla_plat_devices[i].dev = &pdev->dev;
	}

	mdla_plat_load_data(&pdev->dev, &cfg0, &cfg1);

	if (mdla_ipi_init() != 0) {
		dev_info(&pdev->dev, "register apu_ctrl channel : Fail\n");
		if (mdla_plat_pwr_drv_ready())
			mdla_pwr_device_unregister(pdev);
		return -1;
	}

	mdla_dbg_plat_cb()->dbgfs_plat_init = mdla_plat_dbgfs_init;
	mdla_dbg_plat_cb()->memory_show     = mdla_plat_memory_show;

	/* backup size * core num * preempt lv */
	mdla_plat_alloc_mem(&backup_mem, 1024 * nr_core_ids * 4);
	mdla_plat_alloc_mem(&dbg_mem, DEFAULT_DBG_SZ);

	return 0;
}

void mdla_rv_deinit(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s()\n", __func__);

	mdla_plat_free_mem(&backup_mem);
	mdla_plat_free_mem(&dbg_mem);
	mdla_ipi_deinit();

	if (mdla_plat_pwr_drv_ready()
			&& mdla_pwr_device_unregister(pdev))
		dev_info(&pdev->dev, "unregister mdla power fail\n");

	mdla_plat_unload_data(&pdev->dev);
}

