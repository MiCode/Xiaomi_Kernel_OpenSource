/**
 * The device control driver for Fortsense's fingerprint sensor.
 *
 * Copyright (C) 2018 Fortsense Corporation. <http://www.fortsense.com>
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 * Public License for more details.
**/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/input.h>
#include <linux/uaccess.h>
#include <linux/spi/spi.h>

#include "sf_ctl.h"

#if XIAOMI_DRM_INTERFACE_WA
#include <drm/drm_bridge.h>
#include <drm/drm_notifier.h>

#define FP_UNLOCK_REJECTION_TIMEOUT   1500

#endif

#if SF_BEANPOD_COMPATIBLE_V1
#include "nt_smc_call.h"
#endif

#if SF_INT_TRIG_HIGH
#include <linux/irq.h>
#endif

#ifdef CONFIG_RSEE
#include <linux/tee_drv.h>
#endif

//---------------------------------------------------------------------------------
#define SF_DRV_VERSION "v2.4.2-2020-11-09"

#define MODULE_NAME "fortsense-sf_ctl"
#define xprintk(level, fmt, args...) printk(level MODULE_NAME": (%d) "fmt, __LINE__, ##args)

#if SF_TRUSTKERNEL_COMPAT_SPI_MT65XX
#define SPI_MODULE_CLOCK      (120 * 1000 * 1000)
#elif defined(CONFIG_MTK_SPI)
#define SPI_MODULE_CLOCK      (100 * 1000 * 1000)
#endif

#define SF_DEFAULT_SPI_SPEED  (1 * 1000 * 1000)
//---------------------------------------------------------------------------------

//---------------------------------------------------------------------------------
static long sf_ctl_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static int sf_ctl_init_irq(void);
static int sf_ctl_init_input(void);
#ifdef CONFIG_COMPAT
static long sf_ctl_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
#endif
#if SF_REE_PLATFORM
static ssize_t sf_ctl_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t sf_ctl_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
#endif
#if (SF_PROBE_ID_EN && !SF_RONGCARD_COMPATIBLE)
static int sf_read_sensor_id(void);
#endif

extern int sf_platform_init(struct sf_ctl_device *ctl_dev);
extern void sf_platform_exit(struct sf_ctl_device *ctl_dev);

extern int fb_blank(struct fb_info *info, int blank);

//---------------------------------------------------------------------------------

#if QUALCOMM_REE_DEASSERT
static int qualcomm_deassert = 0;
#endif

#ifdef CONFIG_RSEE
int rsee_client_get_fpid(int *vendor_id);
#endif
static struct file_operations sf_ctl_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = sf_ctl_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl   = sf_ctl_compat_ioctl,
#endif
#if SF_REE_PLATFORM
    .read           = sf_ctl_read,
    .write          = sf_ctl_write,
#endif
};

static struct sf_ctl_device sf_ctl_dev = {
    .miscdev = {
        .minor  = MISC_DYNAMIC_MINOR,
        .name   = "fortsense_fp",
        .fops   = &sf_ctl_fops,
    },
    .rst_num = 0,
    .irq_pin = 0,
    .irq_num = 0,
    .spi_buf_size = 25 * 1024,
};

#if SF_REG_DEVICE_BY_DRIVER
static struct platform_device *sf_device = NULL;
#endif

#if (defined(CONFIG_MTK_SPI) || SF_TRUSTKERNEL_COMPAT_SPI_MT65XX)
#define SF_DEFAULT_SPI_HALF_CYCLE_TIME  ((SPI_MODULE_CLOCK / SF_DEFAULT_SPI_SPEED) / 2)
#define SF_DEFAULT_SPI_HIGH_TIME   (((SPI_MODULE_CLOCK / SF_DEFAULT_SPI_SPEED) % 2 == 0) ? \
                                   SF_DEFAULT_SPI_HALF_CYCLE_TIME : (SF_DEFAULT_SPI_HALF_CYCLE_TIME + 1))
#define SF_DEFAULT_SPI_LOW_TIME    SF_DEFAULT_SPI_HALF_CYCLE_TIME

static struct mt_chip_conf smt_conf = {
    .setuptime = 15,
    .holdtime = 15,
    .high_time = SF_DEFAULT_SPI_HIGH_TIME, // 10--6m 15--4m 20--3m 30--2m [ 60--1m 120--0.5m  300--0.2m]
    .low_time  = SF_DEFAULT_SPI_LOW_TIME,
    .cs_idletime = 20,
    .ulthgh_thrsh = 0,
    .cpol = SPI_CPOL_0,
    .cpha = SPI_CPHA_0,
    .rx_mlsb = SPI_MSB,
    .tx_mlsb = SPI_MSB,
    .tx_endian = 0,
    .rx_endian = 0,
    .com_mod = FIFO_TRANSFER,
    .pause = 0,
    .finish_intr = 1,
    .deassert = 0,
    .ulthigh = 0,
    .tckdly = 0,
};
#endif

static int sf_remove(sf_device_t *pdev);
static int sf_probe(sf_device_t *pdev);

#if SF_SPI_RW_EN
static struct of_device_id  sf_of_match[] = {
    { .compatible = COMPATIBLE_SW_FP, },
    {},
};

static struct spi_board_info spi_board_devs[] __initdata = {
    [0] = {
        .modalias = "fortsense-fp",
        .bus_num = 0,
        .chip_select = 0,
        .mode = SPI_MODE_0,
    },
};

static int sf_ctl_spi_speed(unsigned int speed)
{
#ifdef CONFIG_MTK_SPI
    unsigned int time;
    time = SPI_MODULE_CLOCK / speed;
    sf_ctl_dev.mt_conf.high_time = time / 2;
    sf_ctl_dev.mt_conf.low_time  = time / 2;

    if ((time % 2) != 0) {
        sf_ctl_dev.mt_conf.high_time += 1;
    }

#elif (SF_REE_PLATFORM && QUALCOMM_REE_DEASSERT)
    double delay_ns = 0;

    if (speed <= 1000 * 1000) {
        speed = 0.96 * 1000 * 1000; //0.96M
        qualcomm_deassert = 0;
    }
    else if (speed <= 4800 * 1000) {
        delay_ns = (1.0 / (((double)speed) / (1.0 * 1000 * 1000)) - 1.0 / 4.8) * 8 * 1000;
        speed = 4.8 * 1000 * 1000; //4.8M
        qualcomm_deassert = 3;
    }
    else {
        delay_ns = (1.0 / (((double)speed) / (1.0 * 1000 * 1000)) - 1.0 / 9.6) * 8 * 1000;
        speed = 9.6 * 1000 * 1000; //9.6M
        qualcomm_deassert = 10;
    }

    xprintk(KERN_INFO, "need delay_ns = xxx, qualcomm_deassert = %d(maybe custom).\n", qualcomm_deassert);
#endif
    sf_ctl_dev.pdev->max_speed_hz = speed;
    spi_setup(sf_ctl_dev.pdev);
    return 0;
}
#endif

