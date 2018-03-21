/*
 * Goodix GT9xx touchscreen driver
 *
 * Copyright  (C)  2010 - 2014 Goodix. Ltd.
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * Version: 2.4
 * Release Date: 2014/11/28
 */

#include <linux/irq.h>
#include "gt9xx.h"

#if GTP_ICS_SLOT_REPORT
	#include <linux/input/mt.h>
#endif


#if  WT_ADD_CTP_INFO
#include <linux/hardware_info.h>
static char tp_string_version[40];
static int tp_sensor_id = -1;
static u8 cfg_version;

/*TP Color*/
#define  TP_White       0x31
#define  TP_Black       0x32
#define  TP_Golden    0x38
/*TP Maker*/
#define TP_BIEL      0x31
#define GTP_LOCKDOWN_SIZE	8
#define GTP_READ_LOCKDOWN_INFO_ADDR  0x81A0
static u8 lockdown_info[GTP_LOCKDOWN_SIZE];
static u8 TP_Maker, LCD_Maker, Panel_Ink;
extern u8 tp_color;
static u16 IC_Version;
static int gtp_read_lockdown_info(void);
int gtp_hardwareinfo_set(void);
#endif

static const char *goodix_ts_name = "goodix-ts";
static const char *goodix_input_phys = "input/ts";
static struct workqueue_struct *goodix_wq;
struct i2c_client *i2c_connect_client = NULL;
/*
int gtp_rst_gpio;
int gtp_int_gpio;
*/
unsigned int gtp_rst_gpio;
unsigned int gtp_int_gpio;


int gtp_ics_slot_report = 0;/*use to choose report mode A or B, 0 for A and 1 for B */


u8 config[GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH]
				= {GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff};


#ifdef GTP_GLOVE_MODE
u8 gtp_glove_config[GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH]
				= {GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff};
#endif


#if GTP_HAVE_TOUCH_KEY
	static const u16 touch_key_array[] = GTP_KEY_TAB;
	#define GTP_MAX_KEY_NUM  (sizeof(touch_key_array)/sizeof(touch_key_array[0]))

	#define GTP_KEY_MAP_ARRAY         {{175, 1500} , {387, 1500} , {582, 1500} }
	struct touch_virtual_key_map_t {
	int point_x;
	int point_y;
};
static struct touch_virtual_key_map_t touch_key_point_maping_array[] = GTP_KEY_MAP_ARRAY;


#if GTP_DEBUG_ON
	static const int  key_codes[] = {KEY_HOME, KEY_BACK, KEY_MENU, KEY_SEARCH};
	static const char *key_names[] = {"Key_Home", "Key_Back", "Key_Menu", "Key_Search"};
#endif

#endif

#ifdef PMX_DRIVER_GT915L
#define PINCTRL_STATE_ACTIVE			"pmx_ts_active"
#define PINCTRL_STATE_SUSPEND		"pmx_ts_suspend"
#define PINCTRL_STATE_RELEASE		"pmx_ts_release"
static int goodix_ts_pinctrl_init(struct goodix_ts_data *goodix_data);
static int goodix_ts_pinctrl_select(struct goodix_ts_data *goodix_data, bool on);
#endif

static s8 gtp_i2c_test(struct i2c_client *client);
void gtp_reset_guitar(struct i2c_client *client, s32 ms);
s32 gtp_send_cfg(struct i2c_client *client);
void gtp_int_sync(s32 ms);

static ssize_t gt91xx_config_read_proc(struct file *, char __user *, size_t, loff_t *);
static ssize_t gt91xx_config_write_proc(struct file *, const char __user *, size_t, loff_t *);

static struct proc_dir_entry *gt91xx_config_proc = NULL;
static const struct file_operations config_proc_ops = {
	.owner = THIS_MODULE,
	.read = gt91xx_config_read_proc,
	.write = gt91xx_config_write_proc,
};
static int gtp_register_powermanger(struct goodix_ts_data *ts);
static int gtp_unregister_powermanger(struct goodix_ts_data *ts);

#if GTP_CREATE_WR_NODE
extern s32 init_wr_node(struct i2c_client *);
extern void uninit_wr_node(void);
#endif

#if GTP_AUTO_UPDATE
extern u8 gup_init_update_proc(struct goodix_ts_data *);
#endif

#if GTP_ESD_PROTECT
static struct delayed_work gtp_esd_check_work;
static struct workqueue_struct *gtp_esd_check_workqueue = NULL;
static void gtp_esd_check_func(struct work_struct *);
static s32 gtp_init_ext_watchdog(struct i2c_client *client);
void gtp_esd_switch(struct i2c_client *, s32);
#endif


#if GTP_COMPATIBLE_MODE
extern s32 i2c_read_bytes(struct i2c_client *client, u16 addr, u8 *buf, s32 len);
extern s32 i2c_write_bytes(struct i2c_client *client, u16 addr, u8 *buf, s32 len);
extern s32 gup_clk_calibration(void);
extern s32 gup_fw_download_proc(void *dir, u8 dwn_mode);
extern u8 gup_check_fs_mounted(char *path_name);

void gtp_recovery_reset(struct i2c_client *client);
static s32 gtp_esd_recovery(struct i2c_client *client);
s32 gtp_fw_startup(struct i2c_client *client);
static s32 gtp_main_clk_proc(struct goodix_ts_data *ts);
static s32 gtp_bak_ref_proc(struct goodix_ts_data *ts, u8 mode);

#endif


#if GTP_GESTURE_WAKEUP
typedef enum {
	DOZE_DISABLED = 0,
	DOZE_ENABLED = 1,
	DOZE_WAKEUP = 2,
} DOZE_T;
static DOZE_T doze_status = DOZE_DISABLED;
static s8 gtp_enter_doze(struct goodix_ts_data *ts);
#endif

#ifdef GTP_CONFIG_OF
int gtp_parse_dt_cfg(struct device *dev, u8 *cfg, int *cfg_len, u8 sid);
#endif


#if GTP_GLOVE_MODE
char gtp_glove_onoff = '0';
static  char gtp_glove_support_flag_changed = 0;
static const char gtp_glove_support_flag = GTP_GLOVE_SUPPORT_ONOFF;
s32 gtp_send_glove_cfg(struct i2c_client *client);

static ssize_t proc_glove_onoff_read(struct file *file, char __user *page, size_t count, loff_t *ppos)
{
	int num;
	if (*ppos)
		return 0;
	if (0 == gtp_glove_support_flag_changed)
		num = sprintf(page, "%c\n", gtp_glove_support_flag);
	else
		num = sprintf(page, "%c\n", gtp_glove_onoff);
	*ppos += num;
	return num;
}

static ssize_t proc_glove_onoff_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
	sscanf(buffer, "%c", &gtp_glove_onoff);
	gtp_glove_support_flag_changed = 1;

	if ('1' == gtp_glove_onoff) {
		u8 cfg_group[] = CTP_CFG_GROUP1_GLOVE;
	gtp_reset_guitar(i2c_connect_client, 50);
	msleep(100);
	memcpy(&gtp_glove_config[GTP_ADDR_LENGTH],  cfg_group, GTP_CONFIG_MAX_LENGTH);
	gtp_send_glove_cfg(i2c_connect_client);
	} else if ('0' == gtp_glove_onoff) {
	gtp_reset_guitar(i2c_connect_client, 50);
	msleep(100);
	gtp_send_cfg(i2c_connect_client);
	}
	return count;
}

static const struct file_operations gt_glove_onoff_proc_fops = {
	.write = proc_glove_onoff_write,
	.read = proc_glove_onoff_read,
	.open = simple_open,
	.owner = THIS_MODULE,
};

#endif


#if GTP_GESTURE_WAKEUP
char gtp_gesture_coordinate[60] = {0X8140 >> 8, 0X8140 & 0xFF};
char gtp_gesture_value = 0;
char gtp_gesture_onoff = '0';
const char gtp_gesture_type[] = GTP_GESTURE_TPYE_STR;
static const char gtp_gesture_support_flag = GTP_GESTURE_SUPPORT_ONOFF;
static const char gtp_verson[] = GTP_PROC_DRIVER_VERSION;
static  char gtp_gesture_support_flag_changed = 0;
struct i2c_client *i2c_client_point = NULL;


static ssize_t proc_gesture_data_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
	int num = 0;
	if (*ppos)
		return 0;
	printk("proc_gesture_data_read=%d\n", gtp_gesture_value);
	num =  sprintf(buffer, "%c\n", gtp_gesture_value);
	*ppos += num;
	return num;
}

static ssize_t proc_gesture_data_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
	sscanf(buffer, "%c", &gtp_gesture_value);
	return count;
}

static ssize_t proc_gesture_type_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
	int num;
	if (*ppos) {
		printk("%s\n", __func__);
		return 0;
		}
	num = sprintf(buffer, "%s\n", gtp_gesture_type);
	*ppos += num;
	return num;
}

static ssize_t proc_gesture_type_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
	return -EPERM;
}

static ssize_t proc_gesture_onoff_read(struct file *file, char __user *page, size_t count, loff_t *ppos)
{
	int num;
	if (*ppos)
		return 0;
	if (0 == gtp_gesture_support_flag_changed)
		num = sprintf(page, "%c\n", gtp_gesture_support_flag);
	else
		num = sprintf(page, "%c\n", gtp_gesture_onoff);
	*ppos += num;
	return num;
}

static ssize_t proc_gesture_onoff_write(struct file *file,  const char __user *buffer, size_t count, loff_t *ppos)
{
	sscanf(buffer, "%c", &gtp_gesture_onoff);
	gtp_gesture_support_flag_changed = 1;
	printk("in proc_gesture_onoff_write %c \n", gtp_gesture_onoff);
	return count;
}

static ssize_t proc_version_read(struct file *file, char __user *page, size_t count, loff_t *ppos)
{
	int num;
	if (*ppos)
		return 0;
	num = sprintf(page, "%s\n", gtp_verson);
	*ppos += num;
	return num;
}

static ssize_t proc_version_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
	return count;
}

static ssize_t proc_gesture_coordinate_read(struct file *file, char __user *page, size_t count, loff_t *ppos)
{
	if (*ppos)
		return 0;
	memcpy(page, gtp_gesture_coordinate, 60);
	printk("GTP gtp_ges 0x%02x\n", gtp_gesture_coordinate[13]);
	memset(gtp_gesture_coordinate, 0, 60);
	*ppos += 60;
	return 60;
}

static ssize_t proc_gesture_coordinate_write(struct file *file,  const char __user *buffer, size_t count, loff_t *ppos)
{
	return count;
}

static const struct file_operations gt_gesture_var_proc_fops = {
	.write = proc_gesture_data_write,
	.read = proc_gesture_data_read,
	.open = simple_open,
	.owner = THIS_MODULE,
};

static const struct file_operations gt_gesture_type_proc_fops = {
	.write = proc_gesture_type_write,
	.read = proc_gesture_type_read,
	.open = simple_open,
	.owner = THIS_MODULE,
};

static const struct file_operations gt_gesture_onoff_proc_fops = {
	.write = proc_gesture_onoff_write,
	.read = proc_gesture_onoff_read,
	.open = simple_open,
	.owner = THIS_MODULE,
};

static const struct file_operations gt_version_proc_fops = {
	.write = proc_version_write,
	.read = proc_version_read,
	.open = simple_open,
	.owner = THIS_MODULE,
};

static const struct file_operations gt_coordinate_proc_fops = {
	.write = proc_gesture_coordinate_write,
	.read = proc_gesture_coordinate_read,
	.open = simple_open,
	.owner = THIS_MODULE,
};


void Ctp_Gesture_Fucntion_Proc_File(void)
{
	struct proc_dir_entry *ctp_device_proc = NULL;
	struct proc_dir_entry *ctp_gesture_wakup_proc = NULL;
	struct proc_dir_entry *ctp_gesture_var_proc = NULL;
	struct proc_dir_entry *ctp_gesture_type_proc = NULL;
	struct proc_dir_entry *ctp_gesture_onoff_proc = NULL;
	struct proc_dir_entry *ctp_glove_onoff_proc = NULL;
	struct proc_dir_entry *ctp_version_proc = NULL;
	struct proc_dir_entry *ctp_coordinate_proc = NULL;
#define CTP_GESTURE_FUNCTION_AUTHORITY_PROC 0777

	ctp_device_proc = proc_mkdir("touchscreen_feature", NULL);
	ctp_gesture_wakup_proc = proc_mkdir("gesture", NULL);

	ctp_gesture_var_proc = proc_create("data", 0444, ctp_gesture_wakup_proc, &gt_gesture_var_proc_fops);
	if (ctp_gesture_var_proc == NULL) {
		GTP_ERROR("ctp_gesture_var_proc create failed\n");
	}

	ctp_gesture_type_proc = proc_create("gesture_type", 0444, ctp_device_proc, &gt_gesture_type_proc_fops);
	if (ctp_gesture_type_proc == NULL) {
		GTP_ERROR("ctp_gesture_type_proc create failed\n");
	}

	ctp_gesture_onoff_proc = proc_create("onoff", 0666, ctp_gesture_wakup_proc, &gt_gesture_onoff_proc_fops);
	if (ctp_gesture_onoff_proc == NULL) {
		GTP_ERROR("ctp_gesture_onoff_proc create failed\n");
	}

#if GTP_GLOVE_MODE
	ctp_glove_onoff_proc = proc_create("glove_onoff", 0666, ctp_device_proc, &gt_glove_onoff_proc_fops);
	if (ctp_glove_onoff_proc == NULL) {
		GTP_ERROR("ctp_gesture_onoff_proc create failed\n");
	}
#endif

	ctp_version_proc = proc_create("version", 0444, ctp_device_proc, &gt_version_proc_fops);
	if (ctp_version_proc == NULL) {
		GTP_ERROR("create_proc_entry version failed\n");
	}

	ctp_coordinate_proc = proc_create("gesture_coordinate", 0777, ctp_device_proc, &gt_coordinate_proc_fops);
	if (ctp_coordinate_proc == NULL) {
		GTP_ERROR("create_proc_entry version failed\n");
	}

}

#endif





#ifdef CONFIG_TOUCHSCREEN_LOCKDOWN_INFO
static ssize_t ctp_lockdown_proc_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	char *ptr = buf;

	if (*ppos) {
		GTP_INFO("tp test again return\n");
		return 0;
	}
	*ppos += count;

	return  sprintf(ptr, "%02X%02X%02X%02X%02X%02X%02X%02X\n",
				lockdown_info[0], lockdown_info[1], lockdown_info[2], lockdown_info[3],
				lockdown_info[4], lockdown_info[5], lockdown_info[6], lockdown_info[7]);
}
static ssize_t ctp_lockdown_proc_write(struct file *filp, const char __user *userbuf, size_t count, loff_t *ppos)
{
	return -EPERM;
}

