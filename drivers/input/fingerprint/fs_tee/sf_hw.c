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

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/consumer.h>
#include <linux/spi/spi.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>

#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include "sf_ctl.h"

#ifdef CONFIG_MTK_CLKMGR
#include "mach/mt_clkmgr.h"
#endif

#define MODULE_NAME "fortsense-sf_hw"
#define xprintk(level, fmt, args...) printk(level MODULE_NAME": (%d) "fmt, __LINE__, ##args)


#ifndef CONFIG_OF
#error "error: this driver 'MODULE_NAME' only support dts."
#endif

static struct sf_ctl_device *sf_ctl_dev = NULL;
#if (SF_MTK_CPU && !REE_MTK_ANDROID_L)
typedef enum {
#if (SF_POWER_MODE_SEL == PWR_MODE_GPIO)
    SF_PIN_STATE_PWR_ON,
    SF_PIN_STATE_PWR_OFF,
#endif
    SF_PIN_STATE_RST_LOW,
    SF_PIN_STATE_RST_HIGH,
    SF_PIN_STATE_INT_SET,
#if SF_SPI_DTS_CS
    SF_PIN_STATE_CS_SET,
#endif
#if SF_SPI_DTS_Ck
    SF_PIN_STATE_CK_SET,
#endif
#if SF_SPI_DTS_MI
    SF_PIN_STATE_MI_SET,
#endif
#if SF_SPI_DTS_MO
    SF_PIN_STATE_MO_SET,
#endif
#if SF_SPI_DTS_MI_PU
    SF_PIN_STATE_MI_PU,
#elif SF_SPI_DTS_MI_PD
    SF_PIN_STATE_MI_PD,
#endif
#if SF_SPI_DTS_MO_PU
    SF_PIN_STATE_MO_PU,
#elif SF_SPI_DTS_MO_PD
    SF_PIN_STATE_MO_PD,
#endif
    /* Array size */
    SF_PIN_STATE_MAX
} sf_pin_state_t;


static struct pinctrl *sf_pinctrl = NULL;
static struct pinctrl_state *sf_pin_states[SF_PIN_STATE_MAX] = {NULL, };

static const char *sf_pinctrl_state_names[SF_PIN_STATE_MAX] = {
#if (SF_POWER_MODE_SEL == PWR_MODE_GPIO)
    FINGER_POWER_ON, FINGER_POWER_OFF,
#endif
    FINGER_RESET_LOW, FINGER_RESET_HIGH, FINGER_INT_SET,
#if SF_SPI_DTS_CS
    FINGER_CS_SET,
#endif
#if SF_SPI_DTS_Ck
    FINGER_CK_SET,
#endif
#if SF_SPI_DTS_MI
    FINGER_MI_SET,
#endif
#if SF_SPI_DTS_MO
    FINGER_MO_SET,
#endif
#if SF_SPI_DTS_MI_PU
    FINGER_MI_PU,
#elif SF_SPI_DTS_MI_PD
    FINGER_MI_PD,
#endif
#if SF_SPI_DTS_MO_PU
    FINGER_MO_PU,
#elif SF_SPI_DTS_MO_PD
    FINGER_MO_PD,
#endif
};
#endif

