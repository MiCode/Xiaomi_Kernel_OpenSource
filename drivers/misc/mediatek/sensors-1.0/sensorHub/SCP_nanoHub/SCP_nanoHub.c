// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#define pr_fmt(fmt) "[sensorHub] " fmt

#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/atomic.h>
#include <linux/types.h>
#include <linux/module.h>
#include <asm/arch_timer.h>
#include <linux/pm_wakeup.h>
#include <linux/suspend.h>
#include <linux/time.h>
#include "include/scp.h"
#include "cust_sensorHub.h"
#include "hwmsensor.h"
#include "sensors_io.h"
#include "SCP_sensorHub.h"
#include "hwmsen_helper.h"
#include "comms.h"
#include "sensor_event.h"
#include "sensor_performance.h"
#include "SCP_power_monitor.h"
#include <asm/arch_timer.h>
#include <linux/math64.h>
#include <linux/timekeeping.h>
#include <uapi/linux/sched/types.h>

/* ALGIN TO SCP SENSOR_IPI_SIZE AT FILE CONTEXTHUB_FW.H, ALGIN
 * TO SCP_SENSOR_HUB_DATA UNION, ALGIN TO STRUCT DATA_UNIT_T
 * SIZEOF(STRUCT DATA_UNIT_T) = SCP_SENSOR_HUB_DATA = SENSOR_IPI_SIZE
 * BUT AT THE MOMENT AP GET DATA THROUGH IPI, WE ONLY TRANSFER
 * 44 BYTES DATA_UNIT_T, THERE ARE 4 BYTES HEADER IN SCP_SENSOR_HUB_DATA
 * HEAD
 */
#define SENSOR_IPI_SIZE 48
/*
 * experience number for delay_count per DELAY_COUNT sensor input delay 10ms
 * msleep(10) system will schedule to hal process then read input node
 */
#define SENSOR_IPI_HEADER_SIZE 4
#define SENSOR_IPI_PACKET_SIZE (SENSOR_IPI_SIZE - SENSOR_IPI_HEADER_SIZE)
#define SENSOR_DATA_SIZE 44

#if SENSOR_DATA_SIZE > SENSOR_IPI_PACKET_SIZE
#error "SENSOR_DATA_SIZE > SENSOR_IPI_PACKET_SIZE, out of memory"
#endif

#define SYNC_TIME_CYCLC 10000
#define SYNC_TIME_START_CYCLC 3000
#define SCP_sensorHub_DEV_NAME "SCP_sensorHub"

#define CHRE_POWER_RESET_NOTIFY

static int sensor_send_timestamp_to_hub(void);
static int SCP_sensorHub_server_dispatch_data(uint32_t *currWp);
static int SCP_sensorHub_init_flag = -1;
static uint8_t rtc_compensation_suspend;
struct curr_wp_queue {
	spinlock_t buffer_lock;
	uint32_t head;
	uint32_t tail;
	uint32_t bufsize;
	uint32_t *ringbuffer;
};

struct SCP_sensorHub_data {
	/* struct work_struct power_up_work; */

	struct sensorHub_hw *hw;
	struct work_struct direct_push_work;
	struct workqueue_struct	*direct_push_workqueue;
	struct timer_list sync_time_timer;
	struct work_struct sync_time_worker;
	struct wakeup_source *ws;

	struct sensorFIFO *SCP_sensorFIFO;
	struct curr_wp_queue wp_queue;
	phys_addr_t shub_dram_phys;
	phys_addr_t shub_dram_virt;
	SCP_sensorHub_handler dispatch_data_cb[ID_SENSOR_MAX_HANDLE_PLUS_ONE];
	atomic_t traces[ID_SENSOR_MAX_HANDLE_PLUS_ONE];
};
static struct SensorState mSensorState[SENSOR_TYPE_MAX_NUM_PLUS_ONE];
static DEFINE_MUTEX(mSensorState_mtx);
static DEFINE_MUTEX(flush_mtx);
static atomic_t power_status = ATOMIC_INIT(SENSOR_POWER_DOWN);
static DECLARE_WAIT_QUEUE_HEAD(chre_kthread_wait);
static DECLARE_WAIT_QUEUE_HEAD(power_reset_wait);
static uint8_t chre_kthread_wait_condition;
static DEFINE_SPINLOCK(scp_state_lock);
static uint8_t scp_system_ready;
static uint8_t scp_chre_ready;
static struct SCP_sensorHub_data *obj_data;
/* arch counter is 13M, mult is 161319385, shift is 21 */
static inline uint64_t arch_counter_to_ns(uint64_t cyc)
{
#define ARCH_TIMER_MULT 161319385
#define ARCH_TIMER_SHIFT 21
	return (cyc * ARCH_TIMER_MULT) >> ARCH_TIMER_SHIFT;
}

#define FILTER_DATAPOINTS	16
#define FILTER_TIMEOUT		10000000000ULL /* 10 seconds, ~100us drift */
#define FILTER_FREQ			10000000ULL /* 10 ms */
struct moving_average {
	uint64_t last_time;
	int64_t input[FILTER_DATAPOINTS];
	atomic64_t output;
	uint8_t cnt;
	uint8_t tail;
};
static struct moving_average moving_average_algo;
static uint8_t rtc_compensation_suspend;
static void moving_average_filter(struct moving_average *filter,
		uint64_t ap_time, uint64_t hub_time)
{
	int i = 0;
	int64_t avg = 0;
	int64_t ret_avg = 0;

	if (ap_time > filter->last_time + FILTER_TIMEOUT ||
		filter->last_time == 0) {
		filter->tail = 0;
		filter->cnt = 0;
	} else if (ap_time < filter->last_time + FILTER_FREQ) {
		return;
	}
	filter->last_time = ap_time;

	filter->input[filter->tail++] = ap_time - hub_time;
	filter->tail &= (FILTER_DATAPOINTS - 1);
	if (filter->cnt < FILTER_DATAPOINTS)
		filter->cnt++;

	/* pr_err("hongxu raw_offset=%lld\n", ap_time - hub_time); */

	for (i = 1, avg = 0; i < filter->cnt; i++)
		avg += (filter->input[i] - filter->input[0]);
	ret_avg = div_s64(avg, filter->cnt) + filter->input[0];
	atomic64_set(&filter->output, ret_avg);
}

static uint64_t get_filter_output(struct moving_average *filter)
{
	return atomic64_read(&filter->output);
}

struct SCP_sensorHub_Cmd {
	uint32_t reason;
	void (*handler)(union SCP_SENSOR_HUB_DATA *rsp, int rx_len);
};

#define SCP_SENSORHUB_CMD(_reason, _handler) \
	{.reason = _reason, .handler = _handler}

struct ipi_master {
	spinlock_t		    lock;
	struct list_head	queue;
	struct workqueue_struct	*workqueue;
	struct work_struct	work;
};

struct ipi_transfer {
	const unsigned char	*tx_buf;
	unsigned char		rx_buf[SENSOR_IPI_SIZE];
	unsigned int		len;
	struct list_head transfer_list;
};

struct ipi_message {
	struct list_head	transfers;
	struct list_head	queue;
	void			*context;
	int				status;
	void			(*complete)(void *context);
};

struct scp_send_ipi {
	struct completion	 done;
	int			 len;
	int			 count;
	/* data buffers */
	const unsigned char	*tx;
	unsigned char		*rx;
	void			*context;
};

static struct scp_send_ipi txrx_cmd;
static DEFINE_SPINLOCK(txrx_cmd_lock);
static struct ipi_master master;

static inline void ipi_message_init(struct ipi_message *m)
{
	memset(m, 0, sizeof(*m));
	INIT_LIST_HEAD(&m->transfers);
}

static inline void ipi_message_add_tail(struct ipi_transfer *t,
	struct ipi_message *m)
{
	list_add_tail(&t->transfer_list, &m->transfers);
}

static int ipi_txrx_bufs(struct ipi_transfer *t)
{
	int status = 0, retry = 0;
	int timeout;
	unsigned long flags;
	struct scp_send_ipi *hw = &txrx_cmd;

	/* union SCP_SENSOR_HUB_DATA *req =
	 *		(union SCP_SENSOR_HUB_DATA *)t->tx_buf;
	 * pr_err("sensorType:%d, action:%d\n",
	 *req->req.sensorType, req->req.action);
	 */
	spin_lock_irqsave(&txrx_cmd_lock, flags);
	hw->tx = t->tx_buf;
	hw->rx = t->rx_buf;
	hw->len = t->len;

	init_completion(&hw->done);
	hw->context = &hw->done;
	spin_unlock_irqrestore(&txrx_cmd_lock, flags);
	do {
		status = scp_ipi_send(IPI_SENSOR,
			(unsigned char *)hw->tx, hw->len, 0, SCP_A_ID);
		if (status == SCP_IPI_ERROR) {
			pr_err("scp_ipi_send fail\n");
			return -1;
		}
		if (status == SCP_IPI_BUSY) {
			if (retry++ == 1000) {
				pr_err("retry fail\n");
				return -1;
			}
			if (retry % 100 == 0)
				usleep_range(1000, 2000);
		}
	} while (status == SCP_IPI_BUSY);

	if (retry >= 100)
		pr_debug("retry time:%d\n", retry);

	timeout = wait_for_completion_timeout(&hw->done, msecs_to_jiffies(500));
	spin_lock_irqsave(&txrx_cmd_lock, flags);
	if (!timeout) {
		pr_err("transfer timeout!");
		hw->count = -1;
	}
	hw->context = NULL;
	spin_unlock_irqrestore(&txrx_cmd_lock, flags);
	return hw->count;
}

static void ipi_complete(void *arg)
{
	complete(arg);
}

static void ipi_work(struct work_struct *work)
{
	struct ipi_message	*m, *_m;
	struct ipi_transfer	*t = NULL;
	int			status = 0;

	spin_lock(&master.lock);
	list_for_each_entry_safe(m, _m, &master.queue, queue) {
		list_del(&m->queue);
		spin_unlock(&master.lock);
		list_for_each_entry(t, &m->transfers, transfer_list) {
			if (!t->tx_buf && t->len) {
				status = -EINVAL;
				pr_err("transfer param wrong :%d\n",
					status);
				break;
			}
			if (t->len)
				status = ipi_txrx_bufs(t);
			if (status < 0) {
				status = -EREMOTEIO;
				/* pr_err("transfer err :%d\n", status); */
				break;
			} else if (status != t->len) {
				status = -EREMOTEIO;
				pr_err("ack err :%d\n", status);
				break;
			}
			status = 0;
		}
		m->status = status;
		m->complete(m->context);
		spin_lock(&master.lock);
	}
	spin_unlock(&master.lock);
}

static int __ipi_transfer(struct ipi_message *m)
{
	m->status = -EINPROGRESS;

	spin_lock(&master.lock);
	list_add_tail(&m->queue, &master.queue);
	queue_work(master.workqueue, &master.work);
	spin_unlock(&master.lock);
	return 0;
}

static int __ipi_xfer(struct ipi_message *message)
{
	DECLARE_COMPLETION_ONSTACK(done);
	int status;

	message->complete = ipi_complete;
	message->context = &done;

	status = __ipi_transfer(message);

	if (status == 0) {
		wait_for_completion(&done);
		status = message->status;
	}
	message->context = NULL;
	return status;
}

static int scp_ipi_txrx(const unsigned char *txbuf, unsigned int n_tx,
	unsigned char *rxbuf, unsigned int n_rx)
{
	struct ipi_transfer t;
	struct ipi_message	m;
	int status = 0;

	t.tx_buf = txbuf,
	t.len = n_tx,

	ipi_message_init(&m);
	ipi_message_add_tail(&t, &m);
	status =  __ipi_xfer(&m);
	if (status == 0)
		memcpy(rxbuf, t.rx_buf, n_rx);
	return status;
}

static int SCP_sensorHub_ipi_txrx(unsigned char *txrxbuf)
{
	return scp_ipi_txrx(txrxbuf,
		SENSOR_IPI_SIZE, txrxbuf, SENSOR_IPI_SIZE);
}

static int SCP_sensorHub_ipi_master_init(void)
{
	INIT_WORK(&master.work, ipi_work);
	INIT_LIST_HEAD(&master.queue);
	spin_lock_init(&master.lock);
	master.workqueue = create_singlethread_workqueue("ipi_master");
	if (master.workqueue == NULL) {
		pr_err("workqueue fail\n");
		return -1;
	}

	return 0;
}

