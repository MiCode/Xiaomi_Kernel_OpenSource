#include <linux/string.h>

#include "lcm_drv.h"


// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  (320)
#define FRAME_HEIGHT (480)


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
	send_ctrl_cmd(0x11); //Sleep Out
	MDELAY(300);

	send_ctrl_cmd(0xB4); //Set RM, DM
	send_data_cmd(0x00);

	send_ctrl_cmd(0xC8); //Set Gamma
	send_data_cmd(0x00);
	send_data_cmd(0x34);
	send_data_cmd(0x46);
	send_data_cmd(0x31);
	send_data_cmd(0x00);
	send_data_cmd(0x1A);
	send_data_cmd(0x12);
	send_data_cmd(0x34);
	send_data_cmd(0x77);
	send_data_cmd(0x13);
	send_data_cmd(0x0F);
	send_data_cmd(0x00);
	
	send_ctrl_cmd(0xD0); //Set Power
	send_data_cmd(0x51); //DDVDH  0X44
	send_data_cmd(0x42);
	send_data_cmd(0x0F); //VREG1  0X08

	send_ctrl_cmd(0xD1); //Set VCOM
	send_data_cmd(0x4F); //VCOMH
	send_data_cmd(0x1D); //VCOML
	
	send_ctrl_cmd(0xD2); //Set NOROW
	send_data_cmd(0x01); //SAP
	send_data_cmd(0x12); //DC10/00

	send_ctrl_cmd(0xE9); //Set Panel
	send_data_cmd(0x01);

	send_ctrl_cmd(0xEA); //Set STBA
	send_data_cmd(0x03);
	send_data_cmd(0x00);
	send_data_cmd(0x00);

	send_ctrl_cmd(0xEE); //Set EQ
	send_data_cmd(0x13);
	send_data_cmd(0x00);
	send_data_cmd(0x00);
	send_data_cmd(0x13);

	send_ctrl_cmd(0xED); //Set DIR TIM
	send_data_cmd(0x13);
	send_data_cmd(0x13);
	send_data_cmd(0xA2);
	send_data_cmd(0xA2);
	send_data_cmd(0xA3);
	send_data_cmd(0xA3);
	send_data_cmd(0x13);
	send_data_cmd(0x13);
	send_data_cmd(0x13);
	send_data_cmd(0x13);
	send_data_cmd(0xAE);
	send_data_cmd(0xAE);
	send_data_cmd(0x13);
	send_data_cmd(0xA2);
	send_data_cmd(0x13);
	
	send_ctrl_cmd(0x36); 
	send_data_cmd(0x02);

	send_ctrl_cmd(0x3A); 
	send_data_cmd(0x66);
	
	send_ctrl_cmd(0x51);//write display brightness
	send_data_cmd(0x70);//set brightness 0x00-0xff
	MDELAY(50);

	send_ctrl_cmd(0x53);//write ctrl display
	send_data_cmd(0x24);
	MDELAY(50);

	send_ctrl_cmd(0x55);
	send_data_cmd(0x01);//still picture
	MDELAY(50);

	send_ctrl_cmd(0x5e);//write CABC minumum brightness
	send_data_cmd(0x70);//
	MDELAY(50);

	send_ctrl_cmd(0xCA);  //Set UI GAIN 
    send_data_cmd(0x24); 
    send_data_cmd(0x24); 
    send_data_cmd(0x24); 
    send_data_cmd(0x23); 
    send_data_cmd(0x23); 
    send_data_cmd(0x23); 
    send_data_cmd(0x22); 
    send_data_cmd(0x22); 
    send_data_cmd(0x22); 
    send_data_cmd(0x22); 

	send_ctrl_cmd(0x11); 	
	MDELAY(120);
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
	params->io_select_mode = 3;	
	
	params->dbi.port                    = 0;
	params->dbi.clock_freq              = LCM_DBI_CLOCK_FREQ_104M;
	params->dbi.data_width              = LCM_DBI_DATA_WIDTH_18BITS;
	params->dbi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dbi.data_format.trans_seq   = LCM_DBI_TRANS_SEQ_MSB_FIRST;
	params->dbi.data_format.padding     = LCM_DBI_PADDING_ON_MSB;
	params->dbi.data_format.format      = LCM_DBI_FORMAT_RGB666;
	params->dbi.data_format.width       = LCM_DBI_DATA_WIDTH_18BITS;
	params->dbi.cpu_write_bits          = LCM_DBI_CPU_WRITE_32_BITS;
	params->dbi.io_driving_current      = LCM_DRIVING_CURRENT_8MA;

	params->dbi.parallel.write_setup    = 2;
	params->dbi.parallel.write_hold     = 2;
	params->dbi.parallel.write_wait     = 6;
	params->dbi.parallel.read_setup     = 3;
	params->dbi.parallel.read_latency   = 40;
	params->dbi.parallel.wait_period    = 0;

	// enable tearing-free
    params->dbi.te_mode                 = LCM_DBI_TE_MODE_VSYNC_ONLY;
    params->dbi.te_edge_polarity        = LCM_POLARITY_FALLING;
}


static void lcm_init(void)
{
	SET_RESET_PIN(0);
	MDELAY(25);
	SET_RESET_PIN(1);
	MDELAY(50);
	init_lcm_registers();

    //Set TE register
	send_ctrl_cmd(0x35);
	send_data_cmd(0x00);

    send_ctrl_cmd(0X0044);  // Set TE signal delay scanline
    send_data_cmd(0X0000);  // Set as 0-th scanline
    send_data_cmd(0X0000);
	//sw_clear_panel(0);
}


static void lcm_suspend(void)
{
	send_ctrl_cmd(0x10);
	MDELAY(120);
}


static void lcm_resume(void)
{
	send_ctrl_cmd(0x11);
	MDELAY(120);
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

void lcm_setbacklight(unsigned int level)
{
	if(level > 255) level = 255;
	send_ctrl_cmd(0x51);
	send_data_cmd(level);	
}

LCM_DRIVER hx8357b_lcm_drv = 
{
    .name			= "hx8357b",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.update         = lcm_update,
	.set_backlight	= lcm_setbacklight,
};
