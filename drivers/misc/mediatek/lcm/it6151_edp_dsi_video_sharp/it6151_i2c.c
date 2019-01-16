#ifndef BUILD_LK
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>

#include <cust_acc.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <linux/hwmsen_helper.h>
#include <linux/xlog.h>


#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

//#include "it6151_i2c.h"
//#include "cust_charging.h"
//#include <mach/charging.h>


/**********************************************************
  *
  *   [I2C Slave Setting] 
  *
  *********************************************************/

static DEFINE_MUTEX(it6151_i2c_access);

#define it6151_BUSNUM 2

#define DEVICE_NAME "it6151"
#define it6151_DEVICE_ID   0x5c


 struct i2c_client *it6151_0;
 struct i2c_client *it6151_1;



static const struct i2c_device_id it6151_id[] = {
	{ "it6151_0", 0 },
	{ "it6151_1", 0 }
};

static struct i2c_board_info it6151_i2c[] = { 	
      {
	 .type = "it6151_0",
	 .addr = 0x5C, 
	},
	{
	 .type = "it6151_1",
	 .addr = 0x6C, 
	}
};

// i2c_smbus_read_byte_data(sii902xA,RegOffset);
UINT8 it6151_reg_i2c_read (struct i2c_client *client,UINT8 RegOffset)
{
    UINT8 Readnum;
	Readnum = i2c_smbus_read_byte_data(client,RegOffset);
	printk("[6151]client:%s , read RegOffset=0x%x,Readnum=0x%x \n", client->name, RegOffset, Readnum);
	return Readnum;
}

UINT8 it6151_reg_i2c_read_byte(U8 dev_addr,U8  *cmdBuffer, U8 *dataBuffer)
{
	UINT8 RetVal = 0xFF;
       if(dev_addr == it6151_0->addr)
		RetVal = it6151_reg_i2c_read(it6151_0, *cmdBuffer);
	else if(dev_addr == it6151_1->addr)
	       RetVal = it6151_reg_i2c_read(it6151_1, *cmdBuffer);
	else
	       printk("[it6151_reg_i2c_read_byte]error:  no this dev_addr \n");
	       
	return RetVal;
}

void it6151_reg_i2c_write (struct i2c_client *client,UINT8 RegOffset, UINT8 Data)
{
	i2c_smbus_write_byte_data(client, RegOffset, Data);

	printk("[6151]client:%s , write RegOffset=0x%x,Data=0x%x \n", client->name, RegOffset, Data);
}

void it6151_reg_i2c_write_byte(U8 dev_addr,U8  cmd, U8 data)
{

       U8 write_data[8];

	write_data[0] = cmd;
	write_data[1] = data;


       if(dev_addr == 0x5C)
       {
              it6151_0->addr = 0x5C;
		it6151_0->ext_flag = 0;
		i2c_master_send(it6151_0, write_data, 2);
       }
	else if(dev_addr == 0x6C)
	{
	       it6151_1->addr = 0x6C;
		it6151_1->ext_flag = 0;
	       i2c_master_send(it6151_1, write_data, 2);
	}
	else
	       printk("[it6151_reg_i2c_read_byte]error:  no this dev_addr \n");

}


static int match_id(const struct i2c_device_id *id, const struct i2c_client *client)
{
	if (strcmp(client->name, id->name) == 0)
		return true;

	return false;
}

static int it6151_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{

	 int ret = 0;
	 int err=0; 

	if(match_id(&it6151_id[0], client))
		{
		    if (!(it6151_0 = kmalloc(sizeof(struct i2c_client), GFP_KERNEL))) {
        		err = -ENOMEM;
        		goto exit;
    		      }    
			memset(it6151_0, 0, sizeof(struct i2c_client));
			it6151_0 = client;
			dev_info(&client->adapter->dev, "attached it6151_0: %s "
				"into i2c adapter successfully \n", id->name);
		}
	else if(match_id(&it6151_id[1], client))
		{
		    if (!(it6151_1 = kmalloc(sizeof(struct i2c_client), GFP_KERNEL))) {
        		err = -ENOMEM;
        		goto exit;
    		      }    
			memset(it6151_1, 0, sizeof(struct i2c_client));
			it6151_1 = client;
			dev_info(&client->adapter->dev, "attached it6151_1: %s "
				"into i2c adapter successfully \n", id->name);
		}
	else
		{
			dev_err(&client->adapter->dev, "invalid i2c adapter: can not found dev_id matched\n");
			return -EIO;
		}
	
	printk("[it6151_i2c_probe] Suss \n");
	return ret;

exit:
    return err;

}


static struct i2c_driver it6151_i2c_driver = {
	.driver = {
		.name = DEVICE_NAME,
	},
	.probe = it6151_i2c_probe,
	.id_table = it6151_id,
};

static int __init it6151_i2c_init(void)
{    
   // int ret=0;  //fixed for build warning
    
    //battery_xlog_printk(BAT_LOG_CRTI,"[it6151_i2c_init] init start\n");
    printk("[it6151_i2c_init] init start\n");
    
    i2c_register_board_info(it6151_BUSNUM, it6151_i2c, 2);

    if(i2c_add_driver(&it6151_i2c_driver)!=0)
    {
        //battery_xlog_printk(BAT_LOG_CRTI,"[it6151_i2c_init] failed to register it6151 i2c driver.\n");
        printk("[it6151_i2c_init] failed to register it6151 i2c driver.\n");
    }
    else
    {
        //battery_xlog_printk(BAT_LOG_CRTI,"[it6151_i2c_init] Success to register it6151 i2c driver.\n");
        printk("[it6151_i2c_init] Success to register it6151 i2c driver.\n");
    }

    return 0;        
}

static void __exit it6151_i2c_exit(void)
{
    i2c_del_driver(&it6151_i2c_driver);
}

module_init(it6151_i2c_init);
module_exit(it6151_i2c_exit);
   
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C it6151 Driver");
MODULE_AUTHOR("jitao.shi<jitao.shi@mediatek.com>");
#endif

