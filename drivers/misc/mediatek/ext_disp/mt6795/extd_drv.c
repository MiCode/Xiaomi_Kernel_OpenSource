/*****************************************************************************/
/* Copyright (c) 2009 NXP Semiconductors BV                                  */
/*                                                                           */
/* This program is free software; you can redistribute it and/or modify      */
/* it under the terms of the GNU General Public License as published by      */
/* the Free Software Foundation, using version 2 of the License.             */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,           */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the              */
/* GNU General Public License for more details.                              */
/*                                                                           */
/* You should have received a copy of the GNU General Public License         */
/* along with this program; if not, write to the Free Software               */
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307       */
/* USA.                                                                      */
/*                                                                           */
/*****************************************************************************/
#if defined(CONFIG_MTK_HDMI_SUPPORT)
#define TMFL_TDA19989

#define _tx_c_
///#include <linux/autoconf.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/earlysuspend.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/vmalloc.h>
#include <linux/disp_assert_layer.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/switch.h>


#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
#ifdef CONFIG_HAS_SBSUSPEND
#include <linux/sbsuspend.h>
#endif
#endif
#include <asm/uaccess.h>
#include <asm/atomic.h>
///#include <asm/mach-types.h>
#include <asm/cacheflush.h>
#include <asm/io.h>
#include <mach/dma.h>
#include <mach/irqs.h>
#include <asm/tlbflush.h>
#include <asm/page.h>

#include <mach/m4u.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_clkmgr.h>
#include <mach/mt_boot.h>
#include "mach/eint.h"
#include "mach/irqs.h"

#include "mtkfb_info.h"
#include "mtkfb.h"
#include "disp_session.h"

#include "ddp_dpi.h"
#include "ddp_info.h"
#include "ddp_rdma.h"
#include "ddp_irq.h"
#include "ddp_mmp.h"

#include "extd_platform.h"
#include "extd_drv.h"
#include "extd_kernel_drv.h"
#include "extd_drv_log.h"
#include "extd_utils.h"
#include "extd_ddp.h"

#include "hdmi_drv.h"

#ifdef CONFIG_COMPAT
#include <linux/uaccess.h>
#include <linux/compat.h>
#endif


#ifdef MTK_EXT_DISP_SYNC_SUPPORT
#include "display_recorder.h"
#include "mtkfb_fence.h"
#endif

#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
#include "smartbook.h"
#endif

#ifdef I2C_DBG
#include "tmbslHdmiTx_types.h"
#include "tmbslTDA9989_local.h"
#endif

#include <linux/earlysuspend.h>
#include <linux/suspend.h>


#define HDMI_DEVNAME "hdmitx"

#define HW_OVERLAY_COUNT (4)
#define RETIF(cond, rslt)       if ((cond)){HDMI_LOG("return in %d\n",__LINE__);return (rslt);}
#define RET_VOID_IF(cond)       if ((cond)){HDMI_LOG("return in %d\n",__LINE__);return;}
#define RETIF_NOLOG(cond, rslt)       if ((cond)){return (rslt);}
#define RET_VOID_IF_NOLOG(cond)       if ((cond)){return;}
#define RETIFNOT(cond, rslt)    if (!(cond)){HDMI_LOG("return in %d\n",__LINE__);return (rslt);}


#define HDMI_DPI(suffix)        DPI  ## suffix
#define HMID_DEST_DPI           DISP_MODULE_DPI
static int hdmi_bpp = 4;

spinlock_t hdmi_lock;
DEFINE_SPINLOCK(hdmi_lock);

static bool factory_mode = false;

#define ALIGN_TO(x, n)  \
    (((x) + ((n) - 1)) & ~((n) - 1))
#define hdmi_abs(a) (((a) < 0) ? -(a) : (a))

extern const HDMI_DRIVER *HDMI_GetDriver(void);
extern void HDMI_DBG_Init(void);
extern void mmdvfs_mhl_enable(int enable);


static int hdmi_log_on = 1;
static int hdmi_bufferdump_on = 1;
static int hdmi_hwc_on = 1;

static unsigned long hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
static unsigned long force_reschange = 0xffff;



static struct switch_dev hdmi_switch_data;
static struct switch_dev hdmires_switch_data;
HDMI_PARAMS _s_hdmi_params = {0};
HDMI_PARAMS *hdmi_params = &_s_hdmi_params;
static HDMI_DRIVER *hdmi_drv = NULL;

void hdmi_log_enable(int enable)
{
    printk("hdmi log %s\n", enable ? "enabled" : "disabled");
    hdmi_log_on = enable;
    hdmi_drv->log_enable(enable);
}

void hdmi_mmp_enable(int enable)
{
    printk("hdmi log %s\n", enable ? "enabled" : "disabled");
    hdmi_bufferdump_on = enable;
}

void hdmi_hwc_enable(int enable)
{
    printk("hdmi log %s\n", enable ? "enabled" : "disabled");
    hdmi_hwc_on = enable;
}

static DEFINE_SEMAPHORE(hdmi_update_mutex);
typedef struct
{
    bool is_reconfig_needed;    // whether need to reset HDMI memory
    bool is_enabled;    // whether HDMI is enabled or disabled by user
    bool is_force_disable;      //used for camera scenario.
    bool is_clock_on;   // DPI is running or not
    bool is_mhl_video_on;   // DPI is running or not
    atomic_t state; // HDMI_POWER_STATE state
    int     lcm_width;  // LCD write buffer width
    int     lcm_height; // LCD write buffer height
    bool    lcm_is_video_mode;
    int     hdmi_width; // DPI read buffer width
    int     hdmi_height; // DPI read buffer height
    int     bg_width; // DPI read buffer width
    int     bg_height; // DPI read buffer height
    HDMI_VIDEO_RESOLUTION       output_video_resolution;
    HDMI_AUDIO_FORMAT           output_audio_format;
    int     orientation;    // MDP's orientation, 0 means 0 degree, 1 means 90 degree, 2 means 180 degree, 3 means 270 degree
    HDMI_OUTPUT_MODE    output_mode;
    int     scaling_factor;
} _t_hdmi_context;



static _t_hdmi_context hdmi_context;
static _t_hdmi_context *p = &hdmi_context;
static struct list_head  HDMI_Buffer_List;

#define IS_HDMI_ON()            (HDMI_POWER_STATE_ON == atomic_read(&p->state))
#define IS_HDMI_OFF()           (HDMI_POWER_STATE_OFF == atomic_read(&p->state))
#define IS_HDMI_STANDBY()       (HDMI_POWER_STATE_STANDBY == atomic_read(&p->state))

#define IS_HDMI_NOT_ON()        (HDMI_POWER_STATE_ON != atomic_read(&p->state))
#define IS_HDMI_NOT_OFF()       (HDMI_POWER_STATE_OFF != atomic_read(&p->state))
#define IS_HDMI_NOT_STANDBY()   (HDMI_POWER_STATE_STANDBY != atomic_read(&p->state))

#define SET_HDMI_ON()           atomic_set(&p->state, HDMI_POWER_STATE_ON)
#define SET_HDMI_OFF()          atomic_set(&p->state, HDMI_POWER_STATE_OFF)
#define SET_HDMI_STANDBY()      atomic_set(&p->state, HDMI_POWER_STATE_STANDBY)


#define IS_HDMI_FAKE_PLUG_IN()  (true == atomic_read(&hdmi_fake_in))
#define SET_HDMI_FAKE_PLUG_IN() (atomic_set(&hdmi_fake_in, true))
#define SET_HDMI_NOT_FAKE()     (atomic_set(&hdmi_fake_in, false))

int hdmi_allocate_hdmi_buffer(void);
int hdmi_free_hdmi_buffer(void);


#ifdef MTK_HDMI_SCREEN_CAPTURE
bool capture_screen = false;
unsigned long capture_addr;
#endif
unsigned int hdmi_va, hdmi_mva_r;


static dev_t hdmi_devno;
static struct cdev *hdmi_cdev;
static struct class *hdmi_class = NULL;
#ifdef CONFIG_COMPAT
static long hdmi_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
#endif
static long hdmi_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int hdmi_open(struct inode *inode, struct file *file);
static int hdmi_release(struct inode *inode, struct file *file);
static int hdmi_probe(struct platform_device *pdev);
static int hdmi_remove(struct platform_device *pdev);
 
#ifdef CONFIG_PM
int hdmi_pm_suspend(struct device *device)
{
    printk("hdmi_pm_suspend()\n");

    return 0;
}

