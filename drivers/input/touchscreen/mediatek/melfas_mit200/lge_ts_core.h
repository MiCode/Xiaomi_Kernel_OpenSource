/* lge_ts_core.h
 *
 * Copyright (C) 2011 LGE.
 *
 * Author: yehan.ahn@lge.com, hyesung.shin@lge.com
 * Modified by: WX-BSP-TS@lge.com
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef LGE_TS_CORE_H
#define LGE_TS_CORE_H

/* FEATURE */
#define IS_MTK
#define USE_DMA

#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/wakelock.h>
#include <mach/eint.h>
#ifdef IS_MTK
#include <mt_pm_ldo.h>
#endif
//#define TOUCH_USE_DSV

#define POWER_FW_UP_LOCK	0x01
#define POWER_SYSFS_LOCK	0x02

#define MAX_FINGER		10
#define MAX_BUTTON		4

#ifndef IS_MTK
#define BATT_THERMAL		"/sys/class/hwmon/hwmon0/device/batt_therm"
#define BATT_PRESENT		"/sys/class/power_supply/battery/present"
#else
#include "tpd.h"
#define BATT_THERMAL		"/sys/class/power_supply/battery/batt_temp"
#define BATT_PRESENT		"/sys/class/power_supply/battery/present"

#endif


struct touch_device_caps
{
	u8	button_support;
	u16	y_button_boundary;
	u32	button_margin;		// percentage %
	u8	number_of_button;
	u32	button_name[MAX_BUTTON];
	u8	is_width_supported;
	u8 	is_pressure_supported;
	u8	is_id_supported;
	u32	max_width;
	u32	max_pressure;
	u32	max_id;
	int	x_max;
	u32	y_max;
	u32	lcd_x;
	u32	lcd_y;
};

struct touch_limit_value {
	u32	raw_data_max;
	u32	raw_data_min;
	u32	raw_data_margin;
	u32	raw_data_otp_min;
	u32	raw_data_otp_max;
	u32	open_short_min;
	u32	slope_max;
	u32	slope_min;
};

struct touch_operation_role
{
	u8	key_type;		// none = 0, hard_touch_key = 1, virtual_key = 2
	u32	booting_delay;	// ms
	u32	reset_delay;	// ms
	u8	suspend_pwr;
	u8	resume_pwr;

	int	ghost_finger_solution_enable;
	unsigned long	irqflags;
	int	show_touches;
	int	pointer_location;
	int	ta_debouncing_count;
	int	ta_debouncing_finger_num;
	int	ghost_detection_enable;
	int	use_crack_mode;	// Yes = 1, No = 0
};


#define TOUCH_PWR_NUM	3
struct touch_power_info {
	u8	type;			/* 0:none, 1 : gpio, 2: regulator */
	char	name[16];	/* if type == 1 : gpio active contition "low" or "high" */
						/* if type == 2 : supply name for regulator */
	int	value;			/* if type == 1 : gpio pin no. */
						/* if type == 2 : regulator voltage */
};

#define NAME_BUFFER_SIZE 	128

#define PRESS_KEY				1
#define RELEASE_KEY				0

enum {
	LPWG_NONE         = 0,
	LPWG_DOUBLE_TAP   = (1U << 0),  /* 1 */
	LPWG_MULTI_TAP    = (1U << 1),  /* 2 */
	LPWG_SIGNATURE    = (1U << 2),  /* 4 */
};

enum {
	LCD_OFF =0,
	LCD_ON,
};

enum {
	DEV_SUSPEND = 0,
	DEV_RESUME,
	DEV_RESUME_ENABLE,
};

enum {
	CMD_LPWG_ENABLE = 1,
	CMD_LPWG_LCD = 2,
	CMD_LPWG_ACTIVE_AREA = 3,
	CMD_LPWG_TAP_COUNT = 4,
	CMD_LPWG_LCD_RESUME_SUSPEND = 6,
	CMD_LPWG_PROX = 7,
	CMD_LPWG_DOUBLE_TAP_CHECK = 8,
	CMD_LPWG_TOTAL_STATUS = 9,
};

struct touch_platform_data
{
	unsigned long	int_pin;
	unsigned long	reset_pin;
	unsigned long	scl_pin;
	unsigned long	sda_pin;
	int	id_pin;
	int	id2_pin;
	int	panel_id;
#if defined(TOUCH_USE_DSV)
	int	use_dsv;
	int	sensor_value;
	int	enable_sensor_interlock;
#endif
	volatile int curr_pwr_state;
	char	panel_type[4];
	char 	panel_on;
	char	ic_type;
	char	auto_fw_update;
	char	maker[30];
	char	fw_product[9];
	char	fw_image[NAME_BUFFER_SIZE];
	char	panel_spec[NAME_BUFFER_SIZE];
	bool	selfdiagnostic_state[3];
	struct touch_power_info		pwr[TOUCH_PWR_NUM];
	struct touch_device_caps	*caps;
	struct touch_operation_role	*role;
	struct touch_limit_value	*limit;
	struct touch_tci_info		*tci_info;
	u8 lpwg_mode;
	u8 send_lpwg;
	u8 lpwg_debug_enable;
	u8 lpwg_fail_reason;

