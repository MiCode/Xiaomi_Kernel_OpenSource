/*
 * FTS Capacitive touch screen controller (FingerTipS)
 *
 * Copyright (C) 2013 TCL Inc.
 * Author: xiaoyang <xiaoyang.zhang@tcl.com>
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
#include <linux/kobject.h>
#include <linux/miscdevice.h>
#include <linux/workqueue.h>

#include <mach/mt_boot.h>
#include <mach/mt_pm_ldo.h>
#include <cust_eint.h>
#include "cust_gpio_usage.h"

#include "tpd.h"
#include "fts2a052_driver.h"
#include "tpd_custom_fts2a052.h"
#if 0
#ifdef MT6589
	extern void mt65xx_eint_unmask(unsigned int line);
	extern void mt65xx_eint_mask(unsigned int line);
	extern void mt65xx_eint_set_hw_debounce(unsigned int eint_num, unsigned int ms);
	extern unsigned int mt65xx_eint_set_sens(unsigned int eint_num, unsigned int sens);
	extern void mt65xx_eint_registration(unsigned int eint_num, unsigned int is_deb_en, unsigned int pol, void (EINT_FUNC_PTR)(void), unsigned int is_auto_umask);
#endif
#endif

extern struct tpd_device *tpd;
static DECLARE_WAIT_QUEUE_HEAD(st_waiter);
//static DEFINE_MUTEX(melfas_tp_mutex);

struct i2c_client *st_i2c_client = NULL;
static int st_tpd_flag = 0;
static u8 * DMAbuffer_va = NULL;
static dma_addr_t DMAbuffer_pa = NULL;
static int interrupt_count = 0;

static volatile unsigned char wr_buffer[16];
#define ST_MAX_I2C_TRANSFER_SIZE	8
#define ST_I2C_MASTER_CLOCK		100
#define FTS_EVENT_WAIT_MAX                  50//WORKAROUND reduce wake time //200
#define I2C_RETRY_CNT		3

static int fts_forcecal_ready = 0;
static int fts_mutualTune_ready = 0;
static int fts_selfTune_ready = 0;
static int fts_tuningbackup_ready = 0;
static unsigned short mtune_result = 1;
static unsigned short stune_result = 1;
static unsigned int ito_check_status = 1;
static int fts_tune_flag = 0;
static int fts_glovefunction_flag = 0;
static int fts_coverfunction_flag = 0;
static int doubleclick_counter = 0;
static int fts_doubleclick_status = 0;
static unsigned char old_buttons = 0;
static unsigned char modebyte = MODE_NORMAL;
static unsigned int              chip_fw_version;
static unsigned int              code_fw_version;
static unsigned int		 chip_config_version;
static unsigned int		 code_config_version;
static unsigned int		chip_afe1_version;
static unsigned int		chip_afe0_version;

#if 0 //define in tpd_custom_fts2a052.h
#define TPD_HAVE_BUTTON

#define TPD_KEY_COUNT		3
#define TPD_KEYS                {KEY_BACK, KEY_HOMEPAGE ,KEY_MENU}
#define TPD_KEYS_DIM            {{200,1980,100,80},\
				 {500,1980,100,80},\
				 {800,1980,100,80}}
#endif

#ifdef TPD_HAVE_BUTTON 
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif

int SenseChannelLength = 0;
int ForceChannelLength = 0;
short pFrame[36*60*2];
static unsigned char *p_tuneframe = NULL;

static const struct i2c_device_id st_fts_tpd_id[] = {{"st_fts",0},{}};
static struct i2c_board_info __initdata st_fts_i2c_tpd={ I2C_BOARD_INFO("st_fts", (0x92>>1))};

static int tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_i2c_remove(struct i2c_client *client);
static int tpd_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);

static void st_i2c_tpd_eint_interrupt_handler(void);
static int fts_get_fw_version(void);
static int fts_get_config_version(void);
static int fts_systemreset(void);
static int fts_init_flash_reload(void);
static int fts_interrupt_set(int enable);
static int fts_read_rawdata_result(unsigned char type);
static int fts_check_rawdata(short *pData);
static int fts_checkkey_rawdata(short *pData);
static int fts_tune_set(void);
static void fts_cleartune_flag(void);
static int fts_get_afe_version(void);
static int fts_check_autotune_result(void);
static int fts_check_autotune_fm1_result(void);
static void fts_delay(unsigned int ms);
static int fts_check_openshort_status(void);
static int fts_coverfunction_set(unsigned char cmd);
static int fts_glovefunction_set(unsigned char cmd);

const u8 st_firmware[] = {
#include "FTS2A052_hero2_MTK_0429.bin.h"
};

struct firmware fw_info_st = 
{
	.size = sizeof(st_firmware),
	.data = &st_firmware[0],
};

static struct i2c_driver tpd_i2c_driver =
{                       
    .probe = tpd_i2c_probe,                                   
    .remove = tpd_i2c_remove,                           
    .detect = tpd_i2c_detect,                           
    .driver.name = "mtk-tpd", 
    .id_table = st_fts_tpd_id,                             
/*#ifndef CONFIG_HAS_EARLYSUSPEND
    .suspend = melfas_ts_suspend, 
    .resume = melfas_ts_resume,
#endif*/    
}; 

static int fts_read_reg(unsigned char *reg, int cnum,
                        unsigned char *buf,
                        int num)
{    	
    	u8 retry = 0;
    	if (cnum > ST_MAX_I2C_TRANSFER_SIZE) {
    		TPD_DMESG("Line %d, input parameter error\n", __LINE__);
    		return -1;
    	}
    	if (num > ST_MAX_I2C_TRANSFER_SIZE) {
    		TPD_DMESG("Line %d, output parameter error\n", __LINE__);
    		return -1;
    	}
    	
    	memset(wr_buffer, 0, 16);
    	memcpy(wr_buffer, reg, cnum);

    	struct i2c_msg msgs[] =
    	{
        	{            
            	.addr = st_i2c_client->addr,
            	//.flags = 0,
            	.len =  ((num<<8) | cnum),
            	.timing = I2C_MASTER_CLOCK_ST,
            	.buf = wr_buffer,
            	.ext_flag = (((st_i2c_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_WR_FLAG | I2C_RS_FLAG),
        	},
    	};	
    	
    	for (retry = 0; retry < I2C_RETRY_CNT; retry++) {
    		if (i2c_transfer(st_i2c_client->adapter, msgs, 1) == 1)
    			break;
		mdelay(5);
		TPD_DMESG("Line %d, i2c_transfer error, retry = %d\n", __LINE__, retry);
	}
	if (retry == I2C_RETRY_CNT) {
		printk(KERN_ERR "fts_read_reg retry over %d\n",I2C_RETRY_CNT);
		return -1;
	}
	
    	memcpy(buf, wr_buffer, num);
    	/*for(i=0; i<num; i++)
        	buf[i]=wr_buffer[i];*/
        	
    	/*TPD_DMESG("READ DATA:%02x %02x %02x %02x %02x %02x %02x %02x\n",
           wr_buffer[0],wr_buffer[1],wr_buffer[2],wr_buffer[3],
           wr_buffer[4],wr_buffer[5],wr_buffer[6],wr_buffer[7]);*/
        /*TPD_DMESG("READ DATA:%02x %02x %02x %02x %02x %02x %02x %02x\n",
           buf[0],buf[1],buf[2],buf[3],
           buf[4],buf[5],buf[6],buf[7]);*/
              
    	return 0;
}

static int fts_dma_read_reg(unsigned char *reg, int cnum,
                         unsigned char *buf,
                         int num)
{
	int retry = 0;
    
    	memset(DMAbuffer_va, 0, 256);
    	memcpy(DMAbuffer_va, reg, cnum);
   	struct i2c_msg msgs[] =
    	{
        	{
            	//.addr = (((fts_i2c_client->addr ) & I2C_MASK_FLAG ) | I2C_WR_FLAG | I2C_RS_FLAG),
            	.addr = st_i2c_client->addr,
            	//.flags = 0,
            	.timing = I2C_MASTER_CLOCK_ST,
            	.len =  ((num<<8) | cnum),
            	.buf = DMAbuffer_pa,
            	.ext_flag = (((st_i2c_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_WR_FLAG | I2C_RS_FLAG | I2C_DMA_FLAG),
        	},
    	};
    	for (retry = 0; retry < I2C_RETRY_CNT; retry++) {
		if (i2c_transfer(st_i2c_client->adapter, msgs, 1) == 1)
			break;
		mdelay(5);
		TPD_DMESG("Line %d, i2c dma transfer error, retry = %d\n", __LINE__, retry);
	}
	if (retry == I2C_RETRY_CNT) {
		printk(KERN_ERR "fts_dma_read_reg retry over %d\n",I2C_RETRY_CNT);
		return -1;
	}
    
    	memcpy(buf, DMAbuffer_va, num);
    	/*TPD_DMESG("READ DMA DATA:%02x %02x %02x %02x %02x %02x %02x %02x\n",
           DMAbuffer_va[0],DMAbuffer_va[1],DMAbuffer_va[2],DMAbuffer_va[3],
           DMAbuffer_va[4],DMAbuffer_va[5],DMAbuffer_va[6],DMAbuffer_va[7]);*/
           
    	return 0;
}

static int fts_write_reg(unsigned char *reg,
                         unsigned short num_com)
{
    	u8 retry = 0;
    	struct i2c_msg msgs[] =
    	{
        	{
            	//.addr = fts_i2c_client->addr & I2C_MASK_FLAG,  //77,75
            	.addr = st_i2c_client->addr,
            	.flags = 0,
            	.timing = I2C_MASTER_CLOCK_ST,
            	.len = num_com,
            	.buf = reg,
            	.ext_flag = ((st_i2c_client->ext_flag ) & I2C_MASK_FLAG ), //msz 6572
        	}
    	};
    	for (retry = 0; retry < I2C_RETRY_CNT; retry++) {
    		if (i2c_transfer(st_i2c_client->adapter, msgs, 1) == 1)
    			break;
		mdelay(5);
		TPD_DMESG("Line %d, i2c_transfer error, retry = %d\n", __LINE__, retry);
	}
	if (retry == I2C_RETRY_CNT) {
		printk(KERN_ERR "fts_write_reg error %d\n",I2C_RETRY_CNT);
		return -1;
	}
    
    	return 0;
}

static int fts_dma_write_reg( unsigned char *reg,
                          unsigned short num_com)
{

	int retry = 0;
	memset(DMAbuffer_va, 0, 256);
	memcpy(DMAbuffer_va, reg, num_com);
	struct i2c_msg msgs[] =
	{
		{
		.addr = st_i2c_client->addr,
		.len =  num_com,
		.timing = I2C_MASTER_CLOCK_ST,
		.buf = DMAbuffer_pa,
		.ext_flag = (((st_i2c_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_DMA_FLAG),
        	},
	};
	for (retry = 0; retry < I2C_RETRY_CNT; retry++) {
		if (i2c_transfer(st_i2c_client->adapter, msgs, 1) == 1)
			break;
		mdelay(5);
		TPD_DMESG("Line %d, i2c_transfer error, retry = %d\n", __LINE__, retry);
	}
	if (retry == I2C_RETRY_CNT) {
		printk(KERN_ERR "fts_dma_write_reg retry over %d\n",I2C_RETRY_CNT);
		return -1;
	}
	
	return 0;
}

static int fts_command( unsigned char cmd)
{
    unsigned char regAdd = 0;
    int ret = 0;

    regAdd = cmd;
    ret = fts_write_reg(&regAdd, 1);
    TPD_DMESG("FTS Command (%02X) , ret = %d \n", cmd, ret);
    return ret;
}


static ssize_t tp_value_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	char *s = buf;
	unsigned char val[8];
	unsigned char regAdd[3];
	int error;
	int ret;
	//int result;
	int code_fw_ver = 0;
	int chip_fw_ver = 0;
	/* Read out multi-touchscreen chip ID */
	regAdd[0] = 0xB6;
	regAdd[1] = 0x00;
	regAdd[2] = 0x07;
	
	error = fts_read_reg(regAdd, sizeof(regAdd), val, sizeof(val));
	if (error < 0) {
		printk("Cannot read fts device id\n");
		//return -ENODEV;
	}
	error = fts_get_fw_version();
	if (error < 0) {
		printk("Cannot read fts firmware version\n");
		//return -ENODEV;
	}
	error = fts_get_config_version();
	if (error < 0) {
		printk("Cannot read fts config version\n");
		//return -ENODEV;
	}
	s += sprintf(s, "CTP Vendor: %s\n", "ST");
	s += sprintf(s, "CTP Type: %s\n", "Interrupt trigger");
	s += sprintf(s, "Chip ID: %x %x\n", val[1], val[2]);
	s += sprintf(s, "Firmware Version: 0x%x\n", chip_fw_version);
	s += sprintf(s, "Config Version: 0x%x\n", chip_config_version);
	return (s - buf);
}

static ssize_t tp_value_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{
	int save;
	
	if (sscanf(buf, "%d", &save)==0) {
		printk(KERN_ERR "%s -- invalid save string '%s'...\n", __func__, buf);
		return -EINVAL;
	}
	return n;
}

static ssize_t raw_data_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	char *s = buf;	
	int ret = 0;
	
	ret = fts_read_rawdata_result(0);
	if (ret != 0) {
		printk("Cannot read fts raw data \n");
		s += sprintf(s, "CTP read raw data: %s\n", "FAIL");
	} else {
		printk("successful read fts raw data \n");
		s += sprintf(s, "CTP read raw data: %s\n", "OK");
	}	
	
	return (s - buf);
}

static ssize_t raw_data_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{		
	return n;
}

static ssize_t tp_calibration_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	char *s = buf;
	int ap_test_result = 1;
	int fm_test_result = 1;

	fm_test_result = fts_check_autotune_fm1_result();
	ap_test_result = fts_check_autotune_result();
	fts_delay(500);
	fts_check_openshort_status();
	
	if (fts_tune_flag == 1)
		s += sprintf(s, "CTP tune result: %s\n", "FM Tune OK");
	else
		s += sprintf(s, "CTP tune result: %s\n", "FM Tune FAIL");

	if (fm_test_result == 0)
		s += sprintf(s, "CTP tune result: %s\n", "FM OK");
	else
		s += sprintf(s, "CTP tune result: %s\n", "FM FAIL");
		
	if (ap_test_result == 0)
		s += sprintf(s, "CTP tune result: %s\n", "AP OK");
	else
		s += sprintf(s, "CTP tune result: %s\n", "AP FAIL");
	if (ito_check_status == 0)
		s += sprintf(s, "CTP ITO test result: %s\n", "ITO OK");
	else
		s += sprintf(s, "CTP ITO test result: %s\n", "ITO FAIL");
		
	return (s - buf);
}

static ssize_t tp_calibration_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{		
	int save;
	
	if (sscanf(buf, "%d", &save)==0) {
		printk(KERN_ERR "%s -- invalid save string '%s'...\n", __func__, buf);
		return -EINVAL;
	}
	fts_cleartune_flag();
	fts_tune_set();
	return n;
}

static ssize_t tp_glovetest_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	char *s = buf;
	
	s += sprintf(s, "CTP glove test result: %s\n", "TEST");		
	return (s - buf);
}

static ssize_t tp_glovetest_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{		
	int save;
	
	if (sscanf(buf, "%d", &save)==0) {
		printk(KERN_ERR "%s -- invalid save string '%s'...\n", __func__, buf);
		return -EINVAL;
	}

	if (save == 0)
		fts_glovefunction_set(GLOVE_OFF);
	else if (save == 1)
		fts_glovefunction_set(GLOVE_ON);
	return n;
}

static ssize_t tp_covertest_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	char *s = buf;
	
	s += sprintf(s, "CTP cover test result: %s\n", "TEST");		
	return (s - buf);
}

static ssize_t tp_covertest_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{		
	int save;
	
	if (sscanf(buf, "%d", &save)==0) {
		printk(KERN_ERR "%s -- invalid save string '%s'...\n", __func__, buf);
		return -EINVAL;
	}

	if (save == 0)
		fts_doubleclick_status = 0;
	else if (save == 1)
		fts_doubleclick_status = 1;
	return n;
}

static ssize_t tp_doubletaptest_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	char *s = buf;
	
	s += sprintf(s, "CTP doubletap test result: %s\n", "TEST");		
	return (s - buf);
}

