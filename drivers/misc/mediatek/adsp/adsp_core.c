// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/device.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/arm-smccc.h>    /* for Kernel Native SMC API */
#include <linux/soc/mediatek/mtk_sip_svc.h> /* for SMC ID table */
#include "adsp_clk.h"
#include "adsp_timesync.h"
#include "adsp_semaphore.h"
#include "adsp_platform.h"
#include "adsp_platform_driver.h"
#include "adsp_excep.h"
#include "adsp_mbox.h"
#include "adsp_core.h"

#define adsp_smc_send(_opid, _val1, _val2)                   \
({                                                           \
	struct arm_smccc_res res;                            \
	arm_smccc_smc(MTK_SIP_KERNEL_ADSP_CONTROL,           \
		      _opid, _val1, _val2, 0, 0, 0, 0, &res);\
	res.a0;                                              \
})

static int (*ipi_queue_send_msg_handler)(
		uint32_t core_id, /* enum adsp_core_id */
		uint32_t ipi_id,  /* enum adsp_ipi_id */
		void *buf,
		uint32_t len,
		uint32_t wait_ms);

struct adsp_priv *adsp_cores[ADSP_CORE_TOTAL];
struct adspsys_priv *adspsys;

const struct attribute_group *adspsys_attr_groups[] = {
	&adsp_excep_attr_group,
	NULL,
};

/* protect access tcm if set reset flag */
static DEFINE_MUTEX(access_lock);

/* notifier */
static BLOCKING_NOTIFIER_HEAD(adsp_notifier_list);

/* ------------------------------------------------ */
struct adsp_priv *_get_adsp_core(void *ptr, int id)
{
	if (ptr)
		return container_of(ptr, struct adsp_priv, mdev);

	if (id < get_adsp_core_total() && id >= 0)
		return adsp_cores[id];

	return NULL;
}

void set_adsp_state(struct adsp_priv *pdata, int state)
{
	pdata->state = state;
}
EXPORT_SYMBOL(set_adsp_state);

int get_adsp_state(struct adsp_priv *pdata)
{
	return pdata->state;
}
EXPORT_SYMBOL(get_adsp_state);

int is_adsp_ready(u32 cid)
{
	if (unlikely(cid >= get_adsp_core_total()))
		return -EINVAL;

	if (unlikely(!adsp_cores[cid]))
		return -EINVAL;

	return (adsp_cores[cid]->state == ADSP_RUNNING);
}
EXPORT_SYMBOL(is_adsp_ready);

u32 get_adsp_core_total(void)
{
	return adspsys ? adspsys->num_cores : 0;
}
EXPORT_SYMBOL(get_adsp_core_total);

bool is_adsp_system_running(void)
{
	unsigned int id;

	for (id = 0; id < get_adsp_core_total(); id++) {
		if (is_adsp_ready(id) > 0)
			return true;
	}
	return false;
}

int adsp_copy_to_sharedmem(struct adsp_priv *pdata, int id, const void *src,
			   int count)
{
	void __iomem *dst = NULL;
	const struct sharedmem_info *item;

	if (unlikely(id >= ADSP_SHAREDMEM_NUM))
		return 0;

	item = pdata->mapping_table + id;
	if (item->offset)
		dst = pdata->dtcm + pdata->dtcm_size - item->offset;

	if (unlikely(!dst || !src))
		return 0;

	if (count > item->size)
		count = item->size;

	memcpy_toio(dst, src, count);

	return count;
}
EXPORT_SYMBOL(adsp_copy_to_sharedmem);

int adsp_copy_from_sharedmem(struct adsp_priv *pdata, int id, void *dst,
			     int count)
{
	void __iomem *src = NULL;
	const struct sharedmem_info *item;

	if (unlikely(id >= ADSP_SHAREDMEM_NUM))
		return 0;

	item = pdata->mapping_table + id;
	if (item->offset)
		src = pdata->dtcm + pdata->dtcm_size - item->offset;

	if (unlikely(!dst || !src))
		return 0;

	if (count > item->size)
		count = item->size;

	memcpy_fromio(dst, src, count);

	return count;
}
EXPORT_SYMBOL(adsp_copy_from_sharedmem);

