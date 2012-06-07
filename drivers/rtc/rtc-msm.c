/*
 * Copyright (C) 2008 Google, Inc.
 * Copyright (c) 2009-2011 Code Aurora Forum. All rights reserved.
 * Author: San Mehat <san@google.com>
 *
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
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/android_alarm.h>

#include <linux/rtc.h>
#include <linux/rtc-msm.h>
#include <linux/msm_rpcrouter.h>
#include <mach/msm_rpcrouter.h>

#define APP_TIMEREMOTE_PDEV_NAME "rs00000000"

#define TIMEREMOTE_PROCEEDURE_SET_JULIAN	6
#define TIMEREMOTE_PROCEEDURE_GET_JULIAN	7
#ifdef CONFIG_RTC_SECURE_TIME_SUPPORT
#define TIMEREMOTE_PROCEEDURE_GET_SECURE_JULIAN	11
#define TIMEREMOTE_PROCEEDURE_SET_SECURE_JULIAN	16
#endif
#define TIMEREMOTE_PROG_NUMBER 0x30000048
#define TIMEREMOTE_PROG_VER_1 0x00010001
#define TIMEREMOTE_PROG_VER_2 0x00040001

#define RTC_REQUEST_CB_PROC		0x17
#define RTC_CLIENT_INIT_PROC		0x12
#define RTC_EVENT_CB_PROC		0x1
#define RTC_CB_ID			0x1

/* Client request errors */
enum rtc_rpc_err {
	ERR_NONE,
	ERR_CLIENT_ID_PTR,		/* Invalid client ID pointer */
	ERR_CLIENT_TYPE,		/* Invalid client type */
	ERR_CLIENT_ID,			/* Invalid client ID */
	ERR_TASK_NOT_READY,		/* task is not ready for clients */
	ERR_INVALID_PROCESSOR,		/* Invalid processor id */
	ERR_UNSUPPORTED,		/* Unsupported request */
	ERR_GENERAL,			/* Any General Error */
	ERR_RPC,			/* Any ONCRPC Error */
	ERR_ALREADY_REG,		/* Client already registered */
	ERR_MAX
};

enum processor_type {
	CLIENT_PROCESSOR_NONE   = 0,
	CLIENT_PROCESSOR_MODEM,
	CLIENT_PROCESSOR_APP1,
	CLIENT_PROCESSOR_APP2,
	CLIENT_PROCESSOR_MAX
};

/* Client types */
enum client_type {
	CLIENT_TYPE_GEN1 = 0,
	CLIENT_FLOATING1,
	CLIENT_FLOATING2,
	CLIENT_TYPE_INTERNAL,
	CLIENT_TYPE_GENOFF_UPDATE,
	CLIENT_TYPE_MAX
};

/* Event types */
enum event_type {
	EVENT_TOD_CHANGE = 0,
	EVENT_GENOFF_CHANGE,
	EVENT_MAX
};

struct tod_update_info {
	uint32_t	tick;
	uint64_t	stamp;
	uint32_t	freq;
};

enum time_bases_info {
	TIME_RTC = 0,
	TIME_TOD,
	TIME_USER,
	TIME_SECURE,
	TIME_INVALID
};

struct genoff_update_info {
	enum time_bases_info time_base;
	uint64_t	offset;
};

union cb_info {
	struct tod_update_info tod_update;
	struct genoff_update_info genoff_update;
};

struct rtc_cb_recv {
	uint32_t client_cb_id;
	enum event_type event;
	uint32_t cb_info_ptr;
	union cb_info cb_info_data;
};

struct msm_rtc {
	int proc;
	struct msm_rpc_client *rpc_client;
	u8 client_id;
	struct rtc_device *rtc;
#ifdef CONFIG_RTC_SECURE_TIME_SUPPORT
	struct rtc_device *rtcsecure;
#endif
	unsigned long rtcalarm_time;
};

struct rpc_time_julian {
	uint32_t year;
	uint32_t month;
	uint32_t day;
	uint32_t hour;
	uint32_t minute;
	uint32_t second;
	uint32_t day_of_week;
};

struct rtc_tod_args {
	int proc;
	struct rtc_time *tm;
};

