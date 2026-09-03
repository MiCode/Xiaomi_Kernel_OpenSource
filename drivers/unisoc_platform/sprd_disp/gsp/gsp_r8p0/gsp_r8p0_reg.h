/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _GSP_R8P0_REG_H
#define _GSP_R8P0_REG_H

#include "gsp_debug.h"

#define R8P0_IMGL_NUM 2
#define R8P0_OSDL_NUM 2
#define R8P0_IMGSEC_NUM 0
#define R8P0_OSDSEC_NUM 1

/* Global config reg */
#define R8P0_GSP_GLB_CFG(base)		(base )
#define R8P0_GSP_INT(base)		(base + 0x004)
#define R8P0_GSP_MOD_CFG(base)		(base + 0x008)
#define R8P0_GSP_SECURE_CFG(base)	(base + 0x00C)

/* Destination reg 1 */
#define R8P0_DES_DATA_CFG(base)		(base + 0x010)
#define R8P0_DES_Y_ADDR(base)		(base + 0x014)
#define R8P0_DES_U_ADDR(base)		(base + 0x018)
#define R8P0_DES_V_ADDR(base)		(base + 0x01C)
#define R8P0_DES_PITCH(base)		(base + 0x020)
#define R8P0_BACK_RGB(base)		(base + 0x024)
#define R8P0_WORK_AREA_SIZE(base)	(base + 0x028)
#define R8P0_WORK_AREA_XY(base)		(base + 0x02C)

/* LAYERIMG */
#define R8P0_LIMG_CFG(base)		(base + 0x030)
#define R8P0_LIMG_Y_ADDR(base)		(base + 0x034)
#define R8P0_LIMG_U_ADDR(base)		(base + 0x038)
#define R8P0_LIMG_V_ADDR(base)		(base + 0x03C)
#define R8P0_LIMG_PITCH(base)		(base + 0x040)
#define R8P0_LIMG_CLIP_START(base)	(base + 0x044)
#define R8P0_LIMG_CLIP_SIZE(base)	(base + 0x048)
#define R8P0_LIMG_DES_START(base)	(base + 0x04C)
#define R8P0_LIMG_PALLET_RGB(base)	(base + 0x050)
#define R8P0_LIMG_CK(base)		(base + 0x054)
#define R8P0_Y2Y_Y_PARAM(base)		(base + 0x058)
#define R8P0_Y2Y_U_PARAM(base)		(base + 0x05C)
#define R8P0_Y2Y_V_PARAM(base)		(base + 0x060)
#define R8P0_LIMG_DES_SIZE(base)	(base + 0x064)

#define R8P0_LIMG_BASE_ADDR(base)	(base + 0x030)
#define R8P0_LIMG_OFFSET 0x040

/* LAYEROSD */
#define R8P0_LOSD_CFG(base)		(base + 0x0B0)
#define R8P0_LOSD_R_ADDR(base)		(base + 0x0B4)
#define R8P0_LOSD_PITCH(base)		(base + 0x0B8)
#define R8P0_LOSD_CLIP_START(base)	(base + 0x0BC)
#define R8P0_LOSD_CLIP_SIZE(base)	(base + 0x0C0)
#define R8P0_LOSD_DES_START(base)	(base + 0x0C4)
#define R8P0_LOSD_PALLET_RGB(base)	(base + 0x0C8)
#define R8P0_LOSD_CK(base)		(base + 0x0CC)

#define R8P0_LOSD_BASE_ADDR(base)	(base + 0x0B0)
#define R8P0_LOSD_OFFSET 0x020
#define R8P0_LOSD_SEC_ADDR(base)	(base + 0x0D0)

#define R8P0_DES_DATA_CFG1(base)	(base + 0x0F0)
#define R8P0_DES_Y_ADDR1(base)		(base + 0x0F4)
#define R8P0_DES_U_ADDR1(base)		(base + 0x0F8)
#define R8P0_DES_V_ADDR1(base)		(base + 0x0FC)
#define R8P0_DES_PITCH1(base)		(base + 0x100)

