// SPDX-License-Identifier: GPL-2.0
/*
 * File:shub_core.c
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/fcntl.h>
#include <linux/firmware.h>
#include <linux/iio/buffer.h>
#include <linux/iio/buffer_impl.h>
#include <linux/iio/iio.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/ktime.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/pm_wakeup.h>
#include <linux/sched.h>
#include <linux/sipc.h>
#include <linux/slab.h>
#include <linux/soc/sprd/sprd_systimer.h>
#include <linux/string.h>
#include <linux/suspend.h>
#include <linux/sysfs.h>
#include <linux/timekeeping.h>
#include <linux/uaccess.h>
#include <uapi/linux/sched/types.h>
#include <linux/reboot.h>

#include "shub_common.h"
#include "shub_core.h"
#include "shub_opcode.h"
#include "shub_protocol.h"

#define MAX_SENSOR_HANDLE 200
static u8 sensor_status[MAX_SENSOR_HANDLE];

static int reader_flag;
static struct task_struct *thread;
static struct task_struct *thread_nwu;
static struct shub_data_processor shub_stream_processor;
static struct shub_data_processor shub_stream_processor_nwu;
static DECLARE_WAIT_QUEUE_HEAD(waiter);
static struct wakeup_source *sensorhub_wake_lock;
static u32 sensorhub_version;

#if SHUB_DATA_DUMP
#define MAX_RX_LEN 102400
static int total_read_byte_cnt;
static u8 sipc_rx_data[MAX_RX_LEN];
#endif
/* for debug flush event */
static int flush_setcnt;
static int flush_getcnt;
struct shub_data *g_sensor;
static int shub_send_event_to_iio(struct shub_data *sensor, u8 *data, u16 len);
static void shub_synctimestamp(struct shub_data *sensor);
static void shub_assert(struct shub_data *sensor);
/**
 * send data
 * handler time must less than 5s
 */
static int shub_send_data(struct shub_data *sensor, u8 *buf, u32 len)
{
	int nwrite = 0;
	int sent_len = 0;
	int timeout = 100;
	int retry = 0;

	do {
		nwrite =
			sbuf_write(sensor->sipc_sensorhub_id, SMSG_CH_PIPE,
					SIPC_PM_BUFID0,
					(void *)(buf + sent_len),
					len - sent_len,
					msecs_to_jiffies(timeout));
		if (nwrite > 0)
			sent_len += nwrite;
		if (nwrite < len || nwrite < 0)
			dev_err(&sensor->sensor_pdev->dev,
				"nwrite=%d,len=%d,sent_len=%d,timeout=%dms\n",
				nwrite, len, sent_len, timeout);
		/* only handle boot exception */
		if (nwrite < 0) {
			if (nwrite == -ENODEV) {
				msleep(100);
				retry++;
				if (retry > 10)
					break;
			} else {
				dev_err(&sensor->sensor_pdev->dev,
					"nwrite=%d\n", nwrite);
				dev_err(&sensor->sensor_pdev->dev,
					"task #: %s, pid = %d, tgid = %d\n",
					current->comm, current->pid,
					current->tgid);
				WARN_ONCE(1, "%s timeout: %dms\n",
					  __func__, timeout);
				/* BUG(); */
				break;
			}
		}
	} while (nwrite < 0 || sent_len  < len);
	return nwrite;
}

static int shub_send_command(struct shub_data *sensor, int sensor_ID,
			     enum shub_subtype_id opcode,
			     const char *data, int len)
{
	struct cmd_data cmddata;
	int encode_len;
	int nwrite = 0;
	int ret = 0;

	if (len > (MAX_MSG_BUFF_SIZE - SHUB_MAX_HEAD_LEN - SHUB_MAX_DATA_CRC)) {
		dev_err(&sensor->sensor_pdev->dev, "shub send data %d over size (%d)",
			len, (MAX_MSG_BUFF_SIZE - SHUB_MAX_HEAD_LEN - SHUB_MAX_DATA_CRC));
		return -EBADMSG;
	}

	mutex_lock(&sensor->send_command_mutex);

	cmddata.type = sensor_ID;
	cmddata.subtype = opcode;
	cmddata.length = len;
	/* no data command  set default data 0xFF */
	if ((len == 0) || (data == NULL)) {
		cmddata.buff[0] = 0xFF;
		cmddata.length = 1;
		len = 1;
	} else {
		memcpy(cmddata.buff, data, len);
	}
	encode_len =
	    shub_encode_one_packet(&cmddata, sensor->writebuff,
				   MAX_MSG_BUFF_SIZE);

	if (encode_len > SHUB_MAX_HEAD_LEN + SHUB_MAX_DATA_CRC) {
		nwrite =
		    shub_send_data(sensor, sensor->writebuff, encode_len);
	}
	/* command timeout test */

	ret = nwrite;
	mutex_unlock(&sensor->send_command_mutex);

	return ret;
}

static int shub_sipc_channel_read(struct shub_data *sensor)
{
	int nread, retry = 0;

	do {
		nread =
			sbuf_read(sensor->sipc_sensorhub_id, SMSG_CH_PIPE,
				SIPC_PM_BUFID0, (void *)sensor->readbuff,
				SERIAL_READ_BUFFER_MAX - 1, -1);
		if (nread < 0) {
			retry++;
			msleep(500);
			dev_err(&sensor->sensor_pdev->dev,
				"nread=%d,retry=%d\n", nread, retry);
			if (retry > 20)
				break;
		}
	} while (nread < 0);

	if (nread > 0) {
		/* for debug */
#if SHUB_DATA_DUMP
		memcpy(sipc_rx_data + total_read_byte_cnt,
		       sensor->readbuff, nread);
		total_read_byte_cnt += nread;
		if (total_read_byte_cnt >= MAX_RX_LEN) {
			total_read_byte_cnt = 0;
			memset(sipc_rx_data, 0, sizeof(sipc_rx_data));
		}
#endif
		shub_parse_one_packet(&shub_stream_processor,
				      sensor->readbuff, nread);
		memset(sensor->readbuff, 0, sizeof(sensor->readbuff));
	} else {
		dev_info(&sensor->sensor_pdev->dev, "can not get data\n");
	}

	return nread;
}

static int shub_sipc_channel_rdnwu(struct shub_data *sensor)
{
	int nread, retry = 0;

	do {
		nread =
			sbuf_read(sensor->sipc_sensorhub_id, SMSG_CH_PIPE,
				SIPC_PM_BUFID1, (void *)sensor->readbuff_nwu,
				SERIAL_READ_BUFFER_MAX - 1, -1);
		if (nread < 0) {
			retry++;
			msleep(500);
			dev_err(&sensor->sensor_pdev->dev,
				"nread=%d,retry=%d\n", nread, retry);
			if (retry > 20)
				break;
		}
	} while (nread < 0);

	if (nread > 0) {
		shub_parse_one_packet(&shub_stream_processor_nwu,
				      sensor->readbuff_nwu, nread);
		memset(sensor->readbuff_nwu, 0, sizeof(sensor->readbuff));
	} else {
		dev_info(&sensor->sensor_pdev->dev, "can not get data\n");
	}
	return nread;
}

static int shub_read_thread(void *data)
{
	struct sched_param param = {.sched_priority = 5 };
	struct shub_data *sensor = data;

	sched_setscheduler(current, SCHED_RR, &param);

	shub_init_parse_packet(&shub_stream_processor);
	wait_event_interruptible(waiter, (reader_flag != 0));
	set_current_state(TASK_RUNNING);
	do {
		shub_sipc_channel_read(sensor);
	} while (!kthread_should_stop());

	return 0;
}

