#if defined(CONFIG_MTK_HDMI_SUPPORT)
#include <linux/kernel.h>

#include <linux/xlog.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#include <mtk_kpd.h>        /* custom file */
#include "si_timing_defs.h"

#include "hdmi_drv.h"
#include "smartbook.h"

#include "hdmi_cust.h"
/**
* MHL TX Chip Driver User Layer Interface
*/
extern struct mhl_dev_context *si_dev_context;
extern void ForceSwitchToD3( struct mhl_dev_context *dev_context);
extern void	ForceNotSwitchToD3();
extern int si_mhl_tx_post_initialize(struct mhl_dev_context *dev_context, bool bootup);
extern void siHdmiTx_VideoSel (int vmode);
extern void siHdmiTx_AudioSel (int AduioMode);
extern void set_platform_bitwidth(int bitWidth);
extern bool si_mhl_tx_set_path_en_I(struct mhl_dev_context *dev_context);
extern bool packed_pixel_available(struct mhl_dev_context *dev_context);
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
extern Mask_MHL_Intr(void);
extern Unmask_MHL_Intr(void);
extern int32_t sii_8348_tx_init(void);    //Should  move to MHL TX Chip user layer
/**
* LOG For MHL TX Chip HAL
*/
static size_t hdmi_log_on = true;
static int txInitFlag = 0;

#define HDMI_LOG(fmt, arg...)  \
	do { \
		if (hdmi_log_on) printk("[HDMI_Chip_HAL]%s,%d,", __func__, __LINE__); printk(fmt, ##arg); \
	}while (0)

#define HDMI_FUNC()    \
	do { \
		if(hdmi_log_on) printk("[HDMI_Chip_HAL] %s\n", __func__); \
	}while (0)

void hdmi_drv_log_enable(bool enable)
{
	hdmi_log_on = enable;
}

static int not_switch_to_d3 = 0;
static int audio_enable = 0;

void hdmi_drv_force_on(int from_uart_drv )
{
    HDMI_LOG("hdmi_drv_force_on %d\n", from_uart_drv);
    if(from_uart_drv == 0)
        ForceNotSwitchToD3();
    not_switch_to_d3 = 1;
//gpio:uart
    cust_hdmi_i2s_gpio_on(2);
}
 
/************************** Upper Layer To HAL*********************************/
static HDMI_UTIL_FUNCS hdmi_util = {0};
static void hdmi_drv_set_util_funcs(const HDMI_UTIL_FUNCS *util)
{
	memcpy(&hdmi_util, util, sizeof(HDMI_UTIL_FUNCS));
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
			HDMI_LOG("Unknow MHL Cable Type\n");
			return "Unknow MHL Cable Type\n";
	}	
}

static HDMI_CABLE_TYPE MHL_Connect_type = MHL_CABLE;
static bool HDCP_Supported_Info = false;
static void hdmi_drv_get_params(HDMI_PARAMS *params)
{
	memset(params, 0, sizeof(HDMI_PARAMS));
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
    params->cabletype 					= MHL_Connect_type;
	params->HDCPSupported 				= HDCP_Supported_Info;

    HDMI_LOG("type %s\n", cable_type_print(params->cabletype));
	return ;
}

void hdmi_drv_suspend(void) {return ;}
void hdmi_drv_resume(void)  {return ;}
static int hdmi_drv_audio_config(HDMI_AUDIO_FORMAT aformat, int bitWidth)
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
    printk("[EXTD]Set_I2S_Pin, enable = %d\n", enable);            
    //gpio:uart
    if(not_switch_to_d3 == 1)
        cust_hdmi_i2s_gpio_on(2);
    else
        cust_hdmi_i2s_gpio_on(enable);

    audio_enable = enable;
    return 0;
}

static int hdmi_drv_enter(void)  {return 0;}
static int hdmi_drv_exit(void)  {return 0;}

