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
#include <asm/cacheflush.h>
#include<linux/semaphore.h>
#include<linux/slab.h>
#include "../tz_driver/include/teei_id.h"
#include "../tz_driver/include/tz_service.h"
#include "../tz_driver/include/nt_smc_call.h"

#include "teei_fp.h"

/* #define FP_DEBUG */

#define MICROTRUST_FP_SIZE	0x80000
#define CMD_MEM_CLEAR	_IO(0x775A777E, 0x1)
#define CMD_FP_CMD      _IO(0x775A777E, 0x2)
#define CMD_GATEKEEPER_CMD	_IO(0x775A777E, 0x3)
#define CMD_LOAD_TEE			_IO(0x775A777E, 0x4)
#define FP_MAJOR	254
#define SHMEM_ENABLE    0
#define SHMEM_DISABLE   1
#define DEV_NAME "teei_fp"
#define FP_DRIVER_ID 100
#define GK_DRIVER_ID 120
static int fp_major = FP_MAJOR;
static struct class *driver_class;
static dev_t devno;
struct semaphore fp_api_lock;
struct fp_dev {
	struct cdev cdev;
	unsigned char mem[MICROTRUST_FP_SIZE];
	struct semaphore sem;
};

struct semaphore daulOS_rd_sem;
struct semaphore daulOS_wr_sem;
EXPORT_SYMBOL_GPL(daulOS_rd_sem);
EXPORT_SYMBOL_GPL(daulOS_wr_sem);

extern char *fp_buff_addr;
extern unsigned long gatekeeper_buff_addr;
/*extern unsigned int daulOS_shmem_flags;*/

struct fp_dev *fp_devp;
extern struct semaphore boot_decryto_lock;

extern unsigned long teei_config_flag;

DECLARE_WAIT_QUEUE_HEAD(__fp_open_wq);
int wait_teei_config_flag = 1;

