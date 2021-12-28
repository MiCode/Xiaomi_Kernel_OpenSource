// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(msg) "helioscom: %s: " msg, __func__

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
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>
#include "helioscom.h"
#include "helioscom_interface.h"

#define HELIOS_SPI_WORD_SIZE (0x04)
#define HELIOS_SPI_READ_LEN (0x04)
#define HELIOS_SPI_WRITE_CMND_LEN (0x01)
#define HELIOS_SPI_FIFO_READ_CMD (0x41)
#define HELIOS_SPI_FIFO_WRITE_CMD (0x40)
#define HELIOS_SPI_AHB_READ_CMD (0x43)
#define HELIOS_SPI_AHB_WRITE_CMD (0x42)
#define HELIOS_SPI_AHB_CMD_LEN (0x05)
#define HELIOS_SPI_AHB_READ_CMD_LEN (0x08)
#define HELIOS_STATUS_REG (0x05)
#define HELIOS_CMND_REG (0x14)

#define HELIOS_SPI_MAX_WORDS (0x3FFFFFFD)
#define HELIOS_SPI_MAX_REGS (0x0A)
#define HED_EVENT_ID_LEN (0x02)
#define HED_EVENT_SIZE_LEN (0x02)
#define HED_EVENT_DATA_STRT_LEN (0x05)
#define CMA_BFFR_POOL_SIZE (128*1024)

#define HELIOS_OK_SLP_RBSC      BIT(30)
#define HELIOS_OK_SLP_S2R       BIT(31)

#define WR_PROTOCOL_OVERHEAD              (5)
#define WR_PROTOCOL_OVERHEAD_IN_WORDS     (2)

#define WR_BUF_SIZE_IN_BYTES	CMA_BFFR_POOL_SIZE
#define WR_BUF_SIZE_IN_WORDS	(CMA_BFFR_POOL_SIZE / sizeof(uint32_t))
#define WR_BUF_SIZE_IN_WORDS_FOR_USE   \
		(WR_BUF_SIZE_IN_WORDS - WR_PROTOCOL_OVERHEAD_IN_WORDS)

#define MAX_RETRY 100

enum helioscom_state {
	/*HELIOSCOM Staus ready*/
	HELIOSCOM_PROB_SUCCESS = 0,
	HELIOSCOM_PROB_WAIT = 1,
	HELIOSCOM_STATE_SUSPEND = 2,
	HELIOSCOM_STATE_ACTIVE = 3
};

enum helioscom_req_type {
	/*HELIOSCOM local requests*/
	HELIOSCOM_READ_REG = 0,
	HELIOSCOM_READ_FIFO = 1,
	HELIOSCOM_READ_AHB = 2,
	HELIOSCOM_WRITE_REG = 3,
};

struct helios_spi_priv {
	struct spi_device *spi;
	/* Transaction related */
	struct mutex xfer_mutex;
	void *lhandle;
	/* Message for single transfer */
	struct spi_message msg1;
	struct spi_transfer xfer1;
	int irq_lock;

	enum helioscom_state helios_state;
};

struct cb_data {
	void *priv;
	void *handle;
	void (*helioscom_notification_cb)(void *handle, void *priv,
		enum helioscom_event_type event,
		union helioscom_event_data_type *event_data);
	struct list_head list;
};

struct helios_context {
	struct helios_spi_priv *helios_spi;
	enum helioscom_state state;
	struct cb_data *cb;
};

struct event_list {
	struct event *evnt;
	struct list_head list;
};
static void *helios_com_drv;
static uint32_t g_slav_status_reg;

/* HELIOSCOM client callbacks set-up */
static void send_input_events(struct work_struct *work);
static struct list_head cb_head = LIST_HEAD_INIT(cb_head);
static struct list_head pr_lst_hd = LIST_HEAD_INIT(pr_lst_hd);
static DEFINE_SPINLOCK(lst_setup_lock);
static enum helioscom_spi_state spi_state;


static struct workqueue_struct *wq;
static DECLARE_WORK(input_work, send_input_events);

static struct mutex helios_resume_mutex;

static atomic_t  helios_is_spi_active;
static int helios_irq;

static uint8_t *fxd_mem_buffer;
static struct mutex cma_buffer_lock;

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
		//bgrsb_send_input(evnt);
		kfree(evnt);
		spin_lock(&lst_setup_lock);
		list_del(&node->list);
		spin_unlock(&lst_setup_lock);
		kfree(node);
	}
}