static int sf_spi_clock_enable(bool on)
{
    int err = 0;
#if (!SF_REE_PLATFORM) && (SF_MTK_CPU)
#ifdef CONFIG_MTK_CLKMGR

    if (on) {
#ifdef CONFIG_ARCH_MT6580
        /* Switch to 104MHz before transfer */
        clkmux_sel(MT_CLKMUX_SPI_GFMUX_SEL, MT_CG_UPLL_D12, "spi");
#endif
        enable_clock(MT_CG_PERI_SPI0, "spi");
    }
    else {
        disable_clock(MT_CG_PERI_SPI0, "spi");
#ifdef CONFIG_ARCH_MT6580
        /* Switch back to 26MHz after transfer */
        clkmux_sel(MT_CLKMUX_SPI_GFMUX_SEL, MT_CG_SYS_26M, "spi");
#endif
    }

#elif defined(MT6797)
    /* changed after MT6797 platform */
    struct mt_spi_t *ms = NULL;
    static int count;
    ms = spi_master_get_devdata(sf_ctl_dev->pdev->master);

    if (on && (count == 0)) {
        mt_spi_enable_clk(ms);
        count = 1;
    }
    else if ((count > 0) && (on == 0)) {
        mt_spi_disable_clk(ms);
        count = 0;
    }

#else
    static int count;

    if (on && (count == 0)) {
        mt_spi_enable_master_clk(sf_ctl_dev->pdev);
        count = 1;
    }
    else if ((count > 0) && (on == 0)) {
        mt_spi_disable_master_clk(sf_ctl_dev->pdev);
        count = 0;
    }

#endif
#endif
    return err;
}

static int sf_ctl_device_reset(bool output)
{
    int err = 0;
    xprintk(SF_LOG_LEVEL, "%s(..) enter.\n", __FUNCTION__);
#if SF_MTK_CPU
#if REE_MTK_ANDROID_L

    if (output) {
        mt_set_gpio_out(GPIO_SW_RST_PIN, 1);
    }
    else {
        mt_set_gpio_out(GPIO_SW_RST_PIN, 0);
    }

#else

    if (sf_pinctrl == NULL) {
        xprintk(KERN_ERR, "sf_pinctrl is NULL.\n");
        return -1;
    }

    if (output) {
        err = pinctrl_select_state(sf_pinctrl, sf_pin_states[SF_PIN_STATE_RST_HIGH]);
    }
    else {
        err = pinctrl_select_state(sf_pinctrl, sf_pin_states[SF_PIN_STATE_RST_LOW]);
    }

#endif
#else

    if (sf_ctl_dev->rst_num == 0) {
        xprintk(KERN_ERR, "sf_ctl_dev->rst_num is not get.\n");
        return -1;
    }

    if (output) {
        gpio_direction_output(sf_ctl_dev->rst_num, 1);
    }
    else {
        gpio_direction_output(sf_ctl_dev->rst_num, 0);
    }

#endif
    xprintk(SF_LOG_LEVEL, "%s(..) leave.\n", __FUNCTION__);
    return err;
}

#if (SF_POWER_MODE_SEL == PWR_MODE_GPIO)
static int sf_ctl_device_power_by_gpio(bool on)
{
    static bool isPowerOn = false;
    int err = 0;
    xprintk(SF_LOG_LEVEL, "%s(..) enter. on_off: %d, stat: %d\n", __FUNCTION__, on, isPowerOn);
#if SF_MTK_CPU

    if (on && !isPowerOn) {
        err = pinctrl_select_state(sf_pinctrl, sf_pin_states[SF_PIN_STATE_PWR_ON]);
        isPowerOn = true;
        msleep(10);
    }
    else if (!on && isPowerOn) {
        err = pinctrl_select_state(sf_pinctrl, sf_pin_states[SF_PIN_STATE_PWR_OFF]);
        isPowerOn = false;
    }

#else

    if (on && !isPowerOn) {
        err = gpio_direction_output(sf_ctl_dev->pwr_num, 1);
        isPowerOn = true;
        msleep(10);
    }
    else if (!on && isPowerOn) {
        err = gpio_direction_output(sf_ctl_dev->pwr_num, 0);
        isPowerOn = false;
    }

#endif
    xprintk(SF_LOG_LEVEL, "%s(..) leave.\n", __FUNCTION__);
    return err;
}
#endif

