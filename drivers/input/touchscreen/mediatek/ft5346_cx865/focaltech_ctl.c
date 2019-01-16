#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>

#include "tpd_custom_fts.h"
#include "focaltech_ctl.h"
#include "focaltech_ex_fun.h"
#include <linux/netdevice.h>


extern u8 *I2CDMABuf_va ;
extern volatile u32 I2CDMABuf_pa;


static int ft_rw_iic_drv_major = FT_RW_IIC_DRV_MAJOR;
struct ft_rw_i2c_dev 
{
    struct cdev cdev;
    struct semaphore ft_rw_i2c_sem;
    struct i2c_client *client;
};
struct ft_rw_i2c_dev *ft_rw_i2c_dev_tt;
static struct class *fts_class;

static int ft_rw_iic_drv_myread(struct i2c_client *client, u8 *buf, int length)
{
    int ret = 0;    
    ret = fts_i2c_Read(client, NULL, 0, buf, length);

    if(ret<0)
        dev_err(&client->dev, "%s:IIC Read failed\n",
                __func__);
    return ret;
}

static int ft_rw_iic_drv_mywrite(struct i2c_client *client, u8 *buf, int length)
{
    int ret = 0;
    ret = fts_i2c_Write(client, buf, length);
    if(ret<0)
        dev_err(&client->dev, "%s:IIC Write failed\n",
                __func__);
    return ret;
}

static int ft_rw_iic_drv_RDWR(struct i2c_client *client, unsigned long arg)
{
    struct ft_rw_i2c_queue i2c_rw_queue;
    u8 __user **data_ptrs;
    struct ft_rw_i2c * i2c_rw_msg;
    int ret = 0;
    int i;

    if (!access_ok(VERIFY_READ, (struct ft_rw_i2c_queue *)arg, sizeof(struct ft_rw_i2c_queue)))
        return -EFAULT;

    if (copy_from_user(&i2c_rw_queue,
        (struct ft_rw_i2c_queue *)arg, 
        sizeof(struct ft_rw_i2c_queue)))
        return -EFAULT;

    if (i2c_rw_queue.queuenum > FT_I2C_RDWR_MAX_QUEUE)
        return -EINVAL;


    i2c_rw_msg = (struct ft_rw_i2c*)
        kmalloc(i2c_rw_queue.queuenum *sizeof(struct ft_rw_i2c),
        GFP_KERNEL);
    if (!i2c_rw_msg)
        return -ENOMEM;

    if (copy_from_user(i2c_rw_msg, i2c_rw_queue.i2c_queue,
            i2c_rw_queue.queuenum*sizeof(struct ft_rw_i2c))) {
        kfree(i2c_rw_msg);
        return -EFAULT;
    }

    data_ptrs = kmalloc(i2c_rw_queue.queuenum * sizeof(u8 __user *), GFP_KERNEL);
    if (data_ptrs == NULL) {
        kfree(i2c_rw_msg);
        return -ENOMEM;
    }
    
    ret = 0;
    for (i=0; i< i2c_rw_queue.queuenum; i++) {
        if ((i2c_rw_msg[i].length > 8192)||
            (i2c_rw_msg[i].flag & I2C_M_RECV_LEN)) {
            ret = -EINVAL;
            break;
        }
        data_ptrs[i] = (u8 __user *)i2c_rw_msg[i].buf;
        i2c_rw_msg[i].buf = kmalloc(i2c_rw_msg[i].length, GFP_KERNEL);
        if (i2c_rw_msg[i].buf == NULL) {
            ret = -ENOMEM;
            break;
        }

        if (copy_from_user(i2c_rw_msg[i].buf, data_ptrs[i], i2c_rw_msg[i].length)) {
            ++i;
            ret = -EFAULT;
            break;
        }
    }
//printk("ft_rw_iic_drv_RDWR 6  %d %x  %x\n",ret,data_ptrs,i2c_rw_msg);
    if (ret < 0) {
        int j;
        for (j=0; j<i; ++j)
            kfree(i2c_rw_msg[j].buf);
        kfree(data_ptrs);
        kfree(i2c_rw_msg);
        return ret;
    }
//printk("ft_rw_iic_drv_RDWR 7\n");
    for (i=0; i< i2c_rw_queue.queuenum; i++) {
        //printk("ft_rw_iic_drv_RDWR 8  %d  %x  %x  %d\n",i2c_rw_msg[i].flag,i2c_rw_msg[i].buf,data_ptrs[i],i2c_rw_msg[i].length);
        if (i2c_rw_msg[i].flag) {
            ret = ft_rw_iic_drv_myread(client,
                    i2c_rw_msg[i].buf, i2c_rw_msg[i].length);
            if (ret>=0)
                ret = copy_to_user(data_ptrs[i], i2c_rw_msg[i].buf, i2c_rw_msg[i].length);
        }
        else
            ret = ft_rw_iic_drv_mywrite(client,
                    i2c_rw_msg[i].buf, i2c_rw_msg[i].length);
    }
//printk("ft_rw_iic_drv_RDWR 10\n");
    return ret;
    
}

/*
*char device open function interface 
*/
static int ft_rw_iic_drv_open(struct inode *inode, struct file *filp)
{
    filp->private_data = ft_rw_i2c_dev_tt;
    return 0;
}