int helioscom_set_spi_state(enum helioscom_spi_state state)
{
	struct helios_spi_priv *helios_spi = container_of(helios_com_drv,
						struct helios_spi_priv, lhandle);
	const struct device spi_dev = helios_spi->spi->master->dev;
	ktime_t time_start, delta;
	s64 time_elapsed;

	if (state < 0 || state > 1)
		return -EINVAL;

	if (state == spi_state)
		return 0;

	mutex_lock(&helios_spi->xfer_mutex);
	if (state == HELIOSCOM_SPI_BUSY) {
		time_start = ktime_get();
		while (!pm_runtime_status_suspended(spi_dev.parent)) {
			delta = ktime_sub(ktime_get(), time_start);
			time_elapsed = ktime_to_ms(delta);
			WARN_ON(time_elapsed > 5 * MSEC_PER_SEC);
			msleep(100);
		}
	}
	spi_state = state;
	mutex_unlock(&helios_spi->xfer_mutex);
	return 0;
}
EXPORT_SYMBOL(helioscom_set_spi_state);

static inline
void add_to_irq_list(struct  cb_data *data)
{
	list_add_tail(&data->list, &cb_head);
}

static bool is_helioscom_ready(void)
{
	return (helios_com_drv != NULL ? true : false);
}

static void helios_spi_reinit_xfer(struct spi_transfer *xfer)
{
	xfer->tx_buf = NULL;
	xfer->rx_buf = NULL;
	xfer->delay_usecs = 0;
	xfer->len = 0;
}

static int read_helios_locl(enum helioscom_req_type req_type,
	uint32_t no_of_words, void *buf)
{

	struct helios_context clnt_handle;
	struct helios_spi_priv *spi =
			container_of(helios_com_drv, struct helios_spi_priv, lhandle);
	int ret = 0;

	if (!buf)
		return -EINVAL;

	clnt_handle.helios_spi = spi;

	switch (req_type) {
	case HELIOSCOM_READ_REG:
		ret = helioscom_reg_read(&clnt_handle,
			HELIOS_STATUS_REG, no_of_words, buf);
		break;
	case HELIOSCOM_READ_FIFO:
		ret = helioscom_fifo_read(&clnt_handle, no_of_words, buf);
		break;
	case HELIOSCOM_WRITE_REG:
		ret = helioscom_reg_write(&clnt_handle, HELIOS_CMND_REG,
					no_of_words, buf);
		break;
	case HELIOSCOM_READ_AHB:
		break;
	}
	return ret;
}

static int helioscom_transfer(void *handle, uint8_t *tx_buf,
	uint8_t *rx_buf, uint32_t txn_len)
{
	struct spi_transfer *tx_xfer;
	struct helios_spi_priv *helios_spi;
	struct helios_context *cntx;
	struct spi_device *spi;
	int ret;

	if (!handle || !tx_buf)
		return -EINVAL;

	cntx = (struct helios_context *)handle;

	if (cntx->state == HELIOSCOM_PROB_WAIT) {
		if (!is_helioscom_ready())
			return -ENODEV;
		cntx->helios_spi = container_of(helios_com_drv,
						struct helios_spi_priv, lhandle);
		cntx->state = HELIOSCOM_PROB_SUCCESS;
	}
	helios_spi = cntx->helios_spi;

	if (!helios_spi)
		return -ENODEV;

	tx_xfer = &helios_spi->xfer1;
	spi = helios_spi->spi;

	if (!atomic_read(&helios_is_spi_active))
		return -ECANCELED;

	mutex_lock(&helios_spi->xfer_mutex);
	helios_spi_reinit_xfer(tx_xfer);
	tx_xfer->tx_buf = tx_buf;
	if (rx_buf)
		tx_xfer->rx_buf = rx_buf;

	tx_xfer->len = txn_len;
	pm_runtime_get_sync(helios_spi->spi->controller->dev.parent);
	ret = spi_sync(spi, &helios_spi->msg1);
	pm_runtime_put_sync_suspend(helios_spi->spi->controller->dev.parent);
	mutex_unlock(&helios_spi->xfer_mutex);

	if (ret)
		pr_err("SPI transaction failed: %d\n", ret);
	return ret;
}

/* HELIOS-COM Interrupt handling */
static inline
void send_event(enum helioscom_event_type event,
	void *data)
{
	struct list_head *pos;
	struct cb_data *cb;

	/* send interrupt notification for each
	 * registered call-back
	 */
	list_for_each(pos, &cb_head) {
		cb = list_entry(pos, struct cb_data, list);
		cb->helioscom_notification_cb(cb->handle,
		cb->priv,  event, data);
	}
}

void helioscom_heliosdown_handler(void)
{
	send_event(HELIOSCOM_EVENT_RESET_OCCURRED, NULL);
	g_slav_status_reg = 0;
}
EXPORT_SYMBOL(helioscom_heliosdown_handler);

