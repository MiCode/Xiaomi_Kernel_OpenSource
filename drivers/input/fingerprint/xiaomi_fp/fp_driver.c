
/*** Copyright (C) 2018 XiaoMi, Inc. ***/

#include "fp_driver.h"
#define WAKELOCK_HOLD_TIME 3000 /* in ms */
#define FP_UNLOCK_REJECTION_TIMEOUT (WAKELOCK_HOLD_TIME - 500) /*ms*/
#define CONFIG_FOD_FINGERPRINT
//#define XIAOMI_DRM_INTERFACE_WA
#ifdef XIAOMI_DRM_INTERFACE_WA
#include "../../../gpu/drm/mediatek/mediatek_v2/mtk_disp_notify.h"
#endif
/*device name after register in charater*/
#define FP_DEV_NAME "xiaomi-fp"
#define FP_CLASS_NAME "xiaomi_fp"
#define FP_INPUT_NAME "uinput-xiaomi"

#ifdef CONFIG_FP_MTK_PLATFORM
/*#include "teei_fp.h"*/
/*#include "tee_client_api.h"*/

extern void mt_spi_enable_master_clk(struct spi_device *spidev);
extern void mt_spi_disable_master_clk(struct spi_device *spidev);

static atomic_t clk_ref = ATOMIC_INIT(0);
static void fp_spi_clk_enable(struct spi_device *spi)
{
    if (atomic_read(&clk_ref) == 0) {
        pr_debug("enable spi clk\n");
        mt_spi_enable_master_clk(spi);
        atomic_inc(&clk_ref);
        pr_debug("increase spi clk ref to %d\n", atomic_read(&clk_ref));
    }
}
static void fp_spi_clk_disable(struct spi_device *spi)
{
    if (atomic_read(&clk_ref) == 1) {
        atomic_dec(&clk_ref);
        pr_debug(" disable spi clk\n");
        mt_spi_disable_master_clk(spi);
        pr_debug("decrease spi clk ref to %d\n", atomic_read(&clk_ref));
    }
}
#endif

/*************************************************************/
static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);
static struct wakeup_source *fp_wakesrc = NULL;
struct work_struct fp_display_work;
static struct fp_device fp;

static struct fp_key_map maps[] = {
    { EV_KEY, FP_KEY_INPUT_HOME },
    { EV_KEY, FP_KEY_INPUT_MENU },
    { EV_KEY, FP_KEY_INPUT_BACK },
    { EV_KEY, FP_KEY_INPUT_POWER },
};

#ifdef XIAOMI_DRM_INTERFACE_WA

static int fp_fb_notifier_callback(struct notifier_block *self,
                   unsigned long event, void *data)
{
    int *blank = (int *)data;
    struct fp_device *fp_dev = NULL;
    FUNC_ENTRY();

    if (!(event == MTK_DISP_EARLY_EVENT_BLANK ||
          event == MTK_DISP_EVENT_BLANK)) {
        pr_info("event(%lu) do not need process\n", event);
        return NOTIFY_OK;
    }
    fp_dev = container_of(self, struct fp_device, notifier);
    switch (event) {
    case MTK_DISP_EVENT_BLANK:
        if (*blank == MTK_DISP_BLANK_UNBLANK) {
            pr_debug("[%s] : lcd on notify\n", __func__);
            fp_netlink_send(fp_dev, FP_NETLINK_SCREEN_ON);
        }
        break;
    case MTK_DISP_EARLY_EVENT_BLANK:
        if (*blank == MTK_DISP_BLANK_POWERDOWN) {
            pr_debug("[%s] : lcd off notify\n", __func__);
            fp_netlink_send(fp_dev, FP_NETLINK_SCREEN_OFF);
        }
        break;
    default:
        pr_debug("[%s] : other notifier, ignore\n", __func__);
        break;
    }

    FUNC_EXIT();
    printk("fp_fb_notifier_callback end.\n");
    return NOTIFY_OK;
}
#endif /*display_fb_state_chg_callback*/

