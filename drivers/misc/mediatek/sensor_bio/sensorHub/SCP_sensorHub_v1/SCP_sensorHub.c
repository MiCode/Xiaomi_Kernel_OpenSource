/* SCP sensor hub driver
 *
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2011 Bosch Sensortec GmbH
 * All Rights Reserved
 */

#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/atomic.h>
#include <linux/types.h>
#include <linux/module.h>
#include <scp_ipi.h>
#include "scp_helper.h"
#include "scp_excep.h"
#include <linux/time.h>
#include "cust_sensorHub.h"
#include "hwmsensor.h"
#include "hwmsen_dev.h"
#include "sensors_io.h"
#include "SCP_sensorHub.h"
#include "hwmsen_helper.h"
#include "sensor_event.h"
#define SENSOR_DATA_SIZE 48
/*
 * experience number for delay_count per DELAY_COUNT sensor input delay 10ms
 * msleep(20) system will schedule to hal process then read input node
 */
#define DELAY_COUNT			32
#define SCP_sensorHub_DEV_NAME        "SCP_sensorHub"
static int scp_sensorHub_power_adjust(void);
static int sensor_send_ap_timetamp(void);
static int SCP_sensorHub_server_dispatch_data(void);
static int SCP_sensorHub_report_data(struct data_unit_t *data_t);
typedef enum {
	SCP_TRC_FUN = 0x01,
	SCP_TRC_IPI = 0x02,
	SCP_TRC_BATCH = 0x04,
	SCP_TRC_BATCH_DETAIL = 0x08,
} SCP_TRC;
SCP_sensorHub_handler sensor_handler[ID_SENSOR_MAX_HANDLE + 1];
static DEFINE_MUTEX(SCP_sensorHub_op_mutex);
static DEFINE_MUTEX(SCP_sensorHub_req_mutex);
static DEFINE_MUTEX(SCP_sensorHub_report_data_mutex);
static DECLARE_WAIT_QUEUE_HEAD(SCP_sensorHub_req_wq);

static int SCP_sensorHub_init_flag = -1;
struct SCP_sensorHub_data {
	struct sensorHub_hw *hw;
	struct work_struct ipi_work;
	struct work_struct fifo_full_work;

	struct work_struct batch_timeout_work;
	struct work_struct direct_push_work;
	struct work_struct power_notify_work;
	struct timer_list timer;
	struct timer_list notify_timer;
	/*misc */
	atomic_t trace;
	atomic_t suspend;
	atomic_t wait_rsp;
	atomic_t ipi_handler_running;
	atomic_t disable_fifo_full_notify;

	volatile struct sensorFIFO *volatile SCP_sensorFIFO;
	volatile struct sensorFIFO *volatile SCP_directPush_FIFO;
	phys_addr_t shub_dram_phys;
	phys_addr_t shub_dram_virt;
	phys_addr_t shub_direct_push_dram_phys;
	phys_addr_t shub_direct_push_dram_virt;
};

static struct device SCP_sensorHub_dev = {
	.init_name = "SCPdmadev",
	.coherent_dma_mask = ~0,	/* dma_alloc_coherent(): allow any address */
	.dma_mask = &SCP_sensorHub_dev.coherent_dma_mask,	/* other APIs: use the same mask as coherent */
};

static struct SCP_sensorHub_data *obj_data;
static SCP_SENSOR_HUB_DATA_P userData;
static uint *userDataLen;
#define SCP_TAG                  "[sensorHub] "
#define SCP_FUN(f)               pr_debug(SCP_TAG"%s\n", __func__)
#define SCP_ERR(fmt, args...)    pr_err(SCP_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define SCP_LOG(fmt, args...)    pr_debug(SCP_TAG fmt, ##args)

#define SENSOR_SCP_AP_TIME_SYNC

#ifdef SENSOR_SCP_AP_TIME_SYNC

#define SYNC_CYCLC 10		/*600s */

struct timer_list sync_timer;
struct work_struct syncwork;

static void syncwork_fun(struct work_struct *work)
{
	sensor_send_ap_timetamp();
	mod_timer(&sync_timer, jiffies + SYNC_CYCLC * HZ);
}

static void sync_timeout(unsigned long data)
{
	schedule_work(&syncwork);
}

#endif

static unsigned long long SCP_sensorHub_GetCurNS(void)
{
/*
    int64_t  nt;
    struct timespec time;

    time.tv_sec = 0;
    time.tv_nsec = 0;
    get_monotonic_boottime(&time);
    nt = time.tv_sec*1000000000LL+time.tv_nsec;
*/

	return sched_clock();
}

static int SCP_sensorHub_get_scp_semaphore(void)
{
	int64_t start_nt, cur_nt;
	struct timespec time;
	int err;

	time.tv_sec = 0;
	time.tv_nsec = 0;
	get_monotonic_boottime(&time);
	start_nt = time.tv_sec * 1000000000LL + time.tv_nsec;

	do {
		err = get_scp_semaphore(SEMAPHORE_SENSOR);
		if (err < 0) {
			time.tv_sec = 0;
			time.tv_nsec = 0;
			get_monotonic_boottime(&time);
			cur_nt = time.tv_sec * 1000000000LL + time.tv_nsec;
			SCP_ERR("get_scp_semaphore fail : %d, %lld, %lld\n", err, start_nt, cur_nt);
		} else {
			return err;
		}
	} while ((cur_nt - start_nt) < 20000000);	/* try 10 ms to get hw semaphore */

	SCP_ERR("get_scp_semaphore timeout : %d, %lld, %lld\n", err, start_nt, cur_nt);
	return err;
}

