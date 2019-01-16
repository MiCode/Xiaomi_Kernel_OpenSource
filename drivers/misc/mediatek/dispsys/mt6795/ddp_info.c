#define LOG_TAG "INFO"
#include"ddp_info.h"
#include"ddp_debug.h"
#include "ddp_log.h"

char* ddp_get_module_name(DISP_MODULE_ENUM module)
{
    switch(module)
    {
        case DISP_MODULE_UFOE   :    return "ufoe ";
        case DISP_MODULE_AAL    :    return "aal ";
        case DISP_MODULE_COLOR0 :    return "color0 ";
        case DISP_MODULE_COLOR1 :    return "color1 ";
        case DISP_MODULE_RDMA0  :    return "rdma0 ";
        case DISP_MODULE_RDMA1  :    return "rdma1 ";
        case DISP_MODULE_RDMA2  :    return "rdma2 ";
        case DISP_MODULE_WDMA0  :    return "wdma0 ";
        case DISP_MODULE_WDMA1  :    return "wdma1 ";
        case DISP_MODULE_OVL0   :    return "ovl0 ";
        case DISP_MODULE_OVL1   :    return "ovl1 ";
        case DISP_MODULE_GAMMA  :    return "gamma ";
        case DISP_MODULE_PWM0   :    return "pwm0 ";
        case DISP_MODULE_PWM1   :    return "pwm1 ";
        case DISP_MODULE_OD     :    return "od ";
        case DISP_MODULE_MERGE  :    return "merge ";
        case DISP_MODULE_SPLIT0 :    return "split0 ";
        case DISP_MODULE_SPLIT1 :    return "split1 ";
        case DISP_MODULE_DSI0   :    return "dsi0 ";
        case DISP_MODULE_DSI1   :    return "dsi1 ";
        case DISP_MODULE_DSIDUAL:    return "dsidual ";
        case DISP_MODULE_DPI    :    return "dpi ";
        case DISP_MODULE_SMI    :    return "smi ";
        case DISP_MODULE_CONFIG :    return "config ";
        case DISP_MODULE_CMDQ   :    return "cmdq ";
        case DISP_MODULE_MUTEX  :    return "mutex ";
        default:
             DDPMSG("invalid module id=%d", module);
             return "unknown";    	
    }
}
char* ddp_get_reg_module_name(DISP_REG_ENUM reg)
{
	switch (reg) {
	    case DISP_REG_CONFIG			: return "CONFIG";
	    case DISP_REG_OVL0				: return "OVL0";
	    case DISP_REG_OVL1				: return "OVL1";
	    case DISP_REG_RDMA0			  	: return "RDMA0";
	    case DISP_REG_RDMA1			  	: return "RDMA1";
	    case DISP_REG_RDMA2			  	: return "RDMA2";
	    case DISP_REG_WDMA0			  	: return "WDMA0";
	    case DISP_REG_WDMA1			  	: return "WDMA1";
	    case DISP_REG_COLOR0			: return "COLOR0";
	    case DISP_REG_COLOR1			: return "COLOR1";
	    case DISP_REG_AAL				: return "AAL";
	    case DISP_REG_GAMMA			  	: return "GAMMA";
	    case DISP_REG_MERGE			  	: return "MERGE";
	    case DISP_REG_SPLIT0			: return "SPLIT0";
	    case DISP_REG_SPLIT1			: return "SPLIT1";
	    case DISP_REG_UFOE				: return "UFOE";
	    case DISP_REG_DSI0				: return "DSI0";
	    case DISP_REG_DSI1				: return "DSI1";
	    case DISP_REG_DPI				: return "DPI";
	    case DISP_REG_PWM0				: return "PWM0";
	    case DISP_REG_PWM1				: return "PWM1";
	    case DISP_REG_MM_MUTEX			: return "MM_MUTEX";
	    case DISP_REG_SMI_LARB0	  		: return "SMI_LARB0";
	    case DISP_REG_SMI_COMMON		: return "SMI_COMMON";
	    case DISP_REG_OD				: return "OD";	

	    case DISP_REG_MIPITX0			: return "MIPITX0";	
	    case DISP_REG_MIPITX1			: return "MIPITX1";	
		
		default:
			DDPMSG("invalid module id=%d", reg);
			return "unknown";
	}

}

int ddp_get_module_max_irq_bit(DISP_MODULE_ENUM module)
{
    switch(module)
    {
        case DISP_MODULE_UFOE   :    return 0;
        case DISP_MODULE_AAL    :    return 1;
        case DISP_MODULE_COLOR0 :    return 2;
        case DISP_MODULE_COLOR1 :    return 2;
        case DISP_MODULE_RDMA0  :    return 5;
        case DISP_MODULE_RDMA1  :    return 5;
        case DISP_MODULE_RDMA2  :    return 5;
        case DISP_MODULE_WDMA0  :    return 1;
        case DISP_MODULE_WDMA1  :    return 1;
        case DISP_MODULE_OVL0   :    return 3;
        case DISP_MODULE_OVL1   :    return 3;
        case DISP_MODULE_GAMMA  :    return 0;
        case DISP_MODULE_PWM0   :    return 0;
        case DISP_MODULE_PWM1   :    return 0;
        case DISP_MODULE_OD     :    return 0;
        case DISP_MODULE_MERGE  :    return 0;
        case DISP_MODULE_SPLIT0 :    return 0;
        case DISP_MODULE_SPLIT1 :    return 0;
        case DISP_MODULE_DSI0   :    return 6;
        case DISP_MODULE_DSI1   :    return 6;
        case DISP_MODULE_DSIDUAL:    return 6;
        case DISP_MODULE_DPI    :    return 2;
        case DISP_MODULE_SMI    :    return 0;
        case DISP_MODULE_CONFIG :    return 0;
        case DISP_MODULE_CMDQ   :    return 0;
        case DISP_MODULE_MUTEX  :    return 14;
        default:
             DDPMSG("invalid module id=%d", module);
    }
    return 0;
}

