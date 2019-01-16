/*
* Copyright (C) 2011-2014 MediaTek Inc.
* 
* This program is free software: you can redistribute it and/or modify it under the terms of the 
* GNU General Public License version 2 as published by the Free Software Foundation.
* 
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

/******************************************************************************
 *  INCLUDE LINUX HEADER
 ******************************************************************************/
#include <linux/kernel.h> 
#include <linux/module.h> 
#include <linux/init.h> 
#include <linux/moduleparam.h>
#include <linux/slab.h> 
#include <linux/unistd.h> 
#include <linux/sched.h> 
#include <linux/fs.h> 
#include <asm/uaccess.h> 
#include <linux/version.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <mach/memory.h>
#include <asm/io.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

/******************************************************************************
 *  INCLUDE LIBRARY
 ******************************************************************************/
#include <mach/sec_osal.h>
#include "sec_mod.h"
#ifdef MTK_SECURITY_MODULE_LITE
#include "masp_version.h"
#endif
#include "sec_boot_core.h"

#define SEC_DEV_NAME                "sec"
#define SEC_MAJOR                   182
#define MOD                         "MASP"

#define TRACE_FUNC()                MSG_FUNC(SEC_DEV_NAME)

/**************************************************************************
 *  EXTERNAL VARIABLE
 **************************************************************************/
extern const struct sec_ops         *sec_get_ops(void);
extern bool                         bMsg;
extern struct semaphore             hacc_sem;

/*************************************************************************
 *  GLOBAL VARIABLE
 **************************************************************************/
static struct sec_mod sec           = {0};
static struct cdev                  sec_dev;
static struct class                 *sec_class;
static struct device                *sec_device;

#ifdef CONFIG_ARM64
unsigned long long hacc_base;
unsigned long long es_base;
#else
unsigned int hacc_base;
unsigned int es_base;
#endif

static const struct of_device_id masp_of_ids[] = {
    {.compatible = "mediatek,SEJ", },
    {}
};

/* ****************************
 * FOR ES_BASE ONLY
 ******************************/
int es_probe(struct platform_device *dev){

    es_base = of_iomap(dev->dev.of_node, 0);
    if (!es_base) {
        printk(KERN_ERR "[%s] ES register remapping failed\n", SEC_DEV_NAME);
        return -ENXIO;
    }
    return 0;
}

int es_remove(struct platform_device *dev)
{
    es_base = 0;
    return 0;
}

static const struct of_device_id es_of_ids[] = {
    {.compatible = "mediatek,EFUSEC", },
    {}
};

static struct platform_driver es_driver = {
    .driver  = {
        .name  = "es",
        .owner = THIS_MODULE,
        .of_match_table = es_of_ids,
    },
    .probe	= es_probe,
    .remove = es_remove,
};

/**************************************************************************
 *  EXTERNAL FUNCTION
 **************************************************************************/
extern int sec_get_random_id(unsigned int *rid);
extern long sec_core_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
extern void sec_core_init (void);
extern void sec_core_exit (void);

/**************************************************************************
 *  SEC DRIVER OPEN
 **************************************************************************/ 
static int sec_open(struct inode *inode, struct file *file)
{
    return 0;
}

/**************************************************************************
 *  SEC DRIVER RELEASE
 **************************************************************************/ 
static int sec_release(struct inode *inode, struct file *file)
{
    return 0;
}

/**************************************************************************
 *  SEC DRIVER IOCTL
 **************************************************************************/ 
static long sec_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
#ifdef MTK_SECURITY_MODULE_LITE
    return -EIO;
#else
    return sec_core_ioctl(file, cmd, arg);
#endif
}

static struct file_operations sec_fops = {
    .owner   = THIS_MODULE,
    .open    = sec_open,
    .release = sec_release,
    .write   = NULL,
    .read    = NULL,
    .unlocked_ioctl   = sec_ioctl
};

/**************************************************************************
 *  SEC RID PROC FUNCTION
 **************************************************************************/ 
static int sec_proc_rid_show(struct seq_file *m, void *v)
{
    unsigned int rid[4] = {0};
    unsigned int i = 0;
    sec_get_random_id((unsigned int *)rid);

    for (i=0;i<16;i++){
        seq_putc(m,*((char*)rid+i));
    }

    return 0;
}

static int sec_proc_rid_open(struct inode *inode, struct file *file)
{
    return single_open(file, sec_proc_rid_show, NULL);
}

static const struct file_operations sec_proc_rid_fops = {
    .open   = sec_proc_rid_open,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= seq_release,
};


/**************************************************************************
 *  SEC MODULE PARAMETER
 **************************************************************************/ 
static uint recovery_done = 0;
module_param(recovery_done, uint, S_IRUSR|S_IWUSR/*|S_IWGRP*/|S_IRGRP|S_IROTH); /* rw-r--r-- */
MODULE_PARM_DESC(recovery_done, "A recovery sync parameter under sysfs (0=complete, 1=on-going, 2=error)");

/**************************************************************************
 *  SEC DRIVER INIT
 **************************************************************************/ 
