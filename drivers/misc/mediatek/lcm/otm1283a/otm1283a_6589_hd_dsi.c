#ifndef BUILD_LK
#include <linux/string.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
	#include <platform/mt_gpio.h>
#elif defined(BUILD_UBOOT)
	#include <asm/arch/mt_gpio.h>
#else
	#include <mach/mt_gpio.h>
#endif


// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  										(720)
#define FRAME_HEIGHT 										(1280)

#define REGFLAG_DELAY             							0XFEFF
#define REGFLAG_END_OF_TABLE      							0xFFFF   // END OF REGISTERS MARKER

//#define LCM_DSI_CMD_MODE									0

#define LCM_ID_OTM1283 0x40

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))
#define SET_GPIO_OUT(gpio_num,val)    						(lcm_util.set_gpio_out((gpio_num),(val)))


#define UDELAY(n) 											(lcm_util.udelay(n))
#define MDELAY(n) 											(lcm_util.mdelay(n))



//#define _SYNA_INFO_
//#define _SYNA_DEBUG_

#ifdef _LCM_DEBUG_
#define lcm_debug(fmt, args...) printk(fmt, ##args)
#else
#define lcm_debug(fmt, args...) do { } while (0)
#endif

#ifdef _LCM_INFO_
#define lcm_info(fmt, args...) printk(fmt, ##args)
#else
#define lcm_info(fmt, args...) do { } while (0)
#endif
#define lcm_err(fmt, args...) printk(fmt, ##args)

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

	{0x00, 1, {0x00}},
	//{REGFLAG_DELAY, 10, {}},
	{0xff, 3, {0x12,0x83,0x01}},
	//{REGFLAG_DELAY, 10, {}},
	
	{0x00, 1, {0x80}},
	//{REGFLAG_DELAY, 10, {}},
	{0xff, 2, {0x12,0x83}},
	//{REGFLAG_DELAY, 10, {}},

//-------------------- panel setting --------------------//
	{0x00, 1, {0x80}},
	//{REGFLAG_DELAY, 10, {}},
	{0xc0, 9, {0x00,0x64,0x00,0x10,0x10,0x00,0x64,0x10,0x10}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00, 1, {0x90}},
	//{REGFLAG_DELAY, 10, {}},
	{0xc0, 6, {0x00,0x5c,0x00,0x01,0x00,0x04}},
	//{REGFLAG_DELAY, 10, {}},
	
	{0x00, 1, {0xa4}},
	//{REGFLAG_DELAY, 10, {}},
	{0xc0, 1, {0x22}},
	//{REGFLAG_DELAY, 10, {}},
	
	{0x00, 1, {0xb3}},
	//{REGFLAG_DELAY, 10, {}},
	{0xc0,2,{0x00,0x50}},
	//{REGFLAG_DELAY, 10, {}},	
	
	{0x00, 1, {0x81}},
	//{REGFLAG_DELAY, 10, {}},
	{0xc1,1,{0x55}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00, 1, {0x90}},
	//{REGFLAG_DELAY, 10, {}},
	{0xc4, 1, {0x49}},
	//{REGFLAG_DELAY, 10, {}},
	
	{0x00, 1, {0xb9}},
	//{REGFLAG_DELAY, 10, {}},
	{0xb0, 1, {0x51}},
	//{REGFLAG_DELAY, 10, {}},
		
	{0x00,1,{0xa0}},             //dcdc setting
	//{REGFLAG_DELAY, 10, {}},
	{0xc4, 14, {0x05,0x10,0x06,0x02,0x05,0x15,0x10,0x05,0x10,0x07,0x02,0x05,0x15,0x10}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00, 1, {0xb0}},
	//{REGFLAG_DELAY, 10, {}},
	{0xc4, 2, {0x00,0x00}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0x91}},             //VGH=15V, VGL=-10V, pump ratio:VGH=6x, VGL=-5x
	//{REGFLAG_DELAY, 10, {}},
	{0xc5,2,{0x46,0x40}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0x00}},             //GVDD=4.87V, NGVDD=-4.87V
	//{REGFLAG_DELAY, 10, {}},
	{0xd8,2,{0xbc,0xbc}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0x00}},             //VCOMDC=-0.9
	//{REGFLAG_DELAY, 10, {}},
	{0xd9,1,{0x54}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0x81}},             //source bias 0.75uA
	{0xc4,1,{0x82}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0xb0}},             //VDD_18V=1.6V, LVDSVDD=1.55V        --huxh------------------
	{0xc5,2,{0x04,0x38}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0xbb}},             //LVD voltage level setting
	{0xc5,1,{0x80}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0x82}},		// chopper 0: frame 2: line 4: disable
	{0xC4,1,{0x02}}, 
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0xc6}},		// debounce
	{0xB0,1,{0x03}}, 
	//{REGFLAG_DELAY, 10, {}},

