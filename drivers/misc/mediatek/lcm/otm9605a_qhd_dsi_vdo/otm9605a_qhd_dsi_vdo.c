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
#define FRAME_WIDTH  			(540)
#define FRAME_HEIGHT 			(960)
// physical dimension
#define PHYSICAL_WIDTH        (68)
#define PHYSICAL_HIGHT         (121)


#define LCM_ID       (0x40)
#define LCM_DSI_CMD_MODE		0

#define REGFLAG_DELAY 0xAB
#define REGFLAG_END_OF_TABLE 0xAA // END OF REGISTERS MARKER

/*
DSV power +5V,-5v
*/
#ifndef GPIO_DSV_AVDD_EN
#define GPIO_DSV_AVDD_EN (GPIO41 | 0x80000000)
#define GPIO_DSV_AVDD_EN_M_GPIO GPIO_MODE_00
#define GPIO_DSV_AVDD_EN_M_KROW GPIO_MODE_06
#define GPIO_DSV_AVDD_EN_M_PWM GPIO_MODE_05
#endif

#ifndef GPIO_DSV_AVEE_EN
#define GPIO_DSV_AVEE_EN (GPIO42 | 0x80000000)
#define GPIO_DSV_AVEE_EN_M_GPIO GPIO_MODE_00
#define GPIO_DSV_AVEE_EN_M_KROW GPIO_MODE_06
#define GPIO_DSV_AVEE_EN_M_PWM GPIO_MODE_05
#endif

#ifndef GPIO_LCM_PWR
#define GPIO_LCM_PWR         (GPIO115 | 0x80000000)
#endif

#ifndef GPIO_LCM_PWR_M_GPIO
#define GPIO_LCM_PWR_M_GPIO   GPIO_MODE_00
#endif

#ifndef GPIO_DSV_EN
#define GPIO_DSV_EN         (GPIO42 | 0x80000000)
#endif

#ifndef GPIO_DSV_EN_M_GPIO
#define GPIO_DSV_EN_M_GPIO   GPIO_MODE_00
#endif

#ifndef GPIO_LCM_RST
#define GPIO_LCM_RST         (GPIO112 | 0x80000000)
#endif

#ifndef GPIO_LCM_RST_M_GPIO
#define GPIO_LCM_RST_M_GPIO   GPIO_MODE_00
#endif

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

