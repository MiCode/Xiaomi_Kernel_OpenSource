
#ifndef __MAG_H__
#define __MAG_H__


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
#include "mag_factory.h"


#define MAG_TAG					"<MAGNETIC> "
#define MAG_FUN(f)				printk(MAG_TAG"%s\n", __func__)
#define MAG_ERR(fmt, args...)	printk(MAG_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define MAG_LOG(fmt, args...)	printk(MAG_TAG fmt, ##args)
#define MAG_VER(fmt, args...)   printk(MAG_TAG"%s: "fmt, __func__, ##args) //((void)0)

#define OP_MAG_DELAY	0X01
#define	OP_MAG_ENABLE	0X02
#define	OP_MAG_GET_DATA	0X04


#define MAG_INVALID_VALUE -1

#define EVENT_TYPE_MAGEL_X          ABS_X
#define EVENT_TYPE_MAGEL_Y          ABS_Y
#define EVENT_TYPE_MAGEL_Z          ABS_Z
#define EVENT_DIV_MAGEL             ABS_RUDDER
#define EVENT_TYPE_MAGEL_STATUS     ABS_WHEEL

#define EVENT_TYPE_O_X          ABS_RX
#define EVENT_TYPE_O_Y          ABS_RY
#define EVENT_TYPE_O_Z          ABS_RZ
#define EVENT_DIV_O             ABS_GAS
#define EVENT_TYPE_O_STATUS     ABS_THROTTLE

#define MAG_DIV_MAX (32767)
#define MAG_DIV_MIN (1)

#define MAG_VALUE_MAX (32767)
#define MAG_VALUE_MIN (-32768)
#define MAG_STATUS_MIN (0)
#define MAG_STATUS_MAX (64)

#define MAX_CHOOSE_G_NUM 5

#define MAX_M_V_SENSOR  5

#define ID_M_V_MAGNETIC 0
#define ID_M_V_ORIENTATION 1

typedef enum mag_type {
	MAGNETIC=0,
	ORIENTATION=1,	
}MAG_TYPE;

struct mag_data_path
{
	int div_m;
	int div_o;	
	int (*get_data_m)(int *x,int *y, int *z,int *status);
	int (*get_data_o)(int *x,int *y, int *z,int *status);
	int (*get_raw_data)(int *x,int *y, int *z);//version3 sensor common layer factory mode API2
	
};

struct mag_control_path
{
	int (*m_open_report_data)(int en);
	int (*m_set_delay)(u64 delay);
	int (*m_enable)(int en);
	int (*o_open_report_data)(int en);
	int (*o_set_delay)(u64 delay);
	int (*o_enable)(int en);
	bool is_report_input_direct;
	bool is_support_batch;
	bool is_use_common_factory;
};


struct mag_init_info
{
    char *name;
	int (*init)(void);
	int (*uninit)(void);
	struct platform_driver* platform_diver_addr;
};

struct mag_data{
	hwm_sensor_data mag_data ;
	int data_updata;
	//struct mutex lock;
};

struct mag_drv_obj {
    void *self;
	int polling;
	int (*mag_operate)(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout);
};

struct mag_context {
	struct input_dev   *idev;
	struct miscdevice   mdev;
	struct work_struct  report;
	struct mutex mag_op_mutex;
	atomic_t            delay; /*polling period for reporting input event*/
	atomic_t            wake;  /*user-space request to wake-up, used with stop*/
	struct timer_list   timer;  /* polling timer */
	atomic_t            trace;

	struct early_suspend    early_drv;
	struct mag_data_path mag_dev_data;
	struct mag_control_path mag_ctl;
	atomic_t                early_suspend;
	struct mag_drv_obj*    drv_obj[MAX_M_V_SENSOR];
	struct mag_data       drv_data[MAX_M_V_SENSOR];
	bool is_first_data_after_enable;
	bool is_polling_run;
	bool				is_batch_enable;
	uint32_t			active_nodata_sensor;			
	uint32_t			active_data_sensor;		
	//bool mag_type_enabled[MAX_M_V_SENSOR];
	//uint32_t mag_active_sensor;	
	 
};

extern int mag_attach(int sensor,struct mag_drv_obj *obj);

extern int mag_driver_add(struct mag_init_info* obj) ;
extern int mag_data_report(MAG_TYPE type,int x, int y, int z,int status);
extern int mag_register_control_path(struct mag_control_path *ctl);
extern int mag_register_data_path(struct mag_data_path *ctl);




#endif
