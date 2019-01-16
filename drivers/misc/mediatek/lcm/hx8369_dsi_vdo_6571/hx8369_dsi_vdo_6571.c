
#ifdef BUILD_LK
#include <stdio.h>
#include <string.h>
#else
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#include "lcm_drv.h"


// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  (480)
#define FRAME_HEIGHT (800)
#define LCM_ID       (0x69)
#define REGFLAG_DELAY             							0XFE
#define REGFLAG_END_OF_TABLE      							0xFF   // END OF REGISTERS MARKER


#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifdef BUILD_LK
#define LCM_PRINT printf
#else
#define LCM_PRINT printk
#endif

static unsigned int lcm_esd_test = FALSE;      ///only for ESD test

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

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)									lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)				lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)											lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)    

struct LCM_setting_table {
    unsigned char cmd;
    unsigned char count;
    unsigned char para_list[64];
};


static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
    {0x11, 1, {0x00}},
    {REGFLAG_DELAY, 120, {}},
    
    // Display ON
    {0x29, 1, {0x00}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
    // Display off sequence
    {0x28, 1, {0x00}},
    {REGFLAG_DELAY, 10, {}},
    
    // Sleep Mode On
    {0x10, 1, {0x00}},
    {REGFLAG_DELAY, 120, {}},
    
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
    unsigned int i;
    
    for(i = 0; i < count; i++) {
        unsigned cmd;
        cmd = table[i].cmd;
        
        switch (cmd) {
            case REGFLAG_DELAY :
                MDELAY(table[i].count);
                break;
            
            case REGFLAG_END_OF_TABLE :
                break;
            
            default:
                dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);				
            
                if (cmd != 0xFF && cmd != 0x2C && cmd != 0x3C) {
                    //#if defined(BUILD_UBOOT)
                    //	printf("[DISP] - uboot - REG_R(0x%x) = 0x%x. \n", cmd, table[i].para_list[0]);
                    //#endif
                    while(read_reg(cmd) != table[i].para_list[0]);		
                }
        }
    }
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

    params->type = LCM_TYPE_DSI;
    params->width = FRAME_WIDTH;
    params->height = FRAME_HEIGHT;

    params->dsi.mode = BURST_VDO_MODE;
    params->dsi.LANE_NUM = LCM_TWO_LANE;
    params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;
    params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

    params->dsi.vertical_sync_active = 3;
    params->dsi.vertical_backporch = 12;
    params->dsi.vertical_frontporch = 2;
    params->dsi.vertical_active_line = FRAME_HEIGHT;

    params->dsi.horizontal_sync_active = 10;
    params->dsi.horizontal_backporch = 50;
    params->dsi.horizontal_frontporch = 50;
    params->dsi.horizontal_blanking_pixel = 60;
    params->dsi.horizontal_active_pixel = FRAME_WIDTH;

    params->dsi.PLL_CLOCK = 221;
}


static void init_lcm_registers(void)
{
    unsigned int data_array[16];

    // SET password
    data_array[0]= 0x00043902;
    data_array[1]= 0x6983FFB9;
    dsi_set_cmdq(data_array, 2, 1);

    // SET Power
    data_array[0]= 0x00143902;
    data_array[1]= 0x340085B1;
    data_array[2]= 0x0F0F0007;
    data_array[3]= 0x3F3F322A;
    data_array[4]= 0xE6013A01;
    data_array[5]= 0xE6E6E6E6;
    dsi_set_cmdq(data_array, 6, 1);

    // SET Display 480x800
    data_array[0]= 0x00103902;
    data_array[1]= 0x032300B2;
    data_array[2]= 0xFF007003;
    data_array[3]= 0x00000000;
    data_array[4]= 0x01000303;
    dsi_set_cmdq(data_array, 5, 1);

    // SET Display
    data_array[0]= 0x00063902;
    data_array[1]= 0x801800B4;
    data_array[2]= 0x00000206;
    dsi_set_cmdq(data_array, 3, 1);

    // SET VCOM
    data_array[0]= 0x00033902;
    data_array[1]= 0x004242B6;
    dsi_set_cmdq(data_array, 2, 1);

    // SET GIP
    data_array[0]= 0x001B3902;
    data_array[1]= 0x030400D5;
    data_array[2]= 0x28050100;
    data_array[3]= 0x00030170;
    data_array[4]= 0x51064000;
    data_array[5]= 0x41000007;
    data_array[6]= 0x07075006;
    data_array[7]= 0x0000040F;
    dsi_set_cmdq(data_array, 8, 1);

    // SET GAMMA
    data_array[0]= 0x00233902;
    data_array[1]= 0x191300E0;
    data_array[2]= 0x283F3D38;
    data_array[3]= 0x0E0D0746;
    data_array[4]= 0x14121512;
    data_array[5]= 0x1300170F;
    data_array[6]= 0x3F3D3819;
    data_array[7]= 0x0D074628;
    data_array[8]= 0x1215120E;
    data_array[9]= 0x00170F14;
    dsi_set_cmdq(data_array, 10, 1);

    // SET pixel format RGB888
    data_array[0]= 0x773A1500;
    dsi_set_cmdq(data_array, 1, 1);

    // SET MIPI (2 Lane)
    data_array[0]= 0x000E3902;
    data_array[1]= 0xC6A000BA;
    data_array[2]= 0x10000A00;
    data_array[3]= 0x11026F30;
    data_array[4]= 0x00004018;
    dsi_set_cmdq(data_array, 5, 1);

    // Sleep Out
    data_array[0]= 0x00110500;
    dsi_set_cmdq(data_array, 1, 1);

    // Display On
    data_array[0]= 0x00290500;
    dsi_set_cmdq(data_array, 1, 1);
}


