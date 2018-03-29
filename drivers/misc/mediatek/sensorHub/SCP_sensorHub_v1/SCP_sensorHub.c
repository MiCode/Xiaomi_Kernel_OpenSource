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
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <asm/atomic.h>
#include <linux/types.h>
#include <batch.h>
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

#define SENSOR_DATA_SIZE 48
/*
 * experience number for delay_count per DELAY_COUNT sensor input delay 10ms
 * msleep(20) system will schedule to hal process then read input node
 */
#define DELAY_COUNT			32
#define SCP_sensorHub_DEV_NAME        "SCP_sensorHub"
static int SCP_sensorHub_local_remove(void);
static int SCP_sensorHub_local_init(void);
static int scp_sensorHub_power_adjust(void);
static int sensor_send_ap_timetamp(void);
static int SCP_sensorHub_server_dispatch_data(void);
static int SCP_sensorHub_report_data(struct data_unit_t *data_t);
static void clean_up_deactivate_ringbuffer(int handle);

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

static struct batch_init_info SCP_sensorHub_init_info = {
	.name = "SCP_sensorHub",
	.init = SCP_sensorHub_local_init,
	.uninit = SCP_sensorHub_local_remove,
	.platform_diver_addr = NULL,
};

struct sensor_client {
	char name[16];
	spinlock_t buffer_lock;
	unsigned int head;
	unsigned int tail;
	unsigned int bufsize;
	struct data_unit_t *ringbuffer;
	struct work_struct worker;
	struct workqueue_struct *single_thread;
	atomic_t delay_count;
};
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
	unsigned long sensor_activate_bitmap;
	unsigned long flush_remain_data_after_disable;

	volatile struct sensorFIFO *volatile SCP_sensorFIFO;
	volatile struct sensorFIFO *volatile SCP_directPush_FIFO;
	struct sensor_client client[ID_SENSOR_MAX_HANDLE];
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

#if 0
static int SCP_sensorHub_init_client(void)
{				/* call by init done workqueue */
	struct SCP_sensorHub_data *obj = obj_data;
	SCP_SENSOR_HUB_DATA data;
	unsigned int len = 0;

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();
	obj->mapping =
	    dma_map_single(&SCP_sensorHub_dev, (void *)obj->SCP_sensorFIFO,
			   obj->SCP_sensorFIFO->FIFOSize, DMA_BIDIRECTIONAL);
	SCP_ERR("obj->mapping = %p\n", (void *)obj->mapping);
	dma_sync_single_for_device(&SCP_sensorHub_dev, obj->mapping, obj->SCP_sensorFIFO->FIFOSize,
				   DMA_TO_DEVICE);

	data.set_config_req.sensorType = 0;
	data.set_config_req.action = SENSOR_HUB_SET_CONFIG;
	data.set_config_req.bufferBase = (int)(obj->mapping & 0xFFFFFFFF);
	SCP_ERR("data.set_config_req.bufferBase = %d\n", data.set_config_req.bufferBase);
/* SCP_ERR("obj->SCP_sensorFIFO = %p, wp = %p, rp = %p, size = %d\n", obj->SCP_sensorFIFO, */
/* obj->SCP_sensorFIFO->wp, obj->SCP_sensorFIFO->rp, obj->SCP_sensorFIFO->FIFOSize); */
	data.set_config_req.bufferSize = obj->SCP_sensorFIFO->FIFOSize;
	len = sizeof(data.set_config_req);

	SCP_sensorHub_req_send(&data, &len, 1);

	SCP_ERR("fwq SCP_sensorHub_init_client done\n");

	return SCP_SENSOR_HUB_SUCCESS;
}

#endif

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

