#include <linux/string.h>
#if 0
#ifdef BUILD_UBOOT
#include <asm/arch/mt6575_gpio.h>
#else
#include <mach/mt6575_gpio.h>
#endif
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

static void init_lcm_registers(void)
{
    send_ctrl_cmd(0x1100);

    MDELAY(200);

    send_ctrl_cmd(0xC000); send_data_cmd(0x86);
    send_ctrl_cmd(0xC001); send_data_cmd(0x00);
    send_ctrl_cmd(0xC002); send_data_cmd(0x86);
    send_ctrl_cmd(0xC003); send_data_cmd(0x00);
    send_ctrl_cmd(0xC100); send_data_cmd(0x45);
    send_ctrl_cmd(0xC200); send_data_cmd(0x21);
    send_ctrl_cmd(0xC202); send_data_cmd(0x02);
    send_ctrl_cmd(0xB600); send_data_cmd(0x30);
    send_ctrl_cmd(0xB602); send_data_cmd(0x30);
    send_ctrl_cmd(0xC700); send_data_cmd(0x8F);
    send_ctrl_cmd(0xE000); send_data_cmd(0x0E);
    send_ctrl_cmd(0xE001); send_data_cmd(0x14);
    
    send_ctrl_cmd(0xE002); send_data_cmd(0x29);
    send_ctrl_cmd(0xE003); send_data_cmd(0x3A);
    send_ctrl_cmd(0xE004); send_data_cmd(0x1D);
    send_ctrl_cmd(0xE005); send_data_cmd(0x30);
    send_ctrl_cmd(0xE006); send_data_cmd(0x61);
    send_ctrl_cmd(0xE007); send_data_cmd(0x3D);
    send_ctrl_cmd(0xE008); send_data_cmd(0x22);
    send_ctrl_cmd(0xE009); send_data_cmd(0x2A);
    send_ctrl_cmd(0xE00A); send_data_cmd(0x87);
    send_ctrl_cmd(0xE00B); send_data_cmd(0x16);
    send_ctrl_cmd(0xE00C); send_data_cmd(0x3B);
    send_ctrl_cmd(0xE00D); send_data_cmd(0x4C);
    send_ctrl_cmd(0xE00E); send_data_cmd(0x78);
    send_ctrl_cmd(0xE00F); send_data_cmd(0x96);
    send_ctrl_cmd(0xE010); send_data_cmd(0x4A);
    send_ctrl_cmd(0xE011); send_data_cmd(0x4D);
    send_ctrl_cmd(0xE100); send_data_cmd(0x0E);
    send_ctrl_cmd(0xE101); send_data_cmd(0x14);
    send_ctrl_cmd(0xE102); send_data_cmd(0x29);
    send_ctrl_cmd(0xE103); send_data_cmd(0x3A);
    send_ctrl_cmd(0xE104); send_data_cmd(0x1D);
    send_ctrl_cmd(0xE105); send_data_cmd(0x30);
    send_ctrl_cmd(0xE106); send_data_cmd(0x61);
    send_ctrl_cmd(0xE107); send_data_cmd(0x3F);
    send_ctrl_cmd(0xE108); send_data_cmd(0x20);
    send_ctrl_cmd(0xE109); send_data_cmd(0x26);
    send_ctrl_cmd(0xE10A); send_data_cmd(0x83);
    send_ctrl_cmd(0xE10B); send_data_cmd(0x16);
    send_ctrl_cmd(0xE10C); send_data_cmd(0x3B);
    send_ctrl_cmd(0xE10D); send_data_cmd(0x4C);
    send_ctrl_cmd(0xE10E); send_data_cmd(0x78);
    send_ctrl_cmd(0xE10F); send_data_cmd(0x96);
    send_ctrl_cmd(0xE110); send_data_cmd(0x4A);
    send_ctrl_cmd(0xE111); send_data_cmd(0x4D);
    send_ctrl_cmd(0xE200); send_data_cmd(0x0E);
    send_ctrl_cmd(0xE201); send_data_cmd(0x14);
    send_ctrl_cmd(0xE202); send_data_cmd(0x29);
    send_ctrl_cmd(0xE203); send_data_cmd(0x3A);
    send_ctrl_cmd(0xE204); send_data_cmd(0x1D);
    send_ctrl_cmd(0xE205); send_data_cmd(0x30);
    send_ctrl_cmd(0xE206); send_data_cmd(0x61);
    send_ctrl_cmd(0xE207); send_data_cmd(0x3D);
    send_ctrl_cmd(0xE208); send_data_cmd(0x22);
    send_ctrl_cmd(0xE209); send_data_cmd(0x2A);
    send_ctrl_cmd(0xE20A); send_data_cmd(0x87);
    send_ctrl_cmd(0xE20B); send_data_cmd(0x16);
    send_ctrl_cmd(0xE20C); send_data_cmd(0x3B);
    send_ctrl_cmd(0xE20D); send_data_cmd(0x4C);
    send_ctrl_cmd(0xE20E); send_data_cmd(0x78);
    send_ctrl_cmd(0xE20F); send_data_cmd(0x96);
    send_ctrl_cmd(0xE210); send_data_cmd(0x4A);
    send_ctrl_cmd(0xE211); send_data_cmd(0x4D);
    send_ctrl_cmd(0xE300); send_data_cmd(0x0E);
    send_ctrl_cmd(0xE301); send_data_cmd(0x14);
    
    send_ctrl_cmd(0xE302); send_data_cmd(0x29);
    send_ctrl_cmd(0xE303); send_data_cmd(0x3A);
    send_ctrl_cmd(0xE304); send_data_cmd(0x1D);
    send_ctrl_cmd(0xE305); send_data_cmd(0x30);
    send_ctrl_cmd(0xE306); send_data_cmd(0x61);
    send_ctrl_cmd(0xE307); send_data_cmd(0x3F);
    send_ctrl_cmd(0xE308); send_data_cmd(0x20);
    send_ctrl_cmd(0xE309); send_data_cmd(0x26);
    send_ctrl_cmd(0xE30A); send_data_cmd(0x83);
    send_ctrl_cmd(0xE30B); send_data_cmd(0x16);
    send_ctrl_cmd(0xE30C); send_data_cmd(0x3B);
    send_ctrl_cmd(0xE30D); send_data_cmd(0x4C);
    send_ctrl_cmd(0xE30E); send_data_cmd(0x78);
    send_ctrl_cmd(0xE30F); send_data_cmd(0x96);
    send_ctrl_cmd(0xE310); send_data_cmd(0x4A);
    send_ctrl_cmd(0xE311); send_data_cmd(0x4D);
    send_ctrl_cmd(0xE400); send_data_cmd(0x0E);
    send_ctrl_cmd(0xE401); send_data_cmd(0x14);
    send_ctrl_cmd(0xE402); send_data_cmd(0x29);
    send_ctrl_cmd(0xE403); send_data_cmd(0x3A);
    send_ctrl_cmd(0xE404); send_data_cmd(0x1D);
    send_ctrl_cmd(0xE405); send_data_cmd(0x30);
    send_ctrl_cmd(0xE406); send_data_cmd(0x61);
    send_ctrl_cmd(0xE407); send_data_cmd(0x3D);
    send_ctrl_cmd(0xE408); send_data_cmd(0x22);
    send_ctrl_cmd(0xE409); send_data_cmd(0x2A);
    send_ctrl_cmd(0xE40A); send_data_cmd(0x87);
    send_ctrl_cmd(0xE40B); send_data_cmd(0x16);
    send_ctrl_cmd(0xE40C); send_data_cmd(0x3B);
    send_ctrl_cmd(0xE40D); send_data_cmd(0x4C);
    send_ctrl_cmd(0xE40E); send_data_cmd(0x78);
    send_ctrl_cmd(0xE40F); send_data_cmd(0x96);
    send_ctrl_cmd(0xE410); send_data_cmd(0x4A);
    send_ctrl_cmd(0xE411); send_data_cmd(0x4D);
    send_ctrl_cmd(0xE500); send_data_cmd(0x0E);
    send_ctrl_cmd(0xE501); send_data_cmd(0x14);
    send_ctrl_cmd(0xE502); send_data_cmd(0x29);
    send_ctrl_cmd(0xE503); send_data_cmd(0x3A);
    send_ctrl_cmd(0xE504); send_data_cmd(0x1D);
    send_ctrl_cmd(0xE505); send_data_cmd(0x30);
    send_ctrl_cmd(0xE506); send_data_cmd(0x61);
    send_ctrl_cmd(0xE507); send_data_cmd(0x3F);
    send_ctrl_cmd(0xE508); send_data_cmd(0x20);
    send_ctrl_cmd(0xE509); send_data_cmd(0x26);
    send_ctrl_cmd(0xE50A); send_data_cmd(0x83);
    
    send_ctrl_cmd(0xE50B); send_data_cmd(0x16);
    send_ctrl_cmd(0xE50C); send_data_cmd(0x3B);
    send_ctrl_cmd(0xE50D); send_data_cmd(0x4C);
    send_ctrl_cmd(0xE50E); send_data_cmd(0x78);
    send_ctrl_cmd(0xE50F); send_data_cmd(0x96);
    send_ctrl_cmd(0xE510); send_data_cmd(0x4A);
    send_ctrl_cmd(0xE511); send_data_cmd(0x4D);
    send_ctrl_cmd(0x2900);

    MDELAY(200);
    
    send_ctrl_cmd(0x2C00);    
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
    params->dbi.data_width              = LCM_DBI_DATA_WIDTH_16BITS;
    params->dbi.data_format.color_order = LCM_COLOR_ORDER_RGB;
    params->dbi.data_format.trans_seq   = LCM_DBI_TRANS_SEQ_MSB_FIRST;
    params->dbi.data_format.padding     = LCM_DBI_PADDING_ON_LSB;
    params->dbi.data_format.format      = LCM_DBI_FORMAT_RGB565;
    params->dbi.data_format.width       = LCM_DBI_DATA_WIDTH_16BITS;
    params->dbi.cpu_write_bits          = LCM_DBI_CPU_WRITE_32_BITS;
    params->dbi.io_driving_current      = LCM_DRIVING_CURRENT_6575_4MA;

    /* RGB interface configurations */
    
    params->dpi.mipi_pll_clk_div1 = 0x15;
    params->dpi.mipi_pll_clk_div2 = 5;
    params->dpi.dpi_clk_div       = 2;
    params->dpi.dpi_clk_duty      = 1;

    params->dpi.clk_pol           = LCM_POLARITY_FALLING;
    params->dpi.de_pol            = LCM_POLARITY_RISING;
    params->dpi.vsync_pol         = LCM_POLARITY_FALLING;
    params->dpi.hsync_pol         = LCM_POLARITY_FALLING;

    params->dpi.hsync_pulse_width = 10;
    params->dpi.hsync_back_porch  = 30;
    params->dpi.hsync_front_porch = 30;
    params->dpi.vsync_pulse_width = 10;
    params->dpi.vsync_back_porch  = 30;
    params->dpi.vsync_front_porch = 30;
    
    params->dpi.format            = LCM_DPI_FORMAT_RGB565;
    params->dpi.rgb_order         = LCM_COLOR_ORDER_RGB;

	params->dbi.serial.css = 2;
	params->dbi.serial.csh = 2;	
	params->dbi.serial.rd_1st = 2;
	params->dbi.serial.rd_2nd = 2;
	params->dbi.serial.wr_1st = 2;
	params->dbi.serial.wr_2nd = 2;
	params->dbi.serial.sif_3wire = 0;
	params->dbi.serial.sif_sdi = 0;

	params->dbi.serial.sif_1st_pol = 0;
	params->dbi.serial.sif_sck_def = 0;
	params->dbi.serial.sif_div2 = 0;
	params->dbi.serial.sif_hw_cs = 1;

    params->dpi.intermediat_buffer_num = 2;

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

LCM_DRIVER nt35582_rgb_6575_lcm_drv = 
{
	.name			= "nt35582_rgb_6575",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
};

