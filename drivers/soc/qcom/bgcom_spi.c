/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(msg) "bgcom: %s: " msg, __func__

#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/ratelimit.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/bitops.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/kthread.h>
#include "bgcom.h"
#include "bgrsb.h"

#define BG_SPI_WORD_SIZE (0x04)
#define BG_SPI_READ_LEN (0x04)
#define BG_SPI_WRITE_CMND_LEN (0x01)
#define BG_SPI_FIFO_READ_CMD (0x41)
#define BG_SPI_FIFO_WRITE_CMD (0x40)
#define BG_SPI_AHB_READ_CMD (0x43)
#define BG_SPI_AHB_WRITE_CMD (0x42)
#define BG_SPI_AHB_CMD_LEN (0x05)
#define BG_SPI_AHB_READ_CMD_LEN (0x08)
#define BG_STATUS_REG (0x05)
#define BG_CMND_REG (0x14)

#define BG_SPI_MAX_WORDS (0x3FFFFFFD)
#define BG_SPI_MAX_REGS (0x0A)
#define SLEEP_IN_STATE_CHNG 2000
#define HED_EVENT_ID_LEN (0x02)
#define HED_EVENT_SIZE_LEN (0x02)
#define HED_EVENT_DATA_STRT_LEN (0x05)

#define MAX_RETRY 200

enum bgcom_state {
	/*BGCOM Staus ready*/
	BGCOM_PROB_SUCCESS = 0,
	BGCOM_PROB_WAIT = 1,
	BGCOM_STATE_SUSPEND = 2,
	BGCOM_STATE_ACTIVE = 3
};

enum bgcom_req_type {
	/*BGCOM local requests*/
	BGCOM_READ_REG = 0,
	BGCOM_READ_FIFO = 1,
	BGCOM_READ_AHB = 2,
};

struct bg_spi_priv {
	struct spi_device *spi;
	/* Transaction related */
	struct mutex xfer_mutex;
	void *lhandle;
	/* Message for single transfer */
	struct spi_message msg1;
	struct spi_transfer xfer1;
	int irq_lock;

	enum bgcom_state bg_state;
};

struct cb_data {
	void *priv;
	void *handle;
	void (*bgcom_notification_cb)(void *handle, void *priv,
		enum bgcom_event_type event,
		union bgcom_event_data_type *event_data);
	struct list_head list;
};

struct bg_context {
	struct bg_spi_priv *bg_spi;
	enum bgcom_state state;
	struct cb_data *cb;
};

struct event_list {
	struct event *evnt;
	struct list_head list;
};
static void *bg_com_drv;
static uint32_t g_slav_status_reg;

/* BGCOM client callbacks set-up */
static void send_input_events(struct work_struct *work);
static struct list_head cb_head = LIST_HEAD_INIT(cb_head);
static struct list_head pr_lst_hd = LIST_HEAD_INIT(pr_lst_hd);
static enum bgcom_spi_state spi_state;


static struct workqueue_struct *wq;
static DECLARE_WORK(input_work , send_input_events);

static struct mutex bg_resume_mutex;

static void augmnt_fifo(uint8_t *data, int pos)
{
	data[pos] = '\0';
}

static void send_input_events(struct work_struct *work)
{
	struct list_head *temp;
	struct list_head *pos;
	struct event_list *node;
	struct event *evnt;

	if (list_empty(&pr_lst_hd))
		return;

	list_for_each_safe(pos, temp, &pr_lst_hd) {
		node = list_entry(pos, struct event_list, list);
		evnt = node->evnt;
		bgrsb_send_input(evnt);
		kfree(evnt);
		list_del(&node->list);
		kfree(node);
	}
}

int bgcom_set_spi_state(enum bgcom_spi_state state)
{
	struct bg_spi_priv *bg_spi = container_of(bg_com_drv,
						struct bg_spi_priv, lhandle);
	if (state < 0 || state > 1)
		return -EINVAL;

	if (state == spi_state)
		return 0;

	mutex_lock(&bg_spi->xfer_mutex);
	spi_state = state;
	if (spi_state == BGCOM_SPI_BUSY)
		msleep(SLEEP_IN_STATE_CHNG);
	mutex_unlock(&bg_spi->xfer_mutex);
	return 0;
}
EXPORT_SYMBOL(bgcom_set_spi_state);

