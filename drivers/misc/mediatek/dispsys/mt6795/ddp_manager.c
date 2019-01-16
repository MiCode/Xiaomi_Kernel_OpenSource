#define LOG_TAG "ddp_manager"

#include <linux/slab.h>
#include <linux/mutex.h>

#include <mach/mt_irq.h>

#include "lcm_drv.h"
#include "ddp_reg.h"
#include "ddp_path.h"
#include "ddp_irq.h"
#include "ddp_drv.h"
#include "ddp_debug.h"
#include "ddp_manager.h"


#include "ddp_ovl.h"
#include "ddp_rdma.h"

#include "ddp_log.h"
//#pragma GCC optimize("O0")

extern DDP_MODULE_DRIVER  * ddp_modules_driver[DISP_MODULE_NUM];
static int ddp_manager_init = 0;

#define DDP_MAX_MANAGER_HANDLE (DISP_MUTEX_DDP_COUNT+DISP_MUTEX_DDP_FIRST)

typedef struct
{
	volatile unsigned int   init;
	DISP_PATH_EVENT         event;
	wait_queue_head_t       wq;
    volatile unsigned long long data;
}DPMGR_WQ_HANDLE;

typedef struct
{
	DDP_IRQ_BIT 		irq_bit;
}DDP_IRQ_EVENT_MAPPING;

typedef struct
{
    cmdqRecHandle	        cmdqhandle;
    int			            hwmutexid;
    int                     power_sate;
    DDP_MODE                mode;
    struct mutex            mutex_lock;
    DDP_IRQ_EVENT_MAPPING   irq_event_map[DISP_PATH_EVENT_NUM];
    DPMGR_WQ_HANDLE         wq_list[DISP_PATH_EVENT_NUM];
    DDP_SCENARIO_ENUM       scenario;
	DISP_MODULE_ENUM        mem_module;
    disp_ddp_path_config    last_config;
}ddp_path_handle_t, *ddp_path_handle;

typedef struct
{
    int                     handle_cnt;
	int                     mutex_idx;
    int                     power_sate;
	struct mutex            mutex_lock;
	int                     module_usage_table[DISP_MODULE_NUM];
	ddp_path_handle         module_path_table[DISP_MODULE_NUM];
    ddp_path_handle         handle_pool[DDP_MAX_MANAGER_HANDLE];
}DDP_MANAGER_CONTEXT;

#define DEFAULT_IRQ_EVENT_SCENARIO (3)
static DDP_IRQ_EVENT_MAPPING ddp_irq_event_list[DEFAULT_IRQ_EVENT_SCENARIO][DISP_PATH_EVENT_NUM] =
{
	{//ovl0 path
        {DDP_IRQ_RDMA0_DONE          }, /*FRAME_DONE*/
        {DDP_IRQ_RDMA0_START         }, /*FRAME_START*/
        {DDP_IRQ_RDMA0_REG_UPDATE    }, /*FRAME_REG_UPDATE*/
        {DDP_IRQ_RDMA0_TARGET_LINE   }, /*FRAME_TARGET_LINE*/
        {DDP_IRQ_WDMA0_FRAME_COMPLETE}, /*FRAME_COMPLETE*/
        {DDP_IRQ_RDMA0_TARGET_LINE   }, /*FRAME_STOP*/
        {DDP_IRQ_RDMA0_REG_UPDATE    }, /*IF_CMD_DONE*/
        {DDP_IRQ_DSI0_EXT_TE         }, /*IF_VSYNC*/
        {DDP_IRQ_UNKNOW              }, /*TRIGER*/
        {DDP_IRQ_AAL_OUT_END_FRAME   }, /*AAL_OUT_END_EVENT*/
    },
    {//ovl1 path
        {DDP_IRQ_RDMA1_DONE          }, /*FRAME_DONE*/
        {DDP_IRQ_RDMA1_START         }, /*FRAME_START*/
        {DDP_IRQ_RDMA1_REG_UPDATE    }, /*FRAME_REG_UPDATE*/
        {DDP_IRQ_RDMA1_TARGET_LINE   }, /*FRAME_TARGET_LINE*/
        {DDP_IRQ_WDMA1_FRAME_COMPLETE}, /*FRAME_COMPLETE*/
        {DDP_IRQ_RDMA1_TARGET_LINE   }, /*FRAME_STOP*/
        {DDP_IRQ_RDMA1_REG_UPDATE    }, /*IF_CMD_DONE*/
        {DDP_IRQ_RDMA1_TARGET_LINE   }, /*IF_VSYNC*/
        {DDP_IRQ_UNKNOW              }, /*TRIGER*/
        {DDP_IRQ_UNKNOW              }, /*AAL_OUT_END_EVENT*/
    },
    { // rdma path
        {DDP_IRQ_RDMA2_DONE          }, /*FRAME_DONE*/
        {DDP_IRQ_RDMA2_START         }, /*FRAME_START*/
        {DDP_IRQ_RDMA2_REG_UPDATE    }, /*FRAME_REG_UPDATE*/
        {DDP_IRQ_RDMA2_TARGET_LINE   }, /*FRAME_TARGET_LINE*/
        {DDP_IRQ_UNKNOW              }, /*FRAME_COMPLETE*/
        {DDP_IRQ_RDMA2_TARGET_LINE   }, /*FRAME_STOP*/
        {DDP_IRQ_RDMA2_REG_UPDATE    }, /*IF_CMD_DONE*/
        {DDP_IRQ_RDMA2_TARGET_LINE   }, /*IF_VSYNC*/
        {DDP_IRQ_UNKNOW              }, /*TRIGER*/
        {DDP_IRQ_UNKNOW              }, /*AAL_OUT_END_EVENT*/
    },
};

static char * path_event_name(DISP_PATH_EVENT event)
{
    switch(event)
    {
        case DISP_PATH_EVENT_FRAME_START:
             return "FRAME_START";
        case DISP_PATH_EVENT_FRAME_DONE:
             return "FRAME_DONE";
        case DISP_PATH_EVENT_FRAME_REG_UPDATE:
             return "REG_UPDATE";
        case DISP_PATH_EVENT_FRAME_TARGET_LINE:
             return "TARGET_LINE";
        case DISP_PATH_EVENT_FRAME_COMPLETE:
             return "FRAME COMPLETE";
        case DISP_PATH_EVENT_FRAME_STOP:
             return "FRAME_STOP";
        case DISP_PATH_EVENT_IF_CMD_DONE:
             return "FRAME_STOP";
        case DISP_PATH_EVENT_IF_VSYNC:
             return "VSYNC";
        case DISP_PATH_EVENT_TRIGGER:
             return "TRIGGER";
        default:
             return "unknow event";
    }
    return "unknow event";
}

static DDP_MANAGER_CONTEXT* _get_context(void)
{
    static int is_context_inited = 0;
    static DDP_MANAGER_CONTEXT context;
    if(!is_context_inited)
    {
        memset((void*)&context, 0, sizeof(DDP_MANAGER_CONTEXT));
        context.mutex_idx = (1<<DISP_MUTEX_DDP_COUNT) - 1;
        mutex_init(&context.mutex_lock);
        is_context_inited = 1;
    }
    return &context;
}

