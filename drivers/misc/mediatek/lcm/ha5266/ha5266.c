#ifndef BUILD_LK
#include <linux/string.h>
#endif
#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_pmic.h>
#include <debug.h>
#elif (defined BUILD_UBOOT)
#else
#include <mach/mt_gpio.h>
#include <linux/xlog.h>
#include <mach/mt_pm_ldo.h>
#endif
#include "lcm_drv.h"


// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  (800)
#define FRAME_HEIGHT (480)


// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))

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

static void init_lcm_registers(void)
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
    
    params->dpi.mipi_pll_clk_ref  = 0;      //the most important parameters: set pll clk to 66Mhz and dpi clk to 33Mhz
    params->dpi.mipi_pll_clk_div1 = 35;
    params->dpi.mipi_pll_clk_div2 = 7;
    params->dpi.dpi_clk_div       = 2;
    params->dpi.dpi_clk_duty      = 1;

    params->dpi.clk_pol           = LCM_POLARITY_RISING;
    params->dpi.de_pol            = LCM_POLARITY_RISING;
    params->dpi.vsync_pol         = LCM_POLARITY_FALLING;
    params->dpi.hsync_pol         = LCM_POLARITY_FALLING;

    params->dpi.hsync_pulse_width = 30;
    params->dpi.hsync_back_porch  = 16;
    params->dpi.hsync_front_porch = 210;
    params->dpi.vsync_pulse_width = 13;
    params->dpi.vsync_back_porch  = 10;
    params->dpi.vsync_front_porch = 22;
    
    params->dpi.format            = LCM_DPI_FORMAT_RGB888;   // format is 24 bit
    params->dpi.rgb_order         = LCM_COLOR_ORDER_RGB;
    params->dpi.is_serial_output  = 0;

    params->dpi.intermediat_buffer_num = 2;

    params->dpi.io_driving_current = LCM_DRIVING_CURRENT_8MA | LCM_DRIVING_CURRENT_4MA | LCM_DRIVING_CURRENT_2MA;
}


static void lcm_init(void)
{
	lcm_util.set_gpio_mode(GPIO18, GPIO_MODE_00);    
    lcm_util.set_gpio_dir(GPIO18, GPIO_DIR_OUT);
    lcm_util.set_gpio_out(GPIO18, GPIO_OUT_ONE); // RST	
	MDELAY(30);
    lcm_util.set_gpio_mode(GPIO52, GPIO_MODE_00);    
    lcm_util.set_gpio_dir(GPIO52, GPIO_DIR_OUT);
    lcm_util.set_gpio_out(GPIO52, GPIO_OUT_ONE); // LCM_VLED_EN 
    MDELAY(30);	
    lcm_util.set_gpio_mode(GPIO47, GPIO_MODE_00);    
    lcm_util.set_gpio_dir(GPIO47, GPIO_DIR_OUT);
    lcm_util.set_gpio_out(GPIO47, GPIO_OUT_ONE); // LCM_BL_ENABLE
    MDELAY(30);
}


static void lcm_suspend(void)
{
    lcm_util.set_gpio_mode(GPIO18, GPIO_MODE_00);    
    lcm_util.set_gpio_dir(GPIO18, GPIO_DIR_OUT);
    lcm_util.set_gpio_out(GPIO18, GPIO_OUT_ZERO); // RST	
	MDELAY(30);	
    lcm_util.set_gpio_mode(GPIO52, GPIO_MODE_00);    
    lcm_util.set_gpio_dir(GPIO52, GPIO_DIR_OUT);
    lcm_util.set_gpio_out(GPIO52, GPIO_OUT_ZERO); // LCM_VLED_EN 
    MDELAY(30);	
    lcm_util.set_gpio_mode(GPIO47, GPIO_MODE_00);    
    lcm_util.set_gpio_dir(GPIO47, GPIO_DIR_OUT);
    lcm_util.set_gpio_out(GPIO47, GPIO_OUT_ZERO); // LCM_BL_ENABLE
    MDELAY(30);
}


static void lcm_resume(void)
{
    lcm_util.set_gpio_mode(GPIO18, GPIO_MODE_00);    
    lcm_util.set_gpio_dir(GPIO18, GPIO_DIR_OUT);
    lcm_util.set_gpio_out(GPIO18, GPIO_OUT_ONE); // RST	
	MDELAY(30);
    lcm_util.set_gpio_mode(GPIO52, GPIO_MODE_00);    
    lcm_util.set_gpio_dir(GPIO52, GPIO_DIR_OUT);
    lcm_util.set_gpio_out(GPIO52, GPIO_OUT_ONE); // LCM_VLED_EN 
    MDELAY(30);	
    lcm_util.set_gpio_mode(GPIO47, GPIO_MODE_00);    
    lcm_util.set_gpio_dir(GPIO47, GPIO_DIR_OUT);
    lcm_util.set_gpio_out(GPIO47, GPIO_OUT_ONE); // LCM_BL_ENABLE
    MDELAY(60);
}


// ---------------------------------------------------------------------------
//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------
LCM_DRIVER ha5266_lcm_drv = 
{
    .name			= "ha5266",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
};
