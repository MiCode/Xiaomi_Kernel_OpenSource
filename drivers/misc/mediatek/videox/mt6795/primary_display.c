#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/rtpm_prio.h>
#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/ion_drv.h>
#include <linux/mtk_ion.h>

#include "debug.h"

#include "disp_drv_log.h"

#include "disp_lcm.h"
#include "disp_utils.h"
#include "mtkfb.h"

#include "ddp_hal.h"
#include "ddp_dump.h"
#include "ddp_path.h"
#include "ddp_drv.h"

#include "disp_session.h"

#include <mach/m4u.h>
#include <mach/m4u_port.h>
#include "primary_display.h"
#include "cmdq_def.h"
#include "cmdq_record.h"
#include "cmdq_reg.h"
#include "cmdq_core.h"

#include "ddp_manager.h"
#include "mtkfb_fence.h"
#include "disp_drv_platform.h"
#include "display_recorder.h"
#include "fbconfig_kdebug_rome.h"
#include "ddp_mmp.h"
#include "mtk_sync.h"
#include "ddp_irq.h"
#include <mach/mt_spm_idle.h>
#include "mach/eint.h"
#include <cust_eint.h>
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#include "disp_session.h"
#include "disp_helper.h"
#include <mach/mt_spm.h>  // for sodi reg addr define
#include "ddp_reg.h"
#include <mach/mt_idle.h>
#include <mach/mt_spm.h>  // for sodi reg addr define
#include <mach/mt_vcore_dvfs.h>
#include <mach/mt_smi.h>

extern 	int is_DAL_Enabled(void);
extern int dprec_mmp_dump_ovl_layer(OVL_CONFIG_STRUCT *ovl_layer,unsigned int l,unsigned int session/*1:primary, 2:external, 3:memory*/);
extern bool is_ipoh_bootup;
static bool ipoh_after = 0;
extern unsigned int isAEEEnabled;
int primary_display_use_cmdq = CMDQ_DISABLE;
int primary_display_use_m4u = 1;
DISP_PRIMARY_PATH_MODE primary_display_mode = DIRECT_LINK_MODE;

static unsigned long dim_layer_mva = 0;

enum DISP_USER{
	DISP_USER_HWC = 0,
	DISP_USER_FPS,
	DISP_USER_IDLE,
	DISP_USER_LAYER,
	DISP_USER_NORMAL,
	DISP_USER_MMDVFS,
	DISP_USER_TEST,
	DISP_USER_FORCE,
	DISP_USER_TUI
};

static int __primary_display_switch_mode(int sess_mode, unsigned int session, int need_lock, enum DISP_USER user);
static void primary_display_idlemgr_kick(char *source, int need_lock);
static void primary_display_idlemgr_enter_idle(int need_lock);
static void primary_display_idlemgr_leave_idle(int need_lock);
typedef void (*fence_release_callback) (unsigned int data);
static disp_internal_buffer_info *decouple_buffer_info[DISP_INTERNAL_BUFFER_COUNT];
static RDMA_CONFIG_STRUCT decouple_rdma_config;
static WDMA_CONFIG_STRUCT decouple_wdma_config;
static disp_mem_output_config mem_config;
atomic_t hwc_configing = ATOMIC_INIT(0);
static unsigned int primary_session_id = MAKE_DISP_SESSION(DISP_SESSION_PRIMARY,0);

#define FRM_UPDATE_SEQ_CACHE_NUM (DISP_INTERNAL_BUFFER_COUNT+1)
static disp_frm_seq_info frm_update_sequence[FRM_UPDATE_SEQ_CACHE_NUM];
static unsigned int frm_update_cnt;
static unsigned int gPresentFenceIndex = 0;

static const int one_layer_dl_condition = 5;
//DDP_SCENARIO_ENUM ddp_scenario = DDP_SCENARIO_SUB_RDMA1_DISP;
#ifdef DISP_SWITCH_DST_MODE
int primary_display_def_dst_mode = 0;
int primary_display_cur_dst_mode = 0;
#endif
#ifdef CONFIG_OF
extern unsigned int islcmconnected;
#endif
extern unsigned int _need_wait_esd_eof(void);
extern unsigned int _need_register_eint(void);
extern unsigned int _need_do_esd_check(void);
int primary_trigger_cnt = 0;
#define PRIMARY_DISPLAY_TRIGGER_CNT (1)
typedef struct {
	DISP_POWER_STATE			state;
	unsigned int				lcm_fps;
	int					        max_layer;
	int					        need_trigger_overlay;
	int					        need_trigger_ovl1to2;
	int					        need_trigger_dcMirror_out;
	DISP_PRIMARY_PATH_MODE 	    mode;
	unsigned int                session_id;
	int                         session_mode;
	int                         ovl1to2_mode;
	unsigned int				last_vsync_tick;
	unsigned long               framebuffer_mva;
	unsigned long               framebuffer_va;
	struct mutex 				lock;
	struct mutex 				capture_lock;
#ifdef DISP_SWITCH_DST_MODE
	struct mutex 				switch_dst_lock;
#endif
	disp_lcm_handle *			plcm;
	cmdqRecHandle 				cmdq_handle_config_esd;
	cmdqRecHandle 				cmdq_handle_config;
	disp_path_handle 			dpmgr_handle;
	disp_path_handle 			ovl2mem_path_handle;
	disp_path_handle 			dpmgr_handle_two_pipe;
	cmdqRecHandle 				cmdq_handle_ovl1to2_config;
	cmdqRecHandle 				cmdq_handle_trigger;
	cmdqRecHandle 				cmdq_dummy_trigger;
	char *			            mutex_locker;
	int				            vsync_drop;
	unsigned int				dc_buf_id;
	unsigned int				dc_buf[DISP_INTERNAL_BUFFER_COUNT];
	unsigned int				force_fps_keep_count;
	unsigned int				force_fps_skip_count;    
	cmdqBackupSlotHandle		cur_config_fence; 
	cmdqBackupSlotHandle		subtractor_when_free;
	cmdqBackupSlotHandle        rdma_buff_info;
	cmdqBackupSlotHandle        ovl_status_info;
	int							mmdvfs_power_state;	//0, low; 1, high
	int							delay_switch_to_1L_DL_needed;
	int							ovl_one_layer;
	int							test_mode;
}display_primary_path_context;

#define pgc	_get_context()

static display_primary_path_context* _get_context(void)
{
	static int is_context_inited = 0;
	static display_primary_path_context g_context;
	if (!is_context_inited) {
		memset((void *)&g_context, 0, sizeof(display_primary_path_context));
		/*default mmdvfs high*/
		g_context.mmdvfs_power_state = 1;
		is_context_inited = 1;
	}

	return &g_context;
}

/*add begin by lizhiye for esd lock*/
struct mutex esd_mode_switch_lock;
static void _primary_path_esd_check_lock()
{
	mutex_lock(&esd_mode_switch_lock); 
}

static void _primary_path_esd_check_unlock()
{
	mutex_unlock(&esd_mode_switch_lock);
}
/*add end by lizhiye for esd lock*/

static void _primary_path_lock(const char* caller)
{
	dprec_logger_start(DPREC_LOGGER_PRIMARY_MUTEX, 0, 0);
	disp_sw_mutex_lock(&(pgc->lock));
	pgc->mutex_locker = caller;
}

static void _primary_path_unlock(const char* caller)
{
	pgc->mutex_locker = NULL;
	disp_sw_mutex_unlock(&(pgc->lock));
	dprec_logger_done(DPREC_LOGGER_PRIMARY_MUTEX, 0, 0);
}

static void _primary_protect_mode_switch(void)
{
    int try_cnt = 50;
    while ((--try_cnt) && atomic_read(&hwc_configing)){
        msleep(1);
        //DISPCHECK("detecting protect mode switch\n");
    }
    if(try_cnt <= 0 ){
        DISPCHECK("display warning:switch mode when hwc config\n");
    }
    return;
}

int primary_display_is_directlink_mode(void)
{
	DISP_MODE mode = pgc->session_mode;
	if(mode == DISP_SESSION_DIRECT_LINK_MIRROR_MODE|| mode == DISP_SESSION_DIRECT_LINK_MODE)
		return 1;
	else
		return 0;
}

int primary_display_is_decouple_mode(void)
{
	DISP_MODE mode = pgc->session_mode;
	if(mode == DISP_SESSION_DECOUPLE_MODE|| mode == DISP_SESSION_DECOUPLE_MIRROR_MODE)
		return 1;
	else
		return 0;
}

int primary_display_is_mirror_mode(void)
{
	DISP_MODE mode = pgc->session_mode;
	if(mode == DISP_SESSION_DIRECT_LINK_MIRROR_MODE || mode == DISP_SESSION_DECOUPLE_MIRROR_MODE)
		return 1;
	else
		return 0;
}

struct task_struct *primary_display_frame_update_task = NULL;
wait_queue_head_t primary_display_frame_update_wq;
atomic_t primary_display_frame_update_event = ATOMIC_INIT(0);
wait_queue_head_t primary_display_present_fence_wq;
atomic_t primary_display_present_fence_update_event = ATOMIC_INIT(0);
#ifdef DISP_SWITCH_DST_MODE
unsigned long long last_primary_trigger_time;
bool is_switched_dst_mode = false;
static struct task_struct *primary_display_switch_dst_mode_task = NULL;
static void _primary_path_switch_dst_lock(void)
{
	mutex_lock(&(pgc->switch_dst_lock));
}
static void _primary_path_switch_dst_unlock(void)
{
	mutex_unlock(&(pgc->switch_dst_lock));
}

static int _disp_primary_path_switch_dst_mode_thread(void *data)
{
	int ret = 0;
	while(1)
	{   
		msleep(1000);

		if(((sched_clock() - last_primary_trigger_time)/1000) > 500000)//500ms not trigger disp
		{
			primary_display_switch_dst_mode(0);//switch to cmd mode
			is_switched_dst_mode = true;
		}
		if (kthread_should_stop())
			break;
	}
	return 0;
}
#endif

bool is_switched_dst_mode = false;
static struct task_struct *primary_display_switch_dst_mode_task = NULL;

static struct task_struct *primary_display_idlemgr_task = NULL;
static unsigned long long idlemgr_last_kick_time = ~(0ULL);
static DECLARE_WAIT_QUEUE_HEAD(idlemgr_wait_queue);
static int session_mode_before_enter_idle;
static int is_primary_idle = 0;
void _cmdq_insert_wait_frame_done_token_mira(void* handle);
static DECLARE_WAIT_QUEUE_HEAD(resume_wait_queue);

/* use MAX_SCHEDULE_TIMEOUT to wait for ever 
 * NOTES: primary_path_lock should NOT be held when call this func !!!!!!!!
 */
long primary_display_wait_resume(long timeout)
{
	long ret;
	ret = wait_event_timeout(resume_wait_queue, !primary_display_is_sleepd(), timeout);
	return ret;	
}

static int primary_display_is_idle(void)
{
	return is_primary_idle;
}

static int primary_display_set_idle_stat(int is_idle)
{
	int old_stat = is_primary_idle;
	is_primary_idle = is_idle;
	return old_stat;
}

static void primary_display_idlemgr_enter_idle(int need_lock)
{
	int ret = 0;
	DISPCHECK("idlemgr enter idle begin\n");
	if(primary_display_is_video_mode() &&
			!primary_display_is_mirror_mode() &&
			disp_helper_get_option(DISP_HELPER_OPTION_IDLEMGR_SWTCH_DECOUPLE)) {
		if (disp_helper_get_option(DISP_HELPER_OPTION_IDLEMGR_DISABLE_ROUTINE_IRQ)) {
			// disable routine irq before switch to decouple mode, otherwise we need to disable two paths
			dpmgr_path_enable_irq(pgc->dpmgr_handle, NULL, DDP_IRQ_LEVEL_ERROR);
		}
		if(!disp_helper_get_option(DISP_HELPER_OPTION_DEFAULT_DECOUPLE_MODE)){
			/* switch to decouple mode */
			session_mode_before_enter_idle = pgc->session_mode;
			__primary_display_switch_mode(DISP_SESSION_DECOUPLE_MODE, pgc->session_id, need_lock, DISP_USER_IDLE);
			mmdvfs_set_step(SMI_BWC_SCEN_UI_IDLE, MMDVFS_VOLTAGE_LOW);
		}

		if(pgc->plcm->params->dsi.vertical_frontporch_for_low_power) {
			cmdqRecHandle handle = NULL;
			ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &handle);
			if(ret==0) {
				cmdqRecReset(handle);
				cmdqRecClearEventToken(handle, CMDQ_EVENT_DISP_RDMA0_SOF);
				cmdqRecWait(handle, CMDQ_EVENT_DISP_RDMA0_SOF);
				if(pgc->plcm->params->lcm_if == LCM_INTERFACE_DSI_DUAL) {
					DISPMSG("set dsi dual vfp to %d for screen idle\n", pgc->plcm->params->dsi.vertical_frontporch_for_low_power);
					cmdqRecWrite(handle, 0x1401b028, pgc->plcm->params->dsi.vertical_frontporch_for_low_power, ~0);
					cmdqRecWrite(handle, 0x1401c028, pgc->plcm->params->dsi.vertical_frontporch_for_low_power, ~0);
				} else if(pgc->plcm->params->lcm_if == LCM_INTERFACE_DSI0) {
					DISPMSG("set dsi0 vfp to %d for screen idle\n", pgc->plcm->params->dsi.vertical_frontporch_for_low_power);
					cmdqRecWrite(handle, 0x1401b028, pgc->plcm->params->dsi.vertical_frontporch_for_low_power, ~0);
				} else if(pgc->plcm->params->lcm_if == LCM_INTERFACE_DSI1) {
					DISPMSG("set dsi1 vfp to %d for screen idle\n", pgc->plcm->params->dsi.vertical_frontporch_for_low_power);
					cmdqRecWrite(handle, 0x1401c028, pgc->plcm->params->dsi.vertical_frontporch_for_low_power, ~0);
				} else {
					ASSERT(0);
				}
				cmdqRecFlush(handle);
				cmdqRecDestroy(handle);
				mdelay(16);
			}else{
				DISPERR("vfp low power enter:Fail to create cmdq handle\n");
			}
		}
	}

	if (primary_display_is_video_mode()) {
		spm_enable_sodi(1);
		spm_sodi_mempll_pwr_mode(1);
	}
	DISPCHECK("idlemgr enter idle end\n");
	return ;
}

static void primary_display_idlemgr_leave_idle(int need_lock)
{
	int ret = 0;
	DISPCHECK("idlemgr leave idle begin\n");
	if (primary_display_is_video_mode()) {
		spm_enable_sodi(0);
	}

	if(primary_display_is_video_mode() &&
			disp_helper_get_option(DISP_HELPER_OPTION_IDLEMGR_SWTCH_DECOUPLE)) {

		if(pgc->plcm->params->dsi.vertical_frontporch_for_low_power) {
			cmdqRecHandle handle = NULL;
			ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &handle);
			if(ret==0) {
				cmdqRecReset(handle);
				cmdqRecClearEventToken(handle, CMDQ_EVENT_DISP_RDMA0_SOF);
				cmdqRecWait(handle, CMDQ_EVENT_DISP_RDMA0_SOF);
				if(pgc->plcm->params->lcm_if == LCM_INTERFACE_DSI_DUAL) {
					DISPMSG("set dsi dual vfp to %d for screen update\n", pgc->plcm->params->dsi.vertical_frontporch);
					cmdqRecWrite(handle, 0x1401b028, pgc->plcm->params->dsi.vertical_frontporch, ~0);
					cmdqRecWrite(handle, 0x1401c028, pgc->plcm->params->dsi.vertical_frontporch, ~0);
				} else if(pgc->plcm->params->lcm_if == LCM_INTERFACE_DSI0) {
					DISPMSG("set dsi0 vfp to %d for screen update\n", pgc->plcm->params->dsi.vertical_frontporch);
					cmdqRecWrite(handle, 0x1401b028, pgc->plcm->params->dsi.vertical_frontporch, ~0);
				} else if(pgc->plcm->params->lcm_if == LCM_INTERFACE_DSI1) {
					DISPMSG("set dsi1 vfp to %d for screen update\n", pgc->plcm->params->dsi.vertical_frontporch);
					cmdqRecWrite(handle, 0x1401c028, pgc->plcm->params->dsi.vertical_frontporch, ~0);
				} else {
					ASSERT(0);
				}
				cmdqRecFlush(handle);
				cmdqRecDestroy(handle);
				mdelay(16);
			}else{
				DISPERR("vfp low power leave:Fail to create cmdq handle\n");
			}
		}

		if(!disp_helper_get_option(DISP_HELPER_OPTION_DEFAULT_DECOUPLE_MODE)){
			mmdvfs_set_step(SMI_BWC_SCEN_UI_IDLE, MMDVFS_VOLTAGE_DEFAULT_STEP);
			/* switch to the mode before idle */
			__primary_display_switch_mode(session_mode_before_enter_idle, pgc->session_id, need_lock, DISP_USER_IDLE);
		}

		if (disp_helper_get_option(DISP_HELPER_OPTION_IDLEMGR_DISABLE_ROUTINE_IRQ)) {
			// enable routine irq after switch to directlink mode, otherwise we need to disable two paths
			dpmgr_path_enable_irq(pgc->dpmgr_handle, NULL, DDP_IRQ_LEVEL_ALL);
		}
	}
	DISPCHECK("idlemgr leave idle end\n");
	return;
}

static void primary_display_idlemgr_kick(char *source, int need_lock)
{
	MMProfileLogEx(ddp_mmp_get_events()->idlemgr, MMProfileFlagPulse, 1, 0);

	/* get primary lock to protect idlemgr_last_kick_time and primary_display_is_idle() */
	if(need_lock)
		_primary_path_lock(__func__);

	/* update kick timestamp */
	idlemgr_last_kick_time = sched_clock();

	if(primary_display_is_idle()) {
		primary_display_set_idle_stat(0);
		primary_display_idlemgr_leave_idle(0);

		MMProfileLogEx(ddp_mmp_get_events()->idlemgr, MMProfileFlagEnd, 0, 0);
		/* wake up idlemgr process to monitor next idle stat */
		wake_up_interruptible(&idlemgr_wait_queue);
	}

	if(need_lock)
		_primary_path_unlock(__func__);
}

static int _primary_path_idlemgr_monitor_thread(void *data)
{
	msleep(1000);
	while(1)
	{   
		msleep(1000);

		_primary_path_lock(__func__);

		if(pgc->state == DISP_SLEEPED) {
			_primary_path_unlock(__func__);
			primary_display_wait_resume(MAX_SCHEDULE_TIMEOUT);
			continue;
		}

		if(primary_display_is_idle()) {
			_primary_path_unlock(__func__);
			continue;
		}

		if(((local_clock() - idlemgr_last_kick_time)/1000) < 500*1000) {
			/* kicked in 500ms, it's not idle */
			_primary_path_unlock(__func__);
			continue;
		}
		/* enter idle state */
		primary_display_set_idle_stat(1);
		primary_display_idlemgr_enter_idle(0);

		MMProfileLogEx(ddp_mmp_get_events()->idlemgr, MMProfileFlagStart, 0, 0);
		DISPCHECK("primary enter idle state\n");

		_primary_path_unlock(__func__);

		wait_event_interruptible(idlemgr_wait_queue, !primary_display_is_idle());

		if (kthread_should_stop())
			break;
	}

	return 0;
}

static int primary_display_idlemgr_init(void)
{
	static int is_inited = 0;
	if(!is_inited) 
	{
		primary_display_set_idle_stat(0);
		idlemgr_last_kick_time = ~(0ULL);
		primary_display_idlemgr_task = kthread_create(_primary_path_idlemgr_monitor_thread, NULL, "disp_idlemgr");
		wake_up_process(primary_display_idlemgr_task);
		is_inited = 1;
	}

	return 0;
}

extern int disp_od_is_enabled(void);
int primary_display_get_debug_state(char* stringbuf, int buf_len)
{	
	int len = 0;
	LCM_PARAMS *lcm_param = disp_lcm_get_params(pgc->plcm);
	LCM_DRIVER *lcm_drv = pgc->plcm->drv;
	len +=
		scnprintf(stringbuf + len, buf_len - len,
				"|--------------------------------------------------------------------------------------|\n");
	len +=
		scnprintf(stringbuf + len, buf_len - len,
				"|********Primary Display Path General Information********\n");
	len +=
		scnprintf(stringbuf + len, buf_len - len, "|Primary Display is %s\n",
				dpmgr_path_is_idle(pgc->dpmgr_handle) ? "idle" : "busy");

	if (mutex_trylock(&(pgc->lock))) {
		mutex_unlock(&(pgc->lock));
		len +=
			scnprintf(stringbuf + len, buf_len - len,
					"|primary path global mutex is free\n");
	} else {
		len +=
			scnprintf(stringbuf + len, buf_len - len,
					"|primary path global mutex is hold by [%s]\n", pgc->mutex_locker);
	}

	if (lcm_param && lcm_drv)
		len +=
			scnprintf(stringbuf + len, buf_len - len,
					"|LCM Driver=[%s]\tResolution=%dx%d,Interface:%s\n", lcm_drv->name,
					lcm_param->width, lcm_param->height,
					(lcm_param->type == LCM_TYPE_DSI) ? "DSI" : "Other");

	len +=
		scnprintf(stringbuf + len, buf_len - len, "|OD is %s\n",
				disp_od_is_enabled() ? "enabled" : "disabled");

	len +=
		scnprintf(stringbuf + len, buf_len - len,
				"|State=%s\tlcm_fps=%d\tmax_layer=%d\tmode:%d\tvsync_drop=%d\n",
				pgc->state == DISP_ALIVE ? "Alive" : "Sleep", pgc->lcm_fps, pgc->max_layer,
				pgc->session_mode, pgc->vsync_drop);
	len +=
		scnprintf(stringbuf + len, buf_len - len,
				"|cmdq_handle_config=%p\tcmdq_handle_trigger=%p\tdpmgr_handle=%p\tovl2mem_path_handle=%p\n",
				pgc->cmdq_handle_config, pgc->cmdq_handle_trigger, pgc->dpmgr_handle,
				pgc->ovl2mem_path_handle);
	len +=
		scnprintf(stringbuf + len, buf_len - len, "|Current display driver status=%s + %s\n",
				primary_display_is_video_mode() ? "video mode" : "cmd mode",
				primary_display_cmdq_enabled() ? "CMDQ Enabled" : "CMDQ Disabled");

	return len;
}
int primary_display_is_dual_dsi(void);

static DISP_MODULE_ENUM _get_dst_module_by_lcm(disp_lcm_handle *plcm)
{
	if(plcm == NULL)
	{
		DISPERR("plcm is null\n");
		return DISP_MODULE_UNKNOWN;
	}

	if(plcm->params->type == LCM_TYPE_DSI)
	{
		if(plcm->lcm_if_id == LCM_INTERFACE_DSI0)
		{
			return DISP_MODULE_DSI0;
		}
		else if(plcm->lcm_if_id == LCM_INTERFACE_DSI1)
		{
			return DISP_MODULE_DSI1;
		}
		else if(plcm->lcm_if_id == LCM_INTERFACE_DSI_DUAL)
		{
			return DISP_MODULE_DSIDUAL;
		}
		else
		{
			return DISP_MODULE_DSI0;
		}
	}
	else if(plcm->params->type == LCM_TYPE_DPI)
	{
		return DISP_MODULE_DPI;
	}
	else
	{
		DISPERR("can't find primary path dst module\n");
		return DISP_MODULE_UNKNOWN;
	}
}

#define AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA

/***************************************************************
 ***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU  
 *** 1.wait idle:           N         N       Y        Y                 
 *** 2.lcm update:          N         Y       N        Y
 *** 3.path start:      	idle->Y      Y    idle->Y     Y                  
 *** 4.path trigger:     idle->Y      Y    idle->Y     Y  
 *** 5.mutex enable:        N         N    idle->Y     Y        
 *** 6.set cmdq dirty:      N         Y       N        N        
 *** 7.flush cmdq:          Y         Y       N        N        
 ****************************************************************/

int _should_wait_path_idle(void)
{
	/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU	
	 *** 1.wait idle:	          N         N        Y        Y 	 			*/
	if(primary_display_cmdq_enabled())
	{
		if(primary_display_is_video_mode())
		{
			return 0;
		}
		else
		{
			return 0;
		}
	}
	else
	{
		if(primary_display_is_video_mode())
		{
			return dpmgr_path_is_busy(pgc->dpmgr_handle);
		}
		else
		{
			return dpmgr_path_is_busy(pgc->dpmgr_handle);
		}
	}
}

int _should_update_lcm(void)
{
	/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU  
	 *** 2.lcm update:          N         Y       N        Y        **/
	if(primary_display_cmdq_enabled())
	{
		if(primary_display_is_video_mode())
		{
			return 0;
		}
		else
		{
			// TODO: lcm_update can't use cmdq now
			return 0;
		}
	}
	else
	{
		if(primary_display_is_video_mode())
		{
			return 0;
		}
		else
		{
			return 1;
		}
	}
}

int _should_start_path(void)
{
	/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU  
	 *** 3.path start:      	idle->Y      Y    idle->Y     Y        ***/
	if(primary_display_cmdq_enabled())
	{
		if(primary_display_is_video_mode())
		{
			return 0;
			//return dpmgr_path_is_idle(pgc->dpmgr_handle);
		}
		else
		{
			return 0;
		}
	}
	else
	{
		if(primary_display_is_video_mode())
		{
			return dpmgr_path_is_idle(pgc->dpmgr_handle);
		}
		else
		{
			return 1;
		}
	}
}

int _should_trigger_path(void)
{
	/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU              
	 *** 4.path trigger:     idle->Y      Y    idle->Y     Y     
	 *** 5.mutex enable:        N         N    idle->Y     Y        ***/

	// this is not a perfect design, we can't decide path trigger(ovl/rdma/dsi..) seperately with mutex enable
	// but it's lucky because path trigger and mutex enable is the same w/o cmdq, and it's correct w/ CMDQ(Y+N).
	if(primary_display_cmdq_enabled())
	{
		if(primary_display_is_video_mode())
		{
			return 0;
			//return dpmgr_path_is_idle(pgc->dpmgr_handle);
		}
		else
		{
			return 0;
		}
	}
	else
	{
		if(primary_display_is_video_mode())
		{
			return dpmgr_path_is_idle(pgc->dpmgr_handle);
		}
		else
		{
			return 1;
		}
	}
}

int _should_set_cmdq_dirty(void)
{
	/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU  
	 *** 6.set cmdq dirty:	    N         Y       N        N     ***/
	if(primary_display_cmdq_enabled())
	{
		if(primary_display_is_video_mode())
		{
			return 0;
		}
		else
		{
			return 1;
		}
	}
	else
	{
		if(primary_display_is_video_mode())
		{
			return 0;
		}
		else
		{
			return 0;
		}
	}
}

int _should_flush_cmdq_config_handle(void)
{
	/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU        
	 *** 7.flush cmdq:          Y         Y       N        N        ***/
	if(primary_display_cmdq_enabled())
	{
		if(primary_display_is_video_mode())
		{
			return 1;
		}
		else
		{
			return 1;
		}
	}
	else
	{
		if(primary_display_is_video_mode())
		{
			return 0;
		}
		else
		{
			return 0;
		}
	}
}

int _should_reset_cmdq_config_handle(void)
{
	if(primary_display_cmdq_enabled())
	{
		if(primary_display_is_video_mode())
		{
			return 1;
		}
		else
		{
			return 1;
		}
	}
	else
	{
		if(primary_display_is_video_mode())
		{
			return 0;
		}
		else
		{
			return 0;
		}
	}
}

int _should_insert_wait_frame_done_token(void)
{
	/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU    
	 *** 7.flush cmdq:          Y         Y       N        N      */  
	if(primary_display_cmdq_enabled())
	{
		if(primary_display_is_video_mode())
		{
			return 1;
		}
		else
		{
			return 1;
		}
	}
	else
	{
		if(primary_display_is_video_mode())
		{
			return 0;
		}
		else
		{
			return 0;
		}
	}
}

int _should_trigger_interface(void)
{
	if(pgc->mode == DECOUPLE_MODE)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

int _should_config_ovl_input(void)
{
	// should extend this when display path dynamic switch is ready
	if(pgc->mode == SINGLE_LAYER_MODE ||pgc->mode == DEBUG_RDMA1_DSI0_MODE)
		return 0;
	else
		return 1;

}

#define OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO
static long int get_current_time_us(void)
{
	struct timeval t;
	do_gettimeofday(&t);
	return (t.tv_sec & 0xFFF) * 1000000 + t.tv_usec;
}

static struct hrtimer cmd_mode_update_timer;
static ktime_t cmd_mode_update_timer_period;
static int is_fake_timer_inited = 0;
static enum hrtimer_restart _DISP_CmdModeTimer_handler(struct hrtimer *timer)
{
	DISPMSG("fake timer, wake up\n");
	dpmgr_signal_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);		
#if 0
	if((get_current_time_us() - pgc->last_vsync_tick) > 16666)
	{
		dpmgr_signal_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);		
		pgc->last_vsync_tick = get_current_time_us();
	}
#endif
	hrtimer_forward_now(timer, ns_to_ktime(16666666));
	return HRTIMER_RESTART;
}

int _init_vsync_fake_monitor(int fps)
{
	if(is_fake_timer_inited)
		return 0;

	is_fake_timer_inited = 1;

	if(fps == 0) 
	{
		fps = 6000;
	}

	hrtimer_init(&cmd_mode_update_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	cmd_mode_update_timer.function = _DISP_CmdModeTimer_handler;
	hrtimer_start(&cmd_mode_update_timer, ns_to_ktime(16666666), HRTIMER_MODE_REL);

	return 0;
}

static int _build_path_decouple(void)
{
	return 0;
}

static int _build_path_single_layer(void)
{
	return 0;
}

static int _build_path_debug_rdma1_dsi0(void)
{
	int ret = 0;

	DISP_MODULE_ENUM dst_module = 0;

	pgc->mode = DEBUG_RDMA1_DSI0_MODE;

	pgc->dpmgr_handle = dpmgr_create_path(DDP_SCENARIO_SUB_RDMA1_DISP, pgc->cmdq_handle_config);
	if(pgc->dpmgr_handle)
	{
		DISPCHECK("dpmgr create path SUCCESS(%p)\n", pgc->dpmgr_handle);
	}
	else
	{
		DISPCHECK("dpmgr create path FAIL\n");
		return -1;
	}

	dst_module = _get_dst_module_by_lcm(pgc->plcm);
	dpmgr_path_set_dst_module(pgc->dpmgr_handle, dst_module);
	DISPCHECK("dpmgr set dst module FINISHED(%s)\n", ddp_get_module_name(dst_module));

	if(disp_helper_get_option(DISP_HELPER_OPTION_USE_M4U))
	{
		M4U_PORT_STRUCT sPort;
		sPort.ePortID = M4U_PORT_DISP_RDMA1;
		sPort.Virtuality = disp_helper_get_option(DISP_HELPER_OPTION_USE_M4U);
		sPort.Security = 0;
		sPort.Distance = 1;
		sPort.Direction = 0;
		ret = m4u_config_port(&sPort);
		if (ret == 0) {
			DISPCHECK("config M4U Port %s to %s SUCCESS\n",
					ddp_get_module_name(DISP_MODULE_RDMA1),
					disp_helper_get_option(DISP_HELPER_OPTION_USE_M4U) ? "virtual" : "physical");
		} else {
			DISPCHECK("config M4U Port %s to %s FAIL(ret=%d)\n",
					ddp_get_module_name(DISP_MODULE_RDMA1),
					disp_helper_get_option(DISP_HELPER_OPTION_USE_M4U) ? "virtual" : "physical", ret);
			return -1;
		}
	}

	dpmgr_set_lcm_utils(pgc->dpmgr_handle, pgc->plcm->drv);

	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE);

	return ret;
}

static void _start_dummy_trigger_loop(void)
{
    /*cmdq will power off and clear all event flag like WDMA0_EOF when no task is running.
        * take decouple esd check for example,  trigger loop is stoped when esd check recovery,
        * and ovl-wmda is not working. cmdq will power off because no task is running. WDMA_EOF was
        *cleared and ovl-wmda will be timeout because wait wdma_eof timeout.
        * start this dummy trigger loop to disable cmdq power off
       */
    int ret = 0;
    if(pgc->cmdq_dummy_trigger == NULL){
           ret = cmdqRecCreate(CMDQ_SCENARIO_TRIGGER_LOOP, &pgc->cmdq_dummy_trigger);
           if( ret != 0 ) {
               DISPERR("fail to create dummy trigger loop handle\n");
               return;
           }
    }
    cmdqRecReset(pgc->cmdq_dummy_trigger);  
    cmdqRecWait(pgc->cmdq_dummy_trigger, CMDQ_EVENT_DISP_UFOE_EOF);
    cmdqRecStartLoop(pgc->cmdq_dummy_trigger);
}

static void _stop_dummy_trigger_loop(void)
{
    if(pgc->cmdq_dummy_trigger != NULL)
        cmdqRecStopLoop(pgc->cmdq_dummy_trigger);

}

