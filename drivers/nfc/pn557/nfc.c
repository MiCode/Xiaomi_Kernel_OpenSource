/*
 * The original Work has been changed by NXP Semiconductors.
 * Copyright 2013-2019 NXP
 *
 * Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
/*
 * Copyright (C) 2010 Trusted Logic S.A.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/spinlock.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
//#include <linux/gpio/consumer.h>
//#include <linux/err.h>

#include "nfc.h"
#include "sn1xx.h"
#include "pn8xt.h"

#define MAX_BUFFER_SIZE         (512)
#define WAKEUP_SRC_TIMEOUT      (2000)
#define MAX_RETRY_COUNT          3
#define MAX_SECURE_SESSIONS      1

//#include <linux/hardware_info.h>

/*extern char 
[HARDWARE_MAX_ITEM_LONGTH];
bool reset_ntf_flag = false;*/

/*Compile time function calls based on the platform selection*/
#define platform_func(prefix, postfix) prefix##postfix
#define func(prefix, postfix) platform_func(prefix, postfix)

void nfc_disable_irq(struct nfc_dev *nfc_dev)
{
    unsigned long flags;
    spin_lock_irqsave(&nfc_dev->irq_enabled_lock, flags);
    if (nfc_dev->irq_enabled) {
        disable_irq_nosync(nfc_dev->client->irq);
        nfc_dev->irq_enabled = false;
    }
    spin_unlock_irqrestore(&nfc_dev->irq_enabled_lock, flags);
}

void nfc_enable_irq(struct nfc_dev *nfc_dev)
{
    unsigned long flags;
    spin_lock_irqsave(&nfc_dev->irq_enabled_lock, flags);
    if (!nfc_dev->irq_enabled) {
        nfc_dev->irq_enabled = true;
        enable_irq(nfc_dev->client->irq);
    }
    spin_unlock_irqrestore(&nfc_dev->irq_enabled_lock, flags);
}

static irqreturn_t nfc_dev_irq_handler(int irq, void *dev_id)
{
    struct nfc_dev *nfc_dev = dev_id;
    unsigned long flags;

    if (device_may_wakeup(&nfc_dev->client->dev))
        pm_wakeup_event(&nfc_dev->client->dev, WAKEUP_SRC_TIMEOUT);

    nfc_disable_irq(nfc_dev);
    spin_lock_irqsave(&nfc_dev->irq_enabled_lock, flags);
    nfc_dev->count_irq++;
    spin_unlock_irqrestore(&nfc_dev->irq_enabled_lock, flags);
    wake_up(&nfc_dev->read_wq);
    return IRQ_HANDLED;
}