static int path_top_clock_off(void)
{
    int i =0;
    DDP_MANAGER_CONTEXT * context = _get_context();
    if(context->power_sate)
    {
        for(i=0; i< DDP_MAX_MANAGER_HANDLE; i++)
        {
            if(context->handle_pool[i] != NULL && context->handle_pool[i]->power_sate !=0)
            {
                return 0;
            }
        }
        context->power_sate = 0;
        ddp_path_top_clock_off();
    }
    return 0;
}

static int path_top_clock_on(void)
{
    DDP_MANAGER_CONTEXT * context = _get_context();
    if(!context->power_sate)
    {
        context->power_sate = 1;
        ddp_path_top_clock_on();
    }
    return 0;
}

static int module_is_dsi(DISP_MODULE_ENUM module)
{
    if(module == DISP_MODULE_DSI0 || module == DISP_MODULE_DSI1
                                  || module == DISP_MODULE_DSIDUAL)
    {
        return 1;
    }
    return 0;
}

static int module_power_off(DISP_MODULE_ENUM module)
{
    if(module_is_dsi(module))
    {
        return 0;
    }
    if(ddp_modules_driver[module] != 0)
    {
        if(ddp_modules_driver[module]->power_off!= 0)
        {
            DISP_LOG_V("%s power off\n",ddp_get_module_name(module));
            ddp_modules_driver[module]->power_off(module, NULL); // now just 0;
        }
    }
    return 0;
}

static int module_power_on(DISP_MODULE_ENUM module)
{
    if(module_is_dsi(module))
    {   
        /*bypass dsi*/
        return 0;
    }
    if(ddp_modules_driver[module] != 0)
    {
        if(ddp_modules_driver[module]->power_on!= 0)
        {
            DISP_LOG_V("%s power on\n",ddp_get_module_name(module));
            ddp_modules_driver[module]->power_on(module, NULL); // now just 0;
        }
    }
    return 0;
}

static ddp_path_handle find_handle_by_module(DISP_MODULE_ENUM module)
{
	if(module == DISP_MODULE_DSI0 && _get_context()->module_path_table[DISP_MODULE_DSI0]== NULL){
         return _get_context()->module_path_table[DISP_MODULE_DSIDUAL];
	}
	return _get_context()->module_path_table[module];
	
}

int dpmgr_module_notify(DISP_MODULE_ENUM module, DISP_PATH_EVENT event)
{
    ddp_path_handle handle = find_handle_by_module(module);
    //MMProfileLogEx(ddp_mmp_get_events()->primary_display_aalod_trigger, MMProfileFlagPulse, module, 0);
    return dpmgr_signal_event(handle,event);
    return 0;
}

static int assign_default_irqs_table(DDP_SCENARIO_ENUM scenario,DDP_IRQ_EVENT_MAPPING * irq_events)
{
    int idx = 0;
    switch(scenario)
    {
        case DDP_SCENARIO_PRIMARY_DISP:
        case DDP_SCENARIO_PRIMARY_OVL_MEMOUT:
        case DDP_SCENARIO_PRIMARY_ALL:
        case DDP_SCENARIO_PRIMARY_RDMA0_COLOR0_DISP:
        case DDP_SCENARIO_PRIMARY_RDMA0_DISP:
        case DDP_SCENARIO_PRIMARY_OD_MEMOUT:
        case DDP_SCENARIO_DISP:
            idx = 0;
            break;
        case DDP_SCENARIO_SUB_DISP:
        case DDP_SCENARIO_SUB_OVL_MEMOUT:
        case DDP_SCENARIO_SUB_ALL:
        case DDP_SCENARIO_SUB_RDMA1_DISP:
            idx = 1;
            break;
        case DDP_SCENARIO_SUB_RDMA2_DISP:
            idx = 2;
            break;
        default:
            DISP_LOG_E("unknow scenario %d\n",scenario);
    }
    DISP_LOG_D("assign default irqs table index %d\n",idx);
    memcpy(irq_events,ddp_irq_event_list[idx],sizeof(ddp_irq_event_list[idx]));
    return 0;
}

static int acquire_free_bit(unsigned int total)
{
    int free_id =0 ;
    while(total)
    {
        if(total & 0x1)
        {
            return free_id;
        }
        total>>=1;
        ++free_id;
    }
    return -1;
}

static int acquire_mutex(DDP_SCENARIO_ENUM scenario)
{
///: primay use mutex 0
    int mutex_id =0 ;
    DDP_MANAGER_CONTEXT * content = _get_context();
    int mutex_idx_free = content->mutex_idx;
    ASSERT(scenario >= 0  && scenario < DDP_SCENARIO_MAX);
    while(mutex_idx_free)
    {
        if(mutex_idx_free & 0x1)
        {
            content->mutex_idx &= (~(0x1<<mutex_id));
            mutex_id += DISP_MUTEX_DDP_FIRST;
            break;
        }
        mutex_idx_free>>=1;
        ++mutex_id;
    }
    ASSERT(mutex_id < (DISP_MUTEX_DDP_FIRST+DISP_MUTEX_DDP_COUNT));
    DISP_LOG_V("scenario %s acquire mutex %d , left mutex 0x%x!\n",
           ddp_get_scenario_name(scenario), mutex_id, content->mutex_idx);
    return mutex_id;
}

static int release_mutex(int mutex_idx)
{
    DDP_MANAGER_CONTEXT * content = _get_context();
    ASSERT(mutex_idx < (DISP_MUTEX_DDP_FIRST+DISP_MUTEX_DDP_COUNT));
    content->mutex_idx |= 1<< (mutex_idx-DISP_MUTEX_DDP_FIRST);
    DISP_LOG_V("release mutex %d , left mutex 0x%x!\n",
           mutex_idx, content->mutex_idx);
    return 0;
}

int dpmgr_path_set_video_mode(disp_path_handle dp_handle, int is_vdo_mode)
{
    ddp_path_handle handle = NULL;
    ASSERT(dp_handle != NULL);
    handle = (ddp_path_handle)dp_handle;
    handle->mode = is_vdo_mode ?  DDP_VIDEO_MODE : DDP_CMD_MODE;
    DISP_LOG_V("set scenario %s mode: %s\n",ddp_get_scenario_name(handle->scenario),
               is_vdo_mode ? "Video_Mode":"Cmd_Mode");
    return 0;
}

disp_path_handle dpmgr_create_path(DDP_SCENARIO_ENUM scenario, cmdqRecHandle cmdq_handle)
{
    int i =0;
    int module_name ;
    ddp_path_handle path_handle = NULL;
    int * modules = ddp_get_scenario_list(scenario);
    int module_num = ddp_get_module_num(scenario);
    DDP_MANAGER_CONTEXT * content = _get_context();
    path_handle = kzalloc(sizeof(ddp_path_handle_t), GFP_KERNEL);
    if (NULL != path_handle)
    {
        path_handle->cmdqhandle = cmdq_handle;
        path_handle->scenario = scenario;
        path_handle->hwmutexid = acquire_mutex(scenario);
        assign_default_irqs_table(scenario,path_handle->irq_event_map);
        DISP_LOG_V("create handle %p on scenario %s\n",path_handle,ddp_get_scenario_name(scenario));
        for( i=0; i< module_num;i++)
        {
            module_name = modules[i];
            DISP_LOG_V(" scenario %s include module %s\n",ddp_get_scenario_name(scenario),ddp_get_module_name(module_name));
            content->module_usage_table[module_name]++;
            content->module_path_table[module_name] = path_handle;
        }
        content->handle_cnt ++;
        content->handle_pool[path_handle->hwmutexid] = path_handle;
    }
    else
    {
        DISP_LOG_E("Fail to create handle on scenario %s\n",ddp_get_scenario_name(scenario));
    }
    return path_handle;
}

