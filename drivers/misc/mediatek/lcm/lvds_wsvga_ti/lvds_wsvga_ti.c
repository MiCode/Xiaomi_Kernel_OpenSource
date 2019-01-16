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

#define FRAME_WIDTH  (1024)
#define FRAME_HEIGHT (600)


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
    unsigned char temp1 = (unsigned char)((cmd >> 8) & 0xFF);
    unsigned char temp2 = (unsigned char)(cmd & 0xFF);

    lcm_util.send_data(0x2000 | temp1);
    lcm_util.send_data(0x0000 | temp2);
}

static __inline void send_data_cmd(unsigned int data)
{
    lcm_util.send_data(0x0004 | data);
}

static __inline void set_lcm_register(unsigned int regIndex,
                                      unsigned int regData)
{
    send_ctrl_cmd(regIndex);
    send_data_cmd(regData);
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
    params->dpi.mipi_pll_clk_div1 = 31;
    params->dpi.mipi_pll_clk_div2 = 4;
    params->dpi.dpi_clk_div       = 2;
    params->dpi.dpi_clk_duty      = 1;

    params->dpi.clk_pol           = LCM_POLARITY_FALLING;
    params->dpi.de_pol            = LCM_POLARITY_RISING;
    params->dpi.vsync_pol         = LCM_POLARITY_FALLING;
    params->dpi.hsync_pol         = LCM_POLARITY_FALLING;

    params->dpi.hsync_pulse_width = 128;
    params->dpi.hsync_back_porch  = 152;
    params->dpi.hsync_front_porch = 40;
    params->dpi.vsync_pulse_width = 3;
    params->dpi.vsync_back_porch  = 12;
    params->dpi.vsync_front_porch = 10;
    
    params->dpi.format            = LCM_DPI_FORMAT_RGB888;   // format is 24 bit
    params->dpi.rgb_order         = LCM_COLOR_ORDER_RGB;
    params->dpi.is_serial_output  = 0;

    params->dpi.intermediat_buffer_num = 2;

    params->dpi.io_driving_current = LCM_DRIVING_CURRENT_2MA;
}


static void lcm_init(void)
{
    //SET_RESET_PIN(0);
    //MDELAY(10);
    //SET_RESET_PIN(1); //CH7035B reset control

    //lcm_util.set_gpio_mode(GPIO60, GPIO_MODE_00);    
    //lcm_util.set_gpio_dir(GPIO60, GPIO_DIR_OUT);
    //lcm_util.set_gpio_out(GPIO60, GPIO_OUT_ONE); // HDMI_POWER_EN

    lcm_util.set_gpio_mode(GPIO15, GPIO_MODE_00);    
    lcm_util.set_gpio_dir(GPIO15, GPIO_DIR_OUT);
    lcm_util.set_gpio_out(GPIO15, GPIO_OUT_ONE); // LCM_STBY
    MDELAY(5);
    lcm_util.set_gpio_mode(GPIO52, GPIO_MODE_00);    
    lcm_util.set_gpio_dir(GPIO52, GPIO_DIR_OUT);
    lcm_util.set_gpio_out(GPIO52, GPIO_OUT_ONE); // LCM_VLED_EN 
    MDELAY(50);
    lcm_util.set_gpio_mode(GPIO49, GPIO_MODE_00);    
    lcm_util.set_gpio_dir(GPIO49, GPIO_DIR_OUT);
    lcm_util.set_gpio_out(GPIO49, GPIO_OUT_ONE); // LCM_RST
    MDELAY(5);
    lcm_util.set_gpio_mode(GPIO50, GPIO_MODE_00);    
    lcm_util.set_gpio_dir(GPIO50, GPIO_DIR_OUT);
    lcm_util.set_gpio_out(GPIO50, GPIO_OUT_ONE); // LVDS_SHUTDOWN_N   
    MDELAY(50);
    lcm_util.set_gpio_mode(GPIO47, GPIO_MODE_00);    
    lcm_util.set_gpio_dir(GPIO47, GPIO_DIR_OUT);
    lcm_util.set_gpio_out(GPIO47, GPIO_OUT_ONE); // LCM_BL_ENABLE    
}


