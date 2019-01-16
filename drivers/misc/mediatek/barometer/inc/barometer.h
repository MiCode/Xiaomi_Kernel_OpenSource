
#ifndef __BARO_H__
#define __BARO_H__


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
#include <barometer_factory.h>

#define BARO_TAG					"<BAROMETER> "
#define BARO_FUN(f)				printk(BARO_TAG"%s\n", __func__)
#define BARO_ERR(fmt, args...)	printk(BARO_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define BARO_LOG(fmt, args...)	printk(BARO_TAG fmt, ##args)
#define BARO_VER(fmt, args...)   	printk(BARO_TAG"%s: "fmt, __func__, ##args) //((void)0)

#define   	OP_BARO_DELAY		0X01
#define	OP_BARO_ENABLE		0X02
#define	OP_BARO_GET_DATA	0X04

#define BARO_INVALID_VALUE -1

#define EVENT_TYPE_BARO_VALUE          			REL_X
#define EVENT_TYPE_BARO_STATUS     				ABS_WHEEL


#define BARO_VALUE_MAX (32767)
#define BARO_VALUE_MIN (-32768)
#define BARO_STATUS_MIN (0)
#define BARO_STATUS_MAX (64)
#define BARO_DIV_MAX (32767)
#define BARO_DIV_MIN (1)


#define MAX_CHOOSE_BARO_NUM 5

struct baro_control_path
{
	int (*open_report_data)(int open);//open data rerport to HAL
	int (*enable_nodata)(int en);//only enable not report event to HAL
	int (*set_delay)(u64 delay);
	int (*baroess_data_fifo)(void);//version2.used for flush operate
	bool is_report_input_direct;
	bool is_support_batch;//version2.used for batch mode support flag
	bool is_use_common_factory;
};

struct baro_data_path
{
	int (*get_data)(int *value, int *status);
	int (*get_raw_data)(int type, int *value);
	int vender_div;
};

struct baro_init_info
{
    	char *name;
	int (*init)(void);
	int (*uninit)(void);
	struct platform_driver* platform_diver_addr;
};

struct baro_data{
	hwm_sensor_data baro_data ;
	int data_updata;
	//struct mutex lock;
};

struct baro_drv_obj {
    void *self;
	int polling;
	int (*baro_operate)(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout);
};

struct baro_context {
	struct input_dev   *idev;
	struct miscdevice   mdev;
	struct work_struct  report;
	struct mutex baro_op_mutex;
	atomic_t            delay; /*polling period for reporting input event*/
	atomic_t            wake;  /*user-space request to wake-up, used with stop*/
	struct timer_list   timer;  /* polling timer */
	atomic_t            trace;

	struct early_suspend    early_drv;
	atomic_t                early_suspend;

	struct baro_data       drv_data;
	struct baro_control_path   baro_ctl;
	struct baro_data_path   baro_data;
	bool			is_active_nodata;		// Active, but HAL don't need data sensor. such as orientation need
	bool			is_active_data;		// Active and HAL need data .
	bool 		is_first_data_after_enable;
	bool 		is_polling_run;
	bool 		is_batch_enable;	//version2.this is used for judging whether sensor is in batch mode
};

//for auto detect
extern int baro_driver_add(struct baro_init_info* obj) ;
extern int baro_data_report(struct input_dev *dev, int value,int status);
extern int baro_register_control_path(struct baro_control_path *ctl);
extern int baro_register_data_path(struct baro_data_path *data);

#endif
