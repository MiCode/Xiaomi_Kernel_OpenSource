/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/device.h>
#include <linux/io.h>
#include "adsp_clk.h"
#include "adsp_platform.h"
#include "adsp_platform_driver.h"
#include "audio_ipi_platform.h"
#include "adsp_ipi_queue.h"
#include "adsp_mbox.h"
#include "adsp_core.h"

static rwlock_t access_rwlock;

void adsp_A_register_notify(struct notifier_block *nb) {}
void adsp_A_unregister_notify(struct notifier_block *nb) {}
void reset_hal_feature_table(void) {}

struct adsp_priv *_get_adsp_core(void *ptr, int id)
{
	if (ptr)
		return container_of(ptr, struct adsp_priv, mdev);

	if (id < ADSP_CORE_TOTAL)
		return adsp_cores[id];

	return NULL;
}

void set_adsp_state(struct adsp_priv *pdata, int state)
{
	pdata->state = state;
}

int get_adsp_state(struct adsp_priv *pdata)
{
	return pdata->state;
}

int is_adsp_ready(int cid)
{
	if (cid >= ADSP_CORE_TOTAL)
		return 0;

	return (adsp_cores[cid]->state == ADSP_RUNNING);
}

bool is_adsp_system_running(void)
{
	unsigned int id;

	for (id = 0; id < ADSP_CORE_TOTAL; id++) {
		if (is_adsp_ready(id))
			return true;
	}
	return false;
}

int adsp_copy_to_sharedmem(struct adsp_priv *pdata, int id, void *src,
			   int count)
{
	void __iomem *dst = NULL;
	const struct sharedmem_info *item;

	if (id >= ADSP_SHAREDMEM_NUM)
		return 0;

	item = pdata->mapping_table + id;
	if (item->offset)
		dst = pdata->dtcm + pdata->dtcm_size - item->offset;

	if (!dst || !src)
		return 0;

	if (count > item->size)
		count = item->size;

	memcpy_toio(dst, src, count);

	return count;
}

int adsp_copy_from_sharedmem(struct adsp_priv *pdata, int id, void *dst,
			     int count)
{
	void __iomem *src;
	const struct sharedmem_info *item;

	if (id >= ADSP_SHAREDMEM_NUM)
		return 0;

	item = pdata->mapping_table + id;
	if (item->offset)
		dst = pdata->dtcm + pdata->dtcm_size - item->offset;

	if (!dst || !src)
		return 0;

	if (count > item->size)
		count = item->size;

	memcpy_fromio(dst, src, count);

	return count;
}

enum adsp_ipi_status adsp_push_message(enum adsp_ipi_id id, void *buf,
			unsigned int len, unsigned int wait, int core_id)
{
	int ret = 0;
	uint32_t wait_ms = (wait) ? ADSP_IPI_QUEUE_DEFAULT_WAIT_MS : 0;

	/* wait until IPC done */
	ret = scp_send_msg_to_queue(core_id + AUDIO_OPENDSP_USE_HIFI3_A,
				    id, buf, len, wait_ms);

	return (ret == 0) ? ADSP_IPI_DONE : ADSP_IPI_ERROR;
}

enum adsp_ipi_status adsp_send_message(enum adsp_ipi_id id, void *buf,
			unsigned int len, unsigned int wait, int core_id)
{
	struct adsp_priv *pdata = get_adsp_core_by_id(core_id);
	struct mtk_ipi_msg msg;

	if (get_adsp_state(pdata) != ADSP_RUNNING) {
		pr_notice("%s, adsp not enabled, id=%d", __func__, id);
		return ADSP_IPI_ERROR;
	}

	if (len > (SHARE_BUF_SIZE - 16) || buf == NULL) {
		pr_info("%s(), %s buffer error", __func__, "adsp");
		return ADSP_IPI_ERROR;
	}

	msg.ipihd.id = id;
	msg.ipihd.len = len;
	msg.ipihd.options = 0xffff0000;
	msg.ipihd.reserved = 0xdeadbeef;
	msg.data = buf;

	return adsp_mbox_send(pdata->send_mbox, &msg, wait);
}

void enable_adsp(bool en)
{
	if (en)
		adsp_enable_clock();
	else
		adsp_disable_clock();
}

void switch_adsp_power(bool on)
{
	unsigned long flags = 0;

	if (on) {
		adsp_enable_clock();
		adsp_set_clock_freq(CLK_DEFAULT_INIT_CK);

		write_lock_irqsave(&access_rwlock, flags);
		adsp_mt_sw_reset();
		write_unlock_irqrestore(&access_rwlock, flags);
	} else {
		adsp_set_clock_freq(CLK_DEFAULT_26M_CK);
		adsp_disable_clock();
	}
}

void adsp_ready_ipi_handler(int id, void *data, unsigned int len)
{
	unsigned int core_id = *(unsigned int *)data;
	struct adsp_priv *pdata = get_adsp_core_by_id(core_id);

	if (!pdata)
		return;

	if (get_adsp_state(pdata) != ADSP_RUNNING) {
		set_adsp_state(pdata, ADSP_RUNNING);
		complete(&pdata->done);
	}
}

static int __init adsp_init(void)
{
	int ret = 0;

	ret = create_adsp_drivers();

	rwlock_init(&access_rwlock);

	adsp_platform_init(adsp_cores[0]->cfg);

	//register_syscore_ops(&adsp_syscore_ops);
	adsp_ipi_registration(ADSP_IPI_ADSP_A_READY,
			      adsp_ready_ipi_handler,
			      "adsp_ready");

	pr_info("%s, ret(%d)\n", __func__, ret);

	return ret;
}

static int __init adsp_module_init(void)
{
	int ret = 0, cid = 0;

	struct adsp_priv *pdata;

	switch_adsp_power(true);

	for (cid = 0; cid < ADSP_CORE_TOTAL; cid++) {
		pdata = adsp_cores[cid];

		if (!pdata) {
			ret = -EFAULT;
			goto ERROR;
		}

		ret = pdata->ops->initialize(pdata);
		if (ret) {
			pr_warn("%s, initialize %d is fail\n", __func__, cid);
			goto ERROR;
		}

		adsp_mt_run(cid);

		ret = wait_for_completion_timeout(&pdata->done,
					    msecs_to_jiffies(1000));

		if (ret == 0) {
			pr_warn("%s, core %d boot_up timeout\n", __func__, cid);
			ret = -ETIME;
			goto ERROR;
		}
	}

	pr_info("[ADSP] module_init_done\n");
ERROR:
	return ret;
}

/*
 * driver exit point
 */
static void __exit adsp_exit(void)
{

}
subsys_initcall(adsp_init);
module_init(adsp_module_init);
module_exit(adsp_exit);