	//add lpwg point
	int lpwg_x[10];
	int lpwg_y[10];
	int tap_count;
	int lpwg_size;
	int lpwg_panel_on;
	int lpwg_prox;
	int active_area_x1;
	int active_area_x2;
	int active_area_y1;
	int active_area_y2;
	int active_area_gap;
	int double_tap_check;
	int check_openshort;

};

struct touch_tci_info {

	int idle_report_rate;
	int active_report_rate;
	int sensitivity;

	int touch_slope;
	int min_distance;
	int max_distance;
	int min_intertap;
	int max_intertap;
	int tap_count;

//only multi tap
	int touch_slope_2;
	int min_distance_2;
	int max_distance_2;
	int min_intertap_2;
	int max_intertap_2;
	int interrupt_delay_2;

};

struct t_data
{
	u16	id;
	u16	x_position;
	u16	y_position;
	u16	width_major;
	u16	width_minor;
	u16	width_orientation;
	u16	pressure;
	u8	status;
	u8	point_log_state;
	u8  touch_conut;
};

struct b_data
{
	u16	key_code;
	u16	state;
};

struct touch_data
{
	u8	total_num;
	u8	prev_total_num;
	u8	state;
	u8	palm;
	u8	touch_count_num;
	bool	zero_pressure;
	struct t_data	curr_data[MAX_FINGER];
	struct t_data	prev_data[MAX_FINGER];
	struct b_data	curr_button;
	struct b_data	prev_button;
};

struct touch_fw_info
{
	char path[NAME_BUFFER_SIZE];
	volatile bool is_downloading;
	bool force_upgrade;
	bool eraseonly;
	int request;
	struct firmware *fw;
};

struct rect
{
	u16	left;
	u16	right;
	u16	top;
	u16	bottom;
};

struct section_info
{
	struct rect panel;
	struct rect button[MAX_BUTTON];
	struct rect button_cancel[MAX_BUTTON];
	u16 b_inner_width;
	u16 b_width;
	u16 b_margin;
	u16 b_height;
	u16 b_num;
	u16 b_name[MAX_BUTTON];
};

struct ghost_finger_ctrl {
	volatile u8	 stage;
	int		incoming_call;
	int probe;
	int count;
	int min_count;
	int max_count;
	int ghost_check_count;
	int saved_x;
	int saved_y;
	int saved_last_x;
	int saved_last_y;
	int max_moved;
	int max_pressure;
};

struct touch_device_driver {
	int	(*probe)	(struct i2c_client *client, struct touch_platform_data *pdata);
	int	(*resolution)	(struct i2c_client *client);
	void	(*remove)	(struct i2c_client *client);
	int	(*init)		(struct i2c_client *client, struct touch_fw_info* info);
	int	(*data)		(struct i2c_client *client, struct touch_data* data);
	int	(*power)	(struct i2c_client *client, int power_ctrl);
	int	(*ic_ctrl)	(struct i2c_client *client, u32 code, u32 value);
	int 	(*fw_upgrade)	(struct i2c_client *client, struct touch_fw_info* info);
	int (*sysfs) 	(struct i2c_client *client, char *buf1, const char *buf2, u32 code);
	enum window_status (*inspection_crack)	(struct i2c_client *client);
#if defined(TOUCH_USE_DSV)
	void (*dsv_control)		(struct i2c_client *client);
#endif
};

extern struct wake_lock touch_wake_lock;

enum {
	POLLING_MODE = 0,
	INTERRUPT_MODE,
	HYBRIDE_MODE
};

enum {
	POWER_OFF = 0,
	POWER_ON,
	POWER_SLEEP,
	POWER_WAKE
};

enum {
	KEY_NONE = 0,
	TOUCH_HARD_KEY,
	TOUCH_SOFT_KEY,
	VIRTUAL_KEY,
};

enum {
	CONTINUOUS_REPORT_MODE = 0,
	REDUCED_REPORT_MODE,
};

enum {
	RESET_NONE = 0,
	SOFT_RESET,
	PIN_RESET,
	VDD_RESET,
};

enum {
	DOWNLOAD_COMPLETE = 0,
	UNDER_DOWNLOADING,
};

enum {
	REQ_UPGRADE = 0,
	REQ_FILEINFO,
};

enum {
	OP_NULL = 0,
	OP_RELEASE,
	OP_SINGLE,
	OP_MULTI,
	OP_LOCK,
};