static int hdmi_drv_video_config(HDMI_VIDEO_RESOLUTION vformat, HDMI_VIDEO_INPUT_FORMAT vin, HDMI_VIDEO_OUTPUT_FORMAT vout)
{
	if(vformat == HDMI_VIDEO_720x480p_60Hz)
	{
		HDMI_LOG("[hdmi_drv]480p\n");
		siHdmiTx_VideoSel(HDMI_480P60_4X3);
	}
	else if(vformat == HDMI_VIDEO_1280x720p_60Hz)
	{
		HDMI_LOG("[hdmi_drv]720p\n");
		siHdmiTx_VideoSel(HDMI_720P60);
	}
	else if(vformat == HDMI_VIDEO_1920x1080p_30Hz)
	{
		HDMI_LOG("[hdmi_drv]1080p_30 %p\n", si_dev_context);
		siHdmiTx_VideoSel(HDMI_1080P30);
	}
	else if(vformat == HDMI_VIDEO_1920x1080p_60Hz)
	{
		HDMI_LOG("[hdmi_drv]1080p_60 %p\n", si_dev_context);
		siHdmiTx_VideoSel(HDMI_1080P60);
	}
	else
	{
		HDMI_LOG("%s, video format not support now\n", __func__);
	}
	
    if(si_dev_context)
    	si_mhl_tx_set_path_en_I(si_dev_context);
    return 0;
}

static unsigned int sii_mhl_connected = 0;
static uint8_t ReadConnectionStatus(void)
{
    return (sii_mhl_connected == MHL_TX_EVENT_CALLBACK)? 1 : 0;
}
HDMI_STATE hdmi_drv_get_state(void)
{
	int ret = ReadConnectionStatus();
	HDMI_LOG("ret: %d\n", ret);
	
	if(ret == 1)
		return HDMI_STATE_ACTIVE;
	else
		return HDMI_STATE_NO_DEVICE;
}

bool chip_inited = false;
static int hdmi_drv_init(void)
{
    HDMI_LOG("hdmi_drv_init +\n" );

	Mask_MHL_Intr();
	cust_hdmi_power_on(true);
	if(not_switch_to_d3 == 0)
    {
        HalOpenI2cDevice("Sil_MHL", "sii8348drv");
	}
	
	txInitFlag = 0;
	chip_inited = false;
    HDMI_LOG("hdmi_drv_init -\n" );
    return 0;
}

//Should be enhanced
int	chip_device_id = 0;
bool need_reset_usb_switch = true;
int hdmi_drv_power_on(void)
{
    int ret = 1;
	HDMI_FUNC();

    if(not_switch_to_d3 > 0)
    {
        HDMI_LOG("hdmi_drv_power_on direct to exit for forceon(%d_\n", not_switch_to_d3 );
        return ;
    }
    
	cust_hdmi_power_on(true);	
	cust_hdmi_dpi_gpio_on(true);    
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
	
	///Unmask_MHL_Intr();
    HDMI_LOG("status %d, chipid: %x, ret: %d--%d\n", ReadConnectionStatus() , chip_device_id, ret, need_reset_usb_switch);        

    return ret;
}

