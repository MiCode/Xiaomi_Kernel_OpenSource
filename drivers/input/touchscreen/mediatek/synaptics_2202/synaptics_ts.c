/*
 * Copyright (C) 2013 LG Electironics, Inc.
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

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/rtpm_prio.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/byteorder/generic.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
#include <mach/mt_wdt.h>
#include <mach/mt_gpt.h>
#include <mach/mt_reg_base.h>
#include <mach/eint.h>
#ifndef TPD_NO_GPIO
#include <cust_gpio_usage.h>
#endif
#include <tpd.h>
#include <cust_eint.h>

#include "synaptics_ts.h"

#define LGE_USE_SYNAPTICS_FW_UPGRADE
#if defined(LGE_USE_SYNAPTICS_FW_UPGRADE)
#include "SynaImage.h"
#endif

//#define LGE_USE_SYNAPTICS_RED_ON_MTK

#define LGE_USE_SYNAPTICS_F54
#if defined(LGE_USE_SYNAPTICS_F54)
#include "RefCode.h"
#include "RefCode_PDTScan.h"
#endif


//ticklewind.kim@lge.com_S:
#define CUST_G2_TOUCH_WAKEUP_GESTURE	

#ifdef CUST_G2_TOUCH_WAKEUP_GESTURE	
#include <linux/wakelock.h>	
#include <linux/mutex.h>
#include <linux/pm_wakeup.h>
#include <linux/pm_runtime.h>
#include <linux/irq.h>
#if 0
#include <mach/mpm.h>

#define REPORT_MODE_REG 0x0056
static int touch_gesture_enable = 0;	
static struct wake_lock touch_wake_lock;	
static struct mutex i2c_suspend_lock;	
static spinlock_t touch_spin_lock;	
#endif


static int __attribute__ ((unused)) ts_suspend = 0;
static int __attribute__ ((unused)) ts_force_update_chk = 0;
#endif
//ticklewind.kim@lge.com_E:

/* RMI4 spec from (RMI4 spec)511-000136-01_revD
 * Function	Purpose									See page
 * $01		RMI Device Control						29
 * $08		BIST(Built-in Self Test)				38
 * $09		BIST(Built-in Self Test)				42
 * $11		2-D TouchPad sensors					46
 * $19		0-D capacitive button sensors			69
 * $30		GPIO/LEDs (includes mechanical buttons)	76
 * $32		Timer									89
 * $34		Flash Memory Management					93k
 */
#define RMI_DEVICE_CONTROL				0x01
#define TOUCHPAD_SENSORS				0x11
#define CAPACITIVE_BUTTON_SENSORS		0x1A
#define GPIO_LEDS						0x31
//#define GPIO_LEDS						0x30
#define TIMER							0x32
#define FLASH_MEMORY_MANAGEMENT			0x34
#define ANALOG_CONTROL					0x54

/* Register Map & Register bit mask
 * - Please check "One time" this map before using this device driver
 */
#define DEVICE_STATUS_REG				(ts->common_dsc.data_base)			/* Device Status */
#define INTERRUPT_STATUS_REG			(ts->common_dsc.data_base+1)		/* Interrupt Status */
#define DEVICE_STATUS_REG_UNCONFIGURED	0x80
#define DEVICE_STATUS_FLASH_PROG		0x40
#define DEVICE_CRC_ERROR_MASK			0x04
#define DEVICE_FAILURE_MASK				0x03

#define DEVICE_CONTROL_REG 				(ts->common_dsc.control_base)		/* Device Control */
#define INTERRUPT_ENABLE_REG			(ts->common_dsc.control_base+1)		/* Interrupt Enable */
#define DEVICE_CONTROL_REG_SLEEP 		0x01
#define DEVICE_CONTROL_REG_NOSLEEP		0x04
#define DEVICE_CONTROL_REG_CONFIGURED	0x80

#define MANUFACTURER_ID_REG				(ts->common_dsc.query_base)			/* Manufacturer ID */
#define FW_REVISION_REG					(ts->common_dsc.query_base+3)		/* FW revision */
#define PRODUCT_ID_REG					(ts->common_dsc.query_base+11)		/* Product ID */

#define FINGER_STATE_REG				(ts->finger_dsc.data_base)			/* Finger State */
#define FINGER_STATE_MASK				0x03

#define TWO_D_EXTEND_STATUS				(ts->finger_dsc.data_base+27)
#define TWO_D_REPORTING_MODE			(ts->finger_dsc.control_base+0)		/* 2D Reporting Mode */
#define CONTINUOUS_REPORT_MODE			0x0
#define REDUCED_REPORT_MODE				0x1
#define ABS_MODE						0x8
#define REPORT_BEYOND_CLIP				0x80

#define PALM_DETECT_REG 				(ts->finger_dsc.control_base+1)		/* Palm Detect */
#define DELTA_X_THRESH_REG 				(ts->finger_dsc.control_base+2)		/* Delta-X Thresh */
#define DELTA_Y_THRESH_REG 				(ts->finger_dsc.control_base+3)		/* Delta-Y Thresh */
#define SENSOR_MAX_X_POS				(ts->finger_dsc.control_base+6)		/* SensorMaxXPos */
#define SENSOR_MAX_Y_POS				(ts->finger_dsc.control_base+8)		/* SensorMaxYPos */
//#define GESTURE_ENABLE_1_REG 			(ts->finger_dsc.control_base+10)	/* Gesture Enables 1 */
//#define GESTURE_ENABLE_2_REG 			(ts->finger_dsc.control_base+11)	/* Gesture Enables 2 */
#define SMALL_OBJECT_DETECTION_TUNNING_REG	(ts->finger_dsc.control_base+7) 
#define SMALL_OBJECT_DETECTION			0x04

#define FINGER_COMMAND_REG				(ts->finger_dsc.command_base)
#define BUTTON_DATA_REG					(ts->button_dsc.data_base)			/* Button Data */

#define FLASH_CONFIG_ID_REG				(ts->flash_dsc.control_base)		/* Flash Control */
#define FLASH_CONTROL_REG				(ts->flash_dsc.data_base+18)
#define FLASH_STATUS_MASK				0xF0
#define ANALOG_CONTROL_REG				(ts->analog_dsc.control_base)
#define FORCE_FAST_RELAXATION			0x04

#define ANALOG_COMMAND_REG				(ts->analog_dsc.command_base) 
#define FORCE_UPDATE					0x04

#define COMMON_PAGE						(ts->common_page)
#define FINGER_PAGE						(ts->finger_page)
#define BUTTON_PAGE						(ts->button_page)
#define ANALOG_PAGE						(ts->analog_page)
#define FLASH_PAGE						(ts->flash_page)
#define DEFAULT_PAGE					0x00


/* General define */
#define TOUCH_PRESSED				1
#define TOUCH_RELEASED				0
#define BUTTON_CANCLED				0xff

/* Macro */
#define GET_X_POSITION(high, low) 		((int)(high<<4)|(int)(low&0x0F))
#define GET_Y_POSITION(high, low) 		((int)(high<<4)|(int)((low&0xF0)>>4))


#define get_time_interval(a,b) a>=b ? a-b : 1000000+a-b
#define jitter_abs(x)		(x > 0 ? x : -x)
#define jitter_sub(x, y)	(x > y ? x - y : y - x)

static unsigned int synaptics_rmi4_i2c_debug_mask = SYNAPTICS_RMI4_I2C_DEBUG_BUTTON_STATUS;

static struct workqueue_struct *synaptics_wq;
static struct synaptics_ts_data *ts_hub = NULL;


static int pressure_zero = 0;
static int resume_flag = 0;
static int ts_rebase_count =0;
static int ghost_detection = 0;
static int ghost_detection_count = 0;
static int finger_subtraction_check_count = 0;
static int force_continuous_mode = 0;
static int long_press_check = 0;
static int long_press_check_count = 0;
static int button_check = 0;
static unsigned int button_press_count =0;
static unsigned int old_pos_x = 0;
static unsigned int old_pos_y = 0;
char finger_oldstate[MAX_NUM_OF_FINGER];
char button_oldstate[MAX_NUM_OF_BUTTON];
ts_finger_data old_ts_data;
struct timeval t_ex_debug[TIME_EX_PROFILE_MAX];


struct class *touch_class;
struct device *touch_debug_dev;

static int synaptics_tpd_flag = 0;
static int synaptics_tpd_keylock_flag=0;
static DECLARE_WAIT_QUEUE_HEAD(synaptics_waiter);

#if defined(LGE_USE_SYNAPTICS_FW_UPGRADE)
struct device *touch_fw_dev;
struct wake_lock fw_suspend_lock;
extern int FirmwareUpgrade(struct synaptics_ts_data *ts, const char* fw_path);
#endif

#if defined(LGE_USE_SYNAPTICS_RED_ON_MTK)
extern void red_i2c_device_setup(void);
#endif

#if defined(LGE_USE_SYNAPTICS_F54)
struct i2c_client *ds4_i2c_client;
static int f54_fullrawcap_mode = 0;
#endif

#define BUFFER_SIZE 128
struct st_i2c_msgs
{
	struct i2c_msg *msg;
	int count;
} i2c_msgs;

#if 1
#include <linux/dma-mapping.h>

static u8*		I2CDMABuf_va = NULL;
static dma_addr_t		I2CDMABuf_pa = 0;

int i2c_dma_write(struct i2c_client *client, const uint8_t *buf, int len)
{
	int i = 0;
	for(i = 0 ; i < len; i++) {
		I2CDMABuf_va[i] = buf[i];
	}

	if(len < 8) {
		client->addr = (client->addr & I2C_MASK_FLAG) | I2C_ENEXT_FLAG;
		return i2c_master_send(client, buf, len);
	}
	else {
		client->addr = (client->addr & I2C_MASK_FLAG) | I2C_DMA_FLAG | I2C_ENEXT_FLAG;
		return i2c_master_send(client, (const char*)I2CDMABuf_pa, len);
	}
}

int i2c_dma_read(struct i2c_client *client, uint8_t *buf, int len)
{	
	int i = 0, ret = 0; 	   
	if(len < 8) {
		client->addr = (client->addr & I2C_MASK_FLAG) | I2C_ENEXT_FLAG;
		return i2c_master_recv(client, buf, len);
	}
	else {
		client->addr = (client->addr & I2C_MASK_FLAG) | I2C_DMA_FLAG | I2C_ENEXT_FLAG;
		ret = i2c_master_recv(client, (char*)I2CDMABuf_pa, len);
		if(ret < 0) {
			return ret;
		}
		for(i = 0; i < len; i++) {
			buf[i] = I2CDMABuf_va[i];
		}
	}
	return ret;
}



static int i2c_msg_transfer(struct i2c_client *client, struct i2c_msg *msgs, int count)
{
	int i = 0, ret = 0;
	for(i = 0; i < count; i++) {
		if(msgs[i].flags & I2C_M_RD) {
			ret = i2c_dma_read(client, msgs[i].buf, msgs[i].len);
		}
		else {
			ret = i2c_dma_write(client, msgs[i].buf, msgs[i].len);
		}
		if(ret < 0)return ret;
	}
	return 0;
}