void hook_ipi_queue_send_msg_handler(
	int (*send_msg_handler)(
		uint32_t core_id, /* enum adsp_core_id */
		uint32_t ipi_id,  /* enum adsp_ipi_id */
		void *buf,
		uint32_t len,
		uint32_t wait_ms))
{
	ipi_queue_send_msg_handler = send_msg_handler;
}
EXPORT_SYMBOL(hook_ipi_queue_send_msg_handler);

void unhook_ipi_queue_send_msg_handler(void)
{
	ipi_queue_send_msg_handler = NULL;
}
EXPORT_SYMBOL(unhook_ipi_queue_send_msg_handler);

enum adsp_ipi_status adsp_push_message(enum adsp_ipi_id id, void *buf,
			unsigned int len, unsigned int wait_ms,
			unsigned int core_id)
{
	int ret = -1;

	/* send msg to queue */
	if (ipi_queue_send_msg_handler)
		ret = ipi_queue_send_msg_handler(core_id, id, buf, len, wait_ms);
	else
		ret = adsp_send_message(id, buf, len, wait_ms, core_id);

	return (ret == 0) ? ADSP_IPI_DONE : ADSP_IPI_ERROR;
}
EXPORT_SYMBOL(adsp_push_message);

enum adsp_ipi_status adsp_send_message(enum adsp_ipi_id id, void *buf,
			unsigned int len, unsigned int wait,
			unsigned int core_id)
{
	struct adsp_priv *pdata = NULL;
	struct mtk_ipi_msg msg;

	if (core_id >= get_adsp_core_total() || !buf)
		return ADSP_IPI_ERROR;

	pdata = get_adsp_core_by_id(core_id);

	if (get_adsp_state(pdata) != ADSP_RUNNING) {
		pr_notice("%s, adsp not enabled, id=%d", __func__, id);
		return ADSP_IPI_ERROR;
	}

	/* system is going to suspend, reject the following msg */
	if (id == ADSP_IPI_DVFS_SUSPEND)
		set_adsp_state(pdata, ADSP_SUSPENDING);

	msg.ipihd.id = id;
	msg.ipihd.len = len;
	msg.ipihd.options = 0xffff0000;
	msg.ipihd.reserved = 0xdeadbeef;
	msg.data = buf;

	return adsp_mbox_send(pdata->send_mbox, &msg, wait);
}
EXPORT_SYMBOL(adsp_send_message);

static irqreturn_t adsp_irq_top_handler(int irq, void *data)
{
	struct irq_t *pdata = (struct irq_t *)data;

	adsp_mt_clr_spm(pdata->cid);
	if (!pdata->clear_irq)
		return IRQ_NONE;

	pdata->clear_irq(pdata->cid);

	if (pdata->irq_cb)
		pdata->irq_cb(irq, pdata->data, pdata->cid);

	/* wake up bottom half if necessary */
	return pdata->thread_fn ? IRQ_WAKE_THREAD : IRQ_HANDLED;
}

static irqreturn_t adsp_irq_bottom_thread(int irq, void *data)
{
	struct irq_t *pdata = (struct irq_t *)data;

	pdata->thread_fn(irq, pdata->data, pdata->cid);
	return IRQ_HANDLED;
}

int adsp_threaded_irq_registration(u32 core_id, u32 irq_id,
				   void *handler, void *thread_fn, void *data)
{
	int ret = 0;
	struct adsp_priv *pdata = get_adsp_core_by_id(core_id);

	if (unlikely(!pdata))
		return -EACCES;

	if (!handler && !thread_fn)
		return -EINVAL;

	pdata->irq[irq_id].cid = core_id;
	pdata->irq[irq_id].irq_cb = handler;
	pdata->irq[irq_id].thread_fn = thread_fn;
	pdata->irq[irq_id].data = data;

	ret = request_threaded_irq(pdata->irq[irq_id].seq,
				   adsp_irq_top_handler,
				   thread_fn ? adsp_irq_bottom_thread : NULL,
				   IRQF_TRIGGER_HIGH,
				   pdata->name, &pdata->irq[irq_id]);
	if (ret < 0) {
		pr_info("%s(), request_irq(%d) err:%d\n",
			__func__, pdata->irq[irq_id].seq, ret);
		goto EXIT;
	}

	ret = enable_irq_wake(pdata->irq[irq_id].seq);
	if (ret < 0) {
		pr_info("%s(), enable_irq_wake(%d) err:%d\n",
			__func__, pdata->irq[irq_id].seq, ret);
		goto EXIT;
	}
EXIT:
	return ret;
}
EXPORT_SYMBOL_GPL(adsp_threaded_irq_registration);