static int SCP_sensorHub_extract_data(struct hwm_sensor_data *data, struct data_unit_t *data_t)
{
	int err = 0;

	switch (data_t->sensor_type) {
	case ID_ACCELEROMETER:
		data->sensor = data_t->sensor_type;
		data->values[0] = data_t->accelerometer_t.x;
		data->values[1] = data_t->accelerometer_t.y;
		data->values[2] = data_t->accelerometer_t.z;
		data->values[3] = data_t->accelerometer_t.x_bias;
		data->values[4] = data_t->accelerometer_t.y_bias;
		data->values[5] = data_t->accelerometer_t.z_bias;
		data->status = (int8_t) data_t->accelerometer_t.status;
		data->time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_GRAVITY:
		data->sensor = data_t->sensor_type;
		data->values[0] = data_t->accelerometer_t.x;
		data->values[1] = data_t->accelerometer_t.y;
		data->values[2] = data_t->accelerometer_t.z;
		data->status = (int8_t) data_t->accelerometer_t.status;
		data->time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_LINEAR_ACCELERATION:
		data->sensor = data_t->sensor_type;
		data->values[0] = data_t->accelerometer_t.x;
		data->values[1] = data_t->accelerometer_t.y;
		data->values[2] = data_t->accelerometer_t.z;
		data->status = (int8_t) data_t->accelerometer_t.status;
		data->time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_LIGHT:
		data->sensor = data_t->sensor_type;
		data->values[0] = data_t->light;
		data->time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_PROXIMITY:
		data->sensor = data_t->sensor_type;
		data->values[0] = data_t->proximity_t.oneshot;
		data->time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_PRESSURE:
		data->sensor = data_t->sensor_type;
		data->values[0] = data_t->pressure_t.pressure;
		data->status = (int8_t) data_t->pressure_t.status;
		data->time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_GYROSCOPE:
		data->sensor = data_t->sensor_type;
		data->values[0] = data_t->gyroscope_t.x;
		data->values[1] = data_t->gyroscope_t.y;
		data->values[2] = data_t->gyroscope_t.z;
		data->values[3] = data_t->gyroscope_t.x_bias;
		data->values[4] = data_t->gyroscope_t.y_bias;
		data->values[5] = data_t->gyroscope_t.z_bias;
		data->status = (int8_t) data_t->gyroscope_t.status;
		data->time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_GYROSCOPE_UNCALIBRATED:
		data->sensor = data_t->sensor_type;
		data->values[0] = data_t->uncalibrated_gyro_t.x;
		data->values[1] = data_t->uncalibrated_gyro_t.y;
		data->values[2] = data_t->uncalibrated_gyro_t.z;
		data->values[3] = data_t->uncalibrated_gyro_t.x_bias;
		data->values[4] = data_t->uncalibrated_gyro_t.y_bias;
		data->values[5] = data_t->uncalibrated_gyro_t.z_bias;
		data->time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_RELATIVE_HUMIDITY:
		data->sensor = data_t->sensor_type;
		data->values[0] = data_t->relative_humidity_t.relative_humidity;
		data->status = (int8_t) data_t->relative_humidity_t.status;
		data->time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_MAGNETIC:
		data->sensor = data_t->sensor_type;
		data->values[0] = data_t->magnetic_t.x;
		data->values[1] = data_t->magnetic_t.y;
		data->values[2] = data_t->magnetic_t.z;
		data->values[3] = data_t->magnetic_t.x_bias;
		data->values[4] = data_t->magnetic_t.y_bias;
		data->values[5] = data_t->magnetic_t.z_bias;
		data->status = (int8_t) data_t->magnetic_t.status;
		data->time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_MAGNETIC_UNCALIBRATED:
		data->sensor = data_t->sensor_type;
		data->values[0] = data_t->uncalibrated_mag_t.x;
		data->values[1] = data_t->uncalibrated_mag_t.y;
		data->values[2] = data_t->uncalibrated_mag_t.z;
		data->values[3] = data_t->uncalibrated_mag_t.x_bias;
		data->values[4] = data_t->uncalibrated_mag_t.y_bias;
		data->values[5] = data_t->uncalibrated_mag_t.z_bias;
		data->time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_GEOMAGNETIC_ROTATION_VECTOR:
		data->sensor = data_t->sensor_type;
		data->values[0] = data_t->magnetic_t.x;
		data->values[1] = data_t->magnetic_t.y;
		data->values[2] = data_t->magnetic_t.z;
		data->values[3] = data_t->magnetic_t.scalar;
		data->status = (int8_t) data_t->magnetic_t.status;
		data->time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_ORIENTATION:
		data->sensor = data_t->sensor_type;
		data->values[0] = data_t->orientation_t.x;
		data->values[1] = data_t->orientation_t.y;
		data->values[2] = data_t->orientation_t.z;
		data->status = (int8_t) data_t->orientation_t.status;
		data->time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_ROTATION_VECTOR:
		data->sensor = data_t->sensor_type;
		data->values[0] = data_t->orientation_t.azimuth;
		data->values[1] = data_t->orientation_t.pitch;
		data->values[2] = data_t->orientation_t.roll;
		data->values[3] = data_t->orientation_t.scalar;
		data->status = (int8_t) data_t->orientation_t.status;
		data->time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_GAME_ROTATION_VECTOR:
		data->sensor = data_t->sensor_type;
		data->values[0] = data_t->orientation_t.azimuth;
		data->values[1] = data_t->orientation_t.pitch;
		data->values[2] = data_t->orientation_t.roll;
		data->values[3] = data_t->orientation_t.scalar;
		data->status = (int8_t) data_t->orientation_t.status;
		data->time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_STEP_COUNTER:
		data->sensor = data_t->sensor_type;
		data->values[0] = data_t->step_counter_t.accumulated_step_count;
		data->time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_STEP_DETECTOR:
		data->sensor = data_t->sensor_type;
		data->values[0] = data_t->step_detector_t.step_detect;
		data->time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_TILT_DETECTOR:
		data->sensor = data_t->sensor_type;
		data->values[0] = data_t->tilt_event.state;
		data->time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_PEDOMETER:
		data->sensor = data_t->sensor_type;
		data->values[0] = data_t->pedometer_t.accumulated_step_count;
		data->values[1] = data_t->pedometer_t.accumulated_step_length;
		data->values[2] = data_t->pedometer_t.step_frequency;
		data->values[3] = data_t->pedometer_t.step_length;
		data->time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_PDR:
		data->sensor = data_t->sensor_type;
		data->values[0] = data_t->pdr_event.x;
		data->values[1] = data_t->pdr_event.y;
		data->values[2] = data_t->pdr_event.z;
		data->status = (int8_t) data_t->pdr_event.status;
		data->time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_SIGNIFICANT_MOTION:
		data->sensor = data_t->sensor_type;
		data->values[0] = data_t->smd_t.state;
		data->time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_GLANCE_GESTURE:
		data->sensor = data_t->sensor_type;
		data->values[0] = data_t->gesture_data_t.probability;
		data->time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_WAKE_GESTURE:
		data->sensor = data_t->sensor_type;
		data->values[0] = data_t->gesture_data_t.probability;
		data->time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_PICK_UP_GESTURE:
		data->sensor = data_t->sensor_type;
		data->values[0] = data_t->gesture_data_t.probability;
		data->time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_ACTIVITY:
		data->sensor = data_t->sensor_type;
		data->probability[STILL] = data_t->activity_data_t.probability[STILL];
		data->probability[STANDING] = data_t->activity_data_t.probability[STANDING];
		data->probability[SITTING] = data_t->activity_data_t.probability[SITTING];
		data->probability[LYING] = data_t->activity_data_t.probability[LYING];
		data->probability[ON_FOOT] = data_t->activity_data_t.probability[ON_FOOT];
		data->probability[WALKING] = data_t->activity_data_t.probability[WALKING];
		data->probability[RUNNING] = data_t->activity_data_t.probability[RUNNING];
		data->probability[CLIMBING] = data_t->activity_data_t.probability[CLIMBING];
		data->probability[ON_BICYCLE] = data_t->activity_data_t.probability[ON_BICYCLE];
		data->probability[IN_VEHICLE] = data_t->activity_data_t.probability[IN_VEHICLE];
		data->probability[TILTING] = data_t->activity_data_t.probability[TILTING];
		data->probability[UNKNOWN] = data_t->activity_data_t.probability[UNKNOWN];
		data->time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	default:
		err = -1;
		break;
	}
	return err;
}

static int SCP_sensorHub_ReadChipInfo(char *buf, int bufsize)
{
	if ((NULL == buf) || (bufsize <= 30))
		return -1;

	sprintf(buf, "SCP_sensorHub Chip");
	return 0;
}

static int SCP_sensorHub_ReadSensorData(int handle, struct hwm_sensor_data *sensorData)
{
	struct SCP_sensorHub_data *obj = obj_data;
	char *pStart, *pEnd, *pNext;
	struct data_unit_t curData;
	char *rp, *wp;
	int offset;
	int fifo_usage;
	int err;
	int64_t ns;
	struct timespec time;

	time.tv_sec = time.tv_nsec = 0;

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	if (NULL == sensorData)
		return -1;

	err = SCP_sensorHub_get_scp_semaphore();
	if (err < 0) {
		SCP_ERR("SCP_sensorHub_get_scp_semaphore fail : %d\n", err);
		return -2;
	}

	/* dma_sync_single_for_cpu(&SCP_sensorHub_dev, obj->mapping, obj->SCP_sensorFIFO->FIFOSize,
	   DMA_FROM_DEVICE); */
	pStart = (char *)obj->SCP_sensorFIFO + offsetof(struct sensorFIFO, data);
	pEnd = pStart + obj->SCP_sensorFIFO->FIFOSize;
	rp = pStart + obj->SCP_sensorFIFO->rp;
	wp = pStart + obj->SCP_sensorFIFO->wp;

	if (rp < pStart || pEnd <= rp) {
		SCP_ERR("FIFO rp invalid : %p, %p, %p\n", pStart, pEnd, rp);
		err = release_scp_semaphore(SEMAPHORE_SENSOR);
		if (err < 0) {
			SCP_ERR("release_scp_semaphore fail : %d\n", err);
			return -3;
		}
		return -4;
	}

	if (wp < pStart || pEnd <= wp) {
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
		return -6;
	}
	pNext = rp + SENSOR_DATA_SIZE;

	if (SCP_TRC_BATCH_DETAIL & atomic_read(&(obj_data->trace)))
		SCP_LOG("sensor_type = %d, pNext = %p, rp = %p, wp = %p\n",
			((struct data_unit_t *)rp)->sensor_type, pNext, rp, wp);


	if (((struct data_unit_t *)rp)->sensor_type < 0
	    || ((struct data_unit_t *)rp)->sensor_type > 58) {
		SCP_ERR("Wrong data, sensor_type = %d .\n",
			((struct data_unit_t *)rp)->sensor_type);
		return -7;
	}

	if (pNext < pEnd) {
		memcpy((char *)&curData, rp, pNext - rp);
		rp = pNext;
	} else {
		memcpy(&curData, rp, pEnd - rp);
		offset = (int)(pEnd - rp);
		memcpy((char *)&curData + offset, pStart, pNext - pEnd);
		offset = (int)(pNext - pEnd);
		rp = pStart + offset;
	}

	obj->SCP_sensorFIFO->rp = (int)(rp - pStart);
	/* dma_sync_single_for_device(&SCP_sensorHub_dev,
		obj->mapping, obj->SCP_sensorFIFO->FIFOSize, DMA_TO_DEVICE); */
	err = release_scp_semaphore(SEMAPHORE_SENSOR);
	if (err < 0)
		SCP_ERR("release_scp_semaphore fail : %d\n", err);
	get_monotonic_boottime(&time);
	ns = time.tv_sec * 1000000000LL + time.tv_nsec;
	err = SCP_sensorHub_extract_data(sensorData, &curData);
	/*SCP_LOG("type:%d,now time:%lld scp time: %lld, %d, %d, %d, %d, %d, %d\n",
	   sensorData->sensor, ns, sensorData->time,
	   sensorData->values[0], sensorData->values[1], sensorData->values[2],
	   sensorData->values[3], sensorData->values[4], sensorData->values[5]); */
	if (rp <= wp)
		fifo_usage = (int)(wp - rp);
	else
		fifo_usage = obj->SCP_sensorFIFO->FIFOSize - (int)(rp - wp);

	fifo_usage = (fifo_usage * 100) / obj->SCP_sensorFIFO->FIFOSize;

	if (SCP_TRC_BATCH_DETAIL & atomic_read(&(obj_data->trace)))
		SCP_LOG("rp = %p, wp = %p, fifo_usage = %d%%\n", rp, wp, fifo_usage);

	if (fifo_usage < 50)
		atomic_set(&obj->disable_fifo_full_notify, 0);

	return 0;
}

