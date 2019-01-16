#ifdef BUILD_LK
#else
#include <linux/string.h>
#if defined(BUILD_UBOOT)
#include <asm/arch/mt6577_gpio.h>
#else
#include <mach/mt6577_gpio.h>
#endif
#endif
#include "lcm_drv.h"

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  										(480)
#define FRAME_HEIGHT 										(800)

#define REGFLAG_DELAY             							0XAA
#define REGFLAG_END_OF_TABLE      							0xAB   // END OF REGISTERS MARKER

#define LCM_DSI_CMD_MODE									0

#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif

#define LCM_TABLE_V3

#define LCM_ID1       (0x90)
#define LCM_ID2				(0x69)

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

#define dsi_set_cmdq_V3(ppara, size, force_update)	        	lcm_util.dsi_set_cmdq_V3(ppara, size, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)											lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)      

static struct LCM_setting_table {
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[128];
};

#if defined(LCM_TABLE_V3)

static LCM_setting_table_V3 lcm_initialization_setting_V3[] = {

	/*
	Note :

	Structure Format :

	{Data_ID, DCS command, count of parameters, {parameter list}},

	{REGFLAG_ESCAPE_ID, REGFLAG_DELAY_MS_V3,	milliseconds of time,		{}},

	...

	*/
	
	{0x39,	0xB9,	3,	{0xFF, 0x83, 0x69}},

	{0x39,	0xB1,	10,	{0x12, 0x83, 0x77, 0x00,
						 0x8F, 0x0F, 0x1A, 0x1A,
						 0x0C, 0x0A}},
			 
	{0x39,	0xB2,	3,	{0x00, 0x10, 0x02}},
	
	{0x39,	0xB3,	4,	{0x83, 0x00, 0x31, 0x03}},
	
	{0x15,	0xB4,	1,	{0x42}},  
	
	{0x39,	0xB6,	2,	{0x7F, 0x7F}},
	
	{0x39,	0xE3,	4,	{0x01, 0x01, 0x01, 0x01}},
	
	{0x39,	0xC0,	6,	{0x73, 0x50, 0x00, 0x3C,
						 0xC4, 0x00}},

	{0x15,	0xCC,	1,	{0x00}},

#if 1
	{0x39,	0xD5,	92,	{0x00, 0x00, 0x10, 0x00,
						 0x00, 0x00, 0x00, 0x12,
						 0x30, 0x00, 0x00, 0x00,
						 0x01, 0x70, 0x33, 0x00,
						 0x00, 0x12, 0x20, 0x60,
						 0x37, 0x00, 0x00, 0x03,
						 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x03, 0x00, 0x00,
						 0x03, 0x00, 0x00, 0x0B,
						 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x88, 0x88, 0x88,
						 0x88, 0x55, 0x77, 0x11,
						 0x33, 0x32, 0x00, 0x00,
						 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x00, 0x88,
						 0x88, 0x88, 0x88, 0x44,
						 0x66, 0x00, 0x22, 0x10,
						 0x00, 0x00, 0x00, 0x01,
						 0x00, 0x00, 0x00, 0x03,
						 0xAA, 0xFF, 0xFF, 0xFF,
						 0x03, 0xAA, 0xFF, 0xFF,
						 0xFF, 0x00, 0x01, 0x5A}},
#else
	{0x39,	0xD5,	59, {0x00, 0x00, 0x10, 0x00,
						 0x00, 0x00, 0x00, 0x12,
						 0x30, 0x00, 0x00, 0x00,
						 0x01, 0x70, 0x33, 0x00,
						 0x00, 0x12, 0x20, 0x60,
						 0x37, 0x00, 0x00, 0x03,
						 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x03, 0x00, 0x00,
						 0x03, 0x00, 0x00, 0x0B,
						 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x88, 0x88, 0x88,
						 0x88, 0x55, 0x77, 0x11,
						 0x33, 0x32, 0x00, 0x00,
						 0x00, 0x00, 0x00}},
				 
	{0x39,	0xFD,	34, {
 			 			 0xAA, 0x00,
						 0x00, 0x00, 0x00, 0x88,
						 0x88, 0x88, 0x88, 0x44,
						 0x66, 0x00, 0x22, 0x10,
						 0x00, 0x00, 0x00, 0x01,
						 0x00, 0x00, 0x00, 0x03,
						 0xAA, 0xFF, 0xFF, 0xFF,
						 0x03, 0xAA, 0xFF, 0xFF,
						 0xFF, 0x00, 0x01, 0x5A}},
#endif

	{0x39,	0xE0,	35,	{0x00, 0x0C, 0x00, 0x10,
						 0x14, 0x3C, 0x27, 0x34,
						 0x0C, 0x10, 0x11, 0x15,
						 0x18, 0x16, 0x16, 0x13,
						 0x18, 0x0C, 0x00, 0x0C,
						 0x10, 0x14, 0x3C, 0x27,
						 0x34, 0x0C, 0x10, 0x11,
						 0x15, 0x18, 0x16, 0x16,
						 0x13, 0x18, 0x01}},

	{0x15,	0xEA,	1,	{0x62}},
	
	{0x15,	0x3A,	1,	{0x77}},
	
	{0x39,	0xBA,	15,	{0x11, 0x00, 0x16, 0xC6,
						 0x00, 0x0A, 0x00, 0x10,
						 0x24, 0x02, 0x21, 0x21,
						 0x9A, 0x17, 0x1D}}
};


