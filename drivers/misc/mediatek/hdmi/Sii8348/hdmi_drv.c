/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#if defined(CONFIG_MTK_HDMI_SUPPORT)
#include <linux/kernel.h>

/*#include <linux/xlog.h>*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/kthread.h>

/*#include <mtk_kpd.h>*/
#include "si_timing_defs.h"

#include "hdmi_drv.h"
#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
#include "smartbook.h"
#endif

#ifdef CONFIG_MTK_LEGACY
#include "hdmi_cust.h"
#endif
/**
* MHL TX Chip Driver User Layer Interface
*/
extern struct mhl_dev_context *si_dev_context;
extern void ForceSwitchToD3( struct mhl_dev_context *dev_context);
extern void	ForceNotSwitchToD3(void);
extern int si_mhl_tx_post_initialize(struct mhl_dev_context *dev_context, bool bootup);
extern void siHdmiTx_VideoSel (int vmode);
extern void siHdmiTx_AudioSel (int AduioMode);
extern void set_platform_bitwidth(int bitWidth);
extern bool si_mhl_tx_set_path_en_I(struct mhl_dev_context *dev_context);
extern bool packed_pixel_available(struct mhl_dev_context *dev_context);
extern int dongle_dsc_dec_available(struct mhl_dev_context *dev_context);
extern void configure_and_send_audio_info(struct mhl_dev_context *dev_context, int audio_format);

//Should align to mhl_linux_tx.h
#define	MHL_TX_EVENT_DISCONNECTION	0x01
#define	MHL_TX_EVENT_CONNECTION		0x02
#define MHL_TX_EVENT_SMB_DATA		0x40
#define MHL_TX_EVENT_HPD_CLEAR 		0x41
#define MHL_TX_EVENT_HPD_GOT 		0x42
#define MHL_TX_EVENT_DEV_CAP_UPDATE 0x43
#define MHL_TX_EVENT_EDID_UPDATE 	0x44
#define MHL_TX_EVENT_EDID_DONE 		0x45
#define MHL_TX_EVENT_CALLBACK 		0x46

/**
* Platform Related Layer Interface
*/
extern int HalOpenI2cDevice(char const *DeviceName, char const *DriverName);
extern int32_t sii_8348_tx_init(void);    //Should  move to MHL TX Chip user layer
/**
* LOG For MHL TX Chip HAL
*/
static size_t hdmi_log_on = true;
static int txInitFlag = 0;
int	chip_device_id = 0;
bool need_reset_usb_switch = true;