int hdmi_pm_resume(struct device *device)
{
    printk("HDMI_Npm_resume()\n");

    return 0;
}

struct dev_pm_ops hdmi_pm_ops = {
    .suspend =  hdmi_pm_suspend,
    .resume =   hdmi_pm_resume,
};
#endif

static bool otg_enable_status = false;

#ifdef CONFIG_HAS_EARLYSUSPEND
static void hdmi_early_suspend(struct early_suspend *h)
{
    printk(" hdmi_early_suspend \n");
    RETIF(!p->is_enabled, 0);

    hdmi_power_off();
    mmdvfs_mhl_enable(false);
    switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);
}

static void hdmi_late_resume(struct early_suspend *h)
{
	printk(" hdmi_late_resume\n");   
	
	RETIF(!p->is_enabled, 0);
	
    RETIF(otg_enable_status, 0);
    hdmi_power_on();
}

static struct early_suspend hdmi_early_suspend_handler =
{
	.level =    EARLY_SUSPEND_LEVEL_BLANK_SCREEN +1 ,
	.suspend =  hdmi_early_suspend,
	.resume =   hdmi_late_resume,
};
#endif


static const struct file_operations hdmi_fops =
{
    .owner   = THIS_MODULE,
    .unlocked_ioctl   = hdmi_ioctl,
#ifdef CONFIG_COMPAT    
    .compat_ioctl   = hdmi_compat_ioctl,  
#endif    
    .open    = hdmi_open,
    .release = hdmi_release,
};

static const struct of_device_id hdmi_of_ids[] = {
	{.compatible = "mediatek,HDMI", },
	{}
};

#if 0 ///def CONFIG_MTK_HDMI_SUPPORT
static struct platform_device mtk_hdmi_dev = {
    .name = "hdmitx",
    .id   = 0,
};
#endif

static struct platform_driver hdmi_driver =
{
    .probe  = hdmi_probe,
    .remove = hdmi_remove,
    .driver = { 
        .name = HDMI_DEVNAME,
#ifdef CONFIG_PM
        .pm     = &hdmi_pm_ops,
#endif
        .of_match_table = hdmi_of_ids,
    }
};

#include <linux/mmprofile.h>
#include "display_recorder.h"

disp_ddp_path_config extd_dpi_params;

struct task_struct *hdmi_rdma_config_task = NULL;
wait_queue_head_t hdmi_rdma_config_wq;
atomic_t hdmi_rdma_config_event = ATOMIC_INIT(0);

static unsigned int hdmi_resolution_param_table[][3] =
{
    {720,   480,    60},
    {1280,  720,    60},
    {1920,  1080,   30},
    {1920,  1080,   60},
};

