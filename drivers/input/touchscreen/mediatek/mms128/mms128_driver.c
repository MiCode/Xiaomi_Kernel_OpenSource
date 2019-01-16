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
#include <mach/eint.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <cust_eint.h>
#include <linux/jiffies.h>

#include "tpd.h"

#include <linux/spinlock.h>
#include <mach/mt_wdt.h>
#include <mach/mt_gpt.h>
#include <mach/mt_reg_base.h>

//#include <wd_kicker.h>
#include <mach/wd_api.h>

#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
#include <cust_eint.h>

#include <linux/wakelock.h>

/****************************************************************************
* Constants / Definitions
****************************************************************************/
#define TP_DEV_NAME "mms128"
#define I2C_RETRY_CNT 5 //Fixed value
#define DOWNLOAD_RETRY_CNT 5 //Fixed value
#define MELFAS_DOWNLOAD 1 //Fixed value

#define PRESS_KEY 1 //Fixed value
#define RELEASE_KEY 0 //Fixed value

#define TS_READ_LEN_ADDR 0x0F //Fixed value
#define TS_READ_START_ADDR 0x10 //Fixed value
#define TS_READ_REGS_LEN 66 //Fixed value
#define TS_WRITE_REGS_LEN 16 //Fixed value

#define TS_MAX_TOUCH 	5 //Model Dependent

#define TS_READ_HW_VER_ADDR 0xF1 //Model Dependent
#define TS_READ_SW_VER_ADDR 0xF0 //Model Dependent
#define MELFAS_FW_VERSION   0xF5 //Model Dependent

#define MELFAS_CURRENT_FW_VERSION 122

#define MELFAS_HW_REVISON 0x01 //Model Dependent

#define MELFAS_MAX_TRANSACTION_LENGTH 66
#define MELFAS_MAX_I2C_TRANSFER_SIZE  7
#define MELFAS_I2C_DEVICE_ADDRESS_LEN 1
//#define I2C_MASTER_CLOCK       400
#define MELFAS_I2C_MASTER_CLOCK       100
#define MELFAS_I2C_ADDRESS   0x48

#define VAR_CHAR_NUM_MAX      20

#define TPD_HAVE_BUTTON

#ifdef TPD_HAVE_BUTTON
#ifdef LGE_USE_DOME_KEY
#define TPD_KEY_COUNT	2
static int tpd_keys_local[TPD_KEY_COUNT] = {KEY_BACK , KEY_MENU};
#else
#define TPD_KEY_COUNT	4
static int tpd_keys_local[TPD_KEY_COUNT] = {KEY_BACK ,KEY_HOMEPAGE, KEY_MENU};
#endif
#endif

enum {
	None = 0,
	TOUCH_SCREEN,
	TOUCH_KEY
};

/****************************************************************************
* Macros
****************************************************************************/
#define TPD_TAG                  "[Melfas] "
#define TPD_FUN(f)               printk(KERN_ERR TPD_TAG"%s\n", __FUNCTION__)
#define TPD_ERR(fmt, args...)    printk(KERN_ERR TPD_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define TPD_LOG(fmt, args...)    printk(KERN_ERR TPD_TAG fmt, ##args)

/****************************************************************************
* Variables
****************************************************************************/
struct muti_touch_info {
	int strength;
//	int width
	int area;
	int posX;
	int posY;
	int status;
	int pressure;
};

extern struct tpd_device *tpd;
struct i2c_client *melfas_i2c_client = NULL;
static struct muti_touch_info g_Mtouch_info[TS_MAX_TOUCH];

static int melfas_tpd_flag = 0;
unsigned char  touch_fw_version = 0;

/* ghost finger pattern detection */
struct delayed_work ghost_monitor_work;
static int ghost_touch_cnt = 0;
static int ghost_x = 1000;
static int ghost_y = 1000;

/* Ignore Key event during touch event actioned */
static int before_touch_time = 0;
static int current_key_time = 0;
static int is_touch_pressed = 0;