static inline
void add_to_irq_list(struct  cb_data *data)
{
	list_add_tail(&data->list, &cb_head);
}

static bool is_bgcom_ready(void)
{
	return (bg_com_drv != NULL ? true : false);
}

static void bg_spi_reinit_xfer(struct spi_transfer *xfer)
{
	xfer->tx_buf = NULL;
	xfer->rx_buf = NULL;
	xfer->delay_usecs = 0;
	xfer->len = 0;
}

static int read_bg_locl(enum bgcom_req_type req_type,
	uint32_t no_of_words, void *buf)
{

	struct bg_context clnt_handle;
	struct bg_spi_priv *spi =
			container_of(bg_com_drv, struct bg_spi_priv, lhandle);
	int ret = 0;

	if (!buf)
		return -EINVAL;

	clnt_handle.bg_spi = spi;

	switch (req_type) {
	case BGCOM_READ_REG:
		ret = bgcom_reg_read(&clnt_handle,
			BG_STATUS_REG, no_of_words, buf);
		break;
	case BGCOM_READ_FIFO:
		ret = bgcom_fifo_read(&clnt_handle, no_of_words, buf);
		break;
	case BGCOM_READ_AHB:
		break;
	}
	return ret;
}

static int bgcom_transfer(void *handle, uint8_t *tx_buf,
	uint8_t *rx_buf, uint32_t txn_len)
{
	struct spi_transfer *tx_xfer;
	struct bg_spi_priv *bg_spi;
	struct bg_context *cntx;
	struct spi_device *spi;
	int ret;

	if (!handle || !tx_buf)
		return -EINVAL;

	cntx = (struct bg_context *)handle;

	if (cntx->state == BGCOM_PROB_WAIT) {
		if (!is_bgcom_ready())
			return -ENODEV;
		cntx->bg_spi = container_of(bg_com_drv,
						struct bg_spi_priv, lhandle);
		cntx->state = BGCOM_PROB_SUCCESS;
	}
	bg_spi = cntx->bg_spi;

	if (!bg_spi)
		return -ENODEV;

	tx_xfer = &bg_spi->xfer1;
	spi = bg_spi->spi;

	mutex_lock(&bg_spi->xfer_mutex);
	bg_spi_reinit_xfer(tx_xfer);
	tx_xfer->tx_buf = tx_buf;
	if (rx_buf)
		tx_xfer->rx_buf = rx_buf;

	tx_xfer->len = txn_len;
	ret = spi_sync(spi, &bg_spi->msg1);
	mutex_unlock(&bg_spi->xfer_mutex);

	if (ret)
		pr_err("SPI transaction failed: %d\n", ret);
	return ret;
}

/* BG-COM Interrupt handling */
static inline
void send_event(enum bgcom_event_type event,
	void *data)
{
	struct list_head *pos;
	struct cb_data *cb;

	/* send interrupt notification for each
	registered call-back */
	list_for_each(pos, &cb_head) {
		cb = list_entry(pos, struct cb_data, list);
		cb->bgcom_notification_cb(cb->handle,
		cb->priv,  event, data);
	}
}

void bgcom_bgdown_handler(void)
{
	send_event(BGCOM_EVENT_RESET_OCCURRED, NULL);
	g_slav_status_reg = 0;
}
EXPORT_SYMBOL(bgcom_bgdown_handler);