static unsigned int ovl_config_address[4];
#define ENABLE_HDMI_BUFFER_LOG 1
#if ENABLE_HDMI_BUFFER_LOG
bool enable_hdmi_buffer_log = 0;
#define HDMI_BUFFER_LOG(fmt, arg...) \
    do { \
        if(enable_hdmi_buffer_log){printk("[hdmi_buffer] "); printk(fmt, ##arg);} \
    }while (0)
#else
bool enable_hdmi_buffer_log = 0;
#define HDMI_BUFFER_LOG(fmt, arg...)
#endif

static wait_queue_head_t hdmi_vsync_wq;
static bool hdmi_vsync_flag = false;
static int hdmi_vsync_cnt = 0;
static atomic_t hdmi_fake_in = ATOMIC_INIT(false);
static int rdmafpscnt = 0;
struct timer_list timer;

int get_hdmi_support_info(void)
{
	int value = 0, temp =0;
	
#ifdef USING_SCALE_ADJUSTMENT
	value |= HDMI_SCALE_ADJUSTMENT_SUPPORT;
#endif

#ifdef MHL_PHONE_GPIO_REUSAGE
	value |= HDMI_PHONE_GPIO_REUSAGE;
#endif

#ifdef MTK_AUDIO_MULTI_CHANNEL_SUPPORT
	temp = hdmi_drv->get_external_device_capablity();
#else
    temp = 0x2<<3;
#endif

	value |= temp;
	
	HDMI_LOG("value is 0x%x\n", value);
	return value;
}

static void hdmi_udelay(unsigned int us)
{
    udelay(us);
}

static void hdmi_mdelay(unsigned int ms)
{
    msleep(ms);
}

unsigned int hdmi_get_width()
{
    return p->hdmi_width;
}

unsigned int hdmi_get_height()
{
    return p->hdmi_height;
}



// For Debugfs
void hdmi_cable_fake_plug_in(void)
{
    SET_HDMI_FAKE_PLUG_IN();
    HDMI_LOG("[HDMIFake]Cable Plug In\n");

    if (p->is_force_disable == false)
    {
        if (IS_HDMI_STANDBY())
        {
            hdmi_resume();
            ///msleep(1000);
            hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
            switch_set_state(&hdmi_switch_data, HDMI_STATE_ACTIVE);            
        }
    }
}

// For Debugfs
void hdmi_cable_fake_plug_out(void)
{
    SET_HDMI_NOT_FAKE();
    HDMI_LOG("[HDMIFake]Disable\n");

    if (p->is_force_disable == false)
    {
        if (IS_HDMI_ON())
        {
            if (hdmi_drv->get_state() != HDMI_STATE_ACTIVE)
            {
                hdmi_suspend();
                switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);
                switch_set_state(&hdmires_switch_data, 0);
            }
        }
    }
}

void hdmi_force_resolution(int res)
{
    HDMI_LOG("hdmi_force_resolution %d\n", res);

    force_reschange = res;
}

int get_hdmi_dev_info(HDMI_QUERY_TYPE type)
{
    switch (type)
    {
        case HDMI_CHARGE_CURRENT:
        {
            if ((p->is_enabled == false)
                    || hdmi_params->cabletype == HDMI_CABLE)
            {
                return 0;
            }
            else if (hdmi_params->cabletype == MHL_CABLE)
            {
                return 500;
            }
            else if (hdmi_params->cabletype == MHL_2_CABLE)
            {
                return 900;
            }

        }

        default:
            return 0;
    }

}


void hdmi_waitVsync(void)
{
    unsigned int session_id = ext_disp_get_sess_id(); 
    disp_session_sync_info *session_info = disp_get_session_sync_info_for_debug(session_id);

    if(session_info)
    {
        dprec_start(&session_info->event_waitvsync, 0, 0);
    }

    if (p->is_clock_on == false)
    {
        printk("[hdmi]:hdmi has suspend, return directly\n");
        msleep(19);
        return;
    }

    hdmi_vsync_cnt++;

    hdmi_vsync_flag = 0;
    if (wait_event_interruptible_timeout(hdmi_vsync_wq, hdmi_vsync_flag, HZ / 10) == 0)
    {
        printk("[hdmi] Wait VSync timeout. early_suspend=%d\n", p->is_clock_on);
    }

    if(session_info)
	{
		dprec_done(&session_info->event_waitvsync, hdmi_vsync_cnt, 0);
	}
	
    hdmi_vsync_cnt--;
    return;
}

bool is_hdmi_active(void)
{
    return (IS_HDMI_ON()&&p->is_clock_on && p->is_enabled );
}

int get_extd_fps_time(void)
{

    if(hdmi_reschange == HDMI_VIDEO_1920x1080p_30Hz)
        return 34000;
    else
        return 16700;

}
static void _hdmi_rdma_irq_handler(DISP_MODULE_ENUM module, unsigned int param)
{
    RET_VOID_IF_NOLOG(!is_hdmi_active());
    
    if (param & 0x2) //  start 
    {
        ///MMProfileLogEx(ddp_mmp_get_events()->Extd_IrqStatus, MMProfileFlagPulse, module, param);

        atomic_set(&hdmi_rdma_config_event, 1);
        wake_up_interruptible(&hdmi_rdma_config_wq);

        if(hdmi_params->cabletype == MHL_SMB_CABLE)
        {
            hdmi_vsync_flag = 1;
            wake_up_interruptible(&hdmi_vsync_wq);
        }
    }

}

int hdmi_video_config(HDMI_VIDEO_RESOLUTION vformat, HDMI_VIDEO_INPUT_FORMAT vin, HDMI_VIDEO_OUTPUT_FORMAT vout);
extern int is_dim_layer(unsigned int long mva);

static int hdmi_rdma_config_kthread(void *data)
{
    struct sched_param param = { .sched_priority = RTPM_PRIO_SCRN_UPDATE };
    sched_setscheduler(current, SCHED_RR, &param);
    int layid = 0;
    unsigned int session_id = 0;
    int fence_idx = 0;
    bool ovl_reg_updated = false;
    unsigned long lay_addr = 0;
    

    for (;;)
    {
        wait_event_interruptible(hdmi_rdma_config_wq, atomic_read(&hdmi_rdma_config_event));
        atomic_set(&hdmi_rdma_config_event, 0);
        ovl_reg_updated = false;
        
        session_id = ext_disp_get_sess_id(); 
        fence_idx = -1;
        if(get_ext_disp_path_mode() == EXTD_DEBUG_RDMA_DPI_MODE)
        {
            fence_idx = disp_sync_find_fence_idx_by_addr(session_id, 0, ddp_dpi_get_cur_addr(true, 0));
            mtkfb_release_fence(session_id, 0, fence_idx);
        }
        else
        {
            for(layid = 0; layid < HW_OVERLAY_COUNT; layid++)
            {
                if(ovl_config_address[layid] != ddp_dpi_get_cur_addr(false, layid))
                {
                    ovl_config_address[layid] = ddp_dpi_get_cur_addr(false, layid);
                    ovl_reg_updated = true;                    
                }
                lay_addr = ddp_dpi_get_cur_addr(false, layid);
                if(is_dim_layer(lay_addr))
                {
                    lay_addr = 0;
                }             
                fence_idx = disp_sync_find_fence_idx_by_addr(session_id, layid, lay_addr);
                mtkfb_release_fence(session_id, layid, fence_idx);
            }

            if(ovl_reg_updated == false)
                MMProfileLogEx(ddp_mmp_get_events()->Extd_trigger, MMProfileFlagPulse, ddp_dpi_get_cur_addr(false, 0), ddp_dpi_get_cur_addr(false, 1));

            MMProfileLogEx(ddp_mmp_get_events()->Extd_UsedBuff, MMProfileFlagPulse, ddp_dpi_get_cur_addr(false, 0), ddp_dpi_get_cur_addr(false, 1));
        }

        rdmafpscnt++;
        hdmi_video_config(p->output_video_resolution, HDMI_VIN_FORMAT_RGB888, HDMI_VOUT_FORMAT_RGB888);        
        
        if (kthread_should_stop())
        {
            break;
        }
    }

    return 0;
}


/* Allocate memory, set M4U, LCD, MDP, DPI */
/* LCD overlay to memory -> MDP resize and rotate to memory -> DPI read to HDMI */
/* Will only be used in ioctl(MTK_HDMI_AUDIO_VIDEO_ENABLE) */
static HDMI_STATUS hdmi_drv_init(void)
{
    int lcm_width, lcm_height;
    int tmpBufferSize;
    M4U_PORT_STRUCT m4uport;

    HDMI_FUNC();

    p->hdmi_width = hdmi_resolution_param_table[hdmi_params->init_config.vformat][0]; ///hdmi_get_width(hdmi_params->init_config.vformat);
    p->hdmi_height = hdmi_resolution_param_table[hdmi_params->init_config.vformat][1]; ///hdmi_get_height(hdmi_params->init_config.vformat);
    p->bg_width = 0;
    p->bg_height = 0;

    p->output_video_resolution = hdmi_params->init_config.vformat;
    p->output_audio_format = hdmi_params->init_config.aformat;
    p->scaling_factor = hdmi_params->scaling_factor < 10 ? hdmi_params->scaling_factor : 10;

    ///ddp_dpi_init(DISP_MODULE_DPI, 0);

    hdmi_dpi_power_switch(false);   // but dpi power is still off        
    
    if (!hdmi_rdma_config_task)
    {
        disp_register_module_irq_callback(DISP_MODULE_RDMA2, _hdmi_rdma_irq_handler);
        disp_register_module_irq_callback(DISP_MODULE_RDMA1, _hdmi_rdma_irq_handler);
        hdmi_rdma_config_task = kthread_create(hdmi_rdma_config_kthread, NULL, "hdmi_rdma_config_kthread");
        wake_up_process(hdmi_rdma_config_task);
    }


    return HDMI_STATUS_OK;
}

/* Release memory */
/* Will only be used in ioctl(MTK_HDMI_AUDIO_VIDEO_ENABLE) */
static  HDMI_STATUS hdmi_drv_deinit(void)
{
    int temp_va_size;

    HDMI_FUNC();

    hdmi_dpi_power_switch(false);
    hdmi_free_hdmi_buffer();

    return HDMI_STATUS_OK;
} 

#ifdef EXTD_DBG_USE_INNER_BUF
extern unsigned char kara_1280x720[2764800];
#endif

int hdmi_allocate_hdmi_buffer(void)
{   
    M4U_PORT_STRUCT m4uport;
    int ret = 0;
    int hdmiPixelSize = p->hdmi_width * p->hdmi_height;
    int hdmiDataSize = hdmiPixelSize * 4;////hdmi_bpp;
    int hdmiBufferSize = hdmiDataSize * 1; ///hdmi_params->intermediat_buffer_num;
    
    HDMI_FUNC();
#ifdef EXTD_DBG_USE_INNER_BUF
   
    RETIF(hdmi_va, 0);

    hdmi_va = (unsigned int) vmalloc(hdmiBufferSize); 
    if (((void *) hdmi_va) == NULL)
    {
        HDMI_LOG("vmalloc %d bytes fail!!!\n", hdmiBufferSize);
        return -1;
    }

    m4u_client_t *client = NULL;
    client = m4u_create_client();
    if (IS_ERR_OR_NULL(client))
    {
        HDMI_LOG("create client fail!\n");
    }
    
    ret = m4u_alloc_mva(client, M4U_PORT_DISP_OVL1, hdmi_va, 0, hdmiBufferSize, M4U_PROT_READ |M4U_PROT_WRITE, 0, &hdmi_mva_r);

    memcpy(hdmi_va, kara_1280x720, 2764800);
    HDMI_LOG("hdmi_va=0x%08x, hdmi_mva_r=0x%08x, size %d\n", hdmi_va, hdmi_mva_r, hdmiBufferSize);
#endif     
    return 0;

}


int hdmi_free_hdmi_buffer(void)
{
    int hdmi_va_size = p->hdmi_width * p->hdmi_height * hdmi_bpp * hdmi_params->intermediat_buffer_num;
    return;
}

/* Switch DPI Power for HDMI Driver */
/*static*/ void hdmi_dpi_power_switch(bool enable)
{
    int ret = 0;
    int i= 0;
    int session_id = ext_disp_get_sess_id();

    HDMI_LOG("hdmi_dpi_power_switch, current state: %d  -> target state: %d\n",p->is_clock_on, enable);

    if (enable)
    {
        if (p->is_clock_on == true)
        {
            HDMI_LOG("power on request while already powered on!\n");
            return;
        }

        ext_disp_resume();
        
        p->is_clock_on = true;
    }
    else
    {
        p->is_clock_on = false;
        ext_disp_suspend();
        if(IS_HDMI_ON())
        {
            for (i = 0; i < HW_OVERLAY_COUNT; i++)        
            {            
                mtkfb_release_layer_fence(session_id, i);
            }
        }


    }
}

/* Configure video attribute */
int hdmi_video_config(HDMI_VIDEO_RESOLUTION vformat, HDMI_VIDEO_INPUT_FORMAT vin, HDMI_VIDEO_OUTPUT_FORMAT vout)
{
    if(p->is_mhl_video_on == true)
        return 0;
        
    HDMI_LOG("hdmi_video_config video_on=%d fps %d\n", p->is_mhl_video_on, rdmafpscnt );
    RETIF(IS_HDMI_NOT_ON(), 0);

    hdmi_allocate_hdmi_buffer();
    p->is_mhl_video_on = true;

    if(IS_HDMI_FAKE_PLUG_IN())
        return 0;
        
    return hdmi_drv->video_config(vformat, vin, vout);
}

/* Configure audio attribute, will be called by audio driver */
/*
int hdmi_audio_config(int samplerate)
{
    HDMI_FUNC();
    RETIF(!p->is_enabled, 0);
    RETIF(IS_HDMI_NOT_ON(), 0);

    HDMI_LOG("sample rate=%d\n", samplerate);

    if (samplerate == 48000)
    {
        p->output_audio_format = HDMI_AUDIO_48K_2CH;
    }
    else if (samplerate == 44100)
    {
        p->output_audio_format = HDMI_AUDIO_44K_2CH;
    }
    else if (samplerate == 32000)
    {
        p->output_audio_format = HDMI_AUDIO_32K_2CH;
    }
    else
    {
        HDMI_LOG("samplerate not support:%d\n", samplerate);
    }


    hdmi_drv->audio_config(p->output_audio_format, 16);

    return 0;
}
*/

/* No one will use this function */
/*static*/ int hdmi_video_enable(bool enable)
{
    HDMI_FUNC();

    return hdmi_drv->video_enable(enable);
}

/* No one will use this function */
int hdmi_audio_enable(bool enable)
{
    HDMI_FUNC();

    return hdmi_drv->audio_enable(enable);
}


/* Reset HDMI Driver state */
static void hdmi_state_reset(void)
{
    HDMI_FUNC();

    if (hdmi_drv->get_state() == HDMI_STATE_ACTIVE)
    {
        switch_set_state(&hdmi_switch_data, HDMI_STATE_ACTIVE);
        hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
        mmdvfs_mhl_enable(true); 
    }
    else
    {
        switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);
        switch_set_state(&hdmires_switch_data, 0);
        mmdvfs_mhl_enable(false); 
    }
}