#endif // 1
int synaptics_ts_read_f54(struct i2c_client *client, u8 reg, int num, u8 *buf)
{
	int message_count = ((num - 1) / BUFFER_SIZE) + 2;
	int message_rest_count = num % BUFFER_SIZE;
	int i, data_len;
           
	if (i2c_msgs.msg == NULL || i2c_msgs.count < message_count) {
		if (i2c_msgs.msg != NULL)
			kfree(i2c_msgs.msg);
		i2c_msgs.msg = (struct i2c_msg*)kcalloc(message_count, sizeof(struct i2c_msg), GFP_KERNEL);
		i2c_msgs.count = message_count;
		//dev_dbg(&client->dev, "%s: Update message count %d(%d)\n",  __func__, i2c_msgs.count, message_count);
	}
	
	i2c_msgs.msg[0].addr = client->addr;
	i2c_msgs.msg[0].flags = 0;
	i2c_msgs.msg[0].len = 1;
	i2c_msgs.msg[0].buf = &reg;
			
	if (!message_rest_count)
		message_rest_count = BUFFER_SIZE;
	for (i = 0; i < message_count -1 ;i++) {
		if (i == message_count - 1)
			data_len = message_rest_count;
		else
			data_len = BUFFER_SIZE;
		i2c_msgs.msg[i + 1].addr = client->addr;
		i2c_msgs.msg[i + 1].flags = I2C_M_RD;
		i2c_msgs.msg[i + 1].len = data_len;
		i2c_msgs.msg[i + 1].buf = buf + BUFFER_SIZE * i;
	}

	if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE)
		SYNAPTICS_DEBUG_MSG(":%x\n", reg);

#if 1
	if (i2c_msg_transfer(client, i2c_msgs.msg, message_count) < 0) {
#else
	if (i2c_transfer(client->adapter, i2c_msgs.msg, message_count) < 0) {
#endif // 1
		if (printk_ratelimit())
			SYNAPTICS_ERR_MSG("transfer error\n");
		return -EIO;
	} else
		return 0;
}


int synaptics_ts_read(struct i2c_client *client, u8 reg, int num, u8 *buf)
{
		
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = num,
			.buf = buf,
		},
	};

	
	if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE)
		SYNAPTICS_DEBUG_MSG(":%x\n", reg);

#if 1
	if (i2c_msg_transfer(client, msgs, 2) < 0) {
#else
	if (i2c_transfer(client->adapter, msgs, 2) < 0) {
#endif // 1
		if (printk_ratelimit())
			SYNAPTICS_ERR_MSG("transfer error\n");
		return -EIO;
	} else
		return 0;

}
EXPORT_SYMBOL(synaptics_ts_read);

int synaptics_ts_write(struct i2c_client *client, u8 reg, u8 * buf, int len)
{
	
	unsigned char send_buf[len + 1];
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = client->flags,
			.len = len+1,
			.buf = send_buf,
		},
	};


	send_buf[0] = (unsigned char)reg;
	memcpy(&send_buf[1], buf, len);



	
	if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE)
		SYNAPTICS_DEBUG_MSG(":%x\n", reg);

#if 1
	if (i2c_msg_transfer(client, msgs, 1) < 0) {
#else


	if (i2c_transfer(client->adapter, msgs, 1) < 0) {
#endif // 1
		if (printk_ratelimit())
			SYNAPTICS_ERR_MSG("transfer error\n");
		return -EIO;
	} else
		return 0;

}
EXPORT_SYMBOL(synaptics_ts_write);

int synaptics_ts_page_data_read(struct i2c_client *client, u8 page, u8 reg, int size, u8 *data)
{
	int ret = 0;
	
	ret = i2c_smbus_write_byte_data(client, PAGE_SELECT_REG, page);
	if (ret < 0)
		SYNAPTICS_ERR_MSG("PAGE_SELECT_REG write fail\n");

	ret = synaptics_ts_read(client, reg, size, data);
	if (ret < 0)
		SYNAPTICS_ERR_MSG("synaptics_ts_read read fail\n");

	ret = i2c_smbus_write_byte_data(client, PAGE_SELECT_REG, 0x00);
	if (ret < 0)
		SYNAPTICS_ERR_MSG("PAGE_SELECT_REG write fail\n");

	return 0;
}

int synaptics_ts_page_data_write(struct i2c_client *client, u8 page, u8 reg, int size, u8 *data)
{
	int ret = 0;

	ret = i2c_smbus_write_byte_data(client, PAGE_SELECT_REG, page);
	if (ret < 0)
		SYNAPTICS_ERR_MSG("PAGE_SELECT_REG write fail\n");

	ret = synaptics_ts_write(client, reg, data, size);
	if (ret < 0)
		SYNAPTICS_ERR_MSG("synaptics_ts_write read fail\n");

	ret = i2c_smbus_write_byte_data(client, PAGE_SELECT_REG, 0x00);
	if (ret < 0)
		SYNAPTICS_ERR_MSG("PAGE_SELECT_REG write fail\n");

	return 0;
}

void touch_keylock_enable(int key_lock)
{
	struct synaptics_ts_data *ts = ts_hub;

	if(ts == NULL) {
		SYNAPTICS_ERR_MSG("ts is NULL\n");
		return;
	}
	
	if(!key_lock) {
		if(ts->pdata->use_irq)
			mt65xx_eint_unmask(ts->pdata->irq_num);
		else
			hrtimer_start(&ts->timer, ktime_set(0, ts->pdata->report_period+(ts->pdata->ic_booting_delay*1000000)), HRTIMER_MODE_REL);
		synaptics_tpd_keylock_flag=0;
	} 
	else {
		if(ts->pdata->use_irq)
			mt65xx_eint_mask(ts->pdata->irq_num);
		else
			hrtimer_cancel(&ts->timer);
		synaptics_tpd_keylock_flag=1;
	}      
}
EXPORT_SYMBOL(touch_keylock_enable);

static void synaptics_ts_reset(int reset_pin)
{	
	if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE)
		SYNAPTICS_DEBUG_MSG("\n");
	
	mt_set_gpio_mode(reset_pin, GPIO_MODE_00);
	mt_set_gpio_dir(reset_pin, GPIO_DIR_OUT);
	mt_set_gpio_out(reset_pin, GPIO_OUT_ZERO);
	msleep(10);
	mt_set_gpio_mode(reset_pin, GPIO_MODE_00);
	mt_set_gpio_dir(reset_pin, GPIO_DIR_OUT);
	mt_set_gpio_out(reset_pin, GPIO_OUT_ONE);
	msleep(50);
}

static void synaptics_ts_hard_reset(struct synaptics_ts_data *ts)
{
	int ret = 0;

	/* 1. VIO off
	 * 2. VDD off
	 * 3. Wait more than 10ms
	 * 4. VDD on
	 * 5. VIO on
	 * 6. Initialization
	 */
	if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE)
		SYNAPTICS_DEBUG_MSG("\n");
	
	if (ts->pdata->power) {
		ret = ts->pdata->power(0);

		if (ret < 0)
			SYNAPTICS_ERR_MSG("power on failed\n");

		mdelay(20);

		synaptics_ts_reset(ts->pdata->reset_gpio);

		ret = ts->pdata->power(1);

		if (ret < 0)
			SYNAPTICS_ERR_MSG("power on failed\n");
	}

	queue_delayed_work(synaptics_wq, &ts->work,msecs_to_jiffies(ts->pdata->ic_booting_delay));
}

int synaptics_ts_ic_ctrl(struct i2c_client *client, u8 code, u16 value)
{
	struct synaptics_ts_data* ts;
	u8 buf = 0;

	ts = ts_hub;

	if(ts == NULL) {
		SYNAPTICS_ERR_MSG("ts is NULL\n");
		return 0;
	}

	SYNAPTICS_INFO_MSG("synaptics_ts_ic_ctrl: %d, %d\n", code, value);

	switch (code)
	{
		case IC_CTRL_BASELINE:
			switch (value)
			{
				case BASELINE_OPEN:
#if 0
					if (unlikely(synaptics_ts_page_data_write(client, ANALOG_PAGE, ANALOG_CONTROL_REG, 1, FORCE_FAST_RELAXATION) < 0)) {
						SYNAPTICS_ERR_MSG("ANALOG_CONTROL_REG write fail\n");
						return -EIO;
					}

					msleep(10);

					if (unlikely(synaptics_ts_page_data_write(client, ANALOG_PAGE, ANALOG_COMMAND_REG, 1, FORCE_UPDATE) < 0)) {
						SYNAPTICS_ERR_MSG("ANALOG_COMMAND_REG write fail\n");
						return -EIO;
					}

					if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FINGER_STATUS)
						SYNAPTICS_INFO_MSG("BASELINE_OPEN\n");
#endif
				break;

				case BASELINE_FIX:
#if 0
					if (unlikely(synaptics_ts_page_data_write(client, ANALOG_PAGE, ANALOG_CONTROL_REG, 1, 0x00) < 0)) {
						TOUCH_ERR_MSG("ANALOG_CONTROL_REG write fail\n");
						return -EIO;
					}

					msleep(10);

					if (unlikely(synaptics_ts_page_data_write(client, ANALOG_PAGE, ANALOG_COMMAND_REG, 1, FORCE_UPDATE) < 0)) {
						TOUCH_ERR_MSG("ANALOG_COMMAND_REG write fail\n");
						return -EIO;
					}

					if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FINGER_STATUS)
						TOUCH_INFO_MSG("BASELINE_FIX\n");
#endif
				break;
				
				case BASELINE_REBASE:
					/* rebase base line */
					if (likely(ts->finger_dsc.id != 0)) {
						if (unlikely(i2c_smbus_write_byte_data(client, FINGER_COMMAND_REG, 0x1) < 0)) {
							SYNAPTICS_ERR_MSG("finger baseline reset command write fail\n");
							return -EIO;
						}
					}
				break;

				default:
					break;
			}
		break;

		case IC_CTRL_REPORT_MODE:
			switch (value)
			{
				case 0:   // continuous mode
					if (unlikely(i2c_smbus_write_byte_data(client, TWO_D_REPORTING_MODE, REPORT_BEYOND_CLIP | ABS_MODE | CONTINUOUS_REPORT_MODE) < 0)) {
						SYNAPTICS_ERR_MSG("TWO_D_REPORTING_MODE write fail\n");
						return -EIO;
					}
				break;
				
				case 1:  // reduced mode
					if (unlikely(i2c_smbus_write_byte_data(client, TWO_D_REPORTING_MODE, REPORT_BEYOND_CLIP | ABS_MODE | REDUCED_REPORT_MODE) < 0)) {
						SYNAPTICS_ERR_MSG("TWO_D_REPORTING_MODE write fail\n");
						return -EIO;
					}
				break;
				
				default:
				break;
			}
		break;

		default:
		break;
	}

	return buf;
}

void release_all_ts_event(void)
{
	struct synaptics_ts_data *ts;
	unsigned int f_counter = 0;
	unsigned int b_counter = 0;
	char report_enable = 0;

	ts = ts_hub;

	if(ts == NULL) {
		SYNAPTICS_ERR_MSG("ts is NULL\n");
		return;
	}

	/* Finger check */
	for(f_counter = 0; f_counter < ts->pdata->num_of_finger; f_counter++) {
		if (ts->finger_prestate[f_counter] == TOUCH_PRESSED) {
#if defined(MT_PROTOCOL_A)			
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, ts->pre_ts_data.pos_x[f_counter]);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, ts->pre_ts_data.pos_y[f_counter]);
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE, TOUCH_RELEASED);
			input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, TOUCH_RELEASED);
#else			
			input_mt_slot(ts->input_dev, f_counter);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
#endif
			report_enable = 1;
		}
	}

	/* Button check */
	for(b_counter = 0; b_counter < ts->pdata->num_of_button; b_counter++) {
		if (ts->button_prestate[b_counter] == TOUCH_PRESSED) {
			report_enable = 1;
			input_report_key(ts->input_dev, ts->pdata->button[b_counter], TOUCH_RELEASED);
		}
	}

	/* Reset finger position data */
	memset(&old_ts_data, 0x0, sizeof(ts_finger_data));
	memset(&ts->pre_ts_data, 0x0, sizeof(ts_finger_data));

	if (report_enable) {
		SYNAPTICS_INFO_MSG("Release all pressed event before touch power off\n");
		input_sync(ts->input_dev);

		/* Reset finger & button status data */
		memset(finger_oldstate, 0x0, sizeof(char) * ts->pdata->num_of_finger);
		memset(button_oldstate, 0x0, sizeof(char) * ts->pdata->num_of_button);
		memset(ts->finger_prestate, 0x0, sizeof(char) * ts->pdata->num_of_finger);
		memset(ts->button_prestate, 0x0, sizeof(char) * ts->pdata->num_of_button);
	}
}

