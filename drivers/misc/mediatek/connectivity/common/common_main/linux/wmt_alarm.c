/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/alarmtimer.h>
#include <linux/spinlock.h>
#include "osal.h"
#include "wmt_alarm.h"
#include "wmt_lib.h"

#define WMT_ALARM_STATE_UNINIT	0x0
#define WMT_ALARM_STATE_DISABLE	0x01
#define WMT_ALARM_STATE_ENABLE	0x03

struct wmt_alarm {
	struct alarm alarm_timer;
	unsigned int alarm_state;
	unsigned int alarm_sec;
	spinlock_t alarm_lock;
	struct work_struct dmp_info_worker;
	unsigned long flags;
};

struct wmt_alarm g_wmt_alarm;
static CONSYS_STATE_DMP_INFO g_dmp_info;


static void wmt_alarm_dmp_info_handler(struct work_struct *work)
{
	UINT8 dmp_info_buf[DBG_LOG_STR_SIZE];
	int len = 0, i, dmp_cnt = 5;

	if (wmt_lib_dmp_consys_state(&g_dmp_info, dmp_cnt, 0) == MTK_WCN_BOOL_TRUE) {

		len += snprintf(dmp_info_buf + len,
						DBG_LOG_STR_SIZE - len, "0x%08x", g_dmp_info.cpu_pcr[0]);
		for (i = 1; i < dmp_cnt; i++)
			len += snprintf(dmp_info_buf + len,
						DBG_LOG_STR_SIZE - len, ";0x%08x", g_dmp_info.cpu_pcr[i]);

		len += snprintf(dmp_info_buf + len,
					DBG_LOG_STR_SIZE - len, ";<0x%08x>", g_dmp_info.state.lp[1]);

		len += snprintf(dmp_info_buf + len,
					DBG_LOG_STR_SIZE - len, ";<0x%08x>", g_dmp_info.state.gating[1]);

		len += snprintf(dmp_info_buf + len, DBG_LOG_STR_SIZE - len,
					";[0x%08x", g_dmp_info.state.sw_state.info_time);
		len += snprintf(dmp_info_buf + len, DBG_LOG_STR_SIZE - len,
					";0x%08x", g_dmp_info.state.sw_state.is_gating);
		len += snprintf(dmp_info_buf + len, DBG_LOG_STR_SIZE - len,
					";0x%08x", g_dmp_info.state.sw_state.resource_disable_sleep);
		len += snprintf(dmp_info_buf + len, DBG_LOG_STR_SIZE - len,
					";0x%08x", g_dmp_info.state.sw_state.clock_hif_ctrl);
		len += snprintf(dmp_info_buf + len, DBG_LOG_STR_SIZE - len,
					";0x%08x", g_dmp_info.state.sw_state.clock_umac_ctrl);
		len += snprintf(dmp_info_buf + len, DBG_LOG_STR_SIZE - len,
					";0x%08x", g_dmp_info.state.sw_state.clock_mcu);
		len += snprintf(dmp_info_buf + len, DBG_LOG_STR_SIZE - len,
					";0x%08x]", g_dmp_info.state.sw_state.sub_system);

		WMT_INFO_FUNC("%s", dmp_info_buf);
	}
}

static enum alarmtimer_restart alarm_timer_handler(struct alarm *alarm,
	ktime_t now)
{
	ktime_t kt;

	spin_lock_irqsave(&g_wmt_alarm.alarm_lock, g_wmt_alarm.flags);

	schedule_work(&g_wmt_alarm.dmp_info_worker);
	kt = ktime_set(g_wmt_alarm.alarm_sec, 0);
	alarm_start_relative(&g_wmt_alarm.alarm_timer, kt);

	spin_unlock_irqrestore(&g_wmt_alarm.alarm_lock, g_wmt_alarm.flags);

	return ALARMTIMER_NORESTART;
}

static int _wmt_alarm_start_timer_nolock(unsigned int sec)
{
	ktime_t kt;

	g_wmt_alarm.alarm_sec = sec;
	kt = ktime_set(g_wmt_alarm.alarm_sec, 0);
	alarm_start_relative(&g_wmt_alarm.alarm_timer, kt);
	g_wmt_alarm.alarm_state = WMT_ALARM_STATE_ENABLE;

	pr_info("[wmt_alarm] alarm timer enabled timeout=[%d]", g_wmt_alarm.alarm_sec);
	return 0;
}

int wmt_alarm_init(void)
{
	spin_lock_init(&g_wmt_alarm.alarm_lock);

	spin_lock_irqsave(&g_wmt_alarm.alarm_lock, g_wmt_alarm.flags);
	alarm_init(&g_wmt_alarm.alarm_timer, ALARM_REALTIME, alarm_timer_handler);
	INIT_WORK(&g_wmt_alarm.dmp_info_worker, wmt_alarm_dmp_info_handler);
	g_wmt_alarm.alarm_state = WMT_ALARM_STATE_DISABLE;

	spin_unlock_irqrestore(&g_wmt_alarm.alarm_lock, g_wmt_alarm.flags);

	return 0;
}

int wmt_alarm_deinit(void)
{
	wmt_alarm_cancel();
	return 0;
}


int _wmt_alarm_cancel_nolock(void)
{
	if (g_wmt_alarm.alarm_state == WMT_ALARM_STATE_ENABLE) {
		pr_info("disable wmt_alarm");
		alarm_cancel(&g_wmt_alarm.alarm_timer);
		g_wmt_alarm.alarm_state = WMT_ALARM_STATE_DISABLE;
	}
	return 0;
}

int wmt_alarm_start(unsigned int sec)
{
	spin_lock_irqsave(&g_wmt_alarm.alarm_lock, g_wmt_alarm.flags);
	if (g_wmt_alarm.alarm_state == WMT_ALARM_STATE_UNINIT) {
		spin_unlock_irqrestore(&g_wmt_alarm.alarm_lock, g_wmt_alarm.flags);
		return -1;
	}

	if (g_wmt_alarm.alarm_state == WMT_ALARM_STATE_ENABLE)
		_wmt_alarm_cancel_nolock();
	_wmt_alarm_start_timer_nolock(sec);
	spin_unlock_irqrestore(&g_wmt_alarm.alarm_lock, g_wmt_alarm.flags);
	return 0;
}

int wmt_alarm_cancel(void)
{
	int ret;

	spin_lock_irqsave(&g_wmt_alarm.alarm_lock, g_wmt_alarm.flags);
	ret = _wmt_alarm_cancel_nolock();
	spin_unlock_irqrestore(&g_wmt_alarm.alarm_lock, g_wmt_alarm.flags);
	return ret;
}
