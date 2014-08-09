/*
 * MStar MSG21XX touchscreen driver
 *
 * Copyright (c) 2006-2012 MStar Semiconductor, Inc.
 *
 * Copyright (C) 2012 Bruce Ding <bruce.ding@mstarsemi.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/timer.h>
#include <linux/gpio.h>

#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <mach/gpio.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/string.h>
#include <asm/unistd.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#if defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif
#include <linux/input.h>
#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#endif
#ifdef  CONFIG_TOUCHSCREEN_PROXIMITY_SENSOR
#include <linux/input/vir_ps.h>
#endif

/*=============================================================*/
// Macro Definition
/*=============================================================*/

#define TOUCH_DRIVER_DEBUG 0
#if (TOUCH_DRIVER_DEBUG == 1)
#define DBG(fmt, arg...) pr_err(fmt, ##arg) //pr_info(fmt, ##arg)
#else
#define DBG(fmt, arg...)
#endif

/*=============================================================*/
// Constant Value & Variable Definition
/*=============================================================*/

#define U8         unsigned char
#define U16        unsigned short
#define U32        unsigned int
#define S8         signed char
#define S16        signed short
#define S32        signed int

#define TOUCH_SCREEN_X_MIN   (0)
#define TOUCH_SCREEN_Y_MIN   (0)
/*
 * Note.
 * Please change the below touch screen resolution according to the touch panel that you are using.
 */
#define TOUCH_SCREEN_X_MAX   (480)
#define TOUCH_SCREEN_Y_MAX   (800)
/*
 * Note.
 * Please do not change the below setting.
 */
#define TPD_WIDTH   (2048)
#define TPD_HEIGHT  (2048)

/*
 * Note.
 * Please change the below GPIO pin setting to follow the platform that you are using
 */
static int int_gpio = 1;
static int reset_gpio = 0;
#define MS_TS_MSG21XX_GPIO_RST   reset_gpio
#define MS_TS_MSG21XX_GPIO_INT   int_gpio
//---------------------------------------------------------------------//

//#define SYSFS_AUTHORITY_CHANGE_FOR_CTS_TEST

#ifdef SYSFS_AUTHORITY_CHANGE_FOR_CTS_TEST
#define SYSFS_AUTHORITY (0644)
#else
#define SYSFS_AUTHORITY (0777)
#endif

#define FIRMWARE_AUTOUPDATE
#ifdef FIRMWARE_AUTOUPDATE
typedef enum {
    SWID_START = 1,
    SWID_TRULY = SWID_START,
    SWID_NULL,
} SWID_ENUM;

unsigned char MSG_FIRMWARE[1][33*1024] =
{
    {
        #include "msg21xx_truly_update_bin.h"
    }
};
#endif

#define CONFIG_TP_HAVE_KEY

/*
 * Note.
 * If the below virtual key value definition are not consistent with those that defined in key layout file of platform,
 * please change the below virtual key value to follow the platform that you are using.
 */
#ifdef CONFIG_TP_HAVE_KEY
#define TOUCH_KEY_MENU (139) //229
#define TOUCH_KEY_HOME (172) //102
#define TOUCH_KEY_BACK (158)
#define TOUCH_KEY_SEARCH (217)

const U16 tp_key_array[] = {TOUCH_KEY_MENU, TOUCH_KEY_HOME, TOUCH_KEY_BACK, TOUCH_KEY_SEARCH};
#define MAX_KEY_NUM (sizeof(tp_key_array)/sizeof(tp_key_array[0]))
#endif

#define SLAVE_I2C_ID_DBBUS         (0xC4>>1)
#define SLAVE_I2C_ID_DWI2C      (0x4C>>1)

#define DEMO_MODE_PACKET_LENGTH    (8)
#define MAX_TOUCH_NUM           (2)     //5

#define TP_PRINT
#ifdef TP_PRINT
static int tp_print_proc_read(void);
static void tp_print_create_entry(void);
#endif

static char *fw_version = NULL; // customer firmware version
static U16 fw_version_major = 0;
static U16 fw_version_minor = 0;
static U8 temp[94][1024];
static U32 crc32_table[256];
static int FwDataCnt = 0;
static U8 bFwUpdating = 0;
static struct class *firmware_class = NULL;
static struct device *firmware_cmd_dev = NULL;

static struct i2c_client *i2c_client = NULL;

#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
static struct notifier_block msg21xx_fb_notif;
#elif defined (CONFIG_HAS_EARLYSUSPEND)
static struct early_suspend mstar_ts_early_suspend;
#endif

#ifdef CONFIG_TOUCHSCREEN_PROXIMITY_SENSOR
static U8 bEnableTpProximity = 0;
static U8 bFaceClosingTp = 0;
#endif
static U8 bTpInSuspend = 0;

static int irq_msg21xx = -1;
static struct work_struct msg21xx_wk;
static struct mutex msg21xx_mutex;
static struct input_dev *input_dev = NULL;

/*=============================================================*/
// Data Type Definition
/*=============================================================*/

typedef struct
{
    U16 x;
    U16 y;
}   touchPoint_t;

/// max 80+1+1 = 82 bytes
typedef struct
{
    touchPoint_t point[MAX_TOUCH_NUM];
    U8 count;
    U8 keycode;
}   touchInfo_t;

enum i2c_speed
{
    I2C_SLOW = 0,
    I2C_NORMAL = 1, /* Enable erasing/writing for 10 msec. */
    I2C_FAST = 2,   /* Disable EWENB before 10 msec timeout. */
};

typedef enum
{
    EMEM_ALL = 0,
    EMEM_MAIN,
    EMEM_INFO,
} EMEM_TYPE_t;

/*=============================================================*/
// Function Definition
/*=============================================================*/

/// CRC
static U32 _CRC_doReflect(U32 ref, S8 ch)
{
    U32 value = 0;
    U32 i = 0;

    for (i = 1; i < (ch + 1); i ++)
    {
        if (ref & 1)
        {
            value |= 1 << (ch - i);
        }
        ref >>= 1;
    }

    return value;
}

U32 _CRC_getValue(U32 text, U32 prevCRC)
{
    U32 ulCRC = prevCRC;

    ulCRC = (ulCRC >> 8) ^ crc32_table[(ulCRC & 0xFF) ^ text];

    return ulCRC;
}

static void _CRC_initTable(void)
{
    U32 magic_number = 0x04c11db7;
    U32 i, j;

    for (i = 0; i <= 0xFF; i ++)
    {
        crc32_table[i] = _CRC_doReflect (i, 8) << 24;
        for (j = 0; j < 8; j ++)
        {
            crc32_table[i] = (crc32_table[i] << 1) ^ (crc32_table[i] & (0x80000000L) ? magic_number : 0);
        }
        crc32_table[i] = _CRC_doReflect(crc32_table[i], 32);
    }
}