int scp_sensorHub_req_send(union SCP_SENSOR_HUB_DATA *data,
	uint *len, unsigned int wait)
{
	int ret = 0;

	/* pr_err("sensorType = %d, action = %d\n", data->req.sensorType,
	 *	data->req.action);
	 */

	if (*len > SENSOR_IPI_SIZE) {
		pr_err("over sensor data size!!\n");
		return -1;
	}

	if (in_interrupt()) {
		pr_err("Can't do %s in interrupt context!!\n", __func__);
		return -1;
	}

	if (data->rsp.sensorType > ID_SENSOR_MAX_HANDLE) {
		pr_err("SCP_sensorHub_IPI_handler invalid sensor type %d\n",
			data->rsp.sensorType);
		return -1;
	}
	ret = SCP_sensorHub_ipi_txrx((unsigned char *)data);
	if (ret != 0 || data->rsp.errCode != 0)
		return -1;
	return 0;
}

int scp_sensorHub_data_registration(uint8_t sensor,
	SCP_sensorHub_handler handler)
{
	struct SCP_sensorHub_data *obj = obj_data;

	if (sensor > ID_SENSOR_MAX_HANDLE) {
		pr_err("SCP_sensorHub_rsp_registration invalid sensor %d\n",
			sensor);
		return -1;
	}

	if (handler == NULL)
		pr_err("SCP_sensorHub_rsp_registration null handler\n");

	obj->dispatch_data_cb[sensor] = handler;

	return 0;
}
EXPORT_SYMBOL_GPL(scp_sensorHub_data_registration);

static void SCP_sensorHub_write_wp_queue(union SCP_SENSOR_HUB_DATA *rsp)
{
	struct SCP_sensorHub_data *obj = obj_data;
	struct curr_wp_queue *wp_queue = &obj->wp_queue;

	spin_lock(&wp_queue->buffer_lock);
	wp_queue->ringbuffer[wp_queue->head++] = rsp->notify_rsp.currWp;
	wp_queue->head &= wp_queue->bufsize - 1;
	if (unlikely(wp_queue->head == wp_queue->tail))
		pr_err("dropped currWp due to ringbuffer is full\n");
	spin_unlock(&wp_queue->buffer_lock);
}
static int SCP_sensorHub_fetch_next_wp(uint32_t *currWp)
{
	int have_event;
	struct SCP_sensorHub_data *obj = obj_data;
	struct curr_wp_queue *wp_queue = &obj->wp_queue;

	spin_lock_irq(&wp_queue->buffer_lock);

	have_event = wp_queue->head != wp_queue->tail;
	if (have_event) {
		*currWp = wp_queue->ringbuffer[wp_queue->tail++];
		wp_queue->tail &= wp_queue->bufsize - 1;
	}
	spin_unlock_irq(&wp_queue->buffer_lock);
	/* pr_err("head:%d, tail:%d, currWp:%d\n",
	 * wp_queue->head, wp_queue->tail, *currWp);
	 */
	return have_event;
}
static int SCP_sensorHub_read_wp_queue(void)
{
	uint32_t currWp = 0;

	while (SCP_sensorHub_fetch_next_wp(&currWp)) {
		if (SCP_sensorHub_server_dispatch_data(&currWp))
			return -EFAULT;
	}
	return 0;
}
static void SCP_sensorHub_sync_time_work(struct work_struct *work)

{
	struct SCP_sensorHub_data *obj = obj_data;

	sensor_send_timestamp_to_hub();
	mod_timer(&obj->sync_time_timer,
		jiffies +  msecs_to_jiffies(SYNC_TIME_CYCLC));
}

static void SCP_sensorHub_sync_time_func(struct timer_list *t)
{
	struct SCP_sensorHub_data *obj = from_timer(obj, t, sync_time_timer);

	schedule_work(&obj->sync_time_worker);
}

static int SCP_sensorHub_direct_push_work(void *data)
{
	for (;;) {
		wait_event(chre_kthread_wait,
			READ_ONCE(chre_kthread_wait_condition));
		WRITE_ONCE(chre_kthread_wait_condition, false);
		mark_timestamp(0, WORK_START, ktime_get_boot_ns(), 0);
		SCP_sensorHub_read_wp_queue();
	}
	return 0;
}

static void SCP_sensorHub_xcmd_putdata(union SCP_SENSOR_HUB_DATA *rsp,
			int rx_len)
{
	union SCP_SENSOR_HUB_DATA *req;
	struct scp_send_ipi *hw = &txrx_cmd;

	spin_lock(&txrx_cmd_lock);
	if (!hw->context) {
		pr_err("after ipi timeout ack occur then dropped this\n");
		goto out;
	}

	req = (union SCP_SENSOR_HUB_DATA *)hw->tx;

	if (req->req.sensorType != rsp->rsp.sensorType ||
		req->req.action != rsp->rsp.action) {
		pr_debug("req type %d != rsp %d req action %d != rsq %d\n",
			req->req.sensorType, rsp->rsp.sensorType,
			req->req.action, rsp->rsp.action);
	} else {
		memcpy(hw->rx, rsp, rx_len);
		hw->count = rx_len;
		complete(hw->context);
	}
out:
	spin_unlock(&txrx_cmd_lock);
}

static void SCP_sensorHub_enable_cmd(union SCP_SENSOR_HUB_DATA *rsp,
					int rx_len)
{
	SCP_sensorHub_xcmd_putdata(rsp, rx_len);
}
static void SCP_sensorHub_set_delay_cmd(union SCP_SENSOR_HUB_DATA *rsp,
						int rx_len)
{
	SCP_sensorHub_xcmd_putdata(rsp, rx_len);
}
static void SCP_sensorHub_get_data_cmd(union SCP_SENSOR_HUB_DATA *rsp,
						int rx_len)
{
	SCP_sensorHub_xcmd_putdata(rsp, rx_len);
}
static void SCP_sensorHub_batch_cmd(union SCP_SENSOR_HUB_DATA *rsp,
					int rx_len)
{
	SCP_sensorHub_xcmd_putdata(rsp, rx_len);
}
static void SCP_sensorHub_set_cfg_cmd(union SCP_SENSOR_HUB_DATA *rsp,
					int rx_len)
{
	SCP_sensorHub_xcmd_putdata(rsp, rx_len);
}
static void SCP_sensorHub_set_cust_cmd(union SCP_SENSOR_HUB_DATA *rsp,
						int rx_len)
{
	SCP_sensorHub_xcmd_putdata(rsp, rx_len);
}
static void
SCP_sensorHub_batch_timeout_cmd(union SCP_SENSOR_HUB_DATA *rsp,
					int rx_len)
{
	SCP_sensorHub_xcmd_putdata(rsp, rx_len);
}
static void
SCP_sensorHub_set_timestamp_cmd(union SCP_SENSOR_HUB_DATA *rsp,
					int rx_len)
{
	SCP_sensorHub_xcmd_putdata(rsp, rx_len);
}

static void SCP_sensorHub_moving_average(union SCP_SENSOR_HUB_DATA *rsp)
{
	uint64_t ap_now_time = 0, arch_counter = 0;
	uint64_t scp_raw_time = 0, scp_now_time = 0;
	uint64_t ipi_transfer_time = 0;

	/* if (!timekeeping_rtc_skipresume()) {
	 *	if (READ_ONCE(rtc_compensation_suspend))
	 *	return;
	 * }
	 */
	ap_now_time = ktime_get_boottime_ns();
	arch_counter = __arch_counter_get_cntvct();
	scp_raw_time = rsp->notify_rsp.scp_timestamp;
	ipi_transfer_time = arch_counter_to_ns(arch_counter -
		rsp->notify_rsp.arch_counter);
	scp_now_time = scp_raw_time + ipi_transfer_time;
	moving_average_filter(&moving_average_algo, ap_now_time, scp_now_time);
}
static void SCP_sensorHub_notify_cmd(union SCP_SENSOR_HUB_DATA *rsp,
	int rx_len)
{
	/* struct SCP_sensorHub_data *obj = obj_data; */
	/*
	 *struct data_unit_t *event;
	 *int handle = 0;
	 */

	unsigned long flags = 0;

	switch (rsp->notify_rsp.event) {
	case SCP_DIRECT_PUSH:
	case SCP_FIFO_FULL:
		mark_timestamp(0, GOT_IPI, ktime_get_boot_ns(), 0);
		mark_ipi_timestamp(arch_counter_get_cntvct() -
			rsp->notify_rsp.arch_counter);
#if IS_ENABLED(CONFIG_DEBUG_PERFORMANCE_HW_TICK)
		pr_notice("[Performance] AP_get_ipi Stanley kernel tick:%llu\n",
			arch_counter_get_cntvct());

#endif
		SCP_sensorHub_moving_average(rsp);
		SCP_sensorHub_write_wp_queue(rsp);
		/* queue_work(obj->direct_push_workqueue,
		 * &obj->direct_push_work);
		 */
		WRITE_ONCE(chre_kthread_wait_condition, true);
		wake_up(&chre_kthread_wait);
		break;
	case SCP_NOTIFY:
/*
 *		handle = rsp->rsp.sensorType;
 *		if (handle > ID_SENSOR_MAX_HANDLE) {
 *			pr_err("invalid sensor %d\n", handle);
 *		} else {
 *			event = (struct data_unit_t *)rsp->notify_rsp.int8_Data;
 *			if (obj->dispatch_data_cb[handle] != NULL)
 *				obj->dispatch_data_cb[handle](event, NULL);
 *			else
 *				pr_err("type:%d don't support this flow?\n",
 *					handle);
 *			if (event->flush_action == FLUSH_ACTION)
 *				atomic_dec(&mSensorState[handle].flushCnt);
 *		}
 */
		break;
	case SCP_INIT_DONE:
		spin_lock_irqsave(&scp_state_lock, flags);
		WRITE_ONCE(scp_chre_ready, true);
		if (READ_ONCE(scp_system_ready) && READ_ONCE(scp_chre_ready)) {
			spin_unlock_irqrestore(&scp_state_lock, flags);
			atomic_set(&power_status, SENSOR_POWER_UP);
			scp_power_monitor_notify(SENSOR_POWER_UP, NULL);
			/* schedule_work(&obj->power_up_work); */
			wake_up(&power_reset_wait);
		} else
			spin_unlock_irqrestore(&scp_state_lock, flags);
		break;
	default:
		break;
	}
}

static const struct SCP_sensorHub_Cmd SCP_sensorHub_Cmds[] = {
	SCP_SENSORHUB_CMD(SENSOR_HUB_ACTIVATE,
		SCP_sensorHub_enable_cmd),
	SCP_SENSORHUB_CMD(SENSOR_HUB_SET_DELAY,
		SCP_sensorHub_set_delay_cmd),
	SCP_SENSORHUB_CMD(SENSOR_HUB_GET_DATA,
		SCP_sensorHub_get_data_cmd),
	SCP_SENSORHUB_CMD(SENSOR_HUB_BATCH,
		SCP_sensorHub_batch_cmd),
	SCP_SENSORHUB_CMD(SENSOR_HUB_SET_CONFIG,
		SCP_sensorHub_set_cfg_cmd),
	SCP_SENSORHUB_CMD(SENSOR_HUB_SET_CUST,
		SCP_sensorHub_set_cust_cmd),
	SCP_SENSORHUB_CMD(SENSOR_HUB_BATCH_TIMEOUT,
		SCP_sensorHub_batch_timeout_cmd),
	SCP_SENSORHUB_CMD(SENSOR_HUB_SET_TIMESTAMP,
		SCP_sensorHub_set_timestamp_cmd),
	SCP_SENSORHUB_CMD(SENSOR_HUB_NOTIFY,
		SCP_sensorHub_notify_cmd),
};

const struct SCP_sensorHub_Cmd *
	SCP_sensorHub_find_cmd(uint32_t packetReason)
{
	int i;
	const struct SCP_sensorHub_Cmd *cmd;

	for (i = 0; i < ARRAY_SIZE(SCP_sensorHub_Cmds); i++) {
		cmd = &SCP_sensorHub_Cmds[i];
		if (cmd->reason == packetReason)
			return cmd;
	}
	return NULL;
}

static void SCP_sensorHub_IPI_handler(int id,
	void *data, unsigned int len)
{
	union SCP_SENSOR_HUB_DATA *rsp = (union SCP_SENSOR_HUB_DATA *) data;
	const struct SCP_sensorHub_Cmd *cmd;

	if (len > SENSOR_IPI_SIZE) {
		pr_err("%s len=%d error\n", __func__, len);
		return;
	}
	/*pr_err("sensorType:%d, action=%d event:%d len:%d\n",
	 * rsp->rsp.sensorType, rsp->rsp.action, rsp->notify_rsp.event, len);
	 */
	cmd = SCP_sensorHub_find_cmd(rsp->rsp.action);
	if (cmd != NULL)
		cmd->handler(rsp, len);
	else
		pr_err("cannot find cmd!\n");
}