static ssize_t tp_doubletaptest_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{		
	int save;
	
	if (sscanf(buf, "%d", &save)==0) {
		printk(KERN_ERR "%s -- invalid save string '%s'...\n", __func__, buf);
		return -EINVAL;
	}

	if (save == 0)
		fts_coverfunction_set(GOVER_OFF);
	else if (save == 1)
		fts_coverfunction_set(GOVER_ON);
	return n;
}

static struct kobject *tpswitch_ctrl_kobj;

#define tpswitch_ctrl_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0664,			\
	},					\
	.show	= _name##_show,			\
	.store	= _name##_store,		\
}


tpswitch_ctrl_attr(tp_value);
tpswitch_ctrl_attr(raw_data);
tpswitch_ctrl_attr(tp_calibration);
tpswitch_ctrl_attr(tp_glovetest);
tpswitch_ctrl_attr(tp_covertest);
tpswitch_ctrl_attr(tp_doubletaptest);

static struct attribute *g_attr[] = {
	&tp_value_attr.attr,
	&raw_data_attr.attr,
	&tp_calibration_attr.attr,
	&tp_glovetest_attr.attr,
	&tp_covertest_attr.attr,
	&tp_doubletaptest_attr.attr,	
	NULL,
};

static struct attribute_group tpswitch_attr_group = {
	.attrs = g_attr,
};

static int tpswitch_sysfs_init(void)
{ 
	tpswitch_ctrl_kobj = kobject_create_and_add("tp-info", NULL);
	if (!tpswitch_ctrl_kobj)
		return -ENOMEM;

	return sysfs_create_group(tpswitch_ctrl_kobj, &tpswitch_attr_group);
}

static void tpswitch_sysfs_exit(void)
{
	sysfs_remove_group(tpswitch_ctrl_kobj, &tpswitch_attr_group);

	kobject_put(tpswitch_ctrl_kobj);
}

static void st_ts_release_all_finger(void)
{
	int i;
	TPD_DMESG("[st_tpd] %s\n", __func__);
	for (i = 0; i < FINGER_MAX; i++)
	{
		input_mt_slot(tpd->dev, i);
		input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, -1);
	}
	input_sync(tpd->dev);
}

static void fts_delay(unsigned int ms)
{
    msleep(ms);
}

static int fts_get_fw_version(void)
{
	int ret;
	unsigned char data[FTS_EVENT_SIZE];
	unsigned char regAdd[3] = {0xB6, 0x00, 0x07};

	ret = fts_read_reg(regAdd, sizeof(regAdd), data, sizeof(data));

	if (ret)
		chip_fw_version = 0;
	else
		chip_fw_version = (data[5] << 8) | data[4];

	return ret;
}

static int fts_get_config_version(void)
{
	int ret;
	unsigned char data[FTS_EVENT_SIZE];
	unsigned char regoneAdd;
	//unsigned char regAdd[3] = {0xB2, 0x00, 0x01, 0x02};
	int retry = 0;
	unsigned char event_id = 0;
	unsigned char regAdd[4] =
	{
        0xB2, 0x00, 0x01, 0x02
	};

		
	fts_interrupt_set(INT_DISABLE);
	fts_command(FLUSHBUFFER);
	
	TPD_DMESG("FTS fts_get_config_version\n");
	ret = fts_write_reg(&regAdd[0], 4);
	for (retry = 0; retry < 10; retry++) {
		regoneAdd = READ_ONE_EVENT;
    		ret = fts_read_reg(&regoneAdd, 1, data, FTS_EVENT_SIZE);
    		if (ret == 0) {
    			event_id = data[0];
    			if (event_id == 0x12) {
    				chip_config_version = (data[3] << 8) | data[4];
    				break;
    			}
    			else {
    				fts_delay(10);
    				chip_config_version = 0;
				TPD_DMESG("fts tune delay count %d\n",retry);
			}
    		}
    		else {
    			chip_config_version = 0;
			fts_delay(10);
		}
	}
		
	TPD_DMESG("fts_get_config_version is %x.\n",chip_config_version);	
	fts_interrupt_set(INT_ENABLE);
	return ret;
}

static int fts_get_afe_version(void)
{
	int ret;
	unsigned char data[FTS_EVENT_SIZE];
	unsigned char regoneAdd;
	unsigned char regAdd[4] = {0xB2, 0x07, 0xFB, 0x01};
	int retry = 0;
	unsigned char event_id = 0;
	
	fts_interrupt_set(INT_DISABLE);
	fts_command(FLUSHBUFFER);
	
	ret = fts_write_reg(&regAdd[0], 4);	
	for (retry = 0; retry < 10; retry++) {
    		regoneAdd = READ_ONE_EVENT;
    		ret = fts_read_reg(&regoneAdd, 1, data, FTS_EVENT_SIZE);
    		if (ret == 0) {
    			event_id = data[0];
    			if (event_id == 0x12) {
    				chip_afe0_version = data[3];
    				break;
    			}
    			else {
    				fts_delay(10);
				TPD_DMESG("fts tune delay count %d\n",retry);
			}
    		}
    		else {
			fts_delay(10);
		}
	}
	/*if (retry == 10) {
		ret = 2;
		goto ErrorATExit;
	}*/
	
	regAdd[1] = 0x17;
	ret = fts_write_reg(&regAdd[0], 4);
	for (retry = 0; retry < 10; retry++) {
    		regoneAdd = READ_ONE_EVENT;
    		ret = fts_read_reg(&regoneAdd, 1, data, FTS_EVENT_SIZE);
    		if (ret == 0) {
    			event_id = data[0];
    			if (event_id == 0x12) {
    				chip_afe1_version = data[3];
    				break;
    			}
    			else {
    				fts_delay(10);
				TPD_DMESG("fts tune delay count %d\n",retry);
			}
    		}
    		else {
			fts_delay(10);
		}
	}
	/*if (retry == 10) {
		ret = 2;
		goto ErrorATExit;
	}*/
	
	TPD_DMESG("fts_get_afe_version is %x %x.\n",chip_afe0_version,chip_afe1_version);
	
	fts_interrupt_set(INT_ENABLE);
	return ret;
}

static int fts_flash_status(unsigned int timeout, unsigned int steps)
{
	int ret, status;
	unsigned char data;
	unsigned char regAdd[2];

	do {
		regAdd[0] = FLASH_READ_STATUS;
		regAdd[1] = 0;
		
		msleep(20);

		ret = fts_read_reg(regAdd, sizeof(regAdd), &data, sizeof(data));
		if (ret)
			status = FLASH_STATUS_UNKNOWN;
		else
			status = (data & 0x01) ? FLASH_STATUS_BUSY : FLASH_STATUS_READY;

		if (status == FLASH_STATUS_BUSY) {
			timeout -= steps;
			msleep(steps);
		}

	} while ((status == FLASH_STATUS_BUSY) && (timeout));

	return status;
}

static int fts_flash_unlock(void)
{
	int ret;
	unsigned char regAdd[4] = { FLASH_UNLOCK,
				FLASH_UNLOCK_CODE_0,
				FLASH_UNLOCK_CODE_1,
				0x00 };

	ret = fts_write_reg(regAdd, sizeof(regAdd));

	if (ret)
		TPD_DMESG("Cannot unlock flash\n");
	else
		{
		//msleep(FTS_FLASH_COMMAND_DELAY);
		TPD_DMESG( "Flash unlocked\n");
	}

	return ret;
}

static int fts_flash_load(int cmd, int address, const char *data, int size)
{
	int ret;
	unsigned char *cmd_buf;
	unsigned int loaded;

	cmd_buf = kmalloc(FLASH_LOAD_COMMAND_SIZE, GFP_KERNEL);
	if (cmd_buf == NULL) {
		TPD_DMESG("FTS Out of memory when programming flash\n");
		return -1;
	}
	TPD_DMESG("FTS flash size is: %x\n",size);
	loaded = 0;
	while (loaded < size) {
		// TPD_DMESG("FTS flash loaded %x\n",loaded);
#if 1
		cmd_buf[0] = cmd;
		cmd_buf[1] = (address >> 8) & 0xFF;
		cmd_buf[2] = (address) & 0xFF;

		memcpy(&cmd_buf[3], data, FLASH_LOAD_CHUNK_SIZE);
		ret = fts_dma_write_reg(cmd_buf, FLASH_LOAD_COMMAND_SIZE);
		if (ret) {
			TPD_DMESG("FTS Cannot load firmware in RAM\n");
			break;
		}

		data += FLASH_LOAD_CHUNK_SIZE;
		loaded += FLASH_LOAD_CHUNK_SIZE;
		address += FLASH_LOAD_CHUNK_SIZE;
#else
		/* use B3/B1 commands */
		unsigned char cmd_buf_b3[3];

		cmd_buf_b3[0] = 0xB3;
		cmd_buf_b3[1] = 0x00;
		cmd_buf_b3[2] = 0x00;
		ret = fts_write_reg(info, cmd_buf_b3, 3);

		cmd_buf[0] = 0xB1;
		cmd_buf[1] = (address >> 8) & 0xFF;
		cmd_buf[2] = (address) & 0xFF;

		memcpy(&cmd_buf[3], data, FLASH_LOAD_CHUNK_SIZE);
		ret = fts_write_reg(info, cmd_buf, FLASH_LOAD_COMMAND_SIZE);
		if (ret) {
			dev_err(info->dev, "Cannot load firmware in RAM\n");
			break;
		}

		data += FLASH_LOAD_CHUNK_SIZE;
		loaded += FLASH_LOAD_CHUNK_SIZE;
		address += FLASH_LOAD_CHUNK_SIZE;
#endif
	}

	kfree(cmd_buf);
	TPD_DMESG("FTS flash loaded = %d, size = %d\n",loaded, size);
	return (loaded == size) ? 0 : -1;
}


static int fts_flash_erase(int cmd)
{
	int ret;
	unsigned char regAdd = cmd;

	ret = fts_write_reg(&regAdd, sizeof(regAdd));

	if (ret)
		TPD_DMESG("Cannot erase flash\n");
	else
		{
		//msleep(FTS_FLASH_COMMAND_DELAY);
		TPD_DMESG("Flash erased\n");
	}

	return ret;
}

static int fts_flash_program(int cmd)
{
	int ret;
	unsigned char regAdd = cmd;

	ret = fts_write_reg(&regAdd, sizeof(regAdd));

	if (ret)
		TPD_DMESG("Cannot program flash\n");
	else
		{		
		TPD_DMESG("Flash programmed\n");
	}
	return ret;
}


