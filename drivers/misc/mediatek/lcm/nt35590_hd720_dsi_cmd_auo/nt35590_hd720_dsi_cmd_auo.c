#ifndef BUILD_LK
#include <linux/string.h>
#endif

#include "lcm_drv.h"

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

#define FRAME_WIDTH  										(720)
#define FRAME_HEIGHT 										(1280)
#define LCM_ID       (0x69)
#define REGFLAG_DELAY             							0xAB
#define REGFLAG_END_OF_TABLE      							0xAA   // END OF REGISTERS MARKER

#define LCM_ID_NT35590 (0x90)


#if 0
#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif

static unsigned int lcm_esd_test = FALSE;      ///only for ESD test
#endif

#ifndef CONFIG_FPGA_EARLY_PORTING
#define LCM_DSI_CMD_MODE									1
#else
#define LCM_DSI_CMD_MODE									0
#endif

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))

#define UDELAY(n) 											(lcm_util.udelay(n))
#define MDELAY(n) 											(lcm_util.mdelay(n))


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	        lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)											lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)    

static unsigned int need_set_lcm_addr = 1;
struct LCM_setting_table {
    unsigned char cmd;
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

	{0xC2,	1,	{0x08}},
	{0xFF,	1,	{0x00}},
	{0xBA,	1,	{0x02}},		// 3lane
		
	{0x11, 1, {0x00}},
    {REGFLAG_DELAY, 120, {}},

