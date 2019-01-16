#ifndef BUILD_LK
#include <linux/string.h>
#endif
#include "lcm_drv.h"
#include <cust_gpio_usage.h>
#ifdef BUILD_LK
	#include <platform/mt_gpio.h>
	#include <string.h>
#elif defined(BUILD_UBOOT)
	#include <asm/arch/mt_gpio.h>
#else
	#include <mach/mt_gpio.h>
#endif
// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  (720)
#define FRAME_HEIGHT (1280)

#define REGFLAG_DELAY 0xFFFC
#define REGFLAG_UDELAY 0xFFFB

#define REGFLAG_END_OF_TABLE 0xFFFD  // END OF REGISTERS MARKER

#define LCM_DSI_CMD_MODE  0

#define HX8394D_HD720_ID  (0x94)

#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif

#ifndef BUILD_LK
static unsigned int lcm_esd_test = FALSE;      ///only for ESD test
#endif
// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg											lcm_util.dsi_read_reg()
#define read_reg_v2(cmd, buffer, buffer_size)   			lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)    
       
struct LCM_setting_table {
    unsigned int cmd;
    unsigned char count;
    unsigned char para_list[64];
};

static struct LCM_setting_table lcm_init_setting[] = {
	
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

	// Set EXTC
	{0xB9,  3 ,{0xFF, 0x83, 0x94}},

	// Set B0
	{0xB0,  4 ,{0x00, 0x00, 0x7D, 0x0C}},

	// Set MIPI
	{0xBA,  13 ,{0x33, 0x83, 0xA0, 0x6D, 
					0xB2, 0x00, 0x00, 0x40, 
					0x10, 0xFF, 0x0F, 0x00, 
					0x80}},

	// Set Power
	{0xB1,  15 ,{0x64, 0x15, 0x15, 0x34, 
					0x04, 0x11, 0xF1, 0x81, 
					0x76, 0x54, 0x23, 0x80, 
					0xC0, 0xD2, 0x5E}},

	// Set Display
	{0xB2,  15 ,{0x00, 0x64, 0x0E, 0x0D, 
					0x32, 0x1C, 0x08, 0x08, 
					0x1C, 0x4D, 0x00, 0x00, 
					0x30, 0x44, 0x24}},

	// Set CYC
	{0xB4,  22 ,{0x00, 0xFF, 0x03, 0x5A, 
					0x03, 0x5A, 0x03, 0x5A, 
					0x01, 0x70, 0x01, 0x70, 
					0x03, 0x5A, 0x03, 0x5A,  
					0x03, 0x5A, 0x01, 0x70, 
					0x1F, 0x70}},

	// Set D3
	{0xD3,  52 ,{0x00, 0x07, 0x00, 0x3C, 
					0x07, 0x10, 0x00, 0x08, 
					0x10, 0x09, 0x00, 0x09,
					0x54, 0x15, 0x0F, 0x05, 
					0x04, 0x02, 0x12, 0x10, 
					0x05, 0x07, 0x37, 0x33, 
					0x0B, 0x0B, 0x3B, 0x10, 
					0x07, 0x07, 0x08, 0x00, 
					0x00, 0x00, 0x0A, 0x00, 
					0x01, 0x00, 0x00, 0x00, 
					0x00, 0x00, 0x00, 0x00, 
					0x09, 0x05, 0x04, 0x02, 
					0x10, 0x0B, 0x10, 0x00}},

	// Set GIP
	{0xD5,  44 ,{0x1A, 0x1A, 0x1B, 0x1B,
					0x00, 0x01, 0x02, 0x03, 
					0x04, 0x05, 0x06, 0x07, 
					0x08, 0x09, 0x0A, 0x0B, 
					0x24, 0x25, 0x18, 0x18, 
					0x26, 0x27, 0x18, 0x18, 
					0x18, 0x18, 0x18, 0x18, 
					0x18, 0x18, 0x18, 0x18, 
					0x18, 0x18, 0x18, 0x18, 
					0x18, 0x18, 0x20, 0x21, 
					0x18, 0x18, 0x18, 0x18}},

		
		
