#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#include "lcm_drv.h"

#ifdef BUILD_LK
	#include <platform/upmu_common.h>
	#include <platform/mt_gpio.h>
	#include <platform/mt_i2c.h> 
	#include <platform/mt_pmic.h>
	#include <string.h>
#elif defined(BUILD_UBOOT)
    #include <asm/arch/mt_gpio.h>
#else
	#include <mach/mt_pm_ldo.h>
    #include <mach/mt_gpio.h>
#endif

#include <cust_gpio_usage.h>
#include <cust_i2c.h>

#ifdef BUILD_LK
#define LCD_DEBUG(fmt)  dprintf(CRITICAL,fmt)
#else
#define LCD_DEBUG(fmt)  printk(fmt)
#endif


/*********************************************************
* Gate Driver
*********************************************************/
 
#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>  
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
//#include <linux/jiffies.h>
#include <linux/uaccess.h>
//#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
/***************************************************************************** 
 * Define
 *****************************************************************************/

#define TPS_I2C_BUSNUM  I2C_I2C_LCD_BIAS_CHANNEL//for I2C channel 0
#define I2C_ID_NAME "tps65132"
#define TPS_ADDR 0x3E

/***************************************************************************** 
 * GLobal Variable
 *****************************************************************************/
static struct i2c_board_info __initdata tps65132_board_info = {I2C_BOARD_INFO(I2C_ID_NAME, TPS_ADDR)};
static struct i2c_client *tps65132_i2c_client = NULL;


/***************************************************************************** 
 * Function Prototype
 *****************************************************************************/ 
static int tps65132_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tps65132_remove(struct i2c_client *client);
/***************************************************************************** 
 * Data Structure
 *****************************************************************************/

 struct tps65132_dev	{	
	struct i2c_client	*client;
	
};

static const struct i2c_device_id tps65132_id[] = {
	{ I2C_ID_NAME, 0 },
	{ }
};

//#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36))
//static struct i2c_client_address_data addr_data = { .forces = forces,};
//#endif
static struct i2c_driver tps65132_iic_driver = {
	.id_table	= tps65132_id,
	.probe		= tps65132_probe,
	.remove		= tps65132_remove,
	//.detect		= mt6605_detect,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "tps65132",
	},
 
};
/***************************************************************************** 
 * Extern Area
 *****************************************************************************/ 
 
 

/***************************************************************************** 
 * Function
 *****************************************************************************/ 
static int tps65132_probe(struct i2c_client *client, const struct i2c_device_id *id)
{  
	printk( "tps65132_iic_probe\n");
	printk("TPS: info==>name=%s addr=0x%x\n",client->name,client->addr);
	tps65132_i2c_client  = client;		
	return 0;      
}

static int tps65132_remove(struct i2c_client *client)
{  	
  printk( "tps65132_remove\n");
  tps65132_i2c_client = NULL;
   i2c_unregister_device(client);
  return 0;
}

 static int tps65132_write_bytes(unsigned char addr, unsigned char value)
{	
	int ret = 0;
	struct i2c_client *client = tps65132_i2c_client;
	char write_data[2]={0};	
	write_data[0]= addr;
	write_data[1] = value;
    ret=i2c_master_send(client, write_data, 2);
	if(ret<0)
	printk("tps65132 write data fail !!\n");	
	return ret ;
}

/*
 * module load/unload record keeping
 */

static int __init tps65132_iic_init(void)
{

   printk( "tps65132_iic_init\n");
   i2c_register_board_info(TPS_I2C_BUSNUM, &tps65132_board_info, 1);
   printk( "tps65132_iic_init2\n");
   i2c_add_driver(&tps65132_iic_driver);
   printk( "tps65132_iic_init success\n");	
   return 0;
}

static void __exit tps65132_iic_exit(void)
{
  printk( "tps65132_iic_exit\n");
  i2c_del_driver(&tps65132_iic_driver);  
}


module_init(tps65132_iic_init);
module_exit(tps65132_iic_exit);

MODULE_AUTHOR("Xiaokuan Shi");
MODULE_DESCRIPTION("MTK TPS65132 I2C Driver");
MODULE_LICENSE("GPL"); 

