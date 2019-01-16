#include <linux/string.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/fb.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/delay.h>

#include <mach/mt_typedefs.h>
#include <mach/m4u.h>
#include "disp_drv_log.h"
#include "mtkfb.h"
#include "debug.h"
#include "lcm_drv.h"
#include "ddp_ovl.h"
#include "ddp_path.h"
#include "ddp_reg.h"
#include "primary_display.h"
#include "display_recorder.h"
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#include <mach/mt_clkmgr.h>
#include "mtkfb_fence.h"
#include "disp_helper.h"

#include "cmdq_def.h"
#include "cmdq_record.h"
#include "cmdq_reg.h"
#include "cmdq_core.h"

struct MTKFB_MMP_Events_t MTKFB_MMP_Events;

extern LCM_DRIVER *lcm_drv;
extern unsigned int EnableVSyncLog;

#define MTKFB_DEBUG_FS_CAPTURE_LAYER_CONTENT_SUPPORT

// ---------------------------------------------------------------------------
//  External variable declarations
// ---------------------------------------------------------------------------

extern long tpd_last_down_time;
extern int  tpd_start_profiling;
extern void mtkfb_log_enable(int enable);
extern void disp_log_enable(int enable);
extern void mtkfb_vsync_log_enable(int enable);
extern void mtkfb_capture_fb_only(bool enable);
extern void esd_recovery_pause(BOOL en);
extern int mtkfb_set_backlight_mode(unsigned int mode);
extern void mtkfb_pan_disp_test(void);
extern void mtkfb_show_sem_cnt(void);
extern void mtkfb_hang_test(bool en);
extern void mtkfb_switch_normal_to_factory(void);
extern void mtkfb_switch_factory_to_normal(void);
extern int mtkfb_get_debug_state(char* stringbuf, int buf_len);
extern int set_session_mode(disp_session_config * config_info, int force);

extern unsigned int gCaptureLayerEnable;
extern unsigned int gCaptureLayerDownX;
extern unsigned int gCaptureLayerDownY;

extern unsigned int gCaptureOvlThreadEnable;
extern unsigned int gCaptureOvlDownX;
extern unsigned int gCaptureOvlDownY;
extern struct task_struct *captureovl_task;

extern unsigned int gCaptureFBEnable;
extern unsigned int gCaptureFBDownX;
extern unsigned int gCaptureFBDownY;
extern unsigned int gCaptureFBPeriod;
extern struct task_struct *capturefb_task;
extern wait_queue_head_t gCaptureFBWQ;

extern unsigned int gCapturePriLayerEnable;
extern unsigned int gCaptureWdmaLayerEnable;
extern unsigned int gCapturePriLayerDownX;
extern unsigned int gCapturePriLayerDownY;
extern unsigned int gCapturePriLayerNum;
#ifdef MTKFB_DEBUG_FS_CAPTURE_LAYER_CONTENT_SUPPORT
struct dentry *mtkfb_layer_dbgfs[DDP_OVL_LAYER_MUN];

extern OVL_CONFIG_STRUCT cached_layer_config[DDP_OVL_LAYER_MUN];

typedef struct {
    UINT32 layer_index;
    unsigned long working_buf;
    UINT32 working_size;
} MTKFB_LAYER_DBG_OPTIONS;

MTKFB_LAYER_DBG_OPTIONS mtkfb_layer_dbg_opt[DDP_OVL_LAYER_MUN];

#endif
extern LCM_DRIVER *lcm_drv;
// ---------------------------------------------------------------------------
//  Debug Options
// ---------------------------------------------------------------------------

static const long int DEFAULT_LOG_FPS_WND_SIZE = 30;
unsigned int g_mobilelog = 0;

typedef struct {
    unsigned int en_fps_log;
    unsigned int en_touch_latency_log;
    unsigned int log_fps_wnd_size;
    unsigned int force_dis_layers;
} DBG_OPTIONS;

static DBG_OPTIONS dbg_opt = {0};
static bool enable_ovl1_to_mem = true;
static char STR_HELP[] =
    "\n"
    "USAGE\n"
    "        echo [ACTION]... > /d/mtkfb\n"
    "\n"
    "ACTION\n"
	"        mtkfblog:[on|off]\n"
	"             enable/disable [MTKFB] log\n"
	"\n"
	"        displog:[on|off]\n"
	"             enable/disable [DISP] log\n"
	"\n"
	"        mtkfb_vsynclog:[on|off]\n"
	"             enable/disable [VSYNC] log\n"
	"\n"
	"        log:[on|off]\n"
	"             enable/disable above all log\n"
	"\n"
    "        fps:[on|off]\n"
    "             enable fps and lcd update time log\n"
	"\n"
    "        tl:[on|off]\n"
    "             enable touch latency log\n"
    "\n"
    "        layer\n"
    "             dump lcd layer information\n"
    "\n"
    "        suspend\n"
    "             enter suspend mode\n"
    "\n"
    "        resume\n"
    "             leave suspend mode\n"
    "\n"
    "        lcm:[on|off|init]\n"
    "             power on/off lcm\n"
    "\n"
    "        cabc:[ui|mov|still]\n"
    "             cabc mode, UI/Moving picture/Still picture\n"
    "\n"
    "        lcd:[on|off]\n"
    "             power on/off display engine\n"
    "\n"
    "        te:[on|off]\n"
    "             turn on/off tearing-free control\n"
    "\n"
    "        tv:[on|off]\n"
    "             turn on/off tv-out\n"
    "\n"
    "        tvsys:[ntsc|pal]\n"
    "             switch tv system\n"
    "\n"
    "        reg:[lcd|dpi|dsi|tvc|tve]\n"
    "             dump hw register values\n"
    "\n"
    "        regw:addr=val\n"
    "             write hw register\n"
    "\n"
    "        regr:addr\n"
    "             read hw register\n"
    "\n"
    "       cpfbonly:[on|off]\n"
    "             capture UI layer only on/off\n"
    "\n"
    "       esd:[on|off]\n"
    "             esd kthread on/off\n"
    "       HQA:[NormalToFactory|FactoryToNormal]\n"
    "             for HQA requirement\n"
    "\n"
    "       mmp\n"
    "             Register MMProfile events\n"
	"\n"
	"       dump_fb:[on|off[,down_sample_x[,down_sample_y,[delay]]]]\n"
	"             Start/end to capture framebuffer every delay(ms)\n"
	"\n"
	"       dump_ovl:[on|off[,down_sample_x[,down_sample_y]]]\n"
	"             Start to capture OVL only once\n"
	"\n"
	"       dump_layer:[on|off[,down_sample_x[,down_sample_y]][,layer(0:L0,1:L1,2:L2,3:L3,4:L0-3)]\n"
	"             Start/end to capture current enabled OVL layer every frame\n"
    ;