#ifdef CONFIG_PM
struct suspend_state_info {
	atomic_t state;
	int64_t tick_at_suspend;
};

static struct suspend_state_info suspend_state = {ATOMIC_INIT(0), 0};

void msmrtc_updateatsuspend(struct timespec *ts)
{
	int64_t now, sleep, sclk_max;

	if (atomic_read(&suspend_state.state)) {
		now = msm_timer_get_sclk_time(&sclk_max);

		if (now && suspend_state.tick_at_suspend) {
			if (now < suspend_state.tick_at_suspend) {
				sleep = sclk_max -
					suspend_state.tick_at_suspend + now;
			} else
				sleep = now - suspend_state.tick_at_suspend;

			timespec_add_ns(ts, sleep);
			suspend_state.tick_at_suspend = now;
		} else
			pr_err("%s: Invalid ticks from SCLK now=%lld"
				"tick_at_suspend=%lld", __func__, now,
				suspend_state.tick_at_suspend);
	}

}
#else
void msmrtc_updateatsuspend(struct timespec *ts) { }
#endif
EXPORT_SYMBOL(msmrtc_updateatsuspend);

static int msmrtc_tod_proc_args(struct msm_rpc_client *client, void *buff,
							void *data)
{
	struct rtc_tod_args *rtc_args = data;

	if ((rtc_args->proc == TIMEREMOTE_PROCEEDURE_SET_JULIAN)
#ifdef CONFIG_RTC_SECURE_TIME_SUPPORT
	|| (rtc_args->proc == TIMEREMOTE_PROCEEDURE_SET_SECURE_JULIAN)
#endif
	) {
		struct timeremote_set_julian_req {
			uint32_t opt_arg;
			struct rpc_time_julian time;
		};
		struct timeremote_set_julian_req *set_req = buff;

		set_req->opt_arg = cpu_to_be32(0x1);
		set_req->time.year = cpu_to_be32(rtc_args->tm->tm_year);
		set_req->time.month = cpu_to_be32(rtc_args->tm->tm_mon + 1);
		set_req->time.day = cpu_to_be32(rtc_args->tm->tm_mday);
		set_req->time.hour = cpu_to_be32(rtc_args->tm->tm_hour);
		set_req->time.minute = cpu_to_be32(rtc_args->tm->tm_min);
		set_req->time.second = cpu_to_be32(rtc_args->tm->tm_sec);
		set_req->time.day_of_week = cpu_to_be32(rtc_args->tm->tm_wday);

		return sizeof(*set_req);

	} else if ((rtc_args->proc == TIMEREMOTE_PROCEEDURE_GET_JULIAN)
#ifdef CONFIG_RTC_SECURE_TIME_SUPPORT
	|| (rtc_args->proc == TIMEREMOTE_PROCEEDURE_GET_SECURE_JULIAN)
#endif
	) {
		*(uint32_t *)buff = (uint32_t) cpu_to_be32(0x1);

		return sizeof(uint32_t);
	} else
		return 0;
}

static bool rtc_check_overflow(struct rtc_time *tm)
{
	if (tm->tm_year < 138)
		return false;

	if (tm->tm_year > 138)
		return true;

	if ((tm->tm_year == 138) && (tm->tm_mon == 0) && (tm->tm_mday < 19))
		return false;

	return true;
}

