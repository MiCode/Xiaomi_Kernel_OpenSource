/*
SiI8348 Linux Driver

Copyright (C) 2013 Silicon Image, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation version 2.
This program is distributed AS-IS WITHOUT ANY WARRANTY of any
kind, whether express or implied; INCLUDING without the implied warranty
of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.  See 
the GNU General Public License for more details at http://www.gnu.org/licenses/gpl-2.0.html.             
*/
#include <linux/init.h>
//#include <linux/string.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>

#include <linux/semaphore.h>
#include <linux/mutex.h>
/*#include <mach/mt_gpio.h>*/

#include "sii_hal.h"
#include "si_fw_macros.h"
#include "si_mhl_defs.h"
#include "si_infoframe.h"
#include "si_edid.h"
#include "si_mhl2_edid_3d_api.h"
#include "si_mhl_tx_hw_drv_api.h"
#ifdef MEDIA_DATA_TUNNEL_SUPPORT
#include <linux/input.h>
#include "si_mdt_inputdev.h"
#endif
#include "mhl_linux_tx.h"
#include "platform.h"
#include "si_8348_drv.h"
#include "si_8348_regs.h"
#include "si_timing_defs.h"

#include <mach/irqs.h>
/*#include "mach/eint.h"*/

#ifdef CONFIG_MTK_LEGACY
/*#include <cust_eint.h>*/
#include <linux/gpio.h>
#include <mt-plat/mt_gpio.h>
#include <cust_gpio_usage.h>
#include <mach/mt_pm_ldo.h>
#include "hdmi_cust.h"
#include <pmic_drv.h>
#endif

///#include <pmic_drv.h>

/*#include "hdmi_cust.h"*/

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/regulator/consumer.h>
#endif

/* GPIOs assigned to control various starter kit signals */
#ifdef CONFIG_MTK_LEGACY

#ifdef CUST_EINT_MHL_NUM
#define GPIO_MHL_INT		CUST_EINT_MHL_NUM		// BeagleBoard pin ID for TX interrupt		// 135 is pin 'SDMMC2_DAT3', which is pin 11 of EXP_HDR on BeagleBoard
#else
#define GPIO_MHL_INT 0
#endif

#endif

#ifdef GPIO_MHL_RST_B_PIN
#define GPIO_MHL_RESET		GPIO_MHL_RST_B_PIN		// BeagleBoard pin ID for TX reset		// 139 is pin 'SDMMC2_DAT7', which is pin 03 of EXP_HDR on BeagleBoard
#else
#define GPIO_MHL_RESET 0
#endif

