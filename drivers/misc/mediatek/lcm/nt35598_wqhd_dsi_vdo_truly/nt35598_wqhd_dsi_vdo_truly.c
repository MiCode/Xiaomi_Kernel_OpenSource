/* BEGIN PN:DTS2013053103858 , Added by d00238048, 2013.05.31*/
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
    //#include <linux/delay.h>
    #include <mach/mt_gpio.h>
#endif
#include <cust_gpio_usage.h>
#include <cust_i2c.h>
#ifdef BUILD_LK
#define LCD_DEBUG(fmt)  dprintf(CRITICAL,fmt)
#else
#define LCD_DEBUG(fmt)  printk(fmt)
#endif
//static unsigned char lcd_id_pins_value = 0xFF;
static const unsigned char LCD_MODULE_ID = 0x01; //  haobing modified 2013.07.11
// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------
#define LCM_DSI_CMD_MODE									0
#define FRAME_WIDTH  										(1440)
#define FRAME_HEIGHT 										(2560)
#define GPIO_65132_EN GPIO_LCD_BIAS_ENP_PIN

#define REGFLAG_PORT_SWAP									0xFFFA
#define REGFLAG_DELAY             							0xFFFC
#define REGFLAG_END_OF_TABLE      							0xFFFD   // END OF REGISTERS MARKER


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

static const unsigned int BL_MIN_LEVEL =20;
static LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))
#define MDELAY(n) 											(lcm_util.mdelay(n))

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)										lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   			lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)    
#define dsi_swap_port(swap)   								lcm_util.dsi_swap_port(swap)