static void parse_fifo(uint8_t *data, union bgcom_event_data_type *event_data)
{
	uint16_t p_len;
	uint8_t sub_id;
	uint32_t evnt_tm;
	uint16_t event_id;
	void *evnt_data;
	struct event *evnt;
	struct event_list *data_list;

	while (*data != '\0') {

		event_id = *((uint16_t *) data);
		data = data + HED_EVENT_ID_LEN;
		p_len = *((uint16_t *) data);
		data = data + HED_EVENT_SIZE_LEN;

		if (event_id == 0xFFFE) {

			sub_id = *data;
			evnt_tm = *((uint32_t *)(data+1));

			evnt = kmalloc(sizeof(*evnt), GFP_KERNEL);
			evnt->sub_id = sub_id;
			evnt->evnt_tm = evnt_tm;
			evnt->evnt_data =
				*(int16_t *)(data + HED_EVENT_DATA_STRT_LEN);

			data_list = kmalloc(sizeof(*data_list), GFP_KERNEL);
			data_list->evnt = evnt;
			list_add_tail(&data_list->list, &pr_lst_hd);

		} else if (event_id == 0x0001) {
			evnt_data = kmalloc(p_len, GFP_KERNEL);
			if (evnt_data != NULL) {
				memcpy(evnt_data, data, p_len);
				event_data->fifo_data.to_master_fifo_used =
						p_len/BG_SPI_WORD_SIZE;
				event_data->fifo_data.data = evnt_data;
				send_event(BGCOM_EVENT_TO_MASTER_FIFO_USED,
						event_data);
			}
		}
		data = data + p_len;
	}
	if (!list_empty(&pr_lst_hd))
		queue_work(wq, &input_work);
}

static void send_back_notification(uint32_t slav_status_reg,
	uint32_t slav_status_auto_clear_reg,
	uint32_t fifo_fill_reg, uint32_t fifo_size_reg)
{
	uint16_t master_fifo_used;
	uint16_t slave_fifo_free;
	uint32_t *ptr;
	int ret;
	union bgcom_event_data_type event_data = { .fifo_data = {0} };

	master_fifo_used = (uint16_t)fifo_fill_reg;
	slave_fifo_free = (uint16_t)(fifo_fill_reg >> 16);

	if (slav_status_auto_clear_reg & BIT(31))
		send_event(BGCOM_EVENT_RESET_OCCURRED, NULL);

	if (slav_status_auto_clear_reg & BIT(30))
		send_event(BGCOM_EVENT_ERROR_WRITE_FIFO_OVERRUN, NULL);

	if (slav_status_auto_clear_reg & BIT(29))
		send_event(BGCOM_EVENT_ERROR_WRITE_FIFO_BUS_ERR, NULL);

	if (slav_status_auto_clear_reg & BIT(28))
		send_event(BGCOM_EVENT_ERROR_WRITE_FIFO_ACCESS, NULL);

	if (slav_status_auto_clear_reg & BIT(27))
		send_event(BGCOM_EVENT_ERROR_READ_FIFO_UNDERRUN, NULL);

	if (slav_status_auto_clear_reg & BIT(26))
		send_event(BGCOM_EVENT_ERROR_READ_FIFO_BUS_ERR, NULL);

	if (slav_status_auto_clear_reg & BIT(25))
		send_event(BGCOM_EVENT_ERROR_READ_FIFO_ACCESS, NULL);

	if (slav_status_auto_clear_reg & BIT(24))
		send_event(BGCOM_EVENT_ERROR_TRUNCATED_READ, NULL);

	if (slav_status_auto_clear_reg & BIT(23))
		send_event(BGCOM_EVENT_ERROR_TRUNCATED_WRITE, NULL);

	if (slav_status_auto_clear_reg & BIT(22))
		send_event(BGCOM_EVENT_ERROR_AHB_ILLEGAL_ADDRESS, NULL);

	if (slav_status_auto_clear_reg & BIT(21))
		send_event(BGCOM_EVENT_ERROR_AHB_BUS_ERR, NULL);

	/* check if BG status is changed */
	if (g_slav_status_reg ^ slav_status_reg) {
		if (slav_status_reg & BIT(30)) {
			event_data.application_running = true;
			send_event(BGCOM_EVENT_APPLICATION_RUNNING,
				&event_data);
		}

		if (slav_status_reg & BIT(29)) {
			event_data.to_slave_fifo_ready = true;
			send_event(BGCOM_EVENT_TO_SLAVE_FIFO_READY,
				&event_data);
		}

		if (slav_status_reg & BIT(28)) {
			event_data.to_master_fifo_ready = true;
			send_event(BGCOM_EVENT_TO_MASTER_FIFO_READY,
				&event_data);
		}

		if (slav_status_reg & BIT(27)) {
			event_data.ahb_ready = true;
			send_event(BGCOM_EVENT_AHB_READY,
				&event_data);
		}
	}

	if (master_fifo_used > 0) {
		ptr = kzalloc(master_fifo_used*BG_SPI_WORD_SIZE + 1,
			GFP_KERNEL | GFP_ATOMIC);
		if (ptr != NULL) {
			ret = read_bg_locl(BGCOM_READ_FIFO,
				master_fifo_used,  ptr);
			if (!ret) {
				augmnt_fifo((uint8_t *)ptr,
					master_fifo_used*BG_SPI_WORD_SIZE);
				parse_fifo((uint8_t *)ptr, &event_data);
			}
			kfree(ptr);
		}
	}

	if (slave_fifo_free > 0) {
		event_data.to_slave_fifo_free = slave_fifo_free;
		send_event(BGCOM_EVENT_TO_SLAVE_FIFO_FREE, &event_data);
	}
}

