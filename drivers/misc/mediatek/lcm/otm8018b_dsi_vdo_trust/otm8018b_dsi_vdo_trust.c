#ifndef BUILD_LK
#include <linux/string.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
    #include <platform/mt_gpio.h>
    #include <string.h>
#elif defined(BUILD_UBOOT)
    #include <asm/arch/mt_gpio.h>
#else
    #include <mach/mt_gpio.h>
#endif

// ---------------------------------------------------------------------------
//RGK add
// ---------------------------------------------------------------------------
//#include <cust_adc.h>        // zhoulidong  add for lcm detect
#define AUXADC_LCM_VOLTAGE_CHANNEL     0
#define AUXADC_ADC_FDD_RF_PARAMS_DYNAMIC_CUSTOM_CH_CHANNEL     1

#define MIN_VOLTAGE (0)     // zhoulidong  add for lcm detect
#define MAX_VOLTAGE (100)     // zhoulidong  add for lcm detect
// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH                                          (480)
#define FRAME_HEIGHT                                         (800)

#define LCM_ID_OTM8018B                                    0x8009

#ifndef TRUE
    #define   TRUE     1
#endif

#ifndef FALSE
    #define   FALSE    0
#endif

 unsigned static int lcm_esd_test = FALSE;      ///only for ESD test

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util ;

#define SET_RESET_PIN(v)                                    (lcm_util.set_reset_pin((v)))

#define UDELAY(n)                                             (lcm_util.udelay(n))
#define MDELAY(n)                                             (lcm_util.mdelay(n))


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------
#define dsi_set_cmdq_V3(para_tbl,size,force_update)        lcm_util.dsi_set_cmdq_V3(para_tbl,size,force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)            lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)        lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)                                        lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)                    lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)                                            lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)                lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#define   LCM_DSI_CMD_MODE                            0

// zhoulidong  add for lcm detect ,read adc voltage
extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int* rawdata);


