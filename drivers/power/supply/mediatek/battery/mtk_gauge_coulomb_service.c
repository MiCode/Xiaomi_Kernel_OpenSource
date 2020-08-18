/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef _DEA_MODIFY_
#include <mt-plat/upmu_common.h>
#include <mt-plat/mtk_battery.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#endif

#include "mtk_gauge_class.h"
#include <mtk_battery_internal.h>

static struct list_head coulomb_head_plus = LIST_HEAD_INIT(coulomb_head_plus);
static struct list_head coulomb_head_minus = LIST_HEAD_INIT(coulomb_head_minus);
static struct mutex coulomb_lock;
static struct mutex hw_coulomb_lock;
static unsigned long reset_coulomb;
static spinlock_t slock;
static struct wakeup_source wlock;
static wait_queue_head_t wait_que;
static bool coulomb_thread_timeout;
static int fgclog_level;
static int pre_coulomb;
static bool init;
static int coulomb_lock_cnt, hw_coulomb_lock_cnt;
int fix_coverity;

#define FTLOG_ERROR_LEVEL   1
#define FTLOG_DEBUG_LEVEL   2
#define FTLOG_TRACE_LEVEL   3

#define ft_err(fmt, args...)   \
do {									\
	if (fgclog_level >= FTLOG_ERROR_LEVEL) {			\
		pr_notice(fmt, ##args); \
	}								   \
} while (0)

#define ft_debug(fmt, args...)   \
do {									\
	if (fgclog_level >= FTLOG_DEBUG_LEVEL) {		\
		pr_notice(fmt, ##args); \
	}								   \
} while (0)

#define ft_trace(fmt, args...)\
do {									\
	if (fgclog_level >= FTLOG_TRACE_LEVEL) {			\
		pr_notice(fmt, ##args);\
	}						\
} while (0)


void mutex_coulomb_lock(void)
{
	mutex_lock(&coulomb_lock);
	coulomb_lock_cnt++;
}

void mutex_coulomb_unlock(void)
{
	coulomb_lock_cnt--;
	mutex_unlock(&coulomb_lock);
}

void mutex_hw_coulomb_lock(void)
{
	mutex_lock(&hw_coulomb_lock);
	hw_coulomb_lock_cnt++;
}

void mutex_hw_coulomb_unlock(void)
{
	hw_coulomb_lock_cnt--;
	mutex_unlock(&hw_coulomb_lock);
}


void wake_up_gauge_coulomb(void)
{
	unsigned long flags = 0;

	if (init == false) {
		ft_err("[%s]gauge_coulomb service is not rdy\n", __func__);
		return;
	}

	if (is_fg_disabled()) {
		gauge_set_coulomb_interrupt1_ht(0);
		gauge_set_coulomb_interrupt1_lt(0);
		return;
	}

	ft_err("%s %d %d %d %d\n",
		__func__,
		wlock.active,
		coulomb_thread_timeout,
		coulomb_lock_cnt,
		hw_coulomb_lock_cnt);

	mutex_hw_coulomb_lock();
	gauge_set_coulomb_interrupt1_ht(300);
	gauge_set_coulomb_interrupt1_lt(300);
	mutex_hw_coulomb_unlock();
	spin_lock_irqsave(&slock, flags);
	if (wlock.active == 0)
		__pm_stay_awake(&wlock);
	spin_unlock_irqrestore(&slock, flags);

	coulomb_thread_timeout = true;
	wake_up(&wait_que);
	ft_debug("%s end\n", __func__);
}

void gauge_coulomb_set_log_level(int x)
{
	fgclog_level = x;
}

void gauge_coulomb_consumer_init(
	struct gauge_consumer *coulomb, struct device *dev, char *name)
{
	coulomb->name = name;
	INIT_LIST_HEAD(&coulomb->list);
	coulomb->dev = dev;
}
void gauge_coulomb_dump_list(void)
{
	struct list_head *pos;
	struct list_head *phead = &coulomb_head_plus;
	struct gauge_consumer *ptr;
	int car;

	if (init == false) {
		ft_err("[%s]gauge_coulomb service is not rdy\n", __func__);
		return;
	}

	ft_debug("%s %d %d\n",
		__func__,
		wlock.active,
		coulomb_thread_timeout);

	mutex_coulomb_lock();
	car = gauge_get_coulomb();
	if (list_empty(phead) != true) {
		ft_debug("dump plus list start\n");
		list_for_each(pos, phead) {
			ptr = container_of(pos, struct gauge_consumer, list);
			ft_debug(
				"+dump list name:%s start:%ld end:%ld car:%d int:%d\n",
				ptr->name,
			ptr->start, ptr->end, car, ptr->variable);
		}
	}

	phead = &coulomb_head_minus;
	if (list_empty(phead) != true) {
		ft_debug("dump minus list start\n");
		list_for_each(pos, phead) {
			ptr = container_of(pos, struct gauge_consumer, list);
			ft_debug(
				"-dump list name:%s start:%ld end:%ld car:%d int:%d\n",
				ptr->name,
			ptr->start, ptr->end, car, ptr->variable);
		}
	}
	mutex_coulomb_unlock();
}


void gauge_coulomb_before_reset(void)
{
	if (init == false) {
		ft_err("[%s]gauge_coulomb service is not rdy\n", __func__);
		return;
	}
	mutex_coulomb_lock();
	mutex_hw_coulomb_lock();
	gauge_set_coulomb_interrupt1_ht(0);
	gauge_set_coulomb_interrupt1_lt(0);
	mutex_hw_coulomb_unlock();
	mutex_coulomb_unlock();

	reset_coulomb = gauge_get_coulomb();
	ft_err("%s car=%ld\n",
		__func__,
		reset_coulomb);
	gauge_coulomb_dump_list();
}

void gauge_coulomb_after_reset(void)
{
	struct list_head *pos;
	struct list_head *phead;
	struct gauge_consumer *ptr;
	unsigned long now = reset_coulomb;
	unsigned long duraction;

	if (init == false) {
		ft_err("[%s]gauge_coulomb service is not rdy\n", __func__);
		return;
	}

	ft_err("%s\n",
		__func__);
	mutex_coulomb_lock();

	/* check plus list */
	phead = &coulomb_head_plus;
	list_for_each(pos, phead) {
		ptr = container_of(pos, struct gauge_consumer, list);

		ptr->start = 0;
		duraction = ptr->end - now;
		ptr->end = duraction;
		ptr->variable = duraction;
		ft_debug("[%s]+ %s %ld %ld %d\n",
			__func__,
			ptr->name,
		ptr->start, ptr->end, ptr->variable);
	}

	/* check minus list */
	phead = &coulomb_head_minus;
	list_for_each(pos, phead) {
		ptr = container_of(pos, struct gauge_consumer, list);

		ptr->start = 0;
		duraction = ptr->end - now;
		ptr->end = duraction;
		ptr->variable = duraction;
		ft_debug("[%s]- %s %ld %ld %d\n",
			__func__,
			ptr->name,
		ptr->start, ptr->end, ptr->variable);
	}

	mutex_coulomb_unlock();

	gauge_coulomb_dump_list();

	wake_up_gauge_coulomb();
}

void gauge_coulomb_start(struct gauge_consumer *coulomb, int car)
{
	struct list_head *pos;
	struct list_head *phead;
	struct gauge_consumer *ptr = NULL;
	int hw_car, now_car;
	bool wake = false;
	int car_now;

	if (init == false) {
		ft_err("[%s]gauge_coulomb service is not rdy\n", __func__);
		return;
	}

	if (is_fg_disabled()) {
		gauge_set_coulomb_interrupt1_ht(0);
		gauge_set_coulomb_interrupt1_lt(0);
		return;
	}

	if (car == 0)
		return;

	mutex_coulomb_lock();

	car_now = gauge_get_coulomb();
	/* del from old list */
	if (list_empty(&coulomb->list) != true) {
		ft_trace("coulomb_start del name:%s s:%ld e:%ld v:%d car:%d\n",
		coulomb->name,
		coulomb->start, coulomb->end, coulomb->variable, car_now);
		list_del_init(&coulomb->list);
	}

	coulomb->start = car_now;
	coulomb->end = coulomb->start + car;
	coulomb->variable = car;
	now_car = coulomb->start;

	if (car > 0)
		phead = &coulomb_head_plus;
	else
		phead = &coulomb_head_minus;

	/* add node to list */
	list_for_each(pos, phead) {
		ptr = container_of(pos, struct gauge_consumer, list);
		if (car > 0) {
			if (coulomb->end < ptr->end)
				break;
		} else
			if (coulomb->end > ptr->end)
				break;
	}
	list_add(&coulomb->list, pos->prev);

	if (car > 0) {
		list_for_each(pos, phead) {
			ptr = container_of(pos, struct gauge_consumer, list);
			if (ptr->end - now_car <= 0)
				wake = true;
			else
				break;
		}
		hw_car = ptr->end - now_car;
		mutex_hw_coulomb_lock();
		gauge_set_coulomb_interrupt1_ht(hw_car);
		mutex_hw_coulomb_unlock();
	} else {
		list_for_each(pos, phead) {
			ptr = container_of(pos, struct gauge_consumer, list);
			if (ptr->end - now_car >= 0)
				wake = true;
			else
				break;
		}
		hw_car = now_car - ptr->end;
		mutex_hw_coulomb_lock();
		gauge_set_coulomb_interrupt1_lt(hw_car);
		mutex_hw_coulomb_unlock();
	}
	mutex_coulomb_unlock();

	if (wake == true)
		wake_up_gauge_coulomb();

	ft_debug("coulomb_start dev:%s name:%s s:%ld e:%ld v:%d car:%d w:%d\n",
	dev_name(coulomb->dev), coulomb->name, coulomb->start, coulomb->end,
	coulomb->variable, car, wake);


}

void gauge_coulomb_stop(struct gauge_consumer *coulomb)
{
	if (init == false) {
		ft_err("[%s]gauge_coulomb service is not rdy\n", __func__);
		return;
	}

	if (is_fg_disabled()) {
		gauge_set_coulomb_interrupt1_ht(0);
		gauge_set_coulomb_interrupt1_lt(0);
		fix_coverity = 1;
		return;
	}

	ft_debug("coulomb_stop name:%s %ld %ld %d\n",
	coulomb->name, coulomb->start, coulomb->end,
	coulomb->variable);

	mutex_coulomb_lock();
	list_del_init(&coulomb->list);
	mutex_coulomb_unlock();

}

static struct timespec sstart[10];
void gauge_coulomb_int_handler(void)
{
	int car, hw_car;
	struct list_head *pos;
	struct list_head *phead;
	struct gauge_consumer *ptr = NULL;

	get_monotonic_boottime(&sstart[0]);
	car = gauge_get_coulomb();
	ft_trace("[%s] car:%d preCar:%d\n",
		__func__,
		car, pre_coulomb);
	get_monotonic_boottime(&sstart[1]);

	if (list_empty(&coulomb_head_plus) != true) {
		pos = coulomb_head_plus.next;
		phead = &coulomb_head_plus;
		for (pos = phead->next; pos != phead;) {
			struct list_head *ptmp;

			ptr = container_of(pos, struct gauge_consumer, list);
			if (ptr->end <= car) {
				ptmp = pos;
				pos = pos->next;
				list_del_init(ptmp);
				ft_trace(
					"[%s]+ %s s:%ld e:%ld car:%d %d int:%d timeout\n",
					__func__,
					ptr->name,
					ptr->start, ptr->end, car,
					pre_coulomb, ptr->variable);
				if (ptr->callback) {
					mutex_coulomb_unlock();
					ptr->callback(ptr);
					mutex_coulomb_lock();
					pos = coulomb_head_plus.next;
				}
			} else
				break;
		}

		if (list_empty(&coulomb_head_plus) != true) {
			pos = coulomb_head_plus.next;
			ptr = container_of(pos, struct gauge_consumer, list);
			hw_car = ptr->end - car;
			ft_trace(
				"[%s]+ %s %ld %ld %d now:%d dif:%d\n",
				__func__,
					ptr->name,
					ptr->start, ptr->end,
					ptr->variable, car, hw_car);
			mutex_hw_coulomb_lock();
			gauge_set_coulomb_interrupt1_ht(hw_car);
			mutex_hw_coulomb_unlock();
		} else
			ft_trace("+ list is empty\n");
	} else
		ft_trace("+ list is empty\n");


	if (list_empty(&coulomb_head_minus) != true) {
		pos = coulomb_head_minus.next;
		phead = &coulomb_head_minus;
		for (pos = phead->next; pos != phead;) {
			struct list_head *ptmp;

			ptr = container_of(pos, struct gauge_consumer, list);
			if (ptr->end >= car) {
				ptmp = pos;
				pos = pos->next;
				list_del_init(ptmp);
				ft_trace(
					"[%s]- %s s:%ld e:%ld car:%d %d int:%d timeout\n",
					__func__,
					ptr->name,
					ptr->start, ptr->end,
					car, pre_coulomb, ptr->variable);
				if (ptr->callback) {
					mutex_coulomb_unlock();
					ptr->callback(ptr);
					mutex_coulomb_lock();
					pos = coulomb_head_minus.next;
				}

			} else
				break;
		}

		if (list_empty(&coulomb_head_minus) != true) {
			pos = coulomb_head_minus.next;
			ptr = container_of(pos, struct gauge_consumer, list);
			hw_car = car - ptr->end;
			ft_trace(
				"[%s]- %s %ld %ld %d now:%d dif:%d\n",
				__func__,
				ptr->name,
				ptr->start, ptr->end,
				ptr->variable, car, hw_car);
			mutex_hw_coulomb_lock();
			gauge_set_coulomb_interrupt1_lt(hw_car);
			mutex_hw_coulomb_unlock();
		} else
			ft_trace("- list is empty\n");
	} else
		ft_trace("- list is empty\n");

	pre_coulomb = car;
	get_monotonic_boottime(&sstart[2]);
	sstart[0] = timespec_sub(sstart[1], sstart[0]);
	sstart[1] = timespec_sub(sstart[2], sstart[1]);
}

static int gauge_coulomb_thread(void *arg)
{
	unsigned long flags = 0;
	struct timespec start, end, duraction;

	while (1) {
		wait_event(wait_que, (coulomb_thread_timeout == true));
		coulomb_thread_timeout = false;
		get_monotonic_boottime(&start);
		ft_trace("[%s]=>\n", __func__);
		mutex_coulomb_lock();
		gauge_coulomb_int_handler();
		mutex_coulomb_unlock();

		spin_lock_irqsave(&slock, flags);
		__pm_relax(&wlock);
		spin_unlock_irqrestore(&slock, flags);


		get_monotonic_boottime(&end);
		duraction = timespec_sub(end, start);

		ft_trace(
			"%s time:%d ms %d %d\n",
			__func__,
			(int)(duraction.tv_nsec / 1000000),
			(int)(sstart[0].tv_nsec / 1000000),
			(int)(sstart[1].tv_nsec / 1000000));

		if (fix_coverity == 1)
			break;
	}

	return 0;
}

void gauge_coulomb_service_init(void)
{
	ft_trace("gauge coulomb_service_init\n");
	INIT_LIST_HEAD(&coulomb_head_minus);
	INIT_LIST_HEAD(&coulomb_head_plus);
	mutex_init(&coulomb_lock);
	mutex_init(&hw_coulomb_lock);
	spin_lock_init(&slock);
	wakeup_source_init(&wlock, "gauge coulomb wakelock");
	init_waitqueue_head(&wait_que);
	kthread_run(gauge_coulomb_thread, NULL, "gauge_coulomb_thread");

	pmic_register_interrupt_callback(
		FG_BAT1_INT_L_NO, wake_up_gauge_coulomb);
	pmic_register_interrupt_callback(
		FG_BAT1_INT_H_NO, wake_up_gauge_coulomb);
	pre_coulomb = gauge_get_coulomb();
	init = true;

#ifdef _DEA_MODIFY_
	wait_que.function = gauge_coulomb_int_handler;
	INIT_LIST_HEAD(&wait_que.list);
	wait_que.name = "gauge coulomb service";
#endif

}