static int st_fw_update_controller(const struct firmware *fw, struct i2c_client *client)
{
	int ret;	
	int status;
	unsigned char *data;
	unsigned int size;
	int program_command, erase_command, load_command, load_address;
	int update_flag = 0;
	
	data = (unsigned char *)fw->data;
	size = fw->size;
	ret = fts_get_fw_version();
	if (ret) {
		TPD_DMESG(
			"fts Cannot retrieve current firmware version, not upgrading it\n");
		return ret;
	}
	ret = fts_get_config_version();
	if (ret) {
		TPD_DMESG(
			"fts Cannot retrieve current config version, not upgrading it\n");
		return ret;
	}
	
	code_fw_version = (data[5] << 8) | data[4];
	TPD_DMESG("FTS code firmware version is %x.\n",code_fw_version);
	TPD_DMESG("FTS old chip firmware ver is %x.\n",chip_fw_version);
	code_config_version = (data[13] << 8) | data[14];
	TPD_DMESG("FTS code config version is %x.\n",code_config_version);
	TPD_DMESG("FTS old chip config ver is %x.\n",chip_config_version);
	
	if (chip_fw_version < code_fw_version ) {
		update_flag = 1;
	
	} else if (chip_fw_version == code_fw_version ) {
		if (chip_config_version < code_config_version ) 
			update_flag = 1;
	}
	
	if (code_fw_version == 0x3c9)
		update_flag = 0;
	if ((chip_fw_version== 0x3c9) || (chip_fw_version== 0x3f8) || (chip_fw_version== 0x393))
		update_flag = 1;
        if ((chip_fw_version== 0x3de) || (chip_config_version == 0x350))
            update_flag = 1;

	if (update_flag == 1) {
	data += 32;
	size = size - 32;
	program_command = FLASH_PROGRAM;
	erase_command = FLASH_ERASE;
	load_command = FLASH_LOAD_FIRMWARE;
	load_address = FLASH_LOAD_FIRMWARE_OFFSET;			
	
	TPD_DMESG("FTS Flash size %x...\n",size);
	TPD_DMESG("FTS DATA:%02x %02x %02x %02x %02x %02x %02x %02x\n",
           data[0],data[1],data[2],data[3],
           data[4],data[5],data[6],data[7]);
	//if (fw_version < new_fw_version) 
	{
		TPD_DMESG("FTS Flash programming...\n");
		
		TPD_DMESG("FTS 1) checking for status.\n");
		status = fts_flash_status(1000, 100);
		if ((status == FLASH_STATUS_UNKNOWN) || (status == FLASH_STATUS_BUSY)) {
			TPD_DMESG("Wrong flash status\n");
			goto fw_done;
		}
			
		TPD_DMESG("FTS 2) unlock the flash.\n");
		ret = fts_flash_unlock();
		if (ret) {
			TPD_DMESG("Cannot unlock the flash device\n");
			goto fw_done;
		}
		/* wait for a while */
		msleep(FTS_FLASH_COMMAND_DELAY);
		
		TPD_DMESG("FTS 3) load the program.\n");
		ret = fts_flash_load(load_command, load_address, data, size);
		if (ret) {
			TPD_DMESG(
			"Cannot load program to for the flash device\n");
			goto fw_done;
		}
		/* wait for a while */
		msleep(FTS_FLASH_COMMAND_DELAY);
	
		TPD_DMESG("FTS 4) erase the flash.\n");
		ret = fts_flash_erase(erase_command);
		if (ret) {
			TPD_DMESG("Cannot erase the flash device\n");
			goto fw_done;
		}
		/* wait for a while */
		msleep(FTS_FLASH_COMMAND_DELAY);

		TPD_DMESG("FTS 5) checking for status.\n");
		status = fts_flash_status(1000, 100);
		if ((status == FLASH_STATUS_UNKNOWN) || (status == FLASH_STATUS_BUSY)) {
			TPD_DMESG("Wrong flash status\n");
			goto fw_done;
		}
		/* wait for a while */
		msleep(FTS_FLASH_COMMAND_DELAY);

		TPD_DMESG("FTS 6) program the flash.\n");
		ret = fts_flash_program(program_command);
		if (ret) {
			TPD_DMESG("Cannot program the flash device\n");
			goto fw_done;
		}
		/* wait for a while */
		msleep(FTS_FLASH_COMMAND_DELAY);

		TPD_DMESG("FTS , Flash programming: done.\n");
	
		TPD_DMESG("Perform a system reset\n");
		ret = fts_systemreset();
		if (ret) {
			TPD_DMESG("Cannot reset the device\n");
			goto fw_done;
		}

		ret = fts_init_flash_reload();
		if (ret) {
			TPD_DMESG("Cannot initialize the hardware device\n");
			goto fw_done;
		}

		ret = fts_get_fw_version();
		if (ret) {
			TPD_DMESG("Cannot retrieve firmware version\n");
			goto fw_done;
		}
		TPD_DMESG("FTS , new chip firmware ver is:0x%x\n",chip_fw_version);
		ret = fts_get_config_version();
		if (ret) {
			TPD_DMESG("Cannot retrieve config version\n");
			goto fw_done;
		}		
		TPD_DMESG("FTS , new chip config ver is:0x%x\n",chip_config_version);
		/*dev_info(info->dev,
			"New firmware version 0x%04x installed\n",
		info->fw_version);	*/

	}
fw_done:

	return ret;
	}
	else {
		TPD_DMESG("FTS PASS firmware update..\n");
		return 0;
	}
}

static int fts_fw_upgrade(void)
{
	int ret;
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	ret = st_fw_update_controller(&fw_info_st,st_i2c_client);
	
	msleep(10);
   	//irq unmask
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	return ret;
}

static int fts_systemreset(void)
{
	int ret;
	unsigned char regAdd[4] =
	{
        0xB6, 0x00, 0x23, 0x01
	};

	TPD_DMESG("FTS SystemReset\n");
	ret = fts_write_reg(&regAdd[0], 4);
	fts_delay(20);
	return ret;
}

unsigned char fts_read_modebyte()
{
    int rc;
    unsigned char data[8];

    unsigned char regAdd[4] =
    {
        0xB2, 0x00, 0x02, 0x01
    };

    fts_write_reg(&regAdd[0], 4);
    fts_delay(10);

    regAdd[0] = READ_ONE_EVENT;
    rc = fts_read_reg( &regAdd[0], 1, (unsigned char *)data,
                      FTS_EVENT_SIZE);

    return data[3];
}

static void fts_wait_sleepout_ready()
{
    int i;

    /*polling maximum time*/   
    fts_delay(5);

    TPD_DMESG("fts fts_wait_sleepout_ready end polling\n");
    return;
}

//static void fts_wait_forcecal_ready(struct fts_ts_info *info)
static void fts_wait_forcecal_ready(void)
{
    int i;

    /*polling maximum time*/
    for (i = 0; i < FTS_EVENT_WAIT_MAX; i++)
    {
        /*sleepout generates forcecal event*/
        if (fts_forcecal_ready)
        {
            fts_forcecal_ready = 0;
            return;
        }
        fts_delay(5);
    }

    TPD_DMESG("fts fts_wait_forcecal_ready end polling\n");
    return;
}


static void fts_wait_Mutual_atune_ready(void)
{
    int i;

    /*polling maximum time*/
    for (i = 0; i < FTS_EVENT_WAIT_MAX; i++)
    {
        /*sleepout generates forcecal event*/
        if (fts_mutualTune_ready)
        {
            //fts_mutualTune_ready = 0;
            return;
        }
        fts_delay(10);
    }

    TPD_DMESG("fts fts_wait_Mutual_atune_ready end polling\n");
    return;
}

//static void fts_wait_Self_atune_ready(struct fts_ts_info *info)
static void fts_wait_Self_atune_ready(void)
{
    int i;

    /*polling maximum time*/
    for (i = 0; i < FTS_EVENT_WAIT_MAX; i++)
    {
        /*sleepout generates forcecal event*/
        if (fts_selfTune_ready)
        {
            //fts_selfTune_ready = 0;
            return;
        }
        fts_delay(10);
    }

    TPD_DMESG("fts fts_wait_Self_atune_ready end polling\n");
    return;
}


static void fts_wait_tuning_backup_ready(void)
{
    int i;

    /*polling maximum time*/
    for (i = 0; i < FTS_EVENT_WAIT_MAX; i++)
    {
        /*sleepout generates forcecal event*/
        if (fts_tuningbackup_ready)
        {
            fts_tuningbackup_ready = 0;
            return;
        }
        fts_delay(5);
    }

    TPD_DMESG("fts fts_wait_tuning_backup_ready end polling\n");
    return;
}

static int fts_interrupt_set(int enable)
{
	int ret;
    unsigned char regAdd[4] =
    {
        0
    };

    regAdd[0] = 0xB6;
    regAdd[1] = 0x00;
    regAdd[2] = 0x1C;
    regAdd[3] = enable;

    if (enable)
    {
        TPD_DMESG("FTS INT Enable\n");
    }
    else
    {
        TPD_DMESG("FTS INT Disable\n");
    }

    ret = fts_write_reg(&regAdd[0], 4);
	return ret;
}

static void fts_cleartune_flag(void)
{
	TPD_DMESG("fts clear tune flag\n");
	fts_mutualTune_ready = 0;
	fts_selfTune_ready = 0;
	mtune_result = 1;
	stune_result = 1;
	fts_tune_flag = 0;
	return;
}

static int fts_glovefunction_set(unsigned char cmd)
{
	int ret;    
        TPD_DMESG("FTS glove function set %x\n",cmd);
    
    	ret = fts_command(cmd);
    	if (!ret) {
    		if (cmd == GLOVE_ON)
    			fts_glovefunction_flag = 1;
    		else if (cmd == GLOVE_OFF)
    			fts_glovefunction_flag = 0;
    	}
	return ret;
}

static int fts_coverfunction_set(unsigned char cmd)
{
	int ret;
	TPD_DMESG("FTS cover function set %x\n",cmd);

    	ret = fts_command(cmd);
    	if (!ret) {
    		if (cmd == GLOVE_ON)
    			fts_coverfunction_flag = 1;
    		else if (cmd == GLOVE_OFF)
    			fts_coverfunction_flag = 0;
    	}
    	fts_delay(50);
    	fts_command(FORCECALIBRATION);
    	
    	if (cmd == GLOVE_OFF) {
    		if (fts_glovefunction_flag == 1)
    			fts_glovefunction_set(GLOVE_ON);
    	}
    	
	return ret;
}

static int fts_tune_set(void)
{
	int rc = 0;
	int ret = 0;
	
	unsigned char data[FTS_EVENT_SIZE];
	unsigned char regoneAdd;
	int retry = 0;
	unsigned char event_id = 0;
	unsigned char tune_flag = 0;
	unsigned short fm_check_status1 = 1;
	unsigned short fm_check_status2 = 1;
	
	rc += fts_systemreset();
	fts_delay(200);
	rc += fts_command(SLEEPOUT);
	fts_delay(10);
	ret += fts_interrupt_set(INT_DISABLE);
	fts_delay(10);
	ret += fts_command(FLUSHBUFFER);
	fts_delay(10);
	
	rc += fts_command(CX_TUNNING);
	fts_wait_Mutual_atune_ready();

	fts_delay(200);
	for (retry = 0; retry < 10; retry++) {
		regoneAdd = READ_ONE_EVENT;
    		ret = fts_read_reg(&regoneAdd, 1, data, FTS_EVENT_SIZE);
    		if (ret == 0) {
    			TPD_DMESG("FTS fts at event: %02X %02X %02X %02X %02X %02X %02X %02X\n",
           			data[0], data[1], data[2], data[3],
           			data[4], data[5], data[6], data[7]);
    			//TPD_DMESG("FTS fts ito event: %02X %02X %02X %02X\n",data[0],data[1],data[2], data[3]);
    			
    			event_id = data[0];
    			tune_flag = data[1];
    			TPD_DMESG("FTS event: %04X %04X\n",event_id,tune_flag);
    			if ((event_id == 0x16) && ((tune_flag == 0x01) || (tune_flag == 0x02))) {
    				/*if (ito_flag == 0x05) {    				
    				}*/
    				TPD_DMESG("FTS event: %04X %04X\n",event_id,tune_flag);
    				fm_check_status1 = (data[3] << 8) | data[4];
    				break;
    			}
    			else {
    				fts_delay(200);
    				fm_check_status1 = 1;
				TPD_DMESG("FTS fts fm delay count %d\n",retry);
			}
    		}
    		else {
    			fm_check_status1 = 1;
			fts_delay(200);
		}
	}
		
	rc += fts_command(SELF_TUNING);
	fts_delay(200);
	for (retry = 0; retry < 10; retry++) {
		regoneAdd = READ_ONE_EVENT;
    		ret = fts_read_reg(&regoneAdd, 1, data, FTS_EVENT_SIZE);
    		if (ret == 0) {
    			TPD_DMESG("FTS fts at event: %02X %02X %02X %02X %02X %02X %02X %02X\n",
           			data[0], data[1], data[2], data[3],
           			data[4], data[5], data[6], data[7]);
    			//TPD_DMESG("FTS fts ito event: %02X %02X %02X %02X\n",data[0],data[1],data[2], data[3]);
    			
    			event_id = data[0];
    			tune_flag = data[1];
    			TPD_DMESG("FTS event: %04X %04X\n",event_id,tune_flag);
    			if ((event_id == 0x16) && ((tune_flag == 0x01) || (tune_flag == 0x02))) {
    				/*if (ito_flag == 0x05) {    				
    				}*/
    				TPD_DMESG("FTS event: %04X %04X\n",event_id,tune_flag);
    				fm_check_status2 = (data[3] << 8) | data[4];
    				break;
    			}
    			else {
    				fts_delay(200);
    				fm_check_status2 = 1;
				TPD_DMESG("FTS fts fm delay count %d\n",retry);
			}
    		}
    		else {
    			fm_check_status2 = 1;
			fts_delay(200);
		}
	}
	
	if ((fm_check_status1 == 0) && (fm_check_status2 == 0)) {		
		TPD_DMESG("fts tune flag ok\n");	
		rc += fts_command(TUNING_BACKUP1);
		rc += fts_command(TUNING_BACKUP2);
		fts_wait_tuning_backup_ready();
		if (rc == 0) {
			fts_tune_flag = 1;
		} else {
			rc = -1;
			TPD_DMESG("fts tune result fail\n");
		}	
	}
	else {
		rc =-1;	
		TPD_DMESG("fts tune fail\n");
	}
	
	ret += fts_systemreset();
	fts_delay(200);
	/* wake-up the device */
	ret += fts_command(SLEEPOUT);
	fts_delay(50);
	
	/* enable sense */
	fts_command(SENSEON);
	
	fts_command(BUTTON_ON);
		
	//if (fts_tune_flag == 1) {
	rc += fts_command(FORCECALIBRATION);
	fts_wait_forcecal_ready();
	//}
	
	return rc;
}