void hdmi_smb_kpd_enable(void)
{
	HDMI_LOG("hdmi_smb_kpd_enable performed! \n");	
}

void hdmi_smb_kpd_disable(void)
{
    HDMI_LOG("hdmi_smb_kpd_disable performed -state(%d) \n", atomic_read(&p->state));
    if(IS_HDMI_STANDBY()&&(hdmi_params->cabletype == MHL_SMB_CABLE) )
    {    
#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
#ifdef CONFIG_HAS_SBSUSPEND
        sb_plug_out();
#endif
#endif

    }
    
}


#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
#ifdef CONFIG_HAS_SBSUSPEND

static struct sb_handler hdmi_smb_handler_desc = {
	.level		= SB_LEVEL_DISABLE_TOUCH,
	.plug_in	= hdmi_smb_kpd_enable,
	.plug_out	= hdmi_smb_kpd_disable,
};
#endif
#endif

/* HDMI Driver state callback function */
void hdmi_state_callback(HDMI_STATE state)
{

    printk("[hdmi]%s, state = %d\n", __func__, state);

    RET_VOID_IF((p->is_force_disable == true));
    RET_VOID_IF(IS_HDMI_FAKE_PLUG_IN());

    switch (state)
    {
        case HDMI_STATE_NO_DEVICE:
        {
            hdmi_suspend();
            switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);
            switch_set_state(&hdmires_switch_data, 0);
            mmdvfs_mhl_enable(false); 
            break;
        }

        case HDMI_STATE_ACTIVE:
        {

            if(IS_HDMI_ON())
            {
                printk("[hdmi]%s, already on(%d) !\n", __func__, atomic_read(&p->state));
                break;
            }

            hdmi_drv->get_params(hdmi_params);
            hdmi_resume();

            if(atomic_read(&p->state) > HDMI_POWER_STATE_OFF)
            {
                switch_set_state(&hdmi_switch_data, HDMI_STATE_ACTIVE);
                mmdvfs_mhl_enable(true); 
            }

#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
#ifdef CONFIG_HAS_SBSUSPEND
            if (hdmi_params->cabletype == MHL_SMB_CABLE)
            {
                sb_plug_in();
            }
#endif
#endif            
            hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
            break;
        }

        default:
        {
            printk("[hdmi]%s, state not support\n", __func__);
            break;
        }
    }

    return;
}


/*static*/ void hdmi_power_on(void)
{
    HDMI_FUNC();

    RET_VOID_IF(IS_HDMI_NOT_OFF());

    if (down_interruptible(&hdmi_update_mutex))
    {
        printk("[hdmi][HDMI] can't get semaphore in %s()\n", __func__);
        return;
    }

    SET_HDMI_STANDBY();

    hdmi_drv->power_on();
    
    up(&hdmi_update_mutex);


    if (p->is_force_disable == false)
    {
        if (IS_HDMI_FAKE_PLUG_IN())
        {
            //FixMe, deadlock may happened here, due to recursive use mutex
            hdmi_resume();
            msleep(1000);
            switch_set_state(&hdmi_switch_data, HDMI_STATE_ACTIVE);
            hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
        }
        else
        {  
            // this is just a ugly workaround for some tv sets...
            if(hdmi_drv->get_state() == HDMI_STATE_ACTIVE)/// && (factory_mode == true))
            {
				hdmi_drv->get_params(hdmi_params);
				hdmi_resume();
			}
            hdmi_state_reset();
        }
    }


    return;
}

/*static*/ void hdmi_power_off(void)
{
    HDMI_FUNC();

    switch_set_state(&hdmires_switch_data, 0);

    RET_VOID_IF(IS_HDMI_OFF());

    if (down_interruptible(&hdmi_update_mutex))
    {
        printk("[hdmi][HDMI] can't get semaphore in %s()\n", __func__);
        return;
    }

    hdmi_drv->power_off();

    ///ext_disp_suspend();
    hdmi_dpi_power_switch(false);
    SET_HDMI_OFF();
    up(&hdmi_update_mutex);

    return;
}

/*static*/ void hdmi_suspend(void)
{
    HDMI_FUNC();
    RET_VOID_IF(IS_HDMI_NOT_ON());

    if (hdmi_bufferdump_on > 0)
    {
        MMProfileLogEx(ddp_mmp_get_events()->Extd_State, MMProfileFlagStart, Plugout, 0);
    }

    if (down_interruptible(&hdmi_update_mutex))
    {
        printk("[hdmi][HDMI] can't get semaphore in %s()\n", __func__);
        return;
    }

    //SET_HDMI_STANDBY();
    hdmi_drv->suspend();
    p->is_mhl_video_on = false;
   
    hdmi_dpi_power_switch(false);
    SET_HDMI_STANDBY();
    ///ext_disp_suspend();
    
    ///disp_module_clock_off(DISP_MODULE_RDMA2, "HDMI");
    up(&hdmi_update_mutex);

    ext_disp_deinit(NULL);
    
    if (hdmi_bufferdump_on > 0)
    {
        MMProfileLogEx(ddp_mmp_get_events()->Extd_State, MMProfileFlagEnd, Plugout, 0);
    }
}

/*static*/ void hdmi_resume(void)
{
    HDMI_FUNC();
    HDMI_LOG("p->state is %d,(0:off, 1:on, 2:standby)\n", atomic_read(&p->state));

    RET_VOID_IF(IS_HDMI_NOT_STANDBY());
    RET_VOID_IF(IS_HDMI_ON());

    SET_HDMI_ON();

    if (hdmi_bufferdump_on > 0)
    {
        MMProfileLogEx(ddp_mmp_get_events()->Extd_State, MMProfileFlagStart, Plugin, 0);
    }

    if (down_interruptible(&hdmi_update_mutex))
    {
        printk("[hdmi][HDMI] can't get semaphore in %s()\n", __func__);
        return;
    }
    
    hdmi_dpi_power_switch(true);
    ///ext_disp_resume();
    hdmi_drv->resume();
    up(&hdmi_update_mutex);

    if (hdmi_bufferdump_on > 0)
    {
        MMProfileLogEx(ddp_mmp_get_events()->Extd_State, MMProfileFlagEnd, Plugin, 0);
    }
}

static int hdmi_release(struct inode *inode, struct file *file)
{
    HDMI_FUNC();
    return 0;
}

static int hdmi_open(struct inode *inode, struct file *file)
{
    HDMI_FUNC();
    return 0;
}

static BOOL hdmi_drv_init_context(void);