bool chk_time_interval(struct timeval t_aft, struct timeval t_bef, int t_val)
{
	if( t_aft.tv_sec - t_bef.tv_sec == 0 ) {
		if((get_time_interval(t_aft.tv_usec, t_bef.tv_usec)) <= t_val)
			return true;
	} else if( t_aft.tv_sec - t_bef.tv_sec == 1 ) {
		if( t_aft.tv_usec + 1000000 - t_bef.tv_usec <= t_val)
			return true;
	}

	return false;
}

int ghost_detect_solution(void)
{
	int first_int_detection = 0;
	int cnt = 0, id = 0;
	struct synaptics_ts_data *ts;

	ts = ts_hub;

	if(ts == NULL) {
		SYNAPTICS_ERR_MSG("ts is NULL\n");
		return 0;
	}
	
	if (resume_flag) {
		resume_flag = 0;
		do_gettimeofday(&t_ex_debug[TIME_EX_FIRST_INT_TIME]);

		if (t_ex_debug[TIME_EX_FIRST_INT_TIME].tv_sec - t_ex_debug[TIME_EX_INIT_TIME].tv_sec == 0 ) {
			if((get_time_interval(t_ex_debug[TIME_EX_FIRST_INT_TIME].tv_usec, t_ex_debug[TIME_EX_INIT_TIME].tv_usec)) <= 200000)
				first_int_detection = 1;
		}
		else if (t_ex_debug[TIME_EX_FIRST_INT_TIME].tv_sec - t_ex_debug[TIME_EX_INIT_TIME].tv_sec == 1) {
			if(t_ex_debug[TIME_EX_FIRST_INT_TIME].tv_usec + 1000000 - t_ex_debug[TIME_EX_INIT_TIME].tv_usec <= 200000) {
				first_int_detection = 1;
			}
		}
	}

	if (first_int_detection) {
		for (cnt = 0; cnt < ts->pdata->num_of_finger; cnt++) {
			if (ts->finger_prestate[cnt] == TOUCH_PRESSED) {
				SYNAPTICS_INFO_MSG("ghost detected within first input time 200ms\n");
				ghost_detection = 1;
			}
		}
	}

	if (pressure_zero == 1) {
		SYNAPTICS_INFO_MSG("ghost detected on pressure\n");
		ghost_detection = 1;
	}

	if (ts->pre_ts_data.total_num) {
		if (old_ts_data.total_num != ts->pre_ts_data.total_num) {
			if (old_ts_data.total_num <= ts->pre_ts_data.total_num) {
				for(id = 0; id < ts->pdata->num_of_finger; id++) {
					if (ts->finger_prestate[id] == TOUCH_PRESSED && finger_oldstate[id] == TOUCH_RELEASED) {
						break;
					}
				}

				if (id < ts->pdata->num_of_finger) {
					memcpy(&t_ex_debug[TIME_EX_PREV_PRESS_TIME], &t_ex_debug[TIME_EX_CURR_PRESS_TIME], sizeof(struct timeval));
					do_gettimeofday(&t_ex_debug[TIME_EX_CURR_PRESS_TIME]);

					if ((ts->pre_ts_data.pos_x[id] > 0) && (ts->pre_ts_data.pos_x[id] < ts->pdata->x_max) 
						&& (1 <= old_ts_data.total_num) && (1 <= ts->pre_ts_data.total_num) 
						&& (jitter_sub(old_pos_x, ts->pre_ts_data.pos_x[id]) <= 10) && (jitter_sub(old_pos_y, ts->pre_ts_data.pos_y[id]) <= 10)) {
						if (chk_time_interval(t_ex_debug[TIME_EX_CURR_PRESS_TIME], t_ex_debug[TIME_EX_PREV_PRESS_TIME], 50000)) {
							ghost_detection = 1;
							ghost_detection_count++;
						}
					}
					else if ((ts->pre_ts_data.pos_x[id] > 0) && (ts->pre_ts_data.pos_x[id] < ts->pdata->x_max) 
						&& (old_ts_data.total_num == 0) && (ts->pre_ts_data.total_num == 1) 
						&& (jitter_sub(old_pos_x, ts->pre_ts_data.pos_x[id]) <= 10) && (jitter_sub(old_pos_y, ts->pre_ts_data.pos_y[id]) <= 10)) {
						if (chk_time_interval(t_ex_debug[TIME_EX_CURR_PRESS_TIME], t_ex_debug[TIME_EX_PREV_PRESS_TIME], 50000)) {
							ghost_detection = 1;
						}
					}
					else if (5 < jitter_sub(old_ts_data.total_num, ts->pre_ts_data.total_num)) {
						 ghost_detection = 1;
					}
					else
						; //do not anything

					old_pos_x = ts->pre_ts_data.pos_x[id];
					old_pos_y = ts->pre_ts_data.pos_y[id];
				}
			}
			else {
				memcpy(&t_ex_debug[TIME_EX_PREV_PRESS_TIME], &t_ex_debug[TIME_EX_CURR_PRESS_TIME], sizeof(struct timeval));
				do_gettimeofday(&t_ex_debug[TIME_EX_CURR_INT_TIME]);

				if (chk_time_interval(t_ex_debug[TIME_EX_CURR_INT_TIME], t_ex_debug[TIME_EX_PREV_PRESS_TIME], 10999)) {
					finger_subtraction_check_count++;
				}
				else {
					finger_subtraction_check_count = 0;
				}

				if (4 < finger_subtraction_check_count) {
					finger_subtraction_check_count = 0;
					SYNAPTICS_INFO_MSG("need_to_rebase finger_subtraction!!!\n");
					/* rebase is disabled. see TD 41871*/
					//goto out_need_to_rebase;
				}
			}
		}

		if (force_continuous_mode) {
			do_gettimeofday(&t_ex_debug[TIME_EX_CURR_INT_TIME]);
			// if 20 sec have passed since resume, then return to the original report mode.
			if(t_ex_debug[TIME_EX_CURR_INT_TIME].tv_sec - t_ex_debug[TIME_EX_INIT_TIME].tv_sec >= 10) {
				if(synaptics_ts_ic_ctrl(ts->client, IC_CTRL_REPORT_MODE, ts->pdata->report_mode) < 0) {
					SYNAPTICS_ERR_MSG("IC_CTRL_BASELINE(1) handling fail\n");
					goto out_need_to_init;
				}
				force_continuous_mode = 0;
			}

			long_press_check = 0;

			for(cnt = 0; cnt < MAX_NUM_OF_FINGER; cnt++) {
				if (ts->finger_prestate[cnt] == TOUCH_PRESSED) {
					if ((finger_oldstate[cnt] == TOUCH_PRESSED) && (jitter_sub(old_ts_data.pos_x[cnt], ts->pre_ts_data.pos_x[cnt]) < 10) && (jitter_sub(old_ts_data.pos_y[cnt], ts->pre_ts_data.pos_y[cnt]) < 10)) {
						long_press_check = 1;
					}
				}
			}

			if (long_press_check) {
				long_press_check_count++;
			}
			else {
				long_press_check_count = 0;
			}

			if (500 < long_press_check_count) {
				long_press_check_count = 0;
				SYNAPTICS_INFO_MSG("need_to_rebase long press!!!\n");
				goto out_need_to_rebase;
			}
		}
	}
	else if (!ts->pre_ts_data.total_num) {
		long_press_check_count = 0;
		finger_subtraction_check_count = 0;
	}

	button_check = 0;
	for(id = 0; id < ts->pdata->num_of_button; id++) {
		if (ts->button_prestate[id] == TOUCH_PRESSED && button_oldstate[id] == TOUCH_RELEASED) {
			button_check = 1;
			break;
		}
	}
	if (button_check) {
		if (button_press_count == 0)
			do_gettimeofday(&t_ex_debug[TIME_EX_BUTTON_PRESS_START_TIME]);
		else
			do_gettimeofday(&t_ex_debug[TIME_EX_BUTTON_PRESS_END_TIME]);
	
		button_press_count++;
	
		if (6 <= button_press_count) {
			if (chk_time_interval(t_ex_debug[TIME_EX_BUTTON_PRESS_END_TIME], t_ex_debug[TIME_EX_BUTTON_PRESS_START_TIME], 100000)) {
				SYNAPTICS_INFO_MSG("need_to_rebase button zero\n");
				goto out_need_to_rebase;
			}
		else {
				button_press_count = 0;
			}
			}
		else {
			if (!chk_time_interval(t_ex_debug[TIME_EX_BUTTON_PRESS_END_TIME], t_ex_debug[TIME_EX_BUTTON_PRESS_START_TIME], 100000)) {
				button_press_count = 0;
			}
		}
	}

	if ((ghost_detection == 1) && (ts->pre_ts_data.total_num == 0) && (ts->pre_ts_data.palm == 0)) {
		SYNAPTICS_INFO_MSG("need_to_rebase zero\n");
		goto out_need_to_rebase;
	}
	else if ((ghost_detection == 1) && (3 <= ghost_detection_count) && (ts->pre_ts_data.palm == 0)) {
		SYNAPTICS_INFO_MSG("need_to_rebase zero 3\n");
		goto out_need_to_rebase;
	}

	return 0;

out_need_to_rebase:
	{
		ghost_detection = 0;
		ghost_detection_count = 0;
		old_pos_x = 0;
		old_pos_y = 0;
		ts_rebase_count++;

		if (ts_rebase_count == 1) {
			do_gettimeofday(&t_ex_debug[TIME_EX_FIRST_GHOST_DETECT_TIME]);

			if ((t_ex_debug[TIME_EX_FIRST_GHOST_DETECT_TIME].tv_sec - t_ex_debug[TIME_EX_INIT_TIME].tv_sec) <= 3) {
				ts_rebase_count = 0;
				SYNAPTICS_INFO_MSG("need_to_init in 3 sec\n");
				goto out_need_to_init;
			}
		}
		else {
			do_gettimeofday(&t_ex_debug[TIME_EX_SECOND_GHOST_DETECT_TIME]);

			if (((t_ex_debug[TIME_EX_SECOND_GHOST_DETECT_TIME].tv_sec - t_ex_debug[TIME_EX_FIRST_GHOST_DETECT_TIME].tv_sec) <= 5)) {
				ts_rebase_count = 0;
				SYNAPTICS_INFO_MSG("need_to_init\n");
				goto out_need_to_init;
			}
			else {
				ts_rebase_count = 1;
				memcpy(&t_ex_debug[TIME_EX_FIRST_GHOST_DETECT_TIME], &t_ex_debug[TIME_EX_SECOND_GHOST_DETECT_TIME], sizeof(struct timeval));
			}
		}
		
		release_all_ts_event();
		if (synaptics_ts_ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_REBASE) < 0) {
			SYNAPTICS_ERR_MSG("IC_CTRL_REBASE(2) handling fail\n");
		}
		SYNAPTICS_INFO_MSG("need_to_rebase\n");
	}
	return NEED_TO_OUT;

out_need_to_init:	
	return NEED_TO_INIT;
}

