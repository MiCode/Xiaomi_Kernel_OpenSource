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
#ifdef BUILD_LK
#define LCD_DEBUG(fmt)  dprintf(CRITICAL,fmt)
#else
#define LCD_DEBUG(fmt)  printk(fmt)
#endif


const static unsigned char LCD_MODULE_ID = 0x02;
// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

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
    unsigned char para_list[128];
};
//update initial param for IC nt35521 0.01
static struct LCM_setting_table lcm_initialization_setting_tm[] = {
    {0xff,  4,  {0xaa,0x55,0xa5,0x80}},
	{0x6f,  2,  {0x11,0x00}},
	{0xf7,  2,  {0x20,0x00}},
    {0x6f,  1,  {0x11}},
    {0xf3,  1,  {0x01}},

	{0xf0,  5,  {0x55,0xaa,0x52,0x08,0x00}},
	{0xbd,  5,  {0x01,0xa0,0x0c,0x08,0x01}},
	{0x6f,  1,  {0x02}},
	{0xb8,  1,  {0x0c}},
	{0xbb,  2,  {0x11,0x11}},
	{0xbc,  2,  {0x00,0x00}},
	{0xb6,  1,  {0x01}},
	{0xf0,  5,  {0x55,0xaa,0x52,0x08,0x01}},
	{0xb0,  2,  {0x09,0x09}},
	{0xb1,  2,  {0x09,0x09}},
	{0xbc,  2,  {0x68,0x01}},
	{0xbd,  2,  {0x68,0x01}},
	{0xca,  1,  {0x00}},
	{0xc0,  1,  {0x04}},
	{0xb5,  2,  {0x03,0x03}},
	{0xbe,  1,  {0x5b}},
	{0xb3,  2,  {0x28,0x28}},
	{0xb4,  2,  {0x0f,0x0f}},
	{0xb9,  2,  {0x53,0x53}},
	{0xba,  2,  {0x15,0x15}},
	
	{0xf0,  5,  {0x55,0xaa,0x52,0x08,0x02}},
	{0xee,  1,  {0x01}},

	{0xb0,  16, {0x00,0x00,0x00,0x58,0x00,0x88,0x00,0xa7,0x00,0xc0,0x00,0xe8,0x01,0x08,0x01,0x3c}},
	{0xb1,  16, {0x01,0x63,0x01,0xa4,0x01,0xd6,0x02,0x26,0x02,0x67,0x02,0x69,0x02,0xa5,0x02,0xe3}},
	{0xb2,  16, {0x03,0x09,0x03,0x3a,0x03,0x5e,0x03,0x8a,0x03,0xa5,0x03,0xc4,0x03,0xd8,0x03,0xed}},
	{0xb3,  4,  {0x03,0xf7,0x03,0xfb}},

	{0x6f,  1,  {0x02}},
	{0xf7,  1,  {0x47}},

	{0x6f,  1,  {0x0a}},
	{0xf7,  1,  {0x02}},

	{0x6f,  1,  {0x17}},
	{0xf4,  1,  {0x70}},

	{0x6f,  1,  {0x11}},
	{0xf3,  1,  {0x01}},

	{0xf0,  5,  {0x55,0xaa,0x52,0x08,0x06}},
	{0xb0,  2,  {0x12,0x10}},
	{0xb1,  2,  {0x18,0x16}},
	{0xb2,  2,  {0x00,0x02}},
	{0xb3,  2,  {0x31,0x31}},
	{0xb4,  2,  {0x31,0x31}},
	{0xb5,  2,  {0x31,0x31}},
	{0xb6,  2,  {0x31,0x31}},
	{0xb7,  2,  {0x31,0x31}},
	{0xb8,  2,  {0x31,0x08}},
	{0xb9,  2,  {0x2e,0x2d}},
	{0xba,  2,  {0x2d,0x2e}},
	{0xbb,  2,  {0x09,0x31}},
	{0xbc,  2,  {0x31,0x31}},
	{0xbd,  2,  {0x31,0x31}},
	{0xbe,  2,  {0x31,0x31}},
	{0xbf,  2,  {0x31,0x31}},
	{0xc0,  2,  {0x31,0x31}},
	{0xc1,  2,  {0x03,0x01}},
	{0xc2,  2,  {0x17,0x19}},
	{0xc3,  2,  {0x11,0x13}},
	{0xe5,  2,  {0x31,0x31}},