int dpmgr_get_scenario(disp_path_handle dp_handle)
{
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
	return handle->scenario;
}

int dpmgr_modify_path_power_on_new_modules(disp_path_handle dp_handle, DDP_SCENARIO_ENUM new_scenario, int sw_only)
{
    int i =0;
    int module_name = 0;
    DDP_MANAGER_CONTEXT * content = _get_context();
    ddp_path_handle handle = (ddp_path_handle)dp_handle;

    int *new_modules 	= ddp_get_scenario_list(new_scenario);
    int new_module_num 	= ddp_get_module_num(new_scenario);

	for(i=0; i< new_module_num;i++)
    {
        module_name = new_modules[i];
        if(content->module_usage_table[module_name] == 0)
        {
            content->module_usage_table[module_name]++;
            DISP_LOG_D("add module %s\n", ddp_get_module_name(module_name));
            
            content->module_path_table[module_name] = handle;
			if(!sw_only)
	            module_power_on(module_name);
        }
    }
	return 0;
	
}
/* NOTES: modify path should call API like this :
	old_scenario = dpmgr_get_scenario(handle);
	dpmgr_modify_path_power_on_new_modules();
	dpmgr_modify_path();
  after cmdq handle exec done:
	dpmgr_modify_path_power_off_old_modules();
*/
int dpmgr_modify_path(disp_path_handle dp_handle, DDP_SCENARIO_ENUM new_scenario,
			cmdqRecHandle cmdq_handle, DDP_MODE isvdomode, int sw_only)
{
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
	DDP_SCENARIO_ENUM old_scenario = handle->scenario;
	
    handle->cmdqhandle 	= cmdq_handle;
    handle->scenario 	= new_scenario;
    DISP_LOG_I("modify handle %p from %s to %s\n", handle, ddp_get_scenario_name(old_scenario), ddp_get_scenario_name(new_scenario));

	if(!sw_only)
	{
		// mutex set will clear old settings
		ddp_mutex_set(handle->hwmutexid, new_scenario, isvdomode, cmdq_handle);

		// zxc, disable mutex irq for saving APMCU power consumption.
		#if 1
		ddp_mutex_Interrupt_enable(handle->hwmutexid, cmdq_handle);
		#endif
		// disconnect old path first
		ddp_disconnect_path(old_scenario,cmdq_handle);

		// connect new path
		ddp_connect_path(new_scenario,cmdq_handle);
	}
	return 0;
}

int dpmgr_modify_path_power_off_old_modules(DDP_SCENARIO_ENUM old_scenario, DDP_SCENARIO_ENUM new_scenario, int sw_only)
{
    int i =0;
    int module_name = 0;
    DISP_MODULE_ENUM module[DISP_MODULE_NUM]={0};
    DDP_MANAGER_CONTEXT * content = _get_context();

    int *old_modules 	= ddp_get_scenario_list(old_scenario);
    int old_module_num 	= ddp_get_module_num(old_scenario);

	for(i=0; i< old_module_num;i++)
    {
        module_name = old_modules[i];
        if(!ddp_is_module_in_scenario(new_scenario, module_name))
        {
            content->module_usage_table[module_name]--;
            DISP_LOG_D("remove module %s\n", ddp_get_module_name(module_name));
            content->module_path_table[module_name] = NULL;
			if(!sw_only)
	            module_power_off(module_name);
        }
    }
	
	return 0;
}


int dpmgr_destroy_path_handle(disp_path_handle dp_handle)
{
    int i =0;
    int module_name ;
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
    int * modules = ddp_get_scenario_list(handle->scenario);
    int module_num = ddp_get_module_num(handle->scenario);
    DDP_MANAGER_CONTEXT * content = _get_context();
    DISP_LOG_V("destroy path handle %p on scenario %s\n",handle,ddp_get_scenario_name(handle->scenario));
    if(handle != NULL)
    {
         release_mutex(handle->hwmutexid);
         for( i=0; i< module_num;i++)
         {
             module_name = modules[i];
             content->module_usage_table[module_name]--;
             content->module_path_table[module_name] = NULL;
         }
         content->handle_cnt --;
         ASSERT(content->handle_cnt >=0);
         content->handle_pool[handle->hwmutexid] = NULL;
         kfree(handle);
    }
    return 0;
}

int dpmgr_destroy_path(disp_path_handle dp_handle, cmdqRecHandle cmdq_handle)
{

    ddp_path_handle handle = (ddp_path_handle)dp_handle;
	if(handle)
         ddp_disconnect_path(handle->scenario,cmdq_handle);
	dpmgr_destroy_path_handle(dp_handle);
    return 0;
}

int dpmgr_path_memout_clock(disp_path_handle dp_handle, int clock_switch)
{
#if 0
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
	handle->mem_module = ddp_is_scenario_on_primary(handle->scenario) ? DISP_MODULE_WDMA0: DISP_MODULE_WDMA1;
	if(handle->mem_module == DISP_MODULE_WDMA0 || handle->mem_module ==DISP_MODULE_WDMA1)
	{
		if(ddp_modules_driver[handle->mem_module] != 0)
	    {
	        if(clock_switch)
        	{
	            if(ddp_modules_driver[handle->mem_module]->power_on!= 0)
			    {
				    ddp_modules_driver[handle->mem_module]->power_on(handle->mem_module, NULL);
			    }
        	}
			else
			{
	            if(ddp_modules_driver[handle->mem_module]->power_off!= 0)
			    {
				    ddp_modules_driver[handle->mem_module]->power_off(handle->mem_module, NULL);
			    }
				handle->mem_module = DISP_MODULE_UNKNOWN;
			}
	    }
		return 0;
	}
	return -1;
#endif
    return 0;
}

int dpmgr_path_add_memout(disp_path_handle dp_handle, ENGINE_DUMP engine,void * cmdq_handle )
{
    int ret = 0;
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
    ASSERT(handle->scenario ==DDP_SCENARIO_PRIMARY_DISP || handle->scenario ==DDP_SCENARIO_SUB_DISP);
    DISP_MODULE_ENUM wdma = ddp_is_scenario_on_primary(handle->scenario) ? DISP_MODULE_WDMA0: DISP_MODULE_WDMA1;

    if(ddp_is_module_in_scenario(handle->scenario, wdma)==1)
    {
        DDPERR("dpmgr_path_add_memout error, wdma is already in scenario=%s \n", ddp_get_scenario_name(handle->scenario));
        return -1;
    }

    // update contxt
    DDP_MANAGER_CONTEXT * context = _get_context();
    context->module_usage_table[wdma]++;
    context->module_path_table[wdma] = handle;
    handle->scenario = DDP_SCENARIO_PRIMARY_ALL;
    // update connected
    ddp_connect_path(handle->scenario,cmdq_handle);
    ddp_mutex_set(handle->hwmutexid, handle->scenario, handle->mode, cmdq_handle);

    //wdma just need start.
    if(ddp_modules_driver[wdma] != 0)
    {
    	if(ddp_modules_driver[wdma]->init!= 0)
	    {
		    ddp_modules_driver[wdma]->init(wdma, cmdq_handle);
	    }
	    if(ddp_modules_driver[wdma]->start!= 0)
	    {
		    ddp_modules_driver[wdma]->start(wdma, cmdq_handle);
	    }
    }
    return 0;
}

