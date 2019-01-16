
#ifndef __ACTIVITY_H__
#define __ACTIVITY_H__


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


#define ACT_TAG					"<ACTIVITY> "
#define ACT_FUN(f)				printk(ACT_TAG"%s\n", __func__)
#define ACT_ERR(fmt, args...)	printk(ACT_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define ACT_LOG(fmt, args...)	printk(ACT_TAG fmt, ##args)
#define ACT_VER(fmt, args...)   printk(ACT_TAG"%s: "fmt, __func__, ##args) //((void)0)

#define OP_ACT_DELAY	0X01
#define	OP_ACT_ENABLE	0X02
#define	OP_ACT_GET_DATA	0X04

#define ACT_INVALID_VALUE -1

#define EVENT_TYPE_ACT_IN_VEHICLE 		ABS_X
#define EVENT_TYPE_ACT_ON_BICYCLE 		ABS_Y
#define EVENT_TYPE_ACT_ON_FOOT 			ABS_Z
#define EVENT_TYPE_ACT_STILL 			ABS_RX
#define EVENT_TYPE_ACT_UNKNOWN 			ABS_RY
#define EVENT_TYPE_ACT_TILT 			ABS_RZ
#define EVENT_TYPE_ACT_STATUS		     ABS_WHEEL


#define ACT_VALUE_MAX (32767)
#define ACT_VALUE_MIN (-32768)
#define ACT_STATUS_MIN (0)
#define ACT_STATUS_MAX (64)
#define ACT_DIV_MAX (32767)
#define ACT_DIV_MIN (1)

#define MAX_CHOOSE_ACT_NUM 5

struct act_control_path
{
	int (*open_report_data)(int open);//open data rerport to HAL
	int (*enable_nodata)(int en);//only enable not report event to HAL
	int (*set_delay)(u64 delay);
	bool is_report_input_direct;
	bool is_support_batch;
};

typedef struct {
	uint16_t in_vehicle;
	uint16_t on_bicycle;
	uint16_t on_foot;
	uint16_t still;
	uint16_t unknown;
	uint16_t tilt;
} activity_t;

struct act_data_path
{
	int (*get_data)(u16 *value, int *status);
	int vender_div;
};

struct act_init_info
{
  	char *name;
	int (*init)(void);
	int (*uninit)(void);
	struct platform_driver* platform_diver_addr;
};

struct act_data{
	hwm_sensor_data act_data ;
	int data_updata;
	//struct mutex lock;
};

struct act_drv_obj {
  void *self;
	int polling;
	int (*act_operate)(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout);
};

struct act_context {
	struct input_dev   *idev;
	struct miscdevice   mdev;
	struct work_struct  report;
	struct mutex act_op_mutex;
	atomic_t            delay; /*polling period for reporting input event*/
	atomic_t            wake;  /*user-space request to wake-up, used with stop*/
	struct timer_list   timer;  /* polling timer */
	atomic_t            trace;

	struct early_suspend    early_drv;
	atomic_t                early_suspend;
	struct act_data       drv_data;
	struct act_control_path   act_ctl;
	struct act_data_path   act_data;
	bool			is_active_nodata;		// Active, but HAL don't need data sensor. such as orientation need
	bool			is_active_data;		// Active and HAL need data .
	bool is_first_data_after_enable;
	bool is_polling_run;
	bool is_batch_enable;
};

//driver API for internal  
//extern int act_enable_nodata(int enable);
//extern int act_attach(struct act_drv_obj *obj);
//driver API for third party vendor

//for auto detect
extern int act_driver_add(struct act_init_info* obj) ;
extern int act_data_report(hwm_sensor_data data, int status);
extern int act_register_control_path(struct act_control_path *ctl);
extern int act_register_data_path(struct act_data_path *data);
#endif