static ssize_t nfc_dev_read(struct file *filp, char __user *buf,
        size_t count, loff_t *offset)
{
    struct nfc_dev *nfc_dev = filp->private_data;
    char tmp[MAX_BUFFER_SIZE];
    int ret;
    int irq_gpio_val = 0;
    if (!nfc_dev) {
        return -ENODEV;
    }
    if (count > MAX_BUFFER_SIZE)
        count = MAX_BUFFER_SIZE;
    pr_debug("%s: start reading of %zu bytes\n", __func__, count);
    mutex_lock(&nfc_dev->read_mutex);
    irq_gpio_val = gpio_get_value(nfc_dev->irq_gpio);
    if (irq_gpio_val == 0) {
        if (filp->f_flags & O_NONBLOCK) {
            dev_err(&nfc_dev->client->dev,
            ":f_flags has O_NONBLOCK. EAGAIN\n");
            ret = -EAGAIN;
            goto err;
        }
        while (1) {
            ret = 0;
            if (!nfc_dev->irq_enabled) {
                nfc_dev->irq_enabled = true;
                enable_irq(nfc_dev->client->irq);
            }
            if (!gpio_get_value(nfc_dev->irq_gpio)) {
                ret = wait_event_interruptible(nfc_dev->read_wq,
                    !nfc_dev->irq_enabled);
            }
            if (ret)
                goto err;
            nfc_disable_irq(nfc_dev);
            if (gpio_get_value(nfc_dev->irq_gpio))
                break;
            pr_warning("%s: spurious interrupt detected\n", __func__);
        }
    }
    memset(tmp, 0x00, count);
    /* Read data */
    ret = i2c_master_recv(nfc_dev->client, tmp, count);
    mutex_unlock(&nfc_dev->read_mutex);
    /* delay of 1ms for slow devices*/
    udelay(1000);
    if (ret < 0) {
        pr_err("%s: i2c_master_recv returned %d\n", __func__, ret);
        goto err;
    }
    if (ret > count) {
        pr_err("%s: received too many bytes from i2c (%d)\n",
                __func__, ret);
        ret = -EIO;
        goto err;
    }
    if (copy_to_user(buf, tmp, ret)) {
        pr_warning("%s : failed to copy to user space\n", __func__);
        ret = -EFAULT;
        goto err;
    }
   /*
    if(reset_ntf_flag)
    {
        memset(nfc_version,0x00,HARDWARE_MAX_ITEM_LONGTH);
        snprintf(nfc_version,HARDWARE_MAX_ITEM_LONGTH,
                       "%s FW:%.2x.%.2x.%.2x\n","PN557",tmp[6],tmp[7],tmp[8]);
        reset_ntf_flag = false;

    }
    if((0x60 == tmp[0])&&(0x00 == tmp[1])&&(0x09 == tmp[2]))
    {
        reset_ntf_flag = true;
    }
    */
    pr_debug("%s: Success in reading %zu bytes\n", __func__, count);
    return ret;
err:
    mutex_unlock(&nfc_dev->read_mutex);
    return ret;
}

static ssize_t nfc_dev_write(struct file *filp, const char __user *buf,
        size_t count, loff_t *offset)
{
    struct nfc_dev *nfc_dev = filp->private_data;
    char tmp[MAX_BUFFER_SIZE];
    int ret = 0;
    if (!nfc_dev) {
        return -ENODEV;
    }
    if (count > MAX_BUFFER_SIZE) {
        count = MAX_BUFFER_SIZE;
    }
    pr_debug("%s: start writing of %zu bytes\n", __func__, count);
    if (copy_from_user(tmp, buf, count)) {
        pr_err("%s : failed to copy from user space\n", __func__);
        return -EFAULT;
    }
    ret = i2c_master_send(nfc_dev->client, tmp, count);
    if (ret != count) {
        pr_err("%s: i2c_master_send returned %d\n", __func__, ret);
        ret = -EIO;
    }
    pr_debug("%s: Success in writing %zu bytes\n", __func__, count);
    /* delay of 1ms for slow devices*/
    udelay(1000);
    return ret;
}

/* Callback to claim the embedded secure element
 * It is a blocking call, in order to protect the ese
 * from being reset from outside when it is in use.
 */
void nfc_ese_acquire(struct nfc_dev *nfc_dev)
{
    mutex_lock(&nfc_dev->ese_status_mutex);
    pr_debug("%s: ese acquired\n", __func__);
}

/* Callback to release the  embedded secure element
 * it should be released, after completion of any
 * operation (usage or reset) of ese.
 */
void nfc_ese_release(struct nfc_dev *nfc_dev)
{
    mutex_unlock(&nfc_dev->ese_status_mutex);
    pr_debug("%s: ese released\n", __func__);
}

static void nfc_init_stat(struct nfc_dev *nfc_dev)
{
    nfc_dev->count_irq = 0;
}

static int nfc_dev_open(struct inode *inode, struct file *filp)
{
    int ret = 0;
    struct nfc_dev *nfc_dev = container_of(filp->private_data,
            struct nfc_dev, nfc_device);

    filp->private_data = nfc_dev;
    nfc_init_stat(nfc_dev);
    pr_info("%s: %d,%d\n", __func__, imajor(inode), iminor(inode));
    return ret;
}

