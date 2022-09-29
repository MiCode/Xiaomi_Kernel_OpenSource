// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/vmalloc.h>         /* needed by vmalloc */
#include <linux/slab.h>            /* needed by kmalloc */
#include <linux/sysfs.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/io.h>
#if IS_ENABLED(CONFIG_PM_WAKELOCKS)
#include <linux/pm_wakeup.h>
#endif
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif

#include "adsp_reg.h"
#include "adsp_core.h"
#include "adsp_clk.h"
#include "adsp_platform_driver.h"
#include "adsp_excep.h"
#include "adsp_logger.h"

#define ADSP_MAGIC_PATTERN      (0xAD5BAD5B)
#define ADSP_MISC_BUF_SIZE      0x10000 //64KB
#define ADSP_TEST_EE_PATTERN    "Assert-Test"

static char adsp_ke_buffer[ADSP_KE_DUMP_LEN];
static struct adsp_exception_control excep_ctrl;
static bool suppress_test_ee;

static u32 copy_from_buffer(void *dest, size_t destsize, const void *src,
			    size_t srcsize, u32 offset, size_t request)
{
	/* if request == -1, offset == 0, copy full srcsize */
	if (offset + request > srcsize)
		request = srcsize - offset;

	/* if destsize == -1, don't check the request size */
	if (!src || !dest || destsize < request) {
		pr_warn("%s, buffer null or not enough space", __func__);
		return 0;
	}

	memcpy(dest, src + offset, request);

	return request;
}

static inline u32 copy_from_adsp_shared_memory(void *buf, u32 offset,
					size_t size, int id)
{
	void *mem_addr = adsp_get_reserve_mem_virt(id);
	size_t mem_size = adsp_get_reserve_mem_size(id);

	if (!mem_addr)
		return 0;

	return copy_from_buffer(buf, -1, mem_addr, mem_size, offset, size);
}

static u32 write_mem_header(void *buf, size_t size, const char *mem_name, u32 mem_size)
{
	struct adsp_mem_header hd = {0};

	if (!buf || size < sizeof(hd) || !mem_name || !mem_size)
		return 0;

	hd.magic = ADSP_MAGIC_PATTERN;
	hd.size = mem_size;
	strncpy(hd.name, mem_name, sizeof(hd.name));
	memcpy(buf, &hd, sizeof(hd));

	return sizeof(hd);
}

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
static inline u32 dump_adsp_shared_memory(void *buf, size_t size, int id, const char *mem_name)
{
	u32 n = 0;
	void *mem_addr = adsp_get_reserve_mem_virt(id);
	size_t mem_size = adsp_get_reserve_mem_size(id);

	if (!mem_addr || !mem_size || !mem_name)
		return 0;

	n += write_mem_header(buf + n, size - n, mem_name, mem_size);
	n += copy_from_buffer(buf + n, size - n, mem_addr, mem_size, 0, -1);

	return n;
}

static u32 dump_adsp_internal_memory(void *buf, size_t size, struct adsp_priv *pdata)
{
	u32 n = 0;

	adsp_enable_clock();
	adsp_latch_dump_region(true);

	n += write_mem_header(buf + n, size - n, "cfg", adspsys->cfg_size);
	n += copy_from_buffer(buf + n, size - n, adspsys->cfg, adspsys->cfg_size, 0, -1);

	n += write_mem_header(buf + n, size - n, "cfg2", adspsys->cfg2_size);
	n += copy_from_buffer(buf + n, size - n, adspsys->cfg2, adspsys->cfg2_size, 0, -1);

	n += write_mem_header(buf + n, size - n, "itcm", pdata->itcm_size);
	n += copy_from_buffer(buf + n, size - n, pdata->itcm, pdata->itcm_size, 0, -1);

	n += write_mem_header(buf + n, size - n, "dtcm", pdata->dtcm_size);
	n += copy_from_buffer(buf + n, size - n, pdata->dtcm, pdata->dtcm_size, 0, -1);

	adsp_latch_dump_region(false);
	adsp_disable_clock();

	/* sysram not rely on adsp clk */
	n += write_mem_header(buf + n, size - n, "sysram", pdata->sysram_size);
	n += copy_from_buffer(buf + n, size - n, pdata->sysram, pdata->sysram_size, 0, -1);

	return n;
}