static void dpi_setting_res(u8 arg)
{
    DPI_POLARITY clk_pol, de_pol, hsync_pol, vsync_pol;
    unsigned int dpi_clock = 0;  ///khz
    unsigned int dpi_clk_div, dpi_clk_duty, hsync_pulse_width, hsync_back_porch, hsync_front_porch, vsync_pulse_width, vsync_back_porch, vsync_front_porch, intermediat_buffer_num;

    switch (arg)
    {
        case HDMI_VIDEO_720x480p_60Hz:
        {

#if defined(MHL_SII8338) || defined(MHL_SII8348)
            clk_pol     = HDMI_POLARITY_FALLING;
#else
            clk_pol     = HDMI_POLARITY_RISING;
#endif
            de_pol      = HDMI_POLARITY_RISING;
            hsync_pol   = HDMI_POLARITY_RISING;
            vsync_pol   = HDMI_POLARITY_RISING;;

            dpi_clk_div = 2;

            hsync_pulse_width   = 62;
            hsync_back_porch    = 60;
            hsync_front_porch   = 16;

            vsync_pulse_width   = 6;
            vsync_back_porch    = 30;
            vsync_front_porch   = 9;

            p->bg_height = ((480 * p->scaling_factor) / 100 >> 2) << 2 ;
            p->bg_width = ((720 * p->scaling_factor) / 100 >> 2) << 2 ;
            p->hdmi_width = 720 - p->bg_width;
            p->hdmi_height = 480 - p->bg_height;
            p->output_video_resolution = HDMI_VIDEO_720x480p_60Hz;
            dpi_clock = 27027;
            break;
        }

        case HDMI_VIDEO_1280x720p_60Hz:
        {

            clk_pol     = HDMI_POLARITY_RISING;
            de_pol      = HDMI_POLARITY_RISING;
#if defined(HDMI_TDA19989)
            hsync_pol   = HDMI_POLARITY_FALLING;
#else
            hsync_pol   = HDMI_POLARITY_FALLING;
#endif
            vsync_pol   = HDMI_POLARITY_FALLING;

#if defined(MHL_SII8338) || defined(MHL_SII8348)
            clk_pol     = HDMI_POLARITY_FALLING;
            de_pol      = HDMI_POLARITY_RISING;
            hsync_pol   = HDMI_POLARITY_FALLING;
            vsync_pol   = HDMI_POLARITY_FALLING;
#endif

            dpi_clk_div = 2;

            hsync_pulse_width   = 40;
            hsync_back_porch    = 220;
            hsync_front_porch   = 110;

            vsync_pulse_width   = 5;
            vsync_back_porch    = 20;
            vsync_front_porch   = 5;
            dpi_clock = 74250;

            p->bg_height = ((720 * p->scaling_factor) / 100 >> 2) << 2 ;
            p->bg_width = ((1280 * p->scaling_factor) / 100 >> 2) << 2 ;
            p->hdmi_width = 1280 - p->bg_width; //1280  1366
            p->hdmi_height = 720 - p->bg_height;//720;  768

#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT

            if (hdmi_params->cabletype == MHL_SMB_CABLE)
            {
                p->hdmi_width = 1366;
                p->hdmi_height = 768;
                p->bg_height = 0;
                p->bg_width = 0;
            }

#endif
            p->output_video_resolution = HDMI_VIDEO_1280x720p_60Hz;
            HDMI_LOG("dpi_setting_res height:%d cabletype %d\n", p->hdmi_height, hdmi_params->cabletype );
            break;
        }


        case HDMI_VIDEO_1920x1080p_30Hz:
        {
#if defined(MHL_SII8338) || defined(MHL_SII8348)
            clk_pol     = HDMI_POLARITY_FALLING;
            de_pol      = HDMI_POLARITY_RISING;
            hsync_pol   = HDMI_POLARITY_FALLING;
            vsync_pol   = HDMI_POLARITY_FALLING;  
#else
            clk_pol     = HDMI_POLARITY_RISING;
            de_pol      = HDMI_POLARITY_RISING;
            hsync_pol   = HDMI_POLARITY_FALLING;
            vsync_pol   = HDMI_POLARITY_FALLING;
#endif

            dpi_clk_div = 2;

            hsync_pulse_width   = 44;
            hsync_back_porch    = 148;
            hsync_front_porch   = 88;

            vsync_pulse_width   = 5;
            vsync_back_porch    = 36;
            vsync_front_porch   = 4;

            p->bg_height = ((1080 *p->scaling_factor)/100 >>2) <<2 ;
            p->bg_width = ((1920 *p->scaling_factor)/100 >>2) <<2 ;
            p->hdmi_width = 1920 -p->bg_width;
            p->hdmi_height = 1080 - p->bg_height;

            p->output_video_resolution = HDMI_VIDEO_1920x1080p_30Hz;
            dpi_clock = 74250;
            break;
        }

        case HDMI_VIDEO_1920x1080p_60Hz:
        {
#if defined(MHL_SII8338) || defined(MHL_SII8348)
            clk_pol     = HDMI_POLARITY_FALLING;
            de_pol      = HDMI_POLARITY_RISING;
            hsync_pol   = HDMI_POLARITY_FALLING;
            vsync_pol   = HDMI_POLARITY_FALLING;
#else
            clk_pol     = HDMI_POLARITY_RISING;
            de_pol      = HDMI_POLARITY_RISING;
            hsync_pol   = HDMI_POLARITY_FALLING;
            vsync_pol   = HDMI_POLARITY_FALLING;
#endif

            dpi_clk_div = 2;

            hsync_pulse_width   = 44;
            hsync_back_porch    = 148;
            hsync_front_porch   = 88;

            vsync_pulse_width   = 5;
            vsync_back_porch    = 36;
            vsync_front_porch   = 4;

            p->bg_height = ((1080 *p->scaling_factor)/100 >>2) <<2 ;
            p->bg_width = ((1920 *p->scaling_factor)/100 >>2) <<2 ;
            p->hdmi_width = 1920 -p->bg_width;
            p->hdmi_height = 1080 - p->bg_height;

            p->output_video_resolution = HDMI_VIDEO_1920x1080p_60Hz;
            dpi_clock = 148500;
            break;
        }

        default:
            break;
    }

    extd_dpi_params.dispif_config.dpi.width = p->hdmi_width;
    extd_dpi_params.dispif_config.dpi.height = p->hdmi_height;
    extd_dpi_params.dispif_config.dpi.bg_width = p->bg_width;
    extd_dpi_params.dispif_config.dpi.bg_height = p->bg_width;
    
    extd_dpi_params.dispif_config.dpi.clk_pol = clk_pol;
    extd_dpi_params.dispif_config.dpi.de_pol = de_pol;
    extd_dpi_params.dispif_config.dpi.vsync_pol = vsync_pol;
    extd_dpi_params.dispif_config.dpi.hsync_pol = hsync_pol;

    extd_dpi_params.dispif_config.dpi.hsync_pulse_width = hsync_pulse_width;
    extd_dpi_params.dispif_config.dpi.hsync_back_porch = hsync_back_porch;
    extd_dpi_params.dispif_config.dpi.hsync_front_porch = hsync_front_porch;
    extd_dpi_params.dispif_config.dpi.vsync_pulse_width = vsync_pulse_width;
    extd_dpi_params.dispif_config.dpi.vsync_back_porch = vsync_back_porch;
    extd_dpi_params.dispif_config.dpi.vsync_front_porch = vsync_front_porch;
    
    extd_dpi_params.dispif_config.dpi.format = LCM_DPI_FORMAT_RGB888;
    extd_dpi_params.dispif_config.dpi.rgb_order = LCM_COLOR_ORDER_RGB;
    extd_dpi_params.dispif_config.dpi.i2x_en = true;
    extd_dpi_params.dispif_config.dpi.i2x_edge = 2;
    extd_dpi_params.dispif_config.dpi.embsync = false;
    extd_dpi_params.dispif_config.dpi.dpi_clock = dpi_clock;

    HDMI_LOG("dpi_setting_res:%d clk_pol %d\n", arg, extd_dpi_params.dispif_config.dpi.clk_pol );
    
}


int _get_ext_disp_info(void *info)
{
	disp_session_info* dispif_info = (disp_session_info*)info;

	memset((void*)dispif_info, 0, sizeof(dispif_info));
    
	if(get_ext_disp_path_mode() == EXTD_DIRECT_LINK_MODE)
	    dispif_info->maxLayerNum = 4;
    else
	    dispif_info->maxLayerNum = 1;
	    
	dispif_info->displayFormat = DISPIF_FORMAT_RGB888;
    dispif_info->displayHeight = p->hdmi_height;
    dispif_info->displayWidth = p->hdmi_width;
    dispif_info->displayMode = DISP_IF_MODE_VIDEO;
    dispif_info->physicalWidth = 0;
    dispif_info->physicalHeight = 0;

    if (hdmi_params->cabletype == MHL_SMB_CABLE)
    {
        dispif_info->displayType = DISP_IF_HDMI_SMARTBOOK;
        dispif_info->physicalWidth = 254;
        dispif_info->physicalHeight = 145;
        if(IS_HDMI_OFF())
            dispif_info->displayType= DISP_IF_MHL;
    }
    else if (hdmi_params->cabletype == MHL_CABLE)
    {
        dispif_info->displayType = DISP_IF_MHL;
    }
    else
    {
        dispif_info->displayType = DISP_IF_HDMI;
    }

    dispif_info->isHwVsyncAvailable = 0;
    dispif_info->vsyncFPS = 60;	

	dispif_info->isConnected = 1;
	dispif_info->isHDCPSupported = (unsigned int)hdmi_params->HDCPSupported;
    ///HDMI_LOG("_get_ext_disp_info lays %d, type %d, H %d, hdcp %d\n", dispif_info->maxLayerNum , dispif_info->displayType, dispif_info->displayHeight, dispif_info->isHDCPSupported);  

}


