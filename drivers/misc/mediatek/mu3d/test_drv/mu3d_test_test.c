#include <linux/init.h>
#include <linux/irq.h>
#include <linux/log2.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>

#include <linux/kernel.h>       /* printk() */
#include <linux/fs.h>           /* everything... */
#include <linux/errno.h>        /* error codes */
#include <linux/types.h>        /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>        /* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>
//#include <linux/pci.h>
#include <linux/string.h>
#include <linux/random.h>
#include <linux/scatterlist.h>
#include <asm/unaligned.h>
#include <linux/usb/ch9.h>
#include <asm/uaccess.h>
#include <linux/mu3d/test_drv/mu3d_test_test.h>
#include <linux/mu3d/test_drv/mu3d_test_unified.h>

#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/miscdevice.h>
#include <linux/wakelock.h>


struct file_operations mu3d_mtk_test_fops_dev;

static struct miscdevice mu3d_test_uevent_device = {
         .minor = MISC_DYNAMIC_MINOR,
         .name = "usbif_u3d_test_uevent",
         .fops = NULL,
};

////////////////////////////////////////////////////////////////////////////

#define CLI_MAGIC 'C'
#define IOCTL_READ _IOR(CLI_MAGIC, 0, int)
#define IOCTL_WRITE _IOW(CLI_MAGIC, 1, int)

#define BUF_SIZE 200
#define MAX_ARG_SIZE 20

////////////////////////////////////////////////////////////////////////////

typedef struct
{
	char name[256];
	int (*cb_func)(int argc, char** argv);
} CMD_TBL_T;

static CMD_TBL_T _arPCmdTbl_dev[] =
{
	{"auto.dev", &TS_AUTO_TEST},
	{"auto.stop", &TS_AUTO_TEST_STOP},
	{"auto.u3i", &u3init},
	{"auto.u3w", &u3w},
	{"auto.u3r", &u3r},
	{"auto.u3d", &U3D_Phy_Cfg_Cmd},
	{"auto.link", &u3d_linkup},
	{"auto.eyeinit", &dbg_phy_eyeinit},
	{"auto.eyescan", &dbg_phy_eyescan},
#ifdef SUPPORT_OTG
	{"auto.otg", &otg_top},
#endif
	{"", NULL}
};

////////////////////////////////////////////////////////////////////////////

char wr_buf_dev[BUF_SIZE];
char rd_buf_dev[BUF_SIZE] = "this is a test";

static struct wake_lock mu3d_mtk_test_wakelock;

////////////////////////////////////////////////////////////////////////////

void mu3d_mtk_test_wakelock_lock(void){
    if(!wake_lock_active(&mu3d_mtk_test_wakelock))
        wake_lock(&mu3d_mtk_test_wakelock);
    printk("[U3D_T] mu3d_mtk_test_wakelock_lock done\n") ;
}

void mu3d_mtk_test_wakelock_unlock(void){
    if(wake_lock_active(&mu3d_mtk_test_wakelock))
        wake_unlock(&mu3d_mtk_test_wakelock);
    printk("[U3D_T] mu3d_mtk_test_wakelock_unlock done\n") ;
}

void mu3d_mtk_test_wakelock_init(void){
    wake_lock_init(&mu3d_mtk_test_wakelock, WAKE_LOCK_SUSPEND, "mu3d_mtk_test.wakelock");
    printk("[U3D_T] mu3d_mtk_test_wakelock_init done\n") ;
}


int call_function_dev(char *buf)
{
	int i;
	int argc;
	char *argv[MAX_ARG_SIZE];

	argc = 0;
	do
	{
		argv[argc] = strsep(&buf, " ");
		printk(KERN_DEBUG "[%d] %s\r\n", argc, argv[argc]);
		argc++;
	} while (buf);

	#define CMD_NUM 10

	for (i = 0; i < CMD_NUM; i++)
	{
		if ((!strcmp(_arPCmdTbl_dev[i].name, argv[0])) && (_arPCmdTbl_dev[i].cb_func != NULL))
			return _arPCmdTbl_dev[i].cb_func(argc, argv);
	}

	return -1;
}


static int mu3d_mtk_test_open(struct inode *inode, struct file *file)
{
	printk("[U3D_T] mu3d_mtk_test_open\n");
    printk(KERN_DEBUG "mu3d_mtk_test open: successful\n");
    return 0;
}