#define R8P0_GSP_IP_REV(base)		(base + 0x204)
#define R8P0_GSP_DEBUG_CFG(base)	(base + 0x208)
#define R8P0_GSP_DEBUG1(base)		(base + 0x20C)
#define R8P0_GSP_DEBUG2(base)		(base + 0x210)

#define R8P0_SCALE_COEF_ADDR(base)	(base + 0x300)
#define R8P0_SCALE_COEF_OFFSET		0x200

struct R8P0_GSP_GLB_CFG_REG {
	union {
		struct {
			uint32_t GSP_RUN0		:  1;
			uint32_t GSP_RUN1		:  1;
			uint32_t GSP_BUSY0		:  1;
			uint32_t GSP_BUSY1		:  1;
			uint32_t REG_BUSY		:  1;
			uint32_t Reserved1		:  3;
			uint32_t ERR_FLG		:  1;
			uint32_t ERR_CODE		:  7;
			uint32_t Reserved2		:  16;
		};
		uint32_t	value;
	};
};

struct R8P0_GSP_INT_REG {
	union {
		struct {
			uint32_t INT_GSP_RAW		:  1;
			uint32_t INT_CORE1_RAW		:  1;
			uint32_t INT_GERR_RAW		:  1;
			uint32_t INT_CERR1_RAW		:  1;
			uint32_t INT_FBCDPL_RAW		:  1;
			uint32_t INT_FBCDHD_RAW		:  1;
			uint32_t INT_GSP_EN		:  1;
			uint32_t INT_CORE1_EN		:  1;
			uint32_t INT_GERR_EN		:  1;
			uint32_t INT_CERR1_EN		:  1;
			uint32_t INT_FBCDPL_EN		:  1;
			uint32_t INT_FBCDHD_EN		:  1;
			uint32_t INT_GSP_CLR		:  1;
			uint32_t INT_CORE1_CLR		:  1;
			uint32_t INT_GERR_CLR		:  1;
			uint32_t INT_CERR1_CLR		:  1;
			uint32_t INT_FBCDPL_CLR		:  1;
			uint32_t INT_FBCDHD_CLR		:  1;
			uint32_t INT_GSP_STS		:  1;
			uint32_t INT_CORE1_STS		:  1;
			uint32_t INT_GERR_STS		:  1;
			uint32_t INT_CERR1_STS		:  1;
			uint32_t INT_FBCDPL0_STS	:  1;
			uint32_t INT_FBCDPL1_STS	:  1;
			uint32_t INT_FBCDPL2_STS	:  1;
			uint32_t INT_FBCDPL3_STS	:  1;
			uint32_t INT_FBCDHD0_STS	:  1;
			uint32_t INT_FBCDHD1_STS	:  1;
			uint32_t INT_FBCDHD2_STS	:  1;
			uint32_t INT_FBCDHD3_STS	:  1;
			uint32_t Reserved2		:  2;
		};
		uint32_t	value;
	};
};

struct R8P0_GSP_MOD_CFG_REG {
	union {
		struct {
			uint32_t WORK_MOD		:  1;
			uint32_t CORE_NUM		:  1;
			uint32_t CO_WORK0		:  1;
			uint32_t CO_WORK1		:  1;
			uint32_t PMARGB_EN		:  1;
			uint32_t ARLEN_MOD		:  1;
			uint32_t IFBCE_AWLEN_MOD	:  2;
			uint32_t Reserved1		:  24;
		};
		uint32_t	value;
	};
};

struct R8P0_GSP_SECURE_CFG_REG {
	union {
		struct {
			uint32_t SECURE_MOD		:  1;
			uint32_t NONSEC_AWPROT		:  3;
			uint32_t NONSEC_ARPROT		:  3;
			uint32_t SECURE_AWPROT		:  3;
			uint32_t SECURE_ARPROT		:  3;
			uint32_t Reserved1		:  19;
		};
		uint32_t	value;
	};
};