static sf_driver_t sf_driver = {
    .driver = {
        .name = "fortsense-fp",
#if SF_SPI_RW_EN
        .bus = &spi_bus_type,
#endif
        .owner = THIS_MODULE,
#ifdef CONFIG_OF
#if SF_SPI_RW_EN
        .of_match_table = sf_of_match,
#endif
#endif
    },
    .probe  = sf_probe,
    .remove = sf_remove,
};

static sf_version_info_t sf_hw_ver;


//---------------------------------------------------------------------------------
#if SF_INT_TRIG_HIGH
static int sf_ctl_set_irq_type(unsigned long type)
{
    int err = 0;

    if (sf_ctl_dev.irq_num > 0) {
        err = irq_set_irq_type(sf_ctl_dev.irq_num, type | IRQF_NO_SUSPEND | IRQF_ONESHOT);
    }

    return err;
}
#endif

static void sf_ctl_device_event(struct work_struct *ws)
{
    char *uevent_env[2] = { SF_INT_EVENT_NAME, NULL };
    kobject_uevent_env(&sf_ctl_dev.miscdev.this_device->kobj,
                       KOBJ_CHANGE, uevent_env);
}

static irqreturn_t sf_ctl_device_irq(int irq, void *dev_id)
{
    disable_irq_nosync(irq);
    xprintk(SF_LOG_LEVEL, "%s(irq = %d, ..) toggled.\n", __FUNCTION__, irq);
    schedule_work(&sf_ctl_dev.work_queue);
#ifdef CONFIG_PM_WAKELOCKS
    __pm_wakeup_event(&sf_ctl_dev.wakelock, msecs_to_jiffies(5000));
#else
    wake_lock_timeout(&sf_ctl_dev.wakelock, msecs_to_jiffies(5000));
#endif
#if SF_INT_TRIG_HIGH
    sf_ctl_set_irq_type(IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND | IRQF_ONESHOT);
#endif
    enable_irq(irq);
    return IRQ_HANDLED;
}

static int sf_ctl_report_key_event(struct input_dev *input, sf_key_event_t *kevent)
{
    int err = 0;
    unsigned int key_code = KEY_UNKNOWN;
    xprintk(SF_LOG_LEVEL, "%s(..) enter.\n", __FUNCTION__);

    switch (kevent->key) {
        case SF_KEY_HOME:
            key_code = KEY_HOME;
            break;

        case SF_KEY_MENU:
            key_code = KEY_MENU;
            break;

        case SF_KEY_BACK:
            key_code = KEY_BACK;
            break;

        case SF_KEY_F11:
            key_code = XIAOMI_KEYCODE_SINGLE_CLK;
            break;

        case SF_KEY_ENTER:
            key_code = KEY_ENTER;
            break;

        case SF_KEY_UP:
            key_code = KEY_UP;
            break;

        case SF_KEY_LEFT:
            key_code = KEY_LEFT;
            break;

        case SF_KEY_RIGHT:
            key_code = KEY_RIGHT;
            break;

        case SF_KEY_DOWN:
            key_code = KEY_DOWN;
            break;

        case SF_KEY_WAKEUP:
            key_code = KEY_WAKEUP;
#if XIAOMI_DRM_INTERFACE_WA

            if (kevent->value && sf_ctl_dev.is_fb_black) {
                schedule_work(&sf_ctl_dev.work_drm);
            }

#endif
            break;

        case SF_KEY_F12:
            key_code = XIAOMI_KEYCODE_DOUBLE_CLK;
            break;

        default:
            break;
    }

    input_report_key(input, key_code, kevent->value);
    input_sync(input);
    return err;
}

static const char *sf_ctl_get_version(void)
{
    static char version[SF_DRV_VERSION_LEN] = {'\0', };
    strncpy(version, SF_DRV_VERSION, SF_DRV_VERSION_LEN);
    version[SF_DRV_VERSION_LEN - 1] = '\0';
    return (const char *)version;
}

////////////////////////////////////////////////////////////////////////////////
// struct file_operations fields.

static long sf_ctl_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int err = 0;
    sf_key_event_t kevent;

    switch (cmd) {
        case SF_IOC_INIT_DRIVER: {
#if MULTI_HAL_COMPATIBLE
            sf_ctl_dev.gpio_init(&sf_ctl_dev);
#endif
            break;
        }

        case SF_IOC_DEINIT_DRIVER: {
#if MULTI_HAL_COMPATIBLE
            sf_ctl_dev.free_gpio(&sf_ctl_dev);
#endif
            break;
        }

        case SPI_IOC_RST:
        case SF_IOC_RESET_DEVICE: {
            sf_ctl_dev.reset(false);
            msleep(1);
            sf_ctl_dev.reset(true);
            msleep(10);
            break;
        }

        case SF_IOC_ENABLE_IRQ: {
            // TODO:
            break;
        }

        case SF_IOC_DISABLE_IRQ: {
            // TODO:
            break;
        }

        case SF_IOC_REQUEST_IRQ: {
#if MULTI_HAL_COMPATIBLE
            sf_ctl_init_irq();
#endif
            break;
        }

        case SF_IOC_ENABLE_SPI_CLK: {
            sf_ctl_dev.spi_clk_on(true);
            break;
        }

        case SF_IOC_DISABLE_SPI_CLK: {
            sf_ctl_dev.spi_clk_on(false);
            break;
        }

        case SF_IOC_ENABLE_POWER: {
            sf_ctl_dev.power_on(true);
            break;
        }

        case SF_IOC_DISABLE_POWER: {
            //sf_ctl_dev.power_on(false);
            break;
        }

        case SF_IOC_REPORT_KEY_EVENT: {
            if (copy_from_user(&kevent, (sf_key_event_t *)arg, sizeof(sf_key_event_t))) {
                xprintk(KERN_ERR, "copy_from_user(..) failed.\n");
                err = (-EFAULT);
                break;
            }

            err = sf_ctl_report_key_event(sf_ctl_dev.input, &kevent);
            break;
        }

        case SF_IOC_SYNC_CONFIG: {
            // TODO:
            break;
        }

        case SPI_IOC_WR_MAX_SPEED_HZ:
        case SF_IOC_SPI_SPEED: {
#if SF_SPI_RW_EN
            sf_ctl_spi_speed(arg);
#endif
            break;
        }

        case SPI_IOC_RD_MAX_SPEED_HZ: {
            // TODO:
            break;
        }

        case FORTSENSE_IOC_ATTRIBUTE:
        case SF_IOC_ATTRIBUTE: {
            err = __put_user(sf_ctl_dev.attribute, (__u32 __user *)arg);
            break;
        }

        case SF_IOC_GET_VERSION: {
            if (copy_to_user((void *)arg, sf_ctl_get_version(), SF_DRV_VERSION_LEN)) {
                xprintk(KERN_ERR, "copy_to_user(..) failed.\n");
                err = (-EFAULT);
                break;
            }

            break;
        }

        case SF_IOC_SET_LIB_VERSION: {
            if (copy_from_user((void *)&sf_hw_ver, (void *)arg, sizeof(sf_version_info_t))) {
                xprintk(KERN_ERR, "sf_hw_info_t copy_from_user(..) failed.\n");
                err = (-EFAULT);
                break;
            }

            break;
        }

        case SF_IOC_GET_LIB_VERSION: {
            if (copy_to_user((void *)arg, (void *)&sf_hw_ver, sizeof(sf_version_info_t))) {
                xprintk(KERN_ERR, "sf_hw_info_t copy_to_user(..) failed.\n");
                err = (-EFAULT);
                break;
            }

            break;
        }

        case SF_IOC_SET_SPI_BUF_SIZE: {
            sf_ctl_dev.spi_buf_size = arg;
            break;
        }

        case SF_IOC_SET_RESET_OUTPUT: {
            if (arg) {
                sf_ctl_dev.reset(true);
            }
            else {
                sf_ctl_dev.reset(false);
            }

            break;
        }

        default:
            err = (-EINVAL);
            break;
    }

    return err;
}

