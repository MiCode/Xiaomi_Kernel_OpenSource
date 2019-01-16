#define LOG_TAG "WDMA"
#include "ddp_log.h"

#include <mach/mt_clkmgr.h>
#include <linux/delay.h>
#include "ddp_reg.h"
#include "ddp_matrix_para.h"
#include "ddp_info.h"
#include "ddp_wdma.h"
#include "ddp_color_format.h"

#define WDMA_COLOR_SPACE_RGB (0)
#define WDMA_COLOR_SPACE_YUV (1)



enum WDMA_OUTPUT_FORMAT {
    WDMA_OUTPUT_FORMAT_BGR565   = 0x00,   // basic format
    WDMA_OUTPUT_FORMAT_RGB888   = 0x01,
    WDMA_OUTPUT_FORMAT_RGBA8888 = 0x02,
    WDMA_OUTPUT_FORMAT_ARGB8888 = 0x03,
    WDMA_OUTPUT_FORMAT_VYUY     = 0x04,
    WDMA_OUTPUT_FORMAT_YVYU     = 0x05,
    WDMA_OUTPUT_FORMAT_YONLY    = 0x07,
    WDMA_OUTPUT_FORMAT_YV12     = 0x08,
    WDMA_OUTPUT_FORMAT_NV21     = 0x0c,

    WDMA_OUTPUT_FORMAT_UNKNOWN  = 0x100,
};


#define ALIGN_TO(x, n)  \
	(((x) + ((n) - 1)) & ~((n) - 1))

static char* wdma_get_status(unsigned int status)
{
    switch(status){
        case 0x1:   return "idle"; 
        case 0x2:   return "clear"; 
        case 0x4:   return "prepare"; 
        case 0x8:   return "prepare"; 
        case 0x10:  return "data_running"; 
        case 0x20:  return "eof_wait"; 
        case 0x40:  return "soft_reset_wait"; 
        case 0x80:  return "eof_done"; 
        case 0x100: return "soft_reset_done";
        case 0x200: return "frame_complete";
    }
    return "unknow";

}

static unsigned int wdma_index(DISP_MODULE_ENUM module)
{
    int idx = 0;
    switch(module) {
        case DISP_MODULE_WDMA0:
            idx = 0;
            break;
        case DISP_MODULE_WDMA1:
            idx = 1; 
            break;
        default:
            printk("[DDP] error: invalid wdma module=%d \n", module);// invalid module
            ASSERT(0);
    }
    return idx;
}

static int wdma_start(DISP_MODULE_ENUM module,void * handle) 
{
    unsigned int idx = wdma_index(module);
    DISP_REG_SET(handle,idx*DISP_WDMA_INDEX_OFFSET+DISP_REG_WDMA_INTEN, 0x03);
    DISP_REG_SET(handle,idx*DISP_WDMA_INDEX_OFFSET+DISP_REG_WDMA_EN, 0x01);

    return 0;
}

static int wdma_stop(DISP_MODULE_ENUM module,void * handle) 
{
    unsigned int idx = wdma_index(module);

    DISP_REG_SET(handle,idx*DISP_WDMA_INDEX_OFFSET+DISP_REG_WDMA_INTEN, 0x00);
    DISP_REG_SET(handle,idx*DISP_WDMA_INDEX_OFFSET+DISP_REG_WDMA_EN, 0x00);
    DISP_REG_SET(handle,idx*DISP_WDMA_INDEX_OFFSET+DISP_REG_WDMA_INTSTA, 0x00);
    
    return 0;
}

static int wdma_reset(DISP_MODULE_ENUM module,void * handle) 
{
    unsigned int delay_cnt = 0;
    unsigned int idx = wdma_index(module);
    DISP_REG_SET(handle,idx*DISP_WDMA_INDEX_OFFSET+DISP_REG_WDMA_RST, 0x01); // trigger soft reset
    if (!handle) {
         while ((DISP_REG_GET(idx*DISP_WDMA_INDEX_OFFSET+DISP_REG_WDMA_FLOW_CTRL_DBG)&0x1)==0) {
             delay_cnt++;
             udelay(10);
             if (delay_cnt>2000) {
                 DDPERR("wdma%d reset timeout!\n", idx); 
                 break;
             }
        }
    }else{
        //add comdq polling
    }
    DISP_REG_SET(handle,idx*DISP_WDMA_INDEX_OFFSET+DISP_REG_WDMA_RST , 0x0); // trigger soft reset

    return 0;
}

