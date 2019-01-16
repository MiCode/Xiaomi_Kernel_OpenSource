#include <linux/string.h>
#ifdef BUILD_UBOOT
#include <asm/arch/mt6516_gpio.h>
#else
#include <mach/mt6516_gpio.h>
#endif
#include "lcm_drv.h"

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define LSA0_GPIO_PIN (GPIO_DISP_LSA0_PIN)
#define LSCE_GPIO_PIN (GPIO_DISP_LSCE_PIN)
#define LSCK_GPIO_PIN (GPIO_DISP_LSCK_PIN)
#define LSDA_GPIO_PIN (GPIO_DISP_LSDA_PIN)

#define FRAME_WIDTH  (480)
#define FRAME_HEIGHT (800)

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))
#define SET_GPIO_OUT(n, v)  (lcm_util.set_gpio_out((n), (v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define SET_LSCE_LOW   SET_GPIO_OUT(LSCE_GPIO_PIN, 0)
#define SET_LSCE_HIGH  SET_GPIO_OUT(LSCE_GPIO_PIN, 1)
#define SET_LSCK_LOW   SET_GPIO_OUT(LSCK_GPIO_PIN, 0)
#define SET_LSCK_HIGH  SET_GPIO_OUT(LSCK_GPIO_PIN, 1)
#define SET_LSDA_LOW   SET_GPIO_OUT(LSDA_GPIO_PIN, 0)
#define SET_LSDA_HIGH  SET_GPIO_OUT(LSDA_GPIO_PIN, 1)

static __inline void spi_send_data(unsigned int data)
{
    unsigned int i;

    SET_LSCE_LOW;
    UDELAY(1);
    SET_LSCK_HIGH;
    SET_LSDA_HIGH;
    UDELAY(1);

    for (i = 0; i < 24; ++ i)
    {
        SET_LSCK_LOW;
        if (data & (1 << 23)) {
            SET_LSDA_HIGH;
        } else {
            SET_LSDA_LOW;
        }
        UDELAY(1);
        SET_LSCK_HIGH;
        UDELAY(1);
        data <<= 1;
    }

    SET_LSDA_HIGH;
    SET_LSCE_HIGH;
}

#define DEVIE_ID (0x1C << 18)

static __inline void send_ctrl_cmd(unsigned int cmd)
{
    unsigned int out = (DEVIE_ID | ((cmd & 0xFF) << 8));
    spi_send_data(out);
}

static __inline void send_data_cmd(unsigned int data)
{
    unsigned int out = (DEVIE_ID | (0x2 << 16) | ((data & 0xFF) << 8));
    spi_send_data(out);
}

static __inline void set_lcm_register(unsigned int regIndex,
                                      unsigned int regData)
{
    send_ctrl_cmd(regIndex);
    send_data_cmd(regData);
}