#define MHL_DBG(fmt, arg...)  \
	do { \
		if (1) pr_debug("[HDMI_Chip_HAL]:"fmt, ##arg);  \
	}while (0)

#define MHL_FUNC()    \
	do { \
		if(hdmi_log_on) pr_debug("[HDMI_Chip_HAL] %s\n", __func__); \
	}while (0)

void hdmi_drv_log_enable(bool enable)
{
	hdmi_log_on = enable;
}

static int not_switch_to_d3 = 0;
static int audio_enable = 0;

#ifndef CONFIG_MTK_LEGACY 
extern void i2s_gpio_ctrl(int enable);
extern void dpi_gpio_ctrl(int enable);
#endif

void hdmi_drv_force_on(int from_uart_drv )
{
    MHL_DBG("hdmi_drv_force_on %d\n", from_uart_drv);

    if(from_uart_drv == 0)
        ForceNotSwitchToD3();
    not_switch_to_d3 = 1;
#ifndef CONFIG_MTK_LEGACY    
//gpio:uart  to be impl
    i2s_gpio_ctrl(0);
#endif
}
 
/************************** Upper Layer To HAL*********************************/
static struct HDMI_UTIL_FUNCS hdmi_util = {0};
static void hdmi_drv_set_util_funcs(const struct HDMI_UTIL_FUNCS *util)
{
	memcpy(&hdmi_util, util, sizeof(struct HDMI_UTIL_FUNCS));
}

static char* cable_type_print(unsigned short type)
{
	switch(type)
	{
		case HDMI_CABLE:
			return "HDMI_CABLE";
		case MHL_CABLE:
			return "MHL_CABLE";
		case MHL_SMB_CABLE:
			return "MHL_SMB_CABLE";
		case MHL_2_CABLE:
			return "MHL_2_CABLE";
		default:
			MHL_DBG("Unknow MHL Cable Type\n");
			return "Unknow MHL Cable Type\n";
	}	
}

enum HDMI_CABLE_TYPE MHL_Connect_type = MHL_CABLE;
static unsigned int HDCP_Supported_Info = 0;
bool MHL_3D_Support = false;
int MHL_3D_format=0x00;
static void hdmi_drv_get_params(struct HDMI_PARAMS *params)
{
	char* cable_str = "";
	enum HDMI_VIDEO_RESOLUTION input_resolution = params->init_config.vformat;
	memset(params, 0, sizeof(struct HDMI_PARAMS));

	switch (input_resolution)
	{
		case HDMI_VIDEO_720x480p_60Hz:
			params->clk_pol   = HDMI_POLARITY_FALLING;
			params->de_pol    = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_RISING;
			params->vsync_pol = HDMI_POLARITY_RISING;;
			
			params->hsync_pulse_width = 62;
			params->hsync_back_porch  = 60;
			params->hsync_front_porch = 16;
			
			params->vsync_pulse_width = 6;
			params->vsync_back_porch  = 30;
			params->vsync_front_porch = 9;
			
			params->width       = 720;
			params->height      = 480;
			params->input_clock = 27027;

			params->init_config.vformat = HDMI_VIDEO_720x480p_60Hz;
			break;
		case HDMI_VIDEO_1280x720p_60Hz:
			params->clk_pol   = HDMI_POLARITY_FALLING;
			params->de_pol    = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_FALLING;
			params->vsync_pol = HDMI_POLARITY_FALLING;;
			
			params->hsync_pulse_width = 40;
			params->hsync_back_porch  = 220;
			params->hsync_front_porch = 110;
			
			params->vsync_pulse_width = 5;
			params->vsync_back_porch  = 20;
			params->vsync_front_porch = 5;
		
			params->width       = 1280;
			params->height      = 720;
#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
			if (MHL_Connect_type == MHL_SMB_CABLE)
			{
				params->width  = 1366;
				params->height = 768;
			}
#endif
			params->input_clock = 74250;

			params->init_config.vformat = HDMI_VIDEO_1280x720p_60Hz;
			break;
		case HDMI_VIDEO_1920x1080p_30Hz:
			params->clk_pol   = HDMI_POLARITY_FALLING;
			params->de_pol    = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_FALLING;
			params->vsync_pol = HDMI_POLARITY_FALLING;;
			
			params->hsync_pulse_width = 44;
			params->hsync_back_porch  = 148;
			params->hsync_front_porch = 88;
			
			params->vsync_pulse_width = 5;
			params->vsync_back_porch  = 36;
			params->vsync_front_porch = 4;
			
			params->width       = 1920;
			params->height      = 1080;
			params->input_clock = 74250;

			params->init_config.vformat = HDMI_VIDEO_1920x1080p_30Hz;
			break;
		case HDMI_VIDEO_1920x1080p_60Hz:
			params->clk_pol   = HDMI_POLARITY_FALLING;
			params->de_pol    = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_FALLING;
			params->vsync_pol = HDMI_POLARITY_FALLING;;
			
			params->hsync_pulse_width = 44;
			params->hsync_back_porch  = 148;
			params->hsync_front_porch = 88;
			
			params->vsync_pulse_width = 5;
			params->vsync_back_porch  = 36;
			params->vsync_front_porch = 4;
			
			params->width       = 1920;
			params->height      = 1080;
			params->input_clock = 148500;

			params->init_config.vformat = HDMI_VIDEO_1920x1080p_60Hz;
			break;
		case HDMI_VIDEO_2160p_DSC_24Hz:
			params->clk_pol   = HDMI_POLARITY_FALLING;
			params->de_pol    = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_FALLING;
			params->vsync_pol = HDMI_POLARITY_FALLING;;
			
			params->hsync_pulse_width = 30;
			params->hsync_back_porch  = 98;
			params->hsync_front_porch = 242;
			
			params->vsync_pulse_width = 10;
			params->vsync_back_porch  = 72;
			params->vsync_front_porch = 8;
			
			params->width       = 3840;  ///1280
			params->height      = 2160;
			params->input_clock = 89100;

			params->init_config.vformat = HDMI_VIDEO_2160p_DSC_24Hz;
			break;
		case HDMI_VIDEO_2160p_DSC_30Hz:
			params->clk_pol   = HDMI_POLARITY_FALLING;
			params->de_pol    = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_FALLING;
			params->vsync_pol = HDMI_POLARITY_FALLING;;
			
			params->hsync_pulse_width = 30;
			params->hsync_back_porch  = 98;
			params->hsync_front_porch = 66;
			
			params->vsync_pulse_width = 10;
			params->vsync_back_porch  = 72;
			params->vsync_front_porch = 8;
			
			params->width       = 3840;  ///1280
			params->height      = 2160;
			params->input_clock = 99500;

			params->init_config.vformat = HDMI_VIDEO_2160p_DSC_30Hz;
			break;
		default:
			MHL_DBG("Unknow support resolution\n");
			break;
	}

	params->init_config.aformat         = HDMI_AUDIO_44K_2CH;
	params->rgb_order         			= HDMI_COLOR_ORDER_RGB;
	params->io_driving_current 			= IO_DRIVING_CURRENT_2MA;
	params->intermediat_buffer_num 		= 4;
	params->scaling_factor 				= 0;
	params->cabletype 					= MHL_Connect_type;
	params->HDCPSupported 				= HDCP_Supported_Info;
	if(MHL_Connect_type== MHL_3D_GLASSES)    
		params->cabletype 				= MHL_CABLE;

	params->is_3d_support 				= MHL_3D_Support;
	cable_str = cable_type_print(params->cabletype);
    MHL_DBG("type %s-%d hdcp %d-%d\n", cable_str, MHL_Connect_type, params->HDCPSupported, HDCP_Supported_Info);

}

void hdmi_drv_suspend(void) {return ;}
void hdmi_drv_resume(void)  {return ;}
static int hdmi_drv_audio_config(enum HDMI_AUDIO_FORMAT aformat, int bitWidth)
{
	set_platform_bitwidth(bitWidth);
	siHdmiTx_AudioSel(aformat);
	configure_and_send_audio_info(si_dev_context, aformat);
	
	return 0;
}
static int hdmi_drv_video_enable(bool enable) 
{
    return 0;
}

static int hdmi_drv_audio_enable(bool enable)  
{
    MHL_DBG("[EXTD]Set_I2S_Pin, enable = %d\n", enable);   
    //gpio:uart
#ifdef CONFIG_MTK_LEGACY
    if(not_switch_to_d3 == 1)
        cust_hdmi_i2s_gpio_on(2);
    else
        cust_hdmi_i2s_gpio_on(enable);
#else
	if (enable)
		i2s_gpio_ctrl(1);
	else
		i2s_gpio_ctrl(0);
#endif
    audio_enable = enable;

    return 0;
}

static int hdmi_drv_enter(void)  {return 0;}
static int hdmi_drv_exit(void)  {return 0;}

extern void si_mhl_tx_drv_video_3d_update(struct mhl_dev_context *dev_context, int video_3d);
extern void si_mhl_tx_drv_video_3d(struct mhl_dev_context *dev_context, int video_3d);
static int hdmi_drv_video_config(enum HDMI_VIDEO_RESOLUTION vformat, enum HDMI_VIDEO_INPUT_FORMAT vin, int vout)
{
	if(vformat == HDMI_VIDEO_720x480p_60Hz)
	{
		MHL_DBG("[hdmi_drv]480p\n");
		siHdmiTx_VideoSel(HDMI_480P60_4X3);
	}
	else if(vformat == HDMI_VIDEO_1280x720p_60Hz)
	{
		MHL_DBG("[hdmi_drv]720p\n");
		siHdmiTx_VideoSel(HDMI_720P60);
	}
	else if(vformat == HDMI_VIDEO_1920x1080p_30Hz)
	{
		MHL_DBG("[hdmi_drv]1080p_30\n");
		siHdmiTx_VideoSel(HDMI_1080P30);
	}
	else if(vformat == HDMI_VIDEO_1920x1080p_60Hz)
	{
		MHL_DBG("[hdmi_drv]1080p_60\n");
		siHdmiTx_VideoSel(HDMI_1080P60);
	}
	else if(vformat == HDMI_VIDEO_2160p_DSC_24Hz)
	{
		MHL_DBG("[hdmi_drv]2160p_24\n");
		siHdmiTx_VideoSel(HDMI_4k24_DSC);
	}
	else if(vformat == HDMI_VIDEO_2160p_DSC_30Hz)
	{
		MHL_DBG("[hdmi_drv]2160p_30\n");
		siHdmiTx_VideoSel(HDMI_4k30_DSC);
	}
	else
	{
		MHL_DBG("%s, video format not support now\n", __func__);
	}

	if(vout & HDMI_VOUT_FORMAT_3D_SBS)
        MHL_3D_format=VIDEO_3D_SS;
	else if(vout & HDMI_VOUT_FORMAT_3D_TAB)
	    MHL_3D_format=VIDEO_3D_TB;
    	else
	    MHL_3D_format = VIDEO_3D_NONE;
	    
	if(si_dev_context)
	{	    
        	MHL_DBG("video format  -- %d, 0x%x, %d\n", MHL_3D_Support, vout, MHL_3D_format);
		if(vformat < HDMI_VIDEO_RESOLUTION_NUM)
		{
		    	///if(MHL_3D_Support && (MHL_3D_format > 0))
		    	si_mhl_tx_drv_video_3d(si_dev_context, MHL_3D_format);	
		    	if(vformat <= HDMI_VIDEO_1920x1080p_60Hz)
            			si_mhl_tx_set_path_en_I(si_dev_context);
		}
		else
		    	si_mhl_tx_drv_video_3d_update(si_dev_context, MHL_3D_format);
 	}
    return 0;
}

static unsigned int sii_mhl_connected = 0;
static uint8_t ReadConnectionStatus(void)
{
    return (sii_mhl_connected == MHL_TX_EVENT_CALLBACK)? 1 : 0;
}
enum HDMI_STATE hdmi_drv_get_state(void)
{
	int ret = ReadConnectionStatus();
	MHL_DBG("ret: %d\n", ret);
	
	if(ret == 1)
		return HDMI_STATE_ACTIVE;
	else
		return HDMI_STATE_NO_DEVICE;
}

bool chip_inited = false;
static int hdmi_drv_init(void)
{
    MHL_DBG("hdmi_drv_init, not_switch_to_d3: %d, init-%d\n", not_switch_to_d3, chip_inited);
    if(chip_inited == true)
        return 0;

	/*cust_hdmi_power_on(true);*/
	if(not_switch_to_d3 == 0) 
    {
        HalOpenI2cDevice("Sil_MHL", "sii8348drv");
	}
	
	txInitFlag = 0;
	chip_inited = true;

	//Should be enhanced
	chip_device_id = 0;
	need_reset_usb_switch = true;

    MHL_DBG("hdmi_drv_init -\n" );
    return 0;
}

int hdmi_drv_power_on(void)
{
    int ret = 1;
    MHL_FUNC();

/*
    if(not_switch_to_d3 > 0)
    {
        MHL_DBG("hdmi_drv_power_on direct to exit for forceon(%d_\n", not_switch_to_d3);
        return ;
    }
*/
#ifdef CONFIG_MTK_LEGACY     
	cust_hdmi_power_on(1);	
	cust_hdmi_dpi_gpio_on(1);  
#else
	dpi_gpio_ctrl(1);
	i2s_gpio_ctrl(1);
#endif	
    //cust_hdmi_i2s_gpio_on(true);   
    
	if(txInitFlag == 0)
	{
		///sii_8348_tx_init(); 
		txInitFlag = 1;
	}
	
	goto power_on_exit;

/*
	MHL_Power(true);
	Mask_MHL_Intr();

	if(chip_inited == false)
	{
		if(txInitFlag == 0)
		{
			sii_8348_tx_init();
			txInitFlag = 1;
		}
		else
		{
			si_mhl_tx_post_initialize(si_dev_context, false);  
		}

		chip_inited = true;
	}
*/
power_on_exit:

    if(chip_device_id >0)
        ret = 0;
	
    MHL_DBG("status %d, chipid: %x, ret: %d--%d\n", ReadConnectionStatus() , chip_device_id, ret, need_reset_usb_switch);        

    return ret;
}

void hdmi_drv_power_off(void)
{

	MHL_FUNC();
	
    if(not_switch_to_d3 > 0)
    {
        MHL_DBG("hdmi_drv_power_off direct to exit for forceon(%d_\n", not_switch_to_d3 );
        return ;
    }


#ifdef CONFIG_MTK_LEGACY      
    if(audio_enable == 0)
	    cust_hdmi_i2s_gpio_on(0);
#else
    dpi_gpio_ctrl(0);
	i2s_gpio_ctrl(0);
    if(audio_enable == 0)
	    i2s_gpio_ctrl(1);
#endif
  	return ;
	
    if(ReadConnectionStatus()==1){
        need_reset_usb_switch = true;
    	 ForceSwitchToD3(si_dev_context);
    }
    else
        need_reset_usb_switch = false;

	/*cust_hdmi_power_on(false);*/
	chip_inited = false;
    return ;

}

/* for otg currency leakage */
void switch_mhl_to_d3(void)
{    
    if(si_dev_context)
        ForceSwitchToD3(si_dev_context);
}
static unsigned int pal_resulution = 0;
void update_av_info_edid(bool audio_video, unsigned int param1, unsigned int param2)
{
    if(audio_video)///video infor
    {
        switch(param1)
    	{
    	    case 0x22:
    	    case 0x14:
            	pal_resulution |= SINK_1080P30;
            	break;
            case 0x10:
            	if(packed_pixel_available(si_dev_context))
                	pal_resulution |= SINK_1080P60;
            	break;
            case 0x4:
            	pal_resulution |= SINK_720P60;
            	break;
            case 0x3:
            case 0x2:
            	pal_resulution |= SINK_480P;
            	break;
	    case HDMI_4k24_DSC:
	  	pal_resulution |= SINK_2160p24;
	  	break;
	    case HDMI_4k30_DSC:
		pal_resulution |= SINK_2160p30;
		break;

	    default:
		MHL_DBG("param1: %x\n", param1);
    	}
	}	

	return ;
}
unsigned int si_mhl_get_av_info(void)
{
    unsigned int temp = SINK_1080P30;
    
    if(pal_resulution&SINK_1080P60)
        pal_resulution &= (~temp);
        
    return pal_resulution;
}
void reset_av_info(void)
{
    pal_resulution = 0;
}
void hdmi_GetEdidInfo(void *pv_get_info)
{
    struct HDMI_EDID_INFO_T *ptr = (struct HDMI_EDID_INFO_T *)pv_get_info;
    if(ptr)
    {
        ptr->ui4_ntsc_resolution = 0;
		ptr->ui4_pal_resolution = si_mhl_get_av_info();
		if(ptr->ui4_pal_resolution == 0)
		{
			MHL_DBG("MHL edid parse error \n");
			
	        if(si_dev_context && packed_pixel_available(si_dev_context))
	            ptr->ui4_pal_resolution = SINK_720P60 | SINK_1080P60 | SINK_480P;
	        else
	            ptr->ui4_pal_resolution = SINK_720P60 | SINK_1080P30 | SINK_480P;
		}
    }

	if (ptr->ui4_pal_resolution & SINK_2160p30)
		ptr->ui4_pal_resolution &= (~SINK_2160p24);

#ifdef MHL_RESOLUTION_LIMIT_720P_60
		ptr->ui4_pal_resolution &= (~SINK_1080P60);
		ptr->ui4_pal_resolution &= (~SINK_1080P30);
		ptr->ui4_pal_resolution &= (~SINK_2160p30);
		ptr->ui4_pal_resolution &= (~SINK_2160p24);
#endif

#ifdef MHL_RESOLUTION_LIMIT_1080P_30
		if(ptr->ui4_pal_resolution & SINK_1080P60)
		{
			ptr->ui4_pal_resolution &= (~SINK_1080P60);
			ptr->ui4_pal_resolution |= SINK_1080P30;
		}
#endif

    if(si_dev_context)
    {
        MHL_DBG("MHL hdmi_GetEdidInfo ntsc 0x%x,pal: 0x%x, packed: %d, parsed 0x%x\n", ptr->ui4_ntsc_resolution  , 
        	ptr->ui4_pal_resolution, packed_pixel_available(si_dev_context), si_mhl_get_av_info());
    }

}


extern uint8_t  Cap_MAX_channel;
extern uint16_t Cap_SampleRate;
extern uint8_t  Cap_Samplebit;

int hdmi_drv_get_external_device_capablity(void)
{
	int capablity = 0;
	MHL_DBG("Cap_MAX_channel: %d, Cap_Samplebit: %d, Cap_SampleRate: %d\n", Cap_MAX_channel, Cap_Samplebit, Cap_SampleRate);
	capablity = Cap_MAX_channel << 3 | Cap_SampleRate << 7 | Cap_Samplebit << 10;

	if(capablity == 0)
	{
		capablity = HDMI_CHANNEL_2 << 3 | HDMI_SAMPLERATE_44 << 7 |  HDMI_BITWIDTH_16 << 10;
	}

	return capablity;
}

#define HDMI_MAX_INSERT_CALLBACK   10
static CABLE_INSERT_CALLBACK hdmi_callback_table[HDMI_MAX_INSERT_CALLBACK];
void hdmi_register_cable_insert_callback(CABLE_INSERT_CALLBACK cb)
{
    int i = 0;
    for (i = 0; i < HDMI_MAX_INSERT_CALLBACK; i++) {
        if (hdmi_callback_table[i] == cb)
            break;
    }
    if (i < HDMI_MAX_INSERT_CALLBACK)
        return;

    for (i = 0; i < HDMI_MAX_INSERT_CALLBACK; i++) {
        if (hdmi_callback_table[i] == NULL)
            break;
    }
    if (i == HDMI_MAX_INSERT_CALLBACK) {
        MHL_DBG("not enough mhl callback entries for module\n");
        return;
    }

    hdmi_callback_table[i] = cb;
	MHL_DBG("callback: %p,i: %d\n", hdmi_callback_table[i], i);
}

void hdmi_unregister_cable_insert_callback(CABLE_INSERT_CALLBACK cb)
{
    int i;
    for (i=0; i<HDMI_MAX_INSERT_CALLBACK; i++)
    {
        if (hdmi_callback_table[i] == cb)
        {
        	MHL_DBG("unregister cable insert callback: %p, i: %d\n", hdmi_callback_table[i], i);
            hdmi_callback_table[i] = NULL;
            break;
        }
    }
    if (i == HDMI_MAX_INSERT_CALLBACK)
    {
        MHL_DBG("Try to unregister callback function 0x%lx which was not registered\n",(unsigned long int)cb);
        return;
    }
}

void hdmi_invoke_cable_callbacks(enum HDMI_STATE state)
{
    int i = 0, j = 0;   
    for (i=0; i<HDMI_MAX_INSERT_CALLBACK; i++)   // 0 is for external display
    {
        if(hdmi_callback_table[i])
        {
			j = i;
        }
    }

	if (hdmi_callback_table[j])
	{
		MHL_DBG("callback: %p, state: %d, j: %d\n", hdmi_callback_table[j], state, j);
		hdmi_callback_table[j](state);
	}
}

/************************** ****************************************************/
const struct HDMI_DRIVER* HDMI_GetDriver(void)
{
	static const struct HDMI_DRIVER HDMI_DRV =
	{
		.set_util_funcs = hdmi_drv_set_util_funcs,
		.get_params     = hdmi_drv_get_params,
		.init           = hdmi_drv_init,
        .enter          = hdmi_drv_enter,
        .exit           = hdmi_drv_exit,
		.suspend        = hdmi_drv_suspend,
		.resume         = hdmi_drv_resume,
        .video_config   = hdmi_drv_video_config,
        .audio_config   = hdmi_drv_audio_config,
        .video_enable   = hdmi_drv_video_enable,
        .audio_enable   = hdmi_drv_audio_enable,
        .power_on       = hdmi_drv_power_on,
        .power_off      = hdmi_drv_power_off,
        .get_state      = hdmi_drv_get_state,
        .log_enable     = hdmi_drv_log_enable,
        .getedid        = hdmi_GetEdidInfo,
        .get_external_device_capablity = hdmi_drv_get_external_device_capablity,
		.register_callback   = hdmi_register_cable_insert_callback,
		.unregister_callback = hdmi_unregister_cable_insert_callback,
        .force_on = hdmi_drv_force_on,
	};

    MHL_FUNC();
	return &HDMI_DRV;
}
/************************** ****************************************************/

/************************** HAL To SmartBook****************************************/
static void SMB_Init(void)
{
#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
	//SMARTBOOK:  HID init
	if(MHL_Connect_type == MHL_SMB_CABLE)
	{
		SiiHidSuspend(1);
	}
#endif
	return ;
}
static void SMB_Denit(void)
{
#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
	if(MHL_Connect_type == MHL_SMB_CABLE)
	{
		SiiHidSuspend(0);
	}
#endif
	return ;
}
static void SMB_Write_Data(uint8_t *data)
{
#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
	if(MHL_Connect_type == MHL_SMB_CABLE)
	{
		SiiHidWrite(data);
	}
#endif
	return ;
}
static void SMB_HandShake_Init(void)
{
#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
	if(MHL_Connect_type == MHL_SMB_CABLE)
	{
		SiiHandshakeCommand(Init);
	}
#endif
	return ;
}
/************************** *****************************************************/

/************************** MHL TX Chip User Layer To HAL*******************************/
static char* MHL_TX_Event_Print(unsigned int event)
{
	switch(event)
	{
		case MHL_TX_EVENT_CONNECTION:
			return "MHL_TX_EVENT_CONNECTION";
		case MHL_TX_EVENT_DISCONNECTION:
			return "MHL_TX_EVENT_DISCONNECTION";
		case MHL_TX_EVENT_HPD_CLEAR:
			return "MHL_TX_EVENT_HPD_CLEAR";
		case MHL_TX_EVENT_HPD_GOT:
			return "MHL_TX_EVENT_HPD_GOT";
		case MHL_TX_EVENT_DEV_CAP_UPDATE:
			return "MHL_TX_EVENT_DEV_CAP_UPDATE";
		case MHL_TX_EVENT_EDID_UPDATE:
			return "MHL_TX_EVENT_EDID_UPDATE";
		case MHL_TX_EVENT_EDID_DONE:
			return "MHL_TX_EVENT_EDID_DONE";
		case MHL_TX_EVENT_SMB_DATA:
			return "MHL_TX_EVENT_SMB_DATA";
		default:
			MHL_DBG("Unknow MHL TX Event Type\n");
			return "Unknow MHL TX Event Type\n";
	}	
}

void Notify_AP_MHL_TX_Event(unsigned int event, unsigned int event_param, void *param)
{
	char* event_str = "";
	event_str = MHL_TX_Event_Print(event);
	if(event != MHL_TX_EVENT_SMB_DATA)
		MHL_DBG("%s, event_param: %d\n", event_str, event_param);
	switch(event)
	{
		case MHL_TX_EVENT_CONNECTION:
			break;
		case MHL_TX_EVENT_DISCONNECTION:
		{
			sii_mhl_connected = MHL_TX_EVENT_DISCONNECTION;
			hdmi_invoke_cable_callbacks(HDMI_STATE_NO_DEVICE);
			reset_av_info();
			SMB_Denit();
			MHL_Connect_type = MHL_CABLE;
		}
			break;
		case MHL_TX_EVENT_HPD_CLEAR:
		{
			sii_mhl_connected= MHL_TX_EVENT_DISCONNECTION;
			hdmi_invoke_cable_callbacks(HDMI_STATE_NO_DEVICE);
		}
			break;
		case MHL_TX_EVENT_HPD_GOT:
			break;
		case MHL_TX_EVENT_DEV_CAP_UPDATE:
		{
		    if(event_param == 0xBA)
                	MHL_Connect_type = MHL_3D_GLASSES;	
		    else		    
    			MHL_Connect_type = MHL_SMB_CABLE;			
		}
			break;
		case MHL_TX_EVENT_EDID_UPDATE:
		{
			update_av_info_edid(true, event_param, 0);
		}
			break;
		case MHL_TX_EVENT_EDID_DONE:
		{
///#ifdef HDCP_ENABLE
            if(chip_device_id == 0x8346)
			    HDCP_Supported_Info = 140; ///HDCP 1.4
///#endif
			sii_mhl_connected = MHL_TX_EVENT_CALLBACK;
			hdmi_invoke_cable_callbacks(HDMI_STATE_ACTIVE);
			SMB_Init();
			SMB_HandShake_Init();
		}
			break;
		case MHL_TX_EVENT_SMB_DATA:
		{
			//SMARTBOOK: get write burst command
			SMB_Write_Data((uint8_t *)param);
		}
			break;
		default:
			return ;
	}

	return ;
}
/************************** ****************************************************/
#endif
