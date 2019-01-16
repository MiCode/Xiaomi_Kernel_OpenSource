#include <linux/string.h>

#include "lcm_drv.h"

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  										(540)
#define FRAME_HEIGHT 										(960)

#define REGFLAG_DELAY             							0XFE
#define REGFLAG_END_OF_TABLE      							0xFF   // END OF REGISTERS MARKER

#define LCM_DSI_CMD_MODE									0

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

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg											lcm_util.dsi_read_reg()
#define read_reg_v2(cmd, buffer, buffer_size)   				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)    

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

	
	{0xF0,	5,	{0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0xB1,	1,	{0xFC}},
	{0xC8,	1,	{0x01}},
	{0xB6,	1,	{0x0A}},
	{0xB7,	6,	{0x00, 0x34, 0x34, 0x10, 0x10, 0x55}},
	{0xB8,	4,	{0x01, 0x02, 0x02, 0x02}},
	{0xBC,	3,	{0x05, 0x05, 0x05}},

    {REGFLAG_DELAY, 10, {}},

	{0xF0,	5,	{0x55, 0xAA, 0x52, 0x08, 0x01}},
	{0xB0,	3,	{0x05, 0x05, 0x05}},
	{0xB1,	3,	{0x05, 0x05, 0x05}},
	{0xB6,	3,	{0x44, 0x44, 0x44}},
	{0xB7,	3,	{0x34, 0x34, 0x34}},
	{0xB3,	3,	{0x10, 0x10, 0x10}},
	{0xB9,	3,	{0x34, 0x34, 0x34}},
	{0xB4,	3,	{0x0A, 0x0A, 0x0A}},
	{0xBA,	3,	{0x24, 0x24, 0x24}},
	{0xBC,	3,	{0x00, 0x88, 0x4E}},
	{0xBD,	3,	{0x00, 0x88, 0x54}},
	{0xBE,	1,	{0x4C}},
	
	{0xD1,	16,	{0x00,0x00,0x00,0x15,0x00,0x3A,0x00,0x59,0x00,0x73,0x00,0x9F,0x00,0xC2,0x00,0xF9}},
	{0xD2,	16,	{0x01,0x24,0x01,0x66,0x01,0x99,0x01,0xE6,0x02,0x22,0x02,0x23,0x02,0x58,0x02,0x8A}},
	{0xD3,	16,	{0x02,0xA5,0x02,0xC7,0x02,0xE0,0x03,0x0B,0x03,0x31,0x03,0x6E,0x03,0x98,0x03,0xC9}},
	{0xD4,	4,	{0x03,0xF2,0x03,0xFF}},

	{0xD5,	16,	{0x00,0x00,0x00,0x15,0x00,0x3A,0x00,0x59,0x00,0x73,0x00,0x9F,0x00,0xC2,0x00,0xF9}},
	{0xD6,	16,	{0x01,0x24,0x01,0x66,0x01,0x99,0x01,0xE6,0x02,0x22,0x02,0x23,0x02,0x58,0x02,0x8A}},
	{0xD7,	16,	{0x02,0xA5,0x02,0xC7,0x02,0xE0,0x03,0x0B,0x03,0x31,0x03,0x6E,0x03,0x98,0x03,0xC9}},
	{0xD8,	4,	{0x03,0xF2,0x03,0xFF}},

	{0xD9,	16,	{0x00,0x00,0x00,0x15,0x00,0x3A,0x00,0x59,0x00,0x73,0x00,0x9F,0x00,0xC2,0x00,0xF9}},
	{0xDD,	16,	{0x01,0x24,0x01,0x66,0x01,0x99,0x01,0xE6,0x02,0x22,0x02,0x23,0x02,0x58,0x02,0x8A}},
	{0xDE,	16,	{0x02,0xA5,0x02,0xC7,0x02,0xE0,0x03,0x0B,0x03,0x31,0x03,0x6E,0x03,0x98,0x03,0xC9}},
	{0xDF,	4,	{0x03,0xF2,0x03,0xFF}},
	//
	{0xE0,	16, {0x00,0x00,0x00,0x15,0x00,0x3A,0x00,0x59,0x00,0x73,0x00,0x9F,0x00,0xC2,0x00,0xF9}},
	{0xE1,	16, {0x01,0x24,0x01,0x66,0x01,0x99,0x01,0xE6,0x02,0x22,0x02,0x23,0x02,0x58,0x02,0x8A}},
	{0xE2,	16, {0x02,0xA5,0x02,0xC7,0x02,0xE0,0x03,0x0B,0x03,0x31,0x03,0x6E,0x03,0x98,0x03,0xC9}},
	{0xE3,	4,	{0x03,0xF2,0x03,0xFF}},

	{0xE4,	16, {0x00,0x00,0x00,0x15,0x00,0x3A,0x00,0x59,0x00,0x73,0x00,0x9F,0x00,0xC2,0x00,0xF9}},
	{0xE5,	16, {0x01,0x24,0x01,0x66,0x01,0x99,0x01,0xE6,0x02,0x22,0x02,0x23,0x02,0x58,0x02,0x8A}},
	{0xE6,	16, {0x02,0xA5,0x02,0xC7,0x02,0xE0,0x03,0x0B,0x03,0x31,0x03,0x6E,0x03,0x98,0x03,0xC9}},
	{0xE7,	4,	{0x03,0xF2,0x03,0xFF}},

	{0xE8,	16, {0x00,0x00,0x00,0x15,0x00,0x3A,0x00,0x59,0x00,0x73,0x00,0x9F,0x00,0xC2,0x00,0xF9}},
	{0xE9,	16, {0x01,0x24,0x01,0x66,0x01,0x99,0x01,0xE6,0x02,0x22,0x02,0x23,0x02,0x58,0x02,0x8A}},
	{0xEA,	16, {0x02,0xA5,0x02,0xC7,0x02,0xE0,0x03,0x0B,0x03,0x31,0x03,0x6E,0x03,0x98,0x03,0xC9}},
	{0xEB,	4,	{0x03,0xF2,0x03,0xFF}},	

	{0x35,	1,	{0x00}},	

	// Note
	// Strongly recommend not to set Sleep out / Display On here. That will cause messed frame to be shown as later the backlight is on.


	// Setting ending by predefined flag
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};



