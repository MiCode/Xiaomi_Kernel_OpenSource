#include <linux/string.h>

#include "lcm_drv.h"


// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  (320)
#define FRAME_HEIGHT (480)
#define LCM_ID       (0x8194)
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

static __inline unsigned int HIGH_BYTE(unsigned int val)
{
    return (val >> 8) & 0xFF;
}

static __inline unsigned int LOW_BYTE(unsigned int val)
{
    return (val & 0xFF);
}

static __inline void send_ctrl_cmd(unsigned int cmd)
{
    lcm_util.send_cmd(cmd & 0xFF);
}

static __inline void send_data_cmd(unsigned int data)
{
    lcm_util.send_data(data & 0xFF);
}

static __inline unsigned int read_data_cmd(void)
{
    return lcm_util.read_data();
}

static __inline void set_lcm_register(unsigned int regIndex,
                                      unsigned int regData)
{
    send_ctrl_cmd(regIndex);
    send_data_cmd(regData);
}

static void sw_clear_panel(unsigned int color)
{
    unsigned int x0 = 0;
    unsigned int y0 = 0;
    unsigned int x1 = x0 + FRAME_WIDTH - 1;
    unsigned int y1 = y0 + FRAME_HEIGHT - 1;

    unsigned int x, y;

    send_ctrl_cmd(0x2A); 
    send_data_cmd(HIGH_BYTE(x0));
  	send_data_cmd(LOW_BYTE(x0));
    send_data_cmd(HIGH_BYTE(x1));
  	send_data_cmd(LOW_BYTE(x1));

  	send_ctrl_cmd(0x2B); 
    send_data_cmd(HIGH_BYTE(y0));
  	send_data_cmd(LOW_BYTE(y0));
    send_data_cmd(HIGH_BYTE(y1));
  	send_data_cmd(LOW_BYTE(y1));

	send_ctrl_cmd(0x2C);    // send DDRAM set

    // 18-bit mode (256K color) coding
    for (y = y0; y <= y1; ++ y) {
        for (x = x0; x <= x1; ++ x) {
            lcm_util.send_data(color);
        }
    }
}


static void init_lcm_registers(void)
{
    // FROM 9K0804
    send_ctrl_cmd(0X0011);
    MDELAY(20);
    
    send_ctrl_cmd(0X00D0);  // Power_Setting (D0h)
    send_data_cmd(0X0007);
    send_data_cmd(0X0042);
    send_data_cmd(0X001B);
    send_ctrl_cmd(0X00D1);  // VCOM Control (D1h)
    send_data_cmd(0X0000);
    send_data_cmd(0X0025);
    send_data_cmd(0X0012);
    send_ctrl_cmd(0X00D2);  // Power_Setting for Normal Mode
    send_data_cmd(0X0001);
    send_data_cmd(0X0011);
    send_ctrl_cmd(0X00C0);  // Panel Driving Setting (C0h)
    send_data_cmd(0X0010);
    send_data_cmd(0X003B);
    send_data_cmd(0X0000);
    send_data_cmd(0X0012);
    send_data_cmd(0X0001);
    send_ctrl_cmd(0X00C1);
    send_data_cmd(0X0010);
    send_data_cmd(0X0013);
    send_data_cmd(0X0088);
    // xuecheng, lcm fps setting
    // 0x001 for 125hz
    send_ctrl_cmd(0X00C5);
    send_data_cmd(0X0000);
    
    send_ctrl_cmd(0X00C8);
    send_data_cmd(0X0002);
    send_data_cmd(0X0046);
    send_data_cmd(0X0014);
    send_data_cmd(0X0031);
    send_data_cmd(0X000A);
    send_data_cmd(0X0004);
    send_data_cmd(0X0037);
    send_data_cmd(0X0024);
    send_data_cmd(0X0057);
    send_data_cmd(0X0013);
    send_data_cmd(0X0006);
    send_data_cmd(0X000C);
    send_ctrl_cmd(0X0036);
    send_data_cmd(0X000A);
    send_ctrl_cmd(0X003A);
    send_data_cmd(0X0005);
    send_ctrl_cmd(0X002A);
    send_data_cmd(0X0000);
    send_data_cmd(0X0000);
    send_data_cmd(0X0001);
    send_data_cmd(0X003F);
    send_ctrl_cmd(0X002B);
    send_data_cmd(0X0000);
    send_data_cmd(0X0000);
    send_data_cmd(0X0001);
    send_data_cmd(0X00DF);
    MDELAY(120);

    send_ctrl_cmd(0X0029);
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

    params->dbi.port                    = 1;
    params->dbi.clock_freq              = LCM_DBI_CLOCK_FREQ_104M;
    params->dbi.data_width              = LCM_DBI_DATA_WIDTH_16BITS;
    params->dbi.data_format.color_order = LCM_COLOR_ORDER_RGB;
    params->dbi.data_format.trans_seq   = LCM_DBI_TRANS_SEQ_LSB_FIRST;
    params->dbi.data_format.padding     = LCM_DBI_PADDING_ON_MSB;
    params->dbi.data_format.format      = LCM_DBI_FORMAT_RGB565;
    params->dbi.data_format.width       = LCM_DBI_DATA_WIDTH_16BITS;
    params->dbi.cpu_write_bits          = LCM_DBI_CPU_WRITE_16_BITS;
    params->dbi.io_driving_current      = 0;


#if 0
    params->dbi.te_mode                 = LCM_DBI_TE_MODE_VSYNC_ONLY;
    params->dbi.te_edge_polarity        = LCM_POLARITY_RISING;
#else
    params->dbi.te_mode                 = LCM_DBI_TE_MODE_VSYNC_ONLY;
    params->dbi.te_edge_polarity        = LCM_POLARITY_RISING;
    params->dbi.te_hs_delay_cnt         = 25;
    params->dbi.te_vs_width_cnt         = 223;
    params->dbi.te_vs_width_cnt_div     = LCM_DBI_TE_VS_WIDTH_CNT_DIV_16;
#endif


    params->dbi.parallel.write_setup    = 0;
    params->dbi.parallel.write_hold     = 2;
    params->dbi.parallel.write_wait     = 2;
    params->dbi.parallel.read_setup     = 0;
    params->dbi.parallel.read_latency   = 9;
    params->dbi.parallel.wait_period    = 0;
}


