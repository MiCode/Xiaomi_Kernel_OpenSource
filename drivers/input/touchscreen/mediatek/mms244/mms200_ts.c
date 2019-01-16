/*
 * Touchscreen driver for Melfas MMS-200 series
 *
 * Copyright (C) 2013 Melfas Inc.
 * Author: DVK team <dvk@melfas.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
 

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
#include <linux/byteorder/generic.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif 
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/rtpm_prio.h>
#include <linux/proc_fs.h>
#include <linux/jiffies.h>
#include <linux/firmware.h>
#include <linux/earlysuspend.h>
#include <linux/irq.h>
#include <linux/input/mt.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/completion.h>
#include <linux/init.h>

#include <asm/uaccess.h>
#include <cust_eint.h>
#include <asm/unaligned.h>
#include <mach/eint.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_pm_ldo.h>
#include <linux/types.h>
#include <linux/dma-mapping.h>

#ifdef MT6589
#include <mach/mt_boot.h>
#endif

#ifdef MT6592
#include <mach/mt_boot.h>
#endif

#include "tpd.h"
#include "mms200_ts.h"

#define TPD_POWER_SOURCE_CUSTOM		MT6323_POWER_LDO_VGP1

#define TP_DEV_NAME		"mms200"
#define I2C_RETRY_CNT		5 //Fixed value
#define DOWNLOAD_RETRY_CNT	5 //Fixed value
#define MELFAS_DOWNLOAD		1 //Fixed value

#define PRESS_KEY		1 //Fixed value
#define RELEASE_KEY		0 //Fixed value

/*
MMS244 RMI register map
*/
#define TS_READ_LEN_ADDR 		0x0F
#define TS_READ_START_ADDR 		0x10
#define TS_READ_REGS_LEN 		66
#define TS_WRITE_REGS_LEN 		16
#define TS_TSP_REV_ADDR 		0xc0
#define TS_HARDWARE_REV_ADDR 		0xc1
#define TS_COMPATIBILITY_GRU_ADDR 	0xc2
#define TS_FIRMWARE_REV_ADDR 		0xc3
#define TS_CHIP_INFO_ADDR 		0xc4
#define TS_MANUFACTURER_INFO_ADDR 	0xc5

/*
MMS244 FACTORY 
*/
#define TS_TYPE_TRULY			0x02
#define TS_TYPE_BIEL			0x10
#define TS_TYPE_UNKNOWN			0xff

/*
MMS244 VERSION 
*/
#define MELFAS_HW_REVISON 		0x01
#define MELFAS_TURLY_H1_FW_VERSION 	0x04
#define MELFAS_TURLY_H2_FW_VERSION 	0xf8
#define MELFAS_TURLY_H3_FW_VERSION 	0x12
#define MELFAS_BIEL_H1_FW_VERSION 	0x06
#define MELFAS_BIEL_H2_FW_VERSION 	0x09

#define TS_READ_HW_VER_ADDR		0xF1 //Model Dependent
#define TS_READ_SW_VER_ADDR		0xF5 //Model Dependent

#define MELFAS_MAX_TRANSACTION_LENGTH	66
#define MELFAS_MAX_I2C_TRANSFER_SIZE	7
#define MELFAS_I2C_DEVICE_ADDRESS_LEN	1
#define MELFAS_I2C_MASTER_CLOCK		100
#define MELFAS_I2C_ADDRESS		0x20

/*
MMS244 PROC LIST
*/
#define MELFAS_ESD_ERROR 		0x0F
#define TS_MAX_TOUCH 			10
#define MMS244_CHIP			0x14

static int melfas_tpd_flag = 0;
static u8 * DMAbuffer_va = NULL;
static dma_addr_t DMAbuffer_pa = NULL;
static int interrupt_count = 0;

static u8 tp_factory_id_cmd = TS_TYPE_UNKNOWN;
static u8 hardware_ver = 0;

typedef struct muti_touch_info
{
	uint16_t pos_x;
	uint16_t pos_y;
	uint16_t area;
	uint16_t pressure;
	unsigned int pressed;
	bool updated;
};
static struct muti_touch_info g_Mtouch_info[TS_MAX_TOUCH];

//updated
enum tp_finger_status {
	FINGER_UNKNOWN,
	FINGER_DOWN,
	FINGER_UP
};

static int finger_status[TS_MAX_TOUCH] = {FINGER_UNKNOWN};

#define TPD_HAVE_BUTTON
#ifdef TPD_HAVE_BUTTON 
#define TPD_KEY_COUNT		3
#define TPD_KEYS                {KEY_BACK, KEY_HOMEPAGE ,KEY_MENU}
#define TPD_KEYS_DIM            {{200,1980,100,80},\
				 {500,1980,100,80},\
				 {800,1980,100,80}}
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif


static DECLARE_WAIT_QUEUE_HEAD(melfas_waiter);
//static DEFINE_MUTEX(melfas_tp_mutex);

struct i2c_client *melfas_i2c_client = NULL;

static const struct i2c_device_id melfas_tpd_id[] = {{TP_DEV_NAME,0},{}};
static struct i2c_board_info __initdata melfas_i2c_tpd={ I2C_BOARD_INFO(TP_DEV_NAME, MELFAS_I2C_ADDRESS)};

