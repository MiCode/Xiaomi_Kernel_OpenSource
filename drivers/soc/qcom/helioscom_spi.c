// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <linux/ipc_logging.h>
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
#define HELIOS_STATUS_REG (0x01)
#define HELIOS_NON_SEC_REG (0x05)
#define HELIOS_CMND_REG (0x14)
#define HELIOS_STATUS_READ_SIZE (0x07)
#define HELIOS_RESET_BIT BIT(26)
#define HELIOS_SPI_ACCESS_BLOCKED (0xDEADBEEF)
#define HELIOS_SPI_ACCESS_INVALID (0xDEADD00D)
#define HELIOS_AHB_RESUME_REG (0x200E1800)

#define HELIOS_SPI_MAX_WORDS (0x3FFFFFFD)
#define HELIOS_SPI_MAX_REGS (0x0A)
#define HED_EVENT_ID_LEN (0x02)
#define HED_EVENT_SIZE_LEN (0x02)
#define HED_EVENT_DATA_STRT_LEN (0x05)
#define CMA_BFFR_POOL_SIZE (128*1024)

#define HELIOS_OK_SLP_RBSC      BIT(30)
#define HELIOS_OK_SLP_S2R       BIT(31)
#define HELIOS_OK_SLP_S2D      (BIT(31) | BIT(30))

#define WR_PROTOCOL_OVERHEAD              (5)
#define WR_PROTOCOL_OVERHEAD_IN_WORDS     (2)

#define WR_BUF_SIZE_IN_BYTES	CMA_BFFR_POOL_SIZE
#define WR_BUF_SIZE_IN_WORDS	(CMA_BFFR_POOL_SIZE / sizeof(uint32_t))
#define WR_BUF_SIZE_IN_WORDS_FOR_USE   \
		(WR_BUF_SIZE_IN_WORDS - WR_PROTOCOL_OVERHEAD_IN_WORDS)

#define WR_BUF_SIZE_IN_BYTES_FOR_USE	(WR_BUF_SIZE_IN_WORDS_FOR_USE * sizeof(uint32_t))
#define HELIOS_RESUME_IRQ_TIMEOUT 1000
#define HELIOS_SPI_AUTOSUSPEND_TIMEOUT 5000
#define MIN_SLEEP_TIME	5

/* Master_Command[27] */
#define HELIOS_PAUSE_OK		BIT(27)

/* SLAVE_STATUS_AUTO_CLEAR[16:15] */
#define HELIOS_PAUSE_REQ		BIT(15)
#define HELIOS_RESUME_IND	BIT(16)

/* Slave OEM Status */
#define SLAVE_OEM_STATUS_PASS   (0x1)
#define SLAVE_OEM_STATUS_FAIL   (0x2)

#define SPI_FREQ_1MHZ	1000000
#define SPI_FREQ_32MHZ	32000000

/* Define IPC Logging Macros */
#define LOG_PAGES_CNT 2
static void *helioscom_ipc_log;