static ssize_t fortsense_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int len = 0;
    len += sprintf(buf, "%s\n", sf_hw_ver.driver);
    return len;
}
static DEVICE_ATTR(version, S_IRUGO | S_IWUSR, fortsense_version_show, NULL);

static ssize_t fortsense_chip_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int len = 0;
    len += sprintf((char *)buf, "chip   : %s %s\nid     : 0x0 lib:%s\nvendor : fw:%s\nmore   : fingerprint\n",
                   sf_hw_ver.fortsense_id, sf_hw_ver.ca_version,
                   sf_hw_ver.algorithm,
                   sf_hw_ver.firmware);
    return len;
}
static DEVICE_ATTR(chip_info, S_IRUGO | S_IWUSR, fortsense_chip_info_show, NULL);

static ssize_t sf_show_version(struct device *dev, struct device_attribute *attr, char *buf)
{
    int ret = 0;
    ret += sprintf(buf + ret, "solution:%s\n", sf_hw_ver.tee_solution);
    ret += sprintf(buf + ret, "ca      :%s\n", sf_hw_ver.ca_version);
    ret += sprintf(buf + ret, "ta      :%s\n", sf_hw_ver.ta_version);
    ret += sprintf(buf + ret, "alg     :%s\n", sf_hw_ver.algorithm);
    ret += sprintf(buf + ret, "nav     :%s\n", sf_hw_ver.algo_nav);
    ret += sprintf(buf + ret, "driver  :%s\n", sf_hw_ver.driver);
    ret += sprintf(buf + ret, "firmware:%s\n", sf_hw_ver.firmware);
    ret += sprintf(buf + ret, "sensor  :%s\n", sf_hw_ver.fortsense_id);
    ret += sprintf(buf + ret, "vendor  :%s\n", sf_hw_ver.vendor_id);
    return ret;
}

static DEVICE_ATTR(tee_version, S_IWUSR | S_IRUGO, sf_show_version, NULL);

static ssize_t
sf_store_set_fun(struct device *d, struct device_attribute *attr,
                 const char *buf, size_t count)
{
    int blank;
    int ret;
    ret = sscanf(buf, "%d", &blank);
    xprintk(KERN_INFO, "sf_store_set_fun (..) blank = %d.\n", blank);
    fb_blank(NULL, blank);
    xprintk(KERN_INFO, "sf_store_set_fun (..) end.\n");
    return count;
}
static DEVICE_ATTR(set_fun, S_IWUSR | S_IRUGO, NULL, sf_store_set_fun);

static struct attribute *sf_sysfs_entries[] = {
    &dev_attr_set_fun.attr,
    &dev_attr_tee_version.attr,
    &dev_attr_chip_info.attr,
    &dev_attr_version.attr,
    NULL
};

static struct attribute_group sf_attribute_group = {
    .attrs = sf_sysfs_entries,
};

#ifdef CONFIG_COMPAT
static long sf_ctl_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    return sf_ctl_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static int sf_suspend(void)
{
    char *screen[2] = { "SCREEN_STATUS=OFF", NULL };
    kobject_uevent_env(&sf_ctl_dev.miscdev.this_device->kobj, KOBJ_CHANGE, screen);
#if SF_INT_TRIG_HIGH
    sf_ctl_set_irq_type(IRQF_TRIGGER_HIGH);
#endif
    return 0;
}

