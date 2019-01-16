/* BEGIN PN:DTS2013053103858 , Added by d00238048, 2013.05.31*/
#ifndef BUILD_LK
#include <linux/string.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
    #include <platform/disp_drv_platform.h>
	
#elif defined(BUILD_UBOOT)
    #include <asm/arch/mt_gpio.h>
#else
    #include <linux/delay.h>
    #include <mach/mt_gpio.h>
#endif
#include <cust_gpio_usage.h>
#ifdef BUILD_LK
#define LCD_DEBUG(fmt)  dprintf(CRITICAL,fmt)
#else
#define LCD_DEBUG(fmt)  printk(fmt)
#endif
static unsigned char lcd_id_pins_value = 0xFF;
const static unsigned char LCD_MODULE_ID = 0x00;
// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------
#define LCM_DSI_CMD_MODE									1
#define FRAME_WIDTH  										(720)
#define FRAME_HEIGHT 										(1280)


#define REGFLAG_DELAY             							0xFC
#define REGFLAG_END_OF_TABLE      							0xFD   // END OF REGISTERS MARKER


#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif
static unsigned int lcm_esd_test = FALSE;      ///only for ESD test

//avoid 12.5% duty brightness value for TI backlight driver chip bug
static unsigned int MIN_VALUE_DUTY_ONE_EIGHT = 29;
static unsigned int MAX_VALUE_DUTY_ONE_EIGHT = 34;
//avoid 25% duty brightness value for TI backlight driver chip bug
static unsigned int MIN_VALUE_DUTY_ONE_FOUR = 59;
static unsigned int MAX_VALUE_DUTY_ONE_FOUR = 69;
//avoid 50% duty brightness value for TI backlight driver chip bug
static unsigned int MIN_VALUE_DUTY_ONE_TWO = 123;
static unsigned int MAX_VALUE_DUTY_ONE_TWO = 133;
// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

const static unsigned int BL_MIN_LEVEL =20;
static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)										lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   			lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)    

struct LCM_setting_table {
    unsigned char cmd;
    unsigned char count;
    unsigned char para_list[64];
};
//update initial param for IC nt35520 0.01
static struct LCM_setting_table lcm_initialization_setting_tm[] = {
       {0xFF,  4,  {0xAA,0x55,0xA5,0x80}},
	//f00208919 20130817 for boe flinker
    {0x6F,  1,  {0x13}},
    {0xF7,  1,  {0x00}},

    {0xf0,  5,  {0x55,0xaa,0x52,0x08,0x00}},
	{0x6f,  1,  {0x02}},
	{0xb8,  1,  {0x0c}},
	{0xbb,  2,  {0x11,0x11}},
	{0xbc,  2,  {0x00,0x00}},
	{0xb6,  1,  {0x01}},
	{0xb1,  2,  {0x7a,0x21}},
	
	{0xf0,  5,  {0x55,0xaa,0x52,0x08,0x01}},
	{0xb0,  2,  {0x09,0x09}},
	{0xb1,  2,  {0x09,0x09}},
	{0xbc,  2,  {0x98,0x00}},
	{0xbd,  2,  {0x98,0x00}},
	{0xca,  1,  {0x00}},
	{0xc0,  1,  {0x04}},
	{0xb5,  2,  {0x03,0x03}},

	{0xb3,  2,  {0x1b,0x1b}},
	{0xb4,  2,  {0x0f,0x0f}},
	{0xb9,  2,  {0x26,0x26}},
	{0xba,  2,  {0x24,0x24}},
	
	{0xf0,  5,  {0x55,0xaa,0x52,0x08,0x02}},
	{0xee,  1,  {0x01}},
	{0xb0,  16,  {0x00,0x20,0x00,0x69,0x00,0xac,0x00,0xce,0x00,0xe7,0x01,0x11,0x01,0x30,0x01,0x61}},
	{0xb1,  16,  {0x01,0x86,0x01,0xc2,0x01,0xef,0x02,0x37,0x02,0x6f,0x02,0x71,0x02,0xa5,0x02,0xde}},
	{0xb2,  16,  {0x03,0x01,0x03,0x2E,0x03,0x4B,0x03,0x6C,0x03,0x81,0x03,0x95,0x03,0xA0,0x03,0xB2}},
	{0xb3,  4,  {0x03,0xB7,0x03,0xFF}},
	
	{0xf0,  5,  {0x55,0xAA,0x52,0x08,0x06}},
	
