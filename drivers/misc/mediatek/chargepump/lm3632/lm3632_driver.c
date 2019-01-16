/*
* This software program is licensed subject to the GNU General Public License
* (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

* (C) Copyright 2011 Bosch Sensortec GmbH
* All Rights Reserved
*/


/* file lm3632.c
brief This file contains all function implementations for the lm3632 in linux
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
//#include <cust_acc.h>

//#include <linux/hwmsensor.h>
//#include <linux/hwmsen_dev.h>
//#include <linux/sensors_io.h>
//#include <linux/hwmsen_helper.h>

#include <linux/leds.h>

#define LM3632_DSV_VPOS_EN  GPIO_DSV_AVEE_EN
#define LM3632_DSV_VPOS_EN_MODE GPIO_DSV_AVEE_EN_M_GPIO

#define LM3632_DSV_VNEG_EN GPIO_DSV_AVDD_EN
#define LM3632_DSV_VNEG_EN_MODE GPIO_DSV_AVDD_EN_M_GPIO

//#define LCD_LED_MAX 0x7F
//#define LCD_LED_MIN 0

//#define DEFAULT_BRIGHTNESS 0x73 //for 20mA
#define LM3632_MIN_VALUE_SETTINGS 10 /* value leds_brightness_set*/
#define LM3632_MAX_VALUE_SETTINGS 255 /* value leds_brightness_set*/
#define MIN_MAX_SCALE(x) (((x)<LM3632_MIN_VALUE_SETTINGS) ? LM3632_MIN_VALUE_SETTINGS : (((x)>LM3632_MAX_VALUE_SETTINGS) ? LM3632_MAX_VALUE_SETTINGS:(x)))

#define BACKLIHGT_NAME "charge-pump"

