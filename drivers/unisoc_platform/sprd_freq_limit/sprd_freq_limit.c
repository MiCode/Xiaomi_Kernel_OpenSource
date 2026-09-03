// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) "sprd-freq-limit: " fmt

#include <linux/cpufreq.h>
#include <linux/devfreq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pm_qos.h>
#include <linux/platform_device.h>
#include <linux/units.h>

#define to_devfreq(device)		container_of((device), struct devfreq, dev)

struct sprd_cpufreq_limit {
	struct cpufreq_policy *policy;

	uint32_t thermald_max_freq;
	struct freq_qos_request thermald_max_req;

	uint32_t bcl_max_freq;
	struct freq_qos_request bcl_max_req;
};

struct sprd_devfreq_limit {
	struct devfreq *devfreq;

	uint32_t thermald_max_freq;
	struct dev_pm_qos_request thermald_max_req;

	uint32_t bcl_max_freq;
	struct dev_pm_qos_request bcl_max_req;
};

struct sprd_limit_data {
	int (*device_init)(struct device *dev);
	void (*device_exit)(void);
};

enum sprd_limit_device {
	SPRD_APCPU,
	SPRD_DEVICE,
	MAX_DEVICE_NUM,
};

static struct sprd_cpufreq_limit *cpu_limits;
static struct sprd_devfreq_limit *dev_limits;
static unsigned int cluster_num;
static unsigned int device_num;

static inline int sprd_get_cluster(int cpu)
{
	return parse_perf_domain(cpu, "sprd,freq-domain", "#freq-domain-cells");
}

static inline int sprd_get_cluster_num(unsigned int *cluster_num)
{
	int cpu_max = num_possible_cpus() - 1;
	int ret;

	ret = sprd_get_cluster(cpu_max);
	if (ret < 0)
		return ret;

	*cluster_num = ret + 1;

	return 0;
}

static inline struct sprd_devfreq_limit *sprd_get_devlimit(struct devfreq *df)
{
	struct sprd_devfreq_limit *dev_limit;
	int i;

	if (!df || !dev_limits)
		return NULL;

	for (i = 0; i < device_num; i++) {
		dev_limit = dev_limits + i;
		if (dev_limit->devfreq == df)
			return dev_limit;
	}

	return NULL;
}

