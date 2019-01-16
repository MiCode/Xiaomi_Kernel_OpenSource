#ifndef __DDP_PATH_H__
#define __DDP_PATH_H__

#include "ddp_info.h"

#define DDP_OVL_LAYER_MUN 4

typedef enum {
    DDP_VIDEO_MODE = 0,
    DDP_CMD_MODE,
} DDP_MODE;  

typedef enum
{
    DDP_SCENARIO_PRIMARY_DISP = 0,
    DDP_SCENARIO_PRIMARY_RDMA0_COLOR0_DISP,
    DDP_SCENARIO_PRIMARY_RDMA0_DISP,
    DDP_SCENARIO_PRIMARY_BYPASS_RDMA,
    DDP_SCENARIO_PRIMARY_OVL_MEMOUT,
    DDP_SCENARIO_PRIMARY_OD_MEMOUT,
    DDP_SCENARIO_PRIMARY_UFOE_MEMOUT,
    DDP_SCENARIO_SUB_DISP, 
    DDP_SCENARIO_SUB_RDMA1_DISP,
    DDP_SCENARIO_SUB_RDMA2_DISP, 
    DDP_SCENARIO_SUB_OVL_MEMOUT,
    DDP_SCENARIO_SUB_GAMMA_MEMOUT, 
    DDP_SCENARIO_DISP,
    DDP_SCENARIO_RDMA0_DUAL_DISP,
    DDP_SCENARIO_PRIMARY_ALL,     
    DDP_SCENARIO_SUB_ALL,
    DDP_SCENARIO_MAX    
} DDP_SCENARIO_ENUM;

void  ddp_connect_path(DDP_SCENARIO_ENUM scenario,void * handle);
void  ddp_disconnect_path(DDP_SCENARIO_ENUM scenario,void * handle);
int   ddp_get_module_num(DDP_SCENARIO_ENUM scenario);

void  ddp_check_path(DDP_SCENARIO_ENUM scenario);
int   ddp_mutex_set( int mutex_id,DDP_SCENARIO_ENUM scenario,DDP_MODE mode,void * handle);
int   ddp_mutex_clear( int mutex_id, void * handle);
int   ddp_mutex_enable(int mutex_id,DDP_SCENARIO_ENUM scenario,void * handle);
int   ddp_mutex_disable(int mutex_id,DDP_SCENARIO_ENUM scenario,void * handle);
void  ddp_check_mutex(int mutex_id, DDP_SCENARIO_ENUM scenario, DDP_MODE mode);
int   ddp_mutex_reset(int mutex_id, void * handle);

int ddp_mutex_add_module(int mutex_id, DISP_MODULE_ENUM module, void * handle);

int ddp_mutex_remove_module(int mutex_id, DISP_MODULE_ENUM module, void * handle);

int ddp_mutex_Interrupt_enable(int mutex_id,void * handle);

int ddp_mutex_Interrupt_disable(int mutex_id,void * handle);


DISP_MODULE_ENUM   ddp_get_dst_module(DDP_SCENARIO_ENUM scenario);
int   ddp_set_dst_module(DDP_SCENARIO_ENUM scenario, DISP_MODULE_ENUM dst_module);

int * ddp_get_scenario_list(DDP_SCENARIO_ENUM ddp_scenario);

int   ddp_insert_module(DDP_SCENARIO_ENUM ddp_scenario, DISP_MODULE_ENUM place,  DISP_MODULE_ENUM module);
int   ddp_remove_module(DDP_SCENARIO_ENUM ddp_scenario, DISP_MODULE_ENUM module);

int   ddp_is_scenario_on_primary(DDP_SCENARIO_ENUM scenario);

char* ddp_get_scenario_name(DDP_SCENARIO_ENUM scenario);

int   ddp_path_top_clock_off(void);
int   ddp_path_top_clock_on(void);

//should remove
int ddp_insert_config_allow_rec(void *handle);
int ddp_insert_config_dirty_rec(void *handle);

int   disp_get_dst_module(DDP_SCENARIO_ENUM scenario);
int  ddp_is_module_in_scenario(DDP_SCENARIO_ENUM ddp_scenario, DISP_MODULE_ENUM module);

#endif