// ---------------------------------------------------------------------------
//  Information Dump Routines
// ---------------------------------------------------------------------------

void init_mtkfb_mmp_events(void)
{
    if (MTKFB_MMP_Events.MTKFB == 0)
    {
        MTKFB_MMP_Events.MTKFB = MMProfileRegisterEvent(MMP_RootEvent, "MTKFB");
        MTKFB_MMP_Events.PanDisplay = MMProfileRegisterEvent(MTKFB_MMP_Events.MTKFB, "PanDisplay");
        MTKFB_MMP_Events.CreateSyncTimeline = MMProfileRegisterEvent(MTKFB_MMP_Events.MTKFB, "CreateSyncTimeline");
        MTKFB_MMP_Events.SetOverlayLayer = MMProfileRegisterEvent(MTKFB_MMP_Events.MTKFB, "SetOverlayLayer");
        MTKFB_MMP_Events.SetOverlayLayers = MMProfileRegisterEvent(MTKFB_MMP_Events.MTKFB, "SetOverlayLayers");
        MTKFB_MMP_Events.SetMultipleLayers = MMProfileRegisterEvent(MTKFB_MMP_Events.MTKFB, "SetMultipleLayers");
        MTKFB_MMP_Events.CreateSyncFence = MMProfileRegisterEvent(MTKFB_MMP_Events.MTKFB, "CreateSyncFence");
        MTKFB_MMP_Events.IncSyncTimeline = MMProfileRegisterEvent(MTKFB_MMP_Events.MTKFB, "IncSyncTimeline");
        MTKFB_MMP_Events.SignalSyncFence = MMProfileRegisterEvent(MTKFB_MMP_Events.MTKFB, "SignalSyncFence");
        MTKFB_MMP_Events.TrigOverlayOut = MMProfileRegisterEvent(MTKFB_MMP_Events.MTKFB, "TrigOverlayOut");
        MTKFB_MMP_Events.UpdateScreenImpl = MMProfileRegisterEvent(MTKFB_MMP_Events.MTKFB, "UpdateScreenImpl");
        MTKFB_MMP_Events.VSync = MMProfileRegisterEvent(MTKFB_MMP_Events.MTKFB, "VSync");
        MTKFB_MMP_Events.UpdateConfig = MMProfileRegisterEvent(MTKFB_MMP_Events.MTKFB, "UpdateConfig");
        MTKFB_MMP_Events.EsdCheck = MMProfileRegisterEvent(MTKFB_MMP_Events.UpdateConfig, "EsdCheck");
        MTKFB_MMP_Events.ConfigOVL = MMProfileRegisterEvent(MTKFB_MMP_Events.UpdateConfig, "ConfigOVL");
        MTKFB_MMP_Events.ConfigAAL = MMProfileRegisterEvent(MTKFB_MMP_Events.UpdateConfig, "ConfigAAL");
        MTKFB_MMP_Events.ConfigMemOut = MMProfileRegisterEvent(MTKFB_MMP_Events.UpdateConfig, "ConfigMemOut");
        MTKFB_MMP_Events.ScreenUpdate = MMProfileRegisterEvent(MTKFB_MMP_Events.MTKFB, "ScreenUpdate");
        MTKFB_MMP_Events.CaptureFramebuffer = MMProfileRegisterEvent(MTKFB_MMP_Events.MTKFB, "CaptureFB");
        MTKFB_MMP_Events.RegUpdate = MMProfileRegisterEvent(MTKFB_MMP_Events.MTKFB, "RegUpdate");
        MTKFB_MMP_Events.EarlySuspend = MMProfileRegisterEvent(MTKFB_MMP_Events.MTKFB, "EarlySuspend");
        MTKFB_MMP_Events.DispDone = MMProfileRegisterEvent(MTKFB_MMP_Events.MTKFB, "DispDone");
        MTKFB_MMP_Events.DSICmd = MMProfileRegisterEvent(MTKFB_MMP_Events.MTKFB, "DSICmd");
        MTKFB_MMP_Events.DSIIRQ = MMProfileRegisterEvent(MTKFB_MMP_Events.MTKFB, "DSIIrq");
        MTKFB_MMP_Events.WaitVSync = MMProfileRegisterEvent(MTKFB_MMP_Events.MTKFB, "WaitVSync");
        MTKFB_MMP_Events.LayerDump = MMProfileRegisterEvent(MTKFB_MMP_Events.MTKFB, "LayerDump");
        MTKFB_MMP_Events.Layer[0] = MMProfileRegisterEvent(MTKFB_MMP_Events.LayerDump, "Layer0");
        MTKFB_MMP_Events.Layer[1] = MMProfileRegisterEvent(MTKFB_MMP_Events.LayerDump, "Layer1");
        MTKFB_MMP_Events.Layer[2] = MMProfileRegisterEvent(MTKFB_MMP_Events.LayerDump, "Layer2");
        MTKFB_MMP_Events.Layer[3] = MMProfileRegisterEvent(MTKFB_MMP_Events.LayerDump, "Layer3");
        MTKFB_MMP_Events.OvlDump = MMProfileRegisterEvent(MTKFB_MMP_Events.MTKFB, "OvlDump");
        MTKFB_MMP_Events.FBDump = MMProfileRegisterEvent(MTKFB_MMP_Events.MTKFB, "FBDump");
        MTKFB_MMP_Events.DSIRead = MMProfileRegisterEvent(MTKFB_MMP_Events.MTKFB, "DSIRead");
        MTKFB_MMP_Events.GetLayerInfo = MMProfileRegisterEvent(MTKFB_MMP_Events.MTKFB, "GetLayerInfo");
        MTKFB_MMP_Events.LayerInfo[0] = MMProfileRegisterEvent(MTKFB_MMP_Events.GetLayerInfo, "LayerInfo0");
        MTKFB_MMP_Events.LayerInfo[1] = MMProfileRegisterEvent(MTKFB_MMP_Events.GetLayerInfo, "LayerInfo1");
        MTKFB_MMP_Events.LayerInfo[2] = MMProfileRegisterEvent(MTKFB_MMP_Events.GetLayerInfo, "LayerInfo2");
        MTKFB_MMP_Events.LayerInfo[3] = MMProfileRegisterEvent(MTKFB_MMP_Events.GetLayerInfo, "LayerInfo3");
        MTKFB_MMP_Events.IOCtrl = MMProfileRegisterEvent(MTKFB_MMP_Events.MTKFB, "IOCtrl");
        MTKFB_MMP_Events.Debug = MMProfileRegisterEvent(MTKFB_MMP_Events.MTKFB, "Debug");
        MMProfileEnableEventRecursive(MTKFB_MMP_Events.MTKFB, 1);
    }
}

static __inline int is_layer_enable(unsigned int roi_ctl, unsigned int layer)
{
    return (roi_ctl >> (31 - layer)) & 0x1;
}