struct R8P0_DES_DATA_CFG_REG {
	union {
		struct {
			uint32_t Y_ENDIAN_MOD		:  4;
			uint32_t UV_ENDIAN_MOD		:  4;
			uint32_t Reserved1		:  1;
			uint32_t A_SWAP_MOD		:  1;
			uint32_t ROT_MOD		:  3;
			uint32_t R2Y_MOD		:  3;
			uint32_t DES_IMG_FORMAT		:  3;
			uint32_t Reserved2		:  1;
			uint32_t RSWAP_MOD		:  3;
			uint32_t Reserved3		:  3;
			uint32_t FBCE_MOD		:  2;
			uint32_t DITHER_EN		:  1;
			uint32_t BK_EN			:  1;
			uint32_t BK_BLD			:  1;
			uint32_t Reserved4		:  1;
		};
		uint32_t	value;
	};
};

struct R8P0_DES_Y_ADDR_REG {
	union {
		struct {
			uint32_t Reserved		:  4;
			uint32_t DES_Y_BASE_ADDR1	:  28;
		};
		uint32_t	value;
	};
};

struct R8P0_DES_U_ADDR_REG {
	union {
		struct {
			uint32_t Reserved		:  4;
			uint32_t DES_U_BASE_ADDR1	:  28;
		};
		uint32_t	value;
	};
};

struct R8P0_DES_V_ADDR_REG {
	union {
		struct {
			uint32_t DES_V_BASE_ADDR1	:  32;
		};
		uint32_t	value;
	};
};

struct R8P0_DES_PITCH_REG {
	union {
		struct {
			uint32_t DES_PITCH	:  13;
			uint32_t Reserved1	:  3;
			uint32_t DES_HEIGHT	:  13;
			uint32_t Reserved2	:  3;
		};
		uint32_t	value;
	};
};

struct R8P0_DES_DATA_CFG1_REG {
	union {
		struct {
			uint32_t Y_ENDIAN_MOD		:  4;
			uint32_t UV_ENDIAN_MOD		:  4;
			uint32_t Reserved1		:  1;
			uint32_t A_SWAP_MOD		:  1;
			uint32_t ROT_MOD		:  3;
			uint32_t R2Y_MOD		:  3;
			uint32_t DES_IMG_FORMAT		:  3;
			uint32_t Reserved2		:  1;
			uint32_t RSWAP_MOD		:  3;
			uint32_t Reserved3		:  3;
			uint32_t FBCE_MOD		:  2;
			uint32_t DITHER_EN		:  1;
			uint32_t Reserved4		:  3;
		};
		uint32_t	value;
	};
};

struct R8P0_DES_Y_ADDR1_REG {
	union {
		struct {
			uint32_t Reserved		:  4;
			uint32_t DES_Y_BASE_ADDR1	:  28;
		};
		uint32_t	value;
	};
};

struct R8P0_DES_U_ADDR1_REG {
	union {
		struct {
			uint32_t Reserved		:  4;
			uint32_t DES_U_BASE_ADDR1	:  28;
		};
		uint32_t	value;
	};
};

struct R8P0_DES_V_ADDR1_REG {
	union {
		struct {
			uint32_t DES_V_BASE_ADDR1	 :  32;
		};
		uint32_t	value;
	};
};

struct R8P0_DES_PITCH1_REG {
	union {
		struct {
			uint32_t DES_PITCH	:  13;
			uint32_t Reserved1	:  3;
			uint32_t DES_HEIGHT	:  13;
			uint32_t Reserved2	:  3;
		};
		uint32_t	value;
	};
};

