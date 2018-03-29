/*
 *viatel_cbp_sync.c
 *
 *VIA CBP driver for Linux
 *
 *Copyright (C) 2011 VIA TELECOM Corporation, Inc.
 *Author: VIA TELECOM Corporation, Inc.
 *
 *This package is free software; you can redistribute it and/or modify
 *it under the terms of the GNU General Public License version 2 as
 *published by the Free Software Foundation.
 *
 *THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 *IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 *WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/kthread.h>
#include "c2k_hw.h"

static int asc_debug;
#define ASCDPRT(fmt, arg...)  do { \
	if (asc_debug) \
		pr_debug("[C2K MODEM] " fmt, ##arg); \
	} while (0)
#define ASCPRT(fmt, arg...)  pr_debug("[C2K MODEM] " fmt, ##arg)

/*mS*/
#define ASC_RX_WAIT_IDLE_TIME (1000)
#define ASC_TX_WAIT_READY_TIME (500)	/*1000 */
#define ASC_TX_WAIT_IDLE_TIME (2000)
#define ASC_TX_AUTO_DELAY_TIME (200)	/*(2000) */
#define ASC_TX_WAIT_SLEEP_TIME (500)
#define ASC_TX_TRY_TIMES (5)	/*3 */
#define ASC_TX_RETRY_DELAY (100)
#define ASC_TX_DEBOUNCE_TIME (10)
#define ASC_TX_AFTER_CP_SLEEP_TIME (10)

#define ASC_TX_SYSFS_USER "AscApp"
#define ASC_TX_AUTO_USER "AscAuto"

/*asc_list contains all registered struct struct asc_handle*/
static DEFINE_SPINLOCK(hdlock);
static LIST_HEAD(asc_tx_handle_list);
static LIST_HEAD(asc_rx_handle_list);
static struct workqueue_struct *asc_work_queue;
static struct kobject *asc_kobj;

enum {
	ASC_TX_HD = 0,
	ASC_RX_HD
};

#define ASC_EVENT_POOL_MAX (60)
enum {
	ASC_EVENT_UNUSE = 0,
	ASC_EVENT_STATIC,
	ASC_EVENT_DYNAMIC
};

struct asc_event {
	int id;
	struct list_head list;
	char usage;
};

static struct asc_event event_pool[ASC_EVENT_POOL_MAX];

struct asc_user {
	struct asc_infor infor;
	atomic_t count;
	struct list_head node;
};

struct asc_state_dsp {
	char name[ASC_NAME_LEN];
	/*state callback handle for events */
	int (*handle)(void *hd, int event);
};

/*TX STATUS and TX EVENT*/
enum {
	AP_TX_EVENT_REQUEST = 0,	/*internal */
	AP_TX_EVENT_CP_READY,
	AP_TX_EVENT_CP_UNREADY,
	AP_TX_EVENT_WAIT_TIMEOUT,
	AP_TX_EVENT_IDLE_TIMEOUT,
	AP_TX_EVENT_STOP,
	AP_TX_EVENT_RESET,
	AP_TX_EVENT_NUM
};

enum {
	AP_TX_ST_SLEEP = 0,
	AP_TX_ST_WAIT_READY,
	AP_TX_ST_READY,		/*wait All Tx channel finished */
	AP_TX_ST_IDLE,
	AP_TX_ST_NUM
};

struct asc_tx_handle {
	struct asc_config cfg;
	atomic_t state;
	atomic_t count;
	int ready_hold;
	atomic_t delay_sleep;
	struct list_head user_list;
	struct asc_state_dsp *table;
	/*process the event to switch different states */
	struct task_struct *thread;
	atomic_t sleeping;
	int ntf;
	int wait_try;
	int auto_delay;
	spinlock_t slock;
	spinlock_t ready_slock;
	/*spinlock user_count_lock is used to aviod operating user->count by mistake
	   when concurrent between asc_tx_get_ready/asc_tx_auto_ready and asc_tx_put_ready */
	spinlock_t user_count_lock;
	wait_queue_head_t wait;
	wait_queue_head_t wait_tx_state;
	struct mutex mlock;
	struct wake_lock wlock;
	struct timer_list timer_wait_ready;
	struct timer_list timer_wait_idle;
	struct timer_list timer_wait_sleep;
	/*
	   after trigger cp sleep, we should wait for a while before wake cp,
	   to make sure cp can handle ap_wake_cp interrupt
	 */
	struct timer_list timer_wait_after_cp_sleep;
	atomic_t trigger_cp_sleep;

	struct work_struct ntf_work;
	struct list_head event_q;
	struct list_head node;
	struct kobject *kobj;
};

static int asc_tx_handle_sleep(void *, int);
static int asc_tx_handle_wait_ready(void *, int);
static int asc_tx_handle_ready(void *, int);
static int asc_tx_handle_idle(void *, int);

/*the table used to discribe all tx states*/
static struct asc_state_dsp asc_tx_table[AP_TX_ST_NUM] = {
	[AP_TX_ST_SLEEP] = {
			    .name = "AP_TX_ST_SLEEP",
			    .handle = asc_tx_handle_sleep,
			    },
	[AP_TX_ST_WAIT_READY] = {
				 .name = "AP_TX_ST_WAIT_READY",
				 .handle = asc_tx_handle_wait_ready,
				 },
	[AP_TX_ST_READY] = {
			    .name = "AP_TX_ST_READY",
			    .handle = asc_tx_handle_ready,
			    },
	[AP_TX_ST_IDLE] = {
			   .name = "AP_TX_ST_IDLE",
			   .handle = asc_tx_handle_idle,
			   },
};

/*RX STATUS and RX EVENT*/
enum {
	AP_RX_EVENT_REQUEST = 0,
	AP_RX_EVENT_AP_READY,
	AP_RX_EVENT_AP_UNREADY,
	AP_RX_EVENT_STOP,
	AP_RX_EVENT_IDLE_TIMEOUT,
	AP_RX_EVENT_RESET,
	AP_RX_EVENT_NUM
};

enum {
	AP_RX_ST_SLEEP = 0,
	AP_RX_ST_WAIT_READY,
	AP_RX_ST_READY,
	AP_RX_ST_IDLE,
	AP_RX_ST_NUM
};

struct asc_rx_handle {
	struct asc_config cfg;
	atomic_t state;
	struct list_head user_list;
	struct asc_state_dsp *table;
	int ntf;
	/*process the event to switch different states */
	struct task_struct *thread;
	spinlock_t slock;
	wait_queue_head_t wait;
	struct mutex mlock;
	struct wake_lock wlock;
	struct timer_list timer;
	struct list_head event_q;
	struct list_head node;
	struct work_struct ntf_prepare_work;
	struct work_struct ntf_post_work;
	struct kobject *kobj;
};

static int asc_rx_handle_sleep(void *, int);
static int asc_rx_handle_wait_ready(void *, int);
static int asc_rx_handle_ready(void *, int);
static int asc_rx_handle_idle(void *, int);

/*the table used to discribe all rx states*/
static struct asc_state_dsp asc_rx_table[AP_RX_ST_NUM] = {
	[AP_RX_ST_SLEEP] = {
			    .name = "AP_RX_ST_SLEEP",
			    .handle = asc_rx_handle_sleep,
			    },
	[AP_RX_ST_WAIT_READY] = {
				 .name = "AP_RX_ST_WAIT_READY",
				 .handle = asc_rx_handle_wait_ready,
				 },
	[AP_RX_ST_READY] = {
			    .name = "AP_RX_ST_READY",
			    .handle = asc_rx_handle_ready,
			    },
	[AP_RX_ST_IDLE] = {
			   .name = "AP_RX_ST_IDLE",
			   .handle = asc_rx_handle_idle,
			   },
};

static int asc_tx_event_send(struct asc_tx_handle *tx, int id);
static void asc_tx_handle_reset(struct asc_tx_handle *tx);
static int asc_rx_event_send(struct asc_rx_handle *rx, int id);
static void asc_rx_handle_reset(struct asc_rx_handle *rx);

static struct asc_event *asc_event_malloc(void)
{
	int i = 0;
	unsigned long flags = 0;
	struct asc_event *event = NULL;

	spin_lock_irqsave(&hdlock, flags);
	for (i = 0; i < ASC_EVENT_POOL_MAX; i++) {
		if (ASC_EVENT_UNUSE == event_pool[i].usage) {
			event = &(event_pool[i]);
			event->usage = ASC_EVENT_STATIC;
		}
	}

	if (NULL == event) {
		event = kmalloc(sizeof(struct asc_event), GFP_ATOMIC);
		if (event)
			event->usage = ASC_EVENT_DYNAMIC;
	}
	spin_unlock_irqrestore(&hdlock, flags);
	return event;
}

static void asc_event_free(struct asc_event *event)
{
	unsigned long flags = 0;

	if (!event)
		return;
	spin_lock_irqsave(&hdlock, flags);
	if (ASC_EVENT_STATIC == event->usage)
		memset(event, 0, sizeof(struct asc_event));
	else
		kfree(event);

	spin_unlock_irqrestore(&hdlock, flags);
}

static irqreturn_t asc_irq_cp_indicate_state(int irq, void *data)
{
	int level;
	struct asc_tx_handle *tx = (struct asc_tx_handle *)data;
	struct asc_config *cfg = &tx->cfg;

#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
	spin_lock(&tx->ready_slock);
	/*when using internal EINT, we cannot get line status directly from GPIO API */
	level = !!c2k_gpio_to_ls(cfg->gpio_ready);
	c2k_gpio_set_irq_type(cfg->gpio_ready,
			      IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING);
	spin_unlock(&tx->ready_slock);
#else
	c2k_gpio_set_irq_type(cfg->gpio_ready,
			      IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING);
#endif

	level = !!c2k_gpio_get_value(cfg->gpio_ready);
	ASCDPRT("Irq %s cp_indicate_ap %s.\n", cfg->name,
	       (level == cfg->polar) ? "WAKEN" : "SLEEP");
	if (level == cfg->polar) {
		asc_tx_event_send(tx, AP_TX_EVENT_CP_READY);
	} else {
		/*do not care */
		/*asc_tx_event_send(tx, AP_TX_EVENT_CP_UNREADY); */
	}
#if defined(CONFIG_MTK_LEGACY)
	c2k_gpio_irq_unmask(cfg->gpio_ready);
#endif
	return IRQ_HANDLED;
}

int c2k_exception = 0;