	// Set D6
	{0xD6,  44 ,{0x1A, 0x1A, 0x1B, 0x1B,
					0x0B, 0x0A, 0x09, 0x08, 
					0x07, 0x06, 0x05, 0x04, 
					0x03, 0x02, 0x01, 0x00, 
					0x21, 0x20, 0x58, 0x58, 
					0x27, 0x26, 0x18, 0x18, 
					0x18, 0x18, 0x18, 0x18, 
					0x18, 0x18, 0x18, 0x18, 
					0x18, 0x18, 0x18, 0x18, 
					0x18, 0x18, 0x25, 0x24, 
					0x18, 0x18, 0x18, 0x18}},
	
	// Set Panel
	{0xCC,  1 ,{0x09}},
	
	// Set TCON Option
	{0xC7,  4 ,{0x00, 0x00, 0x00, 0xC0}},
	
	// Set BD
	{0xBD,  1 ,{0x00}},
	
	// Set D2
	{0xD2,  1 ,{0x66}},
	
	// Set D8
	{0xD8,  24 ,{0xFF, 0xFF, 0xFF, 0xFF, 
					0xFF, 0xF0, 0xFA, 0xAA, 
					0xAA, 0xAA, 0xAA, 0xA0, 
					0xAA, 0xAA, 0xAA, 0xAA, 
					0xAA, 0xA0, 0xAA, 0xAA, 
					0xAA, 0xAA, 0xAA, 0xA0}},
	
	// Set BD
	{0xBD,  1 ,{0x01}},
	
	// Set D8
	{0xD8,  36 ,{0xAA, 0xAA, 0xAA, 0xAA, 
					0xAA, 0xA0, 0xAA, 0xAA, 
					0xAA, 0xAA, 0xAA, 0xA0, 
					0xFF, 0xFF, 0xFF, 0xFF, 
					0xFF, 0xF0, 0xFF, 0xFF, 
					0xFF, 0xFF, 0xFF, 0xF0, 
					0x00, 0x00, 0x00, 0x00, 
					0x00, 0x00, 0x00, 0x00, 
					0x00, 0x00, 0x00, 0x00}},
	
	// Set BD
	{0xBD,  1 ,{0x02}},
	
	// Set D8
	{0xD8,  12 ,{0xFF, 0xFF, 0xFF, 0xFF, 
					0xFF, 0xF0, 0xFF, 0xFF, 
					0xFF, 0xFF, 0xFF, 0xF0}},
	
	// Set BD
	{0xBD,  1 ,{0x00}},
	
	// Set BE
	{0xBE,  2 ,{0x01, 0xE5}},
	
	// Set Power Option
	{0xBF,  3 ,{0x41, 0x0E, 0x01}},
	
	// Set D9
	{0xD9,  3 ,{0x04, 0x01, 0x01}},
	
	// Sleep Out
	{0x11,0,{}},
	{REGFLAG_DELAY, 120, {}},

	// Display ON
	{0x29,0,{}},
	{REGFLAG_DELAY, 20,{}},