struct R8P0_BACK_RGB_REG {
	union {
		struct {
			uint32_t   BACKGROUND_B		:  8;
			uint32_t   BACKGROUND_G		:  8;
			uint32_t   BACKGROUND_R		:  8;
			uint32_t   BACKGROUND_A		:  8;
		};
		uint32_t	value;
	};
};

struct R8P0_WORK_AREA_SIZE_REG {
	union {
		struct {
			uint32_t WORK_AREA_W	  :  13;
			uint32_t Reserved1	  :  3;
			uint32_t WORK_AREA_H	  :  13;
			uint32_t Reserved2	  :  3;
		};
		uint32_t	value;
	};
};

struct R8P0_WORK_AREA_XY_REG {
	union {
		struct {
			uint32_t WORK_AREA_X	  :  13;
			uint32_t Reserved1	  :  3;
			uint32_t WORK_AREA_Y	  :  13;
			uint32_t Reserved2	  :  3;
		};
		uint32_t	value;
	};
};

struct R8P0_GSP_IP_REV_REG {
	union {
		struct {
			uint32_t PATCH_NUM	:  4;
			uint32_t GSP_IP_REV	:  12;
			uint32_t Reserved1	:  16;
		};
		uint32_t	value;
	};
};

struct R8P0_GSP_DEBUG_CFG_REG {
	union {
		struct {
			uint32_t SCL_CLR1	:  1;
			uint32_t SCL_CLR2	:  1;
			uint32_t CACHE_DIS	:  1;
			uint32_t Reserved1	:  29;
		};
		uint32_t	value;
	};
};

struct R8P0_GSP_DEBUG1_REG {
	union {
		struct {
			uint32_t   Reserved1		:  24;
			uint32_t   SCL_OUT_EMP0		:  1;
			uint32_t   SCL_OUT_FULL0	:  1;
			uint32_t   SCL_OUT_EMP1		:  1;
			uint32_t   SCL_OUT_FULL1	:  1;
			uint32_t   BLD_OUT_EMP		:  1;
			uint32_t   BLD_OUT_FULL		:  1;
			uint32_t   Reserved2		:  2;
		};
		uint32_t	value;
	};
};

struct R8P0_GSP_DEBUG2_REG {
	union {
		struct {
			uint32_t   LAYER0_DBG_STS	:  8;
			uint32_t   LAYER1_DBG_STS	:  8;
			uint32_t   LAYER2_DBG_STS	:  8;
			uint32_t   LAYER3_DBG_STS	:  8;
		};
		uint32_t	value;
	};
};

/* LAYERIMG */
struct R8P0_LAYERIMG_CFG_REG {
	union {
		struct {
			uint32_t   Y_ENDIAN_MOD		:  4;
			uint32_t   UV_ENDIAN_MOD	:  4;
			uint32_t   RGB_SWAP_MOD		:  3;
			uint32_t   A_SWAP_MOD		:  1;
			uint32_t   PMARGB_MOD		:  1;
			uint32_t   ROT_SRC		:  3;
			uint32_t   IMG_FORMAT		:  3;
			uint32_t   CK_EN		:  1;
			uint32_t   PALLET_EN		:  1;
			uint32_t   FBCD_MOD		:  2;
			uint32_t   Y2R_MOD		:  3;
			uint32_t   Y2Y_MOD		:  1;
			uint32_t   ZNUM_L		:  2;
			uint32_t   Reserved1		:  1;
			uint32_t   SCALE_EN		:  1;
			uint32_t   Limg_en		:  1;
		};
		uint32_t	value;
	};
};

struct R8P0_LAYERIMG_Y_ADDR_REG {
	union {
		struct {
			uint32_t   Reserved1		:  4;
			uint32_t   Y_BASE_ADDR		:  28;
		};
		uint32_t	value;
	};
};


struct R8P0_LAYERIMG_U_ADDR_REG {
	union {
		struct {
			uint32_t   Reserved1		:  4;
			uint32_t   U_BASE_ADDR		:  28;
		};
		uint32_t	value;
	};
};