static irqreturn_t asc_irq_cp_wake_ap(int irq, void *data)
{
	int level;
	struct asc_rx_handle *rx = (struct asc_rx_handle *)data;
	struct asc_config *cfg = &rx->cfg;

#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
	/*when using internal EINT, we cannot get line status directly from GPIO API */
	level = !!c2k_gpio_to_ls(cfg->gpio_wake);
#endif
	level = !!c2k_gpio_get_value(cfg->gpio_wake);
	c2k_gpio_set_irq_type(cfg->gpio_wake,
			      IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING);
	ASCDPRT("Irq %s cp_wake_ap, requset ap to be %s.\n", cfg->name,
		(level == cfg->polar) ? "WAKEN" : "SLEEP");

	if (level == cfg->polar) {
		/*Cp requset Ap wake */
		wake_lock(&rx->wlock);
		/*FIXME: jump to ready as soon as possible to avoid the AP_READY error indication to CBP */
		if (AP_RX_ST_IDLE == atomic_read(&rx->state)) {
			ASCDPRT("Rx(%s): process event(%d) in state(%s).\n",
				cfg->name, AP_RX_EVENT_REQUEST,
				rx->table[AP_RX_ST_IDLE].name);
			asc_rx_handle_idle(rx, AP_RX_EVENT_REQUEST);
			ASCDPRT("Rx(%s): go into state(%s).\n", cfg->name,
				rx->table[atomic_read(&rx->state)].name);
		}

		asc_rx_event_send(rx, AP_RX_EVENT_REQUEST);
	} else {
		/*Cp allow Ap sleep */
		asc_rx_event_send(rx, AP_RX_EVENT_STOP);
	}

#if 0
	if (mt_get_gpio_in(GPIO120)) {	/*GPIO120 high, md exception happend */
		pr_debug("[MODEM SDIO] GPIO120 high, modem exception!\n");
		c2k_exception = 1;
		gpio_irq_cbp_excp_ind();
	} else {
		pr_debug("[MODEM SDIO] no exception!\n");
	}
#endif
#if defined(CONFIG_MTK_LEGACY)
	c2k_gpio_irq_unmask(cfg->gpio_wake);
#endif
	return IRQ_HANDLED;
}

static struct asc_tx_handle *asc_tx_handle_lookup(const char *name)
{
	unsigned long flags;
	struct asc_tx_handle *hd, *tmp, *t;

	if (!name)
		return NULL;

	hd = NULL;

	spin_lock_irqsave(&hdlock, flags);
	list_for_each_entry_safe(tmp, t, &asc_tx_handle_list, node) {
		if (!strncmp(name, tmp->cfg.name, ASC_NAME_LEN - 1)) {
			hd = tmp;
			break;
		}
	}
	spin_unlock_irqrestore(&hdlock, flags);
	return hd;
}

static struct asc_rx_handle *asc_rx_handle_lookup(const char *name)
{
	unsigned long flags;
	struct asc_rx_handle *hd, *tmp, *t;

	if (!name)
		return NULL;

	hd = NULL;
	spin_lock_irqsave(&hdlock, flags);
	list_for_each_entry_safe(tmp, t, &asc_rx_handle_list, node) {
		if (!strncmp(name, tmp->cfg.name, ASC_NAME_LEN - 1)) {
			hd = tmp;
			break;
		}
	}
	spin_unlock_irqrestore(&hdlock, flags);
	return hd;
}

static struct asc_user *asc_tx_user_lookup(struct asc_tx_handle *tx,
					   const char *name)
{
	unsigned long flags = 0;
	struct asc_user *user = NULL, *tmp = NULL, *t = NULL;

	if (!name)
		return NULL;

	spin_lock_irqsave(&tx->slock, flags);
	list_for_each_entry_safe(tmp, t, &tx->user_list, node) {
		if (!strncmp(name, tmp->infor.name, ASC_NAME_LEN - 1)) {
			user = tmp;
			break;
		}
	}
	spin_unlock_irqrestore(&tx->slock, flags);

	return user;
}

static struct asc_user *asc_rx_user_lookup(struct asc_rx_handle *rx,
					   const char *name)
{
	unsigned long flags = 0;
	struct asc_user *user = NULL, *tmp = NULL, *t = NULL;

	if (!name)
		return NULL;

	spin_lock_irqsave(&rx->slock, flags);
	list_for_each_entry_safe(tmp, t, &rx->user_list, node) {
		if (!strncmp(name, tmp->infor.name, ASC_NAME_LEN - 1)) {
			user = tmp;
			break;
		}
	}
	spin_unlock_irqrestore(&rx->slock, flags);

	return user;
}

static inline void asc_rx_indicate_wake(struct asc_rx_handle *rx)
{
#ifdef CONFIG_EVDO_DT_VIA_SUPPORT
	/*if(rx->cfg.gpio_ready >= 0) */
	c2k_gpio_direction_output(rx->cfg.gpio_ready, rx->cfg.polar);
#else
	if (rx->cfg.gpio_ready == AP_USING_REGISTER)
		c2k_ap_ready_indicate(rx->cfg.polar);
#endif

}

static inline void asc_rx_indicate_sleep(struct asc_rx_handle *rx)
{
#ifdef CONFIG_EVDO_DT_VIA_SUPPORT
	/*if(rx->cfg.gpio_ready >= 0) */
	c2k_gpio_direction_output(rx->cfg.gpio_ready, !rx->cfg.polar);
#else
	if (rx->cfg.gpio_ready == AP_USING_REGISTER)
		c2k_ap_ready_indicate(!rx->cfg.polar);
#endif

}

static int asc_rx_event_send(struct asc_rx_handle *rx, int id)
{
	unsigned long flags = 0;
	struct asc_event *event = NULL;
	int ret = -ENODEV;

	if (rx->thread == NULL) {
		ASCPRT("%s:no thread for event\n", __func__);
		return ret;
	}

	/*check whether the event is cared by current charge state */
	if (id >= 0) {
		event = asc_event_malloc();
		if (!event) {
			ASCPRT("No memory to create new event.\n");
			ret = -ENOMEM;
			goto send_event_error;
		}
		/*insert a new event to the list tail and wakeup the process thread */
		/*ASCDPRT("Rx(%s):send event(%d)  to state(%s).\n",
		   rx->name, id, rx->table[atomic_read(&rx->state)].name); */
		event->id = id;
		spin_lock_irqsave(&rx->slock, flags);
		if (AP_RX_EVENT_RESET == id)
			list_add(&event->list, &rx->event_q);
		else
			list_add_tail(&event->list, &rx->event_q);
		spin_unlock_irqrestore(&rx->slock, flags);
		wake_up(&rx->wait);
	}
	ret = 0;
 send_event_error:
	return ret;
}

static int asc_rx_event_recv(struct asc_rx_handle *rx)
{
	unsigned long flags = 0;
	struct asc_event *event = NULL;
	int ret = -ENODEV;

	if (rx->thread == NULL) {
		ASCPRT("%s:no thread for event\n", __func__);
		return ret;
	}

	spin_lock_irqsave(&rx->slock, flags);
	if (!list_empty(&rx->event_q)) {
		event = list_first_entry(&rx->event_q, struct asc_event, list);
		list_del(&event->list);
	}
	spin_unlock_irqrestore(&rx->slock, flags);

	if (event) {
		ret = event->id;
		asc_event_free(event);
	}
	return ret;
}

static int asc_rx_event_thread(void *data)
{
	struct asc_rx_handle *rx = (struct asc_rx_handle *)data;
	int id = 0, index;
	char name[ASC_NAME_LEN] = { 0 };
	struct asc_state_dsp *dsp = NULL;

	rx->thread = current;
	snprintf(name, ASC_NAME_LEN, "asc_rx_%s", rx->cfg.name);

	ASCDPRT("%s thread start now.\n", name);

	while (1) {
		/*sleep until receive an evnet or thread exist */
		wait_event(rx->wait, ((id = asc_rx_event_recv(rx)) >= 0)
			   || (!rx->thread));
		/*thread is existed */
		if (!rx->thread)
			break;

		mutex_lock(&rx->mlock);
		if (AP_RX_EVENT_RESET == id) {
			asc_rx_handle_reset(rx);
		} else {
			index = atomic_read(&rx->state);
			dsp = rx->table + index;
			if (dsp->handle) {
				ASCDPRT
				    ("Rx(%s): process event(%d) in state(%s).\n",
				     rx->cfg.name, id, dsp->name);
				dsp->handle(rx, id);
				ASCDPRT("Rx(%s): go into state(%s).\n",
					rx->cfg.name,
					rx->
					table[atomic_read(&rx->state)].name);
			}
		}
		mutex_unlock(&rx->mlock);
	}

	ASCDPRT("%s thread exit.\n", name);
	kfree(rx);
	return 0;
}

static void asc_rx_event_timer(unsigned long data)
{
	struct asc_rx_handle *rx = (struct asc_rx_handle *)data;
	/*ASCDPRT("%s timer is  time out.\n", rx->name); */
	asc_rx_event_send(rx, AP_RX_EVENT_IDLE_TIMEOUT);
}

static void asc_tx_notifier_work(struct work_struct *work)
{
	struct asc_infor *infor;
	struct asc_user *user = NULL, *t = NULL;
	struct asc_tx_handle *tx = container_of(work, struct asc_tx_handle,
						ntf_work);

	list_for_each_entry_safe(user, t, &tx->user_list, node) {
		infor = &user->infor;
		if (infor->notifier)
			infor->notifier(tx->ntf, infor->data);
	}
}

static void asc_rx_notifier_prepare_work(struct work_struct *work)
{
	struct asc_infor *infor;
	struct asc_user *user = NULL, *t = NULL;
	struct asc_rx_handle *rx = container_of(work, struct asc_rx_handle,
						ntf_prepare_work);

	list_for_each_entry_safe(user, t, &rx->user_list, node) {
		infor = &user->infor;
		if (infor->notifier)
			infor->notifier(ASC_NTF_RX_PREPARE, infor->data);
	}
}

static void asc_rx_notifier_post_work(struct work_struct *work)
{
	struct asc_infor *infor;
	struct asc_user *user = NULL, *t = NULL;
	struct asc_rx_handle *rx = container_of(work, struct asc_rx_handle,
						ntf_post_work);

	list_for_each_entry_safe(user, t, &rx->user_list, node) {
		infor = &user->infor;
		if (infor->notifier)
			infor->notifier(ASC_NTF_RX_POST, infor->data);
	}
}

static void asc_tx_notifier(struct asc_tx_handle *tx, int ntf)
{
	tx->ntf = ntf;
	queue_work(asc_work_queue, &tx->ntf_work);
}

static void asc_rx_notifier(struct asc_rx_handle *rx, int ntf)
{
#ifdef CONFIG_EVDO_DT_VIA_SUPPORT
	rx->ntf = ntf;
	if (ASC_NTF_RX_PREPARE == ntf) {
		ASCPRT("asc_rx_notifier: queue_work ntf_prepare_work\n");
		queue_work(asc_work_queue, &rx->ntf_prepare_work);
	} else if (ASC_NTF_RX_POST == ntf) {
		queue_work(asc_work_queue, &rx->ntf_post_work);
	}
#else
	struct asc_infor *infor;
	struct asc_user *user = NULL, *t = NULL;

	ASCDPRT("asc_rx_notifier start, ntf = %d\n", ntf);
	list_for_each_entry_safe(user, t, &rx->user_list, node) {
		infor = &user->infor;
		if (infor->notifier)
			infor->notifier(ntf, infor->data);
	}
	ASCDPRT("asc_rx_notifier end, ntf = %d\n", ntf);
#endif

}

