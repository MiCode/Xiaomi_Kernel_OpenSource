#ifdef BUILD_LK
#include <string.h>
#include <mt_gpio.h>
#include <platform/mt_pmic.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#include <linux/string.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#endif

#include "lcm_drv.h"
#include <cust_gpio_usage.h>
#if defined(BUILD_LK)
#define LCM_PRINT printf
#elif defined(BUILD_UBOOT)
#define LCM_PRINT printf
#else
#define LCM_PRINT printk
#endif

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------
// pixel
#if 1
#define FRAME_WIDTH  			(720)
#define FRAME_HEIGHT 			(1280)
#else
#define FRAME_WIDTH  			(540)
#define FRAME_HEIGHT 			(960)
#endif

// physical dimension
#if 1
#define PHYSICAL_WIDTH        (64)
#define PHYSICAL_HIGHT         (116)
#else
#define PHYSICAL_WIDTH        (70)
#define PHYSICAL_HIGHT         (122)
#endif

#define LCM_ID       (0xb9)
#define LCM_DSI_CMD_MODE		0

#define REGFLAG_DELAY 0xAB
#define REGFLAG_END_OF_TABLE 0xAA // END OF REGISTERS MARKER

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

#define dsi_set_cmdq_V3(para_tbl, size, force_update)   	lcm_util.dsi_set_cmdq_V3(para_tbl, size, force_update)
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
static LCM_setting_table_V3 lcm_initialization_setting_V3[] = {

	{0X39, 0X51, 1, {0XFF}},	//Write_Display_Brightness
	{0X39, 0X53, 1, {0X0C}},		//Write_CTRL_Display
	{0X39, 0X55, 1, {0X00}},		//Write_CABC
	{0X29, 0XB0, 1, {0X04}},		//Test command
	{0X29, 0XC1, 3, {0X84,0X61,0X00}},	//Test command
	{0X29, 0XC7, 30, {0x00,0x0A,0x16,0x20,0x2C, //GAMMA 1
					0x39,0x43,0x52,0x36,0x3E,
					0x4B,0x58,0x5A,0x5F,0x67,
					0x00,0x0A,0x16,0x20,0x2C,
					0x39,0x43,0x52,0x36,0x3E,
					0x4B,0x58,0x5A,0x5F,0x67}}, //GAMMA 2
	{0X29, 0XC8, 19, {0x00,0x00,0x00,0x00,0x00,
						0xFC,0x00,0x00,0x00,0x00,
						0x00,0xFC,0x00,0x00,0x00,
						0x00,0x00,0xFC,0x00}},
	#if 1
	{0X29, 0XB8, 6, {0x07,0x90,0x1E,0x00,0x40,0x32}}, //Back Light Control 1
	{0X29, 0XB9, 6, {0x07,0x8C,0x3C,0x20,0x2D,0x87}},
	{0X29, 0XBA, 6, {0x07,0x82,0x3C,0x10,0x3C,0xB4}},
	{0X29, 0XCE, 24, {0x7D,0x40,0x43,0x49,0x55,
					0x62,0x71,0x82,0x94,0xA8,
					0xB9,0xCB,0xDB,0xE9,0xF5,
					0xFC,0xFF,0x02,0x00,0x04,
					0x04,0x44,0x04,0x01}},
	{0X29, 0XBB, 3, {0x01,0x1E,0x14}},
	{0X29, 0XBC, 3, {0x01,0x50,0x32}},
	{0X29, 0XBD, 3, {0x00,0xB4,0xA0}},
	#endif
	{0X29, 0XD6, 1, {0x01}},
	{0X15, 0X36, 1, {0x00}},
//	{0X05, 0X29, 1, {0x00}},
//	{0X05, 0X11, 1, {0x00}},
};
#else

static struct LCM_setting_table lcm_initialization_setting[] = {
	{0x51,  1,  {0xFF}},     //[1] Power On #1
	{0x53,  1,  {0x2C}},     //[1] Power On #2
	{0x55,  1,  {0x40}},     //[1] Power On #3
	{0xB0,  1,  {0x04}},	 // Test command
	{0xC1,  3,  {0x84, 0x61, 0x00}},	//test command
	{0xD6,  1,  {0x01}},	// test command

	{0x36,  1,  {0x00}},	// set address mode

	/*display on*/
	{0x29,	0,  {}},            //[5] set display on

	/* exit sleep*/
	{0x11,	0,  {}},            //[3] exit sleep mode
	{REGFLAG_DELAY, 120, {}},    //MDELAY(120)
	{REGFLAG_END_OF_TABLE, 0x00, {}},
};