static void lcm_init(void)
{
    SET_RESET_PIN(0);
    MDELAY(40);
    SET_RESET_PIN(1);
    MDELAY(100);

    init_lcm_registers();

    send_ctrl_cmd(0X0035);  // Enable Tearing Control Signal
    send_data_cmd(0X0000);  // Set as mode 1

    send_ctrl_cmd(0X0044);  // Set TE signal delay scanline
    send_data_cmd(0X0000);  // Set as 0-th scanline
    send_data_cmd(0X0000);

    sw_clear_panel(0x0);    // Clean panel as black
}


static void lcm_suspend(void)
{
	send_ctrl_cmd(0x28);

#if 1
    // truely's patch for BT UART noise issue
    send_ctrl_cmd(0xD0);
    send_data_cmd(0x05);
    send_data_cmd(0x47);
    send_data_cmd(0x1D);
	MDELAY(200);
#endif

	send_ctrl_cmd(0x10);
	MDELAY(10);
}


static void lcm_resume(void)
{
#if 1
    // truely's patch for BT UART noise issue
    send_ctrl_cmd(0xD0);
    send_data_cmd(0x07);
    send_data_cmd(0x42);
    send_data_cmd(0x1D);
	MDELAY(200);
#endif

	send_ctrl_cmd(0x11);
	MDELAY(120);
	send_ctrl_cmd(0x29);
	MDELAY(100);            // wait for LCM is stable to show
}


static void lcm_update(unsigned int x, unsigned int y,
                       unsigned int width, unsigned int height)
{
    unsigned int x0 = x;
    unsigned int y0 = y;
    unsigned int x1 = x0 + width - 1;
    unsigned int y1 = y0 + height - 1;

    send_ctrl_cmd(0x2A); 
    send_data_cmd(HIGH_BYTE(x0));
  	send_data_cmd(LOW_BYTE(x0));
    send_data_cmd(HIGH_BYTE(x1));
  	send_data_cmd(LOW_BYTE(x1));

  	send_ctrl_cmd(0x2B); 
    send_data_cmd(HIGH_BYTE(y0));
  	send_data_cmd(LOW_BYTE(y0));
    send_data_cmd(HIGH_BYTE(y1));
  	send_data_cmd(LOW_BYTE(y1));

	send_ctrl_cmd(0x2C);    // send DDRAM set
}

static unsigned int lcm_compare_id(void)
{
    unsigned int id = 0;
	send_ctrl_cmd(0xBF);
	read_data_cmd();//dummy code:0
	read_data_cmd();//MIPI:0x2
	read_data_cmd();//MIPI:0x4
	id = read_data_cmd();//should 0x94
	id |= read_data_cmd() << 8;//should 0x81
	read_data_cmd();//0xFF
    return (LCM_ID == id)?1:0;
}
// ---------------------------------------------------------------------------
//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------
LCM_DRIVER tft1p3037_lcm_drv = 
{
    .name			= "tft1p3037",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.update         = lcm_update,
	.compare_id     = lcm_compare_id
};
