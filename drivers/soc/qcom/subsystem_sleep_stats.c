// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/ioctl.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/soc/qcom/smem.h>
#include <linux/uaccess.h>
#include <soc/qcom/soc_sleep_stats.h>
#include <soc/qcom/subsystem_sleep_stats.h>

#define STATS_BASEMINOR				0
#define STATS_MAX_MINOR				1
#define STATS_DEVICE_NAME			"stats"
#define SUBSYSTEM_STATS_MAGIC_NUM		(0x9d)
#define SUBSYSTEM_STATS_OTHERS_NUM		(-2)

#define DDR_STATS_MAGIC_KEY	0xA1157A75
#define DDR_STATS_MAX_NUM_MODES	0x14
#define DDR_STATS_MAGIC_KEY_ADDR	0x0
#define DDR_STATS_NUM_MODES_ADDR	0x4
#define DDR_STATS_NAME_ADDR		0x0
#define DDR_STATS_COUNT_ADDR		0x4
#define DDR_STATS_DURATION_ADDR		0x8

#define APSS_IOCTL		_IOR(SUBSYSTEM_STATS_MAGIC_NUM, 0, \
				     struct sleep_stats *)
#define MODEM_IOCTL		_IOR(SUBSYSTEM_STATS_MAGIC_NUM, 1, \
				     struct sleep_stats *)
#define WPSS_IOCTL		_IOR(SUBSYSTEM_STATS_MAGIC_NUM, 2, \
				     struct sleep_stats *)
#define ADSP_IOCTL		_IOR(SUBSYSTEM_STATS_MAGIC_NUM, 3, \
				     struct sleep_stats *)
#define ADSP_ISLAND_IOCTL	_IOR(SUBSYSTEM_STATS_MAGIC_NUM, 4, \
				     struct sleep_stats *)
#define CDSP_IOCTL		_IOR(SUBSYSTEM_STATS_MAGIC_NUM, 5, \
				     struct sleep_stats *)
#define SLPI_IOCTL		_IOR(SUBSYSTEM_STATS_MAGIC_NUM, 6, \
				     struct sleep_stats *)
#define GPU_IOCTL		_IOR(SUBSYSTEM_STATS_MAGIC_NUM, 7, \
				     struct sleep_stats *)
#define DISPLAY_IOCTL		_IOR(SUBSYSTEM_STATS_MAGIC_NUM, 8, \
				     struct sleep_stats *)
#define SLPI_ISLAND_IOCTL	_IOR(SUBSYSTEM_STATS_MAGIC_NUM, 9, \
				     struct sleep_stats *)

#define AOSD_IOCTL		_IOR(SUBSYSTEM_STATS_MAGIC_NUM, 10, \
				     struct sleep_stats *)

#define CXSD_IOCTL		_IOR(SUBSYSTEM_STATS_MAGIC_NUM, 11, \
				     struct sleep_stats *)

#define DDR_IOCTL		_IOR(SUBSYSTEM_STATS_MAGIC_NUM, 12, \
				     struct sleep_stats *)

#define DDR_STATS_IOCTL		_IOR(SUBSYSTEM_STATS_MAGIC_NUM, 13, \
				     struct sleep_stats *)

struct sleep_stats {
	u32 version;
	u32 count;
	u64 last_entered_at;
	u64 last_exited_at;
	u64 accumulated;
};

enum subsystem_smem_id {
	AOSD = 0,
	CXSD = 1,
	DDR = 2,
	DDR_STATS = 3,
	MPSS = 605,
	ADSP,
	CDSP,
	SLPI,
	GPU,
	DISPLAY,
	SLPI_ISLAND = 613,
	APSS = 631,
};

enum subsystem_pid {
	PID_APSS = 0,
	PID_MPSS = 1,
	PID_ADSP = 2,
	PID_SLPI = 3,
	PID_CDSP = 5,
	PID_WPSS = 13,
	PID_GPU = PID_APSS,
	PID_DISPLAY = PID_APSS,
	PID_OTHERS = -2,
};

struct stats_config {
	unsigned int offset_addr;
	unsigned int ddr_offset_addr;
	unsigned int num_records;
};

struct sleep_stats_data {
	dev_t		dev_no;
	struct class	*stats_class;
	struct device	*stats_device;
	struct cdev	stats_cdev;
	const struct stats_config	**config;
	void __iomem	*reg_base;
	void __iomem	*ddr_reg;
	void __iomem	**reg;
	u32	ddr_key;
	u32	ddr_entry_count;
};