enum {
	KEY_NULL=0,
	KEY_PANEL,
	KEY_BOUNDARY
};

enum {
	DO_NOT_ANYTHING = 0,
	ABS_PRESS,
	ABS_RELEASE,
	BUTTON_PRESS,
	BUTTON_RELEASE,
	BUTTON_CANCEL,
	TOUCH_BUTTON_LOCK,
	TOUCH_ABS_LOCK
};

enum {
	BUTTON_RELEASED	= 0,
	BUTTON_PRESSED	= 1,
	BUTTON_CANCELED	= 0xff,
};

enum {
	FINGER_UNUSED   = 0,
	FINGER_RELEASED	= 1,
	ALL_FINGER_RELEASED = 2,
	FINGER_PRESSED	= 3,
	FINGER_HOLD	= 4,
};

enum {
	KEYGUARD_RESERVED = 0,
	KEYGUARD_ENABLE,
};

enum {
	INCOMIMG_CALL_RESERVED,
	INCOMIMG_CALL_TOUCH,
};

enum {
	GHOST_STAGE_CLEAR=0,
	GHOST_STAGE_1=1,
	GHOST_STAGE_2=2,
	GHOST_STAGE_3=4,
	GHOST_STAGE_4=8,
};

enum {
	BASELINE_OPEN = 0,
	BASELINE_FIX,
	BASELINE_REBASE,
};

enum window_status {
	NO_CRACK = 0,
	CRACK,
};

enum{
	CRACK_TEST_FINISH = 0,
	CRACK_TEST_START,
};


struct ic_ctrl_param {
	u32 v1;
	u32 v2;
	u32 v3;
	u32 v4;
};

enum {
	IC_CTRL_CODE_NONE = 0,
	IC_CTRL_BASELINE,
	IC_CTRL_READ,
	IC_CTRL_WRITE,
	IC_CTRL_RESET_CMD,
	IC_CTRL_REPORT_MODE,
	IC_CTRL_FIRMWARE_IMG_SHOW,
	IC_CTRL_INFO_SHOW,
	IC_CTRL_TESTMODE_VERSION_SHOW,
	IC_CTRL_SAVE_IC_INFO,
	IC_CTRL_LPWG,
	IC_CTRL_ACTIVE_AREA,
};

enum {
	SYSFS_REG_CONTROL_STORE,
	SYSFS_LPWG_TCI_STORE,
	SYSFS_CHSTATUS_SHOW,
	SYSFS_CHSTATUS_STORE,
	SYSFS_OPENSHORT_SHOW,
	SYSFS_RAWDATA_SHOW,
	SYSFS_RAWDATA_STORE,
	SYSFS_DELTA_SHOW,
	SYSFS_SELF_DIAGNOSTIC_SHOW,
	SYSFS_VERSION_SHOW,
	SYSFS_TESTMODE_VERSION_SHOW,
	SYSFS_SENSING_ALL_BLOCK_CONTROL,
	SYSFS_SENSING_BLOCK_CONTROL,
	SYSFS_FW_DUMP,
	SYSFS_LPWG_SHOW,
	SYSFS_LPWG_STORE,
	SYSFS_KEYGUARD_STORE,
	SYSFS_LPWG_DEBUG_STORE,
	SYSFS_LPWG_REASON_STORE,
};

enum {
	DEBUG_NONE			= 0,
	DEBUG_BASE_INFO		= (1U << 0),	// 1
	DEBUG_TRACE			= (1U << 1),	// 2
	DEBUG_GET_DATA		= (1U << 2),	// 4
	DEBUG_ABS			= (1U << 3),	// 8
	DEBUG_BUTTON		= (1U << 4),	// 16
	DEBUG_FW_UPGRADE	= (1U << 5),	// 32
	DEBUG_GHOST			= (1U << 6),	// 64
	DEBUG_IRQ_HANDLE	= (1U << 7),	// 128
	DEBUG_POWER			= (1U << 8),	// 256
	DEBUG_JITTER		= (1U << 9),	// 512
	DEBUG_ACCURACY		= (1U << 10),	// 1024
	DEBUG_NOISE			= (1U << 11),	// 2048
	DEBUG_CONFIG		= (1U << 12),	// 4096
	DEBUG_ABS_POINT		= (1U << 13),	// 8196
};

#ifdef LGE_TOUCH_TIME_DEBUG
enum {
	TIME_ISR_START = 0,
	TIME_INT_INTERVAL,
	TIME_THREAD_ISR_START,
	TIME_WORKQUEUE_START,
	TIME_WORKQUEUE_END,
	TIME_FW_UPGRADE_START,
	TIME_FW_UPGRADE_END,
	TIME_PROFILE_MAX,
};