#if defined(LGE_USE_SYNAPTICS_FW_UPGRADE)
static int synaptics_ts_need_fw_upgrade(void)
{
	int ret = 0;
	int cur_ver = 0;
	int fw_ver = 0;
	int product_check = 0;
	struct synaptics_ts_data *ts;

	if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE)
		SYNAPTICS_DEBUG_MSG("\n");

	ts = ts_hub;

	if(ts == NULL) {
		SYNAPTICS_ERR_MSG("ts is NULL\n");
		return 0;
	}

	if(!strcmp(ts->product_id, ts->fw_product_id)) {
		product_check = 1;
	}
	cur_ver = (int)simple_strtol(&ts->config_id[1], NULL, 10);
	fw_ver = (int)simple_strtol(&ts->fw_config_id[1], NULL, 10);


	
	SYNAPTICS_INFO_MSG("FW n: %d, %d\n", fw_ver, cur_ver);

	if(ts->fw_path[0] != 0)
		ret = 2; /* FW exist in file */
	else if(ts->fw_force_upgrade == 1)
		ret = 1; /* FW exist in buffer */
	else if(product_check && (fw_ver > cur_ver))
		ret = 1; /* FW exist in buffer */
	else
		ret = 0; /* No need to upgrade FW */


	if( !ts_force_update_chk )	{
		ret = 1;
		ts_force_update_chk = 1;	
	}
	
	SYNAPTICS_INFO_MSG("FW upgrade check: %d\n", ret);

	
	
	return ret;
}

static void synaptics_ts_fw_upgrade(struct synaptics_ts_data *ts, const char* fw_path)
{
	int ret = 0;
	struct synaptics_ts_timestamp time_debug;

	if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE)
		SYNAPTICS_DEBUG_MSG("\n");

	if (likely(!ts->is_downloading)) {
		ts->is_downloading = 1;
		ts->is_probed = 0;
		wake_lock(&fw_suspend_lock);

		if (likely(!ts->is_suspended)) {
			ts->ic_init = 0;

			if (ts->pdata->use_irq)
				mt65xx_eint_mask(ts->pdata->irq_num);
			else
				hrtimer_cancel(&ts->timer);
		} else {
			if (ts->pdata->power) {
				ret = ts->pdata->power(1);

				if (ret < 0) {
					SYNAPTICS_ERR_MSG("power on failed\n");
				} else {
					msleep(ts->pdata->ic_booting_delay);
				}
			}
		}

		if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_UPGRADE_DELAY) {
			memset(&time_debug, 0x0, sizeof(struct synaptics_ts_timestamp));
			atomic_set(&time_debug.ready, 1);
			time_debug.start = cpu_clock(smp_processor_id());
		}

		ret = FirmwareUpgrade(ts, fw_path);
		if(ret < 0) {
			SYNAPTICS_ERR_MSG("Firmware upgrade Fail!!!\n");
		} else {
			SYNAPTICS_INFO_MSG("Firmware upgrade Complete\n");
		}

		if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_UPGRADE_DELAY) {
			if (atomic_read(&time_debug.ready) == 1) {
				time_debug.end = cpu_clock(smp_processor_id());
				time_debug.result_t = time_debug.end - time_debug.start;
				time_debug.rem = do_div(time_debug.result_t , 1000000000);
				SYNAPTICS_DEBUG_MSG("FW upgrade time < %2lu.%06lu\n", (unsigned long)time_debug.result_t, time_debug.rem/1000);
				atomic_set(&time_debug.ready, 0);
			}
		}

		if (likely(!ts->is_suspended)) {
			if (ts->pdata->use_irq)
				mt65xx_eint_unmask(ts->pdata->irq_num);
			else
				hrtimer_start(&ts->timer, ktime_set(0, ts->pdata->report_period+(ts->pdata->ic_booting_delay*1000000)), HRTIMER_MODE_REL);
		}

		memset(ts->fw_path, 0x00, sizeof(ts->fw_path));
		ts->fw_force_upgrade = 0;

		if (likely(!ts->is_suspended)) {
			synaptics_ts_hard_reset(ts);
		} else {
			if (ts->pdata->power) {
				ret = ts->pdata->power(0);

				if (ret < 0)
					SYNAPTICS_ERR_MSG("power on failed\n");
			}
		}

		wake_unlock(&fw_suspend_lock);
		ts->is_downloading = 0;
	} else {
		SYNAPTICS_ERR_MSG("Firmware Upgrade process is aready working on\n");
	}
}

static ssize_t show_version(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct synaptics_ts_data *ts;

	ts = ts_hub;

	if(ts == NULL) {
		SYNAPTICS_ERR_MSG("ts is NULL\n");
		return 0;
	}
	
	return sprintf(buf, "%s : FW_VERSION, %s : HW_VERSION \n", ts->config_id, ts->product_id);
}
static DEVICE_ATTR(version, 0664, show_version, NULL);

static ssize_t store_firmware(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct synaptics_ts_data *ts;
	char path[256] = {0};

	sscanf(buf, "%s", path);

	ts = ts_hub;

	if(ts == NULL) {
		SYNAPTICS_ERR_MSG("ts is NULL\n");
		return size;
	}

	if(ts->pdata->use_irq)
		mt65xx_eint_mask(ts->pdata->irq_num);
	else
		hrtimer_cancel(&ts->timer);

	ts->ic_init = 0;
	ts->is_probed = 0;

	SYNAPTICS_DEBUG_MSG("Firmware image upgrade: %s\n", path);

	if(!strncmp(path, "1", 1)) {
		ts->fw_force_upgrade = 1;
	}
	else {
		memcpy(ts->fw_path, path, sizeof(ts->fw_path));
	}

	if(ts->is_suspended) {
		if(ts->pdata->power)
			ts->pdata->power(1);
	}

	queue_delayed_work(synaptics_wq, &ts->work, msecs_to_jiffies(ts->pdata->ic_booting_delay));

	while(ts->is_downloading);

    return size;
}
static DEVICE_ATTR(firmware, 0664, NULL, store_firmware);
#endif

static ssize_t show_log(struct device *dev, struct device_attribute *attr, char *buf)
{	
	return sprintf(buf, "0x%x\n", synaptics_rmi4_i2c_debug_mask);
}

static ssize_t store_log(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int mask;

	sscanf(buf, "%x", &mask);

	SYNAPTICS_DEBUG_MSG("debug mask: 0x%x\n", mask);
	
	synaptics_rmi4_i2c_debug_mask = mask;

	return size;
}
static DEVICE_ATTR(log, 0664, show_log, store_log);

#if defined(LGE_USE_SYNAPTICS_F54)
static ssize_t show_f54(struct device *dev, struct device_attribute *attr, char *buf)
{	
	int ret = 0;
	struct synaptics_ts_data *ts;

	ts = ts_hub;

	if(ts == NULL) {
		SYNAPTICS_ERR_MSG("ts is NULL\n");
		return ret;
	}

	if (ts->is_suspended == 0) {
		SYNA_PDTScan();
		SYNA_ConstructRMI_F54();
		SYNA_ConstructRMI_F1A();

		ret = sprintf(buf, "====== F54 Function Info ======\n");

		switch(f54_fullrawcap_mode)
		{
			case 0: ret += sprintf(buf+ret, "fullrawcap_mode = For sensor\n");
					break;
			case 1: ret += sprintf(buf+ret, "fullrawcap_mode = For FPC\n");
					break;
			case 2: ret += sprintf(buf+ret, "fullrawcap_mode = CheckTSPConnection\n");
					break;
			case 3: ret += sprintf(buf+ret, "fullrawcap_mode = Baseline\n");
					break;
			case 4: ret += sprintf(buf+ret, "fullrawcap_mode = Delta image\n");
					break;
		}

		if(ts->pdata->use_irq)
			mt65xx_eint_mask(ts->pdata->irq_num);
		else
			hrtimer_cancel(&ts->timer);

		ret += sprintf(buf+ret, "F54_FullRawCap(%d) Test Result: %s", f54_fullrawcap_mode, (F54_FullRawCap(f54_fullrawcap_mode) > 0) ? "Pass\n" : "Fail\n" );
		ret += sprintf(buf+ret, "F54_TxToTxReport() Test Result: %s", (F54_TxToTxReport() > 0) ? "Pass\n" : "Fail\n" );
		ret += sprintf(buf+ret, "F54_RxToRxReport() Test Result: %s", (F54_RxToRxReport() > 0) ? "Pass\n" : "Fail\n" );
		ret += sprintf(buf+ret, "F54_TxToGndReport() Test Result: %s", (F54_TxToGndReport() > 0) ? "Pass\n" : "Fail\n" );
		ret += sprintf(buf+ret, "F54_HighResistance() Test Result: %s", (F54_HighResistance() > 0) ? "Pass\n" : "Fail\n" );

		if (ts->pdata->use_irq)
			mt65xx_eint_unmask(ts->pdata->irq_num);
		else
			hrtimer_start(&ts->timer, ktime_set(0, ts->pdata->report_period+(ts->pdata->ic_booting_delay*1000000)), HRTIMER_MODE_REL);
	} else {
		ret = sprintf(buf+ret, "state=[suspend]. we cannot use I2C, now. Test Result: Fail\n");
	}

	return ret;
}

static ssize_t store_f54(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	
	ret = sscanf(buf, "%d", &f54_fullrawcap_mode);

	return size;
}
static DEVICE_ATTR(f54, 0664, show_f54, store_f54);
#endif

void synaptics_ts_init_sysfs(void)
{
	touch_class = class_create(THIS_MODULE, "touch");
	touch_debug_dev = device_create(touch_class, NULL, 0, NULL, "debug");

	if(device_create_file(touch_debug_dev, &dev_attr_log) < 0) {
		SYNAPTICS_ERR_MSG("log device_create_file failed\n");
	}
	
#if defined(LGE_USE_SYNAPTICS_F54)
	if(device_create_file(touch_debug_dev, &dev_attr_f54) < 0) {
		SYNAPTICS_ERR_MSG("log device_create_file failed\n");
	}
#endif

#if defined(LGE_USE_SYNAPTICS_FW_UPGRADE)
	touch_fw_dev = device_create(touch_class, NULL, 0, NULL, "firmware");

	if(device_create_file(touch_fw_dev, &dev_attr_firmware) < 0) {
		SYNAPTICS_ERR_MSG("firmware device_create_file failed\n");
	}

	if(device_create_file(touch_fw_dev, &dev_attr_version) < 0) {
		SYNAPTICS_ERR_MSG("version device_create_file failed\n");
	}
#endif	
}

static enum hrtimer_restart synaptics_ts_timer_func(struct hrtimer *timer)
{
	struct synaptics_ts_data *ts = container_of(timer, struct synaptics_ts_data, timer);

	if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE)
		SYNAPTICS_DEBUG_MSG("\n");
	
	/* ignore irrelevant timer interrupt during IC power on */
	if (likely(ts->ic_init)) {
		queue_delayed_work(synaptics_wq, &ts->work, 0);
		hrtimer_start(&ts->timer, ktime_set(0, ts->pdata->report_period), HRTIMER_MODE_REL);
	}

	return HRTIMER_NORESTART;
}

