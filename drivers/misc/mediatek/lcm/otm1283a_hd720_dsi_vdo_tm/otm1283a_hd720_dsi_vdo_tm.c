#ifndef BUILD_LK
#include <linux/string.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
	#include <platform/mt_gpio.h>
	#include <platform/mt_i2c.h> 
	#include <platform/mt_pmic.h>
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

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------
#define dsi_set_cmd_by_cmdq(handle,cmd,count,ppara,force_update)    lcm_util.dsi_set_cmdq_V22(handle,cmd,count,ppara,force_update);
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


 int tps65132_write_bytes(unsigned char addr, unsigned char value)
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
EXPORT_SYMBOL_GPL(tps65132_write_bytes);



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
// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  										(720)
#define FRAME_HEIGHT 										(1280)

#define REGFLAG_DELAY             							0XFE
#define REGFLAG_END_OF_TABLE      							0xFF   // END OF REGISTERS MARKER

#define LCM_DSI_CMD_MODE									0
#ifndef CONFIG_FPGA_EARLY_PORTING
#define GPIO_65132_ENP GPIO_LCD_BIAS_ENP_PIN
#define GPIO_65132_ENN GPIO_LCD_BIAS_ENN_PIN
#endif
#define LCM_ID_OTM1283 0x40

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))

#define UDELAY(n) 											(lcm_util.udelay(n))
#define MDELAY(n) 											(lcm_util.mdelay(n))


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg											lcm_util.dsi_read_reg()
#define read_reg_v2(cmd, buffer, buffer_size)   			lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)    
       

static struct LCM_setting_table {
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[64];
};

#if 0
static struct LCM_setting_table lcm_initialization_setting[] = {
	
	/*
	Note :

	Data ID will depends on the following rule.
	
		count of parameters > 1	=> Data ID = 0x39
		count of parameters = 1	=> Data ID = 0x15
		count of parameters = 0	=> Data ID = 0x05

	Structure Format :

	{DCS command, count of parameters, {parameter list}}
	{REGFLAG_DELAY, milliseconds of time, {}},

	...

	Setting ending by predefined flag
	
	{REGFLAG_END_OF_TABLE, 0x00, {}}
	*/


	//must use 0x39 for init setting for all register.

	{0XB9, 3, {0XFF,0X83,0X89}},
	{REGFLAG_DELAY, 10, {}},

#if 0//CLOSE IC ESD Protect enhance
	{0XBA, 17, {0X41,0X83,0X00,0X16,
				0XA4,0X00,0X18,0XFF,
				0X0F,0X21,0X03,0X21,
				0X23,0X25,0X20,0X02, 
		  		0X31}}, 
	{REGFLAG_DELAY, 10, {}},
#else //open IC ESD  Protect enhance 
{0XBA, 17, {0X41,0X83,0X00,0X16,
			0XA4,0X00,0X18,0XFF,
			0X0F,0X21,0X03,0X21,
			0X23,0X25,0X20,0X02, 
			0X35}}, 
{REGFLAG_DELAY, 10, {}},

#endif

	{0XDE, 2, {0X05,0X58}},
	{REGFLAG_DELAY, 10, {}},

	{0XB1, 19, {0X00,0X00,0X04,0XD9,
				0XCF,0X10,0X11,0XAC,
				0X0C,0X1D,0X25,0X1D,
				0X1D,0X42,0X01,0X58,
		  		0XF7,0X20,0X80}},
	{REGFLAG_DELAY, 10, {}},


	{0XB2, 5,   {0X00,0X00,0X78,0X03,
				 0X02}},
	{REGFLAG_DELAY, 10, {}},
	
	{0XB4, 31, {0X82,0X04,0X00,0X32,
				0X10,0X00,0X32,0X10,
				0X00,0X00,0X00,0X00,
				0X17,0X0A,0X40,0X01,
				0X13,0X0A,0X40,0X14,
				0X46,0X50,0X0A,0X0A,
				0X3C,0X0A,0X3C,0X14,
		  		0X46,0X50,0X0A}}, 
	{REGFLAG_DELAY, 10, {}},
	