static int dump_buffer(struct adsp_exception_control *ctrl, int coredump_id)
{
	u32 total = 0, n = 0;
	void *buf = NULL;
	int ret = 0;
	struct adsp_priv *pdata = NULL;


	if (!ctrl || !ctrl->priv_data)
		return -1;

	pdata = (struct adsp_priv *)ctrl->priv_data;

	if (ctrl->buf_backup) {
		/* wait last dump done, and release buf_backup */
		ret = wait_for_completion_timeout(&ctrl->done, 10 * HZ);

		/* if not release buf, return EBUSY */
		if (ctrl->buf_backup)
			return -EBUSY;
	}

	total = 8 * sizeof(struct adsp_mem_header)
		+ adspsys->cfg_size
		+ adspsys->cfg2_size
		+ pdata->itcm_size
		+ pdata->dtcm_size
		+ pdata->sysram_size
		+ adsp_get_reserve_mem_size(coredump_id)
		+ adsp_get_reserve_mem_size(ADSP_A_LOGGER_MEM_ID)
		+ adsp_get_reserve_mem_size(ADSP_B_LOGGER_MEM_ID);

	buf = vzalloc(total);
	if (!buf)
		return -ENOMEM;

	n += dump_adsp_internal_memory(buf + n, total - n, pdata);
	n += dump_adsp_shared_memory(buf + n, total - n, coredump_id, "coredump");
	n += dump_adsp_shared_memory(buf + n, total - n, ADSP_A_LOGGER_MEM_ID, "log_a");
	n += dump_adsp_shared_memory(buf + n, total - n, ADSP_B_LOGGER_MEM_ID, "log_b");

	reinit_completion(&ctrl->done);
	ctrl->buf_backup = buf;
	ctrl->buf_size = total;

	pr_debug("%s, vmalloc size %u, buffer %p, dump_size %u",
		 __func__, total, buf, n);
	return n;
}

static void adsp_exception_dump(struct adsp_exception_control *ctrl)
{
	char detail[ADSP_AED_STR_LEN];
	int db_opt = DB_OPT_DEFAULT;
	char *aed_type;
	bool dump_flag = true;
	int ret = 0, n = 0, coredump_id = 0, coredump_size = 0;
	struct adsp_priv *pdata = (struct adsp_priv *)ctrl->priv_data;
	struct adsp_coredump *coredump;

	/* get adsp title and exception type*/
	switch (ctrl->excep_id) {
	case EXCEP_BOOTUP:
		aed_type = "boot exception";
		break;
	case EXCEP_RUNTIME:
		aed_type = "runtime exception";
		db_opt |= DB_OPT_FTRACE;
		break;
	case EXCEP_KERNEL:
		aed_type = "kernel exception";
		db_opt |= DB_OPT_FTRACE;
		break;
	default:
		dump_flag = false;
		aed_type = "unknown exception";
		break;
	}

	if (pdata->id == ADSP_A_ID)
		coredump_id = ADSP_A_CORE_DUMP_MEM_ID;
	else
		coredump_id = ADSP_B_CORE_DUMP_MEM_ID;

	coredump = adsp_get_reserve_mem_virt(coredump_id);
	coredump_size = adsp_get_reserve_mem_size(coredump_id);

	if (suppress_test_ee && coredump
	    && strstr(coredump->assert_log, ADSP_TEST_EE_PATTERN)) {
		pr_info("%s, suppress Test EE dump", __func__);
		msleep(20);
		return;
	}

	if (dump_flag) {
		ret = dump_buffer(ctrl, coredump_id);
		if (ret < 0)
			pr_info("%s, excep dump fail ret(%d)", __func__, ret);
	}

	n += snprintf(detail + n, ADSP_AED_STR_LEN - n, "%s %s\n",
		      pdata->name, aed_type);
	if (coredump) {
		n += snprintf(detail + n, ADSP_AED_STR_LEN - n,
			      "adsp pc=0x%08x,exccause=0x%x,excvaddr=0x%x\n",
			      coredump->pc,
			      coredump->exccause,
			      coredump->excvaddr);
		n += snprintf(detail + n, ADSP_AED_STR_LEN - n,
			      "CRDISPATCH_KEY:ADSP exception/%s\n",
			      coredump->task_name);
		n += snprintf(detail + n, ADSP_AED_STR_LEN - n, "%s",
			      coredump->assert_log);
	}
	pr_info("%s", detail);

	/* adsp aed api, only detail information available*/
	aed_common_exception_api("adsp", (const int *)coredump, coredump_size,
				 NULL, 0, detail, db_opt);
}
#endif

