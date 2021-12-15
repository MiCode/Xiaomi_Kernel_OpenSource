/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/soc/mediatek/mtk-mbox.h>
#include <linux/vmalloc.h>      /* needed by vmalloc */
#include "adsp_core.h"
#include "adsp_platform.h"
#include "adsp_mbox.h"
#include "audio_ipi_platform.h"
#include "adsp_ipi_queue.h"

struct adsp_ipi_desc {
	void (*handler)(int id, void *data, unsigned int len);
	const char *name;
} adsp_ipi_descs[ADSP_NR_IPI];

static int adsp_mbox_pin_cb(unsigned int id, void *prdata, void *buf,
			    unsigned int len);

struct mtk_mbox_info adsp_mbox_table[ADSP_IPI_CH_CNT] = {
	{ .opt = MBOX_OPT_QUEUE_DIR, .is64d = true},
	{ .opt = MBOX_OPT_QUEUE_DIR, .is64d = true},
	{ .opt = MBOX_OPT_QUEUE_DIR, .is64d = true},
	{ .opt = MBOX_OPT_QUEUE_DIR, .is64d = true},
};

struct mtk_mbox_pin_send adsp_mbox_pin_send[ADSP_TOTAL_SEND_PIN] = {
	{
		.mbox = ADSP_MBOX0_CH_ID,
		.offset = ADSP_MBOX_SEND_SLOT_OFFSET,
		.msg_size = ADSP_MBOX_SLOT_COUNT,
		.pin_index = 0,
	},
	{
		.mbox = ADSP_MBOX2_CH_ID,
		.offset = ADSP_MBOX_SEND_SLOT_OFFSET,
		.msg_size = ADSP_MBOX_SLOT_COUNT,
		.pin_index = 0,
	}
};

struct mtk_mbox_pin_recv adsp_mbox_pin_recv[ADSP_TOTAL_RECV_PIN] = {
	{
		.mbox = ADSP_MBOX1_CH_ID,
		.offset = ADSP_MBOX_RECV_SLOT_OFFSET,
		.msg_size = ADSP_MBOX_SLOT_COUNT,
		.pin_index = 0,
		.mbox_pin_cb = adsp_mbox_pin_cb,
	},
	{
		.mbox = ADSP_MBOX3_CH_ID,
		.offset = ADSP_MBOX_RECV_SLOT_OFFSET,
		.msg_size = ADSP_MBOX_SLOT_COUNT,
		.pin_index = 0,
		.mbox_pin_cb = adsp_mbox_pin_cb,
	}
};

struct mtk_mbox_device adsp_mboxdev = {
	.name = "adsp_mailbox",
	.pin_recv_table = adsp_mbox_pin_recv,
	.pin_send_table = adsp_mbox_pin_send,
	.info_table = adsp_mbox_table,
	.count = ADSP_IPI_CH_CNT,
	.recv_count = ADSP_TOTAL_RECV_PIN,
	.send_count = ADSP_TOTAL_SEND_PIN,
	/*.post_cb = adsp_post_cb, */
};

int adsp_mbox_send(struct mtk_mbox_pin_send *pin_send, void *msg,
		unsigned int wait)
{
	int result;
	struct mtk_mbox_device *mbdev = &adsp_mboxdev;
	ktime_t start_time;
	s64 time_ipc_us;

	if (mutex_trylock(&pin_send->mutex_send) == 0) {
		pr_info("%s, mbox %d mutex_trylock busy",
			__func__, pin_send->mbox);
		return MBOX_PIN_BUSY;
	}

	if (mtk_mbox_check_send_irq(mbdev, pin_send->mbox,
				pin_send->pin_index)) {
		mutex_unlock(&pin_send->mutex_send);
		return MBOX_PIN_BUSY;
	}

	result = mtk_mbox_write_hd(mbdev, pin_send->mbox,
				pin_send->offset, msg);

	if (result != MBOX_DONE) {
		pr_err("%s() error mbox%d write, result %d\n",
		       __func__, pin_send->mbox, result);
		return result;
	}

	dsb(SY);

	result = mtk_mbox_trigger_irq(mbdev, pin_send->mbox,
				      0x1 << pin_send->pin_index);
	if (result != MBOX_DONE) {
		pr_err("%s() error mbox%d trigger, result %d\n",
		       __func__, pin_send->mbox, result);
		return result;
	}

	if (wait) {
		start_time = ktime_get();
		while (mtk_mbox_check_send_irq(mbdev, pin_send->mbox,
						pin_send->pin_index)) {
			time_ipc_us = ktime_us_delta(ktime_get(), start_time);
			if (time_ipc_us > 1000) {/* 1 ms */
				pr_warn("%s, time_ipc_us > 1000", __func__);
				break;
			}
		}
	}
	mutex_unlock(&pin_send->mutex_send);

	return MBOX_DONE;
}