static LCM_setting_table_V3 lcm_initialization_setting_V3[] = {
	{0X39, 0X51, 1, {0XFF}},	//Write_Display_Brightness
	{0X39, 0X53, 1, {0X2C}},		//Write_CTRL_Display
	{0X39, 0X55, 1, {0X40}},		//Write_CABC
#if 0	
	{0X29, 0XB0, 1, {0X04}},		//Test command
	{0X29, 0XC1, 3, {0X84,0X61,0X00}},	//Test command

	{0X29, 0XC7, 30, {0x00,0x0A,0x16,0x20,0x2C, /*GAMMA 1 */
					0x39,0x43,0x52,0x36,0x3E, 
					0x4B,0x58,0x5A,0x5F,0x67,
					0x00,0x0A,0x16,0x20,0x2C,
					0x39,0x43,0x52,0x36,0x3E,
					0x4B,0x58,0x5A,0x5F,0x67}}, /*GAMMA 2*/
	{0X29, 0XC8, 19, {0x00,0x00,0x00,0x00,0x00,
						0xFC,0x00,0x00,0x00,0x00,
						0x00,0xFC,0x00,0x00,0x00,
						0x00,0x00,0xFC,0x00}},
				
	{0X29, 0XB8, 6, {0x07,0x90,0x1E,0x00,0x40,0x32}}, //Back Light Control 1
	{0X29, 0XB9, 6, {0x07,0x8C,0x3C,0x20,0x2D,0x87}}, //Back Light Control 2
	{0X29, 0XBA, 6, {0x07,0x82,0x3C,0x10,0x3C,0xB4}}, //Back Light Control 3
	{0X29, 0XCE, 24, {0x7D,0x40,0x43,0x49,0x55, /*Back Light Control 1 */
					0x62,0x71,0x82,0x94,0xA8,
					0xB9,0xCB,0xDB,0xE9,0xF5,
					0xFC,0xFF,0x02,0x00,0x04,
					0x04,0x44,0x04,0x01}},

	{0X29, 0XBB, 3, {0x01,0x1E,0x14}}, //SRE Control 1
	{0X29, 0XBC, 3, {0x01,0x50,0x32}}, //SRE Control 2
	{0X29, 0XBD, 3, {0x00,0xB4,0xA0}}, //SRE Control 3

	{0X29, 0XD6, 1, {0x01}}, //Test cmd
	#endif	
	{0X15, 0X36, 1, {0x00}}, //set address mode
	{0X05, 0X29, 1, {0x00}},
	{0X05, 0X11, 1, {0x00}},
};
#endif
static struct LCM_setting_table __attribute__ ((unused)) lcm_backlight_level_setting[] = {
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
		case REGFLAG_DELAY:
			MDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
			dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
		}
	}
	LCM_PRINT("[LCD] push_table \n");
}
// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------
static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy((void*)&lcm_util, (void*)util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS * params) 
{
#if 1
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type   = LCM_TYPE_DSI;

	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

       params->dsi.mode   = SYNC_EVENT_VDO_MODE; //BURST_VDO_MODE;//SYNC_PULSE_VDO_MODE;
//	params->dsi.switch_mode = CMD_MODE;
//	params->dsi.switch_mode_enable = 0;

	// DSI
	/* Command mode setting */
	params->dsi.LANE_NUM				    = LCM_FOUR_LANE;
	//The following defined the fomat for data coming from LCD engine.
	params->dsi.data_format.color_order 	= LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   	= LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     	= LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format      	= LCM_DSI_FORMAT_RGB888;

	// Highly depends on LCD driver capability.
	params->dsi.packet_size=256;
	//video mode timing

	params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active				= 1;//2;
	params->dsi.vertical_backporch					= 3;   // from Q driver
	params->dsi.vertical_frontporch					= 4;  // rom Q driver
	params->dsi.vertical_active_line				= FRAME_HEIGHT;

	params->dsi.horizontal_sync_active				= 5;//10;
	params->dsi.horizontal_backporch				= 40; // from Q driver
	params->dsi.horizontal_frontporch				= 140;  // from Q driver
	params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

	params->dsi.PLL_CLOCK = 210;//240; //this value must be in MTK suggested table


#else
	memset(params, 0, sizeof(LCM_PARAMS)); 

	params->type   = LCM_TYPE_DSI;
	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

   params->physical_width=PHYSICAL_WIDTH;
   params->physical_height=PHYSICAL_HIGHT;

	// enable tearing-free
	//params->dbi.te_mode 				= LCM_DBI_TE_MODE_VSYNC_ONLY;
	params->dbi.te_mode 				= LCM_DBI_TE_MODE_DISABLED;
	params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode   = CMD_MODE;
#else
	params->dsi.mode   = SYNC_PULSE_VDO_MODE;//SYNC_EVENT_VDO_MODE;
#endif

	// DSI
	/* Command mode setting */
	params->dsi.LANE_NUM				= LCM_FOUR_LANE;
	//The following defined the fomat for data coming from LCD engine. 

	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;	
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST; 
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB; 
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888; 

	// Highly depends on LCD driver capability. 
	params->dsi.packet_size = 256; 
	// Video mode setting 
	params->dsi.intermediat_buffer_num = 2; 
	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888; 

#if 0
	params->dsi.vertical_sync_active				= 1;//2;
	params->dsi.vertical_backporch					= 3;   // from Q driver
	params->dsi.vertical_frontporch 				= 4;  // rom Q driver
	params->dsi.vertical_active_line				= FRAME_HEIGHT;

	params->dsi.horizontal_sync_active				= 5;//10;
	params->dsi.horizontal_backporch				= 40; // from Q driver
	params->dsi.horizontal_frontporch				= 140;	// from Q driver
	params->dsi.horizontal_active_pixel 			= FRAME_WIDTH;
#else
	params->dsi.vertical_sync_active = 1; 
	params->dsi.vertical_backporch = 3; 
	params->dsi.vertical_frontporch = 6; 
	params->dsi.vertical_active_line = FRAME_HEIGHT; 

	params->dsi.horizontal_sync_active				= 10;
	params->dsi.horizontal_backporch				= 45;
	params->dsi.horizontal_frontporch				= 96;
	params->dsi.horizontal_active_pixel 			= FRAME_WIDTH;
#endif
	// Bit rate calculation
	//params->dsi.pll_div1=35;		// fref=26MHz, fvco=fref*(div1+1)	(div1=0~63, fvco=500MHZ~1GHz)
	//params->dsi.pll_div2=1; 		// div2=0~15: fout=fvo/(2*div2)

#if 0
	/* ESD or noise interference recovery For video mode LCM only. */
	// Send TE packet to LCM in a period of n frames and check the response.
	params->dsi.lcm_int_te_monitor = TRUE;
	params->dsi.lcm_int_te_period = 1;		// Unit : frames

	// Need longer FP for more opportunity to do int. TE monitor applicably.
	if(params->dsi.lcm_int_te_monitor)
		params->dsi.vertical_frontporch *= 2;

	// Monitor external TE (or named VSYNC) from LCM once per 2 sec. (LCM VSYNC must be wired to baseband TE pin.)
	params->dsi.lcm_ext_te_monitor = FALSE;
#endif

	// Non-continuous clock
	params->dsi.noncont_clock = TRUE;
	params->dsi.noncont_clock_period = 2;	// Unit : frames

	// DSI MIPI Spec parameters setting
	params->dsi.HS_TRAIL = 6;
	params->dsi.HS_ZERO = 9;
	params->dsi.HS_PRPR = 5;
	params->dsi.LPX = 4;
	params->dsi.TA_SACK = 1;
	params->dsi.TA_GET = 20;
	params->dsi.TA_SURE = 6;
	params->dsi.TA_GO = 16;
	params->dsi.CLK_TRAIL = 5;
	params->dsi.CLK_ZERO = 18;
	params->dsi.LPX_WAIT = 1;
	params->dsi.CONT_DET = 0;
	params->dsi.CLK_HS_PRPR = 4;


	// Bit rate calculation
	//PLL = (Hsync+HBP+Hadr+HFP)*(Vsync+VBP+Vadr+VFP)*BPP*FPS/ 2 (date per clock)/ lane num.
	params->dsi.PLL_CLOCK = 208;
	//params->dsi.PLL_CLOCK = 234;
	
	LCM_PRINT("[SEOSCTEST] lcm_get_params \n");
	LCM_PRINT("[LCD] lcm_get_params \n");
#endif
}