static int melfas_tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int melfas_tpd_i2c_remove(struct i2c_client *client);
static int melfas_tpd_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
extern int isc_fw_download(struct i2c_client *client, const U8 *data, size_t len);
static int melfas_i2c_read(struct i2c_client *client, U16 addr, U16 len, U8 *rxbuf);
int melfas_i2c_DMAread(struct i2c_client *client, U16 addr, U16 len, U8 *rxbuf);
int melfas_i2c_DMAwrite(struct i2c_client *client, U16 addr, U16 len, U8 *txbuf);

static void melfas_ts_release_all_finger(void);


extern struct tpd_device *tpd;

static struct i2c_driver melfas_tpd_i2c_driver =
{                       
    .probe = melfas_tpd_i2c_probe,                                   
    .remove = melfas_tpd_i2c_remove,                           
    .detect = melfas_tpd_i2c_detect,                           
    .driver.name = "mtk-tpd", 
    .id_table = melfas_tpd_id,                             
#ifndef CONFIG_HAS_EARLYSUSPEND
    .suspend = melfas_ts_suspend, 
    .resume = melfas_ts_resume,
#endif
    
}; 

/*
PR 460136,turly and biel touchpanel compatibility,Firmware update series
Firmware definition
B:biel T:turly
R:hardware version
V:firmware version
*/
#if 1
const u8 MELFAS_biel_h1_binary[] = {
#include "MCH_TDIABLOX_B_R01_V06.mfsb.h"
};
const u8 MELFAS_biel_h2_binary[] = {
//#include "MCH_TDIABLOX_B_R02_V07.mfsb.h"
#include "MCH_TDIABLOX_B_R02_V09.mfsb.h"
};
const u8 MELFAS_turly_h1_binary[] = {
#include "MCH_TDIABLOX_R01_V04.mfsb.h"
};
const u8 MELFAS_turly_h2_binary[] = {
#include "MCH_TDIABLOX_T_R02_VF8.mfsb.h"
};
const u8 MELFAS_turly_h3_binary[] = {
#include "MCH_TDIABLOX_T_R03_V12.mfsb.h"
};

#endif


#define TP_SYSFS_SUPPORT
#ifdef TP_SYSFS_SUPPORT
static ssize_t TP_value_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	char *s = buf;
	int ret;
	u8 val[7];

	ret = melfas_i2c_read(melfas_i2c_client, 0xc1, 1, &val[0]);
	ret = melfas_i2c_read(melfas_i2c_client, 0xc2, 1, &val[1]);
	ret = melfas_i2c_read(melfas_i2c_client, 0xc3, 1, &val[2]);
	ret = melfas_i2c_read(melfas_i2c_client, 0xc4, 1, &val[3]);
	ret = melfas_i2c_read(melfas_i2c_client, 0xc5, 1, &val[4]);
	ret = melfas_i2c_read(melfas_i2c_client, 0xc0, 1, &val[5]);
	
	s += sprintf(s, "TP, mms244_ts, hardware version: %x \n", val[0]);
	s += sprintf(s, "TP, mms244_ts, 0xC2: %x \n", val[1]);
	s += sprintf(s, "TP, mms244_ts, firmware version:  %x \n", val[2]);
	s += sprintf(s, "TP, mms244_ts, 0xC4:  %x \n", val[3]);
	s += sprintf(s, "TP, mms244_ts, manufacture info:  %x \n", val[4]);
	s += sprintf(s, "TP, mms244_ts, 0xC0:  %x \n", val[5]);
	return (s - buf);
}

kal_uint8 TPD_DBG = 0;
static ssize_t TP_value_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{
	char *s = buf;
	int save;
	ssize_t ret_val;
	char cmd_str[16] = {0};
	unsigned int val; 
	sscanf(buf, "%s %d", (char *)&cmd_str, &val);

	if(strcmp(cmd_str, "DEBUG") == 0) {
        	if (val == 1)
            		TPD_DBG = 1;
        	else
            		TPD_DBG = 0;
	}
   
	ret_val = (s - buf);
	return ret_val;
}

static ssize_t debug_touch_show(struct device *dev,
	struct device_attribute *attr, const char *buf)
{	
	char *s = buf;
		
	s += sprintf(s, "TP finger status:%d %d %d %d %d \n", 
		finger_status[0],finger_status[1],finger_status[2],
		finger_status[3],finger_status[4]);
	s += sprintf(s, "TP finger status:%d %d %d %d %d \n", 
		finger_status[5],finger_status[6],finger_status[7],
		finger_status[8],finger_status[9]);
	return (s - buf);
}

static ssize_t debug_touch_store(struct kobject *kobj, 
		struct kobj_attribute *attr, const char *buf, size_t n)
{
	int save;

	if (sscanf(buf, "%d", &save)==0) {
		printk(KERN_ERR "%s -- invalid save string '%s'...\n", 
			__func__, buf);
		return -EINVAL;
	}

	return n;
}

static DEVICE_ATTR(debug_touch, 0664, debug_touch_show, debug_touch_store); 
/*
PR 457838 series,cts permission issue
*/
static DEVICE_ATTR(TP_DEBUG, 0644,  TP_value_show, TP_value_store);   