#define LM3632_GET_BITSLICE(regvar, bitname)\
((regvar & bitname##__MSK) >> bitname##__POS)

#define LM3632_SET_BITSLICE(regvar, bitname, val)\
((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))

#define LM3632_DEV_NAME "charge-pump"

#define CPD_TAG                  "[chargepump] "
#define CPD_FUN(f)               printk(CPD_TAG"%s\n", __FUNCTION__)
#define CPD_ERR(fmt, args...)    printk(CPD_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define CPD_LOG(fmt, args...)    printk(CPD_TAG fmt, ##args)

// I2C variable
static struct i2c_client *new_client = NULL;
static const struct i2c_device_id lm3632_i2c_id[] = {{LM3632_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_lm3632={ I2C_BOARD_INFO("charge-pump", 0x11)};

// Flash control
unsigned char strobe_ctrl;
unsigned char flash_ctrl=0; // flash_en register(0x0A) setting position change.
unsigned char lm3632_mapping_level; 
unsigned char bright_arr[] = {  // array index max 100, value under 255
        20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 22, 22, 22, 22, 24, 24, 24,  // 19
        26, 26, 26, 28, 28, 30, 30, 32, 32, 34, 34, 36, 38, 38, 40, 42, 42, 44, 36, 48,  // 39
        48, 50, 52, 54, 56, 58, 60, 62, 64, 66, 68, 70, 72, 76, 78, 80, 82, 86, 88, 90,  // 59
        94, 96, 98, 102, 104, 108, 110, 114, 118, 120, 124, 128, 130, 134, 138, 142, 146, 148, 152, 156,  //79
        160, 164, 168, 172, 178, 182, 186, 190, 194, 200, 204, 208, 212, 218, 222, 228, 232, 238, 242, 248, 255  // 100
    };

static unsigned char current_brightness = 0;
static unsigned char is_suspend = 0;

struct semaphore lm3632_lock; // Add semaphore for lcd and flash i2c communication.

/* generic */
#define LM3632_MAX_RETRY_I2C_XFER (100)
#define LM3632_I2C_WRITE_DELAY_TIME 1

/* i2c read routine for API*/
static char lm3632_i2c_read(struct i2c_client *client, u8 reg_addr,
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
                CPD_ERR("i2c bus read error\n");
                return -1;
            }
            *data = (u8)(dummy & 0xff);
            #else
            dummy = i2c_master_send(client, (char *)&reg_addr, 1);
            if (dummy < 0)
            {
                CPD_ERR("send dummy is %d\n", dummy);
                return -1;
            }

            dummy = i2c_master_recv(client, (char *)data, 1);
            if (dummy < 0)
            {
                CPD_ERR("recv dummy is %d\n", dummy);
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

        for (retry = 0; retry < LM3632_MAX_RETRY_I2C_XFER; retry++) {
            if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
                break;
            else
                mdelay(LM3632_I2C_WRITE_DELAY_TIME);
        }

        if (LM3632_MAX_RETRY_I2C_XFER <= retry) {
            CPD_ERR("I2C xfer error\n");
            return -EIO;
        }

        return 0;
        #endif
}

/* i2c write routine for */
static char lm3632_i2c_write(struct i2c_client *client, u8 reg_addr,
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
            for (retry = 0; retry < LM3632_MAX_RETRY_I2C_XFER; retry++) {
                if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0) {
                    break;
                }
                else {
                    mdelay(LM3632_I2C_WRITE_DELAY_TIME);
                }
            }
            if (LM3632_MAX_RETRY_I2C_XFER <= retry) {
                return -EIO;
            }
            reg_addr++;
            data++;
        }
        #endif
        //CPD_LOG("lm3632_i2c_write \n");
        return 0;
}

static int lm3632_smbus_read_byte(struct i2c_client *client,
            unsigned char reg_addr, unsigned char *data)
{
        return lm3632_i2c_read(client,reg_addr,data,1);
}

static int lm3632_smbus_write_byte(struct i2c_client *client,
            unsigned char reg_addr, unsigned char *data)
{
        int ret_val = 0;
        int i = 0;

        ret_val = lm3632_i2c_write(client,reg_addr,data,1);

        for ( i = 0; i < 5; i++)
        {
            if (ret_val != 0)
                lm3632_i2c_write(client,reg_addr,data,1);
            else
                return ret_val;
        }
        return ret_val;
}

static int lm3632_smbus_read_byte_block(struct i2c_client *client,
unsigned char reg_addr, unsigned char *data, unsigned char len)
{
        return lm3632_i2c_read(client,reg_addr,data,len);
}

bool check_charger_pump_vendor()
{
        int err = 0;
        unsigned char data = 0;

        err = lm3632_smbus_read_byte(new_client,0x01,&data);

        if(err < 0)
            CPD_ERR("%s read charge-pump vendor id fail\n", __func__);

        CPD_ERR("%s vendor is 0x%x\n", __func__, data&0x03);

        if((data&0x03) == 0x03) //Richtek
            return FALSE;
        else
            return TRUE;
}

int current_level;
int chargepump_set_backlight_level(unsigned int level)
{
        unsigned char data = 0;
        unsigned char data1 = 0;
        unsigned int bright_per = 0;

        CPD_LOG("chargepump_set_backlight_level  [%d]\n",level);
        if (level == 0)  
        {
            if(is_suspend == false)
            {
                CPD_LOG( "backlight off\n");
                down_interruptible(&lm3632_lock); // Add semaphore for lcd and flash i2c communication.
                data1 = 0x00; //backlight2 brightness 0
                lm3632_smbus_write_byte(new_client, 0x04, &data1);  // LSB 3bit all 0		
                lm3632_smbus_write_byte(new_client, 0x05, &data1);  // MSB 3bit all 0
                lm3632_smbus_read_byte(new_client, 0x0A, &data1);
                data1 &= 0x16;  // BLED1_EN, FLASH_MODE, FLASH Enable, AND operation
                lm3632_smbus_write_byte(new_client, 0x0A, &data1);

                is_suspend = true;// Move backlight suspend setting position into semaphore

                up(&lm3632_lock); // Add semaphore for lcd and flash i2c communication.
            }
        }
        else 
        {
            level = MIN_MAX_SCALE(level);
            //CPD_LOG("level = %d\n", level);
            bright_per = (level - (unsigned int)20) *(unsigned int)100 / (unsigned int)235;		
            data = bright_arr[bright_per];
            current_level = bright_per;
            CPD_LOG("%s bright_per = %d, data = %d\n", __func__, bright_per, data);
            lm3632_mapping_level = data;
            if (is_suspend == true) 
            {
                //printk( "------	 backlight_level resume-----\n");
                mdelay(10);
                down_interruptible(&lm3632_lock); // Add semaphore for lcd and flash i2c communication.

                data1 = 0x70;// OVP 29V, Linear mapping mode
                lm3632_smbus_write_byte(new_client, 0x02, &data1);

                data1 = 0x07;  // Backlight brightness LSB 3bits, 0b111
                lm3632_smbus_write_byte(new_client, 0x04, &data1);

                lm3632_smbus_write_byte(new_client, 0x05, &data);  // MSB 8 bit control
                CPD_LOG("backlight brightness Setting[reg0x05][value:0x%x]\n",data);

                lm3632_smbus_read_byte(new_client, 0x0A, &data1);
                data1 |= 0x11;  // BLED1_EN, BL_EN enable
                lm3632_smbus_write_byte(new_client, 0x0A, &data1);

                is_suspend = 0; // Move backlight suspend setting position into semaphore
                up(&lm3632_lock); // Add semaphore for lcd and flash i2c communication.
            }

            if (level != 0)	// Move backlight suspend setting position into semaphore
            {
                down_interruptible(&lm3632_lock); // Add semaphore for lcd and flash i2c communication.
                if(0)  // blocking useless code
                {
                    unsigned char read_data = 0;

                    lm3632_smbus_read_byte(new_client, 0x02, &read_data);
                    CPD_LOG("is_suspend[0x%x]\n",read_data);
                }			

                CPD_LOG("backlight Setting[reg0x05][value:0x%x]\n",data);
                lm3632_smbus_write_byte(new_client, 0x05, &data);
                up(&lm3632_lock); // Add semaphore for lcd and flash i2c communication.
            } // Move backlight suspend setting position into semaphore
        }

        return 0;
}

int chargepump_backlight_level_test(unsigned int level)
{
        unsigned char data = 0;

        if(level > 255)
        {
                level = 255;
        }

        data = 0x70;// OVP 29V, Linear mapping mode
        lm3632_smbus_write_byte(new_client, 0x02, &data);

        data = 0x07;  // Backlight brightness LSB 3bits, 0b111
        lm3632_smbus_write_byte(new_client, 0x04, &data);

        data = level;
        lm3632_smbus_write_byte(new_client, 0x05, &data);  // MSB 8 bit control
        CPD_LOG("backlight brightness Test[reg0x05][value:0x%x]\n",data);

        lm3632_smbus_read_byte(new_client, 0x0A, &data);
        data |= 0x11;  // BLED1_EN, BL_EN enable
        lm3632_smbus_write_byte(new_client, 0x0A, &data);

        return 0;
}

unsigned char get_lm3632_backlight_level(void)
{
        return lm3632_mapping_level;
}


void chargepump_DSV_on()
{
        //CPD_FUN();        
        mt_set_gpio_out(LM3632_DSV_VPOS_EN, GPIO_OUT_ONE);
        mdelay(1);
        mt_set_gpio_out(LM3632_DSV_VNEG_EN, GPIO_OUT_ONE);
}

void chargepump_DSV_off()
{
        //CPD_FUN();
        mt_set_gpio_out(LM3632_DSV_VPOS_EN, GPIO_OUT_ZERO);
        mdelay(1);
        mt_set_gpio_out(LM3632_DSV_VNEG_EN, GPIO_OUT_ZERO);
}

static int lm3632_probe(struct i2c_client *client,
        const struct i2c_device_id *id)
{   
        int ret;
        new_client = client;
        int err;

        CPD_FUN();
        if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
            CPD_LOG("i2c_check_functionality error\n");
            return -1;
        }

        sema_init(&lm3632_lock, 1); // Add semaphore for lcd and flash i2c communication.

        if (client == NULL) 
            printk("%s client is NULL\n", __func__);
        else
        {
            //CPD_LOG("%s %x %x %x\n", __func__, client->adapter, client->addr, client->flags);
            CPD_LOG("%s is OK\n", __func__);
        }
        return 0;
}

static int lm3632_remove(struct i2c_client *client)
{
        new_client = NULL;
        return 0;
}


static int lm3632_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) 
{ 
    return 0;
}

