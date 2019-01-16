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
#ifndef CONFIG_FPGA_EARLY_PORTING
#include <cust_i2c.h>
#endif
#ifdef BUILD_LK
#define LCD_DEBUG(fmt)  dprintf(CRITICAL,fmt)
#else
#define LCD_DEBUG(fmt)  printk(fmt)
#endif


static const unsigned int BL_MIN_LEVEL =20;
static LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))
#define MDELAY(n) 											(lcm_util.mdelay(n))
#define UDELAY(n) 											(lcm_util.udelay(n))


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)										lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   			lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)    

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
#ifndef CONFIG_FPGA_EARLY_PORTING
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
#endif

//static unsigned char lcd_id_pins_value = 0xFF;
static const unsigned char LCD_MODULE_ID = 0x01; //  haobing modified 2013.07.11
// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------
#define LCM_DSI_CMD_MODE									1
#ifdef CONFIG_FPGA_EARLY_PORTING
#define FRAME_WIDTH  										(480)
#define FRAME_HEIGHT 										(800)
#else
#define FRAME_WIDTH  										(480)
#define FRAME_HEIGHT 										(854)
#endif
#ifndef CONFIG_FPGA_EARLY_PORTING
#define GPIO_65132_EN GPIO_LCD_BIAS_ENP_PIN
#endif

#define REGFLAG_DELAY             								0xFC
#define REGFLAG_UDELAY             								0xFB

#define REGFLAG_END_OF_TABLE      							0xFD   // END OF REGISTERS MARKER
#define REGFLAG_RESET_LOW       								0xFE
#define REGFLAG_RESET_HIGH      								0xFF

static LCM_DSI_MODE_SWITCH_CMD lcm_switch_mode_cmd;

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


struct LCM_setting_table {
    unsigned char cmd;
    unsigned char count;
    unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28,0,{}},
	{0x10,0,{}},
	{REGFLAG_DELAY, 120, {}},
	{0x4F,1,{0x01}},
	{REGFLAG_DELAY, 120, {}}
};
	
//update initial param for IC nt35520 0.01
static struct LCM_setting_table lcm_initialization_setting[] = {
	{0xFF,1,{0x24}},	//	Return	To	CMD1		
	{0x6E,1,{0x10}},	//	Return	To	CMD1		
	{0xFB,1,{0x01}},	//	Return	To	CMD1		
	{0xFF,1,{0x10}},	//	Return	To	CMD1		

	{0xFF,1,{0x10}},	//	Return	To	CMD1		
	{REGFLAG_UDELAY, 1, {}},	
#if (LCM_DSI_CMD_MODE)
	{0xBB,1,{0x10}},						
#else
	{0xBB,1,{0x03}},						
#endif
	{0x3B,5,{0x03,0x0A,0x0A,0x0A,0x0A}},	
	{0x53,1,{0x24}},						
	{0x55,1,{0x00}},						
	{0x5E,1,{0x00}},						
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
					
	{0x11,0,{}},						
	{REGFLAG_DELAY, 120, {}},
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	{0xFF,1,{0x24}},	//	CMD2	Page	4	Entrance	
	{REGFLAG_UDELAY, 1, {}},	
	{0xFB,1,{0x01}},						
	{0x9D,1,{0xB0}},						
	{0x72,1,{0x00}},						
	{0x93,1,{0x04}},						
	{0x94,1,{0x04}},						
	{0x9B,1,{0x0F}},						
	{0x8A,1,{0x33}},						
	{0x86,1,{0x1B}},						
	{0x87,1,{0x39}},						
	{0x88,1,{0x1B}},						
	{0x89,1,{0x39}},						
	{0x8B,1,{0xF4}},						
	{0x8C,1,{0x01}},						
	{0x90,1,{0x79}},						
	{0x91,1,{0x4C}},	
	//xuecheng, modify to 0x77 to see whether fps is higher
	//{0x92,1,{0x79}},
	{0x92,1,{0x77}},
	{0x95,1,{0xE4}},						