	{0xb0,  2,  {0x18,0x16}},
	{0xb1,  2,  {0x12,0x10}},
	{0xb2,  2,  {0x00,0x02}},
	{0xb3,  2,  {0x31,0x31}},
	{0xb4,  2,  {0x31,0x34}},
	{0xb5,  2,  {0x34,0x31}},
	{0xb6,  2,  {0x31,0x33}},
	{0xb7,  2,  {0x33,0x33}},
	{0xb8,  2,  {0x31,0x08}},
	{0xb9,  2,  {0x2E,0x2D}},
	{0xba,  2,  {0x2D,0x2E}},
	{0xbb,  2,  {0x09,0x31}},
	{0xbc,  2,  {0x33,0x33}},
	{0xbd,  2,  {0x33,0x31}},
	{0xbe,  2,  {0x31,0x34}},
	{0xbf,  2,  {0x34,0x31}},
	{0xc0,  2,  {0x31,0x31}},
	{0xc1,  2,  {0x03,0x01}},
	{0xc2,  2,  {0x11,0x13}},
	{0xc3,  2,  {0x17,0x19}},
	{0xe5,  2,  {0x31,0x31}},
	
	{0xc4,  2,  {0x11,0x13}},
	{0xc5,  2,  {0x17,0x19}},
	{0xc6,  2,  {0x03,0x01}},
	{0xc7,  2,  {0x31,0x31}},
	{0xc8,  2,  {0x31,0x34}},
	{0xc9,  2,  {0x34,0x31}},
	{0xca,  2,  {0x31,0x33}},
	{0xcb,  2,  {0x33,0x33}},
	{0xcc,  2,  {0x31,0x09}},
	{0xcd,  2,  {0x2D,0x2E}},
	{0xce,  2,  {0x2E,0x2D}},
	{0xcf,  2,  {0x08,0x31}},
	{0xd0,  2,  {0x33,0x33}},
	{0xd1,  2,  {0x33,0x31}},
	{0xd2,  2,  {0x31,0x34}},
	{0xd3,  2,  {0x34,0x31}},
	{0xd4,  2,  {0x31,0x31}},
	{0xd5,  2,  {0x00,0x02}},
	{0xd6,  2,  {0x18,0x16}},
	{0xd7,  2,  {0x12,0x10}},
	
	{0xd8,  5,  {0x00,0x00,0x00,0x00,0x00}},
	{0xd9,  5,  {0x00,0x00,0x00,0x00,0x00}},
	{0xe7,  1,  {0x00}},
	
	{0xf0,  5,  {0x55,0xAA,0x52,0x08,0x05}},
	{0xed,  1,  {0x30}},
	
	{0xf0,  5,  {0x55,0xAA,0x52,0x08,0x03}},
	{0xb1,  2,  {0x00,0x00}},
	{0xb0,  2,  {0x00,0x00}},
	
	{0xf0,  5,  {0x55,0xAA,0x52,0x08,0x05}},
	{0xe5,  1,  {0x00}},
	
	{0xf0,  5,  {0x55,0xAA,0x52,0x08,0x05}},
	{0xb0,  2,  {0x17,0x06}},
	{0xb8,  1,  {0x00}},
	
	{0xbd,  5,  {0x03,0x03,0x01,0x00,0x03}},
	{0xb1,  2,  {0x17,0x06}},
	{0xb9,  2,  {0x00,0x03}},
	{0xb2,  2,  {0x17,0x06}},
	{0xba,  2,  {0x00,0x00}},
	{0xb3,  2,  {0x17,0x06}},
	{0xbb,  2,  {0x00,0x00}},
	{0xb4,  2,  {0x17,0x06}},
	{0xb5,  2,  {0x17,0x06}},
	{0xb6,  2,  {0x17,0x06}},
	{0xb7,  2,  {0x17,0x06}},
	{0xbc,  2,  {0x00,0x03}},
	{0xe5,  1,  {0x06}},
	{0xe6,  1,  {0x06}},
	{0xe7,  1,  {0x06}},
	{0xe8,  1,  {0x06}},
	{0xe9,  1,  {0x06}},
	{0xea,  1,  {0x06}},
	{0xeb,  1,  {0x06}},
	{0xec,  1,  {0x06}},
	
	{0xf0,  5,  {0x55,0xAA,0x52,0x08,0x05}},
	{0xc0,  1,  {0x0f}},
	{0xc1,  1,  {0x0d}},
	{0xc2,  1,  {0x0f}},
	{0xc3,  1,  {0x0d}},
	
