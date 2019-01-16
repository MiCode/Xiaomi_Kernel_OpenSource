#ifndef BUILD_LK
#include <linux/string.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
    #include <platform/mt_gpio.h>
    #include <platform/mt_i2c.h>
    #include <platform/mt_pmic.h>
#elif defined(BUILD_UBOOT)
    #include <asm/arch/mt_gpio.h>
#else
    #include <mach/mt_pm_ldo.h>
    #include <mach/mt_gpio.h>
#endif
#include <cust_gpio_usage.h>

#include <cust_i2c.h>

#ifdef BUILD_LK
#define LCD_DEBUG(fmt)  dprintf(CRITICAL,fmt)
#else
#define LCD_DEBUG(fmt)  printk(fmt)
#endif


static const unsigned int BL_MIN_LEVEL =20;
static LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)        (lcm_util.set_reset_pin((v)))
#define MDELAY(n)               (lcm_util.mdelay(n))

/* Local Functions */
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)    lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)       lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)                                      lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)                  lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)                                       lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)               lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#endif

static const unsigned char LCD_MODULE_ID = 0x01;
#define LCM_DSI_CMD_MODE                    0

#define FRAME_WIDTH                         (720)
#define FRAME_HEIGHT                        (1280)

#define GPIO_DW8768_ENP GPIO_LCD_BIAS_ENP_PIN
#define GPIO_DW8768_ENN GPIO_LCD_BIAS_ENN_PIN

#define REGFLAG_DELAY                       0xFC
#define REGFLAG_END_OF_TABLE                0xFD


#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif

