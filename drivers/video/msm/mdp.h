/* Copyright (c) 2008-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef MDP_H
#define MDP_H

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <linux/hrtimer.h>
#include <linux/msm_mdp.h>
#include <linux/memory_alloc.h>
#include <mach/hardware.h>
#include <linux/msm_ion.h>

#ifdef CONFIG_MSM_BUS_SCALING
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#endif

#include <linux/io.h>

#include <asm/system.h>
#include <asm/mach-types.h>

#include "msm_fb_panel.h"

extern uint32 mdp_hw_revision;
extern ulong mdp4_display_intf;
extern spinlock_t mdp_spin_lock;
extern int mdp_rev;
extern int mdp_iommu_split_domain;
extern struct mdp_csc_cfg mdp_csc_convert[4];
extern struct mdp_csc_cfg_data csc_cfg_matrix[];
extern struct workqueue_struct *mdp_hist_wq;

extern uint32 mdp_intr_mask;

#define MDP4_REVISION_V1		0
#define MDP4_REVISION_V2		1
#define MDP4_REVISION_V2_1	2
#define MDP4_REVISION_NONE	0xffffffff

#ifdef BIT
#undef BIT
#endif

#define BIT(x)  (1<<(x))

#define MDPOP_NOP               0
#define MDPOP_LR                BIT(0)	/* left to right flip */
#define MDPOP_UD                BIT(1)	/* up and down flip */
#define MDPOP_ROT90             BIT(2)	/* rotate image to 90 degree */
#define MDPOP_ROT180            (MDPOP_UD|MDPOP_LR)
#define MDPOP_ROT270            (MDPOP_ROT90|MDPOP_UD|MDPOP_LR)
#define MDPOP_ASCALE            BIT(7)
#define MDPOP_ALPHAB            BIT(8)	/* enable alpha blending */
#define MDPOP_TRANSP            BIT(9)	/* enable transparency */
#define MDPOP_DITHER            BIT(10)	/* enable dither */
#define MDPOP_SHARPENING	BIT(11) /* enable sharpening */
#define MDPOP_BLUR		BIT(12) /* enable blur */
#define MDPOP_FG_PM_ALPHA       BIT(13)
#define MDPOP_LAYER_IS_FG       BIT(14)
#define MDP_ALLOC(x)  kmalloc(x, GFP_KERNEL)

struct mdp_buf_type {
	struct ion_handle *ihdl;
	u32 write_addr;
	u32 read_addr;
	u32 size;
};

struct mdp_table_entry {
	uint32_t reg;
	uint32_t val;
};

extern struct mdp_ccs mdp_ccs_yuv2rgb ;
extern struct mdp_ccs mdp_ccs_rgb2yuv ;
extern unsigned char hdmi_prim_display;
extern unsigned char hdmi_prim_resolution;

struct vsync {
	ktime_t vsync_time;
	struct completion vsync_comp;
	struct device *dev;
	struct work_struct vsync_work;
	int vsync_irq_enabled;
	int vsync_dma_enabled;
	int disabled_clocks;
	struct completion vsync_wait;
	atomic_t suspend;
	atomic_t vsync_resume;
	int sysfs_created;
};

extern struct vsync vsync_cntrl;

/*
 * MDP Image Structure
 */
typedef struct mdpImg_ {
	uint32 imgType;		/* Image type */
	uint32 *bmy_addr;	/* bitmap or y addr */
	uint32 *cbcr_addr;	/* cbcr addr */
	uint32 width;		/* image width */
	uint32 mdpOp;		/* image opertion (rotation,flip up/down, alpha/tp) */
	uint32 tpVal;		/* transparency color */
	uint32 alpha;		/* alpha percentage 0%(0x0) ~ 100%(0x100) */
	int    sp_value;        /* sharpening strength */
} MDPIMG;

#define MDP_OUTP(addr, data) outpdw((addr), (data))

#define MDP_BASE msm_mdp_base

typedef enum {
	MDP_BC_SCALE_POINT2_POINT4,
	MDP_BC_SCALE_POINT4_POINT6,
	MDP_BC_SCALE_POINT6_POINT8,
	MDP_BC_SCALE_POINT8_1,
	MDP_BC_SCALE_UP,
	MDP_PR_SCALE_POINT2_POINT4,
	MDP_PR_SCALE_POINT4_POINT6,
	MDP_PR_SCALE_POINT6_POINT8,
	MDP_PR_SCALE_POINT8_1,
	MDP_PR_SCALE_UP,
	MDP_SCALE_BLUR,
	MDP_INIT_SCALE
} MDP_SCALE_MODE;

typedef enum {
	MDP_BLOCK_POWER_OFF,
	MDP_BLOCK_POWER_ON
} MDP_BLOCK_POWER_STATE;

typedef enum {
	MDP_CMD_BLOCK,
	MDP_OVERLAY0_BLOCK,
	MDP_MASTER_BLOCK,
	MDP_PPP_BLOCK,
	MDP_DMA2_BLOCK,
	MDP_DMA3_BLOCK,
	MDP_DMA_S_BLOCK,
	MDP_DMA_E_BLOCK,
	MDP_OVERLAY1_BLOCK,
	MDP_OVERLAY2_BLOCK,
	MDP_MAX_BLOCK
} MDP_BLOCK_TYPE;