static void init_lcm_registers(void)
{
    // Power Setting 
    set_lcm_register(0x40, 0x00);   // SAP=0 
    set_lcm_register(0x41, 0x00);   // AP=0,PON=0,COM=0,LON=0
    
    set_lcm_register(0x42, 0x03);   // DIVE=3
    set_lcm_register(0x43, 0x40);   // DC0=0,DC1=4
    set_lcm_register(0x44, 0x31);   // VCOMG=1,VBSL0=1,VBSL1=1
    set_lcm_register(0x45, 0x20);   // VC=0,BT=2
    set_lcm_register(0x46, 0xF1);   // APR=1,VRD=F (*)
    set_lcm_register(0x47, 0xAA);   // VRH=A
    set_lcm_register(0x49, 0x12);   // VDV=12
    set_lcm_register(0x4A, 0x22);   // CHU=2,CLU=2
    
    // Display Setting
    set_lcm_register(0x02, 0x21);   // NL=1,DSZ=2
    set_lcm_register(0x03, 0x04);   // NW=0,BC=1
    set_lcm_register(0x04, 0x09);   // HBP=9
    set_lcm_register(0x05, 0x04);   // VBP=4
    set_lcm_register(0x06, 0x00);   // DPL=0,HPL=0,VPL=0,EPL=0,RIM=0,ENE=0
    set_lcm_register(0x08, 0x03);   // SS=1,BGR=1,REV=0
    set_lcm_register(0x09, 0x03);   // SDTE=3 (*)
    set_lcm_register(0x0A, 0x55);   // EQWE=5,EQWE2=5 (*)
    set_lcm_register(0x0B, 0x01);   // MNT=1,ST=0
    
    // Outline Sharpening
    set_lcm_register(0x10, 0x40);   // EEE=0,COE=4
    set_lcm_register(0x11, 0x00);   // EHSA=000
    set_lcm_register(0x12, 0x00);
    set_lcm_register(0x13, 0x3F);
    set_lcm_register(0x14, 0x01);   // EHEA=13F
    set_lcm_register(0x15, 0x00);
    set_lcm_register(0x16, 0x00);   // EVSA=000
    set_lcm_register(0x17, 0x1F);
    set_lcm_register(0x18, 0x03);   // EVEA=31F
    
    set_lcm_register(0x19, 0x80);   // Contrast:CNTR=80
    set_lcm_register(0x1A, 0x80);   // Contrast:CNTG=80
    set_lcm_register(0x1B, 0x80);   // Contrast:CNTB=80
    set_lcm_register(0x1C, 0x40);   // Bright:BRTR=40
    set_lcm_register(0x1D, 0x40);   // Bright:BRTG=40
    set_lcm_register(0x1E, 0x40);   // Bright:BRTB=40

    // Gate Circuit Setting
    set_lcm_register(0x20, 0x13);   // GG=1,FL=1,FG=1 (*)
    set_lcm_register(0x21, 0x20);   // GNP=0,GLOL=2
    set_lcm_register(0x22, 0x32);   // ACFIX=3,ACCYC=2 (*)
    set_lcm_register(0x23, 0x43);   // ACR=3,ACF=4
    set_lcm_register(0x24, 0x43);   // ACBR=3,ACBF=4
    set_lcm_register(0x25, 0x25);   // AC2R=5,AC2F=2
    set_lcm_register(0x26, 0x25);   // ACB2R=5,ACB2F=2 

    // DCDC Setting
    set_lcm_register(0x30, 0x15);   // RGAP=1,RGPRO=1,RGVLT=1
    set_lcm_register(0x31, 0x50);   // RGCYC=0,RGSTP=5 (*)
    set_lcm_register(0x32, 0x48);   // RGMIN=0,RGHC=1,RGMAX=4
    set_lcm_register(0x34, 0x29);   // RGSFT=1,HTMG=1,RGSFS=2 (*)
    set_lcm_register(0x35, 0x21);   // RGCS=1,RGCST=2

    // Analog
    set_lcm_register(0x50, 0x53);   // HIZ=3,HYP=5
    set_lcm_register(0x60, 0x77);   // HIZ=3,HYP=5 (*)
    set_lcm_register(0x61, 0x00);   // HIZ=3,HYP=5
    set_lcm_register(0x62, 0x30);   // HIZ=3,HYP=5 (*)
    set_lcm_register(0x63, 0xA8);   // HIZ=3,HYP=5 (*)
    set_lcm_register(0x64, 0x9B);   // HIZ=3,HYP=5 (*)
    set_lcm_register(0x65, 0x86);   // HIZ=3,HYP=5 (*)
    set_lcm_register(0x66, 0x06);   // HIZ=3,HYP=5 (*)
    set_lcm_register(0x67, 0x0A);   // HIZ=3,HYP=5 (*)
    set_lcm_register(0x68, 0x01);   // HIZ=3,HYP=5 (*)
    set_lcm_register(0x69, 0x77);   // HIZ=3,HYP=5 (*)
    set_lcm_register(0x6A, 0x06);   // HIZ=3,HYP=5 (*)
    set_lcm_register(0x6B, 0x69);   // HIZ=3,HYP=5 (*)
    set_lcm_register(0x6C, 0xB8);   // HIZ=3,HYP=5 (*)
    set_lcm_register(0x6D, 0x9B);   // HIZ=3,HYP=5 (*)
    set_lcm_register(0x6E, 0x02);   // HIZ=3,HYP=5 (*)
    set_lcm_register(0x6F, 0x00);   // HIZ=3,HYP=5
    set_lcm_register(0x70, 0x0A);   // HIZ=3,HYP=5 (*)
    set_lcm_register(0x71, 0x00);   // HIZ=3,HYP=5

    // Digital V
    set_lcm_register(0x80, 0x20);   // GMRA=20
    set_lcm_register(0x81, 0x40);   // CMRB=40
    set_lcm_register(0x82, 0x80);   // CMRC=80
    set_lcm_register(0x83, 0xC0);   // CMRD=C0
    set_lcm_register(0x84, 0x20);   // CMGA=20
    set_lcm_register(0x85, 0x40);   // CMGB=40
    set_lcm_register(0x86, 0x80);   // CMGC=80
    set_lcm_register(0x87, 0xC0);   // CMGD=C0
    set_lcm_register(0x88, 0x20);   // CMBA=20
    set_lcm_register(0x89, 0x40);   // CMBB=40
    set_lcm_register(0x8A, 0x80);   // CMBC=80
    set_lcm_register(0x8B, 0xC0);   // CMBD=C0

    set_lcm_register(0x01, 0x10);   // Display control:D=0,DTE=0,GON=1,CON=0
    set_lcm_register(0x41, 0x02);   // Power setting:AP=2,PON=0,COM=0,LON=0
    set_lcm_register(0x40, 0x10);   // Power setting:SAP=1
    MDELAY(20);
    set_lcm_register(0x41, 0x32);   // Power setting:AP=2,PON=1,COM=1,LON=0
    MDELAY(50);
    set_lcm_register(0x41, 0xB2);   // Power setting:AP=2,PON=1,COM=1,LON=1 (+)
    MDELAY(30);
    set_lcm_register(0x01, 0x11);   // Display control:D=1,DTE=0,GON=1,CON=0
    MDELAY(20);
    set_lcm_register(0x01, 0x33);   // Display control:D=3,DTE=0,GON=1,CON=1
    MDELAY(20);
    set_lcm_register(0x01, 0x3B);   // Display control:D=3,DTE=1,GON=1,CON=1
    MDELAY(40);
    set_lcm_register(0x22, 0x02);   // Gate circuit setting:ACFIX=0,ACCYC=2 (+)
}