#endif


#ifdef BUILD_LK

#define TPS65132_SLAVE_ADDR_WRITE  0x7C  
static struct mt_i2c_t TPS65132_i2c;

static int TPS65132_write_byte(kal_uint8 addr, kal_uint8 value)
{
    kal_uint32 ret_code = I2C_OK;
    kal_uint8 write_data[2];
    kal_uint16 len;

    write_data[0]= addr;
    write_data[1] = value;

    TPS65132_i2c.id = I2C_I2C_LCD_BIAS_CHANNEL;//I2C2;
    /* Since i2c will left shift 1 bit, we need to set FAN5405 I2C address to >>1 */
    TPS65132_i2c.addr = (TPS65132_SLAVE_ADDR_WRITE >> 1);
    TPS65132_i2c.mode = ST_MODE;
    TPS65132_i2c.speed = 100;
    len = 2;

    ret_code = i2c_write(&TPS65132_i2c, write_data, len);
    //printf("%s: i2c_write: ret_code: %d\n", __func__, ret_code);

    return ret_code;
}

#endif



/*********************************************************
* LCM  Driver
*********************************************************/

static const unsigned char LCD_MODULE_ID = 0x01; //  haobing modified 2013.07.11
// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------
#define LCM_DSI_CMD_MODE 1
#define FRAME_WIDTH (1080)
#define FRAME_HEIGHT (1920)
#define GPIO_65132_EN GPIO_LCD_BIAS_ENP_PIN


#define REGFLAG_DELAY 0xFFFC
#define REGFLAG_UDELAY 0xFFFB

#define REGFLAG_END_OF_TABLE 0xFFFD   // END OF REGISTERS MARKER
#define REGFLAG_RESET_LOW 0xFFFE
#define REGFLAG_RESET_HIGH 0xFFFF

static LCM_DSI_MODE_SWITCH_CMD lcm_switch_mode_cmd;

#define UFO_ON_3X_60
//#define UFO_ON_3X_120

#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif

//static unsigned int lcm_esd_test = FALSE;      ///only for ESD test
// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------
static const unsigned int BL_MIN_LEVEL = 20;
static LCM_UTIL_FUNCS lcm_util;
static int lcm_fps = 60;
#define SET_RESET_PIN(v) (lcm_util.set_reset_pin((v)))
#define MDELAY(n)        (lcm_util.mdelay(n))
#define UDELAY(n)        (lcm_util.udelay(n))

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)										lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   			lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)
#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)

struct LCM_setting_table {
    unsigned int cmd;
    unsigned char count;
    unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28,0,{}},
	{0x10,0,{}},

	{REGFLAG_DELAY, 120, {}}
};

static struct LCM_setting_table lcm_60fps_setting[] = {  

    {0x00,1,{0x80}},
    //{0xc0,14,{0x00,0x7f,0x07,0xbf,0x08,0x00,0x7f,0x18,0x08,0x00,0x7f,0x00,0x18,0x08}},
    {0xc0,14,{0x00,0x80,0x07,0xb8,0x08,0x00,0x80,0x18,0x08,0x00,0x80,0x00,0x18,0x08}},

