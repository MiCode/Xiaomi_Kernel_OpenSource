/*
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2011 Bosch Sensortec GmbH
 * All Rights Reserved
 */


/* file rt8542.c
   brief This file contains all function implementations for the rt8542 in linux
   this source file refer to MT6572 platform 
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/kernel.h>
#include <linux/delay.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#include <mach/mt_pm_ldo.h>

#include <linux/platform_device.h>
#include <cust_acc.h>

#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <linux/hwmsen_helper.h>

#include <linux/leds.h>

#define LCD_LED_MAX 0x7F
#define LCD_LED_MIN 0

#define DEFAULT_BRIGHTNESS 0x73 //for 20mA
#define RT8542_MIN_VALUE_SETTINGS 20 /* value leds_brightness_set*/
#define RT8542_MAX_VALUE_SETTINGS 255 /* value leds_brightness_set*/
#define MIN_MAX_SCALE(x) (((x)<RT8542_MIN_VALUE_SETTINGS) ? RT8542_MIN_VALUE_SETTINGS : (((x)>RT8542_MAX_VALUE_SETTINGS) ? RT8542_MAX_VALUE_SETTINGS:(x)))

#define BACKLIHGT_NAME "charge-pump"

#define RT8542_GET_BITSLICE(regvar, bitname)\
	((regvar & bitname##__MSK) >> bitname##__POS)

#define RT8542_SET_BITSLICE(regvar, bitname, val)\
	((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))

#define RT8542_DEV_NAME "charge-pump"

#define CPD_TAG                  "[ChargePump] "
#define CPD_FUN(f)               printk(CPD_TAG"%s\n", __FUNCTION__)
#define CPD_ERR(fmt, args...)    printk(CPD_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define CPD_LOG(fmt, args...)    printk(CPD_TAG fmt, ##args)

// I2C variable
static struct i2c_client *new_client = NULL;
static const struct i2c_device_id rt8542_i2c_id[] = {{RT8542_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_rt8542={ I2C_BOARD_INFO("charge-pump", 0x39)};

// Flash control
unsigned char strobe_ctrl;
unsigned char flash_ctrl;
unsigned char flash_status;
#define GPIO_LCD_BL_EN GPIO_LCM_BL_EN
#define GPIO_LCD_BL_EN_M_GPIO GPIO_LCM_BL_EN_M_GPIO
// Gamma 2.2 Table
unsigned char bright_arr[] = {
	10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12,
	13, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 19, 19, 20, 21, 21, 22, 23, 24,
	24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 38, 39, 40, 41, 43, 44, 45,
	47, 48, 49, 51, 52, 54, 55, 57, 59, 60, 62, 64, 65, 67, 69, 71, 73, 74, 76, 78,
	80, 82, 84, 86, 89, 91, 93, 95, 97, 100, 102, 104, 106, 109, 111, 114, 116, 119, 121, 124, 127};

static unsigned char current_brightness = 0;
static unsigned char is_suspend = 0;

struct semaphore rt8542_lock;

/* generic */
#define RT8542_MAX_RETRY_I2C_XFER (100)
#define RT8542_I2C_WRITE_DELAY_TIME 1

typedef struct
{
    kal_bool       bat_exist;
    kal_bool       bat_full;
    kal_bool       bat_low;
    UINT32      bat_charging_state;
    UINT32      bat_vol;
    kal_bool     charger_exist;
    UINT32      pre_charging_current;
    UINT32      charging_current;
    INT32      charger_vol;
    UINT32       charger_protect_status;
    UINT32      ISENSE;
    UINT32      ICharging;
    INT32       temperature;
    UINT32      total_charging_time;
    UINT32      PRE_charging_time;
    UINT32      CC_charging_time;
    UINT32      TOPOFF_charging_time;
    UINT32      POSTFULL_charging_time;
    UINT32       charger_type;
    UINT32       PWR_SRC;
    UINT32       SOC;
    UINT32       ADC_BAT_SENSE;
    UINT32       ADC_I_SENSE;
} PMU_ChargerStruct;

extern PMU_ChargerStruct BMT_status;

/* i2c read routine for API*/
static char rt8542_i2c_read(struct i2c_client *client, u8 reg_addr,
		u8 *data, u8 len)
{
#if !defined BMA_USE_BASIC_I2C_FUNC
	s32 dummy;
	if (NULL == client)
		return -1;

	while (0 != len--) {
#ifdef BMA_SMBUS
		dummy = i2c_smbus_read_byte_data(client, reg_addr);
		if (dummy < 0) {
			CPD_ERR("i2c bus read error");
			return -1;
		}
		*data = (u8)(dummy & 0xff);
#else
		dummy = i2c_master_send(client, (char *)&reg_addr, 1);
		if (dummy < 0)
		{
            printk("send dummy is %d", dummy);
			return -1;
		}

		dummy = i2c_master_recv(client, (char *)data, 1);
		if (dummy < 0)
		{
            printk("recv dummy is %d", dummy);
			return -1;
		}
#endif
		reg_addr++;
		data++;
	}
	return 0;
#else
	int retry;

	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = &reg_addr,
		},

		{
		 .addr = client->addr,
		 .flags = I2C_M_RD,
		 .len = len,
		 .buf = data,
		 },
	};

	for (retry = 0; retry < RT8542_MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
			break;
		else
			mdelay(RT8542_I2C_WRITE_DELAY_TIME);
	}

	if (RT8542_MAX_RETRY_I2C_XFER <= retry) {
		CPD_ERR("I2C xfer error");
		return -EIO;
	}

	return 0;
#endif
}

/* i2c write routine for */
static char rt8542_i2c_write(struct i2c_client *client, u8 reg_addr,
		u8 *data, u8 len)
{
#if !defined BMA_USE_BASIC_I2C_FUNC
	s32 dummy;
#ifndef BMA_SMBUS
	u8 buffer[2];
#endif

	if (NULL == client)
		return -1;

	while (0 != len--) {
		#if 1
		dummy = i2c_smbus_write_byte_data(client, reg_addr, *data);
#else
		buffer[0] = reg_addr;
		buffer[1] = *data;
		dummy = i2c_master_send(client, (char *)buffer, 2);
		#endif

		reg_addr++;
		data++;
		if (dummy < 0) {
			return -1;
		}
	}

#else
	u8 buffer[2];
	int retry;
	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = 2,
		 .buf = buffer,
		 },
	};

	while (0 != len--) {
		buffer[0] = reg_addr;
		buffer[1] = *data;
		for (retry = 0; retry < RT8542_MAX_RETRY_I2C_XFER; retry++) {
			if (i2c_transfer(client->adapter, msg,
						ARRAY_SIZE(msg)) > 0) {
				break;
			} else {
				mdelay(RT8542_I2C_WRITE_DELAY_TIME);
			}
		}
		if (RT8542_MAX_RETRY_I2C_XFER <= retry) {
			return -EIO;
		}
		reg_addr++;
		data++;
	}
#endif
	printk("\n [RT8542] rt8542_i2c_write \n");
	return 0;
}

