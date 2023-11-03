/*****************************************************************************************
 * Copyright (c)  2021- 2022  TsingTeng MicroSystem Corp.Ltd.
 * TMS_EDIT
 * File       : ese_driver.c
 * Description: Source file for tms ese driver
 * Version    : 1.0
 * Date       : 2022/4/11
 * Team       : NFC Middleware
 * Author     : Guoliang Wu
 * --------------------------------- Revision History: ---------------------------------
 *   <version>    <date>          < author >                            <desc>
 *******************************************************************************************/
#include "ese_driver.h"

#define CLOCK_SPI 1
#ifdef CLOCK_SPI
extern void mt_spi_enable_master_clk(struct spi_device *spidev);
extern void mt_spi_disable_master_clk(struct spi_device *spidev);

static void nfc_spi_clk_enable(struct ese_info *ese, u8 bonoff)
{
	static int count;

	if (bonoff) {
		if (count == 0) {
			pr_err("%s line:%d enable spi clk\n", __func__, __LINE__);
			mt_spi_enable_master_clk(ese->client);
		}
		count++;
	} else {
		count--;
		if (count == 0) {
			pr_err("%s line:%d disable spi clk\n", __func__, __LINE__);
			mt_spi_disable_master_clk(ese->client);
		} else if (count < 0) {
			count = 0;
		}
	}
}

#endif

/*********** PART0: Global Variables Area ***********/

/*********** PART1: Callback Function Area ***********/
int spi_block_read(struct ese_info *ese, uint8_t *buf, size_t count,
                   int timeout)
{
    int ret;
    TMS_DEBUG("Start+\n");

    if (timeout > ESE_CMD_RSP_TIMEOUT_MS) {
        timeout = ESE_CMD_RSP_TIMEOUT_MS;
    }

    if (!gpio_get_value(ese->hw_res.irq_gpio)) {
        while (1) {
            ret = 0;
            ese_enable_irq(ese);

            if (!gpio_get_value(ese->hw_res.irq_gpio)) {
                if (timeout) {
                    ret = wait_event_interruptible_timeout(ese->read_wq, !ese->irq_enable,
                                                           msecs_to_jiffies(timeout));

                    if (ret <= 0) {
                        TMS_ERR("Wakeup of read work queue timeout\n");
                        return -ETIMEDOUT;
                    }
                } else {
                    ret = wait_event_interruptible(ese->read_wq, !ese->irq_enable);

                    if (ret) {
                        TMS_ERR("Wakeup of read work queue failed\n");
                        return ret;
                    }
                }
            }

            ese_disable_irq(ese);

            if (gpio_get_value(ese->hw_res.irq_gpio)) {
                break;
            }

            if (!gpio_get_value(ese->hw_res.irq_gpio)) {
                TMS_ERR("Can not detect interrupt\n");
                return -EIO;
            }

            if (ese->release_read) {
                TMS_ERR("Releasing read\n");
                return -EWOULDBLOCK;
            }

            TMS_INFO("Spurious interrupt detected\n");
        }
    }

    /* Read data */
    ret = spi_read(ese->client, buf, count);
    TMS_DEBUG("Normal end-\n");
    return ret;
}

static int ese_ioctl_set_state(struct ese_info *ese, unsigned long arg)
{
    int ret = SUCCESS;
    TMS_DEBUG("arg = %lu\n", arg);

    switch (arg) {
    case ESE_POWER_ON:
        ese_power_control(ese, ON);
        break;

    case ESE_POWER_OFF:
        ese_power_control(ese, OFF);
        break;
    default:
        TMS_ERR("Bad control arg %lu\n", arg);
        ret = -ENOIOCTLCMD;
        break;
    }

    return ret;
}

/*********** PART2: Operation Function Area ***********/
static long ese_device_ioctl(struct file *file, unsigned int cmd,
                             unsigned long arg)
{
    int ret = 0;
    struct ese_info *ese;
    TMS_DEBUG("cmd = %x arg = %zx\n", cmd, arg);
    ese = file->private_data;

    if (!ese) {
        TMS_ERR("eSE device no longer exists\n");
        return -ENODEV;
    }

    switch (cmd) {
    case ESE_SET_STATE:
        ret = ese_ioctl_set_state(ese, arg);
        break;
#ifdef CLOCK_SPI
    case ESE_ENBLE_SPI_CLK:
        nfc_spi_clk_enable(ese, 1);
        break;
    case ESE_DISABLE_SPI_CLK:
        nfc_spi_clk_enable(ese, 0);
        break;
#endif
    default:
        TMS_ERR("Unknow control cmd[%x]\n", cmd);
        ret = -ENOIOCTLCMD;
    };

    return ret;
}