#if (SF_POWER_MODE_SEL == PWR_MODE_REGULATOR)
static int sf_ctl_device_power_by_regulator(bool on)
{
    static bool isPowerOn = false;
    int err = 0;
    xprintk(SF_LOG_LEVEL, "%s(..) enter. on_off: %d, stat: %d\n", __FUNCTION__, on, isPowerOn);

    if (sf_ctl_dev->vdd_reg == NULL) {
        xprintk(KERN_ERR, "ctl_dev->vdd_reg is NULL.\n");
        return (-ENODEV);
    }

    if (on && !isPowerOn) {
        err = regulator_set_load(sf_ctl_dev->vdd_reg, 100000);

        if (err < 0) {
            xprintk(KERN_ERR, "Regulator set_load failed err = %d\n", err);
        }
 
        err = regulator_enable(sf_ctl_dev->vdd_reg);

        if (err) {
            xprintk(KERN_ERR, "Regulator vdd enable failed err = %d\n", err);
            return err;
        }

        isPowerOn = true;
        msleep(10);
    }
    else if (!on && isPowerOn) {
        err = regulator_disable(sf_ctl_dev->vdd_reg);

        if (err) {
            xprintk(KERN_ERR, "Regulator vdd disable failed err = %d\n", err);
            return err;
        }

        isPowerOn = false;
    }

    xprintk(SF_LOG_LEVEL, "%s(..) leave.\n", __FUNCTION__);
    return err;
}
#endif

static int sf_ctl_device_power(bool on)
{
    int err = 0;
#if (SF_POWER_MODE_SEL == PWR_MODE_GPIO)
    err = sf_ctl_device_power_by_gpio(on);
#elif (SF_POWER_MODE_SEL == PWR_MODE_REGULATOR)
    err = sf_ctl_device_power_by_regulator(on);
#endif
    return err;
}

#if REE_MTK_ANDROID_L

static int sf_ctl_device_free_gpio(struct sf_ctl_device *ctl_dev)
{
    int err = 0;
    xprintk(KERN_ERR, "%s(..) enter, free resource.\n", __FUNCTION__);
    xprintk(SF_LOG_LEVEL, "%s(..) leave.\n", __FUNCTION__);
    return err;
}

