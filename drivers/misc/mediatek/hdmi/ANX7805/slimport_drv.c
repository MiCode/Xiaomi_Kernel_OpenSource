#if defined(CONFIG_MTK_HDMI_SUPPORT)
#include <linux/kernel.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#include "hdmi_drv.h"
/*#include "hdmi_cust.h"*/

#include "slimport.h"
#include "slimport_tx_drv.h"

/*#include "mach/eint.h"*/
#include "mach/irqs.h"
#ifdef CONFIG_MTK_LEGACY
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#endif
/*#include <cust_eint.h>*/
#include <mt-plat/mt_gpio.h>
/* #include <cust_gpio_usage.h> */

/*SLIMPORT_FEATURE*/
#include "slimport_platform.h"

/** 
* Platform Related Layer Interface
*/
/*
extern int HalOpenI2cDevice(char const *DeviceName, char const *DriverName);
extern Mask_MHL_Intr(void);
*/
/**
* LOG For SLIMPORT TX Chip HAL
*/
static size_t slimport_log_on = true;
static int txInitFlag = 0;
bool SlimPort_3D_Support = false;
int  SlimPort_3D_format=0x00;

#define SLIMPORT_LOG(fmt, arg...)  \
	do { \
		if (slimport_log_on) pr_info("[SLIMPORT_Chip_HAL]%s,%d,", __func__, __LINE__); pr_info(fmt, ##arg); \
	}while (0)

#define SLIMPORT_FUNC()    \
	do { \
		if (slimport_log_on) pr_info("[SLIMPORT_Chip_HAL] %s\n", __func__); \
	}while (0)

void slimport_drv_log_enable(bool enable)
{
	slimport_log_on = enable;
}

static int audio_enable = 0;
void slimport_drv_force_on(int from_uart_drv )
{
    SLIMPORT_LOG("slimport_drv_force_on %d\n", from_uart_drv);
}

/************************** Upper Layer To HAL*********************************/
static struct HDMI_UTIL_FUNCS slimport_util = {0};
static void slimport_drv_set_util_funcs(const struct HDMI_UTIL_FUNCS *util)
{
	memcpy(&slimport_util, util, sizeof(struct HDMI_UTIL_FUNCS));
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
		case SLIMPORT_CABLE:
			return "SLIMPORT_CABLE";
		default:
			SLIMPORT_LOG("Unknow MHL Cable Type\n");
			return "Unknow MHL Cable Type\n";
	}	
}

enum HDMI_CABLE_TYPE Connect_cable_type = SLIMPORT_CABLE;
static unsigned int HDCP_Supported_Info = 130;
/*
static void slimport_drv_get_params(struct HDMI_PARAMS *params)
{
	char *str = NULL;
	
	memset(params, 0, sizeof(struct HDMI_PARAMS));
    params->init_config.vformat         = HDMI_VIDEO_1280x720p_60Hz;
    params->init_config.aformat         = HDMI_AUDIO_44K_2CH;

	params->clk_pol           			= HDMI_POLARITY_FALLING;
	params->de_pol            			= HDMI_POLARITY_RISING;
	params->vsync_pol         			= HDMI_POLARITY_RISING;
	params->hsync_pol        		    = HDMI_POLARITY_RISING;

	params->hsync_front_porch 			= 110;
	params->hsync_pulse_width 			= 40;
	params->hsync_back_porch  			= 220;

	params->vsync_front_porch 			= 5;
	params->vsync_pulse_width 			= 5;
	params->vsync_back_porch  			= 20;

	params->rgb_order         			= HDMI_COLOR_ORDER_RGB;

	params->io_driving_current 			= IO_DRIVING_CURRENT_2MA;
	params->intermediat_buffer_num 		= 4;
	params->scaling_factor 				= 0;
	params->cabletype 					= Connect_cable_type;
	params->HDCPSupported 				= HDCP_Supported_Info;

	str = cable_type_print(params->cabletype);
    SLIMPORT_LOG("type %s-%d hdcp %d-%d\n", str, Connect_cable_type, params->HDCPSupported, HDCP_Supported_Info);
	return ;
}
*/
static void slimport_drv_get_params(struct HDMI_PARAMS *params)
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
		default:
			SLIMPORT_LOG("Unknow support resolution\n");
			break;
	}

    params->init_config.aformat         = HDMI_AUDIO_44K_2CH;
	params->rgb_order         			= HDMI_COLOR_ORDER_RGB;
	params->io_driving_current 			= IO_DRIVING_CURRENT_2MA;
	params->intermediat_buffer_num 		= 4;
    params->scaling_factor 				= 0;
    params->cabletype 					= Connect_cable_type;
	params->HDCPSupported 				= HDCP_Supported_Info;
