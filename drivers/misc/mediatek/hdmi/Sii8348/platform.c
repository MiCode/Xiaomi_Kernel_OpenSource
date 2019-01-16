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
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>

#include <linux/semaphore.h>
#include <linux/mutex.h>

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
#include "mach/eint.h"
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#include <cust_eint.h>
#include <mach/mt_pm_ldo.h>
#include <pmic_drv.h>

#include "hdmi_cust.h"

/* GPIOs assigned to control various starter kit signals */
#ifdef CUST_EINT_MHL_NUM
#define GPIO_MHL_INT		CUST_EINT_MHL_NUM		// BeagleBoard pin ID for TX interrupt		// 135 is pin 'SDMMC2_DAT3', which is pin 11 of EXP_HDR on BeagleBoard
#else
#define GPIO_MHL_INT 0
#endif

#ifdef GPIO_MHL_RST_B_PIN
#define GPIO_MHL_RESET		GPIO_MHL_RST_B_PIN		// BeagleBoard pin ID for TX reset		// 139 is pin 'SDMMC2_DAT7', which is pin 03 of EXP_HDR on BeagleBoard
#else
#define GPIO_MHL_RESET 0
#endif

/**
* LOG For HDMI Driver
*/
static size_t hdmi_log_on = true;
#define HDMI_LOG(fmt, arg...)  \
	do { \
		if (hdmi_log_on) printk("[HDMI_Platform]%s,%d ", __func__, __LINE__); printk(fmt, ##arg); \
	}while (0)

#define HDMI_FUNC()    \
	do { \
		if(hdmi_log_on) printk("[HDMI_Platform] %s\n", __func__); \
	}while (0)


static struct i2c_adapter	*i2c_bus_adapter = NULL;
static struct i2c_client	*gpio_client=NULL;

struct i2c_dev_info {
	uint8_t			dev_addr;
	struct i2c_client	*client;
};

#define I2C_DEV_INFO(addr) \
	{.dev_addr = addr >> 1, .client = NULL}

// I2C Page config
static struct i2c_dev_info device_addresses[] = {
	I2C_DEV_INFO(TX_PAGE_L0),
	I2C_DEV_INFO(TX_PAGE_L1),
	I2C_DEV_INFO(TX_PAGE_3),
	I2C_DEV_INFO(TX_PAGE_TPI),
	I2C_DEV_INFO(TX_PAGE_CBUS),
	I2C_DEV_INFO(TX_PAGE_DDC_EDID)
};

extern int I2S_Enable;
int	debug_msgs	= 3;	// print all msgs, default should be '0'
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

#if 0   ///SII_I2C_ADDR==(0x76)
static struct i2c_board_info __initdata i2c_mhl = { 
	.type = "Sil_MHL",
	.addr = 0x3B,
	.irq = 8,
};
#else
static struct i2c_board_info __initdata i2c_mhl = { 
	.type = "Sil_MHL",
	.addr = 0x39,
	.irq = 8,
};
#endif
/*********************dynamic switch I2C address*******************************/
extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int* rawdata);
static uint8_t Need_Switch_I2C_to_High_Address;
void dynamic_switch_i2c_address()
{
	printk("dynamic_switch_i2c_address +\n");
	int res;
	int data[4];
	res = IMM_GetOneChannelValue(12, data, NULL);
	if(res == 0)
	{
		printk("data: %d,%d, %d, %d\n", data[0], data[1], data[2], data[3]);
		if((data[1] >= 73 && data[1] <= 81)  ||  //ID 2.5
		   (data[1] >= 95 && data[1] <= 103) ||  //ID 3
		   (data[1] >= 131 && data[1] <= 139))   //ID 4
		{
			printk("ID 2.5, ID 3, ID 4, Need switch I2C address\n");
			Need_Switch_I2C_to_High_Address = 1;

			i2c_mhl.addr = 0x3B;
		}
		else
		{
			printk("ID 2, Do not need switch I2C address\n");
			Need_Switch_I2C_to_High_Address = 0;
		}
	}
}
uint8_t reGetI2cAddress(uint8_t device_ID)
{
	uint8_t address;
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
			printk("Error: invaild device ID\n");
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
			printk("mhl_i2c_read_len_bytes, len: %d\n", len);
			ret = i2c_master_send(client, (const char*)&regAddress, sizeof(uint8_t));  
			if(ret < 0)
			{
		        printk("[Error]mhl i2c sends command error!\n");
				return 0;
			}
			else
			{
				ret = i2c_master_recv(client, (char*)buf, MAX_I2C_READ_NUM);
				if(ret < 0)
				{
			        printk("[Error]mhl i2c recv data error!\n");
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
		        printk("[Error1]mhl i2c sends command error!\n");
				return 0;
			}
			else
			{
				ret = i2c_master_recv(client, (char*)buf, len);
				if(ret < 0)
				{
			        printk("[Error1]mhl i2c recv data error!\n");
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
	char write_data[8];

	while(len > 0)
	{
		if(len > MAX_I2C_WRITE_NUM)
		{
			printk("mhl_i2c_write_len_bytes, len: %d\n", len);

			write_data[0] = regAddress;
			for(i=0; i< MAX_I2C_WRITE_NUM; i++)
				write_data[i+1] = *(buf+i);
			
			ret = i2c_master_send(client, write_data, MAX_I2C_WRITE_NUM+1);  
			if(ret < 0)
			{
				printk("[Error]mhl i2c write command/data error!\n");
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
				printk("[Error1]mhl i2c write command/data error!\n");
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

    
    int i;
    uint8_t					accessI2cAddr;
    int32_t					status;
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
    si_dev_context->client->timing = 100;

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
            printk("I2C_ReadByte(0x%02x, 0x%02x), i2c_transfer error: %d\n",
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
    //printk("hdmi enter %s (0x%02x, 0x%02x, 0x%02x)\n",__func__, deviceID, offset, len);
    int i;
    uint8_t					accessI2cAddr;
#if USE_DEFAULT_I2C_CODE
    union i2c_smbus_data	data;
#endif
    int32_t					status;
    u8 tmp[2] = {0};
    u32 client_main_addr;
	uint8_t slave_addr = deviceID;

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
    si_dev_context->client->timing = 100;

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
            printk("mhl I2C_WriteBlock error %s ret %d\n",__func__, status);
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
	///printk("%s:%d I2c read page:0x%02x,offset:0x%02x,values:0x%02X,count:0x%02X\n"
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
		printk("%s:%d buffer allocation failed\n",__FUNCTION__,__LINE__);
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
		printk("%s:%d I2c write failed 0x%02x:0x%02x\n"
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

void mhl_tx_vbus_control(enum vbus_power_state power_state)
{
    return ;
    
	struct mhl_dev_context *dev_context;
	dev_context = i2c_get_clientdata(device_addresses[0].client);	// TODO: FD, TBC, it seems the 'client' is always 'NULL', is it right here???

	switch (power_state) {
	case VBUS_OFF:
		//set_pin(dev_context,TX2MHLRX_PWR_M,1);
		//set_pin(dev_context,LED_SRC_VBUS_ON,GPIO_LED_OFF);
		break;

	case VBUS_ON:
		//set_pin(dev_context,TX2MHLRX_PWR_M,0);
		//set_pin(dev_context,LED_SRC_VBUS_ON,GPIO_LED_ON);
		break;

	default:
		dev_err(dev_context->mhl_dev,
				"%s: Invalid power state %d received!\n",
				__func__, power_state);
		break;
	}
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

	printk(msg);

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
static struct gpio starter_kit_control_gpios[] =
{
	/*
	 * GPIO signals needed for the starter kit board.
	 */
	{GPIO_MHL_INT,		GPIOF_IN,		        "MHL_intr"},
	{GPIO_MHL_RESET,	GPIOF_OUT_INIT_HIGH,	"MHL_tx_reset"},
};

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
void Mask_MHL_Intr(void)
{
#ifdef 	CUST_EINT_MHL_NUM
	mt_eint_mask(CUST_EINT_MHL_NUM);
#endif  

	return ;
}

void Unmask_MHL_Intr(void)
{
#ifdef 	CUST_EINT_MHL_NUM
	mt_eint_unmask(CUST_EINT_MHL_NUM);
#endif  

	return ;
}

static struct mhl_drv_info drv_info = {
	.drv_context_size = sizeof(struct drv_hw_context),
	.mhl_device_initialize = si_mhl_tx_chip_initialize,
	.mhl_device_isr = si_mhl_tx_drv_device_isr,
	.mhl_device_dbg_i2c_reg_xfer = si_device_dbg_i2c_reg_xfer,
	.mhl_start_video= si_mhl_tx_drv_enable_video_path,
};

int32_t sii_8348_tx_init()
{
	int32_t ret = 0;

	ret = mhl_tx_init(&drv_info, mClient);
	printk("sii_8348_init, mClient is %p\n", mClient);
	
	return ret;
}

struct i2c_device_id gMhlI2cIdTable[] = 
{
	{
		"Sil_MHL",0
	}
};

static int32_t si_8348_mhl_tx_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	int ret;
 
	printk("%s, client=%p\n", __func__, (void *)client);

    client->timing = 100;
    
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
	
	printk("%s, mhl_tx_init ret %d\n", __func__, ret);
	if (ret){

	}

	Unmask_MHL_Intr();
	
	return ret;
}

struct i2c_driver mhl_i2c_driver = {                       
    .probe = si_8348_mhl_tx_i2c_probe,                                            
    .driver = { .name = "Sil_MHL",}, 
    .id_table = gMhlI2cIdTable,
}; 

halReturn_t HalCloseI2cDevice(void)
{
	return HAL_RET_SUCCESS;
}

int HalOpenI2cDevice(char const *DeviceName, char const *DriverName)
{
	halReturn_t		retStatus;
    int32_t 		retVal;

	///dynamic_switch_i2c_address();
    if(get_hdmi_i2c_addr()==0x76)
	{
        Need_Switch_I2C_to_High_Address = 1;
		i2c_mhl.addr = 0x3B;
	}
    printk("HalOpenI2cDevice done +\n" );
    retVal = strnlen(DeviceName, I2C_NAME_SIZE);
    if (retVal >= I2C_NAME_SIZE)
    {
    	printk("I2c device name too long!\n");
    	return HAL_RET_PARAMETER_ERROR;
    }

    i2c_register_board_info(get_hdmi_i2c_channel(), &i2c_mhl, 1);

    memcpy(gMhlI2cIdTable[0].name, DeviceName, retVal);
    gMhlI2cIdTable[0].name[retVal] = 0;
    gMhlI2cIdTable[0].driver_data = 0;

	//printk("gMhlDevice.driver.driver.name=%s\n", gMhlDevice.driver.driver.name);
	//printk("gMhlI2cIdTable.name=%s\n", gMhlI2cIdTable[0].name);
    retVal = i2c_add_driver(&mhl_i2c_driver);
    //printk("gMhlDevice.pI2cClient =%p\n", gMhlDevice.pI2cClient);
    if (retVal != 0)
    {
    	printk("I2C driver add failed, retVal=%d\n", retVal);
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
/************************** ****************************************************/

MODULE_DESCRIPTION("Silicon Image MHL Transmitter driver");
MODULE_AUTHOR("Silicon Image <http://www.siliconimage.com>");
MODULE_LICENSE("GPL");