static int sf_ctl_device_init_gpio_pins(struct sf_ctl_device *ctl_dev)
{
    int err = 0;
    sf_ctl_dev = ctl_dev;
    xprintk(SF_LOG_LEVEL, "%s(..) enter.\n", __FUNCTION__);
#if MTK_L5_X_POWER_ON
    //power on
    mt_set_gpio_mode(GPIO_SW_PWR_PIN, GPIO_SW_PWR_M_GPIO);
    mt_set_gpio_dir(GPIO_SW_PWR_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_SW_PWR_PIN, 1);
    msleep(1);
#endif
    /*set reset pin to high*/
    mt_set_gpio_mode(GPIO_SW_RST_PIN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_SW_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_SW_RST_PIN, 1);
#if MTK_L5_X_IRQ_SET
    //irq setting
    mt_set_gpio_mode(GPIO_SW_INT_PIN, GPIO_FORTSENSE_IRQ_M_EINT);
    mt_set_gpio_dir(GPIO_SW_INT_PIN, GPIO_DIR_IN);
    mt_set_gpio_pull_enable(GPIO_SW_INT_PIN, GPIO_PULL_DISABLE);
    mt_set_gpio_pull_enable(GPIO_SW_INT_PIN, GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(GPIO_SW_INT_PIN, GPIO_PULL_UP);
#endif
    ctl_dev->irq_num = mt_gpio_to_irq(GPIO_SW_IRQ_NUM);
    xprintk(KERN_INFO, "irq number is %d.\n", ctl_dev->irq_num);
    xprintk(SF_LOG_LEVEL, "%s(..) leave.\n", __FUNCTION__);
    return err;
}
#else

static int sf_ctl_device_free_gpio(struct sf_ctl_device *ctl_dev)
{
    int err = 0;
    xprintk(KERN_ERR, "%s(..) enter, free resource.\n", __FUNCTION__);

    if (ctl_dev->pdev->dev.of_node) {
        ctl_dev->pdev->dev.of_node = NULL;
    }

#if SF_MTK_CPU

    if (sf_pinctrl) {
        pinctrl_put(sf_pinctrl);
        sf_pinctrl = NULL;
    }

#else

    if (gpio_is_valid(ctl_dev->rst_num)) {
        gpio_free(ctl_dev->rst_num);
        ctl_dev->rst_num = 0;
    }

    if (gpio_is_valid(ctl_dev->irq_pin)) {
        gpio_free(ctl_dev->irq_pin);
        ctl_dev->irq_pin = 0;
    }

#endif
#if (SF_POWER_MODE_SEL == PWR_MODE_GPIO)

    if (gpio_is_valid(ctl_dev->pwr_num)) {
        gpio_free(ctl_dev->pwr_num);
        ctl_dev->pwr_num = 0;
    }

#elif (SF_POWER_MODE_SEL == PWR_MODE_REGULATOR)

    if (ctl_dev->vdd_reg) {
        sf_ctl_device_power(false);
        regulator_put(ctl_dev->vdd_reg);
        ctl_dev->vdd_reg = NULL;
    }

#endif
    xprintk(SF_LOG_LEVEL, "%s(..) leave.\n", __FUNCTION__);
    return err;
}

static int sf_ctl_device_init_gpio_pins(struct sf_ctl_device *ctl_dev)
{
    int err = 0;
    sf_ctl_dev = ctl_dev;
    xprintk(SF_LOG_LEVEL, "%s(..) enter.\n", __FUNCTION__);
    ctl_dev->pdev->dev.of_node = of_find_compatible_node(NULL, NULL, COMPATIBLE_SW_FP);

    if (!ctl_dev->pdev->dev.of_node) {
        xprintk(KERN_ERR, "of_find_compatible_node(..) failed.\n");
        return (-ENODEV);
    }

#if SF_MTK_CPU
    sf_pinctrl = pinctrl_get(&ctl_dev->pdev->dev);

    if (!sf_pinctrl) {
        xprintk(KERN_ERR, "pinctrl_get(..) failed.\n");
        return (-ENODEV);
    }

    ctl_dev->irq_num = irq_of_parse_and_map(ctl_dev->pdev->dev.of_node, 0);
    xprintk(KERN_INFO, "irq number is %d.\n", ctl_dev->irq_num);
    {
        int i = 0;

        for (i = 0; i < SF_PIN_STATE_MAX; ++i) {
            sf_pin_states[i] = pinctrl_lookup_state(sf_pinctrl,
                                                    sf_pinctrl_state_names[i]);

            if (!sf_pin_states[i]) {
                xprintk(KERN_ERR, "can't find '%s' pinctrl_state.\n",
                        sf_pinctrl_state_names[i]);
                err = (-ENODEV);
                break;
            }
        }

        if (i < SF_PIN_STATE_MAX) {
            xprintk(KERN_ERR, "%s() failed.\n", __FUNCTION__);
        }

        /* init spi,sunch as cs clck miso mosi mode, gpio pullup pulldown */
        for (i = SF_PIN_STATE_INT_SET + 1; i < SF_PIN_STATE_MAX; ++i) {
            err = pinctrl_select_state(sf_pinctrl, sf_pin_states[i]);

            if (err) {
                xprintk(KERN_ERR, "%s() pinctrl_select_state(%s) failed.\n", __FUNCTION__, sf_pinctrl_state_names[i]);
                break;
            }

            xprintk(KERN_INFO, "pinctrl_select_state(%s) ok.\n", sf_pinctrl_state_names[i]);
        }
    }
#else
    ctl_dev->rst_num = of_get_named_gpio(ctl_dev->pdev->dev.of_node, COMPATIBLE_RESET_GPIO, 0);
    ctl_dev->irq_pin = of_get_named_gpio(ctl_dev->pdev->dev.of_node, COMPATIBLE_IRQ_GPIO, 0);
    xprintk(KERN_INFO, "reset pin number = %d\n", ctl_dev->rst_num);
    xprintk(KERN_INFO, "irq pin number = %d\n", ctl_dev->irq_pin);
#if (SF_POWER_MODE_SEL == PWR_MODE_GPIO)
    ctl_dev->pwr_num = of_get_named_gpio(ctl_dev->pdev->dev.of_node, COMPATIBLE_PWR_GPIO, 0);
    xprintk(KERN_INFO, "pwr_gpio_number = %d\n", ctl_dev->pwr_num);

    if (gpio_is_valid(ctl_dev->pwr_num)) {
        err = gpio_request(ctl_dev->pwr_num, "sf-pwr");

        if (err) {
            xprintk(KERN_ERR, "Could not request pwr gpio.\n");
            return err;
        }
    }
    else {
        xprintk(KERN_ERR, "not valid pwr gpio\n");
        return -EIO;
    }

#endif

    if (gpio_is_valid(ctl_dev->rst_num)) {
        err = gpio_request(ctl_dev->rst_num, "sf-reset");

        if (err) {
            xprintk(KERN_ERR, "Could not request reset gpio.\n");
            return err;
        }
    }
    else {
        xprintk(KERN_ERR, "not valid reset gpio\n");
        return -EIO;
    }

    if (gpio_is_valid(ctl_dev->irq_pin)) {
        ctl_dev->irq_num = gpio_to_irq(ctl_dev->irq_pin);
        xprintk(KERN_INFO, "irq number = %d\n", ctl_dev->irq_num);
        err = gpio_request(ctl_dev->irq_pin, "sf-irq");

        if (err) {
            xprintk(KERN_ERR, "Could not request irq gpio.\n");
            return err;
        }
    }
    else {
        xprintk(KERN_ERR, "not valid irq gpio\n");
        return -EIO;
    }

    gpio_direction_input(ctl_dev->irq_pin);
#endif
#if (SF_POWER_MODE_SEL == PWR_MODE_REGULATOR)
    ctl_dev->vdd_reg = regulator_get(&ctl_dev->pdev->dev, SF_VDD_NAME);

    if (IS_ERR(ctl_dev->vdd_reg)) {
        err = PTR_ERR(ctl_dev->vdd_reg);
        xprintk(KERN_ERR, "Regulator get failed vdd err = %d\n", err);
        return err;
    }

#endif
    sf_ctl_device_power(true);
    xprintk(SF_LOG_LEVEL, "%s(..) ok! exit.\n", __FUNCTION__);
    return err;
}
#endif

int sf_platform_init(struct sf_ctl_device *ctl_dev)
{
    int err = 0;
    xprintk(SF_LOG_LEVEL, "%s(..) enter.\n", __FUNCTION__);

    if (ctl_dev) {
        ctl_dev->gpio_init  = sf_ctl_device_init_gpio_pins;
        ctl_dev->power_on   = sf_ctl_device_power;
        ctl_dev->spi_clk_on = sf_spi_clock_enable;
        ctl_dev->reset      = sf_ctl_device_reset;
        ctl_dev->free_gpio  = sf_ctl_device_free_gpio;
    }
    else {
        xprintk(KERN_ERR, "%s() ctl_dev is NULL.\n", __FUNCTION__);
        err = -1;
    }

    xprintk(SF_LOG_LEVEL, "%s(..) leave.\n", __FUNCTION__);
    return err;
}

void sf_platform_exit(struct sf_ctl_device *ctl_dev)
{
    xprintk(SF_LOG_LEVEL, "%s(..) enter.\n", __FUNCTION__);

    if (ctl_dev) {
        ctl_dev->gpio_init  = NULL;
        ctl_dev->power_on   = NULL;
        ctl_dev->spi_clk_on = NULL;
        ctl_dev->reset      = NULL;
        ctl_dev->free_gpio  = NULL;
    }
    else {
        xprintk(KERN_ERR, "%s() ctl_dev is NULL.\n", __FUNCTION__);
    }

    xprintk(SF_LOG_LEVEL, "%s(..) leave.\n", __FUNCTION__);
}