static int fts_check_fm_status(void)
{
	int ret = 0;
	unsigned char data[FTS_EVENT_SIZE];
	unsigned char regoneAdd;
	//unsigned char regAdd[3] = {0xB2, 0x00, 0x01, 0x02};
	int retry = 0;
	unsigned char event_id = 0;
	unsigned char ito_flag = 0;
	
	ito_check_status = 1;
	#if 1
	ret += fts_systemreset();
	fts_delay(200);
	ret += fts_command(SLEEPOUT);
	fts_delay(50);
	ret += fts_interrupt_set(INT_DISABLE);
	fts_delay(50);
	ret += fts_command(FLUSHBUFFER);
	fts_delay(50);
	ret += fts_command(ITO_CHECK);
	TPD_DMESG("FTS fts ito check\n");
	fts_delay(200);
	for (retry = 0; retry < 10; retry++) {
		regoneAdd = READ_ONE_EVENT;
    		ret = fts_read_reg(&regoneAdd, 1, data, FTS_EVENT_SIZE);
    		if (ret == 0) {
    			TPD_DMESG("FTS fts ito event: %02X %02X %02X %02X %02X %02X %02X %02X\n",
           			data[0], data[1], data[2], data[3],
           			data[4], data[5], data[6], data[7]);
    			//TPD_DMESG("FTS fts ito event: %02X %02X %02X %02X\n",data[0],data[1],data[2], data[3]);
    			
    			event_id = data[0];
    			ito_flag = data[1];
    			TPD_DMESG("FTS event: %04X %04X\n",event_id,ito_flag);
    			if ((event_id == 0x0f) && (ito_flag == 0x05)) {
    				/*if (ito_flag == 0x05) {    				
    				}*/
    				TPD_DMESG("FTS event: %04X %04X\n",event_id,ito_flag);
    				ito_check_status = (data[3] << 8) | data[4];
    				break;
    			}
    			else {
    				fts_delay(200);
    				ito_check_status = 1;
				TPD_DMESG("FTS fts tune delay count %d\n",retry);
			}
    		}
    		else {
    			ito_check_status = 1;
			fts_delay(200);
		}
	}
	
		
	TPD_DMESG("FTS fts check openshort_status is %x.\n",ito_check_status);

	ret += fts_systemreset();
	fts_delay(200);
	/* wake-up the device */
	ret += fts_command(SLEEPOUT);
	fts_delay(50);
	/* enable sense */
	ret += fts_command(SENSEON);
	fts_delay(50);	
	ret += fts_command(BUTTON_ON);
	fts_delay(50);
	fts_command(FORCECALIBRATION);
	fts_wait_forcecal_ready();
	
	ret += fts_interrupt_set(INT_ENABLE);
	#endif
	return ret;
}

static int fts_init(void *unused)
{
    	unsigned char val[16];
    	unsigned char regAdd[8];
    	int rc = 0;
	unsigned char regoneAdd;
	int ret = 0;
	unsigned char event_id = 0;
	unsigned char status_event = 0;
	unsigned char afe_version1 = 0;
	unsigned char afe_version2 = 1;
	int afe_count = 0;
	
	int i,j;
    	TPD_DMESG("fts_init called\n");

    	/* TS Chip ID */
	regAdd[0] = 0xB6;
    	regAdd[1] = 0x00;
	regAdd[2] = 0x07;
	rc = fts_read_reg(regAdd, 3, (unsigned char *)val, 8);
	TPD_DMESG("FTS %02X%02X%02X = %02x %02x %02x %02x %02x %02x %02x %02x\n",
		regAdd[0], regAdd[1], regAdd[2], 
		val[0], val[1], val[2], val[3], 
		val[4], val[5], val[6], val[7]);
	if ((val[1] != FTS_ID0) || (val[2] != FTS_ID1)) {
		return 1;
	}

	rc += fts_fw_upgrade();
	rc += fts_systemreset();
	
	rc += fts_interrupt_set(INT_DISABLE);
	
	/*mt65xx_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE);
	mt65xx_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
	mt65xx_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, CUST_EINT_POLARITY_LOW, st_i2c_tpd_eint_interrupt_handler, 1); 
	mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);*/
	
	//mt65xx_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_LEVEL_SENSITIVE); //level    
    	//mt65xx_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
    	mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, EINTF_TRIGGER_LOW, st_i2c_tpd_eint_interrupt_handler, 0);
    	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
   	rc += fts_interrupt_set(INT_ENABLE);

	rc += fts_command(SLEEPOUT);
	msleep(20);

	/*rc += fts_command(CX_TUNNING);
	fts_wait_Mutual_atune_ready();

	rc += fts_command(SELF_TUNING);
	fts_wait_Self_atune_ready();*/
	
	/* Calibration Info 1 */
	/*regAdd[0] = 0xB2;
    	regAdd[1] = 0x17;
	regAdd[2] = 0xFB;
	rc = fts_read_reg(regAdd, 3, &afe_version1, 1);
	
	regAdd[0] = 0xB2;
    	regAdd[1] = 0x07;
	regAdd[2] = 0xFB;
	rc = fts_read_reg(regAdd, 3, &afe_version2, 1);*/

	rc += fts_get_afe_version();
	TPD_DMESG("fts afe version 1,2 is %02x %02x \n",
		chip_afe0_version, chip_afe1_version);
	#ifdef MINI_MODE_TO_CTP
	if (chip_afe0_version != chip_afe1_version)
	{
	rc += fts_command(CX_TUNNING);
	msleep(100);
	rc += fts_command(SELF_TUNING);
	for (i = 0; i < 40; i++) {
		regoneAdd = READ_ONE_EVENT;
    		ret = fts_read_reg(&regoneAdd, 1, regAdd, FTS_EVENT_SIZE);
    		if (ret == 0) {    			
    			event_id = regAdd[0];
    			status_event = regAdd[1];
    			if (event_id == 0x16) {
    				if (status_event == FTS_STATUS_MUTUAL_TUNE) {
    					mtune_result = regAdd[2];
    					TPD_DMESG("fts mtune ok\n");
    					afe_count++;
    				}
    				if (status_event == FTS_STATUS_SELF_TUNE) {
    					stune_result = regAdd[2];
    					TPD_DMESG("fts atune ok\n");
    					afe_count++;
    				}
    			}    			
			fts_delay(50);
			TPD_DMESG("fts tune delay count %d\n",i);
			if (afe_count >= 2)
				break;
		}
		else {
			fts_delay(50);
			}		
	}
	if ((mtune_result == 0) && (stune_result == 0)) {
			TPD_DMESG("fts tune result ok\n");
			mtune_result = 2;
			stune_result = 2;
			rc += fts_command(TUNING_BACKUP2);
			fts_wait_tuning_backup_ready();
		}
	}
	#endif
	
	#ifdef MINI_MODE_TO_CTP
	ret = fts_check_autotune_result();
	if (ret == 0) {
		TPD_DMESG("fts autotune not needed\n");
	} else if (ret == 2) {
		TPD_DMESG("fts autotune data error\n");
		}
	else if (ret == 3) {
		TPD_DMESG("fts autotune was needed\n");
		fts_cleartune_flag();
		{
		rc += fts_command(CX_TUNNING);
		msleep(100);
		rc += fts_command(SELF_TUNING);
		for (i = 0; i < 40; i++) {
		regoneAdd = READ_ONE_EVENT;
    		ret = fts_read_reg(&regoneAdd, 1, regAdd, FTS_EVENT_SIZE);
    		if (ret == 0) {    			
    			event_id = regAdd[0];
    			status_event = regAdd[1];
    			if (event_id == 0x16) {
    				if (status_event == FTS_STATUS_MUTUAL_TUNE) {
    					mtune_result = regAdd[2];
    					TPD_DMESG("fts mtune ok\n");
    					afe_count++;
    				}
    				if (status_event == FTS_STATUS_SELF_TUNE) {
    					stune_result = regAdd[2];
    					TPD_DMESG("fts atune ok\n");
    					afe_count++;
    				}
    			}    			
			fts_delay(50);
			TPD_DMESG("fts tune delay count %d\n",i);
			if (afe_count >= 2)
				break;
		}
		else {
			fts_delay(50);
			}		
		}
		if ((mtune_result == 0) && (stune_result == 0)) {
			TPD_DMESG("fts tune result ok\n");
			mtune_result = 2;
			stune_result = 2;
			rc += fts_command(TUNING_BACKUP2);
			fts_wait_tuning_backup_ready();
		}
		}
	}
	#endif
	
	rc += fts_command(SENSEON);
	
	rc += fts_command(BUTTON_ON);
	rc += fts_command(FORCECALIBRATION);
	fts_wait_forcecal_ready();
	
	// rc += fts_command(FLUSHBUFFER);
	if (rc)
    		TPD_DMESG("fts initialized failed\n");
	else
		TPD_DMESG("fts initialized successful\n");
	
	fts_cleartune_flag();	
	return rc;
}

static int fts_init_flash_reload(void)
{
    	int rc = 0;

	rc += fts_command(SLEEPOUT);
	msleep(20);
	
	#if 0
	rc += fts_command(CX_TUNNING);
	fts_wait_Mutual_atune_ready();

	rc += fts_command(SELF_TUNING);
	fts_wait_Self_atune_ready();

	rc += fts_command(FORCECALIBRATION);
	fts_wait_forcecal_ready();
	
	rc += fts_command(TUNING_BACKUP2);
	fts_wait_tuning_backup_ready();
	
	rc += fts_command(SENSEON);
	
	rc += fts_command(BUTTON_ON);
	
	rc += fts_command(FLUSHBUFFER);
	// rc += fts_command(FLUSHBUFFER);
	if (rc)
    		TPD_DMESG(KERN_ERR "fts initialized failed\n");
	else
		TPD_DMESG(KERN_ERR "fts initialized successful\n");
	#endif
		
	return rc;	
}

void fts_reboot(void)
{
    TPD_DMESG("fts_reboot\n");
    #if 0
    hwPowerDown(MT65XX_POWER_LDO_VGP4, "TP");
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
    hwPowerOn(MT65XX_POWER_LDO_VGP4, VOL_3300, "TP");
    msleep(20);  //tce, min is 0, max is ?

    //set CE to HIGH
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
    msleep(50);  //tpor, min is 1, max is 5

    //set RSTB to HIGH 
    mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);
    msleep(50);//t boot_core, typicl is 20, max is 25ms
    msleep(300);
    #endif
}

static void fts_enter_pointer_event_handler(unsigned char *event)
{
	unsigned char touch_id;
	int x, y, z;

	//dev_dbg(info->dev, "Received event 0x%02x\n", event[0]);
	//TPD_DMESG("fts Received event 0x%02x\n", event[0]);
	touch_id = event[1] & 0x0F;
	//__set_bit(touchId, &info->touch_id);
	x = (event[2] << 4) | ((event[4] & 0xF0) >> 4);
	y = (event[3] << 4) | (event[4] & 0x0F);
	z = (event[5] & 0x3F);

	if (x == X_AXIS_MAX)
		x--;
	if (y == Y_AXIS_MAX)
		y--;

	//input_mt_slot(tpd->dev, touch_id);
	input_report_key(tpd->dev, BTN_TOUCH, 1);
	input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, touch_id);
	input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
	input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
	input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, z);
	input_report_abs(tpd->dev, ABS_MT_PRESSURE, z);

	/*TPD_DMESG("Event 0x%02x - ID[%d], (x, y, z) = (%3d, %3d, %3d)\n",
			 *event, touch_id, x, y, z);*/

	return;
}

static void fts_motion_pointer_event_handler(unsigned char *event)
{
	unsigned char touch_id;
	int x, y, z;

	//dev_dbg(info->dev, "Received event 0x%02x\n", event[0]);
	//TPD_DMESG("fts Received event 0x%02x\n", event[0]);
	touch_id = event[1] & 0x0F;
	//__set_bit(touchId, &info->touch_id);
	x = (event[2] << 4) | ((event[4] & 0xF0) >> 4);
	y = (event[3] << 4) | (event[4] & 0x0F);
	z = (event[5] & 0x3F);

	if (x == X_AXIS_MAX)
		x--;
	if (y == Y_AXIS_MAX)
		y--;

	//input_mt_slot(tpd->dev, touch_id);
	input_report_key(tpd->dev, BTN_TOUCH, 1);
	input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, touch_id);
	input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
	input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
	input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, z);
	input_report_abs(tpd->dev, ABS_MT_PRESSURE, z);

	/*TPD_DMESG("Event 0x%02x - ID[%d], (x, y, z) = (%3d, %3d, %3d)\n",
			 *event, touch_id, x, y, z);*/

	return;
}

