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

#define LCM_ID_RM68210 (0x8000)


#if 0
#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif

static unsigned int lcm_esd_test = FALSE;      ///only for ESD test
#endif

#define LCM_DSI_CMD_MODE									1

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
		params->dsi.LANE_NUM				= LCM_THREE_LANE;
		//The following defined the fomat for data coming from LCD engine.
		params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;
		params->dsi.vertical_sync_active				= 4;// 3    2
		params->dsi.vertical_backporch					= 4;// 20   1
		params->dsi.vertical_frontporch					= 8; // 1  12
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 

		params->dsi.horizontal_sync_active				= 5;// 50  2
		params->dsi.horizontal_backporch				= 33;
		params->dsi.horizontal_frontporch				= 386;
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH/2;

		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
		params->dsi.ufoe_enable = 1;
		params->dsi.ssc_disable = 1;
#if (LCM_DSI_CMD_MODE)
		params->dsi.PLL_CLOCK = 160;
#else
//    params->dsi.PLL_CLOCK = 148;//dsi clock customization: should config clock value directly
#endif
		params->dsi.fbk_div = 10;
		params->dsi.pll_div1 = 0x0;
		params->dsi.pll_div2 = 0x0;
}

static void lcm_init(void)
{
	//int i;
	//unsigned char buffer[10];
	//unsigned int  array[16];
	unsigned int data_array[16];

		SET_RESET_PIN(1);
		MDELAY(10); 
		SET_RESET_PIN(0);
		MDELAY(10); 
		SET_RESET_PIN(1);
		MDELAY(10);
#if 0
	data_array[0]=0x00023902;
    data_array[1]=0x000001FE;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00001327;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00001328;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00001329;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000132A;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000502F;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00005A34;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000001B;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00005216;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000812;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000061A;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00002846;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00006052;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000053;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00006054;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000055;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x000003FE;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000500;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00001601;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000102;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000503;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00007D04;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000005;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00005006;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000507;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00001608;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000309;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000070A;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00007D0B;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000000C;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000500D;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000050E;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000060F;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000710;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000811;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000012;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00007D13;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000014;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00008515;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000816;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000917;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000a18;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000B19;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000C1A;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000001B;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00007D1C;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000001D;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000851E;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000081F;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000020;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000021;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000022;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000023;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000024;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000025;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000026;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000027;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000028;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000029;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000052a;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000062B;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000072D;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000082F;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000030;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00004031;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000532;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000833;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00005434;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00007D35;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000036;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000937;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000A38;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000B39;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000C3A;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000003B;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000403D;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000053F;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000840;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00005441;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00007D42;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000043;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000044;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000045;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000046;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000047;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000048;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000049;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000004A;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000004B;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000004C;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000004D;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000004E;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000004F;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000050;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000051;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000052;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000053;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000054;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000055;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000056;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000058;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000059;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000005A;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000005B;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000005C;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000005D;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000005E;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000005F;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000060;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000061;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000062;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000063;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000064;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000065;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000066;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000067;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000068;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000069;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000006A;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000006B;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000006C;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000006D;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000006E;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000006F;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000070;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000071;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00002072;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000073;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000874;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000875;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000876;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000877;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000878;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000879;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000007A;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000007B;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x0000007C;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x0000007D;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x0000BF7e;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003F7F;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003F80;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00003F81;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003F82;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003F83;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003F84;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000285;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000686;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003F87;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000888;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000C89;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000A8A;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000E8B;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x0000108C;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x0000148D;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x0000128E;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x0000168F;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000090;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000491;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003F92;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003F93;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003F94;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003F95;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000596;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000197;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00001798;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00001399;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x0000159A;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x0000119B;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000F9C;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000B9D;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000D9E;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x0000099F;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FA0;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x000007A2;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x000003A3;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FA4;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FA5;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FA6;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FA7;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FA9;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FAA;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FAB;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FAC;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FAD;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FAE;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FAF;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FB0;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FB1;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FB2;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x000005B3;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x000001B4;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FB5;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000FB6;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000BB7;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000DB8;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x000009B9;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x000013BA;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x000017BB;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x000011BC;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x000015BD;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x000007BE;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x000003BF;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FC0;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FC1;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FC2;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FC3;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x000002C4;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x000006C5;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x000014C6;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x000010C7;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x000016C8;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x000012C9;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x000008CA;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000CCB;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000ACC;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000ECD;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FCE;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x000000CF;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x000004D0;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FD1;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FD2;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FD3;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FD4;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FD5;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FD6;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00003FD7;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x000004FE;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x00000060;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00001261;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00001962;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000E63;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000664;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00001465;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00001066;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000C67;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00001668;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000C69;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000E6A;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x0000086B;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000F6C;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x0000116D;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000C6E;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x0000006F;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000070;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00001271;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00001972;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000E73;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000674;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00001475;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00001076;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000C77;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00001678;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000C79;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000E7A;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x0000087B;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000F7C;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x0000117D;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00000C7E;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x0000007F;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x000006FE;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00004F15;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x00004D4C;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x000007FE;
	dsi_set_cmdq(data_array, 2, 1);
	
	data_array[0]=0x00023902;
    data_array[1]=0x0000400F;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x000007CC;

//select mtk's ufod
      data_array[0]=0x000A1500;
      dsi_set_cmdq(data_array, 1, 1);
 //vlc enable at bit7    
      data_array[0]=0x000B1500;
      dsi_set_cmdq(data_array, 1, 1);
 //cfg[31:24]
      data_array[0]=0x000C1500;
      dsi_set_cmdq(data_array, 1, 1);
 //cfg[23:16]
      data_array[0]=0x000D1500;
      dsi_set_cmdq(data_array, 1, 1);
 //cfg[15:8]
      data_array[0]=0x010E1500;
      dsi_set_cmdq(data_array, 1, 1);
 //cfg[7:0]


	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x000015B0;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x000059B2;
	dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00023902;
    data_array[1]=0x0000BFB4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x000000FE;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00023902;
    data_array[1]=0x0000A958;
	dsi_set_cmdq(data_array, 2, 1);
	
	data_array[0]=0x00023902;
#if (LCM_DSI_CMD_MODE)
	  	data_array[1]=0x000008C2;//cmd mode
#else
			data_array[1]=0x00000BC2;//video mode
#endif
	dsi_set_cmdq(data_array, 2, 1);
			data_array[0] = 0x00033902;
			data_array[1] = (((FRAME_HEIGHT/2)&0xFF) << 16) | (((FRAME_HEIGHT/2)>>8) << 8) | 0x44;
			dsi_set_cmdq(data_array, 2, 1);
	
      data_array[0]=0x00351500;
      dsi_set_cmdq(data_array, 1, 1);

	data_array[0]=0x00110500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(120);

	data_array[0]=0x00290500;
	dsi_set_cmdq(data_array, 1, 1);
#else
#if 1
      data_array[0]=0x07FE1500;
      dsi_set_cmdq(data_array, 1, 1);
 //engineering mode
      data_array[0]=0x5D031500;
      dsi_set_cmdq(data_array, 1, 1);
#endif	
	  data_array[0]=0x01FE1500;
      dsi_set_cmdq(data_array, 1, 1);
#if 1
      data_array[0]=0x760A1500;
      dsi_set_cmdq(data_array, 1, 1);

      data_array[0]=0x880B1500;
      dsi_set_cmdq(data_array, 1, 1);
#endif
      data_array[0]=0x0F271500;
      dsi_set_cmdq(data_array, 1, 1);

      data_array[0]=0x0F281500;
      dsi_set_cmdq(data_array, 1, 1);

      data_array[0]=0x0F291500;
      dsi_set_cmdq(data_array, 1, 1);

      data_array[0]=0x0F2A1500;
      dsi_set_cmdq(data_array, 1, 1);

      data_array[0]=0x08121500;
      dsi_set_cmdq(data_array, 1, 1);

      data_array[0]=0x28461500;
      dsi_set_cmdq(data_array, 1, 1);

      data_array[0]=0x221B1500;
      dsi_set_cmdq(data_array, 1, 1);

      data_array[0]=0x210E1500;
      dsi_set_cmdq(data_array, 1, 1);

      data_array[0]=0x03FE1500;
      dsi_set_cmdq(data_array, 1, 1);

      data_array[0]=0x05001500;
      dsi_set_cmdq(data_array, 1, 1);

      data_array[0]=0x16011500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x01021500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x05031500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x7D041500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x00051500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x50061500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x05071500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x16081500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x03091500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x070A1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x7D0B1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x000C1500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x500D1500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x050E1500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x060F1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x07101500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x08111500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x00121500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x7D131500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x00141500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x85151500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x08161500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x09171500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x0A181500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x0B191500;
      dsi_set_cmdq(data_array, 1, 1);
  
      data_array[0]=0x0C1A1500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x001B1500;
      dsi_set_cmdq(data_array, 1, 1);

      data_array[0]=0x7D1C1500;
      dsi_set_cmdq(data_array, 1, 1);
 
      data_array[0]=0x001D1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x851E1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x081F1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x00201500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x00211500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x00221500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x00231500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x00241500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x00251500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x00261500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x00271500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x00281500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x00291500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x052A1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x062B1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x072D1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x082F1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x00301500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x40311500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x05321500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x08331500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x54341500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x7D351500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x00361500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x09371500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x0A381500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x0B391500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x0C3A1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x003B1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x403D1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x053F1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x08401500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x54411500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x7D421500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x00431500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x00441500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x00451500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x00461500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x00471500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x00481500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x00491500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x004A1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x004B1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x004C1500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x004D1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x004E1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x004F1500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x00501500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x00511500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x00521500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x00531500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x00541500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x00551500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x00561500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x00581500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x00591500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x005A1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x005B1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x005C1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x005D1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x005E1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x005F1500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x00601500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x00611500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x00621500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x00631500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x00641500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x00651500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x00661500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x00671500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x00681500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x00691500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x006A1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x006B1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x006C1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x006D1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x006E1500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x006F1500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x00701500;
      dsi_set_cmdq(data_array, 1, 1);
  
      data_array[0]=0x00711500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x20721500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x00731500;
      dsi_set_cmdq(data_array, 1, 1);
  
      data_array[0]=0x00741500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x00751500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x00761500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x00771500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x00781500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x00791500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x007A1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x007B1500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x007C1500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x007D1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0xBF7E1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3F7F1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3F801500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3F811500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3F821500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x3F831500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x3F841500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x02851500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x06861500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x3F871500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x08881500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x0C891500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x0A8A1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x0E8B1500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x108C1500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x148D1500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x128E1500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x168F1500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x00901500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x04911500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x3F921500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3F931500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x3F941500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3F951500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x05961500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x01971500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x17981500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x13991500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x159A1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x119B1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x0F9C1500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x0B9D1500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x0D9E1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x099F1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3FA01500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x07A21500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x03A31500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3FA41500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3FA51500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3FA61500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3FA71500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x3FA91500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3FAA1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3FAB1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3FAC1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3FAD1500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x3FAE1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3FAF1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3FB01500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3FB11500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3FB21500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x05B31500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x01B41500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x3FB51500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x0FB61500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x0BB71500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x0DB81500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x09B91500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x13BA1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x17BB1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x11BC1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x15BD1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x07BE1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x03BF1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3FC01500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3FC11500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3FC21500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3FC31500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x02C41500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x06C51500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x14C61500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x10C71500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x16C81500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x12C91500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x08CA1500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x0CCB1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x0ACC1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x0ECD1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3FCE1500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x00CF1500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x04D01500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3FD11500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3FD21500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3FD31500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3FD41500;
      dsi_set_cmdq(data_array, 1, 1);
   
      data_array[0]=0x3FD51500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3FD61500;
      dsi_set_cmdq(data_array, 1, 1);
    
      data_array[0]=0x3FD71500;
      dsi_set_cmdq(data_array, 1, 1);

      data_array[0]=0x06FE1500;
      dsi_set_cmdq(data_array, 1, 1);

      data_array[0]=0x4D4C1500;
      dsi_set_cmdq(data_array, 1, 1);

      data_array[0]=0x07FE1500;
      dsi_set_cmdq(data_array, 1, 1);
 //engineering mode
      data_array[0]=0x07CC1500;
      dsi_set_cmdq(data_array, 1, 1);

      data_array[0]=0x34B21500;
      dsi_set_cmdq(data_array, 1, 1);

      data_array[0]=0x04B51500;
      dsi_set_cmdq(data_array, 1, 1);

      data_array[0]=0x400F1500;
      dsi_set_cmdq(data_array, 1, 1);
 //select mtk's ufod
      data_array[0]=0x800A1500;
      dsi_set_cmdq(data_array, 1, 1);
 //vlc enable at bit7    
      data_array[0]=0x000B1500;
      dsi_set_cmdq(data_array, 1, 1);
 //cfg[31:24]
      data_array[0]=0x000C1500;
      dsi_set_cmdq(data_array, 1, 1);
 //cfg[23:16]
      data_array[0]=0x000D1500;
      dsi_set_cmdq(data_array, 1, 1);
 //cfg[15:8]
      data_array[0]=0x050E1500;
      dsi_set_cmdq(data_array, 1, 1);
 //cfg[7:0]
      data_array[0]=0x00FE1500;
      dsi_set_cmdq(data_array, 1, 1);
#if 0
      data_array[0]=0xC8551500;
      dsi_set_cmdq(data_array, 1, 1);
#endif
      data_array[0]=0xA9581500;
      dsi_set_cmdq(data_array, 1, 1);

#if (LCM_DSI_CMD_MODE)
	  	data_array[0]=0x08C21500;//cmd mode
#else
			data_array[0]=0x08C21500;//video mode
#endif
      dsi_set_cmdq(data_array, 1, 1);

      data_array[0]=0x00111500;
      dsi_set_cmdq(data_array, 1, 1);
      MDELAY(120);

			data_array[0] = 0x00033902;
			data_array[1] = (((FRAME_HEIGHT/2)&0xFF) << 16) | (((FRAME_HEIGHT/2)>>8) << 8) | 0x44;
			dsi_set_cmdq(data_array, 2, 1);
	
      data_array[0]=0x00351500;
      dsi_set_cmdq(data_array, 1, 1);

      data_array[0]=0x00291500;
      dsi_set_cmdq(data_array, 1, 1);
#endif
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
}


