#include <linux/string.h>

#include "lcm_drv.h"


// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  (240)
#define FRAME_HEIGHT (320)
#define LCM_ID       (0x5408)
// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))

#define PANEL_CONTROL_DELAY (1)
#define POWER_ON_SEQ_DELAY  (1)


// ---------------------------------------------------------------------------
//  Forward Declarations
// ---------------------------------------------------------------------------

static void lcm_update(unsigned int x, unsigned int y,
                       unsigned int width, unsigned int height);


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
    unsigned int x, y;

    lcm_update(0, 0, FRAME_WIDTH, FRAME_HEIGHT);

    for (y = 0; y < FRAME_HEIGHT; ++ y) {
        for (x = 0; x < FRAME_WIDTH; ++ x) {
            lcm_util.send_data(color);
        }
    }
}

static void init_lcm_registers(void)
{
    set_lcm_register(0x00, 0x0000);
    set_lcm_register(0x01, 0x0100);
    set_lcm_register(0x03, 0x1030);
    set_lcm_register(0x02, 0x0700);    
    set_lcm_register(0x04, 0x0000);
    set_lcm_register(0x08, 0x0207);
    set_lcm_register(0x09, 0x0000);
    set_lcm_register(0x0A, 0x0000);     // FMARK function

    set_lcm_register(0x0C, 0x0000);     // MCU interface setting
    set_lcm_register(0x0D, 0x0000);     // Frame marker Position
    set_lcm_register(0x0F, 0x0000);     // MCU interface polarity

    set_lcm_register(0x07, 0x0101);

    // ----------- Power On sequence -----------
    
    set_lcm_register(0x10, 0x10B0);     // SAP, BT[3:0], AP, DSTB, SLP, STB
    MDELAY(POWER_ON_SEQ_DELAY);
    set_lcm_register(0x11, 0x0007);     // DC1[2:0], DC0[2:0], VC[2:0]
    MDELAY(POWER_ON_SEQ_DELAY);
    set_lcm_register(0x17, 0x0001);
    MDELAY(POWER_ON_SEQ_DELAY);
    set_lcm_register(0x12, 0x01B9);     // VREG1OUT voltage
    MDELAY(POWER_ON_SEQ_DELAY);
    set_lcm_register(0x13, 0x0A00);     // VDV[4:0] for VCOM amplitude
    MDELAY(POWER_ON_SEQ_DELAY);
    set_lcm_register(0x29, 0x0006);     // VCM[4:0] for VCOMH
    MDELAY(POWER_ON_SEQ_DELAY);

    // ----------- Adjust the Gamma  Curve -----------
    
    set_lcm_register(0x30, 0x0002);      
    set_lcm_register(0x31, 0x0720);      
    set_lcm_register(0x32, 0x0924);
    set_lcm_register(0x33, 0x3f10);
    set_lcm_register(0x34, 0x3d06);
    set_lcm_register(0x35, 0x1003);      
    set_lcm_register(0x36, 0x0507);      
    set_lcm_register(0x37, 0x0411);      
    set_lcm_register(0x38, 0x0005);      
    set_lcm_register(0x39, 0x0003);
    set_lcm_register(0x3A, 0x0805);
    set_lcm_register(0x3B, 0x0b02);
    set_lcm_register(0x3C, 0x040f);      
    set_lcm_register(0x3D, 0x050c);
    set_lcm_register(0x3E, 0x0103);
    set_lcm_register(0x3F, 0x0401);  

    // ----------- Set GRAM area -----------
    
    set_lcm_register(0x50, 0x0000);      // Horizontal GRAM Start Address
    set_lcm_register(0x51, 0x00EF);      // Horizontal GRAM End Address
    set_lcm_register(0x52, 0x0000);      // Vertical GRAM Start Address
    set_lcm_register(0x53, 0x013F);      // Vertical GRAM Start Address

      
    set_lcm_register(0x60, 0x2700);      // Gate Scan Line
    set_lcm_register(0x61, 0x0001);      // NDL,VLE, REV
    set_lcm_register(0x6A, 0x0000);      // set scrolling line

    // ----------- Partial Display Control -----------
    
    set_lcm_register(0x80, 0x0000);
    set_lcm_register(0x81, 0x0000);
    set_lcm_register(0x82, 0x0000);
    set_lcm_register(0x83, 0x0000);
    set_lcm_register(0x84, 0x0000);
    set_lcm_register(0x85, 0x0000);

    // ----------- Panel Control -----------
    
    set_lcm_register(0x90, 0x0010);
    set_lcm_register(0x92, 0x0000);
    set_lcm_register(0x93, 0x0103);
    set_lcm_register(0x95, 0x0110);
    set_lcm_register(0x97, 0x0000);
    set_lcm_register(0x98, 0x0000);

    set_lcm_register(0xF0, 0x5408);
    set_lcm_register(0xF2, 0x00DF);     // green, blue point CR
    set_lcm_register(0xF3, 0x0007);
    set_lcm_register(0xF4, 0x001F);
    set_lcm_register(0xF0, 0x0000);

	set_lcm_register(0x07, 0x0173);
	MDELAY(PANEL_CONTROL_DELAY);
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
    params->dbi.clock_freq              = LCM_DBI_CLOCK_FREQ_52M;
    params->dbi.data_width              = LCM_DBI_DATA_WIDTH_16BITS;
    params->dbi.data_format.color_order = LCM_COLOR_ORDER_RGB;
    params->dbi.data_format.trans_seq   = LCM_DBI_TRANS_SEQ_MSB_FIRST;
    params->dbi.data_format.padding     = LCM_DBI_PADDING_ON_LSB;
    params->dbi.data_format.format      = LCM_DBI_FORMAT_RGB565;
    params->dbi.data_format.width       = LCM_DBI_DATA_WIDTH_16BITS;
    params->dbi.cpu_write_bits          = LCM_DBI_CPU_WRITE_16_BITS;
    params->dbi.io_driving_current      = 0;

    params->dbi.parallel.write_setup    = 0;
    params->dbi.parallel.write_hold     = 3;
    params->dbi.parallel.write_wait     = 3;
    params->dbi.parallel.read_setup     = 2;
    params->dbi.parallel.read_latency   = 19;
    params->dbi.parallel.wait_period    = 0;
}