static const struct file_operations ctp_lockdown_procs_fops = {
	.write = ctp_lockdown_proc_write,
	.read = ctp_lockdown_proc_read,
	.owner = THIS_MODULE,
};

void ctp_lockdown_fucntion_proc_file(void)
{
	struct proc_dir_entry *ctp_device_proc = NULL;
	struct proc_dir_entry *ctp_lockdown_proc = NULL;

	ctp_device_proc = proc_mkdir("touchscreen", NULL);

	ctp_lockdown_proc = proc_create("lockdown_info", 0666, ctp_device_proc, &ctp_lockdown_procs_fops);
	if (ctp_lockdown_proc == NULL) {
		GTP_ERROR("create_proc_entry lockdown failed\n");
	}
}
#endif


/*******************************************************
Function:
	Read data from the i2c slave device.
Input:
	client:     i2c device.
	buf[0~1]:   read start address.
	buf[2~len-1]:   read data buffer.
	len:    GTP_ADDR_LENGTH + read bytes count
Output:
	numbers of i2c_msgs to transfer:
	2: succeed, otherwise: failed
*********************************************************/
s32 gtp_i2c_read(struct i2c_client *client, u8 *buf, s32 len)
{
	struct i2c_msg msgs[2];
	s32 ret = -1;
	s32 retries = 0;

	GTP_DEBUG_FUNC();

	msgs[0].flags = !I2C_M_RD;
	msgs[0].addr  = client->addr;
	msgs[0].len   = GTP_ADDR_LENGTH;
	msgs[0].buf   = &buf[0];


	msgs[1].flags = I2C_M_RD;
	msgs[1].addr  = client->addr;
	msgs[1].len   = len - GTP_ADDR_LENGTH;
	msgs[1].buf   = &buf[GTP_ADDR_LENGTH];


	while (retries < 5) {
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret == 2)
			break;
		retries++;
	}
	if ((retries >= 5)) {
	#if GTP_COMPATIBLE_MODE
		struct goodix_ts_data *ts = i2c_get_clientdata(client);
	#endif

	#if GTP_GESTURE_WAKEUP

		if (DOZE_ENABLED == doze_status) {
			return ret;
		}
	#endif
		GTP_ERROR("I2C Read: 0x%04X, %d bytes failed, errcode: %d! Process reset.", (((u16)(buf[0] << 8)) | buf[1]), len-2, ret);
	#if GTP_COMPATIBLE_MODE
		if (CHIP_TYPE_GT9F == ts->chip_type) {
			gtp_recovery_reset(client);
		} else
	#endif
		{
			gtp_reset_guitar(client, 10);
		}
	}
	return ret;
}



/*******************************************************
Function:
	Write data to the i2c slave device.
Input:
	client:     i2c device.
	buf[0~1]:   write start address.
	buf[2~len-1]:   data buffer
	len:    GTP_ADDR_LENGTH + write bytes count
Output:
	numbers of i2c_msgs to transfer:
		1: succeed, otherwise: failed
*********************************************************/
s32 gtp_i2c_write(struct i2c_client *client, u8 *buf, s32 len)
{
	struct i2c_msg msg;
	s32 ret = -1;
	s32 retries = 0;

	GTP_DEBUG_FUNC();

	msg.flags = !I2C_M_RD;
	msg.addr  = client->addr;
	msg.len   = len;
	msg.buf   = buf;


	while (retries < 5) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret == 1)
			break;
		retries++;
	}
	if ((retries >= 5)) {
	#if GTP_COMPATIBLE_MODE
		struct goodix_ts_data *ts = i2c_get_clientdata(client);
	#endif

	#if GTP_GESTURE_WAKEUP
		if (DOZE_ENABLED == doze_status) {
			return ret;
		}
	#endif
		GTP_ERROR("I2C Write: 0x%04X, %d bytes failed, errcode: %d! Process reset.", (((u16)(buf[0] << 8)) | buf[1]), len-2, ret);
	#if GTP_COMPATIBLE_MODE
		if (CHIP_TYPE_GT9F == ts->chip_type) {
			gtp_recovery_reset(client);
		} else
	#endif
		{
			gtp_reset_guitar(client, 10);
		}
	}
	return ret;
}


/*******************************************************
Function:
	i2c read twice, compare the results
Input:
	client:  i2c device
	addr:    operate address
	rxbuf:   read data to store, if compare successful
	len:     bytes to read
Output:
	FAIL:    read failed
	SUCCESS: read successful
*********************************************************/
s32 gtp_i2c_read_dbl_check(struct i2c_client *client, u16 addr, u8 *rxbuf, int len)
{
	u8 buf[16] = {0};
	u8 confirm_buf[16] = {0};
	u8 retry = 0;

	while (retry++ < 3) {
		memset(buf, 0xAA, 16);
		buf[0] = (u8)(addr >> 8);
		buf[1] = (u8)(addr & 0xFF);
		gtp_i2c_read(client, buf, len + 2);

		memset(confirm_buf, 0xAB, 16);
		confirm_buf[0] = (u8)(addr >> 8);
		confirm_buf[1] = (u8)(addr & 0xFF);
		gtp_i2c_read(client, confirm_buf, len + 2);

		if (!memcmp(buf, confirm_buf, len+2)) {
			memcpy(rxbuf, confirm_buf+2, len);
			return SUCCESS;
		}
	}
	GTP_ERROR("I2C read 0x%04X, %d bytes, double check failed!", addr, len);
	return FAIL;
}

/*******************************************************
Function:
	Send config.
Input:
	client: i2c device.
Output:
	result of i2c write operation.
		1: succeed, otherwise: failed
*********************************************************/

s32 gtp_send_glove_cfg(struct i2c_client *client)
{
	s32 ret = 2;

#if GTP_DRIVER_SEND_CFG
	s32 retry = 0;
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	if (ts->pnl_init_error) {
		GTP_INFO("Error occured in init_panel, no config sent");
		return 0;
	}
	GTP_INFO("Driver send config.");
	for (retry = 0; retry < 5; retry++) {
		ret = gtp_i2c_write(client, gtp_glove_config , GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH);
		if (ret > 0) {
			break;
		}
	}
#endif
	return ret;
}

/*******************************************************
Function:
	Send config.
Input:
	client: i2c device.
Output:
	result of i2c write operation.
		1: succeed, otherwise: failed
*********************************************************/

s32 gtp_send_cfg(struct i2c_client *client)
{
	s32 ret = 2;

#if GTP_DRIVER_SEND_CFG
	s32 retry = 0;
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	if (ts->pnl_init_error) {
		GTP_INFO("Error occured in init_panel, no config sent");
		return 0;
	}

	GTP_INFO("Driver send config.");
	for (retry = 0; retry < 5; retry++) {
		ret = gtp_i2c_write(client, config , GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH);
		if (ret > 0) {
			break;
		}
	}
#endif
	return ret;
}
/*******************************************************
Function:
	Disable irq function
Input:
	ts: goodix i2c_client private data
Output:
	None.
*********************************************************/
void gtp_irq_disable(struct goodix_ts_data *ts)
{
	unsigned long irqflags;

	GTP_DEBUG_FUNC();

	spin_lock_irqsave(&ts->irq_lock, irqflags);
	if (!ts->irq_is_disable) {
		ts->irq_is_disable = 1;
		disable_irq_nosync(ts->client->irq);
	}
	spin_unlock_irqrestore(&ts->irq_lock, irqflags);
}

/*******************************************************
Function:
	Enable irq function
Input:
	ts: goodix i2c_client private data
Output:
	None.
*********************************************************/
void gtp_irq_enable(struct goodix_ts_data *ts)
{
	unsigned long irqflags = 0;

	GTP_DEBUG_FUNC();

	spin_lock_irqsave(&ts->irq_lock, irqflags);
	if (ts->irq_is_disable) {
		enable_irq(ts->client->irq);
		ts->irq_is_disable = 0;
	}
	spin_unlock_irqrestore(&ts->irq_lock, irqflags);
}


/*******************************************************
Function:
	Report touch point event
Input:
	ts: goodix i2c_client private data
	id: trackId
	x:  input x coordinate
	y:  input y coordinate
	w:  input pressure
Output:
	None.
*********************************************************/
static void gtp_touch_down(struct goodix_ts_data *ts, s32 id, s32 x, s32 y, s32 w)
{
#if GTP_CHANGE_X2Y
	GTP_SWAP(x, y);
#endif


	if (gtp_ics_slot_report == 1) {
		input_mt_slot(ts->input_dev, id);
		input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, id);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, w);
		input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, w);
	} else {
		input_report_key(ts->input_dev, BTN_TOUCH, 1);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, w);
		input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, w);
		input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, id);
		input_mt_sync(ts->input_dev);
	}
	
	GTP_DEBUG("ID:%d, X:%d, Y:%d, W:%d", id, x, y, w);
}

/*******************************************************
Function:
	Report touch release event
Input:
	ts: goodix i2c_client private data
Output:
	None.
*********************************************************/
static void gtp_touch_up(struct goodix_ts_data *ts, s32 id)
{

	if (gtp_ics_slot_report == 1) {
		input_mt_slot(ts->input_dev, id);
		input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, -1);
		GTP_DEBUG("Touch id[%2d] release!", id);
	} else {
		input_report_key(ts->input_dev, BTN_TOUCH, 0);
	}

}

#if GTP_WITH_PEN

static void gtp_pen_init(struct goodix_ts_data *ts)
{
	s32 ret = 0;

	GTP_INFO("Request input device for pen/stylus.");

	ts->pen_dev = input_allocate_device();
	if (ts->pen_dev == NULL) {
		GTP_ERROR("Failed to allocate input device for pen/stylus.");
		return;
	}

	ts->pen_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) ;


	if (gtp_ics_slot_report == 1) {
		input_mt_init_slots(ts->pen_dev, 16);
	} else {
		ts->pen_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	}

	set_bit(BTN_TOOL_PEN, ts->pen_dev->keybit);
	set_bit(INPUT_PROP_DIRECT, ts->pen_dev->propbit);


#if GTP_PEN_HAVE_BUTTON
	input_set_capability(ts->pen_dev, EV_KEY, BTN_STYLUS);
	input_set_capability(ts->pen_dev, EV_KEY, BTN_STYLUS2);
#endif

	input_set_abs_params(ts->pen_dev, ABS_MT_POSITION_X, 0, ts->abs_x_max, 0, 0);
	input_set_abs_params(ts->pen_dev, ABS_MT_POSITION_Y, 0, ts->abs_y_max, 0, 0);
	input_set_abs_params(ts->pen_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(ts->pen_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->pen_dev, ABS_MT_TRACKING_ID, 0, 255, 0, 0);

	ts->pen_dev->name = "goodix-pen";
	ts->pen_dev->id.bustype = BUS_I2C;

	ret = input_register_device(ts->pen_dev);
	if (ret) {
		GTP_ERROR("Register %s input device failed", ts->pen_dev->name);
		return;
	}
}