static struct attribute *TP_sysfs_attrs[] = {
	&dev_attr_TP_DEBUG.attr,
	&dev_attr_debug_touch.attr,
	NULL,
};
static struct attribute_group TP_attr_group = {
        .attrs = TP_sysfs_attrs,
};

//add sysfs
struct kobject *TP_ctrl_kobj;
static int TP_sysfs_init(void)
{ 
	TP_ctrl_kobj = kobject_create_and_add("TP", NULL);
	if (!TP_ctrl_kobj)
		return -ENOMEM;

	return sysfs_create_group(TP_ctrl_kobj, &TP_attr_group);
}
//remove sysfs
static void TP_sysfs_exit(void)
{
	sysfs_remove_group(TP_ctrl_kobj, &TP_attr_group);
	kobject_put(TP_ctrl_kobj);
}


#endif

void touchkey_handler(u8 key, bool on)
{
	int i;
	switch(key)
	{
        case 1:
            TPD_DMESG("MMS_TOUCH_KEY_EVENT BACK, %d \n",on);
            input_report_key(tpd->dev, KEY_BACK, on);
        break;
        
        case 2:
            TPD_DMESG("MMS_TOUCH_KEY_EVENT HOME, %d \n",on);
            input_report_key(tpd->dev, KEY_HOMEPAGE, on);
        break;

        case 3:
            TPD_DMESG("MMS_TOUCH_KEY_EVENT MENU, %d \n",on);
            input_report_key(tpd->dev, KEY_MENU, on);
        break;
        
        default:
        break;
	}
}


/*
esd performance revise
if some esd error happened,then we reset the tp
chip factory think 50 ms dalay is acceptable
*/
static void esd_rest_tp(void)
{
	//TPD_DEBUG("==========tp have inter esd =============\n");
	TPD_DMESG("[melfas_tpd] %s\n", __func__);
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
   	msleep(50);
    	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
}

static int melfas_touch_event_handler(void *unused)
{
#if 1
	u8 buf[TS_READ_REGS_LEN] = { 0 };
	int i, read_num, fingerID, Touch_Type = 0, touchState = 0;//, keyID = 0;
	int id = 0;	
	
	struct sched_param param = { .sched_priority = RTPM_PRIO_TPD }; 

	sched_setscheduler(current, SCHED_RR, &param);
	
	do {    
		//mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);	
        	set_current_state(TASK_INTERRUPTIBLE);		
        	wait_event_interruptible(melfas_waiter, melfas_tpd_flag != 0);
		
        	melfas_tpd_flag = 0;
        	set_current_state(TASK_RUNNING); 
        
		melfas_i2c_read(melfas_i2c_client, TS_READ_LEN_ADDR, 1, buf);
		read_num = buf[0];
		//TPD_DMESG("melfas,read_num = %d\n",read_num);
		//TPD_DMESG("melfas,interrupt_count = %d \n",interrupt_count);
		if(read_num) {
			/*
			int siganl rest issue,filter some error packets
			*/
			if (((read_num % 6)!= 0) || (read_num > 60)) {
           			//mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
           			TPD_DMESG("illegal read_num : %d\n", read_num);
           			melfas_ts_release_all_finger();
        	    		esd_rest_tp();
             			continue;
	 		}
	 		
			if (read_num <= 8)
    				melfas_i2c_read(melfas_i2c_client, TS_READ_START_ADDR, read_num, buf);
			else if (read_num > 8)
    				melfas_i2c_DMAread(melfas_i2c_client, TS_READ_START_ADDR, read_num, buf);
    			
    			/*esd error*/
          		if(buf[0] == MELFAS_ESD_ERROR) {
           			//mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
           			TPD_DMESG("ESD error!\n");
           			melfas_ts_release_all_finger();
        	    		esd_rest_tp();
             			continue;
	 		}

			for (i = 0; i < read_num; i = i + FINGER_EVENT_SZ) {
				Touch_Type = (buf[i] >> 5) & 0x03;
                		//TPD_DMESG("%s : touch type = %d, buf[i] = %x \n",__FUNCTION__,Touch_Type, buf[i]);
                		/* touch type is panel */
                		if (Touch_Type == MMS_TOUCH_KEY_EVENT) {
                    			touchkey_handler((buf[i] & 0x0f),(bool)(buf[i] & 0x80));
                		}
               			else {
                    			fingerID = (buf[i] & 0x0F) - 1;
                    			touchState = ((buf[i] & 0x80) >> 7);
                    			
                                        if ((fingerID < 0) || (fingerID >TS_MAX_TOUCH )) {
						TPD_DMESG("illegal finger id (id: %d)\n", fingerID);
						continue;
					}
					if (!touchState && !g_Mtouch_info[fingerID].pressed) {
						TPD_DMESG("Wrong touch release (id: %d)\n", fingerID);
						continue;
					}
		
                    			g_Mtouch_info[fingerID].pos_x = (uint16_t)(buf[i + 2] | 
                    							((buf[i + 1] & 0xf) << 8));
                    			g_Mtouch_info[fingerID].pos_y = (uint16_t)(buf[i + 3] | 
                    							(((buf[i + 1] >> 4 ) & 0xf) << 8));
                    			g_Mtouch_info[fingerID].area = buf[i + 4];
                    			if (touchState)
                        			g_Mtouch_info[fingerID].pressure = buf[i + 5];
                    			else
                        			g_Mtouch_info[fingerID].pressure = 0;
                    			//g_Mtouch_info[fingerID].pressed = pressed;
                    			g_Mtouch_info[fingerID].pressed = touchState;	
		    			g_Mtouch_info[fingerID].updated = true;
	
                    			/*TPD_DMESG("[MMS200]: Touch ID: %d, State : %d, x: %d, y: %d, z: %d w: %d\n",
                                	fingerID, 
                                	touchState, 
                                	g_Mtouch_info[i].pos_x, 
                                	g_Mtouch_info[i].pos_y, 
                                	g_Mtouch_info[i].pressure, 
                                	g_Mtouch_info[i].area);*/
                			}
                
            		}
            
            		for (id = 0; id < TS_MAX_TOUCH; id++) {
            			if (!g_Mtouch_info[id].updated)
					continue;
				g_Mtouch_info[id].updated = false;
		        
        			if (g_Mtouch_info[id].pressed) {			
					input_mt_slot(tpd->dev, id);
					input_report_key(tpd->dev, BTN_TOUCH, 1);
					input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, id+2);

					input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR,
						g_Mtouch_info[id].area);
					input_report_abs(tpd->dev, ABS_MT_POSITION_X,
						g_Mtouch_info[id].pos_x);
					input_report_abs(tpd->dev, ABS_MT_POSITION_Y,
						g_Mtouch_info[id].pos_y);						
					finger_status[id] = FINGER_DOWN;	
				}
				else if (!g_Mtouch_info[id].pressed) {
					input_mt_slot(tpd->dev, id);
					input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, -1);
					finger_status[id] = FINGER_UP;					
				}
		
        		} 
            
			if ( (tpd != NULL) && (tpd->dev != NULL) )
            			input_sync(tpd->dev);
		}

		/* 
		the delay advised by yida.yu 		
		mdelay(8);
		*/
		//mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);	
	} while ( !kthread_should_stop() ); 

	return 0;