static void reset_hw(void)
{
    DBG("reset_hw()\n");

    gpio_direction_output(MS_TS_MSG21XX_GPIO_RST, 1);
    gpio_set_value(MS_TS_MSG21XX_GPIO_RST, 0);
    mdelay(100);     /* Note that the RST must be in LOW 10ms at least */
    gpio_set_value(MS_TS_MSG21XX_GPIO_RST, 1);
    mdelay(100);     /* Enable the interrupt service thread/routine for INT after 50ms */
}

static int read_i2c_seq(U8 addr, U8* buf, U16 size)
{
    int rc = 0;
    struct i2c_msg msgs[] =
    {
        {
            .addr = addr,
            .flags = I2C_M_RD, // read flag
            .len = size,
            .buf = buf,
        },
    };

    /* If everything went ok (i.e. 1 msg transmitted), return #bytes
       transmitted, else error code. */
    if (i2c_client != NULL)
    {
        rc = i2c_transfer(i2c_client->adapter, msgs, 1);
        if (rc < 0)
        {
            DBG("read_i2c_seq() error %d\n", rc);
        }
    }
    else
    {
        DBG("i2c_client is NULL\n");
    }

    return rc;
}

static int write_i2c_seq(U8 addr, U8* buf, U16 size)
{
    int rc = 0;
    struct i2c_msg msgs[] =
    {
        {
            .addr = addr,
            .flags = 0, // if read flag is undefined, then it means write flag.
            .len = size,
            .buf = buf,
        },
    };

    /* If everything went ok (i.e. 1 msg transmitted), return #bytes
       transmitted, else error code. */
    if (i2c_client != NULL)
    {
        rc = i2c_transfer(i2c_client->adapter, msgs, 1);
        if ( rc < 0 )
        {
            DBG("write_i2c_seq() error %d\n", rc);
        }
    }
    else
    {
        DBG("i2c_client is NULL\n");
    }

    return rc;
}

static U16 read_reg(U8 bank, U8 addr)
{
    U8 tx_data[3] = {0x10, bank, addr};
    U8 rx_data[2] = {0};

    write_i2c_seq(SLAVE_I2C_ID_DBBUS, &tx_data[0], 3);
    read_i2c_seq(SLAVE_I2C_ID_DBBUS, &rx_data[0], 2);

    return (rx_data[1] << 8 | rx_data[0]);
}

static void write_reg(U8 bank, U8 addr, U16 data)
{
    U8 tx_data[5] = {0x10, bank, addr, data & 0xFF, data >> 8};
    write_i2c_seq(SLAVE_I2C_ID_DBBUS, &tx_data[0], 5);
}

static void write_reg_8bit(U8 bank, U8 addr, U8 data)
{
    U8 tx_data[4] = {0x10, bank, addr, data};
    write_i2c_seq(SLAVE_I2C_ID_DBBUS, &tx_data[0], 4);
}

void dbbusDWIICEnterSerialDebugMode(void)
{
    U8 data[5];

    // Enter the Serial Debug Mode
    data[0] = 0x53;
    data[1] = 0x45;
    data[2] = 0x52;
    data[3] = 0x44;
    data[4] = 0x42;

    write_i2c_seq(SLAVE_I2C_ID_DBBUS, data, 5);
}

void dbbusDWIICStopMCU(void)
{
    U8 data[1];

    // Stop the MCU
    data[0] = 0x37;

    write_i2c_seq(SLAVE_I2C_ID_DBBUS, data, 1);
}

void dbbusDWIICIICUseBus(void)
{
    U8 data[1];

    // IIC Use Bus
    data[0] = 0x35;

    write_i2c_seq(SLAVE_I2C_ID_DBBUS, data, 1);
}

void dbbusDWIICIICReshape(void)
{
    U8 data[1];

    // IIC Re-shape
    data[0] = 0x71;

    write_i2c_seq(SLAVE_I2C_ID_DBBUS, data, 1);
}

void dbbusDWIICIICNotUseBus(void)
{
    U8 data[1];

    // IIC Not Use Bus
    data[0] = 0x34;

    write_i2c_seq(SLAVE_I2C_ID_DBBUS, data, 1);
}

void dbbusDWIICNotStopMCU(void)
{
    U8 data[1];

    // Not Stop the MCU
    data[0] = 0x36;

    write_i2c_seq(SLAVE_I2C_ID_DBBUS, data, 1);
}

void dbbusDWIICExitSerialDebugMode(void)
{
    U8 data[1];

    // Exit the Serial Debug Mode
    data[0] = 0x45;

    write_i2c_seq(SLAVE_I2C_ID_DBBUS, data, 1);

    // Delay some interval to guard the next transaction
    //udelay ( 200 );        // delay about 0.2ms
}

//---------------------------------------------------------------------//

static U8 get_ic_type(void)
{
    U8 ic_type = 0;

    reset_hw();
    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    mdelay ( 300 );

    // stop mcu
    write_reg_8bit ( 0x0F, 0xE6, 0x01 );
    // disable watch dog
    write_reg ( 0x3C, 0x60, 0xAA55 );
    // get ic type
    ic_type = (0xff)&(read_reg(0x1E, 0xCC));

    if (ic_type != 1        //msg2133
        && ic_type != 2     //msg21xxA
        && ic_type !=  3)   //msg26xxM
    {
        ic_type = 0;
    }

    reset_hw();

    return ic_type;
}

static int get_customer_firmware_version(void)
{
    U8 dbbus_tx_data[3] = {0};
    U8 dbbus_rx_data[4] = {0};
    int ret = 0;

    DBG("get_customer_firmware_version()\n");

    dbbus_tx_data[0] = 0x53;
    dbbus_tx_data[1] = 0x00;
    dbbus_tx_data[2] = 0x2A;
    mutex_lock(&msg21xx_mutex);
    write_i2c_seq(SLAVE_I2C_ID_DWI2C, &dbbus_tx_data[0], 3);
    read_i2c_seq(SLAVE_I2C_ID_DWI2C, &dbbus_rx_data[0], 4);
    mutex_unlock(&msg21xx_mutex);
    fw_version_major = (dbbus_rx_data[1]<<8) + dbbus_rx_data[0];
    fw_version_minor = (dbbus_rx_data[3]<<8) + dbbus_rx_data[2];

    DBG("*** major = %d ***\n", fw_version_major);
    DBG("*** minor = %d ***\n", fw_version_minor);

    if (fw_version == NULL)
    {
        fw_version = kzalloc(sizeof(char), GFP_KERNEL);
    }

    sprintf(fw_version, "%03d%03d", fw_version_major, fw_version_minor);

    return ret;
}

