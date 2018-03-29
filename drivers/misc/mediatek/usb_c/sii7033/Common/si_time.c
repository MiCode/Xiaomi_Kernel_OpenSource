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
#include <Wrap.h>
#include "si_time.h"
#include "si_usbpd_main.h"
#define SII_USE_HIGH_RES_TIMERS 1

#ifdef NOT_DEFINED
static void sii_timer_work_handler(WORK_STRUCT *work)
{
	struct timer_obj *sii_timer;

	sii_timer = container_of(work, struct timer_obj, work_item);

	sii_timer->flags |= TIMER_OBJ_FLAG_WORK_IP;
	/* NOT TESTING FOR FAILURE */
	if (!down_interruptible(&sii_timer->drv_context->isr_lock)) {

		sii_timer->timer_callback_handler(sii_timer->callback_param);

		up(&sii_timer->drv_context->isr_lock);
	}
	sii_timer->flags &= ~TIMER_OBJ_FLAG_WORK_IP;

	if (sii_timer->flags & TIMER_OBJ_FLAG_WORK_RESTART)
		sii_start_timer(sii_timer->drv_context, sii_timer, sii_timer->delay, true);

	if (sii_timer->flags & TIMER_OBJ_FLAG_DEL_REQ) {
		/*
		 * Deletion of this timer was requested during the execution of
		 * the callback handler so go ahead and delete it now.
		 */
		kfree(sii_timer);
	}
}
#endif
uint32_t sii_time_milli_get(void)
{
	return (uint32_t) (jiffies * 1000 / HZ);
}

static void s_time_out_milli_set(uint32_t *p_milli_t_o, uint32_t time_out)
{
	/*SII_PLATFORM_DEBUG_ASSERT(MILLI_TO_MAX > time_out); */
	*p_milli_t_o = sii_time_milli_get() + time_out;
}

void sii_time_out_milli_set(uint32_t *p_milli_t_o, uint32_t time_out)
{
	s_time_out_milli_set(p_milli_t_o, time_out);
}

static bool s_time_out_milli_is(const uint32_t *p_milli_t_o)
{
	uint32_t milli_new = sii_time_milli_get();
	uint32_t milli_dif =
	    (*p_milli_t_o > milli_new) ? (*p_milli_t_o - milli_new) : (milli_new - *p_milli_t_o);

	if (MILLI_TO_MAX < milli_dif)
		return (*p_milli_t_o > milli_new) ? (true) : (false);
	else
		return (*p_milli_t_o <= milli_new) ? (true) : (false);
}

bool sii_time_out_milli_is(const uint32_t *p_milli_t_o)
{
	return s_time_out_milli_is(p_milli_t_o);
}

static bool s_time_out_is_expired(const uint32_t *p_milli_t_o, uint32_t timeout)
{
	uint32_t milli_new = sii_time_milli_get();
	uint32_t milli_dif =
	    (*p_milli_t_o > milli_new) ? (*p_milli_t_o - milli_new) : (milli_new - *p_milli_t_o);

	/* todo. check if it is working fine. */
	/*if (MILLI_TO_MAX < milli_dif) */
	return (milli_dif < timeout) ? (false) : (true);
}

bool sii_time_out_is_expired(const uint32_t *p_milli_t_o, uint32_t timeout)
{
	return s_time_out_is_expired(p_milli_t_o, timeout);
}
