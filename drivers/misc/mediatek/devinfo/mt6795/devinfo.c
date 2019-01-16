#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/ioctl.h>
#include <linux/xlog.h>
#include <linux/device.h>
#ifdef CONFIG_OF
#include <linux/of_fdt.h>
#endif
#include <asm/setup.h>
#include "devinfo.h"

#define DEVINFO_TAG "DEVINFO"

u32 get_devinfo_with_index(u32 index);
u32 g_devinfo_data[DEVINFO_DATA_SIZE];
u32 g_devinfo_data_size = 0;

EXPORT_SYMBOL(g_devinfo_data);
EXPORT_SYMBOL(g_devinfo_data_size);
EXPORT_SYMBOL(get_devinfo_with_index);

/***************************************************************************** 
* FUNCTION DEFINITION 
*****************************************************************************/
static struct cdev devinfo_cdev;
static struct class *devinfo_class;
static dev_t devinfo_dev;
static int dev_open(struct inode *inode, struct file *filp);
static int dev_release(struct inode *inode, struct file *filp);
static long dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg);


#define SLT_DEVINFO_KERNEL_SUPPORT
#ifdef SLT_DEVINFO_KERNEL_SUPPORT
#include <linux/dev_info.h>
static DEFINE_SPINLOCK(dev_lock);
static struct list_head active_devinfo_list ;

int devinfo_ram_size;
static ssize_t devinfo_show(struct device *dev1, struct device_attribute *attr, char *buf)
{
	char *p;
	size_t   info_length = 0;
	unsigned long irqflags;
	struct devinfo_struct *devinfo;

	spin_lock_irqsave(&dev_lock, irqflags);
	p = buf;
	info_length = sprintf(p, "type\t\t\tmodule\t\t\tvender\t\t\tic\t\t\tversion\t\t\tinfo\t\t\tused\n");
	p += info_length;

      list_for_each_entry(devinfo, &active_devinfo_list, device_link) {
		info_length = sprintf(p, "%s\t\t\t%s\t\t\t%s\t\t\t%s\t\t\t%s\t\t\t%s\t\t\t%s\n",
			devinfo->device_type, devinfo->device_module, devinfo->device_vendor,
			devinfo->device_ic, devinfo->device_version, devinfo->device_info, devinfo->device_used);
		p += info_length;
      }
      spin_unlock_irqrestore(&dev_lock, irqflags);
	return (p-buf);
}
static ssize_t raw_show(struct device *dev1, struct device_attribute *attr, char *buf)

{
		sprintf(buf, "%d\n", devinfo_ram_size);
	return strlen(buf);
}
	
static DEVICE_ATTR(devinfo, S_IRUGO , devinfo_show, NULL);
static DEVICE_ATTR(raw, S_IRUGO , raw_show, NULL);


/*********************************************************************************
 * add new device if not yet;
 * Input:	devinfo_struct
 * Output:  1 / 0
 * Note: return 0 for there have a same device registed, not add. return 1 for add new device
 * *******************************************************************************/
int devinfo_check_add_device(struct devinfo_struct *dev)
{
	int result = 0;
      unsigned long irqflags;
	struct devinfo_struct *dev_all;
	printk("[DEVINFO] devinfo_check!\n");
	spin_lock_irqsave(&dev_lock, irqflags);
       list_add_tail(&dev->device_link, &active_devinfo_list);
       spin_unlock_irqrestore(&dev_lock, irqflags);
	return 1;
}
#endif
/**************************************************************************
 *  GET devinfo info with index
 **************************************************************************/
u32 get_devinfo_with_index(u32 index)
{
    int size = (sizeof(g_devinfo_data)/sizeof(u32));
    if ((index >= 0) && (index < size)){
        return g_devinfo_data[index];
    }else{
        pr_warn("devinfo data index out of range:%d\n", index);
        pr_warn("devinfo data size:%d\n", size);
        return 0xFFFFFFFF;
    }
}

u32 get_segment(void)
{
    int function_code = (g_devinfo_data[24] &  0x0F000000) >> 24;

    if(function_code == 0) {/*RD*/ 
        return 0;
    }
    else if(function_code >= 1 && function_code <= 5) {/*M*/  
        return 1;
    }
    else if(function_code >= 6 && function_code <= 10) {/*Normal*/    
        return 2;
    } 
    else if(function_code >= 11 && function_code <= 15) {/*T*/  
        return 3;
    }
    else
       return -1;
}

static struct file_operations devinfo_fops = {        
    .open = dev_open,        
    .release = dev_release,        
    .unlocked_ioctl   = dev_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl = dev_ioctl,
#endif
    .owner = THIS_MODULE,
    };


static int dev_open(struct inode *inode, struct file *filp){
    return 0;
}

static int dev_release(struct inode *inode, struct file *filp){        
    return 0;
}

