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

#include "step_counter.h"
#include "pedometer.h"
#include "activity.h"
#include "in_pocket.h"
#include "face_down.h"
#include "pick_up.h"
#include "shake.h"
#include "heart_rate.h"
#include "tilt_detector.h"
#include "wake_gesture.h"
#include "glance_gesture.h"
#include <batch.h>
#include <mach/md32_ipi.h>
#include <linux/time.h>

#define POWER_NONE_MACRO MT65XX_POWER_NONE

#include <hwmsensor.h>
#include <hwmsen_dev.h>
#include <sensors_io.h>
#include "SCP_sensorHub.h"
#include "cust_sensorHub.h"
#include <hwmsen_helper.h>
#include <mach/mt_clkmgr.h>
#include <scp_helper.h>
/*----------------------------------------------------------------------------*/
/* #define DEBUG 1 */
/* #define SENSORHUB_UT */
/*----------------------------------------------------------------------------*/
/* #define CONFIG_SCP_sensorHub_LOWPASS   //apply low pass filter on output */
#define SW_CALIBRATION
/*----------------------------------------------------------------------------*/
#define SCP_sensorHub_AXIS_X          0
#define SCP_sensorHub_AXIS_Y          1
#define SCP_sensorHub_AXIS_Z          2
#define SCP_sensorHub_AXES_NUM        3
#define SCP_sensorHub_DATA_LEN        6
#define SCP_sensorHub_DEV_NAME        "SCP_sensorHub"

/*----------------------------------------------------------------------------*/
static int SCP_sensorHub_probe(void);
static int SCP_sensorHub_remove(void);
/* static int SCP_sensorHub_suspend(struct platform_device *dev, pm_message_t state); */
/* static int SCP_sensorHub_resume(struct platform_device *dev); */

static int SCP_sensorHub_local_init(void);
#ifdef CONFIG_CUSTOM_KERNEL_STEP_COUNTER
static void SCP_sd_work(struct work_struct *work);
static void SCP_sig_work(struct work_struct *work);
static struct wake_lock sig_lock;
static int SCP_sensorHub_step_counter_init(void);
static int SCP_sensorHub_step_counter_uninit(void);
static void notify_ap_timeout(unsigned long);
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_STEP_COUNTER */

#ifdef CONFIG_CUSTOM_KERNEL_IN_POCKET_SENSOR
static void SCP_inpk_work(struct work_struct *work);
static int SCP_sensorHub_in_pocket_init(void);
static int SCP_sensorHub_in_pocket_uninit(void);
#endif				/* CONFIG_CUSTOM_KERNEL_IN_POCKET_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_PEDOMETER
/* static void SCP_pdr_work(struct work_struct *work); */
static int SCP_sensorHub_pedometer_init(void);
static int SCP_sensorHub_pedometer_uninit(void);
#endif				/* CONFIG_CUSTOM_KERNEL_PEDOMETER */
#ifdef CONFIG_CUSTOM_KERNEL_ACTIVITY_SENSOR
/* static void SCP_act_work(struct work_struct *work); */
static int SCP_sensorHub_activity_init(void);
static int SCP_sensorHub_activity_uninit(void);
#endif				/* CONFIG_CUSTOM_KERNEL_ACTIVITY_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_SHAKE_SENSOR
static void SCP_shk_work(struct work_struct *work);
static int SCP_sensorHub_shake_init(void);
static int SCP_sensorHub_shake_uninit(void);
#endif				/* CONFIG_CUSTOM_KERNEL_SHAKE_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_PICK_UP_SENSOR
static void SCP_pkup_work(struct work_struct *work);
static int SCP_sensorHub_pick_up_init(void);
static int SCP_sensorHub_pick_up_uninit(void);
#endif				/* CONFIG_CUSTOM_KERNEL_PICK_UP_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_FACE_DOWN_SENSOR
static void SCP_fdn_work(struct work_struct *work);
static int SCP_sensorHub_face_down_init(void);
static int SCP_sensorHub_face_down_uninit(void);
#endif				/* CONFIG_CUSTOM_KERNEL_FACE_DOWN_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_HEART_RATE_SENSOR
/* static void SCP_fdn_work(struct work_struct *work); */
static int SCP_sensorHub_heart_rate_init(void);
static int SCP_sensorHub_heart_rate_uninit(void);
#endif				/* CONFIG_CUSTOM_KERNEL_HEART_RATE_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_TILT_DETECTOR_SENSOR
static void SCP_tilt_work(struct work_struct *work);
static int SCP_sensorHub_tilt_detector_init(void);
static int SCP_sensorHub_tilt_detector_uninit(void);
#endif				/* CONFIG_CUSTOM_KERNEL_TILT_DETECTOR_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_WAKE_GESTURE_SENSOR
static void SCP_wag_work(struct work_struct *work);
static int SCP_sensorHub_wake_gesture_init(void);
static int SCP_sensorHub_wake_gesture_uninit(void);
#endif				/* CONFIG_CUSTOM_KERNEL_WAKE_GESTURE_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_GLANCE_GESTURE_SENSOR
static void SCP_glg_work(struct work_struct *work);
static int SCP_sensorHub_glance_gesture_init(void);
static int SCP_sensorHub_glance_gesture_uninit(void);
#endif				/* CONFIG_CUSTOM_KERNEL_GLANCE_GESTURE_SENSOR */

/*----------------------------------------------------------------------------*/
typedef enum {
	SCP_TRC_FUN = 0x01,
	SCP_TRC_IPI = 0x02,
	SCP_TRC_BATCH = 0x04,
	SCP_TRC_BATCH_DETAIL = 0x08,
} SCP_TRC;
/*----------------------------------------------------------------------------*/
SCP_sensorHub_handler sensor_handler[ID_SENSOR_MAX_HANDLE + 1];
/*----------------------------------------------------------------------------*/
#define C_MAX_FIR_LENGTH (32)
/* #define USE_EARLY_SUSPEND */
static DEFINE_MUTEX(SCP_sensorHub_op_mutex);
static DEFINE_MUTEX(SCP_sensorHub_req_mutex);
static DECLARE_WAIT_QUEUE_HEAD(SCP_sensorHub_req_wq);

static int SCP_sensorHub_init_flag = -1;	/* 0<==>OK -1 <==> fail */

static struct batch_init_info SCP_sensorHub_init_info = {
	.name = "SCP_sensorHub",
	.init = SCP_sensorHub_local_init,
	.uninit = SCP_sensorHub_remove,
	.platform_diver_addr = NULL,
};

#ifdef CONFIG_CUSTOM_KERNEL_STEP_COUNTER
static struct step_c_init_info SCP_step_counter_init_info = {
	.name = "SCP_step_counter",
	.init = SCP_sensorHub_step_counter_init,
	.uninit = SCP_sensorHub_step_counter_uninit,
};
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_STEP_COUNTER */