static void fts_leave_pointer_event_handler(unsigned char *event)
{
	unsigned char touch_id;

	//dev_dbg(info->dev, "Received event 0x%02x\n", event[0]);
	//TPD_DMESG("fts Received event 0x%02x\n", event[0]);
	touch_id = event[1] & 0x0F;
	//__clear_bit(touchId, &info->touch_id);

	//input_mt_slot(tpd->dev, touch_id);
	input_report_key(tpd->dev, BTN_TOUCH, 0);

	//TPD_DMESG("Event 0x%02x - release ID[%d]\n", event[0], touch_id);
	return;
}

static void fts_unknown_event_handler(unsigned char *event)
{
    	TPD_DMESG("fts unknown event : %02X %02X %02X %02X %02X %02X %02X %02X\n",
           event[0], event[1], event[2], event[3],
           event[4], event[5], event[6], event[7]);
}

static void fts_debug_event_handler(unsigned char *event)
{
    	TPD_DMESG("fts debug event : %02X %02X %02X %02X %02X %02X %02X %02X\n",
           event[0], event[1], event[2], event[3],
           event[4], event[5], event[6], event[7]);
}

static void fts_error_event_handler(unsigned char *event)
{
    	TPD_DMESG("fts error event : %02X %02X %02X %02X %02X %02X %02X %02X\n",
           event[0], event[1], event[2], event[3],
           event[4], event[5], event[6], event[7]);
}

static void fts_print_event_handler(unsigned char *event)
{
    	TPD_DMESG("fts event : %02X %02X %02X %02X %02X %02X %02X %02X\n",
           event[0], event[1], event[2], event[3],
           event[4], event[5], event[6], event[7]);
}

static void fts_doubleclick_event_handler(unsigned char *event)
{
	unsigned char event_type = 0;
    	TPD_DMESG("fts doubleclick event : %02X %02X %02X %02X %02X %02X %02X %02X\n",
           event[0], event[1], event[2], event[3],
           event[4], event[5], event[6], event[7]);
	event_type =  event[1];          
	if (event_type == TYPE_DOUBLECLICK) {	
	//zijian	input_report_key(tpd->dev, KEY_DOUBLECLICKEVENT, 1);	
	 	//input_mt_sync(tpd->dev);	 
	 	{
         	//msleep(50);
         	doubleclick_counter++;
	 	printk("FTS doubleclick event down %d\n",doubleclick_counter);
	 	}
	}
}

static void fts_status_event_handler(unsigned char *event)
{
	unsigned char status_event = 0;
	TPD_DMESG("fts status event : %02X %02X %02X %02X %02X %02X %02X %02X\n",
           event[0], event[1], event[2], event[3],
           event[4], event[5], event[6], event[7]);
	//TPD_DMESG("fts status event : %02X %02X  %02X\n",event[1], event[2], event[3]);
	status_event = event[1];
	switch (status_event)
                {
                    case FTS_STATUS_MUTUAL_TUNE:
                        fts_mutualTune_ready = 1;
                        mtune_result = (event[3] << 8) | event[4] ;
                        break;

                    case FTS_STATUS_SELF_TUNE:
                        fts_selfTune_ready = 1;
                        stune_result = (event[3] << 8) | event[4];
                        break;

                    case FTS_FORCE_CAL_SELF_MUTUAL:
                        fts_forcecal_ready = 1;
                        break;

                    default:
                        TPD_DMESG("fts not handled event code\n");
                        break;
                }	
	return;
}

static void fts_button_status_event_handler(unsigned char *event)
{
	int i;
	unsigned char buttons, changed, touch_id;
	unsigned char key;
	bool on;
	
	TPD_DMESG("fts received event 0x%02x\n", event[0]);
	/* get current buttons status */
	/* buttons = event[1]; */
	/* mutual capacitance key */
	buttons = event[2];
	//TPD_DMESG("fts button old_button 0x%02x 0x%02x\n", buttons, old_buttons);
	/* check what is changed */
	changed = buttons ^ old_buttons;

	//dev_dbg(info->dev, "Received event 0x%02x\n", event[0]);
	//TPD_DMESG("fts Received event 0x%02x\n", event[0]);
	touch_id = event[1] & 0x0F;

	
	for (i = 0; i < 8; i++) {
		if (changed & (1 << i)) {
			
			key = (1 << i);
			on = ((buttons & key) >> i);
			//TPD_DMESG("fts key on/off 0x%02x 0x%02x\n", key, on);
			switch(key)
				{
        			case 1:
            				TPD_DMESG("ST_KEY_EVENT BACK, %d \n",on);
							if(on){
									input_report_key(tpd->dev, BTN_TOUCH, 1);
									input_report_abs(tpd->dev, ABS_MT_POSITION_X, 270);
									input_report_abs(tpd->dev, ABS_MT_POSITION_Y, 1980);
								}else{
									input_report_key(tpd->dev, BTN_TOUCH, 0);
								}
        				break;
        
        			case 2:
            				TPD_DMESG("ST_KEY_EVENT HOME, %d \n",on);
							if(on){
									input_report_key(tpd->dev, BTN_TOUCH, 1);
									input_report_abs(tpd->dev, ABS_MT_POSITION_X, 540);
									input_report_abs(tpd->dev, ABS_MT_POSITION_Y, 1980);
								}else{
									input_report_key(tpd->dev, BTN_TOUCH, 0);
								}

        				break;

        			case 4:
            				TPD_DMESG("ST_KEY_EVENT MENU, %d \n",on);
							if(on){
									input_report_key(tpd->dev, BTN_TOUCH, 1);
									input_report_abs(tpd->dev, ABS_MT_POSITION_X, 810);
									input_report_abs(tpd->dev, ABS_MT_POSITION_Y, 1980);
								}else{
									input_report_key(tpd->dev, BTN_TOUCH, 0);
								}

        				break;
        
        			default:
        				break;
				}
		}
			/*input_report_key(tpd->dev,
				BTN_0 + i,
				(!(info->buttons & (1 << i))));*/			
	}
	
	old_buttons = buttons;
	return;	
}

static int st_touch_event_handler(void *unused)
{
#if 1
	// u8 buf[TS_READ_REGS_LEN] = { 0 };
	int i, read_num, fingerID, Touch_Type = 0, touchState = 0;//, keyID = 0;
	int id = 0;	
	unsigned char data[FTS_EVENT_SIZE * FTS_FIFO_MAX];
    	int rc;
    	unsigned char regAdd;
    	int left_events = 0;
    	int new_left_events = 0;
    	int total_events = 1;
    	int read_error_flag = 0;
    	unsigned char event_id;
    	unsigned char tmp_data[8];
    	unsigned char firstleftevent = 0;
    	
	struct sched_param param = { .sched_priority = RTPM_PRIO_TPD }; 
	sched_setscheduler(current, SCHED_RR, &param);
	
	do {    
		//mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);	
        	set_current_state(TASK_INTERRUPTIBLE);		
        	wait_event_interruptible(st_waiter, st_tpd_flag != 0);		

        	set_current_state(TASK_RUNNING); 
        	
        	st_tpd_flag = 0;
        	read_error_flag = 0;
        	total_events = 1;
        	// memset(data, 0x0, FTS_EVENT_SIZE * FTS_FIFO_MAX);
    		regAdd = READ_ONE_EVENT;
    		rc = fts_read_reg(&regAdd, 1, (unsigned char *)data, FTS_EVENT_SIZE);
    		// rc = fts_dma_read_reg(&regAdd, 1, data, FTS_EVENT_SIZE);
    		if (rc == 0) {
    			left_events = data[7] & 0x1F;
    			
    			while ((left_events > 0) && (total_events < (FTS_FIFO_MAX-1))) {
    				{
				//memset(tmp_data, 0x0, FTS_EVENT_SIZE);
				if (left_events > READ_EVENT_SIZE) {				
					regAdd = READ_ALL_EVENT;
					rc = fts_dma_read_reg(&regAdd, sizeof(regAdd),
						&data[total_events*FTS_EVENT_SIZE],
						READ_EVENT_SIZE*FTS_EVENT_SIZE);
					total_events += READ_EVENT_SIZE;	
				} else {
					regAdd = READ_ALL_EVENT;
					rc = fts_dma_read_reg(&regAdd, sizeof(regAdd),
						&data[total_events*FTS_EVENT_SIZE],
						left_events*FTS_EVENT_SIZE);
					total_events += left_events;
				}
				data[7] &= 0xE0;
        			if (rc != 0) {
        				TPD_DMESG("fts read fifo error2\n");
        				read_error_flag = 1;
        			}			
        			left_events = data[7] & 0x1F;
				// memcpy(&data[total_events*FTS_EVENT_SIZE], tmp_data, FTS_EVENT_SIZE);
				}    			        
        			
        			#if 0
				for (i=0; i<left_events; i++) {
					memset(tmp_data, 0x0, FTS_EVENT_SIZE);
					regAdd = READ_ONE_EVENT;
        				rc = fts_read_reg(&regAdd, 1, (unsigned char *)tmp_data, FTS_EVENT_SIZE);
        				if (rc != 0) {
        					data[7] &= 0xE0;
        					TPD_DMESG("fts read fifo error2\n");
        					read_error_flag = 1;
        				}
					memcpy(&data[(i+1)*FTS_EVENT_SIZE], tmp_data, FTS_EVENT_SIZE);
				}
				#endif       			
        		} 
        		
    			// printk(KERN_ERR "fts total events = %d\n",total_events);
    			for (i=0; i<total_events; i++) {
    				// fts_print_event_handler(&data[i*FTS_EVENT_SIZE]);
    				event_id = data[i*FTS_EVENT_SIZE];
    				//event_ptr = &data[i*FTS_EVENT_SIZE];
    				switch(event_id)
    				{
    				case EVENTID_NO_EVENT:
                			break;	
    				case EVENTID_ENTER_POINTER:
					fts_enter_pointer_event_handler(&data[i*FTS_EVENT_SIZE]);
					input_mt_sync(tpd->dev);
					break;
    				case EVENTID_MOTION_POINTER:
					fts_motion_pointer_event_handler(&data[i*FTS_EVENT_SIZE]);
					input_mt_sync(tpd->dev);
    					break;
    				case EVENTID_LEAVE_POINTER:    					
    					fts_leave_pointer_event_handler(&data[i*FTS_EVENT_SIZE]);    					
					input_mt_sync(tpd->dev);
    					break;
    				case EVENTID_BUTTON_STATUS:
    					fts_button_status_event_handler(&data[i*FTS_EVENT_SIZE]);
						input_mt_sync(tpd->dev);
                			break;
                		case EVENTID_GESTURE:	
                			fts_doubleclick_event_handler(&data[i*FTS_EVENT_SIZE]);
                			break;
                		case EVENTID_STATUS:
                			fts_status_event_handler(&data[i*FTS_EVENT_SIZE]);
                			break;
                		case EVENTID_DEBUG:
                			fts_debug_event_handler(&data[i*FTS_EVENT_SIZE]);
                			break;	
                		case EVENTID_ERROR:
                			fts_error_event_handler(&data[i*FTS_EVENT_SIZE]);
                			break;
    				default:
    					fts_unknown_event_handler(&data[i*FTS_EVENT_SIZE]);
        				break;
    				}
    			}
    			
    			if ( (tpd != NULL) && (tpd->dev != NULL) )
            			input_sync(tpd->dev);
    		} else {
    			TPD_DMESG("fts read fifo error1\n");
    			read_error_flag = 1;    			
    		}
    		
    		if (read_error_flag == 1) {
    			TPD_DMESG("fts read fifo error\n");
    			fts_command(FLUSHBUFFER);
    		}

		//mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);	
	} while ( !kthread_should_stop() ); 

	return 0;
#endif
}

static void st_i2c_tpd_eint_interrupt_handler(void)
{ 
    //TPD_DMESG_PRINT_INT;
    mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
    st_tpd_flag=1;    
    interrupt_count++;
    //TPD_DMESG("fts,interrupt_count = %d \n",interrupt_count);
    wake_up_interruptible(&st_waiter);
} 

void fts_print_frame(short *pData)
{
	int i = 0;
	int j = 0;
	short value = 0;
	short min = 0x7FFF;
	short max = 0;
	short min_x = 0x7FFF;
	short min_y = 0x7FFF;
	short max_x = 0;
	short max_y = 0;
	short col_min = 0x7FFF;
	short col_max = 0;

	printk("        ");
	for(i = 0; i < SenseChannelLength; i++)
	{
		printk("Rx%02d  ", i);
	}
	printk("  Min  ");
	printk(" Max  ");
	printk("\n");

	printk("     +");
	for(i = 0; i < SenseChannelLength; i++)
	{
		printk("------");
	}
	printk("-+-------------");
	printk("\n");
	
	for(i = 0; i < ForceChannelLength; i++)
	{
		col_min = 0x7FFF;
		col_max = 0;
	
		printk("Tx%02d | ", i);

		for(j = 0; j < SenseChannelLength; j++)
		{
			value = pData[(i*SenseChannelLength) + j];
			printk("%5d ", value);

			if (value < min)
			{
				if( i > 0)
				{
					min = value;
					min_x = j;
					min_y = i;
				}
			}
			if (value > max)
			{
				if( i > 0)
				{
					max = value;
					max_x = j;
					max_y = i;
				}
			}

			if (value < col_min)
				col_min = value;
			if (value > col_max)
				col_max = value;
		}
		printk("| %5d %5d", col_min, col_max);
		printk("\n");
	}
	printk(" Min : %5d [ %02d , %02d ] \n", min, min_x , min_y );
	printk(" Max : %5d [ %02d , %02d ] \n", max, max_x , max_y );
	printk(" Dif : %5d \n", max - min);
}