#if 1
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
#endif
struct LCM_setting_table {
    unsigned int cmd;
    unsigned char count;
    unsigned char para_list[64];
};
//update initial param for IC nt35520 0.01
static struct LCM_setting_table lcm_initialization_setting[] = 
{
{REGFLAG_DELAY, 200, {}},
{0xFF, 3,  {0x12,0x82,0x01}},
{0xFF, 4,  {0xAA,0x55,0xA5,0x80}},
{0xF0, 5,{0x55,0xAA,0x52,0x08,0x00}},						//---------------Page 0 Enable-----------------------------------------------------------------------
{0xB1, 1,{0x21}},											//Display Option Control: 1440xRGB
{0xB2, 2,{0x10, 0x82}},									// Configuration for GPO Pins: GP01=PWM, GPO2=TE, GPO3=HS
{0xB2, 2,{0x87, 0x22}},									// Configuration for GPO Pins: GP00=VS, GPO1=HS, GPO3=TE
{0xB4, 3,{0x25,0x02,0x8C}},								//MIPI DET ON
{0xB5, 2,{0x0A, 0x00}},									// Display Scan Line Control: 2560 line
{0xB8, 3,{0x00, 0x04, 0x02}},								// EQ Control Function for Source Driver
{0xB9, 9,{0x03,0x00,0x18,0x11,0x31,0x44,0x03,0x51,0x00}},	//	MUX Signal Control (11/12 updated)
{0xBA, 2,{0x00, 0x32}},									// Source Control in Vertical Porch Time
{0xBC, 2,{0x4F, 0x00}},									// Inversion Driving Control 
{0xBD, 4,{0x00,0xB6,0x10,0x10}},							//Display timing control
{0xC6, 2,{0x11, 0x10}},									// Source Chopper Control
{0xF0, 5,{0x55,0xAA,0x52,0x08,0x01}},						//---------------Page 1 Enable-----------------------------------------------------------------------
{0xB1, 2,{0x11, 0x11}},									//Setting AVDDR=4.6V and AVEER=-4.6V Voltage 
{0xB2, 6,{0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D}},				//Setting VGH= 8.5 + 1= 9.5V and VGLX=-8.5 - 1 = -9.5V Voltage
{0xB3, 6,{0x19, 0x23, 0x19, 0x23, 0x19, 0x23}},				//Setting VRGH=8.5 and VGL_REG=-8.5  Voltage
{0xB6, 6,{0x15, 0x14, 0x15, 0x14, 0x15, 0x14}},				// BT4 and BT5 Power Control for VGH=AVDD-AVEE and VGLX=AVEE-AVDD
{0xBC, 2,{0x69, 0x00}},									// Setting VGMP=4.1V and VGSP=0V Voltage
{0xBD, 2,{0x69, 0x00}},									// Setting VGMN=-4.1V and VGSN=0V Voltage
{0xBE, 2,{0x00, 0xC8}},									// Setting VCOM=0 Offset Voltage
{0xCA, 1,{0x0F}},											// Gate Signal Voltages Control: Enable VRGH/VGL_REG regulator
{0xF0, 5,{0x55,0xAA,0x52,0x08,0x02}},						//---------------Page 2 Enable-----------------------------------------------------------------------
{0xB0, 14,{0x00, 0x09,0x1B,0x28,0x33,0x3D,0x45,0x30,0x2D,0x32,0x31,0x47,0x56,0x56}},//Positive ?コed??
{0xB1, 15,{0x4E, 0x60,0x74,0x74,0x86,0x8E,0x9C,0x9C,0xA5,0xAD,0xB7,0xC2,0xCE,0xDA,0xFF}},
{0xB2, 14,{0x00, 0x09,0x1B,0x28,0x33,0x3D,0x45,0x30,0x2D,0x32,0x31,0x47,0x56,0x56}},//Positive ?エreen??
{0xB3, 15,{0x4E, 0x60,0x74,0x74,0x86,0x8E,0x9C,0x9C,0xA5,0xAD,0xB7,0xC2,0xCE,0xDA,0xFF}},
{0xB4, 14,{0x00, 0x09,0x1B,0x28,0x33,0x3D,0x45,0x30,0x2D,0x32,0x31,0x47,0x56,0x56}},//Positive ?ィlue??
{0xB5, 15,{0x4E, 0x60,0x74,0x74,0x86,0x8E,0x9C,0x9C,0xA5,0xAD,0xB7,0xC2,0xCE,0xDA,0xFF}},
{0xB6, 14,{0x00, 0x09,0x1B,0x28,0x33,0x3D,0x45,0x30,0x2D,0x32,0x31,0x47,0x56,0x56}},//Negative ?コed??
{0xB7, 15,{0x4E, 0x60,0x74,0x74,0x86,0x8E,0x9C,0x9C,0xA5,0xAD,0xB7,0xC2,0xCE,0xDA,0xFF}},
{0xB8, 14,{0x00, 0x09,0x1B,0x28,0x33,0x3D,0x45,0x30,0x2D,0x32,0x31,0x47,0x56,0x56}},//Negative ?エreen??
{0xB9, 15,{0x4E, 0x60,0x74,0x74,0x86,0x8E,0x9C,0x9C,0xA5,0xAD,0xB7,0xC2,0xCE,0xDA,0xFF}},
{0xBA, 14,{0x00, 0x09,0x1B,0x28,0x33,0x3D,0x45,0x30,0x2D,0x32,0x31,0x47,0x56,0x56}},//Negative ?ィlue??
{0xBB, 15,{0x4E, 0x60,0x74,0x74,0x86,0x8E,0x9C,0x9C,0xA5,0xAD,0xB7,0xC2,0xCE,0xDA,0xFF}},
{0xF0, 5,{0x55,0xAA,0x52,0x08,0x03}},						//---------------Page 3 Enable-----------------------------------------------------------------------
{0xB0, 4,{0x00,0x00,0x00,0x00}},							// EQ Control for Signal Group CLK (10/26 updated)
{0xB1, 4,{0x03,0x00,0x00,0x03}},							// EQ Control for Signal Group MUX
{0xB2, 7,{0x00,0x00,0x64,0x00,0x64,0x05,0x02}},				//Control for Signal Type STV01 (10/26 updated)
{0xB4, 2,{0x00,0x08}},									// Control for Signal Type PRECH (10/26 updated)
{0xB5, 4,{0x12,0x28,0x06,0x06}},							// Control for Signal Type MUX (10/31 Update)
{0xBA, 5,{0x31,0x00,0x00,0x00,0x03}},						// Control for Signal Type CLK01 (10/26 Update)
{0xBB, 5,{0x31,0x00,0x00,0x00,0x03}},						// Control for Signal Type CLK02 (10/26 Update)
{0xF0, 5,{0x55,0xAA,0x52,0x08,0x05}},						//---------------Page 5 Enable-----------------------------------------------------------------------
{0xB0, 3,{0x03,0x11,0x3F}},								//Gate Signal Output Control
{0xB2, 2,{0x06,0x60}},									// Power On/Off Control for MUX Signal Type
{0xB3, 2,{0x10,0x33}},									// Power On/Off Control for VCOM Output
{0xB4, 1,{0x26}},											// Power On/Off Control for STV Signal Type
{0xB5,3,{ 0x06,0x20,0x00}},								// Power On/Off Control for CLK01 & CLK02 Signal Type
{0xB6,3,{ 0x86,0xE0,0x00}},
{0xBA, 5,{0x8E,0x00,0x00,0xA4,0x00}},						// Power On/Off Control for VDC01~ VDC06 Signal Type(10/3 Update)
{0xBB, 5,{0x06,0x00,0x00,0x20,0x00}},
{0xBC, 5,{0x8E,0x00,0x00,0xA4,0x00}},
{0xBD, 5,{0x2E,0x00,0x00,0xA4,0x00}},
{0xBE, 5,{0x8E,0x00,0x00,0xA0,0x00}},
{0xBF, 5,{0x06,0x00,0x00,0x24,0x00}},
{0xC1, 1,{0x00}},											// Operation Initial Control
{0xC8, 2,{0x05,0x10}},									// CLK01 & CLK02 Pulse Waveform Control(10/26 Update)
{0xC9, 2,{0x03,0x10}},
{0xD0, 3,{0x00,0x0A,0x02}},								// CLK01 & CLK02 Status Control in Vertical Porch Area(10/26 Update)
{0xD1, 3,{0x00,0x0A,0x04}},
{0xEC, 1,{0x12}},											// Power Stop Control in Power Off: End of 3nd frame
{0xED, 1,{0x00}},											// Display Off Status Control: Source:Vmin 
{0xEE, 2,{0x03, 0x00}},									// Source Control in Different Status: on clear:1-frame, off clear:1-frame , Source:Vmin 
{0xF0, 5,{0x55,0xAA,0x52,0x08,0x06}},						//---------------Page 6 Enable-----------------------------------------------------------------------
{0xB0, 5,{0x00,0x04,0x08,0x11,0x12}},						// GOUT1_L~GOUT28_L Signal Type Select for Forward Scan 
{0xB1, 5,{0x1F,0x15,0x16,0x1F,0x1F}},
{0xB2, 5,{0x1F,0x13,0x1F,0x1F,0x1F}},
{0xB3, 5,{0x1F,0x19,0x19,0x19,0x19}},
{0xB4, 5,{0x1B,0x1B,0x1B,0x1B,0x1D}},
{0xB5, 3,{0x1D,0x1D,0x1D}},
{0xB6, 5,{0x00,0x05,0x09,0x11,0x12}},						// GOUT1_R~GOUT28_R Signal Type Select for Forward Scan
{0xB7, 5,{0x1F,0x15,0x16,0x1F,0x1F}},
{0xB8, 5,{0x1F,0x13,0x1F,0x1F,0x1F}},
{0xB9, 5,{0x1F,0x19,0x19,0x19,0x19}},
{0xBA, 5,{0x1B,0x1B,0x1B,0x1B,0x1D}},
{0xBB, 3,{0x1D,0x1D,0x1D}},
{0xC0, 5,{0x00,0x09,0x05,0x12,0x11}},						// GOUT1_L~GOUT28_L Signal Type Select for Backward Scan
{0xC1, 5,{0x1F,0x15,0x16,0x1F,0x1F}},
{0xC2, 5,{0x1F,0x13,0x1F,0x1F,0x1F}},
{0xC3, 5,{0x1F,0x19,0x19,0x19,0x19}},
{0xC4, 5,{0x1B,0x1B,0x1B,0x1B,0x1D}},
{0xC5, 3,{0x1D,0x1D,0x1D}},
{0xC6, 5,{0x00,0x08,0x04,0x12,0x11}},						// GOUT1_R~GOUT5_R Signal Type Select for Backward Scan
{0xC7, 5,{0x1F,0x15,0x16,0x1F,0x1F}},
{0xC8, 5,{0x1F,0x13,0x1F,0x1F,0x1F}},
{0xC9, 5,{0x1F,0x19,0x19,0x19,0x19}},
{0xCA, 5,{0x1B,0x1B,0x1B,0x1B,0x1D}},
{0xCB, 3,{0x1D,0x1D,0x1D}},
{0xD0, 3,{0x00, 0xAA, 0x0A}},								// MUX Signal L/R Output Level Swap
#if 1
{0xFF, 4,{0xAA, 0x55, 0xA5, 0x80}},
{0xFE, 1,{0x01}},
{REGFLAG_PORT_SWAP, 0, {}},
{0xFF, 4,{0xAA, 0x55, 0xA5, 0x00}},
#endif
{0x35, 1,{0x00}},	 										//TE ON
{0x11, 0,{}},												// Sleep Out
{REGFLAG_DELAY, 120, {}},
{0x29, 0,{}},												// Display On
{REGFLAG_DELAY, 40, {}}
};
static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28,0,{}},
	{0x10,0,{}},
	{REGFLAG_DELAY, 120, {}}
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
#endif

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
    // Display off sequence
    {0x28, 1, {0x00}},
    {REGFLAG_DELAY, 20, {}},

    // Sleep Mode On
    {0x10, 1, {0x00}},
    {REGFLAG_DELAY, 120, {}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
    unsigned int i;

    for(i = 0; i < count; i++)
    {
        unsigned int cmd;
        cmd = table[i].cmd;

        switch (cmd) {

            case REGFLAG_DELAY :
                if(table[i].count <= 10)
                    MDELAY(table[i].count);
                else
                    MDELAY(table[i].count);
                break;

            case REGFLAG_END_OF_TABLE :
                break;
			case REGFLAG_PORT_SWAP:
				dsi_swap_port(1);
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

#else
  
//	extern int mt8193_i2c_write(u16 addr, u32 data);
//	extern int mt8193_i2c_read(u16 addr, u32 *data);
	
//	#define TPS65132_write_byte(add, data)  mt8193_i2c_write(add, data)
	//#define TPS65132_read_byte(add)  mt8193_i2c_read(add)
  

#endif

static void lcm_init_power(void)
{
#if 1
	unsigned char cmd = 0x0;
	unsigned char data = 0xFF;
	int ret=0;
	cmd=0x00;
	data=0x0a;
	mt_set_gpio_mode(GPIO_65132_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_EN, GPIO_OUT_ONE);

#ifdef BUILD_LK
	mt6331_upmu_set_rg_vgp1_en(1);
	mt6331_upmu_set_rg_vcam_io_en(1);
	ret=TPS65132_write_byte(cmd,data);
	if(ret) 	
	dprintf(0, "[LK]nt35598----tps6132----cmd=%0x--i2c write error----\n",cmd); 	
	else
	dprintf(0, "[LK]nt35598----tps6132----cmd=%0x--i2c write success----\n",cmd);			
#else
	ret=tps65132_write_bytes(cmd,data);
	if(ret<0)
	printk("[KERNEL]nt35598----tps6132---cmd=%0x-- i2c write error-----\n",cmd);
	else
	printk("[KERNEL]nt35598----tps6132---cmd=%0x-- i2c write success-----\n",cmd);
#endif
	cmd=0x01;
	data=0x0a;
#ifdef BUILD_LK
	ret=TPS65132_write_byte(cmd,data);
	if(ret) 	
		dprintf(0, "[LK]nt35598----tps6132----cmd=%0x--i2c write error----\n",cmd); 	
	else
		dprintf(0, "[LK]nt35598----tps6132----cmd=%0x--i2c write success----\n",cmd);	
#else
	ret=tps65132_write_bytes(cmd,data);
	if(ret<0)
	printk("[KERNEL]nt35598----tps6132---cmd=%0x-- i2c write error-----\n",cmd);
	else
	printk("[KERNEL]nt35598----tps6132---cmd=%0x-- i2c write success-----\n",cmd);
#endif
#endif

}

static void lcm_resume_power(void)
{
#if 1
	unsigned char cmd = 0x0;
	unsigned char data = 0xFF;
	int ret=0;
	cmd=0x00;
	data=0x0a;
	mt_set_gpio_mode(GPIO_65132_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_EN, GPIO_OUT_ONE);

#ifdef BUILD_LK
	mt6331_upmu_set_rg_vgp1_en(1);
	mt6331_upmu_set_rg_vcam_io_en(1);
	ret=TPS65132_write_byte(cmd,data);
	if(ret) 	
	dprintf(0, "[LK]nt35598----tps6132----cmd=%0x--i2c write error----\n",cmd); 	
	else
	dprintf(0, "[LK]nt35598----tps6132----cmd=%0x--i2c write success----\n",cmd);			
#else
	ret=tps65132_write_bytes(cmd,data);
	if(ret<0)
	printk("[KERNEL]nt35598----tps6132---cmd=%0x-- i2c write error-----\n",cmd);
	else
	printk("[KERNEL]nt35598----tps6132---cmd=%0x-- i2c write success-----\n",cmd);
#endif
	cmd=0x01;
	data=0x0a;
#ifdef BUILD_LK
	ret=TPS65132_write_byte(cmd,data);
	if(ret) 	
		dprintf(0, "[LK]nt35598----tps6132----cmd=%0x--i2c write error----\n",cmd); 	
	else
		dprintf(0, "[LK]nt35598----tps6132----cmd=%0x--i2c write success----\n",cmd);	
#else
	ret=tps65132_write_bytes(cmd,data);
	if(ret<0)
	printk("[KERNEL]nt35598----tps6132---cmd=%0x-- i2c write error-----\n",cmd);
	else
	printk("[KERNEL]nt35598----tps6132---cmd=%0x-- i2c write success-----\n",cmd);
#endif
#endif

}

static void lcm_suspend_power(void)
{
	mt_set_gpio_mode(GPIO_65132_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_EN, GPIO_OUT_ZERO);
}

static void lcm_init(void)
{
#if 0
	unsigned char cmd = 0x0;
	unsigned char data = 0xFF;
	int ret=0;
	cmd=0x00;
	data=0x0a;
	mt_set_gpio_mode(GPIO_65132_EN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_65132_EN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_65132_EN, GPIO_OUT_ONE);

	#ifdef BUILD_LK
	mt6331_upmu_set_rg_vgp1_en(1);
  	mt6331_upmu_set_rg_vcam_io_en(1);
	ret=TPS65132_write_byte(cmd,data);
    if(ret)    	
    dprintf(0, "[LK]nt35598----tps6132----cmd=%0x--i2c write error----\n",cmd);    	
	else
	dprintf(0, "[LK]nt35598----tps6132----cmd=%0x--i2c write success----\n",cmd);    		
	#else
	ret=tps65132_write_bytes(cmd,data);
	if(ret<0)
	printk("[KERNEL]nt35598----tps6132---cmd=%0x-- i2c write error-----\n",cmd);
	else
	printk("[KERNEL]nt35598----tps6132---cmd=%0x-- i2c write success-----\n",cmd);
	#endif
	MDELAY(500);
	cmd=0x01;
	data=0x0a;
#ifdef BUILD_LK
	ret=TPS65132_write_byte(cmd,data);
    if(ret)    	
	    dprintf(0, "[LK]nt35598----tps6132----cmd=%0x--i2c write error----\n",cmd);    	
	else
		dprintf(0, "[LK]nt35598----tps6132----cmd=%0x--i2c write success----\n",cmd);   
#else
	ret=tps65132_write_bytes(cmd,data);
	if(ret<0)
	printk("[KERNEL]nt35598----tps6132---cmd=%0x-- i2c write error-----\n",cmd);
	else
	printk("[KERNEL]nt35598----tps6132---cmd=%0x-- i2c write success-----\n",cmd);
#endif
#endif

	SET_RESET_PIN(1);
    MDELAY(10);
    SET_RESET_PIN(0);
    MDELAY(10);
    SET_RESET_PIN(1);
    MDELAY(10);

	// when phone initial , config output high, enable backlight drv chip  
    push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);  



}


static void lcm_suspend(void)
{
 push_table(lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);  
}
static void lcm_resume(void)
{
	lcm_init(); 
}
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
         /*BEGIN PN:DTS2013013101431 modified by s00179437 , 2013-01-31*/
         //delete high speed packet
	//data_array[0]=0x00290508;
	//dsi_set_cmdq(data_array, 1, 1);
         /*END PN:DTS2013013101431 modified by s00179437 , 2013-01-31*/
	
	data_array[0]= 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);	
}