static void bg_irq_tasklet_hndlr_l(void)
{
	uint32_t slave_status_reg;
	uint32_t glink_isr_reg;
	uint32_t slav_status_auto_clear_reg;
	uint32_t fifo_fill_reg;
	uint32_t fifo_size_reg;
	int ret =  0;
	uint32_t irq_buf[5] = {0};

	ret = read_bg_locl(BGCOM_READ_REG, 5, &irq_buf[0]);
	if (ret)
		return;

	/* save current state */
	slave_status_reg = irq_buf[0];
	glink_isr_reg = irq_buf[1];
	slav_status_auto_clear_reg = irq_buf[2];
	fifo_fill_reg = irq_buf[3];
	fifo_size_reg = irq_buf[4];

	send_back_notification(slave_status_reg,
		slav_status_auto_clear_reg, fifo_fill_reg, fifo_size_reg);

	g_slav_status_reg = slave_status_reg;
}

int bgcom_ahb_read(void *handle, uint32_t ahb_start_addr,
	uint32_t num_words, void *read_buf)
{
	uint32_t txn_len;
	uint8_t *tx_buf;
	uint8_t *rx_buf;
	uint32_t size;
	int ret;
	uint8_t cmnd = 0;
	uint32_t ahb_addr = 0;

	if (!handle || !read_buf || num_words == 0
		|| num_words > BG_SPI_MAX_WORDS) {
		pr_err("Invalid param\n");
		return -EINVAL;
	}
	if (!is_bgcom_ready())
		return -ENODEV;

	if (spi_state == BGCOM_SPI_BUSY) {
		pr_err("Device busy\n");
		return -EBUSY;
	}

	if (bgcom_resume(handle)) {
		pr_err("Failed to resume\n");
		return -EBUSY;
	}

	size = num_words*BG_SPI_WORD_SIZE;
	txn_len = BG_SPI_AHB_READ_CMD_LEN + size;

	tx_buf = kzalloc(txn_len, GFP_KERNEL);

	if (!tx_buf)
		return -ENOMEM;

	rx_buf = kzalloc(txn_len, GFP_KERNEL);

	if (!rx_buf) {
		kfree(tx_buf);
		return -ENOMEM;
	}

	cmnd |= BG_SPI_AHB_READ_CMD;
	ahb_addr |= ahb_start_addr;

	memcpy(tx_buf, &cmnd, sizeof(cmnd));
	memcpy(tx_buf+sizeof(cmnd), &ahb_addr, sizeof(ahb_addr));

	ret = bgcom_transfer(handle, tx_buf, rx_buf, txn_len);

	if (!ret)
		memcpy(read_buf, rx_buf+BG_SPI_AHB_READ_CMD_LEN, size);

	kfree(tx_buf);
	kfree(rx_buf);
	return ret;
}
EXPORT_SYMBOL(bgcom_ahb_read);

