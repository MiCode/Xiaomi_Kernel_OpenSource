#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "sec_boot_lib.h"

static int secureboot_proc_show(struct seq_file *m, void *v)
{
	int secureboot_status = 0;
	secureboot_status = sec_schip_enabled();
	seq_printf(m, "%d\n", secureboot_status);
	return 0;
}

static int secureboot_open(struct inode *inode, struct file *file)
{
	return single_open(file, secureboot_proc_show, NULL);
}

static const struct file_operations proc_secureboot_operations = {
	.open		= secureboot_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int __init proc_secureboot_init(void)
{
	proc_create("secureboot", 0644, NULL, &proc_secureboot_operations);
	return 0;
}
fs_initcall(proc_secureboot_init);