static int firmware_erase_c33 ( EMEM_TYPE_t emem_type )
{
    // stop mcu
    write_reg ( 0x0F, 0xE6, 0x0001 );

    //disable watch dog
    write_reg_8bit ( 0x3C, 0x60, 0x55 );
    write_reg_8bit ( 0x3C, 0x61, 0xAA );

    // set PROGRAM password
    write_reg_8bit ( 0x16, 0x1A, 0xBA );
    write_reg_8bit ( 0x16, 0x1B, 0xAB );

    write_reg_8bit ( 0x16, 0x18, 0x80 );

    if ( emem_type == EMEM_ALL )
    {
        write_reg_8bit ( 0x16, 0x08, 0x10 ); //mark
    }

    write_reg_8bit ( 0x16, 0x18, 0x40 );
    mdelay ( 10 );

    // clear pce
    write_reg_8bit ( 0x16, 0x18, 0x80 );

    // erase trigger
    if ( emem_type == EMEM_MAIN )
    {
        write_reg_8bit ( 0x16, 0x0E, 0x04 ); //erase main
    }
    else
    {
        write_reg_8bit ( 0x16, 0x0E, 0x08 ); //erase all block
    }

    return ( 1 );
}

static ssize_t firmware_update_c33 ( struct device *dev, struct device_attribute *attr,
                                     const char *buf, size_t size, EMEM_TYPE_t emem_type )
{
    U32 i, j;
    U32 crc_main, crc_main_tp;
    U32 crc_info, crc_info_tp;
    U16 reg_data = 0;
    int update_pass = 1;

    crc_main = 0xffffffff;
    crc_info = 0xffffffff;

    reset_hw();
    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    mdelay ( 300 );

    //erase main
    firmware_erase_c33 ( EMEM_MAIN );
    mdelay ( 1000 );

    reset_hw();
    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    mdelay ( 300 );

    /////////////////////////
    // Program
    /////////////////////////

    //polling 0x3CE4 is 0x1C70
    if ( ( emem_type == EMEM_ALL ) || ( emem_type == EMEM_MAIN ) )
    {
        do
        {
            reg_data = read_reg ( 0x3C, 0xE4 );
        }
        while ( reg_data != 0x1C70 );
    }

    switch ( emem_type )
    {
        case EMEM_ALL:
            write_reg ( 0x3C, 0xE4, 0xE38F );  // for all-blocks
            break;
        case EMEM_MAIN:
            write_reg ( 0x3C, 0xE4, 0x7731 );  // for main block
            break;
        case EMEM_INFO:
            write_reg ( 0x3C, 0xE4, 0x7731 );  // for info block

            write_reg_8bit ( 0x0F, 0xE6, 0x01 );

            write_reg_8bit ( 0x3C, 0xE4, 0xC5 );
            write_reg_8bit ( 0x3C, 0xE5, 0x78 );

            write_reg_8bit ( 0x1E, 0x04, 0x9F );
            write_reg_8bit ( 0x1E, 0x05, 0x82 );

            write_reg_8bit ( 0x0F, 0xE6, 0x00 );
            mdelay ( 100 );
            break;
    }

    // polling 0x3CE4 is 0x2F43
    do
    {
        reg_data = read_reg ( 0x3C, 0xE4 );
    }
    while ( reg_data != 0x2F43 );

    // calculate CRC 32
    _CRC_initTable ();

    for ( i = 0; i < 32; i++ ) // total  32 KB : 2 byte per R/W
    {
        if ( i == 31 )
        {
            temp[i][1014] = 0x5A;
            temp[i][1015] = 0xA5;

            for ( j = 0; j < 1016; j++ )
            {
                crc_main = _CRC_getValue ( temp[i][j], crc_main);
            }
        }
        else
        {
            for ( j = 0; j < 1024; j++ )
            {
                crc_main = _CRC_getValue ( temp[i][j], crc_main);
            }
        }

        //write_i2c_seq(SLAVE_I2C_ID_DWI2C, temp[i], 1024);
        for (j = 0; j < 8; j++)
        {
            write_i2c_seq(SLAVE_I2C_ID_DWI2C, &temp[i][j*128], 128 );
        }
        msleep (100);

        // polling 0x3CE4 is 0xD0BC
        do
        {
            reg_data = read_reg ( 0x3C, 0xE4 );
        }
        while ( reg_data != 0xD0BC );

        write_reg ( 0x3C, 0xE4, 0x2F43 );
    }

    if ( ( emem_type == EMEM_ALL ) || ( emem_type == EMEM_MAIN ) )
    {
        // write file done and check crc
        write_reg ( 0x3C, 0xE4, 0x1380 );
    }
    mdelay ( 10 );

    if ( ( emem_type == EMEM_ALL ) || ( emem_type == EMEM_MAIN ) )
    {
        // polling 0x3CE4 is 0x9432
        do
        {
            reg_data = read_reg ( 0x3C, 0xE4 );
        }while ( reg_data != 0x9432 );
    }

    crc_main = crc_main ^ 0xffffffff;
    crc_info = crc_info ^ 0xffffffff;

    if ( ( emem_type == EMEM_ALL ) || ( emem_type == EMEM_MAIN ) )
    {
        // CRC Main from TP
        crc_main_tp = read_reg ( 0x3C, 0x80 );
        crc_main_tp = ( crc_main_tp << 16 ) | read_reg ( 0x3C, 0x82 );

        // CRC Info from TP
        crc_info_tp = read_reg ( 0x3C, 0xA0 );
        crc_info_tp = ( crc_info_tp << 16 ) | read_reg ( 0x3C, 0xA2 );
    }

    update_pass = 1;
    if ( ( emem_type == EMEM_ALL ) || ( emem_type == EMEM_MAIN ) )
    {
        if ( crc_main_tp != crc_main )
            update_pass = 0;

        /*
        if ( crc_info_tp != crc_info )
            update_pass = 0;
        */
    }

    if ( !update_pass )
    {
        DBG( "update_C33 failed\n" );
        reset_hw();
        FwDataCnt = 0;
        return 0;
    }

    DBG( "update_C33 OK\n" );
    reset_hw();
    FwDataCnt = 0;
    return size;
}

#ifdef FIRMWARE_AUTOUPDATE
unsigned short main_sw_id = 0x7FF, info_sw_id = 0x7FF;
U32 bin_conf_crc32 = 0;

static U32 _CalMainCRC32(void)
{
    U32 ret=0;
    U16  reg_data=0;

    reset_hw();

    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    msleep ( 100 );

    //Stop MCU
    write_reg ( 0x0F, 0xE6, 0x0001 );

    // Stop Watchdog
    write_reg_8bit ( 0x3C, 0x60, 0x55 );
    write_reg_8bit ( 0x3C, 0x61, 0xAA );

    //cmd
    write_reg ( 0x3C, 0xE4, 0xDF4C );
    write_reg ( 0x1E, 0x04, 0x7d60 );
    // TP SW reset
    write_reg ( 0x1E, 0x04, 0x829F );

    //MCU run
    write_reg ( 0x0F, 0xE6, 0x0000 );

    //polling 0x3CE4
    do
    {
        reg_data = read_reg ( 0x3C, 0xE4 );
    }while ( reg_data != 0x9432 );

    // Cal CRC Main from TP
    ret = read_reg ( 0x3C, 0x80 );
    ret = ( ret << 16 ) | read_reg ( 0x3C, 0x82 );

    DBG("[21xxA]:Current main crc32=0x%x\n",ret);
    return (ret);
}