/************************************************************************** 
*  DEV DRIVER IOCTL 
**************************************************************************/ 
static long dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    u32 index = 0;
    int err   = 0;
    int ret   = 0;
    u32 data_size = g_devinfo_data_size;
    u32 data_read = 0;

    /* ---------------------------------- */
    /* IOCTL                              */
    /* ---------------------------------- */
    if (_IOC_TYPE(cmd) != DEV_IOC_MAGIC)
        return -ENOTTY;
    if (_IOC_NR(cmd) > DEV_IOC_MAXNR)
        return -ENOTTY;
    if (_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    if (_IOC_DIR(cmd) & _IOC_WRITE)
        err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
    if (err) return -EFAULT;
    
    switch (cmd) {

        /* ---------------------------------- */
        /* get dev info data                  */
        /* ---------------------------------- */
        case READ_DEV_DATA:
            //xlog_printk(ANDROID_LOG_INFO, DEVINFO_TAG ,"%s CMD - READ_DEV_DATA\n",MODULE_NAME);
            if(copy_from_user((void *)&index, (void __user *)arg, sizeof(u32)))
            {              
                return -EFAULT;            
            }
            //xlog_printk(ANDROID_LOG_INFO, DEVINFO_TAG ,"%s READ_DEV_DATA IDX:%d\n",MODULE_NAME, index);

            if (index < data_size){
                data_read = get_devinfo_with_index(index);
                ret = copy_to_user((void __user *)arg, (void *)&(data_read), sizeof(u32));
            }else{
            pr_warn("%s Error! Data index larger than data size. index:%d, size:%d\n",MODULE_NAME,
                index, data_size);
                return -EFAULT;            
            }
        break;       
    }
    
    return 0;
}




/******************************************************************************
 * devinfo_init
 * 
 * DESCRIPTION: 
 *   Init the device driver ! 
 * 
 * PARAMETERS: 
 *   None
 * 
 * RETURNS: 
 *   0 for success
 * 
 * NOTES: 
 *   None
 * 
 ******************************************************************************/
static int __init devinfo_init(void)
{
    int ret = 0;
    devinfo_dev = MKDEV(MAJOR_DEV_NUM, 0);

    xlog_printk(ANDROID_LOG_INFO, DEVINFO_TAG ," init\n");    

    ret = register_chrdev_region(devinfo_dev, 1, DEV_NAME );
    if (ret) {
        pr_info("[%s] register device failed, ret:%d\n", MODULE_NAME, ret);
        return ret;
    }
    /*create class*/
    devinfo_class = class_create(THIS_MODULE, DEV_NAME);
    if (IS_ERR(devinfo_class)) {
		    ret = PTR_ERR(devinfo_class);
        pr_warn("[%s] register class failed, ret:%d\n", MODULE_NAME, ret);
        return ret;
    }



    /* initialize the device structure and register the device  */
    cdev_init(&devinfo_cdev, &devinfo_fops);
    devinfo_cdev.owner = THIS_MODULE;
    if ((ret = cdev_add(&devinfo_cdev, devinfo_dev  , 1)) < 0) 
    {
        pr_warn("[%s] could not allocate chrdev for the device, ret:%d\n", MODULE_NAME, ret);
        return ret;
    }
    /*create device*/
#ifdef SLT_DEVINFO_KERNEL_SUPPORT
	struct device *devmap;
	devmap = device_create(devinfo_class,NULL,devinfo_dev,NULL,"devmap");
	device_create_file(devmap, &dev_attr_devinfo);
	device_create_file(devmap, &dev_attr_raw);
       INIT_LIST_HEAD(&active_devinfo_list);

#else
    device_create(devinfo_class,NULL,devinfo_dev,NULL,"devmap");
#endif		
    return 0;
}
#ifdef CONFIG_OF
static int __init dt_get_devinfo(unsigned long node, const char *uname, int depth, void *data)
{
    struct devinfo_tag *tags;
    int i;
    unsigned long size = 0 ;

    if (depth != 1 ||(strcmp(uname, "chosen") != 0 && strcmp(uname, "chosen@0") != 0))
        return 0;
        
    tags = (struct devinfo_tag *) of_get_flat_dt_prop(node, "atag,devinfo", &size);
    
    if(tags){
        g_devinfo_data_size = tags->devinfo_data_size;
        for (i=0;i < g_devinfo_data_size; i++){
            g_devinfo_data[i] = tags->devinfo_data[i];
        }
        /* print chip id for debugging purpose */
        pr_info("tag_devinfo_data size:%d \n", g_devinfo_data_size);
	
    }
    
    return 1;	
}

static int __init devinfo_of_init(void)
{
    of_scan_flat_dt(dt_get_devinfo, NULL);

    return 0;
}
#endif
/******************************************************************************
 * devinfo_exit
 * 
 * DESCRIPTION: 
 *   Free the device driver ! 
 * 
 * PARAMETERS: 
 *   None
 * 
 * RETURNS: 
 *   None
 * 
 * NOTES: 
 *   None
 * 
 ******************************************************************************/
static void __exit devinfo_exit(void)
{
	cdev_del(&devinfo_cdev);        
	unregister_chrdev_region(devinfo_dev, 1);
}
#ifdef CONFIG_OF
early_initcall(devinfo_of_init);
#endif
module_init(devinfo_init);
module_exit(devinfo_exit);
MODULE_LICENSE("GPL");