static int is_key_pressed = 0;
static int pressed_keycode = 0;
#define CANCEL_KEY 0xff


static DEFINE_MUTEX(i2c_access);
static DECLARE_WAIT_QUEUE_HEAD(melfas_waiter);


/****************************************************************************
* Extern Function Prototypes
****************************************************************************/
extern int mms100_ISP_download_binary_data ( int dl_mode );
extern int mtk_wdt_enable(enum wk_wdt_en en);


/****************************************************************************
* Local Function Prototypes
****************************************************************************/
static void mms128_eint_interrupt_handler ( void );


/****************************************************************************
* Platform(AP) dependent functions
****************************************************************************/
static void mms128_setup_eint ( void )
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
	mt65xx_eint_registration ( CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, CUST_EINT_POLARITY_LOW, mms128_eint_interrupt_handler, 1 );

	/* unmask external interrupt */
	mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
}

void mms128_power ( unsigned int on )
{
	TPD_FUN ();

	if ( on )
	{
		hwPowerOn ( MT6323_POWER_LDO_VGP2, VOL_3000, "TP" );
		TPD_LOG ( "turned on the power ( VGP2 )\n" );
		msleep(10);
	}
	else
	{
		hwPowerDown ( MT6323_POWER_LDO_VGP2, "TP" );
		TPD_LOG ( "turned off the power ( VGP2 )\n" );
	}
}

/****************************************************************************
* MMS128 I2C Read / Write Funtions
****************************************************************************/
int mms128_i2c_write_bytes ( struct i2c_client *client, u16 addr, int len, u8 *txbuf )
{
    u8 buffer[MELFAS_MAX_TRANSACTION_LENGTH] = { 0 };
    u16 left = len;
    u8 offset = 0;
    u8 retry = 0;

    struct i2c_msg msg = {
        .addr = ( ( client->addr & I2C_MASK_FLAG ) | ( I2C_ENEXT_FLAG ) ),
        .flags = 0,
        .buf = buffer,
        .timing = MELFAS_I2C_MASTER_CLOCK,
    };

    if ( txbuf == NULL )
    {
        return -1;
    }

    TPD_DEBUG ( "i2c_write_bytes to device %02X address %04X len %d\n", client->addr, addr, len );

    while ( left > 0 )
    {
        retry = 0;

        buffer[0] = ( u8 ) addr + offset;

        if ( left > MELFAS_MAX_I2C_TRANSFER_SIZE )
        {
            memcpy ( &buffer[MELFAS_I2C_DEVICE_ADDRESS_LEN], &txbuf[offset], MELFAS_MAX_I2C_TRANSFER_SIZE );
            msg.len = MELFAS_MAX_TRANSACTION_LENGTH;
            left -= MELFAS_MAX_I2C_TRANSFER_SIZE;
            offset += MELFAS_MAX_I2C_TRANSFER_SIZE;
        }
        else
        {
            memcpy ( &buffer[MELFAS_I2C_DEVICE_ADDRESS_LEN], &txbuf[offset], left );
            msg.len = left + MELFAS_I2C_DEVICE_ADDRESS_LEN;
            left = 0;
        }

        TPD_DEBUG ( "byte left %d offset %d\n", left, offset );

        while ( i2c_transfer ( client->adapter, &msg, 1 ) != 1 )
        {
            retry++;
            if ( retry == I2C_RETRY_CNT )
            {
                TPD_ERR ( "I2C write 0x%X%X length=%d failed\n", buffer[0], buffer[1], len );
                return -1;
            }
            else
            {
                TPD_ERR ( "I2C write retry %d addr 0x%X%X\n", retry, buffer[0], buffer[1] );
            }
        }
    }

    return 0;
}

int mms128_i2c_write_data ( struct i2c_client *client, int len, u8 *txbuf )
{
    u16 ret = 0;

	client->addr = client->addr & I2C_MASK_FLAG;

    if ( txbuf == NULL )
    {
        return -1;
    }

	ret = i2c_master_send ( client, txbuf, len );
	if ( ret != len )
	{
		return -1;
	}

    return 0;
}

