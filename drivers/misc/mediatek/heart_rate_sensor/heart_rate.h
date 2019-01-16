
#ifndef __HEART_RATE_H__
#define __HEART_RATE_H__


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
#include <linux/hwmsen_dev.h>


#define HRM_TAG					"<HEART_RATE> "
#define HRM_FUN(f)				printk(HRM_TAG"%s\n", __func__)
#define HRM_ERR(fmt, args...)	printk(HRM_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define HRM_LOG(fmt, args...)	printk(HRM_TAG fmt, ##args)
#define HRM_VER(fmt, args...)   printk(HRM_TAG"%s: "fmt, __func__, ##args) //((void)0)

#define OP_HRM_DELAY	0X01
#define	OP_HRM_ENABLE	0X02
#define	OP_HRM_GET_DATA	0X04

#define HRM_INVALID_VALUE -1

#define EVENT_TYPE_HRM_BPM   		ABS_X
#define EVENT_TYPE_HRM_STATUS		ABS_Y


#define HRM_VALUE_MAX (32767)
#define HRM_VALUE_MIN (-32768)
#define HRM_STATUS_MIN (0)
#define HRM_STATUS_MAX (64)
#define HRM_DIV_MAX (32767)
#define HRM_DIV_MIN (1)

#define MAX_CHOOSE_HRM_NUM 5

struct hrm_control_path
{
	int (*open_report_data)(int open);//open data rerport to HAL
	int (*enable_nodata)(int en);//only enable not report event to HAL
	int (*set_delay)(u64 delay);
	bool is_report_input_direct;
	bool is_support_batch;
};

typedef struct {
	uint32_t bpm; //BPM * 1000
	uint32_t status; //freq*1000
} heart_rate_t;

struct hrm_data_path
{
	int (*get_data)(u32 *value, int *status);
	int vender_div;
};

struct hrm_init_info
{
  	char *name;
	int (*init)(void);
	int (*uninit)(void);
	struct platform_driver* platform_diver_addr;
};

struct hrm_data{
	hwm_sensor_data hrm_data ;
	int data_updata;
	//struct mutex lock;
};

struct hrm_drv_obj {
  void *self;
	int polling;
	int (*hrm_operate)(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout);
};

struct hrm_context {
	struct input_dev   *idev;
	struct miscdevice   mdev;
	struct work_struct  report;
	struct mutex hrm_op_mutex;
	atomic_t            delay; /*polling period for reporting input event*/
	atomic_t            wake;  /*user-space request to wake-up, used with stop*/
	struct timer_list   timer;  /* polling timer */
	atomic_t            trace;

	struct early_suspend    early_drv;
	atomic_t                early_suspend;
	struct hrm_data           drv_data;
	struct hrm_control_path   hrm_ctl;
	struct hrm_data_path      hrm_data;
	bool	 is_active_nodata;		// Active, but HAL don't need data sensor. such as orientation need
	bool	 is_active_data;		// Active and HAL need data .
	bool   is_first_data_after_enable;
	bool   is_polling_run;
	bool   is_batch_enable;
};

//driver API for internal  
//extern int hrm_enable_nodata(int enable);
//extern int hrm_attach(struct hrm_drv_obj *obj);
//driver API for third party vendor

//for auto detect
extern int hrm_driver_add(struct hrm_init_info* obj) ;
extern int hrm_data_report(hwm_sensor_data data, int status);
extern int hrm_register_control_path(struct hrm_control_path *ctl);
extern int hrm_register_data_path(struct hrm_data_path *data);
#endif
