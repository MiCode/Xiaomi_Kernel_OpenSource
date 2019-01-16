#ifdef BUILD_LK
#include <platform/mt_pmic.h>
#else
#include <linux/string.h>
#if defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#include <mach/mt_gpio.h>
#endif
#endif


#include <string.h>
#include <stdio.h>
#include "lcm_drv.h"

#define FRAME_WIDTH  (240)
#define FRAME_HEIGHT (320)
#define LCM_ID       (0x69)

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))

#if defined(BUILD_LK)
#define LCM_PRINT printf
#elif defined(BUILD_UBOOT)
#define LCM_PRINT printf
#else
#define LCM_PRINT printk
#endif

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

static __inline void send_ctrl_cmd(unsigned int cmd)
{
	lcm_util.send_cmd(cmd);
}

static __inline void send_data_cmd(unsigned int data)
{
	lcm_util.send_data(data&0xff);
}

static __inline unsigned int read_data_cmd(void)
{
    return 0xFF&lcm_util.read_data();
}

static __inline void set_lcm_register(unsigned int regIndex,
		unsigned int regData)
{
	send_ctrl_cmd(regIndex);
	send_data_cmd(regData);
}

#if 0 // we don't use it. 
static void sw_clear_panel(unsigned int color)
{
	unsigned short x0, y0, x1, y1, x, y;
	unsigned short h_X_start,l_X_start,h_X_end,l_X_end,h_Y_start,l_Y_start,h_Y_end,l_Y_end;

	x0 = (unsigned short)0;
	y0 = (unsigned short)0;
	x1 = (unsigned short)FRAME_WIDTH-1;
	y1 = (unsigned short)FRAME_HEIGHT-1;

	h_X_start=((x0&0xFF00)>>8);
	l_X_start=(x0&0x00FF);
	h_X_end=((x1&0xFF00)>>8);
	l_X_end=(x1&0x00FF);

	h_Y_start=((y0&0xFF00)>>8);
	l_Y_start=(y0&0x00FF);
	h_Y_end=((y1&0xFF00)>>8);
	l_Y_end=(y1&0x00FF);

	send_ctrl_cmd(0x2A);
	send_data_cmd(h_X_start);
	send_data_cmd(l_X_start);
	send_data_cmd(h_X_end);
	send_data_cmd(l_X_end);

	send_ctrl_cmd(0x2B);
	send_data_cmd(h_Y_start);
	send_data_cmd(l_Y_start);
	send_data_cmd(h_Y_end);
	send_data_cmd(l_Y_end);

	send_ctrl_cmd(0x29);

	send_ctrl_cmd(0x2C);
    for (y = y0; y <= y1; ++ y) {
        for (x = x0; x <= x1; ++ x) {
            lcm_util.send_data(color);
        }
    }
}
#endif