static LCM_setting_table_V3 lcm_initialization_setting_V3[] = {

	/*
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
	{REGFLAG_DELAY, 10, {}},

	...

	Setting ending by predefined flag

	{REGFLAG_END_OF_TABLE, 0x00, {}}
	*/
	{0X15, 0X00,  1, {0X00}},
	{0X39, 0XFF,  3, {0X96,0X05,0X01}},	//SET EXTC
	{0X15, 0X00,  1, {0X80}},
	{0X39, 0XFF,  2, {0X96,0X05}},		//SET MIPI
	{0X15, 0X00,  1, {0X90}},
	{0X39, 0XC5,  2, {0X96,0XD6}},
	{0X15, 0X00,  1, {0X92}},
	{0X15, 0XC5,  1, {0X02}},
	{0X15, 0X00,  1, {0X93}},
	{0X15, 0XC5,  1, {0X03}},
	{0X15, 0X00,  1, {0X94}},
	{0X15, 0XC5,  1, {0X55}},
	{0X15, 0X00,  1, {0X95}},
	{0X15, 0XC5,  1, {0X55}},
	{0X15, 0X00,  1, {0X83}},
	{0X15, 0XC5,  1, {0X00}},
	{0X15, 0X00,  1, {0X00}},
	{0X39, 0XD8,  2, {0X6F,0X6F}},
	{0X15, 0X00,  1, {0X00}},
	{0X15, 0XD9,  1, {0X58}},
	{0X15, 0X00,  1, {0X80}},
	{0X15, 0XC1,  1, {0X36}},
	{0X15, 0X00,  1, {0X81}},
	{0X15, 0XC1,  1, {0X55}},
	{0X15, 0X00,  1, {0X89}},
	{0X15, 0XC0,  1, {0X01}},
	{0X15, 0X00,  1, {0XB1}},
	{0X15, 0XC5,  1, {0X29}},
	{0X15, 0X00,  1, {0XB2}},
	{0X15, 0XC5,  1, {0X01}},
	{0X15, 0X00,  1, {0XC0}},
	{0X15, 0XC5,  1, {0X00}},
	{0X15, 0X00,  1, {0X80}},
	{0X15, 0XC4,  1, {0X9C}},
	{0X15, 0X00,  1, {0XB4}},
	{0X15, 0XC0,  1, {0X50}},
	{0X15, 0X00,  1, {0XA0}},
	{0X15, 0XC1,  1, {0X00}},
	{0X15, 0X00,  1, {0XC5}},
	{0X15, 0XB0,  1, {0X03}},
	{0X15, 0X00,  1, {0XD2}},
	{0X15, 0XB0,  1, {0X04}},
	{0X15, 0X00,  1, {0X84}},
	{0X15, 0XC4,  1, {0X18}},
	{0X15, 0X00,  1, {0X91}},
	{0X15, 0XC0,  1, {0X41}},
	{0X15, 0X00,  1, {0XB5}},
	{0X15, 0XC0,  1, {0X48}},
	{0X15, 0X00,  1, {0X80}},
	{0X39, 0XCE,  6, {0X8B,0X03,0X00,0X8A,	//SET Display
					0X03,0X00}},
	{0X15, 0X00,  1, {0X90}},
	{0X39, 0XCE, 14, {0X23,0XC2,0X00,0X23,	//SET Power
					0XC3,0X00,0X33,0XC5,
					0X00,0X33,0XC6,0X00,
					0X00,0X00}},
	{0X15, 0X00,  1, {0XA0}},
	{0X39, 0XCE, 14, {0X38,0X09,0X03,0XCA,	//SET Power
					0X00,0X00,0X05,0X38,
					0X08,0X03,0XCB,0X00,
					0X00,0X05}},
	{0X15, 0X00,  1, {0XB0}},
	{0X39, 0XCE, 14, {0X38,0X07,0X03,0XCC,	//SET Power
					0X00,0X00,0X05,0X38,
					0X06,0X03,0XCD,0X00,
					0X00,0X05}},
	{0X15, 0X00,  1, {0XC0}},
	{0X39, 0XCE, 14, {0X38,0X05,0X03,0XC6,	//SET Power
					0X00,0X00,0X05,0X38,
					0X04,0X03,0XC7,0X00,
					0X00,0X05}},
	{0X15, 0X00,  1, {0XD0}},
	{0X39, 0XCE, 14, {0X38,0X03,0X03,0XC8,	//SET Power
					0X00,0X00,0X05,0X38,
					0X02,0X03,0XC9,0X00,
					0X00,0X05}},
	{0X15, 0X00,  1, {0XC0}},
	{0X39, 0XCF, 10, {0X3D,0X3D,0X00,0X00,	//SET Power
					0X00,0X00,0X01,0X00,
					0X00,0X00}},
	{0X15, 0X00,  1, {0XC0}},
	{0X39, 0XCB, 15, {0X00,0X00,0X04,0X04,	//SET Power
					0X04,0X04,0X04,0X04,
					0X04,0X04,0X00,0X00,
					0X04,0X00,0X00}},
	{0X15, 0X00,  1, {0XD0}},
	{0X39, 0XCB, 15, {0X00,0X00,0X00,0X00,	//SET Power
					0X00,0X00,0X00,0X04,
					0X04,0X04,0X04,0X04,
					0X04,0X04,0X04}},
	{0X15, 0X00,  1, {0XE0}},
	{0X39, 0XCB, 10, {0X00,0X00,0X04,0X00,	//SET Power
					0X00,0X00,0X00,0X00,
					0X00,0X00}},
	{0X15, 0X00,  1, {0X80}},
	{0X39, 0XCC, 10, {0X00,0X00,0X02,0X0A,	//SET Power
					0X0C,0X0E,0X10,0X21,
					0X22,0X06}},
	{0X15, 0X00,  1, {0X90}},
	{0X39, 0XCC, 15, {0X00,0X00,0X08,0X00,	//SET Power
					0X00,0X00,0X00,0X00,
					0X00,0X00,0X00,0X00,
					0X01,0X09,0X0B}},
	{0X15, 0X00,  1, {0XA0}},
	{0X39, 0XCC, 15, {0X0D,0X0F,0X21,0X22,	//SET Power
					0X05,0X00,0X00,0X07,
					0X00,0X00,0X00,0X00,
					0X00,0X00,0X00}},
	{0X15, 0X00,  1, {0XB0}},
	{0X39, 0XCC, 10, {0X00,0X00,0X05,0X0B,	//SET Power
					0X09,0X0F,0X0D,0X21,
					0X22,0X01}},
	{0X15, 0X00,  1, {0XC0}},
	{0X39, 0XCC, 15, {0X00,0X00,0X07,0X00,	//SET Power
					0X00,0X00,0X00,0X00,
					0X00,0X00,0X00,0X00,
					0X06,0X0C,0X0A}},
	{0X15, 0X00,  1, {0XD0}},
	{0X39, 0XCC, 15, {0X10,0X0E,0X21,0X22,	//SET Power
					0X02,0X00,0X00,0X08,
					0X00,0X00,0X00,0X00,
					0X00,0X00,0X00}},
	{0X15, 0X00,  1, {0X00}},
	{0X39, 0XE1, 16, {0X0C,0X09,0X09,0X07,	//SET Power
					0X10,0X1F,0X0F,0X0F,
					0X00,0X04,0X00,0X03,
					0X0A,0X0C,0X0A,0X08}},
	{0X15, 0X00,  1, {0X00}},
	{0X39, 0XE2, 16, {0X04,0X07,0X0C,0X1F,	//SET Power
					0X06,0X1F,0X0F,0X0F,
					0X00,0X04,0X04,0X09,
					0X12,0X33,0X2F,0X22}},
	{0X15, 0X00,  1, {0X00}},
	{0X39, 0XFF,  3, {0X00,0X00,0X00}},	//SET EXTC
};

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
	memset(params, 0, sizeof(LCM_PARAMS)); 

	params->type   = LCM_TYPE_DSI;
	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

   params->physical_width=PHYSICAL_WIDTH;
   params->physical_height=PHYSICAL_HIGHT;

	// enable tearing-free
	params->dbi.te_mode 				= LCM_DBI_TE_MODE_DISABLED;
	params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode   = CMD_MODE;
