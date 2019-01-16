
#ifndef __BATCH_H__
#define __BATCH_H__


#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/hwmsensor.h>
#include <linux/earlysuspend.h> 
#include <linux/string.h>
#include <linux/hwmsen_dev.h>

#define BATCH_TAG					"<BATCHDEV> "
#define BATCH_FUN(f)				printk(BATCH_TAG"%s\n", __func__)
#define BATCH_ERR(fmt, args...)	printk(KERN_ERR BATCH_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define BATCH_LOG(fmt, args...)	printk(BATCH_TAG fmt, ##args)
#define BATCH_VER(fmt, args...)   printk(BATCH_TAG"%s: "fmt, __func__, ##args) //((void)0)

#define OP_BATCH_DELAY		0X01
#define OP_BATCH_ENABLE		0X02
#define OP_BATCH_GET_DATA	0X04

#define BATCH_INVALID_VALUE -1

#define EVENT_TYPE_BATCH_X          			ABS_X
#define EVENT_TYPE_BATCH_Y          			ABS_Y
#define EVENT_TYPE_BATCH_Z          			ABS_Z
#define EVENT_TYPE_BATCH_STATUS     			ABS_WHEEL
#define EVENT_TYPE_SENSORTYPE							REL_RZ
#define EVENT_TYPE_BATCH_VALUE          	ABS_RX
#define EVENT_TYPE_END_FLAG         			REL_RY
#define EVENT_TYPE_TIMESTAMP_HI    			REL_HWHEEL
#define EVENT_TYPE_TIMESTAMP_LO    			REL_DIAL
#define EVENT_TYPE_BATCH_READY                    REL_X

#define BATCH_VALUE_MAX (32767)
#define BATCH_VALUE_MIN (-32768)
#define BATCH_STATUS_MIN (0)
#define BATCH_STATUS_MAX (64)
#define BATCH_TYPE_MIN (0)
#define BATCH_TYPE_MAX (64)
#define BATCH_DIV_MAX (32767)
#define BATCH_DIV_MIN (1)

enum {
    SENSORS_BATCH_DRY_RUN               = 0x00000001,
    SENSORS_BATCH_WAKE_UPON_FIFO_FULL   = 0x00000002
};
#define MAX_CHOOSE_BATCH_NUM 5
struct batch_init_info
{
    char *name;
	int (*init)(void);
	int (*uninit)(void);
    struct platform_driver* platform_diver_addr;
};

struct batch_control_path
{
	int (*enable_hw_batch)(int handle, int enable, int flag, long long samplingPeriodNs,long long maxBatchReportLatencyNs);//let the hardware know that data should be written to fifo
	int (*flush)(int handle);//open data rerport to HAL
};

struct batch_timestamp_info
{
    int64_t start_t;
    int64_t end_t;
    uint32_t total_count;
    uint32_t num;
};

struct batch_data_path
{
	int (*get_data)(int handle, hwm_sensor_data *data);//sensor data is got one by one, return value: 1 stands for data read not finish 0 stands for read data done 
	int (*get_fifo_status)(int *len, int *status, char *reserved, struct batch_timestamp_info *p_batch_timestampe_info);
	int samplingPeriodMs;
	int maxBatchReportLatencyMs;//report latency for every sensor
	int flags;//reserved
	int is_batch_supported;//batch mode supporting status
	int div;
	int is_timestamp_supported;
};

struct batch_dev_list
{
	struct batch_control_path 	ctl_dev[MAX_ANDROID_SENSOR_NUM+1];//ctl_dev[max] is used for sensor HUB driver to control sensor HUB , ctl_dev[1]... are for single sensor batch mode control
	struct batch_data_path 		data_dev[MAX_ANDROID_SENSOR_NUM+1];//data_dev[max] is used for sensor HUB driver to access single fifo sensor data, data_dev[1]... are for single sensor fifo sensor data
};


struct batch_context {
	struct input_dev   *idev;
	struct miscdevice   mdev;
	struct work_struct  report;
	struct mutex 		batch_op_mutex;
	
	atomic_t            delay; /*polling period for reporting input event*/
	atomic_t            wake;  /*user-space request to wake-up, used with stop*/
	struct timer_list   timer;  /* polling timer */
	atomic_t            trace;

	struct early_suspend    early_drv;
	struct wake_lock        read_data_wake_lock;
	atomic_t                early_suspend;

	struct batch_dev_list 	dev_list;

	uint32_t			active_sensor;
	int				batch_result;
	int				flush_result;
	bool 			is_first_data_after_enable;
	bool 			is_polling_run;
	int 			div_flag;
	int				numOfDataLeft;
	int                 force_wake_upon_fifo_full;

    struct batch_timestamp_info timestamp_info[MAX_ANDROID_SENSOR_NUM+1];
};

typedef enum {
	TYPE_NON   = 0,
	TYPE_MOTION  = 1,
	TYPE_GESTURE = 2,
	TYPE_BATCHTIMEOUT   = 3,
	TYPE_BATCHFULL   = 4,
	TYPE_ERROR = 5,
	TYPE_DATAREADY   = 6
} BATCH_NOTIFY_TYPE;

//driver API for third party vendor
extern int  batch_notify(BATCH_NOTIFY_TYPE type);
extern int  batch_driver_add(struct batch_init_info* obj);
extern void report_batch_data(struct input_dev *dev, hwm_sensor_data *data);
extern void report_batch_finish(struct input_dev *dev, int handle);
extern int batch_register_control_path(int handle, struct batch_control_path *ctl);//when you register control path of sensor hub driver, use handle = [MAX_ANDROID_SENSOR_NUM+1]
extern int batch_register_data_path(int handle, struct batch_data_path *data);//when you register control path of sensor hub driver, use handle = [MAX_ANDROID_SENSOR_NUM+1]
extern int batch_register_support_info(int handle, int support, int div, int timestamp_supported);
#endif
