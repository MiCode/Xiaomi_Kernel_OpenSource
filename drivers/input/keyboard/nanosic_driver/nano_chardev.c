/** ***************************************************************************
 * @file nano_cdev.c
 *
 * @brief Communicate with user space through character device driver
 *        Create /dev/nanodev
 *
 * <em>Copyright (C) 2010, Nanosic, Inc.  All rights reserved.</em>
 * Author : Bin.yuan bin.yuan@nanosic.com
 * */

/*
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/kthread.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include "nano_macro.h"
#include <linux/uaccess.h>
#include <linux/sched/signal.h>

/*buffer节点默认缓冲个数*/
#define NANODEV_BUFSIZE   (10)

/*buffer节点*/
typedef struct{
    int     datalen;
    uint8_t data[I2C_DATA_LENGTH_READ];
}Q_buffer_t;

/*client结构体*/
typedef struct nano_client_s {
    Q_buffer_t buffer[10];
    uint8_t read_pos;
    uint8_t write_pos;
    bool    exit;
    spinlock_t spinlock;
    struct mutex read_mutex;
    struct list_head registry_node;
    wait_queue_head_t waitq;
}nanosic_client_t;

//static struct task_struct *task;
static bool initial = false;
static struct cdev chrdev;
static struct device *d;

static struct   class *nanosic_cdev_class;
static dev_t    nanosic_cdev_basedev;
#define   CHARDEV_MAX_DEVICES       1

/*已open()的客户端列表,支持多应用open()*/
static LIST_HEAD(client_registry);
static DEFINE_SPINLOCK(client_registry_lock);

#define client_of(__n) container_of(__n, nanosic_client_t,registry_node)

/** **************************************************************************
 * @brief Handler for register new client struct
 *
 ** */
static int
Nanosic_chardev_client_register(nanosic_client_t* client)
{
    struct list_head *node;
    int found = 0;
    int ret=0;

    if(IS_ERR(client))
        return -EINVAL;

    spin_lock(&client_registry_lock);

    list_for_each(node,&client_registry)
    {
        if (client_of(node) == client)
        {
            found = 1;
            break;
        }
    }
    /* Step 2: Link in if not there yet */
    if (found) {
        dbgprint(ERROR_LEVEL,"already registered %p\n",client);
        ret = -EEXIST;
        goto _exit;
    }

    dbgprint(DEBUG_LEVEL,"Registering new client %p\n",client);
    list_add_tail(&client->registry_node,&client_registry);

_exit:
    spin_unlock(&client_registry_lock);

    return ret;
}

/** **************************************************************************
 * @brief Handler for deregister client struct
 *
 ** */
static int
Nanosic_chardev_client_deregister(nanosic_client_t* client)
{
    struct list_head *node;
    int found = 0;
    int ret = 0;

    if(IS_ERR(client))
        return -EINVAL;

    spin_lock(&client_registry_lock);

    list_for_each(node,&client_registry)
    {
        if (client_of(node) == client)
        {
            found = 1;
            break;
        }
    }

    /* Step 2: Unlink if found */
    if (!found){
        dbgprint(DEBUG_LEVEL,"Can't find registered client %p\n",client);
        ret = -ENODEV;
        goto _exit;
    }

    dbgprint(DEBUG_LEVEL,"Deregistering client %p\n",client);
    list_del(&client->registry_node);
    kfree(client);

_exit:
    spin_unlock(&client_registry_lock);

    return ret;
}

/** **************************************************************************
 * @brief Handler for notify to nanoapp application
 * write data to framework
 ** */
int
Nanosic_chardev_client_write(char* data, size_t datalen)
{

    struct list_head *node;
    nanosic_client_t* client = NULL;
    int ret = 0;
    unsigned long flags;
    __u8 pos;
    int length;

    if(!data || datalen <=0 )
        return -1;

    if(!initial)
        return -1;

    spin_lock_irqsave(&client_registry_lock,flags);

    list_for_each(node,&client_registry)
    {
        client = client_of(node);
        if(IS_ERR(client))
            continue;
        pos = (client->write_pos + 1) % NANODEV_BUFSIZE;
        dbgprint(DEBUG_LEVEL, "current write_pos %d\n",pos);
        if (pos != client->read_pos) {
            length = datalen > I2C_DATA_LENGTH_READ ? I2C_DATA_LENGTH_READ : datalen;
            memcpy(client->buffer[pos].data , data , length);
            client->buffer[pos].datalen = length;
            dbgprint(DEBUG_LEVEL, "current length %d\n",length);
            client->write_pos = pos;
            wake_up_interruptible(&client->waitq);
            //dbgprint(DEBUG_LEVEL,"update write_pos %d\n",client->write_pos);
        } else {
            dbgprint(DEBUG_LEVEL, "Output queue is full\n");
        }
    }

    spin_unlock_irqrestore(&client_registry_lock,flags);

    return ret;
}

