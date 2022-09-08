// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/iio/iio.h>
#include <linux/module.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include "main.h"
#include "bl.h"
#include "comms.h"
#include "nanohub-mtk.h"
#include "SCP_power_monitor.h"

#define CHRE_IPI_DEBUG	0
struct nanohub_ipi_rx_st {
	u8 *buff;
	int copy_size;
	struct completion isr_comp;
};

struct nanohub_ipi_rx_st nanohub_ipi_rx;

struct iio_dev *nanohub_iio_dev;
struct semaphore scp_nano_ipi_sem;

struct nanohub_ipi_data {
	struct nanohub_data data;
	/* todo */
};

/* scp_nano_ipi_status: 1 :ready to ipi  0:not ready*/
int scp_nano_ipi_status;

#define NANOHUB_IPI_SEND_RETRY 100
void mtk_ipi_scp_isr_sim(int got_size)
{
	int token = got_size;
	int ret;
	int retry = NANOHUB_IPI_SEND_RETRY;
	/* add retry to avoid SCP busy timeout */
	while (retry-- && (READ_ONCE(scp_nano_ipi_status) == 1)) {
		ret = scp_ipi_send(IPI_CHREX, &token, sizeof(token),
				   0, SCP_A_ID);
		if (ret != SCP_IPI_BUSY)
			break;
		usleep_range(100, 200);
	}
}

static void nano_ipi_start(void)
{
	pr_info("%s notify\n", __func__);
	WRITE_ONCE(scp_nano_ipi_status, 1);
}

static void nano_ipi_stop(void)
{
	pr_info("%s notify\n", __func__);
	WRITE_ONCE(scp_nano_ipi_status, 0);
}

static int nano_ipi_event(u8 event, void *ptr)
{
	switch (event) {
	case SENSOR_POWER_UP:
		nano_ipi_start();
		break;
	case SENSOR_POWER_DOWN:
		nano_ipi_stop();
		break;
	}
	return 0;
}

static struct scp_power_monitor nano_ipi_notifier = {
	.name = "nanohub_ipi",
	.notifier_call = nano_ipi_event,
};

int nanohub_ipi_write(void *data, u8 *tx, int length, int timeout)
{
	int ret;
	int retry = NANOHUB_IPI_SEND_RETRY;
#if CHRE_IPI_DEBUG
	int i;

	pr_info("AP->(%d) ", length);
	for (i = 0; i < length; i++)
		pr_info("%02x ", tx[i]);
	pr_info("\n");
#endif
	ret = SCP_IPI_ERROR;
	while (retry-- && (READ_ONCE(scp_nano_ipi_status) == 1)) {
		ret = scp_ipi_send(IPI_CHRE, tx, length, 0, SCP_A_ID);
		if (ret != SCP_IPI_BUSY)
			break;
		usleep_range(100, 200);
	}

	if (ret == SCP_IPI_BUSY)
		pr_info("%s ipi busy, ret=%d\n", __func__, ret);

	if (ret == SCP_IPI_DONE)
		return length;
	else
		return ERROR_NACK;
}

int nanohub_ipi_read(void *data, u8 *rx, int max_length, int timeout)
{
	int ret;
	const int min_size = sizeof(struct nanohub_packet) +
		 sizeof(struct nanohub_packet_crc);

	if (max_length < min_size)
		return -1;
	/* todo: support interruptible? please check it! */
	if (wait_for_completion_interruptible_timeout(&nanohub_ipi_rx.isr_comp,
						      timeout) == 0) {
		ret = 0;	/* return as empty packet */
	} else {
		ret = nanohub_ipi_rx.copy_size;
		memcpy(rx, g_nanohub_data_p->comms.rx_buffer, ret);
		/* send back isr sim */
		mtk_ipi_scp_isr_sim(ret);
	}
#if CHRE_IPI_DEBUG
	pr_info("%s ret %d\n", __func__, ret);
#endif
	return ret;	/* return packet size */
}

static int nanohub_ipi_open(void *data)
{
	down(&scp_nano_ipi_sem);
	reinit_completion(&nanohub_ipi_rx.isr_comp); /*reset when retry start*/
	return 0;
}

static void nanohub_ipi_close(void *data)
{
	up(&scp_nano_ipi_sem);
}

