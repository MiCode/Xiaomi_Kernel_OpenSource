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
#include <cust_pmic.h>

#include "tps6128x.h"
#include "cust_charging.h"
#include <mach/charging.h>

#if defined(CONFIG_MTK_FPGA)
#else
#include <cust_i2c.h>
#endif

/**********************************************************
  *
  *   [I2C Slave Setting] 
  *
  *********************************************************/
#define tps6128x_SLAVE_ADDR_WRITE   0xEA
#define tps6128x_SLAVE_ADDR_Read    0xEB

#ifdef I2C_EXT_VBAT_BOOST_CHANNEL
#define tps6128x_BUSNUM I2C_EXT_VBAT_BOOST_CHANNEL
#else
#define tps6128x_BUSNUM 0//1
#endif

static struct i2c_client *new_client = NULL;
static const struct i2c_device_id tps6128x_i2c_id[] = {{"tps6128x",0},{}};   
static int tps6128x_driver_probe(struct i2c_client *client, const struct i2c_device_id *id);

static struct i2c_driver tps6128x_driver = {
    .driver = {
        .name    = "tps6128x",
    },
    .probe       = tps6128x_driver_probe,
    .id_table    = tps6128x_i2c_id,
};

/**********************************************************
  *
  *   [Global Variable] 
  *
  *********************************************************/
kal_uint8 tps6128x_reg[tps6128x_REG_NUM] = {0};

static DEFINE_MUTEX(tps6128x_i2c_access);

int g_tps6128x_driver_ready=0;
int g_tps6128x_hw_exist=0;

/**********************************************************
  *
  *   [I2C Function For Read/Write tps6128x] 
  *
  *********************************************************/