static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
	{0x11, 0, {0x00}},
    {REGFLAG_DELAY, 120, {}},

    // Display ON
	{0x29, 0, {0x00}},
    {REGFLAG_DELAY, 100, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	// Display off sequence
	{0x28, 0, {0x00}},

    // Sleep Mode On
	{0x10, 0, {0x00}},

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
				//UDELAY(5);//soso add or it will fail to send register
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


		params->dsi.mode   = SYNC_EVENT_VDO_MODE;
	
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

		params->dsi.vertical_sync_active				= 2;
		params->dsi.vertical_backporch					= 4;
		params->dsi.vertical_frontporch					= 4;
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 

		params->dsi.horizontal_sync_active				= 8;
		params->dsi.horizontal_backporch				= 64;
		params->dsi.horizontal_frontporch				= 64;
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

		// Bit rate calculation
		params->dsi.pll_div1=37;		// fref=26MHz, fvco=fref*(div1+1)	(div1=0~63, fvco=500MHZ~1GHz)
		params->dsi.pll_div2=1; 		// div2=0~15: fout=fvo/(2*div2)
}

static unsigned int lcm_compare_id(void);

static void lcm_init(void)
{
    SET_RESET_PIN(1);
    SET_RESET_PIN(0);
    MDELAY(6);//Must > 5ms
    SET_RESET_PIN(1);
    MDELAY(50);//Must > 50ms

	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
}


static void lcm_suspend(void)
{
    SET_RESET_PIN(1);
    SET_RESET_PIN(0);
    MDELAY(6);//Must > 5ms
    SET_RESET_PIN(1);
    MDELAY(50);//Must > 50ms

	push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);
}


static void lcm_resume(void)
{
	lcm_init();
	
	push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
}
static unsigned int lcm_compare_id(void)
{
	unsigned int id = 0;
	unsigned char buffer[3];
	unsigned int array[16];
	
	SET_RESET_PIN(1);  //NOTE:should reset LCM firstly
	SET_RESET_PIN(0);
	MDELAY(6);
	SET_RESET_PIN(1);
	MDELAY(50);

	array[0] = 0x00033700;// read id return two byte,version and id
	dsi_set_cmdq(array, 1, 1);
	read_reg_v2(0x04, buffer, 3);
	id = buffer[0]; //we only need ID
#if defined(BUILD_UBOOT)
	printf("\n\n\n\n[soso]%s, id1 = 0x%08x\n", __func__, id);
#endif
    return (id == 0x11)?1:0;
}


LCM_DRIVER rm68190_dsi_vdo_lcm_drv = 
{
    .name			= "rm68190_dsi_vdo_lcm_drv",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id    = lcm_compare_id,
};

