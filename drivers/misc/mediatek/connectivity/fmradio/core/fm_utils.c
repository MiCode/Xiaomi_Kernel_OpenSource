/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/delay.h>

#include "fm_typedef.h"
#include "fm_dbg.h"
#include "fm_err.h"
#include "fm_stdlib.h"
#include "fm_utils.h"

signed int fm_delayms(unsigned int data)
{
	WCN_DBG(FM_DBG | CHIP, "delay %dms\n", data);
	msleep(data);
	return 0;
}

signed int fm_delayus(unsigned int data)
{
	WCN_DBG(FM_DBG | CHIP, "delay %dus\n", data);
	udelay(data);
	return 0;
}

static unsigned int fm_event_send(struct fm_flag_event *thiz, unsigned int mask)
{
	thiz->flag |= mask;
	/* WCN_DBG(FM_DBG|MAIN, "%s set 0x%08x\n", thiz->name, thiz->flag); */
	wake_up((wait_queue_head_t *) (thiz->priv));

	return thiz->flag;
}

static signed int fm_event_wait(struct fm_flag_event *thiz, unsigned int mask)
{
	return wait_event_interruptible(*(wait_queue_head_t *) (thiz->priv), ((thiz->flag & mask) == mask));
}

/**
 * fm_event_check - sleep until a condition gets true or a timeout elapses
 * @thiz: the pointer of current object
 * @mask: bitmap in unsigned int
 * @timeout: timeout, in jiffies
 *
 * fm_event_set() has to be called after changing any variable that could
 * change the result of the wait condition.
 *
 * The function returns 0 if the @timeout elapsed, and the remaining
 * jiffies if the condition evaluated to true before the timeout elapsed.
 */
long fm_event_wait_timeout(struct fm_flag_event *thiz, unsigned int mask, long timeout)
{
	return wait_event_timeout(*((wait_queue_head_t *) (thiz->priv)), ((thiz->flag & mask) == mask), timeout * HZ);
}

static unsigned int fm_event_clr(struct fm_flag_event *thiz, unsigned int mask)
{
	thiz->flag &= ~mask;
	/* WCN_DBG(FM_DBG|MAIN, "%s clr 0x%08x\n", thiz->name, thiz->flag); */
	return thiz->flag;
}

static unsigned int fm_event_get(struct fm_flag_event *thiz)
{
	return thiz->flag;

}

static unsigned int fm_event_rst(struct fm_flag_event *thiz)
{
	return thiz->flag = 0;
}

struct fm_flag_event *fm_flag_event_create(const signed char *name)
{
	struct fm_flag_event *tmp;
	wait_queue_head_t *wq;

	tmp = fm_zalloc(sizeof(struct fm_flag_event));
	if (!tmp) {
		WCN_DBG(FM_ALT | MAIN, "fm_zalloc(fm_event) -ENOMEM\n");
		return NULL;
	}

	wq = fm_zalloc(sizeof(wait_queue_head_t));
	if (!wq) {
		WCN_DBG(FM_ALT | MAIN, "fm_zalloc(wait_queue_head_t) -ENOMEM\n");
		fm_free(tmp);
		return NULL;
	}

	fm_memcpy(tmp->name, name, (strlen(name) > FM_NAME_MAX) ? (FM_NAME_MAX) : (strlen(name)));
	tmp->priv = wq;
	init_waitqueue_head(wq);
	tmp->ref = 0;

	tmp->send = fm_event_send;
	tmp->wait = fm_event_wait;
	tmp->wait_timeout = fm_event_wait_timeout;
	tmp->clr = fm_event_clr;
	tmp->get = fm_event_get;
	tmp->rst = fm_event_rst;

	tmp->rst(tmp);		/* set flag to 0x00000000 */

	return tmp;
}

