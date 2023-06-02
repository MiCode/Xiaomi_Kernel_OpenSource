// SPDX-License-Identifier: GPL-2.0
/*
 * Qualcomm Peripheral Image Loader for Q6V5
 *
 * Copyright (C) 2016-2018 Linaro Ltd.
 * Copyright (C) 2014 Sony Mobile Communications AB
 * Copyright (c) 2012-2013, 2020-2021, The Linux Foundation. All rights reserved.
 */
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/soc/qcom/smem.h>
#include <linux/soc/qcom/smem_state.h>
#include <linux/remoteproc.h>
#include <linux/delay.h>
#include "qcom_common.h"
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include "qcom_q6v5.h"
#include <trace/events/rproc_qcom.h>

#define Q6V5_PANIC_DELAY_MS	200
#define MAX_SSR_REASON_LEN	256U
#define MAX_CRASH_REASON    256

static int crash_num = 0;
static bool ramdump_state = false;
static char last_modem_sfr_reason[MAX_SSR_REASON_LEN] = "none";
static struct proc_dir_entry *last_modem_sfr_entry = NULL;
static struct proc_dir_entry *ramdump_state_sfr_entry = NULL;
static char modem_crash_reason[MAX_CRASH_REASON][MAX_SSR_REASON_LEN]={"0"};

/* modem crash history entry */
static int last_modem_sfr_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", last_modem_sfr_reason);
	return 0;
}

static int last_modem_sfr_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, last_modem_sfr_proc_show, NULL);
}

static const struct proc_ops last_modem_sfr_file_ops = {
	//.owner   = THIS_MODULE,
	.proc_open    = last_modem_sfr_proc_open,
	.proc_read    = seq_read,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};

/* ramdump state entry */
static int ramdump_state_proc_clear(struct seq_file *m, void *v)
{
	ramdump_state = true;
	pr_err("ramdump state change\n");
	seq_printf(m, "%s\n", "null");
	return 0;
}

static int ramdump_state_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ramdump_state_proc_clear, NULL);
}

static const struct proc_ops ramdump_state_file_ops = {
	//.owner   = THIS_MODULE,
	.proc_open    = ramdump_state_proc_open,
	.proc_read    = seq_read,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};
/**
 * qcom_q6v5_prepare() - reinitialize the qcom_q6v5 context before start
 * @q6v5:	reference to qcom_q6v5 context to be reinitialized
 *
 * Return: 0 on success, negative errno on failure
 */
int qcom_q6v5_prepare(struct qcom_q6v5 *q6v5)
{
	if (last_modem_sfr_entry == NULL) {
		last_modem_sfr_entry = proc_create("last_mcrash", S_IFREG | S_IRUGO, NULL, &last_modem_sfr_file_ops);
	}
	if (!last_modem_sfr_entry) {
		printk(KERN_ERR "pil: cannot create proc entry last_mcrash\n");
	}
	if (ramdump_state_sfr_entry == NULL) {
		ramdump_state_sfr_entry = proc_create("ramdump_state", S_IFREG | S_IRUGO | S_IWUGO, NULL, &ramdump_state_file_ops);
	}
	if (!ramdump_state_sfr_entry) {
		printk(KERN_ERR "pil: cannot create proc entry ramdump_state\n");
	}
	reinit_completion(&q6v5->start_done);
	reinit_completion(&q6v5->stop_done);

	q6v5->running = true;
	q6v5->handover_issued = false;

	enable_irq(q6v5->handover_irq);

	return 0;
}
EXPORT_SYMBOL_GPL(qcom_q6v5_prepare);

/**
 * qcom_q6v5_unprepare() - unprepare the qcom_q6v5 context after stop
 * @q6v5:	reference to qcom_q6v5 context to be unprepared
 *
 * Return: 0 on success, 1 if handover hasn't yet been called
 */
int qcom_q6v5_unprepare(struct qcom_q6v5 *q6v5)
{
	disable_irq(q6v5->handover_irq);

	return !q6v5->handover_issued;
}
EXPORT_SYMBOL_GPL(qcom_q6v5_unprepare);

void qcom_q6v5_register_ssr_subdev(struct qcom_q6v5 *q6v5, struct rproc_subdev *ssr_subdev)
{
	q6v5->ssr_subdev = ssr_subdev;
}
EXPORT_SYMBOL(qcom_q6v5_register_ssr_subdev);