static int mu3d_mtk_test_release(struct inode *inode, struct file *file)
{

    printk(KERN_DEBUG "mu3d_mtk_test release: successful\n");
    return 0;
}

static ssize_t mu3d_mtk_test_read(struct file *file, char *buf, size_t count, loff_t *ptr)
{
	printk("[U3D_T] mu3d_mtk_test read: returning zero bytes\n");
    printk(KERN_DEBUG "mu3d_mtk_test read: returning zero bytes\n");
    return 0;
}

static ssize_t mu3d_mtk_test_write(struct file *file, const char *buf, size_t count, loff_t * ppos)
{
	printk("[U3D_T] mu3d_mtk_test_write: returning zero bytes\n");
    printk(KERN_DEBUG "mu3d_mtk_test write: accepting zero bytes\n");
    return 0;
}

static long mu3d_mtk_test_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{

    int len = BUF_SIZE;

	switch (cmd) {
		case IOCTL_READ:
			if(copy_to_user((char *) arg, rd_buf_dev, len))
				return -EFAULT;
			printk(KERN_DEBUG "IOCTL_READ: %s\n", rd_buf_dev);
			break;
		case IOCTL_WRITE:
			if(copy_from_user(wr_buf_dev, (char *) arg, len))
				return -EFAULT;
			printk("[U3D_T]IOCTL_WRITE: %s\n", wr_buf_dev);

			//invoke function
			return call_function_dev(wr_buf_dev);
			break;
		default:
			printk("cmd=%d\r", cmd);
			return -ENOTTY;
	}

	return len;
}

// USBIF , to send IF uevent
int usbif_u3d_test_send_event(char* event)
{	
	char udev_event[128];
	char *envp[] = {udev_event, NULL };
	int ret ;

	snprintf(udev_event, 128, "USBIF_EVENT=%s",event);
	printk("usbif_u3d_test_send_event - sending event - %s in %s\n", udev_event, kobject_get_path(&mu3d_test_uevent_device.this_device->kobj, GFP_KERNEL));
	ret = kobject_uevent_env(&mu3d_test_uevent_device.this_device->kobj, KOBJ_CHANGE, envp);	
	if (ret < 0)
		printk("mu3d_test_uevent_device sending failed with ret = %d, \n", ret);
		
	return ret;
}

struct file_operations mu3d_mtk_test_fops_dev = {
    .owner =   THIS_MODULE,
    .read =    mu3d_mtk_test_read,
    .write =   mu3d_mtk_test_write,
    .unlocked_ioctl =   mu3d_mtk_test_ioctl,
    .open =    mu3d_mtk_test_open,
    .release = mu3d_mtk_test_release,
};


static int __init mtk_test_init(void)
{
	int retval = 0;
	struct class *cli_class ;
	dev_t cli_dev_t = MKDEV(MU3D_MTK_TEST_MAJOR, 0) ;
	struct device *cli_dev ;
	
	printk(KERN_DEBUG "mu3d cli Init\n");

	mu3d_mtk_test_wakelock_init() ;

	cli_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(cli_class))
		printk(KERN_DEBUG "%s: failed to create cli class ", __func__);
		
	cli_dev = device_create(cli_class, NULL, cli_dev_t, NULL, DEVICE_NAME);
	if (IS_ERR(cli_dev))
		printk(KERN_DEBUG "%s: failed to create cli dev ", __func__);	
	
	retval = register_chrdev(MU3D_MTK_TEST_MAJOR, DEVICE_NAME, &mu3d_mtk_test_fops_dev);
	if(retval < 0)
	{
		printk(KERN_DEBUG "mu3d cli Init failed, %d\n", retval);
		goto fail;
	}

	// USBIF
	if (!misc_register(&mu3d_test_uevent_device)){
		printk("create the mu3d_test_uevent_device uevent device OK!\n") ;

	}else{
		printk("[ERROR] create the mu3d_test_uevent_device uevent device fail\n") ;
	}
	
	return 0;
	fail:
		return retval;
}
module_init(mtk_test_init);

static void __exit mtk_test_cleanup(void)
{
	printk(KERN_DEBUG "mu3d_mtk_test End\n");
	
	// USBIF
	misc_deregister(&mu3d_test_uevent_device);
	
	unregister_chrdev(MU3D_MTK_TEST_MAJOR, DEVICE_NAME);

}
module_exit(mtk_test_cleanup);

MODULE_LICENSE("GPL");
