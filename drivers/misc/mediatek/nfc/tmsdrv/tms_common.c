/*****************************************************************************************
 * Copyright (c)  2021- 2022  TsingTeng MicroSystem Corp.Ltd.
 * TMS_EDIT
 * File       : tms_common.c
 * Description: Source file for tms device common
 * Version    : 1.0
 * Date       : 2022/4/11
 * Team       : NFC Middleware
 * Author     : Guoliang Wu
 * --------------------------------- Revision History: ---------------------------------
 *   <version>    <date>          < author >                            <desc>
 *******************************************************************************************/
#include "tms_common.h"

/*********** PART0: Global Variables Area ***********/
struct tms_info *tms = NULL;
unsigned int tms_debug = LEVEL_DUMP;

/*********** PART1: Declare Area ***********/

/*********** PART2: Function Area ***********/
void tms_buffer_dump(const char *tag, const uint8_t *src, int16_t len)
{
    uint16_t buf_len = (len > PAGESIZE) ? PAGESIZE : len;
    uint16_t index = 0;
    uint16_t i;

    if (tms_debug != LEVEL_DUMP) {
        TMS_DEBUG("%s[%d] bytes", tag, len);
    } else {
        char buf[PAGESIZE * 2 + 1];
        do {
            memset(buf, 0, sizeof(buf));

            for (i = 0; i < buf_len; i++) {
                snprintf(&buf[i * 2], 3, "%02X", src[index++]);
            }

            TMS_ERR("%s[%d] %s", tag, buf_len, buf);
            len = len - buf_len;
            buf_len = (len > PAGESIZE) ? PAGESIZE : len;
        } while (len > 0);
    }
}

struct tms_info *tms_common_data_binding(void)
{
    return tms;
}

void ven_gpio_set(struct hw_resource hw_res, bool state)
{
    if (gpio_get_value(hw_res.ven_gpio) != state) {
        TMS_DEBUG("Ven gpio %d state = %d\n", hw_res.ven_gpio, state);
        gpio_set_value(hw_res.ven_gpio, state);
    } else {
        TMS_ERR("VEN gpio value is %d already\n", gpio_get_value(hw_res.ven_gpio));
    }
}

void download_gpio_set(struct hw_resource hw_res, bool state)
{
    if (gpio_is_valid(hw_res.download_gpio)) {
        TMS_DEBUG("FW download gpio %d state = %d\n", hw_res.download_gpio,
                  state);
        gpio_set_value(hw_res.download_gpio, state);
        /* hardware dependent delay */
        usleep_range(GPIO_SET_WAIT_TIME_US, GPIO_SET_WAIT_TIME_US + 100);
    } else {
        TMS_ERR("FW DOWNLOAD gpio value is %d already\n", gpio_get_value(hw_res.download_gpio));
    }
}

void reset_gpio_set(struct hw_resource hw_res, bool state)
{
    if (gpio_is_valid(hw_res.rst_gpio)) {
        TMS_DEBUG("Reset gpio %d state = %d\n", hw_res.rst_gpio, state);
        gpio_set_value(hw_res.rst_gpio, state);
        /* hardware dependent delay */
        usleep_range(GPIO_SET_WAIT_TIME_US, GPIO_SET_WAIT_TIME_US + 100);
    } else {
        TMS_ERR("RESET gpio value is %d already\n", gpio_get_value(hw_res.rst_gpio));
    }
}

void tms_device_unregister(struct dev_register *dev)
{
    device_destroy(tms->class, dev->devno);
    cdev_del(&dev->chrdev);
    unregister_chrdev_region(dev->devno, dev->count);
    TMS_INFO("Unregister device\n");
}

int tms_device_register(struct dev_register *dev, void *data)
{
    int ret;
    TMS_INFO("start+\n");
    dev->class = tms->class;
    ret = alloc_chrdev_region(&dev->devno, 0, dev->count, dev->name);

    if (ret < 0) {
        TMS_ERR("Alloc chrdev region failed, ret %d\n", ret);
        return ret;
    }

    cdev_init(&dev->chrdev, dev->fops);
    ret = cdev_add(&dev->chrdev, dev->devno, dev->count);

    if (ret < 0) {
        TMS_ERR("Add char device failed, ret %d\n", ret);
        goto err_free_devno;
    }

    dev->creation = device_create(tms->class, NULL, dev->devno, data, dev->name);

    if (IS_ERR(dev->creation)) {
        ret = PTR_ERR(dev->creation);
        TMS_ERR("Create the device failed, ret %d\n", ret);
        goto err_delete_cdev;
    }

    TMS_INFO("Normal end-\n");
    return SUCCESS;
err_delete_cdev:
    cdev_del(&dev->chrdev);
err_free_devno:
    unregister_chrdev_region(dev->devno, dev->count);
    TMS_ERR("Error[%d] end-\n", ret);
    return ret;
}