static struct i2c_driver lm3632_i2c_driver = {
        .driver = {
        //		.owner	= THIS_MODULE,
        .name	= LM3632_DEV_NAME,
        },
        .probe		= lm3632_probe,
        .remove		= lm3632_remove,
        //	.detect		= lm3632_detect,
        .id_table	= lm3632_i2c_id,
        //	.address_data = &lm3632250_i2c_addr_data,
};

static int lm3632_pd_probe(struct platform_device *pdev) 
{
        if(i2c_add_driver(&lm3632_i2c_driver))
        {
            CPD_ERR("add driver error\n");
            return -1;
        }
        else
        {
            CPD_LOG("i2c_add_driver OK\n");       
        }
        return 0;
}

static int lm3632_pd_remove(struct platform_device *pdev)
{
        CPD_FUN();
        i2c_del_driver(&lm3632_i2c_driver);
        return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void lm3632_early_suspend(struct early_suspend *h)
{
        int err = 0;
        unsigned char data;
        down_interruptible(&lm3632_lock); // Add semaphore for lcd and flash i2c communication.
        data = 0x00; //backlight2 brightness 0
        err = lm3632_smbus_write_byte(new_client, 0x04, &data);	
        err = lm3632_smbus_write_byte(new_client, 0x05, &data);

        err = lm3632_smbus_read_byte(new_client, 0x0A, &data);
        data &= 0x06;  // FLASH_MODE, FLASH Enable, AND operation    

        err = lm3632_smbus_write_byte(new_client, 0x0A, &data);
        up(&lm3632_lock); // Add semaphore for lcd and flash i2c communication.
        CPD_LOG("early_suspend  [%d]",data);
        //mt_set_gpio_out(GPIO_LCM_BL_EN,GPIO_OUT_ZERO);
}

void lm3632_flash_strobe_prepare(char OnOff,char ActiveHigh)
{
        int err = 0;

        down_interruptible(&lm3632_lock); // Add semaphore for lcd and flash i2c communication.

        err = lm3632_smbus_read_byte(new_client, 0x09, &strobe_ctrl);        

        // flash_en register(0x0A) setting position change.
        strobe_ctrl &= 0xF3;
        flash_ctrl = OnOff;

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
        }
        else if(OnOff == 2)
        {
            CPD_LOG("Torch mode On\n");
            strobe_ctrl |= 0x10;
        }
        else
        {
            CPD_LOG("Flash Off\n");
            strobe_ctrl &= 0xEF;
        }
        // flash_en register(0x0A) setting position change.
        err = lm3632_smbus_write_byte(new_client, 0x09, &strobe_ctrl);

        up(&lm3632_lock); // Add semaphore for lcd and flash i2c communication.
}

