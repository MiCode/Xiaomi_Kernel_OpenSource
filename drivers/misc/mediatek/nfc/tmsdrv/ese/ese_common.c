/*****************************************************************************************
 * Copyright (c)  2021- 2022  TsingTeng MicroSystem Corp.Ltd.
 * TMS_EDIT
 * File       : ese_common.c
 * Description: Source file for tms ese common
 * Version    : 1.0
 * Date       : 2022/4/11
 * Team       : NFC Middleware
 * Author     : Guoliang Wu
 * --------------------------------- Revision History: ---------------------------------
 *   <version>    <date>          < author >                            <desc>
 *******************************************************************************************/
#include "ese_common.h"

/*********** PART0: Global Variables Area ***********/

/*********** PART1: Function Area ***********/
struct ese_info *ese_data_alloc(struct device *dev, struct ese_info *ese)
{
    ese = devm_kzalloc(dev, sizeof(struct ese_info), GFP_KERNEL);
    return ese;
}

void ese_data_free(struct device *dev, struct ese_info *ese)
{
    if (ese) {
        devm_kfree(dev, ese);
    }

}

struct ese_info *ese_get_data(struct inode *inode)
{
    struct ese_info *ese;
    struct dev_register *char_dev;
    char_dev = container_of(inode->i_cdev, struct dev_register, chrdev);
    ese = container_of(char_dev, struct ese_info, dev);
    return ese;
}

void ese_gpio_release(struct ese_info *ese)
{
    if (ese->independent_support) {
        gpio_free(ese->hw_res.irq_gpio);
        gpio_free(ese->hw_res.rst_gpio);
    }
}
/*
static void ese_soft_reset(struct hw_resource hw_res, uint32_t mdelay)
{
    if (!ese->tms->set_reset) {
        TMS_ERR("ese->tms->set_reset is NULL");
        return;
    }
    ese->tms->set_reset(hw_res, OFF);
    ese->tms->set_reset(hw_res, ON);

    if (mdelay) {
        usleep_range(mdelay, (mdelay) + 100);
    }
}
*/
void ese_enable_irq(struct ese_info *ese)
{
    unsigned long flag;

    if (!ese->independent_support) {
        return;
    }

    spin_lock_irqsave(&ese->irq_enable_slock, flag);

    if (!ese->irq_enable) {
        enable_irq(ese->client->irq);
        ese->irq_enable = true;
    }

    spin_unlock_irqrestore(&ese->irq_enable_slock, flag);
}

void ese_disable_irq(struct ese_info *ese)
{
    unsigned long flag;

    if (!ese->independent_support) {
        return;
    }

    spin_lock_irqsave(&ese->irq_enable_slock, flag);

    if (ese->irq_enable) {
        disable_irq_nosync(ese->client->irq);
        ese->irq_enable = false;
    }

    spin_unlock_irqrestore(&ese->irq_enable_slock, flag);
}

static irqreturn_t ese_irq_handler(int irq, void *dev_id)
{
    struct ese_info *ese;
    ese = dev_id;
    ese_disable_irq(ese);
    wake_up(&ese->read_wq);
    return IRQ_HANDLED;
}

int ese_irq_register(struct ese_info *ese)
{
    int ret;
    TMS_INFO("Start+\n");

    if (!ese->independent_support) {
        TMS_WARN("Not independent ese chip, Normal end-\n");
        return SUCCESS;
    }

    ese->client->irq = gpio_to_irq(ese->hw_res.irq_gpio);

    if (ese->client->irq < 0) {
        TMS_ERR("Get soft irq number failed");
        return -ERROR;
    }

    ret = devm_request_irq(ese->spi_dev, ese->client->irq, ese_irq_handler,
                           (IRQF_TRIGGER_RISING | IRQF_ONESHOT), ese->dev.name, ese);

    if (ret) {
        TMS_ERR("Register irq failed, ret = %d\n", ret);
        return -ERROR;
    }

    TMS_DEBUG("Register eSE IRQ[%d]\n", ese->client->irq);
    TMS_INFO("Normal end-\n");
    return ret;
}

void ese_power_control(struct ese_info *ese, bool state)
{
    if (!ese->tms->set_ven) {
        TMS_ERR("ese->tms->set_ven is NULL");
        return;
    }

    if (state == ON) {
        ese->tms->set_ven(ese->tms->hw_res, ON);
        TMS_INFO("eSE setting ven high\n");
    } else if (state == OFF) {
        ese->tms->set_ven(ese->tms->hw_res, OFF);
        TMS_INFO("eSE setting ven low\n");
    }
}