static void lcm_resume(void)
{
	lcm_init();
}


static void lcm_update(unsigned int x, unsigned int y,
                       unsigned int width, unsigned int height)
{
	static int last_update_x      = -1;
	static int last_update_y      = -1;
	static int last_update_width  = -1;
	static int last_update_height = -1;
	unsigned int need_update = 1;
	
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
	if(-1 == last_update_x && -1 == last_update_y && -1 == last_update_width && -1 == last_update_height)
	{
		last_update_x      = (int)x;
		last_update_y      = (int)y;
		last_update_width  = (int)width;
		last_update_height = (int)height;
	}
	// no need update if the same region as last time
	else if(last_update_x == (int)x && last_update_y == (int)y && last_update_width == (int)width && last_update_height == (int)height)
	{
		//need_update = 0;
	}
	// need update if region change
	else
	{
		last_update_x      = (int)x;
		last_update_y      = (int)y;
		last_update_width  = (int)width;
		last_update_height = (int)height;
	}

	if(need_update)
	{
		data_array[0]= 0x00053902;
		data_array[1]= (x1_MSB<<24)|(x0_LSB<<16)|(x0_MSB<<8)|0x2a;
		data_array[2]= (x1_LSB);
		dsi_set_cmdq(data_array, 3, 1);
		
		data_array[0]= 0x00053902;
		data_array[1]= (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
		data_array[2]= (y1_LSB);
		dsi_set_cmdq(data_array, 3, 1);
	}
	
	data_array[0]= 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);

}