#endif
}

static void melfas_i2c_tpd_eint_interrupt_handler(void)
{ 
    //TPD_DMESG_PRINT_INT;
    melfas_tpd_flag=1;    
    interrupt_count++;
    wake_up_interruptible(&melfas_waiter);
} 

int melfas_i2c_write_bytes( struct i2c_client *client, U16 addr, int len, U32 *txbuf )
{
    u8 buffer[MELFAS_MAX_TRANSACTION_LENGTH]={0};
    u16 left = len;
    u8 offset = 0;
    u8 retry = 0;

    struct i2c_msg msg = 
    {
        .addr = ((client->addr&I2C_MASK_FLAG )|(I2C_ENEXT_FLAG )),
        .flags = 0,
        .buf = buffer,
        .timing = MELFAS_I2C_MASTER_CLOCK,
    };


    if ( txbuf == NULL )
        return -1;

    //TPD_DMESG("i2c_write_bytes to device %02X address %04X len %d\n", client->addr, addr, len );

    while ( left > 0 )
    {
        retry = 0;

        buffer[0] = (u8)addr+offset;

        if ( left > MELFAS_MAX_I2C_TRANSFER_SIZE )
        {
            memcpy( &buffer[MELFAS_I2C_DEVICE_ADDRESS_LEN], &txbuf[offset], MELFAS_MAX_I2C_TRANSFER_SIZE );
            msg.len = MELFAS_MAX_TRANSACTION_LENGTH;
            left -= MELFAS_MAX_I2C_TRANSFER_SIZE;
            offset += MELFAS_MAX_I2C_TRANSFER_SIZE;
        }
        else
        {
            memcpy( &buffer[MELFAS_I2C_DEVICE_ADDRESS_LEN], &txbuf[offset], left );
            msg.len = left + MELFAS_I2C_DEVICE_ADDRESS_LEN;
            left = 0;
        }

        //TPD_DMESG("byte left %d offset %d\n", left, offset );

        while ( i2c_transfer( client->adapter, &msg, 1 ) != 1 )
        {
            retry++;

            if ( retry == I2C_RETRY_CNT )
            {
                TPD_DMESG("I2C write 0x%X%X length=%d failed\n", buffer[0], buffer[1], len);                
                return -1;
            }
            else {
                 //TPD_DMESG("I2C write retry %d addr 0x%X%X\n", retry, buffer[0], buffer[1]);
		}
        }
    }

    return 0;
}


