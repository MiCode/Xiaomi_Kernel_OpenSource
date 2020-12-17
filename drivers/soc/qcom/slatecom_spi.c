// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(msg) "slatecom: %s: " msg, __func__

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
#include "slatecom.h"
#include "slatecom_interface.h"

#define SLATE_SPI_WORD_SIZE (0x04)
#define SLATE_SPI_READ_LEN (0x04)
#define SLATE_SPI_WRITE_CMND_LEN (0x01)
#define SLATE_SPI_FIFO_READ_CMD (0x41)
#define SLATE_SPI_FIFO_WRITE_CMD (0x40)
#define SLATE_SPI_AHB_READ_CMD (0x43)
#define SLATE_SPI_AHB_WRITE_CMD (0x42)
#define SLATE_SPI_AHB_CMD_LEN (0x05)
#define SLATE_SPI_AHB_READ_CMD_LEN (0x08)
#define SLATE_STATUS_REG (0x05)
#define SLATE_CMND_REG (0x14)

#define SLATE_SPI_MAX_WORDS (0x3FFFFFFD)
#define SLATE_SPI_MAX_REGS (0x0A)
#define HED_EVENT_ID_LEN (0x02)
#define HED_EVENT_SIZE_LEN (0x02)
#define HED_EVENT_DATA_STRT_LEN (0x05)
#define CMA_BFFR_POOL_SIZE (128*1024)

#define MAX_RETRY 100

enum slatecom_state {
	/*SLATECOM Staus ready*/
	SLATECOM_PROB_SUCCESS = 0,
	SLATECOM_PROB_WAIT = 1,
	SLATECOM_STATE_SUSPEND = 2,
	SLATECOM_STATE_ACTIVE = 3
};

enum slatecom_req_type {
	/*SLATECOM local requests*/
	SLATECOM_READ_REG = 0,
	SLATECOM_READ_FIFO = 1,
	SLATECOM_READ_AHB = 2,
	SLATECOM_WRITE_REG = 3,
};

struct slate_spi_priv {
	struct spi_device *spi;
	/* Transaction related */
	struct mutex xfer_mutex;
	void *lhandle;
	/* Message for single transfer */
	struct spi_message msg1;
	struct spi_transfer xfer1;
	int irq_lock;

	enum slatecom_state slate_state;
};

struct cb_data {
	void *priv;
	void *handle;
	void (*slatecom_notification_cb)(void *handle, void *priv,
		enum slatecom_event_type event,
		union slatecom_event_data_type *event_data);
	struct list_head list;
};

struct slate_context {
	struct slate_spi_priv *slate_spi;
	enum slatecom_state state;
	struct cb_data *cb;
};

struct event_list {
	struct event *evnt;
	struct list_head list;
};
static void *slate_com_drv;
static uint32_t g_slav_status_reg;

/* SLATECOM client callbacks set-up */
static void send_input_events(struct work_struct *work);
static struct list_head cb_head = LIST_HEAD_INIT(cb_head);
static struct list_head pr_lst_hd = LIST_HEAD_INIT(pr_lst_hd);
static DEFINE_SPINLOCK(lst_setup_lock);
static enum slatecom_spi_state spi_state;


static struct workqueue_struct *wq;
static DECLARE_WORK(input_work, send_input_events);

static struct mutex slate_resume_mutex;

static atomic_t  slate_is_spi_active;
static int slate_irq;

static uint8_t *fxd_mem_buffer;
static struct mutex cma_buffer_lock;

static struct spi_device *get_spi_device(void)
{
	struct slate_spi_priv *slate_spi = container_of(slate_com_drv,
						struct slate_spi_priv, lhandle);
	struct spi_device *spi = slate_spi->spi;
	return spi;
}

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

int slatecom_set_spi_state(enum slatecom_spi_state state)
{
	struct slate_spi_priv *slate_spi = container_of(slate_com_drv,
						struct slate_spi_priv, lhandle);
	const struct device spi_dev = slate_spi->spi->master->dev;
	ktime_t time_start, delta;
	s64 time_elapsed;

	if (state < 0 || state > 1)
		return -EINVAL;

	if (state == spi_state)
		return 0;

	mutex_lock(&slate_spi->xfer_mutex);
	if (state == SLATECOM_SPI_BUSY) {
		time_start = ktime_get();
		while (!pm_runtime_status_suspended(spi_dev.parent)) {
			delta = ktime_sub(ktime_get(), time_start);
			time_elapsed = ktime_to_ms(delta);
			WARN_ON(time_elapsed > 5 * MSEC_PER_SEC);
			msleep(100);
		}
	}
	spi_state = state;
	mutex_unlock(&slate_spi->xfer_mutex);
	return 0;
}
EXPORT_SYMBOL(slatecom_set_spi_state);