static void gtp_pen_down(s32 x, s32 y, s32 w, s32 id)
{
	struct goodix_ts_data *ts = i2c_get_clientdata(i2c_connect_client);

#if GTP_CHANGE_X2Y
	GTP_SWAP(x, y);
#endif

	input_report_key(ts->pen_dev, BTN_TOOL_PEN, 1);


	if (gtp_ics_slot_report == 1) {
		input_mt_slot(ts->pen_dev, id);
		input_report_abs(ts->pen_dev, ABS_MT_TRACKING_ID, id);
		input_report_abs(ts->pen_dev, ABS_MT_POSITION_X, x);
		input_report_abs(ts->pen_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(ts->pen_dev, ABS_MT_PRESSURE, w);
		input_report_abs(ts->pen_dev, ABS_MT_TOUCH_MAJOR, w);
	} else {
		input_report_key(ts->pen_dev, BTN_TOUCH, 1);
		input_report_abs(ts->pen_dev, ABS_MT_POSITION_X, x);
		input_report_abs(ts->pen_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(ts->pen_dev, ABS_MT_PRESSURE, w);
		input_report_abs(ts->pen_dev, ABS_MT_TOUCH_MAJOR, w);
		input_report_abs(ts->pen_dev, ABS_MT_TRACKING_ID, id);
		input_mt_sync(ts->pen_dev);
	}

	GTP_DEBUG("(%d)(%d, %d)[%d]", id, x, y, w);
}

static void gtp_pen_up(s32 id)
{
	struct goodix_ts_data *ts = i2c_get_clientdata(i2c_connect_client);

	input_report_key(ts->pen_dev, BTN_TOOL_PEN, 0);


	if (gtp_ics_slot_report == 1) {
		input_mt_slot(ts->pen_dev, id);
		input_report_abs(ts->pen_dev, ABS_MT_TRACKING_ID, -1);
	} else {
		input_report_key(ts->pen_dev, BTN_TOUCH, 0);
	}

}
#endif

/*******************************************************
Function:
	Goodix touchscreen work function
Input:
	work: work struct of goodix_workqueue
Output:
	None.
*********************************************************/
static void goodix_ts_work_func(struct work_struct *work)
{
	u8  end_cmd[3] = {GTP_READ_COOR_ADDR >> 8, GTP_READ_COOR_ADDR & 0xFF, 0};
	u8  point_data[2 + 1 + 8 * GTP_MAX_TOUCH + 1] = {GTP_READ_COOR_ADDR >> 8, GTP_READ_COOR_ADDR & 0xFF};
	u8  touch_num = 0;
	u8  finger = 0;
	static u16 pre_touch;
	static u8 pre_key;
#if GTP_WITH_PEN
	u8 pen_active = 0;
	static u8 pre_pen;
#endif
	u8  key_value = 0;
	u8 *coor_data = NULL;
	s32 input_x = 0;
	s32 input_y = 0;
	s32 input_w = 0;
	s32 id = 0;
	s32 i  = 0;
	s32 ret = -1;
	struct goodix_ts_data *ts = NULL;

#if GTP_COMPATIBLE_MODE
	u8 rqst_buf[3] = {0x80, 0x43};
#endif

#if GTP_GESTURE_WAKEUP
	u8 doze_buf[3] = {0x81, 0x4B};
#endif

	GTP_DEBUG_FUNC();
	ts = container_of(work, struct goodix_ts_data, work);
	if (ts->enter_update) {
		return;
	}
#if GTP_GESTURE_WAKEUP
	if (DOZE_ENABLED == doze_status) {
		ret = gtp_i2c_read(i2c_connect_client, doze_buf, 3);
		GTP_DEBUG("0x814B = 0x%02X", doze_buf[2]);
		if (ret > 0) {
			if ('1' == gtp_gesture_onoff) {
				gtp_gesture_coordinate[0] = 0X8140 >> 8;
				gtp_gesture_coordinate[1] = 0X8140 & 0xFF;

				if ((doze_buf[2] == 'a') || (doze_buf[2] == 'b') || (doze_buf[2] == 'c') ||
						(doze_buf[2] == 'd') || (doze_buf[2] == 'e') || (doze_buf[2] == 'g') ||
						(doze_buf[2] == 'h') || (doze_buf[2] == 'm') || (doze_buf[2] == 'o') ||
						(doze_buf[2] == 'q') || (doze_buf[2] == 's') || (doze_buf[2] == 'v') ||
						(doze_buf[2] == 'w') || (doze_buf[2] == 'y') || (doze_buf[2] == 'z') ||
						(doze_buf[2] == 0x5E) /* ^ */
						) {
					GTP_INFO("Gesture type;%c\n", doze_buf[2]);
					gtp_gesture_value = doze_buf[2];
					if (doze_buf[2] != 0x5E) {
						GTP_INFO("Wakeup by gesture(%c), light up the screen!", doze_buf[2]);
					} else {
						GTP_INFO("Wakeup by gesture(^), light up the screen!");
					}

					gtp_i2c_read(i2c_client_point, gtp_gesture_coordinate, 47);
					if (NULL != strchr(gtp_gesture_type, gtp_gesture_value)) {
						input_report_key(ts->input_dev, KEY_WAKEUP, 1);
						input_sync(ts->input_dev);
						msleep(300);
						input_report_key(ts->input_dev, KEY_WAKEUP, 0);
						input_sync(ts->input_dev);
					};
					doze_buf[2] = 0x00;
					gtp_i2c_write(i2c_connect_client, doze_buf, 3);
				} else if ((doze_buf[2] == 0xAA) || (doze_buf[2] == 0xBB) ||
						(doze_buf[2] == 0xAB) || (doze_buf[2] == 0xBA)) {
					char *direction[4] = {"Right", "Down", "Up", "Left"};
					u8 type = ((doze_buf[2] & 0x0F) - 0x0A) + (((doze_buf[2] >> 4) & 0x0F) - 0x0A) * 2;
					GTP_INFO("%s slide to light up the screen!", direction[type]);
					switch (doze_buf[2]) {
					case 0xAA:
						gtp_gesture_value = 'R';
						break;
					case 0xAB:
						gtp_gesture_value = 'D';
						break;
					case 0xBA:
						gtp_gesture_value = 'U';
						break;
					case 0xBB:
						gtp_gesture_value = 'L';
						break;
					}
					gtp_i2c_read(i2c_client_point, gtp_gesture_coordinate, 47);
					if (NULL != strchr(gtp_gesture_type, gtp_gesture_value)) {
						input_report_key(ts->input_dev, KEY_WAKEUP, 1);
						input_sync(ts->input_dev);
						msleep(300);
						input_report_key(ts->input_dev, KEY_WAKEUP, 0);
						input_sync(ts->input_dev);
					}
					doze_buf[2] = 0x00;
					gtp_i2c_write(i2c_connect_client, doze_buf, 3);
				} else if (0xCC == doze_buf[2]) {
					GTP_INFO("Double click to light up the screen!");
					gtp_gesture_value = 'K';
					gtp_i2c_read(i2c_client_point, gtp_gesture_coordinate, 47);
					if (NULL != strchr(gtp_gesture_type, gtp_gesture_value)) {
						input_report_key(ts->input_dev, KEY_WAKEUP, 1);
						input_sync(ts->input_dev);
						msleep(300);
						input_report_key(ts->input_dev, KEY_WAKEUP, 0);
						input_sync(ts->input_dev);
					}
					doze_buf[2] = 0x00;
					gtp_i2c_write(i2c_connect_client, doze_buf, 3);
				} else {
					doze_buf[2] = 0x00;
					gtp_i2c_write(i2c_connect_client, doze_buf, 3);
					gtp_enter_doze(ts);
				}
			}
		}
		if (ts->use_irq) {
			gtp_irq_enable(ts);
		}
		return;
	}
#endif

	ret = gtp_i2c_read(ts->client, point_data, 12);
	if (ret < 0) {
		GTP_ERROR("I2C transfer error. errno:%d\n ", ret);
		if (ts->use_irq) {
			gtp_irq_enable(ts);
		}
		return;
	}

	finger = point_data[GTP_ADDR_LENGTH];

#if GTP_COMPATIBLE_MODE

	if ((finger == 0x00) && (CHIP_TYPE_GT9F == ts->chip_type)) {
		ret = gtp_i2c_read(ts->client, rqst_buf, 3);
		if (ret < 0) {
			GTP_ERROR("Read request status error!");
			goto exit_work_func;
		}

		switch (rqst_buf[2]) {
		case GTP_RQST_CONFIG:
			GTP_INFO("Request for config.");
			ret = gtp_send_cfg(ts->client);
			if (ret < 0) {
				GTP_ERROR("Request for config unresponded!");
			} else {
				rqst_buf[2] = GTP_RQST_RESPONDED;
				gtp_i2c_write(ts->client, rqst_buf, 3);
				GTP_INFO("Request for config responded!");
			}
			break;

		case GTP_RQST_BAK_REF:
			GTP_INFO("Request for backup reference.");
			ts->rqst_processing = 1;
			ret = gtp_bak_ref_proc(ts, GTP_BAK_REF_SEND);
			if (SUCCESS == ret) {
				rqst_buf[2] = GTP_RQST_RESPONDED;
				gtp_i2c_write(ts->client, rqst_buf, 3);
				ts->rqst_processing = 0;
				GTP_INFO("Request for backup reference responded!");
			} else {
				GTP_ERROR("Requeset for backup reference unresponed!");
			}
			break;

		case GTP_RQST_RESET:
			GTP_INFO("Request for reset.");
			gtp_recovery_reset(ts->client);
			break;

		case GTP_RQST_MAIN_CLOCK:
			GTP_INFO("Request for main clock.");
			ts->rqst_processing = 1;
			ret = gtp_main_clk_proc(ts);
			if (FAIL == ret) {
				GTP_ERROR("Request for main clock unresponded!");
			} else {
				GTP_INFO("Request for main clock responded!");
				rqst_buf[2] = GTP_RQST_RESPONDED;
				gtp_i2c_write(ts->client, rqst_buf, 3);
				ts->rqst_processing = 0;
				ts->clk_chk_fs_times = 0;
			}
			break;

		default:
			GTP_INFO("Undefined request: 0x%02X", rqst_buf[2]);
			rqst_buf[2] = GTP_RQST_RESPONDED;
			gtp_i2c_write(ts->client, rqst_buf, 3);
			break;
		}
	}
#endif
	if (finger == 0x00) {
		if (ts->use_irq) {
			gtp_irq_enable(ts);
		}
		return;
	}

	if ((finger & 0x80) == 0) {
		goto exit_work_func;
	}

	touch_num = finger & 0x0f;

	if (touch_num > GTP_MAX_TOUCH) {
		goto exit_work_func;
	}

	if (touch_num > 1) {
		u8 buf[8 * GTP_MAX_TOUCH] = {(GTP_READ_COOR_ADDR + 10) >> 8, (GTP_READ_COOR_ADDR + 10) & 0xff};

		ret = gtp_i2c_read(ts->client, buf, 2 + 8 * (touch_num - 1));
		memcpy(&point_data[12], &buf[2], 8 * (touch_num - 1));
	}

#if (GTP_HAVE_TOUCH_KEY || GTP_PEN_HAVE_BUTTON)
	key_value = point_data[3 + 8 * touch_num];

	if (key_value || pre_key) {
	#if GTP_PEN_HAVE_BUTTON
		if (key_value == 0x40) {
			GTP_DEBUG("BTN_STYLUS & BTN_STYLUS2 Down.");
			input_report_key(ts->pen_dev, BTN_STYLUS, 1);
			input_report_key(ts->pen_dev, BTN_STYLUS2, 1);
			pen_active = 1;
		} else if (key_value == 0x10) {
			GTP_DEBUG("BTN_STYLUS Down, BTN_STYLUS2 Up.");
			input_report_key(ts->pen_dev, BTN_STYLUS, 1);
			input_report_key(ts->pen_dev, BTN_STYLUS2, 0);
			pen_active = 1;
		} else if (key_value == 0x20) {
			GTP_DEBUG("BTN_STYLUS Up, BTN_STYLUS2 Down.");
			input_report_key(ts->pen_dev, BTN_STYLUS, 0);
			input_report_key(ts->pen_dev, BTN_STYLUS2, 1);
			pen_active = 1;
		} else {
			GTP_DEBUG("BTN_STYLUS & BTN_STYLUS2 Up.");
			input_report_key(ts->pen_dev, BTN_STYLUS, 0);
			input_report_key(ts->pen_dev, BTN_STYLUS2, 0);
			if ((pre_key == 0x40) || (pre_key == 0x20) ||
				(pre_key == 0x10)
			) {
				pen_active = 1;
			}
		}
		if (pen_active) {
			touch_num = 0;

		}
	#endif

	#if GTP_HAVE_TOUCH_KEY
		if (!pre_touch) {
			for (i = 0; i < GTP_MAX_KEY_NUM; i++) {
			#if GTP_DEBUG_ON
				for (ret = 0; ret < 4; ++ret) {
					if (key_codes[ret] == touch_key_array[i]) {
						GTP_DEBUG("Key: %s %s", key_names[ret], (key_value & (0x01 << i)) ? "Down" : "Up");
						break;
					}
				}
			#endif

			if (gtp_ics_slot_report == 1)
				input_report_key(ts->input_dev, touch_key_array[i], key_value & (0x01<<i));
			else {
				if (key_value & (0x01 << i)) {
					input_x = touch_key_point_maping_array[i].point_x;
					input_y = touch_key_point_maping_array[i].point_y;
					GTP_DEBUG("button =%d %d", input_x, input_y);

					gtp_touch_down(ts, 1, input_x, input_y, 100);
				}
			}
		}
		if (gtp_ics_slot_report == 0) {
			if ((pre_key != 0) && (key_value == 0))
				gtp_touch_up(ts, 1);
		}

		touch_num = 0;
		}
	#endif
	}
#endif
	pre_key = key_value;





if (gtp_ics_slot_report == 1) {

#if GTP_WITH_PEN
	if (pre_pen && (touch_num == 0)) {
		GTP_DEBUG("Pen touch UP(Slot)!");
		gtp_pen_up(0);
		pen_active = 1;
		pre_pen = 0;
	}
#endif
	if (pre_touch || touch_num) {
		s32 pos = 0;
		u16 touch_index = 0;
		u8 report_num = 0;
		coor_data = &point_data[3];

		if (touch_num) {
			id = coor_data[pos] & 0x0F;

		#if GTP_WITH_PEN
			id = coor_data[pos];
			if ((id & 0x80)) {
				GTP_DEBUG("Pen touch DOWN(Slot)!");
				input_x  = coor_data[pos + 1] | (coor_data[pos + 2] << 8);
				input_y  = coor_data[pos + 3] | (coor_data[pos + 4] << 8);
				input_w  = coor_data[pos + 5] | (coor_data[pos + 6] << 8);

				gtp_pen_down(input_x, input_y, input_w, 0);
				pre_pen = 1;
				pre_touch = 0;
				pen_active = 1;
			}
		#endif

			touch_index |= (0x01<<id);
		}

		GTP_DEBUG("id = %d, touch_index = 0x%x, pre_touch = 0x%x\n", id, touch_index, pre_touch);
		for (i = 0; i < GTP_MAX_TOUCH; i++) {
		#if GTP_WITH_PEN
			if (pre_pen == 1) {
				break;
			}
		#endif

			if ((touch_index & (0x01<<i))) {
				input_x  = coor_data[pos + 1] | (coor_data[pos + 2] << 8);
				input_y  = coor_data[pos + 3] | (coor_data[pos + 4] << 8);
				input_w  = coor_data[pos + 5] | (coor_data[pos + 6] << 8);

				gtp_touch_down(ts, id, input_x, input_y, input_w);
				pre_touch |= 0x01 << i;

				report_num++;
				if (report_num < touch_num) {
					pos += 8;
					id = coor_data[pos] & 0x0F;
					touch_index |= (0x01<<id);
				}
			} else {
				gtp_touch_up(ts, i);
				pre_touch &= ~(0x01 << i);
			}
		}
	}
} else {


	if (touch_num) {
		for (i = 0; i < touch_num; i++) {
			coor_data = &point_data[i * 8 + 3];

			id = coor_data[0] & 0x0F;
			input_x  = coor_data[1] | (coor_data[2] << 8);
			input_y  = coor_data[3] | (coor_data[4] << 8);
			input_w  = coor_data[5] | (coor_data[6] << 8);

		#if GTP_WITH_PEN
			id = coor_data[0];
			if (id & 0x80) {
				GTP_DEBUG("Pen touch DOWN!");
				gtp_pen_down(input_x, input_y, input_w, 0);
				pre_pen = 1;
				pen_active = 1;
				break;
			} else
		#endif
			{
				gtp_touch_down(ts, id, input_x, input_y, input_w);
			}
		}
	} else if (pre_touch) {
	#if GTP_WITH_PEN
		if (pre_pen == 1) {
			GTP_DEBUG("Pen touch UP!");
			gtp_pen_up(0);
			pre_pen = 0;
			pen_active = 1;
		} else
	#endif
		{
			GTP_DEBUG("Touch Release!");
			gtp_touch_up(ts, 0);
		}
	}

	pre_touch = touch_num;


}


#if GTP_WITH_PEN
	if (pen_active) {
		pen_active = 0;
		input_sync(ts->pen_dev);
	} else
#endif
	{
		input_sync(ts->input_dev);
	}

exit_work_func:
	if (!ts->gtp_rawdiff_mode) {
		ret = gtp_i2c_write(ts->client, end_cmd, 3);
		if (ret < 0) {
			GTP_INFO("I2C write end_cmd error!");
		}
	}
	if (ts->use_irq) {
		gtp_irq_enable(ts);
	}
}

/*******************************************************
Function:
	Timer interrupt service routine for polling mode.
Input:
	timer: timer struct pointer
Output:
	Timer work mode.
		HRTIMER_NORESTART: no restart mode
*********************************************************/
static enum hrtimer_restart goodix_ts_timer_handler(struct hrtimer *timer)
{
	struct goodix_ts_data *ts = container_of(timer, struct goodix_ts_data, timer);

	GTP_DEBUG_FUNC();

	queue_work(goodix_wq, &ts->work);
	hrtimer_start(&ts->timer, ktime_set(0, (GTP_POLL_TIME+6)*1000000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

/*******************************************************
Function:
	External interrupt service routine for interrupt mode.
Input:
	irq:  interrupt number.
	dev_id: private data pointer
Output:
	Handle Result.
		IRQ_HANDLED: interrupt handled successfully
*********************************************************/
static irqreturn_t goodix_ts_irq_handler(int irq, void *dev_id)
{
	struct goodix_ts_data *ts = dev_id;

	GTP_DEBUG_FUNC();

	gtp_irq_disable(ts);

	queue_work(goodix_wq, &ts->work);

	return IRQ_HANDLED;
}
/*******************************************************
Function:
	Synchronization.
Input:
	ms: synchronization time in millisecond.
Output:
	None.
*******************************************************/
void gtp_int_sync(s32 ms)
{
	GTP_GPIO_OUTPUT(gtp_int_gpio, 0);
	msleep(ms);
	GTP_GPIO_AS_INT(gtp_int_gpio);
}


/*******************************************************
Function:
	Reset chip.
Input:
	ms: reset time in millisecond
Output:
	None.
*******************************************************/
void gtp_reset_guitar(struct i2c_client *client, s32 ms)
{
#if GTP_COMPATIBLE_MODE
	struct goodix_ts_data *ts = i2c_get_clientdata(client);
#endif

	GTP_DEBUG_FUNC();
	GTP_INFO("Guitar reset");
	GTP_GPIO_OUTPUT(gtp_rst_gpio, 0);
	msleep(ms);

	GTP_GPIO_OUTPUT(gtp_int_gpio, client->addr == 0x14);

	msleep(2);
	GTP_GPIO_OUTPUT(gtp_rst_gpio, 1);

	msleep(6);

	GTP_GPIO_AS_INPUT(gtp_rst_gpio);

#if GTP_COMPATIBLE_MODE
	if (CHIP_TYPE_GT9F == ts->chip_type) {
		return;
	}
#endif

	gtp_int_sync(50);
#if GTP_ESD_PROTECT
	gtp_init_ext_watchdog(client);
#endif
}

#if GTP_GESTURE_WAKEUP
/*******************************************************
Function:
	Enter doze mode for sliding wakeup.
Input:
	ts: goodix tp private data
Output:
	1: succeed, otherwise failed
*******************************************************/
static s8 gtp_enter_doze(struct goodix_ts_data *ts)
{
	s8 ret = -1;
	s8 retry = 0;
	u8 i2c_control_buf[3] = {(u8)(GTP_REG_SLEEP >> 8), (u8)GTP_REG_SLEEP, 8};

	GTP_DEBUG_FUNC();

	GTP_DEBUG("Entering gesture mode.");
	while (retry++ < 5) {
		i2c_control_buf[0] = 0x80;
		i2c_control_buf[1] = 0x46;
		ret = gtp_i2c_write(ts->client, i2c_control_buf, 3);
		if (ret < 0) {
			GTP_DEBUG("failed to set doze flag into 0x8046, %d", retry);
			continue;
		}
		i2c_control_buf[0] = 0x80;
		i2c_control_buf[1] = 0x40;
		ret = gtp_i2c_write(ts->client, i2c_control_buf, 3);
		if (ret > 0) {
			doze_status = DOZE_ENABLED;
			GTP_INFO("Gesture mode enabled.");
			return ret;
		}
		msleep(10);
	}
	GTP_ERROR("GTP send gesture cmd failed.");
	return ret;
}
#endif
/*******************************************************
Function:
	Enter sleep mode.
Input:
	ts: private data.
Output:
	Executive outcomes.
	1: succeed, otherwise failed.
*******************************************************/
static s8 gtp_enter_sleep(struct goodix_ts_data *ts)
{
	s8 ret = -1;
	s8 retry = 0;
	u8 i2c_control_buf[3] = {(u8)(GTP_REG_SLEEP >> 8), (u8)GTP_REG_SLEEP, 5};

#if GTP_COMPATIBLE_MODE
	u8 status_buf[3] = {0x80, 0x44};
#endif

	GTP_DEBUG_FUNC();

#if GTP_COMPATIBLE_MODE
	if (CHIP_TYPE_GT9F == ts->chip_type) {

		ret = gtp_i2c_read(ts->client, status_buf, 3);
		if (ret < 0) {
			GTP_ERROR("failed to get backup-reference status");
		}

		if (status_buf[2] & 0x80) {
			ret = gtp_bak_ref_proc(ts, GTP_BAK_REF_STORE);
			if (FAIL == ret) {
				GTP_ERROR("failed to store bak_ref");
			}
		}
	}
#endif

	GTP_GPIO_OUTPUT(gtp_int_gpio, 0);
	msleep(5);

	while (retry++ < 5) {
		ret = gtp_i2c_write(ts->client, i2c_control_buf, 3);
		if (ret > 0) {
			GTP_INFO("GTP enter sleep!");

			return ret;
		}
		msleep(10);
	}
	GTP_ERROR("GTP send sleep cmd failed.");
	return ret;
}

/*******************************************************
Function:
	Wakeup from sleep.
Input:
	ts: private data.
Output:
	Executive outcomes.
		>0: succeed, otherwise: failed.
*******************************************************/
static s8 gtp_wakeup_sleep(struct goodix_ts_data *ts)
{
	u8 retry = 0;
	s8 ret = -1;

	GTP_DEBUG_FUNC();

#if GTP_COMPATIBLE_MODE
	if (CHIP_TYPE_GT9F == ts->chip_type) {
		u8 opr_buf[3] = {0x41, 0x80};

		GTP_GPIO_OUTPUT(gtp_int_gpio, 1);
		msleep(5);

		for (retry = 0; retry < 10; ++retry) {

			opr_buf[2] = 0x0C;
			ret = gtp_i2c_write(ts->client, opr_buf, 3);
			if (FAIL == ret) {
				GTP_ERROR("failed to hold ss51 & dsp!");
				continue;
			}
			opr_buf[2] = 0x00;
			ret = gtp_i2c_read(ts->client, opr_buf, 3);
			if (FAIL == ret) {
				GTP_ERROR("failed to get ss51 & dsp status!");
				continue;
			}
			if (0x0C != opr_buf[2]) {
				GTP_DEBUG("ss51 & dsp not been hold, %d", retry+1);
				continue;
			}
			GTP_DEBUG("ss51 & dsp confirmed hold");

			ret = gtp_fw_startup(ts->client);
			if (FAIL == ret) {
				GTP_ERROR("failed to startup GT9XXF, process recovery");
				gtp_esd_recovery(ts->client);
			}
			break;
		}
		if (retry >= 10) {
			GTP_ERROR("failed to wakeup, processing esd recovery");
			gtp_esd_recovery(ts->client);
		} else {
			GTP_INFO("GT9XXF gtp wakeup success");
		}
		return ret;
	}
#endif

#if GTP_POWER_CTRL_SLEEP
	while (retry++ < 5) {
		gtp_reset_guitar(ts->client, 20);

		GTP_INFO("GTP wakeup sleep.");
		return 1;
	}
#else
	while (retry++ < 10) {
	#if GTP_GESTURE_WAKEUP
	if (DOZE_WAKEUP != doze_status) {
			GTP_INFO("Powerkey wakeup.");
		} else {
			GTP_INFO("Gesture wakeup.");
		}
		doze_status = DOZE_DISABLED;
		gtp_irq_disable(ts);
		gtp_reset_guitar(ts->client, 10);
		gtp_irq_enable(ts);
	#else
		GTP_GPIO_OUTPUT(gtp_int_gpio, 1);
		msleep(5);

	#endif

		ret = gtp_i2c_test(ts->client);
		if (ret > 0) {
			GTP_INFO("GTP wakeup sleep.");

		#if (!GTP_GESTURE_WAKEUP)
			{
				gtp_int_sync(25);
			#if GTP_ESD_PROTECT
				gtp_init_ext_watchdog(ts->client);
			#endif
			}
		#endif

			return ret;
		}
		gtp_reset_guitar(ts->client, 20);
	}
#endif

	GTP_ERROR("GTP wakeup sleep failed.");
	return ret;
}

/*******************************************************
Function:
	Initialize gtp.
Input:
	ts: goodix private data
Output:
	Executive outcomes.
		0: succeed, otherwise: failed
*******************************************************/
static s32 gtp_init_panel(struct goodix_ts_data *ts)
{
	s32 ret = -1;

#if GTP_DRIVER_SEND_CFG
	s32 i = 0;
	u8 check_sum = 0;
	u8 opr_buf[16] = {0};
	u8 sensor_id = 0;
	u8 drv_cfg_version;
	u8 flash_cfg_version;

/* if defined CONFIG_OF, parse config data from dtsi
 *  else parse config data form header file.
 */

#ifdef	GTP_CONFIG_OF
	u8 cfg_info_group0[] = CTP_CFG_GROUP0;
	u8 cfg_info_group1[] = CTP_CFG_GROUP1;
	u8 cfg_info_group2[] = CTP_CFG_GROUP2;
	u8 cfg_info_group3[] = CTP_CFG_GROUP3;
	u8 cfg_info_group4[] = CTP_CFG_GROUP4;
	u8 cfg_info_group5[] = CTP_CFG_GROUP5;

	u8 *send_cfg_buf[] = {cfg_info_group0, cfg_info_group1,
						cfg_info_group2, cfg_info_group3,
						cfg_info_group4, cfg_info_group5};
	u8 cfg_info_len[] = { CFG_GROUP_LEN(cfg_info_group0),
						CFG_GROUP_LEN(cfg_info_group1),
						CFG_GROUP_LEN(cfg_info_group2),
						CFG_GROUP_LEN(cfg_info_group3),
						CFG_GROUP_LEN(cfg_info_group4),
						CFG_GROUP_LEN(cfg_info_group5)};

	GTP_DEBUG("Config Groups\' Lengths: %d, %d, %d, %d, %d, %d",
		cfg_info_len[0], cfg_info_len[1], cfg_info_len[2], cfg_info_len[3],
		cfg_info_len[4], cfg_info_len[5]);
#endif

#if GTP_COMPATIBLE_MODE
	if (CHIP_TYPE_GT9F == ts->chip_type) {
		ts->fw_error = 0;
	} else
#endif
	{	/* check firmware */
		ret = gtp_i2c_read_dbl_check(ts->client, 0x41E4, opr_buf, 1);
		if (SUCCESS == ret) {
			if (opr_buf[0] != 0xBE) {
				ts->fw_error = 1;
				GTP_ERROR("Firmware error, no config sent!");
				return -EPERM;
			}
		}
	}

	/* read sensor id */
#if GTP_COMPATIBLE_MODE
	msleep(50);
#endif
	ret = gtp_i2c_read_dbl_check(ts->client, GTP_REG_SENSOR_ID, &sensor_id, 1);
	if (SUCCESS == ret) {
		if (sensor_id >= 0x06) {
			GTP_ERROR("Invalid sensor_id(0x%02X), No Config Sent!", sensor_id);
			ts->pnl_init_error = 1;
			return -EPERM;
		}
	} else {
		GTP_ERROR("Failed to get sensor_id, No config sent!");
		ts->pnl_init_error = 1;
		return -EPERM;
	}
	GTP_INFO("Sensor_ID: %d", sensor_id);
	tp_sensor_id = sensor_id;

	/* parse config data*/

	GTP_DEBUG("Get config data from header file.");
	if ((!cfg_info_len[1]) && (!cfg_info_len[2]) &&
		(!cfg_info_len[3]) && (!cfg_info_len[4]) &&
		(!cfg_info_len[5])) {
		sensor_id = 0;
	}
	ts->gtp_cfg_len = cfg_info_len[sensor_id];
	memset(&config[GTP_ADDR_LENGTH], 0, GTP_CONFIG_MAX_LENGTH);
	memcpy(&config[GTP_ADDR_LENGTH], send_cfg_buf[sensor_id], ts->gtp_cfg_len);

	GTP_INFO("Config group%d used,length: %d", sensor_id, ts->gtp_cfg_len);

	if (ts->gtp_cfg_len < GTP_CONFIG_MIN_LENGTH) {
		GTP_ERROR("Config Group%d is INVALID CONFIG GROUP(Len: %d)! NO Config Sent! You need to check you header file CFG_GROUP section!", sensor_id, ts->gtp_cfg_len);
		ts->pnl_init_error = 1;
		return -EPERM;
	}

#if GTP_COMPATIBLE_MODE
	if (ts->chip_type != CHIP_TYPE_GT9F)
#endif
	{
		ret = gtp_i2c_read_dbl_check(ts->client, GTP_REG_CONFIG_DATA, &opr_buf[0], 1);
		if (ret == SUCCESS) {
			GTP_DEBUG("Config Version: %d, 0x%02X; IC Config Version: %d, 0x%02X",
					config[GTP_ADDR_LENGTH], config[GTP_ADDR_LENGTH], opr_buf[0], opr_buf[0]);

			flash_cfg_version = opr_buf[0];
			drv_cfg_version = config[GTP_ADDR_LENGTH];

			if (flash_cfg_version < 90 && flash_cfg_version > drv_cfg_version) {
				config[GTP_ADDR_LENGTH] = 0x00;
			}
		} else {
			GTP_ERROR("Failed to get ic config version!No config sent!");
			return -EPERM;
		}
	}

#if GTP_CUSTOM_CFG
	config[RESOLUTION_LOC]     = (u8)GTP_MAX_WIDTH;
	config[RESOLUTION_LOC + 1] = (u8)(GTP_MAX_WIDTH>>8);
	config[RESOLUTION_LOC + 2] = (u8)GTP_MAX_HEIGHT;
	config[RESOLUTION_LOC + 3] = (u8)(GTP_MAX_HEIGHT>>8);

	if (GTP_INT_TRIGGER == 0) {
		config[TRIGGER_LOC] &= 0xfe;
	} else if (GTP_INT_TRIGGER == 1) {
		config[TRIGGER_LOC] |= 0x01;
	}
#endif

	check_sum = 0;
	for (i = GTP_ADDR_LENGTH; i < ts->gtp_cfg_len; i++) {
		check_sum += config[i];
	}
	config[ts->gtp_cfg_len] = (~check_sum) + 1;

#else

	ts->gtp_cfg_len = GTP_CONFIG_MAX_LENGTH;
	ret = gtp_i2c_read(ts->client, config, ts->gtp_cfg_len + GTP_ADDR_LENGTH);
	if (ret < 0) {
		GTP_ERROR("Read Config Failed, Using Default Resolution & INT Trigger!");
		ts->abs_x_max = GTP_MAX_WIDTH;
		ts->abs_y_max = GTP_MAX_HEIGHT;
		ts->int_trigger_type = GTP_INT_TRIGGER;
	}

#endif

	if ((ts->abs_x_max == 0) && (ts->abs_y_max == 0)) {
		ts->abs_x_max = (config[RESOLUTION_LOC + 1] << 8) + config[RESOLUTION_LOC];
		ts->abs_y_max = (config[RESOLUTION_LOC + 3] << 8) + config[RESOLUTION_LOC + 2];
		ts->int_trigger_type = (config[TRIGGER_LOC]) & 0x03;
	}

#if GTP_COMPATIBLE_MODE
	if (CHIP_TYPE_GT9F == ts->chip_type) {
		u8 sensor_num = 0;
		u8 driver_num = 0;
		u8 have_key = 0;

		have_key = (config[GTP_REG_HAVE_KEY - GTP_REG_CONFIG_DATA + 2] & 0x01);

		if (1 == ts->is_950) {
			driver_num = config[GTP_REG_MATRIX_DRVNUM - GTP_REG_CONFIG_DATA + 2];
			sensor_num = config[GTP_REG_MATRIX_SENNUM - GTP_REG_CONFIG_DATA + 2];
			if (have_key) {
				driver_num--;
			}
			ts->bak_ref_len = (driver_num * (sensor_num - 1) + 2) * 2 * 6;
		} else {
			driver_num = (config[CFG_LOC_DRVA_NUM] & 0x1F) + (config[CFG_LOC_DRVB_NUM]&0x1F);
			if (have_key) {
				driver_num--;
			}
			sensor_num = (config[CFG_LOC_SENS_NUM] & 0x0F) + ((config[CFG_LOC_SENS_NUM] >> 4) & 0x0F);
			ts->bak_ref_len = (driver_num * (sensor_num - 2) + 2) * 2;
		}

		GTP_INFO("Drv * Sen: %d * %d(key: %d), X_MAX: %d, Y_MAX: %d, TRIGGER: 0x%02x",
		driver_num, sensor_num, have_key, ts->abs_x_max, ts->abs_y_max, ts->int_trigger_type);
		return 0;
	} else
#endif
	{
#if GTP_DRIVER_SEND_CFG
		ret = gtp_send_cfg(ts->client);
		if (ret < 0) {
			GTP_ERROR("Send config error.");
		}
#if GTP_COMPATIBLE_MODE
	if (ts->chip_type != CHIP_TYPE_GT9F)
#endif
	{
		if (flash_cfg_version < 90 && flash_cfg_version > drv_cfg_version) {
			check_sum = 0;
	 config[GTP_ADDR_LENGTH] = drv_cfg_version;
			for (i = GTP_ADDR_LENGTH; i < ts->gtp_cfg_len; i++) {
				check_sum += config[i];
			}
			config[ts->gtp_cfg_len] = (~check_sum) + 1;
		}
	}

#endif
		GTP_INFO("X_MAX: %d, Y_MAX: %d, TRIGGER: 0x%02x", ts->abs_x_max, ts->abs_y_max, ts->int_trigger_type);
	}

	msleep(10);
	return 0;

}

static ssize_t gt91xx_config_read_proc(struct file *file, char __user *page, size_t size, loff_t *ppos)
{
	char *ptr = page;
	char temp_data[GTP_CONFIG_MAX_LENGTH + 2] = {0x80, 0x47};
	int i;

	if (*ppos) {
		return 0;
	}
	ptr += sprintf(ptr, "==== GT9XX config init value====\n");

	for (i = 0 ; i < GTP_CONFIG_MAX_LENGTH ; i++) {
		ptr += sprintf(ptr, "0x%02X ", config[i + 2]);

		if (i % 8 == 7)
			ptr += sprintf(ptr, "\n");
	}

	ptr += sprintf(ptr, "\n");

	ptr += sprintf(ptr, "==== GT9XX config real value====\n");
	gtp_i2c_read(i2c_connect_client, temp_data, GTP_CONFIG_MAX_LENGTH + 2);
	for (i = 0 ; i < GTP_CONFIG_MAX_LENGTH ; i++) {
		ptr += sprintf(ptr, "0x%02X ", temp_data[i+2]);

		if (i % 8 == 7)
			ptr += sprintf(ptr, "\n");
	}
	*ppos += ptr - page;
	return (ptr - page);
}

static ssize_t gt91xx_config_write_proc(struct file *filp, const char __user *buffer, size_t count, loff_t *off)
{
	s32 ret = 0;

	GTP_DEBUG("write count %ld\n", count);

	if (count > GTP_CONFIG_MAX_LENGTH) {
		GTP_ERROR("size not match [%d:%ld]\n", GTP_CONFIG_MAX_LENGTH, count);
		return -EFAULT;
	}

	if (copy_from_user(&config[2], buffer, count)) {
		GTP_ERROR("copy from user fail\n");
		return -EFAULT;
	}

	ret = gtp_send_cfg(i2c_connect_client);

	if (ret < 0) {
		GTP_ERROR("send config failed.");
	}

	return count;
}
/*******************************************************
Function:
	Read chip version.
Input:
	client:  i2c device
	version: buffer to keep ic firmware version
Output:
	read operation return.
		2: succeed, otherwise: failed
*******************************************************/
s32 gtp_read_version(struct i2c_client *client, u16 *version)
{
	s32 ret = -1;
	u8 buf[8] = {GTP_REG_VERSION >> 8, GTP_REG_VERSION & 0xff};

	GTP_DEBUG_FUNC();

	ret = gtp_i2c_read(client, buf, sizeof(buf));
	if (ret < 0) {
		GTP_ERROR("GTP read version failed");
		return ret;
	}

	if (version) {
		*version = (buf[7] << 8) | buf[6];
	}
	if (buf[5] == 0x00) {
		GTP_INFO("IC Version: %c%c%c_%02x%02x", buf[2], buf[3], buf[4], buf[7], buf[6]);
	} else {
		GTP_INFO("IC Version: %c%c%c%c_%02x%02x", buf[2], buf[3], buf[4], buf[5], buf[7], buf[6]);
	}
	return ret;
}

/*******************************************************
Function:
	I2c test Function.
Input:
	client:i2c client.
Output:
	Executive outcomes.
		2: succeed, otherwise failed.
*******************************************************/
static s8 gtp_i2c_test(struct i2c_client *client)
{
	u8 test[3] = {GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff};
	u8 retry = 0;
	s8 ret = -1;

	GTP_DEBUG_FUNC();

	while (retry++ < 5) {
		ret = gtp_i2c_read(client, test, 3);
		if (ret > 0) {
			return ret;
		}
		GTP_ERROR("GTP i2c test failed time %d.", retry);
		msleep(10);
	}
	return ret;
}

/*******************************************************
Function:
	Request gpio(INT & RST) ports.
Input:
	ts: private data.
Output:
	Executive outcomes.
		>= 0: succeed, < 0: failed
*******************************************************/
static s8 gtp_request_io_port(struct goodix_ts_data *ts)
{
	s32 ret = 0;

	GTP_DEBUG_FUNC();
	ret = GTP_GPIO_REQUEST(gtp_int_gpio, "GTP INT IRQ");

	if (ret < 0) {
		GTP_ERROR("Failed to request GPIO:%d, ERRNO:%d", (s32)gtp_int_gpio, ret);
		ret = -ENODEV;
	} else {
		GTP_GPIO_AS_INT(gtp_int_gpio);
		ts->client->irq = gpio_to_irq(gtp_int_gpio);
	}

	ret = GTP_GPIO_REQUEST(gtp_rst_gpio, "GTP RST PORT");
	if (ret < 0) {
		GTP_ERROR("Failed to request GPIO:%d, ERRNO:%d", (s32)gtp_rst_gpio, ret);
		ret = -ENODEV;
	}

	GTP_GPIO_AS_INPUT(gtp_rst_gpio);

	gtp_reset_guitar(ts->client, 20);

	if (ret < 0) {
		GTP_GPIO_FREE(gtp_rst_gpio);
		GTP_GPIO_FREE(gtp_int_gpio);
	}

	return ret;
}

/*******************************************************
Function:
	Request interrupt.
Input:
	ts: private data.
Output:
	Executive outcomes.
		0: succeed, -1: failed.
*******************************************************/
static s8 gtp_request_irq(struct goodix_ts_data *ts)
{
	s32 ret = -1;
	const u8 irq_table[] = GTP_IRQ_TAB;

	GTP_DEBUG_FUNC();
	GTP_DEBUG("INT trigger type:%x", ts->int_trigger_type);

	ret  = request_irq(ts->client->irq,
					goodix_ts_irq_handler,
					irq_table[ts->int_trigger_type],
					ts->client->name,
					ts);
	if (ret) {
		GTP_ERROR("Request IRQ failed!ERRNO:%d.", ret);
		GTP_GPIO_AS_INPUT(gtp_int_gpio);
		GTP_GPIO_FREE(gtp_int_gpio);

		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = goodix_ts_timer_handler;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
		return -EPERM;
	} else {
		gtp_irq_disable(ts);
		ts->use_irq = 1;
		return 0;
	}
}

static int fts_input_event(struct input_dev *dev,
		unsigned int type, unsigned int code, int value)
{
	char buffer[16];

	if (type == EV_SYN && code == SYN_CONFIG) {
		sprintf(buffer, "%d", value);

		GTP_INFO("GTP:Gesture on/off : %d", value);
		if (value >= MXT_INPUT_EVENT_START && value <= MXT_INPUT_EVENT_END) {
			if (value == MXT_INPUT_EVENT_WAKUP_MODE_ON) {
				gtp_gesture_onoff = '1';
			} else if (value == MXT_INPUT_EVENT_WAKUP_MODE_OFF) {
				gtp_gesture_onoff = '0';
			} else {
				gtp_gesture_onoff = '0';
				GTP_ERROR("Failed Open/Close Gesture Function!\n");
				return -ENOMEM;
			}
		}
	}

	return 0;
}
/*******************************************************
Function:
	Request input device Function.
Input:
	ts:private data.
Output:
	Executive outcomes.
		0: succeed, otherwise: failed.
*******************************************************/
static s8 gtp_request_input_dev(struct goodix_ts_data *ts)
{
	s8 ret = -1;
#if GTP_HAVE_TOUCH_KEY
	u8 index = 0;
#endif

	GTP_DEBUG_FUNC();

	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		GTP_ERROR("Failed to allocate input device.");
		return -ENOMEM;
	}

	ts->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) ;


if (gtp_ics_slot_report == 1) {
	input_mt_init_slots(ts->input_dev, 10, 0);
} else {
	ts->input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
}

	__set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);

#if GTP_HAVE_TOUCH_KEY
	for (index = 0; index < GTP_MAX_KEY_NUM; index++) {
		input_set_capability(ts->input_dev, EV_KEY, touch_key_array[index]);
	}
#endif

#if GTP_GESTURE_WAKEUP
	input_set_capability(ts->input_dev, EV_KEY, KEY_POWER);
	input_set_capability(ts->input_dev, EV_KEY, KEY_WAKEUP);
	__set_bit(KEY_WAKEUP, ts->input_dev->keybit);
	Ctp_Gesture_Fucntion_Proc_File();
#endif

#if GTP_CHANGE_X2Y
	GTP_SWAP(ts->abs_x_max, ts->abs_y_max);
#endif

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, ts->abs_x_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, ts->abs_y_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, 255, 0, 0);

	ts->input_dev->name = goodix_ts_name;
	ts->input_dev->phys = goodix_input_phys;
	ts->input_dev->id.bustype = BUS_I2C;
	ts->input_dev->id.vendor = 0xDEAD;
	ts->input_dev->id.product = 0xBEEF;
	ts->input_dev->id.version = 10427;

	ts->input_dev->event = fts_input_event;


	ret = input_register_device(ts->input_dev);
	if (ret) {
		GTP_ERROR("Register %s input device failed", ts->input_dev->name);
		return -ENODEV;
	}

#if GTP_WITH_PEN
	gtp_pen_init(ts);
#endif

	return 0;
}


#if GTP_COMPATIBLE_MODE

s32 gtp_fw_startup(struct i2c_client *client)
{
	u8 opr_buf[4];
	s32 ret = 0;


	opr_buf[0] = 0xAA;
	ret = i2c_write_bytes(client, 0x8041, opr_buf, 1);
	if (ret < 0) {
		return FAIL;
	}


	opr_buf[0] = 0x00;
	ret = i2c_write_bytes(client, 0x4180, opr_buf, 1);
	if (ret < 0) {
		return FAIL;
	}

	gtp_int_sync(25);


	ret = i2c_read_bytes(client, 0x8041, opr_buf, 1);
	if (ret < 0) {
		return FAIL;
	}
	if (0xAA == opr_buf[0]) {
		GTP_ERROR("IC works abnormally,startup failed.");
		return FAIL;
	} else {
		GTP_INFO("IC works normally, Startup success.");
		opr_buf[0] = 0xAA;
		i2c_write_bytes(client, 0x8041, opr_buf, 1);
		return SUCCESS;
	}
}

static s32 gtp_esd_recovery(struct i2c_client *client)
{
	s32 retry = 0;
	s32 ret = 0;
	struct goodix_ts_data *ts;

	ts = i2c_get_clientdata(client);

	gtp_irq_disable(ts);

	GTP_INFO("GT9XXF esd recovery mode");
	for (retry = 0; retry < 5; retry++) {
		ret = gup_fw_download_proc(NULL, GTP_FL_ESD_RECOVERY);
		if (FAIL == ret) {
			GTP_ERROR("esd recovery failed %d", retry+1);
			continue;
		}
		ret = gtp_fw_startup(ts->client);
		if (FAIL == ret) {
			GTP_ERROR("GT9XXF start up failed %d", retry+1);
			continue;
		}
		break;
	}
	gtp_irq_enable(ts);

	if (retry >= 5) {
		GTP_ERROR("failed to esd recovery");
		return FAIL;
	}

	GTP_INFO("Esd recovery successful");
	return SUCCESS;
}

void gtp_recovery_reset(struct i2c_client *client)
{
#if GTP_ESD_PROTECT
	gtp_esd_switch(client, SWITCH_OFF);
#endif
	GTP_DEBUG_FUNC();

	gtp_esd_recovery(client);

#if GTP_ESD_PROTECT
	gtp_esd_switch(client, SWITCH_ON);
#endif
}

static s32 gtp_bak_ref_proc(struct goodix_ts_data *ts, u8 mode)
{
	s32 ret = 0;
	s32 i = 0;
	s32 j = 0;
	u16 ref_sum = 0;
	u16 learn_cnt = 0;
	u16 chksum = 0;
	s32 ref_seg_len = 0;
	s32 ref_grps = 0;
	struct file *ref_filp = NULL;
	u8 *p_bak_ref;

	ret = gup_check_fs_mounted("/data");
	if (FAIL == ret) {
		ts->ref_chk_fs_times++;
		GTP_DEBUG("Ref check /data times/MAX_TIMES: %d / %d", ts->ref_chk_fs_times, GTP_CHK_FS_MNT_MAX);
		if (ts->ref_chk_fs_times < GTP_CHK_FS_MNT_MAX) {
			msleep(50);
			GTP_INFO("/data not mounted.");
			return FAIL;
		}
		GTP_INFO("check /data mount timeout...");
	} else {
		GTP_INFO("/data mounted!!!(%d/%d)", ts->ref_chk_fs_times, GTP_CHK_FS_MNT_MAX);
	}

	p_bak_ref = (u8 *)kzalloc(ts->bak_ref_len, GFP_KERNEL);

	if (NULL == p_bak_ref) {
		GTP_ERROR("Allocate memory for p_bak_ref failed!");
		return FAIL;
	}

	if (ts->is_950) {
		ref_seg_len = ts->bak_ref_len / 6;
		ref_grps = 6;
	} else {
		ref_seg_len = ts->bak_ref_len;
		ref_grps = 1;
	}
	ref_filp = filp_open(GTP_BAK_REF_PATH, O_RDWR | O_CREAT, 0666);
	if (IS_ERR(ref_filp)) {
		GTP_ERROR("Failed to open/create %s.", GTP_BAK_REF_PATH);
		if (GTP_BAK_REF_SEND == mode) {
			goto bak_ref_default;
		} else {
			goto bak_ref_exit;
		}
	}

	switch (mode) {
	case GTP_BAK_REF_SEND:
		GTP_INFO("Send backup-reference");
		ref_filp->f_op->llseek(ref_filp, 0, SEEK_SET);
		ret = ref_filp->f_op->read(ref_filp, (char *)p_bak_ref, ts->bak_ref_len, &ref_filp->f_pos);
		if (ret < 0) {
			GTP_ERROR("failed to read bak_ref info from file, sending defualt bak_ref");
			goto bak_ref_default;
		}
		for (j = 0; j < ref_grps; ++j) {
			ref_sum = 0;
			for (i = 0; i < (ref_seg_len); i += 2) {
				ref_sum += (p_bak_ref[i + j * ref_seg_len] << 8) + p_bak_ref[i+1 + j * ref_seg_len];
			}
			learn_cnt = (p_bak_ref[j * ref_seg_len + ref_seg_len - 4] << 8) + (p_bak_ref[j * ref_seg_len + ref_seg_len - 3]);
			chksum = (p_bak_ref[j * ref_seg_len + ref_seg_len - 2] << 8) + (p_bak_ref[j * ref_seg_len + ref_seg_len - 1]);
			GTP_DEBUG("learn count = %d", learn_cnt);
			GTP_DEBUG("chksum = %d", chksum);
			GTP_DEBUG("ref_sum = 0x%04X", ref_sum & 0xFFFF);

			if (1 != ref_sum) {
				GTP_INFO("wrong chksum for bak_ref, reset to 0x00 bak_ref");
				memset(&p_bak_ref[j * ref_seg_len], 0, ref_seg_len);
				p_bak_ref[ref_seg_len + j * ref_seg_len - 1] = 0x01;
			} else {
				if (j == (ref_grps - 1)) {
					GTP_INFO("backup-reference data in %s used", GTP_BAK_REF_PATH);
				}
			}
		}
		ret = i2c_write_bytes(ts->client, GTP_REG_BAK_REF, p_bak_ref, ts->bak_ref_len);
		if (FAIL == ret) {
			GTP_ERROR("failed to send bak_ref because of iic comm error");
			goto bak_ref_exit;
		}
		break;

	case GTP_BAK_REF_STORE:
		GTP_INFO("Store backup-reference");
		ret = i2c_read_bytes(ts->client, GTP_REG_BAK_REF, p_bak_ref, ts->bak_ref_len);
		if (ret < 0) {
			GTP_ERROR("failed to read bak_ref info, sending default back-reference");
			goto bak_ref_default;
		}
		ref_filp->f_op->llseek(ref_filp, 0, SEEK_SET);
		ref_filp->f_op->write(ref_filp, (char *)p_bak_ref, ts->bak_ref_len, &ref_filp->f_pos);
		break;

	default:
		GTP_ERROR("invalid backup-reference request");
		break;
	}
	ret = SUCCESS;
	goto bak_ref_exit;

bak_ref_default:

	for (j = 0; j < ref_grps; ++j) {
		memset(&p_bak_ref[j * ref_seg_len], 0, ref_seg_len);
		p_bak_ref[j * ref_seg_len + ref_seg_len - 1] = 0x01;
	}
	ret = i2c_write_bytes(ts->client, GTP_REG_BAK_REF, p_bak_ref, ts->bak_ref_len);
	if (!IS_ERR(ref_filp)) {
		GTP_INFO("write backup-reference data into %s", GTP_BAK_REF_PATH);
		ref_filp->f_op->llseek(ref_filp, 0, SEEK_SET);
		ref_filp->f_op->write(ref_filp, (char *)p_bak_ref, ts->bak_ref_len, &ref_filp->f_pos);
	}
	if (ret == FAIL) {
		GTP_ERROR("failed to load the default backup reference");
	}

bak_ref_exit:

	if (p_bak_ref) {
		kfree(p_bak_ref);
	}
	if (ref_filp && !IS_ERR(ref_filp)) {
		filp_close(ref_filp, NULL);
	}
	return ret;
}


static s32 gtp_verify_main_clk(u8 *p_main_clk)
{
	u8 chksum = 0;
	u8 main_clock = p_main_clk[0];
	s32 i = 0;

	if (main_clock < 50 || main_clock > 120) {
		return FAIL;
	}

	for (i = 0; i < 5; ++i) {
		if (main_clock != p_main_clk[i]) {
			return FAIL;
		}
		chksum += p_main_clk[i];
	}
	chksum += p_main_clk[5];
	if ((chksum) == 0) {
		return SUCCESS;
	} else {
		return FAIL;
	}
}

static s32 gtp_main_clk_proc(struct goodix_ts_data *ts)
{
	s32 ret = 0;
	s32 i = 0;
	s32 clk_chksum = 0;
	struct file *clk_filp = NULL;
	u8 p_main_clk[6] = {0};

	ret = gup_check_fs_mounted("/data");
	if (FAIL == ret) {
		ts->clk_chk_fs_times++;
		GTP_DEBUG("Clock check /data times/MAX_TIMES: %d / %d", ts->clk_chk_fs_times, GTP_CHK_FS_MNT_MAX);
		if (ts->clk_chk_fs_times < GTP_CHK_FS_MNT_MAX) {
			msleep(50);
			GTP_INFO("/data not mounted.");
			return FAIL;
		}
		GTP_INFO("Check /data mount timeout!");
	} else {
		GTP_INFO("/data mounted!!!(%d/%d)", ts->clk_chk_fs_times, GTP_CHK_FS_MNT_MAX);
	}

	clk_filp = filp_open(GTP_MAIN_CLK_PATH, O_RDWR | O_CREAT, 0666);
	if (IS_ERR(clk_filp)) {
		GTP_ERROR("%s is unavailable, calculate main clock", GTP_MAIN_CLK_PATH);
	} else {
		clk_filp->f_op->llseek(clk_filp, 0, SEEK_SET);
		clk_filp->f_op->read(clk_filp, (char *)p_main_clk, 6, &clk_filp->f_pos);

		ret = gtp_verify_main_clk(p_main_clk);
		if (FAIL == ret) {

			GTP_ERROR("main clock data in %s is wrong, recalculate main clock", GTP_MAIN_CLK_PATH);
		} else {
			GTP_INFO("main clock data in %s used, main clock freq: %d", GTP_MAIN_CLK_PATH, p_main_clk[0]);
			filp_close(clk_filp, NULL);
			goto update_main_clk;
		}
	}

#if GTP_ESD_PROTECT
	gtp_esd_switch(ts->client, SWITCH_OFF);
#endif
	ret = gup_clk_calibration();
	gtp_esd_recovery(ts->client);

#if GTP_ESD_PROTECT
	gtp_esd_switch(ts->client, SWITCH_ON);
#endif

	GTP_INFO("calibrate main clock: %d", ret);
	if (ret < 50 || ret > 120) {
		GTP_ERROR("wrong main clock: %d", ret);
		goto exit_main_clk;
	}


	for (i = 0; i < 5; ++i) {
		p_main_clk[i] = ret;
		clk_chksum += p_main_clk[i];
	}
	p_main_clk[5] = 0 - clk_chksum;

	if (!IS_ERR(clk_filp)) {
		GTP_DEBUG("write main clock data into %s", GTP_MAIN_CLK_PATH);
		clk_filp->f_op->llseek(clk_filp, 0, SEEK_SET);
		clk_filp->f_op->write(clk_filp, (char *)p_main_clk, 6, &clk_filp->f_pos);
		filp_close(clk_filp, NULL);
	}

update_main_clk:
	ret = i2c_write_bytes(ts->client, GTP_REG_MAIN_CLK, p_main_clk, 6);
	if (FAIL == ret) {
		GTP_ERROR("update main clock failed!");
		return FAIL;
	}
	return SUCCESS;

exit_main_clk:
	if (!IS_ERR(clk_filp))
		filp_close(clk_filp, NULL);
	return FAIL;
}


s32 gtp_gt9xxf_init(struct i2c_client *client)
{
	s32 ret = 0;

	ret = gup_fw_download_proc(NULL, GTP_FL_FW_BURN);
	if (FAIL == ret)
		return FAIL;

	ret = gtp_fw_startup(client);
	if (FAIL == ret)
		return FAIL;
	return SUCCESS;
}

void gtp_get_chip_type(struct goodix_ts_data *ts)
{
	u8 opr_buf[10] = {0x00};
	s32 ret = 0;

	msleep(10);

	ret = gtp_i2c_read_dbl_check(ts->client, GTP_REG_CHIP_TYPE, opr_buf, 10);

	if (FAIL == ret) {
		GTP_ERROR("Failed to get chip-type, set chip type default: GOODIX_GT9");
		ts->chip_type = CHIP_TYPE_GT9;
		return;
	}

	if (!memcmp(opr_buf, "GOODIX_GT9", 10)) {
		ts->chip_type = CHIP_TYPE_GT9;
	} else {
		ts->chip_type = CHIP_TYPE_GT9F;
	}
	GTP_INFO("Chip Type: %s", (ts->chip_type == CHIP_TYPE_GT9) ? "GOODIX_GT9" : "GOODIX_GT9F");
}
#endif

/*
 * Devices Tree support,
*/
#ifdef GTP_CONFIG_OF
/**
 * gtp_parse_dt - parse platform infomation form devices tree.
 */
static void gtp_parse_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;

	gtp_int_gpio = of_get_named_gpio(np, "goodix,irq-gpio", 0);
	gtp_rst_gpio = of_get_named_gpio(np, "goodix,rst-gpio", 0);

	/********************** add 2016.4.28 **************/


}

