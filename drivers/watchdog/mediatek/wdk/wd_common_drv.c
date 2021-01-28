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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/rtc.h>
#include <linux/cpu.h>
#include <linux/jiffies.h>
#ifdef CONFIG_MTK_AEE_IPANIC
#include <mt-plat/aee.h>
#endif
#ifdef CONFIG_MTK_AEE_IPANIC
#include <mt-plat/mtk_ram_console.h>
#endif
#include <linux/tick.h>
#include <mt-plat/mtk_gpt.h>
#include <ext_wd_drv.h>
#include <mt-plat/mtk_wd_api.h>
#include <linux/seq_file.h>
#include <uapi/linux/sched/types.h>
#include <linux/sched/clock.h>
#include <linux/suspend.h>

/*************************************************************************
 * Feature configure region
 *************************************************************************/
#define __ENABLE_WDT_SYSFS__
#define __ENABLE_WDT_AT_INIT__
#define KWDT_KICK_TIME_ALIGN

/* ------------------------------------------------------------------------ */
#define PFX "wdk: "
#define DEBUG_WDK 0
#if DEBUG_WDK
#define dbgmsg(msg...) pr_debug(PFX msg)
#else
#define dbgmsg(...)
#endif
#define msg(msg...) pr_info(PFX msg)
#define warnmsg(msg...) pr_info(PFX msg)
#define errmsg(msg...) pr_notice(PFX msg)

#define WK_MAX_MSG_SIZE (128)
#define MIN_KICK_INTERVAL	 1
#define MAX_KICK_INTERVAL	30
#define SOFT_KICK_RANGE     (100*1000) // 100ms
#define	MRDUMP_SYSRESETB	0
#define	MRDUMP_EINTRST		1
#define PROC_WK "wdk"
#define	PROC_MRDUMP_RST	"mrdump_rst"

__weak void mtk_wdt_cpu_callback(struct task_struct *wk_tsk, int hotcpu,
				     int kicker_init)
{
}

__weak void mtk_timer_clkevt_aee_dump(void)
{
}

__weak void timer_list_aee_dump(int exclude_cpus)
{
}

static int kwdt_thread(void *arg);
static int start_kicker(void);

static int g_kicker_init;
static int debug_sleep;

static DEFINE_SPINLOCK(lock);

#define CPU_NR (nr_cpu_ids)
struct task_struct *wk_tsk[16] = { 0 };	/* max cpu 16 */
static unsigned int wk_tsk_bind[16] = { 0 };	/* max cpu 16 */
static unsigned long long wk_tsk_bind_time[16] = { 0 };	/* max cpu 16 */
static unsigned long long wk_tsk_kick_time[16] = { 0 };	/* max cpu 16 */
static char wk_tsk_buf[128] = { 0 };

static unsigned long kick_bit;
static unsigned long rtc_update;

enum ext_wdt_mode g_wk_wdt_mode = WDT_DUAL_MODE;
static struct wd_api *g_wd_api;
static int g_kinterval = -1;
static int g_timeout = -1;
static int g_need_config;
static int wdt_start;
static int g_enable = 1;
static struct work_struct wdk_work;
static struct workqueue_struct *wdk_workqueue;
static unsigned int lasthpg_act;
static unsigned int lasthpg_cpu;
static unsigned long long lasthpg_t;
static unsigned long long lastsuspend_t;
static unsigned long long lastresume_t;
static struct notifier_block wdt_pm_nb;
#ifdef KWDT_KICK_TIME_ALIGN
static unsigned long g_nxtKickTime;
#endif
static int g_hang_detected;

static char cmd_buf[256];


static int wk_proc_cmd_read(struct seq_file *s, void *v)
{
	seq_printf(s, "mode interval timeout enable\n%-4d %-9d %-8d %-7d\n",
		g_wk_wdt_mode, g_kinterval, g_timeout, g_enable);
	return 0;
}

static int wk_proc_cmd_open(struct inode *inode, struct file *file)
{
	return single_open(file, wk_proc_cmd_read, NULL);
}