static int sf_resume(void)
{
    char *screen[2] = { "SCREEN_STATUS=ON", NULL };
    kobject_uevent_env(&sf_ctl_dev.miscdev.this_device->kobj, KOBJ_CHANGE, screen);
#if SF_INT_TRIG_HIGH
    sf_ctl_set_irq_type(IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND | IRQF_ONESHOT);
#endif
    return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sf_early_suspend(struct early_suspend *handler)
{
    sf_suspend();
}

static void sf_late_resume(struct early_suspend *handler)
{
    sf_resume();
}

#elif defined(CONFIG_ADF_SPRD)
/*
 * touchscreen's suspend and resume state should rely on screen state,
 * as fb_notifier and early_suspend are all disabled on our platform,
 * we can only use adf_event now
 */
static int sf_adf_event_handler( \
                                 struct notifier_block *nb, unsigned long action, void *data)
{
    struct adf_notifier_event *event = data;
    int adf_event_data;

    if (action != ADF_EVENT_BLANK) {
        return NOTIFY_DONE;
    }

    adf_event_data = *(int *)event->data;
    xprintk(KERN_INFO, "receive adf event with adf_event_data=%d \n", adf_event_data);

    switch (adf_event_data) {
        case DRM_MODE_DPMS_ON:
            sf_resume();
            break;

        case DRM_MODE_DPMS_OFF:
            sf_suspend();
            break;

        default:
            xprintk(KERN_ERR, "receive adf event with error data, adf_event_data=%d \n",
                    adf_event_data);
            break;
    }

    return NOTIFY_OK;
}

#else

static int sf_fb_notifier_callback(struct notifier_block *self,
                                   unsigned long event, void *data)
{
    struct sf_ctl_device *ctl_dev = container_of(self, struct sf_ctl_device, notifier);
    struct fb_event *evdata = data;
    unsigned int blank;
    int retval = 0;

    if ((ctl_dev == NULL) || (evdata == NULL) || (evdata->data == NULL)) {
        return 0;
    }

    blank = *(int *)evdata->data;
#if XIAOMI_DRM_INTERFACE_WA

    if (event == DRM_EVENT_BLANK) {
        switch (blank) {
            case DRM_BLANK_UNBLANK:
                ctl_dev->is_fb_black = false;
                break;

            case DRM_BLANK_POWERDOWN:
                ctl_dev->is_fb_black = true;
                break;

            default:
                break;
        }
    }

#endif

    if (event == FB_EVENT_BLANK /* FB_EARLY_EVENT_BLANK */) {
        switch (blank) {
            case FB_BLANK_UNBLANK:
                sf_resume();
                break;

            case FB_BLANK_POWERDOWN:
                sf_suspend();
                break;

            default:
                break;
        }
    }

    return retval;
}

#if XIAOMI_DRM_INTERFACE_WA
static void sf_drm_notification_work(struct work_struct *work)
{
    xprintk(KERN_ERR, "receive unblank event\n", __FUNCTION__);
    dsi_bridge_interface_enable(FP_UNLOCK_REJECTION_TIMEOUT);
}
#endif

#endif //SF_CFG_HAS_EARLYSUSPEND
////////////////////////////////////////////////////////////////////////////////
static int sf_remove(sf_device_t *spi)
{
    int err = 0;

    if (sf_ctl_dev.pdev != NULL && sf_ctl_dev.gpio_init != NULL) {
#ifdef CONFIG_HAS_EARLYSUSPEND
        unregister_early_suspend(&sf_ctl_dev.early_suspend);
#elif defined(CONFIG_ADF_SPRD)
        // TODO: is it name adf_unregister_client? Unverified it.
        adf_unregister_client(&sf_ctl_dev.adf_event_block);
#else
#if XIAOMI_DRM_INTERFACE_WA
        drm_unregister_client(&sf_ctl_dev.notifier);
#else
        fb_unregister_client(&sf_ctl_dev.notifier);
#endif
#endif

        if (sf_ctl_dev.input) {
            input_unregister_device(sf_ctl_dev.input);
        }

        if (sf_ctl_dev.irq_num >= 0) {
            free_irq(sf_ctl_dev.irq_num, (void *)&sf_ctl_dev);
            sf_ctl_dev.irq_num = 0;
        }

        misc_deregister(&sf_ctl_dev.miscdev);
#ifdef CONFIG_PM_WAKELOCKS
        wakeup_source_trash(&sf_ctl_dev.wakelock);
#else
        wake_lock_destroy(&sf_ctl_dev.wakelock);
#endif
        sf_ctl_dev.free_gpio(&sf_ctl_dev);
        sf_platform_exit(&sf_ctl_dev);
        sf_ctl_dev.pdev = NULL;
    }

    return err;
}

static int sf_probe(sf_device_t *dev)
{
    int err = 0;
    xprintk(KERN_INFO, "fortsense %s enter\n", __FUNCTION__);
    sf_ctl_dev.pdev = dev;
    /* setup spi config */
#ifdef CONFIG_MTK_SPI
    memcpy(&sf_ctl_dev.mt_conf, &smt_conf, sizeof(struct mt_chip_conf));
    sf_ctl_dev.pdev->controller_data = (void *)&sf_ctl_dev.mt_conf;
    xprintk(KERN_INFO, "fortsense %s old MTK SPI.\n", __FUNCTION__);
#endif
#if SF_SPI_RW_EN
    sf_ctl_dev.pdev->mode            = SPI_MODE_0;
    sf_ctl_dev.pdev->bits_per_word   = 8;
    sf_ctl_dev.pdev->max_speed_hz    = SF_DEFAULT_SPI_SPEED;
    spi_setup(sf_ctl_dev.pdev);
#endif
    /* Initialize the platform config. */
    err = sf_platform_init(&sf_ctl_dev);

    if (err) {
        sf_ctl_dev.pdev = NULL;
        xprintk(KERN_ERR, "sf_platform_init failed with %d.\n", err);
        return err;
    }

#ifdef CONFIG_PM_WAKELOCKS
    wakeup_source_init(&sf_ctl_dev.wakelock , "sf_wakelock");
#else
    wake_lock_init(&sf_ctl_dev.wakelock, WAKE_LOCK_SUSPEND, "sf_wakelock");
#endif
    /* Initialize the GPIO pins. */
#if MULTI_HAL_COMPATIBLE
    xprintk(KERN_INFO, " do not initialize the GPIO pins. \n");
#else
    err = sf_ctl_dev.gpio_init(&sf_ctl_dev);

    if (err) {
        xprintk(KERN_ERR, "gpio_init failed with %d.\n", err);
        sf_ctl_dev.free_gpio(&sf_ctl_dev);
        sf_platform_exit(&sf_ctl_dev);
        sf_ctl_dev.pdev = NULL;
        return err;
    }

    sf_ctl_dev.reset(false);
    msleep(1);
    sf_ctl_dev.reset(true);
    msleep(10);
#endif
#if SF_PROBE_ID_EN
#if SF_BEANPOD_COMPATIBLE_V2
    err = get_fp_spi_enable();

    if (err != 1) {
        xprintk(KERN_ERR, "get_fp_spi_enable ret=%d\n", err);
        sf_ctl_dev.free_gpio(&sf_ctl_dev);
        sf_platform_exit(&sf_ctl_dev);
        sf_ctl_dev.pdev = NULL;
        return -1;
    }

#endif
#if SF_RONGCARD_COMPATIBLE
#ifdef CONFIG_RSEE
    uint64_t vendor_id = 0x00;
    sf_ctl_dev.spi_clk_on(true);
    err = rsee_client_get_fpid(&vendor_id);
    sf_ctl_dev.spi_clk_on(false);
    xprintk(KERN_INFO, "rsee_client_get_fpid vendor id is 0x%x\n", vendor_id);

    if (err || !((vendor_id >> 8) == 0x82)) {
        xprintk(KERN_ERR, "rsee_client_get_fpid failed !\n");
        err = -1;
    }

#else
    err = -1;
    xprintk(KERN_INFO, "CONFIG_RSEE not define, skip rsee_client_get_fpid!\n");
#endif
#else
    sf_ctl_dev.spi_clk_on(true);
    err = sf_read_sensor_id();
    sf_ctl_dev.spi_clk_on(false);
#endif

    if (err < 0) {
        xprintk(KERN_ERR, "fortsense probe read chip id is failed\n");
        sf_ctl_dev.free_gpio(&sf_ctl_dev);
        sf_platform_exit(&sf_ctl_dev);
        sf_ctl_dev.pdev = NULL;
        return -1;
    }

#if SF_BEANPOD_COMPATIBLE_V2
#ifndef MAX_TA_NAME_LEN
    set_fp_vendor(FP_VENDOR_FORTSENSE);
#else
    set_fp_ta_name("fp_server_fortsense", MAX_TA_NAME_LEN);
#endif //MAX_TA_NAME_LEN
#endif //SF_BEANPOD_COMPATIBLE_V2
#endif //SF_PROBE_ID_EN
    /* reset spi dma mode in old MTK. */
#if (SF_REE_PLATFORM && defined(CONFIG_MTK_SPI))
    {
        sf_ctl_dev.mt_conf.com_mod = DMA_TRANSFER;
        spi_setup(sf_ctl_dev.pdev);
    }
#endif
    /* Initialize the input subsystem. */
    err = sf_ctl_init_input();

    if (err) {
        sf_ctl_dev.free_gpio(&sf_ctl_dev);
        sf_platform_exit(&sf_ctl_dev);
        sf_ctl_dev.pdev = NULL;
        xprintk(KERN_ERR, "sf_ctl_init_input failed with %d.\n", err);
        return err;
    }

    /* Register as a miscellaneous device. */
    err = misc_register(&sf_ctl_dev.miscdev);

    if (err) {
        sf_ctl_dev.free_gpio(&sf_ctl_dev);
        sf_platform_exit(&sf_ctl_dev);
        xprintk(KERN_ERR, "misc_register(..) = %d.\n", err);
        input_unregister_device(sf_ctl_dev.input);
        sf_ctl_dev.pdev = NULL;
        return err;
    }

    err = sysfs_create_group(&sf_ctl_dev.miscdev.this_device->kobj, &sf_attribute_group);
    /* Initialize the interrupt callback. */
    INIT_WORK(&sf_ctl_dev.work_queue, sf_ctl_device_event);
#if MULTI_HAL_COMPATIBLE
    xprintk(KERN_INFO, " do not initialize the fingerprint interrupt. \n");
#else
    err = sf_ctl_init_irq();

    if (err) {
        xprintk(KERN_ERR, "sf_ctl_init_irq failed with %d.\n", err);
        input_unregister_device(sf_ctl_dev.input);
        misc_deregister(&sf_ctl_dev.miscdev);
        sf_ctl_dev.free_gpio(&sf_ctl_dev);
        sf_platform_exit(&sf_ctl_dev);
        sf_ctl_dev.pdev = NULL;
        return err;
    }

#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
    sf_ctl_dev.early_suspend.level = (EARLY_SUSPEND_LEVEL_DISABLE_FB - 1);
    sf_ctl_dev.early_suspend.suspend = sf_early_suspend;
    sf_ctl_dev.early_suspend.resume = sf_late_resume;
    register_early_suspend(&sf_ctl_dev.early_suspend);
#elif defined(CONFIG_ADF_SPRD)
    sf_ctl_dev.adf_event_block.notifier_call = sf_adf_event_handler;
    err = adf_register_client(&sf_ctl_dev.adf_event_block);

    if (err < 0) {
        xprintk(KERN_ERR, "register adf notifier fail, cannot sleep when screen off");
    }
    else {
        xprintk(KERN_ERR, "register adf notifier succeed");
    }

#else
    sf_ctl_dev.notifier.notifier_call = sf_fb_notifier_callback;
#if XIAOMI_DRM_INTERFACE_WA
    sf_ctl_dev.is_fb_black = false;
    INIT_WORK(&sf_ctl_dev.work_drm, sf_drm_notification_work);
    drm_register_client(&sf_ctl_dev.notifier);
#else
    fb_register_client(&sf_ctl_dev.notifier);
#endif
#endif
    /* beanpod ISEE2.7 */
#if SF_BEANPOD_COMPATIBLE_V2_7
    {
        /* fortsense define, flow by trustonic */
        struct TEEC_UUID vendor_uuid = {0x0401c03f, 0xc30c, 0x4dd0, \
            {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x04} \
        };
        memcpy(&uuid_fp, &vendor_uuid, sizeof(struct TEEC_UUID));
    }
    xprintk(KERN_ERR, "%s set beanpod isee2.7.0 uuid ok \n", __FUNCTION__);
#endif
    xprintk(KERN_ERR, "%s leave\n", __FUNCTION__);
    return err;
}

#if SF_SPI_TRANSFER
static int tee_spi_transfer(void *sf_conf, int cfg_len, const char *txbuf, char *rxbuf, int len)
{
    struct spi_transfer t;
    struct spi_message m;
    memset(&t, 0, sizeof(t));
    spi_message_init(&m);
    t.tx_buf = txbuf;
    t.rx_buf = rxbuf;
    t.bits_per_word = 8;
    t.len = len;
#if QUALCOMM_REE_DEASSERT
    t.speed_hz = qualcomm_deassert;
#else
    t.speed_hz = sf_ctl_dev.pdev->max_speed_hz;
#endif
    spi_message_add_tail(&t, &m);
    return spi_sync(sf_ctl_dev.pdev, &m);
}
#endif

#if SF_REE_PLATFORM
static ssize_t sf_ctl_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    const size_t bufsiz = sf_ctl_dev.spi_buf_size;
    ssize_t status = 0;

    /* chipselect only toggles at start or end of operation */
    if (count > bufsiz) {
        return (-EMSGSIZE);
    }

    if (sf_ctl_dev.spi_buffer == NULL) {
        sf_ctl_dev.spi_buffer = kmalloc(bufsiz, GFP_KERNEL);

        if (sf_ctl_dev.spi_buffer == NULL) {
            xprintk(KERN_ERR, " %s malloc spi_buffer failed.\n", __FUNCTION__);
            return (-ENOMEM);
        }
    }

    memset(sf_ctl_dev.spi_buffer, 0, bufsiz);

    if (copy_from_user(sf_ctl_dev.spi_buffer, buf, count)) {
        xprintk(KERN_ERR, "%s copy_from_user(..) failed.\n", __FUNCTION__);
        return (-EMSGSIZE);
    }

    {
        /* not used */
        void *sf_conf;
        int cfg_len = 0;
        status = tee_spi_transfer(sf_conf, cfg_len, sf_ctl_dev.spi_buffer, sf_ctl_dev.spi_buffer, count);
    }

    if (status == 0) {
        status = copy_to_user(buf, sf_ctl_dev.spi_buffer, count);

        if (status != 0) {
            status = -EFAULT;
        }
        else {
            status = count;
        }
    }
    else {
        xprintk(KERN_ERR, " %s spi_transfer failed.\n", __FUNCTION__);
    }

    return status;
}

