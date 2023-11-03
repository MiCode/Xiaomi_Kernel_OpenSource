/*****************************************************************************************
 * Copyright (c)  2021- 2022  TsingTeng MicroSystem Corp.Ltd.
 * TMS_EDIT
 * File       : nfc_driver.c
 * Description: Source file for tms nfc driver
 * Version    : 1.0
 * Date       : 2022/4/11
 * Team       : NFC Middleware
 * Author     : Guoliang Wu
 * --------------------------------- Revision History: ---------------------------------
 *   <version>    <date>          < author >                            <desc>
 *******************************************************************************************/
#include "nfc_driver.h"

/*********** PART0: Global Variables Area ***********/
size_t last_count = 0;
static ktime_t g_pre_write_time;

/*********** PART1: Callback Function Area ***********/
static int irq_trigger_check(struct nfc_info *nfc)
{
    int value;
    value = gpio_get_value(nfc->hw_res.irq_gpio);

    if (value == 1) {
        value = 1; //means irq is triggered
    } else {
        value = 0; //means irq is not triggered
    }

    TMS_DEBUG("State = %d\n", value);
    return value;
}

static int i2c_master_block_recv(struct nfc_info *nfc, uint8_t *buf, size_t count,
                          int timeout)
{
    int ret;
    TMS_DEBUG("Start+\n");

    if (timeout > NFC_CMD_RSP_TIMEOUT_MS) {
        timeout = NFC_CMD_RSP_TIMEOUT_MS;
    }

    if (!gpio_get_value(nfc->hw_res.irq_gpio)) {
        while (1) {
            ret = SUCCESS;
            nfc_enable_irq(nfc);

            if (!gpio_get_value(nfc->hw_res.irq_gpio)) {
                if (timeout) {
                    ret = wait_event_interruptible_timeout(nfc->read_wq, !nfc->irq_enable,
                                                           msecs_to_jiffies(timeout));

                    if (ret <= 0) {
                        TMS_ERR("Wakeup of read work queue timeout\n");
                        return -ETIMEDOUT;
                    }
                } else {
                    ret = wait_event_interruptible(nfc->read_wq, !nfc->irq_enable);

                    if (ret) {
                        TMS_ERR("Wakeup of read work queue failed\n");
                        return ret;
                    }
                }
            }

            nfc_disable_irq(nfc);

            if (gpio_get_value(nfc->hw_res.irq_gpio)) {
                break;
            }

            if (!gpio_get_value(nfc->hw_res.irq_gpio)) {
                TMS_ERR("Can not detect interrupt\n");
                return -EIO;
            }

            if (nfc->release_read) {
                TMS_ERR("Releasing read\n");
                return -EWOULDBLOCK;
            }

            TMS_INFO("Spurious interrupt detected\n");
        }
    }

    /* Read data */
    ret = i2c_master_recv(nfc->client, buf, count);
    TMS_DEBUG("Normal end-\n");
    return ret;
}

static int nfc_ioctl_set_state(struct nfc_info *nfc, unsigned long arg)
{
    int ret = SUCCESS;
    TMS_DEBUG("arg = %lu\n", arg);

    switch (arg) {
    case NFC_DLD_PWR_VEN_OFF:
    case NFCC_POWER_OFF:
        nfc_power_control(nfc, OFF);
        break;

    case NFC_DLD_PWR_VEN_ON:
    case NFCC_POWER_ON:
        nfc_power_control(nfc, ON);
        break;

    case NFC_DLD_PWR_DL_OFF:
    case NFCC_FW_DWNLD_OFF:
        nfc_fw_download_control(nfc, OFF);
        break;

    case NFC_DLD_PWR_DL_ON:
    case NFCC_FW_DWNLD_ON:
        nfc_fw_download_control(nfc, ON);
        break;

    case NFCC_HARD_RESET:
        nfc_hard_reset(nfc, GPIO_VEN_SET_WAIT_TIME_US);
        break;

    case NFC_DLD_FLUSH:
        /*
         * release blocked user thread waiting for pending read
         */
        if (!mutex_trylock(&nfc->read_mutex)) {
            nfc->release_read = true;
            nfc_disable_irq(nfc);
            wake_up(&nfc->read_wq);
            TMS_INFO("Waiting for release of blocked read\n");
            mutex_lock(&nfc->read_mutex);
            nfc->release_read = false;
        } else {
            TMS_INFO("Read thread already released\n");
        }
        mutex_unlock(&nfc->read_mutex);
        break;

    default:
        TMS_ERR("Unknow control arg %lu\n", arg);
        ret = -ENOIOCTLCMD;
        break;
    }

    return ret;
}