static irqreturn_t fp_irq(int irq, void *handle)
{
    struct fp_device *fp_dev = (struct fp_device *)handle;
#ifdef XIAOMI_DRM_INTERFACE_WA
    uint32_t key_input = 0;
#endif
    FUNC_ENTRY();
    __pm_wakeup_event(fp_wakesrc, WAKELOCK_HOLD_TIME);
    fp_netlink_send(fp_dev, FP_NETLINK_IRQ);
#ifdef XIAOMI_DRM_INTERFACE_WA
    if ((fp_dev->wait_finger_down == true) && (fp_dev->fb_black == 1)) {
        key_input = KEY_RIGHT;
        input_report_key(fp_dev->input, key_input, 1);
        input_sync(fp_dev->input);
        input_report_key(fp_dev->input, key_input, 0);
        input_sync(fp_dev->input);
        fp_dev->wait_finger_down = false;
        schedule_work(&fp_display_work);
    }
#endif
    FUNC_EXIT();
    return IRQ_HANDLED;
}

/* -------------------------------------------------------------------- */
/* file operation function                                              */
/* -------------------------------------------------------------------- */

static long fp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct fp_device *fp_dev = &fp;
    struct fp_key fp_key;
    int retval = 0;
    u8 buf = 0;
    u8 netlink_route = fp_dev->fp_netlink_num;
    struct fp_ioc_chip_info info;

    FUNC_ENTRY();
    if (_IOC_TYPE(cmd) != FP_IOC_MAGIC)
        return -EINVAL;

    if (_IOC_DIR(cmd) & _IOC_READ)
        retval = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
    if (retval == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
        retval = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
    if (retval)
        return -EINVAL;

    switch (cmd) {
    case FP_IOC_INIT:
        pr_debug("FP_IOC_INIT ======\n");
        if (fp_dev->fp_netlink_num <= 0) {
            pr_err("netlink init fail,check dts config.");
            retval = -EFAULT;
            break;
        }
        if (fp_dev->fp_netlink_enabled == 0) {
            retval = fp_netlink_init(fp_dev);
            if (retval != 0) {
                break;
            }
        }
        if (copy_to_user((void __user *)arg, (void *)&netlink_route,
                 sizeof(u8))) {
            retval = -EFAULT;
            break;
        }
        fp_dev->fp_netlink_enabled = 1;
        break;

    case FP_IOC_EXIT:
        pr_debug("FP_IOC_EXIT ======\n");
        fp_disable_irq(fp_dev);
        if (fp_dev->fp_netlink_enabled)
            fp_netlink_destroy(fp_dev);
        fp_dev->fp_netlink_enabled = 0;
        fp_dev->device_available = 0;
        break;

    case FP_IOC_ENABLE_IRQ:
        pr_debug("FP_IOC_ENABLE_IRQ ======\n");
        fp_enable_irq(fp_dev);
        break;

    case FP_IOC_DISABLE_IRQ:
        pr_debug(" FP_IOC_DISABLE_IRQ ======\n");
        fp_disable_irq(fp_dev);
        break;

    case FP_IOC_RESET:
        pr_debug("FP_IOC_RESET  ======\n");
        fp_hw_reset(fp_dev, 60);
        break;

    case FP_IOC_ENABLE_POWER:
        pr_debug(" FP_IOC_ENABLE_POWER ======\n");
        if (fp_dev->device_available == 1) {
            pr_debug("Sensor has already powered-on.\n");
        } else {
            fp_power_on(fp_dev);
            fp_dev->device_available = 1;
        }
        break;

    case FP_IOC_DISABLE_POWER:
        pr_debug(" FP_IOC_DISABLE_POWER ======\n");
        if (fp_dev->device_available == 0) {
            pr_debug("Sensor has already powered-off.\n");
        } else {
            fp_power_off(fp_dev);
            fp_dev->device_available = 0;
        }
        break;

    case FP_IOC_ENABLE_SPI_CLK:
#ifdef CONFIG_FP_MTK_PLATFORM
        pr_debug(" FP_IOC_ENABLE_SPI_CLK ======\n");
        fp_spi_clk_enable(fp_dev->driver_device);
#endif
        break;

    case FP_IOC_DISABLE_SPI_CLK:
#ifdef CONFIG_FP_MTK_PLATFORM
        pr_debug(" FP_IOC_DISABLE_SPI_CLK ======\n");
        fp_spi_clk_disable(fp_dev->driver_device);
#endif
        break;

    case FP_IOC_INPUT_KEY_EVENT:
        pr_debug(" FP_IOC_INPUT_KEY_EVENT ======\n");
        if (copy_from_user(&fp_key, (struct fp_key *)arg,
                   sizeof(struct fp_key))) {
            pr_debug(
                "Failed to copy input key event from user to kernel\n");
            retval = -EFAULT;
            break;
        }
        fp_kernel_key_input(fp_dev, &fp_key);
        break;

    case FP_IOC_ENTER_SLEEP_MODE:
        pr_debug(" FP_IOC_ENTER_SLEEP_MODE ======\n");
        break;

    case FP_IOC_GET_FW_INFO:
        pr_debug(" FP_IOC_GET_FW_INFO ======\n");
        pr_debug(" firmware info  0x%x\n", buf);
        if (copy_to_user((void __user *)arg, (void *)&buf,
                 sizeof(u8))) {
            pr_debug("Failed to copy data to user\n");
            retval = -EFAULT;
        }
        break;

    case FP_IOC_REMOVE:
        pr_debug(" FP_IOC_REMOVE ======\n");
        break;

    case FP_IOC_CHIP_INFO:
        pr_debug(" FP_IOC_CHIP_INFO ======\n");
        if (copy_from_user(&info, (struct fp_ioc_chip_info *)arg,
                   sizeof(struct fp_ioc_chip_info))) {
            retval = -EFAULT;
            break;
        }
        break;

    default:
        pr_debug("fp doesn't support this command(%x)\n", cmd);
        break;
    }

    return retval;
}