struct R8P0_LAYERIMG_V_ADDR_REG {
	union {
		struct {
			uint32_t   Reserved1		:  4;
			uint32_t   V_BASE_ADDR		:  28;
		};
		uint32_t	value;
	};
};

struct R8P0_LAYERIMG_PITCH_REG {
	union {
		struct {
			uint32_t   PITCH		:  13;
			uint32_t   Reserved1		:  3;
			uint32_t   HEIGHT		:  13;
			uint32_t   Reserved2		:  3;
		};
		uint32_t	value;
	};
};

struct R8P0_LAYERIMG_CLIP_START_REG {
	union {
		struct {
			uint32_t   CLIP_START_X		:  13;
			uint32_t   Reserved1		:  3;
			uint32_t   CLIP_START_Y		:  13;
			uint32_t   Reserved2		:  3;
		};
		uint32_t	value;
	};
};

struct R8P0_LAYERIMG_CLIP_SIZE_REG {
	union {
		struct {
			uint32_t   CLIP_SIZE_X		:  13;
			uint32_t   Reserved1		:  3;
			uint32_t   CLIP_SIZE_Y		:  13;
			uint32_t   Reserved2		:  3;
		};
		uint32_t	value;
	};
};

struct R8P0_LAYERIMG_DES_START_REG {
	union {
		struct {
			uint32_t   DES_START_X		:  13;
			uint32_t   Reserved1		:  3;
			uint32_t   DES_START_Y		:  13;
			uint32_t   Reserved2		:  3;
		};
		uint32_t	value;
	};
};

struct R8P0_LAYERIMG_PALLET_RGB_REG {
	union {
		struct {
			uint32_t   PALLET_B		:  8;
			uint32_t   PALLET_G		:  8;
			uint32_t   PALLET_R		:  8;
			uint32_t   PALLET_A		:  8;
		};
		uint32_t	value;
	};
};

struct R8P0_LAYERIMG_CK_REG {
	union {
		struct {
			uint32_t   CK_B			:  8;
			uint32_t   CK_G			:  8;
			uint32_t   CK_R			:  8;
			uint32_t   BLOCK_ALPHA		:  8;
		};
		uint32_t	value;
	};
};

struct R8P0_Y2Y_Y_PARAM_REG {
	union {
		struct {
			uint32_t   Y_CONTRAST		:  10;
			uint32_t   Reserved1		:  6;
			uint32_t   Y_BRIGHTNESS		:  9;
			uint32_t   Reserved2		:  7;
		};
		uint32_t	value;
	};
};

struct R8P0_Y2Y_U_PARAM_REG {
	union {
		struct {
			uint32_t   U_SATURATION		:  10;
			uint32_t   Reserved1		:  6;
			uint32_t   U_OFFSET		:  8;
			uint32_t   Reserved2		:  8;
		};
		uint32_t	value;
	};
};

struct R8P0_Y2Y_V_PARAM_REG {
	union {
		struct {
			uint32_t   V_SATURATION		:  10;
			uint32_t   Reserved1		:  6;
			uint32_t   V_OFFSET		:  8;
			uint32_t   Reserved2		:  8;
		};
		uint32_t	value;
	};
};

struct R8P0_LAYERIMG_DES_SCL_SIZE_REG {
	union {
		struct {
			uint32_t DES_SCL_W	  :  13;
			uint32_t HTAP_MOD	  :  2;
			uint32_t Reserved1	  :  1;
			uint32_t DES_SCL_H	  :  13;
			uint32_t VTAP_MOD	  :  2;
			uint32_t Reserved2	  :  1;
		};
		uint32_t	value;
	};
};