/**
 * gtp_parse_dt_cfg - parse config data from devices tree.
 * @dev: device that this driver attached.
 * @cfg: pointer of the config array.
 * @cfg_len: pointer of the config length.
 * @sid: sensor id.
 * Return: 0-succeed, -1-faileds
 */
int gtp_parse_dt_cfg(struct device *dev, u8 *cfg, int *cfg_len, u8 sid)
{
	struct device_node *np = dev->of_node;
	struct property *prop;
	char cfg_name[18];

	snprintf(cfg_name, sizeof(cfg_name), "goodix,cfg-group%d", sid);
	prop = of_find_property(np, cfg_name, cfg_len);
	if (!prop || !prop->value || *cfg_len == 0 || *cfg_len > GTP_CONFIG_MAX_LENGTH) {
		return -EPERM;/* failed */
	} else {
		memcpy(cfg, prop->value, *cfg_len);
		return 0;
	}
}

/**
 * gtp_power_switch - power switch .
 * @on: 1-switch on, 0-switch off.
 * return: 0-succeed, -1-faileds
 */
static int gtp_power_switch(struct i2c_client *client, int on)
{
	static struct regulator *vdd_ana;
	static struct regulator *vcc_i2c;
	int ret;

	if (!vdd_ana) {
		vdd_ana = regulator_get(&client->dev, "vdd_ana");
		if (IS_ERR(vdd_ana)) {
			GTP_ERROR("regulator get of vdd_ana failed");
			ret = PTR_ERR(vdd_ana);
			vdd_ana = NULL;
			return ret;
		}
	}

	if (!vcc_i2c) {
		vcc_i2c = regulator_get(&client->dev, "vcc_i2c");
		if (IS_ERR(vcc_i2c)) {
			GTP_ERROR("regulator get of vcc_i2c failed");
			ret = PTR_ERR(vcc_i2c);
			vcc_i2c = NULL;
			goto ERR_GET_VCC;
		}
	}

	if (on) {
		GTP_DEBUG("GTP power on.");
		ret = regulator_enable(vdd_ana);
		udelay(2);
		ret = regulator_enable(vcc_i2c);
	} else {
		GTP_DEBUG("GTP power off.");
		ret = regulator_disable(vcc_i2c);
		udelay(2);
		ret = regulator_disable(vdd_ana);
	}
	return ret;

ERR_GET_VCC:
	regulator_put(vdd_ana);
	return ret;
}
#endif