void adsp_register_notify(struct notifier_block *nb)
{
	blocking_notifier_chain_register(&adsp_notifier_list, nb);
}
EXPORT_SYMBOL(adsp_register_notify);

void adsp_unregister_notify(struct notifier_block *nb)
{
	blocking_notifier_chain_unregister(&adsp_notifier_list, nb);
}
EXPORT_SYMBOL(adsp_unregister_notify);

void adsp_extern_notify_chain(enum ADSP_NOTIFY_EVENT event)
{
#ifdef CFG_RECOVERY_SUPPORT
	blocking_notifier_call_chain(&adsp_notifier_list, event, NULL);
#endif
}

/* user-space event notify */
static int adsp_user_event_notify(struct notifier_block *nb,
				  unsigned long event, void *ptr)
{
	struct device *dev = adspsys->mdev.this_device;
	int ret = 0;

	if (!dev)
		return NOTIFY_DONE;

	switch (event) {
	case ADSP_EVENT_STOP:
		ret = kobject_uevent(&dev->kobj, KOBJ_OFFLINE);
		break;
	case ADSP_EVENT_READY:
		ret = kobject_uevent(&dev->kobj, KOBJ_ONLINE);
		break;
	default:
		pr_info("%s, ignore event %lu", __func__, event);
		break;
	}

	if (ret)
		pr_info("%s, uevnet(%lu) fail, ret %d", __func__, event, ret);

	return NOTIFY_OK;
}

struct notifier_block adsp_uevent_notifier = {
	.notifier_call = adsp_user_event_notify,
	.priority = AUDIO_HAL_FEATURE_PRI,
};

#if IS_ENABLED(CONFIG_PM)
static int adsp_pm_suspend_prepare(void)
{
	int cid = 0, ret = 0;
	struct adsp_priv *pdata = NULL;

	for (cid = get_adsp_core_total() - 1; cid >= 0; cid--) {
		pdata = adsp_cores[cid];

		if (pdata->state == ADSP_RUNNING) {
			ret = flush_suspend_work(pdata->id);

			pr_info("%s, flush_suspend_work ret %d, cid %d",
				__func__, ret, cid);
		}
	}

	if (is_adsp_system_running()) {
		adsp_timesync_suspend(APTIME_FREEZE);
		pr_info("%s, time sync freeze", __func__);

		adsp_smc_send(MTK_ADSP_KERNEL_OP_ENTER_LP, 0, 0);
	}

	return NOTIFY_DONE;
}

static int adsp_pm_post_suspend(void)
{
	if (is_adsp_system_running()) {
		adsp_timesync_resume();
		pr_info("%s, time sync unfreeze", __func__);

		adsp_smc_send(MTK_ADSP_KERNEL_OP_LEAVE_LP, 0, 0);
	}

	return NOTIFY_DONE;
}

static int adsp_pm_event(struct notifier_block *notifier,
			 unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_POST_HIBERNATION:
		pr_notice("[ADSP] %s: reboot\n", __func__);
		adsp_reset();
		return NOTIFY_DONE;
	case PM_SUSPEND_PREPARE:
		return adsp_pm_suspend_prepare();
	case PM_POST_SUSPEND:
		return adsp_pm_post_suspend();
	}
	return NOTIFY_OK;
}

static struct notifier_block adsp_pm_notifier_block = {
	.notifier_call = adsp_pm_event,
	.priority = 0,
};
#endif