int dpmgr_path_remove_memout(disp_path_handle dp_handle, void * cmdq_handle)
{
    int ret = 0;
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
    ASSERT(handle->scenario ==DDP_SCENARIO_PRIMARY_DISP || handle->scenario ==DDP_SCENARIO_PRIMARY_ALL
           || handle->scenario ==DDP_SCENARIO_SUB_DISP || handle->scenario ==DDP_SCENARIO_SUB_ALL);
    DISP_MODULE_ENUM wdma = ddp_is_scenario_on_primary(handle->scenario) ? DISP_MODULE_WDMA0: DISP_MODULE_WDMA1;

    if(ddp_is_module_in_scenario(handle->scenario, wdma)==0)
    {
        DDPERR("dpmgr_path_remove_memout error, wdma is not in scenario=%s \n", ddp_get_scenario_name(handle->scenario));
        return -1;
    }

    // update contxt
    DDP_MANAGER_CONTEXT * context = _get_context();
    context->module_usage_table[wdma]--;
    context->module_path_table[wdma] = 0;
    //wdma just need stop
    if(ddp_modules_driver[wdma] != 0)
    {
        if(ddp_modules_driver[wdma]->stop!= 0)
        {
            ddp_modules_driver[wdma]->stop(wdma, cmdq_handle);
        }
    	if(ddp_modules_driver[wdma]->deinit!= 0)
	    {
		    ddp_modules_driver[wdma]->deinit(wdma, cmdq_handle);
	    }
    }
    ddp_disconnect_path(DDP_SCENARIO_PRIMARY_OVL_MEMOUT,cmdq_handle);

    handle->scenario = DDP_SCENARIO_PRIMARY_DISP;
    // update connected
    ddp_connect_path(handle->scenario,cmdq_handle);
    ddp_mutex_set(handle->hwmutexid, handle->scenario, handle->mode, cmdq_handle);

    return 0;
}

int dpmgr_path_set_dst_module(disp_path_handle dp_handle,DISP_MODULE_ENUM dst_module)
{
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
    ASSERT((handle->scenario >= 0  && handle->scenario < DDP_SCENARIO_MAX));
    DISP_LOG_V("set dst module on scenario %s, module %s\n",
          ddp_get_scenario_name(handle->scenario),ddp_get_module_name(dst_module));
    return ddp_set_dst_module(handle->scenario, dst_module);
}

int dpmgr_path_get_mutex(disp_path_handle dp_handle)
{
    ddp_path_handle handle = NULL;
    ASSERT(dp_handle != NULL);
    handle = (ddp_path_handle)dp_handle;
    return handle->hwmutexid;
}

DISP_MODULE_ENUM dpmgr_path_get_dst_module(disp_path_handle dp_handle)
{
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
    ASSERT((handle->scenario >= 0  && handle->scenario < DDP_SCENARIO_MAX));
    DISP_MODULE_ENUM dst_module = ddp_get_dst_module(handle->scenario);
    DISP_LOG_V("get dst module on scenario %s, module %s\n",
             ddp_get_scenario_name(handle->scenario),ddp_get_module_name(dst_module));
    return dst_module;
}

int dpmgr_path_connect(disp_path_handle dp_handle, int encmdq)
{
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
    cmdqRecHandle cmdqHandle = encmdq ? handle->cmdqhandle : NULL;
    DISP_LOG_D("dpmgr_path_connect on scenario %s\n",ddp_get_scenario_name(handle->scenario));

    ddp_connect_path(handle->scenario,cmdqHandle);
    ddp_mutex_set(handle->hwmutexid,
                         handle->scenario,
                         handle->mode,
                         cmdqHandle);
    ddp_mutex_Interrupt_enable(handle->hwmutexid,cmdqHandle);
    return 0;
}

int dpmgr_path_disconnect(disp_path_handle dp_handle, int encmdq)
{
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
    cmdqRecHandle cmdqHandle = encmdq ? handle->cmdqhandle : NULL;
    DISP_LOG_D("dpmgr_path_disconnect on scenario %s\n",ddp_get_scenario_name(handle->scenario));
    ddp_mutex_clear(handle->hwmutexid,cmdqHandle);
    ddp_mutex_Interrupt_disable(handle->hwmutexid,cmdqHandle);
    ddp_disconnect_path(handle->scenario, cmdqHandle);
    return 0;
}

int  dpmgr_path_init(disp_path_handle dp_handle, int encmdq)
{
    int i=0;
    int module_name ;
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
    int * modules = ddp_get_scenario_list(handle->scenario);
    int module_num = ddp_get_module_num(handle->scenario);
    cmdqRecHandle cmdqHandle = encmdq ? handle->cmdqhandle : NULL;
    DISP_LOG_I("path init on scenario %s\n",ddp_get_scenario_name(handle->scenario));
    //open top clock
    path_top_clock_on();
    //seting mutex
    ddp_mutex_set(handle->hwmutexid,
                         handle->scenario,
                         handle->mode,
                         cmdqHandle);
    ddp_mutex_Interrupt_enable(handle->hwmutexid,cmdqHandle);
    //connect path;
    ddp_connect_path(handle->scenario,cmdqHandle);

    // each module init
    for( i=0; i< module_num;i++)
    {
        module_name = modules[i];
        if(ddp_modules_driver[module_name] != 0)
        {
            if(ddp_modules_driver[module_name]->init!= 0)
            {
                 DISP_LOG_D("scenario %s init module  %s\n",ddp_get_scenario_name(handle->scenario),
                    ddp_get_module_name(module_name));
                 ddp_modules_driver[module_name]->init(module_name, cmdqHandle);
            }
            if(ddp_modules_driver[module_name]->set_listener!= 0)
            {
                ddp_modules_driver[module_name]->set_listener(module_name,dpmgr_module_notify);
            }
        }
    }
    //after init this path will power on;
    handle->power_sate = 1;
    return 0;
}

