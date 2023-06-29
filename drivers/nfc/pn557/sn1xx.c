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
 */
/******************************************************************************
 *
 *  The original Work has been changed by NXP Semiconductors.
 *
 *  Copyright (C) 2013-2019 NXP Semiconductors
 *   *
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
 ******************************************************************************/
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

#include "nfc.h"
#include "sn1xx.h"

/*
 * Power management of the eSE
 */
long sn1xx_nfc_ese_ioctl(struct nfc_dev *nfc_dev,  unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    pr_debug("%s: cmd = %u, arg = %lu\n", __func__, cmd, arg);
    switch(cmd) {
        //TODO - add cases for the ESE mechanism
        default:
            pr_err("%s: bad ioctl: cmd = %u, arg = %lu\n", __func__, cmd, arg);
            ret = -ENOIOCTLCMD;
    };
    return ret;
}

/*
 * sn100_nfc_pwr() - power control/firmware download
 * @filp:    pointer to the file descriptor
 * @arg:    mode that we want to move to
 *
 * Device power control. Depending on the arg value, device moves to
 * different states
 * (arg = 1):FW_DL GPIO = 0
 * (arg = 2):FW_DL GPIO = 1 - Power up in firmware download mode
 * (additional ioctl for sn100 because of keeping VEN always high)
 * (arg = 4):enable firmware download via NCI commands
 * (arg = 5):power on/reset NFC and ESE
 * (arg = 6):disable firmware download via NCI commands
 *
 * Return: -ENOIOCTLCMD if arg is not supported, 0 in any other case
 */
static long sn1xx_nfc_pwr(struct nfc_dev *nfc_dev, unsigned long arg)
{
    pr_debug("%s: %lu\n", __func__, arg);
    switch(arg) {
        case 1:
        case 6:
            if (gpio_is_valid(nfc_dev->firm_gpio)) {
                gpio_set_value(nfc_dev->firm_gpio, 0);
                usleep_range(10000, 10100);
            }
            break;
        case 2:
        case 5:
            nfc_ese_acquire(nfc_dev);
            nfc_disable_irq(nfc_dev);
            gpio_set_value(nfc_dev->ven_gpio, 0);
            usleep_range(10000, 10100);
            if (2 == arg) {
                if (gpio_is_valid(nfc_dev->firm_gpio)) {
                    gpio_set_value(nfc_dev->firm_gpio, 1);
                    usleep_range(10000, 10100);
                }
            }
            nfc_enable_irq(nfc_dev);
            gpio_set_value(nfc_dev->ven_gpio, 1);
            usleep_range(10000, 10100);
            nfc_ese_release(nfc_dev);
            break;
        case 4:
            if (gpio_is_valid(nfc_dev->firm_gpio)) {
                gpio_set_value(nfc_dev->firm_gpio, 1);
                usleep_range(10000, 10100);
            }
            break;
        default:
            pr_err("%s bad ioctl %lu\n", __func__, arg);
            return -ENOIOCTLCMD;
    }
    return 0;
}


long  sn1xx_nfc_ioctl(struct nfc_dev *nfc_dev, unsigned int cmd,
        unsigned long arg)
{
    int ret = 0;
    pr_debug("%s: cmd = %u, arg = %lu\n", __func__, cmd, arg);
    switch (cmd) {
    case SN1XX_SET_PWR:
        ret = sn1xx_nfc_pwr(nfc_dev, arg);
        break;
    default:
        pr_err("%s: bad ioctl: cmd = %u, arg = %lu\n", __func__, cmd, arg);
        ret = -ENOIOCTLCMD;
    }
    return ret;
}

int sn1xx_nfc_probe(struct nfc_dev *nfc_dev)
{
    pr_debug("%s: enter\n", __func__);
    /* VBAT--> VDDIO(HIGH) + Guardtime of min 5ms --> VEN(HIGH) */
    nfc_ese_acquire(nfc_dev);
    msleep(5);
    /* VEN toggle(reset) to proceed */
    gpio_set_value(nfc_dev->ven_gpio, 0);
    msleep(5);
    gpio_set_value(nfc_dev->ven_gpio, 1);
    nfc_ese_release(nfc_dev);
    return 0;
}

int sn1xx_nfc_remove(struct nfc_dev *nfc_dev)
{
    /*nothing needed here in addition to generic driver*/
    return 0;
}