static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type   = LCM_TYPE_DSI;

	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->lcm_if = LCM_INTERFACE_DSI_DUAL;
	params->lcm_cmd_if = LCM_INTERFACE_DSI0;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode   = CMD_MODE;
#else
    params->dsi.mode   = SYNC_PULSE_VDO_MODE;
#endif

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

	params->dsi.vertical_sync_active				= 4;
	params->dsi.vertical_backporch					= 4;
	params->dsi.vertical_frontporch					= 8;
	params->dsi.vertical_active_line					= FRAME_HEIGHT;

	params->dsi.horizontal_sync_active				= 8;
	params->dsi.horizontal_backporch				= 60;
	params->dsi.horizontal_frontporch				= 60;
	params->dsi.horizontal_active_pixel				= FRAME_WIDTH;
	params->dsi.PLL_CLOCK = 450; //this value must be in MTK suggested table
	params->dsi.ufoe_enable  = 1;
	params->dsi.ufoe_params.lr_mode_en = 1;
	params->dsi.ssc_disable=1;
	
	params->dsi.HS_PRPR=5;
	params->dsi.cont_clock=1;
	//begin:haobing modified
	/*BEGIN PN:DTS2013013101431 modified by s00179437 , 2013-01-31*/
	//improve clk quality
	
	//params->dsi.pll_div1=0;		// div1=0,1,2,3;div1_real=1,2,4,4 ----0: 546Mbps  1:273Mbps
	//params->dsi.pll_div2=1;		// div2=0,1,2,3;div1_real=1,2,4,4	
	//params->dsi.fbk_div =21;    	// fref=26MHz, fvco=fref*(fbk_div)*2/(div1_real*div2_real)	
	/*END PN:DTS2013013101431 modified by s00179437 , 2013-01-31*/
	//end:haobing modified

}

