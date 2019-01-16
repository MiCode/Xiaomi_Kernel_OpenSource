#ifndef __WAG_H__
#define __WAG_H__


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


#define WAG_TAG		"<WAKE_GESTURE> "
#define WAG_FUN(f)		printk(WAG_TAG"%s\n", __func__)
#define WAG_ERR(fmt, args...)	printk(WAG_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define WAG_LOG(fmt, args...)	printk(WAG_TAG fmt, ##args)
#define WAG_VER(fmt, args...)  printk(WAG_TAG"%s: "fmt, __func__, ##args) //((void)0)

//#define OP_WAG_DELAY		0X01
#define	OP_WAG_ENABLE		0X02
//#define OP_WAG_GET_DATA	0X04

#define WAG_INVALID_VALUE -1

#define EVENT_TYPE_WAG_VALUE		REL_X

#define WAG_VALUE_MAX (32767)
#define WAG_VALUE_MIN (-32768)
#define WAG_STATUS_MIN (0)
#define WAG_STATUS_MAX (64)
#define WAG_DIV_MAX (32767)
#define WAG_DIV_MIN (1)

typedef enum {
	WAG_DEACTIVATE,
	WAG_ACTIVATE,
	WAG_SUSPEND,
	WAG_RESUME
} wag_state_e;

struct wag_control_path
{
//	int (*enable_nodata)(int en);//only enable not report event to HAL
	int (*open_report_data)(int open);//open data rerport to HAL
//	int (*enable)(int en);
	//bool is_support_batch;//version2.used for batch mode support flag
};

struct wag_data_path
{
	int (*get_data)(u16 *value, int *status);
};

struct wag_init_info
{
    	char *name;
	int (*init)(void);
	int (*uninit)(void);
	struct platform_driver* platform_diver_addr;
};

struct wag_data{
	hwm_sensor_data wag_data ;
	int data_updata;
	//struct mutex lock;
};

struct wag_drv_obj {
	void *self;
	int polling;
	int (*wag_operate)(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout);
};

struct wag_context {
	struct input_dev   *idev;
	struct miscdevice   mdev;
	struct work_struct  report;
	struct mutex wag_op_mutex;
	atomic_t            wake;  /*user-space request to wake-up, used with stop*/
	atomic_t            trace;
	struct timer_list   notify_timer;

	struct early_suspend    early_drv;
	atomic_t                early_suspend;
	atomic_t                suspend;

	struct wag_data       drv_data;
	struct wag_control_path   wag_ctl;
	struct wag_data_path   wag_data;
	bool			is_active_nodata;		// Active, but HAL don't need data sensor. such as orientation need
	bool			is_active_data;		// Active and HAL need data .
	bool 		is_batch_enable;	//version2.this is used for judging whether sensor is in batch mode
};

extern int wag_notify(void);
extern int wag_driver_add(struct wag_init_info* obj) ;
extern int wag_register_control_path(struct wag_control_path *ctl);
extern int wag_register_data_path(struct wag_data_path *data);

#endif
