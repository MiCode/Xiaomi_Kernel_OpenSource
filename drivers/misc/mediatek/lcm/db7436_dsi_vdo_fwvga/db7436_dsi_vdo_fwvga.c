#ifdef BUILD_LK
#include <string.h>
#include <mt_gpio.h>
#include <platform/mt_pmic.h>
//#include <lge_bootmode.h>
#include <platform/boot_mode.h>
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

extern void chargepump_DSV_on();
extern void chargepump_DSV_off();

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------
// pixel
#define FRAME_WIDTH  			(480)
#define FRAME_HEIGHT 			(854)
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
#define GPIO_DSV_AVDD_EN (GPIO107 | 0x80000000)
#define GPIO_DSV_AVDD_EN_M_GPIO GPIO_MODE_00
#define GPIO_DSV_AVDD_EN_M_KROW GPIO_MODE_06
#define GPIO_DSV_AVDD_EN_M_PWM GPIO_MODE_05
#endif

#ifndef GPIO_DSV_AVEE_EN
#define GPIO_DSV_AVEE_EN (GPIO106 | 0x80000000)
#define GPIO_DSV_AVEE_EN_M_GPIO GPIO_MODE_00
#define GPIO_DSV_AVEE_EN_M_KROW GPIO_MODE_06
#define GPIO_DSV_AVEE_EN_M_PWM GPIO_MODE_05
#endif

#ifndef GPIO_LCM_PWR
#define GPIO_LCM_PWR         (GPIO75 | 0x80000000)
#define GPIO_LCM_PWR_M_GPIO   GPIO_MODE_00
#define GPIO_LCM_PWR_M_KCOL   GPIO_MODE_01
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

static struct LCM_setting_table lcm_initialization_setting[] = {
	{0xB7, 7, {0x00, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B}},
	{0xB6, 6, {0x06, 0x0A, 0x34, 0x23, 0x41, 0x0A}},
	{0xB8, 4, {0x00, 0x42, 0x12, 0xF7}},
	{0xB0, 1, {0x00}},
       {0xB5, 10, {0x43, 0xA0, 0x01, 0x12, 0x06, 0x00, 0x00, 0x00, 0x00, 0x48}},
       {0xB4, 6, {0x01, 0x0D, 0x02, 0x02, 0x02, 0x02}},
       {0xB3, 3, {0x8B, 0x7F, 0x30}},
       {0xB2, 2, {0x00, 0x03}},       
       {0xB1, 4, {0xC6, 0x1E, 0x0F, 0x00}},       
       {0xB9, 3, {0x00, 0x03, 0x04}},       
       {0xBB, 1, {0x30}},
       {0x36, 1, {0x00}},       
       {0xD9, 1, {0xA0}},       
       {0xF8, 2, {0x00, 0x06}},       
       {0xD7, 2, {0x00, 0xB6}},       
       {0xBD, 2, {0x00, 0x23}},       
       {0xC5, 3, {0x3F, 0x00, 0x50}},       
       {0xC4, 1, {0x00}},       
       {0xD0, 9, {0x00, 0x26, 0x74, 0x14, 0x00, 0x00, 0x32, 0x03, 0x03}},       
       {0xD1, 9, {0x00, 0x26, 0x74, 0x14, 0x00, 0x00, 0x32, 0x03, 0x03}},              
       {0xD2, 9, {0x00, 0x26, 0x74, 0x14, 0x00, 0x00, 0x32, 0x03, 0x03}},                     
       {0xD3, 9, {0x00, 0x26, 0x74, 0x14, 0x00, 0x00, 0x32, 0x03, 0x03}},                     
       {0xD4, 9, {0x00, 0x26, 0x74, 0x14, 0x00, 0x00, 0x32, 0x03, 0x03}},                     
       {0xD5, 9, {0x00, 0x26, 0x74, 0x14, 0x00, 0x00, 0x32, 0x03, 0x03}},
	{REGFLAG_END_OF_TABLE, 0x00, {}},       
};

static struct LCM_setting_table lcm_initialization_setting_ext[] = {
        {0xFE, 1, {0x00}},       
	{0x11,	0,  {}},
	{REGFLAG_DELAY, 120, {}},    //MDELAY(120)
	{0x29,	0,  {}},	
       {REGFLAG_END_OF_TABLE, 0x00, {}},       
};

