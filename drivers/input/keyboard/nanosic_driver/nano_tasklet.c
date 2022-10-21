/** ***************************************************************************
 * @file nano_tasklet.c
 *
 * @brief tasklet demo  
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
#include <linux/interrupt.h>
#include "nano_macro.h"

static handler_tasklet gTaskletFunc = NULL;
atomic_t gTasklet_schedule_cnt;

/** **************************************************************************
 * @func  Nano_tasklet_handler
 *
 * @brief Entry for tasklet schedule
 *        软中断上下文 , 不能睡眠
 ** */
void Nanosic_tasklet_handler()
{
#if 0
    if(in_irq())
        dbgprint(INFO_LEVEL,"in irq\n");
    else if(in_softirq())
        dbgprint(INFO_LEVEL,"in softirq\n");
    else if(in_interrupt())
        dbgprint(INFO_LEVEL,"in interrupt\n");
    else
        dbgprint(INFO_LEVEL,"other context\n");
#endif

    if(gTaskletFunc)
    {
        dbgprint(INFO_LEVEL,"[softirq] run tasklet handler\n");
        atomic_inc(&gTasklet_schedule_cnt);
        gTaskletFunc();
    }
}

/* disable our tasklet default*/
//static DECLARE_TASKLET_DISABLED(Nano_tasklet, Nano_tasklet_handler, NULL);

/* enable our tasklet default*/
static DECLARE_TASKLET(Nanosic_tasklet, Nanosic_tasklet_handler, 0);

/** **************************************************************************
 * @func  Nano_tasklet_schedule
 *
 * @brief used to tigger nanosic tasklet handler
 ** */
void Nanosic_tasklet_schedule(void)
{
    dbgprint(DEBUG_LEVEL,"Nano_tasklet_schedule\n");
    tasklet_schedule(&Nanosic_tasklet);
}
//EXPORT_SYMBOL_GPL(Nano_tasklet_schedule);

/** **************************************************************************
 * @func  Nano_tasklet_create
 *
 * @brief Handler for create tasklet
 ** */
void Nanosic_tasklet_register(handler_tasklet func)
{
    atomic_set(&gTasklet_schedule_cnt,0);
    gTaskletFunc = func;
}

/** **************************************************************************
 * @func  Nano_tasklet_release
 *
 * @brief Handler for release tasklet
 ** */
void Nanosic_tasklet_release(void)
{
    tasklet_disable(&Nanosic_tasklet);
    tasklet_kill(&Nanosic_tasklet);
}
//EXPORT_SYMBOL_GPL(Nano_tasklet_release);