#ifdef CONFIG_COMPAT
static long fp_compat_ioctl(struct file *filp, unsigned int cmd,
                unsigned long arg)
{
    int retval = 0;
    FUNC_ENTRY();
    retval = filp->f_op->unlocked_ioctl(filp, cmd, arg);
    return retval;
}
#endif

/* -------------------------------------------------------------------- */
/* device function                            */
/* -------------------------------------------------------------------- */
static int fp_open(struct inode *inode, struct file *filp)
{
    struct fp_device *fp_dev = NULL;
    int status = -ENXIO;
#ifdef CONFIG_FP_MTK_PLATFORM
    int ret = 0;
#endif
    FUNC_ENTRY();
    mutex_lock(&device_list_lock);
    list_for_each_entry (fp_dev, &device_list, device_entry) {
        if (fp_dev->devt == inode->i_rdev) {
            pr_debug("  Found\n");
            status = 0;
            break;
        }
    }
    mutex_unlock(&device_list_lock);

    if (status == 0) {
        filp->private_data = fp_dev;
        nonseekable_open(inode, filp);
        pr_debug("Success to open device. irq = %d\n", fp_dev->irq_num);
        gpio_direction_input(fp_dev->irq_gpio);
        status =
            request_threaded_irq(fp_dev->irq_num, NULL, fp_irq,
                         IRQF_TRIGGER_RISING | IRQF_ONESHOT,
                         "xiaomi_fp_irq", fp_dev);
        if (!status) {
            pr_debug("irq thread request success!\n");
            fp_dev->irq_enabled = 1;
            fp_disable_irq(fp_dev);
        } else {
            pr_debug("irq thread request failed, status=%d\n", status);
        }
    } else {
        pr_debug("  No device for minor %d\n", iminor(inode));
    }

	fp_power_on(fp_dev);
	mdelay(10);
	pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_reset_high);
#ifdef CONFIG_FP_MTK_PLATFORM
	if (IS_ERR(fp_dev->pins_spiio_spi_mode)) {
		ret = PTR_ERR(fp_dev->pins_spiio_spi_mode);
		pr_debug("%s fingerprint pinctrl spiio_spi_mode NULL\n",
			 __func__);
		return ret;
	} else {
		pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_spiio_spi_mode);
	}
#endif

    FUNC_EXIT();
    return status;
}

static int fp_release(struct inode *inode, struct file *filp)
{
    struct fp_device *fp_dev = NULL;
    int status = 0;
    FUNC_ENTRY();
    fp_dev = filp->private_data;
    if (fp_dev->irq_num) {
        fp_disable_irq(fp_dev);
        free_irq(fp_dev->irq_num, fp_dev);
    }
    FUNC_EXIT();
    return status;
}