#define LCM_ID_NT35598 (0x98)

static unsigned int lcm_compare_id(void)
{
	unsigned int id=0;
	unsigned char buffer[2];
	unsigned int array[16];  

	SET_RESET_PIN(1);
    MDELAY(10);
    SET_RESET_PIN(0);
    MDELAY(10);
    SET_RESET_PIN(1);
    MDELAY(10);

	array[0] = 0x00023700;// read id return two byte,version and id
	dsi_set_cmdq(array, 1, 1);

	array[0] = 0x00063902;
	array[1] = 0x52AA55F0;
	array[2] = 0x0108;
	dsi_set_cmdq(array, 3, 1);
	
	read_reg_v2(0xC5, buffer, 2);
	id = buffer[1]; //we only need ID
#ifdef BUILD_LK
		dprintf(0, "%s, LK nt35598 debug: nt35598 id = 0x%08x\n", __func__, id);
#else
		printk("%s, kernel nt35598 horse debug: nt35598 id = 0x%08x\n", __func__, id);
#endif

	if(id == LCM_ID_NT35598)
		return 1;
	else
		return 0;

}

LCM_DRIVER nt35598_wqhd_dsi_vdo_truly_lcm_drv=
{
    .name           = "nt35598_wqhd_dsi_vdo_truly",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params     = lcm_get_params,
    .init           = lcm_init,/*tianma init fun.*/
    .suspend        = lcm_suspend,
    .resume         = lcm_resume,
     .compare_id     = lcm_compare_id,
     .init_power		= lcm_init_power,
          .resume_power = lcm_resume_power,
     .suspend_power = lcm_suspend_power,
#if (LCM_DSI_CMD_MODE)
    .update         = lcm_update,
#endif
   
};
/* END PN:DTS2013053103858 , Added by d00238048, 2013.05.31*/