static LCM_setting_table_V3 lcm_EXTC_set_enable_V3[] = {
	{0x15, 0xFE, 1, {0x00}},
};

static LCM_setting_table_V3 lcm_initialization_setting_V3[] = {
	{0x39, 0xB7, 7, {0x00, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B}},
	{0x39, 0xB6, 6, {0x06, 0x0A, 0x34, 0x23, 0x41, 0x0A}},
	{0x39, 0xB8, 4, {0x00, 0x42, 0x12, 0xF7}},
	{0x15, 0xB0, 1, {0x00}},
       {0x39, 0xB5, 10, {0x43, 0xA0, 0x01, 0x12, 0x06, 0x00, 0x00, 0x00, 0x00, 0x48}},
       {0x39, 0xB4, 6, {0x01, 0x0D, 0x02, 0x02, 0x02, 0x02}},
       {0x39, 0xB3, 3, {0x8B, 0x7F, 0x30}},
       {0x15, 0xB2, 2, {0x00, 0x03}},       
       {0x39, 0xB1, 4, {0xC6, 0x1E, 0x0F, 0x00}},       
       {0x39, 0xB9, 3, {0x00, 0x03, 0x04}},       
       {0x15, 0xBB, 1, {0x30}},
       {0x15, 0x36, 1, {0x00}},       
       {0x15, 0xD9, 1, {0xA0}},       
       {0x15, 0xF8, 2, {0x00, 0x06}},       
       {0x15, 0xD7, 2, {0x00, 0xB6}},       
       {0x15, 0xBD, 2, {0x00, 0x23}},       
       {0x39, 0xC5, 3, {0x3F, 0x00, 0x50}},       
       {0x15, 0xC4, 1, {0x00}},       
       {0x39, 0xD0, 9, {0x00, 0x26, 0x74, 0x14, 0x00, 0x00, 0x32, 0x03, 0x03}},       
       {0x39, 0xD1, 9, {0x00, 0x26, 0x74, 0x14, 0x00, 0x00, 0x32, 0x03, 0x03}},              
       {0x39, 0xD2, 9, {0x00, 0x26, 0x74, 0x14, 0x00, 0x00, 0x32, 0x03, 0x03}},                     
       {0x39, 0xD3, 9, {0x00, 0x26, 0x74, 0x14, 0x00, 0x00, 0x32, 0x03, 0x03}},                     
       {0x39, 0xD4, 9, {0x00, 0x26, 0x74, 0x14, 0x00, 0x00, 0x32, 0x03, 0x03}},                     
       {0x39, 0xD5, 9, {0x00, 0x26, 0x74, 0x14, 0x00, 0x00, 0x32, 0x03, 0x03}},
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
	params->dsi.mode   = SYNC_EVENT_VDO_MODE;//SYNC_PULSE_VDO_MODE;
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

	params->dsi.vertical_sync_active = 2;//4; 
	params->dsi.vertical_backporch = 11; 
        params->dsi.vertical_frontporch = 127;
	
	params->dsi.vertical_active_line = FRAME_HEIGHT; 

	params->dsi.horizontal_sync_active				= 8;
	params->dsi.horizontal_backporch				= 88;
	params->dsi.horizontal_frontporch				= 24;
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

#ifdef CONFIG_MIXMODE_FOR_INCELL
    params->dsi.mixmode_enable = TRUE;
    params->dsi.pwm_fps = 60;
    params->dsi.mixmode_mipi_clock = 468; 
#endif

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
	//params->dsi.PLL_CLOCK = 416;
	params->dsi.PLL_CLOCK = 208;

	LCM_PRINT("[LCD] lcm_get_params \n");

}

static void init_lcm_registers(void)
{
        push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
	LCM_PRINT("[LCD] init_lcm_registers \n");
}

static void init_lcm_registers_added(void)
{
        push_table(lcm_initialization_setting_ext, sizeof(lcm_initialization_setting_ext) / sizeof(struct LCM_setting_table), 1);

	LCM_PRINT("[LCD] init_lcm_registers_added \n");
}

