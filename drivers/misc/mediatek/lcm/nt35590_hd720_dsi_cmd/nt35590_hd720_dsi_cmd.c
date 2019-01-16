#include <linux/string.h>

#include "lcm_drv.h"

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  										(720)
#define FRAME_HEIGHT 										(1280)
#define LCM_ID       (0x69)
#define REGFLAG_DELAY             							0xAB
#define REGFLAG_END_OF_TABLE      							0xAA   // END OF REGISTERS MARKER

#define LCM_ID1												0x00
#define LCM_ID2												0x00
#define LCM_ID3												0x00

#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif

static unsigned int lcm_esd_test = FALSE;      ///only for ESD test
#define LCM_DSI_CMD_MODE									1

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

	
	{0xFF,	1,	{0x00}},
	{0xBA,	1,	{0x01}},		// 2 lane
	{0xC2,	1,	{0x08}},

	{0xFF,	1,	{0x01}},
	{0x00, 	1,	{0x3A}},
	{0x01, 	1,	{0x33}},
	{0x02, 	1,	{0x53}},
	{0x09, 	1,	{0x85}},
	{0x0E, 	1,	{0x25}},
	{0x0F, 	1,	{0x0A}},
	{0x0B, 	1,	{0x97}},
	{0x0C, 	1,	{0x97}},
	{0x11, 	1,	{0x8C}},
	{0x36, 	1,	{0x7B}},
	{0x71, 	1,	{0x2C}},

	{0xFF, 	1,	{0x05}},
	{0x01, 	1,	{0x00}},
	{0x02, 	1,	{0x8D}},
	{0x03, 	1,	{0x8D}},
	{0x04, 	1,	{0x8D}},
	{0x05, 	1,	{0x30}},
	{0x06, 	1,	{0x33}},
	{0x07, 	1,	{0x77}},
	{0x08, 	1,	{0x00}},
	{0x09, 	1,	{0x00}},
	{0x0A, 	1,	{0x00}},
	{0x0B, 	1,	{0x80}},
	{0x0C, 	1,	{0xC8}},
	{0x0D, 	1,	{0x00}},
	{0x0E, 	1,	{0x1B}},
	{0x0F, 	1,	{0x07}},
	{0x10, 	1,	{0x57}},
	{0x11, 	1,	{0x00}},
	{0x12, 	1,	{0x00}},
	{0x13, 	1,	{0x1E}},
	{0x14, 	1,	{0x00}},
	{0x15, 	1,	{0x1A}},
	{0x16, 	1,	{0x05}},
	{0x17, 	1,	{0x00}},
	{0x18, 	1,	{0x1E}},
	{0x19, 	1,	{0xFF}},
	{0x1A, 	1,	{0x00}},
	{0x1B, 	1,	{0xFC}},
	{0x1C, 	1,	{0x80}},
	{0x1D, 	1,	{0x00}},
	{0x1E, 	1,	{0x00}},
	{0x1F, 	1,	{0x77}},
	{0x20, 	1,	{0x00}},
	{0x21, 	1,	{0x00}},
	{0x22, 	1,	{0x55}},
	{0x23, 	1,	{0x0D}},
	{0x31, 	1,	{0xA0}},
	{0x32, 	1,	{0x00}},
	{0x33, 	1,	{0xB8}},
	{0x34, 	1,	{0xBB}},
	{0x35, 	1,	{0x11}},
	{0x36, 	1,	{0x01}},
	{0x37, 	1,	{0x0B}},
	{0x38, 	1,	{0x01}},
	{0x39, 	1,	{0x0B}},
	{0x44, 	1,	{0x08}},
	{0x45, 	1,	{0x80}},
	{0x46, 	1,	{0xCC}},
	{0x47, 	1,	{0x04}},
	{0x48, 	1,	{0x00}},
	{0x49, 	1,	{0x00}},
	{0x4A, 	1,	{0x01}},
	{0x6C, 	1,	{0x03}},
	{0x6D, 	1,	{0x03}},
	{0x6E, 	1,	{0x2F}},
	{0x43, 	1,	{0x00}},
	{0x4B, 	1,	{0x23}},
	{0x4C, 	1,	{0x01}},
	{0x50, 	1,	{0x23}},
	{0x51, 	1,	{0x01}},
	{0x58, 	1,	{0x23}},
	{0x59, 	1,	{0x01}},
	{0x5D, 	1,	{0x23}},
	{0x5E, 	1,	{0x01}},
	//{0x62, 	1,	{0x23}},
	//{0x63, 	1,	{0x01}},
	//{0x67, 	1,	{0x23}},
	//{0x68, 	1,	{0x01}},
	{0x89, 	1,	{0x00}},
	{0x8D, 	1,	{0x01}},
	{0x8E, 	1,	{0x64}},
	{0x8F, 	1,	{0x20}},
	{0x97, 	1,	{0x8E}},
	{0x82, 	1,	{0x8C}},
	{0x83, 	1,	{0x02}},
	{0xBB, 	1,	{0x0A}},
	{0xBC, 	1,	{0x0A}},
	{0x24, 	1,	{0x25}},
	{0x25, 	1,	{0x55}},
	{0x26, 	1,	{0x05}},
	{0x27, 	1,	{0x23}},
	{0x28, 	1,	{0x01}},
	{0x29, 	1,	{0x31}},
	{0x2A, 	1,	{0x5D}},
	{0x2B, 	1,	{0x01}},
	{0x2F, 	1,	{0x00}},
	{0x30, 	1,	{0x10}},
	{0xA7, 	1,	{0x12}},
	{0x2D, 	1,	{0x03}},
	// Skip Gamma, CABC

	{0xFF, 	1,	{0x00}},
	{0xFB, 	1,	{0x01}},

	{0xFF, 	1,	{0x01}},
	{0xFB, 	1,	{0x01}},

	{0xFF, 	1,	{0x02}},
	{0xFB, 	1,	{0x01}},

	{0xFF, 	1,	{0x03}},
	{0xFB, 	1,	{0x01}},

	{0xFF, 	1,	{0x04}},
	{0xFB, 	1,	{0x01}},

	{0xFF, 	1,	{0x05}},
	{0xFB, 	1,	{0x01}},

	{0xFF, 	1,	{0x00}},
	{0x3A,	1,	{0x77}},
	{0x35,	1,	{0x00}},

	// Note
	// Strongly recommend not to set Sleep out / Display On here. That will cause messed frame to be shown as later the backlight is on.


	// Setting ending by predefined flag
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