static void init_lcm_registers(void)
{

#if 1  
     send_ctrl_cmd(0xCF);	// EXTC Option
    send_data_cmd(0x00);
    send_data_cmd(0x21);
     send_data_cmd(0x20);

    send_ctrl_cmd(0xF2); // 3-Gamma Function Off
    send_data_cmd(0x02);

     send_ctrl_cmd(0xB4); // Inversion Control -> 2Dot inversion
     send_data_cmd(0x02);

     send_ctrl_cmd(0xC0); // Powr control 1
    send_data_cmd(0x15);
    send_data_cmd(0x15);

     send_ctrl_cmd(0xC1); // Power control 2
    send_data_cmd(0x05);

     send_ctrl_cmd(0xC2);	// Powr control 3
     send_data_cmd(0x32);

     send_ctrl_cmd(0xC5);	// Vcom control 1
     send_data_cmd(0xFC);

    send_ctrl_cmd(0xCB);   // V-core Setting
    send_data_cmd(0x31);
    send_data_cmd(0x24);
    send_data_cmd(0x00);
    send_data_cmd(0x34);

     send_ctrl_cmd(0xF6);	// Interface control
     send_data_cmd(0x41);
    send_data_cmd(0x00);
     send_data_cmd(0x00);

     send_ctrl_cmd(0xB7);	// Entry Mode Set
     send_data_cmd(0x06);

     send_ctrl_cmd(0xB1);	// Frame Rate Control
     send_data_cmd(0x00);
    send_data_cmd(0x1B);

     send_ctrl_cmd(0x36);	// Memory Access Control
     send_data_cmd(0x08); // seosc 08 -> C8

     send_ctrl_cmd(0xB5);	// Blanking Porch control
     send_data_cmd(0x02);
     send_data_cmd(0x02);
     send_data_cmd(0x0A);
     send_data_cmd(0x14);

     send_ctrl_cmd(0xB6);	// Display Function control
     send_data_cmd(0x0A);
    send_data_cmd(0x82);
     send_data_cmd(0x27);
    send_data_cmd(0x00);

     send_ctrl_cmd(0x3A);	// Pixel Format->DBI(5=16bit)
    send_data_cmd(0x05);

     send_ctrl_cmd(0x35);	// Tearing Effect Line On
     send_data_cmd(0x00);

     send_ctrl_cmd(0x44);	// Tearing Effect Control Parameter
     send_data_cmd(0x00);
     send_data_cmd(0xEF);

     send_ctrl_cmd(0xE0);	// Positive Gamma Correction
    send_data_cmd(0x00);
    send_data_cmd(0x06);
     send_data_cmd(0x07);
    send_data_cmd(0x03);
    send_data_cmd(0x0A);
     send_data_cmd(0x0A);
    send_data_cmd(0x36);
    send_data_cmd(0x59);
    send_data_cmd(0x4B);
    send_data_cmd(0x0C);
    send_data_cmd(0x18);
    send_data_cmd(0x0F);
    send_data_cmd(0x22);
    send_data_cmd(0x1F);
     send_data_cmd(0x0F);

     send_ctrl_cmd(0xE1);	// Negative Gamma Correction
    send_data_cmd(0x06);
    send_data_cmd(0x23);
    send_data_cmd(0x24);
    send_data_cmd(0x01);
    send_data_cmd(0x0F);
     send_data_cmd(0x01);
    send_data_cmd(0x31);
    send_data_cmd(0x23);
    send_data_cmd(0x40);
    send_data_cmd(0x07);
     send_data_cmd(0x0F);
     send_data_cmd(0x0F);
    send_data_cmd(0x30);
    send_data_cmd(0x31);
    send_data_cmd(0x0E);

     send_ctrl_cmd(0x2A);	// Column address
     send_data_cmd(0x00);
     send_data_cmd(0x00);
     send_data_cmd(0x00);
     send_data_cmd(0xEF);

     send_ctrl_cmd(0x2B);	// Page address
     send_data_cmd(0x00);
     send_data_cmd(0x00);
     send_data_cmd(0x01);
     send_data_cmd(0x3F);

     send_ctrl_cmd(0xE8);
     send_data_cmd(0x84);
     send_data_cmd(0x1A);
     send_data_cmd(0x68);

    send_ctrl_cmd(0x11);  // Exit Sleep
     MDELAY(120);

    send_ctrl_cmd(0x2C);

    MDELAY(80);
    send_ctrl_cmd(0x29);  // LCD on
#else // HDK board
     send_ctrl_cmd(0xCF);	// EXTC Option
     send_data_cmd(0x20);
     send_data_cmd(0x21);
     send_data_cmd(0x20);

     send_ctrl_cmd(0xF2); // 3-Gamma Function Off
     send_data_cmd(0x02);

     send_ctrl_cmd(0xB4); // Inversion Control -> 2Dot inversion
     send_data_cmd(0x02);

     send_ctrl_cmd(0xC0); // Powr control 1
     send_data_cmd(0x15);
     send_data_cmd(0x15);

     send_ctrl_cmd(0xC1); // Power control 2
     send_data_cmd(0x05);

     send_ctrl_cmd(0xC2);	// Powr control 3
     send_data_cmd(0x32);

     send_ctrl_cmd(0xC5);	// Vcom control 1
     send_data_cmd(0xFC);

     send_ctrl_cmd(0xCB);	// V-core Setting
     send_data_cmd(0x31);
     send_data_cmd(0x24);
     send_data_cmd(0x00);
     send_data_cmd(0x34);

     send_ctrl_cmd(0xF6);	// Interface control
     send_data_cmd(0x41);
     send_data_cmd(0x00);
     send_data_cmd(0x00);

     send_ctrl_cmd(0xB7);	// Entry Mode Set
     send_data_cmd(0x06);

     send_ctrl_cmd(0xB1);	// Frame Rate Control
     send_data_cmd(0x00);
     send_data_cmd(0x1B);

     send_ctrl_cmd(0x36);	// Memory Access Control
     send_data_cmd(0x08); // seosc 08 -> C8

     send_ctrl_cmd(0xB5);	// Blanking Porch control
     send_data_cmd(0x02);
     send_data_cmd(0x02);
     send_data_cmd(0x0A);
     send_data_cmd(0x14);

     send_ctrl_cmd(0xB6);	// Display Function control
     send_data_cmd(0x02);
     send_data_cmd(0x82);
     send_data_cmd(0x27);
     send_data_cmd(0x00);

     send_ctrl_cmd(0x3A);	// Pixel Format->DBI(5=16bit)
     send_data_cmd(0x05);

     send_ctrl_cmd(0x51);//write display brightness
     send_data_cmd(0xff);//set brightness 0x00-0xff
     MDELAY(50);

     send_ctrl_cmd(0x53);//write ctrl display
     send_data_cmd(0x24);
     MDELAY(50);

     send_ctrl_cmd(0x55);
     send_data_cmd(0x02);//still picture
     MDELAY(50);

     send_ctrl_cmd(0x5e);//write CABC minumum brightness
     send_data_cmd(0x70);//
     MDELAY(50);

     send_ctrl_cmd(0x35);	// Tearing Effect Line On
     send_data_cmd(0x00);

     send_ctrl_cmd(0x44);	// Tearing Effect Control Parameter
     send_data_cmd(0x00);
     send_data_cmd(0xEF);

     send_ctrl_cmd(0xE0);	// Positive Gamma Correction
     send_data_cmd(0x00);
     send_data_cmd(0x06);
     send_data_cmd(0x07);
     send_data_cmd(0x03);
     send_data_cmd(0x0A);
     send_data_cmd(0x0A);
     send_data_cmd(0x41);
     send_data_cmd(0x59);
     send_data_cmd(0x4D);
     send_data_cmd(0x0C);
     send_data_cmd(0x18);
     send_data_cmd(0x0F);
     send_data_cmd(0x22);
     send_data_cmd(0x1D);
     send_data_cmd(0x0F);

     send_ctrl_cmd(0xE1);	// Negative Gamma Correction
     send_data_cmd(0x06);
     send_data_cmd(0x23);
     send_data_cmd(0x24);
     send_data_cmd(0x01);
     send_data_cmd(0x0F);
     send_data_cmd(0x01);
     send_data_cmd(0x36);
     send_data_cmd(0x23);
     send_data_cmd(0x41);
     send_data_cmd(0x07);
     send_data_cmd(0x0F);
     send_data_cmd(0x0F);
     send_data_cmd(0x30);
     send_data_cmd(0x27);
     send_data_cmd(0x0E);

     send_ctrl_cmd(0x2A);	// Column address
     send_data_cmd(0x00);
     send_data_cmd(0x00);
     send_data_cmd(0x00);
     send_data_cmd(0xEF);

     send_ctrl_cmd(0x2B);	// Page address
     send_data_cmd(0x00);
     send_data_cmd(0x00);
     send_data_cmd(0x01);
     send_data_cmd(0x3F);

     send_ctrl_cmd(0xE8);
     send_data_cmd(0x84);
     send_data_cmd(0x1A);
     send_data_cmd(0x68);

     send_ctrl_cmd(0x11);
     MDELAY(120);

     send_ctrl_cmd(0X29);
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

	params->type   = LCM_TYPE_DBI;
	params->ctrl   = LCM_CTRL_PARALLEL_DBI;
	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->io_select_mode = 1;

	params->dbi.port                    = 0;
	params->dbi.clock_freq              = LCM_DBI_CLOCK_FREQ_52M;
	params->dbi.data_width              = LCM_DBI_DATA_WIDTH_16BITS;
	params->dbi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dbi.data_format.trans_seq   = LCM_DBI_TRANS_SEQ_MSB_FIRST;
	params->dbi.data_format.padding     = LCM_DBI_PADDING_ON_MSB;
	params->dbi.data_format.format      = LCM_DBI_FORMAT_RGB565;
	params->dbi.data_format.width       = LCM_DBI_DATA_WIDTH_16BITS;
	params->dbi.cpu_write_bits          = LCM_DBI_CPU_WRITE_16_BITS;
	params->dbi.io_driving_current      = 0;

	params->dbi.parallel.write_setup    = 2;
	params->dbi.parallel.write_hold     = 2;
	params->dbi.parallel.write_wait     = 4;
	params->dbi.parallel.read_setup     = 2;
	params->dbi.parallel.read_latency   = 31;
	params->dbi.parallel.wait_period    = 9;

    // enable tearing-free
    params->dbi.te_mode                 = LCM_DBI_TE_MODE_VSYNC_ONLY;
    params->dbi.te_edge_polarity        = LCM_POLARITY_RISING;

}



