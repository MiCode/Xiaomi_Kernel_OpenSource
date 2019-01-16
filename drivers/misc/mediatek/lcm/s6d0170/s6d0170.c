#include <linux/string.h>

#include "lcm_drv.h"


// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  (240)
#define FRAME_HEIGHT (320)
#define LCM_ID       (0x0170)
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

static __inline unsigned int to_18_bit_inst_format(unsigned int val)
{
    return ((val & 0xFF00) << 2) |
           ((val & 0x00FF) << 1);
}

static __inline unsigned int to_16_bit_inst_format(unsigned int val)
{
    return (((val >> 2) & 0xFF00) |
           ((val >> 1) & 0x00FF));
}

static __inline void send_ctrl_cmd(unsigned int cmd)
{
    lcm_util.send_cmd(to_18_bit_inst_format(cmd));
}

static __inline void send_data_cmd(unsigned int data)
{
    lcm_util.send_data(to_18_bit_inst_format(data));
}

static __inline unsigned int read_data_cmd(void)
{
    unsigned int data;
    data = to_16_bit_inst_format(lcm_util.read_data());
	return data;
}

static __inline void set_lcm_register(unsigned int regIndex,
                                      unsigned int regData)
{
    send_ctrl_cmd(regIndex);
    send_data_cmd(regData);
}


static void init_lcm_registers(void)
{
    send_ctrl_cmd(0x2300);

	/* Power Supply Setting */
	set_lcm_register(0x11, 0x0000);
    set_lcm_register(0x12, 0x0000);
    set_lcm_register(0x13, 0x0000);
    set_lcm_register(0x14, 0x0000);
    UDELAY(10);
    
    set_lcm_register(0x11, 0x0010);
    set_lcm_register(0x12, 0x3222);
    set_lcm_register(0x13, 0x204E);
    set_lcm_register(0x14, 0x0248);
    set_lcm_register(0x10, 0x0700);
    UDELAY(10);
    
    set_lcm_register(0x11, 0x0112);
    UDELAY(10);
    
    set_lcm_register(0x11, 0x0312);
    UDELAY(10);
    
    set_lcm_register(0x11, 0x0712);
    UDELAY(10);
    
    set_lcm_register(0x11, 0x0F1B);
    UDELAY(10);
    
    set_lcm_register(0x11, 0x0F3B);
    UDELAY(30);

    /* Display Contron Register Setup */
    set_lcm_register(0x01, 0x0136);
    set_lcm_register(0x02, 0x0000);
    set_lcm_register(0x03, 0x9000);
    set_lcm_register(0x07, 0x0104);
    set_lcm_register(0x08, 0x00E2);
    set_lcm_register(0x0B, 0x1100);
    set_lcm_register(0x0C, 0x0000);
    set_lcm_register(0x0F, 0x0001);     // OSC. freq.
    UDELAY(40);
    
    set_lcm_register(0x15, 0x0031);
    set_lcm_register(0x46, 0x00EF);
    set_lcm_register(0x47, 0x0000);
    set_lcm_register(0x48, 0x01AF);
    set_lcm_register(0x49, 0x0000);
    
    // Gamma (R)
    set_lcm_register(0x50, 0x0000);
    set_lcm_register(0x51, 0x030c);
    set_lcm_register(0x52, 0x0801);
    set_lcm_register(0x53, 0x0109);
    set_lcm_register(0x54, 0x0b01);
    set_lcm_register(0x55, 0x0200);
    set_lcm_register(0x56, 0x020d);
    set_lcm_register(0x57, 0x0e00);
    set_lcm_register(0x58, 0x0002);
    set_lcm_register(0x59, 0x010b);

    // Gamma (G)
    set_lcm_register(0x60, 0x0B00);
    set_lcm_register(0x61, 0x000D);
    set_lcm_register(0x62, 0x0000);
    set_lcm_register(0x63, 0x0002);
    set_lcm_register(0x64, 0x0604);
    set_lcm_register(0x65, 0x0000);
    set_lcm_register(0x66, 0x000C);
    set_lcm_register(0x67, 0x060F);
    set_lcm_register(0x68, 0x0F0F);
    set_lcm_register(0x69, 0x0A06);
    
    // Gamma (B)
    set_lcm_register(0x70, 0x0B00);
    set_lcm_register(0x71, 0x000D);
    set_lcm_register(0x72, 0x0000);
    set_lcm_register(0x73, 0x0002);
    set_lcm_register(0x74, 0x0604);
    set_lcm_register(0x75, 0x0000);
    set_lcm_register(0x76, 0x000C);
    set_lcm_register(0x77, 0x060F);
    set_lcm_register(0x78, 0x0F0F);
    set_lcm_register(0x79, 0x0A06);
    set_lcm_register(0x80, 0x0101);
    
    // Display Sequence
    set_lcm_register(0x07, 0x0116);
    UDELAY(40);
    set_lcm_register(0x07, 0x1117);

    set_lcm_register(0x13, 0x2055);

    // Power Control 1(R10h)
    // SAP: Fast    DSTB1F: Off    DSTB: Off    STB: Off
    set_lcm_register(0x10, 0x0700);
		 
    // Blank Period Control(R08h)
    // FP: 2    BP: 2
    set_lcm_register(0x08, 0x0022);
		 
    // Frame Cycle Control(R0Bh)
    // NO: 2 INCLK    SDT: 2 INCLK    DIV: fosc/1    RTN: 17 INCLK
    set_lcm_register(0x0B, 0x2201);
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
    
    params->dbi.port                    = 0;
    params->dbi.clock_freq              = LCM_DBI_CLOCK_FREQ_52M;
    params->dbi.data_width              = LCM_DBI_DATA_WIDTH_18BITS;
    params->dbi.data_format.color_order = LCM_COLOR_ORDER_RGB;
    params->dbi.data_format.trans_seq   = LCM_DBI_TRANS_SEQ_MSB_FIRST;
    params->dbi.data_format.padding     = LCM_DBI_PADDING_ON_LSB;
    params->dbi.data_format.format      = LCM_DBI_FORMAT_RGB666;
    params->dbi.data_format.width       = LCM_DBI_DATA_WIDTH_18BITS;
    params->dbi.cpu_write_bits          = LCM_DBI_CPU_WRITE_32_BITS;
    params->dbi.io_driving_current      = 0;

    // enable tearing-free
    params->dbi.te_mode                 = LCM_DBI_TE_MODE_VSYNC_ONLY;
    params->dbi.te_edge_polarity        = LCM_POLARITY_RISING;

    params->dbi.parallel.write_setup    = 0;
    params->dbi.parallel.write_hold     = 1;
    params->dbi.parallel.write_wait     = 2;
    params->dbi.parallel.read_setup     = 2;
    params->dbi.parallel.read_latency   = 11;
    params->dbi.parallel.wait_period    = 0;
}


