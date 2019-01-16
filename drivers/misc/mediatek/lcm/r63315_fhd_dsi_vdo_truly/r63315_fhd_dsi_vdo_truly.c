/**************************************************************************/
/*                                                            			 */
/*                            PRESENTATION                          	 */
/*              Copyright (c) 2013 JRD Communications, Inc.        		 */
/***************************************************************************/
/*                                                                        */
/*    This material is company confidential, cannot be reproduced in any  */
/*    form without the written permission of JRD Communications, Inc.      */
/*                                                                         */
/*---------------------------------------------------------------------------*/
/*   Author :    XIE Wei        wei.xie@tcl.com                             */
/*---------------------------------------------------------------------------*/
/*    Comments :    driver ic: r63311      module: Truly   								 */
/*    File      :mediatek/custom/common/kernel/lcm/r63311_scribepro/r63311_scribepro.c
/*    Labels   :                                                     */
/*=========================================================*/
/* Modifications on Features list / Changes Request / Problems Report    */                                                                                                           
/* date    | author           | Key                      | comment           */
/*---------|------------------|--------------------------|-------------------*/
/*2013/05/09 |
/*========================================================*/

#ifndef BUILD_LK
#include <linux/string.h>
//#include <linux/delay.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
	#include <platform/mt_gpio.h>
#elif defined(BUILD_UBOOT)
	#include <asm/arch/mt_gpio.h>
#else
	#include <mach/mt_gpio.h>
#endif
// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------
#define FRAME_WIDTH  											(1080)
#define FRAME_HEIGHT 											(1920)
#define REGFLAG_DELAY             								0XFE
#define REGFLAG_END_OF_TABLE      								0xFF   // END OF REGISTERS MARKER

#define SET_RESET_PIN(v)    									(lcm_util.set_reset_pin((v)))
#define SET_GPIO_OUT(n, v)	        							(lcm_util.set_gpio_out((n), (v)))
#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	    lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)			lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)											lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)						lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)											lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

//#define LCD_LDO_ENN_GPIO_PIN          							(GPIO_LCD_ENN)
//#define LCD_LDO_ENP_GPIO_PIN          							(GPIO_LCD_ENP)
#define LCD_LDO_ENN_GPIO_PIN          							(GPIO168)
#define LCD_LDO_ENP_GPIO_PIN          							(GPIO33)


static LCM_UTIL_FUNCS   										lcm_util = {0};


struct LCM_setting_table {
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[64];
};

// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------
static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

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
       	}
    }
}

static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));
	params->type   = LCM_TYPE_DSI;
	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	params->dsi.mode   					= BURST_VDO_MODE;

	params->dsi.LANE_NUM				= LCM_FOUR_LANE;
	//The following defined the fomat for data coming from LCD engine.
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

	params->dsi.packet_size=256;

	params->dsi.intermediat_buffer_num = 0;//because DSI/DPI HW design change, this parameters should be 0 when video mode in MT658X; or memory leakage
	params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.word_count=1080*3;

	params->dsi.vertical_sync_active	= 1;
	params->dsi.vertical_backporch		= 4;
	params->dsi.vertical_frontporch		= 3;
	params->dsi.vertical_active_line	= FRAME_HEIGHT;

	params->dsi.horizontal_sync_active	= 3;
	params->dsi.horizontal_backporch	= 60;
	params->dsi.horizontal_frontporch	= 94;
	params->dsi.horizontal_active_pixel	= FRAME_WIDTH;

	// Bit rate calculation
	params->dsi.PLL_CLOCK=494;	
}

static struct LCM_setting_table lcm_initialization_setting[] = {
	/*
	Note :
	Data ID will depends on the following rule.

		count of parameters > 1	=> Data ID = 0x39
		count of parameters = 1	=> Data ID = 0x15
       	count of parameters = 0	=> Data ID = 0x05
	Structure Format :
	{DCS command, count of parameters, {parameter list}}
	{REGFLAG_DELAY, milliseconds of time, {}},
	...
	Setting ending by predefined flag

	{REGFLAG_END_OF_TABLE, 0x00, {}}
	*/

