// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/ktime.h>
#include <linux/pm_runtime.h>
#include <slbc_ops.h>
#include "adsp_mbox.h"
#include "adsp_logger.h"
#include "adsp_timesync.h"
#include "adsp_semaphore.h"
#include "adsp_excep.h"
#include "adsp_reg.h"
#include "adsp_platform.h"
#include "adsp_platform_driver.h"
#include "adsp_core.h"

const struct attribute_group *adsp_core_attr_groups[] = {
	&adsp_default_attr_group,
	NULL,
};

static int slb_memory_control(bool en);

int adsp_after_bootup(struct adsp_priv *pdata)
{
#ifdef BRINGUP_ADSP
	/* disable adsp suspend by registering feature */
	_adsp_register_feature(pdata->id, SYSTEM_FEATURE_ID, 0);
#endif
	/* force release slb buffer */
	while (slb_memory_control(false) > 0)
		;

	return adsp_awake_unlock(pdata->id);
}
EXPORT_SYMBOL(adsp_after_bootup);

static bool is_adsp_core_suspend(struct adsp_priv *pdata)
{
	u32 status = 0;

	if (unlikely(!pdata))
		return false;

	adsp_copy_from_sharedmem(pdata,
				 ADSP_SHAREDMEM_SYS_STATUS,
				 &status, sizeof(status));

	if (pdata->id == ADSP_A_ID) {
		return check_hifi_status(ADSP_A_IS_WFI) &&
		       check_hifi_status(ADSP_AXI_BUS_IS_IDLE) &&
		       (status == ADSP_SUSPEND);
	} else { /* ADSP_B_ID */
		return check_hifi_status(ADSP_B_IS_WFI) &&
		       (status == ADSP_SUSPEND);
	}
}

static void show_adsp_core_suspend(struct adsp_priv *pdata)
{
	u32 status = 0;

	if (unlikely(!pdata))
		return;

	adsp_copy_from_sharedmem(pdata,
				 ADSP_SHAREDMEM_SYS_STATUS,
				 &status, sizeof(status));

	if (pdata->id == ADSP_A_ID)
		pr_info("%s(), IS_WFI(%d), IS_BUS_IDLE(%d), STATUS(%d)", __func__,
			check_hifi_status(ADSP_A_IS_WFI),
			check_hifi_status(ADSP_AXI_BUS_IS_IDLE),
			status);
	else /* ADSP_B_ID */
		pr_info("%s(), IS_WFI(%d), STATUS(%d)", __func__,
			check_hifi_status(ADSP_B_IS_WFI),
			status);
}

static int wait_another_core_suspend(struct adsp_priv *pdata)
{
	if (!wait_event_timeout(adspsys->waitq,
				(adsp_cores[ADSP_B_ID]->state == ADSP_SUSPEND) ||
				(get_adsp_state(pdata) == ADSP_RESET),
				2 * HZ)) {
		return -EBUSY;
	}
	return 0;
}

int adsp_core0_suspend(void)
{
	int ret = 0, retry = 10;
	u32 status = 0;
	struct adsp_priv *pdata = adsp_cores[ADSP_A_ID];
	ktime_t start = ktime_get();

	if (get_adsp_state(pdata) == ADSP_RUNNING) {
		reinit_completion(&pdata->done);
		adsp_timesync_suspend(APTIME_UNFREEZE);

		if (adsp_push_message(ADSP_IPI_DVFS_SUSPEND, &status,
				      sizeof(status), 2000, pdata->id) != ADSP_IPI_DONE) {
			ret = -EPIPE;
			goto ERROR;
		}

		/* wait core suspend ack timeout 2s */
		ret = wait_for_completion_timeout(&pdata->done, 2 * HZ);
		if (!ret) {
			ret = -ETIMEDOUT;
			goto ERROR;
		}

		while (--retry && !is_adsp_core_suspend(pdata))
			usleep_range(100, 200);

		if (retry == 0 || get_adsp_state(pdata) == ADSP_RESET) {
			show_adsp_core_suspend(pdata);
			ret = -ETIME;
			goto ERROR;
		}

		/* if have more core, wait it suspend done */
		if (get_adsp_core_total() > 1) {
			ret = wait_another_core_suspend(pdata);
			if (ret)
				goto ERROR;
		}

		if (get_adsp_state(pdata) == ADSP_RESET) {
			ret = -EFAULT;
			goto ERROR;
		}

		adsp_core_stop(pdata->id);
		switch_adsp_power(false);
		set_adsp_state(pdata, ADSP_SUSPEND);
	}
	pr_info("%s(), done elapse %lld us", __func__,
		ktime_us_delta(ktime_get(), start));
	return 0;
ERROR:
	pr_warn("%s(), can't going to suspend, ret(%d)\n", __func__, ret);
	adsp_mbox_dump();
	adsp_aed_dispatch(EXCEP_KERNEL, pdata);
	return ret;
}