static int wdma_config_uv(DISP_MODULE_ENUM module, unsigned long uAddr, unsigned long vAddr, unsigned int uvpitch,void * handle)
{
    unsigned int idx = wdma_index(module);
    unsigned int idx_offst = idx*DISP_WDMA_INDEX_OFFSET;
    DISP_REG_SET(handle,idx_offst+DISP_REG_WDMA_DST_ADDR1, uAddr);
    DISP_REG_SET(handle,idx_offst+DISP_REG_WDMA_DST_ADDR2, vAddr);
    DISP_REG_SET_FIELD(handle,DST_W_IN_BYTE_FLD_DST_W_IN_BYTE, idx_offst+DISP_REG_WDMA_DST_UV_PITCH, uvpitch);
    return 0;
}

static int wdma_config_yuv420(DISP_MODULE_ENUM module,
                                  DpColorFormat fmt,
                                  unsigned int dstPitch,
                                  unsigned int Height,
                                  unsigned dstAddress,
                                  void * handle)
{
    unsigned int u_add = 0;
    unsigned int v_add = 0;
    unsigned int uv_add = 0;
    unsigned int c_stride = 0;
    unsigned int y_size = 0;
    unsigned int c_size = 0;
    unsigned int stride = dstPitch;
    if(fmt == eYV12)
    {
        y_size = stride * Height;
        c_stride = ALIGN_TO(stride/2,16);
        c_size = c_stride * Height/2;
        u_add = dstAddress + y_size;
        v_add = dstAddress + y_size + c_size;
        wdma_config_uv(module,u_add,v_add,c_stride,handle);
    }
    else if(fmt == eYV21)
    {
        y_size = stride * Height;
        c_stride = ALIGN_TO(stride/2,16);
        c_size = c_stride * Height/2;
        u_add = dstAddress + y_size;
        v_add = dstAddress + y_size + c_size;
        wdma_config_uv(module,u_add,v_add,c_stride,handle);
    }
    else if(fmt == eNV12 || fmt == eNV21)
    {
        y_size = stride * Height;
        c_stride = stride/2;
        uv_add = dstAddress + y_size;
        wdma_config_uv(module,uv_add,0,c_stride,handle);       
    }
    return 0;
}
static int wdma_config(DISP_MODULE_ENUM module,
               unsigned srcWidth, 
               unsigned srcHeight,
               unsigned clipX, 
               unsigned clipY, 
               unsigned clipWidth, 
               unsigned clipHeight,
               DpColorFormat  out_format, 
               unsigned long dstAddress, 
               unsigned dstPitch,               
               unsigned int useSpecifiedAlpha, 
               unsigned char alpha,
               void * handle) 
{
    unsigned int idx                     = wdma_index(module);
    unsigned int output_swap             = fmt_swap(out_format);
    unsigned int output_color_space      = fmt_color_space(out_format);
    unsigned int bpp                     = fmt_bpp(out_format);
    unsigned int out_fmt_reg             = fmt_hw_value(out_format); 
    unsigned int yuv444_to_yuv422        = 0;
    int color_matrix                     = 0x2; //0010 RGB_TO_BT601
    unsigned int idx_offst = idx*DISP_WDMA_INDEX_OFFSET;
    DDPDBG("module %s, src(w=%d,h=%d), clip(x=%d,y=%d,w=%d,h=%d),out_fmt=%s,dst_address=0x%lx,dst_p=%d,spific_alfa= %d,alpa=%d,handle=%p\n",
        ddp_get_module_name(module),srcWidth,srcHeight,
        clipX,clipY,clipWidth,clipHeight,  
        fmt_string(out_format),dstAddress,dstPitch,  
        useSpecifiedAlpha,alpha,handle);
    
    // should use OVL alpha instead of sw config
    DISP_REG_SET(handle,idx_offst+DISP_REG_WDMA_SRC_SIZE, srcHeight<<16 | srcWidth);
    DISP_REG_SET(handle,idx_offst+DISP_REG_WDMA_CLIP_COORD, clipY<<16 | clipX);
    DISP_REG_SET(handle,idx_offst+DISP_REG_WDMA_CLIP_SIZE, clipHeight<<16 | clipWidth);
    DISP_REG_SET_FIELD(handle,CFG_FLD_OUT_FORMAT,idx_offst+DISP_REG_WDMA_CFG, out_fmt_reg);
    
    if(output_color_space == WDMA_COLOR_SPACE_YUV) {
        yuv444_to_yuv422 = fmt_is_yuv422(out_format);
        // set DNSP for UYVY and YUV_3P format for better quality
        DISP_REG_SET_FIELD(handle,CFG_FLD_DNSP_SEL, idx_offst+DISP_REG_WDMA_CFG, yuv444_to_yuv422);
        if(fmt_is_yuv420(out_format))
        {
            wdma_config_yuv420(module,out_format,dstPitch,clipHeight,dstAddress,handle);
        }
        /*user internal matrix*/
        DISP_REG_SET_FIELD(handle,CFG_FLD_EXT_MTX_EN, idx_offst+DISP_REG_WDMA_CFG, 0);                         
        DISP_REG_SET_FIELD(handle,CFG_FLD_CT_EN, idx_offst+DISP_REG_WDMA_CFG, 1);
        DISP_REG_SET_FIELD(handle,CFG_FLD_INT_MTX_SEL, idx_offst+DISP_REG_WDMA_CFG, color_matrix);  
    }
    else
    {
        DISP_REG_SET_FIELD(handle,CFG_FLD_EXT_MTX_EN, idx_offst+DISP_REG_WDMA_CFG, 0);                         
        DISP_REG_SET_FIELD(handle,CFG_FLD_CT_EN, idx_offst+DISP_REG_WDMA_CFG, 0);  
    }
    DISP_REG_SET_FIELD(handle,CFG_FLD_SWAP, idx_offst+DISP_REG_WDMA_CFG, output_swap);
    DISP_REG_SET(handle, idx_offst+DISP_REG_WDMA_DST_ADDR0, dstAddress);
    DISP_REG_SET(handle, idx_offst+DISP_REG_WDMA_DST_W_IN_BYTE, dstPitch);
    DISP_REG_SET_FIELD(handle,ALPHA_FLD_A_SEL, idx_offst+DISP_REG_WDMA_ALPHA, useSpecifiedAlpha);
    DISP_REG_SET_FIELD(handle,ALPHA_FLD_A_VALUE, idx_offst+DISP_REG_WDMA_ALPHA, alpha);
    /*ultra enable*/
    DISP_REG_SET_FIELD(handle,BUF_CON1_FLD_ULTRA_ENABLE, idx_offst+DISP_REG_WDMA_BUF_CON1, 1);
    return 0;
}