//strobe enable
void lm3632_flash_strobe_en()
{
        // flash_en register(0x0A) setting position change.
        int err = 0;
        int flash_OnOff=0;

        down_interruptible(&lm3632_lock); // Add semaphore for lcd and flash i2c communication.
        err = lm3632_smbus_read_byte(new_client, 0x0A, &flash_OnOff);
        if(flash_ctrl == 1)
            flash_OnOff |= 0x66;
        else if(flash_ctrl == 2)
            flash_OnOff |= 0x62;
        else
            flash_OnOff &= 0x99;
            
        err = lm3632_smbus_write_byte(new_client, 0x0A, &flash_OnOff);
        up(&lm3632_lock); // Add semaphore for lcd and flash i2c communication.
        // flash_en register(0x0A) setting position change.
}

//strobe level
void lm3632_flash_strobe_level(char level)
{
        int err = 0;
        unsigned char data1=0;
        unsigned char data2=0;
        unsigned char torch_level;
        unsigned char strobe_timeout = 0x1F;

        down_interruptible(&lm3632_lock); // Add semaphore for lcd and flash i2c communication.
        #if 0 // Add Main Flash current(4 Step)
        if( level == 1) 
        {
            torch_level = 0x20;
        }
        else
        {
            torch_level = 0x50;
        }

        err = lm3632_smbus_read_byte(new_client, 0x06, &data1);

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
        // Add Main Flash current(4 Step)
        #else
        torch_level = 0x50;

        err = lm3632_smbus_read_byte(new_client, 0x06, &data1);

        strobe_timeout = 0x1F; // Flash Timing Tuning
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
         // Add Main Flash current(4 Step)
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
        err = lm3632_smbus_write_byte(new_client, 0x06, &data1);

        data2 = 0x40 | strobe_timeout;
        CPD_LOG("Storbe Timeout =0x%x\n", data2);
        err |= lm3632_smbus_write_byte(new_client, 0x07, &data2);
        up(&lm3632_lock); // Add semaphore for lcd and flash i2c communication.
}

