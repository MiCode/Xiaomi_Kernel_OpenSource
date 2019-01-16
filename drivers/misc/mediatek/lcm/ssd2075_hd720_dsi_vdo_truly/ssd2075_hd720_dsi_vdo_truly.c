#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
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
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  (720)
#define FRAME_HEIGHT (1280)

#define LCM_ID_SSD2075 (0x2075)

#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif

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

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	        lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)											lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)   


#define   LCM_DSI_CMD_MODE							0


static void init_lcm_registers(void)
{
	unsigned int data_array[16];

	
	data_array[0] = 0x00023902;                          
    data_array[1] = 0x0000A3E1;                 
    dsi_set_cmdq(data_array, 2, 1);
	//MDELAY(1);
	
	data_array[0] = 0x00023902;                          
    data_array[1] = 0x000000B3;                 
    dsi_set_cmdq(data_array, 2, 1);
	//MDELAY(1);

	
	data_array[0] = 0x00053902;                          
    data_array[1] = 0x000F16B6; 
	data_array[2] = 0x00000000; 
    dsi_set_cmdq(data_array, 3, 1); 
    //MDELAY(1);
	 
	data_array[0] = 0x00093902;                          
    data_array[1] = 0x080600B8; 
	data_array[2] = 0x23090700; 
	data_array[3] = 0x00000004; 
    dsi_set_cmdq(data_array, 4, 1);
    //MDELAY(1);
	 
	data_array[0] = 0x00073902;                          
    data_array[1] = 0x220804B9; 
	data_array[2] = 0x000FFFFF;  //0x00FFFFFF
    dsi_set_cmdq(data_array, 3, 1); 
    //MDELAY(1);
	 
	data_array[0] = 0x00093902;                          
    data_array[1] = 0x100E0EBA; 
	data_array[2] = 0x0C0A0A10; 
	data_array[3] = 0x0000000C; 
    dsi_set_cmdq(data_array, 4, 1);
	//MDELAY(1);
	
	data_array[0] = 0x00093902;                          
    data_array[1] = 0xA1A1A1BB; 
	data_array[2] = 0xA1A1A1A1; 
	data_array[3] = 0x000000A1; 
    dsi_set_cmdq(data_array, 4, 1);
	//MDELAY(1);

	
	data_array[0] = 0x00093902;                          
    data_array[1] = 0x000000BC; 
	data_array[2] = 0x00000000; 
	data_array[3] = 0x00000000; 
    dsi_set_cmdq(data_array, 4, 1);
	//MDELAY(1);

	data_array[0] = 0x00093902;                          
    data_array[1] = 0x110F0FBD; 
	data_array[2] = 0x0D0B0B11; 
	data_array[3] = 0x0000000D; 
    dsi_set_cmdq(data_array, 4, 1);
	//MDELAY(1);

	 
	data_array[0] = 0x00093902;                          
    data_array[1] = 0xA1A1A1BE; 
	data_array[2] = 0xA1A1A1A1; 
	data_array[3] = 0x000000A1; 
    dsi_set_cmdq(data_array, 4, 1);
	//MDELAY(1);

	
	data_array[0] = 0x00093902;                          
    data_array[1] = 0x000000BF; 
	data_array[2] = 0x00000000; 
	data_array[3] = 0x00000000; 
    dsi_set_cmdq(data_array, 4, 1);
	//MDELAY(1);

	
	data_array[0] = 0x00043902;                          
    data_array[1] = 0x121E16B1;  //modified  14+3             
    dsi_set_cmdq(data_array, 2, 1); 
	//MDELAY(1);

	
	data_array[0] = 0x00063902;                          
    data_array[1] = 0x020301E0; //VSA
	data_array[2] = 0x00000100; 
    dsi_set_cmdq(data_array, 3, 1); 
	//MDELAY(1);

	
	data_array[0] = 0x00073902;                          
    data_array[1] = 0x100000D0; 
	data_array[2] = 0x002E221E; 
    dsi_set_cmdq(data_array, 3, 1); 
	//MDELAY(1);

	
	data_array[0] = 0x00063902;                          
    data_array[1] = 0x232B26D1; 
	data_array[2] = 0x00000A1B; 
    dsi_set_cmdq(data_array, 3, 1);
	//MDELAY(1);

	
	data_array[0] = 0x00073902;                          
    data_array[1] = 0x100000D2; 
	data_array[2] = 0x002E221E; 
    dsi_set_cmdq(data_array, 3, 1); 
	//MDELAY(1);

	
	data_array[0] = 0x00063902;                          
    data_array[1] = 0x232B26D3; 
	data_array[2] = 0x00000A1B; 
    dsi_set_cmdq(data_array, 3, 1);
	//MDELAY(1);

	
	data_array[0] = 0x00073902;                          
    data_array[1] = 0x100000D4; 
	data_array[2] = 0x002E221E; 
    dsi_set_cmdq(data_array, 3, 1); 
	//MDELAY(1);

	
	data_array[0] = 0x00063902;                          
    data_array[1] = 0x232B26D5; 
	data_array[2] = 0x00000A1B; 
    dsi_set_cmdq(data_array, 3, 1);  
	//MDELAY(1);

	data_array[0] = 0x00073902;                          
    data_array[1] = 0x100000D6; 
	data_array[2] = 0x002E221E; 
    dsi_set_cmdq(data_array, 3, 1);
	//MDELAY(1);

	data_array[0] = 0x00063902;                          
    data_array[1] = 0x232B26D7; 
	data_array[2] = 0x00000A1B; 
    dsi_set_cmdq(data_array, 3, 1); 
	//MDELAY(1);

	data_array[0] = 0x00073902;                          
    data_array[1] = 0x100000D8; 
	data_array[2] = 0x002E221E; 
    dsi_set_cmdq(data_array, 3, 1); 
	//MDELAY(1);

	data_array[0] = 0x00063902;                          
    data_array[1] = 0x232B26D9; 
	data_array[2] = 0x00000A1B; 
    dsi_set_cmdq(data_array, 3, 1); 
	//MDELAY(1);

	data_array[0] = 0x00073902;                          
    data_array[1] = 0x100000DA; 
	data_array[2] = 0x002E221E; 
    dsi_set_cmdq(data_array, 3, 1); 
	//MDELAY(1);
 
	data_array[0] = 0x00063902;                          
    data_array[1] = 0x232B26DB; 
	data_array[2] = 0x00000A1B; 
    dsi_set_cmdq(data_array, 3, 1); 
	//MDELAY(1);

	data_array[0] = 0x00053902;                          
    data_array[1] = 0xFF00D870; 
	data_array[2] = 0x00000080; 
    dsi_set_cmdq(data_array, 3, 1); 
	//MDELAY(1);

	data_array[0] = 0x00023902;                          
    data_array[1] = 0x000001FF;                 
    dsi_set_cmdq(data_array, 2, 1); 
	MDELAY(1);

// add cmd-c6 
	data_array[0] = 0x00033902; 						 
	data_array[1] = 0x003399C6; 				
	dsi_set_cmdq(data_array, 2, 1); 
	MDELAY(1);
//add end

//legen modify 
	data_array[0] = 0x00033902;                          
    data_array[1] = 0x00309DDE; //00309DDE
	//data_array[2] = 0x00130D0C; 
    dsi_set_cmdq(data_array, 2, 1);


	data_array[0] = 0x00023902;                          
    data_array[1] = 0x00000014; 
	//data_array[2] = 0x00130D0C; 
    dsi_set_cmdq(data_array, 2, 1);
//legen modify end


	data_array[0] = 0x00023902;                          
    data_array[1] = 0x000007E9;                 
    dsi_set_cmdq(data_array, 2, 1); 
	MDELAY(1);

	data_array[0] = 0x00033902;                          
    data_array[1] = 0x001060ED;                 
    dsi_set_cmdq(data_array, 2, 1); 
	//MDELAY(1);

	data_array[0] = 0x00023902;                          
    data_array[1] = 0x000012EC;                 
    dsi_set_cmdq(data_array, 2, 1); 
	//MDELAY(1);
 
	data_array[0] = 0x00053902;                          
    data_array[1] = 0x347B77CD; 
	data_array[2] = 0x00000008; 
    dsi_set_cmdq(data_array, 3, 1);
	//MDELAY(1);
 
	data_array[0] = 0x00083902;                          
    data_array[1] = 0x340703C3; 
	data_array[2] = 0x54440105; 
    dsi_set_cmdq(data_array, 3, 1);
	//MDELAY(1);

	data_array[0] = 0x00063902; 
    data_array[1] = 0x700302C4;  //0x701303c4
	data_array[2] = 0x00005A70;  //0x00005C70
    dsi_set_cmdq(data_array, 3, 1);
	//MDELAY(1);

    #if 0
    //no need to config 0xDE register
    data_array[0] = 0x00073902;                          
    data_array[1] = 0xD4CF95DE; 
	data_array[2] = 0x00100F10; 
    dsi_set_cmdq(&data_array, 3, 1);
	#endif
    
	data_array[0] = 0x00043902;                          
    data_array[1] = 0x0080DACB;                 
    dsi_set_cmdq(data_array, 2, 1); 
	//MDELAY(1);

	data_array[0] = 0x00033902;                          
    data_array[1] = 0x002815EA;                 
    dsi_set_cmdq(data_array, 2, 1); 
	//MDELAY(1);
 
	data_array[0] = 0x00053902;                          
    data_array[1] = 0x000038F0; 
	data_array[2] = 0x00000000; 
    dsi_set_cmdq(data_array, 3, 1);
	//MDELAY(1);

	data_array[0] = 0x00043902;                          
    data_array[1] = 0x820060C9;                 
    dsi_set_cmdq(data_array, 2, 1);
	//MDELAY(1);

	data_array[0] = 0x00093902;                          
    data_array[1] = 0x050500B5;
	data_array[2] = 0x2040041E;                          
    data_array[3] = 0x000000FC;                 
    dsi_set_cmdq(data_array, 4, 1);
	//MDELAY(1);

	data_array[0] = 0x00023902;                          
    data_array[1] = 0x00000836;                 
    dsi_set_cmdq(data_array, 2, 1);
    MDELAY(1);//wait for PLL to lock 

    //1 Do not delete 0x11, 0x29 here
	data_array[0] = 0x00110500; // Sleep Out
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(120);
	
	data_array[0] = 0x00290500; // Display On
	dsi_set_cmdq(data_array, 1, 1); 
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

       //1 SSD2075 has no TE Pin
		// enable tearing-free
		params->dbi.te_mode 				= LCM_DBI_TE_MODE_DISABLED;
		params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;

        #if (LCM_DSI_CMD_MODE)
		params->dsi.mode   = CMD_MODE;
        #else
		//params->dsi.mode   = SYNC_PULSE_VDO_MODE;
		params->dsi.mode   = BURST_VDO_MODE;
		//params->dsi.mode   = SYNC_EVENT_VDO_MODE; 
		
        #endif
	
		// DSI
		/* Command mode setting */
		//1 Three lane or Four lane
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
		params->dsi.intermediat_buffer_num = 0;//because DSI/DPI HW design change, this parameters should be 0 when video mode in MT658X; or memory leakage

		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
		params->dsi.word_count=720*3;	

		
		params->dsi.vertical_sync_active				= 3;  //---3
		params->dsi.vertical_backporch					= 12; //---14
		params->dsi.vertical_frontporch					= 8;  //----8
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 

		params->dsi.horizontal_sync_active				= 2;  //----2
		params->dsi.horizontal_backporch				= 28; //----28
		params->dsi.horizontal_frontporch				= 50; //----50
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;


		params->dsi.HS_PRPR=3;
		params->dsi.CLK_HS_POST=22;
		params->dsi.DA_HS_EXIT=20;


		// Bit rate calculation
		//1 Every lane speed
		params->dsi.pll_div1=0;		// div1=0,1,2,3;div1_real=1,2,4,4 ----0: 546Mbps  1:273Mbps
		params->dsi.pll_div2=1;		// div2=0,1,2,3;div1_real=1,2,4,4	
		params->dsi.fbk_div =19;    // fref=26MHz, fvco=fref*(fbk_div+1)*2/(div1_real*div2_real)	

}

