
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

//#define TE_ENABLE_FUNCTION

static struct LCM_setting_table lcm_initialization_setting[] = 
{
	{0xFF, 1, {0xee}},
	{0xFb, 1, {0x01}},
	{0x18, 1, {0x40}},
	{REGFLAG_DELAY, 20, {}},
	{0x18, 1, {0x00}},
	{REGFLAG_DELAY, 20, {}},
	{0x7C, 1, {0x31}},		//abnormal poweroff to let the lcd clean current 

#if 0	//the otp
	{0xFF, 1, {0x01}},
	{0xFB, 1, {0x01}},	
	{0x75, 1, {0x00}},
	{0x76, 1, {0x31}},
	{0x77, 1, {0x00}},
	{0x78, 1, {0x4C}},
	{0x79, 1, {0x00}},
	{0x7A, 1, {0x73}},
	{0x7B, 1, {0x00}},
	{0x7C, 1, {0x90}},
	{0x7D, 1, {0x00}},
	{0x7E, 1, {0xA8}},
	{0x7F, 1, {0x00}},
	{0x80, 1, {0xBC}},
	{0x81, 1, {0x00}},
	{0x82, 1, {0xCE}},
	{0x83, 1, {0x00}},
	{0x84, 1, {0xDE}},
	{0x85, 1, {0x00}},
	{0x86, 1, {0xED}},
	{0x87, 1, {0x01}},
	{0x88, 1, {0x1D}},
	{0x89, 1, {0x01}},
	{0x8A, 1, {0x44}},
	{0x8B, 1, {0x01}},
	{0x8C, 1, {0x81}},
	{0x8D, 1, {0x01}},
	{0x8E, 1, {0xAF}},
	{0x8F, 1, {0x01}},
	{0x90, 1, {0xF7}},
	{0x91, 1, {0x02}},
	{0x92, 1, {0x2F}},
	{0x93, 1, {0x02}},
	{0x94, 1, {0x30}},
	{0x95, 1, {0x02}},
	{0x96, 1, {0x64}},
	{0x97, 1, {0x02}},
	{0x98, 1, {0xA0}},
	{0x99, 1, {0x02}},
	{0x9A, 1, {0xC8}},
	{0x9B, 1, {0x02}},
	{0x9C, 1, {0xFD}},
	{0x9D, 1, {0x03}},
	{0x9E, 1, {0x24}},
	{0x9F, 1, {0x03}},
	{0xA0, 1, {0x50}},
	{0xA2, 1, {0x03}},
	{0xA3, 1, {0x5D}},
	{0xA4, 1, {0x03}},
	{0xA5, 1, {0x6A}},
	{0xA6, 1, {0x03}},
	{0xA7, 1, {0x7A}},
	{0xA9, 1, {0x03}},
	{0xAA, 1, {0x8B}},
	{0xAB, 1, {0x03}},
	{0xAC, 1, {0x9F}},
	{0xAD, 1, {0x03}},
	{0xAE, 1, {0xB6}},
	{0xAF, 1, {0x03}},
	{0xB0, 1, {0xC9}},
	{0xB1, 1, {0x03}},
	{0xB2, 1, {0xCB}},	
	{0xB3, 1, {0x00}},
	{0xB4, 1, {0x31}},
	{0xB5, 1, {0x00}},
	{0xB6, 1, {0x4C}},
	{0xB7, 1, {0x00}},
	{0xB8, 1, {0x73}},
	{0xB9, 1, {0x00}},
	{0xBA, 1, {0x90}},
	{0xBB, 1, {0x00}},
	{0xBC, 1, {0xA8}},
	{0xBD, 1, {0x00}},
	{0xBE, 1, {0xBC}},
	{0xBF, 1, {0x00}},
	{0xC0, 1, {0xCE}},
	{0xC1, 1, {0x00}},
	{0xC2, 1, {0xDE}},
	{0xC3, 1, {0x00}},
	{0xC4, 1, {0xED}},
	{0xC5, 1, {0x01}},
	{0xC6, 1, {0x1D}},
	{0xC7, 1, {0x01}},
	{0xC8, 1, {0x44}},
	{0xC9, 1, {0x01}},
	{0xCA, 1, {0x81}},
	{0xCB, 1, {0x01}},
	{0xCC, 1, {0xAF}},
	{0xCD, 1, {0x01}},
	{0xCE, 1, {0xF7}},
	{0xCF, 1, {0x02}},
	{0xD0, 1, {0x2F}},
	{0xD1, 1, {0x02}},
	{0xD2, 1, {0x30}},
	{0xD3, 1, {0x02}},
	{0xD4, 1, {0x64}},
	{0xD5, 1, {0x02}},
	{0xD6, 1, {0xA0}},
	{0xD7, 1, {0x02}},
	{0xD8, 1, {0xC8}},
	{0xD9, 1, {0x02}},
	{0xDA, 1, {0xFD}},
	{0xDB, 1, {0x03}},
	{0xDC, 1, {0x24}},
	{0xDD, 1, {0x03}},
	{0xDE, 1, {0x50}},
	{0xDF, 1, {0x03}},
	{0xE0, 1, {0x5D}},
	{0xE1, 1, {0x03}},
	{0xE2, 1, {0x6A}},
	{0xE3, 1, {0x03}},
	{0xE4, 1, {0x7A}},
	{0xE5, 1, {0x03}},
	{0xE6, 1, {0x8B}},
	{0xE7, 1, {0x03}},
	{0xE8, 1, {0x9F}},
	{0xE9, 1, {0x03}},
	{0xEA, 1, {0xB6}},
	{0xEB, 1, {0x03}},
	{0xEC, 1, {0xC9}},
	{0xED, 1, {0x03}},
	{0xEE, 1, {0xCB}},	
	{0xEF, 1, {0x00}},
	{0xF0, 1, {0x31}},
	{0xF1, 1, {0x00}},
	{0xF2, 1, {0x4E}},
	{0xF3, 1, {0x00}},
	{0xF4, 1, {0x75}},
	{0xF5, 1, {0x00}},
	{0xF6, 1, {0x92}},
	{0xF7, 1, {0x00}},
	{0xF8, 1, {0xAA}},
	{0xF9, 1, {0x00}},
	{0xFA, 1, {0xBE}},	
	{0xFF, 1, {0x02}},
	{0xFB, 1, {0x01}},	
	{0x00, 1, {0x00}},
	{0x01, 1, {0xD0}},
	{0x02, 1, {0x00}},
	{0x03, 1, {0xE0}},
	{0x04, 1, {0x00}},
	{0x05, 1, {0xEF}},
	{0x06, 1, {0x01}},
	{0x07, 1, {0x1F}},
	{0x08, 1, {0x01}},
	{0x09, 1, {0x46}},
	{0x0A, 1, {0x01}},
	{0x0B, 1, {0x82}},
	{0x0C, 1, {0x01}},
	{0x0D, 1, {0xB1}},
	{0x0E, 1, {0x01}},
	{0x0F, 1, {0xF8}},
	{0x10, 1, {0x02}},
	{0x11, 1, {0x31}},
	{0x12, 1, {0x02}},
	{0x13, 1, {0x32}},
	{0x14, 1, {0x02}},
	{0x15, 1, {0x65}},
	{0x16, 1, {0x02}},
	{0x17, 1, {0xA0}},
	{0x18, 1, {0x02}},
	{0x19, 1, {0xC7}},
	{0x1A, 1, {0x02}},
	{0x1B, 1, {0xFD}},
	{0x1C, 1, {0x03}},
	{0x1D, 1, {0x21}},
	{0x1E, 1, {0x03}},
	{0x1F, 1, {0x4D}},
	{0x20, 1, {0x03}},
	{0x21, 1, {0x5A}},
	{0x22, 1, {0x03}},
	{0x23, 1, {0x68}},
	{0x24, 1, {0x03}},
	{0x25, 1, {0x77}},
	{0x26, 1, {0x03}},
	{0x27, 1, {0x89}},
	{0x28, 1, {0x03}},
	{0x29, 1, {0x9E}},
	{0x2A, 1, {0x03}},
	{0x2B, 1, {0xB7}},
	{0x2D, 1, {0x03}},
	{0x2F, 1, {0xC9}},
	{0x30, 1, {0x03}},
	{0x31, 1, {0xCB}},
	{0x32, 1, {0x00}},
	{0x33, 1, {0x31}},
	{0x34, 1, {0x00}},
	{0x35, 1, {0x4E}},
	{0x36, 1, {0x00}},
	{0x37, 1, {0x75}},
	{0x38, 1, {0x00}},
	{0x39, 1, {0x92}},
	{0x3A, 1, {0x00}},
	{0x3B, 1, {0xAA}},
	{0x3D, 1, {0x00}},
	{0x3F, 1, {0xBE}},
	{0x40, 1, {0x00}},
	{0x41, 1, {0xD0}},
	{0x42, 1, {0x00}},
	{0x43, 1, {0xE0}},
	{0x44, 1, {0x00}},
	{0x45, 1, {0xEF}},
	{0x46, 1, {0x01}},
	{0x47, 1, {0x1F}},
	{0x48, 1, {0x01}},
	{0x49, 1, {0x46}},
	{0x4A, 1, {0x01}},
	{0x4B, 1, {0x82}},
	{0x4C, 1, {0x01}},
	{0x4D, 1, {0xB1}},
	{0x4E, 1, {0x01}},
	{0x4F, 1, {0xF8}},
	{0x50, 1, {0x02}},
	{0x51, 1, {0x31}},
	{0x52, 1, {0x02}},
	{0x53, 1, {0x32}},
	{0x54, 1, {0x02}},
	{0x55, 1, {0x65}},
	{0x56, 1, {0x02}},
	{0x58, 1, {0xA0}},
	{0x59, 1, {0x02}},
	{0x5A, 1, {0xC7}},
	{0x5B, 1, {0x02}},
	{0x5C, 1, {0xFD}},
	{0x5D, 1, {0x03}},
	{0x5E, 1, {0x21}},
	{0x5F, 1, {0x03}},
	{0x60, 1, {0x4D}},
	{0x61, 1, {0x03}},
	{0x62, 1, {0x5A}},
	{0x63, 1, {0x03}},
	{0x64, 1, {0x68}},
	{0x65, 1, {0x03}},
	{0x66, 1, {0x77}},
	{0x67, 1, {0x03}},
	{0x68, 1, {0x89}},
	{0x69, 1, {0x03}},
	{0x6A, 1, {0x9E}},
	{0x6B, 1, {0x03}},
	{0x6C, 1, {0xB7}},
	{0x6D, 1, {0x03}},
	{0x6E, 1, {0xC9}},
	{0x6F, 1, {0x03}},
	{0x70, 1, {0xCB}},
	{0x71, 1, {0x00}},
	{0x72, 1, {0x66}},
	{0x73, 1, {0x00}},
	{0x74, 1, {0x77}},
	{0x75, 1, {0x00}},
	{0x76, 1, {0x94}},
	{0x77, 1, {0x00}},
	{0x78, 1, {0xAC}},
	{0x79, 1, {0x00}},
	{0x7A, 1, {0xC0}},
	{0x7B, 1, {0x00}},
	{0x7C, 1, {0xD2}},
	{0x7D, 1, {0x00}},
	{0x7E, 1, {0xE2}},
	{0x7F, 1, {0x00}},
	{0x80, 1, {0xF1}},
	{0x81, 1, {0x00}},
	{0x82, 1, {0xFE}},
	{0x83, 1, {0x01}},
	{0x84, 1, {0x2C}},
	{0x85, 1, {0x01}},
	{0x86, 1, {0x51}},
	{0x87, 1, {0x01}},
	{0x88, 1, {0x89}},
	{0x89, 1, {0x01}},
	{0x8A, 1, {0xB5}},
	{0x8B, 1, {0x01}},
	{0x8C, 1, {0xFA}},
	{0x8D, 1, {0x02}},
	{0x8E, 1, {0x32}},
	{0x8F, 1, {0x02}},
	{0x90, 1, {0x33}},
	{0x91, 1, {0x02}},
	{0x92, 1, {0x66}},
	{0x93, 1, {0x02}},
	{0x94, 1, {0xA1}},
	{0x95, 1, {0x02}},
	{0x96, 1, {0xCB}},
	{0x97, 1, {0x03}},
	{0x98, 1, {0x05}},
	{0x99, 1, {0x03}},
	{0x9A, 1, {0x2E}},
	{0x9B, 1, {0x03}},
	{0x9C, 1, {0x7D}},
	{0x9D, 1, {0x03}},
	{0x9E, 1, {0x9B}},
	{0x9F, 1, {0x03}},
	{0xA0, 1, {0xA1}},
	{0xA2, 1, {0x03}},
	{0xA3, 1, {0xA2}},
	{0xA4, 1, {0x03}},
	{0xA5, 1, {0xA3}},
	{0xA6, 1, {0x03}},
	{0xA7, 1, {0xAA}},
	{0xA9, 1, {0x03}},
	{0xAA, 1, {0xBF}},
	{0xAB, 1, {0x03}},
	{0xAC, 1, {0xCA}},
	{0xAD, 1, {0x03}},
	{0xAE, 1, {0xCB}},
	{0xAF, 1, {0x00}},
	{0xB0, 1, {0x66}},
	{0xB1, 1, {0x00}},
	{0xB2, 1, {0x77}},
	{0xB3, 1, {0x00}},
	{0xB4, 1, {0x94}},
	{0xB5, 1, {0x00}},
	{0xB6, 1, {0xAC}},
	{0xB7, 1, {0x00}},
	{0xB8, 1, {0xC0}},
	{0xB9, 1, {0x00}},
	{0xBA, 1, {0xD2}},
	{0xBB, 1, {0x00}},
	{0xBC, 1, {0xE2}},
	{0xBD, 1, {0x00}},
	{0xBE, 1, {0xF1}},
	{0xBF, 1, {0x00}},
	{0xC0, 1, {0xFE}},
	{0xC1, 1, {0x01}},
	{0xC2, 1, {0x2C}},
	{0xC3, 1, {0x01}},
	{0xC4, 1, {0x51}},
	{0xC5, 1, {0x01}},
	{0xC6, 1, {0x89}},
	{0xC7, 1, {0x01}},
	{0xC8, 1, {0xB5}},
	{0xC9, 1, {0x01}},
	{0xCA, 1, {0xFA}},
	{0xCB, 1, {0x02}},
	{0xCC, 1, {0x32}},
	{0xCD, 1, {0x02}},
	{0xCE, 1, {0x33}},
	{0xCF, 1, {0x02}},
	{0xD0, 1, {0x66}},
	{0xD1, 1, {0x02}},
	{0xD2, 1, {0xA1}},
	{0xD3, 1, {0x02}},
	{0xD4, 1, {0xCB}},
	{0xD5, 1, {0x03}},
	{0xD6, 1, {0x05}},
	{0xD7, 1, {0x03}},
	{0xD8, 1, {0x2E}},
	{0xD9, 1, {0x03}},
	{0xDA, 1, {0x7D}},
	{0xDB, 1, {0x03}},
	{0xDC, 1, {0x9B}},
	{0xDD, 1, {0x03}},
	{0xDE, 1, {0xA1}},
	{0xDF, 1, {0x03}},
	{0xE0, 1, {0xA2}},
	{0xE1, 1, {0x03}},
	{0xE2, 1, {0xA3}},
	{0xE3, 1, {0x03}},
	{0xE4, 1, {0xAA}},
	{0xE5, 1, {0x03}},
	{0xE6, 1, {0xBF}},
	{0xE7, 1, {0x03}},
	{0xE8, 1, {0xCA}},
	{0xE9, 1, {0x03}},
	{0xEA, 1, {0xCB}},
	{0xFF, 1, {0x00}},
#endif

	
#if 1		//close the hsync scan back function
	{0xFF, 1, {0x05}},
	{0xFB, 1, {0x01}},
	{0xE7, 1, {0x00}},		
#endif

#if 1		//backlight pwm freq: 146/6Khz
	{0xFF, 1, {0x04}},
	{0xFB, 1, {0x01}},
	{0x08, 1, {0x06}},		
#endif

#if 0	//the bit test for lcm
	{0xFF, 1, {0x05}},
	{0xFb, 1, {0x01}},
	{0xEA, 1, {0x04}},	
	{0xEB, 1, {0x00}},	
	{0xEC, 1, {0x01}},	
#endif
	