/*
*char device close function interface 
*/
static int ft_rw_iic_drv_release(struct inode *inode, struct file *filp)
{

    return 0;
}

static int ft_rw_iic_drv_ioctl(struct file *filp, unsigned
  int cmd, unsigned long arg)
{
    int ret = 0;
    struct ft_rw_i2c_dev *ftdev = filp->private_data;
    ftdev = filp->private_data;
    //printk("ft_rw_iic_drv_ioctl\n");
//  down(&ft_rw_i2c_dev_tt->ft_rw_i2c_sem);
    //printk("====ft_rw_iic_drv_ioctl: client = %x\n",ftdev->client);
    switch (cmd)
    {
    case FT_I2C_RW:
        ret = ft_rw_iic_drv_RDWR(ftdev->client, arg);   
        break; 
    default:
        ret =  -ENOTTY;
        break;
    }
//  up(&ft_rw_i2c_dev_tt->ft_rw_i2c_sem);
    //printk("++++ft_rw_iic_drv_ioctl\n");
    return ret; 
}


/*    
*char device file operation which will be put to register the char device
*/
static const struct file_operations ft_rw_iic_drv_fops = {
    .owner          = THIS_MODULE,
    .open           = ft_rw_iic_drv_open,
    .release        = ft_rw_iic_drv_release,
    .unlocked_ioctl = ft_rw_iic_drv_ioctl,
};


static void ft_rw_iic_drv_setup_cdev(struct ft_rw_i2c_dev *dev, int index)
{
    int err, devno = MKDEV(ft_rw_iic_drv_major, index);

    cdev_init(&dev->cdev, &ft_rw_iic_drv_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &ft_rw_iic_drv_fops;
    dev->client = ft_rw_i2c_dev_tt->client;
    printk("ft_rw_iic_drv_setup_cdev, client=%x\n", dev->client->addr);
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
        printk(KERN_NOTICE "Error %d adding LED%d", err, index);
}

static int ft_rw_iic_drv_myinitdev(struct i2c_client *client)
{
    int err = 0;
    dev_t devno = MKDEV(ft_rw_iic_drv_major, 0);

    if (ft_rw_iic_drv_major)
        err = register_chrdev_region(devno, 1, FT_RW_IIC_DRV);
    else {
        err = alloc_chrdev_region(&devno, 0, 1, FT_RW_IIC_DRV);
        ft_rw_iic_drv_major = MAJOR(devno);
    }
    if (err < 0) {
        dev_err(&client->dev, "%s:ft_rw_iic_drv failed  error code=%d---\n",
                __func__, err);
        return err;
    }

    ft_rw_i2c_dev_tt = kmalloc(sizeof(struct ft_rw_i2c_dev), GFP_KERNEL);
    if (!ft_rw_i2c_dev_tt){
        err = -ENOMEM;
        unregister_chrdev_region(devno, 1);
        dev_err(&client->dev, "%s:ft_rw_iic_drv failed\n",
                __func__);
        return err;
    }
    
    ft_rw_i2c_dev_tt->client = client;

    mutex_init(&ft_rw_i2c_dev_tt->ft_rw_i2c_sem);

    cdev_init(&ft_rw_i2c_dev_tt->cdev, &ft_rw_iic_drv_fops);
    ft_rw_i2c_dev_tt->client = client;
    ft_rw_i2c_dev_tt->cdev.owner = THIS_MODULE;
    ft_rw_i2c_dev_tt->cdev.ops = &ft_rw_iic_drv_fops;
     printk("%s: 2.client=%x  \n",__func__,ft_rw_i2c_dev_tt->client->addr);
    err = cdev_add(&ft_rw_i2c_dev_tt->cdev, devno, 1);
    printk("%s: client=%x  \n",__func__,ft_rw_i2c_dev_tt->client->addr);
    if (err)
        printk(KERN_NOTICE "Error %d adding LED", err);
    
    
    //ft_rw_iic_drv_setup_cdev(ft_rw_i2c_dev_tt, 0); 
    //printk("===ft_rw_iic_drv_myinitdev: client=%x \n",ft_rw_i2c_dev_tt->client);

    fts_class = class_create(THIS_MODULE, "fts_class");
    if (IS_ERR(fts_class)) {
        dev_dbg(&client->dev, "%s:failed in creating class.\n",
                __func__);
        return -1; 
    } 
    /*create device node*/
    device_create(fts_class, NULL, MKDEV(ft_rw_iic_drv_major, 0), 
            NULL, FT_RW_IIC_DRV);

    
    return 0;
}

int ft_rw_iic_drv_init(struct i2c_client *client)
{
    dev_dbg(&client->dev, "[FTS]----ft_rw_iic_drv init ---\n");
	return ft_rw_iic_drv_myinitdev(client);
}

void  ft_rw_iic_drv_exit(void)
{
	device_destroy(fts_class, MKDEV(ft_rw_iic_drv_major, 0)); 
	/*delete class created by us*/
	class_destroy(fts_class); 
	/*delet the cdev*/
	cdev_del(&ft_rw_i2c_dev_tt->cdev);
	kfree(ft_rw_i2c_dev_tt);
	unregister_chrdev_region(MKDEV(ft_rw_iic_drv_major, 0), 1); 
}