void adsp_aed_worker(struct work_struct *ws)
{
	struct adsp_exception_control *ctrl = container_of(ws,
						struct adsp_exception_control,
						aed_work);
	struct adsp_priv *pdata = NULL;
	int cid = 0, ret = 0, retry = 0;
#if IS_ENABLED(CONFIG_PM_WAKELOCKS)
	/* wake lock AP*/
	__pm_stay_awake(ctrl->wakeup_lock);
#endif
	/* stop adsp, set reset state */
	for (cid = 0; cid < get_adsp_core_total(); cid++) {
		pdata = get_adsp_core_by_id(cid);
		set_adsp_state(pdata, ADSP_RESET);
		complete_all(&pdata->done);
	}

	/* force wake up if suspend thread wait reset event */
	if (ctrl->waitq)
		wake_up(ctrl->waitq);

	adsp_register_feature(SYSTEM_FEATURE_ID);
	adsp_extern_notify_chain(ADSP_EVENT_STOP);

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	/* exception dump */
	adsp_exception_dump(ctrl);
#endif
	/* reset adsp */
	adsp_enable_clock();
	for (retry = 0; retry < ADSP_RESET_RETRY_MAXTIME; retry++) {
		ret = adsp_reset();

		if (ret == 0)
			break;

		/* reset fail & retry */
		pr_info("%s, reset retry.... (%d)", __func__, retry);
		msleep(20);
	}
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	if (ret) {
		pr_info("%s, adsp dead, wait dump dead body", __func__);
		if (is_infrabus_timeout())
			BUG(); /* reboot for bus dump */
		else
			aee_kernel_exception_api(__FILE__, __LINE__, DB_OPT_DEFAULT,
						 "[ADSP]",
						 "ASSERT: ADSP DEAD! Recovery Fail");
	}
#endif
	adsp_disable_clock();
	adsp_extern_notify_chain(ADSP_EVENT_READY);
	adsp_deregister_feature(SYSTEM_FEATURE_ID);
#if IS_ENABLED(CONFIG_PM_WAKELOCKS)
	__pm_relax(ctrl->wakeup_lock);
#endif
}

bool adsp_aed_dispatch(enum adsp_excep_id type, void *data)
{
	struct adsp_exception_control *ctrl = &excep_ctrl;

	if (work_busy(&ctrl->aed_work))
		return false;

	ctrl->excep_id = type;
	ctrl->priv_data = data;
	return queue_work(ctrl->workq, &ctrl->aed_work);
}

static void adsp_wdt_counter_reset(struct timer_list *t)
{
	excep_ctrl.wdt_counter = 0;
	pr_info("[ADSP] %s\n", __func__);
}

/*
 * init a work struct
 */
int init_adsp_exception_control(struct device *dev,
				struct workqueue_struct *workq,
				struct wait_queue_head *waitq)
{
	struct adsp_exception_control *ctrl = &excep_ctrl;

	ctrl->waitq = waitq;
	ctrl->workq = workq;
	ctrl->buf_backup = NULL;
	ctrl->buf_size = 0;
	init_completion(&ctrl->done);
	INIT_WORK(&ctrl->aed_work, adsp_aed_worker);
#if IS_ENABLED(CONFIG_PM_WAKELOCKS)
	ctrl->wakeup_lock = wakeup_source_register(dev, "adsp wakelock");
#endif
	timer_setup(&ctrl->wdt_timer, adsp_wdt_counter_reset, 0);

	return 0;
}

void adsp_wdt_handler(int irq, void *data, int cid)
{
	struct adsp_priv *pdata = (struct adsp_priv *)data;

	if (!adsp_aed_dispatch(EXCEP_RUNTIME, data))
		pr_info("%s, already resetting, ignore core%d wdt",
			__func__, pdata->id);

}