#ifdef CONFIG_CUSTOM_KERNEL_IN_POCKET_SENSOR
static struct inpk_init_info SCP_in_pocket_init_info = {
	.name = "SCP_in_pocket",
	.init = SCP_sensorHub_in_pocket_init,
	.uninit = SCP_sensorHub_in_pocket_uninit,
};
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_IN_POCKET_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_PEDOMETER
static struct pdr_init_info SCP_pedometer_init_info = {
	.name = "SCP_pedometer",
	.init = SCP_sensorHub_pedometer_init,
	.uninit = SCP_sensorHub_pedometer_uninit,
};
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_PEDOMETER */
#ifdef CONFIG_CUSTOM_KERNEL_ACTIVITY_SENSOR
static struct act_init_info SCP_activity_init_info = {
	.name = "SCP_activity",
	.init = SCP_sensorHub_activity_init,
	.uninit = SCP_sensorHub_activity_uninit,
};
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_ACTIVITY_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_SHAKE_SENSOR
static struct shk_init_info SCP_shake_init_info = {
	.name = "SCP_shake",
	.init = SCP_sensorHub_shake_init,
	.uninit = SCP_sensorHub_shake_uninit,
};
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_SHAKE_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_PICK_UP_SENSOR
static struct pkup_init_info SCP_pick_up_init_info = {
	.name = "SCP_pick_up",
	.init = SCP_sensorHub_pick_up_init,
	.uninit = SCP_sensorHub_pick_up_uninit,
};
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_PICK_UP_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_FACE_DOWN_SENSOR
static struct fdn_init_info SCP_face_down_init_info = {
	.name = "SCP_face_down",
	.init = SCP_sensorHub_face_down_init,
	.uninit = SCP_sensorHub_face_down_uninit,
};
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_FACE_DOWN_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_HEART_RATE_SENSOR
static struct hrm_init_info SCP_heart_rate_init_info = {
	.name = "SCP_heart_rate",
	.init = SCP_sensorHub_heart_rate_init,
	.uninit = SCP_sensorHub_heart_rate_uninit,
};
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_HEART_RATE_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_TILT_DETECTOR_SENSOR
static struct tilt_init_info SCP_tilt_detector_init_info = {
	.name = "SCP_tilt_detector",
	.init = SCP_sensorHub_tilt_detector_init,
	.uninit = SCP_sensorHub_tilt_detector_uninit,
};
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_TILT_DETECTOR_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_WAKE_GESTURE_SENSOR
static struct wag_init_info SCP_wake_gesture_init_info = {
	.name = "SCP_wake_gesture",
	.init = SCP_sensorHub_wake_gesture_init,
	.uninit = SCP_sensorHub_wake_gesture_uninit,
};
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_WAKE_GESTURE_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_GLANCE_GESTURE_SENSOR
static struct glg_init_info SCP_glance_gesture_init_info = {
	.name = "SCP_glance_gesture",
	.init = SCP_sensorHub_glance_gesture_init,
	.uninit = SCP_sensorHub_glance_gesture_uninit,
};
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_GLANCE_GESTURE_SENSOR */



/*----------------------------------------------------------------------------*/
struct data_filter {
	s16 raw[C_MAX_FIR_LENGTH][SCP_sensorHub_AXES_NUM];
	int sum[SCP_sensorHub_AXES_NUM];
	int num;
	int idx;
};
/*----------------------------------------------------------------------------*/
struct SCP_sensorHub_data {
	struct sensorHub_hw *hw;
	struct work_struct ipi_work;
	struct work_struct fifo_full_work;
	struct work_struct sd_work;	/* step detect work */
	struct work_struct sig_work;	/* significant motion work */
	/* struct work_struct            pdr_work; //pedometer work */
	/* struct work_struct            act_work; //activity work */
	struct work_struct inpk_work;	/* in pocket work */
	struct work_struct pkup_work;	/* pick up work */
	struct work_struct fdn_work;	/* face down work */
	struct work_struct shk_work;	/* shake work */
	struct work_struct tilt_work;	/* tilt detector work */
	struct work_struct wag_work;	/* wake gesture work */
	struct work_struct glg_work;	/* glance gesture work */
	struct timer_list timer;
	struct timer_list notify_timer;

	/*misc */
	atomic_t trace;
	atomic_t suspend;
	atomic_t filter;
	s16 cali_sw[SCP_sensorHub_AXES_NUM + 1];
	atomic_t wait_rsp;
	atomic_t ipi_handler_running;
	atomic_t disable_fifo_full_notify;

	/*data */
	s8 offset[SCP_sensorHub_AXES_NUM + 1];	/*+1: for 4-byte alignment */
	s16 data[SCP_sensorHub_AXES_NUM + 1];

	volatile struct sensorFIFO *volatile SCP_sensorFIFO;
	dma_addr_t mapping;

#if defined(CONFIG_SCP_sensorHub_LOWPASS)
	atomic_t firlen;
	atomic_t fir_en;
	struct data_filter fir;
#endif
};

static struct device SCP_sensorHub_dev = {
	.init_name = "SCPdmadev",
	.coherent_dma_mask = ~0,	/* dma_alloc_coherent(): allow any address */
	.dma_mask = &SCP_sensorHub_dev.coherent_dma_mask,	/* other APIs: use the same mask as coherent */
};

/*----------------------------------------------------------------------------*/
static struct SCP_sensorHub_data *obj_data;
static SCP_SENSOR_HUB_DATA_P userData;
static uint *userDataLen;
/*----------------------------------------------------------------------------*/
#define SCP_TAG                  "[sensorHub] "
#define SCP_FUN(f)               pr_debug(SCP_TAG"%s\n", __func__)
#define SCP_ERR(fmt, args...)    pr_err(SCP_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define SCP_LOG(fmt, args...)    pr_debug(SCP_TAG fmt, ##args)
/*--------------------SCP_sensorHub power control function----------------------------------*/
static void SCP_sensorHub_power(struct sensorHub_hw *hw, unsigned int on)
{
}

/*----------------------------------------------------------------------------*/
static unsigned long long SCP_sensorHub_GetCurNS(void)
{
	return sched_clock();
}

/*----------------------------------------------------------------------------*/
/* md32 may lock hw semaphore about 6.x ms to push data to dram. */
static int SCP_sensorHub_get_semaphore(void)
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
			SCP_ERR("get_scp_semaphore fail : %d, %lld, %lld\n", err, start_nt,
				cur_nt);
		} else {
			return err;
		}
	} while ((cur_nt - start_nt) < 20000000);	/* try 10 ms to get hw semaphore */

	SCP_ERR("get_scp_semaphore timeout : %d, %lld, %lld\n", err, start_nt, cur_nt);
	return err;
}