static void dump_layer_info(void)
{
	unsigned int i;
	for(i=0;i<4;i++){
		printk("LayerInfo in LCD driver, layer=%d,layer_en=%d, source=%d, fmt=%d, addr=0x%lx, x=%d, y=%d\n\
		w=%d, h=%d, pitch=%d, keyEn=%d, key=%d, aen=%d, alpha=%d \n ",
	    cached_layer_config[i].layer,   // layer
	    cached_layer_config[i].layer_en,
	    cached_layer_config[i].source,   // data source (0=memory)
	    cached_layer_config[i].fmt,
	    cached_layer_config[i].addr, // addr
	    cached_layer_config[i].dst_x,  // x
	    cached_layer_config[i].dst_y,  // y
	    cached_layer_config[i].dst_w, // width
	    cached_layer_config[i].dst_h, // height
	    cached_layer_config[i].src_pitch, //pitch, pixel number
	    cached_layer_config[i].keyEn,  //color key
	    cached_layer_config[i].key,  //color key
	    cached_layer_config[i].aen, // alpha enable
	    cached_layer_config[i].alpha);
	}
}


// ---------------------------------------------------------------------------
//  FPS Log
// ---------------------------------------------------------------------------

typedef struct {
    long int current_lcd_time_us;
    long int current_te_delay_time_us;
    long int total_lcd_time_us;
    long int total_te_delay_time_us;
    long int start_time_us;
    long int trigger_lcd_time_us;
    unsigned int trigger_lcd_count;

    long int current_hdmi_time_us;
    long int total_hdmi_time_us;
    long int hdmi_start_time_us;
    long int trigger_hdmi_time_us;
    unsigned int trigger_hdmi_count;
} FPS_LOGGER;

static FPS_LOGGER fps = {0};
static FPS_LOGGER hdmi_fps = {0};

static long int get_current_time_us(void)
{
    struct timeval t;
    do_gettimeofday(&t);
    return (t.tv_sec & 0xFFF) * 1000000 + t.tv_usec;
}


static void __inline reset_fps_logger(void)
{
    memset(&fps, 0, sizeof(fps));
}

static void __inline reset_hdmi_fps_logger(void)
{
    memset(&hdmi_fps, 0, sizeof(hdmi_fps));
}

void DBG_OnTriggerLcd(void)
{
    if (!dbg_opt.en_fps_log && !dbg_opt.en_touch_latency_log) return;

    fps.trigger_lcd_time_us = get_current_time_us();
    if (fps.trigger_lcd_count == 0) {
        fps.start_time_us = fps.trigger_lcd_time_us;
    }
}

void DBG_OnTriggerHDMI(void)
{
    if (!dbg_opt.en_fps_log && !dbg_opt.en_touch_latency_log) return;

    hdmi_fps.trigger_hdmi_time_us = get_current_time_us();
    if (hdmi_fps.trigger_hdmi_count == 0) {
        hdmi_fps.hdmi_start_time_us = hdmi_fps.trigger_hdmi_time_us;
    }
}

void DBG_OnTeDelayDone(void)
{
    long int time;

    if (!dbg_opt.en_fps_log && !dbg_opt.en_touch_latency_log) return;

    time = get_current_time_us();
    fps.current_te_delay_time_us = (time - fps.trigger_lcd_time_us);
    fps.total_te_delay_time_us += fps.current_te_delay_time_us;
}


void DBG_OnLcdDone(void)
{
    long int time;

    if (!dbg_opt.en_fps_log && !dbg_opt.en_touch_latency_log) return;

    // deal with touch latency log

    time = get_current_time_us();
    fps.current_lcd_time_us = (time - fps.trigger_lcd_time_us);

#if 0   // FIXME
    if (dbg_opt.en_touch_latency_log && tpd_start_profiling) {

        DISP_LOG_PRINT(ANDROID_LOG_INFO, "DBG", "Touch Latency: %ld ms\n",
               (time - tpd_last_down_time) / 1000);

        DISP_LOG_PRINT(ANDROID_LOG_INFO, "DBG", "LCD update time %ld ms (TE delay %ld ms + LCD %ld ms)\n",
               fps.current_lcd_time_us / 1000,
               fps.current_te_delay_time_us / 1000,
               (fps.current_lcd_time_us - fps.current_te_delay_time_us) / 1000);

        tpd_start_profiling = 0;
    }
#endif

    if (!dbg_opt.en_fps_log) return;

    // deal with fps log

    fps.total_lcd_time_us += fps.current_lcd_time_us;
    ++ fps.trigger_lcd_count;

    if (fps.trigger_lcd_count >= dbg_opt.log_fps_wnd_size) {

        long int f = fps.trigger_lcd_count * 100 * 1000 * 1000
                     / (time - fps.start_time_us);

        long int update = fps.total_lcd_time_us * 100
                          / (1000 * fps.trigger_lcd_count);

        long int te = fps.total_te_delay_time_us * 100
                      / (1000 * fps.trigger_lcd_count);

        long int lcd = (fps.total_lcd_time_us - fps.total_te_delay_time_us) * 100
                       / (1000 * fps.trigger_lcd_count);

        DISP_LOG_PRINT(ANDROID_LOG_INFO, "DBG", "MTKFB FPS: %ld.%02ld, Avg. update time: %ld.%02ld ms "
               "(TE delay %ld.%02ld ms, LCD %ld.%02ld ms)\n",
               f / 100, f % 100,
               update / 100, update % 100,
               te / 100, te % 100,
               lcd / 100, lcd % 100);
		reset_fps_logger();
	}
}

void DBG_OnHDMIDone(void)
{
    long int time;

    if (!dbg_opt.en_fps_log && !dbg_opt.en_touch_latency_log) return;

    // deal with touch latency log

    time = get_current_time_us();
    hdmi_fps.current_hdmi_time_us = (time - hdmi_fps.trigger_hdmi_time_us);


    if (!dbg_opt.en_fps_log) return;

    // deal with fps log

    hdmi_fps.total_hdmi_time_us += hdmi_fps.current_hdmi_time_us;
    ++ hdmi_fps.trigger_hdmi_count;

    if (hdmi_fps.trigger_hdmi_count >= dbg_opt.log_fps_wnd_size) {

        long int f = hdmi_fps.trigger_hdmi_count * 100 * 1000 * 1000
                     / (time - hdmi_fps.hdmi_start_time_us);

        long int update = hdmi_fps.total_hdmi_time_us * 100
                          / (1000 * hdmi_fps.trigger_hdmi_count);

        DISP_LOG_PRINT(ANDROID_LOG_INFO, "DBG", "[HDMI] FPS: %ld.%02ld, Avg. update time: %ld.%02ld ms\n",
               f / 100, f % 100,
               update / 100, update % 100);

        reset_hdmi_fps_logger();
    }
}