static void read_page_description_table(struct synaptics_ts_data *ts)
{
	/* Read config data */
	int ret = 0;
	ts_function_descriptor buffer;
	unsigned short u_address;
	u8 page_num;

	if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE)
		SYNAPTICS_DEBUG_MSG("\n");
	
	memset(&buffer, 0x0, sizeof(ts_function_descriptor));

	ts->common_dsc.id = 0;
	ts->finger_dsc.id = 0;
	ts->button_dsc.id = 0;
	ts->analog_dsc.id = 0;
	ts->flash_dsc.id = 0;

	for(page_num = 0; page_num < PAGE_MAX_NUM; page_num++) {
		ret = i2c_smbus_write_byte_data(ts->client, PAGE_SELECT_REG, page_num);
		if (ret < 0)
			SYNAPTICS_ERR_MSG("PAGE_SELECT_REG write fail\n");
		
		for(u_address = DESCRIPTION_TABLE_START; u_address > 10; u_address -= sizeof(ts_function_descriptor)) {
			ret = synaptics_ts_read(ts->client, u_address, sizeof(buffer), (unsigned char *)&buffer);
			if (ret < 0) {
				SYNAPTICS_ERR_MSG("ts_function_descriptor read fail\n");
				return;
			}

			if (buffer.id == 0)
				break;

			switch (buffer.id) {
			case RMI_DEVICE_CONTROL:
				ts->common_dsc = buffer;
				ts->common_page = page_num;
				break;
			case TOUCHPAD_SENSORS:
				ts->finger_dsc = buffer;
				ts->finger_page= page_num;
				break;
			case CAPACITIVE_BUTTON_SENSORS:
				ts->button_dsc = buffer;
				ts->button_page= page_num;
				break;
			case ANALOG_CONTROL:
				ts->analog_dsc = buffer;
				ts->analog_page = page_num;
				break;
			case FLASH_MEMORY_MANAGEMENT:
				ts->flash_dsc = buffer;
				ts->flash_page = page_num;
				break;
			}
		}
	}

	ret = i2c_smbus_write_byte_data(ts->client, PAGE_SELECT_REG, 0x00);
	if (ret < 0)
		SYNAPTICS_ERR_MSG("PAGE_SELECT_REG write fail\n");
}

static void synaptics_ts_button_lock_work_func(struct work_struct *button_lock_work)
{
	struct synaptics_ts_data *ts = container_of(to_delayed_work(button_lock_work), struct synaptics_ts_data, button_lock_work);
	int ret;

	ts->curr_int_mask = 0xFF;
	if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_INT_STATUS)
		SYNAPTICS_DEBUG_MSG("Interrupt mask 0x%x\n", ts->curr_int_mask);
	ret = i2c_smbus_write_byte_data(ts->client, INTERRUPT_ENABLE_REG, ts->curr_int_mask);
}

static void synaptics_ts_work_func(struct work_struct *work)
{
	struct synaptics_ts_data *ts = container_of(to_delayed_work(work), struct synaptics_ts_data, work);
	int ret = 0;
	int width_max = 0, width_min = 0;
	int width_orientation = 0;
	unsigned int f_counter = 0;
	unsigned int b_counter = 0;
	unsigned int reg_num = 0;
	unsigned int finger_order = 0;
	u8 temp;
	char report_enable = 0;
	ts_sensor_ctrl ts_reg_ctrl;
	ts_sensor_data ts_reg_data;
	ts_finger_data curr_ts_data;
	u8 resolution[2] = {0};
	int index = 0;
#if defined(LGE_USE_SYNAPTICS_FW_UPGRADE)	
	u8 device_status = 0;
	u8 flash_control = 0;
#endif	

	if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE)
		SYNAPTICS_DEBUG_MSG("\n");

	memset(&ts_reg_ctrl, 0x0, sizeof(ts_sensor_ctrl));
	memset(&ts_reg_data, 0x0, sizeof(ts_sensor_data));
	memset(&curr_ts_data, 0x0, sizeof(ts_finger_data));
	pressure_zero = 0;

	if (ts->ic_init) {
		/* read device status */
		ret = synaptics_ts_read(ts->client, DEVICE_STATUS_REG, sizeof(unsigned char), (u8 *) &ts_reg_ctrl.device_status_reg);
		if (ret < 0) {
			SYNAPTICS_ERR_MSG("DEVICE_STATUS_REG read fail\n");
			goto exit_work;
		}

		/* read interrupt status */
		ret = synaptics_ts_read(ts->client, INTERRUPT_STATUS_REG, sizeof(unsigned char), (u8 *) &ts_reg_ctrl.interrupt_status_reg);
		if (ret < 0) {
			SYNAPTICS_ERR_MSG("INTERRUPT_STATUS_REG read fail\n");
			goto exit_work;
		} else {
			if (!(synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_INT_INTERVAL)
					&& (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_INT_ISR_DELAY)
					&& !(synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FINGER_HANDLE_TIME)
					&& !(synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_BUTTON_HANDLE_TIME)
					&& (atomic_read(&ts->int_delay.ready) == 1)) {
				ts->int_delay.end = cpu_clock(smp_processor_id());
				ts->int_delay.result_t = ts->int_delay.end -ts->int_delay.start;
				ts->int_delay.rem = do_div(ts->int_delay.result_t , 1000000);
				SYNAPTICS_DEBUG_MSG("Touch IC interrupt line clear time < %3lu.%03lu\n", (unsigned long)ts->int_delay.result_t, ts->int_delay.rem/1000);
			}
		}

		/* read button data */
		if (likely(ts->button_dsc.id != 0)) {
			ret = synaptics_ts_page_data_read(ts->client, BUTTON_PAGE, BUTTON_DATA_REG, sizeof(ts_reg_ctrl.button_data_reg), (u8 *)&ts_reg_ctrl.button_data_reg);
			if (ret < 0) {
				SYNAPTICS_ERR_MSG("BUTTON_DATA_REG read fail\n");
				goto exit_work;
			}
		}

		/* read finger state & finger data register */
		ret = synaptics_ts_read(ts->client, FINGER_STATE_REG, sizeof(ts_reg_data) - ((MAX_NUM_OF_FINGER - ts->pdata->num_of_finger) * NUM_OF_EACH_FINGER_DATA_REG), (u8 *) &ts_reg_data.finger_state_reg[0]);
		
		if (ret < 0) {
			SYNAPTICS_ERR_MSG("FINGER_STATE_REG read fail\n");
			goto exit_work;
		}

		/* Palm check */
		ret = synaptics_ts_read(ts->client, TWO_D_EXTEND_STATUS, 1, &temp);
		if (ret < 0) {
			SYNAPTICS_ERR_MSG("TWO_D_EXTEND_STATUS read fail\n");
			goto exit_work;
		}
		old_ts_data.palm = ts->pre_ts_data.palm;
		ts->pre_ts_data.palm = temp & 0x2;
		/* ESD damage check */
		if ((ts_reg_ctrl.device_status_reg & DEVICE_FAILURE_MASK)== DEVICE_FAILURE_MASK) {
			SYNAPTICS_ERR_MSG("ESD damage occured. Reset Touch IC\n");
			ts->ic_init = 0;
			synaptics_ts_hard_reset(ts);
			return;
		}

		/* Internal reset check */
		if (((ts_reg_ctrl.device_status_reg & DEVICE_STATUS_REG_UNCONFIGURED) >> 7) == 1) {
			SYNAPTICS_ERR_MSG("Touch IC resetted internally. Reconfigure register setting\n");
			ts->ic_init = 0;
			queue_delayed_work(synaptics_wq, &ts->work, 0);
			return;
		}

		/* finger & button interrupt has no correlation */
		if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_INT_STATUS)
			SYNAPTICS_DEBUG_MSG("Interrupt status register: 0x%x\n", ts_reg_ctrl.interrupt_status_reg);

		ret = synaptics_ts_read(ts->client, INTERRUPT_ENABLE_REG, sizeof(ts->curr_int_mask), (u8 *) &ts->curr_int_mask);

		if (ts_reg_ctrl.interrupt_status_reg & ts->int_status_reg_asb0_bit
			&& ts->curr_int_mask & ts->int_status_reg_asb0_bit) {	/* Finger */
			for(f_counter = 0; f_counter < ts->pdata->num_of_finger; f_counter++) {
				reg_num = f_counter/4;
				finger_order = f_counter%4;

				if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FINGER_REG) {
					if (finger_order == 0)
						SYNAPTICS_DEBUG_MSG("Finger status register%d: 0x%x\n",
								reg_num, ts_reg_data.finger_state_reg[reg_num]);
				}

				if (((ts_reg_data.finger_state_reg[reg_num]>>(finger_order*2))
						& FINGER_STATE_MASK) == 1)
				{
					curr_ts_data.pos_x[f_counter] =
							(int)GET_X_POSITION(ts_reg_data.finger_data[f_counter][REG_X_POSITION],
									ts_reg_data.finger_data[f_counter][REG_YX_POSITION]);
					curr_ts_data.pos_y[f_counter] =
							(int)GET_Y_POSITION(ts_reg_data.finger_data[f_counter][REG_Y_POSITION],
									ts_reg_data.finger_data[f_counter][REG_YX_POSITION]);

					if (((ts_reg_data.finger_data[f_counter][REG_WY_WX] & 0xF0) >> 4)
							> (ts_reg_data.finger_data[f_counter][REG_WY_WX] & 0x0F)) {
						width_max = (ts_reg_data.finger_data[f_counter][REG_WY_WX] & 0xF0) >> 4;
						width_min = ts_reg_data.finger_data[f_counter][REG_WY_WX] & 0x0F;
						width_orientation = 0;
					} else {
						width_max = ts_reg_data.finger_data[f_counter][REG_WY_WX] & 0x0F;
						width_min = (ts_reg_data.finger_data[f_counter][REG_WY_WX] & 0xF0) >> 4;
						width_orientation = 1;
					} 

					curr_ts_data.pressure[f_counter] = ts_reg_data.finger_data[f_counter][REG_Z];		
					
					if (ts->pdata->use_ghost_detection) {
						if (curr_ts_data.pressure[f_counter] == 0)
							pressure_zero = 1;
					}
#if defined(MT_PROTOCOL_A)
					input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, f_counter);
#else
					input_mt_slot(ts->input_dev, f_counter);
					input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);
#endif

					input_report_abs(ts->input_dev, ABS_MT_POSITION_X, curr_ts_data.pos_x[f_counter]);
					input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, curr_ts_data.pos_y[f_counter]);
					input_report_abs(ts->input_dev, ABS_MT_PRESSURE, curr_ts_data.pressure[f_counter]);
					input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, width_max);
					input_report_abs(ts->input_dev, ABS_MT_WIDTH_MINOR, width_min);
					input_report_abs(ts->input_dev, ABS_MT_ORIENTATION, width_orientation);

					report_enable = 1;
					if (ts->finger_prestate[f_counter] == TOUCH_RELEASED) {
						finger_oldstate[f_counter] = ts->finger_prestate[f_counter];
						ts->finger_prestate[f_counter] = TOUCH_PRESSED;
//						if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FINGER_STATUS)
							SYNAPTICS_INFO_MSG("[Touch] Finger%d (%d,%d)pressed\n", f_counter, curr_ts_data.pos_x[f_counter], curr_ts_data.pos_y[f_counter]);

						//LG_CHANGE_S : 20130805 ticklewind.kim@lge.com
						for (b_counter = 0; b_counter < ts->pdata->num_of_button; b_counter++) 
							if ( ts->button_prestate[b_counter] == TOUCH_PRESSED )
							{
								input_report_key(ts->input_dev, ts->pdata->button[b_counter], BUTTON_CANCLED);
								if (likely(synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_BUTTON_STATUS)) 
									SYNAPTICS_INFO_MSG("Touch KEY[%d] is canceled\n", b_counter);
								ts->button_prestate[b_counter] = TOUCH_RELEASED;
							}
						//LG_CHANGE_E : 20130805 ticklewind.kim@lge.com			

						/* button interrupt disable when first finger pressed */
						if (ts->curr_int_mask & ts->int_status_reg_button_bit) {
							ret = cancel_delayed_work_sync(&ts->button_lock_work);

							ts->curr_int_mask = ts->curr_int_mask & ~(ts->int_status_reg_button_bit);
							if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_INT_STATUS)
								SYNAPTICS_DEBUG_MSG("Interrupt mask 0x%x\n", ts->curr_int_mask);
							ret = i2c_smbus_write_byte_data(ts->client, INTERRUPT_ENABLE_REG, ts->curr_int_mask);
						}						
						
					}

					if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FINGER_POSITION)
						SYNAPTICS_INFO_MSG("x:%4d, y:%4d, pressure:%4d\n", curr_ts_data.pos_x[f_counter], curr_ts_data.pos_y[f_counter], curr_ts_data.pressure[f_counter]);

					old_ts_data.pos_x[f_counter]= ts->pre_ts_data.pos_x[f_counter];
					old_ts_data.pos_y[f_counter]= ts->pre_ts_data.pos_y[f_counter];
					old_ts_data.pressure[f_counter]= ts->pre_ts_data.pressure[f_counter];
					ts->pre_ts_data.pos_x[f_counter] = curr_ts_data.pos_x[f_counter];
					ts->pre_ts_data.pos_y[f_counter] = curr_ts_data.pos_y[f_counter];
					ts->pre_ts_data.pressure[f_counter] = curr_ts_data.pressure[f_counter];
					index++;
				} else if (ts->finger_prestate[f_counter] == TOUCH_PRESSED) {
					if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FINGER_STATUS)
						SYNAPTICS_INFO_MSG("[Touch] Finger%d (%d, %d)released\n", f_counter, ts->pre_ts_data.pos_x[f_counter], ts->pre_ts_data.pos_y[f_counter]);


