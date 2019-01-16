
#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/sched.h>		//spinlock
#endif
#include "lcm_drv.h"


#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h> 
#include <platform/mt_pmic.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
//#include <linux/delay.h>
	#include <mach/mt_pm_ldo.h>
#include <mach/mt_gpio.h>


#endif
#include <cust_gpio_usage.h>
#include <cust_i2c.h>

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------
#define LCM_DSI_CMD_MODE									0
#define FRAME_WIDTH  				(1080)
#define FRAME_HEIGHT 				(1920)

#if 1
#define GPIO_LCD_RESET_PIN			GPIO_LCM_RST
#define GPIO_LCD_BL_EN_PIN			GPIO_LCD_BL_EN
#else
#define GPIO_LCD_RESET_PIN			GPIO106
#define GPIO_LCD_BIAS_ENN_PIN 		GPIO133
#define GPIO_LCD_BIAS_ENP_PIN 		GPIO134
#define GPIO_LCD_BL_EN_PIN			GPIO135
#endif

#define REGFLAG_PORT_SWAP					0xFE
#define REGFLAG_DELAY             				0xFC	
#define REGFLAG_END_OF_TABLE      			0xFD   // END OF REGISTERS MARKER

#ifndef TRUE
#define TRUE 1
#endif


#ifndef FALSE
#define FALSE 0
#endif
//static unsigned int lcm_esd_test = FALSE;      ///only for ESD test
// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------
const static unsigned char LCD_MODULE_ID = 0x0;	//tianma: id0: 0: id1: 0

static const unsigned int BL_MIN_LEVEL =20;
static LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))
#define MDELAY(n) 											(lcm_util.mdelay(n))
#define UDELAY(n) 											(lcm_util.udelay(n))

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------
#define dsi_set_cmd_by_cmdq_dual(handle,cmd,count,ppara,force_update)    lcm_util.dsi_set_cmdq_V23(handle,cmd,count,ppara,force_update);
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)										lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   			lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)    
#define dsi_swap_port(swap)   								lcm_util.dsi_swap_port(swap)

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>  
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
//#include <linux/jiffies.h>
#include <linux/uaccess.h>
//#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
/***************************************************************************** 
* Define
*****************************************************************************/
#endif

#ifdef BUILD_LK
extern int TPS65132_write_byte(kal_uint8 addr, kal_uint8 value);
#else
extern int tps65132_write_bytes(unsigned char addr, unsigned char value);
#endif

struct LCM_setting_table {
    unsigned int cmd;
    unsigned char count;
    unsigned char para_list[64];
};


static struct LCM_setting_table lcm_initialization_setting[] = 
{
	//begin add by lizhiye, for sharp gamma 2.4, 20151221
	{0xB0, 1, {0x00}},
	{0XC7, 30, {0X05, 0X14, 0X1E, 0X2A, 0X3B, 0X4A, 0X54, 0X62, 0X47, 0X50, 0X5D, 0X69, 0X70, 0X74, 0X77, 0X07, 0X16, 0X20, 0X2A, 0X3B, 0X4A, 0X54, 0X62, 0X47, 0X4F, 0X5A, 0X66, 0X6D, 0X70, 0X73}},
	{0XC8, 19, {0X01, 0X00, 0X00, 0XFD, 0X02, 0XFC, 0X00, 0X00, 0XFF, 0X01, 0XFF, 0XFC, 0X00, 0X00, 0X01, 0XF5, 0XF6, 0XFC, 0X00}},
	{0xD6, 1, {0x01}},
	{0xB0, 1, {0x03}},
	//end add by lizhiye, for sharp gamma 2.4, 20151221
	
	{0x35, 1, {0x00}},

	{0x51, 1, {0x06}},
	{0x53, 1, {0x24}},	
	{0x55, 1, {0x00}},		//cabc	