static int fts_check_rawdata(short *pData)
{
	int i = 0;
	int j = 0;
	short value = 0;
	short min = 0x7FFF;
	short max = 0;
	short min_x = 0x7FFF;
	short min_y = 0x7FFF;
	short max_x = 0;
	short max_y = 0;
	short col_min = 0x7FFF;
	short col_max = 0;
	int ret = 0;
	short diff_rx = 0;
	short diff_tx = 0;
	short last_rx,this_rx,last_tx,this_tx;
	
	TPD_DMESG("fts check raw data beginnning\n");
	#if 0
	for(i = 0; i < ForceChannelLength; i++) {
		printk("fts ");
		for(j = 0; j < SenseChannelLength; j++) {			
			this_rx = pData[(i*SenseChannelLength) + j];
			printk("%5d ", this_rx);				
			}
			printk("\n");	
	}		
	#endif
	for(i = 0; i < ForceChannelLength; i++) {
		for(j = 0; j < SenseChannelLength-1; j++) {
			last_rx = pData[(i*SenseChannelLength) + j];
			this_rx = pData[(i*SenseChannelLength) + j+1];
			/*if ((i == ForceChannelLength-1)) {
			printk("fts rx| %5d %5d", last_rx, this_rx);
			printk("\n");
			}*/			
			if (last_rx > this_rx)
				diff_rx = last_rx - this_rx;
			else
				diff_rx = this_rx - last_rx ;	
			if (diff_rx > ST_TOUCH_DIFFMAX) {				
				ret = 3;
				TPD_DMESG("fts error rx data is %5d %5d %5d %5d\n", last_rx, this_rx, i, j);
				goto error_rawdata;
			}	
		}		
	}
	/* pass the first row */
	for(i = 0; i < SenseChannelLength; i++) {
		for(j = 1; j < ForceChannelLength-1; j++) {
			last_tx = pData[(j*SenseChannelLength) + i];
			this_tx = pData[((j+1)*SenseChannelLength) + i];
			/*if ( (i == SenseChannelLength-1)) {
			printk("fts tx | %5d %5d", last_tx, this_tx);
			printk("\n");
			}*/
			if (last_tx > this_tx)
				diff_tx = last_tx - this_tx;
			else
				diff_tx = this_tx - last_tx ;	
			if (diff_tx > ST_TOUCH_DIFFMAX) {				
				ret = 3;
				TPD_DMESG("fts error tx data is %5d %5d %5d %5d\n", last_tx, this_tx, i, j);
				goto error_rawdata;
			}					
		}		
	}
		
error_rawdata:
	return ret;
}

static int fts_checkkey_rawdata(short *pData)
{
	int i = 0;
	int ret = 0;
	short this_key;
	
	TPD_DMESG("fts check key raw data beginnning\n");
	
	for(i = 0; i < TPD_KEY_COUNT-1; i++) {
		this_key = pData[i];
		TPD_DMESG("fts key code is:%5d\n", this_key);
		if ((this_key > ST_KEY_RAWMAX) || (this_key < ST_KEY_RAWMIN)) {				
			ret = 4;
			TPD_DMESG("fts error key rx data is %5d\n", this_key);
		}		
	}
	return ret;
}

static int fts_read_rawdata_result(unsigned char type)
{
	unsigned char pChannelLength[8] = {0xB3, 0x00, 0x00, 0xB1, 0xF8, 0x14, 0x03, 0x00};
	unsigned char pFrameAddress[8] = {0xD0, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00};
	unsigned int FrameAddress = 0;
	unsigned int writeAddr = 0;
	unsigned int start_addr = 0;
	unsigned int end_addr = 0;
	unsigned int totalbytes = 0;
	unsigned int remained = 0;
	unsigned int readbytes = 0xFF;
	unsigned int dataposition = 0;
	unsigned char *pRead = NULL;
	int rc = 0;
	int ret = 0;
	int i = 0;
	int j = 0;
	int requestbytes = 0;
	unsigned char tempkey[6] = {0};
	
	pRead = kmalloc(4096, GFP_KERNEL);
	if (pRead == NULL) {
		TPD_DMESG("FTS Out of memory when programming flash\n");
		return -1;
	}
	
	fts_write_reg(&pChannelLength[0], 3);
	ret = fts_read_reg(&pChannelLength[3], 3, pRead, pChannelLength[6]);
	if(ret == 0)
	{
		SenseChannelLength = pRead[1];
		ForceChannelLength = pRead[2];
		totalbytes = SenseChannelLength * ForceChannelLength * 2;
	}
	else
	{
		TPD_DMESG("read failed rc = %d \n", ret);
		rc = 1;
		goto ErrorExit;
	}

	TPD_DMESG("SenseChannelLength = %X, ForceChannelLength = %X \n", SenseChannelLength, ForceChannelLength);

	pFrameAddress[2] = type;

	ret = fts_read_reg(&pFrameAddress[0], 3, pRead, pFrameAddress[3]);
	if(ret == 0)
	{
		FrameAddress = pRead[0] + (pRead[1] << 8);
		start_addr = 0xD0000000 + FrameAddress;
		end_addr = 0xD0000000 + FrameAddress + totalbytes;
	}
	else
	{
		TPD_DMESG("read failed rc = %d \n", ret);
		rc = 2;
		goto ErrorExit;
	}

	TPD_DMESG("FrameAddress = %X \n", start_addr);
	TPD_DMESG("start_addr = %X, end_addr = %X \n", start_addr, end_addr);

	remained = totalbytes;
	
	for(writeAddr = start_addr; writeAddr < end_addr; writeAddr += READ_CHUNK_SIZE)
	{		
		pFrameAddress[1] = (writeAddr >> 8) & 0xFF;
		pFrameAddress[2] = writeAddr & 0xFF;

		if(remained >= READ_CHUNK_SIZE)
		{
			readbytes = READ_CHUNK_SIZE;
		}
		else
		{
			readbytes = remained;
		}

		memset(pRead, 0x0, readbytes);
		TPD_DMESG("%02X%02X%02X readbytes=%d\n", pFrameAddress[0], pFrameAddress[1], pFrameAddress[2], readbytes);
		fts_dma_read_reg(&pFrameAddress[0], 3, pRead, readbytes);

		remained -= readbytes;

		for(i = 0; i < readbytes; i += 2)
		{
			pFrame[dataposition++] = pRead[i] + (pRead[i + 1] << 8);
		}
	}

	TPD_DMESG("writeAddr = %X, start_addr = %X, end_addr = %X \n", writeAddr, start_addr, end_addr);	
	TPD_DMESG("[Raw Touch : 0x%X] \n", start_addr);	
	fts_print_frame(pFrame);
	
	rc = fts_check_rawdata(pFrame);
	if (rc != 0) {
		TPD_DMESG("fts read tp raw data failed rc = %d \n", rc);
		goto ErrorExit;
	}
	
	#if 0
		pFrameAddress[1] = 0x28;//0x98;
		pFrameAddress[2] = 0xb4;//0xF2;
		ret = fts_read_reg(&pFrameAddress[0], 3, tempkey, 6);
		if(ret == 0) {
			rc = fts_checkkey_rawdata(tempkey);
		} else 	{
			TPD_DMESG("fts read key raw data failed rc = %d \n", ret);
			rc = 3;
			goto ErrorExit;
		}
	#endif
		
ErrorExit :
	kfree(pRead);
	return rc;
}

static struct fts_checkdata fts_ap_checkerror_result(void)
{
	unsigned char pChannelLength[8] = {0xB3, 0x00, 0x00, 0xB1, 0xF8, 0x14, 0x03, 0x00};
	unsigned char pFrameAddress[8] = {0xD0, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00};
	unsigned int FrameAddress = 0;
	unsigned int writeAddr = 0;
	unsigned int start_addr = 0;
	unsigned int end_addr = 0;
	unsigned int totalbytes = 0;

	int rc = 0;
	int ret = 0;
	int i = 0;
	int j = 0;
	int requestbytes = 0;
	int SenseChannelnum = 0;
	int ForceChannelnum = 0;
	int inode_address = 0;
	int inode_address_align = 0;
	struct fts_checkdata temp_cd;
	unsigned char pcheckerrorRead[8];
	
	for (i=0; i<5; i++) {
	temp_cd.node[i] = 0;
	}
	temp_cd.check_flag = 0;
	
	fts_write_reg(&pChannelLength[0], 3);
	ret = fts_read_reg(&pChannelLength[3], 3, pcheckerrorRead, pChannelLength[6]);
	if(ret == 0)
	{
		SenseChannelnum = pcheckerrorRead[1];
		ForceChannelnum = pcheckerrorRead[2];
		totalbytes = SenseChannelnum * ForceChannelnum * 2;
	}
	else
	{
		TPD_DMESG("read failed rc = %d \n", ret);
		rc = 1;
		temp_cd.check_flag = 1;
		goto ErrorExit;
	}

	pFrameAddress[2] = 0;
	if ((SenseChannelnum != 0x1d) || (ForceChannelnum != 0x11)) {
		TPD_DMESG("read channels failed rc = %d \n", ret);
		rc = 3;
		temp_cd.check_flag = 1;
		goto ErrorExit;
	}
	
	ret = fts_read_reg(&pFrameAddress[0], 3, pcheckerrorRead, pFrameAddress[3]);
	if(ret == 0)
	{
		FrameAddress = pcheckerrorRead[0] + (pcheckerrorRead[1] << 8);
		start_addr = 0xD0000000 + FrameAddress;
		end_addr = 0xD0000000 + FrameAddress + totalbytes;
	}
	else
	{
		TPD_DMESG("read failed rc = %d \n", ret);
		rc = 2;
		temp_cd.check_flag = 1;
		goto ErrorExit;
	}

	//TPD_DMESG("FrameAddress = %X \n", start_addr);
	//TPD_DMESG("start_addr = %X, end_addr = %X \n", start_addr, end_addr);

	memset(pcheckerrorRead, 0x0, FTS_EVENT_SIZE);
	inode_address = ((((4*SenseChannelnum)+4)*2) / FTS_EVENT_SIZE);
	inode_address_align = ((((4*SenseChannelnum)+4)*2) % FTS_EVENT_SIZE);
	writeAddr = start_addr + inode_address * FTS_EVENT_SIZE;
	pFrameAddress[1] = (writeAddr >> 8) & 0xFF;
	pFrameAddress[2] = writeAddr & 0xFF;
	fts_dma_read_reg(&pFrameAddress[0], 3, pcheckerrorRead, FTS_EVENT_SIZE);
	temp_cd.node[0] = pcheckerrorRead[inode_address_align] + (pcheckerrorRead[inode_address_align + 1] << 8);
	
	inode_address = ((((5*SenseChannelnum)+24)*2) / FTS_EVENT_SIZE);
	inode_address_align = ((((5*SenseChannelnum)+24)*2) % FTS_EVENT_SIZE);
	writeAddr = start_addr + inode_address * FTS_EVENT_SIZE;
	pFrameAddress[1] = (writeAddr >> 8) & 0xFF;
	pFrameAddress[2] = writeAddr & 0xFF;
	fts_dma_read_reg(&pFrameAddress[0], 3, pcheckerrorRead, FTS_EVENT_SIZE);
	temp_cd.node[1] = pcheckerrorRead[inode_address_align] + (pcheckerrorRead[inode_address_align + 1] << 8);
	
	inode_address = ((((11*SenseChannelnum)+12)*2) / FTS_EVENT_SIZE);
	inode_address_align = ((((11*SenseChannelnum)+12)*2) % FTS_EVENT_SIZE);
	writeAddr = start_addr + inode_address * FTS_EVENT_SIZE;
	pFrameAddress[1] = (writeAddr >> 8) & 0xFF;
	pFrameAddress[2] = writeAddr & 0xFF;
	fts_dma_read_reg(&pFrameAddress[0], 3, pcheckerrorRead, FTS_EVENT_SIZE);
	temp_cd.node[2] = pcheckerrorRead[inode_address_align] + (pcheckerrorRead[inode_address_align + 1] << 8);
	
	inode_address = ((((14*SenseChannelnum)+3)*2) / FTS_EVENT_SIZE);
	inode_address_align = ((((14*SenseChannelnum)+3)*2) % FTS_EVENT_SIZE);
	writeAddr = start_addr + inode_address * FTS_EVENT_SIZE;
	pFrameAddress[1] = (writeAddr >> 8) & 0xFF;
	pFrameAddress[2] = writeAddr & 0xFF;
	fts_dma_read_reg(&pFrameAddress[0], 3, pcheckerrorRead, FTS_EVENT_SIZE);
	temp_cd.node[3] = pcheckerrorRead[inode_address_align] + (pcheckerrorRead[inode_address_align + 1] << 8);
	
	inode_address = ((((15*SenseChannelnum)+23)*2) / FTS_EVENT_SIZE);
	inode_address_align = ((((15*SenseChannelnum)+23)*2) % FTS_EVENT_SIZE);
	writeAddr = start_addr + inode_address * FTS_EVENT_SIZE;
	pFrameAddress[1] = (writeAddr >> 8) & 0xFF;
	pFrameAddress[2] = writeAddr & 0xFF;
	fts_dma_read_reg(&pFrameAddress[0], 3, pcheckerrorRead, FTS_EVENT_SIZE);
	temp_cd.node[4] = pcheckerrorRead[inode_address_align] + (pcheckerrorRead[inode_address_align + 1] << 8);
			
ErrorExit :
	return temp_cd;
}