static int SCP_sensorHub_init_client(void)
{				/* call by init done workqueue */
	struct SCP_sensorHub_data *obj = obj_data;
	SCP_SENSOR_HUB_DATA data;
	unsigned int len = 0;

	obj->shub_dram_phys = get_reserve_mem_phys(SENS_MEM_ID);
	obj->shub_dram_virt = get_reserve_mem_virt(SENS_MEM_ID);
	obj->shub_direct_push_dram_phys = get_reserve_mem_phys(SENS_MEM_DIRECT_ID);
	obj->shub_direct_push_dram_virt = get_reserve_mem_virt(SENS_MEM_DIRECT_ID);

	data.set_config_req.sensorType = 0;
	data.set_config_req.action = SENSOR_HUB_SET_CONFIG;
	data.set_config_req.bufferBase = (unsigned int)(obj->shub_dram_phys & 0xFFFFFFFF);
	data.set_config_req.bufferSize = get_reserve_mem_size(SENS_MEM_ID);

	data.set_config_req.directPushbufferBase =
	    (unsigned int)(obj->shub_direct_push_dram_phys & 0xFFFFFFFF);
	data.set_config_req.directPushbufferSize = SCP_DIRECT_PUSH_FIFO_SIZE;
	len = sizeof(data.set_config_req);

	SCP_sensorHub_req_send(&data, &len, 1);
	sensor_send_ap_timetamp();
	SCP_ERR("fwq SCP_sensorHub_init_client done\n");

	return SCP_SENSOR_HUB_SUCCESS;
}
static unsigned long long t1, t2, t3, t4, t5, t6;
int SCP_sensorHub_req_send(SCP_SENSOR_HUB_DATA_P data, uint *len, unsigned int wait)
{
	ipi_status status;
	int err = 0;
	int retry = 0;

	if (SCP_TRC_IPI & atomic_read(&(obj_data->trace)))
		SCP_ERR("len = %d, type = %d, action = %d\n", *len, data->req.sensorType,
			data->req.action);

	if (*len > 48) {
		SCP_ERR("!!\n");
		return -1;
	}

	if (in_interrupt()) {
		SCP_ERR("Can't do %s in interrupt context!!\n", __func__);
		return -1;
	}

	if (ID_SENSOR_MAX_HANDLE < data->rsp.sensorType) {
		SCP_ERR("SCP_sensorHub_IPI_handler invalid sensor type %d\n", data->rsp.sensorType);
		return -1;
	}
	mutex_lock(&SCP_sensorHub_req_mutex);

	userData = data;
	userDataLen = len;

	switch (data->req.action) {
	case SENSOR_HUB_ACTIVATE:
		break;
	case SENSOR_HUB_SET_DELAY:
		break;
	case SENSOR_HUB_GET_DATA:
		break;
	case SENSOR_HUB_BATCH:
		break;
	case SENSOR_HUB_SET_CONFIG:
		break;
	case SENSOR_HUB_SET_TIMESTAMP:
		break;
	case SENSOR_HUB_MASK_NOTIFY:
		break;
	case SENSOR_HUB_BATCH_TIMEOUT:
		break;
	case SENSOR_HUB_SET_CUST:
		break;
	default:
		break;
	}

	if (1 == wait) {
		if (atomic_read(&(obj_data->wait_rsp)) == 1)
			SCP_ERR("SCP_sensorHub_req_send reentry\n");
		atomic_set(&(obj_data->wait_rsp), 1);
	}
	mod_timer(&obj_data->timer, jiffies + 500 * HZ / 1000);

	do {
		status = scp_ipi_send(IPI_SENSOR, data, *len, 0);
		if (ERROR == status) {
			SCP_ERR("scp_ipi_send ERROR\n");
			goto SCP_FAIL;
		}
		if (BUSY == status) {
			if (retry++ == 1000)
				goto SCP_FAIL;
			if (retry % 100 == 0) {
				SCP_ERR("scp_ipi_send retry time is %d\n", retry);
				udelay(10);
			}
		}
	} while (BUSY == status);
	if (SCP_TRC_IPI & atomic_read(&(obj_data->trace)))
		SCP_ERR("scp_ipi_send DONE\n");
	wait_event(SCP_sensorHub_req_wq, (atomic_read(&(obj_data->wait_rsp)) == 0));
	del_timer_sync(&obj_data->timer);
	atomic_set(&(obj_data->wait_rsp), 0);
	err = userData->rsp.errCode;
/*
	if (t6 - t1 > 3000000LL)
		SCP_ERR("%llu, %llu, %llu, %llu, %llu, %llu\n", t1, t2, t3, t4, t5, t6);
	*/
	mutex_unlock(&SCP_sensorHub_req_mutex);

	if (SCP_TRC_IPI & atomic_read(&(obj_data->trace)))
		SCP_ERR("SCP_sensorHub_req_send end\n");
	return err;

SCP_FAIL:
	del_timer_sync(&obj_data->timer);
	mutex_unlock(&SCP_sensorHub_req_mutex);
	atomic_set(&(obj_data->wait_rsp), 0);
	return -1;
}

int SCP_sensorHub_data_registration(uint8_t sensor, SCP_sensorHub_handler handler)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	if (ID_SENSOR_MAX_HANDLE < sensor)
		SCP_ERR("SCP_sensorHub_rsp_registration invalid sensor %d\n", sensor);

	if (NULL == handler)
		SCP_ERR("SCP_sensorHub_rsp_registration null handler\n");

	sensor_handler[sensor] = handler;

	return 0;
}

static void SCP_power_notify_work(struct work_struct *work)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();
	scp_sensorHub_power_adjust();
}
static void SCP_ipi_work(struct work_struct *work)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	SCP_sensorHub_init_client();
}

static void SCP_fifo_full_work(struct work_struct *work)
{
	if (SCP_TRC_FUN & atomic_read(&(obj_data->trace)))
		SCP_FUN();

	mutex_lock(&SCP_sensorHub_report_data_mutex);
	SCP_sensorHub_server_dispatch_data();
	mutex_unlock(&SCP_sensorHub_report_data_mutex);
}

static void SCP_batch_timeout_work(struct work_struct *work)
{
	if (SCP_TRC_FUN & atomic_read(&(obj_data->trace)))
		SCP_FUN();

	mutex_lock(&SCP_sensorHub_report_data_mutex);
	SCP_sensorHub_server_dispatch_data();
	mutex_unlock(&SCP_sensorHub_report_data_mutex);
}

static void SCP_direct_push_work(struct work_struct *work)
{
	if (SCP_TRC_FUN & atomic_read(&(obj_data->trace)))
		SCP_FUN();

	mutex_lock(&SCP_sensorHub_report_data_mutex);
	SCP_sensorHub_server_dispatch_data();
	mutex_unlock(&SCP_sensorHub_report_data_mutex);
}

static void SCP_sensorHub_req_send_timeout(unsigned long data)
{
	if (atomic_read(&(obj_data->wait_rsp)) == 1) {
		if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
			SCP_FUN();

		if (NULL != userData && NULL != userDataLen) {
			userData->rsp.errCode = -1;
			*userDataLen = sizeof(userData->rsp);
		}
		SCP_ERR("SCP_sensorHub_req_send_timeout\n");
		atomic_set(&(obj_data->wait_rsp), 0);
		wake_up(&SCP_sensorHub_req_wq);
	}
}