#ifdef PMX_DRIVER_GT915L
static int goodix_ts_pinctrl_init(struct goodix_ts_data *goodix_data)
{
	int retval;

	goodix_data->ts_pinctrl = devm_pinctrl_get(&(goodix_data->client->dev));
	if (IS_ERR_OR_NULL(goodix_data->ts_pinctrl)) {
		retval = PTR_ERR(goodix_data->ts_pinctrl);
		dev_dbg(&goodix_data->client->dev, "Target does not use pinctrl %d\n", retval);
		goto err_pinctrl_get;
	}
	goodix_data->pinctrl_state_active = pinctrl_lookup_state(goodix_data->ts_pinctrl, PINCTRL_STATE_ACTIVE);

	if (IS_ERR_OR_NULL(goodix_data->pinctrl_state_active)) {
		retval = PTR_ERR(goodix_data->pinctrl_state_active);
		dev_err(&goodix_data->client->dev, "Can not lookup %s pinstate %d\n", PINCTRL_STATE_ACTIVE, retval);
		goto err_pinctrl_lookup;
	}

	goodix_data->pinctrl_state_suspend = pinctrl_lookup_state(goodix_data->ts_pinctrl, PINCTRL_STATE_SUSPEND);

	if (IS_ERR_OR_NULL(goodix_data->pinctrl_state_suspend)) {
		retval = PTR_ERR(goodix_data->pinctrl_state_suspend);
		dev_err(&goodix_data->client->dev, "Can not lookup %s pinstate %d\n", PINCTRL_STATE_SUSPEND, retval);
		goto err_pinctrl_lookup;
	}

	goodix_data->pinctrl_state_release = pinctrl_lookup_state(goodix_data->ts_pinctrl, PINCTRL_STATE_RELEASE);

	if (IS_ERR_OR_NULL(goodix_data->pinctrl_state_release)) {
		retval = PTR_ERR(goodix_data->pinctrl_state_release);
		dev_dbg(&goodix_data->client->dev, "Can not lookup %s pinstate %d\n", PINCTRL_STATE_RELEASE, retval);
	}
	return 0;

err_pinctrl_lookup:
	devm_pinctrl_put(goodix_data->ts_pinctrl);
err_pinctrl_get:
	goodix_data->ts_pinctrl = NULL;
	return retval;
}