long nfc_dev_ioctl(struct file *filep, unsigned int cmd,
        unsigned long arg)
{
    long ret = 0;
    struct nfc_dev *nfc_dev = filep->private_data;
    ret = func(NFC_PLATFORM, _nfc_ioctl)(nfc_dev, cmd, arg);
    if (ret != 0)
        pr_err("%s: ioctl: cmd = %u, arg = %lu\n", __func__, cmd, arg);
    return ret;
}

static const struct file_operations nfc_dev_fops = {
        .owner  = THIS_MODULE,
        .llseek = no_llseek,
        .read   = nfc_dev_read,
        .write  = nfc_dev_write,
        .open   = nfc_dev_open,
        .unlocked_ioctl  = nfc_dev_ioctl,
};

struct nfc_platform_data {
    unsigned int irq_gpio;
    unsigned int ven_gpio;
    unsigned int firm_gpio;
    unsigned int ese_pwr_gpio;
//    struct pinctrl *pinctrl;
//    struct pinctrl_state *req_clk;

};

static int nfc_parse_dt(struct device *dev,
    struct nfc_platform_data *data)
{
    int ret = 0;
    struct device_node *np = dev->of_node;

    data->irq_gpio = of_get_named_gpio(np, "nxp,pn557-irq", 0);
    if ((!gpio_is_valid(data->irq_gpio)))
            return -EINVAL;

    data->ven_gpio = of_get_named_gpio(np, "nxp,pn557-ven", 0);
    if ((!gpio_is_valid(data->ven_gpio)))
            return -EINVAL;

    data->firm_gpio = of_get_named_gpio(np, "nxp,pn557-fw-dwnld", 0);
    if ((!gpio_is_valid(data->firm_gpio)))
            return -EINVAL;

    //required for old platform only
    /*data->ese_pwr_gpio = of_get_named_gpio(np, "nxp,pn557-ese-pwr", 0);
    if ((!gpio_is_valid(data->ese_pwr_gpio)))
        data->ese_pwr_gpio =  -EINVAL;*/
/*  
    data->pinctrl = devm_pinctrl_get(dev);
    if (IS_ERR_OR_NULL(data->pinctrl)) {
        printk("%s, No pinctrl config specified\n", __func__);
        return -EINVAL;
    }

    data->req_clk = pinctrl_lookup_state(data->pinctrl, "default_req");
    if (IS_ERR_OR_NULL(data->req_clk)) {
        printk("%s, could not get pin_suspend\n", __func__);
        return -EINVAL;
    }
    pinctrl_select_state(data->pinctrl, data->req_clk);
*/  
    printk("%s: %d, %d, %d, %d\n", __func__,
                data->irq_gpio, data->ven_gpio, data->firm_gpio, ret);
    return ret;
}

