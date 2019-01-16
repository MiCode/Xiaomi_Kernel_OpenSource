#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/topology.h>

#define MAX_LONG_SIZE 24

struct kobject *cputopo_glb_kobj;

/*
 * nr_clusters attribute
 */
static ssize_t nr_clusters_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_LONG_SIZE, "%u\n", arch_get_nr_clusters());
}
static struct kobj_attribute nr_clusters_attr = __ATTR_RO(nr_clusters);

/*
 * is_big_little attribute
 */
static ssize_t is_big_little_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_LONG_SIZE, "%u\n", arch_is_big_little());
}
static struct kobj_attribute is_big_little_attr = __ATTR_RO(is_big_little);

/*
 * is_multi_cluster attribute
 */
static ssize_t is_multi_cluster_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_LONG_SIZE, "%u\n", arch_is_multi_cluster());
}
static struct kobj_attribute is_multi_cluster_attr = __ATTR_RO(is_multi_cluster);

/*
 * little_cpumask attribute
 */
static ssize_t little_cpumask_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct cpumask big, little;

	arch_get_big_little_cpus(&big, &little);
	return snprintf(buf, MAX_LONG_SIZE, "%02lx\n", *cpumask_bits(&little));
}
static struct kobj_attribute little_cpumask_attr = __ATTR_RO(little_cpumask);

/*
 * big_cpumask attribute
 */
static ssize_t big_cpumask_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct cpumask big, little;

	arch_get_big_little_cpus(&big, &little);
	return snprintf(buf, MAX_LONG_SIZE, "%02lx\n", *cpumask_bits(&big));
}
static struct kobj_attribute big_cpumask_attr = __ATTR_RO(big_cpumask);


/*
 * glbinfo attribute
 */
static ssize_t glbinfo_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int len = 0;
	struct cpumask big, little;

	arch_get_big_little_cpus(&big, &little);
	len += snprintf(buf + len, PAGE_SIZE - len - 1, "big/little arch: %s\n",
					arch_is_big_little() ? "yes" : "no");
	len += snprintf(buf + len, PAGE_SIZE - len - 1, "big/little cpumask:%0lx/%0lx\n",
					*cpumask_bits(&big), *cpumask_bits(&little));
	len += snprintf(buf + len, PAGE_SIZE - len - 1, "nr_cups: %u\n",
					nr_cpu_ids);
	len += snprintf(buf + len, PAGE_SIZE - len - 1, "nr_clusters: %u\n",
					arch_get_nr_clusters());

	return len;
}
static struct kobj_attribute glbinfo_attr = __ATTR_RO(glbinfo);



static struct attribute *cputopo_attrs[] = {
	&nr_clusters_attr.attr,
	&is_big_little_attr.attr,
	&is_multi_cluster_attr.attr,
	&little_cpumask_attr.attr,
	&big_cpumask_attr.attr,
	&glbinfo_attr.attr,
	NULL,
};

static struct attribute_group cputopo_attr_group = {
	.attrs = cputopo_attrs,
};

static int init_cputopo_attribs(void)
{
	int err;

	/* Create /sys/devices/system/cpu/cputopo/... */
	cputopo_glb_kobj = kobject_create_and_add("cputopo", &cpu_subsys.dev_root->kobj);
	if (!cputopo_glb_kobj)
		return -ENOMEM;

	err = sysfs_create_group(cputopo_glb_kobj, &cputopo_attr_group);
	if (err)
		kobject_put(cputopo_glb_kobj);

	return err;
}

static int __init cputopo_info_init(void)
{
	int ret = 0;

	ret = init_cputopo_attribs();

	return ret;
}

core_initcall(cputopo_info_init);
