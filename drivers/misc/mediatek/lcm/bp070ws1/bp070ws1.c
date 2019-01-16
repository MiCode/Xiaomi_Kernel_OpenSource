#ifndef BUILD_LK
#include <linux/string.h>
#endif
#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#else
#include <mach/mt_gpio.h>
#endif

#include "lcm_drv.h"


// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  (1024)
#define FRAME_HEIGHT (600)

#define GPIO_DISP_PWR_EN     GPIO139
#define GPIO_PWR_3V3_EN      GPIO142
#define GPIO_LCD_LDO_EN      GPIO116
#define GPIO_LVDS_SHUTDOWN   GPIO133

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    (mt_set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

static __inline void send_ctrl_cmd(unsigned int cmd)
{

}

static __inline void send_data_cmd(unsigned int data)
{

}

static __inline void set_lcm_register(unsigned int regIndex,
                                      unsigned int regData)
{

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

    params->type   = LCM_TYPE_DPI;
    params->ctrl   = LCM_CTRL_SERIAL_DBI;
    params->width  = FRAME_WIDTH;
    params->height = FRAME_HEIGHT;
    params->io_select_mode = 0;	

    /* RGB interface configurations */    
    params->dpi.mipi_pll_clk_ref  = 0;
    params->dpi.mipi_pll_clk_div1 = 0x80000081;  //lvds pll 204.1M
    params->dpi.mipi_pll_clk_div2 = 0x800fb333;
    params->dpi.dpi_clk_div       = 4;           //{4,2}, pll/4=51.025M
    params->dpi.dpi_clk_duty      = 2;

    params->dpi.clk_pol           = LCM_POLARITY_FALLING;
    params->dpi.de_pol            = LCM_POLARITY_RISING;
    params->dpi.vsync_pol         = LCM_POLARITY_FALLING;
    params->dpi.hsync_pol         = LCM_POLARITY_FALLING;

    params->dpi.hsync_pulse_width = 128;  //HT=1344
    params->dpi.hsync_back_porch  = 152;
    params->dpi.hsync_front_porch = 40;
    params->dpi.vsync_pulse_width = 5;    //VT=635
    params->dpi.vsync_back_porch  = 15;
    params->dpi.vsync_front_porch = 15;
    
    params->dpi.i2x_en = 0;
    
    params->dpi.format            = LCM_DPI_FORMAT_RGB888;   // format is 24 bit
    params->dpi.rgb_order         = LCM_COLOR_ORDER_RGB;
    params->dpi.is_serial_output  = 0;

    params->dpi.intermediat_buffer_num = 0;

    params->dpi.io_driving_current = LCM_DRIVING_CURRENT_6575_8MA;
}


static void lcm_init(void)
{
#ifdef BUILD_LK
    mt_set_gpio_mode(GPIO_DISP_PWR_EN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_DISP_PWR_EN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_DISP_PWR_EN, GPIO_OUT_ONE);
    MDELAY(20);
    mt_set_gpio_mode(GPIO_PWR_3V3_EN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_PWR_3V3_EN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_PWR_3V3_EN, GPIO_OUT_ONE);
    MDELAY(20);
    mt_set_gpio_mode(GPIO_LCD_LDO_EN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_LCD_LDO_EN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_LCD_LDO_EN, GPIO_OUT_ONE);
    MDELAY(20);
    mt_set_gpio_mode(GPIO_LVDS_SHUTDOWN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_LVDS_SHUTDOWN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_LVDS_SHUTDOWN, GPIO_OUT_ONE);
    MDELAY(20);
#elif (defined BUILD_UBOOT)
    // do nothing in uboot
#else
    mt_set_gpio_mode(GPIO_DISP_PWR_EN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_DISP_PWR_EN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_DISP_PWR_EN, GPIO_OUT_ONE);
    MDELAY(20);
    mt_set_gpio_mode(GPIO_PWR_3V3_EN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_PWR_3V3_EN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_PWR_3V3_EN, GPIO_OUT_ONE);
    MDELAY(20);
    mt_set_gpio_mode(GPIO_LCD_LDO_EN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_LCD_LDO_EN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_LCD_LDO_EN, GPIO_OUT_ONE);
    MDELAY(20);
    mt_set_gpio_mode(GPIO_LVDS_SHUTDOWN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_LVDS_SHUTDOWN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_LVDS_SHUTDOWN, GPIO_OUT_ONE);
    MDELAY(20);
#endif    
}


static void lcm_suspend(void)
{
    mt_set_gpio_mode(GPIO_LVDS_SHUTDOWN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_LVDS_SHUTDOWN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_LVDS_SHUTDOWN, GPIO_OUT_ZERO);
    MDELAY(20);	
    mt_set_gpio_mode(GPIO_LCD_LDO_EN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_LCD_LDO_EN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_LCD_LDO_EN, GPIO_OUT_ZERO);
    MDELAY(20);  
    mt_set_gpio_mode(GPIO_PWR_3V3_EN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_PWR_3V3_EN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_PWR_3V3_EN, GPIO_OUT_ZERO);
    MDELAY(20);      
    mt_set_gpio_mode(GPIO_DISP_PWR_EN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_DISP_PWR_EN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_DISP_PWR_EN, GPIO_OUT_ZERO);
    MDELAY(20);
}


static void lcm_resume(void)
{    
    mt_set_gpio_mode(GPIO_DISP_PWR_EN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_DISP_PWR_EN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_DISP_PWR_EN, GPIO_OUT_ONE);
    MDELAY(20);
    mt_set_gpio_mode(GPIO_PWR_3V3_EN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_PWR_3V3_EN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_PWR_3V3_EN, GPIO_OUT_ONE);
    MDELAY(20);
    mt_set_gpio_mode(GPIO_LCD_LDO_EN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_LCD_LDO_EN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_LCD_LDO_EN, GPIO_OUT_ONE);
    MDELAY(20);
    mt_set_gpio_mode(GPIO_LVDS_SHUTDOWN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_LVDS_SHUTDOWN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_LVDS_SHUTDOWN, GPIO_OUT_ONE);
    MDELAY(20);
}

LCM_DRIVER bp070ws1_lcm_drv = 
{
    .name		    = "bp070ws1",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
};

