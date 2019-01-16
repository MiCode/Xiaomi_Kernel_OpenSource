
#if defined(BUILD_LK)
#include <platform/mt_gpio.h>
#include <platform/mt_pmic.h>
#else
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#endif

#if !defined(BUILD_LK)
#include <linux/string.h>
#else
#include <string.h>
#endif
#include "lcm_drv.h"

#if defined(BUILD_LK)
#define LCM_DEBUG(fmt,arg...)  printf("[Eric][lk]""[%s]"fmt"\n",__func__,##arg)
#elif defined(BUILD_UBOOT)
#define LCM_DEBUG(fmt,arg...)  printf("[Eric][uboot]""[%s]"fmt"\n",__func__,##arg)
#else
#define LCM_DEBUG(fmt,arg...)  printk("[Eric][kernel]""[%s]"fmt"\n",__func__,##arg)
#endif

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  										(480)
#define FRAME_HEIGHT 										(854)

// ---------------------------------------------------------------------------
//  Data ID
// ---------------------------------------------------------------------------

#define		DSI_DCS_SHORT_PACKET_ID_0			0x05
#define		DSI_DCS_SHORT_PACKET_ID_1			0x15
#define		DSI_DCS_LONG_PACKET_ID				0x39



// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))

#define UDELAY(n) 											(lcm_util.udelay(n))
#define MDELAY(n) 											(lcm_util.mdelay(n))