static void init_lcm_registers(void)
{
	unsigned int data_array[32];

#if 1
#if 1
	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
#else
/*
{0x51,	1,	{0xFF}},	 //[1] Power On #1
{0x53,	1,	{0x2C}},	 //[1] Power On #2
{0x55,	1,	{0x40}},	 //[1] Power On #3
{0xB0,	1,	{0x04}},	 // Test command
{0xC1,	3,	{0x84, 0x61, 0x00}},	//test command
{0xD6,	1,	{0x01}},	// test command

{0x36,	1,	{0x00}},	// set address mode

{0X39, 0X51, 1, {0XFF}},	//Write_Display_Brightness
{0X39, 0X53, 1, {0X2C}},		//Write_CTRL_Display
{0X39, 0X55, 1, {0X40}},		//Write_CABC
{0X29, 0XB0, 1, {0X04}},		//Test command
{0X29, 0XC1, 3, {0X84,0X61,0X00}},	//Test command
{0X29, 0XD6, 1, {0x01}}, //Test cmd

*/
data_array[0] = 0xff513900;
dsi_set_cmdq(data_array, 1, 1);

data_array[0] = 0x2c533900;
dsi_set_cmdq(data_array, 1, 1);

data_array[0] = 0x40553900;
dsi_set_cmdq(data_array, 1, 1);

data_array[0] = 0x04B02900;
dsi_set_cmdq(data_array, 1, 1);

data_array[0] = 0x84C12900;
data_array[1] = 0x00000061;
dsi_set_cmdq(data_array, 2, 1);

data_array[0] = 0x01D62900;
dsi_set_cmdq(data_array, 1, 1);

data_array[0] = 0x00290500; //Display On
dsi_set_cmdq(data_array, 1, 1);

data_array[0] = 0x00110500; //Sleep Out
dsi_set_cmdq(data_array, 1, 1);

#endif
#else
	dsi_set_cmdq_V3(lcm_initialization_setting_V3, sizeof(lcm_initialization_setting_V3) / sizeof(LCM_setting_table_V3), 1);

	//MDELAY(120);
#if 0
	data_array[0] = 0x00290500;	//Display On
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00110500;	//Sleep Out
	dsi_set_cmdq(data_array, 1, 1);
#endif	
	MDELAY(120);
#endif
	LCM_PRINT("[LCD] init_lcm_registers \n");
}
#if 0
static void init_lcm_registers_added(void)
{
	unsigned int data_array[1];

	data_array[0] = 0x00290500;	//Display On
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(10);
	LCM_PRINT("[LCD] init_lcm_registers_added \n");
}
#endif
static void init_lcm_registers_sleep(void)
{
	unsigned int data_array[1];

	MDELAY(10);
	data_array[0] = 0x00280500; //Display Off
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(20);
	data_array[0] = 0x00100500; //enter sleep
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(80);
	LCM_PRINT("[LCD] init_lcm_registers_sleep \n");
}


