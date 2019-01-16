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

/****************************************************************************
* Include Files
****************************************************************************/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/byteorder/generic.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif 
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/rtpm_prio.h>
#include <linux/proc_fs.h>
#include <linux/jiffies.h>
#include <linux/spinlock.h>
#include <linux/wakelock.h>
#include <linux/dma-mapping.h>

#include <mach/eint.h>
#include <mach/mt_wdt.h>
#include <mach/mt_gpt.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>

#include <asm/uaccess.h>
#include <cust_eint.h>

#include "tpd.h"
#include "s2200_driver.h"

#define LGE_USE_SYNAPTICS_FW_UPGRADE
#if defined(LGE_USE_SYNAPTICS_FW_UPGRADE)
#include "s2200_fw.h"
#endif

#define LGE_USE_SYNAPTICS_F54
#if defined(LGE_USE_SYNAPTICS_F54)
#include "RefCode.h"
#include "RefCode_PDTScan.h"
#endif

/****************************************************************************
* Constants / Definitions
****************************************************************************/
#define	TPD_DEV_NAME						"synaptics_2200"
#define TPD_I2C_ADDRESS						0x20

#define BUFFER_SIZE							128

#define MAX_NUM_OF_FINGER					5

#define X_POSITION							0
#define Y_POSITION							1
#define XY_POSITION							2
#define WX_WY								3
#define PRESSURE							4
#define NUM_OF_EACH_FINGER_DATA_REG			5

#define TOUCH_PRESSED						1
#define TOUCH_RELEASED						0
#define BUTTON_CANCLED						0xFF


#define PAGE_MAX_NUM						4
#define PAGE_SELECT_REG						0xFF
#define DESCRIPTION_TABLE_START				0xE9

#define RMI_DEVICE_CONTROL					0x01
#define TOUCHPAD_SENSORS					0x11
#define CAPACITIVE_BUTTON_SENSORS			0x1A
#define FLASH_MEMORY_MANAGEMENT				0x34

/* Function $01 (RMI_DEVICE_CONTROL) */
#define MANUFACTURER_ID_REG					(device_control.query_base)
#define FW_REVISION_REG						(device_control.query_base+3)
#define PRODUCT_ID_REG						(device_control.query_base+11)

#define DEVICE_CONTROL_REG					(device_control.control_base)
#define NOSLEEP_MASK						0x04
#define CONFIGURED_MASK						0x80
#define INTERRUPT_ENABLE_REG				(device_control.control_base+1)
#define ABS0_MASK							0x04
#define BUTTON_MASK							0x10

#define DEVICE_STATUS_REG					(device_control.data_base)
#define DEVICE_FAILURE_MASK					0x03
#define FW_CRC_FAILURE_MASK					0x04
#define FLASH_PROG_MASK						0x40
#define UNCONFIGURED_MASK					0x80
#define INTERRUPT_STATUS_REG				(device_control.data_base+1)

/* Function $11 (TOUCHPAD_SENSORS) */
#define TWO_D_COMMAND						(finger.command_base)

#define TWO_D_REPORT_MODE_REG				(finger.control_base)
#define CONTINUOUS_REPORTING_MODE_MASK		0x0
#define REDUCED_REPORTING_MODE_MASK			0x01
#define ABS_POS_FILTER_MASK					0x08
#define REPORT_BEYOND_CLIP_MASK				0x80
#define TWO_D_DELTA_X_THRESH_REG			(finger.control_base+2)
#define TWO_D_DELTA_Y_THRESH_REG			(finger.control_base+3)
#define DELTA_POS_THRESHOLD					1

#define TWO_D_FINGER_STATE_REG				(finger.data_base)
#define FINGER_STATE_MASK					0x03
#define TWO_D_EXTENDED_STATUS_REG			(finger.data_base+27)

/* Function $1A (CAPACITIVE_BUTTON_SENSORS) */
#define BUTTON_DATA_REG						(button.data_base)

/* Function $34 (FLASH_MEMORY_MANAGEMENT) */
#define FLASH_CONFIG_ID_REG					(flash_memory.control_base)

#define FLASH_CONTROL_REG					(flash_memory.data_base+18)
#define FLASH_STATUS_MASK					0xF0


#define TPD_HAVE_BUTTON

#ifdef TPD_HAVE_BUTTON
#ifdef LGE_USE_DOME_KEY
#define TPD_KEY_COUNT	2
static int tpd_keys_local[TPD_KEY_COUNT] = {KEY_BACK , KEY_MENU};
#else
#define TPD_KEY_COUNT	4
static int tpd_keys_local[TPD_KEY_COUNT] = {KEY_BACK, KEY_HOMEPAGE, KEY_MENU};
#endif
#endif


//#define ONLY_S2202_RESET_PIN


/****************************************************************************
* Macros
****************************************************************************/
#define GET_X_POSITION(high, low)		((int)(high<<4)|(int)(low&0x0F))
#define GET_Y_POSITION(high, low)		((int)(high<<4)|(int)((low&0xF0)>>4))

#define get_time_interval(a,b)		a>=b ? a-b : 1000000+a-b
#define jitter_abs(x)				(x > 0 ? x : -x)
#define jitter_sub(x, y)			(x > y ? x - y : y - x)

/****************************************************************************
* Type Definitions
****************************************************************************/
typedef struct {
	u8 query_base;
	u8 command_base;
	u8 control_base;
	u8 data_base;
	u8 int_source_count;
	u8 function_exist;
} function_descriptor;

typedef struct {
	unsigned char finger_state[2];
	unsigned char finger_data[MAX_NUM_OF_FINGER][NUM_OF_EACH_FINGER_DATA_REG];
} touch_sensor_data;

typedef struct {
	unsigned int pos_x[MAX_NUM_OF_FINGER];
	unsigned int pos_y[MAX_NUM_OF_FINGER];
	unsigned char pressure[MAX_NUM_OF_FINGER];
	int total_num;
	char palm;
} touch_finger_info;

enum {
	IGNORE_INTERRUPT = 100,
	NEED_TO_OUT,
	NEED_TO_INIT,
};

enum {
	IC_CTRL_CODE_NONE = 0,
	IC_CTRL_BASELINE,
	IC_CTRL_READ,
	IC_CTRL_WRITE,
	IC_CTRL_RESET_CMD,
	IC_CTRL_REPORT_MODE,
};