static void init_lcm_registers_sleep(void)
{
	unsigned int data_array[2];

	data_array[0] = 0x00100500;	//Seep In
	dsi_set_cmdq(data_array, 1, 1);

        MDELAY(100);
        data_array[0] = 0x05FE1500;
	dsi_set_cmdq(data_array, 1, 1);        
        data_array[0] = 0x00033902;
        data_array[1] = 0x000010D8;
	dsi_set_cmdq(data_array, 2, 1);
        LCM_PRINT("[LCD] init_lcm_registers_sleep \n");
}


/* 1.8v LDO enable */
static void ldo_1v8io_on(void)
{
#ifdef BUILD_UBOOT 
	#error "not implemeted"
#elif defined(BUILD_LK) 	
	// IOVCC 1.8v LDO on
	upmu_set_rg_vcamd_vosel(3);
	upmu_set_rg_vcamd_en(1);
#else
	hwPowerOn(MT6323_POWER_LDO_VCAMD, VOL_1800, "1V8_LCD_VIO_MTK_S");
#endif 
}

/* vgp2 1.8v LDO disable */
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

static void ldo_ext_3v0_on(void)
{
#ifdef BUILD_UBOOT 
	#error "not implemeted"
#elif defined(BUILD_LK)
        mt_set_gpio_mode(GPIO_LCM_PWR, GPIO_LCM_PWR_M_GPIO);
        mt_set_gpio_pull_enable(GPIO_LCM_PWR, GPIO_PULL_ENABLE);
        mt_set_gpio_dir(GPIO_LCM_PWR, GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_LCM_PWR, GPIO_OUT_ONE);
#else
        mt_set_gpio_mode(GPIO_LCM_PWR, GPIO_LCM_PWR_M_GPIO);
        mt_set_gpio_pull_enable(GPIO_LCM_PWR, GPIO_PULL_ENABLE);
        mt_set_gpio_dir(GPIO_LCM_PWR, GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_LCM_PWR, GPIO_OUT_ONE);
#endif
}

static void ldo_ext_3v0_off(void)
{
#ifdef BUILD_UBOOT 
	#error "not implemeted"
#elif defined(BUILD_LK)
	mt_set_gpio_mode(GPIO_LCM_PWR, GPIO_LCM_PWR_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_LCM_PWR, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_LCM_PWR, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCM_PWR, GPIO_OUT_ZERO);
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
static void ldo_p5m5_dsv_on(void)
{
    #if defined(CONFIG_LEDS_LM3632)
    chargepump_DSV_on();
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

static void ldo_p5m5_dsv_off(void)
{
    #if defined(CONFIG_LEDS_LM3632)
    chargepump_DSV_off();
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

static void lcm_init(void)
{	
#if defined(BUILD_LK) 	
	ldo_p5m5_dsv_off();
#endif

       SET_RESET_PIN(0);        
	ldo_1v8io_on();
	ldo_ext_3v0_on();
	
        MDELAY(10);
        ldo_p5m5_dsv_on();
        SET_RESET_PIN(1);  

	MDELAY(20);
        init_lcm_registers();   
	init_lcm_registers_added();	//Display On

	MDELAY(10);
	need_set_lcm_addr = 1;

	LCM_PRINT("[LCD] lcm_init \n");
}

static void lcm_suspend(void)
{
	init_lcm_registers_sleep(); // Display off
	LCM_PRINT("[LCD] lcm_suspend \n");
}

static void lcm_suspend_power(void)
{
	MDELAY(120);	
	ldo_p5m5_dsv_off(); // DSV +-5V power off
	//MDELAY(20);
	//VCI/IOVCC off
	//ldo_1v8io_off();
	//ldo_ext_3v0_off();
	LCM_PRINT("[LCD] lcm_suspend_power \n");
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
		return 1;
}
// ---------------------------------------------------------------------------
//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------
LCM_DRIVER db7436_dsi_vdo_fwvga_drv = {
	.name = "db7436_dsi_vdo_fwvga",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.suspend_power = lcm_suspend_power,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.update = lcm_update,
#if (!defined(BUILD_UBOOT) && !defined(BUILD_LK))
	.esd_recover = lcm_esd_recover,
#endif
};
