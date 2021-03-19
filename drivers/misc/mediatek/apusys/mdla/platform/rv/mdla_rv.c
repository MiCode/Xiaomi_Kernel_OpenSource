// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/of_device.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>

#include <common/mdla_power_ctrl.h>

#include <utilities/mdla_debug.h>
#include <utilities/mdla_util.h>
#include <utilities/mdla_ipi.h>

#include <platform/mdla_plat_api.h>

#define DBGFS_PMU_NAME      "pmu_reg"
#define DBGFS_USAGE_NAME    "help"

static struct mdla_dev *mdla_plat_devices;

DEFINE_IPI_DBGFS_ATTRIBUTE(ulog,          MDLA_IPI_ULOG,         0, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(timeout,       MDLA_IPI_TIMEOUT,      0, "%llu ms\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(pwrtime,       MDLA_IPI_PWR_TIME,     0, "%llu ms\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(cmd_check,     MDLA_IPI_CMD_CHECK,    0, "%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(pmu_trace,     MDLA_IPI_TRACE_ENABLE, 0, "%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C1,            MDLA_IPI_PMU_COUNT,    1, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C2,            MDLA_IPI_PMU_COUNT,    2, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C3,            MDLA_IPI_PMU_COUNT,    3, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C4,            MDLA_IPI_PMU_COUNT,    4, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C5,            MDLA_IPI_PMU_COUNT,    5, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C6,            MDLA_IPI_PMU_COUNT,    6, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C7,            MDLA_IPI_PMU_COUNT,    7, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C8,            MDLA_IPI_PMU_COUNT,    8, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C9,            MDLA_IPI_PMU_COUNT,    9, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C10,           MDLA_IPI_PMU_COUNT,   10, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C11,           MDLA_IPI_PMU_COUNT,   11, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C12,           MDLA_IPI_PMU_COUNT,   12, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C13,           MDLA_IPI_PMU_COUNT,   13, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C14,           MDLA_IPI_PMU_COUNT,   14, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(C15,           MDLA_IPI_PMU_COUNT,   15, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(preempt_times, MDLA_IPI_PREEMPT_CNT,  0, "%llu\n");

struct mdla_dbgfs_ipi_file {
	int type0;
	int type1;
	unsigned int sup_mask;			/* 1'b << IP major version */
	umode_t mode;
	char *str;
	const struct file_operations *fops;
	u64 val;				/* update by ipi/debugfs */
};

static struct mdla_dbgfs_ipi_file ipi_dbgfs_file[] = {
	{MDLA_IPI_PWR_TIME,      0, BIT(2) | BIT(3), 0660, "poweroff_time",       &pwrtime_fops, 0},
	{MDLA_IPI_TIMEOUT,       0, BIT(2) | BIT(3), 0660,       "timeout",       &timeout_fops, 0},
	{MDLA_IPI_ULOG,          0, BIT(2) | BIT(3), 0660,          "ulog",          &ulog_fops, 0},
	{MDLA_IPI_CMD_CHECK,     0, BIT(2) | BIT(3), 0660,     "cmd_check",     &cmd_check_fops, 0},
	{MDLA_IPI_TRACE_ENABLE,  0, BIT(2) | BIT(3), 0660,     "pmu_trace",     &pmu_trace_fops, 0},
	{MDLA_IPI_PMU_COUNT,     1, BIT(2) | BIT(3), 0660,            "c1",            &C1_fops, 0},
	{MDLA_IPI_PMU_COUNT,     2, BIT(2) | BIT(3), 0660,            "c2",            &C2_fops, 0},
	{MDLA_IPI_PMU_COUNT,     3, BIT(2) | BIT(3), 0660,            "c3",            &C3_fops, 0},
	{MDLA_IPI_PMU_COUNT,     4, BIT(2) | BIT(3), 0660,            "c4",            &C4_fops, 0},
	{MDLA_IPI_PMU_COUNT,     5, BIT(2) | BIT(3), 0660,            "c5",            &C5_fops, 0},
	{MDLA_IPI_PMU_COUNT,     6, BIT(2) | BIT(3), 0660,            "c6",            &C6_fops, 0},
	{MDLA_IPI_PMU_COUNT,     7, BIT(2) | BIT(3), 0660,            "c7",            &C7_fops, 0},
	{MDLA_IPI_PMU_COUNT,     8, BIT(2) | BIT(3), 0660,            "c8",            &C8_fops, 0},
	{MDLA_IPI_PMU_COUNT,     9, BIT(2) | BIT(3), 0660,            "c9",            &C9_fops, 0},
	{MDLA_IPI_PMU_COUNT,    10, BIT(2) | BIT(3), 0660,           "c10",           &C10_fops, 0},
	{MDLA_IPI_PMU_COUNT,    11, BIT(2) | BIT(3), 0660,           "c11",           &C11_fops, 0},
	{MDLA_IPI_PMU_COUNT,    12, BIT(2) | BIT(3), 0660,           "c12",           &C12_fops, 0},
	{MDLA_IPI_PMU_COUNT,    13, BIT(2) | BIT(3), 0660,           "c13",           &C13_fops, 0},
	{MDLA_IPI_PMU_COUNT,    14, BIT(2) | BIT(3), 0660,           "c14",           &C14_fops, 0},
	{MDLA_IPI_PMU_COUNT,    15, BIT(2) | BIT(3), 0660,           "c15",           &C15_fops, 0},
	{MDLA_IPI_PREEMPT_CNT,   0, BIT(2) | BIT(3), 0660, "preempt_times", &preempt_times_fops, 0},
	{NF_MDLA_IPI_TYPE_0,     0,               0,    0,            NULL,                NULL, 0}
};

static int mdla_plat_init_fw(void)
{
	/* TODO: Nofity uP of firmware address */
	return 0;
}

static int mdla_plat_register_show(struct seq_file *s, void *data)
{
	return 0;
}

static int mdla_plat_dbgfs_usage(struct seq_file *s, void *data)
{
	return 0;
}

static void mdla_plat_dbgfs_init(struct device *dev, struct dentry *parent)
{
	struct mdla_dbgfs_ipi_file *file;
	unsigned int mask;
	int i;

	if (!dev || !parent)
		return;

	debugfs_create_devm_seqfile(dev, DBGFS_PMU_NAME, parent,
				mdla_plat_register_show);
	debugfs_create_devm_seqfile(dev, DBGFS_USAGE_NAME, parent,
				mdla_plat_dbgfs_usage);

	mask = BIT(get_major_num(mdla_plat_get_version()));

	for (i = 0; ipi_dbgfs_file[i].fops != NULL; i++) {
		file = &ipi_dbgfs_file[i];
		if ((mask & file->sup_mask) != 0)
			debugfs_create_file(file->str, file->mode, parent, &file->val, file->fops);
	}
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

	if (mdla_plat_init_fw())
		return -1;

	if (mdla_plat_pwr_drv_ready()) {
		if (mdla_pwr_device_register(pdev, NULL, NULL))
			return -1;
	}

	if (mdla_ipi_init() == 0)
		mdla_dbg_plat_cb()->dbgfs_plat_init = mdla_plat_dbgfs_init;
	else
		dev_info(&pdev->dev, "register apu_ctrl channel : Fail\n");

	return 0;
}

void mdla_rv_deinit(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s()\n", __func__);

	mdla_ipi_deinit();

	if (mdla_plat_pwr_drv_ready()
			&& mdla_pwr_device_unregister(pdev))
		dev_info(&pdev->dev, "unregister mdla power fail\n");
}

