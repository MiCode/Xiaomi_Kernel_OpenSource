/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define DEBUG
#define pr_fmt(fmt)     KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/ktime.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/poll.h>
#include "cam_log.h"

/*device name after register in charater*/
#define CAMLOG_DEV_NAME      "camlog"

static struct camlog_dev camlogdev;

static long cameralog_ioctl(struct file *filp, unsigned int cmd,
    unsigned long arg)
{
    pr_debug(" cameralog_ioctl!\n");
    return 0;
}

static int cameralog_open(struct inode *inode, struct file *filp)
{
    pr_debug(" cameralog_open!\n");
    return 0;
}

/**
*
* @return Number of bytes read.
*/
static ssize_t cameralog_read(struct file *fp, char __user *buff,
    size_t length, loff_t *ppos)
{
    ssize_t bytes_read = 0;
    int ret = 0;
    bool has_message = false;
    struct camlog_dev *camlog_dev = &camlogdev;

    if ( strlen(camlog_dev->m_camlog_message) )
    {
        sprintf(camlog_dev->m_camlog_message, "%s\n", camlog_dev->m_camlog_message);
        has_message = true;
    } else {
        has_message = false;
        strcpy(camlog_dev->m_camlog_message,"\n");
    }
    mutex_lock(&camlog_dev->camlog_message_lock);

    ret = copy_to_user(buff, camlog_dev->m_camlog_message,
                    strlen(camlog_dev->m_camlog_message));

    mutex_unlock(&camlog_dev->camlog_message_lock);

    if ((ret == 0) && has_message)
    {
         bytes_read = strlen(camlog_dev->m_camlog_message);
    }

    memset(camlog_dev->m_camlog_message, 0 , strlen(camlog_dev->m_camlog_message));

    return bytes_read;
}

void camlog_send_message(void){

    pr_debug (" camlog_send_message ss  \n");
}
EXPORT_SYMBOL(camlog_send_message);

/**
*
* @return number of bytes actually written
*/
static ssize_t cameralog_write(struct file *fp, const char *buff,
    size_t length, loff_t *ppos)
{
    int ret = 0;
    ssize_t bytes_read = 0;
    struct camlog_dev *camlog_dev = &camlogdev;

    mutex_lock(&camlog_dev->camlog_message_lock);

    ret = copy_from_user(camlog_dev->m_camlog_message, buff,
                                length);
    if (0 == ret)
    {
        bytes_read = strlen(camlog_dev->m_camlog_message);
    }

    mutex_unlock(&camlog_dev->camlog_message_lock);

    wake_up_interruptible(&camlog_dev->camlog_is_not_empty);

    return bytes_read;
}

static int cameralog_release(struct inode *inode, struct file *filp)
{
    struct camlog_dev *camlog_dev = &camlogdev;
    pr_debug(" cameralog_release!\n");
    wake_up_interruptible(&camlog_dev->camlog_is_not_empty);
    return 0;
}

static unsigned int cameralog_poll(struct file *file,
    struct poll_table_struct *poll_table)
{
    unsigned int mask = 0;
    struct camlog_dev *camlog_dev = &camlogdev;
    poll_wait(file, &camlog_dev->camlog_is_not_empty, poll_table);

    if (strlen(camlog_dev->m_camlog_message))
    {
        mask = POLLIN | POLLRDNORM;
    }

    return mask;
}

static const struct file_operations cameralog_fops =
{
    .owner = THIS_MODULE,
    .unlocked_ioctl = cameralog_ioctl,
    .open = cameralog_open,
    .release = cameralog_release,
    .poll = cameralog_poll,
    .read = cameralog_read,
    .write=cameralog_write,
};

static struct class *camlog_class;
static unsigned int camlog_major;
static int __init cameralog_init(void)
{
    struct device *device;
    struct camlog_dev *camlog_dev = &camlogdev;
    camlog_major = register_chrdev(0, CAMLOG_DEV_NAME, &cameralog_fops);
    camlog_class=class_create(THIS_MODULE, CAMLOG_DEV_NAME);
    if (IS_ERR(camlog_class)) {
         unregister_chrdev(camlog_major,  CAMLOG_DEV_NAME);
        pr_warn("Failed to create class.\n");
        return PTR_ERR(camlog_class);
    }
    camlog_dev->devt = MKDEV(camlog_major, 0);
    device = device_create(camlog_class, NULL, camlog_dev->devt,
                NULL, CAMLOG_DEV_NAME);

    if (IS_ERR(device)) {
        pr_err("error while trying to create %s\n",CAMLOG_DEV_NAME);
        return -EINVAL;
    }
    mutex_init(&camlog_dev->camlog_message_lock);
    init_waitqueue_head(&camlog_dev->camlog_is_not_empty);

    return 0;
}

module_init(cameralog_init);

static void __exit cameralog_exit(void)
{
    struct camlog_dev *camlog_dev = &camlogdev;
    device_destroy(camlog_class, camlog_dev->devt);
    class_destroy(camlog_class);
    unregister_chrdev(camlog_major, CAMLOG_DEV_NAME);
}

module_exit(cameralog_exit);

MODULE_DESCRIPTION("camera log device driver");
MODULE_LICENSE("GPL v2");