int _ioctl_get_ext_disp_info(unsigned long arg)
{
	int ret = 0;
	void __user *argp = (void __user *)arg;
	disp_session_info info;

	if (copy_from_user(&info, argp, sizeof(info))) 
	{
		HDMI_LOG("[FB]: copy_from_user failed! line:%d \n", __LINE__);
		return -EFAULT;
	}

	_get_ext_disp_info(&info);

	if (copy_to_user(argp, &info, sizeof(info))) 
	{
		HDMI_LOG("[FB]: copy_to_user failed! line:%d \n", __LINE__);
		ret = -EFAULT;
	}
	
	return (ret);
}

void hdmi_audio_configure(int format)
{
	HDMI_AUDIO_FORMAT audio_format;
	int channel_count = format & 0xF;
	int sampleRate = (format & 0x70) >> 4;
	int bitWidth = (format & 0x180) >> 7, sampleBit;
	HDMI_LOG("channel_count: %d, sampleRate: %d, bitWidth: %d\n", channel_count, sampleRate, bitWidth);

	if(bitWidth == HDMI_MAX_BITWIDTH_16)
	{
		sampleBit = 16;
		HDMI_LOG("HDMI_MAX_BITWIDTH_16\n");
	}
	else if(bitWidth == HDMI_MAX_BITWIDTH_24)
	{
		sampleBit = 24;
		HDMI_LOG("HDMI_MAX_BITWIDTH_24\n");
	}

	if(channel_count == HDMI_MAX_CHANNEL_2 && sampleRate == HDMI_MAX_SAMPLERATE_44)
	{
		audio_format = HDMI_AUDIO_44K_2CH;
		HDMI_LOG("AUDIO_44K_2CH\n");
	}
	else if(channel_count == HDMI_MAX_CHANNEL_8 && sampleRate == HDMI_MAX_SAMPLERATE_32)
	{
		audio_format = HDMI_AUDIO_32K_8CH;
		HDMI_LOG("AUDIO_32K_8CH\n");
	}
	else if(channel_count == HDMI_MAX_CHANNEL_8 && sampleRate == HDMI_MAX_SAMPLERATE_44)
	{
		audio_format = HDMI_AUDIO_44K_8CH;
		HDMI_LOG("AUDIO_44K_8CH\n");
	}
	else if(channel_count == HDMI_MAX_CHANNEL_8 && sampleRate == HDMI_MAX_SAMPLERATE_48)
	{
		audio_format = HDMI_AUDIO_48K_8CH;
		HDMI_LOG("AUDIO_48K_8CH\n");
	}
	else if(channel_count == HDMI_MAX_CHANNEL_8 && sampleRate == HDMI_MAX_SAMPLERATE_96)
	{
		audio_format = HDMI_AUDIO_96K_8CH;
		HDMI_LOG("AUDIO_96K_8CH\n");
	}

	else if(channel_count == HDMI_MAX_CHANNEL_8 && sampleRate == HDMI_MAX_SAMPLERATE_192)
	{
		audio_format = HDMI_AUDIO_192K_8CH;
		HDMI_LOG("AUDIO_192K_8CH\n");
	}
	else
	{
		HDMI_LOG("audio format is not supported\n");
	}

    RETIF(!p->is_enabled, 0);
    hdmi_drv->audio_config(audio_format, sampleBit);
}

#ifdef CONFIG_COMPAT
static long hdmi_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    
    if (!filp->f_op || !filp->f_op->unlocked_ioctl)
        return -ENOTTY;

    switch (cmd)
    {   
        #if 0
        case MTK_HDMI_IS_FORCE_AWAKE:        
        case MTK_HDMI_GET_DEV_INFO:        
        case MTK_HDMI_GET_EDID:        
        case MTK_HDMI_GET_CAPABILITY:        
        case MTK_HDMI_FACTORY_GET_STATUS:
        {
            HDMI_LOG("TBD compat hdmi ioctl= %s(%d), arg = %lu\n", _hdmi_ioctl_spy(cmd), cmd & 0xff, arg);
            break;
        }
        #endif
		default:
		    HDMI_LOG("TBD compat hdmi ioctl= (%d), arg = %lu\n",  cmd & 0xff, arg);
		    ///ret = (-EFAULT);
            return filp->f_op->unlocked_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
	}

	return ret ;
}
#endif

static long hdmi_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    void __user *argp = (void __user *)arg;
    HDMI_EDID_T pv_get_info;
    int r = 0;

    HDMI_LOG("hdmi ioctl= %s(%d), arg = %lu\n", _hdmi_ioctl_spy(cmd), cmd & 0xff, arg);

    switch (cmd)
    {
        case MTK_HDMI_AUDIO_VIDEO_ENABLE:
        {
            if (arg)
            {
                if (p->is_enabled)
                {
                    break;
                }

                HDMI_CHECK_RET(hdmi_drv_init());

                if (hdmi_drv->enter)
                {
                    hdmi_drv->enter();
                }

                hdmi_power_on();
                p->is_enabled = true;
            }
            else
            {
                if (!p->is_enabled)
                {
                    break;
                }

                p->is_enabled = false;            

                //wait hdmi finish update
                if (down_interruptible(&hdmi_update_mutex))
                {
                    printk("[hdmi][HDMI] can't get semaphore in %s()\n", __func__);
                    return -EFAULT;
                }

                hdmi_video_buffer_info temp;
                up(&hdmi_update_mutex);
                hdmi_power_off();

                //wait hdmi finish update
                if (down_interruptible(&hdmi_update_mutex))
                {
                    printk("[hdmi][HDMI] can't get semaphore in %s()\n", __func__);
                    return -EFAULT;
                }

                HDMI_CHECK_RET(hdmi_drv_deinit());
                
                up(&hdmi_update_mutex);

                if (hdmi_drv->exit)
                {
                    hdmi_drv->exit();
                }

                //when disable hdmi, HPD is disabled
                switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);
                printk("[hdmi] done power off\n");
                
            }

            break;
        }

        case MTK_HDMI_FORCE_FULLSCREEN_ON:
            //case MTK_HDMI_FORCE_CLOSE:
        {
            RETIF(!p->is_enabled, 0);
            RETIF(IS_HDMI_OFF(), 0);

            if (p->is_force_disable == true)
            {
                break;
            }

            if (IS_HDMI_FAKE_PLUG_IN())
            {
                hdmi_suspend();
                switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);
                switch_set_state(&hdmires_switch_data, 0);
            }
            else
            {
                if (hdmi_drv->get_state() == HDMI_STATE_ACTIVE)
                {
                    hdmi_suspend();
                    switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);
                    switch_set_state(&hdmires_switch_data, 0);
                }
            }

            p->is_force_disable = true;

            break;
        }

        case MTK_HDMI_FORCE_FULLSCREEN_OFF:
            //case MTK_HDMI_FORCE_OPEN:
        {
            RETIF(!p->is_enabled, 0);
            RETIF(IS_HDMI_OFF(), 0);

            if (p->is_force_disable == false)
            {
                break;
            }

            if (IS_HDMI_FAKE_PLUG_IN())
            {
                hdmi_resume();
                msleep(1000);
                switch_set_state(&hdmi_switch_data, HDMI_STATE_ACTIVE);
                hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
            }
            else
            {
                if (hdmi_drv->get_state() == HDMI_STATE_ACTIVE)
                {
                    hdmi_resume();
                    msleep(1000);
                    switch_set_state(&hdmi_switch_data, HDMI_STATE_ACTIVE);
                    hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
                }
            }

            p->is_force_disable = false;

            break;
        }

        case MTK_HDMI_POWER_ENABLE:
        {
            RETIF(!p->is_enabled, 0);
            HDMI_LOG("hdmi ioctl(%d) arguments is not support\n",  cmd & 0xff);
            break;
            if (arg)
            {
                RETIF(otg_enable_status, 0);
                hdmi_power_on();
            }
            else
            {
                hdmi_power_off();
                switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);
            }

            break;
        }

