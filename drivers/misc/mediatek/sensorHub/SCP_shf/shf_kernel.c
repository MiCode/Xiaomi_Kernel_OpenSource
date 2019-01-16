#include <asm/atomic.h>
#include <asm/uaccess.h>

#include <cust_acc.h>

#include <mach/md32_ipi.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>

#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/ioctl.h>
#include <linux/irq.h>
#include <linux/kobject.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>

#include "shf_kernel.h"

/*----------------------------------------------------------------------------*/
#define SHF_TAG                  "[shf_kernel] "
#define SHF_FUN(f)               printk( SHF_TAG"%s\n", __FUNCTION__)
#define SHF_ERR(fmt, args...)    printk(KERN_ERR SHF_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define SHF_LOG(fmt, args...)    printk( SHF_TAG fmt, ##args)
/*----------------------------------------------------------------------------*/

//static bool shf_ring_buffer_empty = true;
static int shf_init_flag = -1; //0->ok, -1->fail
static struct platform_driver shf_driver;
struct wake_lock shf_suspend_lock;

static DEFINE_SPINLOCK(shf_spinlock);
bool ring_buffer_poll(ipi_buffer_t* buffer, ipi_data_t* data)
{
    unsigned long flags = 0;
    spin_lock_irqsave(&shf_spinlock, flags);
    SHF_LOG("poll: head=%zu, tail=%zu\n", buffer->head, buffer->tail);
    if (data && buffer->head != buffer->tail) {
        memcpy(data, buffer->data + buffer->head, sizeof(ipi_data_t));
        buffer->head = (buffer->head + 1) % buffer->size;
        spin_unlock_irqrestore(&shf_spinlock, flags);
        return true;
    }
    spin_unlock_irqrestore(&shf_spinlock, flags);
    return false;
}

bool ring_buffer_push(ipi_buffer_t* buffer, void* data, size_t size)
{
    unsigned long flags = 0;
    spin_lock_irqsave(&shf_spinlock, flags);
    SHF_LOG("push: head=%zu, tail=%zu, size=%zu\n", buffer->head, buffer->tail, size);
    if ((buffer->tail + 1) % buffer->size != buffer->head) {
        ipi_data_t* dst = buffer->data + buffer->tail;
        memcpy((void*)dst, data, size);
        dst->size = size;
        buffer->tail = (buffer->tail + 1) % buffer->size;
        spin_unlock_irqrestore(&shf_spinlock, flags);
        return true;
    }
    spin_unlock_irqrestore(&shf_spinlock, flags);
    return false;
}
/******************************************************************/
//cach ipi message to buffer, so ipi callback will return quickly.
#define IPI_TRIGGER_BUFFER_COUNT    (32)

static ipi_data_t trigger_data[IPI_TRIGGER_BUFFER_COUNT];
static ipi_buffer_t trigger_buffer = {
    .head = 0,
    .tail = 0,
    .size = IPI_TRIGGER_BUFFER_COUNT,
    .data = trigger_data,
};

///******************************************************************/
static DECLARE_WAIT_QUEUE_HEAD(data_wq);
static void event_init(void)
{
    init_waitqueue_head(&data_wq);
}

static void event_destroy(void)
{
    //event_destroy(&shf_event);
}

static int wait(void)
{
    int ret;
    ret = wait_event_interruptible(data_wq, trigger_buffer.head != trigger_buffer.tail);
    if (ret) {
        SHF_ERR("wait: head=0x%zx, tail=0x%zx, ret=%d\n", trigger_buffer.head, trigger_buffer.tail, ret);
    }
    return ret;
}

