// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2019, 2021 The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <soc/qcom/boot_stats.h>

#define MAX_STRING_LEN 256
#define BOOT_MARKER_MAX_LEN 50
#define MSM_ARCH_TIMER_FREQ     19200000
#define BOOTKPI_BUF_SIZE (2 * PAGE_SIZE)
#define TIMER_KHZ 32768

struct boot_stats {
	uint32_t bootloader_start;
	uint32_t bootloader_end;
	uint32_t bootloader_display;
	uint32_t bootloader_load_kernel;
#ifdef CONFIG_QGKI_MSM_BOOT_TIME_MARKER
	uint32_t bootloader_load_kernel_start;
	uint32_t bootloader_load_kernel_end;
#endif
};

static void __iomem *mpm_counter_base;
static uint32_t mpm_counter_freq;
static struct boot_stats __iomem *boot_stats;

#ifdef CONFIG_QGKI_MSM_BOOT_TIME_MARKER

struct boot_marker {
	char marker_name[BOOT_MARKER_MAX_LEN];
	unsigned long long timer_value;
	struct list_head list;
	spinlock_t slock;
};

static struct boot_marker boot_marker_list;
static struct kobject *bootkpi_obj;
static struct attribute_group *attr_grp;

unsigned long long msm_timer_get_sclk_ticks(void)
{
	unsigned long long t1, t2;
	int loop_count = 10;
	int loop_zero_count = 3;
	u64 tmp = USEC_PER_SEC;
	void __iomem *sclk_tick;

	do_div(tmp, TIMER_KHZ);
	tmp /= (loop_zero_count-1);
	sclk_tick = mpm_counter_base;
	if (!sclk_tick)
		return -EINVAL;

	while (loop_zero_count--) {
		t1 = readl_no_log(sclk_tick);
		do {
			udelay(1);
			t2 = t1;
			t1 = readl_no_log(sclk_tick);
		} while ((t2 != t1) && --loop_count);
		if (!loop_count) {
			pr_err("boot_stats: SCLK  did not stabilize\n");
			return 0;
		}
		if (t1)
			break;

		udelay(tmp);
	}
	if (!loop_zero_count) {
		pr_err("boot_stats: SCLK reads zero\n");
		return 0;
	}
	return t1;
}

static void _destroy_boot_marker(const char *name)
{
	struct boot_marker *marker;
	struct boot_marker *temp_addr;

	spin_lock(&boot_marker_list.slock);
	list_for_each_entry_safe(marker, temp_addr, &boot_marker_list.list,
			list) {
		if (strnstr(marker->marker_name, name,
			 strlen(marker->marker_name))) {
			list_del(&marker->list);
			kfree(marker);
		}
	}
	spin_unlock(&boot_marker_list.slock);
}

static void _create_boot_marker(const char *name,
		unsigned long long timer_value)
{
	struct boot_marker *new_boot_marker;

	pr_debug("%-41s:%llu.%03llu seconds\n", name,
			timer_value/TIMER_KHZ,
			((timer_value % TIMER_KHZ)
			 * 1000) / TIMER_KHZ);

	new_boot_marker = kmalloc(sizeof(*new_boot_marker), GFP_ATOMIC);
	if (!new_boot_marker)
		return;

	strlcpy(new_boot_marker->marker_name, name,
			sizeof(new_boot_marker->marker_name));
	new_boot_marker->timer_value = timer_value;

	spin_lock(&boot_marker_list.slock);
	list_add_tail(&(new_boot_marker->list), &(boot_marker_list.list));
	spin_unlock(&boot_marker_list.slock);
}

static void boot_marker_cleanup(void)
{
	struct boot_marker *marker;
	struct boot_marker *temp_addr;

	spin_lock(&boot_marker_list.slock);
	list_for_each_entry_safe(marker, temp_addr, &boot_marker_list.list,
			list) {
		list_del(&marker->list);
		kfree(marker);
	}
	spin_unlock(&boot_marker_list.slock);
}