static int adsp_mbox_pin_cb(unsigned int id, void *prdata, void *buf,
			    unsigned int len)
{
	u32 opendsp_id = *(u32 *)prdata ?
		AUDIO_OPENDSP_USE_HIFI3_B : AUDIO_OPENDSP_USE_HIFI3_A;

	adsp_mt_clr_spm(*(u32 *)prdata);

	if (id >= ADSP_NR_IPI) {
		pr_err("%s() invalid ipi_id %d\n", __func__, id);
		return ADSP_IPI_ERROR;
	}

	if (!adsp_ipi_descs[id].handler) {
		pr_err("%s() invalid ipi handler %d\n", __func__, id);
		return ADSP_IPI_ERROR;
	}

	if (id == ADSP_IPI_ADSP_A_READY ||
	    id == ADSP_IPI_DVFS_SUSPEND ||
	    id == ADSP_IPI_LOGGER_INIT)
		adsp_ipi_descs[id].handler(id, buf, len);
	else if (is_scp_ipi_queue_init(opendsp_id))
		scp_dispatch_ipi_hanlder_to_queue(
			opendsp_id, id, buf, len, adsp_ipi_descs[id].handler);
	else {
		pr_info("ipi queue not ready!! core: %u, ipi_id: %u, buf: %p, len: %u, ipi_handler: %p",
			opendsp_id, id, buf, len, adsp_ipi_descs[id].handler);
		WARN_ON(1);
	}

	return ADSP_IPI_DONE;
}

int adsp_mbox_probe(struct platform_device *pdev)
{
	int ret;
	int idx = 0;
	struct mtk_mbox_device *mbdev = &adsp_mboxdev;

	for (idx = 0; idx < ADSP_IPI_CH_CNT; idx++) {
		ret = mtk_mbox_probe(pdev, mbdev, idx);

		if (ret)
			break;
	}

	for (idx = 0; idx < mbdev->send_count; idx++)
		mutex_init(&mbdev->pin_send_table[idx].mutex_send);

	return ret;
}

struct mtk_mbox_pin_send *get_adsp_mbox_pin_send(int index)
{
	int i;
	struct mtk_mbox_info *mbox;
	struct mtk_mbox_pin_send *pin_send;

	if (index >= ADSP_IPI_CH_CNT)
		return ERR_PTR(-EINVAL);

	mbox = adsp_mboxdev.info_table + index;

	if (!mbox->enable) {
		pr_info("the channel %d not enable", index);
		return ERR_PTR(-ENODEV);
	}

	for (i = 0; i < ADSP_TOTAL_SEND_PIN; i++) {
		pin_send = &adsp_mbox_pin_send[i];
		if (pin_send->mbox == index)
			return pin_send;
	}

	pr_info("the channel %d is not send pin", index);
	return ERR_PTR(-ENODEV);
}

struct mtk_mbox_pin_recv *get_adsp_mbox_pin_recv(int index)
{
	int i;
	struct mtk_mbox_info *mbox;
	struct mtk_mbox_pin_recv *pin_recv;

	if (index >= ADSP_IPI_CH_CNT)
		return ERR_PTR(-EINVAL);

	mbox = adsp_mboxdev.info_table + index;

	if (!mbox->enable) {
		pr_info("the channel %d not enable", index);
		return ERR_PTR(-ENODEV);
	}

	for (i = 0; i < ADSP_TOTAL_RECV_PIN; i++) {
		pin_recv = &adsp_mbox_pin_recv[i];
		if (pin_recv->mbox == index)
			return pin_recv;
	}

	pr_info("the channel %d is not recv pin", index);
	return ERR_PTR(-ENODEV);
}

enum adsp_ipi_status adsp_ipi_registration(
	enum adsp_ipi_id id,
	void (*ipi_handler)(int id, void *data, unsigned int len),
	const char *name)
{
	if (id < ADSP_NR_IPI && id >= 0) {
		adsp_ipi_descs[id].name = name;

		if (ipi_handler == NULL)
			return ADSP_IPI_ERROR;

		adsp_ipi_descs[id].handler = ipi_handler;
		return ADSP_IPI_DONE;
	} else
		return ADSP_IPI_ERROR;
}
EXPORT_SYMBOL_GPL(adsp_ipi_registration);

/*
 * API let apps unregister an ipi handler
 * @param id:      IPI ID
 */
enum adsp_ipi_status adsp_ipi_unregistration(enum adsp_ipi_id id)
{
	if (id < ADSP_NR_IPI && id >= 0) {
		adsp_ipi_descs[id].name = "";
		adsp_ipi_descs[id].handler = NULL;
		return ADSP_IPI_DONE;
	} else
		return ADSP_IPI_ERROR;
}
EXPORT_SYMBOL_GPL(adsp_ipi_unregistration);

