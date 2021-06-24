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
struct fps_cooler_info fps_cooler_data;
EXPORT_SYMBOL(fps_cooler_data);

static int  therm_intf_read_csram_s32(int offset)
{
	void __iomem *addr = tm_data.csram_base + offset;

	return sign_extend32(readl(addr), 31);
}

int get_thermal_headroom(enum headroom_id id)
{
	int headroom = 0;

	if (!tm_data.sw_ready)
		return MAX_HEADROOM;

	if (id >= SOC_CPU0 && id < SOC_CPU0 + num_possible_cpus()) {
		headroom = therm_intf_read_csram_s32(CPU_HEADROOM_OFFSET + 4 * id);
	} else if (id == PCB_AP) {
		mutex_lock(&tm_data.lock);
		headroom = therm_intf_read_csram_s32(AP_NTC_HEADROOM_OFFSET);
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

	writel(opp, tm_data.csram_base + CPU_MIN_OPP_HINT_OFFSET + 4 * gear);

	return 0;
}
EXPORT_SYMBOL(set_cpu_min_opp);

int set_cpu_active_bitmask(int mask)
{
	if (!tm_data.sw_ready)
		return -ENODEV;

	writel(mask, tm_data.csram_base + CPU_ACTIVE_BITMASK_OFFSET);

	return 0;
}
EXPORT_SYMBOL(set_cpu_active_bitmask);

int get_cpu_temp(int cpu_id)
{
	int temp = 25000;

	if (!tm_data.sw_ready || cpu_id >= num_possible_cpus())
		return temp;

	temp = therm_intf_read_csram_s32(CPU_TEMP_OFFSET + 4 * cpu_id);

	return temp;
}
EXPORT_SYMBOL(get_cpu_temp);

static ssize_t headroom_info_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int i;
	int len = 0;

	for (i = 0; i < NR_HEADROOM_ID; i++) {
		if (i == NR_HEADROOM_ID - 1)
			len += snprintf(buf + len, PAGE_SIZE - len, "%d\n",
				get_thermal_headroom((enum headroom_id)i));
		else
			len += snprintf(buf + len, PAGE_SIZE - len, "%d,",
				get_thermal_headroom((enum headroom_id)i));
	}

	return len;
}

static void write_ttj(unsigned int cpu_ttj, unsigned int gpu_ttj,
	unsigned int apu_ttj)
{
	void __iomem *addr = tm_data.csram_base + TTJ_OFFSET;

	writel(cpu_ttj, addr);
	writel(gpu_ttj, addr + 4);
	writel(apu_ttj, addr + 8);
}

static ssize_t ttj_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%u, %u, %u\n",
		therm_intf_read_csram_s32(TTJ_OFFSET),
		therm_intf_read_csram_s32(TTJ_OFFSET + 4),
		therm_intf_read_csram_s32(TTJ_OFFSET + 8));

	return len;
}

static ssize_t ttj_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char cmd[10];
	unsigned int cpu_ttj, gpu_ttj, apu_ttj;

	if (sscanf(buf, "%4s %u %u %u", cmd, &cpu_ttj, &gpu_ttj, &apu_ttj)
		== 4) {
		if (strncmp(cmd, "TTJ", 3) == 0) {
			write_ttj(cpu_ttj, gpu_ttj, apu_ttj);

			return count;
		}
	}

	pr_info("[thermal_ttj] invalid input\n");

	return -EINVAL;
}

static void write_power_budget(unsigned int cpu_pb, unsigned int gpu_pb,
	unsigned int apu_pb)
{
	void __iomem *addr = tm_data.csram_base + POWER_BUDGET_OFFSET;

	writel(cpu_pb, addr);
	writel(gpu_pb, addr + 4);
	writel(apu_pb, addr + 8);
}

static ssize_t power_budget_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%u, %u, %u\n",
		therm_intf_read_csram_s32(POWER_BUDGET_OFFSET),
		therm_intf_read_csram_s32(POWER_BUDGET_OFFSET + 4),
		therm_intf_read_csram_s32(POWER_BUDGET_OFFSET + 8));

	return len;
}

static ssize_t power_budget_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char cmd[10];
	unsigned int cpu_pb, gpu_pb, apu_pb;

	if (sscanf(buf, "%3s %u %u %u", cmd, &cpu_pb, &gpu_pb, &apu_pb) == 4) {
		if (strncmp(cmd, "pb", 2) == 0) {
			write_power_budget(cpu_pb, gpu_pb, apu_pb);
			return count;
		}
	}

	pr_info("[thermal_power_budget] invalid input\n");

	return -EINVAL;
}

static ssize_t cpu_info_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		therm_intf_read_csram_s32(CPU_MIN_OPP_HINT_OFFSET),
		therm_intf_read_csram_s32(CPU_MIN_OPP_HINT_OFFSET + 4),
		therm_intf_read_csram_s32(CPU_MIN_OPP_HINT_OFFSET + 8),
		therm_intf_read_csram_s32(CPU_LIMIT_OPP_OFFSET),
		therm_intf_read_csram_s32(CPU_LIMIT_OPP_OFFSET + 4),
		therm_intf_read_csram_s32(CPU_LIMIT_OPP_OFFSET + 8),
		therm_intf_read_csram_s32(CPU_CUR_OPP_OFFSET),
		therm_intf_read_csram_s32(CPU_CUR_OPP_OFFSET + 4),
		therm_intf_read_csram_s32(CPU_CUR_OPP_OFFSET + 8),
		therm_intf_read_csram_s32(CPU_MAX_TEMP_OFFSET),
		therm_intf_read_csram_s32(CPU_MAX_TEMP_OFFSET + 4),
		therm_intf_read_csram_s32(CPU_MAX_TEMP_OFFSET + 8));

	return len;
}