#else
	params->dsi.mode   = SYNC_PULSE_VDO_MODE;//SYNC_EVENT_VDO_MODE;
#endif

	// DSI
	/* Command mode setting */
	params->dsi.LANE_NUM				= LCM_TWO_LANE;
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

	params->dsi.vertical_sync_active = 1; 
	params->dsi.vertical_backporch = 16; 
	params->dsi.vertical_frontporch = 15; 
	params->dsi.vertical_active_line = FRAME_HEIGHT; 

	params->dsi.horizontal_sync_active				= 4;
	params->dsi.horizontal_backporch				= 32;
	params->dsi.horizontal_frontporch				= 32;
	params->dsi.horizontal_active_pixel 			= FRAME_WIDTH;

	// Bit rate calculation
	//params->dsi.pll_div1=35;		// fref=26MHz, fvco=fref*(div1+1)	(div1=0~63, fvco=500MHZ~1GHz)
	//params->dsi.pll_div2=1; 		// div2=0~15: fout=fvo/(2*div2)

	/* ESD or noise interference recovery For video mode LCM only. */
	// Send TE packet to LCM in a period of n frames and check the response.
	//params->dsi.lcm_int_te_monitor = FALSE;
	//params->dsi.lcm_int_te_period = 1;		// Unit : frames

	// Need longer FP for more opportunity to do int. TE monitor applicably.
	//if(params->dsi.lcm_int_te_monitor)
	//	params->dsi.vertical_frontporch *= 2;

	// Monitor external TE (or named VSYNC) from LCM once per 2 sec. (LCM VSYNC must be wired to baseband TE pin.)
	//params->dsi.lcm_ext_te_monitor = FALSE;
	// Non-continuous clock
	//params->dsi.noncont_clock = TRUE;
	//params->dsi.noncont_clock_period = 2;	// Unit : frames

	// DSI MIPI Spec parameters setting
	/*params->dsi.HS_TRAIL = 6;
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
	params->dsi.CLK_HS_PRPR = 4;*/
	// Bit rate calculation
	params->dsi.PLL_CLOCK = 217;

	LCM_PRINT("[LCD] lcm_get_params \n");

}