/*----------------------------------------------------------------------------*/
static int SCP_sensorHub_init_client(void)	/* call by init done workqueue */
{
	struct SCP_sensorHub_data *obj = obj_data;
	SCP_SENSOR_HUB_DATA data;
	unsigned int len = 0;

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	/* enable_clock(MT_CG_INFRA_APDMA, "sensorHub"); */
	/* SCP_ERR("obj=%lld\n", obj); */

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

	SCP_ERR("SCP_sensorHub_init_client done\n");

	return SCP_SENSOR_HUB_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int SCP_sensorHub_ReadChipInfo(char *buf, int bufsize)
{
	if ((NULL == buf) || (bufsize <= 30))
		return -1;

	sprintf(buf, "SCP_sensorHub Chip");
	return 0;
}

/*----------------------------------------------------------------------------*/
static int SCP_sensorHub_ReadSensorData(int handle, hwm_sensor_data *sensorData)
{
	struct SCP_sensorHub_data *obj = obj_data;
	char *pStart, *pEnd, *pNext;
	struct SCP_sensorData curData;
	char *rp, *wp;
	int offset;
	int fifo_usage;
	int err;

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	if (NULL == sensorData)
		return -1;

	err = SCP_sensorHub_get_semaphore();
	if (err < 0) {
		SCP_ERR("SCP_sensorHub_get_semaphore fail : %d\n", err);
		return -2;
	}

	dma_sync_single_for_cpu(&SCP_sensorHub_dev, obj->mapping, obj->SCP_sensorFIFO->FIFOSize,
				DMA_FROM_DEVICE);
	pStart = (char *)obj->SCP_sensorFIFO + offsetof(struct sensorFIFO, data);
	pEnd = (char *)pStart + obj->SCP_sensorFIFO->FIFOSize;
	rp = pStart + (int)obj->SCP_sensorFIFO->rp;
	wp = pStart + (int)obj->SCP_sensorFIFO->wp;

	if (rp < pStart || pEnd <= rp) {
		SCP_ERR("FIFO rp invalid : %p, %p, %p\n", pStart, pEnd, rp);
		err = release_scp_semaphore(SEMAPHORE_SENSOR);
		if (err < 0) {
			SCP_ERR("release_md32_semaphore fail : %d\n", err);
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
		pNext =
		    rp + offsetof(struct SCP_sensorData,
				  data) + ((struct SCP_sensorData *)rp)->dataLength;
		pNext = (char *)((((unsigned long)pNext + 3) >> 2) << 2);

		if (SCP_TRC_BATCH_DETAIL & atomic_read(&(obj_data->trace)))
			SCP_LOG("dataLength = %d, pNext = %p, rp = %p, wp = %p\n",
				((struct SCP_sensorData *)rp)->dataLength, pNext, rp, wp);

		if (((struct SCP_sensorData *)rp)->dataLength != 6
		    && ((struct SCP_sensorData *)rp)->dataLength != 8)
			SCP_ERR("Wrong dataLength = %d\n",
				((struct SCP_sensorData *)rp)->dataLength);

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
		dma_sync_single_for_device(&SCP_sensorHub_dev, obj->mapping,
				obj->SCP_sensorFIFO->FIFOSize, DMA_TO_DEVICE);
		err = release_scp_semaphore(SEMAPHORE_SENSOR);
		if (err < 0)	/* allow scp to access dram */
			SCP_ERR("release_scp_semaphore fail : %d\n", err);

		sensorData->sensor = curData.sensorType;
		sensorData->value_divide = 1000;	/* need to check */
		sensorData->status = SENSOR_STATUS_ACCURACY_MEDIUM;
		sensorData->values[0] = curData.data[0];
		sensorData->values[1] = curData.data[1];
		sensorData->values[2] = curData.data[2];

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

/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	char strbuf[SCP_SENSOR_HUB_TEMP_BUFSIZE];

	SCP_sensorHub_ReadChipInfo(strbuf, SCP_SENSOR_HUB_TEMP_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
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

/*----------------------------------------------------------------------------*/
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

/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(chipinfo, S_IWUSR | S_IRUGO, show_chipinfo_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, show_trace_value, store_trace_value);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *SCP_sensorHub_attr_list[] = {
	&driver_attr_chipinfo,	/*chip information */
	&driver_attr_trace,	/*trace log */
};

/*----------------------------------------------------------------------------*/
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

/*----------------------------------------------------------------------------*/
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

/******************************************************************************
 * Function Configuration
******************************************************************************/
static int SCP_sensorHub_open(struct inode *inode, struct file *file)
{
	file->private_data = obj_data;

	if (file->private_data == NULL) {
		SCP_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}

/*----------------------------------------------------------------------------*/
static int SCP_sensorHub_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

/*----------------------------------------------------------------------------*/
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

#if IS_ENABLED(CONFIG_COMPAT)
static long compat_SCP_sensorHub_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		SCP_ERR("compat_ion_ioctl file has no f_op or no f_op->unlocked_ioctl.\n");
		return -ENOTTY;
	}

	switch (cmd) {
	case COMPAT_GSENSOR_IOCTL_INIT:
	case COMPAT_GSENSOR_IOCTL_READ_CHIPINFO:
	/* case COMPAT_GSENSOR_IOCTL_READ_GAIN: */
	case COMPAT_GSENSOR_IOCTL_READ_RAW_DATA:
	case COMPAT_GSENSOR_IOCTL_READ_SENSORDATA:
		/* NVRAM will use below ioctl */
	case COMPAT_GSENSOR_IOCTL_SET_CALI:
	case COMPAT_GSENSOR_IOCTL_CLR_CALI:
	case COMPAT_GSENSOR_IOCTL_GET_CALI:{
			SCP_LOG("compat_ion_ioctl : GSENSOR_IOCTL_XXX command is 0x%x\n", cmd);
			return filp->f_op->unlocked_ioctl(filp, cmd,
							  (unsigned long)compat_ptr(arg));
		}
	default:{
			SCP_ERR("compat_ion_ioctl : No such command!! 0x%x\n", cmd);
			return -ENOIOCTLCMD;
		}
	}
}
#endif

/*----------------------------------------------------------------------------*/
static const struct file_operations SCP_sensorHub_fops = {
	/* .owner = THIS_MODULE, */
	.open = SCP_sensorHub_open,
	.release = SCP_sensorHub_release,
	.unlocked_ioctl = SCP_sensorHub_unlocked_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = compat_SCP_sensorHub_unlocked_ioctl,
#endif
};

/*----------------------------------------------------------------------------*/
static struct miscdevice SCP_sensorHub_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "SCP_sensorHub",
	.fops = &SCP_sensorHub_fops,
};
/*----------------------------------------------------------------------------*/
#if 0
static int SCP_sensorHub_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

/*----------------------------------------------------------------------------*/
static int SCP_sensorHub_resume(struct platform_device *dev)
{
	return 0;
}

/*----------------------------------------------------------------------------*/
#endif
/*----------------------------------------------------------------------------*/
static unsigned long long t1, t2, t3, t4, t5, t6;
int SCP_sensorHub_req_send(SCP_SENSOR_HUB_DATA_P data, uint *len, unsigned int wait)
{
	ipi_status status;
	int err = 0;

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
	do {
		status = md32_ipi_send(IPI_SENSOR, data, *len, wait);
		if (ERROR == status) {
			SCP_ERR("md32_ipi_send ERROR\n");
			mutex_unlock(&SCP_sensorHub_req_mutex);
			return -1;
		}
	} while (BUSY == status);
	if (SCP_TRC_IPI & atomic_read(&(obj_data->trace)))
		SCP_ERR("md32_ipi_send DONE\n");
	mod_timer(&obj_data->timer, jiffies + 3 * HZ);
	wait_event_interruptible(SCP_sensorHub_req_wq,
				 (atomic_read(&(obj_data->wait_rsp)) == 0));
	del_timer_sync(&obj_data->timer);
	err = userData->rsp.errCode;
	if (t6 - t1 > 3000000LL)
		SCP_ERR("%llu, %llu, %llu, %llu, %llu, %llu\n", t1, t2, t3, t4, t5, t6);
	mutex_unlock(&SCP_sensorHub_req_mutex);

	if (SCP_TRC_IPI & atomic_read(&(obj_data->trace)))
		SCP_ERR("SCP_sensorHub_req_send end\n");

	return err;
}

/*----------------------------------------------------------------------------*/
int SCP_sensorHub_rsp_registration(int sensor, SCP_sensorHub_handler handler)
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

/*----------------------------------------------------------------------------*/
static void SCP_ipi_work(struct work_struct *work)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	SCP_sensorHub_init_client();
}

/*----------------------------------------------------------------------------*/
static void SCP_fifo_full_work(struct work_struct *work)
{
	if (SCP_TRC_FUN & atomic_read(&(obj_data->trace)))
		SCP_FUN();

	batch_notify(TYPE_BATCHFULL);
}

/*----------------------------------------------------------------------------*/
static void SCP_sensorHub_req_send_timeout(unsigned long data)
{
	if (atomic_read(&(obj_data->wait_rsp)) == 1) {
		if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
			SCP_FUN();

		if (NULL != userData && NULL != userDataLen) {
			userData->rsp.errCode = -1;
			*userDataLen = sizeof(userData->rsp);
		}

		atomic_set(&(obj_data->wait_rsp), 0);
		wake_up(&SCP_sensorHub_req_wq);
	}
}

/*----------------------------------------------------------------------------*/
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
				if (atomic_read(&obj->disable_fifo_full_notify) == 0) {
					atomic_set(&obj->disable_fifo_full_notify, 1);
					schedule_work(&obj->fifo_full_work);
				} else {
					SCP_ERR("SCP_FIFO_FULL disabled\n");
				}
				break;
			case SCP_NOTIFY:
				do_registed_handler = true;
				break;
			default:
				break;
			}
			break;
		default:
			SCP_ERR("SCP_sensorHub_IPI_handler unknown action=%d error\n",
				rsp->rsp.action);
			return;
		}

		t2 = SCP_sensorHub_GetCurNS();

		if (ID_SENSOR_MAX_HANDLE < rsp->rsp.sensorType) {
			SCP_ERR("SCP_sensorHub_IPI_handler invalid sensor type %d\n",
				rsp->rsp.sensorType);
			return;
		} else if (true == do_registed_handler) {
			if (NULL != sensor_handler[rsp->rsp.sensorType])
				sensor_handler[rsp->rsp.sensorType] (data, len);
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

/*----------------------------------------------------------------------------*/
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

/*----------------------------------------------------------------------------*/
static int SCP_sensorHub_flush(int handle)
{
	return 0;
}

/*----------------------------------------------------------------------------*/
static int SCP_sensorHub_get_data(int handle, hwm_sensor_data *sensorData)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensorHub_ReadSensorData(handle, sensorData);
}

/*----------------------------------------------------------------------------*/
static int SCP_sensorHub_get_fifo_status(int *dataLen, int *status, char *reserved,
					 struct batch_timestamp_info *p_batch_timestampe_info)
{
	struct SCP_sensorHub_data *obj = obj_data;
	int err = 0;
	SCP_SENSOR_HUB_DATA data;
	char *pStart, *pEnd, *pNext;
	unsigned int len = 0;
	char *rp, *wp;
	struct batch_timestamp_info *pt = p_batch_timestampe_info;
	int i, offset;

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();
	for (i = 0; i <= MAX_ANDROID_SENSOR_NUM; i++)
		pt[i].total_count = 0;