static int ese_gpio_configure_init(struct ese_info *ese)
{
    int ret;
    TMS_INFO("Start+\n");

    if (!ese->independent_support) {
        TMS_INFO("Normal end-\n");
        return SUCCESS;
    }

    if (gpio_is_valid(ese->hw_res.irq_gpio)) {
        ret = gpio_direction_input(ese->hw_res.irq_gpio);

        if (ret < 0) {
            TMS_ERR("Unable to to set irq_gpio as input\n");
            return ret;
        }

        TMS_DEBUG("Set irq gpio as input\n");
    }

    if (gpio_is_valid(ese->hw_res.rst_gpio)) {
        ret = gpio_direction_output(ese->hw_res.rst_gpio, ese->hw_res.rst_flag);

        if (ret < 0) {
            TMS_ERR("Unable to set rst_gpio as output\n");
            return ret;
        }

        TMS_DEBUG("Set reset gpio as output\n");
    }

    TMS_INFO("Normal end-\n");
    return SUCCESS;
}

static int ese_parse_dts_init(struct ese_info *ese)
{
    int ret, rcv;
    struct device_node *np;
    TMS_INFO("Start+\n");
    np = ese->spi_dev->of_node;
    ese->independent_support = of_property_read_bool(np, "independent_support");
    rcv = of_property_read_string(np, "tms,device-name", &ese->dev.name);

    if (rcv < 0) {
        ese->dev.name = "tms_ese";
        TMS_WARN("Device name not specified in dts\n");
    }

    rcv = of_property_read_u32(np, "tms,device-count", &ese->dev.count);

    if (rcv < 0) {
        ese->dev.count = 1;
        TMS_WARN("Number of devices not specified in dts\n");
    }

    if (ese->independent_support) {
        ese->hw_res.irq_gpio = of_get_named_gpio(np, "tms,irq-gpio", 0);

        if (gpio_is_valid(ese->hw_res.irq_gpio)) {
            rcv = gpio_request(ese->hw_res.irq_gpio, "ese_int");

            if (rcv) {
                TMS_WARN("Unable to request gpio[%d] as IRQ\n", ese->hw_res.irq_gpio);
            }
        } else {
            TMS_ERR("Irq gpio not specified in dts\n");
            return -EINVAL;
        }

        ese->hw_res.rst_gpio = of_get_named_gpio_flags(np, "tms,reset-gpio", 0,
                               &ese->hw_res.rst_flag);

        if (gpio_is_valid(ese->hw_res.rst_gpio)) {
            rcv = gpio_request(ese->hw_res.rst_gpio, "ese_rst");

            if (rcv) {
                TMS_WARN("Unable to request gpio[%d] as RST\n",
                         ese->hw_res.rst_gpio);
            }
        } else {
            TMS_ERR("Reset gpio not specified in dts\n");
            ret =  -EINVAL;
            goto err_free_irq;
        }

        TMS_DEBUG("irq_gpio = %d, rst_gpio = %d\n", ese->hw_res.irq_gpio,
                  ese->hw_res.rst_gpio);
    }

    TMS_DEBUG("ese device name is %s, count = %d\n", ese->dev.name,
              ese->dev.count);
    TMS_INFO("Normal end-\n");
    return SUCCESS;
err_free_irq:

    if (ese->independent_support) {
        gpio_free(ese->hw_res.irq_gpio);
    }

    TMS_ERR("Error end, ret = %d\n", ret);
    return ret;
}

int ese_common_info_init(struct ese_info *ese)
{
    int ret;
    TMS_INFO("Start+\n");
    /* step1 : binding tms common data */
    ese->tms = tms_common_data_binding();

    if (ese->tms == NULL) {
        TMS_ERR("Get tms common info  error\n");
        return -ENOMEM;
    }

    /* step2 : dts parse */
    ret = ese_parse_dts_init(ese);

    if (ret) {
        TMS_ERR("Parse dts failed.\n");
        return ret;
    }

    /* step3 : set gpio work mode */
    ret = ese_gpio_configure_init(ese);

    if (ret) {
        TMS_ERR("Init gpio control failed.\n");
        goto err_free_gpio;
    }

    TMS_INFO("Normal end-\n");
    return ret;
err_free_gpio:
    ese_gpio_release(ese);
    TMS_DEBUG("Error end, ret = %d\n", ret);
    return ret;
}
