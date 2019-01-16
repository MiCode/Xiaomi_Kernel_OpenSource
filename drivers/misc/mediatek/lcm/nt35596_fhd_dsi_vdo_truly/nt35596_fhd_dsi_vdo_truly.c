#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
	#include <platform/mt_gpio.h>
	#include <string.h>
#elif defined(BUILD_UBOOT)
	#include <asm/arch/mt_gpio.h>
#else
	#include <mach/mt_gpio.h>
#endif
// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  (1080)
#define FRAME_HEIGHT (1920)

#define LCM_ID_NT35590 (0x90)

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

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	        lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)											lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)   

#define dsi_lcm_set_gpio_out(pin, out)										lcm_util.set_gpio_out(pin, out)
#define dsi_lcm_set_gpio_mode(pin, mode)									lcm_util.set_gpio_mode(pin, mode)
#define dsi_lcm_set_gpio_dir(pin, dir)										lcm_util.set_gpio_dir(pin, dir)
#define dsi_lcm_set_gpio_pull_enable(pin, en)								lcm_util.set_gpio_pull_enable)(pin, en)

#define   LCM_DSI_CMD_MODE							0

static bool lcm_is_init = false;

void TC358768_DCS_write_1A_1P(unsigned char cmd, unsigned char para)
{
	unsigned int data_array[16];
	//unsigned char buffer;

#if 0//ndef BUILD_LK

	do {
		data_array[0] =(0x00001500 | (para<<24) | (cmd<<16));
		dsi_set_cmdq(data_array, 1, 1);

		if (cmd == 0xFF)
			break;

		read_reg_v2(cmd, &buffer, 1);

		if(buffer != para)
			printk("%s, data_array = 0x%08x, (cmd, para, back) = (0x%02x, 0x%02x, 0x%02x)\n", __func__, data_array[0], cmd, para, buffer);	

		MDELAY(1);

	} while (buffer != para);

#else

	data_array[0] =(0x00001500 | (para<<24) | (cmd<<16));
	dsi_set_cmdq(data_array, 1, 1);

	//MDELAY(1);

#endif

}

#define TC358768_DCS_write_1A_0P(cmd)							data_array[0]=(0x00000500 | (cmd<<16)); \
																dsi_set_cmdq(data_array, 1, 1);																									