	{0XD5, 48, {0X00,0X00,0X00,0X00,
				0X01,0X00,0X00,0X00,
				0X20,0X00,0X99,0X88,
				0X88,0X88,0X88,0X88,
				0X88,0X88,0X88,0X01,
				0X88,0X23,0X01,0X88,
				0X88,0X88,0X88,0X88,
				0X88,0X88,0X99,0X88,
				0X88,0X88,0X88,0X88,
				0X88,0X88,0X32,0X88,
				0X10,0X10,0X88,0X88,
				0X88,0X88,0X88,0X88}}, 
	{REGFLAG_DELAY, 10, {}},
	

	{0XB6, 4,   {0X00,0X8A,0X00,0X8A}},
	{REGFLAG_DELAY, 10, {}},

	{0XCC, 1,   {0X02}},
	{REGFLAG_DELAY, 10, {}},


	{0X35, 1,   {0X00}},//TE on
	{REGFLAG_DELAY, 10, {}},

	// Note
	// Strongly recommend not to set Sleep out / Display On here. That will cause messed frame to be shown as later the backlight is on.

	// Setting ending by predefined flag
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif

static struct LCM_setting_table lcm_set_window[] = {
	{0x2A,	4,	{0x00, 0x00, (FRAME_WIDTH>>8), (FRAME_WIDTH&0xFF)}},
	{0x2B,	4,	{0x00, 0x00, (FRAME_HEIGHT>>8), (FRAME_HEIGHT&0xFF)}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
	{0x11, 0, {0x00}},
    {REGFLAG_DELAY, 100, {}},

    // Display ON
	{0x29, 0, {0x00}},
	{REGFLAG_DELAY, 10, {}},
	
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_sleep_in_setting[] = {
	// Display off sequence
	{0x28, 0, {0x00}},

    // Sleep Mode On
	{0x10, 0, {0x00}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
static struct LCM_setting_table lcm_backlight_level_setting[] = {
{0x51, 1, {0xFF}},
{REGFLAG_END_OF_TABLE, 0x00, {}}
};

#ifdef BUILD_LK
#ifndef CONFIG_FPGA_EARLY_PORTING
#define TPS65132_SLAVE_ADDR_WRITE  0x7C  
static struct mt_i2c_t TPS65132_i2c;

int TPS65132_write_byte(kal_uint8 addr, kal_uint8 value)
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

static void lcm_init_registers()
{
#if 1
	unsigned int data_array[16];
	data_array[0] = 0x00002300;
	dsi_set_cmdq(&data_array, 1, 1);//EXTC = 1
	
	data_array[0] = 0x00042902;
	data_array[1] = 0x018312FF;
	dsi_set_cmdq(&data_array, 2, 1);//EXTC = 1
	
	data_array[0] = 0x80002300;
	dsi_set_cmdq(&data_array, 1, 1);	//Orise mode enable
	
	data_array[0] = 0x00032902;
	data_array[1] = 0x008312FF;
	dsi_set_cmdq(&data_array, 2, 1);
	
	/*===================panel setting====================*/
	data_array[0] = 0x80002300;
	dsi_set_cmdq(&data_array, 1, 1);	//TCON Setting
	
	data_array[0] = 0x000A2902;
	data_array[1] = 0x006400C0;
	data_array[2] = 0x6400120e;
	data_array[3] = 0x0000120e;
	dsi_set_cmdq(&data_array, 4, 1);

	data_array[0] = 0xb4002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x55C02300;
	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0] = 0x81002300;
	dsi_set_cmdq(&data_array, 1, 1);	//frame rate: 60Hz
	
	data_array[0] = 0x55C12300;
	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0] = 0x81002300;				//Source bias 0.75uA
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x82C42300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x90002300;				//clock delay for data latch
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x49C42300;
	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0] = 0x82002300;				//clock delay for data latch
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x02C42300;
	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0] = 0xc6002300;				//clock delay for data latch
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x03b02300;
	dsi_set_cmdq(&data_array, 1, 1);