int  dpmgr_path_deinit(disp_path_handle dp_handle, int encmdq)
{
    int i=0;
    int module_name ;
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
    int * modules = ddp_get_scenario_list(handle->scenario);
    int module_num = ddp_get_module_num(handle->scenario);
    cmdqRecHandle cmdqHandle = encmdq ? handle->cmdqhandle : NULL;
    DISP_LOG_I("path deinit on scenario %s\n",ddp_get_scenario_name(handle->scenario));
    ddp_mutex_Interrupt_disable(handle->hwmutexid,cmdqHandle);
    ddp_mutex_clear(handle->hwmutexid,cmdqHandle);
    ddp_disconnect_path(handle->scenario, cmdqHandle);
    for( i=0; i< module_num;i++)
    {
        module_name = modules[i];
        if(ddp_modules_driver[module_name] != 0)
        {
            if(ddp_modules_driver[module_name]->deinit!= 0)
            {
                DISP_LOG_D("scenario %s deinit module  %s\n",ddp_get_scenario_name(handle->scenario),
                    ddp_get_module_name(module_name));
                ddp_modules_driver[module_name]->deinit(module_name, cmdqHandle);
            }
            if(ddp_modules_driver[module_name]->set_listener!= 0)
            {
                ddp_modules_driver[module_name]->set_listener(module_name,NULL);
            }
        }
    }
    handle->power_sate = 0;
    //close top clock when last path init
    path_top_clock_off();
    return 0;
}

int dpmgr_path_start(disp_path_handle dp_handle, int encmdq)
{
    int i=0;
    int module_name ;
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
    int * modules = ddp_get_scenario_list(handle->scenario);
    int module_num = ddp_get_module_num(handle->scenario);
    cmdqRecHandle cmdqHandle = encmdq ? handle->cmdqhandle : NULL;
    DISP_LOG_V("path start on scenario %s\n",ddp_get_scenario_name(handle->scenario));
    for( i=0; i< module_num;i++)
    {
        module_name = modules[i];
        if(ddp_modules_driver[module_name] != 0)
        {
            if(ddp_modules_driver[module_name]->start!= 0)
            {
                DISP_LOG_V("scenario %s start module  %s\n",ddp_get_scenario_name(handle->scenario),
                   ddp_get_module_name(module_name));
                ddp_modules_driver[module_name]->start(module_name, cmdqHandle);
            }
        }
    }
    return 0;
}

int dpmgr_path_stop(disp_path_handle dp_handle, int encmdq)
{

    int i=0;
    int module_name ;
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
    int * modules = ddp_get_scenario_list(handle->scenario);
    int module_num = ddp_get_module_num(handle->scenario);
    cmdqRecHandle cmdqHandle = encmdq ? handle->cmdqhandle : NULL;
    DISP_LOG_V("path stop on scenario %s\n",ddp_get_scenario_name(handle->scenario));
    for( i=module_num-1; i>=0;i--)
    {
        module_name = modules[i];
        if(ddp_modules_driver[module_name] != 0)
        {
            if(ddp_modules_driver[module_name]->stop!= 0)
            {
                DISP_LOG_V("scenario %s  stop module %s \n",ddp_get_scenario_name(handle->scenario),
                    ddp_get_module_name(module_name));
                ddp_modules_driver[module_name]->stop(module_name, cmdqHandle);
            }
        }
    }
    return 0;
}

int dpmgr_path_ioctl(disp_path_handle dp_handle, void * cmdq_handle, DDP_IOCTL_NAME ioctl_cmd, unsigned long *params)
{
	int i=0;
	int ret  = 0;
	int module_name ;
	ASSERT(dp_handle != NULL);
	ddp_path_handle handle = (ddp_path_handle)dp_handle;
	int * modules = ddp_get_scenario_list(handle->scenario);
	int module_num = ddp_get_module_num(handle->scenario);
	DISP_LOG_V("path IOCTL on scenario %s\n",ddp_get_scenario_name(handle->scenario));
	for( i=module_num-1; i >= 0  ;i--)
	{
	    module_name = modules[i];
	    if(ddp_modules_driver[module_name] != 0)
	    {
	        if(ddp_modules_driver[module_name]->ioctl!= 0)
	        {
	            DISP_LOG_D("scenario %s, module %s ioctl\n",ddp_get_scenario_name(handle->scenario),
	                ddp_get_module_name(module_name));
	           ret += ddp_modules_driver[module_name]->ioctl(module_name, cmdq_handle, (unsigned int)ioctl_cmd, params);
	        }
	    }
	}
	return ret;
}

int dpmgr_path_enable_irq(disp_path_handle dp_handle, void * cmdq_handle, DDP_IRQ_LEVEL irq_level)
{
	int i=0;
	int ret  = 0;
	int module_name ;
	ASSERT(dp_handle != NULL);
	ddp_path_handle handle = (ddp_path_handle)dp_handle;
	int * modules = ddp_get_scenario_list(handle->scenario);
	int module_num = ddp_get_module_num(handle->scenario);
	DISP_LOG_D("path enable irq on scenario %s, level %d\n",ddp_get_scenario_name(handle->scenario), irq_level);

	if(irq_level != DDP_IRQ_LEVEL_ALL)
	{
		ddp_mutex_Interrupt_disable(handle->hwmutexid, cmdq_handle);
	}
	else
	{
		ddp_mutex_Interrupt_enable(handle->hwmutexid, cmdq_handle);
	}
	
	for( i=module_num-1; i >= 0  ;i--)
	{
	    module_name = modules[i];
	    if(ddp_modules_driver[module_name] != 0)
	    {
	        if(ddp_modules_driver[module_name]->enable_irq!= 0)
	        {
	            DISP_LOG_V("scenario %s, module %s enable irq level %d\n",ddp_get_scenario_name(handle->scenario),
	                ddp_get_module_name(module_name), irq_level);
	           ret += ddp_modules_driver[module_name]->enable_irq(module_name, cmdq_handle, irq_level);
	        }
	    }
	}
	return ret;
}

int dpmgr_path_reset(disp_path_handle dp_handle, int encmdq)
{
    int i=0;
    int ret = 0;
    int error = 0;
    int module_name ;
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
    int * modules = ddp_get_scenario_list(handle->scenario);
    int module_num = ddp_get_module_num(handle->scenario);
    cmdqRecHandle cmdqHandle = encmdq ? handle->cmdqhandle : NULL;
    DISP_LOG_D("path reset on scenario %s\n",ddp_get_scenario_name(handle->scenario));
    /* first reset mutex */
    ddp_mutex_reset(handle->hwmutexid,cmdqHandle);

    for( i=0; i< module_num;i++)
    {
        module_name = modules[i];
        if(ddp_modules_driver[module_name] != 0)
        {
            if(ddp_modules_driver[module_name]->reset != 0)
            {
                DISP_LOG_V("scenario %s  reset module %s \n",ddp_get_scenario_name(handle->scenario),
                    ddp_get_module_name(module_name));
                ret = ddp_modules_driver[module_name]->reset(module_name, cmdqHandle);
                if(ret != 0)
                {
                    error++;
                }
            }
        }
    }
    return error > 0? -1: 0;
}