#if 0
static struct LCM_setting_table lcm_set_window[] = {
	{0x2A,	4,	{0x00, 0x00, (FRAME_WIDTH>>8), (FRAME_WIDTH&0xFF)}},
	{0x2B,	4,	{0x00, 0x00, (FRAME_HEIGHT>>8), (FRAME_HEIGHT&0xFF)}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif

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


// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}


static void lcm_get_params(LCM_PARAMS *params)
{
		unsigned int div2_real=0;
		unsigned int cycle_time = 0;
		unsigned int ui = 0;
		unsigned int hs_trail_m, hs_trail_n;
		#define NS_TO_CYCLE(n, c)	((n) / c + (( (n) % c) ? 1 : 0))

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

		params->dsi.vertical_sync_active=3;
		params->dsi.vertical_backporch=12;
		params->dsi.vertical_frontporch=2;
		params->dsi.vertical_active_line=800;
	
		params->dsi.line_byte=2048;		// 2256 = 752*3
		params->dsi.horizontal_sync_active_byte=26;
		params->dsi.horizontal_backporch_byte=146;
		params->dsi.horizontal_frontporch_byte=146;	
		params->dsi.rgb_byte=(480*3+6);	
	
		params->dsi.horizontal_sync_active_word_count=20;	
		params->dsi.horizontal_backporch_word_count=140;
		params->dsi.horizontal_frontporch_word_count=140;

		// Bit rate calculation
		params->dsi.pll_div1=37;		// fref=26MHz, fvco=fref*(div1+1)	(div1=0~63, fvco=500MHZ~1GHz)
		params->dsi.pll_div2=0;			// div2=0~15: fout=fvo/(2*div2)

#if 0
		div2_real=params->dsi.pll_div2 ? params->dsi.pll_div2*0x02 : 0x1;
		cycle_time = (8 * 1000 * div2_real)/ (26 * (params->dsi.pll_div1+0x01));
		ui = (1000 * div2_real)/ (26 * (params->dsi.pll_div1+0x01)) + 1;
		
		hs_trail_m=params->dsi.LANE_NUM;
		hs_trail_n=NS_TO_CYCLE(((params->dsi.LANE_NUM * 4 * ui) + 60), cycle_time);

		params->dsi.HS_TRAIL	= ((hs_trail_m > hs_trail_n) ? hs_trail_m : hs_trail_n);//min max(n*8*UI, 60ns+n*4UI)
		params->dsi.HS_ZERO 	= NS_TO_CYCLE((105 + 6 * ui), cycle_time);//min 105ns+6*UI
		params->dsi.HS_PRPR 	= NS_TO_CYCLE((40 + 4 * ui), cycle_time);//min 40ns+4*UI; max 85ns+6UI
		// HS_PRPR can't be 1.
		if (params->dsi.HS_PRPR < 2)
			params->dsi.HS_PRPR = 2;

		params->dsi.LPX 		= NS_TO_CYCLE(50, cycle_time);//min 50ns
		
		params->dsi.TA_SACK 	= 1;
		params->dsi.TA_GET		= 5 * NS_TO_CYCLE(50, cycle_time);//5*LPX
		params->dsi.TA_SURE 	= 3 * NS_TO_CYCLE(50, cycle_time) / 2;//min LPX; max 2*LPX;
		params->dsi.TA_GO		= 4 * NS_TO_CYCLE(50, cycle_time);//4*LPX
	
		params->dsi.CLK_TRAIL	= NS_TO_CYCLE(60, cycle_time);//min 60ns
		// CLK_TRAIL can't be 1.
		if (params->dsi.CLK_TRAIL < 2)
			params->dsi.CLK_TRAIL = 2;
		params->dsi.CLK_ZERO	= NS_TO_CYCLE((300-38), cycle_time);//min 300ns-38ns
		params->dsi.LPX_WAIT	= 1;
		params->dsi.CONT_DET	= 0;
		
		params->dsi.CLK_HS_PRPR = NS_TO_CYCLE((38 + 95) / 2, cycle_time);//min 38ns; max 95ns

#endif		

}


static void lcm_init(void)
{
	int i;
	unsigned char buffer[10];
	unsigned int  array[16];
	unsigned int data_array[16];

    SET_RESET_PIN(1);
    SET_RESET_PIN(0);
    MDELAY(1);
    SET_RESET_PIN(1);
    MDELAY(20);

	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);

}


static void lcm_suspend(void)
{
	push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);
}