/*#ifdef CONFIG_MTK_HDMI_3D_SUPPORT*/	
	params->is_3d_support 				= SlimPort_3D_Support;
/*#endif*/
	cable_str = cable_type_print(params->cabletype);
    SLIMPORT_LOG("type %s-%d hdcp %d-%d, 3d-%d\n", cable_str, Connect_cable_type, params->HDCPSupported, HDCP_Supported_Info, SlimPort_3D_Support);
}

void slimport_drv_suspend(void) {return ;} 
void slimport_drv_resume(void)  {return ;} 
static int slimport_drv_audio_config(enum HDMI_AUDIO_FORMAT aformat, int bitWidth)
{
	struct AudioFormat Audio_format;
	memset(&Audio_format, 0, sizeof(Audio_format));

	pr_info("slimport_drv_audio_config format %d, bitwidth: %d\n", aformat, bitWidth);
	Audio_format.bAudio_word_len = AUDIO_W_LEN_16_20MAX;
	
	if(bitWidth == 24)
		Audio_format.bAudio_word_len = AUDIO_W_LEN_24_24MAX;

	switch(aformat)
	{
		case HDMI_AUDIO_32K_2CH:
		{
			Audio_format.bAudio_Fs = AUDIO_FS_32K;
			Audio_format.bI2S_FORMAT.Channel_Num = I2S_CH_2;
			break;
		}
		case HDMI_AUDIO_44K_2CH:
		{
			Audio_format.bAudio_Fs = AUDIO_FS_441K;
			Audio_format.bI2S_FORMAT.Channel_Num = I2S_CH_2;
			break;
		}
		case HDMI_AUDIO_48K_2CH: 
		{
			Audio_format.bAudio_Fs = AUDIO_FS_48K;
			Audio_format.bI2S_FORMAT.Channel_Num = I2S_CH_2;
			break;
		}
		case HDMI_AUDIO_96K_2CH: 
		{
			Audio_format.bAudio_Fs = AUDIO_FS_96K;
			Audio_format.bI2S_FORMAT.Channel_Num = I2S_CH_2;
			break;
		}
		case HDMI_AUDIO_192K_2CH: 
		{
			Audio_format.bAudio_Fs = AUDIO_FS_192K;
			Audio_format.bI2S_FORMAT.Channel_Num = I2S_CH_2;
			break;
		}
		case HDMI_AUDIO_32K_8CH: 
		{
			Audio_format.bAudio_Fs = AUDIO_FS_32K;
			Audio_format.bI2S_FORMAT.Channel_Num = I2S_CH_8;
			break;
		}
		case HDMI_AUDIO_44K_8CH: 
		{
			Audio_format.bAudio_Fs = AUDIO_FS_441K;
			Audio_format.bI2S_FORMAT.Channel_Num = I2S_CH_8;
			break;
		}
		case HDMI_AUDIO_48K_8CH:
		{
			Audio_format.bAudio_Fs = AUDIO_FS_48K;
			Audio_format.bI2S_FORMAT.Channel_Num = I2S_CH_8;
			break;
		}
		case HDMI_AUDIO_96K_8CH:
		{
			Audio_format.bAudio_Fs = AUDIO_FS_96K;
			Audio_format.bI2S_FORMAT.Channel_Num = I2S_CH_8;
			break;
		}
		case HDMI_AUDIO_192K_8CH:
		{
			Audio_format.bAudio_Fs = AUDIO_FS_192K;
			Audio_format.bI2S_FORMAT.Channel_Num = I2S_CH_8;
			break;
		}
		default:
		{
			pr_info("slimport_drv_audio_config do not support");
			break;
		}
	}
	
	if(Audio_format.bI2S_FORMAT.Channel_Num == I2S_CH_2)
		Audio_format.bI2S_FORMAT.AUDIO_LAYOUT = I2S_LAYOUT_0;
	else
		Audio_format.bI2S_FORMAT.AUDIO_LAYOUT = I2S_LAYOUT_1;
	Audio_format.bAudioType = AUDIO_I2S;

	update_audio_format_setting(Audio_format.bAudio_Fs, Audio_format.bAudio_word_len, Audio_format.bI2S_FORMAT.Channel_Num, Audio_format.bI2S_FORMAT.AUDIO_LAYOUT);
	return 0;
}