/*
0:success
2:read data error
3:autotune data error
*/
static int fts_check_autotune_result(void)
{	
	unsigned char pFrameAddress[4] = {0xB2, 0x00, 0x00, 0x04};
	unsigned char regAdd[8];	
	unsigned char *pRead = NULL;
	unsigned char *pRead_t1 = NULL;
	unsigned short writeAddr = 0;
	int ret = 0;
	int i = 0;
	int j = 0;	
	int m = 0;
	int n = 0;
	unsigned char diff_rx = 0;
	unsigned char diff_tx = 0;
	unsigned char last_rx,this_rx,last_tx,this_tx;
	unsigned char regoneAdd;
	u8 retry = 0;
	
	fts_interrupt_set(INT_DISABLE);
	fts_command(FLUSHBUFFER);
	
	pRead = kmalloc(4096, GFP_KERNEL);
	pRead_t1 = pRead;
	if (pRead == NULL) {
		TPD_DMESG("FTS Out of memory when autotune\n");
		return -1;
	}
	memset(p_tuneframe, 0, 1024);
	
	writeAddr = 0x1018;
	for (i=0; i<FORCECHANNELNUM_THEORY-1; i++) {
		for (j=0; j< ((SENSECHANNELNUM_THEORY * AT_PACKET_BITS)/(8*4)); j++) {
			pFrameAddress[1] = (writeAddr >> 8) & 0xFF;
			pFrameAddress[2] = writeAddr & 0xFF;
			fts_write_reg(&pFrameAddress[0], 4);
			for (retry = 0; retry < AT_READRATRY_COUNT; retry++) {
    				regoneAdd = READ_ONE_EVENT;
    				ret = fts_read_reg(&regoneAdd, 1, regAdd, FTS_EVENT_SIZE);
    				if ((regAdd[0] == 0x12) && ((regAdd[1] == 0x10) || (regAdd[1] == 0x11)))
    					break;
				mdelay(2);
				TPD_DMESG("Line %d, fts auto read error, retry = %d\n", __LINE__, retry);
			}
			if (retry == AT_READRATRY_COUNT) {
				ret = 2;
				goto ErrorATExit;
			}
    			memcpy(pRead, &regAdd[3], 4);
    			writeAddr += 4;
    			pRead += 4;
		}
	}
    	
    	pRead = pRead_t1;
    	for (i=0; i<FORCECHANNELNUM_THEORY-1; i++) {
    		for (j=0; j< SENSECHANNELNUM_THEORY; j+=4) {
    			p_tuneframe[m] = (pRead[n] & 0x3f);
    			p_tuneframe[m+1] = (((pRead[n] & 0xc0)>>6) | ((pRead[n+1] & 0x0f)<< 2));
    			p_tuneframe[m+2] =  (((pRead[n+1] & 0xf0)>>4) | ((pRead[n+2]&0x03)<<4));
    			p_tuneframe[m+3] = ((pRead[n+2]&0xfc) >> 2);
    			m += 4;
    			n += 3;
    		}
    	}
    	#if 1
    	TPD_DMESG("fts read autotune raw data beginnning\n");
    	for (i=0; i<FORCECHANNELNUM_THEORY-1; i++) {
    		for (j=0; j< ((SENSECHANNELNUM_THEORY * AT_PACKET_BITS)/8); j++) {
    			printk("%3x ", pRead[j]);
    		}
    		printk("\n");
    	}
    	TPD_DMESG("fts read autotune raw data ended\n");
    	#endif
    	
    	TPD_DMESG("fts read autotune data beginnning\n");
    	for(i = 0; i < FORCECHANNELNUM_THEORY-1; i++) {
		printk("fts%d ", i);
		for(j = 0; j < SENSECHANNELNUM_THEORY; j++) {			
			this_rx = p_tuneframe[(i*SENSECHANNELNUM_THEORY) + j];
			printk("%3d ", this_rx);				
			}
			printk("\n");	
	}
	TPD_DMESG("fts read autotune data ended\n");

    	/* pass the last three column */
	for(i = 0; i < FORCECHANNELNUM_THEORY-1; i++) {
		for(j = 0; j < SENSECHANNELNUM_THEORY-4; j++) {
			last_rx = p_tuneframe[(i*SENSECHANNELNUM_THEORY) + j];
			this_rx = p_tuneframe[(i*SENSECHANNELNUM_THEORY) + j+1];
			if ((i == FORCECHANNELNUM_THEORY-2)) {	
			printk("fts rx| %5d %5d", last_rx, this_rx);
			printk("\n");
			}
			if (j == 13) {
				TPD_DMESG("fts autotune pass rx data is %5d %5d %5d %5d\n", last_rx, this_rx, i, j);
				}
			else {			
				if (last_rx > this_rx)
					diff_rx = last_rx - this_rx;
				else
					diff_rx = this_rx - last_rx ;	
				if (diff_rx > AT_DIFFMAX) {				
					ret = 3;
					TPD_DMESG("fts autotune error rx data is %5d %5d %5d %5d\n", last_rx, this_rx, i, j);
				goto ErrorATExit;
				}
			}	
		}	
	}
	/* pass the last three column and the first row */
	for(i = 0; i < SENSECHANNELNUM_THEORY-3; i++) {
		for(j = 0; j < FORCECHANNELNUM_THEORY-2; j++) {
			last_tx = p_tuneframe[(j*SENSECHANNELNUM_THEORY) + i];
			this_tx = p_tuneframe[((j+1)*SENSECHANNELNUM_THEORY) + i];
			if ( (i == SENSECHANNELNUM_THEORY-4)) {
			printk("fts tx | %5d %5d", last_tx, this_tx);
			printk("\n");
			}
			if ((i == AT_TCL_SENSEERROR) && (j == AT_TCL_FORCEERROR)) 
				continue;
			else {
				if (last_tx > this_tx)
					diff_tx = last_tx - this_tx;
				else
					diff_tx = this_tx - last_tx ;	
				if (diff_tx > ST_TOUCH_DIFFMAX) {				
					ret = 3;
					TPD_DMESG("fts autotune error tx data is %5d %5d %5d %5d\n", last_tx, this_tx, i, j);
				goto ErrorATExit;
				}
			}					
		}		
	}
	

ErrorATExit :
	pRead = pRead_t1;
	kfree(pRead);
	fts_interrupt_set(INT_ENABLE);	
	return ret;
}

/*
0:success
2:read data error
3:autotune data error
*/
static int fts_check_autotune_fm1_result(void)
{	
	unsigned char pFrameAddress[4] = {0xB2, 0x00, 0x00, 0x04};
	unsigned char regAdd[8];	
	unsigned char *pRead = NULL;
	unsigned char *pRead_t1 = NULL;
	unsigned short writeAddr = 0;
	int ret = 0;
	int i = 0;
	int j = 0;	
	int m = 0;
	int n = 0;
	unsigned char diff_rx = 0;
	unsigned char diff_tx = 0;
	unsigned char last_rx,this_rx,last_tx,this_tx;
	unsigned char regoneAdd;
	u8 retry = 0;
	
	fts_interrupt_set(INT_DISABLE);
	fts_command(FLUSHBUFFER);
	
	pRead = kmalloc(4096, GFP_KERNEL);
	pRead_t1 = pRead;
	if (pRead == NULL) {
		TPD_DMESG("FTS Out of memory when autotune\n");
		return -1;
	}
	memset(p_tuneframe, 0, 1024);
	
	writeAddr = 0x12B9;
	for (i=0; i<((FORCECHANNELNUM_THEORY-1)/4 +1); i++) {	
			pFrameAddress[1] = (writeAddr >> 8) & 0xFF;
			pFrameAddress[2] = writeAddr & 0xFF;
			fts_write_reg(&pFrameAddress[0], 4);
			for (retry = 0; retry < AT_READRATRY_COUNT; retry++) {
    				regoneAdd = READ_ONE_EVENT;
    				ret = fts_read_reg(&regoneAdd, 1, regAdd, FTS_EVENT_SIZE);
    				if ((regAdd[0] == 0x12) && (regAdd[1] == 0x12))
    					break;
				mdelay(2);
				TPD_DMESG("Line %d, fts auto read fm1 error, retry = %d\n", __LINE__, retry);
			}
			if (retry == AT_READRATRY_COUNT) {
				ret = 2;
				goto ErrorATExit;
			}
    			memcpy(pRead, &regAdd[3], 4);
    			writeAddr += 4;
    			pRead += 4;
	}
	
	writeAddr = 0x12CC;
	for (i=0; i< ((SENSECHANNELNUM_THEORY-3)/4 + 1); i++) {	
			pFrameAddress[1] = (writeAddr >> 8) & 0xFF;
			pFrameAddress[2] = writeAddr & 0xFF;
			fts_write_reg(&pFrameAddress[0], 4);
			for (retry = 0; retry < AT_READRATRY_COUNT; retry++) {
    				regoneAdd = READ_ONE_EVENT;
    				ret = fts_read_reg(&regoneAdd, 1, regAdd, FTS_EVENT_SIZE);
    				if ((regAdd[0] == 0x12) && (regAdd[1] == 0x12))
    					break;
				mdelay(2);
				TPD_DMESG("Line %d, fts auto read fm1 error, retry = %d\n", __LINE__, retry);
			}
			if (retry == AT_READRATRY_COUNT) {
				ret = 2;
				goto ErrorATExit;
			}
    			memcpy(pRead, &regAdd[3], 4);
    			writeAddr += 4;
    			pRead += 4;
	}
	/*autotune key value*/
	writeAddr = 0x1015;
	{	
			pFrameAddress[1] = (writeAddr >> 8) & 0xFF;
			pFrameAddress[2] = writeAddr & 0xFF;
			fts_write_reg(&pFrameAddress[0], 4);
			for (retry = 0; retry < AT_READRATRY_COUNT; retry++) {
    				regoneAdd = READ_ONE_EVENT;
    				ret = fts_read_reg(&regoneAdd, 1, regAdd, FTS_EVENT_SIZE);
    				if ((regAdd[0] == 0x12) && (regAdd[1] == 0x10))
    					break;
				mdelay(2);
				TPD_DMESG("Line %d, fts auto read fm1 error, retry = %d\n", __LINE__, retry);
			}
			if (retry == AT_READRATRY_COUNT) {
				ret = 2;
				goto ErrorATExit;
			}
    			memcpy(pRead, &regAdd[3], 4);    			
	}
	
	pRead = pRead_t1;
	for (i=0; i<FORCECHANNELNUM_THEORY-1; i++) {
		p_tuneframe[m] = pRead[n];
		m++;
		n++;
	}
	//n = ((FORCECHANNELNUM_THEORY-1)/4 +1)*4;
	if ((n%4) != 0)
		n = ((n/4)+1)*4;
		
	for (i=0; i<SENSECHANNELNUM_THEORY-3; i++) {
		p_tuneframe[m] = pRead[n];
		m++;
		n++;
	}
	if ((n%4) != 0)
		n = ((n/4)+1)*4;
	  
    	p_tuneframe[m] = (((pRead[n] & 0xc0)>>6) | ((pRead[n+1] & 0x0f)<< 2));
    	p_tuneframe[m+1] =  (((pRead[n+1] & 0xf0)>>4) | ((pRead[n+2]&0x03)<<4));
    	p_tuneframe[m+2] = ((pRead[n+2]&0xfc) >> 2);

    	#if 1
    	TPD_DMESG("fts read AT fm1 data beginnning\n");
    	for (i=0; i<n+3; i++) {
    		printk("%3x ", pRead[i]); 
    		if ( (i%8) == 7)
    			printk("\n");
    	}
    	TPD_DMESG("fts read AT fm2 data beginnning\n");
    	for (i=0; i<FORCECHANNELNUM_THEORY-1+SENSECHANNELNUM_THEORY-3+3; i++) {
    		printk("%3x ", p_tuneframe[i]); 
    		if ( (i%8) == 7)
    			printk("\n");
    	}
    	TPD_DMESG("fts read autotune raw data ended\n");
    	#endif
    	
    	for (i=0; i<FORCECHANNELNUM_THEORY-1; i++) {
		last_rx = p_tuneframe[i];
		this_rx = goldenvalue_tx[i];
		TPD_DMESG("AT txdata is %4d %4d %2d \n", last_rx, this_rx, i);
		if (last_rx > this_rx)
			diff_rx = last_rx - this_rx;
		else
			diff_rx = this_rx - last_rx ;	
		if (diff_rx > AT_SELFDIFFMAX) {				
			ret = 3;
			TPD_DMESG("fts AT error tx data is %4d %4d %2d \n", last_rx, this_rx, i);
		goto ErrorATExit;
		}
	}
	for (i=0; i<SENSECHANNELNUM_THEORY-3; i++) {
		last_rx = p_tuneframe[i+(FORCECHANNELNUM_THEORY-1)];
		this_rx = goldenvalue_rx[i];
		TPD_DMESG("AT rxdata is %4d %4d %2d \n", last_rx, this_rx, i);
		if (last_rx > this_rx)
			diff_rx = last_rx - this_rx;
		else
			diff_rx = this_rx - last_rx ;
		if (diff_rx > AT_SELFDIFFMAX) {				
			ret = 3;
			TPD_DMESG("fts AT error rx data is %4d %4d %2d \n", last_rx, this_rx, i);
		goto ErrorATExit;
		}
	}
	for (i=0; i<ST_KEYNUMBER; i++) {
		last_rx = p_tuneframe[i+ (FORCECHANNELNUM_THEORY-1)+ (SENSECHANNELNUM_THEORY-3)];
		this_rx = goldenvalue_key[i];
		if (last_rx > this_rx)
			diff_rx = last_rx - this_rx;
		else
			diff_rx = this_rx - last_rx ;	
		if (diff_rx > AT_KEYDIFFMAX) {				
			ret = 3;
			TPD_DMESG("fts AT error key data is %4d %4d %2d \n", last_rx, this_rx, i);
		goto ErrorATExit;
		}
	}	

ErrorATExit :
	pRead = pRead_t1;
	kfree(pRead);
	fts_interrupt_set(INT_ENABLE);	
	return ret;
}