	{0xf0,  5,  {0x55,0xAA,0x52,0x08,0x03}},
	{0xb2,  5,  {0x05,0x00,0x00,0x00,0x00}},
	{0xb3,  5,  {0x05,0x00,0x00,0x00,0x00}},
	{0xb4,  5,  {0x05,0x00,0x00,0x00,0x00}},
	{0xb5,  5,  {0x05,0x00,0x00,0x00,0x00}},
	
	{0xf0,  5,  {0x55,0xAA,0x52,0x08,0x05}},
	{0xc4,  1,  {0x14}},
	{0xc5,  1,  {0x14}},
	{0xc6,  1,  {0x14}},
	{0xc7,  1,  {0x14}},
	
	{0xf0,  5,  {0x55,0xAA,0x52,0x08,0x03}},
	{0xb6,  5,  {0x05,0x00,0x00,0x00,0x00}},
	{0xb7,  5,  {0x05,0x00,0x00,0x00,0x00}},
	{0xb8,  5,  {0x05,0x00,0x00,0x00,0x00}},
	{0xb9,  5,  {0x05,0x00,0x00,0x00,0x00}},
	
	{0xf0,  5,  {0x55,0xAA,0x52,0x08,0x05}},
	{0xc8,  2,  {0x0B,0x20}},
	{0xc9,  2,  {0x07,0x20}},
	{0xca,  2,  {0x0B,0x00}},
	{0xcb,  2,  {0x07,0x00}},
	
	{0xf0,  5,  {0x55,0xAA,0x52,0x08,0x03}},
	{0xba,  5,  {0x53,0x00,0x00,0x00,0x00}},
	{0xbb,  5,  {0x53,0x00,0x00,0x00,0x00}},
	{0xbc,  5,  {0x53,0x00,0x00,0x00,0x00}},
	{0xbd,  5,  {0x53,0x00,0x00,0x00,0x00}},
	
	{0xf0,  5,  {0x55,0xAA,0x52,0x08,0x05}},
	{0xd1,  5,  {0x00,0x05,0x04,0x07,0x10}},
	{0xd2,  5,  {0x00,0x05,0x08,0x07,0x10}},
	{0xd3,  5,  {0x00,0x00,0x0A,0x07,0x10}},
	{0xd4,  5,  {0x00,0x00,0x0A,0x07,0x10}},
	