	{0xFF, 1, {0x00}},
	{0xFb, 1, {0x01}},
	
	{0x51, 1, {0xff}},
	{0x53, 1, {0x24}},		// 2c
	{0x55, 1, {0x00}},		//cabc function: 0: close, 1: open

	{0xD3, 1, {0x06}},
	{0xD4, 1, {0x04}},

#ifdef TE_ENABLE_FUNCTION
	{0x35, 1, {0x00}},
#endif

	{0x11, 0, {}},
	{REGFLAG_DELAY, 120, {}},
	{0x29, 0, {}},
	{REGFLAG_DELAY, 20, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_suspend_setting[] = 
{
	{0x28,0,{}},
	{REGFLAG_DELAY, 20, {}},

	{0x10,0,{}},
	{REGFLAG_DELAY, 120, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_backlight_level_setting[] = 
{
	{0x51, 2, {0xff, 0x00}},
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
	params->module="auo";
	params->vendor="auo";
	params->ic="nt35596";
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

	params->dsi.vertical_sync_active				= 2;
	params->dsi.vertical_backporch					= 4;
	params->dsi.vertical_frontporch					= 4;
	params->dsi.vertical_active_line				= FRAME_HEIGHT; 

	params->dsi.horizontal_sync_active				= 4;
	params->dsi.horizontal_backporch				= 76;
	params->dsi.horizontal_frontporch				= 76;
	params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

	// Bit rate calculation
	//1 Every lane speed
#if 1
	params->dsi.PLL_CLOCK=475;
#else
	params->dsi.pll_div1=0;		// div1=0,1,2,3;div1_real=1,2,4,4 ----0: 546Mbps  1:273Mbps
	params->dsi.pll_div2=0;		// div2=0,1,2,3;div1_real=1,2,4,4	
	params->dsi.fbk_div =0x12;    // fref=26MHz, fvco=fref*(fbk_div+1)*2/(div1_real*div2_real)	
#endif

#ifdef TE_ENABLE_FUNCTION
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 0;
#else
//	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd          = 0x0a;
	params->dsi.lcm_esd_check_table[0].count        = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9c;
#endif	
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
				dprintf(0, "nt35596--auo--tps65132_enable----cmd=0x00--i2c write error--num=%d\n", num);		
				MDELAY(5);
			}
			else
			{
				dprintf(0, "nt35596--auo--tps65132_enable----cmd=0x00--i2c write success--num=%d\n", num);
				break;
			}
		}

		for(num = 0; num < 3; num++)
		{	
			ret=TPS65132_write_byte(0x01,0x0f);
			if(ret) 
			{
				dprintf(0, "nt35596--auo--tps65132_enable----cmd=0x01--i2c write error--num=%d\n", num);
				MDELAY(5);
			}
			else
			{
				dprintf(0, "nt35596--auo--tps65132_enable----cmd=0x01--i2c write success--num=%d\n", num);   
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
				printk("nt35596--auo--tps65132_enable-cmd=0x00-- i2c write error-num=%d\n", num);
				MDELAY(5);
			}
			else
			{
				printk("nt35596--auo--tps65132_enable-cmd=0x00-- i2c write success-num=%d\n", num);
				break;
			}
		}

		for(num = 0; num < 3; num++)
		{	
			ret=tps65132_write_bytes(0x01,0x0f);
			if(ret<0)
			{
				printk("nt35596--auo--tps65132_enable-cmd=0x01-- i2c write error-num=%d\n", num);
				MDELAY(5);
			}
			else
			{
				printk("nt35596--auo--tps65132_enable-cmd=0x01-- i2c write success-num=%d\n", num);
				break;
			}
		}
	#endif
	}
	else
	{
	#ifndef BUILD_LK
		printk("[KERNEL]nt35596--auo--tps65132_enable-----sleep--\n");
	#endif
		mt_set_gpio_out(GPIO_LCD_BIAS_ENN_PIN, GPIO_OUT_ZERO);
		MDELAY(12);
		mt_set_gpio_out(GPIO_LCD_BIAS_ENP_PIN, GPIO_OUT_ZERO);
		MDELAY(12);
	}
}

static void lcm_init(void)
{
	tps65132_enable(1);
   	MDELAY(20);

	mt_set_gpio_mode(GPIO_LCD_RESET_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_RESET_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_RESET_PIN, GPIO_OUT_ONE);
	MDELAY(20);
	mt_set_gpio_out(GPIO_LCD_RESET_PIN, GPIO_OUT_ZERO);
	MDELAY(20);
	mt_set_gpio_out(GPIO_LCD_RESET_PIN, GPIO_OUT_ONE);
	MDELAY(50);
	mt_set_gpio_out(GPIO_LCD_RESET_PIN, GPIO_OUT_ZERO);
	MDELAY(50);
	mt_set_gpio_out(GPIO_LCD_RESET_PIN, GPIO_OUT_ONE);
	MDELAY(50);
	
	// when phone initial , config output high, enable backlight drv chip  
	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1); 