/* Let's keep Q Factor power of 2 for optimization */
#define MDP_SCALE_Q_FACTOR 512

#ifdef CONFIG_FB_MSM_MDP31
#define MDP_MAX_X_SCALE_FACTOR (MDP_SCALE_Q_FACTOR*8)
#define MDP_MIN_X_SCALE_FACTOR (MDP_SCALE_Q_FACTOR/8)
#define MDP_MAX_Y_SCALE_FACTOR (MDP_SCALE_Q_FACTOR*8)
#define MDP_MIN_Y_SCALE_FACTOR (MDP_SCALE_Q_FACTOR/8)
#else
#define MDP_MAX_X_SCALE_FACTOR (MDP_SCALE_Q_FACTOR*4)
#define MDP_MIN_X_SCALE_FACTOR (MDP_SCALE_Q_FACTOR/4)
#define MDP_MAX_Y_SCALE_FACTOR (MDP_SCALE_Q_FACTOR*4)
#define MDP_MIN_Y_SCALE_FACTOR (MDP_SCALE_Q_FACTOR/4)
#endif

/* SHIM Q Factor */
#define PHI_Q_FACTOR          29
#define PQF_PLUS_5            (PHI_Q_FACTOR + 5)	/* due to 32 phases */
#define PQF_PLUS_4            (PHI_Q_FACTOR + 4)
#define PQF_PLUS_2            (PHI_Q_FACTOR + 2)	/* to get 4.0 */
#define PQF_MINUS_2           (PHI_Q_FACTOR - 2)	/* to get 0.25 */
#define PQF_PLUS_5_PLUS_2     (PQF_PLUS_5 + 2)
#define PQF_PLUS_5_MINUS_2    (PQF_PLUS_5 - 2)

#define MDP_CONVTP(tpVal) (((tpVal&0xF800)<<8)|((tpVal&0x7E0)<<5)|((tpVal&0x1F)<<3))

#define MDPOP_ROTATION (MDPOP_ROT90|MDPOP_LR|MDPOP_UD)
#define MDP_CHKBIT(val, bit) ((bit) == ((val) & (bit)))

/* overlay interface API defines */
typedef enum {
	MORE_IBUF,
	FINAL_IBUF,
	COMPLETE_IBUF
} MDP_IBUF_STATE;

struct mdp_dirty_region {
	__u32 xoffset;		/* source origin in the x-axis */
	__u32 yoffset;		/* source origin in the y-axis */
	__u32 width;		/* number of pixels in the x-axis */
	__u32 height;		/* number of pixels in the y-axis */
};

/*
 * MDP extended data types
 */
typedef struct mdp_roi_s {
	uint32 x;
	uint32 y;
	uint32 width;
	uint32 height;
	int32 lcd_x;
	int32 lcd_y;
	uint32 dst_width;
	uint32 dst_height;
} MDP_ROI;

typedef struct mdp_ibuf_s {
	uint8 *buf;
	uint32 bpp;
	uint32 ibuf_type;
	uint32 ibuf_width;
	uint32 ibuf_height;

	MDP_ROI roi;
	MDPIMG mdpImg;

	int32 dma_x;
	int32 dma_y;
	uint32 dma_w;
	uint32 dma_h;

	uint32 vsync_enable;
} MDPIBUF;

struct mdp_dma_data {
	boolean busy;
	boolean dmap_busy;
	boolean waiting;
	struct mutex ov_mutex;
	struct semaphore mutex;
	struct completion comp;
	struct completion dmap_comp;
};

extern struct list_head mdp_hist_lut_list;
extern struct mutex mdp_hist_lut_list_mutex;
struct mdp_hist_lut_mgmt {
	uint32_t block;
	struct mutex lock;
	struct list_head list;
};

struct mdp_hist_lut_info {
	uint32_t block;
	boolean is_enabled, has_sel_update;
	int bank_sel;
};

struct mdp_hist_mgmt {
	uint32_t block;
	uint32_t irq_term;
	uint32_t intr;
	uint32_t base;
	struct completion mdp_hist_comp;
	struct mutex mdp_hist_mutex;
	struct mutex mdp_do_hist_mutex;
	boolean mdp_is_hist_start, mdp_is_hist_data;
	boolean mdp_is_hist_valid, mdp_is_hist_init;
	uint8_t frame_cnt, bit_mask, num_bins;
	struct work_struct mdp_histogram_worker;
	struct mdp_histogram_data *hist;
	uint32_t *c0, *c1, *c2;
	uint32_t *extra_info;
};

enum {
	MDP_HIST_MGMT_DMA_P = 0,
	MDP_HIST_MGMT_DMA_S,
	MDP_HIST_MGMT_VG_1,
	MDP_HIST_MGMT_VG_2,
	MDP_HIST_MGMT_MAX,
};

extern struct mdp_hist_mgmt *mdp_hist_mgmt_array[];

#define MDP_CMD_DEBUG_ACCESS_BASE   (MDP_BASE+0x10000)