static void SCP_sensorHub_init_sensor_state(void)
{
	mSensorState[SENSOR_TYPE_ACCELEROMETER].sensorType =
		SENSOR_TYPE_ACCELEROMETER;
	mSensorState[SENSOR_TYPE_ACCELEROMETER].timestamp_filter = true;
#if IS_ENABLED(CONFIG_MTK_UNCALI_ACCHUB)
	mSensorState[SENSOR_TYPE_ACCELEROMETER].alt =
		SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED;
	mSensorState[SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED].sensorType =
		SENSOR_TYPE_ACCELEROMETER;
	mSensorState[SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED].alt =
		SENSOR_TYPE_ACCELEROMETER;
	mSensorState[SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED].timestamp_filter =
		true;
#endif

	mSensorState[SENSOR_TYPE_GYROSCOPE].sensorType = SENSOR_TYPE_GYROSCOPE;
	mSensorState[SENSOR_TYPE_GYROSCOPE].timestamp_filter = true;
#if IS_ENABLED(CONFIG_MTK_UNCALI_GYROHUB)
	mSensorState[SENSOR_TYPE_GYROSCOPE].alt =
		SENSOR_TYPE_GYROSCOPE_UNCALIBRATED;
	mSensorState[SENSOR_TYPE_GYROSCOPE_UNCALIBRATED].sensorType =
		SENSOR_TYPE_GYROSCOPE;
	mSensorState[SENSOR_TYPE_GYROSCOPE_UNCALIBRATED].alt =
		SENSOR_TYPE_GYROSCOPE;
	mSensorState[SENSOR_TYPE_GYROSCOPE_UNCALIBRATED].timestamp_filter =
		true;
#endif

	mSensorState[SENSOR_TYPE_MAGNETIC_FIELD].sensorType =
		SENSOR_TYPE_MAGNETIC_FIELD;
	mSensorState[SENSOR_TYPE_MAGNETIC_FIELD].timestamp_filter = true;
#if IS_ENABLED(CONFIG_MTK_UNCALI_MAGHUB)
	mSensorState[SENSOR_TYPE_MAGNETIC_FIELD].alt =
		SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED;
	mSensorState[SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED].sensorType =
		SENSOR_TYPE_MAGNETIC_FIELD;
	mSensorState[SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED].alt =
		SENSOR_TYPE_MAGNETIC_FIELD;
	mSensorState[SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED].timestamp_filter =
		true;
#endif

	mSensorState[SENSOR_TYPE_LIGHT].sensorType = SENSOR_TYPE_LIGHT;
	mSensorState[SENSOR_TYPE_LIGHT].timestamp_filter = false;

	mSensorState[SENSOR_TYPE_PROXIMITY].sensorType = SENSOR_TYPE_PROXIMITY;
	mSensorState[SENSOR_TYPE_PROXIMITY].timestamp_filter = false;

	mSensorState[SENSOR_TYPE_PRESSURE].sensorType = SENSOR_TYPE_PRESSURE;
	mSensorState[SENSOR_TYPE_PRESSURE].timestamp_filter = false;

	mSensorState[SENSOR_TYPE_ORIENTATION].sensorType =
		SENSOR_TYPE_ORIENTATION;
	mSensorState[SENSOR_TYPE_ORIENTATION].timestamp_filter = true;

	mSensorState[SENSOR_TYPE_ROTATION_VECTOR].sensorType =
		SENSOR_TYPE_ROTATION_VECTOR;
	mSensorState[SENSOR_TYPE_ROTATION_VECTOR].timestamp_filter = true;

	mSensorState[SENSOR_TYPE_GAME_ROTATION_VECTOR].sensorType =
		SENSOR_TYPE_GAME_ROTATION_VECTOR;
	mSensorState[SENSOR_TYPE_GAME_ROTATION_VECTOR].timestamp_filter = true;

	mSensorState[SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR].sensorType =
		SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR;
	mSensorState[SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR].timestamp_filter =
		true;

	mSensorState[SENSOR_TYPE_LINEAR_ACCELERATION].sensorType =
		SENSOR_TYPE_LINEAR_ACCELERATION;
	mSensorState[SENSOR_TYPE_LINEAR_ACCELERATION].timestamp_filter = true;

	mSensorState[SENSOR_TYPE_GRAVITY].sensorType = SENSOR_TYPE_GRAVITY;
	mSensorState[SENSOR_TYPE_GRAVITY].timestamp_filter = true;

	mSensorState[SENSOR_TYPE_SIGNIFICANT_MOTION].sensorType =
		SENSOR_TYPE_SIGNIFICANT_MOTION;
	mSensorState[SENSOR_TYPE_SIGNIFICANT_MOTION].rate = SENSOR_RATE_ONESHOT;
	mSensorState[SENSOR_TYPE_SIGNIFICANT_MOTION].timestamp_filter = false;

	mSensorState[SENSOR_TYPE_STEP_COUNTER].sensorType =
		SENSOR_TYPE_STEP_COUNTER;
	mSensorState[SENSOR_TYPE_STEP_COUNTER].rate = SENSOR_RATE_ONCHANGE;
	mSensorState[SENSOR_TYPE_STEP_COUNTER].timestamp_filter = false;

	mSensorState[SENSOR_TYPE_STEP_DETECTOR].sensorType =
		SENSOR_TYPE_STEP_DETECTOR;
	mSensorState[SENSOR_TYPE_STEP_DETECTOR].rate = SENSOR_RATE_ONCHANGE;
	mSensorState[SENSOR_TYPE_STEP_DETECTOR].timestamp_filter = false;

	mSensorState[SENSOR_TYPE_TILT_DETECTOR].sensorType =
		SENSOR_TYPE_TILT_DETECTOR;
	mSensorState[SENSOR_TYPE_TILT_DETECTOR].rate = SENSOR_RATE_ONCHANGE;
	mSensorState[SENSOR_TYPE_TILT_DETECTOR].timestamp_filter = false;

	mSensorState[SENSOR_TYPE_IN_POCKET].sensorType = SENSOR_TYPE_IN_POCKET;
	mSensorState[SENSOR_TYPE_IN_POCKET].rate = SENSOR_RATE_ONESHOT;
	mSensorState[SENSOR_TYPE_IN_POCKET].timestamp_filter = false;

	mSensorState[SENSOR_TYPE_ACTIVITY].sensorType = SENSOR_TYPE_ACTIVITY;
	mSensorState[SENSOR_TYPE_ACTIVITY].timestamp_filter = false;

	mSensorState[SENSOR_TYPE_GLANCE_GESTURE].sensorType =
		SENSOR_TYPE_GLANCE_GESTURE;
	mSensorState[SENSOR_TYPE_GLANCE_GESTURE].rate = SENSOR_RATE_ONESHOT;
	mSensorState[SENSOR_TYPE_GLANCE_GESTURE].timestamp_filter = false;

	mSensorState[SENSOR_TYPE_PICK_UP_GESTURE].sensorType =
		SENSOR_TYPE_PICK_UP_GESTURE;
	mSensorState[SENSOR_TYPE_PICK_UP_GESTURE].rate = SENSOR_RATE_ONESHOT;
	mSensorState[SENSOR_TYPE_PICK_UP_GESTURE].timestamp_filter = false;

	mSensorState[SENSOR_TYPE_WAKE_GESTURE].sensorType =
		SENSOR_TYPE_WAKE_GESTURE;
	mSensorState[SENSOR_TYPE_WAKE_GESTURE].rate = SENSOR_RATE_ONESHOT;
	mSensorState[SENSOR_TYPE_WAKE_GESTURE].timestamp_filter = false;

	mSensorState[SENSOR_TYPE_ANSWER_CALL].sensorType =
		SENSOR_TYPE_ANSWER_CALL;
	mSensorState[SENSOR_TYPE_ANSWER_CALL].rate = SENSOR_RATE_ONESHOT;
	mSensorState[SENSOR_TYPE_ANSWER_CALL].timestamp_filter = false;

	mSensorState[SENSOR_TYPE_STATIONARY_DETECT].sensorType =
		SENSOR_TYPE_STATIONARY_DETECT;
	mSensorState[SENSOR_TYPE_STATIONARY_DETECT].rate = SENSOR_RATE_ONESHOT;
	mSensorState[SENSOR_TYPE_STATIONARY_DETECT].timestamp_filter = false;

	mSensorState[SENSOR_TYPE_MOTION_DETECT].sensorType =
		SENSOR_TYPE_MOTION_DETECT;
	mSensorState[SENSOR_TYPE_MOTION_DETECT].rate = SENSOR_RATE_ONESHOT;
	mSensorState[SENSOR_TYPE_MOTION_DETECT].timestamp_filter = false;

	mSensorState[SENSOR_TYPE_DEVICE_ORIENTATION].sensorType =
		SENSOR_TYPE_DEVICE_ORIENTATION;
	mSensorState[SENSOR_TYPE_DEVICE_ORIENTATION].rate =
		SENSOR_RATE_ONCHANGE;
	mSensorState[SENSOR_TYPE_DEVICE_ORIENTATION].timestamp_filter = false;

	mSensorState[SENSOR_TYPE_GEOFENCE].sensorType = SENSOR_TYPE_GEOFENCE;
	mSensorState[SENSOR_TYPE_GEOFENCE].rate = SENSOR_RATE_ONCHANGE;
	mSensorState[SENSOR_TYPE_GEOFENCE].timestamp_filter = false;

	mSensorState[SENSOR_TYPE_FLOOR_COUNTER].sensorType =
		SENSOR_TYPE_FLOOR_COUNTER;
	mSensorState[SENSOR_TYPE_FLOOR_COUNTER].rate = SENSOR_RATE_ONCHANGE;
	mSensorState[SENSOR_TYPE_FLOOR_COUNTER].timestamp_filter = false;

	mSensorState[SENSOR_TYPE_FLAT].sensorType = SENSOR_TYPE_FLAT;
	mSensorState[SENSOR_TYPE_FLAT].rate = SENSOR_RATE_ONESHOT;
	mSensorState[SENSOR_TYPE_FLAT].timestamp_filter = false;

	mSensorState[SENSOR_TYPE_RGBW].sensorType = SENSOR_TYPE_RGBW;
	mSensorState[SENSOR_TYPE_RGBW].timestamp_filter = false;

	mSensorState[SENSOR_TYPE_SAR].sensorType = SENSOR_TYPE_SAR;
	mSensorState[SENSOR_TYPE_SAR].timestamp_filter = false;
}

static void init_sensor_config_cmd(struct ConfigCmd *cmd,
		int sensor_type)
{
	uint8_t alt = mSensorState[sensor_type].alt;
	bool enable = 0;

	memset(cmd, 0x00, sizeof(*cmd));

	cmd->evtType = EVT_NO_SENSOR_CONFIG_EVENT;
	cmd->sensorType = mSensorState[sensor_type].sensorType;

	if (alt && mSensorState[alt].enable &&
			mSensorState[sensor_type].enable) {
		cmd->cmd = CONFIG_CMD_ENABLE;
		if (mSensorState[alt].rate > mSensorState[sensor_type].rate)
			cmd->rate = mSensorState[alt].rate;
		else
			cmd->rate = mSensorState[sensor_type].rate;
		if (mSensorState[alt].latency <
				mSensorState[sensor_type].latency)
			cmd->latency = mSensorState[alt].latency;
		else
			cmd->latency = mSensorState[sensor_type].latency;
	} else if (alt && mSensorState[alt].enable) {
		enable = mSensorState[alt].enable;
		cmd->cmd =  enable ? CONFIG_CMD_ENABLE : CONFIG_CMD_DISABLE;
		cmd->rate = mSensorState[alt].rate;
		cmd->latency = mSensorState[alt].latency;
	} else { /* !alt || !mSensorState[alt].enable */
		enable = mSensorState[sensor_type].enable;
		cmd->cmd = enable ? CONFIG_CMD_ENABLE : CONFIG_CMD_DISABLE;
		cmd->rate = mSensorState[sensor_type].rate;
		cmd->latency = mSensorState[sensor_type].latency;
	}
}