	*dataLen = 0;
	*status = 1;

	data.get_data_req.sensorType = 0;
	data.get_data_req.action = SENSOR_HUB_GET_DATA;
	len = sizeof(data.get_data_req);

	err = SCP_sensorHub_req_send(&data, &len, 1);
	if (0 != err) {
		SCP_ERR("SCP_sensorHub_req_send error: ret value=%d\n", err);
		return -1;
	}
		/* To prevent get fifo status during scp wrapper around dram fifo. */
		err = SCP_sensorHub_get_semaphore();
		if (err < 0) {
			SCP_ERR("SCP_sensorHub_get_semaphore fail : %d\n", err);
			return -2;
		}
		dma_sync_single_for_cpu(&SCP_sensorHub_dev, obj->mapping,
					obj->SCP_sensorFIFO->FIFOSize, DMA_FROM_DEVICE);
		/* No data need to sync. back to device, release semaphore immediately. */
		err = release_scp_semaphore(SEMAPHORE_SENSOR);
		if (err < 0) {
			SCP_ERR("release_scp_semaphore fail : %d\n", err);
			return -3;
		}

		pStart = (char *)obj->SCP_sensorFIFO + offsetof(struct sensorFIFO, data);
		pEnd = (char *)pStart + obj->SCP_sensorFIFO->FIFOSize;
		rp = pStart + (int)obj->SCP_sensorFIFO->rp;
		wp = pStart + (int)obj->SCP_sensorFIFO->wp;

		if (SCP_TRC_BATCH & atomic_read(&(obj_data->trace))) {
			SCP_ERR("FIFO pStart = %p, rp = %p, wp = %p, pEnd = %p\n", pStart, rp, wp,
				pEnd);
		}

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
			pNext =
			    rp + offsetof(struct SCP_sensorData,
					  data) + ((struct SCP_sensorData *)rp)->dataLength;
			pNext = (char *)((((unsigned long)pNext + 3) >> 2) << 2);
			if (SCP_TRC_BATCH_DETAIL & atomic_read(&(obj_data->trace)))
				SCP_LOG("rp = %p, dataLength = %d, pNext = %p\n", rp,
					((struct SCP_sensorData *)rp)->dataLength, pNext);

			if (((struct SCP_sensorData *)rp)->dataLength != 6
			    && ((struct SCP_sensorData *)rp)->dataLength != 8) {
				SCP_ERR("Wrong dataLength = %d, sensorType = %d\n",
					((struct SCP_sensorData *)rp)->dataLength,
					((struct SCP_sensorData *)rp)->sensorType);
				return -7;
			}

			pt[((struct SCP_sensorData *)rp)->sensorType].total_count++;

			if (pNext < pEnd) {
				rp = pNext;
			} else {
				offset = (int)(pNext - pEnd);
				rp = pStart + offset;
			}
			(*dataLen)++;
		}

		/* No data changed, sync. to device is not necessary. */
		/* dma_sync_single_for_device(&SCP_sensorHub_dev, obj->mapping
		, obj->SCP_sensorFIFO->FIFOSize, DMA_TO_DEVICE); */

	if (SCP_TRC_BATCH & atomic_read(&(obj_data->trace)))
		SCP_ERR("dataLen = %d, status = %d\n", *dataLen, *status);

	return 0;
}

/*----------------------------------------------------------------------------*/
static int SCP_sensorHub_probe(void)
{
	struct SCP_sensorHub_data *obj;
	int err = 0;
	struct batch_control_path ctl = { 0 };
	struct batch_data_path data = { 0 };

	SCP_FUN();

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (obj == NULL) {
		SCP_ERR("Allocate SCP_sensorHub_data fail\n");
		err = -ENOMEM;
		goto exit;
	}

	memset(obj, 0, sizeof(struct SCP_sensorHub_data));

	obj->SCP_sensorFIFO = kzalloc(SCP_SENSOR_HUB_FIFO_SIZE, GFP_KERNEL);
	if (obj->SCP_sensorFIFO == NULL) {
		SCP_ERR("Allocate SCP_sensorFIFO fail\n");
		err = -ENOMEM;
		goto exit;
	}

	obj->SCP_sensorFIFO->wp = 0;
	/* (struct SCP_sensorData *)((char *)obj->SCP_sensorFIFO
		 + offsetof(struct sensorFIFO, data)); */
	obj->SCP_sensorFIFO->rp = 0;
	/* (struct SCP_sensorData *)((char *)obj->SCP_sensorFIFO
		 + offsetof(struct sensorFIFO, data)); */
	obj->SCP_sensorFIFO->FIFOSize =
	    SCP_SENSOR_HUB_FIFO_SIZE - offsetof(struct sensorFIFO, data);
	obj->hw = get_cust_sensorHub_hw();

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
#ifdef CONFIG_CUSTOM_KERNEL_STEP_COUNTER
	INIT_WORK(&obj->sd_work, SCP_sd_work);
	INIT_WORK(&obj->sig_work, SCP_sig_work);
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_STEP_COUNTER */
#ifdef CONFIG_CUSTOM_KERNEL_IN_POCKET_SENSOR
	INIT_WORK(&obj->inpk_work, SCP_inpk_work);
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_IN_POCKET_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_PEDOMETER
/* INIT_WORK(&obj->pdr_work, SCP_pdr_work); */
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_PEDOMETER */
#ifdef CONFIG_CUSTOM_KERNEL_ACTIVITY_SENSOR
/* INIT_WORK(&obj->act_work, SCP_act_work); */
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_ACTIVITY_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_SHAKE_SENSOR
	INIT_WORK(&obj->shk_work, SCP_shk_work);
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_SHAKE_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_PICK_UP_SENSOR
	INIT_WORK(&obj->pkup_work, SCP_pkup_work);
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_PICK_UP_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_FACE_DOWN_SENSOR
	INIT_WORK(&obj->fdn_work, SCP_fdn_work);
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_FACE_DOWN_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_HEART_RATE_SENSOR
/* INIT_WORK(&obj->hrm_work, SCP_hrm_work); */
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_HEART_RATE_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_TILT_DETECTOR_SENSOR
	INIT_WORK(&obj->tilt_work, SCP_tilt_work);
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_TILT_DETECTOR_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_WAKE_GESTURE_SENSOR
	INIT_WORK(&obj->wag_work, SCP_wag_work);
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_WAKE_GESTURE_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_GLANCE_GESTURE_SENSOR
	INIT_WORK(&obj->glg_work, SCP_glg_work);
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_GLANCE_GESTURE_SENSOR */


	init_waitqueue_head(&SCP_sensorHub_req_wq);
	init_timer(&obj->timer);
	obj->timer.expires = 3 * HZ;
	obj->timer.function = SCP_sensorHub_req_send_timeout;
	obj->timer.data = (unsigned long)obj;

#ifdef CONFIG_CUSTOM_KERNEL_STEP_COUNTER
	init_timer(&obj->notify_timer);
	obj->notify_timer.expires = HZ / 5;	/* 200 ms */
	obj->notify_timer.function = notify_ap_timeout;
	obj->notify_timer.data = (unsigned long)obj;
#endif				/*#ifdef CONFIG_CUSTOM_KERNEL_STEP_COUNTER */

	md32_ipi_registration(IPI_SENSOR, SCP_sensorHub_IPI_handler, "SCP_sensorHub");

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
	err = batch_register_control_path(MAX_ANDROID_SENSOR_NUM, &ctl);
	if (err) {
		SCP_ERR("register SCP sensor hub control path err\n");
		goto exit_kfree;
	}

	data.get_data = SCP_sensorHub_get_data;
	data.get_fifo_status = SCP_sensorHub_get_fifo_status;
	data.is_batch_supported = 1;
	err = batch_register_data_path(MAX_ANDROID_SENSOR_NUM, &data);
	if (err) {
		SCP_ERR("register SCP sensor hub control data path err\n");
		goto exit_kfree;
	}

	SCP_sensorHub_init_flag = 0;
	pr_debug("%s: OK new\n", __func__);

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

/*----------------------------------------------------------------------------*/
static int SCP_sensorHub_remove(void)
{
	struct sensorHub_hw *hw = get_cust_sensorHub_hw();
	int err = 0;

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();
	SCP_sensorHub_power(hw, 0);

	err = SCP_sensorHub_delete_attr(&(SCP_sensorHub_init_info.platform_diver_addr->driver));
	if (err)
		SCP_ERR("SCP_sensorHub_delete_attr fail: %d\n", err);

	err = misc_deregister(&SCP_sensorHub_device);
	if (err)
		SCP_ERR("misc_deregister fail: %d\n", err);

	return 0;
}

/*----------------------------------------------------------------------------*/
static int SCP_sensor_enable(int sensorType, int en)
{
	SCP_SENSOR_HUB_DATA req;
	int len;
	int err = 0;

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	req.activate_req.sensorType = sensorType;
	req.activate_req.action = SENSOR_HUB_ACTIVATE;
	req.activate_req.enable = en;
	len = sizeof(req.activate_req);
	err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err)
		SCP_ERR("SCP_sensorHub_req_send fail!\n");

	return err;
}