#define MDP_DMA2_TERM 0x1
#define MDP_DMA3_TERM 0x2
#define MDP_PPP_TERM 0x4
#define MDP_DMA_S_TERM 0x8
#define MDP_DMA_E_TERM 0x10
#ifdef CONFIG_FB_MSM_MDP40
#define MDP_OVERLAY0_TERM 0x20
#define MDP_OVERLAY1_TERM 0x40
#define MDP_DMAP_TERM MDP_DMA2_TERM	/* dmap == dma2 */
#define MDP_PRIM_VSYNC_TERM 0x100
#define MDP_EXTER_VSYNC_TERM 0x200
#define MDP_PRIM_RDPTR_TERM 0x400
#endif
#define MDP_OVERLAY2_TERM 0x80
#define MDP_HISTOGRAM_TERM_DMA_P 0x10000
#define MDP_HISTOGRAM_TERM_DMA_S 0x20000
#define MDP_HISTOGRAM_TERM_VG_1 0x40000
#define MDP_HISTOGRAM_TERM_VG_2 0x80000
#define MDP_VSYNC_TERM 0x1000

#define ACTIVE_START_X_EN BIT(31)
#define ACTIVE_START_Y_EN BIT(31)
#define ACTIVE_HIGH 0
#define ACTIVE_LOW 1
#define MDP_DMA_S_DONE  BIT(2)
#define MDP_DMA_E_DONE  BIT(3)
#define LCDC_FRAME_START    BIT(15)
#define LCDC_UNDERFLOW      BIT(16)

#ifdef CONFIG_FB_MSM_MDP22
#define MDP_DMA_P_DONE 	BIT(2)
#else
#define MDP_DMA_P_DONE 	BIT(14)
#endif

#define MDP_PPP_DONE 				BIT(0)
#define TV_OUT_DMA3_DONE    BIT(6)
#define TV_ENC_UNDERRUN     BIT(7)
#define MDP_PRIM_RDPTR      BIT(8)
#define TV_OUT_DMA3_START   BIT(13)
#define MDP_HIST_DONE       BIT(20)

/*MDP4 MDP histogram interrupts*/
/*note: these are only applicable on MDP4+ targets*/
#define INTR_VG1_HISTOGRAM		BIT(5)
#define INTR_VG2_HISTOGRAM		BIT(6)
#define INTR_DMA_P_HISTOGRAM		BIT(17)
#define INTR_DMA_S_HISTOGRAM		BIT(26)
/*end MDP4 MDP histogram interrupts*/

/* histogram interrupts */
#define INTR_HIST_DONE			BIT(1)
#define INTR_HIST_RESET_SEQ_DONE	BIT(0)

#ifdef CONFIG_FB_MSM_MDP22
#define MDP_ANY_INTR_MASK (MDP_PPP_DONE| \
			MDP_DMA_P_DONE| \
			TV_ENC_UNDERRUN)
#else
#define MDP_ANY_INTR_MASK (MDP_PPP_DONE| \
			MDP_DMA_P_DONE| \
			MDP_DMA_S_DONE| \
			MDP_DMA_E_DONE| \
			LCDC_UNDERFLOW| \
			TV_ENC_UNDERRUN)
#endif

#define MDP_TOP_LUMA       16
#define MDP_TOP_CHROMA     0
#define MDP_BOTTOM_LUMA    19
#define MDP_BOTTOM_CHROMA  3
#define MDP_LEFT_LUMA      22
#define MDP_LEFT_CHROMA    6
#define MDP_RIGHT_LUMA     25
#define MDP_RIGHT_CHROMA   9

#define CLR_G 0x0
#define CLR_B 0x1
#define CLR_R 0x2
#define CLR_ALPHA 0x3

#define CLR_Y  CLR_G
#define CLR_CB CLR_B
#define CLR_CR CLR_R

/* from lsb to msb */
#define MDP_GET_PACK_PATTERN(a,x,y,z,bit) (((a)<<(bit*3))|((x)<<(bit*2))|((y)<<bit)|(z))

/*
 * 0x0000 0x0004 0x0008 MDP sync config
 */
#ifdef CONFIG_FB_MSM_MDP22
#define MDP_SYNCFG_HGT_LOC 22
#define MDP_SYNCFG_VSYNC_EXT_EN BIT(21)
#define MDP_SYNCFG_VSYNC_INT_EN BIT(20)
#else
#define MDP_SYNCFG_HGT_LOC 21
#define MDP_SYNCFG_VSYNC_EXT_EN BIT(20)
#define MDP_SYNCFG_VSYNC_INT_EN BIT(19)
#define MDP_HW_VSYNC
#endif

/*
 * 0x0018 MDP VSYNC THREASH
 */
#define MDP_PRIM_BELOW_LOC 0
#define MDP_PRIM_ABOVE_LOC 8

/*
 * MDP_PRIMARY_VSYNC_OUT_CTRL
 * 0x0080,84,88 internal vsync pulse config
 */
#define VSYNC_PULSE_EN BIT(31)
#define VSYNC_PULSE_INV BIT(30)

/*
 * 0x008c MDP VSYNC CONTROL
 */
#define DISP0_VSYNC_MAP_VSYNC0 0
#define DISP0_VSYNC_MAP_VSYNC1 BIT(0)
#define DISP0_VSYNC_MAP_VSYNC2 BIT(0)|BIT(1)

#define DISP1_VSYNC_MAP_VSYNC0 0
#define DISP1_VSYNC_MAP_VSYNC1 BIT(2)
#define DISP1_VSYNC_MAP_VSYNC2 BIT(2)|BIT(3)

