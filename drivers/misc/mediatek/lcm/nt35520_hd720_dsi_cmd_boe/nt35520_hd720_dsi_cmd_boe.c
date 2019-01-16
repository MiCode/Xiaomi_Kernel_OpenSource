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
const static unsigned char LCD_MODULE_ID = 0x01; //  haobing modified 2013.07.11
// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------
#define LCM_DSI_CMD_MODE									1
#define FRAME_WIDTH  										(720)
#define FRAME_HEIGHT 										(1280)


#define REGFLAG_DELAY             								0xFC
#define REGFLAG_END_OF_TABLE      							0xFD   // END OF REGISTERS MARKER


#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif
static unsigned int lcm_esd_test = FALSE;      ///only for ESD test
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
static struct LCM_setting_table lcm_initialization_setting_boe[] = {
       {0xFF,  4,  {0xAA,0x55,0xA5,0x80}},
	{0xF4,  1,  {0x00}},
	{0x6F,  1,  {0x19}},
	{0xF7,  1,  {0x02}},
	{0x6F,  1,  {0x02}},
	{0xF7,  1,  {0x2D}},
	{0x6F,  1,  {0x13}},
	{0xF7,  1,  {0x00}},
	
	{0xF0,  5,  {0x55,0xAA,0x52,0x08,0x00}},
	{0xC8,  1,  {0x80}},
	{0xB1,  2,  {0x7a,0x21}},
	{0xBC,  2,  {0x00,0x00}},
	{0xC6,  2,  {0x21,0x18}},
	{0xB6,  1,  {0x08}},
	{0xBB,  2,  {0x11, 0x11}},
	{0xBE,  2,  {0x11, 0x11}},

	{0xF0,  5,  {0x55,0xAA,0x52,0x08,0x01}},
	{0xB0,  2,  {0x05,0x05}},              
	{0xB1,  2,  {0x05,0x05}},
	{0xBC,  2,  {0xB8,0x00}},         
	{0xBD,  2,  {0xB8,0x00}}, 
	{0xCA,  1,  {0x00}},              
	{0xB5,  2,  {0x03,0x03}}, 

	{0xB3,  2,  {0x19,0x19}},              
	{0xB4,  2,  {0x19,0x19}},
	{0xB9,  2,  {0x24,0x24}},         
	{0xBA,  2,  {0x24,0x24}}, 
	{0xC0,  1,  {0x04}},  

	{0xF0,  5,  {0x55,0xAA,0x52,0x08,0x02}},
	{0xEE,  1,  {0x01}}, 

	{0xB0,	16,	{0x00,0x00,0x00,0x9D,0x00,0xAE,0x00,0xC1,0x00,0xD6,0x00,0xE9,0x01,0x09,0x01,0x3E}},
	{0xB1,	16,	{0x01,0x64,0x01,0xA5,0x01,0xDC,0x02,0x2F,0x02,0x64,0x02,0x65,0x02,0x92,0x02,0xBA}},
	{0xB2,	16,	{0x02,0xD1,0x02,0xFB,0x03,0x19,0x03,0x46,0x03,0x61,0x03,0x90,0x03,0xA8,0x03,0xCF}},
	{0xB3,	4,	{0x03,0xFA,0x03,0xFE}},

	{0xF0,  5,  {0x55,0xAA,0x52,0x08,0x03}},
	{0xB0,  2,  {0x20,0x00}},         
	{0xB1,  2,  {0x20,0x00}},
	{0xB2,  5,  {0x05,0x00,0x0A,0x00,0x00}},
	{0xB3,  5,  {0x05,0x00,0x0A,0x00,0x00}},
	{0xB4,  5,  {0x05,0x00,0x0A,0x00,0x00}},
	{0xB5,  5,  {0x05,0x00,0x0A,0x00,0x00}},
	{0xB6,  5,  {0x02,0x00,0x0A,0x00,0x00}},
	{0xB7,  5,  {0x02,0x00,0x0A,0x00,0x00}},
	{0xB8,  5,  {0x02,0x00,0x0A,0x00,0x00}},
	{0xB9,  5,  {0x02,0x00,0x0A,0x00,0x00}},
	{0xBA,  5,  {0x53,0x00,0x0A,0x00,0x00}},
	{0xBB,  5,  {0x53,0x00,0x0A,0x00,0x00}},
	{0xBC,  5,  {0x53,0x00,0x0A,0x00,0x00}},
	{0xBD,  5,  {0x53,0x00,0x0A,0x00,0x00}},
	
	{0xC4,  1,  {0x60}},  
	{0xC5,  1,  {0x40}},  
	{0xC6,  1,  {0x64}},  
	{0xC7,  1,  {0x44}},  