static int shub_read_thread_nwu(void *data)
{
	struct sched_param param = {.sched_priority = 5 };
	struct shub_data *sensor = data;

	sched_setscheduler(current, SCHED_RR, &param);

	shub_init_parse_packet(&shub_stream_processor_nwu);
	wait_event_interruptible(waiter, (reader_flag != 0));
	set_current_state(TASK_RUNNING);
	do {
		shub_sipc_channel_rdnwu(sensor);
	} while (!kthread_should_stop());

	return 0;
}

/* sync from: hardware/libhardware/include/hardware/sensors-base.h */
#define SENSOR_TYPE_LIGHT 5

static void shub_data_callback(struct shub_data *sensor, u8 *data, u32 len)
{
	struct sensor_event_data_t sensor_data;

	sensor_data.cmd = HAL_SEN_DATA;
	memcpy(&sensor_data.shub_sensor_event_t.sensor_handle, data, len);
#if SHUB_DATA_DUMP
	if (!sensor_data.sensor_handle)
		flush_getcnt++;
#endif
	shub_send_event_to_iio(sensor, (u8 *)&sensor_data, sizeof(sensor_data));
}

static void shub_readcmd_callback(struct shub_data *sensor, u8 *data, u32 len)
{
	if (sensor->rx_buf && sensor->rx_len ==  len) {
		memcpy(sensor->rx_buf, data, sensor->rx_len);
		sensor->rx_status = true;
		wake_up_interruptible(&sensor->rxwq);
	} else {
		dev_err(&sensor->sensor_pdev->dev,
			"readcmd_callback error len = %d sensor->rx_len=%d\n", len, sensor->rx_len);
	}
}

static void shub_cm4_read_callback(struct shub_data *sensor,
				   enum shub_subtype_id subtype,
				   u8 *data, u32 len)
{
	switch (subtype) {
	case SHUB_SET_TIMESYNC_SUBTYPE:
		shub_synctimestamp(sensor);
		break;
	case SHUB_GET_SENSORHUB_ASSERT_SUBTYPE:
		shub_assert(sensor);
		break;
	default:
		break;
	}
}

static int shub_sipc_read(struct shub_data *sensor,
			  u8 reg_addr, u8 *data, u32 len)
{
	int err = 0;
	int wait_ret;

	if (reader_flag == 0) {
		dev_info(&sensor->sensor_pdev->dev, "run kthread\n");
		reader_flag = 1;
		wake_up_interruptible(&waiter);
	}

	mutex_lock(&sensor->mutex_read);
	sensor->rx_buf = data;
	sensor->rx_len = len;
	sensor->rx_status = false;

	shub_send_command(sensor, HANDLE_MAX,
			  (enum shub_subtype_id)reg_addr,
			  NULL, 0);
	wait_ret =
	    wait_event_interruptible_timeout(sensor->rxwq,
					     (sensor->rx_status),
					     msecs_to_jiffies
					     (RECEIVE_TIMEOUT_MS));
	sensor->rx_buf = NULL;
	sensor->rx_len = 0;
	if (!sensor->rx_status)
		err = -ETIMEDOUT;
	mutex_unlock(&sensor->mutex_read);

	return err;
}

static int shub_send_event_to_iio(struct shub_data *sensor,
				  u8 *data, u16 len)
{
	u8 event[MAX_CM4_MSG_SIZE];
	u8 i = 0;

	mutex_lock(&sensor->mutex_send);
	memset(event, 0x00, MAX_CM4_MSG_SIZE);
	memcpy(event, data, len);

	if (sensor->log_control.udata[5] == 1) {
		for (i = 0; i < len; i++)
			dev_info(&sensor->sensor_pdev->dev,
				 "data[%d]=%d\n", i, data[i]);
	}
	if (sensor->indio_dev->active_scan_mask &&
	    (!bitmap_empty(sensor->indio_dev->active_scan_mask,
			   sensor->indio_dev->masklength))) {
		iio_push_to_buffers(sensor->indio_dev, event);
	} else if (!sensor->indio_dev->active_scan_mask) {
		dev_err(&sensor->sensor_pdev->dev,
			"active_scan_mask = NULL, event might be missing\n");
	}

	mutex_unlock(&sensor->mutex_send);

	return 0;
}

static void shub_send_ap_status(struct shub_data *sensor, u8 status)
{
	int ret = 0;

	dev_info(&sensor->sensor_pdev->dev, "status=%d\n", status);
	if (sensor->mcu_mode <= SHUB_CALIDOWNLOAD) {
		dev_info(&sensor->sensor_pdev->dev, "mcu_mode == CW_BOOT!\n");
		return;
	}
	sensor->mcu_mode = status;
	ret = shub_send_command(sensor, HANDLE_MAX,
				SHUB_SET_HOST_STATUS_SUBTYPE, &status, 1);
}

static void shub_synctimestamp(struct shub_data *sensor)
{
	unsigned long irq_flags;
	struct cnter_to_boottime convert_para;
	ktime_t kt = 0;

	if (sensor->mcu_mode != SHUB_NORMAL)
		return;

	get_convert_para(&convert_para);
	local_irq_save(irq_flags);
	preempt_disable();
	kt = ktime_get_boottime();
	convert_para.last_systimer_counter = sprd_systimer_read();
	convert_para.last_sysfrt_counter = sprd_sysfrt_read();
	local_irq_restore(irq_flags);
	preempt_enable();

	if (unlikely(sensorhub_version == 20181201))
		convert_para.last_boottime = ktime_to_ms(kt);
	else
		convert_para.last_boottime = ktime_to_ns(kt);

	shub_send_command(sensor,
			  HANDLE_MAX,
			  SHUB_SET_TIMESYNC_SUBTYPE,
			  (char *)&convert_para,
			  sizeof(struct cnter_to_boottime));
}

static void shub_assert(struct shub_data *sensor)
{
	struct shub_mode_info assert_info;

	sensor->mcu_mode = SHUB_BOOT;

	assert_info.cmd = HAL_SHUB_MODE_STORE;
	assert_info.info = SHUB_BOOT;
	dev_info(&sensor->sensor_pdev->dev, "get assert info!\n");
	shub_send_event_to_iio(sensor, (u8 *)&assert_info,
		       sizeof(assert_info));
}
static void shub_synctime_work(struct work_struct *work)
{
	struct shub_data *sensor = container_of((struct delayed_work *)work,
		struct shub_data, time_sync_work);

	shub_synctimestamp(sensor);
	atomic_set(&sensor->delay, SYNC_TIME_DELAY_MS);
	queue_delayed_work(sensor->driver_wq,
			   &sensor->time_sync_work,
			   msecs_to_jiffies(atomic_read(&sensor->delay)));
}

int send_prox_rawdata(u8 data)
{
    int ret = -1;
    if (g_sensor->mcu_mode <= SHUB_CALIDOWNLOAD) {
        dev_info(&g_sensor->sensor_pdev->dev, "mcu_mode == CW_BOOT!\n");
        return ret;
    }
    ret = shub_send_command(g_sensor, SENSOR_PROXIMITY,
        SHUB_SET_PROXIMITY_RAWDATA_SUBTYPE, &data, 1);
    return ret;
}
EXPORT_SYMBOL(send_prox_rawdata);