static ssize_t sf_ctl_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    const size_t bufsiz = sf_ctl_dev.spi_buf_size;
    ssize_t status = 0;

    /* chipselect only toggles at start or end of operation */
    if (count > bufsiz) {
        return (-EMSGSIZE);
    }

    if (sf_ctl_dev.spi_buffer == NULL) {
        sf_ctl_dev.spi_buffer = kmalloc(bufsiz, GFP_KERNEL);

        if (sf_ctl_dev.spi_buffer == NULL) {
            xprintk(KERN_ERR, " %s malloc spi_buffer failed.\n", __FUNCTION__);
            return (-ENOMEM);
        }
    }

    memset(sf_ctl_dev.spi_buffer, 0, bufsiz);

    if (copy_from_user(sf_ctl_dev.spi_buffer, buf, count)) {
        xprintk(KERN_ERR, "%s copy_from_user(..) failed.\n", __FUNCTION__);
        return (-EMSGSIZE);
    }

    {
        /* not used */
        void *sf_conf;
        int cfg_len = 0;
        status = tee_spi_transfer(sf_conf, cfg_len, sf_ctl_dev.spi_buffer, sf_ctl_dev.spi_buffer, count);
    }

    if (status == 0) {
        status = count;
    }
    else {
        xprintk(KERN_ERR, " %s spi_transfer failed.\n", __FUNCTION__);
    }

    return status;
}
#endif