static void lm3632_late_resume(struct early_suspend *h)
{
        int err = 0;
        unsigned char data1;

        //mt_set_gpio_out(GPIO_LCM_BL_EN, GPIO_OUT_ONE);
        mdelay(50);
        down_interruptible(&lm3632_lock); // Add semaphore for lcd and flash i2c communication.

        data1 = 0x07;  // Backlight brightness LSB 3bits, 0b111
        lm3632_smbus_write_byte(new_client, 0x04, &data1);        	
        err = lm3632_smbus_write_byte(new_client, 0x05, &current_brightness);

        err = lm3632_smbus_read_byte(new_client, 0x0A, &data1);
        data1 |= 0x11;  // BLED1_EN, BL_EN enable        

        err = lm3632_smbus_write_byte(new_client, 0x0A, &data1);
        up(&lm3632_lock); // Add semaphore for lcd and flash i2c communication.
        CPD_LOG("lm3632_late_resume  [%d]",data1);
}

static struct early_suspend lm3632_early_suspend_desc = {
        .level		= EARLY_SUSPEND_LEVEL_BLANK_SCREEN,
        .suspend	= lm3632_early_suspend,
        .resume		= lm3632_late_resume,
};
#endif

static struct platform_driver lm3632_backlight_driver = {
        .probe      = lm3632_pd_probe,
        .remove     = lm3632_pd_remove,
        .driver     = {
                .name  = "charge-pump",
                .owner = THIS_MODULE,
        }
};      

static int __init lm3632_init(void)
{
        CPD_FUN();

        i2c_register_board_info(2, &i2c_lm3632, 1); 
        CPD_LOG("lm3632_i2c_master_addr = 2\n");

        #ifndef	CONFIG_MTK_LEDS
        register_early_suspend(&lm3632_early_suspend_desc);
        #endif	

        if(platform_driver_register(&lm3632_backlight_driver))
        {
            CPD_ERR("failed to register driver");
            return -1;
        }
        return 0;
}

static void __exit lm3632_exit(void)
{
        platform_driver_unregister(&lm3632_backlight_driver);
}

EXPORT_SYMBOL(lm3632_flash_strobe_en);
EXPORT_SYMBOL(lm3632_flash_strobe_prepare);
EXPORT_SYMBOL(lm3632_flash_strobe_level);
MODULE_AUTHOR("Albert Zhang <xu.zhang@bosch-sensortec.com>");
MODULE_DESCRIPTION("lm3632 driver");
MODULE_LICENSE("GPL");

module_init(lm3632_init);
module_exit(lm3632_exit);