static void lcm_init(void)
{
    SET_RESET_PIN(0);
    MDELAY(100);
    SET_RESET_PIN(1);
    MDELAY(500);

    init_lcm_registers();
}


static void lcm_suspend(void)
{
    set_lcm_register(0x15, 0x0000);
    set_lcm_register(0x07, 0x0112);
    UDELAY(15);
    set_lcm_register(0x07, 0x0110);
    UDELAY(15);
    set_lcm_register(0x10, 0x0701);
}


static void lcm_resume(void)
{
    set_lcm_register(0x10, 0x0700);
    UDELAY(15);
    set_lcm_register(0x11, 0x0010);
    set_lcm_register(0x14, 0x1f56);
    set_lcm_register(0x10, 0x0700);
    UDELAY(1);
    set_lcm_register(0x11, 0x0112);
    UDELAY(1);
    set_lcm_register(0x11, 0x0312);
    UDELAY(1);
    set_lcm_register(0x11, 0x0712);
    UDELAY(1);
    set_lcm_register(0x11, 0x0F1B);
    UDELAY(1);
    set_lcm_register(0x11, 0x0F3B);
    UDELAY(3);
    set_lcm_register(0x15, 0x0031);
    set_lcm_register(0x07, 0x1116);
    UDELAY(15);
    set_lcm_register(0x07, 0x1117);
    UDELAY(150);
}


static void lcm_update(unsigned int x, unsigned int y,
                       unsigned int width, unsigned int height)
{
    unsigned int x0 = x;
    unsigned int y0 = y;
    unsigned int x1 = x0 + width - 1;
    unsigned int y1 = y0 + height - 1;

	set_lcm_register(0x46, x1);
	set_lcm_register(0x47, x0);
	set_lcm_register(0x48, y1);
	set_lcm_register(0x49, y0);

    send_ctrl_cmd(0x22);
}

static unsigned int lcm_compare_id(void)
{
    send_ctrl_cmd(0x2300);

	send_ctrl_cmd(0x05);
    return (LCM_ID == read_data_cmd())?1:0;
}
// ---------------------------------------------------------------------------
//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------

LCM_DRIVER s6d0170_lcm_drv = 
{
	.name			= "s6d0170",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.update         = lcm_update,
	.compare_id     = lcm_compare_id
};