static void SCP_sensorHub_IPI_handler(int id, void *data, unsigned int len)
{
	struct SCP_sensorHub_data *obj = obj_data;
	SCP_SENSOR_HUB_DATA_P rsp = (SCP_SENSOR_HUB_DATA_P) data;
	struct data_unit_t *event;
	bool wake_up_req = false;
	bool do_registed_handler = false;
	static int first_init_done;

	t1 = SCP_sensorHub_GetCurNS();

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	if (SCP_TRC_IPI & atomic_read(&(obj_data->trace)))
		SCP_ERR("len = %d, type = %d, action = %d, errCode = %d\n", len,
			rsp->rsp.sensorType, rsp->rsp.action, rsp->rsp.errCode);

	if (len > 48) {
		SCP_ERR("SCP_sensorHub_IPI_handler len=%d error\n", len);
		return;
	}
	switch (rsp->rsp.action) {
	case SENSOR_HUB_ACTIVATE:
	case SENSOR_HUB_SET_DELAY:
	case SENSOR_HUB_GET_DATA:
	case SENSOR_HUB_BATCH:
	case SENSOR_HUB_SET_CONFIG:
	case SENSOR_HUB_SET_CUST:
	case SENSOR_HUB_BATCH_TIMEOUT:
	case SENSOR_HUB_SET_TIMESTAMP:
	case SENSOR_HUB_MASK_NOTIFY:
		wake_up_req = true;
		break;
	case SENSOR_HUB_NOTIFY:
		switch (rsp->notify_rsp.event) {
		case SCP_INIT_DONE:
			if (0 == first_init_done) {
				schedule_work(&obj->ipi_work);
				first_init_done = 1;
			}
			do_registed_handler = true;
			break;
		case SCP_FIFO_FULL:
			schedule_work(&obj->fifo_full_work);
			break;
		case SCP_BATCH_TIMEOUT:
			schedule_work(&obj->batch_timeout_work);
			break;
		case SCP_DIRECT_PUSH:
			schedule_work(&obj->direct_push_work);
			break;
		case SCP_NOTIFY:
			do_registed_handler = true;
			break;
		default:
			break;
		}
		break;
	case SENSOR_HUB_POWER_NOTIFY:
		schedule_work(&obj->power_notify_work);
		break;
	default:
		SCP_ERR("SCP_sensorHub_IPI_handler unknown action=%d error\n", rsp->rsp.action);
		return;
	}

	t2 = SCP_sensorHub_GetCurNS();

	if (ID_SENSOR_MAX_HANDLE < rsp->rsp.sensorType) {
		if (rsp->rsp.sensorType != ID_SCP_MAX_SENSOR_TYPE) {
			SCP_ERR("SCP_sensorHub_IPI_handler invalid sensor type %d\n",
				rsp->rsp.sensorType);
			return;
		}
	} else if (true == do_registed_handler) {
		event = (struct data_unit_t *)rsp->notify_rsp.data.int8_Data;
		if (NULL != sensor_handler[rsp->rsp.sensorType])
			sensor_handler[rsp->rsp.sensorType](event, NULL);
		if (rsp->rsp.sensorType == ID_TILT_DETECTOR)
			sensor_handler[ID_WAKE_GESTURE](event, NULL);
	}

	t3 = SCP_sensorHub_GetCurNS();

	if (atomic_read(&(obj_data->wait_rsp)) == 1 && true == wake_up_req) {
		if (NULL == userData || NULL == userDataLen) {
			SCP_ERR("SCP_sensorHub_IPI_handler null pointer\n");
		} else {
			if (userData->req.sensorType != rsp->rsp.sensorType)
				SCP_ERR("SCP_sensorHub_IPI_handler sensor type %d != %d\n",
					userData->req.sensorType, rsp->rsp.sensorType);
			if (userData->req.action != rsp->rsp.action)
				SCP_ERR("SCP_sensorHub_IPI_handler action %d != %d\n",
					userData->req.action, rsp->rsp.action);
			memcpy(userData, rsp, len);
			*userDataLen = len;
		}
		t4 = SCP_sensorHub_GetCurNS();
		atomic_set(&(obj_data->wait_rsp), 0);
		t5 = SCP_sensorHub_GetCurNS();
		wake_up(&SCP_sensorHub_req_wq);
		t6 = SCP_sensorHub_GetCurNS();
	}

}

static int SCP_sensorHub_batch(int handle, int enable, int flag, long long samplingPeriodNs,
				  long long maxBatchReportLatencyNs)
{
	SCP_SENSOR_HUB_DATA req;
	int len;
	int err = 0;

	do_div(maxBatchReportLatencyNs, 1000000);
	do_div(samplingPeriodNs, 1000000);
	req.batch_req.sensorType = handle;
	req.batch_req.action = SENSOR_HUB_BATCH;
	req.batch_req.flag = flag;
	req.batch_req.period_ms = (unsigned int)samplingPeriodNs;
	req.batch_req.timeout_ms = (unsigned int)maxBatchReportLatencyNs;
	len = sizeof(req.batch_req);
	err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err)
		SCP_ERR("SCP_sensorHub_req_send fail!\n");

	return err;
}

static int SCP_sensorHub_flush(int handle)
{
	SCP_SENSOR_HUB_DATA req;
	int len;
	int err = 0;

	req.batch_req.sensorType = handle;
	req.batch_req.action = SENSOR_HUB_BATCH_TIMEOUT;
	len = sizeof(req.batch_req);
	err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err)
		SCP_ERR("SCP_sensorHub_flush fail!\n");

	return err;
}