	//////////////////////////////////////////
#if 0
	data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);	//Panel Timing Setting
	
	data_array[0] = 0x00072902;
	data_array[1] = 0x005C00C0;
	data_array[2] = 0x00040001;
	dsi_set_cmdq(&data_array, 3, 1);
	
	data_array[0] = 0xA4002300;
	dsi_set_cmdq(&data_array, 1, 1);	//Source pre.
	
	data_array[0] = 0x1CC02300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0xB3002300;
	dsi_set_cmdq(&data_array, 1, 1);	//Interval Scan Frame: 0 frame, column inversion
	
	data_array[0] = 0x00032902;
	data_array[1] = 0x005000C0;
	dsi_set_cmdq(&data_array, 2, 1);

	#endif
	/*================Power setting===============*/
	data_array[0] = 0xA0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000F2902;	//DCDC setting
	data_array[1] = 0x061005C4;
	data_array[2] = 0x10150502;
	data_array[3] = 0x02071005;
	data_array[4] = 0x00101505;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xB0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//Clamp coltage setting
	data_array[1] = 0x000000C4;
	dsi_set_cmdq(&data_array, 2, 1);

	data_array[0] = 0xBB002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x80c52300;	//LVD voltage level setting
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x91002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VGH=12v, VGL=-12v, pump ratio: VGH=6x, VGL=-5x
	data_array[1] = 0x005016c5;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0x00002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//GVDD=4.87v, NGVDD = -4.87V
	data_array[1] = 0x00aeaed8;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0xB0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VDD_18v=1.6v, LVDSVDD=1.55v
	data_array[1] = 0x00b804c5;
	dsi_set_cmdq(&data_array, 2, 1);
	
	/*=========================Panel Timming State Control================*/
	data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00052902;	//mode-3
	data_array[1] = 0x021102f5;
	data_array[2] = 0x00000011;
	dsi_set_cmdq(&data_array, 3, 1);
	
		data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x50c52300;	//2xVPNL, 1.5*=00, 2*=50, 3*=A0
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x94002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x66c52300;	//Frequency
	dsi_set_cmdq(&data_array, 1, 1);
	
	/*===============VGL01/02 disable================*/
	data_array[0] = 0xb2002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VGL01
	data_array[1] = 0x000000f5;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0xb4002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VGL01_S
	data_array[1] = 0x000000f5;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0xb6002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VGL02
	data_array[1] = 0x000000f5;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0xb8002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VGL02_S
	data_array[1] = 0x000000f5;
	dsi_set_cmdq(&data_array, 2, 1);


	data_array[0] = 0x94002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VCL ON
	data_array[1] = 0x000002f5;
	dsi_set_cmdq(&data_array, 2, 1);

	data_array[0] = 0xBA002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VSP ON
	data_array[1] = 0x000003f5;
	dsi_set_cmdq(&data_array, 2, 1);

	data_array[0] = 0xB2002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VCL ON
	data_array[1] = 0x000040C5;
	dsi_set_cmdq(&data_array, 2, 1);

	data_array[0] = 0xB4002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VCL ON
	data_array[1] = 0x0000C0C5;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0x80002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000C2902;	//Panel timing state control
	data_array[1] = 0x000000cb;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	dsi_set_cmdq(&data_array, 4, 1);
	