static void _cmdq_build_trigger_loop(void)
{
	int ret = 0;
	if(pgc->cmdq_handle_trigger == NULL)
	{
		cmdqRecCreate(CMDQ_SCENARIO_TRIGGER_LOOP, &(pgc->cmdq_handle_trigger));
		DISPCHECK("primary path trigger thread cmd handle=%p\n", pgc->cmdq_handle_trigger);
	}
	cmdqRecReset(pgc->cmdq_handle_trigger);  

	if(primary_display_is_video_mode())
	{
		// wait and clear stream_done, HW will assert mutex enable automatically in frame done reset.
		// todo: should let dpmanager to decide wait which mutex's eof.
		ret = cmdqRecWait(pgc->cmdq_handle_trigger, CMDQ_EVENT_MUTEX0_STREAM_EOF);

		// for some module(like COLOR) to read hw register to GPR after frame done
		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,CMDQ_AFTER_STREAM_EOF, 0);
	}
	else
	{
		//ret = cmdqRecWaitNoClear(pgc->cmdq_handle_trigger, CMDQ_SYNC_TOKEN_CABC_EOF);
		// DSI command mode doesn't have mutex_stream_eof, need use CMDQ token instead
		ret = cmdqRecWait(pgc->cmdq_handle_trigger, CMDQ_SYNC_TOKEN_CONFIG_DIRTY);

		if(_need_wait_esd_eof())
		{
			// Wait esd config thread done.
			ret = cmdqRecWaitNoClear(pgc->cmdq_handle_trigger, CMDQ_SYNC_TOKEN_ESD_EOF);
		}
		//ret = cmdqRecWait(pgc->cmdq_handle_trigger, CMDQ_EVENT_MDP_DSI0_TE_SOF);
		// for operations before frame transfer, such as waiting for DSI TE
		if(islcmconnected)
		{
			dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,CMDQ_WAIT_LCM_TE, 0);
		}

		// cleat frame done token, now the config thread will not allowed to config registers.
		// remember that config thread's priority is higher than trigger thread, so all the config queued before will be applied then STREAM_EOF token be cleared
		// this is what CMDQ did as "Merge"
		ret = cmdqRecClearEventToken(pgc->cmdq_handle_trigger, CMDQ_SYNC_TOKEN_STREAM_EOF);

		ret = cmdqRecClearEventToken(pgc->cmdq_handle_trigger, CMDQ_SYNC_TOKEN_CONFIG_DIRTY);

		// clear rdma EOF token before wait
		ret = cmdqRecClearEventToken(pgc->cmdq_handle_trigger, CMDQ_EVENT_DISP_RDMA0_EOF);

		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,CMDQ_BEFORE_STREAM_SOF, 0);


#if 1
		// TODO: use seperate api for sodi control flow later
		// Enable EMI Force On(Equal to SODI Disable)
		cmdqRecWrite(pgc->cmdq_handle_trigger, 0x10006b0c, 1, 1);
		// Enable SPM CG Mode(Force 30+ times to ensure write success, need find root cause and fix later)
		cmdqRecWrite(pgc->cmdq_handle_trigger, 0x10006b04, 0x80, 0x80);
		// Polling EMI Status to ensure EMI is enabled
		cmdqRecPoll(pgc->cmdq_handle_trigger, 0x100063b4, 0, 0x00800000);
		// Clear EMI Force on, Let SPM control EMI state now
		cmdqRecWrite(pgc->cmdq_handle_trigger, 0x10006b0c, 0, 1);
#endif

		// enable mutex, only cmd mode need this
		// this is what CMDQ did as "Trigger"
		dpmgr_path_trigger(pgc->dpmgr_handle, pgc->cmdq_handle_trigger, CMDQ_ENABLE);

		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,CMDQ_AFTER_STREAM_SOF, 1);
		//ret = cmdqRecWrite(pgc->cmdq_handle_trigger, (unsigned int)(DISP_REG_CONFIG_MUTEX_EN(0))&0x1fffffff, 1, ~0);

		// waiting for frame done, because we can't use mutex stream eof here, so need to let dpmanager help to decide which event to wait
		// most time we wait rdmax frame done event.
		ret = cmdqRecWait(pgc->cmdq_handle_trigger, CMDQ_EVENT_DISP_RDMA0_EOF);  
		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,CMDQ_WAIT_STREAM_EOF_EVENT, 0);

		// dsi is not idle rightly after rdma frame done, so we need to polling about 1us for dsi returns to idle
		// do not polling dsi idle directly which will decrease CMDQ performance
		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,CMDQ_CHECK_IDLE_AFTER_STREAM_EOF, 0);

		// for some module(like COLOR) to read hw register to GPR after frame done
		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,CMDQ_AFTER_STREAM_EOF, 0);

		// Enable EMI Power Down Mode
		cmdqRecWrite(pgc->cmdq_handle_trigger, 0x10006b04, 0, 0x80);

		// polling DSI idle
		//ret = cmdqRecPoll(pgc->cmdq_handle_trigger, 0x1401b00c, 0, 0x80000000);
		// polling wdma frame done
		//ret = cmdqRecPoll(pgc->cmdq_handle_trigger, 0x140060A0, 1, 0x1);

		// now frame done, config thread is allowed to config register now
		ret = cmdqRecSetEventToken(pgc->cmdq_handle_trigger, CMDQ_SYNC_TOKEN_STREAM_EOF);

		// RUN forever!!!!
		BUG_ON(ret < 0);
	}

	// dump trigger loop instructions to check whether dpmgr_path_build_cmdq works correctly
	DISPCHECK("primary display BUILD cmdq trigger loop finished\n");

	return;
}

void disp_spm_enter_cg_mode(void)
{
	MMProfileLogEx(ddp_mmp_get_events()->cg_mode, MMProfileFlagPulse, 0, 0);
}

void disp_spm_enter_power_down_mode(void)
{
	MMProfileLogEx(ddp_mmp_get_events()->power_down_mode, MMProfileFlagPulse, 0, 0);
}

static void _cmdq_build_monitor_loop(void)
{
	int ret = 0;
	cmdqRecHandle				g_cmdq_handle_monitor;
	cmdqRecCreate(CMDQ_SCENARIO_DISP_SCREEN_CAPTURE, &(g_cmdq_handle_monitor));
	DISPCHECK("primary path monitor thread cmd handle=%p\n", g_cmdq_handle_monitor);

	cmdqRecReset(g_cmdq_handle_monitor);  

	// wait and clear stream_done, HW will assert mutex enable automatically in frame done reset.
	// todo: should let dpmanager to decide wait which mutex's eof.
	ret = cmdqRecWait(g_cmdq_handle_monitor, CMDQ_EVENT_DISP_RDMA0_UNDERRUN);

	cmdqRecReadToDataRegister(g_cmdq_handle_monitor, 0x10006b0c, CMDQ_DATA_REG_2D_SHARPNESS_1_DST);
	cmdqRecWriteFromDataRegister(g_cmdq_handle_monitor, CMDQ_DATA_REG_2D_SHARPNESS_1_DST, 0x1401b280);

	cmdqRecReadToDataRegister(g_cmdq_handle_monitor, 0x10006b08, CMDQ_DATA_REG_2D_SHARPNESS_1_DST);
	cmdqRecWriteFromDataRegister(g_cmdq_handle_monitor, CMDQ_DATA_REG_2D_SHARPNESS_1_DST, 0x1401b284);

	cmdqRecReadToDataRegister(g_cmdq_handle_monitor, 0x10006b04, CMDQ_DATA_REG_2D_SHARPNESS_1_DST);
	cmdqRecWriteFromDataRegister(g_cmdq_handle_monitor, CMDQ_DATA_REG_2D_SHARPNESS_1_DST, 0x1401b288);

	cmdqRecReadToDataRegister(g_cmdq_handle_monitor, 0x1401b16c, CMDQ_DATA_REG_2D_SHARPNESS_1_DST);
	cmdqRecWriteFromDataRegister(g_cmdq_handle_monitor, CMDQ_DATA_REG_2D_SHARPNESS_1_DST, 0x1401b28C);

	ret = cmdqRecStartLoop(g_cmdq_handle_monitor);

	return;
}

void _cmdq_start_trigger_loop(void)
{
	int ret = 0;

	//cmdqRecDumpCommand(pgc->cmdq_handle_trigger);
	
	// this should be called only once because trigger loop will nevet stop
	ret = cmdqRecStartLoop(pgc->cmdq_handle_trigger);
	if(!primary_display_is_video_mode())
	{
		if(_need_wait_esd_eof())
		{
			// Need set esd check eof synctoken to let trigger loop go.
			cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_ESD_EOF);
		}
		// need to set STREAM_EOF for the first time, otherwise we will stuck in dead loop
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_STREAM_EOF);
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_CABC_EOF);
		dprec_event_op(DPREC_EVENT_CMDQ_SET_EVENT_ALLOW);
	}
	else
	{
#if 0
		if(dpmgr_path_is_idle(pgc->dpmgr_handle))
		{
			cmdqCoreSetEvent(CMDQ_EVENT_MUTEX0_STREAM_EOF);
		}
#endif
	}

	DISPCHECK("primary display START cmdq trigger loop finished\n");
}

void _cmdq_stop_trigger_loop(void)
{
	int ret = 0;

	// this should be called only once because trigger loop will nevet stop
	ret = cmdqRecStopLoop(pgc->cmdq_handle_trigger);

	DISPDBG("primary display STOP cmdq trigger loop finished\n");
}


static void _cmdq_set_config_handle_dirty(void)
{
	if(!primary_display_is_video_mode())
	{
		dprec_logger_start(DPREC_LOGGER_PRIMARY_CMDQ_SET_DIRTY, 0, 0);
		// only command mode need to set dirty
		cmdqRecSetEventToken(pgc->cmdq_handle_config, CMDQ_SYNC_TOKEN_CONFIG_DIRTY);
		dprec_event_op(DPREC_EVENT_CMDQ_SET_DIRTY);
		dprec_logger_done(DPREC_LOGGER_PRIMARY_CMDQ_SET_DIRTY, 0, 0);
	}
}

static void _cmdq_handle_clear_dirty(cmdqRecHandle cmdq_handle)
{
	if(!primary_display_is_video_mode())
	{
		dprec_logger_trigger(DPREC_LOGGER_PRIMARY_CMDQ_SET_DIRTY, 0, 1);
		cmdqRecClearEventToken(cmdq_handle, CMDQ_SYNC_TOKEN_CONFIG_DIRTY);
	}
}

static void _cmdq_set_config_handle_dirty_mira(void *handle)
{
	if(!primary_display_is_video_mode())
	{
		dprec_logger_start(DPREC_LOGGER_PRIMARY_CMDQ_SET_DIRTY, 0, 0);
		// only command mode need to set dirty
		cmdqRecSetEventToken(handle, CMDQ_SYNC_TOKEN_CONFIG_DIRTY);
		dprec_event_op(DPREC_EVENT_CMDQ_SET_DIRTY);
		dprec_logger_done(DPREC_LOGGER_PRIMARY_CMDQ_SET_DIRTY, 0, 0);
	}
}

static void _cmdq_reset_config_handle(void)
{
	cmdqRecReset(pgc->cmdq_handle_config);
	dprec_event_op(DPREC_EVENT_CMDQ_RESET);
}

static void _cmdq_flush_config_handle(int blocking, void *callback, unsigned int userdata)
{
	dprec_logger_start(DPREC_LOGGER_PRIMARY_CMDQ_FLUSH, blocking, callback);
	if(blocking)
	{
		//DISPERR("Should not use blocking cmdq flush, may block primary display path for 1 frame period\n");
		cmdqRecFlush(pgc->cmdq_handle_config);
	}
	else
	{
		if(callback)
			cmdqRecFlushAsyncCallback(pgc->cmdq_handle_config, callback, userdata);
		else
			cmdqRecFlushAsync(pgc->cmdq_handle_config);
	}

	dprec_event_op(DPREC_EVENT_CMDQ_FLUSH);
	dprec_logger_done(DPREC_LOGGER_PRIMARY_CMDQ_FLUSH, userdata, 0);	

	if(dprec_option_enabled())
	{
		cmdqRecDumpCommand(pgc->cmdq_handle_config);
	}
}

static void _cmdq_flush_config_handle_mira(void* handle, int blocking)
{
	dprec_logger_start(DPREC_LOGGER_PRIMARY_CMDQ_FLUSH, 0, 0);
	if(blocking)
	{
		cmdqRecFlush(handle);
	}
	else
	{
		cmdqRecFlushAsync(handle);
	}
	dprec_event_op(DPREC_EVENT_CMDQ_FLUSH);
	dprec_logger_done(DPREC_LOGGER_PRIMARY_CMDQ_FLUSH, 0, 0);	
}

static void _cmdq_insert_wait_frame_done_token(void)
{
	if(primary_display_is_video_mode())
	{
		cmdqRecWaitNoClear(pgc->cmdq_handle_config, CMDQ_EVENT_MUTEX0_STREAM_EOF);
	}
	else
	{
		cmdqRecWaitNoClear(pgc->cmdq_handle_config, CMDQ_SYNC_TOKEN_STREAM_EOF);
	}

	dprec_event_op(DPREC_EVENT_CMDQ_WAIT_STREAM_EOF);
}

void _cmdq_insert_wait_frame_done_token_mira(void* handle)
{
	if(primary_display_is_video_mode())
	{
		cmdqRecWaitNoClear(handle, CMDQ_EVENT_MUTEX0_STREAM_EOF);
	}
	else
	{
		cmdqRecWaitNoClear(handle, CMDQ_SYNC_TOKEN_STREAM_EOF);
	}

	dprec_event_op(DPREC_EVENT_CMDQ_WAIT_STREAM_EOF);
}

static void update_frm_seq_info(unsigned int addr, unsigned int addr_offset,unsigned int seq, DISP_FRM_SEQ_STATE state)
{
	int i= 0;

	if(FRM_CONFIG == state)
	{
		frm_update_sequence[frm_update_cnt].state = state;
		frm_update_sequence[frm_update_cnt].mva= addr;
		frm_update_sequence[frm_update_cnt].max_offset= addr_offset;
		if(seq > 0)
			frm_update_sequence[frm_update_cnt].seq= seq;
		MMProfileLogEx(ddp_mmp_get_events()->primary_seq_config, MMProfileFlagPulse, addr, seq);

	}
	else if(FRM_TRIGGER == state)
	{
		frm_update_sequence[frm_update_cnt].state = FRM_TRIGGER;
		MMProfileLogEx(ddp_mmp_get_events()->primary_seq_trigger, MMProfileFlagPulse, frm_update_cnt, frm_update_sequence[frm_update_cnt].seq);

		dprec_logger_frame_seq_begin(pgc->session_id,  frm_update_sequence[frm_update_cnt].seq);

		frm_update_cnt++;
		frm_update_cnt%=FRM_UPDATE_SEQ_CACHE_NUM;


	}
	else if(FRM_START == state)
	{
		for(i= 0; i< FRM_UPDATE_SEQ_CACHE_NUM; i++)
		{            
			if((abs(addr -frm_update_sequence[i].mva) <=  frm_update_sequence[i].max_offset)
					&& (frm_update_sequence[i].state == FRM_TRIGGER))
			{
				MMProfileLogEx(ddp_mmp_get_events()->primary_seq_rdma_irq, MMProfileFlagPulse, 
						frm_update_sequence[i].mva, frm_update_sequence[i].seq);
				frm_update_sequence[i].state = FRM_START;
				dprec_logger_frame_seq_end(pgc->session_id,  frm_update_sequence[i].seq );
				dprec_logger_frame_seq_begin(0,  frm_update_sequence[i].seq);
				///break;
			}
		}
	}
	else if(FRM_END == state)
	{
		for(i= 0; i< FRM_UPDATE_SEQ_CACHE_NUM; i++)
		{
			if(FRM_START == frm_update_sequence[i].state)
			{
				frm_update_sequence[i].state = FRM_END;
				dprec_logger_frame_seq_end(0,  frm_update_sequence[i].seq );
				MMProfileLogEx(ddp_mmp_get_events()->primary_seq_release, MMProfileFlagPulse, 
						frm_update_sequence[i].mva, frm_update_sequence[i].seq);

			}
		}
	}

}

static int _config_wdma_output(WDMA_CONFIG_STRUCT *wdma_config,
		disp_path_handle disp_handle,
		cmdqRecHandle cmdq_handle)
{
	disp_ddp_path_config *pconfig =dpmgr_path_get_last_config(disp_handle);
	pconfig->wdma_config = *wdma_config;
	pconfig->wdma_dirty = 1;
	dpmgr_path_config(disp_handle, pconfig, cmdq_handle);
	return 0;
}

static int _config_rdma_input_data(RDMA_CONFIG_STRUCT *rdma_config,
		disp_path_handle disp_handle,
		cmdqRecHandle cmdq_handle)
{
	disp_ddp_path_config *pconfig =dpmgr_path_get_last_config(disp_handle);
	pconfig->rdma_config = *rdma_config;
	pconfig->rdma_dirty = 1;
	dpmgr_path_config(disp_handle, pconfig, cmdq_handle);
	return 0;
}

static void directlink_path_add_memory(WDMA_CONFIG_STRUCT * p_wdma)
{	
	int ret = 0;
	cmdqRecHandle cmdq_handle= NULL;
	cmdqRecHandle cmdq_wait_handle = NULL;
	disp_ddp_path_config *pconfig =NULL;

	/*create config thread*/
	ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP,&cmdq_handle);
	if(ret!=0)
	{
		DISPCHECK("dl_to_dc capture:Fail to create cmdq handle\n");
		ret = -1;
		goto out;
	}
	cmdqRecReset(cmdq_handle);

	/*create wait thread*/
	ret = cmdqRecCreate(CMDQ_SCENARIO_DISP_SCREEN_CAPTURE,&cmdq_wait_handle);
	if(ret!=0)
	{
		DISPCHECK("dl_to_dc capture:Fail to create cmdq wait handle\n");
		ret = -1;
		goto out;
	}		 
	cmdqRecReset(cmdq_wait_handle);

	/*configure  config thread*/
	_cmdq_insert_wait_frame_done_token_mira(cmdq_handle);

	dpmgr_path_add_memout(pgc->dpmgr_handle, ENGINE_OVL0, cmdq_handle);

	pconfig = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	pconfig->wdma_config = *p_wdma;

	if(disp_helper_get_option(DISP_HELPER_OPTION_DECOUPLE_MODE_USE_RGB565))
	{
		pconfig->wdma_config.outputFormat = eRGB565;
		pconfig->wdma_config.dstPitch = pconfig->wdma_config.srcWidth * 2;
	}

	pconfig->wdma_dirty  = 1;	
	ret = dpmgr_path_config(pgc->dpmgr_handle, pconfig, cmdq_handle);

	_cmdq_set_config_handle_dirty_mira(cmdq_handle);
	_cmdq_flush_config_handle_mira(cmdq_handle, 0);
	DISPMSG("dl_to_dc capture:Flush add memout mva(0x%lx)\n",p_wdma->dstAddress);

	/*wait wdma0 sof*/
	cmdqRecWait(cmdq_wait_handle,CMDQ_EVENT_DISP_WDMA0_SOF);
	cmdqRecFlush(cmdq_wait_handle);
	DISPMSG("dl_to_dc capture:Flush wait wdma sof\n");
#if 0
	cmdqRecReset(cmdq_handle);
	_cmdq_insert_wait_frame_done_token_mira(cmdq_handle);

	dpmgr_path_remove_memout(pgc->dpmgr_handle, cmdq_handle);
	_cmdq_set_config_handle_dirty_mira(cmdq_handle);
	//flush remove memory to cmdq
	_cmdq_flush_config_handle_mira(cmdq_handle, 0);
	DISPMSG("dl_to_dc capture: Flush remove memout\n");

	dpmgr_path_memout_clock(pgc->dpmgr_handle, 0);
#endif
out:
	cmdqRecDestroy(cmdq_handle);
	cmdqRecDestroy(cmdq_wait_handle);
	return;
}

#define DISP_REG_SODI_PA 0x10006b0c
void disp_enable_emi_force_on(unsigned int enable, void* cmdq_handle)
{
	if(cmdq_handle!=NULL)
	{
		if(enable==1)
			cmdqRecWrite(cmdq_handle, DISP_REG_SODI_PA, 0, 1);
		else
			cmdqRecWrite(cmdq_handle, DISP_REG_SODI_PA, 1, 1);
	}
	else
	{
		if(enable==0)
			DISP_REG_SET(0, SPM_PCM_SRC_REQ, DISP_REG_GET(SPM_PCM_SRC_REQ)&(~0x1));
		else
			DISP_REG_SET(0, SPM_PCM_SRC_REQ, DISP_REG_GET(SPM_PCM_SRC_REQ)|0x1);
	}
}

static int _DL_switch_to_DC_fast_two_pipe(void)
{
	int ret = 0;
	DDP_SCENARIO_ENUM old_scenario, new_scenario, new_scenario2;
	RDMA_CONFIG_STRUCT rdma_config = decouple_rdma_config;
	RDMA_CONFIG_STRUCT rdma_config2 = decouple_rdma_config;
	WDMA_CONFIG_STRUCT wdma_config = decouple_wdma_config;	

	ddp_set_dst_module(DDP_SCENARIO_PRIMARY_RDMA0_DISP, DISP_MODULE_DSI0);
	ddp_set_dst_module(DDP_SCENARIO_SUB_RDMA2_DISP, DISP_MODULE_DSI1);

	pgc->dpmgr_handle_two_pipe = dpmgr_create_path(DDP_SCENARIO_SUB_RDMA2_DISP, pgc->cmdq_handle_config);
	dpmgr_path_set_video_mode(pgc->dpmgr_handle_two_pipe, primary_display_is_video_mode());

	disp_ddp_path_config *data_config_dl = NULL;
	disp_ddp_path_config *data_config_dc = NULL;
	unsigned int mva = pgc->dc_buf[pgc->dc_buf_id];
	wdma_config.dstAddress = mva;

	// 1.save a temp frame to intermediate buffer
	directlink_path_add_memory(&wdma_config);

	MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse, 1, 0);


	// 2.reset primary handle
	_cmdq_reset_config_handle();
	_cmdq_handle_clear_dirty(pgc->cmdq_handle_config);
	_cmdq_insert_wait_frame_done_token();


	// 3.modify interface path handle to new scenario(rdma->dsi)
	old_scenario = dpmgr_get_scenario(pgc->dpmgr_handle);
#if CONFIG_FOR_SOURCE_PQ
	new_scenario = DDP_SCENARIO_PRIMARY_RDMA0_DISP;
#else
	new_scenario = DDP_SCENARIO_PRIMARY_RDMA0_DISP;
	new_scenario2 = DDP_SCENARIO_SUB_RDMA2_DISP;
#endif

	dpmgr_modify_path_power_on_new_modules(pgc->dpmgr_handle, new_scenario, 0);

	dpmgr_modify_path(pgc->dpmgr_handle, new_scenario, pgc->cmdq_handle_config,
			primary_display_is_video_mode()?DDP_VIDEO_MODE:DDP_CMD_MODE, 0);

	dpmgr_path_init(pgc->dpmgr_handle_two_pipe, disp_helper_get_option(DISP_HELPER_OPTION_USE_CMDQ));
	if(disp_helper_get_option(DISP_HELPER_OPTION_DECOUPLE_MODE_USE_RGB565))
	{
		rdma_config.inputFormat = eRGB565;
		rdma_config.width = rdma_config.width/2;
		rdma_config.pitch = primary_display_get_width() * 2;
		rdma_config.address = mva;
	}
	else
	{
		// 4.config rdma0 from directlink mode to memory mode
		rdma_config.address = mva;
		rdma_config.width = rdma_config.width/2;
		rdma_config.pitch = primary_display_get_width() * 3;
	}	

	data_config_dl = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	// disable ufor lr mode for dsi0
	data_config_dl->dispif_config.dsi.ufoe_enable = 0;
	data_config_dl->dispif_config.dsi.ufoe_params.lr_mode_en = 0;
	data_config_dl->dst_w = primary_display_get_width()/2;
	data_config_dl->dst_dirty = 1;
	ret = dpmgr_path_config(pgc->dpmgr_handle, data_config_dl, pgc->cmdq_handle_config);

	data_config_dl = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	data_config_dl->rdma_config = rdma_config;
	data_config_dl->rdma_dirty = 1;
	ret = dpmgr_path_config(pgc->dpmgr_handle, data_config_dl, pgc->cmdq_handle_config);

	if(disp_helper_get_option(DISP_HELPER_OPTION_DECOUPLE_MODE_USE_RGB565))
	{
		rdma_config2.inputFormat = eRGB565;
		rdma_config2.pitch = primary_display_get_width() * 2;
		rdma_config2.address = mva + rdma_config2.width/2*2;
	}
	else
	{
		rdma_config2.inputFormat = eRGB888;
		rdma_config2.pitch = primary_display_get_width() * 3;
		rdma_config2.address = mva + rdma_config2.width/2*3;
	}
	rdma_config2.width = rdma_config2.width/2;


	// 4.a config rdma2 from directlink mode to memory mode
	data_config_dl = dpmgr_path_get_last_config(pgc->dpmgr_handle_two_pipe);
	data_config_dl->dispif_config = dpmgr_path_get_last_config(pgc->dpmgr_handle)->dispif_config;
	data_config_dl->dst_w = primary_display_get_width()/2;
	data_config_dl->dst_h = primary_display_get_height();
	data_config_dl->dst_dirty = 1;
	ret = dpmgr_path_config(pgc->dpmgr_handle_two_pipe, data_config_dl, pgc->cmdq_handle_config);

	data_config_dl = dpmgr_path_get_last_config(pgc->dpmgr_handle_two_pipe);
	data_config_dl->rdma_config = rdma_config2;
	data_config_dl->rdma_dirty = 1;
	ret = dpmgr_path_config(pgc->dpmgr_handle_two_pipe, data_config_dl, pgc->cmdq_handle_config);

	// for dsi, .config must be called AFTER .init
	dpmgr_path_start(pgc->dpmgr_handle_two_pipe, disp_helper_get_option(DISP_HELPER_OPTION_USE_CMDQ));
	dpmgr_path_trigger(pgc->dpmgr_handle_two_pipe, pgc->cmdq_handle_config, disp_helper_get_option(DISP_HELPER_OPTION_USE_CMDQ));

	//5. backup rdma address to slots
	cmdqRecBackupUpdateSlot(pgc->cmdq_handle_config, pgc->rdma_buff_info, 0, mva);
	cmdqRecBackupUpdateSlot(pgc->cmdq_handle_config, pgc->rdma_buff_info, 1, rdma_config.pitch);

	//6 .flush to cmdq
	_cmdq_set_config_handle_dirty();

	// xuecheng, for debug
	DISPMSG("dump cmdq command before switch to 2 pipe path\n");
	cmdqRecDumpCommand(pgc->cmdq_handle_config);

	_cmdq_flush_config_handle(1, NULL, 0);
	dpmgr_check_status(pgc->dpmgr_handle);
	dpmgr_check_status(pgc->dpmgr_handle_two_pipe);

	dpmgr_modify_path_power_off_old_modules(old_scenario, new_scenario, 0);

	MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse, 2, 0);

	//ddp_mmp_rdma_layer(&rdma_config, 0,  20, 20);

	//7.reset  cmdq
	_cmdq_reset_config_handle();
	_cmdq_handle_clear_dirty(pgc->cmdq_handle_config);
	_cmdq_insert_wait_frame_done_token();

	//9. create ovl2mem path handle
	cmdqRecReset(pgc->cmdq_handle_ovl1to2_config);
#if CONFIG_FOR_SOURCE_PQ
	pgc->ovl2mem_path_handle = dpmgr_create_path(DDP_SCENARIO_PRIMARY_DITHER_MEMOUT, pgc->cmdq_handle_ovl1to2_config);
#else
	pgc->ovl2mem_path_handle = dpmgr_create_path(DDP_SCENARIO_PRIMARY_OVL_MEMOUT, pgc->cmdq_handle_ovl1to2_config);
#endif
	if(pgc->ovl2mem_path_handle)
	{
		DISPCHECK("dpmgr create ovl memout path SUCCESS(%p)\n", pgc->ovl2mem_path_handle);
	}
	else
	{
		DISPCHECK("dpmgr create path FAIL\n");
		return -1;
	}

	dpmgr_path_set_video_mode(pgc->ovl2mem_path_handle, 0);
	dpmgr_path_init(pgc->ovl2mem_path_handle, CMDQ_ENABLE);

	data_config_dc = dpmgr_path_get_last_config(pgc->ovl2mem_path_handle);
	data_config_dc->dst_w = rdma_config.width;
	data_config_dc->dst_h = rdma_config.height;
	data_config_dc->dst_dirty = 1;

	/* move ovl config info from dl to dc */
	memcpy(data_config_dc->ovl_config, data_config_dl->ovl_config, sizeof(data_config_dl->ovl_config));

	ret = dpmgr_path_config(pgc->ovl2mem_path_handle, data_config_dc, pgc->cmdq_handle_ovl1to2_config);
	ret = dpmgr_path_start(pgc->ovl2mem_path_handle, CMDQ_ENABLE);

	// use blocking flush to make sure all config is done.

	//cmdqRecDumpCommand(pgc->cmdq_handle_ovl1to2_config);
	cmdqRecClearEventToken(pgc->cmdq_handle_ovl1to2_config ,CMDQ_EVENT_DISP_WDMA0_EOF);
	_cmdq_flush_config_handle_mira(pgc->cmdq_handle_ovl1to2_config, 1);

	cmdqRecReset(pgc->cmdq_handle_ovl1to2_config);

	MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse, 3, 0);

	// 11..enable event for new path
	//	  dpmgr_enable_event(pgc->ovl2mem_path_handle, DISP_PATH_EVENT_FRAME_COMPLETE);
	//	  dpmgr_map_event_to_irq(pgc->ovl2mem_path_handle, DISP_PATH_EVENT_FRAME_START, DDP_IRQ_WDMA0_FRAME_COMPLETE);
	//	  dpmgr_enable_event(pgc->ovl2mem_path_handle, DISP_PATH_EVENT_FRAME_START);

	if(primary_display_is_video_mode())
	{
		dpmgr_map_event_to_irq(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC, DDP_IRQ_RDMA0_DONE);
	}

	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
	//dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);

	if(!primary_display_is_video_mode())
	{
		msleep(30);
		_cmdq_stop_trigger_loop();
		_cmdq_build_trigger_loop();
		_cmdq_start_trigger_loop();
	}

out:
	return ret;
}

int decouple_shorter_path = 0;

static int _DL_switch_to_DC_fast(void)
{
	int ret = 0;
	DDP_SCENARIO_ENUM old_scenario, new_scenario;
	RDMA_CONFIG_STRUCT rdma_config = decouple_rdma_config;
	WDMA_CONFIG_STRUCT wdma_config = decouple_wdma_config;

	disp_ddp_path_config *data_config_dl = NULL;
	disp_ddp_path_config *data_config_dc = NULL;
	unsigned int mva = pgc->dc_buf[pgc->dc_buf_id];
	wdma_config.dstAddress = mva;

	// 1.save a temp frame to intermediate buffer
	directlink_path_add_memory(&wdma_config);

	MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse, 1, 0);

	// 2.reset primary handle
	_cmdq_reset_config_handle();
	/* no need to trigger one frame */
	//_cmdq_handle_clear_dirty(pgc->cmdq_handle_config);
	_cmdq_insert_wait_frame_done_token();

	// 3.modify interface path handle to new scenario(rdma->dsi)
	old_scenario = dpmgr_get_scenario(pgc->dpmgr_handle);
#if CONFIG_FOR_SOURCE_PQ
	new_scenario = DDP_SCENARIO_PRIMARY_RDMA0_DISP;
#else
	if(decouple_shorter_path) {
		new_scenario = DDP_SCENARIO_PRIMARY_RDMA0_DISP;
	} else {
		new_scenario = DDP_SCENARIO_PRIMARY_RDMA0_COLOR0_DISP;
	}
#endif
	dpmgr_modify_path_power_on_new_modules(pgc->dpmgr_handle, new_scenario, 0);

	dpmgr_modify_path(pgc->dpmgr_handle, new_scenario, pgc->cmdq_handle_config,
			primary_display_is_video_mode()?DDP_VIDEO_MODE:DDP_CMD_MODE, 0);


	// 4.config rdma from directlink mode to memory mode
	rdma_config.address = mva;

	data_config_dl = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	data_config_dl->rdma_config = rdma_config;
	data_config_dl->rdma_dirty = 1;
	ret = dpmgr_path_config(pgc->dpmgr_handle, data_config_dl, pgc->cmdq_handle_config);

	//5. backup rdma address to slots
	cmdqRecBackupUpdateSlot(pgc->cmdq_handle_config, pgc->rdma_buff_info, 0, mva);
	cmdqRecBackupUpdateSlot(pgc->cmdq_handle_config, pgc->rdma_buff_info, 1, rdma_config.pitch);

	//6 .flush to cmdq
	//_cmdq_set_config_handle_dirty();
	_cmdq_flush_config_handle(1, NULL, 0);

	dpmgr_modify_path_power_off_old_modules(old_scenario, new_scenario, 0);

	MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse, 2, 0);

	// ddp_mmp_rdma_layer(&rdma_config, 0,  20, 20);

	//7.reset  cmdq
	_cmdq_reset_config_handle();
	_cmdq_handle_clear_dirty(pgc->cmdq_handle_config);
	_cmdq_insert_wait_frame_done_token();

	//9. create ovl2mem path handle
	cmdqRecReset(pgc->cmdq_handle_ovl1to2_config);
#if CONFIG_FOR_SOURCE_PQ
	pgc->ovl2mem_path_handle = dpmgr_create_path(DDP_SCENARIO_PRIMARY_DITHER_MEMOUT, pgc->cmdq_handle_ovl1to2_config);
#else
	pgc->ovl2mem_path_handle = dpmgr_create_path(DDP_SCENARIO_PRIMARY_OVL_MEMOUT, pgc->cmdq_handle_ovl1to2_config);