	{0x29, 0, {}},
	{REGFLAG_DELAY, 20, {}},
	{0x11, 0, {}},
	{REGFLAG_DELAY, 120, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_cabc_on_initialization_setting[] = 
{
	//begin add by lizhiye, for sharp gamma 2.4, 20151221
	{0xB0, 1, {0x00}},
	{0XC7, 30, {0X05, 0X14, 0X1E, 0X2A, 0X3B, 0X4A, 0X54, 0X62, 0X47, 0X50, 0X5D, 0X69, 0X70, 0X74, 0X77, 0X07, 0X16, 0X20, 0X2A, 0X3B, 0X4A, 0X54, 0X62, 0X47, 0X4F, 0X5A, 0X66, 0X6D, 0X70, 0X73}},
	{0XC8, 19, {0X01, 0X00, 0X00, 0XFD, 0X02, 0XFC, 0X00, 0X00, 0XFF, 0X01, 0XFF, 0XFC, 0X00, 0X00, 0X01, 0XF5, 0XF6, 0XFC, 0X00}},
	{0xD6, 1, {0x01}},
	{0xB0, 1, {0x03}},
	//end add by lizhiye, for sharp gamma 2.4, 20151221
	
	{0x35, 1, {0x00}},

	{0x51, 1, {0x06}},
	{0x53, 1, {0x24}},	
	{0x55, 1, {0x01}},		//cabc	

	{0x29, 0, {}},
	{REGFLAG_DELAY, 20, {}},
	{0x11, 0, {}},
	{REGFLAG_DELAY, 120, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_suspend_setting[] = 
{
	{0x51, 1, {0x00}},	
	{0x28,0,{}},
	{REGFLAG_DELAY, 40, {}},

	{0x10,0,{}},
	{REGFLAG_DELAY, 100, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_backlight_level_setting[] = 
{
	{0x51, 1, {0xff}},
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
#ifdef BUILD_LK
    			dprintf(0, "[LK]REGFLAG_DELAY\n");
#endif
           		if(table[i].count <= 10)
           			MDELAY(table[i].count);
            	else
            		MDELAY(table[i].count);
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
	params->physical_width = 68;
	params->physical_height = 121;
	
#ifdef CONFIG_DEVINFO_LCM
	params->module="sharp";
	params->vendor="sharp";
	params->ic="R63315";
	params->info="1080*1920";
	//params->version=NULL; 
#endif

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode   = CMD_MODE;
#else
	params->dsi.mode   = BURST_VDO_MODE;
#endif
	params->dsi.switch_mode_enable = 0;
	params->dsi.noncont_clock = TRUE;
	// DSI
	/* Command mode setting */
	//1 Three lane or Four lane
	params->dsi.LANE_NUM				= LCM_FOUR_LANE;
	
	//The following defined the fomat for data coming from LCD engine.
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

	params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.HS_TRAIL = 2;	//MIPI TEST
	
	params->dsi.vertical_sync_active				= 2;
	params->dsi.vertical_backporch					= 4;
	params->dsi.vertical_frontporch					= 4;
	params->dsi.vertical_active_line				= FRAME_HEIGHT; 

	params->dsi.horizontal_sync_active				= 10;
	params->dsi.horizontal_backporch				= 50;
	params->dsi.horizontal_frontporch				= 90;	//min:70; default:102
	params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

	// Bit rate calculation
	// 1 Every lane speed
	params->dsi.PLL_CLOCK=475;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 0;
}

static void tps65132_enable(char en)
{
	int ret=0;
	int ret1=0;
	int ret2=0;
	int num = 0;
	mt_set_gpio_mode(GPIO_LCD_BIAS_ENN_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_BIAS_ENN_PIN, GPIO_DIR_OUT);
	mt_set_gpio_mode(GPIO_LCD_BIAS_ENP_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_BIAS_ENP_PIN, GPIO_DIR_OUT);

	if (en)
	{			
		ret1 = mt_set_gpio_out(GPIO_LCD_BIAS_ENP_PIN, GPIO_OUT_ONE);
		MDELAY(12);
		ret2 = mt_set_gpio_out(GPIO_LCD_BIAS_ENN_PIN, GPIO_OUT_ONE);
		MDELAY(12);
	#ifdef BUILD_LK
		dprintf(0, "[LK]tps65132_enable, ret1 =%d, ret2 =%d\n", ret1, ret2);
		for(num = 0; num < 3; num++)
		{		
			ret=TPS65132_write_byte(0x00,0x0f);
			if(ret) 
			{
				dprintf(0, "r63315--sharp--tps65132_enable----cmd=0x00--i2c write error--num=%d\n", num);		
				MDELAY(5);
			}
			else
			{
				dprintf(0, "r63315--sharp--tps65132_enable----cmd=0x00--i2c write success--num=%d\n", num);
				break;
			}
		}

		for(num = 0; num < 3; num++)
		{	
			ret=TPS65132_write_byte(0x01,0x0f);
			if(ret) 
			{
				dprintf(0, "r63315--sharp--tps65132_enable----cmd=0x01--i2c write error--num=%d\n", num);
				MDELAY(5);
			}
			else
			{
				dprintf(0, "r63315--sharp--tps65132_enable----cmd=0x01--i2c write success--num=%d\n", num);   
				break;
			}
		}
	#else
		printk("tps65132_enable, ret1 =%d, ret2 =%d\n", ret1, ret2);
		for(num = 0; num < 3; num++)
		{	
			ret=tps65132_write_bytes(0x00,0x0f);
			if(ret<0)
			{
				printk("r63315--sharp--tps65132_enable-cmd=0x00-- i2c write error-num=%d\n", num);
				MDELAY(5);
			}
			else
			{
				printk("r63315--sharp--tps65132_enable-cmd=0x00-- i2c write success-num=%d\n", num);
				break;
			}
		}

		for(num = 0; num < 3; num++)
		{	
			ret=tps65132_write_bytes(0x01,0x0f);
			if(ret<0)
			{
				printk("r63315--sharp--tps65132_enable-cmd=0x01-- i2c write error-num=%d\n", num);
				MDELAY(5);
			}
			else
			{
				printk("r63315--sharp--tps65132_enable-cmd=0x01-- i2c write success-num=%d\n", num);
				break;
			}
		}
	#endif
	}
	else
	{
	#ifndef BUILD_LK
		printk("[KERNEL]r63315--sharp--tps65132_enable-----sleep--\n");
	#endif
		mt_set_gpio_out(GPIO_LCD_BIAS_ENN_PIN, GPIO_OUT_ZERO);
		MDELAY(12);
		mt_set_gpio_out(GPIO_LCD_BIAS_ENP_PIN, GPIO_OUT_ZERO);
		MDELAY(12);
	}
}

static void lcm_init(void)
{
	mt_set_gpio_mode(GPIO_LCD_RESET_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_RESET_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_RESET_PIN, GPIO_OUT_ONE);
	MDELAY(10);
	mt_set_gpio_out(GPIO_LCD_RESET_PIN, GPIO_OUT_ZERO);
	MDELAY(10);
	mt_set_gpio_out(GPIO_LCD_RESET_PIN, GPIO_OUT_ONE);
	MDELAY(5);
	
	// when phone initial , config output high, enable backlight drv chip  
	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1); 
}

static void lcm_suspend(void)
{
	push_table(lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);  
	mt_set_gpio_mode(GPIO_LCD_RESET_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_RESET_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_RESET_PIN, GPIO_OUT_ZERO);
	MDELAY(10);

	tps65132_enable(0);
}

extern int cabc_enable_flag;
static unsigned int last_backlight_level = 0;
static void lcm_resume(void)
{
	static unsigned int backlight_array_num = 0;
	static unsigned int lcm_initialization_count = 0;

	if(cabc_enable_flag == 0)
	{
		lcm_initialization_count = sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table);
		for(backlight_array_num = lcm_initialization_count - 1; backlight_array_num >= 0; backlight_array_num--)
		{
			if(0x51 == lcm_initialization_setting[backlight_array_num].cmd)
			{
				break;
			}
		}
	}
	else
	{
		lcm_initialization_count = sizeof(lcm_cabc_on_initialization_setting) / sizeof(struct LCM_setting_table);
		for(backlight_array_num = lcm_initialization_count - 1; backlight_array_num >= 0; backlight_array_num--)
		{
			if(0x51 == lcm_cabc_on_initialization_setting[backlight_array_num].cmd)
			{
				break;
			}
		}
	}
	printk("lizhiye, lcm_initialization_count=%d, backlight_array_num=%d\n", lcm_initialization_count, backlight_array_num);

	tps65132_enable(1);
   	MDELAY(20);

	mt_set_gpio_mode(GPIO_LCD_RESET_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_RESET_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_RESET_PIN, GPIO_OUT_ONE);
	MDELAY(10);
	mt_set_gpio_out(GPIO_LCD_RESET_PIN, GPIO_OUT_ZERO);
	MDELAY(10);
	mt_set_gpio_out(GPIO_LCD_RESET_PIN, GPIO_OUT_ONE);
	MDELAY(5);
	
	// when phone initial , config output high, enable backlight drv chip  
	if(cabc_enable_flag == 0)
	{
		if(last_backlight_level <= 32)
		{		//6
			if(backlight_array_num != 0)
			{
				lcm_initialization_setting[backlight_array_num].para_list[0] = last_backlight_level;
			}
		}
		else
		{
			if(backlight_array_num != 0)
			{
				lcm_initialization_setting[backlight_array_num].para_list[0] = 32;
			}
		}
		push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1); 
	}
	else
	{
		if(last_backlight_level <= 32)
		{
			if(backlight_array_num != 0)
			{
				lcm_cabc_on_initialization_setting[backlight_array_num].para_list[0] = last_backlight_level;
			}
		}
		else
		{
			if(backlight_array_num != 0)
			{
				lcm_cabc_on_initialization_setting[backlight_array_num].para_list[0] = 32;
			}
		}
		push_table(lcm_cabc_on_initialization_setting, sizeof(lcm_cabc_on_initialization_setting) / sizeof(struct LCM_setting_table), 1); 
	}
}

#define LCM_ID_R63315 (0x3315)
static unsigned int lcm_compare_id(void)
{
	unsigned char buffer[8];
	unsigned int array[16];  
	unsigned int lcd_id = 0;

	tps65132_enable(1);
   	MDELAY(20);
	
	mt_set_gpio_mode(GPIO_LCD_RESET_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_RESET_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_RESET_PIN, GPIO_OUT_ONE);
	MDELAY(10);
	mt_set_gpio_out(GPIO_LCD_RESET_PIN, GPIO_OUT_ZERO);
	MDELAY(10);
	mt_set_gpio_out(GPIO_LCD_RESET_PIN, GPIO_OUT_ONE);
	MDELAY(10);
	
	array[0] = 0x00053700;// read id return 5 byte
	dsi_set_cmdq(array, 1, 1);
	MDELAY(20);

	array[0] = 0x04B02900;// unlock for reading ID
	dsi_set_cmdq(array, 1, 1);
	MDELAY(50);
	
	read_reg_v2(0xBF, buffer, 5);
	MDELAY(20);
	lcd_id = (buffer[2] << 8 )| buffer[3];

#ifdef BUILD_LK
    dprintf(0, "%s, LK r63315 debug: r63315 id = 0x%08x\n", __func__, lcd_id);
#else
    printk("%s, kernel r63315  debug: r63315 id = 0x%08x\n", __func__, lcd_id);
#endif

	if(lcd_id == LCM_ID_R63315)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

#ifndef BUILD_LK
static struct LCM_setting_table lcm_cabc_on_setting[]= 
{
	{0x55, 1, {0x01}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_cabc_off_setting[] = 
{
	{0x55, 1, {0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void lcm_cabc_enable_cmdq(void* handle,unsigned int enable)
{
	if(enable == 0)	//cabc off
	{
		push_table(lcm_cabc_off_setting, sizeof(lcm_cabc_off_setting) / sizeof(struct LCM_setting_table), 1);
	}
	else
	{
		push_table(lcm_cabc_on_setting, sizeof(lcm_cabc_on_setting) / sizeof(struct LCM_setting_table), 1);
	}
}
#endif

extern unsigned int esd_backlight_level;
static void lcm_setbacklight_cmdq(void* handle,unsigned int level)
{
	unsigned int cmd = 0x51;
	unsigned int count =1;
	unsigned int value = level;
	unsigned char data = 0;
	static unsigned int old_value = 0;
	static unsigned int first_vlue = 0;
	static unsigned int second_vlue = 0;

	static unsigned int first_resume_vlue = 0;
	static unsigned int second_resume_vlue = 0;
	static unsigned int resume_flag = 0;
	static unsigned int same_value_num = 0;

	first_resume_vlue = value;
	if((first_resume_vlue != 0) &&(second_resume_vlue == 0))
	{
		resume_flag = 1;
	}
	second_resume_vlue = first_resume_vlue;

	if(old_value != value)
	{
		old_value = value;
	}
	else 
	{
		if(resume_flag ==1)
		{
			same_value_num++;
			if(same_value_num >= 3)
			{
				resume_flag = 0;
				same_value_num = 0;
				return;
			}		
		}
		else
		{
			return;
		}
	}

	first_vlue = value;
#ifndef BUILD_LK
	printk("%s, sharp, line=%d, value=%d\n", __FUNCTION__, __LINE__, value);
#endif	

	if((first_vlue == 0) && (second_vlue == 0))
	{
		return;
	}

	mt_set_gpio_mode(GPIO_LCD_BL_EN_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_BL_EN_PIN, GPIO_DIR_OUT);
	if(value == 0)
	{
		mt_set_gpio_out(GPIO_LCD_BL_EN_PIN, GPIO_OUT_ZERO);
		lcm_backlight_level_setting[0].para_list[0] = value;
		push_table(lcm_backlight_level_setting, sizeof(lcm_backlight_level_setting) / sizeof(struct LCM_setting_table), 1);
	}
	else
	{
		 if((value <= 3) && (value >= 1))
		{
			value = 3;
		}
		else if(value > 255)
		{
			value = 255;
		}

		esd_backlight_level = value;	//modify for the esd backlight
		last_backlight_level = value;
		mt_set_gpio_out(GPIO_LCD_BL_EN_PIN, GPIO_OUT_ONE);
		lcm_backlight_level_setting[0].para_list[0] = value;
		push_table(lcm_backlight_level_setting, sizeof(lcm_backlight_level_setting) / sizeof(struct LCM_setting_table), 1);
	}

	second_vlue = first_vlue;
}

LCM_DRIVER r63315_fhd_sharp_phantom_lcm_drv=
{
    .name           = "r63315_fhd_sharp_phantom",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params     = lcm_get_params,
    .init           = lcm_init,
    .suspend        = lcm_suspend,
    .resume         = lcm_resume,
    .compare_id     = lcm_compare_id,  
    .set_backlight_cmdq  = lcm_setbacklight_cmdq,
    .enable_cabc_cmdq = lcm_cabc_enable_cmdq,
};
/* END PN:DTS2013053103858 , Added by d00238048, 2013.05.31*/