static int SCP_sensor_set_delay(int sensorType, int delay)
{
	SCP_SENSOR_HUB_DATA req;
	int len;
	int err = 0;

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	req.set_delay_req.sensorType = sensorType;
	req.set_delay_req.action = SENSOR_HUB_SET_DELAY;
	req.set_delay_req.delay = delay;
	len = sizeof(req.set_delay_req);
	err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err)
		SCP_ERR("SCP_sensorHub_req_send fail!\n");

	return err;
}

/*----------------------------------------------------------------------------*/
static int SCP_sensor_get_data16(int sensorType, void *value, int *status)
{
	SCP_SENSOR_HUB_DATA req;
	int len;
	int err = 0;

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	req.get_data_req.sensorType = sensorType;
	req.get_data_req.action = SENSOR_HUB_GET_DATA;
	len = sizeof(req.get_data_req);
	err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err)
		SCP_ERR("SCP_sensorHub_req_send fail!\n");

	switch (sensorType) {
	case ID_ACTIVITY:	/* there are 6 values in activity */
		*(u16 *) value = *req.get_data_rsp.int16_Data;
		*((u16 *) value + 1) = *(req.get_data_rsp.int16_Data + 1);
		*((u16 *) value + 2) = *(req.get_data_rsp.int16_Data + 2);
		*((u16 *) value + 3) = *(req.get_data_rsp.int16_Data + 3);
		*((u16 *) value + 4) = *(req.get_data_rsp.int16_Data + 4);
		*((u16 *) value + 5) = *(req.get_data_rsp.int16_Data + 5);
		SCP_LOG
		    ("ID_ACTIVITY , value=%d value1=%d value2=%d value3=%d value4=%d value5=%d\n",
		     *((u16 *) value), *((u16 *) value + 1), *((u16 *) value + 2),
		     *((u16 *) value + 3), *((u16 *) value + 4), *((u16 *) value + 5));
		break;
	case ID_IN_POCKET:
		*((u16 *) value) = *(req.get_data_rsp.int16_Data);
		break;
	case ID_PICK_UP_GESTURE:
		*((u16 *) value) = *(req.get_data_rsp.int16_Data);
		break;
	case ID_FACE_DOWN:
		*((u16 *) value) = *(req.get_data_rsp.int16_Data);
		break;
	case ID_SHAKE:
		*((u16 *) value) = *(req.get_data_rsp.int16_Data);
		break;
	case ID_TILT_DETECTOR:
		*((u16 *) value) = *(req.get_data_rsp.int16_Data);
		break;
	case ID_WAKE_GESTURE:
		*((u16 *) value) = *(req.get_data_rsp.int16_Data);
		break;
	case ID_GLANCE_GESTURE:
		*((u16 *) value) = *(req.get_data_rsp.int16_Data);
		break;
	default:
		err = -1;
		break;
	}
	SCP_LOG("sensorType = %d, value = %d\n", sensorType, *((u16 *) value));
	return err;
}

static int SCP_sensor_get_data32(int sensorType, void *value, int *status)
{
	SCP_SENSOR_HUB_DATA req;
	int len;
	int err = 0;

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	req.get_data_req.sensorType = sensorType;
	req.get_data_req.action = SENSOR_HUB_GET_DATA;
	len = sizeof(req.get_data_req);
	err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err)
		SCP_ERR("SCP_sensorHub_req_send fail!\n");

	switch (sensorType) {
	case ID_STEP_COUNTER:
		*((u32 *) value) = *(req.get_data_rsp.int32_Data);
		break;
	case ID_STEP_DETECTOR:
		*((u32 *) value) = *(req.get_data_rsp.int32_Data);
		break;
	case ID_SIGNIFICANT_MOTION:
		*((u32 *) value) = *(req.get_data_rsp.int32_Data);
		break;
	case ID_PEDOMETER:	/* there are 4 values in pedometer */
		*(u32 *) value = *req.get_data_rsp.int32_Data;
		*((u32 *) value + 1) = *(req.get_data_rsp.int32_Data + 1);
		*((u32 *) value + 2) = *(req.get_data_rsp.int32_Data + 2);
		*((u32 *) value + 3) = *(req.get_data_rsp.int32_Data + 3);
		SCP_LOG("ID_PEDOMETER, value=%d value1=%d value2=%d value3=%d\n",
			*((u32 *) value), *((u32 *) value + 1), *((u32 *) value + 2),
			*((u32 *) value + 3));
		break;
	case ID_HEART_RATE:	/* there are 4 values in pedometer */
		*(u32 *) value = *req.get_data_rsp.int32_Data;
		*((u32 *) value + 1) = *(req.get_data_rsp.int32_Data + 1);
		SCP_LOG("ID_HEART_RATE, value=%d value1=%d\n",
			*((u32 *) value), *((u32 *) value + 1));
		break;
	default:
		err = -1;
		break;
	}
	SCP_LOG("sensorType = %d, value = %d\n", sensorType, *((u32 *) value));
	return err;
}

static int SCP_sensor_get_data(int sensorType, void *value, int *status)
{
	SCP_SENSOR_HUB_DATA req;
	int len;
	int err = 0;

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	req.get_data_req.sensorType = sensorType;
	req.get_data_req.action = SENSOR_HUB_GET_DATA;
	len = sizeof(req.get_data_req);
	err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err)
		SCP_ERR("SCP_sensorHub_req_send fail!\n");

	switch (sensorType) {
	case ID_STEP_COUNTER:
		*((u64 *) value) = *(req.get_data_rsp.int32_Data);
		break;
	case ID_STEP_DETECTOR:
		*((u64 *) value) = *(req.get_data_rsp.int32_Data);
		break;
	case ID_SIGNIFICANT_MOTION:
		*((u64 *) value) = *(req.get_data_rsp.int32_Data);
		break;
	case ID_HEART_RATE:	/* there are 2 values in heart rate */
		*(u64 *) value = *req.get_data_rsp.int32_Data;
		*((u64 *) value + 1) = *(req.get_data_rsp.int32_Data + 1);
		SCP_LOG("ID_PEDOMETER, value=%lld value1=%lld\n",
			*((u64 *) value), *((u64 *) value + 1));
		break;
	case ID_PEDOMETER:	/* there are 4 values in pedometer */
		*(u64 *) value = *req.get_data_rsp.int32_Data;
		*((u64 *) value + 1) = *(req.get_data_rsp.int32_Data + 1);
		*((u64 *) value + 2) = *(req.get_data_rsp.int32_Data + 2);
		*((u64 *) value + 3) = *(req.get_data_rsp.int32_Data + 3);
		SCP_LOG("ID_PEDOMETER, value=%lld value1=%lld value2=%lld value3=%lld\n",
			*((u64 *) value), *((u64 *) value + 1), *((u64 *) value + 2),
			*((u64 *) value + 3));
		break;
	case ID_ACTIVITY:	/* there are 6 values in activity */
		*(u64 *) value = *req.get_data_rsp.int16_Data;
		*((u64 *) value + 1) = *(req.get_data_rsp.int16_Data + 1);
		*((u64 *) value + 2) = *(req.get_data_rsp.int16_Data + 2);
		*((u64 *) value + 3) = *(req.get_data_rsp.int16_Data + 3);
		*((u64 *) value + 4) = *(req.get_data_rsp.int16_Data + 4);
		*((u64 *) value + 5) = *(req.get_data_rsp.int16_Data + 5);
		*(u64 *) value &= 0xFFFF;
		*((u64 *) value + 1) &= 0xFFFF;
		*((u64 *) value + 2) &= 0xFFFF;
		*((u64 *) value + 3) &= 0xFFFF;
		*((u64 *) value + 4) &= 0xFFFF;
		*((u64 *) value + 5) &= 0xFFFF;
		SCP_LOG("ID_ACTIVITY 16, Data=%d Data1=%d Data2=%d Data3=%d Data4=%d Data5=%d\n",
			*req.get_data_rsp.int16_Data, *(req.get_data_rsp.int16_Data + 1),
			*(req.get_data_rsp.int16_Data + 2), *(req.get_data_rsp.int16_Data + 3),
			*(req.get_data_rsp.int16_Data + 4), *(req.get_data_rsp.int16_Data + 5));
		SCP_LOG
		    ("ID_ACTIVITY 64, value=%lld value1=%lld value2=%lld value3=%lld value4=%lld value5=%lld\n",
		     *((u64 *) value), *((u64 *) value + 1), *((u64 *) value + 2),
		     *((u64 *) value + 3), *((u64 *) value + 4), *((u64 *) value + 5));
		break;
	case ID_IN_POCKET:
		*((u64 *) value) = *(req.get_data_rsp.int16_Data);
		break;
	case ID_PICK_UP_GESTURE:
		*((u64 *) value) = *(req.get_data_rsp.int16_Data);
		break;
	case ID_FACE_DOWN:
		*((u64 *) value) = *(req.get_data_rsp.int16_Data);
		break;
	case ID_SHAKE:
		*((u64 *) value) = *(req.get_data_rsp.int16_Data);
		break;
	case ID_TILT_DETECTOR:
		*((u64 *) value) = *(req.get_data_rsp.int16_Data);
		break;
	case ID_WAKE_GESTURE:
		*((u64 *) value) = *(req.get_data_rsp.int16_Data);
		break;
	case ID_GLANCE_GESTURE:
		*((u64 *) value) = *(req.get_data_rsp.int16_Data);
		break;
	default:
		err = -1;
		break;
	}

	SCP_LOG("sensorType = %d, value = %lld\n", sensorType, *((u64 *) value));

	return err;
}