static ssize_t logctl_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	char *a = "Description:\n";
	char *b = "\techo \"bit val \" > logctl\n\n";
	char *c = "Detail:\n";
	char *d = "\t bit: [0 ~ 7]\n";
	char *d1 = "\t\tbit0: debug message\n";
	char *d2 = "\t\tbit1: sysfs operate message\n";
	char *d3 = "\t\tbit2: function entry message\n";
	char *d4 = "\t\tbit3: data path message\n";
	char *d5 = "\t\tbit4: dump sensor data\n";
	char *e = "\t val: [0 ~ 1], 0 means close, 1 means open\n\n";
	char *f = "\t bit [8] control all flag, all open or close\n";

	return sprintf(buf, "%s%s%s%s%s%s%s%s%s%s%s",
		a, b, c, d, d1, d2, d3, d4, d5, e, f);
}

static ssize_t logctl_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	/* buf:  bit-flag, bit-val
	 *       bit-flag, bit-val
	 */
	int idx = 0;
	int i = 0;
	int total = count / 4;
	u32 bit_flag = 0;
	u32 bit_val = 0;

	struct shub_data *sensor = dev_get_drvdata(dev);

	sensor->log_control.cmd = HAL_LOG_CTL;
	sensor->log_control.length = total;

	dev_info(&sensor->sensor_pdev->dev,
		 "%s total[%d], count[%d]\n", __func__, total, (int)count);
	dev_info(&sensor->sensor_pdev->dev, "char %s\n", buf);

	for (idx = 0; idx < total; idx++)	{
		if (sscanf(buf + 4 * idx, "%u %u\n",
			   &bit_flag, &bit_val) != 2)
			return -EINVAL;
		dev_info(&sensor->sensor_pdev->dev, "%s bit_flag[%d], bit_val[%d]\n",
			 __func__, bit_flag, bit_val);

		if (bit_flag < MAX_SENSOR_LOG_CTL_FLAG_LEN)
			sensor->log_control.udata[bit_flag] = bit_val;

		if (bit_flag == MAX_SENSOR_LOG_CTL_FLAG_LEN) {
			for (i = 0; i < MAX_SENSOR_LOG_CTL_FLAG_LEN; i++)
				sensor->log_control.udata[i] = bit_val;
		}
	}

	shub_send_event_to_iio(sensor, (u8 *)&sensor->log_control,
			       sizeof(sensor->log_control));

	return count;
}
static DEVICE_ATTR_RW(logctl);

#define SENSOR_TYPE_MAGNETIC_FIELD 2
static void shub_save_mag_offset(struct shub_data *sensor,
				 u8 *data, u32 len)
{
		dev_info(&sensor->sensor_pdev->dev, "%s %d\n", __func__, len);
		sensor->cali_store.cmd = HAL_CALI_STORE;
		sensor->cali_store.length = len;
		sensor->cali_store.type = SENSOR_TYPE_MAGNETIC_FIELD;
		memcpy(sensor->cali_store.udata, data, len);

		shub_send_event_to_iio(sensor, (u8 *)&sensor->cali_store,
				       sizeof(sensor->cali_store));
}

static ssize_t enable_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	int handle, enabled;
	enum shub_subtype_id subtype;

	dev_info(&sensor->sensor_pdev->dev, "buf=%s\n", buf);
	if (sensor->mcu_mode <= SHUB_CALIDOWNLOAD) {
		dev_info(&sensor->sensor_pdev->dev,
			 "[%s]mcu_mode == %d!\n", __func__, sensor->mcu_mode);
		return -EAGAIN;
	}

	if (sscanf(buf, "%d %d\n", &handle, &enabled) != 2)
		return -EINVAL;
	dev_info(&sensor->sensor_pdev->dev,
		 "handle = %d, enabled = %d\n", handle, enabled);
	subtype = (enabled == 0) ? SHUB_SET_DISABLE_SUBTYPE :
		SHUB_SET_ENABLE_SUBTYPE;
	if (shub_send_command(sensor, handle, subtype, NULL, 0) < 0) {
		dev_err(&sensor->sensor_pdev->dev, "%s Fail\n", __func__);
		return -EINVAL;
	}

	if (handle < MAX_SENSOR_HANDLE && handle > 0)
		sensor_status[handle] = enabled;

	return count;
}
static DEVICE_ATTR_WO(enable);

static ssize_t batch_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	int flag = 0;
	struct sensor_batch_cmd batch_cmd;

	dev_info(&sensor->sensor_pdev->dev, "buf=%s\n", buf);
	if (sensor->mcu_mode <= SHUB_CALIDOWNLOAD) {
		dev_info(&sensor->sensor_pdev->dev,
			 "mcu_mode == %d!\n",  sensor->mcu_mode);
		return -EAGAIN;
	}

	if (sscanf(buf, "%d %d %d %lld\n",
		   &batch_cmd.handle, &flag,
		   &batch_cmd.report_rate,
		   &batch_cmd.batch_timeout) != 4)
		return -EINVAL;
	dev_info(&sensor->sensor_pdev->dev,
		 "handle = %d, rate = %d, batch_latency = %lld\n",
		 batch_cmd.handle,
		 batch_cmd.report_rate, batch_cmd.batch_timeout);

	if (shub_send_command(sensor, batch_cmd.handle,
			      SHUB_SET_BATCH_SUBTYPE,
			      (char *)&batch_cmd.report_rate, 12) < 0) {
		dev_err(&sensor->sensor_pdev->dev, "%s Fail\n", __func__);
		return -EINVAL;
	}

	return count;
}
static DEVICE_ATTR_WO(batch);

static ssize_t flush_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "flush_setcnt=%d,flush_getcnt=%d\n",
		flush_setcnt, flush_getcnt);
}

static ssize_t flush_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	int handle;

	if (sensor->mcu_mode <= SHUB_CALIDOWNLOAD) {
		dev_err(&sensor->sensor_pdev->dev, "mcu_mode == SHUB_BOOT\n");
		return -EAGAIN;
	}
	if (sscanf(buf, "%d\n", &handle) != 1)
		return -EINVAL;
#if SHUB_DATA_DUMP
	flush_setcnt++;
#endif
	if (shub_send_command(sensor, handle,
			      SHUB_SET_FLUSH_SUBTYPE,
			      NULL, 0x00) < 0) {
		dev_err(&sensor->sensor_pdev->dev, "%s Fail\n", __func__);
		return -EINVAL;
	}

	__pm_wakeup_event(sensorhub_wake_lock, jiffies_to_msecs(200));

	return count;
}
static DEVICE_ATTR_RW(flush);

static int set_calib_cmd(struct shub_data *sensor, u8 cmd, u8 id,
			 u8 type, int golden_value)
{
	char data[6];
	int err;

	if (sensor->mcu_mode <= SHUB_CALIDOWNLOAD) {
		dev_err(&sensor->sensor_pdev->dev, "mcu_mode == SHUB_BOOT\n");
		return -EINVAL;
	}

	data[0] = cmd;
	data[1] = type;
	memcpy(&data[2], &golden_value, sizeof(golden_value));
	err = shub_send_command(sensor, id,
				SHUB_SET_CALIBRATION_CMD_SUBTYPE,
				data, sizeof(data));
	if (!err)
		dev_err(&sensor->sensor_pdev->dev,
			"Write CalibratorCmd Fail\n");

	return err;
}

static ssize_t mcu_mode_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct shub_data *sensor = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", sensor->mcu_mode);
}

static ssize_t mcu_mode_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	int mode = 0;

	if (sscanf(buf, "%d\n", &mode) != 1)
		return -EINVAL;
	sensor->mcu_mode = mode;
	return count;
}
static DEVICE_ATTR_RW(mcu_mode);