static void goodix_ts_pinctrl_free(struct goodix_ts_data *goodix_data)
{
	/* Put pinctrl if target uses pinctrl */
	if (!IS_ERR_OR_NULL(goodix_data->ts_pinctrl)) {
		devm_pinctrl_put(goodix_data->ts_pinctrl);
		goodix_data->ts_pinctrl = NULL;
	}
}

static int goodix_ts_pinctrl_select(struct goodix_ts_data *goodix_data, bool on)
{
	struct pinctrl_state *pins_state;
	int ret;

	pins_state = on ? goodix_data->pinctrl_state_active
				: goodix_data->pinctrl_state_suspend;
	if (!IS_ERR_OR_NULL(pins_state)) {
		ret = pinctrl_select_state(goodix_data->ts_pinctrl, pins_state);
		if (ret) {
			dev_err(&goodix_data->client->dev,
					"can not set %s pins\n",
					on ? "pmx_ts_active" : "pmx_ts_suspend");
			return ret;
		}
	} else {
		dev_err(&goodix_data->client->dev,
				"not a valid '%s' pinstate\n",
				on ? "pmx_ts_active" : "pmx_ts_suspend");
	}

	return 0;
}

#endif


void parse_cmdline_for_gt915(void)
{
	int ret;
	char *cmdline_tp;
	char *temp;

	cmdline_tp = strstr(saved_command_line, "androidboot.mode=");
	if (cmdline_tp != NULL) {
		temp = cmdline_tp + strlen("androidboot.mode=");
		ret = strncmp(temp, "ffbm", strlen("ffbm"));
		if (ret == 0) {
			gtp_ics_slot_report = 1;/* factory mode*/
			return;
		}
	}
	gtp_ics_slot_report = 0;
	return;
}