#endif
	if(pgc->ovl2mem_path_handle) {
		DISPCHECK("dpmgr create ovl memout path SUCCESS(%p)\n", pgc->ovl2mem_path_handle);
	} else {
		DISPCHECK("dpmgr create path FAIL\n");
		return -1;
	}

	dpmgr_path_set_video_mode(pgc->ovl2mem_path_handle, 0);
	dpmgr_path_init(pgc->ovl2mem_path_handle, CMDQ_ENABLE);

	data_config_dc = dpmgr_path_get_last_config(pgc->ovl2mem_path_handle);
	data_config_dc->dst_w = rdma_config.width;
	data_config_dc->dst_h = rdma_config.height;
	data_config_dc->dst_dirty = 1;

	/* move ovl config info from dl to dc */
	memcpy(data_config_dc->ovl_config, data_config_dl->ovl_config, sizeof(data_config_dl->ovl_config));

	ret = dpmgr_path_config(pgc->ovl2mem_path_handle, data_config_dc, pgc->cmdq_handle_ovl1to2_config);
	ret = dpmgr_path_start(pgc->ovl2mem_path_handle, CMDQ_ENABLE);

	// use blocking flush to make sure all config is done.

	//cmdqRecDumpCommand(pgc->cmdq_handle_ovl1to2_config);
	cmdqRecClearEventToken(pgc->cmdq_handle_ovl1to2_config ,CMDQ_EVENT_DISP_WDMA0_EOF);
	_cmdq_flush_config_handle_mira(pgc->cmdq_handle_ovl1to2_config, 1);

	cmdqRecReset(pgc->cmdq_handle_ovl1to2_config);

	MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse, 3, 0);

	// 11..enable event for new path
	//    dpmgr_enable_event(pgc->ovl2mem_path_handle, DISP_PATH_EVENT_FRAME_COMPLETE);
	//    dpmgr_map_event_to_irq(pgc->ovl2mem_path_handle, DISP_PATH_EVENT_FRAME_START, DDP_IRQ_WDMA0_FRAME_COMPLETE);
	//    dpmgr_enable_event(pgc->ovl2mem_path_handle, DISP_PATH_EVENT_FRAME_START);

	if(primary_display_is_video_mode()) {
		dpmgr_map_event_to_irq(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC, DDP_IRQ_RDMA0_DONE);
	}
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
	//dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);

	if(!primary_display_is_video_mode()) {
		/* make sure hardware is idle now */
		msleep(17);
		_cmdq_stop_trigger_loop();
		_cmdq_build_trigger_loop();
		_cmdq_start_trigger_loop();
	}

out:
	return ret;
}

static int DL_switch_to_DC_fast_two_pipe(int sw_only)
{
	int ret = 0;

	if(!sw_only)
	{
		_DL_switch_to_DC_fast_two_pipe();
	}
	else
		ret = -1; /*not support yet */

	return ret;
}


static int DL_switch_to_DC_fast(int sw_only)
{
	int ret = 0;

	if (!sw_only) {
		ret = _DL_switch_to_DC_fast();
	} else {
		ret = -1; /*not support yet */
	}
	return ret;
}


static void modify_path_power_off_callback(uint32_t userdata)
{
	DDP_SCENARIO_ENUM old_scenario, new_scenario;
	old_scenario = userdata >> 16;
	new_scenario = userdata & ((1<<16)-1);
	dpmgr_modify_path_power_off_old_modules(old_scenario, new_scenario, 0);

	/* release output buffer */
	int layer = disp_sync_get_output_interface_timeline_id();
	mtkfb_release_layer_fence(primary_session_id, layer);
}

static int _DC_switch_to_DL_fast(void)
{
	int ret = 0;
	int layer = 0;
	disp_ddp_path_config *data_config_dl = NULL;
	disp_ddp_path_config *data_config_dc = NULL;
	OVL_CONFIG_STRUCT  ovl_config[4];
	DDP_SCENARIO_ENUM old_scenario, new_scenario;

	// 3.destroy ovl->mem path.
	data_config_dc = dpmgr_path_get_last_config(pgc->ovl2mem_path_handle);
	/*save ovl info*/;
	memcpy(ovl_config, data_config_dc->ovl_config, sizeof(ovl_config));

	dpmgr_path_deinit(pgc->ovl2mem_path_handle, pgc->cmdq_handle_ovl1to2_config);
	dpmgr_destroy_path(pgc->ovl2mem_path_handle, pgc->cmdq_handle_ovl1to2_config);
	/*clear sof token for next dl to dc*/
	cmdqRecClearEventToken(pgc->cmdq_handle_ovl1to2_config ,CMDQ_EVENT_DISP_WDMA0_SOF);

	_cmdq_flush_config_handle_mira(pgc->cmdq_handle_ovl1to2_config, 1);
	cmdqRecReset(pgc->cmdq_handle_ovl1to2_config);
	pgc->ovl2mem_path_handle=NULL;

	MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse, 1, 1);

	/* release output buffer */
	layer = disp_sync_get_output_timeline_id();
	mtkfb_release_layer_fence(primary_session_id, layer);

	// 4.modify interface path handle to new scenario(rdma->dsi)
	_cmdq_reset_config_handle();
	/*no need to trigger one frame */
	//_cmdq_handle_clear_dirty(pgc->cmdq_handle_config);
	_cmdq_insert_wait_frame_done_token();

	if(disp_helper_get_option(DISP_HELPER_OPTION_TWO_PIPE_INTERFACE_PATH) && pgc->dpmgr_handle_two_pipe) {
		DISPCHECK("will deinit two pipe path\n");
		dpmgr_path_deinit(pgc->dpmgr_handle_two_pipe, pgc->cmdq_handle_config);
		dpmgr_destroy_path(pgc->dpmgr_handle_two_pipe, pgc->cmdq_handle_config);
		pgc->dpmgr_handle_two_pipe = NULL;
	}

	old_scenario = dpmgr_get_scenario(pgc->dpmgr_handle);
	new_scenario = DDP_SCENARIO_PRIMARY_DISP;

	dpmgr_modify_path_power_on_new_modules(pgc->dpmgr_handle, new_scenario, 0);

	dpmgr_modify_path(pgc->dpmgr_handle, new_scenario, pgc->cmdq_handle_config,
			primary_display_is_video_mode()?DDP_VIDEO_MODE:DDP_CMD_MODE, 0);

	// always re-config ufoe no matter whether we switched to 2 pipe path
	if(disp_helper_get_option(DISP_HELPER_OPTION_TWO_PIPE_INTERFACE_PATH)) {
		data_config_dl = dpmgr_path_get_last_config(pgc->dpmgr_handle);	
		data_config_dl->dispif_config.dsi.ufoe_enable = 1;
		data_config_dl->dispif_config.dsi.ufoe_params.lr_mode_en = 1;
		data_config_dl->dst_w = primary_display_get_width();
		data_config_dl->dst_h = primary_display_get_height();
		data_config_dl->dst_dirty = 1;
		ret = dpmgr_path_config(pgc->dpmgr_handle, data_config_dl, pgc->cmdq_handle_config);
	}

	// 5.config rdma from memory mode to directlink mode
	data_config_dl = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	data_config_dl->rdma_config = decouple_rdma_config;
	data_config_dl->rdma_config.address = 0;
	data_config_dl->rdma_config.pitch = 0;
	data_config_dl->rdma_dirty = 1;
	memcpy(data_config_dl->ovl_config, ovl_config, sizeof(ovl_config));
	ret = dpmgr_path_config(pgc->dpmgr_handle, data_config_dl, pgc->cmdq_handle_config);

	cmdqRecBackupUpdateSlot(pgc->cmdq_handle_config, pgc->rdma_buff_info, 0, 0);

	//cmdqRecDumpCommand(pgc->cmdq_handle_config);
	/*no need to trigger one frame */
	//_cmdq_set_config_handle_dirty();
	/*if blocking flush won't cause UX issue, we should simplify this code: remove callback *
	 *else we should move disable_sodi to callback, and change to nonblocking flush */
	_cmdq_flush_config_handle(1, modify_path_power_off_callback, (old_scenario<<16) | new_scenario);
	modify_path_power_off_callback((old_scenario<<16) | new_scenario);

	MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse, 2, 1);

	_cmdq_reset_config_handle();
	_cmdq_handle_clear_dirty(pgc->cmdq_handle_config);
	_cmdq_insert_wait_frame_done_token();

	// 9.enable event for new path
	if(primary_display_is_video_mode()) {
		dpmgr_map_event_to_irq(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC, DDP_IRQ_RDMA0_DONE);
	}
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);

	MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse, 3, 1);

	if(!primary_display_is_video_mode()) {
		/* make sure hardware is idle when restart trigger loop*/
		msleep(17);
		_cmdq_stop_trigger_loop();
		_cmdq_build_trigger_loop();
		_cmdq_start_trigger_loop();
	}

out:
	return ret;
}

static int _DC_switch_to_DL_sw_only(void)
{
	int ret = 0;
	int layer = 0;
	disp_ddp_path_config *data_config_dl = NULL;
	disp_ddp_path_config *data_config_dc = NULL;
	OVL_CONFIG_STRUCT  ovl_config[4];
	DDP_SCENARIO_ENUM old_scenario, new_scenario;

	// 3.destroy ovl->mem path.
	data_config_dc = dpmgr_path_get_last_config(pgc->ovl2mem_path_handle);
	/*save ovl info*/;
	memcpy(ovl_config, data_config_dc->ovl_config, sizeof(ovl_config));

	dpmgr_destroy_path_handle(pgc->ovl2mem_path_handle);

	cmdqRecReset(pgc->cmdq_handle_ovl1to2_config);
	pgc->ovl2mem_path_handle=NULL;

	MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse, 1, 1);

	/* release output buffer */
	layer = disp_sync_get_output_timeline_id();
	mtkfb_release_layer_fence(primary_session_id, layer);

	// 4.modify interface path handle to new scenario(rdma->dsi)
	_cmdq_reset_config_handle();
	_cmdq_handle_clear_dirty(pgc->cmdq_handle_config);
	_cmdq_insert_wait_frame_done_token();

	old_scenario = dpmgr_get_scenario(pgc->dpmgr_handle);
	new_scenario = DDP_SCENARIO_PRIMARY_DISP;
	dpmgr_modify_path_power_on_new_modules(pgc->dpmgr_handle, new_scenario, 1);
	dpmgr_modify_path(pgc->dpmgr_handle, new_scenario, pgc->cmdq_handle_config, 
			primary_display_is_video_mode()?DDP_VIDEO_MODE:DDP_CMD_MODE, 1);

	// 5.config rdma from memory mode to directlink mode
	data_config_dl = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	data_config_dl->rdma_config = decouple_rdma_config;
	data_config_dl->rdma_config.address = 0;
	data_config_dl->rdma_config.pitch = 0;
	data_config_dl->rdma_dirty = 1;
	memcpy(data_config_dl->ovl_config, ovl_config, sizeof(ovl_config));
	ret = dpmgr_path_config(pgc->dpmgr_handle, data_config_dl, pgc->cmdq_handle_config);

	cmdqRecBackupUpdateSlot(pgc->cmdq_handle_config, pgc->rdma_buff_info, 0, 0);
	dpmgr_modify_path_power_off_old_modules(old_scenario, new_scenario, 1);

	/* release output buffer */
	layer = disp_sync_get_output_interface_timeline_id();
	mtkfb_release_layer_fence(primary_session_id, layer);

	_cmdq_reset_config_handle();
	_cmdq_handle_clear_dirty(pgc->cmdq_handle_config);
	_cmdq_insert_wait_frame_done_token();

	// 9.enable event for new path
	if(primary_display_is_video_mode()) {
		dpmgr_map_event_to_irq(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC, DDP_IRQ_RDMA0_DONE);
	}

	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);

	if(!primary_display_is_video_mode()) {
		_cmdq_build_trigger_loop();
	}
	MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse, 3, 1);
out:
	return ret;
}

static int DC_switch_to_DL_fast(int sw_only)
{
	int ret = 0;

	if(!sw_only)
		ret = _DC_switch_to_DL_fast();
	else
		ret = _DC_switch_to_DL_sw_only();

	return ret;
}

const char* session_mode_spy(unsigned int mode)
{
	switch(mode)
	{
		case DISP_SESSION_DIRECT_LINK_MODE:
			return "DIRECT_LINK";
		case DISP_SESSION_DIRECT_LINK_MIRROR_MODE:
			return "DIRECT_LINK_MIRROR";
		case DISP_SESSION_DECOUPLE_MODE:
			return "DECOUPLE";
		case DISP_SESSION_DECOUPLE_MIRROR_MODE:
			return "DECOUPLE_MIRROR";
		default:
			return "UNKNOWN";
	}
}

static int config_display_m4u_port()
{
	int ret = 0;

	if(disp_helper_get_option(DISP_HELPER_OPTION_USE_M4U))
	{
		M4U_PORT_STRUCT sPort;
		sPort.ePortID = M4U_PORT_DISP_OVL0;
		sPort.Virtuality = primary_display_use_m4u;                    
		sPort.Security = 0;
		sPort.Distance = 1;
		sPort.Direction = 0;
		ret = m4u_config_port(&sPort);
		if(ret == 0)
		{
			DISPCHECK("config M4U Port %s to %s SUCCESS\n",ddp_get_module_name(DISP_MODULE_OVL0), primary_display_use_m4u?"virtual":"physical");
		}
		else
		{
			DISPCHECK("config M4U Port %s to %s FAIL(ret=%d)\n",ddp_get_module_name(DISP_MODULE_OVL0), primary_display_use_m4u?"virtual":"physical", ret);
			return -1;
		}
		sPort.ePortID = M4U_PORT_DISP_RDMA0;
		ret = m4u_config_port(&sPort);
		if(ret)
		{
			DISPCHECK("config M4U Port %s to %s FAIL(ret=%d)\n",ddp_get_module_name(DISP_MODULE_RDMA0), primary_display_use_m4u?"virtual":"physical", ret);
			return -1;
		}

		sPort.ePortID = M4U_PORT_DISP_RDMA2;
		ret = m4u_config_port(&sPort);
		if(ret)
		{
			DISPCHECK("config M4U Port %s to %s FAIL(ret=%d)\n",ddp_get_module_name(DISP_MODULE_RDMA2), primary_display_use_m4u?"virtual":"physical", ret);
			return -1;
		}

		sPort.ePortID = M4U_PORT_DISP_WDMA0;
		ret = m4u_config_port(&sPort);
		if(ret)
		{
			DISPCHECK("config M4U Port %s to %s FAIL(ret=%d)\n",ddp_get_module_name(DISP_MODULE_WDMA0), primary_display_use_m4u?"virtual":"physical", ret);
			return -1;
		}
	}

	return ret;
}

static disp_internal_buffer_info *allocat_decouple_buffer(int size)
{
	int i = 0;
	void *buffer_va = NULL;
	unsigned int buffer_mva = 0;
	unsigned int mva_size = 0;

	struct ion_mm_data mm_data;
	memset((void*)&mm_data, 0, sizeof(struct ion_mm_data));

	struct ion_client *client = NULL;
	struct ion_handle *handle = NULL;
	disp_internal_buffer_info * buf_info = NULL;
	client = ion_client_create(g_ion_device, "disp_decouple");

	buf_info = kzalloc(sizeof(disp_internal_buffer_info), GFP_KERNEL);
	if (buf_info){
		handle = ion_alloc(client, size, 0, ION_HEAP_MULTIMEDIA_MASK, 0);
		if (IS_ERR(handle)){
			DISPERR("Fatal Error, ion_alloc for size %d failed\n", size);
			ion_free(client,handle);
			ion_client_destroy(client);
			kfree(buf_info);
			return NULL;
		}

		buffer_va = ion_map_kernel(client, handle);
		if (buffer_va == NULL){
			DISPERR("ion_map_kernrl failed\n");
			ion_free(client,handle);
			ion_client_destroy(client);
			kfree(buf_info);
			return NULL;
		}
		mm_data.config_buffer_param.kernel_handle = handle;
		mm_data.mm_cmd = ION_MM_CONFIG_BUFFER;
		if (ion_kernel_ioctl(client, ION_CMD_MULTIMEDIA, (unsigned long)&mm_data) < 0){
			DISPERR("ion_test_drv: Config buffer failed.\n");
			ion_free(client,handle);
			ion_client_destroy(client);
			kfree(buf_info);
			return NULL;
		}

		ion_phys(client, handle, &buffer_mva, &mva_size);
		if(buffer_mva == 0){
			DISPERR("Fatal Error, get mva failed\n");
			ion_free(client,handle);
			ion_client_destroy(client);
			kfree(buf_info);
			return NULL;
		}
		buf_info->handle = handle;
		buf_info->mva = buffer_mva;
		buf_info->size = mva_size;
		buf_info->va = buffer_va;
		DISPMSG("allocat_decouple_buffer, mva=0x%08x, size=0x%08x, va=0x%p\n", (unsigned int)buf_info->mva, (unsigned int)buf_info->size, buf_info->va);
	}else{
		DISPERR("Fatal error, kzalloc internal buffer info failed!!\n");
		kfree(buf_info);
		return NULL;
	}

	return buf_info;
}

static int init_decouple_buffers()
{
	int i =0;
	int height = primary_display_get_height();
	int width = disp_helper_get_option(DISP_HELPER_OPTION_FAKE_LCM_WIDTH);    
	int bpp = primary_display_get_bpp();

	int buffer_size =  width * height * bpp /8;

	for(i = 0; i < DISP_INTERNAL_BUFFER_COUNT; i++){
		decouple_buffer_info[i] = allocat_decouple_buffer(buffer_size);
		if(decouple_buffer_info[i] != NULL){
			pgc->dc_buf[i] = decouple_buffer_info[i]->mva;         
		}
	}

	/*initialize rdma config*/
	decouple_rdma_config.height 			= height;
	decouple_rdma_config.width 				= width;
	decouple_rdma_config.idx 				= 0;
	decouple_rdma_config.inputFormat 		= eRGB888;
	decouple_rdma_config.pitch 				= width * DP_COLOR_BITS_PER_PIXEL(eRGB888)/8;

	/*initialize wdma config*/
	decouple_wdma_config.srcHeight          = height;
	decouple_wdma_config.srcWidth 			= width;
	decouple_wdma_config.clipX              = 0;
	decouple_wdma_config.clipY              = 0;
	decouple_wdma_config.clipHeight         = height;
	decouple_wdma_config.clipWidth          = width;
	decouple_wdma_config.outputFormat       = eRGB888; 
	decouple_wdma_config.useSpecifiedAlpha  = 1;
	decouple_wdma_config.alpha              = 0xFF;
	decouple_wdma_config.dstPitch           = width * DP_COLOR_BITS_PER_PIXEL(eRGB888)/8;

	return 0;

}

static int _build_path_direct_link(void)
{
	int ret = 0;

	DISP_MODULE_ENUM dst_module = 0;
	DISPFUNC(); 
	pgc->mode = DIRECT_LINK_MODE;

	pgc->dpmgr_handle = dpmgr_create_path(DDP_SCENARIO_PRIMARY_DISP, pgc->cmdq_handle_config);
	if(pgc->dpmgr_handle)
	{
		DISPCHECK("dpmgr create path SUCCESS(%p)\n", pgc->dpmgr_handle);
	}
	else
	{
		DISPCHECK("dpmgr create path FAIL\n");
		return -1;
	}

	config_display_m4u_port();
	init_decouple_buffers();
	dpmgr_set_lcm_utils(pgc->dpmgr_handle, pgc->plcm->drv);

	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);

	return ret;
}

static int _convert_disp_input_to_rdma(RDMA_CONFIG_STRUCT *dst, primary_disp_input_config* src)
{
	if(src && dst)
	{    		
		dst->inputFormat = src->fmt;		
		dst->address = src->addr;  
		dst->width = src->src_w;
		dst->height = src->src_h;
		dst->pitch = src->src_pitch;

		return 0;
	}
	else
	{
		DISPERR("src(%p) or dst(%p) is null\n", src, dst);
		return -1;
	}
}

static int _convert_disp_input_to_ovl(OVL_CONFIG_STRUCT *dst, primary_disp_input_config* src)
{
	if(src && dst)
	{
		dst->layer = src->layer;
		dst->layer_en = src->layer_en;
		dst->source = src->buffer_source;        
		dst->fmt = src->fmt;
		dst->addr = src->addr;  
		dst->vaddr = src->vaddr;
		dst->src_x = src->src_x;
		dst->src_y = src->src_y;
		dst->src_w = src->src_w;
		dst->src_h = src->src_h;
		dst->src_pitch = src->src_pitch;
		dst->dst_x = src->dst_x;
		dst->dst_y = src->dst_y;
		dst->dst_w = src->dst_w;
		dst->dst_h = src->dst_h;
		dst->keyEn = src->keyEn;
		dst->key = src->key; 
		dst->aen = src->aen; 
		dst->alpha = src->alpha;
		dst->sur_aen = src->sur_aen;
		dst->src_alpha = src->src_alpha;
		dst->dst_alpha = src->dst_alpha;

		dst->isDirty = src->isDirty;

		dst->buff_idx = src->buff_idx;
		dst->identity = src->identity;
		dst->connected_type = src->connected_type;
		dst->security = src->security;
		dst->yuv_range = src->yuv_range;     

		return 0;
	}
	else
	{
		DISPERR("src(%p) or dst(%p) is null\n", src, dst);
		return -1;
	}
}

int _trigger_display_interface(int blocking, void *callback, unsigned int userdata)
{
	if(_should_wait_path_idle())
		dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ*1);

	if(_should_update_lcm())
		disp_lcm_update(pgc->plcm, 0, 0, pgc->plcm->params->width, pgc->plcm->params->height, 0);

	if(_should_start_path())
		dpmgr_path_start(pgc->dpmgr_handle, primary_display_cmdq_enabled());

	if(_should_trigger_path())
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, primary_display_cmdq_enabled());

	if(_should_set_cmdq_dirty())
		_cmdq_set_config_handle_dirty();

	if(_should_flush_cmdq_config_handle())
		_cmdq_flush_config_handle(blocking, callback, userdata);

	if(_should_reset_cmdq_config_handle())
		_cmdq_reset_config_handle();

	/* clear cmdq dirty incase trigger loop starts here */
	if(_should_set_cmdq_dirty())
		_cmdq_handle_clear_dirty(pgc->cmdq_handle_config);

	if(_should_insert_wait_frame_done_token())
		_cmdq_insert_wait_frame_done_token();

	return 0;
}

int _trigger_ovl_to_memory(disp_path_handle disp_handle,
		cmdqRecHandle cmdq_handle,
		fence_release_callback callback,
		unsigned int data)
{
	dpmgr_path_trigger(disp_handle, cmdq_handle, CMDQ_ENABLE);
	cmdqRecWaitNoClear(cmdq_handle, CMDQ_EVENT_DISP_WDMA0_EOF);

	cmdqRecBackupUpdateSlot(cmdq_handle, pgc->rdma_buff_info, 0, mem_config.addr);

	cmdqRecFlushAsyncCallback(cmdq_handle,callback,data);
	cmdqRecReset(cmdq_handle);
	cmdqRecWait(cmdq_handle, CMDQ_EVENT_DISP_WDMA0_EOF);
	MMProfileLogEx(ddp_mmp_get_events()->ovl_trigger, MMProfileFlagPulse, 0, data);
}

int _trigger_ovl_to_memory_mirror(disp_path_handle disp_handle,
		cmdqRecHandle cmdq_handle,
		fence_release_callback callback,
		unsigned int data)
{
	int layer = 0;
	dpmgr_path_trigger(disp_handle, cmdq_handle, CMDQ_ENABLE);

	cmdqRecWaitNoClear(cmdq_handle, CMDQ_EVENT_DISP_WDMA0_EOF);

	layer = disp_sync_get_output_timeline_id();
	cmdqRecBackupUpdateSlot(cmdq_handle, pgc->cur_config_fence, layer, mem_config.buff_idx);

	layer = disp_sync_get_output_interface_timeline_id();
	cmdqRecBackupUpdateSlot(cmdq_handle, pgc->cur_config_fence, layer, mem_config.interface_idx);

	cmdqRecBackupUpdateSlot(cmdq_handle, pgc->rdma_buff_info, 0, (unsigned int)mem_config.addr);
	cmdqRecBackupUpdateSlot(cmdq_handle, pgc->rdma_buff_info, 1, (unsigned int)mem_config.pitch);

	cmdqRecFlushAsyncCallback(cmdq_handle,callback,data);
	cmdqRecReset(cmdq_handle);
	cmdqRecWait(cmdq_handle, CMDQ_EVENT_DISP_WDMA0_EOF);
	MMProfileLogEx(ddp_mmp_get_events()->ovl_trigger, MMProfileFlagPulse, 0, data);
}

int _trigger_overlay_engine(void)
{
	/* maybe we need a simple merge mechanism for CPU config. */
	dpmgr_path_trigger(pgc->ovl2mem_path_handle, NULL, disp_helper_get_option(DISP_HELPER_OPTION_USE_CMDQ));
}

#define EEEEEEEEEEEEEEE
/******************************************************************************/
/* ESD CHECK / RECOVERY ---- BEGIN                                            */
/******************************************************************************/
static struct task_struct *primary_display_esd_check_task = NULL;

static wait_queue_head_t  esd_check_task_wq; // For Esd Check Task
static atomic_t esd_check_task_wakeup = ATOMIC_INIT(0); // For Esd Check Task
static wait_queue_head_t  esd_ext_te_wq; // For Vdo Mode EXT TE Check
static atomic_t esd_ext_te_event = ATOMIC_INIT(0); // For Vdo Mode EXT TE Check
static atomic_t esd_check_bycmdq = ATOMIC_INIT(0);
static int eint_flag = 0; // For DCT Setting

unsigned int _need_do_esd_check(void)
{
	int ret = 0;
#ifdef CONFIG_OF
	if((pgc->plcm->params->dsi.esd_check_enable == 1)&&(islcmconnected == 1))
	{
		ret = 1;
	}
#else
	if(pgc->plcm->params->dsi.esd_check_enable == 1)
	{
		ret = 1;
	}
#endif
	return ret;
}


unsigned int _need_register_eint(void)
{

	int ret = 1;

	// 1.need do esd check
	// 2.dsi vdo mode
	// 3.customization_esd_check_enable = 0
	if(_need_do_esd_check() == 0)
	{
		ret = 0;
	}
	else if(primary_display_is_video_mode() == 0)
	{
		ret = 0;
	}
	else if(pgc->plcm->params->dsi.customization_esd_check_enable == 1)
	{
		ret = 0;
	}

	return ret;

}
unsigned int _need_wait_esd_eof(void)
{
	int ret = 1;

	// 1.need do esd check
	// 2.customization_esd_check_enable = 1
	// 3.dsi cmd mode
	if(_need_do_esd_check() == 0)
	{
		ret = 0;
	}
	else if(pgc->plcm->params->dsi.customization_esd_check_enable == 0)
	{
		ret = 0;
	}
	else if(primary_display_is_video_mode())
	{
		ret = 0;
	}

	return ret;
}
// For Cmd Mode Read LCM Check
// Config cmdq_handle_config_esd
int _esd_check_config_handle_cmd(void)
{
	int ret = 0; // 0:success

	// 1.reset
	cmdqRecReset(pgc->cmdq_handle_config_esd);

	// 2.write first instruction
	// cmd mode: wait CMDQ_SYNC_TOKEN_STREAM_EOF(wait trigger thread done)
	cmdqRecWaitNoClear(pgc->cmdq_handle_config_esd, CMDQ_SYNC_TOKEN_STREAM_EOF);

	// 3.clear CMDQ_SYNC_TOKEN_ESD_EOF(trigger thread need wait this sync token)
	cmdqRecClearEventToken(pgc->cmdq_handle_config_esd, CMDQ_SYNC_TOKEN_ESD_EOF);

	// 4.write instruction(read from lcm)
	dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_config_esd,CMDQ_ESD_CHECK_READ, 0);

	// 5.set CMDQ_SYNC_TOKE_ESD_EOF(trigger thread can work now)
	cmdqRecSetEventToken(pgc->cmdq_handle_config_esd, CMDQ_SYNC_TOKEN_ESD_EOF);

	// 6.flush instruction
	dprec_logger_start(DPREC_LOGGER_ESD_CMDQ, 0, 0);
	ret = cmdqRecFlush(pgc->cmdq_handle_config_esd);
	dprec_logger_done(DPREC_LOGGER_ESD_CMDQ, 0, 0);


	DISPCHECK("[ESD]_esd_check_config_handle_cmd ret=%d\n",ret);

done:
	if(ret) ret=1;
	return ret;
}

void primary_display_esd_cust_bycmdq(int enable)
{
	atomic_set(&esd_check_bycmdq, enable);
}

int primary_display_esd_cust_get()
{
	return atomic_read(&esd_check_bycmdq);
}

// For Vdo Mode Read LCM Check
// Config cmdq_handle_config_esd
int _esd_check_config_handle_vdo(void)
{
	int ret = 0; // 0:success , 1:fail

	primary_display_esd_cust_bycmdq(1);
	// 1.reset
	cmdqRecReset(pgc->cmdq_handle_config_esd);

	// add by LCT xuwenda 2015-11-20 for panic reboot begin
	_primary_path_lock(__func__);
	// add by LCT xuwenda 2015-11-20 for panic reboot end

	// 2.stop dsi vdo mode
	dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_config_esd,CMDQ_STOP_VDO_MODE, 0);

	// 3.write instruction(read from lcm)
	dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_config_esd,CMDQ_ESD_CHECK_READ, 0);

	// 4.start dsi vdo mode
	dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_config_esd,CMDQ_START_VDO_MODE, 0);
	cmdqRecClearEventToken(pgc->cmdq_handle_config_esd, CMDQ_EVENT_MUTEX0_STREAM_EOF);

	// 5. trigger path
	dpmgr_path_trigger(pgc->dpmgr_handle, pgc->cmdq_handle_config_esd, CMDQ_ENABLE);

	// add by LCT xuwenda 2015-11-20 for panic reboot begin
	_primary_path_unlock(__func__);
	// add by LCT xuwenda 2015-11-20 for panic reboot end

	// 6.flush instruction
	dprec_logger_start(DPREC_LOGGER_ESD_CMDQ, 0, 0);
	ret = cmdqRecFlush(pgc->cmdq_handle_config_esd);
	dprec_logger_done(DPREC_LOGGER_ESD_CMDQ, 0, 0);

	DISPCHECK("[ESD]_esd_check_config_handle_vdo ret=%d\n",ret);

done:
	if(ret) ret=1;

	primary_display_esd_cust_bycmdq(0);
	return ret;
}


// ESD CHECK FUNCTION
// return 1: esd check fail
// return 0: esd check pass
int primary_display_esd_check(void)
{
	int ret = 0;

	dprec_logger_start(DPREC_LOGGER_ESD_CHECK, 0, 0);
	MMProfileLogEx(ddp_mmp_get_events()->esd_check_t, MMProfileFlagStart, 0, 0);
	DISPCHECK("[ESD]ESD check begin\n");

	_primary_path_esd_check_lock();		//add by lizhiye
	
    _primary_path_lock(__func__);
	if(pgc->state == DISP_SLEEPED)
	{
		MMProfileLogEx(ddp_mmp_get_events()->esd_check_t, MMProfileFlagPulse, 1, 0);
		DISPCHECK("[ESD]primary display path is sleeped?? -- skip esd check\n");
        _primary_path_unlock(__func__);
		_primary_path_esd_check_unlock();	//add by lizhiye
		goto done;
	}
	primary_display_idlemgr_kick(__func__, 0);
	_primary_path_unlock(__func__);

	/// Esd Check : EXT TE
	if(pgc->plcm->params->dsi.customization_esd_check_enable==0)
	{
		MMProfileLogEx(ddp_mmp_get_events()->esd_extte, MMProfileFlagStart, 0, 0);
		if(primary_display_is_video_mode())
		{
			primary_display_switch_esd_mode(1);
			if(_need_register_eint())
			{
				MMProfileLogEx(ddp_mmp_get_events()->esd_extte, MMProfileFlagPulse, 1, 1);

				if(wait_event_interruptible_timeout(esd_ext_te_wq,atomic_read(&esd_ext_te_event),HZ/2)>0)
				{
					ret = 0; // esd check pass
					DISPCHECK("[ESD] te mode esd check pass\n");	//add by lizhiye
				}
				else
				{
					ret = 1; // esd check fail
					DISPCHECK("[ESD] te mode esd check fail\n");	//add by lizhiye
				}
				atomic_set(&esd_ext_te_event, 0);
			}
			primary_display_switch_esd_mode(0);
		}
		else
		{
			MMProfileLogEx(ddp_mmp_get_events()->esd_extte, MMProfileFlagPulse, 0, 1);
			if(dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC, HZ/2)>0)
			{
				ret = 0; // esd check pass
			}
			else
			{
				ret = 1; // esd check fail
			}
		}
		MMProfileLogEx(ddp_mmp_get_events()->esd_extte, MMProfileFlagEnd, 0, ret);
		//_primary_path_unlock(__func__);
		_primary_path_esd_check_unlock();		//add by lizhiye
		goto done;
	}

	/// Esd Check : Read from lcm
	MMProfileLogEx(ddp_mmp_get_events()->esd_rdlcm, MMProfileFlagStart, 0, primary_display_cmdq_enabled());
	if(primary_display_cmdq_enabled())
	{
		MMProfileLogEx(ddp_mmp_get_events()->esd_rdlcm, MMProfileFlagPulse, 0, 1);
		// 0.create esd check cmdq
		cmdqRecCreate(CMDQ_SCENARIO_DISP_ESD_CHECK,&(pgc->cmdq_handle_config_esd));	
		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_config_esd,CMDQ_ESD_ALLC_SLOT, 0);
		MMProfileLogEx(ddp_mmp_get_events()->esd_rdlcm, MMProfileFlagPulse, 0, 2);
		DISPCHECK("[ESD]ESD config thread=%p\n",pgc->cmdq_handle_config_esd);

		// 1.use cmdq to read from lcm
		if(primary_display_is_video_mode())
		{
			ret = _esd_check_config_handle_vdo();
		}
		else
		{
			ret = _esd_check_config_handle_cmd();
		}
		MMProfileLogEx(ddp_mmp_get_events()->esd_rdlcm, MMProfileFlagPulse, primary_display_is_video_mode(), 3);
		if(ret == 1) // cmdq fail
		{	
			if(_need_wait_esd_eof())
			{
				// Need set esd check eof synctoken to let trigger loop go.
				cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_ESD_EOF);
			}
			// do dsi reset
			dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_config_esd,CMDQ_DSI_RESET, 0);
			goto destory_cmdq;
		}

		DISPCHECK("[ESD]ESD config thread done~\n");

		// 2.check data(*cpu check now)
		ret = dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_config_esd,CMDQ_ESD_CHECK_CMP, 0);
		MMProfileLogEx(ddp_mmp_get_events()->esd_rdlcm, MMProfileFlagPulse, 0, 4);
		if(ret)
		{
			ret = 1; // esd check fail
		}