// ---------------------------------------------------------------------------
//  Command Processor
// ---------------------------------------------------------------------------
extern void mtkfb_clear_lcm(void);
extern void hdmi_force_init(void);
extern int DSI_BIST_Pattern_Test(DISP_MODULE_ENUM module, cmdqRecHandle cmdq, bool enable, unsigned int color);

bool get_ovl1_to_mem_on()
{
    return enable_ovl1_to_mem;
}

void switch_ovl1_to_mem(bool on)
{
    enable_ovl1_to_mem = on;
    DISP_LOG_PRINT(ANDROID_LOG_INFO, "DBG", "switch_ovl1_to_mem %d\n", enable_ovl1_to_mem);
}


extern void  smp_inner_dcache_flush_all(void);

static int g_display_debug_pattern_index = 0;
extern int DAL_Clean(void);
extern int DAL_Printf(const char *fmt, ...);
extern void DSI_ChangeClk(DISP_MODULE_ENUM module,UINT32 clk);
extern int g_lcm_x;
extern int g_lcm_y;
extern int dpmgr_module_notify(DISP_MODULE_ENUM module, DISP_PATH_EVENT event);
extern int decouple_shorter_path;
extern void _cmdq_start_trigger_loop(void);
extern void _cmdq_stop_trigger_loop(void);