static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	char strbuf[SCP_SENSOR_HUB_TEMP_BUFSIZE];

	SCP_sensorHub_ReadChipInfo(strbuf, SCP_SENSOR_HUB_TEMP_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct SCP_sensorHub_data *obj = obj_data;

	if (obj == NULL) {
		SCP_ERR("SCP_sensorHub_data obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}

static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct SCP_sensorHub_data *obj = obj_data;
	int trace;

	if (obj == NULL) {
		SCP_ERR("SCP_sensorHub_data obj is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace))
		atomic_set(&obj->trace, trace);
	else
		SCP_ERR("invalid content: '%s', length = %d\n", buf, (int)count);

	return count;
}

static DRIVER_ATTR(chipinfo, S_IWUSR | S_IRUGO, show_chipinfo_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, show_trace_value, store_trace_value);
static struct driver_attribute *SCP_sensorHub_attr_list[] = {
	&driver_attr_chipinfo,	/*chip information */
	&driver_attr_trace,	/*trace log */
};

static int SCP_sensorHub_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(SCP_sensorHub_attr_list) / sizeof(SCP_sensorHub_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, SCP_sensorHub_attr_list[idx]);
		if (err) {
			SCP_ERR("driver_create_file (%s) = %d\n",
				SCP_sensorHub_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int SCP_sensorHub_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(SCP_sensorHub_attr_list) / sizeof(SCP_sensorHub_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, SCP_sensorHub_attr_list[idx]);

	return err;
}

static int SCP_sensorHub_open(struct inode *inode, struct file *file)
{
	file->private_data = obj_data;

	if (file->private_data == NULL) {
		SCP_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}

static int SCP_sensorHub_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long SCP_sensorHub_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	char strbuf[SCP_SENSOR_HUB_TEMP_BUFSIZE];
	void __user *data;
	long err = 0;

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

	if (err) {
		SCP_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case GSENSOR_IOCTL_INIT:
		SCP_sensorHub_init_client();
		break;

	case GSENSOR_IOCTL_READ_CHIPINFO:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		SCP_sensorHub_ReadChipInfo(strbuf, SCP_SENSOR_HUB_TEMP_BUFSIZE);
		if (copy_to_user(data, strbuf, strlen(strbuf) + 1)) {
			err = -EFAULT;
			break;
		}
		break;

	case GSENSOR_IOCTL_READ_SENSORDATA:
		err = -EINVAL;
		break;

	case GSENSOR_IOCTL_READ_GAIN:
		err = -EINVAL;
		break;

	case GSENSOR_IOCTL_READ_RAW_DATA:
		err = -EFAULT;
		break;

	case GSENSOR_IOCTL_SET_CALI:
		err = -EINVAL;
		break;

	case GSENSOR_IOCTL_CLR_CALI:
		err = -EINVAL;
		break;

	case GSENSOR_IOCTL_GET_CALI:
		err = -EINVAL;
		break;


	default:
		SCP_ERR("unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;

	}

	return err;
}


static const struct file_operations SCP_sensorHub_fops = {
	/* .owner = THIS_MODULE, */
	.open = SCP_sensorHub_open,
	.release = SCP_sensorHub_release,
	.unlocked_ioctl = SCP_sensorHub_unlocked_ioctl,
};

static struct miscdevice SCP_sensorHub_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "SCP_sensorHub",
	.fops = &SCP_sensorHub_fops,
};

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

int SCP_sensorHub_rsp_registration(uint8_t sensor, SCP_sensorHub_handler handler)
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

static void sensor_client_read_ringbuffer_data(int handle)
{
	struct SCP_sensorHub_data *obj = obj_data;
	unsigned int head = 0, tail = 0;

	clean_up_deactivate_ringbuffer(handle);
	while (1) {
		spin_lock(&obj->client[handle].buffer_lock);
		head = obj->client[handle].head;
		tail = obj->client[handle].tail;
		spin_unlock(&obj->client[handle].buffer_lock);
		if (tail == head)
			break;
		for (; tail != head;) {
			SCP_sensorHub_report_data(&obj->client[handle].ringbuffer[tail]);
			tail++;
			tail &= obj->client[handle].bufsize - 1;
			obj->client[handle].tail++;
			obj->client[handle].tail &= obj->client[handle].bufsize - 1;
		}
	}
}

static void sensor_server_write_ringbuffer_data(struct data_unit_t *event, int handle)
{
	struct SCP_sensorHub_data *obj = obj_data;
	struct sensor_client *client = obj->client;

	switch (handle) {
	case ID_ACCELEROMETER:
	case ID_MAGNETIC:
	case ID_MAGNETIC_UNCALIBRATED:
	case ID_GYROSCOPE:
	case ID_GYROSCOPE_UNCALIBRATED:
	case ID_ORIENTATION:
	case ID_ROTATION_VECTOR:
	case ID_GAME_ROTATION_VECTOR:
	case ID_GEOMAGNETIC_ROTATION_VECTOR:
	case ID_GRAVITY:
	case ID_LINEAR_ACCELERATION:
	case ID_PEDOMETER:
	case ID_ACTIVITY:
		spin_lock(&client[handle].buffer_lock);
		client[handle].ringbuffer[client[handle].head++] = *event;
		client[handle].head &= client[handle].bufsize - 1;
		if (unlikely(client[handle].head == client[handle].tail))
			SCP_ERR("handle(%d) dropped data due to ringbuffer is full\n", handle);
		spin_unlock(&client[handle].buffer_lock);
		break;
	default:
		break;
	}
}

static void accelerometer_report_data_work(struct work_struct *work)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();
	sensor_client_read_ringbuffer_data(ID_ACCELEROMETER);
}

static void magnetic_report_data_work(struct work_struct *work)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();
	sensor_client_read_ringbuffer_data(ID_MAGNETIC);
}

static void unmagnetic_report_data_work(struct work_struct *work)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();
	sensor_client_read_ringbuffer_data(ID_MAGNETIC_UNCALIBRATED);
}

static void gyroscope_report_data_work(struct work_struct *work)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();
	sensor_client_read_ringbuffer_data(ID_GYROSCOPE);
}

static void ungyroscope_report_data_work(struct work_struct *work)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();
	sensor_client_read_ringbuffer_data(ID_GYROSCOPE_UNCALIBRATED);
}