#define PRIMARY_LCD_SYNC_EN BIT(4)
#define PRIMARY_LCD_SYNC_DISABLE 0

#define SECONDARY_LCD_SYNC_EN BIT(5)
#define SECONDARY_LCD_SYNC_DISABLE 0

#define EXTERNAL_LCD_SYNC_EN BIT(6)
#define EXTERNAL_LCD_SYNC_DISABLE 0

/*
 * 0x101f0 MDP VSYNC Threshold
 */
#define VSYNC_THRESHOLD_ABOVE_LOC 0
#define VSYNC_THRESHOLD_BELOW_LOC 16
#define VSYNC_ANTI_TEAR_EN BIT(31)

/*
 * 0x10004 command config
 */
#define MDP_CMD_DBGBUS_EN BIT(0)

/*
 * 0x10124 or 0x101d4PPP source config
 */
#define PPP_SRC_C0G_8BITS (BIT(1)|BIT(0))
#define PPP_SRC_C1B_8BITS (BIT(3)|BIT(2))
#define PPP_SRC_C2R_8BITS (BIT(5)|BIT(4))
#define PPP_SRC_C3A_8BITS (BIT(7)|BIT(6))

#define PPP_SRC_C0G_6BITS BIT(1)
#define PPP_SRC_C1B_6BITS BIT(3)
#define PPP_SRC_C2R_6BITS BIT(5)

#define PPP_SRC_C0G_5BITS BIT(0)
#define PPP_SRC_C1B_5BITS BIT(2)
#define PPP_SRC_C2R_5BITS BIT(4)

#define PPP_SRC_C3_ALPHA_EN BIT(8)

#define PPP_SRC_BPP_INTERLVD_1BYTES 0
#define PPP_SRC_BPP_INTERLVD_2BYTES BIT(9)
#define PPP_SRC_BPP_INTERLVD_3BYTES BIT(10)
#define PPP_SRC_BPP_INTERLVD_4BYTES (BIT(10)|BIT(9))

#define PPP_SRC_BPP_ROI_ODD_X BIT(11)
#define PPP_SRC_BPP_ROI_ODD_Y BIT(12)
#define PPP_SRC_INTERLVD_2COMPONENTS BIT(13)
#define PPP_SRC_INTERLVD_3COMPONENTS BIT(14)
#define PPP_SRC_INTERLVD_4COMPONENTS (BIT(14)|BIT(13))

/*
 * RGB666 unpack format
 * TIGHT means R6+G6+B6 together
 * LOOSE means R6+2 +G6+2+ B6+2 (with MSB)
 * or 2+R6 +2+G6 +2+B6 (with LSB)
 */
#define PPP_SRC_UNPACK_TIGHT BIT(17)
#define PPP_SRC_UNPACK_LOOSE 0
#define PPP_SRC_UNPACK_ALIGN_LSB 0
#define PPP_SRC_UNPACK_ALIGN_MSB BIT(18)

#define PPP_SRC_FETCH_PLANES_INTERLVD 0
#define PPP_SRC_FETCH_PLANES_PSEUDOPLNR BIT(20)

#define PPP_SRC_WMV9_MODE BIT(21)	/* window media version 9 */

/*
 * 0x10138 PPP operation config
 */
#define PPP_OP_SCALE_X_ON BIT(0)
#define PPP_OP_SCALE_Y_ON BIT(1)

#define PPP_OP_CONVERT_RGB2YCBCR 0
#define PPP_OP_CONVERT_YCBCR2RGB BIT(2)
#define PPP_OP_CONVERT_ON BIT(3)

#define PPP_OP_CONVERT_MATRIX_PRIMARY 0
#define PPP_OP_CONVERT_MATRIX_SECONDARY BIT(4)

#define PPP_OP_LUT_C0_ON BIT(5)
#define PPP_OP_LUT_C1_ON BIT(6)
#define PPP_OP_LUT_C2_ON BIT(7)

/* rotate or blend enable */
#define PPP_OP_ROT_ON BIT(8)

#define PPP_OP_ROT_90 BIT(9)
#define PPP_OP_FLIP_LR BIT(10)
#define PPP_OP_FLIP_UD BIT(11)

#define PPP_OP_BLEND_ON BIT(12)

#define PPP_OP_BLEND_SRCPIXEL_ALPHA 0
#define PPP_OP_BLEND_DSTPIXEL_ALPHA BIT(13)
#define PPP_OP_BLEND_CONSTANT_ALPHA BIT(14)
#define PPP_OP_BLEND_SRCPIXEL_TRANSP (BIT(13)|BIT(14))

#define PPP_OP_BLEND_ALPHA_BLEND_NORMAL 0
#define PPP_OP_BLEND_ALPHA_BLEND_REVERSE BIT(15)

#define PPP_OP_DITHER_EN BIT(16)

#define PPP_OP_COLOR_SPACE_RGB 0
#define PPP_OP_COLOR_SPACE_YCBCR BIT(17)