static int SCP_sensorHub_report_data(struct data_unit_t *data_t)
{
	int err = 0, sensor_type = 0;
	int64_t timestamp_ms = 0;
	static int64_t last_timestamp_ms[ID_SENSOR_MAX_HANDLE + 1];
	bool need_send = false;

	/* int64_t now_enter_timestamp = 0;
	struct timespec time;

	time.tv_sec = time.tv_nsec = 0;
	get_monotonic_boottime(&time);
	now_enter_timestamp = time.tv_sec * 1000000000LL + time.tv_nsec;
	SCP_ERR("type:%d,now time:%lld, scp time: %lld\n",
		data_t->sensor_type, now_enter_timestamp, (data_t->time_stamp + data_t->time_stamp_gpt)); */

	sensor_type = data_t->sensor_type;
	if (ID_SENSOR_MAX_HANDLE < sensor_type)
		SCP_ERR("invalid sensor %d\n", sensor_type);
	else {
		if (NULL == sensor_handler[sensor_type]) {
			SCP_ERR("type:%d don't support this flow?\n", sensor_type);
			return 0;
		}
		if (data_t->flush_action != DATA_ACTION)
			need_send = true;
		else {
			/* timestamp filter, drop events which timestamp equal to each other at 1 ms */
			timestamp_ms = (int64_t)(data_t->time_stamp + data_t->time_stamp_gpt) / 1000000;
			if (last_timestamp_ms[sensor_type] != timestamp_ms) {
				last_timestamp_ms[sensor_type] = timestamp_ms;
				need_send = true;
			} else
				need_send = false;
		}
		if (need_send == true)
			err = sensor_handler[sensor_type](data_t, NULL);
	}
	return err;
}
static int SCP_sensorHub_server_dispatch_data(void)
{
	struct SCP_sensorHub_data *obj = obj_data;
	char *pStart, *pEnd, *rp, *wp;
	struct data_unit_t event;
	int err = 0;

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	/* To prevent get fifo status during scp wrapper around dram fifo. */
	err = SCP_sensorHub_get_scp_semaphore();
	if (err < 0) {
		SCP_ERR("SCP_sensorHub_get_scp_semaphore fail : %d\n", err);
		return -2;
	}

	pStart = (char *)obj->SCP_sensorFIFO + offsetof(struct sensorFIFO, data);
	pEnd = pStart + obj->SCP_sensorFIFO->FIFOSize;

	rp = pStart + obj->SCP_sensorFIFO->rp;
	wp = pStart + obj->SCP_sensorFIFO->wp;


	if (wp < pStart || pEnd < wp) {
		SCP_ERR("FIFO wp invalid : %p, %p, %p\n", pStart, pEnd, wp);
		err = release_scp_semaphore(SEMAPHORE_SENSOR);
		if (err < 0) {
			SCP_ERR("release_scp_semaphore fail : %d\n", err);
			return -3;
		}
		return -5;
	}

	if (rp == wp) {
		SCP_ERR("FIFO empty\n");
		err = release_scp_semaphore(SEMAPHORE_SENSOR);
		if (err < 0) {
			SCP_ERR("release_scp_semaphore fail : %d\n", err);
			return -3;
		}
		return 0;
	}

	if (rp < wp) {
		while (rp < wp) {
			memcpy_fromio(&event, rp, SENSOR_DATA_SIZE);
			SCP_sensorHub_report_data(&event);
			rp += SENSOR_DATA_SIZE;
		}
	} else if (rp > wp) {
		while (rp < pEnd) {
			memcpy_fromio(&event, rp, SENSOR_DATA_SIZE);
			SCP_sensorHub_report_data(&event);
			rp += SENSOR_DATA_SIZE;
		}
		rp = pStart;
		while (rp < wp) {
			memcpy_fromio(&event, rp, SENSOR_DATA_SIZE);
			SCP_sensorHub_report_data(&event);
			rp += SENSOR_DATA_SIZE;
		}
	}
	obj->SCP_sensorFIFO->rp = obj->SCP_sensorFIFO->wp;

	if (SCP_TRC_BATCH & atomic_read(&(obj_data->trace))) {
		SCP_ERR("FIFO pStart = %p, rp = %x, wp = %x, pEnd = %p\n", pStart,
			obj->SCP_sensorFIFO->rp, obj->SCP_sensorFIFO->wp, pEnd);
	}
	err = release_scp_semaphore(SEMAPHORE_SENSOR);
	if (err < 0) {
		SCP_ERR("release_scp_semaphore fail : %d\n", err);
		return -3;
	}
	return 0;
}

/* when all sensor don't enable, we adjust scp lower power */
int sensor_send_ap_timetamp(void)
{
	SCP_SENSOR_HUB_DATA req;
	int len;
	int err = 0;
	uint64_t ns;
	struct timespec time;

	time.tv_sec = time.tv_nsec = 0;
	get_monotonic_boottime(&time);
	ns = time.tv_sec * 1000000000LL + time.tv_nsec;
	req.set_config_req.sensorType = 0;
	req.set_config_req.action = SENSOR_HUB_SET_TIMESTAMP;
	req.set_config_req.ap_timestamp = ns;
	len = sizeof(req.set_config_req);
	err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err < 0)
		SCP_ERR("SCP_sensorHub_req_send fail!\n");
	return err;
}

static int scp_sensorHub_power_adjust(void)
{
	deregister_feature(SENS_FEATURE_ID);

	return 0;
}

int sensor_enable_to_hub(uint8_t sensorType, int enabledisable)
{
	SCP_SENSOR_HUB_DATA req;
	int len;
	int err = 0;

	if (enabledisable == 1)
		register_feature(SENS_FEATURE_ID);
	req.activate_req.sensorType = sensorType;
	req.activate_req.action = SENSOR_HUB_ACTIVATE;
	req.activate_req.enable = enabledisable;
	len = sizeof(req.activate_req);
	err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err < 0)
		SCP_ERR("SCP_sensorHub_req_send fail!\n");

	return err;
}

int sensor_set_delay_to_hub(uint8_t sensorType, unsigned int delayms)
{
	SCP_SENSOR_HUB_DATA req;
	int len;
	int err = 0;

	req.set_delay_req.sensorType = sensorType;
	req.set_delay_req.action = SENSOR_HUB_SET_DELAY;
	req.set_delay_req.delay = delayms;
	len = sizeof(req.set_delay_req);
	err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err < 0)
		SCP_ERR("SCP_sensorHub_req_send fail!\n");

	return err;
}
int sensor_batch_to_hub(uint8_t sensorType, int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
	int ret = 0;

	if (ID_SENSOR_MAX_HANDLE < sensorType) {
		SCP_ERR("invalid sensor %d\n", sensorType);
		ret = -1;
	} else
		ret = SCP_sensorHub_batch(sensorType, 0, flag, samplingPeriodNs, maxBatchReportLatencyNs);
	return ret;
}

int sensor_flush_to_hub(uint8_t sensorType)
{
	int ret = 0;

	if (ID_SENSOR_MAX_HANDLE < sensorType) {
		SCP_ERR("invalid sensor %d\n", sensorType);
		ret = -1;
	} else
		ret = SCP_sensorHub_flush(sensorType);
	return ret;
}

int sensor_cfg_to_hub(uint8_t sensorType, uint8_t *data, uint8_t count)
{
	int ret = 0;

	return ret;
}