static int melfas_i2c_read(struct i2c_client *client, U16 addr, U16 len, U8 *rxbuf)
{
    u8 buffer[MELFAS_I2C_DEVICE_ADDRESS_LEN]={0};
    u8 retry;
    u16 left = len;
    u8 offset = 0;

    struct i2c_msg msg[2] =
    {
        {
            .addr = ((client->addr&I2C_MASK_FLAG )|(I2C_ENEXT_FLAG )),
            .flags = 0,
            .buf = buffer,
            .len = MELFAS_I2C_DEVICE_ADDRESS_LEN,
            .timing = MELFAS_I2C_MASTER_CLOCK
        },
        {
            .addr = ((client->addr&I2C_MASK_FLAG )|(I2C_ENEXT_FLAG )),
            .flags = I2C_M_RD,
            .timing = MELFAS_I2C_MASTER_CLOCK
        },
    };

    if ( rxbuf == NULL )
        return -1;

    //TPD_DMESG("i2c_read_bytes to device %02X address %04X len %d\n", client->addr, addr, len );

    while ( left > 0 )
    {
        buffer[0] = (u8)addr+offset;

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

        while ( i2c_transfer( client->adapter, &msg[0], 2 ) != 2 )
        {
            retry++;

            if ( retry == I2C_RETRY_CNT )
            {
                TPD_DMESG("I2C read 0x%X length=%d failed\n", addr + offset, len);
                TPD_DMESG("I2C read 0x%X length=%d failed\n", addr + offset, len);
                return -1;
            }
        }
    }

    return 0;
}

int melfas_i2c_DMAread(struct i2c_client *client, U16 addr, U16 len, U8 *rxbuf)
{
	int retry,i;

	struct i2c_msg msg[] = {
		{
			.addr = client->addr & I2C_MASK_FLAG | I2C_ENEXT_FLAG,
			.flags = 0,
			.len = 1,
			.buf = &addr,
		},
		{
			.addr = client->addr & I2C_MASK_FLAG | I2C_ENEXT_FLAG | I2C_DMA_FLAG,
			.flags = I2C_M_RD,
			.len = len,
			.buf = DMAbuffer_pa,
		}
	};
	for (retry = 0; retry < I2C_RETRY_CNT; retry++) {
		if (i2c_transfer(client->adapter, msg, 2) == 2)
			break;
		mdelay(5);
		TPD_DMESG("Line %d, i2c_transfer error, retry = %d\n", __LINE__, retry);
	}
	if (retry == I2C_RETRY_CNT) {
		printk(KERN_ERR "i2c_read_block retry over %d\n",I2C_RETRY_CNT);
		return -EIO;
	}
	
	for(i=0;i<len;i++)
        rxbuf[i]=DMAbuffer_va[i];
	
	return 0;
}
int melfas_i2c_DMA_RW_isc(struct i2c_client *client, 
                            int rw,
                            U8 *rxbuf, U16 rlen,
                            U8 *txbuf, U16 tlen)
{
	int retry,i;
	
	struct i2c_msg msg[] = {
		{
			.addr = client->addr & I2C_MASK_FLAG | I2C_ENEXT_FLAG | I2C_DMA_FLAG,
			.flags = 0,
			.buf = DMAbuffer_pa,
		}
	};
	/*
	rw = 1 //write
	rw = 2 //read
	*/
	if (rw == 1)//write
	{
        for(i=0;i<tlen;i++)
            DMAbuffer_va[i] = txbuf[i];
        msg[0].flags = 0;
        msg[0].len = tlen;
        for (retry = 0; retry < I2C_RETRY_CNT; retry++) 
        {
            if (i2c_transfer(client->adapter, msg, 1) == 1)
                break;
            mdelay(5);
            TPD_DMESG("Line %d, i2c_transfer error, retry = %d\n", __LINE__, retry);
        }
        if (retry == I2C_RETRY_CNT) {
            printk(KERN_ERR "i2c_read_block retry over %d\n",I2C_RETRY_CNT);
            return -EIO;
        }
	}
	else if (rw == 2)//read
	{
	    //write from TX
        for(i=0;i<tlen;i++)
            DMAbuffer_va[i] = txbuf[i];
        msg[0].flags = 0;
        msg[0].len = tlen;
        for (retry = 0; retry < I2C_RETRY_CNT; retry++) 
        {
            if (i2c_transfer(client->adapter, msg, 1) == 1)
                break;
            mdelay(5);
            TPD_DMESG("Line %d, i2c_transfer error, retry = %d\n", __LINE__, retry);
        }
        if (retry == I2C_RETRY_CNT) {
            printk(KERN_ERR "i2c_read_block retry over %d\n",I2C_RETRY_CNT);
            return -EIO;
        }
        //read into RX
        msg[0].flags = I2C_M_RD;
        msg[0].len = rlen;
        for (retry = 0; retry < I2C_RETRY_CNT; retry++) 
        {
            if (i2c_transfer(client->adapter, msg, 1) == 1)
                break;
            mdelay(5);
            TPD_DMESG("Line %d, i2c_transfer error, retry = %d\n", __LINE__, retry);
        }
        if (retry == I2C_RETRY_CNT) {
            printk(KERN_ERR "i2c_read_block retry over %d\n",I2C_RETRY_CNT);
            return -EIO;
        }
        
        for(i=0;i<rlen;i++)
             rxbuf[i] = DMAbuffer_va[i];
    }
	
	return 0;
}