	{0xDE,1,{0xFF}},						
	{0xDF,1,{0x82}},						

	{0x00,1,{0x0F}},						
	{0x01,1,{0x00}},						
	{0x02,1,{0x00}},						
	{0x03,1,{0x00}},						
	{0x04,1,{0x0B}},						
	{0x05,1,{0x0C}},						
	{0x06,1,{0x00}},						
	{0x07,1,{0x00}},						
	{0x08,1,{0x00}},						
	{0x09,1,{0x00}},						
	{0x0A,1,{0X03}},						
	{0x0B,1,{0X04}},						
	{0x0C,1,{0x01}},						
	{0x0D,1,{0x13}},						
	{0x0E,1,{0x15}},						
	{0x0F,1,{0x17}},						
	{0x10,1,{0x0F}},						
	{0x11,1,{0x00}},						
	{0x12,1,{0x00}},						
	{0x13,1,{0x00}},						
	{0x14,1,{0x0B}},						
	{0x15,1,{0x0C}},						
	{0x16,1,{0x00}},						
	{0x17,1,{0x00}},						
	{0x18,1,{0x00}},						
	{0x19,1,{0x00}},						
	{0x1A,1,{0x03}},						
	{0x1B,1,{0X04}},						
	{0x1C,1,{0x01}},						
	{0x1D,1,{0x13}},						
	{0x1E,1,{0x15}},						
	{0x1F,1,{0x17}},						

	{0x20,1,{0x09}},						
	{0x21,1,{0x01}},						
	{0x22,1,{0x00}},						
	{0x23,1,{0x00}},						
	{0x24,1,{0x00}},						
	{0x25,1,{0x6D}},						
	{0x26,1,{0x00}},						
	{0x27,1,{0x00}},						

	{0x2F,1,{0x02}},						
	{0x30,1,{0x04}},						
	{0x31,1,{0x49}},						
	{0x32,1,{0x23}},						
	{0x33,1,{0x01}},						
	{0x34,1,{0x00}},						
	{0x35,1,{0x69}},						
	{0x36,1,{0x00}},						
	{0x37,1,{0x2D}},						
	{0x38,1,{0x08}},						
	{0x39,1,{0x00}},						
	{0x3A,1,{0x69}},						

	{0x29,1,{0x58}},						
	{0x2A,1,{0x16}},						

	{0x5B,1,{0x00}},						
	{0x5F,1,{0x75}},						
	{0x63,1,{0x00}},						
	{0x67,1,{0x04}},						

	{0x7B,1,{0x80}},						
	{0x7C,1,{0xD8}},						
	{0x7D,1,{0x60}},						
	{0x7E,1,{0x10}},						
	{0x7F,1,{0x19}},						
	{0x80,1,{0x00}},						
	{0x81,1,{0x06}},						
	{0x82,1,{0x03}},						
	{0x83,1,{0x00}},						
	{0x84,1,{0x03}},						
	{0x85,1,{0x07}},						
	{0x74,1,{0x10}},						
	{0x75,1,{0x19}},						
	{0x76,1,{0x06}},						
	{0x77,1,{0x03}},						

	{0x78,1,{0x00}},						
	{0x79,1,{0x00}},						
	{0x99,1,{0x33}},						
	{0x98,1,{0x00}},						
	{0xB3,1,{0x28}},						
	{0xB4,1,{0x05}},						
	{0xB5,1,{0x10}},						