int sensor_get_data_from_hub(uint8_t sensorType, struct data_unit_t *data)
{
	SCP_SENSOR_HUB_DATA req;
	struct data_unit_t *data_t;
	int len = 0, err = 0;

	req.get_data_req.sensorType = sensorType;
	req.get_data_req.action = SENSOR_HUB_GET_DATA;
	len = sizeof(req.get_data_req);
	err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err < 0) {
		SCP_ERR("SCP_sensorHub_req_send fail!\n");
		return -1;
	}
	if (sensorType != req.get_data_rsp.sensorType ||
	    SENSOR_HUB_GET_DATA != req.get_data_rsp.action || 0 != req.get_data_rsp.errCode) {
		SCP_ERR("error : %d\n", req.get_data_rsp.errCode);
		return req.get_data_rsp.errCode;
	}

	data_t = (struct data_unit_t *)req.get_data_rsp.data.int8_Data;
	switch (sensorType) {
	case ID_ACCELEROMETER:
		data->time_stamp = data_t->time_stamp;
		data->time_stamp_gpt = data_t->time_stamp_gpt;
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
		data->time_stamp_gpt = data_t->time_stamp_gpt;
		data->accelerometer_t.x = data_t->accelerometer_t.x;
		data->accelerometer_t.y = data_t->accelerometer_t.y;
		data->accelerometer_t.z = data_t->accelerometer_t.z;
		data->accelerometer_t.status = data_t->accelerometer_t.status;
		break;
	case ID_LINEAR_ACCELERATION:
		data->time_stamp = data_t->time_stamp;
		data->time_stamp_gpt = data_t->time_stamp_gpt;
		data->accelerometer_t.x = data_t->accelerometer_t.x;
		data->accelerometer_t.y = data_t->accelerometer_t.y;
		data->accelerometer_t.z = data_t->accelerometer_t.z;
		data->accelerometer_t.status = data_t->accelerometer_t.status;
		break;
	case ID_LIGHT:
		data->time_stamp = data_t->time_stamp;
		data->time_stamp_gpt = data_t->time_stamp_gpt;
		data->light = data_t->light;
		break;
	case ID_PROXIMITY:
		data->time_stamp = data_t->time_stamp;
		data->time_stamp_gpt = data_t->time_stamp_gpt;
		data->proximity_t.steps = data_t->proximity_t.steps;
		data->proximity_t.oneshot = data_t->proximity_t.oneshot;
		break;
	case ID_PRESSURE:
		data->time_stamp = data_t->time_stamp;
		data->time_stamp_gpt = data_t->time_stamp_gpt;
		data->pressure_t.pressure = data_t->pressure_t.pressure;
		data->pressure_t.status = data_t->pressure_t.status;
		break;
	case ID_GYROSCOPE:
		data->time_stamp = data_t->time_stamp;
		data->time_stamp_gpt = data_t->time_stamp_gpt;
		data->gyroscope_t.x = data_t->gyroscope_t.x;
		data->gyroscope_t.y = data_t->gyroscope_t.y;
		data->gyroscope_t.z = data_t->gyroscope_t.z;
		data->gyroscope_t.x_bias = data_t->gyroscope_t.x_bias;
		data->gyroscope_t.y_bias = data_t->gyroscope_t.y_bias;
		data->gyroscope_t.z_bias = data_t->gyroscope_t.z_bias;
		data->gyroscope_t.status = data_t->gyroscope_t.status;
		break;
	case ID_GYROSCOPE_UNCALIBRATED:
		data->time_stamp = data_t->time_stamp;
		data->time_stamp_gpt = data_t->time_stamp_gpt;
		data->uncalibrated_gyro_t.x = data_t->uncalibrated_gyro_t.x;
		data->uncalibrated_gyro_t.y = data_t->uncalibrated_gyro_t.y;
		data->uncalibrated_gyro_t.z = data_t->uncalibrated_gyro_t.z;
		data->uncalibrated_gyro_t.x_bias = data_t->uncalibrated_gyro_t.x_bias;
		data->uncalibrated_gyro_t.y_bias = data_t->uncalibrated_gyro_t.y_bias;
		data->uncalibrated_gyro_t.z_bias = data_t->uncalibrated_gyro_t.z_bias;
		data->uncalibrated_gyro_t.status = data_t->uncalibrated_gyro_t.status;
		break;
	case ID_RELATIVE_HUMIDITY:
		data->time_stamp = data_t->time_stamp;
		data->time_stamp_gpt = data_t->time_stamp_gpt;
		data->relative_humidity_t.relative_humidity =
		    data_t->relative_humidity_t.relative_humidity;
		data->relative_humidity_t.status = data_t->relative_humidity_t.status;
		break;
	case ID_MAGNETIC:
		data->time_stamp = data_t->time_stamp;
		data->time_stamp_gpt = data_t->time_stamp_gpt;
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
		data->time_stamp_gpt = data_t->time_stamp_gpt;
		data->uncalibrated_mag_t.x = data_t->uncalibrated_mag_t.x;
		data->uncalibrated_mag_t.y = data_t->uncalibrated_mag_t.y;
		data->uncalibrated_mag_t.z = data_t->uncalibrated_mag_t.z;
		data->uncalibrated_mag_t.x_bias = data_t->uncalibrated_mag_t.x_bias;
		data->uncalibrated_mag_t.y_bias = data_t->uncalibrated_mag_t.y_bias;
		data->uncalibrated_mag_t.z_bias = data_t->uncalibrated_mag_t.z_bias;
		data->uncalibrated_mag_t.status = data_t->uncalibrated_mag_t.status;
		break;
	case ID_GEOMAGNETIC_ROTATION_VECTOR:
		data->time_stamp = data_t->time_stamp;
		data->time_stamp_gpt = data_t->time_stamp_gpt;
		data->magnetic_t.x = data_t->magnetic_t.x;
		data->magnetic_t.y = data_t->magnetic_t.y;
		data->magnetic_t.z = data_t->magnetic_t.z;
		data->magnetic_t.scalar = data_t->magnetic_t.scalar;
		data->magnetic_t.status = data_t->magnetic_t.status;
		break;
	case ID_ORIENTATION:
		data->time_stamp = data_t->time_stamp;
		data->time_stamp_gpt = data_t->time_stamp_gpt;
		data->orientation_t.azimuth = data_t->orientation_t.azimuth;
		data->orientation_t.pitch = data_t->orientation_t.pitch;
		data->orientation_t.roll = data_t->orientation_t.roll;
		data->orientation_t.status = data_t->orientation_t.status;
		break;
	case ID_ROTATION_VECTOR:
		data->time_stamp = data_t->time_stamp;
		data->time_stamp_gpt = data_t->time_stamp_gpt;
		data->orientation_t.azimuth = data_t->orientation_t.azimuth;
		data->orientation_t.pitch = data_t->orientation_t.pitch;
		data->orientation_t.roll = data_t->orientation_t.roll;
		data->orientation_t.scalar = data_t->orientation_t.scalar;
		data->orientation_t.status = data_t->orientation_t.status;
		break;
	case ID_GAME_ROTATION_VECTOR:
		data->time_stamp = data_t->time_stamp;
		data->time_stamp_gpt = data_t->time_stamp_gpt;
		data->orientation_t.azimuth = data_t->orientation_t.azimuth;
		data->orientation_t.pitch = data_t->orientation_t.pitch;
		data->orientation_t.roll = data_t->orientation_t.roll;
		data->orientation_t.scalar = data_t->orientation_t.scalar;
		data->orientation_t.status = data_t->orientation_t.status;
		break;
	case ID_STEP_COUNTER:
		data->time_stamp = data_t->time_stamp;
		data->time_stamp_gpt = data_t->time_stamp_gpt;
		data->step_counter_t.accumulated_step_count
		    = data_t->step_counter_t.accumulated_step_count;
		break;
	case ID_STEP_DETECTOR:
		data->time_stamp = data_t->time_stamp;
		data->time_stamp_gpt = data_t->time_stamp_gpt;
		data->step_detector_t.step_detect = data_t->step_detector_t.step_detect;
		break;
	case ID_SIGNIFICANT_MOTION:
		data->time_stamp = data_t->time_stamp;
		data->time_stamp_gpt = data_t->time_stamp_gpt;
		data->smd_t.state = data_t->smd_t.state;
		break;
	case ID_HEART_RATE:
		data->time_stamp = data_t->time_stamp;
		data->time_stamp_gpt = data_t->time_stamp_gpt;
		data->heart_rate_t.bpm = data_t->heart_rate_t.bpm;
		data->heart_rate_t.status = data_t->heart_rate_t.status;
		break;
	case ID_PEDOMETER:
		data->time_stamp = data_t->time_stamp;
		data->time_stamp_gpt = data_t->time_stamp_gpt;
		data->pedometer_t.accumulated_step_count =
		    data_t->pedometer_t.accumulated_step_count;
		data->pedometer_t.accumulated_step_length =
		    data_t->pedometer_t.accumulated_step_length;
		data->pedometer_t.step_frequency = data_t->pedometer_t.step_frequency;
		data->pedometer_t.step_length = data_t->pedometer_t.step_length;
		break;
	case ID_ACTIVITY:
		data->time_stamp = data_t->time_stamp;
		data->time_stamp_gpt = data_t->time_stamp_gpt;
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
		data->time_stamp_gpt = data_t->time_stamp_gpt;
		data->inpocket_event.state = data_t->inpocket_event.state;
		break;
	case ID_PICK_UP_GESTURE:
		data->time_stamp = data_t->time_stamp;
		data->time_stamp_gpt = data_t->time_stamp_gpt;
		data->gesture_data_t.probability = data_t->gesture_data_t.probability;
		break;
	case ID_TILT_DETECTOR:
		data->time_stamp = data_t->time_stamp;
		data->time_stamp_gpt = data_t->time_stamp_gpt;
		data->tilt_event.state = data_t->tilt_event.state;
		break;
	case ID_WAKE_GESTURE:
		data->time_stamp = data_t->time_stamp;
		data->time_stamp_gpt = data_t->time_stamp_gpt;
		data->gesture_data_t.probability = data_t->gesture_data_t.probability;
		break;
	case ID_GLANCE_GESTURE:
		data->time_stamp = data_t->time_stamp;
		data->time_stamp_gpt = data_t->time_stamp_gpt;
		data->gesture_data_t.probability = data_t->gesture_data_t.probability;
		break;
	case ID_PDR:
		data->time_stamp = data_t->time_stamp;
		data->time_stamp_gpt = data_t->time_stamp_gpt;
		data->pdr_event.x = data_t->pdr_event.x;
		data->pdr_event.y = data_t->pdr_event.y;
		data->pdr_event.z = data_t->pdr_event.z;
		data->pdr_event.status = data_t->pdr_event.status;
		break;
	default:
		err = -1;
		break;
	}
	return err;
}