static void orientation_report_data_work(struct work_struct *work)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();
	sensor_client_read_ringbuffer_data(ID_ORIENTATION);
}

static void rotvec_report_data_work(struct work_struct *work)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();
	sensor_client_read_ringbuffer_data(ID_ROTATION_VECTOR);
}

static void gamerotvec_report_data_work(struct work_struct *work)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();
	sensor_client_read_ringbuffer_data(ID_GAME_ROTATION_VECTOR);
}

static void geomagnetic_report_data_work(struct work_struct *work)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();
	sensor_client_read_ringbuffer_data(ID_GEOMAGNETIC_ROTATION_VECTOR);
}

static void gravity_report_data_work(struct work_struct *work)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();
	sensor_client_read_ringbuffer_data(ID_GRAVITY);
}

static void linearaccel_report_data_work(struct work_struct *work)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();
	sensor_client_read_ringbuffer_data(ID_LINEAR_ACCELERATION);
}

static void pedometer_report_data_work(struct work_struct *work)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();
	sensor_client_read_ringbuffer_data(ID_PEDOMETER);
}

static void activity_report_data_work(struct work_struct *work)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();
	sensor_client_read_ringbuffer_data(ID_ACTIVITY);
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
		if (NULL != sensor_handler[rsp->rsp.sensorType])
			sensor_handler[rsp->rsp.sensorType] (data, len);
		if (rsp->rsp.sensorType == ID_TILT_DETECTOR)
			sensor_handler[ID_WAKE_GESTURE] (data, len);
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

int SCP_sensorHub_enable_hw_batch(int handle, int enable, int flag, long long samplingPeriodNs,
				  long long maxBatchReportLatencyNs)
{
	SCP_SENSOR_HUB_DATA req;
	int len;
	int err = 0;

	if (samplingPeriodNs == 0)
		return 0;
	if (SCP_TRC_FUN & atomic_read(&(obj_data->trace)))
		SCP_FUN();

	do_div(maxBatchReportLatencyNs, 1000000);
	do_div(samplingPeriodNs, 1000000);
	req.batch_req.sensorType = handle;
	req.batch_req.action = SENSOR_HUB_BATCH;
	req.batch_req.flag = flag;
	req.batch_req.period_ms = (unsigned int)samplingPeriodNs;
	req.batch_req.timeout_ms = (enable == 0) ? 0 : (unsigned int)maxBatchReportLatencyNs;
	if (SCP_TRC_BATCH & atomic_read(&(obj_data->trace))) {
		SCP_ERR("handle = %d, flag = %d, period_ms = %d, timeout_ms = %d!\n",
			req.batch_req.sensorType, req.batch_req.flag, req.batch_req.period_ms,
			req.batch_req.timeout_ms);
	}
	len = sizeof(req.batch_req);
	err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err)
		SCP_ERR("SCP_sensorHub_req_send fail!\n");

	return err;
}

static int SCP_sensorHub_flush(int handle)
{
	return 0;
}

static int SCP_sensorHub_get_data(int handle, struct hwm_sensor_data *sensorData)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensorHub_ReadSensorData(handle, sensorData);
}

static int SCP_sensorHub_batch_timeout(int handle, int cmd)
{
	SCP_SENSOR_HUB_DATA req;
	int len;
	int err = 0;

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	req.get_data_req.sensorType = handle;
	req.get_data_req.action = SENSOR_HUB_BATCH_TIMEOUT;
	req.get_data_req.reserve[0] = (uint8_t) cmd;
	len = sizeof(req.get_data_req);

	err = SCP_sensorHub_req_send(&req, &len, 1);
	return err;
}

