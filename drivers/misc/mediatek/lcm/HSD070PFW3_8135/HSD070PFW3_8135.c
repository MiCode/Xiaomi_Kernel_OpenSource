#ifndef BUILD_LK
#include <linux/string.h>
#else
#include <string.h>
#endif
#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_pmic.h>
#include <debug.h>
#elif (defined BUILD_UBOOT)
#include <asm/arch/mt6577_gpio.h>
#else
#include <mach/mt_gpio.h>
#include <linux/xlog.h>
#include <mach/mt_pm_ldo.h>
#endif
#include "lcm_drv.h"
#include "mt8193_lvds.h"

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  (1024)
#define FRAME_HEIGHT (600)

#define GPIO_LCD_RST_EN      GPIO65
#define GPIO_LCD_STB_EN      GPIO66

#define HSYNC_PULSE_WIDTH 128 
#define HSYNC_BACK_PORCH  152
#define HSYNC_FRONT_PORCH 40
#define VSYNC_PULSE_WIDTH 6
#define VSYNC_BACK_PORCH  16
#define VSYNC_FRONT_PORCH 14


// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = //{0};
{
	.set_reset_pin = NULL,
	.udelay = NULL,
	.mdelay = NULL,
};

#define SET_RESET_PIN(v)    (mt_set_reset_pin((v)))

#define UDELAY(n) 
#define MDELAY(n) 


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
    params->dpi.mipi_pll_clk_div1 = 0x80000101;  //lvds pll 52M
    params->dpi.mipi_pll_clk_div2 = 0x800a0000;
    params->dpi.dpi_clk_div       = 4;
    params->dpi.dpi_clk_duty      = 2;

    params->dpi.clk_pol           = LCM_POLARITY_FALLING;
    params->dpi.de_pol            = LCM_POLARITY_RISING;
    params->dpi.vsync_pol         = LCM_POLARITY_FALLING;
    params->dpi.hsync_pol         = LCM_POLARITY_FALLING;

    params->dpi.hsync_pulse_width = HSYNC_PULSE_WIDTH;
    params->dpi.hsync_back_porch  = HSYNC_BACK_PORCH;
    params->dpi.hsync_front_porch = HSYNC_FRONT_PORCH;
    params->dpi.vsync_pulse_width = VSYNC_PULSE_WIDTH;
    params->dpi.vsync_back_porch  = VSYNC_BACK_PORCH;
    params->dpi.vsync_front_porch = VSYNC_FRONT_PORCH;
    
    params->dpi.i2x_en = 1;
    params->dpi.lvds_tx_en = 1;
    
    params->dpi.format            = LCM_DPI_FORMAT_RGB888;   // format is 24 bit
    params->dpi.rgb_order         = LCM_COLOR_ORDER_RGB;
    params->dpi.is_serial_output  = 0;

    params->dpi.intermediat_buffer_num = 0;

    params->dpi.io_driving_current = LCM_DRIVING_CURRENT_2MA;
}


static void lcm_init(void)
{
#ifdef BUILD_LK
    printf("[LK/LCM] lcm_init() enter\n");
//VGP6 3.3V
pmic_config_interface(0x424, 0x1, 0x1, 15); 
pmic_config_interface(0x45a, 0x07, 0x07, 5);

mt_set_gpio_mode(GPIO_LCD_RST_EN, GPIO_MODE_00);
mt_set_gpio_dir(GPIO_LCD_RST_EN, GPIO_DIR_OUT);
mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ONE);
MDELAY(20);

mt_set_gpio_mode(GPIO_LCD_STB_EN, GPIO_MODE_00);
mt_set_gpio_dir(GPIO_LCD_STB_EN, GPIO_DIR_OUT);
mt_set_gpio_out(GPIO_LCD_STB_EN, GPIO_OUT_ONE);
MDELAY(20); 

#elif (defined BUILD_UBOOT)
    // do nothing in uboot
#else
    printk("[LCM] lcm_init() enter\n");
mt_set_gpio_mode(GPIO_LCD_RST_EN, GPIO_MODE_00);
mt_set_gpio_dir(GPIO_LCD_RST_EN, GPIO_DIR_OUT);
mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ONE);
MDELAY(20);

mt_set_gpio_mode(GPIO_LCD_STB_EN, GPIO_MODE_00);
mt_set_gpio_dir(GPIO_LCD_STB_EN, GPIO_DIR_OUT);
mt_set_gpio_out(GPIO_LCD_STB_EN, GPIO_OUT_ONE);
MDELAY(20); 

#endif 
 	
}


static void lcm_suspend(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM] lcm_suspend() enter\n");
mt_set_gpio_mode(GPIO_LCD_RST_EN, GPIO_MODE_00);
mt_set_gpio_dir(GPIO_LCD_RST_EN, GPIO_DIR_OUT);
mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ZERO);
MDELAY(20);

mt_set_gpio_mode(GPIO_LCD_STB_EN, GPIO_MODE_00);
mt_set_gpio_dir(GPIO_LCD_STB_EN, GPIO_DIR_OUT);
mt_set_gpio_out(GPIO_LCD_STB_EN, GPIO_OUT_ZERO);
MDELAY(20);   

#elif (defined BUILD_UBOOT)
		// do nothing in uboot
#else
	printk("[LCM] lcm_suspend() enter\n");
mt_set_gpio_mode(GPIO_LCD_RST_EN, GPIO_MODE_00);
mt_set_gpio_dir(GPIO_LCD_RST_EN, GPIO_DIR_OUT);
mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ZERO);
MDELAY(20);

mt_set_gpio_mode(GPIO_LCD_STB_EN, GPIO_MODE_00);
mt_set_gpio_dir(GPIO_LCD_STB_EN, GPIO_DIR_OUT);
mt_set_gpio_out(GPIO_LCD_STB_EN, GPIO_OUT_ZERO);
MDELAY(20);   

#endif
  
}


static void lcm_resume(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM] lcm_resume() enter\n");
//VGP6 3.3V
pmic_config_interface(0x424, 0x1, 0x1, 15); 
pmic_config_interface(0x45a, 0x07, 0x07, 5);

mt_set_gpio_mode(GPIO_LCD_RST_EN, GPIO_MODE_00);
mt_set_gpio_dir(GPIO_LCD_RST_EN, GPIO_DIR_OUT);
mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ONE);
MDELAY(20);

mt_set_gpio_mode(GPIO_LCD_STB_EN, GPIO_MODE_00);
mt_set_gpio_dir(GPIO_LCD_STB_EN, GPIO_DIR_OUT);
mt_set_gpio_out(GPIO_LCD_STB_EN, GPIO_OUT_ONE);
MDELAY(20);

#elif (defined BUILD_UBOOT)
		// do nothing in uboot
#else
	printk("[LCM] lcm_resume() enter\n");
mt_set_gpio_mode(GPIO_LCD_RST_EN, GPIO_MODE_00);
mt_set_gpio_dir(GPIO_LCD_RST_EN, GPIO_DIR_OUT);
mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ONE);
MDELAY(20);

mt_set_gpio_mode(GPIO_LCD_STB_EN, GPIO_MODE_00);
mt_set_gpio_dir(GPIO_LCD_STB_EN, GPIO_DIR_OUT);
mt_set_gpio_out(GPIO_LCD_STB_EN, GPIO_OUT_ONE);
MDELAY(20);

#endif

}

LCM_DRIVER hsd070pfw3_8135_lcm_drv = 
{
    .name		= "HSD070PFW3_8135",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
};