static ssize_t calibrator_cmd_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct shub_data *sensor = dev_get_drvdata(dev);

	return sprintf(buf, "Cmd:%d,Id:%d,Type:%d\n", sensor->cal_cmd,
		       sensor->cal_id, sensor->cal_type);
}

static ssize_t calibrator_cmd_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	int len, err;

	if (sensor->mcu_mode <= SHUB_CALIDOWNLOAD) {
		dev_err(&sensor->sensor_pdev->dev, "mcu_mode == SHUB_BOOT!\n");
		return count;
	}

	dev_info(&sensor->sensor_pdev->dev, "buf=%s\n", buf);
	len = sscanf(buf, "%d %d %d %d\n", &sensor->cal_cmd, &sensor->cal_id,
		     &sensor->cal_type, &sensor->golden_sample);
	/* The 3rd and 4th parameters are optional. */
	if (len < 2 || len > 4)
		return -EINVAL;
	err = set_calib_cmd(sensor, sensor->cal_cmd, sensor->cal_id,
			    sensor->cal_type, sensor->golden_sample);
	dev_info(&sensor->sensor_pdev->dev, "cmd:%d,id:%d,type:%d,golden:%d\n",
		 sensor->cal_cmd, sensor->cal_id, sensor->cal_type,
		 sensor->golden_sample);
	if (err < 0)
		dev_err(&sensor->sensor_pdev->dev, " Write Fail!\n");

	return count;
}
static DEVICE_ATTR_RW(calibrator_cmd);

static ssize_t calibrator_data_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	int err, i, n = 0;

	if (sensor->mcu_mode <= SHUB_CALIDOWNLOAD) {
		dev_err(&sensor->sensor_pdev->dev, "mcu_mode == SHUB_BOOT!\n");
		return -EINVAL;
	}

	memset(sensor->cali_store.udata, 0x00, CALIBRATION_DATA_LENGTH);

	err = shub_sipc_read(sensor,
			     SHUB_GET_CALIBRATION_DATA_SUBTYPE,
			     sensor->cali_store.udata, CALIBRATION_DATA_LENGTH);
	if (err < 0) {
		dev_err(&sensor->sensor_pdev->dev, "Read Cali Data Fail\n");
		return err;
	}

	dev_info(&sensor->sensor_pdev->dev, "cmd:%d,id:%d,type:%d\n",
		 sensor->cal_cmd, sensor->cal_id, sensor->cal_type);
	if (sensor->cal_cmd == CALIB_DATA_READ) {
			memcpy(buf, sensor->cali_store.udata, CALIBRATION_DATA_LENGTH);
			return CALIBRATION_DATA_LENGTH;
	}
	if (sensor->cal_cmd == CALIB_CHECK_STATUS)
		return sprintf(buf, "%d\n", sensor->cali_store.udata[0]);

	for (i = 0; i < CALIBRATION_DATA_LENGTH; i++)
		n += sprintf(buf + n, "%d ", sensor->cali_store.udata[i]);
	return n;
}

static ssize_t calibrator_data_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	int sensor_type;
	int err = -1;
	char data[CALIBRATION_DATA_LENGTH];

	if (count < CALIBRATION_DATA_LENGTH + 1) {
		dev_err(&sensor->sensor_pdev->dev,
			"error! invalid calibration data, count=%lu\n", count);
		return -EINVAL;
	}

	sensor_type = (int)buf[0];

	memcpy(data, &buf[1], CALIBRATION_DATA_LENGTH);

	err = shub_send_command(sensor, sensor_type,
				SHUB_SET_CALIBRATION_DATA_SUBTYPE,
				data, CALIBRATION_DATA_LENGTH);
	if (err < 0) {
		dev_err(&sensor->sensor_pdev->dev, "Write CalibratorData Fail\n");
		return err;
	}

	return (CALIBRATION_DATA_LENGTH + 1);
}
static DEVICE_ATTR_RW(calibrator_data);

static ssize_t version_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	u8 data[4];
	u32 version = 0;
	int err;

	err = shub_sipc_read(sensor, SHUB_GET_FWVERSION_SUBTYPE, data, 4);
	if (err >= 0) {
		memcpy(&version, data, sizeof(version));
		dev_info(&sensor->sensor_pdev->dev, "CM4 Version:%u\n",
			 version);

		sbuf_set_no_need_wake_lock(sensor->sipc_sensorhub_id,
			   SMSG_CH_PIPE, SIPC_PM_BUFID1);

		if (sensor->mcu_mode == SHUB_BOOT) {
			sensor->mcu_mode = SHUB_NORMAL;
			sensorhub_version = version;

			/* start time sync */
			cancel_delayed_work_sync(&sensor->time_sync_work);
			queue_delayed_work(sensor->driver_wq, &sensor->time_sync_work, 0);
		}
	} else {
		dev_err(&sensor->sensor_pdev->dev, "Read  FW Version Fail\n");
		return err;
	}

	return sprintf(buf, "%u\n", version);
}
static DEVICE_ATTR_RO(version);

static ssize_t raw_data_als_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	u8 data[2];
	u16 *ptr;
	int err;

	ptr = (u16 *)data;
	if (sensor->mcu_mode <= SHUB_CALIDOWNLOAD) {
		dev_err(&sensor->sensor_pdev->dev, "mcu_mode == SHUB_BOOT!\n");
		return -EINVAL;
	}

	err = shub_sipc_read(sensor, SHUB_GET_LIGHT_RAWDATA_SUBTYPE, data,
			     sizeof(data));
	if (err < 0) {
		dev_err(&sensor->sensor_pdev->dev,
			"read RegMapR_GetLightRawData failed!\n");
		return err;
	}
	return sprintf(buf, "%d\n", ptr[0]);
}
static DEVICE_ATTR_RO(raw_data_als);

static ssize_t raw_data_ps_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	u8 data[2];
	u16 *ptr = (u16 *)data;
	int err;

	if (sensor->mcu_mode <= SHUB_CALIDOWNLOAD) {
		dev_err(&sensor->sensor_pdev->dev, "mcu_mode == SHUB_BOOT!\n");
		return -EINVAL;
	}
	err = shub_sipc_read(sensor, SHUB_GET_PROXIMITY_RAWDATA_SUBTYPE, data,
			     sizeof(data));
	if (err < 0) {
		dev_err(&sensor->sensor_pdev->dev,
			"read RegMapR_GetProximityRawData failed!\n");
		return err;
	}
	return sprintf(buf, "%d\n", ptr[0]);
}
static DEVICE_ATTR_RO(raw_data_ps);

static ssize_t sensor_info_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int i, len = 0;
	struct shub_data *sensor = dev_get_drvdata(dev);

	for (i = 0; i < DRV_SENSOR_COUNT; i++) {
		if (sensor->sensor_info_list[i].name[0] != 0)
			len += snprintf(buf + len, PAGE_SIZE - len,
			"name:%s vendor:%s version:%d\n",
			sensor->sensor_info_list[i].name,
			sensor->sensor_info_list[i].vendor,
			sensor->sensor_info_list[i].version);
	}
	return len;
}
static DEVICE_ATTR_RO(sensor_info);