static int fts_check_openshort_status(void)
{
	int ret = 0;
	unsigned char data[FTS_EVENT_SIZE];
	unsigned char regoneAdd;
	//unsigned char regAdd[3] = {0xB2, 0x00, 0x01, 0x02};
	int retry = 0;
	unsigned char event_id = 0;
	unsigned char ito_flag = 0;
	
	ito_check_status = 1;
	#if 1
	ret += fts_systemreset();
	fts_delay(200);
	ret += fts_command(SLEEPOUT);
	fts_delay(50);
	ret += fts_interrupt_set(INT_DISABLE);
	fts_delay(50);
	ret += fts_command(FLUSHBUFFER);
	fts_delay(50);
	ret += fts_command(ITO_CHECK);
	TPD_DMESG("FTS fts ito check\n");
	fts_delay(200);
	for (retry = 0; retry < 10; retry++) {
		regoneAdd = READ_ONE_EVENT;
    		ret = fts_read_reg(&regoneAdd, 1, data, FTS_EVENT_SIZE);
    		if (ret == 0) {
    			TPD_DMESG("FTS fts ito event: %02X %02X %02X %02X %02X %02X %02X %02X\n",
           			data[0], data[1], data[2], data[3],
           			data[4], data[5], data[6], data[7]);
    			//TPD_DMESG("FTS fts ito event: %02X %02X %02X %02X\n",data[0],data[1],data[2], data[3]);
    			
    			event_id = data[0];
    			ito_flag = data[1];
    			TPD_DMESG("FTS event: %04X %04X\n",event_id,ito_flag);
    			if ((event_id == 0x0f) && (ito_flag == 0x05)) {
    				/*if (ito_flag == 0x05) {    				
    				}*/
    				TPD_DMESG("FTS event: %04X %04X\n",event_id,ito_flag);
    				ito_check_status = (data[3] << 8) | data[4];
    				break;
    			}
    			else {
    				fts_delay(200);
    				ito_check_status = 1;
				TPD_DMESG("FTS fts tune delay count %d\n",retry);
			}
    		}
    		else {
    			ito_check_status = 1;
			fts_delay(200);
		}
	}
	
		
	TPD_DMESG("FTS fts check openshort_status is %x.\n",ito_check_status);

	ret += fts_systemreset();
	fts_delay(200);
	/* wake-up the device */
	ret += fts_command(SLEEPOUT);
	fts_delay(50);
	/* enable sense */
	ret += fts_command(SENSEON);
	fts_delay(50);	
	ret += fts_command(BUTTON_ON);
	fts_delay(50);
	fts_command(FORCECALIBRATION);
	fts_wait_forcecal_ready();
	
	ret += fts_interrupt_set(INT_ENABLE);
	#endif
	return ret;
}


static int tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{             
     	int err = 0;
     	// u8 chip_info = 0;
     	int ret = 0;
     	int try_count = 0;     
	struct task_struct *thread = NULL;
	struct task_struct *fw_thread = NULL;
	unsigned char val[5];
	unsigned char regAdd[3];
	int error;
	
	TPD_DMESG("[st_tpd] %s\n", __func__);	

	//TP sysfs debug support
	//input_mt_init_slots(tpd->dev, TOUCH_ID_MAX);
    	tpswitch_sysfs_init();
    	old_buttons = 0;
    	
    	//TP DMA support
    	if(DMAbuffer_va == NULL)
        DMAbuffer_va = (u8 *)dma_alloc_coherent(NULL, 4096,&DMAbuffer_pa, GFP_KERNEL);
    
    	TPD_DMESG("dma_alloc_coherent va = 0x%p, pa = 0x%8x \n",DMAbuffer_va,DMAbuffer_pa);
   	if (!DMAbuffer_va) {
        	TPD_DMESG("Allocate DMA I2C Buffer failed!\n");
        	goto ERROR_TP_PROBE1;
    	}
    	p_tuneframe = kmalloc(AT_FRAME_SIZE, GFP_KERNEL);
    	if (!p_tuneframe) {
        	TPD_DMESG("Allocate Autotune frame buffer failed!\n");
        	goto ERROR_TP_PROBE2;
    	}
		
reset_proc:
    
    	st_i2c_client = client;
#ifdef TPD_POWER_SOURCE_1800
    	hwPowerDown(TPD_POWER_SOURCE_1800,"TP-TWO");
#endif
#ifdef TPD_POWER_SOURCE_CUSTOM
		hwPowerDown(TPD_POWER_SOURCE_CUSTOM,"CTP");
#else    	
    	hwPowerDown(MT6323_POWER_LDO_VGP1,"CTP");
#endif
    	mdelay(10);

#ifdef TPD_POWER_SOURCE_CUSTOM
	hwPowerOn(TPD_POWER_SOURCE_CUSTOM,VOL_3300,"CTP");
#else
    	hwPowerOn(MT6323_POWER_LDO_VGP1,VOL_3300,"CTP");
#endif
#ifdef TPD_POWER_SOURCE_1800    	
    	hwPowerOn(TPD_POWER_SOURCE_1800,VOL_1800,"TP-TWO");
#endif
	msleep(100);
	
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
    	msleep(100);
    	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
    	msleep(20);
    	
    	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);
			
	fw_thread = kthread_run(fts_init, 0, "fw_update");
	if (IS_ERR(fw_thread))
	{ 
	err = PTR_ERR(fw_thread);
	TPD_DMESG(TPD_DEVICE "[melfas_tpd] failed to create kernel thread: %d\n", err);
	} 

    	/*
	if (melfas_firmware_update(client) < 0)
	{
	//if firmware update failed, reset IC
        //goto reset_proc;
    	}*/

	/*
	mt65xx_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_LEVEL_SENSITIVE); //level    
    	mt65xx_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
    	mt65xx_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, CUST_EINT_TOUCH_PANEL_POLARITY, st_i2c_tpd_eint_interrupt_handler, 0); 
    	mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	*/
    	thread = kthread_run(st_touch_event_handler, 0, TPD_DEVICE);

    	if (IS_ERR(thread))
    	{ 
        err = PTR_ERR(thread);
        TPD_DMESG(TPD_DEVICE "[melfas_tpd] failed to create kernel thread: %d\n", err);
    	}    
    	mdelay(5);
    		
			TPD_DMESG(KERN_ERR "tpd_i2c_probe success \n");
	
    	#if 0
	/*try 3 times handshake*/
	do {
		try_count++;	
		i_ret = melfas_i2c_read(st_i2c_client, MMS_CHIP_INFO, 1, &chip_info);
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
	#endif
    	tpd_load_status = 1;
    	return 0;
    	
ERROR_TP_PROBE3:	
ERROR_TP_PROBE2:
	if(DMAbuffer_va) {
		dma_free_coherent(NULL, 4096, DMAbuffer_va, DMAbuffer_pa);
		DMAbuffer_va = NULL;
		DMAbuffer_pa = 0;
	}
ERROR_TP_PROBE1:
	//input_mt_destroy_slots(tpd->dev);
	tpswitch_sysfs_exit();	
    	return -1;    	
}

static int tpd_i2c_remove(struct i2c_client *client)
{
	TPD_DMESG("[ST_tpd] %s\n", __func__);
	if(DMAbuffer_va) {
		dma_free_coherent(NULL, 4096, DMAbuffer_va, DMAbuffer_pa);
		DMAbuffer_va = NULL;
		DMAbuffer_pa = 0;
	}
	if (p_tuneframe)
		kfree(p_tuneframe);
	input_mt_destroy_slots(tpd->dev);
	tpswitch_sysfs_exit();	
	//TP_sysfs_exit();
	return 0;
}

static int tpd_i2c_detect(struct i2c_client *client, 
			struct i2c_board_info *info)
{
	TPD_DMESG("[ST_tpd] %s\n", __func__);
	strcpy(info->type, "mtk-tpd");
	return 0;
}
 
static int tpd_local_init(void)
{
 
	TPD_DMESG("STtech FTS I2C Touchscreen Driver (Built %s @ %s)\n", __DATE__, __TIME__);
	if(i2c_add_driver(&tpd_i2c_driver)!=0)
   	{
  		TPD_DMESG("fts unable to add i2c driver.\n");
      	return -1;
    	}
    	if(tpd_load_status == 0) 
    	{
    	TPD_DMESG("fts add error touch panel driver.\n");
    	i2c_del_driver(&tpd_i2c_driver);
    	return -1;
    	}    	
//zijian 	set_bit(KEY_DOUBLECLICKEVENT, tpd->dev->keybit);
#ifdef TPD_HAVE_BUTTON     
    	tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);// initialize tpd button data
#endif    
	TPD_DMESG("end %s, %d\n", __FUNCTION__, __LINE__);  
	tpd_type_cap = 1;
	
//set tp resulution 
	TPD_RES_X = X_AXIS_MAX ;
	TPD_RES_Y = Y_AXIS_MAX;
	
	TPD_DMESG("tpd_local_init, change TPD_RES_X to %d,TPD_RES_Y to %d \n",TPD_RES_X,TPD_RES_Y );	
	return 0; 
}

static void tpd_resume( struct early_suspend *h )
{
  int rc = 0;
	TPD_DMESG("[st_tpd] %s\n", __FUNCTION__);
	/*power up touch*/	
#ifdef TPD_POWER_SOURCE_CUSTOM
	hwPowerOn(TPD_POWER_SOURCE_CUSTOM,VOL_3300,"CTP");
#else
    	hwPowerOn(MT6323_POWER_LDO_VGP1,VOL_3300,"CTP");
#endif
#ifdef TPD_POWER_SOURCE_1800    	
    	hwPowerOn(TPD_POWER_SOURCE_1800,VOL_1800,"TP-TWO");
#endif
	msleep(100);

	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
	msleep(100);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(20);

	/* enable interrupts */
	fts_interrupt_set(INT_ENABLE);

	/* wake-up the device */
	fts_command(SLEEPOUT);
	fts_wait_sleepout_ready();
	/* enable sense */
	fts_command(SENSEON);
	
	fts_command(BUTTON_ON);
	
	fts_command(FORCECALIBRATION);
	fts_wait_forcecal_ready();
	
	/* put back the device in the original mode (see fts_suspend()) */
	switch (modebyte) {
	case MODE_PROXIMITY:
		fts_command(PROXIMITY_ON);
		break;

	case MODE_HOVER:
		fts_command(HOVER_ON);
		break;

	/*case MODE_GESTURE:
		fts_command(GESTURE_ON);
		break;*/

	case MODE_HOVER_N_PROXIMITY:
		fts_command(HOVER_ON);
		fts_command(PROXIMITY_ON);
		break;

	case MODE_GESTURE_N_PROXIMITY:
		//fts_command(GESTURE_ON);
		fts_command(PROXIMITY_ON);
		break;

	case MODE_GESTURE_N_PROXIMITY_N_HOVER:
		fts_command(HOVER_ON);
		//fts_command(GESTURE_ON);
		fts_command(PROXIMITY_ON);
		break;

	default:
		TPD_DMESG("fts Invalid device mode - 0x%02x\n",
				modebyte);
		break;
	}
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
}

static void tpd_suspend( struct early_suspend *h )
{
 
	TPD_DMESG("[st_tpd] %s\n", __FUNCTION__);
	
	old_buttons = 0;		
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);

    	/* Read out mode byte */
    	modebyte = fts_read_modebyte();
		TPD_DMESG("fts device mode - 0x%02x\n",
				modebyte);

    	fts_command(SLEEPIN);
    	fts_command(FLUSHBUFFER);	

    	fts_interrupt_set(INT_DISABLE);
    	st_ts_release_all_finger();    

	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);

#ifdef TPD_POWER_SOURCE_CUSTOM
		hwPowerDown(TPD_POWER_SOURCE_CUSTOM,"CTP");
#else    	
    	hwPowerDown(MT6323_POWER_LDO_VGP1,"CTP");
#endif
#ifdef TPD_POWER_SOURCE_1800
    	hwPowerDown(TPD_POWER_SOURCE_1800,"TP-TWO");
#endif

}

static struct tpd_driver_t tpd_device_driver = {
		 .tpd_device_name = "ST_FTS",
		 .tpd_local_init = tpd_local_init,
		 .suspend = tpd_suspend,
		 .resume = tpd_resume,
#ifdef TPD_HAVE_BUTTON
		 .tpd_have_button = 1,
#else
		 .tpd_have_button = 0,
#endif		
};
 
/* called when loaded into kernel */
static int __init tpd_driver_init(void) {
	printk("MediaTek st_fts touch panel driver init\n");
	i2c_register_board_info(TPD_I2C_NUMBER, &st_fts_i2c_tpd, 1);
	if(tpd_driver_add(&tpd_device_driver) < 0)
		 TPD_DMESG("add st_fts driver failed\n");
	 return 0;
}
 
/* should never be called */
static void __exit tpd_driver_exit(void) {
	TPD_DMESG("MediaTek st_fts touch panel driver exit\n");
	//input_unregister_device(tpd->dev);
	tpd_driver_remove(&tpd_device_driver);
}

module_init(tpd_driver_init);
module_exit(tpd_driver_exit);