#define PPP_OP_SRC_CHROMA_RGB 0
#define PPP_OP_SRC_CHROMA_H2V1 BIT(18)
#define PPP_OP_SRC_CHROMA_H1V2 BIT(19)
#define PPP_OP_SRC_CHROMA_420 (BIT(18)|BIT(19))
#define PPP_OP_SRC_CHROMA_COSITE 0
#define PPP_OP_SRC_CHROMA_OFFSITE BIT(20)

#define PPP_OP_DST_CHROMA_RGB 0
#define PPP_OP_DST_CHROMA_H2V1 BIT(21)
#define PPP_OP_DST_CHROMA_H1V2 BIT(22)
#define PPP_OP_DST_CHROMA_420 (BIT(21)|BIT(22))
#define PPP_OP_DST_CHROMA_COSITE 0
#define PPP_OP_DST_CHROMA_OFFSITE BIT(23)

#define PPP_BLEND_CALPHA_TRNASP BIT(24)

#define PPP_OP_BG_CHROMA_RGB 0
#define PPP_OP_BG_CHROMA_H2V1 BIT(25)
#define PPP_OP_BG_CHROMA_H1V2 BIT(26)
#define PPP_OP_BG_CHROMA_420 BIT(25)|BIT(26)
#define PPP_OP_BG_CHROMA_SITE_COSITE 0
#define PPP_OP_BG_CHROMA_SITE_OFFSITE BIT(27)
#define PPP_OP_DEINT_EN BIT(28)

#define PPP_BLEND_BG_USE_ALPHA_SEL      (1 << 0)
#define PPP_BLEND_BG_ALPHA_REVERSE      (1 << 3)
#define PPP_BLEND_BG_SRCPIXEL_ALPHA     (0 << 1)
#define PPP_BLEND_BG_DSTPIXEL_ALPHA     (1 << 1)
#define PPP_BLEND_BG_CONSTANT_ALPHA     (2 << 1)
#define PPP_BLEND_BG_CONST_ALPHA_VAL(x) ((x) << 24)

#define PPP_OP_DST_RGB 0
#define PPP_OP_DST_YCBCR BIT(30)
/*
 * 0x10150 PPP destination config
 */
#define PPP_DST_C0G_8BIT (BIT(0)|BIT(1))
#define PPP_DST_C1B_8BIT (BIT(3)|BIT(2))
#define PPP_DST_C2R_8BIT (BIT(5)|BIT(4))
#define PPP_DST_C3A_8BIT (BIT(7)|BIT(6))

#define PPP_DST_C0G_6BIT BIT(1)
#define PPP_DST_C1B_6BIT BIT(3)
#define PPP_DST_C2R_6BIT BIT(5)

#define PPP_DST_C0G_5BIT BIT(0)
#define PPP_DST_C1B_5BIT BIT(2)
#define PPP_DST_C2R_5BIT BIT(4)

#define PPP_DST_C3A_8BIT (BIT(7)|BIT(6))
#define PPP_DST_C3ALPHA_EN BIT(8)

#define PPP_DST_PACKET_CNT_INTERLVD_2ELEM BIT(9)
#define PPP_DST_PACKET_CNT_INTERLVD_3ELEM BIT(10)
#define PPP_DST_PACKET_CNT_INTERLVD_4ELEM (BIT(10)|BIT(9))
#define PPP_DST_PACKET_CNT_INTERLVD_6ELEM (BIT(11)|BIT(9))

#define PPP_DST_PACK_LOOSE 0
#define PPP_DST_PACK_TIGHT BIT(13)
#define PPP_DST_PACK_ALIGN_LSB 0
#define PPP_DST_PACK_ALIGN_MSB BIT(14)

#define PPP_DST_OUT_SEL_AXI 0
#define PPP_DST_OUT_SEL_MDDI BIT(15)

#define PPP_DST_BPP_2BYTES BIT(16)
#define PPP_DST_BPP_3BYTES BIT(17)
#define PPP_DST_BPP_4BYTES (BIT(17)|BIT(16))

#define PPP_DST_PLANE_INTERLVD 0
#define PPP_DST_PLANE_PLANAR BIT(18)
#define PPP_DST_PLANE_PSEUDOPLN BIT(19)

#define PPP_DST_TO_TV BIT(20)

#define PPP_DST_MDDI_PRIMARY 0
#define PPP_DST_MDDI_SECONDARY BIT(21)
#define PPP_DST_MDDI_EXTERNAL BIT(22)

/*
 * 0x10180 DMA config
 */
#define DMA_DSTC0G_8BITS (BIT(1)|BIT(0))
#define DMA_DSTC1B_8BITS (BIT(3)|BIT(2))
#define DMA_DSTC2R_8BITS (BIT(5)|BIT(4))

#define DMA_DSTC0G_6BITS BIT(1)
#define DMA_DSTC1B_6BITS BIT(3)
#define DMA_DSTC2R_6BITS BIT(5)

#define DMA_DSTC0G_5BITS BIT(0)
#define DMA_DSTC1B_5BITS BIT(2)
#define DMA_DSTC2R_5BITS BIT(4)

#define DMA_PACK_TIGHT                      BIT(6)
#define DMA_PACK_LOOSE                      0
#define DMA_PACK_ALIGN_LSB                  0
/*
 * use DMA_PACK_ALIGN_MSB if the upper 6 bits from 8 bits output
 * from LCDC block maps into 6 pins out to the panel
 */