int mms128_i2c_read_data ( struct i2c_client *client, int len, u8 *rxbuf )
{
    u16 ret = 0;

	client->addr = client->addr & I2C_MASK_FLAG;

    if ( rxbuf == NULL )
    {
        return -1;
    }

	ret = i2c_master_recv ( client, rxbuf, len );
	if ( ret != len )
	{
		return -1;
	}

    return 0;
}

int mms128_i2c_read ( struct i2c_client *client, u16 addr, u16 len, u8 *rxbuf )
{
    u8 buffer[MELFAS_I2C_DEVICE_ADDRESS_LEN]= { 0 };
    u8 retry;
    u16 left = len;
    u8 offset = 0;

    struct i2c_msg msg[2] = {
		{
            .addr = ( ( client->addr & I2C_MASK_FLAG ) | ( I2C_ENEXT_FLAG ) ),
            .flags = 0,
            .buf = buffer,
            .len = MELFAS_I2C_DEVICE_ADDRESS_LEN,
            .timing = MELFAS_I2C_MASTER_CLOCK
        },
        {
            .addr = ( ( client->addr & I2C_MASK_FLAG ) | ( I2C_ENEXT_FLAG ) ),
            .flags = I2C_M_RD,
            .timing = MELFAS_I2C_MASTER_CLOCK
        },
    };

    if ( rxbuf == NULL )
    {
        return -1;
    }

    TPD_DEBUG ( "i2c_read_bytes to device %02X address %04X len %d\n", client->addr, addr, len );

    while ( left > 0 )
    {
        buffer[0] = ( u8 ) addr + offset;
        msg[1].buf = &rxbuf[offset];

        if ( left > MELFAS_MAX_TRANSACTION_LENGTH )
        {
            msg[1].len = MELFAS_MAX_TRANSACTION_LENGTH;
            left -= MELFAS_MAX_TRANSACTION_LENGTH;
            offset += MELFAS_MAX_TRANSACTION_LENGTH;
        }
        else
        {
            msg[1].len = left;
            left = 0;
        }

        retry = 0;

        while ( i2c_transfer ( client->adapter, &msg[0], 2 ) != 2 )
        {
            retry++;
            if ( retry == I2C_RETRY_CNT )
            {
                TPD_ERR ( "I2C read 0x%X length=%d failed\n", addr + offset, len );
                return -1;
            }
        }
    }

    return 0;
}

/****************************************************************************
* Touch malfunction Prevention Function
****************************************************************************/
static void mms128_release_all_finger ( void )
{	
	int i;

        TPD_FUN ();

	for ( i = 0 ; i < TS_MAX_TOUCH ; i++ )
	{
		g_Mtouch_info[i].pressure = -1;
	}

	input_mt_sync ( tpd->dev );
	input_sync ( tpd->dev );
}

static void mms128_touch_reset ( void )
{
	TPD_FUN ();

	mms128_release_all_finger ();
	mt65xx_eint_mask ( CUST_EINT_TOUCH_PANEL_NUM );
	mms128_power ( 0 );
	msleep ( 100 );
	mms128_power ( 1 );
	msleep ( 100 );
	mt65xx_eint_unmask ( CUST_EINT_TOUCH_PANEL_NUM );
}

static void mms128_read_dummy_interrupt ( void )
{
	uint8_t buf[TS_READ_REGS_LEN] = { 0, };
	int read_num = 0;
	int ret_val = 0;

	ret_val = mms128_i2c_read ( melfas_i2c_client, TS_READ_LEN_ADDR, 1, buf );
	if ( ret_val )
	{
		return;
	}

	read_num = buf[0];
	if ( read_num )
	{
		mms128_i2c_read ( melfas_i2c_client, TS_READ_START_ADDR, read_num, buf );
		if ( 0x0F == buf[0] )
		{
			mms128_touch_reset();
		}
	}

    return;
}