static int msmrtc_tod_proc_result(struct msm_rpc_client *client, void *buff,
							void *data)
{
	struct rtc_tod_args *rtc_args = data;

	if ((rtc_args->proc == TIMEREMOTE_PROCEEDURE_GET_JULIAN)
#ifdef CONFIG_RTC_SECURE_TIME_SUPPORT
	|| (rtc_args->proc == TIMEREMOTE_PROCEEDURE_GET_SECURE_JULIAN)
#endif
	)  {
		struct timeremote_get_julian_rep {
			uint32_t opt_arg;
			struct rpc_time_julian time;
		};
		struct timeremote_get_julian_rep *result = buff;

		if (be32_to_cpu(result->opt_arg) != 0x1)
			return -ENODATA;

		rtc_args->tm->tm_year = be32_to_cpu(result->time.year);
		rtc_args->tm->tm_mon = be32_to_cpu(result->time.month);
		rtc_args->tm->tm_mday = be32_to_cpu(result->time.day);
		rtc_args->tm->tm_hour = be32_to_cpu(result->time.hour);
		rtc_args->tm->tm_min = be32_to_cpu(result->time.minute);
		rtc_args->tm->tm_sec = be32_to_cpu(result->time.second);
		rtc_args->tm->tm_wday = be32_to_cpu(result->time.day_of_week);

		pr_debug("%s: %.2u/%.2u/%.4u %.2u:%.2u:%.2u (%.2u)\n",
			__func__, rtc_args->tm->tm_mon, rtc_args->tm->tm_mday,
			rtc_args->tm->tm_year, rtc_args->tm->tm_hour,
			rtc_args->tm->tm_min, rtc_args->tm->tm_sec,
			rtc_args->tm->tm_wday);

		/* RTC layer expects years to start at 1900 */
		rtc_args->tm->tm_year -= 1900;
		/* RTC layer expects mons to be 0 based */
		rtc_args->tm->tm_mon--;

		if (rtc_valid_tm(rtc_args->tm) < 0) {
			pr_err("%s: Retrieved data/time not valid\n", __func__);
			rtc_time_to_tm(0, rtc_args->tm);
		}

		/*
		 * Check if the time received is > 01-19-2038, to prevent
		 * overflow. In such a case, return the EPOCH time.
		 */
		if (rtc_check_overflow(rtc_args->tm) == true) {
			pr_err("Invalid time (year > 2038)\n");
			rtc_time_to_tm(0, rtc_args->tm);
		}

		return 0;
	} else
		return 0;
}

static int
msmrtc_timeremote_set_time(struct device *dev, struct rtc_time *tm)
{
	int rc;
	struct rtc_tod_args rtc_args;
	struct msm_rtc *rtc_pdata = dev_get_drvdata(dev);

	if (tm->tm_year < 1900)
		tm->tm_year += 1900;

	if (tm->tm_year < 1970)
		return -EINVAL;

	dev_dbg(dev, "%s: %.2u/%.2u/%.4u %.2u:%.2u:%.2u (%.2u)\n",
	       __func__, tm->tm_mon, tm->tm_mday, tm->tm_year,
	       tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_wday);

	rtc_args.proc = TIMEREMOTE_PROCEEDURE_SET_JULIAN;
	rtc_args.tm = tm;
	rc = msm_rpc_client_req(rtc_pdata->rpc_client,
				TIMEREMOTE_PROCEEDURE_SET_JULIAN,
				msmrtc_tod_proc_args, &rtc_args,
				NULL, NULL, -1);
	if (rc) {
		dev_err(dev, "%s: rtc time (TOD) could not be set\n", __func__);
		return rc;
	}

	return 0;
}

static int
msmrtc_timeremote_read_time(struct device *dev, struct rtc_time *tm)
{
	int rc;
	struct rtc_tod_args rtc_args;
	struct msm_rtc *rtc_pdata = dev_get_drvdata(dev);

	rtc_args.proc = TIMEREMOTE_PROCEEDURE_GET_JULIAN;
	rtc_args.tm = tm;

	rc = msm_rpc_client_req(rtc_pdata->rpc_client,
				TIMEREMOTE_PROCEEDURE_GET_JULIAN,
				msmrtc_tod_proc_args, &rtc_args,
				msmrtc_tod_proc_result, &rtc_args, -1);

	if (rc) {
		dev_err(dev, "%s: Error retrieving rtc (TOD) time\n", __func__);
		return rc;
	}

	return 0;
}

static int
msmrtc_virtual_alarm_set(struct device *dev, struct rtc_wkalrm *a)
{
	struct msm_rtc *rtc_pdata = dev_get_drvdata(dev);
	unsigned long now = get_seconds();

	if (!a->enabled) {
		rtc_pdata->rtcalarm_time = 0;
		return 0;
	} else
		rtc_tm_to_time(&a->time, &(rtc_pdata->rtcalarm_time));

	if (now > rtc_pdata->rtcalarm_time) {
		dev_err(dev, "%s: Attempt to set alarm in the past\n",
		       __func__);
		rtc_pdata->rtcalarm_time = 0;
		return -EINVAL;
	}

	return 0;
}