//-------------------- control setting --------------------//
	{0x00,1,{0x00}},             //ID1
	{0xd0,1,{0x40}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0x00}},             //ID2, ID3
	{0xd1,2,{0x00,0x00}},
	//{REGFLAG_DELAY, 10, {}},

//-------------------- panel timing state control --------------------//
	{0x00,1,{0x80}},             //panel timing state control
	//{REGFLAG_DELAY, 10, {}},
	{0xcb,11,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0x90}},             //panel timing state control
	//{REGFLAG_DELAY, 10, {}},
	{0xcb,15,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0xa0}},             //panel timing state control
	//{REGFLAG_DELAY, 10, {}},
	{0xcb,15,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0xb0}},             //panel timing state control
	///{REGFLAG_DELAY, 10, {}},
	{0xcb,15,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0xc0}},             //panel timing state control
	//{REGFLAG_DELAY, 10, {}},
	{0xcb,15,{0x05,0x05,0x05,0x05,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0xd0}},             //panel timing state control
	//{REGFLAG_DELAY, 10, {}},
	{0xcb,15,{0xff,0xff,0xff,0x00,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x00,0x00,0x00}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0xe0}},             //panel timing state control
	//{REGFLAG_DELAY, 10, {}},
	{0xcb,14,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0x00,0x05,0x05,0x05}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0xf0}},             //panel timing state control
	//{REGFLAG_DELAY, 10, {}},
	{0xcb,11,{0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}},
	//{REGFLAG_DELAY, 10, {}},

//-------------------- panel pad mapping control --------------------//
	{0x00,1,{0x80}},             //panel pad mapping control
	//{REGFLAG_DELAY, 10, {}},
	{0xcc,15,{0x02,0x0a,0x0c,0x0e,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0x90}},             //panel pad mapping control
	//{REGFLAG_DELAY, 10, {}},
	{0xcc,15,{0x00,0x00,0x00,0x00,0x2e,0x2d,0x06,0x01,0x09,0x0b,0x0d,0x0f,0x00,0x00,0x00}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0xa0}},             //panel pad mapping control
	//{REGFLAG_DELAY, 10, {}},
	{0xcc,14,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2e,0x2d,0x05}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0xb0}},             //panel pad mapping control
	//{REGFLAG_DELAY, 10, {}},
	{0xcc,15,{0x05,0x0f,0x0d,0x0b,0x09,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0xc0}},             //panel pad mapping control
	//{REGFLAG_DELAY, 10, {}},
	{0xcc,15,{0x00,0x00,0x00,0x00,0x2d,0x2e,0x01,0x06,0x10,0x0e,0x0c,0x0a,0x00,0x00,0x00}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0xd0}},             //panel pad mapping control
	//{REGFLAG_DELAY, 10, {}},
	{0xcc,14,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2d,0x2e,0x02}},
	//{REGFLAG_DELAY, 10, {}},

//-------------------- panel timing setting --------------------//
	{0x00,1,{0x80}},             //panel VST setting
	//{REGFLAG_DELAY, 10, {}},
	{0xce,12,{0x87,0x03,0x10,0x86,0x03,0x10,0x00,0x00,0x00,0x00,0x00,0x00}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0x90}},             //panel VEND setting
	//{REGFLAG_DELAY, 10, {}},
	{0xce,14,{0x35,0x01,0x10,0x35,0x02,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0xa0}},             //panel CLKA1/2 setting
	//{REGFLAG_DELAY, 10, {}},
	{0xce,14,{0x38,0x03,0x04,0xf8,0x00,0x10,0x00,0x38,0x02,0x04,0xf9,0x00,0x10,0x00}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0xb0}},             //panel CLKA3/4 setting
	//{REGFLAG_DELAY, 10, {}},
	{0xce,14,{0x38,0x01,0x04,0xfa,0x00,0x10,0x00,0x38,0x00,0x04,0xfb,0x00,0x10,0x00}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0xc0}},             //panel CLKb1/2 setting
	//{REGFLAG_DELAY, 10, {}},
	{0xce,14,{0x30,0x00,0x04,0xfc,0x00,0x10,0x00,0x30,0x01,0x04,0xfd,0x00,0x10,0x00}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0xd0}},             //panel CLKb3/4 setting
	//{REGFLAG_DELAY, 10, {}},
	{0xce,14,{0x30,0x02,0x04,0xfe,0x00,0x10,0x00,0x30,0x03,0x04,0xff,0x00,0x10,0x00}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0x80}},             //panel CLKc1/2 setting
	//{REGFLAG_DELAY, 10, {}},
	{0xcf,14,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0x90}},             //panel CLKc3/4 setting
	//{REGFLAG_DELAY, 10, {}},
	{0xcf,14,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0xa0}},             //panel CLKd1/2 setting
	//{REGFLAG_DELAY, 10, {}},
	{0xcf,14,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0xb0}},             //panel CLKd3/4 setting
	//{REGFLAG_DELAY, 10, {}},
	{0xcf,14,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0xc0}},             //panel ECLK setting
	//{REGFLAG_DELAY, 10, {}},
	{0xcf,11,{0x01,0x01,0x20,0x20,0x00,0x00,0x01,0x81,0x00,0x03,0x08}}, //gate pre. ena.
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0xb5}},             //TCON_GOA_OUT Setting
	//{REGFLAG_DELAY, 10, {}},
	{0xc5,6,{0x3f,0xff,0xff,0x3f,0xff,0xff}},
	//{REGFLAG_DELAY, 10, {}},