static void mms128_monitor_ghost_finger ( struct work_struct *work )
{
	if ( ghost_touch_cnt >= 45 )
	{
		TPD_LOG("ghost finger pattern DETECTED! : %d\n", ghost_touch_cnt );

		mt65xx_eint_mask ( CUST_EINT_TOUCH_PANEL_NUM );
		wait_event_interruptible ( melfas_waiter, melfas_tpd_flag == 0 );

		mms128_power ( 0 );
		msleep ( 100 );

		mms128_release_all_finger ();
		input_mt_sync ( tpd->dev );
		input_sync ( tpd->dev );

		mms128_power ( 1 );
		msleep(200);
		msleep(100);

		mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	}

	schedule_delayed_work ( &ghost_monitor_work, msecs_to_jiffies ( HZ * 50 ) );

	ghost_touch_cnt = 0;
	ghost_x = 1000;
	ghost_y = 1000;

	return;
}

/****************************************************************************
* MMS128 Interrupt Service Routines
****************************************************************************/
static void mms128_eint_interrupt_handler ( void )
{
    TPD_DEBUG_PRINT_INT;

    melfas_tpd_flag = 1;
    wake_up_interruptible ( &melfas_waiter );
}

static int mms128_event_handler ( void *unused )
{
	uint8_t buf[TS_READ_REGS_LEN] = { 0 };
	int i, read_num, fingerID, Touch_Type = 0, touchState = 0;
	int keyID = 0, reportID = 0;
	int ret;

	int press_count = 0;
	int is_touch_mix = 0;

    struct sched_param param = { .sched_priority = RTPM_PRIO_TPD }; 

    sched_setscheduler ( current, SCHED_RR, &param );

    do
	{
        set_current_state ( TASK_INTERRUPTIBLE );

        wait_event_interruptible ( melfas_waiter, melfas_tpd_flag != 0 );

        melfas_tpd_flag = 0;
        set_current_state ( TASK_RUNNING );
		mutex_lock ( &i2c_access );

		mms128_i2c_read ( melfas_i2c_client, TS_READ_LEN_ADDR, 1, buf );
		read_num = buf[0];
		if ( read_num )
		{
			ret = mms128_i2c_read ( melfas_i2c_client, TS_READ_START_ADDR, read_num, buf );
			if ( ret < 0 )
			{
				TPD_ERR ( "melfas:i2c read error\n" );
				mms128_touch_reset ();
				goto exit_work_func;
			}

          	if ( 0x0F == buf[0] )
           	{
           		TPD_LOG ( "ESD Detected!!\n" );
            	mms128_touch_reset ();
             	goto exit_work_func;
		 	}

			for ( i = 0 ; i < read_num ; i = i + 6 )
            {
                Touch_Type = ( buf[i] >> 5 ) & 0x03;
				fingerID = ( buf[i] & 0x0F ) - 1;
				touchState = ( ( buf[i] & 0x80 ) == 0x80 );
				reportID = ( buf[i] & 0x0F );
				keyID = reportID;

                /* touch type is panel */
				if ( Touch_Type == TOUCH_SCREEN )
                {
                    g_Mtouch_info[fingerID].posX = ( uint16_t ) ( buf[i + 1] & 0x0F ) << 8 | buf[i + 2];
                    g_Mtouch_info[fingerID].posY = ( uint16_t ) ( buf[i + 1] & 0xF0 ) << 4 | buf[i + 3];
                    g_Mtouch_info[fingerID].area = buf[i + 4];

					g_Mtouch_info[fingerID].status = touchState;

                    if ( touchState )
                    {
						g_Mtouch_info[fingerID].pressure = buf[i + 5];
						//g_Mtouch_info[fingerID].pressure = 10;
						//g_Mtouch_info[fingerID].pressure = buf[i + 5];
                    }
                    else
					{
                        g_Mtouch_info[fingerID].pressure = 0;

						/* ghost finger pattern detection */
						if(ghost_touch_cnt == 0)
						{
							ghost_x = g_Mtouch_info[fingerID].posX;
							ghost_y = g_Mtouch_info[fingerID].posY;
							ghost_touch_cnt++;
						}
						else
						{
							if ( ghost_x + 40 >= g_Mtouch_info[fingerID].posX && ghost_x - 40 <= g_Mtouch_info[fingerID].posX )
							{
								if ( ghost_y + 40 >= g_Mtouch_info[fingerID].posY && ghost_y - 40 <= g_Mtouch_info[fingerID].posY )
								{
									ghost_touch_cnt++;
								}
							}
						}
					}

					if ( is_key_pressed == PRESS_KEY )
					{
						//TPD_LOG ( " ++++++++ KEY_CANCEL!!!!!!!!\n\n" );
						input_report_key ( tpd->dev, pressed_keycode, CANCEL_KEY );
						input_sync ( tpd->dev );
						is_key_pressed = CANCEL_KEY;
					}

					is_touch_mix = 1;
                }
				else if ( Touch_Type == TOUCH_KEY )
				{
					current_key_time = jiffies_to_msecs ( jiffies );
					if ( before_touch_time > 0 )
					{
						if ( current_key_time - before_touch_time > 150) // 100
						{
							is_touch_pressed = 0;
						}
						else
						{
							continue;
						}
					}

					before_touch_time = 0;
					current_key_time = 0;
					// Ignore Key event during touch event actioned
					if ( is_touch_mix || is_touch_pressed )
					{
						continue;
					}

					if ( keyID == 0x1 )
					{
						input_report_key ( tpd->dev, tpd_keys_local[keyID-1], touchState ? PRESS_KEY : RELEASE_KEY );
					}
					else if ( keyID == 0x2 )
					{
						input_report_key ( tpd->dev, tpd_keys_local[keyID-1], touchState ? PRESS_KEY : RELEASE_KEY );
					}
					else if ( keyID == 0x3 )
					{
						input_report_key ( tpd->dev, tpd_keys_local[keyID-1], touchState ? PRESS_KEY : RELEASE_KEY );
					}
					else if ( keyID == 0x4 )
					{
						input_report_key ( tpd->dev, tpd_keys_local[keyID-1], touchState ? PRESS_KEY : RELEASE_KEY );
					}
					else
					{
						TPD_ERR ( " KeyID is incorrect!! (0x%x)\n", keyID );
						keyID = 0x00;
					}

					if ( keyID != 0 )
					{
						pressed_keycode = tpd_keys_local[keyID-1];

						if ( touchState )
						{
							is_key_pressed = PRESS_KEY;
							TPD_LOG ( "Touch key press (keyID = 0x%x)\n", keyID );
						}
						else
						{
							is_key_pressed = RELEASE_KEY;
							TPD_LOG ( "Touch key release (keyID = 0x%x)\n", keyID );
						}
					}
				}
            }

			press_count = 0;
			if ( is_touch_mix )
			{
				for ( i = 0 ; i < TS_MAX_TOUCH ; i++ )
				{
					if ( g_Mtouch_info[i].pressure == -1 )
					{
						continue;
					}
					if ( g_Mtouch_info[i].status == 0 )
					{
						is_touch_pressed = 0;
						g_Mtouch_info[i].status = -1;
						continue;
					}
					if ( g_Mtouch_info[i].status == 1 )
					{
						input_report_key ( tpd->dev, BTN_TOUCH, 1 );
						input_report_abs ( tpd->dev, ABS_MT_TRACKING_ID, i );
						input_report_abs ( tpd->dev, ABS_MT_POSITION_X, g_Mtouch_info[i].posX );
						input_report_abs ( tpd->dev, ABS_MT_POSITION_Y, g_Mtouch_info[i].posY );
						input_report_abs ( tpd->dev, ABS_MT_TOUCH_MAJOR, g_Mtouch_info[i].area );
						//input_report_abs ( tpd->dev, ABS_MT_WIDTH_MAJOR, g_Mtouch_info[i].pressure );
						input_report_abs ( tpd->dev, ABS_MT_PRESSURE, g_Mtouch_info[i].pressure );

						is_touch_pressed = 1;
						input_mt_sync ( tpd->dev );
						press_count++;
					}
					if ( g_Mtouch_info[i].pressure == 0 )
					{
						g_Mtouch_info[i].pressure = -1;
					}
				}

				if ( press_count == 0 )
				{
					input_report_key ( tpd->dev, BTN_TOUCH, 0 );
					input_mt_sync ( tpd->dev );
				}

				before_touch_time = jiffies_to_msecs ( jiffies );
			}
			is_touch_mix = 0;
			input_sync ( tpd->dev );
		}
exit_work_func:
		mutex_unlock ( &i2c_access );
    } while ( !kthread_should_stop () );

    return 0;
}