static int slimport_drv_video_enable(bool enable) 
{
    return 0;
}

static int slimport_drv_audio_enable(bool enable)  
{
	pr_info("[EXTD]Set_I2S_Pin, enable = %d\n", enable);
	if (enable)
		i2s_gpio_ctrl(1);
	else
		i2s_gpio_ctrl(0);
    audio_enable = enable;
    return 0;
}

static int slimport_drv_enter(void)  {return 0;}
static int slimport_drv_exit(void)  {return 0;}

static int slimport_drv_video_config(enum HDMI_VIDEO_RESOLUTION vformat, enum HDMI_VIDEO_INPUT_FORMAT vin, int vout)
{
#ifdef dp_to_do
	if(vformat == HDMI_VIDEO_720x480p_60Hz)
	{
		SLIMPORT_LOG("[slimport_drv]480p\n");
		siHdmiTx_VideoSel(HDMI_480P60_4X3);
	}
	else if(vformat == HDMI_VIDEO_1280x720p_60Hz)
	{
		SLIMPORT_LOG("[slimport_drv]720p\n");
		siHdmiTx_VideoSel(HDMI_720P60);
	}
	else if(vformat == HDMI_VIDEO_1920x1080p_30Hz)
	{
		SLIMPORT_LOG("[slimport_drv]1080p_30 %p\n", si_dev_context);
		siHdmiTx_VideoSel(HDMI_1080P30);
	}
	else if(vformat == HDMI_VIDEO_1920x1080p_60Hz)
	{
		SLIMPORT_LOG("[slimport_drv]1080p_60 %p\n", si_dev_context);
		siHdmiTx_VideoSel(HDMI_1080P60);
	}
	else
	{
		SLIMPORT_LOG("%s, video format not support now\n", __func__);
	}


    if(si_dev_context)
    	si_mhl_tx_set_path_en_I(si_dev_context);
#endif

	if(vout & HDMI_VOUT_FORMAT_3D_SBS)
        SlimPort_3D_format=VIDEO_3D_SIDE_BY_SIDE;
	else if(vout & HDMI_VOUT_FORMAT_3D_TAB)
	    SlimPort_3D_format=VIDEO_3D_TOP_AND_BOTTOM;
    else
	    SlimPort_3D_format = VIDEO_3D_NONE;

	SLIMPORT_LOG("SlimPort_3D_format: %d\n", SlimPort_3D_format);
	update_video_format_setting(SlimPort_3D_format);

    return 0;
}

static unsigned int sii_mhl_connected = 0;
static uint8_t ReadConnectionStatus(void)
{
    return (sii_mhl_connected == SLIMPORT_TX_EVENT_CALLBACK)? 1 : 0;
}
enum HDMI_STATE slimport_drv_get_state(void)
{
	int ret = ReadConnectionStatus();
	SLIMPORT_LOG("ret: %d\n", ret);
	
	if(ret == 1)
		ret = HDMI_STATE_ACTIVE;
	else
		ret = HDMI_STATE_NO_DEVICE;

	return ret;
}

bool chip_inited = false;
/*extern int __init anx7805_init(void);*/
static int slimport_drv_init(void)
{
	SLIMPORT_LOG("slimport_drv_init--\n");

///	Mask_MHL_Intr();
	/*cust_hdmi_power_on(true);*/
    slimport_anx7805_init();

	txInitFlag = 0;
	chip_inited = false;
    SLIMPORT_LOG("slimport_drv_init -\n" );
    return 0;
}