static int SCP_sensorHub_report_data(struct data_unit_t *data_t)
{
	struct SCP_sensorHub_data *obj = obj_data;
	int err = 0;
	int value[6] = { 0 };
	struct hwm_sensor_data data;
	int64_t timestamp_ms = 0;
	static int64_t last_timestamp_ms_acc = 0, last_enter_timestamp_acc;
	static int64_t last_timestamp_ms_gravity = 0, last_enter_timestamp_gravity;
	static int64_t last_timestamp_ms_linearaccel = 0, last_enter_timestamp_linearaccel;
	static int64_t last_timestamp_ms_gyro = 0, last_enter_timestamp_gyro;
	static int64_t last_timestamp_ms_gyro_uncali = 0, last_enter_timestamp_gyro_uncali;
	static int64_t last_timestamp_ms_mag = 0, last_enter_timestamp_mag;
	static int64_t last_timestamp_ms_mag_uncali = 0, last_enter_timestamp_mag_uncali;
	static int64_t last_timestamp_ms_grv = 0, last_enter_timestamp_grv;
	static int64_t last_timestamp_ms_gmrv = 0, last_enter_timestamp_gmrv;
	static int64_t last_timestamp_ms_orientation = 0, last_enter_timestamp_orientation;
	static int64_t last_timestamp_ms_rv = 0, last_enter_timestamp_rv;
	static int64_t last_timestamp_ms_pedo = 0, last_enter_timestamp_pedo;
	static int64_t last_timestamp_ms_act = 0, last_enter_timestamp_act;

	int64_t now_enter_timestamp;
	struct timespec time;

	time.tv_sec = time.tv_nsec = 0;
	get_monotonic_boottime(&time);
	now_enter_timestamp = time.tv_sec * 1000000000LL + time.tv_nsec;

	timestamp_ms = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt) / 1000000;
	switch (data_t->sensor_type) {
	case ID_ACCELEROMETER:
#ifdef CONFIG_CUSTOM_KERNEL_ACCELEROMETER
		if ((now_enter_timestamp - last_enter_timestamp_acc) > 10000000)
			atomic_set(&obj->client[data_t->sensor_type].delay_count, 0);
		last_enter_timestamp_acc = now_enter_timestamp;
		if (last_timestamp_ms_acc != timestamp_ms) {
			last_timestamp_ms_acc = timestamp_ms;
			acc_data_report(data_t->accelerometer_t.x, data_t->accelerometer_t.y,
					data_t->accelerometer_t.z, data_t->accelerometer_t.status,
					(int64_t) (data_t->time_stamp + data_t->time_stamp_gpt));
			atomic_inc(&obj->client[data_t->sensor_type].delay_count);
			if (atomic_read(&obj->client[data_t->sensor_type].delay_count) ==
			    DELAY_COUNT) {
				/* SCP_ERR("handle(%d) sleep 10ms\n", data_t->sensor_type); */
				atomic_set(&obj->client[data_t->sensor_type].delay_count, 0);
				msleep(20);
			}
		}
#endif
		break;
	case ID_GRAVITY:
#ifdef CONFIG_CUSTOM_KERNEL_GRAVITY_SENSOR
		if ((now_enter_timestamp - last_enter_timestamp_gravity) > 10000000)
			atomic_set(&obj->client[data_t->sensor_type].delay_count, 0);
		last_enter_timestamp_gravity = now_enter_timestamp;
		if (last_timestamp_ms_gravity != timestamp_ms) {
			last_timestamp_ms_gravity = timestamp_ms;
			grav_data_report(data_t->accelerometer_t.x, data_t->accelerometer_t.y,
					 data_t->accelerometer_t.z, data_t->accelerometer_t.status,
					 (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt));
			atomic_inc(&obj->client[data_t->sensor_type].delay_count);
			if (atomic_read(&obj->client[data_t->sensor_type].delay_count) ==
			    DELAY_COUNT) {
				atomic_set(&obj->client[data_t->sensor_type].delay_count, 0);
				msleep(20);
			}
		}
#endif
		break;
	case ID_LINEAR_ACCELERATION:
#ifdef CONFIG_CUSTOM_KERNEL_LINEARACCEL_SENSOR
		if ((now_enter_timestamp - last_enter_timestamp_linearaccel) > 10000000)
			atomic_set(&obj->client[data_t->sensor_type].delay_count, 0);
		last_enter_timestamp_linearaccel = now_enter_timestamp;
		if (last_timestamp_ms_linearaccel != timestamp_ms) {
			last_timestamp_ms_linearaccel = timestamp_ms;
			la_data_report(data_t->accelerometer_t.x, data_t->accelerometer_t.y,
				       data_t->accelerometer_t.z, data_t->accelerometer_t.status,
				       (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt));
			atomic_inc(&obj->client[data_t->sensor_type].delay_count);
			if (atomic_read(&obj->client[data_t->sensor_type].delay_count) ==
			    DELAY_COUNT) {
				atomic_set(&obj->client[data_t->sensor_type].delay_count, 0);
				msleep(20);
			}
		}
#endif
		break;
	case ID_LIGHT:
		break;
	case ID_PRESSURE:
		break;
	case ID_GYROSCOPE:
#ifdef CONFIG_CUSTOM_KERNEL_GYROSCOPE
		if ((now_enter_timestamp - last_enter_timestamp_gyro) > 10000000)
			atomic_set(&obj->client[data_t->sensor_type].delay_count, 0);
		last_enter_timestamp_gyro = now_enter_timestamp;
		if (last_timestamp_ms_gyro != timestamp_ms) {
			last_timestamp_ms_gyro = timestamp_ms;
			gyro_data_report(data_t->gyroscope_t.x, data_t->gyroscope_t.y,
					 data_t->gyroscope_t.z, data_t->gyroscope_t.status,
					 (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt));
			atomic_inc(&obj->client[data_t->sensor_type].delay_count);
			if (atomic_read(&obj->client[data_t->sensor_type].delay_count) ==
			    DELAY_COUNT) {
				atomic_set(&obj->client[data_t->sensor_type].delay_count, 0);
				msleep(20);
			}
		}
#endif
		break;
	case ID_GYROSCOPE_UNCALIBRATED:
		value[0] = data_t->uncalibrated_gyro_t.x;
		value[1] = data_t->uncalibrated_gyro_t.y;
		value[2] = data_t->uncalibrated_gyro_t.z;
		value[3] = data_t->uncalibrated_gyro_t.x_bias;
		value[4] = data_t->uncalibrated_gyro_t.y_bias;
		value[5] = data_t->uncalibrated_gyro_t.z_bias;
#ifdef CONFIG_CUSTOM_KERNEL_UNCALI_GYRO_SENSOR
		if ((now_enter_timestamp - last_enter_timestamp_gyro_uncali) > 10000000)
			atomic_set(&obj->client[data_t->sensor_type].delay_count, 0);
		last_enter_timestamp_gyro_uncali = now_enter_timestamp;
		if (last_timestamp_ms_gyro_uncali != timestamp_ms) {
			last_timestamp_ms_gyro_uncali = timestamp_ms;
			uncali_gyro_data_report(value, data_t->uncalibrated_gyro_t.status,
						(int64_t) (data_t->time_stamp +
							   data_t->time_stamp_gpt));
			atomic_inc(&obj->client[data_t->sensor_type].delay_count);
			if (atomic_read(&obj->client[data_t->sensor_type].delay_count) ==
			    DELAY_COUNT / 2) {
				atomic_set(&obj->client[data_t->sensor_type].delay_count, 0);
				msleep(20);
			}
		}
#endif
		break;
	case ID_RELATIVE_HUMIDITY:
		break;
	case ID_MAGNETIC:
#ifdef CONFIG_CUSTOM_KERNEL_MAGNETOMETER
		if ((now_enter_timestamp - last_enter_timestamp_mag) > 10000000)
			atomic_set(&obj->client[data_t->sensor_type].delay_count, 0);
		last_enter_timestamp_mag = now_enter_timestamp;
		if (last_timestamp_ms_mag != timestamp_ms) {
			last_timestamp_ms_mag = timestamp_ms;
			magnetic_data_report(data_t->magnetic_t.x, data_t->magnetic_t.y,
					     data_t->magnetic_t.z, data_t->magnetic_t.status,
					     (int64_t) (data_t->time_stamp +
							data_t->time_stamp_gpt));
			atomic_inc(&obj->client[data_t->sensor_type].delay_count);
			if (atomic_read(&obj->client[data_t->sensor_type].delay_count) ==
			    DELAY_COUNT) {
				atomic_set(&obj->client[data_t->sensor_type].delay_count, 0);
				msleep(20);
			}
		}
#endif
		break;
	case ID_MAGNETIC_UNCALIBRATED:
		value[0] = data_t->uncalibrated_mag_t.x;
		value[1] = data_t->uncalibrated_mag_t.y;
		value[2] = data_t->uncalibrated_mag_t.z;
		value[3] = data_t->uncalibrated_mag_t.x_bias;
		value[4] = data_t->uncalibrated_mag_t.y_bias;
		value[5] = data_t->uncalibrated_mag_t.z_bias;
#ifdef CONFIG_CUSTOM_KERNEL_UNCALI_MAG_SENSOR
		if ((now_enter_timestamp - last_enter_timestamp_mag_uncali) > 10000000)
			atomic_set(&obj->client[data_t->sensor_type].delay_count, 0);
		last_enter_timestamp_mag_uncali = now_enter_timestamp;
		if (last_timestamp_ms_mag_uncali != timestamp_ms) {
			last_timestamp_ms_mag_uncali = timestamp_ms;
			uncali_mag_data_report(value, data_t->uncalibrated_mag_t.status,
					       (int64_t) (data_t->time_stamp +
							  data_t->time_stamp_gpt));
			atomic_inc(&obj->client[data_t->sensor_type].delay_count);
			if (atomic_read(&obj->client[data_t->sensor_type].delay_count) ==
			    DELAY_COUNT / 2) {
				atomic_set(&obj->client[data_t->sensor_type].delay_count, 0);
				msleep(20);
			}
		}
#endif
		break;
	case ID_GEOMAGNETIC_ROTATION_VECTOR:
#ifdef CONFIG_CUSTOM_KERNEL_GMRV_SENSOR
		if ((now_enter_timestamp - last_enter_timestamp_gmrv) > 10000000)
			atomic_set(&obj->client[data_t->sensor_type].delay_count, 0);
		last_enter_timestamp_gmrv = now_enter_timestamp;
		if (last_timestamp_ms_gmrv != timestamp_ms) {
			last_timestamp_ms_gmrv = timestamp_ms;
			gmrv_data_report(data_t->magnetic_t.x, data_t->magnetic_t.y,
					 data_t->magnetic_t.z, data_t->magnetic_t.scalar,
					 data_t->magnetic_t.status,
					 (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt));
			atomic_inc(&obj->client[data_t->sensor_type].delay_count);
			if (atomic_read(&obj->client[data_t->sensor_type].delay_count) ==
			    DELAY_COUNT) {
				atomic_set(&obj->client[data_t->sensor_type].delay_count, 0);
				msleep(20);
			}
		}
#endif
		break;
	case ID_ORIENTATION:
#ifdef CONFIG_CUSTOM_KERNEL_MAGNETOMETER
		if ((now_enter_timestamp - last_enter_timestamp_orientation) > 10000000)
			atomic_set(&obj->client[data_t->sensor_type].delay_count, 0);
		last_enter_timestamp_orientation = now_enter_timestamp;
		if (last_timestamp_ms_orientation != timestamp_ms) {
			last_timestamp_ms_orientation = timestamp_ms;
			orientation_data_report(data_t->orientation_t.azimuth,
						data_t->orientation_t.pitch,
						data_t->orientation_t.roll,
						data_t->orientation_t.status,
						(int64_t) (data_t->time_stamp +
							   data_t->time_stamp_gpt));
			atomic_inc(&obj->client[data_t->sensor_type].delay_count);
			if (atomic_read(&obj->client[data_t->sensor_type].delay_count) ==
			    DELAY_COUNT) {
				atomic_set(&obj->client[data_t->sensor_type].delay_count, 0);
				msleep(20);
			}
		}
#endif
		break;
	case ID_ROTATION_VECTOR:
#ifdef CONFIG_CUSTOM_KERNEL_RV_SENSOR
		if ((now_enter_timestamp - last_enter_timestamp_rv) > 10000000)
			atomic_set(&obj->client[data_t->sensor_type].delay_count, 0);
		last_enter_timestamp_rv = now_enter_timestamp;
		if (last_timestamp_ms_rv != timestamp_ms) {
			last_timestamp_ms_rv = timestamp_ms;
			rotationvector_data_report(data_t->orientation_t.azimuth,
						   data_t->orientation_t.pitch,
						   data_t->orientation_t.roll,
						   data_t->orientation_t.scalar,
						   data_t->orientation_t.status,
						   (int64_t) (data_t->time_stamp +
							      data_t->time_stamp_gpt));
			atomic_inc(&obj->client[data_t->sensor_type].delay_count);
			if (atomic_read(&obj->client[data_t->sensor_type].delay_count) ==
			    DELAY_COUNT) {
				atomic_set(&obj->client[data_t->sensor_type].delay_count, 0);
				msleep(20);
			}
		}
#endif
		break;
	case ID_GAME_ROTATION_VECTOR:
#ifdef CONFIG_CUSTOM_KERNEL_GRV_SENSOR
		if ((now_enter_timestamp - last_enter_timestamp_grv) > 10000000)
			atomic_set(&obj->client[data_t->sensor_type].delay_count, 0);
		last_enter_timestamp_grv = now_enter_timestamp;
		if (last_timestamp_ms_grv != timestamp_ms) {
			last_timestamp_ms_grv = timestamp_ms;
			grv_data_report(data_t->orientation_t.azimuth, data_t->orientation_t.pitch,
					data_t->orientation_t.roll, data_t->orientation_t.scalar,
					data_t->orientation_t.status,
					(int64_t) (data_t->time_stamp + data_t->time_stamp_gpt));
			atomic_inc(&obj->client[data_t->sensor_type].delay_count);
			if (atomic_read(&obj->client[data_t->sensor_type].delay_count) ==
			    DELAY_COUNT) {
				atomic_set(&obj->client[data_t->sensor_type].delay_count, 0);
				msleep(20);
			}
		}
#endif
		break;
	case ID_STEP_COUNTER:
		data.sensor = data_t->sensor_type;
		data.values[0] = data_t->step_counter_t.accumulated_step_count;
		data.time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_STEP_DETECTOR:
		data.sensor = data_t->sensor_type;
		data.values[0] = data_t->step_detector_t.step_detect;
		data.time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_TILT_DETECTOR:
		data.sensor = data_t->sensor_type;
		data.values[0] = data_t->tilt_event.state;
		data.time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_PEDOMETER:
		data.sensor = data_t->sensor_type;
		data.values[0] = data_t->pedometer_t.accumulated_step_count;
		data.values[1] = data_t->pedometer_t.accumulated_step_length;
		data.values[2] = data_t->pedometer_t.step_frequency;
		data.values[3] = data_t->pedometer_t.step_length;
		data.time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
#ifdef CONFIG_CUSTOM_KERNEL_PEDOMETER
		if ((now_enter_timestamp - last_enter_timestamp_pedo) > 10000000)
			atomic_set(&obj->client[data_t->sensor_type].delay_count, 0);
		last_enter_timestamp_pedo = now_enter_timestamp;
		if (last_timestamp_ms_pedo != timestamp_ms) {
			last_timestamp_ms_pedo = timestamp_ms;
			pedo_data_report(&data, 2);
			atomic_inc(&obj->client[data_t->sensor_type].delay_count);
			if (atomic_read(&obj->client[data_t->sensor_type].delay_count) ==
			    DELAY_COUNT) {
				atomic_set(&obj->client[data_t->sensor_type].delay_count, 0);
				msleep(20);
			}
		}
#endif
		break;
	case ID_PDR:
		data.sensor = data_t->sensor_type;
		data.values[0] = data_t->pdr_event.x;
		data.values[1] = data_t->pdr_event.y;
		data.values[2] = data_t->pdr_event.z;
		data.status = (int8_t) data_t->pdr_event.status;
		data.time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
		break;
	case ID_ACTIVITY:
		data.sensor = data_t->sensor_type;
		data.probability[STILL] = data_t->activity_data_t.probability[STILL];
		data.probability[STANDING] = data_t->activity_data_t.probability[STANDING];
		data.probability[SITTING] = data_t->activity_data_t.probability[SITTING];
		data.probability[LYING] = data_t->activity_data_t.probability[LYING];
		data.probability[ON_FOOT] = data_t->activity_data_t.probability[ON_FOOT];
		data.probability[WALKING] = data_t->activity_data_t.probability[WALKING];
		data.probability[RUNNING] = data_t->activity_data_t.probability[RUNNING];
		data.probability[CLIMBING] = data_t->activity_data_t.probability[CLIMBING];
		data.probability[ON_BICYCLE] = data_t->activity_data_t.probability[ON_BICYCLE];
		data.probability[IN_VEHICLE] = data_t->activity_data_t.probability[IN_VEHICLE];
		data.probability[TILTING] = data_t->activity_data_t.probability[TILTING];
		data.probability[UNKNOWN] = data_t->activity_data_t.probability[UNKNOWN];
		data.time = (int64_t) (data_t->time_stamp + data_t->time_stamp_gpt);
#ifdef CONFIG_CUSTOM_KERNEL_ACTIVITY_SENSOR
		if ((now_enter_timestamp - last_enter_timestamp_act) > 10000000)
			atomic_set(&obj->client[data_t->sensor_type].delay_count, 0);
		last_enter_timestamp_act = now_enter_timestamp;
		if (last_timestamp_ms_act != timestamp_ms) {
			last_timestamp_ms_act = timestamp_ms;
			act_data_report(&data, 2);
			atomic_inc(&obj->client[data_t->sensor_type].delay_count);
			if (atomic_read(&obj->client[data_t->sensor_type].delay_count) ==
			    DELAY_COUNT / 2) {
				atomic_set(&obj->client[data_t->sensor_type].delay_count, 0);
				msleep(20);
			}
		}
#endif
		break;
	default:
		err = -1;
		break;
	}
	return err;

}

