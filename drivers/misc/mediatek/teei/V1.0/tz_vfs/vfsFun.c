#include<linux/kernel.h>
#include <linux/platform_device.h>
#include<linux/module.h>
#include<linux/types.h>
#include<linux/fs.h>
#include<linux/errno.h>
#include<linux/mm.h>
#include<linux/sched.h>
#include<linux/init.h>
#include<linux/cdev.h>
#include<asm/io.h>
#include<asm/uaccess.h>
#include<linux/semaphore.h>
#include<linux/slab.h>
#include"TEEI.h"
#include"teei_id.h"

#define VFS_SIZE	0x80000
#define MEM_CLEAR	0x1
#define VFS_MAJOR	253

static int vfs_major = VFS_MAJOR;
static struct class *driver_class;
static dev_t devno;

struct vfs_dev {
	struct cdev cdev;
	unsigned char mem[VFS_SIZE];
	struct semaphore sem;
};

extern struct completion global_down_lock;

#ifdef VFS_RDWR_SEM
struct semaphore VFS_rd_sem;
EXPORT_SYMBOL_GPL(VFS_rd_sem);

struct semaphore VFS_wr_sem;
EXPORT_SYMBOL_GPL(VFS_wr_sem);
#else

DECLARE_COMPLETION(VFS_rd_comp);
EXPORT_SYMBOL_GPL(VFS_rd_comp);

DECLARE_COMPLETION(VFS_wr_comp);
EXPORT_SYMBOL_GPL(VFS_wr_comp);
#endif

struct vfs_dev *vfs_devp = NULL;

int tz_vfs_open(struct inode *inode, struct file *filp)
{
	if (vfs_devp == NULL)
		return -EINVAL;

	if (filp == NULL)
		return -EINVAL;

	if (strcmp("teei_daemon", current->comm) != 0)
		return -EINVAL;

	filp->private_data = vfs_devp;
	return 0;
}

int tz_vfs_release(struct inode *inode, struct file *filp)
{
	filp->private_data = NULL;
	return 0;
}

static ssize_t tz_vfs_read(struct file *filp, char __user *buf,
			size_t size, loff_t *ppos)
{
	struct TEEI_vfs_command *vfs_p = NULL;
	int length = 0;
	int ret = 0;

	if (buf == NULL)
		return -EINVAL;

	if (daulOS_VFS_share_mem == NULL)
		return -EINVAL;

	if ((0 > size) || (size > VFS_SIZE))
		return -EINVAL;

	/*pr_debug("read begin cpu[%d]\n",cpu_id);*/
#ifdef VFS_RDWR_SEM
	down_interruptible(&VFS_rd_sem);
#else
	ret = wait_for_completion_interruptible(&VFS_rd_comp);

	if (ret == -ERESTARTSYS) {
		pr_debug("[%s][%d] ----------------wait_for_completion_interruptible_timeout interrupt----------------------- \n", __func__, __LINE__);
		complete(&global_down_lock);
		return ret;
	}
#endif

	vfs_p = (struct TEEI_vfs_command *)daulOS_VFS_share_mem;

#if 1

	if (vfs_p->cmd_size > size)
		length = size;
	else
		length = vfs_p->cmd_size;

#endif

	length = size;

	if (copy_to_user(buf, (void *)vfs_p, length))
		ret = -EFAULT;
	else
		ret = length;
	return ret;
}

static ssize_t tz_vfs_write(struct file *filp, const char __user *buf,
			size_t size, loff_t *ppos)
{
	if (buf == NULL)
		return -EINVAL;

	if (daulOS_VFS_share_mem == NULL)
		return -EINVAL;

	if ((0 > size) || (size > VFS_SIZE))
		return -EINVAL;

	/*pr_debug("write begin cpu_id[%d]\n",cpu_id);*/
	if (copy_from_user((void *)daulOS_VFS_share_mem, buf, size))
		return -EFAULT;

	Flush_Dcache_By_Area((unsigned long)daulOS_VFS_share_mem, (unsigned long)daulOS_VFS_share_mem + size);

#ifdef VFS_RDWR_SEM
	up(&VFS_wr_sem);
#else
	complete(&VFS_wr_comp);
#endif
	return 0;
}

static const struct file_operations vfs_fops = {
	.owner =		THIS_MODULE,
	.read =			tz_vfs_read,
	.write =		tz_vfs_write,
	.open =			tz_vfs_open,
	.release =		tz_vfs_release,
};

static void vfs_setup_cdev(struct vfs_dev *dev, int index)
{
	int err = 0;
	int devno = MKDEV(vfs_major, index);

	cdev_init(&dev->cdev, &vfs_fops);
	dev->cdev.owner = vfs_fops.owner;
	err = cdev_add(&dev->cdev, devno, 1);

	if (err)
		pr_err("Error %d adding socket %d.\n", err, index);
}




int vfs_init(void)
{
	int result = 0;
	struct device *class_dev = NULL;
	devno = MKDEV(vfs_major, 0);

	result = alloc_chrdev_region(&devno, 0, 1, "tz_vfs");
	vfs_major = MAJOR(devno);

	if (result < 0)
		return result;

	driver_class = class_create(THIS_MODULE, "tz_vfs");

	if (IS_ERR(driver_class)) {
		result = -ENOMEM;
		pr_err("class_create failed %d.\n", result);
		goto unregister_chrdev_region;
	}

	class_dev = device_create(driver_class, NULL, devno, NULL, "tz_vfs");

	if (!class_dev) {
		result = -ENOMEM;
		pr_err("class_device_create failed %d.\n", result);
		goto class_destroy;
	}

	vfs_devp = kmalloc(sizeof(struct vfs_dev), GFP_KERNEL);

	if (vfs_devp == NULL) {
		result = -ENOMEM;
		goto class_device_destroy;
	}

	memset(vfs_devp, 0, sizeof(struct vfs_dev));
	vfs_setup_cdev(vfs_devp, 0);
	sema_init(&vfs_devp->sem, 1);

#ifdef VFS_RDWR_SEM
	sema_init(&VFS_rd_sem, 0);
	sema_init(&VFS_wr_sem, 0);
#endif
	goto return_fn;

class_device_destroy:
	device_destroy(driver_class, devno);
class_destroy:
	class_destroy(driver_class);
unregister_chrdev_region:
	unregister_chrdev_region(devno, 1);
return_fn:
	return result;
}

void vfs_exit(void)
{
	device_destroy(driver_class, devno);
	class_destroy(driver_class);
	cdev_del(&vfs_devp->cdev);
	kfree(vfs_devp);
	unregister_chrdev_region(MKDEV(vfs_major, 0), 1);
}

MODULE_AUTHOR("MicroTrust");
MODULE_LICENSE("Dual BSD/GPL");

module_param(vfs_major, int, S_IRUGO);

module_init(vfs_init);
module_exit(vfs_exit);