static int SCP_sensorHub_batch(int handle, int flag,
	long long samplingPeriodNs, long long maxBatchReportLatencyNs)
{
	uint8_t sensor_type = handle + ID_OFFSET;
	struct ConfigCmd cmd;
	int ret = 0;
	uint64_t rate = 1024000000000ULL;

	if (mSensorState[sensor_type].sensorType) {
		if (samplingPeriodNs > 0 && mSensorState[sensor_type].rate !=
			SENSOR_RATE_ONCHANGE &&
			mSensorState[sensor_type].rate != SENSOR_RATE_ONESHOT) {
			rate = div64_u64(rate, samplingPeriodNs);
			mSensorState[sensor_type].rate = rate;
		}
		mSensorState[sensor_type].latency = maxBatchReportLatencyNs;
		init_sensor_config_cmd(&cmd, sensor_type);
		if (atomic_read(&power_status) != SENSOR_POWER_UP)
			return 0;
		ret = nanohub_external_write((const uint8_t *)&cmd,
			sizeof(struct ConfigCmd));
		if (ret < 0) {
			pr_err("fail enbatch h:%d, r: %d,l: %lld, cmd:%d\n",
				handle, cmd.rate, cmd.latency, cmd.cmd);
			return -1;
		}
	} else {
		pr_err("unhandle handle=%d, is inited?\n", handle);
		return -1;
	}
	return 0;
}

static int SCP_sensorHub_flush(int handle)
{
	uint8_t sensor_type = handle + ID_OFFSET;
	struct ConfigCmd cmd;
	int ret = 0;
	atomic_t *p_flush_count = NULL;

	if (mSensorState[sensor_type].sensorType) {
		init_sensor_config_cmd(&cmd, sensor_type);
		cmd.cmd = CONFIG_CMD_FLUSH;
		/*
		 * add count must before flush, if we add count after
		 * flush right return and flush callback directly report
		 * flush will lose flush complete
		 */
		p_flush_count = &mSensorState[sensor_type].flushCnt;
		mutex_lock(&flush_mtx);
		atomic_inc(p_flush_count);
		mutex_unlock(&flush_mtx);
		if (atomic_read(&power_status) == SENSOR_POWER_UP) {
			ret = nanohub_external_write((const uint8_t *)&cmd,
				sizeof(struct ConfigCmd));
			if (ret < 0) {
				pr_err("failed flush handle:%d\n", handle);
				mutex_lock(&flush_mtx);
				if (atomic_read(p_flush_count) > 0)
					atomic_dec(p_flush_count);
				mutex_unlock(&flush_mtx);
				return -1;
			}
		}
	} else {
		pr_err("unhandle handle=%d, is inited?\n", handle);
		return -1;
	}
	return 0;
}

static int SCP_sensorHub_report_raw_data(struct data_unit_t *data_t)
{
	struct SCP_sensorHub_data *obj = obj_data;
	int err = 0, sensor_type = 0, sensor_id = 0;
	atomic_t *p_flush_count = NULL;
	bool raw_enable = 0;
	int64_t raw_enable_time = 0;

	sensor_id = data_t->sensor_type;
	sensor_type = sensor_id + ID_OFFSET;

	if (sensor_id < 0 || sensor_id > ID_SENSOR_MAX_HANDLE) {
		pr_err("invalid sensor %d\n", sensor_id);
		return 0;
	}

	if (obj->dispatch_data_cb[sensor_id] == NULL) {
		pr_err("type:%d don't support this flow?\n", sensor_id);
		return 0;
	}

	raw_enable = READ_ONCE(mSensorState[sensor_type].enable);
	raw_enable_time = atomic64_read(&mSensorState[sensor_type].enableTime);

	if (raw_enable && data_t->flush_action == DATA_ACTION) {
		if (data_t->time_stamp > raw_enable_time)
			err = obj->dispatch_data_cb[sensor_id](data_t, NULL);
		else
			pr_info("ac:%d, e:%lld, d:%lld\n", data_t->flush_action,
				raw_enable_time, data_t->time_stamp);
	} else if (data_t->flush_action == FLUSH_ACTION) {
		mutex_lock(&flush_mtx);
		p_flush_count = &mSensorState[sensor_type].flushCnt;
		if (atomic_read(p_flush_count) > 0) {
			err = obj->dispatch_data_cb[sensor_id](data_t, NULL);
			if (!err)
				atomic_dec(p_flush_count);
		}
		mutex_unlock(&flush_mtx);
	} else if (data_t->flush_action == BIAS_ACTION ||
			data_t->flush_action == CALI_ACTION ||
			data_t->flush_action == TEMP_ACTION ||
			data_t->flush_action == TEST_ACTION)
		err = obj->dispatch_data_cb[sensor_id](data_t, NULL);
	return err;
}

static int SCP_sensorHub_report_alt_data(struct data_unit_t *data_t)
{
	struct SCP_sensorHub_data *obj = obj_data;
	int err = 0, sensor_type = 0, sensor_id = 0, alt_id;
	uint8_t alt = 0;
	atomic_t *p_flush_count = NULL;
	bool alt_enable = 0;
	int64_t alt_enable_time = 0;

	sensor_id = data_t->sensor_type;
	sensor_type = sensor_id + ID_OFFSET;

	if (sensor_id < 0 || sensor_id > ID_SENSOR_MAX_HANDLE) {
		pr_err("invalid sensor %d\n", sensor_id);
		return 0;
	}

	alt = READ_ONCE(mSensorState[sensor_type].alt);
	alt_id = alt - ID_OFFSET;

	if (alt == 0)
		return 0;

	if (obj->dispatch_data_cb[alt_id] == NULL) {
		pr_err("alt:%d don't support this flow?\n", alt_id);
		return 0;
	}

	alt_enable = READ_ONCE(mSensorState[alt].enable);
	alt_enable_time = atomic64_read(&mSensorState[alt].enableTime);

	if (alt_enable && data_t->flush_action == DATA_ACTION) {
		if (data_t->time_stamp > alt_enable_time)
			err = obj->dispatch_data_cb[alt_id](data_t, NULL);
		else
			pr_info("ac:%d, e:%lld, d:%lld\n", data_t->flush_action,
				alt_enable_time, data_t->time_stamp);
	} else if (data_t->flush_action == FLUSH_ACTION) {
		mutex_lock(&flush_mtx);
		p_flush_count = &mSensorState[alt].flushCnt;
		if (atomic_read(p_flush_count) > 0) {
			err = obj->dispatch_data_cb[alt_id](data_t, NULL);
			if (!err)
				atomic_dec(p_flush_count);
		}
		mutex_unlock(&flush_mtx);
	}

	return err;
}

static int SCP_sensorHub_server_dispatch_data(uint32_t *currWp)
{
	struct SCP_sensorHub_data *obj = obj_data;
	char *pStart, *pEnd, *rp, *wp;
	struct data_unit_t event;
	uint32_t wp_copy;
	int err = 0;

	int64_t scp_time = 0;

	pStart = (char *)READ_ONCE(obj->SCP_sensorFIFO) +
		offsetof(struct sensorFIFO, data);
	pEnd = pStart +  READ_ONCE(obj->SCP_sensorFIFO->FIFOSize);
	wp_copy = *currWp;
	rp = pStart + READ_ONCE(obj->SCP_sensorFIFO->rp);
	wp = pStart + wp_copy;

	if (wp < pStart || pEnd < wp) {
		pr_err("FIFO wp invalid : %p, %p, %p\n", pStart, pEnd, wp);
		return -5;
	}
	if (rp == wp) {
		pr_err("FIFO empty\n");
		return 0;
	}
	/*
	 * opimize for dram,no cache,we should cpy data to cacheable ram
	 * event and event_copy are cacheable ram, SCP_sensorHub_report_data
	 * will change time_stamp field, so when SCP_sensorHub_report_data fail
	 * we should reinit the time_stamp by memcpy to event_copy;
	 * why memcpy_fromio(&event_copy), because rp is not cacheable
	 */
	if (rp < wp) {
		while (rp < wp) {
			memcpy_fromio(&event, rp, SENSOR_DATA_SIZE);

			scp_time = event.time_stamp;
			event.time_stamp +=
				get_filter_output(&moving_average_algo);

			do {
				err = SCP_sensorHub_report_raw_data(&event);
				if (err < 0)
					usleep_range(2000, 4000);
			} while (err < 0);

			do {
				err = SCP_sensorHub_report_alt_data(&event);
				if (err < 0)
					usleep_range(2000, 4000);
			} while (err < 0);

			rp += SENSOR_DATA_SIZE;
		}
	} else if (rp > wp) {
		while (rp < pEnd) {
			memcpy_fromio(&event, rp, SENSOR_DATA_SIZE);
			scp_time = event.time_stamp;
			event.time_stamp +=
				get_filter_output(&moving_average_algo);

			do {
				err = SCP_sensorHub_report_raw_data(&event);
				if (err < 0)
					usleep_range(2000, 4000);
			} while (err < 0);

			do {
				err = SCP_sensorHub_report_alt_data(&event);
				if (err < 0)
					usleep_range(2000, 4000);
			} while (err < 0);

			rp += SENSOR_DATA_SIZE;
		}
		rp = pStart;
		while (rp < wp) {
			memcpy_fromio(&event, rp, SENSOR_DATA_SIZE);
			scp_time = event.time_stamp;
			event.time_stamp +=
				get_filter_output(&moving_average_algo);

			do {
				err = SCP_sensorHub_report_raw_data(&event);
				if (err < 0)
					usleep_range(2000, 4000);
			} while (err < 0);

			do {
				err = SCP_sensorHub_report_alt_data(&event);
				if (err < 0)
					usleep_range(2000, 4000);
			} while (err < 0);

			rp += SENSOR_DATA_SIZE;
		}
	}
	/* must obj->SCP_sensorFIFO->rp = wp,
	 *there can not obj->SCP_sensorFIFO->rp = obj->SCP_sensorFIFO->wp
	 */
	WRITE_ONCE(obj->SCP_sensorFIFO->rp, wp_copy);
	return 0;
}

static int sensor_send_dram_info_to_hub(void)
{				/* call by init done workqueue */
	struct SCP_sensorHub_data *obj = obj_data;
	union SCP_SENSOR_HUB_DATA data;
	unsigned int len = 0;
	int err = 0, retry = 0, total = 10;

	obj->shub_dram_phys = scp_get_reserve_mem_phys(SENS_MEM_ID);
	obj->shub_dram_virt = scp_get_reserve_mem_virt(SENS_MEM_ID);

	data.set_config_req.sensorType = 0;
	data.set_config_req.action = SENSOR_HUB_SET_CONFIG;
	data.set_config_req.bufferBase =
		(unsigned int)(obj->shub_dram_phys & 0xFFFFFFFF);

	len = sizeof(data.set_config_req);
	for (retry = 0; retry < total; ++retry) {
		err = scp_sensorHub_req_send(&data, &len, 1);
		if (err < 0) {
			pr_err("%s fail!\n", __func__);
			continue;
		}
		break;
	}
	if (retry < total)
		pr_notice("[sensorHub] %s success\n", __func__);
	return SCP_SENSOR_HUB_SUCCESS;
}

static int sensor_send_timestamp_wake_locked(void)
{
	union SCP_SENSOR_HUB_DATA req;
	int len;
	int err = 0;
	uint64_t now_time, arch_counter;

	/* send_timestamp_to_hub is process context, disable irq is safe */
	local_irq_disable();
	now_time = ktime_get_boottime_ns();
	arch_counter = __arch_counter_get_cntvct();
	local_irq_enable();
	req.set_config_req.sensorType = 0;
	req.set_config_req.action = SENSOR_HUB_SET_TIMESTAMP;
	req.set_config_req.ap_timestamp = now_time;
	req.set_config_req.arch_counter = arch_counter;
	pr_debug("sync ap boottime=%lld\n", now_time);
	len = sizeof(req.set_config_req);
	err = scp_sensorHub_req_send(&req, &len, 1);
	if (err < 0)
		pr_err("scp_sensorHub_req_send fail!\n");
	return err;
}