/*
        case MTK_HDMI_USBOTG_STATUS:
        {
            HDMI_LOG("MTK_HDMI_USBOTG_STATUS, arg=%d, enable %d\n", arg, p->is_enabled);

            RETIF(!p->is_enabled, 0);
            RETIF((hdmi_params->cabletype != MHL_CABLE), 0);

            if (arg)
            {
                otg_enable_status = true;
            }
            else
            {
                otg_enable_status = false;
                RETIF(p->is_force_disable, 0);
                hdmi_power_on();
            }

            break;
        }
*/

        case MTK_HDMI_AUDIO_ENABLE:
        {
            RETIF(!p->is_enabled, 0);

            if (arg)
            {
                HDMI_CHECK_RET(hdmi_audio_enable(true));
            }
            else
            {
                HDMI_CHECK_RET(hdmi_audio_enable(false));
            }

            break;
        }

        case MTK_HDMI_VIDEO_ENABLE:
        {
            RETIF(!p->is_enabled, 0);
            break;
        }

        case MTK_HDMI_VIDEO_CONFIG:
        {      
			HDMI_LOG("video resolution configuration, arg:%ld, origial resolution:%ld, factory_mode:%d, is_video_on:%d\n",
			     arg, hdmi_reschange, factory_mode, p->is_mhl_video_on);          

            RETIF(!p->is_enabled, 0);
            RETIF(IS_HDMI_NOT_ON(), 0);

            //just for debug
            if(force_reschange < 0xff)
            {
                arg = force_reschange;
            }
            
            if (hdmi_reschange == arg)
            {
                HDMI_LOG("hdmi_reschange=%ld\n", hdmi_reschange);
                break;
            }

            hdmi_reschange = arg;
            p->is_clock_on = false;
            
            RETIF(!p->is_enabled, 0);
            RETIF(IS_HDMI_NOT_ON(), 0);
            
            if (hdmi_bufferdump_on > 0)
            {
                MMProfileLogEx(ddp_mmp_get_events()->Extd_State, MMProfileFlagStart, ResChange, arg);
            }
            
            hdmi_dpi_power_switch(false);
            if (down_interruptible(&hdmi_update_mutex))
            {
                HDMI_LOG("[HDMI] can't get semaphore in\n");
                return -EFAULT;
            }
            hdmi_video_buffer_info temp;
            dpi_setting_res((u8)arg);   
            
            p->is_mhl_video_on = false;
            
            if( factory_mode == true)
            {
                ///ext_disp_init(NULL);
                ///hdmi_dpi_power_switch(true);
                #if 0
                ddp_dpi_power_on(DISP_MODULE_DPI, NULL);
                ddp_dpi_stop(DISP_MODULE_DPI, NULL);

                ddp_dpi_config(DISP_MODULE_DPI, &extd_dpi_params, NULL);
                DPI_EnableColorBar();
                ddp_dpi_trigger(DISP_MODULE_DPI, NULL);
                #endif
                hdmi_hwc_on = 0;
            }
            else
            {
                ///hdmi_video_config(p->output_video_resolution, HDMI_VIN_FORMAT_RGB888, HDMI_VOUT_FORMAT_RGB888);
                #if 0
                ext_disp_init(NULL);

                hdmi_dpi_power_switch(true);
                ///ext_disp_resume();

                ddp_dpi_stop(DISP_MODULE_DPI, NULL);
                #endif
            }
            up(&hdmi_update_mutex);
            
            ///hdmi_video_config(p->output_video_resolution, HDMI_VIN_FORMAT_RGB888, HDMI_VOUT_FORMAT_RGB888);

            if (factory_mode == false)
            {   
                if(hdmi_hwc_on)
                {
                    switch_set_state(&hdmires_switch_data, hdmi_reschange + 1);                    
                }
            }
            p->is_clock_on = true;

            rdmafpscnt = 0;
            if (hdmi_bufferdump_on > 0)
            {
                MMProfileLogEx(ddp_mmp_get_events()->Extd_State, MMProfileFlagEnd, ResChange, hdmi_reschange + 1);
            }
            break;
        }

        case MTK_HDMI_AUDIO_CONFIG:
        {
            RETIF(!p->is_enabled, 0);

            break;
        }

        case MTK_HDMI_IS_FORCE_AWAKE:
        {
            if (!hdmi_drv_init_context())
            {
                printk("[hdmi]%s, hdmi_drv_init_context fail\n", __func__);
                return HDMI_STATUS_NOT_IMPLEMENTED;
            }

            r = copy_to_user(argp, &hdmi_params->is_force_awake, sizeof(hdmi_params->is_force_awake)) ? -EFAULT : 0;
            break;
        }
        case MTK_HDMI_FACTORY_MODE_ENABLE:
        {
            if (arg)
            {
                if (hdmi_drv->power_on())
                {
                    r = -EAGAIN;
                    HDMI_LOG("Error factory mode test fail\n"); 
                }
                else
                    ddp_dpi_init(DISP_MODULE_DPI, 0);
            }
            else
            {
                hdmi_drv->power_off(); 
                ddp_dpi_stop(DISP_MODULE_DPI, NULL);
                ddp_dpi_power_off(DISP_MODULE_DPI, NULL);                
            }

            break;
        }

        case MTK_HDMI_FACTORY_GET_STATUS:
        {
            bool hdmi_status = false;

            if (IS_HDMI_ON())
            {
                hdmi_status = true;
            }

            HDMI_LOG("MTK_HDMI_FACTORY_GET_STATUS is %d, state: %d \n", p->is_clock_on, atomic_read(&p->state));

            if (copy_to_user((void __user *)arg, &hdmi_status, sizeof(hdmi_status)))
            {
                HDMI_LOG("copy_to_user failed! line:%d \n", __LINE__);
                r = -EFAULT;
            }

            break;
        }

        case MTK_HDMI_FACTORY_DPI_TEST:
        {
            if (down_interruptible(&hdmi_update_mutex))
            {
                HDMI_LOG("[HDMI] can't get semaphore in\n");
                return EAGAIN;
            }

            ddp_dpi_power_on(DISP_MODULE_DPI, NULL);
            ddp_dpi_stop(DISP_MODULE_DPI, NULL);

            ddp_dpi_config(DISP_MODULE_DPI, &extd_dpi_params, NULL);
            DPI_EnableColorBar();
            ddp_dpi_trigger(DISP_MODULE_DPI, NULL);
            hdmi_hwc_on = 0;           

            DPI_CHECK_RET(HDMI_DPI(_EnableColorBar()));
            ddp_dpi_start(DISP_MODULE_DPI, NULL);

            up(&hdmi_update_mutex);

            if(IS_HDMI_FAKE_PLUG_IN())
            {
                HDMI_LOG("fake cable in to return line:%d \n", __LINE__);
            }
            else
            {
                msleep(50);    
                hdmi_video_config(p->output_video_resolution, HDMI_VIN_FORMAT_RGB888, HDMI_VOUT_FORMAT_RGB888);
            }
            ddp_dpi_dump(DISP_MODULE_DPI , 1);
            break;
        }

        case MTK_HDMI_GET_DEV_INFO:
        {
            int displayid = 0;
            mtk_dispif_info_t hdmi_info;

            if (hdmi_bufferdump_on > 0)
            {
                MMProfileLogEx(ddp_mmp_get_events()->Extd_DevInfo, MMProfileFlagStart, p->is_enabled, p->is_clock_on);
            }

            HDMI_LOG("DEV_INFO configuration get + \n");

            if (copy_from_user(&displayid, (void __user *)arg, sizeof(displayid)))
            {
                if (hdmi_bufferdump_on > 0)
                {
                    MMProfileLogEx(ddp_mmp_get_events()->Extd_ErrorInfo, MMProfileFlagPulse, Devinfo, 0);
                }

                HDMI_LOG(": copy_from_user failed! line:%d \n", __LINE__);
                return -EAGAIN;
            }

            if (displayid != MTKFB_DISPIF_HDMI)
            {
                HDMI_LOG(": invalid display id:%d \n", displayid);
            }

            memset(&hdmi_info, 0, sizeof(hdmi_info));
            hdmi_info.displayFormat = DISPIF_FORMAT_RGB888;
            hdmi_info.displayHeight = p->hdmi_height;
            hdmi_info.displayWidth = p->hdmi_width;
            hdmi_info.display_id = displayid;
            hdmi_info.isConnected = 1;
            hdmi_info.displayMode = DISPIF_MODE_COMMAND;

            if (hdmi_params->cabletype == MHL_SMB_CABLE)
            {
                hdmi_info.displayType = HDMI_SMARTBOOK;
                hdmi_info.physicalWidth = 254;
                hdmi_info.physicalHeight = 145;
            }
            else if (hdmi_params->cabletype == MHL_CABLE)
            {
                hdmi_info.displayType = MHL;
            }
            else
            {
                hdmi_info.displayType = HDMI;
            }

            hdmi_info.isHwVsyncAvailable = 1;
            hdmi_info.vsyncFPS = 60;

            if (copy_to_user((void __user *)arg, &hdmi_info, sizeof(hdmi_info)))
            {
                if (hdmi_bufferdump_on > 0)
                {
                    MMProfileLogEx(ddp_mmp_get_events()->Extd_ErrorInfo, MMProfileFlagPulse, Devinfo, 1);
                }

                HDMI_LOG("copy_to_user failed! line:%d \n", __LINE__);
                r = -EFAULT;
            }

            if (hdmi_bufferdump_on > 0)
            {
                MMProfileLogEx(ddp_mmp_get_events()->Extd_DevInfo, MMProfileFlagEnd, p->is_enabled, hdmi_info.displayType);
            }

            HDMI_LOG("DEV_INFO configuration get displayType-%d \n", hdmi_info.displayType);

            break;
        }
       
        case MTK_HDMI_GET_EDID:
        {   
            memset(&pv_get_info, 0, sizeof(pv_get_info));
            if(hdmi_drv->getedid)
                hdmi_drv->getedid(&pv_get_info);

            if (copy_to_user((void __user *)arg, &pv_get_info, sizeof(pv_get_info))) 
            {
               HDMI_LOG("copy_to_user failed! line:%d \n", __LINE__);
               r = -EFAULT;
            }
            
            break;
        }
        case MTK_HDMI_GET_CAPABILITY:
        {
            int query_type = 0;
            if (copy_from_user(&query_type, (void __user *)arg, sizeof(int)))
            {
                HDMI_LOG(": copy_to_user failed! line:%d \n", __LINE__);
                r = -EFAULT;
            }
			query_type = get_hdmi_support_info();

            if (copy_to_user((void __user *)arg, &query_type, sizeof(query_type)))
            {
                HDMI_LOG(": copy_to_user error! line:%d \n", __LINE__);
                r = -EFAULT;
            }
            HDMI_LOG("[hdmi][HDMI] query_type  done %x\n", query_type);                

			break;
        }
		case MTK_HDMI_AUDIO_FORMAT:
		{
            int audio_format = arg;
			
			hdmi_audio_configure(audio_format);
			break;
		}

        default:
        {
            HDMI_LOG("hdmi ioctl(%d) arguments is not support\n",  cmd & 0xff);
            r = -EFAULT;
            break;
        }
    }

    HDMI_LOG("hdmi ioctl = %s(%d) done\n",  _hdmi_ioctl_spy(cmd), cmd & 0xff);
    return r;
}


