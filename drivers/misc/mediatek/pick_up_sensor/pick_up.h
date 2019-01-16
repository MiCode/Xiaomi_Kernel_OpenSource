#ifndef __PKUP_H__
#define __PKUP_H__


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


#define PKUP_TAG		"<PICK_UP> "
#define PKUP_FUN(f)		printk(PKUP_TAG"%s\n", __func__)
#define PKUP_ERR(fmt, args...)	printk(PKUP_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define PKUP_LOG(fmt, args...)	printk(PKUP_TAG fmt, ##args)
#define PKUP_VER(fmt, args...)  printk(PKUP_TAG"%s: "fmt, __func__, ##args) //((void)0)

//#define OP_PKUP_DELAY		0X01
#define	OP_PKUP_ENABLE		0X02
//#define OP_PKUP_GET_DATA	0X04

#define PKUP_INVALID_VALUE -1

#define EVENT_TYPE_PKUP_VALUE		REL_X

#define PKUP_VALUE_MAX (32767)
#define PKUP_VALUE_MIN (-32768)
#define PKUP_STATUS_MIN (0)
#define PKUP_STATUS_MAX (64)
#define PKUP_DIV_MAX (32767)
#define PKUP_DIV_MIN (1)

typedef enum {
	PKUP_DEACTIVATE,
	PKUP_ACTIVATE,
	PKUP_SUSPEND,
	PKUP_RESUME
} pkup_state_e;

struct pkup_control_path
{
//	int (*enable_nodata)(int en);//only enable not report event to HAL
	int (*open_report_data)(int open);//open data rerport to HAL
//	int (*enable)(int en);
	//bool is_support_batch;//version2.used for batch mode support flag
};

struct pkup_data_path
{
	int (*get_data)(u16 *value, int *status);
};

struct pkup_init_info
{
    	char *name;
	int (*init)(void);
	int (*uninit)(void);
	struct platform_driver* platform_diver_addr;
};

struct pkup_data{
	hwm_sensor_data pkup_data ;
	int data_updata;
	//struct mutex lock;
};

struct pkup_drv_obj {
    void *self;
	int polling;
	int (*pkup_operate)(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout);
};

struct pkup_context {
	struct input_dev   *idev;
	struct miscdevice   mdev;
	struct work_struct  report;
	struct mutex pkup_op_mutex;
	atomic_t            wake;  /*user-space request to wake-up, used with stop*/
	atomic_t            trace;

	struct early_suspend    early_drv;
	atomic_t                early_suspend;
	atomic_t                suspend;

	struct pkup_data       drv_data;
	struct pkup_control_path   pkup_ctl;
	struct pkup_data_path   pkup_data;
	bool			is_active_nodata;		// Active, but HAL don't need data sensor. such as orientation need
	bool			is_active_data;		// Active and HAL need data .
	bool 		is_batch_enable;	//version2.this is used for judging whether sensor is in batch mode
};

extern int pkup_notify(void);
extern int pkup_driver_add(struct pkup_init_info* obj) ;
extern int pkup_register_control_path(struct pkup_control_path *ctl);
extern int pkup_register_data_path(struct pkup_data_path *data);

#endif