static int sensor_send_timestamp_to_hub(void)
{
	int err = 0;
	struct SCP_sensorHub_data *obj = obj_data;

	if (READ_ONCE(rtc_compensation_suspend)) {
		pr_err("rtc_compensation_suspend suspend,drop time sync\n");
		return 0;
	}

	__pm_stay_awake(obj->ws);
	err = sensor_send_timestamp_wake_locked();
	__pm_relax(obj->ws);
	return err;
}
static void sensor_disable_report_flush(uint8_t handle)
{
	struct SCP_sensorHub_data *obj = obj_data;
	uint8_t sensor_type = handle + ID_OFFSET;
	struct data_unit_t data_t;
	atomic_t *p_flush_count = NULL;
	SCP_sensorHub_handler func;
	int ret = 0, retry = 0;

	func = obj->dispatch_data_cb[handle];
	if (!func)
		return;

	/*
	 * disable sensor only check func return err 5 times
	 */
	mutex_lock(&flush_mtx);
	p_flush_count = &mSensorState[sensor_type].flushCnt;
	while (atomic_read(p_flush_count) > 0) {
		atomic_dec(p_flush_count);
		memset(&data_t, 0, sizeof(struct data_unit_t));
		data_t.sensor_type = handle;
		data_t.flush_action = FLUSH_ACTION;
		do {
			ret = func(&data_t, NULL);
			if (ret < 0)
				usleep_range(2000, 4000);
		} while (ret < 0 && retry++ < 5);
		if (ret < 0)
			pr_err("%d flush complete err when disable\n",
				handle);
	}
	mutex_unlock(&flush_mtx);
}
int sensor_enable_to_hub(uint8_t handle, int enabledisable)
{
	uint8_t sensor_type = handle + ID_OFFSET;
	struct ConfigCmd cmd;
	int ret = 0;

	if (enabledisable == 1)
		scp_register_feature(SENS_FEATURE_ID);
	mutex_lock(&mSensorState_mtx);
	if (handle > ID_SENSOR_MAX_HANDLE) {
		pr_err("invalid handle %d\n", handle);
		ret = -1;
		mutex_unlock(&mSensorState_mtx);
		return ret;
	}
	if (mSensorState[sensor_type].sensorType) {
		mSensorState[sensor_type].enable = enabledisable;
		if (enabledisable)
			atomic64_set(&mSensorState[sensor_type].enableTime,
							ktime_get_boottime_ns());
		init_sensor_config_cmd(&cmd, sensor_type);
		if (atomic_read(&power_status) == SENSOR_POWER_UP) {
			ret = nanohub_external_write((const uint8_t *)&cmd,
				sizeof(struct ConfigCmd));
			if (ret < 0)
				pr_err
				    ("fail registerlistener handle:%d,cmd:%d\n",
				     handle, cmd.cmd);
		}
		if (!enabledisable)
			sensor_disable_report_flush(handle);
	} else {
		pr_err("unhandle handle=%d, is inited?\n", handle);
		mutex_unlock(&mSensorState_mtx);
		return -1;
	}
	mutex_unlock(&mSensorState_mtx);
	return ret < 0 ? ret : 0;
}
EXPORT_SYMBOL_GPL(sensor_enable_to_hub);

int sensor_set_delay_to_hub(uint8_t handle, unsigned int delayms)
{
	int ret = 0;
	long long samplingPeriodNs = delayms * 1000000ULL;

	mutex_lock(&mSensorState_mtx);
	if (handle > ID_SENSOR_MAX_HANDLE) {
		pr_err("invalid sensor %d\n", handle);
		ret = -1;
	} else {
		ret = SCP_sensorHub_batch(handle, 0, samplingPeriodNs, 0);
	}
	mutex_unlock(&mSensorState_mtx);
	return ret < 0 ? ret : 0;
}
EXPORT_SYMBOL_GPL(sensor_set_delay_to_hub);

int sensor_batch_to_hub(uint8_t handle,
	int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
	int ret = 0;

	mutex_lock(&mSensorState_mtx);
	if (handle > ID_SENSOR_MAX_HANDLE) {
		pr_err("invalid handle %d\n", handle);
		ret = -1;
	} else
		ret = SCP_sensorHub_batch(handle,
		flag, samplingPeriodNs, maxBatchReportLatencyNs);
	mutex_unlock(&mSensorState_mtx);
	return ret;
}
EXPORT_SYMBOL_GPL(sensor_batch_to_hub);

int sensor_flush_to_hub(uint8_t handle)
{
	int ret = 0;

	mutex_lock(&mSensorState_mtx);
	if (handle > ID_SENSOR_MAX_HANDLE) {
		pr_err("invalid handle %d\n", handle);
		ret = -1;
	} else
		ret = SCP_sensorHub_flush(handle);
	mutex_unlock(&mSensorState_mtx);
	return ret;
}
EXPORT_SYMBOL_GPL(sensor_flush_to_hub);

