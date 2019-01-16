#ifdef BUILD_LK
#else
#include <linux/string.h>
#endif

#include "lcm_drv.h"
//yufeng
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

#define FRAME_WIDTH  										(540)
#define FRAME_HEIGHT 										(960)

#define REGFLAG_DELAY             							0XFE
#define REGFLAG_END_OF_TABLE      							0xFF   // END OF REGISTERS MARKER

#define LCM_DSI_CMD_MODE									1

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
#define read_reg											lcm_util.dsi_read_reg()

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
	params->dbi.te_mode 				= LCM_DBI_TE_MODE_VSYNC_ONLY;
	params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode   = CMD_MODE;
#else
	params->dsi.mode   = SYNC_PULSE_VDO_MODE;
#endif

	// DSI
	/* Command mode setting */
	params->dsi.LANE_NUM				= LCM_THREE_LANE;
	//The following defined the fomat for data coming from LCD engine.
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;
	// Highly depends on LCD driver capability.
	// Not support in MT6573
	params->dsi.packet_size=256;

	// Video mode setting
	params->dsi.intermediat_buffer_num = 2;
	params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.word_count=540*3;
	params->dsi.vertical_sync_active=3;
	params->dsi.vertical_backporch=12;
	params->dsi.vertical_frontporch=2;
	params->dsi.vertical_active_line=960;

	params->dsi.line_byte=2048;		// 2256 = 752*3
	params->dsi.horizontal_sync_active_byte=26;
	params->dsi.horizontal_backporch_byte=146;
	params->dsi.horizontal_frontporch_byte=146;
	params->dsi.rgb_byte=(540*3+6);

	params->dsi.horizontal_sync_active_word_count=20;
	params->dsi.horizontal_backporch_word_count=140;
	params->dsi.horizontal_frontporch_word_count=140;

	// Bit rate calculation
	params->dsi.pll_div1=0;		// 
	params->dsi.pll_div2=1;			// 
	params->dsi.fbk_div=20;
}

#if 0
static void lcm_init(void)
{
	unsigned int data_array[16];    
	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);
	SET_RESET_PIN(1);
	MDELAY(10);

	data_array[0]=0x00110500;
	dsi_set_cmdq(&data_array,1,1);
	MDELAY(100);
	data_array[0]=0x00063902;
	data_array[1]=0x52AA55F0;
	data_array[2]=0x00000008;
	dsi_set_cmdq(&data_array,3,1);
	MDELAY(1);


	data_array[0]=0xecb11500;
	dsi_set_cmdq(&data_array,1,1);
	MDELAY(1);


	data_array[0]=0x00043902;
	data_array[1]=0x020202bc;	
	dsi_set_cmdq(&data_array,2,1);
	MDELAY(1);


	data_array[0]=0x00063902;
	data_array[1]=0x52aa55f0;
	data_array[2]=0x00000108;
	dsi_set_cmdq(&data_array,3,1);
	MDELAY(1);


	data_array[0]=0x51be1500;
	dsi_set_cmdq(&data_array,1,1);
	MDELAY(1);


	data_array[0]=0x00053902;
	data_array[1]=0x100f0fd0;
	data_array[2]=0x00000010;//0x00001010; yufeng
	dsi_set_cmdq(&data_array,3,1);
	MDELAY(1);

	data_array[0]=0x00043902;
	data_array[1]=0x007b00bc;
	dsi_set_cmdq(&data_array,2,1);
	MDELAY(1);


	data_array[0]=0x00043902;
	data_array[1]=0x007b00bd;
	dsi_set_cmdq(&data_array,2,1);
	MDELAY(1);

	data_array[0]=0x00053902;
	data_array[1]=0x03ba03eb;
	data_array[2]=0x000000c1;
	dsi_set_cmdq(&data_array,3,1);	
	MDELAY(1);

	data_array[0]=0x00043902;
	data_array[1]=0x101010b3;
	dsi_set_cmdq(&data_array,2,1);	
	MDELAY(1);

	data_array[0]=0x00043902;
	data_array[1]=0x0a0a0ab4;
	dsi_set_cmdq(&data_array,2,1);
	MDELAY(1);

	data_array[0]=0x00043902;
	data_array[1]=0x111111b2;
	dsi_set_cmdq(&data_array,2,1);
	MDELAY(1);

	data_array[0]=0x00291500;
	dsi_set_cmdq(&data_array,1,1);
	MDELAY(1);

	data_array[0]=0x24531500;
	dsi_set_cmdq(&data_array,1,1);
	MDELAY(1);


	//lcm_util.set_gpio_mode(GPIO129, GPIO_MODE_00);
	//lcm_util.set_gpio_dir(GPIO129, GPIO_DIR_OUT);
	//lcm_util.set_gpio_out(GPIO129, 1);
}
#endif