#define DMA_PACK_ALIGN_MSB                  BIT(7)
#define DMA_PACK_PATTERN_RGB \
       (MDP_GET_PACK_PATTERN(0, CLR_R, CLR_G, CLR_B, 2)<<8)
#define DMA_PACK_PATTERN_BGR \
       (MDP_GET_PACK_PATTERN(0, CLR_B, CLR_G, CLR_R, 2)<<8)
#define DMA_OUT_SEL_AHB                     0
#define DMA_OUT_SEL_LCDC                    BIT(20)
#define DMA_IBUF_FORMAT_RGB888              0
#define DMA_IBUF_FORMAT_xRGB8888_OR_ARGB8888  BIT(26)

#ifdef CONFIG_FB_MSM_MDP303
#define DMA_OUT_SEL_DSI_CMD                  BIT(19)
#define DMA_OUT_SEL_DSI_VIDEO               (3 << 19)
#endif

#ifdef CONFIG_FB_MSM_MDP22
#define DMA_OUT_SEL_MDDI BIT(14)
#define DMA_AHBM_LCD_SEL_PRIMARY 0
#define DMA_AHBM_LCD_SEL_SECONDARY BIT(15)
#define DMA_IBUF_C3ALPHA_EN BIT(16)
#define DMA_DITHER_EN BIT(17)
#define DMA_MDDI_DMAOUT_LCD_SEL_PRIMARY 0
#define DMA_MDDI_DMAOUT_LCD_SEL_SECONDARY BIT(18)
#define DMA_MDDI_DMAOUT_LCD_SEL_EXTERNAL BIT(19)
#define DMA_IBUF_FORMAT_RGB565 BIT(20)
#define DMA_IBUF_FORMAT_RGB888_OR_ARGB8888 0
#define DMA_IBUF_NONCONTIGUOUS BIT(21)
#else
#define DMA_OUT_SEL_MDDI                    BIT(19)
#define DMA_AHBM_LCD_SEL_PRIMARY            0
#define DMA_AHBM_LCD_SEL_SECONDARY          0
#define DMA_IBUF_C3ALPHA_EN                 0
#define DMA_BUF_FORMAT_RGB565		BIT(25)
#define DMA_DITHER_EN                       BIT(24)	/* dma_p */
#define DMA_DEFLKR_EN                       BIT(24)	/* dma_e */
#define DMA_MDDI_DMAOUT_LCD_SEL_PRIMARY     0
#define DMA_MDDI_DMAOUT_LCD_SEL_SECONDARY   0
#define DMA_MDDI_DMAOUT_LCD_SEL_EXTERNAL    0
#define DMA_IBUF_FORMAT_RGB565              BIT(25)
#define DMA_IBUF_NONCONTIGUOUS 0
#endif

/*
 * MDDI Register
 */
#define MDDI_VDO_PACKET_DESC_16  0x5565
#define MDDI_VDO_PACKET_DESC	 0x5666	/* 18 bits */
#define MDDI_VDO_PACKET_DESC_24  0x5888

#define MDP_HIST_INTR_STATUS_OFF	(0x0014)
#define MDP_HIST_INTR_CLEAR_OFF		(0x0018)
#define MDP_HIST_INTR_ENABLE_OFF	(0x001C)

#ifdef CONFIG_FB_MSM_MDP40
#define MDP_INTR_ENABLE		(msm_mdp_base + 0x0050)
#define MDP_INTR_STATUS		(msm_mdp_base + 0x0054)
#define MDP_INTR_CLEAR		(msm_mdp_base + 0x0058)
#define MDP_EBI2_LCD0		(msm_mdp_base + 0x0060)
#define MDP_EBI2_LCD1		(msm_mdp_base + 0x0064)
#define MDP_EBI2_PORTMAP_MODE	(msm_mdp_base + 0x0070)

#define MDP_DMA_P_HIST_INTR_STATUS 	(msm_mdp_base + 0x95014)
#define MDP_DMA_P_HIST_INTR_CLEAR 	(msm_mdp_base + 0x95018)
#define MDP_DMA_P_HIST_INTR_ENABLE 	(msm_mdp_base + 0x9501C)

#else
#define MDP_INTR_ENABLE		(msm_mdp_base + 0x0020)
#define MDP_INTR_STATUS		(msm_mdp_base + 0x0024)
#define MDP_INTR_CLEAR		(msm_mdp_base + 0x0028)
#define MDP_EBI2_LCD0		(msm_mdp_base + 0x003c)
#define MDP_EBI2_LCD1		(msm_mdp_base + 0x0040)
#define MDP_EBI2_PORTMAP_MODE	(msm_mdp_base + 0x005c)

#define MDP_DMA_P_HIST_INTR_STATUS	(msm_mdp_base + 0x94014)
#define MDP_DMA_P_HIST_INTR_CLEAR	(msm_mdp_base + 0x94018)
#define MDP_DMA_P_HIST_INTR_ENABLE	(msm_mdp_base + 0x9401C)
#endif

#define MDP_FULL_BYPASS_WORD43  (msm_mdp_base + 0x101ac)

