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
    lcm_util.send_cmd(cmd);
}

static __inline void send_data_cmd(unsigned int data)
{
    lcm_util.send_data(data);
}

static __inline unsigned int read_data_cmd()
{
    return lcm_util.read_data();
}

static __inline void set_lcm_register(unsigned int regIndex,
                                      unsigned int regData)
{
    send_ctrl_cmd(regIndex);
    send_data_cmd(regData);
}


static void init_lcm_registers(void)
{
    // AUO 3.17" + ILI9481
    
    send_ctrl_cmd(0X0011);
    MDELAY(20);
    
    send_ctrl_cmd(0X00D0);
    send_data_cmd(0X0007);
    send_data_cmd(0X0041);
    send_data_cmd(0X001B);
    
    send_ctrl_cmd(0X00D1);
    send_data_cmd(0X0000);
    send_data_cmd(0X0016); // 0x001b
    send_data_cmd(0X0012);
    
    send_ctrl_cmd(0X00D2);
    send_data_cmd(0X0001);
    send_data_cmd(0X0011);
    
    send_ctrl_cmd(0X00C0);
    send_data_cmd(0X0010);
    send_data_cmd(0X003B);
    send_data_cmd(0X0000);
    send_data_cmd(0X0012);
    send_data_cmd(0X0001);
    
    send_ctrl_cmd(0X00C1);
    send_data_cmd(0X0010);
    send_data_cmd(0X0013);
    send_data_cmd(0X0088);
    send_ctrl_cmd(0X00C5);
    send_data_cmd(0X0002);
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
    send_ctrl_cmd(0X00F3);
    send_data_cmd(0X0040);
    send_data_cmd(0X000A);
    send_ctrl_cmd(0X00F6);
    send_data_cmd(0X0080);
    send_ctrl_cmd(0X00F7);
    send_data_cmd(0X0080);
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
    params->io_select_mode = 3;

    params->dbi.port                    = 1;
    params->dbi.clock_freq              = LCM_DBI_CLOCK_FREQ_52M;
    params->dbi.data_width              = LCM_DBI_DATA_WIDTH_16BITS;
    params->dbi.data_format.color_order = LCM_COLOR_ORDER_RGB;
    params->dbi.data_format.trans_seq   = LCM_DBI_TRANS_SEQ_MSB_FIRST;
    params->dbi.data_format.padding     = LCM_DBI_PADDING_ON_LSB;
    params->dbi.data_format.format      = LCM_DBI_FORMAT_RGB565;
    params->dbi.data_format.width       = LCM_DBI_DATA_WIDTH_16BITS;
    params->dbi.cpu_write_bits          = LCM_DBI_CPU_WRITE_16_BITS;
    params->dbi.io_driving_current      = 0;

    params->dbi.parallel.write_setup    = 1;
    params->dbi.parallel.write_hold     = 1;
    params->dbi.parallel.write_wait     = 3;
    params->dbi.parallel.read_setup     = 1;
    params->dbi.parallel.read_latency   = 31;
    params->dbi.parallel.wait_period    = 2;
}


static void lcm_init(void)
{
    SET_RESET_PIN(0);
    MDELAY(200);
    SET_RESET_PIN(1);
    MDELAY(400);

    init_lcm_registers();
}


static void lcm_suspend(void)
{
	send_ctrl_cmd(0x28);
}


static void lcm_resume(void)
{
	send_ctrl_cmd(0x29);
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

	// Write To GRAM
	send_ctrl_cmd(0x2C);
}


// ---------------------------------------------------------------------------
//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------
LCM_DRIVER ili9481_lcm_drv = 
{
    .name			= "ili9481",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.update         = lcm_update,
};