    {0x00,1,{0x00}},
    {0xFB,1,{0x01}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_120fps_setting[] = {
    {0x00,1,{0x80}},
    //{0xc0,14,{0x00,0x7f,0x00,0x0b,0x08,0x00,0x7f,0x0b,0x08,0x00,0x7f,0x00,0x0b,0x08}},
    {0xc0,14,{0x00,0x80,0x00,0x18,0x08,0x00,0x80,0x18,0x08,0x00,0x80,0x00,0x18,0x08}},

    {0x00,1,{0x00}},
    {0xFB,1,{0x01}},

    {REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_initialization_setting[] = {
	{0x00,1,{0x00}},
	{0xFF,3,{0x19,0x06,0x01}},
	
	{0x00,1,{0x80}},
	{0xFF,2,{0x19,0x06}},
	
	{0x00,1,{0x80}},

#ifdef UFO_ON_3X_120
    //{0xc0,14,{0x00,0x7f,0x00,0x0b,0x08,0x00,0x7f,0x0b,0x08,0x00,0x7f,0x00,0x0b,0x08}},
    {0xc0,14,{0x00,0x80,0x00,0x18,0x08,0x00,0x80,0x18,0x08,0x00,0x80,0x00,0x18,0x08}},
#else
    //{0xc0,14,{0x00,0x7f,0x07,0xbf,0x08,0x00,0x7f,0x18,0x08,0x00,0x7f,0x00,0x18,0x08}},
    {0xc0,14,{0x00,0x80,0x07,0xb8,0x08,0x00,0x80,0x18,0x08,0x00,0x80,0x00,0x18,0x08}},
#endif

	{0x00,1,{0xa0}},
	{0xc0,15,{0x00,0x00,0x00,0x00,0x01,0x1a,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	
	{0x00,1,{0xd0}},
	{0xc0,15,{0x00,0x00,0x00,0x00,0x01,0x1a,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	
	{0x00,1,{0x80}},
	{0xa4,7,{0x88,0x00,0x00,0x02,0x00,0x82,0x00}},
	
	{0x00,1,{0x80}},
	{0xa5,4,{0x0c,0x00,0x01,0x08}},
    
	{0x00,1,{0x80}},
	{0xc2,12,{0x83,0x01,0x78,0x61,0x83,0x01,0x78,0x61,0x00,0x00,0x00,0x00}},

	{0x00,1,{0x90}},
	{0xc2,12,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},

	{0x00,1,{0xa0}},
	{0xc2,13,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},

	{0x00,1,{0xb0}},
	{0xc2,15,{0x84,0x06,0x01,0x00,0x01,0x85,0x05,0x01,0x00,0x01,0x82,0x08,0x01,0x00,0x01}},

	{0x00,1,{0xc0}},
	{0xc2,15,{0x83,0x03,0x01,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},

	{0x00,1,{0xd0}},
	{0xc2,10,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},

	{0x00,1,{0xda}},
	{0xc2,5,{0x33,0x33,0x33,0x33,0x00}},

	{0x00,1,{0xe0}},
	{0xc2,11,{0x85,0x7c,0x04,0x01,0x0a,0x84,0x7c,0x04,0x01,0x0a,0x14}},

	{0x00,1,{0xf0}},
	{0xc2,15,{0x00,0x20,0x01,0x01,0x0a,0x00,0x20,0x01,0x01,0x0a,0x00,0x00,0x00,0x00,0x01}},

	{0x00,1,{0x80}},
	{0xc3,15,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x30,0x00,0x00}},

	{0x00,1,{0xa0}},
	{0xc3,12,{0x83,0x01,0x78,0x61,0x83,0x01,0x78,0x61,0x00,0x00,0x00,0x00}},

	{0x00,1,{0xb0}},
	{0xc3,14,{0x00,0x00,0x00,0x00,0x84,0x06,0x01,0x00,0x01,0x85,0x05,0x01,0x00,0x01}},

	{0x00,1,{0xc0}},
	{0xc3,15,{0x82,0x08,0x01,0x00,0x01,0x83,0x03,0x01,0x00,0x01,0x00,0x00,0x00,0x00,0x00}},

	{0x00,1,{0xd0}},
	{0xc3,15,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},

	{0x00,1,{0xe0}},
	{0xc3,15,{0x33,0x33,0x33,0x33,0x00,0x85,0x7c,0x04,0x00,0x0a,0x85,0x7c,0x04,0x00,0x0a}},

	{0x00,1,{0xf0}},
	{0xc3,15,{0x94,0x00,0x00,0x00,0x80,0x01,0x00,0x0a,0x00,0x80,0x01,0x00,0x0a,0x00,0x00}},

	{0x00,1,{0x80}},
	{0xcb,12,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},    
    
	{0x00,1,{0x90}},
	{0xcb,15,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}}, 

	{0x00,1,{0xa0}},
	{0xcb,15,{0x00,0x00,0x00,0xbf,0x00,0xff,0x00,0x00,0x00,0xff,0x00,0x00,0x00,0x00,0x00}}, 

	{0x00,1,{0xb0}},
	{0xcb,12,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x77,0x77,0x00,0x00}},    

	{0x00,1,{0xc0}},
	{0xcb,15,{0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x01,0x01,0x01,0x01,0x01,0x01,0x01}}, 

	{0x00,1,{0xd0}},
	{0xcb,15,{0x01,0x01,0x01,0xff,0x05,0xff,0x01,0x01,0x01,0xff,0x00,0x01,0x01,0x01,0x01}}, 

	{0x00,1,{0xe0}},
	{0xcb,12,{0x01,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x77,0x77,0x00,0x00}},    

	{0x00,1,{0xf0}},
	{0xcb,11,{0x00,0xcc,0xff,0xff,0xf5,0xfc,0x0c,0x33,0x03,0x00,0x22}}, 

	{0x00,1,{0x80}},
	{0xcc,12,{0x08,0x09,0x18,0x19,0x0c,0x0d,0x0e,0x0f,0x07,0x07,0x07,0x07}},    

	{0x00,1,{0x90}},
	{0xcc,12,{0x09,0x08,0x19,0x18,0x0f,0x0e,0x0d,0x0c,0x07,0x07,0x07,0x07}},    

	{0x00,1,{0xa0}},
	{0xcc,15,{0x14,0x15,0x16,0x17,0x1c,0x1d,0x1e,0x1f,0x20,0x01,0x02,0x03,0x07,0x07,0x00}}, 

	{0x00,1,{0xb0}},
	{0xcc,9,{0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x07,0x00}},

	{0x00,1,{0x80}},
	{0xcd,15,{0x02,0x11,0x12,0x05,0x07,0x1A,0x19,0x1A,0x1A,0x1A,0x24,0x24,0x24,0x1d,0x1e}}, 

	{0x00,1,{0x90}},
	{0xcd,3,{0x1f,0x24,0x24}},  

	{0x00,1,{0xa0}},
	{0xcd,15,{0x01,0x11,0x12,0x06,0x08,0x1A,0x19,0x1A,0x1A,0x1A,0x24,0x24,0x24,0x1d,0x1e}}, 

	{0x00,1,{0xb0}},
	{0xcd,3,{0x1f,0x24,0x24}},

	{0x00,1,{0x00}},
	{0xD8,2,{0x22,0x22}},

	{0x00,1,{0x91}},
	{0xC5,2,{0x1E,0x28}},

	{0x00,1,{0x00}},
	{0xE1,24,{0x00,0x0E,0x19,0x26,0x30,0x37,0x44,0x55,0x61,0x72,0x7E,0x86,0x73,0x6E,0x69,0x5C,0x4B,0x3B,0x2F,0x29,0x21,0x17,0x13,0x00}},
	{0x00,1,{0x00}},

	{0x00,1,{0x00}},
	{0xE2,24,{0x00,0x0E,0x19,0x26,0x30,0x37,0x44,0x55,0x61,0x72,0x7E,0x86,0x73,0x6E,0x69,0x5C,0x4B,0x3B,0x2F,0x29,0x21,0x17,0x13,0x00}},
	{0x00,1,{0x00}},

	{0x00,1,{0x00}},
	{0xE3,24,{0x00,0x0E,0x19,0x26,0x30,0x37,0x44,0x55,0x61,0x72,0x7E,0x86,0x73,0x6E,0x69,0x5C,0x4B,0x3B,0x2F,0x29,0x21,0x17,0x13,0x00}},
	{0x00,1,{0x00}},

	{0x00,1,{0x00}},
	{0xE4,24,{0x00,0x0E,0x19,0x26,0x30,0x37,0x44,0x55,0x61,0x72,0x7E,0x86,0x73,0x6E,0x69,0x5C,0x4B,0x3B,0x2F,0x29,0x21,0x17,0x13,0x00}},
	{0x00,1,{0x00}},

	{0x00,1,{0x00}},
	{0xE5,24,{0x00,0x0E,0x19,0x26,0x30,0x37,0x44,0x55,0x61,0x72,0x7E,0x86,0x73,0x6E,0x69,0x5C,0x4B,0x3B,0x2F,0x29,0x21,0x17,0x13,0x00}},
	{0x00,1,{0x00}},

	{0x00,1,{0x00}},
	{0xE6,24,{0x00,0x0E,0x19,0x26,0x30,0x37,0x44,0x55,0x61,0x72,0x7E,0x86,0x73,0x6E,0x69,0x5C,0x4B,0x3B,0x2F,0x29,0x21,0x17,0x13,0x00}},
	{0x00,1,{0x00}},

	{0x00,1,{0x00}},
	{0xD9,8,{0x00,0xc3,0x00,0xc3,0x00,0xc3,0x00,0xc3}}, // VCOMDC
    
	{0x00,1,{0xA0}},                // for video mode
	{0xC1,3,{0x00,0xc0,0x11}},
#if 0
//#if (defined USING_UFO_3X_60)
	{0x00,1,{0x00}},
	{0x1C,1,{0x04}},	

   //UFO decompression parameter setting 
	{0x00,1,{0x00}},
	{0xb8,8,{0x80,0x0d,0x01,0x05,0x00,0x00,0x00,0x01}},
      			
// TP off 
	{0x00,1,{0x80}},
	{0xa4,1,{0x00}},
	
//---------- 60 Hz -------------------------------------------------------------------------	
	{0x00,1,{0x80}},
	{0xC1,4,{0x11,0x11,0x00,0x30}},
			
//OSC trim
	{0x00,1,{0x83}},
	{0xf4,1,{0x00}},

//VDD18& LVDSVDD
	{0x00,1,{0xC1}},
	{0xC5,1,{0x75}},
//-----------------------------------------------------------------------------------
#endif

#if (defined UFO_ON_3X_60) || (defined UFO_ON_3X_120)
	{0x00,1,{0x00}},
	{0x1C,1,{0x04}},	

//UFO decompression parameter setting 
	{0x00,1,{0x00}},
	{0xb8,8,{0x80,0x0d,0x01,0x05,0x00,0x00,0x00,0x01}},		
// TP off 
	{0x00,1,{0x80}},
	{0xa4,1,{0x00}},
	
//---------- 120 Hz -------------------------------------------------------------------------	
//OSC selection + turbo + SSC force +3%

	{0x00,1,{0x80}},
	{0xC1,4,{0x77,0x77,0x01,0x33}},
			
//OSC trim
	{0x00,1,{0x83}},
	{0xf4,1,{0x0f}},

//VDD18& LVDSVDD
	{0x00,1,{0xC1}},
	{0xC5,1,{0xff}},
//-----------------------------------------------------------------------------------
#else
#if (LCM_DSI_CMD_MODE)
    {0x00,1,{0x00}},
    {0x1C,1,{0x00}},    // by pass
#else
    {0x00,1,{0x00}},
    {0x1C,1,{0x03}},    // by pass
#endif
#endif

    {0x35,1,{0x00}},             
                  
    {0x11,0,{}},    // Sleep out 
    {REGFLAG_DELAY, 120, {}},             
    {0x29,0,{}},    // Display on

    {REGFLAG_END_OF_TABLE, 0x00, {}}	
};


static struct LCM_setting_table lcm_backlight_level_setting[] = {
    {0x51, 1, {0xFF}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void push_table(void * cmdq, struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
    unsigned int i;
    for(i = 0; i < count; i++)
    {
        unsigned cmd;
        cmd = table[i].cmd;

        switch (cmd) {

            case REGFLAG_DELAY :
                if(table[i].count <= 10)
                    MDELAY(table[i].count);
                else
                    MDELAY(table[i].count);
                break;
				
			case REGFLAG_UDELAY :
				UDELAY(table[i].count);
				break;

            case REGFLAG_END_OF_TABLE :
                break;

            default:
                dsi_set_cmdq_V22(cmdq, cmd, table[i].count, table[i].para_list, force_update);
        }
    }
}

// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type   = LCM_TYPE_DSI;

	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode   = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
#else
    params->dsi.mode   = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
#endif
	params->dsi.switch_mode_enable = 0;

	// DSI
	/* Command mode setting */
	params->dsi.LANE_NUM				    = LCM_FOUR_LANE;
	//The following defined the fomat for data coming from LCD engine.
	params->dsi.data_format.color_order 	= LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   	= LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     	= LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format          = LCM_DSI_FORMAT_RGB888;

	// Highly depends on LCD driver capability.
	params->dsi.packet_size=256;
	//video mode timing

	params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active				= 1;
	params->dsi.vertical_backporch					= 5;
	params->dsi.vertical_frontporch					= 6;
	params->dsi.vertical_active_line				= FRAME_HEIGHT;

	params->dsi.horizontal_sync_active				= 4;
	params->dsi.horizontal_backporch				= 42;
	params->dsi.horizontal_frontporch				= 42;
	
	params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

    /*command mode clock*/
#if (LCM_DSI_CMD_MODE)
#if (defined UFO_ON_3X_60) || (defined UFO_ON_3X_120)
    params->dsi.PLL_CLOCK = 340;
#else
    params->dsi.PLL_CLOCK = 500; //this value must be in MTK suggested table
#endif
#endif

    /*video mode clock*/
#if (!LCM_DSI_CMD_MODE)
#if (defined UFO_ON_3X_60) || (defined UFO_ON_3X_120)
    params->dsi.PLL_CLOCK = 340;
#else
    params->dsi.PLL_CLOCK = 450; //this value must be in MTK suggested table
#endif
#endif

#if (defined UFO_ON_3X_60) || (defined UFO_ON_3X_120)
	params->dsi.ufoe_enable = 1;
	params->dsi.ufoe_params.compress_ratio = 3;
	params->dsi.ufoe_params.vlc_disable = 0;
	params->dsi.horizontal_active_pixel	= FRAME_WIDTH/3;
#endif
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd          = 0x53;
	params->dsi.lcm_esd_check_table[0].count        = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x24;

}

static void lcm_init_power(void)
{
#ifdef BUILD_LK
	mt6331_upmu_set_rg_vgp1_en(1);
#else
	printk("%s, begin\n", __func__);
	hwPowerOn(MT6331_POWER_LDO_VGP1, VOL_DEFAULT, "LCM_DRV");	
	printk("%s, end\n", __func__);
#endif
}

static void lcm_suspend_power(void)
{
#ifdef BUILD_LK
	mt6331_upmu_set_rg_vgp1_en(0);
#else
	printk("%s, begin\n", __func__);
	hwPowerDown(MT6331_POWER_LDO_VGP1, "LCM_DRV");	
	printk("%s, end\n", __func__);
#endif

}

static void lcm_resume_power(void)
{
#ifdef BUILD_LK
	mt6331_upmu_set_rg_vgp1_en(1);
#else
	printk("%s, begin\n", __func__);
	hwPowerOn(MT6331_POWER_LDO_VGP1, VOL_DEFAULT, "LCM_DRV");	
	printk("%s, end\n", __func__);
#endif
}

static void lcm_init(void)
{
	unsigned char cmd = 0x0;
	unsigned char data = 0xFF;
	int ret=0;
	cmd=0x00;
	data=0x0E;

	mt_set_gpio_mode(GPIO_65132_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_EN, GPIO_OUT_ONE);

#ifdef BUILD_LK
	ret=TPS65132_write_byte(cmd,data);
    if(ret)    	
    dprintf(0, "[LK]nt35595----tps6132----cmd=%0x--i2c write error----\n",cmd);    	
	else
	dprintf(0, "[LK]nt35595----tps6132----cmd=%0x--i2c write success----\n",cmd);    		
#else
	ret=tps65132_write_bytes(cmd,data);
	if(ret<0)
	printk("[KERNEL]nt35595----tps6132---cmd=%0x-- i2c write error-----\n",cmd);
	else
	printk("[KERNEL]nt35595----tps6132---cmd=%0x-- i2c write success-----\n",cmd);
#endif

	cmd=0x01;
	data=0x0E;
#ifdef BUILD_LK
	ret=TPS65132_write_byte(cmd,data);
    if(ret)    	
	    dprintf(0, "[LK]nt35595----tps6132----cmd=%0x--i2c write error----\n",cmd);    	
	else
		dprintf(0, "[LK]nt35595----tps6132----cmd=%0x--i2c write success----\n",cmd);   
#else
	ret=tps65132_write_bytes(cmd,data);
	if(ret<0)
	printk("[KERNEL]nt35595----tps6132---cmd=%0x-- i2c write error-----\n",cmd);
	else
	printk("[KERNEL]nt35595----tps6132---cmd=%0x-- i2c write success-----\n",cmd);
#endif
	SET_RESET_PIN(1);
	MDELAY(1);
	SET_RESET_PIN(0);
	MDELAY(10);

	SET_RESET_PIN(1);
	MDELAY(10);
#ifdef  UFO_ON_3X_120
    lcm_fps  = 120;
#else
    lcm_fps  = 60;
#endif
	 push_table(0,lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void)
{
	mt_set_gpio_mode(GPIO_65132_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_EN, GPIO_OUT_ZERO);
	push_table(0,lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);  
	SET_RESET_PIN(0);
	MDELAY(10);
}

static void lcm_resume(void)
{
	lcm_init(); 
}

static void lcm_update(unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0>>8)&0xFF);
	unsigned char x0_LSB = (x0&0xFF);
	unsigned char x1_MSB = ((x1>>8)&0xFF);
	unsigned char x1_LSB = (x1&0xFF);
	unsigned char y0_MSB = ((y0>>8)&0xFF);
	unsigned char y0_LSB = (y0&0xFF);
	unsigned char y1_MSB = ((y1>>8)&0xFF);
	unsigned char y1_LSB = (y1&0xFF);

	unsigned int data_array[16];

	data_array[0]= 0x00053902;
	data_array[1]= (x1_MSB<<24)|(x0_LSB<<16)|(x0_MSB<<8)|0x2a;
	data_array[2]= (x1_LSB);
	//dsi_set_cmdq(data_array, 3, 1);
	
	data_array[0]= 0x00053902;
	data_array[1]= (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
	data_array[2]= (y1_LSB);
	//dsi_set_cmdq(data_array, 3, 1);
	
	data_array[0]= 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);	
}

#define LCM_OTM1906A_ID_ADD  (0xDA)
#define LCM_OTM1906A_ID     (0x40)

static unsigned int lcm_compare_id(void)
{

	unsigned int id=0;
	unsigned char buffer[2];
	unsigned int array[16];  

	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);
	
	SET_RESET_PIN(1);
	MDELAY(20); 

	array[0] = 0x00023700;// read id return two byte,version and id
	dsi_set_cmdq(array, 1, 1);
	
	read_reg_v2(LCM_OTM1906A_ID_ADD, buffer, 2);
	id = buffer[0]; //we only need ID
#ifdef BUILD_LK
	dprintf(0, "%s, LK otm1906a debug: otm1906a id = 0x%08x\n", __func__, id);
#else
	printk("%s, kernel otm1906a horse debug: otm1906a id = 0x%08x\n", __func__, id);
#endif

	if(id == LCM_OTM1906A_ID)
		return 1;
	else
		return 0;

}

// return TRUE: need recovery
// return FALSE: No need recovery

static unsigned int lcm_esd_check(void)
{
#ifndef BUILD_LK
	char  buffer[3];
	int   array[4];

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x53, buffer, 1);

	if(buffer[0] != 0x24)
	{
		printk("[LCM ERROR] [0x53]=0x%02x\n", buffer[0]);
		return TRUE;
	}
	else
	{
		printk("[LCM NORMAL] [0x53]=0x%02x\n", buffer[0]);
		return FALSE;
	}
#else
	return FALSE;
#endif

}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	unsigned int ret = 0;
	unsigned int x0 = FRAME_WIDTH/4;
	unsigned int x1 = FRAME_WIDTH*3/4;

	unsigned char x0_MSB = ((x0>>8)&0xFF);
	unsigned char x0_LSB = (x0&0xFF);
	unsigned char x1_MSB = ((x1>>8)&0xFF);
	unsigned char x1_LSB = (x1&0xFF);

	unsigned int data_array[3];
	unsigned char read_buf[4];
	printk("ATA check size = 0x%x,0x%x,0x%x,0x%x\n",x0_MSB,x0_LSB,x1_MSB,x1_LSB);
	data_array[0]= 0x0005390A;//HS packet
	data_array[1]= (x1_MSB<<24)|(x0_LSB<<16)|(x0_MSB<<8)|0x2a;
	data_array[2]= (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00043700;// read id return two byte,version and id
	dsi_set_cmdq(data_array, 1, 1);
	
	read_reg_v2(0x2A, read_buf, 4);

	if((read_buf[0] == x0_MSB) && (read_buf[1] == x0_LSB) 
		&& (read_buf[2] == x1_MSB) && (read_buf[3] == x1_LSB))
		ret = 1;
	else
		ret = 0;

	x0 = 0;
	x1 = FRAME_WIDTH - 1;

	x0_MSB = ((x0>>8)&0xFF);
	x0_LSB = (x0&0xFF);
	x1_MSB = ((x1>>8)&0xFF);
	x1_LSB = (x1&0xFF);

	data_array[0]= 0x0005390A;//HS packet
	data_array[1]= (x1_MSB<<24)|(x0_LSB<<16)|(x0_MSB<<8)|0x2a;
	data_array[2]= (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	return ret;
#else
	return 0;
#endif
}

static void lcm_setbacklight(unsigned int level)
{
#ifdef BUILD_LK
	dprintf(0,"%s,lk nt35595 backlight: level = %d\n", __func__, level);
#else
	printk("%s, kernel nt35595 backlight: level = %d\n", __func__, level);
#endif
	// Refresh value of backlight level.
	lcm_backlight_level_setting[0].para_list[0] = level;
	
	push_table(0,lcm_backlight_level_setting, sizeof(lcm_backlight_level_setting) / sizeof(struct LCM_setting_table), 1);

}

static void* lcm_switch_mode(int mode)
{
#ifndef BUILD_LK
//customization: 1. V2C config 2 values, C2V config 1 value; 2. config mode control register
	if(mode == 0)
	{//V2C
		lcm_switch_mode_cmd.mode = CMD_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;// mode control addr
		lcm_switch_mode_cmd.val[0]= 0x13;//enabel GRAM firstly, ensure writing one frame to GRAM
		lcm_switch_mode_cmd.val[1]= 0x10;//disable video mode secondly
	}
	else
	{//C2V
		lcm_switch_mode_cmd.mode = SYNC_PULSE_VDO_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;
		lcm_switch_mode_cmd.val[0]= 0x03;//disable GRAM and enable video mode
	}
	return (void*)(&lcm_switch_mode_cmd);
#else
	return NULL;
#endif
}


static int lcm_adjust_fps(void * cmdq, int fps)
{

#ifdef BUILD_LK
    dprintf(0,"%s:from %d to %d\n", __func__, lcm_fps, fps);
#else
    printk("%s:from %d to %d\n", __func__, lcm_fps, fps);
#endif

    if(lcm_fps == fps)
        return 0;

    if(fps == 60)
    {
        lcm_fps = 60;
	    push_table(cmdq, lcm_60fps_setting, sizeof(lcm_60fps_setting) / sizeof(struct LCM_setting_table), 1);
    }
    else if(fps == 120)
    {
        lcm_fps = 120;
	    push_table(cmdq, lcm_120fps_setting, sizeof(lcm_120fps_setting) / sizeof(struct LCM_setting_table), 1);
    }
    else
    {
       return -1;
        
    }
    return 0;

}


LCM_DRIVER otm1906a_fhd_dsi_cmd_auto_lcm_drv=
{
    .name           	= "otm1906a_fhd_dsi_cmd_auto",
    .set_util_funcs 	= lcm_set_util_funcs,
    .get_params     	= lcm_get_params,
    .init           	= lcm_init,
    .suspend        	= lcm_suspend,
    .resume         	= lcm_resume,
     .compare_id     	= lcm_compare_id,
     .init_power		= lcm_init_power,
     .resume_power      = lcm_resume_power,
     .suspend_power     = lcm_suspend_power,
     .adjust_fps        = lcm_adjust_fps,
     .esd_check         = lcm_esd_check,
     .set_backlight     = lcm_setbacklight,
	 .ata_check		    = lcm_ata_check,
	 .update            = lcm_update,
     .switch_mode		= lcm_switch_mode,
};

