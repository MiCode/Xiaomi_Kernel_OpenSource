#ifndef __GLG_H__
#define __GLG_H__


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


#define GLG_TAG		"<GLANCE_GESTURE> "
#define GLG_FUN(f)		printk(GLG_TAG"%s\n", __func__)
#define GLG_ERR(fmt, args...)	printk(GLG_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define GLG_LOG(fmt, args...)	printk(GLG_TAG fmt, ##args)
#define GLG_VER(fmt, args...)  printk(GLG_TAG"%s: "fmt, __func__, ##args) //((void)0)

//#define OP_GLG_DELAY		0X01
#define	OP_GLG_ENABLE		0X02
//#define OP_GLG_GET_DATA	0X04

#define GLG_INVALID_VALUE -1

#define EVENT_TYPE_GLG_VALUE		REL_X

#define GLG_VALUE_MAX (32767)
#define GLG_VALUE_MIN (-32768)
#define GLG_STATUS_MIN (0)
#define GLG_STATUS_MAX (64)
#define GLG_DIV_MAX (32767)
#define GLG_DIV_MIN (1)

typedef enum {
	GLG_DEACTIVATE,
	GLG_ACTIVATE,
	GLG_SUSPEND,
	GLG_RESUME
} glg_state_e;

struct glg_control_path
{
//	int (*enable_nodata)(int en);//only enable not report event to HAL
	int (*open_report_data)(int open);//open data rerport to HAL
//	int (*enable)(int en);
	//bool is_support_batch;//version2.used for batch mode support flag
};

struct glg_data_path
{
	int (*get_data)(u16 *value, int *status);
};

struct glg_init_info
{
    	char *name;
	int (*init)(void);
	int (*uninit)(void);
	struct platform_driver* platform_diver_addr;
};

struct glg_data{
	hwm_sensor_data glg_data ;
	int data_updata;
	//struct mutex lock;
};

struct glg_drv_obj {
	void *self;
	int polling;
	int (*glg_operate)(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout);
};

struct glg_context {
	struct input_dev   *idev;
	struct miscdevice   mdev;
	struct work_struct  report;
	struct mutex glg_op_mutex;
	atomic_t            wake;  /*user-space request to wake-up, used with stop*/
	atomic_t            trace;
	struct timer_list   notify_timer;

	struct early_suspend    early_drv;
	atomic_t                early_suspend;
	atomic_t                suspend;

	struct glg_data       drv_data;
	struct glg_control_path   glg_ctl;
	struct glg_data_path   glg_data;
	bool			is_active_nodata;		// Active, but HAL don't need data sensor. such as orientation need
	bool			is_active_data;		// Active and HAL need data .
	bool 		is_batch_enable;	//version2.this is used for judging whether sensor is in batch mode
};

extern int glg_notify(void);
extern int glg_driver_add(struct glg_init_info* obj) ;
extern int glg_register_control_path(struct glg_control_path *ctl);
extern int glg_register_data_path(struct glg_data_path *data);

#endif