/* VCAMD 1.8v LDO enable */
static void ldo_1v8io_on(void)
{
#ifdef BUILD_UBOOT 
	#error "not implemeted"
#elif defined(BUILD_LK) 	
	upmu_set_rg_vcamd_vosel(3);
	upmu_set_rg_vcamd_en(1);
#else
	hwPowerOn(MT6323_POWER_LDO_VCAMD, VOL_1800, "1V8_LCD_VIO_MTK_S");
#endif 
}

/* VCAMD 1.8v LDO disable */
static void ldo_1v8io_off(void)
{
#ifdef BUILD_UBOOT 
#error "not implemeted"
#elif defined(BUILD_LK) 	
	upmu_set_rg_vcamd_en(0);

#else
	hwPowerDown(MT6323_POWER_LDO_VCAMD, "1V8_LCD_VIO_MTK_S");
#endif 
}

/* VGP2 3.0v LDO enable */
static void ldo_ext_3v0_on(void)
{
#if 1 //defined(TARGET_S7)
#ifdef BUILD_UBOOT 
	#error "not implemeted"
#elif defined(BUILD_LK)
	upmu_set_rg_vgp2_vosel(6);  // VGP2_SEL= 101 : 2.8V , 110 : 3.0V
	upmu_set_rg_vgp2_en(1);


#else
	hwPowerOn(MT6323_POWER_LDO_VGP2, VOL_3000, "3V0_LCD_VCC_MTK_S");
#endif
#else
	mt_set_gpio_mode(GPIO_LCM_PWR, GPIO_LCM_PWR_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_LCM_PWR, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_LCM_PWR, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCM_PWR, GPIO_OUT_ONE);
#endif
}

