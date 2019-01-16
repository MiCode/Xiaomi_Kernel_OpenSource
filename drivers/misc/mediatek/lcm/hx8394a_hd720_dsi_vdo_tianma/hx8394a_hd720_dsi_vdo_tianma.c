#ifndef BUILD_LK
#include <linux/string.h>
#endif
#include "lcm_drv.h"
#include <cust_gpio_usage.h>
#ifdef BUILD_LK
	#include <platform/mt_gpio.h>
	#include <string.h>
#elif defined(BUILD_UBOOT)
	#include <asm/arch/mt_gpio.h>
#else
	#include <mach/mt_gpio.h>
#endif
// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  (720)
#define FRAME_HEIGHT (1280)

#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif

#ifndef BUILD_LK
//static unsigned int lcm_esd_test = FALSE;      ///only for ESD test
#endif
// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------
#define dsi_set_cmdq_V3(para_tbl,size,force_update)        lcm_util.dsi_set_cmdq_V3(para_tbl,size,force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	        lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)											lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#define   LCM_DSI_CMD_MODE							0



static LCM_setting_table_V3 lcm_initialization_setting[] = {
	
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

{0x39, 0xB9,  3 ,{0xFF, 0x83, 0x94}},

{0x39, 0xBA,  16 ,{0x13, 0x82, 0x00, 0x16, 
				0xc5, 0x00, 0x10, 0xff, 
				0x0f, 0x24, 0x03, 0x21, 
				0x24, 0x25, 0x20, 0x08}},

{0x39, 0xC1,  127 ,{0x01, 0x00, 0x08, 0x10, 
				0x18, 0x1F, 0x28, 0x2f, 
				0x36, 0x3F, 0x47, 0x4F, 
				0x57, 0x5F, 0x67, 0x6F, 
				0x76, 0x7E, 0x86, 0x8E, 
				0x96, 0x9E, 0xA6, 0xAE, 
				0xB6, 0xBE, 0xC6, 0xCE, 
				0xD6, 0xDE, 0xE6, 0xEE, 
				0xF8, 0xFF, 0x00, 0x00, 
				0x00, 0x00, 0x00, 0x00, 
				0x00, 0x00, 0x00, 0x00, 
				0x08, 0x10, 0x18, 0x21, 
				0x29, 0x31, 0x37, 0x40, 
				0x48, 0x51, 0x58, 0x60, 
				0x67, 0x70, 0x76, 0x7E, 
				0x86, 0x8E, 0x96, 0x9E, 
				0xA6, 0xAE, 0xB6, 0xBE, 
				0xC6, 0xCE, 0xD6, 0xDE, 
				0xE6, 0xEE, 0xF8, 0xFF, 
				0x00, 0x00, 0x00, 0x00, 
				0x00, 0x00, 0x00, 0x00, 
				0x00, 0x00, 0x08, 0x10, 
				0x18, 0x1F, 0x28, 0x30, 
				0x37, 0x40, 0x48, 0x51, 
				0x59, 0x61, 0x69, 0x72, 
				0x79, 0x81, 0x89, 0x92, 
				0x9A, 0xA2, 0xAB, 0xB3, 
				0xBB, 0xC3, 0xCC, 0xD3, 
				0xDB, 0xE3, 0xEB, 0xF1, 
				0xF8, 0xFF, 0x00, 0x00, 
				0x00, 0x00, 0x00, 0x00, 
				0x00, 0x00, 0x00}},
{REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3,5,{}},


{0x39, 0xB1,  15 ,{0x01, 0x00, 0x37, 0x87, 
				0x01, 0x11, 0x11, 0x22, 
				0x2A, 0x3f, 0x3F, 0x57, 
				0x02, 0x00, 0xE6}},
{REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3,10,{}},

{0x15, 0xB2,  1 ,{0x00}},
  
{0x39, 0xBF,  4 ,{0x06, 0x00, 0x10, 0x04}},

{0x39, 0xB4,  22 ,{0x80, 0x08, 0x32, 0x10, 
				0x0d, 0x32, 0x10, 0x08, 
				0x22, 0x10, 0x08, 0x37, 
				0x04, 0x4a, 0x11, 0x37, 
				0x04, 0x44, 0x06, 0x5A, 
				0x5a, 0x06}},




{0x39, 0xD5,  52 ,{0x00, 0x00, 0x00, 0x10, 
				0x0A, 0x00, 0x01, 0x33, 
				0x00, 0x00, 0x33, 0x00, 
				0x23, 0x01, 0x67, 0x45, 
				0x01, 0x23, 0x88, 0x88, 
				0x88, 0x88, 0x88, 0x88, 
				0x88, 0x88, 0x88, 0x88, 
				0x88, 0x45, 0x88, 0x99, 
				0x54, 0x76, 0x10, 0x32, 
				0x32, 0x10, 0x88, 0x88, 
				0x88, 0x88, 0x88, 0x88, 
				0x88, 0x88, 0x88, 0x88, 
				0x88, 0x54, 0x99, 0x88}},


{0x39, 0xC7,  4 ,{0x00, 0x10, 0x00, 0x10}},


{0x39, 0xE0,  42 ,{0x00, 0x0a, 0x13, 0x33, 
				0x38, 0x3f, 0x23, 0x44, 
				0x06, 0x0c, 0x0f, 0x13, 
				0x16, 0x14, 0x15, 0x10, 
				0x17, 0x00, 0x0a, 0x13, 
				0x33, 0x38, 0x3f, 0x23, 
				0x44, 0x06, 0x0c, 0x0f, 
				0x13, 0x16, 0x14, 0x15, 
				0x10, 0x17, 0x09, 0x17, 
				0x08, 0x18, 0x09, 0x17, 
				0x08, 0x18}},


{0x39, 0xC0,  2 ,{0x0c, 0x17}},

{0x15, 0xCC,  1 ,{0x09}},

{0x15, 0xB6,  1 ,{0x22}},

{0x05, 0x11,0,{}},//
{REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3,120,{}},

{0x05, 0x29,0,{}},//
{REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3,20,{}},	  

	/* FIXME */
	/*
		params->dsi.horizontal_sync_active				= 0x16;// 50  2
		params->dsi.horizontal_backporch				= 0x38;
		params->dsi.horizontal_frontporch				= 0x18;
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;
		params->dsi.horizontal_blanking_pixel =0;    //lenovo:fix flicker issue
	    //params->dsi.LPX=8; 
	*/

};

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
		params->dsi.mode   = BURST_VDO_MODE; //SYNC_PULSE_VDO_MODE;//BURST_VDO_MODE; 
        #endif
	
		// DSI
		/* Command mode setting */
		//1 Three lane or Four lane
		params->dsi.LANE_NUM				= LCM_FOUR_LANE;
		//The following defined the fomat for data coming from LCD engine.
		params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

		// Video mode setting		
		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
		
		params->dsi.vertical_sync_active				= 0x05;// 3    2
		params->dsi.vertical_backporch					= 0x0d;// 20   1
		params->dsi.vertical_frontporch					= 0x08; // 1  12
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 

		params->dsi.horizontal_sync_active				= 0x12;// 50  2
		params->dsi.horizontal_backporch				= 0x5f;
		params->dsi.horizontal_frontporch				= 0x5f;
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

	    //params->dsi.LPX=8; 

		// Bit rate calculation
		params->dsi.PLL_CLOCK = 240;
		//1 Every lane speed
		params->dsi.pll_div1=0;		// div1=0,1,2,3;div1_real=1,2,4,4 ----0: 546Mbps  1:273Mbps
		params->dsi.pll_div2=0;		// div2=0,1,2,3;div1_real=1,2,4,4	
		params->dsi.fbk_div =9;    // fref=26MHz, fvco=fref*(fbk_div+1)*2/(div1_real*div2_real)	

}