#define HELIOSCOM_INFO(x, ...)						     \
	ipc_log_string(helioscom_ipc_log, "[%s]: "x, __func__, ##__VA_ARGS__)

#define HELIOSCOM_ERR(x, ...)						     \
do {									     \
	printk_ratelimited("%s[%s]: " x, KERN_ERR, __func__, ##__VA_ARGS__); \
	ipc_log_string(helioscom_ipc_log, "%s[%s]: " x, "", __func__,         \
			##__VA_ARGS__);\
} while (0)

#define POWER_ENABLED 0

enum helioscom_state {
	/*HELIOSCOM Staus ready*/
	HELIOSCOM_PROB_SUCCESS = 0,
	HELIOSCOM_PROB_WAIT = 1,
	HELIOSCOM_STATE_SUSPEND = 2,
	HELIOSCOM_STATE_ACTIVE = 3,
	HELIOSCOM_STATE_RUNTIME_SUSPEND = 4,
	HELIOSCOM_STATE_HIBERNATE = 5,
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
	atomic_t irq_lock;

};

struct cb_data {
	void *priv;
	void *handle;
	void (*helioscom_notification_cb)(void *handle, void *priv,
		enum helioscom_event_type event,
		union helioscom_event_data_type *event_data);
	struct list_head list;
};

struct cb_reset_data {
	void *priv;
	void *handle;
	void (*helioscom_reset_notification_cb)(void *handle, void *priv,
			enum helioscom_reset_type reset_type);
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

static struct cb_reset_data *pil_reset_cb;

static struct workqueue_struct *wq;
static DECLARE_WORK(input_work, send_input_events);

static struct mutex helios_resume_mutex;
static struct mutex helios_task_mutex;

static atomic_t  helios_is_runtime_suspend;
static atomic_t  helios_is_spi_active;
static atomic_t  ok_to_sleep;
static atomic_t  state;
static uint32_t irq_gpio;
static uint32_t helios_irq;
static uint32_t helios_irq_gpio;
static uint32_t helios_spi_freq = SPI_FREQ_32MHZ;

static uint8_t *fxd_mem_buffer;
static struct mutex cma_buffer_lock;
static ktime_t sleep_time_start;

static DECLARE_COMPLETION(helios_resume_wait);
static int helioscom_reg_write_cmd(void *handle, uint8_t reg_start_addr,
					uint8_t num_regs, void *write_buf);

static void helioscom_interrupt_release(struct helios_spi_priv *helios_spi);
static int helioscom_interrupt_acquire(struct helios_spi_priv *helios_spi);

static struct spi_device *get_spi_device(void)
{
	struct helios_spi_priv *helios_spi = container_of(helios_com_drv,
						struct helios_spi_priv, lhandle);
	struct spi_device *spi = helios_spi->spi;
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

static int send_helios_reset_notification(enum helioscom_reset_type reset_type)
{
	HELIOSCOM_ERR("%s Helios reset received type:[%d]\n", __func__, reset_type);
	if (!pil_reset_cb) {
		HELIOSCOM_ERR("%s PIL call back not registered\n", __func__);
		return -EINVAL;
	}
	pil_reset_cb->helioscom_reset_notification_cb(pil_reset_cb->handle,
			pil_reset_cb->priv, reset_type);
	HELIOSCOM_ERR("%s Helios reset notification:[%d] sent to PIL\n", __func__,
		reset_type);
	return 0;
}

int helioscom_set_spi_state(enum helioscom_spi_state state)
{
	struct helios_spi_priv *helios_spi = container_of(helios_com_drv,
						struct helios_spi_priv, lhandle);
	const struct device spi_dev = helios_spi->spi->master->dev;
	ktime_t time_start, delta;
	s64 time_elapsed;

	if (state == HELIOSCOM_SPI_BUSY) {
		HELIOSCOM_INFO("%s: Release interrupt line.\n", __func__);
		helioscom_interrupt_release(helios_spi);
	}

	if (state == HELIOSCOM_SPI_FREE) {
		HELIOSCOM_INFO("%s: Acquire interrupt line.\n", __func__);
		if (helioscom_interrupt_acquire(helios_spi) != 0) {
			HELIOSCOM_ERR("%s FAILED to get interrupt....\n", __func__);
			return -EINVAL;
		}
	}

	if (state < 0 || state > 1) {
		HELIOSCOM_ERR("Invalid spi state. Returning %d\n", -EINVAL);
		return -EINVAL;
	}

	if (state == spi_state) {
		HELIOSCOM_ERR("state same as spi_state. Returning 0\n");
		return 0;
	}

	mutex_lock(&helios_spi->xfer_mutex);
	if (state == HELIOSCOM_SPI_BUSY) {
		time_start = ktime_get();
		while (!pm_runtime_status_suspended(spi_dev.parent)) {
			delta = ktime_sub(ktime_get(), time_start);
			time_elapsed = ktime_to_ms(delta);
			WARN_ON(time_elapsed > 5 * MSEC_PER_SEC);
			HELIOSCOM_INFO("Waiting to set state busy....\n");
			msleep(100);
		}
	}

	spi_state = state;
	HELIOSCOM_INFO("state = %d\n", state);
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
	uint8_t *rx_buf, uint32_t txn_len, uint32_t freq)
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

	if (!atomic_read(&helios_is_spi_active)) {
		HELIOSCOM_ERR("helioscom is inactive\n");
		return -ECANCELED;
	}

	mutex_lock(&helios_spi->xfer_mutex);
	helios_spi_reinit_xfer(tx_xfer);
	tx_xfer->tx_buf = tx_buf;
	if (rx_buf)
		tx_xfer->rx_buf = rx_buf;

	tx_xfer->len = txn_len;
	HELIOSCOM_INFO("txn_len = %d\n", txn_len);
	tx_xfer->speed_hz = freq;
	if (spi_state == HELIOSCOM_SPI_BUSY) {
		HELIOSCOM_ERR("SPI is held by TZ, skip spi_sync\n");
		mutex_unlock(&helios_spi->xfer_mutex);
		return -EBUSY;
	}
	ret = spi_sync(spi, &helios_spi->msg1);
	mutex_unlock(&helios_spi->xfer_mutex);

	if (ret)
		HELIOSCOM_ERR("SPI transaction failed: %d\n", ret);
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
	uint32_t oem_provisioning_status;
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
		HELIOSCOM_ERR("Helios status 0x%x\n", slav_status_reg);
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

		oem_provisioning_status = slav_status_reg & (BIT(23) | BIT(24));
		oem_provisioning_status = ((oem_provisioning_status<<7)>>30);
		HELIOSCOM_ERR("Helios OEM prov. status 0x%x\n", oem_provisioning_status);
		if (oem_provisioning_status == SLAVE_OEM_STATUS_PASS)
			send_helios_reset_notification(HELIOSCOM_OEM_PROV_PASS);
		else if (oem_provisioning_status == SLAVE_OEM_STATUS_FAIL)
			send_helios_reset_notification(HELIOSCOM_OEM_PROV_FAIL);
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
	uint32_t slave_to_host_cmd;
	uint32_t slave_to_host_data;
	uint32_t slave_status_reg;
	uint32_t glink_isr_reg;
	uint32_t slav_status_auto_clear_reg;
	uint32_t fifo_fill_reg;
	uint32_t fifo_size_reg;
	int ret =  0;
	uint32_t irq_buf[HELIOS_STATUS_READ_SIZE] = {0};
	uint32_t cmnd_reg = 0;
	struct helios_context clnt_handle;
	struct helios_spi_priv *spi =
			container_of(helios_com_drv, struct helios_spi_priv, lhandle);
	clnt_handle.helios_spi = spi;

	ret = read_helios_locl(HELIOSCOM_READ_REG, HELIOS_STATUS_READ_SIZE, &irq_buf[0]);
	if (ret) {
		HELIOSCOM_ERR("Returning from tasklet handler with value %d\n", ret);
		return;
	}
	/* save current state */
	slave_to_host_cmd = irq_buf[0];
	slave_to_host_data = irq_buf[1];
	slave_status_reg = irq_buf[2];
	glink_isr_reg = irq_buf[3];
	slav_status_auto_clear_reg = irq_buf[4];
	fifo_fill_reg = irq_buf[5];
	fifo_size_reg = irq_buf[6];

	if ((slave_to_host_cmd != HELIOS_SPI_ACCESS_BLOCKED) &&
		(slave_to_host_cmd != HELIOS_SPI_ACCESS_INVALID) &&
		(slave_to_host_cmd & HELIOS_RESET_BIT)) {
		send_helios_reset_notification(HELIOSCOM_HELIOS_CRASH);
		//helioscom_set_spi_state(HELIOSCOM_SPI_BUSY);
		return;
	}

	if (slav_status_auto_clear_reg & HELIOS_PAUSE_REQ) {
		cmnd_reg |= HELIOS_PAUSE_OK;
		ret = helioscom_reg_write_cmd(&clnt_handle, HELIOS_CMND_REG, 1, &cmnd_reg);
		if (ret == 0) {
			spi_state = HELIOSCOM_SPI_PAUSE;
			pr_debug("SPI is in Pause State\n");
		}
	}

	if (slav_status_auto_clear_reg & HELIOS_RESUME_IND) {
		spi_state = HELIOSCOM_SPI_FREE;
		pr_debug("Apps to resume operation\n");
	}
	send_back_notification(slave_status_reg,
		slav_status_auto_clear_reg, fifo_fill_reg, fifo_size_reg);

	g_slav_status_reg = slave_status_reg;
}

static int helioscom_suspend_l(void *handle)
{
	struct helios_context *cntx;
	int ret = 0;
	uint32_t cmnd_reg = 0;
	struct spi_device *spi = get_spi_device();

	if (handle == NULL)
		return -EINVAL;

	cntx = (struct helios_context *)handle;

	/* if client is outside helioscom scope and
	 * handle is provided before HELIOSCOM probed
	 */
	if (cntx->state == HELIOSCOM_PROB_WAIT) {
		HELIOSCOM_INFO("handle is provided before HELIOSCOM probed\n");
		if (!is_helioscom_ready())
			return -EAGAIN;
		cntx->helios_spi = container_of(helios_com_drv,
						struct helios_spi_priv, lhandle);
		cntx->state = HELIOSCOM_PROB_SUCCESS;
	}

	if (!(g_slav_status_reg & BIT(31))) {
		HELIOSCOM_ERR("Helios boot is not complete, skip SPI suspend\n");
		return 0;
	}

	if (atomic_read(&state) == HELIOSCOM_STATE_SUSPEND)
		return 0;

	cmnd_reg |= HELIOS_OK_SLP_RBSC;

	(!atomic_read(&helios_is_spi_active)) ? pm_runtime_get_sync(&spi->dev)
			: HELIOSCOM_INFO("spi is already active, skip get_sync...\n");

	ret = helioscom_reg_write_cmd(cntx, HELIOS_CMND_REG, 1, &cmnd_reg);

	(!atomic_read(&helios_is_spi_active)) ? pm_runtime_put_sync(&spi->dev)
			: HELIOSCOM_INFO("spi is already active, skip put_sync...\n");

	sleep_time_start = ktime_get();

	HELIOSCOM_INFO("reg write status: %d\n", ret);

	atomic_set(&state, HELIOSCOM_STATE_SUSPEND);
	atomic_set(&helios_is_spi_active, 0);
	atomic_set(&helios_is_runtime_suspend, 0);
	atomic_set(&ok_to_sleep, 1);

	HELIOSCOM_INFO("suspended\n");
	return ret;
}

/* Returns 1, if the helios spi is active */
static int is_helios_resume(void *handle)
{
	uint32_t txn_len;
	int ret;
	uint8_t *tx_buf = NULL;
	uint8_t *rx_buf = NULL;
	uint32_t cmnd_reg = 0;
	uint8_t *tx_ahb_buf = NULL;
	uint8_t *rx_ahb_buf = fxd_mem_buffer;
	uint32_t ahb_addr = HELIOS_AHB_RESUME_REG;
	uint32_t size;
	uint32_t no_of_reg = 1;
	uint8_t cmnd = 0;
	struct helios_spi_priv *helios_spi;
	struct helios_context *cntx;
	ktime_t delta;
	s64 time_elapsed;

	cntx = (struct helios_context *)handle;
	helios_spi = cntx->helios_spi;

	if (spi_state == HELIOSCOM_SPI_BUSY) {
		printk_ratelimited("SPI is held by TZ\n");
		goto ret_err;
	}

	/* require a min time gap between OK_TO_SLEEP message and resume. */
	delta = ktime_sub(ktime_get(), sleep_time_start);
	time_elapsed = ktime_to_ms(delta);
	if (time_elapsed < MIN_SLEEP_TIME) {
		pr_info("avoid aggresive wakeup, sleep for %lu ms\n",
				MIN_SLEEP_TIME - time_elapsed);
		msleep(MIN_SLEEP_TIME - time_elapsed);
	}

	size = no_of_reg*HELIOS_SPI_WORD_SIZE;
	txn_len = HELIOS_SPI_READ_LEN + size;

	tx_buf = kzalloc(txn_len, GFP_KERNEL | GFP_ATOMIC);
	if (!tx_buf) {
		ret = -ENOMEM;
		goto ret_err;
	}

	rx_buf = kzalloc(txn_len, GFP_KERNEL | GFP_ATOMIC);
	if (!rx_buf) {
		kfree(tx_buf);
		ret = -ENOMEM;
		goto ret_err;
	}

	cmnd |= HELIOS_NON_SEC_REG;
	memcpy(tx_buf, &cmnd, sizeof(cmnd));

	ret = helioscom_transfer(handle, tx_buf, rx_buf, txn_len, helios_spi_freq);

	if (!ret)
		memcpy(&cmnd_reg, rx_buf+HELIOS_SPI_READ_LEN, 0x04);

	kfree(tx_buf);
	kfree(rx_buf);

	if (!(cmnd_reg & BIT(31))) {
		pr_err("AHB read to resume\n");

		txn_len = 8;
		tx_ahb_buf = kzalloc(txn_len, GFP_KERNEL | GFP_ATOMIC);
		if (!tx_ahb_buf) {
			ret = -ENOMEM;
			goto ret_err;
		}
		cmnd |= HELIOS_SPI_AHB_READ_CMD;
		memcpy(tx_ahb_buf, &cmnd, sizeof(cmnd));
		memcpy(tx_ahb_buf+sizeof(cmnd), &ahb_addr, sizeof(ahb_addr));

		ret = helioscom_transfer(handle, tx_ahb_buf, rx_ahb_buf, txn_len, SPI_FREQ_1MHZ);
		if (ret)
			pr_err("helioscom_transfer fail with error %d\n", ret);

		kfree(tx_ahb_buf);
	}
ret_err:
	return cmnd_reg & BIT(31);
}
static int helioscom_resume_l(void *handle)
{
	struct helios_spi_priv *helios_spi;
	struct helios_context *cntx;
	int ret = 0;

	if (handle == NULL)
		return -EINVAL;

	if (!atomic_read(&helios_is_spi_active)) {
		HELIOSCOM_ERR("helioscom is inactive. Returning %d\n", -ECANCELED);
		return -ECANCELED;
	}
	cntx = (struct helios_context *)handle;

	/* if client is outside helioscom scope and
	 * handle is provided before HELIOSCOM probed
	 */
	if (cntx->state == HELIOSCOM_PROB_WAIT) {
		HELIOSCOM_INFO("handle is provided before HELIOSCOM probed\n");
		if (!is_helioscom_ready())
			return -EAGAIN;
		cntx->helios_spi = container_of(helios_com_drv,
						struct helios_spi_priv, lhandle);
		cntx->state = HELIOSCOM_PROB_SUCCESS;
	}

	helios_spi = cntx->helios_spi;

	mutex_lock(&helios_resume_mutex);
	if (atomic_read(&state) == HELIOSCOM_STATE_ACTIVE)
		goto unlock;

	if (!(g_slav_status_reg & BIT(31))) {
		HELIOSCOM_ERR("Helios boot is not complete, skip SPI resume\n");
		goto unlock;
	}
	if (!is_helios_resume(handle)) {
		if (atomic_read(&ok_to_sleep)) {
			reinit_completion(&helios_resume_wait);
			ret = wait_for_completion_timeout(
				&helios_resume_wait, msecs_to_jiffies(
					HELIOS_RESUME_IRQ_TIMEOUT));
			if (!ret) {
				HELIOSCOM_ERR("Time out on Helios Resume\n");
				goto error;
			}
		}
	}
	atomic_set(&state, HELIOSCOM_STATE_ACTIVE);
	goto unlock;

error:
	mutex_unlock(&helios_resume_mutex);
	/* HELIOS failed to resume. Trigger watchdog. */
		HELIOSCOM_ERR("HELIOS failed to resume, gpio#30 value is: %d\n",
				gpio_get_value(30));
		BUG();
		return -ETIMEDOUT;

unlock:
	mutex_unlock(&helios_resume_mutex);
	return 0;
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
	struct spi_device *spi = get_spi_device();

	if (!handle || !read_buf || num_words == 0
		|| num_words > HELIOS_SPI_MAX_WORDS) {
		HELIOSCOM_ERR("Invalid param\n");
		return -EINVAL;
	}
	if (!is_helioscom_ready())
		return -ENODEV;

	if (spi_state == HELIOSCOM_SPI_BUSY) {
		HELIOSCOM_ERR("Device busy\n");
		return -EBUSY;
	}

	pm_runtime_get_sync(&spi->dev);
	mutex_lock(&helios_task_mutex);

	size = num_words*HELIOS_SPI_WORD_SIZE;
	txn_len = HELIOS_SPI_AHB_READ_CMD_LEN + size;

	tx_buf = kzalloc(txn_len, GFP_KERNEL | GFP_ATOMIC);
	if (!tx_buf) {
		ret = -ENOMEM;
		goto error_ret;
	}

	rx_buf = kzalloc(txn_len, GFP_KERNEL | GFP_ATOMIC);
	if (!rx_buf) {
		kfree(tx_buf);
		ret = -ENOMEM;
		goto error_ret;
	}

	cmnd |= HELIOS_SPI_AHB_READ_CMD;
	ahb_addr |= ahb_start_addr;

	memcpy(tx_buf, &cmnd, sizeof(cmnd));
	memcpy(tx_buf+sizeof(cmnd), &ahb_addr, sizeof(ahb_addr));

	ret = helioscom_transfer(handle, tx_buf, rx_buf, txn_len, helios_spi_freq);

	if (!ret)
		memcpy(read_buf, rx_buf+HELIOS_SPI_AHB_READ_CMD_LEN, size);

	kfree(tx_buf);
	kfree(rx_buf);

error_ret:
	pm_runtime_mark_last_busy(&spi->dev);
	pm_runtime_put_sync_autosuspend(&spi->dev);
	mutex_unlock(&helios_task_mutex);
	return ret;
}
EXPORT_SYMBOL(helioscom_ahb_read);

int helioscom_ahb_write_bytes(void *handle, uint32_t ahb_start_addr,
	uint32_t num_bytes, void *write_buf)
{
	uint32_t txn_len;
	uint8_t *tx_buf;
	int ret;
	uint8_t cmnd = 0;
	uint32_t ahb_addr = 0;
	uint32_t curr_num_bytes;
	struct spi_device *spi = get_spi_device();

	if (!handle || !write_buf || num_bytes == 0
		|| num_bytes > (HELIOS_SPI_MAX_WORDS * sizeof(int))) {
		HELIOSCOM_ERR("Invalid param\n");
		return -EINVAL;
	}

	if (!is_helioscom_ready())
		return -ENODEV;

	if (spi_state == HELIOSCOM_SPI_BUSY) {
		HELIOSCOM_ERR("Device busy\n");
		return -EBUSY;
	}

	pm_runtime_get_sync(&spi->dev);
	mutex_lock(&helios_task_mutex);

	ahb_addr = ahb_start_addr;

	mutex_lock(&cma_buffer_lock);
	while (num_bytes) {
		curr_num_bytes = (num_bytes < WR_BUF_SIZE_IN_BYTES_FOR_USE) ?
						num_bytes : WR_BUF_SIZE_IN_BYTES_FOR_USE;

		txn_len = HELIOS_SPI_AHB_CMD_LEN + curr_num_bytes;

		if ((txn_len % sizeof(uint32_t)) != 0) {
			txn_len +=
				(sizeof(uint32_t) - (txn_len % sizeof(uint32_t)));
		}
		memset(fxd_mem_buffer, 0, txn_len);
		tx_buf = fxd_mem_buffer;

		cmnd |= HELIOS_SPI_AHB_WRITE_CMD;

		memcpy(tx_buf, &cmnd, sizeof(cmnd));
		memcpy(tx_buf+sizeof(cmnd), &ahb_addr, sizeof(ahb_addr));
		memcpy(tx_buf+HELIOS_SPI_AHB_CMD_LEN, write_buf, curr_num_bytes);

		ret = helioscom_transfer(handle, tx_buf, NULL, txn_len, helios_spi_freq);
		if (ret) {
			HELIOSCOM_ERR("helioscom_transfer fail with error %d\n", ret);
			goto error;
		}
		write_buf += curr_num_bytes;
		ahb_addr += curr_num_bytes;
		num_bytes -= curr_num_bytes;
	}

error:
	mutex_unlock(&cma_buffer_lock);
	pm_runtime_mark_last_busy(&spi->dev);
	pm_runtime_put_sync_autosuspend(&spi->dev);
	mutex_unlock(&helios_task_mutex);
	return ret;
}
EXPORT_SYMBOL(helioscom_ahb_write_bytes);

int helioscom_ahb_write(void *handle, uint32_t ahb_start_addr,
	uint32_t num_words, void *write_buf)
{
	uint32_t txn_len;
	uint8_t *tx_buf;
	int ret;
	uint8_t cmnd = 0;
	uint32_t ahb_addr = 0;
	uint32_t curr_num_words;
	uint32_t curr_num_bytes;
	struct spi_device *spi = get_spi_device();

	if (!handle || !write_buf || num_words == 0
		|| num_words > HELIOS_SPI_MAX_WORDS) {
		HELIOSCOM_ERR("Invalid param\n");
		return -EINVAL;
	}

	if (!is_helioscom_ready())
		return -ENODEV;

	if (spi_state == HELIOSCOM_SPI_BUSY) {
		HELIOSCOM_ERR("Device busy\n");
		return -EBUSY;
	}

	pm_runtime_get_sync(&spi->dev);
	mutex_lock(&helios_task_mutex);

	ahb_addr = ahb_start_addr;

	mutex_lock(&cma_buffer_lock);
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

		ret = helioscom_transfer(handle, tx_buf, NULL, txn_len, helios_spi_freq);
		if (ret) {
			HELIOSCOM_ERR("helioscom_transfer fail with error %d\n", ret);
			goto error;
		}
		write_buf += curr_num_bytes;
		ahb_addr += curr_num_bytes;
		num_words -= curr_num_words;
	}

error:
	mutex_unlock(&cma_buffer_lock);
	pm_runtime_mark_last_busy(&spi->dev);
	pm_runtime_put_sync_autosuspend(&spi->dev);
	mutex_unlock(&helios_task_mutex);
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
	struct spi_device *spi = get_spi_device();

	if (!handle || !write_buf || num_words == 0
		|| num_words > HELIOS_SPI_MAX_WORDS) {
		HELIOSCOM_ERR("Invalid param\n");
		return -EINVAL;
	}

	if (!is_helioscom_ready())
		return -ENODEV;

	if (spi_state == HELIOSCOM_SPI_BUSY) {
		HELIOSCOM_ERR("Device busy\n");
		return -EBUSY;
	}

	pm_runtime_get_sync(&spi->dev);
	mutex_lock(&helios_task_mutex);

	size = num_words*HELIOS_SPI_WORD_SIZE;
	txn_len = HELIOS_SPI_WRITE_CMND_LEN + size;

	tx_buf = kzalloc(txn_len, GFP_KERNEL | GFP_ATOMIC);
	if (!tx_buf) {
		ret = -ENOMEM;
		goto error_ret;
	}

	cmnd |= HELIOS_SPI_FIFO_WRITE_CMD;
	memcpy(tx_buf, &cmnd, sizeof(cmnd));
	memcpy(tx_buf+sizeof(cmnd), write_buf, size);

	ret = helioscom_transfer(handle, tx_buf, NULL, txn_len, helios_spi_freq);
	kfree(tx_buf);

error_ret:
	pm_runtime_mark_last_busy(&spi->dev);
	pm_runtime_put_sync_autosuspend(&spi->dev);
	mutex_unlock(&helios_task_mutex);
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
	struct spi_device *spi = get_spi_device();

	if (!handle || !read_buf || num_words == 0
		|| num_words > HELIOS_SPI_MAX_WORDS) {
		HELIOSCOM_ERR("Invalid param\n");
		return -EINVAL;
	}

	if (!is_helioscom_ready())
		return -ENODEV;

	if (spi_state == HELIOSCOM_SPI_BUSY) {
		HELIOSCOM_ERR("Device busy\n");
		return -EBUSY;
	}

	pm_runtime_get_sync(&spi->dev);
	mutex_lock(&helios_task_mutex);

	size = num_words*HELIOS_SPI_WORD_SIZE;
	txn_len = HELIOS_SPI_READ_LEN + size;
	tx_buf = kzalloc(txn_len, GFP_KERNEL | GFP_ATOMIC);
	if (!tx_buf) {
		ret = -ENOMEM;
		goto error_ret;
	}

	rx_buf = kzalloc(txn_len, GFP_KERNEL | GFP_ATOMIC);
	if (!rx_buf) {
		kfree(tx_buf);
		ret = -ENOMEM;
		goto error_ret;
	}

	cmnd |= HELIOS_SPI_FIFO_READ_CMD;
	memcpy(tx_buf, &cmnd, sizeof(cmnd));

	ret = helioscom_transfer(handle, tx_buf, rx_buf, txn_len, helios_spi_freq);

	if (!ret)
		memcpy(read_buf, rx_buf+HELIOS_SPI_READ_LEN, size);
	kfree(tx_buf);
	kfree(rx_buf);

error_ret:
	pm_runtime_mark_last_busy(&spi->dev);
	pm_runtime_put_sync_autosuspend(&spi->dev);
	mutex_unlock(&helios_task_mutex);
	return ret;
}
EXPORT_SYMBOL(helioscom_fifo_read);

static int helioscom_reg_write_cmd(void *handle, uint8_t reg_start_addr,
	uint8_t num_regs, void *write_buf)
{
	uint32_t txn_len;
	uint8_t *tx_buf;
	uint32_t size;
	uint8_t cmnd = 0;
	int ret =  0;

	if (!handle || !write_buf || num_regs == 0
		|| num_regs > HELIOS_SPI_MAX_REGS) {
		HELIOSCOM_ERR("Invalid param\n");
		return -EINVAL;
	}

	if (!is_helioscom_ready())
		return -ENODEV;

	if (spi_state == HELIOSCOM_SPI_BUSY) {
		HELIOSCOM_ERR("Device busy\n");
		return -EBUSY;
	}
	if (spi_state == HELIOSCOM_SPI_PAUSE) {
		HELIOSCOM_ERR("Device in Pause State\n");
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

	ret = helioscom_transfer(handle, tx_buf, NULL, txn_len, helios_spi_freq);
	kfree(tx_buf);
	return ret;
}

int helioscom_reg_write(void *handle, uint8_t reg_start_addr,
	uint8_t num_regs, void *write_buf)
{
	int ret =  0;
	struct spi_device *spi = get_spi_device();

	pm_runtime_get_sync(&spi->dev);
	mutex_lock(&helios_task_mutex);

	ret = helioscom_reg_write_cmd(handle, reg_start_addr,
					num_regs, write_buf);

	pm_runtime_mark_last_busy(&spi->dev);
	pm_runtime_put_sync_autosuspend(&spi->dev);
	mutex_unlock(&helios_task_mutex);
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
	struct spi_device *spi = get_spi_device();

	if (!handle || !read_buf || num_regs == 0
		|| num_regs > HELIOS_SPI_MAX_REGS) {
		HELIOSCOM_ERR("Invalid param\n");
		return -EINVAL;
	}

	if (!is_helioscom_ready())
		return -ENODEV;

	if (spi_state == HELIOSCOM_SPI_BUSY) {
		HELIOSCOM_ERR("Device busy\n");
		return -EBUSY;
	}

	pm_runtime_get_sync(&spi->dev);
	mutex_lock(&helios_task_mutex);

	size = num_regs*HELIOS_SPI_WORD_SIZE;
	txn_len = HELIOS_SPI_READ_LEN + size;

	tx_buf = kzalloc(txn_len, GFP_KERNEL | GFP_ATOMIC);
	if (!tx_buf) {
		ret = -ENOMEM;
		goto error_ret;
	}

	rx_buf = kzalloc(txn_len, GFP_KERNEL | GFP_ATOMIC);
	if (!rx_buf) {
		kfree(tx_buf);
		ret = -ENOMEM;
		goto error_ret;
	}

	cmnd |= reg_start_addr;
	memcpy(tx_buf, &cmnd, sizeof(cmnd));

	ret = helioscom_transfer(handle, tx_buf, rx_buf, txn_len, helios_spi_freq);

	if (!ret)
		memcpy(read_buf, rx_buf+HELIOS_SPI_READ_LEN, size);
	kfree(tx_buf);
	kfree(rx_buf);

error_ret:
	pm_runtime_mark_last_busy(&spi->dev);
	pm_runtime_put_sync_autosuspend(&spi->dev);
	mutex_unlock(&helios_task_mutex);
	return ret;
}
EXPORT_SYMBOL(helioscom_reg_read);

int helioscom_resume(void *handle)
{
	int ret =  0;

	mutex_lock(&helios_task_mutex);

	if (!atomic_read(&helios_is_spi_active)) {
		HELIOSCOM_INFO("Doing force resume\n");
		atomic_set(&helios_is_spi_active, 1);

		ret = helioscom_resume_l(handle);
	}
	mutex_unlock(&helios_task_mutex);
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

void *helioscom_pil_reset_register(struct helioscom_reset_config_type *open_config)
{
	struct helios_spi_priv *spi;
	struct cb_reset_data *irq_notification;
	struct helios_context  *clnt_handle =
			kzalloc(sizeof(*clnt_handle), GFP_KERNEL);

	if (!clnt_handle)
		return NULL;

	/* if call back already register, don't add additional */
	if (pil_reset_cb != NULL) {
		HELIOSCOM_ERR("%s PIL callback already registered\n", __func__);
		goto error_ret;
	}

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
	if (open_config && open_config->helioscom_reset_notification_cb) {
		irq_notification = kzalloc(sizeof(*irq_notification),
			GFP_KERNEL);
		if (!irq_notification)
			goto error_ret;

		/* set irq node */
		irq_notification->handle = clnt_handle;
		irq_notification->priv = open_config->priv;
		irq_notification->helioscom_reset_notification_cb =
					open_config->helioscom_reset_notification_cb;
		pil_reset_cb = irq_notification;
	}
	HELIOSCOM_INFO("%s PIL reset notification callback registered successfully\n", __func__);
	return clnt_handle;

error_ret:
	kfree(clnt_handle);
	return NULL;
}
EXPORT_SYMBOL(helioscom_pil_reset_register);

int helioscom_pil_reset_unregister(void **handle)
{
	if (*handle == NULL)
		return -EINVAL;

	kfree(*handle);
	*handle = NULL;

	kfree(pil_reset_cb);
	pil_reset_cb = NULL;

	return 0;
}
EXPORT_SYMBOL(helioscom_pil_reset_unregister);

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

int get_helios_sleep_state(void)
{
	int ret = atomic_read(&ok_to_sleep);

	HELIOSCOM_INFO("sleep state =%d\n", ret);
	return ret;
}
EXPORT_SYMBOL(get_helios_sleep_state);

int set_helios_sleep_state(bool sleep_state)
{
	struct helios_context clnt_handle;
	int ret = 0;
	struct helios_spi_priv *spi = NULL;

	if (!is_helioscom_ready())
		return -EAGAIN;

	HELIOSCOM_INFO("set helios in sleep state =%d\n", sleep_state);

	spi = container_of(helios_com_drv, struct helios_spi_priv, lhandle);
	clnt_handle.helios_spi = spi;

	if (!(g_slav_status_reg & BIT(31))) {
		HELIOSCOM_ERR("Helios boot is not complete, skip SPI resume\n");
		return 0;
	}

	if (sleep_state) {
		/*send command to helios, helios can go to sleep*/
		helioscom_suspend_l(&clnt_handle);
	} else {
		/* resume helios */
		if (atomic_read(&helios_is_spi_active)) {
			HELIOSCOM_INFO("Helioscom in restore state\n");
		} else {
			atomic_set(&helios_is_spi_active, 1);
			atomic_set(&helios_is_runtime_suspend, 0);
			ret = helioscom_resume_l(&clnt_handle);
			HELIOSCOM_INFO("Helioscom restore with : %d\n", ret);
		}
		return ret;
	}
	return ret;
}
EXPORT_SYMBOL(set_helios_sleep_state);

static irqreturn_t helios_irq_tasklet_hndlr(int irq, void *device)
{
	struct helios_spi_priv *helios_spi = device;
	struct spi_device *spi = get_spi_device();

	/* Once interrupt received. Helios is OUT of sleep */
	complete(&helios_resume_wait);
	atomic_set(&ok_to_sleep, 0);

	/* check if call-back exists */
	if (atomic_read(&helios_is_runtime_suspend)) {
		pr_debug("Interrupt received in suspend state\n");
		atomic_set(&helios_spi->irq_lock, 1);
		pm_runtime_get_sync(&spi->dev);
		helios_irq_tasklet_hndlr_l();
		pm_runtime_mark_last_busy(&spi->dev);
		pm_runtime_put_sync_autosuspend(&spi->dev);
		atomic_set(&helios_spi->irq_lock, 0);
		return IRQ_HANDLED;
	} else if (list_empty(&cb_head) && (!pil_reset_cb)) {
		HELIOSCOM_INFO("No callback registered\n");
		msleep(50);
		return IRQ_HANDLED;
	} else if (spi_state == HELIOSCOM_SPI_BUSY) {
		/* delay for SPI to be freed */
		msleep(50);
		return IRQ_HANDLED;
	} else if (atomic_read(&helios_spi->irq_lock) == 0) {
		atomic_set(&helios_spi->irq_lock, 1);
		helios_irq_tasklet_hndlr_l();
		atomic_set(&helios_spi->irq_lock, 0);
	}
	return IRQ_HANDLED;
}

static int helioscom_interrupt_acquire(struct helios_spi_priv *helios_spi)
{
	int ret;

	ret = gpio_request(irq_gpio, "helioscom_gpio");
	if (ret) {
		pr_err("gpio %d request failed\n", irq_gpio);
		goto err_ret;
	}
	helios_irq_gpio = irq_gpio;
	HELIOSCOM_INFO("gpio %d gpio_request success\n", irq_gpio);
	ret = gpio_direction_input(irq_gpio);
	if (ret) {
		pr_err("gpio_direction_input not set: %d\n", ret);
		goto err_ret;
	}

	helios_irq = gpio_to_irq(irq_gpio);
	ret = request_threaded_irq(helios_irq, NULL, helios_irq_tasklet_hndlr,
		IRQF_TRIGGER_HIGH | IRQF_ONESHOT, "qcom-helios_spi", helios_spi);

	if (ret) {
		pr_err("%s request_threaded_irq registarion failed\n", __func__);
		goto err_ret;
	}

	ret = irq_set_irq_wake(helios_irq, true);
	if (ret) {
		pr_err("irq set as wakeup return: %d\n", ret);
		goto err_ret;
	}

	HELIOSCOM_INFO("%s: IRQ %d irq_request success\n", __func__, helios_irq);
	return 0;

err_ret:
	HELIOSCOM_ERR("%s: Interrupt registration failed.\n", __func__);
	if (helios_irq) {
		HELIOSCOM_INFO("%s freeing  irq = %d\n", __func__, helios_irq);
		free_irq(helios_irq, helios_spi);
		helios_irq = 0;
	}
	if (helios_irq_gpio) {
		HELIOSCOM_INFO("%s freeing  gpio = %d\n", __func__, helios_irq_gpio);
		gpio_free(helios_irq_gpio);
		helios_irq_gpio = 0;
	}
	return -EINVAL;
}

static void helioscom_interrupt_release(struct helios_spi_priv *helios_spi)
{
	if (helios_irq) {
		HELIOSCOM_INFO("%s freeing  irq = %d\n", __func__, helios_irq);
		free_irq(helios_irq, helios_spi);
		helios_irq = 0;
	}
	if (helios_irq_gpio) {
		HELIOSCOM_INFO("%s freeing  gpio = %d\n", __func__, helios_irq_gpio);
		gpio_free(helios_irq_gpio);
		helios_irq_gpio = 0;
	}
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
	atomic_set(&helios_spi->irq_lock, 0);

	wq = create_singlethread_workqueue("input_wq");

	atomic_set(&state, HELIOSCOM_STATE_ACTIVE);

	helios_com_drv = &helios_spi->lhandle;

	mutex_init(&helios_resume_mutex);
	mutex_init(&helios_task_mutex);

	fxd_mem_buffer = kmalloc(CMA_BFFR_POOL_SIZE, GFP_KERNEL | GFP_ATOMIC);

	mutex_init(&cma_buffer_lock);

	helioscom_ipc_log = ipc_log_context_create(LOG_PAGES_CNT, "helioscom_spi", 0);

}

static int helios_spi_probe(struct spi_device *spi)
{
	struct helios_spi_priv *helios_spi;
	struct device_node *node;
	uint32_t value;

	helios_spi = devm_kzalloc(&spi->dev, sizeof(*helios_spi),
				   GFP_KERNEL | GFP_ATOMIC);

	HELIOSCOM_INFO("%s started\n", __func__);

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

	if (!of_property_read_u32(node, "helios-spi-frequency", &value)) {
		helios_spi_freq = value;
		HELIOSCOM_INFO("%s: helios-spi-frequency set to %d\n", __func__, value);
	}

	atomic_set(&helios_is_spi_active, 1);
	dma_set_coherent_mask(&spi->dev, DMA_BIT_MASK(64));

#if POWER_ENABLED
	/* Enable Runtime PM for this device */
	pm_runtime_enable(&spi->dev);
	pm_runtime_set_autosuspend_delay(&spi->dev, HELIOS_SPI_AUTOSUSPEND_TIMEOUT);
	pm_runtime_use_autosuspend(&spi->dev);
#endif
	HELIOSCOM_INFO("%s: Helioscom Probed successfully\n", __func__);
	return 0;

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
#if POWER_ENABLED
	pm_runtime_disable(&spi->dev);
#endif
	mutex_destroy(&helios_spi->xfer_mutex);
	spi_set_drvdata(spi, NULL);
	if (fxd_mem_buffer != NULL)
		kfree(fxd_mem_buffer);
	mutex_destroy(&cma_buffer_lock);
	mutex_destroy(&helios_task_mutex);
	if (helios_irq) {
		pr_info("%s freeing  irq = %d\n", __func__, helios_irq);
		free_irq(helios_irq, helios_spi);
		helios_irq = 0;
	}
	if (helios_irq_gpio) {
		pr_info("%s freeing  gpio = %d\n", __func__, helios_irq_gpio);
		gpio_free(helios_irq_gpio);
		helios_irq_gpio = 0;
	}
	return 0;
}

static void helios_spi_shutdown(struct spi_device *spi)
{
	disable_irq(helios_irq);
	helios_spi_remove(spi);
}

static int helioscom_pm_prepare(struct device *dev)
{
#if POWER_ENABLED
	struct helios_context clnt_handle;
	uint32_t cmnd_reg = 0;
	struct spi_device *s_dev = to_spi_device(dev);
	struct helios_spi_priv *helios_spi = spi_get_drvdata(s_dev);
	int ret = 0;

	clnt_handle.helios_spi = helios_spi;

	if (!(g_slav_status_reg & BIT(31))) {
		HELIOSCOM_ERR("Helios boot is not complete, skip SPI suspend\n");
		return 0;
	}

	cmnd_reg |= HELIOS_OK_SLP_RBSC;

	(!atomic_read(&helios_is_spi_active)) ? pm_runtime_get_sync(&s_dev->dev)
			: HELIOSCOM_INFO("spi is already active, skip get_sync...\n");

	ret = helioscom_reg_write_cmd(&clnt_handle, HELIOS_CMND_REG, 1, &cmnd_reg);

	(!atomic_read(&helios_is_spi_active)) ? pm_runtime_put_sync(&s_dev->dev)
			: HELIOSCOM_INFO("spi is already active, skip put_sync...\n");

	sleep_time_start = ktime_get();

	HELIOSCOM_INFO("reg write status: %d\n", ret);
	return ret;
#endif
	return 0;
}

static void helioscom_pm_complete(struct device *dev)
{
	/* nothing to do */
}

static int helioscom_pm_suspend(struct device *dev)
{
#if POWER_ENABLED
	if (atomic_read(&state) == HELIOSCOM_STATE_SUSPEND)
		return 0;

	if (!(g_slav_status_reg & BIT(31))) {
		HELIOSCOM_ERR("Helios boot is not complete, skip SPI suspend\n");
		return 0;
	}

	atomic_set(&state, HELIOSCOM_STATE_SUSPEND);
	atomic_set(&helios_is_spi_active, 0);
	atomic_set(&helios_is_runtime_suspend, 0);
	atomic_set(&ok_to_sleep, 1);

	HELIOSCOM_INFO("suspended\n");
	return 0;
#endif
	return 0;
}

static int helioscom_pm_resume(struct device *dev)
{
#if POWER_ENABLED
	struct helios_context clnt_handle;
	int ret;
	struct helios_spi_priv *spi =
		container_of(helios_com_drv, struct helios_spi_priv, lhandle);

	if (atomic_read(&spi->irq_lock) == 1) {
		atomic_set(&helios_is_spi_active, 1);
		atomic_set(&helios_is_runtime_suspend, 0);
		atomic_set(&state, HELIOSCOM_STATE_ACTIVE);
		pr_debug("Shouldn't Execute\n");
		return 0;
	}

	if (atomic_read(&helios_is_spi_active)) {
		HELIOSCOM_INFO("Helioscom in resume state\n");
		return 0;
	}

	if (!(g_slav_status_reg & BIT(31))) {
		HELIOSCOM_ERR("Helios boot is not complete, skip SPI resume\n");
		return 0;
	}
	mutex_lock(&helios_task_mutex);
	clnt_handle.helios_spi = spi;
	atomic_set(&helios_is_spi_active, 1);
	atomic_set(&helios_is_runtime_suspend, 0);
	ret = helioscom_resume_l(&clnt_handle);
	HELIOSCOM_INFO("Helioscom resumed with : %d\n", ret);
	mutex_unlock(&helios_task_mutex);
	return ret;
#endif
	return 0;
}

static int helioscom_pm_runtime_suspend(struct device *dev)
{
	struct helios_context clnt_handle;
	uint32_t cmnd_reg = 0;
	struct spi_device *s_dev = to_spi_device(dev);
	struct helios_spi_priv *helios_spi = spi_get_drvdata(s_dev);
	int ret = 0;

	clnt_handle.helios_spi = helios_spi;

	if (atomic_read(&state) == HELIOSCOM_STATE_RUNTIME_SUSPEND)
		return 0;

	mutex_lock(&helios_task_mutex);

	cmnd_reg |= HELIOS_OK_SLP_RBSC;

	ret = helioscom_reg_write_cmd(&clnt_handle, HELIOS_CMND_REG,
					1, &cmnd_reg);
	sleep_time_start = ktime_get();
	if (ret == 0) {
		atomic_set(&state, HELIOSCOM_STATE_RUNTIME_SUSPEND);
		atomic_set(&helios_is_spi_active, 0);
		atomic_set(&helios_is_runtime_suspend, 1);
		atomic_set(&ok_to_sleep, 1);
	}
	HELIOSCOM_INFO("Runtime suspended with : %d\n", ret);
	mutex_unlock(&helios_task_mutex);
	return ret;
}

static int helioscom_pm_runtime_resume(struct device *dev)
{
	struct helios_context clnt_handle;
	int ret;
	struct helios_spi_priv *spi =
		container_of(helios_com_drv, struct helios_spi_priv, lhandle);

	clnt_handle.helios_spi = spi;

	if (atomic_read(&spi->irq_lock) == 1) {
		atomic_set(&helios_is_spi_active, 1);
		atomic_set(&helios_is_runtime_suspend, 0);
		atomic_set(&state, HELIOSCOM_STATE_ACTIVE);
		pr_debug("Helios Already Woken up! Skip.....\n");
		return 0;
	}
	mutex_lock(&helios_task_mutex);
	atomic_set(&helios_is_spi_active, 1);
	atomic_set(&helios_is_runtime_suspend, 0);
	ret = helioscom_resume_l(&clnt_handle);
	HELIOSCOM_INFO("Helioscom Runtime resumed with : %d\n", ret);
	mutex_unlock(&helios_task_mutex);
	return ret;
}

static int helioscom_pm_freeze(struct device *dev)
{
#if POWER_ENABLED
	struct helios_context clnt_handle;
	uint32_t cmnd_reg = 0;
	struct spi_device *s_dev = to_spi_device(dev);
	struct helios_spi_priv *helios_spi = spi_get_drvdata(s_dev);
	int ret = 0;

	clnt_handle.helios_spi = helios_spi;
	if (atomic_read(&state) == HELIOSCOM_STATE_HIBERNATE)
		return 0;

	if (atomic_read(&state) == HELIOSCOM_STATE_RUNTIME_SUSPEND) {
		atomic_set(&state, HELIOSCOM_STATE_HIBERNATE);
		atomic_set(&helios_is_spi_active, 0);
		atomic_set(&helios_is_runtime_suspend, 0);
		HELIOSCOM_INFO("suspended\n");
		return 0;
	}

	if (!(g_slav_status_reg & BIT(31))) {
		HELIOSCOM_ERR("Helios boot is not complete, skip SPI suspend\n");
		return 0;
	}

	cmnd_reg |= HELIOS_OK_SLP_RBSC;

	ret = helioscom_reg_write_cmd(&clnt_handle, HELIOS_CMND_REG, 1, &cmnd_reg);
	if (ret == 0) {
		atomic_set(&state, HELIOSCOM_STATE_HIBERNATE);
		atomic_set(&helios_is_spi_active, 0);
		atomic_set(&helios_is_runtime_suspend, 0);
		atomic_set(&ok_to_sleep, 1);
	}
	HELIOSCOM_INFO("freezed with : %d\n", ret);
	return ret;
#endif
	return 0;
}

static int helioscom_pm_restore(struct device *dev)
{
#if POWER_ENABLED
	struct helios_context clnt_handle;
	int ret = 0;
	struct helios_spi_priv *spi =
		container_of(helios_com_drv, struct helios_spi_priv, lhandle);

	if (atomic_read(&helios_is_spi_active)) {
		HELIOSCOM_INFO("Helioscom in restore state\n");
	} else {
		if (!(g_slav_status_reg & BIT(31))) {
			HELIOSCOM_ERR("Helios boot is not complete, skip SPI resume\n");
			return 0;
		}
		clnt_handle.helios_spi = spi;
		atomic_set(&helios_is_spi_active, 1);
		atomic_set(&helios_is_runtime_suspend, 0);
		ret = helioscom_resume_l(&clnt_handle);
		HELIOSCOM_INFO("Helioscom restore with : %d\n", ret);
	}
	return ret;
#endif
	return 0;
}

static const struct dev_pm_ops helioscom_pm = {
	.prepare = helioscom_pm_prepare,
	.complete = helioscom_pm_complete,
	.runtime_suspend = helioscom_pm_runtime_suspend,
	.runtime_resume = helioscom_pm_runtime_resume,
	.suspend = helioscom_pm_suspend,
	.resume = helioscom_pm_resume,
	.freeze = helioscom_pm_freeze,
	.restore = helioscom_pm_restore,
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