/* VGP2 3.0v LDO disable */
static void ldo_ext_3v0_off(void)
{
#if 1 //defined(TARGET_S7)
#ifdef BUILD_UBOOT 
	#error "not implemeted"
#elif defined(BUILD_LK)
	upmu_set_rg_vgp2_en(0);
#else
	hwPowerDown(MT6323_POWER_LDO_VGP2, "3V0_LCD_VCC_MTK_S");
#endif
#else
	mt_set_gpio_mode(GPIO_LCM_PWR, GPIO_LCM_PWR_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_LCM_PWR, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_LCM_PWR, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCM_PWR, GPIO_OUT_ZERO);
#endif
}

/*
DSV power +5V,-5v
*/
static void ldo_p5m5_dsv_3v0_on(void)
{
	mt_set_gpio_mode(GPIO_DSV_EN, GPIO_DSV_EN_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_DSV_EN, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_DSV_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_DSV_EN, GPIO_OUT_ONE);
}

static void ldo_p5m5_dsv_3v0_off(void)
{
	mt_set_gpio_mode(GPIO_DSV_EN, GPIO_DSV_EN_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_DSV_EN, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_DSV_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_DSV_EN, GPIO_OUT_ZERO);
}


static void reset_lcd_module(unsigned char reset)
{
	mt_set_gpio_mode(GPIO_LCM_RST, GPIO_LCM_RST_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_LCM_RST, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_LCM_RST, GPIO_DIR_OUT);

   if(reset){
   	mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ZERO);
      MDELAY(50);	
   }else{
   	mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ONE);
   }
}
   

static void lcm_init(void)
{
#if defined(BUILD_LK) 	
	ldo_p5m5_dsv_3v0_off();
#endif
	//SET_RESET_PIN(0);

	//TP_VCI 3.0v on
	ldo_ext_3v0_on();

	ldo_1v8io_on();

	MDELAY(200);

	ldo_p5m5_dsv_3v0_on();

	MDELAY(20);

	SET_RESET_PIN(1);
	MDELAY(20);
	SET_RESET_PIN(0);
	MDELAY(2);
	SET_RESET_PIN(1);
	MDELAY(20);

	init_lcm_registers();	//SET EXTC ~ sleep out register

	MDELAY(80);
	
//	init_lcm_registers_added();	//Display On
	need_set_lcm_addr = 1;
	LCM_PRINT("[SEOSCTEST] lcm_init \n");
	LCM_PRINT("[LCD] lcm_init \n");
}

static void lcm_suspend(void)
{
	init_lcm_registers_sleep();

#if 1 //if LPWG on, Need to block
	SET_RESET_PIN(0);
	MDELAY(20);
	//dsv low
	ldo_p5m5_dsv_3v0_off();
	MDELAY(10);
	//VCI/IOVCC off
	ldo_1v8io_off();
	//ldo_ext_3v0_off();
#endif
	
	LCM_PRINT("[LCD] lcm_suspend \n");
}


static void lcm_resume(void)
{
	lcm_init();
    need_set_lcm_addr = 1;
	LCM_PRINT("[LCD] lcm_resume \n");
}

static void lcm_esd_recover(void)
{
	lcm_suspend();
	lcm_resume();

	LCM_PRINT("[LCD] lcm_esd_recover \n");
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
	LCM_PRINT("[LCD] lcm_update \n");	
}

static unsigned int lcm_compare_id(void)
{
#if 0
	unsigned int id=0;
	unsigned char buffer[2];
	unsigned int array[16];  
    SET_RESET_PIN(1);
    SET_RESET_PIN(0);
    MDELAY(1);
    SET_RESET_PIN(1);
    MDELAY(10);//Must over 6 ms
	array[0]=0x00043902;
	array[1]=0x8983FFB9;// page enable
	dsi_set_cmdq(array, 2, 1);
	MDELAY(10);
	array[0] = 0x00023700;// return byte number
	dsi_set_cmdq(array, 1, 1);
	MDELAY(10);
	read_reg_v2(0xF4, buffer, 2);
	id = buffer[0]; 
	LCM_PRINT("%s, id = 0x%08x\n", __func__, id);
	return (LCM_ID == id)?1:0;
#else
	LCM_PRINT("[SEOSCTEST] lcm_compare_id \n");	
	return 1;
#endif	
}
// ---------------------------------------------------------------------------
//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------
LCM_DRIVER hx8389b_hd720_dsi_vdo_drv = {
	.name = "hx8389b_dsi_vdo",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
//	.compare_id = lcm_compare_id,
//	.update = lcm_update,
#if (!defined(BUILD_UBOOT) && !defined(BUILD_LK))
//	.esd_recover = lcm_esd_recover,
#endif
};