    {REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void init_lcm_registers(void)
{
	 unsigned int data_array[16];


	data_array[0]=0x00043902;
	data_array[1]=0x9483ffb9;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(1);
	
	data_array[0]=0x000c3902;
	data_array[1]=0x204373ba;
    data_array[2]=0x0909B265;
    data_array[3]=0x00001040;
	dsi_set_cmdq(data_array, 4, 1);
	MDELAY(1);
	//BAh,1st para=73,2nd para=43,7th para=09,8th para=40,9th para=10,10th para=00,11th para=00
	data_array[0]=0x00103902;
	data_array[1]=0x11116Cb1;
	data_array[2]=0xF1110437;//vgl=-5.55*2
	data_array[3]=0x2394DF80;
	data_array[4]=0x18D2C080;
	dsi_set_cmdq(data_array, 5, 1);
	MDELAY(10);
	
	data_array[0]=0x000C3902;
	data_array[1]=0x0E6400b2;
	data_array[2]=0x0823320D;
        data_array[3]=0x004D1C08;
	dsi_set_cmdq(data_array, 4, 1);
	MDELAY(1);
	
		
	data_array[0]=0x000D3902;
	data_array[1]=0x03FF00b4;
	data_array[2]=0x03460346;
	data_array[3]=0x016A0146;
	data_array[4]=0x0000006A;
	dsi_set_cmdq(data_array, 5, 1);
	MDELAY(1);
	
	data_array[0]=0x00043902;
	data_array[1]=0x010E41BF;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(1);

	data_array[0]=0x00263902;
	data_array[1]=0x000700D3;
	data_array[2]=0x00100000;
	data_array[3]=0x00051032;
	data_array[4]=0x00103205;
	data_array[5]=0x10320000;
	data_array[6]=0x36000000;
	data_array[7]=0x37090903;   
	data_array[8]=0x00370000;
	data_array[9]=0x0A000000;
	data_array[10]=0x00000100;
	dsi_set_cmdq(data_array, 11, 1);
	MDELAY(1);
	

	
	data_array[0]=0x002D3902;
	data_array[1]=0x000302d5;
	data_array[2]=0x04070601;
	data_array[3]=0x22212005;
	data_array[4]=0x18181823;
	data_array[5]=0x18181818;
	data_array[6]=0x18181818;
	data_array[7]=0x18181818;   
	data_array[8]=0x18181818;
	data_array[9]=0x18181818;
	data_array[10]=0x24181818;
	data_array[11]=0x19181825;
	data_array[12]=0x00000019;
	dsi_set_cmdq(data_array, 13, 1);
	MDELAY(1);


	data_array[0]=0x002D3902;
	data_array[1]=0x070405D6;
	data_array[2]=0x03000106;
	data_array[3]=0x21222302;
	data_array[4]=0x18181820;
	data_array[5]=0x58181818;
	data_array[6]=0x18181858;
	data_array[7]=0x18181818;   
	data_array[8]=0x18181818;
	data_array[9]=0x18181818;
	data_array[10]=0x25181818;
	data_array[11]=0x18191924;
	data_array[12]=0x00000018;
	dsi_set_cmdq(data_array, 13, 1);
	MDELAY(1);

	data_array[0]=0x002b3902;
	data_array[1]=0x111202e0;
	data_array[2]=0x203F2829;//	data_array[2]=0x1A332924;
	data_array[3]=0x0C0A083F;//	data_array[3]=0x0D0B083C;
	data_array[4]=0x14120E17;//	data_array[4]=0x15120F17;
	data_array[5]=0x12081413;
	data_array[6]=0x12021916;
	data_array[7]=0x3F282911;   
	data_array[8]=0x0A083F20;//	data_array[8]=0x0B083C1A;
	data_array[9]=0x120E170C;//	data_array[9]=0x120F170D;
	data_array[10]=0x08141314;//	data_array[10]=0x08141315;
	data_array[11]=0x00191612;
	dsi_set_cmdq(data_array, 12, 1);
	MDELAY(1);

	data_array[0]=0x00023902;
	data_array[1]=0x000009cc;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(1);

	data_array[0]=0x00053902;
	data_array[1]=0x40C000c7;
	data_array[2]=0x000000C0;
	dsi_set_cmdq(data_array, 3, 1);
	MDELAY(1);
	
	//data_array[0]=0x00033902;
    //data_array[1]=0x007F69b6;//VCOM
	//dsi_set_cmdq(data_array, 2, 1);
	//MDELAY(1);

	data_array[0]=0x00033902;
        data_array[1]=0x001430C0;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(1);

	data_array[0]=0x00023902;
        data_array[1]=0x000007BC;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(1);

	data_array[0]=0x00023902;
        data_array[1]=0x0000FF51;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(1);

	data_array[0]=0x00023902;
        data_array[1]=0x00002453;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(1);

	data_array[0]=0x00023902;
        data_array[1]=0x00000055;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(20);

	data_array[0]=0x00023902;
	data_array[1]=0x00000035;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(10);

	
	data_array[0]= 0x00110500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(200);
	
	data_array[0]= 0x00290500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(10);   
 
	//data_array[0]=0x00023902;
    //data_array[1]=0x000062b6;//VCOM
	//dsi_set_cmdq(data_array, 2, 1);
	//MDELAY(10);	
}

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
				
			case REGFLAG_UDELAY :
				UDELAY(table[i].count);
				break;

            case REGFLAG_END_OF_TABLE :
                break;

            default:
                dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
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
	
		params->type   = LCM_TYPE_DSI;

		params->width  = FRAME_WIDTH;
		params->height = FRAME_HEIGHT;

#if (LCM_DSI_CMD_MODE)
		params->dsi.mode   = CMD_MODE;
#else
		params->dsi.mode   = BURST_VDO_MODE; //SYNC_PULSE_VDO_MODE;//BURST_VDO_MODE; 
#endif
	
		// DSI
		/* Command mode setting */
		//1 Three lane or Four lane
		params->dsi.LANE_NUM				= LCM_FOUR_LANE;
		//The following defined the fomat for data coming from LCD engine.
		params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;
		params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
		params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
		params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;
		params->dsi.packet_size=256;

		// Video mode setting		
		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
		
		params->dsi.vertical_sync_active				= 4;
		params->dsi.vertical_backporch					= 12;
		params->dsi.vertical_frontporch					= 16;
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 

		params->dsi.horizontal_sync_active				= 24;
		params->dsi.horizontal_backporch				= 100;
		params->dsi.horizontal_frontporch				= 50;
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

	    //params->dsi.LPX=8; 

		// Bit rate calculation
		params->dsi.PLL_CLOCK = 240;

	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 0;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd          = 0x53;
	params->dsi.lcm_esd_check_table[0].count        = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x24;
}

static void lcm_init(void)
{
		SET_RESET_PIN(0);
		MDELAY(20); 
		SET_RESET_PIN(1);
		MDELAY(20); 
		//push_table(lcm_init_setting,sizeof(lcm_init_setting)/sizeof(lcm_init_setting[0]),1);
        init_lcm_registers();
}


static void lcm_suspend(void)
{
	unsigned int data_array[16];

	data_array[0]=0x00280500; // Display Off
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x00100500; // Sleep In
	dsi_set_cmdq(data_array, 1, 1);

	SET_RESET_PIN(1);	
	SET_RESET_PIN(0);
	MDELAY(1); // 1ms
	
	SET_RESET_PIN(1);
	MDELAY(120);     
	//lcm_util.set_gpio_out(GPIO_LCD_ENN, GPIO_OUT_ZERO);
	//lcm_util.set_gpio_out(GPIO_LCD_ENP, GPIO_OUT_ZERO);
}


static void lcm_resume(void)
{
	//lcm_util.set_gpio_out(GPIO_LCD_ENN, GPIO_OUT_ONE);
	//lcm_util.set_gpio_out(GPIO_LCD_ENP, GPIO_OUT_ONE);
	lcm_init();

    #ifdef BUILD_LK
	  printf("[LK]---cmd---hx8394d_hd720_dsi_vdo----%s------\n",__func__);
    #else
	  printk("[KERNEL]---cmd---hx8394d_hd720_dsi_vdo----%s------\n",__func__);
    #endif	
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
	char  buffer;
	unsigned int data_array[2];

	data_array[0]= 0x00043902;
	data_array[1]= (0x94<<24)|(0x83<<16)|(0xff<<8)|0xb9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]= 0x00023902;
	data_array[1]= (0x33<<8)|0xba;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]= 0x00043902;
	data_array[1]= (0x94<<24)|(0x83<<16)|(0xff<<8)|0xb9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00013700;
	dsi_set_cmdq(data_array, 1, 1);

	read_reg_v2(0xf4, &buffer, 1);

	#ifdef BUILD_LK
		printf("%s, LK debug: hx8394d id = 0x%08x\n", __func__, buffer);
    #else
		printk("%s, kernel debug: hx8394d id = 0x%08x\n", __func__, buffer);
    #endif

	return (buffer == HX8394D_HD720_ID ? 1 : 0);

}


static unsigned int lcm_esd_check(void)
{
  #ifndef BUILD_LK

	if(lcm_esd_test)
	{
		lcm_esd_test = FALSE;
		return TRUE;
	}

	char  buffer;
	read_reg_v2(0x0a, &buffer, 1);
	printk("%s, kernel debug: reg = 0x%08x\n", __func__, buffer);

	return FALSE;
	
#else
	return FALSE;
#endif

}

static unsigned int lcm_esd_recover(void)
{
	lcm_init();

	return TRUE;
}



LCM_DRIVER hx8394d_hd720_dsi_vdo_tianma_lcm_drv = 
{
    .name			= "hx8394d_hd720_dsi_tianma_vdo",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id     = lcm_compare_id,
	//.esd_check 	= lcm_esd_check,
	//.esd_recover	= lcm_esd_recover,
#if (LCM_DSI_CMD_MODE)
    .update         = lcm_update,
#endif
    };