struct system_data {
	const char *name;
	u32 smem_item;
	u32 pid;
	bool not_present;
};

static struct system_data subsystem_stats[] = {
	{ "apss", APSS, QCOM_SMEM_HOST_ANY },
	{ "modem", MPSS, PID_MPSS },
	{ "adsp", ADSP, PID_ADSP },
	{ "adsp_island", SLPI_ISLAND, PID_ADSP },
	{ "cdsp", CDSP, PID_CDSP },
	{ "slpi", SLPI, PID_SLPI },
	{ "slpi_island", SLPI_ISLAND, PID_SLPI },
	{ "gpu", GPU, PID_APSS },
	{ "display", DISPLAY, PID_APSS },
};

static struct system_data system_stats[] = {
	{ "aosd", AOSD, PID_OTHERS },
	{ "cxsd", CXSD, PID_OTHERS },
	{ "ddr", DDR, PID_OTHERS },
};

static bool subsystem_stats_debug_on;
/* Subsystem stats before and after suspend */
static struct sleep_stats *b_subsystem_stats;
static struct sleep_stats *a_subsystem_stats;
/* System sleep stats before and after suspend */
static struct sleep_stats *b_system_stats;
static struct sleep_stats *a_system_stats;
static bool ddr_freq_update;
static DEFINE_MUTEX(sleep_stats_mutex);

static int stats_data_open(struct inode *inode, struct file *file)
{
	struct sleep_stats_data *drvdata = NULL;

	if (!inode || !inode->i_cdev || !file)
		return -EINVAL;

	drvdata = container_of(inode->i_cdev, struct sleep_stats_data, stats_cdev);
	file->private_data = drvdata;

	return 0;
}

void ddr_stats_sleep_stat(struct sleep_stats_data *stats_data, struct sleep_stats *ddr_stats)
{
	void __iomem *reg;
	int i;

	reg = stats_data->ddr_reg + DDR_STATS_NUM_MODES_ADDR + 0x4;
	for (i = 0; i < stats_data->ddr_entry_count; i++) {
		(ddr_stats + i)->version = readl_relaxed(reg + DDR_STATS_NAME_ADDR);
		(ddr_stats + i)->count = readl_relaxed(reg + DDR_STATS_COUNT_ADDR);
		(ddr_stats + i)->last_entered_at = 0xDEADDEAD;
		(ddr_stats + i)->last_exited_at = 0xDEADDEAD;
		(ddr_stats + i)->accumulated = readq_relaxed(reg + DDR_STATS_DURATION_ADDR);
		reg += sizeof(struct sleep_stats) - 2 * sizeof(u64);
	}
}

static int subsystem_sleep_stats(struct sleep_stats_data *stats_data, struct sleep_stats *stats,
					unsigned int pid, unsigned int idx)
{
	struct sleep_stats *subsystem_stats_data;

	if (pid == SUBSYSTEM_STATS_OTHERS_NUM)
		memcpy_fromio(stats, stats_data->reg[idx], sizeof(*stats));
	else {
		subsystem_stats_data = qcom_smem_get(pid, idx, NULL);
		if (IS_ERR(subsystem_stats_data))
			return -ENODEV;

		stats->version = subsystem_stats_data->version;
		stats->count = subsystem_stats_data->count;
		stats->last_entered_at = subsystem_stats_data->last_entered_at;
		stats->last_exited_at = subsystem_stats_data->last_exited_at;
		stats->accumulated = subsystem_stats_data->accumulated;
	}

	return 0;
}

bool has_system_slept(void)
{
	int i;
	bool sleep_flag = true;

	for (i = 0; i < ARRAY_SIZE(system_stats); i++) {
		if (b_system_stats[i].count == a_system_stats[i].count) {
			pr_warn("System %s has not entered sleep\n", system_stats[i].name);
			sleep_flag = false;
		}
	}

	return sleep_flag;
}
EXPORT_SYMBOL(has_system_slept);

bool has_subsystem_slept(void)
{
	int i;
	bool sleep_flag = true;

	for (i = 0; i < ARRAY_SIZE(subsystem_stats); i++) {
		if (subsystem_stats[i].not_present)
			continue;

		if ((b_subsystem_stats[i].count == a_subsystem_stats[i].count) &&
			(a_subsystem_stats[i].last_exited_at >
				a_subsystem_stats[i].last_entered_at)) {
			pr_warn("Subsystem %s has not entered sleep\n", subsystem_stats[i].name);
			sleep_flag = false;
		}
	}

	return sleep_flag;
}
EXPORT_SYMBOL(has_subsystem_slept);