int bgcom_ahb_write(void *handle, uint32_t ahb_start_addr,
	uint32_t num_words, void *write_buf)
{
	uint32_t txn_len;
	uint8_t *tx_buf;
	uint32_t size;
	int ret;
	uint8_t cmnd = 0;
	uint32_t ahb_addr = 0;

	if (!handle || !write_buf || num_words == 0
		|| num_words > BG_SPI_MAX_WORDS) {
		pr_err("Invalid param\n");
		return -EINVAL;
	}

	if (!is_bgcom_ready())
		return -ENODEV;

	if (spi_state == BGCOM_SPI_BUSY) {
		pr_err("Device busy\n");
		return -EBUSY;
	}

	if (bgcom_resume(handle)) {
		pr_err("Failed to resume\n");
		return -EBUSY;
	}

	size = num_words*BG_SPI_WORD_SIZE;
	txn_len = BG_SPI_AHB_CMD_LEN + size;

	tx_buf = kzalloc(txn_len, GFP_KERNEL);

	if (!tx_buf)
		return -ENOMEM;

	cmnd |= BG_SPI_AHB_WRITE_CMD;
	ahb_addr |= ahb_start_addr;

	memcpy(tx_buf, &cmnd, sizeof(cmnd));
	memcpy(tx_buf+sizeof(cmnd), &ahb_addr, sizeof(ahb_addr));
	memcpy(tx_buf+BG_SPI_AHB_CMD_LEN, write_buf, size);

	ret = bgcom_transfer(handle, tx_buf, NULL, txn_len);
	kfree(tx_buf);
	return ret;
}
EXPORT_SYMBOL(bgcom_ahb_write);

int bgcom_fifo_write(void *handle, uint32_t num_words,
	void  *write_buf)
{
	uint32_t txn_len;
	uint8_t *tx_buf;
	uint32_t size;
	int ret;
	uint8_t cmnd = 0;

	if (!handle || !write_buf || num_words == 0
		|| num_words > BG_SPI_MAX_WORDS) {
		pr_err("Invalid param\n");
		return -EINVAL;
	}

	if (!is_bgcom_ready())
		return -ENODEV;

	if (spi_state == BGCOM_SPI_BUSY) {
		pr_err("Device busy\n");
		return -EBUSY;
	}

	if (bgcom_resume(handle)) {
		pr_err("Failed to resume\n");
		return -EBUSY;
	}

	size = num_words*BG_SPI_WORD_SIZE;
	txn_len = BG_SPI_WRITE_CMND_LEN + size;

	tx_buf = kzalloc(txn_len, GFP_KERNEL | GFP_ATOMIC);

	if (!tx_buf)
		return -ENOMEM;

	cmnd |= BG_SPI_FIFO_WRITE_CMD;
	memcpy(tx_buf, &cmnd, sizeof(cmnd));
	memcpy(tx_buf+sizeof(cmnd), write_buf, size);

	ret = bgcom_transfer(handle, tx_buf, NULL, txn_len);
	kfree(tx_buf);
	return ret;
}
EXPORT_SYMBOL(bgcom_fifo_write);

int bgcom_fifo_read(void *handle, uint32_t num_words,
	void *read_buf)
{
	uint32_t txn_len;
	uint8_t *tx_buf;
	uint8_t *rx_buf;
	uint32_t size;
	uint8_t cmnd = 0;
	int ret =  0;

	if (!handle || !read_buf || num_words == 0
		|| num_words > BG_SPI_MAX_WORDS) {
		pr_err("Invalid param\n");
		return -EINVAL;
	}

	if (!is_bgcom_ready())
		return -ENODEV;

	if (spi_state == BGCOM_SPI_BUSY) {
		pr_err("Device busy\n");
		return -EBUSY;
	}

	size = num_words*BG_SPI_WORD_SIZE;
	txn_len = BG_SPI_READ_LEN + size;
	tx_buf = kzalloc(txn_len, GFP_KERNEL | GFP_ATOMIC);

	if (!tx_buf)
		return -ENOMEM;

	rx_buf = kzalloc(txn_len, GFP_KERNEL | GFP_ATOMIC);

	if (!rx_buf) {
		kfree(tx_buf);
		return -ENOMEM;
	}

	cmnd |= BG_SPI_FIFO_READ_CMD;
	memcpy(tx_buf, &cmnd, sizeof(cmnd));

	ret = bgcom_transfer(handle, tx_buf, rx_buf, txn_len);

	if (!ret)
		memcpy(read_buf, rx_buf+BG_SPI_READ_LEN, size);
	kfree(tx_buf);
	kfree(rx_buf);
	return ret;
}
EXPORT_SYMBOL(bgcom_fifo_read);

