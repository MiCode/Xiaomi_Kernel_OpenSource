/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/cpu.h>
#include <linux/random.h>

static unsigned char random_cpuhp_stress[CONFIG_NR_CPUS] ____cacheline_aligned;

int random_cpuhp_ut_init(void)
{
	get_random_bytes(random_cpuhp_stress, CONFIG_NR_CPUS);
	return 0;
}

static ssize_t proc_write(struct file *f, const char *data, size_t len,
	loff_t *offset)
{
	int cpu, online_cpu = 0, i;
	static unsigned int infinity;

	if (data[0] == '0') {
		if (len != 2) {
			infinity = 0;
			return -EINVAL;
		}
		infinity = 1;
	} else if (data[0] == '1') {
		if (len != 2) {
			infinity = 0;
			return -EINVAL;
		}
		infinity = 1;
	} else {
		infinity = 0;
	}

	if (data[0] == '0') {
		if (!cpu_online(0) && cpu_is_hotpluggable(0))
			cpu_up(0);
		while (infinity) {
			for (i = 1; i < CONFIG_NR_CPUS; i++) {
				if (!cpu_online(i) && cpu_is_hotpluggable(i))
					cpu_up(i);
			}
			for (i = CONFIG_NR_CPUS - 1; i > 0; i--) {
				if (cpu_online(i) && cpu_is_hotpluggable(i))
					cpu_down(i);
			}
		}
	} else if (data[0] == '1') {
		while (infinity) {
			random_cpuhp_ut_init();
			for (i = 0; i < CONFIG_NR_CPUS; i++)
				random_cpuhp_stress[i] =
				random_cpuhp_stress[i] % CONFIG_NR_CPUS;

			for (i = 0; i < CONFIG_NR_CPUS - 2; i++) {
				if (cpu_online(random_cpuhp_stress[i]) &&
				cpu_is_hotpluggable(random_cpuhp_stress[i]))
					cpu_down(random_cpuhp_stress[i]);
			}

			for (i = CONFIG_NR_CPUS - 2; i >= 0; i--) {
				if (!cpu_online(random_cpuhp_stress[i]) &&
				cpu_is_hotpluggable(random_cpuhp_stress[i]))
					cpu_up(random_cpuhp_stress[i]);
			}
		};
	}

	for_each_online_cpu(cpu)
		online_cpu = cpu;
	WARN_ON(online_cpu < 0);

	return len;
}

static int proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "num_online_cpus() = %d\n", num_online_cpus());
	seq_printf(m, "num_possible_cpus() = %d\n", num_possible_cpus());
	seq_printf(m, "num_present_cpus() = %d\n", num_present_cpus());
	seq_printf(m, "num_active_cpus() = %d\n", num_active_cpus());

	seq_puts(m, "Usage:\n");
	seq_puts(m,
		"   CPU0 hotplug secondary cores: echo 0 > /proc/cpuhp_ut\n");
	seq_puts(m,
		"   Random CPU hotplug:           echo 1 > /proc/cpuhp_ut\n");
	seq_puts(m,
		"   Stop all CPU hotplug test:    echo X > /proc/cpuhp_ut\n");

	return 0;
}

static int proc_open(struct inode *inode, struct  file *file)
{
	int ret;

	ret = single_open(file, proc_show, NULL);

	return ret;
}

static const struct file_operations proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_open,
	.read = seq_read,
	.write = proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init proc_init(void)
{
	proc_create("cpuhp_ut", 0664, NULL, &proc_fops);
	return 0;
}

static void __exit proc_exit(void)
{
	remove_proc_entry("cpuhp_ut", NULL);
}

MODULE_LICENSE("GPL");
module_init(proc_init);
module_exit(proc_exit);