#if !defined(MT_PROTOCOL_A)
					input_mt_slot(ts->input_dev, f_counter);
					input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
#endif

					finger_oldstate[f_counter] = ts->finger_prestate[f_counter];
					ts->finger_prestate[f_counter] = TOUCH_RELEASED;

					report_enable = 1;

					if (ts_reg_data.finger_state_reg[0] == 0
							&& ts_reg_data.finger_state_reg[1] == 0
							/*&& ts_reg_data.finger_state_reg[2] == 0*/) {
						/* button interrupt enable when all finger released */
						queue_delayed_work(synaptics_wq, &ts->button_lock_work, msecs_to_jiffies(200));
					}

					ts->pre_ts_data.pos_x[f_counter] = 0;
					ts->pre_ts_data.pos_y[f_counter] = 0;
				}
#if defined(MT_PROTOCOL_A)
				if (report_enable)
					input_mt_sync(ts->input_dev);
#endif
				report_enable = 0;
			}
			curr_ts_data.total_num = index;
			old_ts_data.total_num = ts->pre_ts_data.total_num;
			ts->pre_ts_data.total_num = curr_ts_data.total_num;

			input_sync(ts->input_dev);

			if (!(synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_INT_INTERVAL)
					&& !(synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_INT_ISR_DELAY)
					&& (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FINGER_HANDLE_TIME)
					&& !(synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_BUTTON_HANDLE_TIME)) {
				if (atomic_read(&ts->int_delay.ready) == 1) {
					ts->int_delay.end = cpu_clock(smp_processor_id());
					ts->int_delay.result_t = ts->int_delay.end - ts->int_delay.start;
					ts->int_delay.rem = do_div(ts->int_delay.result_t , 1000000);
					SYNAPTICS_DEBUG_MSG("Touch Finger data report done time < %3lu.%03lu\n", (unsigned long)ts->int_delay.result_t, ts->int_delay.rem/1000);
				}
			}
		}

		ret = synaptics_ts_read(ts->client, INTERRUPT_ENABLE_REG, sizeof(ts->curr_int_mask), (u8 *) &ts->curr_int_mask);

		if (likely(ts->button_dsc.id != 0)) {
			if (ts_reg_ctrl.interrupt_status_reg & ts->int_status_reg_button_bit
				&& ts->curr_int_mask & ts->int_status_reg_button_bit) { /* Button */

				if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_BUTTON_REG)
					SYNAPTICS_DEBUG_MSG("Button register: 0x%x\n", ts_reg_ctrl.button_data_reg);

				for (b_counter = 0; b_counter < ts->pdata->num_of_button; b_counter++) {
					if ( ((ts_reg_ctrl.button_data_reg >> b_counter) & 0x1) == 1 && (ts->button_prestate[b_counter] == TOUCH_RELEASED)) {
						button_oldstate[b_counter] = ts->button_prestate[b_counter];
						

						ts->button_prestate[b_counter] = TOUCH_PRESSED; /* pressed */
						report_enable = 1;

						/* finger interrupt disable when button pressed */
						/*	
						if (ts->curr_int_mask & ts->int_status_reg_asb0_bit) {
							ret = cancel_delayed_work_sync(&ts->button_lock_work);

							ts->curr_int_mask = ts->curr_int_mask & ~(ts->int_status_reg_asb0_bit);
							if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_INT_STATUS)
								SYNAPTICS_DEBUG_MSG("Interrupt mask 0x%x\n", ts->curr_int_mask);
								ret = i2c_smbus_write_byte_data(ts->client, INTERRUPT_ENABLE_REG, ts->curr_int_mask);
						}
						*/
						

						
					} else if(((ts_reg_ctrl.button_data_reg >> b_counter) & 0x1) == 0 && (ts->button_prestate[b_counter] == TOUCH_PRESSED)) {
						button_oldstate[b_counter] = ts->button_prestate[b_counter];
						ts->button_prestate[b_counter] = TOUCH_RELEASED; /* released */
						report_enable = 1;
					}

					if (report_enable)
						input_report_key(ts->input_dev, ts->pdata->button[b_counter], ts->button_prestate[b_counter]);

					if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_BUTTON_STATUS) {
						if (report_enable)
							SYNAPTICS_INFO_MSG("Touch KEY%d(code:%d) is %s\n", b_counter, ts->pdata->button[b_counter], ts->button_prestate[b_counter]?"pressed":"released");
					}
					report_enable = 0;
				}

				input_sync(ts->input_dev);

				/* finger interrupt enable when all button released */
				if(ts_reg_ctrl.button_data_reg == 0) {
					queue_delayed_work(synaptics_wq, &ts->button_lock_work, msecs_to_jiffies(200));
				}

				if (!(synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_INT_INTERVAL)
						&& !(synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_INT_ISR_DELAY)
						&& !(synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FINGER_HANDLE_TIME)
						&& (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_BUTTON_HANDLE_TIME)) {
					if (atomic_read(&ts->int_delay.ready) == 1) {
						ts->int_delay.end = cpu_clock(smp_processor_id());
						ts->int_delay.result_t = ts->int_delay.end - ts->int_delay.start;
						ts->int_delay.rem = do_div(ts->int_delay.result_t , 1000000);
						SYNAPTICS_DEBUG_MSG("Touch Button data report done time < %3lu.%03lu\n",
								(unsigned long)ts->int_delay.result_t, ts->int_delay.rem/1000);
					}
				}
			}
		}

		if (ts->pdata->use_ghost_detection) {
			ret = ghost_detect_solution();
			if(ret == NEED_TO_OUT)
				goto exit_work;
			else if(ret == NEED_TO_INIT) {
				release_all_ts_event();
				synaptics_ts_hard_reset(ts);
				synaptics_ts_ic_ctrl(ts->client, IC_CTRL_REPORT_MODE, CONTINUOUS_REPORT_MODE);
				force_continuous_mode = 1;
				ghost_detection = 0;
				ghost_detection_count = 0;
				do_gettimeofday(&t_ex_debug[TIME_EX_INIT_TIME]);
				return;
			}
		}