	{0xFF,1,{0x20}},	//	Page	0,1,{	power-related	setting	
	{REGFLAG_UDELAY, 1, {}},	
	{0x00,1,{0x01}},						
	{0x01,1,{0x55}},						
	{0x02,1,{0x45}},						
	{0x03,1,{0x55}},						
	{0x05,1,{0x50}},						
	{0x06,1,{0x9E}},						
	{0x07,1,{0xA8}},						
	{0x08,1,{0x0C}},						
	{0x0B,1,{0x96}},						
	{0x0C,1,{0x96}},						
	{0x0E,1,{0x00}},						
	{0x0F,1,{0x00}},						
	{0x11,1,{0x29}},						
	{0x12,1,{0x29}},						
	{0x13,1,{0x03}},						
	{0x14,1,{0x0A}},						
	{0x15,1,{0x99}},						
	{0x16,1,{0x99}},						
	{0x6D,1,{0x44}},						
	{0x58,1,{0x05}},						
	{0x59,1,{0x05}},						
	{0x5A,1,{0x05}},						
	{0x5B,1,{0x05}},						
	{0x5C,1,{0x00}},						
	{0x5D,1,{0x00}},						
	{0x5E,1,{0x00}},						
	{0x5F,1,{0x00}},						

	{0x1B,1,{0x39}},						
	{0x1C,1,{0x39}},						
	{0x1D,1,{0x47}},						

	{0xFF,1,{0x20}},	//	Page	0,1,{	power-related	setting	
	{REGFLAG_UDELAY, 1, {}},	
	//	R+	,1,{}},						
	{0x75,1,{0x00}},						
	{0x76,1,{0x00}},						
	{0x77,1,{0x00}},						
	{0x78,1,{0x22}},						
	{0x79,1,{0x00}},						
	{0x7A,1,{0x46}},						
	{0x7B,1,{0x00}},						
	{0x7C,1,{0x5C}},						
	{0x7D,1,{0x00}},						
	{0x7E,1,{0x76}},						
	{0x7F,1,{0x00}},						
	{0x80,1,{0x8D}},						
	{0x81,1,{0x00}},						
	{0x82,1,{0xA6}},						
	{0x83,1,{0x00}},						
	{0x84,1,{0xB8}},						
	{0x85,1,{0x00}},						
	{0x86,1,{0xC7}},						
	{0x87,1,{0x00}},						
	{0x88,1,{0xF6}},						
	{0x89,1,{0x01}},						
	{0x8A,1,{0x1D}},						
	{0x8B,1,{0x01}},						
	{0x8C,1,{0x54}},						
	{0x8D,1,{0x01}},						
	{0x8E,1,{0x81}},						
	{0x8F,1,{0x01}},						
	{0x90,1,{0xCB}},						
	{0x91,1,{0x02}},						
	{0x92,1,{0x05}},						
	{0x93,1,{0x02}},						
	{0x94,1,{0x07}},						
	{0x95,1,{0x02}},						
	{0x96,1,{0x47}},						
	{0x97,1,{0x02}},						
	{0x98,1,{0x82}},						
	{0x99,1,{0x02}},						
	{0x9A,1,{0xAB}},						
	{0x9B,1,{0x02}},						
	{0x9C,1,{0xDC}},						
	{0x9D,1,{0x03}},						
	{0x9E,1,{0x01}},						
	{0x9F,1,{0x03}},						
	{0xA0,1,{0x3A}},						
	{0xA2,1,{0x03}},						
	{0xA3,1,{0x56}},						
	{0xA4,1,{0x03}},						
	{0xA5,1,{0x6D}},						
	{0xA6,1,{0x03}},						
	{0xA7,1,{0x89}},						
	{0xA9,1,{0x03}},						
	{0xAA,1,{0xA3}},						
	{0xAB,1,{0x03}},						
	{0xAC,1,{0xC9}},						
	{0xAD,1,{0x03}},						
	{0xAE,1,{0xDD}},						
	{0xAF,1,{0x03}},						
	{0xB0,1,{0xF5}},						
	{0xB1,1,{0x03}},						
	{0xB2,1,{0xFF}},						
	//	R-	,1,{}},						
	{0xB3,1,{0x00}},						
	{0xB4,1,{0x00}},						
	{0xB5,1,{0x00}},						
	{0xB6,1,{0x22}},						
	{0xB7,1,{0x00}},						
	{0xB8,1,{0x46}},						
	{0xB9,1,{0x00}},						
	{0xBA,1,{0x5C}},						
	{0xBB,1,{0x00}},						
	{0xBC,1,{0x76}},						
	{0xBD,1,{0x00}},						
	{0xBE,1,{0x8D}},						
	{0xBF,1,{0x00}},						
	{0xC0,1,{0xA6}},						
	{0xC1,1,{0x00}},						
	{0xC2,1,{0xB8}},						
	{0xC3,1,{0x00}},						
	{0xC4,1,{0xC7}},						
	{0xC5,1,{0x00}},						
	{0xC6,1,{0xF6}},						
	{0xC7,1,{0x01}},						
	{0xC8,1,{0x1D}},						
	{0xC9,1,{0x01}},						
	{0xCA,1,{0x54}},						
	{0xCB,1,{0x01}},						
	{0xCC,1,{0x81}},						
	{0xCD,1,{0x01}},						
	{0xCE,1,{0xCB}},						
	{0xCF,1,{0x02}},						
	{0xD0,1,{0x05}},						
	{0xD1,1,{0x02}},						
	{0xD2,1,{0x07}},						
	{0xD3,1,{0x02}},						
	{0xD4,1,{0x47}},						
	{0xD5,1,{0x02}},						
	{0xD6,1,{0x82}},						
	{0xD7,1,{0x02}},						
	{0xD8,1,{0xAB}},						
	{0xD9,1,{0x02}},						
	{0xDA,1,{0xDC}},						
	{0xDB,1,{0x03}},						
	{0xDC,1,{0x01}},						
	{0xDD,1,{0x03}},						
	{0xDE,1,{0x3A}},						
	{0xDF,1,{0x03}},						
	{0xE0,1,{0x56}},						
	{0xE1,1,{0x03}},						
	{0xE2,1,{0x6D}},						
	{0xE3,1,{0x03}},						
	{0xE4,1,{0x89}},						
	{0xE5,1,{0x03}},						
	{0xE6,1,{0xA3}},						
	{0xE7,1,{0x03}},						
	{0xE8,1,{0xC9}},						
	{0xE9,1,{0x03}},						
	{0xEA,1,{0xDD}},						
	{0xEB,1,{0x03}},						
	{0xEC,1,{0xF5}},						
	{0xED,1,{0x03}},						
	{0xEE,1,{0xFF}},						
	//	G+	,1,{}},						
	{0xEF,1,{0x00}},						
	{0xF0,1,{0x00}},						
	{0xF1,1,{0x00}},						
	{0xF2,1,{0x22}},						
	{0xF3,1,{0x00}},						
	{0xF4,1,{0x46}},						
	{0xF5,1,{0x00}},						
	{0xF6,1,{0x5C}},						
	{0xF7,1,{0x00}},						
	{0xF8,1,{0x76}},						
	{0xF9,1,{0x00}},						
	{0xFA,1,{0x8D}},						