unsigned int ddp_module_to_idx(int module)
{
    unsigned int id=0; 
    switch(module)
    {
        case DISP_MODULE_UFOE:
        case DISP_MODULE_AAL:
        case DISP_MODULE_COLOR0:
        case DISP_MODULE_RDMA0:
        case DISP_MODULE_WDMA0:
        case DISP_MODULE_OVL0:
        case DISP_MODULE_GAMMA:
        case DISP_MODULE_PWM0:
        case DISP_MODULE_OD:
        case DISP_MODULE_SPLIT0:
        case DISP_MODULE_DSI0:
        case DISP_MODULE_DPI:
          id = 0;
          break;
        
        case DISP_MODULE_COLOR1:
        case DISP_MODULE_RDMA1:
        case DISP_MODULE_WDMA1:
        case DISP_MODULE_OVL1:
        case DISP_MODULE_PWM1:
        case DISP_MODULE_SPLIT1:
        case DISP_MODULE_DSI1:
          id = 1;
          break;
        case DISP_MODULE_RDMA2:
	    case DISP_MODULE_DSIDUAL:
          id = 2;
          break;      
        default:
          DDPERR("ddp_module_to_idx, module=0x%x \n", module);
    }
    
    return id;
}


// dsi
extern DDP_MODULE_DRIVER ddp_driver_dsi0;
extern DDP_MODULE_DRIVER ddp_driver_dsi1;
extern DDP_MODULE_DRIVER ddp_driver_dsidual;
// dpi
extern DDP_MODULE_DRIVER ddp_driver_dpi;

// ovl
extern DDP_MODULE_DRIVER ddp_driver_ovl;
// rdma
extern DDP_MODULE_DRIVER ddp_driver_rdma;
// wdma
 extern DDP_MODULE_DRIVER ddp_driver_wdma;
// color
 extern DDP_MODULE_DRIVER ddp_driver_color;
// aal
 extern DDP_MODULE_DRIVER ddp_driver_aal;
// od
extern  DDP_MODULE_DRIVER ddp_driver_od; 
// ufoe
extern DDP_MODULE_DRIVER ddp_driver_ufoe;
// gamma
extern DDP_MODULE_DRIVER ddp_driver_gamma;

// split
extern DDP_MODULE_DRIVER ddp_driver_split;

// pwm
extern DDP_MODULE_DRIVER ddp_driver_pwm;

DDP_MODULE_DRIVER  * ddp_modules_driver[DISP_MODULE_NUM] = 
{
    &ddp_driver_ufoe, //DISP_MODULE_UFOE   ,
    &ddp_driver_aal,//DISP_MODULE_AAL    ,
    &ddp_driver_color,//DISP_MODULE_COLOR0 ,
    &ddp_driver_color,//DISP_MODULE_COLOR1 ,
    &ddp_driver_rdma,//DISP_MODULE_RDMA0  ,
    &ddp_driver_rdma,//DISP_MODULE_RDMA1  ,  // 5
    &ddp_driver_rdma,//DISP_MODULE_RDMA2  ,
    &ddp_driver_wdma,//DISP_MODULE_WDMA0  ,
    &ddp_driver_wdma,//DISP_MODULE_WDMA1  ,
    &ddp_driver_ovl,//DISP_MODULE_OVL0   ,
    &ddp_driver_ovl,//DISP_MODULE_OVL1   ,  // 10
    &ddp_driver_gamma,//DISP_MODULE_GAMMA  ,
    &ddp_driver_pwm,//DISP_MODULE_PWM0   ,
    &ddp_driver_pwm,//DISP_MODULE_PWM1   ,
    &ddp_driver_od,//DISP_MODULE_OD     ,
    0,//DISP_MODULE_MERGE  ,  // 15
    &ddp_driver_split,//DISP_MODULE_SPLIT0 ,
    &ddp_driver_split,//DISP_MODULE_SPLIT1 ,
    &ddp_driver_dsi0,//DISP_MODULE_DSI0,
    &ddp_driver_dsi1,//DISP_MODULE_DSI1,
    &ddp_driver_dsidual,//DISP_MODULE_DSIDUAL, // 20
    &ddp_driver_dpi,//DISP_MODULE_DPI,
    0,//DISP_MODULE_SMI, 
    0,//DISP_MODULE_CONFIG,
    0,//DISP_MODULE_CMDQ,
    0,//DISP_MODULE_MUTEX,	// 25
    0,//DISP_MODULE_UNKNOWN,
};