static void lcm_init(void)
{
    SET_RESET_PIN(1);
    SET_RESET_PIN(0);
    MDELAY(1);
    SET_RESET_PIN(1);
    MDELAY(10);

    init_lcm_registers();
}


static void lcm_suspend(void)
{
    unsigned int data_array[16];

    data_array[0] = 0x00280500;
    dsi_set_cmdq(data_array, 1, 1);

    data_array[0] = 0x00100500;
    dsi_set_cmdq(data_array, 1, 1);
}


static void lcm_resume(void)
{
    unsigned int data_array[16];

    data_array[0] = 0x00110500;
    dsi_set_cmdq(data_array, 1, 1);
    MDELAY(120);

    data_array[0] = 0x00290500;
    dsi_set_cmdq(data_array, 1, 1);
}
         

static void lcm_setbacklight(unsigned int level)
{
    unsigned int data_array[16];


#if defined(BUILD_LK)
    printf("%s, %d\n", __func__, level);
#else
    printk("lcm_setbacklight = %d\n", level);
#endif
  
    if(level > 255) 
        level = 255;
    
    data_array[0]= 0x00023902;
    data_array[1] =(0x51|(level<<8));
    dsi_set_cmdq(data_array, 2, 1);
}

static unsigned int lcm_esd_check(void)
{
#ifndef BUILD_LK
        if(lcm_esd_test)
        {
            lcm_esd_test = FALSE;
            return TRUE;
        }

        /// please notice: the max return packet size is 1
        /// if you want to change it, you can refer to the following marked code
        /// but read_reg currently only support read no more than 4 bytes....
        /// if you need to read more, please let BinHan knows.
        /*
                unsigned int data_array[16];
                unsigned int max_return_size = 1;
                
                data_array[0]= 0x00003700 | (max_return_size << 16);    
                
                dsi_set_cmdq(&data_array, 1, 1);
        */

        if(read_reg(0xB6) == 0x42)
        {
            return FALSE;
        }
        else
        {            
            return TRUE;
        }
#endif
}

static unsigned int lcm_esd_recover(void)
{
    lcm_init();

    return TRUE;
}

static unsigned int lcm_compare_id(void)
{
    unsigned int id = 0;
    unsigned char buffer[2];
    unsigned int data_array[16];

    SET_RESET_PIN(1);  // NOTE : should reset LCM firstly
    SET_RESET_PIN(0);
    MDELAY(1);
    SET_RESET_PIN(1);
    MDELAY(10);

    // SET password
    data_array[0]= 0x00043902;
    data_array[1]= 0x6983FFB9;
    dsi_set_cmdq(data_array, 2, 1);

    data_array[0] = 0x00023700; // read id return two byte, version and id
    dsi_set_cmdq(data_array, 1, 1);
//	id = read_reg(0xF4);
    read_reg_v2(0xF4, buffer, 2);
    id = buffer[0]; //we only need ID
#if defined(BUILD_LK)
    printf("%s, id1 = 0x%08x\n", __func__, id);
#endif

    return (LCM_ID == id)?1 :0;
}

// ---------------------------------------------------------------------------
//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------
LCM_DRIVER hx8369_dsi_vdo_6571_lcm_drv = 
{
    .name           = "hx8369_dsi_vdo_6571",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params     = lcm_get_params,
    .init           = lcm_init,
    .suspend        = lcm_suspend,
    .resume         = lcm_resume,
    .set_backlight  = lcm_setbacklight,
    //.set_pwm        = lcm_setpwm,
    //.get_pwm        = lcm_getpwm,
    .compare_id     = lcm_compare_id,
    //.esd_check      = lcm_esd_check,
    //.esd_recover    = lcm_esd_recover,
};