       /* r63315 start */
	{0x11,	1,	{0x00}},
	{REGFLAG_DELAY, 150, {}},
	{0xB0,	1,	{0x04}},
	{0xB3,	6,	{0x14,0x00,0x00,0x00,0x00,0x00}},
	{0xB6,	2,	{0x3A,0xB3}},
        {0xB7,	1,	{0x00}},
	{0xC0,	1,	{0x33}},
        {0xC1,	34,	{0x84,0x60,0x10,//0x84
	0xEB,0xFF,0x6F,0xCE,
	0xFF,0xFF,0x17,0x12,
	0x58,0x73,0xAE,0x31,
	0x20,0xC6,0xFF,0xFF,
	0x1F,0xF3,0xFF,0x5F,
	0x10,0x10,0x10,0x10,
	0x00,0x62,0x01,0x22,
	0x22,0x01,0x00}},
	
	{0xC2,	7,	{0x31,0xF7,0x80,0x06,0x08,0x80,0x00}},
        {0xC3,	2,	{0x00,0x00}},
	
	{0xC4,	22,	{0x70,0x00,0x00,
	0x00,0x00,0x00,0x00,
	0x00,0x00,0x0C,0x06,
	0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,
	0x00,0x0C,0x06}},
	
	{0xC6,	40,	{0xC8,0x08,0x67,
	0x08,0x67,0x00,0x00,
	0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,
	0x00,0x00,0x16,0x18,
	0x08,0xC8,0x08,0x67,
	0x08,0x67,0x00,0x00,
	0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,
	0x00,0x00,0x16,0x18,
	0x08}},

	{0xCB,	9,	{0x31,0xFC,0x3F,0x8C,0x00,0x00,0x00,0x00,0xC0}},

	{0xCC,	1,	{0x0B}},

	{0xD0,	10,	{0x44,0x81,0xBB,0x5A,0xDA,0x4C,0x19,0x19,0x0C,0x00}},

	{0xD3,	25,	{0x1B,0x33,0xBB,
	0xBB,0xB3,0x33,0x33,
	0x33,0x00,0x01,0x00,
	0x00,0xD8,0xA0,0x0D,
	0x4C,0x4C,0x44,0x3B,
	0x22,0x73,0x07,0x3D,
	0xBF,0x00}},

	{0xD5,	7,	{0x06,0x00,0x00,0x01,0x10,0x01,0x10}},

        {0xC7,	30,	{0x09,0x0A,0x11,
        0x18,0x26,0x33,0x3E,
        0x50,0x38,0x42,0x52,
        0x60,0x67,0x6E,0x70,
        0x09,0x0A,0x11,0x18,
        0x26,0x33,0x3E,0x50,
        0x38,0x42,0x52,0x60,
        0x67,0x6E,0x70}},

	{0xC8,	19,	{0x01,0x00,0x00,
	0x00,0x00,0xFC,0x00,
	0x00,0x00,0x00,0x00,
	0xFC,0x00,0x00,0x00,
	0x00,0x00,0xFC,0x00}},

        {0xDE,	6,	{0x00,0xFF,0x07,0x10,0x00,0x73}},

        {0x35,	1,	{0x00}},

	{0x29,	1,	{0x00}},
	
	{REGFLAG_DELAY, 80, {}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}
    