static struct rtc_class_ops msm_rtc_ops = {
	.read_time	= msmrtc_timeremote_read_time,
	.set_time	= msmrtc_timeremote_set_time,
	.set_alarm	= msmrtc_virtual_alarm_set,
};

#ifdef CONFIG_RTC_SECURE_TIME_SUPPORT
static int
msmrtc_timeremote_set_time_secure(struct device *dev, struct rtc_time *tm)
{
	int rc;
	struct rtc_tod_args rtc_args;
	struct msm_rtc *rtc_pdata = dev_get_drvdata(dev);

	if (tm->tm_year < 1900)
		tm->tm_year += 1900;

	if (tm->tm_year < 1970)
		return -EINVAL;

	dev_dbg(dev, "%s: %.2u/%.2u/%.4u %.2u:%.2u:%.2u (%.2u)\n",
	       __func__, tm->tm_mon, tm->tm_mday, tm->tm_year,
	       tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_wday);

	rtc_args.proc = TIMEREMOTE_PROCEEDURE_SET_SECURE_JULIAN;
	rtc_args.tm = tm;

	rc = msm_rpc_client_req(rtc_pdata->rpc_client,
			TIMEREMOTE_PROCEEDURE_SET_SECURE_JULIAN,
				msmrtc_tod_proc_args, &rtc_args,
				NULL, NULL, -1);
	if (rc) {
		dev_err(dev,
			"%s: rtc secure time could not be set\n", __func__);
		return rc;
	}

	return 0;
}

static int
msmrtc_timeremote_read_time_secure(struct device *dev, struct rtc_time *tm)
{
	int rc;
	struct rtc_tod_args rtc_args;
	struct msm_rtc *rtc_pdata = dev_get_drvdata(dev);
	rtc_args.proc = TIMEREMOTE_PROCEEDURE_GET_SECURE_JULIAN;
	rtc_args.tm = tm;

	rc = msm_rpc_client_req(rtc_pdata->rpc_client,
		TIMEREMOTE_PROCEEDURE_GET_SECURE_JULIAN, msmrtc_tod_proc_args,
		&rtc_args, msmrtc_tod_proc_result, &rtc_args, -1);

	if (rc) {
		dev_err(dev,
			"%s: Error retrieving secure rtc time\n", __func__);
		return rc;
	}

	return 0;
}

static struct rtc_class_ops msm_rtc_ops_secure = {
	.read_time	= msmrtc_timeremote_read_time_secure,
	.set_time	= msmrtc_timeremote_set_time_secure,
};
#endif

static void process_cb_request(void *buffer)
{
	struct rtc_cb_recv *rtc_cb = buffer;
	struct timespec ts, tv;

	rtc_cb->client_cb_id = be32_to_cpu(rtc_cb->client_cb_id);
	rtc_cb->event = be32_to_cpu(rtc_cb->event);
	rtc_cb->cb_info_ptr = be32_to_cpu(rtc_cb->cb_info_ptr);

	if (rtc_cb->event == EVENT_TOD_CHANGE) {
		/* A TOD update has been received from the Modem */
		rtc_cb->cb_info_data.tod_update.tick =
			be32_to_cpu(rtc_cb->cb_info_data.tod_update.tick);
		rtc_cb->cb_info_data.tod_update.stamp =
			be64_to_cpu(rtc_cb->cb_info_data.tod_update.stamp);
		rtc_cb->cb_info_data.tod_update.freq =
			be32_to_cpu(rtc_cb->cb_info_data.tod_update.freq);
		pr_info("RPC CALL -- TOD TIME UPDATE: ttick = %d\n"
			"stamp=%lld, freq = %d\n",
			rtc_cb->cb_info_data.tod_update.tick,
			rtc_cb->cb_info_data.tod_update.stamp,
			rtc_cb->cb_info_data.tod_update.freq);

		getnstimeofday(&ts);
		msmrtc_updateatsuspend(&ts);
		rtc_hctosys();
		getnstimeofday(&tv);
		/* Update the alarm information with the new time info. */
		alarm_update_timedelta(ts, tv);

	} else
		pr_err("%s: Unknown event EVENT=%x\n",
					__func__, rtc_cb->event);
}

