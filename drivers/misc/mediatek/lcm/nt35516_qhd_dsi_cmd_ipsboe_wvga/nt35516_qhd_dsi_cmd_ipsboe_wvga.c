#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
	#include <stdlib.h>
	#include <string.h>
	#include <platform/mt_gpio.h>
#elif defined(BUILD_UBOOT)
	#include <asm/arch/mt_gpio.h>
#else
	#include <mach/mt_gpio.h>
#endif
// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  (480)
#define FRAME_HEIGHT (800)
#define LCM_DSI_CMD_MODE

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

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))

#define REGFLAG_DELAY             							0XFE
#define REGFLAG_END_OF_TABLE      							0x00   // END OF REGISTERS MARKER


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)									lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)				lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg											lcm_util.dsi_read_reg()
#define read_reg_v2(cmd, buffer, buffer_size)   				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)     

// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

struct LCM_setting_table {
    unsigned char cmd;
    unsigned char count;
    unsigned char para_list[64];
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
       	}
    }
	
}

static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
	{0x11, 1, {0x00}},
    {REGFLAG_DELAY, 120, {}},

    // Display ON
	{0x29, 1, {0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_sleep_in_setting[] = {
	// Display off sequence
	{0x28, 1, {0x00}},
	{REGFLAG_DELAY, 120, {}},

    // Sleep Mode On
	{0x10, 1, {0x00}},

	{REGFLAG_DELAY, 50, {}},
	{0x4F, 1, {0x01}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
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


	{0xFF,4,{0xAA,0x55,0x25,0x01}},
	{0xF3,7,{0x02,0x03,0x07,0x45,0x88,
			0xD4,0x0D}},
	{0xF4,5,{0x00,0x48,0x00,0x20,0x40}},
	{0xFA,14,{0x00,0x00,0x00,0x00,0x00,
      0x00,0x00,0x00,0x00,0x00,
      0x00,0x00,0x03,0x11}},
	
	{0xF0,5,{0x55,0xAA,0x52,0x08,0x00}},
	{0xB0,3,{0x00,0x0C,0x40}},
	{0xB1,3,{0xEC,0x00,0x00}},
	{0xB6,1,{0x08}},
	{0xB7,2,{0x72, 0x72}},
	{0xBA,1,{0x05}},
	{0xBC,1,{0x04}},

        //Modified to shorten TE period since it is 5.5 ms before
//	{0xBD,5,{0x01,0x41,0x10,0x37,0x01}},
	{0xBD,5,{0x01,0x41,0x7,0x39,0x01}},
	{0xCC,1,{0x01}},

	{0xF0,5,{0x55,0xAA,0x52,0x08,0x01}},
	{0xB0,3,{0x0A,0x0A,0x0A}},
	{0xB6,3,{0x54,0x54,0x54}},
	{0xB1,3,{0x0A,0x0A,0x0A}},
	{0xB7,3,{0x24,0x24,0x24}},
	{0xB2,3,{0x03,0x03,0x03}},
	{0xB8,3,{0x30,0x30,0x30}},
	{0xB3,3,{0x0D,0x0D,0x0D}},
	{0xB9,3,{0x24,0x24,0x24}},
	{0xB4,3,{0x0A,0x0A,0x0A}},
	{0xBA,3,{0x24,0x24,0x24}},
	{0xB5,3,{0x07,0x07,0x07}},
	{0xBC,3,{0x00,0xA0,0x01}},
	{0xBD,3,{0x00,0xA0,0x01}},
	{0xBE,1,{0x48}},

	{0xD1,16,{0x00,0x00,0x00,0x68,0x00,
			  0xB1,0x00,0xE4,0x00,0xFD,
			  0x01,0x2D,0x01,0x4C,0x01,
			  0x79}},
	{0xD2,16,{0x01,0x9B,0x01,0xD3,0x02,0x00,0x02,0x4A,0x02,0x87,
		 0x02,0x88,0x02,0xC0,0x02,0xF8}},

	{0xD3,16,{0x03,0x1A,0x03,0x48,0x03,0x63,0x03,0x8C,0x03,0xA4,
				0x03,0xCC,0x03,0xE2,0x03,0xEF}},
	
	{0xd4,4,{0x03,0xf4,0x03,0xff}},

	{0xD5,16,{0x00,0x00,0x00,0x68,0x00,0xB1,0x00,0xE4,0x00,0xFD,
				0x01,0x2D,0x01,0x4C,0x01,0x79}}	,
				
	{0xd6,16,{0x01,0x9B,0x01,0xD3,0x02,0x00,0x02,0x4A,0x02,0x87,
				0x02,0x88,0x02,0xC0,0x02,0xF8}} ,
				
	{0xd7,16,{0x03,0x1A,0x03,0x48,0x03,0x63,0x03,0x8C,0x03,0xA4,
				0x03,0xCC,0x03,0xE2,0x03,0xEF}},
		
	{0xd8,4,{0x03,0xf4,0x03,0xff}},
	
	{0xd9,16,{0x00,0x00,0x00,0x68,0x00,0xB1,0x00,0xE4,0x00,0xFD,
				0x01,0x2D,0x01,0x4C,0x01,0x79}},

	{0xDD,16,{0x01,0x9B,0x01,0xD3,0x02,0x00,0x02,0x4A,0x02,0x87,
				0x02,0x88,0x02,0xC0,0x02,0xF8}} ,

	{0xDE,16,{0x03,0x1A,0x03,0x48,0x03,0x63,0x03,0x8C,0x03,0xA4,
				0x03,0xCC,0x03,0xE2,0x03,0xEF}}  ,

	{0xdf,4,{0x03,0xf4,0x03,0xff}},
	
	{0xe0,16,{0x00,0x00,0x00,0x68,0x00,0xB1,0x00,0xE4,0x00,0xFD,
				0x01,0x2D,0x01,0x4C,0x01,0x79}},
				
	{0xe1,16,{0x01,0x9B,0x01,0xD3,0x02,0x00,0x02,0x4A,0x02,0x87,
				0x02,0x88,0x02,0xC0,0x02,0xF8}},
	
	{0xe2,16,{0x03,0x1A,0x03,0x48,0x03,0x63,0x03,0x8C,0x03,0xA4,
				0x03,0xCC,0x03,0xE2,0x03,0xEF}},
	
	{0xe3,4,{0x03,0xf4,0x03,0xff}},
	
	{0xe4,16,{0x00,0x00,0x00,0x68,0x00,0xB1,0x00,0xE4,0x00,0xFD,
				0x01,0x2D,0x01,0x4C,0x01,0x79}},
				
	{0xe5,16,{0x01,0x9B,0x01,0xD3,0x02,0x00,0x02,0x4A,0x02,0x87,
				0x02,0x88,0x02,0xC0,0x02,0xF8}},
	
	{0xe6,16,{0x03,0x1A,0x03,0x48,0x03,0x63,0x03,0x8C,0x03,0xA4,
				0x03,0xCC,0x03,0xE2,0x03,0xEF}},
				
	{0xe7,4,{0x03,0xf4,0x03,0xff}},
	
	{0xe8,16,{0x00,0x00,0x00,0x68,0x00,0xB1,0x00,0xE4,0x00,0xFD,
				0x01,0x2D,0x01,0x4C,0x01,0x79}},

	{0xe9,16,{0x01,0x9B,0x01,0xD3,0x02,0x00,0x02,0x4A,0x02,0x87,
				0x02,0x88,0x02,0xC0,0x02,0xF8}} ,
	       
	{0xea,16,{0x03,0x1A,0x03,0x48,0x03,0x63,0x03,0x8C,0x03,0xA4,
				0x03,0xCC,0x03,0xE2,0x03,0xEF}},
				
	{0xeb,4,{0x03,0xf4,0x03,0xff}},

	// Strongly recommend not to set Sleep out / Display On here. That will cause messed frame to be shown as later the backlight is on.
	{0x3A,1,{0x77}},

	{0x11,1,{0x00}},
	{REGFLAG_DELAY, 150, {}},

	{0x29,1,{0x77}},
	{REGFLAG_DELAY, 40, {}},

	{0x2C,1,{0x77}},

	{0xF0,5,{0x55,0xAA,0x52,0x08,0x00}},

	{0x44,2,{(FRAME_HEIGHT/2)>>8,(FRAME_HEIGHT/2)&0xFF}},
	{0x35,1,{0x00}},

	{0xB1,2,{0xE8,0x00}},

	{0xE0,2,{0x01,0x03}},
	// Setting ending by predefined flag
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

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

		// enable tearing-free
		params->dbi.te_mode 				= LCM_DBI_TE_MODE_DISABLED;
		params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;

#if defined(LCM_DSI_CMD_MODE)
		params->dsi.mode   = CMD_MODE;
#else
		params->dsi.mode   = SYNC_PULSE_VDO_MODE;
#endif
	
		// DSI
		/* Command mode setting */
		params->dsi.LANE_NUM				= LCM_TWO_LANE;
		//The following defined the fomat for data coming from LCD engine.
		params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
		params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
		params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
		params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;


		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
		params->dsi.vertical_active_line				= FRAME_HEIGHT;

		params->dsi.intermediat_buffer_num = 0;//because DSI/DPI HW design change, this parameters should be 0 when video mode in MT658X; or memory leakage

		params->dsi.pll_div1=1;		// div1=0,1,2,3;div1_real=1,2,4,4
		params->dsi.pll_div2=1;		// div2=0,1,2,3;div1_real=1,2,4,4
		params->dsi.fbk_div =38;		// fref=26MHz, fvco=fref*(fbk_div+1)*2/(div1_real*div2_real)
}

static void init_lcm_registers(void)
{
#if 1
	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
#else
	unsigned int data_array[16];
//*************Enable CMD2 Page1  *******************//
	data_array[0]=0x00053902;
	data_array[1]=0x2555AAFF;
	data_array[2]=0x00000001;
	dsi_set_cmdq(data_array, 3, 1);
	//MDELAY(10);	

	data_array[0]=0x00083902;
	data_array[1]=0x070302F3;
	data_array[2]=0x0DD48845;
	dsi_set_cmdq(data_array, 3, 1);
	//MDELAY(10);	

    data_array[0]=0x00063902;
	data_array[1]=0x004800F4;
	data_array[2]=0x00004000;
	dsi_set_cmdq(data_array, 3, 1);
	//MDELAY(10);

//*************Enable CMD2 Page0  *******************//
	data_array[0]=0x00063902;
	data_array[1]=0x52AA55F0;
	data_array[2]=0x00000008;
	dsi_set_cmdq(data_array, 3, 1);
	//MDELAY(10);

    data_array[0]=0x00063902;
	data_array[1]=0x400C00B0;
	data_array[2]=0x00003C3C;
	dsi_set_cmdq(data_array, 3, 1);
	//MDELAY(10);

	data_array[0]=0x00033902;
	data_array[1]=0x0000ECB1;
	dsi_set_cmdq(data_array, 2, 1);
	//MDELAY(10);

    data_array[0]=0x08B61500;
	dsi_set_cmdq(data_array, 1, 1);
	//MDELAY(10);	

    data_array[0]=0x00033902;
	data_array[1]=0x007272B7;
	dsi_set_cmdq(data_array, 2, 1);
	//MDELAY(10);

    data_array[0]=0x05BA1500;
	dsi_set_cmdq(data_array, 1, 1);
	//MDELAY(10);

    data_array[0]=0x04BC1500; //4 dot inversion
	//data_array[0]=0x00BC1500;
	dsi_set_cmdq(data_array, 1, 1);
	//MDELAY(10);

    data_array[0]=0x00063902;
	data_array[1]=0x104101BD;
	data_array[2]=0x00000137;
	dsi_set_cmdq(data_array, 3, 1);
	//MDELAY(10);

    data_array[0]=0x01CC1500;
	dsi_set_cmdq(data_array, 1, 1);
	//MDELAY(10);

//*************Enable CMD2 Page1  *******************//
	data_array[0]=0x00063902;
	data_array[1]=0x52AA55F0;
	data_array[2]=0x00000108;
	dsi_set_cmdq(data_array, 3, 1);
	//MDELAY(10);

	data_array[0]=0x00043902;
	data_array[1]=0x0A0A0AB0;
	dsi_set_cmdq(data_array, 2, 1);
	//MDELAY(10);

	data_array[0]=0x00043902;
	data_array[1]=0x545454B6;
	dsi_set_cmdq(data_array, 2, 1);
	//MDELAY(10);	

	data_array[0]=0x00043902;
	data_array[1]=0x0A0A0AB1;
	dsi_set_cmdq(data_array, 2, 1);
	//MDELAY(10);

	data_array[0]=0x00043902;
	data_array[1]=0x242424B7;
	dsi_set_cmdq(data_array, 2, 1);
	//MDELAY(10);

	data_array[0]=0x00043902;
	data_array[1]=0x030303B2;
	dsi_set_cmdq(data_array, 2, 1);
	//MDELAY(10);

	data_array[0]=0x00043902;
	data_array[1]=0x303030B8;
	dsi_set_cmdq(data_array, 2, 1);
	//MDELAY(10);

	data_array[0]=0x00043902;
	data_array[1]=0x0D0D0DB3;
	dsi_set_cmdq(data_array, 2, 1);
	//MDELAY(10);

	data_array[0]=0x00043902;
	data_array[1]=0x242424B9;
	dsi_set_cmdq(data_array, 2, 1);
	//MDELAY(10);

	data_array[0]=0x00043902;
	data_array[1]=0x0A0A0AB4;
	dsi_set_cmdq(data_array, 2, 1);
	//MDELAY(10);

	data_array[0]=0x00043902;
	data_array[1]=0x242424BA;
	dsi_set_cmdq(data_array, 2, 1);
	//MDELAY(10);

	data_array[0]=0x00043902;
	data_array[1]=0x070707B5;
	dsi_set_cmdq(data_array, 2, 1);
	//MDELAY(10);

	data_array[0]=0x00043902;
	data_array[1]=0x01A000BC;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00043902;
	data_array[1]=0x01A000BD;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x48BE1500;
	dsi_set_cmdq(data_array, 1, 1);

//start gamma value
	data_array[0]=0x00113902;
	data_array[1]=0x000000D1;
	data_array[2]=0x00B10068;
	data_array[3]=0x01FD00E4;
	data_array[4]=0x014C012D;
	data_array[5]=0x00000079;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00113902;
	data_array[1]=0x019B01D2;
	data_array[2]=0x020002D3;
	data_array[3]=0x0287024A;
	data_array[4]=0x02C00288;
	data_array[5]=0x000000F8;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00113902;
	data_array[1]=0x031A03D3;
	data_array[2]=0x03630348;
	data_array[3]=0x03A4038C;
	data_array[4]=0x03E203CC;
	data_array[5]=0x000000EF;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00053902;
	data_array[1]=0x03F403D4;
	data_array[2]=0x000000FF;
	dsi_set_cmdq(data_array, 3, 1);

//End gamma value
	data_array[0]=0x00113902;
	data_array[1]=0x000000D5;
	data_array[2]=0x00B10068;
	data_array[3]=0x01FD00E4;
	data_array[4]=0x014C012D;
	data_array[5]=0x00000079;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00113902;
	data_array[1]=0x019B01D6;
	data_array[2]=0x020002D3;
	data_array[3]=0x0287024A;
	data_array[4]=0x02C00288;
	data_array[5]=0x000000F8;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00113902;
	data_array[1]=0x031A03D7;
	data_array[2]=0x03630348;
	data_array[3]=0x03A4038C;
	data_array[4]=0x03E203CC;
	data_array[5]=0x000000EF;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00053902;
	data_array[1]=0x03F403D8;
	data_array[2]=0x000000FF;
	dsi_set_cmdq(data_array, 3, 1);
//End gamma value

	data_array[0]=0x00113902;
	data_array[1]=0x000000D9;
	data_array[2]=0x00B10068;
	data_array[3]=0x01FD00E4;
	data_array[4]=0x014C012D;
	data_array[5]=0x00000079;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00113902;
	data_array[1]=0x019B01DD;
	data_array[2]=0x020002D3;
	data_array[3]=0x0287024A;
	data_array[4]=0x02C00288;
	data_array[5]=0x000000F8;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00113902;
	data_array[1]=0x031A03DE;
	data_array[2]=0x03630348;
	data_array[3]=0x03A4038C;
	data_array[4]=0x03E203CC;
	data_array[5]=0x000000EF;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00053902;
	data_array[1]=0x03F403DF;
	data_array[2]=0x000000FF;
	dsi_set_cmdq(data_array, 3, 1);
//End gamma value

	data_array[0]=0x00113902;
	data_array[1]=0x000000E0;
	data_array[2]=0x00B10068;
	data_array[3]=0x01FD00E4;
	data_array[4]=0x014C012D;
	data_array[5]=0x00000079;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00113902;
	data_array[1]=0x019B01E1;
	data_array[2]=0x020002D3;
	data_array[3]=0x0287024A;
	data_array[4]=0x02C00288;
	data_array[5]=0x000000F8;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00113902;
	data_array[1]=0x031A03E2;
	data_array[2]=0x03630348;
	data_array[3]=0x03A4038C;
	data_array[4]=0x03E203CC;
	data_array[5]=0x000000EF;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00053902;
	data_array[1]=0x03F403E3;
	data_array[2]=0x000000FF;
	dsi_set_cmdq(data_array, 3, 1);
//End gamma value

	data_array[0]=0x00113902;
	data_array[1]=0x000000E4;
	data_array[2]=0x00B10068;
	data_array[3]=0x01FD00E4;
	data_array[4]=0x014C012D;
	data_array[5]=0x00000079;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00113902;
	data_array[1]=0x019B01E5;
	data_array[2]=0x020002D3;
	data_array[3]=0x0287024A;
	data_array[4]=0x02C00288;
	data_array[5]=0x000000F8;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00113902;
	data_array[1]=0x031A03E6;
	data_array[2]=0x03630348;
	data_array[3]=0x03A4038C;
	data_array[4]=0x03E203CC;
	data_array[5]=0x000000EF;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00053902;
	data_array[1]=0x03F403E7;
	data_array[2]=0x000000FF;
	dsi_set_cmdq(data_array, 3, 1);
//End gamma value

	data_array[0]=0x00113902;
	data_array[1]=0x000000E8;
	data_array[2]=0x00B10068;
	data_array[3]=0x01FD00E4;
	data_array[4]=0x014C012D;
	data_array[5]=0x00000079;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00113902;
	data_array[1]=0x019B01E9;
	data_array[2]=0x020002D3;
	data_array[3]=0x0287024A;
	data_array[4]=0x02C00288;
	data_array[5]=0x000000F8;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00113902;
	data_array[1]=0x031A03EA;
	data_array[2]=0x03630348;
	data_array[3]=0x03A4038C;
	data_array[4]=0x03E203CC;
	data_array[5]=0x000000EF;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00053902;
	data_array[1]=0x03F403EB;
	data_array[2]=0x000000FF;
	dsi_set_cmdq(data_array, 3, 1);
//End gamma value
    data_array[0]=0x773A1500;//Data Type 
	dsi_set_cmdq(data_array, 1, 1);
	//MDELAY(10);	

//	data_array[0] = 0x00351500;// TE ON
//	dsi_set_cmdq(&data_array, 1, 1);
//	MDELAY(10);

	data_array[0] = 0x00110500;		// Sleep Out
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(120);
	
	data_array[0] = 0x00290500;		// Display On
	dsi_set_cmdq(data_array, 1, 1);
//	MDELAY(40);

	data_array[0] = 0x002C0500; 	// Display On
	dsi_set_cmdq(data_array, 1, 1);
	//MDELAY(10);
	
//******************* ENABLE PAGE0 **************//
	data_array[0]=0x00063902;
	data_array[1]=0x52AA55F0;
	data_array[2]=0x00000008;
	dsi_set_cmdq(data_array, 3, 1);
	//MDELAY(10);	


	//{0x44,	2,	{((FRAME_HEIGHT/2)>>8), ((FRAME_HEIGHT/2)&0xFF)}},
	data_array[0] = 0x00033902;
	data_array[1] = (((FRAME_HEIGHT/2)&0xFF) << 16) | (((FRAME_HEIGHT/2)>>8) << 8) | 0x44;
	dsi_set_cmdq(data_array, 2, 1);
	
	data_array[0] = 0x00351500;// TE ON
	dsi_set_cmdq(data_array, 1, 1);
	//MDELAY(10);

	data_array[0]= 0x00033902;
	data_array[1]= 0x0000E8B1;
	dsi_set_cmdq(data_array, 2, 1);
	//MDELAY(50);

	data_array[0]= 0x00023902;
	data_array[1]= 0x0051;
	dsi_set_cmdq(data_array, 2, 1);
	//MDELAY(50);

        //data_array[0]= 0x00033902;
	//data_array[1]= 0x640044;
	//dsi_set_cmdq(&data_array, 2, 1);
	//MDELAY(50);

	data_array[0]= 0x00023902;
	data_array[1]= 0x2453;
	dsi_set_cmdq(data_array, 2, 1);
	//MDELAY(50);

    	data_array[0] = 0x01551500;	// SET CABC UI MODE
    	dsi_set_cmdq(data_array, 1, 1);
    	//MDELAY(5);

	data_array[0]= 0x00023902;
	data_array[1]= 0x705e;
	dsi_set_cmdq(data_array, 2, 1);
	//MDELAY(50);

	data_array[0]= 0x00033902;
	data_array[1]= 0x000301E0;
	dsi_set_cmdq(data_array, 2, 1);
	//MDELAY(50);
#endif
}

static void lcm_init(void)
{
	SET_RESET_PIN(1);
    MDELAY(50);
	SET_RESET_PIN(0);
    MDELAY(10);
    SET_RESET_PIN(1);
    MDELAY(10);
    init_lcm_registers();
}


static void lcm_suspend(void)
{
#if 1
	push_table(lcm_sleep_in_setting, sizeof(lcm_sleep_in_setting) / sizeof(struct LCM_setting_table), 1);
#ifndef BUILD_LK
	mt_set_gpio_mode(GPIO130, GPIO_MODE_GPIO);
	mt_set_gpio_mode(GPIO131, GPIO_MODE_GPIO);
	mt_set_gpio_mode(GPIO154, GPIO_MODE_GPIO);
	
	mt_set_gpio_dir(GPIO130, GPIO_DIR_IN);
	mt_set_gpio_dir(GPIO131, GPIO_DIR_IN);
	mt_set_gpio_dir(GPIO154, GPIO_DIR_IN);

	mt_set_gpio_pull_enable(GPIO130, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_enable(GPIO131, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_enable(GPIO154, GPIO_PULL_ENABLE);

	mt_set_gpio_pull_select(GPIO130, GPIO_PULL_DOWN);
	mt_set_gpio_pull_select(GPIO131, GPIO_PULL_DOWN);
	mt_set_gpio_pull_select(GPIO154, GPIO_PULL_DOWN);
#endif
#else
		unsigned int data_array[2];

//below BTA for can not sleep in
//	data_array[0]=0x00000504; // BTA
//	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0] = 0x00280500; // Display Off
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(100); 
	data_array[0] = 0x00100500; // Sleep In
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(200);
#endif	
}


static void lcm_resume(void)
{
#if 1
#ifndef BUILD_LK
	mt_set_gpio_mode(GPIO131, GPIO_MODE_07);
	mt_set_gpio_dir(GPIO131, GPIO_DIR_OUT);
#endif
	lcm_init(); //must init because deep standby mode
	push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
#else
	unsigned int data_array[16];
	//lcm_init();
	data_array[0]=0x00110500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(150);
	
	data_array[0]=0x00290500;
	dsi_set_cmdq(data_array, 1, 1);
#endif
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
/*	when resume,the engine is in LP mode,so DSI will timeout.
	data_array[0]= 0x00290508;//HW bug, so need send one HS packet
	dsi_set_cmdq(data_array, 1, 1);
*/	
	data_array[0]= 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);

	

}
static unsigned int lcm_compare_id(void)
{

		UINT32   array[4];
		UINT8  buffer[3];
/*		char  id0=0;
		char  id1=0;
		char  id2=0;
*/

		SET_RESET_PIN(0);
		MDELAY(200);
		SET_RESET_PIN(1);
		MDELAY(200);
		
	array[0] = 0x00083700;// read id return two byte,version and id
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x04,buffer, 3);
	
/*	id0 = buffer[0]; //should be 0x00
	id1 = buffer[1];//should be 0x80
	id2 = buffer[2];//should be 0x00
*/	
	return 0;


}

#if 0
static unsigned int lcm_esd_check(void)
{
#ifndef BUILD_LK
		char  buffer[3];
		int   array[4];
#if 0
        if(lcm_esd_test)
        {
            lcm_esd_test = FALSE;
            return TRUE;
        }
#endif
        /// please notice: the max return packet size is 1
        /// if you want to change it, you can refer to the following marked code
        /// but read_reg currently only support read no more than 4 bytes....
        /// if you need to read more, please let BinHan knows.
        /*
                unsigned int data_array[16];
                unsigned int max_return_size = 1;
                
                data_array[0]= 0x00003700 | (max_return_size << 16);    
                
                dsi_set_cmdq(&data_array, 1, 1);
        */
		array[0] = 0x00043700;// read id return two byte,version and id
		dsi_set_cmdq(array, 1, 1);

        read_reg_v2(0x3A, buffer, 1);
		if(buffer[0]==0x77)
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
//    unsigned char para = 0;

	lcm_init();
	lcm_resume();

    return TRUE;
}
#endif

LCM_DRIVER nt35516_qhd_dsi_cmd_ipsboe_wvga_lcm_drv = 
{
    .name			= "nt35516_qhd_dsi_ips_boe_wvga",
	.set_util_funcs = lcm_set_util_funcs,
	.compare_id     = lcm_compare_id,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
#if defined(LCM_DSI_CMD_MODE)
    .update         = lcm_update,
//    .esd_check      = lcm_esd_check,
//    .esd_recover    = lcm_esd_recover,
#endif
    };