static int asc_rx_handle_init(struct asc_rx_handle *rx)
{
	int ret = 0;
	char *name = NULL;
	struct asc_config *cfg = &rx->cfg;

	asc_rx_indicate_sleep(rx);

#if defined(CONFIG_MTK_LEGACY)
	c2k_gpio_irq_mask(cfg->gpio_wake);
#endif
	c2k_gpio_direction_input_for_irq(cfg->gpio_wake);
	c2k_gpio_set_irq_type(cfg->gpio_wake,
			      IRQF_TRIGGER_RISING |
			      IRQF_TRIGGER_FALLING);
	ret =
	    c2k_gpio_request_irq(cfg->gpio_wake, asc_irq_cp_wake_ap,
				 IRQF_SHARED | IRQF_NO_SUSPEND,
				 "cp_wake_ap", rx);
#if defined(CONFIG_MTK_LEGACY)
	c2k_gpio_irq_unmask(cfg->gpio_wake);
#endif
	if (ret < 0) {
		ASCPRT("fail to request cp_wake_ap irq for %s\n",
		       cfg->name);
		goto err_req_irq_cp_wake_ap;
	}

	rx->table = asc_rx_table;
	mutex_init(&rx->mlock);
	INIT_LIST_HEAD(&rx->event_q);
	INIT_LIST_HEAD(&rx->user_list);
	spin_lock_init(&rx->slock);
	setup_timer(&rx->timer, asc_rx_event_timer, (unsigned long)rx);
	name = kzalloc(ASC_NAME_LEN, GFP_KERNEL);
	if (!name) {
		ret = -ENOMEM;
		ASCPRT("%s: no memory to malloc for wake lock name\n",
		       __func__);
		goto err_malloc_name;
	}
	snprintf(name, ASC_NAME_LEN, "asc_rx_%s", rx->cfg.name);
	wake_lock_init(&rx->wlock, WAKE_LOCK_SUSPEND, name);
	init_waitqueue_head(&rx->wait);
	INIT_WORK(&rx->ntf_prepare_work, asc_rx_notifier_prepare_work);
	INIT_WORK(&rx->ntf_post_work, asc_rx_notifier_post_work);
	atomic_set(&rx->state, AP_RX_ST_SLEEP);

	kthread_run(asc_rx_event_thread, rx, "C2K_RX_ASC");

	if (!!c2k_gpio_get_value(cfg->gpio_wake) == cfg->polar)
		asc_rx_event_send(rx, AP_RX_EVENT_REQUEST);

	return 0;

 err_malloc_name:
	/*if(cfg->gpio_wake >= 0) */
	free_irq(c2k_gpio_to_irq(cfg->gpio_wake), rx);
 err_req_irq_cp_wake_ap:
/*err_request_gpio_cp_wake_ap:*/
/*err_request_gpio_ap_ready:*/
	return ret;
}

static int asc_rx_handle_sleep(void *data, int event)
{
	int ret = 0;
	struct asc_rx_handle *rx = (struct asc_rx_handle *)data;

	/*ASCDPRT("Rx(%s): process event(%d) in state(%s).\n", rx->name,
	   event, rx->table[atomic_read(&rx->state)].name); */

	if (AP_RX_ST_SLEEP != atomic_read(&rx->state))
		return 0;

	switch (event) {
	case AP_RX_EVENT_REQUEST:
		wake_lock(&rx->wlock);
		atomic_set(&rx->state, AP_RX_ST_WAIT_READY);
		asc_rx_notifier(rx, ASC_NTF_RX_PREPARE);
		break;
	default:
		ASCDPRT("ignore the rx event %d in state(%s)", event,
			rx->table[atomic_read(&rx->state)].name);
	}

	/*ASCDPRT("Rx(%s): go into state(%s).\n", rx->name, rx->table[atomic_read(&rx->state)].name); */
	return ret;
}

static int asc_rx_handle_wait_ready(void *data, int event)
{
	int ret = 0;
	struct asc_rx_handle *rx = (struct asc_rx_handle *)data;

	/*ASCDPRT("Rx(%s): process event(%d) in state(%s).\n",
	   rx->name, event, rx->table[atomic_read(&rx->state)].name); */

	if (AP_RX_ST_WAIT_READY != atomic_read(&rx->state))
		return 0;

	switch (event) {
	case AP_RX_EVENT_AP_READY:
		/*need ack ready to cp, do nothing if no gpio for ap_ready */
		asc_rx_indicate_wake(rx);
		atomic_set(&rx->state, AP_RX_ST_READY);
		break;
	case AP_RX_EVENT_AP_UNREADY:
	case AP_RX_EVENT_STOP:
		atomic_set(&rx->state, AP_RX_ST_SLEEP);
		asc_rx_notifier(rx, ASC_NTF_RX_POST);
		/*need ack ready to cp, do nothing if no gpio for ap_ready */
		asc_rx_indicate_sleep(rx);
		wake_unlock(&rx->wlock);
		break;
	default:
		ASCDPRT("ignore the rx event %d in state(%s)", event,
			rx->table[atomic_read(&rx->state)].name);
	}

	/*ASCDPRT("Rx(%s): go into state(%s).\n", rx->name, rx->table[atomic_read(&rx->state)].name); */
	return ret;
}

static int asc_rx_handle_ready(void *data, int event)
{
	int ret = 0;
	struct asc_rx_handle *rx = (struct asc_rx_handle *)data;

	/*ASCDPRT("Rx(%s): process event(%d) in state(%s).\n",
	   rx->name, event, rx->table[atomic_read(&rx->state)].name); */

	if (AP_RX_ST_READY != atomic_read(&rx->state))
		return 0;

	switch (event) {
	case AP_RX_EVENT_STOP:
		atomic_set(&rx->state, AP_RX_ST_IDLE);
		asc_rx_event_send(rx, AP_RX_EVENT_IDLE_TIMEOUT);
		/*mod_timer(&rx->timer,
			  jiffies + msecs_to_jiffies(ASC_RX_WAIT_IDLE_TIME));*/
		break;
	default:
		ASCDPRT("ignore the rx event %d in state(%s)", event,
			rx->table[atomic_read(&rx->state)].name);
	}

	/*ASCDPRT("Rx(%s): go into state(%s).\n", rx->name, rx->table[atomic_read(&rx->state)].name); */
	return ret;
}

static int asc_rx_handle_idle(void *data, int event)
{
	int ret = 0;
	unsigned long flags = 0;
	struct asc_rx_handle *rx = (struct asc_rx_handle *)data;

	/*FIXME: prevent from scheduled and interrupted to avoid error indication to CBP */
	spin_lock_irqsave(&rx->slock, flags);

	/*ASCDPRT("Rx(%s): process event(%d) in state(%s).\n",
	   rx->name, event, rx->table[atomic_read(&rx->state)].name); */
	if (AP_RX_ST_IDLE != atomic_read(&rx->state))
		goto _end;

	switch (event) {
	case AP_RX_EVENT_REQUEST:
		del_timer(&rx->timer);
		atomic_set(&rx->state, AP_RX_ST_READY);
		break;

	case AP_RX_EVENT_IDLE_TIMEOUT:
		asc_rx_notifier(rx, ASC_NTF_RX_POST);
		atomic_set(&rx->state, AP_RX_ST_SLEEP);
		/*need ack ready to cp, do nothing if no gpio for ap_ready */
		asc_rx_indicate_sleep(rx);
		wake_unlock(&rx->wlock);
		break;

	default:
		ASCDPRT("ignore the rx event %d in state(%s)", event,
			rx->table[atomic_read(&rx->state)].name);
	}

 _end:
	spin_unlock_irqrestore(&rx->slock, flags);

	/*ASCDPRT("Rx(%s): go into state(%s).\n", rx->name, rx->table[atomic_read(&rx->state)].name); */
	return ret;
}

static void asc_tx_trig_busy(struct asc_tx_handle *tx)
{
	atomic_set(&tx->delay_sleep, 0);
	mod_timer(&tx->timer_wait_idle,
		  jiffies + msecs_to_jiffies(tx->auto_delay));
}

static inline void asc_tx_wake_cp(struct asc_tx_handle *tx)
{
	int retry = 0;
	unsigned long flags = 0;
#ifdef CONFIG_EVDO_DT_VIA_SUPPORT
	/*if(tx->cfg.gpio_wake >= 0) */
	c2k_gpio_direction_output(tx->cfg.gpio_wake, tx->cfg.polar);
#else
	if (tx->cfg.gpio_wake == AP_USING_REGISTER) {
		while (atomic_read(&tx->trigger_cp_sleep)) {
			msleep(20);
			retry++;
			if (retry >= 5) {
				atomic_set(&tx->trigger_cp_sleep, 0);
				break;
			}
		}
		spin_lock_irqsave(&tx->ready_slock, flags);
		c2k_ap_wake_cp(tx->cfg.polar);
		spin_unlock_irqrestore(&tx->ready_slock, flags);
	}
#endif
}

static inline void asc_tx_sleep_cp(struct asc_tx_handle *tx)
{
#ifdef CONFIG_EVDO_DT_VIA_SUPPORT
	/*if(tx->cfg.gpio_wake >= 0) */
	c2k_gpio_direction_output(tx->cfg.gpio_wake, !tx->cfg.polar);
#else
	if (tx->cfg.gpio_wake == AP_USING_REGISTER) {
		atomic_set(&tx->trigger_cp_sleep, 1);
		mod_timer(&tx->timer_wait_after_cp_sleep,
			  jiffies +
			  msecs_to_jiffies(ASC_TX_AFTER_CP_SLEEP_TIME));
		c2k_ap_wake_cp(!tx->cfg.polar);
	}
#endif

}

static inline int asc_tx_cp_be_ready(struct asc_tx_handle *tx)
{
	int ret = 0;

	/*if(tx->cfg.gpio_ready >= 0) */
	ret =
		   ((!!c2k_gpio_get_value(tx->cfg.gpio_ready)) ==
		     (tx->cfg.polar));

	ASCDPRT("asc_tx_cp_be_ready ret %d.\n", ret);
	return ret;
}

static int asc_tx_event_send(struct asc_tx_handle *tx, int id)
{
	unsigned long flags = 0;
	struct asc_event *event = NULL;
	int ret = -ENODEV;

	if (tx->thread == NULL) {
		ASCPRT("%s:no thread for event\n", __func__);
		return ret;
	}

	/*check whether the event is cared by current charge state */
	if (id >= 0) {
		event = asc_event_malloc();
		if (!event) {
			ASCPRT("No memory to create new event.\n");
			ret = -ENOMEM;
			goto send_event_error;
		}
		/*insert a new event to the list tail and wakeup the process thread */
		/*ASCDPRT("Send tx event(%d)  to state(%s).\n", id, tx->table[atomic_read(&tx->state)].name); */
		event->id = id;
		spin_lock_irqsave(&tx->slock, flags);
		if (AP_TX_EVENT_RESET == id)
			list_add(&event->list, &tx->event_q);
		else
			list_add_tail(&event->list, &tx->event_q);
		spin_unlock_irqrestore(&tx->slock, flags);
		wake_up(&tx->wait);
	}
 send_event_error:
	return ret;
}

static int asc_tx_event_recv(struct asc_tx_handle *tx)
{
	unsigned long flags = 0;
	struct asc_event *event = NULL;
	int ret = -ENODEV;

	if (tx->thread == NULL) {
		ASCPRT("%s:no thread for event\n", __func__);
		return ret;
	}

	spin_lock_irqsave(&tx->slock, flags);
	if (!list_empty(&tx->event_q)) {
		event = list_first_entry(&tx->event_q, struct asc_event, list);
		list_del(&event->list);
	}
	spin_unlock_irqrestore(&tx->slock, flags);

	if (event) {
		ret = event->id;
		asc_event_free(event);
	}
	return ret;
}