static void process_dbg_opt(const char *opt)
{
	if (0 == strncmp(opt, "stop_trigger_loop", 17))
	{
		_cmdq_stop_trigger_loop();
		return;
	}
	else if (0 == strncmp(opt, "vcore", 5))
	{
		int ret =0;
		cmdqRecHandle handle = NULL;
		ret = cmdqRecCreate(CMDQ_SCENARIO_DISP_SCREEN_CAPTURE,&handle);
		cmdqRecReset(handle);
		if(primary_display_is_video_mode())
		{
			cmdqRecWaitNoClear(handle, CMDQ_EVENT_MUTEX0_STREAM_EOF);
		}
		else
		{
			cmdqRecWaitNoClear(handle, CMDQ_SYNC_TOKEN_STREAM_EOF);
		}

		cmdqRecWrite(handle, 0x10209260&0x1fffffff, 0x00000121, ~0);
		cmdqRecWrite(handle, 0x10209264&0x1fffffff, 0x800AD89D, ~0);

		cmdqRecFlushAsync(handle);
		DISPMSG("primary_display_cmdq_set_reg, cmdq flush done\n");

		cmdqRecDestroy(handle);

		return;
	}
	else if (0 == strncmp(opt, "start_trigger_loop", 18))
	{
		_cmdq_start_trigger_loop();
		return;
	}
	else if (0 == strncmp(opt, "cmdqregw:", 9))
    {
        char *p = (char *)opt + 9;
        unsigned int addr = simple_strtoul(p, &p, 16);
        unsigned int val  = simple_strtoul(p + 1, &p, 16);

        if (addr) {
            primary_display_cmdq_set_reg(addr, val);
        } else {
            return;
        }
    }
	else if (0 == strncmp(opt, "dsidual_regw:", 13))
    {
        char *p = (char *)opt + 13;
        unsigned int offset = simple_strtoul(p, &p, 16);
        unsigned int val = simple_strtoul(p + 1, &p, 16);

        if (offset) {
            primary_display_cmdq_set_reg(0x1401b000+offset, val);
            primary_display_cmdq_set_reg(0x1401c000+offset, val);
        } else {
            return;
        }
    }
	if (0 == strncmp(opt, "idle_switch_DC", 14))
    {
        if (0 == strncmp(opt + 14, "on", 2))
        {
			enable_screen_idle_switch_decouple();
            printk("enable screen_idle_switch_decouple\n");
        }
        else if (0 == strncmp(opt + 14, "off", 3))
		{
			disable_screen_idle_switch_decouple();
			printk("disable screen_idle_switch_decouple\n");
        }
	}
	else if (0 == strncmp(opt, "shortpath", 9))
	{
		char *p = (char *)opt + 10;
		int s = simple_strtoul(p, &p, 10);
		DISPMSG("will %s use shorter decouple path\n", s?"":"not");
		disp_helper_set_option(DISP_HELPER_OPTION_TWO_PIPE_INTERFACE_PATH, s);
	}
	else if (0 == strncmp(opt, "helper", 6))
	{
		char *p = (char *)opt + 7;
		int option = simple_strtoul(p, &p, 10);
		int value = simple_strtoul(p + 1, &p, 10);
		DISPMSG("will set option %d to %d\n", option, value);
		disp_helper_set_option(option, value);
	}
	else if (0 == strncmp(opt, "dc565", 5))
	{
		char *p = (char *)opt + 6;
		int s = simple_strtoul(p, &p, 10);
		DISPMSG("will %s use RGB565 decouple path\n", s?"":"not");
		disp_helper_set_option(DISP_HELPER_OPTION_DECOUPLE_MODE_USE_RGB565, s);
	}
	else if (0 == strncmp(opt, "switch_mode:", 12))
	{
		int session_id = MAKE_DISP_SESSION(DISP_SESSION_PRIMARY,0);
		char *p = (char *)opt + 12;
		int sess_mode = simple_strtoul(p, &p, 10);
		DISPMSG("debug_display_mode: 0x%08x\n", sess_mode);
		primary_display_mode_switch_test(sess_mode);
	}
	else if (0 == strncmp(opt, "dsipattern", 10))
	{
		char *p = (char *)opt + 11;
		unsigned int pattern = (unsigned int) simple_strtoul(p, &p, 16);

		if (pattern)
		{
			DSI_BIST_Pattern_Test(DISP_MODULE_DSI0,NULL,true,pattern);
			DISPMSG("enable dsi pattern: 0x%08x\n", pattern);
		}
		else
		{
			primary_display_manual_lock();
			DSI_BIST_Pattern_Test(DISP_MODULE_DSI0,NULL,false,0);
			primary_display_manual_unlock();
			return;
		}
	}
	else if (0 == strncmp(opt, "force_fps:", 9))
    {
        char *p = (char *)opt + 9;
        unsigned long keep = simple_strtoul(p, &p, 10);
        unsigned long skip  = simple_strtoul(p + 1, &p, 10);

		DISPMSG("force set fps, keep %ld, skip %ld\n", keep, skip);
		primary_display_force_set_fps(keep, skip);
    }
	else if (0 == strncmp(opt, "mobile:", 7))
	{
		if (0 == strncmp(opt + 7, "on", 2))
			g_mobilelog = 1;
		else if (0 == strncmp(opt + 7, "off", 3))
			g_mobilelog = 0;
	}
	else if (0 == strncmp(opt, "trigger", 7))
	{
		int i = 0;
		disp_session_vsync_config vsync_config;
		for(i=0;i<1200;i++)
		{
			primary_display_wait_for_vsync(&vsync_config);
			dpmgr_module_notify(DISP_MODULE_AAL, DISP_PATH_EVENT_TRIGGER);
		}
	}
	else if (0 == strncmp(opt, "lcmx:", 5))
	{
		char *p = (char *)opt + 5;
		g_lcm_x = (unsigned int) simple_strtoul(p, &p, 10);
	}
	else if (0 == strncmp(opt, "lcmy:", 5))
	{
		char *p = (char *)opt + 5;
		g_lcm_y = (unsigned int) simple_strtoul(p, &p, 10);
	}
	else if (0 == strncmp(opt, "diagnose", 8))
	{
		primary_display_diagnose();
		return;
	}
	else if (0 == strncmp(opt, "dprec_reset", 11))
	{
		dprec_logger_reset_all();
		return;
	}
	else if (0 == strncmp(opt, "suspend", 4))
    {
        cci400_sel_for_ddp();
    	primary_display_suspend();
		return;
    }
    else if (0 == strncmp(opt, "resume", 4))
    {
    		primary_display_resume();
    }
    else if (0 == strncmp(opt, "dalprintf", 9))
    {
    		DAL_Printf("display aee layer test\n");
    }
    else if (0 == strncmp(opt, "dalclean", 8))
    {
    		DAL_Clean();
    }
    else if (0 == strncmp(opt, "daltest", 7))
    {
    	int i = 1000;
		while(i--)
		{
    		DAL_Printf("display aee layer test\n");
			msleep(20);
    		DAL_Clean();
			msleep(20);
		}
    }
	else if (0 == strncmp(opt, "DP", 2))
	{
		char *p = (char *)opt + 3;
		unsigned int pattern = (unsigned int) simple_strtoul(p, &p, 16);
		g_display_debug_pattern_index = pattern;
		return;
	}
	else if(0==strncmp(opt,"dsi0_clk:",9))
   	{
        char*p=(char*)opt+9;
        UINT32 clk=simple_strtoul(p, &p, 10);
        DSI_ChangeClk(DISP_MODULE_DSI0,clk);
    }
	else if(0==strncmp(opt,"dsidual_clk:",12))
   	{
   		// This can't be used when screen update is running, because we use cpu to re-init MIPITX Clock, which can't ensure 2 dsi's clock is synchronous.
        char*p=(char*)opt+12;
        UINT32 clk=simple_strtoul(p, &p, 10);
        DSI_ChangeClk(DISP_MODULE_DSIDUAL,clk);
    }
    else if (0 == strncmp(opt, "diagnose", 8))
    {
    	primary_display_diagnose();
		return;
    }
    else if (0 == strncmp(opt, "fps:", 4))
    {
        printk("change fps\n");
        char*p=(char*)opt+4;
        int fps=simple_strtoul(p, &p, 10);
    	primary_display_set_fps(fps);
		return;
    }
	else if (0 == strncmp(opt, "switch:", 7))
	{
    	char*p=(char*)opt+7;
    	UINT32 mode=simple_strtoul(p, &p, 10);
		primary_display_switch_dst_mode(mode%2);
		return;
	}
    else if (0 == strncmp(opt, "switchmode:", 11))
    {
        char*p=(char*)opt+11;
        UINT32 mode=simple_strtoul(p, &p, 10);
        printk("switchmode %d\n", mode);
        disp_session_config config;
        config.type = DISP_SESSION_PRIMARY;
        config.device_id = 0;
        config.mode = mode;
        config.session_id = MAKE_DISP_SESSION(DISP_SESSION_PRIMARY,0);
        set_session_mode(&config, 1);
        return;
    }
	else if (0 == strncmp(opt, "regw:", 5))
    {
        char *p = (char *)opt + 5;
        unsigned long addr = simple_strtoul(p, &p, 16);
        unsigned long val  = simple_strtoul(p + 1, &p, 16);

        if (addr) {
            OUTREG32(addr, val);
        } else {
            return;
        }
    }
    else if (0 == strncmp(opt, "regr:", 5))
    {
        char *p = (char *)opt + 5;
        void* addr = (void*) simple_strtoul(p, &p, 16);

        if (addr) {
            printk("Read register 0x%p: 0x%08x\n", addr, INREG32(addr));
        } else {
           return;
        }
    }
    else if (0 == strncmp(opt, "cmmva_dprec", 11))
    {
		dprec_handle_option(0x7);
	}
    else if (0 == strncmp(opt, "cmmpa_dprec", 11))
    {
		dprec_handle_option(0x3);
	}
    else if (0 == strncmp(opt, "dprec", 5))
    {
		char *p = (char *)opt + 6;
		unsigned int option = (unsigned int) simple_strtoul(p, &p, 16);
		dprec_handle_option(option);
	}
    else if (0 == strncmp(opt, "cmdq", 4))
    {
		char *p = (char *)opt + 5;
		unsigned int option = (unsigned int) simple_strtoul(p, &p, 16);
		if(option)
			primary_display_switch_cmdq_cpu(CMDQ_ENABLE);
		else
			primary_display_switch_cmdq_cpu(CMDQ_DISABLE);
	}
    else if (0 == strncmp(opt, "maxlayer", 8))
    {
		char *p = (char *)opt + 9;
		unsigned int maxlayer = (unsigned int) simple_strtoul(p, &p, 10);
		if(maxlayer)
			primary_display_set_max_layer(maxlayer);
		else
			DISPERR("can't set max layer to 0\n");
	}
    else if (0 == strncmp(opt, "primary_reset", 13))
    {
		primary_display_reset();
	}
	else if(0 == strncmp(opt, "esd_check", 9))
	{
		char *p = (char *)opt + 10;
		unsigned int enable = (unsigned int) simple_strtoul(p, &p, 10);
		primary_display_esd_check_enable(enable);
	}
	else if(0 == strncmp(opt, "esd_recovery", 12))
	{
		primary_display_esd_recovery();
	}
	else if(0 == strncmp(opt, "lcm0_reset", 10))
	{
	#if 0
		DISP_CPU_REG_SET(DDP_REG_BASE_MMSYS_CONFIG+0x150,1);
		msleep(10);
		DISP_CPU_REG_SET(DDP_REG_BASE_MMSYS_CONFIG+0x150,0);
		msleep(10);
		DISP_CPU_REG_SET(DDP_REG_BASE_MMSYS_CONFIG+0x150,1);

	#else
		mt_set_gpio_mode(GPIO106|0x80000000, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO106|0x80000000, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO106|0x80000000, GPIO_OUT_ONE);
		msleep(10);
		mt_set_gpio_out(GPIO106|0x80000000, GPIO_OUT_ZERO);
		msleep(10);
		mt_set_gpio_out(GPIO106|0x80000000, GPIO_OUT_ONE);
	#endif
	}
	else if(0 == strncmp(opt, "lcm0_reset0", 11))
	{
		DISP_CPU_REG_SET(MMSYS_CONFIG_BASE+0x150,0);
	}
	else if(0 == strncmp(opt, "lcm0_reset1", 11))
	{
		DISP_CPU_REG_SET(MMSYS_CONFIG_BASE+0x150,1);
	}
	else if (0 == strncmp(opt, "cg", 2))
    {
		char *p = (char *)opt + 2;
		unsigned int enable = (unsigned int) simple_strtoul(p, &p, 10);
		primary_display_enable_path_cg(enable);
	}
	else if (0 == strncmp(opt, "ovl2mem:", 8))
    {
        if (0 == strncmp(opt + 8, "on", 2))
            switch_ovl1_to_mem(true);
        else
            switch_ovl1_to_mem(false);
    }
	else if (0 == strncmp(opt, "dump_layer:", 11))
    {
        if (0 == strncmp(opt + 11, "on", 2))
        {
            char *p = (char *)opt + 14;
            gCapturePriLayerDownX = simple_strtoul(p, &p, 10);
            gCapturePriLayerDownY = simple_strtoul(p+1, &p, 10);
			gCapturePriLayerNum= simple_strtoul(p+1, &p, 10);
			gCapturePriLayerEnable = 1;
			gCaptureWdmaLayerEnable = 1;
			if(gCapturePriLayerDownX==0)
				gCapturePriLayerDownX = 20;
			if(gCapturePriLayerDownY==0)
				gCapturePriLayerDownY = 20;
            printk("dump_layer En %d DownX %d DownY %d,Num %d",gCapturePriLayerEnable,gCapturePriLayerDownX,gCapturePriLayerDownY,gCapturePriLayerNum);

        }
        else if (0 == strncmp(opt + 11, "off", 3))
        {
            gCapturePriLayerEnable = 0;
            gCaptureWdmaLayerEnable = 0;
			gCapturePriLayerNum = OVL_LAYER_NUM;
			printk("dump_layer En %d\n",gCapturePriLayerEnable);
        }
    }
#ifdef MTK_TODO
#error
    if (0 == strncmp(opt, "hdmion", 6))
    {
//	hdmi_force_init();
    }
    else if (0 == strncmp(opt, "fps:", 4))
    {
        if (0 == strncmp(opt + 4, "on", 2)) {
            dbg_opt.en_fps_log = 1;
        } else if (0 == strncmp(opt + 4, "off", 3)) {
            dbg_opt.en_fps_log = 0;
        } else {
            goto Error;
        }
        reset_fps_logger();
    }
    else if (0 == strncmp(opt, "tl:", 3))
    {
        if (0 == strncmp(opt + 3, "on", 2)) {
            dbg_opt.en_touch_latency_log = 1;
        } else if (0 == strncmp(opt + 3, "off", 3)) {
            dbg_opt.en_touch_latency_log = 0;
        } else {
            goto Error;
        }
    }
    else if (0 == strncmp(opt, "black", 5))
    {
	mtkfb_clear_lcm();
    }
    else if (0 == strncmp(opt, "suspend", 4))
    {
        DISP_PanelEnable(FALSE);
        DISP_PowerEnable(FALSE);
    }
    else if (0 == strncmp(opt, "resume", 4))
    {
        DISP_PowerEnable(TRUE);
        DISP_PanelEnable(TRUE);
    }
    else if (0 == strncmp(opt, "lcm:", 4))
    {
        if (0 == strncmp(opt + 4, "on", 2)) {
            DISP_PanelEnable(TRUE);
        } else if (0 == strncmp(opt + 4, "off", 3)) {
            DISP_PanelEnable(FALSE);
        }
		else if (0 == strncmp(opt + 4, "init", 4)) {
			if (NULL != lcm_drv && NULL != lcm_drv->init) {
        		lcm_drv->init();
    		}
        }else {
            goto Error;
        }
    }
    else if (0 == strncmp(opt, "cabc:", 5))
    {
        if (0 == strncmp(opt + 5, "ui", 2)) {
			mtkfb_set_backlight_mode(1);
        }else if (0 == strncmp(opt + 5, "mov", 3)) {
			mtkfb_set_backlight_mode(3);
        }else if (0 == strncmp(opt + 5, "still", 5)) {
			mtkfb_set_backlight_mode(2);
        }else {
            goto Error;
        }
    }
    else if (0 == strncmp(opt, "lcd:", 4))
    {
        if (0 == strncmp(opt + 4, "on", 2)) {
            DISP_PowerEnable(TRUE);
        } else if (0 == strncmp(opt + 4, "off", 3)) {
            DISP_PowerEnable(FALSE);
        } else {
            goto Error;
        }
    }
    else if (0 == strncmp(opt, "vsynclog:", 9))
    {
        if (0 == strncmp(opt + 9, "on", 2))
        {
            EnableVSyncLog = 1;
        } else if (0 == strncmp(opt + 9, "off", 3))
        {
            EnableVSyncLog = 0;
        } else {
            goto Error;
        }
    }
    else if (0 == strncmp(opt, "layer", 5))
    {
        dump_layer_info();
    }
    else if (0 == strncmp(opt, "regw:", 5))
    {
        char *p = (char *)opt + 5;
        unsigned long addr = simple_strtoul(p, &p, 16);
        unsigned long val  = simple_strtoul(p + 1, &p, 16);

        if (addr) {
            OUTREG32(addr, val);
        } else {
            goto Error;
        }
    }
    else if (0 == strncmp(opt, "regr:", 5))
    {
        char *p = (char *)opt + 5;
        unsigned int addr = (unsigned int) simple_strtoul(p, &p, 16);

        if (addr) {
            DISP_LOG_PRINT(ANDROID_LOG_INFO, "DBG", "Read register 0x%08x: 0x%08x\n", addr, INREG32(addr));
        } else {
            goto Error;
        }
    }
    else if(0 == strncmp(opt, "bkl:", 4))
    {
        char *p = (char *)opt + 4;
        unsigned int level = (unsigned int) simple_strtoul(p, &p, 10);

        DISP_LOG_PRINT(ANDROID_LOG_INFO, "DBG", "process_dbg_opt(), set backlight level = %d\n", level);
        DISP_SetBacklight(level);
    }
    else if(0 == strncmp(opt, "dither:", 7))
    {
        unsigned lrs, lgs, lbs, dbr, dbg, dbb;
        char *p = (char *)opt + 7;

        lrs = (unsigned int) simple_strtoul(p, &p, 16);
        p++;
        lgs = (unsigned int) simple_strtoul(p, &p, 16);
        p++;
        lbs = (unsigned int) simple_strtoul(p, &p, 16);
        p++;
        dbr = (unsigned int) simple_strtoul(p, &p, 16);
        p++;
        dbg = (unsigned int) simple_strtoul(p, &p, 16);
        p++;
        dbb = (unsigned int) simple_strtoul(p, &p, 16);

        DISP_LOG_PRINT(ANDROID_LOG_INFO, "DBG", "process_dbg_opt(), %d %d %d %d %d %d\n", lrs, lgs, lbs, dbr, dbg, dbb);
    }
    else if (0 == strncmp(opt, "mtkfblog:", 9))
    {
        if (0 == strncmp(opt + 9, "on", 2)) {
            mtkfb_log_enable(true);
        } else if (0 == strncmp(opt + 9, "off", 3)) {
            mtkfb_log_enable(false);
        } else {
            goto Error;
        }
    }
    else if (0 == strncmp(opt, "displog:", 8))
    {
        if (0 == strncmp(opt + 8, "on", 2)) {
            disp_log_enable(true);
        } else if (0 == strncmp(opt + 8, "off", 3)) {
            disp_log_enable(false);
        } else {
            goto Error;
        }
    }
    else if (0 == strncmp(opt, "mtkfb_vsynclog:", 15))
    {
        if (0 == strncmp(opt + 15, "on", 2)) {
            mtkfb_vsync_log_enable(true);
        } else if (0 == strncmp(opt + 15, "off", 3)) {
            mtkfb_vsync_log_enable(false);
        } else {
            goto Error;
        }
    }
    else if (0 == strncmp(opt, "log:", 4))
    {
        if (0 == strncmp(opt + 4, "on", 2)) {
			mtkfb_log_enable(true);
			disp_log_enable(true);
        } else if (0 == strncmp(opt + 4, "off", 3)) {
            mtkfb_log_enable(false);
			disp_log_enable(false);
        } else {
            goto Error;
        }
    }
    else if (0 == strncmp(opt, "update", 6))
    {
		DISP_UpdateScreen(0, 0, DISP_GetScreenWidth(), DISP_GetScreenHeight());
    }
    else if (0 == strncmp(opt, "pan_disp", 8))
    {
		mtkfb_pan_disp_test();
    }
    else if (0 == strncmp(opt, "sem_cnt", 7))
    {
		mtkfb_show_sem_cnt();
    }
    else if (0 == strncmp(opt, "hang:", 5))
    {
        if (0 == strncmp(opt + 5, "on", 2)) {
            mtkfb_hang_test(true);
        } else if (0 == strncmp(opt + 5, "off", 3)) {
            mtkfb_hang_test(false);
        } else{
            goto Error;
        }
    }
    else if (0 == strncmp(opt, "cpfbonly:", 9))
    {
        if (0 == strncmp(opt + 9, "on", 2))
        {
            mtkfb_capture_fb_only(true);
        }
        else if (0 == strncmp(opt + 9, "off", 3))
        {
            mtkfb_capture_fb_only(false);
        }
    }
    else if (0 == strncmp(opt, "esd:", 4))
    {
        if (0 == strncmp(opt + 4, "on", 2))
        {
            esd_recovery_pause(FALSE);
        }
        else if (0 == strncmp(opt + 4, "off", 3))
        {
            esd_recovery_pause(TRUE);
        }
    }
    else if (0 == strncmp(opt, "HQA:", 4))
    {
        if (0 == strncmp(opt + 4, "NormalToFactory", 15))
        {
            mtkfb_switch_normal_to_factory();
        }
        else if (0 == strncmp(opt + 4, "FactoryToNormal", 15))
        {
            mtkfb_switch_factory_to_normal();
        }
    }
    else if (0 == strncmp(opt, "mmp", 3))
    {
        init_mtkfb_mmp_events();
    }
    else if (0 == strncmp(opt, "dump_ovl:", 9))
    {
        if (0 == strncmp(opt + 9, "on", 2))
        {
            char *p = (char *)opt + 12;
            gCaptureOvlDownX = simple_strtoul(p, &p, 10);
            gCaptureOvlDownY = simple_strtoul(p+1, &p, 10);
            gCaptureOvlThreadEnable = 1;
			wake_up_process(captureovl_task);
        }
        else if (0 == strncmp(opt + 9, "off", 3))
        {
            gCaptureOvlThreadEnable = 0;
        }
    }
    else if (0 == strncmp(opt, "dump_fb:", 8))
    {
        if (0 == strncmp(opt + 8, "on", 2))
        {
            char *p = (char *)opt + 11;
            gCaptureFBDownX = simple_strtoul(p, &p, 10);
            gCaptureFBDownY = simple_strtoul(p+1, &p, 10);
            gCaptureFBPeriod = simple_strtoul(p+1, &p, 10);
            gCaptureFBEnable = 1;
			wake_up_interruptible(&gCaptureFBWQ);
        }
        else if (0 == strncmp(opt + 8, "off", 3))
        {
            gCaptureFBEnable = 0;
        }
    }
    else
	{
	    if (disphal_process_dbg_opt(opt))
		goto Error;
	}

    return;

Error:
    DISP_LOG_PRINT(ANDROID_LOG_INFO, "ERROR", "parse command error!\n\n%s", STR_HELP);
#endif
}