static int get_boot_mode(struct i2c_client *client)
{
	int ret;
	char *cmdline_tp = NULL;
	char *temp;
	char cmd_line[15] = {'\0'};

	cmdline_tp = strstr(saved_command_line, "androidboot.mode=");
	if (cmdline_tp != NULL) {
		temp = cmdline_tp + strlen("androidboot.mode=");
		ret = strncmp(temp, "ffbm", strlen("ffbm"));
		memcpy(cmd_line, temp, 10);
		dev_err(&client->dev, "cmd_line =%s \n", cmd_line);
		if (ret == 0) {
			dev_err(&client->dev, "mode: ffbm\n");
			return 1;/* factory mode*/
		} else {
			dev_err(&client->dev, "mode: no ffbm\n");
			return 2;/* no factory mode*/
		}
	}
	dev_err(&client->dev, "has no androidboot.mode \n");
	return 0;
}


static int gtp_read_lockdown_info(void)
{
	int ret, k;
	char buf[2+GTP_LOCKDOWN_SIZE] = {GTP_READ_LOCKDOWN_INFO_ADDR >> 8, GTP_READ_LOCKDOWN_INFO_ADDR & 0xFF};

		ret = gtp_i2c_read(i2c_connect_client, buf, GTP_LOCKDOWN_SIZE+2);
		if (ret < 0) {
			printk("Read Lockdown info error!");
			return -EPERM;
	}

		for (k = 0; k < GTP_LOCKDOWN_SIZE; k++) {
			lockdown_info[k] = buf[2+k];
		}

	TP_Maker = lockdown_info[0];
	LCD_Maker = lockdown_info[1];
	Panel_Ink = lockdown_info[2];
	tp_color =  lockdown_info[2];
	GTP_INFO("Lockdown info: %02X %02X %02X %02X %02X %02X %02X %02X",
				lockdown_info[0], lockdown_info[1],
				lockdown_info[2], lockdown_info[3],
				lockdown_info[4], lockdown_info[5],
				lockdown_info[6], lockdown_info[7]);

	GTP_INFO("WT: TP_Maker = 0x%x\n", TP_Maker);
	GTP_INFO("WT: LCD_Maker = 0x%x\n", LCD_Maker);
	GTP_INFO("WT: Panel_Ink = 0x%x\n", Panel_Ink);

	return 0;
}

int gtp_hardwareinfo_set(void)
{
	char color[HARDWARE_MAX_ITEM_LONGTH];
	char vendor[HARDWARE_MAX_ITEM_LONGTH];
	int ret;
	u16 ic_ver;
		gtp_read_lockdown_info();
		switch (tp_color) {
		case TP_White:
			snprintf(color, HARDWARE_MAX_ITEM_LONGTH, "White");
			break;
		case TP_Black:
			snprintf(color, HARDWARE_MAX_ITEM_LONGTH, "Black");
			break;
		case TP_Golden:
			snprintf(color, HARDWARE_MAX_ITEM_LONGTH, "Golden");
			break;
		default:
			snprintf(color, HARDWARE_MAX_ITEM_LONGTH, "Other Color");
	}

	switch (TP_Maker) {
	case TP_BIEL:
		snprintf(vendor, HARDWARE_MAX_ITEM_LONGTH, "BIEL");
		break;
	default:
		snprintf(vendor, HARDWARE_MAX_ITEM_LONGTH, "Other Vendor");
	}

	ret = gtp_i2c_read_dbl_check(i2c_client_point, GTP_REG_CONFIG_DATA, &cfg_version, 1);
	if (ret == FAIL) {
		printk("Read IC Config Version Error\n");
		return -EPERM;
	}

	ret = gtp_read_version(i2c_client_point, &ic_ver);
	if (ret < 0) {
			GTP_ERROR("Read version failed.");
		return -EPERM;
		}
	IC_Version = ic_ver;

	printk("IC Config Version: %x, V%d\n", IC_Version, cfg_version);


	sprintf(tp_string_version, "%s, GT917D_%x, V%d, %s", vendor, IC_Version, cfg_version, color);
	hardwareinfo_set_prop(HARDWARE_TP, tp_string_version);
	return 0;
}

/*******************************************************
Function:
	I2c probe.
Input:
	client: i2c device struct.
	id: device id.
Output:
	Executive outcomes.
		0: succeed.
*******************************************************/
static int goodix_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	s32 ret = -1;
	struct goodix_ts_data *ts;
	u16 version_info;
#ifdef PMX_DRIVER_GT915L
	int err;
#endif

	GTP_DEBUG_FUNC();


	parse_cmdline_for_gt915();


	GTP_INFO("GTP Driver Version: %s", GTP_DRIVER_VERSION);

	GTP_INFO("GTP I2C Address: 0x%02x", client->addr);

	i2c_connect_client = client;
	i2c_client_point = client;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		GTP_ERROR("I2C check functionality failed.");
		return -ENODEV;
	}
	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (ts == NULL) {
		GTP_ERROR("Alloc GFP_KERNEL memory failed.");
		return -ENOMEM;
	}

	ts->client = client;

#ifdef GTP_CONFIG_OF	/* device tree support */
	if (client->dev.of_node) {
		gtp_parse_dt(&client->dev);
	}
	ret = gtp_power_switch(client, 1);
	if (ret) {
		GTP_ERROR("GTP power on failed.");
		return -EINVAL;
	}
#else			/* use gpio defined in gt9xx.h */
	gtp_rst_gpio = GTP_RST_PORT;
	gtp_int_gpio = GTP_INT_PORT;
#endif

#ifdef PMX_DRIVER_GT915L
	err = goodix_ts_pinctrl_init(ts);
	if (!err && ts->ts_pinctrl) {
	ret = goodix_ts_pinctrl_select(ts, 1);
	if (ret < 0) {
		dev_err(&client->dev, "can not get idle pin state\n");
		goto gtp_power_off;
	}
}
 #endif

	INIT_WORK(&ts->work, goodix_ts_work_func);
	ts->client = client;
	spin_lock_init(&ts->irq_lock);

#if GTP_ESD_PROTECT
	ts->clk_tick_cnt = 2 * HZ;
	GTP_DEBUG("Clock ticks for an esd cycle: %d", ts->clk_tick_cnt);
	spin_lock_init(&ts->esd_lock);

#endif
	i2c_set_clientdata(client, ts);
	ts->gtp_rawdiff_mode = 0;
	ret = gtp_request_io_port(ts);
	if (ret < 0) {
		GTP_ERROR("GTP request IO port failed.");
		kfree(ts);
		return ret;
	}

#if GTP_COMPATIBLE_MODE
	gtp_get_chip_type(ts);
	if (CHIP_TYPE_GT9F == ts->chip_type) {
		ret = gtp_gt9xxf_init(ts->client);
		if (FAIL == ret) {
			GTP_INFO("Failed to init GT9XXF.");
		}
	}
#endif

#ifdef WT_COMPILE_FACTORY_VERSION
	ret = gtp_read_version(client, &version_info);
	if (ret < 0) {
		GTP_ERROR("Read version failed.");
	goto free_reset_irq_gpio;
	}

	IC_Version = version_info;

	ret = gtp_i2c_test(client);
	if (ret < 0) {
		GTP_ERROR("I2C communication ERROR!");
		goto free_reset_irq_gpio;
	}
#else
	ret = gtp_i2c_test(client);
	if (ret < 0) {
		GTP_ERROR("I2C communication ERROR!");
	goto free_reset_irq_gpio;
	}

	ret = gtp_read_version(client, &version_info);
	if (ret < 0) {
		GTP_ERROR("Read version failed.");
	goto free_reset_irq_gpio;
	}

	IC_Version = version_info;
#endif

	ret = gtp_init_panel(ts);
	if (ret < 0) {
		GTP_ERROR("GTP init panel failed.");
		ts->abs_x_max = GTP_MAX_WIDTH;
		ts->abs_y_max = GTP_MAX_HEIGHT;
		ts->int_trigger_type = GTP_INT_TRIGGER;
	}


	gt91xx_config_proc = proc_create(GT91XX_CONFIG_PROC_FILE, 0666, NULL, &config_proc_ops);
	if (gt91xx_config_proc == NULL) {
		GTP_ERROR("create_proc_entry %s failed\n", GT91XX_CONFIG_PROC_FILE);
	} else {
		GTP_INFO("create proc entry %s success", GT91XX_CONFIG_PROC_FILE);
	}

#if GTP_ESD_PROTECT
	gtp_esd_switch(client, SWITCH_ON);
#endif

#if GTP_AUTO_UPDATE

err = get_boot_mode(client);
if (err == 0) {
	ret = gup_init_update_proc(ts);
	if (ret < 0) {
		GTP_ERROR("Create update thread error.");
	}
} else {
	GTP_INFO("not nomal_boot\n");
}
#endif

	ret = gtp_request_input_dev(ts);
	if (ret < 0) {
		GTP_ERROR("GTP request input dev failed");
	}

	ret = gtp_request_irq(ts);
	if (ret < 0) {
		GTP_INFO("GTP works in polling mode.");
	} else {
		GTP_INFO("GTP works in interrupt mode.");
	}

	if (ts->use_irq) {
		gtp_irq_enable(ts);
#if GTP_GESTURE_WAKEUP
	enable_irq_wake(client->irq);
#endif
	}

#if WT_ADD_CTP_INFO
	gtp_hardwareinfo_set();
#endif


#ifdef CONFIG_TOUCHSCREEN_LOCKDOWN_INFO
	ctp_lockdown_fucntion_proc_file();
#endif


	/* register suspend and resume fucntion*/
	gtp_register_powermanger(ts);

#if GTP_CREATE_WR_NODE
	init_wr_node(client);
#endif
	return 0;


free_reset_irq_gpio:
	if (gpio_is_valid(gtp_rst_gpio))
		GTP_GPIO_FREE(gtp_rst_gpio);
	if (ts->ts_pinctrl) {
		err = goodix_ts_pinctrl_select(ts, false);
		if (err < 0)
			GTP_ERROR("Cannot get idle pinctrl state\n");
	}

	if (gpio_is_valid(gtp_int_gpio))
	GTP_GPIO_FREE(gtp_int_gpio);
	if (ts->ts_pinctrl) {
		err = goodix_ts_pinctrl_select(ts, false);
		if (err < 0)
			GTP_ERROR("Cannot get idle pinctrl state\n");
	}

	goodix_ts_pinctrl_free(ts);

gtp_power_off:
	gtp_power_switch(client, 0);

	return err;
}