	data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel timing state control
	data_array[1] = 0x000000cb;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xa0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel timing state control
	data_array[1] = 0x000000cb;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xb0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel timing state control
	data_array[1] = 0x000000cb;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xc0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel timing state control
	data_array[1] = 0x050505cb;
	data_array[2] = 0x00050505;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xd0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel timing state control
	data_array[1] = 0x000000cb;
	data_array[2] = 0x05050500;
	data_array[3] = 0x05050505;
	data_array[4] = 0x00000505;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xe0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000F2902;	//Panel timing state control
	data_array[1] = 0x000000cb;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00050505;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xf0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000c2902;	//Panel timing state control
	data_array[1] = 0xffffffcb;
	data_array[2] = 0xffffffff;
	data_array[3] = 0xfffffffC;
	dsi_set_cmdq(&data_array, 4, 1);
	
	/*===============Panel pad mapping control===============*/
	data_array[0] = 0x80002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel pad mapping control
	data_array[1] = 0x100A0Ccc;
	data_array[2] = 0x0004020e;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel pad mapping control
	data_array[1] = 0x000000cc;
	data_array[2] = 0x2d2e0600;
	data_array[3] = 0x0d0f090b;
	data_array[4] = 0x00000301;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xa0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel pad mapping control
	data_array[1] = 0x000000cc;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x002d2e05;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xb0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel pad mapping control
	data_array[1] = 0x090f0dcc;
	data_array[2] = 0x0001030b;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xc0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel pad mapping control
	data_array[1] = 0x000000cc;
	data_array[2] = 0x2e2d0600;
	data_array[3] = 0x0c0a100e;
	data_array[4] = 0x00000204;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xd0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel pad mapping control
	data_array[1] = 0x000000cc;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x002e2d05;
	dsi_set_cmdq(&data_array, 5, 1);
	
	/*===============Panel Timing Setting====================*/
	data_array[0] = 0x80002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000d2902;	//Panel VST Setting
	data_array[1] = 0x18038bce;
	data_array[2] = 0x8918038a;
	data_array[3] = 0x03881803;
	data_array[4] = 0x00000018;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel vend setting
	data_array[1] = 0x180f38ce;
	data_array[2] = 0x00180e38;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xa0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clka1/2 setting
	data_array[1] = 0x050738ce;
	data_array[2] = 0x00180000;
	data_array[3] = 0x01050638;
	data_array[4] = 0x00001800;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xb0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clka3/4 setting
	data_array[1] = 0x050538ce;
	data_array[2] = 0x00180002;
	data_array[3] = 0x03050438;
	data_array[4] = 0x00001800;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xc0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clkb1/2 setting
	data_array[1] = 0x050338ce;
	data_array[2] = 0x00180004;
	data_array[3] = 0x05050238;
	data_array[4] = 0x00001800;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xd0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clkb3/4 setting
	data_array[1] = 0x050138ce;
	data_array[2] = 0x00180006;
	data_array[3] = 0x07050038;
	data_array[4] = 0x00001800;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0x80002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clkc1/2 setting
	data_array[1] = 0x000000cf;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clkc3/4 setting
	data_array[1] = 0x000000cf;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xa0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clkd1/2 setting
	data_array[1] = 0x000000cf;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xb0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clkd3/4 setting
	data_array[1] = 0x000000cf;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xc0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000c2902;	//gate pre. ena
	data_array[1] = 0x200101cf;
	data_array[2] = 0x02000020;
	data_array[3] = 0x08030081;
	dsi_set_cmdq(&data_array, 4, 1);
	
	data_array[0] = 0xb5002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00072902;	//normal output with VGH/VGL
	data_array[1] = 0x7f1138c5;
	data_array[2] = 0x007f1138;
	dsi_set_cmdq(&data_array, 3, 1);
	
