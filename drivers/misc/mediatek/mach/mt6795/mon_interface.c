#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/ftrace_event.h>
#include "mach/mt_mon.h"

#define MODULE_NAME "monitor"

static struct mtk_monitor *mtk_mon;

#include <linux/proc_fs.h>

#define DEV_IOCTLID	0xD0

#define	IOCTL_WRITE_PMU			_IOW(DEV_IOCTLID, 1, struct pmu_cfg)
#define IOCTL_READ_PMU  		_IOR(DEV_IOCTLID, 2, struct pmu_cfg)

#ifdef CONFIG_CACHE_L2X0
#define	IOCTL_WRITE_L2C			_IOW(DEV_IOCTLID, 3, struct l2c_cfg)
#define IOCTL_READ_L2C   		_IOR(DEV_IOCTLID, 4, struct l2c_cfg)
#endif

#define IOCTL_WRITE_BM			_IOR(DEV_IOCTLID, 5, int)
#define IOCTL_CTRL				_IOW(DEV_IOCTLID, 6, int)
#define IOCTL_PRINTLOG			_IOR(DEV_IOCTLID, 7, struct mt_mon_log)


#define	MONITOR_START 1
#define MONITOR_STOP 2

static int
mpu_dev_open(struct inode *inode, struct file *filp)
{
    int ret;

	ret = register_monitor(&mtk_mon, MODE_MANUAL_USER);
	
    if(ret != 0) {
    	printk("MTK Monitor Register Fail\n");
    	return -1;
    }
	
	mtk_mon->init();
	
    return 0;
}

static int
mpu_dev_release(struct inode *inode, struct file *filp)
{
    mtk_mon->deinit();
    unregister_monitor(&mtk_mon);
    return 0;
}

static long  mpu_dev_ioctl(struct file *filp,
        unsigned int cmd, unsigned long arg)
{
	u32 index, ret;
	struct pmu_cfg p_cfg;	
#ifdef CONFIG_CACHE_L2X0
	struct l2c_cfg l_cfg;
#endif
	
	if(mtk_mon == NULL) { 
			printk("Don't Have PMU Resource\n");   	
			return -1;
    }

	switch (cmd) {
		case IOCTL_WRITE_PMU:
			ret = copy_from_user((void * )&p_cfg, (const void *)arg, sizeof(struct pmu_cfg));
			if(ret != 0) {
				printk("ERROR: wrong return value [IOCTL_WRITE_PMU]\n");
				return -1;
			}
			mtk_mon->set_pmu(&p_cfg);			
			break;
			
		case IOCTL_READ_PMU:
			mtk_mon->get_pmu(&p_cfg);	
			ret = copy_to_user((void *)arg, (const void * )&p_cfg, sizeof(struct pmu_cfg));
			if(ret != 0) {
				printk("ERROR: wrong return value [IOCTL_READ_PMU]\n");
				return -1;
			}
			break;
#ifdef CONFIG_CACHE_L2X0	
		case IOCTL_WRITE_L2C:
			ret = copy_from_user((void * )&l_cfg, (const void *)arg, sizeof(struct l2c_cfg));
			if(ret != 0) {
				printk("ERROR: wrong return value [IOCTL_WRITE_L2C]\n");
				return -1;
			}
			mtk_mon->set_l2c(&l_cfg);
			break;
			
		case IOCTL_READ_L2C:
			mtk_mon->get_l2c(&l_cfg);	
			ret = copy_to_user((void *)arg, (const void * )&l_cfg, sizeof(struct l2c_cfg));
			if(ret != 0) {
				printk("ERROR: wrong return value [IOCTL_READ_L2C]\n");
				return -1;
			}
			break;
#endif		
		case IOCTL_WRITE_BM:
			mtk_mon->set_bm_rw(arg);
			break;
			
		case IOCTL_CTRL:
			switch (arg) {
    			case MONITOR_START:
    				mtk_mon->enable();
    				break;
    			case MONITOR_STOP:
    				mtk_mon->disable();
    				break;
    			default:
    				printk("Error Number: Start(1), Stop(2)!!!\n");
    				break;
			}
			break;
			
		case IOCTL_PRINTLOG:			
			index = mtk_mon->mon_log(NULL);
			ret = copy_to_user((void *)arg, (const void * )&mtk_mon->log_buff[index], sizeof(struct mt_mon_log));
			if(ret != 0) {
				printk("ERROR: wrong return value [IOCTL_PRINTLOG]\n");
				return -1;
			}
			break;
			
		default:
			return -1;
	}
	return 0;
}


	  
static struct file_operations dev_fops = {
    .owner = THIS_MODULE,
    .open = mpu_dev_open,
    .release = mpu_dev_release,
    .unlocked_ioctl = mpu_dev_ioctl,
};

static struct miscdevice mpu_miscdev = {
    .minor      = MISC_DYNAMIC_MINOR,
    .name       = "mtk_monitor",
    .fops       = &dev_fops,
};


static int __init mon_user_init(void)
{
	int ret;
    
    ret = misc_register(&mpu_miscdev);
	
	if(ret < 0) {
    	printk("PMU MISC Register fail\n");
    	return ret;
    }

	/* everything OK */
    printk(KERN_INFO "%s initialised\n", MODULE_NAME);
    return 0;

}    
static void __exit mon_user_exit(void)
{
    misc_deregister(&mpu_miscdev);
    printk(KERN_INFO "%s removed\n", MODULE_NAME);	
}

module_init(mon_user_init);
module_exit(mon_user_exit);