static void lcm_init(void)
{

	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);
	
	SET_RESET_PIN(1);
	MDELAY(20);      

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
	MDELAY(1);
	SET_RESET_PIN(1);
}


static void lcm_resume(void)
{
   //1 do lcm init again to solve some display issue
	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);
	
	SET_RESET_PIN(1);
	MDELAY(20);      

	init_lcm_registers();

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

	data_array[0]= 0x00290508; //HW bug, so need send one HS packet
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0]= 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);

}
#endif

static unsigned int lcm_compare_id(void)
{


    unsigned int id0,id1,id=0;
	unsigned char buffer[2];
	unsigned int array[16];  

    SET_RESET_PIN(1);
    SET_RESET_PIN(0);
    MDELAY(1);
    SET_RESET_PIN(1);
    MDELAY(10);

	array[0] = 0x00023700;// return byte number
	dsi_set_cmdq(array, 1, 1);
	MDELAY(10);

	read_reg_v2(0xA1, buffer, 2);
	id0 = buffer[0]; 
	id1 = buffer[1];
	id=(id0<<8)|id1;
	
    #ifdef BUILD_LK
	printf("%s, LK ssd2075 id0 = 0x%08x\n", __func__, id0);
	printf("%s, LK ssd2075 id1 = 0x%08x\n", __func__, id1);
	printf("%s, LK ssd2075 id = 0x%08x\n", __func__, id);
   #else
	printk("%s, Kernel ssd2075 id0 = 0x%08x\n", __func__, id0);
	printk("%s, Kernel ssd2075 id1 = 0x%08x\n", __func__, id1);
	printk("%s, Kernel ssd2075 id = 0x%08x\n", __func__, id);
   #endif

  return (LCM_ID_SSD2075 == id)?1:0;


}

#if 0
static unsigned int lcm_esd_check(void)
{
#ifndef BUILD_LK
	char  buffer[3];
	int   array[4];

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
	array[0] = 0x00023700;// read id return two byte,version and id
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0xA1, buffer, 2);
	if(buffer[0]==0x20 && buffer[1] == 0x75)
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
	lcm_resume();

	return TRUE;
}
#endif

LCM_DRIVER ssd2075_hd720_dsi_vdo_truly_lcm_drv = 
{
    .name			= "ssd2075_hd720_dsi_vdo_truly",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id    = lcm_compare_id,
//	.esd_check = lcm_esd_check,
//	.esd_recover = lcm_esd_recover,
    #if (LCM_DSI_CMD_MODE)
    .update         = lcm_update,
    #endif
    };
