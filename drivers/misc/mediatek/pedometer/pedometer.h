
#ifndef __PEDOMETER_H__
#define __PEDOMETER_H__


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


#define PDR_TAG					"<PEDOMETER> "
#define PDR_FUN(f)				printk(PDR_TAG"%s\n", __func__)
#define PDR_ERR(fmt, args...)	printk(PDR_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define PDR_LOG(fmt, args...)	printk(PDR_TAG fmt, ##args)
#define PDR_VER(fmt, args...)   printk(PDR_TAG"%s: "fmt, __func__, ##args) //((void)0)

#define OP_PDR_DELAY	0X01
#define	OP_PDR_ENABLE	0X02
#define	OP_PDR_GET_DATA	0X04

#define PDR_INVALID_VALUE -1

#define EVENT_TYPE_PDR_LENGTH   		ABS_X
#define EVENT_TYPE_PDR_FREQUENCY		ABS_Y
#define EVENT_TYPE_PDR_COUNT    		ABS_Z
#define EVENT_TYPE_PDR_DISTANCE 		ABS_RX
#define EVENT_TYPE_PDR_STATUS		        ABS_WHEEL


#define PDR_VALUE_MAX (32767)
#define PDR_VALUE_MIN (-32768)
#define PDR_STATUS_MIN (0)
#define PDR_STATUS_MAX (64)
#define PDR_DIV_MAX (32767)
#define PDR_DIV_MIN (1)

#define MAX_CHOOSE_PDR_NUM 5

struct pdr_control_path
{
	int (*open_report_data)(int open);//open data rerport to HAL
	int (*enable_nodata)(int en);//only enable not report event to HAL
	int (*set_delay)(u64 delay);
	bool is_report_input_direct;
	bool is_support_batch;
};

typedef struct {
	uint32_t length; //milli-meter
	uint32_t frequency; //freq*1000
	uint32_t count;
	uint32_t distance;
} pedometer_t;

struct pdr_data_path
{
	int (*get_data)(u32 *value, int *status);
	int vender_div;
};

struct pdr_init_info
{
  	char *name;
	int (*init)(void);
	int (*uninit)(void);
	struct platform_driver* platform_diver_addr;
};

struct pdr_data{
	hwm_sensor_data pdr_data ;
	int data_updata;
	//struct mutex lock;
};

struct pdr_drv_obj {
  void *self;
	int polling;
	int (*pdr_operate)(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout);
};

struct pdr_context {
	struct input_dev   *idev;
	struct miscdevice   mdev;
	struct work_struct  report;
	struct mutex pdr_op_mutex;
	atomic_t            delay; /*polling period for reporting input event*/
	atomic_t            wake;  /*user-space request to wake-up, used with stop*/
	struct timer_list   timer;  /* polling timer */
	atomic_t            trace;

	struct early_suspend    early_drv;
	atomic_t                early_suspend;
	struct pdr_data       drv_data;
	struct pdr_control_path   pdr_ctl;
	struct pdr_data_path   pdr_data;
	bool			is_active_nodata;		// Active, but HAL don't need data sensor. such as orientation need
	bool			is_active_data;		// Active and HAL need data .
	bool is_first_data_after_enable;
	bool is_polling_run;
	bool is_batch_enable;
};

//driver API for internal  
//extern int pdr_enable_nodata(int enable);
//extern int pdr_attach(struct pdr_drv_obj *obj);
//driver API for third party vendor

//for auto detect
extern int pdr_driver_add(struct pdr_init_info* obj) ;
extern int pdr_data_report(hwm_sensor_data data, int status);
extern int pdr_register_control_path(struct pdr_control_path *ctl);
extern int pdr_register_data_path(struct pdr_data_path *data);
#endif