int dpmgr_path_config(disp_path_handle dp_handle, disp_ddp_path_config * config, void * cmdq_handle)
{
    int i=0;
    int module_name ;
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
    int * modules =  ddp_get_scenario_list(handle->scenario);
    int module_num = ddp_get_module_num(handle->scenario);
    DISP_LOG_V("path config ovl %d, rdma %d, wdma %d, dst %d on handle %p scenario %s\n",
                config->ovl_dirty,
                config->rdma_dirty,
                config->wdma_dirty,
                config->dst_dirty,
                handle,
                ddp_get_scenario_name(handle->scenario));
	memcpy(&handle->last_config, config, sizeof(disp_ddp_path_config));
    for( i=0; i< module_num;i++)
    {
        module_name = modules[i];
        if(ddp_modules_driver[module_name] != 0)
        {
            if(ddp_modules_driver[module_name]->config!= 0)
            {
                DISP_LOG_V("scenario %s  config module %s \n",ddp_get_scenario_name(handle->scenario),
                   ddp_get_module_name(module_name));
                ddp_modules_driver[module_name]->config(module_name, config, cmdq_handle);
            }
        }
    }
    return 0;
}

disp_ddp_path_config *dpmgr_path_get_last_config(disp_path_handle dp_handle)
{
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
    handle->last_config.ovl_dirty  =0;
    handle->last_config.rdma_dirty =0;
    handle->last_config.wdma_dirty =0;
    handle->last_config.dst_dirty =0;
	return &handle->last_config;
}

void dpmgr_get_input_address(disp_path_handle dp_handle, unsigned long * addr)
{
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
    int * modules = ddp_get_scenario_list(handle->scenario);
    if (modules[0] == DISP_MODULE_OVL0 || modules[0] == DISP_MODULE_OVL1){
        ovl_get_address(modules[0],addr);
    }else if(modules[0] == DISP_MODULE_RDMA0 || modules[0] == DISP_MODULE_RDMA1
        || modules[0] == DISP_MODULE_RDMA2){
        rdma_get_address(modules[0],addr);
    }
    return ;
}

int dpmgr_path_build_cmdq(disp_path_handle dp_handle, void *trigger_loop_handle, CMDQ_STATE state, int reverse)
{
    int ret = 0;
    int i=0;
    int module_name ;
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
    int * modules = ddp_get_scenario_list(handle->scenario);
    int module_num = ddp_get_module_num(handle->scenario);
    DISP_LOG_D("path build cmdq on scenario %s\n",ddp_get_scenario_name(handle->scenario));

	if(reverse)
	{
		for( i=module_num-1; i>=0;i--)
		{
			module_name = modules[i];
			if(ddp_modules_driver[module_name] != 0)
			{
				if(ddp_modules_driver[module_name]->build_cmdq!= 0)
				{
					DISP_LOG_V("%s build cmdq, state=%d\n",ddp_get_module_name(module_name), state);
					ret = ddp_modules_driver[module_name]->build_cmdq(module_name, trigger_loop_handle, state); // now just 0;
				}
			}
		}
	}
	else
	{
		for( i=0; i< module_num;i++)
		{
			module_name = modules[i];
			if(ddp_modules_driver[module_name] != 0)
			{
				if(ddp_modules_driver[module_name]->build_cmdq!= 0)
				{
					DISP_LOG_V("%s build cmdq, state=%d\n",ddp_get_module_name(module_name), state);
					ret = ddp_modules_driver[module_name]->build_cmdq(module_name, trigger_loop_handle, state); // now just 0;
				}
			}
		}
    }
	
    return ret;
}

int dpmgr_path_trigger(disp_path_handle dp_handle, void *trigger_loop_handle, int encmdq)
{
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
    DISP_LOG_V("dpmgr_path_trigger on scenario %s\n",ddp_get_scenario_name(handle->scenario));
    cmdqRecHandle cmdqHandle = encmdq ? handle->cmdqhandle : NULL;
    int i=0;
    int * modules = ddp_get_scenario_list(handle->scenario);
    int module_num = ddp_get_module_num(handle->scenario);
    int module_name ;
    ddp_mutex_enable(handle->hwmutexid,handle->scenario,trigger_loop_handle);
    for( i=0; i< module_num;i++)
    {
        module_name = modules[i];
        if(ddp_modules_driver[module_name] != 0)
        {
            if(ddp_modules_driver[module_name]->trigger!= 0)
            {
                DISP_LOG_V("%s trigger\n",ddp_get_module_name(module_name));
                ddp_modules_driver[module_name]->trigger(module_name, trigger_loop_handle);
            }
        }
    }
    return 0;
}

int dpmgr_path_flush(disp_path_handle dp_handle,int encmdq)
{
   ASSERT(dp_handle != NULL);
   ddp_path_handle handle = (ddp_path_handle)dp_handle;
   cmdqRecHandle cmdqHandle = encmdq ? handle->cmdqhandle : NULL;
   DISP_LOG_V("path flush on scenario %s\n",ddp_get_scenario_name(handle->scenario));
   return ddp_mutex_enable(handle->hwmutexid,handle->scenario,cmdqHandle);
}

int dpmgr_path_power_off(disp_path_handle dp_handle, CMDQ_SWITCH encmdq)
{
    int i=0;
    int module_name ;
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
    int * modules = ddp_get_scenario_list(handle->scenario);
    int module_num = ddp_get_module_num(handle->scenario);
    DISP_LOG_I("path power off on scenario %s\n",ddp_get_scenario_name(handle->scenario));
    for( i=0; i< module_num;i++)
    {
        module_name = modules[i];
        if(ddp_modules_driver[module_name] != 0)
        {
            if(ddp_modules_driver[module_name]->power_off!= 0)
            {
                 DISP_LOG_V(" %s power off\n",ddp_get_module_name(module_name));
	             ddp_modules_driver[module_name]->power_off(module_name, encmdq?handle->cmdqhandle:NULL); // now just 0;
            }
        }
    }
    handle->power_sate = 0;
    path_top_clock_off();
    return 0;
}

int dpmgr_path_power_on(disp_path_handle dp_handle, CMDQ_SWITCH encmdq)
{
    int i=0;
    int module_name ;
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
    int * modules = ddp_get_scenario_list(handle->scenario);
    int module_num = ddp_get_module_num(handle->scenario);
    DISP_LOG_I("path power on scenario %s\n",ddp_get_scenario_name(handle->scenario));
    path_top_clock_on();
    for( i=0; i< module_num;i++)
    {
        module_name = modules[i];
        if(ddp_modules_driver[module_name] != 0)
        {
            if(ddp_modules_driver[module_name]->power_on!= 0)
            {
                DISP_LOG_I("%s power on\n",ddp_get_module_name(module_name));
                ddp_modules_driver[module_name]->power_on(module_name, encmdq?handle->cmdqhandle:NULL); // now just 0;
            }
        }
    }
   //modules on this path will resume power on;
    handle->power_sate = 1;
    return 0;
}

static int is_module_in_path(DISP_MODULE_ENUM module,ddp_path_handle handle)
{
    DDP_MANAGER_CONTEXT * context = _get_context();
    ASSERT(module <DISP_MODULE_UNKNOWN);
    if(context->module_path_table[module]==handle)
    {
        return 1;
    }
    return 0;
}