destory_cmdq:
		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_config_esd,CMDQ_ESD_FREE_SLOT, 0);
		// 3.destory esd config thread
		cmdqRecDestroy(pgc->cmdq_handle_config_esd);
		pgc->cmdq_handle_config_esd = NULL;
		//_primary_path_unlock(__func__);
	}
	else
	{
		/// 0: lock path
		/// 1: stop path
		/// 2: do esd check (!!!)
		/// 3: start path
		/// 4: unlock path

		MMProfileLogEx(ddp_mmp_get_events()->esd_rdlcm, MMProfileFlagPulse, 0, 1);
		_primary_path_lock(__func__);

		/// 1: stop path
		DISPCHECK("[ESD]display cmdq trigger loop stop[begin]\n");
		_cmdq_stop_trigger_loop();
		DISPCHECK("[ESD]display cmdq trigger loop stop[end]\n");

		if(dpmgr_path_is_busy(pgc->dpmgr_handle))
		{
			DISPCHECK("[ESD]primary display path is busy\n");
			ret = dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ*1);
			DISPCHECK("[ESD]wait frame done ret:%d\n", ret);
		}

		DISPCHECK("[ESD]stop dpmgr path[begin]\n");
		dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
		DISPCHECK("[ESD]stop dpmgr path[end]\n");

		if(dpmgr_path_is_busy(pgc->dpmgr_handle))
		{
			DISPCHECK("[ESD]primary display path is busy after stop\n");
			dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ*1);
			DISPCHECK("[ESD]wait frame done ret:%d\n", ret);
		}

		DISPCHECK("[ESD]reset display path[begin]\n");
		dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
		DISPCHECK("[ESD]reset display path[end]\n");

		/// 2: do esd check (!!!)
		MMProfileLogEx(ddp_mmp_get_events()->esd_rdlcm, MMProfileFlagPulse, 0, 2);

		if(primary_display_is_video_mode())
		{
			//ret = 0;
			ret = disp_lcm_esd_check(pgc->plcm);
		}
		else
		{
			ret = disp_lcm_esd_check(pgc->plcm);
		}

		/// 3: start path
		MMProfileLogEx(ddp_mmp_get_events()->esd_rdlcm, MMProfileFlagPulse, primary_display_is_video_mode(), 3);

		DISPCHECK("[ESD]start dpmgr path[begin]\n");
		dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);
		DISPCHECK("[ESD]start dpmgr path[end]\n");

		DISPCHECK("[ESD]start cmdq trigger loop[begin]\n");
		_cmdq_start_trigger_loop();
		DISPCHECK("[ESD]start cmdq trigger loop[end]\n");

		_primary_path_unlock(__func__);
	}
	MMProfileLogEx(ddp_mmp_get_events()->esd_rdlcm, MMProfileFlagEnd, 0, ret);

	_primary_path_esd_check_unlock();		//add by lizhiye

done:
	DISPCHECK("[ESD]ESD check end\n");
	MMProfileLogEx(ddp_mmp_get_events()->esd_check_t, MMProfileFlagEnd, 0, ret);	
	dprec_logger_done(DPREC_LOGGER_ESD_CHECK, 0, 0);
	return ret;

}

// For Vdo Mode EXT TE Check
static irqreturn_t _esd_check_ext_te_irq_handler(int irq, void *data)
{
	MMProfileLogEx(ddp_mmp_get_events()->esd_vdo_eint, MMProfileFlagPulse, 0, 0);
	atomic_set(&esd_ext_te_event, 1);
	wake_up_interruptible(&esd_ext_te_wq);
	return IRQ_HANDLED;	
}

static int primary_display_esd_check_worker_kthread(void *data)
{
	struct sched_param param = { .sched_priority = RTPM_PRIO_FB_THREAD };
	sched_setscheduler(current, SCHED_RR, &param);
	long int ttt = 0;
	int ret = 0;
	int i = 0;
	int esd_try_cnt = 5;  // 20;

	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);

	while(1)
	{
#if 0
		dpmgr_wait_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);
		ret = dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ);
		if(ret <= 0)
		{
			DISPERR("wait frame done timeout, reset whole path now\n");
			primary_display_diagnose();
			dprec_logger_trigger(DPREC_LOGGER_ESD_RECOVERY);
			dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
		}
#else
		msleep(2000); // esd check every 2s
		ret = wait_event_interruptible(esd_check_task_wq,atomic_read(&esd_check_task_wakeup));
		if(ret < 0)
		{
			DISPCHECK("[ESD]esd check thread waked up accidently\n");
			continue;
		}
#ifdef DISP_SWITCH_DST_MODE		
		_primary_path_switch_dst_lock();
#endif
#if 0
		{
			// let's do a mutex holder check here
			unsigned long long period = 0;
			period = dprec_logger_get_current_hold_period(DPREC_LOGGER_PRIMARY_MUTEX);
			if(period > 2000*1000*1000)
			{
				DISPERR("primary display mutex is hold by %s for %dns\n", pgc->mutex_locker, period);				
			}
		}
#endif	
		ret = primary_display_esd_check();
		if(ret == 1)
		{
			DISPCHECK("[ESD]esd check fail, will do esd recovery\n");
			i = esd_try_cnt;
			while(i--)
			{
				DISPCHECK("[ESD]esd recovery try:%d\n", i);
				primary_display_esd_recovery();
				ret = primary_display_esd_check();
				if(ret == 0)
				{
					DISPCHECK("[ESD]esd recovery success\n");
					break;
				}
				else
				{
					DISPCHECK("[ESD]after esd recovery, esd check still fail\n");
					if(i==0)
					{
						DISPCHECK("[ESD]after esd recovery %d times, esd check still fail, disable esd check\n",esd_try_cnt);
						primary_display_esd_check_enable(0);
					}
				}
			}
		}
#ifdef DISP_SWITCH_DST_MODE
		_primary_path_switch_dst_unlock();
#endif
#endif

		if (kthread_should_stop())
			break;
	}
	return 0;
}

//// begin add by lizhiye
#if 1	//defined(NT35596_FHD_DSI_VDO_TIANMA_CX865) || defined(NT35532_FHD_DSI_VDO_BOE_CX865)
unsigned int esd_backlight_level = 0;
int primary_display_esd_setbacklight(unsigned int level)
{
	DISPFUNC();
	int ret = 0;
	
	MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagStart, 0, 0);

	if(primary_display_cmdq_enabled())	
	{
		printk("primary_display_esd_setbacklight, line=%d\n", __LINE__);
		if(primary_display_is_video_mode())
		{
			MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 0, 7);
			disp_lcm_set_backlight(pgc->plcm,NULL,level);
		}
	}
	MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagEnd, 0, 0);
	
	return ret;
}
#endif
//end add by lizhiye

// ESD RECOVERY
int primary_display_esd_recovery(void)
{
	DISP_STATUS ret = DISP_STATUS_OK;

	DISPFUNC();
	dprec_logger_start(DPREC_LOGGER_ESD_RECOVERY, 0, 0);
	MMProfileLogEx(ddp_mmp_get_events()->esd_recovery_t, MMProfileFlagStart, 0, 0);
	DISPCHECK("[ESD]ESD recovery begin\n");
	_primary_path_lock(__func__);
	MMProfileLogEx(ddp_mmp_get_events()->esd_recovery_t, MMProfileFlagPulse, primary_display_is_video_mode(), 1);

	LCM_PARAMS *lcm_param = NULL;

	lcm_param = disp_lcm_get_params(pgc->plcm);
	if(pgc->state == DISP_SLEEPED)
	{
		DISPCHECK("[ESD]esd recovery but primary display path is sleeped??\n");
		goto done;
	}
	MMProfileLogEx(ddp_mmp_get_events()->esd_recovery_t, MMProfileFlagPulse, 0, 2);

	//DISPCHECK("[ESD]display cmdq trigger loop stop[begin]\n");
	//_cmdq_stop_trigger_loop();
	//DISPCHECK("[ESD]display cmdq trigger loop stop[end]\n");

	if(dpmgr_path_is_busy(pgc->dpmgr_handle))
	{
		DISPCHECK("[ESD]primary display path is busy\n");
		ret = dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ*1);
		DISPCHECK("[ESD]wait frame done ret:%d\n", ret);
	}
	MMProfileLogEx(ddp_mmp_get_events()->esd_recovery_t, MMProfileFlagPulse, 0, 3);

	DISPCHECK("[ESD]stop dpmgr path[begin]\n");
	dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("[ESD]stop dpmgr path[end]\n");
	MMProfileLogEx(ddp_mmp_get_events()->esd_recovery_t, MMProfileFlagPulse, 0, 4);

	if(dpmgr_path_is_busy(pgc->dpmgr_handle))
	{
		DISPCHECK("[ESD]primary display path is busy after stop\n");
		dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ*1);
		DISPCHECK("[ESD]wait frame done ret:%d\n", ret);
	}
	MMProfileLogEx(ddp_mmp_get_events()->esd_recovery_t, MMProfileFlagPulse, 0, 5);

	DISPCHECK("[ESD]display cmdq trigger loop stop[begin]\n");
    _start_dummy_trigger_loop();
	_cmdq_stop_trigger_loop();
	DISPCHECK("[ESD]display cmdq trigger loop stop[end]\n");

	MMProfileLogEx(ddp_mmp_get_events()->esd_recovery_t, MMProfileFlagPulse, 0, 6);

	DISPCHECK("[ESD]reset display path[begin]\n");
	dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("[ESD]reset display path[end]\n");

	DISPCHECK("[POWER]lcm suspend[begin]\n");
	disp_lcm_suspend(pgc->plcm);
	DISPCHECK("[POWER]lcm suspend[end]\n");
	DISPCHECK("[ESD]lcm force init[begin]\n");
	disp_lcm_init(pgc->plcm, 1);
	DISPCHECK("[ESD]lcm force init[end]\n");
	MMProfileLogEx(ddp_mmp_get_events()->esd_recovery_t, MMProfileFlagPulse, 0, 8);

	DISPCHECK("[ESD]start dpmgr path[begin]\n");
	dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("[ESD]start dpmgr path[end]\n");
	if(dpmgr_path_is_busy(pgc->dpmgr_handle))
	{
		DISPERR("[ESD]Fatal error, we didn't trigger display path but it's already busy\n");
		ret = -1;
		//goto done;
	}

	MMProfileLogEx(ddp_mmp_get_events()->esd_recovery_t, MMProfileFlagPulse, 0, 9);
	DISPCHECK("[ESD]start cmdq trigger loop[begin]\n");
	_cmdq_start_trigger_loop();
    _stop_dummy_trigger_loop();
	DISPCHECK("[ESD]start cmdq trigger loop[end]\n");
	MMProfileLogEx(ddp_mmp_get_events()->esd_recovery_t, MMProfileFlagPulse, 0, 10);	
	if(primary_display_is_video_mode())
	{
		// for video mode, we need to force trigger here
		// for cmd mode, just set DPREC_EVENT_CMDQ_SET_EVENT_ALLOW when trigger loop start
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, CMDQ_DISABLE);
	}
	MMProfileLogEx(ddp_mmp_get_events()->esd_recovery_t, MMProfileFlagPulse, 0, 11);
	//DISPCHECK("[ESD]start cmdq trigger loop[begin]\n");
	//_cmdq_start_trigger_loop();
	//DISPCHECK("[ESD]start cmdq trigger loop[end]\n");

done:
	_primary_path_unlock(__func__);
	DISPCHECK("[ESD]ESD recovery end\n");
	MMProfileLogEx(ddp_mmp_get_events()->esd_recovery_t, MMProfileFlagEnd, 0, 0);
	dprec_logger_done(DPREC_LOGGER_ESD_RECOVERY, 0, 0);

//begin add by lizhiye
#if 1	//defined(NT35596_FHD_DSI_VDO_TIANMA_CX865) || defined(NT35532_FHD_DSI_VDO_BOE_CX865)
	DISPCHECK("[ESD]lcm force backlight set to %d, line=%d\n", esd_backlight_level, __LINE__);
	mdelay(10);
	if(esd_backlight_level == 0)
	{
		esd_backlight_level = 102;
	}
	primary_display_esd_setbacklight(esd_backlight_level - 1);	//moify by lizhiye, 20151217
#endif
//end add by lizhiye

	return ret;
}

void primary_display_esd_check_enable(int enable)
{
	if(_need_do_esd_check())
	{
		if(_need_register_eint() && eint_flag != 2)
		{
			DISPCHECK("[ESD]Please check DCT setting about GPIO107/EINT107 \n");
			return;
		}

		if(enable)
		{
			DISPCHECK("[ESD]esd check thread wakeup\n");
			atomic_set(&esd_check_task_wakeup, 1);
			wake_up_interruptible(&esd_check_task_wq);
		}
		else
		{
			DISPCHECK("[ESD]esd check thread stop\n");
			atomic_set(&esd_check_task_wakeup, 0); 
		}
	}
}

/******************************************************************************/
/* ESD CHECK / RECOVERY ---- End                                              */
/******************************************************************************/
#define EEEEEEEEEEEEEEEEEEEEEEEEEE

static struct task_struct *primary_path_aal_task = NULL;

static int _disp_primary_path_check_trigger(void *data)
{
	int ret = 0;
	if (disp_helper_get_option(DISP_HELPER_OPTION_USE_CMDQ)) 
	{
		cmdqRecHandle handle = NULL;
		ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &handle);

		dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_TRIGGER);
		while (1) {
			dpmgr_wait_event(pgc->dpmgr_handle, DISP_PATH_EVENT_TRIGGER);
			_primary_path_lock(__func__);

			if (pgc->state != DISP_SLEEPED) {
				MMProfileLogEx(ddp_mmp_get_events()->primary_display_aalod_trigger,
						MMProfileFlagPulse, 0, 0);
				primary_display_idlemgr_kick(__func__, 0);
				cmdqRecReset(handle);
				_cmdq_insert_wait_frame_done_token_mira(handle);
				_cmdq_set_config_handle_dirty_mira(handle);
				_cmdq_flush_config_handle_mira(handle, 0);
			}

			_primary_path_unlock(__func__);

			if (kthread_should_stop())
				break;
		}

		cmdqRecDestroy(handle);		
	}
	else
	{
		dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_TRIGGER);
		while (1) 
		{
			dpmgr_wait_event(pgc->dpmgr_handle, DISP_PATH_EVENT_TRIGGER);
			DISPCHECK("Force Trigger Display Path\n");
			primary_display_trigger(1, NULL, 0);

			if (kthread_should_stop())
				break;
		}
	}

	return 0;
}

#define OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO

//need remove
unsigned int cmdqDdpClockOn(uint64_t engineFlag)
{
	//DISP_LOG_I("cmdqDdpClockOff\n");
	return 0;
}

unsigned int cmdqDdpClockOff(uint64_t engineFlag)
{
	//DISP_LOG_I("cmdqDdpClockOff\n");
	return 0;
}

unsigned int cmdqDdpDumpInfo(uint64_t engineFlag,
		char     *pOutBuf,
		unsigned int bufSize)
{
	DISPERR("cmdq timeout:%llu\n", engineFlag);
	primary_display_diagnose();
	//DISP_LOG_I("cmdqDdpDumpInfo\n");
	return 0;
}

unsigned int cmdqDdpResetEng(uint64_t engineFlag)
{
	//DISP_LOG_I("cmdqDdpResetEng\n");
	return 0;
}

// TODO: these 2 functions should be splited into another file
static void _RDMA0_INTERNAL_IRQ_Handler(DISP_MODULE_ENUM module, unsigned int param)
{
	if (param & 0x2) 
	{
		/* RDMA Start */
		spm_sodi_mempll_pwr_mode(1);
		MMProfileLogEx(ddp_mmp_get_events()->sodi_disable, MMProfileFlagPulse, 0, 0);
	} 
	else if (param & 0x4) 
	{
		/* RDMA Done */
		spm_sodi_mempll_pwr_mode(0);
		MMProfileLogEx(ddp_mmp_get_events()->sodi_enable, MMProfileFlagPulse, 0, 0);
	}
}

void primary_display_sodi_rule_init(void)
{
	if(disp_od_is_enabled() && (primary_display_mode == DECOUPLE_MODE) && primary_display_is_video_mode())
	{
		spm_enable_sodi(0);
		return;
	}

	if(primary_display_is_video_mode())
	{
		spm_enable_sodi(0);
	}
	else
	{
		spm_enable_sodi(1);
	}
	return;

}

extern int dfo_query(const char *s, unsigned long *v);

int primary_display_change_lcm_resolution(unsigned int width, unsigned int height)
{
	if(pgc->plcm)
	{
		DISPCHECK("LCM Resolution will be changed, original: %dx%d, now: %dx%d\n", pgc->plcm->params->width, pgc->plcm->params->height, width, height);
		// align with 4 is the minimal check, to ensure we can boot up into kernel, and could modify dfo setting again using meta tool
		// otherwise we will have a panic in lk(root cause unknown).
		if(width >pgc->plcm->params->width || height > pgc->plcm->params->height || width == 0 || height == 0 || width %4 || height %4)
		{
			DISPERR("Invalid resolution: %dx%d\n", width, height);
			return -1;
		}

		if(primary_display_is_video_mode())
		{
			DISPERR("Warning!!!Video Mode can't support multiple resolution!\n");
			return -1;
		}

		pgc->plcm->params->width = width;
		pgc->plcm->params->height = height;

		return 0;
	}
	else
	{
		return -1;
	}
}
static struct task_struct *fence_release_worker_task = NULL;

extern unsigned int ddp_ovl_get_cur_addr(bool rdma_mode, int layerid );

static void _wdma_fence_release_callback(uint32_t userdata)
{
	int fence_idx, subtractor, layer;
	layer = disp_sync_get_output_timeline_id();

	cmdqBackupReadSlot(pgc->cur_config_fence, layer, &fence_idx);
	mtkfb_release_fence(primary_session_id, layer, fence_idx);
	MMProfileLogEx(ddp_mmp_get_events()->primary_wdma_fence_release, MMProfileFlagPulse, layer, fence_idx);

	return;
}

static void _Interface_fence_release_callback(uint32_t userdata)
{
	int  layer = disp_sync_get_output_interface_timeline_id();

	if(userdata > 0){
		mtkfb_release_fence(primary_session_id, layer, userdata);
		MMProfileLogEx(ddp_mmp_get_events()->primary_wdma_fence_release, MMProfileFlagPulse, layer, userdata);
	}

	return;
}

static void _ovl_fence_release_callback(uint32_t userdata)
{
	int i = 0;
	unsigned int addr = 0;
	int ret = 0;
	MMProfileLogEx(ddp_mmp_get_events()->session_release, MMProfileFlagStart, 1, userdata);

	/*check last ovl status: should be idle when config*/
	if(primary_display_is_video_mode() && !primary_display_is_decouple_mode()) {
		unsigned int status;
		cmdqBackupReadSlot(pgc->ovl_status_info, 0, &status);
		if((status & 0x1) != 0) 
		{
			/* ovl is not idle !! */
			DISPERR("disp ovl status error!! stat=0x%x\n", status);
			//disp_aee_print("ovl_stat 0x%x\n", status);
			MMProfileLogEx(ddp_mmp_get_events()->primary_error, MMProfileFlagPulse, status, 0);
		}
	}

	for(i=0;i<PRIMARY_DISPLAY_SESSION_LAYER_COUNT;i++)
	{
		int fence_idx  =0;
		int subtractor =0;
		if(i==primary_display_get_option("ASSERT_LAYER") && is_DAL_Enabled()) {
			mtkfb_release_layer_fence(primary_session_id, i);
		} else {
			cmdqBackupReadSlot(pgc->cur_config_fence, i, &fence_idx);
			cmdqBackupReadSlot(pgc->subtractor_when_free, i, &subtractor);
			mtkfb_release_fence(primary_session_id, i, fence_idx-subtractor);
		}
		MMProfileLogEx(ddp_mmp_get_events()->primary_ovl_fence_release, MMProfileFlagPulse, i, fence_idx-subtractor);
	}

	addr = ddp_ovl_get_cur_addr(!_should_config_ovl_input(), 0);
	if(( primary_display_is_decouple_mode() == 0))
		update_frm_seq_info(addr, 0, 2, FRM_START);

	/* async callback,need to check if it is still decouple*/
#ifdef UPDATE_RDMA_CONFIG_USING_CMDQ_CALLBACK
	_decouple_mirror_update_rdma_config();
#endif
	MMProfileLogEx(ddp_mmp_get_events()->session_release, MMProfileFlagEnd, 0, 0);

	return;
}


static int _decouple_mirror_update_rdma_config()
{
	int interface_fence = 0;
	int layer = 0;
	int ret = 0;
	_primary_path_lock(__func__);
	if(primary_display_is_decouple_mode() && pgc->state != DISP_SLEEPED) {
		static cmdqRecHandle cmdq_handle = NULL;
		if(cmdq_handle == NULL)
			ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP,&cmdq_handle);

		if (ret == 0){
			if(primary_display_is_mirror_mode()) {
				RDMA_CONFIG_STRUCT tmpConfig = decouple_rdma_config;
				cmdqRecReset(cmdq_handle);
				_cmdq_insert_wait_frame_done_token_mira(cmdq_handle);
				cmdqBackupReadSlot(pgc->rdma_buff_info, 0, &(tmpConfig.address));
				cmdqBackupReadSlot(pgc->rdma_buff_info, 1, &(tmpConfig.pitch));
				_config_rdma_input_data(&tmpConfig, pgc->dpmgr_handle, cmdq_handle);
				layer = disp_sync_get_output_timeline_id();
				cmdqBackupReadSlot(pgc->cur_config_fence, layer, &interface_fence);
				_cmdq_set_config_handle_dirty_mira(cmdq_handle);
				cmdqRecFlushAsyncCallback(cmdq_handle, _Interface_fence_release_callback, 
						interface_fence > 1 ? interface_fence - 1 : 0);
				// ddp_mmp_rdma_layer(&decouple_rdma_config, 0,  20, 20);
				MMProfileLogEx(ddp_mmp_get_events()->primary_rdma_config, MMProfileFlagPulse, interface_fence, tmpConfig.address);
				// cmdqRecDestroy(cmdq_handle);
			}else{
				RDMA_CONFIG_STRUCT tmpConfig = decouple_rdma_config;
				cmdqRecReset(cmdq_handle);
				_cmdq_insert_wait_frame_done_token_mira(cmdq_handle);
				cmdqBackupReadSlot(pgc->rdma_buff_info, 0, &(tmpConfig.address));
				_config_rdma_input_data(&tmpConfig,pgc->dpmgr_handle,cmdq_handle);
				_cmdq_set_config_handle_dirty_mira(cmdq_handle);
				cmdqRecFlushAsyncCallback(cmdq_handle, NULL, 0);
				MMProfileLogEx(ddp_mmp_get_events()->primary_rdma_config, MMProfileFlagPulse, 0, tmpConfig.address);
				//cmdqRecDestroy(cmdq_handle);
			}
		}else{
			DISPERR("fail to create cmdq\n");
		}
	}
	_primary_path_unlock(__func__);
	return 0;
}

//#define UPDATE_RDMA_CONFIG_USING_CMDQ_CALLBACK
static int _olv_wdma_fence_release_callback(uint32_t userdata)
{
	_ovl_fence_release_callback(userdata);
	_wdma_fence_release_callback(userdata);

#ifdef UPDATE_RDMA_CONFIG_USING_CMDQ_CALLBACK
	_decouple_mirror_update_rdma_config();
#endif

	return 0;
}

static struct task_struct *decouple_update_rdma_config_thread = NULL;
static int decouple_mirror_update_rdma_config_thread(void* data);

#ifdef UPDATE_RDMA_CONFIG_USING_CMDQ_CALLBACK
static int decouple_mirror_update_rdma_config_thread(void* data)
{
	return 0;
}
#else

atomic_t decouple_update_rdma_event = ATOMIC_INIT(0);
DECLARE_WAIT_QUEUE_HEAD(decouple_update_rdma_wq);

void decouple_mirror_wdma0_irq_callback(DISP_MODULE_ENUM module, unsigned int reg_value)
{
	ASSERT(module == DISP_MODULE_WDMA0);
	if(reg_value & 0x1) {
		/* wdma frame done */
		atomic_set(&decouple_update_rdma_event, 1);
		wake_up_interruptible(&decouple_update_rdma_wq);
	}
	return;
}

static int decouple_mirror_update_rdma_config_thread(void* data)
{
	struct sched_param param = { .sched_priority = RTPM_PRIO_SCRN_UPDATE };
	sched_setscheduler(current, SCHED_RR, &param);

	disp_register_module_irq_callback(DISP_MODULE_WDMA0, decouple_mirror_wdma0_irq_callback);

	while(1)
	{   
		wait_event_interruptible(decouple_update_rdma_wq, atomic_read(&decouple_update_rdma_event));
		atomic_set(&decouple_update_rdma_event, 0);
		_decouple_mirror_update_rdma_config();
		if(kthread_should_stop())
			break;
	}	

	return 0;
}
#endif

static int primary_display_remove_output(void *callback, unsigned int userdata)
{
	int ret = 0;
	static cmdqRecHandle cmdq_handle = NULL;
	static cmdqRecHandle cmdq_wait_handle = NULL;

	if(pgc->need_trigger_ovl1to2 == 0){
		DISPPR_ERROR("There is no output config when directlink mirror!!\n");
		return 0;
	}
	/*create config thread*/
	if (cmdq_handle == NULL)
		ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP,&cmdq_handle);

	if (ret == 0){
		/* capture thread wait wdma sof*/
		if(cmdq_wait_handle == NULL)
			ret = cmdqRecCreate(CMDQ_SCENARIO_DISP_SCREEN_CAPTURE, &cmdq_wait_handle);

		if(ret == 0 ){
			cmdqRecReset(cmdq_wait_handle);
			cmdqRecWait(cmdq_wait_handle, CMDQ_EVENT_DISP_WDMA0_SOF);
			cmdqRecFlush(cmdq_wait_handle);
			//cmdqRecDestroy(cmdq_wait_handle);
		}else{
			DISPERR("fail to create  wait handle\n");
		}
		cmdqRecReset(cmdq_handle);
		_cmdq_insert_wait_frame_done_token_mira(cmdq_handle);

		//update output fence
		cmdqRecBackupUpdateSlot(cmdq_handle, pgc->cur_config_fence, 
				disp_sync_get_output_timeline_id(), mem_config.buff_idx);

		dpmgr_path_remove_memout(pgc->dpmgr_handle, cmdq_handle);

		cmdqRecClearEventToken(cmdq_handle,CMDQ_EVENT_DISP_WDMA0_SOF);
		_cmdq_set_config_handle_dirty_mira(cmdq_handle);
		cmdqRecFlushAsyncCallback(cmdq_handle, callback, 0);
		pgc->need_trigger_ovl1to2 = 0;
		//cmdqRecDestroy(cmdq_handle);
	}else{
		ret = -1;
		DISPERR("fail to remove memout out\n");
	}
	return ret;
}

static void primary_display_frame_update_irq_callback(DISP_MODULE_ENUM module, unsigned int param)
{
	///if(pgc->session_mode == DISP_SESSION_DIRECT_LINK_MIRROR_MODE)
	///    return;

	if(module == DISP_MODULE_RDMA0)
	{
		if (param & 0x2) //  rdma0 frame start  0x20
		{
			if(pgc->session_id > 0 && primary_display_is_decouple_mode())
				update_frm_seq_info(ddp_ovl_get_cur_addr(1, 0),0, 1, FRM_START);
		}

		if (param & 0x4) //  rdma0 frame done 
		{
			atomic_set(&primary_display_frame_update_event, 1);
			wake_up_interruptible(&primary_display_frame_update_wq);        
		}
	}

	if((module == DISP_MODULE_OVL0 )&&( primary_display_is_decouple_mode() == 0))
	{
		if (param & 0x2) //  ov0 frame done 
		{
			atomic_set(&primary_display_frame_update_event, 1);
			wake_up_interruptible(&primary_display_frame_update_wq);        
		}
	}

}

static int primary_display_frame_update_kthread(void *data)
{
	struct sched_param param = { .sched_priority = RTPM_PRIO_SCRN_UPDATE };
	sched_setscheduler(current, SCHED_RR, &param);
	int layid = 0;

	unsigned int session_id = 0;
	unsigned long frame_update_addr = 0;
	unsigned long frm_seq = 0;

	for (;;)
	{
		wait_event_interruptible(primary_display_frame_update_wq, atomic_read(&primary_display_frame_update_event));
		atomic_set(&primary_display_frame_update_event, 0);

		if(pgc->session_id > 0)
			update_frm_seq_info(0, 0, 0, FRM_END);

		if (kthread_should_stop())
		{
			break;
		}
	}

	return 0;
}


static int _fence_release_worker_thread(void *data)
{
	int ret = 0;
	int i = 0;
	unsigned int addr = 0;
	int fence_idx = -1;
	unsigned int session_id = MAKE_DISP_SESSION(DISP_SESSION_PRIMARY,0);
	struct sched_param param = { .sched_priority = RTPM_PRIO_SCRN_UPDATE };
	sched_setscheduler(current, SCHED_RR, &param);

	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);
	while(1)
	{   
		dpmgr_wait_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);
		if(!primary_display_is_video_mode())
		{
			DISPPR_FENCE("P0/Frame Start\n");
		}

		for(i=0;i<PRIMARY_DISPLAY_SESSION_LAYER_COUNT;i++)
		{
			addr = ddp_ovl_get_cur_addr(!_should_config_ovl_input(), i);
			if(is_dim_layer(addr))
			{
				addr = 0;
			}
			if(i == primary_display_get_option("ASSERT_LAYER") && is_DAL_Enabled())
			{
				mtkfb_release_layer_fence(session_id, 3);
			}
			else
			{
				fence_idx = disp_sync_find_fence_idx_by_addr(session_id, i, addr);
				if (fence_idx < 0) {
					if(fence_idx == -1)
					{
						//DISPPR_ERROR("find fence idx for layer %d,addr 0x%08x fail, unregistered addr%d\n", i, addr, fence_idx);
					}
					else if(fence_idx == -2)
					{

					}
					else
					{
						DISPPR_ERROR("find fence idx for layer %d,addr 0x%08x fail,reason unknown%d\n", i, addr, fence_idx);
					}
				}
				else
				{
					mtkfb_release_fence(session_id, i, fence_idx);
				}
			}
		}

		addr = ddp_ovl_get_cur_addr(!_should_config_ovl_input(), 0);
		if(( primary_display_is_decouple_mode() == 0))
			update_frm_seq_info(addr, 0, 2, FRM_START);

		MMProfileLogEx(ddp_mmp_get_events()->session_release, MMProfileFlagEnd, 0, 0);

		if(kthread_should_stop())
			break;
	}	

	return 0;
}

static struct task_struct *present_fence_release_worker_task = NULL;
extern char *disp_session_mode_spy(unsigned int session_id);

static int _present_fence_release_worker_thread(void *data)
{
	int ret = 0; 
	struct sched_param param = { .sched_priority = RTPM_PRIO_FB_THREAD };
	sched_setscheduler(current, SCHED_RR, &param);

	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);

	while(1)
	{
		extern disp_sync_info * _get_sync_info(unsigned int session_id, unsigned int timeline_id);
		int fence_increment=0;
		int timeline_id;
		wait_event_interruptible(primary_display_present_fence_wq, atomic_read(&primary_display_present_fence_update_event));
		atomic_set(&primary_display_present_fence_update_event, 0);
		if(!islcmconnected && !primary_display_is_video_mode())
		{
			DISPCHECK("LCM Not Connected && CMD Mode\n");
			msleep(16);
		}
		else
		{
			dpmgr_wait_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC); 
		}

		timeline_id = disp_sync_get_present_timeline_id();

		disp_sync_info *layer_info = _get_sync_info(primary_session_id, timeline_id);
		if(layer_info == NULL)
		{
			MMProfileLogEx(ddp_mmp_get_events()->present_fence_release, MMProfileFlagPulse, -1, 0x5a5a5a5a);
			//DISPERR("_get_sync_info fail in present_fence_release thread \n");
			continue;
		}

		_primary_path_lock(__func__);
		fence_increment = gPresentFenceIndex-layer_info->timeline->value;
		if(fence_increment>0)
		{
			timeline_inc(layer_info->timeline, fence_increment);               
			DISPPR_FENCE("R+/%s%d/L%d/id%d\n", disp_session_mode_spy(primary_session_id), 
					DISP_SESSION_DEV(primary_session_id), timeline_id, gPresentFenceIndex);
		}  
		MMProfileLogEx(ddp_mmp_get_events()->present_fence_release, MMProfileFlagPulse, gPresentFenceIndex, fence_increment);
		_primary_path_unlock(__func__);
	}

	return 0;
}
#define xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx

int primary_display_set_frame_buffer_address(unsigned long va,unsigned long mva)
{

	DISPMSG("framebuffer va %lu, mva %lu\n", va, mva);
	pgc->framebuffer_va = va;
	pgc->framebuffer_mva = mva;
	/*
	   int frame_buffer_size = ALIGN_TO(DISP_GetScreenWidth(), MTK_FB_ALIGNMENT) * 
	   ALIGN_TO(DISP_GetScreenHeight(), MTK_FB_ALIGNMENT) * 4;
	   unsigned long dim_layer_va = va + 2*frame_buffer_size;
	   dim_layer_mva = mva + 2*frame_buffer_size;
	   memset(dim_layer_va, 0, frame_buffer_size);
	   */
	return 0;
}

unsigned long primary_display_get_frame_buffer_mva_address(void)
{
	return pgc->framebuffer_mva;
}

unsigned long primary_display_get_frame_buffer_va_address(void)
{
	return pgc->framebuffer_va;
}

int is_dim_layer(unsigned int long mva)
{
	if(mva == dim_layer_mva)
		return 1;
	return 0;
}

unsigned long get_dim_layer_mva_addr(void)
{
	if(dim_layer_mva == 0){
		int frame_buffer_size = ALIGN_TO(DISP_GetScreenWidth(), MTK_FB_ALIGNMENT) * 
			ALIGN_TO(DISP_GetScreenHeight(), MTK_FB_ALIGNMENT) * 4;
		unsigned long dim_layer_va =  pgc->framebuffer_va + 3*frame_buffer_size;
		memset(dim_layer_va, 0, frame_buffer_size);
		dim_layer_mva =  pgc->framebuffer_mva + 3*frame_buffer_size;
		DISPMSG("init dim layer mva %lu, size %d", dim_layer_mva, frame_buffer_size);
	}
	return dim_layer_mva;
}

static int init_cmdq_slots(cmdqBackupSlotHandle *pSlot, int count, int init_val)
{
	int i;
	cmdqBackupAllocateSlot(pSlot, count);

	for(i=0; i<count; i++)
	{
		cmdqBackupWriteSlot(*pSlot, i, init_val);
	}

	return 0;
}

static int update_primary_intferface_module()
{
	/*update interface module, it may be: dsi0/dsi1/dsi_dual*/
	DISP_MODULE_ENUM interface_module;
	interface_module = _get_dst_module_by_lcm(pgc->plcm);
	ddp_set_dst_module(DDP_SCENARIO_PRIMARY_DISP, interface_module);
	ddp_set_dst_module(DDP_SCENARIO_PRIMARY_RDMA0_COLOR0_DISP, interface_module);
	ddp_set_dst_module(DDP_SCENARIO_PRIMARY_RDMA0_DISP, interface_module);
	ddp_set_dst_module(DDP_SCENARIO_PRIMARY_BYPASS_RDMA, interface_module);

	ddp_set_dst_module(DDP_SCENARIO_PRIMARY_ALL, interface_module);

	return 0;
}

static struct task_struct *decouple_trigger_thread = NULL;
static int decouple_trigger_worker_thread(void *data);
atomic_t decouple_trigger_event = ATOMIC_INIT(0);
DECLARE_WAIT_QUEUE_HEAD(decouple_trigger_wq);

//add begin by lizhiye
#ifdef CONFIG_DEVINFO_LCM
static int devinfo_first=0;
extern void devinfo_lcm_reg(void);
#endif
//add end by lizhiye

int primary_display_init(char *lcm_name, unsigned int lcm_fps)
{
	DISPCHECK("primary_display_init begin\n");
	DISP_STATUS ret = DISP_STATUS_OK;
	DISP_MODULE_ENUM dst_module = 0;

	unsigned int lcm_fake_width = 0;
	unsigned int lcm_fake_height = 0;

	LCM_PARAMS *lcm_param = NULL;
	LCM_INTERFACE_ID lcm_id = LCM_INTERFACE_NOTDEFINED;
	dprec_init();
	disp_helper_option_init();
	dpmgr_init();
	init_cmdq_slots(&(pgc->cur_config_fence), DISP_SESSION_TIMELINE_COUNT, 0);
	init_cmdq_slots(&(pgc->subtractor_when_free), DISP_SESSION_TIMELINE_COUNT, 0);
	init_cmdq_slots(&(pgc->rdma_buff_info), 2, 0);
	init_cmdq_slots(&(pgc->ovl_status_info), 1, 0);
	mutex_init(&(pgc->capture_lock));
	mutex_init(&(pgc->lock));
	mutex_init(&(esd_mode_switch_lock));	//add by lizhiye
#ifdef DISP_SWITCH_DST_MODE
	mutex_init(&(pgc->switch_dst_lock));
#endif
	_primary_path_lock(__func__);

	pgc->plcm = disp_lcm_probe( lcm_name, LCM_INTERFACE_NOTDEFINED);

	if (pgc->plcm == NULL) {
		DISPCHECK("disp_lcm_probe returns null\n");
		ret = DISP_STATUS_ERROR;
		goto done;
	} else {
		DISPCHECK("disp_lcm_probe SUCCESS\n");
	}

#ifndef CONFIG_ARCH_MT6795
	if((0 == dfo_query("LCM_FAKE_WIDTH", &lcm_fake_width)) && (0 == dfo_query("LCM_FAKE_HEIGHT", &lcm_fake_height)))
	{
		printk("[DFO] LCM_FAKE_WIDTH=%d, LCM_FAKE_HEIGHT=%d\n", lcm_fake_width, lcm_fake_height);
		if(lcm_fake_width && lcm_fake_height)
		{
			if(DISP_STATUS_OK != primary_display_change_lcm_resolution(lcm_fake_width, lcm_fake_height))
			{
				DISPMSG("[DISP\DFO]WARNING!!! Change LCM Resolution FAILED!!!\n");
			}
		}
	}
#endif

	lcm_param = disp_lcm_get_params(pgc->plcm);

	if(lcm_param == NULL)
	{
		DISPERR("get lcm params FAILED\n");
		ret = DISP_STATUS_ERROR;
		goto done;
	}

//add begin by lihziye
#ifdef CONFIG_DEVINFO_LCM
	if(devinfo_first==0)
	{
		devinfo_first=1;
		devinfo_lcm_reg();
	}
#endif
//add end by lizhiye
		
	update_primary_intferface_module();

	ret = cmdqCoreRegisterCB(CMDQ_GROUP_DISP,	cmdqDdpClockOn,cmdqDdpDumpInfo,cmdqDdpResetEng,cmdqDdpClockOff);
	if(ret)
	{
		DISPERR("cmdqCoreRegisterCB failed, ret=%d \n", ret);
		ret = DISP_STATUS_ERROR;
		goto done;
	}					 

	ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP,&(pgc->cmdq_handle_config));
	if(ret)
	{
		DISPCHECK("cmdqRecCreate FAIL, ret=%d \n", ret);
		ret = DISP_STATUS_ERROR;
		goto done;
	}
	else
	{
		DISPCHECK("cmdqRecCreate SUCCESS, g_cmdq_handle=%p \n", pgc->cmdq_handle_config);
	}
	/*create ovl2mem path cmdq handle */
	ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_MEMOUT, &(pgc->cmdq_handle_ovl1to2_config));
	if(ret != 0){
		DISPERR("cmdqRecCreate FAIL, ret=%d \n", ret);
		ret = DISP_STATUS_ERROR;
		goto done;
	}

	if(primary_display_mode == DIRECT_LINK_MODE)
	{
		_build_path_direct_link();
		pgc->session_mode = DISP_SESSION_DIRECT_LINK_MODE;
		DISPCHECK("primary display is DIRECT LINK MODE\n");
	}
	else if(primary_display_mode == DECOUPLE_MODE)
	{
		_build_path_decouple();
		pgc->session_mode = DISP_SESSION_DECOUPLE_MODE;

		DISPCHECK("primary display is DECOUPLE MODE\n");
	}
	else if(primary_display_mode == SINGLE_LAYER_MODE)
	{
		_build_path_single_layer();

		DISPCHECK("primary display is SINGLE LAYER MODE\n");
	}
	else if(primary_display_mode == DEBUG_RDMA1_DSI0_MODE)
	{
		_build_path_debug_rdma1_dsi0();

		DISPCHECK("primary display is DEBUG RDMA1 DSI0 MODE\n");
	}
	else
	{
		DISPCHECK("primary display mode is WRONG\n");
	}

	if (disp_helper_get_option(DISP_HELPER_OPTION_USE_CMDQ)) {
		_cmdq_reset_config_handle();
		_cmdq_insert_wait_frame_done_token();
	}

	dpmgr_path_set_video_mode(pgc->dpmgr_handle, primary_display_is_video_mode());
	dpmgr_path_init(pgc->dpmgr_handle, disp_helper_get_option(DISP_HELPER_OPTION_USE_CMDQ));

	// use fake timer to generate vsync signal for cmd mode w/o LCM(originally using LCM TE Signal as VSYNC)
	// so we don't need to modify display driver's behavior.
	if(disp_helper_get_option(DISP_HELPER_OPTION_NO_LCM_FOR_LOW_POWER_MEASUREMENT))
	{
		// only for low power measurement
		DISPCHECK("WARNING!!!!!! FORCE NO LCM MODE!!!\n");
		islcmconnected = 0;

		// no need to change video mode vsync behavior
		if(!primary_display_is_video_mode())
		{
			_init_vsync_fake_monitor(lcm_fps);

			dpmgr_map_event_to_irq(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC, DDP_IRQ_UNKNOW);
		}
	}

	if (disp_helper_get_option(DISP_HELPER_OPTION_USE_CMDQ))
	{
		_cmdq_build_trigger_loop();
		_cmdq_start_trigger_loop();
	}

	disp_ddp_path_config *data_config = NULL;

	data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle);

	memcpy(&(data_config->dispif_config), lcm_param, sizeof(LCM_PARAMS));

	data_config->dst_w = disp_helper_get_option(DISP_HELPER_OPTION_FAKE_LCM_WIDTH);
	data_config->dst_h = lcm_param->height;

	if (lcm_param->type == LCM_TYPE_DSI) {
		if (lcm_param->dsi.data_format.format == LCM_DSI_FORMAT_RGB888)
			data_config->lcm_bpp = 24;
		else if(lcm_param->dsi.data_format.format == LCM_DSI_FORMAT_RGB565)
			data_config->lcm_bpp = 16;
		else if(lcm_param->dsi.data_format.format == LCM_DSI_FORMAT_RGB666)
			data_config->lcm_bpp = 18;
	} else if (lcm_param->type == LCM_TYPE_DPI) {
		if (lcm_param->dpi.format == LCM_DPI_FORMAT_RGB888)
			data_config->lcm_bpp = 24;
		else if( lcm_param->dpi.format == LCM_DPI_FORMAT_RGB565)
			data_config->lcm_bpp = 16;
		if( lcm_param->dpi.format == LCM_DPI_FORMAT_RGB666)
			data_config->lcm_bpp = 18;
	}

	data_config->fps = lcm_fps;
	data_config->dst_dirty = 1;

	ret = dpmgr_path_config(pgc->dpmgr_handle, data_config, disp_helper_get_option(DISP_HELPER_OPTION_USE_CMDQ)?pgc->cmdq_handle_config:NULL);

	if (disp_helper_get_option(DISP_HELPER_OPTION_USE_CMDQ)) 
	{
		/* should we set dirty here???????? */
		_cmdq_flush_config_handle(0, NULL, 0);

		_cmdq_reset_config_handle();
		_cmdq_insert_wait_frame_done_token();
	}

	if(disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL)
	{
		ret = disp_lcm_init(pgc->plcm, 0);
	}
	else
	{
		DISPMSG("force init lcm due to stage %s\n", disp_helper_stage_spy());
		ret = disp_lcm_init(pgc->plcm, 1);
	}

	if(primary_display_is_video_mode())
	{
#ifdef DISP_SWITCH_DST_MODE
		primary_display_cur_dst_mode = 1;//video mode
		primary_display_def_dst_mode = 1;//default mode is video mode
#endif
		dpmgr_map_event_to_irq(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC, DDP_IRQ_RDMA0_DONE);
	}

	pgc->lcm_fps = lcm_fps;
	pgc->max_layer = 4;
	primary_display_post_init();
	pgc->state = DISP_ALIVE;
done:
	_primary_path_unlock(__func__);
	return ret;
}

// for low power, debug
int primary_display_post_init(void)
{
	DISP_MODULE_ENUM dst_module = 0;

	if(disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL) {
		primary_display_esd_check_task = kthread_create(primary_display_esd_check_worker_kthread, NULL, "display_esd_check");
		init_waitqueue_head(&esd_ext_te_wq);
		init_waitqueue_head(&esd_check_task_wq);
		if (_need_do_esd_check()) {
			wake_up_process(primary_display_esd_check_task);
		}
		if (_need_register_eint()) {
			//int node;
			struct device_node *node;
			int irq;
			u32 ints[2]={0,0};//gpio pin,dsi te debounce
#ifdef GPIO_DSI_TE_PIN
			// 1.set GPIO107 eint mode	
			mt_set_gpio_mode(GPIO_DSI_TE_PIN, GPIO_DSI_TE_PIN_M_GPIO);
			eint_flag++;
#endif
			// 2.register eint
			node = of_find_compatible_node(NULL,NULL,"mediatek, DSI_TE_1-eint");
			if(node)
			{
				of_property_read_u32_array(node,"debounce",ints,ARRAY_SIZE(ints));
				mt_gpio_set_debounce(ints[0],ints[1]);
				irq = irq_of_parse_and_map(node,0);
				if(request_irq(irq, _esd_check_ext_te_irq_handler, IRQF_TRIGGER_NONE, "DSI_TE_1-eint", NULL))
				{
					DISPCHECK("[ESD]EINT IRQ LINE NOT AVAILABLE!!\n");
				} 
				else 
				{
					eint_flag++;
				}
			} 
			else 
			{
				DISPCHECK("[ESD][%s] can't find DSI_TE_1 eint compatible node\n", __func__);
			}
		}
		if (_need_do_esd_check()) {
			primary_display_esd_check_enable(1);
		}

#ifdef DISP_SWITCH_DST_MODE
		primary_display_switch_dst_mode_task = kthread_create(_disp_primary_path_switch_dst_mode_thread, NULL, "display_switch_dst_mode");
		wake_up_process(primary_display_switch_dst_mode_task);
#endif

		if (decouple_update_rdma_config_thread == NULL) {
			decouple_update_rdma_config_thread = kthread_create(decouple_mirror_update_rdma_config_thread, NULL, "decouple_update_rdma_cfg");
			wake_up_process(decouple_update_rdma_config_thread);
		}

		if (decouple_trigger_thread == NULL) {
			decouple_trigger_thread = kthread_create(decouple_trigger_worker_thread, NULL, "decouple_trigger");
			wake_up_process(decouple_trigger_thread);
		}

		primary_path_aal_task =  kthread_create(_disp_primary_path_check_trigger, NULL, "display_check_aal");
		wake_up_process(primary_path_aal_task);
	}
	init_waitqueue_head(&primary_display_present_fence_wq);
	present_fence_release_worker_task = kthread_create(_present_fence_release_worker_thread, NULL, "present_fence_worker");
	wake_up_process(present_fence_release_worker_task);

	if (primary_display_frame_update_task == NULL) {
		init_waitqueue_head(&primary_display_frame_update_wq);        
		disp_register_module_irq_callback(DISP_MODULE_RDMA0, primary_display_frame_update_irq_callback);
		disp_register_module_irq_callback(DISP_MODULE_OVL0, primary_display_frame_update_irq_callback);
		primary_display_frame_update_task = kthread_create(primary_display_frame_update_kthread, NULL, "frame_update_worker");
		wake_up_process(primary_display_frame_update_task);
	} 

#if 1
	if (primary_display_is_video_mode()) {
		primary_display_idlemgr_init();
	}
#endif

	if(primary_display_is_video_mode()) {
		spm_enable_sodi(0);
	} else {
		spm_enable_sodi(1);
	}

	if(pgc->plcm->params->lcm_if != LCM_INTERFACE_DSI_DUAL) {
		// only dual port could use two pipe path optimization
		disp_helper_set_option(DISP_HELPER_OPTION_TWO_PIPE_INTERFACE_PATH, 0);
	}

	dst_module = _get_dst_module_by_lcm(pgc->plcm);
	if(dst_module == DISP_MODULE_DSIDUAL) {
		DISPCHECK("disable_soidle for DSI0_1/SPLIT1\n");
		enable_soidle_by_bit(MT_CG_DISP1_DSI1_DIGITAL);
		enable_soidle_by_bit(MT_CG_DISP1_DSI1_ENGINE);
		enable_soidle_by_bit(MT_CG_DISP0_DISP_SPLIT1);
	}

	// xuecheng, try for sodi + decouple mode
	enable_soidle_by_bit(MT_CG_DISP0_DISP_WDMA0);
	enable_soidle_by_bit(MT_CG_DISP0_DISP_RDMA2);
	return 0;
}

int primary_display_deinit(void)
{
	_primary_path_lock(__func__);

	_cmdq_stop_trigger_loop();
	dpmgr_path_deinit(pgc->dpmgr_handle, CMDQ_DISABLE);
	_primary_path_unlock(__func__);
	return 0;
}

// register rdma done event
int primary_display_wait_for_idle(void)
{	
	DISP_STATUS ret = DISP_STATUS_OK;

	DISPFUNC();

	_primary_path_lock(__func__);

done:
	_primary_path_unlock(__func__);
	return ret;
}

int primary_display_wait_for_dump(void)
{

}

int primary_display_release_fence_fake(void)
{
	unsigned int layer_en = 0;
	unsigned int addr = 0;
	unsigned int fence_idx = -1;
	unsigned int session_id = MAKE_DISP_SESSION(DISP_SESSION_PRIMARY,0);
	int i = 0;

	DISPFUNC();

	for(i=0;i<PRIMARY_DISPLAY_SESSION_LAYER_COUNT;i++)
	{
		if(i == primary_display_get_option("ASSERT_LAYER") && is_DAL_Enabled())
		{
			mtkfb_release_layer_fence(session_id, 3);
		}
		else
		{
			disp_sync_get_cached_layer_info(session_id, i, &layer_en, &addr, &fence_idx);
			if(fence_idx <0) 
			{
				if(fence_idx == -1)
				{
					DISPPR_ERROR("find fence idx for layer %d,addr 0x%08x fail, unregistered addr%d\n", i, 0, fence_idx);
				}
				else if(fence_idx == -2)
				{

				}
				else
				{
					DISPPR_ERROR("find fence idx for layer %d,addr 0x%08x fail,reason unknown%d\n", i, 0, fence_idx);
				}
			}
			else
			{
				if(layer_en)
					mtkfb_release_fence(session_id, i, fence_idx-1);
				else
					mtkfb_release_fence(session_id, i, fence_idx);
			}
		}
	}

}

static unsigned int g_keep = 0;
static unsigned int g_skip = 0;

int primary_display_wait_for_vsync(void *config)
{
	//DISPMSG("v+\n");
	disp_session_vsync_config *c = (disp_session_vsync_config *)config;
	int ret = 0;
	// kick idle manager here to ensure sodi is disabled when screen update begin(not 100% ensure)
	primary_display_idlemgr_kick(__func__, 1);

#if 0
	if(!islcmconnected)
	{
		DISPCHECK("lcm not connect, use fake vsync\n");
		msleep(16);
		return 0;
	}
#endif

	if(pgc->force_fps_keep_count && pgc->force_fps_skip_count)
	{
		g_keep ++;
		DISPMSG("vsync|keep %d\n", g_keep);
		if(g_keep == pgc->force_fps_keep_count)
		{
			g_keep = 0;

			while(g_skip != pgc->force_fps_skip_count)
			{
				g_skip++;
				DISPMSG("vsync|skip %d\n", g_skip);
				ret = dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC, HZ/10); 
				if(ret == -2)
				{
					DISPCHECK("vsync for primary display path not enabled yet\n");
					return -1;
				}
				else if(ret == 0)
				{
					primary_display_release_fence_fake();
				}			
			}
			g_skip = 0;
		}
	}
	else
	{
		g_keep = 0;
		g_skip = 0;
	}

	ret = dpmgr_wait_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
	if(ret == -2)
	{
		DISPCHECK("vsync for primary display path not enabled yet\n");
		return -1;
	}
	else if(ret == 0)
	{
		//primary_display_release_fence_fake();
	}

	if(pgc->vsync_drop)
	{
		//ret = dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC, HZ/10); 
		ret = dpmgr_wait_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);		
	}

	//DISPMSG("v-\n");
	c->vsync_ts = get_current_time_us();
	c->vsync_cnt ++;

	return ret;
}

unsigned int primary_display_get_ticket(void)
{
	return dprec_get_vsync_count();
}

int primary_suspend_release_fence(void)
{
	unsigned int session = (unsigned int)( (DISP_SESSION_PRIMARY)<<16 | (0));
	unsigned int i=0; 
	for (i = 0; i < HW_OVERLAY_COUNT; i++)
	{
		DISPMSG("mtkfb_release_layer_fence  session=0x%x,layerid=%d \n", session,i);
		mtkfb_release_layer_fence(session, i);
	}
	return 0;
}
int primary_display_suspend(void)
{
	DISP_STATUS ret = DISP_STATUS_OK;

	DISPCHECK("primary_display_suspend begin\n");

	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagStart, 0, 0);
#ifdef DISP_SWITCH_DST_MODE
	primary_display_switch_dst_mode(primary_display_def_dst_mode);
#endif
	disp_sw_mutex_lock(&(pgc->capture_lock));
	_primary_path_esd_check_lock();	//add by lizhiye
	_primary_path_lock(__func__);
	if(pgc->state == DISP_SLEEPED)
	{
		DISPCHECK("primary display path is already sleep, skip\n");
		goto done;
	}
	primary_display_idlemgr_kick(__func__, 0);
	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 0, 1);
	if(dpmgr_path_is_busy(pgc->dpmgr_handle))
	{
		MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 1, 2);
		int event_ret = dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ*1);
		MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 2, 2);
		DISPCHECK("[POWER]primary display path is busy now, wait frame done, event_ret=%d\n", event_ret);
		if(event_ret<=0)
		{
			DISPERR("wait frame done in suspend timeout\n");
			MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 3, 2);
			primary_display_diagnose();
			ret = -1;	
		}
	}	

	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 0, 2);

	if(disp_helper_get_option(DISP_HELPER_OPTION_USE_CMDQ))
	{
		DISPCHECK("[POWER]display cmdq trigger loop stop[begin]\n");
		_cmdq_stop_trigger_loop();
		DISPCHECK("[POWER]display cmdq trigger loop stop[end]\n");
	}
	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 0, 3);

	DISPCHECK("[POWER]primary display path stop[begin]\n");
	dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("[POWER]primary display path stop[end]\n");
	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 0, 4);

	if(dpmgr_path_is_busy(pgc->dpmgr_handle))
	{
		MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse,1, 4);
		DISPERR("[POWER]stop display path failed, still busy\n");
		dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
		ret = -1;
	}
	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 0, 5);

	DISPCHECK("[POWER]lcm suspend[begin]\n");
	disp_lcm_suspend(pgc->plcm);
	DISPCHECK("[POWER]lcm suspend[end]\n");
	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 0, 6);
	DISPCHECK("[POWER]primary display path Release Fence[begin]\n");
	primary_suspend_release_fence();
	DISPCHECK("[POWER]primary display path Release Fence[end]\n");
	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 0, 7);

	DISPCHECK("[POWER]dpmanager path power off[begin]\n");
	dpmgr_path_power_off(pgc->dpmgr_handle, CMDQ_DISABLE);

	if(primary_display_is_decouple_mode())
	{
		if(pgc->ovl2mem_path_handle)
		{
			dpmgr_path_power_off(pgc->ovl2mem_path_handle, CMDQ_DISABLE);
		}
		else
		{
			DISPERR("display is decouple mode, but ovl2mem_path_handle is null\n");
		}
	}
	DISPCHECK("[POWER]dpmanager path power off[end]\n");
	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 0, 8);
done:
	pgc->state = DISP_SLEEPED;
	_primary_path_unlock(__func__);
	 _primary_path_esd_check_unlock();	//add by qinhai 2015-12-05
	disp_sw_mutex_unlock(&(pgc->capture_lock));	

#ifdef CONFIG_MTK_AEE_POWERKEY_HANG_DETECT
	aee_kernel_wdt_kick_Powkey_api("mtkfb_early_suspend", WDT_SETBY_Display);
#endif
	primary_trigger_cnt = 0;
	/* clear dim layer buffer */
	dim_layer_mva = 0;
	if (ipoh_after){
		mmdvfs_set_step(SMI_BWC_SCEN_UI_IDLE, MMDVFS_VOLTAGE_DEFAULT);
	}
	ipoh_after = 0;
	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagEnd, 0, 0);
	DISPCHECK("primary_display_suspend end\n");
	return ret;
}

int primary_display_get_lcm_index(void)
{
	int index = 0;
	DISPFUNC();

	if(pgc->plcm == NULL)
	{
		DISPERR("lcm handle is null\n");
		return 0;
	}

	index = pgc->plcm->index; 
	DISPMSG("lcm index = %d\n", index);
	return index;
}

int cabc_enable_flag = 0;	//add by lizhiye

int primary_display_resume(void)
{
	DISP_STATUS ret = DISP_STATUS_OK;
	DDP_SCENARIO_ENUM old_scenario, new_scenario;
	DISPFUNC();
	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagStart, 0, 0);

	if(is_ipoh_bootup && disp_helper_get_option(DISP_HELPER_OPTION_DEFAULT_DECOUPLE_MODE))
		mmdvfs_set_step(SMI_BWC_SCEN_UI_IDLE, MMDVFS_VOLTAGE_HIGH);

	_primary_path_lock(__func__);
	if(pgc->state == DISP_ALIVE)
	{
		DISPCHECK("primary display path is already resume, skip\n");
		goto done;
	}
	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 1);

	DISPCHECK("dpmanager path power on[begin]\n");

	if (primary_display_is_decouple_mode()) {
		cmdqCoreClearEvent(CMDQ_EVENT_DISP_WDMA0_EOF);
		/* reset deouple cmdq*/
		cmdqRecReset(pgc->cmdq_handle_ovl1to2_config);
		if (pgc->ovl2mem_path_handle == NULL) {
#if CONFIG_FOR_SOURCE_PQ
			pgc->ovl2mem_path_handle = dpmgr_create_path(DDP_SCENARIO_PRIMARY_DITHER_MEMOUT, pgc->cmdq_handle_ovl1to2_config);
#else
			pgc->ovl2mem_path_handle = dpmgr_create_path(DDP_SCENARIO_PRIMARY_OVL_MEMOUT, pgc->cmdq_handle_ovl1to2_config);
#endif
			if (pgc->ovl2mem_path_handle) {
				DISPCHECK("dpmgr create ovl memout path SUCCESS(%p)\n", pgc->ovl2mem_path_handle);
			} else {
				DISPCHECK("dpmgr create path FAIL\n");
				return -1;
			}
			dpmgr_path_set_video_mode(pgc->ovl2mem_path_handle, 0);
			dpmgr_path_init(pgc->ovl2mem_path_handle, CMDQ_ENABLE);
		}else{
			dpmgr_path_power_on(pgc->ovl2mem_path_handle, CMDQ_DISABLE);
		}
	}
	dpmgr_path_power_on(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("dpmanager path power on[end]\n");

	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 2);

	/* clear dim layer buffer */
	dim_layer_mva = 0;
	if(is_ipoh_bootup)
	{
		if(disp_helper_get_option(DISP_HELPER_OPTION_DEFAULT_DECOUPLE_MODE))
			ipoh_after = 1;

		DISPCHECK("[primary display path] leave primary_display_resume -- IPOH\n");
		DISPCHECK("ESD check start[begin]\n");
		primary_display_esd_check_enable(1);
		DISPCHECK("ESD check start[end]\n");
		is_ipoh_bootup = false;
		DISPCHECK("[POWER]start cmdq[begin]--IPOH\n");
		if(disp_helper_get_option(DISP_HELPER_OPTION_USE_CMDQ))
		{
			_cmdq_start_trigger_loop();
		}
		DISPCHECK("[POWER]start cmdq[end]--IPOH\n");	  
		goto done;
	}
	DISPCHECK("[POWER]dpmanager re-init[begin]\n");

	{
		/* disconnect primary path first *
		 * because MMsys config register may not power off during early suspend
		 * BUT session mode may change in primary_display_switch_mode() */
		ddp_disconnect_path(DDP_SCENARIO_PRIMARY_ALL, NULL);
		ddp_disconnect_path(DDP_SCENARIO_PRIMARY_RDMA0_COLOR0_DISP, NULL);

		dpmgr_path_connect(pgc->dpmgr_handle, CMDQ_DISABLE);
		if(primary_display_is_decouple_mode())
		{
			dpmgr_path_connect(pgc->ovl2mem_path_handle, CMDQ_DISABLE);
		}

		MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 1, 2);
		LCM_PARAMS *lcm_param = disp_lcm_get_params(pgc->plcm);

		disp_ddp_path_config * data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle); 

		data_config->dst_w = disp_helper_get_option(DISP_HELPER_OPTION_FAKE_LCM_WIDTH);
		data_config->dst_h = lcm_param->height;

		if(lcm_param->type == LCM_TYPE_DSI)
		{
			if(lcm_param->dsi.data_format.format == LCM_DSI_FORMAT_RGB888)
				data_config->lcm_bpp = 24;
			else if(lcm_param->dsi.data_format.format == LCM_DSI_FORMAT_RGB565)
				data_config->lcm_bpp = 16;
			else if(lcm_param->dsi.data_format.format == LCM_DSI_FORMAT_RGB666)
				data_config->lcm_bpp = 18;
		}
		else if(lcm_param->type == LCM_TYPE_DPI)
		{
			if( lcm_param->dpi.format == LCM_DPI_FORMAT_RGB888)
				data_config->lcm_bpp = 24;
			else if( lcm_param->dpi.format == LCM_DPI_FORMAT_RGB565)
				data_config->lcm_bpp = 16;
			if( lcm_param->dpi.format == LCM_DPI_FORMAT_RGB666)
				data_config->lcm_bpp = 18;
		}

		data_config->fps = pgc->lcm_fps;
		data_config->dst_dirty = 1;

		ret = dpmgr_path_config(pgc->dpmgr_handle, data_config, NULL);
		MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 2, 2);

		if(primary_display_is_decouple_mode())
		{
			// re-config RDMA input address
			unsigned int mva = pgc->dc_buf[pgc->dc_buf_id];

			data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle); 

			data_config->rdma_config.address = mva;

			data_config->rdma_config.height 			= lcm_param->height;
			data_config->rdma_config.width 				= disp_helper_get_option(DISP_HELPER_OPTION_FAKE_LCM_WIDTH);
			data_config->rdma_config.idx 				= 0;
			data_config->rdma_config.inputFormat 		= eRGB888;
			data_config->rdma_config.pitch 				= disp_helper_get_option(DISP_HELPER_OPTION_FAKE_LCM_WIDTH) * DP_COLOR_BITS_PER_PIXEL(eRGB888)/8;

			//ret = _convert_disp_input_to_rdma(&(data_config->rdma_config), &decouple_rdma_config);
			data_config->rdma_dirty = 1;
			dpmgr_path_config(pgc->dpmgr_handle, data_config, NULL);

			data_config = dpmgr_path_get_last_config(pgc->ovl2mem_path_handle); 

			data_config->dst_w = disp_helper_get_option(DISP_HELPER_OPTION_FAKE_LCM_WIDTH);
			data_config->dst_h = lcm_param->height;

			if(lcm_param->type == LCM_TYPE_DSI)
			{
				if(lcm_param->dsi.data_format.format == LCM_DSI_FORMAT_RGB888)
					data_config->lcm_bpp = 24;
				else if(lcm_param->dsi.data_format.format == LCM_DSI_FORMAT_RGB565)
					data_config->lcm_bpp = 16;
				else if(lcm_param->dsi.data_format.format == LCM_DSI_FORMAT_RGB666)
					data_config->lcm_bpp = 18;
			}
			else if(lcm_param->type == LCM_TYPE_DPI)
			{
				if( lcm_param->dpi.format == LCM_DPI_FORMAT_RGB888)
					data_config->lcm_bpp = 24;
				else if( lcm_param->dpi.format == LCM_DPI_FORMAT_RGB565)
					data_config->lcm_bpp = 16;
				if( lcm_param->dpi.format == LCM_DPI_FORMAT_RGB666)
					data_config->lcm_bpp = 18;
			}

			data_config->fps = pgc->lcm_fps;
			data_config->dst_dirty = 1;

			ret = dpmgr_path_config(pgc->ovl2mem_path_handle, data_config, NULL);
		}
	}

	DISPCHECK("[POWER]dpmanager re-init[end]\n");
	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 3);

	DISPCHECK("[POWER]lcm resume[begin]\n");
	disp_lcm_resume(pgc->plcm);
	DISPCHECK("[POWER]lcm resume[end]\n");

	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 4);
	if(dpmgr_path_is_busy(pgc->dpmgr_handle))
	{
		MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 1, 4);
		DISPERR("[POWER]Fatal error, we didn't start display path but it's already busy\n");
		ret = -1;
		//goto done;
	}

	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 5);
	DISPCHECK("[POWER]dpmgr path start[begin]\n");
	dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);

	if(primary_display_is_decouple_mode())
		dpmgr_path_start(pgc->ovl2mem_path_handle, CMDQ_DISABLE);

	DISPCHECK("[POWER]dpmgr path start[end]\n");

	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 6);
	if(dpmgr_path_is_busy(pgc->dpmgr_handle))
	{
		MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 1, 6);
		DISPERR("[POWER]Fatal error, we didn't trigger display path but it's already busy\n");
		ret = -1;
		//goto done;
	}
	primary_display_diagnose();
	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 7);
	if(primary_display_is_video_mode())
	{
		MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 1, 7);
		// for video mode, we need to force trigger here
		// for cmd mode, just set DPREC_EVENT_CMDQ_SET_EVENT_ALLOW when trigger loop start
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, CMDQ_DISABLE);
	}
	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 8);

	if(disp_helper_get_option(DISP_HELPER_OPTION_USE_CMDQ))
	{
		DISPCHECK("[POWER]start cmdq[begin]\n");
		_cmdq_start_trigger_loop();
		DISPCHECK("[POWER]start cmdq[end]\n");
	}
	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 9);

	//primary_display_diagnose();
	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 10);

	if(!primary_display_is_video_mode() )//&& !primary_display_is_dual_dsi())
	{
		/*refresh black picture of ovl bg*/
		DISPCHECK("[POWER]triggger cmdq[begin]\n");
		_trigger_display_interface(1, NULL, 0);
		DISPCHECK("[POWER]triggger cmdq[end]\n");
		mdelay(16);//wait for one frame for pms workarround!!!!
	}
	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 11);

	// reinit fake timer for debug, we can enable option then press powerkey to enable thsi feature.
	// use fake timer to generate vsync signal for cmd mode w/o LCM(originally using LCM TE Signal as VSYNC)
	// so we don't need to modify display driver's behavior.
	if(disp_helper_get_option(DISP_HELPER_OPTION_NO_LCM_FOR_LOW_POWER_MEASUREMENT))
	{
		// only for low power measurement
		DISPCHECK("WARNING!!!!!! FORCE NO LCM MODE!!!\n");
		islcmconnected = 0;

		// no need to change video mode vsync behavior
		if(!primary_display_is_video_mode())
		{
			_init_vsync_fake_monitor(pgc->lcm_fps);

			dpmgr_map_event_to_irq(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC, DDP_IRQ_UNKNOW);
		}
	}

