#ifndef BUILD_LK
#include <linux/string.h>
#endif
#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_pmic.h>
#include <debug.h>
#include <platform/upmu_common.h>

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

#if defined(MTK_ALPS_BOX_SUPPORT)
/*for 8127 box hdmi main path */
#define FRAME_WIDTH  (1920)
#define FRAME_HEIGHT (1080)
#else
#define FRAME_WIDTH  (800)
#define FRAME_HEIGHT (1280)
#endif

//#define GPIO_LCD_RST_EN      GPIO90
//#define GPIO_LCD_STB_EN      GPIO89


#ifdef GPIO_LCM_PWR
#define GPIO_LCD_PWR      GPIO_LCM_PWR
#else
#define GPIO_LCD_PWR      0xFFFFFFFF
#endif

#ifdef GPIO_LCM_PWR_EN
#define GPIO_LCD_PWR_EN      GPIO_LCM_PWR_EN
#else
#define GPIO_LCD_PWR_EN      0xFFFFFFFF
#endif

#ifdef GPIO_LCM_PWR2_EN
#define GPIO_LCD_PWR2_EN      GPIO_LCM_PWR2_EN
#else
#define GPIO_LCD_PWR2_EN      0xFFFFFFFF
#endif


#ifdef GPIO_LCM_RST
#define GPIO_LCD_RST_EN      GPIO_LCM_RST
#else
#define GPIO_LCD_RST_EN      0xFFFFFFFF
#endif

#ifdef GPIO_LCM_STB
#define GPIO_LCD_STB_EN      GPIO_LCM_STB
#else
#define GPIO_LCD_STB_EN      0xFFFFFFFF
#endif


#ifdef GPIO_LCM_LVL_SHIFT_EN
#define GPIO_SHIFT_EN      GPIO_LCM_LVL_SHIFT_EN
#else
#define GPIO_SHIFT_EN      0xFFFFFFFF
#endif

#ifdef GPIO_LCM_BL_EN
#define GPIO_LCD_BL_EN      GPIO_LCM_BL_EN
#else
#define GPIO_LCD_BL_EN      0xFFFFFFFF
#endif


#ifdef GPIO_LCM_BRIDGE_EN
#define GPIO_LCD_BRIDGE_EN      GPIO_LCM_BRIDGE_EN
#else
#define GPIO_LCD_BRIDGE_EN      0xFFFFFFFF
#endif



#define HSYNC_PULSE_WIDTH 16 
#define HSYNC_BACK_PORCH  16
#define HSYNC_FRONT_PORCH 32
#define VSYNC_PULSE_WIDTH 2
#define VSYNC_BACK_PORCH  2
#define VSYNC_FRONT_PORCH 4


// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

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


static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{

    if(GPIO == 0xFFFFFFFF)
    {
    #ifdef BUILD_LK
	 printf("[LK/LCM] GPIO_LCD_PWR =  0x%x \n",GPIO_LCD_PWR);
        printf("[LK/LCM] GPIO_LCD_PWR_EN =   0x%x\n",GPIO_LCD_PWR_EN);
        printf("[LK/LCM] GPIO_LCD_PWR2_EN =  0x%x \n",GPIO_LCD_PWR2_EN);
        printf("[LK/LCM] GPIO_LCD_RST_EN =  0x%x \n",GPIO_LCD_RST_EN);
        printf("[LK/LCM] GPIO_LCD_STB_EN =   0x%x\n",GPIO_LCD_STB_EN);
        printf("[LK/LCM] GPIO_SHIFT_EN =   0x%x\n",GPIO_SHIFT_EN);
        printf("[LK/LCM] GPIO_LCD_BL_EN =   0x%x\n",GPIO_LCD_BL_EN);
        printf("[LK/LCM] GPIO_LCD_BRIDGE_EN =  0x%x \n",GPIO_LCD_BRIDGE_EN);
	
    #elif (defined BUILD_UBOOT)
         // do nothing in uboot
    #else	
	 //printf("kernel] lcm_set gpio()  \n");
    #endif
        return;
    }

    mt_set_gpio_mode(GPIO, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO, (output>0)? GPIO_OUT_ONE: GPIO_OUT_ZERO);
}

#ifndef BUILD_LK
static bool fgisFirst = TRUE;
#endif
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

    params->dpi.PLL_CLOCK = 67;  //67MHz


    /* RGB interface configurations */
    params->dpi.mipi_pll_clk_ref  = 0;
    params->dpi.mipi_pll_clk_div1 = 0x80000101;  //lvds pll 65M
    params->dpi.mipi_pll_clk_div2 = 0x800a0000;
    //params->dpi.dpi_clk_div       = 2;          
    //params->dpi.dpi_clk_duty      = 1;

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

	
    //params->dpi.i2x_en = 1;
    params->dpi.lvds_tx_en = 1;
    params->dpi.ssc_disable = 1;
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

lcm_set_gpio_output(GPIO_LCD_PWR, 1);
lcm_set_gpio_output(GPIO_LCD_PWR_EN, 0);
lcm_set_gpio_output(GPIO_LCD_PWR2_EN, 1);

//VGP6 3.3V
//pmic_config_interface(0x424, 0x1, 0x1, 15); 
//pmic_config_interface(0x45a, 0x07, 0x07, 5);

#ifdef CONFIG_MTK_PMIC_MT6397
    upmu_set_rg_vgp6_vosel(0x7);
    upmu_set_rg_vgp6_sw_en(0x1);
#else
    upmu_set_rg_vgp1_vosel(0x7);
    upmu_set_rg_vgp1_en(0x1);
