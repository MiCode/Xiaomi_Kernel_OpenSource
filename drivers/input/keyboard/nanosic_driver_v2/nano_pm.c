/** ***************************************************************************
 * @file nano_timer.c
 *
 * @brief nanosic timer file
 *
 * <em>Copyright (C) 2010, Nanosic, Inc.  All rights reserved.</em>
 * Author : Bin.yuan bin.yuan@nanosic.com
 * */

/*
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include "nano_macro.h"

#define PM_TIMER_TIMEOUT 20000

static int gpio_irq_pin = -1;
static struct timer_list pm_timer;

/** ************************************************************************//**
*  @func Nanosic_PM_Sleep
*
*  @brief use to control 803 go to sleep
** */
void
Nanosic_PM_sleep(void)
{
    /*going to sleep*/
    Nanosic_GPIO_sleep(false);
}

/** ************************************************************************//**
*  @func Nanosic_PM_try_wakeup
*
*  @brief try to control 803 wakeup from lower power mode
** */
void
Nanosic_PM_try_wakeup(void)
{
    int level = 1;
    int retry = 3;

    if(!g_panel_status) {
        //dbgprint(DEBUG_LEVEL,"8030 is not allowed to wake up under screen off\n");
        return;
    }

    /*get gpio_irq_pin's level , high level present 803 in running state*/
    if(gpio_is_valid(gpio_irq_pin))
        level = gpio_get_value(gpio_irq_pin);

    mod_timer(&pm_timer, jiffies + msecs_to_jiffies(PM_TIMER_TIMEOUT));
    Nanosic_GPIO_sleep(true);
    if(level <= 0){
        dbgprint(ERROR_LEVEL,"waiting for wakeup...\n");
        mdelay(25);
        while(retry--) {
            /*try three times*/
            if(gpio_get_value(gpio_irq_pin)) {
                break;
            }
            /*reset wn8030*/
            if(retry == 0) {
                Nanosic_GPIO_reset();
            }
            /*irq low level duration is 1ms*/
            mdelay(1);
        }
    }
}

/** **************************************************************************
* @func  Nanosic_PM_expire
*
* @brief Handler for timer expire
*
** */
void Nanosic_PM_expire(struct timer_list* timer)
{
    dbgprint(INFO_LEVEL,"going to sleep\n");
    /*go to sleep*/
    Nanosic_PM_sleep();
}

/** ************************************************************************//**
*  @func Nanosic_timer_register
*
*  @brief create a timer
*
** */
void Nanosic_PM_init(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
     setup_timer(&pm_timer, Nanosic_PM_expire, 0);
#else
     timer_setup(&pm_timer, Nanosic_PM_expire, 0);
#endif
    gpio_irq_pin = Nanosic_GPIO_irqget();
    Nanosic_PM_try_wakeup();

    dbgprint(ALERT_LEVEL,"PM module initial\n");
}

/** ************************************************************************//**
 *  @func Nanosic_timer_exit
 *
 *  @brief  destroy the timer
 *
 ** */
void Nanosic_PM_free(void)
{
    if(timer_pending(&pm_timer))
        del_timer(&pm_timer);

    dbgprint(ALERT_LEVEL,"PM module release\n");
}