enum {
	BASELINE_OPEN = 0,
	BASELINE_FIX,
	BASELINE_REBASE,
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

struct st_i2c_msgs {
	struct i2c_msg *msg;
	int count;
} i2c_msgs;

/****************************************************************************
* Variables
****************************************************************************/
static function_descriptor device_control;
static function_descriptor finger;
static function_descriptor button;
static function_descriptor flash_memory;

u8 device_control_page;
u8 finger_page;
u8 button_page;
u8 flash_memory_page;

u8 button_enable_mask;

touch_finger_info pre_touch_info;
char button_prestate[TPD_KEY_COUNT];
char finger_prestate[MAX_NUM_OF_FINGER];

/* ghost finger detection */
int x_max;
int	y_max;
static int use_ghost_detection;
static int pressure_zero = 0;
static int resume_flag = 0;
static int ghost_detection = 0;
static int ghost_detection_count = 0;
static int finger_subtraction_check_count = 0;
static int force_continuous_mode = 0;
static int ts_rebase_count = 0;
static int long_press_check = 0;
static int long_press_check_count = 0;
static int button_check = 0;
static unsigned int button_press_count = 0;
static unsigned int old_pos_x = 0;
static unsigned int old_pos_y = 0;
touch_finger_info old_touch_info;
char button_oldstate[TPD_KEY_COUNT];
char finger_oldstate[MAX_NUM_OF_FINGER];
struct timeval t_ex_debug[TIME_EX_PROFILE_MAX];

#if defined(LGE_USE_SYNAPTICS_FW_UPGRADE)
char fw_path[256];
unsigned char *fw_start;
unsigned long fw_size;
bool fw_force_update;
bool download_status;		/* avoid power off during F/W upgrade */
bool suspend_status;		/* avoid power off during F/W upgrade */
struct wake_lock fw_suspend_lock;
struct device *touch_fw_dev;
#endif

#if defined(LGE_USE_SYNAPTICS_F54)
struct i2c_client *ds4_i2c_client;
static int f54_fullrawcap_mode = 0;
struct device *touch_debug_dev;
#endif

static u8* I2CDMABuf_va = NULL;
static u32 I2CDMABuf_pa = 0;

static int tpd_keylock_flag;


extern struct tpd_device *tpd;
struct i2c_client *tpd_i2c_client = NULL;
static int tpd_flag = 0;

struct class *touch_class;

static DEFINE_MUTEX(i2c_access);
static DECLARE_WAIT_QUEUE_HEAD(tpd_waiter);


/****************************************************************************
* Extern Function Prototypes
****************************************************************************/
extern int FirmwareUpgrade ( struct i2c_client *client, const char* fw_path, unsigned long fw_size, unsigned char* fw_start );


/****************************************************************************
* Local Function Prototypes
****************************************************************************/
static void touch_eint_interrupt_handler ( void );
void synaptics_initialize ( struct i2c_client *client );
static int synaptics_firmware_update ( struct i2c_client *client );


/****************************************************************************
* Platform(AP) dependent functions
****************************************************************************/
static void synaptics_setup_eint ( void )
{
	TPD_FUN ();

	/* Configure GPIO settings for external interrupt pin  */
	mt_set_gpio_dir ( GPIO_CTP_EINT_PIN, GPIO_DIR_IN );
	mt_set_gpio_mode ( GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT );
	mt_set_gpio_pull_enable ( GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE );
	mt_set_gpio_pull_select ( GPIO_CTP_EINT_PIN, GPIO_PULL_UP );

	msleep(50);

	/* Configure external interrupt settings for external interrupt pin */
	mt65xx_eint_set_sens ( CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE );
	mt65xx_eint_set_hw_debounce ( CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN );
	mt65xx_eint_registration ( CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, CUST_EINT_POLARITY_LOW, touch_eint_interrupt_handler, 1 );

	/* unmask external interrupt */
	mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
}

void synaptics_power ( unsigned int on )
{
	TPD_FUN ();

	if ( on )
	{
		hwPowerOn ( MT6323_POWER_LDO_VGP2, VOL_3000, "TP" );
		TPD_LOG ( "turned on the power ( VGP2 )\n" );
		msleep(100);
	}
	else
	{
		hwPowerDown ( MT6323_POWER_LDO_VGP2, "TP" );
		TPD_LOG ( "turned off the power ( VGP2 )\n" );
	}
}

/****************************************************************************
* Synaptics I2C  Read / Write Funtions
****************************************************************************/
int i2c_dma_write ( struct i2c_client *client, const uint8_t *buf, int len )
{
	int i = 0;
	for ( i = 0 ; i < len ; i++ )
	{
		I2CDMABuf_va[i] = buf[i];
	}

	if ( len < 8 )
	{
		client->addr = ( (client->addr & I2C_MASK_FLAG) | (I2C_ENEXT_FLAG) );
		return i2c_master_send(client, buf, len);
	}
	else
	{
		client->addr = ( ( (client->addr & (I2C_MASK_FLAG)) | (I2C_DMA_FLAG) ) | (I2C_ENEXT_FLAG)) ;
		return i2c_master_send(client, (char *)I2CDMABuf_pa, len);
	}
}

int i2c_dma_read ( struct i2c_client *client, uint8_t *buf, int len )
{
	int i = 0, ret = 0;
	if ( len < 8 )
	{
		client->addr = ( (client->addr & I2C_MASK_FLAG) | (I2C_ENEXT_FLAG) );
		return i2c_master_recv(client, buf, len);
	}
	else
	{
		client->addr = ( ( (client->addr & (I2C_MASK_FLAG)) | (I2C_DMA_FLAG) ) | (I2C_ENEXT_FLAG));
		ret = i2c_master_recv(client, (char *)I2CDMABuf_pa, len);
		if ( ret < 0 )
		{
			return ret;
		}

		for ( i = 0 ; i < len ; i++ )
		{
			buf[i] = I2CDMABuf_va[i];
		}
	}

	return ret;
}

int i2c_msg_transfer ( struct i2c_client *client, struct i2c_msg *msgs, int count )
{
	int i = 0, ret = 0;
	for ( i = 0 ; i < count ; i++ )
	{
		if ( msgs[i].flags & I2C_M_RD )
		{
			ret = i2c_dma_read(client, msgs[i].buf, msgs[i].len);
		}
		else
		{
			ret = i2c_dma_write(client, msgs[i].buf, msgs[i].len);
		}

		if ( ret < 0 )
		{
			return ret;
		}
	}

	return 0;
}

int synaptics_ts_read_f54 ( struct i2c_client *client, u8 reg, int num, u8 *buf )
{
	int message_count = ( ( num - 1 ) / BUFFER_SIZE ) + 2;
	int message_rest_count = num % BUFFER_SIZE;
	int i, data_len;

	if ( i2c_msgs.msg == NULL || i2c_msgs.count < message_count )
	{
		if ( i2c_msgs.msg != NULL )
			kfree(i2c_msgs.msg);

		i2c_msgs.msg = (struct i2c_msg*)kcalloc(message_count, sizeof(struct i2c_msg), GFP_KERNEL);
		i2c_msgs.count = message_count;
		//dev_dbg(&client->dev, "%s: Update message count %d(%d)\n",  __func__, i2c_msgs.count, message_count);
	}

	i2c_msgs.msg[0].addr = client->addr;
	i2c_msgs.msg[0].flags = 0;
	i2c_msgs.msg[0].len = 1;
	i2c_msgs.msg[0].buf = &reg;

	if ( !message_rest_count )
		message_rest_count = BUFFER_SIZE;
	for ( i = 0 ; i < message_count - 1 ; i++ )
	{
		if ( i == message_count - 1 )
			data_len = message_rest_count;
		else
			data_len = BUFFER_SIZE;

		i2c_msgs.msg[i + 1].addr = client->addr;
		i2c_msgs.msg[i + 1].flags = I2C_M_RD;
		i2c_msgs.msg[i + 1].len = data_len;
		i2c_msgs.msg[i + 1].buf = buf + BUFFER_SIZE * i;
	}

#if 1
	if ( i2c_msg_transfer(client, i2c_msgs.msg, message_count) < 0 )
	{
#else
	if (i2c_transfer(client->adapter, i2c_msgs.msg, message_count) < 0) {
#endif
		if ( printk_ratelimit() )
			TPD_ERR ( "transfer error\n" );
		return -EIO;
	}
	else
		return 0;
}

int synaptics_ts_read ( struct i2c_client *client, u8 reg, int num, u8 *buf )
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

#if 1
	if ( i2c_msg_transfer(client, msgs, 2) < 0 )
	{
#else
	if (i2c_transfer(client->adapter, msgs, 2) < 0) {
#endif
		if ( printk_ratelimit() )
			TPD_ERR ( "transfer error\n" );
		return -EIO;
	}
	else
		return 0;
}
EXPORT_SYMBOL ( synaptics_ts_read );

int synaptics_ts_write ( struct i2c_client *client, u8 reg, u8 * buf, int len )
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

	send_buf[0] = (unsigned char) reg;
	memcpy(&send_buf[1], buf, len);

#if 1
	if ( i2c_msg_transfer(client, msgs, 1) < 0 )
	{
#else
	if (i2c_transfer(client->adapter, msgs, 1) < 0) {
#endif
		if ( printk_ratelimit() )
			TPD_ERR ( "transfer error\n" );
		return -EIO;
	}
	else
		return 0;
}
EXPORT_SYMBOL ( synaptics_ts_write );

int synaptics_page_data_read ( struct i2c_client *client, u8 page, u8 reg, int size, u8 *data )
{
	int ret = 0;

	ret = synaptics_ts_write ( client, PAGE_SELECT_REG, &page, 1 );
	if ( ret < 0 )
		TPD_ERR ( "PAGE_SELECT_REG write fail\n" );

	ret = synaptics_ts_read ( client, reg, size, data );
	if ( ret < 0 )
		TPD_ERR ( "synaptics_page_data_read fail\n" );

	ret = synaptics_ts_write ( client, PAGE_SELECT_REG, &device_control_page, 1 );
	if ( ret < 0 )
		TPD_ERR ( "PAGE_SELECT_REG write fail\n" );

	return 0;
}

int synaptics_page_data_write ( struct i2c_client *client, u8 page, u8 reg, int size, u8 *data )
{
	int ret = 0;

	ret = synaptics_ts_write ( client, PAGE_SELECT_REG, &page, 1 );
	if ( ret < 0 )
		TPD_ERR ( "PAGE_SELECT_REG write fail\n" );

	ret = synaptics_ts_write ( client, reg, data, size );
	if ( ret < 0 )
		TPD_ERR ( "synaptics_page_data_write fail\n" );

	ret = synaptics_ts_write ( client, PAGE_SELECT_REG, &device_control_page, 1 );
	if ( ret < 0 )
		TPD_ERR ( "PAGE_SELECT_REG write fail\n" );

	return 0;
}

/****************************************************************************
* Touch malfunction Prevention Function
****************************************************************************/
static void synaptics_release_all_finger ( void )
{
	TPD_FUN ();

	/* Reset finger position data */
	memset(&old_touch_info, 0x0, sizeof(touch_finger_info));
	memset(&pre_touch_info, 0x0, sizeof(touch_finger_info));

	/* Reset finger & button status data */
	memset(finger_oldstate, 0x0, sizeof(char) * MAX_NUM_OF_FINGER);
	memset(button_oldstate, 0x0, sizeof(char) * TPD_KEY_COUNT);
	memset(finger_prestate, 0x0, sizeof(char) * MAX_NUM_OF_FINGER);
	memset(button_prestate, 0x0, sizeof(char) * TPD_KEY_COUNT);

	input_report_key ( tpd->dev, BTN_TOUCH, 0 );
	input_mt_sync ( tpd->dev );
	input_sync ( tpd->dev );
}

int synaptics_ic_ctrl ( struct i2c_client *client, u8 code, u16 value )
{
	u8 temp;
	TPD_LOG ( "synaptics_ic_ctrl: %d, %d\n", code, value );

	switch ( code )
	{
		case IC_CTRL_BASELINE:
			switch ( value )
			{
				case BASELINE_OPEN:
#if 0
					if (unlikely(synaptics_page_data_write(client, ANALOG_PAGE, ANALOG_CONTROL_REG, 1, FORCE_FAST_RELAXATION) < 0)) {
						SYNAPTICS_ERR_MSG("ANALOG_CONTROL_REG write fail\n");
						return -EIO;
					}

					msleep(10);

					if (unlikely(synaptics_page_data_write(client, ANALOG_PAGE, ANALOG_COMMAND_REG, 1, FORCE_UPDATE) < 0)) {
						SYNAPTICS_ERR_MSG("ANALOG_COMMAND_REG write fail\n");
						return -EIO;
					}

					if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FINGER_STATUS)
						SYNAPTICS_INFO_MSG("BASELINE_OPEN\n");
#endif
					break;

				case BASELINE_FIX:
#if 0
					if (unlikely(synaptics_page_data_write(client, ANALOG_PAGE, ANALOG_CONTROL_REG, 1, 0x00) < 0)) {
						TOUCH_ERR_MSG("ANALOG_CONTROL_REG write fail\n");
						return -EIO;
					}

					msleep(10);

					if (unlikely(synaptics_page_data_write(client, ANALOG_PAGE, ANALOG_COMMAND_REG, 1, FORCE_UPDATE) < 0)) {
						TOUCH_ERR_MSG("ANALOG_COMMAND_REG write fail\n");
						return -EIO;
					}

					if (synaptics_rmi4_i2c_debug_mask & SYNAPTICS_RMI4_I2C_DEBUG_FINGER_STATUS)
						TOUCH_INFO_MSG("BASELINE_FIX\n");
#endif
					break;

				case BASELINE_REBASE:
					/* rebase base line */
					if ( finger.function_exist != 0 )
					{
						temp = 1;
						if ( synaptics_ts_write ( client, TWO_D_COMMAND, &temp, 1 ) < 0 )
						{
							TPD_ERR ( "finger baseline reset command write fail\n" );
							return -EIO;
						}
					}
					break;

				default:
					break;
			}
			break;

		case IC_CTRL_REPORT_MODE:
			switch ( value )
			{
				case 0:   // continuous mode
					temp = REPORT_BEYOND_CLIP_MASK | ABS_POS_FILTER_MASK | CONTINUOUS_REPORTING_MODE_MASK;
					if ( synaptics_ts_write ( client, TWO_D_REPORT_MODE_REG, &temp, 1 ) < 0 )
					{
						TPD_ERR ( "TWO_D_REPORT_MODE_REG write fail\n" );
						return -EIO;
					}
					break;

				case 1:  // reduced mode
					temp = REPORT_BEYOND_CLIP_MASK | ABS_POS_FILTER_MASK | REDUCED_REPORTING_MODE_MASK;
					if ( synaptics_ts_write ( client, TWO_D_REPORT_MODE_REG, &temp, 1 ) < 0 )
					{
						TPD_ERR ( "TWO_D_REPORT_MODE_REG write fail\n" );
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

	return 0;
}

bool chk_time_interval ( struct timeval t_aft, struct timeval t_bef, int t_val )
{
	if ( t_aft.tv_sec - t_bef.tv_sec == 0 )
	{
		if ( ( get_time_interval(t_aft.tv_usec, t_bef.tv_usec) ) <= t_val )
			return true;
	}
	else if ( t_aft.tv_sec - t_bef.tv_sec == 1 )
	{
		if ( t_aft.tv_usec + 1000000 - t_bef.tv_usec <= t_val )
			return true;
	}

	return false;
}

int ghost_detect_solution ( void )
{
	int first_int_detection = 0;
	int cnt = 0, id = 0;

	if ( resume_flag )
	{
		resume_flag = 0;
		do_gettimeofday ( &t_ex_debug[TIME_EX_FIRST_INT_TIME] );

		if ( t_ex_debug[TIME_EX_FIRST_INT_TIME].tv_sec - t_ex_debug[TIME_EX_INIT_TIME].tv_sec == 0 )
		{
			if ( ( get_time_interval(t_ex_debug[TIME_EX_FIRST_INT_TIME].tv_usec, t_ex_debug[TIME_EX_INIT_TIME].tv_usec) ) <= 200000 )
			{
				first_int_detection = 1;
			}
		}
		else if ( t_ex_debug[TIME_EX_FIRST_INT_TIME].tv_sec - t_ex_debug[TIME_EX_INIT_TIME].tv_sec == 1 )
		{
			if ( t_ex_debug[TIME_EX_FIRST_INT_TIME].tv_usec + 1000000 - t_ex_debug[TIME_EX_INIT_TIME].tv_usec <= 200000 )
			{
				first_int_detection = 1;
			}
		}
	}

	if ( first_int_detection )
	{
		for ( cnt = 0 ; cnt < MAX_NUM_OF_FINGER ; cnt++ )
		{
			if ( finger_prestate[cnt] == TOUCH_PRESSED )
			{
				TPD_LOG ( "ghost detected within first input time 200ms\n" );
				ghost_detection = 1;
			}
		}
	}

	if ( pressure_zero == 1 )
	{
		TPD_LOG ( "ghost detected on pressure\n" );
		ghost_detection = 1;
	}

	if ( pre_touch_info.total_num )
	{
		if ( old_touch_info.total_num != pre_touch_info.total_num )
		{
			if ( old_touch_info.total_num <= pre_touch_info.total_num )
			{
				for ( id = 0 ; id < MAX_NUM_OF_FINGER ; id++ )
				{
					if ( finger_prestate[id] == TOUCH_PRESSED && finger_oldstate[id] == TOUCH_RELEASED )
					{
						break;
					}
				}

				if ( id < MAX_NUM_OF_FINGER )
				{
					memcpy ( &t_ex_debug[TIME_EX_PREV_PRESS_TIME], &t_ex_debug[TIME_EX_CURR_PRESS_TIME], sizeof(struct timeval) );
					do_gettimeofday ( &t_ex_debug[TIME_EX_CURR_PRESS_TIME] );

					if ( ( pre_touch_info.pos_x[id] > 0 ) && ( pre_touch_info.pos_x[id] < x_max )
						&& ( 1 <= old_touch_info.total_num ) && ( 1 <= pre_touch_info.total_num )
						&& ( jitter_sub(old_pos_x, pre_touch_info.pos_x[id]) <= 10 ) && ( jitter_sub(old_pos_y, pre_touch_info.pos_y[id]) <= 10 ) )
					{
						if ( chk_time_interval ( t_ex_debug[TIME_EX_CURR_PRESS_TIME], t_ex_debug[TIME_EX_PREV_PRESS_TIME], 50000 ) )
						{
							ghost_detection = 1;
							ghost_detection_count++;
						}
					}
					else if ( ( pre_touch_info.pos_x[id] > 0 ) && ( pre_touch_info.pos_x[id] < x_max )
						&& ( old_touch_info.total_num == 0 ) && ( pre_touch_info.total_num == 1 )
						&& ( jitter_sub(old_pos_x, pre_touch_info.pos_x[id]) <= 10 ) && ( jitter_sub(old_pos_y, pre_touch_info.pos_y[id]) <= 10 ) )
					{
						if ( chk_time_interval ( t_ex_debug[TIME_EX_CURR_PRESS_TIME], t_ex_debug[TIME_EX_PREV_PRESS_TIME], 50000 ) )
						{
							ghost_detection = 1;
						}
					}
					else if ( 5 < jitter_sub(old_touch_info.total_num, pre_touch_info.total_num) )
					{
						 ghost_detection = 1;
					}
					else
						; //do not anything

					old_pos_x = pre_touch_info.pos_x[id];
					old_pos_y = pre_touch_info.pos_y[id];
				}
			}
			else
			{
				memcpy ( &t_ex_debug[TIME_EX_PREV_PRESS_TIME], &t_ex_debug[TIME_EX_CURR_PRESS_TIME], sizeof(struct timeval) );
				do_gettimeofday ( &t_ex_debug[TIME_EX_CURR_INT_TIME] );

				if ( chk_time_interval ( t_ex_debug[TIME_EX_CURR_INT_TIME], t_ex_debug[TIME_EX_PREV_PRESS_TIME], 10999 ) )
				{
					finger_subtraction_check_count++;
				}
				else
				{
					finger_subtraction_check_count = 0;
				}

				if ( 4 < finger_subtraction_check_count )
				{
					finger_subtraction_check_count = 0;
					TPD_LOG ( "need_to_rebase finger_subtraction!!!\n" );
					/* rebase is disabled. see TD 41871*/
					//goto out_need_to_rebase;
				}
			}
		}

		if ( force_continuous_mode )
		{
			do_gettimeofday ( &t_ex_debug[TIME_EX_CURR_INT_TIME] );
			// if 20 sec have passed since resume, then return to the original report mode.
			if ( t_ex_debug[TIME_EX_CURR_INT_TIME].tv_sec - t_ex_debug[TIME_EX_INIT_TIME].tv_sec >= 10 )
			{
				if ( synaptics_ic_ctrl ( tpd_i2c_client, IC_CTRL_REPORT_MODE, REDUCED_REPORTING_MODE_MASK ) < 0 )
				{
					TPD_ERR ( "IC_CTRL_BASELINE(1) handling fail\n" );
					goto out_need_to_init;
				}
				force_continuous_mode = 0;
			}

			long_press_check = 0;

			for ( cnt = 0 ; cnt < MAX_NUM_OF_FINGER ; cnt++ )
			{
				if ( finger_prestate[cnt] == TOUCH_PRESSED )
				{
					if ( ( finger_oldstate[cnt] == TOUCH_PRESSED ) && ( jitter_sub(old_touch_info.pos_x[cnt], pre_touch_info.pos_x[cnt]) < 10 )
						&& ( jitter_sub(old_touch_info.pos_y[cnt], pre_touch_info.pos_y[cnt]) < 10 ) )
					{
						long_press_check = 1;
					}
				}
			}

			if ( long_press_check )
			{
				long_press_check_count++;
			}
			else
			{
				long_press_check_count = 0;
			}

			if ( 500 < long_press_check_count )
			{
				long_press_check_count = 0;
				TPD_LOG ( "need_to_rebase long press!!!\n" );
				goto out_need_to_rebase;
			}
		}
	}
	else if ( !pre_touch_info.total_num )
	{
		long_press_check_count = 0;
		finger_subtraction_check_count = 0;
	}

	button_check = 0;
	for ( id = 0 ; id < TPD_KEY_COUNT ; id++ )
	{
		if ( button_prestate[id] == TOUCH_PRESSED && button_oldstate[id] == TOUCH_RELEASED )
		{
			button_check = 1;
			break;
		}
	}
	if ( button_check )
	{
		if ( button_press_count == 0 )
			do_gettimeofday ( &t_ex_debug[TIME_EX_BUTTON_PRESS_START_TIME] );
		else
			do_gettimeofday ( &t_ex_debug[TIME_EX_BUTTON_PRESS_END_TIME] );

		button_press_count++;

		if ( 6 <= button_press_count )
		{
			if ( chk_time_interval ( t_ex_debug[TIME_EX_BUTTON_PRESS_END_TIME], t_ex_debug[TIME_EX_BUTTON_PRESS_START_TIME], 100000 ) )
			{
				TPD_LOG ( "need_to_rebase button zero\n" );
				goto out_need_to_rebase;
			}
			else
			{
				button_press_count = 0;
			}
		}
		else
		{
			if ( !chk_time_interval ( t_ex_debug[TIME_EX_BUTTON_PRESS_END_TIME], t_ex_debug[TIME_EX_BUTTON_PRESS_START_TIME], 100000 ) )
			{
				button_press_count = 0;
			}
		}
	}

	if ( ( ghost_detection == 1 ) && ( pre_touch_info.total_num == 0 ) && ( pre_touch_info.palm == 0 ) )
	{
		TPD_LOG ( "need_to_rebase zero\n" );
		goto out_need_to_rebase;
	}
	else if ( ( ghost_detection == 1 ) && ( 3 <= ghost_detection_count ) && ( pre_touch_info.palm == 0 ) )
	{
		TPD_LOG ( "need_to_rebase zero 3\n" );
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

		if ( ts_rebase_count == 1 )
		{
			do_gettimeofday ( &t_ex_debug[TIME_EX_FIRST_GHOST_DETECT_TIME] );

			if ( ( t_ex_debug[TIME_EX_FIRST_GHOST_DETECT_TIME].tv_sec - t_ex_debug[TIME_EX_INIT_TIME].tv_sec ) <= 3 )
			{
				ts_rebase_count = 0;
				TPD_LOG ( "need_to_init in 3 sec\n" );
				goto out_need_to_init;
			}
		}
		else
		{
			do_gettimeofday ( &t_ex_debug[TIME_EX_SECOND_GHOST_DETECT_TIME] );

			if ( ( ( t_ex_debug[TIME_EX_SECOND_GHOST_DETECT_TIME].tv_sec - t_ex_debug[TIME_EX_FIRST_GHOST_DETECT_TIME].tv_sec ) <= 5 ) )
			{
				ts_rebase_count = 0;
				TPD_LOG ( "need_to_init\n" );
				goto out_need_to_init;
			}
			else
			{
				ts_rebase_count = 1;
				memcpy ( &t_ex_debug[TIME_EX_FIRST_GHOST_DETECT_TIME], &t_ex_debug[TIME_EX_SECOND_GHOST_DETECT_TIME], sizeof(struct timeval) );
			}
		}

		synaptics_release_all_finger ();
		if ( synaptics_ic_ctrl ( tpd_i2c_client, IC_CTRL_BASELINE, BASELINE_REBASE ) < 0 )
		{
			TPD_ERR ( "IC_CTRL_REBASE(2) handling fail\n" );
		}
		TPD_LOG ( "need_to_rebase\n" );
	}
	return NEED_TO_OUT;

out_need_to_init:
	return NEED_TO_INIT;
}

#if defined(ONLY_S2202_RESET_PIN)
static void synaptics_reset_pin ( int reset_pin )
{
	TPD_FUN ();

	mt_set_gpio_mode ( reset_pin, GPIO_MODE_00 );
	mt_set_gpio_dir ( reset_pin, GPIO_DIR_OUT );
	mt_set_gpio_out ( reset_pin, GPIO_OUT_ZERO );
	msleep ( 10 );
	mt_set_gpio_mode ( reset_pin, GPIO_MODE_00 );
	mt_set_gpio_dir ( reset_pin, GPIO_DIR_OUT );
	mt_set_gpio_out ( reset_pin, GPIO_OUT_ONE );
	msleep ( 50 );
}
#endif

static void synaptics_ic_reset ( void )
{
	TPD_FUN ();

	synaptics_power ( 0 );
	mdelay(20);

#if defined(ONLY_S2202_RESET_PIN)
	synaptics_reset_pin ( GPIO_TOUCH_RESET_N );
#endif

	synaptics_power ( 1 );

	synaptics_initialize ( tpd_i2c_client );
}

static int synaptics_ic_status_check ( void )
{
	int ret;
	u8 device_status, temp;

	/* read device status */
	ret = synaptics_ts_read ( tpd_i2c_client, DEVICE_STATUS_REG, 1, (u8 *) &device_status );
	if ( ret < 0 )
	{
		TPD_ERR ( "DEVICE_STATUS_REG read fail\n" );
		return -1;
	}

	/* ESD damage check */
	if ( ( device_status & DEVICE_FAILURE_MASK ) == DEVICE_FAILURE_MASK )
	{
		TPD_ERR ( "ESD damage occured. Reset Touch IC\n" );
		synaptics_ic_reset ();
		return -1;
	}

	/* Internal reset check */
	if ( ( ( device_status & UNCONFIGURED_MASK ) >> 7 ) == 1 )
	{
		TPD_ERR ( "Touch IC resetted internally. Reconfigure register setting\n");
		synaptics_initialize ( tpd_i2c_client );
		return -1;
	}

	/* Palm check */
	ret = synaptics_ts_read ( tpd_i2c_client, TWO_D_EXTENDED_STATUS_REG, 1, &temp );
	if ( ret < 0 )
	{
		TPD_ERR ( "TWO_D_EXTENDED_STATUS_REG read fail\n" );
		return -1;
	}
	old_touch_info.palm = pre_touch_info.palm;
	pre_touch_info.palm = temp & 0x2;

	return 0;
}

void touch_keylock_enable ( int key_lock )
{
	TPD_FUN ();

	if ( !key_lock )
	{
		if ( suspend_status == 0 )
		{
			synaptics_ic_reset ();
			mt65xx_eint_unmask ( CUST_EINT_TOUCH_PANEL_NUM );
		}

		tpd_keylock_flag = 0;
	}
	else
	{
		mt65xx_eint_mask ( CUST_EINT_TOUCH_PANEL_NUM );
		synaptics_power ( 0 );

		tpd_keylock_flag = 1;
	}
}
EXPORT_SYMBOL ( touch_keylock_enable );

/****************************************************************************
* Touch Interrupt Service Routines
****************************************************************************/
static void touch_eint_interrupt_handler ( void )
{
	TPD_DEBUG_PRINT_INT;

	tpd_flag = 1;
	wake_up_interruptible ( &tpd_waiter );
}

static int synaptics_touch_event_handler ( void *unused )
{
	int ret = 0;
	u8 int_status;
	u8 int_enable;
	u8 finger_count, button_count;
	u8 reg_num, finger_order;
	u8 button_data;
	int width_min = 0, width_max = 0;
	int width_orientation;
	int index;
	touch_sensor_data sensor_data;
	touch_finger_info finger_info;
	char report_enable = 0;

	struct sched_param param = { .sched_priority = RTPM_PRIO_TPD }; 

	sched_setscheduler ( current, SCHED_RR, &param );

	do
	{
		set_current_state ( TASK_INTERRUPTIBLE );
		wait_event_interruptible ( tpd_waiter, tpd_flag != 0 );

		tpd_flag = 0;
		set_current_state ( TASK_RUNNING );
		mutex_lock ( &i2c_access );

		index = 0;
		memset(&sensor_data, 0x0, sizeof(touch_sensor_data));
		memset(&finger_info, 0x0, sizeof(touch_finger_info));
		pressure_zero = 0;

		ret = synaptics_ic_status_check ();
		if ( ret != 0 )
		{
			goto exit_work;
		}

		/* read interrupt information */
		ret = synaptics_ts_read ( tpd_i2c_client, INTERRUPT_STATUS_REG, 1, (u8 *) &int_status );
		if ( ret < 0 )
		{
			TPD_ERR ( "INTERRUPT_STATUS_REG read fail\n" );
			goto exit_work;
		}
		ret = synaptics_ts_read ( tpd_i2c_client, INTERRUPT_ENABLE_REG, 1, (u8 *) &int_enable );
		if ( ret < 0 )
		{
			TPD_ERR ( "INTERRUPT_ENABLE_REG read fail\n" );
			goto exit_work;
		}

		/* read finger state & finger data */
		ret = synaptics_ts_read ( tpd_i2c_client, TWO_D_FINGER_STATE_REG, sizeof(sensor_data), (u8 *) &sensor_data.finger_state[0] );
		if (ret < 0)
		{
			TPD_ERR ( "TWO_D_FINGER_STATE_REG read fail\n" );
			goto exit_work;
		}


		/* Finger Event Processing */
		if ( ( int_status & ABS0_MASK ) && ( int_enable & ABS0_MASK ) )
		{
			for ( finger_count = 0 ; finger_count < MAX_NUM_OF_FINGER ; finger_count++ )
			{
				reg_num = finger_count / 4;
				finger_order = finger_count % 4;

				if ( ( ( sensor_data.finger_state[reg_num] >> ( finger_order * 2 ) ) & FINGER_STATE_MASK ) == 1 )
				{
					finger_info.pos_x[finger_count] = ( int ) GET_X_POSITION(sensor_data.finger_data[finger_count][X_POSITION], sensor_data.finger_data[finger_count][XY_POSITION]);
					finger_info.pos_y[finger_count] = ( int ) GET_Y_POSITION(sensor_data.finger_data[finger_count][Y_POSITION], sensor_data.finger_data[finger_count][XY_POSITION]);

					if ( ( ( sensor_data.finger_data[finger_count][WX_WY] & 0xF0 ) >> 4 ) > ( sensor_data.finger_data[finger_count][WX_WY] & 0x0F ) )
					{
						width_max = ( sensor_data.finger_data[finger_count][WX_WY] & 0xF0 ) >> 4;
						width_min = sensor_data.finger_data[finger_count][WX_WY] & 0x0F;
						width_orientation = 0;
					}
					else
					{
						width_max = sensor_data.finger_data[finger_count][WX_WY] & 0x0F;
						width_min = ( sensor_data.finger_data[finger_count][WX_WY] & 0xF0 ) >> 4;
						width_orientation = 1;
					}

					finger_info.pressure[finger_count] = sensor_data.finger_data[finger_count][PRESSURE];

					if ( use_ghost_detection )
					{
						if ( finger_info.pressure[finger_count] == 0 )
						pressure_zero = 1;
					}

					input_report_key ( tpd->dev, BTN_TOUCH, 1 );
					input_report_abs ( tpd->dev, ABS_MT_TRACKING_ID, finger_count );
					input_report_abs ( tpd->dev, ABS_MT_POSITION_X, finger_info.pos_x[finger_count] );
					input_report_abs ( tpd->dev, ABS_MT_POSITION_Y, finger_info.pos_y[finger_count] );
					input_report_abs ( tpd->dev, ABS_MT_PRESSURE, finger_info.pressure[finger_count] );
					input_report_abs ( tpd->dev, ABS_MT_WIDTH_MAJOR, width_max );
					input_report_abs ( tpd->dev, ABS_MT_WIDTH_MINOR, width_min );
					input_report_abs ( tpd->dev, ABS_MT_ORIENTATION, width_orientation );

					report_enable = 1;

					old_touch_info.pos_x[finger_count] = pre_touch_info.pos_x[finger_count];
					old_touch_info.pos_y[finger_count] = pre_touch_info.pos_y[finger_count];
					old_touch_info.pressure[finger_count]= pre_touch_info.pressure[finger_count];
					pre_touch_info.pos_x[finger_count] = finger_info.pos_x[finger_count];
					pre_touch_info.pos_y[finger_count] = finger_info.pos_y[finger_count];
					pre_touch_info.pressure[finger_count] = finger_info.pressure[finger_count];
					index++;

					/* ignore Key event during Finger event processing */
					if ( finger_prestate[finger_count] == TOUCH_RELEASED )
					{
						finger_oldstate[finger_count] = finger_prestate[finger_count];
						finger_prestate[finger_count] = TOUCH_PRESSED;

						for ( button_count = 0 ; button_count < TPD_KEY_COUNT ; button_count++ )
						{
							if ( button_prestate[button_count] == TOUCH_PRESSED )
							{
								input_report_key ( tpd->dev, tpd_keys_local[button_count], BUTTON_CANCLED );
								//TPD_LOG ( "Touch KEY[%d] is canceled!\n", button_count + 1 );
								button_prestate[button_count] = TOUCH_RELEASED;
							}
						}

						/* Button interrupt Disable when first finger pressed */
						if ( int_enable & button_enable_mask )
						{
							int_enable = int_enable & ~( button_enable_mask );

							ret = synaptics_ts_write ( tpd_i2c_client, INTERRUPT_ENABLE_REG, &int_enable, 1 );
							if ( ret < 0 )
							{
								TPD_ERR ( "INTERRUPT_ENABLE_REG write fail\n" );
							}
						}
					}
				}
				else if ( finger_prestate[finger_count] == TOUCH_PRESSED )
				{
					finger_oldstate[finger_count] = finger_prestate[finger_count];
					finger_prestate[finger_count] = TOUCH_RELEASED;

					report_enable = 1;

					/* Button interrupt Enable when all finger released */
					if ( sensor_data.finger_state[0] == 0 && sensor_data.finger_state[1] == 0 )
					{
						int_enable = int_enable | button_enable_mask;

						ret = synaptics_ts_write ( tpd_i2c_client, INTERRUPT_ENABLE_REG, &int_enable, 1 );
						if ( ret < 0 )
						{
							TPD_ERR ( "INTERRUPT_ENABLE_REG write fail\n" );
						}
					}

					pre_touch_info.pos_x[finger_count] = 0;
					pre_touch_info.pos_y[finger_count] = 0;
				}

				if ( report_enable )
				{
					if ( index == 0 )
					{
						input_report_key ( tpd->dev, BTN_TOUCH, 0 );
					}
					input_mt_sync ( tpd->dev );
					report_enable = 0;
				}
			}

			finger_info.total_num = index;
			old_touch_info.total_num = pre_touch_info.total_num;
			pre_touch_info.total_num = finger_info.total_num;

			input_sync ( tpd->dev );
		}


		/* Button Event Processing */
		if ( ( int_status & button_enable_mask ) && ( int_enable & button_enable_mask ) )
		{
			/* read button data */
			ret = synaptics_page_data_read ( tpd_i2c_client, button_page, BUTTON_DATA_REG, sizeof(button_data), (u8 *) &button_data );
			if ( ret < 0 )
			{
				TPD_ERR ( "BUTTON_DATA_REG read fail\n" );
				goto exit_work;
			}

			for ( button_count = 0 ; button_count < TPD_KEY_COUNT ; button_count++ )
			{
				if ( ( ( button_data >> button_count ) & 0x1 ) == 1 && ( button_prestate[button_count] == TOUCH_RELEASED ) )
				{
					button_oldstate[button_count] = button_prestate[button_count];
					button_prestate[button_count] = TOUCH_PRESSED;
					report_enable = 1;
				}
				else if ( ( ( button_data >> button_count ) & 0x1 ) == 0 && ( button_prestate[button_count] == TOUCH_PRESSED ) )
				{
					button_oldstate[button_count] = button_prestate[button_count];
					button_prestate[button_count] = TOUCH_RELEASED;
					report_enable = 1;
				}

				if ( report_enable )
				{
					input_report_key ( tpd->dev, tpd_keys_local[button_count], button_prestate[button_count] );
					TPD_LOG ( "Touch key is %s (keyID = 0x%x)\n", button_prestate[button_count] ? "pressed" : "released", button_count );
					report_enable = 0;
				}
			}

			input_sync ( tpd->dev );
		}

		if ( use_ghost_detection )
		{
			ret = ghost_detect_solution ();
			if ( ret == NEED_TO_OUT )
			{
				goto exit_work;
			}
			else if ( ret == NEED_TO_INIT )
			{
				synaptics_release_all_finger ();
				synaptics_ic_reset ();
				synaptics_ic_ctrl ( tpd_i2c_client, IC_CTRL_REPORT_MODE, CONTINUOUS_REPORTING_MODE_MASK );
				force_continuous_mode = 1;
				ghost_detection = 0;
				ghost_detection_count = 0;
				do_gettimeofday ( &t_ex_debug[TIME_EX_INIT_TIME] );
				return 0;
			}
		}
exit_work:
		mutex_unlock ( &i2c_access );
	} while ( !kthread_should_stop () );

	return 0;
}

/****************************************************************************
* ADB Shell command function
****************************************************************************/
#if defined(LGE_USE_SYNAPTICS_F54)
static ssize_t synaptics_show_f54 ( struct device *dev, struct device_attribute *attr, char *buf )
{
	int ret = 0;

	if ( suspend_status == 0 )
	{
		SYNA_PDTScan ();
		SYNA_ConstructRMI_F54 ();
		SYNA_ConstructRMI_F1A ();

		ret = sprintf ( buf, "====== F54 Function Info ======\n" );

		switch ( f54_fullrawcap_mode )
		{
			case 0:
				ret += sprintf ( buf + ret, "fullrawcap_mode = For sensor\n" );
				break;
			case 1:
				ret += sprintf ( buf + ret, "fullrawcap_mode = For FPC\n" );
				break;
			case 2:
				ret += sprintf ( buf + ret, "fullrawcap_mode = CheckTSPConnection\n" );
				break;
			case 3:
				ret += sprintf ( buf + ret, "fullrawcap_mode = Baseline\n" );
				break;
			case 4:
				ret += sprintf ( buf + ret, "fullrawcap_mode = Delta image\n" );
				break;
		}

		mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);

		ret += sprintf ( buf + ret, "F54_FullRawCap(%d) Test Result: %s", f54_fullrawcap_mode, (F54_FullRawCap(f54_fullrawcap_mode) > 0) ? "Pass\n" : "Fail\n" );
		ret += sprintf ( buf + ret, "F54_TxToTxReport() Test Result: %s", (F54_TxToTxReport() > 0) ? "Pass\n" : "Fail\n" );
		ret += sprintf ( buf + ret, "F54_RxToRxReport() Test Result: %s", (F54_RxToRxReport() > 0) ? "Pass\n" : "Fail\n" );
		ret += sprintf ( buf + ret, "F54_TxToGndReport() Test Result: %s", (F54_TxToGndReport() > 0) ? "Pass\n" : "Fail\n" );
		ret += sprintf ( buf + ret, "F54_HighResistance() Test Result: %s", (F54_HighResistance() > 0) ? "Pass\n" : "Fail\n" );

		mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	}
	else
	{
		ret = sprintf ( buf + ret, "state=[suspend]. we cannot use I2C, now. Test Result: Fail\n" );
	}

	return ret;
}

static ssize_t synaptics_store_f54 ( struct device *dev, struct device_attribute *attr, const char *buf, size_t size )
{
	int ret = 0;

	ret = sscanf ( buf, "%d", &f54_fullrawcap_mode );

	return size;
}
static DEVICE_ATTR ( f54, 0664, synaptics_show_f54, synaptics_store_f54 );
#endif

#if defined(LGE_USE_SYNAPTICS_FW_UPGRADE)
static ssize_t synaptics_store_firmware ( struct device *dev, struct device_attribute *attr, const char *buf, size_t size )
{
	int ret;
	char path[256] = {0};

	sscanf ( buf, "%s", path );
	TPD_LOG ( "Firmware image update: %s\n", path );

	if ( !strncmp ( path, "1", 1 ) )
	{
		fw_force_update = 1;
	}
	else
	{
		memcpy ( fw_path, path, sizeof(fw_path) );
	}

	ret = synaptics_firmware_update ( tpd_i2c_client );

	synaptics_initialize ( tpd_i2c_client );

	return size;
}
static DEVICE_ATTR ( firmware, 0664, NULL, synaptics_store_firmware );
#endif

static ssize_t synaptics_show_version ( struct device *dev, struct device_attribute *attr, char *buf )
{
	char fw_config_id[5] = {0};
	char fw_product_id[11] = {0};
	int ret;

	if ( suspend_status )
	{
		synaptics_power ( 1 );
	}

	ret = synaptics_ts_read ( tpd_i2c_client, PRODUCT_ID_REG, 10, &fw_product_id[0] );
	if ( ret < 0 )
	{
		TPD_ERR ( "PRODUCT_ID_REG read fail\n" );
	}
	ret = synaptics_ts_read ( tpd_i2c_client, FLASH_CONFIG_ID_REG, 4, &fw_config_id[0] );
	if ( ret < 0 )
	{
		TPD_ERR ( "FLASH_CONFIG_ID_REG read fail\n" );
	}

	if ( suspend_status )
	{
		synaptics_power ( 0 );
	}

	return sprintf ( buf, "FW_Version[%02x%02x%02x%02x], HW_Version[%s]\n", fw_config_id[0], fw_config_id[1], fw_config_id[2], fw_config_id[3], fw_product_id );
}
static DEVICE_ATTR ( version, 0664, synaptics_show_version, NULL );

/****************************************************************************
* Synaptics_Touch_IC Initialize Function
****************************************************************************/
static void read_page_description_table ( struct i2c_client *client )
{
	int ret = 0;
	function_descriptor buffer;
	u8 page_num;
	u16 u_address;

	TPD_FUN ();

	memset(&buffer, 0x0, sizeof(function_descriptor));

	for ( page_num = 0 ; page_num < PAGE_MAX_NUM ; page_num++ )
	{
		ret = synaptics_ts_write ( client, PAGE_SELECT_REG, &page_num, 1 );
		if ( ret < 0 )
		{
			TPD_ERR ( "PAGE_SELECT_REG write fail (page_num=%d)\n", page_num );
		}

		for ( u_address = DESCRIPTION_TABLE_START ; u_address > 10 ; u_address -= sizeof(function_descriptor) )
		{
			ret = synaptics_ts_read ( client, u_address, sizeof(buffer), (u8 *) &buffer );
			if ( ret < 0 )
			{
				TPD_ERR ( "function_descriptor read fail\n" );
				return;
			}

			if ( buffer.function_exist == 0 )
				break;

			switch ( buffer.function_exist )
			{
				case RMI_DEVICE_CONTROL:
					device_control = buffer;
					device_control_page = page_num;
					break;
				case TOUCHPAD_SENSORS:
					finger = buffer;
					finger_page = page_num;
					break;
				case CAPACITIVE_BUTTON_SENSORS:
					button = buffer;
					button_page = page_num;
					break;
				case FLASH_MEMORY_MANAGEMENT:
					flash_memory = buffer;
					flash_memory_page = page_num;
					break;
			}
		}
	}

	ret = synaptics_ts_write ( client, PAGE_SELECT_REG, &device_control_page, 1 );
	if ( ret < 0 )
	{
		TPD_ERR ( "PAGE_SELECT_REG write fail\n" );
	}
}

#if defined(LGE_USE_SYNAPTICS_FW_UPGRADE)
static int synaptics_firmware_check ( struct i2c_client *client )
{
	int ret;
	u8 device_status = 0;
	u8 flash_control = 0;
	u8 fw_ver, image_ver;
	char fw_config_id[5] = {0};
	char fw_product_id[11] = {0};
	char image_config_id[5] = {0};
	char image_product_id[11] = {0};

	TPD_FUN ();

	/* read Firmware information in Download Image */
	fw_start = (unsigned char *) &SynaFirmware[0];
	fw_size = sizeof(SynaFirmware);
	strncpy ( image_product_id, &SynaFirmware[0x0040], 6 );
	strncpy ( image_config_id, &SynaFirmware[0xb100], 4 );

	if ( fw_path[0] != 0 )
	{
		TPD_LOG ( "Firmware force update by fw file\n" );
		return -1;
	}
	else if ( fw_force_update == 1 )
	{
		TPD_LOG ( "Firmware force update by buffer 1 [%02x%02x%02x%02x Ver]\n", image_config_id[0], image_config_id[1], image_config_id[2], image_config_id[3] );
		return -1;
	}

	ret = synaptics_ts_read ( client, DEVICE_STATUS_REG, 1, &device_status );
	if ( ret < 0 )
	{
		TPD_ERR ( "DEVICE_STATUS_REG read fail\n" );
	}
	ret = synaptics_ts_read ( client, FLASH_CONTROL_REG, 1, &flash_control );
	if ( ret < 0 )
	{
		TPD_ERR ( "FLASH_CONTROL_REG read fail\n" );
	}

	if ( ( device_status & FLASH_PROG_MASK ) || ( device_status & FW_CRC_FAILURE_MASK ) != 0 || ( flash_control & FLASH_STATUS_MASK ) != 0 )
	{
		TPD_ERR ( "Firmware has a problem. [device_status=%x, flash_control=%x]\nso it needs Firmware update. [%02x%02x%02x%02x Ver]\n", device_status, flash_control, image_config_id[0], image_config_id[1], image_config_id[2], image_config_id[3] );
		return -1;
	}
	else
	{
		/* read Firmware information in Touch IC */
		ret = synaptics_ts_read ( client, PRODUCT_ID_REG, 10, &fw_product_id[0] );
		if ( ret < 0 )
		{
			TPD_ERR ( "PRODUCT_ID_REG read fail\n" );
		}
		else
		{
			TPD_LOG ( "PRODUCT_ID_REG : %s \n", fw_product_id );
		}

		ret = synaptics_ts_read ( client, FLASH_CONFIG_ID_REG, 4, &fw_config_id[0] );
		if ( ret < 0 )
		{
			TPD_ERR ( "FLASH_CONFIG_ID_REG read fail\n" );
		}
		else
		{
			TPD_LOG ( "FLASH_CONFIG_ID_REG : %02x%02x%02x%02x \n", fw_config_id[0], fw_config_id[1], fw_config_id[2], fw_config_id[3] );
		}

		fw_ver = fw_config_id[3] & 0x7F;
		image_ver = image_config_id[3] & 0x7F;

		if ( ( !strcmp ( fw_product_id, image_product_id ) ) && ( image_ver != fw_ver ) )
		{
			TPD_LOG ( "[%02x ver ==> %02x ver] Firmware Update\n", fw_ver, image_ver );
			return -1;
		}
		else
		{
			TPD_LOG ( "No need to update Firmware\n" );
			return 0;
		}
	}
}

static int synaptics_firmware_update ( struct i2c_client *client )
{
	int ret;

	TPD_FUN ();

	ret	= synaptics_firmware_check ( client );
	if ( ret != 0 )
	{
		if ( !download_status )
		{
			download_status = 1;
			wake_lock ( &fw_suspend_lock );

			if ( !suspend_status )
			{
				mt65xx_eint_mask ( CUST_EINT_TOUCH_PANEL_NUM );
			}
			else
			{
				synaptics_power ( 1 );
			}

			ret = FirmwareUpgrade ( client, fw_path, fw_size, fw_start );
			if ( ret < 0 )
			{
				TPD_ERR ( "Firmware update Fail!!!\n" );
			}
			else
			{
				TPD_ERR ( "Firmware upgrade Complete\n" );
			}

			memset(fw_path, 0x00, sizeof(fw_path));
			fw_force_update = 0;

			if ( !suspend_status )
			{
				mt65xx_eint_unmask ( CUST_EINT_TOUCH_PANEL_NUM );
				synaptics_ic_reset ();
			}
			else
			{
				synaptics_power ( 0 );
			}

			wake_unlock ( &fw_suspend_lock );
			download_status = 0;
		}
		else
		{
			TPD_ERR ( "Firmware Upgrade process is aready working on\n" );
		}

		read_page_description_table ( client );
	}

	return 0;
}
#endif

void synaptics_initialize ( struct i2c_client *client )
{
	int ret;
	u8 temp,temp2;

	TPD_FUN ();

	temp = NOSLEEP_MASK | CONFIGURED_MASK;
	ret = synaptics_ts_write ( client, DEVICE_CONTROL_REG, &temp, 1 );
	if ( ret < 0 )
	{
		TPD_ERR ( "DEVICE_CONTROL_REG write fail\n" );
	}

	if ( button.function_exist != 0 )
	{
		button_enable_mask = BUTTON_MASK;
	}

	ret = synaptics_ts_read ( client, INTERRUPT_ENABLE_REG, 1, &temp );
	if ( ret < 0 )
	{
		TPD_ERR ( "INTERRUPT_ENABLE_REG read fail\n" );
	}

	temp2 = temp | ABS0_MASK | button_enable_mask;
	ret = synaptics_ts_write ( client, INTERRUPT_ENABLE_REG, &temp2, 1 );
	if (ret < 0)
	{
		TPD_ERR ( "INTERRUPT_ENABLE_REG write fail\n" );
	}

	temp = REDUCED_REPORTING_MODE_MASK | ABS_POS_FILTER_MASK | REPORT_BEYOND_CLIP_MASK;
	ret = synaptics_ts_write ( client, TWO_D_REPORT_MODE_REG, &temp, 1 );
	if (ret < 0)
	{
		TPD_ERR ( "TWO_D_REPORT_MODE_REG write fail\n" );
	}

	temp = (u8) DELTA_POS_THRESHOLD;
	ret = synaptics_ts_write ( client, TWO_D_DELTA_X_THRESH_REG, &temp, 1 );
	if (ret < 0)
	{
		TPD_ERR ( "TWO_D_DELTA_X_THRESH_REG write fail\n" );
	}

	temp = (u8) DELTA_POS_THRESHOLD;
	ret = synaptics_ts_write ( client, TWO_D_DELTA_Y_THRESH_REG, &temp, 1 );
	if (ret < 0)
	{
		TPD_ERR ( "TWO_D_DELTA_Y_THRESH_REG write fail\n" );
	}

	ret = synaptics_ts_read ( client, INTERRUPT_STATUS_REG, 1, &temp );
	if ( ret < 0 )
	{
		TPD_ERR ( "INTERRUPT_STATUS_REG read fail\n" );
	}

	if ( use_ghost_detection )
	{
		synaptics_ic_ctrl ( client, IC_CTRL_REPORT_MODE, REDUCED_REPORTING_MODE_MASK );
		force_continuous_mode = 1;
		ghost_detection = 0;
		ghost_detection_count = 0;
		do_gettimeofday ( &t_ex_debug[TIME_EX_INIT_TIME] );
	}
}

void synaptics_init_sysfs ( void )
{
	int err;

	TPD_FUN ();

	touch_class = class_create(THIS_MODULE, "touch");

#if defined(LGE_USE_SYNAPTICS_F54)
	touch_debug_dev = device_create ( touch_class, NULL, 0, NULL, "debug" );
	err = device_create_file ( touch_debug_dev, &dev_attr_f54 );
	if ( err )
	{
		TPD_ERR ( "Touchscreen : [f54] touch device_create_file Fail\n" );
		device_remove_file ( touch_debug_dev, &dev_attr_f54 );
	}
#endif

#if defined(LGE_USE_SYNAPTICS_FW_UPGRADE)
	touch_fw_dev = device_create ( touch_class, NULL, 0, NULL, "firmware" );
	err = device_create_file ( touch_fw_dev, &dev_attr_firmware );
	if ( err )
	{
		TPD_ERR ( "Touchscreen : [firmware] touch device_create_file Fail\n" );
		device_remove_file ( touch_fw_dev, &dev_attr_firmware );
	}

	err = device_create_file ( touch_fw_dev, &dev_attr_version );
	if ( err )
	{
		TPD_ERR ( "Touchscreen : [version] touch device_create_file Fail\n" );
		device_remove_file ( touch_fw_dev, &dev_attr_version );
	}
#endif
}

/****************************************************************************
* I2C BUS Related Functions
****************************************************************************/
static int synaptics_i2c_probe ( struct i2c_client *client, const struct i2c_device_id *id )
{
	int i, err = 0, ret = 0;
	struct task_struct *thread = NULL;

	TPD_FUN ();

	/* the choice of ghost_detection */
	use_ghost_detection = 0;

	/* X, Y max touch position */
	x_max = 240;
	y_max = 320;

	tpd_i2c_client = client;
#if defined(LGE_USE_SYNAPTICS_F54)
	ds4_i2c_client = client;
#endif

	synaptics_init_sysfs ();

#if defined(ONLY_S2202_RESET_PIN)
	synaptics_reset_pin ( GPIO_TOUCH_RESET_N );
#endif

	/* Turn on the power for TOUCH */
	synaptics_power ( 1 );

#ifdef TPD_HAVE_BUTTON
	for ( i = 0 ; i < TPD_KEY_COUNT ; i++ )
	{
		input_set_capability ( tpd->dev, EV_KEY, tpd_keys_local[i] );
	}
#endif

	thread = kthread_run ( synaptics_touch_event_handler, 0, TPD_DEVICE );
	if ( IS_ERR ( thread ) )
	{
		err = PTR_ERR ( thread );
		TPD_ERR ( "failed to create kernel thread: %d\n", err );
	}

	/* Configure external ( GPIO ) interrupt */
	synaptics_setup_eint ();

	/* find register map */
	read_page_description_table ( client );

#if defined(LGE_USE_SYNAPTICS_FW_UPGRADE)
	wake_lock_init ( &fw_suspend_lock, WAKE_LOCK_SUSPEND, "fw_wakelock" );

	/* Touch Firmware Update */
	ret = synaptics_firmware_update ( client );
#endif

	/* Touch IC initialize */
	synaptics_initialize ( client );

	tpd_load_status = 1;

	return 0;
}

static int synaptics_i2c_remove(struct i2c_client *client)
{
	TPD_FUN ();
	return 0;
}

static int synaptics_i2c_detect ( struct i2c_client *client, struct i2c_board_info *info )
{
	TPD_FUN ();
	strcpy ( info->type, "mtk-tpd" );
	return 0;
}

static const struct i2c_device_id tpd_i2c_id[] = { { TPD_DEV_NAME, 0 },	{} };

static struct i2c_driver tpd_i2c_driver = {
	.driver.name = "mtk-tpd",
	.probe = synaptics_i2c_probe,
	.remove = __devexit_p(synaptics_i2c_remove),
	.detect = synaptics_i2c_detect,
	.id_table = tpd_i2c_id,
};

/****************************************************************************
* Linux Device Driver Related Functions
****************************************************************************/
static int synaptics_local_init ( void )
{
	TPD_FUN ();

	if ( i2c_add_driver ( &tpd_i2c_driver ) != 0 )
	{
		TPD_ERR ( "i2c_add_driver failed\n" );
		return -1;
	}

	if ( tpd_load_status == 0 )
	{
		TPD_ERR ( "touch driver probing failed\n" );
		i2c_del_driver ( &tpd_i2c_driver );
		return -1;
	}

	tpd_type_cap = 1;

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void synaptics_suspend ( struct early_suspend *h )
{
//	int ret;  // unused

	TPD_FUN ();

	if ( use_ghost_detection )
	{
		resume_flag = 0;
	}

	if ( !download_status )
	{
		synaptics_release_all_finger ();

		/* mask external interrupt */
		mt65xx_eint_mask ( CUST_EINT_TOUCH_PANEL_NUM );

		/* Turn off the power for TOUCH */
		synaptics_power ( 0 );
	}

	suspend_status = 1;
}

static void synaptics_resume ( struct early_suspend *h )
{
	TPD_FUN ();

	if ( use_ghost_detection )
	{
		resume_flag = 1;
		ts_rebase_count = 0;
	}

	if ( !download_status && !tpd_keylock_flag )
	{
#if defined(ONLY_S2202_RESET_PIN)
		synaptics_reset_pin ( GPIO_TOUCH_RESET_N );
#endif

		/* Turn on the power for TOUCH */
		synaptics_power ( 1 );

		synaptics_release_all_finger ();
		msleep ( 10 );

		synaptics_initialize ( tpd_i2c_client );

		/* unmask external interrupt */
		mt65xx_eint_unmask ( CUST_EINT_TOUCH_PANEL_NUM );
	}

	suspend_status = 0;
}
#endif

static struct i2c_board_info __initdata i2c_tpd = {	I2C_BOARD_INFO ( TPD_DEV_NAME, TPD_I2C_ADDRESS ) };

static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = TPD_DEV_NAME,
	.tpd_local_init = synaptics_local_init,
#ifdef CONFIG_HAS_EARLYSUSPEND
	.suspend = synaptics_suspend,
	.resume = synaptics_resume,
#endif
#ifdef TPD_HAVE_BUTTON
	.tpd_have_button = 1,
#else
	.tpd_have_button = 0,
#endif
};

static int __init synaptics_driver_init ( void )
{
	TPD_FUN ();

	I2CDMABuf_va = (u8 *) dma_alloc_coherent ( NULL, 4096, &I2CDMABuf_pa, GFP_KERNEL );
	if ( !I2CDMABuf_va )
	{
		TPD_ERR ( "Allocate Touch DMA I2C Buffer failed!\n" );
		return ENOMEM;
	}

	i2c_register_board_info ( 1, &i2c_tpd, 1 );
	if ( tpd_driver_add ( &tpd_device_driver ) < 0 )
	{
		TPD_ERR ( "tpd_driver_add failed\n" );
	}

	return 0;
}

static void __exit synaptics_driver_exit ( void )
{
	TPD_FUN ();

	tpd_driver_remove ( &tpd_device_driver );

	if ( I2CDMABuf_va )
	{
		dma_free_coherent ( NULL, 4096, I2CDMABuf_va, I2CDMABuf_pa );
		I2CDMABuf_va = NULL;
		I2CDMABuf_pa = 0;
	}
}


module_init ( synaptics_driver_init );
module_exit ( synaptics_driver_exit );

MODULE_DESCRIPTION ( "Synaptics 2200 Touchscreen Driver for MTK platform" );
MODULE_AUTHOR ( "Junmo Kang <junmo.kang@lge.com>" );
MODULE_LICENSE ( "GPL" );

/* End Of File */