static void init_lcm_registers(void)
{
	unsigned int data_array[32];

	dsi_set_cmdq_V3(lcm_initialization_setting_V3, sizeof(lcm_initialization_setting_V3) / sizeof(LCM_setting_table_V3), 1);

	MDELAY(120);

	data_array[0] = 0x00110500;	//Sleep Out
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(40);
	
	LCM_PRINT("[LCD] init_lcm_registers \n");
}

static void init_lcm_registers_added(void)
{
	unsigned int data_array[1];

	data_array[0] = 0x00290500;	//Display On
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(10);
	LCM_PRINT("[LCD] init_lcm_registers_added \n");
}

static void init_lcm_registers_sleep(void)
{
	unsigned int data_array[1];

	MDELAY(10);
	data_array[0] = 0x00280500;	//Display Off
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(60);
	LCM_PRINT("[LCD] init_lcm_registers_sleep \n");
}

/* vgp2 1.8v LDO enable */
static void ldo_1v8io_on(void)
{
#ifdef BUILD_UBOOT 
	#error "not implemeted"
#elif defined(BUILD_LK) 	
	// IOVCC 1.8v LDO on
	upmu_set_rg_vgp2_vosel(3); 
	upmu_set_rg_vgp2_en(1);
#else
	hwPowerOn(MT6323_POWER_LDO_VGP2, VOL_1800, "1V8_LCD_VIO_MTK_S");
#endif 
}

/* vgp2 1.8v LDO disable */
static void ldo_1v8io_off(void)
{
#ifdef BUILD_UBOOT 
#error "not implemeted"
#elif defined(BUILD_LK) 	
	upmu_set_rg_vgp2_en(0);
#else
	hwPowerDown(MT6323_POWER_LDO_VGP2, "1V8_LCD_VIO_MTK_S");
#endif 
}

static void ldo_ext_3v0_on(void)
{
#if 1 // L80P_FIX
#ifdef BUILD_UBOOT 
	#error "not implemeted"
#elif defined(BUILD_LK)
	upmu_set_rg_vcam_af_vosel(5);
	upmu_set_rg_vcam_af_en(1);
#else
	hwPowerOn(MT6323_POWER_LDO_VCAM_AF, VOL_2800, "2V8_LCD_VCC_MTK_S");
#endif
#else
	mt_set_gpio_mode(GPIO_LCM_PWR, GPIO_LCM_PWR_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_LCM_PWR, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_LCM_PWR, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCM_PWR, GPIO_OUT_ONE);
#endif
}