static void init_lcm_registers(void)
{
		unsigned int data_array[16];
		//unsigned char buffer[8];

#if 0//ndef BUILD_LK
		data_array[0] = 0x00013700;// read id return two byte,version and id
		dsi_set_cmdq(data_array, 1, 1);
#endif

#if 1
		TC358768_DCS_write_1A_1P(0xFF,0xEE);
		TC358768_DCS_write_1A_1P(0xFB,0x01);
		TC358768_DCS_write_1A_1P(0x1F,0x45);
		TC358768_DCS_write_1A_1P(0x24,0x4F);
		TC358768_DCS_write_1A_1P(0x38,0xC8);
		TC358768_DCS_write_1A_1P(0x39,0x2C);
		TC358768_DCS_write_1A_1P(0x1E,0xBB);
		TC358768_DCS_write_1A_1P(0x1D,0x0F);
		TC358768_DCS_write_1A_1P(0x7E,0xB1);
		//
		TC358768_DCS_write_1A_1P(0xFF,0x00);
		TC358768_DCS_write_1A_1P(0xFB,0x01);
		TC358768_DCS_write_1A_1P(0x35,0x01);
		//
		TC358768_DCS_write_1A_1P(0xFF,0x01);
		TC358768_DCS_write_1A_1P(0xFB,0x01);
		TC358768_DCS_write_1A_1P(0x00,0x01);
		TC358768_DCS_write_1A_1P(0x01,0x55);
		TC358768_DCS_write_1A_1P(0x02,0x40);
		TC358768_DCS_write_1A_1P(0x05,0x00);
		TC358768_DCS_write_1A_1P(0x06,0x1B);
		TC358768_DCS_write_1A_1P(0x07,0x24);
		TC358768_DCS_write_1A_1P(0x08,0x0C);
		TC358768_DCS_write_1A_1P(0x0B,0x87);
		TC358768_DCS_write_1A_1P(0x0C,0x87);
		TC358768_DCS_write_1A_1P(0x0E,0xB0);
		TC358768_DCS_write_1A_1P(0x0F,0xB3);
		TC358768_DCS_write_1A_1P(0x11,0x10);
		TC358768_DCS_write_1A_1P(0x12,0x10);
		TC358768_DCS_write_1A_1P(0x13,0x05);
		TC358768_DCS_write_1A_1P(0x14,0x4A);
		TC358768_DCS_write_1A_1P(0x15,0x18);
		TC358768_DCS_write_1A_1P(0x16,0x18);
		TC358768_DCS_write_1A_1P(0x18,0x00);
		TC358768_DCS_write_1A_1P(0x19,0x77);
		TC358768_DCS_write_1A_1P(0x1A,0x55);
		TC358768_DCS_write_1A_1P(0x1B,0x13);
		TC358768_DCS_write_1A_1P(0x1C,0x00);
		TC358768_DCS_write_1A_1P(0x1D,0x00);
		TC358768_DCS_write_1A_1P(0x1E,0x13);
		TC358768_DCS_write_1A_1P(0x1F,0x00);
		TC358768_DCS_write_1A_1P(0x23,0x00);
		TC358768_DCS_write_1A_1P(0x24,0x00);
		TC358768_DCS_write_1A_1P(0x25,0x00);
		TC358768_DCS_write_1A_1P(0x26,0x00);
		TC358768_DCS_write_1A_1P(0x27,0x00);
		TC358768_DCS_write_1A_1P(0x28,0x00);
		TC358768_DCS_write_1A_1P(0x35,0x00);
		TC358768_DCS_write_1A_1P(0x66,0x00);
		TC358768_DCS_write_1A_1P(0x58,0x82);
		TC358768_DCS_write_1A_1P(0x59,0x02);
		TC358768_DCS_write_1A_1P(0x5A,0x02);
		TC358768_DCS_write_1A_1P(0x5B,0x02);
		TC358768_DCS_write_1A_1P(0x5C,0x82);
		TC358768_DCS_write_1A_1P(0x5D,0x82);
		TC358768_DCS_write_1A_1P(0x5E,0x02);
		TC358768_DCS_write_1A_1P(0x5F,0x02);
		TC358768_DCS_write_1A_1P(0x72,0x31);
		//
		TC358768_DCS_write_1A_1P(0xFF,0x05);
		TC358768_DCS_write_1A_1P(0xFB,0x01);
		TC358768_DCS_write_1A_1P(0x00,0x01);
		TC358768_DCS_write_1A_1P(0x01,0x0B);
		TC358768_DCS_write_1A_1P(0x02,0x0C);
		TC358768_DCS_write_1A_1P(0x03,0x09);
		TC358768_DCS_write_1A_1P(0x04,0x0A);
		TC358768_DCS_write_1A_1P(0x05,0x00);
		TC358768_DCS_write_1A_1P(0x06,0x0F);
		TC358768_DCS_write_1A_1P(0x07,0x10);
		TC358768_DCS_write_1A_1P(0x08,0x00);
		TC358768_DCS_write_1A_1P(0x09,0x00);
		TC358768_DCS_write_1A_1P(0x0A,0x00);
		TC358768_DCS_write_1A_1P(0x0B,0x00);
		TC358768_DCS_write_1A_1P(0x0C,0x00);
		TC358768_DCS_write_1A_1P(0x0D,0x13);
		TC358768_DCS_write_1A_1P(0x0E,0x15);
		TC358768_DCS_write_1A_1P(0x0F,0x17);
		TC358768_DCS_write_1A_1P(0x10,0x01);
		TC358768_DCS_write_1A_1P(0x11,0x0B);
		TC358768_DCS_write_1A_1P(0x12,0x0C);
		TC358768_DCS_write_1A_1P(0x13,0x09);
		TC358768_DCS_write_1A_1P(0x14,0x0A);
		TC358768_DCS_write_1A_1P(0x15,0x00);
		TC358768_DCS_write_1A_1P(0x16,0x0F);
		TC358768_DCS_write_1A_1P(0x17,0x10);
		TC358768_DCS_write_1A_1P(0x18,0x00);
		TC358768_DCS_write_1A_1P(0x19,0x00);
		TC358768_DCS_write_1A_1P(0x1A,0x00);
		TC358768_DCS_write_1A_1P(0x1B,0x00);
		TC358768_DCS_write_1A_1P(0x1C,0x00);
		TC358768_DCS_write_1A_1P(0x1D,0x13);
		TC358768_DCS_write_1A_1P(0x1E,0x15);
		TC358768_DCS_write_1A_1P(0x1F,0x17);
		TC358768_DCS_write_1A_1P(0x20,0x00);
		TC358768_DCS_write_1A_1P(0x21,0x03);
		TC358768_DCS_write_1A_1P(0x22,0x01);
		TC358768_DCS_write_1A_1P(0x23,0x40);
		TC358768_DCS_write_1A_1P(0x24,0x40);
		TC358768_DCS_write_1A_1P(0x25,0xED);
		TC358768_DCS_write_1A_1P(0x29,0x58);
		TC358768_DCS_write_1A_1P(0x2A,0x12);
		TC358768_DCS_write_1A_1P(0x2B,0x01);
		TC358768_DCS_write_1A_1P(0x4B,0x06);
		TC358768_DCS_write_1A_1P(0x4C,0x11);
		TC358768_DCS_write_1A_1P(0x4D,0x20);
		TC358768_DCS_write_1A_1P(0x4E,0x02);
		TC358768_DCS_write_1A_1P(0x4F,0x02);
		TC358768_DCS_write_1A_1P(0x50,0x20);
		TC358768_DCS_write_1A_1P(0x51,0x61);
		TC358768_DCS_write_1A_1P(0x52,0x01);
		TC358768_DCS_write_1A_1P(0x53,0x63);
		TC358768_DCS_write_1A_1P(0x54,0x77);
		TC358768_DCS_write_1A_1P(0x55,0xED);
		TC358768_DCS_write_1A_1P(0x5B,0x00);
		TC358768_DCS_write_1A_1P(0x5C,0x00);
		TC358768_DCS_write_1A_1P(0x5D,0x00);
		TC358768_DCS_write_1A_1P(0x5E,0x00);
		TC358768_DCS_write_1A_1P(0x5F,0x15);
		TC358768_DCS_write_1A_1P(0x60,0x75);
		TC358768_DCS_write_1A_1P(0x61,0x00);
		TC358768_DCS_write_1A_1P(0x62,0x00);
		TC358768_DCS_write_1A_1P(0x63,0x00);
		TC358768_DCS_write_1A_1P(0x64,0x00);
		TC358768_DCS_write_1A_1P(0x65,0x00);
		TC358768_DCS_write_1A_1P(0x66,0x00);
		TC358768_DCS_write_1A_1P(0x67,0x00);
		TC358768_DCS_write_1A_1P(0x68,0x04);
		TC358768_DCS_write_1A_1P(0x69,0x00);
		TC358768_DCS_write_1A_1P(0x6A,0x00);
		TC358768_DCS_write_1A_1P(0x6C,0x40);
		TC358768_DCS_write_1A_1P(0x75,0x01);
		TC358768_DCS_write_1A_1P(0x76,0x01);
		TC358768_DCS_write_1A_1P(0x7A,0x80);
		TC358768_DCS_write_1A_1P(0x7B,0xC5);
		TC358768_DCS_write_1A_1P(0x7C,0xD8);
		TC358768_DCS_write_1A_1P(0x7D,0x60);
		TC358768_DCS_write_1A_1P(0x7F,0x10);
		TC358768_DCS_write_1A_1P(0x80,0x81);
		TC358768_DCS_write_1A_1P(0x83,0x05);
		TC358768_DCS_write_1A_1P(0x93,0x08);
		TC358768_DCS_write_1A_1P(0x94,0x10);
		TC358768_DCS_write_1A_1P(0x8A,0x00);
		TC358768_DCS_write_1A_1P(0x9B,0x0F);
		TC358768_DCS_write_1A_1P(0xEA,0xFF);
		TC358768_DCS_write_1A_1P(0xEC,0x00);
		//
		//
		
		//page selection cmd start
		TC358768_DCS_write_1A_1P(0xFF,0x01);
		TC358768_DCS_write_1A_1P(0xFB,0x01);
		//page selection cmd end
		//R(+) MCR cmd
		TC358768_DCS_write_1A_1P(0x75,0x00);
		TC358768_DCS_write_1A_1P(0x76,0x8E);
		TC358768_DCS_write_1A_1P(0x77,0x00);
		TC358768_DCS_write_1A_1P(0x78,0x90);
		TC358768_DCS_write_1A_1P(0x79,0x00);
		TC358768_DCS_write_1A_1P(0x7A,0xB2);
		TC358768_DCS_write_1A_1P(0x7B,0x00);
		TC358768_DCS_write_1A_1P(0x7C,0xC7);
		TC358768_DCS_write_1A_1P(0x7D,0x00);
		TC358768_DCS_write_1A_1P(0x7E,0xD7);
		TC358768_DCS_write_1A_1P(0x7F,0x00);
		TC358768_DCS_write_1A_1P(0x80,0xE9);
		TC358768_DCS_write_1A_1P(0x81,0x00);
		TC358768_DCS_write_1A_1P(0x82,0xF9);
		TC358768_DCS_write_1A_1P(0x83,0x01);
		TC358768_DCS_write_1A_1P(0x84,0x01);
		TC358768_DCS_write_1A_1P(0x85,0x01);
		TC358768_DCS_write_1A_1P(0x86,0x0B);
		TC358768_DCS_write_1A_1P(0x87,0x01);
		TC358768_DCS_write_1A_1P(0x88,0x3A);
		TC358768_DCS_write_1A_1P(0x89,0x01);
		TC358768_DCS_write_1A_1P(0x8A,0x5D);
		TC358768_DCS_write_1A_1P(0x8B,0x01);
		TC358768_DCS_write_1A_1P(0x8C,0x94);
		TC358768_DCS_write_1A_1P(0x8D,0x01);
		TC358768_DCS_write_1A_1P(0x8E,0xBC);
		TC358768_DCS_write_1A_1P(0x8F,0x02);
		TC358768_DCS_write_1A_1P(0x90,0x00);
		TC358768_DCS_write_1A_1P(0x91,0x02);
		TC358768_DCS_write_1A_1P(0x92,0x39);
		TC358768_DCS_write_1A_1P(0x93,0x02);
		TC358768_DCS_write_1A_1P(0x94,0x3A);
		TC358768_DCS_write_1A_1P(0x95,0x02);
		TC358768_DCS_write_1A_1P(0x96,0x6B);
		TC358768_DCS_write_1A_1P(0x97,0x02);
		TC358768_DCS_write_1A_1P(0x98,0xA2);
		TC358768_DCS_write_1A_1P(0x99,0x02);
		TC358768_DCS_write_1A_1P(0x9A,0xC7);
		TC358768_DCS_write_1A_1P(0x9B,0x02);
		TC358768_DCS_write_1A_1P(0x9C,0xFB);
		TC358768_DCS_write_1A_1P(0x9D,0x03);
		TC358768_DCS_write_1A_1P(0x9E,0x20);
		TC358768_DCS_write_1A_1P(0x9F,0x03);
		TC358768_DCS_write_1A_1P(0xA0,0x54);
		TC358768_DCS_write_1A_1P(0xA2,0x03);
		TC358768_DCS_write_1A_1P(0xA3,0x6D);
		TC358768_DCS_write_1A_1P(0xA4,0x03);
		TC358768_DCS_write_1A_1P(0xA5,0x80);
		TC358768_DCS_write_1A_1P(0xA6,0x03);
		TC358768_DCS_write_1A_1P(0xA7,0x81);
		TC358768_DCS_write_1A_1P(0xA9,0x03);
		TC358768_DCS_write_1A_1P(0xAA,0xC7);
		TC358768_DCS_write_1A_1P(0xAB,0x03);
		TC358768_DCS_write_1A_1P(0xAC,0xF0);
		TC358768_DCS_write_1A_1P(0xAD,0x03);
		TC358768_DCS_write_1A_1P(0xAE,0xF8);
		TC358768_DCS_write_1A_1P(0xAF,0x03);
		TC358768_DCS_write_1A_1P(0xB0,0xFD);
		TC358768_DCS_write_1A_1P(0xB1,0x03);
		TC358768_DCS_write_1A_1P(0xB2,0xFE);
		//R(-) MCR cmd                      
		TC358768_DCS_write_1A_1P(0xB3,0x00);
		TC358768_DCS_write_1A_1P(0xB4,0x8E);
		TC358768_DCS_write_1A_1P(0xB5,0x00);
		TC358768_DCS_write_1A_1P(0xB6,0x90);
		TC358768_DCS_write_1A_1P(0xB7,0x00);
		TC358768_DCS_write_1A_1P(0xB8,0xB2);
		TC358768_DCS_write_1A_1P(0xB9,0x00);
		TC358768_DCS_write_1A_1P(0xBA,0xC7);
		TC358768_DCS_write_1A_1P(0xBB,0x00);
		TC358768_DCS_write_1A_1P(0xBC,0xD7);
		TC358768_DCS_write_1A_1P(0xBD,0x00);
		TC358768_DCS_write_1A_1P(0xBE,0xE9);
		TC358768_DCS_write_1A_1P(0xBF,0x00);
		TC358768_DCS_write_1A_1P(0xC0,0xF9);
		TC358768_DCS_write_1A_1P(0xC1,0x01);
		TC358768_DCS_write_1A_1P(0xC2,0x01);
		TC358768_DCS_write_1A_1P(0xC3,0x01);
		TC358768_DCS_write_1A_1P(0xC4,0x0B);
		TC358768_DCS_write_1A_1P(0xC5,0x01);
		TC358768_DCS_write_1A_1P(0xC6,0x3A);
		TC358768_DCS_write_1A_1P(0xC7,0x01);
		TC358768_DCS_write_1A_1P(0xC8,0x5D);
		TC358768_DCS_write_1A_1P(0xC9,0x01);
		TC358768_DCS_write_1A_1P(0xCA,0x94);
		TC358768_DCS_write_1A_1P(0xCB,0x01);
		TC358768_DCS_write_1A_1P(0xCC,0xBC);
		TC358768_DCS_write_1A_1P(0xCD,0x02);
		TC358768_DCS_write_1A_1P(0xCE,0x00);
		TC358768_DCS_write_1A_1P(0xCF,0x02);
		TC358768_DCS_write_1A_1P(0xD0,0x39);
		TC358768_DCS_write_1A_1P(0xD1,0x02);
		TC358768_DCS_write_1A_1P(0xD2,0x3A);
		TC358768_DCS_write_1A_1P(0xD3,0x02);
		TC358768_DCS_write_1A_1P(0xD4,0x6B);
		TC358768_DCS_write_1A_1P(0xD5,0x02);
		TC358768_DCS_write_1A_1P(0xD6,0xA2);
		TC358768_DCS_write_1A_1P(0xD7,0x02);
		TC358768_DCS_write_1A_1P(0xD8,0xC7);
		TC358768_DCS_write_1A_1P(0xD9,0x02);
		TC358768_DCS_write_1A_1P(0xDA,0xFB);
		TC358768_DCS_write_1A_1P(0xDB,0x03);
		TC358768_DCS_write_1A_1P(0xDC,0x20);
		TC358768_DCS_write_1A_1P(0xDD,0x03);
		TC358768_DCS_write_1A_1P(0xDE,0x54);
		TC358768_DCS_write_1A_1P(0xDF,0x03);
		TC358768_DCS_write_1A_1P(0xE0,0x6D);
		TC358768_DCS_write_1A_1P(0xE1,0x03);
		TC358768_DCS_write_1A_1P(0xE2,0x80);
		TC358768_DCS_write_1A_1P(0xE3,0x03);
		TC358768_DCS_write_1A_1P(0xE4,0x81);
		TC358768_DCS_write_1A_1P(0xE5,0x03);
		TC358768_DCS_write_1A_1P(0xE6,0xC7);
		TC358768_DCS_write_1A_1P(0xE7,0x03);
		TC358768_DCS_write_1A_1P(0xE8,0xF0);
		TC358768_DCS_write_1A_1P(0xE9,0x03);
		TC358768_DCS_write_1A_1P(0xEA,0xF8);
		TC358768_DCS_write_1A_1P(0xEB,0x03);
		TC358768_DCS_write_1A_1P(0xEC,0xFD);
		TC358768_DCS_write_1A_1P(0xED,0x03);
		TC358768_DCS_write_1A_1P(0xEE,0xFE);
		//G(+) MCR cmd                      
		TC358768_DCS_write_1A_1P(0xEF,0x00);
		TC358768_DCS_write_1A_1P(0xF0,0x03);
		TC358768_DCS_write_1A_1P(0xF1,0x00);
		TC358768_DCS_write_1A_1P(0xF2,0x0B);
		TC358768_DCS_write_1A_1P(0xF3,0x00);
		TC358768_DCS_write_1A_1P(0xF4,0x0D);
		TC358768_DCS_write_1A_1P(0xF5,0x00);
		TC358768_DCS_write_1A_1P(0xF6,0x4A);
		TC358768_DCS_write_1A_1P(0xF7,0x00);
		TC358768_DCS_write_1A_1P(0xF8,0x71);
		TC358768_DCS_write_1A_1P(0xF9,0x00);
		TC358768_DCS_write_1A_1P(0xFA,0x8C);
		//page selection cmd start          
		TC358768_DCS_write_1A_1P(0xFF,0x02);
		TC358768_DCS_write_1A_1P(0xFB,0x01);
		//page selection cmd end            
		TC358768_DCS_write_1A_1P(0x00,0x00);
		TC358768_DCS_write_1A_1P(0x01,0xA1);
		TC358768_DCS_write_1A_1P(0x02,0x00);
		TC358768_DCS_write_1A_1P(0x03,0xB6);
		TC358768_DCS_write_1A_1P(0x04,0x00);
		TC358768_DCS_write_1A_1P(0x05,0xC9);
		TC358768_DCS_write_1A_1P(0x06,0x00);
		TC358768_DCS_write_1A_1P(0x07,0xFD);
		TC358768_DCS_write_1A_1P(0x08,0x01);
		TC358768_DCS_write_1A_1P(0x09,0x29);
		TC358768_DCS_write_1A_1P(0x0A,0x01);
		TC358768_DCS_write_1A_1P(0x0B,0x6B);
		TC358768_DCS_write_1A_1P(0x0C,0x01);
		TC358768_DCS_write_1A_1P(0x0D,0x9E);
		TC358768_DCS_write_1A_1P(0x0E,0x01);
		TC358768_DCS_write_1A_1P(0x0F,0xEB);
		TC358768_DCS_write_1A_1P(0x10,0x02);
		TC358768_DCS_write_1A_1P(0x11,0x25);
		TC358768_DCS_write_1A_1P(0x12,0x02);
		TC358768_DCS_write_1A_1P(0x13,0x27);
		TC358768_DCS_write_1A_1P(0x14,0x02);
		TC358768_DCS_write_1A_1P(0x15,0x5C);
		TC358768_DCS_write_1A_1P(0x16,0x02);
		TC358768_DCS_write_1A_1P(0x17,0x95);
		TC358768_DCS_write_1A_1P(0x18,0x02);
		TC358768_DCS_write_1A_1P(0x19,0xBA);
		TC358768_DCS_write_1A_1P(0x1A,0x02);
		TC358768_DCS_write_1A_1P(0x1B,0xEC);
		TC358768_DCS_write_1A_1P(0x1C,0x03);
		TC358768_DCS_write_1A_1P(0x1D,0x0C);
		TC358768_DCS_write_1A_1P(0x1E,0x03);
		TC358768_DCS_write_1A_1P(0x1F,0x34);
		TC358768_DCS_write_1A_1P(0x20,0x03);
		TC358768_DCS_write_1A_1P(0x21,0x3F);
		TC358768_DCS_write_1A_1P(0x22,0x03);
		TC358768_DCS_write_1A_1P(0x23,0x48);
		TC358768_DCS_write_1A_1P(0x24,0x03);
		TC358768_DCS_write_1A_1P(0x25,0x49);
		TC358768_DCS_write_1A_1P(0x26,0x03);
		TC358768_DCS_write_1A_1P(0x27,0x6B);
		TC358768_DCS_write_1A_1P(0x28,0x03);
		TC358768_DCS_write_1A_1P(0x29,0x7E);
		TC358768_DCS_write_1A_1P(0x2A,0x03);
		TC358768_DCS_write_1A_1P(0x2B,0x8F);
		TC358768_DCS_write_1A_1P(0x2D,0x03);
		TC358768_DCS_write_1A_1P(0x2F,0x9E);
		TC358768_DCS_write_1A_1P(0x30,0x03);
		TC358768_DCS_write_1A_1P(0x31,0xA0);
		//G(-) MCR cmd                      
		TC358768_DCS_write_1A_1P(0x32,0x00);
		TC358768_DCS_write_1A_1P(0x33,0x03);
		TC358768_DCS_write_1A_1P(0x34,0x00);
		TC358768_DCS_write_1A_1P(0x35,0x0B);
		TC358768_DCS_write_1A_1P(0x36,0x00);
		TC358768_DCS_write_1A_1P(0x37,0x0D);
		TC358768_DCS_write_1A_1P(0x38,0x00);
		TC358768_DCS_write_1A_1P(0x39,0x4A);
		TC358768_DCS_write_1A_1P(0x3A,0x00);
		TC358768_DCS_write_1A_1P(0x3B,0x71);
		TC358768_DCS_write_1A_1P(0x3D,0x00);
		TC358768_DCS_write_1A_1P(0x3F,0x8C);
		TC358768_DCS_write_1A_1P(0x40,0x00);
		TC358768_DCS_write_1A_1P(0x41,0xA1);
		TC358768_DCS_write_1A_1P(0x42,0x00);
		TC358768_DCS_write_1A_1P(0x43,0xB6);
		TC358768_DCS_write_1A_1P(0x44,0x00);
		TC358768_DCS_write_1A_1P(0x45,0xC9);
		TC358768_DCS_write_1A_1P(0x46,0x00);
		TC358768_DCS_write_1A_1P(0x47,0xFD);
		TC358768_DCS_write_1A_1P(0x48,0x01);
		TC358768_DCS_write_1A_1P(0x49,0x29);
		TC358768_DCS_write_1A_1P(0x4A,0x01);
		TC358768_DCS_write_1A_1P(0x4B,0x6B);
		TC358768_DCS_write_1A_1P(0x4C,0x01);
		TC358768_DCS_write_1A_1P(0x4D,0x9E);
		TC358768_DCS_write_1A_1P(0x4E,0x01);
		TC358768_DCS_write_1A_1P(0x4F,0xEB);
		TC358768_DCS_write_1A_1P(0x50,0x02);
		TC358768_DCS_write_1A_1P(0x51,0x25);
		TC358768_DCS_write_1A_1P(0x52,0x02);
		TC358768_DCS_write_1A_1P(0x53,0x27);
		TC358768_DCS_write_1A_1P(0x54,0x02);
		TC358768_DCS_write_1A_1P(0x55,0x5C);
		TC358768_DCS_write_1A_1P(0x56,0x02);
		TC358768_DCS_write_1A_1P(0x58,0x95);
		TC358768_DCS_write_1A_1P(0x59,0x02);
		TC358768_DCS_write_1A_1P(0x5A,0xBA);
		TC358768_DCS_write_1A_1P(0x5B,0x02);
		TC358768_DCS_write_1A_1P(0x5C,0xEC);
		TC358768_DCS_write_1A_1P(0x5D,0x03);
		TC358768_DCS_write_1A_1P(0x5E,0x0C);
		TC358768_DCS_write_1A_1P(0x5F,0x03);
		TC358768_DCS_write_1A_1P(0x60,0x34);
		TC358768_DCS_write_1A_1P(0x61,0x03);
		TC358768_DCS_write_1A_1P(0x62,0x3F);
		TC358768_DCS_write_1A_1P(0x63,0x03);
		TC358768_DCS_write_1A_1P(0x64,0x48);
		TC358768_DCS_write_1A_1P(0x65,0x03);
		TC358768_DCS_write_1A_1P(0x66,0x49);
		TC358768_DCS_write_1A_1P(0x67,0x03);
		TC358768_DCS_write_1A_1P(0x68,0x6B);
		TC358768_DCS_write_1A_1P(0x69,0x03);
		TC358768_DCS_write_1A_1P(0x6A,0x7E);
		TC358768_DCS_write_1A_1P(0x6B,0x03);
		TC358768_DCS_write_1A_1P(0x6C,0x8F);
		TC358768_DCS_write_1A_1P(0x6D,0x03);
		TC358768_DCS_write_1A_1P(0x6E,0x9E);
		TC358768_DCS_write_1A_1P(0x6F,0x03);
		TC358768_DCS_write_1A_1P(0x70,0xA0);
		//B(+) MCR cmd                      
		TC358768_DCS_write_1A_1P(0x71,0x00);
		TC358768_DCS_write_1A_1P(0x72,0xFB);
		TC358768_DCS_write_1A_1P(0x73,0x00);
		TC358768_DCS_write_1A_1P(0x74,0xFD);
		TC358768_DCS_write_1A_1P(0x75,0x01);
		TC358768_DCS_write_1A_1P(0x76,0x05);
		TC358768_DCS_write_1A_1P(0x77,0x01);
		TC358768_DCS_write_1A_1P(0x78,0x0D);
		TC358768_DCS_write_1A_1P(0x79,0x01);
		TC358768_DCS_write_1A_1P(0x7A,0x17);
		TC358768_DCS_write_1A_1P(0x7B,0x01);
		TC358768_DCS_write_1A_1P(0x7C,0x1F);
		TC358768_DCS_write_1A_1P(0x7D,0x01);
		TC358768_DCS_write_1A_1P(0x7E,0x28);
		TC358768_DCS_write_1A_1P(0x7F,0x01);
		TC358768_DCS_write_1A_1P(0x80,0x32);
		TC358768_DCS_write_1A_1P(0x81,0x01);
		TC358768_DCS_write_1A_1P(0x82,0x38);
		TC358768_DCS_write_1A_1P(0x83,0x01);
		TC358768_DCS_write_1A_1P(0x84,0x53);
		TC358768_DCS_write_1A_1P(0x85,0x01);
		TC358768_DCS_write_1A_1P(0x86,0x72);
		TC358768_DCS_write_1A_1P(0x87,0x01);
		TC358768_DCS_write_1A_1P(0x88,0x9B);
		TC358768_DCS_write_1A_1P(0x89,0x01);
		TC358768_DCS_write_1A_1P(0x8A,0xC3);
		TC358768_DCS_write_1A_1P(0x8B,0x02);
		TC358768_DCS_write_1A_1P(0x8C,0x01);
		TC358768_DCS_write_1A_1P(0x8D,0x02);
		TC358768_DCS_write_1A_1P(0x8E,0x36);
		TC358768_DCS_write_1A_1P(0x8F,0x02);
		TC358768_DCS_write_1A_1P(0x90,0x37);
		TC358768_DCS_write_1A_1P(0x91,0x02);
		TC358768_DCS_write_1A_1P(0x92,0x69);
		TC358768_DCS_write_1A_1P(0x93,0x02);
		TC358768_DCS_write_1A_1P(0x94,0xA1);
		TC358768_DCS_write_1A_1P(0x95,0x02);
		TC358768_DCS_write_1A_1P(0x96,0xC8);
		TC358768_DCS_write_1A_1P(0x97,0x02);
		TC358768_DCS_write_1A_1P(0x98,0xFF);
		TC358768_DCS_write_1A_1P(0x99,0x03);
		TC358768_DCS_write_1A_1P(0x9A,0x26);
		TC358768_DCS_write_1A_1P(0x9B,0x03);
		TC358768_DCS_write_1A_1P(0x9C,0x69);
		TC358768_DCS_write_1A_1P(0x9D,0x03);
		TC358768_DCS_write_1A_1P(0x9E,0x88);
		TC358768_DCS_write_1A_1P(0x9F,0x03);
		TC358768_DCS_write_1A_1P(0xA0,0xF8);
		TC358768_DCS_write_1A_1P(0xA2,0x03);
		TC358768_DCS_write_1A_1P(0xA3,0xF9);
		TC358768_DCS_write_1A_1P(0xA4,0x03);
		TC358768_DCS_write_1A_1P(0xA5,0xFE);
		TC358768_DCS_write_1A_1P(0xA6,0x03);
		TC358768_DCS_write_1A_1P(0xA7,0xFE);
		TC358768_DCS_write_1A_1P(0xA9,0x03);
		TC358768_DCS_write_1A_1P(0xAA,0xFE);
		TC358768_DCS_write_1A_1P(0xAB,0x03);
		TC358768_DCS_write_1A_1P(0xAC,0xFE);
		TC358768_DCS_write_1A_1P(0xAD,0x03);
		TC358768_DCS_write_1A_1P(0xAE,0xFE);
		//B(-) MCR cmd                      
		TC358768_DCS_write_1A_1P(0xAF,0x00);
		TC358768_DCS_write_1A_1P(0xB0,0xFB);
		TC358768_DCS_write_1A_1P(0xB1,0x00);
		TC358768_DCS_write_1A_1P(0xB2,0xFD);
		TC358768_DCS_write_1A_1P(0xB3,0x01);
		TC358768_DCS_write_1A_1P(0xB4,0x05);
		TC358768_DCS_write_1A_1P(0xB5,0x01);
		TC358768_DCS_write_1A_1P(0xB6,0x0D);
		TC358768_DCS_write_1A_1P(0xB7,0x01);
		TC358768_DCS_write_1A_1P(0xB8,0x17);
		TC358768_DCS_write_1A_1P(0xB9,0x01);
		TC358768_DCS_write_1A_1P(0xBA,0x1F);
		TC358768_DCS_write_1A_1P(0xBB,0x01);
		TC358768_DCS_write_1A_1P(0xBC,0x28);
		TC358768_DCS_write_1A_1P(0xBD,0x01);
		TC358768_DCS_write_1A_1P(0xBE,0x32);
		TC358768_DCS_write_1A_1P(0xBF,0x01);
		TC358768_DCS_write_1A_1P(0xC0,0x38);
		TC358768_DCS_write_1A_1P(0xC1,0x01);
		TC358768_DCS_write_1A_1P(0xC2,0x53);
		TC358768_DCS_write_1A_1P(0xC3,0x01);
		TC358768_DCS_write_1A_1P(0xC4,0x72);
		TC358768_DCS_write_1A_1P(0xC5,0x01);
		TC358768_DCS_write_1A_1P(0xC6,0x9B);
		TC358768_DCS_write_1A_1P(0xC7,0x01);
		TC358768_DCS_write_1A_1P(0xC8,0xC3);
		TC358768_DCS_write_1A_1P(0xC9,0x02);
		TC358768_DCS_write_1A_1P(0xCA,0x01);
		TC358768_DCS_write_1A_1P(0xCB,0x02);
		TC358768_DCS_write_1A_1P(0xCC,0x36);
		TC358768_DCS_write_1A_1P(0xCD,0x02);
		TC358768_DCS_write_1A_1P(0xCE,0x37);
		TC358768_DCS_write_1A_1P(0xCF,0x02);
		TC358768_DCS_write_1A_1P(0xD0,0x69);
		TC358768_DCS_write_1A_1P(0xD1,0x02);
		TC358768_DCS_write_1A_1P(0xD2,0xA1);
		TC358768_DCS_write_1A_1P(0xD3,0x02);
		TC358768_DCS_write_1A_1P(0xD4,0xC8);
		TC358768_DCS_write_1A_1P(0xD5,0x02);
		TC358768_DCS_write_1A_1P(0xD6,0xFF);
		TC358768_DCS_write_1A_1P(0xD7,0x03);
		TC358768_DCS_write_1A_1P(0xD8,0x26);
		TC358768_DCS_write_1A_1P(0xD9,0x03);
		TC358768_DCS_write_1A_1P(0xDA,0x69);
		TC358768_DCS_write_1A_1P(0xDB,0x03);
		TC358768_DCS_write_1A_1P(0xDC,0x88);
		TC358768_DCS_write_1A_1P(0xDD,0x03);
		TC358768_DCS_write_1A_1P(0xDE,0xF8);
		TC358768_DCS_write_1A_1P(0xDF,0x03);
		TC358768_DCS_write_1A_1P(0xE0,0xF9);
		TC358768_DCS_write_1A_1P(0xE1,0x03);
		TC358768_DCS_write_1A_1P(0xE2,0xFE);
		TC358768_DCS_write_1A_1P(0xE3,0x03);
		TC358768_DCS_write_1A_1P(0xE4,0xFE);
		TC358768_DCS_write_1A_1P(0xE5,0x03);
		TC358768_DCS_write_1A_1P(0xE6,0xFE);
		TC358768_DCS_write_1A_1P(0xE7,0x03);
		TC358768_DCS_write_1A_1P(0xE8,0xFE);
		TC358768_DCS_write_1A_1P(0xE9,0x03);
		TC358768_DCS_write_1A_1P(0xEA,0xFE);
		
		//
		//
		TC358768_DCS_write_1A_1P(0xFF,0x01);
		TC358768_DCS_write_1A_1P(0xFB,0x01);
		TC358768_DCS_write_1A_1P(0xFF,0x02);
		TC358768_DCS_write_1A_1P(0xFB,0x01);
		TC358768_DCS_write_1A_1P(0xFF,0x04);
		TC358768_DCS_write_1A_1P(0xFB,0x01);
		TC358768_DCS_write_1A_1P(0xFF,0x00);
		
		TC358768_DCS_write_1A_1P(0xD3,0x14);
		TC358768_DCS_write_1A_1P(0xD4,0x14);
		
		TC358768_DCS_write_1A_0P(0x11);
		//Delayus(150000);
		MDELAY(150);
		//
		TC358768_DCS_write_1A_1P(0xFF,0x00);
		//TC358768_DCS_write_1A_1P(0x34,0x00);
		TC358768_DCS_write_1A_1P(0x35,0x00);
		TC358768_DCS_write_1A_0P(0x29);
#else

		TC358768_DCS_write_1A_1P(0xFF,0x01);

		TC358768_DCS_write_1A_1P(0x00,0x01);
		TC358768_DCS_write_1A_1P(0x01,0x55);
		TC358768_DCS_write_1A_1P(0x02,0x40);
		TC358768_DCS_write_1A_1P(0x05,0x40);
		TC358768_DCS_write_1A_1P(0x06,0x19);
		TC358768_DCS_write_1A_1P(0x07,0x1E);
		TC358768_DCS_write_1A_1P(0x08,0x0C);
		TC358768_DCS_write_1A_1P(0x0B,0x87);
		TC358768_DCS_write_1A_1P(0x0C,0x87);
		TC358768_DCS_write_1A_1P(0x0E,0xAE);
		TC358768_DCS_write_1A_1P(0x0F,0xAC);
		TC358768_DCS_write_1A_1P(0x10,0x32);
		TC358768_DCS_write_1A_1P(0x11,0xEA);
		TC358768_DCS_write_1A_1P(0x12,0xEA);
		TC358768_DCS_write_1A_1P(0x13,0x00);
		TC358768_DCS_write_1A_1P(0x14,0x4A);
		TC358768_DCS_write_1A_1P(0x15,0x18);
		TC358768_DCS_write_1A_1P(0x16,0x18);
		TC358768_DCS_write_1A_1P(0x18,0x00);
		TC358768_DCS_write_1A_1P(0x19,0x77);
		TC358768_DCS_write_1A_1P(0x1A,0x55);
		TC358768_DCS_write_1A_1P(0x1B,0x13);
		TC358768_DCS_write_1A_1P(0x1D,0x00);
		TC358768_DCS_write_1A_1P(0x1E,0x13);
		TC358768_DCS_write_1A_1P(0x1F,0x00);
		TC358768_DCS_write_1A_1P(0x23,0x00);
		TC358768_DCS_write_1A_1P(0x24,0x00);
		TC358768_DCS_write_1A_1P(0x25,0x00);
		TC358768_DCS_write_1A_1P(0x26,0x00);
		TC358768_DCS_write_1A_1P(0x27,0x00);
		TC358768_DCS_write_1A_1P(0x28,0x00);	
		TC358768_DCS_write_1A_1P(0x35,0x00);
		TC358768_DCS_write_1A_1P(0x66,0x00);
		TC358768_DCS_write_1A_1P(0x58,0x82);
		TC358768_DCS_write_1A_1P(0x59,0x02);
		TC358768_DCS_write_1A_1P(0x5A,0x02);
		TC358768_DCS_write_1A_1P(0x5B,0x02);
		TC358768_DCS_write_1A_1P(0x5C,0x82);
		TC358768_DCS_write_1A_1P(0x5D,0x82);
		TC358768_DCS_write_1A_1P(0x5E,0x02);
		TC358768_DCS_write_1A_1P(0x5F,0x02);
		TC358768_DCS_write_1A_1P(0x72,0x31);
		TC358768_DCS_write_1A_1P(0xFB,0x01);

		TC358768_DCS_write_1A_1P(0xFF,0x05);
		TC358768_DCS_write_1A_1P(0x00,0x01);
		TC358768_DCS_write_1A_1P(0x01,0x0B);
		TC358768_DCS_write_1A_1P(0x02,0x0C);  
		TC358768_DCS_write_1A_1P(0x03,0x09);
		TC358768_DCS_write_1A_1P(0x04,0x0A);  
		TC358768_DCS_write_1A_1P(0x05,0x00);
		TC358768_DCS_write_1A_1P(0x06,0x0F);
		TC358768_DCS_write_1A_1P(0x07,0x10);
		TC358768_DCS_write_1A_1P(0x08,0x00);
		TC358768_DCS_write_1A_1P(0x09,0x00);
		TC358768_DCS_write_1A_1P(0x0A,0x00);
		TC358768_DCS_write_1A_1P(0x0B,0x00);
		TC358768_DCS_write_1A_1P(0x0C,0x00);
		TC358768_DCS_write_1A_1P(0x0D,0x13);
		TC358768_DCS_write_1A_1P(0x0E,0x15);
		TC358768_DCS_write_1A_1P(0x0F,0x17);
		TC358768_DCS_write_1A_1P(0x10,0x01);
		TC358768_DCS_write_1A_1P(0x11,0x0B);
		TC358768_DCS_write_1A_1P(0x12,0x0C);		
		TC358768_DCS_write_1A_1P(0x13,0x09);
		TC358768_DCS_write_1A_1P(0x14,0x0A);
		TC358768_DCS_write_1A_1P(0x15,0x00);
		TC358768_DCS_write_1A_1P(0x16,0x0F);
		TC358768_DCS_write_1A_1P(0x17,0x10);
		TC358768_DCS_write_1A_1P(0x18,0x00);
		TC358768_DCS_write_1A_1P(0x19,0x00);
		TC358768_DCS_write_1A_1P(0x1A,0x00);
		TC358768_DCS_write_1A_1P(0x1B,0x00);
		TC358768_DCS_write_1A_1P(0x1C,0x00);
		TC358768_DCS_write_1A_1P(0x1D,0x13);
		TC358768_DCS_write_1A_1P(0x1E,0x15);
		TC358768_DCS_write_1A_1P(0x1F,0x17);
		TC358768_DCS_write_1A_1P(0x20,0x00);
		TC358768_DCS_write_1A_1P(0x21,0x03);
		TC358768_DCS_write_1A_1P(0x22,0x01);
		TC358768_DCS_write_1A_1P(0x23,0x40);
		TC358768_DCS_write_1A_1P(0x24,0x40);
		TC358768_DCS_write_1A_1P(0x25,0xED);
		TC358768_DCS_write_1A_1P(0x29,0x58);
		TC358768_DCS_write_1A_1P(0x2A,0x12);
		TC358768_DCS_write_1A_1P(0x2B,0x01);
		TC358768_DCS_write_1A_1P(0x4B,0x06);
		TC358768_DCS_write_1A_1P(0x4C,0x11);
		TC358768_DCS_write_1A_1P(0x4D,0x20);
		TC358768_DCS_write_1A_1P(0x4E,0x02);
		TC358768_DCS_write_1A_1P(0x4F,0x02);
		TC358768_DCS_write_1A_1P(0x50,0x20);
		TC358768_DCS_write_1A_1P(0x51,0x61);
		TC358768_DCS_write_1A_1P(0x52,0x01);
		TC358768_DCS_write_1A_1P(0x53,0x63);
		TC358768_DCS_write_1A_1P(0x54,0x77);
		TC358768_DCS_write_1A_1P(0x55,0xED);
		TC358768_DCS_write_1A_1P(0x5B,0x00);
		TC358768_DCS_write_1A_1P(0x5C,0x00);
		TC358768_DCS_write_1A_1P(0x5D,0x00);
		TC358768_DCS_write_1A_1P(0x5E,0x00);
		TC358768_DCS_write_1A_1P(0x5F,0x15);		
		TC358768_DCS_write_1A_1P(0x60,0x75);
		TC358768_DCS_write_1A_1P(0x61,0x00);		
		TC358768_DCS_write_1A_1P(0x62,0x00);		
		TC358768_DCS_write_1A_1P(0x63,0x00);		
		TC358768_DCS_write_1A_1P(0x64,0x00);
		TC358768_DCS_write_1A_1P(0x65,0x00);
		TC358768_DCS_write_1A_1P(0x66,0x00);
		TC358768_DCS_write_1A_1P(0x67,0x00);
		TC358768_DCS_write_1A_1P(0x68,0x04);
		TC358768_DCS_write_1A_1P(0x69,0x00);
		TC358768_DCS_write_1A_1P(0x6A,0x00);
		TC358768_DCS_write_1A_1P(0x6C,0x40);
		TC358768_DCS_write_1A_1P(0x75,0x01);
		TC358768_DCS_write_1A_1P(0x76,0x01);
		TC358768_DCS_write_1A_1P(0x7A,0x80);
		TC358768_DCS_write_1A_1P(0x7B,0xC5);
		TC358768_DCS_write_1A_1P(0x7C,0xD8);
		TC358768_DCS_write_1A_1P(0x7D,0x60);
		TC358768_DCS_write_1A_1P(0x7F,0x10);
		TC358768_DCS_write_1A_1P(0x80,0x81);
		TC358768_DCS_write_1A_1P(0x83,0x05);
		TC358768_DCS_write_1A_1P(0x8A,0x00);
		TC358768_DCS_write_1A_1P(0x9B,0x0F);
		TC358768_DCS_write_1A_1P(0xFB,0x01);
		TC358768_DCS_write_1A_1P(0xEA,0xFF);
		TC358768_DCS_write_1A_1P(0xEC,0x00);

		TC358768_DCS_write_1A_1P(0xFF,0xEE);
		TC358768_DCS_write_1A_1P(0x46,0x63);
		TC358768_DCS_write_1A_1P(0xFB,0x01);

		TC358768_DCS_write_1A_1P(0xFF,0x00);
		TC358768_DCS_write_1A_1P(0x35,0x00);
		TC358768_DCS_write_1A_1P(0xD3,0x14);
		TC358768_DCS_write_1A_1P(0xD4,0x14);

		TC358768_DCS_write_1A_0P(0x11);
		MDELAY(150);
		TC358768_DCS_write_1A_0P(0x29);

#endif

#if 0//ndef BUILD_LK
		read_reg_v2(0xDA, &buffer[0], 1);
		read_reg_v2(0xDB, &buffer[1], 1);
		read_reg_v2(0xDC, &buffer[2], 1);

		read_reg_v2(0xF4, &buffer[3], 1);

		printk("%s, ID = (0x%02x, 0x%02x, 0x%02x, 0x%02x)\n", __func__, buffer[0], buffer[1], buffer[2], buffer[3]);	
#endif

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

		// enable tearing-free
		params->dbi.te_mode 				= LCM_DBI_TE_MODE_VSYNC_ONLY;
		params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;

        #if (LCM_DSI_CMD_MODE)
		params->dsi.mode   = CMD_MODE;
        #else
		params->dsi.mode   = BURST_VDO_MODE;
        #endif
	
		// DSI
		/* Command mode setting */
		//1 Three lane or Four lane
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

		
		params->dsi.vertical_sync_active				= 2;
		params->dsi.vertical_backporch					= 18;
		params->dsi.vertical_frontporch					= 20;
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 

		params->dsi.horizontal_sync_active				= 4;
		params->dsi.horizontal_backporch				= 118;
		params->dsi.horizontal_frontporch				= 118;
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

		// Bit rate calculation
		//1 Every lane speed
		params->dsi.pll_div1=0;		// div1=0,1,2,3;div1_real=1,2,4,4 ----0: 546Mbps  1:273Mbps
		params->dsi.pll_div2=0;		// div2=0,1,2,3;div1_real=1,2,4,4	
		params->dsi.fbk_div =0x12;    // fref=26MHz, fvco=fref*(fbk_div+1)*2/(div1_real*div2_real)	

}

