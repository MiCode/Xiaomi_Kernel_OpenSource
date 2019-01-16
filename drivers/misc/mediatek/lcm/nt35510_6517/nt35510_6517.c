#include <linux/string.h>
#if defined(BUILD_UBOOT)
#include <asm/arch/mt6577_gpio.h>
#else
#include <mach/mt6577_gpio.h>
#endif

#include "lcm_drv.h"

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  										(480)
#define FRAME_HEIGHT 										(854)

#define REGFLAG_DELAY             							0XFE
#define REGFLAG_END_OF_TABLE      							0xFF   // END OF REGISTERS MARKER

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))

#define UDELAY(n) 											(lcm_util.udelay(n))
#define MDELAY(n) 											(lcm_util.mdelay(n))

#define LCM_ID       (0x55)
#define LCM_ID1       (0xC1)
#define LCM_ID2       (0x80)

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


static void lcm_get_params(LCM_PARAMS *params)
{
		memset(params, 0, sizeof(LCM_PARAMS));
	
		params->type   = LCM_TYPE_DSI;

		params->width  = FRAME_WIDTH;
		params->height = FRAME_HEIGHT;

		// enable tearing-free
		params->dbi.te_mode 			= LCM_DBI_TE_MODE_VSYNC_ONLY;
		params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;

		params->dsi.mode   = CMD_MODE;

		// DSI
		/* Command mode setting */
		params->dsi.LANE_NUM				= LCM_TWO_LANE;
		//The following defined the fomat for data coming from LCD engine.
		params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
		params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
		params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
		params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

		// Highly depends on LCD driver capability.
		params->dsi.packet_size=256;

		// Bit rate calculation
		params->dsi.pll_div1=34;		// fref=26MHz, fvco=fref*(div1+1)	(div1=0~63, fvco=500MHZ~1GHz)
		params->dsi.pll_div2=1;			// div2=0~15: fout=fvo/(2*div2)

		params->dsi.HS_TRAIL	= 10;//min max(n*8*UI, 60ns+n*4UI)
		params->dsi.HS_ZERO 	= 8;//min 105ns+6*UI
		params->dsi.HS_PRPR 	= 4;//min 40ns+4*UI; max 85ns+6UI
		params->dsi.LPX 		= 12;//min 50ns
		
		params->dsi.TA_GO		= 12;//4*LPX
	
		params->dsi.CLK_TRAIL	= 5;//min 60ns
		params->dsi.CLK_ZERO	= 18;//min 300ns-38ns
		params->dsi.LPX_WAIT	= 10;
		params->dsi.CONT_DET	= 0;
		
		params->dsi.CLK_HS_PRPR = 4;//min 38ns; max 95ns
}