static int asc_tx_get_user(struct asc_tx_handle *tx, const char *name)
{
	int ret = 0;
	struct asc_user *user = NULL;

	user = asc_tx_user_lookup(tx, name);
	if (user)
		atomic_inc(&user->count);
	else
		ret = -ENODEV;

	return ret;
}

static int asc_tx_put_user(struct asc_tx_handle *tx, const char *name)
{
	struct asc_user *user = NULL;
	int ret = 0;

	user = asc_tx_user_lookup(tx, name);

	if (user) {
		if (atomic_read(&user->count) >= 1)
			atomic_dec(&user->count);
	} else {
		ret = -ENODEV;
	}

	return ret;
}

static int asc_tx_refer(struct asc_tx_handle *tx, const char *name)
{
	unsigned long flags = 0;
	struct asc_user *user = NULL, *t = NULL;
	int count = 0;

	if (name) {
		/*get the reference count of the user */
		user = asc_tx_user_lookup(tx, name);
		if (user)
			count = atomic_read(&user->count);
	} else {
		spin_lock_irqsave(&tx->slock, flags);
		list_for_each_entry_safe(user, t, &tx->user_list, node) {
			count += atomic_read(&user->count);
		}
		spin_unlock_irqrestore(&tx->slock, flags);
	}

	return count;
}

static int asc_rx_refer(struct asc_rx_handle *rx, const char *name)
{
	unsigned long flags = 0;
	struct asc_user *user = NULL, *t = NULL;
	int count = 0;

	if (name) {
		/*get the reference count of the user */
		user = asc_rx_user_lookup(rx, name);
		if (user)
			count = atomic_read(&user->count);
	} else {
		spin_lock_irqsave(&rx->slock, flags);
		list_for_each_entry_safe(user, t, &rx->user_list, node) {
			count += atomic_read(&user->count);
		}
		spin_unlock_irqrestore(&rx->slock, flags);
	}

	return count;
}

static void asc_tx_refer_clear(struct asc_tx_handle *tx)
{
	unsigned long flags = 0;
	struct asc_user *user = NULL, *t = NULL;

	spin_lock_irqsave(&tx->slock, flags);
	list_for_each_entry_safe(user, t, &tx->user_list, node) {
		atomic_set(&user->count, 0);
	}
	spin_unlock_irqrestore(&tx->slock, flags);
}

static int asc_tx_event_thread(void *data)
{
	struct asc_tx_handle *tx = (struct asc_tx_handle *)data;
	int id = 0, index;
	char name[ASC_NAME_LEN] = { 0 };
	struct asc_state_dsp *dsp = NULL;

	snprintf(name, ASC_NAME_LEN, "asc_tx_%s", tx->cfg.name);
	tx->thread = current;

	ASCDPRT("%s thread start now.\n", name);

	while (1) {
		/*sleep until receive an evnet or thread exist */
		wait_event(tx->wait, ((id = asc_tx_event_recv(tx)) >= 0)
			   || (!tx->thread));
		/*thread is existed */
		if (!tx->thread)
			break;

		mutex_lock(&tx->mlock);
		if (AP_TX_EVENT_RESET == id) {
			asc_tx_handle_reset(tx);
		} else {
			index = atomic_read(&tx->state);
			dsp = tx->table + index;
			if (dsp->handle) {
				ASCDPRT
				    ("Tx(%s): process event(%d) in state(%s).\n",
				     tx->cfg.name, id, dsp->name);
				dsp->handle(tx, id);
				ASCDPRT("Tx(%s): go into state(%s) .\n",
					tx->cfg.name,
					tx->
					table[atomic_read(&tx->state)].name);
			}
		}
		mutex_unlock(&tx->mlock);
	}

	ASCDPRT("%s thread exit.\n", name);
	kfree(tx);
	return 0;
}

static void asc_tx_wait_ready_timer(unsigned long data)
{
	struct asc_tx_handle *tx = (struct asc_tx_handle *)data;
	/*ASCDPRT("%s tx wait ready timer is timeout.\n", tx->name); */
	asc_tx_event_send(tx, AP_TX_EVENT_WAIT_TIMEOUT);
}

static void asc_tx_wait_idle_timer(unsigned long data)
{
	char path[ASC_NAME_LEN] = { 0 };
	struct asc_tx_handle *tx = (struct asc_tx_handle *)data;
	unsigned long flags = 0;

	ASCDPRT("%s: tx wait idle timer is timeout.\n", tx->cfg.name);

	spin_lock_irqsave(&tx->slock, flags);
	if (0 == tx->ready_hold) {
		spin_unlock_irqrestore(&tx->slock, flags);
		snprintf(path, ASC_NAME_LEN, "%s.%s", tx->cfg.name,
			 ASC_TX_AUTO_USER);
		asc_tx_put_ready(path, 0);
	} else {
		atomic_set(&tx->delay_sleep, 1);
		spin_unlock_irqrestore(&tx->slock, flags);
		ASCPRT("%s: has user.\n", tx->cfg.name);
	}
}

static void asc_tx_wait_sleep_timer(unsigned long data)
{
	struct asc_tx_handle *tx = (struct asc_tx_handle *)data;
	/*ASCDPRT("%s tx wait sleep timer is timeout.\n", tx->name); */
	asc_tx_event_send(tx, AP_TX_EVENT_IDLE_TIMEOUT);
}

static void asc_tx_wait_after_cp_sleep(unsigned long data)
{
	struct asc_tx_handle *tx = (struct asc_tx_handle *)data;

	atomic_set(&tx->trigger_cp_sleep, 0);
}

static int asc_tx_handle_init(struct asc_tx_handle *tx)
{
	int ret = 0;
	char *name = NULL;
	struct asc_config *cfg = &tx->cfg;

	spin_lock_init(&tx->ready_slock);

#if defined(CONFIG_MTK_LEGACY)
	c2k_gpio_irq_mask(cfg->gpio_ready);
#endif
	c2k_gpio_direction_input_for_irq(cfg->gpio_ready);
	c2k_gpio_set_irq_type(cfg->gpio_ready,
			      IRQF_TRIGGER_RISING |
			      IRQF_TRIGGER_FALLING);
	ret =
	    c2k_gpio_request_irq(cfg->gpio_ready,
				 asc_irq_cp_indicate_state, IRQF_SHARED,
				 "cp_indicate_state", tx);
#if defined(CONFIG_MTK_LEGACY)
	c2k_gpio_irq_unmask(cfg->gpio_ready);
#endif
	if (ret < 0) {
		ASCPRT("fail to request irq for %s:cp_ready\n",
		       cfg->name);
		goto err_req_irq_cp_indicate_state;
	}

	atomic_set(&tx->trigger_cp_sleep, 0);
	setup_timer(&tx->timer_wait_after_cp_sleep, asc_tx_wait_after_cp_sleep,
		    (unsigned long)tx);
	asc_tx_sleep_cp(tx);

	tx->auto_delay = ASC_TX_AUTO_DELAY_TIME;
	tx->table = asc_tx_table;
	mutex_init(&tx->mlock);
	INIT_LIST_HEAD(&tx->event_q);
	INIT_LIST_HEAD(&tx->user_list);
	spin_lock_init(&tx->slock);
	spin_lock_init(&tx->user_count_lock);
	name = kzalloc(ASC_NAME_LEN, GFP_KERNEL);
	if (!name) {
		ret = -ENOMEM;
		ASCPRT("%s: no memory to malloc for wake lock name\n",
		       __func__);
		goto err_malloc_name;
	}
	snprintf(name, ASC_NAME_LEN, "asc_tx_%s", tx->cfg.name);
	wake_lock_init(&tx->wlock, WAKE_LOCK_SUSPEND, name);
	init_waitqueue_head(&tx->wait);
	init_waitqueue_head(&tx->wait_tx_state);
	setup_timer(&tx->timer_wait_ready, asc_tx_wait_ready_timer,
		    (unsigned long)tx);
	setup_timer(&tx->timer_wait_idle, asc_tx_wait_idle_timer,
		    (unsigned long)tx);
	setup_timer(&tx->timer_wait_sleep, asc_tx_wait_sleep_timer,
		    (unsigned long)tx);
	atomic_set(&tx->state, AP_TX_ST_SLEEP);
	atomic_set(&tx->count, 0);
	atomic_set(&tx->sleeping, 0);
	atomic_set(&tx->delay_sleep, 0);
	tx->ready_hold = 0;
	INIT_WORK(&tx->ntf_work, asc_tx_notifier_work);

	kthread_run(asc_tx_event_thread, tx, "C2K_TX_ASC");

	return 0;

 err_malloc_name:
	kfree(name);
 err_req_irq_cp_indicate_state:
/*err_request_gpio_cp_ready:*/
/*err_request_gpio_ap_wake_cp:*/
	return ret;
}

static int asc_tx_handle_sleep(void *data, int event)
{
	int ret = 0;
	struct asc_tx_handle *tx = (struct asc_tx_handle *)data;

	/*ASCDPRT("Tx(%s): process event(%d) in state(%s).\n",
	   tx->name, event, tx->table[atomic_read(&tx->state)].name); */

	if (AP_TX_ST_SLEEP != atomic_read(&tx->state))
		return 0;

	switch (event) {
	case AP_TX_EVENT_REQUEST:
		wake_lock(&tx->wlock);
		asc_tx_wake_cp(tx);
		/*if(tx->cfg.gpio_ready >= 0) */
		mod_timer(&tx->timer_wait_ready,
			  jiffies +
			  msecs_to_jiffies(ASC_TX_WAIT_READY_TIME));
		atomic_set(&tx->state, AP_TX_ST_WAIT_READY);
		if (asc_tx_cp_be_ready(tx)) {
			mdelay(ASC_TX_DEBOUNCE_TIME);	/*debounce wait, make sure CBP has already be ready */
			if (asc_tx_cp_be_ready(tx)) {
				ASCDPRT("Tx:cp %s was ready now.\n",
					tx->cfg.name);
				asc_tx_handle_wait_ready(tx,
							 AP_TX_EVENT_CP_READY);
			}
		}
		break;
	default:
		ASCDPRT("Tx: ignore event %d in state(%s)", event,
			tx->table[atomic_read(&tx->state)].name);
	}

	/*ASCDPRT("Tx(%s): go into state(%s).\n", tx->name, tx->table[atomic_read(&tx->state)].name); */
	return ret;
}