/* LAYEROSD */
struct R8P0_LAYEROSD_CFG_REG {
	union {
		struct {
			uint32_t   ENDIAN		:  4;
			uint32_t   RGB_SWAP		:  3;
			uint32_t   A_SWAP		:  1;
			uint32_t   PMARGB_MOD		:  1;
			uint32_t   Reserved1		:  7;
			uint32_t   IMG_FORMAT		:  2;
			uint32_t   CK_EN		:  1;
			uint32_t   PALLET_EN		:  1;
			uint32_t   FBCD_MOD		:  1;
			uint32_t   ZNUM_L		:  2;
			uint32_t   Reserved2		:  8;
			uint32_t   Losd_en		:  1;
		};
		uint32_t	value;
	};
};

struct R8P0_LAYEROSD_R_ADDR_REG {
	union {
		struct {
			uint32_t   Reserved1		:  4;
			uint32_t   R_BASE_ADDR		:  28;
		};
		uint32_t	value;
	};
};

struct R8P0_LAYEROSD_PITCH_REG {
	union {
		struct {
			uint32_t   PITCH		:  13;
			uint32_t   Reserved1		:  3;
			uint32_t   HEIGHT		:  13;
			uint32_t   Reserved2		:  3;
		};
		uint32_t	value;
	};
};

struct R8P0_LAYEROSD_CLIP_START_REG {
	union {
		struct {
			uint32_t   CLIP_START_X		:  13;
			uint32_t   Reserved1		:  3;
			uint32_t   CLIP_START_Y		:  13;
			uint32_t   Reserved2		:  3;
		};
		uint32_t	value;
	};
};

struct R8P0_LAYEROSD_CLIP_SIZE_REG {
	union {
		struct {
			uint32_t   CLIP_SIZE_X		:  13;
			uint32_t   Reserved1		:  3;
			uint32_t   CLIP_SIZE_Y		:  13;
			uint32_t   Reserved2		:  3;
		};
		uint32_t	value;
	};
};

struct R8P0_LAYEROSD_DES_START_REG {
	union {
		struct {
			uint32_t   DES_START_X		:  13;
			uint32_t   Reserved1		:  3;
			uint32_t   DES_START_Y		:  13;
			uint32_t   Reserved2		:  3;
		};
		uint32_t	value;
	};
};

struct R8P0_LAYEROSD_PALLET_RGB_REG {
	union {
		struct {
			uint32_t   PALLET_B		:  8;
			uint32_t   PALLET_G		:  8;
			uint32_t   PALLET_R		:  8;
			uint32_t   PALLET_A		:  8;
		};
		uint32_t	value;
	};
};

struct R8P0_LAYEROSD_CK_REG {
	union {
		struct {
			uint32_t   CK_B			:  8;
			uint32_t   CK_G			:  8;
			uint32_t   CK_R			:  8;
			uint32_t   BLOCK_ALPHA		:  8;
		};
		uint32_t	value;
	};
};

struct R8P0_GSP_CTL_REG_T {
	struct R8P0_GSP_GLB_CFG_REG glb_cfg;
	struct R8P0_GSP_INT_REG int_cfg;
	struct R8P0_GSP_MOD_CFG_REG mod_cfg;
	struct R8P0_GSP_SECURE_CFG_REG secure_cfg;

	struct R8P0_DES_DATA_CFG_REG des_data_cfg;
	struct R8P0_DES_Y_ADDR_REG des_y_addr;
	struct R8P0_DES_U_ADDR_REG  des_u_addr;
	struct R8P0_DES_V_ADDR_REG  des_v_addr;
	struct R8P0_DES_PITCH_REG  des_pitch;
	struct R8P0_BACK_RGB_REG  back_rgb;
	struct R8P0_WORK_AREA_SIZE_REG  work_area_size;
	struct R8P0_WORK_AREA_XY_REG  work_area_xy;

