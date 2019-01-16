#ifndef _EXTD_DDP_H_
#define _EXTD_DDP_H_

#include "ddp_hal.h"
#include "ddp_manager.h"

typedef enum
{
	EXTD_DIRECT_LINK_MODE,
	EXTD_DECOUPLE_MODE,
	EXTD_SINGLE_LAYER_MODE,
	EXTD_DEBUG_RDMA_DPI_MODE
}EXT_DISP_PATH_MODE;


// ---------------------------------------------------------------------------
#if 0

#define DISP_RET_0_IF_NULL(x) 			\
	do{									\
		if((x) == NULL) {DISPERR("%s is NULL, return 0\n", #x); return 0;}\
	} while(0)
		
		
#define DISP_RET_VOID_IF_NULL(x) 			\
	do{ 								\
		if((x) == NULL) {DISPERR("%s is NULL, return 0\n", #x); return;}\
	} while(0)
						
#define DISP_RET_VOID_IF_0(x) 			\
	do{ 								\
		if((x) == 0) {DISPERR("%s is NULL, return 0\n", #x); return;}\
	} while(0)
						
#define DISP_RET_0_IF_0(x) 			\
		do{ 								\
			if((x) == 0) {DISPERR("%s is NULL, return 0\n", #x); return 0;}\
		} while(0)				

#define DISP_RET_NULL_IF_0(x) 			\
	do{ 								\
		if((x) == 0) {DISPERR("%s is NULL, return 0\n", #x); return NULL;}\
	} while(0)

		
#define DISP_CHECK_RET(expr)                                                \
    do {                                                                    \
        DISP_STATUS ret = (expr);                                           \
        if (DISP_STATUS_OK != ret) {                                        \
            DISP_LOG_PRINT(ANDROID_LOG_ERROR, "COMMON", "[ERROR][mtkfb] DISP API return error code: 0x%x\n"      \
                   "  file : %s, line : %d\n"                               \
                   "  expr : %s\n", ret, __FILE__, __LINE__, #expr);        \
        }                                                                   \
    } while (0)


// ---------------------------------------------------------------------------

#define ASSERT_LAYER    (DDP_OVL_LAYER_MUN-1)
#define DISP_DEFAULT_UI_LAYER_ID (DDP_OVL_LAYER_MUN-1)
#define DISP_CHANGED_UI_LAYER_ID (DDP_OVL_LAYER_MUN-2)

typedef enum
{	
   DISP_STATUS_OK = 0,

   DISP_STATUS_NOT_IMPLEMENTED,
   DISP_STATUS_ALREADY_SET,
   DISP_STATUS_ERROR,
} DISP_STATUS;


typedef enum {
   DISP_STATE_IDLE = 0,
   DISP_STATE_BUSY,
} DISP_STATE;

typedef enum{
	DISP_OP_PRE = 0,
	DISP_OP_NORMAL,
	DISP_OP_POST,
}DISP_OP_STATE;



typedef enum
{
	DISPLAY_HAL_IOCTL_SET_CMDQ = 0xff00,
	DISPLAY_HAL_IOCTL_ENABLE_CMDQ,
	DISPLAY_HAL_IOCTL_DUMP,
	DISPLAY_HAL_IOCTL_PATTERN,
}DISPLAY_HAL_IOCTL;
#endif

typedef enum
{	
   EXT_DISP_STATUS_OK = 0,

   EXT_DISP_STATUS_NOT_IMPLEMENTED,
   EXT_DISP_STATUS_ALREADY_SET,
   EXT_DISP_STATUS_ERROR,
} EXT_DISP_STATUS;

typedef struct
{
	unsigned int layer;
	unsigned int layer_en;
	unsigned int buffer_source;
	unsigned int fmt;
	unsigned int addr;  
	unsigned int addr_sub_u;
	unsigned int addr_sub_v;
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
    unsigned int  sur_aen;
	unsigned int  src_alpha;
	unsigned int  dst_alpha;

	unsigned int isTdshp;
	unsigned int isDirty;

	unsigned int buff_idx;
	unsigned int identity;
	unsigned int connected_type;
	unsigned int security;
	unsigned int dirty;
 	unsigned int yuv_range;
}ext_disp_input_config;


int ext_disp_init(char *lcm_name,  unsigned int session);
int ext_disp_deinit(char *lcm_name);
int ext_disp_config(unsigned int pa, unsigned int mva);
int ext_disp_suspend(void);
int ext_disp_resume(void);
EXT_DISP_PATH_MODE get_ext_disp_path_mode();


int ext_disp_get_width(void);
int ext_disp_get_height(void);
int ext_disp_get_bpp(void);
int ext_disp_get_pages(void);
unsigned int ext_disp_get_sess_id();


int ext_disp_set_overlay_layer(ext_disp_input_config* input);
int ext_disp_is_alive(void);
int ext_disp_is_sleepd(void);
int ext_disp_wait_for_vsync(void *config);
int ext_disp_config_input(ext_disp_input_config* input);
int ext_disp_config_input_multiple(ext_disp_input_config* input, int idx);
int ext_disp_trigger(int blocking, void *callback, unsigned int userdata);

int ext_disp_get_info(void *info);
int ext_disp_is_video_mode(void);
CMDQ_SWITCH ext_disp_cmdq_enabled(void);
int ext_disp_switch_cmdq_cpu(CMDQ_SWITCH use_cmdq);
int ext_disp_diagnose(void);

#endif