void hdmi_drv_power_off(void)
{

	HDMI_FUNC();
	
    if(not_switch_to_d3 > 0)
    {
        HDMI_LOG("hdmi_drv_power_off direct to exit for forceon(%d_\n", not_switch_to_d3 );
        return ;
    }

    cust_hdmi_dpi_gpio_on(false);
    if(audio_enable == 0)
	    cust_hdmi_i2s_gpio_on(false);

  	return ;
	
	Mask_MHL_Intr();

    if(ReadConnectionStatus()==1){
        need_reset_usb_switch = true;
    	 ForceSwitchToD3(si_dev_context);
    }
    else
        need_reset_usb_switch = false;

	cust_hdmi_power_on(false);
	chip_inited = false;
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
			default:
				HDMI_LOG("param1: %d\n", param1);
    	}
	}	

	return ;
}
unsigned int si_mhl_get_av_info()
{
    unsigned int temp = SINK_1080P30;
    
    if(pal_resulution&SINK_1080P60)
        pal_resulution &= (~temp);
        
    return pal_resulution;
}
void reset_av_info()
{
    pal_resulution = 0;
}
void hdmi_GetEdidInfo(void *pv_get_info)
{
    HDMI_EDID_INFO_T *ptr = (HDMI_EDID_INFO_T *)pv_get_info;
    if(ptr)
    {
        ptr->ui4_ntsc_resolution = 0;
		ptr->ui4_pal_resolution = si_mhl_get_av_info();
		if(ptr->ui4_pal_resolution == 0)
		{
			HDMI_LOG("MHL edid parse error \n");
			
	        if(si_dev_context && packed_pixel_available(si_dev_context))
	            ptr->ui4_pal_resolution = SINK_720P60 | SINK_1080P60 | SINK_480P;
	        else
	            ptr->ui4_pal_resolution = SINK_720P60 | SINK_1080P30 | SINK_480P;
		}
    }

    if(si_dev_context)
    {
        HDMI_LOG("MHL hdmi_GetEdidInfo ntsc 0x%x,pal: 0x%x, packed: %d, parsed 0x%x\n", ptr->ui4_ntsc_resolution  , 
        	ptr->ui4_pal_resolution, packed_pixel_available(si_dev_context), si_mhl_get_av_info());
    }
}


extern uint8_t  Cap_MAX_channel;
extern uint16_t Cap_SampleRate;
extern uint8_t  Cap_Samplebit;

int hdmi_drv_get_external_device_capablity(void)
{
	HDMI_LOG("Cap_MAX_channel: %d, Cap_Samplebit: %d, Cap_SampleRate: %d\n", Cap_MAX_channel, Cap_Samplebit, Cap_SampleRate);
	int capablity = Cap_MAX_channel << 3 | Cap_SampleRate << 7 | Cap_Samplebit << 10;

	if(capablity == 0)
	{
		capablity = HDMI_CHANNEL_2 << 3 | HDMI_SAMPLERATE_44 << 7 |  HDMI_BITWIDTH_16 << 10;
	}

	return capablity;
}

const HDMI_DRIVER* HDMI_GetDriver(void)
{
	static const HDMI_DRIVER HDMI_DRV =
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
        .force_on = hdmi_drv_force_on,
	};

    HDMI_FUNC();
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
			HDMI_LOG("Unknow MHL TX Event Type\n");
			return "Unknow MHL TX Event Type\n";
	}	
}

extern void hdmi_state_callback(HDMI_STATE state);
void Notify_AP_MHL_TX_Event(unsigned int event, unsigned int event_param, void *param)
{
	if(event != MHL_TX_EVENT_SMB_DATA)
		HDMI_LOG("%s, event_param: %d\n", MHL_TX_Event_Print(event), event_param);
	switch(event)
	{
		case MHL_TX_EVENT_CONNECTION:
			break;
		case MHL_TX_EVENT_DISCONNECTION:
		{
			sii_mhl_connected = MHL_TX_EVENT_DISCONNECTION;
			hdmi_state_callback(HDMI_STATE_NO_DEVICE);
			reset_av_info();
			SMB_Denit();
			MHL_Connect_type = MHL_CABLE;
		}
			break;
		case MHL_TX_EVENT_HPD_CLEAR:
		{
			sii_mhl_connected= MHL_TX_EVENT_DISCONNECTION;
			hdmi_state_callback(HDMI_STATE_NO_DEVICE);
		}
			break;
		case MHL_TX_EVENT_HPD_GOT:
			break;
		case MHL_TX_EVENT_DEV_CAP_UPDATE:
		{
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
#ifdef HDCP_ENABLE
			HDCP_Supported_Info = true;
#endif
			sii_mhl_connected = MHL_TX_EVENT_CALLBACK;
			hdmi_state_callback(HDMI_STATE_ACTIVE);
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
