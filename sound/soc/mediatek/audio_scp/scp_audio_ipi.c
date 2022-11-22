// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/errno.h>
#include <linux/printk.h>
#include <linux/soc/mediatek/mtk-mbox.h>
#include "scp.h"
#include "audio_mbox.h"
#include "scp_audio_ipi.h"

#define SHARE_BUF_SIZE  128

static recv_queue_handler_t recv_queue_callback;

struct scp_audio_ipi_desc {
	scp_audio_ipi_handler_t handler;
	const char *name;
} ipi_descs[SCP_AUDIO_NR_IPI];

int scp_send_message(unsigned int id, void *buf, unsigned int len,
		     unsigned int wait, unsigned int cid)
{
	struct mtk_ipi_msg msg;

	if (!buf || (len > (SHARE_BUF_SIZE - sizeof(struct mtk_ipi_msg_hd)))) {
		pr_info("%s(), buffer error, len:%u", __func__, len);
		return SCP_IPI_ERROR;
	}

	msg.ipihd.id = id;
	msg.ipihd.len = len;
	msg.ipihd.options = 0xffff0000;
	msg.ipihd.reserved = 0xdeadbeef;
	msg.data = buf;

	return audio_mbox_send(&msg, wait);
}
EXPORT_SYMBOL_GPL(scp_send_message);

bool is_scp_audio_ready(void)
{
	return is_audio_mbox_init_done() && is_scp_ready(SCP_A_ID);
}
EXPORT_SYMBOL_GPL(is_scp_audio_ready);

int scp_audio_ipi_registration(unsigned int id,
			       scp_audio_ipi_handler_t handler,
			       const char *name)
{
	if (id >= SCP_AUDIO_NR_IPI || !handler)
		return SCP_IPI_ERROR;

	ipi_descs[id].name = name;
	ipi_descs[id].handler = handler;
	return SCP_IPI_DONE;
}
EXPORT_SYMBOL_GPL(scp_audio_ipi_registration);

/*
 * API let apps unregister an ipi handler
 * @param id:      IPI ID
 */
int scp_audio_ipi_unregistration(unsigned int id)
{
	if (id >= SCP_AUDIO_NR_IPI)
		return SCP_IPI_ERROR;

	ipi_descs[id].name = "";
	ipi_descs[id].handler = NULL;
	return SCP_IPI_DONE;
}
EXPORT_SYMBOL_GPL(scp_audio_ipi_unregistration);

void hook_scp_ipi_queue_recv_msg_handler(recv_queue_handler_t queue_handler)
{
	recv_queue_callback = queue_handler;
}
EXPORT_SYMBOL_GPL(hook_scp_ipi_queue_recv_msg_handler);

void unhook_scp_ipi_queue_recv_msg_handler(void)
{
	recv_queue_callback = NULL;
}
EXPORT_SYMBOL_GPL(unhook_scp_ipi_queue_recv_msg_handler);

int audio_mbox_pin_cb(unsigned int id, void *prdata, void *buf, unsigned int len)
{
	int ret = -EPIPE;

	if (id >= SCP_AUDIO_NR_IPI) {
		pr_err("%s() invalid ipi_id %d\n", __func__, id);
		return SCP_IPI_ERROR;
	}

	if (!ipi_descs[id].handler) {
		pr_err("%s() invalid ipi handler %d\n", __func__, id);
		return SCP_IPI_ERROR;
	}

	if (recv_queue_callback != NULL)
		ret = recv_queue_callback(0, id, buf, len, ipi_descs[id].handler);

	if (ret != 0) {
		pr_info("ipi recv_queue not ready(%d), directly handle ipi %d", ret, id);
		ipi_descs[id].handler(id, buf, len);
	}
	return SCP_IPI_DONE;
}

static void audio_ipi_test_recv(int id, void *data, unsigned int len)
{
	int *value = (int *)data;

	pr_info("%s, MBOX receive from scp, id %d, value %d, len %d",
		__func__, id, *value, len);
}

static inline ssize_t audio_ipi_test_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int value = 0;
	int ret = 0;

	if (kstrtoint(buf, 10, &value))
		return -EINVAL;

	scp_audio_ipi_registration(SCP_AUDIO_IPI_TEST1, audio_ipi_test_recv, "ipi_test");
	ret = scp_send_message(SCP_AUDIO_IPI_TEST1, &value, sizeof(value), 20, 0);
	pr_info("audio_ipi_test value:%d, ret:%d\n", value, ret);

	return count;
}
DEVICE_ATTR_WO(audio_ipi_test);