static int asc_tx_handle_wait_ready(void *data, int event)
{
	int ret = 0;
	struct asc_tx_handle *tx = (struct asc_tx_handle *)data;

	if (AP_TX_ST_WAIT_READY != atomic_read(&tx->state))
		return 0;

	/*ASCDPRT("Tx(%s): process event(%d) in state(%s).\n",
	   tx->name, event, tx->table[atomic_read(&tx->state)].name); */
	switch (event) {
	case AP_TX_EVENT_CP_READY:
		if (asc_debug == 1)
			asc_debug = 0;
		del_timer(&tx->timer_wait_ready);
		tx->wait_try = 0;
		atomic_set(&tx->state, AP_TX_ST_READY);
		wake_up_interruptible_all(&tx->wait_tx_state);
		/*if(asc_tx_refer(tx, ASC_TX_AUTO_USER) > 0){ */
		asc_tx_trig_busy(tx);
		/*} */
		asc_tx_notifier(tx, ASC_NTF_TX_READY);
		break;
	case AP_TX_EVENT_WAIT_TIMEOUT:
		ASCPRT("Tx: %s wait cp ready timeout,  try=%d.\n", tx->cfg.name,
		       tx->wait_try);
		if (asc_debug == 0)
			asc_debug = 1;

		asc_tx_sleep_cp(tx);
		mdelay(ASC_TX_RETRY_DELAY);	/*delay to create a implus */
		atomic_set(&tx->state, AP_TX_ST_SLEEP);
		if (tx->wait_try++ <= ASC_TX_TRY_TIMES) {
			asc_tx_event_send(tx, AP_TX_EVENT_REQUEST);
		} else {
			tx->wait_try = 0;
			atomic_set(&tx->state, AP_TX_ST_SLEEP);
			asc_tx_refer_clear(tx);
			wake_up_interruptible_all(&tx->wait_tx_state);
			wake_unlock(&tx->wlock);
			asc_tx_notifier(tx, ASC_NTF_TX_UNREADY);
			ASCPRT("try out to wake %s.\n", tx->cfg.name);
		}
		break;
	case AP_TX_EVENT_STOP:
		asc_tx_sleep_cp(tx);
		del_timer(&tx->timer_wait_ready);
		tx->wait_try = 0;
		atomic_set(&tx->state, AP_TX_ST_SLEEP);
		atomic_set(&tx->sleeping, 0);
		wake_unlock(&tx->wlock);
		wake_up_interruptible_all(&tx->wait_tx_state);
		break;
	default:
		ASCDPRT("Tx: ignore event %d in state(%s)", event,
			tx->table[atomic_read(&tx->state)].name);
	}

	/*ASCDPRT("Tx(%s): go into state(%s).\n", tx->name, tx->table[atomic_read(&tx->state)].name); */
	return ret;
}

static int asc_tx_handle_ready(void *data, int event)
{
	int ret = 0;
	struct asc_tx_handle *tx = (struct asc_tx_handle *)data;

	if (AP_TX_ST_READY != atomic_read(&tx->state))
		return 0;

	/*ASCDPRT("Tx(%s): process event(%d) in state(%s).\n",
	   tx->name, event, tx->table[atomic_read(&tx->state)].name); */
	switch (event) {
	case AP_TX_EVENT_STOP:
		del_timer(&tx->timer_wait_idle);
		asc_tx_sleep_cp(tx);
		atomic_set(&tx->state, AP_TX_ST_SLEEP);
		atomic_set(&tx->sleeping, 0);
		wake_unlock(&tx->wlock);
		/*mod_timer(&tx->timer_wait_sleep, jiffies + msecs_to_jiffies(ASC_TX_WAIT_IDLE_TIME)); */
		break;
	default:
		ASCDPRT("Tx: ignore event %d in state(%s)", event,
			tx->table[atomic_read(&tx->state)].name);
	}

	/*ASCDPRT("Tx(%s): go into state(%s).\n", tx->name, tx->table[atomic_read(&tx->state)].name); */
	return ret;
}

/*Ignore the idle state, wait for a while to let CBP go to sleep*/
static int asc_tx_handle_idle(void *data, int event)
{
	int ret = 0;
	struct asc_tx_handle *tx = (struct asc_tx_handle *)data;

	if (AP_TX_ST_IDLE != atomic_read(&tx->state))
		return 0;

	/*ASCDPRT("Tx(%s): process event(%d) in state(%s).\n",
	   tx->name, event, tx->table[atomic_read(&tx->state)].name); */
	switch (event) {
	case AP_TX_EVENT_IDLE_TIMEOUT:
		atomic_set(&tx->state, AP_TX_ST_SLEEP);
		wake_unlock(&tx->wlock);
		break;
	case AP_TX_EVENT_REQUEST:
		del_timer(&tx->timer_wait_sleep);
		atomic_set(&tx->state, AP_TX_ST_SLEEP);
		/*loop back to SLEEP handle */
		asc_tx_event_send(tx, AP_TX_EVENT_REQUEST);
		break;
	default:
		ASCDPRT("Tx: ignore event %d in state(%s)", event,
			tx->table[atomic_read(&tx->state)].name);
	}

	/*ASCDPRT("Tx(%s): go into state(%s).\n", tx->name, tx->table[atomic_read(&tx->state)].name); */
	return ret;
}

static void asc_tx_handle_reset(struct asc_tx_handle *tx)
{
	unsigned long flags;

	ASCDPRT("%s %s\n", __func__, tx->cfg.name);
	del_timer(&tx->timer_wait_ready);
	del_timer(&tx->timer_wait_idle);
	del_timer(&tx->timer_wait_sleep);
	spin_lock_irqsave(&tx->slock, flags);
	INIT_LIST_HEAD(&tx->event_q);
	spin_unlock_irqrestore(&tx->slock, flags);
	asc_tx_sleep_cp(tx);
	atomic_set(&tx->state, AP_TX_ST_SLEEP);
	wake_up_interruptible_all(&tx->wait_tx_state);
	wake_unlock(&tx->wlock);
}

/**
 *asc_tx_reset - reset the tx handle
 *@name: the config name for the handle
 *
 *return 0 ok, others be error
 */
void asc_tx_reset(const char *name)
{
	struct asc_tx_handle *tx = NULL;

	tx = asc_tx_handle_lookup(name);
	if (tx) {
		asc_tx_event_send(tx, AP_TX_EVENT_RESET);
#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
		c2k_reset_tx_gpio_ready(tx->cfg.gpio_ready);
		pr_debug("[C2K] reset tx handler!");
#endif
	}
}

/**
 *asc_tx_set_auto_delay - change the delay time for auto ready
 *@name: the config name for the handle
 *@delay: the time for auto ready which is valid while more than default value
 *return 0 ok,others be error
 */
int asc_tx_set_auto_delay(const char *name, int delay)
{
	int ret = 0;
	unsigned long flags;
	struct asc_tx_handle *tx;

	tx = asc_tx_handle_lookup(name);
	if (!tx) {
		ret = -ENODEV;
		goto end;
	}
	if (delay > 0) {
		spin_lock_irqsave(&tx->slock, flags);
		tx->auto_delay = delay;
		spin_unlock_irqrestore(&tx->slock, flags);
	}

 end:
	return ret;
}

/**
 *asc_tx_check_ready - check whether tx tanslation has alreay be ready
 *@name: the config name for the handle
 *
 *return 1 waken, 0 not, others be error
 */
int asc_tx_check_ready(const char *name)
{
	int ret = 0;
	struct asc_tx_handle *tx;

	tx = asc_tx_handle_lookup(name);
	if (NULL == tx)
		return -ENODEV;

	ret = atomic_read(&tx->state);

	if ((ret == AP_TX_ST_READY) && (!atomic_read(&tx->sleeping)))
		ret = 1;
	else
		ret = 0;

	return ret;
}

/**
 *asc_tx_user_counts - get the refernce count of the user or the handle
 *@path: (handle name).[user name]
 *If user name is NULL, return the count of tx handle.
 *others return the count of the tx user
 */
int asc_tx_user_count(const char *path)
{
	const char *name;
	char hname[ASC_NAME_LEN] = { 0 };
	struct asc_tx_handle *tx = NULL;

	name = strchr(path, '.');
	if (name) {
		memcpy(hname, path, min((int)(name - path), (int)(ASC_NAME_LEN - 1)));
		name++;
	} else {
		strncpy(hname, path, ASC_NAME_LEN - 1);
	}

	tx = asc_tx_handle_lookup(hname);

	if (NULL == tx)
		return -ENODEV;

	return asc_tx_refer(tx, name);
}

/**
 *asc_tx_add_user - add a user for tx handle
 *@name: the config name for the handle
 *@infor: the user information
 *
 *return 0,  others be error
 */
int asc_tx_add_user(const char *name, struct asc_infor *infor)
{
	int ret = 0;
	unsigned long flags = 0;
	struct asc_tx_handle *tx;
	struct asc_user *user;

	tx = asc_tx_handle_lookup(name);
	if (NULL == tx)
		return -ENODEV;

	user = asc_tx_user_lookup(tx, infor->name);
	if (NULL == user) {
		user = kzalloc(sizeof(*user), GFP_KERNEL);
		if (!user) {
			ASCPRT("No memory to create new user reference.\n");
			ret = -ENOMEM;
			goto error;
		}
		user->infor.data = infor->data;
		user->infor.notifier = infor->notifier;
		strncpy(user->infor.name, infor->name, ASC_NAME_LEN - 1);
		atomic_set(&user->count, 0);
		spin_lock_irqsave(&tx->slock, flags);
		list_add_tail(&user->node, &tx->user_list);
		spin_unlock_irqrestore(&tx->slock, flags);
	} else {
		ASCPRT("%s error: user %s already exist!!\n", __func__,
		       infor->name);
		ret = -EINVAL;
	}
 error:
	return ret;
}

/**
 *asc_tx_del_user - delete a user for tx handle
 *@path: (handle name).(user name)
 *
 *no return
 */
void asc_tx_del_user(const char *path)
{
	unsigned long flags = 0;
	char hname[ASC_NAME_LEN] = { 0 };
	const char *name;
	struct asc_user *user = NULL;
	struct asc_tx_handle *tx = NULL;

	name = strchr(path, '.');
	if (name) {
		memcpy(hname, path, min((int)(name - path), (int)(ASC_NAME_LEN - 1)));
		name++;
	} else {
		ASCPRT("%s: invalid path %s\n", __func__, path);
		return;
	}

	/*if reserve user, do nothing */
	if (!strncmp(name, ASC_TX_SYSFS_USER, ASC_NAME_LEN - 1) ||
	    !strncmp(name, ASC_TX_AUTO_USER, ASC_NAME_LEN - 1)) {
		ASCPRT("Can not delete reserve user %s\n", path);
		return;
	}

	tx = asc_tx_handle_lookup(hname);

	if (NULL == tx)
		return;

	user = asc_tx_user_lookup(tx, name);
	if (user) {
		/*put ready if the user had operated Tx handle */
		if (atomic_read(&user->count) > 0) {
			atomic_set(&user->count, 1);
			asc_tx_put_ready(path, 0);
		}
		spin_lock_irqsave(&tx->slock, flags);
		list_del(&user->node);
		spin_unlock_irqrestore(&tx->slock, flags);
		kfree(user);
	}
}

/**
 *asc_tx_get_ready - lock CBP to work
 *@path: (handle name).(user name)
 *@block: whether block wait for CBP has already waken
 *
 *This function try to wake the CBP and add the reference count.
 *It will block wait for CBP has already be waken if set sync parameter,
 *otherwise it just trig the action to wake CBP, which can not make sure
 *that CBP has be waken after return.
 *return 0 is ok, otherwise something error
 */
