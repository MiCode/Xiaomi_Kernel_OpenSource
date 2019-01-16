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
#define dsi_set_cmd_by_cmdq_dual(handle,cmd,count,ppara,force_update)    lcm_util.dsi_set_cmdq_V23(handle,cmd,count,ppara,force_update);
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)										lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   			lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)    
#define dsi_swap_port(swap)   								lcm_util.dsi_swap_port(swap)

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


int tps65132_write_bytes(unsigned char addr, unsigned char value)
{	
    int ret = 0;
    struct i2c_client *client = tps65132_i2c_client;

	if(client == NULL)
	{
		printk("ERROR!!tps65132_i2c_client is null\n");
		return 0;
	}
	
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

struct LCM_setting_table {
    unsigned int cmd;
    unsigned char count;
    unsigned char para_list[64];
};

static struct LCM_setting_table lcm_initialization_setting[] = 
{
    {0x36, 1, {0x40}},//LCD Pannel position and display
    {0xB0, 1, {0x00}},
    {0xD6, 1, {0x01}},
#if (LCM_DSI_CMD_MODE)    
    {0xB3, 3,{0x04,0x00,0x00}},//Command mode WEM bit 1
#else    
    {0xB3, 3,{0x14,0x00,0x00}},//Video Mode
#endif    
    {0xB4, 1, {0x00}},
    {0xB6, 2, {0x3A,0xD3}},//740Mbps ~ 1000Mbps
    {0xBE, 1, {0x04}},//This register controls DSI virtual channel of PortA setting.
    {0xC3, 3, {0x00,0x00,0x00}},//This register set to enable VSOUT, HSOUT output
    {0xC5, 1, {0x00}},
    {0xC0, 4, {0x00,0x00,0x00,0x00}},//#SOUT Slew rate adjustment
    //#Display setting 1
    //{0xC1, 35, {0x00,0x61,0x00,0x20,0x8C,0xA4,0x16,0xFB,0xBF,0x98,0x83,0xDC,0x7B,0xCF,0x35,0x74,0x4C,0xF9,0x9F,0x2D,0x95,0x88,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x60,0x23,0x03,0x00,0xFF,0x11}},
      {0xC1, 35, {0x00,0x61,0x00,0x20,0x8C,0xA4,0x16,0xFB,0xBF,0x98,0x83,0x9A,0x7B,0xCF,0x35,0x74,0x4C,0xF9,0x9F,0x2D,0x95,0x88,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x63,0x23,0x03,0x00,0xFF,0x11}},
    //#Display setting 2
    {0xC2, 8, {0x0A,0x0A,0x00,0x08,0x08,0xF0,0x00,0x04}},//vbp,vfp
    //#Source timing setting
    {0xC4, 14, {0x70,0x00,0x00,0x33,0x33,0x033,0x33,0x33,0x33,0x33,0x33,0x01,0x05,0x01}},
    //#LTPS timing setting
    {0xC6, 21, {0x5A,0x29,0x29,0x01,0x01,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x15,0x08,0x5A}},
    //#Panel pin control
    {0xCB, 15, {0x7F,0xE0,0x07,0xFF,0x00,0x00,0x00,0x00,0x54,0xE0,0x07,0x2A,0xC8,0x00,0x00}},
    //#Panel intreface control
    {0xCC, 1, {0x11}},
    //#Sequencer timing control
    {0xD7, 13, {0x82,0xFF,0x21,0x8E,0x8C,0xF1,0x87,0x3F,0x7E,0x10,0x00,0x00,0x8F}},
    //#Sequencer control
    {0xD9, 2, {0x00,0x00}},
    //#Power setting(Charge pump setting
    {0xD0, 4, {0x11,0x17,0x17,0xFD}},
    //#Power setting for internal Power
    {0xD2, 16, {0xCD,0x2B,0x2B,0x33,0x12,0x33,0x33,0x33,0x77,0x77,0x33,0x33,0x33,0x00,0x00,0x00}},
    //vplvl 2b 4v vnlvl  2b -4v  12  4.02V ENTER ABNORMAL SEQUENCE
    //#VCOM setting
    {0xD5, 7, {0x06,0x00,0x00,0x01,0x1E,0x01,0x1E}},
    //Gamma Setting with RGB separated setting ON      
	{0xC7, 30, {0x00,0x0C,0x14,0x1D,0x2C,0x3A,0x44,0x54,0x38,0x40,0x4C,0x59,0x63,0x6B,0x7F,0x00,0x0C,0x14,0x1D,0x2C,0x3A,0x44,0x54,0x38,0x40,0x4C,0x59,0x63,0x6B,0x7F}},    
	{0xC8, 19, {0x01,0x00,0x00,0x00,0x00,0xFC,0xEF,0x00,0x00,0x00,0x00,0xFC,0xEF,0x00,0x00,0x00,0x00,0xFC,0x0F}},
    {0xB8, 7,  {0x57,0x3D,0x19,0x1E,0x0A,0x50,0x50}},
    {0xB9, 7,  {0x6F,0x3D,0x28,0x3C,0x14,0xC8,0xC8}},
	{0xBA, 7,  {0xB5,0x33,0x41,0x64,0x23,0xA0,0xA0}},
    {0xCE, 25, {0x55,0x40,0x49,0x53,0x59,0x5E,0x63,0x68,0x6E,0x74,0x7E,0x8A,0x98,0xA8,0xBB,0xD0,0xFF,0x04,0x00,0x04,0x04,0x00,0x00,0x69,0x5A}},
    //self check module
    {0xB0, 1, {0x03}},//Manufacturer Command Access Protect
	{0x35,1,{0x00}},	
    //{0x51, 1, {0xFF}},
    {0x53, 1, {0x2C}},//BL=1h
    //{0x55, 1, {0x00}},//Write CABC

    //{0xDE, 4, {0x01,0x3F,0xFF,0x10}},  //display the test screen 
    //{0xE5, 4, {0x01,0x3F,0xFF,0x10}}, 
    //{0xB3, 3, {0x00,0x00,0x00}},
    //{0xB0, 1, {0x03}},
	
	//{0x2A, 4, {0x00,0x00,0x05,0x9F}},//	GRAM setting
    //{0x2B, 4, {0x00,0x00,0x09,0xFF}},
    //{0x2C, 0, {}},
    //{REGFLAG_PORT_SWAP, 0, {}},
    {0x29, 0,{}},                                               
    {REGFLAG_DELAY, 20, {}},
    {0x11, 0,{},},   
    {REGFLAG_DELAY, 120, {}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};

#if 0
static struct LCM_setting_table lcm_set_window[] = {
    {0x2A,	4,	{0x00, 0x00, (FRAME_WIDTH>>8), (FRAME_WIDTH&0xFF)}},
    {0x2B,	4,	{0x00, 0x00, (FRAME_HEIGHT>>8), (FRAME_HEIGHT&0xFF)}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};
static struct LCM_setting_table lcm_sleep_out_setting[] = {
    {0x29, 1, {0x00}},
    {REGFLAG_DELAY, 120, {}},

    {0x11, 1, {0x00}},
    {REGFLAG_DELAY, 20, {}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_normal_sleep_mode_in_setting[] = {
    // Display off sequence
    {0x28, 1, {0x00}},
    {REGFLAG_DELAY, 20, {}},

    // Sleep Mode On
    {0x10, 1, {0x00}},
    {REGFLAG_DELAY, 120, {}},
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

    {0xB0, 1, {0x00}},
    {0xB1, 1, {0x01}},
    {REGFLAG_DELAY, 20, {}},

    {REGFLAG_END_OF_TABLE, 0x00, {}}
};
static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28,0,{}},
	{0x10,0,{}},
	{REGFLAG_DELAY, 120, {}}
};

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
#ifdef BUILD_LK
    			dprintf(0, "[LK]REGFLAG_DELAY\n");
#endif
           		if(table[i].count <= 10)
           			MDELAY(table[i].count);
            	else
            		MDELAY(table[i].count);
           		break;

            case REGFLAG_END_OF_TABLE :
            	break;
			case REGFLAG_PORT_SWAP:	
#ifdef BUILD_LK
    			dprintf(0, "[LK]push_table end\n");
#endif
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
    params->dsi.mode   = BURST_VDO_MODE;//have no SYNC_PULSE_VDO_MODE;
#endif

    params->dsi.dual_dsi_type = DUAL_DSI_CMD;
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
	params->dsi.ssc_disable=1;
    //video mode timing

    params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

  	params->dsi.vertical_sync_active				= 4;
    params->dsi.vertical_backporch					= 4;
    params->dsi.vertical_frontporch					= 8;
    params->dsi.vertical_active_line				= FRAME_HEIGHT;

    params->dsi.horizontal_sync_active				= 8;//;
    params->dsi.horizontal_backporch				= 60;//hsa+hbp 60~80;
    params->dsi.horizontal_frontporch				= 150;//>150
    params->dsi.horizontal_active_pixel				= FRAME_WIDTH;
#if (LCM_DSI_CMD_MODE)	
    params->dsi.PLL_CLOCK = 450; //this value must be in MTK suggested table
#else
    params->dsi.PLL_CLOCK = 480; 
#endif        
    params->dsi.ufoe_enable  = 1;
    params->dsi.ufoe_params.lr_mode_en = 1;

	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable      = 0;
	params->dsi.lcm_esd_check_table[0].cmd          = 0x53;//0x0A;
	params->dsi.lcm_esd_check_table[0].count        = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x2C;//0x1C;


	// for R63419A Lane Swap
	params->dsi.lane_swap_en = 1;
	params->dsi.lane_swap[MIPITX_PHY_PORT_1][MIPITX_PHY_LANE_0] 	= MIPITX_PHY_LANE_2;
	params->dsi.lane_swap[MIPITX_PHY_PORT_1][MIPITX_PHY_LANE_1] 	= MIPITX_PHY_LANE_CK;
	params->dsi.lane_swap[MIPITX_PHY_PORT_1][MIPITX_PHY_LANE_2] 	= MIPITX_PHY_LANE_0;
	params->dsi.lane_swap[MIPITX_PHY_PORT_1][MIPITX_PHY_LANE_3] 	= MIPITX_PHY_LANE_1;
	params->dsi.lane_swap[MIPITX_PHY_PORT_1][MIPITX_PHY_LANE_CK] 	= MIPITX_PHY_LANE_3;
	params->dsi.lane_swap[MIPITX_PHY_PORT_1][MIPITX_PHY_LANE_RX] 	= MIPITX_PHY_LANE_2;
	
	params->dsi.lane_swap[MIPITX_PHY_PORT_0][MIPITX_PHY_LANE_0] 	= MIPITX_PHY_LANE_0;
	params->dsi.lane_swap[MIPITX_PHY_PORT_0][MIPITX_PHY_LANE_1] 	= MIPITX_PHY_LANE_3;
	params->dsi.lane_swap[MIPITX_PHY_PORT_0][MIPITX_PHY_LANE_2] 	= MIPITX_PHY_LANE_2;
	params->dsi.lane_swap[MIPITX_PHY_PORT_0][MIPITX_PHY_LANE_3] 	= MIPITX_PHY_LANE_1;
	params->dsi.lane_swap[MIPITX_PHY_PORT_0][MIPITX_PHY_LANE_CK] 	= MIPITX_PHY_LANE_CK;
	params->dsi.lane_swap[MIPITX_PHY_PORT_0][MIPITX_PHY_LANE_RX] 	= MIPITX_PHY_LANE_0;
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
#ifdef BUILD_LK
	mt6331_upmu_set_rg_vgp1_en(1);
#else
	printk("%s, begin\n", __func__);
	hwPowerOn(MT6331_POWER_LDO_VGP1, VOL_DEFAULT, "LCM_DRV");	
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
	MDELAY(5);
#ifdef BUILD_LK
	ret=TPS65132_write_byte(cmd,data);
	if(ret) 	
	dprintf(0, "[LK]r63419----tps6132----cmd=%0x--i2c write error----\n",cmd); 	
	else
	dprintf(0, "[LK]r63419----tps6132----cmd=%0x--i2c write success----\n",cmd);			
#else
	ret=tps65132_write_bytes(cmd,data);
	if(ret<0)
	printk("[KERNEL]r63419----tps6132---cmd=%0x-- i2c write error-----\n",cmd);
	else
	printk("[KERNEL]r63419----tps6132---cmd=%0x-- i2c write success-----\n",cmd);
#endif

	cmd=0x01;
	data=0x0E;
#ifdef BUILD_LK
	ret=TPS65132_write_byte(cmd,data);
	if(ret) 	
		dprintf(0, "[LK]r63419----tps6132----cmd=%0x--i2c write error----\n",cmd); 	
	else
		dprintf(0, "[LK]r63419----tps6132----cmd=%0x--i2c write success----\n",cmd);	
#else
	ret=tps65132_write_bytes(cmd,data);
	if(ret<0)
	printk("[KERNEL]r63419----tps6132---cmd=%0x-- i2c write error-----\n",cmd);
	else
	printk("[KERNEL]r63419----tps6132---cmd=%0x-- i2c write success-----\n",cmd);
#endif

    SET_RESET_PIN(1);
    MDELAY(1);
    //MDELAY(10);
    SET_RESET_PIN(0);
    MDELAY(10);
    SET_RESET_PIN(1);
    MDELAY(10);

    // when phone initial , config output high, enable backlight drv chip  
    push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1); 
	
#ifdef BUILD_LK
    dprintf(0, "[LK]push_table end\n");
#endif
}

static void lcm_suspend(void)
{
	mt_set_gpio_mode(GPIO_65132_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_EN, GPIO_OUT_ZERO);
	push_table(lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);  
	SET_RESET_PIN(0);
}

static void lcm_resume(void)
{
	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(10);
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

#define LCM_ID_R63419 (0x3419)

static unsigned int lcm_compare_id(void)
{
	unsigned char buffer[5];
	unsigned int array[16];  
	int i;
	unsigned int lcd_id = 0;
	SET_RESET_PIN(1);
    MDELAY(10);
    SET_RESET_PIN(0);
    MDELAY(10);
    SET_RESET_PIN(1);
    MDELAY(10);
	array[0] = 0x00053700;// read id return two byte,version and id
	dsi_set_cmdq(array, 1, 1);
  
	read_reg_v2(0xBF, buffer, 5);
	MDELAY(20);
	lcd_id = (buffer[2] << 8 )| buffer[3];

#ifdef BUILD_LK
    dprintf(0, "%s, LK r63419 debug: r63419 id = 0x%08x\n", __func__, lcd_id);
#else
    printk("%s, kernel r63419 horse debug: r63419 id = 0x%08x\n", __func__, lcd_id);
#endif

    if(lcd_id == LCM_ID_R63419)
        return 1;
    else
        return 0;

}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK

	return 1;
#endif
}

static void lcm_setbacklight_cmdq(void* handle,unsigned int level)
{
#ifdef BUILD_LK
	dprintf(0,"%s,lk R63419 backlight: level = %d\n", __func__, level);
#else
	printk("%s, kernel R63419 backlight: level = %d\n", __func__, level);
#endif
	// Refresh value of backlight level.
	
	unsigned int cmd = 0x51;
	unsigned int count =1;
	unsigned int value = level;
	dsi_set_cmd_by_cmdq_dual(handle, cmd, count, &value, 1);
	//lcm_backlight_level_setting[0].para_list[0] = level;
	//push_table(lcm_backlight_level_setting, sizeof(lcm_backlight_level_setting) / sizeof(struct LCM_setting_table), 1);
}

LCM_DRIVER r63419_wqhd_truly_phantom_vdo_lcm_drv=
{
    .name           = "r63419_wqhd_truly_phantom_vdo",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params     = lcm_get_params,
    .init           = lcm_init,
    .suspend        = lcm_suspend,
    .resume         = lcm_resume,
    .compare_id     = lcm_compare_id,  
    .init_power		= lcm_init_power,
    .resume_power   = lcm_resume_power,
    .suspend_power  = lcm_suspend_power,
     .ata_check		= lcm_ata_check,
    .set_backlight_cmdq  = lcm_setbacklight_cmdq,
#if (LCM_DSI_CMD_MODE)
    .update         = lcm_update,
#endif

};
/* END PN:DTS2013053103858 , Added by d00238048, 2013.05.31*/