static void lcm_resume(void)
{
	lcm_init();
	
	push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
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

	dsi_set_cmdq(data_array, 7, 0);

}


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

static unsigned int lcm_esd_check(void)
{
#ifndef BUILD_UBOOT
        if(lcm_esd_test)
        {
            lcm_esd_test = FALSE;
            return TRUE;
        }

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

        if(read_reg(0xB6) == 0x42)
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
    unsigned char para = 0;

    SET_RESET_PIN(1);
    SET_RESET_PIN(0);
    MDELAY(1);
    SET_RESET_PIN(1);
    MDELAY(120);
	  push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
    MDELAY(10);
	  push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
    MDELAY(10);
    dsi_set_cmdq_V2(0x35, 1, &para, 1);     ///enable TE
    MDELAY(10);

    return TRUE;
}

static unsigned int lcm_compare_id(void)
{
	unsigned int id1, id2, id3;
	unsigned char buffer[2];
	unsigned int array[16];

    SET_RESET_PIN(1);
    SET_RESET_PIN(0);
    MDELAY(10);
    SET_RESET_PIN(1);
    MDELAY(10);

	// Set Maximum return byte = 1
	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	id1 = read_reg(0xDA);
	id2 = read_reg(0xDB);
	id2 = read_reg(0xDC);

#if defined(BUILD_UBOOT)
	printf("%s, Module ID = {%x, %x, %x} \n", __func__, id1, id2, id3);
#endif

    return (LCM_ID1 == id1 && LCM_ID2 == id2)?1:0;
}

// ---------------------------------------------------------------------------
//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------
LCM_DRIVER nt35590_hd720_dsi_cmd_drv = 
{
    .name			= "nt35590",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
#if (LCM_DSI_CMD_MODE)
	.update         = lcm_update,
	//.set_backlight	= lcm_setbacklight,
//	.set_pwm        = lcm_setpwm,
//	.get_pwm        = lcm_getpwm,
	//.esd_check   = lcm_esd_check,
    //.esd_recover   = lcm_esd_recover,
	.compare_id    = lcm_compare_id,
#endif
};