//Should be enhanced
int	chip_device_id = 0;
bool need_reset_usb_switch = true;
int slimport_drv_power_on(void)
{
    int ret = 1;
	SLIMPORT_FUNC();

	/*
	cust_hdmi_power_on(true);	
	cust_hdmi_dpi_gpio_on(true);    
    //cust_hdmi_i2s_gpio_on(true);   
      */

#ifdef CONFIG_MTK_LEGACY     
		/*cust_hdmi_power_on(true);*/	
		/*cust_hdmi_dpi_gpio_on(true);*/  
#else
		dpi_gpio_ctrl(1);
		i2s_gpio_ctrl(1);
#endif	

	if(txInitFlag == 0)
	{
		txInitFlag = 1;
	}
	
    if(chip_device_id >0)
        ret = 0;
	
	///Unmask_MHL_Intr();
    SLIMPORT_LOG("status %d, chipid: %x, ret: %d--%d\n", ReadConnectionStatus() , chip_device_id, ret, need_reset_usb_switch);        

    return ret;
}

void slimport_drv_power_off(void)
{
	SLIMPORT_FUNC();
	/*
    cust_hdmi_dpi_gpio_on(false);
    if(audio_enable == 0)
	    cust_hdmi_i2s_gpio_on(false);
	*/

#ifdef CONFIG_MTK_LEGACY      
	if(audio_enable == 0)
		cust_hdmi_i2s_gpio_on(false);
#else
	dpi_gpio_ctrl(0);
	i2s_gpio_ctrl(0);
#endif

    return ;
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
                pal_resulution |= SINK_1080P60;
            	break;
            case 0x4:
            	pal_resulution |= SINK_720P60;
            	break;
            case 0x3:
            case 0x2:
            	pal_resulution |= SINK_480P;
            	break;
			default:
				SLIMPORT_LOG("param1: %d\n", param1);
    	}
	}	
	return ;
}
unsigned int si_mhl_get_av_info(void)
{
    unsigned int temp = SINK_1080P30;
    
    if(pal_resulution&SINK_1080P60)
        pal_resulution &= (~temp);

#ifdef MHL_RESOLUTION_LIMIT_720P_60
	pal_resulution &= (~SINK_1080P60);
	pal_resulution &= (~SINK_1080P30);
#endif

    return pal_resulution;
}
void reset_av_info(void)
{
    pal_resulution = 0;
}
void slimport_GetEdidInfo(void *pv_get_info)
{
    struct HDMI_EDID_INFO_T *ptr = (struct HDMI_EDID_INFO_T *)pv_get_info; 
    if(ptr)
    {
        ptr->ui4_ntsc_resolution = 0;
		ptr->ui4_pal_resolution = si_mhl_get_av_info();
		if(ptr->ui4_pal_resolution == 0)
		{
			SLIMPORT_LOG("MHL edid parse error \n");
	        ptr->ui4_pal_resolution = SINK_720P60 | SINK_1080P30 | SINK_480P;
		}
		SLIMPORT_LOG("EDID:0x%x\n", ptr->ui4_pal_resolution);
    }
}


extern uint8_t  Cap_MAX_channel;
extern uint16_t Cap_SampleRate;
extern uint8_t  Cap_Samplebit;
enum
{
	HDMI_CHANNEL_2 = 0x2,
	HDMI_CHANNEL_3 = 0x3,
	HDMI_CHANNEL_4 = 0x4,
	HDMI_CHANNEL_5 = 0x5,
	HDMI_CHANNEL_6 = 0x6,
	HDMI_CHANNEL_7 = 0x7,
	HDMI_CHANNEL_8 = 0x8,
};

enum
{
	HDMI_SAMPLERATE_32  = 0x1,
	HDMI_SAMPLERATE_44  = 0x2,
	HDMI_SAMPLERATE_48  = 0x3,
	HDMI_SAMPLERATE_96  = 0x4,
	HDMI_SAMPLERATE_192 = 0x5,
};

enum
{
	HDMI_BITWIDTH_16  = 0x1,
	HDMI_BITWIDTH_24  = 0x2,
	HDMI_BITWIDTH_20  = 0x3,
};