/** **************************************************************************
 * @brief Handler for try to notify nanoapp close handle
 *
 ** */
int
Nanosic_chardev_client_notify_close(void)
{
    struct list_head *node;
    nanosic_client_t* client = NULL;
    int index=0;
    unsigned long flags;

    if(!initial)
        return -1;

    dbgprint(ALERT_LEVEL, "notify client begin...\n");

    spin_lock_irqsave(&client_registry_lock,flags);

    list_for_each(node,&client_registry)
    {
        client = client_of(node);
        if(IS_ERR(client))
            continue;
        dbgprint(ALERT_LEVEL, "[%d]notify client %p close handle\n",index++,client);
        client->exit = true;
        wake_up_interruptible(&client->waitq);
    }

    spin_unlock_irqrestore(&client_registry_lock,flags);

    return 0;
}

/** **************************************************************************
 * @brief Handler for userspace file open() request
 *
 * @details This function is called when a userspace process opens our device
 * file for access.
 *
 * @param inode The inode of the file that was opened
 * @param file The file struct that describes the opened file
 *
 * @remarks We take an extra kref to our module to prevent unload while a
 * process has a file open.
 ** */
static int
Nanosic_chardev_fops_open(
    struct inode *inode,
    struct file  *file
)
{
    nanosic_client_t *client = NULL;

    client = kzalloc(sizeof(nanosic_client_t),GFP_KERNEL);
    if(IS_ERR(client))
        return -ENOMEM;

    client->read_pos = 0;
    client->write_pos = 0;
    client->exit = false;
    spin_lock_init(&client->spinlock);
    init_waitqueue_head(&client->waitq);
    mutex_init(&client->read_mutex);
    Nanosic_chardev_client_register(client);

    file->private_data = (void *)client;

    try_module_get(THIS_MODULE);
    dbgprint(ALERT_LEVEL,"Nanosic_chardev_fops_open\n");

    return 0;
}

/** ***************************************************************************
 * @brief Handler for userspace file poll() request
 *
 * @details This function is called when a userspace process poll our device
 * file for access.
 *
 * @param inode The inode of the file that was opened
 * @param file The file struct that describes the opened file
 *
 * @remarks We take an extra kref to our module to prevent unload while a
 * process has a file open.
 ** */

static unsigned int
Nanosic_chardev_fops_poll (struct file *file, struct poll_table_struct *wait){
	return -ENOSYS;
}

/** ***************************************************************************
 * @brief Handle userspace read() requests
 *
 * @details This function is called when a userspace process calls read()
 * on one of our device files.  We attempt to satisfy the read from the 
 * buffers that are currently queued.
 *
 * @param file Structure describing file to use
 * @param user User data buffer
 * @param size Number of octets to copy to <user>
 * @param loff (OUT) Offset marker, updated to new offset after read
 *
 * @returns Result code indicating success or failure mode
 *
 * @remarks We never advance the file offset, since we're a pipe-style device
 * @remarks In general, we'll block until we can satisfy the request.  The only
 * exceptions are if the non-blocking flag is set, if we get interrupted, if
 * we hit a copy error, or we encounter a partially-full buffer (i.e. offset +
 * datalen < bufsz).  In those cases, we'll either return the amount of data
 * already read, or an error value.
 ** */
static ssize_t
Nanosic_chardev_fops_read(
    struct file *file,
    char __user *user,
    size_t size,
    loff_t *loff
)
{
    Q_buffer_t* buf = NULL;
    int retval = 0;
    uint8_t read_pos;
    int length;

    nanosic_client_t* client = (nanosic_client_t*)file->private_data;
    if(IS_ERR(client))
        return -EFAULT;

    /*一个client同一时刻只支持一个thread去read process*/
    mutex_lock(&client->read_mutex);

    /*默认为阻塞式read , 不支持非阻塞模式 , 防止被信号打断这里不能用wait_event_interruptible*/
#if 1
    retval = wait_event_interruptible(client->waitq,((client->read_pos != client->write_pos) || client->exit == true));
    if (signal_pending(current)) {
        retval = -ERESTARTSYS;
        dbgprint(ALERT_LEVEL,"wake up by restart signal %d\n",retval);
        mutex_unlock(&client->read_mutex);
        return retval;
    }

    dbgprint(DEBUG_LEVEL,"retval %d\n",retval);
#else
    wait_event(client->waitq,(client->read_pos != client->write_pos));
#endif
    spin_lock(&client_registry_lock);

    if(client->exit == true){
        dbgprint(ALERT_LEVEL,"wake up by exit signal\n");
        retval = -ENODATA;
        goto _out;
    }

    read_pos = (client->read_pos + 1) % NANODEV_BUFSIZE;

    dbgprint(DEBUG_LEVEL,"current read_pos %d\n",read_pos);

    buf = &client->buffer[read_pos];

    if(buf->datalen <= 0)
        goto _out;

    dbgprint(DEBUG_LEVEL,"datalen %d , size %zu\n",buf->datalen , size);

    length = buf->datalen > size ? size : buf->datalen;
    dbgprint(DEBUG_LEVEL,"length %d\n",length);

    if(copy_to_user(user,buf->data,length)){
        /*Partial buffer consumption*/
        retval = -EFAULT;
        goto _out;
    }

    client->read_pos = read_pos;

    retval = length;

_out:
    spin_unlock(&client_registry_lock);
    /*size_t 使用%zu进行打印*/
    dbgprint(DEBUG_LEVEL,"retval %d\n",retval);

    mutex_unlock(&client->read_mutex);
    
	return retval;
}