/****************************************************************************
* MMS128 Firmware Update Function
****************************************************************************/
static int mms128_fw_load ( struct i2c_client *client, int hw_ver )
{
	int ret = 0;
	TPD_FUN ();

   	mt65xx_eint_mask ( CUST_EINT_TOUCH_PANEL_NUM );
	//mtk_wdt_enable ( WK_WDT_DIS );
	mutex_lock ( &i2c_access );

	ret = mms100_ISP_download_binary_data ( hw_ver );
	if ( ret )
	{
		TPD_LOG ( "SET Download ISP Fail\n" );
	}

	mutex_unlock ( &i2c_access );
	//mtk_wdt_enable ( WK_WDT_EN );

	mms128_power ( 0 );
	msleep ( 100 );
	mms128_power ( 1 );
	msleep ( 200 );
	mt65xx_eint_unmask ( CUST_EINT_TOUCH_PANEL_NUM );

    return ret;
}

#if 0
static int mms128_check_firmware ( struct i2c_client *client, u8 *val )
{
    int ret = 0;

    ret = mms128_i2c_read ( client, TS_READ_HW_VER_ADDR, 1, &val[0] );
	if ( ret != 0 )
	{
		return ret;
	}

    ret = mms128_i2c_read ( client, MELFAS_FW_VERSION, 1, &val[1] );

    TPD_LOG ( "Touch IC ==> H/W Ver[0x%x], F/W Ver[0x%x]\n", val[0], val[1] );

	return ret;
}