/* Local Variables */
struct LCM_setting_table {
    unsigned char cmd;
    unsigned char count;
    unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
    /* Display off sequence */
    {0x28, 1, {0x00}},
    {REGFLAG_DELAY, 20, {}},
    /* Sleep Mode On */
    {0x10, 1, {0x00}},
    {REGFLAG_DELAY, 80, {}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_initialization_setting[] = {
    {0x51,  1,  {0xFF}},
    {0x53,  1,  {0x2C}},
    {0x55,  1,  {0x40}},
    {0xB0,  1,  {0x04}},
    {0xC1,  3,  {0x84, 0x61, 0x00}},
    {0xD6,  1,  {0x01}},
    {0x36,  1,  {0x00}},
    /*display on*/
    {0x29,  0,  {}},
    /* exit sleep*/
    {0x11,  0,  {}},
    {REGFLAG_DELAY, 120, {}},
    {REGFLAG_END_OF_TABLE, 0x00, {}},
};

static void init_lcm_registers(void)
{
    unsigned int data_array[16];

    data_array[0]=0x00033902;
    data_array[1]=0x002728b1;
    dsi_set_cmdq(&data_array, 2, 1);
    MDELAY(20);
}

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
    /* Display off sequence */
    {0x28, 1, {0x00}},
    {REGFLAG_DELAY, 20, {}},
    /* Sleep Mode On */
    {0x10, 1, {0x00}},
    {REGFLAG_DELAY, 80, {}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
    unsigned int i;

    for(i = 0; i < count; i++)
    {
        unsigned cmd;
        cmd = table[i].cmd;

        switch (cmd) {

            case REGFLAG_DELAY :
                if(table[i].count <= 10)
                    MDELAY(table[i].count);
                else
                    MDELAY(table[i].count);
                break;

            case REGFLAG_END_OF_TABLE :
                break;

            default:
                dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
        }
    }
}

/* LCM Driver Implementations */
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

    params->dsi.mode   = SYNC_EVENT_VDO_MODE;
    params->dsi.switch_mode = CMD_MODE;
    params->dsi.switch_mode_enable = 0;

    /* Command mode setting */
    params->dsi.LANE_NUM                    = LCM_FOUR_LANE;
    params->dsi.data_format.color_order     = LCM_COLOR_ORDER_RGB;
    params->dsi.data_format.trans_seq       = LCM_DSI_TRANS_SEQ_MSB_FIRST;
    params->dsi.data_format.padding         = LCM_DSI_PADDING_ON_LSB;
    params->dsi.data_format.format          = LCM_DSI_FORMAT_RGB888;

    params->dsi.packet_size=256;

    params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

    params->dsi.vertical_sync_active                = 1;
    params->dsi.vertical_backporch                  = 3;
    params->dsi.vertical_frontporch                 = 6;
    params->dsi.vertical_active_line                = FRAME_HEIGHT;

    params->dsi.horizontal_sync_active              = 5;
    params->dsi.horizontal_backporch                = 60;
    params->dsi.horizontal_frontporch               = 140;
    params->dsi.horizontal_active_pixel             = FRAME_WIDTH;

    params->dsi.PLL_CLOCK = 210;
}

static void lcm_init_power(void)
{
#ifdef BUILD_LK
    mt6325_upmu_set_rg_vgp1_en(1);
    dprintf(0, "vgp3 on\n");
    MDELAY(1);
    mt6325_upmu_set_rg_vgp3_vosel(3);
    mt6325_upmu_set_rg_vgp3_en(1);
#else
    printk("vgp3 on\n");
    hwPowerOn(MT6325_POWER_LDO_VGP3, VOL_1800, "LCD");
#endif
    MDELAY(5);
}

static void lcm_suspend_power(void)
{
#ifdef BUILD_LK
    mt6325_upmu_set_rg_vgp3_en(0);
#else
    hwPowerDown(MT6325_POWER_LDO_VGP3, "LCD");
#endif
    MDELAY(5);
}

static void lcm_resume_power(void)
{
#ifdef BUILD_LK
    mt6325_upmu_set_rg_vgp3_vosel(3);
    mt6325_upmu_set_rg_vgp3_en(1);
#else
    hwPowerOn(MT6325_POWER_LDO_VGP3, VOL_1800, "LCD");
#endif
    MDELAY(5);
}


static void lcm_init(void)
{
    unsigned char cmd = 0x0;
    unsigned char data = 0xFF;
    unsigned int data_array[16];
    int ret=0;
    cmd=0x00;
    data=0x0A;

#ifndef BUILD_LK
    printk("vgp3 on\n");
    hwPowerOn(MT6325_POWER_LDO_VGP3, VOL_1800, "LCD");
#endif
    MDELAY(200);

    /*-----------------DSV start---------------------*/
    mt_set_gpio_mode(GPIO_DW8768_ENP, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_DW8768_ENP, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_DW8768_ENP, GPIO_OUT_ONE);
    mt_set_gpio_mode(GPIO_DW8768_ENN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_DW8768_ENN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_DW8768_ENN, GPIO_OUT_ONE);
    MDELAY(20);
    /*-----------------DSV end---------------------*/

    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ONE);
    MDELAY(20);

    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ZERO);
    MDELAY(2);

    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ONE);
    MDELAY(20);

    push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void)
{
    push_table(lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);

    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ZERO);

    mt_set_gpio_mode(GPIO_DW8768_ENP, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_DW8768_ENP, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_DW8768_ENP, GPIO_OUT_ONE);
    mt_set_gpio_mode(GPIO_DW8768_ENN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_DW8768_ENN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_DW8768_ENN, GPIO_OUT_ONE);
    MDELAY(10);
}

static void lcm_resume(void)
{
    lcm_init();
}

LCM_DRIVER r69338_hd720_dsi_vdo_jdi_dw8755a_drv=
{
    .name               = "r69338_hd720_dsi_vdo_jdi_dw8755a_drv",
    .set_util_funcs     = lcm_set_util_funcs,
    .get_params         = lcm_get_params,
    .init               = lcm_init,/*tianma init fun.*/
    .suspend            = lcm_suspend,
    .resume             = lcm_resume,
     .init_power        = lcm_init_power,
     .suspend_power     = lcm_suspend_power,
};