static int rt8542_smbus_read_byte(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data)
{
	return rt8542_i2c_read(client,reg_addr,data,1);
}

static int rt8542_smbus_write_byte(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data)
{
	int ret_val = 0;
	int i = 0;

	ret_val = rt8542_i2c_write(client,reg_addr,data,1);

	for ( i = 0; i < 5; i++)
	{
		if (ret_val != 0)
			rt8542_i2c_write(client,reg_addr,data,1);
		else
			return ret_val;
	}
	return ret_val;
}

static int rt8542_smbus_read_byte_block(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data, unsigned char len)
{
	return rt8542_i2c_read(client,reg_addr,data,len);
}

bool check_charger_pump_vendor()
{
    int err = 0;
    unsigned char data = 0;

    err = rt8542_smbus_read_byte(new_client,0x01,&data);

    if(err < 0)
        printk(KERN_ERR "%s read charge-pump vendor id fail\n", __func__);

    printk(KERN_INFO "%s vendor is 0x%x\n", __func__, data&0x03);

    if((data&0x03) == 0x03) //Richtek
        return FALSE;
    else
        return TRUE;
}
	
int chargepump_set_backlight_level(unsigned int level)
{
	unsigned char data = 0;
	unsigned char data1 = 0;
	unsigned int bright_per = 0;

	printk("\n[RT8542] chargepump_set_backlight_level  [%d]\n",data1);

	if (level == 0) 
	{
		printk( "------	 backlight_level suspend-----\n");
		down_interruptible(&rt8542_lock);
		data1 = 0x00; //backlight2 brightness 0
		rt8542_smbus_write_byte(new_client, 0x05, &data1);
		
        rt8542_smbus_read_byte(new_client, 0x0A, &data1);
        data1 &= 0x66;

		rt8542_smbus_write_byte(new_client, 0x0A, &data1);
		up(&rt8542_lock); 
		printk( "[RT8542] flash_status = %d\n", flash_status);
		if( flash_status == 0 ){
			mt_set_gpio_out(GPIO_LCD_BL_EN,GPIO_OUT_ZERO);
		}
		is_suspend = 1;
	}
	else 
	{
		level = MIN_MAX_SCALE(level);
		printk(KERN_ERR "[RT8542] level = %d", level);
		
		//Gamma 2.2 Table adapted
		bright_per = (level - (unsigned int)20) *(unsigned int)100 / (unsigned int)235;
		data = bright_arr[bright_per];

		//printk("[Backlight] %s bright_per = %d, data = %d\n", __func__, bright_per, data);

		if (is_suspend == 1) 
		{
			//printk( "------	 backlight_level resume-----\n");
			is_suspend = 0;
			mt_set_gpio_out(GPIO_LCD_BL_EN,GPIO_OUT_ONE);
			mdelay(10);
			down_interruptible(&rt8542_lock); 
			if(check_charger_pump_vendor() == FALSE)
			{
				data1 = 0x54;//0x37; 
				rt8542_smbus_write_byte(new_client, 0x02, &data1);
				printk("[ChargePump]-Richtek\n");
			}
			else
			{
				data1 = 0x54;//0x57; 
				rt8542_smbus_write_byte(new_client, 0x02, &data1);
				printk("[RT8542]-TI\n");
			}

			rt8542_smbus_write_byte(new_client, 0x05, &data);
			printk("[RT8542]-backlight brightness Setting[reg0x05][value:0x%x]\n",data);
			
            rt8542_smbus_read_byte(new_client, 0x0A, &data1);
			data1 |= 0x19;

			rt8542_smbus_write_byte(new_client, 0x0A, &data1);
			up(&rt8542_lock); 
		}

		down_interruptible(&rt8542_lock); 
        {
            unsigned char read_data = 0;
		
            rt8542_smbus_read_byte(new_client, 0x02, &read_data);
            printk("[RT8542]-OVP[0x%x]\n",read_data);
        }

		printk("[RT8542]-backlight Seting[reg0x05][value:0x%x]\n",data);
		rt8542_smbus_write_byte(new_client, 0x05, &data);
		up(&rt8542_lock);
	}
	return 0;
}

