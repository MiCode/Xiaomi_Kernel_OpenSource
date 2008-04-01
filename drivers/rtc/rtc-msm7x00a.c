/* drivers/rtc/rtc-msm7x00a.c
 *
 * Copyright (C) 2008 Google, Inc.
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
#include <linux/rtc.h>
#include <linux/msm_rpcrouter.h>

#include <mach/msm_rpcrouter.h>

#define RTC_DEBUG 0

extern void msm_pm_set_max_sleep_time(int64_t sleep_time_ns);

#if CONFIG_MSM_AMSS_VERSION >= 6350 || defined(CONFIG_ARCH_QSD8X50)
#define APP_TIMEREMOTE_PDEV_NAME "rs30000048:00010000"
#else
#define APP_TIMEREMOTE_PDEV_NAME "rs30000048:0da5b528"
#endif

#define TIMEREMOTE_PROCEEDURE_SET_JULIAN	6
#define TIMEREMOTE_PROCEEDURE_GET_JULIAN	7

struct rpc_time_julian {
	uint32_t year;
	uint32_t month;
	uint32_t day;
	uint32_t hour;
	uint32_t minute;
	uint32_t second;
	uint32_t day_of_week;
};

static struct msm_rpc_endpoint *ep;
static struct rtc_device *rtc;
static unsigned long rtcalarm_time;

static int
msmrtc_timeremote_set_time(struct device *dev, struct rtc_time *tm)
{
	int rc;

	struct timeremote_set_julian_req {
		struct rpc_request_hdr hdr;
		uint32_t opt_arg;

		struct rpc_time_julian time;
	} req;

	struct timeremote_set_julian_rep {
		struct rpc_reply_hdr hdr;
	} rep;

	if (tm->tm_year < 1900)
		tm->tm_year += 1900;

	if (tm->tm_year < 1970)
		return -EINVAL;

#if RTC_DEBUG
	printk(KERN_DEBUG "%s: %.2u/%.2u/%.4u %.2u:%.2u:%.2u (%.2u)\n",
	       __func__, tm->tm_mon, tm->tm_mday, tm->tm_year,
	       tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_wday);
#endif

	req.opt_arg = cpu_to_be32(1);
	req.time.year = cpu_to_be32(tm->tm_year);
	req.time.month = cpu_to_be32(tm->tm_mon + 1);
	req.time.day = cpu_to_be32(tm->tm_mday);
	req.time.hour = cpu_to_be32(tm->tm_hour);
	req.time.minute = cpu_to_be32(tm->tm_min);
	req.time.second = cpu_to_be32(tm->tm_sec);
	req.time.day_of_week = cpu_to_be32(tm->tm_wday);


	rc = msm_rpc_call_reply(ep, TIMEREMOTE_PROCEEDURE_SET_JULIAN,
				&req, sizeof(req),
				&rep, sizeof(rep),
				5 * HZ);
	return rc;
}

static int
msmrtc_timeremote_read_time(struct device *dev, struct rtc_time *tm)
{
	int rc;

	struct timeremote_get_julian_req {
		struct rpc_request_hdr hdr;
		uint32_t julian_time_not_null;
	} req;

	struct timeremote_get_julian_rep {
		struct rpc_reply_hdr hdr;
		uint32_t opt_arg;
		struct rpc_time_julian time;
	} rep;

	req.julian_time_not_null = cpu_to_be32(1);

	rc = msm_rpc_call_reply(ep, TIMEREMOTE_PROCEEDURE_GET_JULIAN,
				&req, sizeof(req),
				&rep, sizeof(rep),
				5 * HZ);
	if (rc < 0)
		return rc;

	if (!be32_to_cpu(rep.opt_arg)) {
		printk(KERN_ERR "%s: No data from RTC\n", __func__);
		return -ENODATA;
	}

	tm->tm_year = be32_to_cpu(rep.time.year);
	tm->tm_mon = be32_to_cpu(rep.time.month);
	tm->tm_mday = be32_to_cpu(rep.time.day);
	tm->tm_hour = be32_to_cpu(rep.time.hour);
	tm->tm_min = be32_to_cpu(rep.time.minute);
	tm->tm_sec = be32_to_cpu(rep.time.second);
	tm->tm_wday = be32_to_cpu(rep.time.day_of_week);

#if RTC_DEBUG
	printk(KERN_DEBUG "%s: %.2u/%.2u/%.4u %.2u:%.2u:%.2u (%.2u)\n",
	       __func__, tm->tm_mon, tm->tm_mday, tm->tm_year,
	       tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_wday);
#endif

	tm->tm_year -= 1900;	/* RTC layer expects years to start at 1900 */
	tm->tm_mon--;		/* RTC layer expects mons to be 0 based */

	if (rtc_valid_tm(tm) < 0) {
		dev_err(dev, "retrieved date/time is not valid.\n");
		rtc_time_to_tm(0, tm);
	}

	return 0;
}