static int mms128_firmware_update ( struct i2c_client *client )
{
	int ret = 0;
    uint8_t fw_ver[2] = { 0, };

	TPD_FUN ();

    ret = mms128_check_firmware ( client, fw_ver );
    if ( ret < 0 )
	{
		TPD_LOG ( "check_firmware fail! [%d]", ret );
		mms128_power ( 0 );
		msleep ( 100 );
		mms128_power ( 1 );
		msleep ( 100 );
		return -1;
	}
    else
    {
		if ( fw_ver[1] != MELFAS_CURRENT_FW_VERSION )
		{
			TPD_LOG ( "0x%x version Firmware Update\n", MELFAS_CURRENT_FW_VERSION );
			ret = mms128_fw_load ( client, 1 );
		}
		else
		{
			TPD_LOG ( "Touch Firmware is the latest version [Ver: 0x%x]\n", fw_ver[1] );
		}
    }

    return ret;
}
#endif

/****************************************************************************
* MMS128 ADB Shell command function
****************************************************************************/
static ssize_t mms128_show_update (struct device *dev,struct device_attribute *attr, char *buf)
{
//   char ver[20];
//	snprintf(buf, VAR_CHAR_NUM_MAX, "%s", ver);
	return 0;
}

static ssize_t mms128_store_update (struct device *dev,struct device_attribute *attr, const char *buf, size_t count)
{
	int hw_ver;
	sscanf ( buf, "%d", &hw_ver );

	cancel_delayed_work ( &ghost_monitor_work );
	if ( hw_ver == 0 )
	{
		mms128_fw_load ( melfas_i2c_client, 0 );
	}
	else
	{
		mms128_fw_load ( melfas_i2c_client, 1 );
	}
	schedule_delayed_work ( &ghost_monitor_work, msecs_to_jiffies ( HZ * 50 ) );
    return count;
}
static DEVICE_ATTR ( update, 0664, mms128_show_update, mms128_store_update );