static void lcm_init(void)
{
	unsigned int data_array[16];    
	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);
	SET_RESET_PIN(1);
	MDELAY(10);

	data_array[0]=0x00063902;
	data_array[1]=0x2555aaff;
	data_array[2]=0x00000101;
	dsi_set_cmdq(&data_array,3,1);

	data_array[0]=0x00363902;
	data_array[1]=0x4A0000F2;
	data_array[2]=0x0000A80A;
	data_array[3]=0x00000000;
	data_array[4]=0x00000000;
	data_array[5]=0x000B0000;
	data_array[6]=0x00000000;
	data_array[7]=0x00000000;
	data_array[8]=0x51014000;
	data_array[9]=0x01000100;
	dsi_set_cmdq(&data_array,10,1);

	data_array[0]=0x00083902;
	data_array[1]=0x070302F3;
	data_array[2]=0x0DD18845;
	dsi_set_cmdq(&data_array,3,1);

	data_array[0]=0x00063902;
	data_array[1]=0x52AA55F0;
	data_array[2]=0x00000008;
	dsi_set_cmdq(&data_array,3,1);

	data_array[0]=0x00043902;
	data_array[1]=0x0000CCB1;
	dsi_set_cmdq(&data_array,2,1);

	data_array[0]=0x01B61500;
	dsi_set_cmdq(&data_array,1,1);

	data_array[0]=0x00023902;
	data_array[1]=0x007272B7;
	dsi_set_cmdq(&data_array,2,1);

	data_array[0]=0x00053902;
	data_array[1]=0x010101B8;
	data_array[2]=0x00000001;
	dsi_set_cmdq(&data_array,3,1);

	data_array[0]=0x53BB1500;
	dsi_set_cmdq(&data_array,1,1);

	data_array[0]=0x00043902;
	data_array[1]=0x040404BC;
	data_array[2]=0x00000004;
	dsi_set_cmdq(&data_array,3,1);

	data_array[0]=0x00063902;
	data_array[1]=0x109301BD;
	data_array[2]=0x00000120;
	dsi_set_cmdq(&data_array,3,1);

	data_array[0]=0x00073902;
	data_array[1]=0x0D0661C9;
	data_array[2]=0x00001717;
	dsi_set_cmdq(&data_array,3,1);

	data_array[0]=0x00063902;
	data_array[1]=0x52AA55F0;
	data_array[2]=0x00000108;
	dsi_set_cmdq(&data_array,3,1);

	data_array[0]=0x00043902;
	data_array[1]=0x0C0C0CB0;
	dsi_set_cmdq(&data_array,2,1);

	data_array[0]=0x00043902;
	data_array[1]=0x0C0C0CB1;
	dsi_set_cmdq(&data_array,2,1);

	data_array[0]=0x00043902;
	data_array[1]=0x020202B2;
	dsi_set_cmdq(&data_array,2,1);

	data_array[0]=0x00043902;
	data_array[1]=0x101010B3;
	dsi_set_cmdq(&data_array,2,1);

	data_array[0]=0x00043902;
	data_array[1]=0x060606B4;
	dsi_set_cmdq(&data_array,2,1);

	data_array[0]=0x00043902;
	data_array[1]=0x545454B6;
	dsi_set_cmdq(&data_array,2,1);

	data_array[0]=0x00043902;
	data_array[1]=0x242424B7;
	dsi_set_cmdq(&data_array,2,1);

	data_array[0]=0x00043902;
	data_array[1]=0x303030B8;
	dsi_set_cmdq(&data_array,2,1);

	data_array[0]=0x00043902;
	data_array[1]=0x343434B9;
	dsi_set_cmdq(&data_array,2,1);

	data_array[0]=0x00043902;
	data_array[1]=0x242424BA;
	dsi_set_cmdq(&data_array,2,1);

	data_array[0]=0x00043902;
	data_array[1]=0x009800BC;
	dsi_set_cmdq(&data_array,2,1);

	data_array[0]=0x00043902;
	data_array[1]=0x009800BD;
	dsi_set_cmdq(&data_array,2,1);

	data_array[0]=0x57BE1500;
	dsi_set_cmdq(&data_array,1,1);

	data_array[0]=0x00C21500;
	dsi_set_cmdq(&data_array,1,1);

	data_array[0]=0x00053902;
	data_array[1]=0x100F0FD0;
	data_array[2]=0x00000010;
	dsi_set_cmdq(&data_array,3,1);