static int SCP_sensorHub_notify_handler(void *data, uint len)
{
	SCP_SENSOR_HUB_DATA_P rsp = (SCP_SENSOR_HUB_DATA_P) data;

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	if (SCP_TRC_IPI == atomic_read(&(obj_data->trace)))
		SCP_LOG("len = %d, type = %d, action = %d, errCode = %d\n", len,
			rsp->rsp.sensorType, rsp->rsp.action, rsp->rsp.errCode);

	if (!obj_data)
		return -1;

	switch (rsp->rsp.action) {
	case SENSOR_HUB_NOTIFY:
		SCP_LOG("SENSOR_HUB_NOTIFY sensorId = %d\n", rsp->notify_rsp.sensorType);
		switch (rsp->notify_rsp.event) {
		case SCP_NOTIFY:
			if (ID_STEP_DETECTOR == rsp->notify_rsp.sensorType)
				schedule_work(&(obj_data->sd_work));
#ifdef CONFIG_CUSTOM_KERNEL_STEP_COUNTER
			else if (ID_SIGNIFICANT_MOTION == rsp->notify_rsp.sensorType) {
				wake_lock(&sig_lock);
				schedule_work(&(obj_data->sig_work));
			}
#endif				/*#ifdef CONFIG_CUSTOM_KERNEL_STEP_COUNTER */
			else if (ID_IN_POCKET == rsp->notify_rsp.sensorType) {
				schedule_work(&(obj_data->inpk_work));
			} else if (ID_PICK_UP_GESTURE == rsp->notify_rsp.sensorType) {
				schedule_work(&(obj_data->pkup_work));
			} else if (ID_FACE_DOWN == rsp->notify_rsp.sensorType) {
				schedule_work(&(obj_data->fdn_work));
			} else if (ID_SHAKE == rsp->notify_rsp.sensorType) {
				schedule_work(&(obj_data->shk_work));
			} else if (ID_TILT_DETECTOR == rsp->notify_rsp.sensorType) {
				schedule_work(&(obj_data->tilt_work));
			} else if (ID_WAKE_GESTURE == rsp->notify_rsp.sensorType) {
				schedule_work(&(obj_data->wag_work));
			} else if (ID_GLANCE_GESTURE == rsp->notify_rsp.sensorType) {
				schedule_work(&(obj_data->glg_work));
			} else {
				SCP_ERR("Unknown notify");
			}
			break;
		default:
			SCP_ERR("Error sensor hub notify");
			break;
		}
		break;
	default:
		SCP_ERR("Error sensor hub action");
		break;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
#ifdef CONFIG_CUSTOM_KERNEL_HEART_RATE_SENSOR
/* static void SCP_hrm_work(struct work_struct *work) */
/* { */
/* if (SCP_TRC_FUN == atomic_read(&(obj_data->trace))) */
/* SCP_FUN(); */
/*  */
/* hrm_notify(); */
/* } */
/* static int hrm_enable(int en) */
/* { */
/* if (SCP_TRC_FUN == atomic_read(&(obj_data->trace))) */
/* SCP_FUN(); */
/*  */
/* return SCP_sensor_enable(ID_HEART_RATE, en); */
/* } */
static int hrm_get_data(u32 *value, int *status)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_get_data32(ID_HEART_RATE, value, status);
}

static int hrm_open_report_data(int open)	/* open data rerport to HAL */
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return 0;
}

/*----------------------------------------------------------------------------*/
static int hrm_enable_nodata(int en)	/* only enable not report event to HAL */
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_enable(ID_HEART_RATE, en);
}

static int hrm_set_delay(u64 delay)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_set_delay(ID_HEART_RATE, delay);
}

static int SCP_sensorHub_heart_rate_init(void)
{
	struct hrm_control_path ctl = { 0 };
	struct hrm_data_path data = { 0 };
	int err = 0;

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	ctl.open_report_data = hrm_open_report_data;
	ctl.enable_nodata = hrm_enable_nodata;
	ctl.set_delay = hrm_set_delay;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = true;
	err = hrm_register_control_path(&ctl);
	if (err) {
		pr_debug("register heart_rate control path err\n");
		return -1;
	}

	data.get_data = hrm_get_data;
	/* data.vender_div = 1; */
	err = hrm_register_data_path(&data);
	if (err) {
		pr_debug("register heart_rate data path err\n");
		return -1;
	}
	return 0;
}

static int SCP_sensorHub_heart_rate_uninit(void)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return 0;
}
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_HEART_RATE_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_PEDOMETER
/* static void SCP_hrm_work(struct work_struct *work) */
/* { */
/* if (SCP_TRC_FUN == atomic_read(&(obj_data->trace))) */
/* SCP_FUN(); */
/*  */
/* pdr_notify(); */
/* } */
/* static int pdr_enable(int en) */
/* { */
/* if (SCP_TRC_FUN == atomic_read(&(obj_data->trace))) */
/* SCP_FUN(); */
/*  */
/* return SCP_sensor_enable(ID_PEDOMETER, en); */
/* } */
static int pdr_get_data(u32 *value, int *status)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_get_data32(ID_PEDOMETER, value, status);
}

static int pdr_open_report_data(int open)	/* open data rerport to HAL */
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return 0;
}

/*----------------------------------------------------------------------------*/
static int pdr_enable_nodata(int en)	/* only enable not report event to HAL */
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_enable(ID_PEDOMETER, en);
}

static int pdr_set_delay(u64 delay)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_set_delay(ID_PEDOMETER, delay);
}

static int SCP_sensorHub_pedometer_init(void)
{
	struct pdr_control_path ctl = { 0 };
	struct pdr_data_path data = { 0 };
	int err = 0;

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	ctl.open_report_data = pdr_open_report_data;
	ctl.enable_nodata = pdr_enable_nodata;
	ctl.set_delay = pdr_set_delay;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = true;
	err = pdr_register_control_path(&ctl);
	if (err) {
		pr_debug("register pedometer control path err\n");
		return -1;
	}

	data.get_data = pdr_get_data;
	/* data.vender_div = 1; */
	err = pdr_register_data_path(&data);
	if (err) {
		pr_debug("register pedometer data path err\n");
		return -1;
	}
	return 0;
}

