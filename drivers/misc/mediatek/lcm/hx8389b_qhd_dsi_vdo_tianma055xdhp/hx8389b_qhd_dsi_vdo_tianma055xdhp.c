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

#define FRAME_WIDTH  (540)
#define FRAME_HEIGHT (960)

#define LCM_ID_HX8389B 0

#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif

static unsigned int lcm_esd_test = FALSE;      ///only for ESD test

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))

#define REGFLAG_DELAY             							0XFE
#define REGFLAG_END_OF_TABLE      							0x00   // END OF REGISTERS MARKER

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


static  LCM_setting_table_V3 lcm_sleep_out_setting[] = {

	// Sleep Out
	{0x05,0x11, 0, {0}},
    {REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3, 120, {0}},

    // Display ON
	{0x05,0x29, 0, {0}}
	//{REGFLAG_END_OF_TABLE, 0x00, {}}

};

static  LCM_setting_table_V3 lcm_sleep_in_setting[] = {

	// Display off sequence
	{0x05,0x28, 0, {0}},
	{REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3, 120, {0}},

    // Sleep Mode On
	{0x05,0x10, 0, {0}},

	{REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3, 50, {0}},
	{0x15,0x4F, 1, {0x01}}
	//{REGFLAG_END_OF_TABLE, 0x00, {}}

};




static  LCM_setting_table_V3 lcm_initialization_setting[] = {
	
	/*
	Note :

	Data ID will depends on the following rule.
	
		count of parameters > 1	=> Data ID = 0x39
		count of parameters = 1	=> Data ID = 0x15
		count of parameters = 0	=> Data ID = 0x05

	Structure Format :

	*/
	{0x39,0xB9, 3 ,{0xFF,0x83,0x89}},
		   {REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3,  1, {}},
	
	   {0x39,0xBA, 7 ,{0x41,0x93,0x00,0x16,0xA4,0x10,0x18}},
		   {REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3,  1, {}},
	
	   {0x15,0xC6, 1 ,{0x08}},
	
	
	   {0x39,0xB1,19 ,{0x00,0x00,0x07,0xFE,0x96,0x10,0x11,0x94,0xf1,0x26,
				  0x2e,0x2D,0x2D,0x42,0x01,0x38,0xF7,0x20,0x80}},
		   {REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3,  10, {}},
	
	   {0x39,0xDE, 3 ,{0x05,0x58,0x10}},
	
	   {0x39,0xB2, 7 ,{0x00,0x00,0x78,0x0E,0x03,0x3F,0x80}},
		   {REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3,  1, {}},
	
	   {0x39,0xB4, 23 ,{0x80,0x28,0x00,0x32,0x10,0x07,0x32,0x10,0x07,0x32,
				 0x10,0x07,0x27,0x01,0x5A,0x0B,0x27,0x01,0x4C,0x14,
				 0x58,0x5B,0x0a}},
		   {REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3,  10, {}},
	
	   {0x39,0xD5, 48 ,{0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x60,0x00,
				 0x99,0x88,0x88,0x88,0x88,0x23,0x88,0x01,0x88,0x67,
				 0x88,0x45,0x01,0x23,0x23,0x88,0x88,0x88,0x88,0x88,
				 0x99,0x88,0x88,0x88,0x54,0x88,0x76,0x88,0x10,0x88,
				 0x32,0x32,0x10,0x88,0x88,0x88,0x88,0x88}},
		   {REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3,  10, {}},
	 
	
	   {0x39,0xE0, 34 ,{0x01,0x1B,0x21,0x34,0x32,0x3F,0x2E,0x4B,0x07,0x0D,
				 0x10,0x13,0x14,0x11,0x11,0x0F,0x19,0x01,0x1B,0x21,
				 0x34,0x32,0x3F,0x2E,0x4B,0x07,0x0D,0x10,0x13,0x14,
				 0x11,0x11,0x0F,0x19}},
			{REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3,  5, {}},
	
	   {0x39,0xB6, 4 ,{0x00,0x90,0x00,0x90}},
			{REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3,  1, {}},
		  
	   {0x15,0xCC, 1 ,{0x02}},
		  
	   {0x39,0xB7, 3 ,{0x00,0x00,0x50}},
	
	   {0x15,0x51,1,{0xFF}},
	   {0x15,0x53,1,{0x2C}},
	   {0x15,0x55,1,{0x02}},
	   
	   {0x05,0x11, 0 ,{}},
			{REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3,  120, {}},
	   {0x05,0x29, 0 ,{}},
			{REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3,  10, {}}

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
		params->dsi.mode   = SYNC_PULSE_VDO_MODE; //SYNC_PULSE_VDO_MODE;//BURST_VDO_MODE; 
        #endif
	
		// DSI
		/* Command mode setting */
		//1 Three lane or Four lane
		params->dsi.LANE_NUM				= LCM_TWO_LANE;
		//The following defined the fomat for data coming from LCD engine.
		params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

		// Video mode setting		
		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
		
		params->dsi.vertical_sync_active				= 0x05;// 3    2
		params->dsi.vertical_backporch					= 0x15;// 20   1
		params->dsi.vertical_frontporch					= 0x09; // 1  12
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 

		params->dsi.horizontal_sync_active				= 0x16;// 50  2
		params->dsi.horizontal_backporch				= 0x38;
		params->dsi.horizontal_frontporch				= 0x18;
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;
		params->dsi.horizontal_blanking_pixel =0;
	    //params->dsi.LPX=8; 

		// Bit rate calculation
		//1 Every lane speed
		//params->dsi.pll_select=1;
		//params->dsi.PLL_CLOCK  = LCM_DSI_6589_PLL_CLOCK_377;
		params->dsi.PLL_CLOCK=250;
		params->dsi.pll_div1=0;		// div1=0,1,2,3;div1_real=1,2,4,4 ----0: 546Mbps  1:273Mbps
		params->dsi.pll_div2=0;		// div2=0,1,2,3;div1_real=1,2,4,4	
#if (LCM_DSI_CMD_MODE)
		params->dsi.fbk_div =9;
#else
		params->dsi.fbk_div =9;    // fref=26MHz, fvco=fref*(fbk_div+1)*2/(div1_real*div2_real)	
#endif
		//params->dsi.compatibility_for_nvk = 1;		// this parameter would be set to 1 if DriverIC is NTK's and when force match DSI clock for NTK's
}

static void lcm_init(void)
{
	    unsigned int data_array[16];
		
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
}


static void lcm_resume(void)
{
	lcm_init();

    #ifdef BUILD_LK
	  printf("[LK]---cmd---hx8389b----%s------\n",__func__);
    #else
	  printk("[KERNEL]---cmd---hx8389b----%s------\n",__func__);
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
		printf("%s, LK hx8389b debug: hx8389b id = 0x%08x\n", __func__, id);
    #else
		printk("%s, kernel hx8389b horse debug: hx8389b id = 0x%08x\n", __func__, id);
    #endif

    if(id == LCM_ID_HX8389B)
    	return 1;
    else
        return 0;


}


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
 #endif

}

static unsigned int lcm_esd_recover(void)
{
	lcm_init();
	lcm_resume();

	return TRUE;
}



LCM_DRIVER hx8389b_qhd_dsi_vdo_tianma055xdhp_lcm_drv = 
{
    .name			= "hx8389b_qhd_dsi_vdo_tianma055xdhp",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	//.compare_id     = lcm_compare_id,
	//.esd_check = lcm_esd_check,
	//.esd_recover = lcm_esd_recover,
#if (LCM_DSI_CMD_MODE)
    .update         = lcm_update,
#endif
    };