#endif
//hwPowerOn(MT65XX_POWER_LDO_VGP6, VOL_3300, "LCM");

lcm_set_gpio_output(GPIO_LCD_RST_EN,GPIO_OUT_ONE);

MDELAY(20);

lcm_set_gpio_output(GPIO_LCD_STB_EN,GPIO_OUT_ONE);

MDELAY(20);   

lcm_set_gpio_output(GPIO_LCD_BL_EN, GPIO_OUT_ONE);


#elif (defined BUILD_UBOOT)
    // do nothing in uboot
#else
    printk("[LCM] lcm_init() enter\n");

lcm_set_gpio_output(GPIO_LCD_RST_EN,GPIO_OUT_ONE);

	
    MDELAY(20);
    
lcm_set_gpio_output(GPIO_LCD_STB_EN,GPIO_OUT_ONE);

	
    MDELAY(20);        	

lcm_set_gpio_output(GPIO_LCD_BL_EN, GPIO_OUT_ONE);
	

#endif 
 	
}


static void lcm_suspend(void)
{
#ifdef BUILD_LK
    printf("[LK/LCM] lcm_suspend() enter\n");

lcm_set_gpio_output(GPIO_LCD_BL_EN, 0);
MDELAY(20);



lcm_set_gpio_output(GPIO_LCD_RST_EN,GPIO_OUT_ZERO);
lcm_set_gpio_output(GPIO_LCD_STB_EN,GPIO_OUT_ZERO);

lcm_set_gpio_output(GPIO_LCD_PWR, 0);
lcm_set_gpio_output(GPIO_LCD_PWR_EN, 0);
lcm_set_gpio_output(GPIO_LCD_PWR2_EN, 0);


MDELAY(20); 

#elif (defined BUILD_UBOOT)
    // do nothing in uboot
#else
    printk("[LCM] lcm_suspend() enter\n");

    lcm_set_gpio_output(GPIO_LCD_BL_EN, 0);

    MDELAY(20);

lcm_set_gpio_output(GPIO_LCD_RST_EN,GPIO_OUT_ZERO);
lcm_set_gpio_output(GPIO_LCD_STB_EN,GPIO_OUT_ZERO);

     lcm_set_gpio_output(GPIO_LCD_PWR, 0);
     lcm_set_gpio_output(GPIO_LCD_PWR_EN, 0);
     lcm_set_gpio_output(GPIO_LCD_PWR2_EN, 0);

if(fgisFirst == TRUE)
{
     fgisFirst = FALSE;
    #ifdef CONFIG_MTK_PMIC_MT6397
        hwPowerOn(MT65XX_POWER_LDO_VGP6, VOL_3300, "LCM");
    #else
        hwPowerOn(MT6323_POWER_LDO_VGP1, VOL_3300, "LCM");
    #endif
}

    MDELAY(20);     
#ifdef CONFIG_MTK_PMIC_MT6397
	hwPowerDown(MT65XX_POWER_LDO_VGP6, "LCM");//
#else
	hwPowerDown(MT6323_POWER_LDO_VGP1, "LCM");//
#endif
	
    MDELAY(20); 

#endif
  
}


static void lcm_resume(void)
{
#ifdef BUILD_LK
    printf("[LK/LCM] lcm_resume() enter\n");
//VGP6 3.3V

#ifdef CONFIG_MTK_PMIC_MT6397
    upmu_set_rg_vgp6_vosel(0x7);
    upmu_set_rg_vgp6_sw_en(0x1);
#else
    upmu_set_rg_vgp1_vosel(0x7);
    upmu_set_rg_vgp1_en(0x1);
#endif
//pmic_config_interface(0x424, 0x1, 0x1, 15); 
//pmic_config_interface(0x45a, 0x07, 0x07, 5);

//    hwPowerOn(MT65XX_POWER_LDO_VGP6, VOL_3300, "LCM");

    lcm_set_gpio_output(GPIO_LCD_STB_EN,GPIO_OUT_ONE);
    MDELAY(20);   

    lcm_set_gpio_output(GPIO_LCD_PWR, 1);
    lcm_set_gpio_output(GPIO_LCD_PWR_EN, 1);
    lcm_set_gpio_output(GPIO_LCD_PWR2_EN, 1);
	
    MDELAY(5);


    lcm_set_gpio_output(GPIO_LCD_RST_EN,GPIO_OUT_ONE);
    MDELAY(20);

    lcm_set_gpio_output(GPIO_LCD_BL_EN, 1);

#elif (defined BUILD_UBOOT)
    // do nothing in uboot
#else
    printk("[LCM] lcm_resume() enter\n");

#ifdef CONFIG_MTK_PMIC_MT6397
    hwPowerOn(MT65XX_POWER_LDO_VGP6, VOL_3300, "LCM");
#else
    hwPowerOn(MT6323_POWER_LDO_VGP1, VOL_3300, "LCM");
#endif

    MDELAY(30);	

    lcm_set_gpio_output(GPIO_LCD_PWR, 1);
    lcm_set_gpio_output(GPIO_LCD_PWR_EN, 1);
    lcm_set_gpio_output(GPIO_LCD_PWR2_EN, 1);
	
    MDELAY(5);
    
    lcm_set_gpio_output(GPIO_LCD_STB_EN, 1);
    MDELAY(5);	
    lcm_set_gpio_output(GPIO_LCD_RST_EN, 1);
    MDELAY(5);	


    lcm_set_gpio_output(GPIO_LCD_BL_EN, 1);

#endif

}

LCM_DRIVER cpt_clap070wp03xg_lvds_lcm_drv = 
{
    .name		= "cpt_clap070wp03xg_lvds",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
};