static void init_lcm_registers(void)
{
	unsigned int data_array[16];

//*************Enable CMD2 Page1  *******************//
	data_array[0]=0x00063902;
	data_array[1]=0x52AA55F0;
	data_array[2]=0x00000108;
	dsi_set_cmdq(data_array, 3, 1);


//AVDD: 6.0V
    data_array[0]=0x00043902;
	data_array[1]=0x0A0A0AB0;
	dsi_set_cmdq(data_array, 2, 1);
    data_array[0]=0x00043902;
	data_array[1]=0x444444B6;
    dsi_set_cmdq(data_array, 2, 1);

//AVEE: -6.0V 
    data_array[0]=0x00043902;
	data_array[1]=0x0A0A0AB1;
    dsi_set_cmdq(data_array, 2, 1);	
    data_array[0]=0x00043902;
	data_array[1]=0x343434B7;
    dsi_set_cmdq(data_array, 2, 1);	

//#VGH:12V 
    data_array[0]=0x00043902;
	data_array[1]=0x050505B3;
    dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00043902;
	data_array[1]=0x242424B9;
    dsi_set_cmdq(data_array, 2, 1);

//#VGLX:-10V 
    data_array[0]=0x00043902;
	data_array[1]=0x080808B5;
    dsi_set_cmdq(data_array, 2, 1);    
	data_array[0]=0x00043902;
	data_array[1]=0x141414BA;
    dsi_set_cmdq(data_array, 2, 1);

//#VGMP:4.7V  /VGSP:0V 
    data_array[0]=0x00043902;
	data_array[1]=0x008800BC;
    dsi_set_cmdq(data_array, 2, 1);  

//#VGMN:-4.7V /VGSN:0V
	data_array[0]=0x00043902;
	data_array[1]=0x008800BD;
    dsi_set_cmdq(data_array, 2, 1);

//##VCOM  Setting 
    data_array[0]=0x00033902;
	data_array[1]=0x002D00BE;
    dsi_set_cmdq(data_array, 2, 1);  

//VCL
	data_array[0]=0x00043902;
	data_array[1]=0x020202B2;
    dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00043902;
	data_array[1]=0x242424B8;
    dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00063902;
	data_array[1]=0x52AA55F0;
	data_array[2]=0x00000108;
	dsi_set_cmdq(data_array, 3, 1);
//# R+ 
	data_array[0]=0x00353902;
	data_array[1]=0x002900D1;
	data_array[2]=0x00330030;
	data_array[3]=0x0068004A;
	data_array[4]=0x01C9009F;
	data_array[5]=0x01310105;
	data_array[6]=0x019A0170;
	data_array[7]=0x021402E0;
	data_array[8]=0x02410215;
	data_array[9]=0x0286026E;
	data_array[10]=0x02AF02A0;
	data_array[11]=0x02CE02C1;
	data_array[12]=0x02E302DC;
	data_array[13]=0x03FA02EF;
	data_array[14]=0x00000060;
	dsi_set_cmdq(data_array, 15, 1);

//#G +  
	data_array[0]=0x00353902;
	data_array[1]=0x002900D2;
	data_array[2]=0x00330030;
	data_array[3]=0x0068004A;
	data_array[4]=0x01C9009F;
	data_array[5]=0x01310105;
	data_array[6]=0x019A0170;
	data_array[7]=0x021402E0;
	data_array[8]=0x02410215;
	data_array[9]=0x0286026E;
	data_array[10]=0x02AF02A0;
	data_array[11]=0x02CE02C1;
	data_array[12]=0x02E302DC;
	data_array[13]=0x03FD02EF;
	data_array[14]=0x00000000;
	dsi_set_cmdq(data_array, 15, 1);

//#B + 
	data_array[0]=0x00353902;
	data_array[1]=0x002900D3;
	data_array[2]=0x00330030;
	data_array[3]=0x0068004A;
	data_array[4]=0x01C9009F;
	data_array[5]=0x01310105;
	data_array[6]=0x019A0170;
	data_array[7]=0x021402E0;
	data_array[8]=0x02410215;
	data_array[9]=0x0286026E;
	data_array[10]=0x02AF02A0;
	data_array[11]=0x02CE02C1;
	data_array[12]=0x02E302DC;
	data_array[13]=0x02F502EF;
	data_array[14]=0x000000D0;
	dsi_set_cmdq(data_array, 15, 1);

//#R - 
	data_array[0]=0x00353902;
	data_array[1]=0x002900D4;
	data_array[2]=0x00330030;
	data_array[3]=0x0068004A;
	data_array[4]=0x01C9009F;
	data_array[5]=0x01310105;
	data_array[6]=0x019A0170;
	data_array[7]=0x021402E0;
	data_array[8]=0x02410215;
	data_array[9]=0x0286026E;
	data_array[10]=0x02AF02A0;
	data_array[11]=0x02CE02C1;
	data_array[12]=0x02E302DC;
	data_array[13]=0x03FA02EF;
	data_array[14]=0x00000060;
	dsi_set_cmdq(data_array, 15, 1);

//#G -
	data_array[0]=0x00353902;
	data_array[1]=0x002900D5;
	data_array[2]=0x00330030;
	data_array[3]=0x0068004A;
	data_array[4]=0x01C9009F;
	data_array[5]=0x01310105;
	data_array[6]=0x019A0170;
	data_array[7]=0x021402E0;
	data_array[8]=0x02410215;
	data_array[9]=0x0286026E;
	data_array[10]=0x02AF02A0;
	data_array[11]=0x02CE02C1;
	data_array[12]=0x02E302DC;
	data_array[13]=0x03FD02EF;
	data_array[14]=0x00000000;
	dsi_set_cmdq(data_array, 15, 1);

//#B - 
	data_array[0]=0x00353902;
	data_array[1]=0x002900D6;
	data_array[2]=0x00330030;
	data_array[3]=0x0068004A;
	data_array[4]=0x01C9009F;
	data_array[5]=0x01310105;
	data_array[6]=0x019A0170;
	data_array[7]=0x021402E0;
	data_array[8]=0x02410215;
	data_array[9]=0x0286026E;
	data_array[10]=0x02AF02A0;
	data_array[11]=0x02CE02C1;
	data_array[12]=0x02E302DC;
	data_array[13]=0x02F502EF;
	data_array[14]=0x000000D0;
	dsi_set_cmdq(data_array, 15, 1);
//#######  ENABLE PAGE 0  ############              
	data_array[0]=0x00063902;
	data_array[1]=0x52AA55F0;
    data_array[2]=0x00000008;
    dsi_set_cmdq(data_array, 3, 1);
//###################################     
//#Vivid Color
	data_array[0]=0x10B41500;
    dsi_set_cmdq(data_array, 1, 1);

//#480*854 
	data_array[0]=0x6CB51500;
    dsi_set_cmdq(data_array, 1, 1);

//#SDT
	data_array[0]=0x05B61500;
    dsi_set_cmdq(data_array, 1, 1);

//#2dot-Inversion  
	data_array[0]=0x00043902;
	data_array[1]=0x020202BC;
    dsi_set_cmdq(data_array, 2, 1);

//#Display option control-> video mode 
	data_array[0]=0x00033902;
	data_array[1]=0x0000D4B1; 
    dsi_set_cmdq(data_array, 2, 1);
	data_array[0]=0x00063902;
	data_array[1]=0x500200C9;
	data_array[2]=0x00005050;
    dsi_set_cmdq(data_array, 3, 1);

//#Gate EQ for rising edge  
	data_array[0]=0x00033902;
	data_array[1]=0x008080B7;
    dsi_set_cmdq(data_array, 2, 1);

//#Source EQ
	data_array[0]=0x00053902;
	data_array[1]=0x070701B8;
	data_array[2]=0x00000007;
    dsi_set_cmdq(data_array, 3, 1);
	data_array[0]=0x00053902;
	data_array[1]=0x0100002A;
	data_array[2]=0x000000DF;
    dsi_set_cmdq(data_array, 3, 1);
	data_array[0]=0x00053902;
	data_array[1]=0x0300002B;
	data_array[2]=0x00000055;
    dsi_set_cmdq(data_array, 3, 1);	

        data_array[0]=0x00351500;
    dsi_set_cmdq(data_array, 1, 1); 

	data_array[0]= 0x00023902;
	data_array[1]= 0x0051;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]= 0x00023902;
	data_array[1]= 0x2453;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]= 0x00023902;
	data_array[1]= 0x0055;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]= 0x00023902;
	data_array[1]= 0x705e;
	dsi_set_cmdq(data_array, 2, 1);

}