static ssize_t wk_proc_cmd_write(struct file *file, const char *buf,
	size_t count, loff_t *data)
{
	int ret;
	int timeout;
	int mode;
	int kinterval;
	int en;	/* enable or disable ext wdt 1<-->enable 0<-->disable */
	struct wd_api *my_wd_api = NULL;

	ret = get_wd_api(&my_wd_api);
	if (ret)
		pr_debug("get public api error in wd common driver %d", ret);

	if (count == 0)
		return -1;

	if (count > 255)
		count = 255;

	ret = copy_from_user(cmd_buf, buf, count);
	if (ret < 0)
		return -1;

	cmd_buf[count] = '\0';

	pr_debug("Write %s\n", cmd_buf);

	ret = sscanf(cmd_buf, "%d %d %d %d %d", &mode,
		&kinterval, &timeout, &debug_sleep, &en);

	pr_debug("[wdk] mode=%d interval=%d timeout=%d enable =%d\n",
		mode, kinterval, timeout, en);

	if (timeout < kinterval) {
		pr_info("Interval(%d) need smaller than timeout value(%d)\n",
		       kinterval, timeout);
		return -1;
	}

	if ((timeout < MIN_KICK_INTERVAL) || (timeout > MAX_KICK_INTERVAL)) {
		pr_info("The timeout(%d) is invalid (%d - %d)\n", kinterval,
			MIN_KICK_INTERVAL, MAX_KICK_INTERVAL);
		return -1;
	}

	if ((kinterval < MIN_KICK_INTERVAL) ||
		(kinterval > MAX_KICK_INTERVAL)) {
		pr_info("The interval(%d) is invalid (%d - %d)\n", kinterval,
			MIN_KICK_INTERVAL, MAX_KICK_INTERVAL);
		return -1;
	}

	if (!((mode == WDT_IRQ_ONLY_MODE) ||
	      (mode == WDT_HW_REBOOT_ONLY_MODE) || (mode == WDT_DUAL_MODE))) {
		pr_info("Tha watchdog kicker wdt mode is not correct %d\n",
			mode);
		return -1;
	}

	if (en == 1) {
		mtk_wdt_enable(WK_WDT_EN);
#ifdef CONFIG_LOCAL_WDT
		local_wdt_enable(WK_WDT_EN);
		pr_debug("[wdk] enable local wdt\n");
#endif
		pr_debug("[wdk] enable wdt\n");
	}
	if (en == 0) {
		mtk_wdt_enable(WK_WDT_DIS);
#ifdef CONFIG_LOCAL_WDT
		local_wdt_enable(WK_WDT_DIS);
		pr_debug("[wdk] disable local wdt\n");
#endif
		pr_debug("[wdk] disable wdt\n");
	}

	spin_lock(&lock);

	g_enable = en;
	g_kinterval = kinterval;

	g_wk_wdt_mode = mode;
	if (mode == 1) {
		/* irq mode only useful to 75 */
		mtk_wdt_swsysret_config(0x20000000, 1);
		pr_debug("[wdk] use irq mod\n");
	} else if (mode == 0) {
		/* reboot mode only useful to 75 */
		mtk_wdt_swsysret_config(0x20000000, 0);
		pr_debug("[wdk] use reboot mod\n");
	} else if (mode == 2)
		my_wd_api->wd_set_mode(WDT_IRQ_ONLY_MODE);
	else
		pr_debug("[wdk] mode err\n");

	g_timeout = timeout;
	if (mode != 2)
		g_need_config = 1;

	spin_unlock(&lock);

	return count;
}

static int mrdump_proc_cmd_read(struct seq_file *s, void *v)
{
	return 0;
}

static int mrdump_proc_cmd_open(struct inode *inode, struct file *file)
{
	return single_open(file, mrdump_proc_cmd_read, NULL);
}