static int wdma_clock_on(DISP_MODULE_ENUM module,void * handle)
{
    unsigned int idx = wdma_index(module);
    DDPMSG("wmda%d_clock_on \n",idx); 
    if(idx == 0){
        enable_clock(MT_CG_DISP0_DISP_WDMA0, "WDMA0");
    }else{
        enable_clock(MT_CG_DISP0_DISP_WDMA1, "WDMA1");    
    }
    return 0;
}
static int wdma_clock_off(DISP_MODULE_ENUM module,void * handle)
{
    unsigned int idx = wdma_index(module);
    DDPMSG("wdma%d_clock_off\n",idx);
    if(idx == 0){
        disable_clock(MT_CG_DISP0_DISP_WDMA0, "WDMA0");
    }else{
        disable_clock(MT_CG_DISP0_DISP_WDMA1, "WDMA1");    
    }
    return 0;
}

void wdma_dump_analysis(DISP_MODULE_ENUM module)
{
    unsigned int index = wdma_index(module);
    unsigned int idx_offst = index*DISP_WDMA_INDEX_OFFSET;
    DDPDUMP("==DISP WDMA%d ANALYSIS==\n",index);
    DDPDUMP("wdma%d:en=%d,w=%d,h=%d,clip=(%d,%d,%d,%d),pitch=(W=%d,UV=%d),addr=(0x%x,0x%x,0x%x),fmt=%s\n",
        index,
        DISP_REG_GET(DISP_REG_WDMA_EN+idx_offst),
        DISP_REG_GET(DISP_REG_WDMA_SRC_SIZE+idx_offst)&0x3fff,
        (DISP_REG_GET(DISP_REG_WDMA_SRC_SIZE+idx_offst)>>16)&0x3fff,
        
        DISP_REG_GET(DISP_REG_WDMA_CLIP_COORD+idx_offst)&0x3fff, 
        (DISP_REG_GET(DISP_REG_WDMA_CLIP_COORD+idx_offst)>>16)&0x3fff,
        DISP_REG_GET(DISP_REG_WDMA_CLIP_SIZE+idx_offst)&0x3fff, 
        (DISP_REG_GET(DISP_REG_WDMA_CLIP_SIZE+idx_offst)>>16)&0x3fff,

        DISP_REG_GET(DISP_REG_WDMA_DST_W_IN_BYTE+idx_offst),
        DISP_REG_GET(DISP_REG_WDMA_DST_UV_PITCH+idx_offst),

        DISP_REG_GET(DISP_REG_WDMA_DST_ADDR0+idx_offst),
        DISP_REG_GET(DISP_REG_WDMA_DST_ADDR1+idx_offst),
        DISP_REG_GET(DISP_REG_WDMA_DST_ADDR2+idx_offst),
        
        fmt_string(fmt_type(
            (DISP_REG_GET(DISP_REG_WDMA_CFG+idx_offst)>>4)&0xf,
            (DISP_REG_GET(DISP_REG_WDMA_CFG+idx_offst)>>11)&0x1
        ))
    );
    DDPDUMP("wdma%d:status=%s,in_req=%d,in_ack=%d, exec=%d, input_pixel=(L:%d,P:%d)\n",
        index,
        wdma_get_status(DISP_REG_GET_FIELD(FLOW_CTRL_DBG_FLD_WDMA_STA_FLOW_CTRL, DISP_REG_WDMA_FLOW_CTRL_DBG+idx_offst)),
        DISP_REG_GET_FIELD(EXEC_DBG_FLD_WDMA_IN_REQ, DISP_REG_WDMA_FLOW_CTRL_DBG+idx_offst),
        DISP_REG_GET_FIELD(EXEC_DBG_FLD_WDMA_IN_ACK, DISP_REG_WDMA_FLOW_CTRL_DBG+idx_offst),
        DISP_REG_GET(DISP_REG_WDMA_EXEC_DBG+idx_offst)&0x1f,
        (DISP_REG_GET(DISP_REG_WDMA_CT_DBG+idx_offst)>>16)&0xffff,
        DISP_REG_GET(DISP_REG_WDMA_CT_DBG+idx_offst)&0xffff
    );
   return;
}

