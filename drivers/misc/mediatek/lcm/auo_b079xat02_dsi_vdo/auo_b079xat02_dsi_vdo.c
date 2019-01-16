#ifndef BUILD_LK
#include <linux/string.h>
#else
#include <string.h>   //for fixed warning issue
#endif

#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_pmic.h>
#else
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#endif
#include "lcm_drv.h"
// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  (768)
#define FRAME_HEIGHT (1024)

#define GPIO_LCD_PWR_EN      GPIO115 //GPIO142
#define GPIO_LCD_RST_EN      GPIO65  //GPIO178
#define GPIO_LCD_BL_EN       GPIO75 //GPIO179

#define LCM_DSI_CMD_MODE									0

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

//static LCM_UTIL_FUNCS lcm_util = {0}; //for fixed warning issue
static LCM_UTIL_FUNCS lcm_util = 
{
	.set_reset_pin = NULL,
	.udelay = NULL,
	.mdelay = NULL,
};


#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))

#define UDELAY(n) 											(lcm_util.udelay(n))
#define MDELAY(n) 											(lcm_util.mdelay(n))

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg											lcm_util.dsi_read_reg()
#define read_reg_v2(cmd, buffer, buffer_size)   				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)    


static void lcd_power_en(unsigned char enabled)
{
    if(enabled)
    {
        mt_set_gpio_mode(GPIO_LCD_PWR_EN, GPIO_MODE_00);
        mt_set_gpio_dir(GPIO_LCD_PWR_EN, GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_LCD_PWR_EN, GPIO_OUT_ONE);
        
      #ifdef BUILD_LK
        //VGP6 3.3V
        //pmic_config_interface(DIGLDO_CON33, 0x07, PMIC_RG_VGP6_VOSEL_MASK, PMIC_RG_VGP6_VOSEL_SHIFT);  
        //pmic_config_interface(DIGLDO_CON12, 0x1, PMIC_RG_VGP6_EN_MASK, PMIC_RG_VGP6_EN_SHIFT); 
		//VGP6 3.3V
	pmic_config_interface(0x424, 0x1, 0x1, 15); 
	pmic_config_interface(0x45a, 0x07, 0x07, 5);
      #else
        hwPowerOn(MT65XX_POWER_LDO_VGP6, VOL_3300, "LCM");
      #endif   
    }
    else
    {
    	;
    	#if 1
        mt_set_gpio_mode(GPIO_LCD_PWR_EN, GPIO_MODE_00);
        mt_set_gpio_dir(GPIO_LCD_PWR_EN, GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_LCD_PWR_EN, GPIO_OUT_ZERO); 
        
      #ifdef BUILD_LK
        //VGP6 3.3V
        //pmic_config_interface(DIGLDO_CON12, 0x0, PMIC_RG_VGP6_EN_MASK, PMIC_RG_VGP6_EN_SHIFT); 
        //pmic_config_interface(DIGLDO_CON33, 0x0, PMIC_RG_VGP6_VOSEL_MASK, PMIC_RG_VGP6_VOSEL_SHIFT);   

		pmic_config_interface(0x424, 0x0, 0x1, 15); 
		pmic_config_interface(0x45a, 0x0, 0x07, 5);
      #else
        hwPowerDown(MT65XX_POWER_LDO_VGP6, "LCM");
      #endif     	
      #endif
    }
}

static void lcd_reset(unsigned char enabled)
{
    if(enabled)
    {
    	  mt_set_gpio_mode(GPIO_LCD_RST_EN, GPIO_MODE_00);
        mt_set_gpio_dir(GPIO_LCD_RST_EN, GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ONE);
    }
    else
    {	
        mt_set_gpio_mode(GPIO_LCD_RST_EN, GPIO_MODE_00);
        mt_set_gpio_dir(GPIO_LCD_RST_EN, GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ZERO);    	
    }
}