int sensor_set_cmd_to_hub(uint8_t sensorType, CUST_ACTION action, void *data)
{
	SCP_SENSOR_HUB_DATA req;
	int len = 0, err = 0;

	req.get_data_req.sensorType = sensorType;
	req.get_data_req.action = SENSOR_HUB_SET_CUST;

	switch (sensorType) {
	case ID_ACCELEROMETER:
		req.set_cust_req.sensorType = ID_ACCELEROMETER;
		req.set_cust_req.action = SENSOR_HUB_SET_CUST;
		switch (action) {
		case CUST_ACTION_RESET_CALI:
			req.set_cust_req.resetCali.action = CUST_ACTION_RESET_CALI;
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.resetCali);
			break;
		case CUST_ACTION_SET_CALI:
			req.set_cust_req.setCali.action = CUST_ACTION_SET_CALI;
			req.set_cust_req.setCali.int32_data[SCP_SENSOR_HUB_X]
			    = *((int32_t *) data + SCP_SENSOR_HUB_X);
			req.set_cust_req.setCali.int32_data[SCP_SENSOR_HUB_Y]
			    = *((int32_t *) data + SCP_SENSOR_HUB_Y);
			req.set_cust_req.setCali.int32_data[SCP_SENSOR_HUB_Z]
			    = *((int32_t *) data + SCP_SENSOR_HUB_Z);
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.setCali);
			break;
		case CUST_ACTION_SET_TRACE:
			req.set_cust_req.setTrace.action = CUST_ACTION_SET_TRACE;
			req.set_cust_req.setTrace.trace = *((int32_t *) data);
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.setTrace);
			break;
		case CUST_ACTION_SET_DIRECTION:
			req.set_cust_req.setDirection.action = CUST_ACTION_SET_DIRECTION;
			req.set_cust_req.setDirection.direction = *((int32_t *) data);
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.setDirection);
			break;
		case CUST_ACTION_SET_FACTORY:
			req.set_cust_req.setFactory.action = CUST_ACTION_SET_FACTORY;
			req.set_cust_req.setFactory.factory = *((int32_t *) data);
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.setFactory);
			break;
		case CUST_ACTION_SHOW_REG:
			req.set_cust_req.showReg.action = CUST_ACTION_SHOW_REG;
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.showReg);
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
			req.set_cust_req.getRawData.action = CUST_ACTION_GET_RAW_DATA;
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.getRawData);
			err = SCP_sensorHub_req_send(&req, &len, 1);
			if (0 == err) {
				if (SENSOR_HUB_SET_CUST != req.set_cust_rsp.action
				    || 0 != req.set_cust_rsp.errCode) {
					SCP_ERR("SCP_sensorHub_req_send failed!\n");
					return -1;
				}
				if (CUST_ACTION_GET_RAW_DATA != req.set_cust_rsp.getRawData.action) {
					SCP_ERR("SCP_sensorHub_req_send failed!\n");
					return -1;
				}
				*((uint8_t *) data) = req.set_cust_rsp.getRawData.uint8_data[0];
			} else {
				SCP_ERR("SCP_sensorHub_req_send failed!\n");
			}
			return 0;
		case CUST_ACTION_SHOW_ALSLV:
			req.set_cust_req.showAlslv.action = CUST_ACTION_SHOW_ALSLV;
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.showAlslv);
			break;
		case CUST_ACTION_SHOW_ALSVAL:
			req.set_cust_req.showAlsval.action = CUST_ACTION_GET_RAW_DATA;
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.showAlsval);
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
			req.set_cust_req.resetCali.action = CUST_ACTION_RESET_CALI;
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.resetCali);
			break;
		case CUST_ACTION_SET_CALI:
			req.set_cust_req.setCali.action = CUST_ACTION_SET_CALI;
			req.set_cust_req.setCali.int32_data[0] = *((int32_t *) data);
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.setCali);
			break;
		case CUST_ACTION_SET_TRACE:
			req.set_cust_req.setTrace.action = CUST_ACTION_SET_TRACE;
			req.set_cust_req.setTrace.trace = *((int32_t *) data);
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.setTrace);
			break;
		case CUST_ACTION_SHOW_REG:
			req.set_cust_req.showReg.action = CUST_ACTION_SHOW_REG;
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.showReg);
			break;
		case CUST_ACTION_SET_PS_THRESHOLD:
			req.set_cust_req.setPSThreshold.action = CUST_ACTION_SET_PS_THRESHOLD;
			req.set_cust_req.setPSThreshold.threshold[0]
			    = *((int32_t *) data + 0);
			req.set_cust_req.setPSThreshold.threshold[1]
			    = *((int32_t *) data + 1);
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.setPSThreshold);
			break;
		case CUST_ACTION_GET_RAW_DATA:
			req.set_cust_req.getRawData.action = CUST_ACTION_GET_RAW_DATA;
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.getRawData);
			err = SCP_sensorHub_req_send(&req, &len, 1);
			if (0 == err) {
				if (SENSOR_HUB_SET_CUST != req.set_cust_rsp.action
				    || 0 != req.set_cust_rsp.errCode) {
					SCP_ERR("SCP_sensorHub_req_send failed!\n");
					return -1;
				}
				if (CUST_ACTION_GET_RAW_DATA != req.set_cust_rsp.getRawData.action) {
					SCP_ERR("SCP_sensorHub_req_send failed!\n");
					return -1;
				}
				*((uint16_t *) data) = req.set_cust_rsp.getRawData.uint16_data[0];
			} else {
				SCP_ERR("SCP_sensorHub_req_send failed!\n");
			}
			return 0;
		default:
			return -1;
		}
		break;
	case ID_PRESSURE:
		req.set_cust_req.sensorType = ID_PRESSURE;
		req.set_cust_req.action = SENSOR_HUB_SET_CUST;
		switch (action) {
		case CUST_ACTION_SET_TRACE:
			req.set_cust_req.setTrace.action = CUST_ACTION_SET_TRACE;
			req.set_cust_req.setTrace.trace = *((int32_t *) data);
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.setTrace);
			break;
		case CUST_ACTION_SHOW_REG:
			req.set_cust_req.showReg.action = CUST_ACTION_SHOW_REG;
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.showReg);
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
			req.set_cust_req.resetCali.action = CUST_ACTION_RESET_CALI;
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.resetCali);
			break;
		case CUST_ACTION_SET_CALI:
			req.set_cust_req.setCali.action = CUST_ACTION_SET_CALI;
			req.set_cust_req.setCali.int32_data[SCP_SENSOR_HUB_X]
			    = *((int32_t *) data + SCP_SENSOR_HUB_X);
			req.set_cust_req.setCali.int32_data[SCP_SENSOR_HUB_Y]
			    = *((int32_t *) data + SCP_SENSOR_HUB_Y);
			req.set_cust_req.setCali.int32_data[SCP_SENSOR_HUB_Z]
			    = *((int32_t *) data + SCP_SENSOR_HUB_Z);
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.setCali);
			break;
		case CUST_ACTION_SET_TRACE:
			req.set_cust_req.setTrace.action = CUST_ACTION_SET_TRACE;
			req.set_cust_req.setTrace.trace = *((int32_t *) data);
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.setTrace);
			break;
		case CUST_ACTION_SET_DIRECTION:
			req.set_cust_req.setDirection.action = CUST_ACTION_SET_DIRECTION;
			req.set_cust_req.setDirection.direction = *((int32_t *) data);
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.setDirection);
			break;
		case CUST_ACTION_SET_FACTORY:
			req.set_cust_req.setFactory.action = CUST_ACTION_SET_FACTORY;
			req.set_cust_req.setFactory.factory = *((int32_t *) data);
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.setFactory);
			break;
		case CUST_ACTION_SHOW_REG:
			req.set_cust_req.showReg.action = CUST_ACTION_SHOW_REG;
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.showReg);
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
			req.set_cust_req.setTrace.action = CUST_ACTION_SET_TRACE;
			req.set_cust_req.setTrace.trace = *((int32_t *) data);
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.setTrace);
			break;
		case CUST_ACTION_SHOW_REG:
			req.set_cust_req.showReg.action = CUST_ACTION_SHOW_REG;
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.showReg);
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
			req.set_cust_req.setTrace.action = CUST_ACTION_SET_TRACE;
			req.set_cust_req.setTrace.trace = *((int32_t *) data);
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.setTrace);
			break;
		case CUST_ACTION_SET_DIRECTION:
			req.set_cust_req.setDirection.action = CUST_ACTION_SET_DIRECTION;
			req.set_cust_req.setDirection.direction = *((int32_t *) data);
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.setDirection);
			break;
		case CUST_ACTION_SHOW_REG:
			req.set_cust_req.showReg.action = CUST_ACTION_SHOW_REG;
			len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData)
			    + sizeof(req.set_cust_req.showReg);
			break;
		default:
			return -1;
		}
		break;
	default:
		err = -1;
		break;
	}

	err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err < 0) {
		SCP_ERR("SCP_sensorHub_req_send fail!\n");
		return -1;
	}
	if (sensorType != req.get_data_rsp.sensorType ||
	    SENSOR_HUB_SET_CUST != req.get_data_rsp.action || 0 != req.get_data_rsp.errCode) {
		SCP_ERR("error : %d\n", req.get_data_rsp.errCode);
		return req.get_data_rsp.errCode;
	}

	return err;
}