static void config_gpio(void)
{
    const unsigned int USED_GPIOS[] = 
    {
        LSCE_GPIO_PIN,
        LSCK_GPIO_PIN,
        LSDA_GPIO_PIN
    };

    unsigned int i;

    lcm_util.set_gpio_mode(LSA0_GPIO_PIN, GPIO_DISP_LSA0_PIN_M_GPIO);
    lcm_util.set_gpio_mode(LSCE_GPIO_PIN, GPIO_DISP_LSCE_PIN_M_GPIO);
    lcm_util.set_gpio_mode(LSCK_GPIO_PIN, GPIO_DISP_LSCK_PIN_M_GPIO);
    lcm_util.set_gpio_mode(LSDA_GPIO_PIN, GPIO_DISP_LSDA_PIN_M_GPIO);

    for (i = 0; i < ARY_SIZE(USED_GPIOS); ++ i)
    {
        lcm_util.set_gpio_dir(USED_GPIOS[i], 1);               // GPIO out
        lcm_util.set_gpio_pull_enable(USED_GPIOS[i], 0);
    }

    // Swithc LSA0 pin to GPIO mode to avoid data contention,
    // since A0 is connected to LCM's SPI SDO pin
    //
    lcm_util.set_gpio_dir(LSA0_GPIO_PIN, 0);                   // GPIO in
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
    params->ctrl   = LCM_CTRL_GPIO;
    params->width  = FRAME_WIDTH;
    params->height = FRAME_HEIGHT;
    
    params->dpi.mipi_pll_clk_ref  = 0;
    params->dpi.mipi_pll_clk_div1 = 42;
    params->dpi.mipi_pll_clk_div2 = 10;
    params->dpi.dpi_clk_div       = 2;
    params->dpi.dpi_clk_duty      = 1;

    params->dpi.clk_pol           = LCM_POLARITY_FALLING;
    params->dpi.de_pol            = LCM_POLARITY_FALLING;
    params->dpi.vsync_pol         = LCM_POLARITY_FALLING;
    params->dpi.hsync_pol         = LCM_POLARITY_FALLING;

    params->dpi.hsync_pulse_width = 4;
    params->dpi.hsync_back_porch  = 10;
    params->dpi.hsync_front_porch = 18;
    params->dpi.vsync_pulse_width = 2;
    params->dpi.vsync_back_porch  = 2;
    params->dpi.vsync_front_porch = 14;
    
    params->dpi.format            = LCM_DPI_FORMAT_RGB666;
    params->dpi.rgb_order         = LCM_COLOR_ORDER_RGB;
    params->dpi.is_serial_output  = 0;

    params->dpi.intermediat_buffer_num = 2;

    params->dpi.io_driving_current = LCM_DRIVING_CURRENT_4MA;
}


static void lcm_init(void)
{
    config_gpio();

    SET_RESET_PIN(0);
    MDELAY(1);
    SET_RESET_PIN(1);
    MDELAY(2);

    init_lcm_registers();
}


static void lcm_suspend(void)
{
    set_lcm_register(0x01, 0x2A);   // Display control:D=2,DTE=1,GON=0,CON=1 
    MDELAY(20);
    set_lcm_register(0x01, 0x00);   // Display control:D=0,DTE=0,GON=0,CON=0 
    MDELAY(20);
    set_lcm_register(0x40, 0x00);   // Power setting:SAP=0
    set_lcm_register(0x41, 0x00);   // AP=0,PON=0,COM=0,LON=0
    MDELAY(80);
    set_lcm_register(0x40, 0x04);   // Deep standby
}


static void lcm_resume(void)
{
    set_lcm_register(0x40, 0x00);   // DSTB mode cancellation(1)
    MDELAY(1);
    set_lcm_register(0x40, 0x00);   // DSTB mode cancellation(2)
    set_lcm_register(0x40, 0x00);   // DSTB mode cancellation(3)

    init_lcm_registers();
}


// ---------------------------------------------------------------------------
//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------
LCM_DRIVER lg4571_lcm_drv = 
{
    .name			= "lg4571",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
};