static int rt8542_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{   
	new_client = client;

    CPD_FUN();
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CPD_LOG("i2c_check_functionality error\n");
		return -1;
	}

    sema_init(&rt8542_lock, 1);

	if (client == NULL) 
		printk("%s client is NULL\n", __func__);
	else
	{
		printk("%s %p %x %x\n", __func__, client->adapter, client->addr, client->flags);
	}
	return 0;
}


static int rt8542_remove(struct i2c_client *client)
{
    new_client = NULL;
	return 0;
}


static int __attribute__ ((unused)) rt8542_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) 
{ 
	return 0;
}

static struct i2c_driver rt8542_i2c_driver = {
	.driver = {
//		.owner	= THIS_MODULE,
		.name	= RT8542_DEV_NAME,
	},
	.probe		= rt8542_probe,
	.remove		= rt8542_remove,
//	.detect		= rt8542_detect,
	.id_table	= rt8542_i2c_id,
//	.address_data = &rt8542250_i2c_addr_data,
};

static int rt8542_pd_probe(struct platform_device *pdev) 
{
	mt_set_gpio_mode(GPIO_LCD_BL_EN, GPIO_LCD_BL_EN_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_LCD_BL_EN, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_LCD_BL_EN, GPIO_DIR_OUT);

	i2c_add_driver(&rt8542_i2c_driver);
	return 0;
}

