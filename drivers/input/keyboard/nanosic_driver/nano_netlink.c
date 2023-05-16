/** ***************************************************************************
 * @file nano_netlink.c
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/stat.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/kthread.h>
#include <net/netlink.h>
#include <net/sock.h>

#include <linux/timer.h>
#include <linux/jiffies.h>
#include "nano_macro.h"

//#define NETLINK_HEARTBEAT  (30000)
#define NETLINK_USER_PID   (8888)
#define NETLINK_NANOSIC_ID (26)

static struct sock *nl_sk = NULL;
static struct task_struct *task;
static atomic_t running;
static wait_queue_head_t waitq;
static atomic_t data_ready;
static spinlock_t gspinlock;
static int initial = 0;

/** ************************************************************************//**
 *  @brief
 *  
 *  invoke this interface to wakeup waitqueue
 ** */
void Nanosic_nl_tigger(void)
{
    if(initial){
        atomic_inc(&data_ready);
        wake_up_interruptible(&waitq);
    }
}
EXPORT_SYMBOL_GPL(Nanosic_nl_tigger);

/** ************************************************************************//**
 *  @brief
 *  
 *  netlink单播
 ** */
static 
void Nanosic_nl_send(uint8_t *message, int len)
{
    struct sk_buff *skb_1;
    struct nlmsghdr *nlh;

    if(!message || !nl_sk)
        return;
    
    skb_1 = alloc_skb(NLMSG_SPACE(len), GFP_KERNEL);
    if( !skb_1 ){
        dbgprint(KERN_ERR,"alloc_skb error!\n");
        return;
    }

    nlh = nlmsg_put(skb_1, 0, 0, 0, len, 0);
    NETLINK_CB(skb_1).portid = 0;
    NETLINK_CB(skb_1).dst_group = 0;
    memcpy(NLMSG_DATA(nlh), message, len);
    netlink_unicast(nl_sk, skb_1, NETLINK_USER_PID, MSG_DONTWAIT);
}

/** ************************************************************************//**
 *  @brief
 *  
 *  unused
 ** */
static 
void Nanosic_nl_recv(struct sk_buff *__skb)
{
    struct sk_buff *skb;
    char str[100];
    struct nlmsghdr *nlh;

    if( !__skb ) {
        return;
    }

    skb = skb_get(__skb);
    if( skb->len < NLMSG_SPACE(0)) {
        return;
    }

    nlh = nlmsg_hdr(skb);
    memset(str, 0, sizeof(str));
    memcpy(str, NLMSG_DATA(nlh), sizeof(str));
    
    dbgprint(DEBUG_LEVEL,"receive message (pid:%d):%s\n", nlh->nlmsg_pid, str);

    return;
}

/** ************************************************************************//**
 *  @brief send tigger signal to userspace
 *  
 ** */
static 
int Nanosic_nl_thread(void *arg)
{
    /* Step 3: Loop over callback handlers */
    while (!kthread_should_stop() && atomic_read(&running))
    {
        if (0 == wait_event_interruptible(waitq,(0<atomic_read(&data_ready))))
        {
            char msg[20]="tigger";
            atomic_dec(&data_ready);
            Nanosic_nl_send(msg,strlen(msg));
            //printk("Nanosic_nl_send [%d]%s\n",(int)strlen(msg),msg);
        }
    }
    
    dbgprint(DEBUG_LEVEL,"Nanosic_nl thread stopping.\n");

    return 0;
}


/** ************************************************************************//**
 *  @brief
 *  
 ** */
int 
Nanosic_nl_create(void)
{
    struct netlink_kernel_cfg nkc;

    /*nanosic nl init*/
    nkc.groups = 0;
    nkc.flags = 0;
    nkc.input = Nanosic_nl_recv;
    nkc.cb_mutex = NULL;
    nkc.bind = NULL;
    nl_sk = netlink_kernel_create(&init_net, NETLINK_NANOSIC_ID, &nkc);
    if( !nl_sk ) {
        dbgprint(KERN_ERR,"[netlink] create netlink socket error!\n");
        goto _nl_err_1;
    }
    
    atomic_set(&running,1);
    atomic_set(&data_ready,0);
    init_waitqueue_head(&waitq);
    spin_lock_init(&gspinlock);

    initial = 1;
    
    /*thread init*/
    task = kthread_create(Nanosic_nl_thread,(void *)NULL,"k-nanosic");
    if (IS_ERR(task)){
        dbgprint(KERN_ERR,"Couldn't start kthread\n");
        task = NULL;
        goto _nl_err_2;
    }
    
    wake_up_process(task);    
#if 0
    /*参考用户手册 page 297页*/
    ret = request_irq( 333 ,Nanosic_nl_irq,IRQF_TRIGGER_RISING,"8030_irq",NULL);
    if(ret){
        printk(KERN_ALERT "request_irq err!\n");
    }
#endif

    dbgprint(DEBUG_LEVEL,"Nanosic_nl_create success!\n");
    
    return 0;

_nl_err_2:
    netlink_kernel_release(nl_sk);
_nl_err_1:
    return -1;
    
}

/** ************************************************************************//**
 *  @brief
 *  
 ** */
void 
Nanosic_nl_release(void)
{
#if 0
    free_irq( 333 ,NULL);
#endif 
    if(nl_sk)
        netlink_kernel_release(nl_sk);

    atomic_set(&running,0);
    
    Nanosic_nl_tigger();
    
    if (task) 
        kthread_stop(task);
    
    initial = 0;
    dbgprint(DEBUG_LEVEL,"Nanosic_nl_init release!\n");
}
 
