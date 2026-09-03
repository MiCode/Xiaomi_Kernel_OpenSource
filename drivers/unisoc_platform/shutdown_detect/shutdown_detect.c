// SPDX-License-Identifier: GPL-2.0
// SPDX-FileCopyrightText: 2023 Unisoc (Shanghai) Technologies Co., Ltd

#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>	/* error codes */
#include <linux/fs.h>		/* everything... */
#include <linux/kernel.h>	/* pr_notice() */
#include <linux/kmsg_dump.h>
#include <linux/math.h>
#include <linux/proc_fs.h>
#include <linux/reboot.h>
#include <linux/rtc.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/sysrq.h>
#include <linux/types.h>	/* size_t */
#include <linux/time.h>
#include <../drivers/unisoc_platform/sysdump/unisoc_sysdump.h>

#define SHUTDOWN_STAGE_KERNEL 20
#define SHUTDOWN_STAGE_INIT 30
#define SHUTDOWN_STAGE_INIT_POFF 70
#define SHUTDOWN_STAGE_SYSTEMSERVER 40
#define SHUTDOWN_TIMEOUT_UMOUNT 31
#define SHUTDOWN_TIMEOUT_VOLUME 32
#define SHUTDOWN_TIMEOUT_SUBSYSTEM 43
#define SHUTDOWN_TIMEOUT_RADIOS 44
#define SHUTDOWN_TIMEOUT_PM 45
#define SHUTDOWN_TIMEOUT_AM 46
#define SHUTDOWN_TIMEOUT_BC 47
#define SHUTDOWN_RUS_MIN 255
#define SHUTDOWN_TOTAL_TIME_MIN 60
#define SHUTDOWN_DEFAULT_NATIVE_TIME 60
#define SHUTDOWN_DEFAULT_JAVA_TIME 60
#define SHUTDOWN_DEFAULT_TOTAL_TIME 150
#define SHUTDOWN_INCREASE_TIME 5

bool is_shutdows;
bool shutdown_detect_enable = true;
long shutdown_phase;
int shutdown_detect_started;
int gnativetimeout = 30;
int gjavatimeout = 30;
int gtotaltimeout = 90;
char time[32];
char reason[32];
struct timespec64 ts;
struct rtc_time tm;
struct task_struct *shd_complete_monitor;
struct task_struct *shutdown_task;

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "shutdown_detect_check: " fmt

static int shutdown_kthread(void *data)
{
	kernel_power_off();
	return 0;
}

static void SEQ_printf(struct seq_file *m, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);

	if (m)
		seq_vprintf(m, fmt, args);
	else
		vprintk(fmt, args);

	va_end(args);
}

static int shutdown_detect_show(struct seq_file *m, void *v)
{
	SEQ_printf(m, "=== shutdown_detect controller ===\n");
	SEQ_printf(m, "0: shutdown_detect abort\n");
	SEQ_printf(m, "20: shutdown_detect systemcall reboot phase\n");
	SEQ_printf(m, "30: shutdown_detect init reboot phase\n");
	SEQ_printf(m, "40: shutdown_detect system server reboot phase\n");
	SEQ_printf(m, "=== shutdown_detect controller ===\n\n");
	SEQ_printf(m, "=== shutdown_detect: shutdown phase: %u\n", shutdown_phase);
	return 0;
}

static int shutdown_detect_open(struct inode *inode, struct file *file)
{
	return single_open(file, shutdown_detect_show, inode->i_private);
}

