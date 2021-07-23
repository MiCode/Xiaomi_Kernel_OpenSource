/* SPDX-License-Identifier: GPL-2.0 */
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
#include <linux/pm_wakeup.h>
#include <mt-plat/aee.h>
#include "adsp_reg.h"
#include "adsp_core.h"
#include "adsp_clk.h"
#include "adsp_platform_driver.h"
#include "adsp_excep.h"
#include "adsp_logger.h"

#define ADSP_MISC_EXTRA_SIZE    0x400 //1KB
#define ADSP_MISC_BUF_SIZE      0x10000 //64KB

static char adsp_ke_buffer[ADSP_KE_DUMP_LEN];
static struct adsp_exception_control excep_ctrl;

static u32 copy_from_buffer(void *dest, size_t destsize, const void *src,
			    size_t srcsize, u32 offset, size_t request)
{
	/* if request == -1, offset == 0, copy full srcsize */
	if (offset + request > srcsize)
		request = srcsize - offset;

	/* if destsize == -1, don't check the request size */
	if (!dest || destsize < request) {
		pr_warn("%s, buffer null or not enough space", __func__);
		return 0;
	}

	memcpy(dest, src + offset, request);

	return request;
}

static inline u32 dump_adsp_shared_memory(void *buf, size_t size, int id)
{
	void *mem_addr = adsp_get_reserve_mem_virt(id);
	size_t mem_size = adsp_get_reserve_mem_size(id);

	if (!mem_addr)
		return 0;

	return copy_from_buffer(buf, size, mem_addr, mem_size, 0, -1);
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

static u32 dump_adsp_internal_mem(struct adsp_priv *pdata,
				  void *buf, size_t size)
{
	u32 clk_cfg = 0, uart_cfg = 0, n = 0;
	u32 clk_mask = ADSP_CLK_UART_EN | ADSP_CLK_CORE_0_EN |
		       ADSP_CLK_CORE_1_EN;
	u32 uart_mask = ADSP_UART_RST_N | ADSP_UART_BCLK_CG;

	adsp_enable_clock();
	mutex_lock(&excep_ctrl.lock);

	clk_cfg = switch_adsp_clk_ctrl_cg(true, clk_mask);
	uart_cfg = switch_adsp_uart_ctrl_cg(true, uart_mask);

	n += copy_from_buffer(buf + n, size - n,
				pdata->cfg, pdata->cfg_size, 0, -1);
	n += copy_from_buffer(buf + n, size - n,
				pdata->itcm, pdata->itcm_size, 0, -1);
	n += copy_from_buffer(buf + n, size - n,
				pdata->dtcm, pdata->dtcm_size, 0, -1);

	switch_adsp_clk_ctrl_cg(false, (~clk_cfg) & clk_mask);
	switch_adsp_uart_ctrl_cg(false, (~uart_cfg) & uart_mask);

	mutex_unlock(&excep_ctrl.lock);
	adsp_disable_clock();
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

	total = pdata->cfg_size
		+ pdata->itcm_size
		+ pdata->dtcm_size
		+ pdata->sysram_size
		+ adsp_get_reserve_mem_size(coredump_id)
		+ adsp_get_reserve_mem_size(ADSP_A_LOGGER_MEM_ID)
		+ adsp_get_reserve_mem_size(ADSP_B_LOGGER_MEM_ID);

	buf = vzalloc(total);
	if (!buf)
		return -ENOMEM;

	n += dump_adsp_internal_mem(pdata, buf + n, total - n);
	n += copy_from_buffer(buf + n, total - n,
			pdata->sysram, pdata->sysram_size, 0, -1);
	n += dump_adsp_shared_memory(buf + n, total - n, coredump_id);
	n += dump_adsp_shared_memory(buf + n, total - n, ADSP_A_LOGGER_MEM_ID);
	n += dump_adsp_shared_memory(buf + n, total - n, ADSP_B_LOGGER_MEM_ID);

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

	if (dump_flag) {
		ret = dump_buffer(ctrl, coredump_id);
		if (ret < 0)
			pr_info("%s, excep dump fail ret(%d)", __func__, ret);
	}
	coredump = adsp_get_reserve_mem_virt(coredump_id);
	coredump_size = adsp_get_reserve_mem_size(coredump_id);

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

void adsp_aed_worker(struct work_struct *ws)
{
	struct adsp_exception_control *ctrl = container_of(ws,
						struct adsp_exception_control,
						aed_work);
	struct adsp_priv *pdata = NULL;
	int cid = 0, ret = 0, retry = 0;

	/* wake lock AP*/
	__pm_stay_awake(&ctrl->wakeup_lock);

	/* stop adsp, set reset state */
	for (cid = 0; cid < ADSP_CORE_TOTAL; cid++) {
		pdata = get_adsp_core_by_id(cid);
		set_adsp_state(pdata, ADSP_RESET);
		complete_all(&pdata->done);
	}

	/* force wake up if suspend thread wait reset event */
	if (ctrl->waitq)
		wake_up(ctrl->waitq);

	adsp_register_feature(SYSTEM_FEATURE_ID);
	adsp_extern_notify_chain(ADSP_EVENT_STOP);

	/* exception dump */
	adsp_exception_dump(ctrl);

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
	adsp_disable_clock();

	if (ret) {
		pr_info("%s, adsp dead, wait dump dead body", __func__);
		aee_kernel_exception_api(__FILE__,
					 __LINE__,
					 DB_OPT_DEFAULT,
					 "[ADSP]",
					 "ASSERT: ADSP DEAD! Recovery Fail");

		/* BUG_ON(1); */
	}

	adsp_extern_notify_chain(ADSP_EVENT_READY);
	adsp_deregister_feature(SYSTEM_FEATURE_ID);

	__pm_relax(&ctrl->wakeup_lock);
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

static void adsp_wdt_counter_reset(unsigned long data)
{
	excep_ctrl.wdt_counter = 0;
	pr_info("[ADSP] %s\n", __func__);
}

/*
 * init a work struct
 */
int init_adsp_exception_control(struct workqueue_struct *workq,
				struct wait_queue_head *waitq)
{
	struct adsp_exception_control *ctrl = &excep_ctrl;

	ctrl->waitq = waitq;
	ctrl->workq = workq;
	ctrl->buf_backup = NULL;
	ctrl->buf_size = 0;
	mutex_init(&ctrl->lock);
	init_completion(&ctrl->done);
	INIT_WORK(&ctrl->aed_work, adsp_aed_worker);
	wakeup_source_init(&ctrl->wakeup_lock, "adsp wakelock");
	setup_timer(&ctrl->wdt_timer, adsp_wdt_counter_reset, 0);

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
	void *buf = adsp_ke_buffer;
	void *addr = NULL;
	u32 len =  ADSP_MISC_BUF_SIZE;

	unsigned int w_pos, r_pos;
	unsigned int data_len[2];
	struct adsp_priv *pdata = NULL;
	struct log_ctrl_s *ctrl;
	struct buffer_info_s *buf_info;
	u32 id;
	u32 n = 0, part_len = len / ADSP_CORE_TOTAL;

	memset(buf, 0, ADSP_KE_DUMP_LEN);

	for (id = 0; id < ADSP_CORE_TOTAL; id++) {
		w_pos = 0;
		pdata = get_adsp_core_by_id(id);
		if (!pdata)
			goto ERROR;

		ctrl = pdata->log_ctrl;
		addr = (void *)ctrl;
		if (!addr)
			goto ERROR;

		buf_info = (struct buffer_info_s *)(addr + ctrl->info_ofs);

		if (!ctrl->inited)
			goto ERROR;

		memcpy_fromio(&w_pos, &buf_info->w_pos, sizeof(w_pos));

		w_pos += ADSP_MISC_EXTRA_SIZE;
		if (w_pos >= ctrl->buff_size)
			w_pos -= ctrl->buff_size;
		if (w_pos < part_len) {
			r_pos = ctrl->buff_size + w_pos - part_len;
			data_len[0] = part_len - w_pos;
			data_len[1] = w_pos;
		} else {
			r_pos = w_pos - part_len;
			data_len[0] = part_len;
			data_len[1] = 0;
		}

		memcpy(buf + n, addr + r_pos, data_len[0]);
		n += data_len[0];
		memcpy(buf + n, addr, data_len[1]);
		n += data_len[1];
	}

	/* return value */
	*vaddr = (unsigned long)buf;
	*size = len;
	return;

ERROR:
	/* return value */
	*vaddr = (unsigned long)buf;
	*size = 0;
}
EXPORT_SYMBOL(get_adsp_misc_buffer);

void get_adsp_aee_buffer(unsigned long *vaddr, unsigned long *size)
{
	u32 clk_cfg = 0, uart_cfg = 0, n = 0;
	u32 clk_mask = ADSP_CLK_UART_EN | ADSP_CLK_CORE_0_EN |
		       ADSP_CLK_CORE_1_EN;
	u32 uart_mask = ADSP_UART_RST_N | ADSP_UART_BCLK_CG;
	struct adsp_priv *pdata = NULL;
	void *buf = adsp_ke_buffer;
	u32 len = ADSP_KE_DUMP_LEN;

	memset(buf, 0, len);
	adsp_enable_clock();
	mutex_lock(&excep_ctrl.lock);
	read_lock(&access_rwlock);

	adsp_mt_clr_sw_reset();

	clk_cfg = switch_adsp_clk_ctrl_cg(true, clk_mask);
	uart_cfg = switch_adsp_uart_ctrl_cg(true, uart_mask);

	pdata = get_adsp_core_by_id(ADSP_A_ID);

	n += copy_from_buffer(buf + n, len - n,
				pdata->cfg, pdata->cfg_size, 0, -1);
	n += copy_from_buffer(buf + n, len - n,
				pdata->dtcm, pdata->dtcm_size, 0, -1);

	pdata = get_adsp_core_by_id(ADSP_B_ID);

	n += copy_from_buffer(buf + n, len - n,
				pdata->dtcm, pdata->dtcm_size, 0, -1);

	switch_adsp_clk_ctrl_cg(false, (~clk_cfg) & clk_mask);
	switch_adsp_uart_ctrl_cg(false, (~uart_cfg) & uart_mask);

	read_unlock(&access_rwlock);
	mutex_unlock(&excep_ctrl.lock);
	adsp_disable_clock();

	/* last adsp_log */
	//n += dump_adsp_partial_log(buf + n, len - n);

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

static struct bin_attribute *adsp_excep_bin_attrs[] = {
	&bin_attr_adsp_dump,
	&bin_attr_adsp_dump_ke,
	&bin_attr_adsp_dump_log,
	NULL,
};

struct attribute_group adsp_excep_attr_group = {
	.bin_attrs = adsp_excep_bin_attrs,
};

