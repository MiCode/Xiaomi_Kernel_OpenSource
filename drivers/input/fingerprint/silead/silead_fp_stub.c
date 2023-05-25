/*
 * @file   silead_fp_stub.c
 * @brief  Contains silead_fp device implements for platform stub.
 *
 *
 * Copyright 2016-2021 Gigadevice/Silead Inc.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 *
 * ------------------- Revision History ------------------------------
 * <author>    <date>   <version>     <desc>
 * Melvin cao  2019/3/18    0.1.0      Init version
 * Bill Yu     2019/7/18    0.1.1      android 7 compatible
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/ioctl.h>
#include <asm/uaccess.h>
#include "silead_fp_stub.h"

#ifdef BSP_SIL_DYNAMIC_SPI

//module_param(sil_stub_major, int, S_IRUGO);
struct sil_stub_dev *sil_stub_devp = NULL;
struct class *sil_stub_class;

/* debug log level */
extern fp_debug_level_t sil_debug_level;

extern int silfp_dev_init(void);
extern void silfp_dev_exit(void);

static int sil_stub_open(struct inode *inode, struct file *filp)
{
    filp->private_data = sil_stub_devp;
    return 0;
}

static int sil_stub_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static long sil_stub_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    unsigned long flags;
    struct sil_stub_dev	*stub_dev;
    long err = 0, ret = 0;

    if(_IOC_TYPE(cmd) != SIFP_IOC_MAGIC)
        return -EINVAL;

    if(_IOC_NR(cmd) > SIL_STUB_MAXNR)
        return -EINVAL;

    if(_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE, (void*)arg, _IOC_SIZE(cmd));
    else if(_IOC_DIR(cmd) & _IOC_WRITE)
        err = !access_ok(VERIFY_READ, (void*)arg, _IOC_SIZE(cmd));

    stub_dev = filp->private_data;
    LOG_MSG_DEBUG(INFO_LOG, "[%s] send cmd %d\n", __func__, cmd);
    switch(cmd) {
    case SIL_STUB_IOCINIT:
        spin_lock_irqsave(&stub_dev->lock, flags);
        if (stub_dev->fp_init) {
            spin_unlock_irqrestore(&stub_dev->lock, flags);
            LOG_MSG_DEBUG(INFO_LOG, "[%s] silead_fp already inited\n", __func__);
            break;
        } else {
            stub_dev->fp_init = 1;
            spin_unlock_irqrestore(&stub_dev->lock, flags);
        }

        ret = silfp_dev_init();
        if (ret < 0) {
            spin_lock_irqsave(&stub_dev->lock, flags);
            stub_dev->fp_init = 0;
            spin_unlock_irqrestore(&stub_dev->lock, flags);
        }
        break;
    case SIL_STUB_IOCDEINIT:
        spin_lock_irqsave(&stub_dev->lock, flags);
        if (!stub_dev->fp_init) {
            spin_unlock_irqrestore(&stub_dev->lock, flags);
            LOG_MSG_DEBUG(INFO_LOG, "[%s] silead_fp already released\n", __func__);
            break;
        } else {
            stub_dev->fp_init = 0;
            spin_unlock_irqrestore(&stub_dev->lock, flags);
        }
        silfp_dev_exit();
        break;
    default:
        return -EINVAL;
    }
    return ret;
}

#ifdef CONFIG_COMPAT
static long
sil_stub_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    return sil_stub_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define sil_stub_compat_ioctl NULL
#endif /* CONFIG_COMPAT */

struct file_operations sil_stub_fops = {
    .owner   = THIS_MODULE,
    .open    = sil_stub_open,
    .release = sil_stub_release,
    .unlocked_ioctl = sil_stub_ioctl,
    .compat_ioctl   = sil_stub_compat_ioctl,
};

static int __init silead_stub_init(void)
{
    int ret = 0;

    if (!sil_stub_devp) {
        sil_stub_devp = kzalloc(sizeof(struct sil_stub_dev), GFP_KERNEL);
        if (!sil_stub_devp) {
            return -ENOMEM;
        }
    }

    if(SIL_STUB_MAJOR > 0) {
        sil_stub_devp->devt = MKDEV(SIL_STUB_MAJOR, 0);
        ret = register_chrdev_region(sil_stub_devp->devt, 1, FP_STUB_DEV_NAME);
    } else {
        ret = alloc_chrdev_region(&sil_stub_devp->devt, 0, 1, FP_STUB_DEV_NAME);
    }

    if(ret < 0) {
        return ret;
    }

    cdev_init(&sil_stub_devp->cdev, &sil_stub_fops);
    sil_stub_devp->cdev.owner = THIS_MODULE;
    sil_stub_devp->cdev.ops   = &sil_stub_fops;

    cdev_add(&sil_stub_devp->cdev, sil_stub_devp->devt, 1);

    sil_stub_class = class_create(THIS_MODULE, FP_STUB_CLASS_NAME);
    device_create(sil_stub_class, NULL,sil_stub_devp->devt, NULL, FP_STUB_DEV_NAME);
    spin_lock_init(&sil_stub_devp->lock);

    return ret;
}

static void __exit silead_stub_exit(void)
{
    cdev_del(&sil_stub_devp->cdev);
    unregister_chrdev_region(sil_stub_devp->devt, 1);
    device_destroy(sil_stub_class, sil_stub_devp->devt);
    class_destroy(sil_stub_class);
    kfree(sil_stub_devp);
    sil_stub_devp = NULL;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Melvin cao");
module_init(silead_stub_init);
module_exit(silead_stub_exit);

#endif /* BSP_SIL_DYNAMIC_SPI */