void adsp_select_clock_mode(enum adsp_clk_mode mode)
{
	if (adspsys)
		adspsys->clk_ops.select(mode);
}

int adsp_enable_clock(void)
{
	return adspsys ? adspsys->clk_ops.enable() : 0;
}

void adsp_disable_clock(void)
{
	if (adspsys)
		adspsys->clk_ops.disable();
}

void switch_adsp_power(bool on)
{
	if (on) {
		adsp_enable_clock();
		adsp_select_clock_mode(CLK_DEFAULT_INIT);
	} else {
		adsp_select_clock_mode(CLK_LOW_POWER);
		adsp_disable_clock();
	}
}

void adsp_latch_dump_region(bool en)
{
	/* MUST! latch/unlatch region symmetric */
	if (en) {
		mutex_lock(&access_lock);
		adsp_smc_send(MTK_ADSP_KERNEL_OP_CFG_LATCH, true, 0);
	} else {
		adsp_smc_send(MTK_ADSP_KERNEL_OP_CFG_LATCH, false, 0);
		mutex_unlock(&access_lock);
	}
}

void adsp_core_start(u32 cid)
{
	mutex_lock(&access_lock);
	adsp_smc_send(MTK_ADSP_KERNEL_OP_CORE_START, cid, 0);
	mutex_unlock(&access_lock);
}

void adsp_core_stop(u32 cid)
{
	adsp_smc_send(MTK_ADSP_KERNEL_OP_CORE_STOP, cid, 0);
}

int adsp_reset(void)
{
#ifdef CFG_RECOVERY_SUPPORT
	int ret = 0;
	unsigned int cid = 0;
	struct adsp_priv *pdata;

	if (!is_adsp_axibus_idle()) {
		pr_info("%s, adsp_axibus busy try again", __func__);
		return -EAGAIN;
	}

	/* clear adsp cfg */
	adsp_smc_send(MTK_ADSP_KERNEL_OP_SYS_CLEAR, 0, 0);

	/* choose default clk mux */
	adsp_select_clock_mode(CLK_LOW_POWER);
	adsp_select_clock_mode(CLK_DEFAULT_INIT);

	/* reload adsp */
	adsp_smc_send(MTK_ADSP_KERNEL_OP_RELOAD, 0, 0);

	/* restart adsp */
	for (cid = 0; cid < get_adsp_core_total(); cid++) {
		pdata = adsp_cores[cid];

		reinit_completion(&pdata->done);
		adsp_core_start(cid);
		ret = wait_for_completion_timeout(&pdata->done, HZ);

		if (unlikely(ret == 0)) {
			adsp_core_stop(cid);
			pr_warn("%s, core %d reset timeout\n", __func__, cid);
			return -ETIME;
		}
	}

	for (cid = 0; cid < get_adsp_core_total(); cid++) {
		pdata = adsp_cores[cid];

		if (pdata->ops->after_bootup)
			pdata->ops->after_bootup(pdata);
	}

	pr_info("[ADSP] reset adsp done\n");
#endif
	return 0;
}

static void adsp_ready_ipi_handler(int id, void *data, unsigned int len)
{
	unsigned int cid = *(unsigned int *)data;
	struct adsp_priv *pdata = get_adsp_core_by_id(cid);

	if (unlikely(!pdata))
		return;

	if (get_adsp_state(pdata) != ADSP_RUNNING) {
		set_adsp_state(pdata, ADSP_RUNNING);
		complete(&pdata->done);
	}
}

static void adsp_suspend_ipi_handler(int id, void *data, unsigned int len)
{
	unsigned int cid = *(unsigned int *)data;
	struct adsp_priv *pdata = get_adsp_core_by_id(cid);

	if (unlikely(!pdata))
		return;

	complete(&pdata->done);
}