//-------------------- for ,{ower IC --------------------//
	{0x00,1,{0x90}},             //Mode-3
	//{REGFLAG_DELAY, 10, {}},
	{0xf5,4,{0x02,0x11,0x02,0x11}},
	//{REGFLAG_DELAY, 10, {}},	
	
	{0x00, 1, {0x90}},
	//{REGFLAG_DELAY, 10, {}},
	{0xc5, 1, {0x50}},
	//{REGFLAG_DELAY, 10, {}},		

	{0x00, 1, {0x94}},
	//{REGFLAG_DELAY, 10, {}},
	{0xc5, 1, {0x66}},
	//{REGFLAG_DELAY, 10, {}},	

//------------------VGLO1/O2 disable----------------
	{0x00, 1, {0xb2}},
	//{REGFLAG_DELAY, 10, {}},
	{0xf5,2,{0x00,0x00}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0xb4}},             //VGLO1_S
	//{REGFLAG_DELAY, 10, {}},
	{0xf5,2,{0x00,0x00}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0xb6}},             //VGLO2
	//{REGFLAG_DELAY, 10, {}},
	{0xf5,2,{0x00,0x00}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0xb8}},             //VGLO2_S
	///{REGFLAG_DELAY, 10, {}},
	{0xf5,2,{0x00,0x00}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0x94}},  		//VCL on  	
	//{REGFLAG_DELAY, 10, {}},
	{0xF5,1,{0x02}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0xBA}},  		//VS,{ on   	
	//{REGFLAG_DELAY, 10, {}},
	{0xF5,1,{0x03}},
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0xb4}},             //VGLO1/2 ,{ull low setting
	//{REGFLAG_DELAY, 10, {}},
	{0xc5,1,{0xc0}},		//d[7] vglo1 d[6] vglo2 => 0: pull vss, 1: pull vgl
	//{REGFLAG_DELAY, 10, {}},

	{0x00,1,{0xb2}},             //C31 cap. not remove
	//{REGFLAG_DELAY, 10, {}},
	{0xc5,1,{0x40}},
	//{REGFLAG_DELAY, 10, {}},


        {0x00,1,{0xA0}}, 
        //{REGFLAG_DELAY, 10, {}},
        {0xC1,1,{0x02}},  // Disable time-out function       -----------huxh---------------------------------------------
       // {REGFLAG_DELAY, 10, {}},
        
//-------------------- Gamma --------------------//
	{0x00,1,{0x00}},
	//{REGFLAG_DELAY, 10, {}},
	{0xE1,16,{0x00,0x15,0x1B,0x0D,0x06,0x0E,0x09,0x09,0x04,0x07,0x0C,0x08,0x0F,0x0F,0x09,0x00}},
	//{REGFLAG_DELAY, 10, {}},
							
	{0x00,1,{0x00}},
	//{REGFLAG_DELAY, 10, {}},
	{0xE2,16,{0x00,0x15,0x1B,0x0D,0x06,0x0E,0x09,0x09,0x04,0x07,0x0C,0x08,0x0F,0x0F,0x09,0x00}},
	//{REGFLAG_DELAY, 10, {}},


	{0x00,1,{0x00}},             //Orise mode disable
	//{REGFLAG_DELAY, 10, {}},
	{0xff,3,{0xff,0xff,0xff}},
	//{REGFLAG_DELAY, 10, {}},
};