static ssize_t mrdump_proc_cmd_write(struct file *file,
	const char *buf, size_t count, loff_t *data)
{
	int ret = 0;
	int mrdump_rst_source;
	int en, mode;/* enable or disable ext wdt 1<-->enable 0<-->disable */
	char mrdump_cmd_buf[256];
	struct wd_api *my_wd_api = NULL;

	ret = get_wd_api(&my_wd_api);
	if (ret)
		pr_debug("get public api error in wd common driver %d", ret);

	if (count == 0)
		return -1;

	if (count > 255)
		count = 255;

	ret = copy_from_user(mrdump_cmd_buf, buf, count);
	if (ret < 0)
		return -1;

	mrdump_cmd_buf[count] = '\0';

	dbgmsg("Write %s\n", mrdump_cmd_buf);

	ret = sscanf(mrdump_cmd_buf, "%d %d %d",
		&mrdump_rst_source, &mode, &en);
	if (ret != 3)
		pr_debug("%s: expect 3 numbers\n", __func__);

	pr_debug("[MRDUMP] rst_source=%d mode=%d enable=%d\n",
		mrdump_rst_source, mode, en);

	if (mrdump_rst_source > 1) {
		errmsg("mrdump_rst_source(%d) value need smaller than 2\n",
		       mrdump_rst_source);
		return -1;
	}

	if (mode > 1) {
		errmsg("mrdump_rst_mode(%d) value should be smaller than 2\n",
			mode);
		return -1;
	}

	spin_lock(&lock);
	if (mrdump_rst_source == MRDUMP_SYSRESETB) {
		ret = my_wd_api->wd_debug_key_sysrst_config(en, mode);
	} else if (mrdump_rst_source == MRDUMP_EINTRST) {
		ret = my_wd_api->wd_debug_key_eint_config(en, mode);
	} else {
		pr_debug("[MRDUMP] invalid mrdump_rst_source\n");
		ret = -1;
	}
	spin_unlock(&lock);

	if (ret == 0)
		pr_debug("[MRDUMP] MRDUMP External success\n");
	else
		pr_debug("[MRDUMP] MRDUMP External key not support!\n");

	return count;
}

static int start_kicker_thread_with_default_setting(void)
{
	int ret = 0;

	spin_lock(&lock);

	g_kinterval = 15;	/* default interval: 15s, timeout between 15-30s */

	g_need_config = 0;/* Note, we DO NOT want to call configure function */

	wdt_start = 1;		/* Start once only */
	rtc_update = jiffies;	/* update rtc_update time base*/

	spin_unlock(&lock);
	start_kicker();

	pr_debug("[wdk] %s done\n", __func__);
	return ret;
}

static unsigned int cpus_kick_bit;
void wk_start_kick_cpu(int cpu)
{
	if (IS_ERR(wk_tsk[cpu])) {
		pr_debug("[wdk] wk_task[%d] is NULL\n", cpu);
	} else {
		kthread_bind(wk_tsk[cpu], cpu);
		pr_info("[wdk] bind thread %d to cpu %d\n",
			wk_tsk[cpu]->pid, cpu);
		wake_up_process(wk_tsk[cpu]);
	}
}

void dump_wdk_bind_info(void)
{
	int i = 0;

#ifdef CONFIG_MTK_AEE_IPANIC
	aee_sram_fiq_log("\n");
#endif
	snprintf(wk_tsk_buf, sizeof(wk_tsk_buf),
		"[wdk] dump at %lld\n", sched_clock());
#ifdef CONFIG_MTK_AEE_IPANIC
	aee_sram_fiq_log(wk_tsk_buf);
#endif
	snprintf(wk_tsk_buf, sizeof(wk_tsk_buf),
		"[wdk]kick_bits: 0x%x, check_bits: 0x%x\n",
		get_kick_bit(), get_check_bit());
#ifdef CONFIG_MTK_AEE_IPANIC
	aee_sram_fiq_log(wk_tsk_buf);
#endif
	for (i = 0; i < CPU_NR; i++) {
		if (wk_tsk[i] != NULL) {
			/*
			 * pr_info("[wdk]CPU %d, %d, %lld, %lu, %d, %ld\n",
			 *	i, wk_tsk_bind[i], wk_tsk_bind_time[i],
			 *	wk_tsk[i]->cpus_allowed.bits[0],
			 *	wk_tsk[i]->on_rq, wk_tsk[i]->state);
			 */
			memset(wk_tsk_buf, 0, sizeof(wk_tsk_buf));
			snprintf(wk_tsk_buf, sizeof(wk_tsk_buf),
				"[wdk]CPU %d, %d, %lld, %lu, %d, %ld, %lld\n",
				i, wk_tsk_bind[i], wk_tsk_bind_time[i],
				wk_tsk[i]->cpus_allowed.bits[0],
				wk_tsk[i]->on_rq, wk_tsk[i]->state,
				wk_tsk_kick_time[i]);
#ifdef CONFIG_MTK_AEE_IPANIC
			aee_sram_fiq_log(wk_tsk_buf);
#endif
		}
	}
#ifdef CONFIG_MTK_AEE_IPANIC
	aee_sram_fiq_log("\n");
#endif
	mtk_timer_clkevt_aee_dump();
	tick_broadcast_mtk_aee_dump();
	timer_list_aee_dump(kick_bit);
}