	{0xFF,1,{0x21}},	//	Page	0,1,{	power-related	setting	
	{REGFLAG_UDELAY, 1, {}},	
	{0x00,1,{0x00}},						
	{0x01,1,{0xA6}},						
	{0x02,1,{0x00}},						
	{0x03,1,{0xB8}},						
	{0x04,1,{0x00}},						
	{0x05,1,{0xC7}},						
	{0x06,1,{0x00}},						
	{0x07,1,{0xF6}},						
	{0x08,1,{0x01}},						
	{0x09,1,{0x1D}},						
	{0x0A,1,{0x01}},						
	{0x0B,1,{0x54}},						
	{0x0C,1,{0x01}},						
	{0x0D,1,{0x81}},						
	{0x0E,1,{0x01}},						
	{0x0F,1,{0xCB}},						
	{0x10,1,{0x02}},						
	{0x11,1,{0x05}},						
	{0x12,1,{0x02}},						
	{0x13,1,{0x07}},						
	{0x14,1,{0x02}},						
	{0x15,1,{0x47}},						
	{0x16,1,{0x02}},						
	{0x17,1,{0x82}},						
	{0x18,1,{0x02}},						
	{0x19,1,{0xAB}},						
	{0x1A,1,{0x02}},						
	{0x1B,1,{0xDC}},						
	{0x1C,1,{0x03}},						
	{0x1D,1,{0x01}},						
	{0x1E,1,{0x03}},						
	{0x1F,1,{0x3A}},						
	{0x20,1,{0x03}},						
	{0x21,1,{0x56}},						
	{0x22,1,{0x03}},						
	{0x23,1,{0x6D}},						
	{0x24,1,{0x03}},						
	{0x25,1,{0x89}},						
	{0x26,1,{0x03}},						
	{0x27,1,{0xA3}},						
	{0x28,1,{0x03}},						
	{0x29,1,{0xC9}},						
	{0x2A,1,{0x03}},						
	{0x2B,1,{0xDD}},						
	{0x2D,1,{0x03}},						
	{0x2F,1,{0xF5}},						
	{0x30,1,{0x03}},						
	{0x31,1,{0xFF}},						
	//	G-	,1,{}},						
	{0x32,1,{0x00}},						
	{0x33,1,{0x00}},						
	{0x34,1,{0x00}},						
	{0x35,1,{0x22}},						
	{0x36,1,{0x00}},						
	{0x37,1,{0x46}},						
	{0x38,1,{0x00}},						
	{0x39,1,{0x5C}},						
	{0x3A,1,{0x00}},						
	{0x3B,1,{0x76}},						
	{0x3D,1,{0x00}},						
	{0x3F,1,{0x8D}},						
	{0x40,1,{0x00}},						
	{0x41,1,{0xA6}},						
	{0x42,1,{0x00}},						
	{0x43,1,{0xB8}},						
	{0x44,1,{0x00}},						
	{0x45,1,{0xC7}},						
	{0x46,1,{0x00}},						
	{0x47,1,{0xF6}},						
	{0x48,1,{0x01}},						
	{0x49,1,{0x1D}},						
	{0x4A,1,{0x01}},						
	{0x4B,1,{0x54}},						
	{0x4C,1,{0x01}},						
	{0x4D,1,{0x81}},						
	{0x4E,1,{0x01}},						
	{0x4F,1,{0xCB}},						
	{0x50,1,{0x02}},						
	{0x51,1,{0x05}},						
	{0x52,1,{0x02}},						
	{0x53,1,{0x07}},						
	{0x54,1,{0x02}},						
	{0x55,1,{0x47}},						
	{0x56,1,{0x02}},						
	{0x58,1,{0x82}},						
	{0x59,1,{0x02}},						
	{0x5A,1,{0xAB}},						
	{0x5B,1,{0x02}},						
	{0x5C,1,{0xDC}},						
	{0x5D,1,{0x03}},						
	{0x5E,1,{0x01}},						
	{0x5F,1,{0x03}},						
	{0x60,1,{0x3A}},						
	{0x61,1,{0x03}},						
	{0x62,1,{0x56}},						
	{0x63,1,{0x03}},						
	{0x64,1,{0x6D}},						
	{0x65,1,{0x03}},						
	{0x66,1,{0x89}},						
	{0x67,1,{0x03}},						
	{0x68,1,{0xA3}},						
	{0x69,1,{0x03}},						
	{0x6A,1,{0xC9}},						
	{0x6B,1,{0x03}},						
	{0x6C,1,{0xDD}},						
	{0x6D,1,{0x03}},						
	{0x6E,1,{0xF5}},						
	{0x6F,1,{0x03}},						
	{0x70,1,{0xFF}},						
	//	B+	,1,{}},						
	{0x71,1,{0x00}},						
	{0x72,1,{0x00}},						
	{0x73,1,{0x00}},						
	{0x74,1,{0x22}},						
	{0x75,1,{0x00}},						
	{0x76,1,{0x46}},						
	{0x77,1,{0x00}},						
	{0x78,1,{0x5C}},						
	{0x79,1,{0x00}},						
	{0x7A,1,{0x76}},						
	{0x7B,1,{0x00}},						
	{0x7C,1,{0x8D}},						
	{0x7D,1,{0x00}},						
	{0x7E,1,{0xA6}},						
	{0x7F,1,{0x00}},						
	{0x80,1,{0xB8}},						
	{0x81,1,{0x00}},						
	{0x82,1,{0xC7}},						
	{0x83,1,{0x00}},						
	{0x84,1,{0xF6}},						
	{0x85,1,{0x01}},						
	{0x86,1,{0x1D}},						
	{0x87,1,{0x01}},						
	{0x88,1,{0x54}},						
	{0x89,1,{0x01}},						
	{0x8A,1,{0x81}},						
	{0x8B,1,{0x01}},						
	{0x8C,1,{0xCB}},						
	{0x8D,1,{0x02}},						
	{0x8E,1,{0x05}},						
	{0x8F,1,{0x02}},						
	{0x90,1,{0x07}},						
	{0x91,1,{0x02}},						
	{0x92,1,{0x47}},						
	{0x93,1,{0x02}},						
	{0x94,1,{0x82}},						
	{0x95,1,{0x02}},						
	{0x96,1,{0xAB}},						
	{0x97,1,{0x02}},						
	{0x98,1,{0xDC}},						
	{0x99,1,{0x03}},						
	{0x9A,1,{0x01}},						
	{0x9B,1,{0x03}},						
	{0x9C,1,{0x3A}},						
	{0x9D,1,{0x03}},						
	{0x9E,1,{0x56}},						
	{0x9F,1,{0x03}},						
	{0xA0,1,{0x6D}},						
	{0xA2,1,{0x03}},						
	{0xA3,1,{0x89}},						
	{0xA4,1,{0x03}},						
	{0xA5,1,{0xA3}},						
	{0xA6,1,{0x03}},						
	{0xA7,1,{0xC9}},						
	{0xA9,1,{0x03}},						
	{0xAA,1,{0xDD}},						
	{0xAB,1,{0x03}},						
	{0xAC,1,{0xF5}},						
	{0xAD,1,{0x03}},						
	{0xAE,1,{0xFF}},						
	//	B-	,1,{}},						
	{0xAF,1,{0x00}},						
	{0xB0,1,{0x00}},						
	{0xB1,1,{0x00}},						
	{0xB2,1,{0x22}},						
	{0xB3,1,{0x00}},						
	{0xB4,1,{0x46}},						
	{0xB5,1,{0x00}},						
	{0xB6,1,{0x5C}},						
	{0xB7,1,{0x00}},						
	{0xB8,1,{0x76}},						
	{0xB9,1,{0x00}},						
	{0xBA,1,{0x8D}},						
	{0xBB,1,{0x00}},						
	{0xBC,1,{0xA6}},						
	{0xBD,1,{0x00}},						
	{0xBE,1,{0xB8}},						
	{0xBF,1,{0x00}},						
	{0xC0,1,{0xC7}},						
	{0xC1,1,{0x00}},						
	{0xC2,1,{0xF6}},						
	{0xC3,1,{0x01}},						
	{0xC4,1,{0x1D}},						
	{0xC5,1,{0x01}},						
	{0xC6,1,{0x54}},						
	{0xC7,1,{0x01}},						
	{0xC8,1,{0x81}},						
	{0xC9,1,{0x01}},						
	{0xCA,1,{0xCB}},						
	{0xCB,1,{0x02}},						
	{0xCC,1,{0x05}},						
	{0xCD,1,{0x02}},						
	{0xCE,1,{0x07}},						
	{0xCF,1,{0x02}},						
	{0xD0,1,{0x47}},						
	{0xD1,1,{0x02}},						
	{0xD2,1,{0x82}},						
	{0xD3,1,{0x02}},						
	{0xD4,1,{0xAB}},						
	{0xD5,1,{0x02}},						
	{0xD6,1,{0xDC}},						
	{0xD7,1,{0x03}},						
	{0xD8,1,{0x01}},						
	{0xD9,1,{0x03}},						
	{0xDA,1,{0x3A}},						
	{0xDB,1,{0x03}},						
	{0xDC,1,{0x56}},						
	{0xDD,1,{0x03}},						
	{0xDE,1,{0x6D}},						
	{0xDF,1,{0x03}},						
	{0xE0,1,{0x89}},						
	{0xE1,1,{0x03}},						
	{0xE2,1,{0xA3}},						
	{0xE3,1,{0x03}},						
	{0xE4,1,{0xC9}},						
	{0xE5,1,{0x03}},						
	{0xE6,1,{0xDD}},						
	{0xE7,1,{0x03}},						
	{0xE8,1,{0xF5}},						
	{0xE9,1,{0x03}},						
	{0xEA,1,{0xFF}},						