static int nfc_probe(struct i2c_client *client,
        const struct i2c_device_id *id)
{
    int ret = 0;
    int irqn = 0;
    struct nfc_platform_data platform_data;
    struct nfc_dev *nfc_dev;
    pr_debug("%s: enter\n", __func__);

    ret = nfc_parse_dt(&client->dev, &platform_data);
    if (ret) {
        pr_err("%s : failed to parse\n", __func__);
        goto err;
    }

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        pr_err("%s : need I2C_FUNC_I2C\n", __func__);
        ret = -ENODEV;
        goto err;
    }
    nfc_dev = kzalloc(sizeof(*nfc_dev), GFP_KERNEL);
    if (nfc_dev == NULL) {
        ret = -ENOMEM;
        goto err;
    }
    nfc_dev->client = client;
    if (gpio_is_valid(platform_data.ven_gpio)) {
        ret = gpio_request(platform_data.ven_gpio, "nfc_reset_gpio");
        if (ret) {
            pr_err("%s: unable to request nfc reset gpio [%d]\n",
                        __func__, platform_data.ven_gpio);
            goto err_mem;
        }
        ret = gpio_direction_output(platform_data.ven_gpio, 0);
        if (ret) {
            pr_err("%s: unable to set direction for nfc reset gpio [%d]\n",
                        __func__, platform_data.ven_gpio);
            goto err_en_gpio;
        }
    } else {
        pr_err("%s: nfc reset gpio not provided\n", __func__);
        goto err_mem;
    }
    if (gpio_is_valid(platform_data.irq_gpio)) {
        ret = gpio_request(platform_data.irq_gpio, "nfc_irq_gpio");
        if (ret) {
            pr_err("%s: unable to request nfc irq gpio [%d]\n",
                        __func__, platform_data.irq_gpio);
            goto err_en_gpio;
        }
        ret = gpio_direction_input(platform_data.irq_gpio);
        if (ret) {
            pr_err("%s: unable to set direction for nfc irq gpio [%d]\n",
                        __func__, platform_data.irq_gpio);
            goto err_irq_gpio;
        }
        irqn = gpio_to_irq(platform_data.irq_gpio);
        if (irqn < 0) {
            ret = irqn;
            goto err_irq_gpio;
        }
        client->irq = irqn;
    } else {
        pr_err("%s: irq gpio not provided\n", __func__);
        goto err_en_gpio;
    }
    if (gpio_is_valid(platform_data.firm_gpio)) {
        ret = gpio_request(platform_data.firm_gpio, "nfc_firm_gpio");
        if (ret) {
            pr_err("%s: unable to request nfc firmware gpio [%d]\n",
                        __func__, platform_data.firm_gpio);
            goto err_irq_gpio;
        }
        ret = gpio_direction_output(platform_data.firm_gpio, 0);
        if (ret) {
            pr_err("%s: cannot set direction for nfc firmware gpio [%d]\n",
                    __func__, platform_data.firm_gpio);
            goto err_firm_gpio;
        }
    } else {
        pr_err("%s: firm gpio not provided\n", __func__);
        goto err_irq_gpio;
    }

    nfc_dev->ven_gpio = platform_data.ven_gpio;
    nfc_dev->irq_gpio = platform_data.irq_gpio;
    nfc_dev->firm_gpio  = platform_data.firm_gpio;
    /* init mutex and queues */
    init_waitqueue_head(&nfc_dev->read_wq);
    mutex_init(&nfc_dev->read_mutex);
    mutex_init(&nfc_dev->ese_status_mutex);
    spin_lock_init(&nfc_dev->irq_enabled_lock);

    nfc_dev->nfc_device.minor = MISC_DYNAMIC_MINOR;
    nfc_dev->nfc_device.name = "pn557";
    nfc_dev->nfc_device.fops = &nfc_dev_fops;

    ret = misc_register(&nfc_dev->nfc_device);
    if (ret) {
        pr_err("%s: misc_register failed\n", __func__);
        goto err_misc_register;
    }
    /* NFC_INT IRQ */
    nfc_dev->irq_enabled = true;
    ret = request_irq(client->irq, nfc_dev_irq_handler,
            IRQF_TRIGGER_HIGH, client->name, nfc_dev);
    if (ret) {
        pr_err("request_irq failed\n");
        goto err_request_irq_failed;
    }
    device_init_wakeup(&client->dev, true);
    device_set_wakeup_capable(&client->dev, true);

    nfc_dev->irq_wake_up = false;

    i2c_set_clientdata(client, nfc_dev);
    /*Enable IRQ and VEN*/
    //nfc_enable_irq(nfc_dev);
    nfc_disable_irq(nfc_dev);
    /*call to platform specific probe*/
    ret = func(NFC_PLATFORM, _nfc_probe)(nfc_dev);
    if (ret != 0) {
        pr_err("%s: probing platform failed\n", __func__);
        goto err_request_irq_failed;
    };
    pr_info("%s: probing NXP NFC exited successfully\n", __func__);
    return 0;

err_request_irq_failed:
    misc_deregister(&nfc_dev->nfc_device);
err_misc_register:
    mutex_destroy(&nfc_dev->read_mutex);
    mutex_destroy(&nfc_dev->ese_status_mutex);