#if 0
	DISPCHECK("[POWER]wakeup aal/od trigger process[begin]\n");
	wake_up_process(primary_path_aal_task);
	DISPCHECK("[POWER]wakeup aal/od trigger process[end]\n");
#endif
done:
	pgc->state = DISP_ALIVE;
	_primary_path_unlock(__func__);

	wake_up(&resume_wait_queue);

#ifdef CONFIG_MTK_AEE_POWERKEY_HANG_DETECT
	aee_kernel_wdt_kick_Powkey_api("mtkfb_late_resume", WDT_SETBY_Display);
#endif
	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagEnd, 0, 0);

	return 0;
}

int primary_display_ipoh_hiberation_prepare(void)
{
	DISPCHECK("ipoh_hiberation_prepare in\n");
	primary_display_esd_check_enable(0);
	if(disp_helper_get_option(DISP_HELPER_OPTION_DEFAULT_DECOUPLE_MODE)){
		mmdvfs_set_step(SMI_BWC_SCEN_UI_IDLE, MMDVFS_VOLTAGE_HIGH);
		_primary_path_lock(__func__);
		pgc->mmdvfs_power_state =1;
		primary_display_idlemgr_kick(__func__, 0);
		__primary_display_switch_mode(DISP_SESSION_DIRECT_LINK_MODE, MAKE_DISP_SESSION(DISP_SESSION_PRIMARY, 0), 0, DISP_USER_NORMAL);
		_primary_path_unlock(__func__);
	}
	DISPCHECK("ipoh_hiberation_prepare out\n");
	return 0;

}

int primary_display_ipoh_restore_prepare(void)
{
	DISPCHECK("ipoh_restore_prepare in\n");
	primary_display_esd_check_enable(0);
	if(disp_helper_get_option(DISP_HELPER_OPTION_DEFAULT_DECOUPLE_MODE)){
		mmdvfs_set_step(SMI_BWC_SCEN_UI_IDLE, MMDVFS_VOLTAGE_HIGH);
		_primary_path_lock(__func__);
		pgc->mmdvfs_power_state =1;
		primary_display_idlemgr_kick(__func__, 0);
		__primary_display_switch_mode(DISP_SESSION_DIRECT_LINK_MODE, MAKE_DISP_SESSION(DISP_SESSION_PRIMARY, 0), 0, DISP_USER_NORMAL);
		_primary_path_unlock(__func__);
	}

	/* always keep in directlink mode during IPOH */
	primary_display_idlemgr_kick(__func__, 1);
	disp_helper_set_option(DISP_HELPER_OPTION_IDLEMGR_SWTCH_DECOUPLE, 0);

	if (NULL != pgc->cmdq_handle_trigger) {
		struct TaskStruct *pTask = pgc->cmdq_handle_trigger->pRunningTask;
		if (NULL != pTask) {
			_cmdq_stop_trigger_loop();
		}
	}
	DISPCHECK("ipoh_restore_prepare out\n");
	return 0;
}

int primary_display_ipoh_post_hiberation(void)
{
	//DISPMSG("ipoh_post_hiberation in\n");
    	//this is done in resume
	//DISPMSG("ipoh_post_hiberation out\n");
	return 0;
}

int primary_display_start(void)
{
	DISP_STATUS ret = DISP_STATUS_OK;

	DISPFUNC();

	_primary_path_lock(__func__);
	dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);

	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		DISPCHECK("Fatal error, we didn't trigger display path but it's already busy\n");
		ret = -1;
		goto done;
	}

done:
	_primary_path_unlock(__func__);
	return ret;
}

int primary_display_stop(void)
{
	DISP_STATUS ret = DISP_STATUS_OK;

	DISPFUNC();
	_primary_path_lock(__func__);

	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ * 1);
	}

	dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);

	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		DISPCHECK("stop display path failed, still busy\n");
		ret = -1;
		goto done;
	}

done:
	_primary_path_unlock(__func__);
	return ret;
}

void primary_display_update_present_fence(unsigned int fence_idx)
{
	gPresentFenceIndex = fence_idx;    
	atomic_set(&primary_display_present_fence_update_event, 1);
	wake_up_interruptible(&primary_display_present_fence_wq);  
}

static int trigger_decouple_mirror()
{
	if(pgc->need_trigger_dcMirror_out == 0){
		DISPCHECK("There is no output config when decouple mirror!!\n");
	} else {
		pgc->need_trigger_dcMirror_out = 0;

		/* check if still in decouple mirror mode: *
		 * DC->DL switch may happen before vsync, it may free ovl2mem_handle ! */
		if(pgc->session_mode == DISP_SESSION_DECOUPLE_MIRROR_MODE) {
			_trigger_ovl_to_memory_mirror(pgc->ovl2mem_path_handle,pgc->cmdq_handle_ovl1to2_config,
					_olv_wdma_fence_release_callback, DISP_SESSION_DECOUPLE_MIRROR_MODE);
			dprec_logger_trigger(DPREC_LOGGER_PRIMARY_TRIGGER, 0xffffffff, 0);
		} else {
			dprec_logger_trigger(DPREC_LOGGER_PRIMARY_TRIGGER, 0xffffffff, 0xaaaaaaaa);
		}
	}
	return 0;
}

static int config_decouple_output_buffer(disp_path_handle disp_handle,
		cmdqRecHandle cmdq_handle)
{
	unsigned int wdma_mva = 0;
	pgc->dc_buf_id++;
	pgc->dc_buf_id %= DISP_INTERNAL_BUFFER_COUNT;
	wdma_mva = pgc->dc_buf[pgc->dc_buf_id];
	WDMA_CONFIG_STRUCT tem_config = decouple_wdma_config;
	tem_config.dstAddress = wdma_mva;
	_config_wdma_output(&tem_config, disp_handle, cmdq_handle);

	mem_config.addr = wdma_mva;
	mem_config.buff_idx= -1;
	mem_config.interface_idx = -1;
	mem_config.security = DISP_NORMAL_BUFFER;
	mem_config.pitch = tem_config.dstPitch;
	MMProfileLogEx(ddp_mmp_get_events()->primary_wdma_config, MMProfileFlagPulse, pgc->dc_buf_id, wdma_mva);
	return 0;
}

static int trigger_decouple()
{
	/* check if still in decouple  mode: *
	 * DC->DL switch may happen before vsync, it may free ovl2mem_handle ! */
	if(pgc->session_mode == DISP_SESSION_DECOUPLE_MODE) {
		config_decouple_output_buffer(pgc->ovl2mem_path_handle,pgc->cmdq_handle_ovl1to2_config);
		_trigger_ovl_to_memory(pgc->ovl2mem_path_handle,pgc->cmdq_handle_ovl1to2_config,
				_ovl_fence_release_callback, DISP_SESSION_DECOUPLE_MODE);
		dprec_logger_trigger(DPREC_LOGGER_PRIMARY_TRIGGER, DISP_SESSION_DECOUPLE_MODE, 0);
	} 
	return 0;
}

int primary_display_trigger(int blocking, void *callback, int need_merge)
{
	int ret = 0;

	dprec_logger_start(DPREC_LOGGER_PRIMARY_TRIGGER, 0, 0);

#ifdef DISP_SWITCH_DST_MODE
	last_primary_trigger_time = sched_clock();
	if(is_switched_dst_mode)
	{
		primary_display_switch_dst_mode(1);//swith to vdo mode if trigger disp
		is_switched_dst_mode = false;
	}
#endif	

	primary_trigger_cnt++;
	_primary_path_lock(__func__);

	primary_display_idlemgr_kick(__func__, 0);

	if(pgc->state == DISP_SLEEPED)
	{
		DISPCHECK("%s, skip because primary dipslay is sleep\n", __func__);
		goto done;
	}

	if(blocking)
		DISPMSG("%s, change blocking to non blocking trigger\n", __func__);

	dprec_logger_start(DPREC_LOGGER_PRIMARY_TRIGGER, 0, 0);

	if(pgc->session_mode == DISP_SESSION_DIRECT_LINK_MODE){	
		_trigger_display_interface(blocking,_ovl_fence_release_callback,DISP_SESSION_DIRECT_LINK_MODE);
	}else if (pgc->session_mode == DISP_SESSION_DIRECT_LINK_MIRROR_MODE){
		_trigger_display_interface(0,_ovl_fence_release_callback, DISP_SESSION_DIRECT_LINK_MIRROR_MODE); 
		primary_display_remove_output(_wdma_fence_release_callback, DISP_SESSION_DIRECT_LINK_MIRROR_MODE);
	}else if (pgc->session_mode == DISP_SESSION_DECOUPLE_MODE){
		if(need_merge == 0) {
			trigger_decouple();
		} else {
			/*   MM/UI thread will  trigger in one vsync */
			/* we have to merge it up, then trigger at next vsync */
			/* see decouple_trigger_worker_thread below */
			DISPCHECK("decouple need merge\n");
			atomic_set(&decouple_trigger_event, 1);
			wake_up(&decouple_trigger_wq);
		}
	}else if(pgc->session_mode == DISP_SESSION_DECOUPLE_MIRROR_MODE){
		if(need_merge == 0) {
			trigger_decouple_mirror();
		} else {
			/* because only one of MM/UI thread will set output */
			/* we have to merge it up, then trigger at next vsync */
			/* see decouple_trigger_worker_thread below */
			atomic_set(&decouple_trigger_event, 1);
			wake_up(&decouple_trigger_wq);
		}
	}

done:
	atomic_set(&hwc_configing, 0);
	_primary_path_unlock(__func__);
	dprec_logger_done(DPREC_LOGGER_PRIMARY_TRIGGER, 0, 0);

#ifdef CONFIG_MTK_AEE_POWERKEY_HANG_DETECT
	if((primary_trigger_cnt > PRIMARY_DISPLAY_TRIGGER_CNT) && aee_kernel_Powerkey_is_press())
	{
		aee_kernel_wdt_kick_Powkey_api("primary_display_trigger", WDT_SETBY_Display);
		primary_trigger_cnt = 0;
	}
#endif

	if(pgc->session_id > 0)
		update_frm_seq_info(0 ,0, 0, FRM_TRIGGER);

	return ret;
}

static int decouple_trigger_worker_thread(void *data)
{
	struct sched_param param = { .sched_priority = RTPM_PRIO_SCRN_UPDATE };
	sched_setscheduler(current, SCHED_RR, &param);

	while(1)
	{   
		wait_event(decouple_trigger_wq, atomic_read(&decouple_trigger_event));
		dpmgr_wait_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);

		_primary_path_lock(__func__);
		if(pgc->session_mode == DISP_SESSION_DECOUPLE_MIRROR_MODE) {
			trigger_decouple_mirror();
		}else if(pgc->session_mode == DISP_SESSION_DECOUPLE_MODE) {
			trigger_decouple();
		}
		atomic_set(&decouple_trigger_event, 0);
		_primary_path_unlock(__func__);
		if(kthread_should_stop()) {
			DISPERR("error: stop %s as demond\n", __func__);
			break;
		}	
	}	
	return 0;
}



static int primary_display_ovl2mem_callback(unsigned int userdata)
{
	int i = 0;
	unsigned int session_id = MAKE_DISP_SESSION(DISP_SESSION_PRIMARY,0);
	int fence_idx = userdata;

	disp_ddp_path_config *data_config= dpmgr_path_get_last_config(pgc->dpmgr_handle);

	if(data_config)
	{
		WDMA_CONFIG_STRUCT wdma_layer;

		wdma_layer.dstAddress = mtkfb_query_buf_mva(session_id, 4, fence_idx);
		wdma_layer.outputFormat = data_config->wdma_config.outputFormat;
		wdma_layer.srcWidth = primary_display_get_width();
		wdma_layer.srcHeight = primary_display_get_height();
		wdma_layer.dstPitch = data_config->wdma_config.dstPitch;
		dprec_mmp_dump_wdma_layer(&wdma_layer, 0);
	}    

	if(fence_idx > 0)
		mtkfb_release_fence(session_id, EXTERNAL_DISPLAY_SESSION_LAYER_COUNT, fence_idx);

	DISPMSG("mem_out release fence idx:0x%x\n", fence_idx);

	return 0;
}

int primary_display_mem_out_trigger(int blocking, void *callback, unsigned int userdata)
{
	int ret = 0;
	//DISPFUNC();
	if(pgc->state == DISP_SLEEPED || pgc->ovl1to2_mode < DISP_SESSION_DIRECT_LINK_MIRROR_MODE)
	{
		DISPCHECK("mem out trigger is already sleeped or mode wrong(%d)\n", pgc->ovl1to2_mode);		
		return 0;
	}

	///dprec_logger_start(DPREC_LOGGER_PRIMARY_TRIGGER, 0, 0);

	//if(blocking)
	{
		_primary_path_lock(__func__);
	}

	if(pgc->need_trigger_ovl1to2 == 0)
	{
		goto done;
	}

	if(_should_wait_path_idle())
	{
		dpmgr_wait_event_timeout(pgc->ovl2mem_path_handle, DISP_PATH_EVENT_FRAME_DONE, HZ*1);
	}

	if(_should_trigger_path())
	{   
		///dpmgr_path_trigger(pgc->dpmgr_handle, NULL, primary_display_cmdq_enabled());
	}
	if(_should_set_cmdq_dirty())
	{
		_cmdq_set_config_handle_dirty_mira(pgc->cmdq_handle_ovl1to2_config);
	}

	if(_should_flush_cmdq_config_handle())
		_cmdq_flush_config_handle_mira(pgc->cmdq_handle_ovl1to2_config, 0);  

	if(_should_reset_cmdq_config_handle())
		cmdqRecReset(pgc->cmdq_handle_ovl1to2_config);

	cmdqRecWait(pgc->cmdq_handle_ovl1to2_config, CMDQ_EVENT_DISP_WDMA0_SOF);

	_cmdq_insert_wait_frame_done_token_mira(pgc->cmdq_handle_ovl1to2_config);
	dpmgr_path_remove_memout(pgc->ovl2mem_path_handle, pgc->cmdq_handle_ovl1to2_config);

	if(_should_set_cmdq_dirty())
		_cmdq_set_config_handle_dirty_mira(pgc->cmdq_handle_ovl1to2_config);


	if(_should_flush_cmdq_config_handle())
		///_cmdq_flush_config_handle_mira(pgc->cmdq_handle_ovl1to2_config, 0);  
		cmdqRecFlushAsyncCallback(pgc->cmdq_handle_ovl1to2_config, primary_display_ovl2mem_callback, userdata);

	if(_should_reset_cmdq_config_handle())
		cmdqRecReset(pgc->cmdq_handle_ovl1to2_config);      

	///_cmdq_insert_wait_frame_done_token_mira(pgc->cmdq_handle_ovl1to2_config);

done:

	pgc->need_trigger_ovl1to2 = 0;

	_primary_path_unlock(__func__); 

	///dprec_logger_done(DPREC_LOGGER_PRIMARY_TRIGGER, 0, 0);

	return ret;
}

static int config_wdma_output(disp_path_handle disp_handle, 
		cmdqRecHandle cmdq_handle, 
		disp_mem_output_config* output)
{
	disp_ddp_path_config *pconfig =NULL;

	ASSERT(output != NULL);
	pconfig = dpmgr_path_get_last_config(disp_handle);
	pconfig->wdma_config.dstAddress         = output->addr;
	pconfig->wdma_config.srcHeight 			= output->h;
	pconfig->wdma_config.srcWidth 			= disp_helper_get_option(DISP_HELPER_OPTION_FAKE_LCM_WIDTH);
	pconfig->wdma_config.clipX 				= output->x;
	pconfig->wdma_config.clipY 				= output->y;
	pconfig->wdma_config.clipHeight 		= output->h;
	pconfig->wdma_config.clipWidth 			= output->w;
	pconfig->wdma_config.outputFormat 		= output->fmt; 
	pconfig->wdma_config.useSpecifiedAlpha 	= 1;
	pconfig->wdma_config.alpha				= 0xFF;
	pconfig->wdma_config.dstPitch			= output->pitch;
	pconfig->wdma_dirty = 1;

	return dpmgr_path_config(disp_handle, pconfig, cmdq_handle);
}

int primary_display_config_output(disp_mem_output_config* output)
{
	int ret = 0;
	int i=0;
	int layer =0;
	disp_ddp_path_config *pconfig =NULL;
	disp_path_handle disp_handle;
	cmdqRecHandle cmdq_handle = NULL;
	_primary_path_lock(__func__);

	if (pgc->state == DISP_SLEEPED){
		DISPCHECK("mem out is already sleeped or mode wrong(%d)\n", pgc->session_mode);		
		goto done;
	}

	if (!primary_display_is_mirror_mode())
	{
		DISPERR("should not config output if not mirror mode!!\n");
		goto done;
	}

	if (primary_display_is_decouple_mode()) {
		disp_handle = pgc->ovl2mem_path_handle;
		cmdq_handle = pgc->cmdq_handle_ovl1to2_config;

	}else{
		disp_handle = pgc->dpmgr_handle;
		cmdq_handle =  pgc->cmdq_handle_config;
	}

	if (primary_display_is_decouple_mode()) {
		pgc->need_trigger_dcMirror_out = 1;
	}else{
		/*direct link mirror mode should add memout first*/
		dpmgr_path_add_memout(pgc->dpmgr_handle, ENGINE_OVL0, cmdq_handle);
		pgc->need_trigger_ovl1to2 = 1;
	}

	ret = config_wdma_output(disp_handle, cmdq_handle, output);

	if((pgc->session_id > 0)&& primary_display_is_decouple_mode())
		update_frm_seq_info(output->addr,0, mtkfb_query_frm_seq_by_addr(pgc->session_id, 0, 0), FRM_CONFIG);

	mem_config = *output;
	MMProfileLogEx(ddp_mmp_get_events()->primary_wdma_config, MMProfileFlagPulse, output->buff_idx, (unsigned int)output->addr);

done:
	_primary_path_unlock(__func__);

	return ret;

}

int primary_display_one_layer_dl(primary_disp_input_config* input)
{
	static int one_layer_cnt = 0;
	static unsigned int session_id = MAKE_DISP_SESSION(DISP_SESSION_PRIMARY, 0);
	disp_path_handle disp_handle = NULL;
	disp_ddp_path_config *data_config = NULL;
	int layer_number = 0;
	int total_height = 0;
	int i = 0;
	int need_dc = 0;
	OVL_CONFIG_STRUCT  ovl_config[HW_OVERLAY_COUNT];

	if (_should_config_ovl_input() && pgc->state == DISP_ALIVE) {
		if (primary_display_is_decouple_mode()){
			disp_handle = pgc->ovl2mem_path_handle;
		}else{
			disp_handle = pgc->dpmgr_handle;
		}
		data_config = dpmgr_path_get_last_config(disp_handle);    

		memcpy(ovl_config, data_config->ovl_config, sizeof(ovl_config));
		for (i = 0;i< HW_OVERLAY_COUNT;i++) {
			if(input[i].dirty){
				//mtk_disp_mgr has maped input to correct layer
				_convert_disp_input_to_ovl(&(ovl_config[i]), &input[i]);
			}
			layer_number += ovl_config[i].layer_en;

			if(ovl_config[i].layer_en)
				total_height += ovl_config[i].dst_h;
		}
		if (layer_number > 1){
			if(total_height > primary_display_get_height()){
				need_dc = 1;
			}
		}
		if (need_dc){
			one_layer_cnt = 0;
			if(pgc->ovl_one_layer == 1) {
				DISPCHECK("[DISPMODE] layer switch to DC\n");
				pgc->ovl_one_layer = 0;
				if(disp_helper_get_option(DISP_HELPER_OPTION_DEFAULT_DECOUPLE_MODE)){
					_primary_path_unlock(__func__);
					mmdvfs_set_step(SMI_BWC_SCEN_UI_IDLE, MMDVFS_VOLTAGE_HIGH);
					__primary_display_switch_mode(DISP_SESSION_DECOUPLE_MODE, session_id, 1, DISP_USER_LAYER);
					mmdvfs_set_step(SMI_BWC_SCEN_UI_IDLE, MMDVFS_VOLTAGE_DEFAULT);
					_primary_path_lock(__func__);

				}else{    
					__primary_display_switch_mode(DISP_SESSION_DECOUPLE_MODE, session_id, 0, DISP_USER_LAYER);
				}
			}
		}else{
			if(one_layer_cnt >= one_layer_dl_condition){
				if(pgc->ovl_one_layer == 0) {
					DISPCHECK("[DISPMODE] layer switch to DL\n");
					pgc->ovl_one_layer = 1;
					__primary_display_switch_mode(DISP_SESSION_DIRECT_LINK_MODE, session_id, 0, DISP_USER_LAYER);
				}
			}else{
				one_layer_cnt ++;
			}
		}
	}
	return 0;
}

int primary_display_config_input_multiple(primary_disp_input_config* input)
{
	int ret = 0;
	int i=0;
	int layer =0;
	disp_path_handle disp_handle = NULL;
	cmdqRecHandle cmdq_handle = NULL;
	disp_ddp_path_config *data_config = NULL;	

	_primary_path_lock(__func__);
	atomic_set(&hwc_configing, 1);
	primary_display_idlemgr_kick(__func__, 0);

	//Check path config first, __primary_display_switch_mode() will restore gce handle token, so it's safe for original ovl config to use
	if(disp_helper_get_option(DISP_HELPER_OPTION_AUTO_1L_DL)) {
		primary_display_one_layer_dl(input);
	}

	if (primary_display_is_decouple_mode()){
		disp_handle = pgc->ovl2mem_path_handle;
		cmdq_handle = pgc->cmdq_handle_ovl1to2_config;
	}else{
		disp_handle = pgc->dpmgr_handle;
		cmdq_handle = pgc->cmdq_handle_config;
	}

	data_config = dpmgr_path_get_last_config(disp_handle);

	if(pgc->state == DISP_SLEEPED) {
		DISPCHECK("%s, skip because primary dipslay is sleep\n", __func__);
		if(isAEEEnabled) {
			if(input[0].layer == primary_display_get_option("ASSERT_LAYER"))
				ret = _convert_disp_input_to_ovl(&(data_config->ovl_config[input[0].layer]), &input[0]);

			DISPCHECK("%s save temp asset layer ,because primary dipslay is sleeped\n", __func__);
		}
		goto done;
	}

	// hope we can use only 1 input struct for input config, just set layer number
	if(_should_config_ovl_input()) {
		for(i = 0;i< HW_OVERLAY_COUNT;i++) {	
			OVL_CONFIG_STRUCT *ovl_cfg = &(data_config->ovl_config[0]);;

			//DISPMSG("[primary], i:%d, layer:%d, layer_en:%d, dirty:%d\n", i, input[i].layer, input[i].layer_en, input[i].dirty);
			if(input[i].dirty) {
				dprec_logger_start(DPREC_LOGGER_PRIMARY_CONFIG, input[i].layer|(input[i].layer_en<<16), input[i].addr);
				dprec_mmp_dump_ovl_layer(&(data_config->ovl_config[input[i].layer]),input[i].layer, 1);
				ret = _convert_disp_input_to_ovl(&(data_config->ovl_config[input[i].layer]), &input[i]);
				dprec_logger_done(DPREC_LOGGER_PRIMARY_CONFIG, input[i].src_x, input[i].src_y);

				data_config->ovl_dirty = 1;	
			}

			if((ovl_cfg->layer == 0)&&(!primary_display_is_decouple_mode()))
				update_frm_seq_info(ovl_cfg->addr,  ovl_cfg->src_x*4 + ovl_cfg->src_y*ovl_cfg->src_pitch,
						mtkfb_query_frm_seq_by_addr(pgc->session_id, 0, 0), FRM_CONFIG);

		}
	} else {
		ret = _convert_disp_input_to_rdma(&(data_config->rdma_config), input);
		data_config->rdma_dirty= 1;
	}

	if(_should_wait_path_idle()) {
		dpmgr_wait_event_timeout(disp_handle, DISP_PATH_EVENT_FRAME_DONE, HZ*1);
	}

	ret = dpmgr_path_config(disp_handle, data_config, cmdq_handle);

	if(_should_config_ovl_input()) {
		for(i = 0;i< HW_OVERLAY_COUNT;i++) {	
			/* write fence_id/enable to DRAM using cmdq 
			 * it will be used when release fence (put these after config registers done)*/
			if(input[i].dirty) {
				unsigned int last_fence, cur_fence;
				primary_disp_input_config *input_cfg = &input[i];
				layer = input_cfg->layer;

				cmdqBackupReadSlot(pgc->cur_config_fence, layer, &last_fence);
				cur_fence = input_cfg->buff_idx;

				if(cur_fence != -1 && cur_fence > last_fence)
					cmdqRecBackupUpdateSlot(cmdq_handle, pgc->cur_config_fence, layer, cur_fence);

				/* for dim_layer/disable_layer/no_fence_layer, just release all fences configured*/
				/* for other layers, release current_fence-1 */
				if(	input_cfg->buffer_source == DISP_BUFFER_ALPHA
						|| 	input_cfg->layer_en == 0
						|| 	cur_fence == -1 ){
					cmdqRecBackupUpdateSlot(cmdq_handle, pgc->subtractor_when_free, layer, 0);
				}else{
					cmdqRecBackupUpdateSlot(cmdq_handle, pgc->subtractor_when_free, layer, 1);
				}
			}

		}
	}
	/* backup ovl status for debug */
	if(primary_display_is_video_mode() && !primary_display_is_decouple_mode()) {
		cmdqRecBackupRegisterToSlot(cmdq_handle, pgc->ovl_status_info, 0,  disp_addr_convert(DISP_REG_OVL_STA));
	}
done:
	_primary_path_unlock(__func__);
	return ret;
}

int primary_display_config_input(primary_disp_input_config* input)
{
	int ret = 0;
	int i=0;
	int layer =0;
	DISPCHECK("primary_display_config_input begin\n");
	disp_ddp_path_config *data_config;	
	disp_path_handle disp_handle = NULL;
	cmdqRecHandle cmdq_handle = NULL;

	_primary_path_lock(__func__);

	primary_display_idlemgr_kick(__func__, 0);

	if (primary_display_is_decouple_mode()) {
		disp_handle = pgc->ovl2mem_path_handle;
		cmdq_handle = pgc->cmdq_handle_ovl1to2_config;
	} else {
		disp_handle = pgc->dpmgr_handle;
		cmdq_handle = pgc->cmdq_handle_config;
	}

	data_config = dpmgr_path_get_last_config(disp_handle);

	if(pgc->state == DISP_SLEEPED)
	{
		if(isAEEEnabled && input->layer == primary_display_get_option("ASSERT_LAYER"))
		{
			// hope we can use only 1 input struct for input config, just set layer number
			if(_should_config_ovl_input())
			{
				ret = _convert_disp_input_to_ovl(&(data_config->ovl_config[input->layer]), input);
				data_config->ovl_dirty = 1;
			}
			else
			{
				ret = _convert_disp_input_to_rdma(&(data_config->rdma_config), input);
				data_config->rdma_dirty= 1;
			}
			DISPCHECK("%s save temp asset layer ,because primary dipslay is sleep\n", __func__);
		}
		DISPCHECK("%s, skip because primary dipslay is sleep\n", __func__);
		goto done;
	}

	dprec_logger_start(DPREC_LOGGER_PRIMARY_CONFIG, input->layer|(input->layer_en<<16), input->addr);

	// hope we can use only 1 input struct for input config, just set layer number
	if(_should_config_ovl_input())
	{
		ret = _convert_disp_input_to_ovl(&(data_config->ovl_config[input->layer]), input);
		data_config->ovl_dirty = 1;
	}
	else
	{
		ret = _convert_disp_input_to_rdma(&(data_config->rdma_config), input);
		data_config->rdma_dirty= 1;
	}

	if(_should_wait_path_idle())
	{
		dpmgr_wait_event_timeout(disp_handle, DISP_PATH_EVENT_FRAME_DONE, HZ*1);
	}

	ret = dpmgr_path_config(disp_handle, data_config, primary_display_cmdq_enabled()?cmdq_handle:NULL);

	// this is used for decouple mode, to indicate whether we need to trigger ovl
	pgc->need_trigger_overlay = 1;

	dprec_logger_done(DPREC_LOGGER_PRIMARY_CONFIG, input->src_x, input->src_y);

done:	
	_primary_path_unlock(__func__);
	DISPCHECK("primary_display_config_input end\n");
	return ret;
}

static int Panel_Master_primary_display_config_dsi(const char * name,UINT32  config_value)

{
	int ret = 0;	
	disp_ddp_path_config *data_config;	
	// all dirty should be cleared in dpmgr_path_get_last_config()
	data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	//modify below for config dsi
	if(!strcmp(name, "PM_CLK"))
	{
		printk("Pmaster_config_dsi: PM_CLK:%d\n", config_value);
		data_config->dispif_config.dsi.PLL_CLOCK= config_value;	
	}
	else if(!strcmp(name, "PM_SSC"))
	{	
		data_config->dispif_config.dsi.ssc_range=config_value;
	}
	printk("Pmaster_config_dsi: will Run path_config()\n");
	ret = dpmgr_path_config(pgc->dpmgr_handle, data_config, NULL);

	return ret;
}
int primary_display_user_cmd(unsigned int cmd, unsigned long arg)
{
	int ret =0;
	cmdqRecHandle handle = NULL;
	int cmdqsize = 0;

	MMProfileLogEx(ddp_mmp_get_events()->primary_display_cmd, MMProfileFlagStart, handle, 0);    	

	ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP,&handle);
	cmdqRecReset(handle);
	//_cmdq_handle_clear_dirty(handle);
	_cmdq_insert_wait_frame_done_token_mira(handle);		        
	cmdqsize = cmdqRecGetInstructionCount(handle);

	_primary_path_lock(__func__);
	if(pgc->state == DISP_SLEEPED)
	{
		cmdqRecDestroy(handle);
		handle = NULL;
	}
	_primary_path_unlock(__func__); 		

	ret = dpmgr_path_user_cmd(pgc->dpmgr_handle,cmd,arg, handle);

	if(handle)
	{
		if(cmdqRecGetInstructionCount(handle) > cmdqsize)
		{
			_primary_path_lock(__func__);
			if(pgc->state == DISP_ALIVE)
			{
				// do not set dirty here, just write register.
				// if set dirty needed, will be implemented by dpmgr_module_notify()
				//_cmdq_set_config_handle_dirty_mira(handle);
				// use non-blocking flush here to avoid primary path is locked for too long
				_cmdq_flush_config_handle_mira(handle, 0);
			}
			_primary_path_unlock(__func__);			
		}

		cmdqRecDestroy(handle);
	}

	MMProfileLogEx(ddp_mmp_get_events()->primary_display_cmd, MMProfileFlagEnd, handle, cmdqsize);    		

	return ret;
}