static void _ReadBinConfig ( void )
{
    U8  dbbus_tx_data[5]={0};
    U8  dbbus_rx_data[4]={0};
    U16 reg_data=0;

    reset_hw();

    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    msleep ( 100 );

    //Stop MCU
    write_reg ( 0x0F, 0xE6, 0x0001 );

    // Stop Watchdog
    write_reg_8bit ( 0x3C, 0x60, 0x55 );
    write_reg_8bit ( 0x3C, 0x61, 0xAA );

    //cmd
    write_reg ( 0x3C, 0xE4, 0xA4AB );
    write_reg ( 0x1E, 0x04, 0x7d60 );

    // TP SW reset
    write_reg ( 0x1E, 0x04, 0x829F );

    //MCU run
    write_reg ( 0x0F, 0xE6, 0x0000 );

   //polling 0x3CE4
    do
    {
        reg_data = read_reg ( 0x3C, 0xE4 );
    }
    while ( reg_data != 0x5B58 );

    dbbus_tx_data[0] = 0x72;
    dbbus_tx_data[1] = 0x7F;
    dbbus_tx_data[2] = 0x55;
    dbbus_tx_data[3] = 0x00;
    dbbus_tx_data[4] = 0x04;
    write_i2c_seq(SLAVE_I2C_ID_DWI2C, &dbbus_tx_data[0], 5 );
    read_i2c_seq(SLAVE_I2C_ID_DWI2C, &dbbus_rx_data[0], 4 );
    if ((dbbus_rx_data[0]>=0x30 && dbbus_rx_data[0]<=0x39)
        &&(dbbus_rx_data[1]>=0x30 && dbbus_rx_data[1]<=0x39)
        &&(dbbus_rx_data[2]>=0x31 && dbbus_rx_data[2]<=0x39))
    {
    	main_sw_id = (dbbus_rx_data[0]-0x30)*100+(dbbus_rx_data[1]-0x30)*10+(dbbus_rx_data[2]-0x30);
    }

    dbbus_tx_data[0] = 0x72;
    dbbus_tx_data[1] = 0x7F;
    dbbus_tx_data[2] = 0xFC;
    dbbus_tx_data[3] = 0x00;
    dbbus_tx_data[4] = 0x04;
    write_i2c_seq(SLAVE_I2C_ID_DWI2C, &dbbus_tx_data[0], 5 );
    read_i2c_seq(SLAVE_I2C_ID_DWI2C, &dbbus_rx_data[0], 4 );
    bin_conf_crc32 = dbbus_rx_data[0];
    bin_conf_crc32 = (bin_conf_crc32<<8)|dbbus_rx_data[1];
    bin_conf_crc32 = (bin_conf_crc32<<8)|dbbus_rx_data[2];
    bin_conf_crc32 = (bin_conf_crc32<<8)|dbbus_rx_data[3];

    dbbus_tx_data[0] = 0x72;
    dbbus_tx_data[1] = 0x83;
    dbbus_tx_data[2] = 0x00;
    dbbus_tx_data[3] = 0x00;
    dbbus_tx_data[4] = 0x04;
    write_i2c_seq(SLAVE_I2C_ID_DWI2C, &dbbus_tx_data[0], 5 );
    read_i2c_seq(SLAVE_I2C_ID_DWI2C, &dbbus_rx_data[0], 4 );
    if ((dbbus_rx_data[0]>=0x30 && dbbus_rx_data[0]<=0x39)
        &&(dbbus_rx_data[1]>=0x30 && dbbus_rx_data[1]<=0x39)
        &&(dbbus_rx_data[2]>=0x31 && dbbus_rx_data[2]<=0x39))
    {
    	info_sw_id = (dbbus_rx_data[0]-0x30)*100+(dbbus_rx_data[1]-0x30)*10+(dbbus_rx_data[2]-0x30);
    }

    DBG("[21xxA]:main_sw_id = %d, info_sw_id = %d, bin_conf_crc32=0x%x\n", main_sw_id, info_sw_id, bin_conf_crc32);
}

static int fwAutoUpdate(void *unused)
{
    int time = 0;
    ssize_t ret = 0;

    for (time = 0; time < 5; time++)
    {
        DBG("fwAutoUpdate time = %d\n",time);
        ret = firmware_update_c33(NULL, NULL, NULL, 1, EMEM_MAIN);
        if (ret == 1)
        {
            DBG("AUTO_UPDATE OK!!!");
            break;
        }
    }
    if (time == 5)
    {
        DBG("AUTO_UPDATE failed!!!");
    }
    enable_irq(irq_msg21xx);
    return 0;
}
#endif

//------------------------------------------------------------------------------//
static ssize_t firmware_update_show(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
    DBG("*** firmware_update_show() fw_version = %s ***\n", fw_version);

    return sprintf(buf, "%s\n", fw_version);
}

static ssize_t firmware_update_store(struct device *dev,
                                     struct device_attribute *attr, const char *buf, size_t size)
{
    bFwUpdating = 1;
    disable_irq(irq_msg21xx);

    DBG("*** update fw size = %d ***\n", FwDataCnt);
    size = firmware_update_c33 (dev, attr, buf, size, EMEM_MAIN);

    enable_irq(irq_msg21xx);
    bFwUpdating = 0;

    return size;
}

static DEVICE_ATTR(update, SYSFS_AUTHORITY, firmware_update_show, firmware_update_store);

static ssize_t firmware_version_show(struct device *dev,
                                     struct device_attribute *attr, char *buf)
{
    DBG("*** firmware_version_show() fw_version = %s ***\n", fw_version);

    return sprintf(buf, "%s\n", fw_version);
}

static ssize_t firmware_version_store(struct device *dev,
                                      struct device_attribute *attr, const char *buf, size_t size)
{
    get_customer_firmware_version();

    DBG("*** firmware_version_store() fw_version = %s ***\n", fw_version);

    return size;
}

static DEVICE_ATTR(version, SYSFS_AUTHORITY, firmware_version_show, firmware_version_store);

static ssize_t firmware_data_show(struct device *dev,
                                  struct device_attribute *attr, char *buf)
{
    DBG("*** firmware_data_show() FwDataCnt = %d ***\n", FwDataCnt);

    return FwDataCnt;
}

static ssize_t firmware_data_store(struct device *dev,
                                   struct device_attribute *attr, const char *buf, size_t size)
{
    int count = size / 1024;
    int i;

    for (i = 0; i < count; i ++)
    {
        memcpy(temp[FwDataCnt], buf+(i*1024), 1024);

        FwDataCnt ++;
    }

