#ifndef _DDP_RDMA_API_H_
#define _DDP_RDMA_API_H_
#include "ddp_info.h"

#define RDMA_INSTANCES  3
#define RDMA_MAX_WIDTH  4095
#define RDMA_MAX_HEIGHT 4095

enum RDMA_OUTPUT_FORMAT {
    RDMA_OUTPUT_FORMAT_ARGB   = 0,
    RDMA_OUTPUT_FORMAT_YUV444 = 1,
};

enum RDMA_MODE {
    RDMA_MODE_DIRECT_LINK = 0,
    RDMA_MODE_MEMORY      = 1,
};

// start module
int rdma_start(DISP_MODULE_ENUM module,void * handle);

// stop module
int rdma_stop(DISP_MODULE_ENUM module,void * handle);

// reset module
int rdma_reset(DISP_MODULE_ENUM module,void * handle);

// configu module
int rdma_config(DISP_MODULE_ENUM module,
                    enum RDMA_MODE mode,
                    unsigned long address, 
                    DpColorFormat inFormat,
                    unsigned pitch,
                    unsigned width, 
                    unsigned height, 
                    unsigned ufoe_enable,
                    void * handle); // ourput setting

void rdma_set_target_line(DISP_MODULE_ENUM module, unsigned int line,void * handle);

void rdma_get_address(DISP_MODULE_ENUM module, unsigned long * data);

void rdma_dump_reg(DISP_MODULE_ENUM module);
void rdma_dump_analysis(DISP_MODULE_ENUM module);
void rdma_get_info(int idx,RDMA_BASIC_STRUCT * info );

#endif
