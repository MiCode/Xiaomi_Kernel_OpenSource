// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/of.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/soc/mediatek/mtk-mbox.h>
#include "scp_ipi_pin.h"
#include "audio_mbox.h"

static u32 audio_mbox_pin_buf[AUDIO_MBOX_RECV_SLOT_SIZE];
static bool init_done;

struct mtk_mbox_info audio_mbox_table[AUDIO_TOTAL_MBOX] = {
	{ .opt = MBOX_OPT_QUEUE_DIR, .is64d = true},
};

static struct mtk_mbox_pin_send audio_mbox_pin_send[AUDIO_TOTAL_SEND_PIN] = {
	{
		.mbox = AUDIO_MBOX_CH_ID,
		.offset = AUDIO_MBOX_SEND_SLOT_OFFSET,
		.msg_size = AUDIO_MBOX_SEND_SLOT_SIZE,
		.pin_index = 0,
	},
};

static struct mtk_mbox_pin_recv audio_mbox_pin_recv[AUDIO_TOTAL_RECV_PIN] = {
	{
		.mbox = AUDIO_MBOX_CH_ID,
		.offset = AUDIO_MBOX_RECV_SLOT_OFFSET,
		.msg_size = AUDIO_MBOX_RECV_SLOT_SIZE,
		.pin_index = 0,
		.mbox_pin_cb = audio_mbox_pin_cb,
		.pin_buf = audio_mbox_pin_buf,
	},
};

struct mtk_mbox_device audio_mboxdev = {
	.name = "audio_mailbox",
	.pin_recv_table = audio_mbox_pin_recv,
	.pin_send_table = audio_mbox_pin_send,
	.info_table = audio_mbox_table,
	.count = AUDIO_TOTAL_MBOX,
	.recv_count = AUDIO_TOTAL_RECV_PIN,
	.send_count = AUDIO_TOTAL_SEND_PIN,
	.post_cb = (mbox_rx_cb_t)scp_clr_spm_reg,
};

static struct miscdevice mdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "scp_audio_ipi",
};

bool is_audio_mbox_init_done(void)
{
	return init_done;
}
EXPORT_SYMBOL_GPL(is_audio_mbox_init_done);

int audio_mbox_send(void *msg, unsigned int wait)
{
	int ret;
	struct mtk_mbox_device *mbdev = &audio_mboxdev;
	struct mtk_mbox_pin_send *pin_send = &audio_mbox_pin_send[0];
	ktime_t ts;

	if (!init_done) {
		pr_info_ratelimited("%s, not implemented", __func__);
		return -1;
	}

	if (mutex_trylock(&pin_send->mutex_send) == 0) {
		pr_info("%s, mbox %d mutex_trylock busy", __func__, pin_send->mbox);
		return MBOX_PIN_BUSY;
	}

	/* TODO : maybe move to audio_ipi_queue */
	scp_awake_lock((void *)SCP_A_ID);

	if (mtk_mbox_check_send_irq(mbdev, pin_send->mbox, pin_send->pin_index)) {
		ret = MBOX_PIN_BUSY;
		goto EXIT;
	}

	ret = mtk_mbox_write_hd(mbdev, pin_send->mbox, pin_send->offset, msg);
	if (ret)
		goto EXIT;

	dsb(SY);

	ret = mtk_mbox_trigger_irq(mbdev, pin_send->mbox, 0x1 << pin_send->pin_index);
	if (!wait || ret)
		goto EXIT;

	ts = ktime_get();
	while (mtk_mbox_check_send_irq(mbdev, pin_send->mbox, pin_send->pin_index)) {
		if (ktime_us_delta(ktime_get(), ts) > 1000LL) {/* 1 ms */
			pr_warn("%s, time_ipc_us > 1000", __func__);
			break;
		}
	}
EXIT:
	if (ret && ret != MBOX_PIN_BUSY)
		pr_err("%s() fail, mbox error = %d\n", __func__, pin_send->mbox, ret);

	/* TODO : maybe move to audio_ipi_queue */
	scp_awake_unlock((void *)SCP_A_ID);

	mutex_unlock(&pin_send->mutex_send);
	return ret;
}

static int scp_audio_mbox_dev_probe(struct platform_device *pdev)
{
	int ret = -1;
	int mbox = AUDIO_MBOX_CH_ID;
	struct mtk_mbox_device *mbdev = &audio_mboxdev;

	ret = mtk_mbox_probe(pdev, mbdev, mbox);
	if (ret) {
		pr_warn("%s, mtk_mbox_probe mboxdev fail ret(%d)", __func__, ret);
		goto EXIT;
	}

	enable_irq_wake(mbdev->info_table[mbox].irq_num);
	mutex_init(&audio_mbox_pin_send[0].mutex_send);

	ret = misc_register(&mdev);
	if (ret)
		pr_info("%s, cannot register misc device\n", __func__);

	ret = device_create_file(mdev.this_device, &dev_attr_audio_ipi_test);
	if (ret)
		pr_info("%s, cannot create dev_attr_audio_ipi_test\n", __func__);

	init_done = true;
EXIT:
	pr_info("%s, done ret:%d", __func__, ret);
	return ret;
}

static const struct of_device_id scp_audio_mbox_dt_match[] = {
	{ .compatible = "mediatek,scp_audio_mbox", },
	{},
};
MODULE_DEVICE_TABLE(of, scp_audio_mbox_dt_match);

static struct platform_driver scp_audio_mbox_driver = {
	.driver = {
		   .name = "scp_audio_mbox",
		   .owner = THIS_MODULE,
		   .of_match_table = scp_audio_mbox_dt_match,
	},
	.probe = scp_audio_mbox_dev_probe,
};

module_platform_driver(scp_audio_mbox_driver);

MODULE_DESCRIPTION("Mediatek common driver for scp audio mbox");
MODULE_AUTHOR("Chien-wei Hsu <chien-Wei.Hsu@mediatek.com>");
MODULE_LICENSE("GPL v2");