static ssize_t sensorlist_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err;
	int number = 0;
	u32 len = 0;
	u8 data[4];
	u8 *sensorlist = NULL;
	u32 sensor_info_len = 0;
	u32 max_len;

	struct shub_data *sensor = dev_get_drvdata(dev);

	err = shub_sipc_read(sensor, SHUB_GET_PHYSICAL_SENSOR_NUMBER_SUBTYPE, data, sizeof(number));
	if (err >= 0) {
		memcpy(&number, data, sizeof(number));
		dev_info(&sensor->sensor_pdev->dev, "sensor number = %d\n", number);
	} else {
		dev_info(&sensor->sensor_pdev->dev,
				"sipc read sensorlist support number fail, err = %d\n", err);
		return err;
	}

	if (number >= 0) {
		len = number * sizeof(struct sensor_info_t);
		sensorlist = kzalloc(len, GFP_KERNEL);
		if (!sensorlist)
			return -ENOMEM;

		err = shub_sipc_read(sensor, SHUB_GET_SENSORINFO_SUBTYPE, sensorlist, len);
		if (err >= 0) {
			memcpy(buf, sensorlist, len);
			max_len = DRV_SENSOR_COUNT * sizeof(struct sensor_info_t);
			sensor_info_len = len < max_len ? len : max_len;
			memcpy(&sensor->sensor_info_list, sensorlist, sensor_info_len);
			kfree(sensorlist);
		} else {
			dev_info(&sensor->sensor_pdev->dev,
					"sipc read sensorlist fail, err = %d\n", err);
			kfree(sensorlist);
			return err;
		}
	} else {
		memcpy(buf, &number, sizeof(number));
		return 0;
	}

	if (len > MAX_STRING_SIZE) {
		dev_info(&sensor->sensor_pdev->dev,
				"the sensorlist len is out of read length, len = %d\n", len);
		return -EFBIG;
	}

	return len;
}
static DEVICE_ATTR_RO(sensorlist);

static ssize_t virtual_handle_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err;
	u32 number = 0;
	u32 len = 0;
	u8 data[4];
	u8 *virtual_handle = NULL;

	struct shub_data *sensor = dev_get_drvdata(dev);

	err = shub_sipc_read(sensor, SHUB_GET_VIRTUAL_SENSOR_NUMBER_SUBTYPE, data, sizeof(number));
	if (err >= 0) {
		memcpy(&number, data, sizeof(number));
		len = number * sizeof(number);
		virtual_handle = kzalloc(len, GFP_KERNEL);
		if (!virtual_handle)
			return -ENOMEM;

		err = shub_sipc_read(sensor, SHUB_GET_VIRTUAL_HANDLE_SUBTYPE, virtual_handle, len);
		if (err >= 0) {
			memcpy(buf, virtual_handle, len);
			kfree(virtual_handle);
		} else {
			dev_info(&sensor->sensor_pdev->dev,
					"sipc read virtual handle fail, err = %d\n", err);
			kfree(virtual_handle);
			return err;
		}
	} else {
		dev_info(&sensor->sensor_pdev->dev,
				"sipc read support virtual handle number fail, err = %d\n", err);
		return err;
	}

	if (len > MAX_STRING_SIZE) {
		dev_info(&sensor->sensor_pdev->dev,
				"the virtual len is out of read length, len = %d\n", len);
		return -EFBIG;
	}

	return len;
}
static DEVICE_ATTR_RO(virtual_handle);

static ssize_t cm4_operate_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	char l[50], m[50];

	snprintf(l, sizeof(l), "[cm4_operate]operate:0x%x interface:0x%x ",
		 sensor->cm4_operate_data[0],
		 sensor->cm4_operate_data[1]);

	snprintf(m, sizeof(m),
		 "addr:0x%x reg:0x%x value:0x%x status:0x%x\n\n",
		 sensor->cm4_operate_data[2],
		 sensor->cm4_operate_data[3],
		 sensor->cm4_operate_data[4],
		 sensor->cm4_operate_data[5]);

	memset(sensor->cm4_operate_data, 0, sizeof(sensor->cm4_operate_data));

	return sprintf(buf, "\nDescription :\n"
		 "\techo \"op intf addr reg value mask\" > cm4_operate\n"
		 "\tcat cm4_operate\n\n"
		 "Detail :\n"
		 "\top: 1:read 2:write 3:check_reg 4:set_delay 5:set_gpio "
		 "6:algo_log_control\n"
		 "\tintf(i2c interface): 0:i2c0 1:i2c1\n"
		 "\taddr: IC slave address.\n"
		 "\treg: IC reg or set_gpio reg\n"
		 "\tvalue: i2c writen value or set_gpio value "
		 "or control algo log(0 or 1)\n"
		 "\tmask: i2c r/w bit operate or set_delay value(ms)\n\n"
		 "\tstatus: show execution result. 1:success 0:fail\n\n"
		 "%s%s\n", l, m);
}

static ssize_t cm4_operate_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	u8 cm4_buf[6];
	struct shub_data *sensor = dev_get_drvdata(dev);

	if (sscanf(buf, "%4hhx %4hhx %4hhx %4hhx %4hhx %4hhx\n", &cm4_buf[0],
		   &cm4_buf[1], &cm4_buf[2], &cm4_buf[3], &cm4_buf[4],
		   &cm4_buf[5]) != 6)
		return -EINVAL;

	shub_send_command(sensor, SHUB_NODATA,
			  SHUB_CM4_OPERATE, cm4_buf,
			  sizeof(cm4_buf));

	return count;
}

static DEVICE_ATTR_RW(cm4_operate);

static ssize_t cm4_spi_set_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	char m[50];

	snprintf(m, sizeof(m), "status:0x%x\n\n", sensor->cm4_operate_data[5]);

	memset(sensor->cm4_operate_data, 0, sizeof(sensor->cm4_operate_data));

	return sprintf(buf, "\nDescription :\n"
		 "\techo \"bus_num set_op freq cs mode bit_per_word\" > cm4_operate\n"
		 "\tcat cm4_spi_set\n\n"
		 "Detail :\n"
		 "\tbus_num: 0: cm4 spi0\n"
		 "\tset_op: 3\n"
		 "\tfreq: spi frequency, for example 9: 9MHz, a: 10MHz, b: 11MHz\n"
		 "\tcs: spi chip_select num, default 0\n"
		 "\tmode: configure spi CPOL, CPHA, valid value: 0, 1, 2, or 3\n"
		 "\tbit_per_word: valid value: 8, 16 or 32\n\n"
		 "\tstatus: show execution result. 1:success 0:fail\n\n"
		 "%s\n", m);
}
static DEVICE_ATTR_RO(cm4_spi_set);

static ssize_t cm4_spi_sync_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	char l[50], m[50];

	snprintf(l, sizeof(l), "[cm4_operate]read_val1:0x%x read_val2:0x%x ",
		 sensor->cm4_operate_data[2],
		 sensor->cm4_operate_data[3]);

	snprintf(m, sizeof(m),
		 "read_val3:0x%x bytes:%u\n\n",
		 sensor->cm4_operate_data[4],
		 sensor->cm4_operate_data[5]);

	memset(sensor->cm4_operate_data, 0, sizeof(sensor->cm4_operate_data));

	return sprintf(buf, "\nDescription :\n"
		 "\techo \"op spi_sync reg_addr value1 value2 len\" > cm4_operate\n"
		 "\tcat cm4_spi_sync\n\n"
		 "Detail :\n"
		 "\top: 1:read 0:write\n"
		 "\tspi_sync: 4\n"
		 "\treg_addr: the reg to be read or written\n"
		 "\tvalue1: the first value to be written\n"
		 "\tvalue2: the second value to be written\n"
		 "\tlen: num of regs to be read or written\n\n"
		 "execution result:\n"
		 "%s%s\n", l, m);
}
static DEVICE_ATTR_RO(cm4_spi_sync);