static inline
void add_to_irq_list(struct  cb_data *data)
{
	list_add_tail(&data->list, &cb_head);
}

static bool is_slatecom_ready(void)
{
	return (slate_com_drv != NULL ? true : false);
}

static void slate_spi_reinit_xfer(struct spi_transfer *xfer)
{
	xfer->tx_buf = NULL;
	xfer->rx_buf = NULL;
	xfer->delay_usecs = 0;
	xfer->len = 0;
}

static int read_slate_locl(enum slatecom_req_type req_type,
	uint32_t no_of_words, void *buf)
{

	struct slate_context clnt_handle;
	struct slate_spi_priv *spi =
			container_of(slate_com_drv, struct slate_spi_priv, lhandle);
	int ret = 0;

	if (!buf)
		return -EINVAL;

	clnt_handle.slate_spi = spi;

	switch (req_type) {
	case SLATECOM_READ_REG:
		ret = slatecom_reg_read(&clnt_handle,
			SLATE_STATUS_REG, no_of_words, buf);
		break;
	case SLATECOM_READ_FIFO:
		ret = slatecom_fifo_read(&clnt_handle, no_of_words, buf);
		break;
	case SLATECOM_WRITE_REG:
		ret = slatecom_reg_write(&clnt_handle, SLATE_CMND_REG,
					no_of_words, buf);
		break;
	case SLATECOM_READ_AHB:
		break;
	}
	return ret;
}

static int slatecom_transfer(void *handle, uint8_t *tx_buf,
	uint8_t *rx_buf, uint32_t txn_len)
{
	struct spi_transfer *tx_xfer;
	struct slate_spi_priv *slate_spi;
	struct slate_context *cntx;
	struct spi_device *spi;
	int ret;

	if (!handle || !tx_buf)
		return -EINVAL;

	cntx = (struct slate_context *)handle;

	if (cntx->state == SLATECOM_PROB_WAIT) {
		if (!is_slatecom_ready())
			return -ENODEV;
		cntx->slate_spi = container_of(slate_com_drv,
						struct slate_spi_priv, lhandle);
		cntx->state = SLATECOM_PROB_SUCCESS;
	}
	slate_spi = cntx->slate_spi;

	if (!slate_spi)
		return -ENODEV;

	tx_xfer = &slate_spi->xfer1;
	spi = slate_spi->spi;

	if (!atomic_read(&slate_is_spi_active))
		return -ECANCELED;

	mutex_lock(&slate_spi->xfer_mutex);
	slate_spi_reinit_xfer(tx_xfer);
	tx_xfer->tx_buf = tx_buf;
	if (rx_buf)
		tx_xfer->rx_buf = rx_buf;

	tx_xfer->len = txn_len;
	pm_runtime_get_sync(slate_spi->spi->controller->dev.parent);
	ret = spi_sync(spi, &slate_spi->msg1);
	pm_runtime_put_sync_suspend(slate_spi->spi->controller->dev.parent);
	mutex_unlock(&slate_spi->xfer_mutex);

	if (ret)
		pr_err("SPI transaction failed: %d\n", ret);
	return ret;
}

/* SLATE-COM Interrupt handling */
static inline
void send_event(enum slatecom_event_type event,
	void *data)
{
	struct list_head *pos;
	struct cb_data *cb;

	/* send interrupt notification for each
	 * registered call-back
	 */
	list_for_each(pos, &cb_head) {
		cb = list_entry(pos, struct cb_data, list);
		cb->slatecom_notification_cb(cb->handle,
		cb->priv,  event, data);
	}
}

void slatecom_slatedown_handler(void)
{
	send_event(SLATECOM_EVENT_RESET_OCCURRED, NULL);
	g_slav_status_reg = 0;
}
EXPORT_SYMBOL(slatecom_slatedown_handler);