        /* r63315 end */

};
static unsigned int lcd_id;
static void lcm_init(void)
{
#ifdef BUILD_LK
    printf("%s, %d,r63315 id = 0x%x\n",__FUNCTION__,__LINE__,lcd_id);
#endif

    lcm_util.set_gpio_mode(GPIO112, GPIO_MODE_00);
	lcm_util.set_gpio_dir(GPIO112, GPIO_DIR_OUT); 
	lcm_util.set_gpio_pull_enable(GPIO112, GPIO_PULL_DISABLE); 
	
	lcm_util.set_gpio_mode(LCD_LDO_ENP_GPIO_PIN, GPIO_MODE_00);
	lcm_util.set_gpio_dir(LCD_LDO_ENP_GPIO_PIN, GPIO_DIR_OUT); 
	lcm_util.set_gpio_pull_enable(LCD_LDO_ENP_GPIO_PIN, GPIO_PULL_DISABLE); 

	lcm_util.set_gpio_mode(LCD_LDO_ENN_GPIO_PIN, GPIO_MODE_00);
	lcm_util.set_gpio_dir(LCD_LDO_ENN_GPIO_PIN, GPIO_DIR_OUT); 
	lcm_util.set_gpio_pull_enable(LCD_LDO_ENN_GPIO_PIN, GPIO_PULL_DISABLE); 

	//SET_RESET_PIN(0); //for GPIO reset type  reset low 
	lcm_util.set_gpio_out(GPIO112 , 0); //for GPIO reset type  reset low 

	MDELAY(5);
	
	SET_GPIO_OUT(LCD_LDO_ENP_GPIO_PIN , 1);//power on +5
	MDELAY(10);
	SET_GPIO_OUT(LCD_LDO_ENN_GPIO_PIN , 1);//power on -5
	MDELAY(10);

	//SET_RESET_PIN(1);//rise edge for power on,for LCD idle current= 3.8mA 
	lcm_util.set_gpio_out(GPIO112, 1);	//for LCD idle current= 3.8mA

	MDELAY(10);

    push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);

}

static void lcm_suspend(void)
{
	unsigned int data_array[16];
	
	//SET_RESET_PIN(0);//Reset Low
	lcm_util.set_gpio_mode(GPIO112, GPIO_MODE_00);
	lcm_util.set_gpio_dir(GPIO112, GPIO_DIR_OUT); 
	lcm_util.set_gpio_pull_enable(GPIO112, GPIO_PULL_DISABLE); 
	lcm_util.set_gpio_out(GPIO112 , 0);//for LCD idle current= 3.8mA 
	
	MDELAY(10);

    //SET_RESET_PIN(1);
    lcm_util.set_gpio_out(GPIO112 , 1);//for LCD idle current= 3.8mA 
	
	MDELAY(10);

	//data_array[0] = 0x00280500; 
//	dsi_set_cmdq(data_array, 1, 1);
//	MDELAY(10);

//	data_array[0] = 0x00100500; 
//	dsi_set_cmdq(data_array, 1, 1);
//	MDELAY(120);//delay more for 3 frames time  17*3=54ms

	data_array[0] = 0x04B02900;
	dsi_set_cmdq(data_array, 1, 1);

//	data_array[0] = 0x00000500;
//	dsi_set_cmdq(data_array, 1, 1);

//	data_array[0] = 0x00000500;
//	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x01B12900;//Deep standby
	dsi_set_cmdq(data_array, 1, 1);
    MDELAY(20);

	SET_GPIO_OUT(LCD_LDO_ENN_GPIO_PIN , 0);
	MDELAY(2);
	SET_GPIO_OUT(LCD_LDO_ENP_GPIO_PIN , 0);
	MDELAY(10);

    lcm_util.set_gpio_out(GPIO112 , 0);
	
}

static void lcm_resume(void)
{
//#if defined(BUILD_LK) || defined(BUILD_UBOOT)

//#else
	lcm_init();
//#endif

}

static int dummy_delay = 0;
static unsigned int lcm_esd_check(void)
{  
    #ifndef BUILD_LK
    unsigned int  data_array[16];
    unsigned char buffer_0a;
    unsigned char buffer_0e;
    unsigned char buffer_05;
    unsigned int retval = 0;
    
    dummy_delay ++;
    
    if (dummy_delay >=10000)
        dummy_delay = 0;

    if(dummy_delay %2 == 0)
    {    
        //printk("%s return 1\n",__FUNCTION__);

	    data_array[0] = 0x00013700;
	    dsi_set_cmdq(data_array, 1, 1);
	    read_reg_v2(0x05,&buffer_05, 1);

	    data_array[0] = 0x00013700;
	    dsi_set_cmdq(data_array, 1, 1);
	    read_reg_v2(0x0A,&buffer_0a, 1);

	    data_array[0] = 0x00013700;
	    dsi_set_cmdq(data_array, 1, 1);
	    read_reg_v2(0x0E,&buffer_0e, 1);

            //printk("lcm_esd_check lcm 0x0A is %x-----------------\n", buffer_0a);
	    //printk("lcm_esd_check lcm 0x0E is %x-----------------\n", buffer_0e);
	    //printk("lcm_esd_check lcm 0x05 is %x-----------------\n", buffer_05);
	
	    if ((buffer_0a==0x1C)&&((buffer_0e&0x01)==0x00)&&(buffer_05==0x00)){
		    //printk("diablox_lcd lcm_esd_check done\n");
		    retval = 0;
	    }else{
		    //printk("diablox_lcd lcm_esd_check return true\n");
		    retval = 1;
	    }
    }

	return retval;
    #endif
}