static void qcom_q6v5_crash_handler_work(struct work_struct *work)
{
	struct qcom_q6v5 *q6v5 = container_of(work, struct qcom_q6v5, crash_handler);
	struct rproc *rproc = q6v5->rproc;
	struct rproc_subdev *subdev;
	int votes;

	mutex_lock(&rproc->lock);

	rproc->state = RPROC_CRASHED;

	votes = atomic_xchg(&rproc->power, 0);
	/* if votes are zero, rproc has already been shutdown */
	if (votes == 0) {
		mutex_unlock(&rproc->lock);
		return;
	}

	list_for_each_entry_reverse(subdev, &rproc->subdevs, node) {
		if (subdev->stop)
			subdev->stop(subdev, true);
	}

	mutex_unlock(&rproc->lock);

	/*
	 * Temporary workaround until ramdump userspace application calls
	 * sync() and fclose() on attempting the dump.
	 */
	msleep(100);
	panic("Panicking, remoteproc %s crashed\n", q6v5->rproc->name);
}

static irqreturn_t q6v5_wdog_interrupt(int irq, void *data)
{
	struct qcom_q6v5 *q6v5 = data;
	struct qcom_rproc_ssr *ssr;
	size_t len;
	char *msg;
	int temp_num;

	/* Sometimes the stop triggers a watchdog rather than a stop-ack */
	if (!q6v5->running) {
		dev_info(q6v5->dev, "received wdog irq while q6 is offline\n");
		complete(&q6v5->stop_done);
		return IRQ_HANDLED;
	}

	msg = qcom_smem_get(QCOM_SMEM_HOST_ANY, q6v5->crash_reason, &len);
	if (!IS_ERR(msg) && len > 0 && msg[0]){
		dev_err(q6v5->dev, "watchdog received: %s\n", msg);
		dev_err(q6v5->dev, "subsystem failure reason: %s. \n", msg);
        strlcpy(last_modem_sfr_reason, msg, MAX_SSR_REASON_LEN);
		if (ramdump_state)
			strlcpy(modem_crash_reason[crash_num++], msg, MAX_SSR_REASON_LEN);
	}else {
		dev_err(q6v5->dev, "watchdog without message\n");
		dev_err(q6v5->dev, "subsystem failure reason: watchdog without message. \n");
        //strlcpy(last_modem_sfr_reason, msg, MAX_SSR_REASON_LEN);
	}

	q6v5->running = false;
	temp_num = crash_num-1;
	while((temp_num--) && crash_num && (crash_num >= 10))
	{
		if(!strcmp(modem_crash_reason[crash_num-1],modem_crash_reason[temp_num]))
		{
			crash_num = crash_num-1;
			q6v5->rproc->dump_conf = RPROC_COREDUMP_DISABLED;
			pr_err("qcom_q6v5.c :in compare, same crash reason to skip dump");
			break;
		}else if (!temp_num){
			q6v5->rproc->dump_conf = RPROC_COREDUMP_ENABLED;
			break;
		}
	}

	if(!IS_ERR(msg) && len > 0 && msg[0])
		trace_rproc_qcom_event(dev_name(q6v5->dev), "q6v5_wdog", msg);
	else
		trace_rproc_qcom_event(dev_name(q6v5->dev), "q6v5_wdog", "");
	if (q6v5->rproc->recovery_disabled) {
		schedule_work(&q6v5->crash_handler);
	} else {
		if (q6v5->ssr_subdev) {
			qcom_notify_early_ssr_clients(q6v5->ssr_subdev);
			ssr = container_of(q6v5->ssr_subdev, struct qcom_rproc_ssr, subdev);
			ssr->is_notified = true;
		}

		rproc_report_crash(q6v5->rproc, RPROC_WATCHDOG);
	}

	return IRQ_HANDLED;
}

static irqreturn_t q6v5_fatal_interrupt(int irq, void *data)
{
	struct qcom_q6v5 *q6v5 = data;
	struct qcom_rproc_ssr *ssr;
	size_t len;
	char *msg;
	int temp_num;

	if (!q6v5->running) {
		dev_info(q6v5->dev, "received fatal irq while q6 is offline\n");
		return IRQ_HANDLED;
	}

	msg = qcom_smem_get(QCOM_SMEM_HOST_ANY, q6v5->crash_reason, &len);
	if (!IS_ERR(msg) && len > 0 && msg[0]){
		dev_err(q6v5->dev, "fatal error received: %s\n", msg);
		dev_err(q6v5->dev, "subsystem failure reason: %s. \n", msg);
        strlcpy(last_modem_sfr_reason, msg, MAX_SSR_REASON_LEN);
		if (ramdump_state)
			strlcpy(modem_crash_reason[crash_num++], msg, MAX_SSR_REASON_LEN);
	}else {
		dev_err(q6v5->dev, "fatal error without message\n");
		dev_err(q6v5->dev, "subsystem failure reason: fatal error without message. \n");
        //strlcpy(last_modem_sfr_reason, msg, MAX_SSR_REASON_LEN);
	}
	
	q6v5->running = false;
	temp_num = crash_num-1;
	while((temp_num--) && crash_num && (crash_num >= 10))
	{
		if(!strcmp(modem_crash_reason[crash_num-1],modem_crash_reason[temp_num]))
		{
			crash_num = crash_num-1;
			q6v5->rproc->dump_conf = RPROC_COREDUMP_DISABLED;
			pr_err("qcom_q6v5.c :in compare, same crash reason to skip dump");
			break;
		}else if (!temp_num){
			q6v5->rproc->dump_conf = RPROC_COREDUMP_ENABLED;
			break;
		}
	}
	if(!IS_ERR(msg) && len > 0 && msg[0])
		trace_rproc_qcom_event(dev_name(q6v5->dev), "q6v5_fatal", msg);
	else
		trace_rproc_qcom_event(dev_name(q6v5->dev), "q6v5_fatal", "");
	if (q6v5->rproc->recovery_disabled) {
		schedule_work(&q6v5->crash_handler);
	} else {
		if (q6v5->ssr_subdev) {
			qcom_notify_early_ssr_clients(q6v5->ssr_subdev);
			ssr = container_of(q6v5->ssr_subdev, struct qcom_rproc_ssr, subdev);
			ssr->is_notified = true;
		}

		rproc_report_crash(q6v5->rproc, RPROC_FATAL_ERROR);
	}

	return IRQ_HANDLED;
}