static void parse_fifo(uint8_t *data, union slatecom_event_data_type *event_data)
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
						p_len/SLATE_SPI_WORD_SIZE;
				event_data->fifo_data.data = evnt_data;
				send_event(SLATECOM_EVENT_TO_MASTER_FIFO_USED,
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
	union slatecom_event_data_type event_data = { .fifo_data = {0} };

	master_fifo_used = (uint16_t)fifo_fill_reg;
	slave_fifo_free = (uint16_t)(fifo_fill_reg >> 16);

	if (slav_status_auto_clear_reg & BIT(31))
		send_event(SLATECOM_EVENT_RESET_OCCURRED, NULL);

	if (slav_status_auto_clear_reg & BIT(30))
		send_event(SLATECOM_EVENT_ERROR_WRITE_FIFO_OVERRUN, NULL);

	if (slav_status_auto_clear_reg & BIT(29))
		send_event(SLATECOM_EVENT_ERROR_WRITE_FIFO_BUS_ERR, NULL);

	if (slav_status_auto_clear_reg & BIT(28))
		send_event(SLATECOM_EVENT_ERROR_WRITE_FIFO_ACCESS, NULL);

	if (slav_status_auto_clear_reg & BIT(27))
		send_event(SLATECOM_EVENT_ERROR_READ_FIFO_UNDERRUN, NULL);

	if (slav_status_auto_clear_reg & BIT(26))
		send_event(SLATECOM_EVENT_ERROR_READ_FIFO_BUS_ERR, NULL);

	if (slav_status_auto_clear_reg & BIT(25))
		send_event(SLATECOM_EVENT_ERROR_READ_FIFO_ACCESS, NULL);

	if (slav_status_auto_clear_reg & BIT(24))
		send_event(SLATECOM_EVENT_ERROR_TRUNCATED_READ, NULL);

	if (slav_status_auto_clear_reg & BIT(23))
		send_event(SLATECOM_EVENT_ERROR_TRUNCATED_WRITE, NULL);

	if (slav_status_auto_clear_reg & BIT(22))
		send_event(SLATECOM_EVENT_ERROR_AHB_ILLEGAL_ADDRESS, NULL);

	if (slav_status_auto_clear_reg & BIT(21))
		send_event(SLATECOM_EVENT_ERROR_AHB_BUS_ERR, NULL);

	/* check if SLATE status is changed */
	if (g_slav_status_reg ^ slav_status_reg) {
		if (slav_status_reg & BIT(30)) {
			event_data.application_running = true;
			send_event(SLATECOM_EVENT_APPLICATION_RUNNING,
				&event_data);
		}

		if (slav_status_reg & BIT(29)) {
			event_data.to_slave_fifo_ready = true;
			send_event(SLATECOM_EVENT_TO_SLAVE_FIFO_READY,
				&event_data);
		}

		if (slav_status_reg & BIT(28)) {
			event_data.to_master_fifo_ready = true;
			send_event(SLATECOM_EVENT_TO_MASTER_FIFO_READY,
				&event_data);
		}

		if (slav_status_reg & BIT(27)) {
			event_data.ahb_ready = true;
			send_event(SLATECOM_EVENT_AHB_READY,
				&event_data);
		}

		if (slav_status_reg & BIT(26)) {
			pr_debug("Slate DSP DOWN\n", __func__);
			set_slate_dsp_state(false);
		} else if (slav_status_reg & BIT(30)) {
			pr_debug("Slate DSP UP\n", __func__);
			set_slate_dsp_state(true);
		}

		if (slav_status_reg & BIT(25)) {
			pr_debug("Slate BT DOWN\n", __func__);
			set_slate_bt_state(false);
		} else if (slav_status_reg & BIT(30)) {
			pr_debug("Slate BT UP\n", __func__);
			set_slate_bt_state(true);
		}
	}

	if (master_fifo_used > 0) {
		ptr = kzalloc(master_fifo_used*SLATE_SPI_WORD_SIZE + 1,
			GFP_KERNEL | GFP_ATOMIC);
		if (ptr != NULL) {
			ret = read_slate_locl(SLATECOM_READ_FIFO,
				master_fifo_used,  ptr);
			if (!ret) {
				augmnt_fifo((uint8_t *)ptr,
					master_fifo_used*SLATE_SPI_WORD_SIZE);
				parse_fifo((uint8_t *)ptr, &event_data);
			}
			kfree(ptr);
		}
	}

	event_data.to_slave_fifo_free = slave_fifo_free;
	send_event(SLATECOM_EVENT_TO_SLAVE_FIFO_FREE, &event_data);
}