void wdma_dump_reg(DISP_MODULE_ENUM module)
{
    unsigned int idx = wdma_index(module);
    unsigned int off_sft = idx*DISP_WDMA_INDEX_OFFSET;
    DDPDUMP("==DISP WDMA%d REGS==\n", idx);

	DDPDUMP("WDMA:0x000=0x%08x,0x004=0x%08x,0x008=0x%08x,0x00c=0x%08x\n",
	                        DISP_REG_GET(DISP_REG_WDMA_INTEN+off_sft),
	                        DISP_REG_GET(DISP_REG_WDMA_INTSTA+off_sft),
	                        DISP_REG_GET(DISP_REG_WDMA_EN+off_sft),
	                        DISP_REG_GET(DISP_REG_WDMA_RST+off_sft));
	
	DDPDUMP("WDMA:0x010=0x%08x,0x014=0x%08x,0x018=0x%08x,0x01c=0x%08x\n",
                        DISP_REG_GET(DISP_REG_WDMA_SMI_CON+off_sft),
                        DISP_REG_GET(DISP_REG_WDMA_CFG+off_sft),
                        DISP_REG_GET(DISP_REG_WDMA_SRC_SIZE+off_sft),
                        DISP_REG_GET(DISP_REG_WDMA_CLIP_SIZE+off_sft));
    
	DDPDUMP("WDMA:0x020=0x%08x,0x028=0x%08x,0x02c=0x%08x,0x038=0x%08x\n",
                        DISP_REG_GET(DISP_REG_WDMA_CLIP_COORD+off_sft),
                        DISP_REG_GET(DISP_REG_WDMA_DST_W_IN_BYTE+off_sft),
                        DISP_REG_GET(DISP_REG_WDMA_ALPHA+off_sft),
                        DISP_REG_GET(DISP_REG_WDMA_BUF_CON1+off_sft));    

	DDPDUMP("WDMA:0x03c=0x%08x,0x058=0x%08x,0x05c=0x%08x,0x060=0x%08x\n",
                        DISP_REG_GET(DISP_REG_WDMA_BUF_CON2+off_sft),
                        DISP_REG_GET(DISP_REG_WDMA_PRE_ADD0+off_sft),
                        DISP_REG_GET(DISP_REG_WDMA_PRE_ADD2+off_sft),
                        DISP_REG_GET(DISP_REG_WDMA_POST_ADD0+off_sft)); 
    
	DDPDUMP("WDMA:0x064=0x%08x,0x078=0x%08x,0x080=0x%08x,0x084=0x%08x\n",
                        DISP_REG_GET(DISP_REG_WDMA_POST_ADD2+off_sft),
                        DISP_REG_GET(DISP_REG_WDMA_DST_UV_PITCH+off_sft),
                        DISP_REG_GET(DISP_REG_WDMA_DST_ADDR_OFFSET0+off_sft),
                        DISP_REG_GET(DISP_REG_WDMA_DST_ADDR_OFFSET1+off_sft));     
    
	DDPDUMP("WDMA:0x088=0x%08x,0x0a0=0x%08x,0x0a4=0x%08x,0x0a8=0x%08x\n",
                        DISP_REG_GET(DISP_REG_WDMA_DST_ADDR_OFFSET2+off_sft),
                        DISP_REG_GET(DISP_REG_WDMA_FLOW_CTRL_DBG+off_sft),
                        DISP_REG_GET(DISP_REG_WDMA_EXEC_DBG+off_sft),
                        DISP_REG_GET(DISP_REG_WDMA_CT_DBG+off_sft)); 

 	DDPDUMP("WDMA:0x0ac=0x%08x,0xf00=0x%08x,0xf04=0x%08x,0xf08=0x%08x,\n",
                        DISP_REG_GET(DISP_REG_WDMA_DEBUG+off_sft),
                        DISP_REG_GET(DISP_REG_WDMA_DST_ADDR0+off_sft),
                        DISP_REG_GET(DISP_REG_WDMA_DST_ADDR1+off_sft),
                        DISP_REG_GET(DISP_REG_WDMA_DST_ADDR2+off_sft));   
    return ;
}