void subsystem_sleep_debug_enable(bool enable)
{
	subsystem_stats_debug_on = enable;
}
EXPORT_SYMBOL(subsystem_sleep_debug_enable);

static long stats_data_ioctl(struct file *file, unsigned int cmd,
			     unsigned long arg)
{
	struct sleep_stats_data *drvdata = file->private_data;
	struct sleep_stats *temp;
	int ret = -ENOMEM;
	unsigned int pid, smem_item;

	mutex_lock(&sleep_stats_mutex);
	if (cmd != DDR_STATS_IOCTL)
		temp = kzalloc(sizeof(struct sleep_stats), GFP_KERNEL);
	else
		temp = kcalloc(DDR_STATS_MAX_NUM_MODES, sizeof(struct sleep_stats), GFP_KERNEL);
	if (!temp)
		goto out_unlock;

	switch (cmd) {
	case APSS_IOCTL:
		pid = QCOM_SMEM_HOST_ANY;
		smem_item = APSS;
		break;
	case MODEM_IOCTL:
		pid = PID_MPSS;
		smem_item = MPSS;
		break;
	case WPSS_IOCTL:
		pid = PID_MPSS;
		smem_item = MPSS;
		break;
	case ADSP_IOCTL:
		pid = PID_ADSP;
		smem_item = ADSP;
		break;
	case ADSP_ISLAND_IOCTL:
		pid = PID_ADSP;
		smem_item = SLPI_ISLAND;
		break;
	case CDSP_IOCTL:
		pid = PID_CDSP;
		smem_item = CDSP;
		break;
	case SLPI_IOCTL:
		pid = PID_SLPI;
		smem_item = SLPI;
		break;
	case GPU_IOCTL:
		pid = PID_GPU;
		smem_item = GPU;
		break;
	case DISPLAY_IOCTL:
		pid = PID_DISPLAY;
		smem_item = DISPLAY;
		break;
	case SLPI_ISLAND_IOCTL:
		pid = PID_SLPI;
		smem_item = SLPI_ISLAND;
		break;
	case AOSD_IOCTL:
		pid = PID_OTHERS;
		smem_item = AOSD;
		break;
	case CXSD_IOCTL:
		pid = PID_OTHERS;
		smem_item = CXSD;
		break;
	case DDR_IOCTL:
		pid = PID_OTHERS;
		smem_item = DDR;
		break;
	case DDR_STATS_IOCTL:
		pid = PID_OTHERS;
		smem_item = DDR_STATS;
		break;
	default:
		pr_err("Incorrect command error\n");
		ret = -EINVAL;
		goto out_free;
	}

	if (cmd != DDR_STATS_IOCTL) {
		ret = subsystem_sleep_stats(drvdata, temp, pid, smem_item);
		if (ret < 0)
			goto out_free;

		/*
		 * If a subsystem is in sleep when reading the sleep stats from SMEM
		 * adjust the accumulated sleep duration to show actual sleep time.
		 * This ensures that the displayed stats are real when used for
		 * the purpose of computing battery utilization.
		 */
		if (temp->last_entered_at > temp->last_exited_at) {
			temp->accumulated +=
					(__arch_counter_get_cntvct()
					- temp->last_entered_at);
		}

		ret = copy_to_user((void __user *)arg, temp, sizeof(struct sleep_stats));
	} else {
		int modes = DDR_STATS_MAX_NUM_MODES;

		if (ddr_freq_update) {
			ret = ddr_stats_freq_sync_send_msg();
			if (ret < 0)
				goto out_free;
			udelay(500);
		}

		ddr_stats_sleep_stat(drvdata, temp);
		if (ddr_freq_update) {
			int i;
			/* Before transmitting ddr sleep_stats, check ddr freq's count. */
			for (i = DDR_STATS_NUM_MODES_ADDR; i < drvdata->ddr_entry_count; i++) {
				if ((temp + i)->count == 0) {
					pr_err("ddr_stats: Freq update failed\n");
					modes = DDR_STATS_NUM_MODES_ADDR;
				}
			}
		}

		ret = copy_to_user((void __user *)arg, temp,
					modes * sizeof(struct sleep_stats));
	}

	kfree(temp);
	mutex_unlock(&sleep_stats_mutex);

	return ret;
out_free:
	kfree(temp);
out_unlock:
	mutex_unlock(&sleep_stats_mutex);
	return ret;

}