#define cpu_store_one(file_name, object)				\
static ssize_t store_##file_name					\
(struct cpufreq_policy *policy, const char *buf, size_t count)		\
{									\
	struct sprd_cpufreq_limit *cpu_limit;				\
	unsigned int idx;						\
	unsigned long val;						\
	int ret;							\
									\
	cpu_limit = cpu_limits + sprd_get_cluster(policy->cpu);		\
	if (!cpu_limit || !cpu_limit->policy)				\
		return -EINVAL;						\
									\
	ret = kstrtoul(buf, 10, &val);					\
	if (ret)							\
		return ret;						\
									\
	val = clamp_val(val, policy->cpuinfo.min_freq, policy->cpuinfo.max_freq);\
	idx = cpufreq_table_find_index_al(policy, val);			\
	val = policy->freq_table[idx].frequency;			\
									\
	if (val == cpu_limit->object##_freq)				\
		return count;						\
									\
	ret = freq_qos_update_request(&cpu_limit->object##_req, val);	\
	if (ret >= 0) {							\
		cpu_limit->object##_freq = val;				\
		return count;						\
	}								\
									\
	return ret;							\
}

cpu_store_one(thermald_max_freq, thermald_max);
cpu_store_one(bcl_max_freq, bcl_max);

#define cpu_show_one(file_name, object)				\
static ssize_t show_##file_name					\
(struct cpufreq_policy *policy, char *buf)			\
{								\
	struct sprd_cpufreq_limit *cpu_limit;			\
								\
	cpu_limit = cpu_limits + sprd_get_cluster(policy->cpu);	\
	if (!cpu_limit || !cpu_limit->policy)			\
		return -EINVAL;					\
								\
	return sprintf(buf, "%u\n", cpu_limit->object##_freq);	\
}

cpu_show_one(thermald_max_freq, thermald_max);
cpu_show_one(bcl_max_freq, bcl_max);

#define cpu_freq_limit_attr_rw(_name)				\
static struct freq_attr _name =					\
__ATTR(_name, 0644, show_##_name, store_##_name)		\

cpu_freq_limit_attr_rw(thermald_max_freq);
cpu_freq_limit_attr_rw(bcl_max_freq);

static const struct attribute *cpufreq_limit_attrs[] = {
	&thermald_max_freq.attr,
	&bcl_max_freq.attr,
	NULL,
};

#define dev_show_one(file_name, object)					\
static ssize_t file_name##_show						\
(struct device *dev, struct device_attribute *attr, char *buf)		\
{									\
	struct devfreq *df = to_devfreq(dev);				\
	struct sprd_devfreq_limit *dev_limit = sprd_get_devlimit(df);	\
									\
	if (!dev_limit)							\
		return -EINVAL;						\
									\
	return sprintf(buf, "%u\n", dev_limit->object##_freq);		\
}

dev_show_one(thermald_max_freq, thermald_max);
dev_show_one(bcl_max_freq, bcl_max);

#define dev_store_one(file_name, object)				\
static ssize_t file_name##_store					\
(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)\
{									\
	struct devfreq *df = to_devfreq(dev);				\
	struct sprd_devfreq_limit *dev_limit = sprd_get_devlimit(df);	\
	unsigned long value;						\
	int ret;							\
									\
	if (!dev_limit)							\
		return -EINVAL;						\
									\
	if (!dev_pm_qos_request_active(&dev_limit->object##_req))	\
		return -EINVAL;						\
									\
	ret = kstrtoul(buf, 10, &value);				\
	if (ret)							\
		return ret;						\
									\
	if (value) {							\
		dev_limit->object##_freq = value;			\
		value = DIV_ROUND_UP(value, HZ_PER_KHZ);		\
	} else {							\
		dev_limit->object##_freq = PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE;\
		value = PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE;		\
	}								\
									\
	ret = dev_pm_qos_update_request(&dev_limit->object##_req, value);\
	if (ret < 0)							\
		return ret;						\
									\
	return count;							\
}

dev_store_one(thermald_max_freq, thermald_max);
dev_store_one(bcl_max_freq, bcl_max);

static DEVICE_ATTR_RW(thermald_max_freq);
static DEVICE_ATTR_RW(bcl_max_freq);

static const struct attribute *devfreq_limit_attrs[] = {
	&dev_attr_thermald_max_freq.attr,
	&dev_attr_bcl_max_freq.attr,
	NULL,
};

static void sprd_cpufreq_remove_qos(struct sprd_cpufreq_limit *cpu_limit)
{

	if (cpu_limit->bcl_max_freq) {
		freq_qos_remove_request(&cpu_limit->bcl_max_req);
		cpu_limit->bcl_max_freq = 0;
	}

	if (cpu_limit->thermald_max_freq) {
		freq_qos_remove_request(&cpu_limit->thermald_max_req);
		cpu_limit->thermald_max_freq = 0;
	}
}

static int sprd_cpufreq_limit_init(struct device *dev)
{
	struct sprd_cpufreq_limit *cpu_limit;
	struct cpufreq_policy *policy;
	unsigned int cluster_id;
	int cpu, ret;

	ret = sprd_get_cluster_num(&cluster_num);
	if (ret) {
		pr_err("Failed to get cpu topology(%d)\n", ret);
		return ret;
	}

	cpu_limits = devm_kzalloc(dev, sizeof(*cpu_limits) * cluster_num, GFP_KERNEL);
	if (!cpu_limits)
		return -ENOMEM;

	for_each_possible_cpu(cpu) {
		cluster_id = sprd_get_cluster(cpu);
		if (cluster_id >= cluster_num)
			return -EINVAL;

		cpu_limit = cpu_limits + cluster_id;
		if (!cpu_limit)
			return -EINVAL;

		if (cpu_limit->policy)
			continue;

		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			return -EINVAL;

		ret = freq_qos_add_request(&policy->constraints,
					   &cpu_limit->thermald_max_req, FREQ_QOS_MAX,
					   policy->cpuinfo.max_freq);
		if (ret < 0) {
			pr_err("add cluster %u thermald qos error(%d)\n", cluster_id, ret);
			goto remove_policy;
		}

		cpu_limit->thermald_max_freq = policy->cpuinfo.max_freq;

		ret = freq_qos_add_request(&policy->constraints,
					   &cpu_limit->bcl_max_req, FREQ_QOS_MAX,
					   policy->cpuinfo.max_freq);
		if (ret < 0) {
			pr_err("add cluster %u bcl qos error(%d)\n", cluster_id, ret);
			goto remove_qos;
		}

		cpu_limit->bcl_max_freq = policy->cpuinfo.max_freq;

		ret = sysfs_create_files(&policy->kobj, cpufreq_limit_attrs);
		if (ret) {
			pr_err("create cluster %u sysfs error(%d)\n", cluster_id, ret);
			goto remove_qos;
		}

		cpu_limit->policy = policy;
		cpufreq_cpu_put(policy);
	}

	return 0;

remove_qos:
	sprd_cpufreq_remove_qos(cpu_limit);

remove_policy:
	cpufreq_cpu_put(policy);

	return ret;
}

static void sprd_cpufreq_limit_exit(void)
{
	struct sprd_cpufreq_limit *cpu_limit;
	int i;

	for (i = 0; i < cluster_num; i++) {
		cpu_limit = cpu_limits + i;
		if (!cpu_limit || !cpu_limit->policy)
			continue;

		sprd_cpufreq_remove_qos(cpu_limit);

		sysfs_remove_files(&cpu_limit->policy->kobj, cpufreq_limit_attrs);

		cpu_limit->policy = NULL;
	}
}

static void sprd_devfreq_remove_qos(struct sprd_devfreq_limit *dev_limit)
{
	if (dev_pm_qos_request_active(&dev_limit->bcl_max_req)) {
		dev_pm_qos_remove_request(&dev_limit->bcl_max_req);
		dev_limit->bcl_max_freq = 0;
	}

	if (dev_pm_qos_request_active(&dev_limit->thermald_max_req)) {
		dev_pm_qos_remove_request(&dev_limit->thermald_max_req);
		dev_limit->thermald_max_freq = 0;
	}
}

static int sprd_devfreq_limit_init(struct device *dev)
{
	struct sprd_devfreq_limit *dev_limit;
	struct devfreq *devfreq;
	int i, ret;

	device_num = of_property_count_elems_of_size(dev->of_node, "sprd,devfreq-cells", sizeof(u32));
	if (device_num <= 0)
		return -ENODEV;

	dev_limits = devm_kzalloc(dev, sizeof(*dev_limits) * device_num, GFP_KERNEL);
	if (!dev_limits)
		return -ENOMEM;

	for (i = 0; i < device_num; i++) {
		devfreq = devfreq_get_devfreq_by_phandle(dev, "sprd,devfreq-cells", i);
		if (IS_ERR(devfreq)) {
			pr_err("get devfreq by phandle(%d) has error\n", i);
			return -EPROBE_DEFER;
		}

		dev_limit = dev_limits + i;

		ret = dev_pm_qos_add_request(devfreq->dev.parent, &dev_limit->thermald_max_req,
					     DEV_PM_QOS_MAX_FREQUENCY,
					     PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE);
		if (ret < 0) {
			pr_err("add dev %u thermald qos error(%d)\n", i, ret);
			return ret;
		}

		dev_limit->thermald_max_freq = PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE;

		ret = dev_pm_qos_add_request(devfreq->dev.parent, &dev_limit->bcl_max_req,
					     DEV_PM_QOS_MAX_FREQUENCY,
					     PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE);
		if (ret < 0) {
			pr_err("add dev %u bcl qos error(%d)\n", i, ret);
			goto remove_qos;
		}

		dev_limit->bcl_max_freq = PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE;

		ret = sysfs_create_files(&devfreq->dev.kobj, devfreq_limit_attrs);
		if (ret) {
			pr_err("create (%d) devfreq sysfs error(%d)\n", i, ret);
			goto remove_qos;
		}

		dev_limit->devfreq = devfreq;
	}

	return 0;

remove_qos:
	sprd_devfreq_remove_qos(dev_limit);

	return ret;
}

static void sprd_devfreq_limit_exit(void)
{
	struct sprd_devfreq_limit *dev_limit;
	int i;

	for (i = 0; i < device_num; i++) {
		dev_limit = dev_limits + i;
		if (!dev_limit || !dev_limit->devfreq)
			continue;

		sprd_devfreq_remove_qos(dev_limit);

		sysfs_remove_files(&dev_limit->devfreq->dev.kobj, devfreq_limit_attrs);

		dev_limit->devfreq = NULL;
	}
}

static struct sprd_limit_data device_array[MAX_DEVICE_NUM] = {
	[SPRD_APCPU] = {
		.device_init = sprd_cpufreq_limit_init,
		.device_exit = sprd_cpufreq_limit_exit,
	},
	[SPRD_DEVICE] = {
		.device_init = sprd_devfreq_limit_init,
		.device_exit = sprd_devfreq_limit_exit,
	},
};

static int sprd_freq_limit_driver_remove(struct platform_device *pdev)
{
	struct sprd_limit_data *limit_device;
	int i;

	for (i = 0; i < MAX_DEVICE_NUM; i++) {
		limit_device = device_array + i;
		if (!limit_device || !limit_device->device_exit)
			continue;

		limit_device->device_exit();
	}

	return 0;
}

static int sprd_freq_limit_driver_probe(struct platform_device *pdev)
{
	struct sprd_limit_data *limit_device;
	struct device_node *np;
	unsigned int device_sign = 0;
	int i, ret;

	np = pdev->dev.of_node;
	if (!np) {
		pr_err("freq limit has not found device node\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(np, "sprd,device-sign", &device_sign);
	if (ret) {
		pr_err("freq limit has not defined device sign\n");
		return -EINVAL;
	}

	for (i = 0; i < MAX_DEVICE_NUM; i++) {
		limit_device = device_array + i;
		if (!limit_device) {
			pr_err("limit device(%u) does not exist\n", i);
			return -EINVAL;
		}

		if ((device_sign >> i) & 0x1) {
			if (!limit_device->device_init) {
				pr_err("limit device(%u) definition error\n", i);
				return -EINVAL;
			}

			ret = limit_device->device_init(&pdev->dev);
			if (ret == -EPROBE_DEFER) {
				sprd_freq_limit_driver_remove(pdev);
				return ret;
			} else if (ret) {
				pr_err("limit device(%u) init error(%d)", i, ret);
				continue;
			}
		} else {
			limit_device->device_init = NULL;
			limit_device->device_exit = NULL;
		}
	}

	return 0;
}

static const struct of_device_id sprd_freq_limit_of_match[] = {
	{
		.compatible = "sprd,freq-limit",
	}, {
		/* Sentinel */
	},
};

static struct platform_driver sprd_freq_limit_driver = {
	.driver = {
		.name = "sprd-freq-limit",
		.of_match_table = sprd_freq_limit_of_match,
	},
	.probe = sprd_freq_limit_driver_probe,
	.remove = sprd_freq_limit_driver_remove,
};

static int __init sprd_freq_limit_init(void)
{
	return platform_driver_register(&sprd_freq_limit_driver);
}

static void __exit sprd_freq_limit_exit(void)
{
	platform_driver_unregister(&sprd_freq_limit_driver);
}

late_initcall(sprd_freq_limit_init);
module_exit(sprd_freq_limit_exit);

MODULE_DESCRIPTION("unisoc limit the device freq");
MODULE_AUTHOR("Jinyu Xiong<jinyu.xiong@unisoc.com>");
MODULE_LICENSE("GPL");