static DISP_MODE select_mode_from_feature()
{
	if(pgc->test_mode != DISP_INVALID_SESSION_MODE) {
		DISPCHECK("display test mode %s\n", session_mode_spy(pgc->test_mode));
		return pgc->test_mode;
	}
	if(disp_helper_get_option(DISP_HELPER_OPTION_DEFAULT_DECOUPLE_MODE)){
		DISPMSG("mode condition: layer %d, power %d, idle %d\n", pgc->ovl_one_layer,pgc->mmdvfs_power_state,primary_display_is_idle());
		if ((pgc->ovl_one_layer==1 || pgc->mmdvfs_power_state == 1) /*&& !primary_display_is_idle()*/) 
			return DISP_SESSION_DIRECT_LINK_MODE;
		else
			return DISP_SESSION_DECOUPLE_MODE;
	}else{
		if (primary_display_is_idle()){
			return DISP_SESSION_DECOUPLE_MODE;
		}else{
			return DISP_SESSION_DIRECT_LINK_MODE;
		}
	}
}

static DISP_MODE select_session_mode(int request_mode, enum DISP_USER user)
{
	int  final_mode = pgc->session_mode;
	if (request_mode == DISP_SESSION_DIRECT_LINK_MODE) {
		if(final_mode == DISP_SESSION_DECOUPLE_MIRROR_MODE){
			if (user == DISP_USER_HWC) {
				/*only hwc has this permission*/
				final_mode = select_mode_from_feature();
			}
		}else if (final_mode == DISP_SESSION_DECOUPLE_MODE) {
			final_mode = select_mode_from_feature();
		} else {
			final_mode = select_mode_from_feature();
		}
	} else if (request_mode == DISP_SESSION_DECOUPLE_MIRROR_MODE) {
		final_mode = DISP_SESSION_DECOUPLE_MIRROR_MODE;
	} else if (request_mode == DISP_SESSION_DECOUPLE_MODE) {
		if(final_mode != DISP_SESSION_DECOUPLE_MIRROR_MODE){
			final_mode = select_mode_from_feature();
		}
	} else {
		DISPERR("invalid session mode %d\n", request_mode);
	}
	if(DISP_USER_FORCE == user){
		final_mode = request_mode;
	}
	//DISPMSG("cur %d, req %d, res %d by user(%d)\n",
	//	pgc->session_mode, request_mode, final_mode, user);
	return final_mode;
}

static int __primary_display_switch_mode(int sess_mode, unsigned int session, int need_lock, enum DISP_USER user)
{
	int ret = 0;
	int sw_only = 0;
	int request_mode = sess_mode;
	if(need_lock)
		_primary_path_lock(__func__);

	MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagStart, pgc->session_mode, sess_mode);
	if (disp_helper_get_option(DISP_HELPER_OPTION_DEFAULT_DECOUPLE_MODE)) {
		sess_mode = select_session_mode(request_mode, user);
		if (sess_mode != request_mode) { 
			DISPCHECK("[DISPMODE]: cur %s, req %s, res %s by user(%d)\n",
					session_mode_spy(pgc->session_mode),session_mode_spy(request_mode), session_mode_spy(sess_mode), user);
		}
	}

	if(pgc->session_mode == sess_mode)
	{
		goto done;
	}
	if(pgc->state == DISP_SLEEPED)
	{
		DISPCHECK("primary display switch from %s to %s in suspend state!!!\n", 
				session_mode_spy(pgc->session_mode), session_mode_spy(sess_mode));
		sw_only = 1;
	}
	//DISPMSG("primary display will switch from %s to %s by %d\n", session_mode_spy(pgc->session_mode), session_mode_spy(sess_mode),user);

	if(pgc->session_mode == DISP_SESSION_DIRECT_LINK_MODE &&  sess_mode == DISP_SESSION_DECOUPLE_MODE)
	{
		/* dl to dc*/
		// 2 pipe will only be used for decouple mode, not decouple mirror mode for now.
		if(disp_helper_get_option(DISP_HELPER_OPTION_TWO_PIPE_INTERFACE_PATH))
		{
			DL_switch_to_DC_fast_two_pipe(sw_only);
		}
		else
		{
			DL_switch_to_DC_fast(sw_only);
		}

		pgc->session_mode = sess_mode;
		MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse, pgc->session_mode, sess_mode);
	}else if(pgc->session_mode == DISP_SESSION_DECOUPLE_MODE && sess_mode == DISP_SESSION_DIRECT_LINK_MODE){
		/* dc to dl*/
		DC_switch_to_DL_fast(sw_only);
		pgc->session_mode = sess_mode;
		MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse, pgc->session_mode, sess_mode);
		//primary_display_diagnose();
	}else if(pgc->session_mode == DISP_SESSION_DIRECT_LINK_MODE && sess_mode == DISP_SESSION_DIRECT_LINK_MIRROR_MODE){
		/* dl to dl mirror*/
		pgc->session_mode = sess_mode;
		MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse, pgc->session_mode, sess_mode);
	}else if(pgc->session_mode == DISP_SESSION_DIRECT_LINK_MIRROR_MODE && sess_mode == DISP_SESSION_DIRECT_LINK_MODE){
		/*dl mirror to dl*/
		pgc->session_mode = sess_mode;
		MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse, pgc->session_mode, sess_mode);
	}else if(pgc->session_mode == DISP_SESSION_DIRECT_LINK_MODE && sess_mode == DISP_SESSION_DECOUPLE_MIRROR_MODE){
		/* dl to dc mirror */
		DL_switch_to_DC_fast(sw_only);        
		pgc->session_mode = sess_mode;
		MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse, pgc->session_mode, sess_mode);   
	}else if(pgc->session_mode == DISP_SESSION_DECOUPLE_MODE && sess_mode == DISP_SESSION_DECOUPLE_MIRROR_MODE){
		/* dc to dc mirror */
		//DL_switch_to_DC_fast(sw_only);        
		pgc->session_mode = sess_mode;
		MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse, pgc->session_mode, sess_mode);
		//primary_display_diagnose();        
	}else if(pgc->session_mode == DISP_SESSION_DECOUPLE_MIRROR_MODE && sess_mode == DISP_SESSION_DIRECT_LINK_MODE){
		/*dc mirror  to dl*/
		DC_switch_to_DL_fast(sw_only);
		pgc->session_mode = sess_mode;

		MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse, pgc->session_mode, sess_mode);
		//primary_display_diagnose();
	}else if(pgc->session_mode == DISP_SESSION_DECOUPLE_MIRROR_MODE && sess_mode == DISP_SESSION_DECOUPLE_MODE){   
		/* release output buffer */
		int layer = 0;
		layer = disp_sync_get_output_timeline_id();
		mtkfb_release_layer_fence(primary_session_id, layer);
		pgc->session_mode = sess_mode;

		MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse, pgc->session_mode, sess_mode);
	}else{
		DISPERR("invalid mode switch from %s to %s\n", session_mode_spy(pgc->session_mode), session_mode_spy(sess_mode));
	}

	DISPCHECK("[DISPMODE]switch mode done, primary display is %s mode now\n", session_mode_spy(pgc->session_mode));
done:	
	if(need_lock)
		_primary_path_unlock(__func__);
	pgc->session_id = session;

	MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagEnd, pgc->session_mode, sess_mode);
	return 0;
}

int primary_display_switch_mode(int sess_mode, unsigned int session, int force)
{
	int ret;
	_primary_path_lock(__func__);
	primary_display_idlemgr_kick(__func__, 0);
	ret = __primary_display_switch_mode(sess_mode, session, 0, DISP_USER_HWC);
	_primary_path_unlock(__func__);

	return ret;
}

int primary_display_mode_switch_test(int sess_mode)
{
	_primary_protect_mode_switch();
	_primary_path_lock(__func__);
	pgc->test_mode = sess_mode;
	__primary_display_switch_mode(sess_mode, MAKE_DISP_SESSION(DISP_SESSION_PRIMARY, 0), 0, DISP_USER_TEST);
	_primary_path_unlock(__func__);
	return 0;
}

int primary_display_switch_mode_for_mmdvfs(int sess_mode, unsigned int session, int blocking)
{
	int ret = 0;
	if (disp_helper_get_option(DISP_HELPER_OPTION_DEFAULT_DECOUPLE_MODE)) {
		DISPCHECK("[mmdvfs:DISPMODE] switch to mode %d begin\n", sess_mode);
		_primary_protect_mode_switch();
		_primary_path_lock(__func__);

		if (sess_mode == DISP_SESSION_DIRECT_LINK_MODE) {
			pgc->mmdvfs_power_state = 1;
			/*not urgent, so wait next frame to switch by hwc*/
			DISPCHECK("[mmdvfs:DISPMODE]power state 1\n");
		} else if(sess_mode == DISP_SESSION_DECOUPLE_MODE) {
			pgc->mmdvfs_power_state = 0;
			DISPCHECK("[mmdvfs:DISPMODE]power state 0, display suspend %d\n",pgc->state == DISP_SLEEPED);
			if (pgc->state == DISP_ALIVE) {
				primary_display_idlemgr_kick(__func__, 0);
				ret = __primary_display_switch_mode(sess_mode, session, 0, DISP_USER_MMDVFS);
			}
		} else {
			DISPERR("invalid sess_mode request by mmdvfs[%s]\n", disp_session_mode_spy(sess_mode));
		}
		_primary_path_unlock(__func__);
		DISPCHECK("[mmdvfs:DISPMODE] switch to mode %d end\n", sess_mode);
	} else {
		DISPCHECK("mmdvfs switch request skipped\n");
	}
	return ret;
}

int primary_display_is_alive(void)
{
	unsigned int temp = 0;
	//DISPFUNC();
	_primary_path_lock(__func__);

	if(pgc->state == DISP_ALIVE)
	{
		temp = 1;
	}

	_primary_path_unlock(__func__);

	return temp;
}

int primary_display_is_sleepd(void)
{
	unsigned int temp = 0;
	//DISPFUNC();
	_primary_path_lock(__func__);

	if(pgc->state == DISP_SLEEPED)
	{
		temp = 1;
	}

	_primary_path_unlock(__func__);

	return temp;
}



int primary_display_get_width(void)
{
	if(pgc->plcm == NULL)
	{
		DISPERR("lcm handle is null\n");
		return 0;
	}

	if(pgc->plcm->params)
	{
		return pgc->plcm->params->width;
	}
	else
	{
		DISPERR("lcm_params is null!\n");
		return 0;
	}
}

int primary_display_get_height(void)
{
	if(pgc->plcm == NULL)
	{
		DISPERR("lcm handle is null\n");
		return 0;
	}

	if(pgc->plcm->params)
	{
		return pgc->plcm->params->height;
	}
	else
	{
		DISPERR("lcm_params is null!\n");
		return 0;
	}
}


int primary_display_get_original_width(void)
{
	if(pgc->plcm == NULL)
	{
		DISPERR("lcm handle is null\n");
		return 0;
	}

	if(pgc->plcm->params)
	{
		return pgc->plcm->lcm_original_width;
	}
	else
	{
		DISPERR("lcm_params is null!\n");
		return 0;
	}
}

int primary_display_get_original_height(void)
{
	if(pgc->plcm == NULL)
	{
		DISPERR("lcm handle is null\n");
		return 0;
	}

	if(pgc->plcm->params)
	{
		return pgc->plcm->lcm_original_height;
	}
	else
	{
		DISPERR("lcm_params is null!\n");
		return 0;
	}
}

int primary_display_get_bpp(void)
{
	return 32;
}

void primary_display_set_max_layer(int maxlayer)
{
	pgc->max_layer = maxlayer;
}

int primary_display_get_info(void *info)
{
#if 1
	//DISPFUNC();
	disp_session_info* dispif_info = (disp_session_info*)info;

	LCM_PARAMS *lcm_param = disp_lcm_get_params(pgc->plcm);
	if(lcm_param == NULL)
	{
		DISPCHECK("lcm_param is null\n");
		return -1;
	}

	memset((void*)dispif_info, 0, sizeof(disp_session_info));

	// TODO: modify later
	if(is_DAL_Enabled() && pgc->max_layer == 4)
		dispif_info->maxLayerNum = pgc->max_layer -1;		
	else
		dispif_info->maxLayerNum = pgc->max_layer;

	switch(lcm_param->type)
	{
		case LCM_TYPE_DBI:
			{
				dispif_info->displayType = DISP_IF_TYPE_DBI;
				dispif_info->displayMode = DISP_IF_MODE_COMMAND;
				dispif_info->isHwVsyncAvailable = 1;
				//DISPMSG("DISP Info: DBI, CMD Mode, HW Vsync enable\n");
				break;
			}
		case LCM_TYPE_DPI:
			{
				dispif_info->displayType = DISP_IF_TYPE_DPI;
				dispif_info->displayMode = DISP_IF_MODE_VIDEO;
				dispif_info->isHwVsyncAvailable = 1;				
				//DISPMSG("DISP Info: DPI, VDO Mode, HW Vsync enable\n");
				break;
			}
		case LCM_TYPE_DSI:
			{
				dispif_info->displayType = DISP_IF_TYPE_DSI0;
				if(lcm_param->dsi.mode == CMD_MODE)
				{
					dispif_info->displayMode = DISP_IF_MODE_COMMAND;
					dispif_info->isHwVsyncAvailable = 1;
					//DISPMSG("DISP Info: DSI, CMD Mode, HW Vsync enable\n");
				}
				else
				{
					dispif_info->displayMode = DISP_IF_MODE_VIDEO;
					dispif_info->isHwVsyncAvailable = 1;
					//DISPMSG("DISP Info: DSI, VDO Mode, HW Vsync enable\n");
				}

				break;
			}
		default:
			break;
	}


	dispif_info->displayFormat = DISP_IF_FORMAT_RGB888;

	dispif_info->displayWidth = primary_display_get_width();
	dispif_info->displayHeight = primary_display_get_height();

	dispif_info->physicalWidth = DISP_GetActiveWidth();
	dispif_info->physicalHeight = DISP_GetActiveHeight();

	dispif_info->vsyncFPS = pgc->lcm_fps;

	dispif_info->isConnected = 1;

#ifdef ROME_TODO
#error
	{
		LCM_PARAMS lcm_params_temp;
		memset((void*)&lcm_params_temp, 0, sizeof(lcm_params_temp));
		if(lcm_drv)
		{
			lcm_drv->get_params(&lcm_params_temp);
			dispif_info->lcmOriginalWidth = lcm_params_temp.width;
			dispif_info->lcmOriginalHeight = lcm_params_temp.height;			
			DISPMSG("DISP Info: LCM Panel Original Resolution(For DFO Only): %d x %d\n", dispif_info->lcmOriginalWidth, dispif_info->lcmOriginalHeight);
		}
		else
		{
			DISPMSG("DISP Info: Fatal Error!!, lcm_drv is null\n");
		}
	}
#endif

#endif
}

int primary_display_get_pages(void)
{
	return 3;
}


int primary_display_is_video_mode(void)
{
	// TODO: we should store the video/cmd mode in runtime, because ROME will support cmd/vdo dynamic switch
	return disp_lcm_is_video_mode(pgc->plcm);
}

int primary_display_diagnose(void)
{
	int ret = 0;
	dpmgr_check_status(pgc->dpmgr_handle);

	if(primary_display_is_decouple_mode())
	{
		if (pgc->ovl2mem_path_handle) {
			dpmgr_check_status(pgc->ovl2mem_path_handle);
		} else {
			DISPERR("display is decouple mode, but ovl2mem_path_handle is null\n");
		}
	}

	primary_display_check_path(NULL, 0);

	return ret;
}
int primary_display_is_dual_dsi(void)
{
	DISP_MODULE_ENUM dst_module = 0;
	dst_module = _get_dst_module_by_lcm(pgc->plcm);

	if(dst_module == DISP_MODULE_DSIDUAL)
	{
		return 1;
	}
	else
	{
		return 0;
	}
	//return disp_lcm_is_dual_dsi(pgc->plcm);
}

CMDQ_SWITCH primary_display_cmdq_enabled(void)
{
	return disp_helper_get_option(DISP_HELPER_OPTION_USE_CMDQ);
}

int primary_display_switch_cmdq_cpu(CMDQ_SWITCH use_cmdq)
{
	_primary_path_lock(__func__);

	primary_display_use_cmdq = use_cmdq;
	DISPCHECK("display driver use %s to config register now\n", (use_cmdq==CMDQ_ENABLE)?"CMDQ":"CPU");

	_primary_path_unlock(__func__);
	return primary_display_use_cmdq;
}

int primary_display_manual_lock(void)
{
	_primary_path_lock(__func__);
}

int primary_display_manual_unlock(void)
{
	_primary_path_unlock(__func__);
}

void primary_display_reset(void)
{
	dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
}

unsigned int primary_display_get_fps(void)
{
	unsigned int fps = 0;
	_primary_path_lock(__func__);
	fps = pgc->lcm_fps;
	_primary_path_unlock(__func__);

	return fps;
}

int primary_display_force_set_fps(unsigned int keep, unsigned int skip)
{
	int ret = 0;	
	DISPCHECK("force set fps to keep %d, skip %d\n", keep, skip);
	_primary_path_lock(__func__);

	pgc->force_fps_keep_count = keep;
	pgc->force_fps_skip_count = skip;

	g_keep = 0;
	g_skip = 0;
	_primary_path_unlock(__func__);

	return ret;
}

int primary_display_force_set_vsync_fps(unsigned int fps)
{
	int ret = 0;	
	DISPCHECK("force set fps to %d\n", fps);
	_primary_path_lock(__func__);

	if(fps == pgc->lcm_fps)
	{
		pgc->vsync_drop = 0;
		ret = 0;
	}
	else if(fps == 30)
	{	
		pgc->vsync_drop = 1;
		ret = 0;
	}
	else
	{
		ret = -1;
	}

	_primary_path_unlock(__func__);

	return ret;
}

int primary_display_enable_path_cg(int enable)
{
	int ret = 0;	
	DISPMSG("%s primary display's path cg\n", enable?"enable":"disable");
	_primary_path_lock(__func__);

	if(enable)
	{
		ret += disable_clock(MT_CG_DISP1_DSI0_ENGINE, "DSI0");
		ret += disable_clock(MT_CG_DISP1_DSI0_DIGITAL, "DSI0");
		ret += disable_clock(MT_CG_DISP0_DISP_RDMA0 , "DDP");
		ret += disable_clock(MT_CG_DISP0_DISP_OVL0 , "DDP");
		ret += disable_clock(MT_CG_DISP0_DISP_COLOR0, "DDP");
		ret += disable_clock(MT_CG_DISP0_DISP_OD , "DDP");
		ret += disable_clock(MT_CG_DISP0_DISP_UFOE , "DDP");
		ret += disable_clock(MT_CG_DISP0_DISP_AAL, "DDP");
		//ret += disable_clock(MT_CG_DISP1_DISP_PWM0_26M , "PWM");
		//ret += disable_clock(MT_CG_DISP1_DISP_PWM0_MM , "PWM");

		ret += disable_clock(MT_CG_DISP0_MUTEX_32K	 , "Debug");
		ret += disable_clock(MT_CG_DISP0_SMI_LARB0	 , "Debug");
		ret += disable_clock(MT_CG_DISP0_SMI_COMMON  , "Debug");

		ret += disable_clock(MT_CG_DISP0_MUTEX_32K   , "Debug2");
		ret += disable_clock(MT_CG_DISP0_SMI_LARB0   , "Debug2");
		ret += disable_clock(MT_CG_DISP0_SMI_COMMON  , "Debug2");

	}		
	else
	{
		ret += enable_clock(MT_CG_DISP1_DSI0_ENGINE, "DSI0");
		ret += enable_clock(MT_CG_DISP1_DSI0_DIGITAL, "DSI0");
		ret += enable_clock(MT_CG_DISP0_DISP_RDMA0 , "DDP");
		ret += enable_clock(MT_CG_DISP0_DISP_OVL0 , "DDP");
		ret += enable_clock(MT_CG_DISP0_DISP_COLOR0, "DDP");
		ret += enable_clock(MT_CG_DISP0_DISP_OD , "DDP");
		ret += enable_clock(MT_CG_DISP0_DISP_UFOE , "DDP");
		ret += enable_clock(MT_CG_DISP0_DISP_AAL, "DDP");
		//ret += enable_clock(MT_CG_DISP1_DISP_PWM0_26M, "PWM");
		//ret += enable_clock(MT_CG_DISP1_DISP_PWM0_MM, "PWM");

		ret += enable_clock(MT_CG_DISP0_MUTEX_32K	 , "Debug");
		ret += enable_clock(MT_CG_DISP0_SMI_LARB0	 , "Debug");
		ret += enable_clock(MT_CG_DISP0_SMI_COMMON  , "Debug");

		ret += enable_clock(MT_CG_DISP0_MUTEX_32K   , "Debug2");
		ret += enable_clock(MT_CG_DISP0_SMI_LARB0   , "Debug2");
		ret += enable_clock(MT_CG_DISP0_SMI_COMMON  , "Debug2");
	}

	_primary_path_unlock(__func__);

	return ret;
}

int primary_display_set_fps(int fps)
{
	int ret =0;
	static cmdqRecHandle cmdq_handle = NULL;
	if(cmdq_handle == NULL){
		ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP,&cmdq_handle);
		if(ret!=0)
		{
			DISPCHECK("fail to create primary cmdq handle for adjust fps\n");
			return -1;
		}
	}else{
		DISPCHECK("primary_display_set_fps:fps(%d)\n", fps);
		_primary_path_lock(__func__);
		cmdqRecReset(cmdq_handle);
		_cmdq_handle_clear_dirty(cmdq_handle);
		_cmdq_insert_wait_frame_done_token_mira(cmdq_handle);
		disp_lcm_adjust_fps(cmdq_handle, pgc->plcm, fps);
		_cmdq_set_config_handle_dirty_mira(cmdq_handle);
		_cmdq_flush_config_handle_mira(cmdq_handle, 1);
		_primary_path_unlock(__func__);
	}
	return 0;
}

int _set_backlight_by_cmdq(unsigned int level)
{
	MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 1, 1);
	int ret=0;
	cmdqRecHandle cmdq_handle_backlight = NULL;
	ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP,&cmdq_handle_backlight);
	DISPCHECK("primary backlight, handle=%p\n", cmdq_handle_backlight);
	if(ret!=0)
	{
		DISPCHECK("fail to create primary cmdq handle for backlight\n");
		return -1;
	}

	if(primary_display_is_video_mode())
	{	
		MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 1, 2);
		cmdqRecReset(cmdq_handle_backlight);	
		//dpmgr_path_ioctl(pgc->dpmgr_handle, cmdq_handle_backlight, DDP_BACK_LIGHT, &level);
		disp_lcm_set_backlight(pgc->plcm,cmdq_handle_backlight,level);
		_cmdq_flush_config_handle_mira(cmdq_handle_backlight, 1);		
		DISPCHECK("[BL]_set_backlight_by_cmdq ret=%d\n",ret);
	}
	else
	{	
		MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 1, 3);
		cmdqRecReset(cmdq_handle_backlight);
		_cmdq_handle_clear_dirty(cmdq_handle_backlight);
		_cmdq_insert_wait_frame_done_token_mira(cmdq_handle_backlight);
		//cmdqRecClearEventToken(cmdq_handle_backlight, CMDQ_SYNC_TOKEN_CABC_EOF);
		//dpmgr_path_ioctl(pgc->dpmgr_handle, cmdq_handle_backlight, DDP_BACK_LIGHT, &level);
		disp_lcm_set_backlight(pgc->plcm,cmdq_handle_backlight,level);
		//cmdqRecSetEventToken(cmdq_handle_backlight, CMDQ_SYNC_TOKEN_CABC_EOF);
		cmdqRecSetEventToken(cmdq_handle_backlight, CMDQ_SYNC_TOKEN_CONFIG_DIRTY);
		MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 1, 4);
		_cmdq_flush_config_handle_mira(cmdq_handle_backlight, 1);
		MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 1, 6);
		DISPCHECK("[BL]_set_backlight_by_cmdq ret=%d\n",ret);
	}	
	cmdqRecDestroy(cmdq_handle_backlight);		
	cmdq_handle_backlight = NULL;
	MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 1, 5);
	return ret;
}
int _set_backlight_by_cpu(unsigned int level)
{
	int ret = 0;

	if(disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL)
	{
		DISPMSG("%s skip due to stage %s\n", __func__, disp_helper_stage_spy());
		return 0;
	}

	MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 0, 1);
	if(primary_display_is_video_mode())
	{
		disp_lcm_set_backlight(pgc->plcm,NULL,level);
	}
	else
	{		
		DISPCHECK("[BL]display cmdq trigger loop stop[begin]\n");
		if(primary_display_cmdq_enabled())
		{
			_cmdq_stop_trigger_loop();
		}
		DISPCHECK("[BL]display cmdq trigger loop stop[end]\n");

		if(dpmgr_path_is_busy(pgc->dpmgr_handle))
		{
			DISPCHECK("[BL]primary display path is busy\n");
			ret = dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ*1);
			DISPCHECK("[BL]wait frame done ret:%d\n", ret);
		}

		DISPCHECK("[BL]stop dpmgr path[begin]\n");
		dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
		DISPCHECK("[BL]stop dpmgr path[end]\n");
		if(dpmgr_path_is_busy(pgc->dpmgr_handle))
		{
			DISPCHECK("[BL]primary display path is busy after stop\n");
			dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ*1);
			DISPCHECK("[BL]wait frame done ret:%d\n", ret);
		}
		DISPCHECK("[BL]reset display path[begin]\n");
		dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
		DISPCHECK("[BL]reset display path[end]\n");

		MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 0, 2);

		disp_lcm_set_backlight(pgc->plcm,NULL,level);

		MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 0, 3);

		DISPCHECK("[BL]start dpmgr path[begin]\n"); 			
		dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);
		DISPCHECK("[BL]start dpmgr path[end]\n");

		if(primary_display_cmdq_enabled())
		{
			DISPCHECK("[BL]start cmdq trigger loop[begin]\n");
			_cmdq_start_trigger_loop();
		}
		DISPCHECK("[BL]start cmdq trigger loop[end]\n");
	}		
	MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 0, 7);
	return ret;
}
int primary_display_setbacklight(unsigned int level)
{
	DISPFUNC();
	int ret = 0;

	if(disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL)
	{
		DISPMSG("%s skip due to stage %s\n", __func__, disp_helper_stage_spy());
		return 0;
	}

	MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagStart, 0, 0);
#ifdef DISP_SWITCH_DST_MODE
	_primary_path_switch_dst_lock();
#endif
	_primary_path_esd_check_lock();		//add by lizhiye
	_primary_path_lock(__func__);
	if(pgc->state == DISP_SLEEPED)
	{
		DISPCHECK("Sleep State set backlight invald\n");
	}
	else
	{
		if(primary_display_cmdq_enabled())	
		{	
			if(primary_display_is_video_mode())
			{
				MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 0, 7);
				disp_lcm_set_backlight(pgc->plcm,NULL,level);
			}
			else
			{
				_set_backlight_by_cmdq(level);
			}
		}
		else
		{
			_set_backlight_by_cpu(level);
		}
	}
	_primary_path_unlock(__func__);
	_primary_path_esd_check_unlock();		//add by lizhiye
#ifdef DISP_SWITCH_DST_MODE
	_primary_path_switch_dst_lock();
#endif
	MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagEnd, 0, 0);
	return ret;
}

//add begin by lizhiye
int primary_display_enable_cabc(unsigned int enable)	
{
	DISPFUNC();
	int ret = 0;
	if(enable == 1)
	{
		cabc_enable_flag = 1;
	}
	else
	{
		cabc_enable_flag = 0;
	}

	if(disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL)
	{
		DISPMSG("%s skip due to stage %s\n", __func__, disp_helper_stage_spy());
		return 0;
	}

#ifdef DISP_SWITCH_DST_MODE
	_primary_path_switch_dst_lock();
#endif
	_primary_path_esd_check_lock();		//add by lizhiye
	_primary_path_lock(__func__);
	if(pgc->state == DISP_SLEEPED)
	{
		DISPCHECK("Sleep State set cabc invald\n");
	}
	else
	{
		if(primary_display_cmdq_enabled())	
		{	
			if(primary_display_is_video_mode())
			{
				disp_lcm_enable_cabc(pgc->plcm,NULL,enable);
			}
			else
			{
				//...
			}
		}
		else
		{
			//....
		}
	}
	_primary_path_unlock(__func__);
	_primary_path_esd_check_unlock();		//add by lizhiye
#ifdef DISP_SWITCH_DST_MODE
	_primary_path_switch_dst_lock();
#endif
	return ret;
}
//add end by lizhiye


#define LLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLL

/***********************/ 
/*****Legacy DISP API*****/
/***********************/
UINT32 DISP_GetScreenWidth(void)
{
	return primary_display_get_width();
}

UINT32 DISP_GetScreenHeight(void)
{
	return primary_display_get_height();
}
UINT32 DISP_GetActiveHeight(void)
{
	if(pgc->plcm == NULL)
	{
		DISPERR("lcm handle is null\n");
		return 0;
	}

	if(pgc->plcm->params)
	{
		return pgc->plcm->params->physical_height;
	}
	else
	{
		DISPERR("lcm_params is null!\n");
		return 0;
	}
}


UINT32 DISP_GetActiveWidth(void)
{
	if(pgc->plcm == NULL)
	{
		DISPERR("lcm handle is null\n");
		return 0;
	}

	if(pgc->plcm->params)
	{
		return pgc->plcm->params->physical_width;
	}
	else
	{
		DISPERR("lcm_params is null!\n");
		return 0;
	}
}

LCM_PARAMS * DISP_GetLcmPara(void)
{
	if(pgc->plcm == NULL)
	{
		DISPERR("lcm handle is null\n");
		return NULL;
	}

	if(pgc->plcm->params)
	{
		return pgc->plcm->params;
	}
	else
		return NULL;
}

LCM_DRIVER * DISP_GetLcmDrv(void)
{

	if(pgc->plcm == NULL)
	{
		DISPERR("lcm handle is null\n");
		return NULL;
	}

	if(pgc->plcm->drv)
	{
		return pgc->plcm->drv ;
	}
	else
		return NULL;
}

int primary_display_capture_framebuffer_ovl(unsigned long pbuf, unsigned int format)
{
	unsigned int i =0;
	int ret =0;
	cmdqRecHandle cmdq_handle = NULL;
	cmdqRecHandle cmdq_wait_handle = NULL;
	disp_ddp_path_config *pconfig =NULL;
	m4u_client_t * m4uClient = NULL;
	unsigned int mva = 0;
	unsigned int w_xres = primary_display_get_width();
	unsigned int h_yres = primary_display_get_height();
	unsigned int pixel_byte = primary_display_get_bpp() / 8;	/* bpp is either 32 or 16, can not be other value */
	int buffer_size = h_yres * w_xres * pixel_byte;

	if(disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL)
	{
		DISPMSG("%s skip due to stage %s\n", __func__, disp_helper_stage_spy());
		return 0;
	}

	DISPCHECK("primary capture: begin\n");

	disp_sw_mutex_lock(&(pgc->capture_lock));

	if (primary_display_is_sleepd()|| !primary_display_cmdq_enabled())
	{
		memset(pbuf, 0,buffer_size);
		DISPMSG("primary capture: Fail black End\n");
		goto out;
	}

	m4uClient = m4u_create_client();
	if(m4uClient == NULL)
	{
		DISPCHECK("primary capture:Fail to alloc  m4uClient\n");
		ret = -1;
		goto out;
	}

	ret = m4u_alloc_mva(m4uClient,M4U_PORT_DISP_WDMA0, pbuf, NULL,buffer_size,M4U_PROT_READ | M4U_PROT_WRITE,0,&mva);
	if(ret !=0 )
	{
		DISPCHECK("primary capture:Fail to allocate mva\n");
		ret = -1;
		goto out;
	}

	ret = m4u_cache_sync(m4uClient, M4U_PORT_DISP_WDMA0, pbuf, buffer_size, mva, M4U_CACHE_FLUSH_BY_RANGE);
	if(ret !=0 )
	{
		DISPCHECK("primary capture:Fail to cach sync\n");
		ret = -1;
		goto out;
	}

	if(primary_display_cmdq_enabled())
	{
		/*create config thread*/
		ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP,&cmdq_handle);
		if(ret!=0)
		{
			DISPCHECK("primary capture:Fail to create primary cmdq handle for capture\n");
			ret = -1;
			goto out;
		}
		cmdqRecReset(cmdq_handle);

		/*create wait thread*/
		ret = cmdqRecCreate(CMDQ_SCENARIO_DISP_SCREEN_CAPTURE,&cmdq_wait_handle);
		if(ret!=0)
		{
			DISPCHECK("primary capture:Fail to create primary cmdq wait handle for capture\n");
			ret = -1;
			goto out;
		}        
		cmdqRecReset(cmdq_wait_handle);

		/*configure  config thread*/
		_cmdq_insert_wait_frame_done_token_mira(cmdq_handle);
		dpmgr_path_memout_clock(pgc->dpmgr_handle, 1);

		_primary_path_lock(__func__);		
		pconfig = dpmgr_path_get_last_config(pgc->dpmgr_handle);
		pconfig->wdma_dirty 					= 1;
		pconfig->wdma_config.dstAddress 		= mva;
		pconfig->wdma_config.srcHeight			= h_yres;
		pconfig->wdma_config.srcWidth			= w_xres;
		pconfig->wdma_config.clipX				= 0;
		pconfig->wdma_config.clipY				= 0;
		pconfig->wdma_config.clipHeight 		= h_yres;
		pconfig->wdma_config.clipWidth			= w_xres;
		pconfig->wdma_config.outputFormat		= format; 
		pconfig->wdma_config.useSpecifiedAlpha	= 1;
		pconfig->wdma_config.alpha				= 0xFF;
		pconfig->wdma_config.dstPitch			= w_xres * DP_COLOR_BITS_PER_PIXEL(format)/8;
		dpmgr_path_add_memout(pgc->dpmgr_handle, ENGINE_OVL0, cmdq_handle);
		ret = dpmgr_path_config(pgc->dpmgr_handle, pconfig, cmdq_handle);
		pconfig->wdma_dirty  = 0;
		_primary_path_unlock(__func__);
		_cmdq_set_config_handle_dirty_mira(cmdq_handle);
		_cmdq_flush_config_handle_mira(cmdq_handle, 0);
		DISPMSG("primary capture:Flush add memout mva(0x%x)\n",mva);

		/*wait wdma0 sof*/
		cmdqRecWait(cmdq_wait_handle,CMDQ_EVENT_DISP_WDMA0_SOF);
		cmdqRecFlush(cmdq_wait_handle);
		DISPMSG("primary capture:Flush wait wdma sof\n");

		cmdqRecReset(cmdq_handle);
		_cmdq_insert_wait_frame_done_token_mira(cmdq_handle);
		_primary_path_lock(__func__);
		dpmgr_path_remove_memout(pgc->dpmgr_handle, cmdq_handle);
		_primary_path_unlock(__func__);
		cmdqRecClearEventToken(cmdq_handle,CMDQ_EVENT_DISP_WDMA0_SOF);
		_cmdq_set_config_handle_dirty_mira(cmdq_handle);
		//flush remove memory to cmdq
		_cmdq_flush_config_handle_mira(cmdq_handle, 1);
		DISPMSG("primary capture: Flush remove memout\n");

		dpmgr_path_memout_clock(pgc->dpmgr_handle, 0);
	}