static void lcm_init(void)
{
		SET_RESET_PIN(0);
		MDELAY(20); 
		SET_RESET_PIN(1);
		MDELAY(20); 
		dsi_set_cmdq_V3(lcm_initialization_setting,sizeof(lcm_initialization_setting)/sizeof(lcm_initialization_setting[0]),1);

}



static void lcm_suspend(void)
{
	unsigned int data_array[16];

	data_array[0]=0x00280500; // Display Off
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x00100500; // Sleep In
	dsi_set_cmdq(data_array, 1, 1);

	
	SET_RESET_PIN(1);	
	SET_RESET_PIN(0);
	MDELAY(1); // 1ms
	
	SET_RESET_PIN(1);
	MDELAY(120);     
	lcm_util.set_gpio_out(GPIO_LCD_ENN, GPIO_OUT_ZERO);
	lcm_util.set_gpio_out(GPIO_LCD_ENP, GPIO_OUT_ZERO); 
}


static void lcm_resume(void)
{
	lcm_util.set_gpio_out(GPIO_LCD_ENN, GPIO_OUT_ONE);
	lcm_util.set_gpio_out(GPIO_LCD_ENP, GPIO_OUT_ONE);
	lcm_init();

    #ifdef BUILD_LK
	  printf("[LK]---cmd---hx8394a_hd720_dsi_vdo_tianma----%s------\n",__func__);
    #else
	  printk("[KERNEL]---cmd---hx8394a_hd720_dsi_vdo_tianma----%s------\n",__func__);
    #endif	
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

static unsigned int lcm_compare_id(void)
{

  	unsigned int ret = 0;

	ret = mt_get_gpio_in(GPIO92);
#if defined(BUILD_LK)
	printf("%s, [jx]hx8394a GPIO92 = %d \n", __func__, ret);
#endif	

	return (ret == 0)?1:0; 


}

#if 0
static unsigned int lcm_esd_check(void)
{
  #ifndef BUILD_LK
	char  buffer[3];
	int   array[4];

	if(lcm_esd_test)
	{
		lcm_esd_test = FALSE;
		return TRUE;
	}

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x36, buffer, 1);
	if(buffer[0]==0x90)
	{
		return FALSE;
	}
	else
	{			 
		return TRUE;
	}
#else
	return FALSE;
#endif

}

static unsigned int lcm_esd_recover(void)
{
	lcm_init();
	lcm_resume();

	return TRUE;
}
#endif


LCM_DRIVER hx8394a_hd720_dsi_vdo_tianma_lcm_drv = 
{
    .name			= "hx8394a_hd720_dsi_vdo_tianma",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id     = lcm_compare_id,
	//.esd_check = lcm_esd_check,
	//.esd_recover = lcm_esd_recover,
#if (LCM_DSI_CMD_MODE)
    .update         = lcm_update,
#endif
    };