	{0xB0,  2,  {0x00,0x00}},  
	{0xB1,  2,  {0x00,0x00}}, 
	
	{0xC0,	4,	{0x00,0x34,0x00,0x00}},
	{0xC1,	4,	{0x00,0x34,0x00,0x00}},
	{0xC2,	4,	{0x00,0x34,0x00,0x00}},
	{0xC3,	4,	{0x00,0x34,0x00,0x00}},

	{0xF0,  5,  {0x55,0xAA,0x52,0x08,0x05}},
	{0xED,  1,  {0x30}}, 

	{0xB0,  2,  {0x17,0x06}}, 
	{0xB8,  1,  {0x00}},
	{0xBD,  5,  {0x03,0x03,0x00,0x00,0x03}},
	{0xB1,  2,  {0x17,0x06}},
	{0xB9,  2,  {0x00,0x03}},
	{0xB2,  2,  {0x17,0x06}},
	{0xBA,  2,  {0x00,0x00}},
	{0xB3,  2,  {0x17,0x06}},
	{0xBB,  2,  {0x00,0x00}},
	{0xB4,  2,  {0x17,0x06}},
	{0xB5,  2,  {0x17,0x06}},
	{0xB6,  2,  {0x17,0x06}},
	{0xB7,  2,  {0x17,0x06}},
	{0xBC,  2,  {0x00,0x03}},

	{0xE5,	1,	{0x06}},
	{0xE6,	1,	{0x06}},
	{0xE7,	1,	{0x06}},
	{0xE8,	1,	{0x06}},
	{0xE9,	1,	{0x06}},
	{0xEA,	1,	{0x06}},
	{0xEB,	1,	{0x06}},
	{0xEC,	1,	{0x06}},

	{0xC0,	1,	{0x0F}},
	{0xC1,	1,	{0x0D}},
	{0xC2,	1,	{0x23}},
	{0xC3,	1,	{0x40}},

	{0xC4,	1,	{0x84}},
	{0xC5,	1,	{0x82}},
	{0xC6,	1,	{0x82}},
	{0xC7,	1,	{0x80}},

	{0xC8,	2,	{0x0B,0x30}},
	{0xC9,	2,	{0x05,0x10}},
	{0xCA,	2,	{0x01,0x10}},
	{0xCB,	2,	{0x01,0x10}},

	{0xD1,  5,  {0x00,0x05,0x05,0x07,0x00}},
	{0xD2,  5,  {0x00,0x05,0x09,0x03,0x00}},
	{0xD3,  5,  {0x00,0x00,0x6A,0x07,0x10}},
	{0xD4,  5,  {0x30,0x00,0x6A,0x07,0x10}},

	{0xF0,  5,  {0x55,0xAA,0x52,0x08,0x06}},
	{0xB0,  2,  {0x10,0x12}}, 
	{0xB1,  2,  {0x14,0x16}},
	{0xB2,  2,  {0x00,0x02}},
	{0xB3,  2,  {0x31,0x31}},
	{0xB4,  2,  {0x31,0x34}},
	{0xB5,  2,  {0x34,0x34}},
	{0xB6,  2,  {0x34,0x31}},
	{0xB7,  2,  {0x31,0x31}},
	{0xB8,  2,  {0x31,0x31}}, 
	{0xB9,  2,  {0x2D,0x2E}},
	{0xBA,  2,  {0x2E,0x2D}},
	{0xBB,  2,  {0x31,0x31}},
	{0xBC,  2,  {0x31,0x31}},
	{0xBD,  2,  {0x31,0x34}},
	{0xBE,  2,  {0x34,0x34}},
	{0xBF,  2,  {0x34,0x31}},

	{0xC0,  2,  {0x31,0x31}}, 
	{0xC1,  2,  {0x03,0x01}},
	{0xC2,  2,  {0x17,0x15}},
	{0xC3,  2,  {0x13,0x11}},
	{0xE5,  2,  {0x31,0x31}}, 
	{0xC4,  2,  {0x17,0x15}},
	{0xC5,  2,  {0x13,0x11}},
	{0xC6,  2,  {0x03,0x01}},
	{0xC7,  2,  {0x31,0x31}},
	{0xC8,  2,  {0x31,0x34}}, 
	{0xC9,  2,  {0x34,0x34}},
	{0xCA,  2,  {0x34,0x31}},
	{0xCB,  2,  {0x31,0x31}},
	{0xCC,  2,  {0x31,0x31}},
	{0xCD,  2,  {0x2E,0x2D}},
	{0xCE,  2,  {0x2D,0x2E}},
	{0xCF,  2,  {0x31,0x31}},
	