static void process_dbg_cmd(char *cmd)
{
    char *tok;

    DISP_LOG_PRINT(ANDROID_LOG_INFO, "DBG", "[mtkfb_dbg] %s\n", cmd);

    while ((tok = strsep(&cmd, " ")) != NULL)
    {
        process_dbg_opt(tok);
    }
}


// ---------------------------------------------------------------------------
//  Debug FileSystem Routines
// ---------------------------------------------------------------------------

struct dentry *mtkfb_dbgfs = NULL;


static ssize_t debug_open(struct inode *inode, struct file *file)
{
    file->private_data = inode->i_private;
    return 0;
}


static char debug_buffer[16*1024*3*2*2];

int debug_get_info(unsigned char *stringbuf, int buf_len)
{
	int i = 0;
	int n = 0;

	DISPFUNC();

	n += mtkfb_get_debug_state(stringbuf + n, buf_len-n); 	                          DISPMSG("%s,%d, n=%d\n", __func__, __LINE__, n);

	n += primary_display_get_debug_state(stringbuf + n, buf_len-n);             DISPMSG("%s,%d, n=%d\n", __func__, __LINE__, n);

	n += disp_sync_get_debug_info(stringbuf + n, buf_len-n);                   	DISPMSG("%s,%d, n=%d\n", __func__, __LINE__, n);

	n += dprec_logger_get_result_string_all(stringbuf + n, buf_len-n);	DISPMSG("%s,%d, n=%d\n", __func__, __LINE__, n);

	n += primary_display_check_path(stringbuf + n, buf_len-n);	DISPMSG("%s,%d, n=%d\n", __func__, __LINE__, n);

	n += dprec_logger_get_buf(DPREC_LOGGER_ERROR, stringbuf + n, buf_len-n);	DISPMSG("%s,%d, n=%d\n", __func__, __LINE__, n);

	n += dprec_logger_get_buf(DPREC_LOGGER_FENCE, stringbuf + n, buf_len-n);	DISPMSG("%s,%d, n=%d\n", __func__, __LINE__, n);

	n += dprec_logger_get_buf(DPREC_LOGGER_HWOP, stringbuf + n, buf_len-n); DISPMSG("%s,%d, n=%d\n", __func__, __LINE__, n);

	n += disp_helper_get_option_list(stringbuf + n, buf_len-n); DISPMSG("%s,%d, n=%d\n", __func__, __LINE__, n);
	n += dprec_logger_get_buf(DPREC_LOGGER_DEBUG, stringbuf + n, buf_len - n);
	stringbuf[n++] = 0;
	return n;
}

