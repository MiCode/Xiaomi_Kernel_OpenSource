/** ***************************************************************************
 * @file nano_workqueue.c
 *
 * @brief workqueue demo  
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

/*重要信息只打印一次*/
static bool print_once = true;

/** **************************************************************************
 * @func  Nanosic_workQueue_delay_handler
 *
 * @brief Entry for delay workqueue schedule
 *        内核线程(进程)上下文 , 允许睡眠
 ** */
static void 
Nanosic_workQueue_delay_handler(struct work_struct* work)
{
	struct nano_worker_client *work_client = container_of(work, struct nano_worker_client, worker_delay.work);
    struct nano_i2c_client* i2c_client = NULL;
    
    dbgprint(ALERT_LEVEL,"running delay work %p\n",work_client);

    if(IS_ERR_OR_NULL(work_client)){
        dbgprint(DEBUG_LEVEL,"work_client is NULL\n");
        return;
    }

    i2c_client = work_client->worker_data;
    if(IS_ERR_OR_NULL(i2c_client)){
        dbgprint(DEBUG_LEVEL,"i2c_client is NULL\n");
        return;
    }

    if(IS_ERR_OR_NULL(i2c_client->func))
        return;
    
    while(atomic_read(&work_client->schedule_delay_count)){
        i2c_client->func(i2c_client);
        atomic_dec(&work_client->schedule_delay_count);
    }
}

/** **************************************************************************
 * @func  Nano_workQueue_handler
 *
 * @brief Entry for workqueue schedule
 *        内核线程(进程)上下文 , 允许睡眠
 ** */
static void 
Nanosic_workQueue_handler(struct work_struct* work)
{
    struct nano_worker_client* work_client = container_of(work, struct nano_worker_client, worker);
    struct nano_i2c_client* i2c_client = NULL;

    if(IS_ERR_OR_NULL(work_client)){
        dbgprint(DEBUG_LEVEL,"work_client is NULL\n");
        return;
    }

    i2c_client = work_client->worker_data;
    if(IS_ERR_OR_NULL(i2c_client)){
        dbgprint(DEBUG_LEVEL,"i2c_client is NULL\n");
        return;
    }

    if(print_once)
    {
        dbgprint(ALERT_LEVEL,"got i2c %p\n",i2c_client);
        dbgprint(ALERT_LEVEL,"got work %p\n",work_client);
        print_once = false;
    }

    if(IS_ERR_OR_NULL(i2c_client->func))
        return;

    i2c_client->func(i2c_client);

}

#if 0
/** **************************************************************************
 * @func  Nanosic_workQueue_schedule_external
 *
 * @brief used to tigger nanosic workQueue handler  支持软中断/中断上下文调用
 ** */
void 
Nanosic_workQueue_schedule_external(void)
{
    queue_work(worker_client.worker_queue, &worker_client.worker);
}
EXPORT_SYMBOL_GPL(Nanosic_workQueue_schedule_external);
#endif

/** **************************************************************************
 * @func  Nano_workQueue_schedule
 *
 * @brief used to tigger nanosic workQueue handler
 ** */
void 
Nanosic_workQueue_schedule(struct nano_worker_client* worker_client)
{
    bool delay_work_running =false;
    
    if(IS_ERR_OR_NULL(worker_client)){
        dbgprint(ERROR_LEVEL,"worker_client is NULL\n");
        return;
    }
    atomic_inc(&worker_client->schedule_count);                 /*记录总调度次数*/
    
    if (!work_pending(&worker_client->worker))                  /*当前workqueue是不是在running*/
        queue_work(worker_client->worker_queue, &worker_client->worker);
    else{
        atomic_inc(&worker_client->schedule_delay_count);       /*延时10ms后再调度*/
        delay_work_running = schedule_delayed_work(&worker_client->worker_delay,msecs_to_jiffies(22));
    }
}
EXPORT_SYMBOL_GPL(Nanosic_workQueue_schedule);

/** **************************************************************************
 * @func  Nanosic_workQueue_register
 *
 * @brief Handler for create a workqueue
 ** */
struct nano_worker_client* 
Nanosic_workQueue_register(struct nano_i2c_client* i2c_client)
{
    struct nano_worker_client* worker_client = kzalloc(sizeof(struct nano_worker_client),GFP_KERNEL);
    if(IS_ERR_OR_NULL(worker_client))
        return NULL;
    
    worker_client->worker_queue = create_singlethread_workqueue("nanosic workqueue");
    if(IS_ERR_OR_NULL(worker_client->worker_queue)){
        goto _err1;
    }

    /*initial delay workqueue*/
	INIT_DELAYED_WORK(&worker_client->worker_delay, Nanosic_workQueue_delay_handler);
    /*initial workqueue*/
    INIT_WORK(&worker_client->worker, Nanosic_workQueue_handler);
    atomic_set(&worker_client->schedule_count,0);
    atomic_set(&worker_client->schedule_delay_count,0);
    worker_client->worker_data = i2c_client;

    return worker_client;

 _err1:
    dbgprint(ERROR_LEVEL,"Register workqueue err\n");
    kfree(worker_client);
    return NULL;
}

/** **************************************************************************
 * @func  Nano_workQueue_release
 *
 * @brief Handler for release workqueue
 ** */
void 
Nanosic_workQueue_release(struct nano_worker_client* worker_client)
{
    if(IS_ERR_OR_NULL(worker_client)){
        dbgprint(ERROR_LEVEL,"worker_client is NULL\n");
        return;
    }

	cancel_delayed_work_sync(&worker_client->worker_delay);
    
    if(!IS_ERR_OR_NULL(worker_client->worker_queue))
        destroy_workqueue(worker_client->worker_queue);

    kfree(worker_client);
}