static const struct file_operations stats_data_fops = {
	.owner		=	THIS_MODULE,
	.open		=	stats_data_open,
	.unlocked_ioctl =	stats_data_ioctl,
};

static int subsystem_stats_probe(struct platform_device *pdev)
{
	struct sleep_stats_data *stats_data;
	const struct stats_config *config;
	struct resource *res;
	void __iomem *offset_addr;
	phys_addr_t stats_base;
	resource_size_t stats_size;
	int ret = -ENOMEM;
	int i;
	u32 offset;

	stats_data = devm_kzalloc(&pdev->dev, sizeof(struct sleep_stats_data), GFP_KERNEL);
	if (!stats_data)
		return ret;

	ret = alloc_chrdev_region(&stats_data->dev_no, STATS_BASEMINOR,
				  STATS_MAX_MINOR, STATS_DEVICE_NAME);
	if (ret)
		goto fail_alloc_chrdev;

	cdev_init(&stats_data->stats_cdev, &stats_data_fops);
	ret = cdev_add(&stats_data->stats_cdev, stats_data->dev_no, 1);
	if (ret)
		goto fail_cdev_add;

	stats_data->stats_class = class_create(THIS_MODULE, STATS_DEVICE_NAME);
	if (IS_ERR_OR_NULL(stats_data->stats_class)) {
		ret =  -EINVAL;
		goto fail_class_create;
	}

	stats_data->stats_device = device_create(stats_data->stats_class, NULL,
						 stats_data->dev_no, NULL,
						 STATS_DEVICE_NAME);
	if (IS_ERR_OR_NULL(stats_data->stats_device)) {
		ret = -EINVAL;
		goto fail_device_create;
	}

	config = device_get_match_data(&pdev->dev);
	if (!config) {
		ret = -ENODEV;
		goto fail;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = PTR_ERR(res);
		goto fail;
	}

	offset_addr = devm_ioremap(&pdev->dev, res->start + config->offset_addr, sizeof(u32));
	if (IS_ERR(offset_addr)) {
		ret = PTR_ERR(offset_addr);
		goto fail;
	}

	stats_base = res->start | readl_relaxed(offset_addr);
	stats_size = resource_size(res);

	stats_data->reg_base = devm_ioremap(&pdev->dev, stats_base, stats_size);
	if (!stats_data->reg_base) {
		ret = -ENOMEM;
		goto fail;
	}

	stats_data->config = devm_kcalloc(&pdev->dev, config->num_records,
				sizeof(struct stats_config *), GFP_KERNEL);
	if (!stats_data->config) {
		ret = -ENOMEM;
		goto fail;
	}

	stats_data->reg = devm_kcalloc(&pdev->dev, config->num_records, sizeof(void __iomem *),
				GFP_KERNEL);
	if (!stats_data->reg) {
		ret = -ENOMEM;
		goto fail;
	}

	for (i = 0; i < config->num_records; i++) {
		stats_data->config[i] = config;
		offset = (i * sizeof(struct sleep_stats));
		stats_data->reg[i] = stats_data->reg_base + offset;
	}

	offset_addr = devm_ioremap(&pdev->dev, res->start + config->ddr_offset_addr, sizeof(u32));
	if (IS_ERR(offset_addr)) {
		ret = PTR_ERR(offset_addr);
		goto fail;
	}

	stats_base = res->start | readl_relaxed(offset_addr);
	stats_data->ddr_reg = devm_ioremap(&pdev->dev, stats_base, stats_size);
	if (!stats_data->ddr_reg) {
		ret = -ENOMEM;
		goto fail;
	}

	stats_data->ddr_key = readl_relaxed(stats_data->ddr_reg + DDR_STATS_MAGIC_KEY_ADDR);
	if (stats_data->ddr_key != DDR_STATS_MAGIC_KEY) {
		ret = -EINVAL;
		goto fail;
	}

	stats_data->ddr_entry_count = readl_relaxed(stats_data->ddr_reg + DDR_STATS_NUM_MODES_ADDR);
	if (stats_data->ddr_entry_count > DDR_STATS_MAX_NUM_MODES) {
		ret = -EINVAL;
		goto fail;
	}

	subsystem_stats_debug_on = false;
	b_subsystem_stats = devm_kcalloc(&pdev->dev, ARRAY_SIZE(subsystem_stats),
					 sizeof(struct sleep_stats), GFP_KERNEL);
	if (!b_subsystem_stats) {
		ret = -ENOMEM;
		goto fail;
	}

	a_subsystem_stats = devm_kcalloc(&pdev->dev, ARRAY_SIZE(subsystem_stats),
					 sizeof(struct sleep_stats), GFP_KERNEL);
	if (!a_subsystem_stats) {
		ret = -ENOMEM;
		goto fail;
	}

	b_system_stats = devm_kcalloc(&pdev->dev, ARRAY_SIZE(system_stats),
				      sizeof(struct sleep_stats), GFP_KERNEL);
	if (!b_system_stats) {
		ret = -ENOMEM;
		goto fail;
	}

	a_system_stats = devm_kcalloc(&pdev->dev, ARRAY_SIZE(system_stats),
				      sizeof(struct sleep_stats), GFP_KERNEL);
	if (!a_system_stats) {
		ret = -ENOMEM;
		goto fail;
	}

	ddr_freq_update = of_property_read_bool(pdev->dev.of_node,
							"ddr-freq-update");

	platform_set_drvdata(pdev, stats_data);

	return 0;

fail:
	device_destroy(stats_data->stats_class, stats_data->dev_no);
fail_device_create:
	class_destroy(stats_data->stats_class);
fail_class_create:
	cdev_del(&stats_data->stats_cdev);
fail_cdev_add:
	unregister_chrdev_region(stats_data->dev_no, 1);
fail_alloc_chrdev:
	return ret;
}