int dpmgr_path_user_cmd(disp_path_handle dp_handle, int msg, unsigned long arg, void *cmdqhandle)
{
    int ret = -1;
    DISP_MODULE_ENUM dst = DISP_MODULE_UNKNOWN;
    ddp_path_handle handle = NULL;
    ASSERT(dp_handle != NULL);
    handle = (ddp_path_handle)dp_handle;
    //DISP_LOG_W("dpmgr_path_user_cmd msg 0x%08x\n",msg);
    switch(msg)
    {
        case DISP_IOCTL_AAL_EVENTCTL:
        case DISP_IOCTL_AAL_GET_HIST:
        case DISP_IOCTL_AAL_INIT_REG:
        case DISP_IOCTL_AAL_SET_PARAM:
        {
            if(is_module_in_path(DISP_MODULE_AAL,handle))
            {
                if(ddp_modules_driver[DISP_MODULE_AAL]->cmd!=NULL)
                {
                    ret = ddp_modules_driver[DISP_MODULE_AAL]->cmd(DISP_MODULE_AAL,msg,arg,
                        cmdqhandle);
                }
            }
            break;
        }
        case DISP_IOCTL_SET_GAMMALUT:
        case DISP_IOCTL_SET_CCORR:
            if(ddp_modules_driver[DISP_MODULE_GAMMA]->cmd!=NULL)
            {
                ret = ddp_modules_driver[DISP_MODULE_GAMMA]->cmd(DISP_MODULE_GAMMA,msg,arg,
                    cmdqhandle);
            }
            break;
        case DISP_IOCTL_SET_PQPARAM:
        case DISP_IOCTL_GET_PQPARAM:
        case DISP_IOCTL_SET_PQINDEX:
        case DISP_IOCTL_GET_PQINDEX:
        case DISP_IOCTL_SET_TDSHPINDEX:
        case DISP_IOCTL_GET_TDSHPINDEX:
        case DISP_IOCTL_SET_PQ_CAM_PARAM:
        case DISP_IOCTL_GET_PQ_CAM_PARAM:
        case DISP_IOCTL_SET_PQ_GAL_PARAM:
        case DISP_IOCTL_GET_PQ_GAL_PARAM:
        case DISP_IOCTL_PQ_SET_BYPASS_COLOR:
        case DISP_IOCTL_PQ_SET_WINDOW:
        case DISP_IOCTL_WRITE_REG:
        case DISP_IOCTL_READ_REG:
        case DISP_IOCTL_MUTEX_CONTROL:
        case DISP_IOCTL_PQ_GET_TDSHP_FLAG:
        case DISP_IOCTL_PQ_SET_TDSHP_FLAG:
        case DISP_IOCTL_PQ_GET_DC_PARAM:
        case DISP_IOCTL_PQ_SET_DC_PARAM:
        case DISP_IOCTL_WRITE_SW_REG:
        case DISP_IOCTL_READ_SW_REG:
        {
            if(is_module_in_path(DISP_MODULE_COLOR0,handle))
            {
                dst = DISP_MODULE_COLOR0;
            }
            else if(is_module_in_path(DISP_MODULE_COLOR1,handle))
            {
                dst = DISP_MODULE_COLOR1;
            }
            else
            {
                DISP_LOG_W("dpmgr_path_user_cmd color is not on this path\n");
            }
            if(dst != DISP_MODULE_UNKNOWN)
            {

                if(ddp_modules_driver[dst]->cmd!=NULL)
                {
                    ret = ddp_modules_driver[dst]->cmd(dst,msg,arg,
                        cmdqhandle);
                }
            }
            break;
        }
        case DISP_IOCTL_OD_CTL:
        {
            if(is_module_in_path(DISP_MODULE_OD,handle))
            {
                if(ddp_modules_driver[DISP_MODULE_OD]->cmd!=NULL)
                {
                    ret = ddp_modules_driver[DISP_MODULE_OD]->cmd(DISP_MODULE_OD,msg,arg,
                        cmdqhandle);
                }
            }
            break;
        }
        default:
        {
            DISP_LOG_W("dpmgr_path_user_cmd io not supported\n");
            break;
        }
    }
    return ret;
}

int dpmgr_path_set_parameter(disp_path_handle dp_handle, int io_evnet, void * data)
{
    return 0;
}

int dpmgr_path_get_parameter(disp_path_handle dp_handle,int io_evnet, void * data )
{
    return 0;
}

int dpmgr_path_is_idle(disp_path_handle dp_handle)
{
	ASSERT(dp_handle != NULL);
	return !dpmgr_path_is_busy(dp_handle);
}

int dpmgr_path_is_busy(disp_path_handle dp_handle)
{
    int i=0;
    int module_name ;
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
    int * modules = ddp_get_scenario_list(handle->scenario);
    int module_num = ddp_get_module_num(handle->scenario);
    DISP_LOG_V("path check busy on scenario %s\n",ddp_get_scenario_name(handle->scenario));
    for( i=module_num-1; i>=0;i--)
    {
        module_name = modules[i];
        if(ddp_modules_driver[module_name] != 0)
        {
            if(ddp_modules_driver[module_name]->is_busy!= 0)
            {
                if(ddp_modules_driver[module_name]->is_busy(module_name))
                {
                    DISP_LOG_V("%s is busy\n", ddp_get_module_name(module_name));
                    return 1;
                }
            }
        }
    }
return 0;
}

int dpmgr_set_lcm_utils(disp_path_handle dp_handle, void *lcm_drv)
{
    int i=0;
    int module_name ;
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
    int * modules = ddp_get_scenario_list(handle->scenario);
    int module_num = ddp_get_module_num(handle->scenario);

    DISP_LOG_V("path set lcm drv handle %p\n", handle);
    for( i=0; i< module_num;i++)
    {
        module_name = modules[i];
        if(ddp_modules_driver[module_name] != 0)
        {
            if((ddp_modules_driver[module_name]->set_lcm_utils!= 0)&&lcm_drv)
            {
                DISP_LOG_V("%s set lcm utils\n",ddp_get_module_name(module_name));
                ddp_modules_driver[module_name]->set_lcm_utils(module_name, lcm_drv); // now just 0;
            }
        }
    }
    return 0;
}

int dpmgr_enable_event(disp_path_handle dp_handle, DISP_PATH_EVENT event)
{
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
	DPMGR_WQ_HANDLE *wq_handle = &handle->wq_list[event];
    DISP_LOG_D("enable event %s on scenario %s, irtbit 0x%x\n",
         path_event_name(event),
         ddp_get_scenario_name(handle->scenario),
         handle->irq_event_map[event].irq_bit);
    if(!wq_handle->init)
    {
    	init_waitqueue_head(&(wq_handle->wq));
        wq_handle->init = 1;
        wq_handle->data= 0;
        wq_handle->event = event;
    }
    return 0;
}

int dpmgr_map_event_to_irq(disp_path_handle dp_handle, DISP_PATH_EVENT event, DDP_IRQ_BIT irq_bit)
{
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
    DDP_IRQ_EVENT_MAPPING *irq_table = handle->irq_event_map;
    if(event < DISP_PATH_EVENT_NUM)
    {
       DISP_LOG_D("map event %s to irq 0x%x on scenario %s\n",path_event_name(event),irq_bit,ddp_get_scenario_name(handle->scenario));
       irq_table[event].irq_bit= irq_bit;
       return 0;
    }
    DISP_LOG_E("fail to map event %s to irq 0x%x on scenario %s\n",path_event_name(event),irq_bit,ddp_get_scenario_name(handle->scenario));
    return -1;
}