int bgcom_reg_write(void *handle, uint8_t reg_start_addr,
	uint8_t num_regs, void *write_buf)
{
	uint32_t txn_len;
	uint8_t *tx_buf;
	uint32_t size;
	uint8_t cmnd = 0;
	int ret =  0;

	if (!handle || !write_buf || num_regs == 0
		|| num_regs > BG_SPI_MAX_REGS) {
		pr_err("Invalid param\n");
		return -EINVAL;
	}

	if (!is_bgcom_ready())
		return -ENODEV;

	if (spi_state == BGCOM_SPI_BUSY) {
		pr_err("Device busy\n");
		return -EBUSY;
	}

	size = num_regs*BG_SPI_WORD_SIZE;
	txn_len = BG_SPI_WRITE_CMND_LEN + size;

	tx_buf = kzalloc(txn_len, GFP_KERNEL);

	if (!tx_buf)
		return -ENOMEM;

	cmnd |= reg_start_addr;
	memcpy(tx_buf, &cmnd, sizeof(cmnd));
	memcpy(tx_buf+sizeof(cmnd), write_buf, size);

	ret = bgcom_transfer(handle, tx_buf, NULL, txn_len);
	kfree(tx_buf);
	return ret;
}
EXPORT_SYMBOL(bgcom_reg_write);

int bgcom_reg_read(void *handle, uint8_t reg_start_addr,
	uint32_t num_regs, void *read_buf)
{
	uint32_t txn_len;
	uint8_t *tx_buf;
	uint8_t *rx_buf;
	uint32_t size;
	int ret;
	uint8_t cmnd = 0;

	if (!handle || !read_buf || num_regs == 0
		|| num_regs > BG_SPI_MAX_REGS) {
		pr_err("Invalid param\n");
		return -EINVAL;
	}

	if (!is_bgcom_ready())
		return -ENODEV;

	if (spi_state == BGCOM_SPI_BUSY) {
		pr_err("Device busy\n");
		return -EBUSY;
	}

	size = num_regs*BG_SPI_WORD_SIZE;
	txn_len = BG_SPI_READ_LEN + size;

	tx_buf = kzalloc(txn_len, GFP_KERNEL | GFP_ATOMIC);

	if (!tx_buf)
		return -ENOMEM;

	rx_buf = kzalloc(txn_len, GFP_KERNEL | GFP_ATOMIC);

	if (!rx_buf) {
		kfree(tx_buf);
		return -ENOMEM;
	}

	cmnd |= reg_start_addr;
	memcpy(tx_buf, &cmnd, sizeof(cmnd));

	ret = bgcom_transfer(handle, tx_buf, rx_buf, txn_len);

	if (!ret)
		memcpy(read_buf, rx_buf+BG_SPI_READ_LEN, size);
	kfree(tx_buf);
	kfree(rx_buf);
	return ret;
}
EXPORT_SYMBOL(bgcom_reg_read);

static int is_bg_resume(void *handle)
{
	uint32_t txn_len;
	int ret;
	uint8_t tx_buf[8] = {0};
	uint8_t rx_buf[8] = {0};
	uint32_t cmnd_reg = 0;

	txn_len = 0x08;
	tx_buf[0] = 0x05;
	ret = bgcom_transfer(handle, tx_buf, rx_buf, txn_len);
	if (!ret)
		memcpy(&cmnd_reg, rx_buf+BG_SPI_READ_LEN, 0x04);
	return cmnd_reg & BIT(31);
}