	struct R8P0_LAYERIMG_CFG_REG  limg_cfg[R8P0_IMGL_NUM];
	struct R8P0_LAYERIMG_Y_ADDR_REG  limg_y_addr[R8P0_IMGL_NUM];
	struct R8P0_LAYERIMG_U_ADDR_REG  limg_u_addr[R8P0_IMGL_NUM];
	struct R8P0_LAYERIMG_V_ADDR_REG  limg_v_addr[R8P0_IMGL_NUM];
	struct R8P0_LAYERIMG_PITCH_REG  limg_pitch[R8P0_IMGL_NUM];
	struct R8P0_LAYERIMG_CLIP_START_REG  limg_clip_start[R8P0_IMGL_NUM];
	struct R8P0_LAYERIMG_CLIP_SIZE_REG  limg_clip_size[R8P0_IMGL_NUM];
	struct R8P0_LAYERIMG_DES_START_REG  limg_des_start[R8P0_IMGL_NUM];
	struct R8P0_LAYERIMG_PALLET_RGB_REG  limg_pallet_rgb[R8P0_IMGL_NUM];
	struct R8P0_LAYERIMG_CK_REG  limg_ck[R8P0_IMGL_NUM];
	struct R8P0_Y2Y_Y_PARAM_REG  y2y_y_param[R8P0_IMGL_NUM];
	struct R8P0_Y2Y_U_PARAM_REG  y2y_u_param[R8P0_IMGL_NUM];
	struct R8P0_Y2Y_V_PARAM_REG  y2y_v_param[R8P0_IMGL_NUM];
	struct R8P0_LAYERIMG_DES_SCL_SIZE_REG  limg_des_scl_size[R8P0_IMGL_NUM];

	struct R8P0_LAYEROSD_CFG_REG  losd_cfg[R8P0_OSDL_NUM];
	struct R8P0_LAYEROSD_R_ADDR_REG  losd_r_addr[R8P0_OSDL_NUM];
	struct R8P0_LAYEROSD_PITCH_REG  losd_pitch[R8P0_OSDL_NUM];
	struct R8P0_LAYEROSD_CLIP_START_REG  losd_clip_start[R8P0_OSDL_NUM];
	struct R8P0_LAYEROSD_CLIP_SIZE_REG  losd_clip_size[R8P0_OSDL_NUM];
	struct R8P0_LAYEROSD_DES_START_REG  losd_des_start[R8P0_OSDL_NUM];
	struct R8P0_LAYEROSD_PALLET_RGB_REG  losd_pallet_rgb[R8P0_OSDL_NUM];
	struct R8P0_LAYEROSD_CK_REG  losd_ck[R8P0_OSDL_NUM];

	struct R8P0_LAYEROSD_CFG_REG  osdsec_cfg[R8P0_OSDSEC_NUM];
	struct R8P0_LAYEROSD_R_ADDR_REG  osdsec_r_addr[R8P0_OSDSEC_NUM];
	struct R8P0_LAYEROSD_PITCH_REG  osdsec_pitch[R8P0_OSDSEC_NUM];
	struct R8P0_LAYEROSD_CLIP_START_REG  osdsec_clip_start[R8P0_OSDSEC_NUM];
	struct R8P0_LAYEROSD_CLIP_SIZE_REG  osdsec_clip_size[R8P0_OSDSEC_NUM];
	struct R8P0_LAYEROSD_DES_START_REG  osdsec_des_start[R8P0_OSDSEC_NUM];
	struct R8P0_LAYEROSD_PALLET_RGB_REG  osdsec_pallet_rgb[R8P0_OSDSEC_NUM];
	struct R8P0_LAYEROSD_CK_REG  osdsec_ck[R8P0_OSDSEC_NUM];

	struct R8P0_GSP_IP_REV_REG ip_rev;
	struct R8P0_GSP_DEBUG_CFG_REG debug_cfg;
	struct R8P0_GSP_DEBUG1_REG debug1_cfg;
	struct R8P0_GSP_DEBUG2_REG debug2_cfg;
};

#endif