#if (SF_PROBE_ID_EN && !SF_RONGCARD_COMPATIBLE)
static int sf_read_sensor_id(void)
{
    int ret = -1;
    int trytimes = 3;
    char readbuf[16]  = {0};
    char writebuf[16] = {0};
    int cfg_len = 0;
    //默认速度设置为1M, 不然8201/8211系列有可能读不到ID
#if (defined(CONFIG_MTK_SPI) || SF_TRUSTKERNEL_COMPAT_SPI_MT65XX)
    smt_conf.high_time = SF_DEFAULT_SPI_HIGH_TIME;
    smt_conf.low_time  = SF_DEFAULT_SPI_LOW_TIME;
    smt_conf.com_mod   = FIFO_TRANSFER;
    smt_conf.cpol      = SPI_CPOL_0; // SPI_MODE_0: cpol = 0 and cpha = 0
    smt_conf.cpha      = SPI_CPHA_0;
#if (SF_COMPATIBLE_SEL == SF_COMPATIBLE_TRUSTKERNEL)
    cfg_len = sizeof(struct mt_chip_conf);
#else
    memcpy(&sf_ctl_dev.mt_conf, &smt_conf, sizeof(struct mt_chip_conf));
#endif
#else
    /* not used */
    int smt_conf;
#endif
    sf_ctl_dev.pdev->max_speed_hz = SF_DEFAULT_SPI_SPEED;
    sf_ctl_dev.pdev->bits_per_word = 8;
    sf_ctl_dev.pdev->mode = SPI_MODE_0;
    spi_setup(sf_ctl_dev.pdev);
    xprintk(SF_LOG_LEVEL, "%s(..) enter.\n", __FUNCTION__);
    msleep(10);

    do {
        /* 0.under display fingerprint */
        sf_ctl_dev.reset(false);
        msleep(1);
        sf_ctl_dev.reset(true);
        msleep(50);
        memset(readbuf,  0, sizeof(readbuf));
        memset(writebuf, 0, sizeof(writebuf));

        do {
            int ic_type = -1;
            char chip_id[2] = {0x00, 0x00};
            writebuf[0] = 0x00;
            writebuf[1] = 0x00;
            writebuf[2] = 0x00;
            writebuf[3] = 0x00;
            writebuf[4] = 0x00;
            ret =  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 5);

            if (ret != 0) {
                xprintk(KERN_ERR, "SPI transfer address 0x00 = 0x%02x failed, ret = %d\n", readbuf[4], ret);
                break;
            }

            chip_id[0] = readbuf[4];
            writebuf[0] = 0x00;
            writebuf[1] = 0x01;
            writebuf[2] = 0x00;
            writebuf[3] = 0x00;
            writebuf[4] = 0x00;
            ret =  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 5);

            if (ret != 0) {
                xprintk(KERN_ERR, "SPI transfer address 0x01 = 0x%02x failed, ret = %d\n", readbuf[4], ret);
                break;
            }

            chip_id[1] = readbuf[4];

            if ((chip_id[0] == 0x42) && (chip_id[1] == 0x53)) {
                ic_type = 1;
                xprintk(KERN_INFO, "%s: found ic_type 1\n", __FUNCTION__);
            }
            else if ((chip_id[0] == 0x53) && (chip_id[1] == 0x75)) {
                ic_type = 2;
                xprintk(KERN_INFO, "%s: found ic_type 2\n", __FUNCTION__);
            }
            else {
                ic_type = -1;
                xprintk(KERN_ERR, "%s: unknown chip_id = 0x%02x, 0x%02x\n", __FUNCTION__, chip_id[0], chip_id[1]);
                break;
            }

            if (ic_type == 1) {
                // I2C config 0x42
                writebuf[0] = 0x01;
                writebuf[1] = 0x42;
                writebuf[2] = 0x00;
                writebuf[3] = 0x00;
                writebuf[4] = 0x00;
                writebuf[5] = 0x00;
                ret =  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 7);

                if (ret != 0 || readbuf[6] != writebuf[2]) {
                    xprintk(KERN_ERR, "SPI transfer readbuf[6] = %x failed, ret = %d\n", readbuf[6], ret);
                    break;
                }

                // I2C config 0x43
                writebuf[0] = 0x01;
                writebuf[1] = 0x43;
                writebuf[2] = 0x3b;
                writebuf[3] = 0x00;
                writebuf[4] = 0x00;
                writebuf[5] = 0x00;
                ret =  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 7);

                if (ret != 0 || readbuf[6] != writebuf[2]) {
                    xprintk(KERN_ERR, "SPI transfer readbuf[6] = %x failed, ret = %d\n", readbuf[6], ret);
                    break;
                }

                // I2C config 0x40
                writebuf[0] = 0x01;
                writebuf[1] = 0x40;
                writebuf[2] = 0x80;
                writebuf[3] = 0x00;
                writebuf[4] = 0x00;
                writebuf[5] = 0x00;
                ret =  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 7);

                if (ret != 0 || readbuf[6] != writebuf[2]) {
                    xprintk(KERN_ERR, "SPI transfer readbuf[6] = %x failed, ret = %d\n", readbuf[6], ret);
                    break;
                }

                // read chip ID 0x31 or 0x32
                writebuf[0] = 0x03;
                writebuf[1] = 0x49;
                writebuf[2] = 0x31;
                writebuf[3] = 0x08;
                writebuf[4] = (0x60 | 0x01);
                ret =  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 5);
                msleep(2);
                writebuf[0] = 0x00;
                writebuf[1] = 0x47;
                writebuf[2] = 0x00;
                writebuf[3] = 0x00;
                ret +=  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 5);

                if (ret != 0 || ((readbuf[4] != 0x31) && (readbuf[4] != 0x32))) {
                    xprintk(KERN_ERR, "SPI transfer readbuf[4] = 0x%02x failed, ret = %d\n", readbuf[4], ret);
                    break;
                }

                xprintk(KERN_INFO, "read under display chip type %d is ok\n", ic_type);
                return 0;
            }
            else { // ic_type == 2
                writebuf[0]  = 0xaa;
                writebuf[1]  = 0x06;
                writebuf[2]  = 0x00;
                writebuf[3]  = 0x00;
                writebuf[4]  = 0x00;
                writebuf[5]  = 0x00;
                writebuf[6]  = 0x04;
                writebuf[7]  = 0x31;
                writebuf[8]  = 0x08;
                writebuf[9]  = 0x00;
                writebuf[10] = 0x01;
                writebuf[11] = 0x00;
                writebuf[12] = 0x00;
                writebuf[13] = 0x00;
                ret = tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 14);
                msleep(2);
                writebuf[0]  = 0xaa;
                writebuf[1]  = 0x07;
                writebuf[2]  = 0x00;
                writebuf[3]  = 0x00;
                writebuf[4]  = 0x00;
                writebuf[5]  = 0x00;
                writebuf[6]  = 0x01;
                writebuf[7]  = 0x00;
                writebuf[8]  = 0x00;
                writebuf[9]  = 0x00;
                writebuf[10] = 0x00;
                ret += tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 11);

                if (ret != 0 || ((readbuf[7] != 0x31) && (readbuf[7] != 0x32))) {
                    xprintk(KERN_ERR, "SPI transfer readbuf[7] = 0x%02x failed, ret = %d\n", readbuf[7], ret);
                    break;
                }

                xprintk(KERN_INFO, "read under display chip type %d is ok\n", ic_type);
                return 0;
            }
        }
        while (0);

        /* 1.detect Fortsense ID */
        sf_ctl_dev.reset(false);
        msleep(1);
        sf_ctl_dev.reset(true);
        msleep(10);
        memset(readbuf,  0, sizeof(readbuf));
        memset(writebuf, 0, sizeof(writebuf));
        writebuf[0] = 0x65;
        writebuf[1] = (uint8_t)(~0x65);
        writebuf[2] = 0x00;
        writebuf[3] = 0x04;
        writebuf[4] = 0x20;
        writebuf[5] = 0x04;
        writebuf[6] = 0x20;
        ret = tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 11);

        if (ret != 0) {
            xprintk(KERN_ERR, "SPI transfer failed\n");
            continue;
        }

        xprintk(KERN_INFO, " tee_spi_transfer.readbuf = 0x%02x-%02x-%02x-%02x\n", readbuf[7], readbuf[8], readbuf[9],
                readbuf[10]);

        if (((0x53 == readbuf[7]) && (0x75 == readbuf[8]) && (0x6e == readbuf[9]) && (0x57 == readbuf[10]))
            || ((0x46 == readbuf[7]) && (0x74 == readbuf[8]) && (0x53 == readbuf[9]) && (0x73 == readbuf[10]))) {
            xprintk(KERN_INFO, "read fortsense id is ok\n");
            return 0;
        }

        /* 2.detect 8205, 8231, 8241 or 8271 */
        sf_ctl_dev.reset(false);
        msleep(1);
        sf_ctl_dev.reset(true);
        msleep(10);
        memset(readbuf,  0, sizeof(readbuf));
        memset(writebuf, 0, sizeof(writebuf));
        writebuf[0] = 0xA0;
        writebuf[1] = (uint8_t)(~0xA0);
        ret =  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 6);

        if (ret != 0) {
            xprintk(KERN_ERR, "SPI transfer failed\n");
            continue;
        }

        if ((0x53 == readbuf[2]) && (0x75 == readbuf[3]) && (0x6e == readbuf[4])
            && (0x57 == readbuf[5])) {
            xprintk(KERN_INFO, "read chip is ok\n");
            return 0;
        }

        /* 3.detect 8202, 8205 or 8231 */
        sf_ctl_dev.reset(false);
        msleep(1);
        sf_ctl_dev.reset(true);
        msleep(10);
        memset(readbuf,  0, sizeof(readbuf));
        memset(writebuf, 0, sizeof(writebuf));
        writebuf[0] = 0x60;
        writebuf[1] = (uint8_t)(~0x60);
        writebuf[2] = 0x28;
        writebuf[3] = 0x02;
        writebuf[4] = 0x00;
        ret =  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 7);

        if (ret != 0) {
            xprintk(KERN_ERR, "SPI transfer failed\n");
            continue;
        }

        if (readbuf[5] == 0x82) {
            xprintk(KERN_INFO, "read chip is ok\n");
            return 0;
        }

        /* 4.detect 8221 */
        sf_ctl_dev.reset(false);
        msleep(1);
        sf_ctl_dev.reset(true);
        msleep(10);
        memset(readbuf,  0, sizeof(readbuf));
        memset(writebuf, 0, sizeof(writebuf));
        writebuf[0] = 0x60;
        writebuf[1] = 0x28;
        writebuf[2] = 0x02;
        writebuf[3] = 0x00;
        ret =  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 6);

        if (ret != 0) {
            xprintk(KERN_ERR, "SPI transfer failed\n");
            continue;
        }

        if (readbuf[4] == 0x82) {
            xprintk(KERN_INFO, "read chip is ok\n");
            return 0;
        }

        /* trustkernel bug, can not read more than 8 bytes ,
        #if 1, trustkernel has fix it with 12bytes in version 0.5.2,
        show trustkernel version you can see it by cat /proc/tkcore/tkcore_os_version*/