/*******************************************************
Function:
	Goodix touchscreen driver release function.
Input:
	client: i2c device struct.
Output:
	Executive outcomes. 0---succeed.
*******************************************************/
static int goodix_ts_remove(struct i2c_client *client)
{
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	GTP_DEBUG_FUNC();

	gtp_unregister_powermanger(ts);

#if GTP_CREATE_WR_NODE
	uninit_wr_node();
#endif

#if GTP_ESD_PROTECT
	destroy_workqueue(gtp_esd_check_workqueue);
#endif

	if (ts) {
		if (ts->use_irq) {
			GTP_GPIO_AS_INPUT(gtp_int_gpio);
			GTP_GPIO_FREE(gtp_int_gpio);
			free_irq(client->irq, ts);
		} else {
			hrtimer_cancel(&ts->timer);
		}
	}

	GTP_INFO("GTP driver removing...");
	i2c_set_clientdata(client, NULL);
	input_unregister_device(ts->input_dev);
	kfree(ts);

	return 0;
}


/*******************************************************
Function:
	Early suspend function.
Input:
	h: early_suspend struct.
Output:
	None.
*******************************************************/
static void goodix_ts_suspend(struct goodix_ts_data *ts)
{
	s8 ret = -1;

	GTP_DEBUG_FUNC();
	if (ts->enter_update) {
		return;
	}
	GTP_INFO("System suspend.");

	ts->gtp_is_suspend = 1;
#if GTP_ESD_PROTECT
	gtp_esd_switch(ts->client, SWITCH_OFF);
#endif


#if GTP_GESTURE_WAKEUP
GTP_INFO("Gesture gtp_gesture_onoff :%c\n", gtp_gesture_onoff);
if ('1' == gtp_gesture_onoff)
	ret = gtp_enter_doze(ts);
else
#endif
{
	if (ts->use_irq) {
		gtp_irq_disable(ts);
	} else {
		hrtimer_cancel(&ts->timer);
	}
	ret = gtp_enter_sleep(ts);
}

	if (ret < 0) {
		GTP_ERROR("GTP early suspend failed.");
	}


	msleep(58);
}

/*******************************************************
Function:
	Late resume function.
Input:
	h: early_suspend struct.
Output:
	None.
*******************************************************/
static void goodix_ts_resume(struct goodix_ts_data *ts)
{
	s8 ret = -1;

	GTP_DEBUG_FUNC();
	if (ts->enter_update) {
		return;
	}
	GTP_INFO("System resume.");

	ret = gtp_wakeup_sleep(ts);

#if GTP_GESTURE_WAKEUP
	doze_status = DOZE_DISABLED;
#endif

	if (ret < 0) {
		GTP_ERROR("GTP later resume failed.");
	}
#if (GTP_COMPATIBLE_MODE)
	if (CHIP_TYPE_GT9F == ts->chip_type) {

	} else
#endif
	{
		gtp_send_cfg(ts->client);
	}

	if (ts->use_irq) {
		GTP_DEBUG("GTP use_irq.");
		gtp_irq_enable(ts);
	} else {
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	}

	ts->gtp_is_suspend = 0;
#if GTP_ESD_PROTECT
	gtp_esd_switch(ts->client, SWITCH_ON);
#endif
}


#if   defined(CONFIG_FB)
/* frame buffer notifier block control the suspend/resume procedure */
static int gtp_fb_notifier_callback(struct notifier_block *noti, unsigned long event, void *data)
{
	struct fb_event *ev_data = data;
	struct goodix_ts_data *ts = container_of(noti, struct goodix_ts_data, notifier);
	int *blank;

	if (ev_data && ev_data->data && event == FB_EVENT_BLANK && ts) {
		blank = ev_data->data;
		if (*blank == FB_BLANK_UNBLANK) {
			GTP_DEBUG("Resume by fb notifier.");
			goodix_ts_resume(ts);

		} else if (*blank == FB_BLANK_POWERDOWN) {
			GTP_DEBUG("Suspend by fb notifier.");
			goodix_ts_suspend(ts);
		}
	}

	return 0;
}
#elif defined(CONFIG_PM)
/* bus control the suspend/resume procedure */
static int gtp_pm_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	if (ts) {
		GTP_DEBUG("Suspend by i2c pm.");
		goodix_ts_suspend(ts);
	}

	return 0;
}
static int gtp_pm_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	if (ts) {
		GTP_DEBUG("Resume by i2c pm.");
		goodix_ts_resume(ts);
	}

	return 0;
}

static struct dev_pm_ops gtp_pm_ops = {
	.suspend = gtp_pm_suspend,
	.resume  = gtp_pm_resume,
};

#elif defined(CONFIG_HAS_EARLYSUSPEND)
/* earlysuspend module the suspend/resume procedure */
static void gtp_early_suspend(struct early_suspend *h)
{
	struct goodix_ts_data *ts = container_of(h, struct goodix_ts_data, early_suspend);

	if (ts) {
		GTP_DEBUG("Suspend by earlysuspend module.");
		goodix_ts_suspend(ts);
	}
}
static void gtp_early_resume(struct early_suspend *h)
{
	struct goodix_ts_data *ts = container_of(h, struct goodix_ts_data, early_suspend);

	if (ts) {
		GTP_DEBUG("Resume by earlysuspend module.");
		goodix_ts_resume(ts);
	}
}
#endif

static int gtp_register_powermanger(struct goodix_ts_data *ts)
{
#if   defined(CONFIG_FB)
	ts->notifier.notifier_call = gtp_fb_notifier_callback;
	fb_register_client(&ts->notifier);

#elif defined(CONFIG_HAS_EARLYSUSPEND)
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = goodix_ts_early_suspend;
	ts->early_suspend.resume = goodix_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	return 0;
}

static int gtp_unregister_powermanger(struct goodix_ts_data *ts)
{
#if   defined(CONFIG_FB)
		fb_unregister_client(&ts->notifier);

#elif defined(CONFIG_HAS_EARLYSUSPEND)
		unregister_early_suspend(&ts->early_suspend);
#endif
	return 0;
}

/* end */

#if GTP_ESD_PROTECT
s32 gtp_i2c_read_no_rst(struct i2c_client *client, u8 *buf, s32 len)
{
	struct i2c_msg msgs[2];
	s32 ret = -1;
	s32 retries = 0;

	GTP_DEBUG_FUNC();

	msgs[0].flags = !I2C_M_RD;
	msgs[0].addr  = client->addr;
	msgs[0].len   = GTP_ADDR_LENGTH;
	msgs[0].buf   = &buf[0];


	msgs[1].flags = I2C_M_RD;
	msgs[1].addr  = client->addr;
	msgs[1].len   = len - GTP_ADDR_LENGTH;
	msgs[1].buf   = &buf[GTP_ADDR_LENGTH];


	while (retries < 5) {
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret == 2)
			break;
		retries++;
	}
	if ((retries >= 5)) {
		GTP_ERROR("I2C Read: 0x%04X, %d bytes failed, errcode: %d!", (((u16)(buf[0] << 8)) | buf[1]), len-2, ret);
	}
	return ret;
}

s32 gtp_i2c_write_no_rst(struct i2c_client *client, u8 *buf, s32 len)
{
	struct i2c_msg msg;
	s32 ret = -1;
	s32 retries = 0;

	GTP_DEBUG_FUNC();

	msg.flags = !I2C_M_RD;
	msg.addr  = client->addr;
	msg.len   = len;
	msg.buf   = buf;


	while (retries < 5) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret == 1)
			break;
		retries++;
	}
	if ((retries >= 5)) {
		GTP_ERROR("I2C Write: 0x%04X, %d bytes failed, errcode: %d!", (((u16)(buf[0] << 8)) | buf[1]), len-2, ret);
	}
	return ret;
}
/*******************************************************
Function:
	switch on & off esd delayed work
Input:
	client:  i2c device
	on:      SWITCH_ON / SWITCH_OFF
Output:
	void
*********************************************************/
void gtp_esd_switch(struct i2c_client *client, s32 on)
{
	struct goodix_ts_data *ts;

	ts = i2c_get_clientdata(client);
	spin_lock(&ts->esd_lock);

	if (SWITCH_ON == on) {
		if (!ts->esd_running) {
			ts->esd_running = 1;
			spin_unlock(&ts->esd_lock);
			GTP_INFO("Esd started");
			queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work, ts->clk_tick_cnt);
		} else {
			spin_unlock(&ts->esd_lock);
		}
	} else {
		if (ts->esd_running) {
			ts->esd_running = 0;
			spin_unlock(&ts->esd_lock);
			GTP_INFO("Esd cancelled");
			cancel_delayed_work_sync(&gtp_esd_check_work);
		} else {
			spin_unlock(&ts->esd_lock);
		}
	}
}

/*******************************************************
Function:
	Initialize external watchdog for esd protect
Input:
	client:  i2c device.
Output:
	result of i2c write operation.
		1: succeed, otherwise: failed
*********************************************************/
static s32 gtp_init_ext_watchdog(struct i2c_client *client)
{
	u8 opr_buffer[3] = {0x80, 0x41, 0xAA};
	GTP_DEBUG("[Esd]Init external watchdog");
	return gtp_i2c_write_no_rst(client, opr_buffer, 3);
}

/*******************************************************
Function:
	Esd protect function.
	External watchdog added by meta, 2013/03/07
Input:
	work: delayed work
Output:
	None.
*******************************************************/
static void gtp_esd_check_func(struct work_struct *work)
{
	s32 i;
	s32 ret = -1;
	struct goodix_ts_data *ts = NULL;
	u8 esd_buf[5] = {0x80, 0x40};

	GTP_DEBUG_FUNC();

	ts = i2c_get_clientdata(i2c_connect_client);

	if (ts->gtp_is_suspend || ts->enter_update) {
		GTP_INFO("Esd suspended!");
		return;
	}

	for (i = 0; i < 3; i++) {
		ret = gtp_i2c_read_no_rst(ts->client, esd_buf, 4);

		GTP_DEBUG("[Esd]0x8040 = 0x%02X, 0x8041 = 0x%02X", esd_buf[2], esd_buf[3]);
		if ((ret < 0)) {
			continue;
		} else {
			if ((esd_buf[2] == 0xAA) || (esd_buf[3] != 0xAA)) {
				u8 chk_buf[4] = {0x80, 0x40};
				gtp_i2c_read_no_rst(ts->client, chk_buf, 4);
				GTP_DEBUG("[Check]0x8040 = 0x%02X, 0x8041 = 0x%02X", chk_buf[2], chk_buf[3]);
				if ((chk_buf[2] == 0xAA) || (chk_buf[3] != 0xAA)) {
					i = 3;
					break;
				} else {
					continue;
				}
			} else {

				esd_buf[2] = 0xAA;
				gtp_i2c_write_no_rst(ts->client, esd_buf, 3);
				break;
			}
		}
	}
	if (i >= 3) {
	#if GTP_COMPATIBLE_MODE
		if (CHIP_TYPE_GT9F == ts->chip_type) {
			if (ts->rqst_processing) {
				GTP_INFO("Request processing, no esd recovery");
			} else {
				GTP_ERROR("IC working abnormally! Process esd recovery.");
				esd_buf[0] = 0x42;
				esd_buf[1] = 0x26;
				esd_buf[2] = 0x01;
				esd_buf[3] = 0x01;
				esd_buf[4] = 0x01;
				gtp_i2c_write_no_rst(ts->client, esd_buf, 5);
				msleep(50);
		#ifdef GTP_CONFIG_OF
				gtp_power_switch(ts->client, 0);
				msleep(20);
				gtp_power_switch(ts->client, 1);
				msleep(20);
		#endif
				gtp_esd_recovery(ts->client);
			}
		} else
	#endif
		{
			GTP_ERROR("IC working abnormally! Process reset guitar.");
			esd_buf[0] = 0x42;
			esd_buf[1] = 0x26;
			esd_buf[2] = 0x01;
			esd_buf[3] = 0x01;
			esd_buf[4] = 0x01;
			gtp_i2c_write_no_rst(ts->client, esd_buf, 5);
			msleep(50);
		#ifdef GTP_CONFIG_OF
			gtp_power_switch(ts->client, 0);
			msleep(20);
			gtp_power_switch(ts->client, 1);
			msleep(20);
		#endif
			gtp_reset_guitar(ts->client, 50);
			msleep(50);
			gtp_send_cfg(ts->client);
		}
	}

	if (!ts->gtp_is_suspend) {
		queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work, ts->clk_tick_cnt);
	} else {
		GTP_INFO("Esd suspended!");
	}
	return;
}
#endif

#ifdef GTP_CONFIG_OF
static const struct of_device_id goodix_match_table[] = {
		{.compatible = "goodix,gt9xx",},
		{ },
};
#endif

static const struct i2c_device_id goodix_ts_id[] = {
	{ GTP_I2C_NAME, 0 },
	{ }
};

static struct i2c_driver goodix_ts_driver = {
	.probe      = goodix_ts_probe,
	.remove     = goodix_ts_remove,
	.id_table   = goodix_ts_id,
	.driver = {
		.name     = GTP_I2C_NAME,
		.owner    = THIS_MODULE,
#ifdef GTP_CONFIG_OF
		.of_match_table = goodix_match_table,
#endif
#if !defined(CONFIG_FB) && defined(CONFIG_PM)
		.pm		= &gtp_pm_ops,
#endif
	},
};

/*******************************************************
Function:
	Driver Install function.
Input:
	None.
Output:
	Executive Outcomes. 0---succeed.
********************************************************/

static int __init goodix_ts_init(void)
{
	s32 ret;

	GTP_DEBUG_FUNC();
	GTP_INFO("GTP driver installing...");
	goodix_wq = create_singlethread_workqueue("goodix_wq");
	if (!goodix_wq) {
		GTP_ERROR("Creat workqueue failed.");
		return -ENOMEM;
	}
#if GTP_ESD_PROTECT
	INIT_DELAYED_WORK(&gtp_esd_check_work, gtp_esd_check_func);
	gtp_esd_check_workqueue = create_workqueue("gtp_esd_check");
#endif
	ret = i2c_add_driver(&goodix_ts_driver);
	return ret;
}

/*******************************************************
Function:
	Driver uninstall function.
Input:
	None.
Output:
	Executive Outcomes. 0---succeed.
********************************************************/
static void __exit goodix_ts_exit(void)
{
	GTP_DEBUG_FUNC();
	GTP_INFO("GTP driver exited.");
	i2c_del_driver(&goodix_ts_driver);
	if (goodix_wq) {
		destroy_workqueue(goodix_wq);
	}
}

device_initcall_sync(goodix_ts_init);
module_exit(goodix_ts_exit);

MODULE_DESCRIPTION("GTP Series Driver");
MODULE_LICENSE("GPL");