static irqreturn_t q6v5_ready_interrupt(int irq, void *data)
{
	struct qcom_q6v5 *q6v5 = data;

	complete(&q6v5->start_done);

	return IRQ_HANDLED;
}

/**
 * qcom_q6v5_wait_for_start() - wait for remote processor start signal
 * @q6v5:	reference to qcom_q6v5 context
 * @timeout:	timeout to wait for the event, in jiffies
 *
 * qcom_q6v5_unprepare() should not be called when this function fails.
 *
 * Return: 0 on success, -ETIMEDOUT on timeout
 */
int qcom_q6v5_wait_for_start(struct qcom_q6v5 *q6v5, int timeout)
{
	int ret;

	ret = wait_for_completion_timeout(&q6v5->start_done, timeout);
	if (!ret)
		disable_irq(q6v5->handover_irq);

	return !ret ? -ETIMEDOUT : 0;
}
EXPORT_SYMBOL_GPL(qcom_q6v5_wait_for_start);

static irqreturn_t q6v5_handover_interrupt(int irq, void *data)
{
	struct qcom_q6v5 *q6v5 = data;

	if (q6v5->handover)
		q6v5->handover(q6v5);

	q6v5->handover_issued = true;

	return IRQ_HANDLED;
}

static irqreturn_t q6v5_stop_interrupt(int irq, void *data)
{
	struct qcom_q6v5 *q6v5 = data;

	complete(&q6v5->stop_done);

	return IRQ_HANDLED;
}

/**
 * qcom_q6v5_request_stop() - request the remote processor to stop
 * @q6v5:	reference to qcom_q6v5 context
 * @sysmon:	reference to the remote's sysmon instance, or NULL
 *
 * Return: 0 on success, negative errno on failure
 */
int qcom_q6v5_request_stop(struct qcom_q6v5 *q6v5, struct qcom_sysmon *sysmon)
{
	int ret;
    
	if (last_modem_sfr_entry) {
        remove_proc_entry("last_mcrash", NULL);
        last_modem_sfr_entry = NULL;
	}
	if (ramdump_state_sfr_entry) {
		remove_proc_entry("ramdump_state", NULL);
		ramdump_state_sfr_entry = NULL;
	}
	q6v5->running = false;

	/* Don't perform SMP2P dance if sysmon already shut
	 * down the remote or if it isn't running
	 */
	if (q6v5->rproc->state != RPROC_RUNNING || qcom_sysmon_shutdown_acked(sysmon))
		return 0;

	qcom_smem_state_update_bits(q6v5->state,
				    BIT(q6v5->stop_bit), BIT(q6v5->stop_bit));

	ret = wait_for_completion_timeout(&q6v5->stop_done, 5 * HZ);

	qcom_smem_state_update_bits(q6v5->state, BIT(q6v5->stop_bit), 0);

	return ret == 0 ? -ETIMEDOUT : 0;
}
EXPORT_SYMBOL_GPL(qcom_q6v5_request_stop);

/**
 * qcom_q6v5_panic() - panic handler to invoke a stop on the remote
 * @q6v5:	reference to qcom_q6v5 context
 *
 * Set the stop bit and sleep in order to allow the remote processor to flush
 * its caches etc for post mortem debugging.
 *
 * Return: 200ms
 */
unsigned long qcom_q6v5_panic(struct qcom_q6v5 *q6v5)
{
	qcom_smem_state_update_bits(q6v5->state,
				    BIT(q6v5->stop_bit), BIT(q6v5->stop_bit));

	return Q6V5_PANIC_DELAY_MS;
}
EXPORT_SYMBOL_GPL(qcom_q6v5_panic);