static ssize_t mms128_show_firmware ( struct device *dev,  struct device_attribute *attr,  char *buf )
{
	int r;
	u8 product_id;
	u8 product_id2;

	r = snprintf ( buf, PAGE_SIZE, "%d\n", touch_fw_version );

	mms128_i2c_read ( melfas_i2c_client, MELFAS_FW_VERSION, sizeof ( product_id ), &product_id );
	mms128_i2c_read ( melfas_i2c_client, TS_READ_HW_VER_ADDR, sizeof ( product_id2 ), &product_id2 );

	return sprintf ( buf, "H/W ver: 0x%x, F/W ver: 0x%x\n", product_id2, product_id );
}
static DEVICE_ATTR ( fw, 0664, mms128_show_firmware, NULL );

static ssize_t mms128_show_reset ( struct device *dev, struct device_attribute *attr, char *buf )
{
	mms128_power ( 0 );
	msleep(100);
	mms128_power ( 1 );
	msleep(100);

	return 0;
}

static ssize_t mms128_store_reset ( struct device *dev, struct device_attribute *attr, const char *buffer100, size_t count )
{
	uint8_t buf[TS_READ_REGS_LEN] = { 0, };
	int read_num = 0;

	mms128_i2c_read ( melfas_i2c_client, TS_READ_LEN_ADDR, 1, buf );
	read_num = buf[0];
	if ( read_num )
	{
		mms128_i2c_read ( melfas_i2c_client, TS_READ_START_ADDR, read_num, buf );

		if ( 0x0F == buf[0] )
		{
			mms128_touch_reset ();
		}
	}

	return count;
}
static DEVICE_ATTR ( reset, 0664, mms128_show_reset, mms128_store_reset );

/****************************************************************************
* I2C BUS Related Functions
****************************************************************************/
static int mms128_i2c_probe ( struct i2c_client *client, const struct i2c_device_id *id )
{
    int i, err = 0, ret = 0;
	struct task_struct *thread = NULL;
	int gpio_touch_id = 0;

	TPD_FUN();

	/* Turn on the power for MMS128 */
	mms128_power ( 1 );
    msleep(200);
	msleep(100);

    melfas_i2c_client = client;

	err = device_create_file ( &client->dev, &dev_attr_update );
	if ( err )
	{
		TPD_ERR ( "Touchscreen : update_touch device_create_file: Fail\n" );
		device_remove_file ( &client->dev, &dev_attr_update );
		return err;
	}

	err = device_create_file ( &client->dev, &dev_attr_fw );
	if ( err )
	{
		TPD_ERR ( "Touchscreen : fw_touch device_create_file: Fail\n" );
		device_remove_file ( &client->dev, &dev_attr_fw );
		return err;
	}

	err = device_create_file ( &client->dev, &dev_attr_reset );
	if ( err )
	{
		TPD_ERR ( "Touchscreen : reset_touch device_create_file: Fail\n" );
		device_remove_file ( &client->dev, &dev_attr_reset );
		return err;
	}

//	gpio_touch_id = mt_get_gpio_in ( GPIO_TOUCH_MAKER_ID );
	TPD_LOG ( "TOUCH_ID[%d]\n", gpio_touch_id );

#ifdef TPD_HAVE_BUTTON
	for ( i = 0 ; i < TPD_KEY_COUNT ; i++ )
	{
		input_set_capability ( tpd->dev, EV_KEY, tpd_keys_local[i] );
	}
#endif

	/* Touch Firmware Update */
	//ret = mms128_firmware_update ( client );

    thread = kthread_run ( mms128_event_handler, 0, TPD_DEVICE );
    if ( IS_ERR ( thread ) )
    {
        err = PTR_ERR ( thread );
        TPD_ERR ( "failed to create kernel thread: %d\n", err );
    }

	/* Configure external ( GPIO ) interrupt */
	mms128_setup_eint ();

    tpd_load_status = 1;

	/* ghost finger pattern detection */
	INIT_DELAYED_WORK ( &ghost_monitor_work, mms128_monitor_ghost_finger );
	schedule_delayed_work ( &ghost_monitor_work, msecs_to_jiffies ( HZ * 50 ) );

	if ( ret == 0 )
	{
		mms128_read_dummy_interrupt ();
	}

    return 0;
}