#if !SF_TRUSTKERNEL_COMPATIBLE
        //#if 1
        /* 5.detect 8211 */
        {
            int retry_8201 = 0;

            do {
                sf_ctl_dev.reset(false);
                msleep(1);
                sf_ctl_dev.reset(true);
                msleep(10);
                /* reset 脚拉高后，需在 6ms~26ms 内读 ID，小于 6ms 或者超过 26ms 都读不到 ID */
                msleep(1 + retry_8201 * 2);
                memset(readbuf,  0, sizeof(readbuf));
                memset(writebuf, 0, sizeof(writebuf));
                writebuf[0] = 0x32;
                writebuf[1] = (uint8_t)(~0x32);
                writebuf[2] = 0x00;
                writebuf[3] = 0x00;
                writebuf[4] = 0x84;
                ret =  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 5);

                if (ret != 0) {
                    xprintk(KERN_ERR, "SPI transfer failed step 1\n");
                    continue;
                }

                msleep(1);
                memset(readbuf,  0, sizeof(readbuf));
                memset(writebuf, 0, sizeof(writebuf));
                writebuf[0] = 0x32;
                writebuf[1] = (uint8_t)(~0x32);
                writebuf[2] = 0x00;
                writebuf[3] = 0x00;
                writebuf[4] = 0x81;
                ret =  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 5);

                if (ret != 0) {
                    xprintk(KERN_ERR, "SPI transfer failed step 2\n");
                    continue;
                }

                msleep(5);
                memset(readbuf,  0, sizeof(readbuf));
                memset(writebuf, 0, sizeof(writebuf));
                writebuf[0] = 0x81;
                writebuf[1] = (uint8_t)(~0x81);
                writebuf[2] = 0x00;
                writebuf[3] = 0x00;
                writebuf[4] = 0x21;
                writebuf[5] = 0x00;
                writebuf[6] = 0x00;
                writebuf[7] = 0x00;
                ret =  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 10);

                if (ret != 0) {
                    xprintk(KERN_ERR, "SPI transfer failed step 3\n");
                    continue;
                }

                if (readbuf[8] == 0x82) {
                    xprintk(KERN_INFO, "read chip is ok: 0x%02x%02x\n", readbuf[8], readbuf[9]);
                    memset(readbuf,  0, sizeof(readbuf));
                    memset(writebuf, 0, sizeof(writebuf));
                    writebuf[0] = 0x32;
                    writebuf[1] = (uint8_t)(~0x32);
                    writebuf[2] = 0x00;
                    writebuf[3] = 0x00;
                    writebuf[4] = 0x83;
                    ret =  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 5);
                    return 0;
                }
                else {
                    xprintk(KERN_ERR, "read chip: 0x%02x%02x\n", readbuf[8], readbuf[9]);
                }
            }
            while (retry_8201++ < 10);
        }