    // Display ON
	{0x29, 1, {0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {}},
	// Note
	// Strongly recommend not to set Sleep out / Display On here. That will cause messed frame to be shown as later the backlight is on.


	// Setting ending by predefined flag
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif


#if 0
static struct LCM_setting_table lcm_set_window[] = {
	{0x2A,	4,	{0x00, 0x00, (FRAME_WIDTH>>8), (FRAME_WIDTH&0xFF)}},
	{0x2B,	4,	{0x00, 0x00, (FRAME_HEIGHT>>8), (FRAME_HEIGHT&0xFF)}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif
#if 0
static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
	{0x11, 1, {0x00}},
    {REGFLAG_DELAY, 120, {}},

    // Display ON
	{0x29, 1, {0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	// Display off sequence
	{0x28, 1, {0x00}},
	
	{REGFLAG_DELAY, 50, {}},
	
	 // Sleep Mode On
	 {0x10, 1, {0x00}},
	
	 {REGFLAG_DELAY, 100, {}},
	
	 {0x4F, 1, {0x01}},
	
	 {REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif
/*
static struct LCM_setting_table lcm_compare_id_setting[] = {
	// Display off sequence
	{0xB9,	3,	{0xFF, 0x83, 0x69}},
	{REGFLAG_DELAY, 10, {}},

    // Sleep Mode On
//	{0xC3, 1, {0xFF}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
*/
#if 0
static struct LCM_setting_table lcm_backlight_level_setting[] = {
	{0x51, 1, {0xFF}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

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

				if (cmd != 0xFF && cmd != 0x2C && cmd != 0x3C) {
					//#if defined(BUILD_UBOOT)
					//	printf("[DISP] - uboot - REG_R(0x%x) = 0x%x. \n", cmd, table[i].para_list[0]);
					//#endif
					while(read_reg(cmd) != table[i].para_list[0]);		
				}
       	}
		
    }
	
}
#endif

// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy((void*)&lcm_util, (void*)util, sizeof(LCM_UTIL_FUNCS));
}


static void lcm_get_params(LCM_PARAMS *params)
{
		memset((void*)params, 0, sizeof(LCM_PARAMS));
	
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
		params->dsi.LANE_NUM				= LCM_THREE_LANE;
		//The following defined the fomat for data coming from LCD engine.
		params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;
#ifndef CONFIG_FPGA_EARLY_PORTING
		params->dsi.vertical_sync_active				= 1;// 3    2
		params->dsi.vertical_backporch					= 1;// 20   1
		params->dsi.vertical_frontporch					= 2; // 1  12
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 

		params->dsi.horizontal_sync_active				= 2;// 50  2
		params->dsi.horizontal_backporch				= 12;
		params->dsi.horizontal_frontporch				= 80;
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
		params->dsi.CLK_HS_POST=26;
		params->dsi.compatibility_for_nvk = 0;

    	params->dsi.PLL_CLOCK = 330;//dsi clock customization: should config clock value directly
    	params->dsi.pll_div1=0;
    	params->dsi.pll_div2=0;
    	params->dsi.fbk_div=11;
#else
		params->dsi.vertical_sync_active				= 1;// 3    2
		params->dsi.vertical_backporch					= 1;// 20   1
		params->dsi.vertical_frontporch					= 2; // 1  12
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 

		params->dsi.horizontal_sync_active				= 10;// 50  2
		params->dsi.horizontal_backporch				= 42;
		params->dsi.horizontal_frontporch				= 52;
		params->dsi.horizontal_bllp				= 85;

		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
		params->dsi.compatibility_for_nvk = 0;

    	params->dsi.PLL_CLOCK = 26;//dsi clock customization: should config clock value directly
#endif
}

static void lcm_init(void)
{
	//int i;
	//unsigned char buffer[10];
	//unsigned int  array[16];
	unsigned int data_array[16];

		MDELAY(40); 
		SET_RESET_PIN(1);
		MDELAY(5); 
	
		data_array[0] = 0x00023902;
		data_array[1] = 0x0000EEFF; 				
		dsi_set_cmdq(data_array, 2, 1);
		MDELAY(2); 
		data_array[0] = 0x00023902;
		data_array[1] = 0x00000826; 				
		dsi_set_cmdq(data_array, 2, 1);
		MDELAY(2); 
		data_array[0] = 0x00023902;
		data_array[1] = 0x00000026; 				
		dsi_set_cmdq(data_array, 2, 1);
		MDELAY(2); 
		data_array[0] = 0x00023902;
		data_array[1] = 0x000000FF; 				
		dsi_set_cmdq(data_array, 2, 1);
		
		MDELAY(20); 
		SET_RESET_PIN(0);
		MDELAY(1); 
		SET_RESET_PIN(1);
		MDELAY(40); 
	
	
	data_array[0]=0x00023902;
#if (LCM_DSI_CMD_MODE)
	data_array[1]=0x000008C2;//cmd mode
#else
	data_array[1]=0x000003C2;//video mode
#endif
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
	data_array[1]=0x000002BA;//MIPI lane
	dsi_set_cmdq(data_array, 2, 1);

	//{0x44,	2,	{((FRAME_HEIGHT/2)>>8), ((FRAME_HEIGHT/2)&0xFF)}},
	data_array[0] = 0x00033902;
	data_array[1] = (((FRAME_HEIGHT/2)&0xFF) << 16) | (((FRAME_HEIGHT/2)>>8) << 8) | 0x44;
	dsi_set_cmdq(data_array, 2, 1);
	
	data_array[0] = 0x00351500;// TE ON
	dsi_set_cmdq(data_array, 1, 1);
	//MDELAY(10);

	data_array[0]=0x00110500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(120); 

    data_array[0] = 0x00023902;
    data_array[1] = 0x0000EEFF; 				
    dsi_set_cmdq(data_array, 2, 1);


    data_array[0] = 0x00023902;
    data_array[1] = 0x000001FB; 				
    dsi_set_cmdq(data_array, 2, 1);


    data_array[0] = 0x00023902;
    data_array[1] = 0x00005012;
    dsi_set_cmdq(data_array, 2, 1);


    data_array[0] = 0x00023902;
    data_array[1] = 0x00000213; 				
    dsi_set_cmdq(data_array, 2, 1);
	
	data_array[0] = 0x00023902;////CMD1 
	data_array[1] = 0x000000FF; 				
	dsi_set_cmdq(data_array, 2, 1);    
	data_array[0] = 0x00023902;
	data_array[1] = 0x000001FB; 				
	dsi_set_cmdq(data_array, 2, 1);  

	data_array[0]=0x00290500;
	dsi_set_cmdq(data_array, 1, 1);
	
	//MDELAY(50);
//	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
	need_set_lcm_addr = 1;
}


static void lcm_suspend(void)
{
	unsigned int data_array[16];

	data_array[0]=0x00280500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(120);
	
	data_array[0]=0x00100500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(50);

	data_array[0]=0x00023902;
	data_array[1]=0x0000014F;
	dsi_set_cmdq(data_array, 2, 1);

}


static void lcm_resume(void)
{
	unsigned int data_array[16];
		
	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(50);

    data_array[0] = 0x00023902;
	data_array[1] = 0x0000EEFF; 				
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(2); 
	data_array[0] = 0x00023902;
	data_array[1] = 0x00000826; 				
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(2); 
	data_array[0] = 0x00023902;
	data_array[1] = 0x00000026; 				
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(2); 
	data_array[0] = 0x00023902;
	data_array[1] = 0x000000FF; 				
	dsi_set_cmdq(data_array, 2, 1);
		
	MDELAY(20); 
	SET_RESET_PIN(0);
	MDELAY(1); 
	SET_RESET_PIN(1);
	MDELAY(40); 

    data_array[0]=0x00023902;
#if (LCM_DSI_CMD_MODE)
		data_array[1]=0x000008C2;//cmd mode
#else
		data_array[1]=0x000003C2;//cmd mode
#endif
    dsi_set_cmdq(data_array, 2, 1);

    data_array[0]=0x00023902;
    data_array[1]=0x000002BA;//MIPI lane
    dsi_set_cmdq(data_array, 2, 1);

    //{0x44,	2,	{((FRAME_HEIGHT/2)>>8), ((FRAME_HEIGHT/2)&0xFF)}},
    data_array[0] = 0x00033902;
    data_array[1] = (((FRAME_HEIGHT/2)&0xFF) << 16) | (((FRAME_HEIGHT/2)>>8) << 8) | 0x44;
    dsi_set_cmdq(data_array, 2, 1);

    data_array[0] = 0x00351500;// TE ON
    dsi_set_cmdq(data_array, 1, 1);
    //MDELAY(10);

    data_array[0]=0x00110500;
    dsi_set_cmdq(data_array, 1, 1);
    MDELAY(120);
	

    data_array[0] = 0x00023902;
    data_array[1] = 0x0000EEFF; 				
    dsi_set_cmdq(data_array, 2, 1);


    data_array[0] = 0x00023902;
    data_array[1] = 0x000001FB; 				
    dsi_set_cmdq(data_array, 2, 1);


    data_array[0] = 0x00023902;
    data_array[1] = 0x00005012;
    dsi_set_cmdq(data_array, 2, 1);


    data_array[0] = 0x00023902;
    data_array[1] = 0x00000213; 				
    dsi_set_cmdq(data_array, 2, 1);
	
	data_array[0] = 0x00023902;////CMD1 
	data_array[1] = 0x000000FF; 				
	dsi_set_cmdq(data_array, 2, 1);    
	data_array[0] = 0x00023902;
	data_array[1] = 0x000001FB; 				
	dsi_set_cmdq(data_array, 2, 1);  

    data_array[0]=0x00290500;
    dsi_set_cmdq(data_array, 1, 1);
    need_set_lcm_addr = 1;

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

	// need update at the first time
	if(need_set_lcm_addr)
	{
		data_array[0]= 0x00053902;
		data_array[1]= (x1_MSB<<24)|(x0_LSB<<16)|(x0_MSB<<8)|0x2a;
		data_array[2]= (x1_LSB);
		dsi_set_cmdq(data_array, 3, 1);
		
		data_array[0]= 0x00053902;
		data_array[1]= (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
		data_array[2]= (y1_LSB);
		dsi_set_cmdq(data_array, 3, 1);
		
		need_set_lcm_addr = 0;
	}
	
	data_array[0]= 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);

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
	printk("%s, xxh esd check, read 0xBA = 0x%08x\n", __func__, buffer[0]);
	
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

	return TRUE;
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
		printf("%s, LK nt35590 debug: nt35590 id = 0x%08x\n", __func__, id);
    #else
		printk("%s, kernel nt35590 horse debug: nt35590 id = 0x%08x\n", __func__, id);
    #endif

    if(id == LCM_ID_NT35590)
    	return 1;
    else
        return 0;

}

#ifndef BUILD_LK
static unsigned int lcm_esd_test = FALSE;      ///only for ESD test
#endif

static unsigned int lcm_esd_check(void)
{
  #ifndef BUILD_LK
	char  buffer[3];
	int   array[4];
	int ret = 0;
	
	if(lcm_esd_test)
	{
		lcm_esd_test = FALSE;
		return TRUE;
	}

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x0F, buffer, 1);
	if(buffer[0] != 0xc0)
	{
		printk("[LCM ERROR] [0x0F]=0x%02x\n", buffer[0]);
		ret++;
	}

	read_reg_v2(0x05, buffer, 1);
	if(buffer[0] != 0x00)
	{
		printk("[LCM ERROR] [0x05]=0x%02x\n", buffer[0]);
		ret++;
	}
	
	read_reg_v2(0x0A, buffer, 1);
	if((buffer[0]&0xf)!=0x0C)
	{
		printk("[LCM ERROR] [0x0A]=0x%02x\n", buffer[0]);
		ret++;
	}

	// return TRUE: need recovery
	// return FALSE: No need recovery
	if(ret)
	{
		return TRUE;
	}
	else
	{			 
		return FALSE;
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
	need_set_lcm_addr = 1;
	return ret;
#else
	return 0;
#endif
}
// ---------------------------------------------------------------------------
//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------
LCM_DRIVER nt35590_hd720_dsi_cmd_auo_lcm_drv = 
{
    .name			= "nt35590_AUO",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id     = lcm_compare_id,
	.ata_check		= lcm_ata_check,
	.esd_check   	= lcm_esd_check,
    .esd_recover	= lcm_esd_recover,
#if (LCM_DSI_CMD_MODE)
	.update         = lcm_update,
	//.set_backlight	= lcm_setbacklight,
//	.set_pwm        = lcm_setpwm,
//	.get_pwm        = lcm_getpwm,
#endif
};