static int mms128_i2c_remove ( struct i2c_client *client )
{
    return 0;
}

static int mms128_i2c_detect ( struct i2c_client *client, struct i2c_board_info *info )
{
    strcpy ( info->type, "mtk-tpd" );

	TPD_FUN ();
	
    return 0;
}

static const struct i2c_device_id mms128_i2c_id[] = { { TP_DEV_NAME, 0 }, {} };

static struct i2c_driver mms128_i2c_driver = {
	.driver.name = "mtk-tpd",
	.probe = mms128_i2c_probe,
    .remove = __devexit_p(mms128_i2c_remove),
    .detect = mms128_i2c_detect,
    .id_table = mms128_i2c_id,
};

/****************************************************************************
* Linux Device Driver Related Functions
****************************************************************************/
static int mms128_local_init ( void )
{
	TPD_FUN ();
    if ( i2c_add_driver ( &mms128_i2c_driver ) != 0 )
    {
        TPD_ERR ( "unable to add i2c driver.\n" );
        return -1;
    }

    if ( tpd_load_status == 0 )
    {
    	TPD_ERR ( "add error touch panel driver.\n" );
    	i2c_del_driver ( &mms128_i2c_driver );
    	return -1;
    }

    tpd_type_cap = 1;

    return 0;
}

static void mms128_suspend ( struct early_suspend *h )
{
	TPD_FUN ();

    mms128_release_all_finger ();

	/* mask external interrupt */
	mt65xx_eint_mask ( CUST_EINT_TOUCH_PANEL_NUM );

	/* Turn off the power for MMS128 */
	mms128_power ( 0 );

	/* ghost finger pattern detection */
	cancel_delayed_work ( &ghost_monitor_work );
}

static void mms128_resume ( struct early_suspend *h )
{
	TPD_FUN ();

	/* Turn on the power for MMS128 */
	mms128_power ( 1 );

	mms128_release_all_finger ();
	msleep ( 100 );
	mms128_read_dummy_interrupt ();

	/* unmask external interrupt */
	mt65xx_eint_unmask ( CUST_EINT_TOUCH_PANEL_NUM );

	/* ghost finger pattern detection */
	ghost_touch_cnt = 0;
	schedule_delayed_work ( &ghost_monitor_work, msecs_to_jiffies ( HZ * 50 ) );
}

static struct i2c_board_info __initdata i2c_MMS128={ I2C_BOARD_INFO ( TP_DEV_NAME, MELFAS_I2C_ADDRESS ) };

static struct tpd_driver_t mms128_device_driver = {
    .tpd_device_name = "mms128",
    .tpd_local_init = mms128_local_init,
    .suspend = mms128_suspend,
    .resume = mms128_resume,
#ifdef TPD_HAVE_BUTTON
    .tpd_have_button = 1,
#else
    .tpd_have_button = 0,
#endif		
};

static int __init mms128_driver_init ( void )
{
    TPD_FUN ();

	i2c_register_board_info ( 1, &i2c_MMS128, 1 );
    if ( tpd_driver_add ( &mms128_device_driver ) < 0 )
    {
        TPD_ERR ( "melfas driver add failed\n" );
    }

    return 0;
}

static void __exit mms128_driver_exit ( void )
{
    TPD_FUN ();

    tpd_driver_remove ( &mms128_device_driver );
}


module_init ( mms128_driver_init );
module_exit ( mms128_driver_exit );

MODULE_AUTHOR ( "Kang Jun Mo" );
MODULE_DESCRIPTION ( "mms128 driver" );
MODULE_LICENSE ( "GPL" );

/* End Of File */