static unsigned int fp_poll(struct file *filp, poll_table *wait)
{
    struct fp_device *fp_dev = &fp;
    unsigned int mask = 0;
    FUNC_ENTRY();
    /*poll wait*/
    poll_wait(filp, &fp_dev->fp_wait_queue, wait);
    if (fp_dev->fp_poll_have_data)
        mask = POLLERR | POLLPRI;
    return mask;
}

static ssize_t fp_read(struct file *filp, char __user *buf, size_t size,
               loff_t *ppos)
{
    int ret = 0;
    struct fp_device *fp_dev = &fp;
    char value = 0;
    FUNC_ENTRY();
    value = (char)gpio_get_value(fp_dev->irq_gpio) + '0';
    if (copy_to_user(buf, &value, 1)) {
        ret = -EFAULT;
        pr_debug("mi_read copy_to_user error");
    } else {
        ret = 1;
        fp_dev->fp_poll_have_data = 0;
    }
    return ret;
}

static const struct file_operations fp_fops = {
    .owner = THIS_MODULE,
    .open = fp_open,
    .unlocked_ioctl = fp_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl = fp_compat_ioctl,
#endif
    .release = fp_release,
    .poll = fp_poll,
    .read = fp_read,
};

#ifdef CONFIG_FOD_FINGERPRINT
static ssize_t get_fingerdown_event(struct device *dev,
                                    struct device_attribute *attr,
                                    char *buf)
{
    struct fp_device *fp_dev = &fp;

    return snprintf(buf, PAGE_SIZE, "%d\n", fp_dev->fingerdown);
}

static ssize_t set_fingerdown_event(struct device *dev,
                                    struct device_attribute *attr,
                                    const char *buf, size_t count)
{
    struct fp_device *fp_dev = &fp;

    dev_info(fp_dev->device, "%s -> %s\n", __func__, buf);
    if (!strncmp(buf, "1", strlen("1"))) {
        fp_dev->fingerdown = 1;
        dev_info(dev, "%s set fingerdown 1 \n", __func__);
        sysfs_notify(&fp_dev->driver_device->dev.kobj, NULL, "fingerdown");
    } else if (!strncmp(buf, "0", strlen("0"))) {
        fp_dev->fingerdown = 0;
        dev_info(dev, "%s set fingerdown 0 \n", __func__);
    } else {
        dev_err(dev,"failed to set fingerdown\n");
        return -EINVAL;
    }
    return count;
}
static DEVICE_ATTR(fingerdown, S_IRUSR | S_IWUSR, get_fingerdown_event, set_fingerdown_event);

static struct attribute *attributes[] = {
	&dev_attr_fingerdown.attr,
	NULL
};

static const struct attribute_group attribute_group = {
    .attrs = attributes,
};

#endif

#ifdef CONFIG_FP_MTK_PLATFORM
static int fp_probe(struct spi_device *driver_device)
#else
static int fp_probe(struct platform_device *driver_device)
#endif
{
    struct fp_device *fp_dev = &fp;
    int status = -EINVAL;
#ifdef CONFIG_FP_MTK_PLATFORM
    int ret = 0;
#endif
    int i;
    FUNC_ENTRY();

    INIT_LIST_HEAD(&fp_dev->device_entry);

    fp_dev->irq_gpio = -EINVAL;
    fp_dev->device_available = 0;
    fp_dev->fb_black = 0;
    fp_dev->wait_finger_down = false;
    fp_dev->fp_netlink_enabled = 0;
    /*setup fp configurations. */
    pr_debug(" Setting fp device configuration==========\n");
    fp_dev->driver_device = driver_device;
    /* get gpio info from dts or defination */
    status = fp_parse_dts(fp_dev);
    if (status) {
        goto err_dts;
    }

    if (!IS_ERR(fp_dev->pins_eint_default)) {
        pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_eint_default);
    }