static void slate_irq_tasklet_hndlr_l(void)
{
	uint32_t slave_status_reg;
	uint32_t glink_isr_reg;
	uint32_t slav_status_auto_clear_reg;
	uint32_t fifo_fill_reg;
	uint32_t fifo_size_reg;
	int ret =  0;
	uint32_t irq_buf[5] = {0};

	ret = read_slate_locl(SLATECOM_READ_REG, 5, &irq_buf[0]);
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

int slatecom_ahb_read(void *handle, uint32_t ahb_start_addr,
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
		|| num_words > SLATE_SPI_MAX_WORDS) {
		pr_err("Invalid param\n");
		return -EINVAL;
	}
	if (!is_slatecom_ready())
		return -ENODEV;

	if (spi_state == SLATECOM_SPI_BUSY) {
		pr_err("Device busy\n");
		return -EBUSY;
	}

	if (slatecom_resume(handle)) {
		pr_err("Failed to resume\n");
		return -EBUSY;
	}

	size = num_words*SLATE_SPI_WORD_SIZE;
	txn_len = SLATE_SPI_AHB_READ_CMD_LEN + size;

	tx_buf = kzalloc(txn_len, GFP_KERNEL | GFP_ATOMIC);
	if (!tx_buf)
		return -ENOMEM;

	rx_buf = kzalloc(txn_len, GFP_KERNEL | GFP_ATOMIC);
	if (!rx_buf) {
		kfree(tx_buf);
		return -ENOMEM;
	}

	cmnd |= SLATE_SPI_AHB_READ_CMD;
	ahb_addr |= ahb_start_addr;

	memcpy(tx_buf, &cmnd, sizeof(cmnd));
	memcpy(tx_buf+sizeof(cmnd), &ahb_addr, sizeof(ahb_addr));

	ret = slatecom_transfer(handle, tx_buf, rx_buf, txn_len);

	if (!ret)
		memcpy(read_buf, rx_buf+SLATE_SPI_AHB_READ_CMD_LEN, size);

	kfree(tx_buf);
	kfree(rx_buf);
	return ret;
}
EXPORT_SYMBOL(slatecom_ahb_read);

int slatecom_ahb_write(void *handle, uint32_t ahb_start_addr,
	uint32_t num_words, void *write_buf)
{
	dma_addr_t dma_hndl;
	uint32_t txn_len;
	uint8_t *tx_buf;
	uint32_t size;
	int ret;
	bool is_cma_used = false;
	uint8_t cmnd = 0;
	uint32_t ahb_addr = 0;
	struct spi_device *spi = get_spi_device();

	if (!handle || !write_buf || num_words == 0
		|| num_words > SLATE_SPI_MAX_WORDS) {
		pr_err("Invalid param\n");
		return -EINVAL;
	}

	if (!is_slatecom_ready())
		return -ENODEV;

	if (spi_state == SLATECOM_SPI_BUSY) {
		pr_err("Device busy\n");
		return -EBUSY;
	}

	if (slatecom_resume(handle)) {
		pr_err("Failed to resume\n");
		return -EBUSY;
	}

	mutex_lock(&cma_buffer_lock);
	size = num_words*SLATE_SPI_WORD_SIZE;
	txn_len = SLATE_SPI_AHB_CMD_LEN + size;
	if (fxd_mem_buffer != NULL && txn_len <= CMA_BFFR_POOL_SIZE) {
		memset(fxd_mem_buffer, 0, txn_len);
		tx_buf = fxd_mem_buffer;
		is_cma_used = true;
	} else {
		pr_info("DMA memory used for size[%d]\n", txn_len);
		tx_buf = dma_alloc_coherent(&spi->dev, txn_len,
						&dma_hndl, GFP_KERNEL);
	}

	if (!tx_buf) {
		mutex_unlock(&cma_buffer_lock);
		return -ENOMEM;
	}

	cmnd |= SLATE_SPI_AHB_WRITE_CMD;
	ahb_addr |= ahb_start_addr;

	memcpy(tx_buf, &cmnd, sizeof(cmnd));
	memcpy(tx_buf+sizeof(cmnd), &ahb_addr, sizeof(ahb_addr));
	memcpy(tx_buf+SLATE_SPI_AHB_CMD_LEN, write_buf, size);

	ret = slatecom_transfer(handle, tx_buf, NULL, txn_len);
	if (!is_cma_used)
		dma_free_coherent(&spi->dev, txn_len, tx_buf, dma_hndl);
	mutex_unlock(&cma_buffer_lock);
	return ret;
}
EXPORT_SYMBOL(slatecom_ahb_write);