static LCM_setting_table_V3 lcm_sleep_out_setting_V3[] = {
    // Sleep Out
	{0x15,	0x11, 1, {0x00}},
	{REGFLAG_ESCAPE_ID, REGFLAG_DELAY_MS_V3,			150, 	{}},

    // Display ON
	{0x15,	0x29, 1, {0x00}},
	{REGFLAG_ESCAPE_ID, REGFLAG_DELAY_MS_V3,			10, 	{}}

};


static LCM_setting_table_V3 lcm_deep_sleep_mode_in_setting_V3[] = {
	// Display off sequence
	{0x15,	0x28, 1, {0x00}},

    // Sleep Mode On
	{0x15,	0x10, 1, {0x00}}
};


static LCM_setting_table_V3 lcm_compare_id_setting_V3[] = {
	// Display off sequence
	{0x39,	0xB9,	3,	{0xFF, 0x83, 0x69}},
	{REGFLAG_ESCAPE_ID, REGFLAG_DELAY_MS_V3,			10, 	{}}
};


static LCM_setting_table_V3 lcm_backlight_level_setting_V3[] = {
	{0x15,	0x51, 1, {0xFF}}
};


#else

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

	{0xB9,	3,	{0xFF, 0x83, 0x69}},
	
	{0xB1,	10,	{0x12, 0x83, 0x77, 0x00,
				 			 0x8F, 0x0F, 0x1A, 0x1A,
				 			 0x0C, 0x0A}},
				 
	{0xB2,	3,	{0x00, 0x10, 0x02}},
	
	{0xB3,	4,	{0x83, 0x00, 0x31, 0x03}},
	
	{0xB4,	1,	{0x42}},  
	
	{0xB6,	2,	{0x7F, 0x7F}},
	
	{0xE3,	4,	{0x01, 0x01, 0x01, 0x01}},
	
	{0xC0,	6,	{0x73, 0x50, 0x00, 0x3C,
				 			 0xC4, 0x00}},

	{0xCC,	1,	{0x00}},

#if 1
	{0xD5,	92,	{0x00, 0x00, 0x10, 0x00,
				 			 0x00, 0x00, 0x00, 0x12,
				 			 0x30, 0x00, 0x00, 0x00,
				 			 0x01, 0x70, 0x33, 0x00,
							 0x00, 0x12, 0x20, 0x60,
							 0x37, 0x00, 0x00, 0x03,
							 0x00, 0x00, 0x00, 0x00,
							 0x00, 0x03, 0x00, 0x00,
							 0x03, 0x00, 0x00, 0x0B,
							 0x00, 0x00, 0x00, 0x00,
							 0x00, 0x00, 0x00, 0x00,
							 0x00, 0x88, 0x88, 0x88,
							 0x88, 0x55, 0x77, 0x11,
							 0x33, 0x32, 0x00, 0x00,
							 0x00, 0x00, 0x00, 0x00,
							 0x00, 0x00, 0x00, 0x88,
							 0x88, 0x88, 0x88, 0x44,
							 0x66, 0x00, 0x22, 0x10,
							 0x00, 0x00, 0x00, 0x01,
							 0x00, 0x00, 0x00, 0x03,
							 0xAA, 0xFF, 0xFF, 0xFF,
							 0x03, 0xAA, 0xFF, 0xFF,
							 0xFF, 0x00, 0x01, 0x5A}},