#ifdef CONFIG_FP_MTK_PLATFORM
    if (IS_ERR(fp_dev->pins_spiio_spi_mode)) {
        ret = PTR_ERR(fp_dev->pins_spiio_spi_mode);
        pr_debug("%s fingerprint pinctrl spiio_spi_mode NULL\n",
             __func__);
        return ret;
    } else {
        pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_spiio_spi_mode);
    }
#endif
    /* create class */
    fp_dev->class = class_create(/*THIS_MODULE, */FP_CLASS_NAME);
    if (IS_ERR(fp_dev->class)) {
        pr_debug("Failed to create class.\n");
        status = -ENODEV;
        goto err_class;
    }
    /* get device no */
    status = alloc_chrdev_region(&fp_dev->devt, 0, 1, FP_DEV_NAME);
    if (status < 0) {
        pr_debug("Failed to alloc devt.\n");
        goto err_devno;
    }
    /* create device */
    fp_dev->device = device_create(fp_dev->class, &driver_device->dev,
                       fp_dev->devt, fp_dev, FP_DEV_NAME);
    if (IS_ERR(fp_dev->device)) {
        pr_debug("  Failed to create device.\n");
        status = -ENODEV;
        goto err_device;
    } else {
        mutex_lock(&device_list_lock);
        list_add(&fp_dev->device_entry, &device_list);
        mutex_unlock(&device_list_lock);
    }
    /* cdev init and add */
    cdev_init(&fp_dev->cdev, &fp_fops);
    fp_dev->cdev.owner = THIS_MODULE;
    status = cdev_add(&fp_dev->cdev, fp_dev->devt, 1);
    if (status) {
        pr_debug("Failed to add cdev.\n");
        goto err_cdev;
    }
    /*register device within input system. */
    fp_dev->input = input_allocate_device();
    if (fp_dev->input == NULL) {
        pr_debug("Failed to allocate input device.\n");
        status = -ENOMEM;
        goto err_input;
    }
    for (i = 0; i < ARRAY_SIZE(maps); i++) {
        input_set_capability(fp_dev->input, maps[i].type, maps[i].code);
    }
    fp_dev->input->name = FP_INPUT_NAME;

    if (input_register_device(fp_dev->input)) {
        pr_debug("Failed to register input device.\n");
        status = -ENODEV;
        goto err_input_2;
    }
    init_waitqueue_head(&fp_dev->fp_wait_queue);
#ifdef XIAOMI_DRM_INTERFACE_WA
    /* register screen on/off callback */
    fp_dev->notifier.notifier_call = fp_fb_notifier_callback;
    ret = mtk_disp_notifier_register("fp_fb_notifier", &fp_dev->notifier);
    if (ret)
        printk("fp_fb_register_client fail.\n");
    else
        printk("fp_fb_register_client success.\n");
#endif
    /* netlink interface init */
    status = fp_netlink_init(fp_dev);
    if (status == -1) {
        input_unregister_device(fp_dev->input);
        fp_dev->input = NULL;
        goto err_input;
    }
    fp_dev->fp_netlink_enabled = 1;

    fp_wakesrc = wakeup_source_register(&fp_dev->driver_device->dev,
                        "fp_wakesrc");
#ifndef CONFIG_SIDE_FINGERPRINT
    if (device_may_wakeup(fp_dev->device)) {
        pr_debug("device_may_wakeup\n");
        disable_irq_wake(fp_dev->irq_num);
    }
    pr_debug("CONFIG_SIDE_FINGERPRINT not define, is FOD project\n");
#else
    enable_irq_wake(fp_dev->irq_num);
    pr_debug(
        "CONFIG_SIDE_FINGERPRINT define, is side fingerprint project\n");
#endif

#ifdef CONFIG_FOD_FINGERPRINT
    status = sysfs_create_group(&fp_dev->driver_device->dev.kobj, &attribute_group);
    if (status) {
        pr_debug("could not create sysfs\n");
    }
    fp_dev->fingerdown = 0;
#endif
    pr_debug("fp probe success");
    FUNC_EXIT();
    return 0;

err_input_2:

    if (fp_dev->input != NULL) {
        input_free_device(fp_dev->input);
        fp_dev->input = NULL;
    }

err_input:
    cdev_del(&fp_dev->cdev);

err_cdev:
    device_destroy(fp_dev->class, fp_dev->devt);
    list_del(&fp_dev->device_entry);

