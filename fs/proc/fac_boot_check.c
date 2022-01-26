#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <asm/memory.h>


static char *fac_boot_check_get(void)
{
#ifdef WT_COMPILE_FACTORY_VERSION
	char *check = "1";
#else
	char *check = "0";
#endif
	return check;
}

static int fac_boot_check_show(struct seq_file *m, void *v)
{
	seq_printf(m,"%s\n",fac_boot_check_get());
	return 0;

}

static int factory_boot_check_porc_open(struct inode *inode, struct file *file)
{
	return single_open(file, fac_boot_check_show, NULL);
}

static const struct file_operations factory_boot_check_proc_fops = {
	.owner      = THIS_MODULE,
	.open		= factory_boot_check_porc_open,
	.read		= seq_read,
};

static int __init proc_factory_boot_check_init(void)
{
	proc_create("factory_mode_boot_check", 0644, NULL, &factory_boot_check_proc_fops);
	return 0;
}
fs_initcall(proc_factory_boot_check_init);