//#Gamma Setting
	data_array[0]=0x00173902;
	data_array[1]=0x002300D1;
	data_array[2]=0x00310024;
	data_array[3]=0x00720052;
	data_array[4]=0x01DE00AE;
	data_array[5]=0x00000024;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00173902;
	data_array[1]=0x015401D2;
	data_array[2]=0x02C8019F;
	data_array[3]=0x02350208;
	data_array[4]=0x025E0236;
	data_array[5]=0x00000083;
	dsi_set_cmdq(&data_array,6,1);
	
	data_array[0]=0x00173902;
	data_array[1]=0x029802D3;//yufeng
	data_array[2]=0x02C002AF;
	data_array[3]=0x02D802D1;
	data_array[4]=0x03F802F3;
	data_array[5]=0x00000000;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00053902;
	data_array[1]=0x031C03D4;
	data_array[2]=0x00000052;
	dsi_set_cmdq(&data_array,3,1);

	data_array[0]=0x00173902;
	data_array[1]=0x002300D5;
	data_array[2]=0x00310024;
	data_array[3]=0x00720052;
	data_array[4]=0x01DE00AE;
	data_array[5]=0x00000024;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00173902;
	data_array[1]=0x015401D6;
	data_array[2]=0x02C8019F;
	data_array[3]=0x02350208;
	data_array[4]=0x025E0236;
	data_array[5]=0x00000083;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00173902;
	data_array[1]=0x029802D7;
	data_array[2]=0x02C002AF;
	data_array[3]=0x02D802D1;
	data_array[4]=0x03F802F3;
	data_array[5]=0x00000000;
	dsi_set_cmdq(&data_array,6,1);
	
 	data_array[0]=0x00053902;
	data_array[1]=0x031C03D8;
	data_array[2]=0x00000052;
	dsi_set_cmdq(&data_array,3,1);

	data_array[0]=0x00173902;
	data_array[1]=0x002300D9;
	data_array[2]=0x00310024;
	data_array[3]=0x00720052;
	data_array[4]=0x01DE00AE;
	data_array[5]=0x00000024;
	dsi_set_cmdq(&data_array,6,1);

    	data_array[0]=0x00173902;
	data_array[1]=0x015401DD;
	data_array[2]=0x02C8019F;
	data_array[3]=0x02350208;
	data_array[4]=0x025E0236;
	data_array[5]=0x00000083;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00173902;
	data_array[1]=0x029802DE;
	data_array[2]=0x02C002AF;
	data_array[3]=0x02D802D1;
	data_array[4]=0x03F802F3;
	data_array[5]=0x00000000;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00053902;
	data_array[1]=0x031C03DF;
	data_array[2]=0x00000052;
	dsi_set_cmdq(&data_array,3,1);

	data_array[0]=0x00173902;
	data_array[1]=0x002300E0;
	data_array[2]=0x00310024;
	data_array[3]=0x00720052;
	data_array[4]=0x01DE00AE;
	data_array[5]=0x00000024;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00173902;
	data_array[1]=0x015401E1;
	data_array[2]=0x02C8019F;
	data_array[3]=0x02350208;
	data_array[4]=0x025E0236;
	data_array[5]=0x00000083;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00173902;
	data_array[1]=0x029802E2;
	data_array[2]=0x02C002AF;
	data_array[3]=0x02D802D1;
	data_array[4]=0x03F802F3;
	data_array[5]=0x00000000;
	dsi_set_cmdq(&data_array,6,1);

    	data_array[0]=0x00053902;
	data_array[1]=0x031C03E3;
	data_array[2]=0x00000052;
	dsi_set_cmdq(&data_array,3,1);

	data_array[0]=0x00173902;
	data_array[1]=0x002300E4;
	data_array[2]=0x00310024;
	data_array[3]=0x00720052;
	data_array[4]=0x01DE00AE;
	data_array[5]=0x00000024;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00173902;
	data_array[1]=0x015401E5;
	data_array[2]=0x02C8019F;
	data_array[3]=0x02350208;
	data_array[4]=0x025E0236;
	data_array[5]=0x00000083;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00173902;
	data_array[1]=0x029802E6;
	data_array[2]=0x02C002AF;
	data_array[3]=0x02D802D1;
	data_array[4]=0x03F802F3;
	data_array[5]=0x00000000;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00053902;
	data_array[1]=0x031C03E7;
	data_array[2]=0x00000052;
	dsi_set_cmdq(&data_array,3,1);

	data_array[0]=0x00173902;
	data_array[1]=0x002300E8;
	data_array[2]=0x00310024;
	data_array[3]=0x00720052;
	data_array[4]=0x01DE00AE;
	data_array[5]=0x00000024;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00173902;
	data_array[1]=0x015401E9;
	data_array[2]=0x02C8019F;
	data_array[3]=0x02350208;
	data_array[4]=0x025E0236;
	data_array[5]=0x00000083;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00173902;
	data_array[1]=0x029802EA;
	data_array[2]=0x02C002AF;
	data_array[3]=0x02D802D1;
	data_array[4]=0x03F802F3;
	data_array[5]=0x00000000;
	dsi_set_cmdq(&data_array,6,1);

	data_array[0]=0x00053902;
	data_array[1]=0x031C03EB;
	data_array[2]=0x00000052;
	dsi_set_cmdq(&data_array,3,1);

	data_array[0]=0x773A1500;
	dsi_set_cmdq(&data_array,1,1);

    	data_array[0]=0x00361500;
	dsi_set_cmdq(&data_array,1,1);

	data_array[0]=0x00351500;
	dsi_set_cmdq(&data_array,1,1);

	data_array[0]=0x00291500;
	dsi_set_cmdq(&data_array,1,1);
	MDELAY(40);

	data_array[0]=0x00110500;
	dsi_set_cmdq(&data_array,1,1);
	MDELAY(150);	

	//lcm_util.set_gpio_mode(GPIO129, GPIO_MODE_00);
	//lcm_util.set_gpio_dir(GPIO129, GPIO_DIR_OUT);
	//lcm_util.set_gpio_out(GPIO129, 1);
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
	dsi_set_cmdq(&data_array, 3, 1);
	//MDELAY(1);
	
	data_array[0]= 0x00053902;
	data_array[1]= (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
	data_array[2]= (y1_LSB);
	dsi_set_cmdq(&data_array, 3, 1);
	//MDELAY(1);
	
	data_array[0]= 0x00290508;
	dsi_set_cmdq(&data_array, 1, 1);
	//MDELAY(1);
	
	data_array[0]= 0x002c3909;
	dsi_set_cmdq(&data_array, 1, 0);
	//MDELAY(1);

}