int slatecom_fifo_write(void *handle, uint32_t num_words,
	void  *write_buf)
{
	uint32_t txn_len;
	uint8_t *tx_buf;
	uint32_t size;
	int ret;
	uint8_t cmnd = 0;

	if (!handle || !write_buf || num_words == 0
		|| num_words > SLATE_SPI_MAX_WORDS) {
		pr_err("Invalid param\n");
		return -EINVAL;
	}

	if (!is_slatecom_ready())
		return -ENODEV;

	if (spi_state == SLATECOM_SPI_BUSY) {
		pr_err("Device busy\n");
		return -EBUSY;
	}

	if (slatecom_resume(handle)) {
		pr_err("Failed to resume\n");
		return -EBUSY;
	}

	size = num_words*SLATE_SPI_WORD_SIZE;
	txn_len = SLATE_SPI_WRITE_CMND_LEN + size;

	tx_buf = kzalloc(txn_len, GFP_KERNEL | GFP_ATOMIC);

	if (!tx_buf)
		return -ENOMEM;

	cmnd |= SLATE_SPI_FIFO_WRITE_CMD;
	memcpy(tx_buf, &cmnd, sizeof(cmnd));
	memcpy(tx_buf+sizeof(cmnd), write_buf, size);

	ret = slatecom_transfer(handle, tx_buf, NULL, txn_len);
	kfree(tx_buf);
	return ret;
}
EXPORT_SYMBOL(slatecom_fifo_write);

int slatecom_fifo_read(void *handle, uint32_t num_words,
	void *read_buf)
{
	uint32_t txn_len;
	uint8_t *tx_buf;
	uint8_t *rx_buf;
	uint32_t size;
	uint8_t cmnd = 0;
	int ret =  0;

	if (!handle || !read_buf || num_words == 0
		|| num_words > SLATE_SPI_MAX_WORDS) {
		pr_err("Invalid param\n");
		return -EINVAL;
	}

	if (!is_slatecom_ready())
		return -ENODEV;

	if (spi_state == SLATECOM_SPI_BUSY) {
		pr_err("Device busy\n");
		return -EBUSY;
	}

	if (slatecom_resume(handle)) {
		pr_err("Failed to resume\n");
		return -EBUSY;
	}

	size = num_words*SLATE_SPI_WORD_SIZE;
	txn_len = SLATE_SPI_READ_LEN + size;
	tx_buf = kzalloc(txn_len, GFP_KERNEL | GFP_ATOMIC);

	if (!tx_buf)
		return -ENOMEM;

	rx_buf = kzalloc(txn_len, GFP_KERNEL | GFP_ATOMIC);

	if (!rx_buf) {
		kfree(tx_buf);
		return -ENOMEM;
	}

	cmnd |= SLATE_SPI_FIFO_READ_CMD;
	memcpy(tx_buf, &cmnd, sizeof(cmnd));

	ret = slatecom_transfer(handle, tx_buf, rx_buf, txn_len);

	if (!ret)
		memcpy(read_buf, rx_buf+SLATE_SPI_READ_LEN, size);
	kfree(tx_buf);
	kfree(rx_buf);
	return ret;
}
EXPORT_SYMBOL(slatecom_fifo_read);