	{0xD0,  2,  {0x31,0x31}}, 
	{0xD1,  2,  {0x31,0x34}},
	{0xD2,  2,  {0x34,0x34}},
	{0xD3,  2,  {0x34,0x31}},
	{0xD4,  2,  {0x31,0x31}},
	{0xD5,  2,  {0x00,0x02}},
	{0xD6,  2,  {0x10,0x12}},
	{0xD7,  2,  {0x14,0x16}},
	{0xE6,  2,  {0x32,0x32}}, 
	{0xD8,  5,  {0x00,0x00,0x00,0x00,0x00}},
	{0xD9,  5,  {0x00,0x00,0x00,0x00,0x00}},
	{0xE7,  1,  {0x00}}, 
	
	{0x2A,  4,  {0x00,0x00,0x02,0xCF}},
	{0x11,  1,  {0x00}},
	{REGFLAG_DELAY, 120, {}},
	{0x29,  1,  {0x00}},
	{0x35,  1,  {0x00}},
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
	params->dsi.data_format.color_order 	= LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   	= LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     	= LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format      		= LCM_DSI_FORMAT_RGB888;

	// Highly depends on LCD driver capability.
	params->dsi.packet_size=256;
	//video mode timing

	params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active				= 2;
	params->dsi.vertical_backporch					= 8;
	params->dsi.vertical_frontporch					= 10;
	params->dsi.vertical_active_line					= FRAME_HEIGHT;

	params->dsi.horizontal_sync_active				= 10;
	params->dsi.horizontal_backporch				= 20;
	params->dsi.horizontal_frontporch				= 40;
	params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

	//begin:haobing modified
	/*BEGIN PN:DTS2013013101431 modified by s00179437 , 2013-01-31*/
	//improve clk quality
	//params->dsi.PLL_CLOCK = 240; //this value must be in MTK suggested table
	params->dsi.pll_div1=0;		// div1=0,1,2,3;div1_real=1,2,4,4 ----0: 546Mbps  1:273Mbps
	params->dsi.pll_div2=1;		// div2=0,1,2,3;div1_real=1,2,4,4	
	params->dsi.fbk_div =21;    	// fref=26MHz, fvco=fref*(fbk_div)*2/(div1_real*div2_real)	
	/*END PN:DTS2013013101431 modified by s00179437 , 2013-01-31*/
	//end:haobing modified

}
/*to prevent electric leakage*/
static void lcm_id_pin_handle(void)
{
    mt_set_gpio_pull_select(GPIO_DISP_ID0_PIN,GPIO_PULL_UP);
    mt_set_gpio_pull_select(GPIO_DISP_ID1_PIN,GPIO_PULL_DOWN);//GPIO_PULL_UP  modified by haobing
}

static void lcm_init_boe(void)
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
    push_table(lcm_initialization_setting_boe, sizeof(lcm_initialization_setting_boe) / sizeof(struct LCM_setting_table), 1);  


    LCD_DEBUG("uboot:boe_nt35520_lcm_init\n");

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

    LCD_DEBUG("uboot:boe_nt35520_lcm_suspend\n");

}
static void lcm_resume_boe(void)
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

    push_table(lcm_initialization_setting_boe, sizeof(lcm_initialization_setting_boe) / sizeof(struct LCM_setting_table), 1);
    /*BEGIN PN:DTS2013061501413 ,  Modified by s00179437 , 2013-6-15*/
    //Back to MP.P7 baseline , solve LCD display abnormal On the right
    //when sleep out, config output high ,enable backlight drv chip  
    lcm_util.set_gpio_out(GPIO_LCD_DRV_EN_PIN, GPIO_OUT_ONE);
    /*END PN:DTS2013061501413 ,  Modified by s00179437 , 2013-6-15*/

    LCD_DEBUG("kernel:boe_nt35520_lcm_resume\n");

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

static unsigned int lcm_compare_id_boe(void)
{
/* BEGIN PN:SPBB-1229 ,Modified by b00214920, 2013/01/07*/
    	unsigned char LCD_ID_value = 0;
	LCD_ID_value = which_lcd_module_triple();

	if(LCD_MODULE_ID == LCD_ID_value)
    	{
        	return 1;
    	}
    	else
    	{
        	return 0;
    	}
/* END PN:SPBB-1229 ,Modified by b00214920, 2013/01/07*/
}
LCM_DRIVER nt35520_hd720_boe_lcm_drv =
{
    .name           = "nt35520_hd720_dsi_cmd_boe",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params     = lcm_get_params,
    .init           = lcm_init_boe,/*tianma init fun.*/
    .suspend        = lcm_suspend,
    .resume         = lcm_resume_boe,
     .compare_id     = lcm_compare_id_boe,
#if (LCM_DSI_CMD_MODE)
    .update         = lcm_update,
#endif
   
};
/* END PN:DTS2013053103858 , Added by d00238048, 2013.05.31*/
