#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <asm/io.h>

#include <generated/autoconf.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <asm/uaccess.h>
//#include <asm/mach-types.h>

#define MMPROFILE_INTERNAL
#include <linux/mmprofile_internal.h>
/* #pragma GCC optimize ("O0") */
#define MMP_DEVNAME "mmp"

void MMProfileStart(int start)
{
}

void MMProfileEnable(int enable)
{
}

/* Exposed APIs begin */
MMP_Event MMProfileRegisterEvent(MMP_Event parent, const char *name)
{
	return 0;
}
EXPORT_SYMBOL(MMProfileRegisterEvent);

MMP_Event MMProfileFindEvent(MMP_Event parent, const char *name)
{
	return 0;
}
EXPORT_SYMBOL(MMProfileFindEvent);

void MMProfileEnableEvent(MMP_Event event, long enable)
{
}
EXPORT_SYMBOL(MMProfileEnableEvent);

void MMProfileEnableEventRecursive(MMP_Event event, long enable)
{
}
EXPORT_SYMBOL(MMProfileEnableEventRecursive);

long MMProfileQueryEnable(MMP_Event event)
{
	return 0;
}
EXPORT_SYMBOL(MMProfileQueryEnable);

void MMProfileLogEx(MMP_Event event, MMP_LogType type, unsigned long data1, unsigned long data2)
{
}
EXPORT_SYMBOL(MMProfileLogEx);

void MMProfileLog(MMP_Event event, MMP_LogType type)
{
}
EXPORT_SYMBOL(MMProfileLog);

long MMProfileLogMeta(MMP_Event event, MMP_LogType type, MMP_MetaData_t *pMetaData)
{
	return 0;
}
EXPORT_SYMBOL(MMProfileLogMeta);

long MMProfileLogMetaStructure(MMP_Event event, MMP_LogType type,
			      MMP_MetaDataStructure_t *pMetaData)
{
	return 0;
}
EXPORT_SYMBOL(MMProfileLogMetaStructure);

long MMProfileLogMetaStringEx(MMP_Event event, MMP_LogType type, unsigned long data1,
			     unsigned long data2, const char *str)
{
	return 0;
}
EXPORT_SYMBOL(MMProfileLogMetaStringEx);

long MMProfileLogMetaString(MMP_Event event, MMP_LogType type, const char *str)
{
	return 0;
}
EXPORT_SYMBOL(MMProfileLogMetaString);

long MMProfileLogMetaBitmap(MMP_Event event, MMP_LogType type, MMP_MetaDataBitmap_t *pMetaData)
{
	return 0;
}
EXPORT_SYMBOL(MMProfileLogMetaBitmap);

/* Exposed APIs end */

/* Driver specific begin */
/*
static dev_t mmprofile_devno;
static struct cdev *mmprofile_cdev;
static struct class *mmprofile_class = NULL;
*/
static int mmprofile_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int mmprofile_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t mmprofile_read(struct file *file, char __user *data, size_t len, loff_t *ppos)
{
	return 0;
}

static ssize_t mmprofile_write(struct file *file, const char __user *data, size_t len,
			       loff_t *ppos)
{
	return 0;
}

static long mmprofile_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return 0;
}

static int mmprofile_mmap(struct file *file, struct vm_area_struct *vma)
{
	return -EINVAL;
}

struct file_operations mmprofile_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = mmprofile_ioctl,
	.open = mmprofile_open,
	.release = mmprofile_release,
	.read = mmprofile_read,
	.write = mmprofile_write,
	.mmap = mmprofile_mmap,
};

// fix build warning: unused
#if 0
static int mmprofile_probe(struct platform_device *pdev)
{
#if 0
	struct class_device *class_dev = 0;
	int ret = alloc_chrdev_region(&mmprofile_devno, 0, 1, MMP_DEVNAME);

	mmprofile_cdev = cdev_alloc();
	mmprofile_cdev->owner = THIS_MODULE;
	mmprofile_cdev->ops = &mmprofile_fops;
	ret = cdev_add(mmprofile_cdev, mmprofile_devno, 1);
	mmprofile_class = class_create(THIS_MODULE, MMP_DEVNAME);
	class_dev =
	    (struct class_device *)device_create(mmprofile_class, NULL, mmprofile_devno, NULL,
						 MMP_DEVNAME);
#endif
	return 0;
}
#endif

// fix build warning: unused
#if 0
static int mmprofile_remove(struct platform_device *pdev)
{
	return 0;
}
#endif

#if 0
static struct platform_driver mmprofile_driver = {
	.probe = mmprofile_probe,
	.remove = mmprofile_remove,
	.driver = {.name = MMP_DEVNAME}
};

static struct platform_device mmprofile_device = {
	.name = MMP_DEVNAME,
	.id = 0,
};

#endif

static int __init mmprofile_init(void)
{
#if 0
	if (platform_device_register(&mmprofile_device)) {
		return -ENODEV;
	}
	if (platform_driver_register(&mmprofile_driver)) {
		platform_device_unregister(&mmprofile_device);
		return -ENODEV;
	}
#endif
	return 0;
}

static void __exit mmprofile_exit(void)
{
#if 0
	device_destroy(mmprofile_class, mmprofile_devno);
	class_destroy(mmprofile_class);
	cdev_del(mmprofile_cdev);
	unregister_chrdev_region(mmprofile_devno, 1);

	platform_driver_unregister(&mmprofile_driver);
	platform_device_unregister(&mmprofile_device);
#endif
}

/* Driver specific end */

module_init(mmprofile_init);
module_exit(mmprofile_exit);
MODULE_AUTHOR("Tianshu Qiu <tianshu.qiu@mediatek.com>");
MODULE_DESCRIPTION("MMProfile Driver");
MODULE_LICENSE("GPL");