/*********** PART2: Operation Function Area ***********/
static long nfc_device_ioctl(struct file *file, unsigned int cmd,
                             unsigned long arg)
{
    int ret;
    struct nfc_info *nfc;
    TMS_DEBUG("cmd = %x arg = %zx\n", cmd, arg);
    nfc = file->private_data;

    if (!nfc) {
        TMS_ERR("NFC device no longer exists\n");
        return -ENODEV;
    }

    switch (cmd) {
    case NFC_IRQ_STATE:
        ret = irq_trigger_check(nfc);
        break;

    case NFC_SET_STATE:
        ret = nfc_ioctl_set_state(nfc, arg);
        break;

    case NFC_SET_ESE_STATE:
        ret = nfc_ioctl_set_ese_state(nfc, arg);
        break;

    case NFC_GET_ESE_STATE:
        ret = nfc_ioctl_get_ese_state(nfc, arg);
        break;

    default:
        TMS_ERR("Unknow control cmd[%x]\n", cmd);
        ret = -ENOIOCTLCMD;
    };

    return ret;
}

int nfc_device_flush(struct file *file, fl_owner_t id)
{
    struct nfc_info *nfc;
    nfc = file->private_data;

    if (!nfc) {
        TMS_ERR("NFC device no longer exists\n");
        return -ENODEV;
    }

    if (!mutex_trylock(&nfc->read_mutex)) {
        nfc->release_read = true;
        nfc_disable_irq(nfc);
        wake_up(&nfc->read_wq);
        TMS_INFO("Waiting for release of blocked read\n");
        mutex_lock(&nfc->read_mutex);
        nfc->release_read = false;
    } else {
        TMS_INFO("Read thread already released\n");
    }

    mutex_unlock(&nfc->read_mutex);
    return SUCCESS;
}

//static unsigned int nfc_device_poll(struct file *file, poll_table *wait)
//{
//    struct nfc_info *nfc;
//    unsigned int mask = 0;
//    int irqtrge = 0;
//    nfc = file->private_data;
//    /* wait for irq trigger is high */
//    poll_wait(file, &nfc->read_wq, wait);
//    irqtrge = irq_trigger_check(nfc);
//
//    if (irqtrge != 0) {
//        /* signal data avail */
//        mask = POLLIN | POLLRDNORM;
//        nfc_disable_irq(nfc);
//    } else {
//        /* irq trigger is low. Activate ISR */
//        if (!nfc->irq_enable) {
//            TMS_DEBUG("Enable IRQ\n");
//            nfc_enable_irq(nfc);
//        } else {
//            TMS_DEBUG("IRQ already enabled\n");
//        }
//    }
//
//    return mask;
//}