void place_marker(const char *name)
{
	_create_boot_marker((char *)name, msm_timer_get_sclk_ticks());
}
EXPORT_SYMBOL(place_marker);

void destroy_marker(const char *name)
{
	_destroy_boot_marker((char *) name);
}
EXPORT_SYMBOL(destroy_marker);

static void set_bootloader_stats(void)
{
	if (IS_ERR_OR_NULL(boot_stats)) {
		pr_err("boot_marker: imem not initialized!\n");
		return;
	}

	_create_boot_marker("M - APPSBL Start - ",
		readl_relaxed(&boot_stats->bootloader_start));
	_create_boot_marker("M - APPSBL Kernel Load Start - ",
		readl_relaxed(&boot_stats->bootloader_load_kernel_start));
	_create_boot_marker("M - APPSBL Kernel Load End - ",
		readl_relaxed(&boot_stats->bootloader_load_kernel_end));
	_create_boot_marker("D - APPSBL Kernel Load Time - ",
		readl_relaxed(&boot_stats->bootloader_load_kernel));
	_create_boot_marker("M - APPSBL End - ",
		readl_relaxed(&boot_stats->bootloader_end));
}

static ssize_t bootkpi_reader(struct kobject *obj, struct kobj_attribute *attr,
		char *user_buffer)
{
	int rc = 0;
	char *buf;
	int temp = 0;
	struct boot_marker *marker;

	buf = kmalloc(BOOTKPI_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	spin_lock(&boot_marker_list.slock);
	list_for_each_entry(marker, &boot_marker_list.list, list) {
		WARN_ON((BOOTKPI_BUF_SIZE - temp) <= 0);
		temp += scnprintf(buf + temp, BOOTKPI_BUF_SIZE - temp,
				"%-41s:%llu.%03llu seconds\n",
				marker->marker_name,
				marker->timer_value/TIMER_KHZ,
				(((marker->timer_value % TIMER_KHZ)
				  * 1000) / TIMER_KHZ));
	}
	spin_unlock(&boot_marker_list.slock);
	rc = scnprintf(user_buffer, temp + 1, "%s\n", buf);
	kfree(buf);
	return rc;
}

static ssize_t bootkpi_writer(struct kobject *obj, struct kobj_attribute *attr,
		const char *user_buffer, size_t count)
{
	int rc = 0;
	char buf[MAX_STRING_LEN];

	if (count >= MAX_STRING_LEN)
		return -EINVAL;

	rc = scnprintf(buf, sizeof(buf) - 1, "%s", user_buffer);
	if (rc < 0)
		return rc;

	buf[rc] = '\0';
	place_marker(buf);
	return rc;
}

static ssize_t mpm_timer_read(struct kobject *obj, struct kobj_attribute *attr,
		char *user_buffer)
{
	unsigned long long timer_value;
	char buf[100];
	int temp = 0;

	timer_value = msm_timer_get_sclk_ticks();

	temp = scnprintf(buf, sizeof(buf), "%llu.%03llu seconds\n",
			timer_value/TIMER_KHZ,
			(((timer_value % TIMER_KHZ) * 1000) / TIMER_KHZ));

	return scnprintf(user_buffer, temp + 1, "%s\n", buf);
}

static struct kobj_attribute kpi_values_attribute =
	__ATTR(kpi_values, 0644, bootkpi_reader, bootkpi_writer);

static struct kobj_attribute mpm_timer_attribute =
	__ATTR(mpm_timer, 0444, mpm_timer_read, NULL);

static struct attribute *attrs[] = {
	&kpi_values_attribute.attr,
	&mpm_timer_attribute.attr,
	NULL,
};

static int bootkpi_sysfs_init(void)
{
	int ret;

	attr_grp = kzalloc(sizeof(struct attribute_group), GFP_KERNEL);
	if (!attr_grp)
		return -ENOMEM;

	bootkpi_obj = kobject_create_and_add("boot_kpi", kernel_kobj);
	if (!bootkpi_obj) {
		pr_err("boot_marker: Could not create kobject\n");
		ret = -ENOMEM;
		goto kobj_err;
	}

	attr_grp->attrs = attrs;

	ret = sysfs_create_group(bootkpi_obj, attr_grp);
	if (ret) {
		pr_err("boot_marker: Could not create sysfs group\n");
		goto err;
	}
	return 0;
err:
	kobject_del(bootkpi_obj);
kobj_err:
	kfree(attr_grp);
	return ret;
}

static int init_bootkpi(void)
{
	int ret = 0;

	ret = bootkpi_sysfs_init();
	if (ret)
		return ret;

	INIT_LIST_HEAD(&boot_marker_list.list);
	spin_lock_init(&boot_marker_list.slock);
	return 0;
}

static void exit_bootkpi(void)
{
	boot_marker_cleanup();
	sysfs_remove_group(bootkpi_obj, attr_grp);
	kobject_del(bootkpi_obj);
	kfree(attr_grp);
}
#endif

static int mpm_parse_dt(void)
{
	struct device_node *np_imem, *np_mpm2;

	np_imem = of_find_compatible_node(NULL, NULL,
				"qcom,msm-imem-boot_stats");
	if (!np_imem) {
		pr_err("can't find qcom,msm-imem node\n");
		return -ENODEV;
	}
	boot_stats = of_iomap(np_imem, 0);
	if (!boot_stats) {
		pr_err("boot_stats: Can't map imem\n");
		goto err1;
	}

	np_mpm2 = of_find_compatible_node(NULL, NULL,
				"qcom,mpm2-sleep-counter");
	if (!np_mpm2) {
		pr_err("mpm_counter: can't find DT node\n");
		goto err1;
	}

	if (of_property_read_u32(np_mpm2, "clock-frequency", &mpm_counter_freq))
		goto err2;

	if (of_get_address(np_mpm2, 0, NULL, NULL)) {
		mpm_counter_base = of_iomap(np_mpm2, 0);
		if (!mpm_counter_base) {
			pr_err("mpm_counter: cant map counter base\n");
			goto err2;
		}
	} else
		goto err2;

	return 0;

err2:
	of_node_put(np_mpm2);
err1:
	of_node_put(np_imem);
	return -ENODEV;
}

static void print_boot_stats(void)
{
	pr_info("KPI: Bootloader start count = %u\n",
		readl_relaxed(&boot_stats->bootloader_start));
	pr_info("KPI: Bootloader end count = %u\n",
		readl_relaxed(&boot_stats->bootloader_end));
	pr_info("KPI: Bootloader display count = %u\n",
		readl_relaxed(&boot_stats->bootloader_display));
	pr_info("KPI: Bootloader load kernel count = %u\n",
		readl_relaxed(&boot_stats->bootloader_load_kernel));
	pr_info("KPI: Kernel MPM timestamp = %u\n",
		readl_relaxed(mpm_counter_base));
	pr_info("KPI: Kernel MPM Clock frequency = %u\n",
		mpm_counter_freq);
}

static int __init boot_stats_init(void)
{
	int ret;

	ret = mpm_parse_dt();
	if (ret < 0)
		return -ENODEV;

	print_boot_stats();
	if (boot_marker_enabled()) {
		ret = init_bootkpi();
		if (ret) {
			pr_err("boot_stats: BootKPI init failed %d\n");
			return ret;
		}
#ifdef CONFIG_QGKI_MSM_BOOT_TIME_MARKER
		set_bootloader_stats();
#endif
	} else {
		iounmap(boot_stats);
		iounmap(mpm_counter_base);
	}

	return 0;
}
subsys_initcall(boot_stats_init);

static void __exit boot_stats_exit(void)
{
	if (boot_marker_enabled()) {
		exit_bootkpi();
		iounmap(boot_stats);
		iounmap(mpm_counter_base);
	}
}
module_exit(boot_stats_exit)

MODULE_DESCRIPTION("MSM boot stats info driver");
MODULE_LICENSE("GPL v2");
