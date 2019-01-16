#include <linux/string.h>

#include "lcm_drv.h"

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  (320)
#define FRAME_HEIGHT (480)

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg											lcm_util.dsi_read_reg()
       

#define LCM_DSI_CMD_MODE


#define REGFLAG_DELAY             0XFE
#define REGFLAG_END_OF_TABLE      0xFF   // END OF REGISTERS MARKER


struct LCM_setting_table {
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[64];
};


static struct LCM_setting_table initialization_setting[] = {

	{0xB9,	3,	{0xFF, 0x83, 0x69}},
	{REGFLAG_DELAY, 10, {}},

	{0xB2,	15,	{0x00, 0x20, 0x03, 0x03,
				 0x70, 0x00, 0xFF, 0x00,
				 0x00, 0x00, 0x00, 0x03,
				 0x03, 0x00, 0x01}},
	{REGFLAG_DELAY, 10, {}},


	{0xB4, 	5,	{0x00, 0x08, 0x70, 0x0E,
				 0x06}},
	{REGFLAG_DELAY, 10, {}},

	{0xD5,	26, {0x00, 0x01, 0x03, 0x00,
				 0x01, 0x02, 0x08, 0x80,
				 0x11, 0x13, 0x00, 0x00,
				 0x40, 0x06, 0x51, 0x07,
				 0x00, 0x00, 0x71, 0x05,
				 0x60, 0x04, 0x07, 0x0F,
				 0x06, 0x00}},
	{REGFLAG_DELAY, 10, {}},

	{0xB1,	19,	{0x85, 0x00, 0x34, 0x06,
				 0x00, 0x0E, 0x0E, 0x24,
				 0x2C, 0x1A, 0x1A, 0x07,
				 0x3A, 0x01, 0xE6, 0xE6,
				 0xE6, 0xE6, 0xE6}},
	{REGFLAG_DELAY, 10, {}},


	{0x3A,	1,	{0x07}},
	{0xCC,	1,	{0x02}},

	{0xB6,	2,	{0x6C, 0x6C}},
	{REGFLAG_DELAY, 10, {}},

	// ENABLE FMARK
	{0x35,	1,	{0x00}},

	// SET GAMMA
	{0xE0,	34,	{0x00, 0x0C, 0x14, 0x3F,
				 0x3F, 0x3F, 0x29, 0x54,
				 0x06, 0x0C, 0x0F, 0x13,
				 0x15, 0x13, 0x15, 0x14,
				 0x1F, 0x00, 0x0C, 0x14,
				 0x3F, 0x3F, 0x3F, 0x29,
				 0x54, 0x06, 0x0C, 0x0F,
				 0x13, 0x15, 0x13, 0x15,
				 0x14, 0x1F}},
	{REGFLAG_DELAY, 10, {}},

	{0xBA,	13,	{0x00, 0xA0, 0xC6, 0x00,
				 0x0A, 0x00, 0x10, 0x30,
				 0x6F, 0x02, 0x11, 0x18,
				 0x40}},
	{REGFLAG_DELAY, 10, {}},

	{0x51,	1,	{0x00}},
	{REGFLAG_DELAY, 50, {}},

	{0x53,	1,	{0x24}},
	{REGFLAG_DELAY, 50, {}},

	{0x55,	1,	{0x01}},
	{REGFLAG_DELAY, 50, {}},