static LCM_setting_table_V3 lcm_initialization_setting[] = {

    {0x15,  0x00,1,{0x00}},
    {0x39,  0xFF,3,{0x80,0x09,0x01}},

    {0x15,  0x00,1,{0x80}},
    {0x39,  0xFF,2,{0x80,0x09}}, // enable Orise mode

    {0x15,  0x00,1,{0x03}},
    {0x15,  0xff,1,{0x01}},// enable SPI+I2C cmd2 read

    {0x15,  0x00,1,{0xB4}},
    {0x15,  0xc0,1,{0x10}},//1+2dot inversion

        {0x15,  0x00,1,{0x82}},
    {0x15,  0xc5,1,{0xa3}},

        {0x15,  0x00,1,{0x90}},
    {0x39,  0xc5,2,{0x96,0x76}},//Pump setting (3x=D6)-->(2x=96)//v02 01/11  d5 87

        {0x15,  0x00,1,{0x00}},
    {0x39,  0xd8,2,{0x72,0x75}},//GVDD=4.5V

    {0x15,  0x00,1,{0x00}},
    {0x15,  0xd9,1,{0x69}},// VCOMDC=

    {0x15,  0x00,1,{0x81}},
    {0x15,  0xc1,1,{0x77}}, //Frame rate 65Hz//V02///66

    {0x15,  0x00,1,{0x89}},
    {0x15,  0xc4,1,{0x08}},

    {0x15,  0x00,1,{0xa2}},
    {0x39,  0xc0,3,{0x04,0x00,0x02}},

    {0x15,  0x00,1,{0x80}},
    {0x15,  0xc4,1,{0x30}},

    {0x15,  0x00,1,{0xa6}},
    {0x15,  0xc1,1,{0x01}},

    {0x15,  0x00,1,{0xc0}},
    {0x15,  0xc5,1,{0x00}},

    {0x15,  0x00,1,{0x8b}},
    {0x15,  0xb0,1,{0x40}},

    {0x15,  0x00,1,{0xb2}},
    {0x39,  0xf5,4,{0x15,0x00,0x15,0x00}},

    {0x15,  0x00,1,{0x93}},
    {0x15,  0xc5,1,{0x03}},

    {0x15,  0x00,1,{0x81}},
    {0x15,  0xc4,1,{0x83}},

    {0x15,  0x00,1,{0x92}},
    {0x15,  0xc5,1,{0x01}},

    {0x15,  0x00,1,{0xb1}},
    {0x15,  0xc5,1,{0xa9}},

    {0x15,  0x00,1,{0x90}},
    {0x39,  0xc0,6,{0x00,0x44,0x00,0x00,0x00,0x03}},

    {0x15,  0x00,1,{0xa6}},
    {0x39,  0xc1,3,{0x00,0x00,0x00}},

    //{0x00,1,{0xa0}},
    //{0xc1,1,{0xea}},

        {0x15,  0x00,1,{0x80}},
    {0x39,  0xce,12,{0x87,0x03,0x00,0x85,0x03,0x00,0x86,0x03,0x00,0x84,0x03,0x00}},

    //CEAx : clka1, clka2
    {0x15,  0x00,1,{0xa0}},
    {0x39,  0xce,14,{0x38,0x03,0x03,0x20,0x00,0x00,0x00,0x38,0x02,0x03,0x21,0x00,0x00,0x00}},

    //CEBx : clka3, clka4
    {0x15,  0x00,1,{0xb0}},
    {0x39,  0xce,14,{0x38,0x01,0x03,0x22,0x00,0x00,0x00,0x38,0x00,0x03,0x23,0x00,0x00,0x00}},

    //CECx : clkb1, clkb2
    {0x15,  0x00,1,{0xc0}},
    {0x39,  0xce,14,{0x30,0x00,0x03,0x24,0x00,0x00,0x00,0x30,0x01,0x03,0x25,0x00,0x00,0x00}},

    //CEDx : clkb3, clkb4
    {0x15,  0x00,1,{0xd0}},
    {0x39,  0xce,14,{0x30,0x02,0x03,0x26,0x00,0x00,0x00,0x30,0x03,0x03,0x27,0x00,0x00,0x00}},

    //CFDx :
    {0x15,  0x00,1,{0xc7}},
    {0x15,  0xcf,1,{0x00}},

    {0x15,  0x00,1,{0xc9}},
    {0x15,  0xcf,1,{0x00}},

    {0x15,  0x00,1,{0xc4}},
    {0x39,  0xcb,6,{0x04,0x04,0x04,0x04,0x04,0x04}},

    {0x15,  0x00,1,{0xd9}},
    {0x39,  0xcb,6,{0x04,0x04,0x04,0x04,0x04,0x04}},

    {0x15,  0x00,1,{0x84}},
    {0x39,  0xcc,6,{0x0c,0x0a,0x10,0x0e,0x03,0x04}},

    {0x15,  0x00,1,{0x9e}},
    {0x15,  0xcc,1,{0x0b}},

    {0x15,  0x00,1,{0xa0}},
    {0x39,  0xcc,5,{0x09,0x0f,0x0d,0x01,0x02}},

    {0x15,  0x00,1,{0xb4}},
    {0x39,  0xcc,6,{0x0d,0x0f,0x09,0x0b,0x02,0x01}},

    {0x15,  0x00,1,{0xce}},
    {0x15,  0xcc,1,{0x0e}},

    {0x15,  0x00,1,{0xd0}},
    {0x39,  0xcc,5,{0x10,0x0a,0x0c,0x04,0x03}},

    {0x15,  0x00,1,{0x00}},
    {0x39,  0xe1,16,{0x00,0x02,0x04,0x0c,0x06,0x1c,0x0f,0x0f,0x00,0x04,0x01,0x07,0x0e,0x23,0x20,0x14}},

    {0x15,  0x00,1,{0x00}},
    {0x39,  0xe2,16,{0x00,0x02,0x04,0x0c,0x06,0x1c,0x0f,0x0f,0x00,0x04,0x02,0x07,0x0e,0x24,0x20,0x14}},


    //{0x00,1,{0x00}},
    //{0xE1,16,{0x09,0x0B,0x0F,0x0E,0x0A,0x1B,0x0A,0x0A,0x00,0x03,0x03,0x05,0x0F,0x24,0x1E,0x03}},

        //{0x00,1,{0x00}},
    //{0xE2,16,{0x09,0x0B,0x0F,0x11,0x0E,0x1F,0x0A,0x0A,0x00,0x03,0x03,0x05,0x0F,0x22,0x1E,0x03}},

    {0x15,  0x00,1,{0x00}},
    {0x39,  0xff,3,{0xff,0xff,0xff,}},


    {0x15,  0x35,1,{0x00}}, //open the TE zzf add

    {0x15,  0x3A,1,{0x77}},
    {0x05,0x11,0,{}},
    {REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3, 120, {}},
    {0x05, 0x29,0,{}},
    {REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3, 10, {}},

};

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

        params->type   = LCM_TYPE_DSI;

        params->width  = FRAME_WIDTH;
        params->height = FRAME_HEIGHT;

        #if (LCM_DSI_CMD_MODE)
        params->dsi.mode   = CMD_MODE;
        #else
        params->dsi.mode   = SYNC_PULSE_VDO_MODE; //SYNC_PULSE_VDO_MODE;//BURST_VDO_MODE;
        #endif

        // DSI
        /* Command mode setting */
        //1 Three lane or Four lane
        params->dsi.LANE_NUM                = LCM_TWO_LANE;

        //The following defined the fomat for data coming from LCD engine.
        params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

        // Video mode setting
        params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

        params->dsi.vertical_sync_active                = 4;
        params->dsi.vertical_backporch                    = 16;
        params->dsi.vertical_frontporch                    = 15;
        params->dsi.vertical_active_line                = FRAME_HEIGHT;

        params->dsi.horizontal_sync_active                = 6;
        params->dsi.horizontal_backporch                = 37;
        params->dsi.horizontal_frontporch                = 37;
        params->dsi.horizontal_active_pixel                = FRAME_WIDTH;

        //params->dsi.LPX=8;

        // Bit rate calculation
        //1 Every lane speed
        //params->dsi.pll_select=1;
        //params->dsi.PLL_CLOCK  = LCM_DSI_6589_PLL_CLOCK_377;
        params->dsi.PLL_CLOCK=169;
        params->dsi.pll_div1=0;        // div1=0,1,2,3;div1_real=1,2,4,4 ----0: 546Mbps  1:273Mbps
        params->dsi.pll_div2=0;        // div2=0,1,2,3;div1_real=1,2,4,4
        #if (LCM_DSI_CMD_MODE)
        params->dsi.fbk_div =7;
        #else
        params->dsi.fbk_div =7;    // fref=26MHz, fvco=fref*(fbk_div+1)*2/(div1_real*div2_real)
        #endif
        //params->dsi.compatibility_for_nvk = 1;        // this parameter would be set to 1 if DriverIC is NTK's and when force match DSI clock for NTK's

}


