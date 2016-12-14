/*
 * Copyright (C) 2016 XiaoMi, Inc.
 */
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/cpufreq.h>
#include <linux/seq_file.h>

static struct proc_dir_entry *entry;

static int cpumaxfreq_read(struct seq_file *m, void *v)
{
	int cpu, ret;
	struct cpufreq_policy policy;
	/*
	 * Using static variable ensures that we won't get a smaller value
	 * in next reading until system rebooting.
	 */
	static unsigned int cpumaxfreq;

	for_each_possible_cpu(cpu) {
		if (cpufreq_get_policy(&policy, cpu))
			continue;
		if (policy.cpuinfo.max_freq > cpumaxfreq)
			cpumaxfreq = policy.cpuinfo.max_freq;
	}

	pr_debug("cpumaxfreq: %u kHz\n", cpumaxfreq);

	ret = cpumaxfreq / 1000 + 5;
	seq_printf(m, "%u.%u\n", ret / 1000, ret % 1000 / 10);

	return 0;
}

static int cpumaxfreq_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, cpumaxfreq_read, NULL);
}

static const struct file_operations cpumaxfreq_fops = {
	.open = cpumaxfreq_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init cpumaxfreq_init(void)
{
	entry = proc_create("cpumaxfreq", 0444 /* read only */ ,
			    NULL /* parent dir */ , &cpumaxfreq_fops);
	return !entry;
}

postcore_initcall(cpumaxfreq_init);

static void __exit cpumaxfreq_exit(void)
{
	proc_remove(entry);
}

module_exit(cpumaxfreq_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION
    ("Return the max freqency supported by any core of the processor");
