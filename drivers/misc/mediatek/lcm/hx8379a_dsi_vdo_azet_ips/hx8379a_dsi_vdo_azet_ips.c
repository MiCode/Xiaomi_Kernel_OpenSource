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

#if 0
#define MIN_VOLTAGE (100)     // zhoulidong  add for lcm detect
#define MAX_VOLTAGE (300)     // zhoulidong  add for lcm detect
#else
#define MIN_VOLTAGE (250)     //xiaoanxiang change for 6582
#define MAX_VOLTAGE (350)     //xiaoanxiang change for 6582
#endif

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH                          (480)
#define FRAME_HEIGHT                         (800)

#define LCM_ID_HX8379            0x79  //D3


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

#define SET_RESET_PIN(v)                        (lcm_util.set_reset_pin((v)))

#define UDELAY(n)                         (lcm_util.udelay(n))
#define MDELAY(n)                         (lcm_util.mdelay(n))


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------
#define dsi_set_cmdq_V3(para_tbl,size,force_update)        lcm_util.dsi_set_cmdq_V3(para_tbl,size,force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)    lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)        lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)                        lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)            lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)                        lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)           lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#define   LCM_DSI_CMD_MODE                    0

// zhoulidong  add for lcm detect ,read adc voltage
extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int* rawdata);


static LCM_setting_table_V3 lcm_initialization_setting[] = {




    {0x39, 0xFF, 3,{0xFF,0x98,0x16}},



    {0x15, 0xBA, 1,{0x60}},



    {0x15, 0xB0, 1,{0x01}},



    {0x39, 0xBC, 18,{0x01,0x0F,0x61,0x39,0x01,0x01,0x1B,0x11,0x38,0x63,0xFF,0xFF,0x01,0x01,0x0D,0x00,0xFF,0xF2}},



    {0x39, 0xBD, 8,{0x01,0x23,0x45,0x67,0x01,0x23,0x45,0x67}},



    {0x39, 0xBE, 17,{0x13,0x22,0x22,0x22,0x22,0xBB,0xAA,0xDD,0xCC,0x22,0x66,0x22,0x88,0x22,0x22,0x22,0x22}},

    {0x39, 0xED, 2,{0x7F,0x0F}},



    {0x15, 0xF3, 1,{0x70}},



    {0x15, 0xB4, 1,{0x02}},



    {0x39, 0xC0, 3,{0x57,0x0B,0x4A}},  //0x57,0x0B,0x0A



    {0x39, 0xC1, 4,{0x17,0x98,0x88,0x20}},



    {0x15, 0xD8, 1,{0x50}},



    {0x15, 0xFC, 1,{0x08}},



    {0x39, 0xE0, 16,{0x00,0x08,0x11,0x0d,0x10,0x0b,0Xd7,0x06,0x08,0x0c,0x10,0x0F,0x0f,0x19,0x11,0x00}},



    {0x39, 0xE1, 16,{0x00,0x0e,0x14,0x0c,0x0c,0x09,0X77,0x04,0x07,0x0c,0x10,0x0f,0x0d,0x0b,0x05,0x00}},



    {0x39, 0xD5, 8,{0x0A,0x09,0x0F,0x06,0xCB,0XA5,0x01,0x04}},



    {0x15, 0xF7, 1,{0x8A}},



    {0x15, 0xC7, 1,{0x79}},

    {0x15, 0x3A, 1,{0x77}},
    {0x15, 0x36, 1,{0x00}},
    {0x15, 0x35, 1,{0x00}},

    {0x05,0x11,0,{}},
    {REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3, 120, {}},

    {0x39, 0xEE, 9, {0x0A,0x1B,0x5F,0x40,0x00,0x00,0X10,0x00,0x58}},

    {0x05, 0x29,0,{}},
    {REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3, 10, {}},

};