    DBG("***FwDataCnt = %d ***\n", FwDataCnt);

    if (buf != NULL)
    {
        DBG("*** buf[0] = %c ***\n", buf[0]);
    }

    return size;
}

static DEVICE_ATTR(data, SYSFS_AUTHORITY, firmware_data_show, firmware_data_store);

#ifdef TP_PRINT
static ssize_t tp_print_show(struct device *dev,
                                     struct device_attribute *attr, char *buf)
{
    tp_print_proc_read();

    return sprintf(buf, "%d\n", bTpInSuspend);
}

static ssize_t tp_print_store(struct device *dev,
                                      struct device_attribute *attr, const char *buf, size_t size)
{
    DBG("*** tp_print_store() ***\n");

    return size;
}

static DEVICE_ATTR(tpp, SYSFS_AUTHORITY, tp_print_show, tp_print_store);
#endif

//------------------------------------------------------------------------------//
#ifdef CONFIG_TOUCHSCREEN_PROXIMITY_SENSOR
static void _msg_enable_proximity(void)
{
    U8 tx_data[4] = {0};

    DBG("_msg_enable_proximity!");
    tx_data[0] = 0x52;
    tx_data[1] = 0x00;
    tx_data[2] = 0x47;
    tx_data[3] = 0xa0;
    mutex_lock(&msg21xx_mutex);
    write_i2c_seq(SLAVE_I2C_ID_DWI2C, &tx_data[0], 4);
    mutex_unlock(&msg21xx_mutex);

    bEnableTpProximity = 1;
}

static void _msg_disable_proximity(void)
{
    U8 tx_data[4] = {0};

    DBG("_msg_disable_proximity!");
    tx_data[0] = 0x52;
    tx_data[1] = 0x00;
    tx_data[2] = 0x47;
    tx_data[3] = 0xa1;
    mutex_lock(&msg21xx_mutex);
    write_i2c_seq(SLAVE_I2C_ID_DWI2C, &tx_data[0], 4);
    mutex_unlock(&msg21xx_mutex);

    bEnableTpProximity = 0;
    bFaceClosingTp = 0;
}

void tsps_msg21xx_enable(int en)
{
    if (en)
    {
        _msg_enable_proximity();
    }
    else
    {
        _msg_disable_proximity();
    }
}

int tsps_msg21xx_data(void)
{
    return bFaceClosingTp;
}
#endif

static U8 calculate_checksum(U8 *msg, S32 length)
{
    S32 Checksum = 0;
    S32 i;

    for (i = 0; i < length; i ++)
    {
        Checksum += msg[i];
    }

    return (U8)((-Checksum) & 0xFF);
}

static S32 parse_info(touchInfo_t *info)
{
    U8 data[DEMO_MODE_PACKET_LENGTH] = {0};
    U8 checksum = 0;
    U32 x = 0, y = 0;
    U32 x2 = 0, y2 = 0;
    U32 delta_x = 0, delta_y = 0;

    mutex_lock(&msg21xx_mutex);
    read_i2c_seq(SLAVE_I2C_ID_DWI2C, &data[0], DEMO_MODE_PACKET_LENGTH);
    mutex_unlock(&msg21xx_mutex);
    checksum = calculate_checksum(&data[0], (DEMO_MODE_PACKET_LENGTH-1));
    DBG("check sum: [%x] == [%x]? \n", data[DEMO_MODE_PACKET_LENGTH-1], checksum);

    if(data[DEMO_MODE_PACKET_LENGTH-1] != checksum)
    {
        DBG("WRONG CHECKSUM\n");
        return -1;
    }

    if(data[0] != 0x52)
    {
        DBG("WRONG HEADER\n");
        return -1;
    }

    info->keycode = 0xFF;
    if ((data[1] == 0xFF) && (data[2] == 0xFF) && (data[3] == 0xFF) && (data[4] == 0xFF) && (data[6] == 0xFF))
    {
        if ((data[5] == 0xFF) || (data[5] == 0))
        {
            info->keycode = 0xFF;
        }
        else if ((data[5] == 1) || (data[5] == 2) || (data[5] == 4) || (data[5] == 8))
        {
            if (data[5] == 1)
            {
                info->keycode = 0;
            }
            else if (data[5] == 2)
            {
                info->keycode = 1;
            }
            else if (data[5] == 4)
            {
                info->keycode = 2;
            }
            else if (data[5] == 8)
            {
                info->keycode = 3;
            }
        }
    #ifdef CONFIG_TOUCHSCREEN_PROXIMITY_SENSOR
        else if (bEnableTpProximity &&((data[5] == 0x80) || (data[5] == 0x40)))
        {
            if (data[5] == 0x80)
            {
                bFaceClosingTp = 1;
            }
            else if (data[5] == 0x40)
            {
                bFaceClosingTp = 0;
            }
            DBG("bEnableTpProximity=%d; bFaceClosingTp=%d; data[5]=%x;\n", bEnableTpProximity, bFaceClosingTp, data[5]);
            return -1;
        }
    #endif
        else
        {
            DBG("WRONG KEY\n");
            return -1;
        }
    }
    else
    {
        x = (((data[1] & 0xF0 ) << 4) | data[2]);
        y = ((( data[1] & 0x0F) << 8) | data[3]);
        delta_x = (((data[4] & 0xF0) << 4 ) | data[5]);
        delta_y = (((data[4] & 0x0F) << 8 ) | data[6]);

        if ((delta_x == 0) && (delta_y == 0))
        {
            info->point[0].x = x * TOUCH_SCREEN_X_MAX / TPD_WIDTH;
            info->point[0].y = y * TOUCH_SCREEN_Y_MAX/ TPD_HEIGHT;
            info->count = 1;
        }
        else
        {
            if (delta_x > 2048)
            {
                delta_x -= 4096;
            }
            if (delta_y > 2048)
            {
                delta_y -= 4096;
            }
            x2 = (U32)((S16)x + (S16)delta_x);
            y2 = (U32)((S16)y + (S16)delta_y);
            info->point[0].x = x * TOUCH_SCREEN_X_MAX / TPD_WIDTH;
            info->point[0].y = y * TOUCH_SCREEN_Y_MAX/ TPD_HEIGHT;
            info->point[1].x = x2 * TOUCH_SCREEN_X_MAX / TPD_WIDTH;
            info->point[1].y = y2 * TOUCH_SCREEN_Y_MAX/ TPD_HEIGHT;
            info->count = 2;
        }
    }

    return 0;
}

static void touch_driver_touch_pressed(int x, int y)
{
    DBG("point touch pressed");

    input_report_key(input_dev, BTN_TOUCH, 1);
    input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, 1);
    input_report_abs(input_dev, ABS_MT_POSITION_X, x);
    input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
    input_mt_sync(input_dev);
}

static void touch_driver_touch_released(void)
{
    DBG("point touch released");

    input_report_key(input_dev, BTN_TOUCH, 0);
    input_mt_sync(input_dev);
}

