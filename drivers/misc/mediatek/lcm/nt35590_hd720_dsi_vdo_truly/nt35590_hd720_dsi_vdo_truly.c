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

#define LCM_ID_NT35590 (0x90)

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

#if defined(MTK_WFD_SUPPORT)
#define   LCM_DSI_CMD_MODE							1
#else
#define   LCM_DSI_CMD_MODE							0
#endif

#if 0
static void init_lcm_registers(void)
{
	unsigned int data_array[16];

	data_array[0] = 0x00023902;                          
    data_array[1] = 0x00009036;                 
    dsi_set_cmdq(data_array, 2, 1); 

	data_array[0] = 0x00023902;//CMD1                           
    data_array[1] = 0x000000FF;                 
    dsi_set_cmdq(data_array, 2, 1);     
    	
    data_array[0] = 0x00023902;//03 4lane  02 3lanes               
    data_array[1] = 0x000002BA;                 
    dsi_set_cmdq(data_array, 2, 1);    
    	
    data_array[0] = 0x00023902;//03 Video 08 command
    #if (LCM_DSI_CMD_MODE)
		data_array[1] = 0x000008C2; 
    #else
		data_array[1] = 0x000003C2; 
    #endif                
    dsi_set_cmdq(data_array, 2, 1);   
    	
    data_array[0] = 0x00023902;//CMD2,Page0  
    data_array[1] = 0x000001FF;                 
    dsi_set_cmdq(data_array, 2, 1);   
    	
    data_array[0] = 0x00023902;//720*1280 
    data_array[1] = 0x00003A00;                 
    dsi_set_cmdq(data_array, 2, 1);  
    	
    data_array[0] = 0x00023902;
    data_array[1] = 0x00003301; //4401                
    dsi_set_cmdq(data_array, 2, 1);  
    	
    data_array[0] = 0x00023902;
    data_array[1] = 0x00005302; //5402               
    dsi_set_cmdq(data_array, 2, 1); 
	
	data_array[0] = 0x00023902;//VGL=-6V 
    data_array[1] = 0x00008509; //0309                
    dsi_set_cmdq(data_array, 2, 1);  
    	
    data_array[0] = 0x00023902;//VGH=+8.6V 
    data_array[1] = 0x0000250E;                 
    dsi_set_cmdq(data_array, 2, 1);  
    	
    data_array[0] = 0x00023902;//turn off VGLO regulator   
    data_array[1] = 0x00000A0F;                 
    dsi_set_cmdq(data_array, 2, 1);  
    	
    data_array[0] = 0x00023902;//GVDDP=4V     
    data_array[1] = 0x0000970B;                 
    dsi_set_cmdq(data_array, 2, 1);  
    	
    data_array[0] = 0x00023902;
    data_array[1] = 0x0000970C;                 
    dsi_set_cmdq(data_array, 2, 1);  

    data_array[0] = 0x00023902; 
    data_array[1] = 0x00008611; //8611                
    dsi_set_cmdq(data_array, 2, 1); 

	data_array[0] = 0x00023902;//VCOMDC 
    data_array[1] = 0x00000312;                 
    dsi_set_cmdq(data_array, 2, 1); 
    	
    data_array[0] = 0x00023902;  
    data_array[1] = 0x00007B36;                 
    dsi_set_cmdq(data_array, 2, 1);
	
#if 1
	data_array[0] = 0x00023902;  
    data_array[1] = 0x000080B0;                 
    dsi_set_cmdq(data_array, 2, 1); 

	data_array[0] = 0x00023902;  
    data_array[1] = 0x000002B1;                 
    dsi_set_cmdq(data_array, 2, 1); 
#endif 

    data_array[0] = 0x00023902;//GVDDP=4V     
    data_array[1] = 0x00002C71;                 
    dsi_set_cmdq(data_array, 2, 1);  
#if 1
    data_array[0] = 0x00023902;
    data_array[1] = 0x000005FF;         
    dsi_set_cmdq(data_array, 2, 1);   

	data_array[0] = 0x00023902; /////////////LTPS 
    data_array[1] = 0x00000001;                   
    dsi_set_cmdq(data_array, 2, 1);              
    data_array[0] = 0x00023902;                   
    data_array[1] = 0x00008D02;                   
    dsi_set_cmdq(data_array, 2, 1);              
    data_array[0] = 0x00023902;                   
    data_array[1] = 0x00008D03;                   
    dsi_set_cmdq(data_array, 2, 1);              
    data_array[0] = 0x00023902;                   
    data_array[1] = 0x00008D04;     
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;     
    data_array[1] = 0x00003005;     
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;//06         
    data_array[1] = 0x00003306;             
    dsi_set_cmdq(data_array, 2, 1); 
	
    data_array[0] = 0x00023902;             
    data_array[1] = 0x00007707;             
    dsi_set_cmdq(data_array, 2, 1);        
    data_array[0] = 0x00023902;             
    data_array[1] = 0x00000008;        
    dsi_set_cmdq(data_array, 2, 1);   
    data_array[0] = 0x00023902;        
    data_array[1] = 0x00000009;        
    dsi_set_cmdq(data_array, 2, 1);   
    data_array[0] = 0x00023902;        
    data_array[1] = 0x0000000A;        
    dsi_set_cmdq(data_array, 2, 1);   
    data_array[0] = 0x00023902;        
    data_array[1] = 0x0000800B;     
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;//0C 
    data_array[1] = 0x0000C80C;     
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; //0D
    data_array[1] = 0x0000000D;     
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;     
    data_array[1] = 0x00001B0E; 
	
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;     
    data_array[1] = 0x0000070F;     
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;     
    data_array[1] = 0x00005710;     
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;     
    data_array[1] = 0x00000011;     
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;//12 
    data_array[1] = 0x00000012;     
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;            
    data_array[1] = 0x00001E13;            
    dsi_set_cmdq(data_array, 2, 1);       
    data_array[0] = 0x00023902;            
    data_array[1] = 0x00000014;            
    dsi_set_cmdq(data_array, 2, 1);       
    data_array[0] = 0x00023902;            
    data_array[1] = 0x00001A15;            
    dsi_set_cmdq(data_array, 2, 1);       
    data_array[0] = 0x00023902;            
    data_array[1] = 0x00000516;            
    dsi_set_cmdq(data_array, 2, 1); 
	
    data_array[0] = 0x00023902;            
    data_array[1] = 0x00000017;             
    dsi_set_cmdq(data_array, 2, 1);     
    data_array[0] = 0x00023902;//12 
    data_array[1] = 0x00001E18;     
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;            
    data_array[1] = 0x0000FF19;            
    dsi_set_cmdq(data_array, 2, 1);       
    data_array[0] = 0x00023902;            
    data_array[1] = 0x0000001A;            
    dsi_set_cmdq(data_array, 2, 1);       
    data_array[0] = 0x00023902;            
    data_array[1] = 0x0000FC1B;            
    dsi_set_cmdq(data_array, 2, 1);       
    data_array[0] = 0x00023902;            
    data_array[1] = 0x0000801C;            
    dsi_set_cmdq(data_array, 2, 1);       
    data_array[0] = 0x00023902;            
    data_array[1] = 0x0000001D; //101D            
    dsi_set_cmdq(data_array, 2, 1);     
    data_array[0] = 0x00023902;
	data_array[1] = 0x0000101E; //011E            
	dsi_set_cmdq(data_array, 2, 1);     
			                                     
	data_array[0] = 0x00023902;          
    data_array[1] = 0x0000771F;          
    dsi_set_cmdq(data_array, 2, 1);  
	data_array[0] = 0x00023902;                                   
    data_array[1] = 0x00000020;          
    dsi_set_cmdq(data_array, 2, 1);     
    data_array[0] = 0x00023902;          
    data_array[1] = 0x00000221;         
    dsi_set_cmdq(data_array, 2, 1);     
    data_array[0] = 0x00023902;          
    data_array[1] = 0x00000022; //5522          
    dsi_set_cmdq(data_array, 2, 1);      
    data_array[0] = 0x00023902;            
    data_array[1] = 0x00000D23;            
    dsi_set_cmdq(data_array, 2, 1);        
    data_array[0] = 0x00023902;//06 
    data_array[1] = 0x0000A031;     
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;     
    data_array[1] = 0x00000032;     
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;     
    data_array[1] = 0x0000B833;         
    dsi_set_cmdq(data_array, 2, 1);
	
    data_array[0] = 0x00023902;            
    data_array[1] = 0x0000BB34;            
    dsi_set_cmdq(data_array, 2, 1);        
    data_array[0] = 0x00023902;             
    data_array[1] = 0x00001135;             
    dsi_set_cmdq(data_array, 2, 1);        
    data_array[0] = 0x00023902;             
    data_array[1] = 0x00000136;             
    dsi_set_cmdq(data_array, 2, 1);        
    data_array[0] = 0x00023902;//0C         
    data_array[1] = 0x00000B37;             
    dsi_set_cmdq(data_array, 2, 1);        
    data_array[0] = 0x00023902; //0D        
    data_array[1] = 0x00000138;             
    dsi_set_cmdq(data_array, 2, 1);        
    data_array[0] = 0x00023902;             
    data_array[1] = 0x00000B39;             
    dsi_set_cmdq(data_array, 2, 1); 	
    data_array[0] = 0x00023902;             
    data_array[1] = 0x00000844;             
    dsi_set_cmdq(data_array, 2, 1);        
    data_array[0] = 0x00023902;             
    data_array[1] = 0x00008045;             
    dsi_set_cmdq(data_array, 2, 1); 
	
    data_array[0] = 0x00023902;                
    data_array[1] = 0x0000CC46;                
    dsi_set_cmdq(data_array, 2, 1);           
    data_array[0] = 0x00023902;//12            
    data_array[1] = 0x00000447;                
    dsi_set_cmdq(data_array, 2, 1);           
    data_array[0] = 0x00023902;                          
    data_array[1] = 0x00000048;                          
    dsi_set_cmdq(data_array, 2, 1);                     
    data_array[0] = 0x00023902;                          
    data_array[1] = 0x00000049;                                 
    dsi_set_cmdq(data_array, 2, 1);                            
    data_array[0] = 0x00023902;                                 
    data_array[1] = 0x0000014A;                                 
    dsi_set_cmdq(data_array, 2, 1);  
	data_array[0] = 0x00023902;                                 
    data_array[1] = 0x0000036C;                                 
    dsi_set_cmdq(data_array, 2, 1);                            
    data_array[0] = 0x00023902;                                 
    data_array[1] = 0x0000036D;                                 
    dsi_set_cmdq(data_array, 2, 1);                            
    data_array[0] = 0x00023902;//18                             
    data_array[1] = 0x00002F6E;                                 
	dsi_set_cmdq(data_array, 2, 1); 		
			
    data_array[0] = 0x00023902; ////
    data_array[1] = 0x00000043;     
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;     
    data_array[1] = 0x0000234B;     
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;     
    data_array[1] = 0x0000014C;     
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;      
    data_array[1] = 0x00002350;      
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;      
    data_array[1] = 0x00000151;      
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;//06  
    data_array[1] = 0x00002358;      
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;      
    data_array[1] = 0x00000159;     
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;     
    data_array[1] = 0x0000235D;     
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;     
    data_array[1] = 0x0000015E;     
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;     
    data_array[1] = 0x00002362;     
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;     
    data_array[1] = 0x00000163;     
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;//0C 
    data_array[1] = 0x00002367;       
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; //0D
    data_array[1] = 0x00000168;     
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;     
    data_array[1] = 0x00000089;     
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;     
    data_array[1] = 0x0000018D;     
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;     
    data_array[1] = 0x0000648E;
    dsi_set_cmdq(data_array, 2, 1);
	
    data_array[0] = 0x00023902;                       
    data_array[1] = 0x0000208F;                       
    dsi_set_cmdq(data_array, 2, 1); 	
	data_array[0] = 0x00023902;//12                   
    data_array[1] = 0x00008E97;                       
    dsi_set_cmdq(data_array, 2, 1);                  
    data_array[0] = 0x00023902;                                 
    data_array[1] = 0x00008C82;                                 
    dsi_set_cmdq(data_array, 2, 1);                            
    data_array[0] = 0x00023902;                                 
    data_array[1] = 0x00000283;                                 
    dsi_set_cmdq(data_array, 2, 1);                            
    data_array[0] = 0x00023902;                                 
    data_array[1] = 0x00000ABB;                                 
    dsi_set_cmdq(data_array, 2, 1);                            
    data_array[0] = 0x00023902;                                 
    data_array[1] = 0x00000ABC; // 02BC                                
    dsi_set_cmdq(data_array, 2, 1);                            
    data_array[0] = 0x00023902;                                 
    data_array[1] = 0x00002524;                                 
    dsi_set_cmdq(data_array, 2, 1);                            
    data_array[0] = 0x00023902;//18                             
    data_array[1] = 0x00005525;                                 
	dsi_set_cmdq(data_array, 2, 1); 	
			
	data_array[0] = 0x00023902;      
    data_array[1] = 0x00000526;      
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;      
    data_array[1] = 0x00002327;      
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;      
    data_array[1] = 0x00000128;      
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;      
    data_array[1] = 0x00003129; // 0029     
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;      
    data_array[1] = 0x00005D2A;      
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;//06 
    data_array[1] = 0x0000012B;     
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;     
    data_array[1] = 0x0000002F;     
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;     
    data_array[1] = 0x00001030; 
    dsi_set_cmdq(data_array, 2, 1); 
	
    data_array[0] = 0x00023902;             
    data_array[1] = 0x000012A7;             
    dsi_set_cmdq(data_array, 2, 1);        
    data_array[0] = 0x00023902;             
    data_array[1] = 0x0000032D;             
    dsi_set_cmdq(data_array, 2, 1);
#endif

    data_array[0] = 0x00023902;////CMD1 
    data_array[1] = 0x000000FF;                 
    dsi_set_cmdq(data_array, 2, 1);    
    data_array[0] = 0x00023902;
    data_array[1] = 0x000001FB;                 
    dsi_set_cmdq(data_array, 2, 1);    
    data_array[0] = 0x00023902;//CMD2,Page0 
    data_array[1] = 0x000001FF;                 
    dsi_set_cmdq(data_array, 2, 1);       
    data_array[0] = 0x00023902;
    data_array[1] = 0x000001FB;                 
    dsi_set_cmdq(data_array, 2, 1);   
    data_array[0] = 0x00023902;//CMD2,Page1 
    data_array[1] = 0x000002FF;                 
    dsi_set_cmdq(data_array, 2, 1);       	
    data_array[0] = 0x00023902;
    data_array[1] = 0x000001FB;                 
    dsi_set_cmdq(data_array, 2, 1);       
    	
    data_array[0] = 0x00023902;//CMD2,Page2 
    data_array[1] = 0x000003FF;                 
    dsi_set_cmdq(data_array, 2, 1);       	
    data_array[0] = 0x00023902;
    data_array[1] = 0x000001FB;                 
    dsi_set_cmdq(data_array, 2, 1);     
    data_array[0] = 0x00023902;//CMD2,Page3
    data_array[1] = 0x000004FF;         
    dsi_set_cmdq(data_array, 2, 1);                                        
    data_array[0] = 0x00023902;         
    data_array[1] = 0x000001FB;         
    dsi_set_cmdq(data_array, 2, 1);    
    data_array[0] = 0x00023902;//CMD2,Page4
    data_array[1] = 0x000005FF;         
    dsi_set_cmdq(data_array, 2, 1);    
    data_array[0] = 0x00023902;         
    data_array[1] = 0x000001FB;         
    dsi_set_cmdq(data_array, 2, 1);
	data_array[0] = 0x00023902;     ////CMD1     
    data_array[1] = 0x000000FF;         
    dsi_set_cmdq(data_array, 2, 1); 

	/*******debug-----start********/
	data_array[0] = 0x00110500;                
    dsi_set_cmdq(data_array, 1, 1); 
    MDELAY(120); 
    	
    data_array[0] = 0x00023902;//not open CABC    
    data_array[1] = 0x0000FF51;         
    dsi_set_cmdq(data_array, 2, 1);    
    	                                    
    data_array[0] = 0x00023902;         
    data_array[1] = 0x00002C53;         
    dsi_set_cmdq(data_array, 2, 1); 
    	
    data_array[0] = 0x00023902;     
    data_array[1] = 0x00000055;         
    dsi_set_cmdq(data_array, 2, 1);  
    	
    data_array[0] = 0x00290500;                
    dsi_set_cmdq(data_array, 1, 1); 
    	
    data_array[0] = 0x00023902;         
    data_array[1] = 0x000000FF;         
    dsi_set_cmdq(data_array, 2, 1); 
    	
    data_array[0] = 0x00023902;     
    data_array[1] = 0x00000035;         
    dsi_set_cmdq(data_array, 2, 1); 
	
	data_array[0] = 0x00033902;
	data_array[1] = (((FRAME_HEIGHT/2)&0xFF) << 16) | (((FRAME_HEIGHT/2)>>8) << 8) | 0x44;
	dsi_set_cmdq(data_array, 2, 1);
	/*******debug-----end********/

}
#endif
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
		params->dsi.LANE_NUM				= LCM_THREE_LANE;
		//The following defined the fomat for data coming from LCD engine.
		params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

		// Video mode setting		
		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
		
		params->dsi.vertical_sync_active				= 1;// 3    2
		params->dsi.vertical_backporch					= 1;// 20   1
		params->dsi.vertical_frontporch					= 2; // 1  12
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 

		params->dsi.horizontal_sync_active				= 2;// 50  2
		params->dsi.horizontal_backporch				= 12;
		params->dsi.horizontal_frontporch				= 80;
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

	    //params->dsi.LPX=8; 

		// Bit rate calculation
		//1 Every lane speed
		params->dsi.pll_div1=0;		// div1=0,1,2,3;div1_real=1,2,4,4 ----0: 546Mbps  1:273Mbps
		params->dsi.pll_div2=1;		// div2=0,1,2,3;div1_real=1,2,4,4	
		params->dsi.fbk_div =21;    // fref=26MHz, fvco=fref*(fbk_div+1)*2/(div1_real*div2_real)	

}