kal_uint32 tps6128x_read_byte(kal_uint8 cmd, kal_uint8 *returnData)
{
    char     cmd_buf[1]={0x00};
    char     readData = 0;
    int      ret=0;

    mutex_lock(&tps6128x_i2c_access);
    
    //new_client->addr = ((new_client->addr) & I2C_MASK_FLAG) | I2C_WR_FLAG;    
    new_client->ext_flag=((new_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_WR_FLAG | I2C_DIRECTION_FLAG;

    cmd_buf[0] = cmd;
    ret = i2c_master_send(new_client, &cmd_buf[0], (1<<8 | 1));
    if (ret < 0) 
    {    
        //new_client->addr = new_client->addr & I2C_MASK_FLAG;
        new_client->ext_flag=0;

        mutex_unlock(&tps6128x_i2c_access);
        return 0;
    }
    
    readData = cmd_buf[0];
    *returnData = readData;

    // new_client->addr = new_client->addr & I2C_MASK_FLAG;
    new_client->ext_flag=0;
    
    mutex_unlock(&tps6128x_i2c_access);    
    return 1;
}

kal_uint32 tps6128x_write_byte(kal_uint8 cmd, kal_uint8 writeData)
{
    char    write_data[2] = {0};
    int     ret=0;
    
    mutex_lock(&tps6128x_i2c_access);
    
    write_data[0] = cmd;
    write_data[1] = writeData;
    
    new_client->ext_flag=((new_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_DIRECTION_FLAG;
    
    ret = i2c_master_send(new_client, write_data, 2);
    if (ret < 0) 
    {
       
        new_client->ext_flag=0;
        mutex_unlock(&tps6128x_i2c_access);
        return 0;
    }
    
    new_client->ext_flag=0;
    mutex_unlock(&tps6128x_i2c_access);
    return 1;
}

/**********************************************************
  *
  *   [Read / Write Function] 
  *
  *********************************************************/
kal_uint32 tps6128x_read_interface (kal_uint8 RegNum, kal_uint8 *val, kal_uint8 MASK, kal_uint8 SHIFT)
{
    kal_uint8 tps6128x_reg = 0;
    kal_uint32 ret = 0;

    battery_xlog_printk(BAT_LOG_FULL,"--------------------------------------------------\n");

    ret = tps6128x_read_byte(RegNum, &tps6128x_reg);

    battery_xlog_printk(BAT_LOG_FULL,"[tps6128x_read_interface] Reg[%x]=0x%x\n", RegNum, tps6128x_reg);
	
    tps6128x_reg &= (MASK << SHIFT);
    *val = (tps6128x_reg >> SHIFT);
	
    battery_xlog_printk(BAT_LOG_FULL,"[tps6128x_read_interface] val=0x%x\n", *val);
	
    return ret;
}

kal_uint32 tps6128x_config_interface (kal_uint8 RegNum, kal_uint8 val, kal_uint8 MASK, kal_uint8 SHIFT)
{
    kal_uint8 tps6128x_reg = 0;
    kal_uint32 ret = 0;

    battery_xlog_printk(BAT_LOG_FULL,"--------------------------------------------------\n");

    ret = tps6128x_read_byte(RegNum, &tps6128x_reg);
    battery_xlog_printk(BAT_LOG_FULL,"[tps6128x_config_interface] Reg[%x]=0x%x\n", RegNum, tps6128x_reg);
    
    tps6128x_reg &= ~(MASK << SHIFT);
    tps6128x_reg |= (val << SHIFT);

    ret = tps6128x_write_byte(RegNum, tps6128x_reg);
    battery_xlog_printk(BAT_LOG_FULL,"[tps6128x_config_interface] write Reg[%x]=0x%x\n", RegNum, tps6128x_reg);

    // Check
    //tps6128x_read_byte(RegNum, &tps6128x_reg);
    //printk("[tps6128x_config_interface] Check Reg[%x]=0x%x\n", RegNum, tps6128x_reg);

    return ret;
}

//write one register directly
kal_uint32 tps6128x_reg_config_interface (kal_uint8 RegNum, kal_uint8 val)
{   
    kal_uint32 ret = 0;
    
    ret = tps6128x_write_byte(RegNum, val);

    return ret;
}

/**********************************************************
  *
  *   [Internal Function] 
  *
  *********************************************************/
void tps6128x_dump_register(void)
{
    kal_uint8 i=0;
    printk("[tps6128x] ");
    for (i=0;i<tps6128x_REG_NUM;i++)
    {
        tps6128x_read_byte(i, &tps6128x_reg[i]);
        printk("[0x%x]=0x%x ", i, tps6128x_reg[i]);        
    }
    printk("\n");
}

int is_tps6128x_sw_ready(void)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","g_tps6128x_driver_ready=%d\n", g_tps6128x_driver_ready);
    
    return g_tps6128x_driver_ready;
}

int is_tps6128x_exist(void)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","g_tps6128x_hw_exist=%d\n", g_tps6128x_hw_exist);
    
    return g_tps6128x_hw_exist;
}

void tps6128x_hw_component_detect(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=tps6128x_read_interface(0x3,&val,0xFF, 0);
    
    if(val == 0)
        g_tps6128x_hw_exist=0;
    else
        g_tps6128x_hw_exist=1;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[tps6128x_hw_component_detect] exist=%d, Reg[0x3]=0x%x\n", 
        g_tps6128x_hw_exist, val);
}

void tps6128x_hw_init(void)
{     
	kal_uint32 ret=0;
	kal_uint8 chip_version=0;
	
	ret=tps6128x_read_interface(0x0,&chip_version,0xFF, 0);
	
     //if(chip_version==0x02)  //tps61280 0x02; tps61280a 0x03,Cause 95+32 Swtich charger,when enable power path, it always happen Vsys slowly rises to 1.5v in 70ms
	{	 
		battery_xlog_printk(BAT_LOG_CRTI,"[tps6128x_hw_init] sw workaround for tps61280 no-A chip,wait for 10ms\n");
		ret=tps6128x_config_interface(0x3, 0x1F, 0x1F, 0); // Output voltage threshold = 4.4v
		msleep(10);//wait for more than 5ms
	}

    tps6128x_config_interface(0x1, 0x1, 0x3, 0); // MODE_CTRL[1:0]=01 ,PFMAuto mode

    battery_xlog_printk(BAT_LOG_CRTI,"[tps6128x_hw_init] From Johnson\n");
    tps6128x_config_interface(0x3, 0xB, 0x1F, 0); // Output voltage threshold = 3.4V
    tps6128x_config_interface(0x4, 0xF, 0xF,  0); // OC_input=max    

    battery_xlog_printk(BAT_LOG_CRTI,"[tps6128x_hw_init] After HW init\n");    
    tps6128x_dump_register();
}

static int tps6128x_driver_probe(struct i2c_client *client, const struct i2c_device_id *id) 
{             
    int err=0; 

    battery_xlog_printk(BAT_LOG_CRTI,"[tps6128x_driver_probe] \n");

    if (!(new_client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL))) {
        err = -ENOMEM;
        goto exit;
    }    
    memset(new_client, 0, sizeof(struct i2c_client));

    new_client = client;    

    //---------------------
    tps6128x_hw_component_detect(); 
    if(g_tps6128x_hw_exist==1)
    {
        tps6128x_dump_register();
        tps6128x_hw_init();    
    }
    g_tps6128x_driver_ready=1;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[tps6128x_driver_probe] g_tps6128x_hw_exist=%d, g_tps6128x_driver_ready=%d\n", 
        g_tps6128x_hw_exist, g_tps6128x_driver_ready);

    if(g_tps6128x_hw_exist==0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[tps6128x_driver_probe] return err\n");
        return err;
    }
        
    return 0;                                                                                       