ssize_t nfc_device_read(struct file *file, char __user *buf, size_t count,
                        loff_t *offset)
{
    int ret;
    uint8_t *read_buf;
    bool need2byte = false;
    struct nfc_info *nfc;
    nfc = file->private_data;

    if (!nfc) {
        TMS_ERR("NFC device no longer exists\n");
        return -ENODEV;
    }

    if (count > 0 && count < MAX_BUFFER_SIZE) {
    } else if (count > MAX_BUFFER_SIZE) {
        TMS_WARN("The read bytes[%zu] exceeded the buffer max size, count = %d\n",
                 count, MAX_BUFFER_SIZE);
        count = MAX_BUFFER_SIZE;
    } else {
        TMS_ERR("read error,count = %zu\n", count);
        return -EPERM;
    }
    mutex_lock(&nfc->read_mutex);

    if (last_count == 3 && count == 1) {
        TMS_WARN("Need read 2 bytes\n");
        need2byte = true;
        ++count;
    }
    /* malloc read buffer */
    read_buf = devm_kzalloc(nfc->i2c_dev, count, GFP_DMA | GFP_KERNEL);

    if (!read_buf) {
        TMS_ERR("devm_kzalloc read buffer error\n");
        mutex_unlock(&nfc->read_mutex);
        return -ENOMEM;
    }

    memset(read_buf, 0x00, count);

    if (file->f_flags & O_NONBLOCK) {
        /* Noblock read data mode */
        if (!gpio_get_value(nfc->hw_res.irq_gpio)) {
            TMS_WARN("Read called but no IRQ!\n");
            ret = -EAGAIN;
            goto err_release_read;
        } else {
            /* Noblock read data mode */
            ret = i2c_master_recv(nfc->client, read_buf, count);
        }
    } else {
        /* Block read data mode */
        ret = i2c_master_block_recv(nfc, read_buf, count, 0);
    }

    if (need2byte) {
        --count;
        --ret;
    }

    if (ret != count) {
        TMS_ERR("I2C read failed ret = %d\n", ret);
        goto err_release_read;
    }

    g_pre_write_time = ktime_get_boottime();

    if (copy_to_user(buf, read_buf, ret)) {
        TMS_ERR("Copy to user space failed\n");
        ret = -EFAULT;
        goto err_release_read;
    }

    last_count = count;
    tms_buffer_dump("Rx <-", read_buf, count);
err_release_read:
    mutex_unlock(&nfc->read_mutex);
    devm_kfree(nfc->i2c_dev, read_buf);
    return ret;
}

static ssize_t nfc_device_write(struct file *file, const char __user *buf,
                                size_t count, loff_t *offset)
{
    int ret = -EIO;
    uint8_t *write_buf;
    struct nfc_info *nfc = NULL;
    char wakeup_cmd[1] = {0};
    int wakeup_len = 1;
    int retry_count = 0;
    ktime_t elapse_time = 0;
    ktime_t write_time;
    nfc = file->private_data;

    if (!nfc) {
        TMS_ERR("NFC device no longer exists\n");
        return -ENODEV;
    }

    if (count > 0 && count < MAX_BUFFER_SIZE) {
    } else if (count > MAX_BUFFER_SIZE) {
        TMS_WARN("The write bytes[%zu] exceeded the buffer max size, count = %d\n",
                 count, MAX_BUFFER_SIZE);
        count = MAX_BUFFER_SIZE;
    } else {
        TMS_ERR("write error,count = %zu\n", count);
        return -EPERM;
    }

    /* malloc write buffer */
    write_buf = devm_kzalloc(nfc->i2c_dev, count, GFP_DMA | GFP_KERNEL);

    if (!write_buf) {
        TMS_ERR("Read buffer alloc failed\n");
        return -ENOMEM;
    }

    memset(write_buf, 0x00, count);
    mutex_lock(&nfc->write_mutex);

    if (copy_from_user(write_buf, buf, count)) {
        TMS_ERR("Copy from user space failed\n");
        ret = -EFAULT;
        goto err_release_write;
    }

    if (write_buf[0] != T1_HEAD) {
        write_time = ktime_get_boottime();
        elapse_time = write_time - g_pre_write_time;
        // make sure elapse_time is not overflow
        if (elapse_time < 0) {
            elapse_time = I2C_ELAPSE_TIMEOUT;
        }
        g_pre_write_time = write_time;
        if (elapse_time >= I2C_ELAPSE_TIMEOUT) {
            TMS_INFO("TMS NFC need to send 0x00\n");
            while (++retry_count < MAX_I2C_WAKEUP_TIME) {
                ret = i2c_master_send(nfc->client, wakeup_cmd, wakeup_len);
                usleep_range(I2C_WAKEUP_SLEEP_TIME1, I2C_WAKEUP_SLEEP_TIME2);
                if (ret == wakeup_len) {
                    break;
                }
            }
            if (ret < 0) {
                TMS_ERR("TMS NFC failed to write wakeup_cmd : %d, retry for : %d times\n", ret, retry_count);
            }
        }
    }

    /* Write data */
    ret = i2c_master_send(nfc->client, write_buf, count);

    if (ret != count) {
        TMS_ERR("I2C writer error = %d\n", ret);
        ret = -EIO;
        goto err_release_write;
    }

    tms_buffer_dump("Tx ->", write_buf, count);
err_release_write:
    mutex_unlock(&nfc->write_mutex);
    devm_kfree(nfc->i2c_dev, write_buf);
    return ret;
}

