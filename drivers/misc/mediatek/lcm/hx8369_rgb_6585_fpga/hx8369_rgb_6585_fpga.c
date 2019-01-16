#ifdef BUILD_LK
#else
    #include <linux/string.h>
#endif
#include "lcm_drv.h"


// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  (480)
#define FRAME_HEIGHT (800)


// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))

/*
#define LSA0_GPIO_PIN (GPIO_DISP_LSA0_PIN)
#define LSCE_GPIO_PIN (GPIO_DISP_LSCE_PIN)
#define LSCK_GPIO_PIN (GPIO_DISP_LSCK_PIN)
#define LSDA_GPIO_PIN (GPIO_DISP_LSDA_PIN)
*/
/*
#define LSA0_GPIO_PIN (GPIO103)
#define LSCE_GPIO_PIN (GPIO105)
#define LSCK_GPIO_PIN (GPIO102)
#define LSDA_GPIO_PIN (GPIO104)
*/
// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------
/*static void config_gpio(void)
{
    lcm_util.set_gpio_mode(LSA0_GPIO_PIN, GPIO_DISP_LSA0_PIN_M_LSA);
    lcm_util.set_gpio_mode(LSCE_GPIO_PIN, GPIO_DISP_LSCE_PIN_M_LSCE0B);
    lcm_util.set_gpio_mode(LSCK_GPIO_PIN, GPIO_DISP_LSCK_PIN_M_LSCK);
    lcm_util.set_gpio_mode(LSDA_GPIO_PIN, GPIO_DISP_LSDA_PIN_M_SDA);

    lcm_util.set_gpio_dir(LSA0_GPIO_PIN, GPIO_DIR_OUT);
    lcm_util.set_gpio_dir(LSCE_GPIO_PIN, GPIO_DIR_OUT);
    lcm_util.set_gpio_dir(LSCK_GPIO_PIN, GPIO_DIR_OUT);
    lcm_util.set_gpio_dir(LSDA_GPIO_PIN, GPIO_DIR_OUT);

	lcm_util.set_gpio_pull_enable(LSA0_GPIO_PIN, GPIO_PULL_DISABLE);
	lcm_util.set_gpio_pull_enable(LSCE_GPIO_PIN, GPIO_PULL_DISABLE);
	lcm_util.set_gpio_pull_enable(LSCK_GPIO_PIN, GPIO_PULL_DISABLE);
	lcm_util.set_gpio_pull_enable(LSDA_GPIO_PIN, GPIO_PULL_DISABLE);
}

static void config_gpio(void)
{
    lcm_util.set_gpio_mode(LSA0_GPIO_PIN, GPIO_MODE_01);
    lcm_util.set_gpio_mode(LSCE_GPIO_PIN, GPIO_MODE_01);
    lcm_util.set_gpio_mode(LSCK_GPIO_PIN, GPIO_MODE_01);
    lcm_util.set_gpio_mode(LSDA_GPIO_PIN, GPIO_MODE_01);

    lcm_util.set_gpio_dir(LSA0_GPIO_PIN, GPIO_DIR_OUT);
    lcm_util.set_gpio_dir(LSCE_GPIO_PIN, GPIO_DIR_OUT);
    lcm_util.set_gpio_dir(LSCK_GPIO_PIN, GPIO_DIR_OUT);
    lcm_util.set_gpio_dir(LSDA_GPIO_PIN, GPIO_DIR_OUT);

    lcm_util.set_gpio_pull_enable(LSA0_GPIO_PIN, GPIO_PULL_DISABLE);
    lcm_util.set_gpio_pull_enable(LSCE_GPIO_PIN, GPIO_PULL_DISABLE);
    lcm_util.set_gpio_pull_enable(LSCK_GPIO_PIN, GPIO_PULL_DISABLE);
    lcm_util.set_gpio_pull_enable(LSDA_GPIO_PIN, GPIO_PULL_DISABLE);
}*/
// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

static __inline void send_ctrl_cmd(unsigned int cmd)
{
	lcm_util.send_cmd(cmd);
}

static __inline void send_data_cmd(unsigned int data)
{
	lcm_util.send_data(data);
}


static __inline void set_lcm_register(unsigned int regIndex,
                                      unsigned int regData)
{
    send_ctrl_cmd(regIndex);
    send_data_cmd(regData);
}