exit:
    return err;

}

/**********************************************************
  *
  *   [platform_driver API] 
  *
  *********************************************************/
#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA) || defined(DISABLE_TPS6128X)
//
#else
kal_uint8 g_reg_value_tps6128x=0;
static ssize_t show_tps6128x_access(struct device *dev,struct device_attribute *attr, char *buf)
{
    battery_xlog_printk(BAT_LOG_CRTI,"[show_tps6128x_access] 0x%x\n", g_reg_value_tps6128x);
    return sprintf(buf, "%u\n", g_reg_value_tps6128x);
}
static ssize_t store_tps6128x_access(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    int ret=0;
    char *pvalue = NULL;
    unsigned int reg_value = 0;
    unsigned int reg_address = 0;
    
    battery_xlog_printk(BAT_LOG_CRTI,"[store_tps6128x_access] \n");
    
    if(buf != NULL && size != 0)
    {
        battery_xlog_printk(BAT_LOG_CRTI,"[store_tps6128x_access] buf is %s \n",buf);
        reg_address = simple_strtoul(buf,&pvalue,16);
        
        if(size > 3)
        {        
            reg_value = simple_strtoul((pvalue+1),NULL,16);        
            battery_xlog_printk(BAT_LOG_CRTI,"[store_tps6128x_access] write tps6128x reg 0x%x with value 0x%x !\n",reg_address,reg_value);
            ret=tps6128x_config_interface(reg_address, reg_value, 0xFF, 0x0);
        }
        else
        {    
            ret=tps6128x_read_interface(reg_address, &g_reg_value_tps6128x, 0xFF, 0x0);
            battery_xlog_printk(BAT_LOG_CRTI,"[store_tps6128x_access] read tps6128x reg 0x%x with value 0x%x !\n",reg_address,g_reg_value_tps6128x);
            battery_xlog_printk(BAT_LOG_CRTI,"[store_tps6128x_access] Please use \"cat tps6128x_access\" to get value\r\n");
        }        
    }    
    return size;
}
static DEVICE_ATTR(tps6128x_access, 0664, show_tps6128x_access, store_tps6128x_access); //664

static int tps6128x_user_space_probe(struct platform_device *dev)    
{    
    int ret_device_file = 0;

    battery_xlog_printk(BAT_LOG_CRTI,"******** tps6128x_user_space_probe!! ********\n" );
    
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_tps6128x_access);
    
    return 0;
}

struct platform_device tps6128x_user_space_device = {
    .name   = "tps6128x-user",
    .id     = -1,
};

static struct platform_driver tps6128x_user_space_driver = {
    .probe      = tps6128x_user_space_probe,
    .driver     = {
        .name = "tps6128x-user",
    },
};

static struct i2c_board_info __initdata i2c_tps6128x = { I2C_BOARD_INFO("tps6128x", (tps6128x_SLAVE_ADDR_WRITE>>1))};
#endif

static int __init tps6128x_init(void)
{    
#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA) || defined(DISABLE_TPS6128X)
    battery_xlog_printk(BAT_LOG_CRTI,"[tps6128x_init] not support or disable\n");
    g_tps6128x_hw_exist=0;
    g_tps6128x_driver_ready=1;
    
#else
    int ret=0;
    battery_xlog_printk(BAT_LOG_CRTI,"[tps6128x_init] init start. ch=%d\n", tps6128x_BUSNUM);
    
    i2c_register_board_info(tps6128x_BUSNUM, &i2c_tps6128x, 1);

    if(i2c_add_driver(&tps6128x_driver)!=0)
    {
        battery_xlog_printk(BAT_LOG_CRTI,"[tps6128x_init] failed to register tps6128x i2c driver.\n");
    }
    else
    {
        battery_xlog_printk(BAT_LOG_CRTI,"[tps6128x_init] Success to register tps6128x i2c driver.\n");
    }

    // tps6128x user space access interface
    ret = platform_device_register(&tps6128x_user_space_device);
    if (ret) {
        battery_xlog_printk(BAT_LOG_CRTI,"****[tps6128x_init] Unable to device register(%d)\n", ret);
        return ret;
    }    
    ret = platform_driver_register(&tps6128x_user_space_driver);
    if (ret) {
        battery_xlog_printk(BAT_LOG_CRTI,"****[tps6128x_init] Unable to register driver (%d)\n", ret);
        return ret;
    }
#endif    
    
    return 0;        
}

static void __exit tps6128x_exit(void)
{
    i2c_del_driver(&tps6128x_driver);
}

module_init(tps6128x_init);
module_exit(tps6128x_exit);
   
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C tps6128x Driver");
MODULE_AUTHOR("James Lo<james.lo@mediatek.com>");