static ssize_t sois_cmd_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	u8 sois_status[2];

	if (sensor->mcu_mode <= SHUB_CALIDOWNLOAD) {
		dev_err(&sensor->sensor_pdev->dev, "mcu_mode == SHUB_BOOT!\n");
		return -EINVAL;
	}

	if (shub_sipc_read(sensor, SHUB_GET_SOIS_CMD_SUBTYPE,
			sois_status, sizeof(sois_status)) < 0) {
		dev_err(&sensor->sensor_pdev->dev, "Read sois cmd Fail\n");
		return -EINVAL;
	}

	return sprintf(buf, "%d\n", sois_status[0]);
}

static ssize_t sois_cmd_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	struct sois_cmd sois = {0};
	int ret;

	if (sensor->mcu_mode <= SHUB_CALIDOWNLOAD) {
		dev_err(&sensor->sensor_pdev->dev, "mcu_mode == SHUB_BOOT!\n");
		return -EINVAL;
	}

	ret = sscanf(buf, "%hhu %hhu %hu %hu\n", &sois.cmd,
			&sois.mode, &sois.gyro_gain_x, &sois.gyro_gain_y);
	if (ret < 2 || ret > 6) {
		dev_err(&sensor->sensor_pdev->dev, "sois cmd mismatch! ret = %d\n", ret);
		return -EINVAL;
	}

	dev_info(&sensor->sensor_pdev->dev, "cmd=%d mode=%d gyro_gain_x=%d gyro_gain_y=%d",
		sois.cmd, sois.mode, sois.gyro_gain_x, sois.gyro_gain_y);

	if (shub_send_command(sensor, HANDLE_MAX,
			SHUB_SET_SOIS_CMD_SUBTYPE, (char *)&sois, 6) < 0) {
		dev_err(&sensor->sensor_pdev->dev, "Write sois cmd Fail\n");
		return -EINVAL;
	}

	return count;
}
static DEVICE_ATTR_RW(sois_cmd);

static ssize_t saralgo_step_threshold_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	if (sensor->mcu_mode <= SHUB_CALIDOWNLOAD) {
		dev_err(&sensor->sensor_pdev->dev, "mcu_mode == SHUB_BOOT!\n");
		return -1;
	}
	if(shub_send_command(sensor, HANDLE_MAX,
		SHUB_SET_SARALGO_STEP_THRESHOLD_SUBTYPE, buf, count) < 0) {
		dev_err(&sensor->sensor_pdev->dev, "Write saralgo step threshold Fail\n");
		return -1;
	}
	return count;
}
static DEVICE_ATTR_WO(saralgo_step_threshold);

static ssize_t mag_nfc_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct shub_data *sensor = dev_get_drvdata(dev);

	if (sensor->mcu_mode <= SHUB_CALIDOWNLOAD) {
		dev_err(&sensor->sensor_pdev->dev, "mcu_mode == SHUB_BOOT 1!\n");
		return -1;
	}
    dev_err(&sensor->sensor_pdev->dev,"boardid = %s\n",buf);
	if(shub_send_command(sensor, HANDLE_MAX,
		SHUB_SET_MAG_NFC_SUBTYPE, buf,count) < 0) {
		dev_err(&sensor->sensor_pdev->dev, "Write mag nfc Fail\n");
		return -1;
	}

	return count;
}
static DEVICE_ATTR_WO(mag_nfc);

static struct attribute *sensorhub_attrs[] = {
	&dev_attr_logctl.attr,
	&dev_attr_enable.attr,
	&dev_attr_batch.attr,
	&dev_attr_flush.attr,
	&dev_attr_mcu_mode.attr,
	&dev_attr_calibrator_cmd.attr,
	&dev_attr_calibrator_data.attr,
	&dev_attr_version.attr,
	&dev_attr_raw_data_als.attr,
	&dev_attr_raw_data_ps.attr,
	&dev_attr_sensor_info.attr,
	&dev_attr_sensorlist.attr,
	&dev_attr_virtual_handle.attr,
	&dev_attr_cm4_operate.attr,
	&dev_attr_cm4_spi_set.attr,
	&dev_attr_cm4_spi_sync.attr,
	&dev_attr_sois_cmd.attr,
	&dev_attr_saralgo_step_threshold.attr,
	&dev_attr_mag_nfc.attr,
	NULL,
};
ATTRIBUTE_GROUPS(sensorhub);

static int shub_notifier_fn(struct notifier_block *nb,
			    unsigned long action, void *data)
{
	struct shub_data *sensor = container_of(nb, struct shub_data,
			early_suspend);

	switch (action) {
	case PM_SUSPEND_PREPARE:
		cancel_delayed_work_sync(&sensor->time_sync_work);
		shub_synctimestamp(sensor);
		shub_send_ap_status(sensor, SHUB_SLEEP);
		break;
	case PM_POST_SUSPEND:
		shub_send_ap_status(sensor, SHUB_NORMAL);
		queue_delayed_work(sensor->driver_wq,
				   &sensor->time_sync_work, 0);
		break;
	default:
		break;
	}
	return 0;
}

static int shub_reboot_notifier_fn(struct notifier_block *nb, unsigned long action, void *data)
{
	struct shub_data *sensor = container_of(nb, struct shub_data, shub_reboot_notifier);
	int i = 0;

	for (i = 0; i < MAX_SENSOR_HANDLE; i++) {
		if (sensor_status[i]) {
			if (shub_send_command(sensor, i, sensor_status[i], NULL, 0) < 0)
				dev_err(&sensor->sensor_pdev->dev, "reboot write disable failed\n");
		}
	}
	return NOTIFY_OK;
}

/* iio device reg */
static void iio_trigger_work(struct irq_work *work)
{
	struct shub_data *mcu_data =
	    container_of(work, struct shub_data, iio_irq_work);
	iio_trigger_poll(mcu_data->trig);
}

static irqreturn_t shub_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct shub_data *mcu_data = iio_priv(indio_dev);

	mutex_lock(&mcu_data->mutex_lock);
	iio_trigger_notify_done(mcu_data->indio_dev->trig);
	mutex_unlock(&mcu_data->mutex_lock);
	return IRQ_HANDLED;
}

static const struct iio_buffer_setup_ops shub_iio_buffer_setup_ops = {

};

static void shub_pseudo_irq_enable(struct iio_dev *indio_dev)
{
	struct shub_data *mcu_data = iio_priv(indio_dev);

	atomic_cmpxchg(&mcu_data->pseudo_irq_enable, 0, 1);
}

static void shub_pseudo_irq_disable(struct iio_dev *indio_dev)
{
	struct shub_data *mcu_data = iio_priv(indio_dev);

	atomic_cmpxchg(&mcu_data->pseudo_irq_enable, 1, 0);
}

static void shub_set_pseudo_irq(struct iio_dev *indio_dev, int enable)
{
	if (enable)
		shub_pseudo_irq_enable(indio_dev);
	else
		shub_pseudo_irq_disable(indio_dev);
}

static int shub_data_rdy_trigger_set_state(struct iio_trigger *trig, bool state)
{
	struct iio_dev *indio_dev =
		iio_trigger_get_drvdata(trig);
	struct shub_data *mcu_data = iio_priv(indio_dev);

	mutex_lock(&mcu_data->mutex_lock);
	shub_set_pseudo_irq(indio_dev, state);
	mutex_unlock(&mcu_data->mutex_lock);
	return 0;
}

