#ifndef BUILD_LK
#include <linux/string.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
	#include <platform/mt_gpio.h>
#elif defined(BUILD_UBOOT)
	#include <asm/arch/mt_gpio.h>
#else
	#include <mach/mt_gpio.h>
#endif
// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------
#define FRAME_WIDTH  											(1080)
#define FRAME_HEIGHT 											(1920)

#define SET_RESET_PIN(v)    									(lcm_util.set_reset_pin((v)))
#define SET_GPIO_OUT(n, v)	        							(lcm_util.set_gpio_out((n), (v)))
#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	    lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)			lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)											lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)						lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)											lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#define LCD_LDO_ENN_GPIO_PIN          							(GPIO_LCD_ENN_PIN)//168
#define LCD_LDO_ENP_GPIO_PIN          							(GPIO_LCD_ENP_PIN)//mocku=20;proto=83

static LCM_UTIL_FUNCS   lcm_util = {0};
static unsigned int lcd_id;

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
	//params->dbi.te_mode 				= LCM_DBI_TE_MODE_VSYNC_ONLY;
	//params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;
	params->dsi.mode   					= BURST_VDO_MODE;
	// DSI
	/* Command mode setting */
	params->dsi.LANE_NUM				= LCM_FOUR_LANE;
	//The following defined the fomat for data coming from LCD engine.
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

	// Highly depends on LCD driver capability.
	// Not support in MT6573
	params->dsi.packet_size=256;
	// Video mode setting
	params->dsi.intermediat_buffer_num = 0;//because DSI/DPI HW design change, this parameters should be 0 when video mode in MT658X; or memory leakage
	params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.word_count=720*3;

	params->dsi.vertical_sync_active	= 1;
	params->dsi.vertical_backporch		= 4;
	params->dsi.vertical_frontporch		= 3;
	params->dsi.vertical_active_line	= FRAME_HEIGHT;

	params->dsi.horizontal_sync_active	= 3;
	params->dsi.horizontal_backporch	= 60;
	params->dsi.horizontal_frontporch	= 94;
	params->dsi.horizontal_active_pixel	= FRAME_WIDTH;
#if 0
	// Bit rate calculation
	params->dsi.pll_div1=0;		// div1=0,1,2,3;div1_real=1,2,4,4 ----0: 546Mbps  1:273Mbps
	params->dsi.pll_div2=0;		// div2=0,1,2,3;div1_real=1,2,4,4
	params->dsi.fbk_div =16;    // fref=26MHz, fvco=fref*(fbk_div+1)*2/(div1_real*div2_real)
#else
	params->dsi.PLL_CLOCK=475;
#endif
}

static void init_lcm_registers(void)
{
	unsigned int data_array[16];

	data_array[0] = 0x04B02900;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00000500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00000500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00042902;
	data_array[1] = 0x100001C3;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x03B02900;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00000500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00000500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00361500;//0xC0361500
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00290500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00110500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(120);
}

static void lcm_init(void)
{
	lcm_util.set_gpio_mode(GPIO112, GPIO_MODE_00);
	lcm_util.set_gpio_dir(GPIO112, GPIO_DIR_OUT); 
	lcm_util.set_gpio_pull_enable(GPIO112, GPIO_PULL_DISABLE); 

	lcm_util.set_gpio_out(GPIO112 , 0);
	MDELAY(50);

	SET_GPIO_OUT(LCD_LDO_ENP_GPIO_PIN , 1);
	MDELAY(10);
	SET_GPIO_OUT(LCD_LDO_ENN_GPIO_PIN , 1);
	MDELAY(100);
    
	lcm_util.set_gpio_out(GPIO112 , 1);
	MDELAY(20);

	init_lcm_registers();
}

static void lcm_suspend(void)
{
	unsigned int data_array[16];

	data_array[0]=0x00280500; 
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(10);

	data_array[0] = 0x00100500; 
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(120);//delay more for 3 frames time  17*3=54ms

	data_array[0] = 0x04B02900;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00000500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00000500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x01B12900;//Deep standby
	dsi_set_cmdq(data_array, 1, 1);

	lcm_util.set_gpio_mode(GPIO112, GPIO_MODE_00);
	lcm_util.set_gpio_dir(GPIO112, GPIO_DIR_OUT); 
	lcm_util.set_gpio_pull_enable(GPIO112, GPIO_PULL_DISABLE); 
	lcm_util.set_gpio_out(GPIO112 , 0);

	MDELAY(2);
	SET_GPIO_OUT(LCD_LDO_ENN_GPIO_PIN , 0);
	MDELAY(10);
	SET_GPIO_OUT(LCD_LDO_ENP_GPIO_PIN , 0);
}

static void lcm_resume(void)
{

	lcm_init();

}

static unsigned int lcm_compare_id(void)
{
	unsigned int id=0;
	unsigned char buffer[5];
	unsigned int array[16];  
	int i;
    #ifdef BUILD_LK
		printf("%s\n", __func__);
    #else
		printk("%s\n", __func__);
    #endif
    
	lcm_util.set_gpio_mode(LCD_LDO_ENP_GPIO_PIN, GPIO_MODE_00);
	lcm_util.set_gpio_dir(LCD_LDO_ENP_GPIO_PIN, GPIO_DIR_OUT); 
	lcm_util.set_gpio_pull_enable(LCD_LDO_ENP_GPIO_PIN, GPIO_PULL_DISABLE); 

	lcm_util.set_gpio_mode(LCD_LDO_ENN_GPIO_PIN, GPIO_MODE_00);
	lcm_util.set_gpio_dir(LCD_LDO_ENN_GPIO_PIN, GPIO_DIR_OUT); 
	lcm_util.set_gpio_pull_enable(LCD_LDO_ENN_GPIO_PIN, GPIO_PULL_DISABLE); 

	lcm_util.set_gpio_mode(GPIO112, GPIO_MODE_00);
	lcm_util.set_gpio_dir(GPIO112, GPIO_DIR_OUT); 
	lcm_util.set_gpio_pull_enable(GPIO112, GPIO_PULL_DISABLE); 
	lcm_util.set_gpio_out(GPIO112 , 0);

	MDELAY(50);
	
	SET_GPIO_OUT(LCD_LDO_ENP_GPIO_PIN , 1);//power on +5
	MDELAY(10);
	SET_GPIO_OUT(LCD_LDO_ENN_GPIO_PIN , 1);//power on -5
	MDELAY(100);

	lcm_util.set_gpio_out(GPIO112 , 1);
	MDELAY(50);


    for(i=0;i<10;i++)
    {
	    array[0] = 0x00053700;// read id return two byte,version and id
    	dsi_set_cmdq(array, 1, 1);
	
	    read_reg_v2(0xBF, buffer, 5);
	    MDELAY(20);
	    lcd_id = (buffer[2] << 8 )| buffer[3];
	    if (lcd_id == 0x3111)
	        break;
	}
	
    #ifdef BUILD_LK
		printf("%s, LK r63311_jdi_diabloX debug: r63311_jdi_diabloX id = 0x%08x\n", __func__, id);
    #else
		printk("%s, kernel r63311_jdi_diabloX horse debug: r63311_jdi_diabloX id = 0x%08x\n", __func__, id);
    #endif

    if(lcd_id == 0x3311)
    	return 1;
    else
        return 0;


}
LCM_DRIVER r63311_fhd_dsi_vedio_lcm_drv =
{
    .name			= "r63311_jdi_diabloX",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params     = lcm_get_params,
    .init           = lcm_init,
    .suspend        = lcm_suspend,
    .resume         = lcm_resume,
    .compare_id   = lcm_compare_id,

    
};