#define MDP_CSC_PFMVn(n)	(msm_mdp_base + 0x40400 + 4 * (n))
#define MDP_CSC_PRMVn(n)	(msm_mdp_base + 0x40440 + 4 * (n))
#define MDP_CSC_PRE_BV1n(n)	(msm_mdp_base + 0x40500 + 4 * (n))
#define MDP_CSC_PRE_BV2n(n)	(msm_mdp_base + 0x40540 + 4 * (n))
#define MDP_CSC_POST_BV1n(n)	(msm_mdp_base + 0x40580 + 4 * (n))
#define MDP_CSC_POST_BV2n(n)	(msm_mdp_base + 0x405c0 + 4 * (n))

#ifdef CONFIG_FB_MSM_MDP31
#define MDP_CSC_PRE_LV1n(n)	(msm_mdp_base + 0x40600 + 4 * (n))
#define MDP_CSC_PRE_LV2n(n)	(msm_mdp_base + 0x40640 + 4 * (n))
#define MDP_CSC_POST_LV1n(n)	(msm_mdp_base + 0x40680 + 4 * (n))
#define MDP_CSC_POST_LV2n(n)	(msm_mdp_base + 0x406c0 + 4 * (n))
#define MDP_PPP_SCALE_COEFF_LSBn(n)	(msm_mdp_base + 0x50400 + 8 * (n))
#define MDP_PPP_SCALE_COEFF_MSBn(n)	(msm_mdp_base + 0x50404 + 8 * (n))

#define SCALE_D0_SET  0
#define SCALE_D1_SET  BIT(0)
#define SCALE_D2_SET  BIT(1)
#define SCALE_U1_SET  (BIT(0)|BIT(1))

#else
#define MDP_CSC_PRE_LV1n(n)	(msm_mdp_base + 0x40580 + 4 * (n))
#endif

#define MDP_CURSOR_WIDTH 64
#define MDP_CURSOR_HEIGHT 64
#define MDP_CURSOR_SIZE (MDP_CURSOR_WIDTH*MDP_CURSOR_WIDTH*4)

#define MDP_DMA_P_LUT_C0_EN   BIT(0)
#define MDP_DMA_P_LUT_C1_EN   BIT(1)
#define MDP_DMA_P_LUT_C2_EN   BIT(2)
#define MDP_DMA_P_LUT_POST    BIT(4)

void mdp_hw_init(int splash);
int mdp_ppp_pipe_wait(void);
void mdp_pipe_kickoff(uint32 term, struct msm_fb_data_type *mfd);
void mdp_clk_ctrl(int on);
void mdp_pipe_ctrl(MDP_BLOCK_TYPE block, MDP_BLOCK_POWER_STATE state,
		   boolean isr);
void mdp_set_dma_pan_info(struct fb_info *info, struct mdp_dirty_region *dirty,
			  boolean sync);
void mdp_dma_pan_update(struct fb_info *info);
void mdp_refresh_screen(unsigned long data);
int mdp_ppp_blit(struct fb_info *info, struct mdp_blit_req *req);
void mdp_lcd_update_workqueue_handler(struct work_struct *work);
void mdp_vsync_resync_workqueue_handler(struct work_struct *work);
void mdp_dma2_update(struct msm_fb_data_type *mfd);
void mdp_vsync_cfg_regs(struct msm_fb_data_type *mfd,
	boolean first_time);
void mdp_config_vsync(struct platform_device *pdev,
	struct msm_fb_data_type *mfd);
uint32 mdp_get_lcd_line_counter(struct msm_fb_data_type *mfd);
enum hrtimer_restart mdp_dma2_vsync_hrtimer_handler(struct hrtimer *ht);
void mdp_set_scale(MDPIBUF *iBuf,
		   uint32 dst_roi_width,
		   uint32 dst_roi_height,
		   boolean inputRGB, boolean outputRGB, uint32 *pppop_reg_ptr);
void mdp_init_scale_table(void);
void mdp_adjust_start_addr(uint8 **src0,
			   uint8 **src1,
			   int v_slice,
			   int h_slice,
			   int x,
			   int y,
			   uint32 width,
			   uint32 height, int bpp, MDPIBUF *iBuf, int layer);
void mdp_set_blend_attr(MDPIBUF *iBuf,
			uint32 *alpha,
			uint32 *tpVal,
			uint32 perPixelAlpha, uint32 *pppop_reg_ptr);

int mdp_dma3_on(struct platform_device *pdev);
int mdp_dma3_off(struct platform_device *pdev);
void mdp_dma3_update(struct msm_fb_data_type *mfd);

int mdp_lcdc_on(struct platform_device *pdev);
int mdp_lcdc_off(struct platform_device *pdev);
void mdp_lcdc_update(struct msm_fb_data_type *mfd);
void mdp_free_splash_buffer(struct msm_fb_data_type *mfd);
#ifdef CONFIG_FB_MSM_MDP303
int mdp_dsi_video_on(struct platform_device *pdev);
int mdp_dsi_video_off(struct platform_device *pdev);
void mdp_dsi_video_update(struct msm_fb_data_type *mfd);
void mdp3_dsi_cmd_dma_busy_wait(struct msm_fb_data_type *mfd);
static inline int mdp4_dsi_cmd_off(struct platform_device *pdev)
{
	return 0;
}
static inline int mdp4_dsi_video_off(struct platform_device *pdev)
{
	return 0;
}
static inline int mdp4_lcdc_off(struct platform_device *pdev)
{
	return 0;
}
static inline int mdp4_mddi_off(struct platform_device *pdev)
{
	return 0;
}
static inline int mdp4_dsi_cmd_on(struct platform_device *pdev)
{
	return 0;
}
static inline int mdp4_dsi_video_on(struct platform_device *pdev)
{
	return 0;
}
static inline int mdp4_lcdc_on(struct platform_device *pdev)
{
	return 0;
}
static inline int mdp4_mddi_on(struct platform_device *pdev)
{
	return 0;
}
#endif