	{0x5E,	1,	{0x70}},
	{REGFLAG_DELAY, 50, {}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_display_on[] = {
    // Sleep Out
	{0x11,	0,	{}},
    {REGFLAG_DELAY, 100, {}},

    // Display ON
	{0x29,	0,	{}},
    {REGFLAG_DELAY, 10, {0}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_display_off[] = {
    // Display Off
	{0x28,	0,	{}},
    {REGFLAG_DELAY, 10, {0}},

	// Sleep In
	{0x10,	0,	{}},
	{REGFLAG_DELAY, 100, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_set_window[] = {
	{0x2A,	4,	{0x00, 0x00, (FRAME_WIDTH>>8), (FRAME_WIDTH&0xFF)}},
	{0x2B,	4,	{0x00, 0x00, (FRAME_HEIGHT>>8), (FRAME_HEIGHT&0xFF)}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table mddi_lgd_display_on[] = {
    // Sleep Out
	{0x11, 4, {0x00, 0x00, 0x00, 0x00}},
    {REGFLAG_DELAY, 120, {}},

    // Display ON
	{0x29, 4, {0x00, 0x00, 0x00, 0x00}},
    {REGFLAG_DELAY, 40, {0}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mddi_lgd_deep_sleep_mode_in_data[] = {
	// Display off sequence
	{0x28, 1, {0x00}},
	{REGFLAG_DELAY, 40, {}},

    // Sleep Mode On
	{0x10, 1, {0x00}},
	{REGFLAG_DELAY, 40, {}},
#if 0
    // MCAP
	{0xb0, 1, {0x00}},
	{REGFLAG_DELAY, 40, {}},


    // Low Power Mode Control
	{0xb1, 1, {0x01}},
#endif
	{REGFLAG_DELAY, 100, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


// LGD 1st
static struct LCM_setting_table mddi_lgd_initialize[] = {
	// MCAP
	{0xb0, 1, {0x04}},
	{REGFLAG_DELAY, 120, {}},

	// Set Tear On
	{0x35, 1, {0x00}},
	{REGFLAG_DELAY, 120, {}},


	// Set Address Mode
	{0x36, 1, {0x08}},
	{REGFLAG_DELAY, 120, {}},


	// Set Pixel Format
	//{0x3a, 1, {0x55}},    //0x55 : 16bit, 0x66 : 18bit, 0x77 : 24bit
	{0x3a, 1, {0x77}},    //0x55 : 16bit, 0x66 : 18bit, 0x77 : 24bit
	{REGFLAG_DELAY, 120, {}},


	// Set Column Address
	{0x2a, 4, {0x00, 0x00, 0x01, 0x3f}},
	{REGFLAG_DELAY, 120, {}},


	// Set Page Address
	{0x2b, 4, {0x00, 0x00, 0x01, 0xdf}},
	{REGFLAG_DELAY, 120, {}},


	// Frame Memory Access and Interface Setting
	{0xb3, 4, {0x02, 0x00, 0x00, 0x00}},
	{REGFLAG_DELAY, 120, {}},


	// Panel Driving Setting
//	{0xc0, 8, {0x01, 0xdf, 0x40, 0x13, 0x00, 0x01, 0x00, 0x33}},
	{0xc0, 8, {0x01, 0xdf, 0x40, 0x10, 0x00, 0x01, 0x00, 0x33}},	// rev 0.4
	{REGFLAG_DELAY, 120, {}},

	
	// Display Timing Setting for Normal Mode
//	{0xc1, 5, {0x07, 0x27, 0x08, 0x08, 0x50}},
	{0xc1, 5, {0x07, 0x27, 0x08, 0x08, 0x10}},	// rev 0.4
	{REGFLAG_DELAY, 120, {}},

//  1-line inversion이 자연스럽군.
//	{0xc1, 5, {0x07, 0x27, 0x08, 0x08, 0x00}},

	// Source/Gate Driving Timing Setting
//	{0xc4, 4, {0x44, 0x00, 0x03, 0x03}},
	{0xc4, 4, {0x77, 0x00, 0x03, 0x01}},
	{REGFLAG_DELAY, 120, {}},


	// DPI Polarity Control
	{0xc6, 1, {0x00}},
	{REGFLAG_DELAY, 120, {}},

	
	// Gamma Setting A Set
// V01
//	{0xc8,24,{0x00,0x11,0x22,0x2b,0x37,0x51,0x32,0x1d,
//                0x11,0x0d,0x0a,0x00,0x00,0x12,0x20,0x26,
//                0x33,0x49,0x40,0x2f,0x25,0x21,0x1e,0x15}},
// V02
//	{0xc8,24,{0x00,0x12,0x16,0x1E,0x2C,0x46,0x36,0x1D,
//		0x0E,0x07,0x02,0x00,0x00,0x12,0x16,0x1E,
//		0x2C,0x46,0x36,0x1D,0x0E,0x07,0x02,0x00}},
// V03
//	{0xc8,24,{0x00,0x07,0x1F,0x23,0x30,0x48,0x37,0x25,
//		0x1C,0x16,0x10,0x00,0x00,0x07,0x1F,0x23,
//		0x30,0x48,0x37,0x25,0x1C,0x16,0x10,0x00}},
// rev 0.4
	{0xc8,24,{0x00,0x04,0x11,0x1C,0x2E,0x46,0x39,0x21,
		0x15,0x0A,0x05,0x00,0x00,0x04,0x11,0x1C,
		0x2E,0x46,0x39,0x21,0x15,0x0A,0x05,0x00}},

	{REGFLAG_DELAY, 120, {}},

	
	// Gamma Setting B Set
// V01
//	{0xc9,24,{0x00,0x11,0x22,0x2b,0x37,0x51,0x32,0x1d,
//                0x11,0x0d,0x0a,0x00,0x00,0x12,0x20,0x26,
//                0x33,0x49,0x40,0x2f,0x25,0x21,0x1e,0x15}},
// V02
//	{0xc9,24,{0x00,0x12,0x16,0x1E,0x2C,0x46,0x36,0x1D,
//		0x0E,0x07,0x02,0x00,0x00,0x12,0x16,0x1E,
//		0x2C,0x46,0x36,0x1D,0x0E,0x07,0x02,0x00}},
// V03
//	{0xc9,24,{0x00,0x07,0x1F,0x23,0x30,0x48,0x37,0x25,
//		0x1C,0x16,0x10,0x00,0x00,0x07,0x1F,0x23,
//		0x30,0x48,0x37,0x25,0x1C,0x16,0x10,0x00}},
// rev 0.4
	{0xc9,24,{0x00,0x04,0x11,0x1C,0x2E,0x46,0x39,0x21,
		0x15,0x0A,0x05,0x00,0x00,0x04,0x11,0x1C,
		0x2E,0x46,0x39,0x21,0x15,0x0A,0x05,0x00}},
	{REGFLAG_DELAY, 120, {}},

	
	// Gamma Setting C Set
// V01
//	{0xca,24,{0x00,0x11,0x22,0x2b,0x37,0x51,0x32,0x1d,
//                0x11,0x0d,0x0a,0x00,0x00,0x12,0x20,0x26,
//                0x33,0x49,0x40,0x2f,0x25,0x21,0x1e,0x15}},
// V02
//	{0xca,24,{0x00,0x12,0x16,0x1E,0x2C,0x46,0x36,0x1D,
//		0x0E,0x07,0x02,0x00,0x00,0x12,0x16,0x1E,
//		0x2C,0x46,0x36,0x1D,0x0E,0x07,0x02,0x00}},
// V03
//	{0xca,24,{0x00,0x07,0x1F,0x23,0x30,0x48,0x37,0x25,
//		0x1C,0x16,0x10,0x00,0x00,0x07,0x1F,0x23,
//		0x30,0x48,0x37,0x25,0x1C,0x16,0x10,0x00}},
// rev 0.4
	{0xca,24,{0x00,0x04,0x11,0x1C,0x2E,0x46,0x39,0x21,
		0x15,0x0A,0x05,0x00,0x00,0x04,0x11,0x1C,
		0x2E,0x46,0x39,0x21,0x15,0x0A,0x05,0x00}},

	{REGFLAG_DELAY, 120, {}},

	
	// Power Setting (Charge Pump Setting)
//	{0xd0, 16, {0x95, 0x06, 0x08, 0x20, 0x31, 0x04, 0x01, 0x00,
//                0x08, 0x01, 0x00, 0x06, 0x01, 0x00, 0x00, 0x20}},
//	{0xd0, 16, {0x95, 0x0E, 0x08, 0x20, 0x31, 0x04, 0x01, 0x00,
//                0x08, 0x01, 0x00, 0x06, 0x01, 0x00, 0x00, 0x20}},
	{0xd0, 16, {0x95, 0x06, 0x08, 0x20, 0x31, 0x04, 0x01, 0x00,
                0x08, 0x01, 0x00, 0x06, 0x01, 0x00, 0x00, 0x20}},	// rev 0.4

	{REGFLAG_DELAY, 120, {}},


	// VCOM Setting
//	{0xd1, 4, {0x02, 0x1f, 0x1f, 0x5e}},
	{0xd1, 4, {0x02, 0x1f, 0x1f, 0x38}},
	{REGFLAG_DELAY, 120, {}},


	// Backlight Control(1)
	{0xb8, 20, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00}},
	{REGFLAG_DELAY, 120, {}},

	
	// Backlight Control(2)
	{0xb9, 4, {0x01, 0x00, 0xFF, 0x10}},
	{REGFLAG_DELAY, 120, {}},

	
	// NVM Access Control
	{0xe0, 4, {0x00, 0x00, 0x00, 0x00}},
	{REGFLAG_DELAY, 120, {}},


	// Set DDB Write Control
	{0xe1, 6, {0x00, 0x00, 0x00, 0x00, 0x00, 0x70}},
	{REGFLAG_DELAY, 120, {}},


	// NVM Load Control
	{0xe2, 1, {0x80}},
	{REGFLAG_DELAY, 120, {}},


	// Write Memory Start
	//{0x2C, 1, {0x00}},

	{REGFLAG_END_OF_TABLE, 0x00, {0}}
}; 


static struct LCM_setting_table mddi_lgd_backlight_enable[] = {

	// Backlight Control(2)
	{0xb9, 4, {0x01, 0xFF, 0xFF, 0x18}},
	{REGFLAG_DELAY, 40, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {0}}

};


static struct LCM_setting_table mddi_lgd_backlight_disable[] = {

	// Backlight Control(2)
	{0xb9, 4, {0x01, 0x00, 0xFF, 0x10}},
	{REGFLAG_DELAY, 40, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {0}}

};


void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
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


static void init_lcm_registers(void)
{
	unsigned int data_array[16];

#if defined(LCM_DSI_CMD_MODE)
	{
		//push_table(initialization_setting, sizeof(initialization_setting) / sizeof(struct LCM_setting_table), 1);
		//push_table(lcm_display_on, sizeof(lcm_display_on) / sizeof(struct LCM_setting_table), 1);
		//push_table(lcm_set_window, sizeof(lcm_set_window) / sizeof(struct LCM_setting_table), 1);
		push_table(mddi_lgd_initialize, sizeof(mddi_lgd_initialize) / sizeof(struct LCM_setting_table), 1);
		//push_table(mddi_lgd_display_on, sizeof(mddi_lgd_display_on) / sizeof(struct LCM_setting_table), 1);
	}
#else
	{
		data_array[0] = 0x00043902; // SET password
		data_array[1] = 0x6983FFB9; //
		dsi_set_cmdq(&data_array, 2, 1);
		MDELAY(10);

		data_array[0] = 0x00143902; //// SET Power
		data_array[1] = 0x340085B1; //
		data_array[2] = 0x0F0F0007; //
		data_array[3] = 0x3F3F322A; //
		data_array[4] = 0xE6013A01; //
		data_array[5] = 0xE6E6E6E6;
		dsi_set_cmdq(&data_array, 6, 1);
		MDELAY(10);

		data_array[0] = 0x00103902; //// SET Display 480x800
		data_array[1] = 0x032300B2; //
		data_array[2] = 0xFF007003; //
		data_array[3] = 0x00000000; //
		data_array[4] = 0x01000303; //
		dsi_set_cmdq(&data_array, 5, 1);
		MDELAY(10);

		data_array[0] = 0x00063902; // SET Display
		data_array[1] = 0x801800B4;
		data_array[2] = 0x00000206;
		dsi_set_cmdq(&data_array, 3, 1);
		MDELAY(10);

		data_array[0] = 0x00033902; //// SET VCOM
		data_array[1] = 0x004242B6; 
		dsi_set_cmdq(&data_array, 2, 1);
		MDELAY(10);

		data_array[0] = 0x001B3902; //// SET GIP
		data_array[1] = 0x030400D5; 
		data_array[2] = 0x28050100; 
		data_array[3] = 0x00030170; 
		data_array[4] = 0x51064000; 
		data_array[5] = 0x41000007;
		data_array[6] = 0x07075006;
		data_array[7] = 0x0000040F;
		dsi_set_cmdq(&data_array, 8, 1);
		MDELAY(10);

		data_array[0] = 0x00233902; //// SET GAMMA
		data_array[1] = 0x191300E0; //
		data_array[2] = 0x283F3D38; //
		data_array[3] = 0x0E0D0746; //
		data_array[4] = 0x14121512; //
		data_array[5] = 0x1300170F;
		data_array[6] = 0x3F3D3819;
		data_array[7] = 0x0D074628;
		data_array[8] = 0x1215120E;
		data_array[9] = 0x00170F14;
		dsi_set_cmdq(&data_array, 10, 1);
		MDELAY(10);

#if 0
		switch(g_ColorFormat)
		{
			case PACKED_RGB565:
				data_array[0] = 0x553A1500; // SET pixel format
			break;
			
			case LOOSED_RGB666:
				data_array[0] = 0x663A1500; // SET pixel format
			break; 
			
			case PACKED_RGB888:
				data_array[0] = 0x773A1500; // SET pixel format
			break;
			
			case PACKED_RGB666:
				data_array[0] = 0x663A1500; // SET pixel format
			break;   		

			default:
				//dbg_print("Format setting error \n\r");
				while(1);
			break;
		}

		dsi_set_cmdq(&data_array, 1, 1);
		MDELAY(100);
#endif
		data_array[0] = 0x000E3902; // SET MIPI (1 or 2 Lane)
		data_array[1] = 0xC6A000BA; //
		data_array[2] = 0x10000A00; //
		if (0)//(g_LaneNumber==1)
		{
			data_array[3] = 0x10026F30;
		}
		else
		{
			data_array[3] = 0x11026F30;
		}
		data_array[4] = 0x00004018;
		dsi_set_cmdq(&data_array, 5, 1);
		MDELAY(10);

		data_array[0] = 0x00110500;		// Sleep Out
		dsi_set_cmdq(&data_array, 1, 1);
		MDELAY(10);

		data_array[0] = 0x00290500;		// Display On
		dsi_set_cmdq(&data_array, 1, 1);
		MDELAY(10);
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
#if defined(LCM_DSI_CMD_MODE)
		params->dsi.mode   = CMD_MODE;
#else
		params->dsi.mode   = SYNC_EVENT_VDO_MODE;
#endif	
		params->ctrl   = LCM_CTRL_PARALLEL_DBI;
		//params->ctrl   = LCM_CTRL_NONE;
		params->width  = FRAME_WIDTH;
		params->height = FRAME_HEIGHT;
	
		// DBI
		//params->dbi.port                    = 0;
		params->dbi.clock_freq              = LCM_DBI_CLOCK_FREQ_104M;
		params->dbi.data_width              = LCM_DBI_DATA_WIDTH_16BITS;
		params->dbi.data_format.color_order = LCM_COLOR_ORDER_RGB;
		params->dbi.data_format.trans_seq   = LCM_DBI_TRANS_SEQ_MSB_FIRST;
		params->dbi.data_format.padding     = LCM_DBI_PADDING_ON_LSB;
		params->dbi.data_format.format      = LCM_DBI_FORMAT_RGB888;
		params->dbi.data_format.width       = LCM_DBI_DATA_WIDTH_24BITS;
	  params->dbi.cpu_write_bits          = LCM_DBI_CPU_WRITE_16_BITS;
	  params->dbi.io_driving_current      = 0;

		// enable tearing-free
		params->dbi.te_mode 				= LCM_DBI_TE_MODE_VSYNC_ONLY;
		//params->dbi.te_mode 				= LCM_DBI_TE_MODE_DISABLED;
		params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;
	
		// DPI
		params->dpi.format			  = LCM_DPI_FORMAT_RGB888;
		params->dpi.rgb_order		  = LCM_COLOR_ORDER_RGB;
		params->dpi.intermediat_buffer_num = 2;
	
		// DSI
		params->dsi.DSI_WMEM_CONTI=0x3C;
		params->dsi.DSI_RMEM_CONTI=0x3E;
		//params->dsi.LANE_NUM=LCM_ONE_LANE;
		params->dsi.LANE_NUM=LCM_ONE_LANE;
		params->dsi.VC_NUM=0x0;
		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
		params->dsi.word_count=480*3;
	
		params->dsi.vertical_sync_active=2;
		params->dsi.vertical_backporch=2;
		params->dsi.vertical_frontporch=2;
		params->dsi.vertical_active_line=800;
	
		params->dsi.line_byte=2180;		// 2256 = 752*3
		params->dsi.horizontal_sync_active_byte=26;
		params->dsi.horizontal_backporch_byte=206;
		params->dsi.horizontal_frontporch_byte=206;	
		params->dsi.rgb_byte=(480*3+6);	
	
		params->dsi.horizontal_sync_active_word_count=20;	
		params->dsi.horizontal_backporch_word_count=200;
		params->dsi.horizontal_frontporch_word_count=200;
	
		params->dsi.HS_TRAIL=0x14;
		params->dsi.HS_ZERO=0x14;
		params->dsi.HS_PRPR=0x0A;
		params->dsi.LPX=0x05;
	
		params->dsi.TA_SACK=0x01;
		params->dsi.TA_GET=0x37;
		params->dsi.TA_SURE=0x16;	
		params->dsi.TA_GO=0x10;
	
		params->dsi.CLK_TRAIL=0x14;
		params->dsi.CLK_ZERO=0x14;	
		params->dsi.LPX_WAIT=0x0A;
		params->dsi.CONT_DET=0x00;
	
		params->dsi.CLK_HS_PRPR=0x0A;
	
		params->dsi.pll_div1=24;		// fref=26MHz, fvco=fref*(div1+1)	(div1=0~63, fvco=500MHZ~1GHz)
	
		//params->dsi.pll_div2=4;			// div2=0~15: fout=fvo/(2*div2)
		params->dsi.pll_div2=1;			// div2=0~15: fout=fvo/(2*div2)

}


static void lcm_init(void)
{
    SET_RESET_PIN(0);
    MDELAY(25);
    SET_RESET_PIN(1);
    MDELAY(50);

    init_lcm_registers();
/*
	{
		unsigned int data_array[16];
		unsigned int divider=4;
		data_array[0]=0x000A3902; // Display Off
		data_array[1]=0x00003EC9;
		data_array[2]=0x1E0e0001|((0xE|(divider<<4))<<8);
		data_array[3]=0x001E;
		dsi_set_cmdq(&data_array, 4, 1);

	}
*/
}


static void lcm_suspend(void)
{
	//push_table(lcm_display_off, sizeof(lcm_display_off) / sizeof(struct LCM_setting_table), 1);
	push_table(mddi_lgd_deep_sleep_mode_in_data, sizeof(mddi_lgd_deep_sleep_mode_in_data) / sizeof(struct LCM_setting_table), 1);
}


static void lcm_resume(void)
{
	//push_table(lcm_display_on, sizeof(lcm_display_on) / sizeof(struct LCM_setting_table), 1);
	push_table(mddi_lgd_display_on, sizeof(mddi_lgd_display_on) / sizeof(struct LCM_setting_table), 1);
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
	//data_array[6]= 0x002c3901;

	dsi_set_cmdq(&data_array, 7, 0);

	//push_table(lcm_set_window, sizeof(lcm_set_window) / sizeof(struct LCM_setting_table), 0);

}


void lcm_setbacklight(unsigned int level)
{
	unsigned int default_level = 147;
	unsigned int mapped_level = 0;
	unsigned int data_array[16];

	//for LGE backlight IC mapping table
	if(level > 255) 
		level = 255;

	if(level >0) 
	{
		mapped_level = default_level+(level)*(255-default_level)/(255);
	}
	else
	{
		mapped_level=0;
	}

	mddi_lgd_backlight_enable[0].para_list[1] = mapped_level;

	if (level > 0)
		push_table(mddi_lgd_backlight_enable, sizeof(mddi_lgd_backlight_enable) / sizeof(struct LCM_setting_table), 1);
	else
		push_table(mddi_lgd_backlight_disable, sizeof(mddi_lgd_backlight_disable) / sizeof(struct LCM_setting_table), 1);				
}


static unsigned int lcm_compare_id(void)
{
    unsigned int id = 0;

	return 1;
}


void lcm_setpwm(unsigned int divider)
{
	unsigned int data_array[16];
	
	data_array[0]= 0x000A3902;
	data_array[1]= 0x00003EC9;
	data_array[2]= 0x1E020001|((0xF|(divider<<4))<<8);
	data_array[3]= 0x001E;
	
	dsi_set_cmdq(&data_array, 4, 1);
}

unsigned int lcm_getpwm(unsigned int divider)
{
	//	ref freq = 15MHz, B0h setting 0x80, so 80.6% * freq is pwm_clk;
	//  pwm_clk / 255 / 2(lcm_setpwm() 6th params) = pwm_duration = 23706
	unsigned int pwm_clk = 23706 / (1<<divider);	
	return pwm_clk;
}


LCM_DRIVER r81592_hvga_dsi_cmd_drv = 
{
    .name			= "lge73_hdk",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
    .set_backlight	= lcm_setbacklight
	//.set_pwm       	= lcm_setpwm,
	//.get_pwm        	= lcm_getpwm        
	//.compare_id     	= lcm_compare_id
#if defined(LCM_DSI_CMD_MODE)
        ,
        .update         = lcm_update
#endif
};