static int SCP_sensorHub_mask_notify(void)
{
	SCP_SENSOR_HUB_DATA req;
	int len;
	int err = 0;

	req.mask_notify_req.sensorType = 0;
	req.mask_notify_req.action = SENSOR_HUB_MASK_NOTIFY;
	req.mask_notify_req.if_mask_or_not = 1;
	len = sizeof(req.mask_notify_req);
	err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err < 0)
		SCP_ERR("SCP_sensorHub_req_send fail!\n");
	return err;
}

static int SCP_sensorHub_unmask_notify(void)
{
	SCP_SENSOR_HUB_DATA req;
	int len;
	int err = 0;

	req.mask_notify_req.sensorType = 0;
	req.mask_notify_req.action = SENSOR_HUB_MASK_NOTIFY;
	req.mask_notify_req.if_mask_or_not = 0;
	len = sizeof(req.mask_notify_req);
	err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err < 0)
		SCP_ERR("SCP_sensorHub_req_send fail!\n");
	return err;
}

static int sensorHub_probe(struct platform_device *pdev)
{
	struct SCP_sensorHub_data *obj;
	int err = 0;

	SCP_FUN();
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		SCP_ERR("Allocate SCP_sensorHub_data fail\n");
		err = -ENOMEM;
		goto exit;
	}

	memset(obj, 0, sizeof(struct SCP_sensorHub_data));
	/* obj->SCP_sensorFIFO = kzalloc(SCP_SENSOR_HUB_FIFO_SIZE, GFP_KERNEL); */
	obj->SCP_sensorFIFO = (struct sensorFIFO *)get_reserve_mem_virt(SENS_MEM_ID);
	if (!obj->SCP_sensorFIFO) {
		SCP_ERR("Allocate SCP_sensorFIFO fail\n");
		err = -ENOMEM;
		goto exit;
	}
	obj->SCP_directPush_FIFO = (struct sensorFIFO *)get_reserve_mem_virt(SENS_MEM_DIRECT_ID);
	if (!obj->SCP_directPush_FIFO) {
		SCP_ERR("Allocate SCP_sensorFIFO fail\n");
		err = -ENOMEM;
		goto exit;
	}

	obj->SCP_sensorFIFO->wp = 0;
	obj->SCP_sensorFIFO->rp = 0;
	obj->SCP_sensorFIFO->FIFOSize =
	    SCP_SENSOR_HUB_FIFO_SIZE - offsetof(struct sensorFIFO, data);

	obj->SCP_directPush_FIFO->wp = 0;
	obj->SCP_directPush_FIFO->rp = 0;
	obj->SCP_directPush_FIFO->FIFOSize =
	    SCP_DIRECT_PUSH_FIFO_SIZE - offsetof(struct sensorFIFO, data);

	SCP_ERR("obj->SCP_sensorFIFO = %p, wp = %d, rp = %d, size = %d\n", obj->SCP_sensorFIFO,
		obj->SCP_sensorFIFO->wp, obj->SCP_sensorFIFO->rp, obj->SCP_sensorFIFO->FIFOSize);

	obj_data = obj;

	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);
	atomic_set(&obj->wait_rsp, 0);
	atomic_set(&obj->ipi_handler_running, 0);
	atomic_set(&obj->disable_fifo_full_notify, 0);
	INIT_WORK(&obj->ipi_work, SCP_ipi_work);
	INIT_WORK(&obj->fifo_full_work, SCP_fifo_full_work);

	INIT_WORK(&obj->batch_timeout_work, SCP_batch_timeout_work);
	INIT_WORK(&obj->direct_push_work, SCP_direct_push_work);
	INIT_WORK(&obj->power_notify_work, SCP_power_notify_work);

	init_waitqueue_head(&SCP_sensorHub_req_wq);
	init_timer(&obj->timer);
	obj->timer.expires = 3 * HZ;
	obj->timer.function = SCP_sensorHub_req_send_timeout;
	obj->timer.data = (unsigned long)obj;