enum
{
	AUDIO_32K_2CH		= 0x01,
	AUDIO_44K_2CH		= 0x02,
	AUDIO_48K_2CH		= 0x03,
	AUDIO_96K_2CH		= 0x05,
	AUDIO_192K_2CH	    = 0x07,
	AUDIO_32K_8CH		= 0x81,
	AUDIO_44K_8CH		= 0x82,
	AUDIO_48K_8CH		= 0x83,
	AUDIO_96K_8CH		= 0x85,
	AUDIO_192K_8CH	    = 0x87,
	AUDIO_INITIAL		= 0xFF
};

int slimport_drv_get_external_device_capablity(void)
{
	int capablity = 0;
	/*capablity = HDMI_CHANNEL_8 << 3 | HDMI_SAMPLERATE_192 << 7 | HDMI_BITWIDTH_24 << 10;
	return capablity;*/
	/*
	* bit3-bit6: channal count; bit7-bit9: sample rate; bit10-bit11: bitwidth
	*/
	SLIMPORT_LOG("Cap_MAX_channel: %d, Cap_Samplebit: %d, Cap_SampleRate: %d\n", Cap_MAX_channel, Cap_Samplebit, Cap_SampleRate);
	capablity = Cap_MAX_channel << 3 | Cap_SampleRate << 7 | Cap_Samplebit << 10;
	if(capablity == 0)
	{
		capablity = HDMI_CHANNEL_2 << 3 | HDMI_SAMPLERATE_44 << 7 |  HDMI_BITWIDTH_16 << 10;
	}
	return capablity;
}

#define SLIMPORT_MAX_INSERT_CALLBACK   10
static CABLE_INSERT_CALLBACK slimport_callback_table[SLIMPORT_MAX_INSERT_CALLBACK];
void slimport_register_cable_insert_callback(CABLE_INSERT_CALLBACK cb)
{
    int i = 0;
    for (i = 0; i < SLIMPORT_MAX_INSERT_CALLBACK; i++) {
        if (slimport_callback_table[i] == cb)
            break;
    }
    if (i < SLIMPORT_MAX_INSERT_CALLBACK)
        return;

    for (i = 0; i < SLIMPORT_MAX_INSERT_CALLBACK; i++) {
        if (slimport_callback_table[i] == NULL)
            break;
    }
    if (i == SLIMPORT_MAX_INSERT_CALLBACK) {
        SLIMPORT_LOG("not enough mhl callback entries for module\n");
        return;
    }

    slimport_callback_table[i] = cb;
	SLIMPORT_LOG("callback: %p,i: %d\n", slimport_callback_table[i], i);
    return;
}

void slimport_unregister_cable_insert_callback(CABLE_INSERT_CALLBACK cb)
{
    int i;
    for (i=0; i<SLIMPORT_MAX_INSERT_CALLBACK; i++)
    {
        if (slimport_callback_table[i] == cb)
        {
        	SLIMPORT_LOG("unregister cable insert callback: %p, i: %d\n", slimport_callback_table[i], i);
            slimport_callback_table[i] = NULL;
            break;
        }
    }
    if (i == SLIMPORT_MAX_INSERT_CALLBACK)
    {
        SLIMPORT_LOG("Try to unregister callback function 0x%lx which was not registered\n",(unsigned long int)cb);
        return;
    }
    return;
}

void slimport_invoke_cable_callbacks(enum HDMI_STATE state)
{
    int i = 0, j = 0;   
    for (i=0; i<SLIMPORT_MAX_INSERT_CALLBACK; i++)   // 0 is for external display
    {
        if(slimport_callback_table[i])
        {
			j = i;
        }
    }

	if (slimport_callback_table[j])
	{
		SLIMPORT_LOG("callback: %p, state: %d, j: %d\n", slimport_callback_table[j], state, j);
		slimport_callback_table[j](state);
	}
}

static void slimport_drv_dump_register(void)
{
	SP_CTRL_Dump_Reg();
}

