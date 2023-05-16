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

static struct timer_data_t {
    struct timer_list gTimer;
    struct nano_i2c_client* i2c_client;
} timer_data;

#define HEARTBEAT  (10)                             /*1000ms的定时器*/

/** ************************************************************************//**
*  @func Nanosic_timer_handler
*  
*  @brief Handler for timer expired
*
** */
static
void Nanosic_timer_handler(struct timer_list *t)
{
    struct timer_data_t *timer_data_p = from_timer(timer_data_p, t, gTimer);
    struct nano_i2c_client* i2c_client =timer_data_p->i2c_client;
    //dbgprint(ERROR_LEVEL,"liuyx: Nanosic_timer_handler\n");
    timer_data.gTimer.expires = jiffies + HEARTBEAT * HZ/1000;   /* HZ/1000 表示 ms*/
    mod_timer(&timer_data.gTimer, timer_data.gTimer.expires);               /* jiffies 表示 当前微秒级时间*/

    if(IS_ERR_OR_NULL(i2c_client)){
        dbgprint(ERROR_LEVEL,"i2c_client is NULL\n");
        return;
    }
    /*通过定时读取i2c数据来测试*/
    Nanosic_workQueue_schedule(i2c_client->worker);
}

 /** ************************************************************************//**
 *  @func Nanosic_timer_register
 *  
 *  @brief create a timer
 *
 ** */
void Nanosic_timer_register(struct nano_i2c_client* i2c_client)
{
    timer_data.i2c_client = i2c_client;
    dbgprint(ERROR_LEVEL,"liuyx: Nanosic_timer_register\n");
    timer_setup(&timer_data.gTimer, Nanosic_timer_handler, 0);
    timer_data.gTimer.expires = jiffies + HEARTBEAT * HZ/1000;
    add_timer(&timer_data.gTimer);
}

/** ************************************************************************//**
 *  @func Nanosic_timer_exit
 *  
 *  @brief  destroy the timer
 *
 ** */
void Nanosic_timer_release(void)
{
    dbgprint(ERROR_LEVEL,"liuyx: Nanosic_timer_release\n");
    if(timer_pending(&timer_data.gTimer))
    {
        del_timer(&timer_data.gTimer);
    }
}
 