static void init_lcm_registers(void)
{
	send_ctrl_cmd(0xB9);  // SET password
	send_data_cmd(0xFF);  
	send_data_cmd(0x83);  
	send_data_cmd(0x69);

	send_ctrl_cmd(0xB1);  //Set Power
	send_data_cmd(0x85);
	send_data_cmd(0x00);
	send_data_cmd(0x34);
	send_data_cmd(0x07);
	send_data_cmd(0x00);
	send_data_cmd(0x0F);
	send_data_cmd(0x0F);
	send_data_cmd(0x2A);
	send_data_cmd(0x32);
	send_data_cmd(0x3F);
	send_data_cmd(0x3F);
	send_data_cmd(0x01);
	send_data_cmd(0x3A);
	send_data_cmd(0x01);
	send_data_cmd(0xE6);
	send_data_cmd(0xE6);
	send_data_cmd(0xE6);
	send_data_cmd(0xE6);
	send_data_cmd(0xE6);

	send_ctrl_cmd(0xB2);  // SET Display  480x800
	send_data_cmd(0x00);  
	send_data_cmd(0x28);  
	send_data_cmd(0x05);  
	send_data_cmd(0x05);  
	send_data_cmd(0x70);  
	send_data_cmd(0x00);  
	send_data_cmd(0xFF);  
	send_data_cmd(0x00);  
	send_data_cmd(0x00);  
	send_data_cmd(0x00);  
	send_data_cmd(0x00);  
	send_data_cmd(0x03);  
	send_data_cmd(0x03);  
	send_data_cmd(0x00);  
	send_data_cmd(0x01);  

	send_ctrl_cmd(0xB4);  // SET Display  column inversion
	send_data_cmd(0x00);  
	send_data_cmd(0x18);  
	send_data_cmd(0x80);  
	send_data_cmd(0x06);  
	send_data_cmd(0x02);  

	send_ctrl_cmd(0xB6);  // SET VCOM
	send_data_cmd(0x42);  
	send_data_cmd(0x42);  

	send_ctrl_cmd(0xD5);
	send_data_cmd(0x00);
	send_data_cmd(0x04);
	send_data_cmd(0x03);
	send_data_cmd(0x00);
	send_data_cmd(0x01);
	send_data_cmd(0x05);
	send_data_cmd(0x28);
	send_data_cmd(0x70);
	send_data_cmd(0x01);
	send_data_cmd(0x03);
	send_data_cmd(0x00);
	send_data_cmd(0x00);
	send_data_cmd(0x40);
	send_data_cmd(0x06);
	send_data_cmd(0x51);
	send_data_cmd(0x07);
	send_data_cmd(0x00);
	send_data_cmd(0x00);
	send_data_cmd(0x41);
	send_data_cmd(0x06);
	send_data_cmd(0x50);
	send_data_cmd(0x07);
	send_data_cmd(0x07);
	send_data_cmd(0x0F);
	send_data_cmd(0x04);
	send_data_cmd(0x00);

	send_ctrl_cmd(0xE0); // Set Gamma
	send_data_cmd(0x00);  
	send_data_cmd(0x13);  
	send_data_cmd(0x19);  
	send_data_cmd(0x38);  
	send_data_cmd(0x3D);  
	send_data_cmd(0x3F);  
	send_data_cmd(0x28);  
	send_data_cmd(0x46);  
	send_data_cmd(0x07);  
	send_data_cmd(0x0D);  
	send_data_cmd(0x0E);  
	send_data_cmd(0x12);  
	send_data_cmd(0x15);  
	send_data_cmd(0x12);  
	send_data_cmd(0x14);  
	send_data_cmd(0x0F);  
	send_data_cmd(0x17);  
	send_data_cmd(0x00);  
	send_data_cmd(0x13);  
	send_data_cmd(0x19);  
	send_data_cmd(0x38);  
	send_data_cmd(0x3D);  
	send_data_cmd(0x3F);  
	send_data_cmd(0x28);  
	send_data_cmd(0x46);  
	send_data_cmd(0x07);  
	send_data_cmd(0x0D);  
	send_data_cmd(0x0E);  
	send_data_cmd(0x12);  
	send_data_cmd(0x15);  
	send_data_cmd(0x12);  
	send_data_cmd(0x14);  
	send_data_cmd(0x0F);  
	send_data_cmd(0x17);  

	send_ctrl_cmd(0xC1); // Set DGC
	send_data_cmd(0x01);  
	send_data_cmd(0x04);  
	send_data_cmd(0x13);  
	send_data_cmd(0x1A);  
	send_data_cmd(0x20);  
	send_data_cmd(0x27);  
	send_data_cmd(0x2C);  
	send_data_cmd(0x32);  
	send_data_cmd(0x36);  
	send_data_cmd(0x3F);  
	send_data_cmd(0x47);  
	send_data_cmd(0x50);  
	send_data_cmd(0x59);  
	send_data_cmd(0x60);  
	send_data_cmd(0x68);  
	send_data_cmd(0x71);  
	send_data_cmd(0x7B);  
	send_data_cmd(0x82);  
	send_data_cmd(0x89);  
	send_data_cmd(0x91);  
	send_data_cmd(0x98);  
	send_data_cmd(0xA0);  
	send_data_cmd(0xA8);  
	send_data_cmd(0xB0);  
	send_data_cmd(0xB8);  
	send_data_cmd(0xC1);  
	send_data_cmd(0xC9);  
	send_data_cmd(0xD0);  
	send_data_cmd(0xD7);  
	send_data_cmd(0xE0);  
	send_data_cmd(0xE7);  
	send_data_cmd(0xEF);  
	send_data_cmd(0xF7);  
	send_data_cmd(0xFE);  
	send_data_cmd(0xCF);  
	send_data_cmd(0x52);  
	send_data_cmd(0x34);  
	send_data_cmd(0xF8);  
	send_data_cmd(0x51);  
	send_data_cmd(0xF5);  
	send_data_cmd(0x9D);  
	send_data_cmd(0x75);  
	send_data_cmd(0x00);  
	send_data_cmd(0x04);  
	send_data_cmd(0x13);  
	send_data_cmd(0x1A);  
	send_data_cmd(0x20);  
	send_data_cmd(0x27);  
	send_data_cmd(0x2C);  
	send_data_cmd(0x32);  
	send_data_cmd(0x36);  
	send_data_cmd(0x3F);  
	send_data_cmd(0x47);  
	send_data_cmd(0x50);  
	send_data_cmd(0x59);  
	send_data_cmd(0x60);  
	send_data_cmd(0x68);  
	send_data_cmd(0x71);  
	send_data_cmd(0x7B);  
	send_data_cmd(0x82);  
	send_data_cmd(0x89);  
	send_data_cmd(0x91);  
	send_data_cmd(0x98);  
	send_data_cmd(0xA0);  
	send_data_cmd(0xA8);  
	send_data_cmd(0xB0);  
	send_data_cmd(0xB8);  
	send_data_cmd(0xC1); 
	send_data_cmd(0xC9);  
	send_data_cmd(0xD0);  
	send_data_cmd(0xD7);  
	send_data_cmd(0xE0);  
	send_data_cmd(0xE7);  
	send_data_cmd(0xEF);  
	send_data_cmd(0xF7);  
	send_data_cmd(0xFE);  
	send_data_cmd(0xCF);  
	send_data_cmd(0x52);  
	send_data_cmd(0x34);  
	send_data_cmd(0xF8);  
	send_data_cmd(0x51);  
	send_data_cmd(0xF5);  
	send_data_cmd(0x9D);  
	send_data_cmd(0x75);  
	send_data_cmd(0x00);  
	send_data_cmd(0x04);  
	send_data_cmd(0x13);  
	send_data_cmd(0x1A);  
	send_data_cmd(0x20);  
	send_data_cmd(0x27);  
	send_data_cmd(0x2C);  
	send_data_cmd(0x32);  
	send_data_cmd(0x36);  
	send_data_cmd(0x3F);  
	send_data_cmd(0x47);  
	send_data_cmd(0x50);  
	send_data_cmd(0x59);  
	send_data_cmd(0x60);  
	send_data_cmd(0x68);  
	send_data_cmd(0x71);  
	send_data_cmd(0x7B);  
	send_data_cmd(0x82); 
	send_data_cmd(0x89);  
	send_data_cmd(0x91);  
	send_data_cmd(0x98);  
	send_data_cmd(0xA0);  
	send_data_cmd(0xA8);  
	send_data_cmd(0xB0);  
	send_data_cmd(0xB8);  
	send_data_cmd(0xC1);  
	send_data_cmd(0xC9);  
	send_data_cmd(0xD0);  
	send_data_cmd(0xD7);  
	send_data_cmd(0xE0);  
	send_data_cmd(0xE7);  
	send_data_cmd(0xEF);  
	send_data_cmd(0xF7);  
	send_data_cmd(0xFE);  
	send_data_cmd(0xCF);  
	send_data_cmd(0x52);  
	send_data_cmd(0x34);  
	send_data_cmd(0xF8);  
	send_data_cmd(0x51);  
	send_data_cmd(0xF5);  
	send_data_cmd(0x9D);  
	send_data_cmd(0x75);  
	send_data_cmd(0x00);

	send_ctrl_cmd(0x3A);  // set Interface Pixel Format
	send_data_cmd(0x77);   // 0x07=24 Bit/Pixel; 0x06=18 Bit/Pixel; 0x05=16 Bit/Pixel

	send_ctrl_cmd(0x11); 	
	MDELAY(120);   
	send_ctrl_cmd(0x29); 	

	send_ctrl_cmd(0x2C);
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

    /* serial host interface configurations */
    
    params->dbi.port                    = 0;
    params->dbi.data_width              = LCM_DBI_DATA_WIDTH_8BITS;
    params->dbi.data_format.color_order = LCM_COLOR_ORDER_RGB;
    params->dbi.data_format.trans_seq   = LCM_DBI_TRANS_SEQ_MSB_FIRST;
    params->dbi.data_format.padding     = LCM_DBI_PADDING_ON_LSB;
    params->dbi.data_format.format      = LCM_DBI_FORMAT_RGB565;
    params->dbi.data_format.width       = LCM_DBI_DATA_WIDTH_8BITS;
    params->dbi.cpu_write_bits          = LCM_DBI_CPU_WRITE_8_BITS;
    params->dbi.io_driving_current      = LCM_DRIVING_CURRENT_6575_4MA;

    /* RGB interface configurations */
    
    params->dpi.mipi_pll_clk_div1 = 0x1f;
    params->dpi.mipi_pll_clk_div2 = 5;
    params->dpi.dpi_clk_div       = 0x04;
    params->dpi.dpi_clk_duty      = 0x1;

    params->dpi.clk_pol           = LCM_POLARITY_FALLING;
    params->dpi.de_pol            = LCM_POLARITY_RISING;
    params->dpi.vsync_pol         = LCM_POLARITY_RISING;
    params->dpi.hsync_pol         = LCM_POLARITY_RISING;

    params->dpi.hsync_pulse_width = 0x1A;
    params->dpi.hsync_back_porch  = 0x1A;
    params->dpi.hsync_front_porch = 0x1A;
    params->dpi.vsync_pulse_width = 0xA;
    params->dpi.vsync_back_porch  = 0xA;
    params->dpi.vsync_front_porch = 0xA;
    
    params->dpi.format            = LCM_DPI_FORMAT_RGB888;
    params->dpi.rgb_order         = LCM_COLOR_ORDER_RGB;

	params->dbi.serial.css = 0xf;
	params->dbi.serial.csh = 0xf;	
	params->dbi.serial.rd_1st = 0xf;
	params->dbi.serial.rd_2nd = 0xf;
	params->dbi.serial.wr_1st = 0xf;
	params->dbi.serial.wr_2nd = 0xf;
	params->dbi.serial.sif_3wire = 0;
	params->dbi.serial.sif_sdi = 0;

	params->dbi.serial.sif_1st_pol = 0;
	params->dbi.serial.sif_sck_def = 1;
	params->dbi.serial.sif_div2 = 1;
	params->dbi.serial.sif_hw_cs = 1;

//    params->dpi.intermediat_buffer_num = 2;

    params->dpi.io_driving_current = LCM_DRIVING_CURRENT_6575_4MA;
	params->dpi.i2x_en = 0;
	params->dpi.i2x_edge = 0;
}


static void lcm_init(void)
{
    SET_RESET_PIN(0);
    MDELAY(25);
    SET_RESET_PIN(1);
    MDELAY(50);
//    config_gpio();
    init_lcm_registers();
}


static void lcm_suspend(void)
{
//	config_gpio();
    send_ctrl_cmd(0x2800);
    send_ctrl_cmd(0x1000);
    MDELAY(20);
}


static void lcm_resume(void)
{
//	config_gpio();
    send_ctrl_cmd(0x1100);
    MDELAY(200);
	send_ctrl_cmd(0x2900);
}


// ---------------------------------------------------------------------------
//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------
LCM_DRIVER hx8369_rgb_6585_fpga_lcm_drv = 
{
	.name			= "hx8369_rgb_6585_fpga",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
};