int slatecom_reg_write(void *handle, uint8_t reg_start_addr,
	uint8_t num_regs, void *write_buf)
{
	uint32_t txn_len;
	uint8_t *tx_buf;
	uint32_t size;
	uint8_t cmnd = 0;
	int ret =  0;

	if (!handle || !write_buf || num_regs == 0
		|| num_regs > SLATE_SPI_MAX_REGS) {
		pr_err("Invalid param\n");
		return -EINVAL;
	}

	if (!is_slatecom_ready())
		return -ENODEV;

	if (spi_state == SLATECOM_SPI_BUSY) {
		pr_err("Device busy\n");
		return -EBUSY;
	}

	if (slatecom_resume(handle)) {
		pr_err("Failed to resume\n");
		return -EBUSY;
	}

	size = num_regs*SLATE_SPI_WORD_SIZE;
	txn_len = SLATE_SPI_WRITE_CMND_LEN + size;

	tx_buf = kzalloc(txn_len, GFP_KERNEL);

	if (!tx_buf)
		return -ENOMEM;

	cmnd |= reg_start_addr;
	memcpy(tx_buf, &cmnd, sizeof(cmnd));
	memcpy(tx_buf+sizeof(cmnd), write_buf, size);

	ret = slatecom_transfer(handle, tx_buf, NULL, txn_len);
	kfree(tx_buf);
	return ret;
}
EXPORT_SYMBOL(slatecom_reg_write);

int slatecom_reg_read(void *handle, uint8_t reg_start_addr,
	uint32_t num_regs, void *read_buf)
{
	uint32_t txn_len;
	uint8_t *tx_buf;
	uint8_t *rx_buf;
	uint32_t size;
	int ret;
	uint8_t cmnd = 0;

	if (!handle || !read_buf || num_regs == 0
		|| num_regs > SLATE_SPI_MAX_REGS) {
		pr_err("Invalid param\n");
		return -EINVAL;
	}

	if (!is_slatecom_ready())
		return -ENODEV;

	if (spi_state == SLATECOM_SPI_BUSY) {
		pr_err("Device busy\n");
		return -EBUSY;
	}

	size = num_regs*SLATE_SPI_WORD_SIZE;
	txn_len = SLATE_SPI_READ_LEN + size;

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

	ret = slatecom_transfer(handle, tx_buf, rx_buf, txn_len);

	if (!ret)
		memcpy(read_buf, rx_buf+SLATE_SPI_READ_LEN, size);
	kfree(tx_buf);
	kfree(rx_buf);
	return ret;
}
EXPORT_SYMBOL(slatecom_reg_read);

static int is_slate_resume(void *handle)
{
	uint32_t txn_len;
	int ret;
	uint8_t tx_buf[8] = {0};
	uint8_t rx_buf[8] = {0};
	uint32_t cmnd_reg = 0;

	if (spi_state == SLATECOM_SPI_BUSY) {
		printk_ratelimited("SPI is held by TZ\n");
		goto ret_err;
	}

	txn_len = 0x08;
	tx_buf[0] = 0x05;
	ret = slatecom_transfer(handle, tx_buf, rx_buf, txn_len);
	if (!ret)
		memcpy(&cmnd_reg, rx_buf+SLATE_SPI_READ_LEN, 0x04);

ret_err:
	return cmnd_reg & BIT(31);
}

int slatecom_resume(void *handle)
{
	struct slate_spi_priv *slate_spi;
	struct slate_context *cntx;
	int retry = 0;

	if (handle == NULL)
		return -EINVAL;

	if (!atomic_read(&slate_is_spi_active))
		return -ECANCELED;

	cntx = (struct slate_context *)handle;

	/* if client is outside slatecom scope and
	 * handle is provided before SLATECOM probed
	 */
	if (cntx->state == SLATECOM_PROB_WAIT) {
		pr_info("handle is provided before SLATECOM probed\n");
		if (!is_slatecom_ready())
			return -EAGAIN;
		cntx->slate_spi = container_of(slate_com_drv,
						struct slate_spi_priv, lhandle);
		cntx->state = SLATECOM_PROB_SUCCESS;
	}

	slate_spi = cntx->slate_spi;

	mutex_lock(&slate_resume_mutex);
	if (slate_spi->slate_state == SLATECOM_STATE_ACTIVE)
		goto unlock;
	enable_irq(slate_irq);
	do {
		if (is_slate_resume(handle)) {
			slate_spi->slate_state = SLATECOM_STATE_ACTIVE;
			break;
		}
		udelay(1000);
		++retry;
	} while (retry < MAX_RETRY);

unlock:
	mutex_unlock(&slate_resume_mutex);
	if (retry == MAX_RETRY) {
		/* SLATE failed to resume. Trigger SLATE soft reset. */
		pr_err("SLATE failed to resume\n");
		pr_err("%s: gpio#95 value is: %d\n",
				__func__, gpio_get_value(95));
		pr_err("%s: gpio#97 value is: %d\n",
				__func__, gpio_get_value(97));
		BUG();
		//bg_soft_reset();
		return -ETIMEDOUT;
	}
	return 0;
}
EXPORT_SYMBOL(slatecom_resume);