static int msmrtc_cb_func(struct msm_rpc_client *client, void *buffer, int size)
{
	int rc = -1;
	struct rpc_request_hdr *recv = buffer;

	recv->xid = be32_to_cpu(recv->xid);
	recv->type = be32_to_cpu(recv->type);
	recv->rpc_vers = be32_to_cpu(recv->rpc_vers);
	recv->prog = be32_to_cpu(recv->prog);
	recv->vers = be32_to_cpu(recv->vers);
	recv->procedure = be32_to_cpu(recv->procedure);

	if (recv->procedure == RTC_EVENT_CB_PROC)
		process_cb_request((void *) (recv + 1));

	msm_rpc_start_accepted_reply(client, recv->xid,
				RPC_ACCEPTSTAT_SUCCESS);

	rc = msm_rpc_send_accepted_reply(client, 0);
	if (rc) {
		pr_debug("%s: sending reply failed: %d\n", __func__, rc);
		return rc;
	}

	return 0;
}

static int msmrtc_rpc_proc_args(struct msm_rpc_client *client, void *buff,
							void *data)
{
	struct msm_rtc *rtc_pdata = data;

	if (rtc_pdata->proc == RTC_CLIENT_INIT_PROC) {
		/* arguments passed to the client_init function */
		struct rtc_client_init_req {
			enum client_type client;
			uint32_t client_id_ptr;
			u8 client_id;
			enum processor_type processor;
		};
		struct rtc_client_init_req *req_1 = buff;

		req_1->client = cpu_to_be32(CLIENT_TYPE_INTERNAL);
		req_1->client_id_ptr = cpu_to_be32(0x1);
		req_1->client_id = (u8) cpu_to_be32(0x1);
		req_1->processor = cpu_to_be32(CLIENT_PROCESSOR_APP1);

		return sizeof(*req_1);

	} else if (rtc_pdata->proc == RTC_REQUEST_CB_PROC) {
		/* arguments passed to the request_cb function */
		struct rtc_event_req {
			u8 client_id;
			uint32_t rtc_cb_id;
		};
		struct rtc_event_req *req_2 = buff;

		req_2->client_id =  (u8) cpu_to_be32(rtc_pdata->client_id);
		req_2->rtc_cb_id = cpu_to_be32(RTC_CB_ID);

		return sizeof(*req_2);
	} else
		return 0;
}

static int msmrtc_rpc_proc_result(struct msm_rpc_client *client, void *buff,
							void *data)
{
	uint32_t result = -EINVAL;
	struct msm_rtc *rtc_pdata = data;

	if (rtc_pdata->proc == RTC_CLIENT_INIT_PROC) {
		/* process reply received from client_init function */
		uint32_t client_id_ptr;
		result = be32_to_cpu(*(uint32_t *)buff);
		buff += sizeof(uint32_t);
		client_id_ptr = be32_to_cpu(*(uint32_t *)(buff));
		buff += sizeof(uint32_t);
		if (client_id_ptr == 1)
			rtc_pdata->client_id = (u8)
					be32_to_cpu(*(uint32_t *)(buff));
		else {
			pr_debug("%s: Client-id not received from Modem\n",
								__func__);
			return -EINVAL;
		}
	} else if (rtc_pdata->proc == RTC_REQUEST_CB_PROC) {
		/* process reply received from request_cb function */
		result = be32_to_cpu(*(uint32_t *)buff);
	}

	if (result == ERR_NONE) {
		pr_debug("%s: RPC client reply for PROC=%x success\n",
					 __func__, rtc_pdata->proc);
		return 0;
	}

	pr_debug("%s: RPC client registration failed ERROR=%x\n",
						__func__, result);
	return -EINVAL;
}