static int sec_init(struct platform_device * dev)
{
    int ret = 0;
    dev_t id;    

    printk( "[%s] sec_init zhaoyang (%d)\n", SEC_DEV_NAME, ret);

    hacc_base = of_iomap(dev->dev.of_node, 0);
    if (!hacc_base) {
        printk(KERN_ERR "[%s] HACC register remapping failed\n", SEC_DEV_NAME);
        return -ENXIO;
    }

    id = MKDEV(SEC_MAJOR, 0);    
    ret = register_chrdev_region(id, 1, SEC_DEV_NAME);

    if (ret) 
    {
        printk(KERN_ERR "[%s] Regist Failed (%d)\n", SEC_DEV_NAME, ret);
	return ret;
    }

    sec_class = class_create(THIS_MODULE, SEC_DEV_NAME);
    if (NULL == sec_class)
    {
	printk(KERN_ERR "[%s] Create class failed(0x%x)\n", SEC_DEV_NAME, ret);
	ret = -1;
	return ret;
    }

    cdev_init(&sec_dev, &sec_fops);
    sec_dev.owner = THIS_MODULE;

    ret = cdev_add(&sec_dev, id, 1);
    if (ret < 0)
    {
        goto exit;
    }

    sec_device = device_create(sec_class, NULL, id, NULL, SEC_DEV_NAME);
    if (NULL == sec_class)
    {
	printk(KERN_ERR "[%s] Create device failed(0x%x)\n", SEC_DEV_NAME, ret);
	class_destroy(sec_class);
	ret = -1;
	return ret;
    }

    sec.id   = id;
    sec.init = 1;
    spin_lock_init(&sec.lock);

    proc_create("rid", 0, NULL, &sec_proc_rid_fops);

#ifdef MTK_SECURITY_MODULE_LITE
    printk("[MASP Lite] version '%s%s', enter.\n",BUILD_TIME,BUILD_BRANCH);
#endif

exit:
    if (ret != 0) 
    {
    	device_destroy(sec_class, id);
    	class_destroy(sec_class);
        unregister_chrdev_region(id, 1);
        memset(&sec, 0, sizeof(sec));
    }

    return ret;
}


/**************************************************************************
 *  SEC DRIVER EXIT
 **************************************************************************/ 
static void sec_exit(void)
{    
    remove_proc_entry("rid", NULL);
    cdev_del(&sec_dev);
    unregister_chrdev_region(sec.id, 1);
    memset(&sec, 0, sizeof(sec));

#ifdef MTK_SECURITY_MODULE_LITE    
    printk("[MASP Lite] version '%s%s', exit.\n",BUILD_TIME,BUILD_BRANCH);
#else
    sec_core_exit();
#endif
}

/**************************************************************************
 *  MASP PLATFORM DRIVER WRAPPER, FOR BUILD-IN SEQUENCE
 **************************************************************************/ 
int masp_probe(struct platform_device *dev)
{
    int ret = 0;
    ret = sec_init(dev);
    return ret;
}

int  masp_remove(struct platform_device * dev)
{
    sec_exit();
    return 0;
}


static struct platform_driver masp_driver = {
    .driver  = {
        .name  = "masp",
        .owner = THIS_MODULE,
        .of_match_table = masp_of_ids,
    },
    .probe	= masp_probe,
    .remove  	= masp_remove,
};

/* dummy interrupt handler for gcpu */
static irqreturn_t gcpu_irq_handler(int irq, void *dev_id)
{  
    return IRQ_HANDLED;
}

static int __init masp_init(void)
{
    int ret;
    struct device_node *node = NULL;
    unsigned int gcpu_irq = 0;

    ret = platform_driver_register(&es_driver);
    if (ret) 
    {        	
        printk(KERN_ERR "[ES] Reg platform driver failed (%d)\n", ret);
        return ret;
    }

    ret = platform_driver_register(&masp_driver);
    if (ret) 
    {        	
        printk(KERN_ERR "[%s] Reg platform driver failed (%d)\n", SEC_DEV_NAME, ret);
        return ret;
    }

    /* register for GCPU */
    node = of_find_compatible_node(NULL, NULL, "mediatek,GCPU");
    
    if(node){
        gcpu_irq = irq_of_parse_and_map(node,0);
        pr_info("[GCPU] irq_no: (%d)\n", gcpu_irq);
        ret = request_irq(gcpu_irq, (irq_handler_t)gcpu_irq_handler, IRQF_TRIGGER_LOW , "gcpu", NULL);    
        if(ret != 0)
        {
            pr_err("[GCPU] Failed to request irq! (%d) irq no: (%d)\n", ret, gcpu_irq);
        }else{
            pr_info("[GCPU] request irq (%d) succeed!!\n", gcpu_irq);
        }
    } else{
        pr_err("[GCPU] DT find compatible node failed!! \n");
        ret = -1;
    }

    return ret;
}


static void __exit masp_exit(void)
{
    platform_driver_unregister(&es_driver);
    platform_driver_unregister(&masp_driver);
}

module_init(masp_init);
module_exit(masp_exit);

/**************************************************************************
 *  EXPORT FUNCTION
 **************************************************************************/ 
EXPORT_SYMBOL(sec_get_random_id);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MediaTek Inc.");
#ifdef MTK_SECURITY_MODULE_LITE   
MODULE_DESCRIPTION("Mediatek Security Module Lite");
#else
MODULE_DESCRIPTION("Mediatek Security Module");
#endif