	MDELAY(20);
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

static void lcm_resume(void)
{
	tps65132_enable(1);
   	MDELAY(15);

	mt_set_gpio_mode(GPIO_LCD_RESET_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_RESET_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_RESET_PIN, GPIO_OUT_ONE);
	MDELAY(3);
	mt_set_gpio_out(GPIO_LCD_RESET_PIN, GPIO_OUT_ZERO);
	MDELAY(3);
	mt_set_gpio_out(GPIO_LCD_RESET_PIN, GPIO_OUT_ONE);
	MDELAY(3);
	mt_set_gpio_out(GPIO_LCD_RESET_PIN, GPIO_OUT_ZERO);
	MDELAY(3);
	mt_set_gpio_out(GPIO_LCD_RESET_PIN, GPIO_OUT_ONE);
	MDELAY(22);
	
	// when phone initial , config output high, enable backlight drv chip  
	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1); 
//	MDELAY(20);
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

#define LCM_ID_NT35596 (0x96)

static unsigned int lcm_compare_id(void)
{

	unsigned char buffer[5];
	unsigned int array[16];  
	unsigned int lcd_id = 0;
    unsigned char LCD_ID_value = 0;
	LCD_ID_value = which_lcd_module_triple();
#ifdef BUILD_LK
    dprintf(0, "%s, LK LCD_ID_value = 0x%x\n", __func__, LCD_ID_value);
#else
    printk("%s, kernel LCD_ID_value = 0x%x\n", __func__, LCD_ID_value);
#endif

	if(LCD_MODULE_ID != LCD_ID_value)
	{
		#ifdef BUILD_LK
		    dprintf(0, "%s, LK not the NT35596 auo\n", __func__);
		#else
		    printk("%s, kernel not the NT35596 auo\n", __func__);
		#endif
		return 0;
	}

	mt_set_gpio_mode(GPIO_LCD_RESET_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_RESET_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_RESET_PIN, GPIO_OUT_ONE);
	MDELAY(1);
	mt_set_gpio_out(GPIO_LCD_RESET_PIN, GPIO_OUT_ZERO);
	MDELAY(10);
	mt_set_gpio_out(GPIO_LCD_RESET_PIN, GPIO_OUT_ONE);
	MDELAY(10);

	array[0] = 0x00023700;// read id return two byte,version and id
	dsi_set_cmdq(array, 1, 1);
	array[0] = 0x00ff1500;
	dsi_set_cmdq(array, 1, 1); 
	array[0] = 0x01fb1500;
	dsi_set_cmdq(array, 1, 1); 
	MDELAY(10);
	read_reg_v2(0xf4, buffer, 1);
	MDELAY(20);
	lcd_id = buffer[0];

#ifdef BUILD_LK
    dprintf(0, "%s, LK NT35596 debug: NT35596 id = 0x%08x\n", __func__, lcd_id);
#else
    printk("%s, kernel NT35596 horse debug: NT35596 id = 0x%08x\n", __func__, lcd_id);
#endif

	if(lcd_id == LCM_ID_NT35596)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

static void lcm_setbacklight_cmdq(void* handle,unsigned int level)
{
	unsigned int cmd = 0x51;
	unsigned int count =1;
	unsigned int value = level;
	unsigned char data = 0;

	mt_set_gpio_mode(GPIO_LCD_BL_EN_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_BL_EN_PIN, GPIO_DIR_OUT);
	if(value == 0)
	{
		mt_set_gpio_out(GPIO_LCD_BL_EN_PIN, GPIO_OUT_ZERO);
		MDELAY(30);

		lcm_backlight_level_setting[0].para_list[0] = value;
		push_table(lcm_backlight_level_setting, sizeof(lcm_backlight_level_setting) / sizeof(struct LCM_setting_table), 1);
	}
	else
	{
		 if((value <= 4) && (value >= 1))
		{
			value = 4;
		}
		else if(value > 255)
		{
			value = 255;
		}
		
		mt_set_gpio_out(GPIO_LCD_BL_EN_PIN, GPIO_OUT_ONE);
		MDELAY(10);
		lcm_backlight_level_setting[0].para_list[0] = value;
		push_table(lcm_backlight_level_setting, sizeof(lcm_backlight_level_setting) / sizeof(struct LCM_setting_table), 1);
	}
}

LCM_DRIVER nt35596_fhd_auo_phantom_lcm_drv=
{
    .name           = "nt35596_fhd_auo_phantom",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params     = lcm_get_params,
    .init           = lcm_init,
    .suspend        = lcm_suspend,
    .resume         = lcm_resume,
    .compare_id     = lcm_compare_id,  
    .set_backlight_cmdq  = lcm_setbacklight_cmdq,
#if (LCM_DSI_CMD_MODE)
    .update         = lcm_update,
#endif
};
/* END PN:DTS2013053103858 , Added by d00238048, 2013.05.31*/