static int msmrtc_setup_cb(struct msm_rtc *rtc_pdata)
{
	int rc;

	/* Register with the server with client specific info */
	rtc_pdata->proc = RTC_CLIENT_INIT_PROC;
	rc = msm_rpc_client_req(rtc_pdata->rpc_client, RTC_CLIENT_INIT_PROC,
				msmrtc_rpc_proc_args, rtc_pdata,
				msmrtc_rpc_proc_result, rtc_pdata, -1);
	if (rc) {
		pr_debug("%s: RPC client registration for PROC:%x failed\n",
					__func__, RTC_CLIENT_INIT_PROC);
		return rc;
	}

	/* Register with server for the callback event */
	rtc_pdata->proc = RTC_REQUEST_CB_PROC;
	rc = msm_rpc_client_req(rtc_pdata->rpc_client, RTC_REQUEST_CB_PROC,
				msmrtc_rpc_proc_args, rtc_pdata,
				msmrtc_rpc_proc_result, rtc_pdata, -1);
	if (rc) {
		pr_debug("%s: RPC client registration for PROC:%x failed\n",
					__func__, RTC_REQUEST_CB_PROC);
	}

	return rc;
}

static int __devinit
msmrtc_probe(struct platform_device *pdev)
{
	int rc;
	struct msm_rtc *rtc_pdata = NULL;
	struct rpcsvr_platform_device *rdev =
		container_of(pdev, struct rpcsvr_platform_device, base);
	uint32_t prog_version;


	if (pdev->id == (TIMEREMOTE_PROG_VER_1 & RPC_VERSION_MAJOR_MASK))
		prog_version = TIMEREMOTE_PROG_VER_1;
	else if (pdev->id == (TIMEREMOTE_PROG_VER_2 &
			      RPC_VERSION_MAJOR_MASK))
		prog_version = TIMEREMOTE_PROG_VER_2;
	else
		return -EINVAL;

	rtc_pdata = kzalloc(sizeof(*rtc_pdata), GFP_KERNEL);
	if (rtc_pdata == NULL) {
		dev_err(&pdev->dev,
			"%s: Unable to allocate memory\n", __func__);
		return -ENOMEM;
	}
	rtc_pdata->rpc_client = msm_rpc_register_client("rtc", rdev->prog,
				prog_version, 1, msmrtc_cb_func);
	if (IS_ERR(rtc_pdata->rpc_client)) {
		dev_err(&pdev->dev,
			 "%s: init RPC failed! VERS = %x\n", __func__,
					prog_version);
		rc = PTR_ERR(rtc_pdata->rpc_client);
		kfree(rtc_pdata);
		return rc;
	}

	/*
	 * Set up the callback client.
	 * For older targets this initialization will fail
	 */
	rc = msmrtc_setup_cb(rtc_pdata);
	if (rc)
		dev_dbg(&pdev->dev, "%s: Could not initialize RPC callback\n",
								__func__);

	rtc_pdata->rtcalarm_time = 0;
	platform_set_drvdata(pdev, rtc_pdata);

	rtc_pdata->rtc = rtc_device_register("msm_rtc",
				  &pdev->dev,
				  &msm_rtc_ops,
				  THIS_MODULE);
	if (IS_ERR(rtc_pdata->rtc)) {
		dev_err(&pdev->dev, "%s: Can't register RTC device (%ld)\n",
		       pdev->name, PTR_ERR(rtc_pdata->rtc));
		rc = PTR_ERR(rtc_pdata->rtc);
		goto fail_cb_setup;
	}

#ifdef CONFIG_RTC_SECURE_TIME_SUPPORT
	rtc_pdata->rtcsecure = rtc_device_register("msm_rtc_secure",
				  &pdev->dev,
				  &msm_rtc_ops_secure,
				  THIS_MODULE);

	if (IS_ERR(rtc_pdata->rtcsecure)) {
		dev_err(&pdev->dev,
			"%s: Can't register RTC Secure device (%ld)\n",
		       pdev->name, PTR_ERR(rtc_pdata->rtcsecure));
		rtc_device_unregister(rtc_pdata->rtc);
		rc = PTR_ERR(rtc_pdata->rtcsecure);
		goto fail_cb_setup;
	}
#endif

#ifdef CONFIG_RTC_ASYNC_MODEM_SUPPORT
	rtc_hctosys();
#endif

	return 0;

fail_cb_setup:
	msm_rpc_unregister_client(rtc_pdata->rpc_client);
	kfree(rtc_pdata);
	return rc;
}


#ifdef CONFIG_PM

static void
msmrtc_alarmtimer_expired(unsigned long _data,
				struct msm_rtc *rtc_pdata)
{
	pr_debug("%s: Generating alarm event (src %lu)\n",
	       rtc_pdata->rtc->name, _data);