/**
* LOG For HDMI Driver
*/
#define MHL_LOG(fmt, arg...)  \
	do { \
		if (hdmi_log_on) pr_err("[HDMI_Platform]%s,%d ", __func__, __LINE__); pr_debug(fmt, ##arg); \
	}while (0)

#define MHL_FUNC()    \
	do { \
		if(hdmi_log_on) pr_err("[HDMI_Platform] %s\n", __func__); \
	}while (0)

#define MHL_DBG(fmt, arg...) \
	do { \
	pr_err("[EXTD][MHL]"fmt, ##arg); \
	}while (0)

#define MHL_WARN(fmt, arg...) \
	do { \
		pr_debug("[hdmi-platform]"fmt, ##arg); \
	}while (0)

static struct i2c_adapter	*i2c_bus_adapter = NULL;

struct i2c_dev_info {
	uint8_t			dev_addr;
	struct i2c_client	*client;
};

#define I2C_DEV_INFO(addr) \
	{.dev_addr = addr >> 1, .client = NULL}

// I2C Page config
/*
static struct i2c_dev_info device_addresses[] = {
	I2C_DEV_INFO(TX_PAGE_L0),
	I2C_DEV_INFO(TX_PAGE_L1),
	I2C_DEV_INFO(TX_PAGE_3),
	I2C_DEV_INFO(TX_PAGE_TPI),
	I2C_DEV_INFO(TX_PAGE_CBUS),
	I2C_DEV_INFO(TX_PAGE_DDC_EDID)
};
*/

extern int I2S_Enable;
int	debug_msgs	= 1;	// print all msgs, default should be '0'
//int	debug_msgs	= 3;	// print all msgs, default should be '0'

static bool reset_on_exit = 0; // request to reset hw before unloading driver

module_param(debug_msgs, int, S_IRUGO);
module_param(reset_on_exit, bool, S_IRUGO);

#define USE_DEFAULT_I2C_CODE 0
extern struct mhl_dev_context *si_dev_context;

static int mhl_i2c_status = 0;

static struct mutex mhl_lock;
int mhl_mutex_init(struct mutex *m)
{
	mutex_init(m);
	return 0;
}
int mhl_sw_mutex_lock(struct mutex*m)
{
	mutex_lock(m);
	return 0;
}
int mhl_sw_mutex_unlock(struct mutex*m)
{
	mutex_unlock(m);
	return 0;
}

/*********************dynamic switch I2C address*******************************/
extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int* rawdata);
static uint8_t Need_Switch_I2C_to_High_Address;

uint8_t reGetI2cAddress(uint8_t device_ID)
{
	uint8_t address;

	address = 0;
	switch(device_ID)
	{
		case 0x72:
			address = 0x76;
			break;
		case 0x7A:
			address = 0x7E;
			break;
		case 0x9A:
			address = 0x9E;
			break;
		case 0x92:
			address = 0x96;
			break;
		case 0xC8:
			address = 0xCC;
			break;
		case 0xA0:
			address = 0xA0;
			break;
		default:
			MHL_WARN("Error: invaild device ID\n");
	}

	return address;
}
/****************************Platform I2C Read/Write*****************************/
#define MAX_I2C_READ_NUM 8
#define MAX_I2C_WRITE_NUM 7

uint8_t mhl_i2c_read_len_bytes(struct i2c_client *client, uint8_t offset, uint8_t *buf, uint8_t len)
{
	uint8_t regAddress = offset;
	int ret = 0;

	while(len > 0)
	{
		if(len > MAX_I2C_READ_NUM)
		{
			MHL_DBG("mhl_i2c_read_len_bytes, len: %d\n", len);
			ret = i2c_master_send(client, (const char*)&regAddress, sizeof(uint8_t));  
			if(ret < 0)
			{
		        MHL_WARN("[Error]mhl i2c sends command error!\n");
				return 0;
			}
			else
			{
				ret = i2c_master_recv(client, (char*)buf, MAX_I2C_READ_NUM);
				if(ret < 0)
				{
			        MHL_WARN("[Error]mhl i2c recv data error!\n");
				}

				regAddress += MAX_I2C_READ_NUM;
				buf += MAX_I2C_READ_NUM;

				len -= MAX_I2C_READ_NUM;
			}
		}
		else
		{
			ret = i2c_master_send(client, (const char*)&regAddress, sizeof(uint8_t));  
			if(ret < 0)
			{
		        MHL_DBG("[Error1]mhl i2c sends command error!\n");
				return 0;
			}
			else
			{
				ret = i2c_master_recv(client, (char*)buf, len);
				if(ret < 0)
				{
			        MHL_DBG("[Error1]mhl i2c recv data error!\n");
				}

				regAddress += len;
				buf += len;

				len -= len;
			}	
		}
	}

	return 1;
}
uint8_t mhl_i2c_write_len_bytes(struct i2c_client *client, uint8_t offset, uint8_t *buf, uint8_t len)
{
	uint8_t regAddress = offset;
	int ret = 0;
	int i=0;
	char write_data[17];

	while(len > 0)
	{
		if(len > MAX_I2C_WRITE_NUM)
		{
			MHL_DBG("mhl_i2c_write_len_bytes, len: %d\n", len);

			write_data[0] = regAddress;
			for(i=0; i< MAX_I2C_WRITE_NUM; i++)
				write_data[i+1] = *(buf+i);
			
			ret = i2c_master_send(client, write_data, MAX_I2C_WRITE_NUM+1);  
			if(ret < 0)
			{
				MHL_WARN("[Error]mhl i2c write command/data error!\n");
				return 0;
			}

			regAddress += MAX_I2C_WRITE_NUM;
			len -= MAX_I2C_WRITE_NUM;
			buf += MAX_I2C_WRITE_NUM;
		}
		else
		{
			write_data[0] = regAddress;
			for(i=0; i< len; i++)
				write_data[i+1] = *(buf+i);
			
			ret = i2c_master_send(client, write_data, len+1);  
			if(ret < 0)
			{
				MHL_WARN("[Error1]mhl i2c write command/data error!\n");
				return 0;
			}

			regAddress += len;
			len -= len;
			buf += len;
		}
	}

	return 1;
}

uint8_t I2C_ReadBlock(uint8_t deviceID, uint8_t offset,uint8_t *buf, uint8_t len)
{
    uint8_t					accessI2cAddr;
    u32 client_main_addr;
	uint8_t slave_addr = deviceID;
    mhl_sw_mutex_lock(&mhl_lock);
    
    mhl_i2c_status |= 1;

	if(Need_Switch_I2C_to_High_Address)
	{
		slave_addr = reGetI2cAddress(deviceID);
	}
    accessI2cAddr = slave_addr>>1;

    //backup addr
    client_main_addr = si_dev_context->client->addr;
    si_dev_context->client->addr = accessI2cAddr;
    //si_dev_context->client->addr = (accessI2cAddr & I2C_MASK_FLAG)|I2C_WR_FLAG;
    /*si_dev_context->client->timing = 100; */

    memset(buf,0xff,len);
	mhl_i2c_read_len_bytes(si_dev_context->client, offset, buf, len);
/*
    memset(buf,0xff,len);
    for(i = 0 ;i < len;i++)
    {

        u8 tmp;
        tmp = offset + i;
        ///gMhlDevice.pI2cClient->ext_flag |= I2C_DIRECTION_FLAG;
        #if 0
        status = i2c_master_send(si_dev_context->client, (const char*)&tmp, 1);
        if (status < 0)
        {
            MHL_DBG("I2C_ReadByte(0x%02x, 0x%02x), i2c_transfer error: %d\n",
                    deviceID, offset, status);
        }

        status = i2c_master_recv(si_dev_context->client, (char*)&tmp, 1);
        #else
        status = i2c_master_send(si_dev_context->client, &tmp, 0x101);    
        #endif
		
        *buf = tmp; 
        buf++;
    }
*/

    /* restore default client address */
    si_dev_context->client->addr = client_main_addr;
    mhl_i2c_status &= 0xfe;
    mhl_sw_mutex_unlock(&mhl_lock);
    return len;
}

void I2C_WriteBlock(uint8_t deviceID, uint8_t offset, uint8_t *buf, uint16_t len)
{
    uint8_t					accessI2cAddr;
#if USE_DEFAULT_I2C_CODE
    union i2c_smbus_data	data;
#endif
    u32 client_main_addr;
	uint8_t slave_addr = deviceID;

    //MHL_DBG("hdmi enter %s (0x%02x, 0x%02x, 0x%02x)\n",__func__, deviceID, offset, len);
    mhl_sw_mutex_lock(&mhl_lock);
        
    mhl_i2c_status |= 2;
	if(Need_Switch_I2C_to_High_Address)
	{
		slave_addr = reGetI2cAddress(deviceID);
	}
    accessI2cAddr = slave_addr>>1;

    //backup addr
    client_main_addr = si_dev_context->client->addr;
    si_dev_context->client->addr = accessI2cAddr;
    /*si_dev_context->client->timing = 100; */

	mhl_i2c_write_len_bytes(si_dev_context->client, offset, buf, len);
/*
    for(i = 0 ;i < len;i++)
    {
#if USE_DEFAULT_I2C_CODE
        data.byte = *buf;
        status = i2c_smbus_xfer(si_dev_context->client->adapter, accessI2cAddr,
                0, I2C_SMBUS_WRITE, offset + i, I2C_SMBUS_BYTE_DATA,
                &data);
#else
        tmp[0] = offset + i;
        tmp[1] = *buf;
        ///gMhlDevice.pI2cClient->ext_flag |= I2C_DIRECTION_FLAG;
        status = i2c_master_send( si_dev_context->client, (const char*)tmp, 2);
#endif
        if (status < 0)
        {
            si_dev_context->client->addr = client_main_addr;
            MHL_DBG("mhl I2C_WriteBlock error %s ret %d\n",__func__, status);
            goto done ;
        }
        buf++;
    }
done:
*/

    /* restore default client address */
    si_dev_context->client->addr = client_main_addr;
    mhl_i2c_status &= 0xfd;
    mhl_sw_mutex_unlock(&mhl_lock);

    return ;
}

static inline int platform_read_i2c_block(struct i2c_adapter *i2c_bus
		, u8 page
		, u8 offset
		, u8 count
		, u8 *values
		)
{
#if 0
	struct i2c_msg msg[2];

	msg[0].flags = 0;
	msg[0].addr = page >> 1;
	msg[0].buf = &offset;
	msg[0].len = 1;

	msg[1].flags = I2C_M_RD;
	msg[1].addr = page >> 1;
	msg[1].buf = values;
	msg[1].len = count;

	return i2c_transfer(i2c_bus_adapter, msg, 2);
#endif
   
	I2C_ReadBlock(page, offset,values, count);
	///MHL_DBG("%s:%d I2c read page:0x%02x,offset:0x%02x,values:0x%02X,count:0x%02X\n"
    ///                ,__FUNCTION__,__LINE__, page, offset, values, count);
    return 2;
    
}

static inline int platform_write_i2c_block(struct i2c_adapter *i2c_bus
		, u8 page
		, u8 offset
		, u16 count
		, u8 *values
		)
{
#if 0
	struct i2c_msg	msg;
	u8		*buffer;
	int		ret;

	buffer = kmalloc(count + 1, GFP_KERNEL);
	if (!buffer) {
		MHL_WARN("%s:%d buffer allocation failed\n",__FUNCTION__,__LINE__);
		return -ENOMEM;
	}

	buffer[0] = offset;
	memmove(&buffer[1], values, count);

	msg.flags = 0;
	msg.addr = page >> 1;
	msg.buf = buffer;
	msg.len = count + 1;

	ret = i2c_transfer(i2c_bus, &msg, 1);

	kfree(buffer);

	if (ret != 1) {
		MHL_DBG("%s:%d I2c write failed 0x%02x:0x%02x\n"
				,__FUNCTION__,__LINE__, page, offset);
		ret = -EIO;
	} else {
		ret = 0;
	}

	return ret;
	#endif
	
	I2C_WriteBlock(page, offset, values, count);
	return 0;
	
}

/***************************End*******************************/
///#define ENABLE_MHL_VBUS_POWER_OUT
#ifdef ENABLE_MHL_VBUS_POWER_OUT
extern void mtk_disable_pmic_otg_mode(void);
extern void mtk_enable_pmic_otg_mode(void);
bool VBUS_state = false;
#include <mach/battery_meter.h>
#endif
void mhl_tx_vbus_control(enum vbus_power_state power_state)
{

#ifdef ENABLE_MHL_VBUS_POWER_OUT
	///struct mhl_dev_context *dev_context;
	///dev_context = i2c_get_clientdata(device_addresses[0].client);	// TODO: FD, TBC, it seems the 'client' is always 'NULL', is it right here
	pr_info("%s: mhl_tx_vbus_control3 %d-%d received!\n", __func__, VBUS_state, power_state);
    if(VBUS_state == power_state)
        return;
        
    VBUS_state = power_state;

	switch (power_state) {
	case VBUS_OFF:
		//set_pin(dev_context,TX2MHLRX_PWR_M,1);
		//set_pin(dev_context,LED_SRC_VBUS_ON,GPIO_LED_OFF);
		mtk_disable_pmic_otg_mode();
		break;

	case VBUS_ON:
		//set_pin(dev_context,TX2MHLRX_PWR_M,0);
		//set_pin(dev_context,LED_SRC_VBUS_ON,GPIO_LED_ON);
		pr_info("%s:  power chg %d received!\n",
				__func__, battery_meter_get_charger_voltage());
		if(battery_meter_get_charger_voltage() > 4000)
		    VBUS_state = VBUS_OFF;
		else
    		mtk_enable_pmic_otg_mode();
		pr_info("%s:  power chg %d received!\n",
				__func__, battery_meter_get_charger_voltage());
		msleep(100);
		break;

	default:
		pr_info("%s: Invalid power state %d received!\n",
				__func__, power_state);
		break;
	}        
#else
	pr_info("%s: do not support power out %d received!\n",
				__func__, power_state);
#endif	
}

/******************************Debug Start*****************************/
int si_device_dbg_i2c_reg_xfer(void *dev_context, u8 page, u8 offset, u8 count, bool rw_flag, u8 *buffer)
{
	if (rw_flag == DEBUG_I2C_WRITE)
		return mhl_tx_write_reg_block(dev_context, page, offset, count, buffer);
	else
		return mhl_tx_read_reg_block(dev_context, page, offset, count, buffer);
}


#define MAX_DEBUG_MSG_SIZE	1024

#if defined(DEBUG)

/*
 * Return a pointer to the file name part of the
 * passed path spec string.
 */
char *find_file_name(const char *path_spec)
{
	char *pc;

	for (pc = (char *)&path_spec[strlen(path_spec)]; pc != path_spec; --pc) {
		if ('\\' == *pc) {
			++pc;
			break;
		}
		if ('/' == *pc) {
			++pc;
			break;
		}
	}
	return pc;
}

void print_formatted_debug_msg(int level,
		char *file_spec, const char *func_name,
		int line_num, 
		char *fmt, ...)
{
	uint8_t		*msg = NULL;
	uint8_t		*msg_offset;
	char		*file_spec_sep = NULL;
	int		remaining_msg_len = MAX_DEBUG_MSG_SIZE;
	int		len;
	va_list		ap;

	/*
	 * Allow informational level debug messages to be turned off
	 * by a switch on the starter kit board
	 */
	if (level > debug_msgs){
		return;
	}

	if (fmt == NULL)
		return;

	/*
	 * Only want to print the file name where the debug print
	 * statement originated, not the path to it.
	 */
	if (file_spec != NULL)
		file_spec = find_file_name(file_spec);

	msg = kmalloc(remaining_msg_len, GFP_KERNEL);
	if(msg == NULL)
		return;

	msg_offset = msg;

	if (file_spec != NULL) {
		if (func_name != NULL)
			file_spec_sep = "->";
		else if (line_num != -1)
			file_spec_sep = ":";
	}

	len = scnprintf(msg_offset, remaining_msg_len, "mhl ");
	msg_offset += len;
	remaining_msg_len -= len;

	if (file_spec) {
		len = scnprintf(msg_offset, remaining_msg_len, "%s", file_spec);
		msg_offset += len;
		remaining_msg_len -= len;
	}

	if (file_spec_sep) {
		len = scnprintf(msg_offset, remaining_msg_len, "%s", file_spec_sep);
		msg_offset += len;
		remaining_msg_len -= len;
	}

	if (func_name) {
		len = scnprintf(msg_offset, remaining_msg_len, "%s", func_name);
		msg_offset += len;
		remaining_msg_len -= len;
	}

	if (line_num != -1) {
		if ((file_spec != NULL) || (func_name != NULL))
			len = scnprintf(msg_offset, remaining_msg_len, ":%d ", line_num);
		else
			len = scnprintf(msg_offset, remaining_msg_len, "%d ", line_num);

		msg_offset += len;
		remaining_msg_len -= len;
	}

	va_start(ap, fmt);
	len = vscnprintf(msg_offset, remaining_msg_len, fmt, ap);
	va_end(ap);

	pr_info("%s\n", msg);

	kfree(msg);
}

void dump_i2c_transfer(void *context, u8 page, u8 offset, u16 count, u8 *values, bool write)
{
	int		buf_size = 64;
	u16		idx;
	int		buf_offset;
	char		*buf;

	if (count > 1) {
		buf_size += count * 3; 				/* 3 characters per byte displayed */
		buf_size += ((count / 16) + 1) * 8;		/* plus per display row overhead */
	}

	buf = kmalloc(buf_size, GFP_KERNEL);
	if (!buf)
		return;

	if (count == 1) {

		scnprintf(buf, buf_size, "   I2C_%s %02X:%02X %s %02X\n",
				write ? "W" : "R",
				page, offset,
				write ? "<-" : "==",
				values[0]);
	} else {
		idx = 0;
		buf_offset = scnprintf(buf, buf_size, "I2C_%sB %02X:%02X - %d bytes:",
				write ? "W" : "R", page, offset, count);

		for (idx = 0; idx < count; idx++) {
			if (0 == (idx & 0x0F))
				buf_offset += scnprintf(&buf[buf_offset], buf_size - buf_offset,
						"\n%04X: ", idx);

			buf_offset += scnprintf(&buf[buf_offset], buf_size - buf_offset,
					"%02X ", values[idx]);
		}
		buf_offset += scnprintf(&buf[buf_offset], buf_size - buf_offset, "\n");
	}

	print_formatted_debug_msg(DBG_MSG_LEVEL_INFO, NULL, NULL, -1, buf);

	kfree(buf);
}
#endif /* #if defined(DEBUG) */
/******************************Debug End*******************************/

struct i2c_client *mClient = NULL;
static int mhl_eint_number = 0xffff;

/*
#ifdef CONFIG_MTK_LEGACY
static struct gpio starter_kit_control_gpios[] =
{
	{GPIO_MHL_INT,		GPIOF_IN,		        "MHL_intr"},
	{GPIO_MHL_RESET,	GPIOF_OUT_INIT_HIGH,	"MHL_tx_reset"},
};
#endif
*/
bool is_reset_on_exit_requested(void)
{
	return reset_on_exit;
}

int mhl_tx_write_reg_block(void *drv_context, u8 page, u8 offset, u16 count, u8 *values)
{
	DUMP_I2C_TRANSFER(drv_context, page, offset, count, values, true);

	return platform_write_i2c_block(i2c_bus_adapter,page, offset, count,values);
}


int mhl_tx_write_reg(void *drv_context, u8 page, u8 offset, u8 value)
{
	return mhl_tx_write_reg_block(drv_context, page, offset, 1, &value);
}


int mhl_tx_read_reg_block(void *drv_context, u8 page, u8 offset, u8 count, u8 *values)
{
	int ret;
	ret = platform_read_i2c_block(i2c_bus_adapter
			, page
			, offset
			, count
			, values
			);
	if (ret != 2) {
		MHL_TX_DBG_ERR(drv_context, "I2c read failed, 0x%02x:0x%02x\n", page, offset);
		ret = -EIO;
	} else {
		ret = 0;
		DUMP_I2C_TRANSFER(drv_context, page, offset, count, values, false);
	}

	return ret;
}

int mhl_tx_read_reg(void *drv_context, u8 page, u8 offset)
{
	u8		byte_read;
	int		status;

	status = mhl_tx_read_reg_block(drv_context, page, offset, 1, &byte_read);

	return status ? status : byte_read;
}

int mhl_tx_modify_reg(void *drv_context, u8 page, u8 offset, u8 mask, u8 value)
{
	int	reg_value;
	int	write_status;

	reg_value = mhl_tx_read_reg(drv_context, page, offset);
	if (reg_value < 0)
		return reg_value;

	reg_value &= ~mask;
	reg_value |= mask & value;

	write_status = mhl_tx_write_reg(drv_context, page, offset, reg_value);

	if (write_status < 0)
		return write_status;
	else
		return reg_value;
}

/*
static int32_t si_8348_mhl_tx_remove(struct i2c_client *client)
{
#if 1 //(
	if (gpio_client){
		struct mhl_dev_context *dev_context = i2c_get_clientdata(client);
		free_irq(gpio_client->irq,dev_context);
	}
#endif //)
	i2c_unregister_device(gpio_client);
	gpio_client = NULL;
	//gpio_free_array(starter_kit_control_gpios_for_expander,
	//		array_size_of_starter_kit_control_gpios_for_expander);
	gpio_free_array(starter_kit_control_gpios,
			ARRAY_SIZE(starter_kit_control_gpios));
	return 0;
}


static const struct i2c_device_id si_8348_mhl_tx_id[] = {
	{MHL_DEVICE_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, si_8348_mhl_tx_id);

static struct i2c_driver si_8348_mhl_tx_i2c_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = MHL_DRIVER_NAME,
		   },
	.id_table = si_8348_mhl_tx_id,
	.probe = si_8348_mhl_tx_i2c_probe,
	.remove = si_8348_mhl_tx_remove,
	.command = NULL,
};

static struct i2c_board_info __initdata si_8348_i2c_boardinfo[] = {
	{
	   	I2C_BOARD_INFO(MHL_DEVICE_NAME, (TX_PAGE_L0 >> 1)),
		.flags = I2C_CLIENT_WAKE,
		.irq = CUST_EINT_MHL_NUM,
	}
};

static void __exit si_8348_exit(void)
{
	int	idx;

	mhl_tx_remove(device_addresses[0].client);
	MHL_TX_DBG_INFO(NULL, "client removed\n");
	i2c_del_driver(&si_8348_mhl_tx_i2c_driver);
	MHL_TX_DBG_INFO(NULL, "i2c driver deleted from context\n");

	for (idx = 0; idx < ARRAY_SIZE(device_addresses); idx++) {
		MHL_TX_DBG_INFO(NULL, "\n");
		if (device_addresses[idx].client != NULL){
			MHL_TX_DBG_INFO(NULL, "unregistering device:%p\n",device_addresses[idx].client);
			i2c_unregister_device(device_addresses[idx].client);
		}
	}
}
*/
/************************** HAL To Platform****************************************/ 
static struct mhl_drv_info drv_info = {
	.drv_context_size = sizeof(struct drv_hw_context),
	.mhl_device_initialize = si_mhl_tx_chip_initialize,
	.mhl_device_isr = si_mhl_tx_drv_device_isr,
	.mhl_device_dbg_i2c_reg_xfer = si_device_dbg_i2c_reg_xfer,
	.mhl_start_video= si_mhl_tx_drv_enable_video_path,
};

int32_t sii_8348_tx_init(void) 
{
	int32_t ret = 0;

    MHL_DBG("mhl sii_8348_init\n");
#ifdef ENABLE_MHL_VBUS_POWER_OUT	
    VBUS_state = false;
#endif
	ret = mhl_tx_init(&drv_info, mClient);
	MHL_WARN("mhl sii_8348_init, mClient is %p\n", mClient);
	
	return ret;
}


extern wait_queue_head_t mhl_irq_wq;
extern atomic_t mhl_irq_event ;

#ifndef CONFIG_MTK_LEGACY
int get_mhl_irq_num(void)
{
    return mhl_eint_number;
}
#endif

void Mask_MHL_Intr(bool irq_context)
{
#ifdef CONFIG_MTK_LEGACY
#ifdef CUST_EINT_MHL_NUM
	mt_eint_mask(CUST_EINT_MHL_NUM);
#endif	
#else
    if(irq_context)
        disable_irq_nosync(get_mhl_irq_num());
    else    
        disable_irq(get_mhl_irq_num());
#endif  

	return ;
}

void Unmask_MHL_Intr(void)
{
#ifdef CONFIG_MTK_LEGACY
#ifdef CUST_EINT_MHL_NUM
	mt_eint_unmask(CUST_EINT_MHL_NUM);
#endif	
#else	
	enable_irq(get_mhl_irq_num());
#endif  
}

#ifdef CONFIG_MTK_LEGACY
#ifdef CUST_EINT_MHL_NUM
static void mhl8338_irq_handler(void)
{
 	atomic_set(&mhl_irq_event, 1);
    wake_up_interruptible(&mhl_irq_wq); 
	//mt65xx_eint_unmask(CUST_EINT_HDMI_HPD_NUM);   
}
#endif

void register_mhl_eint(void)
{
#ifdef CUST_EINT_MHL_NUM
    mt_eint_registration(CUST_EINT_MHL_NUM, CUST_EINT_MHL_TYPE, &mhl8338_irq_handler, 0);
    MHL_DBG("%s,CUST_EINT_MHL_NUM is %d \n", __func__, CUST_EINT_MHL_NUM);
#else
    MHL_WARN("%s,%d Error: GPIO_MHL_RST_B_PIN is not defined\n", __func__, __LINE__);
#endif    
    Mask_MHL_Intr(false);    
}

#else

static irqreturn_t mhl_eint_irq_handler(int irq, void *data)
{
	atomic_set(&mhl_irq_event, 1);
    wake_up_interruptible(&mhl_irq_wq); 
    
    Mask_MHL_Intr(true);
	return IRQ_HANDLED;
}

void register_mhl_eint(void)
{
    struct device_node *node = NULL;
    u32 ints[2]={0, 0};

    node = of_find_compatible_node(NULL, NULL, "mediatek,extd_dev");
    if(node)
    {
        of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
	///mt_eint_set_hw_debounce(ints[0],ints[1]);
	/*mt_gpio_set_debounce(ints[0],ints[1]);*/
	mhl_eint_number = irq_of_parse_and_map(node, 0);
	///irq_set_irq_type(mhl_eint_number,MT_LEVEL_SENSITIVE);
    	if(request_irq(mhl_eint_number, mhl_eint_irq_handler, IRQF_TRIGGER_NONE, "mediatek,extd_dev", NULL)) ///IRQF_TRIGGER_LOW
    	{
    		 MHL_DBG("request_irq fail-%d\n",mhl_eint_number);
    	}
    	else
        {
        	Mask_MHL_Intr(false);
        	MHL_DBG("request_irq success to return\n");
        	return;
    	}
    }
	
    Mask_MHL_Intr(false);
    MHL_WARN("Error: MHL EINT IRQ NOT AVAILABLE, node %p-irq %d!!\n", node, get_mhl_irq_num());
    
}



struct pinctrl *mhl_pinctrl;
struct pinctrl_state *pin_state;
extern struct device *ext_dev_context;
struct regulator *reg_v12_power = NULL;

char* dpi_gpio_name[32] = {
"dpi_d0_def", "dpi_d0_cfg","dpi_d1_def", "dpi_d1_cfg","dpi_d2_def", "dpi_d2_cfg","dpi_d3_def", "dpi_d3_cfg",	
"dpi_d4_def", "dpi_d4_cfg","dpi_d5_def", "dpi_d5_cfg","dpi_d6_def", "dpi_d6_cfg","dpi_d7_def", "dpi_d7_cfg",	
"dpi_d8_def", "dpi_d8_cfg","dpi_d9_def", "dpi_d9_cfg","dpi_d10_def", "dpi_d10_cfg","dpi_d11_def", "dpi_d11_cfg",	
"dpi_ck_def", "dpi_ck_cfg","dpi_de_def", "dpi_de_cfg","dpi_hsync_def", "dpi_hsync_cfg","dpi_vsync_def", "dpi_vsync_cfg"
};

char* i2s_gpio_name[6] ={
"i2s_dat_def","i2s_dat_cfg","i2s_ws_def","i2s_ws_cfg","i2s_ck_def","i2s_ck_cfg"
};

char* rst_gpio_name[2] ={
    "rst_low_cfg", "rst_high_cfg"
};

char* eint_gpio_name[1] ={
    "eint_input_cfg"
};

void dpi_gpio_ctrl(int enable)
{
    int offset = 0;
    int ret = 0;
    MHL_DBG("dpi_gpio_ctrl+  %ld !!\n",sizeof(dpi_gpio_name)); 
    if (IS_ERR(mhl_pinctrl)) {
        ret = PTR_ERR(mhl_pinctrl);
        MHL_WARN("Cannot find MHL RST pinctrl for dpi_gpio_ctrl!\n");
        return;
    }  
    
    if(enable)
        offset = 1;

    for(; offset < 32 ;)
    {
        pin_state = pinctrl_lookup_state(mhl_pinctrl, dpi_gpio_name[offset]);
        if (IS_ERR(pin_state)) {
            ret = PTR_ERR(pin_state);
            MHL_WARN("Cannot find MHL pinctrl--%s!!\n", dpi_gpio_name[offset]);
        }
        else
            pinctrl_select_state(mhl_pinctrl, pin_state); 
            
        offset +=2;
    }
}

void i2s_gpio_ctrl(int enable)
{
    int offset = 0;
    int ret = 0;

    MHL_DBG("i2s_gpio_ctrl+  %ld !!\n", sizeof(i2s_gpio_name)); 
    
    if (IS_ERR(mhl_pinctrl)) {
        ret = PTR_ERR(mhl_pinctrl);
        MHL_WARN("Cannot find MHL RST pinctrl for i2s_gpio_ctrl!\n");
        return;
    }  
    
    if(enable)
        offset = 1;
    for(; offset < 6 ;)
    {

        pin_state = pinctrl_lookup_state(mhl_pinctrl, i2s_gpio_name[offset]);
        if (IS_ERR(pin_state)) {
            ret = PTR_ERR(pin_state);
            MHL_WARN("Cannot find MHL pinctrl--%s!!\n", i2s_gpio_name[offset]);
        }
        else
            pinctrl_select_state(mhl_pinctrl, pin_state);   
        
        offset +=2;
    }

}

void mhl_power_ctrl(int enable)
{

}

void reset_mhl_board(int hwResetPeriod, int hwResetDelay)
{
    struct pinctrl_state *rst_low_state = NULL;
    struct pinctrl_state *rst_high_state = NULL;
    int err_cnt = 0;
    int ret = 0;
    
    MHL_DBG("reset_mhl_board+  %ld !!\n", sizeof(rst_gpio_name)); 
    if (IS_ERR(mhl_pinctrl)) {
        ret = PTR_ERR(mhl_pinctrl);
        MHL_WARN("Cannot find MHL RST pinctrl for reset_mhl_board!\n");
        return;
    }    
    
    rst_low_state = pinctrl_lookup_state(mhl_pinctrl, rst_gpio_name[0]);
    if (IS_ERR(rst_low_state)) {
        ret = PTR_ERR(rst_low_state);
        MHL_WARN("Cannot find MHL pinctrl--%s!!\n", rst_gpio_name[0]);
        err_cnt++;
    }
    
    rst_high_state = pinctrl_lookup_state(mhl_pinctrl, rst_gpio_name[1]);
    if (IS_ERR(rst_high_state)) {
        ret = PTR_ERR(rst_high_state);
        MHL_WARN("Cannot find MHL pinctrl--%s!!\n", rst_gpio_name[1]);
        err_cnt++;
    }

    if(err_cnt > 0)
        return;
        
    pinctrl_select_state(mhl_pinctrl, rst_high_state);   
    mdelay(hwResetPeriod);
    pinctrl_select_state(mhl_pinctrl, rst_low_state);   
    mdelay(hwResetPeriod);
    pinctrl_select_state(mhl_pinctrl, rst_high_state);   
    mdelay(hwResetDelay);

}

void cust_power_init(void)
{

}

void cust_power_on(int enable)
{
/*
    if(enable)
        regulator_enable(reg_v12_power);
    else
        regulator_disable(reg_v12_power);
*/
}

void mhl_platform_init(void)
{
	int ret =0;

	struct device_node *kd_node =NULL;
	/* backup original dev.of_node */

	MHL_DBG("mhl_platform_init start !!\n");

	if(ext_dev_context == NULL)
	{
		MHL_DBG("Cannot find device in platform_init!\n");
		goto plat_init_exit;

	}
	mhl_pinctrl = devm_pinctrl_get(ext_dev_context);
	if (IS_ERR(mhl_pinctrl)) {
		ret = PTR_ERR(mhl_pinctrl);
		MHL_DBG("Cannot find MHL Pinctrl!!!!\n");
		goto plat_init_exit;
	}

	pin_state = pinctrl_lookup_state(mhl_pinctrl, rst_gpio_name[1]);
	if (IS_ERR(pin_state)) {
		ret = PTR_ERR(pin_state);
		MHL_DBG("Cannot find MHL RST pinctrl low!!\n");
	}
	else
		pinctrl_select_state(mhl_pinctrl, pin_state);
	MHL_DBG("mhl_platform_init reset gpio init done!!\n");

	pin_state = pinctrl_lookup_state(mhl_pinctrl, eint_gpio_name[0]);
	if (IS_ERR(pin_state)) {
		ret = PTR_ERR(pin_state);
		MHL_DBG("Cannot find MHL eint pinctrl low!!\n");
	}
	else
		pinctrl_select_state(mhl_pinctrl, pin_state);
	MHL_DBG("mhl_platform_init eint gpio init done!!\n");


	i2s_gpio_ctrl(0);
	dpi_gpio_ctrl(0);

	kd_node = ext_dev_context->of_node;
	/* get regulator supply node */
	ext_dev_context->of_node = of_find_compatible_node(NULL,NULL,"mediatek,mt_pmic_regulator_supply"); 	

	MHL_DBG("mhl_platform_init can't get MHL_POWER_LD01!\n");

	if (reg_v12_power == NULL)
	{
		reg_v12_power = regulator_get(ext_dev_context, "mhl_12v");	
	}

	ext_dev_context->of_node = kd_node ;
	if (IS_ERR(reg_v12_power))
		MHL_DBG("mhl_platform_init ldo error %p!!!!!!!!!!!!!!\n", reg_v12_power );
	else {
		MHL_DBG("mhl_platform_init ldo init done %p\n", reg_v12_power );
		regulator_set_voltage(reg_v12_power, 1200000, 1200000);
		ret = regulator_enable(reg_v12_power);
	}

plat_init_exit:

	return;
}

#endif

static int32_t si_8348_mhl_tx_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;
 
	MHL_WARN("%s, client=%p\n", __func__, (void *)client);
   	/*client->timing = 100; */
    
    i2c_bus_adapter = to_i2c_adapter(client->dev.parent);
	/*
	 * On some boards the configuration switches 
	 *	are connected via an I2C controlled GPIO expander.
	 * At this point in the initialization, we're not 
	 *	ready to to I2C yet, so don't try to read any config
	 *  switches here.  Instead, wait until gpio_expander_init().
	 */	 
	 
	ret = mhl_tx_init(&drv_info, client);
	mClient = client;
	
	MHL_DBG("%s, mhl_tx_init ret %d\n", __func__, ret);
	if (ret){

	}

	Unmask_MHL_Intr();
	ret = 0;
	return ret;
}


//"Sil_MHL"   "mediatek,EXT_DISP"
struct i2c_device_id gMhlI2cIdTable[] = 
{
	{
		"Sil_MHL",0
	}
};

#ifdef CONFIG_OF
static const struct of_device_id Mhl_of_match[] = {
        {.compatible = "mediatek,ext_disp"},
        {},
};
#endif

#ifdef CONFIG_MTK_LEGACY
/*
static struct i2c_board_info __initdata i2c_mhl = { 
	.type = "Sil_MHL",
	.addr = 0x39,
	.irq = 8,
};
*/

struct i2c_driver mhl_i2c_driver = {                       
    .probe = si_8348_mhl_tx_i2c_probe,                                            
    .driver = { .name = "Sil_MHL",}, 
    .id_table = gMhlI2cIdTable,
}; 

#else 
struct i2c_driver mhl_i2c_driver = {                       
    .probe = si_8348_mhl_tx_i2c_probe,                                            
    .id_table = gMhlI2cIdTable,    
    .driver = {
        .name = "Sil_MHL",
#ifdef CONFIG_OF
        .of_match_table = Mhl_of_match,
#endif

        }, 
}; 
#endif

halReturn_t HalCloseI2cDevice(void)
{
	return HAL_RET_SUCCESS;
}

#ifdef CONFIG_MTK_LEGACY
int HalOpenI2cDevice(char const *DeviceName, char const *DriverName)
{
	halReturn_t		retStatus;
    int32_t 		retVal;

	///dynamic_switch_i2c_address();
/*
    if(get_hdmi_i2c_addr()==0x76)
    {
        Need_Switch_I2C_to_High_Address = 1;
	i2c_mhl.addr = 0x3B;
    }
*/
    MHL_DBG("HalOpenI2cDevice done +\n" );
    retVal = strnlen(DeviceName, I2C_NAME_SIZE);
    if (retVal >= I2C_NAME_SIZE)
    {
	MHL_WARN("I2c device name too long!\n");
	return HAL_RET_PARAMETER_ERROR;
    }

    /*i2c_register_board_info(get_hdmi_i2c_channel(), &i2c_mhl, 1);*/

    memcpy(gMhlI2cIdTable[0].name, DeviceName, retVal);
    gMhlI2cIdTable[0].name[retVal] = 0;
    gMhlI2cIdTable[0].driver_data = 0;

    //MHL_DBG("gMhlDevice.driver.driver.name=%s\n", gMhlDevice.driver.driver.name);
    //MHL_DBG("gMhlI2cIdTable.name=%s\n", gMhlI2cIdTable[0].name);
    retVal = i2c_add_driver(&mhl_i2c_driver);
    if (retVal != 0)
    {
	MHL_WARN("I2C driver add failed, retVal=%d\n", retVal);
	retStatus = HAL_RET_FAILURE;
    }
    else
    {
    	{
    		retStatus = HAL_RET_SUCCESS; 
    	}
    }

    mhl_mutex_init(&mhl_lock);
    
    return retStatus;
}
#else
int HalOpenI2cDevice(char const *DeviceName, char const *DriverName)
{
	halReturn_t		retStatus; 
    int32_t 		retVal;

    MHL_DBG("%s, + \n", __func__);

	mhl_mutex_init(&mhl_lock);
    mhl_platform_init();

    retVal = i2c_add_driver(&mhl_i2c_driver);
    if (retVal != 0)
    {
	MHL_WARN("I2C driver add failed, retVal=%d\n", retVal);
        retStatus = HAL_RET_FAILURE;
    }
    else
    {
    	{
    		retStatus = HAL_RET_SUCCESS;
    	}
    }
    MHL_DBG("%s, done %d\n", __func__, retVal);
    return retStatus;
}


#endif


/************************** ****************************************************/

MODULE_DESCRIPTION("Silicon Image MHL Transmitter driver");
MODULE_AUTHOR("Silicon Image <http://www.siliconimage.com>");
MODULE_LICENSE("GPL");