static int SCP_sensorHub_pedometer_uninit(void)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return 0;
}
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_PEDOMETER */
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_CUSTOM_KERNEL_ACTIVITY_SENSOR
/* static void SCP_act_work(struct work_struct *work) */
/* { */
/* if (SCP_TRC_FUN == atomic_read(&(obj_data->trace))) */
/* SCP_FUN(); */
/*  */
/* act_notify(); */
/* } */
/* static int act_enable(int en) */
/* { */
/* if (SCP_TRC_FUN == atomic_read(&(obj_data->trace))) */
/* SCP_FUN(); */
/*  */
/* return SCP_sensor_enable(ID_ACTIVITY, en); */
/* } */
static int act_get_data(u16 *value, int *status)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_get_data16(ID_ACTIVITY, value, status);
}

static int act_open_report_data(int open)	/* open data rerport to HAL */
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return 0;
}

/*----------------------------------------------------------------------------*/
static int act_enable_nodata(int en)	/* only enable not report event to HAL */
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_enable(ID_ACTIVITY, en);
}

static int act_set_delay(u64 delay)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_set_delay(ID_ACTIVITY, delay);
}

static int SCP_sensorHub_activity_init(void)
{
	struct act_control_path ctl = { 0 };
	struct act_data_path data = { 0 };
	int err = 0;

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	ctl.open_report_data = act_open_report_data;
	ctl.enable_nodata = act_enable_nodata;
	ctl.set_delay = act_set_delay;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = true;
	err = act_register_control_path(&ctl);
	if (err) {
		pr_debug("register pedometer control path err\n");
		return -1;
	}

	data.get_data = act_get_data;
	/* data.vender_div = 1; */
	err = act_register_data_path(&data);
	if (err) {
		pr_debug("register pedometer data path err\n");
		return -1;
	}
	return 0;
}

static int SCP_sensorHub_activity_uninit(void)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return 0;
}
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_ACTIVITY_SENSOR */
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_CUSTOM_KERNEL_IN_POCKET_SENSOR
static void SCP_inpk_work(struct work_struct *work)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	inpk_notify();
}

static int inpk_open_report_data(int open)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_enable(ID_IN_POCKET, open);
}

static int inpk_get_data(u16 *value, int *status)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_get_data16(ID_IN_POCKET, value, status);
}

static int SCP_sensorHub_in_pocket_init(void)
{
	struct inpk_control_path ctl = { 0 };
	struct inpk_data_path data = { 0 };
	int err = 0;

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	ctl.open_report_data = inpk_open_report_data;
	err = inpk_register_control_path(&ctl);
	if (err) {
		pr_debug("register in pocket control path err\n");
		return -1;
	}
	data.get_data = inpk_get_data;
	err = inpk_register_data_path(&data);
	if (err) {
		pr_debug("register in pocket data path err\n");
		return -1;
	}
	SCP_sensorHub_rsp_registration(ID_IN_POCKET, SCP_sensorHub_notify_handler);
	return 0;
}

static int SCP_sensorHub_in_pocket_uninit(void)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return 0;
}
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_IN_POCKET_SENSOR */
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_CUSTOM_KERNEL_SHAKE_SENSOR
static void SCP_shk_work(struct work_struct *work)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	shk_notify();
}

static int shk_open_report_data(int open)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_enable(ID_SHAKE, open);
}

static int shk_get_data(u16 *value, int *status)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_get_data16(ID_SHAKE, value, status);
}

static int SCP_sensorHub_shake_init(void)
{
	struct shk_control_path ctl = { 0 };
	struct shk_data_path data = { 0 };
	int err = 0;

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	ctl.open_report_data = shk_open_report_data;
	err = shk_register_control_path(&ctl);
	if (err) {
		pr_debug("register shake control path err\n");
		return -1;
	}
	data.get_data = shk_get_data;
	err = shk_register_data_path(&data);
	if (err) {
		pr_debug("register shake data path err\n");
		return -1;
	}
	SCP_sensorHub_rsp_registration(ID_SHAKE, SCP_sensorHub_notify_handler);
	return 0;
}

static int SCP_sensorHub_shake_uninit(void)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return 0;
}
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_SHAKE_SENSOR */
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_CUSTOM_KERNEL_PICK_UP_SENSOR
static void SCP_pkup_work(struct work_struct *work)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	pkup_notify();
}

static int pkup_open_report_data(int open)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_enable(ID_PICK_UP_GESTURE, open);
}

static int pkup_get_data(u16 *value, int *status)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_get_data16(ID_PICK_UP_GESTURE, value, status);
}

static int SCP_sensorHub_pick_up_init(void)
{
	struct pkup_control_path ctl = { 0 };
	struct pkup_data_path data = { 0 };
	int err = 0;

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	ctl.open_report_data = pkup_open_report_data;
	err = pkup_register_control_path(&ctl);
	if (err) {
		pr_debug("register pick up control path err\n");
		return -1;
	}
	data.get_data = pkup_get_data;
	err = pkup_register_data_path(&data);
	if (err) {
		pr_debug("register pick up data path err\n");
		return -1;
	}
	SCP_sensorHub_rsp_registration(ID_PICK_UP_GESTURE, SCP_sensorHub_notify_handler);
	return 0;
}

static int SCP_sensorHub_pick_up_uninit(void)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return 0;
}
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_PICK_UP_SENSOR */
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_CUSTOM_KERNEL_FACE_DOWN_SENSOR
static void SCP_fdn_work(struct work_struct *work)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	fdn_notify();
}

static int fdn_open_report_data(int open)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_enable(ID_FACE_DOWN, open);
}

static int fdn_get_data(u16 *value, int *status)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_get_data16(ID_FACE_DOWN, value, status);
}

static int SCP_sensorHub_face_down_init(void)
{
	struct fdn_control_path ctl = { 0 };
	struct fdn_data_path data = { 0 };
	int err = 0;

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	ctl.open_report_data = fdn_open_report_data;
	err = fdn_register_control_path(&ctl);
	if (err) {
		pr_debug("register face down control path err\n");
		return -1;
	}
	data.get_data = fdn_get_data;
	err = fdn_register_data_path(&data);
	if (err) {
		pr_debug("register face down data path err\n");
		return -1;
	}
	SCP_sensorHub_rsp_registration(ID_FACE_DOWN, SCP_sensorHub_notify_handler);
	return 0;
}

static int SCP_sensorHub_face_down_uninit(void)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return 0;
}
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_FACE_DOWN_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_TILT_DETECTOR_SENSOR
static void SCP_tilt_work(struct work_struct *work)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	tilt_notify();
}

static int tilt_open_report_data(int open)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_enable(ID_TILT_DETECTOR, open);
}

static int tilt_get_data(u16 *value, int *status)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_get_data16(ID_TILT_DETECTOR, value, status);
}

static int SCP_sensorHub_tilt_detector_init(void)
{
	struct tilt_control_path ctl = { 0 };
	struct tilt_data_path data = { 0 };
	int err = 0;

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	ctl.open_report_data = tilt_open_report_data;
	err = tilt_register_control_path(&ctl);
	if (err) {
		pr_debug("register tilt_detector control path err\n");
		return -1;
	}
	data.get_data = tilt_get_data;
	err = tilt_register_data_path(&data);
	if (err) {
		pr_debug("register tilt_detector data path err\n");
		return -1;
	}
	SCP_sensorHub_rsp_registration(ID_TILT_DETECTOR, SCP_sensorHub_notify_handler);
	return 0;
}

static int SCP_sensorHub_tilt_detector_uninit(void)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return 0;
}
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_TILT_DETECTOR_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_WAKE_GESTURE_SENSOR
static void SCP_wag_work(struct work_struct *work)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	wag_notify();
}

static int wag_open_report_data(int open)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_enable(ID_WAKE_GESTURE, open);
}

static int wag_get_data(u16 *value, int *status)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_get_data16(ID_WAKE_GESTURE, value, status);
}