static void lcm_suspend(void)
{
    //SET_RESET_PIN(0);  //CH7035B reset control

    //lcm_util.set_gpio_mode(GPIO60, GPIO_MODE_00);    
    //lcm_util.set_gpio_dir(GPIO60, GPIO_DIR_OUT);
    //lcm_util.set_gpio_out(GPIO60, GPIO_OUT_ZERO); // HDMI_POWER_EN

    lcm_util.set_gpio_mode(GPIO47, GPIO_MODE_00);    
    lcm_util.set_gpio_dir(GPIO47, GPIO_DIR_OUT);
    lcm_util.set_gpio_out(GPIO47, GPIO_OUT_ZERO); // LCM_BL_ENABLE
    MDELAY(10);
    lcm_util.set_gpio_mode(GPIO50, GPIO_MODE_00);    
    lcm_util.set_gpio_dir(GPIO50, GPIO_DIR_OUT);
    lcm_util.set_gpio_out(GPIO50, GPIO_OUT_ZERO); // LVDS_SHUTDOWN_N      
    MDELAY(10);
    lcm_util.set_gpio_mode(GPIO15, GPIO_MODE_00);    
    lcm_util.set_gpio_dir(GPIO15, GPIO_DIR_OUT);
    lcm_util.set_gpio_out(GPIO15, GPIO_OUT_ZERO); // LCM_STBY     
    MDELAY(10);
    lcm_util.set_gpio_mode(GPIO49, GPIO_MODE_00);    
    lcm_util.set_gpio_dir(GPIO49, GPIO_DIR_OUT);
    lcm_util.set_gpio_out(GPIO49, GPIO_OUT_ZERO); // LCM_RST    
    MDELAY(10);
    lcm_util.set_gpio_mode(GPIO52, GPIO_MODE_00);    
    lcm_util.set_gpio_dir(GPIO52, GPIO_DIR_OUT);
    lcm_util.set_gpio_out(GPIO52, GPIO_OUT_ZERO); // LCM_VLED_EN
    MDELAY(60); // avoid LCD resume transint
}


static void lcm_resume(void)
{
    //SET_RESET_PIN(1);  //CH7035B reset control

    //lcm_util.set_gpio_mode(GPIO60, GPIO_MODE_00);    
    //lcm_util.set_gpio_dir(GPIO60, GPIO_DIR_OUT);
    //lcm_util.set_gpio_out(GPIO60, GPIO_OUT_ONE); // HDMI_POWER_EN

    lcm_util.set_gpio_mode(GPIO15, GPIO_MODE_00);    
    lcm_util.set_gpio_dir(GPIO15, GPIO_DIR_OUT);
    lcm_util.set_gpio_out(GPIO15, GPIO_OUT_ONE); // LCM_STBY
    MDELAY(5);
    lcm_util.set_gpio_mode(GPIO52, GPIO_MODE_00);    
    lcm_util.set_gpio_dir(GPIO52, GPIO_DIR_OUT);
    lcm_util.set_gpio_out(GPIO52, GPIO_OUT_ONE); // LCM_VLED_EN 
    MDELAY(50);
    lcm_util.set_gpio_mode(GPIO49, GPIO_MODE_00);    
    lcm_util.set_gpio_dir(GPIO49, GPIO_DIR_OUT);
    lcm_util.set_gpio_out(GPIO49, GPIO_OUT_ONE); // LCM_RST
    MDELAY(5);
    lcm_util.set_gpio_mode(GPIO50, GPIO_MODE_00);    
    lcm_util.set_gpio_dir(GPIO50, GPIO_DIR_OUT);
    lcm_util.set_gpio_out(GPIO50, GPIO_OUT_ONE); // LVDS_SHUTDOWN_N   
    MDELAY(50);
    lcm_util.set_gpio_mode(GPIO47, GPIO_MODE_00);    
    lcm_util.set_gpio_dir(GPIO47, GPIO_DIR_OUT);
    lcm_util.set_gpio_out(GPIO47, GPIO_OUT_ONE); // LCM_BL_ENABLE
    MDELAY(120); // avoid LCD resume transint
}

LCM_DRIVER lvds_wsvga_ti_lcm_drv = 
{
    .name			= "lvds_wsvga_ti",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
};

