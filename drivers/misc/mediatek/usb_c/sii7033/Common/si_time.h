/*
*Copyright (C) 2013-2014 Silicon Image, Inc.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation version 2.
* This program is distributed AS-IS WITHOUT ANY WARRANTY of any
* kind, whether express or implied; INCLUDING without the implied warranty
* of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
* See the GNU General Public License for more details at
* http://www.gnu.org/licenses/gpl-2.0.html.
*/
#ifndef SI_TIME_H
#define SI_TIME_H
#include <Wrap.h>
#ifdef HR_TIMER_INCLUDE
int sii_start_timer(void *context, void **timer_handle);
int sii_stop_timer(void *context, void **timer_handle);
/*int sii_create_timer(void *context, void (*callback_handler)
	(void *callback_param),void *callback_param, void **timer_handle);
int sii_delete_timer(void *context, void **timer_handle);*/
#endif
struct timer_obj {
	WAIT_QUEUE_HEAD_T timer_run_wait_queue;
	WAIT_QUEUE_HEAD_T timer_del_wait_queue;
	struct timer_list timer;
	TASK_STRUCT timer_task_thread;
	struct list_head list_link;
#ifdef HR_TIMER_INCLUDE
	struct hrtimer hr_timer;
#endif
	uint8_t wake_flag;
	uint8_t stop_flag;
	uint8_t del_flag;
	uint32_t timer_cnt;
	struct usbpd_device *drv_context;
	uint8_t flags;
#define TIMER_OBJ_FLAG_WORK_IP  0x01
#define TIMER_OBJ_FLAG_WORK_RESTART     0x02
#define TIMER_OBJ_FLAG_DEL_REQ  0x04
	void *callback_param;
	void (*timer_callback_handler)(void *callback_param);
	unsigned long delay;
	unsigned long periodicity;
	unsigned long time_interval;
};
/* Convert a value specified in milliseconds to nanoseconds */
#define MSEC_TO_NSEC(x) (x * 1000000UL)
#define MILLI_TO_MAX          (((uint32_t)~0)>>1)	/* maximum milli */

#ifdef HR_TIMER_INCLUDE
int sii_create_timer(struct usbpd_device *context,
		     void (*callback_handler)(void *callback_param),
		     void *callback_param, struct usbpd_timer *usbpdtmr,
		     uint32_t time_msec, bool periodicity);
int sii_stop_timer(struct usbpd_device *context, struct usbpd_timer *usbpdtmr);
int sii_start_timer(struct usbpd_device *context, struct usbpd_timer *usbpdtmr);
int sii_delete_timer(struct usbpd_device *context, struct usbpd_timer *usbpdtmr);
#else
int sii_timer_create(void (*callback_handler) (void *callback_param),
		     void *callback_param, void **timer_handle, uint32_t time_msec,
		     bool periodicity);
int sii_timer_start(void **timer_handle);
int sii_timer_stop(void **timer_handle);
int sii_timer_delete(void **timer_handle);
#endif
#endif