	{0xFF,1,{0x21}},	//	Page	,1,{	Gamma	Default	Update
	{REGFLAG_UDELAY, 1, {}},	
	{0xEB,1,{0x30}},						
	{0xEC,1,{0x17}},						
	{0xED,1,{0x20}},						
	{0xEE,1,{0x0F}},						
	{0xEF,1,{0x1F}},						
	{0xF0,1,{0x0F}},						
	{0xF1,1,{0x0F}},						
	{0xF2,1,{0x07}},						

	{0xFF,1,{0x23}},	//	CMD2	Page	3	Entrance	
	{REGFLAG_UDELAY, 1, {}},	
	{0x08,1,{0x04}},						
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//image.first
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	{0xFF,1,{0x10}},	//	Return	To	CMD1		
	{REGFLAG_UDELAY, 1, {}},	
	{0x35,1,{0x00}},						
	{0x29,0,{}},
	//{0x51,1,{0xFF}},	//	write	display	brightness		
};
							
#if 0
static struct LCM_setting_table lcm_set_window[] = {
	{0x2A,	4,	{0x00, 0x00, (FRAME_WIDTH>>8), (FRAME_WIDTH&0xFF)}},
	{0x2B,	4,	{0x00, 0x00, (FRAME_HEIGHT>>8), (FRAME_HEIGHT&0xFF)}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif
#if 0
static struct LCM_setting_table lcm_sleep_out_setting[] = {
    //Sleep Out
    {0x11, 1, {0x00}},
    {REGFLAG_DELAY, 120, {}},

    // Display ON
    {0x29, 1, {0x00}},
    {REGFLAG_DELAY, 20, {}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
    // Display off sequence
    {0x28, 1, {0x00}},
    {REGFLAG_DELAY, 20, {}},

    // Sleep Mode On
    {0x10, 1, {0x00}},
    {REGFLAG_DELAY, 120, {}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif
static struct LCM_setting_table lcm_backlight_level_setting[] = {
{0x51, 1, {0xFF}},
{REGFLAG_END_OF_TABLE, 0x00, {}}
};
static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
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
                dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
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
	params->dsi.LANE_NUM				= LCM_FOUR_LANE;
	//The following defined the fomat for data coming from LCD engine.
	params->dsi.data_format.color_order 	= LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   	= LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     	= LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format      		= LCM_DSI_FORMAT_RGB888;

	// Highly depends on LCD driver capability.
	params->dsi.packet_size=256;
	//video mode timing

	params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active				= 2;
	params->dsi.vertical_backporch					= 8;
	params->dsi.vertical_frontporch					= 10;
	params->dsi.vertical_active_line					= FRAME_HEIGHT;

	params->dsi.horizontal_sync_active				= 10;
	params->dsi.horizontal_backporch				= 20;
	params->dsi.horizontal_frontporch				= 40;
	params->dsi.horizontal_active_pixel				= FRAME_WIDTH;
    	//params->dsi.ssc_disable							= 1;
#ifndef CONFIG_FPGA_EARLY_PORTING
#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 120; //this value must be in MTK suggested table
#else
	params->dsi.PLL_CLOCK = 80; //this value must be in MTK suggested table
#endif
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif

	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd          = 0x53;
	params->dsi.lcm_esd_check_table[0].count        = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x24;

}

#ifdef BUILD_LK
#ifndef CONFIG_FPGA_EARLY_PORTING
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

#else
  
//	extern int mt8193_i2c_write(u16 addr, u32 data);
//	extern int mt8193_i2c_read(u16 addr, u32 *data);
	
//	#define TPS65132_write_byte(add, data)  mt8193_i2c_write(add, data)
	//#define TPS65132_read_byte(add)  mt8193_i2c_read(add)
  
#endif
#endif


static void lcm_init_power(void)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef BUILD_LK
	mt6325_upmu_set_rg_vgp1_en(1);
#else
	printk("%s, begin\n", __func__);
	hwPowerOn(MT6325_POWER_LDO_VGP1, VOL_DEFAULT, "LCM_DRV");	
	printk("%s, end\n", __func__);
#endif
#endif
}

static void lcm_suspend_power(void)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef BUILD_LK
	mt6325_upmu_set_rg_vgp1_en(0);
#else
	printk("%s, begin\n", __func__);
	hwPowerDown(MT6325_POWER_LDO_VGP1, "LCM_DRV");	
	printk("%s, end\n", __func__);
#endif
#endif
}

static void lcm_resume_power(void)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef BUILD_LK
	mt6325_upmu_set_rg_vgp1_en(1);
#else
	printk("%s, begin\n", __func__);
	hwPowerOn(MT6325_POWER_LDO_VGP1, VOL_DEFAULT, "LCM_DRV");	
	printk("%s, end\n", __func__);
#endif
#endif
}


static void lcm_init(void)
{
	unsigned char cmd = 0x0;
	unsigned char data = 0xFF;
	int ret=0;
	cmd=0x00;
	data=0x0E;
#ifndef CONFIG_FPGA_EARLY_PORTING
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
#endif
	SET_RESET_PIN(1);
	MDELAY(1);
	SET_RESET_PIN(0);
	MDELAY(10);

	SET_RESET_PIN(1);
	MDELAY(10);

	// when phone initial , config output high, enable backlight drv chip  
	 push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);  
}