int sensor_cfg_to_hub(uint8_t handle, uint8_t *data, uint8_t count)
{
	struct ConfigCmd *cmd = NULL;
	int ret = 0;

	if (handle > ID_SENSOR_MAX_HANDLE) {
		pr_err("invalid handle %d\n", handle);
		ret = -1;
	} else {
		cmd = vzalloc(sizeof(struct ConfigCmd) + count);
		if (cmd == NULL)
			return -1;
		cmd->evtType = EVT_NO_SENSOR_CONFIG_EVENT;
		cmd->sensorType = handle + ID_OFFSET;
		cmd->cmd = CONFIG_CMD_CFG_DATA;
		memcpy(cmd->data, data, count);
		ret = nanohub_external_write((const uint8_t *)cmd,
			sizeof(struct ConfigCmd) + count);
		if (ret < 0) {
			pr_err("failed cfg data handle:%d, cmd:%d\n",
				handle, cmd->cmd);
			ret =  -1;
		}
		vfree(cmd);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(sensor_cfg_to_hub);

int sensor_calibration_to_hub(uint8_t handle)
{
	uint8_t sensor_type = handle + ID_OFFSET;
	struct ConfigCmd cmd;
	int ret = 0;

	if (mSensorState[sensor_type].sensorType) {
		init_sensor_config_cmd(&cmd, sensor_type);
		cmd.cmd = CONFIG_CMD_CALIBRATE;
		ret = nanohub_external_write((const uint8_t *)&cmd,
			sizeof(struct ConfigCmd));
		if (ret < 0) {
			pr_err("failed calibration handle:%d\n",
				handle);
			return -1;
		}
	} else {
		pr_err("unhandle handle=%d, is inited?\n", handle);
		return -1;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(sensor_calibration_to_hub);

int sensor_selftest_to_hub(uint8_t handle)
{
	uint8_t sensor_type = handle + ID_OFFSET;
	struct ConfigCmd cmd;
	int ret = 0;

	if (mSensorState[sensor_type].sensorType) {
		init_sensor_config_cmd(&cmd, sensor_type);
		cmd.cmd = CONFIG_CMD_SELF_TEST;
		ret = nanohub_external_write((const uint8_t *)&cmd,
			sizeof(struct ConfigCmd));
		if (ret < 0) {
			pr_err("failed selfttest handle:%d\n",
				handle);
			return -1;
		}
	} else {
		pr_err("unhandle handle=%d, is inited?\n", handle);
		return -1;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(sensor_selftest_to_hub);

int sensor_get_data_from_hub(uint8_t sensorType,
	struct data_unit_t *data)
{
	union SCP_SENSOR_HUB_DATA req;
	struct data_unit_t *data_t;
	int len = 0, err = 0;

	if (atomic_read(&power_status) == SENSOR_POWER_DOWN) {
		pr_err("scp power down, we can not access scp\n");
		return -1;
	}

	req.get_data_req.sensorType = sensorType;
	req.get_data_req.action = SENSOR_HUB_GET_DATA;
	len = sizeof(req.get_data_req);
	err = scp_sensorHub_req_send(&req, &len, 1);
	if (err < 0) {
		pr_err("fail :%d!\n", err);
		return -1;
	}
	if (sensorType != req.get_data_rsp.sensorType ||
		req.get_data_rsp.action != SENSOR_HUB_GET_DATA ||
		req.get_data_rsp.errCode != 0) {
		pr_err("req Type: %d, rsp Type:%d action:%d, errcode:%d\n",
			sensorType, req.get_data_rsp.sensorType,
			req.get_data_rsp.action, req.get_data_rsp.errCode);

		return req.get_data_rsp.errCode;
	}

	data_t = (struct data_unit_t *)req.get_data_rsp.data.int8_Data;
	switch (sensorType) {
	case ID_ACCELEROMETER:
		data->time_stamp = data_t->time_stamp;
		data->accelerometer_t.x = data_t->accelerometer_t.x;
		data->accelerometer_t.y = data_t->accelerometer_t.y;
		data->accelerometer_t.z = data_t->accelerometer_t.z;
		data->accelerometer_t.x_bias = data_t->accelerometer_t.x_bias;
		data->accelerometer_t.y_bias = data_t->accelerometer_t.y_bias;
		data->accelerometer_t.z_bias = data_t->accelerometer_t.z_bias;
		data->accelerometer_t.status = data_t->accelerometer_t.status;
		break;
	case ID_GRAVITY:
		data->time_stamp = data_t->time_stamp;
		data->accelerometer_t.x = data_t->accelerometer_t.x;
		data->accelerometer_t.y = data_t->accelerometer_t.y;
		data->accelerometer_t.z = data_t->accelerometer_t.z;
		data->accelerometer_t.status = data_t->accelerometer_t.status;
		break;
	case ID_LINEAR_ACCELERATION:
		data->time_stamp = data_t->time_stamp;
		data->accelerometer_t.x = data_t->accelerometer_t.x;
		data->accelerometer_t.y = data_t->accelerometer_t.y;
		data->accelerometer_t.z = data_t->accelerometer_t.z;
		data->accelerometer_t.status = data_t->accelerometer_t.status;
		break;
	case ID_LIGHT:
		data->time_stamp = data_t->time_stamp;
		data->light = data_t->light;
		break;
	case ID_PROXIMITY:
		data->time_stamp = data_t->time_stamp;
		data->proximity_t.steps = data_t->proximity_t.steps;
		data->proximity_t.oneshot = data_t->proximity_t.oneshot;
		break;
	case ID_PRESSURE:
		data->time_stamp = data_t->time_stamp;
		data->pressure_t.pressure = data_t->pressure_t.pressure;
		data->pressure_t.status = data_t->pressure_t.status;
		break;
	case ID_GYROSCOPE:
		data->time_stamp = data_t->time_stamp;
		data->gyroscope_t.x = data_t->gyroscope_t.x;
		data->gyroscope_t.y = data_t->gyroscope_t.y;
		data->gyroscope_t.z = data_t->gyroscope_t.z;
		data->gyroscope_t.x_bias = data_t->gyroscope_t.x_bias;
		data->gyroscope_t.y_bias  = data_t->gyroscope_t.y_bias;
		data->gyroscope_t.z_bias  = data_t->gyroscope_t.z_bias;
		data->gyroscope_t.status = data_t->gyroscope_t.status;
		break;
	case ID_GYROSCOPE_UNCALIBRATED:
		data->time_stamp = data_t->time_stamp;
		data->uncalibrated_gyro_t.x = data_t->uncalibrated_gyro_t.x;
		data->uncalibrated_gyro_t.y = data_t->uncalibrated_gyro_t.y;
		data->uncalibrated_gyro_t.z = data_t->uncalibrated_gyro_t.z;
		data->uncalibrated_gyro_t.x_bias =
			data_t->uncalibrated_gyro_t.x_bias;
		data->uncalibrated_gyro_t.y_bias  =
			data_t->uncalibrated_gyro_t.y_bias;
		data->uncalibrated_gyro_t.z_bias  =
			data_t->uncalibrated_gyro_t.z_bias;
		data->uncalibrated_gyro_t.status =
			data_t->uncalibrated_gyro_t.status;
		break;
	case ID_RELATIVE_HUMIDITY:
		data->time_stamp = data_t->time_stamp;
		data->relative_humidity_t.relative_humidity =
		data_t->relative_humidity_t.relative_humidity;
		data->relative_humidity_t.status =
			data_t->relative_humidity_t.status;
		break;
	case ID_MAGNETIC:
		data->time_stamp = data_t->time_stamp;
		data->magnetic_t.x = data_t->magnetic_t.x;
		data->magnetic_t.y = data_t->magnetic_t.y;
		data->magnetic_t.z = data_t->magnetic_t.z;
		data->magnetic_t.x_bias = data_t->magnetic_t.x_bias;
		data->magnetic_t.y_bias = data_t->magnetic_t.y_bias;
		data->magnetic_t.z_bias = data_t->magnetic_t.z_bias;
		data->magnetic_t.status = data_t->magnetic_t.status;
		break;
	case ID_MAGNETIC_UNCALIBRATED:
		data->time_stamp = data_t->time_stamp;
		data->uncalibrated_mag_t.x = data_t->uncalibrated_mag_t.x;
		data->uncalibrated_mag_t.y = data_t->uncalibrated_mag_t.y;
		data->uncalibrated_mag_t.z = data_t->uncalibrated_mag_t.z;
		data->uncalibrated_mag_t.x_bias =
			data_t->uncalibrated_mag_t.x_bias;
		data->uncalibrated_mag_t.y_bias =
			data_t->uncalibrated_mag_t.y_bias;
		data->uncalibrated_mag_t.z_bias =
			data_t->uncalibrated_mag_t.z_bias;
		data->uncalibrated_mag_t.status =
			data_t->uncalibrated_mag_t.status;
		break;
	case ID_GEOMAGNETIC_ROTATION_VECTOR:
		data->time_stamp = data_t->time_stamp;
		data->magnetic_t.x = data_t->magnetic_t.x;
		data->magnetic_t.y = data_t->magnetic_t.y;
		data->magnetic_t.z = data_t->magnetic_t.z;
		data->magnetic_t.scalar = data_t->magnetic_t.scalar;
		data->magnetic_t.status = data_t->magnetic_t.status;
		break;
	case ID_ORIENTATION:
		data->time_stamp = data_t->time_stamp;
		data->orientation_t.azimuth = data_t->orientation_t.azimuth;
		data->orientation_t.pitch = data_t->orientation_t.pitch;
		data->orientation_t.roll = data_t->orientation_t.roll;
		data->orientation_t.status = data_t->orientation_t.status;
		break;
	case ID_ROTATION_VECTOR:
		data->time_stamp = data_t->time_stamp;
		data->orientation_t.azimuth = data_t->orientation_t.azimuth;
		data->orientation_t.pitch = data_t->orientation_t.pitch;
		data->orientation_t.roll = data_t->orientation_t.roll;
		data->orientation_t.scalar = data_t->orientation_t.scalar;
		data->orientation_t.status = data_t->orientation_t.status;
		break;
	case ID_GAME_ROTATION_VECTOR:
		data->time_stamp = data_t->time_stamp;
		data->orientation_t.azimuth = data_t->orientation_t.azimuth;
		data->orientation_t.pitch = data_t->orientation_t.pitch;
		data->orientation_t.roll = data_t->orientation_t.roll;
		data->orientation_t.scalar = data_t->orientation_t.scalar;
		data->orientation_t.status = data_t->orientation_t.status;
		break;
	case ID_STEP_COUNTER:
		data->time_stamp = data_t->time_stamp;
		data->step_counter_t.accumulated_step_count
		    = data_t->step_counter_t.accumulated_step_count;
		break;
	case ID_STEP_DETECTOR:
		data->time_stamp = data_t->time_stamp;
		data->step_detector_t.step_detect =
			data_t->step_detector_t.step_detect;
		break;
	case ID_SIGNIFICANT_MOTION:
		data->time_stamp = data_t->time_stamp;
		data->smd_t.state = data_t->smd_t.state;
		break;
	case ID_HEART_RATE:
		data->time_stamp = data_t->time_stamp;
		data->heart_rate_t.bpm = data_t->heart_rate_t.bpm;
		data->heart_rate_t.status = data_t->heart_rate_t.status;
		break;
	case ID_PEDOMETER:
		data->time_stamp = data_t->time_stamp;
		data->pedometer_t.accumulated_step_count =
		    data_t->pedometer_t.accumulated_step_count;
		data->pedometer_t.accumulated_step_length =
		    data_t->pedometer_t.accumulated_step_length;
		data->pedometer_t.step_frequency =
			data_t->pedometer_t.step_frequency;
		data->pedometer_t.step_length =
			data_t->pedometer_t.step_length;
		break;
	case ID_ACTIVITY:
		data->time_stamp = data_t->time_stamp;
		data->activity_data_t.probability[STILL] =
		    data_t->activity_data_t.probability[STILL];
		data->activity_data_t.probability[STANDING] =
		    data_t->activity_data_t.probability[STANDING];
		data->activity_data_t.probability[SITTING] =
		    data_t->activity_data_t.probability[SITTING];
		data->activity_data_t.probability[LYING] =
		    data_t->activity_data_t.probability[LYING];
		data->activity_data_t.probability[ON_FOOT] =
		    data_t->activity_data_t.probability[ON_FOOT];
		data->activity_data_t.probability[WALKING] =
		    data_t->activity_data_t.probability[WALKING];
		data->activity_data_t.probability[RUNNING] =
		    data_t->activity_data_t.probability[RUNNING];
		data->activity_data_t.probability[CLIMBING] =
		    data_t->activity_data_t.probability[CLIMBING];
		data->activity_data_t.probability[ON_BICYCLE] =
		    data_t->activity_data_t.probability[ON_BICYCLE];
		data->activity_data_t.probability[IN_VEHICLE] =
		    data_t->activity_data_t.probability[IN_VEHICLE];
		data->activity_data_t.probability[TILTING] =
		    data_t->activity_data_t.probability[TILTING];
		data->activity_data_t.probability[UNKNOWN] =
		    data_t->activity_data_t.probability[UNKNOWN];
		break;
	case ID_IN_POCKET:
		data->time_stamp = data_t->time_stamp;
		data->inpocket_event.state = data_t->inpocket_event.state;
		break;
	case ID_PICK_UP_GESTURE:
		data->time_stamp = data_t->time_stamp;
		data->gesture_data_t.probability =
			data_t->gesture_data_t.probability;
		break;
	case ID_TILT_DETECTOR:
		data->time_stamp = data_t->time_stamp;
		data->tilt_event.state = data_t->tilt_event.state;
		break;
	case ID_WAKE_GESTURE:
		data->time_stamp = data_t->time_stamp;
		data->gesture_data_t.probability =
			data_t->gesture_data_t.probability;
		break;
	case ID_GLANCE_GESTURE:
		data->time_stamp = data_t->time_stamp;
		data->gesture_data_t.probability =
			data_t->gesture_data_t.probability;
		break;
	case ID_PDR:
		data->time_stamp = data_t->time_stamp;
		data->pdr_event.x = data_t->pdr_event.x;
		data->pdr_event.y = data_t->pdr_event.y;
		data->pdr_event.z = data_t->pdr_event.z;
		data->pdr_event.status = data_t->pdr_event.status;
		break;
	case ID_FLOOR_COUNTER:
		data->time_stamp = data_t->time_stamp;
		data->floor_counter_t.accumulated_floor_count
		    = data_t->floor_counter_t.accumulated_floor_count;
		break;
	case ID_SAR:
		data->time_stamp = data_t->time_stamp;
		data->sar_event.data[0] = data_t->sar_event.data[0];
		data->sar_event.data[1] = data_t->sar_event.data[1];
		data->sar_event.data[2] = data_t->sar_event.data[2];
		break;
	default:
		err = -1;
		break;
	}
	return err;
}
EXPORT_SYMBOL_GPL(sensor_get_data_from_hub);

int sensor_set_cmd_to_hub(uint8_t sensorType,
	enum CUST_ACTION action, void *data)
{
	union SCP_SENSOR_HUB_DATA req;
	int len = 0, err = 0;
	struct SCP_SENSOR_HUB_GET_RAW_DATA *pGetRawData;

	req.get_data_req.sensorType = sensorType;
	req.get_data_req.action = SENSOR_HUB_SET_CUST;

	if (atomic_read(&power_status) == SENSOR_POWER_DOWN) {
		pr_err("scp power down, we can not access scp\n");
		return -1;
	}

	switch (sensorType) {
	case ID_ACCELEROMETER:
		req.set_cust_req.sensorType = ID_ACCELEROMETER;
		req.set_cust_req.action = SENSOR_HUB_SET_CUST;
		switch (action) {
		case CUST_ACTION_RESET_CALI:
			req.set_cust_req.resetCali.action =
				CUST_ACTION_RESET_CALI;
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.resetCali);
			break;
		case CUST_ACTION_SET_CALI:
			req.set_cust_req.setCali.action = CUST_ACTION_SET_CALI;
			req.set_cust_req.setCali.int32_data[SCP_SENSOR_HUB_X]
			    = *((int32_t *) data + SCP_SENSOR_HUB_X);
			req.set_cust_req.setCali.int32_data[SCP_SENSOR_HUB_Y]
			    = *((int32_t *) data + SCP_SENSOR_HUB_Y);
			req.set_cust_req.setCali.int32_data[SCP_SENSOR_HUB_Z]
			    = *((int32_t *) data + SCP_SENSOR_HUB_Z);
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.setCali);
			break;
		case CUST_ACTION_SET_TRACE:
			req.set_cust_req.setTrace.action =
				CUST_ACTION_SET_TRACE;
			req.set_cust_req.setTrace.trace = *((int32_t *) data);
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.setTrace);
			break;
		case CUST_ACTION_SET_DIRECTION:
			req.set_cust_req.setDirection.action =
				CUST_ACTION_SET_DIRECTION;
			req.set_cust_req.setDirection.direction =
				*((int32_t *) data);
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
			     custData) + sizeof(req.set_cust_req.setDirection);
			break;
		case CUST_ACTION_SET_FACTORY:
			req.set_cust_req.setFactory.action =
				CUST_ACTION_SET_FACTORY;
			req.set_cust_req.setFactory.factory =
				*((int32_t *) data);
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.setFactory);
			break;
		case CUST_ACTION_SHOW_REG:
			req.set_cust_req.showReg.action = CUST_ACTION_SHOW_REG;
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.showReg);
			break;
		case CUST_ACTION_GET_SENSOR_INFO:
			req.set_cust_req.getInfo.action =
				CUST_ACTION_GET_SENSOR_INFO;
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.getInfo);
			break;
		default:
			return -1;
		}
		break;
	case ID_LIGHT:
		req.set_cust_req.sensorType = ID_LIGHT;
		req.set_cust_req.action = SENSOR_HUB_SET_CUST;
		switch (action) {
		case CUST_ACTION_GET_RAW_DATA:
			req.set_cust_req.getRawData.action =
				CUST_ACTION_GET_RAW_DATA;
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.getRawData);
			err = scp_sensorHub_req_send(&req, &len, 1);
			if (err == 0) {
				if ((req.set_cust_rsp.action !=
					SENSOR_HUB_SET_CUST)
					|| (req.set_cust_rsp.errCode != 0)) {
					pr_err("scp_sHub_req_send fail!\n");
					return -1;
				}
				if (req.set_cust_rsp.getRawData.action !=
					CUST_ACTION_GET_RAW_DATA) {
					pr_err("scp_sHub_req_send fail!\n");
					return -1;
				}
				pGetRawData = &req.set_cust_rsp.getRawData;
				*((uint8_t *) data) =
					pGetRawData->uint8_data[0];
			} else {
				pr_err("scp_sensorHub_req_send failed!\n");
			}
			return 0;
		case CUST_ACTION_SHOW_ALSLV:
			req.set_cust_req.showAlslv.action =
				CUST_ACTION_SHOW_ALSLV;
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.showAlslv);
			break;
		case CUST_ACTION_SHOW_ALSVAL:
			req.set_cust_req.showAlsval.action =
				CUST_ACTION_GET_RAW_DATA;
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.showAlsval);
			break;
		case CUST_ACTION_GET_SENSOR_INFO:
			req.set_cust_req.getInfo.action =
				CUST_ACTION_GET_SENSOR_INFO;
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.getInfo);
			break;
		default:
			return -1;
		}
		break;
	case ID_PROXIMITY:
		req.set_cust_req.sensorType = ID_PROXIMITY;
		req.set_cust_req.action = SENSOR_HUB_SET_CUST;
		switch (action) {
		case CUST_ACTION_RESET_CALI:
			req.set_cust_req.resetCali.action =
				CUST_ACTION_RESET_CALI;
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.resetCali);
			break;
		case CUST_ACTION_SET_CALI:
			req.set_cust_req.setCali.action = CUST_ACTION_SET_CALI;
			req.set_cust_req.setCali.int32_data[0] =
				*((int32_t *) data);
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.setCali);
			break;
		case CUST_ACTION_SET_TRACE:
			req.set_cust_req.setTrace.action =
				CUST_ACTION_SET_TRACE;
			req.set_cust_req.setTrace.trace = *((int32_t *) data);
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.setTrace);
			break;
		case CUST_ACTION_SHOW_REG:
			req.set_cust_req.showReg.action = CUST_ACTION_SHOW_REG;
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.showReg);
			break;
		case CUST_ACTION_SET_PS_THRESHOLD:
			req.set_cust_req.setPSThreshold.action =
				CUST_ACTION_SET_PS_THRESHOLD;
			req.set_cust_req.setPSThreshold.threshold[0]
			    = *((int32_t *) data + 0);
			req.set_cust_req.setPSThreshold.threshold[1]
			    = *((int32_t *) data + 1);
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
			    custData) + sizeof(req.set_cust_req.setPSThreshold);
			break;
		case CUST_ACTION_GET_RAW_DATA:
			req.set_cust_req.getRawData.action =
				CUST_ACTION_GET_RAW_DATA;
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.getRawData);
			err = scp_sensorHub_req_send(&req, &len, 1);
			if (err == 0) {
				if ((req.set_cust_rsp.action !=
					SENSOR_HUB_SET_CUST)
					|| (req.set_cust_rsp.errCode != 0)) {
					pr_err("scp_sHub_req_send fail!\n");
					return -1;
				}
				if (req.set_cust_rsp.getRawData.action !=
					CUST_ACTION_GET_RAW_DATA) {
					pr_err("scp_sHub_req_send fail!\n");
					return -1;
				}
				pGetRawData = &req.set_cust_rsp.getRawData;
				*((uint16_t *) data) =
					pGetRawData->uint16_data[0];
			} else {
				pr_err("scp_sensorHub_req_send failed!\n");
			}
			return 0;
		case CUST_ACTION_GET_SENSOR_INFO:
			req.set_cust_req.getInfo.action =
				CUST_ACTION_GET_SENSOR_INFO;
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.getInfo);
			break;
		default:
			return -1;
		}
		break;
	case ID_PRESSURE:
		req.set_cust_req.sensorType = ID_PRESSURE;
		req.set_cust_req.action = SENSOR_HUB_SET_CUST;
		switch (action) {
		case CUST_ACTION_SET_TRACE:
			req.set_cust_req.setTrace.action =
				CUST_ACTION_SET_TRACE;
			req.set_cust_req.setTrace.trace = *((int32_t *) data);
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.setTrace);
			break;
		case CUST_ACTION_SHOW_REG:
			req.set_cust_req.showReg.action = CUST_ACTION_SHOW_REG;
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.showReg);
			break;
		case CUST_ACTION_GET_SENSOR_INFO:
			req.set_cust_req.getInfo.action =
				CUST_ACTION_GET_SENSOR_INFO;
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.getInfo);
			break;
		default:
			return -1;
		}
		break;
	case ID_GYROSCOPE:
		req.set_cust_req.sensorType = ID_GYROSCOPE;
		req.set_cust_req.action = SENSOR_HUB_SET_CUST;
		switch (action) {
		case CUST_ACTION_RESET_CALI:
			req.set_cust_req.resetCali.action =
				CUST_ACTION_RESET_CALI;
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.resetCali);
			break;
		case CUST_ACTION_SET_CALI:
			req.set_cust_req.setCali.action = CUST_ACTION_SET_CALI;
			req.set_cust_req.setCali.int32_data[SCP_SENSOR_HUB_X]
			    = *((int32_t *) data + SCP_SENSOR_HUB_X);
			req.set_cust_req.setCali.int32_data[SCP_SENSOR_HUB_Y]
			    = *((int32_t *) data + SCP_SENSOR_HUB_Y);
			req.set_cust_req.setCali.int32_data[SCP_SENSOR_HUB_Z]
			    = *((int32_t *) data + SCP_SENSOR_HUB_Z);
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.setCali);
			break;
		case CUST_ACTION_SET_TRACE:
			req.set_cust_req.setTrace.action =
				CUST_ACTION_SET_TRACE;
			req.set_cust_req.setTrace.trace = *((int32_t *) data);
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.setTrace);
			break;
		case CUST_ACTION_SET_DIRECTION:
			req.set_cust_req.setDirection.action =
				CUST_ACTION_SET_DIRECTION;
			req.set_cust_req.setDirection.direction =
				*((int32_t *) data);
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
			     custData) + sizeof(req.set_cust_req.setDirection);
			break;
		case CUST_ACTION_SET_FACTORY:
			req.set_cust_req.setFactory.action =
				CUST_ACTION_SET_FACTORY;
			req.set_cust_req.setFactory.factory =
				*((int32_t *) data);
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.setFactory);
			break;
		case CUST_ACTION_SHOW_REG:
			req.set_cust_req.showReg.action =
				CUST_ACTION_SHOW_REG;
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.showReg);
			break;
		case CUST_ACTION_GET_SENSOR_INFO:
			req.set_cust_req.getInfo.action =
				CUST_ACTION_GET_SENSOR_INFO;
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.getInfo);
			break;
		default:
			return -1;
		}
		break;
	case ID_RELATIVE_HUMIDITY:
		req.set_cust_req.sensorType = ID_MAGNETIC;
		req.set_cust_req.action = SENSOR_HUB_SET_CUST;
		switch (action) {
		case CUST_ACTION_SET_TRACE:
			req.set_cust_req.setTrace.action =
				CUST_ACTION_SET_TRACE;
			req.set_cust_req.setTrace.trace = *((int32_t *) data);
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.setTrace);
			break;
		case CUST_ACTION_SHOW_REG:
			req.set_cust_req.showReg.action = CUST_ACTION_SHOW_REG;
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.showReg);
			break;
		default:
			return -1;
		}
		break;
	case ID_MAGNETIC:
		req.set_cust_req.sensorType = ID_MAGNETIC;
		req.set_cust_req.action = SENSOR_HUB_SET_CUST;
		switch (action) {
		case CUST_ACTION_SET_TRACE:
			req.set_cust_req.setTrace.action =
				CUST_ACTION_SET_TRACE;
			req.set_cust_req.setTrace.trace = *((int32_t *) data);
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.setTrace);
			break;
		case CUST_ACTION_SET_DIRECTION:
			req.set_cust_req.setDirection.action =
				CUST_ACTION_SET_DIRECTION;
			req.set_cust_req.setDirection.direction =
				*((int32_t *) data);
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
			     custData) + sizeof(req.set_cust_req.setDirection);
			break;
		case CUST_ACTION_SHOW_REG:
			req.set_cust_req.showReg.action = CUST_ACTION_SHOW_REG;
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.showReg);
			break;
		case CUST_ACTION_GET_SENSOR_INFO:
			req.set_cust_req.getInfo.action =
				CUST_ACTION_GET_SENSOR_INFO;
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.getInfo);
			break;
		default:
			return -1;
		}
		break;
	case ID_SAR:
		req.set_cust_req.sensorType = ID_SAR;
		req.set_cust_req.action = SENSOR_HUB_SET_CUST;
		switch (action) {
		case CUST_ACTION_GET_SENSOR_INFO:
			req.set_cust_req.getInfo.action =
				CUST_ACTION_GET_SENSOR_INFO;
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.getInfo);
			break;
		default:
			return -1;
		}
		break;
	default:
		req.set_cust_req.sensorType = sensorType;
		req.set_cust_req.action = SENSOR_HUB_SET_CUST;
		switch (action) {
		case CUST_ACTION_SET_TRACE:
			req.set_cust_req.setTrace.action =
				CUST_ACTION_SET_TRACE;
			req.set_cust_req.setTrace.trace = *((int32_t *) data);
			len = offsetof(struct SCP_SENSOR_HUB_SET_CUST_REQ,
				custData) + sizeof(req.set_cust_req.setTrace);
			break;
		default:
			return -1;
		}
	}
	err = scp_sensorHub_req_send(&req, &len, 1);
	if (err < 0) {
		pr_err("scp_sensorHub_req_send fail!\n");
		return -1;
	}
	if (sensorType != req.get_data_rsp.sensorType
		|| SENSOR_HUB_SET_CUST != req.get_data_rsp.action
		|| 0 != req.get_data_rsp.errCode) {
		pr_err("error : %d\n", req.get_data_rsp.errCode);
		return req.get_data_rsp.errCode;
	}

	switch (action) {
	case CUST_ACTION_GET_SENSOR_INFO:
		if (req.set_cust_rsp.getInfo.action !=
			CUST_ACTION_GET_SENSOR_INFO) {
			pr_info("scp_sensorHub_req_send failed action!\n");
			return -1;
		}
		memcpy((struct sensorInfo_t *)data,
			&req.set_cust_rsp.getInfo.sensorInfo,
			sizeof(struct sensorInfo_t));
		break;
	default:
		break;
	}
	return err;
}
EXPORT_SYMBOL_GPL(sensor_set_cmd_to_hub);