static void lcm_init(void)
{
    SET_RESET_PIN(1);
    SET_RESET_PIN(0);
    MDELAY(10);
    SET_RESET_PIN(1);
    MDELAY(120);

        dsi_set_cmdq_V3(lcm_initialization_setting,sizeof(lcm_initialization_setting)/sizeof(lcm_initialization_setting[0]),1);

}


static LCM_setting_table_V3  lcm_deep_sleep_mode_in_setting[] = {
    // Display off sequence
    {0x05, 0x28, 0, {}},
    {REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3, 10, {}},

    // Sleep Mode On
    {0x05, 0x10, 0, {}},
    {REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3, 120, {}},
};
static void lcm_suspend(void)
{
    dsi_set_cmdq_V3(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting)/sizeof(lcm_deep_sleep_mode_in_setting[0]), 1);

    SET_RESET_PIN(1);
    SET_RESET_PIN(0);
    MDELAY(20); // 1ms

    SET_RESET_PIN(1);
    MDELAY(120);
}


static void lcm_resume(void)
{
    lcm_init();

}

#if (LCM_DSI_CMD_MODE)
static void lcm_update(unsigned int x, unsigned int y,
                       unsigned int width, unsigned int height)
{
    unsigned int x0 = x;
    unsigned int y0 = y;
    unsigned int x1 = x0 + width - 1;
    unsigned int y1 = y0 + height - 1;

    unsigned char x0_MSB = ((x0>>8)&0xFF);
    unsigned char x0_LSB = (x0&0xFF);
    unsigned char x1_MSB = ((x1>>8)&0xFF);
    unsigned char x1_LSB = (x1&0xFF);
    unsigned char y0_MSB = ((y0>>8)&0xFF);
    unsigned char y0_LSB = (y0&0xFF);
    unsigned char y1_MSB = ((y1>>8)&0xFF);
    unsigned char y1_LSB = (y1&0xFF);

    unsigned int data_array[16];

    data_array[0]= 0x00053902;
    data_array[1]= (x1_MSB<<24)|(x0_LSB<<16)|(x0_MSB<<8)|0x2a;
    data_array[2]= (x1_LSB);
    dsi_set_cmdq(data_array, 3, 1);

    data_array[0]= 0x00053902;
    data_array[1]= (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
    data_array[2]= (y1_LSB);
    dsi_set_cmdq(data_array, 3, 1);

    data_array[0]= 0x002c3909;
    dsi_set_cmdq(data_array, 1, 0);

}
#endif





// zhoulidong  add for lcm detect (start)

static unsigned int lcm_compare_id(void)
{
    int array[4];
    char buffer[5];
    char id_high=0;
    char id_low=0;
    int id=0;

    SET_RESET_PIN(1);
    SET_RESET_PIN(0);
    MDELAY(10);
    SET_RESET_PIN(1);
    MDELAY(200);

    array[0] = 0x00053700;
    dsi_set_cmdq(array, 1, 1);
    read_reg_v2(0xa1, buffer, 5);

    id_high = buffer[2];
    id_low = buffer[3];
    id = (id_high<<8) | id_low;


       #ifdef BUILD_LK
       #else

    #if defined(BUILD_UBOOT)
        printf("OTM8018B uboot %s \n", __func__);
           printf("%s id = 0x%08x \n", __func__, id);
    #else
        printk("OTM8018B kernel %s \n", __func__);
        printk("%s id = 0x%08x \n", __func__, id);
    #endif
       #endif

    return 1;
}

// zhoulidong  add for lcm detect (start)
static unsigned int rgk_lcm_compare_id(void)
{
    int data[4] = {0,0,0,0};
    int res = 0;
    int rawdata = 0;
    int lcm_vol = 0;
#ifdef AUXADC_LCM_VOLTAGE_CHANNEL
    res = IMM_GetOneChannelValue(AUXADC_LCM_VOLTAGE_CHANNEL,data,&rawdata);
    if(res < 0)
    {
    #ifdef BUILD_LK
    printf("[adc_uboot]: get data error\n");
    #endif
    return 0;

    }
#endif

    lcm_vol = data[0]*1000+data[1]*10;

    #ifdef BUILD_LK
    printf("[adc_uboot]: lcm_vol= %d\n",lcm_vol);
    #endif

    if (lcm_vol>=MIN_VOLTAGE &&lcm_vol <= MAX_VOLTAGE)
    {
    return 1;
    }

    return 0;

}


// zhoulidong  add for lcm detect (end)

// zhoulidong add for eds(start)
static unsigned int lcm_esd_check(void)
{
    #ifdef BUILD_LK
        //printf("lcm_esd_check()\n");
    #else
        //printk("lcm_esd_check()\n");
    #endif
 #ifndef BUILD_LK
    char  buffer[3];
    int   array[4];

    if(lcm_esd_test)
    {
        lcm_esd_test = FALSE;
        return TRUE;
    }

    array[0] = 0x00013700;
    dsi_set_cmdq(array, 1, 1);

    read_reg_v2(0x0a, buffer, 1);
    if(buffer[0]==0x9c)
    {
        //#ifdef BUILD_LK
        //printf("%s %d\n FALSE", __func__, __LINE__);
        //#else
        //printk("%s %d\n FALSE", __func__, __LINE__);
        //#endif
        return FALSE;
    }
    else
    {
        //#ifdef BUILD_LK
        //printf("%s %d\n FALSE", __func__, __LINE__);
        //#else
        //printk("%s %d\n FALSE", __func__, __LINE__);
        //#endif
        return TRUE;
    }
 #endif

}

static unsigned int lcm_esd_recover(void)
{

    #ifdef BUILD_LK
        printf("lcm_esd_recover()\n");
    #else
        printk("lcm_esd_recover()\n");
    #endif

    lcm_init();

    return TRUE;
}
// zhoulidong add for eds(end)
LCM_DRIVER otm8018b_dsi_vdo_trust_lcm_drv =
{
    .name            = "otm8018b_dsi_vdo_trust",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params     = lcm_get_params,
    .init           = lcm_init,
    .suspend        = lcm_suspend,
    .resume         = lcm_resume,
    .compare_id    = rgk_lcm_compare_id,
//    .esd_check = lcm_esd_check,
//    .esd_recover = lcm_esd_recover,
#if (LCM_DSI_CMD_MODE)
    .update         = lcm_update,
#endif
};