static void notify(void)
{
    wake_up(&data_wq);
    wake_lock_timeout(&shf_suspend_lock, HZ / 2);
}
/****************************************************************************** 
 * Function Configuration
******************************************************************************/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
    return 0;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_buffer_value(struct device_driver *ddri, char *buf)
{
    return 0;
}
/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(chipinfo,    S_IWUSR | S_IRUGO, show_chipinfo_value,     NULL);
static DRIVER_ATTR(buffer,      S_IWUSR | S_IRUGO, show_buffer_value,       NULL);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *shf_attr_list[] = {
    &driver_attr_chipinfo,     /*chip information*/
    &driver_attr_buffer,   /*dump buffer data*/
};
/*----------------------------------------------------------------------------*/
static int shf_create_attr(struct device_driver *driver) 
{
    int idx, err = 0;
    int num = (int)(sizeof(shf_attr_list)/sizeof(shf_attr_list[0]));
    if (driver == NULL) {
        return -EINVAL;
    }

    for(idx = 0; idx < num; idx++) {
        if((err = driver_create_file(driver, shf_attr_list[idx])) != 0) {            
            SHF_ERR("driver_create_file (%s) = %d\n", shf_attr_list[idx]->attr.name, err);
            break;
        }
    }    
    return err;
}
/*----------------------------------------------------------------------------*/
static int shf_delete_attr(struct device_driver *driver)
{
    int idx ,err = 0;
    int num = (int)(sizeof(shf_attr_list)/sizeof(shf_attr_list[0]));

    if(driver == NULL) {
        return -EINVAL;
    }

    for(idx = 0; idx < num; idx++) {
        driver_remove_file(driver, shf_attr_list[idx]);
    }
    
    return err;
}
/*----------------------------------------------------------------------------*/
static int shf_open(struct inode *inode, struct file *file)
{
    SHF_FUN();
    //Now, we don't hava any private data
    file->private_data = NULL;
    return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int shf_release(struct inode *inode, struct file *file)
{
    file->private_data = NULL;
    return 0;
}
/*----------------------------------------------------------------------------*/
/*
static void shf_print_bytes(void* buffer, size_t size) {
    uint8_t* data;
    int i;
    if (!buffer) {
        SHF_ERR("print: null\n");
        return;
    }
    data = (uint8_t*)buffer;
    SHF_LOG("print: size=%d. ", size);
    for (i = 0; i < size; i++) {
        SHF_LOG("0x%.2x ", *(data + i));
    }
    SHF_LOG("\n");
}
*/
/*----------------------------------------------------------------------------*/
static long shf_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    ipi_data_t in_data;
    ipi_data_t out_data;    
    void __user *data;
    long err = 0;
    ipi_status status = DONE;
    ipi_status pre_status = DONE;

    if (shf_init_flag != 0) {
        SHF_ERR("IOCTL: initflag=%d\n", shf_init_flag);
        return -EFAULT;//TODO should check error no
    }
    /*
    if(_IOC_DIR(cmd) & _IOC_READ) {
        err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    }
    else if(_IOC_DIR(cmd) & _IOC_WRITE) {
        err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
    }

    if(err) {
        SHF_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
        return -EFAULT;
    }
    */
    switch(cmd) {
    case SHF_IPI_SEND:
        //SHF_LOG("IOCTL: SHF_IPI_SEND\n");
        data = (void __user *) arg;
        if(copy_from_user((void*)&in_data, data, sizeof(ipi_data_t))) {
            err = -EFAULT;
            SHF_ERR("SHF_IPI_SEND: copy failed!\n");
            break;      
        }
        //shf_print_bytes(in_data.data, in_data.size);
        do {
            status = md32_ipi_send(IPI_SHF, in_data.data, in_data.size, 0);
            if (status != pre_status || DONE == pre_status)
                SHF_LOG("SHF_IPI_SEND: size=%zu, status=%d\n", in_data.size, status);
            err = status;
            pre_status = status;
        } while(DONE != status);
        mdelay(10);
        //SHF_LOG("SHF_IPI_SEND: delayed 10ms.\n");
        break;
    case SHF_IPI_POLL:
        //SHF_LOG("IOCTL: SHF_IPI_POLL\n");
        data = (void __user*)arg;
        if(data == NULL) {
            err = -EINVAL;
            break;      
        }
        memset(&out_data, 0, sizeof(ipi_data_t));
        if (trigger_buffer.head == trigger_buffer.tail) {
            if (wait()) {
                err = -EINTR;
                break;
            }
        }
        if(!ring_buffer_poll(&trigger_buffer, &out_data)) {
            err = -EFAULT;
            SHF_ERR("SHF_IPI_POLL: failed!\n");
            break;
        }
        if(copy_to_user(data, &out_data, sizeof(ipi_data_t))) {
            err = -EFAULT;
            SHF_ERR("SHF_IPI_POLL: copy failed!\n");
            break;
        }
        break;
    case SHF_GESTURE_ENABLE: 
        //SHF_LOG("IOCTL: SHF_GESTURE_ENABLE. enable=%d\n", arg);
#ifdef MTK_SENSOR_HUB_SUPPORT
        tpd_scp_wakeup_enable(arg);
#endif
        break;
    default:
        SHF_ERR("unknown IOCTL: 0x%08x\n", cmd);
        err = -ENOIOCTLCMD;
        break;
    }
    return err;
}
/*----------------------------------------------------------------------------*/
static struct file_operations shf_fops = {
    .owner = THIS_MODULE,
    .open = shf_open,
    .release = shf_release,
    .unlocked_ioctl = shf_unlocked_ioctl,
    //.ioctl = shf_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice shf_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "shf",
    .fops = &shf_fops,
};
/*----------------------------------------------------------------------------*/
static void shf_ipi_receive_handler(int id, void* data, uint size)
{
    if (id == IPI_SHF) {
        //shf_print_bytes(data, size);
        SHF_LOG("IPI_SHF\n");
        ring_buffer_push(&trigger_buffer, data, size);
        notify();
    }
}
/*----------------------------------------------------------------------------*/
static int shf_driver_probe(struct platform_device *pdev)
{
    int err = 0;
    event_init();//init event for wait/notify
    if((err = misc_register(&shf_device)) != 0) {
        SHF_ERR("register device failed!\n");
        goto exit_misc_device_register_failed;
    }
    if((err = shf_create_attr(&shf_driver.driver)) != 0) {
        SHF_ERR("create attribute err=%d\n", err);
        goto exit_create_attr_failed;
    }
    err = md32_ipi_registration(IPI_SHF, shf_ipi_receive_handler, "shf_ipi_receive_handler");
    if (DONE != err) {
        goto exit_ipi_receive_register_failed;
    }
    shf_init_flag = 0;
    SHF_LOG("register device succeed.\n");
    return 0;

    exit_ipi_receive_register_failed:
    exit_create_attr_failed:
    misc_deregister(&shf_device);

    exit_misc_device_register_failed:
    SHF_ERR("%s: err=%d\n", __func__, err);
    shf_init_flag =-1;
    return err;
}