int bgcom_resume(void *handle)
{
	struct bg_spi_priv *bg_spi;
	struct bg_context *cntx;
	int retry = 0;

	if (handle == NULL)
		return -EINVAL;

	cntx = (struct bg_context *)handle;
	bg_spi = cntx->bg_spi;

	mutex_lock(&bg_resume_mutex);
	if (bg_spi->bg_state == BGCOM_STATE_ACTIVE)
		goto unlock;
	do {
		if (is_bg_resume(handle)) {
			bg_spi->bg_state = BGCOM_STATE_ACTIVE;
			break;
		}
		udelay(10);
		++retry;
	} while (retry < MAX_RETRY);

unlock:
	mutex_unlock(&bg_resume_mutex);
	pr_info("BG retries for wake up : %d\n", retry);
	return (retry == MAX_RETRY ? -ETIMEDOUT : 0);
}
EXPORT_SYMBOL(bgcom_resume);

int bgcom_suspend(void *handle)
{
	struct bg_spi_priv *bg_spi;
	struct bg_context *cntx;
	uint32_t cmnd_reg = 0;
	int ret = 0;

	if (handle == NULL)
		return -EINVAL;

	cntx = (struct bg_context *)handle;
	bg_spi = cntx->bg_spi;
	mutex_lock(&bg_resume_mutex);
	if (bg_spi->bg_state == BGCOM_STATE_SUSPEND)
		goto unlock;

	cmnd_reg |= BIT(31);
	ret = bgcom_reg_write(handle, BG_CMND_REG, 1, &cmnd_reg);
	if (ret == 0)
		bg_spi->bg_state = BGCOM_STATE_SUSPEND;

unlock:
	mutex_unlock(&bg_resume_mutex);
	pr_info("suspended with : %d\n", ret);
	return ret;
}
EXPORT_SYMBOL(bgcom_suspend);

void *bgcom_open(struct bgcom_open_config_type *open_config)
{
	struct bg_spi_priv *spi;
	struct cb_data *irq_notification;
	struct bg_context  *clnt_handle =
			kzalloc(sizeof(*clnt_handle), GFP_KERNEL);

	if (!clnt_handle)
		return NULL;

	/* Client handle Set-up */
	if (!is_bgcom_ready()) {
		clnt_handle->bg_spi = NULL;
		clnt_handle->state = BGCOM_PROB_WAIT;
	} else {
		spi = container_of(bg_com_drv, struct bg_spi_priv, lhandle);
		clnt_handle->bg_spi = spi;
		clnt_handle->state = BGCOM_PROB_SUCCESS;
	}
	clnt_handle->cb = NULL;
	/* Interrupt callback Set-up */
	if (open_config && open_config->bgcom_notification_cb) {
		irq_notification = kzalloc(sizeof(*irq_notification),
			GFP_KERNEL);
		if (!irq_notification)
			goto error_ret;

		/* set irq node */
		irq_notification->handle = clnt_handle;
		irq_notification->priv = open_config->priv;
		irq_notification->bgcom_notification_cb =
					open_config->bgcom_notification_cb;
		add_to_irq_list(irq_notification);
		clnt_handle->cb = irq_notification;
	}
	return clnt_handle;

error_ret:
	kfree(clnt_handle);
	return NULL;
}
EXPORT_SYMBOL(bgcom_open);

int bgcom_close(void **handle)
{
	struct bg_context *lhandle;
	struct cb_data *cb = NULL;

	if (*handle == NULL)
		return -EINVAL;
	lhandle = *handle;
	cb = lhandle->cb;
	if (cb)
		list_del(&cb->list);

	kfree(*handle);
	*handle = NULL;
	return 0;
}
EXPORT_SYMBOL(bgcom_close);