/* read data through I2C then report data to input sub-system when interrupt occurred */
void touch_driver_do_work(struct work_struct *work)
{
    touchInfo_t info;
    int i = 0;
    static int last_keycode = 0xFF;
    static int last_count = 0;

    DBG("touch_driver_do_work()\n");

    memset(&info, 0x0, sizeof(info));
    if (0 == parse_info(&info))
    {
    #ifdef CONFIG_TP_HAVE_KEY
        if (info.keycode != 0xFF)   //key touch pressed
        {
            DBG("touch_driver_do_work() info.keycode=%x, last_keycode=%x, tp_key_array[%d]=%d\n", info.keycode, last_keycode, info.keycode, tp_key_array[info.keycode]);
            if (info.keycode < MAX_KEY_NUM)
            {
                if (info.keycode != last_keycode)
                {
                    DBG("key touch pressed");

                    input_report_key(input_dev, BTN_TOUCH, 1);
                    input_report_key(input_dev, tp_key_array[info.keycode], 1);

                    last_keycode = info.keycode;
                }
                else
                {
                    /// pass duplicate key-pressing
                    DBG("REPEATED KEY\n");
                }
            }
            else
            {
                DBG("WRONG KEY\n");
            }
        }
        else                        //key touch released
        {
            if (last_keycode != 0xFF)
            {
                DBG("key touch released");

                input_report_key(input_dev, BTN_TOUCH, 0);
                input_report_key(input_dev, tp_key_array[last_keycode], 0);

                last_keycode = 0xFF;
            }
        }
    #endif //CONFIG_TP_HAVE_KEY

        if (info.count > 0)          //point touch pressed
        {
            for (i = 0; i < info.count; i ++)
            {
                touch_driver_touch_pressed(info.point[i].x, info.point[i].y);
            }
            last_count = info.count;
        }
        else if (last_count > 0)                        //point touch released
        {
            touch_driver_touch_released();
            last_count = 0;
        }

        input_sync(input_dev);
    }

    enable_irq(irq_msg21xx);
}

/* The interrupt service routine will be triggered when interrupt occurred */
irqreturn_t touch_driver_isr(int irq, void *dev_id)
{
    DBG("touch_driver_isr()\n");

    disable_irq_nosync(irq_msg21xx);
    schedule_work(&msg21xx_wk);

    return IRQ_HANDLED;
}

#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
    struct fb_event *evdata = data;
    int *blank;

    if (evdata && evdata->data && event == FB_EVENT_BLANK )
    {
        blank = evdata->data;
        if (*blank == FB_BLANK_UNBLANK)
        {
            if (bTpInSuspend)
            {
                gpio_direction_output(MS_TS_MSG21XX_GPIO_RST, 1);
                mdelay(10);
                gpio_set_value(MS_TS_MSG21XX_GPIO_RST, 0);
                mdelay(10);
                gpio_set_value(MS_TS_MSG21XX_GPIO_RST, 1);
                mdelay(200);

                touch_driver_touch_released();
                input_sync(input_dev);

                enable_irq(irq_msg21xx);
            }
            bTpInSuspend = 0;
        }
        else if (*blank == FB_BLANK_POWERDOWN)
        {
            if (bFwUpdating)
            {
                DBG("suspend bFwUpdating=%d\n", bFwUpdating);
                return 0;
            }

        #ifdef CONFIG_TOUCHSCREEN_PROXIMITY_SENSOR
            if (bEnableTpProximity)
            {
                DBG("suspend bEnableTpProximity=%d\n", bEnableTpProximity);
                return 0;
            }
        #endif

            if (bTpInSuspend == 0)
            {
                disable_irq(irq_msg21xx);
                gpio_set_value(MS_TS_MSG21XX_GPIO_RST, 0);
            }
            bTpInSuspend = 1;
        }
    }

    return 0;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
void touch_driver_early_suspend(struct early_suspend *p)
{
    DBG("touch_driver_early_suspend()\n");

    if (bFwUpdating)
    {
        DBG("suspend bFwUpdating=%d\n", bFwUpdating);
        return;
    }

#ifdef CONFIG_TOUCHSCREEN_PROXIMITY_SENSOR
    if (bEnableTpProximity)
    {
        DBG("suspend bEnableTpProximity=%d\n", bEnableTpProximity);
        return;
    }
#endif

    if (bTpInSuspend == 0)
    {
        disable_irq(irq_msg21xx);
        gpio_set_value(MS_TS_MSG21XX_GPIO_RST, 0);
    }
    bTpInSuspend = 1;
}

void touch_driver_early_resume(struct early_suspend *p)
{
    DBG("touch_driver_early_resume() bTpInSuspend=%d\n", bTpInSuspend);

    if (bTpInSuspend)
    {
        gpio_direction_output(MS_TS_MSG21XX_GPIO_RST, 1);
        mdelay(10);
        gpio_set_value(MS_TS_MSG21XX_GPIO_RST, 0);
        mdelay(10);
        gpio_set_value(MS_TS_MSG21XX_GPIO_RST, 1);
        mdelay(200);

        touch_driver_touch_released();
        input_sync(input_dev);

        enable_irq(irq_msg21xx);
    }
    bTpInSuspend = 0;
}
#endif