int dpmgr_disable_event(disp_path_handle dp_handle, DISP_PATH_EVENT event)
{
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
    DISP_LOG_D("disable event %s on scenario %s \n",path_event_name(event),ddp_get_scenario_name(handle->scenario));
	DPMGR_WQ_HANDLE *wq_handle = &handle->wq_list[event];
    wq_handle->init = 0;
    wq_handle->data= 0;
    return 0;
}

int dpmgr_check_status(disp_path_handle dp_handle)
{
    int i =0;
    int module_name ;
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
    int * modules = ddp_get_scenario_list(handle->scenario);
    int module_num = ddp_get_module_num(handle->scenario);
    DISP_LOG_D("check status on scenario %s\n",
                ddp_get_scenario_name(handle->scenario));
    ddp_check_path(handle->scenario);
    ddp_check_mutex(handle->hwmutexid,handle->scenario,
                    handle->mode);
    for( i=0; i< module_num;i++)
    {
        module_name = modules[i];
		if(ddp_modules_driver[module_name] != 0 && ddp_modules_driver[module_name]->dump_info!= 0)
        {
            ddp_modules_driver[module_name]->dump_info(module_name, 1);
        }
        else
        {
           ddp_dump_analysis(module_name);
           ddp_dump_reg(module_name);
        }
    }
    ddp_dump_analysis(DISP_MODULE_CONFIG);
    ddp_dump_reg(DISP_MODULE_CONFIG);

    ddp_dump_analysis(DISP_MODULE_MUTEX);
    ddp_dump_reg(DISP_MODULE_MUTEX);
    return 0;
}

void dpmgr_debug_path_status(int mutex_id)
{
    int i=0;
    DDP_MANAGER_CONTEXT * content = _get_context();
    disp_path_handle handle=NULL;
    if(mutex_id >=DISP_MUTEX_DDP_FIRST && mutex_id < (DISP_MUTEX_DDP_FIRST+DISP_MUTEX_DDP_COUNT))
    {
        handle = (disp_path_handle)content->handle_pool[mutex_id];
        if(handle)
        {
            dpmgr_check_status(handle);
        }
    }
    else
    {
        for(i=DISP_MUTEX_DDP_FIRST; i<(DISP_MUTEX_DDP_FIRST+DISP_MUTEX_DDP_COUNT);i++)
        {
            handle = (disp_path_handle)content->handle_pool[i];
            if(handle)
            {
                dpmgr_check_status(handle);
            }
        }
    }
    return ;
}

int dpmgr_wait_event_timeout(disp_path_handle dp_handle, DISP_PATH_EVENT event, int timeout)
{
    int ret = -1;
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
    DPMGR_WQ_HANDLE *wq_handle = & handle->wq_list[event];
    if(wq_handle->init)
    {
        DISP_LOG_V("wait event %s on scenario %s\n", path_event_name(event),ddp_get_scenario_name(handle->scenario));
        unsigned long long cur_time = sched_clock();
        ret = wait_event_interruptible_timeout(wq_handle->wq, cur_time < wq_handle->data,timeout);
        if(ret == 0)
        {
            DISP_LOG_E("wait %s timeout on scenario %s\n", path_event_name(event),ddp_get_scenario_name(handle->scenario));
        }
        else if(ret < 0)
        {
            DISP_LOG_E("wait %s interrupt by other timeleft %d on scenario %s\n", path_event_name(event),ret,ddp_get_scenario_name(handle->scenario));
        }
        else
        {
            DISP_LOG_V("received event %s timeleft %d on scenario %s\n", path_event_name(event), ret,ddp_get_scenario_name(handle->scenario));
        }
        return ret;
    }
    DISP_LOG_E("wait event %s not initialized on scenario %s\n", path_event_name(event),ddp_get_scenario_name(handle->scenario));
    return ret;
}

int dpmgr_wait_event(disp_path_handle dp_handle, DISP_PATH_EVENT event)
{
    int ret = -1;
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
    DPMGR_WQ_HANDLE *wq_handle = & handle->wq_list[event];
    if(wq_handle->init)
    {
        DISP_LOG_V("wait event %s on scenario %s\n", path_event_name(event),ddp_get_scenario_name(handle->scenario));
        unsigned long long cur_time = sched_clock();
        ret = wait_event_interruptible(wq_handle->wq, cur_time < wq_handle->data);
        if(ret < 0)
        {
            DISP_LOG_E("wait %s interrupt by other ret %d on scenario %s\n", path_event_name(event), ret,ddp_get_scenario_name(handle->scenario));
        }
        else
        {
            DISP_LOG_V("received event %s ret %d on scenario %s\n", path_event_name(event), ret,ddp_get_scenario_name(handle->scenario));
        }
        return ret;
    }
    DISP_LOG_E("wait event %s not initialized on scenario %s\n", path_event_name(event),ddp_get_scenario_name(handle->scenario));
    return ret;
}

int dpmgr_signal_event(disp_path_handle dp_handle, DISP_PATH_EVENT event)
{
    ASSERT(dp_handle != NULL);
    ddp_path_handle handle = (ddp_path_handle)dp_handle;
    DPMGR_WQ_HANDLE *wq_handle = &handle->wq_list[event];
    if(handle->wq_list[event].init)
    {
        wq_handle->data = sched_clock();
        DISP_LOG_V("wake up evnet %s on scenario %s\n", path_event_name(event),ddp_get_scenario_name(handle->scenario));
        wake_up_interruptible(&(handle->wq_list[event].wq));
    }
    return 0;
}

static void dpmgr_irq_handler(DISP_MODULE_ENUM module,unsigned int regvalue)
{
    int i = 0;
    int j = 0;
    int irq_bits_num =0;
    int irq_bit = 0;
    ddp_path_handle handle = NULL;
    handle   = find_handle_by_module(module);
    if(handle == NULL)
    {
        return;
    }
    irq_bits_num =  ddp_get_module_max_irq_bit(module);
    for(i = 0; i<=irq_bits_num; i++)
    {
        if(regvalue & (0x1<<i))
        {
            irq_bit = MAKE_DDP_IRQ_BIT(module,i);
	        dprec_stub_irq(irq_bit);
            for(j = 0; j < DISP_PATH_EVENT_NUM; j++)
            {
                if(handle->wq_list[j].init && irq_bit ==  handle->irq_event_map[j].irq_bit)
                {
                    dprec_stub_event(j);
                    handle->wq_list[j].data =sched_clock();
                    DDPIRQ("irq signal event %s on cycle %llu on scenario %s\n", path_event_name(j), handle->wq_list[j].data ,ddp_get_scenario_name(handle->scenario));
                    wake_up_interruptible(&(handle->wq_list[j].wq));
                    //MMProfileLogEx(ddp_mmp_get_events()->primary_wakeup, MMProfileFlagPulse, j, irq_bit);
                }
            }
        }
    }
    return ;
}

int dpmgr_init(void)
{
    DISP_LOG_I("ddp manager init\n");
    if (ddp_manager_init)
        return 0;
    ddp_manager_init = 1;
    ddp_debug_init();
    disp_init_irq();
	disp_register_irq_callback(dpmgr_irq_handler);

    return  0;
}