	{0xf0,  5,  {0x55,0xAA,0x52,0x08,0x05}},
	{0xd0,  7,  {0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{0xd5,  11,  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{0xd6,  11,  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{0xd7,  11,  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{0xd8,  5,  {0x00,0x00,0x00,0x00,0x00}},
	
	{0xf0,  5,  {0x55,0xAA,0x52,0x08,0x03}},
	{0xc4,  1,  {0x60}},
	{0xc5,  1,  {0x40}},
	{0xc6,  1,  {0x60}},
	{0xc7,  1,  {0x40}},
	
	{0x35,  1,  {0x00}},
	
	{0x11, 1,   {0x00}},
	{REGFLAG_DELAY, 120, {}},

	{0x29, 1, {0x00}},
	
	
	
};
							
#if 0
static struct LCM_setting_table lcm_set_window[] = {
	{0x2A,	4,	{0x00, 0x00, (FRAME_WIDTH>>8), (FRAME_WIDTH&0xFF)}},
	{0x2B,	4,	{0x00, 0x00, (FRAME_HEIGHT>>8), (FRAME_HEIGHT&0xFF)}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif
static struct LCM_setting_table lcm_sleep_out_setting[] = {
    //Sleep Out
    {0x11, 1, {0x00}},
    {REGFLAG_DELAY, 120, {}},

    // Display ON
    {0x29, 1, {0x00}},
    {REGFLAG_DELAY, 20, {}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
    // Display off sequence
    {0x28, 1, {0x00}},
    {REGFLAG_DELAY, 20, {}},

    // Sleep Mode On
    {0x10, 1, {0x00}},
    {REGFLAG_DELAY, 120, {}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
    unsigned int i;

    for(i = 0; i < count; i++)
    {
        unsigned cmd;
        cmd = table[i].cmd;

        switch (cmd) {

            case REGFLAG_DELAY :
                if(table[i].count <= 10)
                    mdelay(table[i].count);
                else
                    msleep(table[i].count);
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
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}


static void lcm_get_params(LCM_PARAMS *params)
{
		memset(params, 0, sizeof(LCM_PARAMS));
	
		params->type   = LCM_TYPE_DSI;

		params->width  = FRAME_WIDTH;
		params->height = FRAME_HEIGHT;

#if (LCM_DSI_CMD_MODE)
    params->dsi.mode   = CMD_MODE;
#else
    params->dsi.mode   = SYNC_PULSE_VDO_MODE;
#endif

		// DSI
		/* Command mode setting */
		params->dsi.LANE_NUM				= LCM_FOUR_LANE;
		//The following defined the fomat for data coming from LCD engine.
		params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
		params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
		params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
		params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

		// Highly depends on LCD driver capability.
		params->dsi.packet_size=256;
		//video mode timing

    params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

    params->dsi.vertical_sync_active				= 2;
    params->dsi.vertical_backporch					= 8;
    params->dsi.vertical_frontporch					= 10;
    params->dsi.vertical_active_line				= FRAME_HEIGHT;

    params->dsi.horizontal_sync_active				= 10;
    params->dsi.horizontal_backporch				= 20;
    params->dsi.horizontal_frontporch				= 40;
    params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

    /*BEGIN PN:DTS2013013101431 modified by s00179437 , 2013-01-31*/
    //improve clk quality
    params->dsi.PLL_CLOCK = 240; //this value must be in MTK suggested table
    /*END PN:DTS2013013101431 modified by s00179437 , 2013-01-31*/

}
/*to prevent electric leakage*/
static void lcm_id_pin_handle(void)
{
    mt_set_gpio_pull_select(GPIO_DISP_ID0_PIN,GPIO_PULL_DOWN);
    mt_set_gpio_pull_select(GPIO_DISP_ID1_PIN,GPIO_PULL_DOWN);
}

static void lcm_init_tm(void)
{
	 //enable VSP & VSN
	lcm_util.set_gpio_out(GPIO_LCD_BIAS_ENP_PIN, GPIO_OUT_ONE);
	lcm_util.set_gpio_out(GPIO_LCD_BIAS_ENN_PIN, GPIO_OUT_ONE);
	msleep(50);
	//reset low to high
    lcm_util.set_gpio_out(GPIO_DISP_LRSTB_PIN, GPIO_OUT_ONE);
    mdelay(5);   
    lcm_util.set_gpio_out(GPIO_DISP_LRSTB_PIN, GPIO_OUT_ZERO);
    mdelay(5); 
    lcm_util.set_gpio_out(GPIO_DISP_LRSTB_PIN, GPIO_OUT_ONE);
    msleep(10);

    lcm_id_pin_handle();
	// when phone initial , config output high, enable backlight drv chip  
	lcm_util.set_gpio_out(GPIO_LCD_DRV_EN_PIN, GPIO_OUT_ONE); 
    push_table(lcm_initialization_setting_tm, sizeof(lcm_initialization_setting_tm) / sizeof(struct LCM_setting_table), 1);  

	LCD_DEBUG("uboot:tm_nt35520_lcm_init\n");

}


static void lcm_suspend(void)
{
    /*BEGIN PN:DTS2013061501413 ,  Modified by s00179437 , 2013-6-15*/
    //Back to MP.P7 baseline , solve LCD display abnormal On the right
    // when phone sleep , config output low, disable backlight drv chip  
    lcm_util.set_gpio_out(GPIO_LCD_DRV_EN_PIN, GPIO_OUT_ZERO);
    /*END PN:DTS2013061501413 ,  Modified by s00179437 , 2013-6-15*/
    push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);
    //reset low
    lcm_util.set_gpio_out(GPIO_DISP_LRSTB_PIN, GPIO_OUT_ZERO);
    mdelay(5);
    //disable VSP & VSN
	lcm_util.set_gpio_out(GPIO_LCD_BIAS_ENP_PIN, GPIO_OUT_ZERO);
	lcm_util.set_gpio_out(GPIO_LCD_BIAS_ENN_PIN, GPIO_OUT_ZERO);
    mdelay(5);	

    LCD_DEBUG("kernel:tm_nt35520_lcm_suspend\n");

}
static void lcm_resume_tm(void)
{

    //enable VSP & VSN
	lcm_util.set_gpio_out(GPIO_LCD_BIAS_ENP_PIN, GPIO_OUT_ONE);
	lcm_util.set_gpio_out(GPIO_LCD_BIAS_ENN_PIN, GPIO_OUT_ONE);
    msleep(50);

    //reset low to high
    lcm_util.set_gpio_out(GPIO_DISP_LRSTB_PIN, GPIO_OUT_ONE);
    mdelay(5);   
    lcm_util.set_gpio_out(GPIO_DISP_LRSTB_PIN, GPIO_OUT_ZERO);
    mdelay(5); 
    lcm_util.set_gpio_out(GPIO_DISP_LRSTB_PIN, GPIO_OUT_ONE);
    msleep(10);	

    push_table(lcm_initialization_setting_tm, sizeof(lcm_initialization_setting_tm) / sizeof(struct LCM_setting_table), 1);
    /*BEGIN PN:DTS2013061501413 ,  Modified by s00179437 , 2013-6-15*/
    //Back to MP.P7 baseline , solve LCD display abnormal On the right
    //when sleep out, config output high ,enable backlight drv chip  
    lcm_util.set_gpio_out(GPIO_LCD_DRV_EN_PIN, GPIO_OUT_ONE);
    /*END PN:DTS2013061501413 ,  Modified by s00179437 , 2013-6-15*/
    LCD_DEBUG("kernel:tm_nt35520_lcm_resume\n");
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
         /*BEGIN PN:DTS2013013101431 modified by s00179437 , 2013-01-31*/
         //delete high speed packet
	//data_array[0]=0x00290508;
	//dsi_set_cmdq(data_array, 1, 1);
         /*END PN:DTS2013013101431 modified by s00179437 , 2013-01-31*/
	
	data_array[0]= 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);

}
/*BEGIN PN:DTS2013011703806, Added by y00213338 , 2013-01-13*/
/******************************************************************************
  Function:       lcm_set_pwm_level_XXX
  Description:    set different values for each LCD
  Input:          level
  Output:         NONE
  Return:         mapped_level
  Others:         none
******************************************************************************/

static unsigned int lcm_set_pwm_level_tm(unsigned int level )
{
    unsigned int mapped_level = 0;
    if( 0 == level)
    {
        mapped_level = level;
    }
    else if(( 0 < level ) && (  BL_MIN_LEVEL > level ))
    {
        //Some 3rd APK will set values < 20 , set value(1-19) is 8
        mapped_level = 8;
    }
    else
    {
        //Reduce brightness for power consumption , MAX value > 350cd/cm2
        mapped_level = (unsigned int)((level-8) * 7 /10);
    }
    /*BEGIN PN:DTS2013042409674, Added by s00179437 , 2013-04-24*/
    if((mapped_level >= MIN_VALUE_DUTY_ONE_EIGHT) && (mapped_level <= MAX_VALUE_DUTY_ONE_EIGHT )) //12.5% duty shanshuo
    {
        //avoid 12.5% duty brightness value for TI backlight driver chip bug
        mapped_level = MIN_VALUE_DUTY_ONE_EIGHT-1;
    }
    else if((mapped_level >= MIN_VALUE_DUTY_ONE_FOUR) && (mapped_level <= MAX_VALUE_DUTY_ONE_FOUR))
    {
        //avoid 25% duty brightness value for TI backlight driver chip bug
        mapped_level = MIN_VALUE_DUTY_ONE_FOUR-1;
    }
    else if((mapped_level >= MIN_VALUE_DUTY_ONE_TWO) && (mapped_level <= MAX_VALUE_DUTY_ONE_TWO))
    {
        //avoid 50% duty brightness value for TI backlight driver chip bug
        mapped_level = MIN_VALUE_DUTY_ONE_TWO-1;
    }
   
    #ifdef BUILD_LK
        dprintf(CRITICAL,"uboot:tm_nt35520_lcm_set_pwm mapped_level = %d,level=%d\n",mapped_level,level);
    #else
        printk("kernel:tm_nt35520_lcm_set_pwm mapped_level = %d,level=%d\n",mapped_level,level);
    #endif
    return mapped_level;
}
static unsigned int lcm_compare_id_tm(void)
{
/* BEGIN PN:SPBB-1229 ,Modified by b00214920, 2013/01/07*/
    unsigned char module_id = which_lcd_module_triple();
    return ((LCD_MODULE_ID == module_id )? 1 : 0);
/* END PN:SPBB-1229 ,Modified by b00214920, 2013/01/07*/
}
LCM_DRIVER nt35520_hd720_tm_lcm_drv =
{
    .name           = "nt35520_hd720_dsi_cmd_tm",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params     = lcm_get_params,
    .init           = lcm_init_tm,/*tianma init fun.*/
    .suspend        = lcm_suspend,
    .resume         = lcm_resume_tm,
     .compare_id     = lcm_compare_id_tm,
#if (LCM_DSI_CMD_MODE)
    .update         = lcm_update,
//    .set_backlight  = lcm_setbacklight,
    //.set_pwm_level = lcm_set_pwm_level_tm,
//  .set_pwm        = lcm_setpwm,
//  .get_pwm        = lcm_getpwm,
//  .esd_check      = lcm_esd_check,
//  .esd_recover    = lcm_esd_recover,
#endif
   
};
/* END PN:DTS2013053103858 , Added by d00238048, 2013.05.31*/