/* probe function is used for matching and initializing input device */
static int touch_driver_probe(struct i2c_client *client,
        const struct i2c_device_id *id)
{
#ifdef FIRMWARE_AUTOUPDATE
    unsigned short update_bin_major = 0, update_bin_minor = 0;
    int i, update_flag = 0;
#endif
    int ret = 0;

    if (input_dev != NULL)
    {
        DBG("input device has found\n");
        return -1;
    }

    DBG("*** %s ***\n", __FUNCTION__);

    i2c_client = client;

    ret = gpio_request(MS_TS_MSG21XX_GPIO_RST, "reset");
    if (ret < 0)
    {
        pr_err("*** Failed to request GPIO %d, error %d ***\n", MS_TS_MSG21XX_GPIO_RST, ret);
        goto err0;
    }

    // power on TP
    gpio_direction_output(MS_TS_MSG21XX_GPIO_RST, 1);
    mdelay(100);
    gpio_set_value(MS_TS_MSG21XX_GPIO_RST, 0);
    mdelay(10);
    gpio_set_value(MS_TS_MSG21XX_GPIO_RST, 1);
    mdelay(200);
    if (0 == get_ic_type())
    {
        pr_err("the currnet ic is not Mstar\n");
        ret = -1;
        goto err0;
    }

    mutex_init(&msg21xx_mutex);

    /* allocate an input device */
    input_dev = input_allocate_device();
    if (!input_dev)
    {
        ret = -ENOMEM;
        pr_err("*** input device allocation failed ***\n");
        goto err1;
    }

    input_dev->name = client->name;
    input_dev->phys = "I2C";
    input_dev->dev.parent = &client->dev;
    input_dev->id.bustype = BUS_I2C;

    /* set the supported event type for input device */
    set_bit(EV_ABS, input_dev->evbit);
    set_bit(EV_SYN, input_dev->evbit);
    set_bit(EV_KEY, input_dev->evbit);
    set_bit(BTN_TOUCH, input_dev->keybit);
    set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

#ifdef CONFIG_TP_HAVE_KEY
    {
        int i;
        for (i = 0; i < MAX_KEY_NUM; i ++)
        {
            input_set_capability(input_dev, EV_KEY, tp_key_array[i]);
        }
    }
#endif

    input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 2, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_POSITION_X, TOUCH_SCREEN_X_MIN, TOUCH_SCREEN_X_MAX, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_POSITION_Y, TOUCH_SCREEN_Y_MIN, TOUCH_SCREEN_Y_MAX, 0, 0);

    /* register the input device to input sub-system */
    ret = input_register_device(input_dev);
    if (ret < 0)
    {
        pr_err("*** Unable to register ms-touchscreen input device ***\n");
        goto err1;
    }

    /* set sysfs for firmware */
    firmware_class = class_create(THIS_MODULE, "ms-touchscreen-msg20xx"); //client->name
    if (IS_ERR(firmware_class))
        pr_err("Failed to create class(firmware)!\n");

    firmware_cmd_dev = device_create(firmware_class, NULL, 0, NULL, "device");
    if (IS_ERR(firmware_cmd_dev))
        pr_err("Failed to create device(firmware_cmd_dev)!\n");

    // version
    if (device_create_file(firmware_cmd_dev, &dev_attr_version) < 0)
        pr_err("Failed to create device file(%s)!\n", dev_attr_version.attr.name);
    // update
    if (device_create_file(firmware_cmd_dev, &dev_attr_update) < 0)
        pr_err("Failed to create device file(%s)!\n", dev_attr_update.attr.name);
    // data
    if (device_create_file(firmware_cmd_dev, &dev_attr_data) < 0)
        pr_err("Failed to create device file(%s)!\n", dev_attr_data.attr.name);

#ifdef TP_PRINT
    tp_print_create_entry();
#endif

    dev_set_drvdata(firmware_cmd_dev, NULL);

    /* initialize the work queue */
    INIT_WORK(&msg21xx_wk, touch_driver_do_work);

    ret = gpio_request(MS_TS_MSG21XX_GPIO_INT, "interrupt");
    if (ret < 0)
    {
        pr_err("*** Failed to request GPIO %d, error %d ***\n", MS_TS_MSG21XX_GPIO_INT, ret);
        goto err2;
    }
    gpio_direction_input(MS_TS_MSG21XX_GPIO_INT);
    gpio_set_value(MS_TS_MSG21XX_GPIO_INT, 1);

    irq_msg21xx = gpio_to_irq(MS_TS_MSG21XX_GPIO_INT);

    /* request an irq and register the isr */
    ret = request_irq(irq_msg21xx, touch_driver_isr, IRQF_TRIGGER_RISING, "msg21xx", NULL);
    if (ret != 0)
    {
        pr_err("*** Unable to claim irq %d; error %d ***\n", MS_TS_MSG21XX_GPIO_INT, ret);
        goto err3;
    }

    disable_irq(irq_msg21xx);

#if defined(CONFIG_FB)
    msg21xx_fb_notif.notifier_call = fb_notifier_callback;
    ret = fb_register_client(&msg21xx_fb_notif);
#elif defined (CONFIG_HAS_EARLYSUSPEND)
    mstar_ts_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
    mstar_ts_early_suspend.suspend = touch_driver_early_suspend;
    mstar_ts_early_suspend.resume = touch_driver_early_resume;
    register_early_suspend(&mstar_ts_early_suspend);
#endif

#ifdef  CONFIG_TOUCHSCREEN_PROXIMITY_SENSOR
    tsps_assist_register_callback("msg21xx", &tsps_msg21xx_enable, &tsps_msg21xx_data);
#endif

#ifdef FIRMWARE_AUTOUPDATE
    get_customer_firmware_version();
    _ReadBinConfig();

    if (main_sw_id == info_sw_id)
    {
        if (_CalMainCRC32() == bin_conf_crc32)
        {
            if ((main_sw_id >= SWID_START) && (main_sw_id < SWID_NULL))
            {
                update_bin_major= (MSG_FIRMWARE[main_sw_id-SWID_START][0x7f4f] << 8) + MSG_FIRMWARE[main_sw_id-SWID_START][0x7f4e];
                update_bin_minor= (MSG_FIRMWARE[main_sw_id-SWID_START][0x7f51] << 8) + MSG_FIRMWARE[main_sw_id-SWID_START][0x7f50];

                //check upgrading
                if ((update_bin_major == fw_version_major) && (update_bin_minor > fw_version_minor))
                {
                    update_flag = 1;
                }
            }
            DBG("MAIN sw_id=%d,update_flag=%d,update_bin_major=%d,update_bin_minor=%d\n",main_sw_id,update_flag,update_bin_major,update_bin_minor);
        }
        else
        {
            if ((info_sw_id >= SWID_START) && (info_sw_id < SWID_NULL))
            {
                update_bin_major= (MSG_FIRMWARE[info_sw_id-SWID_START][0x7f4f] << 8) + MSG_FIRMWARE[info_sw_id-SWID_START][0x7f4e];
                update_bin_minor= (MSG_FIRMWARE[info_sw_id-SWID_START][0x7f51] << 8) + MSG_FIRMWARE[info_sw_id-SWID_START][0x7f50];
                update_flag = 1;
            }
            DBG("INFO1 sw_id=%d,update_flag=%d,update_bin_major=%d,update_bin_minor=%d\n",info_sw_id,update_flag,update_bin_major,update_bin_minor);
        }
    }
    else
    {
        if ((info_sw_id >= SWID_START) && (info_sw_id < SWID_NULL))
        {
            update_bin_major= (MSG_FIRMWARE[info_sw_id-SWID_START][0x7f4f] << 8) + MSG_FIRMWARE[info_sw_id-SWID_START][0x7f4e];
            update_bin_minor= (MSG_FIRMWARE[info_sw_id-SWID_START][0x7f51] << 8) + MSG_FIRMWARE[info_sw_id-SWID_START][0x7f50];
            update_flag = 1;
        }
        DBG("INFO2 sw_id=%d,update_flag=%d,update_bin_major=%d,update_bin_minor=%d\n",info_sw_id,update_flag,update_bin_major,update_bin_minor);
    }

    if (update_flag == 1)
    {
        DBG("MSG21XX_fw_auto_update begin....\n");
        //transfer data
        for (i = 0; i < 33; i++)
        {
            firmware_data_store(NULL, NULL, &(MSG_FIRMWARE[info_sw_id-SWID_START][i*1024]), 1024);
        }

        kthread_run(fwAutoUpdate, 0, "MSG21XX_fw_auto_update");
        DBG("*** mstar touch screen registered ***\n");
        return 0;
    }

    reset_hw();