int asc_tx_get_ready(const char *path, int sync)
{
	int ret = 0;
	int try = 1;
	char hname[ASC_NAME_LEN] = { 0 };
	const char *name;
	struct asc_tx_handle *tx = NULL;
	unsigned long flags = 0;

	name = strchr(path, '.');
	if (name) {
		memcpy(hname, path, min((int)(name - path), (int)(ASC_NAME_LEN - 1)));
		name++;
	} else {
		ASCPRT("Invalid path %s\n", path);
		return -EINVAL;
	}
	tx = asc_tx_handle_lookup(hname);
	if (NULL == tx)
		return -ENODEV;

	if (!strncmp(name, ASC_TX_AUTO_USER, strlen(ASC_TX_AUTO_USER))) {
		ASCPRT("%s:tx user name %s is reserved\n", __func__, name);
		return -EINVAL;
	}

	spin_lock_irqsave(&tx->user_count_lock, flags);
	if (asc_tx_get_user(tx, name) < 0) {
		ASCPRT("%s:tx user name %s is unknown\n", __func__, name);
		spin_unlock_irqrestore(&tx->user_count_lock, flags);
		return -ENODEV;
	}

	ASCDPRT("%s: %s=%d, %s=%d\n", __func__,
		tx->cfg.name, asc_tx_refer(tx, NULL), path, asc_tx_refer(tx,
									 name));
	spin_unlock_irqrestore(&tx->user_count_lock, flags);

	switch (atomic_read(&tx->state)) {
	case AP_TX_ST_SLEEP:
		/*To make CP wake ASAP,call the function directly */
		if (!list_empty(&tx->event_q))
			asc_tx_handle_sleep(tx, AP_TX_EVENT_REQUEST);
		else
			asc_tx_event_send(tx, AP_TX_EVENT_REQUEST);

		break;
	case AP_TX_ST_IDLE:
		asc_tx_event_send(tx, AP_TX_EVENT_REQUEST);
		break;
	case AP_TX_ST_WAIT_READY:
		break;
	case AP_TX_ST_READY:
		if (atomic_read(&tx->sleeping)) {
			ASCDPRT("%s: tx state is sleeping\n", __func__);
			asc_tx_event_send(tx, AP_TX_EVENT_REQUEST);
		}
		break;
	default:
		ASCPRT("unknown tx state %d\n", atomic_read(&tx->state));
		return -EINVAL;
	}

	if (sync) {
		if ((AP_TX_ST_READY != atomic_read(&tx->state))
		    || atomic_read(&tx->sleeping)) {
			do {
				wait_event_interruptible(tx->wait_tx_state,
				(AP_TX_ST_READY == atomic_read(&tx->state))
		    && !atomic_read(&tx->sleeping));
				if (AP_TX_ST_READY == atomic_read(&tx->state)) {
					break;
				} else if (try < ASC_TX_TRY_TIMES) {
					asc_tx_event_send(tx,
							  AP_TX_EVENT_REQUEST);
					try++;
				} else {
					ret = -EBUSY;
					break;
				}
			} while (1);
		}
	}

	return ret;
}

/**
 *asc_tx_put_ready - lock CBP to work if not set auto sleep
 *@path: (config name).[user name]
 *@block: whether block wait for CBP has already waken
 *
 *This function try to wake the CBP. It will block wait for CBP
 *has already be sleep if set sync parameter, otherwise it just
 *trig the action to sleep CBP, which can not make sure that
 *CBP has be sleep after return. If the reference count is not 1. it
 *do nothing but sub one.
 *return 0 is ok, otherwise something error
 */