static int nfc_device_open(struct inode *inode, struct file *file)
{
    struct nfc_info *nfc = NULL;
    TMS_WARN("Kernel version : %06x, TMS version : %s\n", LINUX_VERSION_CODE, DRIVER_VERSION);
    TMS_WARN("NFC driver version : %s\n", NFC_VERSION);
    TMS_DEBUG("NFC device number is %d-%d\n", imajor(inode),
              iminor(inode));
    nfc = nfc_get_data(inode);

    if (!nfc) {
        TMS_ERR("NFC device not exist\n");
        return -ENODEV;
    }

    mutex_lock(&nfc->open_dev_mutex);
    file->private_data = nfc;

    if (nfc->open_dev_count == 0) {
        nfc_fw_download_control(nfc, OFF);
        nfc_enable_irq(nfc);
    }

    nfc->open_dev_count++;
    mutex_unlock(&nfc->open_dev_mutex);
    return SUCCESS;
}

static int nfc_device_close(struct inode *inode, struct file *file)
{
    struct nfc_info *nfc = NULL;
    TMS_DEBUG("Close NFC device[%d-%d]\n", imajor(inode),
              iminor(inode));
    nfc = nfc_get_data(inode);

    if (!nfc) {
        TMS_ERR("NFC device not exist\n");
        return -ENODEV;
    }

    mutex_lock(&nfc->open_dev_mutex);

    if (nfc->open_dev_count == 1) {
        nfc_disable_irq(nfc);
        nfc_fw_download_control(nfc, OFF);
        TMS_DEBUG("Close all NFC device\n");
    }

    if (nfc->open_dev_count > 0) {
        nfc->open_dev_count--;
    }

    file->private_data = NULL;
    mutex_unlock(&nfc->open_dev_mutex);
    return SUCCESS;
}

static const struct file_operations nfc_fops = {
    .owner          = THIS_MODULE,
    .llseek         = no_llseek,
    .open           = nfc_device_open,
    .release        = nfc_device_close,
    .read           = nfc_device_read,
    .write          = nfc_device_write,
    .flush          = nfc_device_flush,
//    .poll           = nfc_device_poll,
    .unlocked_ioctl = nfc_device_ioctl,
};