#endif

    DBG("*** mstar touch screen registered ***\n");
    enable_irq(irq_msg21xx);
    return 0;

err3:
    free_irq(irq_msg21xx, input_dev);

err2:
    gpio_free(MS_TS_MSG21XX_GPIO_INT);

err1:
    mutex_destroy(&msg21xx_mutex);
    input_unregister_device(input_dev);
    input_free_device(input_dev);
    input_dev = NULL;

err0:
    gpio_free(MS_TS_MSG21XX_GPIO_RST);

    return ret;
}

/* remove function is triggered when the input device is removed from input sub-system */
static int touch_driver_remove(struct i2c_client *client)
{
    DBG("touch_driver_remove()\n");

    free_irq(irq_msg21xx, input_dev);
    gpio_free(MS_TS_MSG21XX_GPIO_INT);
    gpio_free(MS_TS_MSG21XX_GPIO_RST);
    input_unregister_device(input_dev);
    mutex_destroy(&msg21xx_mutex);

    return 0;
}

/* The I2C device list is used for matching I2C device and I2C device driver. */
static const struct i2c_device_id touch_device_id[] =
{
    {"msg21xx", 0},
    {}, /* should not omitted */
};

MODULE_DEVICE_TABLE(i2c, touch_device_id);

static struct i2c_driver touch_device_driver =
{
    .driver = {
        .name = "msg21xx",
        .owner = THIS_MODULE,
    },
    .probe = touch_driver_probe,
    .remove = touch_driver_remove,
    .id_table = touch_device_id,
};

static int __init touch_driver_init(void)
{
    int ret;

    /* register driver */
    ret = i2c_add_driver(&touch_device_driver);
    if (ret < 0)
    {
        DBG("add touch_device_driver i2c driver failed.\n");
        return -ENODEV;
    }
    DBG("add touch_device_driver i2c driver.\n");

    return ret;
}

static void __exit touch_driver_exit(void)
{
    DBG("remove touch_device_driver i2c driver.\n");

    i2c_del_driver(&touch_device_driver);
}

#ifdef TP_PRINT
#include <linux/proc_fs.h>

static U16 InfoAddr = 0x0F, PoolAddr = 0x10, TransLen = 256;
static U8 row, units, cnt;

static int tp_print_proc_read(void)
{
    U16 i, j;
    U16 left, offset = 0;
    U8 dbbus_tx_data[3] = {0};
    U8 u8Data;
    S16 s16Data;
    S32 s32Data;
    char *buf = NULL;

    left = cnt*row*units;
    if ((bTpInSuspend == 0) && (InfoAddr != 0x0F) && (PoolAddr != 0x10) && (left > 0))
    {
        buf = kmalloc(left, GFP_KERNEL);
        if (buf != NULL)
        {
            printk("tpp: \n");

            while (left > 0)
            {
                dbbus_tx_data[0] = 0x53;
                dbbus_tx_data[1] = ((PoolAddr + offset) >> 8) & 0xFF;
                dbbus_tx_data[2] = (PoolAddr + offset) & 0xFF;
                mutex_lock(&msg21xx_mutex);
                write_i2c_seq(SLAVE_I2C_ID_DWI2C, &dbbus_tx_data[0], 3);
                read_i2c_seq(SLAVE_I2C_ID_DWI2C, &buf[offset], left > TransLen ? TransLen : left);
                mutex_unlock(&msg21xx_mutex);

                if (left > TransLen)
                {
                    left -= TransLen;
                    offset += TransLen;
                }
                else
                {
                    left = 0;
                }
            }

            for (i = 0; i < cnt; i++)
            {
                printk("tpp: ");
                for (j = 0; j < row; j++)
                {
                    if (units == 1)
                    {
                        u8Data = buf[i*row*units + j*units];
                        printk("%d\t", u8Data);
                    }
                    else if (units == 2)
                    {
                        s16Data = buf[i*row*units + j*units] + (buf[i*row*units + j*units + 1] << 8);
                        printk("%d\t", s16Data);
                    }
                    else if (units == 4)
                    {
                        s32Data = buf[i*row*units + j*units] + (buf[i*row*units + j*units + 1] << 8) + (buf[i*row*units + j*units + 2] << 16) + (buf[i*row*units + j*units + 3] << 24);
                        printk("%d\t", s32Data);
                    }
                }
                printk("\n");
            }

            kfree(buf);
        }
    }

    return 0;
}

static void tp_print_create_entry(void)
{
    U8 dbbus_tx_data[3] = {0};
    U8 dbbus_rx_data[8] = {0};

    dbbus_tx_data[0] = 0x53;
    dbbus_tx_data[1] = 0x00;
    dbbus_tx_data[2] = 0x58;
    mutex_lock(&msg21xx_mutex);
    write_i2c_seq(SLAVE_I2C_ID_DWI2C, &dbbus_tx_data[0], 3);
    read_i2c_seq(SLAVE_I2C_ID_DWI2C, &dbbus_rx_data[0], 4);
    mutex_unlock(&msg21xx_mutex);
    InfoAddr = (dbbus_rx_data[1]<<8) + dbbus_rx_data[0];
    PoolAddr = (dbbus_rx_data[3]<<8) + dbbus_rx_data[2];
    printk("InfoAddr=0x%X\n", InfoAddr);
    printk("PoolAddr=0x%X\n", PoolAddr);

    if ((InfoAddr != 0x0F) && (PoolAddr != 0x10))
    {
        msleep(10);
        dbbus_tx_data[0] = 0x53;
        dbbus_tx_data[1] = (InfoAddr >> 8) & 0xFF;
        dbbus_tx_data[2] = InfoAddr & 0xFF;
        mutex_lock(&msg21xx_mutex);
        write_i2c_seq(SLAVE_I2C_ID_DWI2C, &dbbus_tx_data[0], 3);
        read_i2c_seq(SLAVE_I2C_ID_DWI2C, &dbbus_rx_data[0], 8);
        mutex_unlock(&msg21xx_mutex);

        units = dbbus_rx_data[0];
        row = dbbus_rx_data[1];
        cnt = dbbus_rx_data[2];
        TransLen = (dbbus_rx_data[7]<<8) + dbbus_rx_data[6];
        printk("tpp: row=%d, units=%d\n", row, units);
        printk("tpp: cnt=%d, TransLen=%d\n", cnt, TransLen);

        // tpp
        if (device_create_file(firmware_cmd_dev, &dev_attr_tpp) < 0)
        {
            pr_err("Failed to create device file(%s)!\n", dev_attr_tpp.attr.name);
        }
    }
}
#endif

module_init(touch_driver_init);
module_exit(touch_driver_exit);
MODULE_AUTHOR("MStar Semiconductor, Inc.");
MODULE_LICENSE("GPL v2");

