// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
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

static const struct proc_ops proc_fops = {
	.proc_open  = name ## _proc_open,
	.proc_read  = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
	.proc_write = name ## _proc_write,
};

int proc_init(void)
{
	proc_create("cpuhp_ut", 0664, NULL, &proc_fops);
	return 0;
}

void proc_exit(void)
{
	remove_proc_entry("cpuhp_ut", NULL);
}