static void lcm_init(void)
{
	lcm_is_init = true;
	// Enable EN_PWR for NT50198 PMIC
	dsi_lcm_set_gpio_mode(GPIO139, GPIO_MODE_GPIO);
	dsi_lcm_set_gpio_dir(GPIO139, GPIO_DIR_OUT);
	dsi_lcm_set_gpio_out(GPIO139, GPIO_OUT_ONE);

	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(10);
	
	SET_RESET_PIN(1);
	MDELAY(20);      

	init_lcm_registers();
}

static void lcm_suspend(void)
{
	unsigned int data_array[16];
	//unsigned char buffer[2];

#if 0//ndef BUILD_LK
	data_array[0] = 0x00013700;// read id return two byte,version and id
	dsi_set_cmdq(data_array, 1, 1);
	
	read_reg_v2(0xFE, buffer, 1);
	printk("%s, kernel nt35596 horse debug: nt35596 id = 0x%08x\n", __func__, buffer[0]);
#endif

	data_array[0]=0x00280500; // Display Off
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x00100500; // Sleep In
	dsi_set_cmdq(data_array, 1, 1);

	MDELAY(120);
	SET_RESET_PIN(0);

	dsi_lcm_set_gpio_out(GPIO139, GPIO_OUT_ZERO);

	lcm_is_init = false;
}


