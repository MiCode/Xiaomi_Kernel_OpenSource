#define LOG_TAG "SPLIT" 
#include "ddp_log.h"

#include <mach/mt_clkmgr.h>
#include <linux/delay.h>

#include "ddp_info.h"
#include "ddp_hal.h"
#include "ddp_reg.h"
#include "ddp_split.h"


static unsigned int split_index(DISP_MODULE_ENUM module)
{
    int idx = 0;
    switch(module) {
        case DISP_MODULE_SPLIT0:
            idx = 0;
            break;
        case DISP_MODULE_SPLIT1:
            idx = 1; 
            break;
        default:
            DDPERR("invalid split module=%d \n", module);// invalid module
            ASSERT(0);
    }
    return idx;
}

static char * split_state(unsigned int state) {
    switch(state)
    {
        case 1:
            return "idle";
        case 2:
            return "wait";
        case 4:
            return "busy";
        default:
            return "unknow";
    }
    return "unknow";
}

static int split_clock_on(DISP_MODULE_ENUM module,void * handle) {
    int idx = split_index(module);
    enable_clock(MT_CG_DISP0_DISP_SPLIT0+idx , "split");
    DDPMSG("Split%dClockOn CG 0x%x \n",idx, DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0)); 
	return 0;
}

static int split_clock_off(DISP_MODULE_ENUM module,void * handle) {
    int idx = split_index(module);
    DDPMSG("Split%dClockoff CG\n",idx);     
    disable_clock(MT_CG_DISP0_DISP_SPLIT0+idx , "split");
    return 0;
}

static int split_init(DISP_MODULE_ENUM module,void * handle) {
    split_clock_on(module,handle);
    return 0;
}

static int split_deinit(DISP_MODULE_ENUM module,void * handle) {
    split_clock_off(module,handle);
    return 0;
}

static int split_start(DISP_MODULE_ENUM module,void * handle) {
    int idx = split_index(module);
    DISP_REG_SET(handle,idx*DISP_SPLIT_INDEX_OFFSET+DISP_REG_SPLIT_ENABLE, 0x01);
    return 0;
}

static int split_stop(DISP_MODULE_ENUM module,void * handle) {
    int idx = split_index(module);
    DISP_REG_SET(handle,idx*DISP_SPLIT_INDEX_OFFSET+DISP_REG_SPLIT_ENABLE, 0x0);
    return 0;
}

static int split_busy(DISP_MODULE_ENUM module) {
    int idx = split_index(module);
    unsigned int state = DISP_REG_GET_FIELD(DEBUG_FLD_SPLIT_FSM,DISP_REG_SPLIT_DEBUG+DISP_SPLIT_INDEX_OFFSET*idx);
    return (state & 0x4);
}

static int split_idle(DISP_MODULE_ENUM module) {
    int idx = split_index(module);
    unsigned int state = DISP_REG_GET_FIELD(DEBUG_FLD_SPLIT_FSM,DISP_REG_SPLIT_DEBUG+DISP_SPLIT_INDEX_OFFSET*idx);
    return (state & 0x3);
}

int split_reset(DISP_MODULE_ENUM module,void * handle) {
    unsigned int delay_cnt = 0;
    int idx = split_index(module);
    DISP_REG_SET(handle,idx*DISP_SPLIT_INDEX_OFFSET+DISP_REG_SPLIT_SW_RESET, 0x1);
    DISP_REG_SET(handle,idx*DISP_SPLIT_INDEX_OFFSET+DISP_REG_SPLIT_SW_RESET, 0x0);
    /*always use cpu do reset*/
    while((DISP_REG_GET_FIELD(DEBUG_FLD_SPLIT_FSM, DISP_REG_SPLIT_DEBUG+DISP_SPLIT_INDEX_OFFSET*idx) 
           & 0x3)==0)
    {
        delay_cnt++;
        udelay(10);
        if(delay_cnt>2000)
        {
            DDPERR("split%d_reset() timeout!\n",idx);
            break;
        }
    }
    return 0;
}

static int split_dump_regs(DISP_MODULE_ENUM module) {
    int idx = split_index(module);
    DDPMSG("== DISP SPLIT%d REGS  ==\n", idx); 
    DDPMSG("(0x000)S_ENABLE       =0x%x\n", DISP_REG_GET(DISP_REG_SPLIT_ENABLE+DISP_SPLIT_INDEX_OFFSET*idx));             
    DDPMSG("(0x004)S_SW_RST       =0x%x\n", DISP_REG_GET(DISP_REG_SPLIT_SW_RESET+DISP_SPLIT_INDEX_OFFSET*idx));             
    DDPMSG("(0x008)S_DEBUG        =0x%x\n", DISP_REG_GET(DISP_REG_SPLIT_DEBUG+DISP_SPLIT_INDEX_OFFSET*idx));             
    return 0;
}

static int split_dump_analysis(DISP_MODULE_ENUM module) {
    int idx = split_index(module);
    DDPMSG("== DISP SPLIT%d ANALYSIS ==\n", idx);  
    unsigned int pixel = DISP_REG_GET_FIELD(DEBUG_FLD_IN_PIXEL_CNT,DISP_REG_SPLIT_DEBUG+DISP_SPLIT_INDEX_OFFSET*idx);
    unsigned int state = DISP_REG_GET_FIELD(DEBUG_FLD_SPLIT_FSM,DISP_REG_SPLIT_DEBUG+DISP_SPLIT_INDEX_OFFSET*idx);  
    
    DDPMSG("cur_pixel %u, state %s\n",pixel,split_state(state));    
    return 0;
}

static int split_dump(DISP_MODULE_ENUM module,int level) {
    split_dump_analysis(module);
    split_dump_regs(module);

    return 0;
}


DDP_MODULE_DRIVER ddp_driver_split =
{
    .init            = split_init,
    .deinit          = split_deinit,
	.config          = NULL,
	.start 	         = split_start,
	.trigger         = NULL,
	.stop	         = split_stop,
	.reset           = split_reset,
	.power_on        = split_clock_on,
    .power_off       = split_clock_off,
    .is_idle         = split_idle,
    .is_busy         = split_busy,
    .dump_info       = split_dump,
    .bypass          = NULL,
    .build_cmdq      = NULL,
    .set_lcm_utils   = NULL,
};