static void lcm_init(void)
{
    SET_RESET_PIN(0);
    MDELAY(2);
    SET_RESET_PIN(1);
    MDELAY(2);

    init_lcm_registers();
    sw_clear_panel(0x0);              // clean screen as all black
}


static void lcm_suspend(void)
{
    set_lcm_register(0x07, 0x0101);
    MDELAY(10);
    set_lcm_register(0x07, 0x0000);   // display off
    MDELAY(10);
    set_lcm_register(0x10, 0x10B2);
    MDELAY(50);
}


static void lcm_resume(void)
{
#if 0
    set_lcm_register(0x00, 0x0000);
    set_lcm_register(0x01, 0x0100);
    set_lcm_register(0x03, 0x10B0);
    set_lcm_register(0x02, 0x0700);   // set N_line inversion
    
    set_lcm_register(0x04, 0x0000);
    set_lcm_register(0x08, 0x0207);
    set_lcm_register(0x09, 0x0000);
    set_lcm_register(0x0A, 0x0000);
    set_lcm_register(0x0C, 0x0000);
    set_lcm_register(0x0D, 0x0000);
    set_lcm_register(0x0F, 0x0000);
    set_lcm_register(0x07, 0x0101);
    
    set_lcm_register(0x10, 0x10B0);   // 0x12B0 power control  start
    MDELAY(0x20);                       // delay 20ms for voltage setup
    set_lcm_register(0x11, 0x0007);
    MDELAY(0x20);
    set_lcm_register(0x17, 0x0001);
    MDELAY(0x20);
    set_lcm_register(0x12, 0x01B9);   // 0x01BD
    MDELAY(0x20);
    set_lcm_register(0x13, 0x0A00);   // 0x1800
    MDELAY(0x20);
    set_lcm_register(0x29, 0x0008);   // 0x0019 power control end
    MDELAY(0x20);
    
    set_lcm_register(0x30, 0x0002);   // Gamma 2.4 start
    set_lcm_register(0x31, 0x0720);
    set_lcm_register(0x32, 0x0924);
    set_lcm_register(0x33, 0x3F10);
    set_lcm_register(0x34, 0x3D06);
    set_lcm_register(0x35, 0x1003);
    set_lcm_register(0x36, 0x0507);
    set_lcm_register(0x37, 0x0411);
    set_lcm_register(0x38, 0x0005);
    set_lcm_register(0x39, 0x0003);
    set_lcm_register(0x3A, 0x0805);
    set_lcm_register(0x3B, 0x0B02);
    set_lcm_register(0x3C, 0x040F);
    set_lcm_register(0x3D, 0x050C);
    set_lcm_register(0x3E, 0x0103);
    set_lcm_register(0x3F, 0x0401);   // Gamma 2.4 end
    
    set_lcm_register(0x50, 0x0000);
    set_lcm_register(0x51, 0x00EF);
    set_lcm_register(0x52, 0x0000);
    set_lcm_register(0x53, 0x013F);
    
    set_lcm_register(0x60, 0x2700);
    set_lcm_register(0x61, 0x0001);
    set_lcm_register(0x6A, 0x0000);
    
    set_lcm_register(0x80, 0x0000);
    set_lcm_register(0x81, 0x0000);
    set_lcm_register(0x82, 0x0000);
    set_lcm_register(0x83, 0x0000);
    set_lcm_register(0x84, 0x0000);
    set_lcm_register(0x85, 0x0000);
    
    set_lcm_register(0x90, 0x0012);
    set_lcm_register(0x92, 0x0000);
    set_lcm_register(0x93, 0x0103);
    set_lcm_register(0x95, 0x0110);
    set_lcm_register(0x97, 0x0000);
    set_lcm_register(0x98, 0x0000);
    
    set_lcm_register(0xF0, 0x5408);
    set_lcm_register(0xF2, 0x00DF);   // green, blue point CR
    set_lcm_register(0xF3, 0x0007);
    set_lcm_register(0xF4, 0x001F);
    set_lcm_register(0xF0, 0x0000);
#endif
    set_lcm_register(0x10, 0x10B0);
    MDELAY(25);
    set_lcm_register(0x07, 0x0173);
    MDELAY(175);
}


static void lcm_update(unsigned int x, unsigned int y,
                       unsigned int width, unsigned int height)
{
    unsigned int x0 = x;
    unsigned int y0 = y;
    unsigned int x1 = x0 + width - 1;
    unsigned int y1 = y0 + height - 1;

	set_lcm_register(0x50, x0);
	set_lcm_register(0x51, x1);
	set_lcm_register(0x52, y0);
	set_lcm_register(0x53, y1);
	set_lcm_register(0x20, x0);
	set_lcm_register(0x21, y0);

    send_ctrl_cmd(0x22);
}

static unsigned int lcm_compare_id(void)
{
	send_ctrl_cmd(0x00);
    return (LCM_ID == read_data_cmd())?1:0;
}


// ---------------------------------------------------------------------------
//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------
LCM_DRIVER spfd5461a_lcm_drv = 
{
    .name			= "spfd5461a",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.update         = lcm_update,
	.compare_id     = lcm_compare_id
};