void kicker_cpu_bind(int cpu)
{
	if (IS_ERR(wk_tsk[cpu]))
		pr_debug("[wdk]wk_task[%d] is NULL\n", cpu);
	else {
		/* kthread_bind(wk_tsk[cpu], cpu); */
		WARN_ON_ONCE(set_cpus_allowed_ptr(wk_tsk[cpu],
			cpumask_of(cpu)) < 0);
		wake_up_process(wk_tsk[cpu]);
		wk_tsk_bind[cpu] = 1;
		wk_tsk_bind_time[cpu] = sched_clock();
	}
}

void wk_cpu_update_bit_flag(int cpu, int plug_status)
{
	if (plug_status == 1) {	/* plug on */
		spin_lock(&lock);
		cpus_kick_bit |= (1 << cpu);
		kick_bit = 0;
		lasthpg_cpu = cpu;
		lasthpg_act = plug_status;
		lasthpg_t = sched_clock();
		spin_unlock(&lock);
	}
	if (plug_status == 0) {	/* plug off */
		spin_lock(&lock);
		cpus_kick_bit &= (~(1 << cpu));
		kick_bit = 0;
		lasthpg_cpu = cpu;
		lasthpg_act = plug_status;
		lasthpg_t = sched_clock();
		wk_tsk_bind[cpu] = 0;
		spin_unlock(&lock);
	}
}

unsigned int wk_check_kick_bit(void)
{
	return cpus_kick_bit;
}