static void lcm_init(void)
{
        	  upmu_set_rg_vgp1_vosel(3);  // set 1.8v for VGP1
         	 upmu_set_rg_vgp1_en(1);      //  VGP1 power ON
            MDELAY(1);
            upmu_set_rg_vcam_af_vosel(5);  // set 2.8V  for VCAM_AF
            upmu_set_rg_vcam_af_en(1);      // VCAM_AF power ON
            MDELAY(1);

    SET_RESET_PIN(1);
	MDELAY(1);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(120);
	init_lcm_registers();
	LCM_PRINT("[LCD] lcm_init \n");

}


static void lcm_suspend(void)
{
#if 1 
	send_ctrl_cmd(0x10);
	MDELAY(120);
        upmu_set_rg_vcam_af_en(0);  // VCAM_AF power OFF
        upmu_set_rg_vgp1_en(0);      //  VGP1 power OFF
    #else
    //sw_clear_panel(0);
	send_ctrl_cmd(0x10);
	MDELAY(5);
    #endif 
	LCM_PRINT("[LCD] lcm_suspend \n");

}


static void lcm_resume(void)
{
	//send_ctrl_cmd(0x11);


	lcm_init();
	MDELAY(120);

	LCM_PRINT("[LCD] lcm_resume \n");
}