int adsp_core0_resume(void)
{
	int ret = 0;
	struct adsp_priv *pdata = adsp_cores[ADSP_A_ID];
	ktime_t start = ktime_get();

	if (get_adsp_state(pdata) == ADSP_SUSPEND) {
		switch_adsp_power(true);
		reinit_completion(&pdata->done);
		adsp_core_start(pdata->id);
		ret = wait_for_completion_timeout(&pdata->done, 2 * HZ);

		if (get_adsp_state(pdata) != ADSP_RUNNING) {
			pr_warn("%s, can't going to resume\n", __func__);
			adsp_mbox_dump();
			adsp_aed_dispatch(EXCEP_KERNEL, pdata);
			return -ETIME;
		}

		adsp_timesync_resume();

		pr_info("%s(), done elapse %lld us", __func__,
			ktime_us_delta(ktime_get(), start));
	}
	return 0;
}

int adsp_core1_suspend(void)
{
	int ret = 0, retry = 10;
	u32 status = 0;
	struct adsp_priv *pdata = adsp_cores[ADSP_B_ID];
	ktime_t start = ktime_get();

	if (get_adsp_state(pdata) == ADSP_RUNNING) {
		reinit_completion(&pdata->done);
		if (adsp_push_message(ADSP_IPI_DVFS_SUSPEND, &status,
				      sizeof(status), 2000, pdata->id) != ADSP_IPI_DONE) {
			ret = -EPIPE;
			goto ERROR;
		}

		/* wait core suspend ack timeout 2s */
		ret = wait_for_completion_timeout(&pdata->done, 2 * HZ);
		if (!ret) {
			ret = -ETIMEDOUT;
			goto ERROR;
		}

		while (--retry && !is_adsp_core_suspend(pdata))
			usleep_range(100, 200);

		if (retry == 0 || get_adsp_state(pdata) == ADSP_RESET) {
			show_adsp_core_suspend(pdata);
			ret = -ETIME;
			goto ERROR;
		}

		adsp_core_stop(pdata->id);
		set_adsp_state(pdata, ADSP_SUSPEND);

		/* notify another core suspend done */
		wake_up(&adspsys->waitq);
	}
	pr_info("%s(), done elapse %lld us", __func__,
		ktime_us_delta(ktime_get(), start));
	return 0;
ERROR:
	pr_warn("%s(), can't going to suspend, ret(%d)\n", __func__, ret);
	adsp_mbox_dump();
	adsp_aed_dispatch(EXCEP_KERNEL, pdata);
	return ret;
}

int adsp_core1_resume(void)
{
	int ret = 0;
	struct adsp_priv *pdata = adsp_cores[ADSP_B_ID];
	ktime_t start = ktime_get();

	if (get_adsp_state(pdata) == ADSP_SUSPEND) {
		/* core A force awake, for resume core B faster */
		adsp_awake_lock(ADSP_A_ID);

		reinit_completion(&pdata->done);
		adsp_core_start(pdata->id);
		ret = wait_for_completion_timeout(&pdata->done, 2 * HZ);

		if (get_adsp_state(pdata) != ADSP_RUNNING) {
			pr_warn("%s, can't going to resume\n", __func__);
			adsp_mbox_dump();
			adsp_aed_dispatch(EXCEP_KERNEL, pdata);
			return -ETIME;
		}

		adsp_awake_unlock(ADSP_A_ID);

		pr_info("%s(), done elapse %lld us", __func__,
			ktime_us_delta(ktime_get(), start));
	}

	return 0;
}

void adsp_logger_init0_cb(struct work_struct *ws)
{
	int ret;
	uint64_t info[6];

	info[0] = adsp_get_reserve_mem_phys(ADSP_A_LOGGER_MEM_ID);
	info[1] = adsp_get_reserve_mem_size(ADSP_A_LOGGER_MEM_ID);
	info[2] = adsp_get_reserve_mem_phys(ADSP_A_CORE_DUMP_MEM_ID);
	info[3] = adsp_get_reserve_mem_size(ADSP_A_CORE_DUMP_MEM_ID);
	info[4] = adsp_get_reserve_mem_phys(ADSP_A_DEBUG_DUMP_MEM_ID);
	info[5] = adsp_get_reserve_mem_size(ADSP_A_DEBUG_DUMP_MEM_ID);

	_adsp_register_feature(ADSP_A_ID, ADSP_LOGGER_FEATURE_ID, 0);

	ret = adsp_push_message(ADSP_IPI_LOGGER_INIT, (void *)info,
		sizeof(info), 20, ADSP_A_ID);

	_adsp_deregister_feature(ADSP_A_ID, ADSP_LOGGER_FEATURE_ID, 0);

	if (ret != ADSP_IPI_DONE)
		pr_err("[ADSP]logger initial fail, ipi ret=%d\n", ret);
}