static void lcm_suspend(void)
{
	unsigned int data_array[16];
	data_array[0]=0x00280500;
	dsi_set_cmdq(&data_array,1,1);
	MDELAY(10);
	data_array[0]=0x00100500;
	dsi_set_cmdq(&data_array,1,1);
	MDELAY(100);
}


static void lcm_resume(void)
{
	unsigned int data_array[16];
	data_array[0]=0x00110500;
	dsi_set_cmdq(&data_array,1,1);
	MDELAY(100);
	data_array[0]=0x00290500;
	dsi_set_cmdq(&data_array,1,1);
	MDELAY(10);
}


static unsigned int lcm_compare_id(void)
{
    #if 0
	unsigned int id=0;
	unsigned char buffer[2];
	unsigned int array[16];  

	//Do reset here
	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(10);
	
	SET_RESET_PIN(1);
	MDELAY(10);      

	array[0]=0x00063902;
	array[1]=0x52aa55f0;
	array[2]=0x00000108;
	dsi_set_cmdq(array, 3, 1);
	MDELAY(10);

	array[0] = 0x00083700;
	dsi_set_cmdq(array, 1, 1);

	//read_reg_v2(0x04, buffer, 3);//if read 0x04,should get 0x008000,that is both OK.
	read_reg_v2(0xc5, buffer,2);

	id = buffer[0]<<8 |buffer[1]; 
	      
    #ifdef BUILD_LK
	  printf("%s, id = 0x%x \n", __func__, id);
    #endif

    if(id == LCM_ID_NT35590)
    	return 1;
    else
    	return 0;
	#endif
	return 1;

}


LCM_DRIVER nt35516_qhd_rav4_lcm_drv = 
{
	.name		= "nt35516_qhd_rav4",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
#if (LCM_DSI_CMD_MODE)
	.compare_id    = lcm_compare_id,
	.update         = lcm_update,
#endif
};