static void lcm_init(void)
{
    SET_RESET_PIN(1);
    MDELAY(1);
    SET_RESET_PIN(0);
    MDELAY(10);
    SET_RESET_PIN(1);
    MDELAY(10);

    init_lcm_registers();
}


static void lcm_suspend(void)
{
	//push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);
	unsigned int data_array[16];
	
	data_array[0]=0x00280500;
	dsi_set_cmdq(data_array, 1, 1);
	//MDELAY(50);
	
	data_array[0]=0x00100500;
	dsi_set_cmdq(data_array, 1, 1);	
	MDELAY(120);
}

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
	data_array[3]= 0x00053902;
	data_array[4]= (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
	data_array[5]= (y1_LSB);
	data_array[6]= 0x002c3909;

	dsi_set_cmdq(data_array, 7, 0);

}


void lcm_setbacklight(unsigned int level)
{
	unsigned int data_array[16];

#if defined(BUILD_UBOOT)
        printf("%s,lcm_setbacklight level=%d\n", __func__,level);
	MDELAY(50);
#else
	printk("lcm_setbacklight level=%d\n",level);
#endif

	if(level > 255) 
	    level = 255;

	data_array[0]= 0x00023902;
	data_array[1] =(0x51|(level<<8));
	dsi_set_cmdq(data_array, 2, 1);
}

static void lcm_resume(void)
{
	//push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
	unsigned int data_array[16];
	
	lcm_init();
	data_array[0]=0x00110500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(120);
	
	data_array[0]=0x00290500;
	dsi_set_cmdq(data_array, 1, 1);	
}

void lcm_setpwm(unsigned int divider)
{
	// TBD
}


unsigned int lcm_getpwm(unsigned int divider)
{
	// ref freq = 15MHz, B0h setting 0x80, so 80.6% * freq is pwm_clk;
	// pwm_clk / 255 / 2(lcm_setpwm() 6th params) = pwm_duration = 23706
	unsigned int pwm_clk = 23706 / (1<<divider);	
	return pwm_clk;
}

static unsigned int lcm_compare_id(void)
{
	unsigned int id = 0, id2 = 0;
	unsigned char buffer[2];

	unsigned int data_array[16];
	
	SET_RESET_PIN(1);  //NOTE:should reset LCM firstly
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(10);	

/*	
	data_array[0] = 0x00110500;		// Sleep Out
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(120);
*/
		
//*************Enable CMD2 Page1  *******************//
	data_array[0]=0x00063902;
	data_array[1]=0x52AA55F0;
	data_array[2]=0x00000108;
	dsi_set_cmdq(data_array, 3, 1);
	MDELAY(10); 

	data_array[0] = 0x00023700;// read id return two byte,version and id
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(10); 
	
	read_reg_v2(0xC5, buffer, 2);
	id = buffer[0]; //we only need ID
	id2= buffer[1]; //we test buffer 1

        return (LCM_ID == id)?1:0;
}

// ---------------------------------------------------------------------------
//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------

LCM_DRIVER nt35510_6517_lcm_drv = 
{
	.name			= "nt35510_6517",
        .set_util_funcs = lcm_set_util_funcs,
        .get_params     = lcm_get_params,
        .init           = lcm_init,
        .suspend        = lcm_suspend,
        .resume         = lcm_resume,
        .set_backlight	= lcm_setbacklight,
		//.set_pwm        = lcm_setpwm,
		//.get_pwm        = lcm_getpwm,
	.compare_id    = lcm_compare_id,
        .update         = lcm_update

};