static struct LCM_setting_table lcm_set_window[] = {
	{0x2A,	4,	{0x00, 0x00, (FRAME_WIDTH>>8), (FRAME_WIDTH&0xFF)}},
	{0x2B,	4,	{0x00, 0x00, (FRAME_HEIGHT>>8), (FRAME_HEIGHT&0xFF)}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
	{0x11, 0, {0x00}},
    {REGFLAG_DELAY, 120, {}},

    // Display ON
	{0x29, 0, {0x00}},
//	{REGFLAG_DELAY, 20, {}},
	
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_sleep_in_setting[] = {
	// Display off sequence
	{0x28, 0, {0x00}},

    // Sleep Mode On
	{0x10, 0, {0x00}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
static void lcm_init_registers()
{
	unsigned int data_array[16];
	
	lcm_debug("liyibo : %s %d\n", __func__,__LINE__);
	
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
}
static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;
	lcm_debug("liyibo : %s %d\n", __func__,__LINE__);
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
		lcm_debug("liyibo : %s %d\n", __func__,__LINE__);
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}


static void lcm_get_params(LCM_PARAMS *params)
{
	  	lcm_debug("liyibo : %s %d\n", __func__,__LINE__);
		memset(params, 0, sizeof(LCM_PARAMS));
	
		params->type   = LCM_TYPE_DSI;

		params->width  = FRAME_WIDTH;
		params->height = FRAME_HEIGHT;

		// enable tearing-free
		params->dbi.te_mode 				= LCM_DBI_TE_MODE_VSYNC_ONLY;
		params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;

#if (LCM_DSI_CMD_MODE)
		params->dsi.mode   = CMD_MODE;
#else
		params->dsi.mode   = BURST_VDO_MODE;
#endif
	
		// DSI
		/* Command mode setting */
		params->dsi.LANE_NUM				= LCM_FOUR_LANE;
		//The following defined the fomat for data coming from LCD engine.
		params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
		params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
		params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
		params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

		// Highly depends on LCD driver capability.
		// Not support in MT6573
		params->dsi.packet_size=256;

		// Video mode setting		
		params->dsi.intermediat_buffer_num = 0;

		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

		params->dsi.vertical_sync_active				= 4;//2;
		params->dsi.vertical_backporch					= 16;//14;
		params->dsi.vertical_frontporch					= 16;
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 

		params->dsi.horizontal_sync_active				= 6;//2;
		params->dsi.horizontal_backporch				= 44;//44;//42;
		params->dsi.horizontal_frontporch				= 44;
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

    params->dsi.PLL_CLOCK = 215;//156;
		// Bit rate calculation
		params->dsi.pll_div1=0;		// fref=26MHz, fvco=fref*(div1+1)	(div1=0~63, fvco=500MHZ~1GHz)
		params->dsi.pll_div2=1; 		// div2=0~15: fout=fvo/(2*div2)
		params->dsi.fbk_div =11;    //fref=26MHz, fvco=fref*(fbk_div+1)*2/(div1_real*div2_real)

}

static unsigned int lcm_compare_id(void)
{
		unsigned int id = 0;
		unsigned char buffer[2];
		unsigned int array[16];
			lcm_debug("liyibo : %s %d\n", __func__,__LINE__);
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
		lcm_debug("liyibo : %s %d\n", __func__,__LINE__);
	SET_RESET_PIN(1);
    SET_RESET_PIN(0);
	MDELAY(1);//50
    SET_RESET_PIN(1);
	MDELAY(10);//100

	//lcm_init_registers();
	//data_array[0] = 0x00352500;
	//dsi_set_cmdq(&data_array, 1, 1);
	push_table(lcm_initialization_setting,sizeof(lcm_initialization_setting)/sizeof(lcm_initialization_setting[0]),1);
	push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
}


static void lcm_suspend(void)
{
		lcm_debug("liyibo : %s %d\n", __func__,__LINE__);
	SET_RESET_PIN(0);	
	MDELAY(1);	
	SET_RESET_PIN(1);

	push_table(lcm_sleep_in_setting, sizeof(lcm_sleep_in_setting) / sizeof(struct LCM_setting_table), 1);
    
	SET_GPIO_OUT(GPIO_LCM_PWR_EN,0);//Disable LCM Power
}



static void lcm_resume(void)
{
	lcm_debug("liyibo : %s %d\n", __func__,__LINE__);
	SET_GPIO_OUT(GPIO_LCM_PWR_EN,1);  //Enable LCM Power
	lcm_init();
//	push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
}


static void lcm_update(unsigned int x, unsigned int y,
                       unsigned int width, unsigned int height)
{
		lcm_debug("liyibo : %s %d\n", __func__,__LINE__);
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
	data_array[3]= 0x00000000;
	data_array[4]= 0x00053902;
	data_array[5]= (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
	data_array[6]= (y1_LSB);
	data_array[7]= 0x00000000;
	data_array[8]= 0x002c3909;

	dsi_set_cmdq(&data_array, 9, 0);

}







LCM_DRIVER otm1283a_6589_hd_dsi = 
{
    .name			= "otm1283a_6589_dsi",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id     = lcm_compare_id,
#if (LCM_DSI_CMD_MODE)
	.set_backlight	= lcm_setbacklight,
    .update         = lcm_update,
#endif
};