static void restoring_enable_sensorHub_sensor(int handle)
{
	uint8_t sensor_type = handle + ID_OFFSET;
	int ret = 0;
	int i = 0, flush_cnt = 0;
	struct ConfigCmd cmd;

	if (mSensorState[sensor_type].sensorType &&
		mSensorState[sensor_type].enable) {
		init_sensor_config_cmd(&cmd, sensor_type);
		pr_debug("restoring: handle=%d,enable=%d,rate=%d,latency=%lld\n",
			handle, mSensorState[sensor_type].enable,
			mSensorState[sensor_type].rate,
			mSensorState[sensor_type].latency);
		ret = nanohub_external_write((const uint8_t *)&cmd,
			sizeof(struct ConfigCmd));
		if (ret < 0)
			pr_notice("failed registerlistener handle:%d, cmd:%d\n",
				handle, cmd.cmd);

		cmd.cmd = CONFIG_CMD_FLUSH;
		mutex_lock(&flush_mtx);
		flush_cnt = atomic_read(&mSensorState[sensor_type].flushCnt);
		for (i = 0; i < flush_cnt; i++) {
			ret = nanohub_external_write((const uint8_t *)&cmd,
				sizeof(struct ConfigCmd));
			if (ret < 0)
				pr_notice("failed flush handle:%d\n", handle);
		}
		mutex_unlock(&flush_mtx);
	}

}

void sensorHub_power_up_loop(void *data)
{
	int handle = 0;
	struct SCP_sensorHub_data *obj = obj_data;
	unsigned long flags = 0;

	wait_event(power_reset_wait,
		READ_ONCE(scp_system_ready) && READ_ONCE(scp_chre_ready));
	spin_lock_irqsave(&scp_state_lock, flags);
	WRITE_ONCE(scp_chre_ready, false);
	WRITE_ONCE(scp_system_ready, false);
	spin_unlock_irqrestore(&scp_state_lock, flags);

	/* firstly we should update dram information */
	/* 1. reset wp queue head and tail */
	obj->wp_queue.head = 0;
	obj->wp_queue.tail = 0;
	/* 2. init dram information */
	WRITE_ONCE(obj->SCP_sensorFIFO,
		(struct sensorFIFO *)
		(long)scp_get_reserve_mem_virt(SENS_MEM_ID));
	WARN_ON(obj->SCP_sensorFIFO == NULL);
	WRITE_ONCE(obj->SCP_sensorFIFO->wp, 0);
	WRITE_ONCE(obj->SCP_sensorFIFO->rp, 0);
	WRITE_ONCE(obj->SCP_sensorFIFO->FIFOSize,
		((long)scp_get_reserve_mem_size(SENS_MEM_ID) -
		offsetof(struct sensorFIFO, data)) /
		SENSOR_DATA_SIZE * SENSOR_DATA_SIZE);
	pr_debug("obj->SCP_sensorFIFO =%p, wp =%d, rp =%d, size =%d\n",
		READ_ONCE(obj->SCP_sensorFIFO),
		READ_ONCE(obj->SCP_sensorFIFO->wp),
		READ_ONCE(obj->SCP_sensorFIFO->rp),
		READ_ONCE(obj->SCP_sensorFIFO->FIFOSize));
#if !IS_ENABLED(CONFIG_CHRE_POWER_RESET_NOTIFY)
	/* 3. wait for chre init done when don't support power reset feature */
	msleep(2000);
#endif
	/* 4. send dram information to scp */
	sensor_send_dram_info_to_hub();
	/* secondly we enable sensor which sensor is enable by framework */
	mutex_lock(&mSensorState_mtx);
	for (handle = 0; handle < ID_SENSOR_MAX_HANDLE_PLUS_ONE; handle++)
		restoring_enable_sensorHub_sensor(handle);
	mutex_unlock(&mSensorState_mtx);
}

static int sensorHub_power_up_work(void *data)
{
	for (;;)
		sensorHub_power_up_loop(data);
	return 0;
}

static int send_sensor_init_start_event(void)
{
	enum scp_ipi_status ipi_status = SCP_IPI_ERROR;
	uint32_t sensor_init_start_event = 0;
	uint32_t retry = 0;

	do {
		ipi_status = scp_ipi_send(IPI_SENSOR_INIT_START,
			&sensor_init_start_event,
			sizeof(sensor_init_start_event),
			0, SCP_A_ID);
		if (ipi_status == SCP_IPI_ERROR) {
			pr_err("IPI_SENSOR_INIT_START: ipi_send fail\n");
			return SCP_IPI_ERROR;
		}
		if (ipi_status == SCP_IPI_BUSY) {
			if (retry++ == 1000) {
				pr_err("retry fail\n");
				return -SCP_IPI_BUSY;
			}
			if (retry % 10 == 0)
				usleep_range(1000, 2000);
		}
	} while (ipi_status == SCP_IPI_BUSY);

	return 0;
}