static int __attribute__ ((unused)) rt8542_pd_remove(struct platform_device *pdev)
{
    CPD_FUN();
    i2c_del_driver(&rt8542_i2c_driver);
    return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void rt8542_early_suspend(struct early_suspend *h)
{
	int err = 0;
	unsigned char data;
	down_interruptible(&rt8542_lock); 
	data = 0x00; //backlight2 brightness 0
	err = rt8542_smbus_write_byte(new_client, 0x05, &data);

    err = rt8542_smbus_read_byte(new_client, 0x0A, &data);
    data &= 0x66;

	err = rt8542_smbus_write_byte(new_client, 0x0A, &data);
	up(&rt8542_lock); 
	printk("\n[RT8542] rt8542_early_suspend  [%d]",data);
	mt_set_gpio_out(GPIO_LCD_BL_EN,GPIO_OUT_ZERO);
}

void rt8542_flash_strobe_prepare(char OnOff,char ActiveHigh)
{
	int err = 0;

    down_interruptible(&rt8542_lock);

    err = rt8542_smbus_read_byte(new_client, 0x09, &strobe_ctrl);        

    err = rt8542_smbus_read_byte(new_client, 0x0A, &flash_ctrl);

    strobe_ctrl &= 0xF3;
    
	if(ActiveHigh)
	{
		strobe_ctrl |= 0x20;
	}
	else
	{
		strobe_ctrl &= 0xDF;
	}

	if(OnOff == 1)
	{
		CPD_LOG("Strobe mode On\n");
		strobe_ctrl |= 0x10;		
        flash_ctrl |= 0x66;
		flash_status = 1;
		printk( "[RT8542][Strobe] flash_status = %d\n", flash_status);
    }
    else if(OnOff == 2)
    {
		CPD_LOG("Torch mode On\n");
		strobe_ctrl |= 0x10;
        flash_ctrl |= 0x62;
		flash_status = 1;
		printk( "[RT8542][Torch] flash_status = %d\n", flash_status);
    }
	else
	{
		CPD_LOG("Flash Off\n");
		strobe_ctrl &= 0xEF;
		flash_ctrl &= 0x99;
		flash_status = 0;
		printk( "[RT8542][off] flash_status = %d\n", flash_status);
	}

	err = rt8542_smbus_write_byte(new_client, 0x09, &strobe_ctrl);

    up(&rt8542_lock);
}

//strobe enable
void rt8542_flash_strobe_en()
{
	int err = 0;
    down_interruptible(&rt8542_lock); 
	err = rt8542_smbus_write_byte(new_client, 0x0A, &flash_ctrl); 
    up(&rt8542_lock); 
}

//strobe level
void rt8542_flash_strobe_level(char level)
{
	int err = 0;
	unsigned char data1=0;
    unsigned char data2=0;
    unsigned char torch_level;
    unsigned char strobe_timeout = 0x1F;
    down_interruptible(&rt8542_lock); 
#if 0
    if( level == 1) 
    {
        torch_level = 0x20;
    }
    else
    {
        torch_level = 0x50;
    }
        
    err = rt8542_smbus_read_byte(new_client, 0x06, &data1);

	if(31 < level)
    {    
		data1= torch_level | 0x0A;
        strobe_timeout = 0x0F;
    }
    else if(level < 0)
    {
		data1= torch_level ;
    }
    else
    {
		data1= torch_level | level; 
    }

#else
    torch_level = 0x50;

    err = rt8542_smbus_read_byte(new_client, 0x06, &data1);

    strobe_timeout = 0x0F;
    if(level < 0)
        data1= torch_level;
    else if(level == 1)
        data1= torch_level | 0x03;
    else if(level == 2)
        data1= torch_level | 0x05;
    else if(level == 3)
        data1= torch_level | 0x08;
    else if(level == 4)
        data1= torch_level | 0x0A;
    else
        data1= torch_level | level;

#endif

/*
    if(0)
    {
	    CPD_LOG("Batt temp=%d\n", BMT_status.temperature );
        
        torch_level = 0xF0 & data1;
        level = 0x0F & data1;
        torch_level = 0xF0 & (torch_level >> 2);
        level = 0x0F & (level >> 2);
        
        data1 = torch_level | level;
    }
*/
	CPD_LOG("Flash Level =0x%x\n", data1);
    err = rt8542_smbus_write_byte(new_client, 0x06, &data1);
    
    data2 = 0x40 | strobe_timeout;
    CPD_LOG("Storbe Timeout =0x%x\n", data2);
    err |= rt8542_smbus_write_byte(new_client, 0x07, &data2);
    up(&rt8542_lock); 
}

static void rt8542_late_resume(struct early_suspend *h)
{
	int err = 0;
	unsigned char data1;
	
	mt_set_gpio_out(GPIO_LCD_BL_EN, GPIO_OUT_ONE);
	mdelay(50);
	down_interruptible(&rt8542_lock); 
	err = rt8542_smbus_write_byte(new_client, 0x05, &current_brightness);
	
    err = rt8542_smbus_read_byte(new_client, 0x0A, &data1);
	data1 |= 0x19;//backlight enable

	err = rt8542_smbus_write_byte(new_client, 0x0A, &data1);
	up(&rt8542_lock); 
	printk("\n [RT8542] rt8542_late_resume  [%d]",data1);
}

static struct early_suspend __attribute__ ((unused)) rt8542_early_suspend_desc = {
	.level		= EARLY_SUSPEND_LEVEL_BLANK_SCREEN,
	.suspend	= rt8542_early_suspend,
	.resume		= rt8542_late_resume,
};
#endif

static struct platform_driver rt8542_backlight_driver = {
	.probe      = rt8542_pd_probe,
	.remove     = rt8542_pd_remove,
	.driver     = {
	.name  = "charge-pump",
	.owner = THIS_MODULE,
	}
};      

static struct platform_device mtk_backlight_dev = {
	.name = "charge-pump",
	.id   = -1,};

static int __init rt8542_init(void)
{
	CPD_FUN();
	if (platform_device_register(&mtk_backlight_dev))
	{
		CPD_ERR("failed to register device");
		return -1;

	}
	//i2c number 1(0~2) control
	i2c_register_board_info(3, &i2c_rt8542, 1); 

	#ifndef	CONFIG_MTK_LEDS
	register_early_suspend(&rt8542_early_suspend_desc);
	#endif	

	if(platform_driver_register(&rt8542_backlight_driver))
	{
		CPD_ERR("failed to register driver");
		return -1;
	}

	return 0;
}

static void __exit rt8542_exit(void)
{
	platform_driver_unregister(&rt8542_backlight_driver);
}

EXPORT_SYMBOL(rt8542_flash_strobe_en);
EXPORT_SYMBOL(rt8542_flash_strobe_prepare);
EXPORT_SYMBOL(rt8542_flash_strobe_level);
MODULE_AUTHOR("Albert Zhang <xu.zhang@bosch-sensortec.com>");
MODULE_DESCRIPTION("rt8542 driver");
MODULE_LICENSE("GPL");

module_init(rt8542_init);
module_exit(rt8542_exit);