/*
compatibility with all hardware ver for T&B
Turly firmware history
H1:04
H2:06,e6-f8
H3:08-09
Biel firmware history
H1:04-06
H2:07
*/
int melfas_check_firmware(struct i2c_client *client)
{
    int ret = 0;
    //uint8_t i = 0;
    u8 val[7];
    
    TPD_DMESG("[melfas_tpd]: i2c addr is 0x%02x",client->addr);
    ret = melfas_i2c_read(client, 0xc3, 1, &val[0]);
    ret = melfas_i2c_read(client, 0xc4, 1, &val[1]);    
    ret = melfas_i2c_read(client, 0xc0, 1, &val[3]);
    ret = melfas_i2c_read(client, 0xc2, 1, &val[5]);
    ret = melfas_i2c_read(client, 0xc1, 1, &val[4]);
    if (ret >= 0) {
    	hardware_ver = val[4];
	} else {
	goto out;
	}

    ret = melfas_i2c_read(client, 0xc5, 1, &val[2]);        
    if (ret >= 0)
    {
    	tp_factory_id_cmd = val[2];    	
        TPD_DMESG("[melfas_tpd]: 0Xc3[0x%02x],0Xc4[0x%02x],0Xc5[0x%02x]",
                    val[0], val[1],val[2]);
        if (tp_factory_id_cmd == TS_TYPE_TRULY) {
        	if (hardware_ver == 1) {
        		if (val[0] < MELFAS_TURLY_H1_FW_VERSION)
            			ret = 1;
        		else
            			ret = 0;
            	} else if (hardware_ver == 2) {
            		if (val[0] < MELFAS_TURLY_H2_FW_VERSION)
            			ret = 1;
        		else
            			ret = 0;
            	} else if (hardware_ver == 3) {
            		if (val[0] < MELFAS_TURLY_H3_FW_VERSION)
            			ret = 1;
        		else
            			ret = 0;
            	}
        } else if (tp_factory_id_cmd == TS_TYPE_BIEL) {
        	if (hardware_ver == 1) {
        		if (val[0] < MELFAS_BIEL_H1_FW_VERSION)
            			ret = 1;
        		else
            			ret = 0;
            	} else if (hardware_ver == 2) {
            		if (val[0] < MELFAS_BIEL_H2_FW_VERSION)
            			ret = 1;
        		else
            			ret = 0;
            	}
        }
    
        TPD_DMESG("[melfas_tpd]: MMS_FW_VERSION is 0x%02x, IC version is 0x%02x\n",
                    MELFAS_TURLY_H2_FW_VERSION,val[0]);
        goto out;
    }
    else if (ret < 0)
    {
        TPD_DMESG("[melfas_tpd] %s,%d: i2c read fail[%d] \n", __FUNCTION__, __LINE__, ret);
        goto out;
    }

out:	
	return ret;
}

extern void mms_fw_update_controller(const struct firmware *fw, 
			struct i2c_client *client);
struct firmware fw_info_turly_h3 = 
{
	.size = sizeof(MELFAS_turly_h3_binary),
	.data = &MELFAS_turly_h3_binary[0],
};
			
struct firmware fw_info_turly_h2 = 
{
	.size = sizeof(MELFAS_turly_h2_binary),
	.data = &MELFAS_turly_h2_binary[0],
};

struct firmware fw_info_turly_h1 = 
{
	.size = sizeof(MELFAS_turly_h1_binary),
	.data = &MELFAS_turly_h1_binary[0],
};

struct firmware fw_info_biel_h1 = 
{
	.size = sizeof(MELFAS_biel_h1_binary),
	.data = &MELFAS_biel_h1_binary[0],
};

struct firmware fw_info_biel_h2 = 
{
	.size = sizeof(MELFAS_biel_h2_binary),
	.data = &MELFAS_biel_h2_binary[0],
};

static int melfas_firmware_update(struct i2c_client *client)
{
	int ret = 0;
	ret = melfas_check_firmware(client);
	if (ret != 0) {
#if MELFAS_DOWNLOAD
        int ver;
        //MELFAS_binary = kstrdup(fw_name,GFP_KERNEL);
    	//ret = request_firmware_nowait(THIS_MODULE, true, fw_name, &client->dev,
    	//	GFP_KERNEL, client, mms_fw_update_controller);
		if (tp_factory_id_cmd == TS_TYPE_TRULY) {
    			if (hardware_ver == 1) {
        			TPD_DMESG("[melfas_tpd] MELFAS_bin_len = %x\n",
        				fw_info_turly_h1.size);
        			TPD_DMESG("[melfas_tpd] MELFAS_bin = %x, addr = %p\n", 
        				fw_info_turly_h1.data[0],
        				fw_info_turly_h1.data);
        		mms_fw_update_controller(&fw_info_turly_h1,client);
			} else if (hardware_ver == 2) {
        			TPD_DMESG("[melfas_tpd] MELFAS_bin_len = %x\n", 
        				fw_info_turly_h2.size);
        			TPD_DMESG("[melfas_tpd] MELFAS_bin = %x, addr = %p\n", 
        				fw_info_turly_h2.data[0],
        				fw_info_turly_h2.data);
        			mms_fw_update_controller(&fw_info_turly_h2,client);
			} else if (hardware_ver == 3) {
        			TPD_DMESG("[melfas_tpd] MELFAS_bin_len = %x\n", 
        				fw_info_turly_h3.size);
        			TPD_DMESG("[melfas_tpd] MELFAS_bin = %x, addr = %p\n", 
        				fw_info_turly_h3.data[0],
        				fw_info_turly_h3.data);
        			mms_fw_update_controller(&fw_info_turly_h3,client);
			}
		} else if (tp_factory_id_cmd == TS_TYPE_BIEL) {
			if (hardware_ver == 1) {
				TPD_DMESG("[melfas_tpd] MELFAS_bin_len = %x\n", 
					fw_info_biel_h1.size);
        			TPD_DMESG("[melfas_tpd] MELFAS_bin = %x, addr = %p\n", 
        				fw_info_biel_h1.data[0],fw_info_biel_h1.data);
        			mms_fw_update_controller(&fw_info_biel_h1,client);
        		} else if (hardware_ver == 2) {
        			TPD_DMESG("[melfas_tpd] MELFAS_bin_len = %x\n", 
					fw_info_biel_h2.size);
        			TPD_DMESG("[melfas_tpd] MELFAS_bin = %x, addr = %p\n", 
        				fw_info_biel_h2.data[0],fw_info_biel_h2.data);
        			mms_fw_update_controller(&fw_info_biel_h2,client);
        		}
		}
#endif
    }
    return ret;
}