void debug_info_dump_to_printk(char *buf, int buf_len)
{
	int i = 0;
	int n = buf_len;
	for(i=0;i<n;i+=256)
		DISPMSG("%s", buf+i);
}

static ssize_t debug_read(struct file *file,
                          char __user *ubuf, size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(debug_buffer) - 1;
	int n = 0;

	/* Debugfs read only fetch 4096 byte each time, thus whole ringbuffer need massive
	 * iteration. We only copy ringbuffer content to debugfs buffer at first time (*ppos = 0)
	 */
	if (*ppos != 0)
		goto out;

	DISPFUNC();

	n = mtkfb_get_debug_state(debug_buffer + n, debug_bufmax - n);
	n += primary_display_get_debug_state(debug_buffer + n, debug_bufmax - n);
	n += disp_sync_get_debug_info(debug_buffer + n, debug_bufmax - n);
	n += dprec_logger_get_result_string_all(debug_buffer + n, debug_bufmax - n);
	n += primary_display_check_path(debug_buffer + n, debug_bufmax - n);
	n += dprec_logger_get_buf(DPREC_LOGGER_ERROR, debug_buffer + n, debug_bufmax - n);
	n += dprec_logger_get_buf(DPREC_LOGGER_FENCE, debug_buffer + n, debug_bufmax - n);
	n += dprec_logger_get_buf(DPREC_LOGGER_DUMP, debug_buffer + n, debug_bufmax - n);
	n += dprec_logger_get_buf(DPREC_LOGGER_DEBUG, debug_buffer + n, debug_bufmax - n);
out:
	//debug_info_dump_to_printk();
	return simple_read_from_buffer(ubuf, count, ppos, debug_buffer, n);
}