	/*===================Gamma====================*/
	data_array[0] = 0x00002300;
	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0] = 0x00112902;
	data_array[1] = 0x1c1605e1;
	data_array[2] = 0x0c11060d;
	data_array[3] = 0x0807020b;
	data_array[4] = 0x0b100e07;
	data_array[5] = 0x00000002;
	dsi_set_cmdq(&data_array, 6 ,1);
	
	data_array[0] = 0x00002300;
	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0] = 0x00112902;
	data_array[1] = 0x1C1605e2;
	data_array[2] = 0x0c12060d;
	data_array[3] = 0x0907020b;
	data_array[4] = 0x0b110e07;
	data_array[5] = 0x00000002;
	dsi_set_cmdq(&data_array, 6 ,1);
	
	data_array[0] = 0x92002300;
	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0] = 0x00032902;
	data_array[1] = 0x000230ff;
	dsi_set_cmdq(&data_array, 2 ,1);
	
	data_array[0] = 0x00002300;
	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0] = 0x00022902;
	data_array[1] = 0x000070d9;
	dsi_set_cmdq(&data_array, 2 ,1);
#else
	unsigned int data_array[16];
	data_array[0] = 0x00042902;
	data_array[1] = 0x018312FF;
	dsi_set_cmdq(&data_array, 2, 1);//EXTC = 1
	
	data_array[0] = 0x80002300;
	dsi_set_cmdq(&data_array, 1, 1);	//Orise mode enable
	
	data_array[0] = 0x00032902;
	data_array[1] = 0x008312FF;
	dsi_set_cmdq(&data_array, 2, 1);
	
	/*===================panel setting====================*/
	data_array[0] = 0x80002300;
	dsi_set_cmdq(&data_array, 1, 1);	//TCON Setting
	
	data_array[0] = 0x000A2902;
	data_array[1] = 0x006400C0;
	data_array[2] = 0x6400110F;
	data_array[3] = 0x0000110F;
	dsi_set_cmdq(&data_array, 4, 1);
	
	data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);	//Panel Timing Setting
	
	data_array[0] = 0x00072902;
	data_array[1] = 0x005C00C0;
	data_array[2] = 0x00040001;
	dsi_set_cmdq(&data_array, 3, 1);
	
	data_array[0] = 0xA4002300;
	dsi_set_cmdq(&data_array, 1, 1);	//Source pre.
	
	data_array[0] = 0x1CC02300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0xB3002300;
	dsi_set_cmdq(&data_array, 1, 1);	//Interval Scan Frame: 0 frame, column inversion
	
	data_array[0] = 0x00032902;
	data_array[1] = 0x005000C0;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0x81002300;
	dsi_set_cmdq(&data_array, 1, 1);	//frame rate: 60Hz
	
	data_array[0] = 0x55C12300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x81002300;				//Source bias 0.75uA
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x82C42300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x90002300;				//clock delay for data latch
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x49C42300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	/*================Power setting===============*/
	data_array[0] = 0xA0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000F2902;	//DCDC setting
	data_array[1] = 0x061005C4;
	data_array[2] = 0x10150502;
	data_array[3] = 0x02071005;
	data_array[4] = 0x00101505;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xB0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//Clamp coltage setting
	data_array[1] = 0x000000C4;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0x91002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VGH=12v, VGL=-12v, pump ratio: VGH=6x, VGL=-5x
	data_array[1] = 0x005019c5;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0x00002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//GVDD=4.87v, NGVDD = -4.87V
	data_array[1] = 0x00bcbcd8;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0x00002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0xd9652300;		//VCOMDC=-1.1
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0xB0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VDD_18v=1.6v, LVDSVDD=1.55v
	data_array[1] = 0x00b804c5;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0xBB002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x80c52300;	//LVD voltage level setting
	dsi_set_cmdq(&data_array, 1, 1);
	
	/*=========================Control setting======================*/
	data_array[0] = 0x00002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x40d02300;		//ID1
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//ID2, ID3
	data_array[1] = 0x000000d1;
	dsi_set_cmdq(&data_array, 2, 1);
	
	/*=========================Panel Timming State Control================*/
	data_array[0] = 0x80002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000C2902;	//Panel timing state control
	data_array[1] = 0x000000cb;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	dsi_set_cmdq(&data_array, 4, 1);
	
	data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel timing state control
	data_array[1] = 0x000000cb;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xa0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel timing state control
	data_array[1] = 0x000000cb;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xb0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel timing state control
	data_array[1] = 0x000000cb;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xc0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel timing state control
	data_array[1] = 0x050505cb;
	data_array[2] = 0x00050505;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xd0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel timing state control
	data_array[1] = 0x000000cb;
	data_array[2] = 0x05050000;
	data_array[3] = 0x05050505;
	data_array[4] = 0x00000505;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xe0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000F2902;	//Panel timing state control
	data_array[1] = 0x000000cb;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00050500;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xf0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000c2902;	//Panel timing state control
	data_array[1] = 0xffffffcb;
	data_array[2] = 0xffffffff;
	data_array[3] = 0xffffffff;
	dsi_set_cmdq(&data_array, 4, 1);
	
	/*===============Panel pad mapping control===============*/
	data_array[0] = 0x80002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel pad mapping control
	data_array[1] = 0x0e0c0acc;
	data_array[2] = 0x00040210;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel pad mapping control
	data_array[1] = 0x000000cc;
	data_array[2] = 0x2d2e0000;
	data_array[3] = 0x0f0d0b09;
	data_array[4] = 0x00000301;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xa0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel pad mapping control
	data_array[1] = 0x000000cc;
	data_array[2] = 0x00040210;
	data_array[3] = 0x00000000;
	data_array[4] = 0x002d2e00;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xb0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel pad mapping control
	data_array[1] = 0x0b0d0fcc;
	data_array[2] = 0x00010309;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xc0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel pad mapping control
	data_array[1] = 0x000000cc;
	data_array[2] = 0x2e2d0000;
	data_array[3] = 0x0a0c0e10;
	data_array[4] = 0x00000204;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xd0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel pad mapping control
	data_array[1] = 0x000000cc;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x002e2d00;
	dsi_set_cmdq(&data_array, 5, 1);
	
	/*===============Panel Timing Setting====================*/
	data_array[0] = 0x80002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000d2902;	//Panel VST Setting
	data_array[1] = 0x18038fce;
	data_array[2] = 0x8d18038e;
	data_array[3] = 0x038c1803;
	data_array[4] = 0x00000018;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel vend setting
	data_array[1] = 0x000000ce;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xa0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clka1/2 setting
	data_array[1] = 0x050b38ce;
	data_array[2] = 0x00000000;
	data_array[3] = 0x01050a38;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xb0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clka3/4 setting
	data_array[1] = 0x050938ce;
	data_array[2] = 0x00000002;
	data_array[3] = 0x03050838;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xc0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clkb1/2 setting
	data_array[1] = 0x050738ce;
	data_array[2] = 0x00000004;
	data_array[3] = 0x05050638;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xd0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clkb3/4 setting
	data_array[1] = 0x050538ce;
	data_array[2] = 0x00000006;
	data_array[3] = 0x07050438;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0x80002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clkc1/2 setting
	data_array[1] = 0x000000cf;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clkc3/4 setting
	data_array[1] = 0x000000cf;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xa0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clkd1/2 setting
	data_array[1] = 0x000000cf;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xb0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clkd3/4 setting
	data_array[1] = 0x000000cf;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xc0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000c2902;	//gate pre. ena
	data_array[1] = 0x200101cf;
	data_array[2] = 0x01000020;
	data_array[3] = 0x08030081;
	dsi_set_cmdq(&data_array, 4, 1);
	
	data_array[0] = 0xb5002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00072902;	//normal output with VGH/VGL
	data_array[1] = 0xfff133c5;
	data_array[2] = 0x00fff133;
	dsi_set_cmdq(&data_array, 3, 1);
	
	data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00052902;	//mode-3
	data_array[1] = 0x021102f5;
	data_array[2] = 0x00000011;
	dsi_set_cmdq(&data_array, 3, 1);
	
	data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x50c52300;	//2xVPNL, 1.5*=00, 2*=50, 3*=A0
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x94002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x66c52300;	//Frequency
	dsi_set_cmdq(&data_array, 1, 1);
	
	/*===============VGL01/02 disable================*/
	data_array[0] = 0xb2002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VGL01
	data_array[1] = 0x000000f5;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0xb4002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VGL01_S
	data_array[1] = 0x000000f5;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0xb6002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VGL02
	data_array[1] = 0x000000f5;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0xb8002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VGL02_S
	data_array[1] = 0x000000f5;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0xb4002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0xC0C52300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	/*===================Gamma====================*/
	data_array[0] = 0x00002300;
	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0] = 0x00112902;
	data_array[1] = 0x231d0ae1;
	data_array[2] = 0x0a0f040d;
	data_array[3] = 0x0a060309;
	data_array[4] = 0x0d110e05;
	data_array[5] = 0x00000001;
	dsi_set_cmdq(&data_array, 6 ,1);
	
	data_array[0] = 0x00002300;
	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0] = 0x00112902;
	data_array[1] = 0x221d0ae2;
	data_array[2] = 0x0b0e040e;
	data_array[3] = 0x0906020a;
	data_array[4] = 0x0d110e05;
	data_array[5] = 0x00000001;
	dsi_set_cmdq(&data_array, 6 ,1);
	
	data_array[0] = 0x00002300;
	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0] = 0x00042902;
	data_array[1] = 0x000000ff;
	dsi_set_cmdq(&data_array, 2 ,1);