static unsigned int lcm_compare_id(void)
{
	unsigned int id=0;
	unsigned char buffer[3];
	unsigned int array[16];  

		SET_RESET_PIN(1);
		MDELAY(10); 
		SET_RESET_PIN(0);
		MDELAY(10); 
		SET_RESET_PIN(1);
		MDELAY(10);

	array[0] = 0x00033700;// read id return two byte,version and id
	dsi_set_cmdq(array, 1, 1);
	
	read_reg_v2(0x04, buffer, 3);
	id = buffer[0]|(buffer[1]<<8); //we only need ID
    #ifdef BUILD_LK
		printf("%s, LK RM68210 debug: RM68210 id = 0x%08x\n", __func__, id);
    #else
		printk("%s, kernel RM68210 horse debug: RM68210 id = 0x%08x\n", __func__, id);
    #endif

    if(id == LCM_ID_RM68210)
    	return 1;
    else
        return 0;

}

#if 0
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
	return TRUE;
#endif
}

static unsigned int lcm_esd_recover(void)
{
	lcm_init();
	lcm_resume();

	return TRUE;
}
#endif

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
// ---------------------------------------------------------------------------
//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------
LCM_DRIVER rm68210_hd720_dsi_ufoe_cmd_lcm_drv = 
{
    .name			= "rm68210_ufoe",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id     = lcm_compare_id,
	.ata_check		= lcm_ata_check,
//	.esd_check   	= lcm_esd_check,
//    .esd_recover	= lcm_esd_recover,
#if (LCM_DSI_CMD_MODE)
	.update         = lcm_update,
	//.set_backlight	= lcm_setbacklight,
//	.set_pwm        = lcm_setpwm,
//	.get_pwm        = lcm_getpwm,
#endif
};