	{0xc4,  2,  {0x17,0x19}},
	{0xc5,  2,  {0x11,0x13}},
	{0xc6,  2,  {0x03,0x01}},
	{0xc7,  2,  {0x31,0x31}},
	{0xc8,  2,  {0x31,0x31}},
	{0xc9,  2,  {0x31,0x31}},
	{0xca,  2,  {0x31,0x31}},
	{0xcb,  2,  {0x31,0x31}},
	{0xcc,  2,  {0x31,0x09}},
	{0xcd,  2,  {0x2d,0x2e}},
	{0xce,  2,  {0x2e,0x2d}},
	{0xcf,  2,  {0x08,0x31}},
	{0xd0,  2,  {0x31,0x31}},
	{0xd1,  2,  {0x31,0x31}},
	{0xd2,  2,  {0x31,0x31}},
	{0xd3,  2,  {0x31,0x31}},
	{0xd4,  2,  {0x31,0x31}},
	{0xd5,  2,  {0x00,0x02}},
	{0xd6,  2,  {0x12,0x10}},
	{0xd7,  2,  {0x18,0x16}},

	{0xd8,  5,  {0x00,0x00,0x00,0x00,0x00}},
	{0xd9,  5,  {0x00,0x00,0x00,0x00,0x00}},
	{0xe7,  1,  {0x00}},

	{0xf0,  5,  {0x55,0xaa,0x52,0x08,0x05}},
	{0xed,  1,  {0x30}},

	{0xf0,  5,  {0x55,0xaa,0x52,0x08,0x03}},
	{0xb1,  2,  {0x00,0x00}},
	{0xb0,  2,  {0x20,0x00}},

	{0xf0,  5,  {0x55,0xaa,0x52,0x08,0x05}},
	{0xe5,  1,  {0x00}},

	{0xf0,  5,  {0x55,0xaa,0x52,0x08,0x05}},
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
	{0xf0,  5,  {0x55,0xaa,0x52,0x08,0x05}},
	{0xc0,  1,  {0x0b}},
	{0xc1,  1,  {0x09}},
	{0xc2,  1,  {0x0b}},
	{0xc3,  1,  {0x09}},

	{0xf0,  5,  {0x55,0xaa,0x52,0x08,0x03}},
	{0xb2,  5,  {0x05,0x00,0x00,0x00,0x90}},
	{0xb3,  5,  {0x05,0x00,0x00,0x00,0x90}},
	{0xb4,  5,  {0x05,0x00,0x00,0x00,0x90}},
	{0xb5,  5,  {0x05,0x00,0x00,0x00,0x90}},

	{0xf0,  5,  {0x55,0xaa,0x52,0x08,0x05}},
	{0xc4,  1,  {0x10}},
	{0xc5,  1,  {0x10}},
	{0xc6,  1,  {0x10}},
	{0xc7,  1,  {0x10}},
	
	{0xf0,  5,  {0x55,0xaa,0x52,0x08,0x03}},
        {0xb6,  5,  {0x05,0x00,0x00,0x00,0x90}},
        {0xb7,  5,  {0x05,0x00,0x00,0x00,0x90}},
        {0xb8,  5,  {0x05,0x00,0x00,0x00,0x90}},
        {0xb9,  5,  {0x05,0x00,0x00,0x00,0x90}},

	{0xf0,  5,  {0x55,0xaa,0x52,0x08,0x05}},
	{0xc8,  2,  {0x07,0x20}},
	{0xc9,  2,  {0x03,0x20}},
	{0xca,  2,  {0x07,0x00}},
	{0xcb,  2,  {0x03,0x00}},

	{0xf0,  5,  {0x55,0xaa,0x52,0x08,0x03}},
        {0xba,  5,  {0x44,0x00,0x00,0x00,0x90}},
        {0xbb,  5,  {0x44,0x00,0x00,0x00,0x90}},
        {0xbc,  5,  {0x44,0x00,0x00,0x00,0x90}},
        {0xbd,  5,  {0x44,0x00,0x00,0x00,0x90}},

	{0xf0,  5,  {0x55,0xaa,0x52,0x08,0x05}},
        {0xd1,  5,  {0x00,0x05,0x00,0x07,0x10}},
        {0xd2,  5,  {0x00,0x05,0x04,0x07,0x10}},
        {0xd3,  5,  {0x00,0x00,0x0a,0x07,0x10}},
        {0xd4,  5,  {0x00,0x00,0x0a,0x07,0x10}},