static const struct iio_trigger_ops shub_iio_trigger_ops = {
	.set_trigger_state = &shub_data_rdy_trigger_set_state,
};

static int shub_probe_trigger(struct iio_dev *iio_dev)
{
	struct shub_data *mcu_data = iio_priv(iio_dev);
	int ret;
	//dev_err(&pdev->dev, " 2222\n");

	iio_dev->pollfunc = iio_alloc_pollfunc(&iio_pollfunc_store_time,
					       &shub_trigger_handler,
					       IRQF_ONESHOT,
					       iio_dev,
					       "%s_consumer%d",
					       iio_dev->name, iio_device_id(iio_dev));
	if (!iio_dev->pollfunc)
		return -ENOMEM;

	mcu_data->trig =
	    iio_trigger_alloc(iio_dev->dev.parent, "%s-dev%d", iio_dev->name,
				iio_device_id(iio_dev));
	if (!mcu_data->trig) {
		iio_dealloc_pollfunc(iio_dev->pollfunc);
		return -ENOMEM;
	}
	mcu_data->trig->dev.parent = &mcu_data->sensor_pdev->dev;
	mcu_data->trig->ops = &shub_iio_trigger_ops;
	iio_trigger_set_drvdata(mcu_data->trig, iio_dev);

	ret = iio_trigger_register(mcu_data->trig);
	if (ret)
		goto error_free_trig;

	return 0;

error_free_trig:
	iio_trigger_free(mcu_data->trig);
	iio_dealloc_pollfunc(iio_dev->pollfunc);
	return ret;
}

static int shub_read_raw(struct iio_dev *iio_dev,
			 struct iio_chan_spec const *chan, int *val, int *val2,
			 long mask)
{
	struct shub_data *mcu_data = iio_priv(iio_dev);
	int ret = -EINVAL;

	if (chan->type != IIO_ACCEL)
		return ret;

	mutex_lock(&mcu_data->mutex_lock);
	switch (mask) {
	case 0:
		*val = mcu_data->iio_data[chan->channel2 - IIO_MOD_X];
		ret = IIO_VAL_INT;
		break;

	case IIO_CHAN_INFO_SCALE:
		/* Gain : counts / uT = 1000 [nT] */
		/* Scaling factor : 1000000 / Gain = 1000 */
		*val = 0;
		*val2 = 1000;
		ret = IIO_VAL_INT_PLUS_MICRO;
		break;

	default:
		break;
	}
	mutex_unlock(&mcu_data->mutex_lock);
	return ret;
}

#define IIO_ST(si, rb, sb, sh)						\
	{ .sign = si, .realbits = rb, .storagebits = sb, .shift = sh }

#define SHUB_CHANNEL(axis, bits) {                  \
	.type = IIO_ACCEL,	                    \
	.modified = 1,                          \
	.channel2 = (axis) + 1,                     \
	.scan_index = (axis),	                    \
	.scan_type = IIO_ST('u', (bits), (bits), 0)	    \
}

static const struct iio_chan_spec shub_channels[] = {
	SHUB_CHANNEL(SHUB_SCAN_ID, SHUB_IIO_CHN_BITS),
	SHUB_CHANNEL(SHUB_SCAN_RAW_0, SHUB_IIO_CHN_BITS),
	SHUB_CHANNEL(SHUB_SCAN_RAW_1, SHUB_IIO_CHN_BITS),
	SHUB_CHANNEL(SHUB_SCAN_RAW_2, SHUB_IIO_CHN_BITS),
	IIO_CHAN_SOFT_TIMESTAMP(SHUB_SCAN_TIMESTAMP),
	{
		.type = IIO_LIGHT,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.scan_index = -1,
	}
};

static long shub_allchannel_scan_masks[] = {
	/* timestamp channel is not managed by scan mask */
	BIT(SHUB_SCAN_ID) | BIT(SHUB_SCAN_RAW_0) |
	BIT(SHUB_SCAN_RAW_1) | BIT(SHUB_SCAN_RAW_2),
	0x00000000
};

static const struct iio_info shub_iio_info = {
	.read_raw = &shub_read_raw,
};


static int create_sysfs_interfaces(struct shub_data *mcu_data)
{
	int ret;

	mcu_data->sensor_class = class_create(THIS_MODULE, "sprd_sensorhub");
	if (IS_ERR(mcu_data->sensor_class))
		return PTR_ERR(mcu_data->sensor_class);

	mcu_data->sensor_dev =
	    device_create(mcu_data->sensor_class, NULL, 0, "%s", "sensor_hub");
	if (IS_ERR(mcu_data->sensor_dev)) {
		ret = PTR_ERR(mcu_data->sensor_dev);
		goto err_device_create;
	}
	debugfs_create_symlink("sensor", NULL,
			       "/sys/class/sprd_sensorhub/sensor_hub");

	dev_set_drvdata(mcu_data->sensor_dev, mcu_data);

	ret = sysfs_create_groups(&mcu_data->sensor_dev->kobj,
				  sensorhub_groups);
	if (ret) {
		dev_err(mcu_data->sensor_dev, "failed to create sysfs device attributes\n");
		goto error;
	}

	ret = sysfs_create_link(&mcu_data->sensor_dev->kobj,
				&mcu_data->indio_dev->dev.kobj, "iio");
	if (ret < 0)
		goto error;

	return 0;

error:
	put_device(mcu_data->sensor_dev);
	device_unregister(mcu_data->sensor_dev);
err_device_create:
	class_destroy(mcu_data->sensor_class);
	return ret;
}

static void shub_remove_trigger(struct iio_dev *indio_dev)
{
	struct shub_data *mcu_data = iio_priv(indio_dev);

	iio_trigger_unregister(mcu_data->trig);
	iio_trigger_free(mcu_data->trig);
	iio_dealloc_pollfunc(indio_dev->pollfunc);
}

static void shub_remove_buffer(struct iio_dev *indio_dev)
{
	iio_kfifo_free(indio_dev->buffer);
}