exit_work:
		atomic_dec(&ts->interrupt_handled);
		atomic_inc(&ts->int_delay.ready);

		/* Safety code: Check interrupt line status */
		if (ts->pdata->use_irq != 0) {
			if (mt_get_gpio_in(ts->pdata->int_gpio) != 1
					&& atomic_read(&ts->interrupt_handled) == 0) {
				/* interrupt line to high by Touch IC */
				ret = synaptics_ts_read(ts->client, INTERRUPT_STATUS_REG, 1, &temp);
				if (ret < 0)
					SYNAPTICS_ERR_MSG("INTERRUPT_STATUS_REG read fail\n");

				/* FIXME:
				 * 	We haven't seen this error case.
				 *	So, can not sure it is OK or have to force re-scanning touch IC.
				 */
				SYNAPTICS_ERR_MSG("WARNING - Safety: Interrupt line isn't set high on time cause unexpected incident\n");
			}
		}

		if ((synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_INT_INTERVAL)
				|| (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_INT_ISR_DELAY)
				|| (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FINGER_HANDLE_TIME)
				|| (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_BUTTON_HANDLE_TIME)) {
			/* clear when all event is released */
			if (likely(ts->button_dsc.id == 0)) {
				if (ts_reg_data.finger_state_reg[0] == 0
						&& ts_reg_data.finger_state_reg[1] == 0
						/*&& ts_reg_data.finger_state_reg[2] == 0*/
						&& ts_reg_ctrl.button_data_reg == 0
						&& atomic_read(&ts->interrupt_handled) == 0)
					atomic_set(&ts->int_delay.ready, 0);
			} else {
				if (ts_reg_data.finger_state_reg[0] == 0
						&& ts_reg_data.finger_state_reg[1] == 0
						/*&& ts_reg_data.finger_state_reg[2] == 0*/
						&& atomic_read(&ts->interrupt_handled) == 0)
					atomic_set(&ts->int_delay.ready, 0);
			}
		}
	} else {
		/* Touch IC init */
		if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE)
			SYNAPTICS_DEBUG_MSG("Touch IC init vale setting\n");

		/* check device existence using I2C read */
		if (!ts->is_probed) {
			/* find register map */
			read_page_description_table(ts);

			/* define button & finger interrupt maks */
			if (likely(ts->button_dsc.id != 0)) {
				ts->int_status_reg_asb0_bit = 0x4;
				ts->int_status_reg_button_bit = 0x10;
			} else {
				ts->int_status_reg_asb0_bit = 0x4;
			}

			ret = synaptics_ts_read(ts->client, MANUFACTURER_ID_REG, 1, &ts->manufcturer_id);
			if (ret < 0)
				SYNAPTICS_ERR_MSG("Manufcturer ID read fail\n");
			SYNAPTICS_INFO_MSG("Manufcturer ID: %d\n", ts->manufcturer_id);

			ret = synaptics_ts_read(ts->client, FW_REVISION_REG, 1, &ts->fw_rev);
			if (ret < 0)
				SYNAPTICS_ERR_MSG("FW revision read fail\n");
			SYNAPTICS_INFO_MSG("FW revision: %d\n", ts->fw_rev);

			ret = synaptics_ts_read(ts->client, PRODUCT_ID_REG, 10, &ts->product_id[0]); //depend on kernel in the file (i2c.h)
			if (ret < 0)
				SYNAPTICS_ERR_MSG("Product ID read fail\n");
			SYNAPTICS_INFO_MSG("Product ID: %s\n", ts->product_id);

			ret = synaptics_ts_read(ts->client, FLASH_CONFIG_ID_REG, sizeof(ts->config_id)-1, &ts->config_id[0]);
			if (ret < 0)
				SYNAPTICS_ERR_MSG("Config ID read fail\n");
			SYNAPTICS_INFO_MSG("Config ID: %s\n", ts->config_id);

			ret = synaptics_ts_read(ts->client, SENSOR_MAX_X_POS, sizeof(resolution), resolution);
			if (ret < 0)
				SYNAPTICS_ERR_MSG("SENSOR_MAX_X_POS read fail\n");
			SYNAPTICS_INFO_MSG("SENSOR_MAX_X=%d\n", (int)(resolution[1] << 8 | resolution[0]));
			
			ret = synaptics_ts_read(ts->client, SENSOR_MAX_Y_POS, sizeof(resolution), resolution);
			if (ret < 0)
				SYNAPTICS_ERR_MSG("SENSOR_MAX_Y_POS read fail\n");
			SYNAPTICS_INFO_MSG("SENSOR_MAX_Y=%d\n", (int)(resolution[1] << 8 | resolution[0]));

#if defined(LGE_USE_SYNAPTICS_FW_UPGRADE)
			ts->fw_start = (unsigned char *)&SynaFirmware[0];
			ts->fw_size = sizeof(SynaFirmware);

			strncpy(ts->fw_config_id, &SynaFirmware[0xb100], 4);
			strncpy(ts->fw_product_id, &SynaFirmware[0x0040], 6);

			SYNAPTICS_INFO_MSG("fw_rev:%d, fw_config_id:%s, fw_product_id:%s\n", ts->fw_start[31], ts->fw_config_id, ts->fw_product_id);

			ret = synaptics_ts_read(ts->client, FLASH_CONTROL_REG, sizeof(flash_control), &flash_control);
			if (ret < 0)
				SYNAPTICS_ERR_MSG("FLASH_CONTROL_REG read fail\n");

			ret = synaptics_ts_read(ts->client, DEVICE_STATUS_REG, sizeof(device_status), &device_status);
			if (ret < 0)
				SYNAPTICS_ERR_MSG("DEVICE_STATUS_REG read fail\n");

			/* Firmware has a problem, so we should firmware-upgrade */
			if(device_status & DEVICE_STATUS_FLASH_PROG || (device_status & DEVICE_CRC_ERROR_MASK) != 0 || (flash_control & FLASH_STATUS_MASK) != 0) {
				SYNAPTICS_ERR_MSG("Firmware has a unknown-problem, so it needs firmware-upgrade.\n");
				SYNAPTICS_ERR_MSG("FLASH_CONTROL[%x] DEVICE_STATUS_REG[%x]\n", (u32)flash_control, (u32)device_status);
				SYNAPTICS_ERR_MSG("FW-upgrade Force Rework.\n");

				/* firmware version info change by force for rework */
				snprintf(ts->fw_config_id, sizeof(ts->fw_config_id), "ERR");
				ts->fw_force_upgrade = 1;
			}

			ret = synaptics_ts_need_fw_upgrade();
			if(ret != 0) {
				synaptics_ts_fw_upgrade(ts, ts->fw_path);
				return;
			}
#endif
			ts->is_probed = 1;
		}

		ret = i2c_smbus_write_byte_data(ts->client, DEVICE_CONTROL_REG, (DEVICE_CONTROL_REG_NOSLEEP | DEVICE_CONTROL_REG_CONFIGURED));
		if (ret < 0)
			SYNAPTICS_ERR_MSG("DEVICE_CONTROL_REG write fail\n");

		ret = synaptics_ts_read(ts->client, DEVICE_CONTROL_REG, 1, &temp);
		if (ret < 0)
			SYNAPTICS_ERR_MSG("DEVICE_CONTROL_REG read fail\n");
		SYNAPTICS_INFO_MSG("DEVICE_CONTROL_REG = %x\n", temp);

		ret = synaptics_ts_read(ts->client, INTERRUPT_ENABLE_REG, 1, &temp);
		if (ret < 0)
			SYNAPTICS_ERR_MSG("INTERRUPT_ENABLE_REG read fail\n");
		SYNAPTICS_INFO_MSG("INTERRUPT_ENABLE_REG = %x\n", temp);

		ret = i2c_smbus_write_byte_data(ts->client, INTERRUPT_ENABLE_REG, (temp | ts->int_status_reg_asb0_bit | ts->int_status_reg_button_bit));
		if (ret < 0)
			SYNAPTICS_ERR_MSG("GESTURE_ENABLE_1_REG write fail\n");

		ret = i2c_smbus_write_byte_data(ts->client, TWO_D_REPORTING_MODE, (ts->pdata->report_mode | ABS_MODE | REPORT_BEYOND_CLIP));
		if (ret < 0)
			SYNAPTICS_ERR_MSG("TWO_D_REPORTING_MODE write fail\n");

		ret = i2c_smbus_write_byte_data(ts->client, DELTA_X_THRESH_REG, (u8)ts->pdata->delta_pos_threshold);
		if (ret < 0)
			SYNAPTICS_ERR_MSG("DELTA_X_THRESH_REG write fail\n");
		
		ret = i2c_smbus_write_byte_data(ts->client, DELTA_Y_THRESH_REG, (u8)ts->pdata->delta_pos_threshold);
		if (ret < 0)
			SYNAPTICS_ERR_MSG("DELTA_Y_THRESH_REG write fail\n");

		ret = synaptics_ts_read(ts->client, SMALL_OBJECT_DETECTION_TUNNING_REG, 1, &temp);
		if (ret < 0)
			SYNAPTICS_ERR_MSG("SMALL_OBJECT_DETECTION_TUNNING_REG read fail\n");
		SYNAPTICS_INFO_MSG("SMALL_OBJECT_DETECTION_TUNNING_REG = %x\n", temp);

		ret = synaptics_ts_read(ts->client, INTERRUPT_STATUS_REG, 1, &temp);
		if (ret < 0)
			SYNAPTICS_ERR_MSG("INTERRUPT_STATUS_REG read fail\n");
		if (ts->pdata->use_ghost_detection) {
			synaptics_ts_ic_ctrl(ts->client, IC_CTRL_REPORT_MODE, REDUCED_REPORT_MODE);
			force_continuous_mode = 1;
			ghost_detection = 0;
			ghost_detection_count = 0;
			do_gettimeofday(&t_ex_debug[TIME_EX_INIT_TIME]);
		}
		ts->ic_init = 1;
	}
}

static void synaptics_eint_interrupt_handler(void)
{
	struct synaptics_ts_data *ts;

	ts = ts_hub;

	if(ts == NULL) {
		SYNAPTICS_ERR_MSG("ts is NULL\n");
		return;
	}

	if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE)
		SYNAPTICS_DEBUG_MSG(": %d\n", ts->ic_init);
	
	if (likely(ts->ic_init)) {
		if (ts->pdata->use_irq)
			mt65xx_eint_mask(ts->pdata->irq_num);

		synaptics_tpd_flag = 1;
		wake_up_interruptible(&synaptics_waiter);

		if (ts->pdata->use_irq)
			mt65xx_eint_unmask(ts->pdata->irq_num);		
	}
}

static int synaptics_touch_event_handler(void *unused)
{
	struct sched_param param = {.sched_priority = RTPM_PRIO_TPD};
	struct synaptics_ts_data *ts;
	
	sched_setscheduler(current, SCHED_RR, &param);
	
	do
	{
		set_current_state(TASK_INTERRUPTIBLE);
		wait_event_interruptible(synaptics_waiter, (synaptics_tpd_flag != 0));
		synaptics_tpd_flag = 0;
		ts = ts_hub;
		if(ts != NULL) {
			set_current_state(TASK_RUNNING);
			queue_delayed_work(synaptics_wq, &ts->work, 0);
		}

	}while(!kthread_should_stop());

	return 0;
}

static int synaptics_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct synaptics_ts_platform_data *pdata;
	struct synaptics_ts_data *ts;
	int ret = 0;
	int count = 0;
	int err = 0;
	char temp;
	struct task_struct *thread = NULL;


	if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE)
		SYNAPTICS_DEBUG_MSG("\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		SYNAPTICS_ERR_MSG("i2c functionality check error\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	pdata = client->dev.platform_data;
	if (pdata == NULL) {
		SYNAPTICS_ERR_MSG("Can not read platform data\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (ts == NULL) {
		SYNAPTICS_ERR_MSG("Can not allocate  memory\n");
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	/* Device data setting */
	ts->pdata = pdata;
	//ts->button_width = (ts->pdata->x_max - (ts->pdata->num_of_button - 1) * BUTTON_MARGIN) / ts->pdata->num_of_button;

	synaptics_ts_init_sysfs();
#if defined(LGE_USE_SYNAPTICS_FW_UPGRADE)	
	wake_lock_init(&fw_suspend_lock, WAKE_LOCK_SUSPEND, "fw_wakelock");
#endif
	INIT_DELAYED_WORK(&ts->work, synaptics_ts_work_func);
	INIT_DELAYED_WORK(&ts->button_lock_work, synaptics_ts_button_lock_work_func);
	ts->client = client;
	i2c_set_clientdata(client, ts);
	ts_hub = ts;
#if defined(LGE_USE_SYNAPTICS_F54)
	ds4_i2c_client = client;
#endif

	tpd_load_status = 1;

	synaptics_ts_reset(ts->pdata->reset_gpio);
	
	if (ts->pdata->power) {
		ret = ts->pdata->power(1);

		if (ret < 0) {
			SYNAPTICS_ERR_MSG("power on failed\n");
			goto err_power_failed;
		}
	}

	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		SYNAPTICS_ERR_MSG("Failed to allocate input device\n");
		goto err_input_dev_alloc_failed;
	}

	ts->input_dev->name = "synaptics_ts"; //driver name 

	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);
	set_bit(EV_ABS, ts->input_dev->evbit);
	set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);

	for(count = 0; count < ts->pdata->num_of_button; count++) {
		set_bit(ts->pdata->button[count], ts->input_dev->keybit);
	}

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, ts->pdata->x_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, ts->pdata->y_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 15, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MINOR, 0, 15, 0, 0);
#if defined(MT_PROTOCOL_A)
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, 10, 0, 0);
#else
	input_mt_init_slots(ts->input_dev, ts->pdata->num_of_finger);
#endif
	input_set_abs_params(ts->input_dev, ABS_MT_ORIENTATION, 0, 1, 0, 0);

	ret = input_register_device(ts->input_dev);
	if (ret < 0) {
		SYNAPTICS_ERR_MSG("Unable to register %s input device\n",
				ts->input_dev->name);
		goto err_input_register_device_failed;
	}

	/* interrupt mode */
	if (likely(ts->pdata->use_irq && ts->pdata->irq_num)) {
		if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE)
			SYNAPTICS_DEBUG_MSG("irq [%d], irqflags[0x%lx]\n", client->irq, ts->pdata->irqflags);

		thread = kthread_run(synaptics_touch_event_handler, 0, TPD_DEVICE);
		if (IS_ERR(thread)) {
        	err = PTR_ERR(thread);
        	SYNAPTICS_ERR_MSG("failed to create kernel thread: %d\n", err);
		}
		
		mt_set_gpio_mode(ts->pdata->int_gpio, GPIO_CTP_EINT_PIN_M_EINT);
	    	mt_set_gpio_dir(ts->pdata->int_gpio, GPIO_DIR_IN);
	    	mt_set_gpio_pull_enable(ts->pdata->int_gpio, GPIO_PULL_ENABLE);
	    	mt_set_gpio_pull_select(ts->pdata->int_gpio, GPIO_PULL_UP);

		mt65xx_eint_set_sens(ts->pdata->irq_num, CUST_EINT_TOUCH_PANEL_SENSITIVE);
		mt65xx_eint_set_hw_debounce(ts->pdata->irq_num, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
		mt65xx_eint_registration(ts->pdata->irq_num, CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, ts->pdata->irqflags, synaptics_eint_interrupt_handler, 1);
		mt65xx_eint_unmask(ts->pdata->irq_num);


		if (ret < 0) {
			ts->pdata->use_irq = 0;
			SYNAPTICS_ERR_MSG("request_irq failed. use polling mode\n");
		} else {
			if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE)
				SYNAPTICS_DEBUG_MSG("request_irq succeed\n");
		}
	} else {
		ts->pdata->use_irq = 0;
	}

	/* using hrtimer case of polling mode */
	if (unlikely(!ts->pdata->use_irq)) {
		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = synaptics_ts_timer_func;
		hrtimer_start(&ts->timer, ktime_set(0, (ts->pdata->report_period * 2) + (ts->pdata->ic_booting_delay*1000000)), HRTIMER_MODE_REL);
	}

	if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE)
		SYNAPTICS_DEBUG_MSG("Start touchscreen %s in %s mode\n", ts->input_dev->name, ts->pdata->use_irq ? "interrupt" : "polling");

	/* Touch I2C sanity check */
	msleep(ts->pdata->ic_booting_delay);
	ret = synaptics_ts_read(ts->client, DESCRIPTION_TABLE_START, 1, &temp);
	if (ret < 0) {
		SYNAPTICS_ERR_MSG("ts_function_descriptor read fail\n");
		goto err_input_register_device_failed;
	}

	/* Touch IC init setting */
	queue_delayed_work(synaptics_wq, &ts->work, 0);
	return 0;