static int adsp_system_init(void)
{
	int ret = 0;

	if (!adspsys)
		return -EFAULT;

	adsp_hardware_init(adspsys);

	/* ipi of ready/suspend ack */
	adsp_ipi_registration(ADSP_IPI_ADSP_A_READY,
			      adsp_ready_ipi_handler,
			      "adsp_ready");
	adsp_ipi_registration(ADSP_IPI_DVFS_SUSPEND,
			      adsp_suspend_ipi_handler,
			      "adsp_suspend_ack");

	/* time sync with adsp */
	adsp_timesync_init();

	/* hw semaphore */
	adsp_semaphore_init(adspsys->desc->semaphore_ways,
			    adspsys->desc->semaphore_ctrl,
			    adspsys->desc->semaphore_retry);

	/* exception init */
	adspsys->workq = alloc_workqueue("adsp_wq", WORK_CPU_UNBOUND | WQ_HIGHPRI, 0);
	init_waitqueue_head(&adspsys->waitq);
	init_adsp_exception_control(adspsys->dev, adspsys->workq, &adspsys->waitq);

#if IS_ENABLED(CONFIG_PM)
	ret = register_pm_notifier(&adsp_pm_notifier_block);
	if (ret)
		pr_warn("[ADSP] failed to register PM notifier %d\n", ret);
#endif
	/* register misc device */
	adspsys->mdev.minor = MISC_DYNAMIC_MINOR;
	adspsys->mdev.name = "adsp";
	adspsys->mdev.fops = &adspsys_file_ops;
	adspsys->mdev.groups = adspsys_attr_groups;

	ret = misc_register(&adspsys->mdev);
	if (unlikely(ret != 0))
		pr_warn("%s(), misc_register fail, %d\n", __func__, ret);

	/* kernel event to userspace */
	adsp_register_notify(&adsp_uevent_notifier);

	return ret;
}

int adsp_system_bootup(void)
{
	int ret = 0;
	unsigned int cid = 0;
	struct adsp_priv *pdata;

	ret = adsp_system_init();
	if (ret)
		goto ERROR;

	switch_adsp_power(true);
	adsp_smc_send(MTK_ADSP_KERNEL_OP_SYS_CLEAR, 0, 0);

	for (cid = 0; cid < get_adsp_core_total(); cid++) {
		pdata = adsp_cores[cid];
		if (unlikely(!pdata)) {
			ret = -EFAULT;
			goto ERROR;
		}

		ret = pdata->ops->initialize(pdata);
		if (unlikely(ret)) {
			pr_warn("%s, initialize %d is fail\n", __func__, cid);
			goto ERROR;
		}

		reinit_completion(&pdata->done);
		adsp_core_start(cid);
		ret = wait_for_completion_timeout(&pdata->done, 2 * HZ);

		if (unlikely(ret == 0)) {
			adsp_core_stop(cid);
			pr_warn("%s, core %d boot_up timeout\n", __func__, cid);
			ret = -ETIME;
			goto ERROR;
		}
	}

	adsp_register_feature(SYSTEM_FEATURE_ID); /* regi for trigger suspend */

	for (cid = 0; cid < get_adsp_core_total(); cid++) {
		pdata = adsp_cores[cid];

		if (pdata->ops->after_bootup)
			pdata->ops->after_bootup(pdata);
	}

	adsp_deregister_feature(SYSTEM_FEATURE_ID);
	pr_info("%s done\n", __func__);
	return 0;

ERROR:
	pr_info("%s fail ret(%d)\n", __func__, ret);
	return ret;
}
EXPORT_SYMBOL(adsp_system_bootup);

void register_adspsys(struct adspsys_priv *mt_adspsys)
{
	adspsys = mt_adspsys;
	pr_info("%s(), %p done\n", __func__, mt_adspsys);
}
EXPORT_SYMBOL(register_adspsys);

void register_adsp_core(struct adsp_priv *pdata)
{
	adsp_cores[pdata->id] = pdata;
	pr_info("%s(), id %d, %p done\n", __func__, pdata->id, pdata);
}
EXPORT_SYMBOL(register_adsp_core);

MODULE_AUTHOR("Chien-Wei Hsu <Chien-Wei.Hsu@mediatek.com>");
MODULE_DESCRIPTION("MTK AUDIO DSP Device Driver");
MODULE_LICENSE("GPL v2");