static unsigned int lcm_esd_recover(void)
{
    //printk("%s \n",__FUNCTION__);
    
    lcm_resume();
    lcm_init();

    return 1;
}

#if 0
static unsigned int lcm_compare_id(void)
{
	unsigned int hd_id=0;
	unsigned int id=0;
	unsigned char buffer[5];
	unsigned int array[16];  
	int i;
    #ifdef BUILD_LK
		printf("%s\n", __func__);
    #else
		printk("%s\n", __func__);
    #endif
    
	
	lcm_util.set_gpio_mode(LCD_LDO_ENP_GPIO_PIN, GPIO_MODE_00);
	lcm_util.set_gpio_dir(LCD_LDO_ENP_GPIO_PIN, GPIO_DIR_OUT); 
	lcm_util.set_gpio_pull_enable(LCD_LDO_ENP_GPIO_PIN, GPIO_PULL_DISABLE); 

	lcm_util.set_gpio_mode(LCD_LDO_ENN_GPIO_PIN, GPIO_MODE_00);
	lcm_util.set_gpio_dir(LCD_LDO_ENN_GPIO_PIN, GPIO_DIR_OUT); 
	lcm_util.set_gpio_pull_enable(LCD_LDO_ENN_GPIO_PIN, GPIO_PULL_DISABLE); 

	SET_RESET_PIN(0);

	MDELAY(50);
	
	SET_GPIO_OUT(LCD_LDO_ENP_GPIO_PIN , 1);//power on +5
	MDELAY(2);
	SET_GPIO_OUT(LCD_LDO_ENN_GPIO_PIN , 1);//power on -5
	MDELAY(100);

	SET_RESET_PIN(1);

	MDELAY(50);

    for(i=0;i<10;i++)
    {
	    array[0] = 0x00053700;// read id return two byte,version and id
    	dsi_set_cmdq(array, 1, 1);
	
	    read_reg_v2(0xBF, buffer, 5);
	    MDELAY(20);
	    lcd_id = (buffer[2] << 8 )| buffer[3];
	    if (lcd_id == 0x3155)
	        break;
	}
	
    mt_set_gpio_mode(GPIO_LCD_ID1,0);  // gpio mode   high
    mt_set_gpio_pull_enable(GPIO_LCD_ID1,0);
    mt_set_gpio_dir(GPIO_LCD_ID1,0);  //input
    hd_id = mt_get_gpio_in(GPIO_LCD_ID1);//should be 

#ifdef BUILD_LK
    printf("%s, LK lcm_compare_id debug: R63315_ScribePro lcd_id = 0x%08x\n", __func__, lcd_id);
    printf("%s, LK lcm_compare_id debug: R63315_ScribePro hd_id = 0x%08x\n", __func__, hd_id);
#else
    printk("%s, kernel lcm_compare_id horse debug: R63315_ScribePro lcd_id = 0x%08x\n", __func__, lcd_id);
    printk("%s, kernel lcm_compare_id horse debug: R63315_ScribePro hd_id = 0x%08x\n", __func__, hd_id);
#endif
    
    if (!hd_id) //1=>nt hw lcd;0=>truly hw lcd
    {
        if(lcd_id == 0x3315)
            return 1;
        else
            return 0;
    }
    else
    {
        return 0;
    }
}
#endif

LCM_DRIVER r63315_fhd_dsi_vdo_truly_lcm_drv =
{
    .name			= "r63315_fhd_dsi_vdo_truly",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	//.compare_id   = lcm_compare_id,
	.esd_check		= lcm_esd_check,
	.esd_recover	= lcm_esd_recover,
};
