#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <asm/setup.h>

static int cpumaxfreq_show(struct seq_file *m, void *v)
{
	seq_printf(m, "2.05\n");
	return 0;
}

static int cpumaxfreq_open(struct inode *inode, struct file *file)
{
	return single_open(file, &cpumaxfreq_show, NULL);
}

static const struct file_operations proc_cpumaxfreq_operations = {
	.open 		= cpumaxfreq_open,
	.read 		= seq_read,
	.llseek 	= seq_lseek,
	.release	= seq_release,
};

static int __init bootinfo_init(void)
{
	int res = 0;
	struct proc_dir_entry *file;

	file = proc_create("cpumaxfreq", S_IRUGO, NULL, &proc_cpumaxfreq_operations);
	if (!file)
		res = -ENOMEM;
	return res;
}


static void __exit bootinfo_exit(void)
{
	remove_proc_entry("cpumaxfreq", NULL);
	return;
}

core_initcall(bootinfo_init);
module_exit(bootinfo_exit);