static void init_lcm_registers(void)
{

    unsigned int data_array[16];
#if 0
//HX8379A_BOE3.97IPS_131108
    data_array[0]=0x00043902;//Enable external Command
    data_array[1]=0x7983FFB9;
    dsi_set_cmdq(&data_array, 2, 1);
    MDELAY(1);//3000

    data_array[0]=0x00023902;
    data_array[1]=0x000051BA;
    dsi_set_cmdq(&data_array, 2, 1);
    MDELAY(1);//3000

    data_array[0]=0x00143902;
    data_array[1]=0x445000B1;
    data_array[2]=0x110894DE;
    data_array[3]=0x2f2f1111;
    data_array[4]=0x08421d9d;
    data_array[5]=0xE600F16E;
    dsi_set_cmdq(&data_array, 6, 1);


    data_array[0]=0x000E3902;
    data_array[1]=0x3C0000b2; //
    data_array[2]=0x22190505;
    data_array[3]=0x0409FF00;
    data_array[4]=0x00002019;
    dsi_set_cmdq(&data_array, 5, 1);
    MDELAY(1);


    data_array[0]=0x00203902;
    data_array[1]=0x000A80b4;
    data_array[2]=0x32041032;
    data_array[3]=0x10327013;
    data_array[4]=0x40001708;
    data_array[5]=0x18082304;
    data_array[6]=0x04303008;
    data_array[7]=0x28084000;
    data_array[8]=0x04303008;
    dsi_set_cmdq(&data_array, 9, 1);

    data_array[0]=0x00023902;
    data_array[1]=0x000002CC;
    dsi_set_cmdq(&data_array, 2, 1);

    data_array[0]=0x00303902;//Enable external Command//3
    data_array[1]=0x0A0000d5;
    data_array[2]=0x00000100;
    data_array[3]=0x99011100;
    data_array[4]=0x88103210;
    data_array[5]=0x88886745;
    data_array[6]=0x88888888;
    data_array[7]=0x54768888;
    data_array[8]=0x10325476;
    data_array[9]=0x88881032;
    data_array[10]=0x88888888;
    data_array[11]=0x00008888;
    data_array[12]=0x00000000;
    dsi_set_cmdq(&data_array, 13, 1);

    data_array[0]=0x00253902;
    data_array[1]=0x080079E0;
    data_array[2]=0x3F3F3F0F;
    data_array[3]=0x0C065327;
    data_array[4]=0x1415130F;
    data_array[5]=0x001F1514;
    data_array[6]=0x3F3F0F08;
    data_array[7]=0x0653273F;
    data_array[8]=0x15130F0C;
    data_array[9]=0x1F151414;
    data_array[10]=0x0000001F;
    dsi_set_cmdq(&data_array, 11, 1);
    MDELAY(5);

    data_array[0]=0x00053902;
    data_array[1]=0x008C00B6;
    data_array[2]=0x0000008C;
    dsi_set_cmdq(&data_array, 3, 1);

    data_array[0]=0x00023902;
    data_array[1]=0x0000773A;
    dsi_set_cmdq(&data_array, 2, 1);

    data_array[0]=0x00023902;
    data_array[1]=0x00000036;
    dsi_set_cmdq(&data_array, 2, 1);

    data_array[0] = 0x00110500;
    dsi_set_cmdq(&data_array, 1, 1);
    MDELAY(150);

    data_array[0] = 0x00290500;
    dsi_set_cmdq(&data_array, 1, 1);
    MDELAY(30);


#else
   //HX8379A_BOE3.97IPS
    data_array[0]=0x00043902;//Enable external Command
    data_array[1]=0x7983FFB9;
    dsi_set_cmdq(&data_array, 2, 1);
    MDELAY(1);//3000

    data_array[0]=0x00033902;
    data_array[1]=0x009351BA;
    dsi_set_cmdq(&data_array, 2, 1);
    MDELAY(1);//3000

    data_array[0]=0x00143902;
    data_array[1]=0x445000B1;
    data_array[2]=0x11086FE3;
    data_array[3]=0x38361111;
    data_array[4]=0x0B4229A9;
    data_array[5]=0xE600F16a;
    dsi_set_cmdq(&data_array, 6, 1);


    data_array[0]=0x000E3902;
    data_array[1]=0x3C0000b2; //
    data_array[2]=0x2218040C;
    data_array[3]=0x040CFF00;
    data_array[4]=0x00002018;
    dsi_set_cmdq(&data_array, 5, 1);
    MDELAY(1);


    data_array[0]=0x00203902;
    data_array[1]=0x000C80b4;
    data_array[2]=0x00061030;
    data_array[3]=0x00000000;
    data_array[4]=0x48001100;
    data_array[5]=0x403C2307;
    data_array[6]=0x04303008;
    data_array[7]=0x28084000;
    data_array[8]=0x04303008;
    dsi_set_cmdq(&data_array, 9, 1);

    data_array[0]=0x00023902;
    data_array[1]=0x000002CC;
    dsi_set_cmdq(&data_array, 2, 1);

    data_array[0]=0x00053902;
    data_array[1]=0x008200B6;
    data_array[2]=0x00000082;
    dsi_set_cmdq(&data_array, 3, 1);

    data_array[0]=0x00243902;
    data_array[1]=0x1F0079E0;
    data_array[2]=0x3F3A3323;
    data_array[3]=0x0D07432C;
    data_array[4]=0x14151410;
    data_array[5]=0x00181014;
    data_array[6]=0x3A33231F;
    data_array[7]=0x07432C3F;
    data_array[8]=0x1514100D;
    data_array[9]=0x18101414;
    dsi_set_cmdq(&data_array, 10, 1);
    MDELAY(5);

    data_array[0]=0x00303902;//Enable external Command//3
    data_array[1]=0x0A0000d5;
    data_array[2]=0x00000100;
    data_array[3]=0x99881000;
    data_array[4]=0x88103210;
    data_array[5]=0x88888888;
    data_array[6]=0x88888888;
    data_array[7]=0x88888888;
    data_array[8]=0x99010123;
    data_array[9]=0x88888888;
    data_array[10]=0x88888888;
    data_array[11]=0x00048888;
    data_array[12]=0x00000000;
    dsi_set_cmdq(&data_array, 13, 1);

    //delete by yangjuwei
    //data_array[0]=0x00033902;
    //data_array[1]=0x00010CE4;
    //dsi_set_cmdq(&data_array, 2, 1);
    //delete by yangjuwei

    data_array[0]=0x00023902;
    data_array[1]=0x0000773A;
    dsi_set_cmdq(&data_array, 2, 1);

    data_array[0]=0x00023902;
    data_array[1]=0x00000036;
    dsi_set_cmdq(&data_array, 2, 1);

    data_array[0] = 0x00110500;
    dsi_set_cmdq(&data_array, 1, 1);
    MDELAY(150);

    data_array[0] = 0x00290500;
    dsi_set_cmdq(&data_array, 1, 1);
    MDELAY(30);
    #endif

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

    params->dsi.vertical_sync_active                =4;
    params->dsi.vertical_backporch                    = 10;
    params->dsi.vertical_frontporch                    = 10;
    params->dsi.vertical_active_line                = FRAME_HEIGHT;


    params->dsi.horizontal_sync_active                = 20;///////////////20 20 4  20  14  6
     params->dsi.horizontal_backporch                = 100;
    params->dsi.horizontal_frontporch                = 100;
    params->dsi.horizontal_active_pixel                = FRAME_WIDTH;

        //params->dsi.LPX=8;

        // Bit rate calculation
        //1 Every lane speed
        //params->dsi.pll_select=1;
        //params->dsi.PLL_CLOCK  = LCM_DSI_6589_PLL_CLOCK_377;
        params->dsi.PLL_CLOCK=234;
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
    MDELAY(20);
    SET_RESET_PIN(0);
    MDELAY(20);
    SET_RESET_PIN(1);
    MDELAY(120);

    init_lcm_registers();

}