/**
 * qcom_q6v5_init() - initializer of the q6v5 common struct
 * @q6v5:	handle to be initialized
 * @pdev:	platform_device reference for acquiring resources
 * @rproc:	associated remoteproc instance
 * @crash_reason: SMEM id for crash reason string, or 0 if none
 * @handover:	function to be called when proxy resources should be released
 *
 * Return: 0 on success, negative errno on failure
 */
int qcom_q6v5_init(struct qcom_q6v5 *q6v5, struct platform_device *pdev,
		   struct rproc *rproc, int crash_reason,
		   void (*handover)(struct qcom_q6v5 *q6v5))
{
	int ret;
	if (last_modem_sfr_entry == NULL) {
		last_modem_sfr_entry = proc_create("last_mcrash", S_IFREG | S_IRUGO, NULL, &last_modem_sfr_file_ops);
	}
	if (!last_modem_sfr_entry) {
		printk(KERN_ERR "pil: cannot create proc entry last_mcrash\n");
	}

	if (ramdump_state_sfr_entry == NULL) {
		ramdump_state_sfr_entry = proc_create("ramdump_state", S_IFREG | S_IRUGO | S_IWUGO, NULL, &ramdump_state_file_ops);
	}
	if (!ramdump_state_sfr_entry) {
		printk(KERN_ERR "pil: cannot create proc entry ramdump_state\n");
	}
	q6v5->rproc = rproc;
	q6v5->dev = &pdev->dev;
	q6v5->crash_reason = crash_reason;
	q6v5->handover = handover;
	q6v5->ssr_subdev = NULL;

	init_completion(&q6v5->start_done);
	init_completion(&q6v5->stop_done);

	q6v5->wdog_irq = platform_get_irq_byname(pdev, "wdog");
	if (q6v5->wdog_irq < 0)
		return q6v5->wdog_irq;

	ret = devm_request_threaded_irq(&pdev->dev, q6v5->wdog_irq,
					NULL, q6v5_wdog_interrupt,
					IRQF_ONESHOT,
					"q6v5 wdog", q6v5);
	if (ret) {
		dev_err(&pdev->dev, "failed to acquire wdog IRQ\n");
		return ret;
	}

	q6v5->fatal_irq = platform_get_irq_byname(pdev, "fatal");
	if (q6v5->fatal_irq < 0)
		return q6v5->fatal_irq;

	ret = devm_request_threaded_irq(&pdev->dev, q6v5->fatal_irq,
					NULL, q6v5_fatal_interrupt,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"q6v5 fatal", q6v5);
	if (ret) {
		dev_err(&pdev->dev, "failed to acquire fatal IRQ\n");
		return ret;
	}

	q6v5->ready_irq = platform_get_irq_byname(pdev, "ready");
	if (q6v5->ready_irq < 0)
		return q6v5->ready_irq;

	ret = devm_request_threaded_irq(&pdev->dev, q6v5->ready_irq,
					NULL, q6v5_ready_interrupt,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"q6v5 ready", q6v5);
	if (ret) {
		dev_err(&pdev->dev, "failed to acquire ready IRQ\n");
		return ret;
	}

	q6v5->handover_irq = platform_get_irq_byname(pdev, "handover");
	if (q6v5->handover_irq < 0)
		return q6v5->handover_irq;

	ret = devm_request_threaded_irq(&pdev->dev, q6v5->handover_irq,
					NULL, q6v5_handover_interrupt,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"q6v5 handover", q6v5);
	if (ret) {
		dev_err(&pdev->dev, "failed to acquire handover IRQ\n");
		return ret;
	}
	disable_irq(q6v5->handover_irq);

	q6v5->stop_irq = platform_get_irq_byname(pdev, "stop-ack");
	if (q6v5->stop_irq < 0)
		return q6v5->stop_irq;

	ret = devm_request_threaded_irq(&pdev->dev, q6v5->stop_irq,
					NULL, q6v5_stop_interrupt,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"q6v5 stop", q6v5);
	if (ret) {
		dev_err(&pdev->dev, "failed to acquire stop-ack IRQ\n");
		return ret;
	}

	q6v5->state = qcom_smem_state_get(&pdev->dev, "stop", &q6v5->stop_bit);
	if (IS_ERR(q6v5->state)) {
		dev_err(&pdev->dev, "failed to acquire stop state\n");
		return PTR_ERR(q6v5->state);
	}

	INIT_WORK(&q6v5->crash_handler, qcom_q6v5_crash_handler_work);

	return 0;
}
EXPORT_SYMBOL_GPL(qcom_q6v5_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Peripheral Image Loader for Q6V5");