void get_adsp_misc_buffer(unsigned long *vaddr, unsigned long *size)
{
	struct adsp_priv *pdata = NULL;
	struct log_info_s *log_info = NULL;
	struct buffer_info_s *buf_info = NULL;
	void *buf = adsp_ke_buffer;
	void *addr = NULL;
	unsigned int w_pos, r_pos, buf_size;
	unsigned int data_len[2];
	unsigned int id = 0, n = 0;
	unsigned int part_len = ADSP_MISC_BUF_SIZE / ADSP_CORE_TOTAL;

	memset(buf, 0, ADSP_KE_DUMP_LEN);
	if (!adspsys) {
		pr_info("adsp image not load, skip dump");
		goto ERROR;
	}

	for (id = 0; id < get_adsp_core_total(); id++) {
		w_pos = 0;
		pdata = get_adsp_core_by_id(id);
		if (!pdata || !pdata->log_ctrl ||
		    !pdata->log_ctrl->inited || !pdata->log_ctrl->priv)
			goto ERROR;

		log_info = (struct log_info_s *)pdata->log_ctrl->priv;
		buf_info = (struct buffer_info_s *)(pdata->log_ctrl->priv
						  + log_info->info_ofs);

		buf_size = log_info->buff_size;
		memcpy_fromio(&w_pos, &buf_info->w_pos, sizeof(w_pos));

		if (w_pos >= buf_size)
			w_pos -= buf_size;

		if (w_pos < part_len) {
			r_pos = buf_size + w_pos - part_len;
			data_len[0] = part_len - w_pos;
			data_len[1] = w_pos;
		} else {
			r_pos = w_pos - part_len;
			data_len[0] = part_len;
			data_len[1] = 0;
		}

		addr = pdata->log_ctrl->priv + log_info->buff_ofs;
		memcpy(buf + n, addr + r_pos, data_len[0]);
		n += data_len[0];
		memcpy(buf + n, addr, data_len[1]);
		n += data_len[1];
	}

	/* return value */
	*vaddr = (unsigned long)buf;
	*size = n;
	return;

ERROR:
	/* return value */
	*vaddr = (unsigned long)buf;
	*size = 0;
}
EXPORT_SYMBOL(get_adsp_misc_buffer);

void get_adsp_aee_buffer(unsigned long *vaddr, unsigned long *size)
{
	struct adsp_priv *pdata = NULL;
	void *buf = adsp_ke_buffer;
	u32 len = ADSP_KE_DUMP_LEN;
	u32 n = 0;

	memset(buf, 0, len);
	if (!adspsys) {
		pr_info("adsp image not load, skip dump");
		goto EXIT;
	}

	adsp_enable_clock();
	adsp_latch_dump_region(true);

	pdata = get_adsp_core_by_id(ADSP_A_ID);
	if (pdata) {
		n += copy_from_buffer(buf + n, len - n,
					adspsys->cfg, adspsys->cfg_size, 0, -1);
		n += copy_from_buffer(buf + n, len - n,
					adspsys->cfg2, adspsys->cfg2_size, 0, -1);
		n += copy_from_buffer(buf + n, len - n,
					pdata->dtcm, pdata->dtcm_size, 0, -1);
	}

	pdata = get_adsp_core_by_id(ADSP_B_ID);
	if (pdata) {
		n += copy_from_buffer(buf + n, len - n,
					pdata->dtcm, pdata->dtcm_size, 0, -1);
	}
	adsp_latch_dump_region(false);
	adsp_disable_clock();

	/* last adsp_log */
	//n += dump_adsp_partial_log(buf + n, len - n);
EXIT:
	/* return value */
	*vaddr = (unsigned long)buf;
	*size = len;
}
EXPORT_SYMBOL(get_adsp_aee_buffer);

/*
 * sysfs bin_attribute node
 */