static void lcm_resume(void)
{
	//unsigned int data_array[16];
	//unsigned char buffer[2];
	if(!lcm_is_init)
		lcm_init();

#if 0//ndef BUILD_LK
	data_array[0] = 0x00013700;// read id return two byte,version and id
	dsi_set_cmdq(data_array, 1, 1);
	
	read_reg_v2(0xFE, buffer, 1);
	printk("%s, kernel nt35596 horse debug: nt35596 id = 0x%08x\n", __func__, buffer[0]);
#endif

	//TC358768_DCS_write_1A_0P(0x11); // Sleep Out
	//MDELAY(150);

	//TC358768_DCS_write_1A_0P(0x29); // Display On

}
         
#if (LCM_DSI_CMD_MODE)
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

	data_array[0]= 0x00290508; //HW bug, so need send one HS packet
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0]= 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);

}
#endif
#if 0
static unsigned int lcm_compare_id(void)
{
	unsigned int id=0;
	unsigned char buffer[2];
	unsigned int array[16];  

	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);
	
	SET_RESET_PIN(1);
	MDELAY(20); 

	array[0] = 0x00023700;// read id return two byte,version and id
	dsi_set_cmdq(array, 1, 1);
	
	read_reg_v2(0xF4, buffer, 2);
	id = buffer[0]; //we only need ID
    #ifdef BUILD_LK
		printf("%s, LK nt35590 debug: nt35590 id = 0x%08x\n", __func__, id);
    #else
		printk("%s, kernel nt35590 horse debug: nt35590 id = 0x%08x\n", __func__, id);
    #endif

    if(id == LCM_ID_NT35590)
    	return 1;
    else
        return 0;


}
#endif

LCM_DRIVER nt35596_fhd_dsi_vdo_truly_lcm_drv = 
{
    .name			= "nt35596_fhd_dsi_vdo_truly",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	//.compare_id     = lcm_compare_id,
#if (LCM_DSI_CMD_MODE)
    .update         = lcm_update,
#endif
    };