err_firm_gpio:
    gpio_free(platform_data.firm_gpio);
err_irq_gpio:
    gpio_free(platform_data.irq_gpio);
err_en_gpio:
    gpio_free(platform_data.ven_gpio);
err_mem:
    kfree(nfc_dev);
err:
    pr_err("%s: probing NXP NFC driver failed, check hardware\n", __func__);
    return ret;
}

static int nfc_remove(struct i2c_client *client)
{
    int ret = 0;
    struct nfc_dev *nfc_dev;
    pr_info("%s: remove device\n", __func__);
    nfc_dev = i2c_get_clientdata(client);
    if (!nfc_dev) {
        pr_err("%s: device doesn't exist anymore\n", __func__);
        ret = -ENODEV;
        goto err;
    }
    /*call to platform specific remove*/
    ret = func(NFC_PLATFORM, _nfc_remove)(nfc_dev);
    if (ret != 0) {
        pr_err("%s: platform failed\n", __func__);
        goto err;
    }
    free_irq(client->irq, nfc_dev);
    misc_deregister(&nfc_dev->nfc_device);
    mutex_destroy(&nfc_dev->read_mutex);
    mutex_destroy(&nfc_dev->ese_status_mutex);
    gpio_free(nfc_dev->ese_pwr_gpio);
    gpio_free(nfc_dev->firm_gpio);
    gpio_free(nfc_dev->irq_gpio);
    gpio_free(nfc_dev->ven_gpio);
    kfree(nfc_dev);
err:
    return ret;
}

static int nfc_suspend(struct device *device)
{
    struct i2c_client *client = to_i2c_client(device);
    struct nfc_dev *nfc_dev = i2c_get_clientdata(client);

    pr_info("%s\n", __func__);
    //if (device_may_wakeup(&client->dev)) {
    if (device_may_wakeup(&client->dev) && nfc_dev->irq_enabled) {
        if (!enable_irq_wake(client->irq))
            nfc_dev->irq_wake_up = true;
            pr_info("%s enable irq wake \n", __func__);
    }
    return 0;
}

static int nfc_resume(struct device *device)
{
    struct i2c_client *client = to_i2c_client(device);
    struct nfc_dev *nfc_dev = i2c_get_clientdata(client);

    pr_info("%s\n", __func__);
    if (device_may_wakeup(&client->dev) && nfc_dev->irq_wake_up) {
        if (!disable_irq_wake(client->irq))
            nfc_dev->irq_wake_up = false;
            pr_info("%s disable irq wake \n", __func__);
    }
    return 0;
}

static const struct dev_pm_ops nfc_pm_ops = {
    SET_SYSTEM_SLEEP_PM_OPS(nfc_suspend, nfc_resume)
};

static const struct i2c_device_id nfc_id[] = {
        { "pn557", 0 },
        { }
};

static struct of_device_id nfc_match_table[] = {
    {.compatible = "nxp,pn557",},
    {}
};
MODULE_DEVICE_TABLE(of, nfc_match_table);

static struct i2c_driver nfc_driver = {
        .id_table   = nfc_id,
        .probe      = nfc_probe,
        .remove     = nfc_remove,
        .driver     = {
                .owner = THIS_MODULE,
                .name  = "pn557",
                .of_match_table = nfc_match_table,
        .pm = &nfc_pm_ops,
        },
};

static int __init nfc_dev_init(void)
{
    if(strstr(saved_command_line,"androidboot.product.hardware.sku=disabled")){
        pr_info("not support nfc pn557\n");
        return -1;
    }else{
        pr_info("Loading NXP NFC driver\n");
        return i2c_add_driver(&nfc_driver);
    }
}
module_init(nfc_dev_init);

static void __exit nfc_dev_exit(void)
{
    pr_info("Unloading NXP NFC driver\n");
    i2c_del_driver(&nfc_driver);
}
module_exit(nfc_dev_exit);

MODULE_DESCRIPTION("NXP NFC driver");
MODULE_LICENSE("GPL");