#ifdef	SENSOR_SCP_AP_TIME_SYNC
	INIT_WORK(&syncwork, syncwork_fun);
	sync_timer.expires = jiffies + 3 * HZ;
	sync_timer.function = sync_timeout;
	init_timer(&sync_timer);
	mod_timer(&sync_timer, jiffies + 3 * HZ);
#endif

	scp_ipi_registration(IPI_SENSOR, SCP_sensorHub_IPI_handler, "SCP_sensorHub");

	SCP_sensorHub_init_flag = 0;
	SCP_sensorHub_init_client();
	SCP_ERR("init done, data_unit_t size: %d, SCP_SENSOR_HUB_DATA size:%d\n",
			(int)sizeof(struct data_unit_t), (int)sizeof(SCP_SENSOR_HUB_DATA));
	BUG_ON(sizeof(struct data_unit_t) != SENSOR_DATA_SIZE || sizeof(SCP_SENSOR_HUB_DATA) != SENSOR_DATA_SIZE);
	return 0;

exit:
	SCP_ERR("%s: err = %d\n", __func__, err);
	SCP_sensorHub_init_flag = -1;
	return err;
}

static int sensorHub_remove(struct platform_device *pdev)
{
	return 0;
}

static int sensorHub_suspend(struct platform_device *pdev, pm_message_t msg)
{
	sensor_send_ap_timetamp();
	SCP_sensorHub_mask_notify();
	return 0;
}

static int sensorHub_resume(struct platform_device *pdev)
{
	sensor_send_ap_timetamp();
	SCP_sensorHub_unmask_notify();
	return 0;
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
};

static int __init SCP_sensorHub_init(void)
{
	SCP_FUN();
	if (platform_device_register(&sensorHub_device)) {
		SCP_ERR("SCP_sensorHub platform device error\n");
		return -1;
	}
	if (platform_driver_register(&sensorHub_driver)) {
		SCP_ERR("SCP_sensorHub platform driver error\n");
		return -1;
	}
	return 0;
}

static void __exit SCP_sensorHub_exit(void)
{
	SCP_FUN();
}
late_initcall(SCP_sensorHub_init);
module_exit(SCP_sensorHub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SCP sensor hub driver");
MODULE_AUTHOR("andrew.yang@mediatek.com");