static irqreturn_t bg_irq_tasklet_hndlr(int irq, void *device)
{
	struct bg_spi_priv *bg_spi = device;
	/* check if call-back exists */
	if (list_empty(&cb_head)) {
		pr_debug("No callback registered\n");
		return IRQ_HANDLED;
	} else if (spi_state == BGCOM_SPI_BUSY) {
		return IRQ_HANDLED;
	} else if (!bg_spi->irq_lock) {
		bg_spi->irq_lock = 1;
		bg_irq_tasklet_hndlr_l();
		bg_spi->irq_lock = 0;
	}
	return IRQ_HANDLED;
}

static void bg_spi_init(struct bg_spi_priv *bg_spi)
{
	if (!bg_spi) {
		pr_err("device not found\n");
		return;
	}

	/* BGCOM SPI set-up */
	mutex_init(&bg_spi->xfer_mutex);
	spi_message_init(&bg_spi->msg1);
	spi_message_add_tail(&bg_spi->xfer1, &bg_spi->msg1);

	/* BGCOM IRQ set-up */
	bg_spi->irq_lock = 0;

	spi_state = BGCOM_SPI_FREE;

	wq = create_singlethread_workqueue("input_wq");

	bg_spi->bg_state = BGCOM_STATE_ACTIVE;

	bg_com_drv = &bg_spi->lhandle;

	mutex_init(&bg_resume_mutex);
}

static int bg_spi_probe(struct spi_device *spi)
{
	struct bg_spi_priv *bg_spi;
	struct device_node *node;
	int irq_gpio = 0;
	int bg_irq = 0;
	int ret;

	bg_spi = devm_kzalloc(&spi->dev, sizeof(*bg_spi),
				   GFP_KERNEL | GFP_ATOMIC);
	if (!bg_spi)
		return -ENOMEM;
	bg_spi->spi = spi;
	spi_set_drvdata(spi, bg_spi);
	bg_spi_init(bg_spi);

	/* BGCOM Interrupt probe */
	node = spi->dev.of_node;
	irq_gpio = of_get_named_gpio(node, "qcom,irq-gpio", 0);
	if (!gpio_is_valid(irq_gpio)) {
		pr_err("gpio %d found is not valid\n", irq_gpio);
		goto err_ret;
	}

	ret = gpio_request(irq_gpio, "bgcom_gpio");
	if (ret) {
		pr_err("gpio %d request failed\n", irq_gpio);
		goto err_ret;
	}

	ret = gpio_direction_input(irq_gpio);
	if (ret) {
		pr_err("gpio_direction_input not set: %d\n", ret);
		goto err_ret;
	}

	bg_irq = gpio_to_irq(irq_gpio);
	ret = request_threaded_irq(bg_irq, NULL, bg_irq_tasklet_hndlr,
		IRQF_TRIGGER_HIGH | IRQF_ONESHOT, "qcom,bg_spi", bg_spi);

	if (ret)
		goto err_ret;

	pr_info("Bgcom Probed successfully\n");
	return ret;

err_ret:
	bg_com_drv = NULL;
	mutex_destroy(&bg_spi->xfer_mutex);
	spi_set_drvdata(spi, NULL);
	return -ENODEV;
}

static int bg_spi_remove(struct spi_device *spi)
{
	struct bg_spi_priv *bg_spi = spi_get_drvdata(spi);

	mutex_destroy(&bg_spi->xfer_mutex);
	devm_kfree(&spi->dev, bg_spi);
	spi_set_drvdata(spi, NULL);

	return 0;
}

static const struct of_device_id bg_spi_of_match[] = {
	{ .compatible = "qcom,bg-spi", },
	{ }
};
MODULE_DEVICE_TABLE(of, bg_spi_of_match);

static struct spi_driver bg_spi_driver = {
	.driver = {
		.name = "bg-spi",
		.of_match_table = bg_spi_of_match,
	},
	.probe = bg_spi_probe,
	.remove = bg_spi_remove,
};

module_spi_driver(bg_spi_driver);
MODULE_DESCRIPTION("bg SPI driver");
MODULE_LICENSE("GPL v2");