static const struct file_operations wk_proc_cmd_fops = {
	.owner = THIS_MODULE,
	.open = wk_proc_cmd_open,
	.read = seq_read,
	.write = wk_proc_cmd_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations mrdump_rst_proc_cmd_fops = {
	.owner = THIS_MODULE,
	.open = mrdump_proc_cmd_open,
	.read = seq_read,
	.write = mrdump_proc_cmd_write,
	.llseek = seq_lseek,
	.release = single_release,
};

int wk_proc_init(void)
{

	struct proc_dir_entry *de = NULL;

	de = proc_create(PROC_WK, 0660, NULL, &wk_proc_cmd_fops);

	if (!de)
		pr_debug("[%s]: create /proc/wdk failed\n", __func__);

	de = proc_create(PROC_MRDUMP_RST, 0660, NULL,
		&mrdump_rst_proc_cmd_fops);
	if (!de)
		pr_debug("[%s]: create /proc/mrdump_rst failed\n", __func__);

	pr_debug("[wdk] Initialize proc\n");

/* de->read_proc = wk_proc_cmd_read; */
/* de->write_proc = wk_proc_cmd_write; */

	return 0;
}


void wk_proc_exit(void)
{

	remove_proc_entry(PROC_WK, NULL);

}

static void kwdt_print_utc(char *msg_buf, int msg_buf_size)
{
	struct rtc_time tm;
	struct timeval tv = { 0 };
	/* android time */
	struct rtc_time tm_android;
	struct timeval tv_android = { 0 };

	do_gettimeofday(&tv);
	tv_android = tv;
	rtc_time_to_tm(tv.tv_sec, &tm);
	tv_android.tv_sec -= sys_tz.tz_minuteswest * 60;
	rtc_time_to_tm(tv_android.tv_sec, &tm_android);
	snprintf(msg_buf, msg_buf_size,
		"[thread:%d][RT:%lld] %d-%02d-%02d %02d:%02d:%02d.%u UTC;"
		"android time %d-%02d-%02d %02d:%02d:%02d.%03d\n",
		current->pid, sched_clock(), tm.tm_year + 1900, tm.tm_mon + 1,
		tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
		(unsigned int)tv.tv_usec, tm_android.tm_year + 1900,
		tm_android.tm_mon + 1, tm_android.tm_mday, tm_android.tm_hour,
		tm_android.tm_min, tm_android.tm_sec,
		(unsigned int)tv_android.tv_usec);
}
static void kwdt_process_kick(int local_bit, int cpu,
				unsigned long curInterval, char msg_buf[])
{
	unsigned int dump_timeout = 0, tmp = 0;
	void __iomem *apxgpt_base = 0;

	local_bit = kick_bit;
	if ((local_bit & (1 << cpu)) == 0) {
		/* pr_debug("[wdk] set kick_bit\n"); */
		local_bit |= (1 << cpu);
		/* aee_rr_rec_wdk_kick_jiffies(jiffies); */
	} else if (g_hang_detected == 0) {
		g_hang_detected = 1;
		dump_timeout = 1;
	}

	/*
	 * do not print message with spinlock held to
	 *  avoid bulk of delayed printk happens here
	 */
	wk_tsk_kick_time[cpu] = sched_clock();
	snprintf(msg_buf, WK_MAX_MSG_SIZE,
	 "[wdk-c] cpu=%d,lbit=0x%x,cbit=0x%x,%d,%d,%lld,%lld,%lld,[%lld,%ld]\n",
	 cpu, local_bit, wk_check_kick_bit(), lasthpg_cpu, lasthpg_act,
	 lasthpg_t, lastsuspend_t, lastresume_t, wk_tsk_kick_time[cpu],
	 curInterval);

	if (local_bit == wk_check_kick_bit()) {
		msg_buf[5] = 'k';
		mtk_wdt_restart(WD_TYPE_NORMAL);/* for KICK external wdt */
		local_bit = 0;
	}

	kick_bit = local_bit;

	apxgpt_base = mtk_wdt_apxgpt_base();
	if (apxgpt_base) {
		/* "DB" signature */
		tmp = 0x4442 << 16;
		tmp |= (local_bit & 0xFF) << 8;
		tmp |= wk_check_kick_bit() & 0xFF;
		__raw_writel(tmp, apxgpt_base + 0x7c);
	}

	spin_unlock(&lock);

	/*
	 * [wdt-c]: mark local bit only.
	 * [wdt-k]: kick watchdog actaully, this log is more important thus
	 *	    using printk_deferred to ensure being printed.
	 */
	if (msg_buf[5] != 'k')
		pr_info("%s", msg_buf);
	else
		printk_deferred("%s", msg_buf);

	if (dump_timeout)
		dump_wdk_bind_info();

#ifdef CONFIG_LOCAL_WDT
	printk_deferred("[wdk] cpu:%d, kick local wdt,RT[%lld]\n",
			cpu, sched_clock());
	/* kick local wdt */
	mpcore_wdt_restart(WD_TYPE_NORMAL);
#endif
}

static int kwdt_thread(void *arg)
{
	struct sched_param param = {.sched_priority = 99 };
	int cpu = 0;
	int local_bit = 0, loc_need_config = 0, loc_timeout = 0;
	unsigned long curInterval = 0;
	struct wd_api *loc_wk_wdt = NULL;
	char msg_buf[WK_MAX_MSG_SIZE];

	sched_setscheduler(current, SCHED_FIFO, &param);
	set_current_state(TASK_INTERRUPTIBLE);

	for (;;) {

		if (kthread_should_stop()) {
			pr_info("[wdk] kthread_should_stop do !!\n");
			break;
		}

		msg_buf[0] = '\0';

		spin_lock(&lock);
		loc_wk_wdt = g_wd_api;
		loc_need_config = g_need_config;
		loc_timeout = g_timeout;
		spin_unlock(&lock);

		/*
		 * pr_debug("[wdk] loc_wk_wdt(%x),loc_wk_wdt->ready(%d)\n",
		 * loc_wk_wdt ,loc_wk_wdt->ready);
		 */
		curInterval = g_kinterval*1000*1000;
		if (loc_wk_wdt && loc_wk_wdt->ready && g_enable) {
			if (loc_need_config) {
				/* daul  mode */
				loc_wk_wdt->wd_config(WDT_DUAL_MODE,
					loc_timeout);
				spin_lock(&lock);
				g_need_config = 0;
				spin_unlock(&lock);
			}
			/*
			 * pr_debug("[wdk]  cpu-task=%d, current_pid=%d\n",
			 * wk_tsk[cpu]->pid,  current->pid);
			 */

			spin_lock(&lock);

			/* smp_processor_id does not
			 * allowed preemptible context
			 */
			cpu = smp_processor_id();

			/* to avoid wk_tsk[cpu] had not created out */
			if (wk_tsk[cpu] != 0) {

				/* only process kicking info
				 * if thread-x is on cpu-x
				 */
				if (wk_tsk[cpu]->pid == current->pid) {
#ifdef KWDT_KICK_TIME_ALIGN
					if (kick_bit == 0) {
						g_nxtKickTime =
							ktime_to_us(ktime_get())
							+ g_kinterval*1000*1000;
						curInterval =
							g_kinterval*1000*1000;
					} else {
						curInterval =	g_nxtKickTime
						- ktime_to_us(ktime_get());
					}
					/* to avoid interval too long */
					if (curInterval > g_kinterval*1000*1000)
						curInterval =
							g_kinterval*1000*1000;
#endif
					kwdt_process_kick(local_bit, cpu,
						curInterval, msg_buf);
				} else
					spin_unlock(&lock);
			} else
				spin_unlock(&lock);
		} else if (g_enable == 0) {
			pr_debug("[wdk] stop to kick\n");
		} else {
			pr_info("[wdk] no wdt driver is hooked\n");
			WARN_ON(1);
		}

		/* to avoid wk_tsk[cpu] had not created out */
		if (wk_tsk[cpu] != 0) {
			if (wk_tsk[cpu]->pid == current->pid) {
#if (DEBUG_WDK == 1)
				msleep_interruptible(debug_sleep * 1000);
				pr_debug("[wdk] wdk woke up %d\n",
					debug_sleep);
#endif
				/* limit the rtc time update frequency */
				spin_lock(&lock);
				msg_buf[0] = '\0';
				if (time_after(jiffies, rtc_update)) {
					rtc_update = jiffies + (1 * HZ);
					kwdt_print_utc(msg_buf,
						WK_MAX_MSG_SIZE);
				}
				spin_unlock(&lock);

				/*
				 * do not print message with spinlock held to
				 * avoid bulk of delayed printk happens here
				 */
				if (msg_buf[0] != '\0')
					pr_info("%s", msg_buf);
			}
		}

#ifdef KWDT_KICK_TIME_ALIGN
		usleep_range(curInterval, curInterval + SOFT_KICK_RANGE);
#else
		usleep_range(g_kinterval*1000*1000,
			g_kinterval*1000*1000 + SOFT_KICK_RANGE);
#endif

#ifdef CONFIG_MTK_AEE_POWERKEY_HANG_DETECT
		if ((cpu == 0) && (wk_tsk[cpu]->pid == current->pid)) {
			/* only effect at cpu0 */
			if (aee_kernel_wdt_kick_api(g_kinterval) ==
				WDT_PWK_HANG_FORCE_HWT) {
				printk_deferred("power key trigger HWT\n");
				/* Try to force to HWT */
				cpus_kick_bit = 0xFFFF;
			}
		}
#endif
	}
	pr_debug("[wdk] wdk thread stop, cpu:%d, pid:%d\n", cpu, current->pid);
	return 0;
}

static int start_kicker(void)
{

	int i;

	for (i = 0; i < CPU_NR; i++) {
		wk_tsk[i] = kthread_create(kwdt_thread,
			(void *)(unsigned long)i, "wdtk-%d", i);
		if (IS_ERR(wk_tsk[i])) {
			int ret = PTR_ERR(wk_tsk[i]);

			wk_tsk[i] = NULL;
			pr_info("[wdk]kthread_create failed, wdtk-%d\n", i);
			return ret;
		}
		/* wk_cpu_update_bit_flag(i,1); */
		wk_start_kick_cpu(i);
	}
	g_kicker_init = 1;
	pr_info("[wdk] WDT start kicker done CPU_NR=%d\n", CPU_NR);
	return 0;
}

unsigned int get_check_bit(void)
{
	return wk_check_kick_bit();
}

unsigned int get_kick_bit(void)
{
	return kick_bit;
}


/******************************************************************************
 * SYSFS support
 *****************************************************************************/
#ifdef __ENABLE_WDT_SYSFS__
/*---------------------------------------------------------------------------*/
/*define sysfs entry for configuring debug level and sysrq*/
const struct sysfs_ops mtk_rgu_sysfs_ops = {
	.show = mtk_rgu_attr_show,
	.store = mtk_rgu_attr_store,
};

/*---------------------------------------------------------------------------*/
struct mtk_rgu_sys_entry {
	struct attribute attr;
	 ssize_t (*show)(struct kobject *kobj, char *page);
	 ssize_t (*store)(struct kobject *kobj, const char *page, size_t size);
};
/*---------------------------------------------------------------------------*/
static struct mtk_rgu_sys_entry pause_wdt_entry = {
	{.name = "pause", .mode = 0644},
	mtk_rgu_pause_wdt_show,
	mtk_rgu_pause_wdt_store,
};

/*---------------------------------------------------------------------------*/
struct attribute *mtk_rgu_attributes[] = {
	&pause_wdt_entry.attr,
	NULL,
};

/*---------------------------------------------------------------------------*/
struct kobj_type mtk_rgu_ktype = {
	.sysfs_ops = &mtk_rgu_sysfs_ops,
	.default_attrs = mtk_rgu_attributes,
};

/*---------------------------------------------------------------------------*/
static struct mtk_rgu_sysobj {
	struct kobject kobj;
} rgu_sysobj;
/*---------------------------------------------------------------------------*/
int mtk_rgu_sysfs(void)
{
	struct mtk_rgu_sysobj *obj = &rgu_sysobj;

	memset(&obj->kobj, 0x00, sizeof(obj->kobj));

	obj->kobj.parent = kernel_kobj;
	if (kobject_init_and_add(&obj->kobj, &mtk_rgu_ktype, NULL, "mtk_rgu")) {
		kobject_put(&obj->kobj);
		return -ENOMEM;
	}
	kobject_uevent(&obj->kobj, KOBJ_ADD);

	return 0;
}

/*---------------------------------------------------------------------------*/
ssize_t mtk_rgu_attr_show(struct kobject *kobj,
	struct attribute *attr, char *buffer)
{
	struct mtk_rgu_sys_entry *entry = NULL;

	entry = container_of(attr, struct mtk_rgu_sys_entry, attr);

	return entry->show(kobj, buffer);
}

/*---------------------------------------------------------------------------*/
ssize_t mtk_rgu_attr_store(struct kobject *kobj,
	struct attribute *attr, const char *buffer,
			   size_t size)
{
	struct mtk_rgu_sys_entry *entry = NULL;

	entry = container_of(attr, struct mtk_rgu_sys_entry, attr);

	return entry->store(kobj, buffer, size);
}

/*---------------------------------------------------------------------------*/
ssize_t mtk_rgu_pause_wdt_show(struct kobject *kobj, char *buffer)
{
	int remain = PAGE_SIZE;
	int len = 0;
	char *ptr = buffer;

	ptr += len;
	remain -= len;

	return (PAGE_SIZE - remain);
}

/*---------------------------------------------------------------------------*/
ssize_t mtk_rgu_pause_wdt_store(struct kobject *kobj,
	const char *buffer, size_t size)
{
	char pause_wdt;
	int pause_wdt_b;
	int res = sscanf(buffer, "%c", &pause_wdt);

	pause_wdt_b = pause_wdt;

	if (res != 1) {
		pr_info("%s: expect 1 numbers\n", __func__);
	} else {
		/* For real case, pause wdt if get value is not zero.
		 * Suspend and resume may enable wdt again
		 */
		if (pause_wdt_b)
			mtk_wdt_enable(WK_WDT_DIS);
	}
	return size;
}

/*---------------------------------------------------------------------------*/
#endif /*__ENABLE_WDT_SYSFS__*/
/*---------------------------------------------------------------------------*/

static int wk_cpu_callback_online(unsigned int cpu)
{
	wk_cpu_update_bit_flag(cpu, 1);

	mtk_wdt_restart(WD_TYPE_NORMAL);

#ifdef CONFIG_LOCAL_WDT
	pr_debug("[wdk]cpu %d plug on kick local wdt\n", cpu);
	/* kick local wdt */
	mpcore_wdt_restart(WD_TYPE_NORMAL);
#endif
	/*
	 * Bind WDK thread to this CPU.
	 * NOTE: Thread binding must be executed after CPU is ready
	 * (online).
	 */
	if (g_kicker_init == 1)
		kicker_cpu_bind(cpu);
	else
		pr_info("kicker was not bound to CPU%d\n", cpu);

	mtk_wdt_cpu_callback(wk_tsk[cpu], cpu, g_kicker_init);

	return 0;
}

static int wk_cpu_callback_offline(unsigned int cpu)
{
#ifdef CONFIG_LOCAL_WDT
	pr_debug("[wdk]cpu %d plug off kick local wdt\n", cpu);
	/* kick local wdt */
	/* mpcore_wdt_restart(WD_TYPE_NORMAL); */
	/* disable local watchdog */
	mpcore_wk_wdt_stop();
#endif
	wk_cpu_update_bit_flag(cpu, 0);
	/* pr_info("[wdk]cpu %d plug off, kick wdt\n", hotcpu); */

	mtk_wdt_restart(WD_TYPE_NORMAL);/* for KICK external wdt */

	mtk_wdt_cpu_callback(wk_tsk[cpu], cpu, g_kicker_init);

	return 0;
}

static void wdk_work_callback(struct work_struct *work)
{
	int res = 0;
	int i = 0;
	/* init api */
	wd_api_init();
	/*  */
	res = get_wd_api(&g_wd_api);
	if (res)
		pr_info("get public api error in wd common driver %d", res);

#ifdef __ENABLE_WDT_SYSFS__
	mtk_rgu_sysfs();
#endif

	cpu_hotplug_disable();

	wk_proc_init();

	res = cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN,
		"watchdog:wdkctrl:online", wk_cpu_callback_online, NULL);
	if (res < 0)
		pr_info("[wdk]setup CPUHP_AP_ONLINE_DYN fail %d\n", res);

	res = cpuhp_setup_state_nocalls(CPUHP_BP_PREPARE_DYN,
		"watchdog:wdkctrl:offline", NULL, wk_cpu_callback_offline);
	if (res < 0)
		pr_info("[wdk]setup CPUHP_BP_PREPARE_DYN fail %d\n", res);

	for (i = 0; i < CPU_NR; i++) {
		if (cpu_online(i)) {
			wk_cpu_update_bit_flag(i, 1);
			pr_debug("[wdk]init cpu online %d\n", i);
		} else {
			wk_cpu_update_bit_flag(i, 0);
			pr_debug("[wdk]init cpu offline %d\n", i);
		}
	}
	mtk_wdt_restart(WD_TYPE_NORMAL);	/* for KICK external wdt */

#ifdef __ENABLE_WDT_AT_INIT__
	start_kicker_thread_with_default_setting();
#endif
	cpu_hotplug_enable();

	pr_info("[wdk]init_wk done late_initcall cpus_kick_bit=0x%x -----\n",
		cpus_kick_bit);

}