static LCM_setting_table_V3  lcm_deep_sleep_mode_in_setting[] = {
    // Display off sequence
//    {0x05, 0x28, 0, {}},
//    {REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3, 10, {}},

    // Sleep Mode On
    {0x05, 0x10, 0, {}},
    {REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3, 120, {}},
};

static void lcm_suspend(void)
{

    dsi_set_cmdq_V3(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting)/sizeof(lcm_deep_sleep_mode_in_setting[0]), 1);
    //SET_RESET_PIN(1);
    //SET_RESET_PIN(0);
    //MDELAY(20); // 1ms

    //SET_RESET_PIN(1);
    //MDELAY(120);
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





static unsigned int lcm_compare_id(void)
{
    int array[4];
    char buffer[4]={0,0,0,0};
    char id_high=0;
    char id_low=0;
    int id=0;

    SET_RESET_PIN(1);
    SET_RESET_PIN(0);
    MDELAY(10);
    SET_RESET_PIN(1);
    MDELAY(200);

    array[0]=0x00043902;
    array[1]=0x7983FFB9;
    dsi_set_cmdq(array, 2, 1);

    MDELAY(10);
    array[0]=0x00033902;
    array[1]=0x009351ba;
    dsi_set_cmdq(array, 2, 1);

    MDELAY(10);
    array[0] = 0x00013700;
    dsi_set_cmdq(array, 1, 1);

    MDELAY(10);
    read_reg_v2(0xF4, buffer, 4);//    NC 0x00  0x98 0x16

    id = buffer[0];
    //id_low = buffer[1];
    //id = (id_high<<8) | id_low;

    #ifdef BUILD_LK

        printf("ILI9806 uboot %s \n", __func__);
        printf("%s id = 0x%08x \n", __func__, id);

    #else
        printk("ILI9806 kernel %s \n", __func__);
        printk("%s id = 0x%08x \n", __func__, id);

    #endif


    return (LCM_ID_HX8379 == id)?1:0;

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
    printf("@@@@@@@[adc_uboot]: lcm_vol= %d , file : %s, line : %d\n",lcm_vol, __FILE__, __LINE__);
    #endif

    if (lcm_vol>=MIN_VOLTAGE &&lcm_vol <= MAX_VOLTAGE &&lcm_compare_id())
    {
    return 1;
    }

    return 0;

}
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
LCM_DRIVER hx8379a_dsi_vdo_azet_ips_lcm_drv =
{
        .name            = "hx8379a_dsi_vdo_azet_ips",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params     = lcm_get_params,
    .init           = lcm_init,
    .suspend        = lcm_suspend,
    .resume         = lcm_resume,
    .compare_id     = rgk_lcm_compare_id,
//    .esd_check = lcm_esd_check,
//    .esd_recover = lcm_esd_recover,
#if (LCM_DSI_CMD_MODE)

    .update         = lcm_update,
#endif
};