#endif
}
static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;

    for(i = 0; i < count; i++) {
		
        unsigned cmd;
        cmd = table[i].cmd;
		
        switch (cmd) {
			
            case REGFLAG_DELAY :
                MDELAY(table[i].count);
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
#else
		params->dsi.mode   = BURST_VDO_MODE;
#endif
	
		// DSI
		/* Command mode setting */
		params->dsi.LANE_NUM				= LCM_FOUR_LANE;
            //The following defined the fomat for data coming from LCD engine.
            params->dsi.data_format.color_order     = LCM_COLOR_ORDER_RGB;
            params->dsi.data_format.trans_seq       = LCM_DSI_TRANS_SEQ_MSB_FIRST;
            params->dsi.data_format.padding         = LCM_DSI_PADDING_ON_LSB;
            params->dsi.data_format.format              = LCM_DSI_FORMAT_RGB888;

            // Highly depends on LCD driver capability.
                 params->dsi.packet_size=256;

		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

		params->dsi.vertical_sync_active				= 2;
		params->dsi.vertical_backporch					= 14;
		params->dsi.vertical_frontporch					= 16;
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 

		params->dsi.horizontal_sync_active				= 2;
		params->dsi.horizontal_backporch				= 42;
		params->dsi.horizontal_frontporch				= 44;
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

#ifndef CONFIG_FPGA_EARLY_PORTING
                params->dsi.PLL_CLOCK = 220; //this value must be in MTK suggested table
#else
                params->dsi.pll_div1 = 0;
                params->dsi.pll_div2 = 0;
                params->dsi.fbk_div = 0x1;
#endif
		params->dsi.esd_check_enable = 1;
		params->dsi.customization_esd_check_enable = 1;
		params->dsi.lcm_esd_check_table[0].cmd 			= 0x0a;
		params->dsi.lcm_esd_check_table[0].count 		= 1;
		params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;
		params->dsi.vertical_vfp_lp = 100;

}

static unsigned int lcm_compare_id(void)
{
		unsigned int id = 0;
		unsigned char buffer[2];
		unsigned int array[16];
			SET_RESET_PIN(1);  //NOTE:should reset LCM firstly
			SET_RESET_PIN(0);
			MDELAY(1);
			SET_RESET_PIN(1);
			MDELAY(150);
	
	//	push_table(lcm_compare_id_setting, sizeof(lcm_compare_id_setting) / sizeof(struct LCM_setting_table), 1);
	
		array[0] = 0x00023700;// read id return two byte,version and id
		dsi_set_cmdq(array, 1, 1);
		read_reg_v2(0xDA, buffer, 1);
	
		id = buffer[0]; //we only need ID
      #ifdef BUILD_UBOOT
		printf("%s,  id otm1283A= 0x%08x\n", __func__, id);
	  #endif
		return (LCM_ID_OTM1283 == id)?1:0;


}


static void lcm_init(void)
{
	unsigned int data_array[16];

	unsigned char cmd = 0x0;
	unsigned char data = 0xFF;
	int ret=0;
	cmd=0x00;
	data=0x0E;
#ifndef CONFIG_FPGA_EARLY_PORTING
	mt_set_gpio_mode(GPIO_65132_ENP, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_ENP, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_ENP, GPIO_OUT_ONE);
	MDELAY(10);
	mt_set_gpio_mode(GPIO_65132_ENN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_ENN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_ENN, GPIO_OUT_ONE);

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
	MDELAY(10);//5
    SET_RESET_PIN(0);
	MDELAY(120);//50
    SET_RESET_PIN(1);
	MDELAY(10);//100

	lcm_init_registers();
	data_array[0] = 0x00352500;
	dsi_set_cmdq(&data_array, 1, 1);
	data_array[0] = 0x00362500;
	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0]= 0x00023902;
	data_array[1]= 0x0051;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]= 0x00023902;
	data_array[1]= 0x2453;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]= 0x00023902;
	data_array[1]= 0x0055;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]= 0x00023902;
	data_array[1]= 0x705e;
	dsi_set_cmdq(data_array, 2, 1);

	push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