static void parse_fifo(uint8_t *data, union helioscom_event_data_type *event_data)
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
			spin_lock(&lst_setup_lock);
			list_add_tail(&data_list->list, &pr_lst_hd);
			spin_unlock(&lst_setup_lock);
		} else if (event_id == 0x0001) {
			evnt_data = kmalloc(p_len, GFP_KERNEL);
			if (evnt_data != NULL) {
				memcpy(evnt_data, data, p_len);
				event_data->fifo_data.to_master_fifo_used =
						p_len/HELIOS_SPI_WORD_SIZE;
				event_data->fifo_data.data = evnt_data;
				send_event(HELIOSCOM_EVENT_TO_MASTER_FIFO_USED,
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
	union helioscom_event_data_type event_data = { .fifo_data = {0} };

	master_fifo_used = (uint16_t)fifo_fill_reg;
	slave_fifo_free = (uint16_t)(fifo_fill_reg >> 16);

	if (slav_status_auto_clear_reg & BIT(31))
		send_event(HELIOSCOM_EVENT_RESET_OCCURRED, NULL);

	if (slav_status_auto_clear_reg & BIT(30))
		send_event(HELIOSCOM_EVENT_ERROR_WRITE_FIFO_OVERRUN, NULL);

	if (slav_status_auto_clear_reg & BIT(29))
		send_event(HELIOSCOM_EVENT_ERROR_WRITE_FIFO_BUS_ERR, NULL);

	if (slav_status_auto_clear_reg & BIT(28))
		send_event(HELIOSCOM_EVENT_ERROR_WRITE_FIFO_ACCESS, NULL);

	if (slav_status_auto_clear_reg & BIT(27))
		send_event(HELIOSCOM_EVENT_ERROR_READ_FIFO_UNDERRUN, NULL);

	if (slav_status_auto_clear_reg & BIT(26))
		send_event(HELIOSCOM_EVENT_ERROR_READ_FIFO_BUS_ERR, NULL);

	if (slav_status_auto_clear_reg & BIT(25))
		send_event(HELIOSCOM_EVENT_ERROR_READ_FIFO_ACCESS, NULL);

	if (slav_status_auto_clear_reg & BIT(24))
		send_event(HELIOSCOM_EVENT_ERROR_TRUNCATED_READ, NULL);

	if (slav_status_auto_clear_reg & BIT(23))
		send_event(HELIOSCOM_EVENT_ERROR_TRUNCATED_WRITE, NULL);

	if (slav_status_auto_clear_reg & BIT(22))
		send_event(HELIOSCOM_EVENT_ERROR_AHB_ILLEGAL_ADDRESS, NULL);

	if (slav_status_auto_clear_reg & BIT(21))
		send_event(HELIOSCOM_EVENT_ERROR_AHB_BUS_ERR, NULL);

	/* check if HELIOS status is changed */
	if (g_slav_status_reg ^ slav_status_reg) {
		if (slav_status_reg & BIT(30)) {
			event_data.application_running = true;
			send_event(HELIOSCOM_EVENT_APPLICATION_RUNNING,
				&event_data);
		}

		if (slav_status_reg & BIT(29)) {
			event_data.to_slave_fifo_ready = true;
			send_event(HELIOSCOM_EVENT_TO_SLAVE_FIFO_READY,
				&event_data);
		}

		if (slav_status_reg & BIT(28)) {
			event_data.to_master_fifo_ready = true;
			send_event(HELIOSCOM_EVENT_TO_MASTER_FIFO_READY,
				&event_data);
		}

		if (slav_status_reg & BIT(27)) {
			event_data.ahb_ready = true;
			send_event(HELIOSCOM_EVENT_AHB_READY,
				&event_data);
		}


	}

	if (master_fifo_used > 0) {
		ptr = kzalloc(master_fifo_used*HELIOS_SPI_WORD_SIZE + 1,
			GFP_KERNEL | GFP_ATOMIC);
		if (ptr != NULL) {
			ret = read_helios_locl(HELIOSCOM_READ_FIFO,
				master_fifo_used,  ptr);
			if (!ret) {
				augmnt_fifo((uint8_t *)ptr,
					master_fifo_used*HELIOS_SPI_WORD_SIZE);
				parse_fifo((uint8_t *)ptr, &event_data);
			}
			kfree(ptr);
		}
	}

	event_data.to_slave_fifo_free = slave_fifo_free;
	send_event(HELIOSCOM_EVENT_TO_SLAVE_FIFO_FREE, &event_data);
}

static void helios_irq_tasklet_hndlr_l(void)
{
	uint32_t slave_status_reg;
	uint32_t glink_isr_reg;
	uint32_t slav_status_auto_clear_reg;
	uint32_t fifo_fill_reg;
	uint32_t fifo_size_reg;
	int ret =  0;
	uint32_t irq_buf[5] = {0};

	ret = read_helios_locl(HELIOSCOM_READ_REG, 5, &irq_buf[0]);
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

int helioscom_ahb_read(void *handle, uint32_t ahb_start_addr,
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
		|| num_words > HELIOS_SPI_MAX_WORDS) {
		pr_err("Invalid param\n");
		return -EINVAL;
	}
	if (!is_helioscom_ready())
		return -ENODEV;

	if (spi_state == HELIOSCOM_SPI_BUSY) {
		pr_err("Device busy\n");
		return -EBUSY;
	}

	if (helioscom_resume(handle)) {
		pr_err("Failed to resume\n");
		return -EBUSY;
	}

	size = num_words*HELIOS_SPI_WORD_SIZE;
	txn_len = HELIOS_SPI_AHB_READ_CMD_LEN + size;

	tx_buf = kzalloc(txn_len, GFP_KERNEL | GFP_ATOMIC);
	if (!tx_buf)
		return -ENOMEM;

	rx_buf = kzalloc(txn_len, GFP_KERNEL | GFP_ATOMIC);
	if (!rx_buf) {
		kfree(tx_buf);
		return -ENOMEM;
	}

	cmnd |= HELIOS_SPI_AHB_READ_CMD;
	ahb_addr |= ahb_start_addr;

	memcpy(tx_buf, &cmnd, sizeof(cmnd));
	memcpy(tx_buf+sizeof(cmnd), &ahb_addr, sizeof(ahb_addr));

	ret = helioscom_transfer(handle, tx_buf, rx_buf, txn_len);

	if (!ret)
		memcpy(read_buf, rx_buf+HELIOS_SPI_AHB_READ_CMD_LEN, size);

	kfree(tx_buf);
	kfree(rx_buf);
	return ret;
}
EXPORT_SYMBOL(helioscom_ahb_read);

int helioscom_ahb_write(void *handle, uint32_t ahb_start_addr,
	uint32_t num_words, void *write_buf)
{
	uint32_t txn_len;
	uint8_t *tx_buf;
	uint32_t size;
	int ret;
	uint8_t cmnd = 0;
	uint32_t ahb_addr = 0;
	uint32_t curr_num_words;
	uint32_t curr_num_bytes;

	if (!handle || !write_buf || num_words == 0
		|| num_words > HELIOS_SPI_MAX_WORDS) {
		pr_err("Invalid param\n");
		return -EINVAL;
	}

	if (!is_helioscom_ready())
		return -ENODEV;

	if (spi_state == HELIOSCOM_SPI_BUSY) {
		pr_err("Device busy\n");
		return -EBUSY;
	}

	if (helioscom_resume(handle)) {
		pr_err("Failed to resume\n");
		return -EBUSY;
	}

	ahb_addr = ahb_start_addr;

	mutex_lock(&cma_buffer_lock);
	size = num_words*HELIOS_SPI_WORD_SIZE;
	while (num_words) {
		curr_num_words = (num_words < WR_BUF_SIZE_IN_WORDS_FOR_USE) ?
						num_words : WR_BUF_SIZE_IN_WORDS_FOR_USE;
		curr_num_bytes = curr_num_words * HELIOS_SPI_WORD_SIZE;

		txn_len = HELIOS_SPI_AHB_CMD_LEN + curr_num_bytes;
		memset(fxd_mem_buffer, 0, txn_len);
		tx_buf = fxd_mem_buffer;

		cmnd |= HELIOS_SPI_AHB_WRITE_CMD;

		memcpy(tx_buf, &cmnd, sizeof(cmnd));
		memcpy(tx_buf+sizeof(cmnd), &ahb_addr, sizeof(ahb_addr));
		memcpy(tx_buf+HELIOS_SPI_AHB_CMD_LEN, write_buf, curr_num_bytes);

		ret = helioscom_transfer(handle, tx_buf, NULL, txn_len);
		if (ret) {
			pr_err("helioscom_transfer fail with error %d\n", ret);
			goto error;
		}
		write_buf += curr_num_bytes;
		ahb_addr += curr_num_bytes;
		num_words -= curr_num_words;
	}

error:
	mutex_unlock(&cma_buffer_lock);
	return ret;
}
EXPORT_SYMBOL(helioscom_ahb_write);

int helioscom_fifo_write(void *handle, uint32_t num_words,
	void  *write_buf)
{
	uint32_t txn_len;
	uint8_t *tx_buf;
	uint32_t size;
	int ret;
	uint8_t cmnd = 0;

	if (!handle || !write_buf || num_words == 0
		|| num_words > HELIOS_SPI_MAX_WORDS) {
		pr_err("Invalid param\n");
		return -EINVAL;
	}

	if (!is_helioscom_ready())
		return -ENODEV;

	if (spi_state == HELIOSCOM_SPI_BUSY) {
		pr_err("Device busy\n");
		return -EBUSY;
	}

	if (helioscom_resume(handle)) {
		pr_err("Failed to resume\n");
		return -EBUSY;
	}

	size = num_words*HELIOS_SPI_WORD_SIZE;
	txn_len = HELIOS_SPI_WRITE_CMND_LEN + size;

	tx_buf = kzalloc(txn_len, GFP_KERNEL | GFP_ATOMIC);

	if (!tx_buf)
		return -ENOMEM;

	cmnd |= HELIOS_SPI_FIFO_WRITE_CMD;
	memcpy(tx_buf, &cmnd, sizeof(cmnd));
	memcpy(tx_buf+sizeof(cmnd), write_buf, size);

	ret = helioscom_transfer(handle, tx_buf, NULL, txn_len);
	kfree(tx_buf);
	return ret;
}
EXPORT_SYMBOL(helioscom_fifo_write);

int helioscom_fifo_read(void *handle, uint32_t num_words,
	void *read_buf)
{
	uint32_t txn_len;
	uint8_t *tx_buf;
	uint8_t *rx_buf;
	uint32_t size;
	uint8_t cmnd = 0;
	int ret =  0;

	if (!handle || !read_buf || num_words == 0
		|| num_words > HELIOS_SPI_MAX_WORDS) {
		pr_err("Invalid param\n");
		return -EINVAL;
	}

	if (!is_helioscom_ready())
		return -ENODEV;

	if (spi_state == HELIOSCOM_SPI_BUSY) {
		pr_err("Device busy\n");
		return -EBUSY;
	}

	if (helioscom_resume(handle)) {
		pr_err("Failed to resume\n");
		return -EBUSY;
	}

	size = num_words*HELIOS_SPI_WORD_SIZE;
	txn_len = HELIOS_SPI_READ_LEN + size;
	tx_buf = kzalloc(txn_len, GFP_KERNEL | GFP_ATOMIC);

	if (!tx_buf)
		return -ENOMEM;

	rx_buf = kzalloc(txn_len, GFP_KERNEL | GFP_ATOMIC);

	if (!rx_buf) {
		kfree(tx_buf);
		return -ENOMEM;
	}

	cmnd |= HELIOS_SPI_FIFO_READ_CMD;
	memcpy(tx_buf, &cmnd, sizeof(cmnd));

	ret = helioscom_transfer(handle, tx_buf, rx_buf, txn_len);

	if (!ret)
		memcpy(read_buf, rx_buf+HELIOS_SPI_READ_LEN, size);
	kfree(tx_buf);
	kfree(rx_buf);
	return ret;
}
EXPORT_SYMBOL(helioscom_fifo_read);

int helioscom_reg_write(void *handle, uint8_t reg_start_addr,
	uint8_t num_regs, void *write_buf)
{
	uint32_t txn_len;
	uint8_t *tx_buf;
	uint32_t size;
	uint8_t cmnd = 0;
	int ret =  0;

	if (!handle || !write_buf || num_regs == 0
		|| num_regs > HELIOS_SPI_MAX_REGS) {
		pr_err("Invalid param\n");
		return -EINVAL;
	}

	if (!is_helioscom_ready())
		return -ENODEV;

	if (spi_state == HELIOSCOM_SPI_BUSY) {
		pr_err("Device busy\n");
		return -EBUSY;
	}

	if (helioscom_resume(handle)) {
		pr_err("Failed to resume\n");
		return -EBUSY;
	}

	size = num_regs*HELIOS_SPI_WORD_SIZE;
	txn_len = HELIOS_SPI_WRITE_CMND_LEN + size;

	tx_buf = kzalloc(txn_len, GFP_KERNEL);

	if (!tx_buf)
		return -ENOMEM;

	cmnd |= reg_start_addr;
	memcpy(tx_buf, &cmnd, sizeof(cmnd));
	memcpy(tx_buf+sizeof(cmnd), write_buf, size);

	ret = helioscom_transfer(handle, tx_buf, NULL, txn_len);
	kfree(tx_buf);
	return ret;
}
EXPORT_SYMBOL(helioscom_reg_write);

int helioscom_reg_read(void *handle, uint8_t reg_start_addr,
	uint32_t num_regs, void *read_buf)
{
	uint32_t txn_len;
	uint8_t *tx_buf;
	uint8_t *rx_buf;
	uint32_t size;
	int ret;
	uint8_t cmnd = 0;

	if (!handle || !read_buf || num_regs == 0
		|| num_regs > HELIOS_SPI_MAX_REGS) {
		pr_err("Invalid param\n");
		return -EINVAL;
	}

	if (!is_helioscom_ready())
		return -ENODEV;

	if (spi_state == HELIOSCOM_SPI_BUSY) {
		pr_err("Device busy\n");
		return -EBUSY;
	}

	size = num_regs*HELIOS_SPI_WORD_SIZE;
	txn_len = HELIOS_SPI_READ_LEN + size;

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

	ret = helioscom_transfer(handle, tx_buf, rx_buf, txn_len);

	if (!ret)
		memcpy(read_buf, rx_buf+HELIOS_SPI_READ_LEN, size);
	kfree(tx_buf);
	kfree(rx_buf);
	return ret;
}
EXPORT_SYMBOL(helioscom_reg_read);

static int is_helios_resume(void *handle)
{
	uint32_t txn_len;
	int ret;
	uint8_t tx_buf[8] = {0};
	uint8_t rx_buf[8] = {0};
	uint32_t cmnd_reg = 0;

	if (spi_state == HELIOSCOM_SPI_BUSY) {
		printk_ratelimited("SPI is held by TZ\n");
		goto ret_err;
	}

	txn_len = 0x08;
	tx_buf[0] = 0x05;
	ret = helioscom_transfer(handle, tx_buf, rx_buf, txn_len);
	if (!ret)
		memcpy(&cmnd_reg, rx_buf+HELIOS_SPI_READ_LEN, 0x04);

ret_err:
	return cmnd_reg & BIT(31);
}

int helioscom_resume(void *handle)
{
	struct helios_spi_priv *helios_spi;
	struct helios_context *cntx;
	int retry = 0;

	if (handle == NULL)
		return -EINVAL;

	if (!atomic_read(&helios_is_spi_active))
		return -ECANCELED;

	cntx = (struct helios_context *)handle;

	/* if client is outside helioscom scope and
	 * handle is provided before HELIOSCOM probed
	 */
	if (cntx->state == HELIOSCOM_PROB_WAIT) {
		pr_info("handle is provided before HELIOSCOM probed\n");
		if (!is_helioscom_ready())
			return -EAGAIN;
		cntx->helios_spi = container_of(helios_com_drv,
						struct helios_spi_priv, lhandle);
		cntx->state = HELIOSCOM_PROB_SUCCESS;
	}

	helios_spi = cntx->helios_spi;

	mutex_lock(&helios_resume_mutex);
	if (helios_spi->helios_state == HELIOSCOM_STATE_ACTIVE)
		goto unlock;
	enable_irq(helios_irq);
	do {
		if (!(g_slav_status_reg & BIT(31))) {
			pr_err("Helios boot is not complete, skip SPI resume\n");
			return 0;
		}
		if (is_helios_resume(handle)) {
			helios_spi->helios_state = HELIOSCOM_STATE_ACTIVE;
			break;
		}
		udelay(1000);
		++retry;
	} while (retry < MAX_RETRY);

unlock:
	mutex_unlock(&helios_resume_mutex);
	if (retry == MAX_RETRY) {
		/* HELIOS failed to resume. Trigger HELIOS soft reset. */
		pr_err("HELIOS failed to resume\n");
		//pr_err("%s: gpio#95 value is: %d\n",
		//		__func__, gpio_get_value(95));
		//pr_err("%s: gpio#97 value is: %d\n",
		//		__func__, gpio_get_value(97));
		BUG();
		//bg_soft_reset();
		return -ETIMEDOUT;
	}
	return 0;
}
EXPORT_SYMBOL(helioscom_resume);

int helioscom_suspend(void *handle)
{
	if (!handle)
		return -EINVAL;
	return 0;
}
EXPORT_SYMBOL(helioscom_suspend);

void *helioscom_open(struct helioscom_open_config_type *open_config)
{
	struct helios_spi_priv *spi;
	struct cb_data *irq_notification;
	struct helios_context  *clnt_handle =
			kzalloc(sizeof(*clnt_handle), GFP_KERNEL);

	if (!clnt_handle)
		return NULL;

	/* Client handle Set-up */
	if (!is_helioscom_ready()) {
		clnt_handle->helios_spi = NULL;
		clnt_handle->state = HELIOSCOM_PROB_WAIT;
	} else {
		spi = container_of(helios_com_drv, struct helios_spi_priv, lhandle);
		clnt_handle->helios_spi = spi;
		clnt_handle->state = HELIOSCOM_PROB_SUCCESS;
	}
	clnt_handle->cb = NULL;
	/* Interrupt callback Set-up */
	if (open_config && open_config->helioscom_notification_cb) {
		irq_notification = kzalloc(sizeof(*irq_notification),
			GFP_KERNEL);
		if (!irq_notification)
			goto error_ret;

		/* set irq node */
		irq_notification->handle = clnt_handle;
		irq_notification->priv = open_config->priv;
		irq_notification->helioscom_notification_cb =
					open_config->helioscom_notification_cb;
		add_to_irq_list(irq_notification);
		clnt_handle->cb = irq_notification;
	}
	return clnt_handle;

error_ret:
	kfree(clnt_handle);
	return NULL;
}
EXPORT_SYMBOL(helioscom_open);


void *helioscom_pil_reset_register(struct helioscom_open_config_type *open_config)
{
	struct helios_spi_priv *spi;
	struct cb_data *irq_notification;
	struct helios_context  *clnt_handle =
			kzalloc(sizeof(*clnt_handle), GFP_KERNEL);

	if (!clnt_handle)
		return NULL;

	/* Client handle Set-up */
	if (!is_helioscom_ready()) {
		clnt_handle->helios_spi = NULL;
		clnt_handle->state = HELIOSCOM_PROB_WAIT;
	} else {
		spi = container_of(helios_com_drv, struct helios_spi_priv, lhandle);
		clnt_handle->helios_spi = spi;
		clnt_handle->state = HELIOSCOM_PROB_SUCCESS;
	}
	clnt_handle->cb = NULL;
	/* Interrupt callback Set-up */
	if (open_config && open_config->helioscom_notification_cb) {
		irq_notification = kzalloc(sizeof(*irq_notification),
			GFP_KERNEL);
		if (!irq_notification)
			goto error_ret;

		/* set irq node */
		irq_notification->handle = clnt_handle;
		irq_notification->priv = open_config->priv;
		irq_notification->helioscom_notification_cb =
					open_config->helioscom_notification_cb;
		add_to_irq_list(irq_notification);
		clnt_handle->cb = irq_notification;
	}
	return clnt_handle;

error_ret:
	kfree(clnt_handle);
	return NULL;
}
EXPORT_SYMBOL(helioscom_pil_reset_register);

int helioscom_close(void **handle)
{
	struct helios_context *lhandle;
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
EXPORT_SYMBOL(helioscom_close);

static irqreturn_t helios_irq_tasklet_hndlr(int irq, void *device)
{
	struct helios_spi_priv *helios_spi = device;

	/* check if call-back exists */
	if (!atomic_read(&helios_is_spi_active)) {
		pr_debug("Interrupt received in suspend state\n");
		return IRQ_HANDLED;
	} else if (list_empty(&cb_head)) {
		pr_debug("No callback registered\n");
		return IRQ_HANDLED;
	} else if (spi_state == HELIOSCOM_SPI_BUSY) {
		/* delay for SPI to be freed */
		msleep(50);
		return IRQ_HANDLED;
	} else if (!helios_spi->irq_lock) {
		helios_spi->irq_lock = 1;
		helios_irq_tasklet_hndlr_l();
		helios_spi->irq_lock = 0;
	}
	return IRQ_HANDLED;
}

static void helios_spi_init(struct helios_spi_priv *helios_spi)
{
	if (!helios_spi) {
		pr_err("device not found\n");
		return;
	}

	/* HELIOSCOM SPI set-up */
	mutex_init(&helios_spi->xfer_mutex);
	spi_message_init(&helios_spi->msg1);
	spi_message_add_tail(&helios_spi->xfer1, &helios_spi->msg1);

	/* HELIOSCOM IRQ set-up */
	helios_spi->irq_lock = 0;

	spi_state = HELIOSCOM_SPI_FREE;

	wq = create_singlethread_workqueue("input_wq");

	helios_spi->helios_state = HELIOSCOM_STATE_ACTIVE;

	helios_com_drv = &helios_spi->lhandle;

	mutex_init(&helios_resume_mutex);

	fxd_mem_buffer = kmalloc(CMA_BFFR_POOL_SIZE, GFP_KERNEL | GFP_ATOMIC);

	mutex_init(&cma_buffer_lock);
}

static int helios_spi_probe(struct spi_device *spi)
{
	struct helios_spi_priv *helios_spi;
	struct device_node *node;
	int irq_gpio = 0;
	int ret;

	helios_spi = devm_kzalloc(&spi->dev, sizeof(*helios_spi),
				   GFP_KERNEL | GFP_ATOMIC);

	pr_info("%s started\n", __func__);

	if (!helios_spi)
		return -ENOMEM;
	helios_spi->spi = spi;
	spi_set_drvdata(spi, helios_spi);
	helios_spi_init(helios_spi);

	/* HELIOSCOM Interrupt probe */
	node = spi->dev.of_node;
	irq_gpio = of_get_named_gpio(node, "qcom,irq-gpio", 0);
	if (!gpio_is_valid(irq_gpio)) {
		pr_err("gpio %d found is not valid\n", irq_gpio);
		goto err_ret;
	}

	ret = gpio_request(irq_gpio, "helioscom_gpio");
	if (ret) {
		pr_err("gpio %d request failed\n", irq_gpio);
		goto err_ret;
	}

	ret = gpio_direction_input(irq_gpio);
	if (ret) {
		pr_err("gpio_direction_input not set: %d\n", ret);
		goto err_ret;
	}

	helios_irq = gpio_to_irq(irq_gpio);
	ret = request_threaded_irq(helios_irq, NULL, helios_irq_tasklet_hndlr,
		IRQF_TRIGGER_HIGH | IRQF_ONESHOT, "qcom-helios_spi", helios_spi);

	if (ret)
		goto err_ret;

	atomic_set(&helios_is_spi_active, 1);
	dma_set_coherent_mask(&spi->dev, DMA_BIT_MASK(64));
	pr_info("%s success\n", __func__);
	pr_info("Bgcom Probed successfully\n");
	return ret;

err_ret:
	helios_com_drv = NULL;
	mutex_destroy(&helios_spi->xfer_mutex);
	spi_set_drvdata(spi, NULL);
	return -ENODEV;
}

static int helios_spi_remove(struct spi_device *spi)
{
	struct helios_spi_priv *helios_spi = spi_get_drvdata(spi);

	helios_com_drv = NULL;
	mutex_destroy(&helios_spi->xfer_mutex);
	spi_set_drvdata(spi, NULL);
	if (fxd_mem_buffer != NULL)
		kfree(fxd_mem_buffer);
	mutex_destroy(&cma_buffer_lock);
	return 0;
}

static void helios_spi_shutdown(struct spi_device *spi)
{
	helios_spi_remove(spi);
}

static int helioscom_pm_suspend(struct device *dev)
{
	uint32_t cmnd_reg = 0;
	struct spi_device *s_dev = to_spi_device(dev);
	struct helios_spi_priv *helios_spi = spi_get_drvdata(s_dev);
	int ret = 0;

	if (helios_spi->helios_state == HELIOSCOM_STATE_SUSPEND)
		return 0;

	if (!(g_slav_status_reg & BIT(31))) {
		pr_err("Helios boot is not complete, skip SPI suspend\n");
		return 0;
	}

	cmnd_reg |= HELIOS_OK_SLP_RBSC;

	ret = read_helios_locl(HELIOSCOM_WRITE_REG, 1, &cmnd_reg);
	if (ret == 0) {
		helios_spi->helios_state = HELIOSCOM_STATE_SUSPEND;
		atomic_set(&helios_is_spi_active, 0);
		disable_irq(helios_irq);
	}
	pr_info("suspended with : %d\n", ret);
	return ret;
}

static int helioscom_pm_resume(struct device *dev)
{
	struct helios_context clnt_handle;
	int ret;
	struct helios_spi_priv *spi =
		container_of(helios_com_drv, struct helios_spi_priv, lhandle);

	if (!(g_slav_status_reg & BIT(31))) {
		pr_err("Helios boot is not complete, skip SPI resume\n");
		return 0;
	}
	clnt_handle.helios_spi = spi;
	atomic_set(&helios_is_spi_active, 1);
	ret = helioscom_resume(&clnt_handle);
	pr_info("Bgcom resumed with : %d\n", ret);
	return ret;
}

static const struct dev_pm_ops helioscom_pm = {
	.suspend = helioscom_pm_suspend,
	.resume = helioscom_pm_resume,
};

static const struct of_device_id helios_spi_of_match[] = {
	{ .compatible = "qcom,helios-spi", },
	{ }
};
MODULE_DEVICE_TABLE(of, helios_spi_of_match);

static struct spi_driver helios_spi_driver = {
	.driver = {
		.name = "helios-spi",
		.of_match_table = helios_spi_of_match,
		.pm = &helioscom_pm,
	},
	.probe = helios_spi_probe,
	.remove = helios_spi_remove,
	.shutdown = helios_spi_shutdown,
};

module_spi_driver(helios_spi_driver);
MODULE_DESCRIPTION("helios SPI driver");
MODULE_LICENSE("GPL v2");