int fp_open(struct inode *inode, struct file *filp)
{
	if (wait_teei_config_flag == 1) {
		int ret;
		pr_debug("[I]%s : Teei_config_flag = %lu\n", __func__, teei_config_flag);
		ret = wait_event_timeout(__fp_open_wq, (teei_config_flag == 1), msecs_to_jiffies(1000*10));

		if (ret == 0) {
			pr_err("[E]%s : Tees's loading is not finished, and has already waited 10s.\n", __func__);
			return -1;
		}

		if (ret < 0) {
			pr_err("[E]%s : Wait_event_timeout error.\n", __func__);
			return -1;
		}

		pr_debug("[I]%s : Load tees finished, and wait for %u msecs\n", __func__, (1000*10-jiffies_to_msecs(ret)));
		wait_teei_config_flag = 0;
	}

#ifdef FP_DEBUG
	pr_debug("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!say hello  from fp!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
#endif
	filp->private_data = fp_devp;

	return 0;
}

int fp_release(struct inode *inode, struct file *filp)
{
	filp->private_data = NULL;
	return 0;
}

static long fp_ioctl(struct file *filp, unsigned cmd, unsigned long arg)
{
	unsigned int args_len = 0;
	unsigned int fp_cid = 0xFF;
	unsigned int fp_fid = 0xFF;
	down(&fp_api_lock);
#ifdef FP_DEBUG
	pr_debug("##################################\n");
	pr_debug("fp ioctl received received cmd is: %x arg is %x\n", cmd, (unsigned int)arg);
	pr_debug("CMD_MEM_CLEAR is: %x CMD_FP_CMD is %x \n", CMD_MEM_CLEAR, CMD_FP_CMD);
#endif

	switch (cmd) {
	case CMD_MEM_CLEAR:
		pr_debug(KERN_INFO "CMD MEM CLEAR. \n");
		break;

	case CMD_FP_CMD:
		/*TODO compute args length*/
		/*[11-15] is the length of data*/
		args_len = *((unsigned int *)(arg + 12));
		/*[0-3] is cmd id*/
		fp_cid = *((unsigned int *)(arg));
		/*[4-7] is fuction id*/
		fp_fid = *((unsigned int *)(arg + 4));
#ifdef FP_DEBUG
		pr_debug("invoke fp cmd CMD_FP_CMD: arg's address is %x, args's length %d\n", (unsigned int)arg, args_len);
		pr_debug("invoke fp cmd fp_cid is %d fp_fid is %d \n", fp_cid, fp_fid);
#endif

		if (!fp_buff_addr) {
			pr_err("fp_buiff_addr is invalid!. \n");
			up(&fp_api_lock);
			return -EFAULT;
		}

		memset((void *)fp_buff_addr, 0, args_len + 16);

		if (copy_from_user((void *)fp_buff_addr, (void *)arg, args_len + 16)) {
			pr_err(KERN_INFO "copy from user failed. \n");
			up(&fp_api_lock);
			return -EFAULT;
		}

		Flush_Dcache_By_Area((unsigned long)fp_buff_addr, (unsigned long)fp_buff_addr + MICROTRUST_FP_SIZE);
		/*send command data to TEEI*/
		send_fp_command(FP_DRIVER_ID);
#ifdef FP_DEBUG
		pr_debug("back from TEEI try copy share mem to user \n");
		pr_debug("result in share memory %d  \n", *((unsigned int *)fp_buff_addr));
		pr_debug("[%s][%d] fp_buff_addr 88 - 91 = %d\n", __func__, args_len, *((unsigned int *)(fp_buff_addr + 88)));
#endif

		if (copy_to_user((void *)arg, fp_buff_addr, args_len + 16)) {
			pr_err("copy from user failed. \n");
			up(&fp_api_lock);
			return -EFAULT;
		}

#ifdef FP_DEBUG
		pr_debug("result after copy %d  \n", *((unsigned int *)arg));
		pr_debug("invoke fp cmd end. \n");
#endif
		break;

	case CMD_GATEKEEPER_CMD:
#ifdef FP_DEBUG
		pr_debug("case CMD_GATEKEEPER_CMD\n");
#endif

		if (!gatekeeper_buff_addr) {
			pr_err("gatekeeper_buff_addr is invalid!. \n");
			up(&fp_api_lock);
			return -EFAULT;
		}

#ifdef FP_DEBUG
		pr_debug("varify gatekeeper_buff_addr  ok\n");
		pr_debug("the value of gatekeeper_buff_addr is %lu\n", gatekeeper_buff_addr);
#endif
		memset((void *)gatekeeper_buff_addr, 0, 0x1000);
#ifdef FP_DEBUG
		pr_debug("memset  ok\n");
#endif

		if (copy_from_user((void *)gatekeeper_buff_addr, (void *)arg, 0x1000)) {
			pr_err(KERN_INFO "copy from user failed. \n");
			up(&fp_api_lock);
			return -EFAULT;
		}

#ifdef FP_DEBUG
		pr_debug("copy_from_user  ok\n");
#endif
		Flush_Dcache_By_Area((unsigned long)gatekeeper_buff_addr,
					(unsigned long)gatekeeper_buff_addr + 0x1000);
#ifdef FP_DEBUG
		pr_debug("Flush_Dcache_By_Area  ok\n");
#endif
		send_gatekeeper_command(GK_DRIVER_ID);
#ifdef FP_DEBUG
		pr_debug("send_gatekeeper_command  ok\n");
#endif

		if (copy_to_user((void *)arg, (void *)gatekeeper_buff_addr, 0x1000)) {
			pr_err("copy from user failed. \n");
			up(&fp_api_lock);
			return -EFAULT;
		}

#ifdef FP_DEBUG
		pr_debug("copy_to_user  ok\n");
#endif
		break;

	case CMD_LOAD_TEE:
#ifdef FP_DEBUG
		pr_debug("case CMD_LOAD_TEE\n");
#endif
		up(&boot_decryto_lock);
		break;

	default:
		up(&fp_api_lock);
		return -EINVAL;
	}

	up(&fp_api_lock);
	return 0;
}

static ssize_t fp_read(struct file *filp, char __user *buf,
			size_t size, loff_t *ppos)
{
	int ret = 0;
	return ret;
}

static ssize_t fp_write(struct file *filp, const char __user *buf,
			size_t size, loff_t *ppos)
{
	return 0;
}

static loff_t fp_llseek(struct file *filp, loff_t offset, int orig)
{
	return 0;
}
static const struct file_operations fp_fops = {
	.owner = THIS_MODULE,
	.llseek = fp_llseek,
	.read = fp_read,
	.write = fp_write,
	.unlocked_ioctl = fp_ioctl,
	.compat_ioctl = fp_ioctl,
	.open = fp_open,
	.release = fp_release,
};

static void fp_setup_cdev(struct fp_dev *dev, int index)
{
	int err = 0;
	int devno = MKDEV(fp_major, index);

	cdev_init(&dev->cdev, &fp_fops);
	dev->cdev.owner = fp_fops.owner;
	err = cdev_add(&dev->cdev, devno, 1);

	if (err) {
		pr_err(KERN_NOTICE "Error %d adding fp %d.\n", err, index);
	}
}

int fp_init(void)
{
	int result = 0;
	struct device *class_dev = NULL;
	devno = MKDEV(fp_major, 0);

	result = alloc_chrdev_region(&devno, 0, 1, DEV_NAME);
	fp_major = MAJOR(devno);
	sema_init(&(fp_api_lock), 1);

	if (result < 0) {
		return result;
	}

	driver_class = NULL;
	driver_class = class_create(THIS_MODULE, DEV_NAME);

	if (IS_ERR(driver_class)) {
		result = -ENOMEM;
		pr_err("class_create failed %d.\n", result);
		goto unregister_chrdev_region;
	}

	class_dev = device_create(driver_class, NULL, devno, NULL, DEV_NAME);

	if (!class_dev) {
		result = -ENOMEM;
		pr_err("class_device_create failed %d.\n", result);
		goto class_destroy;
	}

	fp_devp = NULL;
	fp_devp = kmalloc(sizeof(struct fp_dev), GFP_KERNEL);

	if (fp_devp == NULL) {
		result = -ENOMEM;
		goto class_device_destroy;
	}

	memset(fp_devp, 0, sizeof(struct fp_dev));
	fp_setup_cdev(fp_devp, 0);
	sema_init(&fp_devp->sem, 1);
	sema_init(&daulOS_rd_sem, 0);
	sema_init(&daulOS_wr_sem, 0);

	pr_debug("[%s][%d]create the teei_fp device node successfully!\n", __func__, __LINE__);
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

void fp_exit(void)
{
	device_destroy(driver_class, devno);
	class_destroy(driver_class);
	cdev_del(&fp_devp->cdev);
	kfree(fp_devp);
	unregister_chrdev_region(MKDEV(fp_major, 0), 1);
}

MODULE_AUTHOR("Microtrust");
MODULE_LICENSE("Dual BSD/GPL");

module_param(fp_major, int, S_IRUGO);

module_init(fp_init);
module_exit(fp_exit);