out:
	cmdqRecDestroy(cmdq_handle);
	cmdqRecDestroy(cmdq_wait_handle);
	if(mva > 0) 
		m4u_dealloc_mva(m4uClient,M4U_PORT_DISP_WDMA0,mva);

	if(m4uClient != 0)
		m4u_destroy_client(m4uClient);

	disp_sw_mutex_unlock(&(pgc->capture_lock));
	DISPCHECK("primary capture: end\n");

	return ret;
}

int primary_display_capture_framebuffer(unsigned long pbuf)
{
#if 1
	unsigned int fb_layer_id = primary_display_get_option("FB_LAYER");
	unsigned int w_xres = primary_display_get_width();
	unsigned int h_yres = primary_display_get_height();
	unsigned int pixel_bpp = primary_display_get_bpp() / 8; // bpp is either 32 or 16, can not be other value
	unsigned int w_fb = ALIGN_TO(w_xres, MTK_FB_ALIGNMENT);
	unsigned int fbsize = w_fb * h_yres * pixel_bpp; // frame buffer size
	unsigned int fbaddress = dpmgr_path_get_last_config(pgc->dpmgr_handle)->ovl_config[fb_layer_id].addr;
	unsigned int mem_off_x = 0;
	unsigned int mem_off_y = 0;
	void* fbv = 0;
	DISPMSG("w_res=%d, h_yres=%d, pixel_bpp=%d, w_fb=%d, fbsize=%d, fbaddress=0x%08x\n", w_xres, h_yres, pixel_bpp, w_fb, fbsize, fbaddress);
	fbv = ioremap(fbaddress, fbsize);
	DISPMSG("w_xres = %d, h_yres = %d, w_fb = %d, pixel_bpp = %d, fbsize = %d, fbaddress = 0x%08x\n", w_xres, h_yres, w_fb, pixel_bpp, fbsize, fbaddress);
	if (!fbv)
	{
		DISPMSG("[FB Driver], Unable to allocate memory for frame buffer: address=0x%08x, size=0x%08x\n", \
				fbaddress, fbsize);
		return -1;
	}

	unsigned int i;
	unsigned long ttt = get_current_time_us();
	for(i = 0;i < h_yres; i++)
	{
		//DISPMSG("i=%d, dst=0x%08x,src=%08x\n", i, (pbuf + i * w_xres * pixel_bpp), (fbv + i * w_fb * pixel_bpp));
		memcpy((void *)(pbuf + i * w_xres * pixel_bpp), (void *)(fbv + i * w_fb * pixel_bpp), w_xres * pixel_bpp);
	}
	DISPMSG("capture framebuffer cost %ldus\n", get_current_time_us() - ttt);
	iounmap((void *)fbv);
#endif
	return -1;
}

int primary_display_freeze(void)
{
#if 0
	DISPCHECK("primary_display_freeze\n");
	primary_display_esd_check_enable(0);
	if(disp_helper_get_option(DISP_HELPER_OPTION_DEFAULT_DECOUPLE_MODE)) {
		_primary_path_lock(__func__);
		__primary_display_switch_mode(DISP_SESSION_DIRECT_LINK_MODE, MAKE_DISP_SESSION(DISP_SESSION_PRIMARY,0), 0, DISP_USER_FORCE);
		_primary_path_unlock(__func__);
	}
#endif
	return 0;
}

static UINT32 disp_fb_bpp = 32;
static UINT32 disp_fb_pages = 4;    //three for framebuffer, one for dim layer

UINT32 DISP_GetScreenBpp(void)
{
	return disp_fb_bpp; 
}

UINT32 DISP_GetPages(void)
{
	return disp_fb_pages;
}

UINT32 DISP_GetFBRamSize(void)
{
	return ALIGN_TO(DISP_GetScreenWidth(), MTK_FB_ALIGNMENT) * 
		ALIGN_TO(DISP_GetScreenHeight(), MTK_FB_ALIGNMENT) * 
		((DISP_GetScreenBpp() + 7) >> 3) * 
		DISP_GetPages();
}


UINT32 DISP_GetVRamSize(void)
{
#if 0
	// Use a local static variable to cache the calculated vram size
	//    
	static UINT32 vramSize = 0;

	if (0 == vramSize)
	{
		disp_drv_init_context();

		///get framebuffer size
		vramSize = DISP_GetFBRamSize();

		///get DXI working buffer size
		vramSize += disp_if_drv->get_working_buffer_size();

		// get assertion layer buffer size
		vramSize += DAL_GetLayerSize();

		// Align vramSize to 1MB
		//
		vramSize = ALIGN_TO_POW_OF_2(vramSize, 0x100000);

		DISP_LOG("DISP_GetVRamSize: %u bytes\n", vramSize);
	}

	return vramSize;
#endif
	return 0;
}
extern char* saved_command_line;

UINT32 DISP_GetVRamSizeBoot(char *cmdline)
{
	extern vramsize;
	extern void _parse_tag_videolfb(void);
	_parse_tag_videolfb();
	if (vramsize == 0)
		vramsize = 0x3000000;
	DISPCHECK("[DT]display vram size = 0x%08x|%d\n", vramsize, vramsize);
	return vramsize;
}


struct sg_table table;

int disp_hal_allocate_framebuffer(phys_addr_t pa_start, phys_addr_t pa_end, unsigned long* va,
		unsigned long* mva)
{
	int ret = 0;
	printk("disphal_allocate_fb, pa=%pa\n", &pa_start);
	*va = (unsigned long) ioremap_nocache(pa_start, pa_end - pa_start + 1);
	printk("disphal_allocate_fb, pa=%pa, va=0x%lx\n", &pa_start, *va);

	if (disp_helper_get_option(DISP_HELPER_OPTION_USE_M4U)) {
		m4u_client_t *client;

		struct sg_table *sg_table = &table;		
		sg_alloc_table(sg_table, 1, GFP_KERNEL);

		sg_dma_address(sg_table->sgl) = pa_start;
		sg_dma_len(sg_table->sgl) = (pa_end - pa_start + 1);
		client = m4u_create_client();
		if (IS_ERR_OR_NULL(client)) {
			DISPCHECK("create client fail!\n");
		}

		*mva = pa_start & 0xffffffffULL;
		ret = m4u_alloc_mva(client, M4U_PORT_DISP_OVL0, 0, sg_table, (pa_end - pa_start + 1), M4U_PROT_READ |M4U_PROT_WRITE, M4U_FLAGS_FIX_MVA, mva);
		//m4u_alloc_mva(M4U_PORT_DISP_OVL0, pa_start, (pa_end - pa_start + 1), 0, 0, mva);
		if(ret)
		{
			DISPCHECK("m4u_alloc_mva returns fail: %d\n", ret);
		}
		printk("[DISPHAL] FB MVA is 0x%lx PA is %pa\n", *mva, &pa_start);

	} else {
		*mva = pa_start & 0xffffffffULL;
	}

	return 0;
}

int primary_display_remap_irq_event_map(void)
{
	return 0;
}

unsigned int primary_display_get_option(const char* option)
{
	if(!strcmp(option, "FB_LAYER"))
		return 0;
	if(!strcmp(option, "ASSERT_LAYER"))
		return 3;
	if (!strcmp(option, "M4U_ENABLE"))
		return disp_helper_get_option(DISP_HELPER_OPTION_USE_M4U);

	ASSERT(0);
	return 0;
}

int primary_display_get_debug_info(char *buf)
{
	// resolution
	// cmd/video mode
	// display path
	// dsi data rate/lane number/state
	// primary path trigger count
	// frame done count
	// suspend/resume count
	// current fps 10s/5s/1s
	// error count and message
	// current state of each module on the path
	return 0;
}

#include "ddp_reg.h"

#define IS_READY(x)	((x)?"READY\t":"Not READY")
#define IS_VALID(x)	((x)?"VALID\t":"Not VALID")

#define READY_BIT0(x) ((DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b8) & (1 << x)))
#define VALID_BIT0(x) ((DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b0) & (1 << x)))

#define READY_BIT1(x) ((DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8bc) & (1 << x)))
#define VALID_BIT1(x) ((DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b4) & (1 << x)))

int primary_display_check_path(char* stringbuf, int buf_len)
{
	int len = 0;
	if (stringbuf) {
		len +=
			scnprintf(stringbuf + len, buf_len - len,
					"|--------------------------------------------------------------------------------------|\n");

		len +=
			scnprintf(stringbuf + len, buf_len - len,
					"READY0=0x%08x, READY1=0x%08x, VALID0=0x%08x, VALID1=0x%08x\n",
					DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b8),
					DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8bC),
					DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b0),
					DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b4));
		len +=
			scnprintf(stringbuf + len, buf_len - len, "OVL0\t\t\t%s\t%s\n",
					IS_READY(READY_BIT0(DDP_SIGNAL_OVL0__OVL0_MOUT)),
					IS_VALID(VALID_BIT0(DDP_SIGNAL_OVL0__OVL0_MOUT)));
		len +=
			scnprintf(stringbuf + len, buf_len - len, "OVL0_MOUT:\t\t%s\t%s\n",
					IS_READY(READY_BIT1(DDP_SIGNAL_OVL0_MOUT0__COLOR0_SEL1)),
					IS_VALID(VALID_BIT1(DDP_SIGNAL_OVL0_MOUT0__COLOR0_SEL1)));
		len +=
			scnprintf(stringbuf + len, buf_len - len, "COLOR0_SEL:\t\t%s\t%s\n",
					IS_READY(READY_BIT0(DDP_SIGNAL_COLOR0_SEL__COLOR0)),
					IS_VALID(VALID_BIT0(DDP_SIGNAL_COLOR0_SEL__COLOR0)));
		len +=
			scnprintf(stringbuf + len, buf_len - len, "COLOR0:\t\t\t%s\t%s\n",
					IS_READY(READY_BIT0(DDP_SIGNAL_COLOR0__COLOR0_SOUT)),
					IS_VALID(VALID_BIT0(DDP_SIGNAL_COLOR0__COLOR0_SOUT)));
		len +=
			scnprintf(stringbuf + len, buf_len - len, "COLOR0_SOUT:\t\t%s\t%s\n",
					IS_READY(READY_BIT0(DDP_SIGNAL_COLOR0_SOUT0__AAL_SEL0)),
					IS_VALID(VALID_BIT0(DDP_SIGNAL_COLOR0_SOUT0__AAL_SEL0)));
		len +=
			scnprintf(stringbuf + len, buf_len - len, "AAL_SEL:\t\t%s\t%s\n",
					IS_READY(READY_BIT0(DDP_SIGNAL_AAL_SEL__AAL0)),
					IS_VALID(VALID_BIT0(DDP_SIGNAL_AAL_SEL__AAL0)));
		len +=
			scnprintf(stringbuf + len, buf_len - len, "AAL0:\t\t\t%s\t%s\n",
					IS_READY(READY_BIT0(DDP_SIGNAL_AAL0__OD)),
					IS_VALID(VALID_BIT0(DDP_SIGNAL_AAL0__OD)));
		len +=
			scnprintf(stringbuf + len, buf_len - len, "OD:\t\t\t%s\t%s\n",
					IS_READY(READY_BIT1(DDP_SIGNAL_OD__OD_MOUT)),
					IS_VALID(VALID_BIT1(DDP_SIGNAL_OD__OD_MOUT)));
		len +=
			scnprintf(stringbuf + len, buf_len - len, "OD_MOUT:\t\t%s\t%s\n",
					IS_READY(READY_BIT1(DDP_SIGNAL_OD_MOUT0__RDMA0)),
					IS_VALID(VALID_BIT1(DDP_SIGNAL_OD_MOUT0__RDMA0)));
		len +=
			scnprintf(stringbuf + len, buf_len - len, "RDMA0:\t\t\t%s\t%s\n",
					IS_READY(READY_BIT1(DDP_SIGNAL_RDMA0__RDMA0_SOUT)),
					IS_VALID(VALID_BIT1(DDP_SIGNAL_RDMA0__RDMA0_SOUT)));
		len +=
			scnprintf(stringbuf + len, buf_len - len, "RDMA0_SOUT:\t\t%s\t%s\n",
					IS_READY(READY_BIT1(DDP_SIGNAL_RDMA0_SOUT0__PATH0_SEL0)),
					IS_VALID(VALID_BIT1(DDP_SIGNAL_RDMA0_SOUT0__PATH0_SEL0)));
		len +=
			scnprintf(stringbuf + len, buf_len - len, "PATH0_SEL:\t\t%s\t%s\n",
					IS_READY(READY_BIT0(DDP_SIGNAL_PATH0_SEL__PATH0_SOUT)),
					IS_VALID(VALID_BIT0(DDP_SIGNAL_PATH0_SEL__PATH0_SOUT)));
		len +=
			scnprintf(stringbuf + len, buf_len - len, "PATH0_SOUT:\t\t%s\t%s\n",
					IS_READY(READY_BIT0(DDP_SIGNAL_PATH0_SOUT0__UFOE_SEL0)),
					IS_VALID(VALID_BIT0(DDP_SIGNAL_PATH0_SOUT0__UFOE_SEL0)));
		len +=
			scnprintf(stringbuf + len, buf_len - len, "UFOE:\t\t\t%s\t%s\n",
					IS_READY(READY_BIT0(DDP_SIGNAL_UFOE_SEL__UFOE0)),
					IS_VALID(VALID_BIT0(DDP_SIGNAL_UFOE_SEL__UFOE0)));
		len +=
			scnprintf(stringbuf + len, buf_len - len, "UFOE_MOUT:\t\t%s\t%s\n",
					IS_READY(READY_BIT0(DDP_SIGNAL_UFOE0__UFOE_MOUT)),
					IS_VALID(VALID_BIT0(DDP_SIGNAL_UFOE0__UFOE_MOUT)));
		len +=
			scnprintf(stringbuf + len, buf_len - len, "DSI0_SEL:\t\t%s\t%s\n",
					IS_READY(READY_BIT1(DDP_SIGNAL_UFOE_MOUT0__DSI0_SEL0)),
					IS_VALID(VALID_BIT1(DDP_SIGNAL_UFOE_MOUT0__DSI0_SEL0)));
		len +=
			scnprintf(stringbuf + len, buf_len - len, "DSI0:\t\t\t%s\t%s\n",
					IS_READY(READY_BIT0(DDP_SIGNAL_DSI0_SEL__DSI0)),
					IS_VALID(VALID_BIT0(DDP_SIGNAL_DSI0_SEL__DSI0)));
	} else {
		DISPMSG
			("|--------------------------------------------------------------------------------------|\n");

		DISPMSG("READY0=0x%08x, READY1=0x%08x, VALID0=0x%08x, VALID1=0x%08x\n",
				DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b8),
				DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8bC),
				DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b0),
				DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b4));
		DISPMSG("OVL0\t\t\t%s\t%s\n", IS_READY(READY_BIT0(DDP_SIGNAL_OVL0__OVL0_MOUT)),
				IS_VALID(VALID_BIT0(DDP_SIGNAL_OVL0__OVL0_MOUT)));
		DISPMSG("OVL0_MOUT:\t\t%s\t%s\n",
				IS_READY(READY_BIT1(DDP_SIGNAL_OVL0_MOUT0__COLOR0_SEL1)),
				IS_VALID(VALID_BIT1(DDP_SIGNAL_OVL0_MOUT0__COLOR0_SEL1)));
		DISPMSG("COLOR0_SEL:\t\t%s\t%s\n",
				IS_READY(READY_BIT0(DDP_SIGNAL_COLOR0_SEL__COLOR0)),
				IS_VALID(VALID_BIT0(DDP_SIGNAL_COLOR0_SEL__COLOR0)));
		DISPMSG("COLOR0:\t\t\t%s\t%s\n",
				IS_READY(READY_BIT0(DDP_SIGNAL_COLOR0__COLOR0_SOUT)),
				IS_VALID(VALID_BIT0(DDP_SIGNAL_COLOR0__COLOR0_SOUT)));
		DISPMSG("COLOR0_SOUT:\t\t%s\t%s\n",
				IS_READY(READY_BIT0(DDP_SIGNAL_COLOR0_SOUT0__AAL_SEL0)),
				IS_VALID(VALID_BIT0(DDP_SIGNAL_COLOR0_SOUT0__AAL_SEL0)));
		DISPMSG("AAL_SEL:\t\t%s\t%s\n", IS_READY(READY_BIT0(DDP_SIGNAL_AAL_SEL__AAL0)),
				IS_VALID(VALID_BIT0(DDP_SIGNAL_AAL_SEL__AAL0)));
		DISPMSG("AAL0:\t\t\t%s\t%s\n", IS_READY(READY_BIT0(DDP_SIGNAL_AAL0__OD)),
				IS_VALID(VALID_BIT0(DDP_SIGNAL_AAL0__OD)));
		DISPMSG("OD:\t\t\t%s\t%s\n", IS_READY(READY_BIT1(DDP_SIGNAL_OD__OD_MOUT)),
				IS_VALID(VALID_BIT1(DDP_SIGNAL_OD__OD_MOUT)));
		DISPMSG("OD_MOUT:\t\t%s\t%s\n", IS_READY(READY_BIT1(DDP_SIGNAL_OD_MOUT0__RDMA0)),
				IS_VALID(VALID_BIT1(DDP_SIGNAL_OD_MOUT0__RDMA0)));
		DISPMSG("RDMA0:\t\t\t%s\t%s\n", IS_READY(READY_BIT1(DDP_SIGNAL_RDMA0__RDMA0_SOUT)),
				IS_VALID(VALID_BIT1(DDP_SIGNAL_RDMA0__RDMA0_SOUT)));
		DISPMSG("RDMA0_SOUT:\t\t%s\t%s\n",
				IS_READY(READY_BIT1(DDP_SIGNAL_RDMA0_SOUT0__PATH0_SEL0)),
				IS_VALID(VALID_BIT1(DDP_SIGNAL_RDMA0_SOUT0__PATH0_SEL0)));
		DISPMSG("PATH0_SEL:\t\t%s\t%s\n",
				IS_READY(READY_BIT0(DDP_SIGNAL_PATH0_SEL__PATH0_SOUT)),
				IS_VALID(VALID_BIT0(DDP_SIGNAL_PATH0_SEL__PATH0_SOUT)));
		DISPMSG("PATH0_SOUT:\t\t%s\t%s\n",
				IS_READY(READY_BIT0(DDP_SIGNAL_PATH0_SOUT0__UFOE_SEL0)),
				IS_VALID(VALID_BIT0(DDP_SIGNAL_PATH0_SOUT0__UFOE_SEL0)));
		DISPMSG("UFOE:\t\t\t%s\t%s\n", IS_READY(READY_BIT0(DDP_SIGNAL_UFOE_SEL__UFOE0)),
				IS_VALID(VALID_BIT0(DDP_SIGNAL_UFOE_SEL__UFOE0)));
		DISPMSG("UFOE_MOUT:\t\t%s\t%s\n", IS_READY(READY_BIT0(DDP_SIGNAL_UFOE0__UFOE_MOUT)),
				IS_VALID(VALID_BIT0(DDP_SIGNAL_UFOE0__UFOE_MOUT)));
		DISPMSG("DSI0_SEL:\t\t%s\t%s\n",
				IS_READY(READY_BIT1(DDP_SIGNAL_UFOE_MOUT0__DSI0_SEL0)),
				IS_VALID(VALID_BIT1(DDP_SIGNAL_UFOE_MOUT0__DSI0_SEL0)));
		DISPMSG("DSI0:\t\t\t%s\t%s\n", IS_READY(READY_BIT0(DDP_SIGNAL_DSI0_SEL__DSI0)),
				IS_VALID(VALID_BIT0(DDP_SIGNAL_DSI0_SEL__DSI0)));
	}

	return len;
}

int primary_display_lcm_ATA(void)
{
	DISP_STATUS ret = DISP_STATUS_OK;

	DISPFUNC();
	_primary_path_lock(__func__);
	if (pgc->state == 0) {
		DISPCHECK("ATA_LCM, primary display path is already sleep, skip\n");
		goto done;
	}

	DISPCHECK("[ATA_LCM]primary display path stop[begin]\n");
	if (primary_display_is_video_mode()) {
		dpmgr_path_ioctl(pgc->dpmgr_handle, NULL, DDP_STOP_VIDEO_MODE, NULL);
	}
	DISPCHECK("[ATA_LCM]primary display path stop[end]\n");
	ret = disp_lcm_ATA(pgc->plcm);
	dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);
	if(primary_display_is_video_mode()){
		// for video mode, we need to force trigger here
		// for cmd mode, just set DPREC_EVENT_CMDQ_SET_EVENT_ALLOW when trigger loop start
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, CMDQ_DISABLE);
	}
done:
	_primary_path_unlock(__func__);
	return ret;
}

int fbconfig_get_esd_check_test(UINT32 dsi_id,UINT32 cmd,UINT8*buffer,UINT32 num)
{
	int ret = 0;
	_primary_path_lock(__func__);

	if (pgc->state == DISP_SLEEPED) {
		DISPCHECK("[ESD]primary display path is sleeped?? -- skip esd check\n");
		_primary_path_unlock(__func__);
		goto done;
	}
	primary_display_esd_check_enable(0);
	/// 1: stop path
	_cmdq_stop_trigger_loop();
	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		DISPCHECK("[ESD]wait frame done ret:%d\n", ret);
	}
	dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("[ESD]stop dpmgr path[end]\n");
	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		DISPCHECK("[ESD]wait frame done ret:%d\n", ret);
	}
	dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
	extern int fbconfig_get_esd_check(DSI_INDEX dsi_id,UINT32 cmd,UINT8*buffer,UINT32 num);
	ret=fbconfig_get_esd_check(dsi_id,cmd,buffer,num);
	dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("[ESD]start dpmgr path[end]\n");
	if (primary_display_is_video_mode()) {
		/* for video mode, we need to force trigger here */
		/* for cmd mode, just set DPREC_EVENT_CMDQ_SET_EVENT_ALLOW when trigger loop start */
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, CMDQ_DISABLE);
	}
	_cmdq_start_trigger_loop();
	DISPCHECK("[ESD]start cmdq trigger loop[end]\n");
	primary_display_esd_check_enable(1);
	_primary_path_unlock(__func__);

done:
	return ret;
}
int Panel_Master_dsi_config_entry(const char * name,void *config_value)
{
	int ret = 0;
	int force_trigger_path=0;
	UINT32* config_dsi  = (UINT32 *)config_value;
	DISPFUNC();
	LCM_PARAMS *lcm_param = NULL;
	LCM_DRIVER *pLcm_drv=DISP_GetLcmDrv();
	int	esd_check_backup=atomic_read(&esd_check_task_wakeup);
	if(!strcmp(name, "DRIVER_IC_RESET") || !strcmp(name, "PM_DDIC_CONFIG"))	
	{	
		primary_display_esd_check_enable(0);
		msleep(2500);
	}
	_primary_path_lock(__func__);		

	lcm_param = disp_lcm_get_params(pgc->plcm);
	if(pgc->state == DISP_SLEEPED)
	{
		DISPERR("[Pmaster]Panel_Master: primary display path is sleeped??\n");
		goto done;
	}
	/// Esd Check : Read from lcm
	/// the following code is to
	/// 0: lock path
	/// 1: stop path
	/// 2: do esd check (!!!)
	/// 3: start path
	/// 4: unlock path
	/// 1: stop path
	_cmdq_stop_trigger_loop();

	if(dpmgr_path_is_busy(pgc->dpmgr_handle))
	{
		DISPCHECK("[ESD]wait frame done ret:%d\n", ret);
	}

	dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("[ESD]stop dpmgr path[end]\n");

	if(dpmgr_path_is_busy(pgc->dpmgr_handle))
	{
		DISPCHECK("[ESD]wait frame done ret:%d\n", ret);
	}
	dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
	if((!strcmp(name, "PM_CLK"))||(!strcmp(name, "PM_SSC")))
		Panel_Master_primary_display_config_dsi(name,*config_dsi);
	else if(!strcmp(name, "PM_DDIC_CONFIG"))
	{
		Panel_Master_DDIC_config();	
		force_trigger_path=1;
	}else if(!strcmp(name, "DRIVER_IC_RESET"))
	{
		if(pLcm_drv&&pLcm_drv->init_power)
			pLcm_drv->init_power();	
		if(pLcm_drv)
			pLcm_drv->init();		
		else
			ret=-1;	
		force_trigger_path=1;
	}		
	dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);	
	if(primary_display_is_video_mode())
	{
		// for video mode, we need to force trigger here
		// for cmd mode, just set DPREC_EVENT_CMDQ_SET_EVENT_ALLOW when trigger loop start
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, CMDQ_DISABLE);
		force_trigger_path=0;	
	}
	_cmdq_start_trigger_loop();
	DISPCHECK("[Pmaster]start cmdq trigger loop\n");	
done:
	_primary_path_unlock(__func__);

	if(force_trigger_path)		//command mode only
	{
		primary_display_trigger(0,NULL,0);
		DISPCHECK("[Pmaster]force trigger display path\r\n");	
	}
	atomic_set(&esd_check_task_wakeup, esd_check_backup);

	return ret;
}

/*
mode: 0, switch to cmd mode; 1, switch to vdo mode
*/
int primary_display_switch_dst_mode(int mode)
{
	DISP_STATUS ret = DISP_STATUS_ERROR;
#ifdef DISP_SWITCH_DST_MODE
	void* lcm_cmd = NULL;
	DISPFUNC();
	_primary_path_switch_dst_lock();
	disp_sw_mutex_lock(&(pgc->capture_lock));
	if(pgc->plcm->params->type != LCM_TYPE_DSI)
	{
		printk("[primary_display_switch_dst_mode] Error, only support DSI IF\n");
		goto done;
	}
	if(pgc->state == DISP_SLEEPED)
	{
		DISPCHECK("[primary_display_switch_dst_mode], primary display path is already sleep, skip\n");
		goto done;
	}

	if(mode == primary_display_cur_dst_mode){
		DISPCHECK("[primary_display_switch_dst_mode]not need switch,cur_mode:%d, switch_mode:%d\n",primary_display_cur_dst_mode,mode);
		goto done;
	}
	//	DISPCHECK("[primary_display_switch_mode]need switch,cur_mode:%d, switch_mode:%d\n",primary_display_cur_dst_mode,mode);
	lcm_cmd = disp_lcm_switch_mode(pgc->plcm,mode);
	if(lcm_cmd == NULL) 
	{
		DISPCHECK("[primary_display_switch_dst_mode]get lcm cmd fail\n",primary_display_cur_dst_mode,mode);
		goto done;
	}
	else
	{
		int temp_mode = 0;
		if(0 != dpmgr_path_ioctl(pgc->dpmgr_handle, pgc->cmdq_handle_config, DDP_SWITCH_LCM_MODE, lcm_cmd))
		{
			printk("switch lcm mode fail, return directly\n");
			goto done;
		}
		_primary_path_lock(__func__);
		temp_mode = (int)(pgc->plcm->params->dsi.mode);
		pgc->plcm->params->dsi.mode = pgc->plcm->params->dsi.switch_mode;
		pgc->plcm->params->dsi.switch_mode = temp_mode;
		dpmgr_path_set_video_mode(pgc->dpmgr_handle, primary_display_is_video_mode());
		if(0 != dpmgr_path_ioctl(pgc->dpmgr_handle, pgc->cmdq_handle_config, DDP_SWITCH_DSI_MODE, lcm_cmd))
		{
			printk("switch dsi mode fail, return directly\n");
			_primary_path_unlock(__func__);
			goto done;
		}
	}
	primary_display_sodi_rule_init();
	_cmdq_stop_trigger_loop();
	_cmdq_build_trigger_loop();
	_cmdq_start_trigger_loop();
	_cmdq_reset_config_handle();//must do this
	_cmdq_handle_clear_dirty(pgc->cmdq_handle_config);
	_cmdq_insert_wait_frame_done_token();

	primary_display_cur_dst_mode = mode;

	if(primary_display_is_video_mode())
	{
		dpmgr_map_event_to_irq(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC, DDP_IRQ_RDMA0_DONE);
	}
	else
	{
		dpmgr_map_event_to_irq(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC, DDP_IRQ_DSI0_EXT_TE);
	}
	_primary_path_unlock(__func__);
	ret = DISP_STATUS_OK;
done:
	//	dprec_handle_option(0x0);
	disp_sw_mutex_unlock(&(pgc->capture_lock)); 
	_primary_path_switch_dst_unlock();
#else
	DISPCHECK("[ERROR: primary_display_switch_dst_mode]this function not enable in disp driver\n");
#endif
	return ret;
}

int primary_display_cmdq_set_reg(unsigned int addr, unsigned int val)
{
	int ret =0;
	cmdqRecHandle handle = NULL;

	DISPMSG("primary_display_cmdq_set_reg, addr:0x%08x, val:0x%08x\n", addr, val);

	ret = cmdqRecCreate(CMDQ_SCENARIO_DISP_SCREEN_CAPTURE,&handle);
	cmdqRecReset(handle);
	_cmdq_insert_wait_frame_done_token_mira(handle);		        

	cmdqRecWrite(handle, addr&0x1fffffff, val, ~0);
	cmdqRecFlushAsync(handle);
	DISPMSG("primary_display_cmdq_set_reg, cmdq flush done\n");

	cmdqRecDestroy(handle);

	return 0;
}

int primary_display_switch_esd_mode(int mode)
{
	DISPFUNC();
	int ret=0;
	int gpio_mode=0;


	if(pgc->plcm->params->dsi.customization_esd_check_enable!=0)
		return;

	DISPMSG("switch esd mode to %d\n", mode);

#ifdef GPIO_DSI_TE_PIN
	gpio_mode = mt_get_gpio_mode(GPIO_DSI_TE_PIN);
	//DISPMSG("[ESD]gpio_mode=%d\n",gpio_mode);
#endif
	if(mode==1)
	{
		//switch to vdo mode
		if(gpio_mode==GPIO_DSI_TE_PIN_M_DSI_TE)
		{
			//if(_need_register_eint())
			{   
				//DISPMSG("[ESD]switch video mode\n");
				struct device_node *node;
				int irq;
				u32 ints[2]={0, 0};
#ifdef GPIO_DSI_TE_PIN
				// 1.set GPIO107 eint mode	
				mt_set_gpio_mode(GPIO_DSI_TE_PIN, GPIO_DSI_TE_PIN_M_GPIO);
#endif
				// 2.register eint
				node = of_find_compatible_node(NULL, NULL, "mediatek, DSI_TE_1-eint");
				if(node)
				{   
					//DISPMSG("node 0x%x\n", node);
					of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
					mt_gpio_set_debounce(ints[0], ints[1]);
					irq = irq_of_parse_and_map(node, 0);
					if(request_irq(irq, _esd_check_ext_te_irq_handler, IRQF_TRIGGER_NONE, "DSI_TE_1-eint", NULL))
					{
						DISPERR("[ESD]EINT IRQ LINE NOT AVAILABLE!!\n");
					}
				}
				else
				{
					DISPERR("[ESD][%s] can't find DSI_TE_1 eint compatible node\n",__func__);
				}
			}
		}
	}
	else if(mode==0)
	{
		//switch to cmd mode
		if(gpio_mode==GPIO_DSI_TE_PIN_M_GPIO)
		{
			struct device_node *node;
			int irq;
			//DISPMSG("[ESD]switch cmd mode\n");

			//unregister eint 
			node = of_find_compatible_node(NULL, NULL, "mediatek, DSI_TE_1-eint");
			//DISPMSG("node 0x%x\n", node);
			if(node)
			{	
				irq = irq_of_parse_and_map(node, 0);
				free_irq(irq, NULL);
			}	
			// set GPIO107 DSI TE mode	
			mt_set_gpio_mode(GPIO_DSI_TE_PIN, GPIO_DSI_TE_PIN_M_DSI_TE);
		}
	}

	//DISPMSG("primary_display_switch_esd_mode end\n");
	return ret;
}