/** ***************************************************************************
 * @brief Handle userspace write() requests
 *
 * @details This function is called when a userspace process calls write()
 * on one of our device files.  We convert this to an albrx_device_send() and
 * pass it down.
 *
 * @param file Structure describing file to use
 * @param user User data buffer
 * @param size Number of octets in <user> to write
 * @param loff (OUT) Offset marker, updated to new offset after write
 *
 * @returns Result code indicating success or failure mode
 *
 * @remarks We never advance the file offset, since we are all pipe-style.
 ** */
static ssize_t
Nanosic_chardev_fops_write(
    struct file *file,
    const char __user *user,
    size_t size,
    loff_t *loff
)
{
    char data[I2C_DATA_LENGTH_WRITE]={0};
    int  ret = 0;
    unsigned long copy = 0;
    nanosic_client_t* client = (nanosic_client_t*)file->private_data;
    if(IS_ERR(client))
        return -EINVAL;
    
    copy = min(size,(size_t)I2C_DATA_LENGTH_WRITE);
    
    if(copy_from_user(data, user, copy)){
        return -EFAULT;
    }

    rawdata_show("write cmd",data,copy);

    if(copy != I2C_DATA_LENGTH_WRITE)
        return copy;

    if((data[0]==0x32) && (data[1]==0x00) && (data[2]==0x4F) && (data[3]==0x20) && (data[4]==0x80) && (data[5]==0x80) && (data[6]==0xE1) && (data[7]==0x01) && (data[8]==0x00)){
        dbgprint(ERROR_LEVEL,"receive I2C hall read message !\n");
        ret = Nanosic_Hall_notify(gpio_hall_n_pin, gpio_hall_s_pin);
    }else if((data[0]==0x32) && (data[2]==0x4F) && (data[3]==0x30) && (data[4]==0x80) && (data[5]==0x18) && (data[6]==0x1D))
    {
        ret = Nanosic_GPIO_recovery(gI2c_client,data,copy);
    }else{
        ret = Nanosic_i2c_write(gI2c_client,data,copy);
    }
    
	return ret;
}

/** ***************************************************************************
 * @brief Handler for userspace file close
 *
 * @details This function is called when a userspace process has finished 
 * reading from or writing do our device file, and closes the descriptor.
 *
 * @param inode The inode of the opened device file
 * @param file  The file structure containing context for the open file
 *
 * @returns Result code indicating success or failure mode
 *
 * @remarks
 ** */
static int
Nanosic_chardev_fops_release(
    struct inode *inode,
    struct file  *file
)
{
    nanosic_client_t* client = (nanosic_client_t*)file->private_data;
    if(IS_ERR(client))
        return -EINVAL;

    Nanosic_chardev_client_deregister(client);

    module_put(THIS_MODULE);

    file->private_data = NULL;

    dbgprint(ALERT_LEVEL,"Nanosic_chardev_fops_release\n");

    return 0;
}

/** ***************************************************************************
 * @brief Handler for ioctl call
 *
 ** */
static long
Nanosic_chardev_fops_ioctl(struct file *file, unsigned int cmd, unsigned long args)
{
	return -ENOSYS;
}

/** ************************************************************************//**
 * @brief Handle device node mods during /dev/nanodev file create
 *
 * @details This function is called by the Linux driver core during class device
 * creation, and can be used to dynamically rename or change permissions of a
 * file in /dev.
 *
 * @param dev Pointer to the device to be named
 * @param modep Pointer to a mode object that can be modified
 *
 * @returns NULL; Usually we'd return a modified name, but we like the name
 * that we gave it during class creation.
 ** */