void mms_reboot(void)
{
    TPD_DMESG("mms_reboot\n");
    hwPowerDown(TPD_POWER_SOURCE_CUSTOM, "TP");
    //init GPIO pin
    //init CE //GPIO_CTP_RST_PIN, is CE
    mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
    //init EINT, mask CTP EINT //GPIO_CTP_EINT_PIN, is RSTB
    mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
    mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
    mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
    mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_DOWN);
    msleep(10);  //dummy delay here

    //turn on VDD33, LDP_VGP4
    hwPowerOn(TPD_POWER_SOURCE_CUSTOM, VOL_3300, "TP");
    msleep(20);  //tce, min is 0, max is ?

    //set CE to HIGH
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
    msleep(50);  //tpor, min is 1, max is 5

    //set RSTB to HIGH 
    mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);
    msleep(50);//t boot_core, typicl is 20, max is 25ms
    msleep(300);
}


struct completion mms200_init_done;

static int melfas_tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{             
     int err = 0;
     u8 chip_info = 0;
     int i_ret = 0;
     int try_count = 0;
     
	struct task_struct *thread = NULL;
	
	TPD_DMESG("[melfas_tpd] %s\n", __func__);
	
	//init_completion(&mms200_init_done);
	
#if 1//def TP_SYSFS_SUPPORT
    //TP sysfs debug support
	TP_sysfs_init();
#endif

    //TP DMA support
    if(DMAbuffer_va == NULL)
        DMAbuffer_va = (u8 *)dma_alloc_coherent(NULL, 4096,&DMAbuffer_pa, GFP_KERNEL);
    
    TPD_DMESG("dma_alloc_coherent va = 0x%p, pa = 0x%08x \n",DMAbuffer_va,DMAbuffer_pa);
    if(!DMAbuffer_va)
    {
        TPD_DMESG("Allocate DMA I2C Buffer failed!\n");
        return -1;
    }
    
reset_proc:
    mms_reboot();
    
    melfas_i2c_client = client;
    
	if (melfas_firmware_update(client) < 0)
	{
	//if firmware update failed, reset IC
        //goto reset_proc;
    }
    
    thread = kthread_run(melfas_touch_event_handler, 0, TPD_DEVICE);

    if (IS_ERR(thread))
    { 
        err = PTR_ERR(thread);
        TPD_DMESG(TPD_DEVICE "[melfas_tpd] failed to create kernel thread: %d\n", err);
    }
	mt_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, 0);
    //mt65xx_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE);
    mt_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
    //mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, CUST_EINT_POLARITY_LOW, melfas_i2c_tpd_eint_interrupt_handler, 1);
    mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINTF_TRIGGER_FALLING, melfas_i2c_tpd_eint_interrupt_handler, 1); 
    mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
    
    mdelay(5);
    
	/*try 3 times handshake*/
	do {
		try_count++;	
		i_ret = melfas_i2c_read(melfas_i2c_client, MMS_CHIP_INFO, 1, &chip_info);
		if (i_ret < 0)
			mms_reboot();
	} while( (i_ret < 0) && (try_count <= 3) );
	TPD_DMESG("[melfas_tpd] communication times is %d\n", try_count);
	if (i_ret < 0)
		return -1;
	if (chip_info == MMS244_CHIP)
		TPD_DMESG("[melfas_tpd] MMS-244 was probed successfully \n");
	/*
	sometimes tp do not work,so add rst operation
	*/	
#if 1
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
	msleep(20);
	//power on
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(200);
   	//irq unmask
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif
    tpd_load_status = 1;
    return 0;

}

static int melfas_tpd_i2c_remove(struct i2c_client *client)
{
	TPD_DMESG("[melfas_tpd] %s\n", __func__);
	if(DMAbuffer_va) {
		dma_free_coherent(NULL, 4096, DMAbuffer_va, DMAbuffer_pa);
		DMAbuffer_va = NULL;
		DMAbuffer_pa = 0;
	}
	input_mt_destroy_slots(tpd->dev);
	TP_sysfs_exit();
	return 0;
}