enum {
	DEBUG_TIME_PROFILE_NONE		= 0,
	DEBUG_TIME_INT_INTERVAL		= (1U << 0),	// 1
	DEBUG_TIME_INT_IRQ_DELAY	= (1U << 1),	// 2
	DEBUG_TIME_INT_THREAD_IRQ_DELAY	= (1U << 2),	// 4
	DEBUG_TIME_DATA_HANDLE		= (1U << 3),	// 8
	DEBUG_TIME_FW_UPGRADE		= (1U << 4),	// 16
	DEBUG_TIME_PROFILE_ALL		= (1U << 5),	// 32
};
#endif

enum {
	WORK_POST_COMPLATE = 0,
	WORK_POST_OUT,
	WORK_POST_ERR_RETRY,
	WORK_POST_ERR_CIRTICAL,
	WORK_POST_MAX,
};

enum {
	IGNORE_INTERRUPT = 100,
	NEED_TO_OUT,
	NEED_TO_INIT,
};

enum {
	TIME_EX_INIT_TIME,
	TIME_EX_FIRST_INT_TIME,
	TIME_EX_PREV_PRESS_TIME,
	TIME_EX_CURR_PRESS_TIME,
	TIME_EX_BUTTON_PRESS_START_TIME,
	TIME_EX_BUTTON_PRESS_END_TIME,
	TIME_EX_FIRST_GHOST_DETECT_TIME,
	TIME_EX_SECOND_GHOST_DETECT_TIME,
	TIME_EX_CURR_INT_TIME,
	TIME_EX_PROFILE_MAX
};

#define MAX_RETRY_COUNT		5
#define MAX_GHOST_CHECK_COUNT	3

/* Debug Mask setting */
#define TOUCH_DEBUG_PRINT   	1
#define TOUCH_ERROR_PRINT   	1
#define TOUCH_INFO_PRINT	1

#if defined(TOUCH_INFO_PRINT)
#define TOUCH_INFO_MSG(fmt, args...) 	printk(KERN_ERR "[Touch] " fmt, ##args)
#else
#define TOUCH_INFO_MSG(fmt, args...)    do {} while(0)
#endif

#if defined(TOUCH_ERROR_PRINT)
#define TOUCH_ERR_MSG(fmt, args...) 	printk(KERN_ERR "[Touch E][%s %d] " fmt, \
						__func__, __LINE__, ##args)
#else
#define TOUCH_ERR_MSG(fmt, args...)     do {} while(0)
#endif

#if defined(TOUCH_DEBUG_PRINT)
#define TOUCH_DEBUG_MSG(fmt, args...) 	printk(KERN_ERR "[Touch D] [%s %d] " fmt, \
						__func__, __LINE__, ##args)
#else
#define TOUCH_DEBUG_MSG(fmt, args...)	do {} while(0)
#endif

#define TOUCH_TRACE_FUNC() \
	if(unlikely(touch_debug_mask_ & DEBUG_TRACE)) TOUCH_DEBUG_MSG("\n")

#define TOUCH_FW_MSG(fmt, args...) \
	if (unlikely(touch_debug_mask_ & DEBUG_FW_UPGRADE)) TOUCH_DEBUG_MSG(fmt, ##args)

#define TOUCH_POWER_MSG(fmt, args...) \
	if (unlikely(touch_debug_mask_ & DEBUG_POWER)) TOUCH_DEBUG_MSG(fmt, ##args) 


extern u32 touch_debug_mask_;
#if defined(TOUCH_USE_DSV)
extern void apds9930_register_lux_change_callback (void (*callback) (int));
#endif
void set_touch_handle_(struct i2c_client *client, void* h_touch);
void* get_touch_handle_(struct i2c_client *client);
#if 0
int touch_i2c_read(struct i2c_client *client, u8 reg, u8 *buf, int len);
int touch_i2c_write(struct i2c_client *client, u8 reg, u8 *buf, int len);
int touch_i2c_write_byte(struct i2c_client *client, u8 reg, u8 data);
#endif
int touch_driver_register_(struct touch_device_driver* driver);
void touch_driver_unregister_(void);
void touch_send_wakeup(void *info);

void power_lock(int value);
void power_unlock(int value);

void touch_enable(unsigned int irq);
void touch_disable(unsigned int irq);
void touch_enable_wake(unsigned int irq);
void touch_disable_wake(unsigned int irq);

#define EXTERNAL_FW_PATH	"/etc/firmware/"
#define EXTERNAL_FW_NAME	"mit_ts.fw"


#ifdef USE_DMA
int i2c_msg_transfer ( struct i2c_client *client, struct i2c_msg *msgs, int count );
int i2c_dma_write ( struct i2c_client *client, const uint8_t *buf, int len );
int i2c_dma_read ( struct i2c_client *client, uint8_t *buf, int len );
#endif //USE_DMA

#endif