#ifndef CONFIG_FB_MSM_MDDI
static inline void mdp4_mddi_rdptr_init(int cndx)
{
	/* empty */
}

#endif

void set_cont_splashScreen_status(int);

int mdp_hw_cursor_update(struct fb_info *info, struct fb_cursor *cursor);
#if defined(CONFIG_FB_MSM_OVERLAY) && defined(CONFIG_FB_MSM_MDP40)
int mdp_hw_cursor_sync_update(struct fb_info *info, struct fb_cursor *cursor);
#else
static inline int mdp_hw_cursor_sync_update(struct fb_info *info,
		struct fb_cursor *cursor)
{
	return 0;
}
#endif

void mdp_enable_irq(uint32 term);
void mdp_disable_irq(uint32 term);
void mdp_disable_irq_nosync(uint32 term);
int mdp_get_bytes_per_pixel(uint32_t format,
				 struct msm_fb_data_type *mfd);
int mdp_set_core_clk(u32 rate);
int mdp_clk_round_rate(u32 rate);

unsigned long mdp_get_core_clk(void);

#ifdef CONFIG_MSM_BUS_SCALING
int mdp_bus_scale_update_request(u64 ab, u64 ib);
#else
static inline int mdp_bus_scale_update_request(u64 ab,
					       u64 ib)
{
	return 0;
}
#endif
void mdp_dma_vsync_ctrl(int enable);
void mdp_dma_video_vsync_ctrl(int enable);
void mdp_dma_lcdc_vsync_ctrl(int enable);
ssize_t mdp_dma_show_event(struct device *dev,
		struct device_attribute *attr, char *buf);
ssize_t mdp_dma_video_show_event(struct device *dev,
		struct device_attribute *attr, char *buf);
ssize_t mdp_dma_lcdc_show_event(struct device *dev,
		struct device_attribute *attr, char *buf);

#ifdef MDP_HW_VSYNC
void vsync_clk_prepare_enable(void);
void vsync_clk_disable_unprepare(void);
void mdp_hw_vsync_clk_enable(struct msm_fb_data_type *mfd);
void mdp_hw_vsync_clk_disable(struct msm_fb_data_type *mfd);
void mdp_vsync_clk_disable(void);
void mdp_vsync_clk_enable(void);
#endif

#ifdef CONFIG_DEBUG_FS
int mdp_debugfs_init(void);
#endif

void mdp_dma_s_update(struct msm_fb_data_type *mfd);
int mdp_histogram_start(struct mdp_histogram_start_req *req);
int mdp_histogram_stop(struct fb_info *info, uint32_t block);
int mdp_histogram_ctrl(boolean en, uint32_t block);
int mdp_histogram_ctrl_all(boolean en);
int mdp_histogram_block2mgmt(uint32_t block, struct mdp_hist_mgmt **mgmt);
void mdp_histogram_handle_isr(struct mdp_hist_mgmt *mgmt);
void __mdp_histogram_kickoff(struct mdp_hist_mgmt *mgmt);
void __mdp_histogram_reset(struct mdp_hist_mgmt *mgmt);
void mdp_footswitch_ctrl(boolean on);

#ifdef CONFIG_FB_MSM_MDP303
static inline void mdp4_dsi_cmd_dma_busy_wait(struct msm_fb_data_type *mfd)
{
	/* empty */
}

static inline void mdp4_dsi_blt_dmap_busy_wait(struct msm_fb_data_type *mfd)
{
	/* empty */
}
static inline void mdp4_overlay_dsi_state_set(int state)
{
	/* empty */
}
static inline int mdp4_overlay_dsi_state_get(void)
{
	return 0;
}
#endif

#ifndef CONFIG_FB_MSM_MDP40
static inline void mdp_dsi_cmd_overlay_suspend(struct msm_fb_data_type *mfd)
{
	/* empty */
}
static inline int msmfb_overlay_vsync_ctrl(struct fb_info *info,
						void __user *argp)
{
	return 0;
}
#endif

int mdp_ppp_v4l2_overlay_set(struct fb_info *info, struct mdp_overlay *req);
int mdp_ppp_v4l2_overlay_clear(void);
int mdp_ppp_v4l2_overlay_play(struct fb_info *info, bool bUserPtr,
	unsigned long srcp0_addr, unsigned long srcp0_size,
	unsigned long srcp1_addr, unsigned long srcp1_size);
void mdp_update_pm(struct msm_fb_data_type *mfd, ktime_t pre_vsync);

u32 mdp_get_panel_framerate(struct msm_fb_data_type *mfd);

#ifdef CONFIG_FB_MSM_DTV
void mdp_vid_quant_set(void);
#else
static inline void mdp_vid_quant_set(void)
{
	/* empty */
}
#endif
#endif /* MDP_H */