void adsp_logger_init1_cb(struct work_struct *ws)
{
	int ret;
	uint64_t info[6];

	info[0] = adsp_get_reserve_mem_phys(ADSP_B_LOGGER_MEM_ID);
	info[1] = adsp_get_reserve_mem_size(ADSP_B_LOGGER_MEM_ID);
	info[2] = adsp_get_reserve_mem_phys(ADSP_B_CORE_DUMP_MEM_ID);
	info[3] = adsp_get_reserve_mem_size(ADSP_B_CORE_DUMP_MEM_ID);
	info[4] = adsp_get_reserve_mem_phys(ADSP_B_DEBUG_DUMP_MEM_ID);
	info[5] = adsp_get_reserve_mem_size(ADSP_B_DEBUG_DUMP_MEM_ID);

	_adsp_register_feature(ADSP_B_ID, ADSP_LOGGER_FEATURE_ID, 0);

	ret = adsp_push_message(ADSP_IPI_LOGGER_INIT, (void *)info,
		sizeof(info), 20, ADSP_B_ID);

	_adsp_deregister_feature(ADSP_B_ID, ADSP_LOGGER_FEATURE_ID, 0);

	if (ret != ADSP_IPI_DONE)
		pr_err("[ADSP]logger initial fail, ipi ret=%d\n", ret);
}

static struct slbc_data slb_data = {
	.uid = UID_HIFI3,
	.type = TP_BUFFER
};

static int slb_memory_control(bool en)
{
	static DEFINE_MUTEX(lock);
	static int use_cnt;
	int ret = 0;

	mutex_lock(&lock);
	if (en) {
		if (use_cnt == 0) {
			ret = slbc_request(&slb_data);
			if (ret)
				goto EXIT;
			slbc_power_on(&slb_data);
		}
		use_cnt++;
	} else {
		if (use_cnt == 0)
			goto EXIT;

		if (--use_cnt == 0) {
			slbc_power_off(&slb_data);
			ret = slbc_release(&slb_data);
		}
	}
EXIT:
	if (ret)
		pr_info("%s, fail slbc request %d, ret %d, cnt %d",
			__func__, en, ret, use_cnt);
	mutex_unlock(&lock);
	return ret < 0 ? ret : use_cnt;
}

static void adsp_slb_init_handler(int id, void *data, unsigned int len)
{
	u32 cid = *(u32 *)data;
	u32 request = *((u32 *)data + 1);
	unsigned long info[2] = {0};
	int ret;

	ret = slb_memory_control(request);

	if (ret >= 0) {
		info[0] = (unsigned long)slb_data.paddr;
		info[1] = (unsigned long)slb_data.size;
	}
	pr_info("%s(), addr:0x%lx, size:0x%lx, cid %d, request %d, ret %d",
		__func__, info[0], info[1], cid, request, ret);

	_adsp_register_feature(cid, SYSTEM_FEATURE_ID, 0);

	ret = adsp_push_message(ADSP_IPI_SLB_INIT, info, sizeof(info), 20, cid);

	_adsp_deregister_feature(cid, SYSTEM_FEATURE_ID, 0);

	if (ret != ADSP_IPI_DONE)
		pr_info("%s, fail send msg to cid %d, ret %d", __func__, cid, ret);
}

int adsp_core_common_init(struct adsp_priv *pdata)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_DEBUG_FS)
	char name[10] = {0};

	snprintf(name, 10, "audiodsp%d", pdata->id);
	pdata->debugfs = debugfs_create_file(name, S_IFREG | 0644, NULL,
					     pdata, &adsp_debug_ops);
#endif

	/* wdt irq */
	adsp_irq_registration(pdata->id, ADSP_IRQ_WDT_ID, adsp_wdt_handler, pdata);

	/* mailbox */
	pdata->recv_mbox->prdata = &pdata->id;

	/* slb init ipi */
	adsp_ipi_registration(ADSP_IPI_SLB_INIT, adsp_slb_init_handler, "slb_init");

#if IS_ENABLED(CONFIG_MTK_AUDIODSP_DEBUG_SUPPORT)
	/* register misc device */
	pdata->mdev.minor = MISC_DYNAMIC_MINOR;
	pdata->mdev.name = pdata->name;
	pdata->mdev.fops = &adsp_core_file_ops;
	pdata->mdev.groups = adsp_core_attr_groups;

	ret = misc_register(&pdata->mdev);
	if (unlikely(ret != 0))
		pr_info("%s(), misc_register fail, %d\n", __func__, ret);
#endif
	return ret;
}

int adsp_core0_init(struct adsp_priv *pdata)
{
	int ret = 0;

	init_adsp_feature_control(pdata->id, pdata->feature_set, 1100,
				adspsys->workq,
				adsp_core0_suspend,
				adsp_core0_resume);

	/* logger */
	pdata->log_ctrl = adsp_logger_init(ADSP_A_LOGGER_MEM_ID, adsp_logger_init0_cb);

	ret = adsp_core_common_init(pdata);

	return ret;
}
EXPORT_SYMBOL(adsp_core0_init);

int adsp_core1_init(struct adsp_priv *pdata)
{
	int ret = 0;

	init_adsp_feature_control(pdata->id, pdata->feature_set, 900,
				adspsys->workq,
				adsp_core1_suspend,
				adsp_core1_resume);

	/* logger */
	pdata->log_ctrl = adsp_logger_init(ADSP_B_LOGGER_MEM_ID, adsp_logger_init1_cb);

	ret = adsp_core_common_init(pdata);

	return ret;
}
EXPORT_SYMBOL(adsp_core1_init);