static ssize_t cpu_temp_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;
	int cpu_id = 0;

	for (cpu_id = 0; cpu_id < num_possible_cpus(); cpu_id++) {
		if (cpu_id == num_possible_cpus() - 1)
			len += snprintf(buf + len, PAGE_SIZE - len, "%d\n",
				get_cpu_temp(cpu_id));
		else
			len += snprintf(buf + len, PAGE_SIZE - len, "%d,",
				get_cpu_temp(cpu_id));
	}

	return len;
}

static ssize_t gpu_info_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%d,%d,%d\n",
		therm_intf_read_csram_s32(GPU_TEMP_OFFSET),
		therm_intf_read_csram_s32(GPU_TEMP_OFFSET + 4),
		therm_intf_read_csram_s32(GPU_TEMP_OFFSET + 8));

	return len;
}

static ssize_t apu_info_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%d,%d,%d\n",
		therm_intf_read_csram_s32(APU_TEMP_OFFSET),
		therm_intf_read_csram_s32(APU_TEMP_OFFSET + 4),
		therm_intf_read_csram_s32(APU_TEMP_OFFSET + 8));

	return len;
}

static ssize_t fps_cooler_info_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%d,%d,%d,%d,%d\n",
		fps_cooler_data.target_fps, fps_cooler_data.tpcb,
		fps_cooler_data.tpcb_slope, fps_cooler_data.ap_headroom,
		fps_cooler_data.n_sec_to_ttpcb);

	return len;
}

static ssize_t fps_cooler_info_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int target_fps, tpcb, tpcb_slope, ap_headroom, n_sec_to_ttpcb;
	void __iomem *addr = tm_data.csram_base + AP_NTC_HEADROOM_OFFSET;
	void __iomem *tpcb_addr = tm_data.csram_base + TPCB_OFFSET;

	if(sscanf(buf, "%d,%d,%d,%d,%d", &target_fps, &tpcb, &tpcb_slope,
				&ap_headroom, &n_sec_to_ttpcb) == 5)
	{
		if ((ap_headroom >= -100) && (ap_headroom <= 100))
		{
			writel(ap_headroom, addr);
			fps_cooler_data.ap_headroom = ap_headroom;
		} else {
			pr_info("[fps_cooler_info_store] invalid ap head room input\n");
			return -EINVAL;
		}

		writel(tpcb, tpcb_addr);
		fps_cooler_data.tpcb = tpcb;
		fps_cooler_data.target_fps = target_fps;
		fps_cooler_data.tpcb_slope = tpcb_slope;
		fps_cooler_data.n_sec_to_ttpcb = n_sec_to_ttpcb;
	} else {
		pr_info("[fps_cooler_info_store] invalid input\n");
		return -EINVAL;
	}

	return count;
}

static ssize_t atc_show(struct kobject *kobj, struct kobj_attribute *attr,
	char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%d, %d, %d\n",
		therm_intf_read_csram_s32(ATC_OFFSET),
		therm_intf_read_csram_s32(ATC_OFFSET + 4),
		therm_intf_read_csram_s32(ATC_OFFSET + 8));

	return len;
}

static ssize_t target_tpcb_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;
	void __iomem *target_tpcb_addr = tm_data.csram_base + TARGET_TPCB_OFFSET;
	int target_tpcb = readl(target_tpcb_addr);

	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", target_tpcb);

	return len;
}

static ssize_t target_tpcb_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int target_tpcb = 0;
	void __iomem *target_tpcb_addr = tm_data.csram_base + TARGET_TPCB_OFFSET;

	if(sscanf(buf, "%d", &target_tpcb) == 1)
		writel(target_tpcb, target_tpcb_addr);
	else {
		pr_info("[target_tpcb_store] invalid input\n");
		return -EINVAL;
	}

	return count;
}

static struct kobj_attribute ttj_attr = __ATTR_RW(ttj);
static struct kobj_attribute power_budget_attr = __ATTR_RW(power_budget);
static struct kobj_attribute cpu_info_attr = __ATTR_RO(cpu_info);
static struct kobj_attribute gpu_info_attr = __ATTR_RO(gpu_info);
static struct kobj_attribute apu_info_attr = __ATTR_RO(apu_info);
static struct kobj_attribute fps_cooler_info_attr = __ATTR_RW(fps_cooler_info);
static struct kobj_attribute cpu_temp_attr = __ATTR_RO(cpu_temp);
static struct kobj_attribute headroom_info_attr = __ATTR_RO(headroom_info);
static struct kobj_attribute atc_attr = __ATTR_RO(atc);
static struct kobj_attribute target_tpcb_attr = __ATTR_RW(target_tpcb);

static struct attribute *thermal_attrs[] = {
	&ttj_attr.attr,
	&power_budget_attr.attr,
	&cpu_info_attr.attr,
	&gpu_info_attr.attr,
	&apu_info_attr.attr,
	&fps_cooler_info_attr.attr,
	&cpu_temp_attr.attr,
	&headroom_info_attr.attr,
	&atc_attr.attr,
	&target_tpcb_attr.attr,
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

	ret = sysfs_create_group(kernel_kobj, &thermal_attr_group);
	if (ret) {
		dev_info(&pdev->dev, "failed to create thermal sysfs, ret=%d!\n", ret);
		return ret;
	}

	writel(100, tm_data.csram_base + AP_NTC_HEADROOM_OFFSET);
	write_ttj(0, 0, 0);
	write_power_budget(0, 0, 0);

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