static void clean_up_deactivate_ringbuffer(int handle)
{
	struct SCP_sensorHub_data *obj = obj_data;
	struct sensor_client *client = obj->client;

	if (test_bit(handle, &obj->flush_remain_data_after_disable) != 0) {
		clear_bit(handle, &obj->flush_remain_data_after_disable);
		switch (handle) {
		case ID_ACCELEROMETER:
		case ID_MAGNETIC:
		case ID_MAGNETIC_UNCALIBRATED:
		case ID_GYROSCOPE:
		case ID_GYROSCOPE_UNCALIBRATED:
		case ID_ORIENTATION:
		case ID_ROTATION_VECTOR:
		case ID_GAME_ROTATION_VECTOR:
		case ID_GEOMAGNETIC_ROTATION_VECTOR:
		case ID_GRAVITY:
		case ID_LINEAR_ACCELERATION:
		case ID_PEDOMETER:
		case ID_ACTIVITY:
			spin_lock(&client[handle].buffer_lock);
			client[handle].head = 0;
			client[handle].tail = 0;
			spin_unlock(&client[handle].buffer_lock);
			break;
		default:
			break;
		}
	}
}

static void wake_up_activate_thread(void)
{
	struct SCP_sensorHub_data *obj = obj_data;
	int handle = 0;
	struct sensor_client *client = obj->client;

	for (handle = ID_ACCELEROMETER; handle < ID_SENSOR_MAX_HANDLE; ++handle) {
		if (test_bit(handle, &obj->sensor_activate_bitmap) != 0 ||
		    test_bit(handle, &obj->flush_remain_data_after_disable) != 0) {
			switch (handle) {
			case ID_ACCELEROMETER:
			case ID_MAGNETIC:
			case ID_MAGNETIC_UNCALIBRATED:
			case ID_GYROSCOPE:
			case ID_GYROSCOPE_UNCALIBRATED:
			case ID_ORIENTATION:
			case ID_ROTATION_VECTOR:
			case ID_GAME_ROTATION_VECTOR:
			case ID_GEOMAGNETIC_ROTATION_VECTOR:
			case ID_GRAVITY:
			case ID_LINEAR_ACCELERATION:
			case ID_PEDOMETER:
			case ID_ACTIVITY:
				queue_work(client[handle].single_thread, &client[handle].worker);
				break;
			default:
				break;
			}
		}
	}
}