/************************** ****************************************************/
const struct HDMI_DRIVER *HDMI_GetDriver(void)
{
	static const struct HDMI_DRIVER HDMI_DRV =
	{
		.set_util_funcs = slimport_drv_set_util_funcs,
		.get_params     = slimport_drv_get_params,
		.init           = slimport_drv_init,
        .enter          = slimport_drv_enter,
        .exit           = slimport_drv_exit,
		.suspend        = slimport_drv_suspend,
		.resume         = slimport_drv_resume,
        .video_config   = slimport_drv_video_config,
        .audio_config   = slimport_drv_audio_config,
        .video_enable   = slimport_drv_video_enable,
        .audio_enable   = slimport_drv_audio_enable,
        .power_on       = slimport_drv_power_on,
        .power_off      = slimport_drv_power_off,
        .get_state      = slimport_drv_get_state,
        .log_enable     = slimport_drv_log_enable,
        .getedid        = slimport_GetEdidInfo,
        .get_external_device_capablity = slimport_drv_get_external_device_capablity,
		.register_callback   = slimport_register_cable_insert_callback,
		.unregister_callback = slimport_unregister_cable_insert_callback,
        .force_on = slimport_drv_force_on,
        .dump = slimport_drv_dump_register,
	};

    	SLIMPORT_FUNC();
	return &HDMI_DRV;
}

/************************** *****************************************************/

/************************** MHL TX Chip User Layer To HAL*******************************/
static char* MHL_TX_Event_Print(unsigned int event)
{
	switch(event)
	{
		case SLIMPORT_TX_EVENT_CONNECTION:
			return "SLIMPORT_TX_EVENT_CONNECTION";
		case SLIMPORT_TX_EVENT_DISCONNECTION:
			return "SLIMPORT_TX_EVENT_DISCONNECTION";
		case SLIMPORT_TX_EVENT_HPD_CLEAR:
			return "SLIMPORT_TX_EVENT_HPD_CLEAR";
		case SLIMPORT_TX_EVENT_HPD_GOT:
			return "SLIMPORT_TX_EVENT_HPD_GOT";
		case SLIMPORT_TX_EVENT_DEV_CAP_UPDATE:
			return "SLIMPORT_TX_EVENT_DEV_CAP_UPDATE";
		case SLIMPORT_TX_EVENT_EDID_UPDATE:
			return "SLIMPORT_TX_EVENT_EDID_UPDATE";
		case SLIMPORT_TX_EVENT_EDID_DONE:
			return "SLIMPORT_TX_EVENT_EDID_DONE";
		default:
			SLIMPORT_LOG("Unknow SLIMPORT TX Event Type\n");
			return "Unknow SLIMPORT TX Event Type\n";
	}	
}

void Notify_AP_MHL_TX_Event(unsigned int event, unsigned int event_param, void *param)
{
	if(event != SLIMPORT_TX_EVENT_SMB_DATA)
		SLIMPORT_LOG("%s, event_param: %d\n", MHL_TX_Event_Print(event), event_param);
	switch(event)
	{
		case SLIMPORT_TX_EVENT_CONNECTION:
			break;
		case SLIMPORT_TX_EVENT_DISCONNECTION:
		{
			sii_mhl_connected = SLIMPORT_TX_EVENT_DISCONNECTION;
			slimport_invoke_cable_callbacks(HDMI_STATE_NO_DEVICE);
			reset_av_info();
			Connect_cable_type = MHL_CABLE;
		}
			break;
		case SLIMPORT_TX_EVENT_HPD_CLEAR:
		{
			sii_mhl_connected= SLIMPORT_TX_EVENT_DISCONNECTION;
			slimport_invoke_cable_callbacks(HDMI_STATE_NO_DEVICE);
		}
			break;
		case SLIMPORT_TX_EVENT_HPD_GOT:
			break;
		case SLIMPORT_TX_EVENT_DEV_CAP_UPDATE:
		{
    			Connect_cable_type = MHL_SMB_CABLE;			
		}
			break;
		case SLIMPORT_TX_EVENT_EDID_UPDATE:
		{
			update_av_info_edid(true, event_param, 0);
		}
			break;
		case SLIMPORT_TX_EVENT_EDID_DONE:
		{
            /*if(chip_device_id == 0x8346)*/
			HDCP_Supported_Info = 130; ///HDCP 1.3 for slimport
			sii_mhl_connected = SLIMPORT_TX_EVENT_CALLBACK;
			slimport_invoke_cable_callbacks(HDMI_STATE_ACTIVE);
		}
			break;
		default:
			return ;
	}

	return ;
}
/************************** ****************************************************/
#endif