static ssize_t ese_device_write(struct file *file, const char *buf,
                                size_t count, loff_t *offset)
{
    int ret = -EIO;
    uint8_t *write_buf;
    struct ese_info *ese;
    ese = file->private_data;

    if (!ese) {
        TMS_ERR("eSE device no longer exists\n");
        return -ENODEV;
    }

    if (count > 0 && count < ESE_MAX_BUFFER_SIZE) {
    } else if (count > ESE_MAX_BUFFER_SIZE) {
        TMS_WARN("The write bytes[%zu] exceeded the buffer max size, count = %d\n",
                 count, ESE_MAX_BUFFER_SIZE);
        count = ESE_MAX_BUFFER_SIZE;
    } else {
        TMS_ERR("Write error,count = %zu\n", count);
        return -EPERM;
    }

    /* malloc write buffer */
    write_buf = devm_kzalloc(ese->spi_dev, count, GFP_DMA | GFP_KERNEL);

    if (!write_buf) {
        return -ENOMEM;
    }

    memset(write_buf, 0x00, count);
    mutex_lock(&ese->write_mutex);

    if (copy_from_user(write_buf, buf, count)) {
        TMS_ERR("Copy from user space failed\n");
        ret = -EFAULT;
        goto err_release_write;
    }

    /* Write data */
    ret = spi_write(ese->client, write_buf, count);

    if (ret == 0) {
        ret = count;
    } else {
        TMS_ERR("SPI writer error = %d\n", ret);
        ret = -EIO;
        goto err_release_write;
    }

    tms_buffer_dump("Tx ->", write_buf, count);
err_release_write:
    mutex_unlock(&ese->write_mutex);
    devm_kfree(ese->spi_dev, write_buf);
    return ret;
}

static ssize_t ese_device_read(struct file *file, char *buf, size_t count,
                               loff_t *offset)
{
    int ret = -EIO;
    uint8_t *read_buf;
    struct ese_info *ese;
    ese = file->private_data;

    if (!ese) {
        TMS_ERR("eSE device no longer exists\n");
        return -ENODEV;
    }

    if (count > 0 && count < ESE_MAX_BUFFER_SIZE) {
    } else if (count > ESE_MAX_BUFFER_SIZE) {
        TMS_WARN("The read bytes[%zu] exceeded the buffer max size, count = %d\n",
                 count, ESE_MAX_BUFFER_SIZE);
        count = ESE_MAX_BUFFER_SIZE;
    } else {
        TMS_ERR("read error,count = %zu\n", count);
        return -EPERM;
    }

    /* malloc read buffer */
    read_buf = devm_kzalloc(ese->spi_dev, count, GFP_DMA | GFP_KERNEL);

    if (!read_buf) {
        return -ENOMEM;
    }

    memset(read_buf, 0x00, count);
    mutex_lock(&ese->read_mutex);

    if (file->f_flags & O_NONBLOCK) {
        /* Noblock read data mode */
        ret = spi_read(ese->client, read_buf, count);
    } else {
        /* Block read data mode */
        ret = spi_block_read(ese, read_buf, count, 0);
    }

    if (ret == 0) {
        ret = count;
    } else {
        TMS_ERR("SPI read failed ret = %d\n", ret);
        ret = -EFAULT;
        goto err_release_read;
    }

    if (copy_to_user(buf, read_buf, count)) {
        TMS_ERR("Copy to user space failed\n");
        ret = -EFAULT;
        goto err_release_read;
    }

    tms_buffer_dump("Rx <-", read_buf, count);
err_release_read:
    mutex_unlock(&ese->read_mutex);
    devm_kfree(ese->spi_dev, read_buf);
    return ret;
}

int ese_device_flush(struct file *file, fl_owner_t id)
{
    struct ese_info *ese;
    ese = file->private_data;

    if (!ese) {
        TMS_ERR("eSE device no longer exists\n");
        return -ENODEV;
    }

    if (!ese->independent_support) {
        return SUCCESS;
    }

    if (!mutex_trylock(&ese->read_mutex)) {
        ese->release_read = true;
        ese_disable_irq(ese);
        wake_up(&ese->read_wq);
        TMS_INFO("Waiting for release of blocked read\n");
        mutex_lock(&ese->read_mutex);
        ese->release_read = false;
    } else {
        TMS_INFO("Read thread already released\n");
    }

    mutex_unlock(&ese->read_mutex);
    return SUCCESS;
}

static int ese_device_close(struct inode *inode, struct file *file)
{
    struct ese_info *ese = NULL;
    TMS_DEBUG("Close eSE device[%d-%d]\n", imajor(inode),
              iminor(inode));
    ese = ese_get_data(inode);

    if (!ese) {
        TMS_ERR("eSE device not exist\n");
        return -ENODEV;
    }

    file->private_data = NULL;
    return SUCCESS;
}

static int ese_device_open(struct inode *inode, struct file *file)
{
    struct ese_info *ese = NULL;
    TMS_WARN("Kernel version : %06x, TMS version : %s\n", LINUX_VERSION_CODE, DRIVER_VERSION);
    TMS_WARN("eSE driver version : %s\n", ESE_VERSION);
    TMS_DEBUG("eSE device number is %d-%d\n", imajor(inode),
              iminor(inode));
    ese = ese_get_data(inode);

    if (!ese) {
        TMS_ERR("eSE device not exist\n");
        return -ENODEV;
    }

    file->private_data = ese;
    return SUCCESS;
}