static void lcm_init(void)
{
	    unsigned int data_array[16];
		
		SET_RESET_PIN(0);
		MDELAY(20); 
		SET_RESET_PIN(1);
		MDELAY(20); 
	
		data_array[0] = 0x00023902;
		data_array[1] = 0x0000EEFF; 				
		dsi_set_cmdq(data_array, 2, 1);
		MDELAY(2); 
		data_array[0] = 0x00023902;
		data_array[1] = 0x00000826; 				
		dsi_set_cmdq(data_array, 2, 1);
		MDELAY(2); 
		data_array[0] = 0x00023902;
		data_array[1] = 0x00000026; 				
		dsi_set_cmdq(data_array, 2, 1);
		MDELAY(2); 
		data_array[0] = 0x00023902;
		data_array[1] = 0x000000FF; 				
		dsi_set_cmdq(data_array, 2, 1);
		
		SET_RESET_PIN(0);
		MDELAY(20); 
		SET_RESET_PIN(1);
		MDELAY(20); 
	
		data_array[0] = 0x00023902; 						 
		data_array[1] = 0x00009036; 				
		dsi_set_cmdq(data_array, 2, 1); 
	
		data_array[0] = 0x00023902;//CMD1							
		data_array[1] = 0x000000FF; 				
		dsi_set_cmdq(data_array, 2, 1); 	
			
		data_array[0] = 0x00023902;//03 4lane  02 3lanes			   
		data_array[1] = 0x000002BA; 				
		dsi_set_cmdq(data_array, 2, 1);    
			
		data_array[0] = 0x00023902;//03 Video 08 command
    #if (LCM_DSI_CMD_MODE)
			data_array[1] = 0x000008C2; 
    #else
			data_array[1] = 0x000003C2; 
    #endif                
		dsi_set_cmdq(data_array, 2, 1);   
			
		data_array[0] = 0x00023902;//CMD2,Page0  
		data_array[1] = 0x000001FF; 				
		dsi_set_cmdq(data_array, 2, 1);   
			
		data_array[0] = 0x00023902;//720*1280 
		data_array[1] = 0x00003A00; 				
		dsi_set_cmdq(data_array, 2, 1);  
			
		data_array[0] = 0x00023902;
		data_array[1] = 0x00003301; //4401				  
		dsi_set_cmdq(data_array, 2, 1);  
			
		data_array[0] = 0x00023902;
		data_array[1] = 0x00005302; //5402				 
		dsi_set_cmdq(data_array, 2, 1); 
		
		data_array[0] = 0x00023902;//VGL=-6V 
		data_array[1] = 0x00008509; //0309				  
		dsi_set_cmdq(data_array, 2, 1);  
			
		data_array[0] = 0x00023902;//VGH=+8.6V 
		data_array[1] = 0x0000250E; 				
		dsi_set_cmdq(data_array, 2, 1);  
			
		data_array[0] = 0x00023902;//turn off VGLO regulator   
		data_array[1] = 0x00000A0F; 				
		dsi_set_cmdq(data_array, 2, 1);  
			
		data_array[0] = 0x00023902;//GVDDP=4V	  
		data_array[1] = 0x0000970B; 				
		dsi_set_cmdq(data_array, 2, 1);  
			
		data_array[0] = 0x00023902;
		data_array[1] = 0x0000970C; 				
		dsi_set_cmdq(data_array, 2, 1);  
	
		data_array[0] = 0x00023902; 
		data_array[1] = 0x00008611; //8611				  
		dsi_set_cmdq(data_array, 2, 1); 
	
		data_array[0] = 0x00023902;//VCOMDC 
		data_array[1] = 0x00000312; 				
		dsi_set_cmdq(data_array, 2, 1); 
			
		data_array[0] = 0x00023902;  
		data_array[1] = 0x00007B36; 				
		dsi_set_cmdq(data_array, 2, 1);
		
#if 1
		data_array[0] = 0x00023902;  
		data_array[1] = 0x000080B0; 				
		dsi_set_cmdq(data_array, 2, 1); 
	
		data_array[0] = 0x00023902;  
		data_array[1] = 0x000002B1; 				
		dsi_set_cmdq(data_array, 2, 1); 
#endif 
	
		data_array[0] = 0x00023902;//GVDDP=4V	  
		data_array[1] = 0x00002C71; 				
		dsi_set_cmdq(data_array, 2, 1);  
#if 1
		data_array[0] = 0x00023902;
		data_array[1] = 0x000005FF; 		
		dsi_set_cmdq(data_array, 2, 1);   
	
		data_array[0] = 0x00023902; /////////////LTPS 
		data_array[1] = 0x00000001; 				  
		dsi_set_cmdq(data_array, 2, 1); 			 
		data_array[0] = 0x00023902; 				  
		data_array[1] = 0x00008D02; 				  
		dsi_set_cmdq(data_array, 2, 1); 			 
		data_array[0] = 0x00023902; 				  
		data_array[1] = 0x00008D03; 				  
		dsi_set_cmdq(data_array, 2, 1); 			 
		data_array[0] = 0x00023902; 				  
		data_array[1] = 0x00008D04; 	
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902; 	
		data_array[1] = 0x00003005; 	
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902;//06 		
		data_array[1] = 0x00003306; 			
		dsi_set_cmdq(data_array, 2, 1); 
		
		data_array[0] = 0x00023902; 			
		data_array[1] = 0x00007707; 			
		dsi_set_cmdq(data_array, 2, 1); 	   
		data_array[0] = 0x00023902; 			
		data_array[1] = 0x00000008; 	   
		dsi_set_cmdq(data_array, 2, 1);   
		data_array[0] = 0x00023902; 	   
		data_array[1] = 0x00000009; 	   
		dsi_set_cmdq(data_array, 2, 1);   
		data_array[0] = 0x00023902; 	   
		data_array[1] = 0x0000000A; 	   
		dsi_set_cmdq(data_array, 2, 1);   
		data_array[0] = 0x00023902; 	   
		data_array[1] = 0x0000800B; 	
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902;//0C 
		data_array[1] = 0x0000C80C; 	
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902; //0D
		data_array[1] = 0x0000000D; 	
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902; 	
		data_array[1] = 0x00001B0E; 
		
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902; 	
		data_array[1] = 0x0000070F; 	
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902; 	
		data_array[1] = 0x00005710; 	
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902; 	
		data_array[1] = 0x00000011; 	
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902;//12 
		data_array[1] = 0x00000012; 	
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902; 		   
		data_array[1] = 0x00001E13; 		   
		dsi_set_cmdq(data_array, 2, 1); 	  
		data_array[0] = 0x00023902; 		   
		data_array[1] = 0x00000014; 		   
		dsi_set_cmdq(data_array, 2, 1); 	  
		data_array[0] = 0x00023902; 		   
		data_array[1] = 0x00001A15; 		   
		dsi_set_cmdq(data_array, 2, 1); 	  
		data_array[0] = 0x00023902; 		   
		data_array[1] = 0x00000516; 		   
		dsi_set_cmdq(data_array, 2, 1); 
		
		data_array[0] = 0x00023902; 		   
		data_array[1] = 0x00000017; 			
		dsi_set_cmdq(data_array, 2, 1); 	
		data_array[0] = 0x00023902;//12 
		data_array[1] = 0x00001E18; 	
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902; 		   
		data_array[1] = 0x0000FF19; 		   
		dsi_set_cmdq(data_array, 2, 1); 	  
		data_array[0] = 0x00023902; 		   
		data_array[1] = 0x0000001A; 		   
		dsi_set_cmdq(data_array, 2, 1); 	  
		data_array[0] = 0x00023902; 		   
		data_array[1] = 0x0000FC1B; 		   
		dsi_set_cmdq(data_array, 2, 1); 	  
		data_array[0] = 0x00023902; 		   
		data_array[1] = 0x0000801C; 		   
		dsi_set_cmdq(data_array, 2, 1); 	  
		data_array[0] = 0x00023902; 		   
		data_array[1] = 0x0000001D; //101D			  
		dsi_set_cmdq(data_array, 2, 1); 	
		data_array[0] = 0x00023902;
		data_array[1] = 0x0000101E; //011E			  
		dsi_set_cmdq(data_array, 2, 1); 	
													 
		data_array[0] = 0x00023902; 		 
		data_array[1] = 0x0000771F; 		 
		dsi_set_cmdq(data_array, 2, 1);  
		data_array[0] = 0x00023902; 								  
		data_array[1] = 0x00000020; 		 
		dsi_set_cmdq(data_array, 2, 1); 	
		data_array[0] = 0x00023902; 		 
		data_array[1] = 0x00000221; 		
		dsi_set_cmdq(data_array, 2, 1); 	
		data_array[0] = 0x00023902; 		 
		data_array[1] = 0x00000022; //5522			
		dsi_set_cmdq(data_array, 2, 1); 	 
		data_array[0] = 0x00023902; 		   
		data_array[1] = 0x00000D23; 		   
		dsi_set_cmdq(data_array, 2, 1); 	   
		data_array[0] = 0x00023902;//06 
		data_array[1] = 0x0000A031; 	
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902; 	
		data_array[1] = 0x00000032; 	
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902; 	
		data_array[1] = 0x0000B833; 		
		dsi_set_cmdq(data_array, 2, 1);
		
		data_array[0] = 0x00023902; 		   
		data_array[1] = 0x0000BB34; 		   
		dsi_set_cmdq(data_array, 2, 1); 	   
		data_array[0] = 0x00023902; 			
		data_array[1] = 0x00001135; 			
		dsi_set_cmdq(data_array, 2, 1); 	   
		data_array[0] = 0x00023902; 			
		data_array[1] = 0x00000136; 			
		dsi_set_cmdq(data_array, 2, 1); 	   
		data_array[0] = 0x00023902;//0C 		
		data_array[1] = 0x00000B37; 			
		dsi_set_cmdq(data_array, 2, 1); 	   
		data_array[0] = 0x00023902; //0D		
		data_array[1] = 0x00000138; 			
		dsi_set_cmdq(data_array, 2, 1); 	   
		data_array[0] = 0x00023902; 			
		data_array[1] = 0x00000B39; 			
		dsi_set_cmdq(data_array, 2, 1); 	
		data_array[0] = 0x00023902; 			
		data_array[1] = 0x00000844; 			
		dsi_set_cmdq(data_array, 2, 1); 	   
		data_array[0] = 0x00023902; 			
		data_array[1] = 0x00008045; 			
		dsi_set_cmdq(data_array, 2, 1); 
		
		data_array[0] = 0x00023902; 			   
		data_array[1] = 0x0000CC46; 			   
		dsi_set_cmdq(data_array, 2, 1); 		  
		data_array[0] = 0x00023902;//12 		   
		data_array[1] = 0x00000447; 			   
		dsi_set_cmdq(data_array, 2, 1); 		  
		data_array[0] = 0x00023902; 						 
		data_array[1] = 0x00000048; 						 
		dsi_set_cmdq(data_array, 2, 1); 					
		data_array[0] = 0x00023902; 						 
		data_array[1] = 0x00000049; 								
		dsi_set_cmdq(data_array, 2, 1); 						   
		data_array[0] = 0x00023902; 								
		data_array[1] = 0x0000014A; 								
		dsi_set_cmdq(data_array, 2, 1);  
		data_array[0] = 0x00023902; 								
		data_array[1] = 0x0000036C; 								
		dsi_set_cmdq(data_array, 2, 1); 						   
		data_array[0] = 0x00023902; 								
		data_array[1] = 0x0000036D; 								
		dsi_set_cmdq(data_array, 2, 1); 						   
		data_array[0] = 0x00023902;//18 							
		data_array[1] = 0x00002F6E; 								
		dsi_set_cmdq(data_array, 2, 1); 		
				
		data_array[0] = 0x00023902; ////
		data_array[1] = 0x00000043; 	
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902; 	
		data_array[1] = 0x0000234B; 	
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902; 	
		data_array[1] = 0x0000014C; 	
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902; 	 
		data_array[1] = 0x00002350; 	 
		dsi_set_cmdq(data_array, 2, 1); 
		data_array[0] = 0x00023902; 	 
		data_array[1] = 0x00000151; 	 
		dsi_set_cmdq(data_array, 2, 1); 
		data_array[0] = 0x00023902;//06  
		data_array[1] = 0x00002358; 	 
		dsi_set_cmdq(data_array, 2, 1); 
		data_array[0] = 0x00023902; 	 
		data_array[1] = 0x00000159; 	
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902; 	
		data_array[1] = 0x0000235D; 	
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902; 	
		data_array[1] = 0x0000015E; 	
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902; 	
		data_array[1] = 0x00002362; 	
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902; 	
		data_array[1] = 0x00000163; 	
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902;//0C 
		data_array[1] = 0x00002367; 	  
		dsi_set_cmdq(data_array, 2, 1); 
		data_array[0] = 0x00023902; //0D
		data_array[1] = 0x00000168; 	
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902; 	
		data_array[1] = 0x00000089; 	
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902; 	
		data_array[1] = 0x0000018D; 	
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902; 	
		data_array[1] = 0x0000648E;
		dsi_set_cmdq(data_array, 2, 1);
		
		data_array[0] = 0x00023902; 					  
		data_array[1] = 0x0000208F; 					  
		dsi_set_cmdq(data_array, 2, 1); 	
		data_array[0] = 0x00023902;//12 				  
		data_array[1] = 0x00008E97; 					  
		dsi_set_cmdq(data_array, 2, 1); 				 
		data_array[0] = 0x00023902; 								
		data_array[1] = 0x00008C82; 								
		dsi_set_cmdq(data_array, 2, 1); 						   
		data_array[0] = 0x00023902; 								
		data_array[1] = 0x00000283; 								
		dsi_set_cmdq(data_array, 2, 1); 						   
		data_array[0] = 0x00023902; 								
		data_array[1] = 0x00000ABB; 								
		dsi_set_cmdq(data_array, 2, 1); 						   
		data_array[0] = 0x00023902; 								
		data_array[1] = 0x00000ABC; // 02BC 							   
		dsi_set_cmdq(data_array, 2, 1); 						   
		data_array[0] = 0x00023902; 								
		data_array[1] = 0x00002524; 								
		dsi_set_cmdq(data_array, 2, 1); 						   
		data_array[0] = 0x00023902;//18 							
		data_array[1] = 0x00005525; 								
		dsi_set_cmdq(data_array, 2, 1); 	
				
		data_array[0] = 0x00023902; 	 
		data_array[1] = 0x00000526; 	 
		dsi_set_cmdq(data_array, 2, 1); 
		data_array[0] = 0x00023902; 	 
		data_array[1] = 0x00002327; 	 
		dsi_set_cmdq(data_array, 2, 1); 
		data_array[0] = 0x00023902; 	 
		data_array[1] = 0x00000128; 	 
		dsi_set_cmdq(data_array, 2, 1); 
		data_array[0] = 0x00023902; 	 
		data_array[1] = 0x00003129; // 0029 	
		dsi_set_cmdq(data_array, 2, 1); 
		data_array[0] = 0x00023902; 	 
		data_array[1] = 0x00005D2A; 	 
		dsi_set_cmdq(data_array, 2, 1); 
		data_array[0] = 0x00023902;//06 
		data_array[1] = 0x0000012B; 	
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902; 	
		data_array[1] = 0x0000002F; 	
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902; 	
		data_array[1] = 0x00001030; 
		dsi_set_cmdq(data_array, 2, 1); 
		
		data_array[0] = 0x00023902; 			
		data_array[1] = 0x000012A7; 			
		dsi_set_cmdq(data_array, 2, 1); 	   
		data_array[0] = 0x00023902; 			
		data_array[1] = 0x0000032D; 			
		dsi_set_cmdq(data_array, 2, 1);
#endif
	#if 1
//GAMMA 2.2 START
//R+
    data_array[0] = 0x00023902;
    data_array[1] = 0x000001FF;                 
    dsi_set_cmdq(data_array, 2, 1);
 
    data_array[0] = 0x00023902;
    data_array[1] = 0x00000075;                 
    dsi_set_cmdq(data_array, 2, 1); 
    
    data_array[0] = 0x00023902;
    data_array[1] = 0x00004276;                 
    dsi_set_cmdq(data_array, 2, 1); 
    
    data_array[0] = 0x00023902;
    data_array[1] = 0x00000077; //V1-->GRAY 254               
    dsi_set_cmdq(data_array, 2, 1); 
    
    data_array[0] = 0x00023902;////CMD1 
    data_array[1] = 0x00005878;                 
    dsi_set_cmdq(data_array, 2, 1); 
    
    data_array[0] = 0x00023902;
    data_array[1] = 0x00000079; //V3 -->GRAY252               
    dsi_set_cmdq(data_array, 2, 1); 
    
    data_array[0] = 0x00023902;////CMD1 
    data_array[1] = 0x0000757A;                 
    dsi_set_cmdq(data_array, 2, 1); 
    
    data_array[0] = 0x00023902;
    data_array[1] = 0x0000007B;//V5 -->GRAY250                
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;////CMD1 
    data_array[1] = 0x0000937C;                 
    dsi_set_cmdq(data_array, 2, 1);

    data_array[0] = 0x00023902;
    data_array[1] = 0x0000007D;//V7 -->GRAY248                
    dsi_set_cmdq(data_array, 2, 1);
 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000A47E;                 
    dsi_set_cmdq(data_array, 2, 1); 
     
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000007F;//V9 -->GRAY246                
    dsi_set_cmdq(data_array, 2, 1); 
 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000B280;                 
    dsi_set_cmdq(data_array, 2, 1);
     
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000081;//V11  -->GRAY244               
    dsi_set_cmdq(data_array, 2, 1); 

    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000c582;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000083;//V13 -->GRAY242                
    dsi_set_cmdq(data_array, 2, 1); 

    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000d184;                 
    dsi_set_cmdq(data_array, 2, 1);
     
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000085;//V15  -->GRAY240               
    dsi_set_cmdq(data_array, 2, 1); 
 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000e086;                 
    dsi_set_cmdq(data_array, 2, 1);

    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000187;//V23 -->GRAY232                
    dsi_set_cmdq(data_array, 2, 1); 
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000588;                 
    dsi_set_cmdq(data_array, 2, 1); 

    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000189;//V31  -->GRAY224               
    dsi_set_cmdq(data_array, 2, 1); 
  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000268A;                 
    dsi_set_cmdq(data_array, 2, 1);

    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000018B;//V47  -->GRAY208               
    dsi_set_cmdq(data_array, 2, 1);
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00005f8C;                 
    dsi_set_cmdq(data_array, 2, 1); 

    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000018D;//V63 -->GRAY192                
    dsi_set_cmdq(data_array, 2, 1); 

    data_array[0] = 0x00023902; 
    data_array[1] = 0x00008d8E;                 
    dsi_set_cmdq(data_array, 2, 1); 
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000018F;//V95 -->GRAY160                
    dsi_set_cmdq(data_array, 2, 1); 

    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000d890;                 
    dsi_set_cmdq(data_array, 2, 1); 

    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000291;//V127 -->GRAY128                
    dsi_set_cmdq(data_array, 2, 1);

    data_array[0] = 0x00023902; 
    data_array[1] = 0x00001592;                 
    dsi_set_cmdq(data_array, 2, 1);

    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000293;//V128 -->GRAY127                
    dsi_set_cmdq(data_array, 2, 1);

    data_array[0] = 0x00023902; 
    data_array[1] = 0x00001694;                 
    dsi_set_cmdq(data_array, 2, 1);

    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000295;//V160 -->GRAY95                
    dsi_set_cmdq(data_array, 2, 1);

    data_array[0] = 0x00023902; 
    data_array[1] = 0x00004996;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000297;//V192 -->GRAY63                
    dsi_set_cmdq(data_array, 2, 1);

    data_array[0] = 0x00023902; 
    data_array[1] = 0x00008398;                 
    dsi_set_cmdq(data_array, 2, 1);

    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000299;//V208 -->GRAY47                
    dsi_set_cmdq(data_array, 2, 1); 
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000Aa9A;                 
    dsi_set_cmdq(data_array, 2, 1); 

    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000029B;//V224  -->GRAY31               
    dsi_set_cmdq(data_array, 2, 1); 

    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000Dd9C;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000029D;//V232 -->GRAY23                
    dsi_set_cmdq(data_array, 2, 1);

    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000fc9E;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000039F;//V240 -->GRAY15                
    dsi_set_cmdq(data_array, 2, 1); 
 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000028A0;                 
    dsi_set_cmdq(data_array, 2, 1);
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003A2;//V242 -->GRAY13               
    dsi_set_cmdq(data_array, 2, 1); 

    data_array[0] = 0x00023902; 
    data_array[1] = 0x000032A3;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003A4;//V244 -->GRAY11                
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000040A5;                 
    dsi_set_cmdq(data_array, 2, 1); 

    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003A6;//V246 -->GRAY9                
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00004cA7;                 
    dsi_set_cmdq(data_array, 2, 1);

    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003A9;//V248-->GRAY7                 
    dsi_set_cmdq(data_array, 2, 1);    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00005BAA;                 
    dsi_set_cmdq(data_array, 2, 1);
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003AB;//V250-->GRAY5                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000063AC;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003AD;//V252-->GRAY3                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00007BAE;                 
    dsi_set_cmdq(data_array, 2, 1); 
     
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003AF; //V254-->GRAY1                
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000083B0;                 
    dsi_set_cmdq(data_array, 2, 1); 
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003B1;//V255 -->GRAY0                
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000085B2;                 
    dsi_set_cmdq(data_array, 2, 1); 
//R- 
    
    data_array[0] = 0x00023902;////CMD1 
    data_array[1] = 0x000000B3;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;////CMD1 
    data_array[1] = 0x000042B4;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;////CMD1 
    data_array[1] = 0x000000B5;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;////CMD1 
    data_array[1] = 0x000058B6;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;////CMD1 
    data_array[1] = 0x000000B7;                 
    dsi_set_cmdq(data_array, 2, 1); 

    data_array[0] = 0x00023902;////CMD1 
    data_array[1] = 0x000075B8;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;////CMD1 
    data_array[1] = 0x000000B9;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;////CMD1 
    data_array[1] = 0x000093BA;                 
    dsi_set_cmdq(data_array, 2, 1);
    
    data_array[0] = 0x00023902;////CMD1 
    data_array[1] = 0x000000BB;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000a4BC;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000000BD;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000b2BE;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000000BF;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000c5C0;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000000C1;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000d1C2;                 
    dsi_set_cmdq(data_array, 2, 1);   
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000000C3;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000e0C4;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000001C5;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000005C6;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000001C7;                 
    dsi_set_cmdq(data_array, 2, 1);   
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000026C8;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000001C9;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00005fCA;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000001CB;                 
    dsi_set_cmdq(data_array, 2, 1);    
     
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00008dCC;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000001CD;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000d8CE;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000002CF;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000015D0;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000002D1;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000016D2;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000002D3;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000049D4;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000002D5;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000083D6;                 
    dsi_set_cmdq(data_array, 2, 1);
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000002D7;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000AaD8;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000002D9;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000DdDA;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000002DB;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000FcDC;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003DD;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000028DE;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003DF;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000032E0;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003E1;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000040E2;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003E3;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00004cE4;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003E5;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00005BE6;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003E7;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000063E8;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003E9;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00007BEA;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003EB;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000083EC;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003ED;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000085EE;                 
    dsi_set_cmdq(data_array, 2, 1);                                                  
//G+
   data_array[0] = 0x00023902;////CMD1 
    data_array[1] = 0x000000EF;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;////CMD1 
    data_array[1] = 0x000042F0;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;////CMD1 
    data_array[1] = 0x000000F1;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;////CMD1 
    data_array[1] = 0x000058F2;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;////CMD1 
    data_array[1] = 0x000000F3;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;////CMD1 
    data_array[1] = 0x000075F4;                 
    dsi_set_cmdq(data_array, 2, 1); 
    
    data_array[0] = 0x00023902;////CMD1 
    data_array[1] = 0x000000F5;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;////CMD1 
    data_array[1] = 0x00008EF6;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;////CMD1 
    data_array[1] = 0x000000F7;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00009FF8;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000000F9;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000ADFA; 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; //CMD2 PAGE1
    data_array[1] = 0x000002FF;    
    dsi_set_cmdq(data_array, 2, 1);         
  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000000;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000C001;                 
    dsi_set_cmdq(data_array, 2, 1); 
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000002;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000C903;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000004;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000D805;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000006;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000FB07;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000108;                 
    dsi_set_cmdq(data_array, 2, 1);   
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00001C09;                 
    dsi_set_cmdq(data_array, 2, 1);
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000010A;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000550B;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000010C;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000820D;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000010E;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000CA0F;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000210;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000511;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000212;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000613;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000214;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00003D15;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000216;                 
    dsi_set_cmdq(data_array, 2, 1);
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00007A17;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000218;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000A019;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000021A;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000D31B;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000021C;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000F21D;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000031E;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000201F;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000320;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00002A21;                 
    dsi_set_cmdq(data_array, 2, 1);  
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000322;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00003B23;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000324;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00004B25;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000326;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00005B27;                 
    dsi_set_cmdq(data_array, 2, 1);
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000328;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00006329;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000032A;                 
    dsi_set_cmdq(data_array, 2, 1);  
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00007B2B;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000032D;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000832F;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000330;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00008531;                 
    dsi_set_cmdq(data_array, 2, 1); 
//G- 
    data_array[0] = 0x00023902;
    data_array[1] = 0x00000032;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00004233;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;
    data_array[1] = 0x00000034;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00005835;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000036;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00007537;                 
    dsi_set_cmdq(data_array, 2, 1); 
    
    data_array[0] = 0x00023902;
    data_array[1] = 0x00000038;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;
    data_array[1] = 0x00008E39;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;
    data_array[1] = 0x0000003a;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00009f3B;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000003D;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000Ad3F;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000040;                 
    dsi_set_cmdq(data_array, 2, 1); 
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000c041;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000042;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000c943;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000044;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000d845;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000046;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000FB47;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000148;                 
    dsi_set_cmdq(data_array, 2, 1);   
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00001C49;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000014A;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000554B;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000014C;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000824D;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000014E;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000CA4F;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000250;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000551;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000252;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000653;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000254;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00003D55;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000256;                 
    dsi_set_cmdq(data_array, 2, 1);
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00007A58;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000259;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000A05A;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000025B;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000D35C;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000025D;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000f25E;                 
    dsi_set_cmdq(data_array, 2, 1);  
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000035F;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00002060;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000361;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00002a62;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000363;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00003b64;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000365;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00004b66;                 
    dsi_set_cmdq(data_array, 2, 1);
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000367;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00005B68;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000369;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000636A;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000036B;                 
    dsi_set_cmdq(data_array, 2, 1);  
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00007B6C;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000036D;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000836E;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000036F;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00008570;                 
    dsi_set_cmdq(data_array, 2, 1);                     
//B+
    data_array[0] = 0x00023902;
    data_array[1] = 0x00000071;                 
    dsi_set_cmdq(data_array, 2, 1); 
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00004272;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;
    data_array[1] = 0x00000073;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00005874;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000075;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00007576;                 
    dsi_set_cmdq(data_array, 2, 1); 
    
    data_array[0] = 0x00023902;
    data_array[1] = 0x00000077;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;
    data_array[1] = 0x00008e78;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;
    data_array[1] = 0x00000079;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00009f7A;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000007B;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000Ad7C;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000007D;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000c07E;                 
    dsi_set_cmdq(data_array, 2, 1); 
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000007F;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000c980;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000081;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000d882;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000083;                 
    dsi_set_cmdq(data_array, 2, 1); 
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000FB84;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000185;                 
    dsi_set_cmdq(data_array, 2, 1);   
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00001C86;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000187;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00005588;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000189;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000828A;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000018B;                 
    dsi_set_cmdq(data_array, 2, 1); 
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000CA8C;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000028D;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000058E;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000028F;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000690;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000291;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00003D92;                 
    dsi_set_cmdq(data_array, 2, 1); 
     
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000293;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00007A94;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000295;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000A096;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000297;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000D398;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00000299;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000f29A;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000039B;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000209C;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000039D;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00002A9E;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000039F;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00003bA0;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003A2;                 
    dsi_set_cmdq(data_array, 2, 1); 
     
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00004bA3;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003A4;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00005BA5;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003A6;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000063A7;                 
    dsi_set_cmdq(data_array, 2, 1);
      
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003A9;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00007BAA;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003AB;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000083AC;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003AD;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000085AE;                 
    dsi_set_cmdq(data_array, 2, 1); 
//B-
    data_array[0] = 0x00023902;
    data_array[1] = 0x000000AF;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000042B0;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;
    data_array[1] = 0x000000B1;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000058B2;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000000B3;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000075B4;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;
    data_array[1] = 0x000000B5;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902;
    data_array[1] = 0x00008EB6;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902;
    data_array[1] = 0x000000B7;                 
    dsi_set_cmdq(data_array, 2, 1); 
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00009fB8;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000000B9;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000AdBA;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000000BB;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000c0BC;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000000BD;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000c9BE;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000000BF;                 
    dsi_set_cmdq(data_array, 2, 1);  
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000d8C0;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000000C1;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000FBC2;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000001C3;                 
    dsi_set_cmdq(data_array, 2, 1);   
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00001CC4;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000001C5;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000055C6;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000001C7;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000082C8;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000001C9;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000CACA;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000002CB;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000005CC;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000002CD;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000006CE;                 
    dsi_set_cmdq(data_array, 2, 1);
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000002CF;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00003DD0;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000002D1;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00007AD2;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000002D3;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000A0D4;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000002D5;                 
    dsi_set_cmdq(data_array, 2, 1); 
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000D3D6;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000002D7;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x0000f2D8;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003D9;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000020DA;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003DB;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00002aDC;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003DD;                 
    dsi_set_cmdq(data_array, 2, 1); 
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00003bDE;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003DF;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00004bE0;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003E1;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00005bE2;                 
    dsi_set_cmdq(data_array, 2, 1);
    
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003E3; //03E2  legen              
    dsi_set_cmdq(data_array, 2, 1); 
   
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000063E4;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003E5;                 
    dsi_set_cmdq(data_array, 2, 1);  
    data_array[0] = 0x00023902; 
    data_array[1] = 0x00007BE6;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003E7;                 
    dsi_set_cmdq(data_array, 2, 1);
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000083E8;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000003E9;                 
    dsi_set_cmdq(data_array, 2, 1); 
    data_array[0] = 0x00023902; 
    data_array[1] = 0x000081EA;                 
    dsi_set_cmdq(data_array, 2, 1); 
#endif
//GAMMA2.2 END
		data_array[0] = 0x00023902;////CMD1 
		data_array[1] = 0x000000FF; 				
		dsi_set_cmdq(data_array, 2, 1);    
		data_array[0] = 0x00023902;
		data_array[1] = 0x000001FB; 				
		dsi_set_cmdq(data_array, 2, 1);    
		data_array[0] = 0x00023902;//CMD2,Page0 
		data_array[1] = 0x000001FF; 				
		dsi_set_cmdq(data_array, 2, 1); 	  
		data_array[0] = 0x00023902;
		data_array[1] = 0x000001FB; 				
		dsi_set_cmdq(data_array, 2, 1);   
		data_array[0] = 0x00023902;//CMD2,Page1 
		data_array[1] = 0x000002FF; 				
		dsi_set_cmdq(data_array, 2, 1); 		
		data_array[0] = 0x00023902;
		data_array[1] = 0x000001FB; 				
		dsi_set_cmdq(data_array, 2, 1); 	  
			
		data_array[0] = 0x00023902;//CMD2,Page2 
		data_array[1] = 0x000003FF; 				
		dsi_set_cmdq(data_array, 2, 1); 		
		data_array[0] = 0x00023902;
		data_array[1] = 0x000001FB; 				
		dsi_set_cmdq(data_array, 2, 1); 	
		data_array[0] = 0x00023902;//CMD2,Page3
		data_array[1] = 0x000004FF; 		
		dsi_set_cmdq(data_array, 2, 1); 									   
		data_array[0] = 0x00023902; 		
		data_array[1] = 0x000001FB; 		
		dsi_set_cmdq(data_array, 2, 1);    
		data_array[0] = 0x00023902;//CMD2,Page4
		data_array[1] = 0x000005FF; 		
		dsi_set_cmdq(data_array, 2, 1);    
		data_array[0] = 0x00023902; 		
		data_array[1] = 0x000001FB; 		
		dsi_set_cmdq(data_array, 2, 1);
		////RTN for frame rate of cmd mode
		data_array[0] = 0x00023902; 		
		data_array[1] = 0x00000001; 		
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902; 		
		data_array[1] = 0x00008802; 		
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902; 		
		data_array[1] = 0x00008803; 		
		dsi_set_cmdq(data_array, 2, 1);
		data_array[0] = 0x00023902; 		
		data_array[1] = 0x00008804; 		
		dsi_set_cmdq(data_array, 2, 1);

		
		data_array[0] = 0x00023902; 	////CMD1	 
		data_array[1] = 0x000000FF; 		
		dsi_set_cmdq(data_array, 2, 1); 
	
		/*******debug-----start********/
		data_array[0] = 0x00110500; 			   
		dsi_set_cmdq(data_array, 1, 1); 
		MDELAY(120); 
	
		data_array[0] = 0x00023902;
		data_array[1] = 0x0000EEFF; 				
		dsi_set_cmdq(data_array, 2, 1);
		MDELAY(1); 
		data_array[0] = 0x00023902;
		data_array[1] = 0x00005012; 				
		dsi_set_cmdq(data_array, 2, 1);
		MDELAY(1); 
		data_array[0] = 0x00023902;
		data_array[1] = 0x00000213; 				
		dsi_set_cmdq(data_array, 2, 1);
		MDELAY(1); 
		data_array[0] = 0x00023902;
		data_array[1] = 0x0000606A; 				
		dsi_set_cmdq(data_array, 2, 1);
		MDELAY(1); 
		data_array[0] = 0x00023902;
		data_array[1] = 0x000000FF; 				
		dsi_set_cmdq(data_array, 2, 1);
		MDELAY(1); 
		data_array[0] = 0x00023902; 	
		data_array[1] = 0x00000035; 		
		dsi_set_cmdq(data_array, 2, 1); 
		
		data_array[0] = 0x00033902;
		data_array[1] = (((FRAME_HEIGHT/2)&0xFF) << 16) | (((FRAME_HEIGHT/2)>>8) << 8) | 0x44;
		dsi_set_cmdq(data_array, 2, 1);
			
		data_array[0] = 0x00290500; 			   
		dsi_set_cmdq(data_array, 1, 1); 
		MDELAY(50);
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
}


