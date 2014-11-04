#ifndef __GDC_GLOBAL_H_INCLUDED__
#define __GDC_GLOBAL_H_INCLUDED__

#define IS_GDC_VERSION_1

#include <gdc_defs.h>

/*
 * Storage addresses for packed data transfer
 */
#define GDC_PARAM_ICX_LEFT_ROUNDED_IDX            0
#define GDC_PARAM_OXDIM_FLOORED_IDX               1
#define GDC_PARAM_OXDIM_LAST_IDX                  2
#define GDC_PARAM_WOIX_LAST_IDX                   3
#define GDC_PARAM_IY_TOPLEFT_IDX                  4
#define GDC_PARAM_CHUNK_CNT_IDX                   5
/* Derived from bpp */
/*#define GDC_PARAM_ELEMENTS_PER_XMEM_ADDR_IDX    6 */
#define GDC_PARAM_BPP_IDX						  6
#define GDC_PARAM_BLOCK_HEIGHT_IDX                7
/* The DMA stride == the GDC buffer stride */
/*#define GDC_PARAM_DMA_CHANNEL_STRIDE_A_IDX        8 */
#define GDC_PARAM_WOIX_IDX						  8
#define GDC_PARAM_DMA_CHANNEL_STRIDE_B_IDX        9
#define GDC_PARAM_DMA_CHANNEL_WIDTH_A_IDX        10
#define GDC_PARAM_DMA_CHANNEL_WIDTH_B_IDX        11
#define GDC_PARAM_VECTORS_PER_LINE_IN_IDX        12
#define GDC_PARAM_VECTORS_PER_LINE_OUT_IDX       13
#define GDC_PARAM_VMEM_IN_DIMY_IDX               14
#define GDC_PARAM_COMMAND_IDX                    15
#define N_GDC_PARAM                              16

/* Because of the packed parameter thransfer max(params) == max(fragments) */
#define	N_GDC_FRAGMENTS		N_GDC_PARAM

typedef enum {
	gdc_8_bpp  = 8,
	gdc_10_bpp = 10,
	gdc_12_bpp = 12,
	gdc_14_bpp = 14
} gdc_bits_per_pixel_t;

typedef struct {
	unsigned int      origin_x;
	unsigned int      origin_y;
	unsigned int      in_addr_offset;
	unsigned int      in_block_width;
	unsigned int      in_block_height;
	unsigned int      p0_x;
	unsigned int      p0_y;
	unsigned int      p1_x;
	unsigned int      p1_y;
	unsigned int      p2_x;
	unsigned int      p2_y;
	unsigned int      p3_x;
	unsigned int      p3_y;
	unsigned int      padding[3];
} gdc_warp_param_mem_t;

#endif /* __GDC_GLOBAL_H_INCLUDED__ */
