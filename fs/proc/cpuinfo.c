// SPDX-License-Identifier: GPL-2.0
#include <linux/cpufreq.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

char sn_system_serial_num_string[128];
char fuse_state_string[8];

__weak void arch_freq_prepare_all(void)
{
}

extern const struct seq_operations cpuinfo_op;

static int __init serial_num_state_param(char *line)
{
	strlcpy(sn_system_serial_num_string, line, sizeof(sn_system_serial_num_string));
	return 1;
}

__setup("androidboot.cpuid=", serial_num_state_param);

static inline char *sn_system_serial_num(void) {
	if (sn_system_serial_num_string[0])
		return(sn_system_serial_num_string);
	return "0x0";
}

static int serial_num_show(struct seq_file *s, void *p)
{
	seq_printf(s, "%s\n", sn_system_serial_num());
	return 0;
}

static int __init fuse_state_param(char *line)
{
	strlcpy(fuse_state_string, line, sizeof(fuse_state_string));
	return 1;
}

__setup("androidboot.fuse_state=", fuse_state_param);

static inline char *fuse_state(void) {
	if (fuse_state_string[0])
		return(fuse_state_string);
	return "0x0";
}

static int fuse_state_show(struct seq_file *s, void *p)
{
	seq_printf(s, "%s\n", fuse_state());
	return 0;
}

static int cpuinfo_open(struct inode *inode, struct file *file)
{
	arch_freq_prepare_all();
	return seq_open(file, &cpuinfo_op);
}

static int serial_num_open(struct inode *inode, struct file *file)
{
	return single_open(file, serial_num_show, NULL);
}

static int fuse_state_open(struct inode *inode, struct file *file)
{
	return single_open(file, fuse_state_show, NULL);
}

static const struct file_operations proc_cpuinfo_operations = {
	.open		= cpuinfo_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static const struct file_operations proc_serial_num_fops = {
	.open		= serial_num_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations proc_fuse_state_fops = {
	.open		= fuse_state_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_cpuinfo_init(void)
{
	proc_create("cpuinfo", 0, NULL, &proc_cpuinfo_operations);
	proc_create("serial_num", 0, NULL, &proc_serial_num_fops);
	proc_create("fuse_state", 0, NULL, &proc_fuse_state_fops);
	return 0;
}
fs_initcall(proc_cpuinfo_init);