#ifndef BUILD_LK	
	// Refresh value of backlight level.for esd check recovery because lcm init need set backlight as 0.
	lcm_backlight_level_setting[0].para_list[0] = 25;
	push_table(lcm_backlight_level_setting, sizeof(lcm_backlight_level_setting) / sizeof(struct LCM_setting_table), 1);
#endif	
}


static void lcm_suspend(void)
{
	push_table(lcm_sleep_in_setting, sizeof(lcm_sleep_in_setting) / sizeof(struct LCM_setting_table), 1);
    MDELAY(5);
    
    SET_RESET_PIN(0);
	MDELAY(1);
    SET_RESET_PIN(1);

	mt_set_gpio_mode(GPIO_65132_ENP, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_ENP, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_ENP, GPIO_OUT_ZERO);

	mt_set_gpio_mode(GPIO_65132_ENN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_ENN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_ENN, GPIO_OUT_ZERO);

    SET_RESET_PIN(0);
}



static void lcm_resume(void)
{

	lcm_init();
//	push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
}

#if (LCM_DSI_CMD_MODE)
static void lcm_update(unsigned int x, unsigned int y,
                       unsigned int width, unsigned int height)
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
#endif
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

static void lcm_set_cmd(void* handle,int *lcm_cmd,unsigned int cmd_num)
{
#ifdef BUILD_LK
	dprintf(0,"%s,lk nt35595 set cmd: num = %d\n", __func__, cmd_num);
#else
	printk("%s, kernel nt35595 set cmd: num = %d\n", __func__, cmd_num);
    if(cmd_num==0)
		return;
//customize example 
	unsigned int cmd = 0x51;
	unsigned int count =1;
    unsigned int level = lcm_cmd[0];
	printk("[lcm_set_cmd mode = %d\n", lcm_cmd[0]);
	dsi_set_cmd_by_cmdq(handle, cmd, count, &level, 1);
//	
#endif
}
LCM_DRIVER otm1283a_hd720_dsi_vdo_tm_lcm_drv = 
{
    .name			= "otm1283a_dsi_vdo_tm",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id     = lcm_compare_id,
	.init_power		= lcm_init_power,
    .resume_power = lcm_resume_power,
    .suspend_power = lcm_suspend_power,
	.set_backlight	= lcm_setbacklight,
	     .set_cmd = lcm_set_cmd,
#if (LCM_DSI_CMD_MODE)
    .update         = lcm_update,
#endif
};