static const struct file_operations ese_fops = {
    .owner          = THIS_MODULE,
    .open           = ese_device_open,
    .release        = ese_device_close,
    .read           = ese_device_read,
    .write          = ese_device_write,
    .flush          = ese_device_flush,
    .unlocked_ioctl = ese_device_ioctl,
};

/*********** PART3: eSE Driver Start Area ***********/
static int ese_device_probe(struct spi_device *client)
{
    int ret;
    struct ese_info *ese = NULL;
    TMS_INFO("eSE Probe start+\n");

    TMS_DEBUG("chip select = %d , bus number = %d \n",
              client->chip_select, client->master->bus_num);
    /* step1 : alloc ese_info */
    ese = ese_data_alloc(&client->dev, ese);

    if (ese == NULL) {
        TMS_ERR("eSE info alloc memory error\n");
        return -ENOMEM;
    }

    /* step2 : init and binding parameters for easy operate */
    ese->client                = client;
    ese->spi_dev               = &client->dev;
    ese->client->mode          = SPI_MODE_0;
    ese->client->bits_per_word = 8;
    ese->irq_enable            = true;
    ese->release_read          = false;
    ese->dev.fops              = &ese_fops;
    /* step3 : register common ese */
    ret = ese_common_info_init(ese);

    if (ret) {
        TMS_ERR("Init common eSE device failed\n");
        goto err_free_ese_malloc;
    }

    /* step4 : setup spi */
    ret = spi_setup(ese->client);

    if (ret < 0) {
        TMS_ERR("Failed to perform SPI setup\n");
        goto err_free_ese_info;
    }

    /* step5 : init mutex and queues */
    init_waitqueue_head(&ese->read_wq);
    mutex_init(&ese->read_mutex);
    mutex_init(&ese->write_mutex);

    if (ese->independent_support) {
        spin_lock_init(&ese->irq_enable_slock);
    }

    /* step6 : register ese device */
    if (!ese->tms->registe_device) {
        TMS_ERR("ese->tms->registe_device is NULL\n");
        ret = -ERROR;
        goto err_destroy_mutex;
    }
    ret = ese->tms->registe_device(&ese->dev, ese);

    if (ret) {
        TMS_ERR("eSE device register failed\n");
        goto err_destroy_mutex;
    }

    /* step7 : register ese irq */
    ret = ese_irq_register(ese);

    if (ret) {
        TMS_ERR("register irq failed\n");
        goto err_unregiste_device;
    }

    spi_set_drvdata(client, ese);

#ifdef CLOCK_SPI
	pr_info("%s now disable spi clk", __func__);
	nfc_spi_clk_enable(ese, 0);
#endif

    TMS_INFO("Probe successfully\n");
    return SUCCESS;
err_unregiste_device:
    if (ese->tms->unregiste_device) {
        ese->tms->unregiste_device(&ese->dev);
    }
err_destroy_mutex:
    mutex_destroy(&ese->read_mutex);
    mutex_destroy(&ese->write_mutex);
err_free_ese_info:
    ese_gpio_release(ese);
err_free_ese_malloc:
    ese_data_free(&client->dev, ese);
    ese = NULL;
    TMS_ERR("Probe failed, ret = %d\n", ret);
    return ret;
}

static int ese_device_remove(struct spi_device *client)
{
    struct ese_info *ese;
    TMS_INFO("Remove eSE device\n");
    ese = spi_get_drvdata(client);

    if (!ese) {
        TMS_ERR("eSE device no longer exists\n");
        return -ENODEV;
    }

    free_irq(ese->client->irq, ese);
    mutex_destroy(&ese->read_mutex);
    mutex_destroy(&ese->write_mutex);
    ese_gpio_release(ese);
    if (ese->tms->unregiste_device) {
        ese->tms->unregiste_device(&ese->dev);
    }
    ese_data_free(&client->dev, ese);
    ese = NULL;
    spi_set_drvdata(client, NULL);
    return SUCCESS;
}

static const struct spi_device_id ese_device_id[] = {
    {ESE_DEVICE, 0 },
    { }
};

static struct of_device_id ese_match_table[] = {
    { .compatible = ESE_DEVICE, },
    { }
};

static struct spi_driver ese_spi_driver = {
    .probe    = ese_device_probe,
    .remove   = ese_device_remove,
    .id_table = ese_device_id,
    .driver   = {
        .owner          = THIS_MODULE,
        .name           = ESE_DEVICE,
        .of_match_table = ese_match_table,
        .probe_type     = PROBE_PREFER_ASYNCHRONOUS,
    },
};

MODULE_DEVICE_TABLE(of, ese_match_table);

int ese_driver_init(void)
{
    int ret;
    TMS_INFO("Loading eSE driver\n");
    ret = spi_register_driver(&ese_spi_driver);

    if (ret) {
        TMS_ERR("Unable to register spi driver, ret = %d\n", ret);
    }

    return ret;
}

void ese_driver_exit(void)
{
    TMS_INFO("Unloading eSE driver\n");
    spi_unregister_driver(&ese_spi_driver);
}

MODULE_DESCRIPTION("TMS eSE Driver");
MODULE_LICENSE("GPL");