static int hdmi_remove(struct platform_device *pdev)
{
    return 0;
}

static BOOL hdmi_drv_init_context(void)
{
    static const HDMI_UTIL_FUNCS hdmi_utils =
    {
        .udelay                 = hdmi_udelay,
        .mdelay                 = hdmi_mdelay,
        .state_callback         = hdmi_state_callback,
    };

    if (hdmi_drv != NULL)
    {
        return TRUE;
    }


    hdmi_drv = (HDMI_DRIVER *)HDMI_GetDriver();

    if (NULL == hdmi_drv)
    {
        return FALSE;
    }

    hdmi_drv->set_util_funcs(&hdmi_utils);
    hdmi_drv->get_params(hdmi_params);

    return TRUE;
}

static void __exit hdmi_exit(void)
{

    device_destroy(hdmi_class, hdmi_devno);
    class_destroy(hdmi_class);
    cdev_del(hdmi_cdev);
    unregister_chrdev_region(hdmi_devno, 1);

}


static int hdmi_probe(struct platform_device *pdev)
{
    int ret = 0;
    struct class_device *class_dev = NULL;

    printk("[hdmi]%s\n", __func__);

    /* Allocate device number for hdmi driver */
    ret = alloc_chrdev_region(&hdmi_devno, 0, 1, HDMI_DEVNAME);

    if (ret)
    {
        printk("[hdmi]alloc_chrdev_region fail\n");
        return -1;
    }

    /* For character driver register to system, device number binded to file operations */
    hdmi_cdev = cdev_alloc();
    hdmi_cdev->owner = THIS_MODULE;
    hdmi_cdev->ops = &hdmi_fops;
    ret = cdev_add(hdmi_cdev, hdmi_devno, 1);

    /* For device number binded to device name(hdmitx), one class is corresponeded to one node */
    hdmi_class = class_create(THIS_MODULE, HDMI_DEVNAME);
    /* mknod /dev/hdmitx */
    class_dev = (struct class_device *)device_create(hdmi_class, NULL, hdmi_devno, NULL, HDMI_DEVNAME);

    printk("[hdmi][%s] current=%p\n", __func__, current);

    if (!hdmi_drv_init_context())
    {
        printk("[hdmi]%s, hdmi_drv_init_context fail\n", __func__);
        return HDMI_STATUS_NOT_IMPLEMENTED;
    }

    init_waitqueue_head(&hdmi_rdma_config_wq);
    init_waitqueue_head(&hdmi_vsync_wq);

    return 0;
}


static int __init hdmi_init(void)
{
    int ret = 0;
    printk("[hdmi]%s\n", __func__);

#if 0///def CONFIG_MTK_HDMI_SUPPORT
        retval = platform_device_register(&mtk_hdmi_dev);
        printk("[%s]: mtk_hdmi_dev, retval=%d \n!", __func__, retval);
        if (retval != 0){
            return retval;
        }
#endif

    if (platform_driver_register(&hdmi_driver))
    {
        printk("[hdmi]failed to register mtkfb driver\n");
        return -1;
    }

    memset((void *)&hdmi_context, 0, sizeof(_t_hdmi_context));
    memset((void *)&extd_dpi_params, 0, sizeof(extd_dpi_params));  
    
    SET_HDMI_OFF();


    if (!hdmi_drv_init_context())
    {
        printk("[hdmi]%s, hdmi_drv_init_context fail\n", __func__);
        return HDMI_STATUS_NOT_IMPLEMENTED;
    }

    p->output_mode = hdmi_params->output_mode;
    p->orientation = 0;
    p->is_mhl_video_on = false;
    hdmi_drv->init();

    HDMI_DBG_Init();

    hdmi_switch_data.name = "hdmi";
    hdmi_switch_data.index = 0;
    hdmi_switch_data.state = NO_DEVICE;

    // for support hdmi hotplug, inform AP the event
    ret = switch_dev_register(&hdmi_switch_data);

    hdmires_switch_data.name = "res_hdmi";
    hdmires_switch_data.index = 0;
    hdmires_switch_data.state = 0;

    // for support hdmi hotplug, inform AP the event
    ret = switch_dev_register(&hdmires_switch_data);

    if (ret)
    {
        printk("[hdmi][HDMI]switch_dev_register returned:%d!\n", ret);
        return 1;
    }


    int tmp_boot_mode = get_boot_mode();

    if ((tmp_boot_mode == FACTORY_BOOT) || (tmp_boot_mode == ATE_FACTORY_BOOT))
    {
        factory_mode = true;
    }

#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
#ifdef CONFIG_HAS_SBSUSPEND
	///register_sb_handler(&hdmi_smb_handler_desc);
#endif
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
        register_early_suspend(&hdmi_early_suspend_handler);
#endif 

    return 0;
}

module_init(hdmi_init);
module_exit(hdmi_exit);
MODULE_AUTHOR("www.mediatek.com>");
MODULE_DESCRIPTION("HDMI Driver");
MODULE_LICENSE("GPL");

#endif