static int
msmrtc_virtual_alarm_set(struct device *dev, struct rtc_wkalrm *a)
{
	unsigned long now = get_seconds();

	if (!a->enabled) {
		rtcalarm_time = 0;
		return 0;
	} else
		rtc_tm_to_time(&a->time, &rtcalarm_time);

	if (now > rtcalarm_time) {
		printk(KERN_ERR "%s: Attempt to set alarm in the past\n",
		       __func__);
		rtcalarm_time = 0;
		return -EINVAL;
	}

	return 0;
}

static struct rtc_class_ops msm_rtc_ops = {
	.read_time	= msmrtc_timeremote_read_time,
	.set_time	= msmrtc_timeremote_set_time,
	.set_alarm	= msmrtc_virtual_alarm_set,
};

static void
msmrtc_alarmtimer_expired(unsigned long _data)
{
#if RTC_DEBUG
	printk(KERN_DEBUG "%s: Generating alarm event (src %lu)\n",
	       rtc->name, _data);
#endif
	rtc_update_irq(rtc, 1, RTC_IRQF | RTC_AF);
	rtcalarm_time = 0;
}

static int
msmrtc_probe(struct platform_device *pdev)
{
	struct rpcsvr_platform_device *rdev =
		container_of(pdev, struct rpcsvr_platform_device, base);

	ep = msm_rpc_connect(rdev->prog, rdev->vers, 0);
	if (IS_ERR(ep)) {
		printk(KERN_ERR "%s: init rpc failed! rc = %ld\n",
		       __func__, PTR_ERR(ep));
		return PTR_ERR(ep);
	}

	rtc = rtc_device_register("msm_rtc",
				  &pdev->dev,
				  &msm_rtc_ops,
				  THIS_MODULE);
	if (IS_ERR(rtc)) {
		printk(KERN_ERR "%s: Can't register RTC device (%ld)\n",
		       pdev->name, PTR_ERR(rtc));
		return PTR_ERR(rtc);
	}
	return 0;
}


static unsigned long msmrtc_get_seconds(void)
{
	struct rtc_time tm;
	unsigned long now;

	msmrtc_timeremote_read_time(NULL, &tm);
	rtc_tm_to_time(&tm, &now);
	return now;
}

static int
msmrtc_suspend(struct platform_device *dev, pm_message_t state)
{
	if (rtcalarm_time) {
		unsigned long now = msmrtc_get_seconds();
		int diff = rtcalarm_time - now;
		if (diff <= 0) {
			msmrtc_alarmtimer_expired(1);
			msm_pm_set_max_sleep_time(0);
			return 0;
		}
		msm_pm_set_max_sleep_time((int64_t) ((int64_t) diff * NSEC_PER_SEC));
	} else
		msm_pm_set_max_sleep_time(0);
	return 0;
}

static int
msmrtc_resume(struct platform_device *dev)
{
	if (rtcalarm_time) {
		unsigned long now = msmrtc_get_seconds();
		int diff = rtcalarm_time - now;
		if (diff <= 0)
			msmrtc_alarmtimer_expired(2);
	}
	return 0;
}

static struct platform_driver msmrtc_driver = {
	.probe		= msmrtc_probe,
	.suspend	= msmrtc_suspend,
	.resume		= msmrtc_resume,
	.driver	= {
		.name	= APP_TIMEREMOTE_PDEV_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init msmrtc_init(void)
{
	rtcalarm_time = 0;
	return platform_driver_register(&msmrtc_driver);
}

module_init(msmrtc_init);

MODULE_DESCRIPTION("RTC driver for Qualcomm MSM7x00a chipsets");
MODULE_AUTHOR("San Mehat <san@android.com>");
MODULE_LICENSE("GPL");