/*********** PART3: NFC Driver Start Area ***********/
int nfc_device_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret;
    struct nfc_info *nfc = NULL;
    TMS_INFO("NFC Probe start+\n");

    /* step1 : alloc nfc_info */
    nfc = nfc_data_alloc(&client->dev, nfc);

    if (nfc == NULL) {
        TMS_ERR("Nfc info alloc memory error\n");
        return -ENOMEM;
    }

    /* step2 : init and binding parameters for easy operate */
    nfc->client             = client;
    nfc->i2c_dev            = &client->dev;
    nfc->irq_enable         = true;
    nfc->irq_wake_up        = false;
    nfc->dev.fops           = &nfc_fops;
    /* step3 : register nfc info*/
    ret = nfc_common_info_init(nfc);

    if (ret) {
        TMS_ERR("Init common nfc device failed\n");
        goto err_free_nfc_malloc;
    }

    /* step4 : I2C function check */
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        TMS_ERR("need I2C_FUNC_I2C\n");
        ret = -ENODEV;
        goto err_free_nfc_info;
    }

    /* step5 : init mutex and queues */
    init_waitqueue_head(&nfc->read_wq);
    mutex_init(&nfc->read_mutex);
    mutex_init(&nfc->write_mutex);
    spin_lock_init(&nfc->irq_enable_slock);
    /* step6 : register nfc device */
    if (!nfc->tms->registe_device) {
        TMS_ERR("nfc->tms->registe_device is NULL\n");
        ret = -ERROR;
        goto err_destroy_mutex;
    }
    ret = nfc->tms->registe_device(&nfc->dev, nfc);
    if (ret) {
        TMS_ERR("NFC device register failed\n");
        goto err_destroy_mutex;
    }
#if !defined(CONFIG_TMS_GUIDE_DEVICE) && !defined(CONFIG_TMS_GUIDE_DEVICE_MODULE)
    /* step7 : Create system property node */
    ret = nfc_create_sysfs_interfaces(&client->dev);

    if (ret < 0) {
        TMS_ERR("Create sysfs interface failed\n");
        goto err_unregiste_device;
    }

    ret = sysfs_create_link(NULL, &client->dev.kobj, "nfc");

    if (ret < 0) {
        TMS_ERR("create sysfs link failed\n");
        goto err_remove_sysfs_interface;
    }
#endif

    /* step8 : register nfc irq */
    ret = nfc_irq_register(nfc);

    if (ret) {
        TMS_ERR("register irq failed\n");
        goto err_remove_sysfs_link;
    }

    nfc_disable_irq(nfc);
    nfc_hard_reset(nfc, GPIO_VEN_SET_WAIT_TIME_US);
    device_init_wakeup(nfc->i2c_dev, true);
    i2c_set_clientdata(client, nfc);
    TMS_INFO("Probe successfully\n");
    return SUCCESS;

err_remove_sysfs_link:
#if !defined(CONFIG_TMS_GUIDE_DEVICE) && !defined(CONFIG_TMS_GUIDE_DEVICE_MODULE)
    sysfs_remove_link(NULL, "nfc");
err_remove_sysfs_interface:
    nfc_remove_sysfs_interfaces(&client->dev);
err_unregiste_device:
#endif
    if (nfc->tms->unregiste_device) {
        nfc->tms->unregiste_device(&nfc->dev);
    }
err_destroy_mutex:
    mutex_destroy(&nfc->read_mutex);
    mutex_destroy(&nfc->write_mutex);
err_free_nfc_info:
    nfc_gpio_release(nfc);
err_free_nfc_malloc:
    nfc_data_free(&client->dev, nfc);
    nfc = NULL;
    TMS_ERR("Probe failed, ret = %d\n", ret);
    return ret;
}

int nfc_device_remove(struct i2c_client *client)
{
    struct nfc_info *nfc;
    TMS_INFO("Remove NFC device\n");
    nfc = i2c_get_clientdata(client);

    if (!nfc) {
        TMS_ERR("NFC device no longer exists\n");
        return -ENODEV;
    }

    if (nfc->open_dev_count > 0) {
        TMS_ERR("NFC device is being occupied\n");
        return -EBUSY;
    }

    nfc->tms->ven_enable = false;
#if !defined(CONFIG_TMS_GUIDE_DEVICE) && !defined(CONFIG_TMS_GUIDE_DEVICE_MODULE)
    nfc_remove_sysfs_interfaces(&client->dev);
#endif
    device_init_wakeup(nfc->i2c_dev, false);
    free_irq(nfc->client->irq, nfc);
    mutex_destroy(&nfc->read_mutex);
    nfc_gpio_release(nfc);
    if (nfc->tms->unregiste_device) {
        nfc->tms->unregiste_device(&nfc->dev);
    }
    nfc_data_free(&client->dev, nfc);
    nfc = NULL;
    i2c_set_clientdata(client, NULL);
    return SUCCESS;
}