int slatecom_suspend(void *handle)
{
	if (!handle)
		return -EINVAL;
	return 0;
}
EXPORT_SYMBOL(slatecom_suspend);

void *slatecom_open(struct slatecom_open_config_type *open_config)
{
	struct slate_spi_priv *spi;
	struct cb_data *irq_notification;
	struct slate_context  *clnt_handle =
			kzalloc(sizeof(*clnt_handle), GFP_KERNEL);

	if (!clnt_handle)
		return NULL;

	/* Client handle Set-up */
	if (!is_slatecom_ready()) {
		clnt_handle->slate_spi = NULL;
		clnt_handle->state = SLATECOM_PROB_WAIT;
	} else {
		spi = container_of(slate_com_drv, struct slate_spi_priv, lhandle);
		clnt_handle->slate_spi = spi;
		clnt_handle->state = SLATECOM_PROB_SUCCESS;
	}
	clnt_handle->cb = NULL;
	/* Interrupt callback Set-up */
	if (open_config && open_config->slatecom_notification_cb) {
		irq_notification = kzalloc(sizeof(*irq_notification),
			GFP_KERNEL);
		if (!irq_notification)
			goto error_ret;

		/* set irq node */
		irq_notification->handle = clnt_handle;
		irq_notification->priv = open_config->priv;
		irq_notification->slatecom_notification_cb =
					open_config->slatecom_notification_cb;
		add_to_irq_list(irq_notification);
		clnt_handle->cb = irq_notification;
	}
	return clnt_handle;

error_ret:
	kfree(clnt_handle);
	return NULL;
}
EXPORT_SYMBOL(slatecom_open);

