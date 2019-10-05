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

#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/kallsyms.h>
#include <linux/utsname.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/uaccess.h>
#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "internal.h"
#include "mtk_sched_mon.h"

#define BOOT_STR_SIZE 256
#define BUF_COUNT 12
#define LOGS_PER_BUF 80

struct log_t {
	/* task cmdline for first 16 bytes
	 * and boot event for the rest
	 */
	char *comm_event;
	pid_t pid;
	u64 timestamp;
};

static struct log_t *bootprof[BUF_COUNT];
static unsigned long log_count;
static DEFINE_MUTEX(bootprof_lock);
static bool enabled;
static int bootprof_lk_t, bootprof_pl_t, bootprof_logo_t;
static u64 timestamp_on, timestamp_off;
bool boot_finish;

module_param_named(pl_t, bootprof_pl_t, int, 0644);
module_param_named(lk_t, bootprof_lk_t, int, 0644);
module_param_named(logo_t, bootprof_logo_t, int, 0644);

#define MSG_SIZE 128

#ifdef CONFIG_BOOTPROF_THRESHOLD_MS
#define BOOTPROF_THRESHOLD (CONFIG_BOOTPROF_THRESHOLD_MS*1000000)
#else
#define BOOTPROF_THRESHOLD 15000000
#endif

void log_boot(char *str)
{
	unsigned long long ts;
	struct log_t *p = NULL;
	size_t n;

	if (!str) {
		pr_info("[BOOTPROF] Null buffer. Skip log.\n");
		return;
	}

	if (!enabled)
		return;
	n = strlen(str) + 1;

	ts = sched_clock();
	pr_info("BOOTPROF:%10lld.%06ld:%s\n", msec_high(ts), msec_low(ts), str);

	mutex_lock(&bootprof_lock);
	if (log_count >= (LOGS_PER_BUF * BUF_COUNT)) {
		pr_info("[BOOTPROF] not enuough bootprof buffer\n");
		goto out;
	} else if (log_count && !(log_count % LOGS_PER_BUF)) {
		bootprof[log_count / LOGS_PER_BUF] =
			kcalloc(LOGS_PER_BUF, sizeof(struct log_t),
				GFP_ATOMIC | __GFP_NORETRY | __GFP_NOWARN);
	}
	if (!bootprof[log_count / LOGS_PER_BUF]) {
		pr_info("no memory for bootprof\n");
		goto out;
	}
	p = &bootprof[log_count / LOGS_PER_BUF][log_count % LOGS_PER_BUF];

	p->timestamp = ts;
	p->pid = current->pid;
	n += TASK_COMM_LEN;
	p->comm_event = kzalloc(n, GFP_ATOMIC | __GFP_NORETRY |
			  __GFP_NOWARN);
	if (!p->comm_event) {
		enabled = false;
		goto out;
	}

	memcpy(p->comm_event, current->comm, TASK_COMM_LEN);
	memcpy(p->comm_event + TASK_COMM_LEN, str, n - TASK_COMM_LEN);
	log_count++;
out:
	mutex_unlock(&bootprof_lock);
}

void bootprof_initcall(initcall_t fn, unsigned long long ts)
{
	/* log more than threshold initcalls */
	unsigned long msec_rem;
	char msgbuf[MSG_SIZE];

	if (ts > BOOTPROF_THRESHOLD) {
		msec_rem = do_div(ts, NSEC_PER_MSEC);
		snprintf(msgbuf, sizeof(msgbuf), "initcall: %ps %5llu.%06lums",
			 fn, ts, msec_rem);
		log_boot(msgbuf);
	}
}

void bootprof_probe(unsigned long long ts, struct device *dev,
			   struct device_driver *drv, unsigned long probe)
{
	/* log more than threshold probes*/
	unsigned long msec_rem;
	char msgbuf[MSG_SIZE];
	int pos;

	if (ts <= BOOTPROF_THRESHOLD)
		return;
	msec_rem = do_div(ts, NSEC_PER_MSEC);

	pos = snprintf(msgbuf, sizeof(msgbuf), "probe: probe=%ps",
					(void *)probe);
	if (drv)
		pos += snprintf(msgbuf + pos, sizeof(msgbuf) - pos,
				" drv=%s(%ps)", drv->name ? drv->name : "",
				(void *)drv);

	if (dev && dev->init_name)
		pos += snprintf(msgbuf + pos, sizeof(msgbuf) - pos,
				" dev=%s(%ps)", dev->init_name, (void *)dev);

	pos += snprintf(msgbuf + pos, sizeof(msgbuf) - pos,
				" %5llu.%06lums", ts, msec_rem);
	log_boot(msgbuf);
}

