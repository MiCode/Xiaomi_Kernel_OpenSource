#include <linux/init.h>
#include <linux/export.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include "mi_memory_sysfs.h"

static struct memory_info *mi_memory;

static const struct attribute_group *memory_sysfs_groups[] = {
	&ufshcd_sysfs_group,
	&dram_sysfs_group,
	NULL,
};

struct file_operations mem_ops = {
	.owner  = THIS_MODULE,
};

static int __init mi_memory_sysfs_init(void)
{
	int ret = 0;

	mi_memory = kzalloc(sizeof(struct memory_info), GFP_KERNEL);

	mi_memory->mem_class = class_create(THIS_MODULE, MI_MEMORY_CLASS);
	if (IS_ERR(mi_memory->mem_class)) {
		ret = PTR_ERR(mi_memory->mem_class);
		pr_err("%s: mi memory info class creation failed (err = %d)\n", __func__, ret);
		goto out;
	}

	mi_memory->major = register_chrdev(MEMORYDEV_MAJOR, MI_MEMORY_MODULE, &mem_ops);
	if (mi_memory->major < 0) {
		ret = mi_memory->major;
		pr_err("%s: mi memory info chrdev creation failed (err = %d)\n", __func__, ret);
		goto class_unreg;
	}

	mi_memory->mem_dev = device_create(mi_memory->mem_class, NULL, MKDEV(mi_memory->major, MEMORYDEV_MINOR), NULL, MI_MEMORY_DEVICE);
	if (IS_ERR(mi_memory->mem_dev)) {
		ret = -ENODEV;
		pr_err("%s: mi memory info device creation failed (err = %d)\n", __func__, ret);
		goto chrdev_unreg;
	}

	ret = sysfs_create_groups(&mi_memory->mem_dev->kobj, memory_sysfs_groups);
	if (ret) {
		pr_err("%s: sysfs groups creation failed (err = %d)\n", __func__, ret);
		goto memdev_unreg;
	}

	return ret;

memdev_unreg:
	device_destroy(mi_memory->mem_class, MKDEV(mi_memory->major,MEMORYDEV_MINOR));
chrdev_unreg:
	unregister_chrdev(MEMORYDEV_MAJOR, MI_MEMORY_MODULE);
class_unreg:
	class_destroy(mi_memory->mem_class);
out:
	kfree(mi_memory);
	return ret;
}

static void __exit mi_memory_sysfs_exit(void)
{
	sysfs_remove_groups(&mi_memory->mem_dev->kobj, memory_sysfs_groups);
	device_destroy(mi_memory->mem_class, MKDEV(mi_memory->major,MEMORYDEV_MINOR));
	unregister_chrdev(MEMORYDEV_MAJOR, MI_MEMORY_MODULE);
	class_destroy(mi_memory->mem_class);
	kfree(mi_memory);
}

subsys_initcall(mi_memory_sysfs_init);
module_exit(mi_memory_sysfs_exit);

MODULE_DESCRIPTION("Interface for xiaomi memory");
MODULE_AUTHOR("Venco <duwenchao@xiaomi.com>");
MODULE_LICENSE("GPL");
