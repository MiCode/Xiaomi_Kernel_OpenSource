#include <linux/init.h>
#include <linux/export.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#include <linux/completion.h>


static int __init mi_memory_sysfs_init(void)
{
	int ret = 0;
	return ret;
}

static void __exit mi_memory_sysfs_exit(void)
{
}

subsys_initcall(mi_memory_sysfs_init);
module_exit(mi_memory_sysfs_exit);

MODULE_DESCRIPTION("mi-memory");
MODULE_AUTHOR("");
MODULE_LICENSE("GPL");