static void lcm_suspend(void)
{
	mt_set_gpio_mode(GPIO_65132_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_EN, GPIO_OUT_ZERO);
	push_table(lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);  
	//SET_RESET_PIN(0);
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
	dsi_set_cmdq(data_array, 3, 1);
	
	data_array[0]= 0x00053902;
	data_array[1]= (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
	data_array[2]= (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	
	data_array[0]= 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);	
}

#define LCM_ID_NT35595 (0x95)

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
	
	read_reg_v2(0xF4, buffer, 2);
	id = buffer[0]; //we only need ID
#ifdef BUILD_LK
		dprintf(0, "%s, LK nt35595 debug: nt35595 id = 0x%08x\n", __func__, id);
#else
		printk("%s, kernel nt35595 horse debug: nt35595 id = 0x%08x\n", __func__, id);
#endif

	if(id == LCM_ID_NT35595)
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
	
	push_table(lcm_backlight_level_setting, sizeof(lcm_backlight_level_setting) / sizeof(struct LCM_setting_table), 1);

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


LCM_DRIVER nt35595_fhd_dsi_cmd_truly_nt50358_fwvga_lcm_drv=
{
    .name           	= "nt35595_fhd_dsi_cmd_truly_nt50358_fwvga_drv",
    .set_util_funcs 	= lcm_set_util_funcs,
    .get_params     	= lcm_get_params,
    .init           		= lcm_init,/*tianma init fun.*/
    .suspend        	= lcm_suspend,
    .resume         	= lcm_resume,
     .compare_id     	= lcm_compare_id,
     .init_power		= lcm_init_power,
     .resume_power = lcm_resume_power,
     .suspend_power = lcm_suspend_power,
     .esd_check = lcm_esd_check,
     .set_backlight = lcm_setbacklight,
	 .ata_check		= lcm_ata_check,
	 .update         = lcm_update,
     .switch_mode		= lcm_switch_mode,
};
/* END PN:DTS2013053103858 , Added by d00238048, 2013.05.31*/