static void ldo_ext_3v0_off(void)
{
#if 1 // L80P_FIX
#ifdef BUILD_UBOOT 
	#error "not implemeted"
#elif defined(BUILD_LK)
	upmu_set_rg_vcam_af_en(0);
#else
	hwPowerDown(MT6323_POWER_LDO_VCAM_AF, "2V8_LCD_VCC_MTK_S");
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
#if 0
	mt_set_gpio_mode(GPIO_DSV_EN, GPIO_DSV_EN_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_DSV_EN, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_DSV_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_DSV_EN, GPIO_OUT_ONE);
#else	
	mt_set_gpio_mode(GPIO_DSV_AVDD_EN, GPIO_DSV_AVDD_EN_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_DSV_AVDD_EN, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_DSV_AVDD_EN, GPIO_DIR_OUT);
	mt_set_gpio_mode(GPIO_DSV_AVEE_EN, GPIO_DSV_AVEE_EN_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_DSV_AVEE_EN, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_DSV_AVEE_EN, GPIO_DIR_OUT);
	
	mt_set_gpio_out(GPIO_DSV_AVDD_EN, GPIO_OUT_ONE);
	MDELAY(1);
	mt_set_gpio_out(GPIO_DSV_AVEE_EN, GPIO_OUT_ONE);
#endif
}

static void ldo_p5m5_dsv_3v0_off(void)
{
#if 0
	mt_set_gpio_mode(GPIO_DSV_EN, GPIO_DSV_EN_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_DSV_EN, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_DSV_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_DSV_EN, GPIO_OUT_ZERO);
#else
	mt_set_gpio_mode(GPIO_DSV_AVDD_EN, GPIO_DSV_AVDD_EN_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_DSV_AVDD_EN, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_DSV_AVDD_EN, GPIO_DIR_OUT);
	mt_set_gpio_mode(GPIO_DSV_AVEE_EN, GPIO_DSV_AVEE_EN_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_DSV_AVEE_EN, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_DSV_AVEE_EN, GPIO_DIR_OUT);
	
	mt_set_gpio_out(GPIO_DSV_AVDD_EN, GPIO_OUT_ZERO);
	MDELAY(1);
	mt_set_gpio_out(GPIO_DSV_AVEE_EN, GPIO_OUT_ZERO);
#endif
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

	/*
	/	Power IOVCC > VCI On
	/ 	vgp2 1.8v LDO enable
	/   gpio pin for external LDO enable
	/	delay 1ms
	*/
	ldo_1v8io_on();

	//External LDO 3.0v on
	//ldo_ext_3v0_on();

	ldo_p5m5_dsv_3v0_on();
	/* 
	/ reset pin High > delay 1ms > Low > delay 1ms > High
	/ LCD RESET PIN 
	*/	
	SET_RESET_PIN(1);
	MDELAY(1);
	SET_RESET_PIN(0);
	MDELAY(1);
	SET_RESET_PIN(1);
	MDELAY(50);

	init_lcm_registers();	//SET EXTC ~ sleep out register

	MDELAY(80);
	init_lcm_registers_added();	//Display On
	need_set_lcm_addr = 1;

	LCM_PRINT("[LCD] lcm_init \n");
}

static void lcm_suspend(void)
{
	init_lcm_registers_sleep();

	//dsv low
	ldo_p5m5_dsv_3v0_off();

	SET_RESET_PIN(0);
	MDELAY(20);

	//VCI/IOVCC off
	ldo_1v8io_off();

	//ldo_ext_3v0_off();
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
	read_reg_v2(0xDC, buffer, 2);
	id = buffer[0]; 
	LCM_PRINT("%s, id = 0x%08x\n", __func__, id);
	return (LCM_ID == id)?1:0;
#else
	return 1;
#endif	
}
// ---------------------------------------------------------------------------
//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------
LCM_DRIVER otm9605a_qhd_dsi_vdo_drv = {
	.name = "otm9605a_qhd_dsi_vdo",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.update = lcm_update,
#if (!defined(BUILD_UBOOT) && !defined(BUILD_LK))
	.esd_recover = lcm_esd_recover,
#endif
};