static int wdma_dump(DISP_MODULE_ENUM module,int level)
{
    wdma_dump_analysis(module);
    wdma_dump_reg(module);

    return 0;
}

static int wdma_check_input_param(WDMA_CONFIG_STRUCT *config)
{
    int unique = fmt_hw_value(config->outputFormat);

    if (unique >  WDMA_OUTPUT_FORMAT_YV12 && unique != WDMA_OUTPUT_FORMAT_NV21){
        DDPERR("wdma parameter invalidate outfmt 0x%x\n",config->outputFormat);
        return -1;
    }
        
    if(config->dstAddress==0 ||
       config->srcWidth==0   ||
       config->srcHeight==0)
    {
        DDPERR("wdma parameter invalidate, addr=0x%lx, w=%d, h=%d\n",
               config->dstAddress, 
               config->srcWidth,
               config->srcHeight);
        return -1;
    }
    return 0;
}

static int wdma_config_l(DISP_MODULE_ENUM module, disp_ddp_path_config* pConfig, void *handle)
{

    WDMA_CONFIG_STRUCT* config = &pConfig->wdma_config;
    if (!pConfig->wdma_dirty) {
        return 0;
    }

    if (wdma_check_input_param(config) == 0)
        wdma_config(module, 
                   config->srcWidth, 
                   config->srcHeight, 
                   config->clipX, 
                   config->clipY, 
                   config->clipWidth, 
                   config->clipHeight, 
                   config->outputFormat, 
                   config->dstAddress, 
                   config->dstPitch, 
                   config->useSpecifiedAlpha, 
                   config->alpha,
                   handle);      
    return 0;
}

// wdma

DDP_MODULE_DRIVER ddp_driver_wdma =
{
    .module          = DISP_MODULE_WDMA0,
    .init            = wdma_clock_on,
    .deinit          = wdma_clock_off,
    .config          = wdma_config_l,
    .start 	     = wdma_start,
    .trigger         = NULL,
    .stop	     = wdma_stop,
    .reset           = wdma_reset,
    .power_on        = wdma_clock_on,
    .power_off       = wdma_clock_off,
    .is_idle         = NULL,
    .is_busy         = NULL,
    .dump_info       = wdma_dump,
    .bypass          = NULL,
    .build_cmdq      = NULL,
    .set_lcm_utils   = NULL,
};