void nanohub_ipi_comms_init(struct nanohub_ipi_data *ipi_data)
{
	struct nanohub_comms *comms = &ipi_data->data.comms;

	comms->seq = 1;
	comms->timeout_write = msecs_to_jiffies(512);
	comms->timeout_ack = msecs_to_jiffies(3);
	comms->timeout_reply = msecs_to_jiffies(3);
	comms->open = nanohub_ipi_open;
	comms->close = nanohub_ipi_close;
	comms->write = nanohub_ipi_write;
	comms->read = nanohub_ipi_read;
/*	comms->tx_buffer = kmalloc(4096, GFP_KERNEL | GFP_DMA); */
	comms->rx_buffer = kmalloc(4096, GFP_KERNEL | GFP_DMA);
	nanohub_ipi_rx.buff = comms->rx_buffer;
	sema_init(&scp_nano_ipi_sem, 1);
}

static int nanohub_ipi_remove(struct platform_device *pdev);

struct platform_device nanohub_ipi_pdev = {
	.name = "nanohub_ipi",
	.id = -1,
};

int nanohub_ipi_suspend(struct platform_device *dev, pm_message_t state)
{
	int ret = 0;

	if (READ_ONCE(scp_nano_ipi_status) == 1)
		ret = nanohub_suspend(nanohub_iio_dev);
	else
		ret = 0;

	return ret;
}

int nanohub_ipi_resume(struct platform_device *dev)
{
	int ret = 0;

	if (READ_ONCE(scp_nano_ipi_status) == 1)
		ret = nanohub_resume(nanohub_iio_dev);
	else
		ret = 0;

	return ret;
}

void scp_to_ap_ipi_handler(int id, void *data, unsigned int len)
{
#if CHRE_IPI_DEBUG
	int i;
	unsigned char *data_p = data;

	pr_info("->AP(%d):", len);
	for (i = 0; i < len; i++)
		pr_info("%02x ", data_p[i]);
	pr_info("\n");
#endif
	nanohub_ipi_rx.copy_size = len;
	memcpy(g_nanohub_data_p->comms.rx_buffer, data, len);
	/*todo: check size ? */
	complete(&nanohub_ipi_rx.isr_comp);
}

int nanohub_ipi_probe(struct platform_device *pdev)
{
	struct nanohub_ipi_data *ipi_data;
	struct iio_dev *iio_dev;
	enum scp_ipi_status status;

	iio_dev = iio_device_alloc(&nanohub_ipi_pdev.dev, sizeof(struct nanohub_ipi_data));
	if (!iio_dev)
		return -ENOMEM;
	nanohub_iio_dev = iio_dev;
	/*iio_dev = nanohub_probe(&pdev->dev, iio_dev);*/
	nanohub_probe(&nanohub_ipi_pdev.dev, iio_dev);
	ipi_data = iio_priv(iio_dev);

	nanohub_ipi_comms_init(ipi_data);
	init_completion(&nanohub_ipi_rx.isr_comp);
	status = scp_ipi_registration(IPI_CHRE,
				      scp_to_ap_ipi_handler, "chre_ap_rx");
	/*init nano scp ipi status*/
	WRITE_ONCE(scp_nano_ipi_status, 1);
	scp_power_monitor_register(&nano_ipi_notifier);

	return 0;
}

static int nanohub_ipi_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver nanohub_ipi_pdrv = {
	.probe = nanohub_ipi_probe,
	.remove = nanohub_ipi_remove,
	.suspend = nanohub_ipi_suspend,
	.resume = nanohub_ipi_resume,
	.driver = {
		.name = "nanohub_ipi",
		.owner = THIS_MODULE,
		/*.of_match_table = scpdvfs_of_ids,*/
	},
};

int __init nanohub_ipi_init(void)
{
	int ret = 0;

	ret = platform_device_register(&nanohub_ipi_pdev);
	if (ret) {
		pr_debug("nanohub_ipi_pdev fail\n");
		goto _nanohub_ipi_init_exit;
	}

	ret = platform_driver_register(&nanohub_ipi_pdrv);
	if (ret) {
		pr_debug("nanohub_ipi_pdrv fail\n");
		platform_device_unregister(&nanohub_ipi_pdev);
		goto _nanohub_ipi_init_exit;
	}
	platform_set_drvdata(&nanohub_ipi_pdev, g_nanohub_data_p);

_nanohub_ipi_init_exit:
	return ret;
}
