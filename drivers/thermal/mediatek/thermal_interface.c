// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/cpumask.h>
#include <linux/hrtimer.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#include <linux/device.h>
#include <linux/err.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/timer.h>
#include <linux/types.h>


#include <linux/io.h>
#include "thermal_interface.h"

#define THERM_INTF_DEBUGFS_ENTRY_RO(name) \
static int therm_intf_##name##_open(struct inode *i, struct file *file) \
{ \
	return single_open(file, therm_intf_##name##_show, i->i_private); \
} \
\
static const struct file_operations therm_intf_##name##_fops = { \
	.owner = THIS_MODULE, \
	.open = therm_intf_##name##_open, \
	.read = seq_read, \
	.llseek = seq_lseek, \
	.release = single_release, \
}

#define THERM_INTF_DEBUGFS_ENTRY_RW(name) \
static int therm_intf_##name##_open(struct inode *i, struct file *file) \
{ \
	return single_open(file, therm_intf_##name##_show, i->i_private); \
} \
\
static const struct file_operations therm_intf_##name##_fops = { \
	.owner = THIS_MODULE, \
	.open = therm_intf_##name##_open, \
	.read = seq_read, \
	.write = therm_intf_##name##_write, \
	.llseek = seq_lseek, \
	.release = single_release, \
}

#define MAX_HEADROOM		(100)

struct therm_intf_info {
	int sw_ready;
	unsigned int cpu_cluster_num;
	struct device *dev;
	void __iomem *csram_base;
	struct mutex lock;
	struct dentry *debug_dir;
};

static struct therm_intf_info tm_data;
void __iomem * thermal_csram_base;
EXPORT_SYMBOL(thermal_csram_base);

static int  therm_intf_get_cpu_headroom(enum headroom_id id)
{
	void __iomem *addr = tm_data.csram_base + CPU_HEADROOM_OFFSET + 4 * id;

	return sign_extend32(readl(addr), 31);
}

int get_thermal_headroom(enum headroom_id id)
{
	int headroom = 0;

	if (!tm_data.sw_ready)
		return MAX_HEADROOM;

	if (id >= SOC_CPU0 && id < SOC_CPU0 + NR_CPUS) {
		headroom = therm_intf_get_cpu_headroom(id);
	} else if (id == PCB_AP) {
		mutex_lock(&tm_data.lock);
		headroom = readl(tm_data.csram_base + AP_NTC_HEADROOM_OFFSET);
		mutex_unlock(&tm_data.lock);
	}

	return headroom;
}
EXPORT_SYMBOL(get_thermal_headroom);

int set_cpu_min_opp(int gear, int opp)
{
	if (!tm_data.sw_ready)
		return -ENODEV;

	if (gear >= tm_data.cpu_cluster_num)
		return -EINVAL;

	writel(opp, tm_data.csram_base + CPU_MIN_OPP_OFFSET + 4 * gear);

	return 0;
}
EXPORT_SYMBOL(set_cpu_min_opp);

static int therm_intf_hr_info_show(struct seq_file *m, void *unused)
{
	int i;

	for (i = 0; i < NR_HEADROOM_ID; i++) {
		if (i == NR_HEADROOM_ID - 1)
			seq_printf(m, "%d\n", get_thermal_headroom((enum headroom_id)i));
		else
			seq_printf(m, "%d,", get_thermal_headroom((enum headroom_id)i));
	}

	return 0;
}
THERM_INTF_DEBUGFS_ENTRY_RO(hr_info);

static ssize_t ttj_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char cmd[10];
	void __iomem *addr = tm_data.csram_base + CPU_TTJ_OFFSET;
	int ttj = 0;

	if (sscanf(buf, "%4s %d", cmd, &ttj) == 2) {
		if ((strncmp(cmd, "TTJ", 3) == 0)
			&& (ttj >= 0) && (ttj <= 100))
			writel(ttj, addr);
		else
			pr_info("get ttj fail\n");
	} else {
		pr_info("[thermal_ttj] invalid input\n");
		return -EINVAL;
	}

	return count;
}
static ssize_t ap_headroom_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "AP NTC HEADROOM = %d\n",
		readl(tm_data.csram_base + AP_NTC_HEADROOM_OFFSET));

	return len;
}