int asc_tx_put_ready(const char *path, int sync)
{
	int ret = 0;
	char hname[ASC_NAME_LEN] = { 0 };
	const char *name;
	struct asc_tx_handle *tx = NULL;
	unsigned long flags = 0;

	name = strchr(path, '.');
	if (name) {
		memcpy(hname, path, min((int)(name - path), (int)(ASC_NAME_LEN - 1)));
		name++;
	} else {
		ASCPRT("Invalid path %s\n", path);
		return -EINVAL;
	}

	tx = asc_tx_handle_lookup(hname);
	if (NULL == tx)
		return -ENODEV;

	spin_lock_irqsave(&tx->user_count_lock, flags);
	if (asc_tx_put_user(tx, name) < 0) {
		ASCPRT("%s:tx user name %s is unknown\n", __func__, name);
		spin_unlock_irqrestore(&tx->user_count_lock, flags);
		return -ENODEV;
	}
	ASCDPRT("%s: %s=%d, %s=%d\n", __func__,
		tx->cfg.name, asc_tx_refer(tx, NULL), path, asc_tx_refer(tx,
									 name));
	/*count is not 0, so do nothing */
	if (asc_tx_refer(tx, NULL) != 0) {
		ASCPRT("%s:asc_tx_refer user count is not 0\n", __func__);
		spin_unlock_irqrestore(&tx->user_count_lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&tx->user_count_lock, flags);

	switch (atomic_read(&tx->state)) {
	case AP_TX_ST_SLEEP:
		break;
	case AP_TX_ST_WAIT_READY:
	case AP_TX_ST_READY:
		atomic_set(&tx->sleeping, 1);
		asc_tx_event_send(tx, AP_TX_EVENT_STOP);
		break;
	case AP_TX_ST_IDLE:
		asc_tx_event_send(tx, AP_TX_EVENT_IDLE_TIMEOUT);
		break;
	default:
		ASCPRT("unknown tx state %d\n", atomic_read(&tx->state));
		return -EINVAL;
	}

	if (sync) {
		if (AP_TX_ST_SLEEP != atomic_read(&tx->state)) {
			wait_event_interruptible(tx->wait_tx_state,
			AP_TX_ST_SLEEP == atomic_read(&tx->state));
			if (AP_TX_ST_SLEEP != atomic_read(&tx->state))
				ret = -EBUSY;
		}
	}

	return ret;
}

/**
 *asc_tx_auto_ready - call each time before operate for CBP if set auto sleep
 *@name: the cofnig name for the handle
 *@sync: whether block wait for CBP has already waken
 *
 *This function try to wake the CBP and trig the tx state. It will
 *block wait for CBP has already be waken if set sync parameter,
 *otherwise it just trig the action to wake CBP, which can not make
 *sure that CBP has be waken after return.
 *return 0 is ok, otherwise something error
 */
int asc_tx_auto_ready(const char *name, int sync)
{
	int ret = 0;
	int try = 1;
	long timeout = 1;
	long cur_timeout = 0;
	struct asc_user *user;
	struct asc_tx_handle *tx;
	unsigned long flags = 0;

	if (!name) {
		ASCPRT("%s:Invalid name\n", __func__);
		return -EINVAL;
	}

	tx = asc_tx_handle_lookup(name);
	if (NULL == tx)
		return -ENODEV;

	user = asc_tx_user_lookup(tx, ASC_TX_AUTO_USER);
	if (!user)
		return -ENODEV;

	spin_lock_irqsave(&tx->user_count_lock, flags);
	if (atomic_read(&user->count) == 0) {
		ASCDPRT("%s: %s=%d, %s=%d\n", __func__,
			tx->cfg.name, asc_tx_refer(tx, NULL), ASC_TX_AUTO_USER,
			asc_tx_refer(tx, ASC_TX_AUTO_USER));
		atomic_inc(&user->count);
	}
	spin_unlock_irqrestore(&tx->user_count_lock, flags);

	switch (atomic_read(&tx->state)) {
	case AP_TX_ST_SLEEP:
		/*To make CP wake ASAP,call the function directly */
		if (!list_empty(&tx->event_q))
			asc_tx_handle_sleep(tx, AP_TX_EVENT_REQUEST);
		else
			asc_tx_event_send(tx, AP_TX_EVENT_REQUEST);

		break;
	case AP_TX_ST_IDLE:
		asc_tx_event_send(tx, AP_TX_EVENT_REQUEST);
		break;
	case AP_TX_ST_WAIT_READY:
		break;
	case AP_TX_ST_READY:
		/*NOTE: it will be concurrent while tx idle timer is running in other CPU,
		   which maybe cause message STOP behind REQUEST.
		   To avoid misleading, make sure the timer had been expired */
		for (;;) {
			ret = try_to_del_timer_sync(&tx->timer_wait_idle);
			if (ret > 0) {	/*pending timer, just trig busy */
				asc_tx_trig_busy(tx);
				break;
			} else if (ret == 0) {	/*expired timer, send REQUEST to reactive the STOP state */
				asc_tx_event_send(tx, AP_TX_EVENT_REQUEST);
				break;
			}
			/*running timer, just wait for expired */
			mdelay(1);
		}
		ret = 0;
		break;
	default:
		ASCPRT("unknown tx state %d\n", atomic_read(&tx->state));
		return -EINVAL;
	}

	if (sync) {
		if ((AP_TX_ST_READY != atomic_read(&tx->state))
		    || atomic_read(&tx->sleeping)) {
			do {
				cur_timeout = wait_event_interruptible_timeout
				    (tx->wait_tx_state, (AP_TX_ST_READY == atomic_read(&tx->state))
				    && !atomic_read(&tx->sleeping), msecs_to_jiffies(20));
				if (cur_timeout == 0)
					cur_timeout = msecs_to_jiffies(20);
				timeout += cur_timeout;
/*interruptible_sleep_on(&tx->wait_tx_state);*/
				if (AP_TX_ST_READY == atomic_read(&tx->state)) {
					if (timeout >
					    msecs_to_jiffies
					    (ASC_TX_WAIT_READY_TIME)) {
						/*unlikely,unless sleeping has some unknown bug */
						ASCDPRT
						    ("why come here . %s %s now is ready,but wait time expire\n",
						     __func__, tx->cfg.name);
						break;
					}
					if (atomic_read(&tx->sleeping)) {
						/*sleep handle is on the way,we should wait another 20ms for it done */
						ASCDPRT
						    ("%s %s sleep handle is on the way,should wait it finish\n",
						     __func__, tx->cfg.name);
						continue;
					} else {
						/*likely,cp is ready now */
						break;
					}
				} else if (try < ASC_TX_TRY_TIMES) {
					if (timeout <=
					    msecs_to_jiffies
					    (ASC_TX_WAIT_READY_TIME)) {
						/*one time of waitting cp ready's time had not expired,
						   contine to wait another 20ms */
						continue;
					}
					/*now one time of waitting cp ready time had expired,retry once more */
					asc_tx_event_send(tx,
							  AP_TX_EVENT_REQUEST);
					timeout = 0;
					try++;
				} else {
					ret = -EBUSY;
					ASCDPRT(" %s %s wait cp ready failed\n",
						__func__, tx->cfg.name);
					break;
				}
			} while (1);
		}
	}

	return ret;

}

/*inc: 1 inc, 0 dec*/
int asc_tx_ready_count(const char *name, int inc)
{
	struct asc_tx_handle *tx = NULL;
	char path[ASC_NAME_LEN] = { 0 };
	unsigned long flags = 0;

	if (!name) {
		ASCPRT("%s:Invalid name\n", __func__);
		return -EINVAL;
	}

	tx = asc_tx_handle_lookup(name);
	if (NULL == tx)
		return -ENODEV;

	spin_lock_irqsave(&tx->slock, flags);
	if (inc) {
		tx->ready_hold++;
	} else {
		tx->ready_hold--;
		if (tx->ready_hold == 0 && atomic_read(&tx->delay_sleep)) {
			atomic_set(&tx->delay_sleep, 0);
			spin_unlock_irqrestore(&tx->slock, flags);
			ASCDPRT("%s:asc_tx_put_ready for %s name\n", __func__,
			       tx->cfg.name);
			snprintf(path, ASC_NAME_LEN, "%s.%s", tx->cfg.name,
				 ASC_TX_AUTO_USER);
			asc_tx_put_ready(path, 0);
			return 0;
		}
	}
	spin_unlock_irqrestore(&tx->slock, flags);

	return 0;
}

static void asc_rx_handle_reset(struct asc_rx_handle *rx)
{
	unsigned long flags;

	ASCDPRT("%s %s\n", __func__, rx->cfg.name);
	del_timer(&rx->timer);
	wake_unlock(&rx->wlock);
	asc_rx_indicate_sleep(rx);
	atomic_set(&rx->state, AP_RX_ST_SLEEP);
	spin_lock_irqsave(&rx->slock, flags);
	INIT_LIST_HEAD(&rx->event_q);
	spin_unlock_irqrestore(&rx->slock, flags);

}

/**
 *asc_rx_reset - reset the rx handle
 *@name: the config name for the handle
 *
 *return 0 ok, others be error
 */
void asc_rx_reset(const char *name)
{
	struct asc_rx_handle *rx = NULL;

	rx = asc_rx_handle_lookup(name);
	if (rx)
		asc_rx_event_send(rx, AP_RX_EVENT_RESET);

}

/**
 *asc_rx_add_user - add a user for rx handle
 *@name: the config name for the handle
 *@infor: the user information
 *
 *return 0,  others be error
 */
int asc_rx_add_user(const char *name, struct asc_infor *infor)
{
	int ret = 0;
	unsigned long flags = 0;
	struct asc_rx_handle *rx;
	struct asc_user *user;

	rx = asc_rx_handle_lookup(name);
	if (NULL == rx)
		return -ENODEV;

	user = asc_rx_user_lookup(rx, infor->name);
	if (NULL == user) {
		user = kzalloc(sizeof(*user), GFP_KERNEL);
		if (!user) {
			ASCPRT("No memory to create new user reference.\n");
			ret = -ENOMEM;
			goto error;
		}
		user->infor.data = infor->data;
		user->infor.notifier = infor->notifier;
		strncpy(user->infor.name, infor->name, ASC_NAME_LEN - 1);
		atomic_set(&user->count, 0);
		spin_lock_irqsave(&rx->slock, flags);
		list_add_tail(&user->node, &rx->user_list);
		spin_unlock_irqrestore(&rx->slock, flags);
		if (AP_RX_ST_WAIT_READY == atomic_read(&rx->state)) {
			if (infor->notifier) {
				infor->notifier(ASC_NTF_RX_PREPARE,
						infor->data);
			}
		}
	} else {
		ASCPRT("%s error: user %s already exist!!\n", __func__,
		       infor->name);
		ret = -EINVAL;
	}
 error:
	return ret;
}

/**
 *asc_rx_del_user - add a user for rx handle
 *@path: (config name).[user name]
 *
 *no return
 */
void asc_rx_del_user(const char *path)
{
	unsigned long flags = 0;
	const char *name;
	char hname[ASC_NAME_LEN] = { 0 };
	struct asc_user *user;
	struct asc_rx_handle *rx;

	name = strchr(path, '.');
	if (name) {
		memcpy(hname, path, min((int)(name - path), (int)(ASC_NAME_LEN - 1)));
		name++;
	} else {
		ASCPRT("%s: Invalid path %s\n", __func__, path);
		return;
	}

	rx = asc_rx_handle_lookup(hname);
	if (NULL == rx)
		return;

	user = asc_rx_user_lookup(rx, name);
	if (user) {
		atomic_set(&user->count, 0);
		spin_lock_irqsave(&rx->slock, flags);
		list_del(&user->node);
		spin_unlock_irqrestore(&rx->slock, flags);
		kfree(user);
		if (list_empty(&rx->user_list))
			asc_rx_handle_reset(rx);
	}
}

/**
 *asc_rx_confirm_ready - echo AP state to rx CBP data
 *@name: the config name to  rx handle
 *@ready: whether AP has been ready to rx data
 *
 *After CBP request AP to rx data, the function can be used to
 *tell CBP whether AP has been ready to receive.
 *return 0 is ok, otherwise something error
 */
int asc_rx_confirm_ready(const char *name, int ready)
{
	struct asc_rx_handle *rx = NULL;

	rx = asc_rx_handle_lookup(name);
	if (!rx) {
		ASCDPRT("%s: name %s is unknown\n", __func__, name);
		return -ENODEV;
	}

	ASCDPRT("Rx(%s) confirm ready=%d\n", rx->cfg.name, ready);
	return asc_rx_event_send(rx,
				 ready ? AP_RX_EVENT_AP_READY :
				 AP_RX_EVENT_AP_UNREADY);
}

/**
 *check_on_start - prevent from missing cp waking
 *@name: the name of rx handle
 *
 *Before the rx user is registed,CP may wake up AP.Usually AP will ignore
 *the waking.Because the interrupt don't be register at that time.So the
 *signal will be missed.When opening the tty device, we should check whether CP
 *has waken up AP or not.If CP did that,we should send the signal "AP READY"
 *to CP.
 */

int asc_rx_check_on_start(const char *name)
{
	int level;
	struct asc_config *cfg = NULL;
	struct asc_rx_handle *rx = NULL;
	int ret = 1;

	rx = asc_rx_handle_lookup(name);
	if (!rx) {
		ASCPRT("config %s has not already exist.\n", name);
		return -EINVAL;
	}

	cfg = &(rx->cfg);
	level = !!c2k_gpio_get_value(cfg->gpio_wake);
	if (level == cfg->polar) {
		/*Cp has requested Ap wake */
		if (AP_RX_ST_SLEEP == atomic_read(&rx->state)) {
			ASCDPRT
			    ("Rx(%s):check_on_start--send event AP_RX_EVENT_REQUEST.\n",
			     cfg->name);
			ret = asc_rx_event_send(rx, AP_RX_EVENT_REQUEST);
		} else {
			ASCDPRT
			    ("Rx(%s): check_on_start--send event AP_RX_EVENT_AP_READY.\n",
			     cfg->name);
			ret = asc_rx_event_send(rx, AP_RX_EVENT_AP_READY);
		}
	}

	return ret;
}

static ssize_t asc_debug_show(struct kobject *kobj, struct kobj_attribute *attr,
			      char *buf)
{
	char *s = buf;

	s += snprintf(s, 32, "%d\n", asc_debug);

	return s - buf;
}

static ssize_t asc_debug_store(struct kobject *kobj,
			       struct kobj_attribute *attr, const char *buf,
			       size_t n)
{
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		return -EINVAL;

	asc_debug = val;

	return n;
}

static ssize_t asc_infor_show(struct kobject *kobj, struct kobj_attribute *attr,
			      char *buf)
{
	char *s = buf;
	int val1, val2;
	struct asc_config *cfg;
	struct asc_infor *infor;
	struct asc_user *user = NULL, *tuser = NULL;
	struct asc_tx_handle *tx = NULL, *ttmp = NULL;
	struct asc_rx_handle *rx = NULL, *rtmp = NULL;

	list_for_each_entry_safe(tx, ttmp, &asc_tx_handle_list, node) {
		cfg = &tx->cfg;
		val1 = val2 = -1;
		/*if(cfg->gpio_wake >= 0) */
		val1 = !!c2k_gpio_get_value(cfg->gpio_wake);
		/*if(cfg->gpio_ready >= 0) */
		val2 = !!c2k_gpio_get_value(cfg->gpio_ready);

		s += snprintf(s, 256,
			     "Tx %s: ref=%d, ap_wake_cp(%d)=%d, cp_ready(%d)=%d, polar=%d, auto_delay=%d mS\n",
			     cfg->name, asc_tx_refer(tx, NULL), cfg->gpio_wake,
			     val1, cfg->gpio_ready, val2, cfg->polar,
			     tx->auto_delay);

		list_for_each_entry_safe(user, tuser, &tx->user_list, node) {
			infor = &user->infor;
			s += snprintf(s, 256, "       user %s: ref=%d\n", infor->name,
				     atomic_read(&user->count));
		}
	}

	s += snprintf(s, 32, "\n");

	list_for_each_entry_safe(rx, rtmp, &asc_rx_handle_list, node) {
		cfg = &rx->cfg;
		val1 = val2 = -1;
		/*if(cfg->gpio_wake >= 0) */
		val1 = !!c2k_gpio_get_value(cfg->gpio_wake);
		/*if(cfg->gpio_ready >= 0) */
		val2 = !!c2k_gpio_get_value(cfg->gpio_ready);

		s += snprintf(s, 256,
			     "Rx %s: ref=%d, cp_wake_ap(%d)=%d, ap_ready(%d)=%d, polar=%d\n",
			     cfg->name, asc_rx_refer(rx, NULL), cfg->gpio_wake,
			     val1, cfg->gpio_ready, val2, cfg->polar);
		list_for_each_entry_safe(user, tuser, &rx->user_list, node) {
			infor = &user->infor;
			s += snprintf(s, 256, "       user %s: ref=%d\n", infor->name,
				     atomic_read(&user->count));
		}
	}

	return s - buf;
}

static ssize_t asc_infor_store(struct kobject *kobj,
			       struct kobj_attribute *attr, const char *buf,
			       size_t n)
{
	return n;
}

static ssize_t asc_refer_show(struct kobject *kobj, struct kobj_attribute *attr,
			      char *buf)
{
	unsigned long flags;
	char *s = buf;
	struct asc_tx_handle *tx, *tmp, *t;

	tx = tmp = NULL;
	spin_lock_irqsave(&hdlock, flags);
	list_for_each_entry_safe(tmp, t, &asc_tx_handle_list, node) {
		if (tmp->kobj == kobj) {
			tx = tmp;
			break;
		}
	}
	spin_unlock_irqrestore(&hdlock, flags);

	if (tx) {
		s += snprintf(s, 32, "%d\n", asc_tx_refer(tx, ASC_TX_SYSFS_USER));
		return s - buf;
	}
	ASCPRT("%s read error\n", __func__);
	return -EINVAL;
}

static ssize_t asc_refer_store(struct kobject *kobj,
			       struct kobj_attribute *attr, const char *buf,
			       size_t n)
{
	unsigned long flags;
	char *p;
	int error = 0, len;
	char path[ASC_NAME_LEN] = { 0 };
	struct asc_tx_handle *tx, *tmp, *t;

	tx = tmp = NULL;
	spin_lock_irqsave(&hdlock, flags);
	list_for_each_entry_safe(tmp, t, &asc_tx_handle_list, node) {
		if (tmp->kobj == kobj) {
			tx = tmp;
			break;
		}
	}
	spin_unlock_irqrestore(&hdlock, flags);

	if (tx) {
		p = memchr(buf, '\n', n);
		len = p ? p - buf : n;
		snprintf(path, ASC_NAME_LEN, "%s.%s", tx->cfg.name,
			 ASC_TX_SYSFS_USER);

		if (len == 3 && !strncmp(buf, "get", len))
			error = asc_tx_get_ready(path, 1);
		else if (len == 3 && !strncmp(buf, "put", len))
			error = asc_tx_put_ready(path, 1);
	}

	return error ? error : n;
}

static ssize_t asc_state_show(struct kobject *kobj, struct kobj_attribute *attr,
			      char *buf)
{
	unsigned long flags;
	char *s = buf;
	struct asc_tx_handle *tx, *tmp, *t;

	tx = tmp = NULL;
	spin_lock_irqsave(&hdlock, flags);
	list_for_each_entry_safe(tmp, t, &asc_tx_handle_list, node) {
		if (tmp->kobj == kobj) {
			tx = tmp;
			break;
		}
	}
	spin_unlock_irqrestore(&hdlock, flags);

	if (tx) {
		s += snprintf(s, ASC_NAME_LEN, "%s\n",
			     tx->table[atomic_read(&tx->state)].name);
		return s - buf;
	}
	ASCPRT("%s read error\n", __func__);
	return -EINVAL;
}

static ssize_t asc_state_store(struct kobject *kobj,
			       struct kobj_attribute *attr, const char *buf,
			       size_t n)
{
	return n;
}

static ssize_t asc_auto_ready_show(struct kobject *kobj,
				   struct kobj_attribute *attr, char *buf)
{
	unsigned long flags;
	char *s = buf;
	struct asc_tx_handle *tx, *tmp, *t;

	tx = tmp = NULL;
	spin_lock_irqsave(&hdlock, flags);
	list_for_each_entry_safe(tmp, t, &asc_tx_handle_list, node) {
		if (tmp->kobj == kobj) {
			tx = tmp;
			break;
		}
	}
	spin_unlock_irqrestore(&hdlock, flags);

	if (tx) {
		s += snprintf(s, 32, "%d\n", tx->auto_delay);
		return s - buf;
	}
	ASCPRT("%s read error\n", __func__);
	return -EINVAL;
}

static ssize_t asc_auto_ready_store(struct kobject *kobj,
				    struct kobj_attribute *attr,
				    const char *buf, size_t n)
{
	int error = 0;
	long val;
	unsigned long flags;
	struct asc_tx_handle *tx, *tmp, *t;

	tx = tmp = NULL;
	spin_lock_irqsave(&hdlock, flags);
	list_for_each_entry_safe(tmp, t, &asc_tx_handle_list, node) {
		if (tmp->kobj == kobj) {
			tx = tmp;
			break;
		}
	}
	spin_unlock_irqrestore(&hdlock, flags);

	if (tx) {
		error = kstrtol(buf, 10, &val);
		if (error || (val < 0)) {
			error = -EINVAL;
			goto end;
		}

		if (val > 0) {
			spin_lock_irqsave(&tx->slock, flags);
			tx->auto_delay = val;
			spin_unlock_irqrestore(&tx->slock, flags);
		}
		error = asc_tx_auto_ready(tx->cfg.name, 1);
	} else {
		ASCPRT("%s read error\n", __func__);
		error = -EINVAL;
	}

 end:
	return error ? error : n;
}

static ssize_t asc_confirm_ready_show(struct kobject *kobj,
				      struct kobj_attribute *attr, char *buf)
{
	unsigned long flags;
	char *s = buf;
	struct asc_rx_handle *rx, *tmp, *t;

	rx = tmp = NULL;
	spin_lock_irqsave(&hdlock, flags);
	list_for_each_entry_safe(tmp, t, &asc_rx_handle_list, node) {
		if (tmp->kobj == kobj) {
			rx = tmp;
			break;
		}
	}
	spin_unlock_irqrestore(&hdlock, flags);

	if (rx)
		s += snprintf(s, 32, "done\n");
	else
		s += snprintf(s, 32, "null\n");

	return s - buf;
}

static ssize_t asc_confirm_ready_store(struct kobject *kobj,
				       struct kobj_attribute *attr,
				       const char *buf, size_t n)
{
	int error = 0;
	long val;
	unsigned long flags;
	struct asc_rx_handle *rx, *tmp, *t;

	rx = tmp = NULL;
	spin_lock_irqsave(&hdlock, flags);
	list_for_each_entry_safe(tmp, t, &asc_rx_handle_list, node) {
		if (tmp->kobj == kobj) {
			rx = tmp;
			break;
		}
	}
	spin_unlock_irqrestore(&hdlock, flags);

	if (rx) {
		error = kstrtol(buf, 10, &val);
		if (error || (val < 0)) {
			error = -EINVAL;
			goto end;
		}

		error = asc_rx_confirm_ready(rx->cfg.name, !!val);
	} else {
		ASCPRT("%s read error\n", __func__);
		error = -EINVAL;
	}

 end:
	return error ? error : n;
}

#define asc_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0644,			\
	},					\
	.show	= asc_##_name##_show,			\
	.store	= asc_##_name##_store,		\
}

