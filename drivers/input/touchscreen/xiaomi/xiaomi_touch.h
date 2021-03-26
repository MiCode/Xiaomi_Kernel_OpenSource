#ifndef __XIAOMI__TOUCH_H
#define __XIAOMI__TOUCH_H
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/slab.h>


/*CUR,DEFAULT,MIN,MAX*/
#define VALUE_TYPE_SIZE 6
#define VALUE_GRIP_SIZE 9
#define MAX_BUF_SIZE 256

/*Xiaomi Touch driver log level
  *error    : 0
  *info      : 1
  *notice : 2
  *debug : 3
*/
extern int mi_log_level;

#define 	TOUCH_ERROR    0
#define 	TOUCH_INFO         1
#define 	TOUCH_NOTICE   2
#define 	TOUCH_DEBUG    3


#define MI_TOUCH_LOGD(level, fmt, args...) {\
	if (mi_log_level == TOUCH_DEBUG && level == 1) {\
		pr_info("[TouchDebug]"fmt, ##args); } \
	}

#define MI_TOUCH_LOGN(level, fmt, args...) {\
	if (mi_log_level >= TOUCH_NOTICE && level == 1) {\
		pr_info(fmt, ##args); } \
	}

#define MI_TOUCH_LOGI(level, fmt, args...) {\
	if (mi_log_level >= TOUCH_INFO && level == 1) {\
		pr_info(fmt, ##args); } \
	}

#define MI_TOUCH_LOGE(level, fmt, args...) {\
	if (level == 1) {\
		pr_err(fmt, ##args); } \
	}


enum MODE_CMD {
	SET_CUR_VALUE = 0,
	GET_CUR_VALUE,
	GET_DEF_VALUE,
	GET_MIN_VALUE,
	GET_MAX_VALUE,
	GET_MODE_VALUE,
	RESET_MODE,
	SET_LONG_VALUE,
};

enum  MODE_TYPE {
	Touch_Game_Mode        = 0,
	Touch_Active_MODE      = 1,
#ifdef CONFIG_NEW_TOUCH_GAMEMODE
	Touch_Tap_Sensitivity	    = 2,
	Touch_Follow_Performance    = 3,
	Touch_Aim_Sensitivity       = 4,
	Touch_Tap_Stability         = 5,
	Touch_Expert_Mode           = 6,
#else
	Touch_UP_THRESHOLD     = 2,
	Touch_Tolerance        = 3,
	Touch_Wgh_Min          = 4,
	Touch_Wgh_Max          = 5,
	Touch_Wgh_Step         = 6,
#endif
	Touch_Edge_Filter      = 7,
	Touch_Panel_Orientation = 8,
	Touch_Report_Rate      = 9,
	Touch_Fod_Enable       = 10,
	Touch_Aod_Enable       = 11,
	Touch_Resist_RF        = 12,
	Touch_Idle_Time        = 13,
	Touch_Doubletap_Mode   = 14,
	Touch_Grip_Mode        = 15,
	Touch_FodIcon_Enable   = 16,
	Touch_Nonui_Mode       = 17,
	Touch_Debug_Level      = 18,
	Touch_Power_Status     = 19,
	Touch_Mode_NUM         = 20,
};

struct xiaomi_touch_interface {
	int touch_mode[Touch_Mode_NUM][VALUE_TYPE_SIZE];
	int touch_edge[VALUE_GRIP_SIZE];
	int (*setModeValue)(int Mode, int value);
	int (*setModeLongValue)(int Mode, int value_len, int *value);
	int (*getModeValue)(int Mode, int value_type);
	int (*getModeAll)(int Mode, int *modevalue);
	int (*resetMode)(int Mode);
	int (*p_sensor_read)(void);
	int (*p_sensor_write)(int on);
	int (*palm_sensor_read)(void);
	int (*palm_sensor_write)(int on);
	char (*touch_vendor_read)(void);
	char (*panel_vendor_read)(void);
	char (*panel_color_read)(void);
	char (*panel_display_read)(void);
	int long_mode_len;
	int long_mode_value[MAX_BUF_SIZE];
};

struct xiaomi_touch {
	struct miscdevice 	misc_dev;
	struct device *dev;
	struct class *class;
	struct attribute_group attrs;
	struct mutex  mutex;
	struct mutex  palm_mutex;
	struct mutex  psensor_mutex;
	wait_queue_head_t 	wait_queue;
};

struct xiaomi_touch_pdata{
	struct xiaomi_touch *device;
	struct xiaomi_touch_interface *touch_data;
	int palm_value;
	bool palm_changed;
	int psensor_value;
	bool psensor_changed;
	const char *name;
	u8 debug_log;
};

struct xiaomi_touch *xiaomi_touch_dev_get(int minor);

extern struct class *get_xiaomi_touch_class(void);

extern struct device *get_xiaomi_touch_dev(void);

extern int update_palm_sensor_value(int value);

extern int update_p_sensor_value(int value);

int xiaomitouch_register_modedata(struct xiaomi_touch_interface *data);

#endif