#define REGFLAG_DELAY             							0XFE
#define REGFLAG_END_OF_TABLE      							0x100   // END OF REGISTERS MARKER

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V3(para_tbl, size, force_update)   	lcm_util.dsi_set_cmdq_V3(para_tbl, size, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg											lcm_util.dsi_read_reg()
       
#define LCM_DSI_CMD_MODE									0

struct LCM_setting_table {
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[64];
};


static struct LCM_setting_table lcm_initialization_setting[] = {
	
#if 0
// SET password		
{0xB9, 3,  {0xFF,0x83,0x79}}, 

{0xBA, 1,  {0x51}},   

// Issue DSC NOP command
{0xB1, 19, {0x00,0x50,0x44,0xEA,0x8D,0x08,0x11,0x11,0x11,0x27,    
            0x2F,0x9A,0x1A,0x42,0x0B,0x6E,0xF1,0x00,0xE6}},   

{0xB2, 13, {0x00,0x00,0xFE,0x08,0x04,0x19,0x22,0x00,0xFF,0x08,    
	    0x04,0x19,0x20}},    

{0xB4, 31, {0x80,0x08,0x00,0x32,0x10,0x03,0x32,0x13,0x70,0x32,    
            0x10,0x08,0x37,0x01,0x28,0x07,0x37,0x08,0x4C,0x20,    
            0x44,0x44,0x08,0x00,0x40,0x08,0x28,0x08,0x30,0x30,    
            0x04}},

{0xD5, 47, {0x00,0x00,0x0A,0x00,0x01,0x05,0x00,0x03,0x00,0x88,
	    0x88,0x88,0x88,0x23,0x01,0x67,0x45,0x02,0x13,0x88,
	    0x88,0x88,0x88,0x88,0x88,0x88,0x88,0x88,0x88,0x54,
	    0x76,0x10,0x32,0x31,0x20,0x88,0x88,0x88,0x88,0x88,
	    0x88,0x00,0x00,0x00,0x00,0x00,0x00}},

{0xE0, 35, {0x79,0x00,0x01,0x02,0x22,0x23,0x30,0x23,0x43,0x00,
	    0x01,0x12,0x17,0x19,0x17,0x17,0x13,0x18,0x00,0x01,
	    0x02,0x22,0x23,0x30,0x23,0x43,0x00,0x01,0x12,0x17,
	    0x19,0x17,0x17,0x13,0x18}},	
         		
{0xCC, 1,  {0x02}},
		         		
{0xB6, 4,  {0x00,0x88,0x00,0x88}},

// Sleep Out
{0x11, 1,  {0X00}},
{REGFLAG_DELAY, 150, {}}, 

// Display On
{0x29, 1,  {0X00}},
{REGFLAG_DELAY, 50, {}},

#else
{0xB9,3,{ 0xFF ,0x83 ,0x79}},
{0xBA,1,{ 0x51}},
{0xB1,19,{ 0x00 ,0x50 ,0x44 ,0xEA ,0x8D ,0x08 ,0x11 ,0x11 ,0x11 ,0x25 ,0x2d ,0x9A ,0x1A ,0x42 ,0x0B ,0x66 ,0xF1 ,0x00 ,0xE6}},
{0xB2,13,{ 0x00 ,0x00 ,0xFE ,0x08 ,0x04 ,0x19 ,0x12 ,0x00 ,0xFF ,0x08 ,0x04 ,0x19 ,0x20}},
{0xB4,31,{ 0x80 ,0x08 ,0x00 ,0x32 ,0x10 ,0x03 ,0x32 ,0x13 ,0x70 ,0x32 ,0x10 ,0x08 ,0x37 ,0x01 ,0x28 ,0x07 ,0x37 ,0x08 ,0x3C ,0x20 ,0x44 ,0x44 ,0x08 ,0x00 ,0x40 ,0x08 ,0x28 ,0x08 ,0x30 ,0x30 ,0x08}},
{0xD5,47,{ 0x00 ,0x00 ,0x0a ,0x00 ,0x01 ,0x05 ,0x00 ,0x03 ,0x00 ,0x88 ,0x88 ,0x88 ,0x88 ,0x23 ,0x01 ,0x67 ,0x45 ,0x02 ,0x13 ,0x88 ,0x88 ,0x88 ,0x88 ,0x88 ,0x88 ,0x88 ,0x88 ,0x88 ,0x88 ,0x54 ,0x76 ,0x10 ,0x32 ,0x31 ,0x20 ,0x88 ,0x88 ,0x88 ,0x88 ,0x88 ,0x88 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00}},
{0xE0,35,{ 0x79 ,0x05 ,0x0F ,0x14 ,0x26 ,0x29 ,0x3F ,0x2B ,0x45 ,0x04 ,0x0E ,0x13 ,0x17 ,0x18 ,0x16 ,0x16 ,0x12 ,0x17 ,0x05 ,0x0F ,0x14 ,0x26 ,0x29 ,0x3F ,0x2B ,0x45 ,0x04 ,0x0E ,0x13 ,0x17 ,0x18 ,0x16 ,0x16 ,0x12 ,0x17}},
{0xCC,1,{ 0x02}},
{0xB6,4,{ 0x00 ,0x88 ,0x00 ,0x88}}, //83
{0x36,1,{ 0x00}},
{0x3A,1,{ 0x77}},
{0x11,0,{}},
{REGFLAG_DELAY, 150, {}}, 
{0x29,0,{}},
{REGFLAG_DELAY, 20, {}},  
#endif
};


#if 0
static struct LCM_setting_table lcm_set_window[] = {
	{0x2A,	4,	{0x00, 0x00, (FRAME_WIDTH>>8), (FRAME_WIDTH&0xFF)}},
	{0x2B,	4,	{0x00, 0x00, (FRAME_HEIGHT>>8), (FRAME_HEIGHT&0xFF)}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
	{0x11, 0, {0x00}},
    {REGFLAG_DELAY, 120, {}},

    // Display ON
	{0x29, 0, {0x00}},
	{REGFLAG_DELAY, 10, {}},
	
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	// Display off sequence
	{0x28, 0, {0x00}},

    // Sleep Mode On
	{0x10, 0, {0x00}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif


static struct LCM_setting_table lcm_backlight_level_setting[] = {
	{0x51, 1, {0xFF}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};



static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;
	LCM_setting_table_V3 para_tbl[1];

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
				//dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);

				para_tbl[0].cmd = table[i].cmd;
				para_tbl[0].count = table[i].count;

				if(para_tbl[0].count > 1){
					para_tbl[0].id = DSI_DCS_LONG_PACKET_ID;
				    memcpy(para_tbl[0].para_list,table[i].para_list,para_tbl[0].count);
				}else if(para_tbl[0].count == 1){
					para_tbl[0].id = DSI_DCS_SHORT_PACKET_ID_1;
				    memcpy(para_tbl[0].para_list,table[i].para_list,para_tbl[0].count);
				}else if(para_tbl[0].count == 0){
					para_tbl[0].id = DSI_DCS_SHORT_PACKET_ID_0;
				}
				dsi_set_cmdq_V3(para_tbl,1,1);
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

		// enable tearing-free
		params->dbi.te_mode 				= LCM_DBI_TE_MODE_DISABLED; //LCM_DBI_TE_MODE_VSYNC_ONLY;
		params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;

		params->dbi.io_driving_current      = LCM_DRIVING_CURRENT_6575_16MA;
		
		params->dbi.data_format.format      = 3; // [chenlang], just a flag to idicate command set

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

		params->dsi.word_count=480*3;	//DSI CMD mode need set these two bellow params, different to 6577
		params->dsi.vertical_active_line=854;

		// Highly depends on LCD driver capability.
		// Not support in MT6573
		params->dsi.packet_size=256;
		//params->dsi.intermediat_buffer_num = 0;
		// Video mode setting		
		params->dsi.intermediat_buffer_num = 2;

		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
#if 0
		params->dsi.vertical_sync_active				= 4; //5;
		params->dsi.vertical_backporch					= 6; //5;
		params->dsi.vertical_frontporch					= 6; //5;
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 

		params->dsi.horizontal_sync_active				= 20;
		params->dsi.horizontal_backporch				= 46;
		params->dsi.horizontal_frontporch				= 21+10;
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;
#else
		params->dsi.vertical_sync_active				= 4;
		params->dsi.vertical_backporch					= 7;
		params->dsi.vertical_frontporch					= 7;
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 

		params->dsi.horizontal_sync_active				= 78;
		params->dsi.horizontal_backporch				= 78;
		params->dsi.horizontal_frontporch				= 78;
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

		params->dsi.horizontal_blanking_pixel				= 60;

#endif
		// Bit rate calculation
		params->dsi.pll_div1=1;		// div1=0,1,2,3;div1_real=1,2,4,4 ----0: 546Mbps  1:273Mbps
		params->dsi.pll_div2=1;		// div2=0,1,2,3;div1_real=1,2,4,4	
		params->dsi.fbk_div =30;    // fref=26MHz, fvco=fref*(fbk_div+1)*2/(div1_real*div2_real)

}


static void lcm_init(void)
{
	//power on
	lcm_util.set_gpio_mode(GPIO20, GPIO_MODE_00);    
	lcm_util.set_gpio_dir(GPIO20, GPIO_DIR_OUT);
	lcm_util.set_gpio_out(GPIO20, GPIO_OUT_ONE);


	MDELAY(20);

    SET_RESET_PIN(1);
	MDELAY(15);
    SET_RESET_PIN(0);
    MDELAY(5);
    SET_RESET_PIN(1);
    MDELAY(20);

	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);

}


static void lcm_suspend(void)
{

#if 0
	push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);
	MDELAY(20);

    SET_RESET_PIN(1);
	MDELAY(15);
    SET_RESET_PIN(0);
    MDELAY(5);
    SET_RESET_PIN(1);
    MDELAY(20);

	#if defined(BUILD_LK)
	upmu_set_rg_vgp5_en(0);
	#else
	hwPowerDown(MT65XX_POWER_LDO_VGP5, "Lance_LCM");
	#endif
#else
	SET_RESET_PIN(1);
	MDELAY(10); 
	SET_RESET_PIN(0);
	MDELAY(10); 
	SET_RESET_PIN(1); // fix me
	MDELAY(10); 
#endif

	//set power off
	lcm_util.set_gpio_mode(GPIO20, GPIO_MODE_00);    
	lcm_util.set_gpio_dir(GPIO20, GPIO_DIR_OUT);
	lcm_util.set_gpio_out(GPIO20, GPIO_OUT_ZERO);
}


static void lcm_resume(void)
{

	lcm_init();
	
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

	data_array[0]= 0x00290508; //HW bug, so need send one HS packet
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0]= 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);

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

LCM_DRIVER RX_498HX_615B_lcm_drv = 
{
    .name			= "RX_498HX_615B",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
#if (LCM_DSI_CMD_MODE)
	.set_backlight	= lcm_setbacklight,
    .update         = lcm_update,
#endif
};


