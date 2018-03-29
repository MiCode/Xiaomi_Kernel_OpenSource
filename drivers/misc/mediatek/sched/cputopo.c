#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/topology.h>
#include <linux/seq_file.h>

#define MAX_LONG_SIZE 24

struct kobject *cputopo_glb_kobj;

/* tasks all */
static int tasks_all_show(struct seq_file *m, void *v)
{
	struct task_struct *g, *p;
	unsigned long flags;
	int cpu;
#if 0
	u64 localtime = local_clock();
#endif

#if 0
	seq_printf(m, "%5llu.%06llu\n", localtime/NSEC_PER_SEC, localtime/NSEC_PER_MSEC);
#endif
	read_lock_irqsave(&tasklist_lock, flags);
	for_each_online_cpu(cpu) {
		seq_printf(m, "cpu%d: ", cpu);
		do_each_thread(g, p) {
			if (p->flags == PF_KTHREAD || !p->mm || task_cpu(p) != cpu)
				continue;
			seq_printf(m, "%d ", task_pid_nr(p));
		} while_each_thread(g, p);
		seq_puts(m, "\n");
	}
	read_unlock_irqrestore(&tasklist_lock, flags);

	return 0;
}

static int tasks_all_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, tasks_all_show, NULL);
}

static const struct file_operations tasks_all_fops = {
	.open		= tasks_all_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/* tasks in runqueue */
static int tasks_rq_show(struct seq_file *m, void *v)
{
	struct task_struct *g, *p;
	unsigned long flags;
	int cpu;
#if 0
	u64 localtime = local_clock();
#endif

#if 0
	seq_printf(m, "%5llu.%06llu\n", localtime/NSEC_PER_SEC, localtime/NSEC_PER_MSEC);
#endif
	read_lock_irqsave(&tasklist_lock, flags);
	for_each_online_cpu(cpu) {
		seq_printf(m, "cpu%d: ", cpu);
		do_each_thread(g, p) {
			if (!p->on_rq || p->flags == PF_KTHREAD || !p->mm || task_cpu(p) != cpu)
				continue;
			seq_printf(m, "%d ", task_pid_nr(p));
		} while_each_thread(g, p);
		seq_puts(m, "\n");
	}
	read_unlock_irqrestore(&tasklist_lock, flags);

	return 0;
}

static int tasks_rq_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, tasks_rq_show, NULL);
}

static const struct file_operations tasks_rq_fops = {
	.open		= tasks_rq_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

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
	return snprintf(buf, MAX_LONG_SIZE, "%u\n", !arch_is_smp());
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
 * glbinfo attribute
 */
static ssize_t glbinfo_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int i, len = 0;
	struct cpumask cpus;

	len += snprintf(buf + len, PAGE_SIZE - len - 1, "big/little arch: %s\n",
					arch_is_smp() ? "no" : "yes");
	len += snprintf(buf + len, PAGE_SIZE - len - 1, "nr_cups: %u\n",
					nr_cpu_ids);
	len += snprintf(buf + len, PAGE_SIZE - len - 1, "nr_clusters: %u\n",
					arch_get_nr_clusters());
	for (i = 0; i < arch_get_nr_clusters(); i++) {
		arch_get_cluster_cpus(&cpus, i);
		len += snprintf(buf + len, PAGE_SIZE - len - 1, "cluster%d: %0lx\n", i, *cpumask_bits(&cpus));
	}

	return len;
}
static struct kobj_attribute glbinfo_attr = __ATTR_RO(glbinfo);

/*
 * cpus_per_cluster attribute
 */
static ssize_t cpus_per_cluster_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int i, len = 0;
	struct cpumask cpus;

	for (i = 0; i < arch_get_nr_clusters(); i++) {
		arch_get_cluster_cpus(&cpus, i);
		len += snprintf(buf + len, PAGE_SIZE - len - 1, "cluster%d: %0lx\n", i, *cpumask_bits(&cpus));
	}

	return len;
}
static struct kobj_attribute cpus_per_cluster_attr = __ATTR_RO(cpus_per_cluster);

static struct attribute *cputopo_attrs[] = {
	&nr_clusters_attr.attr,
	&is_big_little_attr.attr,
	&is_multi_cluster_attr.attr,
	&glbinfo_attr.attr,
	&cpus_per_cluster_attr.attr,
	NULL,
};

static struct attribute_group cputopo_attr_group = {
	.attrs = cputopo_attrs,
};

static int init_cputopo_attribs(void)
{
	int err;
	struct proc_dir_entry *pe;

	/* Create /sys/devices/system/cpu/cputopo/... */
	cputopo_glb_kobj = kobject_create_and_add("cputopo", &cpu_subsys.dev_root->kobj);
	if (!cputopo_glb_kobj)
		return -ENOMEM;

	err = sysfs_create_group(cputopo_glb_kobj, &cputopo_attr_group);
	if (err)
		kobject_put(cputopo_glb_kobj);

	pe = proc_create("tasks_all", 0444, NULL, &tasks_all_fops);
	if (!pe)
		return -ENOMEM;

	pe = proc_create("tasks_rq", 0444, NULL, &tasks_rq_fops);
	if (!pe)
		return -ENOMEM;

	return err;
}

static int __init cputopo_info_init(void)
{
	int ret = 0;

	ret = init_cputopo_attribs();

	return ret;
}

core_initcall(cputopo_info_init);