err_device:
    unregister_chrdev_region(fp_dev->devt, 1);

err_devno:
    class_destroy(fp_dev->class);

err_class:
#ifdef CONFIG_FP_MTK_PLATFORM
    fp_spi_clk_disable(fp_dev->driver_device);
#endif
    fp_power_off(fp_dev);

err_dts:
    fp_dev->driver_device = NULL;
    fp_dev->device_available = 0;
    pr_debug("fp probe fail\n");
    FUNC_EXIT();
    return status;
}

#ifdef CONFIG_FP_MTK_PLATFORM
static void fp_remove(struct spi_device *driver_device)
#else
static int fp_remove(struct platform_device *driver_device)
#endif
{
    struct fp_device *fp_dev = &fp;
    FUNC_ENTRY();
    wakeup_source_unregister(fp_wakesrc);
    fp_wakesrc = NULL;
    /* make sure ops on existing fds can abort cleanly */
    if (fp_dev->irq_num) {
        free_irq(fp_dev->irq_num, fp_dev);
        fp_dev->irq_enabled = 0;
        fp_dev->irq_num = 0;
    }

#ifdef XIAOMI_DRM_INTERFACE_WA
    mtk_disp_notifier_unregister(&fp_dev->notifier);
#endif

    if (fp_dev->input != NULL) {
        input_unregister_device(fp_dev->input);
        input_free_device(fp_dev->input);
    }
    fp_netlink_destroy(fp_dev);
    fp_dev->fp_netlink_enabled = 0;
    cdev_del(&fp_dev->cdev);
    device_destroy(fp_dev->class, fp_dev->devt);
    list_del(&fp_dev->device_entry);

    unregister_chrdev_region(fp_dev->devt, 1);
    class_destroy(fp_dev->class);

#ifdef CONFIG_FOD_FINGERPRINT
     sysfs_remove_group(&fp_dev->driver_device->dev.kobj, &attribute_group);
#endif

#ifdef CONFIG_FP_MTK_PLATFORM
    if (!IS_ERR(fp_dev->pins_spiio_spi_mode)) {
        pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_spiio_gpio_mode);
    }
#endif

    if (!IS_ERR(fp_dev->pins_reset_low)) {
        pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_reset_low);
    }
    if (!IS_ERR(fp_dev->pins_eint_default)) {
        pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_eint_default);
    }
    fp_power_off(fp_dev);
    fp_dev->driver_device = NULL;
    FUNC_EXIT();
#ifdef CONFIG_FP_MTK_PLATFORM
        return;
#else
        return 0;
#endif
}

static const struct of_device_id fp_of_match[] = {
    {
        .compatible = DRIVER_COMPATIBLE,
    },
    {},
};
MODULE_DEVICE_TABLE(of, fp_of_match);

#ifdef CONFIG_FP_MTK_PLATFORM
static struct spi_driver fp_spi_driver = {
#else
static struct platform_driver fp_platform_driver = {
#endif
    .driver = {
           .name = FP_DEV_NAME,
           .owner = THIS_MODULE,
           .of_match_table = fp_of_match,
           },
    .probe = fp_probe,
    .remove = fp_remove,
};

/*-------------------------------------------------------------------------*/
static int __init fp_init(void)
{
    int status = 0;
    FUNC_ENTRY();
#ifdef CONFIG_FP_MTK_PLATFORM
    status = spi_register_driver(&fp_spi_driver);
#else
    status = platform_driver_register(&fp_platform_driver);
#endif
    if (status < 0) {
        pr_debug("Failed to register fp driver.\n");
        return -EINVAL;
    }
    FUNC_EXIT();
    return status;
}

module_init(fp_init);

static void __exit fp_exit(void)
{
    FUNC_ENTRY();
#ifdef CONFIG_FP_MTK_PLATFORM
    spi_unregister_driver(&fp_spi_driver);
#else /*  */
    platform_driver_unregister(&fp_platform_driver);
#endif
    FUNC_EXIT();
}

module_exit(fp_exit);

MODULE_AUTHOR("xiaomi");
MODULE_DESCRIPTION("xiaomi Fingerprint driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("xiaomi-fp");