	rtc_update_irq(rtc_pdata->rtc, 1, RTC_IRQF | RTC_AF);
	rtc_pdata->rtcalarm_time = 0;
}

static int
msmrtc_suspend(struct platform_device *dev, pm_message_t state)
{
	int rc, diff;
	struct rtc_time tm;
	unsigned long now;
	struct msm_rtc *rtc_pdata = platform_get_drvdata(dev);

	suspend_state.tick_at_suspend = msm_timer_get_sclk_time(NULL);
	if (rtc_pdata->rtcalarm_time) {
		rc = msmrtc_timeremote_read_time(&dev->dev, &tm);
		if (rc) {
			dev_err(&dev->dev,
				"%s: Unable to read from RTC\n", __func__);
			return rc;
		}
		rtc_tm_to_time(&tm, &now);
		diff = rtc_pdata->rtcalarm_time - now;
		if (diff <= 0) {
			msmrtc_alarmtimer_expired(1 , rtc_pdata);
			msm_pm_set_max_sleep_time(0);
			atomic_inc(&suspend_state.state);
			return 0;
		}
		msm_pm_set_max_sleep_time((int64_t)
			((int64_t) diff * NSEC_PER_SEC));
	} else
		msm_pm_set_max_sleep_time(0);
	atomic_inc(&suspend_state.state);
	return 0;
}

static int
msmrtc_resume(struct platform_device *dev)
{
	int rc, diff;
	struct rtc_time tm;
	unsigned long now;
	struct msm_rtc *rtc_pdata = platform_get_drvdata(dev);

	if (rtc_pdata->rtcalarm_time) {
		rc = msmrtc_timeremote_read_time(&dev->dev, &tm);
		if (rc) {
			dev_err(&dev->dev,
				"%s: Unable to read from RTC\n", __func__);
			return rc;
		}
		rtc_tm_to_time(&tm, &now);
		diff = rtc_pdata->rtcalarm_time - now;
		if (diff <= 0)
			msmrtc_alarmtimer_expired(2 , rtc_pdata);
	}
	suspend_state.tick_at_suspend = 0;
	atomic_dec(&suspend_state.state);
	return 0;
}
#else
#define msmrtc_suspend NULL
#define msmrtc_resume  NULL
#endif

static int __devexit msmrtc_remove(struct platform_device *pdev)
{
	struct msm_rtc *rtc_pdata = platform_get_drvdata(pdev);

	rtc_device_unregister(rtc_pdata->rtc);
#ifdef CONFIG_RTC_SECURE_TIME_SUPPORT
	rtc_device_unregister(rtc_pdata->rtcsecure);
#endif
	msm_rpc_unregister_client(rtc_pdata->rpc_client);
	kfree(rtc_pdata);

	return 0;
}

static struct platform_driver msmrtc_driver = {
	.probe		= msmrtc_probe,
	.suspend	= msmrtc_suspend,
	.resume		= msmrtc_resume,
	.remove		= __devexit_p(msmrtc_remove),
	.driver	= {
		.name	= APP_TIMEREMOTE_PDEV_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init msmrtc_init(void)
{
	int rc;

	/*
	 * For backward compatibility, register multiple platform
	 * drivers with the RPC PROG_VERS to be supported.
	 *
	 * Explicit cast away of 'constness' for driver.name in order to
	 * initialize it here.
	 */
	snprintf((char *)msmrtc_driver.driver.name,
		 strlen(msmrtc_driver.driver.name)+1,
		 "rs%08x", TIMEREMOTE_PROG_NUMBER);
	pr_debug("RTC Registering with %s\n", msmrtc_driver.driver.name);

	rc = platform_driver_register(&msmrtc_driver);
	if (rc)
		pr_err("%s: platfrom_driver_register failed\n", __func__);

	return rc;
}

static void __exit msmrtc_exit(void)
{
	platform_driver_unregister(&msmrtc_driver);
}

module_init(msmrtc_init);
module_exit(msmrtc_exit);

MODULE_DESCRIPTION("RTC driver for Qualcomm MSM7x00a chipsets");
MODULE_AUTHOR("San Mehat <san@android.com>");
MODULE_LICENSE("GPL");