/*----------------------------------------------------------------------------*/
static int shf_driver_remove(struct platform_device *pdev)
{
    int err = 0;
    SHF_FUN();
    if((err = shf_delete_attr(&shf_driver.driver)) != 0) {
        SHF_ERR("shf_delete_attr fail: %d\n", err);
    }
    if((err = misc_deregister(&shf_device)) != 0) {
        SHF_ERR("misc_deregister shf_device fail: %d\n", err);
    }
    md32_ipi_registration(IPI_SHF, NULL, NULL);
    event_destroy();//destroy event for wait/notify
    return 0;
}
/*----------------------------------------------------------------------------*/
/*
static struct platform_driver shf_driver = {
    .probe      = shf_driver_probe,
    .remove     = shf_driver_remove,    
    .driver     = {
        .name  = "shf",
        .owner = THIS_MODULE,
    }
};
*/
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_OF
static const struct of_device_id shf_of_match[] = {    
    { .compatible = "mediatek,shf", },    
    {},
};
#endif

static struct platform_driver shf_driver = {
    .probe      = shf_driver_probe,    
    .remove     = shf_driver_remove,
    .driver     =   
    {
        .name  = "shf",        
#ifdef CONFIG_OF     
        .of_match_table = shf_of_match,     
#endif  
    }
};
/*----------------------------------------------------------------------------*/
static int __init shf_driver_init(void)
{
    SHF_FUN();
    if(platform_driver_register(&shf_driver)) {
        SHF_ERR("failed to register shf driver");
        return -ENODEV;
    }
    wake_lock_init(&shf_suspend_lock, WAKE_LOCK_SUSPEND, "shf_wakelock");
    return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit shf_driver_exit(void)
{
    SHF_FUN();
    platform_driver_unregister(&shf_driver);
}
/*----------------------------------------------------------------------------*/
module_init(shf_driver_init);
module_exit(shf_driver_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("sensor hub framework driver");
MODULE_AUTHOR("mediatek.inc.");
/*----------------------------------------------------------------------------*/
