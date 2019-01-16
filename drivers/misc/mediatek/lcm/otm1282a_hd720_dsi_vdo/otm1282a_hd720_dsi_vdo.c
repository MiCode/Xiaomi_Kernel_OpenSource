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

#define LCM_ID       (0x69)
#define REGFLAG_DELAY             							0XFE
#define REGFLAG_END_OF_TABLE      							0XFD   // END OF REGISTERS MARKER

#define LCM_ID_OTM1282A (0x90)


#if 0
#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif

static unsigned int lcm_esd_test = FALSE;      ///only for ESD test
#endif

#define LCM_DSI_CMD_MODE									0

#define FRAME_WIDTH  										(720)
#define FRAME_HEIGHT 										(1280)

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

static struct LCM_setting_table lcm_vdo_initialization_setting[] = {
	{0xff, 3,  {0x12,0x82,0x01}},
	{0x00, 1,  {0x80}}, 
	{0xff, 2,  {0x12,0x82}},
	{0x00, 1,  {0x00}},
	{0x1C, 1,  {0x02}},
	{0x00, 1,  {0x80}},
	{0xa4, 1,  {0xe8}},
	{0x00, 1,  {0x80}},
	{0xc0, 14, {0x00, 0x60, 0x00, 0x02, 0x04, 0x00, 0x60, 0x02, 0x04, 0x00, 0x60, 0x00, 0x02, 0x04}},
	{0x00, 1,  {0xc0}},
	{0xc5, 2,  {0x12, 0xfa}},
	{0x00, 1,  {0x80}},
	{0xa5, 3,  {0x0c, 0x44, 0x12}},	
	{0x00, 1,  {0xa0}},
	{0xc1, 3,  {0xe0, 0xe0, 0x10}},

	// Sleep Out
	{0x11, 1, {0x00}},
	{REGFLAG_DELAY, 120, {}},
	
