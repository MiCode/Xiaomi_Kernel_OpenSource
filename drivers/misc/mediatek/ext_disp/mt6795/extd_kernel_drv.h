// ---------------------------------------------------------------------------

#ifndef     HDMITX_DRV_H
#define     HDMITX_DRV_H

#define HDMI_CHECK_RET(expr)                                                \
    do {                                                                    \
        HDMI_STATUS ret = (expr);                                           \
        if (HDMI_STATUS_OK != ret) {                                        \
            printk("[ERROR][mtkfb] HDMI API return error code: 0x%x\n"      \
                   "  file : %s, line : %d\n"                               \
                   "  expr : %s\n", ret, __FILE__, __LINE__, #expr);        \
        }                                                                   \
    } while (0)

enum HDMI_report_state
{
    NO_DEVICE = 0,
    HDMI_PLUGIN = 1,
};

typedef enum
{
    HDMI_CHARGE_CURRENT,

} HDMI_QUERY_TYPE;

typedef enum
{
    Plugout  = 0,
    Plugin ,
    ResChange,
    Devinfo,
    Power_on = 4,
    Power_off,
    Config,
    Trigger = 7
} Extd_State;

typedef enum
{
    insert_Buffer_Err1 = 0xeff0,
    insert_Buffer_Err2 ,
    insert_Buffer_Err3 ,
    insert_Buffer_Err4,
    insert_Buffer_Err5,
    Buffer_INFO_Err,  ///5
    Timeline_Err,
    Buffer_Not_Enough,   ///7
    Buffer_Empt_Err,
    Fence_Err,  ///9
    Mutex_Err1,
    Mutex_Err2,
    Mutex_Err3,
    Buff_Dup_Err1, ///0xeffd
    Buff_ION_Err1

} HDMI_MMP_Err;


int get_hdmi_dev_info(HDMI_QUERY_TYPE type);
bool is_hdmi_active(void);
int get_extd_fps_time(void);
void hdmi_suspend(void);
void hdmi_resume(void);
void hdmi_power_on(void);
void hdmi_power_off(void);
void hdmi_update(void);
void hdmi_dpi_power_switch(bool enable);
int hdmi_audio_config(int samplerate);
int hdmi_video_enable(bool enable);
int hdmi_audio_enable(bool enable);
int hdmi_audio_delay_mute(int latency);
void hdmi_set_mode(unsigned char ucMode);
void hdmi_reg_dump(void);
void hdmi_waitVsync(void);
void hdmi_smb_kpd_disable(void);

int _get_ext_disp_info(void *info);

#ifdef MTK_MT8193_HDMI_SUPPORT
void hdmi_read_reg(unsigned char u8Reg, unsigned int *p4Data);
#else
void hdmi_read_reg(unsigned char u8Reg);
#endif
void hdmi_write_reg(unsigned char u8Reg, unsigned char u8Data);
#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
void smartbook_state_callback();
#endif
#endif
