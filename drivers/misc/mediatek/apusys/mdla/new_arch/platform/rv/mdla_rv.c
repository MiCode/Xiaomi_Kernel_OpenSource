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

struct mdla_dbgfs_ipi_file {
	char *str;
	u64 val;
	umode_t mode;
	const struct file_operations *fops;
};

DEFINE_IPI_DBGFS_ATTRIBUTE(klog,    MDLA_IPI_KLOG,     0, "0x%llx\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(timeout, MDLA_IPI_TIMEOUT,  0, "%llu ms\n");
DEFINE_IPI_DBGFS_ATTRIBUTE(pwrtime, MDLA_IPI_PWR_TIME, 0, "%llu ms\n");

static struct mdla_dbgfs_ipi_file ipi_dbgfs_file[NF_MDLA_IPI_TYPE_0 + 1] = {
	[MDLA_IPI_PWR_TIME]     = { .str = "poweroff_time",
					.mode = 0660, .fops = &pwrtime_fops},
	[MDLA_IPI_TIMEOUT]      = { .str = "timeout",
					.mode = 0660, .fops = &timeout_fops},
	[MDLA_IPI_KLOG]         = { .str = "klog",
					.mode = 0660, .fops = &klog_fops},
	[NF_MDLA_IPI_TYPE_0]    = { .str = "unknown"},
};

static int mdla_plat_init_fw(void)
{
	/* TODO: Nofity uP of firmware address */
	return 0;
}

#if 0
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
	int i;

	if (!dev || !parent)
		return;

	debugfs_create_devm_seqfile(dev, DBGFS_PMU_NAME, parent,
				mdla_plat_register_show);
	debugfs_create_devm_seqfile(dev, DBGFS_USAGE_NAME, parent,
				mdla_plat_dbgfs_usage);

	for (i = 0; i < NF_MDLA_IPI_TYPE_0; i++) {
		file = &ipi_dbgfs_file[i];
		debugfs_create_file(file->str, file->mode, parent, &file->val, file->fops);
	}
}
#endif

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

	mdla_ipi_init();

#if 0
	mdla_dbg_plat_cb()->dbgfs_plat_init = mdla_plat_dbgfs_init;

	ipi_dbgfs_file[MDLA_IPI_KLOG].val     = mdla_dbg_read_u32(FS_KLOG);
	ipi_dbgfs_file[MDLA_IPI_PWR_TIME].val = mdla_dbg_read_u32(FS_POWEROFF_TIME);
	ipi_dbgfs_file[MDLA_IPI_TIMEOUT].val  = mdla_dbg_read_u32(FS_TIMEOUT);
#endif

	/* Initialize uP debug parameters */
	for (i = 0; i < NF_MDLA_IPI_TYPE_0; i++)
		mdla_ipi_send(i, 0, ipi_dbgfs_file[i].val);

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