#endif
    }
    while (trytimes--);

    return -1;
}

#endif

////////////////////////////////////////////////////////////////////////////////
static int sf_ctl_init_irq(void)
{
    int err = 0;
    unsigned long flags = IRQF_TRIGGER_FALLING; // IRQF_TRIGGER_FALLING or IRQF_TRIGGER_RISING
#if !SF_MTK_CPU
    flags |= IRQF_ONESHOT;
#if SF_INT_TRIG_HIGH
    flags |= IRQF_NO_SUSPEND;
#endif
#endif
    xprintk(SF_LOG_LEVEL, "%s(..) enter.\n", __FUNCTION__);
    /* Register interrupt callback. */
    err = request_irq(sf_ctl_dev.irq_num, sf_ctl_device_irq,
                      flags, "sf-irq", NULL);

    if (err) {
        xprintk(KERN_ERR, "request_irq(..) = %d.\n", err);
    }

    enable_irq_wake(sf_ctl_dev.irq_num);
    xprintk(SF_LOG_LEVEL, "%s(..) leave.\n", __FUNCTION__);
    return err;
}

static int sf_ctl_init_input(void)
{
    int err = 0;
    xprintk(SF_LOG_LEVEL, "%s(..) enter.\n", __FUNCTION__);
    sf_ctl_dev.input = input_allocate_device();

    if (!sf_ctl_dev.input) {
        xprintk(KERN_ERR, "input_allocate_device(..) failed.\n");
        return (-ENOMEM);
    }

    sf_ctl_dev.input->name = "uinput-fortsense";
    sf_ctl_dev.input->id.vendor  = 0x0666;
    sf_ctl_dev.input->id.product = 0x0888;
    __set_bit(EV_KEY,     sf_ctl_dev.input->evbit );
    __set_bit(KEY_HOME,   sf_ctl_dev.input->keybit);
    __set_bit(KEY_MENU,   sf_ctl_dev.input->keybit);
    __set_bit(KEY_BACK,   sf_ctl_dev.input->keybit);
    __set_bit(XIAOMI_KEYCODE_SINGLE_CLK, sf_ctl_dev.input->keybit);
    __set_bit(KEY_ENTER,  sf_ctl_dev.input->keybit);
    __set_bit(KEY_UP,     sf_ctl_dev.input->keybit);
    __set_bit(KEY_LEFT,   sf_ctl_dev.input->keybit);
    __set_bit(KEY_RIGHT,  sf_ctl_dev.input->keybit);
    __set_bit(KEY_DOWN,   sf_ctl_dev.input->keybit);
    __set_bit(XIAOMI_KEYCODE_DOUBLE_CLK, sf_ctl_dev.input->keybit);
    err = input_register_device(sf_ctl_dev.input);

    if (err) {
        xprintk(KERN_ERR, "input_register_device(..) = %d.\n", err);
        input_free_device(sf_ctl_dev.input);
        sf_ctl_dev.input = NULL;
        return (-ENODEV);
    }

    xprintk(SF_LOG_LEVEL, "%s(..) leave.\n", __FUNCTION__);
    return err;
}

static int __init sf_ctl_driver_init(void)
{
    int err = 0;
    xprintk(KERN_INFO, "'%s' SW_BUS_NAME = %s\n", __FUNCTION__, SW_BUS_NAME);
#if SF_BEANPOD_COMPATIBLE_V1
    uint64_t fp_vendor_id = 0x00;
    get_t_device_id(&fp_vendor_id);
    xprintk(KERN_INFO, "'%s' fp_vendor_id = 0x%x\n", __FUNCTION__, fp_vendor_id);

    if (fp_vendor_id != 0x02) {
        return 0;
    }

#endif
#if SF_SPI_RW_EN
    /**register SPI device、driver***/
    spi_register_board_info(spi_board_devs, ARRAY_SIZE(spi_board_devs));
    err = spi_register_driver(&sf_driver);

    if (err < 0) {
        xprintk(KERN_ERR, "%s, Failed to register SPI driver.\n", __FUNCTION__);
    }

#else
#if SF_REG_DEVICE_BY_DRIVER
    sf_device = platform_device_alloc("fortsense-fp", 0);

    if (sf_device) {
        err = platform_device_add(sf_device);

        if (err) {
            platform_device_put(sf_device);
            sf_device = NULL;
        }
    }
    else {
        err = -ENOMEM;
    }

#endif

    if (err) {
        xprintk(KERN_ERR, "%s, Failed to register platform device.\n", __FUNCTION__);
    }

    err = platform_driver_register(&sf_driver);

    if (err) {
        xprintk(KERN_ERR, "%s, Failed to register platform driver.\n", __FUNCTION__);
        return -EINVAL;
    }

#endif
    xprintk(KERN_INFO, "fortsense fingerprint device control driver registered.\n");
    xprintk(KERN_INFO, "driver version: '%s'.\n", sf_ctl_get_version());
    return err;
}

static void __exit sf_ctl_driver_exit(void)
{
#if SF_SPI_RW_EN
    spi_unregister_driver(&sf_driver);
#else
    platform_driver_unregister(&sf_driver);
#endif
#if SF_REG_DEVICE_BY_DRIVER

    if (sf_device) {
        platform_device_unregister(sf_device);
    }

#endif
    xprintk(KERN_INFO, "fortsense fingerprint device control driver released.\n");
}

module_init(sf_ctl_driver_init);
module_exit(sf_ctl_driver_exit);

MODULE_DESCRIPTION("The device control driver for Fortsense's fingerprint sensor.");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Fortsense");