	{0xf0,  5,  {0x55,0xaa,0x52,0x08,0x05}},
        {0xd0,  7,  {0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
        {0xd5,  11, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
        {0xd6,  11, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
        {0xd7,  11, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
        {0xd8,  5,  {0x00,0x00,0x00,0x00,0x00}},

	{0xf0,  5,  {0x55,0xaa,0x52,0x08,0x03}},
	{0xc4,  1,  {0x60}},
	{0xc5,  1,  {0x40}},
	{0xc6,  1,  {0x60}},
	{0xc7,  1,  {0x40}},

	{0x6f,  1,  {0x01}},
	{0xf9,  1,  {0x46}},

	{0x11, 1,   {0x00}},
	{REGFLAG_DELAY, 120, {}},

	{0x29, 1, {0x00}},
	
	
	
};
							
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

    params->dsi.mode   = SYNC_PULSE_VDO_MODE;

    // DSI
    /* Command mode setting */
    params->dsi.LANE_NUM				= LCM_FOUR_LANE;
    //The following defined the fomat for data coming from LCD engine.
    params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
    params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
    params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
    params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

   // Highly depends on LCD driver capability.
   //video mode timing

    params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

    params->dsi.vertical_sync_active				= 2;
    params->dsi.vertical_backporch				= 15;
    params->dsi.vertical_frontporch				= 10;
    params->dsi.vertical_active_line				= FRAME_HEIGHT;

    params->dsi.horizontal_sync_active				= 10;
    params->dsi.horizontal_backporch				= 90;
    params->dsi.horizontal_frontporch				= 90;
    params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

    //improve clk quality
    params->dsi.PLL_CLOCK = 240; //this value must be in MTK suggested table
    params->dsi.compatibility_for_nvk = 1;
    params->dsi.ssc_disable = 1;

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
	//reset high to low to high
    lcm_util.set_gpio_out(GPIO_DISP_LRSTB_PIN, GPIO_OUT_ONE);
    mdelay(5);
    lcm_util.set_gpio_out(GPIO_DISP_LRSTB_PIN, GPIO_OUT_ZERO);
    mdelay(5);
    lcm_util.set_gpio_out(GPIO_DISP_LRSTB_PIN, GPIO_OUT_ONE);
    msleep(10);

    lcm_id_pin_handle();
	// when phone initial , config output high, enable backlight drv chip  
    push_table(lcm_initialization_setting_tm, sizeof(lcm_initialization_setting_tm) / sizeof(struct LCM_setting_table), 1);  

    lcm_util.set_gpio_out(GPIO_LCD_DRV_EN_PIN, GPIO_OUT_ONE);

    LCD_DEBUG("uboot:tm_nt35521_lcm_init\n");
}


static void lcm_suspend(void)
{
    //Back to MP.P7 baseline , solve LCD display abnormal On the right
    // when phone sleep , config output low, disable backlight drv chip  
    lcm_util.set_gpio_out(GPIO_LCD_DRV_EN_PIN, GPIO_OUT_ZERO);
    push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);
    //reset low
    lcm_util.set_gpio_out(GPIO_DISP_LRSTB_PIN, GPIO_OUT_ZERO);
    mdelay(5);
    //disable VSP & VSN
	lcm_util.set_gpio_out(GPIO_LCD_BIAS_ENP_PIN, GPIO_OUT_ZERO);
	lcm_util.set_gpio_out(GPIO_LCD_BIAS_ENN_PIN, GPIO_OUT_ZERO);
    mdelay(5);	
    LCD_DEBUG("uboot:tm_nt35521_lcm_suspend\n");

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
    //Back to MP.P7 baseline , solve LCD display abnormal On the right
    //when sleep out, config output high ,enable backlight drv chip  
    lcm_util.set_gpio_out(GPIO_LCD_DRV_EN_PIN, GPIO_OUT_ONE);

    LCD_DEBUG("uboot:tm_nt35521_lcm_resume\n");

}

static unsigned int lcm_compare_id_tm(void)
{
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
	
}
LCM_DRIVER nt35521_hd720_tm_lcm_drv =
{
    .name           = "nt35521_hd720_dsi_video_tm",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params     = lcm_get_params,
    .init           = lcm_init_tm,/*tianma init fun.*/
    .suspend        = lcm_suspend,
    .resume         = lcm_resume_tm,
    .compare_id     = lcm_compare_id_tm,
   
};