void nfc_device_shutdown(struct i2c_client *client)
{
    struct nfc_info *nfc;
    TMS_INFO("NFC device shutdown\n");

    nfc = i2c_get_clientdata(client);

    if (!nfc) {
        TMS_ERR("NFC device no longer exists\n");
        return;
    }
    nfc_power_control(nfc, OFF);
    msleep(20); // hard reset guard time
}

int nfc_device_suspend(struct device *device)
{
    struct i2c_client *client;
    struct nfc_info *nfc;
    TMS_INFO("NFC device suspend start+\n");
    client = to_i2c_client(device);
    nfc    = i2c_get_clientdata(client);

    if (!nfc) {
        TMS_ERR("NFC device no longer exists\n");
        return -ENODEV;
    }

    if (device_may_wakeup(nfc->i2c_dev) && nfc->irq_enable) {
        if (!enable_irq_wake(nfc->client->irq)) {
            nfc->irq_wake_up = true;
        }
    }

    TMS_DEBUG("irq_wake_up = %d\n", nfc->irq_wake_up);
    TMS_INFO("Normal end-\n");
    return SUCCESS;
}

int nfc_device_resume(struct device *device)
{
    struct i2c_client *client;
    struct nfc_info *nfc;
    TMS_INFO("Start+\n");
    client = to_i2c_client(device);
    nfc    = i2c_get_clientdata(client);

    if (!nfc) {
        TMS_ERR("NFC device no longer exists\n");
        return -ENODEV;
    }

    if (device_may_wakeup(nfc->i2c_dev) && nfc->irq_wake_up) {
        if (!disable_irq_wake(nfc->client->irq)) {
            nfc->irq_wake_up = false;
        }
    }

    TMS_DEBUG("irq_wake_up = %d\n", nfc->irq_wake_up);
    TMS_INFO("Normal end-\n");
    return SUCCESS;
}

#if !defined(CONFIG_TMS_GUIDE_DEVICE) && !defined(CONFIG_TMS_GUIDE_DEVICE_MODULE)
static const struct dev_pm_ops nfc_pm_ops = {
    .suspend = nfc_device_suspend,
    .resume  = nfc_device_resume,
};

static const struct i2c_device_id nfc_device_id[] = {
    {NFC_DEVICE, 0 },
    { }
};

static struct of_device_id nfc_match_table[] = {
    { .compatible = NFC_DEVICE, },
    { }
};

static struct i2c_driver nfc_i2c_driver = {
    .probe    = nfc_device_probe,
    .remove   = nfc_device_remove,
    .id_table = nfc_device_id,
    .shutdown = nfc_device_shutdown,
    .driver   = {
        .owner          = THIS_MODULE,
        .name           = NFC_DEVICE,
        .of_match_table = nfc_match_table,
        .pm             = &nfc_pm_ops,
        .probe_type     = PROBE_PREFER_ASYNCHRONOUS,
    },
};

MODULE_DEVICE_TABLE(of, nfc_match_table);

int nfc_driver_init(void)
{
    int ret;
    TMS_INFO("Loading nfc driver\n");
    ret = i2c_add_driver(&nfc_i2c_driver);

    if (ret) {
        TMS_ERR("Unable to add i2c driver, ret = %d\n", ret);
    }

    return ret;
}

void nfc_driver_exit(void)
{
    TMS_INFO("Unloading nfc driver\n");
    i2c_del_driver(&nfc_i2c_driver);
}
#endif

MODULE_DESCRIPTION("TMS NFC Driver");
MODULE_LICENSE("GPL");