void bootprof_pdev_register(unsigned long long ts, struct platform_device *pdev)
{
	/* log more than threshold register*/
	unsigned long msec_rem;
	char msgbuf[MSG_SIZE];

	if (ts <= BOOTPROF_THRESHOLD || !pdev)
		return;
	msec_rem = do_div(ts, NSEC_PER_MSEC);
	snprintf(msgbuf, sizeof(msgbuf), "probe: pdev=%s(%ps) %5llu.%06lums",
		 pdev->name, (void *)pdev, ts, msec_rem);
	log_boot(msgbuf);
}

static void bootup_finish(void)
{
	initcall_debug = 0;
#ifdef CONFIG_MTK_PRINTK_UART_CONSOLE
	mt_disable_uart();
#endif
#ifdef CONFIG_MTK_SCHED_MON_DEFAULT_ENABLE
	mt_sched_monitor_switch(1);
#endif
}

static void mt_bootprof_switch(int on)
{
	mutex_lock(&bootprof_lock);
	if (enabled ^ on) {
		unsigned long long ts = sched_clock();

		pr_info("BOOTPROF:%10lld.%06ld: %s\n",
		       msec_high(ts), msec_low(ts), on ? "ON" : "OFF");

		if (on) {
			enabled = 1;
			timestamp_on = ts;
		} else {
			/* boot up complete */
			enabled = 0;
			timestamp_off = ts;
			boot_finish = true;
			bootup_finish();
		}
	}
	mutex_unlock(&bootprof_lock);
}

static ssize_t
mt_bootprof_write(struct file *filp, const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[BOOT_STR_SIZE];
	size_t copy_size = cnt;

	if (cnt >= sizeof(buf))
		copy_size = BOOT_STR_SIZE - 1;

	if (copy_from_user(&buf, ubuf, copy_size))
		return -EFAULT;

	if (cnt == 1 && buf[0] == '1') {
		mt_bootprof_switch(1);
		return 1;
	} else if (cnt == 1 && buf[0] == '0') {
		mt_bootprof_switch(0);
		return 1;
	}

	buf[copy_size] = 0;
	log_boot(buf);

	return cnt;

}

static int mt_bootprof_show(struct seq_file *m, void *v)
{
	int i;
	struct log_t *p;

	if (m == NULL) {
		pr_info("seq_file is Null.\n");
		return 0;
	}

	seq_puts(m, "----------------------------------------\n");
	seq_printf(m, "%d	    BOOT PROF (unit:msec)\n", enabled);
	seq_puts(m, "----------------------------------------\n");

	if (bootprof_pl_t > 0 && bootprof_lk_t > 0) {
		seq_printf(m, "%10d        : %s\n", bootprof_pl_t, "preloader");
		if (bootprof_logo_t > 0) {
			seq_printf(m, "%10d        : %s (%s: %d)\n",
			bootprof_lk_t, "lk", "Start->Show logo",
			bootprof_logo_t);
		} else {
			seq_printf(m, "%10d        : %s\n",
			bootprof_lk_t, "lk");
		}
		seq_puts(m, "----------------------------------------\n");
	}

	seq_printf(m, "%10lld.%06ld : ON (THR:%10lld ms)\n",
		   msec_high(timestamp_on), msec_low(timestamp_on),
		   msec_high(BOOTPROF_THRESHOLD));

	for (i = 0; i < log_count; i++) {
		p = &bootprof[i / LOGS_PER_BUF][i % LOGS_PER_BUF];
		if (!p->comm_event)
			continue;

		seq_printf(m, "%10llu.%06lu :%5d-%-16s: %s\n",
			   msec_high(p->timestamp),
			   msec_low(p->timestamp),
			   p->pid, p->comm_event,
			   p->comm_event + TASK_COMM_LEN);
	}

	seq_printf(m, "%10lld.%06ld : OFF\n",
		   msec_high(timestamp_off), msec_low(timestamp_off));
	seq_puts(m, "----------------------------------------\n");
	return 0;
}

/*** Seq operation of mtprof ****/
static int mt_bootprof_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_bootprof_show, inode->i_private);
}

static const struct file_operations mt_bootprof_fops = {
	.open = mt_bootprof_open,
	.write = mt_bootprof_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init init_boot_prof(void)
{
	struct proc_dir_entry *pe;

	pe = proc_create("bootprof", 0664, NULL, &mt_bootprof_fops);
	if (!pe)
		return -ENOMEM;
	return 0;
}

static int __init init_bootprof_buf(void)
{
	memset(bootprof, 0, sizeof(struct log_t *) * BUF_COUNT);
	bootprof[0] = kcalloc(LOGS_PER_BUF, sizeof(struct log_t),
			      GFP_ATOMIC | __GFP_NORETRY | __GFP_NOWARN);
	if (!bootprof[0])
		goto fail;
	mt_bootprof_switch(1);
fail:
	return 0;
}

early_initcall(init_bootprof_buf);
device_initcall(init_boot_prof);