static int wdt_pm_notify(struct notifier_block *notify_block,
			unsigned long mode, void *unused)
{
	switch (mode) {
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
	case PM_RESTORE_PREPARE:
		lastsuspend_t = sched_clock();
		break;

	case PM_POST_SUSPEND:
	case PM_POST_HIBERNATION:
	case PM_POST_RESTORE:
		lastresume_t = sched_clock();
		break;
	}

	return 0;
}

static int __init init_wk(void)
{
	int res = 0;

	wdk_workqueue = create_singlethread_workqueue("mt-wdk");
	INIT_WORK(&wdk_work, wdk_work_callback);

	res = queue_work(wdk_workqueue, &wdk_work);

	if (!res)
		pr_info("[wdk]wdk_work start return:%d!\n", res);

	wdt_pm_nb.notifier_call = wdt_pm_notify;
	register_pm_notifier(&wdt_pm_nb);

	return 0;
}

static void __exit exit_wk(void)
{
	unregister_pm_notifier(&wdt_pm_nb);
	wk_proc_exit();
	kthread_stop((struct task_struct *)wk_tsk);
}

static int __init init_wk_check_bit(void)
{
	int i = 0;

	pr_debug("[wdk]arch init check_bit=0x%x+++++\n", cpus_kick_bit);
	for (i = 0; i < CPU_NR; i++)
		wk_cpu_update_bit_flag(i, 1);

	pr_debug("[wdk]arch init check_bit=0x%x-----\n", cpus_kick_bit);
	return 0;
}

late_initcall(init_wk);
arch_initcall(init_wk_check_bit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mediatek inc.");
MODULE_DESCRIPTION("The watchdog kicker");