static char *
Nanosic_chardev_devnode(
    struct device *dev,
    umode_t *mode
)
{
    dbgprint(ALERT_LEVEL,"change devnode\n");

    /* Step 1: Make our node globally writable for testing */
    if (mode)
    {
        dbgprint(ALERT_LEVEL,"change devnode to 666\n");
        *mode = 0666;
    }

    return NULL;
}

/** ************************************************************************//**
 *  @brief a sets of file operations method
 *
 ** */
static const struct file_operations nanosic_cdev_fops = {
    .owner = THIS_MODULE,
    .llseek  = no_llseek, /* We're a pipe, dude. */
    .open    = Nanosic_chardev_fops_open,
    .release = Nanosic_chardev_fops_release,
    .write   = Nanosic_chardev_fops_write,
    .read    = Nanosic_chardev_fops_read,
    .compat_ioctl   = Nanosic_chardev_fops_ioctl,
    .poll    = Nanosic_chardev_fops_poll,
};

/** ************************************************************************//**
 *  @brief Invoke device_create interface to generate device node in system
 *
 ** */
static int
Nanosic_chardev_claim(void)
{
    //struct file* filp;
    //struct inode* inode;

    cdev_init(&chrdev,&nanosic_cdev_fops);

    if (cdev_add(&chrdev, (nanosic_cdev_basedev + 0), 1))
    {
        dbgprint(ERROR_LEVEL,"Cannot add character device.\n");
        return -1;
    }

    d = device_create(nanosic_cdev_class, /* Owning class */
                      NULL,
                      nanosic_cdev_basedev + 0,  /* dev_t */
                      (void *)NULL,              /* drvdata for callbacks */
                      "%s%d", DRV_TOKEN, 0);     /* devname */
    if (IS_ERR(d))
    {
        dbgprint(ERROR_LEVEL,"Cannot create nanosic device.\n");
        goto _alcli_claim_fail_1;
    }

#if 0 /*修改默认的权限*/
    filp=filp_open("/dev/nanodev0", O_RDONLY|O_CREAT, 0);
    if(!IS_ERR(filp)){
        inode = filp->f_dentry->d_inode;
        inode->i_mode |= 0660;
        filp_close(filp, NULL);
    }
#endif

    /*initializa sysfs node */
    Nanosic_sysfs_create(d);

    return 0;

    /* Step 5: Handle any and all errors */
_alcli_claim_fail_1:
    cdev_del(&chrdev);
    return -1;
}

/** ************************************************************************//**
 *  @brief
 *  Create the /dev/nanodev char device node.
 *  Create the /sys/class/nanodev/nanodev0/debuglevel sysfs node.
 ** */
int
Nanosic_chardev_register(void)
{
    int ret;

    /* Step 1: Setup character device class */
    /* Step 1.1: Try to get a char device region */
    if (0 > alloc_chrdev_region(&nanosic_cdev_basedev,
        0,                      /* Dynamic alloc of major number */
        CHARDEV_MAX_DEVICES,    /* number of minors */
        DRV_TOKEN))
    {
        dbgprint(ERROR_LEVEL,"Failed to setup device number range for chardev registration\n");
        return -1;
    }

    /* Step 1.2: Create the class */
    if (NULL != (nanosic_cdev_class  = class_create(THIS_MODULE, DRV_TOKEN))){
        nanosic_cdev_class->devnode   = Nanosic_chardev_devnode;
    }

    ret = Nanosic_chardev_claim();
    if(ret < 0)
        return -1;

    initial = true;

    dbgprint(ALERT_LEVEL,"chardev create ok!\n");

    return 0;
}
//EXPORT_SYMBOL_GPL(Nanosic_chardev_init);

/** ************************************************************************//**
 *  @brief
 *  Release the char device node
 *  Release the sysfd node
 */
void
Nanosic_chardev_release(void)
{
    dbgprint(ALERT_LEVEL,"chardev release begin!\n");

    if(initial == false)
        return;

    /*通知nanoapp尝试close handle*/
    //Nanosic_chardev_client_notify_close();

    /*延时500ms*/
    //mdelay(500);

    if(!IS_ERR(d))
    {
        /*release sysfs node */
        Nanosic_sysfs_release(d);
    }

    /* Step 1: Tear down our char device */
    device_destroy(nanosic_cdev_class, nanosic_cdev_basedev + 0);
    cdev_del(&chrdev);

    /* Step 2: Class/device cleanup */
    class_destroy(nanosic_cdev_class);
    nanosic_cdev_class = NULL;
    unregister_chrdev_region(nanosic_cdev_basedev,CHARDEV_MAX_DEVICES);

    initial = false;

    dbgprint(ALERT_LEVEL,"chardev release ok!\n");
}
//EXPORT_SYMBOL_GPL(Nanosic_chardev_exit);