static int SCP_sensorHub_server_dispatch_data(void)
{
	struct SCP_sensorHub_data *obj = obj_data;
	char *pStart, *pEnd, *rp, *wp;
	struct data_unit_t *event;
	int err = 0, handle = 0;

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	wake_up_activate_thread();
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
			event = (struct data_unit_t *)rp;
			handle = event->sensor_type;
			sensor_server_write_ringbuffer_data(event, handle);
			rp += SENSOR_DATA_SIZE;
		}
	} else if (rp > wp) {
		while (rp < pEnd) {
			event = (struct data_unit_t *)rp;
			handle = event->sensor_type;
			sensor_server_write_ringbuffer_data(event, handle);
			rp += SENSOR_DATA_SIZE;
		}
		rp = pStart;
		while (rp < wp) {
			event = (struct data_unit_t *)rp;
			handle = event->sensor_type;
			sensor_server_write_ringbuffer_data(event, handle);
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
	wake_up_activate_thread();
	return 0;
}

static int SCP_sensorHub_get_fifo_status(int *dataLen, int *status, char *reserved,
					 struct batch_timestamp_info *p_batch_timestampe_info)
{
	struct SCP_sensorHub_data *obj = obj_data;
	int err = 0;
	/*SCP_SENSOR_HUB_DATA data; */
	char *pStart, *pEnd, *pNext;
	/*unsigned int len = 0; */
	char *rp, *wp;
	struct batch_timestamp_info *pt = p_batch_timestampe_info;
	int i, offset;

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();
	for (i = 0; i <= ID_SENSOR_MAX_HANDLE; i++)
		pt[i].total_count = 0;

	*dataLen = 0;
	*status = 1;

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

	err = release_scp_semaphore(SEMAPHORE_SENSOR);
	if (err < 0) {
		SCP_ERR("release_scp_semaphore fail : %d\n", err);
		return -3;
	}

	if (SCP_TRC_BATCH & atomic_read(&(obj_data->trace)))
		SCP_ERR("FIFO pStart = %p, rp = %p, wp = %p, pEnd = %p\n", pStart, rp, wp, pEnd);

	if (rp < pStart || pEnd <= rp) {
		SCP_ERR("FIFO rp invalid : %p, %p, %p\n", pStart, pEnd, rp);
		return -4;
	}

	if (wp < pStart || pEnd < wp) {
		SCP_ERR("FIFO wp invalid : %p, %p, %p\n", pStart, pEnd, wp);
		return -5;
	}

	if (rp == wp) {
		SCP_ERR("FIFO empty\n");
		return -6;
	}


	while (rp != wp) {
		struct data_unit_t *pdata = (struct data_unit_t *)rp;

		pNext = rp + SENSOR_DATA_SIZE;

		if (SCP_TRC_BATCH_DETAIL & atomic_read(&(obj_data->trace)))
			SCP_LOG("rp = %p, sensor_type = %d, pNext = %p\n", rp,
				pdata->sensor_type, pNext);

		if (pdata->sensor_type < 0 || pdata->sensor_type > 58) {
			SCP_ERR("Wrong data, sensor_type = %d .\n", pdata->sensor_type);
			return -7;
		}

		pt[pdata->sensor_type].total_count++;

		if (pNext < pEnd)
			rp = pNext;
		else {
			offset = (int)(pNext - pEnd);
			rp = pStart + offset;
		}
		(*dataLen)++;
	}
	if (SCP_TRC_BATCH & atomic_read(&(obj_data->trace)))
		SCP_ERR("dataLen = %d, status = %d\n", *dataLen, *status);
	pStart = (char *)obj->SCP_sensorFIFO + offsetof(struct sensorFIFO, data);
	pEnd = (char *)pStart + obj->SCP_sensorFIFO->FIFOSize;
	rp = pStart + (int)obj->SCP_sensorFIFO->rp;
	wp = pStart + (int)obj->SCP_sensorFIFO->wp;
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
	struct SCP_sensorHub_data *obj = obj_data;

	if (enabledisable == 1) {
		set_bit(sensorType, &obj->sensor_activate_bitmap);
		clear_bit(sensorType, &obj->flush_remain_data_after_disable);
		register_feature(SENS_FEATURE_ID);
	} else {
		clear_bit(sensorType, &obj->sensor_activate_bitmap);
		set_bit(sensorType, &obj->flush_remain_data_after_disable);
	}
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
	int err = 0, handle = 0;
	struct batch_control_path ctl = { 0 };
	struct batch_data_path data = { 0 };

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
	for (handle = ID_ACCELEROMETER; handle < ID_SENSOR_MAX_HANDLE; handle++) {
		switch (handle) {
		case ID_ACCELEROMETER:
		case ID_MAGNETIC:
		case ID_MAGNETIC_UNCALIBRATED:
		case ID_ORIENTATION:
		case ID_ROTATION_VECTOR:
		case ID_GAME_ROTATION_VECTOR:
		case ID_GEOMAGNETIC_ROTATION_VECTOR:
		case ID_GRAVITY:
		case ID_LINEAR_ACCELERATION:
		case ID_PEDOMETER:
			sprintf(obj->client[handle].name, "sensor%d", handle);
			spin_lock_init(&obj->client[handle].buffer_lock);
			atomic_set(&obj->client[handle].delay_count, 0);
			obj->client[handle].head = 0;
			obj->client[handle].tail = 0;
			obj->client[handle].bufsize = SCP_KFIFO_BUFFER_SIZE;
			obj->client[handle].ringbuffer =
			    vzalloc(obj->client[handle].bufsize * sizeof(struct data_unit_t));
			if (!obj->client[handle].ringbuffer) {
				SCP_ERR("Alloc ringbuffer error!\n");
				goto exit;
			}
			obj->client[handle].single_thread =
			    create_singlethread_workqueue(obj->client[handle].name);
			break;
		case ID_ACTIVITY:
			sprintf(obj->client[handle].name, "sensor%d", handle);
			spin_lock_init(&obj->client[handle].buffer_lock);
			atomic_set(&obj->client[handle].delay_count, 0);
			obj->client[handle].head = 0;
			obj->client[handle].tail = 0;
			obj->client[handle].bufsize = SCP_KFIFO_BUFFER_SIZE * 128;
			obj->client[handle].ringbuffer =
			    vzalloc(obj->client[handle].bufsize * sizeof(struct data_unit_t));
			if (!obj->client[handle].ringbuffer) {
				SCP_ERR("Alloc ringbuffer error!\n");
				goto exit;
			}
			obj->client[handle].single_thread =
			    create_singlethread_workqueue(obj->client[handle].name);
			break;
		case ID_GYROSCOPE:
		case ID_GYROSCOPE_UNCALIBRATED:
			sprintf(obj->client[handle].name, "sensor%d", handle);
			spin_lock_init(&obj->client[handle].buffer_lock);
			atomic_set(&obj->client[handle].delay_count, 0);
			obj->client[handle].head = 0;
			obj->client[handle].tail = 0;
			obj->client[handle].bufsize = SCP_KFIFO_BUFFER_SIZE * 2;
			obj->client[handle].ringbuffer =
			    vzalloc(obj->client[handle].bufsize * sizeof(struct data_unit_t));
			if (!obj->client[handle].ringbuffer) {
				SCP_ERR("Alloc ringbuffer error!\n");
				goto exit;
			}
			obj->client[handle].single_thread =
			    create_singlethread_workqueue(obj->client[handle].name);
			break;
		default:
			break;
		}
	}
	INIT_WORK(&obj->client[ID_ACCELEROMETER].worker, accelerometer_report_data_work);
	INIT_WORK(&obj->client[ID_MAGNETIC].worker, magnetic_report_data_work);
	INIT_WORK(&obj->client[ID_MAGNETIC_UNCALIBRATED].worker, unmagnetic_report_data_work);
	INIT_WORK(&obj->client[ID_GYROSCOPE].worker, gyroscope_report_data_work);
	INIT_WORK(&obj->client[ID_GYROSCOPE_UNCALIBRATED].worker, ungyroscope_report_data_work);
	INIT_WORK(&obj->client[ID_ORIENTATION].worker, orientation_report_data_work);
	INIT_WORK(&obj->client[ID_ROTATION_VECTOR].worker, rotvec_report_data_work);
	INIT_WORK(&obj->client[ID_GAME_ROTATION_VECTOR].worker, gamerotvec_report_data_work);
	INIT_WORK(&obj->client[ID_GEOMAGNETIC_ROTATION_VECTOR].worker,
		  geomagnetic_report_data_work);
	INIT_WORK(&obj->client[ID_GRAVITY].worker, gravity_report_data_work);
	INIT_WORK(&obj->client[ID_LINEAR_ACCELERATION].worker, linearaccel_report_data_work);
	INIT_WORK(&obj->client[ID_PEDOMETER].worker, pedometer_report_data_work);
	INIT_WORK(&obj->client[ID_ACTIVITY].worker, activity_report_data_work);

	obj->sensor_activate_bitmap = 0;
	obj->flush_remain_data_after_disable = 0;
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

	err = misc_register(&SCP_sensorHub_device);
	if (err) {
		SCP_ERR("SCP_sensorHub_device register failed\n");
		goto exit_misc_device_register_failed;
	}

	err = SCP_sensorHub_create_attr(&(SCP_sensorHub_init_info.platform_diver_addr->driver));
	if (err) {
		SCP_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}

	ctl.enable_hw_batch = SCP_sensorHub_enable_hw_batch;
	ctl.flush = SCP_sensorHub_flush;
	err = batch_register_control_path(ID_SENSOR_MAX_HANDLE, &ctl);
	if (err) {
		SCP_ERR("register SCP sensor hub control path err\n");
		goto exit_kfree;
	}

	data.get_data = SCP_sensorHub_get_data;
	data.get_fifo_status = SCP_sensorHub_get_fifo_status;
	data.batch_timeout = SCP_sensorHub_batch_timeout;
	data.is_batch_supported = 1;
	err = batch_register_data_path(ID_SENSOR_MAX_HANDLE, &data);
	if (err) {
		SCP_ERR("register SCP sensor hub control data path err\n");
		goto exit_kfree;
	}
	SCP_sensorHub_init_flag = 0;
	SCP_sensorHub_init_client();
	SCP_ERR("fwq init done\n");

	return 0;

exit_create_attr_failed:
	misc_deregister(&SCP_sensorHub_device);
exit_misc_device_register_failed:
exit_kfree:
	kfree(obj);
exit:
	SCP_ERR("%s: err = %d\n", __func__, err);
	SCP_sensorHub_init_flag = -1;
	return err;
}

static int sensorHub_remove(struct platform_device *pdev)
{
	int err = 0;

	err = SCP_sensorHub_delete_attr(&(SCP_sensorHub_init_info.platform_diver_addr->driver));
	if (err)
		SCP_ERR("SCP_sensorHub_delete_attr fail: %d\n", err);

	err = misc_deregister(&SCP_sensorHub_device);
	if (err)
		SCP_ERR("misc_deregister fail: %d\n", err);
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

static int SCP_sensorHub_local_init(void)
{
	if (platform_driver_register(&sensorHub_driver)) {
		SCP_ERR("add driver error\n");
		return -1;
	}
	if (-1 == SCP_sensorHub_init_flag)
		return -1;
	return 0;
}

static int SCP_sensorHub_local_remove(void)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	platform_driver_unregister(&sensorHub_driver);
	return 0;
}

static int __init SCP_sensorHub_init(void)
{
	SCP_FUN();
	if (platform_device_register(&sensorHub_device)) {
		SCP_ERR("SCP_sensorHub platform device error\n");
		return -1;
	}
	batch_driver_add(&SCP_sensorHub_init_info);
	return 0;
}

static void __exit SCP_sensorHub_exit(void)
{
	SCP_FUN();
}
module_init(SCP_sensorHub_init);
module_exit(SCP_sensorHub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SCP sensor hub driver");
MODULE_AUTHOR("andrew.yang@mediatek.com");