#if MAG_CHARGING_MODE
static int send_current_now(struct shub_data *sensor,int sensor_type, const char *data, int length)
{
	if (sensor->mcu_mode <= SHUB_CALIDOWNLOAD) {
		dev_err(&sensor->sensor_pdev->dev, "mcu_mode == SHUB_BOOT 1!\n");
		return -1;
	}

	if(shub_send_command(sensor, SENSOR_GEOMAGNETIC_FIELD,
		SHUB_SET_MAG_CHARGER_SUBTYPE, data,length) < 0) {
		dev_err(&sensor->sensor_pdev->dev, "Write mag chargerFail\n");
		return -1;
	}
	return 0;
}
static void mag_current_now_work_func(struct work_struct *work)
{
	struct shub_data *sensor = container_of((struct delayed_work *)work,
	                           struct shub_data,mag_current_now_work);
	int ret = 0;
	char buf[1024] = {0,};
	struct power_supply *bat_psy;
	union power_supply_propval propval = {0, };
	bat_psy = power_supply_get_by_name("battery");
	if (!bat_psy) {
		dev_err(&sensor->sensor_pdev->dev,"get battery psy failed\n");
		return;
	}
	ret = power_supply_get_property(bat_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &propval);
	if(ret) {
		dev_err(&sensor->sensor_pdev->dev,"bat_psy is null or error !\n");
		return;
	}
	sensor->mag_current_now = propval.intval;
	sprintf(buf,"%d",sensor->mag_current_now);
	ret = send_current_now(sensor,SENSOR_GEOMAGNETIC_FIELD,buf,sizeof(buf));
	if (ret < 0) {
		dev_err(&sensor->sensor_pdev->dev,"%s failed!!\n", __func__);
		return;
	}
	dev_info(&sensor->sensor_pdev->dev,"%s: battery_curr_now = %d\n", __func__, sensor->mag_current_now);
	schedule_delayed_work(&sensor->mag_current_now_work, msecs_to_jiffies(2000));
}
static int mag_current_now_notifier_callback(struct notifier_block *nb, unsigned long ev, void *v)
{
	int err = 0;
	struct shub_data *sensor= container_of(nb, struct shub_data, current_now_nb);
	if (ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_DONE;
	err = schedule_delayed_work(&sensor->mag_current_now_work, 0);
	if (err < 0) {
		dev_err(&sensor->sensor_pdev->dev,"%s is failed!!\n", __func__);
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}
#endif

static int shub_probe(struct platform_device *pdev)
{
	struct shub_data *mcu;
	struct iio_dev *indio_dev;
	int error;
	indio_dev = iio_device_alloc(&pdev->dev, sizeof(*mcu));
	if (!indio_dev) {
		dev_err(&pdev->dev, " iio_device_alloc failed\n");
		return -ENOMEM;
	}

	indio_dev->name = SHUB_NAME;
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->info = &shub_iio_info;
	indio_dev->channels = shub_channels;
	indio_dev->num_channels = ARRAY_SIZE(shub_channels);

	mcu = iio_priv(indio_dev);
	mcu->sensor_pdev = pdev;
	mcu->indio_dev = indio_dev;
	g_sensor = mcu;

	error = of_property_read_u32(pdev->dev.of_node,
		"sipc-sensorhub-id", &(mcu->sipc_sensorhub_id));
	if (error) {
		mcu->sipc_sensorhub_id = SIPC_ID_PM_SYS;
	}
	dev_info(&pdev->dev, "sipc_sensorhub_id=%u\n", mcu->sipc_sensorhub_id);

	mcu->mcu_mode = SHUB_BOOT;
	dev_set_drvdata(&pdev->dev, mcu);

	mcu->data_callback = shub_data_callback;
	mcu->save_mag_offset = shub_save_mag_offset;
	mcu->readcmd_callback = shub_readcmd_callback;
	mcu->cm4_read_callback = shub_cm4_read_callback;
	init_waitqueue_head(&mcu->rxwq);

	mutex_init(&mcu->mutex_lock);
	mutex_init(&mcu->mutex_read);
	mutex_init(&mcu->mutex_send);
	mutex_init(&mcu->send_command_mutex);

	indio_dev->modes |= INDIO_BUFFER_TRIGGERED;
	error = devm_iio_kfifo_buffer_setup(&pdev->dev, indio_dev,
					  INDIO_BUFFER_TRIGGERED,
					  &shub_iio_buffer_setup_ops);
	if (error) {
		dev_err(&pdev->dev, " iio_kfifo_buffer_setup failed\n");
		goto error_free_dev;
	}
	indio_dev->buffer->scan_mask = shub_allchannel_scan_masks;
	indio_dev->buffer->scan_timestamp = true;

	error = shub_probe_trigger(indio_dev);
	if (error) {
		dev_err(&pdev->dev,
			" shub_probe_trigger failed\n");
		goto error_remove_buffer;
	}
	error = devm_iio_device_register(&pdev->dev, indio_dev);
	if (error) {
		dev_err(&pdev->dev, "iio_device_register failed\n");
		goto error_remove_trigger;
	}

	error = create_sysfs_interfaces(mcu);
	if (error)
		goto err_free_mem;

	sensorhub_wake_lock = wakeup_source_create("sensorhub_wake_lock");
	wakeup_source_add(sensorhub_wake_lock);

	/* init time sync and download firmware work */
	INIT_DELAYED_WORK(&mcu->time_sync_work, shub_synctime_work);
	mcu->driver_wq = create_singlethread_workqueue("sensorhub_daemon");
	if (!mcu->driver_wq) {
		error = -ENOMEM;
		goto err_free_mem;
	}

	init_irq_work(&mcu->iio_irq_work, iio_trigger_work);

	thread = kthread_run(shub_read_thread, mcu, SHUB_RD);
	if (IS_ERR(thread)) {
		error = PTR_ERR(thread);
		dev_warn(&pdev->dev,
			 "failed to create kernel thread: %d\n", error);
	}

	thread_nwu = kthread_run(shub_read_thread_nwu, mcu, SHUB_RD_NWU);
	if (IS_ERR(thread_nwu)) {
		error = PTR_ERR(thread_nwu);
		dev_warn(&pdev->dev,
			 "failed to create kernel thread_nwu: %d\n", error);
	}
	mcu->early_suspend.notifier_call = shub_notifier_fn;
	register_pm_notifier(&mcu->early_suspend);
	mcu->shub_reboot_notifier.notifier_call = shub_reboot_notifier_fn;
	register_reboot_notifier(&mcu->shub_reboot_notifier);

#if MAG_CHARGING_MODE
	INIT_DELAYED_WORK(&mcu->mag_current_now_work, mag_current_now_work_func);
	mcu->current_now_nb.notifier_call = mag_current_now_notifier_callback;
	error = power_supply_reg_notifier(&mcu->current_now_nb);
	if (error < 0) {
		dev_err(&pdev->dev,"Couldn't register psy notifier ret = %d\n", error);
		goto err_register_power_usb_notifier_failed;
	}
#endif

	return 0;

#if MAG_CHARGING_MODE
	power_supply_unreg_notifier(&mcu->current_now_nb);
err_register_power_usb_notifier_failed:
	mcu->current_now_nb.notifier_call = NULL;
	cancel_delayed_work_sync(&mcu->mag_current_now_work);
#endif

err_free_mem:
	iio_device_unregister(indio_dev);
error_remove_trigger:
	shub_remove_trigger(indio_dev);
error_remove_buffer:
	shub_remove_buffer(indio_dev);
error_free_dev:
	iio_device_free(indio_dev);

	return error;
}

static int shub_remove(struct platform_device *pdev)
{
	struct shub_data *mcu = platform_get_drvdata(pdev);
	struct iio_dev *indio_dev = mcu->indio_dev;

#if MAG_CHARGING_MODE
	power_supply_unreg_notifier(&mcu->current_now_nb);
	mcu->current_now_nb.notifier_call = NULL;
	cancel_delayed_work_sync(&mcu->mag_current_now_work);
#endif
	unregister_pm_notifier(&mcu->early_suspend);
	if (!IS_ERR_OR_NULL(thread))
		kthread_stop(thread);
	if (!IS_ERR_OR_NULL(thread_nwu))
		kthread_stop(thread_nwu);
	iio_device_unregister(indio_dev);
	shub_remove_trigger(indio_dev);
	shub_remove_buffer(indio_dev);
	iio_device_free(indio_dev);
	kfree(mcu);

	return 0;
}

static const struct of_device_id shub_match_table[] = {
	{.compatible = "sprd,sensor-hub",},
	{},
};

static struct platform_driver shub_driver = {
	.probe = shub_probe,
	.remove = shub_remove,
	.driver = {
		   .name = SHUB_NAME,
		   .of_match_table = shub_match_table,
	},
};

module_platform_driver(shub_driver);

MODULE_DESCRIPTION("Spreadtrum Sensor Hub");
MODULE_LICENSE("GPL v2");