static int SCP_sensorHub_wake_gesture_init(void)
{
	struct wag_control_path ctl = { 0 };
	struct wag_data_path data = { 0 };
	int err = 0;

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	ctl.open_report_data = wag_open_report_data;
	err = wag_register_control_path(&ctl);
	if (err) {
		pr_debug("register wake_gesture control path err\n");
		return -1;
	}
	data.get_data = wag_get_data;
	err = wag_register_data_path(&data);
	if (err) {
		pr_debug("register wake_gesture data path err\n");
		return -1;
	}
	SCP_sensorHub_rsp_registration(ID_WAKE_GESTURE, SCP_sensorHub_notify_handler);
	return 0;
}

static int SCP_sensorHub_wake_gesture_uninit(void)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return 0;
}
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_WAKE_GESTURE_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_GLANCE_GESTURE_SENSOR
static void SCP_glg_work(struct work_struct *work)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	glg_notify();
}

static int glg_open_report_data(int open)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_enable(ID_GLANCE_GESTURE, open);
}

static int glg_get_data(u16 *value, int *status)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_get_data16(ID_GLANCE_GESTURE, value, status);
}

static int SCP_sensorHub_glance_gesture_init(void)
{
	struct glg_control_path ctl = { 0 };
	struct glg_data_path data = { 0 };
	int err = 0;

	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	ctl.open_report_data = glg_open_report_data;
	err = glg_register_control_path(&ctl);
	if (err) {
		pr_debug("register glance_gesture control path err\n");
		return -1;
	}
	data.get_data = glg_get_data;
	err = glg_register_data_path(&data);
	if (err) {
		pr_debug("register glance_gesture data path err\n");
		return -1;
	}
	SCP_sensorHub_rsp_registration(ID_GLANCE_GESTURE, SCP_sensorHub_notify_handler);
	return 0;
}

static int SCP_sensorHub_glance_gesture_uninit(void)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return 0;
}
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_GLANCE_GESTURE_SENSOR */

#ifdef CONFIG_CUSTOM_KERNEL_STEP_COUNTER
static void SCP_sd_work(struct work_struct *work)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	step_notify(TYPE_STEP_DETECTOR);
}

/*----------------------------------------------------------------------------*/
static void SCP_sig_work(struct work_struct *work)
{
	if (SCP_TRC_FUN == atomic_read(&(obj_data->trace)))
		SCP_FUN();

	mod_timer(&obj_data->notify_timer, jiffies + HZ / 5);
	step_notify(TYPE_SIGNIFICANT);
}

static void notify_ap_timeout(unsigned long data)
{
	wake_unlock(&sig_lock);
}

/*----------------------------------------------------------------------------*/
static int step_counter_open_report_data(int open)	/* open data rerport to HAL */
{
	if (SCP_TRC_FUN & atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return 0;
}

/*----------------------------------------------------------------------------*/
static int step_counter_enable_nodata(int en)	/* only enable not report event to HAL */
{
	if (SCP_TRC_FUN & atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_enable(ID_STEP_COUNTER, en);
}

/*----------------------------------------------------------------------------*/
static int step_detect_enable(int en)
{
	if (SCP_TRC_FUN & atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_enable(ID_STEP_DETECTOR, en);
}

/*----------------------------------------------------------------------------*/
static int significant_motion_enable(int en)
{
	if (SCP_TRC_FUN & atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_enable(ID_SIGNIFICANT_MOTION, en);
}

/*----------------------------------------------------------------------------*/
static int step_counter_set_delay(u64 delay)
{
	if (SCP_TRC_FUN & atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return 0;
}

/*----------------------------------------------------------------------------*/
static int step_counter_get_data(u32 *value, int *status)
{
	if (SCP_TRC_FUN & atomic_read(&(obj_data->trace)))
		SCP_FUN();

	*status = 3;
	return SCP_sensor_get_data32(ID_STEP_COUNTER, value, status);
}

/*----------------------------------------------------------------------------*/
static int step_detect_get_data(u32 *value, int *status)
{
	if (SCP_TRC_FUN & atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_get_data32(ID_STEP_DETECTOR, value, status);
}

/*----------------------------------------------------------------------------*/
static int significant_motion_get_data(u32 *value, int *status)
{
	if (SCP_TRC_FUN & atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return SCP_sensor_get_data32(ID_SIGNIFICANT_MOTION, value, status);
}

/*----------------------------------------------------------------------------*/
static int SCP_sensorHub_step_counter_init(void)
{
	struct step_c_control_path ctl = { 0 };
	struct step_c_data_path data = { 0 };
	int err = 0;

	if (SCP_TRC_FUN & atomic_read(&(obj_data->trace)))
		SCP_FUN();

	/* register step */
	ctl.open_report_data = step_counter_open_report_data;
	ctl.enable_nodata = step_counter_enable_nodata;
	ctl.set_delay = step_counter_set_delay;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = true;

	ctl.enable_significant = significant_motion_enable;
	ctl.enable_step_detect = step_detect_enable;

	err = step_c_register_control_path(&ctl);
	if (err) {
		pr_debug("register step_counter control path err\n");
		return -1;

	}

	data.get_data = step_counter_get_data;
	data.get_data_significant = significant_motion_get_data;
	data.get_data_step_d = step_detect_get_data;
	data.vender_div = 1;
	err = step_c_register_data_path(&data);
	if (err) {
		pr_debug("register step counter data path err\n");
		return -1;
	}

	wake_lock_init(&sig_lock, WAKE_LOCK_SUSPEND, "signficiant wakelock");

	SCP_sensorHub_rsp_registration(ID_SIGNIFICANT_MOTION, SCP_sensorHub_notify_handler);
	SCP_sensorHub_rsp_registration(ID_STEP_DETECTOR, SCP_sensorHub_notify_handler);

	return 0;
}

/*----------------------------------------------------------------------------*/
static int SCP_sensorHub_step_counter_uninit(void)
{
	if (SCP_TRC_FUN & atomic_read(&(obj_data->trace)))
		SCP_FUN();

	return 0;
}
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_STEP_COUNTER */

/*----------------------------------------------------------------------------*/
static int SCP_sensorHub_local_init(void)
{
	SCP_sensorHub_probe();

	if (-1 == SCP_sensorHub_init_flag)
		return -1;

	return 0;
}

/*----------------------------------------------------------------------------*/
static int __init SCP_sensorHub_init(void)
{
	SCP_FUN();
	batch_driver_add(&SCP_sensorHub_init_info);
#ifdef CONFIG_CUSTOM_KERNEL_STEP_COUNTER
	step_c_driver_add(&SCP_step_counter_init_info);
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_STEP_COUNTER */
#ifdef CONFIG_CUSTOM_KERNEL_IN_POCKET_SENSOR
	inpk_driver_add(&SCP_in_pocket_init_info);
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_IN_POCKET_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_PEDOMETER
	pdr_driver_add(&SCP_pedometer_init_info);
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_PEDOMETER */
#ifdef CONFIG_CUSTOM_KERNEL_ACTIVITY_SENSOR
	act_driver_add(&SCP_activity_init_info);
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_ACTIVITY_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_SHAKE_SENSOR
	shk_driver_add(&SCP_shake_init_info);
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_SHAKE_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_PICK_UP_SENSOR
	pkup_driver_add(&SCP_pick_up_init_info);
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_PICK_UP_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_FACE_DOWN_SENSOR
	fdn_driver_add(&SCP_face_down_init_info);
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_FACE_DOWN_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_HEART_RATE_SENSOR
	hrm_driver_add(&SCP_heart_rate_init_info);
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_HEART_RATE_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_TILT_DETECTOR_SENSOR
	tilt_driver_add(&SCP_tilt_detector_init_info);
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_TILT_DETECTOR_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_WAKE_GESTURE_SENSOR
	wag_driver_add(&SCP_wake_gesture_init_info);
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_WAKE_GESTURE_SENSOR */
#ifdef CONFIG_CUSTOM_KERNEL_GLANCE_GESTURE_SENSOR
	glg_driver_add(&SCP_glance_gesture_init_info);
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_GLANCE_GESTURE_SENSOR */

	return 0;
}

/*----------------------------------------------------------------------------*/
static void __exit SCP_sensorHub_exit(void)
{
	SCP_FUN();
}

/*----------------------------------------------------------------------------*/
/* late_initcall(SCP_sensorHub_init); */
module_init(SCP_sensorHub_init);
module_exit(SCP_sensorHub_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SCP sensor hub driver");
MODULE_AUTHOR("andrew.yang@mediatek.com");