static int subsystem_stats_remove(struct platform_device *pdev)
{
	struct sleep_stats_data *stats_data;

	stats_data = platform_get_drvdata(pdev);
	if (!stats_data)
		return 0;

	device_destroy(stats_data->stats_class, stats_data->dev_no);
	class_destroy(stats_data->stats_class);
	cdev_del(&stats_data->stats_cdev);
	unregister_chrdev_region(stats_data->dev_no, 1);

	return 0;
}

static int subsytem_stats_suspend(struct device *dev)
{
	struct sleep_stats_data *stats_data = dev_get_drvdata(dev);
	int ret;
	int i;

	if (!subsystem_stats_debug_on)
		return 0;

	mutex_lock(&sleep_stats_mutex);
	for (i = 0; i < ARRAY_SIZE(subsystem_stats); i++) {
		ret = subsystem_sleep_stats(stats_data, b_subsystem_stats + i,
					subsystem_stats[i].pid, subsystem_stats[i].smem_item);
		if (ret == -ENODEV)
			subsystem_stats[i].not_present = true;
		else
			subsystem_stats[i].not_present = false;
	}

	for (i = 0; i < ARRAY_SIZE(system_stats); i++)
		subsystem_sleep_stats(stats_data, b_system_stats + i,
					system_stats[i].pid, system_stats[i].smem_item);
	mutex_unlock(&sleep_stats_mutex);

	return 0;
}

static int subsytem_stats_resume(struct device *dev)
{
	struct sleep_stats_data *stats_data = dev_get_drvdata(dev);
	int i;

	if (!subsystem_stats_debug_on)
		return 0;

	mutex_lock(&sleep_stats_mutex);
	for (i = 0; i < ARRAY_SIZE(subsystem_stats); i++)
		subsystem_sleep_stats(stats_data, a_subsystem_stats + i,
					subsystem_stats[i].pid, subsystem_stats[i].smem_item);

	for (i = 0; i < ARRAY_SIZE(system_stats); i++)
		subsystem_sleep_stats(stats_data, a_system_stats + i,
					system_stats[i].pid, system_stats[i].smem_item);
	mutex_unlock(&sleep_stats_mutex);

	return 0;
}

static const struct stats_config rpmh_data = {
	.offset_addr = 0x4,
	.ddr_offset_addr = 0x1c,
	.num_records = 3,
};

static const struct of_device_id subsystem_stats_table[] = {
	{ .compatible = "qcom,subsystem-sleep-stats", .data = &rpmh_data},
	{},
};

static const struct dev_pm_ops subsytem_stats_pm_ops = {
	.suspend_late = subsytem_stats_suspend,
	.resume_early = subsytem_stats_resume,
};

static struct platform_driver subsystem_sleep_stats_driver = {
	.probe	= subsystem_stats_probe,
	.remove	= subsystem_stats_remove,
	.driver	= {
		.name	= "subsystem_sleep_stats",
		.of_match_table	= subsystem_stats_table,
		.pm = &subsytem_stats_pm_ops,
	},
};

module_platform_driver(subsystem_sleep_stats_driver);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. (QTI) subsystem sleep stats driver");