static int melfas_tpd_i2c_detect(struct i2c_client *client, 
			struct i2c_board_info *info)
{
	TPD_DMESG("[melfas_tpd] %s\n", __func__);
	strcpy(info->type, "mtk-tpd");
	return 0;
}
static int melfas_tpd_local_init(void) 
{

	TPD_DMESG("[melfas_tpd] end %s, %d\n", __FUNCTION__, __LINE__);  
	if(i2c_add_driver(&melfas_tpd_i2c_driver)!= 0) {
        	TPD_DMESG("[melfas_tpd] unable to add i2c driver.\n");
        	return -1;
	}
	if(tpd_load_status == 0) {
    		TPD_DMESG("[melfas_tpd] add error touch panel driver.\n");
    		i2c_del_driver(&melfas_tpd_i2c_driver);
    	return -1;
    }
  	#ifdef TPD_HAVE_BUTTON     
    tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);
	#endif   

    input_mt_init_slots(tpd->dev, TS_MAX_TOUCH, 1);
    tpd_type_cap = 1;

    return 0;
}


#ifdef SLOT_TYPE
static void melfas_ts_release_all_finger(void)
{
	int i;
	TPD_DMESG("[melfas_tpd] %s\n", __func__);
	for (i = 0; i < TS_MAX_TOUCH; i++)
	{
		input_mt_slot(tpd->dev, i);
		input_mt_report_slot_state(tpd->dev, MT_TOOL_FINGER, false);
	}
	input_sync(tpd->dev);
}
#else
/*
TP freeze workaround
*/
static void special_ts_release_all_finger(void)
{
	unsigned char i;
	TPD_DMESG("[melfas_tpd] %s\n", __func__);
	for(i=0; i< TS_MAX_TOUCH; i++){
		input_mt_slot(tpd->dev, i);
		input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, -1);		
	}
	input_sync(tpd->dev);
	
	for(i=0; i< TS_MAX_TOUCH; i++){
		input_mt_slot(tpd->dev, i);
		input_report_key(tpd->dev, BTN_TOUCH, 1);
		input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, i+2);

		input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR,
				20);
		input_report_abs(tpd->dev, ABS_MT_POSITION_X,
				1100);
		input_report_abs(tpd->dev, ABS_MT_POSITION_Y,	
				0);		
	}
	input_sync(tpd->dev);
		
	for(i=0; i< TS_MAX_TOUCH; i++){
		input_mt_slot(tpd->dev, i);
		input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, -1);		
	}
	input_sync(tpd->dev);	
}
static void melfas_ts_release_all_finger(void)
{
	unsigned char i;
	TPD_DMESG("[melfas_tpd] %s\n", __func__);
	for(i=0; i< TS_MAX_TOUCH; i++){
		input_mt_slot(tpd->dev, i);
		input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, -1);
	}
	input_sync(tpd->dev);
}
#endif

/*
add power on/off operation
*/
#ifdef CONFIG_HAS_EARLYSUSPEND
static void melfas_tpd_early_suspend(struct early_suspend *h)
{
	TPD_DMESG("[melfas_tpd] %s\n", __func__);
	//mutex_lock(&melfas_tp_mutex);
	//irq mask
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	//power down
	/*hwPowerDown(TPD_POWER_SOURCE_CUSTOM, "TP");
	msleep(10);*/
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
	msleep(20);
	special_ts_release_all_finger();
	//mutex_unlock(&melfas_tp_mutex);
}

static void melfas_tpd_late_resume(struct early_suspend *h)
{
	//int ret;
	//struct melfas_ts_data *ts = i2c_get_clientdata(client);
	//mutex_lock(&melfas_tp_mutex);
	TPD_DMESG("[melfas_tpd] %s\n", __func__);
	/*hwPowerOn(TPD_POWER_SOURCE_CUSTOM, VOL_3300, "TP");
	msleep(10);*/
	//power on
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(200);
   	//irq unmask
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	//mutex_unlock(&melfas_tp_mutex);
}
#endif

static struct tpd_driver_t melfas_tpd_device_driver =
{
    .tpd_device_name = "melfas_mms200",
    .tpd_local_init = melfas_tpd_local_init,
#ifdef CONFIG_HAS_EARLYSUSPEND    
    .suspend = melfas_tpd_early_suspend,
    .resume = melfas_tpd_late_resume,
#endif
#ifdef TPD_HAVE_BUTTON
    .tpd_have_button = 1,
#else
    .tpd_have_button = 0,
#endif		
};

/* called when loaded into kernel */
static int __init melfas_tpd_driver_init(void)
{
    TPD_DMESG("[melfas_tpd] %s\n", __func__);

	i2c_register_board_info(0, &melfas_i2c_tpd, 1);
    if ( tpd_driver_add(&melfas_tpd_device_driver) < 0)
        TPD_DMESG("[melfas_tpd] add generic driver failed\n");

    return 0;
}

/* should never be called */
static void __exit melfas_tpd_driver_exit(void)
{
    TPD_DMESG("[melfas_tpd] %s\n", __func__);
    tpd_driver_remove(&melfas_tpd_device_driver);
}


module_init(melfas_tpd_driver_init);
module_exit(melfas_tpd_driver_exit);