static void lcm_resume(void)
{
	lcm_init();

    #ifdef BUILD_LK
	  printf("[LK]---cmd---nt35590----%s------\n",__func__);
    #else
	  printk("[KERNEL]---cmd---nt35590----%s------\n",__func__);
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
	unsigned int id=0;
	unsigned char buffer[2];
	unsigned int array[16];  

	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);
	
	SET_RESET_PIN(1);
	MDELAY(20); 

	array[0] = 0x00023700;// read id return two byte,version and id
	dsi_set_cmdq(array, 1, 1);
	
	read_reg_v2(0xF4, buffer, 2);
	id = buffer[0]; //we only need ID
    #ifdef BUILD_LK
		printf("%s, LK nt35590 debug: nt35590 id = 0x%08x\n", __func__, id);
    #else
		printk("%s, kernel nt35590 horse debug: nt35590 id = 0x%08x\n", __func__, id);
    #endif

    if(id == LCM_ID_NT35590)
    	return 1;
    else
        return 0;


}


static unsigned int lcm_esd_check(void)
{
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

	read_reg_v2(0x36, buffer, 1);
	if(buffer[0]==0x90)
	{
		return FALSE;
	}
	else
	{			 
		return TRUE;
	}
#else
	return FALSE;
 #endif

}

static unsigned int lcm_esd_recover(void)
{
	lcm_init();
	lcm_resume();

	return TRUE;
}



LCM_DRIVER nt35590_hd720_dsi_vdo_truly_lcm_drv = 
{
    .name			= "nt35590_hd720_dsi_vdo_truly",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id     = lcm_compare_id,
	.esd_check = lcm_esd_check,
	.esd_recover = lcm_esd_recover,
#if (LCM_DSI_CMD_MODE)
    .update         = lcm_update,
#endif
    };