#else
	{0xD5,	59, {0x00, 0x00, 0x10, 0x00,
							 0x00, 0x00, 0x00, 0x12,
							 0x30, 0x00, 0x00, 0x00,
							 0x01, 0x70, 0x33, 0x00,
							 0x00, 0x12, 0x20, 0x60,
							 0x37, 0x00, 0x00, 0x03,
							 0x00, 0x00, 0x00, 0x00,
							 0x00, 0x03, 0x00, 0x00,
							 0x03, 0x00, 0x00, 0x0B,
							 0x00, 0x00, 0x00, 0x00,
							 0x00, 0x00, 0x00, 0x00,
							 0x00, 0x88, 0x88, 0x88,
							 0x88, 0x55, 0x77, 0x11,
							 0x33, 0x32, 0x00, 0x00,
							 0x00, 0x00, 0x00}},
				 
	{0xFD,	34, {
				 			 0xAA, 0x00,
							 0x00, 0x00, 0x00, 0x88,
							 0x88, 0x88, 0x88, 0x44,
							 0x66, 0x00, 0x22, 0x10,
							 0x00, 0x00, 0x00, 0x01,
							 0x00, 0x00, 0x00, 0x03,
							 0xAA, 0xFF, 0xFF, 0xFF,
							 0x03, 0xAA, 0xFF, 0xFF,
							 0xFF, 0x00, 0x01, 0x5A}},
#endif

#if 0
	{0xD5,	56, {0x00, 0x00, 0x10, 0x00,
							 0x00, 0x00, 0x00, 0x12,
							 0x30, 0x00, 0x00, 0x00,
							 0x01, 0x70, 0x33, 0x00,
							 0x00, 0x12, 0x20, 0x60,
							 0x37, 0x00, 0x00, 0x03,
							 0x00, 0x00, 0x00, 0x00,
							 0x00, 0x03, 0x00, 0x00,
							 0x03, 0x00, 0x00, 0x0B,
							 0x00, 0x00, 0x00, 0x00,
							 0x00, 0x00, 0x00, 0x00,
							 0x00, 0x88, 0x88, 0x88,
							 0x88, 0x55, 0x77, 0x11,
							 0x33, 0x32, 0x00, 0x00}},
				 
	{0xFD,	38, {0xAA, 0xAA,
							 0x00, 0x00, 0x00, 0x00,
							 0x00, 0x00, 0x00, 0x88,
							 0x88, 0x88, 0x88, 0x44,
							 0x66, 0x00, 0x22, 0x10,
							 0x00, 0x00, 0x00, 0x01,
							 0x00, 0x00, 0x00, 0x03,
							 0xAA, 0xFF, 0xFF, 0xFF,
							 0x03, 0xAA, 0xFF, 0xFF,
							 0xFF, 0x00, 0x01, 0x5A}},
