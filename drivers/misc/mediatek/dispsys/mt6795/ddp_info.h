#ifndef _H_DDP_INFO
#define _H_DDP_INFO
#include <linux/types.h>
#include "ddp_hal.h"
#include "DpDataType.h"
#include "lcm_drv.h"
#include "disp_event.h"
#include "disp_helper.h"

typedef struct _OVL_CONFIG_STRUCT
{
    unsigned int ovl_index;
    unsigned int layer;
	unsigned int layer_en;
    enum OVL_LAYER_SOURCE source;
    unsigned int fmt;
    unsigned long addr;  
    unsigned long vaddr;
    unsigned int src_x;
    unsigned int src_y;
    unsigned int src_w;
    unsigned int src_h;
    unsigned int src_pitch;
    unsigned int dst_x;
    unsigned int dst_y;
    unsigned int dst_w;
    unsigned int dst_h;                  // clip region
    unsigned int keyEn;
    unsigned int key; 
    unsigned int aen; 
    unsigned char alpha;
    
    unsigned int sur_aen;
    unsigned int src_alpha;
    unsigned int dst_alpha;

    unsigned int isTdshp;
    unsigned int isDirty;

    unsigned int buff_idx;
    unsigned int identity;
    unsigned int connected_type;
    unsigned int security;
    unsigned int yuv_range;
}OVL_CONFIG_STRUCT;

typedef struct _OVL_BASIC_STRUCT
{
	unsigned int layer;
	unsigned int layer_en;
	unsigned int fmt;
    unsigned long addr;
	unsigned int src_w;
    unsigned int src_h;
    unsigned int src_pitch;
    unsigned int bpp;
}OVL_BASIC_STRUCT;

typedef struct _RDMA_BASIC_STRUCT
{
    unsigned long addr;
	unsigned int src_w;
    unsigned int src_h;
    unsigned int bpp;
}RDMA_BASIC_STRUCT;

typedef struct _RDMA_CONFIG_STRUCT
{
    unsigned idx;            // instance index
    DpColorFormat inputFormat;
    unsigned long address;
    unsigned pitch;
    unsigned width; 
    unsigned height; 
}RDMA_CONFIG_STRUCT;

typedef struct _WDMA_CONFIG_STRUCT
{
    unsigned srcWidth; 
    unsigned srcHeight;     // input
    unsigned clipX; 
    unsigned clipY; 
    unsigned clipWidth; 
    unsigned clipHeight;    // clip
    DpColorFormat outputFormat; 
    unsigned long dstAddress; 
    unsigned dstPitch;     // output
    unsigned int useSpecifiedAlpha; 
    unsigned char alpha;
}WDMA_CONFIG_STRUCT;

typedef struct
{
    // for ovl
    bool ovl_dirty;
    bool rdma_dirty;
    bool wdma_dirty;
    bool dst_dirty;
    OVL_CONFIG_STRUCT  ovl_config[4];
    RDMA_CONFIG_STRUCT rdma_config;
    WDMA_CONFIG_STRUCT wdma_config;
    LCM_PARAMS dispif_config;	
    unsigned int lcm_bpp;
    unsigned int dst_w;
    unsigned int dst_h;
    unsigned int fps;
}disp_ddp_path_config;

typedef int (*ddp_module_notify)(DISP_MODULE_ENUM, DISP_PATH_EVENT);

typedef struct
{
	DISP_MODULE_ENUM	module;
	int (*init)(DISP_MODULE_ENUM module, void *handle);
	int (*deinit)(DISP_MODULE_ENUM module, void *handle);
	int (*config)(DISP_MODULE_ENUM module, disp_ddp_path_config *config, void *handle);
	int (*start)(DISP_MODULE_ENUM module, void *handle);
	int (*trigger)(DISP_MODULE_ENUM module, void *handle);
	int (*stop)(DISP_MODULE_ENUM module, void *handle);
	int (*reset)(DISP_MODULE_ENUM module, void *handle);
	int (*power_on)(DISP_MODULE_ENUM module, void *handle);
	int (*power_off)(DISP_MODULE_ENUM module, void *handle);
    int (*suspend)(DISP_MODULE_ENUM module, void *handle);
	int (*resume)(DISP_MODULE_ENUM module, void *handle);
	int (*is_idle)(DISP_MODULE_ENUM module);
	int (*is_busy)(DISP_MODULE_ENUM module);
	int (*dump_info)(DISP_MODULE_ENUM module, int level);
	int (*bypass)(DISP_MODULE_ENUM module, int bypass);	
	int (*build_cmdq)(DISP_MODULE_ENUM module, void *cmdq_handle, CMDQ_STATE state);	
	int (*set_lcm_utils)(DISP_MODULE_ENUM module, LCM_DRIVER *lcm_drv);
    int (*set_listener)(DISP_MODULE_ENUM module,ddp_module_notify notify);
    int (*cmd)(DISP_MODULE_ENUM module,int msg, unsigned long arg, void * handle);
	int (*ioctl)(DISP_MODULE_ENUM module, void *handle, unsigned int ioctl_cmd, unsigned long *params);
	int (*enable_irq)(DISP_MODULE_ENUM module, void *handle, DDP_IRQ_LEVEL irq_level);
} DDP_MODULE_DRIVER;

char* ddp_get_module_name(DISP_MODULE_ENUM module);
char* ddp_get_reg_module_name(DISP_REG_ENUM reg);
int   ddp_get_module_max_irq_bit(DISP_MODULE_ENUM module);

#endif