static int sensorHub_ready_event(struct notifier_block *this,
	unsigned long event, void *ptr)
{
	unsigned long flags = 0;
	int ret = -1;

	if (event == SCP_EVENT_STOP) {
		spin_lock_irqsave(&scp_state_lock, flags);
		WRITE_ONCE(scp_system_ready, false);
		spin_unlock_irqrestore(&scp_state_lock, flags);
		atomic_set(&power_status, SENSOR_POWER_DOWN);
		scp_power_monitor_notify(SENSOR_POWER_DOWN, ptr);
	}

	if (event == SCP_EVENT_READY) {
		ret = send_sensor_init_start_event();
		if (ret < 0) {
			pr_err("send_sensor_init_start_event: ipi_send fail\n");
			return NOTIFY_BAD;
		}
		spin_lock_irqsave(&scp_state_lock, flags);
		WRITE_ONCE(scp_system_ready, true);
		if (READ_ONCE(scp_system_ready) && READ_ONCE(scp_chre_ready)) {
			spin_unlock_irqrestore(&scp_state_lock, flags);
			atomic_set(&power_status, SENSOR_POWER_UP);
			scp_power_monitor_notify(SENSOR_POWER_UP, ptr);
			/* schedule_work(&obj->power_up_work); */
			wake_up(&power_reset_wait);
		} else
			spin_unlock_irqrestore(&scp_state_lock, flags);
	}

	return NOTIFY_DONE;
}

static struct notifier_block sensorHub_ready_notifier = {
	.notifier_call = sensorHub_ready_event,
};

static int sensorHub_probe(struct platform_device *pdev)
{
	struct SCP_sensorHub_data *obj;
	int err = 0, index;
	struct task_struct *task = NULL;
	struct task_struct *task_power_reset = NULL;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

	pr_debug("%s\n", __func__);
	SCP_sensorHub_init_sensor_state();
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		pr_err("Allocate SCP_sensorHub_data fail\n");
		err = -ENOMEM;
		goto exit;
	}
	memset(obj, 0, sizeof(struct SCP_sensorHub_data));
	obj_data = obj;

	/* init sensor share dram write pointer event queue */
	spin_lock_init(&obj->wp_queue.buffer_lock);
	obj->wp_queue.head = 0;
	obj->wp_queue.tail = 0;
	obj->wp_queue.bufsize = 32;
	obj->wp_queue.ringbuffer =
		vzalloc(obj->wp_queue.bufsize * sizeof(uint32_t));
	if (!obj->wp_queue.ringbuffer) {
		pr_err("Alloc ringbuffer error!\n");
		goto exit_wp_queue;
	}
	/* register ipi interrupt handler */
	scp_ipi_registration(IPI_SENSOR,
		SCP_sensorHub_IPI_handler, "SCP_sensorHub");

	/* init receive scp dram data worker */
	/* INIT_WORK(&obj->direct_push_work, SCP_sensorHub_direct_push_work); */
	/* obj->direct_push_workqueue = alloc_workqueue("chre_work",
	 * WQ_MEM_RECLAIM | WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1);
	 */
	/* obj->direct_push_workqueue =
	 *		create_singlethread_workqueue("chre_work");
	 *	if (obj->direct_push_workqueue == NULL) {
	 *	pr_err("direct_push_workqueue fail\n");
	 *	return -1;
	 *}
	 */

	WRITE_ONCE(chre_kthread_wait_condition, false);
	task = kthread_run(SCP_sensorHub_direct_push_work,
		NULL, "chre_kthread");
	if (IS_ERR(task)) {
		pr_err("SCP_sensorHub_direct_push_work create fail!\n");
		goto exit_direct_push;
	}
	sched_setscheduler(task, SCHED_FIFO, &param);
	/* init the debug trace flag */
	for (index = 0; index < ID_SENSOR_MAX_HANDLE_PLUS_ONE; index++)
		atomic_set(&obj->traces[index], 0);
	/* init timestamp sync worker */
	INIT_WORK(&obj->sync_time_worker, SCP_sensorHub_sync_time_work);
	obj->sync_time_timer.expires =
		jiffies + msecs_to_jiffies(SYNC_TIME_START_CYCLC);
	obj->sync_time_timer.function = SCP_sensorHub_sync_time_func;
	timer_setup(&obj->sync_time_timer, SCP_sensorHub_sync_time_func, 0);
	mod_timer(&obj->sync_time_timer,
		jiffies + msecs_to_jiffies(SYNC_TIME_START_CYCLC));
	obj->ws = wakeup_source_register(NULL, "sync_time");
	if (!obj->ws) {
		pr_err("SCP_sensorHub: wakeup source init fail\n");
		err = -ENOMEM;
		goto exit_wakeup;
	}

	/* this call back can get scp power down status */
	scp_A_register_notify(&sensorHub_ready_notifier);
	/* this call back can get scp power UP status */
	/* INIT_WORK(&obj->power_up_work, sensorHub_power_up_work); */
	task_power_reset = kthread_run(sensorHub_power_up_work,
		NULL, "scp_power_reset");
	if (IS_ERR(task_power_reset)) {
		pr_err("sensorHub_power_up_work create fail!\n");
		goto exit_kthread_power_up;
	}

	SCP_sensorHub_init_flag = 0;
	pr_debug("init done, data_unit_t size: %d,SCP_SENSOR_HUB_DATA size:%d\n",
		(int)sizeof(struct data_unit_t),
		(int)sizeof(union SCP_SENSOR_HUB_DATA));
	BUG_ON(sizeof(struct data_unit_t) != SENSOR_DATA_SIZE
		|| sizeof(union SCP_SENSOR_HUB_DATA) != SENSOR_IPI_SIZE);
	return 0;

exit_kthread_power_up:
	scp_A_unregister_notify(&sensorHub_ready_notifier);
	wakeup_source_unregister(obj->ws);
exit_wakeup:
	if (!IS_ERR(task))
		kthread_stop(task);
exit_direct_push:
	vfree(obj->wp_queue.ringbuffer);
exit_wp_queue:
	kfree(obj);
exit:
	pr_err("%s: err = %d\n", __func__, err);
	SCP_sensorHub_init_flag = -1;
	return err;
}

static int sensorHub_remove(struct platform_device *pdev)
{
	struct SCP_sensorHub_data *obj = obj_data;

	if (obj)
		wakeup_source_unregister(obj->ws);

	return 0;
}

static int sensorHub_suspend(struct platform_device *pdev,
	pm_message_t msg)
{
	/* sensor_send_timestamp_to_hub(); */
	return 0;
}

static int sensorHub_resume(struct platform_device *pdev)
{
	/* sensor_send_timestamp_to_hub(); */
	return 0;
}

static void sensorHub_shutdown(struct platform_device *pdev)
{
	int handle = 0;
	uint8_t sensor_type;
	struct ConfigCmd cmd;
	int ret = 0;

	mutex_lock(&mSensorState_mtx);
	for (handle = 0; handle < ID_SENSOR_MAX_HANDLE_PLUS_ONE; handle++) {
		sensor_type = handle + ID_OFFSET;
		if (mSensorState[sensor_type].sensorType &&
				mSensorState[sensor_type].enable) {
			mSensorState[sensor_type].enable = false;
			init_sensor_config_cmd(&cmd, sensor_type);

			ret = nanohub_external_write((const uint8_t *)&cmd,
				sizeof(struct ConfigCmd));
			if (ret < 0)
				pr_notice("failed registerlistener handle:%d, cmd:%d\n",
					handle, cmd.cmd);
		}
	}
	mutex_unlock(&mSensorState_mtx);
}
static ssize_t nanohub_trace_show(struct device_driver *ddri, char *buf)
{
	struct SCP_sensorHub_data *obj = obj_data;
	int i;
	ssize_t res = 0;

	for (i = 0; i < ID_SENSOR_MAX_HANDLE_PLUS_ONE; i++)
		res += snprintf(&buf[res], PAGE_SIZE, "%2d:[%d]\n",
			i, atomic_read(&obj->traces[i]));
	return res;
}

static ssize_t nanohub_trace_store(struct device_driver *ddri,
	const char *buf, size_t count)
{
	struct SCP_sensorHub_data *obj = obj_data;
	int handle, trace = 0;
	int res = 0;

	pr_debug("%s buf:%s\n", __func__, buf);
	if (sscanf(buf, "%d,%d", &handle, &trace) != 2) {
		pr_err("invalid content: '%s', length = %zu\n", buf, count);
		goto err_out;
	}

	if (handle < 0 || handle > ID_SENSOR_MAX_HANDLE) {
		pr_debug("invalid handle value:%d,should be '0<=handle<=%d'\n",
			trace, ID_SENSOR_MAX_HANDLE);
		goto err_out;
	}

	if (trace != 0 && trace != 1) {
		pr_debug("invalid trace value:%d,trace should be '0' or '1'",
			trace);
		goto err_out;
	}

	res = sensor_set_cmd_to_hub(handle, CUST_ACTION_SET_TRACE, &trace);
	if (res < 0) {
		pr_err("cmd_to_hub (ID: %d),(action: %d)err: %d\n", handle,
					CUST_ACTION_SET_TRACE, res);
	} else
		atomic_set(&obj->traces[handle], trace);

err_out:
	return count;
}

static DRIVER_ATTR_RW(nanohub_trace);

static struct driver_attribute *nanohub_attr_list[] = {
	&driver_attr_nanohub_trace,	/*trace log */
};

static int nanohub_create_attr(struct device_driver *driver)
{
	int idx = 0, err = 0;
	int num = (int)(ARRAY_SIZE(nanohub_attr_list));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, nanohub_attr_list[idx]);
		if (err) {
			pr_err("driver_create_file (%s) = %d\n",
				nanohub_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int nanohub_delete_attr(struct device_driver *driver)
{
	int idx = 0, err = 0;
	int num = (int)(ARRAY_SIZE(nanohub_attr_list));

	if (!driver)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, nanohub_attr_list[idx]);

	return err;
}

static struct platform_device sensorHub_device = {
	.name = "sensor_hub_pl",
	.id = -1,
};

static struct platform_driver sensorHub_driver = {
	.driver = {
	   .name = "sensor_hub_pl",
	},
	.probe = sensorHub_probe,
	.remove = sensorHub_remove,
	.suspend = sensorHub_suspend,
	.resume = sensorHub_resume,
	.shutdown = sensorHub_shutdown,
};

#if IS_ENABLED(CONFIG_PM)
static int sensorHub_pm_event(struct notifier_block *notifier,
	unsigned long pm_event,
			void *unused)
{
	switch (pm_event) {
	case PM_POST_SUSPEND:
		pr_debug("resume ap boottime=%lld\n", ktime_get_boottime_ns());
		WRITE_ONCE(rtc_compensation_suspend, false);
		sensor_send_timestamp_to_hub();
		return NOTIFY_DONE;
	case PM_SUSPEND_PREPARE:
		pr_debug("suspend ap boottime=%lld\n", ktime_get_boottime_ns());
		WRITE_ONCE(rtc_compensation_suspend, true);
		return NOTIFY_DONE;
	default:
		return NOTIFY_OK;
	}
	return NOTIFY_OK;
}

static struct notifier_block sensorHub_pm_notifier_func = {
	.notifier_call = sensorHub_pm_event,
	.priority = 0,
};
#endif /* CONFIG_PM */

int __init SCP_sensorHub_init(void)
{
	nanohub_init();
	SCP_sensorHub_ipi_master_init();
	pr_debug("%s\n", __func__);
	if (platform_device_register(&sensorHub_device)) {
		pr_err("SCP_sensorHub platform device error\n");
		return -1;
	}
	if (platform_driver_register(&sensorHub_driver)) {
		pr_err("SCP_sensorHub platform driver error\n");
		return -1;
	}
	if (nanohub_create_attr(&sensorHub_driver.driver)) {
		pr_err("create attribute err\n");
		nanohub_delete_attr(&sensorHub_driver.driver);
	}
#if IS_ENABLED(CONFIG_PM)
	if (register_pm_notifier(&sensorHub_pm_notifier_func)) {
		pr_err("Failed to register PM notifier.\n");
		return -1;
	}
#endif /* CONFIG_PM */
	return 0;
}

void __exit SCP_sensorHub_exit(void)
{
	pr_debug("%s\n", __func__);
	nanohub_cleanup();
}
module_init(SCP_sensorHub_init);
module_exit(SCP_sensorHub_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SCP sensor hub driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