#endif

	{0xE0,	35,	{0x00, 0x0C, 0x00, 0x10,
							 0x14, 0x3C, 0x27, 0x34,
							 0x0C, 0x10, 0x11, 0x15,
							 0x18, 0x16, 0x16, 0x13,
							 0x18, 0x0C, 0x00, 0x0C,
							 0x10, 0x14, 0x3C, 0x27,
							 0x34, 0x0C, 0x10, 0x11,
							 0x15, 0x18, 0x16, 0x16,
							 0x13, 0x18, 0x01}},

	{0xEA,	1,	{0x62}},
	
	{0x3A,	1,	{0x77}},
	
	{0xBA,	15,	{0x11, 0x00, 0x16, 0xC6,
							 0x00, 0x0A, 0x00, 0x10,
							 0x24, 0x02, 0x21, 0x21,
							 0x9A, 0x17, 0x1D}},	
	
	// Note
	// Strongly recommend not to set Sleep out / Display On here. That will cause messed frame to be shown as later the backlight is on.


	// Setting ending by predefined flag
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_set_window[] = {
	{0x2A,	4,	{0x00, 0x00, (FRAME_WIDTH>>8), (FRAME_WIDTH&0xFF)}},
	{0x2B,	4,	{0x00, 0x00, (FRAME_HEIGHT>>8), (FRAME_HEIGHT&0xFF)}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
	{0x11, 1, {0x00}},
    {REGFLAG_DELAY, 150, {}},

    // Display ON
	{0x29, 1, {0x00}},
	{REGFLAG_DELAY, 10, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	// Display off sequence
	{0x28, 1, {0x00}},

    // Sleep Mode On
	{0x10, 1, {0x00}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_compare_id_setting[] = {
	// Display off sequence
	{0xB9,	3,	{0xFF, 0x83, 0x69}},
	{REGFLAG_DELAY, 10, {}},

    // Sleep Mode On
//	{0xC3, 1, {0xFF}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_backlight_level_setting[] = {
	{0x51, 1, {0xFF}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

#endif

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i, j, k;
	unsigned int para_int, read_int;
	unsigned char	buffer[256];

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
#if 0
				do {
					read_int = read_reg(cmd);
					para_int = *((unsigned int *)table[i].para_list);
					printf("%s, compare = {%04x, %04x} \n", __func__, read_int, para_int);					
				} while(read_int != para_int);
#endif			
       	}
    }

#if 0//defined(BUILD_UBOOT)
	int remain_bytes;
	int r_byte_count;
	unsigned int  array[16];

	for(i = 0; i < count; i++)	{
		if(table[i].cmd != REGFLAG_DELAY && table[i].cmd != REGFLAG_END_OF_TABLE)
		{
			remain_bytes = table[i].count;
			j = 0;
			
			do {
					//printf("Read 0x%02x round %d. Remain %d bytes \n", table[i].cmd, j, remain_bytes);
					r_byte_count = (remain_bytes > 8) ? 8 : remain_bytes;
				
					array[0] = 0x00003700 | (r_byte_count<<16);// read id return two byte,version and id
					dsi_set_cmdq(array, 1, 1);
					
					if(j>0)
						read_reg_v2(0xFD, (buffer + j*8), r_byte_count);
					else
						read_reg_v2(table[i].cmd, buffer, r_byte_count);

					j++;
					remain_bytes-=8;
			} while(remain_bytes>0);

			printf("0x%02X[%02d] => { ", table[i].cmd, table[i].count);
			for(k = 0; k < table[i].count; k++)
			{
				if((k % 4 ) == 0 && k > 0)
					printf("\n              ", buffer[k]);			
				printf("0x%02X ", buffer[k]);
			}
			printf("} \n");
			
		}
	}
#endif
	
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

		// enable tearing-free
		params->dbi.te_mode 				= LCM_DBI_TE_MODE_DISABLED;
		params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;

#if (LCM_DSI_CMD_MODE)
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

		// Highly depends on LCD driver capability.
		// Not support in MT6573
		params->dsi.packet_size=256;

		// Video mode setting		
		params->dsi.intermediat_buffer_num = 2;

		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
		params->dsi.word_count=480*3;
#if 0
		params->dsi.vertical_sync_active				= 10;
		params->dsi.vertical_backporch					= 10;
		params->dsi.vertical_frontporch					= 10;	// 2
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 

		params->dsi.horizontal_sync_active				= 50;
		params->dsi.horizontal_backporch				= 50;
		params->dsi.horizontal_frontporch				= 50;
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;
#else
		params->dsi.vertical_sync_active				= 4;
		params->dsi.vertical_backporch					= 17;
		params->dsi.vertical_frontporch					= 17;	// 2
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 

		params->dsi.horizontal_sync_active				= 17;
		params->dsi.horizontal_backporch				= 49;
		params->dsi.horizontal_frontporch				= 57;
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;
#endif
		// Bit rate calculation
		params->dsi.pll_div1=28;		// fref=26MHz, fvco=fref*(div1+1)	(div1=0~63, fvco=500MHZ~1GHz)
		params->dsi.pll_div2=1; 		// div2=0~15: fout=fvo/(2*div2)

		/* ESD or noise interference recovery For video mode LCM only. */
		// Send TE packet to LCM in a period of n frames and check the response.
		params->dsi.lcm_int_te_monitor = FALSE;
		params->dsi.lcm_int_te_period = 1;		// Unit : frames

		// Need longer FP for more opportunity to do int. TE monitor applicably.
		if(params->dsi.lcm_int_te_monitor)
			params->dsi.vertical_frontporch *= 2;
		
		// Monitor external TE (or named VSYNC) from LCM once per 2 sec. (LCM VSYNC must be wired to baseband TE pin.)
		params->dsi.lcm_ext_te_monitor = FALSE;
		// Non-continuous clock
		params->dsi.noncont_clock = TRUE;
		params->dsi.noncont_clock_period = 2;	// Unit : frames

}


static void lcm_init(void)
{
	unsigned char buffer[10];
	unsigned int  array[16];

    SET_RESET_PIN(1);
    SET_RESET_PIN(0);
    MDELAY(1);
    SET_RESET_PIN(1);
    MDELAY(10);

#if defined(LCM_TABLE_V3)
	dsi_set_cmdq_V3(lcm_initialization_setting_V3, sizeof(lcm_initialization_setting_V3) / sizeof(LCM_setting_table_V3), 1);
#else
	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
#endif
}


static void lcm_suspend(void)
{
#if defined(LCM_TABLE_V3)
	dsi_set_cmdq_V3(lcm_deep_sleep_mode_in_setting_V3, sizeof(lcm_deep_sleep_mode_in_setting_V3) / sizeof(LCM_setting_table_V3), 1);
#else
	push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);
#endif
	SET_RESET_PIN(0);
	MDELAY(1);
    SET_RESET_PIN(1);
}


static void lcm_resume(void)
{
	lcm_init();

#if defined(LCM_TABLE_V3)	
	dsi_set_cmdq_V3(lcm_sleep_out_setting_V3, sizeof(lcm_sleep_out_setting_V3) / sizeof(LCM_setting_table_V3), 1);
#else
	push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
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
	data_array[3]= 0x00053902;
	data_array[4]= (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
	data_array[5]= (y1_LSB);
	data_array[6]= 0x002c3909;

	dsi_set_cmdq(&data_array, 7, 0);

}

#if 0
static void lcm_setbacklight(unsigned int level)
{
	unsigned int default_level = 145;
	unsigned int mapped_level = 0;

	//for LGE backlight IC mapping table
	if(level > 255) 
			level = 255;

	if(level >0) 
			mapped_level = default_level+(level)*(255-default_level)/(255);
	else
			mapped_level=0;

	// Refresh value of backlight level.
	lcm_backlight_level_setting[0].para_list[0] = mapped_level;

	push_table(lcm_backlight_level_setting, sizeof(lcm_backlight_level_setting) / sizeof(struct LCM_setting_table), 1);
}
#endif

static void lcm_setpwm(unsigned int divider)
{
	// TBD
}


static unsigned int lcm_getpwm(unsigned int divider)
{
	// ref freq = 15MHz, B0h setting 0x80, so 80.6% * freq is pwm_clk;
	// pwm_clk / 255 / 2(lcm_setpwm() 6th params) = pwm_duration = 23706
	unsigned int pwm_clk = 23706 / (1<<divider);	
	return pwm_clk;
}

static unsigned int lcm_compare_id(void)
{
	unsigned int id1, id2;
	unsigned char buffer[2];
	unsigned int array[16];

	//push_table(lcm_compare_id_setting, sizeof(lcm_compare_id_setting) / sizeof(struct LCM_setting_table), 1);
	lcm_init();

	array[0] = 0x00013700;// read id return two byte,version and id
	dsi_set_cmdq(array, 1, 1);
	id1 = read_reg(0xDA);
	id2 = read_reg(0xDB);

#if defined(BUILD_UBOOT)
	printf("%s, id1 = 0x%02x, id2 = 0x%02x \n", __func__, id1, id2);
#endif

    return (LCM_ID1 == id1 && LCM_ID2 == id2 ) ? 1 : 0;
}

LCM_DRIVER hx8369b_dsi_vdo_lcm_drv = 
{
    .name			= "hx8369b_dsi_vdo",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id    = lcm_compare_id,
#if (LCM_DSI_CMD_MODE)
    .update         = lcm_update,
#endif
};