static void lcm_update(unsigned int x, unsigned int y,
		unsigned int width, unsigned int height)
{
	unsigned short x0, y0, x1, y1;
	unsigned short h_X_start,l_X_start,h_X_end,l_X_end,h_Y_start,l_Y_start,h_Y_end,l_Y_end;

	x0 = (unsigned short)x;
	y0 = (unsigned short)y;
	x1 = (unsigned short)x+width-1;
	y1 = (unsigned short)y+height-1;


    h_X_start=((x0&0xFF00)>>8);
    l_X_start=(x0&0x00FF);
    h_X_end=((x1&0xFF00)>>8);
    l_X_end=(x1&0x00FF);

    h_Y_start=((y0&0xFF00)>>8);
    l_Y_start=(y0&0x00FF);
    h_Y_end=((y1&0xFF00)>>8);
    l_Y_end=(y1&0x00FF);

	send_ctrl_cmd(0x2A);
	send_data_cmd(h_X_start);
	send_data_cmd(l_X_start);
	send_data_cmd(h_X_end);
	send_data_cmd(l_X_end);

	send_ctrl_cmd(0x2B);
	send_data_cmd(h_Y_start);
	send_data_cmd(l_Y_start);
	send_data_cmd(h_Y_end);
	send_data_cmd(l_Y_end);

	send_ctrl_cmd(0x29);

	send_ctrl_cmd(0x2C);
}

static void lcm_setbacklight(unsigned int level)
{
	if(level > 255) level = 255;
#if 0
	send_ctrl_cmd(0x51);
	send_data_cmd(level);
#else
    send_ctrl_cmd(0xBE);
    send_data_cmd(0x0F);
#endif
}
static unsigned int lcm_compare_id(void)
{
#if 0
    send_ctrl_cmd(0xB9);  // SET password
	send_data_cmd(0xFF);
	send_data_cmd(0x83);
	send_data_cmd(0x69);
    send_ctrl_cmd(0xC3);
	send_data_cmd(0xFF);

	send_ctrl_cmd(0xF4);
	read_data_cmd();
    return (LCM_ID == read_data_cmd())?1:0;
#else
    return 1;
#endif
}

static void lcm_set_pwm(unsigned int divider)
{
#if 0
 send_ctrl_cmd(0xBE);
 send_data_cmd(0xFF);

 send_ctrl_cmd(0xBF);
 send_data_cmd(0x07);
#endif
}


LCM_DRIVER ili9340d_dbi_qvga_drv =
{
    .name			= " ili9340d_dbi_qvga",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.update         = lcm_update,
	.set_backlight	= lcm_setbacklight,
	.set_pwm        = lcm_set_pwm,
	.compare_id     = lcm_compare_id,
};

// ---------------------------------------------------------------------------
//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------
/*
LCM_DRIVER nt35510_dsi_cmd_6572_drv = {
    .name = "nt35510_dsi_cmd_6572",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params = lcm_get_params,
    .init = lcm_init,
    .suspend = lcm_suspend,
    .resume = lcm_resume,
    //.set_backlight = lcm_setbacklight,
    //.set_pwm        = lcm_setpwm,
    //.get_pwm        = lcm_getpwm,
    //.compare_id = lcm_compare_id,
    .update = lcm_update
};
*/