asc_attr(debug);
asc_attr(infor);

static struct attribute *g_attr[] = {
	&debug_attr.attr,
	&infor_attr.attr,
	NULL,
};

static struct attribute_group g_attr_group = {
	.attrs = g_attr,
};

asc_attr(refer);
asc_attr(state);
asc_attr(auto_ready);

static struct attribute *tx_hd_attr[] = {
	&refer_attr.attr,
	&state_attr.attr,
	&auto_ready_attr.attr,
	NULL,
};

static struct attribute_group tx_hd_attr_group = {
	.attrs = tx_hd_attr,
};

asc_attr(confirm_ready);

static struct attribute *rx_hd_attr[] = {
	&confirm_ready_attr.attr,
	NULL,
};

static struct attribute_group rx_hd_attr_group = {
	.attrs = rx_hd_attr,
};

static struct platform_driver asc_driver = {
	.driver.name = "asc",
};

static struct platform_device asc_device = {
	.name = "asc",
};

/**
 *asc_rx_register_handle - register the rx handle
 *@cfg: the config for the handle
 *
 *the device which receive data from CBP can register a notifier to
 *listen the event according to the changes from CBP.
 *ASC_PREPARE_RX_DATA event will be send when CBP start tx data
 *to the device which must be ready to work;
 *ASC_POST_RX_DATA event will be send when CBP stop tx data to
 *the device which can go to sleep.
 *The gpio for ap_ready can be -1 which be ignored when the device
 *can receive the data from CBP correctly any time.
 *return index according to the notifier in handle, otherwise something error
 */
int asc_rx_register_handle(struct asc_config *cfg)
{
	int ret = 0;
	unsigned long flags;
	struct asc_rx_handle *rx = NULL;

	if (NULL == asc_work_queue) {
		ASCPRT("%s: error Asc has not been init\n", __func__);
		return -EINVAL;
	}

	if (NULL == cfg)
		return -EINVAL;

	rx = asc_rx_handle_lookup(cfg->name);
	if (rx) {
		ASCPRT("config %s has already exist.\n", cfg->name);
		return -EINVAL;
	}

	rx = kzalloc(sizeof(struct asc_rx_handle), GFP_KERNEL);
	if (NULL == rx) {
		ASCPRT("No memory to alloc rx handle.\n");
		return -ENOMEM;
	}

	rx->cfg.gpio_ready = cfg->gpio_ready;
	rx->cfg.gpio_wake = cfg->gpio_wake;
	rx->cfg.polar = !!cfg->polar;
	strncpy(rx->cfg.name, cfg->name, ASC_NAME_LEN - 1);

	ret = asc_rx_handle_init(rx);
	if (ret < 0) {
		ASCPRT("fail to init rx handle %s\n", rx->cfg.name);
		kfree(rx);
		return -EINVAL;
	}

	rx->kobj = kobject_create_and_add(cfg->name, asc_kobj);
	if (!rx->kobj) {
		ASCPRT("fail to create rx handle %s kobject\n", rx->cfg.name);
		kfree(rx);
		return -ENOMEM;
	}
	/*Add the handle to the asc list */
	spin_lock_irqsave(&hdlock, flags);
	list_add(&rx->node, &asc_rx_handle_list);
	spin_unlock_irqrestore(&hdlock, flags);
	ASCDPRT("Register rx handle %s\n", rx->cfg.name);

	return sysfs_create_group(rx->kobj, &rx_hd_attr_group);
}

/**
 *asc_tx_register_handle - register the tx handle for state change
 *@cfg: the config for the handle
 *
 *the chip which exchanged data with CBP must create a handle.
 *There is only one tx state handle between the AP and CBP because
 *all devices in CBP will be ready to receive data after CBP has been
 *waken. But servial rx state handles can be exist because different
 *devices in AP maybe waken indivially. Each rx state handle must be
 *registed a notifier to listen the evnets.
 *return 0 is ok, otherwise something error
 */
int asc_tx_register_handle(struct asc_config *cfg)
{
	int ret = 0;
	unsigned long flags;
	struct asc_infor infor;
	struct asc_tx_handle *tx = NULL;

	if (NULL == asc_work_queue) {
		ASCPRT("%s: error Asc has not been init\n", __func__);
		return -EINVAL;
	}

	if (NULL == cfg)
		return -EINVAL;

	tx = asc_tx_handle_lookup(cfg->name);
	if (tx) {
		ASCPRT("config %s has already exist.\n", cfg->name);
		return -EINVAL;
	}

	tx = kzalloc(sizeof(struct asc_tx_handle), GFP_KERNEL);
	if (NULL == tx) {
		ASCPRT("Fail to alloc memory for tx handle.\n");
		return -ENOMEM;
	}

	tx->cfg.gpio_ready = cfg->gpio_ready;
	tx->cfg.gpio_wake = cfg->gpio_wake;
	tx->cfg.polar = !!cfg->polar;
	strncpy(tx->cfg.name, cfg->name, ASC_NAME_LEN - 1);
	ret = asc_tx_handle_init(tx);
	if (ret < 0) {
		ASCPRT("Fail to init tx handle %s.\n", tx->cfg.name);
		goto err_tx_handle_init;
	}

	/*Add the handle to the asc list */
	spin_lock_irqsave(&hdlock, flags);
	list_add(&tx->node, &asc_tx_handle_list);
	spin_unlock_irqrestore(&hdlock, flags);
	ASCDPRT("Register tx handle %s.\n", tx->cfg.name);

	tx->kobj = kobject_create_and_add(cfg->name, asc_kobj);
	if (!tx->kobj) {
		ret = -ENOMEM;
		goto err_create_kobj;
	}

	/*add default user for application */
	memset(&infor, 0, sizeof(infor));
	strncpy(infor.name, ASC_TX_SYSFS_USER, ASC_NAME_LEN);
	asc_tx_add_user(tx->cfg.name, &infor);
	memset(&infor, 0, sizeof(infor));
	strncpy(infor.name, ASC_TX_AUTO_USER, ASC_NAME_LEN);
	asc_tx_add_user(tx->cfg.name, &infor);
	return sysfs_create_group(tx->kobj, &tx_hd_attr_group);

 err_create_kobj:
	list_del(&tx->node);
 err_tx_handle_init:
	kfree(tx);

	return ret;
}

static int __init asc_init(void)
{
	int ret;

	ret = platform_device_register(&asc_device);
	if (ret) {
		ASCPRT("platform_device_register failed\n");
		goto err_platform_device_register;
	}
	ret = platform_driver_register(&asc_driver);
	if (ret) {
		ASCPRT("platform_driver_register failed\n");
		goto err_platform_driver_register;
	}

	asc_work_queue = create_singlethread_workqueue("asc_work");
	if (asc_work_queue == NULL) {
		ret = -ENOMEM;
		goto err_create_work_queue;
	}

	asc_kobj = c2k_kobject_add("asc");
	if (!asc_kobj) {
		ret = -ENOMEM;
		goto err_create_kobj;
	}

	return sysfs_create_group(asc_kobj, &g_attr_group);

 err_create_kobj:
	destroy_workqueue(asc_work_queue);
 err_create_work_queue:
	platform_driver_unregister(&asc_driver);
 err_platform_driver_register:
	platform_device_unregister(&asc_device);
 err_platform_device_register:
	return ret;
}

device_initcall(asc_init);