signed int fm_flag_event_get(struct fm_flag_event *thiz)
{
	if (thiz == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	thiz->ref++;
	return 0;
}

signed int fm_flag_event_put(struct fm_flag_event *thiz)
{
	if (thiz == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	thiz->ref--;

	if (thiz->ref == 0) {
		fm_free(thiz->priv);
		fm_free(thiz);
		return 0;
	} else if (thiz->ref > 0) {
		return -FM_EINUSE;
	} else {
		return -FM_EPARA;
	}
}

/* fm lock methods */
static signed int fm_lock_try(struct fm_lock *thiz, signed int retryCnt)
{
	signed int retry_cnt = 0;
	struct semaphore *sem;
	struct task_struct *task = current;

	if (thiz == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (thiz->priv == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	while (down_trylock((struct semaphore *)thiz->priv)) {
		WCN_DBG(FM_WAR | MAIN, "down_trylock failed\n");
		if (++retry_cnt < retryCnt) {
			WCN_DBG(FM_WAR | MAIN, "[retryCnt=%d]\n", retry_cnt);
			msleep_interruptible(50);
			continue;
		} else {
			WCN_DBG(FM_CRT | MAIN, "down_trylock retry failed\n");
			return -FM_ELOCK;
		}
	}

	sem = (struct semaphore *)thiz->priv;
	WCN_DBG(FM_NTC | MAIN, "%s --->trylock, cnt=%d, pid=%d\n", thiz->name, (int)sem->count, task->pid);
	return 0;
}

/* fm try lock methods */
static signed int fm_lock_lock(struct fm_lock *thiz)
{
	struct semaphore *sem;
	struct task_struct *task = current;

	if (thiz == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (thiz->priv == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (down_interruptible((struct semaphore *)thiz->priv)) {
		WCN_DBG(FM_CRT | MAIN, "get mutex failed\n");
		return -FM_ELOCK;
	}

	sem = (struct semaphore *)thiz->priv;
	WCN_DBG(FM_DBG | MAIN, "%s --->lock, cnt=%d, pid=%d\n",
	    thiz->name, (int)sem->count, task->pid);
	return 0;
}

static signed int fm_lock_unlock(struct fm_lock *thiz)
{
	struct semaphore *sem;
	struct task_struct *task = current;

	if (thiz == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (thiz->priv == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	sem = (struct semaphore *)thiz->priv;
	WCN_DBG(FM_DBG | MAIN, "%s <---unlock, cnt=%d, pid=%d\n",
	    thiz->name, (int)sem->count + 1, task->pid);
	up((struct semaphore *)thiz->priv);
	return 0;
}

struct fm_lock *fm_lock_create(const signed char *name)
{
	struct fm_lock *tmp;
	struct semaphore *mutex;

	tmp = fm_zalloc(sizeof(struct fm_lock));
	if (!tmp) {
		WCN_DBG(FM_ALT | MAIN, "fm_zalloc(fm_lock) -ENOMEM\n");
		return NULL;
	}

	mutex = fm_zalloc(sizeof(struct semaphore));
	if (!mutex) {
		WCN_DBG(FM_ALT | MAIN, "fm_zalloc(struct semaphore) -ENOMEM\n");
		fm_free(tmp);
		return NULL;
	}

	tmp->priv = mutex;
	sema_init(mutex, 1);
	tmp->ref = 0;
	fm_memcpy(tmp->name, name, (strlen(name) > FM_NAME_MAX) ? (FM_NAME_MAX) : (strlen(name)));

	tmp->lock = fm_lock_lock;
	tmp->trylock = fm_lock_try;
	tmp->unlock = fm_lock_unlock;

	return tmp;
}

signed int fm_lock_get(struct fm_lock *thiz)
{
	if (thiz == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	thiz->ref++;
	return 0;
}

signed int fm_lock_put(struct fm_lock *thiz)
{
	if (thiz == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	thiz->ref--;

	if (thiz->ref == 0) {
		fm_free(thiz->priv);
		fm_free(thiz);
		return 0;
	} else if (thiz->ref > 0) {
		return -FM_EINUSE;
	} else {
		return -FM_EPARA;
	}
}

/* fm lock methods */
static signed int fm_spin_lock_lock(struct fm_lock *thiz)
{
	struct task_struct *task = current;

	if (thiz == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (thiz->priv == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	spin_lock_bh((spinlock_t *) thiz->priv);

	WCN_DBG(FM_DBG | MAIN, "%s --->lock pid=%d\n", thiz->name, task->pid);
	return 0;
}

static signed int fm_spin_lock_unlock(struct fm_lock *thiz)
{
	struct task_struct *task = current;

	if (thiz == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (thiz->priv == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	WCN_DBG(FM_DBG | MAIN, "%s <---unlock, pid=%d\n", thiz->name, task->pid);
	spin_unlock_bh((spinlock_t *) thiz->priv);
	return 0;
}

struct fm_lock *fm_spin_lock_create(const signed char *name)
{
	struct fm_lock *tmp;
	spinlock_t *spin_lock;

	tmp = fm_zalloc(sizeof(struct fm_lock));
	if (!tmp) {
		WCN_DBG(FM_ALT | MAIN, "fm_zalloc(fm_lock) -ENOMEM\n");
		return NULL;
	}

	spin_lock = fm_zalloc(sizeof(spinlock_t));
	if (!spin_lock) {
		WCN_DBG(FM_ALT | MAIN, "fm_zalloc(spinlock_t) -ENOMEM\n");
		fm_free(tmp);
		return NULL;
	}

	tmp->priv = spin_lock;
	spin_lock_init(spin_lock);
	tmp->ref = 0;
	fm_memcpy(tmp->name, name, (strlen(name) > FM_NAME_MAX) ? (FM_NAME_MAX) : (strlen(name)));

	tmp->lock = fm_spin_lock_lock;
	tmp->unlock = fm_spin_lock_unlock;

	return tmp;
}

signed int fm_spin_lock_get(struct fm_lock *thiz)
{
	if (thiz == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	thiz->ref++;
	return 0;
}

signed int fm_spin_lock_put(struct fm_lock *thiz)
{
	if (thiz == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	thiz->ref--;

	if (thiz->ref == 0) {
		fm_free(thiz->priv);
		fm_free(thiz);
		return 0;
	} else if (thiz->ref > 0) {
		return -FM_EINUSE;
	} else {
		return -FM_EPARA;
	}
}

/*
 * fm timer
 *
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0))
static signed int fm_timer_init(struct fm_timer *thiz, void (*timeout) (struct timer_list *timer),
			    unsigned long data, signed long time, signed int flag)
#else
static signed int fm_timer_init(struct fm_timer *thiz, void (*timeout) (unsigned long data),
			    unsigned long data, signed long time, signed int flag)
#endif
{
	struct timer_list *timerlist = (struct timer_list *)thiz->priv;

	thiz->flag = flag;
	thiz->flag &= ~FM_TIMER_FLAG_ACTIVATED;
	thiz->timeout_func = timeout;
	thiz->data = data;
	thiz->timeout_ms = time;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0))
	timer_setup(timerlist, thiz->timeout_func, 0);
#else
	init_timer(timerlist);
	timerlist->function = thiz->timeout_func;
	timerlist->data = (unsigned long)thiz->data;
#endif
	timerlist->expires = jiffies + (thiz->timeout_ms) / (1000 / HZ);

	return 0;
}

static signed int fm_timer_start(struct fm_timer *thiz)
{
	struct timer_list *timerlist = (struct timer_list *)thiz->priv;

	thiz->flag |= FM_TIMER_FLAG_ACTIVATED;
	mod_timer(timerlist, jiffies + (thiz->timeout_ms) / (1000 / HZ));

	return 0;
}

static signed int fm_timer_update(struct fm_timer *thiz)
{
	struct timer_list *timerlist = (struct timer_list *)thiz->priv;

	if (thiz->flag & FM_TIMER_FLAG_ACTIVATED) {
		mod_timer(timerlist, jiffies + (thiz->timeout_ms) / (1000 / HZ));
		return 0;
	} else {
		return 1;
	}
}

static signed int fm_timer_stop(struct fm_timer *thiz)
{
	struct timer_list *timerlist = (struct timer_list *)thiz->priv;

	thiz->flag &= ~FM_TIMER_FLAG_ACTIVATED;
	del_timer(timerlist);

	return 0;
}

static signed int fm_timer_control(struct fm_timer *thiz, enum fm_timer_ctrl cmd, void *arg)
{

	return 0;
}

struct fm_timer *fm_timer_create(const signed char *name)
{
	struct fm_timer *tmp;
	struct timer_list *timerlist;

	tmp = fm_zalloc(sizeof(struct fm_timer));
	if (!tmp) {
		WCN_DBG(FM_ALT | MAIN, "fm_zalloc(fm_timer) -ENOMEM\n");
		return NULL;
	}

	timerlist = fm_zalloc(sizeof(struct timer_list));
	if (!timerlist) {
		WCN_DBG(FM_ALT | MAIN, "fm_zalloc(struct timer_list) -ENOMEM\n");
		fm_free(tmp);
		return NULL;
	}

	fm_memcpy(tmp->name, name, (strlen(name) > FM_NAME_MAX) ? (FM_NAME_MAX) : (strlen(name)));
	tmp->priv = timerlist;
	tmp->ref = 0;
	tmp->init = fm_timer_init;
	tmp->start = fm_timer_start;
	tmp->stop = fm_timer_stop;
	tmp->update = fm_timer_update;
	tmp->control = fm_timer_control;

	return tmp;
}

signed int fm_timer_get(struct fm_timer *thiz)
{
	if (thiz == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	thiz->ref++;
	return 0;
}

signed int fm_timer_put(struct fm_timer *thiz)
{
	if (thiz == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	thiz->ref--;

	if (thiz->ref == 0) {
		fm_free(thiz->priv);
		fm_free(thiz);
		return 0;
	} else if (thiz->ref > 0) {
		return -FM_EINUSE;
	} else {
		return -FM_EPARA;
	}
}

/*
 * FM work thread mechanism
 */
static signed int fm_work_init(struct fm_work *thiz, work_func_t work_func, unsigned long data)
{
	struct work_struct *sys_work = (struct work_struct *)thiz->priv;
	work_func_t func;

	thiz->work_func = work_func;
	thiz->data = data;
	func = (work_func_t) thiz->work_func;

	INIT_WORK(sys_work, func);

	return 0;

}

struct fm_work *fm_work_create(const signed char *name)
{
	struct fm_work *my_work;
	struct work_struct *sys_work;

	my_work = fm_zalloc(sizeof(struct fm_work));
	if (!my_work) {
		WCN_DBG(FM_ALT | MAIN, "fm_zalloc(fm_work) -ENOMEM\n");
		return NULL;
	}

	sys_work = fm_zalloc(sizeof(struct work_struct));
	if (!sys_work) {
		WCN_DBG(FM_ALT | MAIN, "fm_zalloc(struct work_struct) -ENOMEM\n");
		fm_free(my_work);
		return NULL;
	}

	fm_memcpy(my_work->name, name, (strlen(name) > FM_NAME_MAX) ? (FM_NAME_MAX) : (strlen(name)));
	my_work->priv = sys_work;
	my_work->init = fm_work_init;

	return my_work;
}

signed int fm_work_get(struct fm_work *thiz)
{
	if (thiz == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	thiz->ref++;
	return 0;
}

signed int fm_work_put(struct fm_work *thiz)
{
	if (thiz == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	thiz->ref--;

	if (thiz->ref == 0) {
		fm_free(thiz->priv);
		fm_free(thiz);
		return 0;
	} else if (thiz->ref > 0) {
		return -FM_EINUSE;
	} else {
		return -FM_EPARA;
	}
}

static signed int fm_workthread_add_work(struct fm_workthread *thiz, struct fm_work *work)
{
	if (thiz == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (work == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	queue_work((struct workqueue_struct *)thiz->priv, (struct work_struct *)work->priv);
	return 0;
}

struct fm_workthread *fm_workthread_create(const signed char *name)
{
	struct fm_workthread *my_thread;
	struct workqueue_struct *sys_thread;

	my_thread = fm_zalloc(sizeof(struct fm_workthread));
	if (!my_thread) {
		WCN_DBG(FM_ALT | MAIN, "fm_zalloc(fm_workthread) -ENOMEM\n");
		return NULL;
	}

	sys_thread = create_singlethread_workqueue(name);

	fm_memcpy(my_thread->name, name, (strlen(name) > FM_NAME_MAX) ? (FM_NAME_MAX) : (strlen(name)));
	my_thread->priv = sys_thread;
	my_thread->add_work = fm_workthread_add_work;

	return my_thread;
}

signed int fm_workthread_get(struct fm_workthread *thiz)
{
	if (thiz == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	thiz->ref++;
	return 0;
}

signed int fm_workthread_put(struct fm_workthread *thiz)
{
	if (thiz == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	thiz->ref--;

	if (thiz->ref == 0) {
		destroy_workqueue((struct workqueue_struct *)thiz->priv);
		fm_free(thiz);
		return 0;
	} else if (thiz->ref > 0) {
		return -FM_EINUSE;
	} else {
		return -FM_EPARA;
	}
}

signed int fm_fifo_in(struct fm_fifo *thiz, void *item)
{
	if (item == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (thiz->len < thiz->size) {
		fm_memcpy((thiz->obj.priv + (thiz->item_size * thiz->in)), item, thiz->item_size);
		thiz->in = (thiz->in + 1) % thiz->size;
		thiz->len++;
		/* WCN_DBG(FM_DBG | MAIN, "add a new item[len=%d]\n", thiz->len); */
	} else {
		WCN_DBG(FM_WAR | MAIN, "%s fifo is full\n", thiz->obj.name);
		return -FM_ENOMEM;
	}

	return 0;
}

signed int fm_fifo_out(struct fm_fifo *thiz, void *item)
{
	if (thiz->len > 0) {
		if (item) {
			fm_memcpy(item, (thiz->obj.priv + (thiz->item_size * thiz->out)), thiz->item_size);
			fm_memset((thiz->obj.priv + (thiz->item_size * thiz->out)), 0, thiz->item_size);
		}
		thiz->out = (thiz->out + 1) % thiz->size;
		thiz->len--;
		/* WCN_DBG(FM_DBG | MAIN, "del an item[len=%d]\n", thiz->len); */
	} else {
		WCN_DBG(FM_WAR | MAIN, "%s fifo is empty\n", thiz->obj.name);
	}

	return 0;
}

bool fm_fifo_is_full(struct fm_fifo *thiz)
{
	return (thiz->len == thiz->size) ? true : false;
}

bool fm_fifo_is_empty(struct fm_fifo *thiz)
{
	return (thiz->len == 0) ? true : false;
}

signed int fm_fifo_get_total_len(struct fm_fifo *thiz)
{
	return thiz->size;
}

signed int fm_fifo_get_valid_len(struct fm_fifo *thiz)
{
	return thiz->len;
}

signed int fm_fifo_reset(struct fm_fifo *thiz)
{
	fm_memset(thiz->obj.priv, 0, thiz->item_size * thiz->size);
	thiz->in = 0;
	thiz->out = 0;
	thiz->len = 0;

	return 0;
}

struct fm_fifo *fm_fifo_init(struct fm_fifo *fifo, void *buf, const signed char *name, signed int item_size,
								signed int item_num)
{
	fm_memcpy(fifo->obj.name, name, 20);
	fifo->size = item_num;
	fifo->in = 0;
	fifo->out = 0;
	fifo->len = 0;
	fifo->item_size = item_size;
	fifo->obj.priv = buf;

	fifo->input = fm_fifo_in;
	fifo->output = fm_fifo_out;
	fifo->is_full = fm_fifo_is_full;
	fifo->is_empty = fm_fifo_is_empty;
	fifo->get_total_len = fm_fifo_get_total_len;
	fifo->get_valid_len = fm_fifo_get_valid_len;
	fifo->reset = fm_fifo_reset;

	WCN_DBG(FM_NTC | LINK, "%s inited\n", fifo->obj.name);

	return fifo;
}

struct fm_fifo *fm_fifo_create(const signed char *name, signed int item_size, signed int item_num)
{
	struct fm_fifo *tmp;
	void *buf;

	tmp = fm_zalloc(sizeof(struct fm_fifo));
	if (!tmp) {
		WCN_DBG(FM_ALT | MAIN, "fm_zalloc(fm_fifo) -ENOMEM\n");
		return NULL;
	}

	buf = fm_zalloc(item_size * item_num);
	if (!buf) {
		WCN_DBG(FM_ALT | MAIN, "fm_zalloc(fm_fifo) -ENOMEM\n");
		fm_free(tmp);
		return NULL;
	}

	tmp = fm_fifo_init(tmp, buf, name, item_size, item_num);

	WCN_DBG(FM_NTC | LINK, "%s created\n", tmp->obj.name);

	return tmp;
}

signed int fm_fifo_release(struct fm_fifo *fifo)
{
	if (fifo) {
		WCN_DBG(FM_NTC | LINK, "%s released\n", fifo->obj.name);
		if (fifo->obj.priv)
			fm_free(fifo->obj.priv);

		fm_free(fifo);
	}

	return 0;
}

unsigned short fm_get_u16_from_auc(unsigned char *buf)
{
	return (unsigned short)((unsigned short)buf[0] + ((unsigned short) buf[1] << 8));
}

void fm_set_u16_to_auc(unsigned char *buf, unsigned short val)
{
	buf[0] = (unsigned char)(val & 0xFF);
	buf[1] = (unsigned char)(val >> 8);
}

unsigned int fm_get_u32_from_auc(unsigned char *buf)
{
	return ((unsigned int)(*buf) + ((unsigned int)(*(buf + 1)) << 8) +
		((unsigned int)(*(buf + 2)) << 16) + ((unsigned int)(*(buf + 3)) << 24));
}

void fm_set_u32_to_auc(unsigned char *buf, unsigned int val)
{
	buf[0] = (unsigned char)val;
	buf[1] = (unsigned char)(val >> 8);
	buf[2] = (unsigned char)(val >> 16);
	buf[3] = (unsigned char)(val >> 24);
}