	// Display ON
	{0x29, 1, {0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

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
#endif

#if 1
// 120 hz
static unsigned char od_table_33x33[] =
{  
0,11,21,31,44,57,71,86,99,111,122,132,141,151,160,168,175,183,190,197,203,209,216,222,228,234,239,245,251,255,255,255,255,
0,8,18,28,41,54,68,81,95,107,118,128,139,148,156,164,173,181,188,194,202,208,214,221,227,233,238,244,250,255,255,255,255,
0,5,16,25,38,50,65,77,90,103,115,125,136,145,153,162,171,178,186,192,200,206,213,219,226,232,237,243,249,255,255,255,255,
0,2,13,24,35,47,61,73,86,100,111,122,133,142,151,159,168,176,184,190,198,205,211,218,224,231,237,243,249,255,255,255,255,
0,0,10,21,32,44,56,69,83,96,107,119,130,139,148,157,166,174,182,190,196,203,210,217,223,229,236,242,248,254,255,255,255,
0,0,7,18,29,40,52,65,78,91,103,115,125,135,145,154,162,171,179,187,193,201,208,215,221,228,235,241,247,254,255,255,255,
0,0,4,15,25,36,48,60,73,86,98,110,120,131,142,151,159,168,176,185,191,199,206,213,219,227,234,240,246,253,255,255,255,
0,0,1,12,22,33,44,56,68,81,93,105,116,128,138,148,156,166,174,182,189,197,205,212,218,226,232,239,246,252,255,255,255,
0,0,0,9,19,29,41,52,64,76,88,101,113,125,135,144,154,163,171,179,188,195,203,210,217,224,231,238,245,252,255,255,255,
0,0,0,7,16,26,38,48,60,72,84,96,108,120,130,141,151,159,169,177,186,193,201,209,216,223,230,237,244,251,255,255,255,
0,0,0,4,14,24,35,45,56,67,80,92,104,116,126,138,148,156,166,175,184,191,200,207,214,221,229,236,243,250,255,255,255,
0,0,0,2,12,21,32,41,52,63,76,88,100,112,123,134,144,153,164,173,182,189,198,206,213,220,228,235,242,249,255,255,255,
0,0,0,0,9,18,28,38,49,59,71,84,96,108,119,130,141,151,161,170,179,188,196,204,212,219,227,234,241,249,255,255,255,
0,0,0,0,7,16,25,36,45,56,68,79,92,104,116,127,138,148,157,168,177,186,194,203,211,219,226,234,241,248,255,255,255,
0,0,0,0,4,13,23,33,42,52,64,75,87,100,112,124,135,145,155,166,175,184,192,202,210,218,226,233,240,247,254,255,255,
0,0,0,0,1,11,20,30,39,49,60,70,83,95,108,120,131,142,152,163,173,182,190,200,209,218,226,233,240,247,254,255,255,
0,0,0,0,0,8,18,27,37,46,55,66,78,91,103,116,128,139,150,161,171,180,190,199,208,217,226,232,239,246,253,255,255,
0,0,0,0,0,6,15,24,34,43,52,63,75,86,99,111,124,136,147,158,168,178,188,197,207,216,225,232,239,246,253,255,255,
0,0,0,0,0,4,12,21,31,39,49,60,71,82,95,107,119,132,144,156,166,176,186,196,205,215,224,231,238,245,252,255,255,
0,0,0,0,0,2,10,18,28,36,46,56,67,78,90,103,115,128,140,152,163,173,184,194,204,214,224,231,238,245,252,255,255,
0,0,0,0,0,0,8,16,24,33,42,52,62,74,86,98,111,123,136,148,160,171,181,192,202,212,222,230,237,244,251,255,255,
0,0,0,0,0,0,5,13,21,31,40,48,58,70,82,94,106,119,131,144,156,168,179,189,200,210,219,229,236,243,251,255,255,
0,0,0,0,0,0,3,11,19,28,36,45,54,66,77,89,102,115,127,140,152,165,176,187,197,208,216,227,235,242,250,255,255,
0,0,0,0,0,0,1,8,16,25,33,41,51,61,72,85,98,111,124,136,149,161,172,184,195,205,214,225,233,241,249,255,255,
0,0,0,0,0,0,0,6,13,21,28,37,47,56,68,80,94,107,120,132,145,157,169,180,192,203,213,224,232,240,248,255,255,
0,0,0,0,0,0,0,3,11,18,25,35,44,52,64,76,88,101,115,127,141,153,165,176,187,200,211,222,230,238,246,255,255,
0,0,0,0,0,0,0,1,8,15,22,32,40,48,59,71,83,96,110,122,136,149,161,172,183,193,208,220,228,236,245,253,255,
0,0,0,0,0,0,0,0,5,12,19,28,35,44,55,65,77,90,104,118,132,145,157,169,179,189,202,216,226,235,243,252,255,
0,0,0,0,0,0,0,0,2,9,16,23,30,40,50,59,72,85,99,113,127,140,154,165,175,185,196,210,224,233,242,250,255,
0,0,0,0,0,0,0,0,0,5,12,19,26,36,45,54,66,78,91,105,120,134,148,159,169,183,194,208,221,232,241,250,255,
0,0,0,0,0,0,0,0,0,1,8,15,22,31,39,48,60,71,84,98,112,127,142,153,165,180,192,205,218,230,240,249,255,
0,0,0,0,0,0,0,0,0,0,4,11,18,26,34,42,53,64,77,91,105,120,135,148,161,177,190,203,216,227,238,248,255,
0,0,0,0,0,0,0,0,0,0,1,8,15,21,28,37,48,58,70,84,99,114,130,144,158,173,188,201,213,225,235,246,255,
};
#else
// 60 hz
static unsigned char od_table_33x33[] =
{  
0,9,18,26,36,46,56,66,76,86,95,105,114,123,132,140,148,155,163,170,177,185,192,200,208,216,223,230,236,242,248,254,255,
0,8,17,26,35,45,55,65,75,84,94,104,113,122,131,139,147,155,162,169,177,184,192,199,207,215,222,229,236,242,248,254,255,
0,8,16,25,34,44,53,63,73,83,92,103,112,121,129,137,146,154,161,169,176,183,191,198,207,215,220,229,235,241,248,254,255,
0,7,16,24,33,42,52,61,71,82,91,101,110,119,128,136,144,153,161,168,175,182,190,198,206,214,220,229,235,241,247,254,255,
0,7,15,24,32,41,50,59,69,80,91,100,109,118,127,135,143,152,160,167,174,182,189,197,205,213,221,228,234,241,247,253,255,
0,7,15,23,31,40,49,58,68,78,89,98,107,116,125,134,142,150,158,166,173,181,188,196,204,212,221,228,234,240,247,253,255,
0,6,14,22,29,39,48,57,67,77,87,96,106,115,123,132,140,149,156,165,172,180,187,195,203,212,220,227,234,240,247,253,255,
0,6,13,21,28,37,47,56,65,75,85,95,104,113,122,131,139,147,155,164,171,179,186,194,202,211,219,227,233,240,246,253,255,
0,5,13,20,27,36,45,55,64,74,84,94,103,111,120,129,138,146,155,163,170,178,185,193,202,210,219,226,233,239,246,253,255,
0,4,11,19,26,35,44,53,63,72,82,92,101,110,119,127,136,145,154,162,169,177,185,193,201,209,218,226,232,239,246,252,255,
0,3,10,18,25,33,43,52,61,71,80,90,99,108,117,126,135,144,152,161,169,176,184,192,200,209,217,225,232,239,246,252,255,
0,1,9,17,24,32,42,51,60,69,79,88,98,107,116,125,134,143,151,160,168,176,184,191,200,208,216,224,231,238,245,252,255,
0,0,8,16,24,32,40,49,58,68,77,86,96,105,115,124,133,142,150,159,167,175,183,191,199,207,216,224,231,238,245,252,255,
0,0,7,15,23,32,40,49,57,67,76,85,94,104,113,123,132,140,149,157,166,174,182,190,199,207,215,222,230,238,245,252,255,
0,0,6,15,23,31,39,48,56,66,74,83,93,102,112,121,130,139,148,156,165,174,182,190,198,206,214,222,230,237,245,252,255,
0,0,6,14,22,31,39,47,55,64,73,82,91,100,110,120,129,138,147,155,164,173,181,189,198,206,214,221,229,237,244,252,255,
0,0,5,13,22,30,38,46,54,62,71,80,89,99,108,118,128,137,146,155,164,172,180,189,197,205,213,221,229,236,244,251,255,
0,0,4,13,21,29,37,45,53,61,69,79,88,97,107,117,126,136,145,154,163,171,180,188,196,204,213,221,229,236,244,251,255,
0,0,4,12,20,27,35,43,51,59,68,77,86,96,105,115,125,135,144,153,162,170,179,187,196,204,212,220,228,236,243,251,255,
0,0,3,11,19,26,34,42,50,58,66,75,85,94,104,114,123,133,143,152,161,169,178,186,195,203,211,220,228,235,243,250,255,
0,0,3,10,18,25,33,41,48,56,64,73,83,92,102,112,122,132,141,151,160,169,177,186,194,202,211,219,227,235,243,250,255,
0,0,2,9,17,24,32,39,47,55,64,72,81,91,101,111,120,130,140,149,159,168,177,185,194,202,210,219,227,235,242,250,255,
0,0,1,8,16,23,31,38,46,53,63,70,80,89,99,109,119,129,138,148,157,167,176,185,193,202,210,218,227,234,242,250,255,
0,0,0,7,15,22,30,37,45,52,61,69,78,88,98,108,118,127,137,147,156,165,175,184,193,201,210,218,226,234,242,250,255,
0,0,0,6,13,21,28,36,43,51,59,67,77,86,96,106,116,126,136,145,155,164,173,183,192,201,209,218,226,234,242,250,255,
0,0,0,5,12,20,27,35,42,50,57,67,75,85,95,105,115,125,134,144,154,163,172,181,191,200,209,217,225,233,241,249,255,
0,0,0,4,11,19,26,34,41,49,56,66,74,83,93,103,113,123,133,143,152,162,171,180,189,199,208,217,225,233,241,249,255,
0,0,0,3,10,18,25,33,40,48,55,64,72,82,92,101,111,121,131,141,151,161,170,179,188,197,207,216,224,232,240,248,255,
0,0,0,2,9,17,24,32,39,47,54,61,71,80,90,100,110,120,130,140,150,160,169,178,187,196,205,215,224,232,240,248,255,
0,0,0,0,8,15,23,30,37,45,52,60,70,78,88,98,108,118,128,138,148,158,167,176,185,195,204,213,223,232,240,248,255,
0,0,0,0,7,14,21,29,36,43,50,58,68,76,86,96,106,116,126,137,147,156,166,175,184,193,202,212,222,231,240,248,255,
0,0,0,0,5,12,20,27,34,41,49,56,65,74,84,94,104,115,125,135,145,154,164,173,182,191,201,211,220,230,239,248,255,
0,0,0,0,4,11,18,25,32,40,47,54,62,71,82,92,103,113,123,133,143,153,163,172,181,189,199,209,219,229,238,247,255,
};
#endif


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
		params->dsi.LANE_NUM				= LCM_FOUR_LANE;

		//The following defined the fomat for data coming from LCD engine.
		params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;
		params->dsi.vertical_sync_active				= 3;// 3    2
		params->dsi.vertical_backporch					= 4;// 20   1
		params->dsi.vertical_frontporch					= 2; // 1  12
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 

		params->dsi.horizontal_sync_active				= 2;// 50  2
		params->dsi.horizontal_backporch				= 5;
		params->dsi.horizontal_frontporch				= 5;
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
		params->dsi.CLK_HS_POST=26;
#ifndef CONFIG_MTK_FPGA
    		params->dsi.PLL_CLOCK = 380; //390 for 120fps//320;//dsi clock customization: should config clock value directly    	
		params->dsi.ssc_disable = 1;
#else
		params->dsi.fbk_div = 0x10;
		params->dsi.pll_div1 = 0x2;
		params->dsi.pll_div2 = 0x2;
#endif

                params->od_table_size = 33 * 33;
                params->od_table = (void*)&od_table_33x33;
}

static void lcm_init(void)
{
	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(10);
	push_table(lcm_vdo_initialization_setting, sizeof(lcm_vdo_initialization_setting) / sizeof(struct LCM_setting_table), 1);
	need_set_lcm_addr = 1;
}

static void lcm_suspend(void)
{
	push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);
}


static void lcm_resume(void)
{

	lcm_init();
	push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
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
		printf("%s, LK otm1282a debug: nt35590 id = 0x%08x\n", __func__, id);
    #else
		printk("%s, kernel otm1282a horse debug: otm1282a id = 0x%08x\n", __func__, id);
    #endif

    if(id == LCM_ID_OTM1282A)
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
LCM_DRIVER otm1282a_hd720_dsi_vdo_lcm_drv = 
{
    .name			= "otm1282a_hd720_dsi_vdo",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id     = lcm_compare_id,
	.ata_check		= lcm_ata_check,
	//.esd_check   	= lcm_esd_check,
    //.esd_recover	= lcm_esd_recover,
#if (LCM_DSI_CMD_MODE)
	.update         = lcm_update,
	//.set_backlight	= lcm_setbacklight,
//	.set_pwm        = lcm_setpwm,
//	.get_pwm        = lcm_getpwm,
#endif
};