static ssize_t proc_debug_control_read(struct file *file, char __user *buf,
                                       size_t count, loff_t *ppos)
{
    uint8_t ret;
    char page[PAGESIZE] = {0};
    TMS_DEBUG("Read debug_level is %d\n", tms_debug);
    snprintf(page, PAGESIZE - 1, "%u\n", tms_debug);
    ret = simple_read_from_buffer(buf, count, ppos, page, strlen(page));
    return ret;
}

static ssize_t proc_debug_control_write(struct file *file,
                                        const char __user *buf, size_t count, loff_t *lo)
{
    int tmp = 0;
    char buffer[4] = {0};

    if (count > 2) {
        TMS_ERR("Input format error.\n");
        return count;
    }

    if (copy_from_user(buffer, buf, count)) {
        TMS_ERR("Get debug level error.\n");
        return count;
    }

    if (sscanf(buffer, "%d", &tmp) == 1) {
        tms_debug = tmp;
        TMS_DEBUG("Set debug level is %d\n", tms_debug);
    } else {
        TMS_ERR("Invalid content:'%s', length = %zd\n", buf, count);
    }

    return count;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
static const struct proc_ops proc_debug_control_ops = {
    .proc_write = proc_debug_control_write,
    .proc_read  = proc_debug_control_read,
    .proc_open  = simple_open,
};
#else
static const struct file_operations proc_debug_control_ops = {
    .write = proc_debug_control_write,
    .read  = proc_debug_control_read,
    .open  = simple_open,
    .owner = THIS_MODULE,
};
#endif

/*********** PART3: TMS Common Start Area ***********/
static int tms_proc_init(void)
{
    int ret;
    struct proc_dir_entry *prEntry_tmp = NULL;
    TMS_INFO("Start+\n");
    tms->prEntry = proc_mkdir("tmsdev", NULL);

    if (tms->prEntry == NULL) {
        TMS_ERR("Couldn't create tmsdev proc entry\n");
        return -EFAULT;
    }

    prEntry_tmp = proc_create_data("debug_level", 0644, tms->prEntry,
                                   &proc_debug_control_ops, NULL);

    if (prEntry_tmp == NULL) {
        ret = -EFAULT;
        TMS_ERR("Couldn't create debug_level proc entry\n");
        goto err_remove_proc;
    }

    TMS_INFO("Normal end-\n");
    return SUCCESS;
err_remove_proc:
    proc_remove(tms->prEntry);
    TMS_ERR("Error[%d] end-\n", ret);
    return ret;
}

static int tms_common_probe(void)
{
    int ret;
    TMS_INFO("Start+\n");

    /* step1 : alloc tms common data */
    tms = kzalloc(sizeof(struct tms_info), GFP_KERNEL);

    if (tms == NULL) {
        TMS_ERR("TMS info alloc memory error\n");
        return -ENOMEM;
    }

    memset(tms, 0, sizeof(*tms));
    /* step2 : tms class register */
    tms->class = class_create(THIS_MODULE, DEVICES_CLASS_NAME);

    if (IS_ERR(tms->class)) {
        ret = PTR_ERR(tms->class);
        TMS_ERR("Failed to register device class ret %d\n", ret);
        goto err_free_tms_info;
    }

    /* step3 : binding common data and function */
    tms->nfc_name         = "thn31";
    tms->registe_device   = tms_device_register;
    tms->unregiste_device = tms_device_unregister;
    tms->set_ven          = ven_gpio_set;
    tms->set_download     = download_gpio_set;
    tms->set_reset        = reset_gpio_set;
    /* step4 : init proc */
    ret = tms_proc_init();

    if (ret) {
        TMS_ERR("NFC device proc create failed.\n");
        goto err_destroy_class;
    }

    TMS_INFO("Probe successfully\n");
    return SUCCESS;
err_destroy_class:
    class_destroy(tms->class);
err_free_tms_info:
    kfree(tms);
    tms = NULL;
    TMS_ERR("Probe failed, ret = %d\n", ret);
    return ret;
}

int tms_common_init(void)
{
    int ret;
    TMS_INFO("TMS Common init is called!\n");
    ret = tms_common_probe();

    if (ret) {
        TMS_ERR("Unable to init TMS Common, ret = %d\n", ret);
        return ret;
    }

    return SUCCESS;
}

void tms_common_exit(void)
{
    proc_remove(tms->prEntry);

    if (!IS_ERR(tms->class)) {
        class_destroy(tms->class);
    }

    if (tms) {
        kfree(tms);
        tms = NULL;
    }

    TMS_INFO("Remove TMS Common\n");
}

MODULE_DESCRIPTION("TMS Device Common");
MODULE_LICENSE("GPL");