static ssize_t ap_headroom_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char cmd[10];
	void __iomem *addr = tm_data.csram_base + AP_NTC_HEADROOM_OFFSET;
	int headroom;

	if (sscanf(buf, "%6s %d", cmd, &headroom) == 2) {
		if ((strncmp(cmd, "AP_HD", 5) == 0)
			&& (headroom >= -100) && (headroom <= 100))
			writel(headroom, addr);
		else
			pr_info("get ap ntc headroom fail\n");
	} else {
		pr_info("[headroom_store] invalid input\n");
		return -EINVAL;
	}

	return count;
}

static struct kobj_attribute ttj_attr = __ATTR_WO(ttj);
static struct kobj_attribute ap_ntc_headroom_attr = __ATTR_RW(ap_headroom);

static struct attribute *thermal_attrs[] = {
	&ttj_attr.attr,
	&ap_ntc_headroom_attr.attr,
	NULL
};
static struct attribute_group thermal_attr_group = {
	.name	= "thermal",
	.attrs	= thermal_attrs,
};


static const struct of_device_id therm_intf_of_match[] = {
	{ .compatible = "mediatek,therm_intf", },
	{},
};
MODULE_DEVICE_TABLE(of, therm_intf_of_match);

static int therm_intf_probe(struct platform_device *pdev)
{
	struct resource *res;
	void __iomem *csram;
	struct device_node *cpu_np;
	struct of_phandle_args args;
	unsigned int cpu, max_perf_domain = 0;
	int ret;

	if (!pdev->dev.of_node) {
		dev_info(&pdev->dev, "Only DT based supported\n");
		return -ENODEV;
	}

	tm_data.dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	csram = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(csram))
		return PTR_ERR(csram);

	tm_data.csram_base = csram;
	thermal_csram_base = csram;

	/* get CPU cluster num */
	for_each_possible_cpu(cpu) {
		cpu_np = of_cpu_device_node_get(cpu);
		if (!cpu_np) {
			dev_info(&pdev->dev, "Failed to get cpu %d device\n", cpu);
			return -ENODEV;
		}

		ret = of_parse_phandle_with_args(cpu_np, "performance-domains",
						 "#performance-domain-cells", 0,
						 &args);

		if (ret < 0)
			return ret;

		max_perf_domain = max(max_perf_domain, args.args[0]);
	}

	tm_data.cpu_cluster_num = max_perf_domain + 1;
	dev_info(&pdev->dev, "cpu_cluster_num = %d\n", tm_data.cpu_cluster_num);

	/* debugfs */
	tm_data.debug_dir = debugfs_create_dir("therm_intf", NULL);
	if (!tm_data.debug_dir) {
		dev_info(tm_data.dev, "failed to create therm_intf debugfs dir\n");
		return -ENODEV;
	}
	if (!debugfs_create_file("hr_info", 0440,
		tm_data.debug_dir, NULL, &therm_intf_hr_info_fops))
		return -ENODEV;

	ret = sysfs_create_group(kernel_kobj, &thermal_attr_group);
	if (ret) {
		dev_info(&pdev->dev, "failed to create thermal sysfs, ret=%d!\n", ret);
		return ret;
	}
	writel(100, tm_data.csram_base + AP_NTC_HEADROOM_OFFSET);

	mutex_init(&tm_data.lock);

	tm_data.sw_ready = 1;

	return 0;
}

static int therm_intf_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver therm_intf_driver = {
	.probe = therm_intf_probe,
	.remove = therm_intf_remove,
	.driver = {
		.name = "mtk-thermal-interface",
		.of_match_table = therm_intf_of_match,
	},
};

module_platform_driver(therm_intf_driver);

MODULE_AUTHOR("Henry Huang <henry.huang@mediatek.com>");
MODULE_DESCRIPTION("Mediatek thermal interface driver");
MODULE_LICENSE("GPL v2");