err_input_register_device_failed:
	input_free_device(ts->input_dev);
err_input_dev_alloc_failed:
	if (ts->pdata->power)
		ts->pdata->power(0);
err_power_failed:
	kfree(ts);
	ts_hub = NULL;
err_alloc_data_failed:
err_check_functionality_failed:
	return ret;
}

static int synaptics_ts_remove(struct i2c_client *client)
{
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);

	if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE)
		SYNAPTICS_DEBUG_MSG("\n");

	if (ts->pdata->use_irq) {
		//free_irq(client->irq, ts);
	}
	else {
		hrtimer_cancel(&ts->timer);
	}

	input_unregister_device(ts->input_dev);
	kfree(ts);

	return 0;
}

static void synaptics_ts_suspend_func(struct synaptics_ts_data *ts)
{

	int ret = 0;
	unsigned int f_counter = 0;
	unsigned int b_counter = 0;
	char report_enable = 0;

	if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE)
		SYNAPTICS_DEBUG_MSG("\n");

	if (ts->pdata->use_irq)
		mt65xx_eint_mask(ts->pdata->irq_num);
	else
		hrtimer_cancel(&ts->timer);

	ret = cancel_delayed_work_sync(&ts->work);

	ret = i2c_smbus_write_byte_data(ts->client, DEVICE_CONTROL_REG, DEVICE_CONTROL_REG_SLEEP); /* sleep */

	/* Ghost finger & missed release event defense code
	 * 	Release report if we have not released event until suspend
	 */

	/* Finger check */
	for(f_counter = 0; f_counter < ts->pdata->num_of_finger; f_counter++) {
		if (ts->finger_prestate[f_counter] == TOUCH_PRESSED) {
#if defined(MT_PROTOCOL_A)
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, ts->pre_ts_data.pos_x[f_counter]);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, ts->pre_ts_data.pos_y[f_counter]);
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE, TOUCH_RELEASED);
			input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, TOUCH_RELEASED);

#else
			input_mt_slot(ts->input_dev, f_counter);
                        input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
#endif

			report_enable = 1;
		}
	}

	/* Button check */
	for(b_counter = 0; b_counter < ts->pdata->num_of_button; b_counter++) {
		if (ts->button_prestate[b_counter] == TOUCH_PRESSED) {
			report_enable = 1;
			input_report_key(ts->input_dev,
					ts->pdata->button[b_counter], TOUCH_RELEASED);
		}
	}

	/* Reset finger position data */
	memset(&ts->pre_ts_data, 0x0, sizeof(ts_finger_data));

	if (report_enable) {
		SYNAPTICS_INFO_MSG("Release all pressed event before touch power off\n");
		input_sync(ts->input_dev);

		/* Reset finger & button status data */
		memset(ts->finger_prestate, 0x0, sizeof(char) * ts->pdata->num_of_finger);
		memset(ts->button_prestate, 0x0, sizeof(char) * ts->pdata->num_of_button);
	}

	/* Reset interrupt debug time struct */
	if ((synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_INT_INTERVAL)
			|| (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_INT_ISR_DELAY)
			|| (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FINGER_HANDLE_TIME)
			|| (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_BUTTON_HANDLE_TIME))
		memset(&ts->int_delay, 0x0, sizeof(struct synaptics_ts_timestamp));

	if (ts->pdata->power) {
		ret = ts->pdata->power(0);

		if (ret < 0) {
			SYNAPTICS_ERR_MSG("power off failed\n");
		} else {
			ts->ic_init = 0;
			atomic_set(&ts->interrupt_handled, 0);
		}
	}
}

static void synaptics_ts_resume_func(struct synaptics_ts_data *ts)
{
	int ret = 0;

	if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE)
		SYNAPTICS_DEBUG_MSG("\n");

	if (ts->pdata->power) {
		ret = ts->pdata->power(1);

		if (ret < 0)
			SYNAPTICS_ERR_MSG("power on failed\n");
	}

	queue_delayed_work(synaptics_wq, &ts->work,msecs_to_jiffies(ts->pdata->ic_booting_delay));

	if ( !synaptics_tpd_keylock_flag )
	{
		if (ts->pdata->use_irq)
			mt65xx_eint_unmask(ts->pdata->irq_num);
		else
			hrtimer_start(&ts->timer, ktime_set(0, ts->pdata->report_period+(ts->pdata->ic_booting_delay*1000000)), HRTIMER_MODE_REL);
	}
}

int touch_power_control(int on)
{
	if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE)
		SYNAPTICS_DEBUG_MSG(": %d\n", on);
	
	if(on)
		hwPowerOn(MT6323_POWER_LDO_VGP2, VOL_3000, "TP");
	else
		hwPowerDown(MT6323_POWER_LDO_VGP2, "TP");
	
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void synaptics_tpd_suspend(struct early_suspend *h)
{
	struct synaptics_ts_data *ts = ts_hub;

	if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE)
		SYNAPTICS_DEBUG_MSG("\n");

	if(ts == NULL) {
		SYNAPTICS_ERR_MSG("ts is NULL\n");
		return;
	}
	
	if (ts->pdata->use_ghost_detection) {
		resume_flag = 0;
	}

	if (likely(!ts->is_downloading))
		synaptics_ts_suspend_func(ts);

	ts->is_suspended = 1;
}

static void synaptics_tpd_resume(struct early_suspend *h)
{
	struct synaptics_ts_data *ts = ts_hub;

	if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE)
		SYNAPTICS_DEBUG_MSG ("\n");

	if(ts == NULL) {
		SYNAPTICS_ERR_MSG("ts is NULL\n");
		return;
	}

	if (ts->pdata->use_ghost_detection) {
		resume_flag = 1;
		ts_rebase_count = 0;
	}

	if (likely(!ts->is_downloading))
		synaptics_ts_resume_func(ts);

	ts->is_suspended = 0;
}
#else
static int synaptics_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return 0;
}

static int synaptics_ts_resume(struct i2c_client *client)
{
	return 0;
}
#endif


static const struct i2c_device_id synaptics_ts_id[] = {
	{"mtk-tpd", 0},
	{},
};

static struct i2c_driver synaptics_ts_driver = {
	.probe = synaptics_ts_probe,
	.remove = synaptics_ts_remove,
	
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend = synaptics_ts_suspend,
	.resume = synaptics_ts_resume,
#endif
	.id_table = synaptics_ts_id,
	.driver = {
		.name = "mtk-tpd",
		.owner = THIS_MODULE,
	},
};


int synaptics_tpd_local_init(void)
{
	int ret = 0;
	
	if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE)
		SYNAPTICS_DEBUG_MSG("\n");
	
	if(i2c_add_driver(&synaptics_ts_driver)!= 0) {
		SYNAPTICS_ERR_MSG("FAIL: i2c_add_driver\n");
		return -1;
    }

	return ret;
}


static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = "synaptics_2202",
	.tpd_local_init = synaptics_tpd_local_init,
#ifdef CONFIG_HAS_EARLYSUSPEND	
	.suspend = synaptics_tpd_suspend,
	.resume = synaptics_tpd_resume,
#endif	
#ifdef TPD_HAVE_BUTTON
	.tpd_have_button = 1,
#else
	.tpd_have_button = 0,
#endif		
};

static struct synaptics_ts_platform_data l50_ts_data = {
	.use_irq = 1,
	.irqflags = CUST_EINT_TOUCH_PANEL_POLARITY,
	.irq_num = CUST_EINT_TOUCH_PANEL_NUM,
	.int_gpio = GPIO_CTP_EINT_PIN,
	.reset_gpio = GPIO_TOUCH_RESET_N,
	.power = touch_power_control,
	.ic_booting_delay = 100,		/* ms */
	.report_period = 10000000,			/* ns */
	.num_of_finger = MAX_NUM_OF_FINGER,
#if defined(LGE_USE_DOME_KEY)
	.num_of_button = 2,
	.button = {KEY_BACK,KEY_MENU},
#else
	.num_of_button = 4,
	.button = {KEY_BACK,KEY_HOMEPAGE,KEY_MENU},
#endif
	.x_max = 479,
	.y_max = 799,
	.fw_ver = 0,
	.palm_threshold = 1,
	.delta_pos_threshold = 1,
#if defined(LGE_USE_DOME_KEY)	
	.use_ghost_detection = 0,
#else
	.use_ghost_detection = 0,
#endif
	.report_mode = REDUCED_REPORT_MODE,
};

static struct i2c_board_info __initdata i2c_synaptics[] = {
	[0] = {
		I2C_BOARD_INFO("mtk-tpd", 0x20),
		.platform_data = &l50_ts_data,
	},
};


static int __devinit synaptics_ts_init(void)
{
	int ret = 0;

	if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE)
		SYNAPTICS_DEBUG_MSG("\n");

	synaptics_wq = create_singlethread_workqueue("synaptics_wq");
	if (!synaptics_wq) {
		SYNAPTICS_ERR_MSG("failed to create singlethread workqueue\n");
		return -ENOMEM;
	}
#if 1
	I2CDMABuf_va = (u8 *)dma_alloc_coherent(NULL, 4096, &I2CDMABuf_pa, GFP_KERNEL);
	if(!I2CDMABuf_va) {
		SYNAPTICS_ERR_MSG("Allocate Touch DMA I2C Buffer failed!\n");
		return ENOMEM;
	}
#endif // 1
	
#if defined(LGE_USE_SYNAPTICS_RED_ON_MTK)
	red_i2c_device_setup();
#else
 	i2c_register_board_info(1, &i2c_synaptics[0], 1);
#endif
	if(tpd_driver_add(&tpd_device_driver) < 0) {
		SYNAPTICS_ERR_MSG("failed to i2c_add_driver\n");
		destroy_workqueue(synaptics_wq);
		ret = -1;
	}
	
	return ret;
}

static void __exit synaptics_ts_exit(void)
{
	if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE)
		SYNAPTICS_DEBUG_MSG("\n");
	
	i2c_del_driver(&synaptics_ts_driver);
	tpd_driver_remove(&tpd_device_driver);

#if 1
	if(I2CDMABuf_va) {
		dma_free_coherent(NULL, 4096, I2CDMABuf_va, I2CDMABuf_pa);
		I2CDMABuf_va = NULL;
		I2CDMABuf_pa = 0;
	}
#endif // 1
	if (synaptics_wq)
		destroy_workqueue(synaptics_wq);	
}


module_init(synaptics_ts_init);
module_exit(synaptics_ts_exit);

MODULE_DESCRIPTION("Synaptics 7020 Touchscreen Driver for MTK platform");
MODULE_AUTHOR("TY Kang <taiyou.kang@lge.com>");
MODULE_LICENSE("GPL");