static ssize_t debug_write(struct file *file,
                           const char __user *ubuf, size_t count, loff_t *ppos)
{
    const int debug_bufmax = sizeof(debug_buffer) - 1;
	size_t ret;

	ret = count;

	if (count > debug_bufmax)
        count = debug_bufmax;

	if (copy_from_user(&debug_buffer, ubuf, count))
		return -EFAULT;

	debug_buffer[count] = 0;

    process_dbg_cmd(debug_buffer);

    return ret;
}


static struct file_operations debug_fops = {
	.read  = debug_read,
    .write = debug_write,
	.open  = debug_open,
};

#ifdef MTKFB_DEBUG_FS_CAPTURE_LAYER_CONTENT_SUPPORT

static ssize_t layer_debug_open(struct inode *inode, struct file *file)
{
    MTKFB_LAYER_DBG_OPTIONS *dbgopt;
    ///record the private data
    file->private_data = inode->i_private;
    dbgopt = (MTKFB_LAYER_DBG_OPTIONS *)file->private_data;

    dbgopt->working_size = DISP_GetScreenWidth()*DISP_GetScreenHeight()*2 + 32;
    dbgopt->working_buf = vmalloc(dbgopt->working_size);
    if(dbgopt->working_buf == 0)
        DISP_LOG_PRINT(ANDROID_LOG_INFO, "DBG", "Vmalloc to get temp buffer failed\n");

    return 0;
}


static ssize_t layer_debug_read(struct file *file,
                          char __user *ubuf, size_t count, loff_t *ppos)
{
	return 0;
}


static ssize_t layer_debug_write(struct file *file,
                           const char __user *ubuf, size_t count, loff_t *ppos)
{
    MTKFB_LAYER_DBG_OPTIONS *dbgopt = (MTKFB_LAYER_DBG_OPTIONS *)file->private_data;

    DISP_LOG_PRINT(ANDROID_LOG_INFO, "DBG", "mtkfb_layer%d write is not implemented yet \n", dbgopt->layer_index);

    return count;
}

static int layer_debug_release(struct inode *inode, struct file *file)
{
    MTKFB_LAYER_DBG_OPTIONS *dbgopt;
    dbgopt = (MTKFB_LAYER_DBG_OPTIONS *)file->private_data;

    if(dbgopt->working_buf != 0)
        vfree((void *)dbgopt->working_buf);

    dbgopt->working_buf = 0;

    return 0;
}


static struct file_operations layer_debug_fops = {
	.read  = layer_debug_read,
    .write = layer_debug_write,
	.open  = layer_debug_open,
    .release = layer_debug_release,
};

#endif

void DBG_Init(void)
{
    mtkfb_dbgfs = debugfs_create_file("mtkfb",
        S_IFREG|S_IRUGO, NULL, (void *)0, &debug_fops);

    memset(&dbg_opt, 0, sizeof(dbg_opt));
    memset(&fps, 0, sizeof(fps));

    dbg_opt.log_fps_wnd_size = DEFAULT_LOG_FPS_WND_SIZE;
	// xuecheng, enable fps log by default
	dbg_opt.en_fps_log = 1;

#ifdef MTKFB_DEBUG_FS_CAPTURE_LAYER_CONTENT_SUPPORT
    {
        unsigned int i;
        unsigned char a[13];

        a[0] = 'm';
        a[1] = 't';
        a[2] = 'k';
        a[3] = 'f';
        a[4] = 'b';
        a[5] = '_';
        a[6] = 'l';
        a[7] = 'a';
        a[8] = 'y';
        a[9] = 'e';
        a[10] = 'r';
        a[11] = '0';
        a[12] = '\0';

        for(i=0;i<DDP_OVL_LAYER_MUN;i++)
        {
            a[11] = '0' + i;
            mtkfb_layer_dbg_opt[i].layer_index = i;
            mtkfb_layer_dbgfs[i] = debugfs_create_file(a,
                S_IFREG|S_IRUGO, NULL, (void *)&mtkfb_layer_dbg_opt[i], &layer_debug_fops);
        }
    }
#endif
}


void DBG_Deinit(void)
{
    debugfs_remove(mtkfb_dbgfs);
#ifdef MTKFB_DEBUG_FS_CAPTURE_LAYER_CONTENT_SUPPORT
    {
        unsigned int i;

        for(i=0;i<DDP_OVL_LAYER_MUN;i++)
            debugfs_remove(mtkfb_layer_dbgfs[i]);
    }
#endif
}