static ssize_t adsp_dump_show(struct file *filep, struct kobject *kobj,
				struct bin_attribute *attr,
				char *buf, loff_t offset, size_t size)
{
	ssize_t n = 0;
	struct adsp_exception_control *ctrl = &excep_ctrl;

	if (ctrl->buf_backup) {
		n = copy_from_buffer(buf, -1, ctrl->buf_backup,
			ctrl->buf_size, offset, size);

		if (n == 0) {
			vfree(ctrl->buf_backup);
			ctrl->buf_backup = NULL;
			ctrl->buf_size = 0;

			/* if dump_buffer wait for dump, wake up it */
			complete(&ctrl->done);
		}
	}

	return n;
}

static ssize_t adsp_dump_ke_show(struct file *filep, struct kobject *kobj,
				struct bin_attribute *attr,
				char *buf, loff_t offset, size_t size)
{
	unsigned long tmp[2];
	ssize_t n = 0;
	ssize_t threshold[3];

	if (offset == 0) /* only do ke ramdump once at start */
		get_adsp_aee_buffer(&tmp[0], &tmp[1]);

	threshold[0] = ADSP_KE_DUMP_LEN;
	threshold[1] = threshold[0] +
		adsp_get_reserve_mem_size(ADSP_A_LOGGER_MEM_ID);
	threshold[2] = threshold[1] +
		adsp_get_reserve_mem_size(ADSP_B_LOGGER_MEM_ID);

	if (offset >= 0 && offset < threshold[0]) {
		n = copy_from_buffer(buf, -1, adsp_ke_buffer,
			threshold[0], offset, size);
	} else if (offset >= threshold[0] && offset < threshold[1]) {
		n = copy_from_adsp_shared_memory(
				buf, offset - threshold[0],
				size, ADSP_A_LOGGER_MEM_ID);
	} else if (offset >= threshold[1] && offset < threshold[2]) {
		n = copy_from_adsp_shared_memory(
				buf, offset - threshold[1],
				size, ADSP_B_LOGGER_MEM_ID);
	}

	return n;
}

static ssize_t adsp_dump_log_show(struct file *filep, struct kobject *kobj,
				struct bin_attribute *attr,
				char *buf, loff_t offset, size_t size)
{
	ssize_t n = 0;
	ssize_t threshold[2];

	threshold[0] = adsp_get_reserve_mem_size(ADSP_A_LOGGER_MEM_ID);
	threshold[1] = threshold[0] +
		adsp_get_reserve_mem_size(ADSP_B_LOGGER_MEM_ID);

	if (offset >= 0 && offset < threshold[0]) {
		n = copy_from_adsp_shared_memory(buf, offset,
				size, ADSP_A_LOGGER_MEM_ID);
	} else if (offset >= threshold[0] && offset < threshold[1]) {
		n = copy_from_adsp_shared_memory(buf, offset - threshold[0],
				size, ADSP_B_LOGGER_MEM_ID);
	}
	return n;
}

static struct bin_attribute bin_attr_adsp_dump = {
	.attr = {
		.name = "adsp_dump",
		.mode = 0444,
	},
	.size = 0,
	.read = adsp_dump_show,
};

static struct bin_attribute bin_attr_adsp_dump_ke = {
	.attr = {
		.name = "adsp_dump_ke",
		.mode = 0444,
	},
	.size = 0,
	.read = adsp_dump_ke_show,
};

struct bin_attribute bin_attr_adsp_dump_log = {
	.attr = {
		.name = "adsp_last_log",
		.mode = 0444,
	},
	.size = 0,
	.read = adsp_dump_log_show,
};

static inline ssize_t suppress_ee_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned int input = 0;

	if (kstrtouint(buf, 0, &input) != 0)
		return -EINVAL;

	suppress_test_ee = !!input;

	return count;
}
DEVICE_ATTR_WO(suppress_ee);

static struct bin_attribute *adsp_excep_bin_attrs[] = {
	&bin_attr_adsp_dump,
	&bin_attr_adsp_dump_ke,
	&bin_attr_adsp_dump_log,
	NULL,
};

static struct attribute *adsp_excep_attrs[] = {
	&dev_attr_suppress_ee.attr,
	NULL,
};

struct attribute_group adsp_excep_attr_group = {
	.attrs = adsp_excep_attrs,
	.bin_attrs = adsp_excep_bin_attrs,
};