int slatecom_close(void **handle)
{
	struct slate_context *lhandle;
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
EXPORT_SYMBOL(slatecom_close);

static irqreturn_t slate_irq_tasklet_hndlr(int irq, void *device)
{
	struct slate_spi_priv *slate_spi = device;

	/* check if call-back exists */
	if (!atomic_read(&slate_is_spi_active)) {
		pr_debug("Interrupt received in suspend state\n");
		return IRQ_HANDLED;
	} else if (list_empty(&cb_head)) {
		pr_debug("No callback registered\n");
		return IRQ_HANDLED;
	} else if (spi_state == SLATECOM_SPI_BUSY) {
		/* delay for SPI to be freed */
		msleep(50);
		return IRQ_HANDLED;
	} else if (!slate_spi->irq_lock) {
		slate_spi->irq_lock = 1;
		slate_irq_tasklet_hndlr_l();
		slate_spi->irq_lock = 0;
	}
	return IRQ_HANDLED;
}

static void slate_spi_init(struct slate_spi_priv *slate_spi)
{
	if (!slate_spi) {
		pr_err("device not found\n");
		return;
	}

	/* SLATECOM SPI set-up */
	mutex_init(&slate_spi->xfer_mutex);
	spi_message_init(&slate_spi->msg1);
	spi_message_add_tail(&slate_spi->xfer1, &slate_spi->msg1);

	/* SLATECOM IRQ set-up */
	slate_spi->irq_lock = 0;

	spi_state = SLATECOM_SPI_FREE;

	wq = create_singlethread_workqueue("input_wq");

	slate_spi->slate_state = SLATECOM_STATE_ACTIVE;

	slate_com_drv = &slate_spi->lhandle;

	mutex_init(&slate_resume_mutex);

	fxd_mem_buffer = kmalloc(CMA_BFFR_POOL_SIZE, GFP_KERNEL | GFP_ATOMIC);

	mutex_init(&cma_buffer_lock);
}

static int slate_spi_probe(struct spi_device *spi)
{
	struct slate_spi_priv *slate_spi;
	struct device_node *node;
	int irq_gpio = 0;
	int ret;

	slate_spi = devm_kzalloc(&spi->dev, sizeof(*slate_spi),
				   GFP_KERNEL | GFP_ATOMIC);

	pr_info("%s started\n", __func__);

	if (!slate_spi)
		return -ENOMEM;
	slate_spi->spi = spi;
	spi_set_drvdata(spi, slate_spi);
	slate_spi_init(slate_spi);

	/* SLATECOM Interrupt probe */
	node = spi->dev.of_node;
	irq_gpio = of_get_named_gpio(node, "qcom,irq-gpio", 0);
	if (!gpio_is_valid(irq_gpio)) {
		pr_err("gpio %d found is not valid\n", irq_gpio);
		goto err_ret;
	}

	ret = gpio_request(irq_gpio, "slatecom_gpio");
	if (ret) {
		pr_err("gpio %d request failed\n", irq_gpio);
		goto err_ret;
	}

	ret = gpio_direction_input(irq_gpio);
	if (ret) {
		pr_err("gpio_direction_input not set: %d\n", ret);
		goto err_ret;
	}

	slate_irq = gpio_to_irq(irq_gpio);
	ret = request_threaded_irq(slate_irq, NULL, slate_irq_tasklet_hndlr,
		IRQF_TRIGGER_HIGH | IRQF_ONESHOT, "qcom-slate_spi", slate_spi);

	if (ret)
		goto err_ret;

	atomic_set(&slate_is_spi_active, 1);
	dma_set_coherent_mask(&spi->dev, DMA_BIT_MASK(64));
	pr_info("%s success\n", __func__);
	pr_info("Bgcom Probed successfully\n");
	return ret;

err_ret:
	slate_com_drv = NULL;
	mutex_destroy(&slate_spi->xfer_mutex);
	spi_set_drvdata(spi, NULL);
	return -ENODEV;
}

static int slate_spi_remove(struct spi_device *spi)
{
	struct slate_spi_priv *slate_spi = spi_get_drvdata(spi);

	slate_com_drv = NULL;
	mutex_destroy(&slate_spi->xfer_mutex);
	spi_set_drvdata(spi, NULL);
	if (fxd_mem_buffer != NULL)
		kfree(fxd_mem_buffer);
	mutex_destroy(&cma_buffer_lock);
	return 0;
}

static void slate_spi_shutdown(struct spi_device *spi)
{
	slate_spi_remove(spi);
}

static int slatecom_pm_suspend(struct device *dev)
{
	uint32_t cmnd_reg = 0;
	struct spi_device *s_dev = to_spi_device(dev);
	struct slate_spi_priv *slate_spi = spi_get_drvdata(s_dev);
	int ret = 0;

	if (slate_spi->slate_state == SLATECOM_STATE_SUSPEND)
		return 0;

	cmnd_reg |= BIT(31);
	ret = read_slate_locl(SLATECOM_WRITE_REG, 1, &cmnd_reg);
	if (ret == 0) {
		slate_spi->slate_state = SLATECOM_STATE_SUSPEND;
		atomic_set(&slate_is_spi_active, 0);
		disable_irq(slate_irq);
	}
	pr_info("suspended with : %d\n", ret);
	return ret;
}

static int slatecom_pm_resume(struct device *dev)
{
	struct slate_context clnt_handle;
	int ret;
	struct slate_spi_priv *spi =
		container_of(slate_com_drv, struct slate_spi_priv, lhandle);

	clnt_handle.slate_spi = spi;
	atomic_set(&slate_is_spi_active, 1);
	ret = slatecom_resume(&clnt_handle);
	pr_info("Bgcom resumed with : %d\n", ret);
	return ret;
}

static const struct dev_pm_ops slatecom_pm = {
	.suspend = slatecom_pm_suspend,
	.resume = slatecom_pm_resume,
};

static const struct of_device_id slate_spi_of_match[] = {
	{ .compatible = "qcom,slate-spi", },
	{ }
};
MODULE_DEVICE_TABLE(of, slate_spi_of_match);

static struct spi_driver slate_spi_driver = {
	.driver = {
		.name = "slate-spi",
		.of_match_table = slate_spi_of_match,
		.pm = &slatecom_pm,
	},
	.probe = slate_spi_probe,
	.remove = slate_spi_remove,
	.shutdown = slate_spi_shutdown,
};

module_spi_driver(slate_spi_driver);
MODULE_DESCRIPTION("slate SPI driver");
MODULE_LICENSE("GPL v2");