static int shutdown_detect_func(void *dummy)
{
	msleep(gtotaltimeout * 1000);
	pr_err("call sysrq show block and cpu thread. BUG\n");
	handle_sysrq('w');
	handle_sysrq('l');
	#ifndef CONFIG_SPRD_DEBUG
		if (is_shutdows) {
			if (shutdown_phase >= SHUTDOWN_STAGE_INIT) {
				rtc_time64_to_tm(ts.tv_sec, &tm);
				snprintf(time, 32, "%04d-%02d-%02d_%02d:%02d:%02d",
				tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec);
				strncpy(reason, "usershutdownerror", sizeof(reason));
				shutdown_save_log_to_partition(time, reason);
			}
			pr_err("shutdown_detect: shutdown or reboot? reboot\n");
			if (shutdown_task)
				wake_up_process(shutdown_task);
		}
	#endif
	#ifdef CONFIG_SPRD_DEBUG
		pr_err("shutdown_detect: shutdown or reboot? reboot\n");
		rtc_time64_to_tm(ts.tv_sec, &tm);
		snprintf(time, 32, "%04d-%02d-%02d_%02d:%02d:%02d",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
		if (is_shutdows) {
			strncpy(reason, "userdebugpowerofftimeout", sizeof(reason));
			shutdown_save_log_to_partition(time, reason);
		} else {
			strncpy(reason, "userdebugreboottimeout", sizeof(reason));
			shutdown_save_log_to_partition(time, reason);
			panic("userdebug version reboottime_out panic now\n");
		}
	#endif
	return 0;
}

static int shutdown_thread_check(void)
{
	if (!shutdown_task && is_shutdows) {
		shutdown_task = kthread_create(shutdown_kthread, NULL, "shutdown_kthread");
		if (IS_ERR(shutdown_task)) {
			pr_err("create shutdown thread fail, will panic()\n");
			msleep(60*1000);
			panic("create shutdown thread fail make panic\n");
		}
	}
	return 0;
}

static ssize_t shutdown_detect_trigger(struct file *filp, const char *ubuf,
									   size_t cnt, loff_t *data)
{
	char buf[64];
	long val = 0;
	int ret = 0;
	int rtc1 = 10;
	int rtc2 = 9;
	static ktime_t shutdown_start_time;
	static ktime_t shutdown_kernel_start_time;
	static ktime_t shutdown_init_start_time;
	static ktime_t shutdown_systemserver_start_time;
	int forrts = int_pow(rtc1, rtc2);
	unsigned int temp = SHUTDOWN_DEFAULT_TOTAL_TIME;

	if (shutdown_detect_enable == false)
		return -EPERM;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	ret = kstrtoul(buf, 0, (unsigned long *)&val);

	if (ret < 0)
		return ret;

	if (val == SHUTDOWN_STAGE_INIT_POFF) {
		is_shutdows = true;
		val = SHUTDOWN_STAGE_INIT;
	}

	if (val > SHUTDOWN_RUS_MIN) {
		gnativetimeout = val % 16;
		gjavatimeout = ((val - gnativetimeout) % 256) / 16;
		temp = val / 256;
		gtotaltimeout = (temp < SHUTDOWN_TOTAL_TIME_MIN) ? SHUTDOWN_TOTAL_TIME_MIN : temp;
		#if IS_ENABLED(CONFIG_SPRD_DEBUG)
			gnativetimeout += SHUTDOWN_INCREASE_TIME;
			gjavatimeout += SHUTDOWN_INCREASE_TIME;
		#endif
		pr_info("shutdown_detect_func_start rus val %ld %d %d %d\n",
				 val, gnativetimeout, gjavatimeout, gtotaltimeout);

		return cnt;
	}

	switch (val) {
	case 0:
		if (shutdown_detect_started) {
			shutdown_detect_started = 0;
			shutdown_phase = 0;
		}
		shutdown_detect_enable = false;
		pr_err("shutdown_detect: abort shutdown detect\n");
		break;
	case SHUTDOWN_STAGE_KERNEL:
		shutdown_kernel_start_time = ktime_get();
		if ((shutdown_kernel_start_time/forrts -
			 shutdown_init_start_time/forrts) > gnativetimeout) {
			pr_err("shutdown_detect_timeout: timeout happened in reboot.cpp\n");
			rtc_time64_to_tm(ts.tv_sec, &tm);
			snprintf(time, 32, "%04d-%02d-%02d_%02d:%02d:%02d",
			tm.tm_year + 1900, tm.tm_mon + 1,
			tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
			if (is_shutdows)
				strncpy(reason, "initpowerofftimeout", sizeof(reason));
			else
				strncpy(reason, "initreboottimeout", sizeof(reason));
			shutdown_save_log_to_partition(time, reason);
		}
		shutdown_phase = val;
		pr_err("shutdown_detect_phase: shutdown  current phase systemcall\n");

		break;
	case SHUTDOWN_STAGE_INIT:
		if (!shutdown_detect_started) {
			shutdown_detect_started = true;
			shutdown_init_start_time = ktime_get();
			shutdown_start_time = shutdown_init_start_time;
			shd_complete_monitor =
			kthread_run(shutdown_detect_func, NULL, "shutdown_detect_thread");
			if (IS_ERR(shd_complete_monitor)) {
				ret = PTR_ERR(shd_complete_monitor);
				pr_err("shutdown_detect: cannot start thread: %d\n", ret);
			}
		} else {
			shutdown_init_start_time = ktime_get();
			if ((shutdown_init_start_time/forrts -
				 shutdown_systemserver_start_time/forrts) >
			    gjavatimeout && shutdown_systemserver_start_time != 0) {
				pr_err("shutdown_detect_timeout:timeout happened in system_server stage\n");
				rtc_time64_to_tm(ts.tv_sec, &tm);
				snprintf(time, 32, "%04d-%02d-%02d_%02d:%02d:%02d",
				tm.tm_year + 1900, tm.tm_mon + 1,
				tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
				if (is_shutdows)
					strncpy(reason, "sysframeworkpowerofftimeout",
							sizeof(reason));
				else
					strncpy(reason, "sysframeworkreboottimeout",
							sizeof(reason));
				shutdown_save_log_to_partition(time, reason);
			}
		}
		shutdown_phase = val;
		pr_err("shutdown_detect_phase: shutdown current phase init\n");
		break;
	case SHUTDOWN_TIMEOUT_UMOUNT:
		pr_err("shutdown_detect_timeout: umount timeout\n");
		break;
	case SHUTDOWN_TIMEOUT_VOLUME:
		pr_err("shutdown_detect_timeout: volume shutdown timeout\n");
		break;
	case SHUTDOWN_STAGE_SYSTEMSERVER:
		shutdown_systemserver_start_time = ktime_get();
		if (!shutdown_detect_started) {
			shutdown_detect_started = true;
			shutdown_start_time = shutdown_systemserver_start_time;
			shd_complete_monitor =
			kthread_run(shutdown_detect_func, NULL, "shutdown_detect_thread");
		}
		shutdown_phase = val;
		pr_err("shutdown_detect_phase: shutdown current phase systemserver\n");
		break;
	case SHUTDOWN_TIMEOUT_SUBSYSTEM:
		pr_err("shutdown_detect_timeout: ShutdownSubSystem timeout\n");
		break;
	case SHUTDOWN_TIMEOUT_RADIOS:
		pr_err("shutdown_detect_timeout: ShutdownRadios timeout\n");
		break;
	case SHUTDOWN_TIMEOUT_PM:
		pr_err("shutdown_detect_timeout: ShutdownPackageManager timeout\n");
		break;
	case SHUTDOWN_TIMEOUT_AM:
		pr_err("shutdown_detect_timeout: ShutdownActivityManager timeout\n");
		break;
	case SHUTDOWN_TIMEOUT_BC:
		pr_err("shutdown_detect_timeout: ShutdownBroadcast timeout\n");
		break;
	default:
		break;
	}
	shutdown_thread_check();

	return cnt;
}

static const struct proc_ops shutdown_detect_fops = {
	.proc_open       = shutdown_detect_open,
	.proc_write      = shutdown_detect_trigger,
	.proc_read       = seq_read,
	.proc_lseek      = seq_lseek,
	.proc_release    = single_release,
};

static int __init init_shutdown_detect_ctrl(void)
{
	struct proc_dir_entry *pe;

	pr_err("shutdown detect:register shutdown_detect interface\n");
	pe = proc_create("shutdown_detect", 0664, NULL, &shutdown_detect_fops);
	if (!pe) {
		pr_err("shutdown detect:Failed to register shutdown_detect interface\n");
		return -ENOMEM;
	}
	return 0;
}

device_initcall(init_shutdown_detect_ctrl);
MODULE_LICENSE("GPL");