static void lcd_bl_en(unsigned char enabled)
{
    if(enabled)
    {
        mt_set_gpio_mode(GPIO_LCD_BL_EN, GPIO_MODE_00);
        mt_set_gpio_dir(GPIO_LCD_BL_EN, GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_LCD_BL_EN, GPIO_OUT_ONE);
    }
    else
    {	
        mt_set_gpio_mode(GPIO_LCD_BL_EN, GPIO_MODE_00);
        mt_set_gpio_dir(GPIO_LCD_BL_EN, GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_LCD_BL_EN, GPIO_OUT_ZERO);    	
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
		params->dsi.mode   = BURST_VDO_MODE;
	
		// DSI
		/* Command mode setting */
		params->dsi.LANE_NUM				= LCM_FOUR_LANE;
		//The following defined the fomat for data coming from LCD engine.
		params->dsi.data_format.format		= LCM_DSI_FORMAT_RGB666;

		// Video mode setting		
		params->dsi.PS=LCM_PACKED_PS_18BIT_RGB666;

		params->dsi.vertical_sync_active				= 50;
		params->dsi.vertical_backporch					= 30;
		params->dsi.vertical_frontporch 				= 36;
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 

		params->dsi.horizontal_sync_active				= 64;
		params->dsi.horizontal_backporch				= 56;
		params->dsi.horizontal_frontporch				= 60;
		params->dsi.horizontal_active_pixel 			= FRAME_WIDTH;

		// Bit rate calculation
		//params->dsi.pll_div1=0;		
		//params->dsi.pll_div2=1; 		
		//params->dsi.fbk_div =19;
		params->dsi.PLL_CLOCK = LCM_DSI_6589_PLL_CLOCK_260;
		
		params->dsi.cont_clock = 1;
    params->dsi.horizontal_bllp = 0x115; //word count=0x340
}

extern void DSI_clk_HS_mode(unsigned char enter);
static void init_lcm_registers(void)
{
	  unsigned int data_array[16];
	  
		#ifdef BUILD_LK
		printf("%s, LK \n", __func__);
    #else
		printk("%s, kernel", __func__);
    #endif
	  
	  data_array[0] = 0x00010500;  //software reset					 
	  dsi_set_cmdq(data_array, 1, 1);
  	MDELAY(10);
	DSI_clk_HS_mode(1);
	  MDELAY(80);
	  
	  data_array[0] = 0x00290500;  //display on                        
	  dsi_set_cmdq(data_array, 1, 1);
}

static void lcm_init(void)
{
	#ifdef BUILD_LK
		printf("%s, LK \n", __func__);
#else
		printk("%s, kernel", __func__);
#endif
	#ifdef BUILD_LK
	 //lcd_bl_en(0);
    lcd_reset(0);
    //lcd_power_en(0);
	MDELAY(10);//Must > 5ms
    lcd_power_en(1);
    MDELAY(100);//Must > 5ms
    lcd_reset(1);
    MDELAY(200);//Must > 50ms
    init_lcm_registers();
	  lcd_bl_en(1);
	  return;//LK donothing
	#else
    //lcd_bl_en(0);
    #if 0
    lcd_reset(0);
	MDELAY(10);//Must > 5ms
   // lcd_power_en(0);
    lcd_power_en(1);
    MDELAY(100);//Must > 5ms
    lcd_reset(1);
    MDELAY(200);//Must > 50ms
    init_lcm_registers();
	  lcd_bl_en(1);
	  #endif
	#endif  
}


static void lcm_suspend(void)
{
	  unsigned int data_array[16];
  #ifdef BUILD_LK
		printf("%s, LK \n", __func__);
#else
		printk("%s, kernel", __func__);
#endif
    data_array[0] = 0x00280500;  //display off                        
	  dsi_set_cmdq(data_array, 1, 1);
	  MDELAY(5);
	  lcd_bl_en(0);
    lcd_reset(0);
    lcd_power_en(0);
	  DSI_clk_HS_mode(0);
	  MDELAY(10);
}


static void lcm_resume(void)
{
    //lcd_bl_en(0);
    
	#ifdef BUILD_LK
		printf("%s, LK \n", __func__);
#else
		printk("%s, kernel", __func__);
#endif
    lcd_reset(0);
    //lcd_power_en(0);
	MDELAY(10);//Must > 5ms
    lcd_power_en(1);
    MDELAY(100);//Must > 5ms
    lcd_reset(1);
    MDELAY(200);//Must > 50ms
    init_lcm_registers();
	  lcd_bl_en(1);
}

LCM_DRIVER auo_b079xat02_dsi_vdo_lcm_drv = 
{
    .name			= "AUO_B079XAT02_DSI_VDO",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	//.compare_id    = lcm_compare_id,
};

